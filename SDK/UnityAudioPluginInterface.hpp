#ifndef AUDIO_PLUGIN_INTERFACE_HPP
#define AUDIO_PLUGIN_INTERFACE_HPP

#include <memory>
#include <optional>
#include <vector>

namespace audio::plugin
{

struct Flags
{
    enum
    {
        is_playing           = 0b0001, // Set when engine is in play/paused mode.
        is_paused            = 0b0010, // Set when engine is paused mode.
        is_muted             = 0b0100, // Set when the plugin is being muted.
        is_side_chain_target = 0b1000  // Does this effect need a side chain buffer and can it be targeted by a Send?
    };
    
    static constexpr bool is_inactive (uint32_t flags)
    {
        return !(flags & is_playing) || (flags & (is_muted | is_paused));
    }
};

struct Plugin
{
    virtual ~Plugin() {}
    
    virtual std::optional<float> get_float_parameter (int index) const = 0;
    
    virtual bool set_float_parameter (int index, float value) = 0;
    
    virtual void process
        (
            uint32_t flags,
            float * input_buffer,
            float * output_buffer,
            int input_channel_count,
            int output_channel_count,
            int per_channel_sample_count
        ) = 0;
    
    virtual bool get_float_buffer
        (char const * name, float * buffer, int sample_count) { return false; }
    
    virtual void set_position(int position) {}
    
    virtual void reset () {}
    
    virtual void load_buffer (int sample_count, float * buffer, int slot) {}
};

struct ParameterDescriptor
{
    char name [16], unit [16];
    float min_value, max_value, default_value;
    float display_scale, display_exp;
};

struct DspConfiguration
{
    float sample_rate;
    int max_sample_count;
};

struct EntryPoint
{
    char name [32];
    std::unique_ptr<Plugin> (* factory) (DspConfiguration);
    std::vector<ParameterDescriptor> (* parameters) ();
};

} // namespace

namespace audio::plugin::raw
{

struct RawEntryPoint;

template <plugin::EntryPoint const & entry_point> RawEntryPoint raw ();

} // namespace

namespace audio
{

extern std::vector<audio::plugin::raw::RawEntryPoint> const entry_points;

} // namespace

#endif /* AUDIO_PLUGIN_INTERFACE_HPP */
