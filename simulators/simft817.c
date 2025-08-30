// can run this using rigctl/rigctld and socat pty devices
#define _XOPEN_SOURCE 700
// since we are POSIX here we need this
#include <stdio.h>
#include <unistd.h>

#include "sim.h"

int vfo = 0; // 0=A, !0=B
float freqA = 14074000;
float freqB = 14074500;
char tx_vfo = '0';
char rx_vfo = '0';
char modeA = '1';
char modeB = '1';
int width_main = 500;
int width_sub = 700;


int main(int argc, char *argv[])
{
    unsigned char buf[BUFSIZE];

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

        switch (buf[4])
        {
        case 0x00: printf("LOCK ON\n"); break;

        case 0x80: printf("LOCK OFF\n"); break;

        case 0x08: printf("PTT ON\n"); break;

        case 0x88: printf("PTT OFF\n"); break;

        case 0x07:
            printf("MODE %0xx\n", buf[0]);

            if (vfo == 0) { modeA = buf[0]; }
            else { modeB = buf[0]; }

            break;

        case 0x05: printf("CLAR ON\n"); break;

        case 0x85: printf("CLAR OFF\n"); break;

        case 0xF5: printf("FREQ\n"); break;

        case 0x81:
            vfo = !vfo;
            printf("VFO TOGGLE, %s active\n", vfo == 0 ? "VFOA" : "VFOB");
            break;

        case 0x02: printf("SPLIT ON\n"); break;

        case 0x82: printf("SPLIT OFF\n"); break;

        case 0x09: printf("REPEATER SHIFT\n"); break;

        case 0xF9: printf("REPEATER FREQ\n"); break;

        case 0x0A: printf("CTCSS/DCS MODE\n"); break;

        case 0x0B: printf("CTCSS TONE\n"); break;

        case 0x0C: printf("DCS CODE\n"); break;

        case 0xE7: printf("READ RX STATUS\n"); break;

        case 0xF7: printf("READ TX STATUS\n"); break;

        case 0x03:
            printf("READ RX STATUS\n");
            buf[0] = 0x01;
            buf[1] = 0x40;
            buf[2] = 0x74;
            buf[3] = 0x00;
            buf[4] = 0x03; write(fd, buf, 5);
            break;

        case 0xbb:
            buf[0] = 80; buf[1] = 0; printf("READ EPROM\n");
            write(fd, buf, 2);
            break;

        default: printf("Unknown cmd=%02x\n", buf[4]);
        }
    }

    return 0;
}
