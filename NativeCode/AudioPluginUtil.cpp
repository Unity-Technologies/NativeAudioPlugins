#include "AudioPluginUtil.h"
#include <stdarg.h>

namespace AudioPluginUtil
{

#define ENABLE_TESTS ((PLATFORM_WIN || PLATFORM_OSX) && 1)

char* strnew(const char* src)
{
    size_t size = strlen(src) + (size_t)1;
    char* newstr = new char[size];
    memcpy(newstr, src, size);
    return newstr;
}

char* tmpstr(int index, const char* fmtstr, ...)
{
    static char buf[4][1024];
    va_list args;
    va_start(args, fmtstr);
    vsprintf_s(buf[index], fmtstr, args);
    va_end(args);
    return buf[index];
}

template<typename T> void UnitySwap(T& a, T& b) { T t = a; a = b; b = t; }

template<typename T>
static void FFTProcess(UnityComplexNumber* data, int numsamples, bool forward)
{
    unsigned int count = 1, numbits = 0;
    while ((int)count < numsamples)
    {
        count += count;
        ++numbits;
    }

    static unsigned int* reversetable[32] = { NULL };
    unsigned int* tbl = reversetable[numbits];
    if (tbl == NULL)
    {
        tbl = new unsigned int[numsamples];
        for (unsigned int n = 0; n < (unsigned)numsamples; n++)
        {
            unsigned int j = 1, k = 0, m = numsamples >> 1;
            while (m > 0)
            {
                if (n & m)
                    k |= j;
                j += j;
                m >>= 1;
            }
            tbl[n] = k;
        }
#if ENABLE_TESTS
        for (unsigned int n = 0; n < (unsigned)numsamples; n++)
        {
            assert(tbl[tbl[n]] == n);
        }
#endif
        reversetable[numbits] = tbl;
    }

    for (unsigned int i = 0; i < (unsigned)numsamples; i++)
    {
        unsigned int j = tbl[i];
        if (i < j)
        {
            UnitySwap(data[i].re, data[j].re);
            UnitySwap(data[i].im, data[j].im);
        }
    }

    T w0 = (forward) ? -T(kPI_double) : T(kPI_double);
    for (int j = 1; j < numsamples; j += j)
    {
        UnityComplexNumberT<T> wr, wd;
        wr.Set(T(cos(w0)), T(sin(w0)));
        wd.Set(T(1.0), T(0.0));
        int step = j + j;
        for (int m = 0; m < j; ++m)
        {
            for (int i = m; i < numsamples; i += step)
            {
                UnityComplexNumberT<T> t;
                UnityComplexNumber::Mul(wd, data[i + j], t);
                UnityComplexNumber::Sub(data[i], t, data[i + j]);
                UnityComplexNumber::Add(data[i], t, data[i]);
            }
            UnityComplexNumber::Mul(wd, wr, wd);
        }
        w0 *= T(0.5);
    }
}

void FFT::Forward(UnityComplexNumber* data, int numsamples, bool highprecision)
{
    if (highprecision)
        FFTProcess<double>(data, numsamples, true);
    else
        FFTProcess<float>(data, numsamples, true);
}

void FFT::Backward(UnityComplexNumber* data, int numsamples, bool highprecision)
{
    if (highprecision)
        FFTProcess<double>(data, numsamples, false);
    else
        FFTProcess<float>(data, numsamples, false);

    const float scale = 1.0f / (float)numsamples;
    for (int n = 0; n < numsamples; n++)
    {
        data[n].re *= scale;
        data[n].im *= scale;
    }
}

void FFTAnalyzer::Cleanup()
{
    delete[] window;
    delete[] ibuffer;
    delete[] obuffer;
    delete[] ispec1;
    delete[] ispec2;
    delete[] ospec1;
    delete[] ospec2;
    delete[] cspec;
}

void FFTAnalyzer::AnalyzeInput(float* data, int numchannels, int numsamples, float decaySpeed)
{
    CheckInitialized();

    for (int n = 0; n < spectrumSize - numsamples; n++)
        ibuffer[n] = ibuffer[n + numsamples];
    for (int n = 0; n < numsamples; n++)
        ibuffer[n + spectrumSize - numsamples] = data[n * numchannels];
    for (int n = 0; n < spectrumSize; n++)
        cspec[n].Set(ibuffer[n] * window[n], 0.0f);
    Forward(cspec, spectrumSize, true);
    for (int n = 0; n < spectrumSize / 2; n++)
    {
        float a = cspec[n].Magnitude();
        ispec1[n] = (a > ispec2[n]) ? a : ispec2[n] * decaySpeed;
    }
}

void FFTAnalyzer::AnalyzeOutput(float* data, int numchannels, int numsamples, float decaySpeed)
{
    CheckInitialized();

    for (int n = 0; n < spectrumSize - numsamples; n++)
        obuffer[n] = obuffer[n + numsamples];
    for (int n = 0; n < numsamples; n++)
        obuffer[n + spectrumSize - numsamples] = data[n * numchannels];
    for (int n = 0; n < spectrumSize; n++)
        cspec[n].Set(obuffer[n] * window[n], 0.0f);
    Forward(cspec, spectrumSize, true);
    for (int n = 0; n < spectrumSize / 2; n++)
    {
        float a = cspec[n].Magnitude();
        ospec1[n] = (a > ospec2[n]) ? a : ospec2[n] * decaySpeed;
    }

    float* tmp;
    tmp = ispec1; ispec1 = ispec2; ispec2 = tmp;
    tmp = ospec1; ospec1 = ospec2; ospec2 = tmp;

    if (numSpectraReady < 2)
        numSpectraReady++;
}

void FFTAnalyzer::CheckInitialized()
{
    if (window == NULL)
    {
        window = new float[spectrumSize];
        ibuffer = new float[spectrumSize];
        obuffer = new float[spectrumSize];
        ispec1 = new float[spectrumSize / 2];
        ispec2 = new float[spectrumSize / 2];
        ospec1 = new float[spectrumSize / 2];
        ospec2 = new float[spectrumSize / 2];
        cspec = new UnityComplexNumber[spectrumSize];
        for (int n = 0; n < spectrumSize; n++)
            window[n] = 0.54f - 0.46f * cosf(n * (kPI / (float)spectrumSize));
        memset(ibuffer, 0, sizeof(float) * spectrumSize);
        memset(obuffer, 0, sizeof(float) * spectrumSize);
        memset(ispec1, 0, sizeof(float) * (spectrumSize / 2));
        memset(ispec2, 0, sizeof(float) * (spectrumSize / 2));
        memset(ospec1, 0, sizeof(float) * (spectrumSize / 2));
        memset(ospec2, 0, sizeof(float) * (spectrumSize / 2));
        memset(cspec, 0, sizeof(UnityComplexNumber) * spectrumSize);
    }
}

bool FFTAnalyzer::CanBeRead() const
{
    return numSpectraReady >= 2;
}

void FFTAnalyzer::ReadBuffer(float* buffer, int numsamples, bool readInputBuffer)
{
    if (!CanBeRead())
    {
        memset(buffer, 0, sizeof(float) * numsamples);
        return;
    }
    if (numsamples > spectrumSize)
        numsamples = spectrumSize;
    float* buf = (readInputBuffer) ? ispec2 : ospec2;
    float scale = (float)((spectrumSize / 2) - 2) / (float)(numsamples - 1);
    for (int n = 0; n < numsamples; n++)
    {
        float f = n * scale;
        int i = FastFloor(f);
        buffer[n] = buf[i] + (buf[i + 1] - buf[i]) * (f - i);
    }
}

HistoryBuffer::HistoryBuffer()
    : length(0)
    , writeindex(0)
    , data(NULL)
{
}

HistoryBuffer::~HistoryBuffer()
{
    delete[] data;
}

void HistoryBuffer::Init(int _length)
{
    length = _length;
    data = new float[length];
    memset(data, 0, sizeof(float) * length);
}

void HistoryBuffer::ReadBuffer(float* buffer, int numsamplesTarget, int numsamplesSource, float offset)
{
    numsamplesTarget--; // reserve last sample for count of how much we were able to read
    float speed = (float)numsamplesSource / (float)numsamplesTarget;
    int n, w = writeindex; // since ReadBuffer is called from the GUI thread, writeindex may be modified by the DSP thread simultaneously
    float p = offset;
    for (n = 0; n < numsamplesTarget; n++)
    {
        float f = w - p;
        if (f < 0.0f)
            f += length;
        int i = FastFloor(f);
        float s1 = data[(i == 0) ? (length - 1) : (i - 1)];
        float s2 = data[i];
        buffer[numsamplesTarget - 1 - n] = s1 + (s2 - s1) * (f - i);
        p += speed;
        if (p >= length)
            break;
    }
    buffer[numsamplesTarget] = (float)n; // how many samples were written
}

Mutex::Mutex()
{
#if PLATFORM_WIN
#if PLATFORM_WINRT
    BOOL const result = InitializeCriticalSectionEx(&crit_sec, 0, CRITICAL_SECTION_NO_DEBUG_INFO);
    assert(FALSE != result);
#else
    InitializeCriticalSection(&crit_sec);
#endif
#else
    pthread_mutexattr_t attr;
    pthread_mutexattr_init(&attr);
    pthread_mutexattr_settype(&attr, PTHREAD_MUTEX_RECURSIVE);
    pthread_mutex_init(&mutex, &attr);
    pthread_mutexattr_destroy(&attr);
#endif
}

Mutex::~Mutex()
{
#if PLATFORM_WIN
    DeleteCriticalSection(&crit_sec);
#else
    pthread_mutex_destroy(&mutex);
#endif
}

bool Mutex::TryLock()
{
#if PLATFORM_WIN
    return TryEnterCriticalSection(&crit_sec) != 0;
#else
    return pthread_mutex_trylock(&mutex) == 0;
#endif
}

void Mutex::Lock()
{
#if PLATFORM_WIN
    EnterCriticalSection(&crit_sec);
#else
    pthread_mutex_lock(&mutex);
#endif
}

void Mutex::Unlock()
{
#if PLATFORM_WIN
    LeaveCriticalSection(&crit_sec);
#else
    pthread_mutex_unlock(&mutex);
#endif
}

void RegisterParameter(
    UnityAudioEffectDefinition& definition,
    const char* name,
    const char* unit,
    float minval,
    float maxval,
    float defaultval,
    float displayscale,
    float displayexponent,
    int enumvalue,
    const char* description
    )
{
    assert(defaultval >= minval);
    assert(defaultval <= maxval);
    strcpy_s(definition.paramdefs[enumvalue].name, name);
    strcpy_s(definition.paramdefs[enumvalue].unit, unit);
    definition.paramdefs[enumvalue].description = (description != NULL) ? strnew(description) : (name != NULL) ? strnew(name) : NULL;
    definition.paramdefs[enumvalue].defaultval = defaultval;
    definition.paramdefs[enumvalue].displayscale = displayscale;
    definition.paramdefs[enumvalue].displayexponent = displayexponent;
    definition.paramdefs[enumvalue].min = minval;
    definition.paramdefs[enumvalue].max = maxval;
    if (enumvalue >= (int)definition.numparameters)
        definition.numparameters = enumvalue + 1;
}

// Helper function to fill default values from the effect definition into the params array -- called by Create callbacks
void InitParametersFromDefinitions(
    InternalEffectDefinitionRegistrationCallback registereffectdefcallback,
    float* params
    )
{
    UnityAudioEffectDefinition definition;
    memset(&definition, 0, sizeof(definition));
    registereffectdefcallback(definition);
    for (UInt32 n = 0; n < definition.numparameters; n++)
    {
        params[n] = definition.paramdefs[n].defaultval;
        delete[] definition.paramdefs[n].description;
    }
    delete[] definition.paramdefs; // assumes that definition.paramdefs was allocated by registereffectdefcallback or is NULL
}

void DeclareEffect(
    UnityAudioEffectDefinition& definition,
    const char* name,
    UnityAudioEffect_CreateCallback createcallback,
    UnityAudioEffect_ReleaseCallback releasecallback,
    UnityAudioEffect_ProcessCallback processcallback,
    UnityAudioEffect_SetFloatParameterCallback setfloatparametercallback,
    UnityAudioEffect_GetFloatParameterCallback getfloatparametercallback,
    UnityAudioEffect_GetFloatBufferCallback getfloatbuffercallback,
    InternalEffectDefinitionRegistrationCallback registereffectdefcallback
    )
{
    memset(&definition, 0, sizeof(definition));
    strcpy_s(definition.name, name);
    definition.structsize = sizeof(UnityAudioEffectDefinition);
    definition.paramstructsize = sizeof(UnityAudioParameterDefinition);
    definition.apiversion = UNITY_AUDIO_PLUGIN_API_VERSION;
    definition.pluginversion = 0x010000;
    definition.create = createcallback;
    definition.release = releasecallback;
    definition.process = processcallback;
    definition.setfloatparameter = setfloatparametercallback;
    definition.getfloatparameter = getfloatparametercallback;
    definition.getfloatbuffer = getfloatbuffercallback;
    registereffectdefcallback(definition);
}

} // namespace AudioPluginUtil

