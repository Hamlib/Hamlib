// can run this using rigctl/rigctld and socat pty devices
#define _XOPEN_SOURCE 700
// since we are POSIX here we need this
#include <stdio.h>
#include <string.h>
#include <unistd.h>

#define BUFSIZE 256

float freqA = 14074000;
float freqB = 14074500;
char tx_vfo = '0';
char rx_vfo = '0';
char modeA = '1';
char modeB = '1';
int width_main = 500;
int width_sub = 700;


int
_getmyline(int fd, unsigned char *buf)
{
    unsigned char c;
    int i = 0;
    int n = 0;
    memset(buf, 0, BUFSIZE);

    while (i < 5 && read(fd, &c, 1) > 0)
    {
        buf[i++] = c;
        n++;
    }

    printf("n1=%d %02x %02x %02x %02x %02x\n", n, buf[0], buf[1], buf[2], buf[3],
           buf[4]);

    i = buf[4];  // length of packet
    n = read(fd, &buf[5], i);
    printf("read %d bytes\n", n);

    if (n == 0) { return 0; }

    n += 5;
    printf("n2=%d", n);

    for (i = 0; i < n; ++i) { printf(" x%02x", buf[i]); }

    printf("\n");
    return n;
}

#include "sim.h"


int main(int argc, char *argv[])
{
    unsigned char buf[BUFSIZE];

    int fd = openPort(argv[1]);

    while (1)
    {
        int bytes = _getmyline(fd, buf);

        if (bytes == 0)
        {
            continue;
        }

        switch (buf[5])
        {
        case 0x07: printf("PTT=%d\n", buf[6]); write(fd, buf, 9); break;

        case 0x0c: printf("Power=%d\n", buf[6]); break;

        default: printf("Unknown cmd=%02x\n", buf[4]);
        }
    }

    return 0;
}
