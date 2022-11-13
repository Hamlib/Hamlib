/*
 *  Hamlib Kenwood TH-D74 backend
 *  Copyright (c) 2000-2011 by Stephane Fillod
 *  Copyright (c) 2018 by Sebastian Denz, based on THD72 from Brian Lucas
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

#include <hamlib/config.h>

#include <stdlib.h>
#include <math.h>

#include "hamlib/rig.h"
#include "kenwood.h"
#include "th.h"
#include "num_stdio.h"
#include "iofunc.h"
#include "serial.h"
#include "misc.h"


#define THD74_MODES (RIG_MODE_FM|RIG_MODE_AM|RIG_MODE_LSB|RIG_MODE_USB|RIG_MODE_CW|RIG_MODE_FMN|RIG_MODE_WFM|RIG_MODE_CWR)
#define THD74_MODES_TX  (RIG_MODE_FM)

#define THD74_FUNC_ALL (RIG_FUNC_TSQL|   \
                       RIG_FUNC_TONE)

#define THD74_LEVEL_ALL (RIG_LEVEL_RFPOWER|\
            RIG_LEVEL_SQL|\
            RIG_LEVEL_ATT|\
            RIG_LEVEL_VOXGAIN|\
                        RIG_LEVEL_VOXDELAY)

#define THD74_PARMS (RIG_PARM_TIME)

#define THD74_VFO_OP (RIG_OP_NONE)

#define THD74_VFO (RIG_VFO_A|RIG_VFO_B)

static rmode_t thd74_mode_table[10] =
{
    [0] = RIG_MODE_FM,  /* normal, but narrow compared to broadcast */
//    [1] = RIG_MODE_DV,
    [2] = RIG_MODE_AM,
    [3] = RIG_MODE_LSB,
    [4] = RIG_MODE_USB,
    [5] = RIG_MODE_CW,
    [6] = RIG_MODE_FMN,  /* what kenwood calls narrow */
//    [7] = RIG_MODE_DR,
    [8] = RIG_MODE_WFM,
    [9] = RIG_MODE_CWR,
};

static pbwidth_t thd74_width_table[10] =
{
    [0] = 10000,    // +-5KHz
    [1] =  5000,    // +-2.5KHz
    [2] = 10000,    // FIXME: what should this be?
    [3] = 10000,    // FIXME: what should this be?
    [4] = 10000,    // FIXME: what should this be?
    [5] = 10000,    // FIXME: what should this be?
    [6] = 10000,    // FIXME: what should this be?
    [7] = 10000,    // FIXME: what should this be?
    [8] = 10000,    // FIXME: what should this be?
    [9] = 10000,    // FIXME: what should this be?
};

static rptr_shift_t thd74_rshf_table[3] =
{
    [0] = RIG_RPT_SHIFT_NONE,
    [1] = RIG_RPT_SHIFT_PLUS,
    [2] = RIG_RPT_SHIFT_MINUS,
};

static int thd74tuningstep_fine[4] =
{
    [0] = 20,
    [1] = 100,
    [2] = 500,
    [3] = 1000,
};

static int thd74tuningstep[11] =
{
    [0] = 5000,
    [1] = 6250,
    [2] = 8330,
    [3] = 9000,
    [4] = 10000,
    [5] = 15000,
    [6] = 20000,
    [7] = 25000,
    [8] = 30000,
    [9] = 50000,
    [10] = 100000,
};

static int thd74voxdelay[7] =
{
    [0] =  2500,
    [1] =  5000,
    [2] =  7500,
    [3] = 10000,
    [4] = 15000,
    [5] = 20000,
    [6] = 30000
};

static float thd74sqlevel[6] =
{
    [0] = 0.0,      /* open */
    [1] = 0.2,
    [2] = 0.4,
    [3] = 0.6,
    [4] = 0.8,
    [5] = 1.0
};

static tone_t thd74dcs_list[105] =
{
    23,  25,  26,  31,  32,  36,  43,  47,
    51,  53,  54,  65,  71,  72,  73,  74,
    114, 115, 116, 122, 125, 131, 132, 134,
    143, 145, 152, 155, 156, 162, 165, 172,
    174, 205, 212, 223, 225, 226, 243, 244,
    245, 246, 251, 252, 255, 261, 263, 265,
    266, 271, 274, 306, 311, 315, 325, 331,
    332, 343, 346, 351, 356, 364, 365, 371,
    411, 412, 413, 423, 431, 432, 445, 446,
    452, 454, 455, 462, 464, 465, 466, 503,
    506, 516, 523, 526, 532, 546, 565, 606,
    612, 624, 627, 631, 632, 654, 662, 664,
    703, 712, 723, 731, 732, 734, 743, 754,
    0
};

static struct kenwood_priv_caps thd74_priv_caps =
{
    .cmdtrm =  EOM_TH,   /* Command termination character */
    .mode_table = thd74_mode_table,
};



int thd74_open(RIG *rig)
{
    //int ret;
    //struct kenwood_priv_data *priv = rig->state.priv;
    // this is already done in kenwood_init
    //strcpy(priv->verify_cmd, "ID\r");

    //ret = kenwood_transaction(rig, "", NULL, 0);

    return RIG_OK;
}



static int thd74_set_vfo(RIG *rig, vfo_t vfo)
{
    const char *cmd;

    rig_debug(RIG_DEBUG_TRACE, "%s: called\n", __func__);

    switch (vfo)
    {
    case RIG_VFO_A:
    case RIG_VFO_VFO:
    case RIG_VFO_MAIN:
        cmd = "BC 0";
        break;

    case RIG_VFO_B:
        cmd = "BC 1";
        break;

    default:
        rig_debug(RIG_DEBUG_ERR, "%s: Unsupported VFO: %s\n", __func__,
                  rig_strvfo(vfo));
        return -RIG_ENTARGET;
    }

    return kenwood_simple_transaction(rig, cmd, 4);
}

