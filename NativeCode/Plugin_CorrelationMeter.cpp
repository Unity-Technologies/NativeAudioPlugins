#include "AudioPluginUtil.h"

namespace CorrelationMeter
{
    enum Param
    {
        P_Window,
        P_Scale,
        P_NUM
    };

    struct EffectData
    {
        float p[P_NUM];
        AudioPluginUtil::HistoryBuffer history[8];
        int numchannels;
    };

    int InternalRegisterEffectDefinition(UnityAudioEffectDefinition& definition)
    {
        int numparams = P_NUM;
        definition.paramdefs = new UnityAudioParameterDefinition[numparams];
        AudioPluginUtil::RegisterParameter(definition, "Window", "s", 0.1f, 2.0f, 0.15f, 1.0f, 1.0f, P_Window, "Length of analysis window (note: longer windows slow down framerate)");
        AudioPluginUtil::RegisterParameter(definition, "Scale", "%", 0.01f, 10.0f, 1.0f, 100.0f, 1.0f, P_Scale, "Amplitude scaling for monitored signal");
        return numparams;
    }

    UNITY_AUDIODSP_RESULT UNITY_AUDIODSP_CALLBACK CreateCallback(UnityAudioEffectState* state)
    {
        EffectData* data = new EffectData;
        memset(data, 0, sizeof(EffectData));
        AudioPluginUtil::InitParametersFromDefinitions(InternalRegisterEffectDefinition, data->p);
        state->effectdata = data;
        for (int i = 0; i < 8; i++)
            data->history[i].Init(state->samplerate * 2);
        return UNITY_AUDIODSP_OK;
    }

    UNITY_AUDIODSP_RESULT UNITY_AUDIODSP_CALLBACK ReleaseCallback(UnityAudioEffectState* state)
    {
        EffectData* data = state->GetEffectData<EffectData>();
        delete data;
        return UNITY_AUDIODSP_OK;
    }

    UNITY_AUDIODSP_RESULT UNITY_AUDIODSP_CALLBACK ProcessCallback(UnityAudioEffectState* state, float* inbuffer, float* outbuffer, unsigned int length, int inchannels, int outchannels)
    {
        EffectData* data = state->GetEffectData<EffectData>();

        memcpy(outbuffer, inbuffer, sizeof(float) * length * inchannels);

        for (unsigned int n = 0; n < length; n++)
            for (int i = 0; i < inchannels; i++)
                data->history[i].Feed(*inbuffer++);
        data->numchannels = inchannels;

        return UNITY_AUDIODSP_OK;
    }

    UNITY_AUDIODSP_RESULT UNITY_AUDIODSP_CALLBACK SetFloatParameterCallback(UnityAudioEffectState* state, int index, float value)
    {
        EffectData* data = state->GetEffectData<EffectData>();
        if (index >= P_NUM)
            return UNITY_AUDIODSP_ERR_UNSUPPORTED;
        data->p[index] = value;
        return UNITY_AUDIODSP_OK;
    }

    UNITY_AUDIODSP_RESULT UNITY_AUDIODSP_CALLBACK GetFloatParameterCallback(UnityAudioEffectState* state, int index, float* value, char *valuestr)
    {
        EffectData* data = state->GetEffectData<EffectData>();
        if (value != NULL)
            *value = data->p[index];
        if (valuestr != NULL)
            valuestr[0] = 0;
        return UNITY_AUDIODSP_OK;
    }

    int UNITY_AUDIODSP_CALLBACK GetFloatBufferCallback(UnityAudioEffectState* state, const char* name, float* buffer, int numsamples)
    {
        EffectData* data = state->GetEffectData<EffectData>();
        AudioPluginUtil::HistoryBuffer& l = data->history[0];
        AudioPluginUtil::HistoryBuffer& r = data->history[1];
        int w1 = l.writeindex;
        int w2 = r.writeindex;
        for (int n = 0; n < numsamples / 2; n++)
        {
            buffer[n * 2 + 0] = l.data[w1];
            if (--w1 < 0)
                w1 = l.length - 1;
            if (n * 2 + 1 < numsamples)
                buffer[n * 2 + 1] = r.data[w2];
            if (--w2 < 0)
                w2 = r.length - 1;
        }
        return UNITY_AUDIODSP_OK;
    }
}
