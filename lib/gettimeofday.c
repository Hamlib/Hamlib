
#include <config.h>

#ifdef HAVE_WINDOWS_H
#include <windows.h>
#endif
#ifdef HAVE_WINBASE_H
# include <winbase.h>
#endif

#include <sys/time.h>

#ifndef timezone
struct timezone {
	int  tz_minuteswest;
	int  tz_dsttime;
};
#endif

/* 
 * broken implementation for WIN32. 
 * FIXME: usec precision
 */
int gettimeofday(struct timeval *tv, struct timezone *tz)
{
	if (tv) {
		time_t tm;
		
		time(&tm);
		tv->tv_sec = tm;
		tv->tv_usec = 0;
	}
	return 0;
}

