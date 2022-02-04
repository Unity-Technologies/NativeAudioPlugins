#include "AudioPluginUtil.h"

namespace ImpactGenerator
{
    const int MAXIMPACTS = 1000;
    const int MAXINSTANCES = 4;

    enum Param
    {
        P_INSTANCE,
        P_GAIN,
        P_THR,
        P_MAXIMPACTS,
        P_NUM
    };

    struct Impact
    {
        float volume;
        float decay;
        float lpf;
        float bpf;
        float cut;
        float bw;
        inline float Process(AudioPluginUtil::Random& random)
        {
            volume *= decay;
            lpf += cut * bpf;
            bpf += cut * (random.GetFloat(-1.0f, 1.0f) - lpf - bpf * bw);
            return bpf * volume;
        }
    };

    struct EffectData
    {
        float p[P_NUM];
    };

    struct ImpactInstance
    {
        AudioPluginUtil::Random random;
        int maximpacts;
        int numimpacts;
        AudioPluginUtil::RingBuffer<MAXIMPACTS, Impact> impactrb;
        Impact impacts[MAXIMPACTS];
    };

    inline ImpactInstance* GetImpactInstance(int index)
    {
        static bool initialized[MAXINSTANCES] = { false };
        static ImpactInstance instance[MAXINSTANCES];
        if (index < 0 || index >= MAXINSTANCES)
            return NULL;
        if (!initialized[index])
        {
            initialized[index] = true;
            instance[index].maximpacts = MAXIMPACTS;
            instance[index].numimpacts = 0;
        }
        return &instance[index];
    }

    int InternalRegisterEffectDefinition(UnityAudioEffectDefinition& definition)
    {
        int numparams = P_NUM;
        definition.paramdefs = new UnityAudioParameterDefinition[numparams];
        AudioPluginUtil::RegisterParameter(definition, "Instance", "", 0.0f, (float)(MAXINSTANCES - 1), 0.0f, 1.0f, 1.0f, P_INSTANCE, "Determines the instance from which impacts are received via ImpactGenerator_AddImpact");
        AudioPluginUtil::RegisterParameter(definition, "Gain", "dB", -120.0f, 50.0f, 0.0f, 1.0f, 1.0f, P_GAIN, "Overall gain");
        AudioPluginUtil::RegisterParameter(definition, "Threshold", "dB", -120.0f, 0.0f, -60.0f, 1.0f, 1.0f, P_THR, "Threshold after which impacts stop playing");
        AudioPluginUtil::RegisterParameter(definition, "MaxImpacts", "", 1.0f, (float)MAXIMPACTS, 200.0f, 1.0f, 1.0f, P_MAXIMPACTS, "Maximum number of impact sounds played simultaneously on this instance");
        return numparams;
    }

    UNITY_AUDIODSP_RESULT UNITY_AUDIODSP_CALLBACK CreateCallback(UnityAudioEffectState* state)
    {
        EffectData* effectdata = new EffectData;
        memset(effectdata, 0, sizeof(EffectData));
        state->effectdata = effectdata;
        AudioPluginUtil::InitParametersFromDefinitions(InternalRegisterEffectDefinition, effectdata->p);
        return UNITY_AUDIODSP_OK;
    }

    UNITY_AUDIODSP_RESULT UNITY_AUDIODSP_CALLBACK ReleaseCallback(UnityAudioEffectState* state)
    {
        EffectData* data = state->GetEffectData<EffectData>();
        delete data;
        return UNITY_AUDIODSP_OK;
    }

    UNITY_AUDIODSP_RESULT UNITY_AUDIODSP_CALLBACK SetFloatParameterCallback(UnityAudioEffectState* state, int index, float value)
    {
        EffectData* data = state->GetEffectData<EffectData>();
        if (index >= P_NUM)
            return UNITY_AUDIODSP_ERR_UNSUPPORTED;
        data->p[index] = value;
        if (index == P_MAXIMPACTS)
        {
            ImpactInstance* instance = GetImpactInstance((int)data->p[P_INSTANCE]);
            if (instance != NULL)
                instance->maximpacts = (int)value;
        }
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
        return UNITY_AUDIODSP_OK;
    }

    UNITY_AUDIODSP_RESULT UNITY_AUDIODSP_CALLBACK ProcessCallback(UnityAudioEffectState* state, float* inbuffer, float* outbuffer, unsigned int length, int inchannels, int outchannels)
    {
        EffectData* data = state->GetEffectData<EffectData>();

        memset(outbuffer, 0, sizeof(float) * length * outchannels);

        ImpactInstance* instance = GetImpactInstance((int)data->p[P_INSTANCE]);
        if (instance == NULL)
            return UNITY_AUDIODSP_OK;

        float gain = powf(10.0f, data->p[P_GAIN] * 0.05f);
        float thr = powf(10.0f, data->p[P_THR] * 0.05f);

        Impact imp;
        while (instance->impactrb.Read(imp))
            if (instance->numimpacts < instance->maximpacts)
                instance->impacts[instance->numimpacts++] = imp;

        int i = 0;
        while (i < instance->numimpacts)
        {
            Impact& imp = instance->impacts[i];
            for (unsigned int n = 0; n < length; n++)
            {
                float impact = imp.Process(instance->random) * gain;
                for (int c = 0; c < inchannels; c++)
                    outbuffer[n * inchannels + c] += impact;
            }
            if (imp.volume < thr)
                imp = instance->impacts[--instance->numimpacts];
            else
                ++i;
        }

        return UNITY_AUDIODSP_OK;
    }

    extern "C" UNITY_AUDIODSP_EXPORT_API void ImpactGenerator_AddImpact(int index, float volume, float decay, float cut, float bw)
    {
        ImpactInstance* instance = GetImpactInstance(index);
        if (instance == NULL)
            return;
        if (instance->numimpacts >= instance->maximpacts)
            return;
        Impact imp;
        imp.volume = volume;
        imp.decay = decay;
        imp.cut = cut;
        imp.bw = bw;
        imp.lpf = 0.0f;
        imp.bpf = 0.0f;
        instance->impactrb.Feed(imp);
    }
}
