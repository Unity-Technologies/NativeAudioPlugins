#include "AudioPluginUtil.h"

namespace Granulator
{
    const int MAXGRAINS = 500;
    const int MAXDELAYLENGTH = 0x40000;
    const int MAXSAMPLE = 16;

    AudioPluginUtil::Mutex sampleMutex;
    int debug_graincount = 0;

    struct GranulatorSample
    {
        float* data;
        float* preview;
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
        P_FREEZE,
        P_OFFSET,
        P_RATE,
        P_RRATE,
        P_PANBASE,
        P_PANRANGE,
        P_SHAPE,
        P_USESAMPLE,
        P_NUM
    };

    struct Grain
    {
        GranulatorSample* sample;
        int channel;
        int length;
        int wrapping;
        float offset;
        float pos;
        float speed;
        float pan;
        float shape;

        // Note that the sample pointer stays constant but it's contents may be changed from other threads (hence the need for the mutex).
        inline void Setup(GranulatorSample* _sample, int _channel, AudioPluginUtil::Random& random, const float sampletime, const int delaypos, const float* params, float startsample, int _wrapping)
        {
            sample = _sample;
            channel = _channel;
            wrapping = _wrapping;
            float maxtime = (float)(sample->numsamples - 1);
            length = (int)(maxtime * random.GetFloat(params[P_WLEN], params[P_WLEN] + params[P_RWLEN]));
            float invlength = 1.0f / (float)length;
            offset = delaypos + maxtime * AudioPluginUtil::FastClip(random.GetFloat(params[P_OFFSET] - params[P_ROFS], params[P_OFFSET]), 0.0f, 1.0f);
            speed = AudioPluginUtil::FastMax(0.001f, random.GetFloat(params[P_SPEED], params[P_SPEED] + params[P_RSPEED])) * invlength * sample->samplerate * sampletime;
            pos = -speed * startsample;
            pan = params[P_PANBASE] + random.GetFloat(-params[P_PANRANGE], params[P_PANRANGE]);
            shape = params[P_SHAPE];
        }

        inline float Scan()
        {
            const float* src = sample->data + channel;
            float p = AudioPluginUtil::FastMax(0.0f, pos);
            pos += speed;
            float amp = 1.0f - fabsf(p + p - 1.0f);
            amp = AudioPluginUtil::FastClip(amp * shape, 0.0f, 1.0f);
            p = offset + p * length;
            int i = AudioPluginUtil::FastFloor(p);
            p -= i;
            if (i >= sample->numsamples)
            {
                if (!wrapping)
                    return 0.0f;
                i -= sample->numsamples;
            }
            float s = src[sample->numchannels * i++];
            if (i >= sample->numsamples)
            {
                if (!wrapping)
                    return s * amp;
                i -= sample->numsamples;
            }
            return amp * (s + (src[sample->numchannels * i] - s) * p);
        }
    };

    struct EffectData
    {
        float p[P_NUM];
        AudioPluginUtil::Random random;
        int delaypos;
        float env[8];
        double integrator[8];
        float samplecounter;
        float nextrandtime;
        int activegrains;
        Grain grains[MAXGRAINS];
        GranulatorSample delay;
    };

    int InternalRegisterEffectDefinition(UnityAudioEffectDefinition& definition)
    {
        int numparams = P_NUM;
        definition.paramdefs = new UnityAudioParameterDefinition[numparams];
        AudioPluginUtil::RegisterParameter(definition, "Speed", "%", 0.001f, 25.0f, 0.01f, 100.0f, 2.5f, P_SPEED, "The speed in samples at which the grains will be replayed relative to the original");
        AudioPluginUtil::RegisterParameter(definition, "Window len", "%", 0.001f, 1.0f, 0.1f, 100.0f, 2.5f, P_WLEN, "Length of grain in seconds");
        AudioPluginUtil::RegisterParameter(definition, "Rnd speed", "%", 0.0f, 5.0f, 0.0f, 100.0f, 2.5f, P_RSPEED, "Randomized amount of speed in samples");
        AudioPluginUtil::RegisterParameter(definition, "Rnd offset", "%", 0.0f, 1.0f, 0.0f, 100.0f, 2.5f, P_ROFS, "Randomized offset in seconds");
        AudioPluginUtil::RegisterParameter(definition, "Rnd window len", "%", 0.0f, 1.0f, 0.0f, 100.0f, 2.5f, P_RWLEN, "Randomized amount of grain length in seconds");
        AudioPluginUtil::RegisterParameter(definition, "Freeze", "", 0.0f, 1.0f, 0.0f, 1.0f, 1.0f, P_FREEZE, "Freeze threshold (only for live input)");
        AudioPluginUtil::RegisterParameter(definition, "Offset", "%", 0.0f, 1.0f, 0.0f, 100.0f, 1.0f, P_OFFSET, "Offset in recorded or sampled waveform");
        AudioPluginUtil::RegisterParameter(definition, "Rate", "Hz", 0.0f, 1000.0f, 0.5f, 1.0f, 2.5f, P_RATE, "Grain emission rate");
        AudioPluginUtil::RegisterParameter(definition, "Random rate", "Hz", 0.0f, 1000.0f, 0.5f, 1.0f, 2.5f, P_RRATE, "Random grain emission rate");
        AudioPluginUtil::RegisterParameter(definition, "Pan base", "%", 0.0f, 1.0f, 0.5f, 100.0f, 1.0f, P_PANBASE, "Panning position base");
        AudioPluginUtil::RegisterParameter(definition, "Pan range", "%", 0.0f, 1.0f, 0.5f, 100.0f, 1.0f, P_PANRANGE, "Panning position range");
        AudioPluginUtil::RegisterParameter(definition, "Shape", "%", 1.0f, 10.0f, 1.0f, 100.0f, 1.0f, P_SHAPE, "Grain shape (1 = triangular)");
        AudioPluginUtil::RegisterParameter(definition, "Use Sample", "", -1.0f, MAXSAMPLE - 1, -1.0f, 1.0f, 1.0f, P_USESAMPLE, "-1 = use live input, otherwise indicates the slot of a sample uploaded by scripts via Granulator_UploadSample");
        return numparams;
    }

