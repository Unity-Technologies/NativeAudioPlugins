#include "UnityAudioPluginInterface.hpp"
#include "UnityAudioPluginInterface-Raw.h"
#include "UnityAudioPluginInterface-Raw.hpp"

#include <atomic>
#include <cstring>

extern float audio::plugin::raw::impl::raw_effect_state_get_sample_rate
    (
        RawUnityAudioEffectState const * raw_effect_state
    )
{
    return static_cast<float>(raw_effect_state->samplerate);
}

extern int audio::plugin::raw::impl::raw_effect_state_get_max_buffer_sample_count
    (
        RawUnityAudioEffectState const * raw_effect_state
    )
{
    return static_cast<int>(raw_effect_state->dspbuffersize);
}

extern void audio::plugin::raw::impl::raw_effect_state_set_plugin
    (
        RawUnityAudioEffectState * raw_effect_state,
        std::unique_ptr<audio::plugin::Plugin> plugin
    )
{
    EffectData * effect_data = new audio::plugin::raw::EffectData;
    
    effect_data->plugin = std::move(plugin);
    
    raw_effect_state->effectdata = static_cast<void *>(effect_data);
}

static UNITY_AUDIODSP_RESULT UNITY_AUDIODSP_CALLBACK release_callback
    (
        UnityAudioEffectState * state
    )
{
    auto effect_data = state->GetEffectData<audio::plugin::raw::EffectData>();
    
    effect_data->plugin = nullptr;
    delete effect_data;
    
    return UNITY_AUDIODSP_OK;
}

static UNITY_AUDIODSP_RESULT UNITY_AUDIODSP_CALLBACK set_float_parameter_callback
    (
        UnityAudioEffectState * state,
        int index,
        float value
    )
{
    auto effect_data = state->GetEffectData<audio::plugin::raw::EffectData>();
    
    bool const did_set = effect_data->plugin->set_float_parameter(index, value);
    
    return did_set ? UNITY_AUDIODSP_OK : UNITY_AUDIODSP_ERR_UNSUPPORTED;
}

static UNITY_AUDIODSP_RESULT UNITY_AUDIODSP_CALLBACK get_float_parameter_callback
    (
        UnityAudioEffectState * state,
        int index,
        float * value,
        char * valuestr
    )
{
    auto effect_data = state->GetEffectData<audio::plugin::raw::EffectData>();
    
    auto optional_value = effect_data->plugin->get_float_parameter(index);
    
    if (optional_value) { *value = *optional_value; }
    
    if (valuestr != nullptr) { valuestr[0] = 0; }
    
    return optional_value ? UNITY_AUDIODSP_OK : UNITY_AUDIODSP_ERR_UNSUPPORTED;
}

static UNITY_AUDIODSP_RESULT UNITY_AUDIODSP_CALLBACK get_float_buffer_callback
    (
        UnityAudioEffectState * state,
        char const * name,
        float * buffer,
        int numsamples
    )
{
    auto effect_data = state->GetEffectData<audio::plugin::raw::EffectData>();
    
    bool const did_write =
        effect_data->plugin->get_float_buffer(name, buffer, numsamples);
    
    return did_write ? UNITY_AUDIODSP_OK : UNITY_AUDIODSP_ERR_UNSUPPORTED;;
}

static UNITY_AUDIODSP_RESULT UNITY_AUDIODSP_CALLBACK process_callback
    (
        UnityAudioEffectState * state,
        float * inbuffer,
        float * outbuffer,
        unsigned int length,
        int inchannels,
        int outchannels
    )
{
    auto effect_data = state->GetEffectData<audio::plugin::raw::EffectData>();
    
    effect_data->plugin->process
    (
        state->flags, inbuffer, outbuffer, inchannels, outchannels, length
    );
    
    return UNITY_AUDIODSP_OK;
}

static UNITY_AUDIODSP_RESULT UNITY_AUDIODSP_CALLBACK reset_callback
    (
        UnityAudioEffectState * state
    )
{
    auto effect_data = state->GetEffectData<audio::plugin::raw::EffectData>();
    
    effect_data->plugin->reset();
    
    return UNITY_AUDIODSP_OK;
}

static UNITY_AUDIODSP_RESULT UNITY_AUDIODSP_CALLBACK set_position_callback
    (
        UnityAudioEffectState * state,
        unsigned int position
    )
{
    auto effect_data = state->GetEffectData<audio::plugin::raw::EffectData>();
    
    effect_data->plugin->set_position(static_cast<int>(position));
    
    return UNITY_AUDIODSP_OK;
}

void declare_effect
    (
        UnityAudioEffectDefinition & definition,
        audio::plugin::raw::RawEntryPoint entry_point
    )
{
    memset(&definition, 0, sizeof(definition));
    memcpy(definition.name, entry_point.name, sizeof(definition.name));
    definition.structsize = sizeof(UnityAudioEffectDefinition);
    definition.paramstructsize = sizeof(UnityAudioParameterDefinition);
    definition.apiversion = UNITY_AUDIO_PLUGIN_API_VERSION;
    definition.pluginversion = 0x010000;
    definition.create = entry_point.raw_plugin_factory;
    definition.release = release_callback;
    definition.process = process_callback;
    definition.reset = reset_callback;
    definition.setfloatparameter = set_float_parameter_callback;
    definition.getfloatparameter = get_float_parameter_callback;
    definition.getfloatbuffer = get_float_buffer_callback;
    definition.setposition = set_position_callback;
    
    auto parameters = entry_point.parameters();
    
    definition.numparameters = static_cast<UInt32>(parameters.size());
    definition.paramdefs =
        new UnityAudioParameterDefinition [definition.numparameters];
    
    for (auto & parameter_dsc : parameters)
    {
        auto index = &parameter_dsc - &parameters[0];
        
        memcpy(
            definition.paramdefs[index].name,
            parameter_dsc.name,
            sizeof(UnityAudioParameterDefinition::name)
        );
        
        memcpy(
            definition.paramdefs[index].unit,
            parameter_dsc.unit,
            sizeof(UnityAudioParameterDefinition::unit)
        );
        
        definition.paramdefs[index].description = "";
        definition.paramdefs[index].defaultval = parameter_dsc.default_value;
        definition.paramdefs[index].displayscale = parameter_dsc.display_scale;
        definition.paramdefs[index].displayexponent = parameter_dsc.display_exp;
        definition.paramdefs[index].min = parameter_dsc.min_value;
        definition.paramdefs[index].max = parameter_dsc.max_value;
    }
}

extern "C" UNITY_AUDIODSP_EXPORT_API int AUDIO_CALLING_CONVENTION
    UnityGetAudioEffectDefinitions
    (
        UnityAudioEffectDefinition * * * output
    )
{
    static std::atomic_flag is_initialised {};
    static UnityAudioEffectDefinition definitions [256];
    static UnityAudioEffectDefinition * definition_pointers [256];
    
    static const size_t entry_points_count = audio::entry_points.size();
    
    if (!is_initialised.test_and_set())
    {
        for (size_t index = 0; index < entry_points_count; index++)
        {
            declare_effect(definitions[index], audio::entry_points[index]);
            definition_pointers[index] = &definitions[index];
        }
    }
    
    *output = definition_pointers;
    
    return static_cast<int>(entry_points_count);
}
