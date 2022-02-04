#include "AudioPluginUtil.h"

namespace WahWah
{
    enum Param
    {
        P_ATK,
        P_REL,
        P_BASE,
        P_SENS,
        P_RESO,
        P_TYPE,
        P_DEPTH,
        P_SIDECHAIN,
        P_NUM
    };

    struct EffectData
    {
        struct Data
        {
            float p[P_NUM];
            struct Channel
            {
                AudioPluginUtil::StateVariableFilter filter1;
                AudioPluginUtil::StateVariableFilter filter2;
                float env;
            } channels[8];
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
        AudioPluginUtil::RegisterParameter(definition, "Attack Time", "s", 0.001f, 2.0f, 0.1f, 1.0f, 3.0f, P_ATK, "Attack time");
        AudioPluginUtil::RegisterParameter(definition, "Release Time", "s", 0.001f, 2.0f, 0.5f, 1.0f, 3.0f, P_REL, "Release time");
        AudioPluginUtil::RegisterParameter(definition, "Base Level", "%", 0.0f, 1.0f, 0.1f, 100.0f, 1.0f, P_BASE, "Base filter level");
        AudioPluginUtil::RegisterParameter(definition, "Sensitivity", "%", -1.0f, 1.0f, 0.1f, 100.0f, 1.0f, P_SENS, "Filter sensitivity");
        AudioPluginUtil::RegisterParameter(definition, "Resonance", "%", 0.0f, 1.0f, 0.1f, 100.0f, 1.0f, P_RESO, "Filter resonance");
        AudioPluginUtil::RegisterParameter(definition, "Type", "", 0.0f, 1.0f, 0.0f, 1.0f, 1.0f, P_TYPE, "Filter type (0 = lowpass, 1 = bandpass)");
        AudioPluginUtil::RegisterParameter(definition, "Depth", "", 0.0f, 1.0f, 0.0f, 1.0f, 1.0f, P_DEPTH, "Filter depth (0 = 12 dB, 1 = 24 dB)");
        AudioPluginUtil::RegisterParameter(definition, "Sidechain Mix", "%", 0.0f, 1.0f, 0.0f, 100.0f, 1.0f, P_SIDECHAIN, "Sidechain mix (0 = use input, 1 = use sidechain)");
        definition.flags |= UnityAudioEffectDefinitionFlags_IsSideChainTarget;
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

        const float atksamples = data->p[P_ATK] * state->samplerate;
        const float relsamples = data->p[P_REL] * state->samplerate;
        const float atkconst = (atksamples <= 1.0f) ? 1.0f : (1.0f - powf(0.01f, 1.0f / atksamples));
        const float relconst = (relsamples <= 1.0f) ? 1.0f : (1.0f - powf(0.01f, 1.0f / relsamples));
        const float bw = powf(1.0f - 0.999f * data->p[P_RESO], 3.0f);

        for (int i = 0; i < inchannels; i++)
        {
            EffectData::Data::Channel& ch = data->channels[i];
            ch.filter1.bandwidth = bw;
            ch.filter2.bandwidth = bw;
            float* src = inbuffer + i;
            float* dst = outbuffer + i;
            float* sc = state->sidechainbuffer + i;
            for (unsigned int n = 0; n < length; n++)
            {
                float s = *src;
                float a = fabsf(s + (*sc - s) * data->p[P_SIDECHAIN]);
                ch.env += (a - ch.env) * ((a > ch.env) ? atkconst : relconst);
                ch.filter1.cutoff = AudioPluginUtil::FastClip(data->p[P_BASE] + ch.env * data->p[P_SENS], 0.0f, 1.4f);
                ch.filter2.cutoff = ch.filter1.cutoff;
                ch.filter2.ProcessLPF(ch.filter1.ProcessLPF(*src));
                float lpf = ch.filter1.lpf + (ch.filter2.lpf - ch.filter1.lpf) * data->p[P_DEPTH];
                float bpf = ch.filter1.bpf + (ch.filter2.bpf - ch.filter1.bpf) * data->p[P_DEPTH];
                *dst = lpf + (bpf - lpf) * data->p[P_TYPE];
                src += inchannels;
                dst += outchannels;
                sc += inchannels;
            }
        }

        return UNITY_AUDIODSP_OK;
    }
}
