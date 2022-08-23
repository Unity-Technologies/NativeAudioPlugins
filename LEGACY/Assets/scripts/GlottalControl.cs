using UnityEngine;
using UnityEngine.Audio;
using System.Collections;

public class GlottalControl : MonoBehaviour
{
    public AudioMixer mixer;

    void Start()
    {
    }

    void Update()
    {
        if (Input.GetMouseButton(0))
        {
            mixer.SetFloat("Radius1", 0.1f + 2.0f * Mathf.Pow(Input.mousePosition.x / Screen.width, 3.0f));
            mixer.SetFloat("Radius2", 0.1f + 2.0f * Mathf.Pow(Input.mousePosition.y / Screen.width, 3.0f));
            mixer.SetFloat("Period", 15.0f + 20.0f * Mathf.Pow(Input.mousePosition.y / Screen.width, 3.0f));
        }
    }
}
