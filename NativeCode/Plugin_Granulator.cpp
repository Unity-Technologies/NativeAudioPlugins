#include "AudioPluginUtil.h"

namespace Granulator
{
    const int MAXGRAINS = 500;
    const int MAXLENGTH = 0x100000;
    const int MAXSAMPLE = 16;

    Mutex sampleMutex;
	int debug_graincount = 0;

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
        float offset;
        int length;
        int numsamples;
        const float* src;
        float pos;
        float speed;
		float pan;
		float shape;
        inline void Setup(Random& random, const float wavesamplerate, const float samplerate, const float sampletime, const int writepos, const float* params, const float* _src, int _numsamples, float startsample)
        {
            src = _src;
            numsamples = _numsamples;
            float maxtime = (float)(numsamples - 1);
            length = (int)(maxtime * random.GetFloat(params[P_WLEN], params[P_WLEN] + params[P_RWLEN]));
            float invlength = 1.0f / (float)length;
            offset = writepos + maxtime * FastClip(random.GetFloat(params[P_OFFSET] - params[P_ROFS], params[P_OFFSET]), 0.0f, 1.0f);
            speed = FastMax(0.001f, random.GetFloat(params[P_SPEED], params[P_SPEED] + params[P_RSPEED])) * invlength * wavesamplerate * sampletime;
            pos = -speed * startsample;
			pan = params[P_PANBASE] + random.GetFloat(-params[P_PANRANGE], params[P_PANRANGE]);
			shape = params[P_SHAPE];
        }

