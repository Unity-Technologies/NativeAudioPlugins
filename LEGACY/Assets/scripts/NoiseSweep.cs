using UnityEngine;
using UnityEngine.Audio;
using System.Collections;

[RequireComponent(typeof(AudioSource))]
public class NoiseSweep : MonoBehaviour
{
    public AudioMixer mixer;

    int samplerate;
    ulong seed;
    float lastPosX, lastPosY;
    float lpf, bpf, amp, amptarget;

    void Start()
    {
        var r = new System.Random();
        seed = (ulong)r.Next();
        samplerate = AudioSettings.outputSampleRate;
    }

    void Update()
    {
        if (Input.GetMouseButtonDown(0))
        {
            mixer.SetFloat("ModalSeed", seed & 65535);
            if (Input.mousePosition.x > Screen.width * 0.5f)
            {
                amp = 1.0f;
                amptarget = 0.0f;
            }
        }
        else
        {
            if (Input.GetMouseButton(0))
            {
                float dx = Input.mousePosition.x - lastPosX;
                float dy = Input.mousePosition.y - lastPosY;
                amptarget = 0.01f * Mathf.Sqrt(dx * dx + dy * dy);
            }
            else
            {
                amptarget = 0.0f;
            }
        }
        lastPosX = Input.mousePosition.x;
        lastPosY = Input.mousePosition.y;
    }

    float GetRandom()
    {
        const float scale = 1.0f / (float)0x7FFFFFFF;
        seed = seed * 69069 + 1;
        return (((seed >> 16) ^ seed) & 0x7FFFFFFF) * scale;
    }

    float GetRandom(float minVal, float maxVal)
    {
        return minVal + (maxVal - minVal) * GetRandom();
    }

    void OnAudioFilterRead(float[] data, int numchannels)
    {
        if (samplerate == 0)
            return;

        for (int n = 0; n < data.Length; n += numchannels)
        {
            amp += (amptarget - amp) * 0.0003f;
            float cut = Mathf.Clamp(amp, 0.001f, 0.7f), bw = 0.7f;
            float s = Mathf.Pow(GetRandom() * 2.0f - 1.0f, 15.0f);
            lpf += cut * bpf;
            bpf += cut * (s - lpf - bpf * bw);
            lpf += cut * bpf;
            bpf += cut * (s - lpf - bpf * bw);
            s = lpf * amp;
            for (int i = 0; i < numchannels; i++)
                data[n + i] += s;
        }
    }
}
