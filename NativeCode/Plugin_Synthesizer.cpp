#include "AudioPluginUtil.h"

#if !PLATFORM_WINRT

namespace MIDI
{
    struct MidiEvent
    {
        UInt64 sample;
        UInt32 msg;
    };

    // For lack of a ring buffer that supports multiple producers we use two separate ring buffers here ;-)
    static AudioPluginUtil::RingBuffer<8192, MidiEvent> scheduledata;
    static AudioPluginUtil::RingBuffer<8192, UInt32> livedata;

    #if PLATFORM_WIN
        #include <windows.h>
        #include <mmsystem.h>
    #elif PLATFORM_OSX
        #include <CoreMIDI/MIDIServices.h>
    #endif

    class MidiInput
    {
    public:
        MidiInput();
        ~MidiInput();
    private:
    #if PLATFORM_WIN
        HMIDIIN m_midihandle;
        static void CALLBACK MidiInCallbackProc(HMIDIIN hMidiIn, UINT wMsg, DWORD dwInstance, DWORD dwParam1, DWORD dwParam2);
    #elif PLATFORM_OSX
        MIDIClientRef client;
        MIDIPortRef inPort;
        static void MidiInCallbackProc(const MIDIPacketList* pktlist, void* refCon, void* connRefCon);
    #endif
    };

    MidiInput::MidiInput()
    {
    #if PLATFORM_WIN
        int numdevs = midiInGetNumDevs();
        for (int n = 0; n < numdevs; n++)
        {
            midiInOpen(&m_midihandle, n, (DWORD_PTR)&MidiInCallbackProc, n, CALLBACK_FUNCTION);
            midiInStart(m_midihandle);
        }
    #elif PLATFORM_OSX
        client = NULL;
        MIDIClientCreate(CFSTR("MIDI Echo"), NULL, NULL, &client);
        inPort = NULL;
        MIDIInputPortCreate(client, CFSTR("Input port"), MidiInCallbackProc, this, &inPort);
        int n = MIDIGetNumberOfSources();
        for (int i = 0; i < n; ++i)
        {
            MIDIEndpointRef src = MIDIGetSource(i);
            MIDIPortConnectSource(inPort, src, NULL);
        }
    #endif
    }

    MidiInput::~MidiInput()
    {
    #if PLATFORM_WIN
        if (m_midihandle)
        {
            midiInStop(m_midihandle);
            midiInClose(m_midihandle);
        }
    #elif PLATFORM_OSX
        // TODO
    #endif
    }

    #if PLATFORM_WIN
    void CALLBACK MidiInput::MidiInCallbackProc(HMIDIIN hMidiIn, UINT wMsg, DWORD dwInstance, DWORD dwParam1, DWORD dwParam2)
    {
        if (wMsg == MIM_DATA)
            livedata.Feed(dwParam1);
    }

    #elif PLATFORM_OSX
    void MidiInput::MidiInCallbackProc(const MIDIPacketList* pktlist, void* refCon, void* connRefCon)
    {
        MIDIPacket* packet = (MIDIPacket*)pktlist->packet;
        for (unsigned int j = 0; j < pktlist->numPackets; j++)
        {
            if (packet->data[0] >= 0x80 && packet->data[0] < 0xF0)
            {
                int n = 0;
                while (n < packet->length)
                {
                    switch (packet->data[n] & 0xF0)
                    {
                        case 0x80:
                        case 0x90:
                        case 0xA0:
                        case 0xB0:
                        case 0xE0:
                            livedata.Feed(packet->data[n] + packet->data[n + 1] * 0x100 + packet->data[n + 2] * 0x10000);
                            n += 3;
                            break;
                        case 0xC0:
                        case 0xD0:
                            livedata.Feed(packet->data[n] + packet->data[n + 1] * 0x100);
                            n += 2;
                            break;
                        case 0xF0:
                            n = packet->length;
                            break;
                    }
                }
            }
            packet = MIDIPacketNext(packet);
        }
    }

    #endif
}

