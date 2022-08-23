using UnityEngine;
using System.Collections;

[RequireComponent(typeof(AudioSource))]
public class ShowPlayPos : MonoBehaviour
{
    void Start()
    {
    }

    void OnGUI()
    {
        GUILayout.Label(GetComponent<AudioSource>().time.ToString());
    }
}