static int thd74_get_vfo(RIG *rig, vfo_t *vfo)
{
    int retval;
    char c, buf[10];
    size_t length;

    rig_debug(RIG_DEBUG_TRACE, "%s: called\n", __func__);

    retval = kenwood_transaction(rig, "BC", buf, sizeof(buf));

    if (retval != RIG_OK)
    {
        return retval;
    }

    length = strlen(buf);

    if (length == 4)
    {
        c = buf[3];
    }
    else
    {
        rig_debug(RIG_DEBUG_ERR, "%s: Unexpected answer length %d\n", __func__,
                  (int)length);
        return -RIG_EPROTO;
    }

    switch (c)
    {
    case '0': *vfo = RIG_VFO_A; break;

    case '1': *vfo = RIG_VFO_B; break;

    default:
        rig_debug(RIG_DEBUG_ERR, "%s: Unsupported VFO: %s\n", __func__,
                  rig_strvfo(*vfo));
        return -RIG_EVFO;
    }

    return RIG_OK;
}

static int thd74_vfoc(RIG *rig, vfo_t vfo, char *vfoc)
{
    vfo = (vfo == RIG_VFO_CURR) ? rig->state.current_vfo : vfo;

    switch (vfo)
    {
    case RIG_VFO_A: *vfoc = '0'; break;

    case RIG_VFO_B: *vfoc = '1'; break;

    default:
        rig_debug(RIG_DEBUG_ERR, "%s: Unsupported VFO: %s\n", __func__,
                  rig_strvfo(vfo));
        return -RIG_ENTARGET;
    }

    return RIG_OK;
}

static int thd74_get_freq_info(RIG *rig, vfo_t vfo, char *buf)
{
    int retval;
    char c, cmd[8];

    rig_debug(RIG_DEBUG_TRACE, "%s: called\n", __func__);
    retval = thd74_vfoc(rig, vfo, &c);

    if (retval != RIG_OK)
    {
        return retval;
    }

    SNPRINTF(cmd, sizeof(cmd), "FO %c", c);
    retval = kenwood_transaction(rig, cmd, buf, 73);
    return retval;
}

/* item is an offset into reply buf that is a single char */
static int thd74_get_freq_item(RIG *rig, vfo_t vfo, int item, int hi, int *val)
{
    int retval, lval;
    char c, buf[128];

    retval = thd74_get_freq_info(rig, vfo, buf);

    if (retval != RIG_OK)
    {
        return retval;
    }

    c = buf[item];
    rig_debug(RIG_DEBUG_TRACE, "%s: c:%c\n", __func__, c);

    if (c < '0' || c > '9')
    {
        return -RIG_EPROTO;
    }

    lval = c - '0';

    if (lval > hi)
    {
        return -RIG_EPROTO;
    }

    *val = lval;
    return RIG_OK;
}

static int thd74_set_freq_item(RIG *rig, vfo_t vfo, int item, int val)
{
    int retval;
    char buf[128];

    rig_debug(RIG_DEBUG_TRACE, "%s: called\n", __func__);

    retval = thd74_get_freq_info(rig, vfo, buf);

    if (retval != RIG_OK)
    {
        return retval;
    }

    buf[item] = val + '0';
    return kenwood_simple_transaction(rig, buf, 72);
}

static int thd74_get_ts(RIG *rig, vfo_t vfo, shortfreq_t *ts)
{
    int retval, tsinx, fine, fine_ts;

    rig_debug(RIG_DEBUG_TRACE, "%s: called\n", __func__);

    retval = thd74_get_freq_item(rig, vfo, 16, 9, &tsinx);

    if (retval != RIG_OK)
    {
        rig_debug(RIG_DEBUG_TRACE, "%s: fail1\n", __func__);
        return retval;
    }

    retval = thd74_get_freq_item(rig, vfo, 33, 1, &fine);

    if (retval != RIG_OK)
    {
        rig_debug(RIG_DEBUG_TRACE, "%s: fail1\n", __func__);
        return retval;
    }

    retval = thd74_get_freq_item(rig, vfo, 35, 3, &fine_ts);

    if (retval != RIG_OK)
    {
        rig_debug(RIG_DEBUG_TRACE, "%s: fail1\n", __func__);
        return retval;
    }

    rig_debug(RIG_DEBUG_TRACE, "%s: tsinx is %d\n", __func__, tsinx);
    rig_debug(RIG_DEBUG_TRACE, "%s: fine is %d\n", __func__, fine);
    rig_debug(RIG_DEBUG_TRACE, "%s: fine_ts is %d\n", __func__, fine_ts);

    if (fine > 0)
    {
        *ts = thd74tuningstep_fine[fine_ts];
    }
    else
    {
        *ts = thd74tuningstep[tsinx];
    }

    rig_debug(RIG_DEBUG_TRACE, "%s: stepsize is %d\n", __func__, (int)*ts);
    return RIG_OK;
}

// needs rig and vfo to get correct stepsize
static int thd74_round_freq(RIG *rig, vfo_t vfo, freq_t freq)
{
    int64_t f;
    long double r;
    shortfreq_t ts;
    // cppcheck-suppress *
    char *fmt = "%s: rounded %"PRIll" to %"PRIll" because stepsize:%d\n";

    rig_debug(RIG_DEBUG_TRACE, "%s: called\n", __func__);

    thd74_get_ts(rig, vfo, &ts);

    f = (int64_t)freq;
    r = round((double)f / (double)ts);
    r = ts * r;

    rig_debug(RIG_DEBUG_TRACE, fmt, __func__, f, (int64_t)r, (int)ts);

    return (freq_t)r;
}

static int thd74_set_freq(RIG *rig, vfo_t vfo, freq_t freq)
{
    struct kenwood_priv_data *priv = rig->state.priv;
    int retval;
    char buf[128], fbuf[12];

    rig_debug(RIG_DEBUG_TRACE, "%s: called\n", __func__);

    if (priv->split == RIG_SPLIT_ON)
    {
        vfo = RIG_VFO_B;
    }

    retval = thd74_get_freq_info(rig, vfo, buf);

    if (retval != RIG_OK)
    {
        return retval;
    }

    freq = thd74_round_freq(rig, vfo, freq);
    SNPRINTF(fbuf, sizeof(fbuf), "%010"PRIll, (int64_t)freq);
    memcpy(buf + 5, fbuf, 10);
    retval = kenwood_simple_transaction(rig, buf, 72);
    return retval;
}

static int thd74_get_freq(RIG *rig, vfo_t vfo, freq_t *freq)
{
    struct kenwood_priv_data *priv = rig->state.priv;
    int retval;
    char buf[128];

    rig_debug(RIG_DEBUG_TRACE, "%s: called\n", __func__);

    if (priv->split == RIG_SPLIT_ON)
    {
        vfo = RIG_VFO_B;
    }

    retval = thd74_get_freq_info(rig, vfo, buf);

    if (retval != RIG_OK)
    {
        return retval;
    }

    sscanf(buf + 5, "%"SCNfreq, freq);
    return RIG_OK;
}

