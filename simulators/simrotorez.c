// can run this using rigctl/rigctld and socat pty devices
// gcc -o simspid simspid.c
#define _XOPEN_SOURCE 700
// since we are POSIX here we need this
struct ip_mreq
{
    int dummy;
};

#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>
#include <string.h>
#include <unistd.h>
#include "../include/hamlib/rig.h"

#define BUFSIZE 256

static void *rotorez_thread(void *arg);

int
getmyline(int fd, char *buf)
{
    unsigned char c = 0;
    int i = 0;
    int n = 0;
    memset(buf, 0, BUFSIZE);

    //printf("fd=%d\n", fd);

    while (read(fd, &c, 1) > 0 && c != ';')
    {
        buf[i++] = c;
        n++;

        for (int i = 0; i < strlen(buf); ++i) { printf("%02x ", buf[i]); }

        printf("\n");
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

int thread_args[2];


int main(int argc, char *argv[])
{
    int fd = openPort(argv[1]);
    int fd2 = openPort(argv[2]);
    pthread_t threads[2];
    thread_args[0] = fd;
    thread_args[1] = fd2;
    pthread_create(&threads[0], NULL, rotorez_thread, (void *)&thread_args[0]);
    pthread_create(&threads[1], NULL, rotorez_thread, (void *)&thread_args[1]);
    pthread_exit(NULL);
    return 0;
    /*
    again:
    int flag = 0;

    while (1)
    {
        int bytes;
        if (!flag) bytes = getmyline(fd, buf);
        else bytes = getmyline(fd2, buf);
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
    char buf[256];
    int fd = *(int *)arg;
    float az = 123;
    float el = 45;
again:

    while (1)
    {
        int bytes;
        bytes = getmyline(fd, buf);

        if (bytes == 0)
        {
            //close(fd);
            hl_usleep(100 * 1000);
            //printf("again\n");
            goto again;
        }

        printf("line[%d]=%s\n", fd, buf);

        if (strncmp(buf, "BI1", 3) == 0)
        {
            if (fd == thread_args[0])
            {
                sprintf(buf, "%3.1f;", az);
                printf("az=%f\n", az);
            }
            else
            {
                sprintf(buf, "%3.1f;", el);
                printf("el=%f\n", el);
            }

            n = write(fd, buf, strlen(buf));
            printf("n=%d fd=%d\n", n, fd);
        }
        else if (strncmp(buf, "AP1", 3) == 0)
        {
            if (fd == thread_args[0])
            {
                sscanf(buf, "AP1%f", &az);
            }
            else
            {
                sscanf(buf, "AP1%f", &el);
            }
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
