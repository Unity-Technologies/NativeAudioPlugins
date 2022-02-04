#include "AudioPluginUtil.h"

namespace ConvolutionReverb
{
    const float MAXLENGTH = 15.0f;
    const int MAXSAMPLE = 16;

    AudioPluginUtil::Mutex sampleMutex;

    struct IRSample
    {
        float* data;
        int numsamples;
        int numchannels;
        int samplerate;
        int updatecount;
        int allocated;
        char name[1024];
    };

    inline IRSample& GetIRSample(int index)
    {
        static bool initialized = false;
        static IRSample samples[MAXSAMPLE];
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
        P_WET,
        P_GAIN,
        P_TIME,
        P_DECAY,
        P_DIFFUSION,
        P_STEREO,
        P_CUTHI,
        P_CUTLO,
        P_RESONANCE,
        P_USESAMPLE,
        P_REVERSE,
        P_NUM
    };

    struct Channel
    {
        AudioPluginUtil::UnityComplexNumber** h;
        AudioPluginUtil::UnityComplexNumber** x;
        float* impulse;
        float* s;
    };

    struct EffectData
    {
        AudioPluginUtil::Mutex* mutex;
        float p[P_NUM];
        int numchannels;
        int numpartitions;
        int fftsize;
        int hopsize;
        int bufferindex;
        int writeoffset;
        int samplerate;
        float lastparams[P_NUM];
        AudioPluginUtil::UnityComplexNumber* tmpoutput;
        Channel* channels;
    };

    int InternalRegisterEffectDefinition(UnityAudioEffectDefinition& definition)
    {
        int numparams = P_NUM;
        definition.paramdefs = new UnityAudioParameterDefinition[numparams];
        AudioPluginUtil::RegisterParameter(definition, "Wet", "%", 0.0f, 100.0f, 30.0f, 1.0f, 1.0f, P_WET, "Wet signal mix amount");
        AudioPluginUtil::RegisterParameter(definition, "Gain", "dB", -50.0f, 50.0f, 0.0f, 1.0f, 1.0f, P_GAIN, "Overall impulse response gain");
        AudioPluginUtil::RegisterParameter(definition, "Time", "s", 0.01f, MAXLENGTH, 2.0f, 1.0f, 3.0f, P_TIME, "Length of synthetic impulse response");
        AudioPluginUtil::RegisterParameter(definition, "Decay", "%", 0.01f, 100.0f, 50.0f, 1.0f, 3.0f, P_DECAY, "Decay time of synthetic impulse response and filter curve");
        AudioPluginUtil::RegisterParameter(definition, "Diffusion", "%", 0.0f, 100.0f, 100.0f, 1.0f, 1.0f, P_DIFFUSION, "Diffusiveness of synthetic impulse response");
        AudioPluginUtil::RegisterParameter(definition, "StereoSpread", "%", 0.0f, 100.0f, 30.0f, 1.0f, 1.0f, P_STEREO, "Stereo width of synthetic impulse response");
        AudioPluginUtil::RegisterParameter(definition, "Cut High", "Hz", 1.0f, 20000.0f, 10000.0f, 1.0f, 3.0f, P_CUTHI, "High cutoff of filter decay curve (applied both to synthetic and sample impulse responses)");
        AudioPluginUtil::RegisterParameter(definition, "Cut Low", "Hz", 1.0f, 20000.0f, 8000.0f, 1.0f, 3.0f, P_CUTLO, "Low cutoff of filter decay curve (applied both to synthetic and sample impulse responses)");
        AudioPluginUtil::RegisterParameter(definition, "Resonance", "%", 0.0f, 1.0f, 0.0f, 100.0f, 3.0f, P_RESONANCE, "Resonance amount of filter (applied both to synthetic and sample impulse responses)");
        AudioPluginUtil::RegisterParameter(definition, "Use Sample", "", -1.0f, MAXSAMPLE - 1, -1.0f, 1.0f, 1.0f, P_USESAMPLE, "-1 = use synthetic impulse response, otherwise indicates the slot of a sample uploaded by scripts via ConvolutionReverb_UploadSample");
        AudioPluginUtil::RegisterParameter(definition, "Reverse", "", 0.0f, 1.0f, 0.0f, 1.0f, 1.0f, P_REVERSE, "Reverse impulse response for scary effects ;-)");
        return numparams;
    }

