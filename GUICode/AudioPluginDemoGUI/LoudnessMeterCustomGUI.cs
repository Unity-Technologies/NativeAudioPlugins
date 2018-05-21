using System;
using UnityEditor;
using UnityEngine;

public class LoudnessMeterCustomGUI : IAudioEffectPluginGUI
{
    public override string Name
    {
        get { return "Demo LoudnessMeter"; }
    }

    public override string Description
    {
        get { return "Loudness meter demo plugin for Unity's audio plugin system"; }
    }

    public override string Vendor
    {
        get { return "Unity"; }
    }

    private void DrawCurve(Rect r, float[] curve, float yoffset, float yscale, Color col, float labeloffset)
    {
        int numsamples = curve.Length - 1; // the last sample in buffer is counter for how much we could read
        float transitionPixels = 30.0f;
        float x0 = 1.0f - ((float)curve[numsamples] / (float)curve.Length);
        float xr = transitionPixels / r.width;
        float xscale = numsamples - 2;
        float thr_dB = -120.0f;
        float thr_lin = 20.0f * Mathf.Pow(10.0f, thr_dB * 0.05f);
        col.a /= xr;
        float peakVal = -100000.0f, peakPos = -1000000.0f;
        AudioCurveRendering.DrawFilledCurve(
            r,
            delegate(float x, out Color color)
            {
                float f = Mathf.Clamp(x * xscale, 0.0f, xscale);
                color = col;
                color.a = col.a * Mathf.Clamp(x - x0, 0, xr);
                int i = (int)Mathf.Floor(f);
                float mag = curve[i] + (curve[i + 1] - curve[i]) * (f - i);
                float mag_dB = (mag < thr_lin) ? thr_dB : (20.0f * Mathf.Log10(mag));
                float pos = (mag_dB - yoffset) * yscale;
                peakVal = Mathf.Max(peakVal, mag_dB);
                peakPos = Mathf.Max(peakPos, pos);
                return pos;
            }
            );
        peakPos = r.y + r.height * (0.5f - 0.5f * peakPos);
        if (peakPos >= r.y && peakPos <= r.y + r.height)
        {
            var col2 = new Color(col.r, col.g, col.b, 0.7f);
            GUIHelpers.DrawLine(r.x, peakPos, r.x + r.width, peakPos, col2);
            GUIHelpers.DrawText(r.x + labeloffset - 30, peakPos + 6, 60, string.Format("{0:F1} dB", peakVal), col2);
        }
    }

    public bool DrawControl(IAudioEffectPlugin plugin, Rect r, float samplerate)
    {
        Event evt = Event.current;

        int dragControlID = GUIUtility.GetControlID(FocusType.Passive);

        r = AudioCurveRendering.BeginCurveFrame(r);

        float windowMin, windowMax, windowDef; plugin.GetFloatParameterInfo("Window", out windowMin, out windowMax, out windowDef);
        float yscaleMin, yscaleMax, yscaleDef; plugin.GetFloatParameterInfo("YScale", out yscaleMin, out yscaleMax, out yscaleDef);
        float yoffsetMin, yoffsetMax, yoffsetDef; plugin.GetFloatParameterInfo("YOffset", out yoffsetMin, out yoffsetMax, out yoffsetDef);

        float window; plugin.GetFloatParameter("Window", out window);
        float yscale; plugin.GetFloatParameter("YScale", out yscale);
        float yoffset; plugin.GetFloatParameter("YOffset", out yoffset);

        float blend = plugin.IsPluginEditableAndEnabled() ? 1.0f : 0.5f;

        switch (evt.GetTypeForControl(dragControlID))
        {
            case EventType.MouseDown:
                if (evt.button == 0 && r.Contains(evt.mousePosition) && GUIUtility.hotControl == 0)
                {
                    GUIUtility.hotControl = dragControlID;
                    evt.Use();
                }
                break;

            case EventType.MouseUp:
                if (evt.button == 0 && GUIUtility.hotControl == dragControlID)
                {
                    GUIUtility.hotControl = 0;
                    evt.Use();
                }
                break;

            case EventType.MouseDrag:
                if (GUIUtility.hotControl == dragControlID)
                {
                    window = Mathf.Clamp(window + evt.delta.x * 0.1f, windowMin, windowMax);
                    if (evt.shift)
                        yoffset = Mathf.Clamp(yoffset - (0.5f * evt.delta.y / yscale), yoffsetMin, yoffsetMax);
                    else
                        yscale = Mathf.Clamp(yscale - evt.delta.y * 0.01f, yscaleMin, yscaleMax);
                    plugin.SetFloatParameter("Window", window);
                    plugin.SetFloatParameter("YScale", yscale);
                    plugin.SetFloatParameter("YOffset", yoffset);
                    evt.Use();
                }
                break;

            case EventType.ScrollWheel:
                if (r.Contains(evt.mousePosition))
                {
                    window = Mathf.Clamp(window + evt.delta.x * 0.1f, windowMin, windowMax);
                    yoffset = Mathf.Clamp(yoffset - (0.5f * evt.delta.y / yscale), yoffsetMin, yoffsetMax);
                    plugin.SetFloatParameter("Window", window);
                    plugin.SetFloatParameter("YScale", yscale);
                    plugin.SetFloatParameter("YOffset", yoffset);
                    evt.Use();
                }
                break;

            case EventType.Repaint:
            {
                float yscaleDraw = yscale * 0.05f;

                // Background grid and values
                Color lineColor = new Color(0, 0, 0, 0.2f);
                Color textColor = new Color(1.0f, 1.0f, 1.0f, 0.3f * blend);
                GUIHelpers.DrawDbTickMarks(r, yoffset, yscaleDraw, textColor, lineColor);
                GUIHelpers.DrawTimeTickMarks(r, window, textColor, lineColor);

                // Curves
                int numsamples = (int)r.width;
                float[] mcurve; plugin.GetFloatBuffer("MomentaryRMS", out mcurve, numsamples);
                float[] scurve; plugin.GetFloatBuffer("ShortTermRMS", out scurve, numsamples);
                float[] icurve; plugin.GetFloatBuffer("IntegratedRMS", out icurve, numsamples);

                DrawCurve(r, mcurve, yoffset, yscaleDraw, new Color(1.0f, 0.0f, 0.0f, blend * 0.5f), 90);
                DrawCurve(r, scurve, yoffset, yscaleDraw, new Color(0.0f, 1.0f, 0.0f, blend * 0.3f), 150);
                DrawCurve(r, icurve, yoffset, yscaleDraw, new Color(0.0f, 0.0f, 1.0f, blend * 0.3f), 210);
            }
            break;
        }

        AudioCurveRendering.EndCurveFrame();
        return false;
    }

    public override bool OnGUI(IAudioEffectPlugin plugin)
    {
        GUILayout.Space(5f);
        Rect r = GUILayoutUtility.GetRect(200, 150, GUILayout.ExpandWidth(true));
        DrawControl(plugin, r, plugin.GetSampleRate());
        GUILayout.Space(5f);
        return true;
    }
}