// setting the mode via FO leads to response 'N.' from the handset
int thd74_set_mode(RIG *rig, vfo_t vfo, rmode_t mode, pbwidth_t width)
{
    char kmode, mdbuf[8], replybuf[8], v;
    int retval;
    const struct kenwood_priv_caps *priv = (const struct kenwood_priv_caps *)
                                           rig->caps->priv;

    rig_debug(RIG_DEBUG_TRACE, "%s: called\n", __func__);

    retval = thd74_vfoc(rig, vfo, &v);

    if (retval != RIG_OK)
    {
        return retval;
    }

    if (priv->mode_table)
    {
        kmode = rmode2kenwood(mode, priv->mode_table);

        if (kmode < 0)
        {
            rig_debug(RIG_DEBUG_WARN, "%s: Unsupported Mode value '%s'\n",
                      __func__, rig_strrmode(mode));
            return -RIG_EINVAL;
        }

        kmode += '0';
    }
    else
    {
        switch (mode)
        {
        case RIG_MODE_FM: kmode = '0'; break;

        case RIG_MODE_AM: kmode = '1'; break;

//        case RIG_MODE_DV: kmode = '2'; break;

        case RIG_MODE_LSB: kmode = '3'; break;

        case RIG_MODE_USB: kmode = '4'; break;

        case RIG_MODE_CW: kmode = '5'; break;

        case RIG_MODE_FMN: kmode = '6'; break;

//      case RIG_MODE_DR: kmode = '7'; break;

        case RIG_MODE_WFM: kmode = '8'; break;

        case RIG_MODE_CWR: kmode = '9'; break;

        default:
            rig_debug(RIG_DEBUG_ERR, "%s: unsupported mode %s\n", __func__,
                      rig_strrmode(mode));
            return -RIG_EINVAL;
        }
    }

    SNPRINTF(mdbuf, sizeof(mdbuf), "MD %c,%c", v, kmode);
    rig_debug(RIG_DEBUG_ERR, "%s: mdbuf: %s\n", __func__, mdbuf);

    retval = kenwood_transaction(rig, mdbuf, replybuf, 7);
    rig_debug(RIG_DEBUG_ERR, "%s: retval: %d\n", __func__, retval);

    if (retval != RIG_OK)
    {
        return retval;
    }

    return RIG_OK;
}

static int thd74_get_mode(RIG *rig, vfo_t vfo, rmode_t *mode, pbwidth_t *width)
{
    int retval;
    char modec, buf[128];

    rig_debug(RIG_DEBUG_TRACE, "%s: called\n", __func__);

    retval = thd74_get_freq_info(rig, vfo, buf);

    if (retval != RIG_OK)
    {
        return retval;
    }

    modec = buf[31];

    if (modec >= '0' && modec <= '9')
    {
        *mode = thd74_mode_table[modec - '0'];
        *width = thd74_width_table[modec - '0'];
    }
    else
    {
        return -RIG_EINVAL;
    }

    return RIG_OK;
}

static int thd74_set_rptr_shft(RIG *rig, vfo_t vfo, rptr_shift_t rptr_shift)
{
    int rsinx;

    rig_debug(RIG_DEBUG_TRACE, "%s: called\n", __func__);

    switch (rptr_shift)
    {
    case RIG_RPT_SHIFT_NONE:  rsinx = 0; break;

    case RIG_RPT_SHIFT_PLUS:  rsinx = 1; break;

    case RIG_RPT_SHIFT_MINUS: rsinx = 2; break;

    default:
        return  -RIG_EINVAL;
    }

    return thd74_set_freq_item(rig, vfo, 47, rsinx);
}

static int thd74_get_rptr_shft(RIG *rig, vfo_t vfo, rptr_shift_t *rptr_shift)
{
    int retval, rsinx;

    rig_debug(RIG_DEBUG_TRACE, "%s: called\n", __func__);

    retval = thd74_get_freq_item(rig, vfo, 47, 3, &rsinx);

    if (retval != RIG_OK)
    {
        return retval;
    }

    /* rsinx == 3 indicates split mode? */
    *rptr_shift = (rsinx == 3) ? RIG_RPT_SHIFT_NONE : thd74_rshf_table[rsinx];
    return RIG_OK;
}


static int thd74_set_rptr_offs(RIG *rig, vfo_t vfo, shortfreq_t offs)
{
    int retval;
    char boff[11], buf[128];

    rig_debug(RIG_DEBUG_TRACE, "%s: called\n", __func__);

    retval = thd74_get_freq_info(rig, vfo, buf);

    if (retval != RIG_OK)
    {
        return retval;
    }

    SNPRINTF(boff, sizeof(boff), "%010ld", offs);
    memcpy(buf + 16, boff, 10);
    retval = kenwood_simple_transaction(rig, buf, 72);
    return retval;
}

static int thd74_get_rptr_offs(RIG *rig, vfo_t vfo, shortfreq_t *offs)
{
    int retval;
    char buf[128];

    rig_debug(RIG_DEBUG_TRACE, "%s: called\n", __func__);

    retval = thd74_get_freq_info(rig, vfo, buf);

    if (retval != RIG_OK)
    {
        return retval;
    }

    sscanf(buf + 16, "%ld", offs);
    return RIG_OK;
}

static int thd74_set_ts(RIG *rig, vfo_t vfo, shortfreq_t ts)
{
    int tsinx;

    rig_debug(RIG_DEBUG_TRACE, "%s: called\n", __func__);

    for (tsinx = 0; tsinx < 4; tsinx++)
    {
        if (thd74tuningstep_fine[tsinx] >= ts)
        {
            thd74_set_freq_item(rig, vfo, 33, 1); // Turn fine mode on
            thd74_set_freq_item(rig, vfo, 35, tsinx);
            return RIG_OK;
        }
    }

    for (tsinx = 0; tsinx < 10; tsinx++)
    {

        if (thd74tuningstep[tsinx] >= ts)
        {
            thd74_set_freq_item(rig, vfo, 33, 0); //Turn fine mode off
            thd74_set_freq_item(rig, vfo, 27, tsinx);
            return RIG_OK;
        }
    }

    return -RIG_EINVAL;
}