    static void SetupImpulse(EffectData* data, int numchannels, int blocksize, int samplerate)
    {
        AudioPluginUtil::MutexScopeLock mutexScope1(*data->mutex);

        AudioPluginUtil::Random random;

        int usesample = (int)data->p[P_USESAMPLE];

        // if no parameters have changed, there's no need to recalculate the impulse
        if (data->numchannels == numchannels &&
            data->hopsize == blocksize &&
            data->samplerate == samplerate &&
            data->lastparams[P_TIME] == data->p[P_TIME] &&
            data->lastparams[P_DECAY] == data->p[P_DECAY] &&
            data->lastparams[P_DIFFUSION] == data->p[P_DIFFUSION] &&
            data->lastparams[P_STEREO] == data->p[P_STEREO] &&
            data->lastparams[P_CUTHI] == data->p[P_CUTHI] &&
            data->lastparams[P_CUTLO] == data->p[P_CUTLO] &&
            data->lastparams[P_RESONANCE] == data->p[P_RESONANCE] &&
            (int)data->lastparams[P_USESAMPLE] == usesample &&
            data->lastparams[P_REVERSE] == data->p[P_REVERSE] &&
            (usesample < 0 || GetIRSample(usesample).updatecount == globalupdatecount)
            )
            return;

        AudioPluginUtil::MutexScopeLock mutexScope2(sampleMutex);

        // delete old buffers (can be avoided if numchannels, numpartitions and hopsize stay the same)
        for (int i = 0; i < data->numchannels; i++)
        {
            Channel& c = data->channels[i];
            for (int k = 0; k < data->numpartitions; k++)
            {
                delete[] c.h[k];
                delete[] c.x[k];
            }
            delete[] c.h;
            delete[] c.x;
            delete[] c.s;
            delete[] c.impulse;
        }
        delete[] data->channels;
        delete[] data->tmpoutput;

        memcpy(data->lastparams, data->p, sizeof(data->p));

        // reinitialize data
        data->bufferindex = 0;
        data->writeoffset = 0;
        data->numchannels = numchannels;
        data->hopsize = blocksize;
        data->fftsize = blocksize * 2;
        data->tmpoutput = new AudioPluginUtil::UnityComplexNumber[data->fftsize];
        data->channels = new Channel[data->numchannels];
        data->samplerate = samplerate;

        memset(data->tmpoutput, 0, sizeof(AudioPluginUtil::UnityComplexNumber) * data->fftsize);

        // calculate length of impulse in samples
        int reallength = (int)ceilf(samplerate * data->p[P_TIME]);
        if (usesample >= 0)
        {
            IRSample& s = GetIRSample(usesample);
            if (s.numsamples == 0)
                reallength = 256;
            else
                reallength = (int)ceilf(s.numsamples * (float)samplerate / (float)s.samplerate);
        }

        // calculate length of impulse in samples as a multiple of the number of partitions processed
        data->numpartitions = 0;
        while (data->numpartitions * data->hopsize < reallength)
            data->numpartitions++;
        int impulsesamples = data->numpartitions * data->hopsize;

        // calculate individual impulse responses per channel
        float sampletime = 1.0f / (float)samplerate;
        for (int i = 0; i < data->numchannels; i++)
        {
            Channel& c = data->channels[i];
            c.impulse = new float[impulsesamples];
            c.s = new float[data->fftsize];
            memset(c.s, 0, sizeof(float) * data->fftsize);

            float cuthi = 2.0f * sinf(0.25f * AudioPluginUtil::kPI * data->p[P_CUTHI] * sampletime);
            float cutlo = 2.0f * sinf(0.25f * AudioPluginUtil::kPI * data->p[P_CUTLO] * sampletime);
            float bw = 0.9f - 0.89f * data->p[P_RESONANCE]; bw *= bw;
            float decayconst = (data->p[P_STEREO] * random.GetFloat(0.0f, 0.01f) - 1.0f) / (reallength * 0.01f * data->p[P_DECAY]);

            if (usesample < 0)
            {
                // calculate the impulse response as decaying white noise
                float d = 10.0f - 0.09f * data->p[P_DIFFUSION];
                for (int n = 0; n < impulsesamples; n++)
                {
                    float env = expf(decayconst * n);
                    c.impulse[n] = env * powf(random.GetFloat(0.1f, 1.0f), d) * random.GetFloat(-1.0f, 1.0f);
                }
            }
            else
            {
                IRSample& s = GetIRSample(usesample);
                if (s.numsamples == 0)
                {
                    static float dummydata[256 * 8];
                    static IRSample dummysample;
                    dummysample.data = dummydata;
                    dummysample.numchannels = numchannels;
                    dummysample.numsamples = 256;
                    dummysample.samplerate = samplerate;
                    for (int n = 0; n < numchannels; n++)
                        dummydata[n] = 1.0f;
                    s = dummysample;
                }

                int channel = (i < numchannels) ? i : (numchannels - 1);
                float speed = (float)s.samplerate / (float)samplerate;
                for (int n = 0; n < impulsesamples; n++)
                {
                    float fpos = n * speed;
                    int ipos1 = (int)ceilf(fpos);
                    if (ipos1 >= s.numsamples)
                        ipos1 = s.numsamples - 1;
                    int ipos2 = ipos1 + 1;
                    if (ipos2 >= s.numsamples)
                        ipos2 = s.numsamples - 1;
                    fpos -= ipos1;
                    float s1 = s.data[ipos1 * s.numchannels + channel];
                    float s2 = s.data[ipos2 * s.numchannels + channel];
                    c.impulse[n] = s1 + (s2 - s1) * fpos;
                }

                s.updatecount = globalupdatecount;
            }

            float lpf = 0.0f, bpf = 0.0f, gain = 0.5f * (1.0f - bw * bw);
            for (int n = 0; n < impulsesamples; n++)
            {
                float env = expf(decayconst * n);
                float cut = cutlo + (cuthi - cutlo) * env;
                lpf += cut * bpf;
                bpf += cut * (c.impulse[n] - lpf - bpf * bw);
                lpf += cut * bpf;
                bpf -= cut * (lpf + bpf * bw);
                c.impulse[n] = gain * lpf;
                //c.impulse[n] = env * sinf(n * 2.0f * 3.1415926f * 1000.0f / 44100.0f); // damped sine -- useful for debugging with click input signals
            }

            if (data->p[P_REVERSE] > 0.5f)
            {
                int len = impulsesamples >> 1;
                for (int n = 0; n < len; n++)
                {
                    float tmp = c.impulse[n];
                    c.impulse[n] = c.impulse[impulsesamples - 1 - n];
                    c.impulse[impulsesamples - 1 - n] = tmp;
                }
            }

            // measure signal power
            float power = 0.0f;
            for (int n = 0; n < impulsesamples; n++)
                power += c.impulse[n] * c.impulse[n];

            // normalize gain
            float scale = 1.0f / sqrtf(power);
            for (int n = 0; n < impulsesamples; n++)
                c.impulse[n] *= scale;

            // partition the impulse response
            c.h = new AudioPluginUtil::UnityComplexNumber*[data->numpartitions];
            c.x = new AudioPluginUtil::UnityComplexNumber*[data->numpartitions];
            float* src = c.impulse;
            for (int k = 0; k < data->numpartitions; k++)
            {
                c.h[k] = new AudioPluginUtil::UnityComplexNumber[data->fftsize];
                c.x[k] = new AudioPluginUtil::UnityComplexNumber[data->fftsize];
                memset(c.x[k], 0, sizeof(AudioPluginUtil::UnityComplexNumber) * data->fftsize);
                memset(c.h[k], 0, sizeof(AudioPluginUtil::UnityComplexNumber) * data->fftsize);
                for (int n = 0; n < data->hopsize; n++)
                    c.h[k][n].re = *src++;
                AudioPluginUtil::FFT::Forward(c.h[k], data->fftsize, false);
            }

            // integrate peak detection filtered impulse for later resampling via box-filtering when GUI requests preview waveform
            double sum = 0.0, peak = 0.0;
            for (int n = 0; n < impulsesamples; n++)
            {
                float a = fabsf(c.impulse[n]);
                if (a > peak)
                    peak = a;
                else
                    peak = peak * 0.99f + 1.0e-9f;
                sum += peak;
                c.impulse[n] = (float)sum;
            }
            double dc = -sum / (double)impulsesamples;
            sum = 0.0;
            for (int n = 0; n < impulsesamples; n++)
            {
                c.impulse[n] -= (float)sum;
                sum -= dc;
            }
        }
    }

