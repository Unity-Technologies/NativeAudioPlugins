#pragma once

#include "AudioPluginInterface.h"

#include <math.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <assert.h>

#if UNITY_WIN
#   include <windows.h>
#else
#   if UNITY_SPU
#       include <stdint.h>
#       include "ps3/AudioPluginInterfacePS3.h"
#   else
#       include <pthread.h>
#   endif
#   define strcpy_s strcpy
#endif

typedef int (*InternalEffectDefinitionRegistrationCallback)(UnityAudioEffectDefinition& desc);

const float kMaxSampleRate = 22050.0f;
const float kPI = 3.141592653589f;

inline float FastClip(float x, float minval, float maxval) { return (fabsf(x - minval) - fabsf(x - maxval) + (minval + maxval)) * 0.5f; }
inline float FastMin(float a, float b) { return (a + b - fabsf(a - b)) * 0.5f; }
inline float FastMax(float a, float b) { return (a + b + fabsf(a - b)) * 0.5f; }
inline int FastFloor(float x) { return (int)floorf(x); } // TODO: Optimize

char* strnew(const char* src);
char* tmpstr(int index, const char* fmtstr, ...);

class UnityComplexNumber
{
public:
    // No constructor because we want to be able to define this inside anonymous unions (this is also why we don't use std::complex<T> here)

    inline void Set(float _re, float _im)
    {
        re = _re;
        im = _im;
    }

    inline void Set(const UnityComplexNumber& c)
    {
        re = c.re;
        im = c.im;
    }

    inline static void Mul(const UnityComplexNumber& a, float b, UnityComplexNumber& result)
    {
        result.re = a.re * b;
        result.im = a.im * b;
    }

    inline static void Mul(const UnityComplexNumber& a, const UnityComplexNumber& b, UnityComplexNumber& result)
    {
        // Store temporarily in case a or b reference the same memory as result
        float t = a.re * b.im + a.im * b.re;
        result.re = a.re * b.re - a.im * b.im;
        result.im = t;
    }

    inline static void Add(const UnityComplexNumber& a, const UnityComplexNumber& b, UnityComplexNumber& result)
    {
        result.re = a.re + b.re;
        result.im = a.im + b.im;
    }

    inline static void Sub(const UnityComplexNumber& a, const UnityComplexNumber& b, UnityComplexNumber& result)
    {
        result.re = a.re - b.re;
        result.im = a.im - b.im;
    }

    inline const UnityComplexNumber operator *(float c) const
    {
        UnityComplexNumber result;
        result.re = re * c;
        result.im = im * c;
        return result;
    }

    inline const UnityComplexNumber operator *(const UnityComplexNumber& c) const
    {
        UnityComplexNumber result;
        result.re = re * c.re - im * c.im;
        result.im = re * c.im + im * c.re;
        return result;
    }

    inline const UnityComplexNumber operator +(const UnityComplexNumber& c) const
    {
        UnityComplexNumber result;
        result.re = re + c.re;
        result.im = im + c.im;
        return result;
    }

    inline const UnityComplexNumber operator -(const UnityComplexNumber& c) const
    {
        UnityComplexNumber result;
        result.re = re - c.re;
        result.im = im - c.im;
        return result;
    }

    inline float Magnitude() const
    {
        return sqrtf(re * re + im * im);
    }

    inline float Magnitude2() const
    {
        return re * re + im * im;
    }

public:
    float re, im;
};

class FFT
{
public:
    static void Forward(UnityComplexNumber* data, int numsamples);
    static void Backward(UnityComplexNumber* data, int numsamples);
};

class FFTAnalyzer : public FFT
{
public:
    void Cleanup(); // Assumes zero-initialization
    void AnalyzeInput(float* data, int numchannels, int numsamples, float specAlpha);
    void AnalyzeOutput(float* data, int numchannels, int numsamples, float specAlpha);
    void CheckInitialized();
    bool CanBeRead() const;
    void ReadBuffer(float* buffer, int numsamples, bool readInputBuffer);

public:
    float* window;
    float* ibuffer;
    float* obuffer;
    UnityComplexNumber* cspec;
    float* ispec1;
    float* ispec2;
    float* ospec1;
    float* ospec2;
    int spectrumSize;
    int numSpectraReady;
};

