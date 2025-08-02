// can run this using rigctl/rigctld and socat pty devices
#define _XOPEN_SOURCE 700
// since we are POSIX here we need this
#if 0
struct ip_mreq
{
    int dummy;
};
#endif

#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>
#include <string.h>
#include <unistd.h>
#include "hamlib/rig.h"

#define BUFSIZE 256

int mysleep = 20;

float freqA = 14074000;
float freqB = 14074500;
int filternum = 7;
int datamode = 0;
int vfo, vfo_tx, ptt, ptt_data, ptt_mic, ptt_tune;

int
getmyline(int fd, char *buf)
{
    char c;
    int i = 0;
    memset(buf, 0, BUFSIZE);

    while (read(fd, &c, 1) > 0)
    {
        buf[i++] = c;

        if (c == 0x0d) { return strlen(buf); }
    }

    if (strlen(buf) == 0) { hl_usleep(10 * 1000); }

    return strlen(buf);
}

#include "sim.h"


int main(int argc, char *argv[])
{
    char buf[256];
    int fd = openPort(argv[1]);

    while (1)
    {
        buf[0] = 0;

        if (getmyline(fd, buf) > 0) { printf("Cmd:%s\n", buf); }

        if (strncmp(buf, "BC", 2) == 0)
        {
            SNPRINTF(buf, sizeof(buf), "BC %d %d%c", vfo, vfo_tx, 0x0d);
            printf("R:%s\n", buf);
            write(fd, buf, strlen(buf));
            continue;
        }
        else if (strncmp(buf, "FO", 2) == 0)
        {
            if (buf[3] == '0')
            {
                SNPRINTF(buf, sizeof(buf), "FO 0 %d%c", freqA, 0x0d);
            }
            else
            {
                SNPRINTF(buf, sizeof(buf), "FO 1 %d%c", freqB, 0x0d);
            }

            printf("R:%s\n", buf);
            write(fd, buf, strlen(buf));
            continue;
        }
        else if (strlen(buf) > 0)
        {
            fprintf(stderr, "Unknown command: %s\n", buf);
        }
    }

    return 0;
}
