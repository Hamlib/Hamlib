// simicom will show the pts port to use for rigctl on Unix
// using virtual serial ports on Windows is to be developed yet
// Needs a lot of improvement to work on all Icoms
#define _XOPEN_SOURCE 700
// since we are POSIX here we need this
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>

#include "misc.h"
#include "sim.h"
/* Simulators really shouldn't be using ANY of the definitions
 *  from the Hamlib rig.h parameters, but only those of the
 *  rig itself.  This still won't be a clean room implementation,
 *  but lets try to use as many as possible of Icom's definitions
 *  for the rig parameters.
 */
#include "../rigs/icom/icom_defs.h"

#define X25
#undef SATMODE

unsigned char civaddr = 0xA4;
int civ_731_mode = 0;
int current_vfo = S_VFOA;
int split = 0;
int keyspd = 85; // 85=20WPM

// we make B different from A to ensure we see a difference at startup
float freqA = 14074000;
float freqB = 14074500;
unsigned char modeA = S_FM;
unsigned char modeB = S_FM;
int datamodeA = 0;
int datamodeB = 0;
int widthA = 0;
int widthB = 1;
unsigned char filterA = 1, filterB = 1;
//ant_t ant_curr = 0;
//int ant_option = 0;
int ptt = 0;
int satmode = 0;
int agc_time = 1;
int ovf_status = 0;
int powerstat = 1;
const char *vfonames[2] = {"VFOA", "VFOB"};

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

        if (c == FI)  // End of message (EOM)
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
    int n = 0;
    unsigned char acknak = ACK; // Hope for the best

    if (len == 0)
    {
        printf("%s: len==0\n", __func__);
        return;
    }

    printf("Here#1\n");
    dumphex(frame, len);

    if (frame[0] != 0xfe || frame[1] != 0xfe)
    {
        printf("expected fe fe, got ");
        dumphex(frame, len);
        return;
    }

    switch (frame[4])
    {
    case C_RD_FREQ: // 0x03

        //from_bcd(frameackbuf[2], (civ_731_mode ? 4 : 5) * 2);
        if (current_vfo == S_VFOA || current_vfo == S_MAIN)
        {
            printf("get_freqA\n");
            to_bcd(&frame[5], (long long)freqA, (civ_731_mode ? 4 : 5) * 2);
        }
        else
        {
            printf("get_freqB\n");
            to_bcd(&frame[5], (long long)freqB, (civ_731_mode ? 4 : 5) * 2);
        }

        frame[10] = FI;

        if (powerstat)
        {
            n = write(fd, frame, 11);
        }

        break;

    case C_RD_MODE: // 0x04
        if (current_vfo == S_VFOA || current_vfo == S_MAIN)
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

    case C_SET_FREQ: // 0x05
        freq = from_bcd(&frame[5], (civ_731_mode ? 4 : 5) * 2);
        printf("set_freq to %.0f\n", freq);

        if (current_vfo == S_VFOA || current_vfo == S_MAIN) { freqA = freq; }
        else { freqB = freq; }

        frame[4] = ACK;
        frame[5] = FI;
        n = write(fd, frame, 6);
        break;

    case C_SET_MODE: // 0x06
        if (current_vfo == S_VFOA || current_vfo == S_MAIN) { modeA = frame[6]; }
        else { modeB = frame[6]; }

        frame[4] = ACK;
        frame[5] = FI;
        n = write(fd, frame, 6);
        break;

    case C_SET_VFO: // 0x07
        switch (frame[5])
        {
        case S_VFOA:
        case S_VFOB:
            current_vfo = frame[5];
            break;
	case S_BTOA:
            // Figure out what this really does
            break;
        case S_XCHNG:
            // Ditto
            break;
        default:
            acknak = NAK;
        }

        printf("set_vfo to %s\n", vfonames[current_vfo]);

        frame[4] = acknak;
        frame[5] = FI;
        n = write(fd, frame, 6);
        break;

    case C_CTL_SPLT: // 0x0F
        if (frame[5] == 0) { split = 0; }
        else if (frame[5] == 1) { split = 1; }
        else { frame[6] = split; }

        if (frame[5] == FI)
        {
            printf("get split %d\n", split);
            frame[7] = FI;
            n = write(fd, frame, 8);
        }
        else
        {
            printf("set split %d\n", split);
            frame[4] = ACK;
            frame[5] = FI;
            n = write(fd, frame, 6);
        }

        break;

#if 0 // No antenna control
    case C_CTL_ANT: // 0x12 we're simulating the 3-byte version -- not the 2-byte
        if (frame[5] != FI)
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
        printf("write 8 bytes\n");
        dump_hex(frame, 8);
        n = write(fd, frame, 8);
        break;
#endif

    case C_CTL_LVL: // 0x14
        printf("cmd=0x14\n");

        switch (frame[5])
        {
            static int power_level = 0;

        case S_LVL_PBTIN:  // 0x07
        case S_LVL_PBTOUT: // 0x08
            if (frame[6] != 0xfd)
            {
                frame[6] = 0xfb;
                dumphex(frame, 7);
                n = write(fd, frame, 7);
                printf("ACK x14 x08\n");
            }
            else
            {
                to_bcd(&frame[6], (long long)128, 2);
                frame[8] = 0xfb;
                dumphex(frame, 9);
                n = write(fd, frame, 9);
                printf("SEND x14 x08\n");
            }

            break;

        case S_LVL_RFPOWER: // 0x0A
            power_level += 10;
            if (power_level > 250) { power_level = 0; }
            printf("Using power level %d\n", power_level);

            to_bcd(&frame[6], (long long)power_level, 2);
            frame[8] = 0xfd;
            n = write(fd, frame, 9);
            break;

        case S_LVL_KEYSPD: // 0x0C
            //dumphex(frame, 10);

            if (frame[6] != 0xfd) // then we have data
            {
                keyspd = from_bcd(&frame[6], 2);
                frame[6] = 0xfb;
                n = write(fd, frame, 7);
            }
            else
            {
                to_bcd(&frame[6], keyspd, 2);
                frame[8] = 0xfd;
                n = write(fd, frame, 9);
            }

            break;
        }

        break;

    case C_RD_SQSM: // 0x15
        switch (frame[5])
        {
            static int meter_level = 0;

        case S_SML: // 0x02
            frame[6] = 00;
            frame[7] = 00;
            frame[8] = FI;
            n = write(fd, frame, 9);
            break;

        case S_OVF: // 0x07
            frame[6] = ovf_status;
            frame[7] = FI;
            n = write(fd, frame, 8);
            ovf_status = !ovf_status;
            break;

        case S_RFML: // 0x11
            meter_level += 10;
            if (meter_level > 250) { meter_level = 0; }
            printf("Using meter level %d\n", meter_level);

            to_bcd(&frame[6], (long long)meter_level, 2);
            frame[8] = FI;
            n = write(fd, frame, 9);
            break;
        }

    case C_CTL_FUNC: // 0x16
        switch (frame[5])
        {
#ifdef SATMODE
        case S_FUNC_SATM: // 0x5A
            if (frame[6] == 0xfe)
            {
                satmode = frame[6];
            }
            else
            {
                frame[6] = satmode;
                frame[7] = FI;
                n = write(fd, frame, 8);
            }

            break;
#endif
        }

        break;

    case C_SET_PWR: // 0x18 miscellaneous things
        frame[5] = 1;
        frame[6] = FI;
        n = write(fd, frame, 7);
        break;

    case C_RD_TRXID: // 0x19 miscellaneous things
        frame[5] = 0xA4;
        frame[6] = FI;
        n = write(fd, frame, 7);
        break;

    case C_CTL_MEM: // 0x1A miscellaneous things
        switch (frame[5])
        {
        case S_MEM_FILT_WDTH:  // 0x03 width
            if (frame[6] == FI) // Query
            {
                if (current_vfo == S_VFOA || current_vfo == S_MAIN) { frame[6] = widthA; }
                else { frame[6] = widthB; }

                frame[7] = FI;
                n = write(fd, frame, 8);
            }
            else // set
            {
                if (current_vfo == S_VFOA) { widthA = frame[6]; }
                else { widthB = frame[6]; }
                frame[4] = ACK;
                frame[5] = FI;
                n = write(fd, frame, 6);
            }
            break;

        case 0x04: // AGC TIME
            printf("frame[6]==x%02x, frame[7]=0%02x\n", frame[6], frame[7]);

            if (frame[6] == FI)   // then we are reading
            {
                frame[6] = agc_time;
                frame[7] = FI;
                n = write(fd, frame, 8);
            }
            else
            {
                printf("AGC_TIME RESPONSE******************************");
                agc_time = frame[6];
                frame[4] = 0xfb;
                frame[5] = 0xfd;
                n = write(fd, frame, 6);
            }

            break;

#ifdef SATMODE
        case 0x07: // satmode
            frame[4] = 0;
            frame[7] = 0xfd;
            n = write(fd, frame, 8);
            break;
#endif

        }

        break;

    case C_CTL_PTT: //0x1C
        switch (frame[5])
        {
        case S_PTT: // 0x00
            if (frame[6] == 0xfd)
            {
                frame[6] = ptt;
                frame[7] = FI;
                n = write(fd, frame, 8);
            }
            else
            {
                ptt = frame[6];
                frame[7] = ACK;
                frame[8] = FI;
                n = write(fd, frame, 9);
            }

            break;

        }

        break;


#ifdef X25

    case C_SEND_SEL_FREQ: // 0x25
        if (frame[6] == FI)
        {
            if (frame[5] == 0x00)
            {
                to_bcd(&frame[6], (long long)freqA, (civ_731_mode ? 4 : 5) * 2);
                printf("X25 get_freqA=%.0f\n", freqA);
            }
            else
            {
                to_bcd(&frame[6], (long long)freqB, (civ_731_mode ? 4 : 5) * 2);
                printf("X25 get_freqB=%.0f\n", freqB);
            }

            frame[11] = FI;
#if 0
            unsigned char frame2[11];

            frame2[0] = 0xfe;
            frame2[1] = 0xfe;
            frame2[2] = 0x00; // send transceive frame
            frame2[3] = frame[3]; // send transceive frame
            frame2[4] = 0x00;
            frame2[5] = 0x00;
            frame2[6] = 0x35;
            frame2[7] = 0x57;
            frame2[8] = 0x03;
            frame2[9] = 0x00;
            frame2[10] = 0xfd;
            n = write(fd, frame2, 11);
#else
            n = write(fd, frame, 12);
#endif
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
            n = write(fd, frame, 11);
#endif
        }

        break;

    case C_SEND_SEL_MODE: // 0x26

        if (frame[6] == 0xfd) // then a query
        {
            frame[6] = frame[5] == 0 ? modeA : modeB;
            frame[7] = frame[5] == 0 ? datamodeA : datamodeB;
            frame[8] = frame[5] == 0 ? filterA : filterB;
            frame[9] = 0xfd;
            printf("  ->");
            for (int i = 0; i < 10; ++i) { printf("%02x:", frame[i]); }
            printf("\n");
            n = write(fd, frame, 10);
        }
        else
        {
            if (frame[5] == 0)
            {
                modeA = frame[6];
                datamodeA = frame[7];
                filterA = frame[8];
            }
            else
            {
                modeB = frame[6];
                datamodeB = frame[7];
                filterB = frame[8];
            }

            frame[4] = ACK;
            frame[5] = FI;
            n = write(fd, frame, 6);
        }

        break;
#else

    case 0x25:
        printf("x25 send nak\n");
        frame[4] = NAK;
        frame[5] = FI;
        n = write(fd, frame, 6);
        break;

    case 0x26:
        printf("x26 send nak\n");
        frame[4] = NAK;
        frame[5] = FI;
        n = write(fd, frame, 6);
        break;
#endif

    default: printf("cmd 0x%02x unknown\n", frame[4]);
    }

    if (n == 0) { printf("Write failed=%s\n", strerror(errno)); }

// don't care about the rig type yet

}


void rigStatus()
{
    char vfoa = current_vfo == S_VFOA ? '*' : ' ';
    char vfob = current_vfo == S_VFOB ? '*' : ' ';
    printf("%cVFOA: mode=%d datamode=%d width=%d freq=%.0f\n", vfoa, modeA,
           datamodeA,
           widthA,
           freqA);
    printf("%cVFOB: mode=%d datamode=%d width=%d freq=%.0f\n", vfob, modeB,
           datamodeB,
           widthB,
           freqB);
}

int main(int argc, char **argv)
{
    unsigned char buf[BUFSIZE];
    int fd = openPort(argv[1]);

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
            unsigned char tmp = buf[2];
            buf[2] = buf[3];
            buf[3] = tmp;

            frameParse(fd, buf, len);
        }
        else
        {
            sleep(1);
        }

        rigStatus();
    }

    return 0;
}
