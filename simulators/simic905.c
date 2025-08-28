// simicom will show the pts port to use for rigctl on Unix
// using virtual serial ports on Windows is to be developed yet
// Needs a lot of improvement to work on all Icoms
#define _XOPEN_SOURCE 700
// since we are POSIX here we need this
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>
#include <sys/types.h>

#include "hamlib/rig.h"
#include "misc.h"
#include "sim.h"

#define X25

int civ_731_mode = 0;
vfo_t current_vfo = RIG_VFO_A;
int split = 0;

// we make B different from A to ensure we see a difference at startup
double freqA = 145123456;
double freqB = 1407450;
mode_t modeA = RIG_MODE_PKTUSB;
mode_t modeB = RIG_MODE_PKTUSB;
int datamodeA = 0;
int datamodeB = 0;
int filterA = 1;
int filterB = 2;
pbwidth_t widthA = 0;
pbwidth_t widthB = 1;
ant_t ant_curr = 0;
int ant_option = 0;
int ptt = 0;
int satmode = 0;
int agc_time = 1;
int ovf_status = 0;
int powerstat = 1;
int keyspd = 25;
int datamode = 0;
int filter = 0;

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

        if (i > 2 && c == 0xfe)
        {
            printf("Turning power on due to 0xfe string\n");
            powerstat = 1;
            int j;

            for (j = i; j < 175; ++j)
            {
                if (read(fd, &c, 1) < 0) { break; }
            }

            i = 0;
            continue;
        }
    }

    printf("Error %s\n", strerror(errno));

    return 0;
}

