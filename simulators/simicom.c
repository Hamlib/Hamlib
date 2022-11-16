// simicom will show the pts port to use for rigctl on Unix
// using virtual serial ports on Windows is to be developed yet
// Needs a lot of improvement to work on all Icoms
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
//#define X25

int civ_731_mode = 0;
vfo_t current_vfo = RIG_VFO_A;
int split = 0;

// we make B different from A to ensure we see a difference at startup
float freqA = 14074000;
float freqB = 14074500;
mode_t modeA = RIG_MODE_CW;
mode_t modeB = RIG_MODE_USB;
int datamodeA = 0;
int datamodeB = 0;
pbwidth_t widthA = 0;
pbwidth_t widthB = 1;
ant_t ant_curr = 0;
int ant_option = 0;
int ptt = 0;

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
    int n;

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
        if (current_vfo == RIG_VFO_A || current_vfo == RIG_VFO_MAIN)
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
        n = write(fd, frame, 11);

        if (n != 11) {printf("Error!\n"); exit(1);}

        break;

    case 0x04:
        if (current_vfo == RIG_VFO_A || current_vfo == RIG_VFO_MAIN)
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
        n = write(fd, frame, 8);
        break;

    case 0x05:
        freq = from_bcd(&frame[5], (civ_731_mode ? 4 : 5) * 2);
        printf("set_freq to %.0f\n", freq);

        if (current_vfo == RIG_VFO_A || current_vfo == RIG_VFO_MAIN) { freqA = freq; }
        else { freqB = freq; }

        frame[4] = 0xfb;
        frame[5] = 0xfd;
        n = write(fd, frame, 6);
        break;

    case 0x06:
        if (current_vfo == RIG_VFO_A || current_vfo == RIG_VFO_MAIN) { modeA = frame[6]; }
        else { modeB = frame[6]; }

        frame[4] = 0xfb;
        frame[5] = 0xfd;
        n = write(fd, frame, 6);
        break;

    case 0x07:

        switch (frame[5])
        {
        case 0x00: current_vfo = RIG_VFO_A; break;

        case 0x01: current_vfo = RIG_VFO_B; break;

        case 0xd0: current_vfo = RIG_VFO_MAIN; break;

        case 0xd1: current_vfo = RIG_VFO_SUB; break;
        }

        printf("set_vfo to %s\n", rig_strvfo(current_vfo));

        frame[4] = 0xfb;
        frame[5] = 0xfd;
        n = write(fd, frame, 6);
        break;

    case 0x0f:
        if (frame[5] == 0) { split = 0; }
        else { split = 1; }

        printf("set split %d\n", 1);
        frame[4] = 0xfb;
        frame[5] = 0xfd;
        n = write(fd, frame, 6);
        break;

    case 0x12: // we're simulating the 3-byte version -- not the 2-byte
        if (frame[5] != 0xfd)
        {
            printf("Set ant %d\n", -1);
            ant_curr = frame[5];
            ant_option = frame[6];
            dump_hex(frame, 8);
        }
        else
        {
            printf("Get ant\n");
        }

        frame[5] = ant_curr;
        frame[6] = ant_option;
        frame[7] = 0xfd;
        printf("n=write 8 bytes\n");
        dump_hex(frame, 8);
        n = write(fd, frame, 8);
        break;

    case 0x14:
        switch (frame[5])
        {
            static int power_level = 0;
            static int level = 0;

        case 0x01:
            level = 255;
            printf("Using AF level %d\n", level);
            to_bcd(&frame[6], (long long) level, 2);
            frame[8] = 0xfd;
            n = write(fd, frame, 9);
            break;

        case 0x0a:
            printf("Using power level %d\n", power_level);
            power_level += 10;

            if (power_level > 250) { power_level = 0; }

            to_bcd(&frame[6], (long long)power_level, 2);
            frame[8] = 0xfd;
            n = write(fd, frame, 9);
            break;
        }

        break;

    case 0x15:
        switch (frame[5])
        {
            static int meter_level = 0;

        case 0x11:
            printf("Using meter level %d\n", meter_level);
            meter_level += 10;

            if (meter_level > 250) { meter_level = 0; }

            to_bcd(&frame[6], (long long)meter_level, 2);
            frame[8] = 0xfd;
            n = write(fd, frame, 9);
            break;
        }

        break;

    case 0x18: // miscellaneous things
        frame[5] = 1;
        frame[6] = 0xfd;
        n = write(fd, frame, 7);
        break;

    case 0x1a: // miscellaneous things
        switch (frame[5])
        {
        case 0x03:  // width
            if (current_vfo == RIG_VFO_A || current_vfo == RIG_VFO_MAIN) { frame[6] = widthA; }
            else { frame[6] = widthB; }

            frame[7] = 0xfd;
            n = write(fd, frame, 8);
            break;

        case 0x04: // IC7200 data mode
            frame[6] = 0;
            frame[7] = 0;
            frame[8] = 0xfd;
            n = write(fd, frame, 9);
            break;

        case 0x07: // satmode
            frame[6] = 0;
            frame[7] = 0xfd;
            n = write(fd, frame, 8);
            break;

        }

        break;

    case 0x1c:
        switch (frame[5])
        {
        case 0:
            if (frame[6] == 0xfd)
            {
                frame[6] = ptt;
                frame[7] = 0xfd;
                n = write(fd, frame, 8);
            }
            else
            {
                ptt = frame[6];
                frame[7] = 0xfb;
                frame[8] = 0xfd;
                n = write(fd, frame, 9);
            }

            break;

        }

        break;


