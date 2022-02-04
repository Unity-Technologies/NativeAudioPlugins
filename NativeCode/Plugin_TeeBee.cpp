#include "AudioPluginUtil.h"

namespace TeeBee
{
    enum Param
    {
        P_SEED,
        P_MINNOTE,
        P_MAXNOTE,
        P_CUT,
        P_ENV,
        P_CUTRND,
        P_ENVRND,
        P_DECAY,
        P_RES,
        P_DIST,
        P_GLIDE,
        P_BPM,
        P_LFOFREQ,
        P_LFOCUT,
        P_LFOCUTENV,
        P_INPUTMIX,
        P_NUMSTEPS,
        P_NUM
    };

    struct EffectData
    {
        struct Data
        {
            float pattern[64];
            float phase;
            float lfophase;
            float freq;
            float lpf;
            float bpf;
            float env;
            float lfoenv;
            float cutrnd;
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
        AudioPluginUtil::RegisterParameter(definition, "Seed", "", 0.0f, 1000.0f, 0.0f, 1.0f, 1.0f, P_SEED, "Random seed that determines the pattern played.");
        AudioPluginUtil::RegisterParameter(definition, "LowNote", "semitones", 0.0f, 100.0f, 24.0f, 1.0f, 1.0f, P_MINNOTE, "Deepest note in the pattern.");
        AudioPluginUtil::RegisterParameter(definition, "HighNote", "semitones", 0.0f, 100.0f, 48.0f, 1.0f, 1.0f, P_MAXNOTE, "Highest note in the pattern.");
        AudioPluginUtil::RegisterParameter(definition, "Cutoff", "Hz", 0.0f, AudioPluginUtil::kMaxSampleRate, 1000.0f, 1.0f, 3.0f, P_CUT, "Base cutoff frequency of resonant lowpass filter.");
        AudioPluginUtil::RegisterParameter(definition, "Envelope", "Hz", 0.0f, AudioPluginUtil::kMaxSampleRate, 500.0f, 1.0f, 3.0f, P_ENV, "Amount of decay envelope applied to cutoff frequency.");
        AudioPluginUtil::RegisterParameter(definition, "CutRnd", "Hz", 0.0f, AudioPluginUtil::kMaxSampleRate, 0.0f, 1.0f, 3.0f, P_CUTRND, "Amount of cutoff randomization applied at note onset.");
        AudioPluginUtil::RegisterParameter(definition, "EnvRnd", "Hz", 0.0f, AudioPluginUtil::kMaxSampleRate, 0.0f, 1.0f, 3.0f, P_ENVRND, "Amount of randomization applied to cutoff envelope at note onset.");
        AudioPluginUtil::RegisterParameter(definition, "Decay", "s", 0.0f, 5.0f, 0.5f, 1.0f, 3.0f, P_DECAY, "Decay time of cutoff envelope.");
        AudioPluginUtil::RegisterParameter(definition, "Resonance", "%", 0.0f, 1.0f, 0.2f, 100.0f, 2.0f, P_RES, "Resonance amount of lowpass filter.");
        AudioPluginUtil::RegisterParameter(definition, "Distortion", "%", 0.0f, 100.0f, 3.0f, 100.0f, 1.0f, P_DIST, "Amount of distortion applied after the resonant lowpass filter.");
        AudioPluginUtil::RegisterParameter(definition, "Glide", "s", 0.001f, 1.0f, 0.01f, 1.0f, 3.0f, P_GLIDE, "Pitch glide time.");
        AudioPluginUtil::RegisterParameter(definition, "BPM", "BPM", 10.0f, 300.0f, 120.0f, 1.0f, 1.0f, P_BPM, "Tempo of pattern in beats per minute.");
        AudioPluginUtil::RegisterParameter(definition, "LFOFreq", "Hz", 0.0f, 50.0f, 0.1f, 1.0f, 3.0f, P_LFOFREQ, "Frequency of the low frequency oscillator that modulates the cutoff frequency of the resonant lowpass filter.");
        AudioPluginUtil::RegisterParameter(definition, "LFOCut", "Hz", 0.0f, AudioPluginUtil::kMaxSampleRate, 0.0f, 1.0f, 3.0f, P_LFOCUT, "Modulation amount of the low frequency oscillator that modulates the cutoff frequency of the resonant lowpass filter.");
        AudioPluginUtil::RegisterParameter(definition, "LFOCutEnv", "Hz", 0.0f, AudioPluginUtil::kMaxSampleRate, 0.0f, 1.0f, 3.0f, P_LFOCUTENV, "Modulation amount of the low frequency oscillator that modulates the envelope cutoff of the resonant lowpass filter.");
        AudioPluginUtil::RegisterParameter(definition, "InputMix", "%", 0.0f, 100.0f, 100.0f, 1.0f, 1.0f, P_INPUTMIX, "Amount of input signal mixed to the output of the synthesizer.");
        AudioPluginUtil::RegisterParameter(definition, "NumSteps", "", 1.0f, 64.0f, 16.0f, 1.0f, 1.0f, P_NUMSTEPS, "Number of steps in the played pattern.");
        return numparams;
    }

