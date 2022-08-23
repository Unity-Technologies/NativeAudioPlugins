#ifdef AUDIO_PLUGIN_INTERFACE_RAW_INL
#  error
#else
#  define AUDIO_PLUGIN_INTERFACE_RAW_INL
#endif

#include "UnityAudioPluginInterface-Raw.h"

typedef struct UnityAudioEffectState RawUnityAudioEffectState;

namespace audio::plugin::raw::impl
{

extern float raw_effect_state_get_sample_rate
    (
        RawUnityAudioEffectState const *
    );

extern int raw_effect_state_get_max_buffer_sample_count
    (
        RawUnityAudioEffectState const *
    );

extern void raw_effect_state_set_plugin
    (
        RawUnityAudioEffectState *,
        std::unique_ptr<plugin::Plugin>
    );

} // namespace

namespace audio::plugin::raw
{

struct RawEntryPoint
{
    char const * name;
    UnityAudioEffect_CreateCallback raw_plugin_factory;
    std::vector<plugin::ParameterDescriptor> (* parameters) ();
};

template <plugin::EntryPoint const & entry_point>
UNITY_AUDIODSP_RESULT UNITY_AUDIODSP_CALLBACK factory
    (
        RawUnityAudioEffectState * raw_effect_state
    )
{
    plugin::DspConfiguration const dsp_configuration
    {
        .sample_rate = impl::raw_effect_state_get_sample_rate(raw_effect_state),
        .max_sample_count = impl::raw_effect_state_get_max_buffer_sample_count(raw_effect_state)
    };
    
    auto plugin = entry_point.factory(dsp_configuration);
    
    impl::raw_effect_state_set_plugin(raw_effect_state, std::move(plugin));
    
    return static_cast<int>(UNITY_AUDIODSP_OK);
}

template <plugin::EntryPoint const & entry_point> RawEntryPoint raw
    (
    )
{
    return RawEntryPoint
    {
        .name = entry_point.name,
        .raw_plugin_factory = factory<entry_point>,
        .parameters = entry_point.parameters
    };
}

struct EffectData
{
    alignas(16) std::unique_ptr<plugin::Plugin> plugin;
};

} // namespace
