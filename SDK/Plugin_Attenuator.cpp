#include "UnityAudioPluginInterface.hpp"

#include <cmath>
#include <vector>

namespace attenuator
{

namespace dsp
{

static float db_to_ratio(float dB)
{
    return powf(10.0f, dB / 20.0f);
}

} // namespace

struct Lpf
{
    Lpf (float sample_rate, float cutoff_frequency)
    {
        float const T = 1.0f / sample_rate;
        float const G = 1.0f / (1.0f + T * 0.5f * cutoff_frequency);
        
        a = T * 0.5f * cutoff_frequency * G;
        b = G;
    }
    
    float process (float x)
    {
        auto const y = a * x + b * z;
        z = (y + y) - z;
        return y;
    }
    
    void reset ()
    {
        z = 0.0f;
    }
    
  private:
    
    float prewarp(float w, float T)
    {
        return (2.0f / T) * tan((T / 2.0) * w);
    }
    
    float a, b, c, d, z {};
};

struct Parameters
{
    struct Index { enum { gain }; };
    
    static audio::plugin::ParameterDescriptor constexpr descriptors [] =
    {
        audio::plugin::ParameterDescriptor
        {
            .name = "Gain", .unit = "dB",
            .min_value = -72.0f, .max_value = +24.0f,
            .default_value = 0.0f,
            .display_scale = 1.0f, .display_exp = 1.0f
        }
    };
    
    float gain { descriptors[Index::gain].default_value };
};

struct Plugin : audio::plugin::Plugin
{
    static constexpr float smoother_lpf_cutoff = 10.0f;
    
    Parameters parameters;
    Lpf smoother;
    
    Plugin (audio::plugin::DspConfiguration configuration)
        : smoother { configuration.sample_rate, smoother_lpf_cutoff } {}
    
    std::optional<float> get_float_parameter (int index) const override
    {
        switch (index)
        {
            case Parameters::Index::gain: return { parameters.gain };
        }
        
        return {};
    }
    
    bool set_float_parameter (int index, float value) override
    {
        switch (index)
        {
            case Parameters::Index::gain: parameters.gain = value; return true;
        }
        
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
        for (int n = 0; n < channel_sample_count; n++)
        {
            for (int channel = 0; channel < output_channel_count; channel++)
            {
                auto x = input_buffer[n * output_channel_count + channel];
                auto y = x * dsp::db_to_ratio(smoother.process(parameters.gain));
                output_buffer[n * output_channel_count + channel] = y;
            }
        }
    }
    
    void reset () override
    {
        smoother.reset();
    }
};

extern audio::plugin::EntryPoint const entry_point
{
    .name = "Demo Attenuator",
    
    .factory = [](audio::plugin::DspConfiguration configuration)
        -> std::unique_ptr<audio::plugin::Plugin>
    {
        return std::make_unique<Plugin>(configuration);
    },
    
    .parameters = []()
    {
        return std::vector<audio::plugin::ParameterDescriptor>
            (Parameters::descriptors, std::end(Parameters::descriptors));
    }
};

} // namespace
