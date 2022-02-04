// Implements a parallel filter bank of resonators that amplify specific narrow frequency bands.
// This is useful for synthesizing metallic sounds that have very dominant peaks in the spectrum.
// By sending source signals consisting of noise filtered with a wide bandpass a lot of different types
// of impact and friction noises can be simulated and the filter bank will respond in a natural way
// like a metallic object being excited by same type of force.

#include "AudioPluginUtil.h"

namespace ModalFilter
{
    const int MAXRESONATORS = 256;

    enum Param
    {
        P_SEED,
        P_NUMMODES,
        P_FREQSHIFT,
        P_FREQSHIFTVAR,
        P_FREQSCALE,
        P_FREQSCALEVAR,
        P_BWSCALE,
        P_BWSCALEVAR,
        P_GAINSCALE,
        P_GAINSCALEVAR,
        P_SHOWSPECTRUM,
        P_SPECTRUMDECAY,
        P_SPECTRUMOFFSET,
        P_NUM
    };

    class Resonator
    {
    public:
        inline void Setup(float fFreq, float fBandwidth, float fGain)
        {
            float fCutoff = AudioPluginUtil::FastClip(fFreq, 0.0001f, 0.9999f);
            float fRadius = AudioPluginUtil::FastClip(1.0f - fBandwidth, 0.0001f, 0.9999f);
            a0 = fGain * 0.5f * (1.0f - fRadius * fRadius);
            a1 = -2.0f * fRadius * cosf(fCutoff * AudioPluginUtil::kPI);
            a2 = fRadius * fRadius;
        }

        inline float Process(const float input)
        {
            float fIIR = input * a0 - d1 * a1 - d2 * a2;
            fIIR += 1.0e-7f;
            fIIR -= 1.0e-7f;
            float output = fIIR - d2;
            d2 = d1;
            d1 = fIIR;
            return output;
        }

    protected:
        float d1, d2;

    public:
        float a0, a1, a2;
    };

    struct EffectData
    {
        struct Data
        {
            float p[P_NUM];
            float prevp[P_NUM];
            AudioPluginUtil::Random random;
            Resonator resonators[8][MAXRESONATORS];
            AudioPluginUtil::FFTAnalyzer analyzer;
            float* display1;
            float* display2;
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
        AudioPluginUtil::RegisterParameter(definition, "Random seed", "", 0.0f, 100000.0f, 0.0f, 1.0f, 1.0f, P_SEED, "Random seed, selects locations of modes and their bandwidth randomly");
        AudioPluginUtil::RegisterParameter(definition, "Num modes", "", 1.0f, (float)MAXRESONATORS, 10.0f, 1.0f, 1.0f, P_NUMMODES, "Number of modes or partials");
        AudioPluginUtil::RegisterParameter(definition, "Freq shift", "Hz", -3000.0f, 3000.0f, 0.0f, 1.0f, 1.0f, P_FREQSHIFT, "Frequency shift in Hz");
        AudioPluginUtil::RegisterParameter(definition, "Freq shift var", "Hz", -3000.0f, 3000.0f, 0.01f, 1.0f, 1.0f, P_FREQSHIFTVAR, "Randomized frequency shift in Hz");
        AudioPluginUtil::RegisterParameter(definition, "Freq scale", "", -10.0f, 10.0f, 1.0f, 1.0f, 1.0f, P_FREQSCALE, "Frequency scaling in Hz");
        AudioPluginUtil::RegisterParameter(definition, "Freq scale var", "", -10.0f, 10.0f, 0.0f, 1.0, 1.0f, P_FREQSCALEVAR, "Randomized frequency scaling in Hz");
        AudioPluginUtil::RegisterParameter(definition, "BW scale", "", 0.001f, 10.0f, 1.0f, 1.0f, 1.0f, P_BWSCALE, "Bandwidth scaling factor");
        AudioPluginUtil::RegisterParameter(definition, "BW scale var", "", 0.001f, 10.0f, 0.001f, 1.0f, 1.0f, P_BWSCALEVAR, "Randomized bandwidth scaling factor");
        AudioPluginUtil::RegisterParameter(definition, "Gain scale", "dB", -100.0f, 100.0f, 0.0f, 1.0f, 1.0f, P_GAINSCALE, "Gain scaling in dB");
        AudioPluginUtil::RegisterParameter(definition, "Gain scale var", "dB", -100.0f, 100.0f, 0.0f, 1.0f, 1.0f, P_GAINSCALEVAR, "Randomized gain scaling in dB");
        AudioPluginUtil::RegisterParameter(definition, "ShowSpectrum", "", 0.0f, 1.0f, 0.0f, 1.0f, 1.0f, P_SHOWSPECTRUM, "Overlay input spectrum (green) and output spectrum (red)");
        AudioPluginUtil::RegisterParameter(definition, "SpectrumDecay", "dB/s", -50.0f, 0.0f, -10.0f, 1.0f, 1.0f, P_SPECTRUMDECAY, "Hold time for overlaid spectra");
        AudioPluginUtil::RegisterParameter(definition, "SpectrumOffset", "dB", -100.0f, 100.0f, 0.0f, 1.0f, 1.0f, P_SPECTRUMOFFSET, "Spectrum drawing offset in dB");
        return numparams;
    }

