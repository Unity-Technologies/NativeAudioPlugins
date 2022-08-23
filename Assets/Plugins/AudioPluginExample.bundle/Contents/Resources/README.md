# A Revised SDK for Unity Naive Audio Plug-ins

This repository contains a proposal for an updated Unity Native Audio Plugin SDK.

## Why

Working with the original SDK, I found that it is quite convoluted and exposes
an unnecessary amount of the low-level C API to users. Furthermore, it uses the
C macro preprocessor extensively which is fragile and cumbersome to debug.

Moreover, the project is somewhat of a mouthful with almost 30 audio effects and
instruments of varying complexity. A lot of those plug-ins are not
production-ready as they do memory allocation and mutex locking on the audio
thread. This includes the convolution reverb. In my opinion, a leaner project
with a few, simple plug-ins that demonstrate the SDK, is a better approach,
provided our goal is to teach users how the SDK works.

Also, right now the 'AudioPluginUtil' component is a frankenstein mash-up of
essential functions related to the SDK and convenience functions for doing
complex number algebra (why don't we use std::complex?!), Fourier transforms,
mutex locking (users should probably use std::mutex instead), and other stuff.

Finally, the original SDK needs some love anyway, as the Xcode project no longer
compiles out-of-the-box (haven't been able to test the VS project).

## Approach

I've created a clean C++17 layer that completely shields the low-level API from
the user. It declares a handful of interfaces that users implement and then the
SDK handles all low-level stuff behind the scenes. All of the SDK that users
need to understand is contained in a single header,
'UnityAudioPluginInterface.hpp', which is about 100 lines of code. Having a
clean and self-contained C++ interface simplifies the mental burden that we put
on our users. Also, getting rid of all the macro-preprocessor non-sense and
weird callbacks such as 'AudioPluginUtil::RegisterParameter' from the original
SDK, makes plug-ins easier to maintain and debug.

On top of the C++ interface, I have implemented two simple plug-ins, an
attenuator and a noise generator. Both are very simple plugins. We could probably
do with an extra and more complicated plug-in with multiple parameters and more
advanced DSP.

I have also created a Unity project, an Xcode project (for macOS) and a Visual
Studio project (for Windows) and tested that the code compiles on both platforms.
On macOS, I have verified that the plug-ins can load and that they work as expected.
I haven't been able to do this on Windows as I don't have a Windows machine yet.

NOTE: I haven't made any changes to the low-level C API. (So far!)

## Unclarified Points

1. We need to discuss what version of Unity / Xcode / Visual Studio we want to
use for the SDK.

2. The way parameters is exposed in the new SDK via a callback function that
returns a vector of parameter descriptors (in the 'EntryPoint' struct) is not
as userfriendly as it could be.

3. Is it ok to use the C++ Standard Library (STL)?

## Next Steps

1. We need to implement a sane way to transfer samples from the managed code to
a plugin. The way it is done now in the convolution reverb example is a bit of
a hack and uses locks and memory allocation on the audio thread.

2. Better functionality for sending events to plugins from the managed code
would be useful for any kind of procedural audio that we want to trigger from
within a game.

3. This one is a little bit complicated! If we made it possible to pass data
to a 'UnityAudioEffect_CreateCallback', e.g. via the 'UnityAudioEffectState'
struct, we can most likely merge the 'UnityAudioPluginInterface-Raw.hpp' and
'UnityAudioPluginInterface-Raw.inl' headers into the
'UnityAudioPluginInterface.cpp' file. As of now, we need to include both of the
'Raw'-files in the 'EntryPoints.cpp'-file, which is annoying. Getting rid of
those two files, 'UnityAudioPluginInterface.hpp' would be the only file that
users ever need to think about.

4. A more complex demo effect, such as an equalizer or something similar, with
multiple parameters would be nice.
