// can run this using rigctl/rigctld and socat pty devices
#define _XOPEN_SOURCE 700
// since we are POSIX here we need this
#include <stdio.h>
#include <unistd.h>
//#include "hamlib/rig.h"

#include "sim.h"

/* In hono(u)r of the 10Hz resolution of the FT-847, vfo frequencies
 *   are stored in decaHertz(daHz)
 */
int freqs[4] = {1407400, 43510000, 43720000, 0};
int modes[4] = {0x01, 0x02, 0x02, 0};
char *vfoNames[4] = {"Main", "SAT Rx", "SAT Tx", "Bogus"};
int width_main = 500;
int width_sub = 700;
int vfo;


int main(int argc, char *argv[])
{
    unsigned char buf[BUFSIZE];
    int freq, i, n;

    int fd = openPort(argv[1]);

    while (1)
    {
        int bytes = getmyline5(fd, buf);

        if (bytes == 0)
        {
            continue;
        }

        if (bytes != 5)
        {
            printf("Not 5 bytes?  bytes=%d\n", bytes);
        }
        n = 0;

        switch (buf[4])
        {
        case 0x00: printf("LOCK ON\n"); break;

        case 0x80: printf("LOCK OFF\n"); break;

        case 0x08: printf("PTT ON\n"); break;

        case 0x88: printf("PTT OFF\n"); break;

        case 0x07:
        case 0x17:
        case 0x27:
            modes[(buf[4] & 0x30) >> 4] = buf[0];
            printf("MODE\n"); break;

        case 0x05: printf("CLAR ON\n"); break;

        case 0x85: printf("CLAR OFF\n"); break;

        case 0xF5: printf("FREQ\n"); break;

        case 0x81: printf("VFO TOGGLE\n"); break;

        case 0x02: printf("SPLIT ON\n"); break;

        case 0x82: printf("SPLIT OFF\n"); break;

        case 0x09: printf("REPEATER SHIFT\n"); break;

        case 0xF9: printf("REPEATER FREQ\n"); break;

        case 0x0A: printf("CTCSS/DCS MODE\n"); break;

        case 0x0B: printf("CTCSS TONE\n"); break;

        case 0x0C: printf("DCS CODE\n"); break;

        case 0xE7: printf("READ RX STATUS\n"); break;

        case 0xF7: printf("READ TX STATUS\n"); break;

        case 0x01:
        case 0x11:
        case 0x21:
            freq = 0;
            for (i = 0; i < 4; i++)
            {
                freq = freq * 100 + (((buf[i] & 0xf0) * 5) / 8) + (buf[i] & 0x0f);
            }
            freqs[(buf[4] & 0x30) >> 4] = freq;
            printf("FREQ SET = %d\n", freq * 10);
            break;

        case 0x03:
        case 0x13:
        case 0x23:
            printf("READ RX STATUS\n");
            vfo = (buf[4] & 0x30) >> 4;
            freq = freqs[vfo];
            for (i = 3; i >= 0; i--)
            {
                buf[i] = freq % 10;
                freq /= 10;
                buf[i] += (freq % 10) << 4;
                freq /= 10;
            }
            buf[4] = modes[vfo];
            n = write(fd, buf, 5);
            break;

        case 0xbb: buf[0] = buf[1] = 0; printf("READ EPROM\n"); n = write(fd, buf, 2);
            break;

        default: printf("Unknown cmd=%02x\n", buf[4]);
        }
        if (n < 0) {printf("Write failed - n = %d\n", n); }
    }

    return 0;
}
