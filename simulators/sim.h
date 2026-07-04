#include <errno.h>
#include <fcntl.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>

/* Define macros for handling attributes, if the compiler implements them
 *   Should be available in c23-capable compilers, or c++11 ones
 */
// From ISO/IEC 9899:202y n3301 working draft
#ifndef __has_c_attribute
#define __has_c_attribute(x) 0
#endif

// Macro for allowing unused variables/functions to go unmentioned
// This is really a C23 feature, so older compilers may still produce
//   a warning - please ignore it.
#if __has_c_attribute(maybe_unused)
#define MAY_BE_UNUSED [[maybe_unused]]
#else
#define MAY_BE_UNUSED
#endif

// Size of command buffer
#define BUFSIZE 256

/* ID 0310 == 310, Must drop leading zero */
typedef enum nc_rigid_e
{
    NC_RIGID_NONE            = 0,
    NC_RIGID_FT450           = 241,
    NC_RIGID_FT450D          = 244,
    NC_RIGID_FT950           = 310,
    NC_RIGID_FT891           = 135,
    NC_RIGID_FT991           = 570,
    NC_RIGID_FT991A          = 670,
    NC_RIGID_FT2000          = 251,
    NC_RIGID_FT2000D         = 252,
    NC_RIGID_FTDX1200        = 583,
    NC_RIGID_FTDX10          = 761,
    NC_RIGID_FTDX9000D       = 101,
    NC_RIGID_FTDX9000Contest = 102,
    NC_RIGID_FTDX9000MP      = 103,
    NC_RIGID_FTDX5000        = 362,
    NC_RIGID_FTDX3000        = 462,
    NC_RIGID_FTDX101D        = 681,
    NC_RIGID_FTDX101MP       = 682,
    NC_RIGID_FT710           = 800,
    NC_RIGID_FTX1            = 840,
} nc_rigid_t;

void dumphex(const unsigned char *buf, int n)
{
    for (int i = 0; i < n; ++i) { printf("%02x ", buf[i]); }

    printf("\n");
}

#define WRITE(f,b,l) write_sim(f,(const unsigned char*)b,l,__func__,__LINE__)

int write_sim(int fd, const unsigned char *buf, int buflen, const char *func,
              int linenum)
{
    int n;
    dumphex(buf, buflen);
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

int
getmyline(int fd, char *buf)
{
    char c;
    int i = 0;
    memset(buf, 0, BUFSIZE);

    while (read(fd, &c, 1) > 0)
    {
        buf[i++] = c;

        if (c == ';') { return i; }
    }

    if (i == 0) { sleep(1); }

    return i;
}

int
getmyline5(int fd, unsigned char *buf)
{
    unsigned char c;
    int i = 0;
    memset(buf, 0, BUFSIZE);

    while (i < 5 && read(fd, &c, 1) > 0)
    {
        buf[i++] = c;
    }

#if 0
    if (i > 0) {
        printf("n=%d %02x %02x %02x %02x %02x\n", i,
               buf[0], buf[1], buf[2], buf[3], buf[4]);
    }
#endif

    if (i == 0) {
        //hl_usleep(10 * 1000);
        sleep(1);
    }

    return i;
}

/* Vendor specific data and routines
 *
 * Some may be re-implementation of Hamlib, but let's keep them disjoint
 */
#if SIM == Icom

#include "../rigs/icom/icom_defs.h"

MAY_BE_UNUSED
static const char *mode_names[] = {
  "LSB", "USB", "AM", "CW", "RTTY", "FM", "WFM", "CW-R", "RTTY-R" };

MAY_BE_UNUSED
static int acknak(int fd, unsigned char *frame, unsigned char status)
{
  frame[4] = status;
  frame[5] = FI;
  return write(fd, frame, ACKFRMLEN);
}

#elif SIM == Kenwood
#elif SIM == Yaesu
#endif