    void ResetGrains(EffectData* data)
    {
        for (int n = 0; n < MAXGRAINS; n++)
            data->grains[n].pos = 1.0f;
    }

    UNITY_AUDIODSP_RESULT UNITY_AUDIODSP_CALLBACK CreateCallback(UnityAudioEffectState* state)
    {
        EffectData* data = new EffectData;
        memset(data, 0, sizeof(EffectData));
        ResetGrains(data);
        state->effectdata = data;
        data->delay.numsamples = MAXDELAYLENGTH;
        data->delay.numchannels = 1;
        data->delay.samplerate = state->samplerate;
        data->delay.data = new float[data->delay.numsamples * 8]; // channel count is dynamic
        data->delay.preview = new float[data->delay.numsamples * 8];
        memset(data->delay.data, 0, sizeof(float) * data->delay.numsamples * data->delay.numchannels);
        memset(data->delay.preview, 0, sizeof(float) * data->delay.numsamples * data->delay.numchannels);
        AudioPluginUtil::InitParametersFromDefinitions(InternalRegisterEffectDefinition, data->p);
        return UNITY_AUDIODSP_OK;
    }

    UNITY_AUDIODSP_RESULT UNITY_AUDIODSP_CALLBACK ReleaseCallback(UnityAudioEffectState* state)
    {
        EffectData* data = state->GetEffectData<EffectData>();
        delete[] data->delay.data;
        delete[] data->delay.preview;
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
            int usesample = (int)data->p[P_USESAMPLE];
            AudioPluginUtil::MutexScopeLock mutexScope(Granulator::sampleMutex, usesample >= 0);
            GranulatorSample* gs = (usesample >= 0) ? &GetGranulatorSample(usesample) : &data->delay;
            if (gs->numsamples == 0 || gs->numchannels == 0)
            {
                memset(buffer, 0, sizeof(float) * numsamples);
                return UNITY_AUDIODSP_OK;
            }
            int channel = name[8] - '0';
            if (channel >= gs->numchannels)
                channel = gs->numchannels - 1;
            int delaypos = (usesample >= 0) ? 0 : data->delaypos;
            const float* src = gs->preview + channel;
            float scale = (float)(gs->numsamples - 2) / (float)numsamples;
            float invscale = 1.0f / scale, prev = 0.0f;
            for (int n = 0; n < numsamples; n++)
            {
                float f = n * scale;
                int i = AudioPluginUtil::FastFloor(f);
                f -= i;
                if (gs == &data->delay)
                    i += delaypos;
                float s = 0.0f;
                if (usesample < 0 && i >= gs->numsamples)
                    i -= gs->numsamples;
                if (i < gs->numsamples)
                {
                    s = src[gs->numchannels * i++];
                    if (usesample < 0 && i >= gs->numsamples)
                        i -= gs->numsamples;
                    if (i < gs->numsamples)
                        s += (src[gs->numchannels * i] - s) * f;
                }
                buffer[n] = (n == 0) ? 0.0f : (s - prev) * invscale;
                prev = s;
            }
        }
        return UNITY_AUDIODSP_OK;
    }

    UNITY_AUDIODSP_RESULT UNITY_AUDIODSP_CALLBACK ProcessCallback(UnityAudioEffectState* state, float* inbuffer, float* outbuffer, unsigned int length, int inchannels, int outchannels)
    {
        EffectData* data = state->GetEffectData<EffectData>();

        const int samplerate = state->samplerate;
        const float sampletime = 1.0f / (float)samplerate;
        const float freeze = data->p[P_FREEZE] * 2.0f;
        const int usesample = (int)data->p[P_USESAMPLE];
        const float* params = data->p;

        AudioPluginUtil::MutexScopeLock mutexScope(Granulator::sampleMutex, usesample >= 0);

        data->delay.numchannels = inchannels;

        GranulatorSample* gs = (usesample >= 0) ? &GetGranulatorSample(usesample) : &data->delay;

        // Fill in live data
        const float* src = inbuffer;
        for (unsigned int n = 0; n < length; n++)
        {
            bool record = false;
            for (int i = 0; i < inchannels; i++)
            {
                float input = src[i];
                float a = fabsf(input) + 1.0e-11f;
                data->env[i] = (a > data->env[i]) ? a : (data->env[i] * 0.9995f);
                record |= (data->env[i] > freeze);
            }
            if (record)
            {
                data->delaypos = (data->delaypos + MAXDELAYLENGTH - 1) & (MAXDELAYLENGTH - 1);
                for (int i = 0; i < inchannels; i++)
                {
                    data->delay.data[data->delaypos * inchannels + i] = src[i];

                    // Calculate integrated signal on the fly for better reconstruction in GetFloatBufferCallback.
                    // The small leak of 0.1% prevents build-up of DC.
                    data->integrator[i] = data->integrator[i] * 0.9999f + fabsf(src[i]);
                    data->delay.preview[data->delaypos * inchannels + i] = (float)(data->integrator[i]);
                }
            }
            src += inchannels;
        }

        memset(outbuffer, 0, length * outchannels * sizeof(float));

        if (state->flags & (UnityAudioEffectStateFlags_IsMuted | UnityAudioEffectStateFlags_IsPaused))
            return UNITY_AUDIODSP_OK;

        if ((state->flags & UnityAudioEffectStateFlags_IsPlaying) == 0)
        {
            ResetGrains(data);
            return UNITY_AUDIODSP_OK;
        }

        debug_graincount = data->activegrains;

        // Fill in new grains
        float rate = data->p[P_RATE] + data->p[P_RRATE] * data->nextrandtime;
        float nexteventsample = (rate > 0.0f) ? (samplerate / rate) : 100000000;
        for (unsigned int n = 0; n < length; n++)
        {
            if (++data->samplecounter >= nexteventsample)
            {
                data->samplecounter -= nexteventsample;
                float fracpos = 1.0f - data->samplecounter;
                data->nextrandtime = data->random.GetFloat(0.0f, 1.0f);
                rate = data->p[P_RATE] + data->p[P_RRATE] * data->nextrandtime;
                nexteventsample = (rate > 0.0f) ? (samplerate / rate) : 100000000;
                if (data->activegrains >= MAXGRAINS || gs->numsamples == 0)
                    continue;
                data->grains[data->activegrains++].Setup(
                    gs,
                    data->random.Get() % gs->numchannels,
                    data->random,
                    sampletime,
                    (usesample >= 0) ? 0 : data->delaypos,
                    params,
                    n + fracpos,
                    usesample < 0
                    );
            }
        }

        // Process grains
        Grain* g = data->grains;
        Grain* g_end = data->grains + data->activegrains;
        while (g < g_end)
        {
            float* dst = outbuffer;
            for (unsigned int n = 0; n < length; n++)
            {
                float s = g->Scan();
                dst[0] += s * (1.0f - g->pan);
                dst[1] += s * g->pan;
                dst += outchannels;
            }
            if (g->pos >= 0.99999f)
                *g = *(--g_end);
            else
                ++g;
        }
        data->activegrains = (int)(g_end - data->grains);

        return UNITY_AUDIODSP_OK;
    }
}

