#include "AudioPluginUtil.h"

namespace Granulator
{
    const int MAXGRAINS = 500;
    const int MAXLENGTH = 0x100000;
    const int MAXSAMPLE = 16;

    Mutex sampleMutex;

    struct GranulatorSample
    {
        float* data;
        float* integrated;
        int numsamples;
        int numchannels;
        int samplerate;
        int updatecount;
        int allocated;
        char name[1024];
    };

    inline GranulatorSample& GetGranulatorSample(int index)
    {
        static bool initialized = false;
        static GranulatorSample samples[MAXSAMPLE];
        if (!initialized)
        {
            memset(samples, 0, sizeof(samples));
            initialized = true;
        }
        return samples[index];
    }

    int globalupdatecount = 0;

    enum Param
    {
        P_SPEED,
        P_WLEN,
        P_RSPEED,
        P_ROFS,
        P_RWLEN,
        P_NUMGRAINS,
        P_FREEZE,
        P_OFFSET,
        P_RSTP,
        P_USESAMPLE,
        P_NUM
    };

    struct Grain
    {
        float offset;
        int length;
        int numsamples;
        int numchannels;
        const float* src;
        float pos;
        float speed;
        inline void Setup(Random& random, const float wavesamplerate, const float samplerate, const float sampletime, const int writepos, const float* params, const float* _src, int _numsamples, int _numchannels)
        {
            src = _src;
            numsamples = _numsamples;
            numchannels = _numchannels;
            float maxtime = (float)(numsamples - 1);
            length = (int)(maxtime * random.GetFloat(params[P_WLEN], params[P_WLEN] + params[P_RWLEN]));
            float invlength = 1.0f / (float)length;
            float relpos = maxtime * (random.GetFloat(params[P_OFFSET], params[P_OFFSET] + params[P_ROFS]));
            offset = writepos + relpos;
            speed = FastMax(0.001f, random.GetFloat(params[P_SPEED], params[P_SPEED] + params[P_RSPEED])) * invlength * wavesamplerate * sampletime;
            pos = -speed * samplerate * random.GetFloat(0.0f, params[P_RSTP]);
        }

        inline float Scan()
        {
            float p = FastMax(0.0f, pos);
            float amp = 1.0f - fabsf(p + p - 1.0f);
            p = offset + p * length;
            int i = (int)floorf(p);
            p -= i;
            while (i >= numsamples)
                i -= numsamples;
            float s1 = src[numchannels * i++];
            while (i >= numsamples)
                i -= numsamples;
            float s2 = src[numchannels * i];
            pos = FastMin(pos + speed, 1.0f);
            return (s1 + (s2 - s1) * p) * amp;
        }
    };

    struct EffectData
    {
        float p[P_NUM];
        Random random;
        struct Channel
        {
            int writepos;
            float env;
            float previewsum;
            Grain grains[MAXGRAINS];
            float delay[MAXLENGTH];
            float preview[MAXLENGTH];
        } channel[8];
    };

