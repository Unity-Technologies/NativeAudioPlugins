using UnityEngine;
using System.Collections;

[RequireComponent(typeof(AudioSource))]
public class ShowPlayPos : MonoBehaviour
{
    void Start()
    {
    }

    void Update()
    {
        GetComponent<GUIText>().text = GetComponent<AudioSource>().time.ToString();
    }
}