static int thd74_set_ctcss_tone(RIG *rig, vfo_t vfo, tone_t tone)
{
    int retval, tinx;
    char buf[64], tmp[4];

    rig_debug(RIG_DEBUG_TRACE, "%s: called\n", __func__);

    tinx = 0;       /* default */

    if (tone != 0)
    {
        for (tinx = 0; tinx < 42; tinx++)
        {
            if (tone == kenwood42_ctcss_list[tinx])
            {
                break;
            }
        }

        if (tinx >= 42)
        {
            return -RIG_EINVAL;
        }
    }

    retval = thd74_get_freq_info(rig, vfo, buf);

    if (retval != RIG_OK)
    {
        return retval;
    }

    buf[22] = (tone == 0) ? '0' : '1';
    SNPRINTF(tmp, sizeof(tmp), "%02d", tinx);
    memcpy(buf + 30, tmp, 2);
    return kenwood_simple_transaction(rig, buf, 52);
}

static int thd74_get_ctcss_tone(RIG *rig, vfo_t vfo, tone_t *tone)
{
    int retval, tinx;
    char buf[64];

    rig_debug(RIG_DEBUG_TRACE, "%s: called\n", __func__);

    retval = thd74_get_freq_info(rig, vfo, buf);

    if (retval != RIG_OK)
    {
        return retval;
    }

    if (buf[22] == '0') /* no tone */
    {
        *tone = 0;
    }
    else
    {
        sscanf(buf + 30, "%d", &tinx);

        if (tinx >= 0 && tinx <= 41)
        {
            *tone = kenwood42_ctcss_list[tinx];
        }
        else
        {
            return -RIG_EINVAL;
        }
    }

    return RIG_OK;
}

static int thd74_set_dcs_code(RIG *rig, vfo_t vfo, tone_t code)
{
    int retval, cinx;
    char buf[64], tmp[4];

    rig_debug(RIG_DEBUG_TRACE, "%s: called\n", __func__);

    cinx = 0;       /* default */

    if (code != 0)
    {
        for (cinx = 0; cinx < 104; cinx++)
        {
            if (code == thd74dcs_list[cinx])
            {
                break;
            }
        }

        if (cinx >= 104)
        {
            return -RIG_EINVAL;
        }
    }

    retval = thd74_get_freq_info(rig, vfo, buf);

    if (retval != RIG_OK)
    {
        return retval;
    }

    buf[26] = (code == 0) ? '0' : '1';
    SNPRINTF(tmp, sizeof(tmp), "%03d", cinx);
    memcpy(buf + 36, tmp, 3);
    return kenwood_simple_transaction(rig, buf, 52);
}

static int thd74_get_dcs_code(RIG *rig, vfo_t vfo, tone_t *code)
{
    int retval, cinx;
    char buf[64];

    rig_debug(RIG_DEBUG_TRACE, "%s: called\n", __func__);

    retval = thd74_get_freq_info(rig, vfo, buf);

    if (retval != RIG_OK)
    {
        return retval;
    }

    if (buf[26] == '0') /* no tone */
    {
        *code = 0;
    }
    else
    {
        sscanf(buf + 36, "%d", &cinx);
        *code = thd74dcs_list[cinx];
    }

    return RIG_OK;
}

static int thd74_set_ctcss_sql(RIG *rig, vfo_t vfo, tone_t tone)
{
    int retval, tinx;
    char buf[64], tmp[4];

    rig_debug(RIG_DEBUG_TRACE, "%s: called\n", __func__);

    tinx = 0;       /* default */

    if (tone != 0)
    {
        for (tinx = 0; tinx < 42; tinx++)
        {
            if (tone == kenwood42_ctcss_list[tinx])
            {
                break;
            }
        }

        if (tinx >= 42)
        {
            return -RIG_EINVAL;
        }
    }

    retval = thd74_get_freq_info(rig, vfo, buf);

    if (retval != RIG_OK)
    {
        return retval;
    }

    buf[24] = (tone == 0) ? '0' : '1';
    SNPRINTF(tmp, sizeof(tmp), "%02d", tinx);
    memcpy(buf + 33, tmp, 2);
    return kenwood_simple_transaction(rig, buf, 52);
}

static int thd74_get_ctcss_sql(RIG *rig, vfo_t vfo, tone_t *tone)
{
    int retval, tinx;
    char buf[64];

    rig_debug(RIG_DEBUG_TRACE, "%s: called\n", __func__);

    retval = thd74_get_freq_info(rig, vfo, buf);

    if (retval != RIG_OK)
    {
        return retval;
    }

    if (buf[24] == '0') /* no tsql */
    {
        *tone = 0;
    }
    else
    {
        sscanf(buf + 33, "%d", &tinx);

        if (tinx >= 0 && tinx <= 41)
        {
            *tone = kenwood42_ctcss_list[tinx];
        }
        else
        {
            return -RIG_EINVAL;
        }
    }

    return RIG_OK;
}

int thd74_set_ptt(RIG *rig, vfo_t vfo, ptt_t ptt)
{
    const char *ptt_cmd;

    rig_debug(RIG_DEBUG_VERBOSE, "%s called\n", __func__);

    switch (ptt)
    {
    case RIG_PTT_ON:
        ptt_cmd = "TX";
        return kenwood_simple_transaction(rig, ptt_cmd, 4);
        break;

    case RIG_PTT_OFF:
        ptt_cmd = "RX";
        return kenwood_simple_transaction(rig, ptt_cmd, 2);
        break;

    default: return -RIG_EINVAL;
    }
}

