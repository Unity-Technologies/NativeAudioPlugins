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

    struct Filter
    {
        float cut, bw;
        float prev1, lpf1, bpf1;
        float prev2, lpf2, bpf2;
        inline float Process(float input)
        {
            input += 1.0e-11f; // Kill denormals
            float s1 = (input + prev1) * 0.5f;
            lpf1 += cut * bpf1;
            bpf1 += cut * ((s1 - bpf1) * bw - lpf1);
            lpf1 += cut * bpf1;
            bpf1 += cut * (-bpf1 * bw - lpf1);
            prev1 = input;
            float s2 = (bpf1 + prev2) * 0.5f;
            lpf2 += cut * bpf2;
            bpf2 += cut * ((s2 - bpf2) * bw - lpf2);
            lpf2 += cut * bpf2;
            bpf2 += cut * (-bpf2 * bw - lpf2);
            prev2 = bpf1;
            return bpf2;
        }
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
        Filter analysis;
        Filter synthesis;
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

#if !UNITY_SPU

    int InternalRegisterEffectDefinition(UnityAudioEffectDefinition& definition)
    {
        int numparams = P_NUM;
        definition.paramdefs = new UnityAudioParameterDefinition[numparams];
        RegisterParameter(definition, "Gain", "dB", -100.0f, 20.0f, -30.0f, 1.0f, 1.0f, P_GAIN, "Overall gain.");
        RegisterParameter(definition, "Formant Shift", "Hz", -1500.0f, 1500.0f, 0.0f, 1.0f, 3.0f, P_FMTSHIFT, "Relative shifting of filterbank center frequencies.");
        RegisterParameter(definition, "Formant Scale", "x", 0.05f, 10.0f, 1.0f, 1.0f, 3.0f, P_FMTSCALE, "Scaling of filterbank center frequencies.");
        RegisterParameter(definition, "Analysis BW", "%", 0.001f, 1.0f, 0.1f, 100.0f, 1.0f, P_ANALYSISBW, "Analysis filterbank bandwidth.");
        RegisterParameter(definition, "Synthesis BW", "%", 0.001f, 1.0f, 0.1f, 100.0f, 1.0f, P_SYNTHESISBW, "Synthesis filterbank bandwidth.");
        RegisterParameter(definition, "Envelope Decay", "s", 0.001f, 0.4f, 0.01f, 1.0f, 1.0f, P_ENVDECAY, "Envelope follower decay time. Inversely proportional to the speed at which changes are detected.");
        RegisterParameter(definition, "Emphasis", "%", 0.5f, 1.5f, 1.2f, 100.0f, 1.0f, P_EMPHASIS, "Emphasis amount. Can be used to tilt the filter bank to improve intelligibility of consonants.");
        definition.flags |= UnityAudioEffectDefinitionFlags_IsSideChainTarget;
        return numparams;
    }

    UNITY_AUDIODSP_RESULT UNITY_AUDIODSP_CALLBACK CreateCallback(UnityAudioEffectState* state)
    {
        EffectData* effectdata = new EffectData;
        memset(effectdata, 0, sizeof(EffectData));
        state->effectdata = effectdata;
        InitParametersFromDefinitions(InternalRegisterEffectDefinition, effectdata->data.p);
        return UNITY_AUDIODSP_OK;
    }

    UNITY_AUDIODSP_RESULT UNITY_AUDIODSP_CALLBACK ReleaseCallback(UnityAudioEffectState* state)
    {
        EffectData::Data* data = &state->GetEffectData<EffectData>()->data;
        delete data;
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

#endif

#if !UNITY_PS3 || UNITY_SPU

    static float freqs[] = { 100, 225, 330, 470, 700, 1030, 1500, 2280, 3300, 4700, 9000 };

#if UNITY_SPU
    EffectData  g_EffectData __attribute__((aligned(16)));
    extern "C"
#endif
    UNITY_AUDIODSP_RESULT UNITY_AUDIODSP_CALLBACK ProcessCallback(UnityAudioEffectState* state, float* inbuffer, float* outbuffer, unsigned int length, int inchannels, int outchannels)
    {
        EffectData::Data* data = &state->GetEffectData<EffectData>()->data;

#if UNITY_SPU
        UNITY_PS3_CELLDMA_GET(&g_EffectData, state->effectdata, sizeof(g_EffectData));
        data = &g_EffectData.data;
#endif

        float gain = powf(10.0f, 0.05f * data->p[P_GAIN] + 2.5);
        float maxfreq = 0.25f * state->samplerate;
        float sampletime = 1.0f / (float)state->samplerate;
        float w0 = 0.5f * kPI * sampletime;
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
                data->bands[i][j].analysis.cut = ca;
                data->bands[i][j].analysis.bw = ra;
                data->bands[i][j].synthesis.cut = cs;
                data->bands[i][j].synthesis.bw = rs;
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
                    float carrier = b->synthesis.Process(input);
                    float source = b->analysis.Process(sidechainInput);
                    float env = b->envfollow.Process(source, envdecay);
                    sum += carrier * env;
                    ++b;
                }
                *outbuffer++ = sum;
            }
        }

#if UNITY_SPU
        UNITY_PS3_CELLDMA_PUT(&g_EffectData, state->effectdata, sizeof(g_EffectData));
#endif
        return UNITY_AUDIODSP_OK;
    }

#endif
}
