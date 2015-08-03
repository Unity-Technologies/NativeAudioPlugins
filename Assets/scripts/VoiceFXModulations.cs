using UnityEngine;
using UnityEngine.Audio;
using System.Collections;

public class VoiceFXModulations : MonoBehaviour
{
    public AudioMixer mixer;

    public class RandomCurve
    {
        private System.Random random = new System.Random();
        private float currValue;
        private float nextValue;
        private float phase = -1;

        public float Evaluate(float freq, float minval, float maxval)
        {
            if (phase <= 0.0f)
            {
                currValue = nextValue;
                nextValue = (float)random.NextDouble() * (maxval - minval) + minval;
                if (phase == -1)
                {
                    currValue = nextValue;
                    phase = 0.0f;
                }
                else
                    phase = 1.0f;
            }
            float value = currValue + (nextValue - currValue) * (1.0f - phase);
            phase -= freq;
            return value;
        }
    }

    public RandomCurve[] randomCurve = new RandomCurve[6];

    void Start()
    {
        for (int n = 0; n < randomCurve.Length; n++)
            randomCurve[n] = new RandomCurve();
    }

    void Update()
    {
        float t = Time.deltaTime * 0.5f;
        mixer.SetFloat("RingModFreq1",
            randomCurve[0].Evaluate(2.3f * t, 300.0f, 11000.0f));
        mixer.SetFloat("RingModFreq2", randomCurve[1].Evaluate(1.31f * t, 300.0f, 11000.0f));
        mixer.SetFloat("NoiseAmount", randomCurve[2].Evaluate(0.33f * t, -40.0f, -20.0f));
        mixer.SetFloat("NoiseAddFreq", randomCurve[3].Evaluate(0.37f * t, 300.0f, 7000.0f));
        mixer.SetFloat("NoiseMulFreq", randomCurve[4].Evaluate(0.39f * t, 300.0f, 7000.0f));
        mixer.SetFloat("ParamEQFreq", randomCurve[5].Evaluate(0.19f * t, 700.0f, 7000.0f));
    }
}
