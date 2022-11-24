/*
 *  Hamlib FlexRadio backend - 6K series rigs
 *  Copyright (c) 2002-2009 by Stephane Fillod
 *  Copyright (C) 2010,2011,2012,2013 by Nate Bargmann, n0nb@arrl.net
 *  Copyright (C) 2013 by Steve Conklin AI4QR, steve@conklinhouse.com
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
 *  See the file 'COPYING.LIB' in the main Hamlib distribution directory for
 *  the complete text of the GNU Lesser Public License version 2.1.
 *
 */

#include <hamlib/config.h>

#include <stdlib.h>
#include <math.h>

#include <hamlib/rig.h>
#include "kenwood.h"
#include "bandplan.h"
#include "flex.h"
#include "token.h"
#include "cal.h"

#define F6K_MODES (RIG_MODE_CW|RIG_MODE_SSB|RIG_MODE_AM|RIG_MODE_FM|RIG_MODE_PKTLSB|RIG_MODE_PKTUSB)

#define F6K_FUNC_ALL (RIG_FUNC_VOX|RIG_FUNC_TUNER)

#define F6K_LEVEL_ALL (RIG_LEVEL_SLOPE_HIGH|RIG_LEVEL_SLOPE_LOW|RIG_LEVEL_KEYSPD)

#define F6K_VFO (RIG_VFO_A|RIG_VFO_B)
#define POWERSDR_VFO_OP (RIG_OP_BAND_UP|RIG_OP_BAND_DOWN|RIG_OP_UP|RIG_OP_DOWN)

#define F6K_ANTS (RIG_ANT_1|RIG_ANT_2|RIG_ANT_3)

/* PowerSDR differences */
#define POWERSDR_MODES (RIG_MODE_CW|RIG_MODE_SSB|RIG_MODE_AM|RIG_MODE_FM|RIG_MODE_PKTLSB|RIG_MODE_PKTUSB|RIG_MODE_SPEC)

#define POWERSDR_FUNC_ALL (RIG_FUNC_VOX|RIG_FUNC_SQL|RIG_FUNC_NB|RIG_FUNC_ANF|RIG_FUNC_MUTE|RIG_FUNC_RIT|RIG_FUNC_XIT|RIG_FUNC_TUNER)

#define POWERSDR_LEVEL_ALL (RIG_LEVEL_SLOPE_HIGH|RIG_LEVEL_SLOPE_LOW|RIG_LEVEL_KEYSPD|RIG_LEVEL_RFPOWER|RIG_LEVEL_RFPOWER_METER|RIG_LEVEL_RFPOWER_METER_WATTS|RIG_LEVEL_MICGAIN|RIG_LEVEL_VOXGAIN|RIG_LEVEL_SQL|RIG_LEVEL_AF|RIG_LEVEL_AGC|RIG_LEVEL_RF|RIG_LEVEL_IF|RIG_LEVEL_STRENGTH)
#define POWERSDR_LEVEL_SET (RIG_LEVEL_SLOPE_HIGH|RIG_LEVEL_SLOPE_LOW|RIG_LEVEL_KEYSPD|RIG_LEVEL_RFPOWER|RIG_LEVEL_MICGAIN|RIG_LEVEL_VOXGAIN|RIG_LEVEL_SQL|RIG_LEVEL_AF|RIG_LEVEL_AGC|RIG_LEVEL_RF|RIG_LEVEL_IF)


static rmode_t flex_mode_table[KENWOOD_MODE_TABLE_MAX] =
{
    [0] = RIG_MODE_NONE,
    [1] = RIG_MODE_LSB,
    [2] = RIG_MODE_USB,
    [3] = RIG_MODE_CW,
    [4] = RIG_MODE_FM,
    [5] = RIG_MODE_AM,
    [6] = RIG_MODE_PKTLSB,
    [7] = RIG_MODE_NONE,
    [8] = RIG_MODE_NONE,
    [9] = RIG_MODE_PKTUSB
};

static rmode_t powersdr_mode_table[KENWOOD_MODE_TABLE_MAX] =
{
    [0] = RIG_MODE_LSB,
    [1] = RIG_MODE_USB,
    [2] = RIG_MODE_DSB,
    [3] = RIG_MODE_CWR,
    [4] = RIG_MODE_CW,
    [5] = RIG_MODE_FM,
    [6] = RIG_MODE_AM,
    [7] = RIG_MODE_PKTUSB,
    [8] = RIG_MODE_NONE, // SPEC -- not implemented
    [9] = RIG_MODE_PKTLSB,
    [10] = RIG_MODE_SAM,
    [11] = RIG_MODE_NONE // DRM -- not implemented
};

static struct kenwood_priv_caps f6k_priv_caps  =
{
    .cmdtrm =  EOM_KEN,
    .mode_table = flex_mode_table,
    .if_len = 37
};

static struct kenwood_priv_caps powersdr_priv_caps  =
{
    .cmdtrm =  EOM_KEN,
    .mode_table = powersdr_mode_table,
    .if_len = 37
};

#define DSP_BW_NUM 8

static int dsp_bw_ssb[DSP_BW_NUM] =
{
    4000, 3300, 2900, 2700, 2400, 2100, 1800, 1600
};

static int dsp_bw_am[DSP_BW_NUM] =
{
    20000, 16000, 14000, 12000, 10000, 8000, 6000, 5600
};

static int dsp_bw_cw[DSP_BW_NUM] =
{
    3000, 1500, 1000, 800, 400, 250, 100, 50
};

static int dsp_bw_dig[DSP_BW_NUM] =
{
    3000, 2000, 1500, 1000, 600, 300, 150, 100
};

// PowerSDR settings
#define DSP_BW_NUM_POWERSDR 12

static int dsp_bw_ssb_powersdr[DSP_BW_NUM_POWERSDR] =
{
    5000, 4400, 3800, 3300, 2900, 2700, 2400, 2100, 1800, 1000, 0, 0
};