    int InternalRegisterEffectDefinition(UnityAudioEffectDefinition& definition)
    {
        int numparams = P_NUM;
        definition.paramdefs = new UnityAudioParameterDefinition[numparams];
        RegisterParameter(definition, "Speed", "%", 0.001f, 25.0f, 0.01f, 100.0f, 2.5f, P_SPEED, "The speed in samples at which the grains will be replayed relative to the original");
        RegisterParameter(definition, "Window len", "%", 0.001f, 1.0f, 0.1f, 100.0f, 2.5f, P_WLEN, "Length of grain in seconds");
        RegisterParameter(definition, "Rnd speed", "%", 0.0f, 5.0f, 0.0f, 100.0f, 2.5f, P_RSPEED, "Randomized amount of speed in samples");
        RegisterParameter(definition, "Rnd offset", "%", 0.0f, 1.0f, 0.0f, 100.0f, 2.5f, P_ROFS, "Randomized offset in seconds");
        RegisterParameter(definition, "Rnd window len", "%", 0.0f, 1.0f, 0.0f, 100.0f, 2.5f, P_RWLEN, "Randomized amount of grain length in seconds");
        RegisterParameter(definition, "Num grains", "", 1.0f, MAXGRAINS, 5.0f, 1.0f, 1.0f, P_NUMGRAINS, "Number of grains");
        RegisterParameter(definition, "Freeze", "", 0.0f, 1.0f, 0.0f, 1.0f, 1.0f, P_FREEZE, "Freeze threshold (only for live input)");
        RegisterParameter(definition, "Offset", "%", 0.0f, 1.0f, 0.0f, 100.0f, 1.0f, P_OFFSET, "Offset in recorded or sampled waveform");
        RegisterParameter(definition, "Random startpos", "s", 0.01f, 2.0f, 0.5f, 1.0f, 2.5f, P_RSTP, "Random start position");
        RegisterParameter(definition, "Use Sample", "", -1.0f, MAXSAMPLE - 1, -1.0f, 1.0f, 1.0f, P_USESAMPLE, "-1 = use live input, otherwise indicates the slot of a sample uploaded by scripts via Granulator_UploadSample");
        return numparams;
    }

    void ResetGrains(EffectData* data)
    {
        for (int c = 0; c < 8; c++)
            for (int n = 0; n < MAXGRAINS; n++)
                data->channel[c].grains[n].pos = 1.0f;
    }

    UNITY_AUDIODSP_RESULT UNITY_AUDIODSP_CALLBACK CreateCallback(UnityAudioEffectState* state)
    {
        EffectData* data = new EffectData;
        memset(data, 0, sizeof(EffectData));
        ResetGrains(data);
        state->effectdata = data;
        InitParametersFromDefinitions(InternalRegisterEffectDefinition, data->p);
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
        EffectData* data = state->GetEffectData<EffectData>();
        if (strncmp(name, "Waveform", 8) == 0)
        {
            int channel = name[8] - '0';
            int useSample = (int)data->p[P_USESAMPLE];
            MutexScopeLock mutexScope(Granulator::sampleMutex, useSample >= 0);
            GranulatorSample* gs = (useSample >= 0) ? &GetGranulatorSample(useSample) : NULL;
            if (gs != NULL && gs->numsamples == 0)
                gs = NULL;
            int wp = (gs == NULL) ? data->channel[channel].writepos : 0;
            int numchannels = (gs != NULL) ? gs->numchannels : 1;
            int wavelength = (gs != NULL) ? gs->numsamples : MAXLENGTH;
            const float* src = (gs != NULL) ? (gs->data + channel) : data->channel[channel].preview;
            float scale = (float)(wavelength - 2) / (float)numsamples;
            float invscale = 1.0f / scale, prev = 0.0f;
            for (int n = 0; n < numsamples; n++)
            {
                float f = n * scale;
                int i = (int)floorf(f);
                f -= i;
                if (gs == NULL)
                    i += wp;
                while (i < 0)
                    i += wavelength;
                while (i >= wavelength)
                    i -= wavelength;
                float s1 = src[numchannels * i++];
                while (i >= wavelength)
                    i -= wavelength;
                float s2 = src[numchannels * i];
                float curr = s1 + (s2 - s1) * f;
                buffer[n] = (curr - prev) * invscale;
                prev = curr;
            }
        }
        return UNITY_AUDIODSP_OK;
    }