    static void CalcPattern(EffectData::Data* data)
    {
        AudioPluginUtil::Random random;
        random.Seed((unsigned long)(data->p[P_SEED] * 1000));
        for (int i = 0; i < 64; i++)
        {
            int note = (int)random.GetFloat(data->p[P_MINNOTE], data->p[P_MAXNOTE]);
            data->pattern[i] = 440.0f * powf(2.0f, (note - 60) / 12.0f);
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
        if (index == P_SEED || index == P_MINNOTE || index == P_MAXNOTE)
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
        float max_cut = 0.4f * sr;
        float glide = 1.0f - powf(0.01f, 1.0f / (sr * data->p[P_GLIDE]));
        float envdecay = powf(0.01f, 1.0f / (sr * data->p[P_DECAY]));
        float wetTarget = ((state->flags & UnityAudioEffectStateFlags_IsPlaying) && !(state->flags & (UnityAudioEffectStateFlags_IsMuted | UnityAudioEffectStateFlags_IsPaused))) ? 1.0f : 0.0f;
        float inputmix = data->p[P_INPUTMIX] * 0.01f;
        UInt64 pattern_length = (UInt64)(sr * 0.25f * 60.0f / data->p[P_BPM]);
        for (unsigned int n = 0; n < length; n++)
        {
            if (((state->currdsptick + n) % pattern_length) == 0)
            {
                data->pattern_index = ((state->currdsptick + n) / pattern_length) % (int)data->p[P_NUMSTEPS];
                data->env = data->p[P_ENV] + data->random.GetFloat(0.0f, data->p[P_ENVRND]);
                data->lfoenv = 1.0f;
                data->cutrnd = data->random.GetFloat(0.0f, data->p[P_CUTRND]);
            }
            data->phase += data->freq;
            data->phase -= AudioPluginUtil::FastFloor(data->phase);
            data->lfophase += data->p[P_LFOFREQ] * st;
            data->lfophase -= AudioPluginUtil::FastFloor(data->lfophase);
            data->freq += (data->pattern[data->pattern_index] * st - data->freq) * glide;
            float outval = data->phase * 2.0f - 1.0f;
            float lfocut = 0.5f + 0.5f * sinf(data->lfophase * 2.0f * AudioPluginUtil::kPI);
            float cut = data->p[P_CUT] + data->cutrnd + data->env + lfocut * (data->p[P_LFOCUT] + data->lfoenv * data->p[P_LFOCUTENV]);
            if (cut < 0.0f)
                cut = 0.0f;
            else if (cut > max_cut)
                cut = max_cut;
            cut = 2.0f * sinf(0.5f * AudioPluginUtil::kPI * cut * st);
            if (cut > 1.4f)
                cut = 1.4f;
            float bw = 1.0f - data->p[P_RES];
            data->env = data->env * envdecay + 1.0e-11f;
            data->lfoenv = data->lfoenv * envdecay + 1.0e-11f;
            float tmp = 0.5f * cut * data->bpf;
            float lpf_out = data->lpf + tmp; data->lpf = lpf_out + tmp;
            tmp = 0.5f * cut * (outval - data->lpf - data->bpf * bw);
            float bpf_out = data->bpf + tmp; data->bpf = bpf_out + tmp;
            data->wetmix += (wetTarget - data->wetmix) * 0.05f + 1.0e-9f;
            outval = atanf(lpf_out * data->p[P_DIST]) * (1.0f / AudioPluginUtil::kPI);
            for (int i = 0; i < outchannels; i++)
            {
                float inval = inbuffer[n * inchannels + i];
                outbuffer[n * outchannels + i] = inval + (outval + inval * inputmix - inval) * data->wetmix;
            }
        }

        return UNITY_AUDIODSP_OK;
    }
}