class HistoryBuffer
{
public:
    HistoryBuffer();
    ~HistoryBuffer();

public:
    void Init(int _length);
    void ReadBuffer(float* buffer, int numsamplesTarget, int numsamplesSource, float offset);

public:
    inline void Feed(float sample)
    {
        // Don't try to optimize this with ++
        // The writeindex veriable may be read at the same time, so we don't want intermediate values indexing out of the array.
        int w = writeindex + 1;
        if (w == length)
            w = 0;
        data[w] = sample;
        writeindex = w;
    }

public:
    int length;
    int writeindex;
    float* data;
};

template<const int _LENGTH, typename T = float>
class RingBuffer
{
public:
    enum { LENGTH = _LENGTH };

    volatile int readpos;
    volatile int writepos;
    T buffer[LENGTH];

    inline bool Read(T& val)
    {
        int r = readpos;
        if (r == writepos)
            return false;
        r = (r == LENGTH - 1) ? 0 : (r + 1);
        val = buffer[r];
        readpos = r;
        return true;
    }

    inline void Skip(int num)
    {
        int r = readpos + num;
        if (r >= LENGTH)
            r -= LENGTH;
        readpos = r;
    }

    inline void SyncWritePos()
    {
        writepos = readpos;
    }

    inline bool Feed(const T& input)
    {
        int w = (writepos == LENGTH - 1) ? 0 : (writepos + 1);
        buffer[w] = input;
        writepos = w;
        return true;
    }

    inline int GetNumBuffered() const
    {
        int b = writepos - readpos;
        if (b < 0)
            b += LENGTH;
        return b;
    }

    inline void Clear()
    {
        writepos = 0;
        readpos = 0;
    }
};

class BiquadFilter
{
public:
    inline void SetupPeaking(float cutoff, float samplerate, float gain, float Q);
    inline void SetupLowShelf(float cutoff, float samplerate, float gain, float Q);
    inline void SetupHighShelf(float cutoff, float samplerate, float gain, float Q);
    inline void SetupLowpass(float cutoff, float samplerate, float Q);
    inline void SetupHighpass(float cutoff, float samplerate, float Q);

public:
    inline float Process(float input)
    {
        float iir =    input - a1 * z1 - a2 * z2;
        float fir = b0 * iir + b1 * z1 + b2 * z2;
        z2 = z1;
        z1 = iir;
        return fir;
    }

    inline void StoreCoeffs(float*& data)
    {
        *data++ = b2;
        *data++ = b1;
        *data++ = b0;
        *data++ = a2;
        *data++ = a1;
    }

protected:
    float a1, a2, b0, b1, b2;
    float z1, z2;
};

// The filter coefficient formulae below are taken from Robert Bristow-Johnsons excellent EQ biquad filter cookbook:
// http://www.musicdsp.org/files/Audio-EQ-Cookbook.txt

void BiquadFilter::SetupPeaking(float cutoff, float samplerate, float gain, float Q)
{
    float w0 = 2.0f * kPI * cutoff / samplerate, A = powf(10.0f, gain * 0.025f), alpha = sinf(w0) / (2.0f * Q), a0;
    b0 = 1.0f + alpha * A;
    b1 = -2.0f * cosf(w0);
    b2 = 1.0f - alpha * A;
    a0 = 1.0f + alpha / A;
    a1 = -2.0f * cosf(w0);
    a2 = 1.0f - alpha / A;
    float inv_a0 = 1.0f / a0; a1 *= inv_a0; a2 *= inv_a0; b0 *= inv_a0; b1 *= inv_a0; b2 *= inv_a0;
}

