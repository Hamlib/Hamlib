// can run this using rigctl/rigctld and socat pty devices
#define _XOPEN_SOURCE 700
// since we are POSIX here we need this
#include <stdio.h>
#include <unistd.h>

#include "sim.h"

float freqA = 14074000;
float freqB = 14074500;
char tx_vfo = '0';
char rx_vfo = '0';
char modeA = '1';
char modeB = '1';
int width_main = 500;
int width_sub = 700;


int main(int argc, char *argv[])
{
    unsigned char buf[BUFSIZE];

    int fd = openPort(argv[1]);

    while (1)
    {
        int bytes = getmyline5(fd, buf);

        if (bytes == 0)
        {
            continue;
        }

        if (bytes != 5)
        {
            printf("Not 5 bytes?  bytes=%d\n", bytes);
        }

        switch (buf[4])
        {
        case 0x01:
            printf("SPLIT\n");
            break;

        case 0x02:
            printf("MEMORY\n");
            break;

        case 0x03:
            printf("VFO_TO_M\n");
            break;

        case 0x04:
            printf("DLOCK\n");
            break;

        case 0x05:
            printf("A_BVFO\n");
            break;

        case 0x06:
            printf("M_TO_VFO\n");
            break;

        case 0x07:
            printf("UP500K\n");
            break;

        case 0x08:
            printf("DN500K\n");
            break;

        case 0x09:
            printf("CLAR\n");
            break;

        case 0x0a:
            printf("FREQ_SET\n");
            break;

        case 0x0c:
            printf("MODE_SET\n");
            break;

        case 0x0e:
            printf("PACING\n");  // no reply
            break;

        case 0x0f:
            printf("PTT\n");
            break;

        case 0x10:
            printf("UPDATE\n");
            buf[0] = 0x01; // status byte -- split on
            buf[1] = 0x14;
            buf[2] = 0x07;
            buf[3] = 0x41;
            buf[4] = 0x02;
            write(fd, buf, 5);
            buf[0] = 0;

            for (int i = 5; i < 340; ++i) { write(fd, buf, 1); }

            break;

        default: printf("Unknown cmd=%02x\n", buf[4]);
        }
    }

    return 0;
}
