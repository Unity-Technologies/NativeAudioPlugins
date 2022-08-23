#if defined(WIN32) || defined(_WIN32) || defined(__WIN32__) || defined(_WIN64)
#define UNITY_WIN 1
#if defined(WINAPI_FAMILY) && (WINAPI_FAMILY == WINAPI_FAMILY_APP)
#    define UNITY_WINRT 1
#endif
#elif defined(__MACH__) || defined(__APPLE__)
#define UNITY_OSX 1
#elif defined(__ANDROID__)
#define UNITY_ANDROID 1
#elif defined(__linux__)
#define UNITY_LINUX 1
#endif

#ifndef UNITY_WIN
#define UNITY_WIN 0
#endif

#ifndef UNITY_WINRT
#define UNITY_WINRT 0
#endif

#ifndef UNITY_OSX
#define UNITY_OSX 0
#endif

#ifndef UNITY_ANDROID
#define UNITY_ANDROID 0
#endif

#ifndef UNITY_LINUX
#define UNITY_LINUX 0
#endif

#include <stdio.h>

#if UNITY_OSX | UNITY_LINUX
    #include <sys/mman.h>
    #include <sys/types.h>
    #include <sys/stat.h>
    #include <fcntl.h>
    #include <errno.h>
    #include <unistd.h>
    #include <string.h>
#elif UNITY_WIN
    #include <windows.h>
#endif

#include "TeleportLib.h"

#if defined(__GNUC__) || defined(__SNC__)
#define TELEPORT_ALIGN(val) __attribute__((aligned(val))) __attribute__((packed))
#elif defined(_MSC_VER)
#define TELEPORT_ALIGN(val) __declspec(align(val))
#else
#define TELEPORT_ALIGN(val)
#endif

namespace Teleport
{
    const int NUMPARAMS = 4;
    const int NUMSTREAMS = 8;

    struct Parameter
    {
        float value;
        int changed;
    } TELEPORT_ALIGN(4);

    struct Stream
    {
        enum { LENGTH = 2 * 44100 };

        Parameter params[NUMPARAMS];

        int readpos;
        int writepos;
        float buffer[LENGTH];

        inline bool Read(float& val)
        {
            int r = readpos;
            if (r == writepos)
                return false;
            readpos = (r == LENGTH - 1) ? 0 : (r + 1);
            val = buffer[r];
            return true;
        }

        inline bool Feed(float input)
        {
            int w = (writepos == LENGTH - 1) ? 0 : (writepos + 1);
            buffer[w] = input;
            writepos = w;
            return true;
        }

        inline int GetNumBuffered() const
        {
            int b = writepos - readpos;
            if (b < 0)
                b += LENGTH;
            return b;
        }
    } TELEPORT_ALIGN(4);

    struct SharedMemory
    {
        Stream streams[NUMSTREAMS];
    };

    class SharedMemoryHandle
    {
    protected:
        SharedMemory* data;
#if UNITY_WIN
        HANDLE hMapFile;
#endif

