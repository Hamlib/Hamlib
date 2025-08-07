// can run this using rigctl/rigctld and socat pty devices
#define _XOPEN_SOURCE 700
// since we are POSIX here we need this
#include <stdio.h>
#include <string.h>
#include <unistd.h>

#include "hamlib/rig.h"
#include "misc.h"


float freqA = 14074000;
float freqB = 14074500;
int vfo = 0;
int ft = 0;
int md = 1;
int vs = 0;
int tx = 0;
int ai = 0;
int sh = 25;
int na = 0;
int ex039 = 0;
int keyspd = 20;


#include "sim.h"


int main(int argc, char *argv[])
{
    char buf[BUFSIZE];
    char *pbuf;
    int n;
    int fd = openPort(argv[1]);

    while (1)
    {
        if (getmyline(fd, buf))
        {
            printf("Cmd:%s\n", buf);
        }
        else { continue; }

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
        else if (strcmp(buf, "IF;") == 0)
        {
            printf("%s\n", buf);
            hl_usleep(50 * 1000);
            pbuf = "IF059014200000+000000700000;";
            n = write(fd, pbuf, strlen(pbuf));
            printf("n=%d\n", n);

            if (n <= 0) { perror("IF"); }
        }
        else if (strcmp(buf, "ID;") == 0)
        {
            printf("%s\n", buf);
            hl_usleep(50 * 1000);
            int id = NC_RIGID_FTDX3000;
            SNPRINTF(buf, sizeof(buf), "ID%03d;", id);
            n = write(fd, buf, strlen(buf));
            printf("n=%d\n", n);

            if (n <= 0) { perror("ID"); }
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

        else if (strcmp(buf, "FA;") == 0)
        {
            SNPRINTF(buf, sizeof(buf), "FA%08.0f;", freqA);
            n = write(fd, buf, strlen(buf));
        }
        else if (strncmp(buf, "FA", 2) == 0)
        {
            sscanf(buf, "FA%f", &freqA);
        }
        else if (strcmp(buf, "FB;") == 0)
        {
            SNPRINTF(buf, sizeof(buf), "FB%08.0f;", freqB);
            n = write(fd, buf, strlen(buf));
        }
        else if (strncmp(buf, "FB", 2) == 0)
        {
            sscanf(buf, "FB%f", &freqB);
        }
        else if (strcmp(buf, "FT;") == 0)
        {
            SNPRINTF(buf, sizeof(buf), "FT%d;", ft);
            n = write(fd, buf, strlen(buf));
        }
        else if (strcmp(buf, "MD0;") == 0)
        {
            SNPRINTF(buf, sizeof(buf), "MD0%d;", md);
            n = write(fd, buf, strlen(buf));
        }
        else if (strcmp(buf, "VS;") == 0)
        {
            SNPRINTF(buf, sizeof(buf), "VS%c;", vfo == 0 ? '0' : '1');
            n = write(fd, buf, strlen(buf));
        }
        else if (strncmp(buf, "VS", 2) == 0)
        {
            sscanf(buf, "VS%d", &vs);
        }
        else if (strcmp(buf, "TX;") == 0)
        {
            SNPRINTF(buf, sizeof(buf), "TX%d;", tx);
            n = write(fd, buf, strlen(buf));
        }
        else if (strncmp(buf, "TX", 2) == 0)
        {
            sscanf(buf, "TX%d", &tx);
        }
        else if (strcmp(buf, "AI;") == 0)
        {
            SNPRINTF(buf, sizeof(buf), "AI%d;", ai);
            n = write(fd, buf, strlen(buf));
        }
        else if (strncmp(buf, "AI", 2) == 0)
        {
            sscanf(buf, "AI%d", &ai);
        }
        else if (strcmp(buf, "SH0;") == 0)
        {
            SNPRINTF(buf, sizeof(buf), "SH0%d;", sh);
            n = write(fd, buf, strlen(buf));
        }
        else if (strncmp(buf, "SH0", 3) == 0)
        {
            sscanf(buf, "SH0%d", &sh);
        }
        else if (strcmp(buf, "NA0;") == 0)
        {
            SNPRINTF(buf, sizeof(buf), "NA0%d;", na);
            n = write(fd, buf, strlen(buf));
        }
        else if (strncmp(buf, "NA0", 3) == 0)
        {
            sscanf(buf, "NA0%d", &na);
        }
        else if (strcmp(buf, "EX039;") == 0)
        {
            SNPRINTF(buf, sizeof(buf), "EX039%d;", ex039);
            n = write(fd, buf, strlen(buf));
        }
        else if (strncmp(buf, "EX039", 5) == 0)
        {
            sscanf(buf, "EX039%d", &ex039);
        }
        else if (strcmp(buf, "PS;") == 0)
        {
            SNPRINTF(buf, sizeof(buf), "PS1;");
            n = write(fd, buf, strlen(buf));
        }
        else if (strncmp(buf, "KS;", 3) == 0)
        {
            sprintf(buf, "KS%d;", keyspd);
            n = write(fd, buf, strlen(buf));
        }
        else if (strncmp(buf, "KS", 2) == 0)
        {
            sscanf(buf, "KS%03d", &keyspd);
        }

        else if (strlen(buf) > 0)
        {
            fprintf(stderr, "Unknown command: %s\n", buf);
        }

    }

    return 0;
}
