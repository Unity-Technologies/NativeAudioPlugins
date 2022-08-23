#include "UnityAudioPluginInterface.hpp"

#include <cstdint>
#include <vector>

namespace noise_generator {

struct NoiseSource
{
    float process ()
    {
        // Non-cryptographic integer hash function taken from
        // https://github.com/skeeto/hash-prospector
        
        uint32_t x = state;
        
        x ^= x >> 17;
        x *= 0xed5ad4bb;
        x ^= x >> 11;
        x *= 0xac4c1b51;
        x ^= x >> 15;
        x *= 0x31848bab;
        x ^= x >> 14;
        
        state += 1;
        
        uint32_t constexpr mask = 0xFFFFFF; // 24 bits of precision.
        
        return (float) (x & mask) * (1.0f / (float) mask);
    }
    
    private: uint32_t state {};
};

struct Plugin : audio::plugin::Plugin
{
    NoiseSource noise_source {};
    
    Plugin (audio::plugin::DspConfiguration) {}
    
    std::optional<float> get_float_parameter (int index) const override
    {
        return {};
    }
    
    bool set_float_parameter (int index, float value) override
    {
        return false;
    }
    
    void process
        (
            uint32_t flags,
            float * input_buffer, float * output_buffer,
            int input_channel_count, int output_channel_count,
            int channel_sample_count
        ) override
    {
        if (audio::plugin::Flags::is_inactive(flags))
        {
            memcpy(output_buffer, input_buffer,
                sizeof(float) * channel_sample_count * output_channel_count);
        }
        else
        {
            for (int n = 0; n < channel_sample_count; n++)
            {
                for (int channel = 0; channel < output_channel_count; channel++)
                {
                    auto y = noise_source.process();
                    
                    output_buffer[n * output_channel_count + channel] = y;
                }
            }
        }
    }
};

extern audio::plugin::EntryPoint const entry_point
{
    .name = "Demo Noise Generator",
    
    .factory = [](audio::plugin::DspConfiguration configuration)
        -> std::unique_ptr<audio::plugin::Plugin>
    {
        return std::make_unique<Plugin>(configuration);
    },
    
    .parameters = []()
    {
        return std::vector<audio::plugin::ParameterDescriptor>();
    }
};

} // namespace
