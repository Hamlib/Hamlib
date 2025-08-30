// can run this using rigctl/rigctld and socat pty devices
#define _XOPEN_SOURCE 700
// since we are POSIX here we need this
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <pthread.h>

#include "misc.h"

#define BUFSIZE 256

static void *rotorez_thread(void *arg);

int
_getmyline(int fd, char *buf)
{
    unsigned char c = 0;
    int i = 0;
    int n = 0;
    memset(buf, 0, BUFSIZE);

    //printf("fd=%d\n", fd);

    while (read(fd, &c, 1) > 0 && c != 0xa)
    {
        buf[i++] = c;
        n++;

        for (int j = 0; j < strlen(buf); ++j) { printf("%02x ", buf[j]); }

        printf("\n");
    }

    if (strlen(buf) == 0) { hl_usleep(10 * 1000); }

    return n;
}

#include "sim.h"

int thread_args[2];


int main(int argc, char *argv[])
{
    int fd = openPort(argv[1]);
    //int fd2 = openPort(argv[2]);
    pthread_t threads[2];
    thread_args[0] = fd;
    //thread_args[1] = fd2;
    pthread_create(&threads[0], NULL, rotorez_thread, (void *)&thread_args[0]);
    //pthread_create(&threads[1], NULL, rotorez_thread, (void *)&thread_args[1]);
    pthread_exit(NULL);
    return 0;
    /*
    again:
    int flag = 0;

    while (1)
    {
        int bytes;
        if (!flag) bytes = _getmyline(fd, buf);
        else bytes = _getmyline(fd2, buf);
        flag = !flag;

        if (bytes == 0)
        {
            //close(fd);
            printf("again\n");
            goto again;
        }
        printf("line=%s\n", buf);

        if (strncmp(buf,"BI1",3) == 0)
        {
            sprintf(buf,"%3.1f;", az);
            n = write(flag?fd:fd2, buf, strlen(buf));
            printf("n=%d\n", n);
        }
        else if (strncmp(buf,"AP1",3) == 0)
        {
            sscanf(buf,"AP1%f", &az);
        }
        else
        {
            printf("Unknown cmd=%s\n", buf);
        }

    #if 0
        switch (buf[0])
        {
        case '?': printf("Query %c\n", buf[1]); break;

        case '*': printf("Set %c\n", buf[1]); break;

        default: printf("Unknown cmd=%02x\n", buf[4]);
        }
    #endif
    }

    return 0;
    */
}

static void *rotorez_thread(void *arg)
{
    int n = 0;
    char buf[BUFSIZE];
    int fd = *(int *)arg;
    float az = 123;
    float el = 45;

    while (1)
    {
        int bytes;
        bytes = _getmyline(fd, buf);

        if (bytes == 0)
        {
            hl_usleep(100 * 1000);
            continue;
        }

        printf("line[%d]=%s\n", fd, buf);

        if (sscanf(buf, "AZ%g EL%g", &az, &el) == 2)
        {
            //n = write(fd, buf, strlen(buf));
            printf("n=%d fd=%d\n", n, fd);
        }
        else if (strcmp(buf,"AZ")==0)
        {
            sprintf(buf,"AZ%.1f\n", az);
            n = write(fd, buf, strlen(buf));
        }
        else if (strcmp(buf,"EL")==0)
        {
            sprintf(buf,"EL%.1f\n", el);
            n = write(fd, buf, strlen(buf));
        }
        else
        {
            printf("Unknown cmd=%s\n", buf);
        }

#if 0

        switch (buf[0])
        {
        case '?': printf("Query %c\n", buf[1]); break;

        case '*': printf("Set %c\n", buf[1]); break;

        default: printf("Unknown cmd=%02x\n", buf[4]);
        }

#endif
    }

    pthread_exit(NULL);
}
