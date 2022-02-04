#include "AudioPluginUtil.h"

namespace Lofinator
{
    enum Param
    {
        P_DECRATE,
        P_QUANT,
        P_NUM
    };

    struct EffectData
    {
        struct Data
        {
            float p[P_NUM];
            float samplehold[8];
            float sampleholdcount[8];
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
        AudioPluginUtil::RegisterParameter(definition, "Decimation", "Hz", 0.1f, 24000.0f, 24000.0f, 1.0f, 3.0f, P_DECRATE, "Decimation rate. Determines the interval at which new samples are read and held.");
        AudioPluginUtil::RegisterParameter(definition, "Quantization", "", 1.0f, 24.0f, 8.0f, 1.0f, 1.0f, P_QUANT, "Word length in bits by which the signal will be quantized.");
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

    UNITY_AUDIODSP_RESULT UNITY_AUDIODSP_CALLBACK ProcessCallback(UnityAudioEffectState* state, float* inbuffer, float* outbuffer, unsigned int length, int inchannels, int outchannels)
    {
        EffectData::Data* data = &state->GetEffectData<EffectData>()->data;

        float quant1 = powf(2.0f, data->p[P_QUANT]);
        float quant2 = 1.0f / quant1;
        float decrate = (float)state->samplerate / data->p[P_DECRATE];
        for (unsigned int n = 0; n < length; n++)
        {
            for (int i = 0; i < outchannels; i++)
            {
                data->sampleholdcount[i] += 1.0f;
                if (data->sampleholdcount[i] >= decrate)
                {
                    data->sampleholdcount[i] -= decrate;
                    data->samplehold[i] = inbuffer[n * outchannels + i];
                }
                outbuffer[n * outchannels + i] = (signed int)(data->samplehold[i] * quant1) * quant2;
            }
        }

        return UNITY_AUDIODSP_OK;
    }
}
