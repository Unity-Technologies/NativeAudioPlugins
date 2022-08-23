#include "AudioPluginUtil.h"

#if PLATFORM_OSX | PLATFORM_LINUX | PLATFORM_WIN

#include "TeleportLib.cpp"

namespace Teleport
{
    enum Param
    {
        P_STREAM,
        P_SEND,
        P_INPUTGAIN,
        P_TELEPORTGAIN,
        P_PARAM1,
        P_PARAM2,
        P_PARAM3,
        P_PARAM4,
        P_NUM
    };

    struct EffectData
    {
        float p[P_NUM];
    };

    int InternalRegisterEffectDefinition(UnityAudioEffectDefinition& definition)
    {
        int numparams = P_NUM;
        definition.paramdefs = new UnityAudioParameterDefinition[numparams];
        AudioPluginUtil::RegisterParameter(definition, "Stream", "", 0.0f, 7.0f, 0.0f, 1.0f, 1.0f, P_STREAM, "External audio stream to read from or write to.");
        AudioPluginUtil::RegisterParameter(definition, "Send", "", 0.0f, 1.0f, 0.0f, 1.0f, 1.0f, P_SEND, "Gain of signal sent to the external audio stream.");
        AudioPluginUtil::RegisterParameter(definition, "Input Gain", "", 0.0f, 1.0f, 1.0f, 1.0f, 1.0f, P_INPUTGAIN, "Gain of signal passing through effect.");
        AudioPluginUtil::RegisterParameter(definition, "Teleport Gain", "", 0.0f, 1.0f, 1.0f, 1.0f, 1.0f, P_TELEPORTGAIN, "Gain of the received external audio stream.");
        AudioPluginUtil::RegisterParameter(definition, "Param1", "", 0.0f, 1.0f, 0.0f, 1.0f, 1.0f, P_PARAM1, "User-defined parameter 1 (read/write)");
        AudioPluginUtil::RegisterParameter(definition, "Param2", "", 0.0f, 1.0f, 0.0f, 1.0f, 1.0f, P_PARAM2, "User-defined parameter 2 (read/write)");
        AudioPluginUtil::RegisterParameter(definition, "Param3", "", 0.0f, 1.0f, 0.0f, 1.0f, 1.0f, P_PARAM3, "User-defined parameter 3 (read/write)");
        AudioPluginUtil::RegisterParameter(definition, "Param4", "", 0.0f, 1.0f, 0.0f, 1.0f, 1.0f, P_PARAM4, "User-defined parameter 4 (read/write)");
        return numparams;
    }

    UNITY_AUDIODSP_RESULT UNITY_AUDIODSP_CALLBACK CreateCallback(UnityAudioEffectState* state)
    {
        EffectData* data = new EffectData;
        memset(data, 0, sizeof(EffectData));
        state->effectdata = data;
        AudioPluginUtil::InitParametersFromDefinitions(InternalRegisterEffectDefinition, data->p);
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

        int stream = (int)data->p[P_STREAM];
        assert(stream >= 0);
        assert(stream < Teleport::NUMSTREAMS);

        SharedMemoryHandle& shared = GetSharedMemory();
        Stream& s = shared->streams[stream];

        length *= outchannels;
        if (data->p[P_SEND] >= 0.5f)
        {
            for (unsigned int n = 0; n < length; n++)
            {
                float x = inbuffer[n];
                s.Feed(x * data->p[P_TELEPORTGAIN]);
                outbuffer[n] = x * data->p[P_INPUTGAIN];
            }
        }
        else
        {
            for (unsigned int n = 0; n < length; n++)
            {
                float x = 0.0f;
                s.Read(x);
                outbuffer[n] = inbuffer[n] * data->p[P_INPUTGAIN] + x * data->p[P_TELEPORTGAIN];
            }
        }

        return UNITY_AUDIODSP_OK;
    }

    UNITY_AUDIODSP_RESULT UNITY_AUDIODSP_CALLBACK SetFloatParameterCallback(UnityAudioEffectState* state, int index, float value)
    {
        EffectData* data = state->GetEffectData<EffectData>();

        if (index >= P_NUM)
            return UNITY_AUDIODSP_ERR_UNSUPPORTED;

        if (index >= P_PARAM1 && index <= P_PARAM4)
        {
            int stream = (int)data->p[P_STREAM];
            assert(stream >= 0);
            assert(stream < Teleport::NUMSTREAMS);

            SharedMemoryHandle& shared = GetSharedMemory();
            Stream& s = shared->streams[stream];

            s.params[index - P_PARAM1].changed = 1;
            s.params[index - P_PARAM1].value = value;
        }
        else
            data->p[index] = value;

        return UNITY_AUDIODSP_OK;
    }

    UNITY_AUDIODSP_RESULT UNITY_AUDIODSP_CALLBACK GetFloatParameterCallback(UnityAudioEffectState* state, int index, float* value, char *valuestr)
    {
        EffectData* data = state->GetEffectData<EffectData>();

        if (index >= P_NUM)
            return UNITY_AUDIODSP_ERR_UNSUPPORTED;

        if (value != NULL)
        {
            if (index >= P_PARAM1 && index <= P_PARAM4)
            {
                int stream = (int)data->p[P_STREAM];
                assert(stream >= 0);
                assert(stream < Teleport::NUMSTREAMS);

                SharedMemoryHandle& shared = GetSharedMemory();
                Stream& s = shared->streams[stream];

                *value = s.params[index - P_PARAM1].value;
            }
            else
                *value = data->p[index];
        }

        if (valuestr != NULL)
            valuestr[0] = 0;

        return UNITY_AUDIODSP_OK;
    }

    int UNITY_AUDIODSP_CALLBACK GetFloatBufferCallback(UnityAudioEffectState* state, const char* name, float* buffer, int numsamples)
    {
        return UNITY_AUDIODSP_OK;
    }
}

#endif
