// can run this using rigctl/rigctld and socat pty devices
#define _XOPEN_SOURCE 700
// since we are POSIX here we need this
#include <stdio.h>
#include <string.h>

#include "hamlib/rig.h"
#include "sim.h"
#include "misc.h"


int freqA = 14074000;
int freqB = 14074500;
int afgain = 180;
int rfgain = 190;
int micgain = 30;
int noiseblanker = 0;
int bandwidthA = 200;
int bandwidthB = 200;
int ifshift = 0;
int preampA = 0;
int preampB = 0;
int rxattenuatorA = 0;
int rxattenuatorB = 0;
int keyspd = 20;
int ai = 0;
int dt = 0;
int modeA = 2;
int modeB = 2;
int ptt = 0;
//    int freqa = 14074000, freqb = 14073500;




int main(int argc, char *argv[])
{
    char buf[BUFSIZE];
    char *pbuf;
    int n;
    int fd = openPort(argv[1]);

    while (1)
    {
        buf[0] = 0;

        if ((n = getmyline(fd, buf)) > 0) { if (strstr(buf, "BW0")) printf("Cmd:%s, len=%d\n", buf, n); }
        else {continue; }

        if (strcmp(buf, "RM5;") == 0)
        {
            printf("%s\n", buf);
            hl_usleep(50 * 1000);
            pbuf = "RM5100000;";
            WRITE(fd, pbuf, strlen(pbuf));
        }
        else if (strcmp(buf, "AI;") == 0)
        {
            SNPRINTF(buf, sizeof(buf), "AI%d;", ai);
            WRITE(fd, buf, strlen(buf));
        }
        else if (strncmp(buf, "AI", 2) == 0)
        {
            sscanf(buf, "AI%d", &ai);
        }

        else if (strcmp(buf, "AN0;") == 0)
        {
            printf("%s\n", buf);
            hl_usleep(50 * 1000);
            pbuf = "AN030;";
            WRITE(fd, pbuf, strlen(pbuf));
        }
        else if (strcmp(buf, "IF;") == 0)
        {
            printf("%s\n", buf);
            hl_usleep(50 * 1000);
            //pbuf = "IF059014200000+000000700000;";
            pbuf = "IF00007230000     -000000 0001000001 ;" ;
            WRITE(fd, pbuf, strlen(pbuf));
        }
        else if (strcmp(buf, "ID;") == 0)
        {
            printf("%s\n", buf);
            hl_usleep(50 * 1000);
            int id = 24;
            SNPRINTF(buf, sizeof(buf), "ID%03d;", id);
            WRITE(fd, buf, strlen(buf));
        }
        else if (strcmp(buf, "PS;") == 0)
        {
            SNPRINTF(buf, sizeof(buf), "PS1;");
            WRITE(fd, buf, strlen(buf));
        }
        else if (strcmp(buf, "BW$;") == 0)
        {
            fprintf(stderr, "***** %d\n", __LINE__);
            SNPRINTF(buf, sizeof(buf), "BW$%04d;", bandwidthB);
            WRITE(fd, buf, strlen(buf));
        }
        else if (strncmp(buf, "BW$", 3) == 0)
        {
            sscanf(buf, "BW$%d", &bandwidthB);
        }
        else if (strcmp(buf, "BW;") == 0)
        {
            SNPRINTF(buf, sizeof(buf), "BW%04d;", bandwidthA);
            WRITE(fd, buf, strlen(buf));
        }
        else if (strncmp(buf, "BW", 2) == 0)
        {
            sscanf(buf, "BW%d", &bandwidthA);
        }
        else if (strcmp(buf, "DT;") == 0)
        {
            SNPRINTF(buf, sizeof(buf), "DT%d;", dt);
            WRITE(fd, buf, strlen(buf));
        }
        else if (strncmp(buf, "DT", 2) == 0)
        {
            sscanf(buf, "DT%d", &dt);
        }
        else if (strcmp(buf, "BN;") == 0)
        {
            SNPRINTF(buf, sizeof(buf), "BN03;");
            WRITE(fd, buf, strlen(buf));
        }
        else if (strcmp(buf, "SM;") == 0)
        {
            static int meter = 0;
            SNPRINTF(buf, sizeof(buf), "SM%04d;", meter++);

            if (meter > 15) { meter = 0; }

            WRITE(fd, buf, strlen(buf));
        }
        else if (strcmp(buf, "RG;") == 0)
        {
            SNPRINTF(buf, sizeof(buf), "RG%03d;", rfgain);
            WRITE(fd, buf, strlen(buf));
        }
        else if (strncmp(buf, "RG", 2) == 0)
        {
            sscanf(buf, "RG%d", &rfgain);
        }
        else if (strcmp(buf, "MG;") == 0)
        {
            SNPRINTF(buf, sizeof(buf), "MG%03d;", micgain);
            WRITE(fd, buf, strlen(buf));
        }
        else if (strncmp(buf, "MG", 2) == 0)
        {
            sscanf(buf, "MG%d", &micgain);
        }
        else if (strcmp(buf, "AG;") == 0)
        {
            SNPRINTF(buf, sizeof(buf), "MG%03d;", afgain);
            WRITE(fd, buf, strlen(buf));
        }
        else if (strncmp(buf, "AG", 2) == 0)
        {
            sscanf(buf, "AG%d", &afgain);
        }
        else if (strcmp(buf, "NB;") == 0)
        {
            SNPRINTF(buf, sizeof(buf), "NB%d;", noiseblanker);
            WRITE(fd, buf, strlen(buf));
        }
        else if (strncmp(buf, "NB", 2) == 0)
        {
            sscanf(buf, "NB%d", &noiseblanker);
        }
        else if (strcmp(buf, "IS;") == 0)
        {
            SNPRINTF(buf, sizeof(buf), "IS %04d;", ifshift);
            WRITE(fd, buf, strlen(buf));
        }
        else if (strncmp(buf, "IS", 2) == 0)
        {
            sscanf(buf, "IS %d", &ifshift);
        }


#if 0
        else if (strncmp(buf, "AI", 2) == 0)
        {
            if (strcmp(buf, "AI;"))
            {
                printf("%s\n", buf);
                hl_usleep(50 * 1000);
                n = fprintf(fp, "%s", "AI0;");

                if (n <= 0) { perror("AI"); }
            }
        }

#endif
        else if (strcmp(buf, "VS;") == 0)
        {
            printf("%s\n", buf);
            hl_usleep(50 * 1000);
            pbuf = "VS0;";
            WRITE(fd, pbuf, strlen(pbuf));
        }
        else if (strcmp(buf, "EX032;") == 0)
        {
            static int ant = 0;
            ant = (ant + 1) % 3;
            printf("%s\n", buf);
            hl_usleep(50 * 1000);
            SNPRINTF(buf, sizeof(buf), "EX032%1d;", ant);
            WRITE(fd, buf, strlen(buf));
        }
        else if (strcmp(buf, "OM;") == 0)
        {
            // KPA3 SNPRINTF(buf, sizeof(buf), "OM AP----L-----;");
            // K4+KPA3
            SNPRINTF(buf, sizeof(buf), "OM AP-S----4---;");
            WRITE(fd, buf, strlen(buf));
        }
        else if (strcmp(buf, "K2;") == 0)
        {
            WRITE(fd, "K20;", 4);
        }
        else if (strcmp(buf, "K3;") == 0)
        {
            WRITE(fd, "K30;", 4);
        }
        else if (strncmp(buf, "RV", 2) == 0)
        {
            WRITE(fd, "RV02.37;", 8);
        }
        else if (strcmp(buf, "MD;") == 0)
        {
            SNPRINTF(buf, sizeof(buf), "MD%d;", modeA);
            WRITE(fd, buf, strlen(buf));
        }
        else if (strcmp(buf, "MD$;") == 0)
        {
            SNPRINTF(buf, sizeof(buf), "MD$%d;", modeB);
            WRITE(fd, buf, strlen(buf));
        }
        else if (strncmp(buf, "MD", 2) == 0)
        {
            if (buf[2] == '$') { sscanf(buf, "MD$%d;", &modeB); }
            else { sscanf(buf, "MD%d;", &modeA); }
        }
        else if (strcmp(buf, "FA;") == 0)
        {
            SNPRINTF(buf, sizeof(buf), "FA%011d;", freqA);
            WRITE(fd, buf, strlen(buf));
        }
        else if (strcmp(buf, "FB;") == 0)
        {
            SNPRINTF(buf, sizeof(buf), "FB%011d;", freqB);
            WRITE(fd, buf, strlen(buf));
        }

        else if (strncmp(buf, "FA", 2) == 0)
        {
            sscanf(buf, "FA%d", &freqA);
        }
        else if (strncmp(buf, "FB", 2) == 0)
        {
            sscanf(buf, "FB%d", &freqB);
        }
        else if (strncmp(buf, "FR;", 3) == 0)
        {
            SNPRINTF(buf, sizeof(buf), "FR0;");
            WRITE(fd, buf, strlen(buf));
        }
        else if (strncmp(buf, "FR", 2) == 0)
        {
            // we ignore FR for the K3
        }
        else if (strncmp(buf, "FT;", 3) == 0)
        {
            SNPRINTF(buf, sizeof(buf), "FT0;");
            WRITE(fd, buf, strlen(buf));
        }
        else if (strncmp(buf, "KS;", 3) == 0)
        {
            SNPRINTF(buf, sizeof(buf), "KS%03d;", keyspd);
            WRITE(fd, buf, strlen(buf));
        }
        else if (strncmp(buf, "KS", 2) == 0)
        {
            sscanf(buf, "KS%d", &keyspd);
        }
        else if (strncmp(buf, "TQ;", 3) == 0)
        {
            SNPRINTF(buf, sizeof(buf), "TQ%d;", ptt);
            WRITE(fd, buf, strlen(buf));
        }
        else if (strncmp(buf, "PC;", 3) == 0)
        {
            SNPRINTF(buf, sizeof(buf), "PC0980;");
            WRITE(fd, buf, strlen(buf));
        }
        else if (strncmp(buf, "PA;", 3) == 0)
        {
            SNPRINTF(buf, sizeof(buf), "PA%d;", preampA);
            WRITE(fd, buf, strlen(buf));
        }
        else if (strncmp(buf, "PA$;", 4) == 0)
        {
            SNPRINTF(buf, sizeof(buf), "PA$%d;", preampB);
            WRITE(fd, buf, strlen(buf));
        }
        else if (strncmp(buf, "PA", 2) == 0)
        {
            sscanf(buf, "PA%d;", &preampA);
        }
        else if (strncmp(buf, "PA$", 3) == 0)
        {
            sscanf(buf, "PA$%d;", &preampB);
        }
        else if (strncmp(buf, "RA;", 3) == 0)
        {
            SNPRINTF(buf, sizeof(buf), "RA%02d;", rxattenuatorA);
            WRITE(fd, buf, strlen(buf));
        }
        else if (strncmp(buf, "RA$;", 4) == 0)
        {
            SNPRINTF(buf, sizeof(buf), "RA$%02d;", rxattenuatorA);
            WRITE(fd, buf, strlen(buf));
        }
        else if (strncmp(buf, "RA", 2) == 0)
        {
            sscanf(buf, "RA%d;", &rxattenuatorB);
        }
        else if (strncmp(buf, "RA$", 3) == 0)
        {
            sscanf(buf, "RA$%d;", &rxattenuatorB);
        }
        else if (strncmp(buf, "KY;", 3) == 0)
        {
            int status = 0;
            printf("KY query\n");
            SNPRINTF(buf, sizeof(buf), "KY%d;", status);
            WRITE(fd, buf, strlen(buf));
        }
        else if (strncmp(buf, "KY", 2) == 0)
        {
            printf("Morse: %s\n", buf);
        }
        else if (strncmp(buf, "TM", 2) == 0)
        {
            SNPRINTF(buf, sizeof(buf), "TM001002003004;");
            WRITE(fd, buf, strlen(buf));
        }
        else if (strncmp(buf, "TX", 2) == 0)
        {
            ptt = 1;
        }
        else if (strncmp(buf, "RX", 2) == 0)
        {
            ptt = 0;
        }
        else if (strlen(buf) > 0)
        {
            fprintf(stderr, "Unknown command: %s\n", buf);
        }
    }

    return 0;
}