static int thd74_set_level(RIG *rig, vfo_t vfo, setting_t level, value_t val)
{
    int retval, lvl;
    char c, lvlc, cmd[11];

    rig_debug(RIG_DEBUG_TRACE, "%s: called\n", __func__);
    rig_debug(RIG_DEBUG_TRACE, "%s: level: %s\n", __func__, rig_strlevel(level));
    rig_debug(RIG_DEBUG_TRACE, "%s: value.i: %d\n", __func__, val.i);
    rig_debug(RIG_DEBUG_TRACE, "%s: value.f: %lf\n", __func__, val.f);

    retval = thd74_vfoc(rig, vfo, &c);

    if (retval != RIG_OK)
    {
        return retval;
    }

    switch (level)
    {
    case RIG_LEVEL_RFPOWER:
        if (val.f <= 0.01) { lvlc = '3'; }
        else if (val.f <= 0.1) { lvlc = '2'; }
        else if (val.f <= 0.4) { lvlc = '1'; }
        else { lvlc = '0'; }

        SNPRINTF(cmd, sizeof(cmd), "PC %c,%c", c, lvlc);
        return kenwood_simple_transaction(rig, cmd, 6);

    case RIG_LEVEL_VOXGAIN:
        SNPRINTF(cmd, sizeof(cmd), "VG %d", (int)(val.f * 10.0 - 0.5));
        return kenwood_simple_transaction(rig, cmd, 4);

    case RIG_LEVEL_VOXDELAY:
        if (val.i > 20000) { lvl = 6; }
        else if (val.i > 10000) { lvl = val.i / 10000 + 3; }
        else { lvl = val.i / 2500; }

        SNPRINTF(cmd, sizeof(cmd), "VD %d", lvl);
        return kenwood_simple_transaction(rig, cmd, 4);

    case RIG_LEVEL_SQL:
        SNPRINTF(cmd, sizeof(cmd), "SQ %c,%d", c, (int)val.f);
        return kenwood_simple_transaction(rig, cmd, 6);

    case RIG_LEVEL_ATT: // no value provided to distinguish between on/off??
        SNPRINTF(cmd, sizeof(cmd), "RA %c,%d", c, (int)val.f);
        return kenwood_simple_transaction(rig, cmd, 6);

    default:
        rig_debug(RIG_DEBUG_ERR, "%s: unsupported level %s\n", __func__,
                  rig_strlevel(level));
        return -RIG_EINVAL;
    }

    return retval;
}

static int thd74_get_level(RIG *rig, vfo_t vfo, setting_t level, value_t *val)
{
    int retval, v, l;
    char c, cmd[10], buf[128];

    rig_debug(RIG_DEBUG_TRACE, "%s: called\n", __func__);

    retval = thd74_vfoc(rig, vfo, &c);

    if (retval != RIG_OK)
    {
        return retval;
    }

    switch (level)
    {
    case RIG_LEVEL_RFPOWER:
        SNPRINTF(cmd, sizeof(cmd), "PC %c", c);
        retval = kenwood_transaction(rig, cmd, buf, sizeof(buf));

        if (retval != RIG_OK)
        {
            return retval;
        }

        retval = sscanf(buf, "PC %d,%d", &v, &l);

        if (retval != 2 || l < 0 || l > 3)
        {
            rig_debug(RIG_DEBUG_ERR, "%s: Unexpected reply '%s'\n", __func__, buf);
            return -RIG_ERJCTED;
        }

        switch (l)
        {
        case 0: val->f = 1.00; break;   /* 5.0 W */

        case 1: val->f = 0.40; break;   /* 2.0 W */

        case 2: val->f = 0.1; break;    /* 500 mW */

        case 3: val->f = 0.01; break;   /* 50 mW */
        }

        break;

    case RIG_LEVEL_VOXGAIN:
        SNPRINTF(cmd, sizeof(cmd), "VG");
        retval = kenwood_transaction(rig, cmd, buf, sizeof(buf));

        if (retval != RIG_OK)
        {
            return retval;
        }

        rig_debug(RIG_DEBUG_TRACE, "%s: VOXGAIN buf:%s\n", __func__, buf);
        /* FIXME - if VOX is off, what do we return */
        val->f = (buf[0] - '0') / 9.0;
        break;

    case RIG_LEVEL_VOXDELAY:
        SNPRINTF(cmd, sizeof(cmd), "VD");
        retval = kenwood_transaction(rig, cmd, buf, sizeof(buf));

        if (retval != RIG_OK)
        {
            return retval;
        }

        /* FIXME - if VOX is off, what do we return */
        rig_debug(RIG_DEBUG_TRACE, "%s: VOXDELAY buf:%s\n", __func__, buf);
        val->i = thd74voxdelay[buf[0] - '0'];
        break;

    case RIG_LEVEL_SQL:
        SNPRINTF(cmd, sizeof(cmd), "SQ %c", c);
        retval = kenwood_transaction(rig, cmd, buf, sizeof(buf));

        if (retval != RIG_OK)
        {
            return retval;
        }

        retval = sscanf(buf, "SQ %d,%d", &v, &l);

        if (retval != 2 || l < 0 || l >= 6)
        {
            rig_debug(RIG_DEBUG_ERR, "%s: Unexpected reply '%s'\n", __func__, buf);
            return -RIG_ERJCTED;
        }

        val->f = thd74sqlevel[l];
        break;

    case RIG_LEVEL_ATT:
        SNPRINTF(cmd, sizeof(cmd), "RA %c", c);
        retval = kenwood_transaction(rig, cmd, buf, 7);

        if (retval != RIG_OK)
        {
            return retval;
        }

        sscanf(buf + 5, "%d", &val->i);
        break;

    default:
        rig_debug(RIG_DEBUG_ERR, "%s: unsupported level %s\n", __func__,
                  rig_strlevel(level));
        return -RIG_EINVAL;
    }

    return RIG_OK;
}

static int thd74_set_func(RIG *rig, vfo_t vfo, setting_t func, int status)
{
    rig_debug(RIG_DEBUG_TRACE, "%s: called\n", __func__);

    switch (func)
    {
    case RIG_FUNC_TONE:
        return thd74_set_freq_item(rig, vfo, 37, status);

    case RIG_FUNC_TSQL:
        return thd74_set_freq_item(rig, vfo, 39, status);

    default:
        return -RIG_EINVAL;
    }

    return RIG_OK;
}

static int thd74_get_func(RIG *rig, vfo_t vfo, setting_t func, int *status)
{
    int retval, f;

    rig_debug(RIG_DEBUG_TRACE, "%s: called\n", __func__);

    switch (func)
    {
    case RIG_FUNC_TONE:
        retval = thd74_get_freq_item(rig, vfo, 37, 1, &f);
        break;

    case RIG_FUNC_TSQL:
        retval = thd74_get_freq_item(rig, vfo, 39, 1, &f);
        break;

    default:
        return -RIG_EINVAL;
    }

    if (retval != RIG_OK)
    {
        return retval;
    }

    *status = f;
    return RIG_OK;
}

static int thd74_set_parm(RIG *rig, setting_t parm, value_t val)
{
    rig_debug(RIG_DEBUG_TRACE, "%s: called\n", __func__);

    switch (parm)
    {
    case RIG_PARM_TIME: // FIXME check val, send formatted via RT
    default:
        return -RIG_EINVAL;
    }

    return RIG_OK;
}