    UNITY_AUDIODSP_RESULT UNITY_AUDIODSP_CALLBACK CreateCallback(UnityAudioEffectState* state)
    {
        EffectData* data = new EffectData;
        memset(data, 0, sizeof(EffectData));
        data->mutex = new AudioPluginUtil::Mutex();
        state->effectdata = data;
        AudioPluginUtil::InitParametersFromDefinitions(InternalRegisterEffectDefinition, data->p);
        SetupImpulse(data, 2, 1024, state->samplerate); // Assuming stereo and 1024 sample block size
        return UNITY_AUDIODSP_OK;
    }

    UNITY_AUDIODSP_RESULT UNITY_AUDIODSP_CALLBACK ReleaseCallback(UnityAudioEffectState* state)
    {
        EffectData* data = state->GetEffectData<EffectData>();
        delete data->mutex;
        delete data;
        return UNITY_AUDIODSP_OK;
    }

    UNITY_AUDIODSP_RESULT UNITY_AUDIODSP_CALLBACK ProcessCallback(UnityAudioEffectState* state, float* inbuffer, float* outbuffer, unsigned int length, int inchannels, int outchannels)
    {
        EffectData* data = state->GetEffectData<EffectData>();

        const float wet = data->p[P_WET] * 0.01f;
        const float gain = powf(10.0f, 0.05f * data->p[P_GAIN]);

        // this should be done on a separate thread to avoid cpu spikes
        SetupImpulse(data, outchannels, (int)length, state->samplerate);

        // Lock data here in case float parameters are changed in pause/stopped mode and cause further calls to SetupImpulse
        AudioPluginUtil::MutexScopeLock mutexScope1(*data->mutex);

        int writeoffset; // set inside loop

        for (int i = 0; i < inchannels; i++)
        {
            Channel& c = data->channels[i];

            // feed new data to input buffer s
            float* s = c.s;
            const int mask = data->fftsize - 1;
            writeoffset = data->writeoffset;
            for (int n = 0; n < data->hopsize; n++)
            {
                s[writeoffset] = inbuffer[n * inchannels + i];
                writeoffset = (writeoffset + 1) & mask;
            }

            // calculate X=FFT(s)
            writeoffset = data->writeoffset;
            AudioPluginUtil::UnityComplexNumber* x = c.x[data->bufferindex];
            for (int n = 0; n < data->fftsize; n++)
            {
                x[n].Set(s[writeoffset], 0.0f);
                writeoffset = (writeoffset + 1) & mask;
            }
            AudioPluginUtil::FFT::Forward(x, data->fftsize, false);

            writeoffset = (writeoffset + data->hopsize) & mask;

            // calculate y=IFFT(sum(convolve(H_k, X_k), k=1..numpartitions))
            AudioPluginUtil::UnityComplexNumber* y = data->tmpoutput;
            memset(y, 0, sizeof(AudioPluginUtil::UnityComplexNumber) * data->fftsize);
            for (int k = 0; k < data->numpartitions; k++)
            {
                AudioPluginUtil::UnityComplexNumber* h = c.h[k];
                AudioPluginUtil::UnityComplexNumber* x = c.x[(k + data->bufferindex) % data->numpartitions];
                for (int n = 0; n < data->fftsize; n++)
                    AudioPluginUtil::UnityComplexNumber::MulAdd(h[n], x[n], y[n], y[n]);
            }
            AudioPluginUtil::FFT::Backward(y, data->fftsize, false);

            // overlap-save readout
            for (int n = 0; n < data->hopsize; n++)
            {
                float input = inbuffer[n * outchannels + i];
                outbuffer[n * outchannels + i] = input + (gain * y[n].re - input) * wet;
            }
        }

        if (--data->bufferindex < 0)
            data->bufferindex = data->numpartitions - 1;

        data->writeoffset = writeoffset;

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
        AudioPluginUtil::MutexScopeLock mutexScope(*data->mutex);
        if (strncmp(name, "Impulse", 7) == 0)
        {
            SetupImpulse(data, data->numchannels, data->hopsize, data->samplerate);
            int index = name[7] - '0';
            if (index >= data->numchannels)
                return UNITY_AUDIODSP_OK;
            const float* src = data->channels[index].impulse;
            float scale = (float)(data->hopsize * data->numpartitions - 2) / (float)numsamples;
            float prev_val = 0.0f, time_scale = 1.0f / scale;
            for (int n = 0; n < numsamples; n++)
            {
                // resample pre-integrated curve via box-filtering: f(x) = (F(x+dx)-F(x)) / dx
                float next_time = n * scale;
                int i = AudioPluginUtil::FastFloor(next_time);
                float next_val = src[i] + (src[i + 1] - src[i]) * (next_time - i);
                buffer[n] = (next_val - prev_val) * time_scale;
                prev_val = next_val;
            }
        }
        return UNITY_AUDIODSP_OK;
    }
}