        inline float Scan(int numsrcchannels)
        {
            float p = FastMax(0.0f, pos);
			pos += speed;
            float amp = 1.0f - fabsf(p + p - 1.0f);
			amp = FastClip(amp * shape, 0.0f, 1.0f);
            p = offset + p * length;
            int i = FastFloor(p);
            p -= i;
            if (i >= numsamples)
				return 0.0f;
            float s = src[numsrcchannels * i++];
            if (i >= numsamples)
				return s * amp;
            return amp * (s + (src[numsrcchannels * i] - s) * p);
        }
    };

    struct EffectData
    {
        float p[P_NUM];
        Random random;
		int writepos;
        float env;
        float previewsum;
		float samplecounter;
		float nextrandtime;
		int activegrains;
        Grain grains[MAXGRAINS];
        float delay[MAXLENGTH];
        float preview[MAXLENGTH];
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
        RegisterParameter(definition, "Freeze", "", 0.0f, 1.0f, 0.0f, 1.0f, 1.0f, P_FREEZE, "Freeze threshold (only for live input)");
        RegisterParameter(definition, "Offset", "%", 0.0f, 1.0f, 0.0f, 100.0f, 1.0f, P_OFFSET, "Offset in recorded or sampled waveform");
        RegisterParameter(definition, "Rate", "Hz", 0.0f, 1000.0f, 0.5f, 1.0f, 2.5f, P_RATE, "Grain emission rate");
		RegisterParameter(definition, "Random rate", "Hz", 0.0f, 1000.0f, 0.5f, 1.0f, 2.5f, P_RRATE, "Random grain emission rate");
		RegisterParameter(definition, "Pan base", "%", 0.0f, 1.0f, 0.5f, 100.0f, 1.0f, P_PANBASE, "Panning position base");
		RegisterParameter(definition, "Pan range", "%", 0.0f, 1.0f, 0.5f, 100.0f, 1.0f, P_PANRANGE, "Panning position range");
		RegisterParameter(definition, "Shape", "%", 1.0f, 10.0f, 1.0f, 100.0f, 1.0f, P_SHAPE, "Grain shape (1 = triangular)");
        RegisterParameter(definition, "Use Sample", "", -1.0f, MAXSAMPLE - 1, -1.0f, 1.0f, 1.0f, P_USESAMPLE, "-1 = use live input, otherwise indicates the slot of a sample uploaded by scripts via Granulator_UploadSample");
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
            int wp = (gs == NULL) ? data->writepos : 0;
            int numchannels = (gs != NULL) ? gs->numchannels : 1;
            int numsrcsamples = (gs != NULL) ? gs->numsamples : MAXLENGTH;
            const float* src = (gs != NULL) ? (gs->data + channel) : data->preview;
            float scale = (float)(numsrcsamples - 2) / (float)numsamples;
            float invscale = 1.0f / scale, prev = 0.0f;
            for (int n = 0; n < numsamples; n++)
            {
                float f = n * scale;
                int i = FastFloor(f);
                f -= i;
                if (gs == NULL)
                    i += wp;
				float s = 0.0f;
                if (i < numsrcsamples)
				{
                    s = src[numchannels * i++];
	                if (i < numsrcsamples)
		                s += (src[numchannels * i] - s) * f;
				}
                buffer[n] = (s - prev) * invscale;
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
        const float freeze = data->p[P_FREEZE];
        const int useSample = (int)data->p[P_USESAMPLE];
        const float* params = data->p;

        MutexScopeLock mutexScope(Granulator::sampleMutex, useSample >= 0);

        GranulatorSample* gs = (useSample >= 0) ? &GetGranulatorSample(useSample) : NULL;
        if (gs != NULL && gs->numsamples == 0)
            gs = NULL;

        for (int c = 0; c < inchannels; c++)
        {
            const float* src = inbuffer + c;

            for (int n = 0; n < length; n++)
            {
                float input = *src;
                src += inchannels;

                data->env += (fabsf(input) - data->env) * 0.01f;
                if (data->env > freeze)
                {
                    data->previewsum = data->previewsum * 0.999f + input;
                    data->delay[data->writepos] = input;
                    data->preview[data->writepos] = data->previewsum;
                    data->writepos = (data->writepos + MAXLENGTH - 1) & (MAXLENGTH - 1);
                }
            }
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
			
		int numsrcchannels = (gs == NULL) ? 1 : gs->numchannels;
		
		// Fill in new grains
		float rate = data->p[P_RATE] + data->p[P_RRATE] * data->nextrandtime;
		float nexteventsample = (rate > 0.0f) ? (samplerate / rate) : 100000000;
		for (int n = 0; n < length; n++)
		{
			if (++data->samplecounter >= nexteventsample)
			{
				data->samplecounter -= nexteventsample;
				float fracpos = 1.0f - data->samplecounter;
				data->nextrandtime = data->random.GetFloat(0.0f, 1.0f);
				rate = data->p[P_RATE] + data->p[P_RRATE] * data->nextrandtime;
				nexteventsample = (rate > 0.0f) ? (samplerate / rate) : 100000000;
				if (data->activegrains >= MAXGRAINS)
					continue;
				Grain* g = &data->grains[data->activegrains++];
				if (gs == NULL)
					g->Setup(data->random, samplerate, samplerate, sampletime, data->writepos, params, data->delay, MAXLENGTH, n + fracpos);
				else
					g->Setup(data->random, gs->samplerate, samplerate, sampletime, 0, params, gs->data + (data->random.Get() % numsrcchannels), gs->numsamples, n + fracpos);
			}
		}
		
		// Process grains
		Grain* g = data->grains;
		Grain* g_end = data->grains + data->activegrains;
		while (g < g_end)
		{
			float* dst = outbuffer;
			for (int n = 0; n < length; n++)
			{
				float s = g->Scan(numsrcchannels);
				dst[0] += s * (1.0f - g->pan);
				dst[1] += s * g->pan;
				dst += outchannels;
			}
			if (g->pos >= 0.99999f)
				*g = *(--g_end);
			else
				++g;
		}
		data->activegrains = g_end - data->grains;

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

extern "C" UNITY_AUDIODSP_EXPORT_API int Granulator_DebugGetGrainCount()
{
	return Granulator::debug_graincount;
}
