#include "AudioPluginUtil.h"

namespace Equalizer
{
    enum Param
    {
        P_MasterGain,
        P_LowGain,
        P_MidGain,
        P_HighGain,
        P_LowFreq,
        P_MidFreq,
        P_HighFreq,
        P_LowQ,
        P_MidQ,
        P_HighQ,
        P_UseLogScale,
        P_ShowSpectrum,
        P_SpectrumDecay,
        P_NUM
    };

    struct EffectData
    {
        struct Data
        {
            float p[P_NUM];
            AudioPluginUtil::BiquadFilter FilterH[8];
            AudioPluginUtil::BiquadFilter FilterP[8];
            AudioPluginUtil::BiquadFilter FilterL[8];
            AudioPluginUtil::BiquadFilter DisplayFilterCoeffs[3];
            float sr;
            AudioPluginUtil::Random random;
            AudioPluginUtil::FFTAnalyzer analyzer;
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
        AudioPluginUtil::RegisterParameter(definition, "MasterGain", "dB", -100.0f, 100.0f, 0.0f, 1.0f, 1.0f, P_MasterGain, "Overall gain applied");
        AudioPluginUtil::RegisterParameter(definition, "LowGain", "dB", -100.0f, 100.0f, 0.0f, 1.0f, 1.0f, P_LowGain, "Gain applied to lower frequency band");
        AudioPluginUtil::RegisterParameter(definition, "MidGain", "dB", -100.0f, 100.0f, 0.0f, 1.0f, 1.0f, P_MidGain, "Gain applied to middle frequency band");
        AudioPluginUtil::RegisterParameter(definition, "HighGain", "dB", -100.0f, 100.0f, 0.0f, 1.0f, 1.0f, P_HighGain, "Gain applied to high frequency band");
        AudioPluginUtil::RegisterParameter(definition, "LowFreq", "Hz", 0.01f, 24000.0f, 800.0f, 1.0f, 3.0f, P_LowFreq, "Cutoff frequency of lower frequency band");
        AudioPluginUtil::RegisterParameter(definition, "MidFreq", "Hz", 0.01f, 24000.0f, 4000.0f, 1.0f, 3.0f, P_MidFreq, "Center frequency of middle frequency band");
        AudioPluginUtil::RegisterParameter(definition, "HighFreq", "Hz", 0.01f, 24000.0f, 8000.0f, 1.0f, 3.0f, P_HighFreq, "Cutoff frequency of high frequency band");
        AudioPluginUtil::RegisterParameter(definition, "LowQ", "", 0.01f, 10.0f, 0.707f, 1.0f, 3.0f, P_LowQ, "Q-factor of lower frequency band (inversely proportional to resonance)");
        AudioPluginUtil::RegisterParameter(definition, "MidQ", "", 0.01f, 10.0f, 0.707f, 1.0f, 3.0f, P_MidQ, "Q-factor of middle frequency band (inversely proportional to resonance)");
        AudioPluginUtil::RegisterParameter(definition, "HighQ", "", 0.01f, 10.0f, 0.707f, 1.0f, 3.0f, P_HighQ, "Q-factor of high frequency band (inversely proportional to resonance)");
        AudioPluginUtil::RegisterParameter(definition, "UseLogScale", "", 0.0f, 1.0f, 1.0f, 1.0f, 1.0f, P_UseLogScale, "Use logarithmic scale for plotting the filter curve frequency response and input/output spectra");
        AudioPluginUtil::RegisterParameter(definition, "ShowSpectrum", "", 0.0f, 1.0f, 0.0f, 1.0f, 1.0f, P_ShowSpectrum, "Overlay input spectrum (green) and output spectrum (red)");
        AudioPluginUtil::RegisterParameter(definition, "SpectrumDecay", "dB/s", -50.0f, 0.0f, -10.0f, 1.0f, 1.0f, P_SpectrumDecay, "Hold time for overlaid spectra");
        return numparams;
    }

    UNITY_AUDIODSP_RESULT UNITY_AUDIODSP_CALLBACK CreateCallback(UnityAudioEffectState* state)
    {
        EffectData* effectdata = new EffectData;
        memset(effectdata, 0, sizeof(EffectData));
        effectdata->data.analyzer.spectrumSize = 4096;
        AudioPluginUtil::InitParametersFromDefinitions(InternalRegisterEffectDefinition, effectdata->data.p);
        state->effectdata = effectdata;
        return UNITY_AUDIODSP_OK;
    }

