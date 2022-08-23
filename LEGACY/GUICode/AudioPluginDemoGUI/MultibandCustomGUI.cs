using UnityEditor;
using UnityEngine;
using System;

public class MultibandCustomGUI : FilterCurveUI
{
    private float masterGain;
    private float lowGain, midGain, highGain;
    private float lowFreq, highFreq;
    private float lowAttackTime, midAttackTime, highAttackTime;
    private float lowReleaseTime, midReleaseTime, highReleaseTime;
    private float lowThreshold, midThreshold, highThreshold;
    private float lowRatio, midRatio, highRatio;
    private float lowKnee, midKnee, highKnee;
    private float filterOrder;
    private bool useLogScale;
    private bool showSpectrum;

    public override string Name
    {
        get { return "Demo Multiband"; }
    }

    public override string Description
    {
        get { return "Multiband compressor demo plugin for Unity's audio plugin system"; }
    }

    public override string Vendor
    {
        get { return "Unity"; }
    }

    private void DrawFilterCurve(
        Rect r,
        float[] coeffs,
        float lowGain, float midGain, float highGain,
        Color color,
        bool filled,
        double samplerate,
        double magScale)
    {
        double wm = -2.0 * 3.1415926 / samplerate;

        AudioCurveRendering.AudioCurveEvaluator d = delegate(float x) {
                MathHelpers.ComplexD w = MathHelpers.ComplexD.Exp(wm * GUIHelpers.MapNormalizedFrequency((double)x, samplerate, useLogScale, true));
                MathHelpers.ComplexD lpf  = MathHelpers.ComplexD.Pow((w * (w * coeffs[0] + coeffs[1]) + coeffs[2]) / (w * (w * coeffs[3] + coeffs[4]) + 1.0f), filterOrder);
                MathHelpers.ComplexD bpf1 = MathHelpers.ComplexD.Pow((w * (w * coeffs[5] + coeffs[6]) + coeffs[7]) / (w * (w * coeffs[8] + coeffs[9]) + 1.0f), filterOrder);
                MathHelpers.ComplexD bpf2 = MathHelpers.ComplexD.Pow((w * (w * coeffs[10] + coeffs[11]) + coeffs[12]) / (w * (w * coeffs[13] + coeffs[14]) + 1.0f), filterOrder);
                MathHelpers.ComplexD hpf  = MathHelpers.ComplexD.Pow((w * (w * coeffs[15] + coeffs[16]) + coeffs[17]) / (w * (w * coeffs[18] + coeffs[19]) + 1.0f), filterOrder);
                double h = (lpf * lowGain).Mag2() + (bpf1 * bpf2 * midGain).Mag2() + (hpf * highGain).Mag2();
                double mag = masterGain + 10.0 * Math.Log10(h);
                return (float)(mag * magScale);
            };

        if (filled)
            AudioCurveRendering.DrawFilledCurve(r, d, color);
        else
            AudioCurveRendering.DrawCurve(r, d, color);
    }