#ifdef X25

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
            n = write(fd, frame, 12);
        }
        else
        {
            freq = from_bcd(&frame[6], (civ_731_mode ? 4 : 5) * 2);
            printf("set_freq to %.0f\n", freq);

            if (frame[5] == 0x00) { freqA = freq; }
            else { freqB = freq; }

            frame[4] = 0xfb;
            frame[5] = 0xfd;
            n = write(fd, frame, 6);
        }

        break;

    case 0x26:
        for (int i = 0; i < 6; ++i) { printf("%02x:", frame[i]); }

        if (frame[6] == 0xfd) // then a query
        {
            for (int i = 0; i < 6; ++i) { printf("%02x:", frame[i]); }

            frame[6] = frame[5] == 0 ? modeA : modeB;
            frame[7] = frame[5] == 0 ? datamodeA : datamodeB;
            frame[8] = 0xfb;
            frame[9] = 0xfd;
            n = write(fd, frame, 10);
        }
        else
        {
            for (int i = 0; i < 12; ++i) { printf("%02x:", frame[i]); }

            if (frame[5] == 0)
            {
                modeA = frame[6];
                datamodeA = frame[7];
            }
            else
            {
                modeB = frame[6];
                datamodeB = frame[7];
            }

            frame[4] = 0xfb;
            frame[5] = 0xfd;
            n = write(fd, frame, 6);
        }

        printf("\n");
        break;
#else

    case 0x25:
        frame[4] = 0xfa;
        frame[5] = 0xfd;
        break;

    case 0x26:
        frame[4] = 0xfa;
        frame[5] = 0xfd;
        break;
#endif

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
    char vfoa = current_vfo == RIG_VFO_A ? '*' : ' ';
    char vfob = current_vfo == RIG_VFO_B ? '*' : ' ';
    printf("%cVFOA: mode=%d datamode=%d width=%ld freq=%.0f\n", vfoa, modeA,
           datamodeA,
           widthA,
           freqA);
    printf("%cVFOB: mode=%d datamode=%d width=%ld freq=%.0f\n", vfob, modeB,
           datamodeB,
           widthB,
           freqB);
}

int main(int argc, char **argv)
{
    unsigned char buf[256];
    int fd = openPort(argv[1]);

    printf("%s: %s\n", argv[0], rig_version());
#ifdef X25
    printf("x25/x26 command recognized\n");
#else
    printf("x25/x26 command rejected\n");
#endif
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
