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
int filternum = 7;
int datamode = 0;
int vfo, vfo_tx, ptt, ptt_data, ptt_mic, ptt_tune;

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
    char *pbuf;
    int n;
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
            usleep(50 * 1000);
            pbuf = "RM5100000;";
            n = write(fd, pbuf, strlen(pbuf));
//            printf("n=%d\n", n);

            if (n <= 0) { perror("RM5"); }
        }

        else if (strcmp(buf, "AN0;") == 0)
        {
            printf("%s\n", buf);
            usleep(50 * 1000);
            pbuf = "AN030;";
            n = write(fd, pbuf, strlen(pbuf));
//            printf("n=%d\n", n);

            if (n <= 0) { perror("AN"); }
        }
        else if (strcmp(buf, "IF;") == 0)
        {
            char ifbuf[256];
            printf("%s\n", buf);
            usleep(50 * 1000);
            pbuf = "IF000503130001000+0000000000030000000;";
            sprintf(ifbuf, "IF%011d0001000+0000000000030000000;", freqa);
            //pbuf = "IF00010138698     +00000000002000000 ;
            n = write(fd, ifbuf, strlen(ifbuf));
//            printf("n=%d\n", n);

            if (n <= 0) { perror("IF"); }

            continue;
        }
        else if (strcmp(buf, "FV;") == 0)
        {
            usleep(50 * 1000);
            pbuf = "FV1.2;";
            n = write(fd, pbuf, strlen(pbuf));
            continue;
        }
        else if (strcmp(buf, "FW;") == 0)
        {
            usleep(50 * 1000);
            pbuf = "FW2400;";
            n = write(fd, pbuf, strlen(pbuf));
            continue;
        }
        else if (strcmp(buf, "ID;") == 0)
        {
            printf("%s\n", buf);
            usleep(50 * 1000);
            int id = 24;
            SNPRINTF(buf, sizeof(buf), "ID%03d;", id);
            n = write(fd, buf, strlen(buf));
//            printf("n=%d\n", n);

            if (n <= 0) { perror("ID"); }

            continue;
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
        else if (strcmp(buf, "VS;") == 0)
        {
            printf("%s\n", buf);
            usleep(50 * 1000);
            pbuf = "VS0;";
            n = write(fd, pbuf, strlen(pbuf));
//            printf("n=%d\n", n);

            if (n < 0) { perror("VS"); }

            continue;
        }
        else if (strcmp(buf, "EX032;") == 0)
        {
            static int ant = 0;
            ant = (ant + 1) % 3;
            printf("%s\n", buf);
            usleep(50 * 1000);
            SNPRINTF(buf, sizeof(buf), "EX032%1d;", ant);
            n = write(fd, buf, strlen(buf));
//            printf("n=%d\n", n);

            if (n < 0) { perror("EX032"); }

            continue;
        }
        else if (strcmp(buf, "FA;") == 0)
        {
            SNPRINTF(buf, sizeof(buf), "FA%011d;", freqa);
            n = write(fd, buf, strlen(buf));
            continue;
        }
        else if (strcmp(buf, "FB;") == 0)
        {
            SNPRINTF(buf, sizeof(buf), "FB%011d;", freqb);
            n = write(fd, buf, strlen(buf));
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
            n = write(fd, buf, strlen(buf));
            continue;
        }

        if (strncmp(buf, "PS;", 3) == 0)
        {
            SNPRINTF(buf, sizeof(buf), "PS1;");
            n = write(fd, buf, strlen(buf));
            continue;
        }
        else if (strncmp(buf, "SA;", 3) == 0)
        {
            SNPRINTF(buf, sizeof(buf), "SA0;");
            n = write(fd, buf, strlen(buf));
        }
        else if (strncmp(buf, "ZZMD;", 5) == 0)
        {
            SNPRINTF(buf, sizeof(buf), "ZZMD%d;",
                     modeA); // not worried about modeB yet for simulator
            n = write(fd, buf, strlen(buf));
            continue;
        }
        else if (strncmp(buf, "ZZMD", 4) == 0)
        {
            sscanf(buf, "ZZMD%d", &modeA); // not worried about modeB yet for simulator
            continue;
        }
        else if (strncmp(buf, "FL;", 3) == 0)
        {
            SNPRINTF(buf, sizeof(buf), "FL%03d;", filternum);
            n = write(fd, buf, strlen(buf));
            continue;
        }
        else if (strncmp(buf, "ZZFL;", 5) == 0)
        {
            SNPRINTF(buf, sizeof(buf), "ZZFL+%04d;", 100);
            n = write(fd, buf, strlen(buf));
            continue;
        }
        else if (strncmp(buf, "ZZFH;", 5) == 0)
        {
            SNPRINTF(buf, sizeof(buf), "ZZFH+%04d;", 3500);
            n = write(fd, buf, strlen(buf));
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
            n = write(fd, buf, strlen(buf));
            continue;
        }
        else if (strncmp(buf, "FR", 2) == 0)
        {
            sscanf(buf, "FR%d", &vfo);
        }
        else if (strcmp(buf, "FT;") == 0)
        {
            SNPRINTF(buf, sizeof(buf), "FR%d;", vfo_tx);
            n = write(fd, buf, strlen(buf));
            continue;
        }
        else if (strncmp(buf, "FT", 2) == 0)
        {
            sscanf(buf, "FT%d", &vfo_tx);
        }
        else if (strncmp(buf, "DA;", 3) == 0)
        {
            SNPRINTF(buf, sizeof(buf), "DA%d;", datamode);
            n = write(fd, buf, strlen(buf));
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
        else if (strncmp(buf, "ZZMN", 4) == 0)
        {
            SNPRINTF(buf, sizeof(buf),
                     "ZZMN01 5.0k 5100  100 4.4k 4500  100 3.8k 3900  100 3.3k 3400  100 2.9k 3000  100 2.7k 2800  100 2.4k 2500  100 2.1k 2200  100 1.8k 1900  100 1.0k 1100  100Var 1 3150  100Var 2 2800  100;");
            n = write(fd, buf, strlen(buf));
            printf("%s\n", buf);
            continue;
        }
        else if (strncmp(buf, "ZZSQ000;", 5) == 0)
        {
            continue;
        }
        else if (strncmp(buf, "ZZAR;", 5) == 0)
        {
            SNPRINTF(buf, sizeof(buf), "ZZAR+068;");
            n = write(fd, buf, strlen(buf));
            printf("%s\n", buf);
            continue;
        }
        else if (strncmp(buf, "NT;", 3) == 0)
        {
            SNPRINTF(buf, sizeof(buf), "NT0;");
            n = write(fd, buf, strlen(buf));
            printf("%s\n", buf);
            continue;
        }
        else if (strncmp(buf, "AG0;", 4) == 0)
        {
            SNPRINTF(buf, sizeof(buf), "AG0000;");
            n = write(fd, buf, strlen(buf));
            printf("%s\n", buf);
            continue;
        }
        else if (strncmp(buf, "PC;", 3) == 0)
        {
            SNPRINTF(buf, sizeof(buf), "PC100;");
            n = write(fd, buf, strlen(buf));
            printf("%s\n", buf);
            continue;
        }
        else if (strncmp(buf, "ZZSQ;", 5) == 0)
        {
            SNPRINTF(buf, sizeof(buf), "ZZSQ000;");
            n = write(fd, buf, strlen(buf));
            printf("%s\n", buf);
            continue;
        }
        else if (strncmp(buf, "ZZMG;", 5) == 0)
        {
            SNPRINTF(buf, sizeof(buf), "ZZMG+00;");
            n = write(fd, buf, strlen(buf));
            printf("%s\n", buf);
            continue;
        }
        else if (strncmp(buf, "ZZNA;", 5) == 0)
        {
            SNPRINTF(buf, sizeof(buf), "ZZNA0;");
            n = write(fd, buf, strlen(buf));
            printf("%s\n", buf);
            continue;
        }
        else if (strncmp(buf, "ZZNR;", 5) == 0)
        {
            SNPRINTF(buf, sizeof(buf), "ZZNR0;");
            n = write(fd, buf, strlen(buf));
            printf("%s\n", buf);
            continue;
        }
        else if (strncmp(buf, "ZZNS;", 5) == 0)
        {
            SNPRINTF(buf, sizeof(buf), "ZZNS0;");
            n = write(fd, buf, strlen(buf));
            printf("%s\n", buf);
            continue;
        }
        else if (strncmp(buf, "ZZSM0;", 6) == 0)
        {
            SNPRINTF(buf, sizeof(buf), "ZZSM0%d;", 140);
            n = write(fd, buf, strlen(buf));
            printf("%s\n", buf);
            continue;
        }
        else if (strncmp(buf, "ZZSP;", 6) == 0)
        {
            SNPRINTF(buf, sizeof(buf), "ZZSP0;");
            n = write(fd, buf, strlen(buf));
            printf("%s\n", buf);
            continue;
        }
        else if (strncmp(buf, "ZZFI;", 5) == 0)
        {
            SNPRINTF(buf, sizeof(buf), "ZZFI%d;", 10);
            n = write(fd, buf, strlen(buf));
            printf("%s\n", buf);
            continue;
        }
        else if (strncmp(buf, "ZZTU;", 5) == 0)
        {
            SNPRINTF(buf, sizeof(buf), "ZZTU%d;", 0);
            n = write(fd, buf, strlen(buf));
            printf("%s\n", buf);
            continue;
        }
        else if (strncmp(buf, "ZZTX;", 5) == 0)
        {
            SNPRINTF(buf, sizeof(buf), "ZZTX%d;", ptt);
            n = write(fd, buf, strlen(buf));
            printf("%s\n", buf);
            continue;
        }
        else if (strncmp(buf, "ZZTX", 4) == 0)
        {
            ptt = ptt_mic = ptt_data = ptt_tune = 0;

            switch (buf[4])
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