void frameParse(int fd, unsigned char *frame, int len)
{
    double freq;
    int freq_len = 5;

    if (len == 0)
    {
        printf("%s: len==0\n", __func__);
        return;
    }

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
            if (freqA > 5.85e9) { freq_len = 6; }

            printf("get_freqA len=%d\n", freq_len);
            to_bcd(&frame[5], (long long)freqA, freq_len * 2);
        }
        else
        {
            if (freqB > 5.85e9) { freq_len = 6; }

            printf("get_freqB len=%d\n", freq_len);
            to_bcd(&frame[5], (long long)freqB, freq_len * 2);
        }

        frame[5 + freq_len] = 0xfd;

        if (powerstat)
        {
            WRITE(fd, frame, 11);
        }

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
        WRITE(fd, frame, 8);
        break;

    case 0x05:
        freq = from_bcd(&frame[5], (civ_731_mode ? 4 : 5) * 2);
        printf("set_freq to %.0f\n", freq);

        if (current_vfo == RIG_VFO_A || current_vfo == RIG_VFO_MAIN) { freqA = freq; }
        else { freqB = freq; }

        frame[4] = 0xfb;
        frame[5] = 0xfd;
        WRITE(fd, frame, 6);
        break;

    case 0x06:
        if (current_vfo == RIG_VFO_A || current_vfo == RIG_VFO_MAIN) { modeA = frame[6]; }
        else { modeB = frame[6]; }

        frame[4] = 0xfb;
        frame[5] = 0xfd;
        WRITE(fd, frame, 6);
        break;

    case 0x07:

        switch (frame[5])
        {
        case 0x00: current_vfo = RIG_VFO_A; break;

        case 0x01: current_vfo = RIG_VFO_B; break;

        case 0xa0: freqB = freqA; modeB = modeA; break;

        case 0xb0: current_vfo = RIG_VFO_SUB; exit(1); break;
        }

        printf("set_vfo to %s\n", rig_strvfo(current_vfo));

        frame[4] = 0xfb;
        frame[5] = 0xfd;
        WRITE(fd, frame, 6);
        break;

    case 0x0f:
        if (frame[5] == 0xfd)
        {
            printf("get split %d\n", split);
            frame[5] = split;
            frame[6] = 0xfd;
            WRITE(fd, frame, 7);
        }
        else
        {
            printf("set split %d\n", 1);
            split = frame[5];
            frame[4] = 0xfb;
            frame[5] = 0xfd;
            WRITE(fd, frame, 6);
        }

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
        printf("WRITE 8 bytes\n");
        dump_hex(frame, 8);
        WRITE(fd, frame, 8);
        break;

    case 0x14:
        switch (frame[5])
        {
            static int power_level = 0;

        case 0x07:
        case 0x08:
            if (frame[6] != 0xfd)
            {
                frame[6] = 0xfb;
                dumphex(frame, 7);
                WRITE(fd, frame, 7);
                printf("ACK x14 x08\n");
            }
            else
            {
                to_bcd(&frame[6], (long long)128, 2);
                frame[8] = 0xfb;
                dumphex(frame, 9);
                WRITE(fd, frame, 9);
                printf("SEND x14 x08\n");
            }

            break;

        case 0x0a:
            printf("Using power level %d\n", power_level);
            power_level += 10;

            if (power_level > 250) { power_level = 0; }

            to_bcd(&frame[6], (long long)power_level, 2);
            frame[8] = 0xfd;
            WRITE(fd, frame, 9);
            break;

        case 0x0c:
            dumphex(frame, 10);
            printf("subcmd=0x0c #1\n");

            if (frame[6] != 0xfd) // then we have data
            {
                printf("subcmd=0x0c #1\n");
                keyspd = from_bcd(&frame[6], 2);
                frame[6] = 0xfb;
                WRITE(fd, frame, 7);
            }
            else
            {
                printf("subcmd=0x0c #1\n");
                to_bcd(&frame[6], keyspd, 2);
                frame[8] = 0xfd;
                WRITE(fd, frame, 9);
            }

            break;
        }

        break;


    case 0x15:
        switch (frame[5])
        {
            static int meter_level = 0;

        case 0x07:
            frame[6] = ovf_status;
            frame[7] = 0xfd;
            WRITE(fd, frame, 8);
            ovf_status = ovf_status == 0 ? 1 : 0;
            break;

        case 0x11:
            printf("Using meter level %d\n", meter_level);
            meter_level += 10;

            if (meter_level > 250) { meter_level = 0; }

            to_bcd(&frame[6], (long long)meter_level, 2);
            frame[8] = 0xfd;
            WRITE(fd, frame, 9);
            break;
        }

    case 0x16:
        switch (frame[5])
        {
        case 0x5a:
            if (frame[6] == 0xfe)
            {
                satmode = frame[6];
            }
            else
            {
                frame[6] = satmode;
                frame[7] = 0xfd;
                WRITE(fd, frame, 8);
            }

            break;
        }

        break;

    case 0x18: // miscellaneous things
        frame[5] = 1;
        frame[6] = 0xfd;
        WRITE(fd, frame, 7);
        break;

    case 0x19: // miscellaneous things
        frame[5] = 0x94;
        frame[6] = 0xfd;
        WRITE(fd, frame, 7);
        break;

    case 0x1a: // miscellaneous things
        switch (frame[5])
        {
        case 0x03:  // width
            if (current_vfo == RIG_VFO_A || current_vfo == RIG_VFO_MAIN) { frame[6] = widthA; }
            else { frame[6] = widthB; }

            frame[7] = 0xfd;
            WRITE(fd, frame, 8);
            break;

        case 0x04: // AGC TIME
            printf("frame[6]==x%02x, frame[7]=0%02x\n", frame[6], frame[7]);

            if (frame[6] == 0xfd)   // the we are reading
            {
                frame[6] = agc_time;
                frame[7] = 0xfd;
                WRITE(fd, frame, 8);
            }
            else
            {
                printf("AGC_TIME RESPONSE******************************");
                agc_time = frame[6];
                frame[4] = 0xfb;
                frame[5] = 0xfd;
                WRITE(fd, frame, 6);
            }

            break;

        case 0x06: // datamode
            if (frame[5] == 0xfd)
            {
                frame[6] = datamode;
                frame[7] = filter;
                frame[8] = 0xfd;
                WRITE(fd, frame, 9);
            }
            else
            {
                datamode = frame[6];
                filter = frame[7];
                frame[4] = 0xfb;
                frame[5] = 0xfd;
                WRITE(fd, frame, 6);
            }

        case 0x07: // satmode
            frame[4] = 0;
            frame[7] = 0xfd;
            WRITE(fd, frame, 8);
            break;

        }

        break;

    case 0x1c:
        switch (frame[5])
        {
        case 0:
            if (frame[6] == 0xfd)
            {
                int tmp = frame[2];
                frame[2] = frame[3];
                frame[3] = tmp;
                frame[6] = ptt;
                frame[7] = 0xfd;
                WRITE(fd, frame, 8);
            }
            else
            {
                ptt = frame[6];
                int tmp = frame[2];
                frame[2] = frame[3];
                frame[3] = tmp;
                frame[4] = 0xfb;
                frame[5] = 0xfd;
                WRITE(fd, frame, 6);
            }

            break;

        }

        break;


