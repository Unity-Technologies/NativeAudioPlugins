using UnityEditor;
using UnityEngine;

public class OscilloscopeCustomGUI : IAudioEffectPluginGUI
{
    public override string Name
    {
        get { return "Demo Oscilloscope"; }
    }

    public override string Description
    {
        get { return "Oscilloscope demo plugin for Unity's audio plugin system"; }
    }

    public override string Vendor
    {
        get { return "Unity"; }
    }

    Vector3[] points = new Vector3[65536];

    public bool DrawControl(IAudioEffectPlugin plugin, Rect r, float samplerate)
    {
        r = AudioCurveRendering.BeginCurveFrame(r);

        if (Event.current.type == EventType.Repaint)
        {
            float blend = plugin.IsPluginEditableAndEnabled() ? 1.0f : 0.5f;

            float window, scale;
            plugin.GetFloatParameter("Window", out window);
            plugin.GetFloatParameter("Scale", out scale);
            window *= samplerate;
            if (window > samplerate)
                window = samplerate;

            float[] buffer;
            int numsamples = (int)window;
            plugin.GetFloatBuffer("Signal", out buffer, numsamples);
            numsamples = buffer.Length;

            float cy = r.y + r.height * 0.5f;
            float sx = (float)r.width / (float)numsamples;
            float sy = r.height * 0.5f * scale;
            for (int n = 0; n < numsamples; n++)
                points[n] = new Vector3(r.x + n * sx, cy - buffer[n] * sy, 0.0f);

            float lineTint = 0.5f;
            Handles.color = new Color(lineTint, lineTint, lineTint, 0.75f * blend);

            HandleUtilityWrapper.handleWireMaterial.SetPass(0);
            Handles.DrawAAPolyLine(2.0f, numsamples, points);
        }
        AudioCurveRendering.EndCurveFrame();
        return false;
    }

    public override bool OnGUI(IAudioEffectPlugin plugin)
    {
        float active, window, scale;
        plugin.GetFloatParameter("Active", out active);
        plugin.GetFloatParameter("Window", out window);
        plugin.GetFloatParameter("Scale", out scale);
        GUILayout.Space(5.0f);
        Rect r = GUILayoutUtility.GetRect(200, 80, GUILayout.ExpandWidth(true));

        if (DrawControl(plugin, r, plugin.GetSampleRate()))
        {
            plugin.SetFloatParameter("Window", window);
            plugin.SetFloatParameter("Scale", scale);
        }
        GUILayout.Space(5.0f);
        return true;
    }
}
