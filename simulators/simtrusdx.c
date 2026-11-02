/* CAT control simulator for the documented (tr)uSDX subset.
 * https://dl2man.de/5-trusdx-details/
 * Audio streaming is not simulated.
 */
#define _XOPEN_SOURCE 700
#include <stdio.h>
#include <string.h>
#include <sys/types.h>
#include "hamlib/rig.h"
#include "misc.h"
#include "sim.h"

int main(int argc, char *argv[])
{
    char buf[BUFSIZE], reply[BUFSIZE];
    int fd;
    long freq = 14075000;
    int mode = 2;

    if (argc != 2) { return 1; }
    fd = openPort(argv[1]);
    if (fd < 0) { return 1; }

    while (1)
    {
        buf[0] = 0;
        if (getmyline(fd, buf) <= 0) { continue; }
        printf("Cmd:%s\n", buf);
        reply[0] = 0;

        if (!strcmp(buf, "ID;")) { strcpy(reply, "ID020;"); }
        else if (!strcmp(buf, "FA;")) { SNPRINTF(reply, sizeof(reply), "FA%011ld;", freq); }
        else if (strlen(buf) == 14 && !strncmp(buf, "FA", 2)
                 && strspn(buf + 2, "0123456789") == 11 && buf[13] == ';')
        {
            freq = atol(buf + 2);
        }
        else if (!strcmp(buf, "MD;")) { SNPRINTF(reply, sizeof(reply), "MD%d;", mode); }
        else if (strlen(buf) == 4 && !strncmp(buf, "MD", 2)
                 && buf[2] >= '0' && buf[2] <= '5' && buf[3] == ';')
        {
            if (buf[2] != '0') { mode = buf[2] - '0'; }
        }
        else if (!strcmp(buf, "IF;"))
        {
            /* Only frequency and mode are meaningful in truSDX IF replies. */
            SNPRINTF(reply, sizeof(reply), "IF%011ld00000+0000000000%d0000000;", freq, mode);
        }
        else if (!strcmp(buf, "PS;")) { strcpy(reply, "PS1;"); }
        else if (!strcmp(buf, "AI;")) { strcpy(reply, "AI0;"); }
        else if (!strcmp(buf, "RX;") || !strcmp(buf, "TX;")
                 || !strcmp(buf, "TX0;") || !strcmp(buf, "TX1;")
                 || !strcmp(buf, "TX2;") || !strcmp(buf, "PS1;")
                 || !strcmp(buf, "AI0;") || !strcmp(buf, "UA0;"))
        {
            /* Control commands with no reply. */
        }
        else
        {
            fprintf(stderr, "Unsupported simulator command: %s\n", buf);
            strcpy(reply, "?;");
        }

        if (reply[0]) { WRITE(fd, reply, strlen(reply)); }
    }
}
