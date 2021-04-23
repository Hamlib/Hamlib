// simicom will show the pts port to use for rigctl on Unix
// using virtual serial ports on Windows is to be developed yet
// gcc -g -Wall -o simicom simicom.c -lhamlib
// On mingw in the hamlib src directory
// gcc -static -I../include -g -Wall -o simicom simicom.c -L../../build/src/.libs -lhamlib -lwsock32 -lws2_32
#define _XOPEN_SOURCE 600
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/time.h>
#include <hamlib/rig.h>
#include "../src/misc.h"

#define BUFSIZE 256

int civ_731_mode = 0;
vfo_t vfo_curr = RIG_VFO_A;
int split = 0;

float freqA = 14074000;
float freqB = 14074500;
mode_t modeA = RIG_MODE_CW;
mode_t modeB = RIG_MODE_CW;
pbwidth_t widthA = 200;
pbwidth_t widthB = 300;

void dumphex(unsigned char *buf, int n)
{
    for (int i = 0; i < n; ++i) { printf("%02x ", buf[i]); }

    printf("\n");
}

int
frameGet(int fd, unsigned char *buf)
{
    int i = 0;
    memset(buf, 0, BUFSIZE);
    unsigned char c;

    while (read(fd, &c, 1) > 0)
    {
        buf[i++] = c;
        //printf("i=%d, c=0x%02x\n",i,c);

        if (c == 0xfd)
        {
            dumphex(buf, i);
            return i;
        }
    }

    printf("Error???\n");

    return 0;
}

void frameParse(int fd, unsigned char *frame, int len)
{
    double freq;

    dumphex(frame, len);

    if (frame[0] != 0xfe && frame[1] != 0xfe)
    {
        printf("expected fe fe, got ");
        dumphex(frame, len);
        return;
    }

    switch (frame[4])
    {
    case 0x03:

        //from_bcd(frameackbuf[2], (civ_731_mode ? 4 : 5) * 2);
        if (vfo_curr == RIG_VFO_A || vfo_curr == RIG_VFO_MAIN)
        {
            printf("get_freqA\n");
            to_bcd(&frame[5], (long long)freqA, (civ_731_mode ? 4 : 5) * 2);
        }
        else
        {
            printf("get_freqB\n");
            to_bcd(&frame[5], (long long)freqB, (civ_731_mode ? 4 : 5) * 2);
        }

        frame[10] = 0xfd;
        write(fd, frame, 11);
        break;

    case 0x04:
        if (vfo_curr == RIG_VFO_A || vfo_curr == RIG_VFO_MAIN)
        {
            printf("get_modeA\n");
            frame[5] = modeA;
            frame[6] = widthA;
        }
        else
        {
            printf("get_modeB\n");
            frame[5] = modeB;
            frame[6] = widthB;
        }

        frame[7] = 0xfd;
        write(fd, frame, 8);
        break;

    case 0x05:
        freq = from_bcd(&frame[5], (civ_731_mode ? 4 : 5) * 2);
        printf("set_freq to %.0f\n", freq);

        if (vfo_curr == RIG_VFO_A || vfo_curr == RIG_VFO_MAIN) { freqA = freq; }
        else { freqB = freq; }

//    case 0x06:


    case 0x07:

        switch (frame[5])
        {
        case 0x00: vfo_curr = RIG_VFO_A; break;

        case 0x01: vfo_curr = RIG_VFO_B; break;

        case 0xd0: vfo_curr = RIG_VFO_MAIN; break;

        case 0xd1: vfo_curr = RIG_VFO_SUB; break;
        }

        printf("set_vfo to %s\n", rig_strvfo(vfo_curr));

        frame[4] = 0xfb;
        frame[5] = 0xfd;
        write(fd, frame, 6);
        break;

    case 0x0f:
        if (frame[5] == 0) { split = 0; }
        else { split = 1; }

        printf("set split %d\n", 1);
        frame[4] = 0xfb;
        frame[5] = 0xfd;
        write(fd, frame, 6);
        break;

    case 0x25:
        if (frame[6] == 0xfd)
        {
            if (frame[5] == 0x00)
            {
                to_bcd(&frame[6], (long long)freqA, (civ_731_mode ? 4 : 5) * 2);
                printf("get_freqA=%.0f\n", freqA);
            }
            else
            {
                to_bcd(&frame[6], (long long)freqB, (civ_731_mode ? 4 : 5) * 2);
                printf("get_freqB=%.0f\n", freqB);
            }

            frame[11] = 0xfd;
            write(fd, frame, 12);
        }
        else
        {
            freq = from_bcd(&frame[5], (civ_731_mode ? 4 : 5) * 2);
            printf("set_freq to %.0f\n", freq);

            if (frame[5] == 0x00) { freqA = freq; }
            else { freqB = freq; }

            frame[4] = 0xfb;
            frame[5] = 0xfd;
        }

        break;

    default: printf("cmd 0x%02x unknown\n", frame[4]);
    }

    // don't care about the rig type yet

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

void rigStatus()
{
    printf("VFOA: mode=%s width=%ld freq=%.0f\n", rig_strrmode(modeA), widthA,
           freqA);
    printf("VFOB: mode=%s width=%ld freq=%.0f\n", rig_strrmode(modeB), widthB,
           freqB);
}

int main(int argc, char **argv)
{
    unsigned char buf[256];
    int fd = openPort(argv[1]);

    printf("%s: %s\n", argv[0], rig_version());
#if defined(WIN32) || defined(_WIN32)

    if (argc != 2)
    {
        printf("Missing comport argument\n");
        printf("%s [comport]\n", argv[0]);
        exit(1);
    }

#endif

    while (1)
    {
        int len = frameGet(fd, buf);

        if (len <= 0)
        {
            close(fd);
            fd = openPort(argv[1]);
        }

        frameParse(fd, buf, len);
        rigStatus();
    }

    return 0;
}