namespace Synthesizer
{
    const int OVERSAMPLING = 8;
    const int MAXVOICES = 32;
    const int MAXCHANNELS = 16;
    const int MAXOSCILLATORS = 8;
    const int RAMPSAMPLES = 64;

    static const float OSCSCALE = (const float)(0.5f / (float)(MAXOSCILLATORS * 0x100000000));
    static const float RAMPSCALE = (const float)(1.0f / (float)RAMPSAMPLES);

    static const float ONE_OVER_127 = (const float)(1.0f / 127.0f);
    static const float ONE_OVER_12 = (const float)(1.0f / 12.0f);
    static const float ONE_OVER_MAXOSCILLATORS = (const float)(1.0f / (float)MAXOSCILLATORS);

    enum Param
    {
        P_STREAM,
        P_CUTOFF,
        P_CUTENV,
        P_RESONANCE,
        P_DECAY,
        P_RELEASE,
        P_DETUNE1,
        P_DETUNE2,
        P_TYPE,
        P_NUM
    };

    struct VoiceChannel
    {
        UInt32 phase[MAXOSCILLATORS];
        UInt32 freq;
        UInt32 detune;
        UInt32 mask;

        float lpf;
        float bpf;

        inline void Reset()
        {
            memset(this, 0, sizeof(*this));
        }

        inline float Process(float cut, float bw)
        {
            float osc = 0.0f;
            UInt32 f = freq;
            for (int i = 0; i < MAXOSCILLATORS; i++)
            {
                UInt32 p = phase[i];
                for (int k = 0; k < OVERSAMPLING; k++)
                {
                    osc += p & mask;
                    p += f;
                }
                phase[i] = p;
                f += detune;
            }

            osc = (osc - MAXOSCILLATORS * 0.5f) * OSCSCALE;

            lpf += cut * bpf;
            bpf += cut * (osc - lpf - bpf * bw);
            lpf += cut * bpf;
            bpf += cut * (osc - lpf - bpf * bw);

            return lpf;
        }
    };

    struct Voice
    {
        float aenv, aenvdecay;
        float fenv, fenvdecay;
        float amp;
        float sampletime;
        float note;
        float* p;
        int rampcount;
        VoiceChannel channels[2];
        AudioPluginUtil::Random random;

        static inline float FreqFromNote(float note)
        {
            return 440.0f * powf(2.0f, (float)(note - 57) * ONE_OVER_12);
        }

        void NoteOn(int note, int velocity, float* p, float sampletime)
        {
            aenv = 1.0f;
            aenvdecay = 1.0f;
            fenv = 1.0f;
            fenvdecay = powf(0.0001f, sampletime / p[P_DECAY]);
            amp = velocity * ONE_OVER_127;
            rampcount = 0;
            channels[0].Reset();
            channels[1].Reset();
            this->p = p;
            this->sampletime = sampletime;
            this->note = (float)note;
            for (int i = 0; i < MAXOSCILLATORS; i++)
            {
                channels[0].phase[i] = random.Get();
                channels[1].phase[i] = random.Get();
            }
        }

        void NoteOff(int note, int velocity)
        {
            aenvdecay = powf(0.0001f, sampletime / p[P_RELEASE]);
            fenvdecay = aenvdecay;
        }

        inline float GetImportance() const
        {
            return aenv * fenv;
        }

        inline bool IsDonePlaying() const
        {
            return aenv < 0.001f;
        }

        inline void FrameSetup()
        {
            float st = sampletime * (const float)(0x100000000 / OVERSAMPLING);
            float dt1 = p[P_DETUNE1] + 0.5f * p[P_DETUNE2];
            float dt2 = p[P_DETUNE1] - 0.5f * p[P_DETUNE2];
            channels[0].freq = (UInt32)(FreqFromNote(note - dt1) * st);
            channels[1].freq = (UInt32)(FreqFromNote(note - dt2) * st);
            channels[0].detune = (UInt32)((FreqFromNote(note + dt1) * st - channels[0].freq) * ONE_OVER_MAXOSCILLATORS);
            channels[1].detune = (UInt32)((FreqFromNote(note + dt2) * st - channels[1].freq) * ONE_OVER_MAXOSCILLATORS);
            channels[0].mask = ((UInt32)AudioPluginUtil::FastFloor(p[P_TYPE] * 127) + 128) << 24;
            channels[1].mask = ((UInt32)AudioPluginUtil::FastFloor(p[P_TYPE] * 127) + 128) << 24;
        }

