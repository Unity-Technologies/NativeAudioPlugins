#include "AudioPluginUtil.h"

namespace Multiband
{
    enum Param
    {
        P_MasterGain,
        P_LowFreq, P_HighFreq,
        P_LowGain, P_MidGain, P_HighGain,
        P_LowAttack, P_MidAttack, P_HighAttack,
        P_LowRelease, P_MidRelease, P_HighRelease,
        P_LowThreshold, P_MidThreshold, P_HighThreshold,
        P_LowRatio, P_MidRatio, P_HighRatio,
        P_LowKnee, P_MidKnee, P_HighKnee,
        P_FilterOrder,
        P_UseLogScale,
        P_ShowSpectrum,
        P_SpectrumDecay,
        P_NUM
    };

    struct CompressorChannel
    {
        float env;
        float atk;
        float rel;
        float thr;
        float ratio;
        float knee;
        float reduction;
        float exp1;
        float exp2;

        float GetTimeConstant(float accuracy, float numSamples)
        {
            /*
            Derivation of time constant from transition length specified as numSamples and desired accuracy within which target is reached:
            y(n) = y(n-1) + [x(n) - y(n-1)] * alpha
            y(0) = 1, x(n) = 0   =>
            y(1) = 1 + [0 - 1] * alpha = 1-alpha
            y(2) = 1-alpha + [0 - (1-alpha)] * alpha = (1-alpha)*(1-alpha) = (1-alpha)^2
            y(3) = (1-alpha)^2 + [0 - (1-alpha)^2] * alpha = (1-alpha) * (1-alpha)^2 = (1-alpha)^3
            ...
            y(n) = (1-alpha)^n = 1-accuracy   =>
            1-alpha = (1-accuracy)^(1/n)
            alpha = 1 - (1-accuracy)^(1/n)
            */
            if (numSamples <= 0.0f)
                return 1.0f;
            return 1.0f - powf(1.0f - accuracy, 1.0f / numSamples);
        }

        void Setup(float _atk, float _rel, float _thr, float _ratio, float _knee)
        {
            thr = _thr;
            ratio = _ratio;
            knee = _knee;

            float g = 0.05f * ((1.0f / ratio) - 1.0f);
            exp1 = powf(10.0f, g * 0.25f / ((knee > 0.0f) ? knee : 1.0f));
            exp2 = powf(10.0f, g);
            atk = GetTimeConstant(0.99f, atk);
            rel = GetTimeConstant(0.99f, rel);
        }

        inline float Process(float input)
        {
            float g = 1.0f;
            float s = AudioPluginUtil::FastClip(input * input, 1.0e-11f, 100.0f);
            float timeConst = (s > env) ? atk : rel;
            env += (s - env) * timeConst + 1.0e-16f; // add small constant to always positive number to avoid denormal numbers
            float sideChainLevel = 10.0f * log10f(env); // multiply by 10 (not 20) because duckEnvelope is RMS
            float t = sideChainLevel - thr;
            if (fabsf(t) < knee)
            {
                t += knee;
                g = powf(exp1, t * t);
            }
            else if (t > 0.0f)
                g = powf(exp2, t);
            reduction = g;
            return input * g;
        }
    };

    const int MAXORDER = 4;

