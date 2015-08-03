using UnityEngine;
using UnityEngine.Audio;
using System.Collections;

public class FireSelector : MonoBehaviour
{
    public AudioMixerSnapshot[] fireTypeSnapshots;
    public AudioMixerSnapshot fireType;

    void Start()
    {
        fireType = fireTypeSnapshots[0];
        fireType.TransitionTo(0);
    }

    void OnGUI()
    {
        GUILayout.Label("Fire type:");
        foreach (var t in fireTypeSnapshots)
        {
            string name = t.name;
            if (fireType == t)
                name = "> " + name + " <";
            if (GUILayout.Button(name))
            {
                fireType = t;
                t.TransitionTo(3.0f);
            }
        }
    }
}
