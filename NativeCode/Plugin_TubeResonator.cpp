#include "AudioPluginUtil.h"

namespace TubeResonator
{
    const int MAXSECTIONS = 10;

    enum Param
    {
        P_NUMSECTIONS,
        P_FB,
        P_NL,
        P_MIKEPOS,
        P_L1,
        P_A1,
        P_NUM = P_L1 + MAXSECTIONS * 2
    };

    struct Delay
    {
        enum { MAXLEN = 1 << 8, MASK = MAXLEN - 1 };
        float delay;
        float input;
        float output;
        int writepos;
        float data[MAXLEN + 1];
        inline void Process()
        {
            data[writepos] = input;
            float f = writepos + MASK - delay;
            int r = AudioPluginUtil::FastFloor(f);
            f -= r;
            r &= MASK;
            writepos = (writepos + 1) & MASK;
            float s1 = data[r & MASK];
            float s2 = data[(r + 1) & MASK];
            output = s1 + (s2 - s1) * f;
        }
    };

    struct Section
    {
        float coeff;
        Delay upper;
        Delay lower;
    };

    struct EffectData
    {
        struct Data
        {
            float p[P_NUM];
            struct Channel
            {
                Section section[MAXSECTIONS];
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
        AudioPluginUtil::RegisterParameter(definition, "Num sections", "", 1.0f, (float)MAXSECTIONS, 3.0f, 1.0f, 1.0f, P_NUMSECTIONS, "Number of sections");
        AudioPluginUtil::RegisterParameter(definition, "Feedback", "%", 0.0f, 1.0f, 0.5f, 100.0f, 1.0f, P_FB, "Feedback");
        AudioPluginUtil::RegisterParameter(definition, "Nonlinearity", "%", 0.0f, 1.0f, 0.0f, 100.0f, 1.0f, P_NL, "Amount of nonlinearity at reflection");
        AudioPluginUtil::RegisterParameter(definition, "Mike position", "%", 0.0f, 1.0f, 0.0f, 100.0f, 1.0f, P_MIKEPOS, "Microphone position");
        for (int n = 0; n < MAXSECTIONS; n++)
        {
            AudioPluginUtil::RegisterParameter(definition, AudioPluginUtil::tmpstr(0, "Length %d", n + 1), "cm", 0.01f, (float)Delay::MASK * (34000.0f / 48000.0f), 7.0f, 1.0f, 3.0f, P_L1 + n * 2, AudioPluginUtil::tmpstr(1, "Section %d length", n + 1));
            AudioPluginUtil::RegisterParameter(definition, AudioPluginUtil::tmpstr(0, "Radius %d", n + 1), "cm", 0.01f, 100.0f, 3.0f, 1.0f, 3.0f, P_A1 + n * 2, AudioPluginUtil::tmpstr(1, "Section %d radius", n + 1));
        }
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

        const float lenscale = state->samplerate / 34000.0f;
        const float fb = -data->p[P_FB];
        const float nl = data->p[P_NL] * 3.0f;
        const int numsections = (int)data->p[P_NUMSECTIONS];
        const int mikepos = (int)(data->p[P_MIKEPOS] * (numsections - 1));

        for (int i = 0; i < inchannels; i++)
        {
            EffectData::Data::Channel& ch = data->channels[i];

            for (int k = 0; k < numsections; k++)
            {
                if (k < numsections - 1)
                {
                    float A1 = data->p[P_A1 + k * 2];
                    float A2 = data->p[P_A1 + k * 2 + 2];
                    ch.section[k].coeff = (A2 - A1) / (A1 + A2);
                }
                const float len = data->p[P_L1 + k * 2] * lenscale - 1.0f;
                ch.section[k].upper.delay = len;
                ch.section[k].lower.delay = len;
            }

            float* src = inbuffer + i;
            float* dst = outbuffer + i;
            for (unsigned int n = 0; n < length; n++)
            {
                float refl = ch.section[numsections - 1].upper.output;
                float r = AudioPluginUtil::FastMin(refl * refl, 1.0f);
                refl += (r * refl - refl) * nl;
                ch.section[0].upper.input = ch.section[0].lower.output + *src;
                ch.section[numsections - 1].lower.input = refl * fb;
                for (int k = 0; k < numsections - 1; k++)
                {
                    float c = ch.section[k].coeff;
                    float u = ch.section[k].upper.output;
                    float l = ch.section[k + 1].lower.output;
                    ch.section[k + 1].upper.input = u * (1.0f + c) - c * l;
                    ch.section[k].lower.input = l * (1.0f - c) + c * u;
                }
                for (int k = 0; k < numsections; k++)
                {
                    ch.section[k].upper.Process();
                    ch.section[k].lower.Process();
                }
                *dst = ch.section[mikepos].upper.output;
                src += inchannels;
                dst += outchannels;
            }
        }

        return UNITY_AUDIODSP_OK;
    }
}
