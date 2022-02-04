#include "AudioPluginUtil.h"

namespace Routing
{
    const int MAXINDEX = 16;

    enum Param
    {
        P_TARGET,
        P_NUM
    };

    int bufferchannels[MAXINDEX];
    AudioPluginUtil::RingBuffer<65536> buffer[MAXINDEX];

    struct EffectData
    {
        float p[P_NUM];
    };

    int InternalRegisterEffectDefinition(UnityAudioEffectDefinition& definition)
    {
        int numparams = P_NUM;
        definition.paramdefs = new UnityAudioParameterDefinition[numparams];
        AudioPluginUtil::RegisterParameter(definition, "Target", "", 0.0f, MAXINDEX - 1, 0.0f, 1.0f, 1.0f, P_TARGET, "Specifies the output that the input signal is routed to. This can be read by scripts via RoutingDemo_GetData");
        for (int i = 0; i < MAXINDEX; i++)
            buffer[i].Clear();
        return numparams;
    }

    UNITY_AUDIODSP_RESULT UNITY_AUDIODSP_CALLBACK CreateCallback(UnityAudioEffectState* state)
    {
        EffectData* effectdata = new EffectData;
        memset(effectdata, 0, sizeof(EffectData));
        state->effectdata = effectdata;
        AudioPluginUtil::InitParametersFromDefinitions(InternalRegisterEffectDefinition, effectdata->p);
        return UNITY_AUDIODSP_OK;
    }

    UNITY_AUDIODSP_RESULT UNITY_AUDIODSP_CALLBACK ReleaseCallback(UnityAudioEffectState* state)
    {
        EffectData* data = state->GetEffectData<EffectData>();
        delete data;
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
        EffectData* data = state->GetEffectData<EffectData>();

        memcpy(outbuffer, inbuffer, sizeof(float) * length * inchannels);

        int target = (int)data->p[P_TARGET];
        if (!(state->flags & UnityAudioEffectStateFlags_IsPlaying) && (state->flags & (UnityAudioEffectStateFlags_IsMuted | UnityAudioEffectStateFlags_IsPaused)))
            Routing::buffer[target].SyncWritePos();

        bufferchannels[target] = inchannels;

        for (unsigned int n = 0; n < length; n++)
            for (int i = 0; i < inchannels; i++)
                buffer[target].Feed(inbuffer[n * inchannels + i]);

        return UNITY_AUDIODSP_OK;
    }
}

extern "C" UNITY_AUDIODSP_EXPORT_API void RoutingDemo_GetData(int target, float* data, int numsamples, int numchannels)
{
    if (target < 0 || target >= Routing::MAXINDEX)
        return;
    int skipchannels = Routing::bufferchannels[target] - numchannels; if (skipchannels < 0)
        skipchannels = 0;
    int zerochannels = numchannels - Routing::bufferchannels[target]; if (zerochannels < 0)
        zerochannels = 0;
    for (int n = 0; n < numsamples; n++)
    {
        for (int i = 0; i < numchannels; i++)
            Routing::buffer[target].Read(data[n * numchannels + i]);
        Routing::buffer[target].Skip(skipchannels);
        for (int i = 0; i < zerochannels; i++)
            data[n * numchannels + i + numchannels - zerochannels] = 0.0f;
    }
}
