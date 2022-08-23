using UnityEngine;
using System.Collections;

[RequireComponent(typeof(AudioSource))]
public class SilentAudioSource : MonoBehaviour
{
    void OnAudioFilterRead(float[] data, int channels)
    {
    }
}
