#include "AudioPluginUtil.h"
#include <stdarg.h>

char* strnew(const char* src)
{
    char* newstr = new char[strlen(src) + 1];
    strcpy(newstr, src);
    return newstr;
}

char* tmpstr(int index, const char* fmtstr, ...)
{
    static char buf[4][1024];
    va_list args;
    va_start(args, fmtstr);
    vsprintf(buf[index], fmtstr, args);
    va_end(args);
    return buf[index];
}

template<typename T> void UnitySwap(T& a, T& b) { T t = a; a = b; b = t; }

static void FFTProcess(UnityComplexNumber* data, int numsamples, bool forward)
{
    int j = 0;
    for (int i = 0; i < numsamples - 1; i++)
    {
        if (i < j)
        {
            UnitySwap(data[i].re, data[j].re);
            UnitySwap(data[i].im, data[j].im);
        }
        int m = numsamples >> 1;
        j ^= m;
        while ((j & m) == 0)
        {
            m >>= 1;
            j ^= m;
        }
    }
    const float k = (forward) ? -kPI : kPI;
    for (int j = 1; j < numsamples; j <<= 1)
    {
        const float w0 = k / (float)j;
        UnityComplexNumber wr; wr.Set(cosf(w0), sinf(w0));
        UnityComplexNumber w; w.Set(1.0f, 0.0f);
        for (int m = 0; m < j; m++)
        {
            for (int i = m; i < numsamples; i += j << 1)
            {
                UnityComplexNumber t; UnityComplexNumber::Mul(w, data[i + j], t);
                UnityComplexNumber::Sub(data[i], t, data[i + j]);
                UnityComplexNumber::Add(data[i], t, data[i]);
            }
            UnityComplexNumber::Mul(w, wr, w);
        }
    }
}

void FFT::Forward(UnityComplexNumber* data, int numsamples)
{
    FFTProcess(data, numsamples, true);
}

void FFT::Backward(UnityComplexNumber* data, int numsamples)
{
    for (int n = 0; n < numsamples; n++)
        data[n].im = -data[n].im;
    FFTProcess(data, numsamples, false);
    const float scale = 1.0f / (float)numsamples;
    for (int n = 0; n < numsamples; n++)
    {
        data[n].re =  scale * data[n].re;
        data[n].im = -scale * data[n].im;
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
    Forward(cspec, spectrumSize);
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
    Forward(cspec, spectrumSize);
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
#if UNITY_WIN
#if UNITY_WINRT
    BOOL const result = InitializeCriticalSectionEx(&crit_sec, 0, CRITICAL_SECTION_NO_DEBUG_INFO);
    assert(FALSE != result);
#else
    InitializeCriticalSection(&crit_sec);
#endif
#else
# if !UNITY_SPU
    pthread_mutexattr_t attr;
    pthread_mutexattr_init(&attr);
    pthread_mutexattr_settype(&attr, PTHREAD_MUTEX_RECURSIVE);
    pthread_mutex_init(&mutex, &attr);
    pthread_mutexattr_destroy(&attr);
# endif
#endif
}

Mutex::~Mutex()
{
#if UNITY_WIN
    DeleteCriticalSection(&crit_sec);
#else
# if !UNITY_SPU
    pthread_mutex_destroy(&mutex);
# endif
#endif
}

bool Mutex::TryLock()
{
#if UNITY_WIN
    return TryEnterCriticalSection(&crit_sec) != 0;
#else
# if !UNITY_SPU
    return pthread_mutex_trylock(&mutex) == 0;
# endif
#endif
}

void Mutex::Lock()
{
#if UNITY_WIN
    EnterCriticalSection(&crit_sec);
#else
# if !UNITY_SPU
    pthread_mutex_lock(&mutex);
# endif
#endif
}

void Mutex::Unlock()
{
#if UNITY_WIN
    LeaveCriticalSection(&crit_sec);
#else
# if !UNITY_SPU
    pthread_mutex_unlock(&mutex);
# endif
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

#if UNITY_PS3
    #define DECLARE_EFFECT(namestr,ns) \
    extern char _binary_spu_ ## ns ## _spu_elf_start[];
    #include "PluginList.h"
    #undef DECLARE_EFFECT
#endif

#define DECLARE_EFFECT(namestr,ns) \
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

#if UNITY_PS3
    #define DECLARE_EFFECT(namestr,ns) \
    DeclareEffect( \
    definition[numeffects++], \
    namestr, \
    ns::CreateCallback, \
    ns::ReleaseCallback, \
    (UnityAudioEffect_ProcessCallback)_binary_spu_ ## ns ## _spu_elf_start, \
    ns::SetFloatParameterCallback, \
    ns::GetFloatParameterCallback, \
    ns::GetFloatBufferCallback, \
    ns::InternalRegisterEffectDefinition);
#else
    #define DECLARE_EFFECT(namestr,ns) \
    DeclareEffect( \
    definition[numeffects++], \
    namestr, \
    ns::CreateCallback, \
    ns::ReleaseCallback, \
    ns::ProcessCallback, \
    ns::SetFloatParameterCallback, \
    ns::GetFloatParameterCallback, \
    ns::GetFloatBufferCallback, \
    ns::InternalRegisterEffectDefinition);
#endif

extern "C" UNITY_AUDIODSP_EXPORT_API int UnityGetAudioEffectDefinitions(UnityAudioEffectDefinition*** definitionptr)
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
