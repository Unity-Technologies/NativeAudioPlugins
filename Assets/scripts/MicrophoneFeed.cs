using UnityEngine;
using System.Collections;
using UnityEngine.Audio;

[RequireComponent(typeof(AudioSource))]
public class MicrophoneFeed : MonoBehaviour
{
    public bool useMicrophone = false;

    private AudioSource source;
    private string device = "Built-in Microphone"; // May need to change this for Windows
    private bool prevUseMicrophone = false;
    private AudioClip prevClip = null;

    void Update()
    {
        if (useMicrophone != prevUseMicrophone)
        {
            prevUseMicrophone = useMicrophone;
            if (useMicrophone)
            {
                source = GetComponent<AudioSource>();
                prevClip = source.clip;
                source.Stop();
                source.clip = Microphone.Start(device, true, 1, AudioSettings.outputSampleRate);
                source.Play();

                int dspBufferSize, dspNumBuffers;
                AudioSettings.GetDSPBufferSize(out dspBufferSize, out dspNumBuffers);

                source.timeSamples = (Microphone.GetPosition(device) + AudioSettings.outputSampleRate - 2 * dspBufferSize * dspNumBuffers) % AudioSettings.outputSampleRate;
            }
            else
            {
                Microphone.End(device);
                source.clip = prevClip;
                source.Play();
            }
        }
    }

    void OnGUI()
    {
        if (GUILayout.Button(useMicrophone ? "Disable microphone" : "Enable microphone"))
            useMicrophone = !useMicrophone;
    }
}
