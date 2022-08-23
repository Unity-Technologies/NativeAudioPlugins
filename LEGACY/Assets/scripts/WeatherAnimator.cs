using UnityEngine;
using UnityEngine.Audio;
using System.Collections;

public class WeatherAnimator : MonoBehaviour
{
    class RandomTrack
    {
        public RandomTrack(float minPeriod, float maxPeriod)
        {
            this.minPeriod = minPeriod;
            this.maxPeriod = maxPeriod;
        }

        public float Update(float deltatime)
        {
            time += deltatime;
            if (time >= currPeriod)
            {
                time = 0;
                currVal = nextVal;
                nextVal = (float)random.NextDouble();
                currPeriod = (float)random.NextDouble() * (maxPeriod - minPeriod) + minPeriod;
            }
            return currVal + (nextVal - currVal) * Mathf.Clamp(time / currPeriod, 0.0f, 1.0f);
        }

        System.Random random = new System.Random();
        float minPeriod;
        float maxPeriod;
        float time;
        float currPeriod;
        float currVal;
        float nextVal;
    }

    public float windSpeed = 0.0f;
    public float windLevel = -80.0f;
    public float rainLevel = -80.0f;
    public float thunderLevel = -80.0f;
    public float thunderDistance = 2.0f;

    public AudioMixer windMixer;

    System.Random random = new System.Random();
    RandomTrack flutterLevel = new RandomTrack(0.01f, 0.1f);
    RandomTrack baseCut1 = new RandomTrack(3.0f, 3.0f);
    RandomTrack baseCut2 = new RandomTrack(0.2f, 0.5f);
    RandomTrack flashCut = new RandomTrack(0.02f, 0.2f);
    RandomTrack flashLevel = new RandomTrack(0.02f, 0.05f);

    float timeToNextThunder = 0.0f;
    float thunderStartTime;
    float thunderCutoffBase;
    float thunderCutoffDecay;

    void Start()
    {
        Update();
    }

    void Update()
    {
        // Rain
        windMixer.SetFloat("RainLevel", rainLevel);
        windMixer.SetFloat("RainCut", (rainLevel + 50) * 200);

        // Wind
        windMixer.SetFloat("WindLevel", windLevel);
        windMixer.SetFloat("FlutterLevel", (float)(flutterLevel.Update(Time.deltaTime) * 10.0f * windSpeed - 5.0f));
        windMixer.SetFloat("FlutterCut", 700.0f * windSpeed + 3000.0f * windSpeed);
        windMixer.SetFloat("BaseCut", 400.0f * windSpeed + (2000.0f * baseCut1.Update(Time.deltaTime) + 500.0f * baseCut2.Update(Time.deltaTime)) * windSpeed);
        windMixer.SetFloat("BaseLevel", -30.0f + 30.0f * windSpeed);
        windMixer.SetFloat("HowlingCut", 800.0f + 600.0f * windSpeed);
        windMixer.SetFloat("HowlingPeakCut", 1200.0f + 500.0f * windSpeed);

        // Thunder
        timeToNextThunder -= Time.deltaTime;
        if (timeToNextThunder <= 0.0f)
        {
            timeToNextThunder = (float)random.NextDouble() * 10.0f + 1.0f;
            thunderStartTime = Time.time;
            thunderCutoffBase = (float)random.NextDouble() * 4000.0f + 5000.0f;
            thunderCutoffDecay = (float)random.NextDouble() * 3.0f + 5.0f;
        }

        float t = (Time.time - thunderStartTime) * 0.3f;
        windMixer.SetFloat("FlashLevel", -30.0f + 25.0f * Mathf.Exp(-t) * (1.0f + flashLevel.Update(Time.deltaTime)) - thunderDistance * 3.0f);
        windMixer.SetFloat("FlashCut", thunderCutoffBase * Mathf.Exp(-thunderCutoffDecay * t) * (1.0f + flashCut.Update(Time.deltaTime)) / (1.0f + thunderDistance));
        windMixer.SetFloat("RumbleLevel", -20.0f + 120.0f * Mathf.Exp(-t) * t - thunderDistance);
        windMixer.SetFloat("ThunderLevel", thunderLevel);
    }

    void Slider(string name, float minval, float maxval, ref float value)
    {
        GUILayout.BeginHorizontal();
        GUILayout.Label(name);
        value = GUILayout.HorizontalSlider(value, minval, maxval, GUILayout.Width(300));
        GUILayout.EndHorizontal();
    }

    void OnGUI()
    {
        Slider("Wind speed", 0.0f, 1.0f, ref windSpeed);
        Slider("Wind level", -40.0f, 0.0f, ref windLevel);
        Slider("Rain level", -50.0f, -10.0f, ref rainLevel);
        Slider("Thunder level", -80.0f, -20.0f, ref thunderLevel);
        Slider("Thunder distance", 0.0f, 10.0f, ref thunderDistance);
        if (GUILayout.Button("Thunder!"))
            thunderStartTime = Time.time;
    }
}
