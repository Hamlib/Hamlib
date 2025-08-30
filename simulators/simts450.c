// can run this using rigctl/rigctld and socat pty devices
#define _XOPEN_SOURCE 700
// since we are POSIX here we need this
#include <stdio.h>
#include <string.h>
#include <sys/types.h>

#include "hamlib/rig.h"
#include "misc.h"


int mysleep = 20;

float freqA = 14074000;
float freqB = 14074500;
int filternum = 7;
int datamode = 0;
int vfo, vfo_tx, ptt, ptt_data, ptt_mic, ptt_tune;
int tomode = 0;
int keyspd = 25;


#include "sim.h"


int main(int argc, char *argv[])
{
    char buf[BUFSIZE];
    char *pbuf;
    int fd = openPort(argv[1]);
    int freqa = 14074000, freqb = 140735000;
    int modeA = 1, modeB = 2;

    while (1)
    {
        buf[0] = 0;

        if (getmyline(fd, buf) > 0) { printf("Cmd:%s\n", buf); }

//        else { return 0; }

        if (strcmp(buf, "RM5;") == 0)
        {
            printf("%s\n", buf);
            hl_usleep(mysleep * 1000);
            pbuf = "RM5100000;";
            WRITE(fd, pbuf, strlen(pbuf));
        }

        else if (strcmp(buf, "AN0;") == 0)
        {
            printf("%s\n", buf);
            hl_usleep(mysleep * 1000);
            pbuf = "AN030;";
            WRITE(fd, pbuf, strlen(pbuf));
        }
        else if (strcmp(buf, "IF;") == 0)
        {
            char ifbuf[256];
            printf("%s\n", buf);
            hl_usleep(mysleep * 1000);
            pbuf = "IF000503130001000+0000000000030000000;";
            sprintf(ifbuf, "IF%011d0001000+0000000000030000000;", freqa);
            //pbuf = "IF00010138698     +00000000002000000 ;
            WRITE(fd, ifbuf, strlen(ifbuf));
        }
        else if (strcmp(buf, "NB;") == 0)
        {
            hl_usleep(mysleep * 1000);
            pbuf = "NB0;";
            WRITE(fd, pbuf, strlen(pbuf));
        }
        else if (strcmp(buf, "RA;") == 0)
        {
            hl_usleep(mysleep * 1000);
            pbuf = "RA01;";
            WRITE(fd, pbuf, strlen(pbuf));
        }
        else if (strcmp(buf, "RG;") == 0)
        {
            hl_usleep(mysleep * 1000);
            pbuf = "RG055;";
            WRITE(fd, pbuf, strlen(pbuf));
        }
        else if (strcmp(buf, "MG;") == 0)
        {
            hl_usleep(mysleep * 1000);
            pbuf = "MG050;";
            WRITE(fd, pbuf, strlen(pbuf));
        }
        else if (strcmp(buf, "AG;") == 0)
        {
            hl_usleep(mysleep * 1000);
            pbuf = "AG100;";
            WRITE(fd, pbuf, strlen(pbuf));
        }
        else if (strcmp(buf, "FV;") == 0)
        {
            hl_usleep(mysleep * 1000);
            pbuf = "FV1.2;";
            WRITE(fd, pbuf, strlen(pbuf));
        }
        else if (strncmp(buf, "IS;", 3) == 0)
        {
            SNPRINTF(buf, sizeof(buf), "IS+0000;");
            WRITE(fd, buf, strlen(buf));
            printf("%s\n", buf);
        }
        else if (strncmp(buf, "IS", 2) == 0)
        {
        }
        else if (strncmp(buf, "SM;", 3) == 0)
        {
            SNPRINTF(buf, sizeof(buf), "SM0035;");
            WRITE(fd, buf, strlen(buf));
            printf("%s\n", buf);
        }
        else if (strncmp(buf, "PC;", 3) == 0)
        {
            SNPRINTF(buf, sizeof(buf), "PC100;");
            WRITE(fd, buf, strlen(buf));
            printf("%s\n", buf);
        }
        else if (strcmp(buf, "FW;") == 0)
        {
            //usleep(mysleep * 1000);
            pbuf = "FW240";
            WRITE(fd, pbuf, strlen(pbuf));
            hl_usleep(20 * 1000);
            pbuf = "0;";
            WRITE(fd, pbuf, strlen(pbuf));
        }
        else if (strncmp(buf, "FW", 2) == 0)
        {
        }
        else if (strcmp(buf, "ID;") == 0)
        {
            printf("%s\n", buf);
            hl_usleep(mysleep * 1000);
            SNPRINTF(buf, sizeof(buf), "ID%03d;", 10);
            WRITE(fd, buf, strlen(buf));
        }

        else if (strcmp(buf, "VS;") == 0)
        {
            printf("%s\n", buf);
            hl_usleep(mysleep * 1000);
            pbuf = "VS0;";
            WRITE(fd, pbuf, strlen(pbuf));
        }
        else if (strcmp(buf, "EX032;") == 0)
        {
            static int ant = 0;
            ant = (ant + 1) % 3;
            printf("%s\n", buf);
            hl_usleep(mysleep * 1000);
            SNPRINTF(buf, sizeof(buf), "EX032%1d;", ant);
            WRITE(fd, buf, strlen(buf));
        }
        else if (strncmp(buf, "EX", 2) == 0)
        {
        }
        else if (strcmp(buf, "FA;") == 0)
        {
            SNPRINTF(buf, sizeof(buf), "FA%011d;", freqa);
            WRITE(fd, buf, strlen(buf));
        }
        else if (strcmp(buf, "FB;") == 0)
        {
            SNPRINTF(buf, sizeof(buf), "FB%011d;", freqb);
            WRITE(fd, buf, strlen(buf));
        }
        else if (strncmp(buf, "FA", 2) == 0)
        {
            sscanf(buf, "FA%d", &freqa);
        }
        else if (strncmp(buf, "FB", 2) == 0)
        {
            sscanf(buf, "FB%d", &freqb);
        }
        else if (strncmp(buf, "AI", 2) == 0)
        {
            // nothing to do yet
        }

        else if (strncmp(buf, "PS;", 3) == 0)
        {
            SNPRINTF(buf, sizeof(buf), "PS1;");
            WRITE(fd, buf, strlen(buf));
        }
        else if (strncmp(buf, "SA;", 3) == 0)
        {
            SNPRINTF(buf, sizeof(buf), "SA0;");
            WRITE(fd, buf, strlen(buf));
        }
        else if (buf[3] == ';' && strncmp(buf, "SF", 2) == 0)
        {
            SNPRINTF(buf, sizeof(buf), "SF%c%011.0f%c;", buf[2],
                     buf[2] == '0' ? freqA : freqB,
                     buf[2] == '0' ? modeA + '0' : modeB + '0');
            WRITE(fd, buf, strlen(buf));
        }
        else if (strncmp(buf, "SF", 2) == 0)
        {
            mode_t tmpmode = buf[14];

            if (buf[2] == '0') { modeA = tmpmode - '0'; }
            else { modeB = tmpmode - '0'; }

            printf("modeA=%c, modeB=%c\n", modeA, modeB);

        }
        else if (strncmp(buf, "MD;", 3) == 0)
        {
            SNPRINTF(buf, sizeof(buf), "MD%d;",
                     modeA); // not worried about modeB yet for simulator
            WRITE(fd, buf, strlen(buf));
        }
        else if (strncmp(buf, "MD", 2) == 0)
        {
            sscanf(buf, "MD%d", &modeA); // not worried about modeB yet for simulator
        }
        else if (strncmp(buf, "FL;", 3) == 0)
        {
            SNPRINTF(buf, sizeof(buf), "FL%03d;", filternum);
            WRITE(fd, buf, strlen(buf));
        }
        else if (strncmp(buf, "FL", 2) == 0)
        {
            sscanf(buf, "FL%d", &filternum);
        }
        else if (strcmp(buf, "FR;") == 0)
        {
            SNPRINTF(buf, sizeof(buf), "FR%d;", vfo);
            WRITE(fd, buf, strlen(buf));
        }
        else if (strncmp(buf, "FR", 2) == 0)
        {
            sscanf(buf, "FR%d", &vfo);
        }
        else if (strcmp(buf, "FT;") == 0)
        {
            SNPRINTF(buf, sizeof(buf), "FT%d;", vfo_tx);
            WRITE(fd, buf, strlen(buf));
        }
        else if (strncmp(buf, "FT", 2) == 0)
        {
            sscanf(buf, "FT%d", &vfo_tx);
        }
        else if (strncmp(buf, "DA;", 3) == 0)
        {
            SNPRINTF(buf, sizeof(buf), "DA%d;", datamode);
            WRITE(fd, buf, strlen(buf));
            printf("%s\n", buf);
        }
        else if (strncmp(buf, "DA", 2) == 0)
        {
            sscanf(buf, "DA%d", &datamode);
            printf("%s\n", buf);
        }
        else if (strncmp(buf, "TO;", 3) == 0)
        {
            SNPRINTF(buf, sizeof(buf), "TO%d;", tomode);
        }
        else if (strncmp(buf, "BD;", 3) == 0)
        {
        }
        else if (strncmp(buf, "BU;", 3) == 0)
        {
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

        }

        else if (strlen(buf) > 0)
        {
            fprintf(stderr, "Unknown command: %s\n", buf);
        }
    }

    return 0;
}
