using UnityEngine;
using System.Collections;

[RequireComponent(typeof(AudioSource))]
public class SineOsc : MonoBehaviour
{
    public float freq = 440.0f;
    public float amp = 1.0f;

    private float phase = 0.0f;
    private float st = 0.0f;

    // Use this for initialization
    void Start()
    {
        st = 1.0f / AudioSettings.outputSampleRate;
    }

    // Update is called once per frame
    void Update()
    {
    }

    void OnAudioFilterRead(float[] data, int channels)
    {
        if (st == 0.0f)
            return;

        float f = freq * st;
        for (int n = 0; n < data.Length; n += channels)
        {
            float s = Mathf.Sin(phase * 2.0f * 3.1415926f) * amp;
            phase += f;
            phase -= Mathf.Floor(phase);
            for (int i = 0; i < channels; i++)
                data[n + i] = s;
        }
    }
}
