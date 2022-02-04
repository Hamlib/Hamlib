
#include <hamlib/config.h>

#ifdef HAVE_WINDOWS_H
#include <windows.h>
#endif
#ifdef HAVE_WINBASE_H
# include <winbase.h>
#endif

#ifdef HAVE_SYS_TIME_H
#include <sys/time.h>
#endif

/*
 * broken implementation for WIN32.
 * FIXME: usec precision
 */
int gettimeofday(struct timeval *tv, struct timezone *tz)
{
    if (tv)
    {
        time_t tm;

        time(&tm);
        tv->tv_sec = tm;
        tv->tv_usec = 0;
    }

    return 0;
}

