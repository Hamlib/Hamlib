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
        case 0x00:
            printf("CAT On\n");
            break;

        case 0x80:
            printf("CAT Off\n");
            break;

        case 0x01:
            printf("FREQ_SET\n");
            break;

        case 0x07:
            printf("MODE_SET\n");
            break;

        case 0x0e:
            printf("Full Duplex On\n");
            break;

        case 0x8e:
            printf("Full Duplex Off\n");
            break;

        case 0x08:
            printf("Tx\n");
            break;

        case 0x88:
            printf("Rx\n");
            break;

        case 0x17:
            printf("Full Duplex Rx Mode\n");
            break;

        case 0x27:
            printf("Full Duplex Tx Mode\n");
            break;

        case 0x1e:
            printf("Full Duplex Rx Freq\n");
            break;

        case 0x2e:
            printf("Full Duplex Tx Freq\n");
            break;

        default: printf("Unknown cmd=%02x\n", buf[4]);
        }

        fflush(stdout);
    }

    return 0;
}
