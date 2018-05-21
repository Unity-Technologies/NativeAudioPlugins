// The spatialization API is only supported by the final Unity 5.2 version and newer.
// If you get script compile errors in this file, comment out the line below.
#define ENABLE_SPATIALIZER_API

using UnityEngine;
using System.Collections;

public class SpatializerUserParams : MonoBehaviour
{
    #if ENABLE_SPATIALIZER_API
    public bool EnableSpatialization = true;
    public float DistanceAttn = 1.0f;
    public float FixedVolume = 0.0f;
    public float CustomRolloff = 0.0f;
    #endif

    void Start()
    {
    }

    void Update()
    {
        var source = GetComponent<AudioSource>();
        #if ENABLE_SPATIALIZER_API
        source.SetSpatializerFloat(0, DistanceAttn);
        source.SetSpatializerFloat(1, FixedVolume);
        source.SetSpatializerFloat(2, CustomRolloff);
        source.GetSpatializerFloat(0, out DistanceAttn); // Get back clipped parameters from plugin
        source.GetSpatializerFloat(1, out FixedVolume);
        source.GetSpatializerFloat(2, out CustomRolloff);
        source.spatialize = EnableSpatialization;
        #endif
    }
}