void BiquadFilter::SetupLowShelf(float cutoff, float samplerate, float gain, float Q)
{
    float w0 = 2.0f * kPI * cutoff / samplerate, A = powf(10.0f, gain * 0.025f), alpha = sinf(w0) / (2.0f * Q), a0;
    b0 =          A * ((A + 1.0f) - (A - 1.0f) * cosf(w0) + 2.0f * sqrtf(A) * alpha);
    b1 =   2.0f * A * ((A - 1.0f) - (A + 1.0f) * cosf(w0));
    b2 =          A * ((A + 1.0f) - (A - 1.0f) * cosf(w0) - 2.0f * sqrtf(A) * alpha);
    a0 =               (A + 1.0f) + (A - 1.0f) * cosf(w0) + 2.0f * sqrtf(A) * alpha;
    a1 =  -2.0f     * ((A - 1.0f) + (A + 1.0f) * cosf(w0));
    a2 =               (A + 1.0f) + (A - 1.0f) * cosf(w0) - 2.0f * sqrtf(A) * alpha;
    float inv_a0 = 1.0f / a0; a1 *= inv_a0; a2 *= inv_a0; b0 *= inv_a0; b1 *= inv_a0; b2 *= inv_a0;
}

void BiquadFilter::SetupHighShelf(float cutoff, float samplerate, float gain, float Q)
{
    float w0 = 2.0f * kPI * cutoff / samplerate, A = powf(10.0f, gain * 0.025f), alpha = sinf(w0) / (2.0f * Q), a0;
    b0 =          A * ((A + 1.0f) + (A - 1.0f) * cosf(w0) + 2.0f * sqrtf(A) * alpha);
    b1 =  -2.0f * A * ((A - 1.0f) + (A + 1.0f) * cosf(w0));
    b2 =          A * ((A + 1.0f) + (A - 1.0f) * cosf(w0) - 2.0f * sqrtf(A) * alpha);
    a0 =               (A + 1.0f) - (A - 1.0f) * cosf(w0) + 2.0f * sqrtf(A) * alpha;
    a1 =   2.0f     * ((A - 1.0f) - (A + 1.0f) * cosf(w0));
    a2 =               (A + 1.0f) - (A - 1.0f) * cosf(w0) - 2.0f * sqrtf(A) * alpha;
    float inv_a0 = 1.0f / a0; a1 *= inv_a0; a2 *= inv_a0; b0 *= inv_a0; b1 *= inv_a0; b2 *= inv_a0;
}

void BiquadFilter::SetupLowpass(float cutoff, float samplerate, float Q)
{
    float w0 = 2.0f * kPI * cutoff / samplerate, alpha = sinf(w0) / (2.0f * Q), a0;
    b0 =  (1.0f - cosf(w0)) * 0.5f;
    b1 =   1.0f - cosf(w0);
    b2 =  (1.0f - cosf(w0)) * 0.5f;
    a0 =   1.0f + alpha;
    a1 =  -2.0f * cosf(w0);
    a2 =   1.0f - alpha;
    float inv_a0 = 1.0f / a0; a1 *= inv_a0; a2 *= inv_a0; b0 *= inv_a0; b1 *= inv_a0; b2 *= inv_a0;
}

void BiquadFilter::SetupHighpass(float cutoff, float samplerate, float Q)
{
    float w0 = 2.0f * kPI * cutoff / samplerate, alpha = sinf(w0) / (2.0f * Q), a0;
    b0 =  (1.0f + cosf(w0)) * 0.5f;
    b1 = -(1.0f + cosf(w0));
    b2 =  (1.0f + cosf(w0)) * 0.5f;
    a0 =   1.0f + alpha;
    a1 =  -2.0f * cosf(w0);
    a2 =   1.0f - alpha;
    float inv_a0 = 1.0f / a0; a1 *= inv_a0; a2 *= inv_a0; b0 *= inv_a0; b1 *= inv_a0; b2 *= inv_a0;
}

