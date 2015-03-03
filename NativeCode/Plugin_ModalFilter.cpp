#include "AudioPluginUtil.h"

namespace ModalFilter
{
    const int MAXRESONATORS = 256;

    enum Param
    {
        P_SEED,
        P_NUMMODES,
        P_FREQSHIFT,
        P_FREQSHIFTVAR,
        P_FREQSCALE,
        P_FREQSCALEVAR,
        P_BWSCALE,
        P_BWSCALEVAR,
        P_GAINSCALE,
        P_GAINSCALEVAR,
        P_NUM
    };

    class Resonator
    {
    public:
        inline void Setup(float fCutoff, float fBandwidth, float fGain)
        {
            fCutoff = FastClip(fCutoff, 0.0001f, 0.9999f);
            float fRadius = FastClip(1.0f - fBandwidth, 0.0001f, 0.9999f);
            a0 = fGain * 0.5f * (1.0f - fRadius * fRadius);
            a1 = -2.0f * fRadius * cosf(fCutoff * kPI);
            a2 = fRadius * fRadius;
        }

        inline float Process(const float input)
        {
            float fIIR = input * a0 - d1 * a1 - d2 * a2;
            fIIR += 1.0e-7f;
            fIIR -= 1.0e-7f;
            float output = fIIR - d2;
            d2 = d1;
            d1 = fIIR;
            return output;
        }

    protected:
        float d1, d2;
        float a0, a1, a2;
    };

    struct EffectData
    {
        struct Data
        {
            float p[P_NUM];
            float prev[P_NUM];
            Random random;
            Resonator resonators[8][MAXRESONATORS];
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
        RegisterParameter(definition, "Random seed", "", 0.0f, 100000.0f, 0.0f, 1.0f, 1.0f, P_SEED, "Random seed, selects locations of modes and their bandwidth randomly");
        RegisterParameter(definition, "Num modes", "", 1.0f, MAXRESONATORS, 10.0f, 1.0f, 1.0f, P_NUMMODES, "Number of modes or partials");
        RegisterParameter(definition, "Freq shift", "Hz", -3000.0f, 3000.0f, 0.0f, 1.0f, 1.0f, P_FREQSHIFT, "Frequency shift in Hz");
        RegisterParameter(definition, "Freq shift var", "Hz", -3000.0f, 3000.0f, 0.01f, 1.0f, 1.0f, P_FREQSHIFTVAR, "Randomized frequency shift in Hz");
        RegisterParameter(definition, "Freq scale", "%", -10.0f, 10.0f, 1.0f, 100.0f, 1.0f, P_FREQSCALE, "Frequency scaling in Hz");
        RegisterParameter(definition, "Freq scale var", "%", -10.0f, 10.0f, 0.0f, 100.0f, 1.0f, P_FREQSCALEVAR, "Randomized frequency scaling in Hz");
        RegisterParameter(definition, "BW scale", "%", 0.001f, 10.0f, 1.0f, 100.0f, 1.0f, P_BWSCALE, "Bandwidth scaling factor");
        RegisterParameter(definition, "BW scale var", "%", 0.001f, 10.0f, 0.001f, 100.0f, 1.0f, P_BWSCALEVAR, "Randomized bandwidth scaling factor");
        RegisterParameter(definition, "Gain scale", "dB", -100.0f, 100.0f, 0.0f, 1.0f, 1.0f, P_GAINSCALE, "Gain scaling in dB");
        RegisterParameter(definition, "Gain scale var", "dB", -100.0f, 100.0f, 0.0f, 1.0f, 1.0f, P_GAINSCALEVAR, "Randomized gain scaling in dB");
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
        if (index >= P_NUM)
            return UNITY_AUDIODSP_ERR_UNSUPPORTED;
        data->p[index] = value;
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

#endif

#if !UNITY_PS3 || UNITY_SPU

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

        memset(outbuffer, 0, outchannels * length * sizeof(float));

        const float fSampleTime = 1.0f / state->samplerate;
        const int nNumResonators = (int)data->p[P_NUMMODES];

        if (memcmp(data->prev, data->p, sizeof(data->p)) != 0)
        {
            memcpy(data->prev, data->p, sizeof(data->p));

            data->random.Seed((int)data->p[P_SEED]);

            for (int i = 0; i < inchannels; i++)
            {
                for (int k = 0; k < nNumResonators; k++)
                {
                    float fFreq = 0.002f * (k + 1);
                    fFreq *= data->p[P_FREQSCALE] + data->random.GetFloat(-data->p[P_FREQSCALEVAR], data->p[P_FREQSCALEVAR]);
                    fFreq += (data->p[P_FREQSHIFT] + data->random.GetFloat(-data->p[P_FREQSHIFTVAR], data->p[P_FREQSHIFTVAR])) * fSampleTime;
                    float fBandwidth = powf(0.001f, data->p[P_BWSCALE] + data->random.GetFloat(-data->p[P_BWSCALEVAR], data->p[P_BWSCALEVAR]));
                    float fGain = powf(10.0f, 0.05f * (data->p[P_GAINSCALE] + data->random.GetFloat(-data->p[P_GAINSCALEVAR], data->p[P_GAINSCALEVAR])));
                    Resonator& resonator = data->resonators[i][k];
                    resonator.Setup(fFreq, fBandwidth, fGain);
                }
            }
        }

        for (int i = 0; i < inchannels; i++)
        {
            float denormalFix = data->random.GetFloat(-1.0f, 1.0f) * 1.0e-9f;
            for (int k = 0; k < nNumResonators; k++)
            {
                Resonator& resonator = data->resonators[i][k];
                float* src = inbuffer + i;
                float* dst = outbuffer + i;
                for (unsigned int n = 0; n < length; n++)
                {
                    *dst += resonator.Process(*src + denormalFix);
                    src += inchannels;
                    dst += outchannels;
                }
            }
        }

#if UNITY_SPU
        UNITY_PS3_CELLDMA_PUT(&g_EffectData, state->effectdata, sizeof(g_EffectData));
#endif

        return UNITY_AUDIODSP_OK;
    }

#endif
}
