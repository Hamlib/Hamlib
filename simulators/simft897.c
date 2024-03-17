// can run this using rigctl/rigctld and socat pty devices
// gcc -o simft897 simft897.c
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
    int n;


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
        }

        switch (buf[4])
        {
        case 0x00: printf("LOCK ON\n"); break;

        case 0x80: printf("LOCK OFF\n"); break;

        case 0x08: printf("PTT ON\n"); break;

        case 0x88: printf("PTT OFF\n"); break;

        case 0x07: printf("MODE\n"); break;

        case 0x05: printf("CLAR ON\n"); break;

        case 0x85: printf("CLAR OFF\n"); break;

        case 0xF5: printf("FREQ\n"); break;

        case 0x81: 
            rx_vfo = rx_vfo == 0? 1: 0;
            printf("VFO TOGGLE to %dE\n", rx_vfo); 
            break;


        case 0x02: printf("SPLIT ON\n"); break;

        case 0x82: printf("SPLIT OFF\n"); break;

        case 0x09: printf("REPEATER SHIFT\n"); break;

        case 0xF9: printf("REPEATER FREQ\n"); break;

        case 0x0A: printf("CTCSS/DCS MODE\n"); break;

        case 0x0B: printf("CTCSS TONE\n"); break;

        case 0x0C: printf("DCS CODE\n"); break;

        case 0xE7: printf("READ RX STATUS\n"); break;

        case 0xF7: 
            printf("READ TX STATUS\n");
            buf[0] = 0x01;
            buf[1] = 0x40;
            buf[2] = 0x74;
            buf[3] = 0x00;
            buf[4] = 0x03; n = write(fd, buf, 5);
            break;

        case 0x03:
            printf("READ RX STATUS\n");
            buf[0] = 0x01;
            buf[1] = 0x40;
            buf[2] = 0x74;
            buf[3] = 0x00;
            buf[4] = 0x03; n = write(fd, buf, 5);
            break;

        case 0xbb: buf[0] = buf[1] = 0; printf("READ EPROM\n"); n = write(fd, buf, 2);
            break;

        default: printf("Unknown cmd=%02x\n", buf[4]);
        }
    }

    return 0;
}
