using UnityEditor;
using UnityEngine;
using System.Runtime.InteropServices;

public class GranulatorCustomGUI : IAudioEffectPluginGUI
{
    [DllImport("AudioPluginDemo")]
    private static extern System.IntPtr Granulator_GetSampleName(int index);

    public override string Name
    {
        get { return "Demo Granulator"; }
    }

    public override string Description
    {
        get { return "Granular synthesizer demo plugin for Unity's audio plugin system"; }
    }

    public override string Vendor
    {
        get { return "Unity"; }
    }

    Color m_Wave1Color = AudioCurveRendering.kAudioOrange;
    Color m_Wave2Color = AudioCurveRendering.kAudioOrange;
    Color m_Wave1ColorOfs = Color.red;
    Color m_Wave2ColorOfs = Color.red;

    private void DrawCurve(Rect r, float[] curve, float yscale, Color col1, Color col2, float labeloffset, float ofs1, float ofs2)
    {
        float xscale = curve.Length - 2;
        float maxval = 0.0f;
        for (int n = 0; n < curve.Length; n++)
            maxval = Mathf.Max(maxval, Mathf.Abs(curve[n]));
        yscale *= (maxval > 0.0f) ? (1.0f / maxval) : 0.0f;
        AudioCurveRendering.DrawSymmetricFilledCurve(
            r,
            delegate(float x, out Color color)
        {
            float f = Mathf.Clamp(x * xscale, 0.0f, xscale);
            int i = (int)Mathf.Floor(f);
            color = (x<ofs1 || x> ofs2) ? col1 : col2;
            return (curve[i] + (curve[i + 1] - curve[i]) * (f - i)) * yscale;
        }
            );
    }

    public void DrawControl(IAudioEffectPlugin plugin, Rect r, float samplerate)
    {
        r = AudioCurveRendering.BeginCurveFrame(r);

        if (Event.current.type == EventType.Repaint)
        {
            float blend = plugin.IsPluginEditableAndEnabled() ? 1.0f : 0.5f;

            int numsamples = (int)r.width;
            float[] wave1; plugin.GetFloatBuffer("Waveform0", out wave1, numsamples);
            float[] wave2; plugin.GetFloatBuffer("Waveform1", out wave2, numsamples);

            float ofs; plugin.GetFloatParameter("Offset", out ofs);
            float rofs; plugin.GetFloatParameter("Rnd offset", out rofs);
            float useSample; plugin.GetFloatParameter("Use Sample", out useSample);

            m_Wave1Color.a = m_Wave2Color.a = blend;

            var r2 = new Rect(r.x, r.y, r.width, r.height * 0.5f);
            DrawCurve(r2, wave1, 1.0f, m_Wave1Color, m_Wave1ColorOfs, 90, ofs, ofs + rofs);
            r2.y += r2.height;
            DrawCurve(r2, wave2, 1.0f, m_Wave2Color, m_Wave2ColorOfs, 150, ofs, ofs + rofs);

            float x1 = r.x + r.width * ofs;
            float x2 = r.x + r.width * (ofs + rofs);
            GUIHelpers.DrawLine(x1, r.y, x1, r.y + r.height, Color.red);
            GUIHelpers.DrawLine(x2, r.y, x2, r.y + r.height, Color.red);

            string name = "Sample: " + Marshal.PtrToStringAnsi(Granulator_GetSampleName((int)useSample));
            GUIHelpers.DrawText(r2.x + 5, r2.y - 5, r2.width, name, Color.white);
        }
        AudioCurveRendering.EndCurveFrame();
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
