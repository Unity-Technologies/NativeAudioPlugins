using UnityEngine;
using System.Collections;

[RequireComponent(typeof(AudioSource))]
public class Clicker : MonoBehaviour
{
    public float interval = 5.0f;
    public float amp = 1.0f;
    public float decay = 0.0f;

    private int sr = 0;
    private int counter = 0;
    private float env = 0.0f;

    void Start()
    {
        sr = AudioSettings.outputSampleRate;
    }

    // Update is called once per frame
    void Update()
    {
    }

    void OnAudioFilterRead(float[] data, int channels)
    {
        if (sr == 0)
            return;

        int k = (int)(interval * sr);
        for (int n = 0; n < data.Length; n += channels)
        {
            env = (counter == 0) ? amp : 0.0f;
            if (++counter >= k)
                counter = 0;
            for (int i = 0; i < channels; i++)
                data[n + i] = env;
            env = env * decay + 1.0e-9f;
        }
    }
}