    public:
        SharedMemoryHandle()
        {
            for (int attempt = 0; attempt < 10; attempt++)
            {
                bool clearmemory = true;

#if UNITY_WIN

#if !UNITY_WINRT
                char filename[1024];
                sprintf_s(filename, "UnityAudioTeleport%d", attempt);
                hMapFile = CreateFileMapping(INVALID_HANDLE_VALUE, NULL, PAGE_READWRITE, 0, sizeof(SharedMemory), filename);
#else
                wchar_t filePath[MAX_PATH];
                GetTempPathW(MAX_PATH, filePath);

                wchar_t fileName[30];
                swprintf_s(fileName, L"UnityAudioTeleport%d", attempt);

                wcscat_s(filePath, fileName);                
                hMapFile = CreateFileMappingFromApp(INVALID_HANDLE_VALUE, NULL, PAGE_READWRITE, sizeof(SharedMemory), filePath);
#endif
                if (hMapFile == NULL)
                {
                    clearmemory = false;
#if !UNITY_WINRT
                    hMapFile = OpenFileMapping(FILE_MAP_ALL_ACCESS, FALSE, filename);
#else
                    hMapFile = OpenFileMappingFromApp(FILE_MAP_ALL_ACCESS, FALSE, filePath);
#endif
                }
                if (hMapFile == NULL)
                {
                    printf("Could not create file mapping object (%d).\n", GetLastError());
                    continue;
                }

                data = (SharedMemory*)MapViewOfFile(hMapFile, FILE_MAP_ALL_ACCESS, 0, 0, sizeof(SharedMemory));
                if (data == NULL)
                {
                    printf("Could not map view of file (%d).\n", GetLastError());
                    CloseHandle(hMapFile);
                    continue;
                }

#else
                char filename[1024];

#if UNITY_LINUX
                // a shared memory object should be identified by a name of the form /somename;
                // that is, a null-terminated string of up to NAME_MAX (i.e.,  255)  characters consisting of an initial slash,
                // followed by one or more characters, none of which are slashes.
                sprintf(filename, "/UnityAudioTeleport%d", attempt);
#else
                sprintf(filename, "/tmp/UnityAudioTeleport%d", attempt);
#endif
                clearmemory = (access(filename, F_OK) == -1);
                int handle = shm_open(filename, O_RDWR | O_CREAT, 0777);
                if (handle == -1)
                {
                    fprintf(stderr, "Open failed: %s\n", strerror(errno));
                    continue;
                }

                if (ftruncate(handle, sizeof(SharedMemory)) == -1)
                {
                    fprintf(stderr, "ftruncate error (ignored)\n");
                    //continue;
                }

                data = (SharedMemory*)mmap(0, sizeof(SharedMemory), PROT_READ | PROT_WRITE, MAP_SHARED, handle, 0);
                if (data == (void*)-1)
                {
                    fprintf(stderr, "mmap failed\n");
                    continue;
                }

                //close(handle);
                //shm_unlink(filename);

#endif

                if (clearmemory)
                    memset(data, 0, sizeof(SharedMemory));

                break; // intentional (see continue's above)
            }
        }

        ~SharedMemoryHandle()
        {
#if UNITY_WIN

            UnmapViewOfFile(data);
            CloseHandle(hMapFile);

#else

            if (data)
            {
                munmap(data, sizeof(SharedMemory));
            }

#endif
        }

        inline SharedMemory* operator->() const { return data; }
    };

    inline SharedMemoryHandle& GetSharedMemory()
    {
        static SharedMemoryHandle shared;
        return shared;
    }
}

extern "C" UNITY_AUDIODSP_EXPORT_API int TeleportFeed(int stream, float* samples, int numsamples)
{
    Teleport::Stream& s = Teleport::GetSharedMemory()->streams[stream];
    for (int n = 0; n < numsamples; n++)
        s.Feed(samples[n]);
    return s.writepos;
}

extern "C" UNITY_AUDIODSP_EXPORT_API int TeleportRead(int stream, float* samples, int numsamples)
{
    Teleport::Stream& s = Teleport::GetSharedMemory()->streams[stream];
    for (int n = 0; n < numsamples; n++)
        if (!s.Read(samples[n]))
            samples[n] = 0.0f;
    return s.readpos;
}

extern "C" UNITY_AUDIODSP_EXPORT_API int TeleportGetNumBuffered(int stream)
{
    Teleport::Stream& s = Teleport::GetSharedMemory()->streams[stream];
    return s.GetNumBuffered();
}

extern "C" UNITY_AUDIODSP_EXPORT_API int TeleportSetParameter(int stream, int index, float value)
{
    Teleport::Stream& s = Teleport::GetSharedMemory()->streams[stream];
    s.params[index].changed = 1;
    s.params[index].value = value;
    return 1;
}

extern "C" UNITY_AUDIODSP_EXPORT_API int TeleportGetParameter(int stream, int index, float* value)
{
    Teleport::Stream& s = Teleport::GetSharedMemory()->streams[stream];
    *value = s.params[index].value;
    int changed = s.params[index].changed;
    s.params[index].changed = 0;
    return changed;
}
