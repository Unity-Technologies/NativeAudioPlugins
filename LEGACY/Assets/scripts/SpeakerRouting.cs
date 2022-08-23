using UnityEngine;
using System.Collections;
using System.Runtime.InteropServices;

[RequireComponent(typeof(AudioSource))]
public class SpeakerRouting : MonoBehaviour
{
    [DllImport("AudioPluginDemo")]
    private static extern void RoutingDemo_GetData(int target, float[] data, int numsamples, int numchannels);

    public int target = 0;

    private bool ready = false;
    private float rms = 0.0f;

    // Use this for initialization
    void Start()
    {
        ready = true;
    }

    // Update is called once per frame
    void Update()
    {
        var r = GetComponent<Renderer>();
        r.material.color = new Color(1.0f, rms, 0.0f, 1.0f);
    }

    void OnAudioFilterRead(float[] data, int numchannels)
    {
        if (ready)
        {
            RoutingDemo_GetData(target, data, data.Length / numchannels, numchannels);

            float sum = 0.0f;
            for (int n = 0; n < data.Length; n++)
                sum += data[n] * data[n];
            rms = Mathf.Sqrt(sum / data.Length);
        }
    }
}
