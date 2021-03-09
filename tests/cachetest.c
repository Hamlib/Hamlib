/*  This program does a number of iterations of v f m t s
 *  By Michael Black W9MDB
 *  This simulates what WSJT-X and JTDX do
 *  Used in testing caching effects that have been added
 *  Original performance against dummy device with 20ms delays showed
 *  about 50 calls/sec to get frequency.  After caching was added can
 *  do about 37,000 calls/sec or about 740 times faster.
 *  To compile:
 *      gcc -I../src -I../include -g -o cachetest cachetest.c -lhamlib
 *  To test dummy device cache effect:
 *      dummy without any cache -- 12 iterations per second
 *      ./cachetest 1 "" 0 12 0
 *      Elapsed:Elapsed 1.004sec
 *
 *      dummy with 500ms cache (default value) -- 12 iterations in 0.071sec
 *      ./cachetest 1 "" 0 12 500
 *      Elapsed 0.071sec
 *
 *      dummy with 5700 iterations in less than 1 second with 500ms cache
 *      ./cachetest 1 "" 0 5700 500
 *      Elapsed 0.872sec
 *
 *      ic-706mkiig  -- no cache
 *      ./cachetest 3011 /dev/ttyUSB0 19200 12 0
 *      Elapsed 1.182sec
 *
 *      ic-706mkiig  -- 500ms cache
 *      ./cachetest 3011 /dev/ttyUSB0 19200 12 500
 *      Elapsed 0.13sec
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <hamlib/rig.h>
#include <hamlib/riglist.h>
#include "sprintflst.h"
#include "misc.h"


int main(int argc, char *argv[])
{
    RIG *my_rig;
    char *rig_file, *info_buf;
    freq_t freq;
    int retcode;
    int model;
    int baud;
    int loops;
    int cache_timeout = 500;
    int i;
    struct timespec start, startall;

    if (argc < 5)
    {
        fprintf(stderr, "Usage: %s model port baud loops [cachetimems]\n", argv[0]);
        fprintf(stderr,
                "cachetimems defaults to 500ms, 0 to disable or pick your own cache time\n");
        fprintf(stderr, "To test rigctld: %s 2 127.0.0.1:4532 0 1000 500\n", argv[0]);
        exit(1);
    }

    model = atoi(argv[1]);
    baud = atoi(argv[3]);
    loops = atoi(argv[4]);

    if (argc == 6)
    {
        cache_timeout = atoi(argv[5]);
    }

    rig_set_debug(RIG_DEBUG_NONE);

    /* Instantiate a rig */
    my_rig = rig_init(model); // your rig model.

    /* Set up serial port, baud rate */
    rig_file = argv[2];        // your serial device

    strncpy(my_rig->state.rigport.pathname, rig_file, HAMLIB_FILPATHLEN - 1);

    my_rig->state.rigport.parm.serial.rate = baud; // your baud rate

    /* Open my rig */
    retcode = rig_open(my_rig);

    if (retcode != RIG_OK)
    {
        fprintf(stderr, "%s: rig_open failed %s\n", __func__,
                rigerror(retcode));
        return 1;
    }

    rig_set_cache_timeout_ms(my_rig, HAMLIB_CACHE_ALL, cache_timeout);
    /* Give me ID info, e.g., firmware version. */
    info_buf = (char *)rig_get_info(my_rig);

    if (info_buf)
    {
        strtok(info_buf, "\r\n");
        printf("Rig_info: '%s'\n", info_buf);
    }

    elapsed_ms(&startall, HAMLIB_ELAPSED_SET);

    for (i = 0; i < loops; ++i)
    {
        rmode_t mode;
        pbwidth_t width;
        vfo_t vfo;
        ptt_t ptt;
        split_t split;

        elapsed_ms(&start, HAMLIB_ELAPSED_SET);

        retcode = rig_get_vfo(my_rig, &vfo);

        if (retcode != RIG_OK) { printf("Get vfo failed?? Err=%s\n", rigerror(retcode)); }

        printf("%4dms: VFO = %s\n", (int)elapsed_ms(&start, HAMLIB_ELAPSED_GET),
               rig_strvfo(vfo));

        elapsed_ms(&start, HAMLIB_ELAPSED_SET);
        retcode = rig_get_freq(my_rig, RIG_VFO_CURR, &freq);

        if (retcode != RIG_OK) { printf("Get freq failed?? Err=%s\n", rigerror(retcode)); }

        printf("%4dms: VFO freq. = %.1f Hz\n", (int)elapsed_ms(&start,
                HAMLIB_ELAPSED_GET),
               freq);
        elapsed_ms(&start, HAMLIB_ELAPSED_SET);
        retcode = rig_get_mode(my_rig, RIG_VFO_CURR, &mode, &width);

        if (retcode != RIG_OK) { printf("Get mode failed?? Err=%s\n", rigerror(retcode)); }

        printf("%4dms: Current mode = %s, width = %ld\n", (int)elapsed_ms(&start,
                HAMLIB_ELAPSED_GET),
               rig_strrmode(mode), width);

        elapsed_ms(&start, HAMLIB_ELAPSED_SET);
        retcode = rig_get_ptt(my_rig, RIG_VFO_A, &ptt);

        if (retcode != RIG_OK) { printf("Get ptt failed?? Err=%s\n", rigerror(retcode)); }

        printf("%4dms: ptt=%d\n", (int)elapsed_ms(&start, HAMLIB_ELAPSED_GET), ptt);

#if 1
        elapsed_ms(&start, HAMLIB_ELAPSED_SET);
        retcode = rig_get_split_vfo(my_rig, RIG_VFO_A, &split, &vfo);

        if (retcode != RIG_OK) { printf("Get split_vfo failed?? Err=%s\n", rigerror(retcode)); }

        printf("%4dms: split=%d, tx_vfo=%s\n", (int)elapsed_ms(&start,
                HAMLIB_ELAPSED_GET),
               split,
               rig_strvfo(vfo));
#endif
    }

    printf("Elapsed %gsec\n", (int)elapsed_ms(&startall,
            HAMLIB_ELAPSED_GET) / 1000.0);

    rig_close(my_rig);
    return 0;

};
