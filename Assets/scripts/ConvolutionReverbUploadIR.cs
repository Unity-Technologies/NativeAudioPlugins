using UnityEngine;
using System.Collections;
using System.Runtime.InteropServices;

public class ConvolutionReverbUploadIR : MonoBehaviour
{
    [DllImport("AudioPluginDemo")]
    private static extern bool ConvolutionReverb_UploadSample(int index, float[] data, int numsamples, int numchannels, int samplerate, [MarshalAs(UnmanagedType.LPStr)] string name);

    public AudioClip[] impulse;
    public int index;

    void Start()
    {
        int currindex = index;
        foreach (var s in impulse)
        {
            if (s == null)
                return;
            float[] data = new float[s.samples];
            s.GetData(data, 0);
            ConvolutionReverb_UploadSample(currindex, data, data.Length / s.channels, s.channels, s.frequency, s.name);
            currindex++;
        }
    }

    void Update()
    {
    }
}
