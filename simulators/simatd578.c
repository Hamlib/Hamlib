// can run this using rigctl/rigctld and socat pty devices
#define _XOPEN_SOURCE 700
// since we are POSIX here we need this
#include <stdio.h>
#include <string.h>
#include <unistd.h>

#include "sim.h"

#define BUFSIZE 256

float freqA = 14074000;
float freqB = 14074500;
char tx_vfo = '0';
char rx_vfo = '0';
char modeA = '1';
char modeB = '1';
int width_main = 500;
int width_sub = 700;
int ptt = 0;
int curr_vfo = 0;


int
_getmyline(int fd, unsigned char *buf)
{
    unsigned char c;
    int i = 0;
    int n = 0;
    memset(buf, 0, BUFSIZE);

    // seems the anytone only gives 8-byte commands and 1-byte responses
    do
    {
        int bytes = read(fd, &c, 1);

        if (bytes > 0)
        {
            buf[i++] = c;
        }
    }
    while (c != 0x0a);

    printf("n=%d", i);
    for (n = 0; n < i; ++n) { printf(" %02x", buf[n]); }
    printf("\n");

    return i;
}


int main(int argc, char *argv[])
{
    unsigned char buf[BUFSIZE], buf2[256];

    int fd = openPort(argv[1]);

    while (1)
    {
        int bytes = _getmyline(fd, buf);
        int n = 0;

        if (bytes == 0)
        {
            continue;
        }

        if (bytes != 8)
        {
            printf("Not 8 bytes?  bytes=%d\n", bytes);
        }

        switch (buf[0])
        {
        case 0x41:
            if (buf[4] == 0x00) // set ptt
            {
                if (buf[1] == 1)
                {
                    ptt = 1;
                    printf("PTT ON\n");
                }
                else
                {
                    ptt = 0;
                    printf("PTT OFF\n");
                }

                buf[0] = 0x06;
                n = write(fd, buf, 1);
            }

            if (buf[4] == 0x20) // get vfo
            {
                printf("Get VFO curr_vfo=%d\n", curr_vfo);

                if (curr_vfo == 1)
                {
                    curr_vfo = 0;
                }
                else
                {
                    curr_vfo = 1;
                }

                printf("Get VFO curr_vfo=%d\n", curr_vfo);
                buf2[0] = 0xaa;
                buf2[1] = 0x53;
                buf2[2] = 0x00;
                buf2[3] = 0x00;
                buf2[4] = 0x00;
                buf2[5] = 0x01;
                buf2[6] = 0x01;
                buf2[7] = 0x00;
                buf2[8] = curr_vfo;
                buf2[9] = 0x00;
                buf2[10] = 0x10;
                buf2[11] = 0x00;
                buf2[12] = 0x00;
                buf2[13] = 0x00;
                buf2[14] = 0x00;
                buf2[15] = 0x00;
                buf2[16] = 0x06;

                n = write(fd, buf2, 17);
            }

            break;

        case 0x06:
            buf[0] = 0x06;
            n = write(fd, buf, 1);
            break;

        default: printf("Unknown cmd=%02x\n", buf[0]); continue;
        }

        printf("%d bytes returned\n", n);
    }

    return 0;
}