class StateVariableFilter
{
public:
    float cutoff;
    float bandwidth;

public:
    inline float ProcessHPF(float input)
    {
        input += 1.0e-11f; // Kill denormals

        lpf += cutoff * bpf;
        float hpf = (input - bpf) * bandwidth - lpf;
        bpf += cutoff * hpf;

        lpf += cutoff * bpf;
        hpf = (input - bpf) * bandwidth - lpf;
        bpf += cutoff * hpf;

        return hpf;
    }

    inline float ProcessBPF(float input)
    {
        ProcessHPF(input);
        return bpf;
    }

    inline float ProcessLPF(float input)
    {
        ProcessHPF(input);
        return lpf;
    }

public:
    float lpf, bpf;
};

class Random
{
public:
    inline void Seed(unsigned long _seed)
    {
        seed = _seed;
    }

    inline unsigned int Get()
    {
        seed = (seed * 1664525 + 1013904223) & 0xFFFFFFFF;
        return seed ^ (seed >> 16);
    }

    inline float GetFloat(float minval, float maxval)
    {
        return minval + (maxval - minval) * (Get() & 0xFFFFFF) * (const float)(1.0f / (float)0xFFFFFF);
    }

protected:
    unsigned int seed;
};

class NoiseGenerator
{
public:
    void Init()
    {
        level = 0.0f;
        delta = 0.0f;
        minval = 0.0f;
        maxval = 1.0f;
        period = 100.0f;
        invperiod = 0.01f;
        samplesleft = 0;
    }

    inline void SetRange(float minval, float maxval)
    {
        this->minval = minval;
        this->maxval = maxval;
    }

    inline void SetPeriod(float period)
    {
        SetPeriod(period, 1.0f / period);
    }

    inline void SetPeriod(float period, float invperiod)
    {
        period = period;
        invperiod = invperiod;
    }

    inline float Sample(Random& random)
    {
        if (--samplesleft <= 0)
        {
            samplesleft = (int)period;
            delta = (random.GetFloat(minval, maxval) - level) * invperiod;
        }
        level += delta;
        return level;
    }

public:
    float level;
    float delta;
    float minval;
    float maxval;
    float period;
    float invperiod;
    int samplesleft;
};

class Mutex
{
public:
    Mutex();
    ~Mutex();
public:
    bool TryLock();
    void Lock();
    void Unlock();
protected:
#if UNITY_WIN
    CRITICAL_SECTION crit_sec;
#else
# if !UNITY_SPU
    pthread_mutex_t mutex;
# endif
#endif
};

class MutexScopeLock
{
public:
    MutexScopeLock(Mutex& _mutex, bool condition = true) : mutex(condition ? &_mutex : NULL) { if (mutex != NULL) mutex->Lock(); }
    ~MutexScopeLock() { if (mutex != NULL) mutex->Unlock(); }
protected:
    Mutex* mutex;
};

void RegisterParameter(
    UnityAudioEffectDefinition& desc,
    const char* name,
    const char* unit,
    float minval,
    float maxval,
    float defaultval,
    float displayscale,
    float displayexponent,
    int enumvalue,
    const char* description = NULL
    );

void InitParametersFromDefinitions(
    InternalEffectDefinitionRegistrationCallback registereffectdefcallback,
    float* params
    );

void DeclareEffect(
    UnityAudioEffectDefinition& desc,
    const char* name,
    UnityAudioEffect_CreateCallback createcallback,
    UnityAudioEffect_ReleaseCallback releasecallback,
    UnityAudioEffect_ProcessCallback processcallback,
    UnityAudioEffect_SetFloatParameterCallback setfloatparametercallback,
    UnityAudioEffect_GetFloatParameterCallback getfloatparametercallback,
    UnityAudioEffect_GetFloatBufferCallback getfloatbuffercallback,
    InternalEffectDefinitionRegistrationCallback registereffectdefcallback
    );