    UNITY_AUDIODSP_RESULT UNITY_AUDIODSP_CALLBACK CreateCallback(UnityAudioEffectState* state)
    {
        EffectData* effectdata = new EffectData;
        memset(effectdata, 0, sizeof(EffectData));
        state->effectdata = effectdata;
        effectdata->data.analyzer.spectrumSize = 4096;
        effectdata->data.display1 = new float[MAXRESONATORS * 3];
        effectdata->data.display2 = new float[MAXRESONATORS * 3];
        AudioPluginUtil::InitParametersFromDefinitions(InternalRegisterEffectDefinition, effectdata->data.p);
        return UNITY_AUDIODSP_OK;
    }

    UNITY_AUDIODSP_RESULT UNITY_AUDIODSP_CALLBACK ReleaseCallback(UnityAudioEffectState* state)
    {
        EffectData* effectdata = state->GetEffectData<EffectData>();
        EffectData::Data* data = &effectdata->data;
        data->analyzer.Cleanup();
        delete[] data->display1;
        delete[] data->display2;
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
        EffectData::Data* data = &state->GetEffectData<EffectData>()->data;
        if (strcmp(name, "InputSpec") == 0)
            data->analyzer.ReadBuffer(buffer, numsamples, true);
        else if (strcmp(name, "OutputSpec") == 0)
            data->analyzer.ReadBuffer(buffer, numsamples, false);
        else if (strcmp(name, "Coeffs") == 0)
        {
            int maxsamples = 3 * (int)data->p[P_NUMMODES];
            if (numsamples > maxsamples)
                numsamples = maxsamples;
            memcpy(buffer, data->display1, numsamples * sizeof(float));
        }

        return UNITY_AUDIODSP_OK;
    }

    static void SetupResonators(EffectData::Data* data, int numchannels, float sampletime)
    {
        const int nNumResonators = (int)data->p[P_NUMMODES];

        data->random.Seed((int)data->p[P_SEED]);

        float* dst = data->display2;
        for (int i = 0; i < numchannels; i++)
        {
            for (int k = 0; k < nNumResonators; k++)
            {
                float fFreq = 0.002f * (k + 1);
                fFreq *= data->p[P_FREQSCALE] + data->random.GetFloat(-data->p[P_FREQSCALEVAR], data->p[P_FREQSCALEVAR]);
                fFreq += (data->p[P_FREQSHIFT] + data->random.GetFloat(-data->p[P_FREQSHIFTVAR], data->p[P_FREQSHIFTVAR])) * sampletime;
                float fBandwidth = powf(0.01f, data->p[P_BWSCALE] + data->random.GetFloat(-data->p[P_BWSCALEVAR], data->p[P_BWSCALEVAR]));
                float fGain = powf(10.0f, 0.05f * (data->p[P_GAINSCALE] + data->random.GetFloat(-data->p[P_GAINSCALEVAR], data->p[P_GAINSCALEVAR])));
                Resonator& resonator = data->resonators[i][k];
                resonator.Setup(fFreq, fBandwidth, fGain);

                if (i == 0)
                {
                    *dst++ = resonator.a0;
                    *dst++ = resonator.a1;
                    *dst++ = resonator.a2;
                }
            }
        }

        float* tmp = data->display1;
        data->display1 = data->display2;
        data->display2 = tmp;
    }

    UNITY_AUDIODSP_RESULT UNITY_AUDIODSP_CALLBACK ProcessCallback(UnityAudioEffectState* state, float* inbuffer, float* outbuffer, unsigned int length, int inchannels, int outchannels)
    {
        EffectData::Data* data = &state->GetEffectData<EffectData>()->data;

        const float sampletime = 1.0f / state->samplerate;
        float specDecay = powf(10.0f, 0.05f * data->p[P_SPECTRUMDECAY] * length * sampletime);
        bool calcSpectrum = (data->p[P_SHOWSPECTRUM] >= 0.5f);
        if (calcSpectrum)
            data->analyzer.AnalyzeInput(inbuffer, inchannels, length, specDecay);

        memset(outbuffer, 0, outchannels * length * sizeof(float));

        const int nNumResonators = (int)data->p[P_NUMMODES];

        if (memcmp(data->p, data->prevp, sizeof(data->p)) != 0)
        {
            memcpy(data->prevp, data->p, sizeof(data->p));
            SetupResonators(data, inchannels, sampletime);
        }

        for (int i = 0; i < inchannels; i++)
        {
            float denormalFix = data->random.GetFloat(-1.0f, 1.0f) * 1.0e-9f;
            for (int k = 0; k < nNumResonators; k++)
            {
                Resonator& resonator = data->resonators[i][k];
                float* src = inbuffer + i;
                float* dst = outbuffer + i;
                for (unsigned int n = 0; n < length; n++)
                {
                    *dst += resonator.Process(*src + denormalFix);
                    src += inchannels;
                    dst += outchannels;
                }
            }
        }

        if (calcSpectrum)
            data->analyzer.AnalyzeOutput(outbuffer, outchannels, length, specDecay);

        return UNITY_AUDIODSP_OK;
    }
}
