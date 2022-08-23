using UnityEngine;
using System.Collections;

[RequireComponent(typeof(AudioSource))]
public class ImpactNoiseGenerator : MonoBehaviour
{
    public float frictionPeriodMin = 300.0f;
    public float frictionPeriodMax = 900.0f;
    public float frictionFrequencyMin = 100.0f;
    public float frictionFrequencyMax = 300.0f;
    public float frictionWhiteNoiseAmp = 0.0f;
    public float frictionAmp = 1.0f;
    public float frictionDecayTime = 0.1f;
    public float frictionBandwidth = 0.3f;

    public float impactDecayTimeMin = 0.1f;
    public float impactDecayTimeMax = 0.1f;
    public float impactAmpMin = 1.0f;
    public float impactAmpMax = 1.0f;
    public float impactFrequencyMin = 300.0f;
    public float impactFrequencyMax = 900.0f;
    public float impactBandwidthMin = 0.3f;
    public float impactBandwidthMax = 0.3f;
    public float stayFrictionAmp = 0.05f;

    Vector3 lastPos = Vector3.zero;
    float friction = 0.0f;
    float frictionCount = 0.0f;
    float frictionCutoff = 0.0f;
    float frictionLPF = 0.0f;
    float frictionBPF = 0.0f;
    float frictionNorm = 0.0f;
    float speed = 0.0f;
    float lastSpeed = 0.0f;
    float currSpeed = 0.0f;

    ulong seed = 0;
    float lastImpact = 0.0f;
    float impact = 0.0f;
    float impactDecayConst = 0.0f;
    float impactCutoff = 0.0f;
    float impactBandwidth = 0.0f;
    float impactLPF = 0.0f;
    float impactBPF = 0.0f;
    float impactNorm = 0.0f;
    int samplerate = 0;
    int skipfirstframes = 10;
    float stayFriction = 0.0f;

    void Start()
    {
        var r = new System.Random();
        seed = (ulong)r.Next();
        samplerate = AudioSettings.outputSampleRate;
    }

    void Update()
    {
        if (skipfirstframes > 0)
        {
            skipfirstframes--;
            return;
        }
        var currPos = gameObject.transform.position;
        Vector3 delta = currPos - lastPos;
        lastPos = currPos;
        speed = delta.magnitude;
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

    void OnCollisionEnter(Collision col)
    {
        var rb = GetComponent<Rigidbody>();
        impact = rb.mass * col.relativeVelocity.magnitude * col.relativeVelocity.magnitude;
        //Debug.Log("Collision");

        //foreach (ContactPoint contact in col.contacts)
        //  Debug.DrawRay(contact.point, contact.normal, Color.red, 10.0f);
    }

    void OnCollisionStay(Collision col)
    {
        stayFriction = stayFrictionAmp;
    }

    void OnAudioFilterRead(float[] data, int numchannels)
    {
        if (samplerate == 0)
            return;

        if (lastImpact < impact)
        {
            float impactDecayTime = GetRandom(impactDecayTimeMin, impactDecayTimeMax);
            impactDecayConst = (impactDecayTime <= 0.0f) ? 0.0f : Mathf.Exp(-1.0f / (impactDecayTime * samplerate));
            impactCutoff = 2.0f * Mathf.Sin(Mathf.PI * Mathf.Clamp(GetRandom(impactFrequencyMin, impactFrequencyMax), 0.0f, samplerate * 0.5f) / (float)samplerate);
            impactBandwidth = GetRandom(impactBandwidthMin, impactBandwidthMax);
            impact = GetRandom(impactAmpMin, impactAmpMax);
            impactNorm = Mathf.Sqrt(Mathf.Abs(impactBandwidth) * 0.5f + 0.001f);
        }

        float frictionDecay = (frictionDecayTime <= 0.0f) ? 0.0f : Mathf.Exp(-1.0f / (frictionDecayTime * samplerate));

        for (int n = 0; n < data.Length; n += numchannels)
        {
            if ((n & 15) == 0)
            {
                currSpeed = lastSpeed + (speed - lastSpeed) * n / (float)data.Length;
                frictionCutoff = 2.0f * Mathf.Sin(Mathf.PI * Mathf.Clamp(frictionFrequencyMin + (frictionFrequencyMax - frictionFrequencyMin) * currSpeed, 0.0f, samplerate * 0.5f) / (float)samplerate);
                frictionNorm = Mathf.Sqrt(Mathf.Abs(frictionBandwidth) * 0.5f + 0.001f);
            }

            frictionCount -= currSpeed;
            while (frictionCount <= 0)
            {
                frictionCount += GetRandom(frictionPeriodMin, frictionPeriodMax) * samplerate;
                friction = GetRandom() * frictionAmp;
            }

            float r = GetRandom() - 0.5f;
            friction = friction * frictionDecay + 1.0e-9f;
            impactLPF += impactCutoff * impactBPF;
            impactBPF += impactCutoff * (r * impact * impactNorm - impactLPF - impactBPF * impactBandwidth);
            impact = impact * impactDecayConst + 1.0e-9f;
            frictionLPF += frictionCutoff * frictionBPF;
            frictionBPF += frictionCutoff * (r * (frictionWhiteNoiseAmp + friction + stayFriction) * frictionNorm * currSpeed - frictionLPF - frictionBPF * frictionBandwidth);
            for (int i = 0; i < numchannels; i++)
                data[n + i] += impactBPF + frictionBPF;
        }

        lastImpact = impact;
        lastSpeed = speed;
    }
}
