// can run this using rigctl/rigctld and socat pty devices
// gcc -o simft1000 simft1000.c
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
#include "../include/hamlib/rig.h"

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
getmyline(int fd, unsigned char *buf)
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

    printf("n=%d %02x %02x %02x %02x %02x\n", n, buf[0], buf[1], buf[2], buf[3],
           buf[4]);
    return n;
}

#if defined(WIN32) || defined(_WIN32)
int openPort(char *comport) // doesn't matter for using pts devices
{
    int fd;
    fd = open(comport, O_RDWR);

    if (fd < 0)
    {
        perror(comport);
    }

    return fd;
}

#else
int openPort(char *comport) // doesn't matter for using pts devices
{
    int fd = posix_openpt(O_RDWR);
    char *name = ptsname(fd);

    if (name == NULL)
    {
        perror("pstname");
        return -1;
    }

    printf("name=%s\n", name);

    if (fd == -1 || grantpt(fd) == -1 || unlockpt(fd) == -1)
    {
        perror("posix_openpt");
        return -1;
    }

    return fd;
}
#endif



int main(int argc, char *argv[])
{
    unsigned char buf[256];

again:
    int fd = openPort(argv[1]);

    while (1)
    {
        int bytes = getmyline(fd, buf);

        if (bytes == 0)
        {
            close(fd);
            goto again;
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