    struct EffectData
    {
        struct Data
        {
            float p[P_NUM];
            AudioPluginUtil::BiquadFilter bandsplit[8][MAXORDER][4];
            AudioPluginUtil::BiquadFilter previewBandsplit[4];
            CompressorChannel band[3][8];
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
        static const char* bandname[] = { "Low", "Mid", "High" };
        int numparams = P_NUM;
        definition.paramdefs = new UnityAudioParameterDefinition[numparams];
        AudioPluginUtil::RegisterParameter(definition, "MasterGain", "dB", -100.0f, 100.0f, 0.0f, 1.0f, 1.0f, P_MasterGain, "Overall gain");
        AudioPluginUtil::RegisterParameter(definition, "LowFreq", "Hz", 0.01f, 24000.0f, 800.0f, 1.0f, 3.0f, P_LowFreq, "Low/Mid cross-over frequency");
        AudioPluginUtil::RegisterParameter(definition, "HighFreq", "Hz", 0.01f, 24000.0f, 5000.0f, 1.0f, 3.0f, P_HighFreq, "Mid/High cross-over frequency");
        for (int i = 0; i < 3; i++)
            AudioPluginUtil::RegisterParameter(definition, AudioPluginUtil::tmpstr(0, "%sGain", bandname[i]), "dB", -100.0f, 100.0f, 0.0f, 1.0f, 1.0f, P_LowGain + i, AudioPluginUtil::tmpstr(1, "%s band gain in dB", bandname[i]));
        for (int i = 0; i < 3; i++)
            AudioPluginUtil::RegisterParameter(definition, AudioPluginUtil::tmpstr(0, "%sAttackTime", bandname[i]), "ms", 0.0f, 10.0f, 0.1f, 1000.0f, 4.0f, P_LowAttack + i, AudioPluginUtil::tmpstr(1, "%s band attack time in seconds", bandname[i]));
        for (int i = 0; i < 3; i++)
            AudioPluginUtil::RegisterParameter(definition, AudioPluginUtil::tmpstr(0, "%sReleaseTime", bandname[i]), "ms", 0.0f, 10.0f, 0.5f, 1000.0f, 4.0f, P_LowRelease + i, AudioPluginUtil::tmpstr(1, "%s band release time in seconds", bandname[i]));
        for (int i = 0; i < 3; i++)
            AudioPluginUtil::RegisterParameter(definition, AudioPluginUtil::tmpstr(0, "%sThreshold", bandname[i]), "dB", -50.0f, 0.0f, -10.0f, 1.0f, 1.0f, P_LowThreshold + i, AudioPluginUtil::tmpstr(1, "%s band compression level threshold time in dB", bandname[i]));
        for (int i = 0; i < 3; i++)
            AudioPluginUtil::RegisterParameter(definition, AudioPluginUtil::tmpstr(0, "%sRatio", bandname[i]), "%", 1.0f, 30.0f, 1.0f, 100.0f, 1.0f, P_LowRatio + i, AudioPluginUtil::tmpstr(1, "%s band compression ratio time in percent", bandname[i]));
        for (int i = 0; i < 3; i++)
            AudioPluginUtil::RegisterParameter(definition, AudioPluginUtil::tmpstr(0, "%sKnee", bandname[i]), "dB", 0.0f, 40.0f, 10.0f, 1.0f, 1.0f, P_LowKnee + i, AudioPluginUtil::tmpstr(1, "%s band compression curve knee range in dB", bandname[i]));
        AudioPluginUtil::RegisterParameter(definition, "FilterOrder", "", 1.0f, (float)MAXORDER, 1.0f, 1.0f, 1.0f, P_FilterOrder, "Filter order of cross-over filters");
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

    static void SetupFilterCoeffs(EffectData::Data* data, int samplerate, AudioPluginUtil::BiquadFilter* filter0, AudioPluginUtil::BiquadFilter* filter1, AudioPluginUtil::BiquadFilter* filter2, AudioPluginUtil::BiquadFilter* filter3)
    {
        const float qfactor = 0.707f;
        filter0->SetupLowpass(data->p[P_LowFreq], (float)samplerate, qfactor);
        filter1->SetupHighpass(data->p[P_LowFreq], (float)samplerate, qfactor);
        filter2->SetupLowpass(data->p[P_HighFreq], (float)samplerate, qfactor);
        filter3->SetupHighpass(data->p[P_HighFreq], (float)samplerate, qfactor);
    }

