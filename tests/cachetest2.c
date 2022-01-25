/*  This program does a fast iteration of v f m t s
 *  By Michael Black W9MDB
 *  This allows testing of another program using rigctld
 *  to test changin vfo, freq, mode, PTT, or split and see the change immediately in this program.
 *  Used in testing caching effects that have been added
 *  To compile:
 *      gcc -I../src -I../include -g -o cachetest2 cachetest2.c -lhamlib
 *  To run:
 *      rigctld
 *      ./cachetest2
 *      Note: cachtest2 should show immediately when v f m t s are changed
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
    int retcode;
    int model;
    int cache_timeout = 0;

    if (argc != 1)
    {
        fprintf(stderr, "Usage: %s\n", argv[0]);
        exit(1);
    }

    model = 2;

    rig_set_debug(RIG_DEBUG_NONE);

    /* Instantiate a rig */
    my_rig = rig_init(model); // your rig model.

    /* Set up serial port, baud rate */
    rig_file = "127.0.0.1:4532";        // your serial device

    strncpy(my_rig->state.rigport.pathname, rig_file, HAMLIB_FILPATHLEN - 1);

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

    while (1)
    {
        static freq_t freq, freq_last = 0;
        static rmode_t mode, mode_last = 0;
        static pbwidth_t width, width_last = 0;
        static vfo_t vfo, vfo_last = RIG_VFO_NONE;
        static vfo_t txvfo, txvfo_last = RIG_VFO_NONE;
        static ptt_t ptt, ptt_last = -1;
        static split_t split, split_last = -1;

        retcode = rig_get_vfo(my_rig, &vfo);

        if (retcode != RIG_OK) { printf("Get vfo failed?? Err=%s\n", rigerror(retcode)); }

        if (vfo != vfo_last)
        {
            printf("VFO = %s\n", rig_strvfo(vfo));
            vfo_last = vfo;
        }

        retcode = rig_get_freq(my_rig, RIG_VFO_CURR, &freq);

        if (retcode != RIG_OK) { printf("Get freq failed?? Err=%s\n", rigerror(retcode)); }

        if (freq != freq_last)
        {
            printf("VFO freq. = %.1f Hz\n", freq);
            freq_last = freq;
        }

        retcode = rig_get_mode(my_rig, RIG_VFO_CURR, &mode, &width);

        if (retcode != RIG_OK) { printf("Get mode failed?? Err=%s\n", rigerror(retcode)); }

        if (mode != mode_last || width != width_last)
        {
            printf("Current mode = %s, width = %ld\n", rig_strrmode(mode), width);
            mode_last = mode;
            width_last = width;
        }

        retcode = rig_get_ptt(my_rig, RIG_VFO_CURR, &ptt);

        if (retcode != RIG_OK) { printf("Get ptt failed?? Err=%s\n", rigerror(retcode)); }

        if (ptt != ptt_last)
        {
            printf("ptt=%d\n", ptt);
            ptt_last = ptt;
        }

        retcode = rig_get_split_vfo(my_rig, RIG_VFO_CURR, &split, &txvfo);

        if (retcode != RIG_OK) { printf("Get split_vfo failed?? Err=%s\n", rigerror(retcode)); }

        if (split != split_last || txvfo != txvfo_last)
        {
            printf("split=%d, tx_vfo=%s\n", split,
                   rig_strvfo(vfo));
            split_last = split;
            txvfo_last = txvfo;
        }
    }

    rig_close(my_rig);
    return 0;

};
