using UnityEngine;
using System.Collections;
using System.Runtime.InteropServices;

public class MidiSynthPhrase : MonoBehaviour
{
    [DllImport("AudioPluginDemo")]
    private static extern void Synthesizer_AddMessage(ulong sample, int msg);

    [DllImport("AudioPluginDemo")]
    private static extern void Synthesizer_KillAll();

    void Start()
    {
    }

    void Update()
    {
    }

    void OnGUI()
    {
        float bpm = 120.0f;
        int transpose = -24 * 256;
        double dt = 120.0f / (16 * bpm);
        double t0 = AudioSettings.dspTime + dt * 16;
        if (GUILayout.Button("Start"))
        {
            Synthesizer_AddMessage((ulong)(AudioSettings.outputSampleRate * t0), 0x7F4090 + transpose); t0 += dt;
            Synthesizer_AddMessage((ulong)(AudioSettings.outputSampleRate * t0), 0x7F4490 + transpose); t0 += dt;
            Synthesizer_AddMessage((ulong)(AudioSettings.outputSampleRate * t0), 0x7F4790 + transpose); t0 += dt;
            Synthesizer_AddMessage((ulong)(AudioSettings.outputSampleRate * t0), 0x7F4E90 + transpose);
        }
        if (GUILayout.Button("Stop"))
        {
            Synthesizer_AddMessage((ulong)(AudioSettings.outputSampleRate * t0), 0x4080 + transpose); t0 += dt;
            Synthesizer_AddMessage((ulong)(AudioSettings.outputSampleRate * t0), 0x4480 + transpose); t0 += dt;
            Synthesizer_AddMessage((ulong)(AudioSettings.outputSampleRate * t0), 0x4780 + transpose); t0 += dt;
            Synthesizer_AddMessage((ulong)(AudioSettings.outputSampleRate * t0), 0x4E80 + transpose);
        }
        if (GUILayout.Button("Arp"))
        {
            for (int n = 0; n < 64; n++)
            {
                int note = ((n * 17 + n * n * 3) % 24) + 36;
                Synthesizer_AddMessage((ulong)(AudioSettings.outputSampleRate * t0), 0x7F0090 + note * 0x100); t0 += dt;
                Synthesizer_AddMessage((ulong)(AudioSettings.outputSampleRate * t0), 0x80 + note * 0x100); t0 += dt;
            }
        }
        if (GUILayout.Button("Mute All Playing"))
        {
            for (int n = 0; n < 128; n++)
                Synthesizer_AddMessage((ulong)(AudioSettings.outputSampleRate * t0), 0x80 + n * 0x100);
        }
        if (GUILayout.Button("All Sound Off"))
        {
            Synthesizer_KillAll();
        }
    }
}
