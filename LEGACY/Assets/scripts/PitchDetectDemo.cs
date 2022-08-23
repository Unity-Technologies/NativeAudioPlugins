using UnityEngine;
using UnityEngine.Audio;
using System.Collections;
using System.Runtime.InteropServices;

public class PitchDetectDemo : MonoBehaviour
{
    [DllImport("AudioPluginDemo")]
    private static extern float PitchDetectorGetFreq(int index);

    [DllImport("AudioPluginDemo")]
    private static extern int PitchDetectorDebug(float[] data);

    float[] history = new float[1000];
    float[] debug = new float[65536];

    string[] noteNames = { "C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B" };

    public Material mat;
    public string frequency = "detected frequency";
    public string note = "detected note";
    public AudioMixer mixer;
    public InfoText pitchText;

    // Use this for initialization
    void Start()
    {
    }

    // Update is called once per frame
    void Update()
    {
        float freq = PitchDetectorGetFreq(0), deviation = 0.0f;
        frequency = freq.ToString() + " Hz";

        if (freq > 0.0f)
        {
            float noteval = 57.0f + 12.0f * Mathf.Log10(freq / 440.0f) / Mathf.Log10(2.0f);
            float f = Mathf.Floor(noteval + 0.5f);
            deviation = Mathf.Floor((noteval - f) * 100.0f);
            int noteIndex = (int)f % 12;
            int octave = (int)Mathf.Floor((noteval + 0.5f) / 12.0f);
            note = noteNames[noteIndex] + " " + octave;
        }
        else
        {
            note = "unknown";
        }

        if (pitchText != null)
            pitchText.text = "Detected frequency: " + frequency + "\nDetected note: " + note + " (deviation: " + deviation + " cents)";
    }

    Vector3 Plot(float[] data, int num, float x0, float y0, float w, float h, Color col, float thr)
    {
        GL.Begin(GL.LINES);
        GL.Color(col);
        float xs = w / num, ys = h;
        float px = 0, py = 0;
        for (int n = 1; n < num; n++)
        {
            float nx = x0 + n * xs, ny = y0 + data[n] * ys;
            if (n > 1 && data[n] > thr && data[n - 1] > thr)
            {
                GL.Vertex3(px, py, 0);
                GL.Vertex3(nx, ny, 0);
            }
            px = nx;
            py = ny;
        }
        GL.End();
        return new Vector3(x0 + w, py, 0);
    }

    void OnRenderObject()
    {
        mat.SetPass(0);

        GL.Begin(GL.LINES);
        GL.Color(Color.green);
        GL.Vertex3(-5, 0, 0);
        GL.Vertex3(5, 0, 0);
        GL.End();

        for (int n = 1; n < history.Length; n++)
            history[n - 1] = history[n];
        history[history.Length - 1] = PitchDetectorGetFreq(0);
        transform.position = Plot(history, history.Length, -45.0f, 0.0f, 50.0f, 0.01f, Color.white, 0.1f);

        int num = PitchDetectorDebug(debug);
        Plot(debug, num, -5.0f, 1.0f, 10.0f, 0.0002f, Color.red, 0.1f);
    }

    void OnGUI()
    {
        float monitor;
        if (mixer != null && mixer.GetFloat("Monitor", out monitor))
        {
            GUILayout.Space(30);
            if (GUILayout.Button(monitor > 0.0f ? "Monitoring is ON" : "Monitoring is OFF"))
                monitor = (monitor > 0.0f) ? 0.0f : 0.5f;
            mixer.SetFloat("Monitor", monitor);
        }
    }
}