static int thd74_get_parm(RIG *rig, setting_t parm, value_t *val)
{
    int retval, hh, mm, ss;
    char buf[48];

    rig_debug(RIG_DEBUG_TRACE, "%s: called\n", __func__);

    switch (parm)
    {
    case RIG_PARM_TIME:
        retval = kenwood_transaction(rig, "RT", buf, sizeof(buf));

        if (retval != RIG_OK)
        {
            return retval;
        }

        sscanf(buf + 11, "%2d%2d%2d", &hh, &mm, &ss);
        val->i = ss + 60 * (mm + 60 * hh);
        break;

    default:
        return -RIG_EINVAL;
    }

    return RIG_OK;
}

static int thd74_set_mem(RIG *rig, vfo_t vfo, int ch)
{
    int retval;
    char c, cmd[10];

    rig_debug(RIG_DEBUG_TRACE, "%s: called\n", __func__);

    retval = thd74_vfoc(rig, vfo, &c);

    if (retval != RIG_OK)
    {
        return retval;
    }

    SNPRINTF(cmd, sizeof(cmd), "MR %c,%03d", c, ch);
    return kenwood_simple_transaction(rig, cmd, 8);
}

static int thd74_get_mem(RIG *rig, vfo_t vfo, int *ch)
{
    int retval;
    char c, cmd[10], buf[10];

    rig_debug(RIG_DEBUG_TRACE, "%s: called\n", __func__);

    retval = thd74_vfoc(rig, vfo, &c);

    if (retval != RIG_OK)
    {
        return retval;
    }

    SNPRINTF(cmd, sizeof(cmd), "MR %c", c);
    retval = kenwood_transaction(rig, cmd, buf, sizeof(buf));

    if (retval != RIG_OK)
    {
        return retval;
    }

    sscanf(buf + 3, "%d", ch);
    return RIG_OK;
}

static int thd74_set_channel(RIG *rig, vfo_t vfo, const channel_t *chan)
{
    rig_debug(RIG_DEBUG_TRACE, "%s: called\n", __func__);

    return -RIG_EINVAL;
}

static int thd74_parse_channel(int kind, const char *buf, channel_t *chan)
{
    int tmp;
    char c;
    const char *data;

    if (kind == 0) { data = buf + 5; }
    else { data = buf + 7; }

    sscanf(data, "%"SCNfreq, &chan->freq);
    c = data[46];   // mode

    if (c >= '0' && c <= '2')
    {
        chan->mode = thd74_mode_table[c - '0'];
        chan->width = thd74_width_table[c - '0'];
    }

    c = data[11];   // tuning step

    if (c >= '0' && c <= '9')
    {
        chan->tuning_step = thd74tuningstep[c - '0'];
    }

    c = data[13];   // repeater shift

    if (c >= '0' && c <= '2')
    {
        chan->rptr_shift = thd74_rshf_table[c - '0'];
    }

    sscanf(data + 37, "%ld", &chan->rptr_offs);
    c = data[17];   // Tone status

    if (c != '0')
    {
        sscanf(data + 25, "%d", &tmp);

        if (tmp > 0 && tmp < 42)
        {
            chan->ctcss_tone = kenwood42_ctcss_list[tmp];
        }
    }
    else
    {
        chan->ctcss_tone = 0;
    }

    c = data[19];   // TSQL status

    if (c != '0')
    {
        sscanf(data + 28, "%d", &tmp);

        if (tmp > 0 && tmp < 42)
        {
            chan->ctcss_sql = kenwood42_ctcss_list[tmp];
        }
    }
    else
    {
        chan->ctcss_sql = 0;
    }

    c = data[21];   // DCS status

    if (c != '0')
    {
        sscanf(data + 31, "%d", &tmp);
        chan->dcs_code = tmp;
    }
    else
    {
        chan->dcs_code = 0;
    }

    return RIG_OK;
}

static int thd74_get_channel(RIG *rig, vfo_t vfo, channel_t *chan,
                             int read_only)
{
    int retval;
    char buf[72];

    rig_debug(RIG_DEBUG_TRACE, "%s: called\n", __func__);

    if (chan->vfo == RIG_VFO_MEM)   /* memory channel */
    {
        int len;
        char cmd[16];
        SNPRINTF(cmd, sizeof(cmd), "ME %03d", chan->channel_num);
        retval = kenwood_transaction(rig, cmd, buf, sizeof(buf));

        if (retval != RIG_OK)
        {
            return retval;
        }

        retval = thd74_parse_channel(1, buf, chan);

        if (retval != RIG_OK)
        {
            return retval;
        }

        cmd[1] = 'N';   /* change ME to MN */
        retval = kenwood_transaction(rig, cmd, buf, sizeof(buf));

        if (retval != RIG_OK)
        {
            return retval;
        }

        len = strlen(buf);
        memcpy(chan->channel_desc, buf + 7, len - 7);
    }
    else                    /* current channel */
    {
        retval = thd74_get_freq_info(rig, chan->vfo, buf);

        if (retval != RIG_OK)
        {
            return retval;
        }

        return thd74_parse_channel(0, buf, chan);
    }

    if (!read_only)
    {
        // Set rig to channel values
        rig_debug(RIG_DEBUG_ERR,
                  "%s: please contact hamlib mailing list to implement this\n", __func__);
        rig_debug(RIG_DEBUG_ERR,
                  "%s: need to know if rig updates when channel read or not\n", __func__);
        return -RIG_ENIMPL;
    }

    return RIG_OK;
}

int thd74_set_split_vfo(RIG *rig, vfo_t vfo, split_t split, vfo_t txvfo)
{
    struct kenwood_priv_data *priv = rig->state.priv;

    rig_debug(RIG_DEBUG_TRACE, "%s: called\n", __func__);

    if (txvfo != RIG_VFO_A)
    {
        return -RIG_EINVAL;
    }

    priv->split = split;

    return RIG_OK;
}

int thd74_get_split_vfo(RIG *rig, vfo_t vfo, split_t *split, vfo_t *txvfo)
{
    struct kenwood_priv_data *priv = rig->state.priv;

    rig_debug(RIG_DEBUG_TRACE, "%s: called\n", __func__);

    if (priv->split == RIG_SPLIT_ON)
    {
        *txvfo = RIG_VFO_A;
    }
    else
    {
        return -RIG_EPROTO;
    }

    return RIG_OK;
}