    UNITY_AUDIODSP_RESULT UNITY_AUDIODSP_CALLBACK ProcessCallback(UnityAudioEffectState* state, float* inbuffer, float* outbuffer, unsigned int length, int inchannels, int outchannels)
    {
        EffectData* data = state->GetEffectData<EffectData>();

        const int samplerate = state->samplerate;
        const float sampletime = 1.0f / (float)samplerate;
        const float freeze = data->p[P_FREEZE];
        const int useSample = (int)data->p[P_USESAMPLE];
        const int numgrains = (int)data->p[P_NUMGRAINS];
        const float* params = data->p;

        MutexScopeLock mutexScope(Granulator::sampleMutex, useSample >= 0);

        GranulatorSample* gs = (useSample >= 0) ? &GetGranulatorSample(useSample) : NULL;
        if (gs != NULL && gs->numsamples == 0)
            gs = NULL;

        EffectData::Channel* ch = data->channel;
        for (int c = 0; c < inchannels; c++)
        {
            const float* src = inbuffer + c;

            for (int n = 0; n < length; n++)
            {
                float input = *src;
                src += inchannels;

                ch->env += (fabsf(input) - ch->env) * 0.01f;
                if (ch->env > freeze)
                {
                    ch->previewsum = ch->previewsum * 0.999f + input;
                    ch->delay[ch->writepos] = input;
                    ch->preview[ch->writepos] = ch->previewsum;
                    ch->writepos = (ch->writepos + MAXLENGTH - 1) & (MAXLENGTH - 1);
                }
            }
        }

        memset(outbuffer, 0, length * outchannels * sizeof(float));

        if (state->flags & (UnityAudioEffectStateFlags_IsMuted | UnityAudioEffectStateFlags_IsPaused))
            return UNITY_AUDIODSP_OK;

        if (!state->flags & UnityAudioEffectStateFlags_IsPlaying)
        {
            ResetGrains(data);
            return UNITY_AUDIODSP_OK;
        }

        for (int c = 0; c < inchannels; c++)
        {
            Grain* g = ch->grains;
            for (int i = 0; i < numgrains; i++)
            {
                float* dst = outbuffer + c;
                for (int n = 0; n < length; n++)
                {
                    if ((int)(g->pos + 0.00001f) >= 1)
                    {
                        if (gs == NULL)
                            g->Setup(data->random, samplerate, samplerate, sampletime, ch->writepos, params, ch->delay, MAXLENGTH, 1);
                        else
                            g->Setup(data->random, gs->samplerate, samplerate, sampletime, 0, params, gs->data, gs->numsamples + c, gs->numchannels);
                    }
                    *dst += g->Scan();
                    dst += outchannels;
                }
                ++g;
            }

            ch++;
        }

        return UNITY_AUDIODSP_OK;
    }
}

extern "C" UNITY_AUDIODSP_EXPORT_API bool Granulator_UploadSample(int index, float* data, int numsamples, int numchannels, int samplerate, const char* name)
{
    if (index < 0 || index >= Granulator::MAXSAMPLE)
        return false;
    MutexScopeLock mutexScope(Granulator::sampleMutex);
    Granulator::GranulatorSample& s = Granulator::GetGranulatorSample(index);
    if (s.allocated)
        delete[] s.data;
    int num = numsamples * numchannels;
    if (num > 0)
    {
        s.data = new float[num];
        s.integrated = new float[num];
        s.allocated = 1;
        strcpy(s.name, name);
        memcpy(s.data, data, num * sizeof(float));
        for (int c = 0; c < numchannels; c++)
        {
            double sum = 0.0;
            for (int n = 0; n < numsamples; n++)
            {
                sum += s.data[n * numchannels + c];
                s.integrated[n * numchannels + c] = (float)sum;
            }
        }
    }
    else
    {
        s.data = NULL;
        s.integrated = NULL;
        s.allocated = 0;
    }
    s.numsamples = numsamples;
    s.numchannels = numchannels;
    s.samplerate = samplerate;
    s.updatecount = ++Granulator::globalupdatecount;
    return true;
}

extern "C" UNITY_AUDIODSP_EXPORT_API const char* Granulator_GetSampleName(int index)
{
    if (index < 0)
        return "Input";

    if (index < Granulator::MAXSAMPLE) ;
    {
        MutexScopeLock mutexScope(Granulator::sampleMutex);
        Granulator::GranulatorSample* s = &Granulator::GetGranulatorSample(index);
        if (s->numsamples == 0)
            return "Undefined";
        return s->name;
    }

    return "Undefined";
}
