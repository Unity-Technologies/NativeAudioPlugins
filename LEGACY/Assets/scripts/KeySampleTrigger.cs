using UnityEngine;
using System.Collections;

[RequireComponent(typeof(AudioSource))]
public class KeySampleTrigger : MonoBehaviour
{
    public AudioClip clip;
    public string key;
    public float starttime;
    public float endtime;

    void Start()
    {
    }

    void Update()
    {
        if (Input.GetKeyDown(key))
        {
            var source = GetComponent<AudioSource>();
            double t = AudioSettings.dspTime;
            source.clip = clip;
            source.time = starttime;
            source.PlayScheduled(t);
            source.SetScheduledEndTime(t + endtime - starttime);
        }
    }
}