    public bool DrawControl(IAudioEffectPlugin plugin, Rect r, float samplerate)
    {
        Event evt = Event.current;
        int controlID = GUIUtility.GetControlID(FocusType.Passive);
        EventType evtType = evt.GetTypeForControl(controlID);

        r = AudioCurveRendering.BeginCurveFrame(r);

        float thr = 4.0f;
        bool changed = false;
        float x = evt.mousePosition.x - r.x;
        if (evtType == EventType.MouseDown && r.Contains(evt.mousePosition) && evt.button == 0)
        {
            float lf = (float)GUIHelpers.MapNormalizedFrequency(lowFreq, samplerate, useLogScale, false) * r.width;
            float hf = (float)GUIHelpers.MapNormalizedFrequency(highFreq, samplerate, useLogScale, false) * r.width;
            dragOperation = DragOperation.Mid;
            if (x < lf + thr)
                dragOperation = DragOperation.Low;
            else if (x > hf - thr)
                dragOperation = DragOperation.High;
            GUIUtility.hotControl = controlID;
            EditorGUIUtility.SetWantsMouseJumping(1);
            evt.Use();
        }
        else if (evtType == EventType.MouseDrag && GUIUtility.hotControl == controlID)
        {
            if (dragOperation == DragOperation.Low || dragOperation == DragOperation.Mid)
                lowFreq = Mathf.Clamp((float)GUIHelpers.MapNormalizedFrequency(GUIHelpers.MapNormalizedFrequency(lowFreq, samplerate, useLogScale, false) + evt.delta.x / r.width, samplerate, useLogScale, true), 10.0f, highFreq);
            if (dragOperation == DragOperation.Low)
                lowGain = Mathf.Clamp(lowGain - evt.delta.y * 0.5f, -100.0f, 100.0f);
            if (dragOperation == DragOperation.Mid)
                midGain = Mathf.Clamp(midGain - evt.delta.y * 0.5f, -100.0f, 100.0f);
            if (dragOperation == DragOperation.Mid || dragOperation == DragOperation.High)
                highFreq = Mathf.Clamp((float)GUIHelpers.MapNormalizedFrequency(GUIHelpers.MapNormalizedFrequency(highFreq, samplerate, useLogScale, false) + evt.delta.x / r.width, samplerate, useLogScale, true), lowFreq, samplerate * 0.5f);
            if (dragOperation == DragOperation.High)
                highGain = Mathf.Clamp(highGain - evt.delta.y * 0.5f, -100.0f, 100.0f);
            changed = true;
            evt.Use();
        }
        else if (evtType == EventType.MouseUp && evt.button == 0 && GUIUtility.hotControl == controlID)
        {
            GUIUtility.hotControl = 0;
            EditorGUIUtility.SetWantsMouseJumping(0);
            evt.Use();
        }

        if (Event.current.type == EventType.Repaint)
        {
            float blend = plugin.IsPluginEditableAndEnabled() ? 1.0f : 0.5f;

            // Mark bands (low, medium and high bands)
            Color lowColor = new Color(0.0f, 0.0f, 0.0f, blend);
            Color midColor = new Color(0.5f, 0.5f, 0.5f, blend);
            Color highColor = new Color(1.0f, 1.0f, 1.0f, blend);
            DrawBandSplitMarker(plugin, r, (float)GUIHelpers.MapNormalizedFrequency(lowFreq, samplerate, useLogScale, false) * r.width, thr, GUIUtility.hotControl == controlID && (dragOperation == DragOperation.Low || dragOperation == DragOperation.Mid), lowColor);
            DrawBandSplitMarker(plugin, r, (float)GUIHelpers.MapNormalizedFrequency(highFreq, samplerate, useLogScale, false) * r.width, thr, GUIUtility.hotControl == controlID && (dragOperation == DragOperation.High || dragOperation == DragOperation.Mid), highColor);

            const float dbRange = 40.0f;
            const float magScale = 1.0f / dbRange;

            float[] liveData;
            plugin.GetFloatBuffer("LiveData", out liveData, 6);

            float[] coeffs;
            plugin.GetFloatBuffer("Coeffs", out coeffs, 20);

            if (GUIUtility.hotControl == controlID)
                DrawFilterCurve(
                    r,
                    coeffs,
                    (dragOperation == DragOperation.Low) ? Mathf.Pow(10.0f, 0.05f * lowGain) : 0.0f,
                    (dragOperation == DragOperation.Mid) ? Mathf.Pow(10.0f, 0.05f * midGain) : 0.0f,
                    (dragOperation == DragOperation.High) ? Mathf.Pow(10.0f, 0.05f * highGain) : 0.0f,
                    new Color(1.0f, 1.0f, 1.0f, 0.2f * blend),
                    true,
                    samplerate,
                    magScale);

            DrawFilterCurve(r, coeffs, Mathf.Pow(10.0f, 0.05f * lowGain) * liveData[0], 0.0f, 0.0f, lowColor, false, samplerate, magScale);
            DrawFilterCurve(r, coeffs, 0.0f, Mathf.Pow(10.0f, 0.05f * midGain) * liveData[1], 0.0f, midColor, false, samplerate, magScale);
            DrawFilterCurve(r, coeffs, 0.0f, 0.0f, Mathf.Pow(10.0f, 0.05f * highGain) * liveData[2], highColor, false, samplerate, magScale);

            DrawFilterCurve(
                r,
                coeffs,
                Mathf.Pow(10.0f, 0.05f * lowGain) * liveData[0],
                Mathf.Pow(10.0f, 0.05f * midGain) * liveData[1],
                Mathf.Pow(10.0f, 0.05f * highGain) * liveData[2],
                ScaleAlpha(AudioCurveRendering.kAudioOrange, 0.5f),
                false,
                samplerate,
                magScale);

            DrawFilterCurve(
                r,
                coeffs,
                Mathf.Pow(10.0f, 0.05f * lowGain),
                Mathf.Pow(10.0f, 0.05f * midGain),
                Mathf.Pow(10.0f, 0.05f * highGain),
                AudioCurveRendering.kAudioOrange,
                false,
                samplerate,
                magScale);

            if (showSpectrum)
            {
                int specLen = (int)r.width;
                float[] spec;

                plugin.GetFloatBuffer("InputSpec", out spec, specLen);
                DrawSpectrum(r, useLogScale, spec, dbRange, samplerate, 0.3f, 1.0f, 0.3f, 0.5f * blend, 0.0f);

                plugin.GetFloatBuffer("OutputSpec", out spec, specLen);
                DrawSpectrum(r, useLogScale, spec, dbRange, samplerate, 1.0f, 0.3f, 0.3f, 0.5f * blend, 0.0f);
            }

            GUIHelpers.DrawFrequencyTickMarks(r, samplerate, useLogScale, new Color(1.0f, 1.0f, 1.0f, 0.3f * blend));
        }

        AudioCurveRendering.EndCurveFrame();
        return changed;
    }

