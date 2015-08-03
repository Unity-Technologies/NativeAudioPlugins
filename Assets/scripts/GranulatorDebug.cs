using UnityEngine;
using System.Collections;
using System.Runtime.InteropServices;

public class GranulatorDebug : MonoBehaviour
{
    [DllImport("AudioPluginDemo")]
    private static extern int Granulator_DebugGetGrainCount();

    // Use this for initialization
    void Start()
    {
    }

    // Update is called once per frame
    void Update()
    {
        var guitext = GetComponent<GUIText>();
        guitext.text = "Grain count: " + Granulator_DebugGetGrainCount();
    }
}
