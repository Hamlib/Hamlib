# Check for (mingw)win32 POSIX replacements.             -*- Autoconf -*-

# Copyright 2003 Free Software Foundation, Inc.
#
# This file is part of GNU Radio
#
# GNU Radio is free software; you can redistribute it and/or modify
# it under the terms of the GNU General Public License as published by
# the Free Software Foundation; either version 2, or (at your option)
# any later version.
#
# GNU Radio is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
# GNU General Public License for more details.
#
# You should have received a copy of the GNU General Public License
# along with GNU Radio; see the file COPYING.  If not, write to
# the Free Software Foundation, Inc., 59 Temple Place - Suite 330,
# Boston, MA 02111-1307, USA.


AC_DEFUN([GR_PWIN32],
[
AC_SEARCH_LIBS([nanosleep], [pthread])
AC_CHECK_HEADERS([pthread.h])
AC_CHECK_HEADERS([sys/types.h])
AC_CHECK_HEADERS([windows.h])
AC_CHECK_HEADERS([winioctl.h winbase.h], [], [], [
	#if HAVE_WINDOWS_H
	#include <windows.h>
	#endif
])

AC_CHECK_FUNCS([getopt getopt_long usleep sleep nanosleep gettimeofday])
AC_CHECK_TYPES([struct timezone, ssize_t],[],[],[
     #if HAVE_PTHREAD_H
     #include <pthread.h>
     #endif
     #if HAVE_SYS_TYPES_H
     # include <sys/types.h>
     #endif
     #if TIME_WITH_SYS_TIME
     # include <sys/time.h>
     # include <time.h>
     #else
     # if HAVE_SYS_TIME_H
     #  include <sys/time.h>
     # else
     #  include <time.h>
     # endif
     #endif
])

dnl Checks for replacements
AC_REPLACE_FUNCS([getopt getopt_long usleep gettimeofday])

AC_MSG_CHECKING(for Sleep)
AC_LINK_IFELSE([AC_LANG_PROGRAM([
		#include <windows.h>
		#include <winbase.h>
		], [ Sleep(0); ])],
		[AC_DEFINE(HAVE_SSLEEP,1,[Define to 1 if you have Windows Sleep])
		AC_MSG_RESULT(yes)],
		AC_MSG_RESULT(no)
		)

AH_BOTTOM(
[
/* Define missing prototypes, implemented in replacement lib */
#ifdef  __cplusplus
extern "C" {
#endif

#ifndef HAVE_GETOPT
int getopt (int argc, char * const argv[], const char * optstring);
extern char * optarg;
extern int optind, opterr, optopt;
#endif

#ifndef HAVE_GETOPT_LONG
struct option;
int getopt_long (int argc, char * const argv[], const char * optstring,
			const struct option * longopts, int * longindex);
#endif

#ifndef HAVE_USLEEP
int usleep(unsigned long usec);	/* SUSv2 */
#endif

#ifndef HAVE_GETTIMEOFDAY
#ifdef HAVE_SYS_TIME_H
#include <sys/time.h>
#endif
#ifndef HAVE_STRUCT_TIMEZONE
struct timezone {
        int  tz_minuteswest;
	int  tz_dsttime;
};
#endif
int gettimeofday(struct timeval *tv, struct timezone *tz);
#endif

#ifndef timersub
# define timersub(a, b, result) \
    do { \
        (result)->tv_sec = (a)->tv_sec - (b)->tv_sec; \
        (result)->tv_usec = (a)->tv_usec - (b)->tv_usec; \
        if ((result)->tv_usec < 0) { \
            --(result)->tv_sec; \
            (result)->tv_usec += 1000000; \
        } \
    } while (0)
#endif

#ifndef HAVE_SSIZE_T
typedef size_t ssize_t;
#endif

#ifdef  __cplusplus
}
#endif
])


])
