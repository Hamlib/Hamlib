/*
 * Hamlib Yaesu backend - FTX-1 Mode Commands
 * Copyright (c) 2025 by Terrell Deppe (KJ5HST)
 *
 * Mode commands are mostly handled by newcat_set_mode/newcat_get_mode.
 * This file adds an FTX-1 set_mode wrapper that forces the radio out
 * of Memory mode before delegating to newcat, because Memory-mode MD
 * sets on the Main side are accepted by firmware but treated as a
 * transient memory-tune overlay that does not persist when the user
 * leaves the channel.
 *
 * CAT Commands:
 *   MD P1 P2;  - Operating Mode (P1=VFO 0/1, P2=mode code)
 *
 * Mode Codes (P2 for MD command):
 *   1=LSB, 2=USB, 3=CW-U, 4=FM, 5=AM, 6=RTTY-L, 7=CW-L,
 *   8=DATA-L, 9=RTTY-U, A=DATA-FM, B=FM-N, C=DATA-U, D=AM-N,
 *   E=PSK, F=DATA-FM-N, H=C4FM-DN, I=C4FM-VW
 *
 * Note: SH (Width) command - read-write per CAT manual (Set/Read/Answer).
 *       NA (Notch Auto) - handled via ftx1_noise.c
 */

#include <hamlib/rig.h>
#include "newcat.h"
#include "ftx1.h"

static int ftx1_check_freq_wfm(const RIG *rig, freq_t freq)
{
    size_t i;

    for (i = 0; i < HAMLIB_FRQRANGESIZ
            && !RIG_IS_FRNG_END(STATE(rig)->rx_range_list[i]); i++)
    {
        if (freq >= STATE(rig)->rx_range_list[i].startf && freq <= STATE(rig)->rx_range_list[i].endf)
        {
            if (STATE(rig)->rx_range_list[i].modes & RIG_MODE_WFM)
            {
                return 1;
            }
        }
    }

    return 0;
}

int ftx1_get_mode(RIG *rig, vfo_t vfo, rmode_t *mode, pbwidth_t *width)
{
    int ret;
    freq_t freq;

    ret = newcat_get_mode(rig, vfo, mode, width);
    if (ret == RIG_OK)
    {
        return ret;
    }

    /* We add special handing on -RIG_EPROTO */
    if (ret != -RIG_EPROTO)
    {
        return ret;
    }

    rig_debug(RIG_DEBUG_TRACE, "%s: got RIG_EPROTO from newcat_get_mode, check if it is WFM\n", __func__);

    if (*mode != '0')
    {
        return ret;
    }
    ret = rig_get_freq(rig, vfo, &freq);

    if (ret != RIG_OK)
    {
        rig_debug(RIG_DEBUG_ERR, "%s: rig_get_freq failed: %d\n", __func__, ret);
        return ret;
    }

    if (!ftx1_check_freq_wfm(rig, freq))
    {
        rig_debug(RIG_DEBUG_ERR, "%s: frequency %f is not in any WFM range, returning original error\n", __func__, freq);
        return -RIG_EPROTO;
    }

    rig_debug(RIG_DEBUG_VERBOSE, "%s: frequency %f is in WFM range, setting mode to WFM\n", __func__, freq);
    *mode = RIG_MODE_WFM;
    *width = rig_passband_normal(rig, *mode);
    return RIG_OK;
}

int ftx1_set_mode(RIG *rig, vfo_t vfo, rmode_t mode, pbwidth_t width)
{
    freq_t freq;
    int ret;

    /* Memory-mode MD sets on Main are accepted but do not persist —
     * they act as a transient memory-tune overlay. Force an exit so
     * the user's mode change actually sticks. */
    ftx1_ensure_vfo_mode(rig);

    if (mode == RIG_MODE_WFM)
    {
        rig_debug(RIG_DEBUG_VERBOSE, "%s: trying to set WFM mode, checking frequency\n", __func__);
        ret = rig_get_freq(rig, vfo, &freq);
        if (ret != RIG_OK)
        {
            rig_debug(RIG_DEBUG_ERR, "%s: rig_get_freq failed: %d\n", __func__, ret);
            return ret;
        }
        if (!ftx1_check_freq_wfm(rig, freq))
        {
            rig_debug(RIG_DEBUG_ERR, "%s: frequency %f is not in any WFM range, cannot set WFM mode\n", __func__, freq);
            return -RIG_EINVAL;
        }
        rig_debug(RIG_DEBUG_VERBOSE, "%s: frequency %f is in WFM range, proceeding to set WFM mode\n", __func__, freq);
        rig_debug(RIG_DEBUG_VERBOSE, "%s: assume normal FM mode\n", __func__);
        mode = RIG_MODE_FM;
        width = RIG_PASSBAND_NOCHANGE;
    }

    return newcat_set_mode(rig, vfo, mode, width);
}
