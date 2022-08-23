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

    const int scopeheight = 120;
    const int maxspeclen = 4096;
    Color[] spec = new Color[maxspeclen];
    Texture2D[] spectex = new Texture2D[8];
    int[] specpos = new int[8];

    public bool DrawControl(IAudioEffectPlugin plugin, Rect r, float samplerate, int channel)
    {
        r = AudioCurveRendering.BeginCurveFrame(r);

        if (Event.current.type == EventType.Repaint)
        {
            float blend = plugin.IsPluginEditableAndEnabled() ? 1.0f : 0.5f;

            float window, scale, mode;
            plugin.GetFloatParameter("Window", out window);
            plugin.GetFloatParameter("Scale", out scale);
            plugin.GetFloatParameter("Mode", out mode);

            float[] buffer;
            int numsamples = (mode >= 1.0f) ? maxspeclen : (int)(window * samplerate);
            plugin.GetFloatBuffer("Channel" + channel.ToString(), out buffer, numsamples);
            numsamples = buffer.Length;

            if (mode < 2.0f)
            {
                Color lineColor = new Color(1.0f, 0.5f, 0.2f, blend);
                if (mode >= 1.0f)
                {
                    scale *= 0.1f;
                    AudioCurveRendering.DrawFilledCurve(r, delegate(float x)
                        {
                            float f = Mathf.Clamp(x * (numsamples - 2) * window * 0.5f, 0, numsamples - 2);
                            int i = (int)Mathf.Floor(f);
                            float s1 = 20.0f * Mathf.Log10(buffer[i] + 0.0001f);
                            float s2 = 20.0f * Mathf.Log10(buffer[i + 1] + 0.0001f);
                            return (s1 + (s2 - s1) * (f - i)) * scale;
                        }, lineColor);
                    GUIHelpers.DrawFrequencyTickMarks(r, samplerate * window * 0.5f, false, Color.red);
                    GUIHelpers.DrawDbTickMarks(r, 1.0f / scale, scale, Color.red, new Color(1.0f, 0.0f, 0.0f, 0.25f));
                }
                else
                {
                    AudioCurveRendering.DrawCurve(r, delegate(float x) { return scale * buffer[(int)Mathf.Floor(x * (numsamples - 2))]; }, lineColor);
                    GUIHelpers.DrawTimeTickMarks(r, window, Color.red, new Color(1.0f, 0.0f, 0.0f, 0.25f));
                }
            }
            else
            {
                scale *= 0.1f;

                for (int i = 0; i < maxspeclen; i++)
                {
                    float v = 20.0f * Mathf.Log10(buffer[i] + 0.0001f) * scale;
                    spec[i] = new Color(
                            Mathf.Clamp(v * 4.0f - 1.0f, 0.0f, 1.0f),
                            Mathf.Clamp(v * 4.0f - 2.0f, 0.0f, 1.0f),
                            1.0f - Mathf.Clamp(Mathf.Abs(v * 4.0f - 1.0f), 0.0f, 1.0f) * Mathf.Clamp(4.0f - 4.0f * v, 0.0f, 1.0f),
                            1.0f);
                }

                if (spectex[channel] == null)
                    spectex[channel] = new Texture2D(maxspeclen, scopeheight);

                specpos[channel] = (specpos[channel] + 1) % scopeheight;
                spectex[channel].SetPixels(0, specpos[channel], maxspeclen, 1, spec);
                spectex[channel].Apply();

                Color oldColor = GUI.color;
                GUI.color = new Color(1.0f, 1.0f, 1.0f, blend);

                Rect r2 = new Rect(r.x, r.y + specpos[channel], r.width / (window * 0.5f), scopeheight);
                GUI.DrawTexture(r2, spectex[channel], ScaleMode.StretchToFill, false, 1.0f);

                r2.y -= scopeheight;
                GUI.DrawTexture(r2, spectex[channel], ScaleMode.StretchToFill, false, 1.0f);

                GUI.color = oldColor;

                GUIHelpers.DrawFrequencyTickMarks(r, samplerate * window * 0.5f, false, Color.red);
            }
        }
        AudioCurveRendering.EndCurveFrame();
        return false;
    }

    public override bool OnGUI(IAudioEffectPlugin plugin)
    {
        float active, window, scale, mode;
        plugin.GetFloatParameter("Active", out active);
        plugin.GetFloatParameter("Window", out window);
        plugin.GetFloatParameter("Scale", out scale);
        plugin.GetFloatParameter("Mode", out mode);
        GUILayout.Space(5.0f);

        DrawControl(plugin, GUILayoutUtility.GetRect(200, scopeheight, GUILayout.ExpandWidth(true)), plugin.GetSampleRate(), 0);
        GUILayout.Space(5.0f);

        DrawControl(plugin, GUILayoutUtility.GetRect(200, scopeheight, GUILayout.ExpandWidth(true)), plugin.GetSampleRate(), 1);
        GUILayout.Space(5.0f);

        return true;
    }
}