#ifdef X25

    case 0x25:
        if (frame[6] == 0xfd)
        {
            freq_len = 5;

            if (frame[5] == 0x00)
            {
                if (freqA > 5.85e9) { freq_len = 6; }

                to_bcd(&frame[6], (long long)freqA, freq_len * 2);
                printf("X25 get_freqA=%.0f\n", freqA);
                frame[6 + freq_len] = 0xfd;
                WRITE(fd, frame, 7 + freq_len);
            }
            else
            {
                if (freqB > 5.85e9) { freq_len = 6; }

                to_bcd(&frame[6], (long long)freqB, freq_len * 2);
                printf("X25 get_freqB=%.0f\n", freqB);
                frame[6 + freq_len] = 0xfd;
                WRITE(fd, frame, 7 + freq_len);
            }

            //unsigned char frame2[12];

#if 0
            frame2[0] = 0xfe;
            frame2[1] = 0xfe;
            frame2[2] = 0x00; // send transceive frame
            frame2[3] = frame[3]; // send transceive frame
            frame2[4] = 0x00;
            frame2[5] = 0x70;
            frame2[6] = 0x28;
            frame2[7] = 0x57;
            frame2[8] = 0x03;
            frame2[9] = 0x00;
            frame2[10] = 0x00;
            frame2[11] = 0xfd;
            WRITE(fd, frame2, 12);
#endif
        }
        else
        {
            freq_len = 5;

            if (frame[11] != 0xfd) { freq_len = 6; }

            freq = from_bcd(&frame[6], freq_len * 2);
            printf("set_freq to %.0f\n", freq);

            if (frame[5] == 0x00) { freqA = freq; }
            else { freqB = freq; }

            int tmp = frame[2];
            frame[2] = frame[3];
            frame[3] = tmp;
            frame[4] = 0xfb;
            frame[5] = 0xfd;
            WRITE(fd, frame, 6);
#if 0
            // send async frame
            frame[2] = 0x00; // async freq
            frame[3] = 0xa2;
            frame[4] = 0x00;
            frame[5] = 0x00;
            frame[6] = 0x10;
            frame[7] = 0x01;
            frame[8] = 0x96;
            frame[9] = 0x12;
            frame[10] = 0xfd;
            WRITE(fd, frame, 11);
#endif
        }

        break;

    case 0x26:
        for (int i = 0; i < 6; ++i) { printf("%02x:", frame[i]); }

        if (frame[6] == 0xfd) // then a query
        {
            for (int i = 0; i < 6; ++i) { printf("%02x:", frame[i]); }

            frame[6] = frame[5] == 0 ? modeA : modeB;
            frame[7] = frame[5] == 0 ? datamodeA : datamodeB;
            frame[8] = frame[5] == 0 ? filterA : filterB;
            frame[9] = 0xfd;
            WRITE(fd, frame, 10);
        }
        else
        {
            for (int i = 0; i < 12; ++i) { printf("%02x:", frame[i]); }

            if (frame[6] == 0)
            {
                modeA = frame[7];
                datamodeA = frame[8];
            }
            else
            {
                modeB = frame[7];
                datamodeB = frame[8];
            }

            frame[4] = 0xfb;
            frame[5] = 0xfd;
            WRITE(fd, frame, 6);
        }

        printf("\n");
        break;
#else

    case 0x25:
        printf("x25 send nak\n");
        frame[4] = 0xfa;
        frame[5] = 0xfd;
        WRITE(fd, frame, 6);
        break;

    case 0x26:
        printf("x26 send nak\n");
        frame[4] = 0xfa;
        frame[5] = 0xfd;
        WRITE(fd, frame, 6);
        break;
#endif

    default: printf("cmd 0x%02x unknown\n", frame[4]);
    }

// don't care about the rig type yet
}

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
    unsigned char buf[BUFSIZE];
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

        if (powerstat)
        {
            frameParse(fd, buf, len);
        }
        else
        {
            hl_usleep(1000 * 1000);
        }

        rigStatus();
    }

    return 0;
}
