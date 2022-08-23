#define WRITEHEADER

using UnityEngine;
using System.Collections;
using System.IO;

[RequireComponent(typeof(AudioListener))]
public class AudioRecorder : MonoBehaviour
{
    BinaryWriter binwriter;

    public string filename = "record.wav";

    // Use this for initialization
    void Start()
    {
    #if WRITEHEADER
        var stream = new FileStream(filename, FileMode.Create);
        binwriter = new BinaryWriter(stream);
        for (int n = 0; n < 44; n++)
            binwriter.Write((byte)0);
    #else
        var stream = new FileStream("record.raw", FileMode.Create);
        binwriter = new BinaryWriter(stream);
    #endif
    }

    // Update is called once per frame
    void Update()
    {
    }

    void OnApplicationQuit()
    {
        var closewriter = binwriter;
        binwriter = null;
        #if WRITEHEADER
        int subformat = 3; // float
        int numchannels = 2;
        int numbits = 32;
        int samplerate = AudioSettings.outputSampleRate;
        Debug.Log("Closing file");
        long pos = closewriter.BaseStream.Length;
        closewriter.Seek(0, SeekOrigin.Begin);
        closewriter.Write((byte)'R'); closewriter.Write((byte)'I'); closewriter.Write((byte)'F'); closewriter.Write((byte)'F');
        closewriter.Write((uint)(pos - 8));
        closewriter.Write((byte)'W'); closewriter.Write((byte)'A'); closewriter.Write((byte)'V'); closewriter.Write((byte)'E');
        closewriter.Write((byte)'f'); closewriter.Write((byte)'m'); closewriter.Write((byte)'t'); closewriter.Write((byte)' ');
        closewriter.Write((uint)16);
        closewriter.Write((ushort)subformat);
        closewriter.Write((ushort)numchannels);
        closewriter.Write((uint)samplerate);
        closewriter.Write((uint)((samplerate * numchannels * numbits) / 8));
        closewriter.Write((ushort)((numchannels * numbits) / 8));
        closewriter.Write((ushort)numbits);
        closewriter.Write((byte)'d'); closewriter.Write((byte)'a'); closewriter.Write((byte)'t'); closewriter.Write((byte)'a');
        closewriter.Write((uint)(pos - 36));
        closewriter.Seek((int)pos, SeekOrigin.Begin);
        #endif
        closewriter.Flush();
    }

    void OnAudioFilterRead(float[] data, int numChannels)
    {
        if (binwriter == null)
            return;
        for (int n = 0; n < data.Length; n++)
            binwriter.Write(data[n]);
    }
}
