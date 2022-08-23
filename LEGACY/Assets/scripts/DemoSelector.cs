using UnityEngine;
using UnityEngine.SceneManagement;
using System.Collections;

public class DemoSelector : MonoBehaviour
{
    string[] demos =
    {
        "--- Procedural ---",
        "fire", "Fire",
        "glottal", "Vowel synthesis",
        "weather", "Weather sounds",

        "--- Granular (re)synthesis ---",
        "engine", "Car engine",
        "granulator fractal demo", "3D fractal demo",
        "granulator live demo", "Water drops",
        "granulator pitch shifting", "Pitch shifter effect",

        "--- Modal synthesis ---",
        "physics1", "Impacts and friction",
        "physics2", "Lots of impacts",
        "swords", "Sliding and hit sword",

        "--- Reverbs and spatialization ---",
        "convolution reverb", "Colvolution reverb",
        "velvetreverb", "Velvet reverb",
        "spatialization", "HRTF spatializer (5.2)",

        "--- Voice processing and effects ---",
        "pitch detection", "Pitch detection",
        "vocoder", "Vocoder",
        "voicefx", "Walkie-talkie",
        "wahwah", "Wah-Wah",

        "--- Synthesizers ---",
        "synthesizers", "303 and 909 like synths",
        "midisynthesizer", "MIDI-controlled synth",

        "--- Monitoring ---",
        "metering", "Metering and monitoring",

        "--- Signal routing ---",
        "speakerrouting", "Internal routing",
        "teleport", "Inter-process routing",
    };

    void OnGUI()
    {
        GUILayout.BeginHorizontal();
        int n = 0, i = 0;
        while (i < demos.Length)
        {
            bool header = demos[i][0] == '-';
            if (header || n++ == 4)
            {
                GUILayout.EndHorizontal();
                if (header)
                    GUILayout.Label(demos[i++]);
                GUILayout.BeginHorizontal();
                n = 0;
            }
            if (!header)
            {
                if (GUILayout.Button(demos[i + 1], GUILayout.Width(155)))
                    SceneManager.LoadScene(demos[i]);
                i += 2;
            }
        }
        GUILayout.EndHorizontal();
    }
}
