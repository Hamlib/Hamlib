/**
 * \addtogroup rig
 * @{
 */

/**
 * \file src/sleep.c
 * \brief Replacements for sleep and usleep
 * \author Michael Black W9MDB
 * \date 2020
 */
/*
 *  Hamlib Interface - Replacements for sleep and usleep
 *  Copyright (c) 2020 by Michael Black W9MDB
 *
 *
 *   This library is free software; you can redistribute it and/or
 *   modify it under the terms of the GNU Lesser General Public
 *   License as published by the Free Software Foundation; either
 *   version 2.1 of the License, or (at your option) any later version.
 *
 *   This library is distributed in the hope that it will be useful,
 *   but WITHOUT ANY WARRANTY; without even the implied warranty of
 *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 *   Lesser General Public License for more details.
 *
 *   You should have received a copy of the GNU Lesser General Public
 *   License along with this library; if not, write to the Free Software
 *   Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA
 *
 */

/**
 * \brief provide sleep and usleep replacements
 * \note parameters are same as man page for each
 *
 */
#include "hamlib/config.h"
#include <unistd.h>
#include <pthread.h>
#include "sleep.h"

#ifdef  __cplusplus
extern "C" {
#endif

extern double monotonic_seconds();

int hl_usleep(rig_useconds_t usec)
{
    double sleep_time = usec / 1e6;
    struct timespec tv1;
    double delay = sleep_time;

    if (sleep_time > .001) { delay -= .0001; }
    else if (sleep_time > .0001) { delay -= .00005; }

    tv1.tv_sec = (time_t) delay;
    tv1.tv_nsec = (long)((delay - tv1.tv_sec) * 1e+9);
//    rig_debug(RIG_DEBUG_CACHE,"usec=%ld, sleep_time=%f, tv1=%ld,%ld\n", usec, sleep_time, (long)tv1.tv_sec,
//           (long)tv1.tv_nsec);

#ifdef __WIN32__

    if (sleep_time < 0.003)
    {
        // Busy-wait for small durations < 2 milliseconds
        LARGE_INTEGER frequency, start, end;
        double elapsed;
        QueryPerformanceFrequency(&frequency);
        QueryPerformanceCounter(&start);

        do
        {
            struct timespec startc;
            clock_gettime(CLOCK_REALTIME, &startc);
            QueryPerformanceCounter(&end);
            elapsed = (double)(end.QuadPart - start.QuadPart) / frequency.QuadPart;
        }
        while (elapsed < sleep_time);
    }
    else
    {
        // Use Sleep for larger durations >= 3 milliseconds
        //Sleep((DWORD)(seconds * 1000 - 1));  // Convert seconds to milliseconds
        usleep(sleep_time * 1e6 - 400);
    }

#else
    {
        struct timespec tv2;
        double lasterr = 0;
        double start_at = monotonic_seconds();
        double end_at = start_at + sleep_time;
        tv2.tv_sec = 0;
        tv2.tv_nsec = 1000000;
        nanosleep(&tv1, NULL);

        while (((lasterr = end_at - monotonic_seconds()) > 0))
        {
            nanosleep(&tv2, NULL);
        }
    }

#endif
    return 0;
}

#if 0
// In order to stop the usleep warnings in cppcheck we provide our own interface
// So this will use system usleep or our usleep depending on availability of nanosleep
// This version of usleep can handle > 1000000 usec values
int hl_usleep(rig_useconds_t usec)
{
    int retval = 0;
    rig_debug(RIG_DEBUG_ERR, "%s: usec=%ld\n", __func__, usec);

    if (usec <= 1000) { return 0; } // don't sleep if only 1ms is requested -- speeds things up on Windows

    while (usec > 1000000)
    {
        if (retval != 0) { return retval; }

        retval = usleep(1000000);
        usec -= 1000000;
    }

#ifdef HAVE_NANOSLEEP
    struct timespec t, tleft;
    t.tv_sec = usec / 1e6;
    t.tv_nsec = (usec - (t.tv_sec * 1e6)) * 1e3;
    return nanosleep(&t, &tleft);
#else
    return usleep(usec);
#endif
}
#endif


#ifdef HAVE_NANOSLEEP
#ifndef HAVE_SLEEP
/**
 * \brief sleep
 * \param secs is seconds to sleep
 */
unsigned int sleep(unsigned int secs)
{
    int retval;
    struct timespec t;
    struct timespec tleft;
    t.tv_sec = secs;
    t.tv_nsec = 0;
    retval = nanosleep(&t, &tleft);

    if (retval == -1) { return tleft.tv_sec; }

    return 0;
}
#endif


#if 0
/**
 * \brief microsecond sleep
 * \param usec is microseconds to sleep
 * This does not have the same 1000000 limit as POSIX usleep
 */
int usleep(rig_useconds_t usec)
{
    int retval;
    unsigned long sec = usec / 1000000ul;
    unsigned long nsec = usec * 1000ul - (sec * 1000000000ul);
    struct timespec t;
    t.tv_sec = sec;
    t.tv_nsec = nsec;
    retval = nanosleep(&t, NULL);

    // EINTR is the only error return usleep should need
    // since EINVAL should not be a problem
    if (retval == -1) { return EINTR; }

    return 0;
}
#endif

#endif // HAVE_NANOSLEEP
#ifdef __cplusplus
}
#endif
/** @} */

#ifdef TEST
#include "misc.h"
// cppcheck-suppress unusedFunction
double get_elapsed_time(struct tm start, struct tm end)
{
    // Convert struct tm to time_t
    time_t start_seconds = mktime(&start);
    time_t end_seconds = mktime(&end);

    double elapsed_time = difftime(end_seconds, start_seconds);
    return elapsed_time;
}


int main()
{
    //struct tm start_time, end_time;
    time_t rawtime;

    for (int i = 0; i < 11; ++i)
    {
        char buf[256];
        time(&rawtime);
        hl_usleep(1000000); // test 1s sleep
        date_strget(buf, sizeof(buf), 0);
        printf("%s\n", buf);
        time(&rawtime);
    }

    return 0;
}
#endif
