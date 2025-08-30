// can run this using rigctl/rigctld and socat pty devices
#define _XOPEN_SOURCE 700
// since we are POSIX here we need this
#include <stdio.h>

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
            continue;
        }

        switch (buf[4])
        {
        case 0x01:
            printf("Split\n");
            break;

        case 0x05:
            printf("Select VFO\n");
            break;

        case 0x0a:
            printf("Set Main freq\n");
            break;

        case 0x10:
            printf("Info\n");
            break;

        case 0x0c:
            printf("Set mode\n");
            break;

        case 0x0e:
            printf("Pacing\n");
            break;

        case 0x0f:
            printf("Tx\n");
            break;

        case 0x83:
            printf("Full Duplex Rx Mode\n");
            break;

        case 0x8a:
            printf("Set Sub freq\n");
            break;

        case 0x8b:
            printf("Bandwidth\n");
            break;

        case 0xf7:
            printf("Read meter\n");
            break;

        default: printf("Unknown cmd=%02x\n", buf[4]);
        }

        fflush(stdout);
    }

    return 0;
}
