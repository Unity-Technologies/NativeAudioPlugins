using UnityEngine;
using System.Collections;
using System.Runtime.InteropServices;

public class ConvolutionReverbUploadIR : MonoBehaviour
{
    [DllImport("AudioPluginDemo")]
    private static extern bool ConvolutionReverb_UploadSample(int index, float[] data, int numsamples, int numchannels, int samplerate, [MarshalAs(UnmanagedType.LPStr)] string name);

    public AudioClip[] impulse = new AudioClip[0];
    public int index;

    private bool[] uploaded = new bool[64];
    private AudioClip[] currImpulse = new AudioClip[64];

    void Start()
    {
        UploadChangedClips();
    }

    void Update()
    {
        UploadChangedClips();
    }

    void UploadChangedClips()
    {
        int currindex = index;
        foreach (var s in impulse)
        {
            if (currImpulse[currindex] != s)
                uploaded[currindex] = false;

            if (s != null && s.loadState == AudioDataLoadState.Loaded && !uploaded[currindex])
            {
                Debug.Log("Uploading impulse response " + s.name + " to slot " + currindex);
                float[] data = new float[s.samples];
                s.GetData(data, 0);
                ConvolutionReverb_UploadSample(currindex, data, data.Length / s.channels, s.channels, s.frequency, s.name);
                uploaded[currindex] = true;
                currImpulse[currindex] = s;
            }

            currindex++;
        }
    }
}