        inline void Process(float& l, float& r)
        {
            float cut = AudioPluginUtil::FastClip(p[P_CUTOFF] + p[P_CUTENV] * fenv, 0.0001f, 0.99f); cut = cut * cut * 0.707f;
            float bw = 1.0f - p[P_RESONANCE]; bw *= bw;
            float ramped_amp = aenv * amp;
            if (rampcount < RAMPSAMPLES)
                ramped_amp *= (++rampcount) * RAMPSCALE;
            l += channels[0].Process(cut, bw) * ramped_amp;
            r += channels[1].Process(cut, bw) * ramped_amp;
            aenv = aenv * aenvdecay + 1.0e-11f;
            fenv = fenv * fenvdecay + 1.0e-11f;
        }
    };

    struct SynthesizerChannel
    {
        Voice* voicepool[MAXVOICES];
        Voice* keys[128];
        float ctrl[128];
        int numvoices;

        void Init()
        {
            memset(this, 0, sizeof(*this));
            for (int n = 0; n < MAXVOICES; n++)
                voicepool[n] = new Voice();
        }

        ~SynthesizerChannel()
        {
            for (int n = 0; n < MAXVOICES; n++)
                delete voicepool[n];
        }

        Voice* AllocateVoice(int key)
        {
            Voice* v = keys[key];
            if (v != NULL)
                return v;

            if (numvoices < MAXVOICES)
                v = voicepool[numvoices++];
            else
            {
                v = voicepool[0];
                for (int i = 1; i < numvoices; i++)
                {
                    Voice* q = voicepool[i];
                    if (q->GetImportance() < v->GetImportance())
                        v = q;
                }
                for (int n = 0; n < 128; n++)
                    if (keys[n] == v)
                        keys[n] = NULL;
            }

            keys[key] = v;
            return v;
        }

        void NoteOn(int note, int velocity, float* p, float sampletime)
        {
            Voice* v = AllocateVoice(note);
            v->NoteOn(note, velocity, p, sampletime);
        }

        void NoteOff(int note, int velocity, float* p, float sampletime)
        {
            Voice* v = keys[note];
            if (v == NULL)
                return;
            v->NoteOff(note, velocity);
            if (keys[note] == v)
                keys[note] = NULL;
        }

        void Control(int index, int value, float* p, float sampletime)
        {
            ctrl[index] = value * ONE_OVER_127;
        }

        void Process(float* outbuffer, int length, int outchannels, float* p)
        {
            for (int k = 0; k < numvoices; k++)
            {
                Voice* v = voicepool[k];
                v->FrameSetup();
                float* dst = outbuffer;
                for (int n = 0; n < length; n++)
                {
                    v->Process(dst[0], dst[1]);
                    dst += outchannels;
                }
            }

            int i = 0;
            while (i < numvoices)
            {
                if (voicepool[i]->IsDonePlaying())
                {
                    for (int n = 0; n < 128; n++)
                        if (keys[n] == voicepool[i])
                            keys[n] = NULL;
                    Voice* t = voicepool[i];
                    voicepool[i] = voicepool[--numvoices];
                    voicepool[numvoices] = t;
                }
                else
                    ++i;
            }
        }
    };

    const int MAXPENDING = 8192;

    struct EffectData
    {
        float p[P_NUM];
        int arpkeys[128];
        int numpending;
        MIDI::MidiEvent pending[MAXPENDING];
        SynthesizerChannel synthchannel[MAXCHANNELS];
    };