static int dsp_bw_am_powersdr[DSP_BW_NUM_POWERSDR] =
{
    16000, 12000, 10000, 8000, 6600, 5200, 4000, 3100, 2900, 2400, 0, 0
};

static int dsp_bw_cw_powersdr[DSP_BW_NUM_POWERSDR] =
{
    1000, 800, 600, 500, 400, 250, 150, 100, 50, 25, 0, 0
};

static int dsp_bw_dig_powersdr[DSP_BW_NUM_POWERSDR] =
{
    3000, 2500, 2000, 1500, 1000, 800, 600, 300, 150, 75, 0, 0
};

#if 0 // not used yet
static int dsp_bw_sam_powersdr[DSP_BW_NUM_POWERSDR] =
{
    20000, 18000, 16000, 12000, 10000, 9000, 8000, 7000, 6000, 5000, 0, 0
};
#endif


/* Private helper functions */

static int flex6k_get_mode(RIG *rig, vfo_t vfo, rmode_t *mode, pbwidth_t *width)
{
    struct kenwood_priv_caps *caps = kenwood_caps(rig);
    char modebuf[10];
    int index;
    int retval;

    rig_debug(RIG_DEBUG_VERBOSE, "%s called\n", __func__);

    if (!mode || !width)
    {
        return -RIG_EINVAL;
    }

    retval = kenwood_safe_transaction(rig, "MD", modebuf, 6, 3);

    if (retval != RIG_OK)
    {
        return retval;
    }

    *mode = kenwood2rmode(modebuf[2] - '0', caps->mode_table);

    if ((vfo == RIG_VFO_VFO) || (vfo == RIG_VFO_CURR))
    {
        vfo = rig->state.current_vfo;
        rig_debug(RIG_DEBUG_VERBOSE, "%s: setting VFO to current\n", __func__);
    }

    /*
     * The Flex CAT interface does not support FW for reading filter width,
     * so use the ZZFI
     * Have to determine what to do with receiver#2 if anybody ever asks
     */
    switch (vfo)
    {
    case RIG_VFO_A:
        retval = kenwood_safe_transaction(rig, "ZZFI", modebuf, 10, 6);
        break;

    case RIG_VFO_B:
        retval = kenwood_safe_transaction(rig, "ZZFJ", modebuf, 10, 6);
        break;

    default:
        rig_debug(RIG_DEBUG_ERR, "%s: unsupported VFO %s\n", __func__, rig_strvfo(vfo));
        return -RIG_EINVAL;
    }

    if (retval != RIG_OK)
    {
        return retval;
    }

    index = atoi(&modebuf[4]);

    if (index >= DSP_BW_NUM)
    {
        rig_debug(RIG_DEBUG_ERR,
                  "%s: unexpected ZZF[IJ] answer, index=%d\n", __func__, index);
        return -RIG_ERJCTED;
    }

    switch (*mode)
    {
    case RIG_MODE_AM:
    case RIG_MODE_DSB:
        *width = dsp_bw_am[index];
        break;

    case RIG_MODE_CW:
        *width = dsp_bw_cw[index];
        break;

    case RIG_MODE_USB:
    case RIG_MODE_LSB:
        *width = dsp_bw_ssb[index];
        break;

    //case RIG_MODE_FM:
    //*width = 3000; /* not supported yet, needs followup */
    //break;
    case RIG_MODE_PKTLSB:
    case RIG_MODE_PKTUSB:
        *width = dsp_bw_dig[index];
        break;

    default:
        rig_debug(RIG_DEBUG_ERR, "%s: unsupported mode %s, setting default BW\n",
                  __func__, rig_strrmode(*mode));
        *width = 3000;
        break;
    }

    return RIG_OK;
}

static int powersdr_get_mode(RIG *rig, vfo_t vfo, rmode_t *mode,
                             pbwidth_t *width)
{
    struct kenwood_priv_caps *caps = kenwood_caps(rig);
    char modebuf[10];
    int retval;

    rig_debug(RIG_DEBUG_VERBOSE, "%s called\n", __func__);

    if (!mode || !width)
    {
        return -RIG_EINVAL;
    }

    retval = kenwood_safe_transaction(rig, "ZZMD", modebuf, 10, 6);

    if (retval != RIG_OK)
    {
        return retval;
    }

    *mode = kenwood2rmode(atoi(&modebuf[4]), caps->mode_table);

    if ((vfo == RIG_VFO_VFO) || (vfo == RIG_VFO_CURR))
    {
        vfo = rig->state.current_vfo;
        rig_debug(RIG_DEBUG_VERBOSE, "%s: setting VFO to current\n", __func__);
    }

    /*
     * The Flex CAT interface does not support FW for reading filter width,
     * so use the ZZFI or ZZFJ command
     */
    switch (vfo)
    {
        int lo, hi;

    case RIG_VFO_A:
    case RIG_VFO_B:
        retval = kenwood_safe_transaction(rig, "ZZFL", modebuf, 10, 9);

        if (retval != RIG_OK)
        {
            return retval;
        }

        lo = atoi(&modebuf[4]);
        retval = kenwood_safe_transaction(rig, "ZZFH", modebuf, 10, 9);

        if (retval != RIG_OK)
        {
            return retval;
        }

        hi = atoi(&modebuf[4]);
        rig_debug(RIG_DEBUG_VERBOSE, "%s: lo=%d, hi=%d\n", __func__, lo, hi);
        *width = hi - lo;
        break;

    default:
        rig_debug(RIG_DEBUG_ERR, "%s: unsupported VFO %s\n", __func__, rig_strvfo(vfo));
        return -RIG_EINVAL;
    }

    return RIG_OK;
}

/* Private helper functions */

