// can run this using rigctl/rigctld and socat pty devices
// gcc -o simatd578 simatd578.c
#define _XOPEN_SOURCE 700
// since we are POSIX here we need this
struct ip_mreq
{
    int dummy;
};

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
int ptt = 0;
int curr_vfo = 0;


int
getmyline(int fd, unsigned char *buf)
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
        n++;
    }
    while (c != 0x0a);

    printf("n=%d \n", n);

    for (i = 0; i < n; ++i) { printf("%02x ", buf[i]); }

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
    unsigned char buf[256], buf2[256];
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

                if (buf[4] == 0x20)
                {
                    if (curr_vfo == 1)
                    {
                        curr_vfo = 0;
                    }
                    else
                    {
                        curr_vfo = 1;
                    }
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

                if (buf[4] == 0x20) { n = write(fd, buf2, 17); }
                else { n = 0; }
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
