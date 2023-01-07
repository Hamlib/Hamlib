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
    int modea, modeb = 0;
    int freqa = 14074000, freqb = 14073500;

    while (1)
    {
        buf[0] = 0;

        if (getmyline(fd, buf) > 0) { printf("Cmd:%s\n", buf); }
        else { return 0; }

        if (strcmp(buf, "RM5;") == 0)
        {
            printf("%s\n", buf);
            usleep(50 * 1000);
            pbuf = "RM5100000;";
            n = write(fd, pbuf, strlen(pbuf));
            printf("n=%d\n", n);

            if (n <= 0) { perror("RM5"); }
        }

        else if (strcmp(buf, "AN0;") == 0)
        {
            printf("%s\n", buf);
            usleep(50 * 1000);
            pbuf = "AN030;";
            n = write(fd, pbuf, strlen(pbuf));
            printf("n=%d\n", n);

            if (n <= 0) { perror("AN"); }
        }
        else if (strcmp(buf, "IF;") == 0)
        {
            printf("%s\n", buf);
            usleep(50 * 1000);
            //pbuf = "IF059014200000+000000700000;";
            pbuf = "IF00007230000     -000000 0001000001 ;" ;
            n = write(fd, pbuf, strlen(pbuf));
            printf("n=%d\n", n);

            if (n <= 0) { perror("IF"); }
        }
        else if (strcmp(buf, "ID;") == 0)
        {
            printf("%s\n", buf);
            usleep(50 * 1000);
            int id = 24;
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
            printf("n=%d\n", n);

            if (n < 0) { perror("VS"); }
        }
        else if (strcmp(buf, "EX032;") == 0)
        {
            static int ant = 0;
            ant = (ant + 1) % 3;
            printf("%s\n", buf);
            usleep(50 * 1000);
            SNPRINTF(buf, sizeof(buf), "EX032%1d;", ant);
            n = write(fd, buf, strlen(buf));
            printf("n=%d\n", n);

            if (n < 0) { perror("EX032"); }
        }
        else if (strcmp(buf, "OM;") == 0)
        {
            // KPA3 SNPRINTF(buf, sizeof(buf), "OM AP----L-----;");
            // K4+KPA3
            SNPRINTF(buf, sizeof(buf), "OM AP-S----4---;");
            n = write(fd, buf, strlen(buf));
            printf("n=%d\n", n);

            if (n < 0) { perror("OM"); }
        }
        else if (strcmp(buf, "K2;") == 0)
        {
            write(fd, "K20;", 4);
        }
        else if (strcmp(buf, "K3;") == 0)
        {
            write(fd, "K30;", 4);
        }
        else if (strcmp(buf, "RVM;") == 0)
        {
            write(fd, "RV02.37;", 8);
        }
        else if (strcmp(buf, "AI;") == 0)
        {
            write(fd, "AI0;", 4);
        }
        else if (strcmp(buf, "MD;") == 0)
        {
            SNPRINTF(buf, sizeof(buf), "MD%d;", modea);
            write(fd, buf, strlen(buf));
        }
        else if (strcmp(buf, "MD$;") == 0)
        {
            SNPRINTF(buf, sizeof(buf), "MD$%d;", modeb);
            write(fd, buf, strlen(buf));
        }
        else if (strncmp(buf, "MD", 2) == 0)
        {
            if (buf[2] == '$') { sscanf(buf, "MD$%d;", &modeb); }
            else { sscanf(buf, "MD%d;", &modea); }
        }
        else if (strcmp(buf, "FA;") == 0)
        {
            SNPRINTF(buf, sizeof(buf), "FA%011d;", freqa);
            write(fd, buf, strlen(buf));
        }
        else if (strcmp(buf, "FB;") == 0)
        {
            SNPRINTF(buf, sizeof(buf), "FB%011d;", freqb);
            write(fd, buf, strlen(buf));
        }

        else if (strncmp(buf, "FA", 2) == 0)
        {
            sscanf(buf, "FA%d", &freqa);
        }
        else if (strncmp(buf, "FB", 2) == 0)
        {
            sscanf(buf, "FB%d", &freqb);
        }
        else if (strncmp(buf, "FR;", 3) == 0)
        {
            SNPRINTF(buf, sizeof(buf), "FR0;");
            write(fd, buf, strlen(buf));
        }
        else if (strncmp(buf, "FT;", 3) == 0)
        {
            SNPRINTF(buf, sizeof(buf), "FT0;");
            write(fd, buf, strlen(buf));
        }
        else if (strncmp(buf, "TQ;", 3) == 0)
        {
            SNPRINTF(buf, sizeof(buf), "TQ0;");
            write(fd, buf, strlen(buf));
        }
        else if (strncmp(buf, "PC;", 3) == 0)
        {
            SNPRINTF(buf, sizeof(buf), "PC0150;");
            n = write(fd, buf, strlen(buf));
        }
        else if (strlen(buf) > 0)
        {
            fprintf(stderr, "Unknown command: %s\n", buf);
        }

    }

    return 0;
}
