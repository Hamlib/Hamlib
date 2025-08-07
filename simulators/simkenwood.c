// can run this using rigctl/rigctld and socat pty devices
#define _XOPEN_SOURCE 700
// since we are POSIX here we need this
#include <stdio.h>
#include <string.h>
#include <unistd.h>

#include "hamlib/rig.h"
#include "misc.h"


int mysleep = 20;

float freqA = 14074000;
float freqB = 14074500;
int filternum = 7;
int datamode = 0;
int vfo, vfo_tx, ptt, ptt_data, ptt_mic, ptt_tune;


#include "sim.h"


int main(int argc, char *argv[])
{
    char buf[BUFSIZE];
    char *pbuf;
    int fd = openPort(argv[1]);
    int freqa = 14074000, freqb = 140735000;
    int modeA = 0; // , modeB = 0;

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
            write(fd, pbuf, strlen(pbuf));
        }

        else if (strcmp(buf, "AN0;") == 0)
        {
            printf("%s\n", buf);
            hl_usleep(mysleep * 1000);
            pbuf = "AN030;";
            write(fd, pbuf, strlen(pbuf));
        }
        else if (strcmp(buf, "IF;") == 0)
        {
            char ifbuf[256];
            printf("%s\n", buf);
            hl_usleep(mysleep * 1000);
            pbuf = "IF000503130001000+0000000000030000000;";
            sprintf(ifbuf, "IF%011d0001000+0000000000030000000;", freqa);
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
        else if (strcmp(buf, "MG;") == 0)
        {
            hl_usleep(mysleep * 1000);
            pbuf = "MG050;";
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
            printf("%s\n", buf);
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
            printf("%s\n", buf);
            continue;
        }
        else if (strncmp(buf, "PC;", 3) == 0)
        {
            SNPRINTF(buf, sizeof(buf), "PC100;");
            write(fd, buf, strlen(buf));
            printf("%s\n", buf);
            continue;
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
            printf("%s\n", buf);
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
                printf("%s\n", buf);
                hl_usleep(mysleep * 1000);
                fprintf(fp, "%s", "AI0;");

            }
        }

#endif
        else if (strcmp(buf, "VS;") == 0)
        {
            printf("%s\n", buf);
            hl_usleep(mysleep * 1000);
            pbuf = "VS0;";
            write(fd, pbuf, strlen(pbuf));
            continue;
        }
        else if (strcmp(buf, "EX032;") == 0)
        {
            static int ant = 0;
            ant = (ant + 1) % 3;
            printf("%s\n", buf);
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
            SNPRINTF(buf, sizeof(buf), "FA%011d;", freqa);
            write(fd, buf, strlen(buf));
            continue;
        }
        else if (strcmp(buf, "FB;") == 0)
        {
            SNPRINTF(buf, sizeof(buf), "FB%011d;", freqb);
            write(fd, buf, strlen(buf));
            continue;
        }
        else if (strncmp(buf, "FA", 2) == 0)
        {
            sscanf(buf, "FA%d", &freqa);
            continue;
        }
        else if (strncmp(buf, "FB", 2) == 0)
        {
            sscanf(buf, "FB%d", &freqb);
            continue;
        }
        else if (strncmp(buf, "AI;", 3) == 0)
        {
            SNPRINTF(buf, sizeof(buf), "AI0;");
            write(fd, buf, strlen(buf));
            continue;
        }

        if (strncmp(buf, "PS;", 3) == 0)
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
        else if (strncmp(buf, "MD;", 3) == 0)
        {
            SNPRINTF(buf, sizeof(buf), "MD%d;",
                     modeA); // not worried about modeB yet for simulator
            write(fd, buf, strlen(buf));
            continue;
        }
        else if (strncmp(buf, "MD", 2) == 0)
        {
            sscanf(buf, "MD%d", &modeA); // not worried about modeB yet for simulator
            continue;
        }
        else if (strncmp(buf, "FL;", 3) == 0)
        {
            SNPRINTF(buf, sizeof(buf), "FL%03d;", filternum);
            write(fd, buf, strlen(buf));
            continue;
        }
        else if (strncmp(buf, "FL", 2) == 0)
        {
            sscanf(buf, "FL%d", &filternum);
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
            printf("%s\n", buf);
            continue;
        }
        else if (strncmp(buf, "DA", 2) == 0)
        {
            sscanf(buf, "DA%d", &datamode);
            printf("%s\n", buf);
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
        else if (strlen(buf) > 0)
        {
            fprintf(stderr, "Unknown command: %s\n", buf);
        }


    }

    return 0;
}
