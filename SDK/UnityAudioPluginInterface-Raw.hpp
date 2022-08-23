#ifndef AUDIO_PLUGIN_INTERFACE_RAW_HPP
#define AUDIO_PLUGIN_INTERFACE_RAW_HPP

#include "UnityAudioPluginInterface.hpp"

namespace audio::plugin::raw
{

struct RawEntryPoint;

template <plugin::EntryPoint const & entry_point> RawEntryPoint raw ();

} // namespace

#include "UnityAudioPluginInterface-Raw.inl"

namespace audio::plugin
{

extern std::vector<audio::plugin::raw::RawEntryPoint> const entry_points;

} // namespace

#endif /* AUDIO_PLUGIN_INTERFACE_RAW_HPP */