/*
if priv->split is RIG_SPLIT_ON set *tx_freq to freq of VFOA and return RIG_OK
otherwise return -RIG_EPROTO
*/
int thd74_get_split_freq(RIG *rig, vfo_t vfo, freq_t *tx_freq)
{
    struct kenwood_priv_data *priv = rig->state.priv;
    int retval;
    char buf[128];

    rig_debug(RIG_DEBUG_TRACE, "%s: called\n", __func__);

    if (priv->split == RIG_SPLIT_ON)
    {
        vfo = RIG_VFO_A;
    }
    else
    {
        return -RIG_EINVAL;
    }

    retval = thd74_get_freq_info(rig, vfo, buf);

    if (retval != RIG_OK)
    {
        return retval;
    }

    sscanf(buf + 5, "%"SCNfreq, tx_freq);
    return RIG_OK;
}

/*
if priv->split is RIG_SPLIT_ON set freq of VFOA to txfreq and return RIG_OK
otherwise return -RIG_EPROTO
*/
int thd74_set_split_freq(RIG *rig, vfo_t vfo, freq_t tx_freq)
{
    struct kenwood_priv_data *priv = rig->state.priv;

    rig_debug(RIG_DEBUG_TRACE, "%s: called\n", __func__);

    if (priv->split == RIG_SPLIT_ON)
    {
        char fbuf[12], buf[128];
        int retval = thd74_get_freq_info(rig, RIG_VFO_A, buf);

        if (retval != RIG_OK)
        {
            return retval;
        }

        tx_freq = thd74_round_freq(rig, RIG_VFO_A, tx_freq);
        SNPRINTF(fbuf, sizeof(fbuf), "%010"PRIll, (int64_t)tx_freq);
        memcpy(buf + 5, fbuf, 10);
        return  kenwood_simple_transaction(rig, buf, 72);
    }

    return -RIG_EPROTO;
}

#ifdef false    /* not working */
#define CMD_SZ 5
#define BLOCK_SZ 256
#define BLOCK_COUNT 256
#define CHAN_PER_BLOCK 4

static int thd74_get_block(RIG *rig, int block_num, char *block)
{
    hamlib_port_t *rp = &rig->state.rigport;
    char cmd[CMD_SZ] = "R\0\0\0\0";
    char resp[CMD_SZ];
    int ret;

    /* fetching block i */
    cmd[2] = block_num & 0xff;

    ret = write_block(rp, cmd, CMD_SZ);

    if (ret != RIG_OK)
    {
        return ret;
    }

    /* read response first */
    ret = read_block(rp, resp, CMD_SZ);

    if (ret != CMD_SZ)
    {
        return ret;
    }

    if (resp[0] != 'W' || memcmp(cmd + 1, resp + 1, CMD_SZ - 1))
    {
        return -RIG_EPROTO;
    }

    /* read block */
    ret = read_block(rp, block, BLOCK_SZ);

    if (ret != BLOCK_SZ)
    {
        return ret;
    }

    ret = write_block(rp, "\006", 1);

    if (ret != RIG_OK)
    {
        return ret;
    }

    ret = read_block(rp, resp, 1);

    if (ret != 1)
    {
        return ret;
    }

    if (resp[0] != 0x06)
    {
        return -RIG_EPROTO;
    }

    return RIG_OK;
}

#ifdef XXREMOVEDXX
int thd74_get_chan_all_cb(RIG *rig, chan_cb_t chan_cb, rig_ptr_t arg)
{
    int i, j, ret;
    hamlib_port_t *rp = &rig->state.rigport;
    channel_t *chan;
    chan_t *chan_list = rig->state.chan_list;
    int chan_next = chan_list[0].start;
    char block[BLOCK_SZ];
    char resp[CMD_SZ];

    ret = kenwood_transaction(rig, "0M PROGRAM", resp, CMD_SZ);

    if (ret != RIG_OK)
    {
        return ret;
    }

    if (strlen(resp) != 2 || memcmp(resp, "0M", 2))
    {
        return -RIG_EPROTO;
    }

    rp->parm.serial.rate = 57600;
    serial_setup(rp);


    hl_usleep(100 * 1000); /* let the pcr settle */
    rig_flush(rp);   /* flush any remaining data */
    ret = ser_set_rts(rp, 1);   /* setRTS or Hardware flow control? */

    if (ret != RIG_OK)
    {
        return ret;
    }

    /*
     * setting chan to NULL means the application
     * has to provide a struct where to store data
     * future data for channel channel_num
     */
    chan = NULL;
    ret = chan_cb(rig, &chan, chan_next, chan_list, arg);

    if (ret != RIG_OK)
    {
        return ret;
    }

    if (chan == NULL)
    {
        return -RIG_ENOMEM;
    }

    for (i = 0; i < BLOCK_COUNT; i++)
    {

        ret = thd74_get_block(rig, i, block);

        if (ret != RIG_OK)
        {
            return ret;
        }

        /*
         * Most probably, there's 64 bytes per channel (256*256 / 1000+)
         */
        for (j = 0; j < CHAN_PER_BLOCK; j++)
        {
            char *block_chan = block + j * (BLOCK_SZ / CHAN_PER_BLOCK);
            memset(chan, 0, sizeof(channel_t));
            chan->vfo = RIG_VFO_MEM;
            chan->channel_num = i * CHAN_PER_BLOCK + j;

            /* What are the extra 64 channels ? */
            if (chan->channel_num >= 1000)
            {
                break;
            }

            /* non-empty channel ? */
            // if (block_chan[0] != 0xff) {
            // since block_chan is *signed* char, this maps to -1
            if (block_chan[0] != -1)
            {

                memcpy(chan->channel_desc, block_chan, 8);
                /* TODO: chop off trailing chars */
                chan->channel_desc[8] = '\0';

                /* TODO: parse block and fill in chan */
            }

            /* notify the end? */
            chan_next = chan_next < chan_list[i].end ? chan_next + 1 : chan_next;

            /*
             * provide application with channel data,
             * and ask for a new channel structure
             */
            chan_cb(rig, &chan, chan_next, chan_list, arg);
        }
    }

    ret = write_block(rp, "E", 1);

    if (ret != RIG_OK)
    {
        return ret;
    }

    ret = read_block(rp, resp, 1);

    if (ret != 1)
    {
        return ret;
    }

    if (resp[0] != 0x06)
    {
        return -RIG_EPROTO;
    }

    /* setRTS?? getCTS needed? */
    ret = ser_set_rts(rp, 1);

    if (ret != RIG_OK)
    {
        return ret;
    }

    return RIG_OK;
}
#endif
#endif  /* none working stuff */
/*
 * th-d74 rig capabilities.
 */
