// can run this using rigctl/rigctld and socat pty devices
// emulates 1.2 ROM FT990 which can only read 1492 bytes
#define _XOPEN_SOURCE 700
// since we are POSIX here we need this
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#define BUFSIZE 256

float freqA = 14074000;
float freqB = 14074500;
char tx_vfo = '0';
char rx_vfo = '0';
char modeA = '1';
char modeB = '1';
int width_main = 500;
int width_sub = 700;


static void load_dat(const char *filename, unsigned char buf[1492])
{
    FILE *fp = fopen(filename, "r");
    char line[4096];
    int n = 0;

    while (fgets(line, sizeof(line), fp))
    {
        char *s = strdup(line);
        unsigned int val;
        char *p = strtok(line, " \r\n");

        do
        {
            sscanf(p, "%x", &val);
            buf[n++] = val;
        }
        while (p = strtok(NULL, " \r\n"));

        strtok(s, "\r\n");
        //printf("n=%d, %s\n",n,s);
        free(s);
    }

    fclose(fp);
    printf("%d bytes read\n", n);
}

static unsigned char alldata[1492];

#include "sim.h"


int main(int argc, char *argv[])
{
    char buf[BUFSIZE];
    int fd = openPort(argv[1]);

    load_dat("simft990.dat", alldata);

    while (1)
    {
        int bytes = getmyline5(fd, buf);

        if (bytes == 0) { continue; }

        if (bytes != 5)
        {
            printf("Not 5 bytes?  bytes=%d\n", bytes);
        }

        if (buf[4] == 0x10)
        {
            write(fd, alldata, 1492);
        }
    }

    return 0;
}
