xcodebuild -project Xcode/AudioPluginDemo.xcodeproj
rm TeleportDemoOSX
g++ TeleportDemo.cpp -o TeleportDemoOSX
./TeleportDemoOSX