    int InternalRegisterEffectDefinition(UnityAudioEffectDefinition& definition)
    {
        int numparams = P_NUM;
        definition.paramdefs = new UnityAudioParameterDefinition[numparams];
        AudioPluginUtil::RegisterParameter(definition, "Stream", "", 0.0f, MAXCHANNELS - 1, 0.0f, 100.0f, 1.0f, P_STREAM, "MIDI stream");
        AudioPluginUtil::RegisterParameter(definition, "Cutoff", "%", 0.0f, 1.0f, 0.1f, 100.0f, 1.0f, P_CUTOFF, "Cutoff frequency");
        AudioPluginUtil::RegisterParameter(definition, "Cutoff env", "%", 0.0f, 1.0f, 0.3f, 100.0f, 1.0f, P_CUTENV, "Cutoff envelope");
        AudioPluginUtil::RegisterParameter(definition, "Resonance", "%", 0.0f, 1.0f, 0.3f, 100.0f, 1.0f, P_RESONANCE, "Resonance amount");
        AudioPluginUtil::RegisterParameter(definition, "Decay time", "s", 0.0f, 10.0f, 0.1f, 1.0f, 1.0f, P_DECAY, "Envelope decay time");
        AudioPluginUtil::RegisterParameter(definition, "Release time", "s", 0.0f, 10.0f, 0.01f, 1.0f, 1.0f, P_RELEASE, "Envelope release time");
        AudioPluginUtil::RegisterParameter(definition, "Voice Detuning", "%", 0.0f, 1.0f, 0.03f, 100.0f, 1.0f, P_DETUNE1, "Voice detuning amount");
        AudioPluginUtil::RegisterParameter(definition, "Stereo Detuning", "%", 0.0f, 1.0f, 0.01f, 100.0f, 1.0f, P_DETUNE2, "Stereo detuning amount");
        AudioPluginUtil::RegisterParameter(definition, "Type", "%", 0.0f, 1.0f, 1.0f, 100.0f, 1.0f, P_TYPE, "Pulse wave to sawtooth mix");
        return numparams;
    }

    UNITY_AUDIODSP_RESULT UNITY_AUDIODSP_CALLBACK CreateCallback(UnityAudioEffectState* state)
    {
        static MIDI::MidiInput midiinput;
        EffectData* effectdata = new EffectData;
        memset(effectdata, 0, sizeof(EffectData));
        for (int n = 0; n < MAXCHANNELS; n++)
            effectdata->synthchannel[n].Init();
        state->effectdata = effectdata;
        AudioPluginUtil::InitParametersFromDefinitions(InternalRegisterEffectDefinition, effectdata->p);
        return UNITY_AUDIODSP_OK;
    }

    UNITY_AUDIODSP_RESULT UNITY_AUDIODSP_CALLBACK ReleaseCallback(UnityAudioEffectState* state)
    {
        EffectData* data = state->GetEffectData<EffectData>();
        delete data;
        return UNITY_AUDIODSP_OK;
    }

    UNITY_AUDIODSP_RESULT UNITY_AUDIODSP_CALLBACK SetFloatParameterCallback(UnityAudioEffectState* state, int index, float value)
    {
        EffectData* data = state->GetEffectData<EffectData>();
        if (index >= P_NUM)
            return UNITY_AUDIODSP_ERR_UNSUPPORTED;
        data->p[index] = value;
        return UNITY_AUDIODSP_OK;
    }

    UNITY_AUDIODSP_RESULT UNITY_AUDIODSP_CALLBACK GetFloatParameterCallback(UnityAudioEffectState* state, int index, float* value, char *valuestr)
    {
        EffectData* data = state->GetEffectData<EffectData>();
        if (index >= P_NUM)
            return UNITY_AUDIODSP_ERR_UNSUPPORTED;
        if (value != NULL)
            *value = data->p[index];
        if (valuestr != NULL)
            valuestr[0] = 0;
        return UNITY_AUDIODSP_OK;
    }

    int UNITY_AUDIODSP_CALLBACK GetFloatBufferCallback(UnityAudioEffectState* state, const char* name, float* buffer, int numsamples)
    {
        return UNITY_AUDIODSP_OK;
    }

