// can run this using rigctl/rigctld and socat pty devices
#define _XOPEN_SOURCE 700
// since we are POSIX here we need this
#include <stdio.h>
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
_getmyline(int fd, char *buf)
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
    char buf[BUFSIZE];
    int fd = openPort(argv[1]);

    while (1)
    {
        buf[0] = 0;

        if (_getmyline(fd, buf) > 0) { printf("Cmd:%s\n", buf); }

        if (strncmp(buf, "BC", 2) == 0)
        {
            SNPRINTF(buf, sizeof(buf), "BC %d,%d%c", vfo, vfo_tx, 0x0d);
            printf("R:%s\n", buf);
            write(fd, buf, strlen(buf));
            continue;
        }
        else if (strncmp(buf, "FO", 2) == 0)
        {
            char vfon = buf[3];
            int frequency;
            const char tone_frequency[] = "10"; // 94.8
            const char ctcss_frequency[] = "05"; // 79,7
            const char dcs_frequency[] = "016"; // 114

            if (vfon == '0') {
                frequency = (int)freqA;
            } else {
                frequency = (int)freqB;
            }
            SNPRINTF(buf, sizeof(buf), "FO %c,%.10d,0,0,0,0,0,0,%.2s,%.2s,%.3s,00000000,0%c",
                     vfon, frequency, tone_frequency, ctcss_frequency, dcs_frequency, 0x0d);

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
