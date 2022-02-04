#include "AudioPluginUtil.h"

namespace TeeDee
{
    enum Param
    {
        P_SEED,
        P_BPM,
        P_SINE,
        P_NOISE,
        P_CUT,
        P_RES,
        P_FREQ,
        P_ADECAY,
        P_FDECAY,
        P_PDECAY,
        P_FILTERTYPE,
        P_POSTDIST,
        P_AENV,
        P_FENV,
        P_PENV,
        P_PREDIST,
        P_INPUTMIX,
        P_NUMSTEPS,
        P_NUM
    };

    struct EffectData
    {
        struct Data
        {
            int pattern[64];
            float phase;
            float aenv;
            float fenv;
            float penv;
            float lpf;
            float bpf;
            float wetmix;
            float p[P_NUM];
            int pattern_index;
            AudioPluginUtil::Random random;
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
        AudioPluginUtil::RegisterParameter(definition, "Seed", "", 0.0f, 100.0f, 0.0f, 1.0f, 1.0f, P_SEED, "Random seed of the pattern played. The first 5 patterns are pre-defined typical rhythm patterns.");
        AudioPluginUtil::RegisterParameter(definition, "BPM", "BPM", 10.0f, 300.0f, 120.0f, 1.0f, 1.0f, P_BPM, "Tempo of the played pattern in beats per minute.");
        AudioPluginUtil::RegisterParameter(definition, "Sine", "%", 0.0f, 1.0f, 1.0f, 100.0f, 1.0f, P_SINE, "Amount of sine oscillator.");
        AudioPluginUtil::RegisterParameter(definition, "Noise", "%", 0.0f, 1.0f, 0.0f, 100.0f, 1.0f, P_NOISE, "Amount of noise generator.");
        AudioPluginUtil::RegisterParameter(definition, "Cutoff", "Hz", 0.0f, AudioPluginUtil::kMaxSampleRate, 1000.0f, 1.0f, 3.0f, P_CUT, "Base cutoff frequency of resonant lowpass/highpass filter");
        AudioPluginUtil::RegisterParameter(definition, "Resonance", "%", 0.0f, 1.0f, 0.2f, 100.0f, 3.0f, P_RES, "Resonance amount of filter.");
        AudioPluginUtil::RegisterParameter(definition, "Freq", "Hz", 0.0f, 1000.0f, 200.0f, 1.0f, 3.0f, P_FREQ, "Base frequency of sine oscillator.");
        AudioPluginUtil::RegisterParameter(definition, "AmpDecay", "s", 0.0f, 5.0f, 0.5f, 1.0f, 3.0f, P_ADECAY, "Amplitude decay time in seconds.");
        AudioPluginUtil::RegisterParameter(definition, "FilterDecay", "s", 0.0f, 5.0f, 0.5f, 1.0f, 3.0f, P_FDECAY, "Frequency decay time in seconds.");
        AudioPluginUtil::RegisterParameter(definition, "PitchDecay", "s", 0.0f, 5.0f, 0.5f, 1.0f, 3.0f, P_PDECAY, "Pitch decay time in seconds.");
        AudioPluginUtil::RegisterParameter(definition, "FilterType", "%", 0.0f, 1.0f, 0.0f, 100.0f, 1.0f, P_FILTERTYPE, "Mix ratio between lowpass and highpass filters.");
        AudioPluginUtil::RegisterParameter(definition, "Distortion", "%", 0.0f, 100.0f, 3.0f, 100.0f, 1.0f, P_POSTDIST, "Amount of distortion applied after the resonant filter.");
        AudioPluginUtil::RegisterParameter(definition, "AmpEnv", "%", 0.0f, 1.0f, 1.0f, 100.0f, 1.0f, P_AENV, "Amplitude envelope amount.");
        AudioPluginUtil::RegisterParameter(definition, "FilterEnv", "%", 0.0f, AudioPluginUtil::kMaxSampleRate, 5000.0f, 1.0f, 3.0f, P_FENV, "Frequency envelope amount.");
        AudioPluginUtil::RegisterParameter(definition, "PitchEnv", "%", 0.0f, 1000.0f, 0.0f, 100.0f, 1.0f, P_PENV, "Pitch envelope amount.");
        AudioPluginUtil::RegisterParameter(definition, "PreDist", "%", 0.0f, 100.0f, 1.0f, 100.0f, 1.0f, P_PREDIST, "Distortion applied before the resonant filter.");
        AudioPluginUtil::RegisterParameter(definition, "InputMix", "%", 0.0f, 100.0f, 100.0f, 1.0f, 1.0f, P_INPUTMIX, "Amount of input signals mixed to the output of the synthesizer.");
        AudioPluginUtil::RegisterParameter(definition, "NumSteps", "", 1.0f, 64.0f, 16.0f, 1.0f, 1.0f, P_NUMSTEPS, "Number of steps in the played pattern.");
        return numparams;
    }

