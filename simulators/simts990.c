// can run this using rigctl/rigctld and socat pty devices
#define _XOPEN_SOURCE 700
// since we are POSIX here we need this
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>

#include "hamlib/rig.h"
#include "misc.h"


int mysleep = 20;

int freqA = 14074000;
int freqB = 14074500;
int filternum1 = 7;
int filternum2 = 8;
int datamode = 0;
int vfo, vfo_tx, ptt, ptt_data, ptt_mic, ptt_tune;
int operatingband;
int split;
int keyspd = 20;
int rg0 = 50;
int rg1 = 50;
int sq0 = 0;
int sq1 = 0;
int mg = 0;
int nt0 = 0;
int nt1 = 0;
int nr0 = 0;
int nr1 = 0;
int gc0 = 0;
int gc1 = 0;
int mv0 = 0;
int mv1 = 0;
int pa0 = 0;
int pa1 = 0;
int ra0 = 0;
int ra1 = 0;
int rl0 = 0;
int rl1 = 0;
int ml = 0;
int ag0 = 0;
int ag1 = 0;
int sl0 = 0;
int sl1 = 0;
int sh0 = 0;
int sh1 = 0;
int mo0 = 0;
int mo1 = 0;
int pc = 50;


#include "sim.h"




int main(int argc, char *argv[])
{
    char buf[BUFSIZE];
    char *pbuf;
    int fd = openPort(argv[1]);
    char modeA = '1', modeB = '2';

    while (1)
    {
        hl_usleep(10);
        buf[0] = 0;

        //if (getmyline(fd, buf) > 0) { printf("Cmd:%s\n", buf); }
        getmyline(fd, buf);

//        else { return 0; }

        if (strncmp(buf, "AC000;", 3) == 0)
        {
            continue;
        }
        else if (strncmp(buf, "RMA2", 3) == 0)
        {
            pbuf = "RM20020;";
            write(fd, pbuf, strlen(pbuf));
        }
        else if (strcmp(buf, "RM51;") == 0)
        {
            hl_usleep(mysleep * 1000);
            pbuf = "RM5100001;";
            write(fd, pbuf, strlen(pbuf));
        }

        else if (strcmp(buf, "AN0;") == 0)
        {
            hl_usleep(mysleep * 1000);
            pbuf = "AN030;";
            write(fd, pbuf, strlen(pbuf));
        }
        else if (strcmp(buf, "IF;") == 0)
        {
            char ifbuf[256];
            hl_usleep(mysleep * 1000);
//            pbuf = "IF000503130001000+0000000000030000000;"
            sprintf(ifbuf, "IF%011d1000+0000002000000000000;", freqA);
            //pbuf = "IF00010138698     +00000000002000000 ;
            write(fd, ifbuf, strlen(ifbuf));
            continue;
        }
        else if (strcmp(buf, "NB;") == 0)
        {
            hl_usleep(mysleep * 1000);
            pbuf = "NB0;";
            write(fd, pbuf, strlen(pbuf));
            continue;
        }
        else if (strcmp(buf, "RA;") == 0)
        {
            hl_usleep(mysleep * 1000);
            pbuf = "RA01;";
            write(fd, pbuf, strlen(pbuf));
            continue;
        }
        else if (strcmp(buf, "RG;") == 0)
        {
            hl_usleep(mysleep * 1000);
            pbuf = "RG055;";
            write(fd, pbuf, strlen(pbuf));
            continue;
        }
        else if (strcmp(buf, "AG;") == 0)
        {
            hl_usleep(mysleep * 1000);
            pbuf = "AG100;";
            write(fd, pbuf, strlen(pbuf));
            continue;
        }
        else if (strcmp(buf, "FV;") == 0)
        {
            hl_usleep(mysleep * 1000);
            pbuf = "FV1.2;";
            write(fd, pbuf, strlen(pbuf));
            continue;
        }
        else if (strncmp(buf, "IS;", 3) == 0)
        {
            SNPRINTF(buf, sizeof(buf), "IS+0000;");
            write(fd, buf, strlen(buf));
            continue;
        }
        else if (strncmp(buf, "IS", 2) == 0)
        {
            continue;
        }
        else if (strncmp(buf, "SM;", 3) == 0)
        {
            SNPRINTF(buf, sizeof(buf), "SM0035;");
            write(fd, buf, strlen(buf));
            continue;
        }
        else if (strncmp(buf, "PC;", 3) == 0)
        {
            SNPRINTF(buf, sizeof(buf), "PC%03d;", pc);
            write(fd, buf, strlen(buf));
            continue;
        }
        else if (strncmp(buf, "PC", 2) == 0)
        {
            sscanf(buf, "PC%d", &pc);
        }
        else if (strcmp(buf, "FW;") == 0)
        {
            //usleep(mysleep * 1000);
            pbuf = "FW240";
            write(fd, pbuf, strlen(pbuf));
            hl_usleep(20 * 1000);
            pbuf = "0;";
            write(fd, pbuf, strlen(pbuf));
            continue;
        }
        else if (strncmp(buf, "FW", 2) == 0)
        {
            continue;
        }
        else if (strcmp(buf, "ID;") == 0)
        {
            hl_usleep(mysleep * 1000);
            int id = 24;
            SNPRINTF(buf, sizeof(buf), "ID%03d;", id);
            write(fd, buf, strlen(buf));
            continue;
        }

#if 0
        else if (strncmp(buf, "AI", 2) == 0)
        {
            if (strcmp(buf, "AI;"))
            {
                hl_usleep(mysleep * 1000);
                n = fprintf(fp, "%s", "AI0;");
            }
        }

#endif
        else if (strcmp(buf, "VS;") == 0)
        {
            hl_usleep(mysleep * 1000);
            pbuf = "VS0;";
            write(fd, pbuf, strlen(pbuf));
            continue;
        }
        else if (strcmp(buf, "EX032;") == 0)
        {
            static int ant = 0;
            ant = (ant + 1) % 3;
            hl_usleep(mysleep * 1000);
            SNPRINTF(buf, sizeof(buf), "EX032%1d;", ant);
            write(fd, buf, strlen(buf));
            continue;
        }
        else if (strncmp(buf, "EX", 2) == 0)
        {
            continue;
        }
        else if (strcmp(buf, "FA;") == 0)
        {
            SNPRINTF(buf, sizeof(buf), "FA%011d;", freqA);
            write(fd, buf, strlen(buf));
            continue;
        }
        else if (strcmp(buf, "FB;") == 0)
        {
            SNPRINTF(buf, sizeof(buf), "FB%011d;", freqB);
            write(fd, buf, strlen(buf));
            continue;
        }
        else if (strncmp(buf, "FA", 2) == 0)
        {
            sscanf(buf, "FA%d", &freqA);
            continue;
        }
        else if (strncmp(buf, "FB", 2) == 0)
        {
            sscanf(buf, "FB%d", &freqB);
            continue;
        }
        else if (strncmp(buf, "AI;", 3) == 0)
        {
            SNPRINTF(buf, sizeof(buf), "AI0;");
            write(fd, buf, strlen(buf));
            continue;
        }

        else if (strncmp(buf, "PS;", 3) == 0)
        {
            SNPRINTF(buf, sizeof(buf), "PS1;");
            write(fd, buf, strlen(buf));
            continue;
        }
        else if (strncmp(buf, "SA;", 3) == 0)
        {
            SNPRINTF(buf, sizeof(buf), "SA0;");
            write(fd, buf, strlen(buf));
        }
        else if (buf[3] == ';' && strncmp(buf, "SF", 2) == 0)
        {
            SNPRINTF(buf, sizeof(buf), "SF%c%011d%c;", buf[2],
                     buf[2] == '0' ? freqA : freqB,
                     buf[2] == '0' ? modeA : modeB);
            write(fd, buf, strlen(buf));
            continue;
        }
        else if (strncmp(buf, "SF", 2) == 0)
        {
            mode_t tmpmode = buf[14];

            modeA = tmpmode;

            printf("modeA=%c, modeB=%c\n", modeA, modeB);

            continue;
        }
        else if (strncmp(buf, "FL;", 3) == 0)
        {
            SNPRINTF(buf, sizeof(buf), "FL%03d%03d;", filternum1, filternum2);
            write(fd, buf, strlen(buf));
            continue;
        }
        else if (strncmp(buf, "FL", 2) == 0)
        {
            sscanf(buf, "FL%3d%3d", &filternum1, &filternum2);
            continue;
        }
        else if (strcmp(buf, "FR;") == 0)
        {
            SNPRINTF(buf, sizeof(buf), "FR%d;", vfo);
            write(fd, buf, strlen(buf));
            continue;
        }
        else if (strncmp(buf, "FR", 2) == 0)
        {
            sscanf(buf, "FR%d", &vfo);
        }
        else if (strcmp(buf, "FT;") == 0)
        {
            SNPRINTF(buf, sizeof(buf), "FR%d;", vfo_tx);
            write(fd, buf, strlen(buf));
            continue;
        }
        else if (strncmp(buf, "FT", 2) == 0)
        {
            sscanf(buf, "FT%d", &vfo_tx);
        }
        else if (strncmp(buf, "DA;", 3) == 0)
        {
            SNPRINTF(buf, sizeof(buf), "DA%d;", datamode);
            write(fd, buf, strlen(buf));
            continue;
        }
        else if (strncmp(buf, "DA", 2) == 0)
        {
            sscanf(buf, "DA%d", &datamode);
            continue;
        }
        else if (strncmp(buf, "BD;", 3) == 0)
        {
            continue;
        }
        else if (strncmp(buf, "BU;", 3) == 0)
        {
            continue;
        }
        else if (strcmp(buf, "RX;") == 0)
        {
            ptt = ptt_mic = ptt_data = ptt_tune = 0;
        }
        else if (strncmp(buf, "TX", 2) == 0)
        {
            ptt = ptt_mic = ptt_data = ptt_tune = 0;

            switch (buf[2])
            {
            case ';': ptt = 1;

            case '0': ptt_mic = 1;

            case '1': ptt_data = 1;

            case '2': ptt_tune = 1;
            }

            continue;
        }
        else if (strncmp(buf, "CB;", 3) == 0)
        {
            sprintf(buf, "CB%d;", operatingband);
            write(fd, buf, strlen(buf));
        }
        else if (strncmp(buf, "CB", 2) == 0)
        {
            sscanf(buf, "CB%d", &operatingband);
        }
        else if (strncmp(buf, "TB;", 3) == 0)
        {
            sprintf(buf, "TB%d;", split);
            write(fd, buf, strlen(buf));
        }
        else if (strncmp(buf, "TB", 2) == 0)
        {
            sscanf(buf, "TB%d", &split);
        }
        else if (strncmp(buf, "KS;", 3) == 0)
        {
            sprintf(buf, "KS%03d;", keyspd);
            write(fd, buf, strlen(buf));
        }
        else if (strncmp(buf, "KS", 2) == 0)
        {
            sscanf(buf, "KS%03d", &keyspd);
        }
        else if (strncmp(buf, "OM0;", 4) == 0)
        {
            sprintf(buf, "OM0%c;", modeA);
            write(fd, buf, strlen(buf));
        }
        else if (strncmp(buf, "OM0", 3) == 0)
        {
            modeA = buf[3];
        }
        else if (strncmp(buf, "OM1;", 4) == 0)
        {
            sprintf(buf, "OM1%c;", modeB);
            write(fd, buf, strlen(buf));
        }
        else if (strncmp(buf, "OM1", 3) == 0)
        {
            modeB = buf[3];
        }
        else if (strcmp(buf, "RM;") == 0)
        {
            sprintf(buf, "RM2%04d;", 10);
            write(fd, buf, strlen(buf));
        }
        else if (strcmp(buf, "RG0;") == 0)
        {
            sprintf(buf, "RG0%03d;", rg0);
            write(fd, buf, strlen(buf));
        }
        else if (strncmp(buf, "RG0", 3) == 0)
        {
            sscanf(buf, "RG0%d", &rg0);
        }
        else if (strcmp(buf, "RG1;") == 0)
        {
            sprintf(buf, "RG1%03d;", rg1);
            write(fd, buf, strlen(buf));
        }
        else if (strncmp(buf, "RG1", 3) == 0)
        {
            sscanf(buf, "RG0%d", &rg1);
        }
        else if (strcmp(buf, "SQ0;") == 0)
        {
            sprintf(buf, "SQ0%03d;", sq0);
            write(fd, buf, strlen(buf));
        }
        else if (strncmp(buf, "SQ0", 3) == 0)
        {
            sscanf(buf, "SQ0%d", &sq0);
        }
        else if (strcmp(buf, "SQ1;") == 0)
        {
            sprintf(buf, "SQ1%03d;", sq1);
            write(fd, buf, strlen(buf));
        }
        else if (strncmp(buf, "SQ1", 3) == 0)
        {
            sscanf(buf, "SQ1%d", &sq1);
        }

        else if (strcmp(buf, "MG;") == 0)
        {
            sprintf(buf, "MG%03d;", mg);
            write(fd, buf, strlen(buf));
        }
        else if (strncmp(buf, "MG", 2) == 0)
        {
            sscanf(buf, "MG%d", &mg);
        }

        else if (strcmp(buf, "ML;") == 0)
        {
            sprintf(buf, "ML%03d;", ml);
            write(fd, buf, strlen(buf));
        }
        else if (strncmp(buf, "ML", 2) == 0)
        {
            sscanf(buf, "ML%d", &ml);
        }

        else if (strcmp(buf, "NT0;") == 0)
        {
            sprintf(buf, "NT0%03d;", nt0);
            write(fd, buf, strlen(buf));
        }
        else if (strncmp(buf, "NT0", 3) == 0)
        {
            sscanf(buf, "NT0%d", &nt0);
        }
        else if (strcmp(buf, "NT1;") == 0)
        {
            sprintf(buf, "NT1%03d;", nt1);
            write(fd, buf, strlen(buf));
        }
        else if (strncmp(buf, "NT1", 3) == 0)
        {
            sscanf(buf, "NT1%d", &nt1);
        }

        else if (strcmp(buf, "NR0;") == 0)
        {
            sprintf(buf, "NR0%03d;", nr0);
            write(fd, buf, strlen(buf));
        }
        else if (strncmp(buf, "NR0", 3) == 0)
        {
            sscanf(buf, "NR0%d", &nr0);
        }
        else if (strcmp(buf, "NR1;") == 0)
        {
            sprintf(buf, "NR1%03d;", nr1);
            write(fd, buf, strlen(buf));
        }
        else if (strncmp(buf, "NR1", 3) == 0)
        {
            sscanf(buf, "NR1%d", &nr1);
        }

        else if (strcmp(buf, "GC0;") == 0)
        {
            sprintf(buf, "GC0%03d;", gc0);
            write(fd, buf, strlen(buf));
        }
        else if (strncmp(buf, "GC0", 3) == 0)
        {
            sscanf(buf, "GC0%d", &gc0);
        }
        else if (strcmp(buf, "GC1;") == 0)
        {
            sprintf(buf, "GC1%03d;", gc1);
            write(fd, buf, strlen(buf));
        }
        else if (strncmp(buf, "GC1", 3) == 0)
        {
            sscanf(buf, "GC1%d", &gc1);
        }

        else if (strcmp(buf, "MV0;") == 0)
        {
            sprintf(buf, "MV0%03d;", mv0);
            write(fd, buf, strlen(buf));
        }
        else if (strncmp(buf, "MV0", 3) == 0)
        {
            sscanf(buf, "MV0%d", &mv0);
        }
        else if (strcmp(buf, "MV1;") == 0)
        {
            sprintf(buf, "MV1%03d;", mv1);
            write(fd, buf, strlen(buf));
        }
        else if (strncmp(buf, "MV1", 3) == 0)
        {
            sscanf(buf, "MV1%d", &mv1);
        }

        else if (strcmp(buf, "PA0;") == 0)
        {
            sprintf(buf, "PA0%03d;", pa0);
            write(fd, buf, strlen(buf));
        }
        else if (strncmp(buf, "PA0", 3) == 0)
        {
            sscanf(buf, "PA0%d", &pa0);
        }
        else if (strcmp(buf, "PA1;") == 0)
        {
            sprintf(buf, "PA1%03d;", pa1);
            write(fd, buf, strlen(buf));
        }
        else if (strncmp(buf, "PA1", 3) == 0)
        {
            sscanf(buf, "PA1%d", &pa1);
        }

        else if (strcmp(buf, "RA0;") == 0)
        {
            sprintf(buf, "RA0%03d;", ra0);
            write(fd, buf, strlen(buf));
        }
        else if (strncmp(buf, "RA0", 3) == 0)
        {
            sscanf(buf, "RA0%d", &ra0);
        }
        else if (strcmp(buf, "RA1;") == 0)
        {
            sprintf(buf, "RA1%03d;", ra1);
            write(fd, buf, strlen(buf));
        }
        else if (strncmp(buf, "RA1", 3) == 0)
        {
            sscanf(buf, "RA1%d", &ra1);
        }

        else if (strcmp(buf, "RL10;") == 0)
        {
            sprintf(buf, "RL10%02d;", rl0);
            write(fd, buf, strlen(buf));
        }
        else if (strncmp(buf, "RL10", 3) == 0)
        {
            sscanf(buf, "RL10%d", &rl0);
        }
        else if (strcmp(buf, "RL11;") == 0)
        {
            sprintf(buf, "RL11%02d;", rl1);
            write(fd, buf, strlen(buf));
        }
        else if (strncmp(buf, "RL1", 3) == 0)
        {
            sscanf(buf, "RL1%d", &rl1);
        }

        else if (strcmp(buf, "AG0;") == 0)
        {
            sprintf(buf, "AG0%03d;", ag0);
            write(fd, buf, strlen(buf));
        }
        else if (strncmp(buf, "AG0", 3) == 0)
        {
            sscanf(buf, "AG0%d", &ag0);
        }
        else if (strcmp(buf, "AG1;") == 0)
        {
            sprintf(buf, "AG1%03d;", ag1);
            write(fd, buf, strlen(buf));
        }
        else if (strncmp(buf, "AG1", 3) == 0)
        {
            sscanf(buf, "AG1%d", &ag1);
        }

        else if (strcmp(buf, "SL0;") == 0)
        {
            sprintf(buf, "SL0%03d;", sl0);
            write(fd, buf, strlen(buf));
        }
        else if (strncmp(buf, "SL0", 3) == 0)
        {
            sscanf(buf, "SL0%d", &sl0);
        }
        else if (strcmp(buf, "SL1;") == 0)
        {
            sprintf(buf, "SL1%03d;", sl1);
            write(fd, buf, strlen(buf));
        }
        else if (strncmp(buf, "SL1", 3) == 0)
        {
            sscanf(buf, "SL1%d", &sl1);
        }

        else if (strcmp(buf, "SH0;") == 0)
        {
            sprintf(buf, "SH0%03d;", sh0);
            write(fd, buf, strlen(buf));
        }
        else if (strncmp(buf, "SH0", 3) == 0)
        {
            sscanf(buf, "SH0%d", &sh0);
        }
        else if (strcmp(buf, "SH1;") == 0)
        {
            sprintf(buf, "SH1%03d;", sh1);
            write(fd, buf, strlen(buf));
        }
        else if (strncmp(buf, "SH1", 3) == 0)
        {
            sscanf(buf, "SH1%d", &sh1);
        }

        else if (strcmp(buf, "MO0;") == 0)
        {
            sprintf(buf, "MO0%d;", mo0);
            write(fd, buf, strlen(buf));
        }
        else if (strncmp(buf, "MO0", 3) == 0)
        {
            sscanf(buf, "MO0%d", &mo0);
        }
        else if (strcmp(buf, "MO1;") == 0)
        {
            sprintf(buf, "MO1%d;", mo1);
            write(fd, buf, strlen(buf));
        }
        else if (strncmp(buf, "MO1", 3) == 0)
        {
            sscanf(buf, "MO1%d", &mo1);
        }
        else if (strncmp(buf, "CK0", 3) == 0)
        {
            continue;  // setting clock no action
        }

        else if (strlen(buf) > 0)
        {
            fprintf(stderr, "Unknown command: %s\n", buf);
        }


    }

    return 0;
}
