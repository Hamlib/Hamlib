// can run this using rigctl/rigctld and socat pty devices
// gcc -o simorion simorion.c
#define _XOPEN_SOURCE 700
// since we are POSIX here we need this
#if 0
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

int freqA = 14074000;
int freqB = 14074500;
int modeA = 0;
int width = 3010;
int ptt = 0;
int keyspd = 20;

int
getmyline(int fd, char *buf)
{
    unsigned char c;
    int i = 0;
    int n = 0;
    memset(buf, 0, BUFSIZE);

    while (read(fd, &c, 1) > 0)
    {
        if (c == 0x0d) { break; }

        buf[i++] = c;
        n++;
    }

    return n;
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
    char buf[256], reply[256];

again:
    int fd = openPort(argv[1]);

    while (1)
    {
        int bytes = getmyline(fd, buf);

        if (bytes == 0)
        {
            close(fd);
            goto again;
        }

        if (strncmp(buf, "?V", 2) == 0)
        {
            char *s = "Version 3.032x7";
            write(fd, s, strlen(s));
        }
        else if (strncmp(buf, "?AF", 3) == 0)
        {
            sprintf(reply, "@AF%8d\r", freqA);
            write(fd, reply, strlen(reply));
        }
        else if (strncmp(buf, "?BF", 3) == 0)
        {
            sprintf(reply, "@BF%8d\r", freqB);
            write(fd, reply, strlen(reply));
        }
        else if (strncmp(buf, "*AF", 3) == 0)
        {
            sscanf(buf, "*AF%d", &freqA);
        }
        else if (strncmp(buf, "*BF", 3) == 0)
        {
            sscanf(buf, "*BF%d", &freqB);
        }
        else if (strncmp(buf, "?KV", 3) == 0)
        {
            sprintf(reply, "@KVANA\r");
            write(fd, reply, strlen(reply));
        }
        else if (strncmp(buf, "?RMM", 4) == 0)
        {
            sprintf(reply, "@RMM%d\r", modeA);
            write(fd, reply, strlen(reply));
        }
        else if (strncmp(buf, "?RMF", 4) == 0)
        {
            sprintf(reply, "@RMF%d\r", width);
            write(fd, reply, strlen(reply));
        }
        else if (strncmp(buf, "?S", 2) == 0)
        {
            if (ptt)
            {
                sprintf(reply, "@STWF002R000S256\r");
            }
            else
            {
                sprintf(reply, "@SRM055S000\r");
            }

            write(fd, reply, strlen(reply));
        }
        else if (strncmp(buf, "*TK", 3) == 0)
        {
            ptt = 1;
        }
        else if (strncmp(buf, "*TU", 3) == 0)
        {
            ptt = 0;
        }
        else if (strncmp(buf, "?CS", 3) == 0)
        {
            sprintf(reply, "@CS%d\r", keyspd);
        }
        else if (strncmp(buf, "*CS", 3) == 0)
        {
            sscanf(buf, "*CS%d\r", &keyspd);
        }
        else { printf("Unknown cmd=%s\n", buf); }
    }

    //flush(fd);

    return 0;
}