    static void CalcPattern(EffectData::Data* data)
    {
        int seed = (int)data->p[P_SEED];
        AudioPluginUtil::Random random;
        random.Seed((unsigned long)(seed * 1000));
        for (int i = 0; i < 64; i++)
        {
            switch (seed)
            {
                case 0: data->pattern[i] = ((i & 3) ==  0) ? 1 : 0; break;
                case 1: data->pattern[i] = ((i & 7) ==  4) ? 1 : 0; break;
                case 2: data->pattern[i] = ((i & 7) <   2) ? 1 : 0; break;
                case 3: data->pattern[i] = ((i & 7) == 12) ? 1 : 0; break;
                case 4: data->pattern[i] = 1; break;
                default: data->pattern[i] = random.Get() & 1;
            }
        }
    }

    UNITY_AUDIODSP_RESULT UNITY_AUDIODSP_CALLBACK CreateCallback(UnityAudioEffectState* state)
    {
        EffectData* effectdata = new EffectData;
        memset(effectdata, 0, sizeof(EffectData));
        state->effectdata = effectdata;
        AudioPluginUtil::InitParametersFromDefinitions(InternalRegisterEffectDefinition, effectdata->data.p);
        CalcPattern(&effectdata->data);
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
        if (index >= P_NUM)
            return UNITY_AUDIODSP_ERR_UNSUPPORTED;
        data->p[index] = value;
        if (index == P_SEED)
            CalcPattern(data);
        return UNITY_AUDIODSP_OK;
    }

    UNITY_AUDIODSP_RESULT UNITY_AUDIODSP_CALLBACK GetFloatParameterCallback(UnityAudioEffectState* state, int index, float* value, char *valuestr)
    {
        EffectData::Data* data = &state->GetEffectData<EffectData>()->data;
        if (index >= P_NUM)
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

    UNITY_AUDIODSP_RESULT UNITY_AUDIODSP_CALLBACK ProcessCallback(UnityAudioEffectState* state, float* inbuffer, float* outbuffer, unsigned int length, int inchannels, int outchannels)
    {
        EffectData::Data* data = &state->GetEffectData<EffectData>()->data;

        float sr = (float)state->samplerate;
        float st = 1.0f / sr;
        float sr_half = sr * 0.5f;
        float adecay = (data->p[P_ADECAY] == 0) ? 1.0f : powf(0.01f, 1.0f / (sr * data->p[P_ADECAY]));
        float fdecay = (data->p[P_FDECAY] == 0) ? 1.0f : powf(0.01f, 1.0f / (sr * data->p[P_FDECAY]));
        float pdecay = (data->p[P_PDECAY] == 0) ? 1.0f : powf(0.01f, 1.0f / (sr * data->p[P_PDECAY]));
        float wetTarget = ((state->flags & UnityAudioEffectStateFlags_IsPlaying) && !(state->flags & (UnityAudioEffectStateFlags_IsMuted | UnityAudioEffectStateFlags_IsPaused))) ? 1.0f : 0.0f;
        float inputmix = data->p[P_INPUTMIX] * 0.01f;
        UInt64 pattern_length = (UInt64)(sr * 0.25f * 60.0f / data->p[P_BPM]);
        for (unsigned int n = 0; n < length; n++)
        {
            if (((state->currdsptick + n) % pattern_length) == 0)
            {
                data->pattern_index = ((state->currdsptick + n) / pattern_length) % (int)data->p[P_NUMSTEPS];
                if (data->pattern[data->pattern_index])
                {
                    data->aenv = data->p[P_AENV];
                    data->fenv = data->p[P_FENV];
                    data->penv = data->p[P_PENV];
                }
            }
            data->phase += (data->p[P_FREQ] + data->penv) * st;
            data->phase -= AudioPluginUtil::FastFloor(data->phase);
            data->aenv = data->aenv * adecay + 1.0e-11f;
            data->fenv = data->fenv * fdecay + 1.0e-11f;
            data->penv = data->penv * pdecay + 1.0e-11f;
            float outval = sinf(data->phase * 2.0f * AudioPluginUtil::kPI) * data->p[P_SINE] + data->random.GetFloat(-1.0f, 1.0f) * data->p[P_NOISE];
            outval = atanf(outval * data->p[P_PREDIST]) * (1.0f / AudioPluginUtil::kPI) * data->aenv;
            float cut = data->p[P_CUT] + data->fenv;
            if (cut < 0.0f)
                cut = 0.0f;
            else if (cut > sr_half)
                cut = sr_half;
            cut = 2.0f * sinf(0.5f * AudioPluginUtil::kPI * cut * st);
            if (cut > 1.4f)
                cut = 1.4f;
            float bw = 1.0f - data->p[P_RES]; bw *= bw;
            data->lpf += cut * data->bpf;
            float hpf = outval - data->lpf - data->bpf * bw;
            data->bpf += cut * hpf;
            data->wetmix += (wetTarget - data->wetmix) * 0.05f + 1.0e-9f;
            outval = data->lpf + (hpf - data->lpf) * data->p[P_FILTERTYPE];
            outval = atanf(outval * data->p[P_POSTDIST]) * (1.0f / AudioPluginUtil::kPI) * data->aenv;
            for (int i = 0; i < outchannels; i++)
            {
                float inval = inbuffer[n * inchannels + i];
                outbuffer[n * outchannels + i] = inval + (outval + inval * inputmix - inval) * data->wetmix;
            }
        }

        return UNITY_AUDIODSP_OK;
    }
}
