#include "UnityAudioPluginInterface-Raw.hpp"

namespace attenuator { extern audio::plugin::EntryPoint const entry_point; };
namespace noise_generator { extern audio::plugin::EntryPoint const entry_point; };

namespace audio
{

extern std::vector<plugin::raw::RawEntryPoint> const entry_points
{
    plugin::raw::raw<attenuator::entry_point>(),
    plugin::raw::raw<noise_generator::entry_point>(),
};

} // namespace