    UNITY_AUDIODSP_RESULT UNITY_AUDIODSP_CALLBACK ReleaseCallback(UnityAudioEffectState* state)
    {
        EffectData* effectdata = state->GetEffectData<EffectData>();
        EffectData::Data* data = &effectdata->data;
        data->analyzer.Cleanup();
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
        if (value != NULL)
            *value = data->p[index];
        if (valuestr != NULL)
            valuestr[0] = 0;
        return UNITY_AUDIODSP_OK;
    }

    static void SetupFilterCoeffs(EffectData::Data* data, AudioPluginUtil::BiquadFilter* filterH, AudioPluginUtil::BiquadFilter* filterP, AudioPluginUtil::BiquadFilter* filterL, float samplerate)
    {
        filterH->SetupHighShelf(data->p[P_HighFreq], samplerate, data->p[P_HighGain], data->p[P_HighQ]);
        filterP->SetupPeaking(data->p[P_MidFreq], samplerate, data->p[P_MidGain], data->p[P_MidQ]);
        filterL->SetupLowShelf(data->p[P_LowFreq], samplerate, data->p[P_LowGain], data->p[P_LowQ]);
    }

    int UNITY_AUDIODSP_CALLBACK GetFloatBufferCallback(UnityAudioEffectState* state, const char* name, float* buffer, int numsamples)
    {
        EffectData::Data* data = &state->GetEffectData<EffectData>()->data;
        if (strcmp(name, "InputSpec") == 0)
            data->analyzer.ReadBuffer(buffer, numsamples, true);
        else if (strcmp(name, "OutputSpec") == 0)
            data->analyzer.ReadBuffer(buffer, numsamples, false);
        else if (strcmp(name, "Coeffs") == 0)
        {
            SetupFilterCoeffs(data, &data->DisplayFilterCoeffs[0], &data->DisplayFilterCoeffs[1], &data->DisplayFilterCoeffs[2], (float)state->samplerate);
            data->DisplayFilterCoeffs[2].StoreCoeffs(buffer);
            data->DisplayFilterCoeffs[1].StoreCoeffs(buffer);
            data->DisplayFilterCoeffs[0].StoreCoeffs(buffer);
        }
        else
            memset(buffer, 0, sizeof(float) * numsamples);

        return UNITY_AUDIODSP_OK;
    }

    UNITY_AUDIODSP_RESULT UNITY_AUDIODSP_CALLBACK ProcessCallback(UnityAudioEffectState* state, float* inbuffer, float* outbuffer, unsigned int length, int inchannels, int outchannels)
    {
        EffectData::Data* data = &state->GetEffectData<EffectData>()->data;

        float sr = (float)state->samplerate;
        for (int i = 0; i < inchannels; i++)
            SetupFilterCoeffs(data, &data->FilterH[i], &data->FilterP[i], &data->FilterL[i], sr);

        float specDecay = powf(10.0f, 0.05f * data->p[P_SpectrumDecay] * length / sr);
        bool calcSpectrum = (data->p[P_ShowSpectrum] >= 0.5f);
        if (calcSpectrum)
            data->analyzer.AnalyzeInput(inbuffer, inchannels, length, specDecay);

        const float masterGain = powf(10.0f, data->p[P_MasterGain] * 0.05f);
        for (unsigned int n = 0; n < length; n++)
        {
            for (int i = 0; i < outchannels; i++)
            {
                float killdenormal = (float)(data->random.Get() & 255) * 1.0e-9f;
                float y = inbuffer[n * inchannels + i] + killdenormal;
                y = data->FilterH[i].Process(y);
                y = data->FilterP[i].Process(y);
                y = data->FilterL[i].Process(y);
                outbuffer[n * outchannels + i] = y * masterGain;
            }
        }

        if (calcSpectrum)
            data->analyzer.AnalyzeOutput(outbuffer, outchannels, length, specDecay);

        return UNITY_AUDIODSP_OK;
    }
}
