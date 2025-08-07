// can run this using rigctl/rigctld and socat pty devices
#define _XOPEN_SOURCE 700
// since we are POSIX here we need this
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "hamlib/rig.h"
#include "misc.h"


float freqA = 14074000;
float freqB = 14074500;
char tx_vfo = '0';
char rx_vfo = '0';
vfo_t curr_vfo = RIG_VFO_A;
char modeA = '1';
char modeB = '1';
int width = 0;
int ptt;


#include "sim.h"


int main(int argc, char *argv[])
{
    char buf[BUFSIZE];
    char resp[256];
    char *pbuf;
    int n;
    int fd = openPort(argv[1]);

    while (1)
    {
        buf[0] = 0;

        if (getmyline(fd, buf))
        {
            printf("Cmd:%s\n", buf);
        }

        //else { return 0; }

        if (strcmp(buf, "RM5;") == 0)
        {
            printf("%s\n", buf);
            hl_usleep(50 * 1000);
            pbuf = "RM5100000;";
            n = write(fd, pbuf, strlen(pbuf));
            printf("n=%d\n", n);

            if (n <= 0) { perror("RM5"); }
        }

        if (strcmp(buf, "AN0;") == 0)
        {
            printf("%s\n", buf);
            hl_usleep(50 * 1000);
            pbuf = "AN030;";
            n = write(fd, pbuf, strlen(pbuf));
            printf("n=%d\n", n);

            if (n <= 0) { perror("AN"); }
        }
        else if (strcmp(buf, "FA;") == 0)
        {
            //SNPRINTF(resp, sizeof(resp), "FA%010.0f;", freqA);
            SNPRINTF(resp, sizeof(resp), "FA%08.0f;", freqA);
            freqA += 10;
            n = write(fd, resp, strlen(resp));
        }
        else if (strncmp(buf, "FA", 2) == 0)
        {
            sscanf(buf, "FA%f", &freqA);
        }
        else if (strcmp(buf, "FB;") == 0)
        {
            //SNPRINTF(resp, sizeof(resp), "FB%0010.0f;", freqB);
            SNPRINTF(resp, sizeof(resp), "FB%08.0f;", freqB);
            n = write(fd, resp, strlen(resp));
        }
        else if (strncmp(buf, "FB", 2) == 0)
        {
            sscanf(buf, "FB%f", &freqB);
        }
        else if (strcmp(buf, "IF;") == 0)
        {
            printf("%s\n", buf);
            hl_usleep(50 * 1000);
            //pbuf = "IF00107041000+000000200000;";
            pbuf = "IF00010138698     +00000000002000000 ;";
            n = write(fd, pbuf, strlen(pbuf));
            printf("n=%d\n", n);

            if (n <= 0) { perror("IF"); }
        }
        else if (strcmp(buf, "ID;") == 0)
        {
            printf("%s\n", buf);
            hl_usleep(50 * 1000);
            int id = NC_RIGID_FTDX3000DM;
            SNPRINTF(buf, sizeof(buf), "ID%03d;", id);
            n = write(fd, buf, strlen(buf));
            printf("n=%d\n", n);

            if (n <= 0) { perror("ID"); }
        }
        else if (strcmp(buf, "AI;") == 0)
        {
            printf("%s\n", buf);
            hl_usleep(50 * 1000);
            SNPRINTF(buf, sizeof(buf), "AI0;");
            n = write(fd, buf, strlen(buf));
            printf("n=%d\n", n);

            if (n <= 0) { perror("ID"); }
        }
        else if (strcmp(buf, "AI0;") == 0)
        {
            hl_usleep(50 * 1000);
        }
        else if (strcmp(buf, "AB;") == 0)
        {
            freqB = freqA;
            modeB = modeA;
        }

#if 0
        else if (strncmp(buf, "AI", 2) == 0)
        {
            if (strcmp(buf, "AI;"))
            {
                printf("%s\n", buf);
                hl_usleep(50 * 1000);
                n = fprintf(fp, "%s", "AI0;");
                printf("n=%d\n", n);

                if (n <= 0) { perror("AI"); }
            }
        }

#endif
        else if (strcmp(buf, "VS") == 0 && strlen(buf) > 3)
        {
            curr_vfo = buf[3] == '1' ? RIG_VFO_B : RIG_VFO_A;
            hl_usleep(50 * 1000);
        }
        else if (strcmp(buf, "VS;") == 0)
        {
            printf("%s\n", buf);
            hl_usleep(50 * 1000);
            pbuf = strdup("VS0;");

            if (curr_vfo == RIG_VFO_B || curr_vfo == RIG_VFO_SUB) { pbuf[2] = '1'; }

            n = write(fd, pbuf, strlen(pbuf));
            printf("%s\n", pbuf);
            free(pbuf);

            if (n < 0) { perror("VS"); }
        }
        else if (strcmp(buf, "FT;") == 0)
        {
            hl_usleep(50 * 1000);
            SNPRINTF(resp, sizeof(resp), "FT%c;", tx_vfo);
            printf(" FT response#1=%s, tx_vfo=%c\n", resp, tx_vfo);
            n = write(fd, resp, strlen(resp));
            printf(" FT response#2=%s\n", resp);

            if (n < 0) { perror("FT"); }
        }
        else if (strncmp(buf, "FT", 2) == 0)
        {
            tx_vfo = buf[2];

            if (tx_vfo == '3') { tx_vfo = '1'; }
            else if (tx_vfo == '2') { tx_vfo = '0'; }
            else { perror("Expected 2 or 3"); }
        }
        else if (strcmp(buf, "MD0;") == 0)
        {
            hl_usleep(50 * 1000);
            SNPRINTF(resp, sizeof(resp), "MD0%c;", modeA);
            n = write(fd, resp, strlen(resp));

            if (n < 0) { perror("MD0;"); }
        }
        else if (strncmp(buf, "MD0", 3) == 0)
        {
            modeA = buf[3];
        }
        else if (strcmp(buf, "MD1;") == 0)
        {
            hl_usleep(50 * 1000);
            SNPRINTF(resp, sizeof(resp), "MD1%c;", modeB);
            n = write(fd, resp, strlen(resp));

            if (n < 0) { perror("MD0;"); }
        }
        else if (strncmp(buf, "MD1", 3) == 0)
        {
            modeB = buf[3];
        }
        else if (strcmp(buf, "SM0;") == 0)
        {
            hl_usleep(50 * 1000);
            SNPRINTF(resp, sizeof(resp), "SM0111;");
            n = write(fd, resp, strlen(resp));

            if (n < 0) { perror("SM"); }
        }
        else if (strcmp(buf, "TX;") == 0)
        {
            hl_usleep(50 * 1000);
            SNPRINTF(resp, sizeof(resp), "TX%d;", ptt);
            n = write(fd, resp, strlen(resp));

            if (n < 0) { perror("TX"); }
        }
        else if (strncmp(buf, "TX", 2) == 0)
        {
            hl_usleep(50 * 1000);
            ptt = buf[2] == '0' ? 0 : 1;
        }
        else if (strcmp(buf, "EX032;") == 0)
        {
            static int ant = 0;
            ant = (ant + 1) % 3;
            printf("%s\n", buf);
            hl_usleep(50 * 1000);
            SNPRINTF(buf, sizeof(buf), "EX032%1d;", ant);
            n = write(fd, buf, strlen(buf));
            printf("n=%d\n", n);

            if (n < 0) { perror("EX032"); }
        }
        else if (strcmp(buf, "SH0;") == 0)
        {
            SNPRINTF(buf, sizeof(buf), "SH0%02d;", width);
            hl_usleep(50 * 1000);
            n = write(fd, buf, strlen(buf));
            printf("%s n=%d\n", buf, n);
        }
        else if (strcmp(buf, "NA0;") == 0)
        {
            SNPRINTF(buf, sizeof(buf), "NA00;");
            hl_usleep(50 * 1000);
            n = write(fd, buf, strlen(buf));
            printf("%s n=%d\n", buf, n);
        }

        else if (strlen(buf) > 0)
        {
            fprintf(stderr, "Unknown command: %s\n", buf);
        }

        if (n == 0) { fprintf(stderr, "Write error? n==0\n"); }

    }

    return 0;
}
