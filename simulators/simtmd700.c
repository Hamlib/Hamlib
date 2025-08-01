// can run this using rigctl/rigctld and socat pty devices
#define _XOPEN_SOURCE 700
// since we are POSIX here we need this
#if  0
struct ip_mreq
{
    int dummy;
};
#endif

#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>
#include <string.h>
#include <unistd.h>
#include "../include/hamlib/rig.h"

#define BUFSIZE 256
double freqA = 147000000;
double freqB = 148000000;
char tx_vfo = '0';
char rx_vfo = '0';
char modeA = '0';
char modeB = '0';
int band = 0;
int control = 1;

int
getmyline(int fd, char *buf)
{
    char c;
    int i = 0;
    memset(buf, 0, BUFSIZE);

    while (read(fd, &c, 1) > 0)
    {
        if (c == 0x0d) { return strlen(buf); }

        buf[i++] = c;
    }

    if (strlen(buf) == 0) { hl_usleep(10 * 1000); }

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
        perror("ptsname");
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
    int n;
    int fd = openPort(argv[1]);

    while (1)
    {
        if (getmyline(fd, buf))
        {
            printf("Cmd:%s\n", buf);
        }
        else { continue; }

        if (strcmp(buf, "ID") == 0)
        {
            printf("%s\n", buf);
            hl_usleep(50 * 1000);
            SNPRINTF(buf, sizeof(buf), "ID TM-D700\r");
            n = write(fd, buf, strlen(buf));
            printf("n=%d\n", n);

            if (n <= 0) { perror("ID"); }
        }
        else if (strcmp(buf, "BC") == 0)
        {
            SNPRINTF(buf, sizeof(buf), "BC %d,%d\r", band, control);
            n = write(fd, buf, strlen(buf));
        }
        else if (strncmp(buf, "BC ", 3) == 0)
        {
            sscanf(buf, "BC %d,%d", &band, &control);
            SNPRINTF(buf, sizeof(buf), "BC %d,%d\r", band, control);
            n = write(fd, buf, strlen(buf));
        }
        else if (strncmp(buf, "VMC ", 4) == 0)
        {
            SNPRINTF(buf, sizeof(buf), "VMC 0,0\r");
            n = write(fd, buf, strlen(buf));
        }
        else if (strncmp(buf, "AI", 2) == 0)
        {
            SNPRINTF(buf, sizeof(buf), "AI 0\r");
            n = write(fd, buf, strlen(buf));
        }
        else if (strncmp(buf, "FQ", 2) == 0)
        {
            SNPRINTF(buf, sizeof(buf), "FQ %011.0f,0\r", freqA);
            n = write(fd, buf, strlen(buf));
        }
        else if (strncmp(buf, "FQ ", 3) == 0)
        {
            sscanf(buf, "FQ %lf,0", &freqA);
            SNPRINTF(buf, sizeof(buf), "FQ %011.0f,0\r", freqA);
            n = write(fd, buf, strlen(buf));
        }
        else if (strncmp(buf, "MD", 2) == 0)
        {
            SNPRINTF(buf, sizeof(buf), "MD 0\r");
            n = write(fd, buf, strlen(buf));
        }
        else if (strlen(buf) > 0)
        {
            fprintf(stderr, "Unknown command: %s\n", buf);
        }

    }

    return 0;
}