extern "C" UNITY_AUDIODSP_EXPORT_API bool ConvolutionReverb_UploadSample(int index, float* data, int numsamples, int numchannels, int samplerate, const char* name)
{
    if (index < 0 || index >= ConvolutionReverb::MAXSAMPLE)
        return false;
    AudioPluginUtil::MutexScopeLock mutexScope(ConvolutionReverb::sampleMutex);
    ConvolutionReverb::IRSample& s = ConvolutionReverb::GetIRSample(index);
    if (s.allocated)
        delete[] s.data;
    int num = numsamples * numchannels;
    if (num > 0)
    {
        s.data = new float[num];
        s.allocated = 1;
        strcpy_s(s.name, name);
        memcpy(s.data, data, numsamples * numchannels * sizeof(float));
    }
    else
    {
        s.data = NULL;
        s.allocated = 1;
    }
    s.numsamples = numsamples;
    s.numchannels = numchannels;
    s.samplerate = samplerate;
    s.updatecount = ++ConvolutionReverb::globalupdatecount;
    return true;
}

extern "C" UNITY_AUDIODSP_EXPORT_API const char* ConvolutionReverb_GetSampleName(int index)
{
    if (index < 0)
        return "Synthetic";

    if (index < ConvolutionReverb::MAXSAMPLE)
    {
        AudioPluginUtil::MutexScopeLock mutexScope(ConvolutionReverb::sampleMutex);
        ConvolutionReverb::IRSample& s = ConvolutionReverb::GetIRSample(index);
        if (!s.allocated)
            return "Not set";
        return s.name;
    }

    return "Not set";
}