extern "C" UNITY_AUDIODSP_EXPORT_API bool Granulator_UploadSample(int index, float* data, int numsamples, int numchannels, int samplerate, const char* name)
{
    if (index < 0 || index >= Granulator::MAXSAMPLE)
        return false;

    AudioPluginUtil::MutexScopeLock mutexScope(Granulator::sampleMutex);

    Granulator::GranulatorSample& s = Granulator::GetGranulatorSample(index);
    if (s.allocated)
    {
        delete[] s.data;
        delete[] s.preview;
    }

    int num = numsamples * numchannels;
    if (num > 0)
    {
        s.data = new float[num];
        s.preview = new float[num];
        s.allocated = 1;
        strcpy_s(s.name, name);
        memcpy(s.data, data, num * sizeof(float));
        double integrator[8]; memset(integrator, 0, sizeof(integrator));
        float* src = s.data;
        float* dst = s.preview;
        for (int n = 0; n < numsamples; n++)
        {
            for (int i = 0; i < numchannels; i++)
            {
                // Calculate full integrated signal for better reconstruction in GetFloatBufferCallback.
                // The small leak of 0.1% prevents build-up of DC.
                integrator[i] = integrator[i] * 0.9999f + fabsf(*src++);
                *dst++ = (float)(integrator[i]);
            }
        }
    }
    else
    {
        s.data = NULL;
        s.preview = NULL;
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

    if (index < Granulator::MAXSAMPLE)
    {
        AudioPluginUtil::MutexScopeLock mutexScope(Granulator::sampleMutex);
        Granulator::GranulatorSample* s = &Granulator::GetGranulatorSample(index);
        if (s->numsamples == 0)
            return "Undefined";
        return s->name;
    }

    return "Undefined";
}

extern "C" UNITY_AUDIODSP_EXPORT_API int Granulator_DebugGetGrainCount()
{
    return Granulator::debug_graincount;
}