static int flex6k_find_width(rmode_t mode, pbwidth_t width, int *ridx)
{
    int *w_a; // Width array, these are all ordered in descending order!
    int idx = 0;

    rig_debug(RIG_DEBUG_VERBOSE, "%s called\n", __func__);

    switch (mode)
    {
    case RIG_MODE_AM:
        w_a = dsp_bw_am;
        break;

    case RIG_MODE_CW:
        w_a = dsp_bw_cw;
        break;

    case RIG_MODE_USB:
    case RIG_MODE_LSB:
        w_a = dsp_bw_ssb;
        break;

    //case RIG_MODE_FM:
    //*width = 3000; /* not supported yet, needs followup */
    //break;
    case RIG_MODE_PKTLSB:
    case RIG_MODE_PKTUSB:
        w_a = dsp_bw_dig;
        break;

    default:
        rig_debug(RIG_DEBUG_ERR, "%s: unsupported mode %s\n", __func__,
                  rig_strrmode(mode));
        return -RIG_EINVAL;
    }

    // return the first smaller or equal possibility
    while ((idx < DSP_BW_NUM) && (w_a[idx] > width))
    {
        idx++;
    }

    if (idx >= DSP_BW_NUM)
    {
        idx = DSP_BW_NUM - 1;
    }

    *ridx = idx;
    return RIG_OK;
}

static int powersdr_find_width(rmode_t mode, pbwidth_t width, int *ridx)
{
    int *w_a; // Width array, these are all ordered in descending order!
    int idx = 0;

    rig_debug(RIG_DEBUG_VERBOSE, "%s called\n", __func__);

    switch (mode)
    {
    case RIG_MODE_AM:
        w_a = dsp_bw_am_powersdr;
        break;

    case RIG_MODE_CW:
    case RIG_MODE_CWR:
        w_a = dsp_bw_cw_powersdr;
        break;

    case RIG_MODE_USB:
    case RIG_MODE_LSB:
        w_a = dsp_bw_ssb_powersdr;
        break;

    //case RIG_MODE_FM:
    //*width = 3000; /* not supported yet, needs followup */
    //break;
    case RIG_MODE_PKTLSB:
    case RIG_MODE_PKTUSB:
        w_a = dsp_bw_dig_powersdr;
        break;

    default:
        rig_debug(RIG_DEBUG_ERR, "%s: unsupported mode %s\n", __func__,
                  rig_strrmode(mode));
        return -RIG_EINVAL;
    }

    // return the first smaller or equal possibility
    while ((idx < DSP_BW_NUM) && (w_a[idx] > width))
    {
        idx++;
    }

    if (idx >= DSP_BW_NUM)
    {
        idx = DSP_BW_NUM - 1;
    }

    *ridx = idx;
    return RIG_OK;
}

static int flex6k_set_mode(RIG *rig, vfo_t vfo, rmode_t mode, pbwidth_t width)
{
    struct kenwood_priv_caps *caps = kenwood_caps(rig);
    char buf[10];
    char kmode;
    int idx;
    int err;

    rig_debug(RIG_DEBUG_VERBOSE, "%s called\n", __func__);

    kmode = rmode2kenwood(mode, caps->mode_table);

    if (kmode < 0)
    {
        rig_debug(RIG_DEBUG_WARN, "%s: unsupported mode '%s'\n",
                  __func__, rig_strrmode(mode));
        return -RIG_EINVAL;
    }

    SNPRINTF(buf, sizeof(buf), "MD%c", '0' + kmode);
    err = kenwood_transaction(rig, buf, NULL, 0);

    if (err != RIG_OK)
    {
        return err;
    }

    if ((vfo == RIG_VFO_VFO) || (vfo == RIG_VFO_CURR))
    {
        vfo = rig->state.current_vfo;
        rig_debug(RIG_DEBUG_VERBOSE, "%s: setting VFO to current\n", __func__);
    }

    if (RIG_PASSBAND_NOCHANGE == width) { return err; }

    err = flex6k_find_width(mode, width, &idx);

    if (err != RIG_OK)
    {
        return err;
    }

    /*
     * The Flex CAT interface does not support FW for reading filter width,
     * so use the ZZFI or ZZFJ command
     */
    switch (vfo)
    {
    case RIG_VFO_A:
        SNPRINTF(buf, sizeof(buf), "ZZFI%02d;", idx);
        break;

    case RIG_VFO_B:
        SNPRINTF(buf, sizeof(buf), "ZZFJ%02d;", idx);
        break;

    default:
        rig_debug(RIG_DEBUG_ERR, "%s: unsupported VFO %s\n", __func__, rig_strvfo(vfo));
        return -RIG_EINVAL;
    }

    err = kenwood_transaction(rig, buf, NULL, 0);

    if (err != RIG_OK)
    {
        return err;
    }

    return RIG_OK;
}

