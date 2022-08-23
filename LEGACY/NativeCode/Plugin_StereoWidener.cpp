#include "AudioPluginUtil.h"

namespace StereoWidener
{
    enum Param
    {
        P_AMOUNT,
        P_PREDLY,
        P_POSTDLY,
        P_NUM
    };

    const int kDelayLen = 0x80;

    struct EffectData
    {
        struct Data
        {
            float p[P_NUM];
            int writepos;
            float delay1[kDelayLen * 8];
            float delay2[kDelayLen * 8];
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
        AudioPluginUtil::RegisterParameter(definition, "Amount", "%", -10.0f, 10.0f, 1.0f, 100.0f, 1.0f, P_AMOUNT, "Amount of stereo widening applied. The default of 100% corresponds to the original stereo width of the input signal.");
        AudioPluginUtil::RegisterParameter(definition, "Pre-delay", "ms", -0.0025f, 0.0025f, 0.0f, 1000.0f, 3.0f, P_PREDLY, "Pre-delay applied before the stereo widening.");
        AudioPluginUtil::RegisterParameter(definition, "Post-delay", "ms", -0.0025f, 0.0025f, 0.0f, 1000.0f, 3.0f, P_POSTDLY, "Post-delay applied after the stereo widening.");
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

        memcpy(outbuffer, inbuffer, sizeof(float) * length * inchannels);

        const float amount = 0.5f * data->p[P_AMOUNT];
        const int predelay = kDelayLen - (int)fabsf(data->p[P_PREDLY] * state->samplerate);
        const int postdelay = kDelayLen - (int)fabsf(data->p[P_POSTDLY] * state->samplerate);
        const float predelaygain = (data->p[P_PREDLY] > 0.0f) ? 0.0f : 1.0f;
        const float postdelaygain = (data->p[P_POSTDLY] > 0.0f) ? 0.0f : 1.0f;
        for (unsigned int n = 0; n < length; n++)
        {
            float li = inbuffer[n * inchannels];
            float ri = inbuffer[n * inchannels + 1];
            data->delay1[data->writepos] = li + predelaygain * (ri - li);
            float d1 = data->delay1[(data->writepos + predelay) & (kDelayLen - 1)];
            float l = li + (d1 - li) * (1.0f - predelaygain);
            float r = ri + (d1 - ri) * predelaygain;
            float m = (l + r) * 0.5f;
            float s = (l - m) * amount;
            float lo = m + s;
            float ro = m - s;
            data->delay2[data->writepos] = lo + postdelaygain * (ro - lo);
            float d2 = data->delay2[(data->writepos + postdelay) & (kDelayLen - 1)];
            outbuffer[n * outchannels] = lo + (d2 - lo) * (1.0f - postdelaygain);
            outbuffer[n * outchannels + 1] = ro + (d2 - ro) * postdelaygain;
            data->writepos = (data->writepos + 1) & (kDelayLen - 1);
        }

        return UNITY_AUDIODSP_OK;
    }
}
