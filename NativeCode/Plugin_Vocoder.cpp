#include "AudioPluginUtil.h"

namespace Vocoder
{
    enum Param
    {
        P_GAIN,
        P_FMTSHIFT,
        P_FMTSCALE,
        P_ANALYSISBW,
        P_SYNTHESISBW,
        P_ENVDECAY,
        P_EMPHASIS,
        P_NUM
    };

    struct EnvFollower
    {
        float env;
        inline float Process(float input, float decay)
        {
            env += (fabsf(input) - env) * decay + 1.0e-11f;
            return env;
        }
    };

    struct Band
    {
        AudioPluginUtil::StateVariableFilter analysis1;
        AudioPluginUtil::StateVariableFilter analysis2;
        AudioPluginUtil::StateVariableFilter synthesis1;
        AudioPluginUtil::StateVariableFilter synthesis2;
        EnvFollower envfollow;
    };

    const int NUMBANDS = 11;

    struct EffectData
    {
        struct Data
        {
            float p[P_NUM];
            Band bands[8][NUMBANDS];
        };
        union
        {
            Data data;
            unsigned char pad[(sizeof(Data) + 15) & ~15]; // This entire structure must be a multiple of 16 bytes (and and instance 16 byte aligned) for PS3 SPU DMA requirements
        };
    };

    int InternalRegisterEffectDefinition(UnityAudioEffectDefinition& definition)
    {
        int numparams = P_NUM;
        definition.paramdefs = new UnityAudioParameterDefinition[numparams];
        AudioPluginUtil::RegisterParameter(definition, "Gain", "dB", -100.0f, 20.0f, -30.0f, 1.0f, 1.0f, P_GAIN, "Overall gain.");
        AudioPluginUtil::RegisterParameter(definition, "Formant Shift", "Hz", -1500.0f, 1500.0f, 0.0f, 1.0f, 3.0f, P_FMTSHIFT, "Relative shifting of filterbank center frequencies.");
        AudioPluginUtil::RegisterParameter(definition, "Formant Scale", "x", 0.05f, 10.0f, 1.0f, 1.0f, 3.0f, P_FMTSCALE, "Scaling of filterbank center frequencies.");
        AudioPluginUtil::RegisterParameter(definition, "Analysis BW", "%", 0.001f, 1.0f, 0.1f, 100.0f, 1.0f, P_ANALYSISBW, "Analysis filterbank bandwidth.");
        AudioPluginUtil::RegisterParameter(definition, "Synthesis BW", "%", 0.001f, 1.0f, 0.1f, 100.0f, 1.0f, P_SYNTHESISBW, "Synthesis filterbank bandwidth.");
        AudioPluginUtil::RegisterParameter(definition, "Envelope Decay", "s", 0.001f, 0.4f, 0.01f, 1.0f, 1.0f, P_ENVDECAY, "Envelope follower decay time. Inversely proportional to the speed at which changes are detected.");
        AudioPluginUtil::RegisterParameter(definition, "Emphasis", "%", 0.5f, 1.5f, 1.2f, 100.0f, 1.0f, P_EMPHASIS, "Emphasis amount. Can be used to tilt the filter bank to improve intelligibility of consonants.");
        definition.flags |= UnityAudioEffectDefinitionFlags_IsSideChainTarget;
        return numparams;
    }

    UNITY_AUDIODSP_RESULT UNITY_AUDIODSP_CALLBACK CreateCallback(UnityAudioEffectState* state)
    {
        EffectData* effectdata = new EffectData;
        memset(effectdata, 0, sizeof(EffectData));
        state->effectdata = effectdata;
        AudioPluginUtil::InitParametersFromDefinitions(InternalRegisterEffectDefinition, effectdata->data.p);
        return UNITY_AUDIODSP_OK;
    }

    UNITY_AUDIODSP_RESULT UNITY_AUDIODSP_CALLBACK ReleaseCallback(UnityAudioEffectState* state)
    {
        EffectData* effectdata = state->GetEffectData<EffectData>();
        delete effectdata;
        return UNITY_AUDIODSP_OK;
    }