static int powersdr_set_mode(RIG *rig, vfo_t vfo, rmode_t mode, pbwidth_t width)
{
    struct kenwood_priv_caps *caps = kenwood_caps(rig);
    char buf[64];
    char kmode;
    int idx;
    int err;

    rig_debug(RIG_DEBUG_VERBOSE, "%s called mode=%s, width=%d\n", __func__,
              rig_strrmode(mode), (int)width);

    kmode = rmode2kenwood(mode, caps->mode_table);

    if (kmode < 0)
    {
        rig_debug(RIG_DEBUG_WARN, "%s: unsupported mode '%s'\n",
                  __func__, rig_strrmode(mode));
        return -RIG_EINVAL;
    }

    SNPRINTF(buf, sizeof(buf), "ZZMD%02d", kmode);
    err = kenwood_transaction(rig, buf, NULL, 0);

    if (err != RIG_OK)
    {
        return err;
    }

    if ((vfo == RIG_VFO_VFO) || (vfo == RIG_VFO_CURR))
    {
        vfo = rig->state.current_vfo;
        rig_debug(RIG_DEBUG_VERBOSE, "%s: setting VFO to current\n", __func__);
    }

    if (RIG_PASSBAND_NOCHANGE == width) { return err; }

    err = powersdr_find_width(mode, width, &idx);

    if (err != RIG_OK)
    {
        return err;
    }

    /*
     * The Flex CAT interface does not support FW for reading filter width,
     * so use the ZZFI or ZZFJ command
     */
    switch (vfo)
    {
    case RIG_VFO_B:
    case RIG_VFO_A:
        if ((mode == RIG_MODE_PKTUSB || mode == RIG_MODE_PKTLSB) && width > 3000)
        {
            // 150Hz on the low end should be enough
            // Set high to the width requested
            SNPRINTF(buf, sizeof(buf), "ZZFL00150;ZZFH%05d;", (int)width);
        }
        else
        {
            SNPRINTF(buf, sizeof(buf), "ZZFI%02d;", idx);
        }

        break;

    // what do we do about RX2 ??

    default:
        rig_debug(RIG_DEBUG_ERR, "%s: unsupported VFO %s\n", __func__, rig_strvfo(vfo));
        return -RIG_EINVAL;
    }

    err = kenwood_transaction(rig, buf, NULL, 0);

    if (err != RIG_OK)
    {
        return err;
    }

    return RIG_OK;
}
/*
 * flex6k_get_ptt
 */
int flex6k_get_ptt(RIG *rig, vfo_t vfo, ptt_t *ptt)
{
    const char *ptt_cmd;
    int err;
    char response[16] = "";

    rig_debug(RIG_DEBUG_VERBOSE, "%s called\n", __func__);


    if (!ptt)
    {
        return -RIG_EINVAL;
    }

    ptt_cmd = "ZZTX";

    err =  kenwood_transaction(rig, ptt_cmd, response, sizeof(response));

    if (err != RIG_OK)
    {
        return err;
    }

    *ptt = response[4] == '0' ? RIG_PTT_OFF : RIG_PTT_ON;

    return RIG_OK;
}

int flex6k_set_ptt(RIG *rig, vfo_t vfo, ptt_t ptt)
{
    const char *ptt_cmd;
    char response[16] = "";
    int err;
    int retry = 3;

    rig_debug(RIG_DEBUG_VERBOSE, "%s called\n", __func__);

    switch (ptt)
    {
    case RIG_PTT_ON_DATA:
    case RIG_PTT_ON_MIC:
    case RIG_PTT_ON:  ptt_cmd = "ZZTX1;ZZTX"; break;

    case RIG_PTT_OFF: ptt_cmd = "ZZTX0;ZZTX"; break;

    default: return -RIG_EINVAL;
    }

    do
    {
        err =  kenwood_transaction(rig, ptt_cmd, response, sizeof(response));

        if (ptt_cmd[4] != response[4])
        {
            rig_debug(RIG_DEBUG_ERR, "%s: %s != %s\n", __func__, ptt_cmd, response);
            hl_usleep(20 * 1000); // takes a bit to do PTT off
        }
    }
    while (ptt_cmd[4] != response[4] && --retry);

    return err;
}

/*
 * powersdr_set_level
 */
int powersdr_set_level(RIG *rig, vfo_t vfo, setting_t level, value_t val)
{
    char cmd[KENWOOD_MAX_BUF_LEN];
    int retval;
    int ival;
    rmode_t mode;
    pbwidth_t width;

    rig_debug(RIG_DEBUG_VERBOSE, "%s called\n", __func__);

    switch (level)
    {
    case RIG_LEVEL_AF:
        if (val.f > 1.0) { return -RIG_EINVAL; }

        ival = val.f * 100;
        SNPRINTF(cmd, sizeof(cmd) - 1, "ZZAG%03d", ival);
        break;

    case RIG_LEVEL_IF:
        SNPRINTF(cmd, sizeof(cmd) - 1, "ZZIT%+05d", val.i);
        break;

    case RIG_LEVEL_RF:
        if (val.f > 1.0) { return -RIG_EINVAL; }

        ival = val.f * (120 - -20) - 20;
        SNPRINTF(cmd, sizeof(cmd) - 1, "ZZAR%+04d", ival);
        break;

    case RIG_LEVEL_MICGAIN:
        if (val.f > 1.0) { return -RIG_EINVAL; }

        ival = val.f * (10 - -40) - 40;
        SNPRINTF(cmd, sizeof(cmd) - 1, "ZZMG%03d", ival);
        break;

    case RIG_LEVEL_AGC:
        if (val.i > 5)
        {
            val.i = 5;    /* 0.. 255 */
        }

        SNPRINTF(cmd, sizeof(cmd), "GT%03d", (int)val.i);
        break;

    case RIG_LEVEL_VOXGAIN:
        if (val.f > 1.0) { return -RIG_EINVAL; }

        ival = val.f * 1000;
        SNPRINTF(cmd, sizeof(cmd) - 1, "ZZVG%04d", ival);
        break;

    case RIG_LEVEL_SQL:
        if (val.f > 1.0) { return -RIG_EINVAL; }

        powersdr_get_mode(rig, vfo, &mode, &width);

        if (mode == RIG_MODE_FM)
        {
            ival = val.f * 100; // FM mode is 0 to 100
        }
        else
        {
            ival = 160 - (val.f * 160); // all other modes  0 to 160
        }

        SNPRINTF(cmd, sizeof(cmd) - 1, "ZZSQ%03d", ival);
        break;

    default:
        return kenwood_set_level(rig, vfo, level, val);
    }

    retval = kenwood_transaction(rig, cmd, NULL, 0);

    if (retval != RIG_OK)
    {
        return retval;
    }


    rig_debug(RIG_DEBUG_VERBOSE, "%s exiting\n", __func__);

    return RIG_OK;
}

/*
 * powersdr_get_level
 */
