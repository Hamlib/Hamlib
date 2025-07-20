/* Borrowed for Hamlib from:
 *  String Crypt Test (Linux)
 *  Copyright (C) 2012, 2015
 *
 *  Author: Paul E. Jones <paulej@packetizer.com>
 *
 * This software is licensed as "freeware."  Permission to distribute
 * this software in source and binary forms is hereby granted without a
 * fee.  THIS SOFTWARE IS PROVIDED 'AS IS' AND WITHOUT ANY EXPRESSED OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE.
 * THE AUTHOR SHALL NOT BE HELD LIABLE FOR ANY DAMAGES RESULTING FROM
 * THE USE OF THIS SOFTWARE, EITHER DIRECTLY OR INDIRECTLY, INCLUDING,
 * BUT NOT LIMITED TO, LOSS OF DATA OR DATA BEING RENDERED INACCURATE.
 *
 */

#include <stdio.h>
#include <string.h>
#include "hamlib/rig.h"
#ifdef _WIN32
#include <windows.h>
//#include <Wincrypt.h>
#else
#include <unistd.h>
#endif

#include "AESStringCrypt.h"
#include "password.h"
#include "../src/misc.h"
#include "hamlib/config.h"

#if defined(_WIN32)
// gmtime_r can be defined by mingw
#ifndef gmtime_r
static struct tm *gmtime_r(const time_t *t, struct tm *r)
{
    // gmtime is threadsafe in windows because it uses TLS
    const struct tm *theTm = gmtime(t);

    if (theTm)
    {
        *r = *theTm;
        return r;
    }
    else
    {
        return 0;
    }
}
#endif // gmtime_r
#endif // _WIN32


// using tv_usec with a sleep gives a fairly good random number
static int my_rand(int max)
{
    time_t t;
    struct timeval tv;
    struct tm result;

    t = time(NULL);
    gmtime_r(&t, &result);

    gettimeofday(&tv, NULL);
    hl_usleep(100);
    int val = tv.tv_usec % max;
    return val;
}

void rig_make_key(char key[33])
{
    const char *all =
        "abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ123467890!@#$%^&*()_=~<>/?";
    int max = strlen(all);
    int i;

    for (i = 0; i < 32; ++i)
    {
        key[i] = all[my_rand(max)];
    }

    key[32] = 0;
}