    UNITY_AUDIODSP_RESULT UNITY_AUDIODSP_CALLBACK SetFloatParameterCallback(UnityAudioEffectState* state, int index, float value)
    {
        EffectData::Data* data = &state->GetEffectData<EffectData>()->data;
        if (index < 0 || index >= P_NUM)
            return UNITY_AUDIODSP_ERR_UNSUPPORTED;
        data->p[index] = value;
        return UNITY_AUDIODSP_OK;
    }

    UNITY_AUDIODSP_RESULT UNITY_AUDIODSP_CALLBACK GetFloatParameterCallback(UnityAudioEffectState* state, int index, float* value, char *valuestr)
    {
        EffectData::Data* data = &state->GetEffectData<EffectData>()->data;
        if (index < 0 || index >= P_NUM)
            return UNITY_AUDIODSP_ERR_UNSUPPORTED;
        if (value != NULL)
            *value = data->p[index];
        if (valuestr != NULL)
            valuestr[0] = 0;
        return UNITY_AUDIODSP_OK;
    }

    int UNITY_AUDIODSP_CALLBACK GetFloatBufferCallback(UnityAudioEffectState* state, const char* name, float* buffer, int numsamples)
    {
        return UNITY_AUDIODSP_OK;
    }

    static float freqs[] = { 100, 225, 330, 470, 700, 1030, 1500, 2280, 3300, 4700, 9000 };

    UNITY_AUDIODSP_RESULT UNITY_AUDIODSP_CALLBACK ProcessCallback(UnityAudioEffectState* state, float* inbuffer, float* outbuffer, unsigned int length, int inchannels, int outchannels)
    {
        EffectData::Data* data = &state->GetEffectData<EffectData>()->data;
        float gain = powf(10.0f, 0.05f * data->p[P_GAIN] + 2.5f);
        float maxfreq = 0.25f * state->samplerate;
        float sampletime = 1.0f / (float)state->samplerate;
        float w0 = 0.5f * AudioPluginUtil::kPI * sampletime;
        float envdecay = 1.0f - powf(0.001f, sampletime / data->p[P_ENVDECAY]);
        float emph = data->p[P_EMPHASIS];
        for (int j = 0; j < NUMBANDS; j++)
        {
            float f = 0.25f * (freqs[j] * data->p[P_FMTSCALE] + data->p[P_FMTSHIFT]);
            if (f < 10.0f)
                f = 10.0f;
            else if (f > maxfreq)
                f = maxfreq;
            float w = f * w0;
            float ra = data->p[P_ANALYSISBW];
            float ca = 2.0f * sinf(w);
            float rs = data->p[P_SYNTHESISBW];
            float cs = 2.0f * sinf(w);
            for (int i = 0; i < inchannels; i++)
            {
                data->bands[i][j].analysis1.cutoff = ca;
                data->bands[i][j].analysis2.cutoff = ca;
                data->bands[i][j].analysis1.bandwidth = ra;
                data->bands[i][j].analysis2.bandwidth = ra;
                data->bands[i][j].synthesis1.cutoff = cs;
                data->bands[i][j].synthesis2.cutoff = cs;
                data->bands[i][j].synthesis1.bandwidth = rs;
                data->bands[i][j].synthesis2.bandwidth = rs;
            }
            gain *= emph;
        }

        float* sidechainBuffer = state->sidechainbuffer;
        for (unsigned int n = 0; n < length; n++)
        {
            for (int i = 0; i < inchannels; i++)
            {
                float input = (*inbuffer++) * gain + 1.0e-11f;
                float sidechainInput = *sidechainBuffer++;
                float sum = 0.0f;
                Band* b = data->bands[i];
                Band* b_end = b + NUMBANDS;
                while (b != b_end)
                {
                    float carrier = b->synthesis2.ProcessBPF(b->synthesis1.ProcessBPF(input));
                    float source = b->analysis2.ProcessBPF(b->analysis1.ProcessBPF(sidechainInput));
                    float env = b->envfollow.Process(source, envdecay);
                    sum += carrier * env;
                    ++b;
                }
                *outbuffer++ = sum;
            }
        }

        return UNITY_AUDIODSP_OK;
    }
}
