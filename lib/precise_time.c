// ---------------------------------------------------------------------
// precise_time.cxx
//
// Copyright (C) 2023
//      Dave Freese, W1HKJ
//
// This file is part of flrig
//
// flrig is free software: you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation, either version 3 of the License, or
// (at your option) any later version.
//
// flrig is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.
//
// You should have received a copy of the GNU General Public License
// along with fldigi.  If not, see <http://www.gnu.org/licenses/>.
// ---------------------------------------------------------------------

#include <errno.h>
#include <time.h>
#include <sys/time.h>

#if 0
// return current tick time in seconds
double monotonic_seconds()
{
    static struct timeval t1;
    gettimeofday(&t1, NULL);
    return t1.tv_sec + t1.tv_usec / 1e6;
}

#else

//======================================================================
//
// A cross platform monotonic timer.
// Copyright 2013 Alex Reece.
//

#include <unistd.h>

#define NANOS_PER_SECF 1000000000.0
#define USECS_PER_SEC 1000000


#if _POSIX_TIMERS > 0 && defined(_POSIX_MONOTONIC_CLOCK)
// If we have it, use clock_gettime and CLOCK_MONOTONIC.

#include <time.h>

static int showme = 0;

double monotonic_seconds()
{
    if (showme)
    {
        showme = 0;
    }

    struct timespec time;

    // Note: Make sure to link with -lrt to define clock_gettime.
    clock_gettime(CLOCK_MONOTONIC, &time);

    return ((double) time.tv_sec) + ((double) time.tv_nsec / (NANOS_PER_SECF));
}

#elif defined(__APPLE__)
// If we don't have CLOCK_MONOTONIC, we might be on a Mac. There we instead
// use mach_absolute_time().

#include <mach/mach_time.h>

static mach_timebase_info_data_t info;
static void __attribute__((constructor)) init_info()
{
    mach_timebase_info(&info);
}

double monotonic_seconds()
{
    uint64_t time = mach_absolute_time();
    double dtime = (double) time;
    dtime *= (double) info.numer;
    dtime /= (double) info.denom;
    return dtime / NANOS_PER_SECF;
}

#elif defined(__WIN32__)
// On Windows, use QueryPerformanceCounter and QueryPerformanceFrequency.

#include <windows.h>

static double PCFreq = 0.0;

// According to http://stackoverflow.com/q/1113409/447288, this will
// make this function a constructor.
// TODO(awreece) Actually attempt to compile on windows.
// w1hkj - builds OK on mingw32

static void __cdecl init_pcfreq();
__declspec(allocate(".CRT$XCU")) void (__cdecl *init_pcfreq_)() = init_pcfreq;
static void __cdecl init_pcfreq()
{
    // Accoring to http://stackoverflow.com/a/1739265/447288, this will
    // properly initialize the QueryPerformanceCounter.
    LARGE_INTEGER li;
    int has_qpc = QueryPerformanceFrequency(&li);
    assert(has_qpc);

    PCFreq = ((double) li.QuadPart) / 1000.0;
}

double monotonic_seconds()
{
    LARGE_INTEGER li;
    QueryPerformanceCounter(&li);
    return ((double) li.QuadPart) / PCFreq;
}

#else
// Fall back to rdtsc. The reason we don't use clock() is this scary message
// from the man page:
//     "On several other implementations, the value returned by clock() also
//      includes the times of any children whose status has been collected via
//      wait(2) (or another wait-type call)."
//
// Also, clock() only has microsecond accuracy.
//
// This whitepaper offered excellent advice on how to use rdtscp for
// profiling: http://download.intel.com/embedded/software/IA/324264.pdf
//
// Unfortunately, we can't follow its advice exactly with our semantics,
// so we're just going to use rdtscp with cpuid.
//
// Note that rdtscp will only be available on new processors.

#include <stdint.h>

static inline uint64_t rdtsc()
{
    uint32_t hi, lo;
    uint64_t hi64, lo64;
    asm volatile("rdtscp\n"
                 "movl %%edx, %0\n"
                 "movl %%eax, %1\n"
                 "cpuid"
                 : "=r"(hi), "=r"(lo) : : "%rax", "%rbx", "%rcx", "%rdx");
    hi64 = hi;
    lo64 = lo;
    return (hi64 << 32) | lo64;
}

static uint64_t rdtsc_per_sec = 0;
static void __attribute__((constructor)) init_rdtsc_per_sec()
{
    uint64_t before, after;

    before = rdtsc();
    usleep(USECS_PER_SEC);
    after = rdtsc();

    rdtsc_per_sec = after - before;
}

double monotonic_seconds()
{
    if (showme)
    {
        showme = false;
    }

    return (double) rdtsc() / (double) rdtsc_per_sec;
}

#endif

#endif
