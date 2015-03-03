using UnityEngine;
using System.Collections;
using System.Runtime.InteropServices;

public class ConvolutionReverbUploadIR : MonoBehaviour
{
    [DllImport("AudioPluginDemo")]
    private static extern bool ConvolutionReverb_UploadSample(int index, float[] data, int numsamples, int numchannels, int samplerate, [MarshalAs(UnmanagedType.LPStr)] string name);

    public AudioClip impulse;
    public int index;

    void Start()
    {
        if (impulse == null)
            return;
        float[] data = new float[impulse.samples];
        impulse.GetData(data, 0);
        ConvolutionReverb_UploadSample(index, data, data.Length / impulse.channels, impulse.channels, impulse.frequency, impulse.name);
    }

    void Update()
    {
    }
}
