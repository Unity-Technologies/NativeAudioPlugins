using UnityEngine;
using System.Collections;

[RequireComponent(typeof(AudioSource))]
public class ModifyWave : MonoBehaviour
{
    public enum Mode
    {
        Off,
        DC,
        Noise
    };

    public Mode mode = Mode.DC;

    // Use this for initialization
    void Start()
    {
        if (mode == Mode.DC)
        {
            var clip = GetComponent<AudioSource>().clip;
            float[] data = new float[clip.samples * clip.channels];
            for (int n = 0; n < data.Length; n++)
                data[n] = 1.0f;
            clip.SetData(data, 0);
        }
        else if (mode == Mode.Noise)
        {
            System.Random r = new System.Random();
            var clip = GetComponent<AudioSource>().clip;
            float[] data = new float[clip.samples * clip.channels];
            for (int n = 0; n < data.Length; n++)
                data[n] = (float)r.NextDouble() * 2.0f - 1.0f;
            clip.SetData(data, 0);
        }
    }

    // Update is called once per frame
    void Update()
    {
    }
}