#define DECLARE_EFFECT(namestr, ns) \
    namespace ns \
    { \
    UNITY_AUDIODSP_RESULT UNITY_AUDIODSP_CALLBACK CreateCallback            (UnityAudioEffectState* state); \
    UNITY_AUDIODSP_RESULT UNITY_AUDIODSP_CALLBACK ReleaseCallback           (UnityAudioEffectState* state); \
    UNITY_AUDIODSP_RESULT UNITY_AUDIODSP_CALLBACK ProcessCallback           (UnityAudioEffectState* state, float* inbuffer, float* outbuffer, unsigned int length, int inchannels, int outchannels); \
    UNITY_AUDIODSP_RESULT UNITY_AUDIODSP_CALLBACK SetFloatParameterCallback (UnityAudioEffectState* state, int index, float value); \
    UNITY_AUDIODSP_RESULT UNITY_AUDIODSP_CALLBACK GetFloatParameterCallback (UnityAudioEffectState* state, int index, float* value, char *valuestr); \
    UNITY_AUDIODSP_RESULT UNITY_AUDIODSP_CALLBACK GetFloatBufferCallback    (UnityAudioEffectState* state, const char* name, float* buffer, int numsamples); \
    int InternalRegisterEffectDefinition(UnityAudioEffectDefinition& definition); \
    }