int powersdr_get_level(RIG *rig, vfo_t vfo, setting_t level, value_t *val)
{
    char lvlbuf[KENWOOD_MAX_BUF_LEN];
    char *cmd;
    int retval;
    int len, ans;
    rmode_t mode;
    pbwidth_t width;
    ptt_t ptt;
    double dval;

    rig_debug(RIG_DEBUG_VERBOSE, "%s called\n", __func__);

    if (!val)
    {
        return -RIG_EINVAL;
    }

    switch (level)
    {
    case RIG_LEVEL_AGC:
        cmd = "GT";
        len = 2;
        ans = 3;
        break;

    case RIG_LEVEL_AF:
        cmd = "ZZAG";
        len = 4;
        ans = 3;
        break;

    case RIG_LEVEL_IF:
        cmd = "ZZIT";
        len = 4;
        ans = 5;
        break;

    case RIG_LEVEL_RF:
        cmd = "ZZAR";
        len = 4;
        ans = 4;
        break;

    case RIG_LEVEL_STRENGTH:
        flex6k_get_ptt(rig, vfo, &ptt);

        if (ptt) // not applicable if transmitting
        {
            val->f = 0;
            return RIG_OK;
        }

        cmd = "ZZRM0";
        len = 5;
        ans = 9;
        break;

    case RIG_LEVEL_RFPOWER:
        cmd = "ZZPC";
        len = 4;
        ans = 8;
        break;

    case RIG_LEVEL_RFPOWER_METER:
    case RIG_LEVEL_RFPOWER_METER_WATTS:
        flex6k_get_ptt(rig, vfo, &ptt);

        if (!ptt)
        {
            val->f = 0;
            return RIG_OK;
        }

        cmd = "ZZRM5";
        len = 5;
        ans = 3;
        break;

    case RIG_LEVEL_MICGAIN:
        cmd = "ZZMG";
        len = 4;
        ans = 3;
        break;

    case RIG_LEVEL_VOXGAIN:
        cmd = "ZZVG";
        len = 4;
        ans = 4;
        break;

    case RIG_LEVEL_SQL:
        cmd = "ZZSQ";
        len = 4;
        ans = 3;
        break;

    default:
        return kenwood_get_level(rig, vfo, level, val);
    }

    retval = kenwood_safe_transaction(rig, cmd, lvlbuf, sizeof(lvlbuf), len + ans);

    if (retval != RIG_OK)
    {
        return retval;
    }

    int n;

    switch (level)
    {
    case RIG_LEVEL_AGC:
        n = sscanf(lvlbuf + len, "%d", &val->i);

        if (n != 1)
        {
            rig_debug(RIG_DEBUG_ERR, "%s: Error parsing value from lvlbuf='%s'\n",
                      __func__, lvlbuf);
            return -RIG_EPROTO;
        }

        break;

    case RIG_LEVEL_IF:
        n = sscanf(lvlbuf + len, "%d", &val->i);

        if (n != 1)
        {
            rig_debug(RIG_DEBUG_ERR, "%s: Error parsing value from lvlbuf='%s'\n",
                      __func__, lvlbuf);
            return -RIG_EPROTO;
        }

        break;

    case RIG_LEVEL_STRENGTH:
        n = sscanf(lvlbuf, "ZZRM0%lf", &dval);

        if (n != 1)
        {
            rig_debug(RIG_DEBUG_ERR, "%s: Error parsing value from lvlbuf='%s'\n",
                      __func__, lvlbuf);
            val->i = 0;
            return -RIG_EPROTO;
        }

        val->i = dval + 73; // dbm to S9-based=0dB
        break;


    case RIG_LEVEL_AF:
        n = sscanf(lvlbuf, "ZZAG%f", &val->f);

        if (n != 1)
        {
            rig_debug(RIG_DEBUG_ERR, "%s: Error parsing value from lvlbuf='%s'\n",
                      __func__, lvlbuf);
            val->f = 0;
            return -RIG_EPROTO;
        }

        val->f /= 100.0;
        break;

    case RIG_LEVEL_RFPOWER:
        n = sscanf(lvlbuf, "ZZPC%f", &val->f);

        if (n != 1)
        {
            rig_debug(RIG_DEBUG_ERR, "%s: Error parsing value from lvlbuf='%s'\n",
                      __func__, lvlbuf);
            val->f = 0;
            return -RIG_EPROTO;
        }

        val->f /= 100;
        break;

    case RIG_LEVEL_RFPOWER_METER:
    case RIG_LEVEL_RFPOWER_METER_WATTS:
        n = sscanf(lvlbuf, "ZZRM5%f", &val->f);

        if (n != 1)
        {
            rig_debug(RIG_DEBUG_ERR, "%s: Error parsing value from lvlbuf='%s'\n",
                      __func__, lvlbuf);
            val->f = 0;
            return -RIG_EPROTO;
        }

        if (level != RIG_LEVEL_RFPOWER_METER_WATTS)
        {
            val->f /= 100;
        }

        break;

    case RIG_LEVEL_RF:
        n = sscanf(lvlbuf + len, "%d", &val->i);

        if (n != 1)
        {
            rig_debug(RIG_DEBUG_ERR, "%s: Error parsing value from lvlbuf='%s'\n",
                      __func__, lvlbuf);
            return -RIG_EPROTO;
        }

        n = val->i;
        val->f = (n + 20.0) / (120.0 - -20.0);

        break;

    case RIG_LEVEL_MICGAIN:
        n = sscanf(lvlbuf + len, "%f", &val->f);

        if (n != 1)
        {
            rig_debug(RIG_DEBUG_ERR, "%s: Error parsing value from lvlbuf='%s'\n",
                      __func__, lvlbuf);
            return -RIG_EPROTO;
        }

        // Thetis returns -40 to 10 -- does PowerSDR do the same?
        // Setting
        val->f = (val->f - -40) / (10 - -40);
        break;

    case RIG_LEVEL_VOXGAIN:
        // return is 0-1000
        n = sscanf(lvlbuf + len, "%f", &val->f);

        if (n != 1)
        {
            rig_debug(RIG_DEBUG_ERR, "%s: Error parsing value from lvlbuf='%s'\n",
                      __func__, lvlbuf);
            return -RIG_EPROTO;
        }

        val->f /= 1000;
        break;

    case RIG_LEVEL_SQL:
        n = sscanf(lvlbuf + len, "%f", &val->f);

        if (n != 1)
        {
            rig_debug(RIG_DEBUG_ERR, "%s: Error parsing value from lvlbuf='%s'\n",
                      __func__, lvlbuf);
            return -RIG_EPROTO;
        }


        powersdr_get_mode(rig, vfo, &mode, &width);

        if (mode == RIG_MODE_FM)
        {
            val->f /= 100; // FM mode is 0 to 100
        }
        else
        {
            val->f = fabs((val->f - 160) / -160); // all other modes  0 to 160
        }

        break;

    default:
        rig_debug(RIG_DEBUG_ERR, "%s: should never get here\n", __func__);
    }

    return RIG_OK;
}

