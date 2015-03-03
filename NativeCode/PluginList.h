DECLARE_EFFECT("Demo Equalizer", Equalizer)
DECLARE_EFFECT("Demo LevelMixer", LevelMixer)
DECLARE_EFFECT("Demo Lofinator", Lofinator)
DECLARE_EFFECT("Demo ModalFilter", ModalFilter)
DECLARE_EFFECT("Demo Multiband", Multiband)
DECLARE_EFFECT("Demo NoiseBox", NoiseBox)
DECLARE_EFFECT("Demo PitchDetector", PitchDetector)
DECLARE_EFFECT("Demo RingModulator", RingModulator)
DECLARE_EFFECT("Demo StereoWidener", StereoWidener)
DECLARE_EFFECT("Demo TeeBee3o3", TeeBee)
DECLARE_EFFECT("Demo TeeDee9o9", TeeDee)
DECLARE_EFFECT("Demo Vocoder", Vocoder)

#if !UNITY_PS3
DECLARE_EFFECT("Demo ConvolutionReverb", ConvolutionReverb)
DECLARE_EFFECT("Demo CorrelationMeter", CorrelationMeter)
DECLARE_EFFECT("Demo Granulator", Granulator)
DECLARE_EFFECT("Demo ImpactGenerator", ImpactGenerator)
DECLARE_EFFECT("Demo LoudnessMeter", LoudnessMeter)
DECLARE_EFFECT("Demo Routing", Routing)
#endif

#if UNITY_OSX | UNITY_LINUX | UNITY_WIN
DECLARE_EFFECT("Demo Teleport", Teleport)
#endif
