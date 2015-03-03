// In order to compile the AudioUnit Teleport plugin you must first download the file
// https://developer.apple.com/library/mac/samplecode/CoreAudioUtilityClasses/CoreAudioUtilityClasses.zip
// and extract it in the NativeCode/Xcode/AUTeleport/CoreAudioUtilityClasses subfolder
// I've used the 2014 version of CoreAudioUtilityClasses which require Xcode 5.0.2 or later.

#include "AUEffectBase.h"
#include "AUTeleportVersion.h"
#include "../../TeleportLib.cpp"

const int kNumberOfParameters = 1;

enum Parameters
{
    kParameter_Target,
};

class AUTeleport : public AUEffectBase
{
public:
    AUTeleport(AudioUnit component);
    virtual OSStatus ProcessBufferLists(AudioUnitRenderActionFlags& ioActionFlags, const AudioBufferList& inBuffer, AudioBufferList& outBuffer, UInt32 inFramesToProcess);
    virtual ComponentResult GetParameterValueStrings(AudioUnitScope inScope, AudioUnitParameterID inParameterID, CFArrayRef* outStrings);
    virtual ComponentResult GetParameterInfo(AudioUnitScope inScope, AudioUnitParameterID inParameterID, AudioUnitParameterInfo& outParameterInfo);
    virtual ComponentResult GetPropertyInfo(AudioUnitPropertyID inID, AudioUnitScope inScope, AudioUnitElement inElement, UInt32& outDataSize, Boolean& outWritable);
    virtual ComponentResult GetProperty(AudioUnitPropertyID inID, AudioUnitScope inScope, AudioUnitElement inElement, void* outData);
    virtual bool SupportsTail() { return true; }
    virtual ComponentResult Version() { return kAUTeleportVersion; }
    virtual ComponentResult GetPresets(CFArrayRef* outData) const;
    float interleavebuf[0x10000];
};

AUTeleport::AUTeleport(AudioUnit component) : AUEffectBase(component)
{
    CreateElements();
    Globals()->UseIndexedParameters(kNumberOfParameters);
    SetParameter(kParameter_Target, 0);
}

ComponentResult AUTeleport::GetParameterInfo(AudioUnitScope inScope, AudioUnitParameterID inParameterID, AudioUnitParameterInfo& outParameterInfo)
{
    ComponentResult result = noErr;
    outParameterInfo.flags = kAudioUnitParameterFlag_IsWritable | kAudioUnitParameterFlag_IsReadable;

    if (inScope == kAudioUnitScope_Global)
    {
        switch (inParameterID)
        {
            case kParameter_Target:
                AUBase::FillInParameterName(outParameterInfo, CFSTR("Target"), false);
                outParameterInfo.unit           = kAudioUnitParameterUnit_Indexed;
                outParameterInfo.minValue       = 0;
                outParameterInfo.maxValue       = Teleport::NUMSTREAMS - 1;
                outParameterInfo.defaultValue   = 0;
                break;

            default:
                result = kAudioUnitErr_InvalidParameter;
                break;
        }
    }
    else
    {
        result = kAudioUnitErr_InvalidParameter;
    }
    return result;
}

ComponentResult AUTeleport::GetParameterValueStrings(AudioUnitScope inScope, AudioUnitParameterID inParameterID, CFArrayRef* outStrings)
{
    return kAudioUnitErr_InvalidParameter;
}

ComponentResult AUTeleport::GetPropertyInfo(AudioUnitPropertyID inID, AudioUnitScope inScope, AudioUnitElement inElement, UInt32& outDataSize, Boolean& outWritable)
{
    return AUEffectBase::GetPropertyInfo(inID, inScope, inElement, outDataSize, outWritable);
}

ComponentResult AUTeleport::GetProperty(AudioUnitPropertyID inID, AudioUnitScope inScope, AudioUnitElement inElement, void* outData)
{
    return AUEffectBase::GetProperty(inID, inScope, inElement, outData);
}

ComponentResult AUTeleport::GetPresets(CFArrayRef* outData) const
{
    if (outData == NULL)
        return noErr;

    *outData = (CFArrayRef)CFArrayCreateMutable(NULL, 0, NULL);
    return noErr;
}

OSStatus AUTeleport::ProcessBufferLists(AudioUnitRenderActionFlags& ioActionFlags, const AudioBufferList& inBuffer, AudioBufferList& outBuffer, UInt32 inFramesToProcess)
{
    int target = (int)GetParameter(kParameter_Target);
    for (int c = 0; c < inBuffer.mNumberBuffers; c++)
    {
        float* src = (float*)inBuffer.mBuffers[c].mData;
        float* dst = interleavebuf + c;
        for (int n = 0; n < inFramesToProcess; n++)
        {
            *dst = *src++;
            dst += inBuffer.mNumberBuffers;
        }
    }
    TeleportFeed(target, interleavebuf, inBuffer.mNumberBuffers * inFramesToProcess);
    return noErr;
}

AUDIOCOMPONENT_ENTRY(AUBaseFactory, AUTeleport)
