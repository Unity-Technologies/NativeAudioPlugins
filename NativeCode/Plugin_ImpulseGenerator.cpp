#include "AudioPluginUtil.h"

namespace ImpulseGenerator
{
    enum Param
    {
        P_BASEPERIOD,
        P_RANDOMPERIOD,
        P_BASEAMP,
        P_RANDOMAMP,
        P_BASEDECAY,
        P_RANDOMDECAY,
        P_NOISEADD,
        P_NOISEMIX,
        P_NUM
    };

    struct EffectData
    {
        struct Data
        {
            float p[P_NUM];
            float level;
            float decay;
            float samplesleft;
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
        AudioPluginUtil::RegisterParameter(definition, "Base Period", "ms", 0.0f, 5000.0f, 200.0f, 1.0f, 3.0f, P_BASEPERIOD, "Base time between impulses");
        AudioPluginUtil::RegisterParameter(definition, "Random Period", "ms", 0.0f, 5000.0f, 100.0f, 1.0f, 3.0f, P_RANDOMPERIOD, "Random time between impulses");
        AudioPluginUtil::RegisterParameter(definition, "Base Amp", "", 0.0f, 1.0f, 0.5f, 1.0f, 1.0f, P_BASEAMP, "Base amplitude of impulses");
        AudioPluginUtil::RegisterParameter(definition, "Random Amp", "", 0.0f, 1.0f, 0.5f, 1.0f, 3.0f, P_RANDOMAMP, "Random amplitude of impulses");
        AudioPluginUtil::RegisterParameter(definition, "Base Decay", "ms", 0.0f, 10000.0f, 10.0f, 1.0f, 3.0f, P_BASEDECAY, "Decay time of impulses");
        AudioPluginUtil::RegisterParameter(definition, "Random Decay", "ms", 0.0f, 10000.0f, 0.0f, 1.0f, 3.0f, P_RANDOMDECAY, "Decay time of impulses");
        AudioPluginUtil::RegisterParameter(definition, "Noise Level", "", 0.0f, 50.0f, 0.0f, 1.0f, 3.0f, P_NOISEADD, "Level of additive white noise");
        AudioPluginUtil::RegisterParameter(definition, "Noise Mix", "", 0.0f, 1.0f, 0.0f, 1.0f, 3.0f, P_NOISEMIX, "Level of white noise multiplied by impulse");
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

        if ((state->flags & UnityAudioEffectStateFlags_IsPlaying) == 0 || (state->flags & (UnityAudioEffectStateFlags_IsMuted | UnityAudioEffectStateFlags_IsPaused)) != 0)
        {
            memset(outbuffer, 0, sizeof(float) * length * outchannels);
            return UNITY_AUDIODSP_OK;
        }

        for (unsigned int n = 0; n < length; n++)
        {
            data->samplesleft -= 1.0f;
            if (data->samplesleft <= 0.0f)
            {
                float decaytime = data->p[P_BASEDECAY] + data->random.GetFloat(0.0f, data->p[P_RANDOMDECAY]);
                data->decay = (decaytime <= 0.0f) ? 0.0f : powf(0.001f, 1000.0f / (state->samplerate * decaytime));
                data->level = data->p[P_BASEAMP] + data->random.GetFloat(0.0f, data->p[P_RANDOMAMP]);
                data->samplesleft = AudioPluginUtil::FastMax(0.0f, data->samplesleft + (data->p[P_BASEPERIOD] + data->random.GetFloat(0.0f, data->p[P_RANDOMPERIOD])) * 0.001f * state->samplerate);
            }
            float noise = data->random.GetFloat(-1.0f, 1.0f);
            float s = noise * data->p[P_NOISEADD] + data->level + data->p[P_NOISEMIX] * (noise * data->level - data->level);
            for (int i = 0; i < outchannels; i++)
                outbuffer[i] = inbuffer[i] + s;
            inbuffer += inchannels;
            outbuffer += outchannels;
            data->level *= data->decay;
        }

        return UNITY_AUDIODSP_OK;
    }
}
