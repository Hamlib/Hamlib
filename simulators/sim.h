#include <errno.h>
#include <fcntl.h>

#include "../src/misc.h"

#define WRITE(f,b,l) write_sim(f,(const unsigned char*)b,l,__func__,__LINE__)

int write_sim(int fd, const unsigned char *buf, int buflen, const char *func,
              int linenum)
{
    int n;
    dump_hex(buf, buflen);
    n = write(fd, buf, buflen);

    if (n <= 0)
    {
        fprintf(stderr, "%s(%d) buf='%s' write error %d: %s\n", func, linenum, buf, n,
                strerror(errno));
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
