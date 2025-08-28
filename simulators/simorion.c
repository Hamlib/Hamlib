// can run this using rigctl/rigctld and socat pty devices
#define _XOPEN_SOURCE 700
// since we are POSIX here we need this
#include <stdio.h>
#include <string.h>
#include <unistd.h>

#define BUFSIZE 256

int freqA = 14074000;
int freqB = 14074500;
int modeA = 0;
int width = 3010;
int ptt = 0;
int keyspd = 20;

int
_getmyline(int fd, char *buf)
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

#include "sim.h"


int main(int argc, char *argv[])
{
    char buf[BUFSIZE], reply[256];

    int fd = openPort(argv[1]);

    while (1)
    {
        int bytes = _getmyline(fd, buf);

        if (bytes == 0)
        {
            continue;
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
