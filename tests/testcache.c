/*  This program does a check of cache timing and hit/miss
 *  By Michael Black W9MDB
 *  Used in testing caching effects that have been added
 *  To compile:
 *      gcc -I../src -I../include -g -o cachetest3 cachetest3.c -lhamlib
 *  To run:
 *      ./cachetest3
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <hamlib/rig.h>
#include <hamlib/riglist.h>
//#include "misc.h"


int main(int argc, char *argv[])
{
    RIG *my_rig;
    char *rig_file, *info_buf;
    int retcode;
    int model;
    int cache_timeout = 0;

    model = 1; // we'll just use the dummy rig by default
    rig_file = "127.0.0.1:4532";        // default if we use model#2

    if (argc == 2)
    {
        model = atoi(argv[1]);

        if (model == 1) { rig_file = ""; }
    }

    if (argc == 3)
    {
        rig_file = argv[2];
    }

    printf("Model#%d\n", model);
    rig_set_debug(RIG_DEBUG_CACHE);

    /* Instantiate a rig */
    my_rig = rig_init(model); // your rig model.

    /* Set up serial port, baud rate */

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
        char *s = strdup(info_buf);
        strtok(s, "\r\n");
        printf("Rig_info: '%s'\n", s);
        free(s);
    }

    vfo_t vfo;
    retcode = rig_get_vfo(my_rig, &vfo);

    if (vfo != RIG_VFO_A)
    {
        printf("VFO != VFOA\n");
        exit(1);
    }

    printf("VFO: %s\n", rig_strvfo(vfo));
    freq_t freqA, freqB, freqC;
    rmode_t modeA, modeB, modeC;
    pbwidth_t widthA, widthB, widthC;
    int freq_ms, mode_ms, width_ms;
    rig_get_cache(my_rig, vfo, &freqA, &freq_ms, &modeA, &mode_ms, &widthA,
                  &width_ms);
    printf("freq=%.0f cache times=%d,%d,%d\n", freqA, freq_ms, mode_ms, width_ms);
    rig_get_cache(my_rig, vfo, &freqA, &freq_ms, &modeA, &mode_ms, &widthA,
                  &width_ms);
    printf("freq=%.0f cache times=%d,%d,%d\n", freqA, freq_ms, mode_ms, width_ms);
    rig_set_freq(my_rig, RIG_VFO_A, 14074000.0);
    rig_set_freq(my_rig, RIG_VFO_B, 14075000.0);
    rig_set_freq(my_rig, RIG_VFO_C, 14076000.0);
    rig_set_mode(my_rig, RIG_VFO_A, RIG_MODE_USB, 1000);
    rig_set_mode(my_rig, RIG_VFO_B, RIG_MODE_LSB, 2000);
    rig_set_mode(my_rig, RIG_VFO_C, RIG_MODE_PKTUSB, 3000);

    rig_get_cache(my_rig, RIG_VFO_A, &freqA, &freq_ms, &modeA, &mode_ms, &widthA,
                  &width_ms);
    printf("VFOA freq=%.0f, mode=%s, width=%d, cache times=%d,%d,%d\n", freqA,
           rig_strrmode(modeA), (int)widthA, freq_ms, mode_ms, width_ms);

    rig_get_cache(my_rig, RIG_VFO_B, &freqB, &freq_ms, &modeB, &mode_ms, &widthB,
                  &width_ms);
    printf("VFOB freq=%.0f, mode=%s, width=%d, cache times=%d,%d,%d\n", freqB,
           rig_strrmode(modeB), (int)widthB, freq_ms,  mode_ms, width_ms);

    rig_get_cache(my_rig, RIG_VFO_C, &freqC, &freq_ms, &modeC, &mode_ms, &widthC,
                  &width_ms);
    printf("VFOC freq=%.0f, mode=%s, width=%d, cache times=%d,%d,%d\n", freqC,
           rig_strrmode(modeC), (int)widthC, freq_ms, mode_ms, width_ms);

    if (freqA != 14074000) { printf("freqA == %.1f\n", freqA); exit(1); }

    if (modeA != RIG_MODE_USB) { printf("modeA = %s\n", rig_strrmode(modeA)); exit(1); }

    if (widthA != 1000) { printf("widthA = %d\n", (int)widthA); exit(1); }

    if (freqB != 14075000) { printf("freqB = %.1f\n", freqB); exit(1); }

    if (modeB != RIG_MODE_LSB) { printf("modeB = %s\n", rig_strrmode(modeB)); exit(1); }

    if (widthB != 2000) { printf("widthB = %d\n", (int)widthB); exit(1); }

    if (freqC != 14076000) { printf("freqC = %.1f\n", freqC); exit(1); }

    if (modeC != RIG_MODE_PKTUSB) { printf("modeC = %s\n", rig_strrmode(modeC)); exit(1); }

    if (widthC != 3000) { printf("widthC = %d\n", (int)widthC); exit(1); }

#if 0 // PTT does not work for dummy device
    printf("PTT ON\n");
    rig_set_ptt(my_rig, RIG_VFO_CURR, RIG_PTT_ON);
    ptt_t ptt;
    printf("PTT get ptt ON\n");
    rig_get_ptt(my_rig, RIG_VFO_CURR, &ptt);

    if (ptt != RIG_PTT_ON) { printf("ptt != ON\n"); exit(1); }

    hl_usleep(1000 * 1000);
    rig_get_ptt(my_rig, RIG_VFO_CURR, &ptt);
    printf("PTT get ptt ON\n");

    if (ptt != RIG_PTT_ON) { printf("ptt != ON\n"); exit(1); }

    printf("PTT ptt OFF\n");
    rig_set_ptt(my_rig, RIG_VFO_CURR, RIG_PTT_OFF);

    if (ptt != RIG_PTT_ON) { printf("ptt != ON\n"); exit(1); }

    rig_get_ptt(my_rig, RIG_VFO_CURR, &ptt);
    printf("PTT get ptt OFF\n");
#endif

    vfo_t tx_vfo;
    split_t split;
    rig_get_split_vfo(my_rig, RIG_VFO_A, &split, &tx_vfo);
    printf("split=%d, tx_vfo=%s\n", split, rig_strvfo(tx_vfo));

    if (split != 0 || tx_vfo != RIG_VFO_A) { printf("split#1 failed\n"); exit(1); }

    rig_set_split_vfo(my_rig, RIG_VFO_A, RIG_SPLIT_ON, RIG_VFO_B);
    hl_usleep(1000 * 1000);
    rig_get_split_vfo(my_rig, RIG_VFO_A, &split, &tx_vfo);
    printf("split=%d, tx_vfo=%s\n", split, rig_strvfo(tx_vfo));

    if (split != RIG_SPLIT_ON || (tx_vfo != RIG_VFO_B && tx_vfo != RIG_VFO_SUB)) { printf("split#2 failed\n"); exit(1); }

    printf("All OK\n");
    rig_close(my_rig);
    return 0 ;
};
