// can run this using rigctl/rigctld and socat pty devices
// gcc -o simyaesu simyaesu.c
#define _XOPEN_SOURCE 600
#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>
#include <string.h>
#include <unistd.h>
#include <hamlib/rig.h>

#define BUFSIZE 256

float freqA = 14074000;
float freqB = 14074500;
char tx_vfo = '0';
char rx_vfo = '0';
vfo_t curr_vfo = RIG_VFO_A;
char modeA = '1';
char modeB = '1';
int ptt;
int power = 1;
int roofing_filter_main = 1;
int roofing_filter_sub = 6;
int width_main = 0;
int width_sub = 0;
int ex039 = 0;
int lk = 0;

// ID 0310 == 310, Must drop leading zero
typedef enum nc_rigid_e
{
    NC_RIGID_NONE            = 0,
    NC_RIGID_FT450           = 241,
    NC_RIGID_FT450D          = 244,
    NC_RIGID_FT950           = 310,
    NC_RIGID_FT891           = 135,
    NC_RIGID_FT991           = 135,
    NC_RIGID_FT2000          = 251,
    NC_RIGID_FT2000D         = 252,
    NC_RIGID_FTDX1200        = 583,
    NC_RIGID_FTDX9000D       = 101,
    NC_RIGID_FTDX9000Contest = 102,
    NC_RIGID_FTDX9000MP      = 103,
    NC_RIGID_FTDX5000        = 362,
    NC_RIGID_FTDX3000        = 460,
    NC_RIGID_FTDX3000DM      = 462,
    NC_RIGID_FTDX101D        = 681,
    NC_RIGID_FTDX101MP       = 682
} nc_rigid_t;

int
getmyline(int fd, char *buf)
{
    char c;
    int i = 0;
    memset(buf, 0, BUFSIZE);

    while (read(fd, &c, 1) > 0)
    {
        buf[i++] = c;

        if (c == ';') { return strlen(buf); }
    }

    return strlen(buf);
}

#if defined(WIN32) || defined(_WIN32)
int openPort(char *comport) // doesn't matter for using pts devices
{
    int fd;
    fd = open(comport, O_RDWR);

    if (fd < 0)
    {
        perror(comport);
    }

    return fd;
}

#else
int openPort(char *comport) // doesn't matter for using pts devices
{
    int fd = posix_openpt(O_RDWR);
    char *name = ptsname(fd);

    if (name == NULL)
    {
        perror("pstname");
        return -1;
    }

    printf("name=%s\n", name);

    if (fd == -1 || grantpt(fd) == -1 || unlockpt(fd) == -1)
    {
        perror("posix_openpt");
        return -1;
    }

    return fd;
}
#endif



