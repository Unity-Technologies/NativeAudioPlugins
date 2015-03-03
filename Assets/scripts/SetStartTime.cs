using UnityEngine;
using System.Collections;

[RequireComponent(typeof(AudioSource))]
public class SetStartTime : MonoBehaviour
{
    public float startTime = 0.0f;

    // Use this for initialization
    void Start()
    {
        var s = GetComponent<AudioSource>();
        s.time = startTime;
        s.Play();
    }

    // Update is called once per frame
    void Update()
    {
    }
}