    public override bool OnGUI(IAudioEffectPlugin plugin)
    {
        float useLogScaleFloat;
        float showSpectrumFloat;
        plugin.GetFloatParameter("MasterGain", out masterGain);
        plugin.GetFloatParameter("LowGain", out lowGain);
        plugin.GetFloatParameter("MidGain", out midGain);
        plugin.GetFloatParameter("HighGain", out highGain);
        plugin.GetFloatParameter("LowFreq", out lowFreq);
        plugin.GetFloatParameter("HighFreq", out highFreq);
        plugin.GetFloatParameter("LowAttackTime", out lowAttackTime);
        plugin.GetFloatParameter("MidAttackTime", out midAttackTime);
        plugin.GetFloatParameter("HighAttackTime", out highAttackTime);
        plugin.GetFloatParameter("LowReleaseTime", out lowReleaseTime);
        plugin.GetFloatParameter("MidReleaseTime", out midReleaseTime);
        plugin.GetFloatParameter("HighReleaseTime", out highReleaseTime);
        plugin.GetFloatParameter("LowThreshold", out lowThreshold);
        plugin.GetFloatParameter("MidThreshold", out midThreshold);
        plugin.GetFloatParameter("HighThreshold", out highThreshold);
        plugin.GetFloatParameter("LowRatio", out lowRatio);
        plugin.GetFloatParameter("MidRatio", out midRatio);
        plugin.GetFloatParameter("HighRatio", out highRatio);
        plugin.GetFloatParameter("LowKnee", out lowKnee);
        plugin.GetFloatParameter("MidKnee", out midKnee);
        plugin.GetFloatParameter("HighKnee", out highKnee);
        plugin.GetFloatParameter("FilterOrder", out filterOrder);
        plugin.GetFloatParameter("UseLogScale", out useLogScaleFloat);
        plugin.GetFloatParameter("ShowSpectrum", out showSpectrumFloat);
        useLogScale = useLogScaleFloat > 0.5f;
        showSpectrum = showSpectrumFloat > 0.5f;
        GUILayout.Space(5f);
        Rect r = GUILayoutUtility.GetRect(200, 150, GUILayout.ExpandWidth(true));
        if (DrawControl(plugin, r, plugin.GetSampleRate()))
        {
            plugin.SetFloatParameter("MasterGain", masterGain);
            plugin.SetFloatParameter("LowGain", lowGain);
            plugin.SetFloatParameter("MidGain", midGain);
            plugin.SetFloatParameter("HighGain", highGain);
            plugin.SetFloatParameter("LowFreq", lowFreq);
            plugin.SetFloatParameter("HighFreq", highFreq);
            plugin.SetFloatParameter("LowAttackTime", lowAttackTime);
            plugin.SetFloatParameter("MidAttackTime", midAttackTime);
            plugin.SetFloatParameter("HighAttackTime", highAttackTime);
            plugin.SetFloatParameter("LowReleaseTime", lowReleaseTime);
            plugin.SetFloatParameter("MidReleaseTime", midReleaseTime);
            plugin.SetFloatParameter("HighReleaseTime", highReleaseTime);
            plugin.SetFloatParameter("LowThreshold", lowThreshold);
            plugin.SetFloatParameter("MidThreshold", midThreshold);
            plugin.SetFloatParameter("HighThreshold", highThreshold);
            plugin.SetFloatParameter("LowRatio", lowRatio);
            plugin.SetFloatParameter("MidRatio", midRatio);
            plugin.SetFloatParameter("HighRatio", highRatio);
            plugin.SetFloatParameter("LowKnee", lowKnee);
            plugin.SetFloatParameter("MidKnee", midKnee);
            plugin.SetFloatParameter("HighKnee", highKnee);
        }
        return true;
    }
}
