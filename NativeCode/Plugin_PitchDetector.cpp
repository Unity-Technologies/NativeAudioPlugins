#include "AudioPluginUtil.h"

namespace PitchDetector
{
    const int MAXINDEX = 16;
    const int FFTSIZE = 8192;
    const int WINSIZE = 4096;
    const int HOPSIZE = 1024;

    static float detected_freqs[MAXINDEX];
    static float debugdata[FFTSIZE];

    enum Param
    {
        P_INDEX,
        P_LOCUT,
        P_HICUT,
        P_LOBIN,
        P_HIBIN,
        P_THR,
        P_OSCPITCH,
        P_MONITOR,
        P_NUM
    };

    struct EffectData
    {
        struct Data
        {
            float p[P_NUM];
            int counter;
            float env;
            float phase;
            float buffer[WINSIZE];
            float window[WINSIZE];
            float acnf[FFTSIZE];
            AudioPluginUtil::UnityComplexNumber spec[FFTSIZE];
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
        AudioPluginUtil::RegisterParameter(definition, "Index", "", 0.0f, MAXINDEX - 1, 0.0f, 1.0f, 1.0f, P_INDEX, "Determines where the pitch data is written to for access by scripts via PitchDetectorGetFreq");
        AudioPluginUtil::RegisterParameter(definition, "Low Cut", "", 0.0f, (FFTSIZE / 2) - 1, 20, 1.0f, 1.0f, P_LOCUT, "Low frequency cut-off for input preprocessing");
        AudioPluginUtil::RegisterParameter(definition, "High Cut", "", 0.0f, (FFTSIZE / 2) - 1, 1000, 1.0f, 1.0f, P_HICUT, "High frequency cut-off for input preprocessing");
        AudioPluginUtil::RegisterParameter(definition, "Low Bin", "", 0.0f, (FFTSIZE / 2) - 1, 50, 1.0f, 1.0f, P_LOBIN, "Low detection bin in autocorrelation");
        AudioPluginUtil::RegisterParameter(definition, "High Bin", "", 0.0f, (FFTSIZE / 2) - 1, 1500, 1.0f, 1.0f, P_HIBIN, "High detection bin in autocorrelation");
        AudioPluginUtil::RegisterParameter(definition, "Threshold", "%", 0.0f, 1.0f, 0.05f, 1.0f, 1.0f, P_THR, "Input signal envelope threshold above which pitch detection is attempted");
        AudioPluginUtil::RegisterParameter(definition, "Osc Pitch", "", -48.0f, 48.0f, 0.0f, 1.0f, 1.0f, P_OSCPITCH, "Relative oscillator pitch in semitones");
        AudioPluginUtil::RegisterParameter(definition, "Monitor", "%", 0.0f, 1.0f, 0.5f, 100.0f, 1.0f, P_MONITOR, "Monitor mix for auditioning the pitch tracking");
        return numparams;
    }

    UNITY_AUDIODSP_RESULT UNITY_AUDIODSP_CALLBACK CreateCallback(UnityAudioEffectState* state)
    {
        EffectData* effectdata = new EffectData;
        memset(effectdata, 0, sizeof(EffectData));

        // Calculate window and autocorrelation normalization factor
        EffectData::Data* data = &effectdata->data;
        for (int i = 0; i < WINSIZE; i++)
        {
            float w = 0.5f - 0.5f * cosf(i * AudioPluginUtil::kPI / (float)WINSIZE);
            data->window[i] = w;
            data->spec[i].re = w;
        }

        // Window correction
        AudioPluginUtil::FFT::Forward(data->spec, FFTSIZE, true);
        for (int i = 0; i < FFTSIZE; i++)
            data->acnf[i] = 1.0f / data->spec[i].Magnitude2();

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
        const float sampletime = 1.0f / state->samplerate;
        const float monitor = data->p[P_MONITOR];
        const float oscpitch = powf(2.0f, data->p[P_OSCPITCH] / 12.0f) * sampletime;

        memcpy(outbuffer, inbuffer, length * inchannels * sizeof(float));

        for (unsigned int n = 0; n < length; n++)
        {
            float input = inbuffer[n * inchannels];

            data->env += (fabsf(input) - data->env) * 0.01f;
            data->buffer[data->counter + WINSIZE - HOPSIZE] = input;

            if (++data->counter == HOPSIZE)
            {
                data->counter = 0;

                for (int i = 0; i < WINSIZE - HOPSIZE; i++)
                    data->buffer[i] = data->buffer[i + HOPSIZE];

                for (int i = 0; i < WINSIZE; i++)
                {
                    data->spec[i].re = data->buffer[i] * data->window[i];
                    data->spec[i].im = 0.0f;
                }

                for (int i = WINSIZE; i < FFTSIZE; i++)
                {
                    data->spec[i].re = 0.0f;
                    data->spec[i].im = 0.0f;
                }

                AudioPluginUtil::FFT::Forward(data->spec, FFTSIZE, true);

                int locut = (int)data->p[P_LOCUT];
                int hicut = (int)data->p[P_HICUT];
                memset(data->spec, 0, sizeof(AudioPluginUtil::UnityComplexNumber) * locut);
                memset(data->spec + hicut, 0, sizeof(AudioPluginUtil::UnityComplexNumber) * (FFTSIZE - 2 * hicut));
                memset(data->spec + FFTSIZE - locut, 0, sizeof(AudioPluginUtil::UnityComplexNumber) * locut);

                // Fast autocorrelation
                for (int i = 0; i < FFTSIZE; i++)
                {
                    data->spec[i].re = data->spec[i].Magnitude2() * data->acnf[n]; // Correct for windowing
                    data->spec[i].im = 0.0f;
                    debugdata[i] = data->spec[i].re;
                }
                AudioPluginUtil::FFT::Backward(data->spec, FFTSIZE, true);

                int startbin = (int)data->p[P_LOBIN];
                int endbin = (int)data->p[P_HIBIN];
                int maxbin = 0;
                float maxval = 0.0f;
                for (int n = startbin; n < endbin; n++)
                {
                    float a = data->spec[n].re;
                    if (a > maxval)
                    {
                        maxbin = n;
                        maxval = a;
                    }
                }

                if (data->env > data->p[P_THR] && maxbin > 0)
                    detected_freqs[(int)data->p[P_INDEX]] = (float)state->samplerate / (float)maxbin;
            }

            data->phase += detected_freqs[(int)data->p[P_INDEX]] * oscpitch;
            data->phase -= AudioPluginUtil::FastFloor(data->phase);

            for (int c = 0; c < outchannels; c++)
                outbuffer[n * outchannels + c] += ((data->phase * 2.0f - 1.0f) * data->env - outbuffer[n * outchannels + c]) * monitor;
        }

        return UNITY_AUDIODSP_OK;
    }
}

extern "C" UNITY_AUDIODSP_EXPORT_API float PitchDetectorGetFreq(int index)
{
    if (index < 0 || index >= PitchDetector::MAXINDEX)
        return 0.0f;
    return PitchDetector::detected_freqs[index];
}

extern "C" UNITY_AUDIODSP_EXPORT_API int PitchDetectorDebug(float* data)
{
    for (int i = 0; i < PitchDetector::FFTSIZE; i++)
        data[i] = PitchDetector::debugdata[i];
    return PitchDetector::FFTSIZE;
}
