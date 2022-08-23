#include "AudioPluginUtil.h"

namespace LoudnessMeter
{
    enum Param
    {
        P_Window,
        P_YOffset,
        P_YScale,
        P_NUM
    };

    class LoudnessAnalyzer
    {
    public:
        float peak[8];
        float rms[8];
        float attack;
        float release;
        float updateperiod;
        float updatecount;
        AudioPluginUtil::HistoryBuffer peakbuf;
        AudioPluginUtil::HistoryBuffer rmsbuf;
    public:
        void Init(float lengthInSeconds, float updateRateInHz, float attackTime, float releaseTime, float samplerate)
        {
            attack = 1.0f - powf(0.01f, 1.0f / (samplerate * attackTime));
            release = 1.0f - powf(0.01f, 1.0f / (samplerate * releaseTime));
            updateperiod = samplerate / updateRateInHz;
            int length = (int)ceilf(lengthInSeconds * updateRateInHz);
            peakbuf.Init(length);
            rmsbuf.Init(length);
        }

        inline void Feed(const float* inputs, int numchannels)
        {
            float maxPeak = 0.0f, maxRMS = 0.0f;
            for (int i = 0; i < numchannels; i++)
            {
                float x = inputs[i];
                x = fabsf(x);
                peak[i] += (x - peak[i]) * ((x > peak[i]) ? attack : release);
                x *= x;
                rms[i] += (x - rms[i]) * ((x > rms[i]) ? attack : release);
                if (peak[i] > maxPeak)
                    maxPeak = peak[i];
                if (rms[i] > maxRMS)
                    maxRMS = rms[i];
            }
            if (--updatecount <= 0.0f)
            {
                updatecount += updateperiod;
                peakbuf.Feed(maxPeak);
                rmsbuf.Feed(sqrtf(maxRMS));
            }
        }

        void ReadBuffer(float* buffer, int numsamplesTarget, float windowLength, float samplerate, bool rms)
        {
            int numsamplesSource = (int)ceilf(samplerate * windowLength / updateperiod);
            AudioPluginUtil::HistoryBuffer& buf = (rms) ? rmsbuf : peakbuf;
            buf.ReadBuffer(buffer, numsamplesTarget, numsamplesSource, (float)updatecount / (float)updateperiod);
        }
    };

    struct EffectData
    {
        float p[P_NUM];
        LoudnessAnalyzer momentary;
        LoudnessAnalyzer shortterm;
        LoudnessAnalyzer integrated;
    };

    const float kMaxWindowLength = 30.0f * 60.0f;

    int InternalRegisterEffectDefinition(UnityAudioEffectDefinition& definition)
    {
        int numparams = P_NUM;
        definition.paramdefs = new UnityAudioParameterDefinition[numparams];
        AudioPluginUtil::RegisterParameter(definition, "Window", "s", 0.1f, kMaxWindowLength, 1.0f, 1.0f, 1.0f, P_Window, "Length of analysis window");
        AudioPluginUtil::RegisterParameter(definition, "YOffset", "dB", -200.0f, 200.0f, 0.0f, 1.0f, 1.0f, P_YOffset, "Zoom offset on y-axis around which the loudness graphs will be plotted");
        AudioPluginUtil::RegisterParameter(definition, "YScale", "%", 0.001f, 10.0f, 1.0f, 100.0f, 1.0f, P_YScale, "Zoom factor for loudness graph");
        return numparams;
    }

    UNITY_AUDIODSP_RESULT UNITY_AUDIODSP_CALLBACK CreateCallback(UnityAudioEffectState* state)
    {
        EffectData* data = new EffectData;
        memset(data, 0, sizeof(EffectData));
        AudioPluginUtil::InitParametersFromDefinitions(InternalRegisterEffectDefinition, data->p);
        state->effectdata = data;
        data->momentary.Init(3.0f, (float)state->samplerate, 0.4f, 0.4f, (float)state->samplerate);
        data->shortterm.Init(kMaxWindowLength, 4.0f, 3.0f, 3.0f, (float)state->samplerate);
        data->integrated.Init(kMaxWindowLength, 1.0f, 3.0f, 3.0f, (float)state->samplerate);
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

        const float* src = inbuffer;
        for (unsigned int n = 0; n < length; n++)
        {
            data->momentary.Feed(src, inchannels);
            data->shortterm.Feed(src, inchannels);
            data->integrated.Feed(src, inchannels);
            src += inchannels;
        }

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
        if (strcmp(name, "Momentary") == 0)
            data->momentary.ReadBuffer(buffer, numsamples, data->p[P_Window], (float)state->samplerate, false);
        if (strcmp(name, "MomentaryRMS") == 0)
            data->momentary.ReadBuffer(buffer, numsamples, data->p[P_Window], (float)state->samplerate, true);
        else if (strcmp(name, "ShortTerm") == 0)
            data->shortterm.ReadBuffer(buffer, numsamples, data->p[P_Window], (float)state->samplerate, false);
        else if (strcmp(name, "ShortTermRMS") == 0)
            data->shortterm.ReadBuffer(buffer, numsamples, data->p[P_Window], (float)state->samplerate, true);
        else if (strcmp(name, "Integrated") == 0)
            data->integrated.ReadBuffer(buffer, numsamples, data->p[P_Window], (float)state->samplerate, false);
        else if (strcmp(name, "IntegratedRMS") == 0)
            data->integrated.ReadBuffer(buffer, numsamples, data->p[P_Window], (float)state->samplerate, true);
        else
            memset(buffer, 0, sizeof(float) * numsamples);
        return UNITY_AUDIODSP_OK;
    }
}