    static void HandleEvent(EffectData* data, UInt32 msg, float sampletime)
    {
        int channel = msg & 15;
        int command = msg & 0xF0;
        int data1 = (msg >> 8) & 255;
        int data2 = (msg >> 16) & 255;
        SynthesizerChannel* synthchannel = &data->synthchannel[channel];
        switch (command)
        {
            case 0x90:
                if (data2 > 0)
                {
                    data->arpkeys[data1] = 1;
                    synthchannel->NoteOn(data1, data2, data->p, sampletime);
                    break;
                }
            case 0x80:
                data->arpkeys[data1] = 0;
                synthchannel->NoteOff(data1, data2, data->p, sampletime);
                break;
            case 0xB0:
                synthchannel->Control(data1, data2, data->p, sampletime);
                break;
            case 0xF0:
                if (channel == 8)
                {
                    // All sound off
                    MIDI::scheduledata.Clear();
                    data->numpending = 0;
                    memset(data->arpkeys, 0, sizeof(data->arpkeys));
                    for (int c = 0; c < 16; c++)
                    {
                        SynthesizerChannel* synthchannel = &data->synthchannel[c];
                        memset(synthchannel->keys, 0, sizeof(synthchannel->keys));
                        synthchannel->numvoices = 0;
                    }
                }
                break;
        }
    }

    UNITY_AUDIODSP_RESULT UNITY_AUDIODSP_CALLBACK ProcessCallback(UnityAudioEffectState* state, float* inbuffer, float* outbuffer, unsigned int length, int inchannels, int outchannels)
    {
        EffectData* data = state->GetEffectData<EffectData>();

        memset(outbuffer, 0, sizeof(float) * length * outchannels);

        float sampletime = 1.0f / (float)state->samplerate;

        MIDI::MidiEvent ev;
        while (MIDI::scheduledata.Read(ev))
        {
            if (ev.sample > state->currdsptick)
            {
                data->pending[data->numpending++] = ev;
                continue;
            }
            HandleEvent(data, ev.msg, sampletime);
        }

        while (MIDI::livedata.Read(ev.msg))
            HandleEvent(data, ev.msg, sampletime);

        UInt64 currtick = state->currdsptick;
        int samplesleft = length;
        while (samplesleft > 0)
        {
            UInt64 frameend = currtick + samplesleft;
            UInt64 nextevent = frameend;
            for (int n = 0; n < data->numpending; n++)
                if (data->pending[n].sample < nextevent)
                    nextevent = data->pending[n].sample;
            if (nextevent < currtick)
                nextevent = currtick;
            if (nextevent < frameend)
            {
                int i = 0, j = 0;
                while (i < data->numpending)
                {
                    MIDI::MidiEvent& ev = data->pending[i++];
                    if (ev.sample <= nextevent)
                        HandleEvent(data, ev.msg, sampletime);
                    else
                        data->pending[j++] = ev;
                }
                data->numpending = j;
            }
            int block = (int)(nextevent - currtick);
            if (block == 0)
                continue;
            if (block > samplesleft)
                block = samplesleft;
            for (int n = 0; n < MAXCHANNELS; n++)
            {
                SynthesizerChannel* synthchannel = &data->synthchannel[n];
                synthchannel->Process(outbuffer, block, outchannels, data->p);
            }
            outbuffer += block * outchannels;
            samplesleft -= block;
        }

        return UNITY_AUDIODSP_OK;
    }

    extern "C" UNITY_AUDIODSP_EXPORT_API void Synthesizer_AddMessage(UInt64 sample, int msg)
    {
        MIDI::MidiEvent ev;
        ev.sample = sample;
        ev.msg = msg;
        MIDI::scheduledata.Feed(ev);
    }

    extern "C" UNITY_AUDIODSP_EXPORT_API void Synthesizer_KillAll()
    {
        Synthesizer_AddMessage(0, 0xF8);
    }
}

#endif