const struct rig_caps thd74_caps =
{
    RIG_MODEL(RIG_MODEL_THD74),
    .model_name = "TH-D74",
    .mfg_name =  "Kenwood",
    .version =  BACKEND_VER ".3",
    .copyright =  "LGPL",
    .status =  RIG_STATUS_STABLE,
    .rig_type =  RIG_TYPE_HANDHELD | RIG_FLAG_APRS | RIG_FLAG_TNC | RIG_FLAG_DXCLUSTER,
    .ptt_type =  RIG_PTT_RIG,
    .dcd_type =  RIG_DCD_RIG,
    .port_type =  RIG_PORT_SERIAL,
    .serial_rate_min =  9600,
    .serial_rate_max =  9600,
    .serial_data_bits =  8,
    .serial_stop_bits =  1,
    .serial_parity =  RIG_PARITY_NONE,
    .serial_handshake =  RIG_HANDSHAKE_XONXOFF,
    .write_delay =  0,
    .post_write_delay =  0,
    .timeout =  500,
    .retry =  3,

    .has_get_func =  THD74_FUNC_ALL,
    .has_set_func =  THD74_FUNC_ALL,
    .has_get_level =  THD74_LEVEL_ALL,
    .has_set_level =  RIG_LEVEL_SET(THD74_LEVEL_ALL),
    .has_get_parm =  THD74_PARMS,
    .has_set_parm =  THD74_PARMS,
    .level_gran =
    {
#include "level_gran_kenwood.h"
    },
    .parm_gran =  {},
    .ctcss_list =  kenwood38_ctcss_list,
    .dcs_list =  NULL,
    .preamp =   { RIG_DBLST_END, },
    .attenuator =   { RIG_DBLST_END, },
    .max_rit =  Hz(0),
    .max_xit =  Hz(0),
    .max_ifshift =  Hz(0),
    .vfo_ops =  THD74_VFO_OP,
    .targetable_vfo =  RIG_TARGETABLE_FREQ,
    .transceive =  RIG_TRN_RIG,
    .bank_qty =   0,
    .chan_desc_sz =  6, /* TBC */
    .chan_list =  {
        {  0,  999, RIG_MTYPE_MEM, {TH_CHANNEL_CAPS}},   /* TBC MEM */
        RIG_CHAN_END,
    },
    .rx_range_list1 =  { RIG_FRNG_END, },    /* FIXME: enter region 1 setting */
    .tx_range_list1 =  { RIG_FRNG_END, },
    .rx_range_list2 =  {
        {MHz(118), MHz(174), THD74_MODES, -1, -1, THD74_VFO},
        {MHz(320), MHz(524), THD74_MODES, -1, -1, THD74_VFO},
        RIG_FRNG_END,
    }, /* rx range */
    .tx_range_list2 =  {
        {MHz(144), MHz(148), THD74_MODES_TX, W(0.05), W(5), THD74_VFO},
        {MHz(430), MHz(440), THD74_MODES_TX, W(0.05), W(5), THD74_VFO},
        RIG_FRNG_END,
    }, /* tx range */

    .tuning_steps =  {
        {THD74_MODES, kHz(5)},
        {THD74_MODES, kHz(6.25)},
        /* kHz(8.33)  ?? */
        {THD74_MODES, kHz(10)},
        {THD74_MODES, kHz(12.5)},
        {THD74_MODES, kHz(15)},
        {THD74_MODES, kHz(20)},
        {THD74_MODES, kHz(25)},
        {THD74_MODES, kHz(30)},
        {THD74_MODES, kHz(50)},
        {THD74_MODES, kHz(100)},
        RIG_TS_END,
    },
    /* mode/filter list, remember: order matters! */
    .filters =  {
        {RIG_MODE_FM, kHz(14)},
        {RIG_MODE_FMN, kHz(7)},
        {RIG_MODE_AM, kHz(9)},
        RIG_FLT_END,
    },
    .priv = (void *)& thd74_priv_caps,

    .rig_init = kenwood_init,
    .rig_cleanup = kenwood_cleanup,
    .rig_open = kenwood_open,
    .set_freq = thd74_set_freq,
    .get_freq = thd74_get_freq,
    .set_mode = thd74_set_mode,
    .get_mode = thd74_get_mode,
    .set_vfo =  thd74_set_vfo,
    .get_vfo =  thd74_get_vfo,
    .set_ptt = thd74_set_ptt,
    .set_rptr_shift = thd74_set_rptr_shft,
    .get_rptr_shift = thd74_get_rptr_shft,
    .set_rptr_offs = thd74_set_rptr_offs,
    .get_rptr_offs = thd74_get_rptr_offs,
    .set_ts =    thd74_set_ts,
    .get_ts =    thd74_get_ts,
    .set_ctcss_tone =  thd74_set_ctcss_tone,
    .get_ctcss_tone =  thd74_get_ctcss_tone,
    .set_dcs_code = thd74_set_dcs_code,
    .get_dcs_code = thd74_get_dcs_code,
    .set_ctcss_sql = thd74_set_ctcss_sql,
    .get_ctcss_sql = thd74_get_ctcss_sql, .set_level = thd74_set_level,
    .get_level = thd74_get_level,
    .set_func = thd74_set_func,
    .get_func = thd74_get_func,
    .set_parm = thd74_set_parm,
    .get_parm = thd74_get_parm,
    .set_mem  = thd74_set_mem,
    .get_mem  = thd74_get_mem,
    .set_channel = thd74_set_channel,
    .get_channel = thd74_get_channel,
    .set_split_vfo = thd74_set_split_vfo,
    .get_split_vfo = thd74_get_split_vfo,
    .set_split_freq = thd74_set_split_freq,
    .get_split_freq = thd74_get_split_freq,
//.get_chan_all_cb = thd74_get_chan_all_cb, this doesn't work yet

    .get_info =  th_get_info,
    .hamlib_check_rig_caps = HAMLIB_CHECK_RIG_CAPS
};
