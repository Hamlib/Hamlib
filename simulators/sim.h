#include "../src/misc.h"
#include <errno.h>

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