#include "PluginList.h"
#undef DECLARE_EFFECT

#define DECLARE_EFFECT(namestr, ns) \
AudioPluginUtil::DeclareEffect( \
definition[numeffects++], \
namestr, \
ns::CreateCallback, \
ns::ReleaseCallback, \
ns::ProcessCallback, \
ns::SetFloatParameterCallback, \
ns::GetFloatParameterCallback, \
ns::GetFloatBufferCallback, \
ns::InternalRegisterEffectDefinition);

extern "C" UNITY_AUDIODSP_EXPORT_API int AUDIO_CALLING_CONVENTION UnityGetAudioEffectDefinitions(UnityAudioEffectDefinition*** definitionptr)
{
    static UnityAudioEffectDefinition definition[256];
    static UnityAudioEffectDefinition* definitionp[256];
    static int numeffects = 0;
    if (numeffects == 0)
    {
        #include "PluginList.h"
    }
    for (int n = 0; n < numeffects; n++)
        definitionp[n] = &definition[n];
    *definitionptr = definitionp;
    return numeffects;
}

// Simplistic unit-test framework
#if ENABLE_TESTS
    #define NAP_TESTSUITE(name) \
        namespace testsuite_##name { inline const char* GetSuiteName() { return #name; } }\
        namespace testsuite_##name
    #define NAP_UNITTEST(name) \
        struct NAP_Test_##name { NAP_Test_##name(const char* testname); };\
        static NAP_Test_##name test_##name(#name);\
        NAP_Test_##name::NAP_Test_##name(const char* testname)
    #define NAP_CHECK(...) \
        do\
        {\
            if(!(__VA_ARGS__))\
            {\
                printf("%s(%d): Unit test '%s' failed for expression '%s'.\n", __FILE__, __LINE__, testname, #__VA_ARGS__);\
                assert(false && "Unit test in native audio plugin framework failed!");\
            }\
        } while(false)
#else
    #define NAP_TESTSUITE(name) namespace testsuite_##name
    #define NAP_UNITTEST(name) static void test_##name()
    #define NAP_CHECK(...) do {} while(false)
#endif

NAP_TESTSUITE(FFT)
{
    NAP_UNITTEST(Accuracy)
    {
        for (int test = 0; test < 2; test++)
        {
            bool highprecision = (test == 1);

            AudioPluginUtil::Random r;
            for (int b = 4; b <= 20; b++)
            {
                int num = 1 << b;

                AudioPluginUtil::UnityComplexNumber* test1 = new AudioPluginUtil::UnityComplexNumber[num];
                AudioPluginUtil::UnityComplexNumber* test2 = new AudioPluginUtil::UnityComplexNumber[num];

                for (int n = 0; n < num; n++)
                {
                    test1[n].re = r.GetFloat(-1.0f, 1.0f);
                    test1[n].im = r.GetFloat(-1.0f, 1.0f);
                    test2[n].re = test1[n].re;
                    test2[n].im = test1[n].im;
                }

                AudioPluginUtil::FFT::Forward(test2, num, highprecision);
                AudioPluginUtil::FFT::Backward(test2, num, highprecision);

                double errtol = (highprecision) ? 1.0e-6 : 1.5e-3;
                double maxerr = 0.0f, errsum = 0.0, rms = 0.0;
                for (int n = 0; n < num; n++)
                {
                    float err, diff;
                    diff = test1[n].re - test2[n].re; err = fabsf(diff); NAP_CHECK(err < errtol); errsum += err; if (err > maxerr)
                        maxerr = err;
                    rms += diff * diff;
                    diff = test1[n].im - test2[n].im; err = fabsf(diff); NAP_CHECK(err < errtol); errsum += err; if (err > maxerr)
                        maxerr = err;
                    rms += diff * diff;
                }

                double avgerr = errsum / (double)num;
                rms = sqrt(rms / (double)num);

                delete[] test1;
                delete[] test2;

                printf("%2d bits: MaxErr=%15.8g ErrSum=%15.8g AvgErr=%15.8g ErrRMS=%15.8g [%s precision]\n", b, maxerr, errsum, avgerr, rms, highprecision ? "high" : "low");
                NAP_CHECK(avgerr < errtol);
                NAP_CHECK(rms < errtol);
            }
        }
    }
}
