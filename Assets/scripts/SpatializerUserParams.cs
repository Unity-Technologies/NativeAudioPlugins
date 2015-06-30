#if UNITY_5_2
// Spatialization API is only supported by Unity 5.2 and newer
#define ENABLE_SPATIALIZER_API
#endif

using UnityEngine;
using System.Collections;

public class SpatializerUserParams : MonoBehaviour
{
	#if ENABLE_SPATIALIZER_API
	public float DistanceAttn = 1.0f;
	public float FixedVolume = 0.0f;
	#endif

	void Start ()
	{
	}
	
	void Update ()
	{
		var source = GetComponent<AudioSource> ();
		#if ENABLE_SPATIALIZER_API
		source.SetSpatializerFloat (0, DistanceAttn);
		source.SetSpatializerFloat (1, FixedVolume);
		#endif
	}
}
