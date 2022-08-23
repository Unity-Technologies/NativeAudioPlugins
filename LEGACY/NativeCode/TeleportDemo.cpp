#include <stdlib.h>
#include <stdio.h>
#include <math.h>

#include "TeleportLib.cpp"

#if defined(__MACH__) || defined(__APPLE__) || defined(__linux__)
    #define UNITY_WIN 0
    #define SLEEP(t) usleep(t * 1000.0f)
#else
    #define UNITY_WIN 1
    #define SLEEP(t)//Sleep(t)
#endif

int main(int argc, char** argv)
{
    bool ishost = (argc < 2);

    const int bufsize = 1024;
    float buf[bufsize];
    float sr = 44100.0f;

    if (ishost)
    {
        fprintf(stdout, "Starting host loop\n");
        fflush(stdout);

        float phase = 0.0f, lfophase = 0.0f;
        while (1)
        {
            float params[4];
            for (int n = 0; n < 4; n++)
                if (TeleportGetParameter(0, n, &params[n]))
                    printf("Parameter %d changed to %f\n", n, params[n]);

            float lfofreq = (0.5f + params[1] * 10.0f) / sr;
            float basefreq = (200.0f + params[0] * 500.0f) / sr;

            int numbuffered = TeleportGetNumBuffered(0);
            if (numbuffered < 8000)
            {
                for (int n = 0; n < bufsize; n++)
                {
                    buf[n] = sinf(2.0f * 3.14159265f * phase);
                    phase += basefreq * (1.0f + 0.5f * sinf(2.0f * 3.14159265f * lfophase));
                    phase -= floorf(phase);
                    lfophase += lfofreq;
                    lfophase -= floorf(lfophase);
                }
                int writepos = TeleportFeed(0, buf, bufsize);
                printf("Write pos: %.3f\n", writepos / (float)(2 * sr));
            }
            SLEEP(1);
        }
    }
    else
    {
        fprintf(stdout, "Starting client loop\n");
        fflush(stdout);

        while (1)
        {
            int readpos = TeleportRead(0, buf, bufsize);
            printf("Read pos: %.3f\n", readpos / (float)(2 * sr));
            SLEEP(1);
        }
    }

    return 0;
}
