#include "AudioPluginUtil.h"

namespace LevelMixer
{
    enum Param
    {
        P_GAIN1,
        P_GAIN2,
        P_GAIN3,
        P_GAIN4,
        P_GAIN5,
        P_GAIN6,
        P_GAIN7,
        P_GAIN8,
        P_NUM
    };

    struct EffectData
    {
        struct Data
        {
            float p[P_NUM];
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
        AudioPluginUtil::RegisterParameter(definition, "Gain1", "dB", -120.0f, 50.0f, 0.0f, 1.0f, 1.0f, P_GAIN1, "Gain of input channel 1 (left or mono)");
        AudioPluginUtil::RegisterParameter(definition, "Gain2", "dB", -120.0f, 50.0f, 0.0f, 1.0f, 1.0f, P_GAIN2, "Gain of input channel 2 (right)");
        AudioPluginUtil::RegisterParameter(definition, "Gain3", "dB", -120.0f, 50.0f, 0.0f, 1.0f, 1.0f, P_GAIN3, "Gain of input channel 3");
        AudioPluginUtil::RegisterParameter(definition, "Gain4", "dB", -120.0f, 50.0f, 0.0f, 1.0f, 1.0f, P_GAIN4, "Gain of input channel 4");
        AudioPluginUtil::RegisterParameter(definition, "Gain5", "dB", -120.0f, 50.0f, 0.0f, 1.0f, 1.0f, P_GAIN5, "Gain of input channel 5");
        AudioPluginUtil::RegisterParameter(definition, "Gain6", "dB", -120.0f, 50.0f, 0.0f, 1.0f, 1.0f, P_GAIN6, "Gain of input channel 6");
        AudioPluginUtil::RegisterParameter(definition, "Gain7", "dB", -120.0f, 50.0f, 0.0f, 1.0f, 1.0f, P_GAIN7, "Gain of input channel 7");
        AudioPluginUtil::RegisterParameter(definition, "Gain8", "dB", -120.0f, 50.0f, 0.0f, 1.0f, 1.0f, P_GAIN8, "Gain of input channel 8");
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

        float gains[8];
        for (int i = 0; i < inchannels; i++)
            gains[i] = powf(10.0f, data->p[P_GAIN1 + i] * 0.05f);

        for (unsigned int n = 0; n < length; n++)
            for (int i = 0; i < inchannels; i++)
                outbuffer[n * inchannels + i] = inbuffer[n * inchannels + i] * gains[i];

        return UNITY_AUDIODSP_OK;
    }
}
