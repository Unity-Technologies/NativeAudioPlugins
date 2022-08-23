using UnityEngine;
using System.Collections;
using System.Runtime.InteropServices;

public class ImpactSignal : MonoBehaviour
{
    [DllImport("AudioPluginDemo")]
    private static extern void ImpactGenerator_AddImpact(int index, float volume, float decay, float cut, float bw);

    public AudioListener listener;
    public int instance = 0;
    public float minCutoff = 0.03f;
    public float maxCutoff = 0.4f;
    public float minBW = 0.1f;
    public float maxBW = 0.4f;
    public float minDecay = 0.1f;
    public float maxDecay = 0.5f;

    private float decayConst = 0.1f;
    private ulong seed = 0;
    private bool ready = false;

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

    void Start()
    {
        decayConst = -1.0f / (float)AudioSettings.outputSampleRate;
        ready = true;
    }

    void OnCollisionEnter(Collision col)
    {
        if (!ready)
            return;
        var rb = GetComponent<Rigidbody>();
        var impact = rb.mass * col.relativeVelocity.magnitude * col.relativeVelocity.magnitude;
        var delta = rb.position - listener.transform.position;
        var cut = 2.0f * Mathf.Sin(0.25f * Mathf.PI * GetRandom(minCutoff, maxCutoff));
        var bw = GetRandom(minBW, maxBW);
        var gain = 0.5f * (1.0f - bw * bw);
        var decay = Mathf.Exp(decayConst / GetRandom(minDecay, maxDecay));
        ImpactGenerator_AddImpact(instance, gain * impact / (1.0f + 5.0f * delta.magnitude), decay, cut, bw);
        //Debug.Log("Add impact cut=" + cut + " bw=" + bw + " gain=" + gain + " decay=" + decay);
        //Debug.Log("Collision");
    }
}
