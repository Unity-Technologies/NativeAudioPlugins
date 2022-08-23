# README #

This repository hosts the official Unity Native Audio Plugin SDK. It is a standard Unity project that comes with prebuilt plugin effects and custom GUIs for OSX and Windows. Full source code for all plugin effects is provided, both for the native code (Xcode/VisualStudio C++ projects code in the NativeCode folder) as well as the custom GUIs (MonoDevelop project in the GUICode folder).

### What license are these demos shipped under? ###
Like the rest of the Unity open source projects, everything in this repository is released under an MIT/X11 license.

### Additional dependencies ###
While all other scenes in the project are self-contained and self-running, the “teleport” scene demonstrates how to send audio from external applications into Unity. This can either happen through the NativeCode/TeleportDemo.cpp command line program which can act both as a sender or receiver or via the included AUTeleport demo in NativeCode/Xcode/AUTeleport which shows how to send audio from an AudioUnits plugin that can be inserted in the output bus of sequencers like Logic and send audio data from there into the native audio plugin on the Unity side. A prebuilt version of the plugin is included, but if you want to build it yourself, you need to download the additional “Core Audio Utility Classes” source code from Apple as mentioned in NativeCode/Xcode/AUTeleport/readme.txt

### Further information ###
For diagrams describing some of the demos and techniques used see [the slides from Unite Europe 2015](http://files.unity3d.com/janm/UniteEurope2015.pdf)