int powersdr_set_func(RIG *rig, vfo_t vfo, setting_t func, int status)
{
    char cmd[KENWOOD_MAX_BUF_LEN];

    rig_debug(RIG_DEBUG_VERBOSE, "%s called\n", __func__);

    switch (func)
    {
    case RIG_FUNC_MUTE:
        SNPRINTF(cmd, sizeof(cmd) - 1, "ZZMA%01d", status);
        break;

    case RIG_FUNC_VOX:
        SNPRINTF(cmd, sizeof(cmd) - 1, "ZZVE%01d", status);
        break;

    case RIG_FUNC_SQL:
        SNPRINTF(cmd, sizeof(cmd) - 1, "ZZSO%01d", status);
        break;

    case RIG_FUNC_TUNER:
        SNPRINTF(cmd, sizeof(cmd) - 1, "ZZTU%01d", status);
        break;

    default:
        return kenwood_set_func(rig, vfo, func, status);
    }

    return kenwood_transaction(rig, cmd, NULL, 0);
}

int powersdr_get_func(RIG *rig, vfo_t vfo, setting_t func, int *status)
{
    char lvlbuf[KENWOOD_MAX_BUF_LEN];
    char *cmd;
    int retval;
    int len, ans;

    rig_debug(RIG_DEBUG_VERBOSE, "%s called\n", __func__);

    if (!status)
    {
        return -RIG_EINVAL;
    }

    switch (func)
    {
    case RIG_FUNC_MUTE:
        cmd = "ZZMA";
        len = 4;
        ans = 1;
        break;

    case RIG_FUNC_VOX:
        cmd = "ZZVE";
        len = 4;
        ans = 1;
        break;

    case RIG_FUNC_SQL:
        cmd = "ZZSO";
        len = 4;
        ans = 1;
        break;

    default:
        return kenwood_get_func(rig, vfo, func, status);
    }

    retval = kenwood_safe_transaction(rig, cmd, lvlbuf, 10, len + ans);

    if (retval != RIG_OK)
    {
        return retval;
    }

    switch (func)
    {
    case RIG_FUNC_MUTE:
    case RIG_FUNC_VOX:
    case RIG_FUNC_SQL:
        sscanf(lvlbuf + len, "%d", status);
        break;

    default:
        rig_debug(RIG_DEBUG_ERR, "%s: should never get here\n", __func__);
    }

    return RIG_OK;
}

/*
 * F6K rig capabilities.
 */
