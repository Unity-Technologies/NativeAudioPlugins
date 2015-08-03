using UnityEngine;
using UnityEngine.Audio;
using System.Collections;

public class EngineControl : MonoBehaviour
{
    public AudioMixer mixer;
    private float speed = 20.0f;

    void Start()
    {
    }

    void Update()
    {
        float acceleration = 100.0f * Time.deltaTime;
        if (Input.GetMouseButton(0))
            speed += acceleration;
        else
            speed = Mathf.Max(14.0f, speed * Mathf.Pow(0.4f, Time.deltaTime));
        mixer.SetFloat("PistonRate", speed);
        mixer.SetFloat("PistonSpeed", 0.6f + 0.002f * speed);
        mixer.SetFloat("PistonRandomOffset", 0.015f / (1.0f + 0.003f * speed));
        mixer.SetFloat("PistonLength", 0.05f / (1.0f + 0.003f * speed));
        mixer.SetFloat("PistonDistortion", 0.7f - 0.0002f * speed);
    }
}
