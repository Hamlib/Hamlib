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
        }

        switch (buf[0])
        {
        case '?': printf("Query %c\n", buf[1]); break;

        case '*': printf("Set %c\n", buf[1]); break;

        default: printf("Unknown cmd=%02x\n", buf[4]);
        }
    }

    return 0;
}
