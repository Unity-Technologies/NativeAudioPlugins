#include "AudioPluginUtil.h"

namespace Oscilloscope
{
    enum Param
    {
        P_Window,
        P_Scale,
        P_Mode,
        P_SpectrumDecay,
        P_NUM
    };

    const int FFTSIZE = 8192;

    struct EffectData
    {
        float p[P_NUM];
        AudioPluginUtil::HistoryBuffer history[8];
        AudioPluginUtil::HistoryBuffer spectrum[8];
        int numchannels;
        AudioPluginUtil::UnityComplexNumber fftbuf[FFTSIZE];
        float smoothspec[8][FFTSIZE];
    };

    int InternalRegisterEffectDefinition(UnityAudioEffectDefinition& definition)
    {
        int numparams = P_NUM;
        definition.paramdefs = new UnityAudioParameterDefinition[numparams];
        AudioPluginUtil::RegisterParameter(definition, "Window", "s", 0.01f, 2.0f, 0.1f, 1.0f, 3.0f, P_Window, "Length of analysis window");
        AudioPluginUtil::RegisterParameter(definition, "Scale", "%", 0.01f, 10.0f, 1.0f, 100.0f, 3.0f, P_Scale, "Amplitude scaling for monitored signal");
        AudioPluginUtil::RegisterParameter(definition, "Mode", "", 0.0f, 3.0f, 0.0f, 1.0f, 1.0f, P_Mode, "Display mode (0=scope, 1=spectrum, 2=spectrogram)");
        AudioPluginUtil::RegisterParameter(definition, "SpectrumDecay", "dB/s", -100.0f, 0.0f, -10.0f, 1.0f, 1.0f, P_SpectrumDecay, "Hold time for overlaid spectra");
        return numparams;
    }

    UNITY_AUDIODSP_RESULT UNITY_AUDIODSP_CALLBACK CreateCallback(UnityAudioEffectState* state)
    {
        EffectData* data = new EffectData;
        memset(data, 0, sizeof(EffectData));
        AudioPluginUtil::InitParametersFromDefinitions(InternalRegisterEffectDefinition, data->p);
        state->effectdata = data;
        for (int i = 0; i < 8; i++)
        {
            data->history[i].Init(state->samplerate * 2);
            data->spectrum[i].Init(state->samplerate * 2);
        }
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

        for (int i = 0; i < inchannels; i++)
            data->history[i].Feed(inbuffer + i, length, inchannels);

        if (data->p[P_Mode] >= 1.0f)
        {
            for (int i = 0; i < inchannels; i++)
            {
                AudioPluginUtil::HistoryBuffer& history = data->history[i];
                AudioPluginUtil::HistoryBuffer& spectrum = data->spectrum[i];
                int windowsize = FFTSIZE / 2;
                int w = history.writeindex;
                float c = 1.0f, s = 0.0f, f = 2.0f * sinf(AudioPluginUtil::kPI / (float)windowsize);
                memset(data->fftbuf, 0, sizeof(AudioPluginUtil::UnityComplexNumber) * FFTSIZE);
                for (int n = 0; n < windowsize; n++)
                {
                    data->fftbuf[n].re = history.data[w] * (0.5f - 0.5f * c);
                    s += c * f;
                    c -= s * f;
                    if (--w < 0)
                        w = history.length - 1;
                }
                AudioPluginUtil::FFT::Forward(data->fftbuf, FFTSIZE, true);
                float specdecay = powf(10.0f, 0.05f * data->p[P_SpectrumDecay] * length / (float)state->samplerate);
                for (int n = 0; n < FFTSIZE / 2; n++)
                {
                    float a = data->fftbuf[n].Magnitude();
                    data->smoothspec[i][n] = (a > data->smoothspec[i][n]) ? a : data->smoothspec[i][n] * specdecay;
                }
                spectrum.Feed(data->smoothspec[i], FFTSIZE / 2, 1);
            }
        }

        data->numchannels = inchannels;

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
        int channel = name[7] - '0';
        if (data->p[P_Mode] >= 1.0f)
            data->spectrum[channel].ReadBuffer(buffer, numsamples, FFTSIZE / 2, 0.0f);
        else
            data->history[channel].ReadBuffer(buffer, numsamples, numsamples, 0.0f);
        return UNITY_AUDIODSP_OK;
    }
}
