#pragma once

extern "C"
{
typedef UNITY_AUDIODSP_EXPORT_API int (*TeleportFeedFunc)(int stream, float* samples, int numsamples);
typedef UNITY_AUDIODSP_EXPORT_API int (*TeleportReadFunc)(int stream, float* samples, int numsamples);
typedef UNITY_AUDIODSP_EXPORT_API int (*TeleportGetNumBufferedFunc)(int stream);
typedef UNITY_AUDIODSP_EXPORT_API int (*TeleportSetParameterFunc)(int stream, int index, float value);
typedef UNITY_AUDIODSP_EXPORT_API int (*TeleportGetParameterFunc)(int stream, int index, float* value);
};