    int UNITY_AUDIODSP_CALLBACK GetFloatBufferCallback(UnityAudioEffectState* state, const char* name, float* buffer, int numsamples)
    {
        EffectData::Data* data = &state->GetEffectData<EffectData>()->data;
        if (strcmp(name, "InputSpec") == 0)
            data->analyzer.ReadBuffer(buffer, numsamples, true);
        else if (strcmp(name, "OutputSpec") == 0)
            data->analyzer.ReadBuffer(buffer, numsamples, false);
        else if (strcmp(name, "LiveData") == 0)
        {
            buffer[0] = data->band[0][0].reduction;
            buffer[1] = data->band[1][0].reduction;
            buffer[2] = data->band[2][0].reduction;
            buffer[3] = data->band[0][0].env;
            buffer[4] = data->band[1][0].env;
            buffer[5] = data->band[2][0].env;
        }
        else if (strcmp(name, "Coeffs") == 0)
        {
            SetupFilterCoeffs(data, state->samplerate, &data->previewBandsplit[0], &data->previewBandsplit[1], &data->previewBandsplit[2], &data->previewBandsplit[3]);
            data->previewBandsplit[0].StoreCoeffs(buffer);
            data->previewBandsplit[1].StoreCoeffs(buffer);
            data->previewBandsplit[2].StoreCoeffs(buffer);
            data->previewBandsplit[3].StoreCoeffs(buffer);
        }
        else
            memset(buffer, 0, sizeof(float) * numsamples);

        return UNITY_AUDIODSP_OK;
    }

    UNITY_AUDIODSP_RESULT UNITY_AUDIODSP_CALLBACK ProcessCallback(UnityAudioEffectState* state, float* inbuffer, float* outbuffer, unsigned int length, int inchannels, int outchannels)
    {
        EffectData::Data* data = &state->GetEffectData<EffectData>()->data;
        const float sr = (float)state->samplerate;
        float specDecay = powf(10.0f, 0.05f * data->p[P_SpectrumDecay] * length / sr);
        bool calcSpectrum = (data->p[P_ShowSpectrum] >= 0.5f);
        if (calcSpectrum)
            data->analyzer.AnalyzeInput(inbuffer, inchannels, length, specDecay);

        for (int i = 0; i < inchannels; i++)
        {
            data->band[0][i].Setup(data->p[P_LowAttack] * sr, data->p[P_LowRelease] * sr, data->p[P_LowThreshold], data->p[P_LowRatio], data->p[P_LowKnee]);
            data->band[1][i].Setup(data->p[P_MidAttack] * sr, data->p[P_MidRelease] * sr, data->p[P_MidThreshold], data->p[P_MidRatio], data->p[P_MidKnee]);
            data->band[2][i].Setup(data->p[P_HighAttack], data->p[P_HighRelease], data->p[P_HighThreshold], data->p[P_HighRatio], data->p[P_HighKnee]);
            for (int k = 0; k < MAXORDER; k++)
                SetupFilterCoeffs(data, state->samplerate, &data->bandsplit[i][k][0], &data->bandsplit[i][k][1], &data->bandsplit[i][k][2], &data->bandsplit[i][k][3]);
        }

        const float  lowGainLin = powf(10.0f, (data->p[P_LowGain] + data->p[P_MasterGain]) * 0.05f);
        const float  midGainLin = powf(10.0f, (data->p[P_MidGain] + data->p[P_MasterGain]) * 0.05f);
        const float highGainLin = powf(10.0f, (data->p[P_HighGain] + data->p[P_MasterGain]) * 0.05f);
        const int order = (int)data->p[P_FilterOrder];
        for (unsigned int n = 0; n < length; n++)
        {
            for (int i = 0; i < outchannels; i++)
            {
                float killdenormal = (float)(data->random.Get() & 255) * 1.0e-9f;
                float input = inbuffer[n * inchannels + i] + killdenormal;
                float lpf = input, bpf = input, hpf = input;
                for (int k = 0; k < order; k++)
                {
                    lpf = data->bandsplit[i][k][0].Process(lpf);
                    bpf = data->bandsplit[i][k][1].Process(bpf);
                }
                for (int k = 0; k < order; k++)
                {
                    bpf = data->bandsplit[i][k][2].Process(bpf);
                    hpf = data->bandsplit[i][k][3].Process(hpf);
                }
                outbuffer[n * outchannels + i] =
                    data->band[0]->Process(lpf) *  lowGainLin +
                    data->band[1]->Process(bpf) *  midGainLin +
                    data->band[2]->Process(hpf) * highGainLin;
            }
        }

        if (calcSpectrum)
            data->analyzer.AnalyzeOutput(outbuffer, outchannels, length, specDecay);

        return UNITY_AUDIODSP_OK;
    }
}
