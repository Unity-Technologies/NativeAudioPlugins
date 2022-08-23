using UnityEngine;
using System.Collections;
using System.Runtime.InteropServices;

public class GranulatorUploadSample : MonoBehaviour
{
    [DllImport("AudioPluginDemo")]
    private static extern bool Granulator_UploadSample(int index, float[] data, int numsamples, int numchannels, int samplerate, [MarshalAs(UnmanagedType.LPStr)] string name);

    public AudioClip[] sample;
    public int index;
    public float lowcut = 0.0f;
    public float highcut = 24000.0f;
    public int order = 3;

    private bool[] uploaded = new bool[64];
    private AudioClip[] currSample = new AudioClip[64];

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
        foreach (var s in sample)
        {
            if (currSample[currindex] != s)
                uploaded[currindex] = false;

            if (s != null && s.loadState == AudioDataLoadState.Loaded && !uploaded[currindex])
            {
                Debug.Log("Uploading sample " + s.name + " to slot " + currindex);

                int numsamples = s.samples;
                int numchannels = s.channels;
                float[] data = new float[numsamples * numchannels];
                s.GetData(data, 0);
                for (int c = 0; c < numchannels; c++)
                {
                    bool modified = false;
                    float sr = (float)s.frequency, bw = 0.707f;
                    for (int k = 0; k < order; k++)
                    {
                        if (lowcut > 0.0f)
                        {
                            float lpf = 0.0f, bpf = 0.0f, cutoff = 2.0f * Mathf.Sin(0.25f * Mathf.Min(lowcut / sr, 0.5f));
                            for (int n = 0; n < numsamples; n++)
                            {
                                lpf += bpf * cutoff;
                                float hpf = bw * data[n * numchannels + c] - lpf - bpf * bw;
                                bpf += hpf * cutoff;
                                lpf += bpf * cutoff;
                                hpf = bw * data[n * numchannels + c] - lpf - bpf * bw;
                                bpf += hpf * cutoff;
                                data[n * numchannels + c] = hpf;
                            }
                            modified = true;
                        }
                        if (highcut < sr * 0.5f)
                        {
                            float lpf = 0.0f, bpf = 0.0f, cutoff = 2.0f * Mathf.Sin(0.25f * Mathf.Min(highcut / sr, 0.5f));
                            for (int n = 0; n < numsamples; n++)
                            {
                                lpf += bpf * cutoff;
                                float hpf = bw * data[n * numchannels + c] - lpf - bpf * bw;
                                bpf += hpf * cutoff;
                                lpf += bpf * cutoff;
                                hpf = bw * data[n * numchannels + c] - lpf - bpf * bw;
                                bpf += hpf * cutoff;
                                data[n * numchannels + c] = lpf;
                            }
                            modified = true;
                        }
                        if (k == order - 1 && modified)
                        {
                            float peak = 0.0f;
                            for (int n = 0; n < numsamples; n++)
                            {
                                float a = Mathf.Abs(data[n * numchannels + c]);
                                if (a > peak)
                                    peak = a;
                            }
                            float scale = 1.0f / peak;
                            for (int n = 0; n < numsamples; n++)
                                data[n * numchannels + c] *= scale;
                        }
                    }
                }
                Granulator_UploadSample(currindex, data, numsamples, numchannels, s.frequency, s.name);
                uploaded[currindex] = true;
                currSample[currindex] = s;
            }

            currindex++;
        }
    }
}