const struct rig_caps f6k_caps =
{
    RIG_MODEL(RIG_MODEL_F6K),
    .model_name =       "6xxx",
    .mfg_name =     "FlexRadio",
    .version =      "20220306.0",
    .copyright =        "LGPL",
    .status =       RIG_STATUS_STABLE,
    .rig_type =     RIG_TYPE_TRANSCEIVER,
    .ptt_type =     RIG_PTT_RIG,
    .dcd_type =     RIG_DCD_NONE,
    .port_type =        RIG_PORT_NETWORK,
    // The combination of timeout and retry is important
    // We need at least 3 seconds to do profile switches
    // Hitting the timeout is OK as long as we retry
    // Previous note showed FA/FB may take up to 500ms on band change
    .timeout =      700,
    .retry =        13,

    .has_get_func =     RIG_FUNC_NONE, /* has VOX but not implemented here */
    .has_set_func =     RIG_FUNC_NONE,
    .has_get_level =    F6K_LEVEL_ALL,
    .has_set_level =    F6K_LEVEL_ALL,
    .has_get_parm =     RIG_PARM_NONE,
    .has_set_parm =     RIG_PARM_NONE,  /* FIXME: parms */
    .level_gran =       {},     /* FIXME: granularity */
    .parm_gran =        {},
    //.extlevels =      elecraft_ext_levels,
    //.extparms =       kenwood_cfg_params,
    .post_write_delay = 20,
    .preamp =       { RIG_DBLST_END, },
    .attenuator =       { RIG_DBLST_END, },
    .max_rit =      Hz(99999),
    .max_xit =      Hz(99999),
    .max_ifshift =      Hz(0),
    .vfo_ops =      RIG_OP_NONE,
    .targetable_vfo =   RIG_TARGETABLE_FREQ | RIG_TARGETABLE_MODE,
    .transceive =       RIG_TRN_RIG,
    .bank_qty =     0,
    .chan_desc_sz =     0,

    .chan_list =        { RIG_CHAN_END },

    .rx_range_list1 =  {
        {kHz(30), MHz(77), F6K_MODES, -1, -1, F6K_VFO, F6K_ANTS},
        {MHz(135), MHz(165), F6K_MODES, -1, - 1, F6K_VFO, F6K_ANTS},
        RIG_FRNG_END,
    }, /* rx range */
    .tx_range_list1 =  {
        FRQ_RNG_HF(1, F6K_MODES, mW(10), W(100), F6K_VFO, F6K_ANTS),
        FRQ_RNG_6m(1, F6K_MODES, mW(10), W(100), F6K_VFO, F6K_ANTS),
        FRQ_RNG_2m(1, F6K_MODES, mW(10), W(100), F6K_VFO, F6K_ANTS),
        RIG_FRNG_END,
    }, /* tx range */

    .rx_range_list2 =  {
        {kHz(30), MHz(77), F6K_MODES, -1, -1, F6K_VFO, F6K_ANTS},
        { MHz(135), MHz(165), F6K_MODES, -1, -1, F6K_VFO, F6K_ANTS},
        RIG_FRNG_END,
    }, /* rx range */
    .tx_range_list2 =  {
        FRQ_RNG_HF(2, F6K_MODES, mW(10), W(100), F6K_VFO, F6K_ANTS),
        FRQ_RNG_6m(2, F6K_MODES, mW(10), W(100), F6K_VFO, F6K_ANTS),
        FRQ_RNG_2m(2, F6K_MODES, mW(10), W(100), F6K_VFO, F6K_ANTS),
        RIG_FRNG_END,
    }, /* tx range */

    .tuning_steps =  {
        {F6K_MODES, 1},
        RIG_TS_END,
    },

    /* mode/filter list, remember: order matters! */
    .filters =  {
        {RIG_MODE_SSB, kHz(2.7)},
        {RIG_MODE_SSB, kHz(3.3)},
        {RIG_MODE_SSB, kHz(1.8)},
        {RIG_MODE_SSB, kHz(1.6)},
        {RIG_MODE_SSB, kHz(4.0)},
        {RIG_MODE_SSB, RIG_FLT_ANY},
        {RIG_MODE_CW, kHz(0.4)},
        {RIG_MODE_CW, kHz(1.5)},
        {RIG_MODE_CW, Hz(50)},
        {RIG_MODE_CW, kHz(3.0)},
        {RIG_MODE_CW, RIG_FLT_ANY},
        {RIG_MODE_PKTUSB | RIG_MODE_PKTLSB, kHz(1.5)},
        {RIG_MODE_PKTUSB | RIG_MODE_PKTLSB, kHz(3.0)},
        {RIG_MODE_PKTUSB | RIG_MODE_PKTLSB, kHz(0.1)},
        {RIG_MODE_PKTUSB | RIG_MODE_PKTLSB, RIG_FLT_ANY},
        {RIG_MODE_AM, kHz(6)},
        {RIG_MODE_AM, kHz(14)},
        {RIG_MODE_AM, kHz(5.6)},
        {RIG_MODE_AM, kHz(20.0)},
        {RIG_MODE_AM, RIG_FLT_ANY},
        {RIG_MODE_FM, kHz(13)}, /* TBC */
        RIG_FLT_END,
    },
    .priv = (void *)& f6k_priv_caps,

    .rig_init =     kenwood_init,
    .rig_cleanup =      kenwood_cleanup,
    .rig_open =     flexradio_open,
    .rig_close =        kenwood_close,
    .set_freq =     kenwood_set_freq,
    .get_freq =     kenwood_get_freq,
    .set_rit =  kenwood_set_rit,
    .get_rit =  kenwood_get_rit,
    .set_xit =  kenwood_set_xit,
    .get_xit =  kenwood_get_xit,
    .set_mode =     flex6k_set_mode,
    .get_mode =     flex6k_get_mode,
    .set_vfo =      kenwood_set_vfo,
    .get_vfo =      kenwood_get_vfo_if,
    .set_split_vfo =    kenwood_set_split_vfo,
    .get_split_vfo =    kenwood_get_split_vfo_if,
    .get_ptt =      kenwood_get_ptt,
    .set_ptt =      kenwood_set_ptt,
    // TODO copy over kenwood_[set|get]_level and modify to handle DSP filter values
    // correctly - use actual values instead of indices
    .set_level =        kenwood_set_level,
    .get_level =        kenwood_get_level,
    //.set_ant =       kenwood_set_ant_no_ack,
    //.get_ant =       kenwood_get_ant,
    .hamlib_check_rig_caps = HAMLIB_CHECK_RIG_CAPS
};

/*
 * PowerSDR rig capabilities.
 */