int main(int argc, char *argv[])
{
    char buf[256];
    char resp[256];
    char *pbuf;
    int n;
    int fd = openPort(argv[1]);

    while (1)
    {
        if (getmyline(fd, buf))
        {
            printf("Cmd:%s\n", buf);
        }

        //else { return 0; }

        if (buf[0] == 0x0a)
        {
            continue;
        }

        if (power == 0 && strcmp(buf, "PS1;") != 0) { continue; }

        if (strcmp(buf, "PS;") == 0)
        {
            sprintf(resp, "PS%d;", power);
            n = write(fd, resp, strlen(resp));

            if (n <= 0) { perror("PS"); }
        }
        else if (strncmp(buf, "PS", 2) == 0)
        {
            sscanf(buf, "PS%d", &power);
        }
        else if (strcmp(buf, "RM5;") == 0)
        {
            printf("%s\n", buf);
            usleep(50 * 1000);
            pbuf = "RM5100000;";
            n = write(fd, pbuf, strlen(pbuf));
            //printf("n=%d\n", n);

            if (n <= 0) { perror("RM5"); }
        }
        else if (strcmp(buf, "RM8;") == 0)
        {
            printf("%s\n", buf);
            usleep(50 * 1000);
            pbuf = "RM8197000;";
            n = write(fd, pbuf, strlen(pbuf));
            //printf("n=%d\n", n);

            if (n <= 0) { perror("RM8"); }
        }
        else if (strcmp(buf, "RM9;") == 0)
        {
            printf("%s\n", buf);
            usleep(50 * 1000);
            pbuf = "RM9089000;";
            n = write(fd, pbuf, strlen(pbuf));
            //printf("n=%d\n", n);

            if (n <= 0) { perror("RM9"); }
        }

        else if (strcmp(buf, "AN0;") == 0)
        {
            printf("%s\n", buf);
            usleep(50 * 1000);
            pbuf = "AN030;";
            n = write(fd, pbuf, strlen(pbuf));
            //printf("n=%d\n", n);

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
            usleep(50 * 1000);
            pbuf = "IF00107041000+000000200000;";
            n = write(fd, pbuf, strlen(pbuf));
            //printf("n=%d\n", n);

            if (n <= 0) { perror("IF"); }
        }
        else if (strcmp(buf, "ID;") == 0)
        {
            printf("%s\n", buf);
            usleep(50 * 1000);
            int id = NC_RIGID_FT991;
            SNPRINTF(buf, sizeof(buf), "ID%03d;", id);
            n = write(fd, buf, strlen(buf));
            //printf("n=%d\n", n);

            if (n <= 0) { perror("ID"); }
        }
        else if (strcmp(buf, "AI;") == 0)
        {
            printf("%s\n", buf);
            usleep(50 * 1000);
            SNPRINTF(buf, sizeof(buf), "AI0;");
            n = write(fd, buf, strlen(buf));
            //printf("n=%d\n", n);

            if (n <= 0) { perror("ID"); }
        }
        else if (strcmp(buf, "AI0;") == 0)
        {
            usleep(50 * 1000);
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
                usleep(50 * 1000);
                n = fprintf(fp, "%s", "AI0;");
                printf("n=%d\n", n);

                if (n <= 0) { perror("AI"); }
            }
        }

#endif
        else if (strcmp(buf, "VS") == 0 && strlen(buf) > 3)
        {
            curr_vfo = buf[3] == '1' ? RIG_VFO_B : RIG_VFO_A;
            usleep(50 * 1000);
        }
        else if (strcmp(buf, "VS;") == 0)
        {
            printf("%s\n", buf);
            usleep(50 * 1000);
            pbuf = "VS0;";

            if (curr_vfo == RIG_VFO_B || curr_vfo == RIG_VFO_SUB) { pbuf[2] = '1'; }

            n = write(fd, pbuf, strlen(pbuf));
            printf("%s\n", pbuf);

            if (n < 0) { perror("VS"); }
        }
        else if (strcmp(buf, "FT;") == 0)
        {
            usleep(50 * 1000);
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
            usleep(50 * 1000);
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
            usleep(50 * 1000);
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
            usleep(50 * 1000);
            SNPRINTF(resp, sizeof(resp), "SM0111;");
            n = write(fd, resp, strlen(resp));

            if (n < 0) { perror("SM"); }
        }
        else if (strcmp(buf, "TX;") == 0)
        {
            usleep(50 * 1000);
            SNPRINTF(resp, sizeof(resp), "TX%d;", ptt);
            n = write(fd, resp, strlen(resp));

            if (n < 0) { perror("TX"); }
        }
        else if (strncmp(buf, "TX", 2) == 0)
        {
            usleep(50 * 1000);
            ptt = buf[2] == '0' ? 0 : 1;
        }
        else if (strcmp(buf, "EX032;") == 0)
        {
            static int ant = 0;
            ant = (ant + 1) % 3;
            printf("%s\n", buf);
            usleep(50 * 1000);
            SNPRINTF(buf, sizeof(buf), "EX032%1d;", ant);
            n = write(fd, buf, strlen(buf));
            //printf("n=%d\n", n);

            if (n < 0) { perror("EX032"); }
        }
        else if (strcmp(buf, "NA0;") == 0)
        {
            SNPRINTF(buf, sizeof(buf), "NA00;");
            usleep(50 * 1000);
            n = write(fd, buf, strlen(buf));
            //printf("%s n=%d\n", buf, n);
        }
        else if (strcmp(buf, "RF0;") == 0)
        {
            SNPRINTF(buf, sizeof(buf), "RF0%d;", roofing_filter_main);
            usleep(50 * 1000);
            n = write(fd, buf, strlen(buf));
        }
        else if (strcmp(buf, "RF1;") == 0)
        {
            SNPRINTF(buf, sizeof(buf), "RF1%d;", roofing_filter_sub);
            usleep(50 * 1000);
            n = write(fd, buf, strlen(buf));
        }
        else if (strncmp(buf, "RF", 2) == 0)
        {
            SNPRINTF(buf, sizeof(buf), "RF%c%d;", buf[2],
                     buf[2] == 0 ? roofing_filter_main : roofing_filter_sub);
            usleep(50 * 1000);
            n = write(fd, buf, strlen(buf));
        }
        else if (strcmp(buf, "SH0;") == 0)
        {
            SNPRINTF(buf, sizeof(buf), "SH%c%02d;", buf[2], width_main);
            usleep(50 * 1000);
            n = write(fd, buf, strlen(buf));
        }
        else if (strcmp(buf, "SH1;") == 0)
        {
            SNPRINTF(buf, sizeof(buf), "SH%c%02d;", buf[2], width_sub);
            usleep(50 * 1000);
            n = write(fd, buf, strlen(buf));
        }
        else if (strncmp(buf, "SH", 2) == 0 && strlen(buf) > 4)
        {
            int vfo, twidth;
            sscanf(buf, "SH%1d%d", &vfo, &twidth);

            if (vfo == 0) { width_main = twidth; }
            else { width_sub = twidth; }

            printf("width_main=%d, width_sub=%d\n", width_main, width_sub);
        }
        else if (strncmp(buf, "SH", 2) == 0)
        {
            SNPRINTF(buf, sizeof(buf), "SH%c%02d;", buf[2],
                     buf[2] == 0 ? width_main : width_sub);
        }
        else if (strcmp(buf, "EX039;") == 0)
        {
            SNPRINTF(buf, sizeof(buf), "EX039%d;", ex039);
            n = write(fd, buf, strlen(buf));
        }
        else if (strncmp(buf, "EX039", 3) == 0)
        {
            sscanf(buf, "EX039%d", &ex039);
        }
        else if (strcmp(buf, "LK;") == 0)
        {
            SNPRINTF(buf, sizeof(buf), "LK%d;", lk);
            n = write(fd, buf, strlen(buf));
        }
        else if (strncmp(buf, "LK", 3) == 0)
        {
            sscanf(buf, "LK%d", &lk);
        }
        else if (strncmp(buf, "DT0;", 4) == 0)
        {
            SNPRINTF(buf, sizeof(buf), "DT020221022;");
            n = write(fd, buf, strlen(buf));
        }
        else if (strncmp(buf, "DT1;", 4) == 0)
        {
            SNPRINTF(buf, sizeof(buf), "DT1222100;");
            n = write(fd, buf, strlen(buf));
        }
        else if (strncmp(buf, "DT2;", 4) == 0)
        {
            SNPRINTF(buf, sizeof(buf), "DT2+0500;");
            n = write(fd, buf, strlen(buf));
        }


        else if (strlen(buf) > 0)
        {
            fprintf(stderr, "Unknown command: '%s'\n", buf);
        }

    }

    return 0;
}
