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

    void Start()
    {
		int currindex = index;
		foreach(var s in sample)
		{
	        if (s == null)
	            continue;
	        float[] data = new float[s.samples];
	        s.GetData(data, 0);
	        bool modified = false;
	        float sr = (float)AudioSettings.outputSampleRate, bw = 0.707f;
	        for (int k = 0; k < order; k++)
	        {
	            if (lowcut > 0.0f)
	            {
	                float lpf = 0.0f, bpf = 0.0f, cutoff = 2.0f * Mathf.Sin(0.25f * Mathf.Min(lowcut / sr, 0.5f));
	                for (int n = 0; n < data.Length; n++)
	                {
	                    lpf += bpf * cutoff;
	                    float hpf = bw * data[n] - lpf - bpf * bw;
	                    bpf += hpf * cutoff;
	                    lpf += bpf * cutoff;
	                    hpf = bw * data[n] - lpf - bpf * bw;
	                    bpf += hpf * cutoff;
	                    data[n] = hpf;
	                }
	                modified = true;
	            }
	            if (highcut < sr * 0.5f)
	            {
	                float lpf = 0.0f, bpf = 0.0f, cutoff = 2.0f * Mathf.Sin(0.25f * Mathf.Min(highcut / sr, 0.5f));
	                for (int n = 0; n < data.Length; n++)
	                {
	                    lpf += bpf * cutoff;
	                    float hpf = bw * data[n] - lpf - bpf * bw;
	                    bpf += hpf * cutoff;
	                    lpf += bpf * cutoff;
	                    hpf = bw * data[n] - lpf - bpf * bw;
	                    bpf += hpf * cutoff;
	                    data[n] = lpf;
	                }
	                modified = true;
	            }
	            if (k == order - 1 && modified)
	            {
	                float peak = 0.0f;
	                for (int n = 0; n < data.Length; n++)
	                {
	                    float a = Mathf.Abs(data[n]);
	                    if (a > peak)
	                        peak = a;
	                }
	                float scale = 1.0f / peak;
	                for (int n = 0; n < data.Length; n++)
	                    data[n] *= scale;
	            }
	        }
			Granulator_UploadSample(currindex++, data, data.Length / s.channels, s.channels, s.frequency, s.name);
		}
    }

    void Update()
    {
    }
}
