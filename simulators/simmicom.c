// can run this using rigctl/rigctld and socat pty devices
// gcc -o simspid simspid.c
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
#include "../src/misc.h"

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
    int i = 0;
    int n = 0;
    memset(buf, 0, BUFSIZE);

    n = read(fd, buf, 4);

    if (n <= 0) { sleep(1); return 0;}

    int bytesToRead = buf[1] + 2; //; len does not include cksum, or eom
    n += read(fd, &buf[4], bytesToRead);
    printf("n=%d:", n);

    for (i = 0; i < n; ++i)
    {
        printf(" %02x", buf[i]);
    }

    printf("\n");
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

        switch (buf[3])
        {
        case 0x06:
            printf("Report receiver freq\n");
            unsigned char cmd[11] = { 0x24, 0x06, 0x18, 0x05, 0x01, 0x00, 0x38, 0xea, 0x50, 0xba, 0x03};
            dump_hex(cmd, 11);
            int n = write(fd, cmd, sizeof(cmd));
            printf("%d bytes sent\n", n);
            break;

        case 0x13:
            printf("PTT On\n");
            break;

        case 0x14:
            printf("PTT Off\n");
            break;

        case 0x36:
            printf("Key request\n");
            break;

        default: printf("Unknown cmd=%02x\n", buf[3]);
        }
    }

    return 0;
}