const struct rig_caps powersdr_caps =
{
    RIG_MODEL(RIG_MODEL_POWERSDR),
    .model_name =       "PowerSDR/Thetis",
    .mfg_name =     "FlexRadio/ANAN",
    .version =      "20221123.0",
    .copyright =        "LGPL",
    .status =       RIG_STATUS_STABLE,
    .rig_type =     RIG_TYPE_TRANSCEIVER,
    .ptt_type =     RIG_PTT_RIG,
    .dcd_type =     RIG_DCD_NONE,
    .port_type =        RIG_PORT_SERIAL,
    .serial_rate_min =  300,
    .serial_rate_max =  115200,
    .serial_data_bits =  8,
    .serial_stop_bits =  1,
    .serial_parity =  RIG_PARITY_NONE,
    .serial_handshake =  RIG_HANDSHAKE_NONE,
    .write_delay =  0,
    .post_write_delay = 0,
    // The combination of timeout and retry is important
    // We need at least 3 seconds to do profile switches
    // Hitting the timeout is OK as long as we retry
    // Previous note showed FA/FB may take up to 500ms on band change
    // Flex 1500 needs about 6 seconds for a band change in PowerSDR
    .timeout =      800, // some band transitions can take 600ms
    .retry =        10,

    .has_get_func =     POWERSDR_FUNC_ALL,
    .has_set_func =     POWERSDR_FUNC_ALL,
    .has_get_level =    POWERSDR_LEVEL_ALL,
    .has_set_level =    POWERSDR_LEVEL_SET,
    .has_get_parm =     RIG_PARM_NONE,
    .has_set_parm =     RIG_PARM_NONE,  /* FIXME: parms */
    .level_gran =       {},     /* FIXME: granularity */
    .parm_gran =        {},
    //.extlevels =      elecraft_ext_levels,
    //.extparms =       kenwood_cfg_params,
    .preamp =       { RIG_DBLST_END, },
    .attenuator =       { RIG_DBLST_END, },
    .max_rit =      Hz(0),
    .max_xit =      Hz(0),
    .max_ifshift =      Hz(0),
    .vfo_op =      kenwood_vfo_op,
    .vfo_ops =      POWERSDR_VFO_OP,
    .targetable_vfo =   RIG_TARGETABLE_FREQ | RIG_TARGETABLE_MODE,
    .transceive =       RIG_TRN_RIG,
    .agc_level_count = 6,
    .agc_levels = { RIG_AGC_OFF, RIG_AGC_LONG, RIG_AGC_SLOW, RIG_AGC_MEDIUM, RIG_AGC_FAST, RIG_AGC_USER },
    .bank_qty =     0,
    .chan_desc_sz =     0,

    .chan_list =        { RIG_CHAN_END },

    .rx_range_list1 =  {
        {kHz(30), MHz(77), POWERSDR_MODES, -1, -1, F6K_VFO, F6K_ANTS},
        {MHz(135), MHz(165), POWERSDR_MODES, -1, - 1, F6K_VFO, F6K_ANTS},
        RIG_FRNG_END,
    }, /* rx range */
    .tx_range_list1 =  {
        FRQ_RNG_HF(1, POWERSDR_MODES, mW(10), W(100), F6K_VFO, F6K_ANTS),
        FRQ_RNG_6m(1, POWERSDR_MODES, mW(10), W(100), F6K_VFO, F6K_ANTS),
        FRQ_RNG_2m(1, POWERSDR_MODES, mW(10), W(100), F6K_VFO, F6K_ANTS),
        RIG_FRNG_END,
    }, /* tx range */

    .rx_range_list2 =  {
        {kHz(30), MHz(77), POWERSDR_MODES, -1, -1, F6K_VFO, F6K_ANTS},
        { MHz(135), MHz(165), POWERSDR_MODES, -1, -1, F6K_VFO, F6K_ANTS},
        RIG_FRNG_END,
    }, /* rx range */
    .tx_range_list2 =  {
        FRQ_RNG_HF(2, POWERSDR_MODES, mW(10), W(100), F6K_VFO, F6K_ANTS),
        FRQ_RNG_6m(2, POWERSDR_MODES, mW(10), W(100), F6K_VFO, F6K_ANTS),
        FRQ_RNG_2m(2, POWERSDR_MODES, mW(10), W(100), F6K_VFO, F6K_ANTS),
        RIG_FRNG_END,
    }, /* tx range */

    .tuning_steps =  {
        {POWERSDR_MODES, 1},
        RIG_TS_END,
    },

    /* mode/filter list, remember: order matters! */
    .filters =  {
        {RIG_MODE_SSB, kHz(2.7)},
        {RIG_MODE_SSB, kHz(3.3)},
        {RIG_MODE_SSB, kHz(1.8)},
        {RIG_MODE_SSB, kHz(1.6)},
        {RIG_MODE_SSB, kHz(4.0)},
        {RIG_MODE_SSB, RIG_FLT_ANY},
        {RIG_MODE_CW, kHz(0.4)},
        {RIG_MODE_CW, kHz(1.5)},
        {RIG_MODE_CW, Hz(50)},
        {RIG_MODE_CW, kHz(3.0)},
        {RIG_MODE_CW, RIG_FLT_ANY},
        {RIG_MODE_PKTUSB | RIG_MODE_PKTLSB, kHz(1.5)},
        {RIG_MODE_PKTUSB | RIG_MODE_PKTLSB, kHz(3.0)},
        {RIG_MODE_PKTUSB | RIG_MODE_PKTLSB, kHz(0.1)},
        {RIG_MODE_PKTUSB | RIG_MODE_PKTLSB, RIG_FLT_ANY},
        {RIG_MODE_AM, kHz(6)},
        {RIG_MODE_AM, kHz(14)},
        {RIG_MODE_AM, kHz(5.6)},
        {RIG_MODE_AM, kHz(20.0)},
        {RIG_MODE_AM, RIG_FLT_ANY},
        {RIG_MODE_FM, kHz(13)}, /* TBC */
        RIG_FLT_END,
    },
    .priv = (void *)& powersdr_priv_caps,

    .rig_init =     kenwood_init,
    .rig_cleanup =      kenwood_cleanup,
    .rig_open =     flexradio_open,
    .rig_close =        kenwood_close,
    .set_freq =     kenwood_set_freq,
    .get_freq =     kenwood_get_freq,
    .set_mode =     powersdr_set_mode,
    .get_mode =     powersdr_get_mode,
    .set_vfo =      kenwood_set_vfo,
    .get_vfo =      kenwood_get_vfo_if,
    .set_split_vfo =    kenwood_set_split_vfo,
    .get_split_vfo =    kenwood_get_split_vfo_if,
    .get_ptt =      flex6k_get_ptt,
    .set_ptt =      flex6k_set_ptt,
    .get_powerstat = kenwood_get_powerstat,
    .set_powerstat = kenwood_set_powerstat,
    // TODO copy over kenwood_[set|get]_level and modify to handle DSP filter values
    // correctly - use actual values instead of indices
    .set_level =        powersdr_set_level,
    .get_level =        powersdr_get_level,
    .get_func =         powersdr_get_func,
    .set_func =         powersdr_set_func,
    //.set_ant =       kenwood_set_ant_no_ack,
    //.get_ant =       kenwood_get_ant,
    .hamlib_check_rig_caps = HAMLIB_CHECK_RIG_CAPS
};

