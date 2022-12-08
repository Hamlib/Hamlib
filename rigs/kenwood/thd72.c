/*
 *  Hamlib Kenwood TH-D72 backend
 *  Copyright (c) 2000-2011 by Stephane Fillod
 *  Copyright (c) 2018 by Brian Lucas
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


// Some commands are very slow to process so we put a DELAY in those places
#define DELAY hl_usleep(300*1000)

#define THD72_MODES (RIG_MODE_FM|RIG_MODE_FMN|RIG_MODE_AM)
#define THD72_MODES_TX  (RIG_MODE_FM|RIG_MODE_FMN)

#define THD72_FUNC_ALL (RIG_FUNC_TSQL| \
                       RIG_FUNC_AIP| \
                       RIG_FUNC_TONE| \
                       RIG_FUNC_ARO)

#define THD72_LEVEL_ALL (RIG_LEVEL_RFPOWER| \
      RIG_LEVEL_SQL| \
      RIG_LEVEL_BALANCE| \
      RIG_LEVEL_VOXGAIN| \
                        RIG_LEVEL_VOXDELAY)

#define THD72_PARMS (RIG_PARM_APO| \
      RIG_PARM_TIME)

#define THD72_VFO_OP (RIG_OP_NONE)

#define THD72_VFO (RIG_VFO_A|RIG_VFO_B)

static rmode_t thd72_mode_table[3] =
{
    [0] = RIG_MODE_FM,  /* normal, but narrow compared to broadcast */
    [1] = RIG_MODE_FMN, /* what kenwood calls narrow */
    [2] = RIG_MODE_AM,
};

static pbwidth_t thd72_width_table[3] =
{
    [0] = 10000,  // +-5KHz
    [1] =  5000,  // +-2.5KHz
    [2] = 10000,  // what should this be?
};

static rptr_shift_t thd72_rshf_table[3] =
{
    [0] = RIG_RPT_SHIFT_NONE,
    [1] = RIG_RPT_SHIFT_PLUS,
    [2] = RIG_RPT_SHIFT_MINUS,
};

static int thd72tuningstep[11] =
{
    [0] = 5000,
    [1] = 6250,
    [2] = 0, // not used in thd72
    [3] = 10000,
    [4] = 12500,
    [5] = 15000,
    [6] = 20000,
    [7] = 25000,
    [8] = 30000,
    [9] = 50000,
    [10] = 100000
};

static int thd72voxdelay[7] =
{
    [0] =  2500,
    [1] =  5000,
    [2] =  7500,
    [3] = 10000,
    [4] = 15000,
    [5] = 20000,
    [6] = 30000
};

static float thd72sqlevel[6] =
{
    [0] = 0.0,    /* open */
    [1] = 0.2,
    [2] = 0.4,
    [3] = 0.6,
    [4] = 0.8,
    [5] = 1.0
};

static int thd72apo[4] =
{
    [0] = 0,
    [1] = 15,
    [2] = 30,
    [3] = 60
};

static tone_t thd72dcs_list[105] =
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

static struct kenwood_priv_caps thd72_priv_caps =
{
    .cmdtrm =  EOM_TH,   /* Command termination character */
    .mode_table = thd72_mode_table,
};



int thd72_open(RIG *rig)
{
    int ret;
    struct kenwood_priv_data *priv = rig->state.priv;
    strcpy(priv->verify_cmd, "ID\r");

    //ret = kenwood_transaction(rig, "", NULL, 0);
    //DELAY;
    ret = rig_set_vfo(rig, RIG_VFO_A);

    return ret;
}

static int thd72_set_vfo(RIG *rig, vfo_t vfo)
{
    const char *cmd;

    rig_debug(RIG_DEBUG_TRACE, "%s: called\n", __func__);

    switch (vfo)
    {
    case RIG_VFO_A:
    case RIG_VFO_VFO:
    case RIG_VFO_MAIN:
        cmd = "BC 0";
        rig->state.current_vfo = RIG_VFO_A;
        break;

    case RIG_VFO_B:
    case RIG_VFO_SUB:
        rig->state.current_vfo = RIG_VFO_B;
        cmd = "BC 1";
        break;

    default:
        rig_debug(RIG_DEBUG_ERR, "%s: Unsupported VFO: %s\n", __func__,
                  rig_strvfo(vfo));
        return -RIG_ENTARGET;
    }

    return kenwood_simple_transaction(rig, cmd, 4);
}

static int thd72_set_ptt(RIG *rig, vfo_t vfo, ptt_t ptt)
{
    int retval;
    char vfobuf[16];
    struct kenwood_priv_data *priv = rig->state.priv;
    char vfonum = '0';

    rig_debug(RIG_DEBUG_TRACE, "%s: called\n", __func__);

    if (vfo == RIG_VFO_B || priv->split)
    {
        vfonum = '1';
    }

    SNPRINTF(vfobuf, sizeof(vfobuf), "BC %c", vfonum);
    retval = kenwood_transaction(rig, vfobuf, NULL, 0);

    if (retval != RIG_OK)
    {
        return retval;
    }

    return kenwood_transaction(rig, (ptt == RIG_PTT_ON) ? "TX" : "RX", NULL, 0);
}

static int thd72_get_vfo(RIG *rig, vfo_t *vfo)
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
        rig_debug(RIG_DEBUG_ERR, "%s: Unsupported VFO: %c\n", __func__, c);
        return -RIG_EVFO;
    }

    return RIG_OK;
}

static int thd72_set_split_vfo(RIG *rig, vfo_t vfo, split_t split, vfo_t txvfo)
{
    struct kenwood_priv_data *priv = rig->state.priv;
    char vfobuf[16];
    int retval;

    rig_debug(RIG_DEBUG_TRACE, "%s: called %s\n", __func__, rig_strvfo(vfo));

    if (txvfo != RIG_VFO_B) // Only split with RIG_VFO_B as tx
    {
        return -RIG_EINVAL;
    }

    /* Set VFO mode */
    SNPRINTF(vfobuf, sizeof(vfobuf), "VMC 0,0");
    retval = kenwood_transaction(rig, vfobuf, NULL, 0);

    if (retval != RIG_OK)
    {
        return retval;
    }

    SNPRINTF(vfobuf, sizeof(vfobuf), "VMC 1,0");
    retval = kenwood_transaction(rig, vfobuf, NULL, 0);

    if (retval != RIG_OK)
    {
        return retval;
    }

    SNPRINTF(vfobuf, sizeof(vfobuf), "BC 1"); // leave VFOB as selected VFO
    retval = kenwood_transaction(rig, vfobuf, NULL, 0);

    if (retval != RIG_OK)
    {
        return retval;
    }

    /* Remember whether split is on, for thd72_set_vfo */
    priv->split = split;

    return RIG_OK;
}

static int thd72_get_split_vfo(RIG *rig, vfo_t vfo, split_t *split,
                               vfo_t *txvfo)
{
    struct kenwood_priv_data *priv = rig->state.priv;
    char buf[10];
    int retval;

    rig_debug(RIG_DEBUG_TRACE, "%s: called\n", __func__);

    /* Get VFO band */

    retval = kenwood_safe_transaction(rig, "BC", buf, 10, 4);

    if (retval != RIG_OK)
    {
        return retval;
    }

    switch (buf[5])
    {
    case '0': *txvfo = RIG_VFO_A; break;

    case '1': *txvfo = RIG_VFO_B; break;

    default:
        rig_debug(RIG_DEBUG_ERR, "%s: Unexpected txVFO value '%c'\n", __func__, buf[5]);
        return -RIG_EPROTO;
    }

    *split = (buf[3] == buf[5]) ? RIG_SPLIT_OFF : RIG_SPLIT_ON;

    /* Remember whether split is on, for th_set_vfo */
    priv->split = *split;

    return RIG_OK;
}


static int thd72_vfoc(RIG *rig, vfo_t vfo, char *vfoc)
{
    rig_debug(RIG_DEBUG_TRACE, "%s: called VFO=%s\n", __func__, rig_strvfo(vfo));
    vfo = (vfo == RIG_VFO_CURR) ? rig->state.current_vfo : vfo;

    switch (vfo)
    {
    case RIG_VFO_A:
    case RIG_VFO_MAIN: *vfoc = '0'; break;

    case RIG_VFO_B:
    case RIG_VFO_SUB: *vfoc = '1'; break;

    default:
        rig_debug(RIG_DEBUG_ERR, "%s: Unsupported VFO: %s\n", __func__,
                  rig_strvfo(vfo));
        return -RIG_ENTARGET;
    }

    return RIG_OK;
}

static int thd72_get_freq_info(RIG *rig, vfo_t vfo, char *buf)
{
    int retval;
    char c, cmd[8];

    rig_debug(RIG_DEBUG_TRACE, "%s: called VFO=%s\n", __func__, rig_strvfo(vfo));
    retval = thd72_vfoc(rig, vfo, &c);

    if (retval != RIG_OK)
    {
        return retval;
    }

    SNPRINTF(cmd, sizeof(cmd), "FO %c", c);
    retval = kenwood_transaction(rig, cmd, buf, 53);
    return retval;
}

/* item is an offset into reply buf that is a single char */
static int thd72_get_freq_item(RIG *rig, vfo_t vfo, int item, int hi, int *val)
{
    int retval, lval;
    char c, buf[64];

    retval = thd72_get_freq_info(rig, vfo, buf);

    if (retval != RIG_OK)
    {
        return retval;
    }

    c = buf[item];

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

static int thd72_set_freq_item(RIG *rig, vfo_t vfo, int item, int val)
{
    int retval;
    char buf[64];

    retval = thd72_get_freq_info(rig, vfo, buf);

    if (retval != RIG_OK)
    {
        return retval;
    }

    buf[item] = val + '0';
    return kenwood_simple_transaction(rig, buf, 52);
}

static int thd72_set_freq(RIG *rig, vfo_t vfo, freq_t freq)
{
    int retval;
    int tsindex;
    shortfreq_t ts;
    char buf[64], fbuf[11];

    rig_debug(RIG_DEBUG_TRACE, "%s: called, vfo=%s, freq=%f\n", __func__,
              rig_strvfo(vfo), freq);


    retval = thd72_get_freq_info(rig, vfo, buf);

    if (retval != RIG_OK)
    {
        return retval;
    }

    tsindex = buf[16] - '0';

    if (buf[16] >= 'A') { tsindex = buf[16] - 'A' + 10; }

    ts = thd72tuningstep[tsindex];
    rig_debug(RIG_DEBUG_VERBOSE, "%s: tsindex=%d, stepsize=%d\n", __func__, tsindex,
              (int)ts);
    freq = roundl(freq / ts) * ts;
    // cppcheck-suppress *
    SNPRINTF(fbuf, sizeof(fbuf), "%010"PRIll, (int64_t)freq);
    memcpy(buf + 5, fbuf, 10);
    retval = kenwood_simple_transaction(rig, buf, 52);
    return retval;
}

static int thd72_get_freq(RIG *rig, vfo_t vfo, freq_t *freq)
{
    int retval;
    int tsindex;
    shortfreq_t ts;
    char buf[64];

    rig_debug(RIG_DEBUG_TRACE, "%s: called\n", __func__);

    retval = thd72_get_freq_info(rig, vfo, buf);

    if (retval != RIG_OK)
    {
        return retval;
    }

    tsindex = buf[16] - '0';
    ts = thd72tuningstep[tsindex];
    rig_debug(RIG_DEBUG_VERBOSE, "%s: tsindex=%d, stepsize=%d\n", __func__, tsindex,
              (int)ts);
    sscanf(buf + 5, "%"SCNfreq, freq);
    return RIG_OK;
}

static int thd72_set_mode(RIG *rig, vfo_t vfo, rmode_t mode, pbwidth_t width)
{
    int val;

    rig_debug(RIG_DEBUG_TRACE, "%s: called\n", __func__);

    switch (mode)
    {
    case RIG_MODE_FM:  val = 0; break;

    case RIG_MODE_FMN: val = 1; break;

    case RIG_MODE_AM:  val = 2; break;

    default:
        return -RIG_EINVAL;
    }

    return thd72_set_freq_item(rig, vfo, 51, val);
}

static int thd72_get_mode(RIG *rig, vfo_t vfo, rmode_t *mode, pbwidth_t *width)
{
    int retval, modeinx;

    rig_debug(RIG_DEBUG_TRACE, "%s: called\n", __func__);

    retval = thd72_get_freq_item(rig, vfo, 51, 2, &modeinx);

    if (retval != RIG_OK)
    {
        return retval;
    }

    *mode = thd72_mode_table[modeinx];
    *width = thd72_width_table[modeinx];
    return RIG_OK;
}

static int thd72_set_rptr_shft(RIG *rig, vfo_t vfo, rptr_shift_t rptr_shift)
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

    return thd72_set_freq_item(rig, vfo, 18, rsinx);
}

static int thd72_get_rptr_shft(RIG *rig, vfo_t vfo, rptr_shift_t *rptr_shift)
{
    int retval, rsinx;

    rig_debug(RIG_DEBUG_TRACE, "%s: called\n", __func__);

    retval = thd72_get_freq_item(rig, vfo, 18, 3, &rsinx);

    if (retval != RIG_OK)
    {
        return retval;
    }

    /* rsinx == 3 indicates split mode? */
    *rptr_shift = (rsinx == 3) ? RIG_RPT_SHIFT_NONE : thd72_rshf_table[rsinx];
    return RIG_OK;
}


static int thd72_set_rptr_offs(RIG *rig, vfo_t vfo, shortfreq_t offs)
{
    int retval;
    char boff[9], buf[64];

    rig_debug(RIG_DEBUG_TRACE, "%s: called\n", __func__);

    retval = thd72_get_freq_info(rig, vfo, buf);

    if (retval != RIG_OK)
    {
        return retval;
    }

    SNPRINTF(boff, sizeof(boff), "%08ld", offs);
    memcpy(buf + 42, boff, 8);
    retval = kenwood_simple_transaction(rig, buf, 52);
    return retval;
}

static int thd72_get_rptr_offs(RIG *rig, vfo_t vfo, shortfreq_t *offs)
{
    int retval;
    char buf[64];

    rig_debug(RIG_DEBUG_TRACE, "%s: called\n", __func__);

    retval = thd72_get_freq_info(rig, vfo, buf);

    if (retval != RIG_OK)
    {
        return retval;
    }

    sscanf(buf + 42, "%ld", offs);
    return RIG_OK;
}

static int thd72_set_ts(RIG *rig, vfo_t vfo, shortfreq_t ts)
{
    int tsinx;

    rig_debug(RIG_DEBUG_TRACE, "%s: called\n", __func__);

    for (tsinx = 0; tsinx < 10; tsinx++)
    {
        if (thd72tuningstep[tsinx] >= ts)
        {
            thd72_set_freq_item(rig, vfo, 16, tsinx);
            return RIG_OK;
        }
    }

    return -RIG_EINVAL;
}

static int thd72_get_ts(RIG *rig, vfo_t vfo, shortfreq_t *ts)
{
    int retval, tsinx;

    rig_debug(RIG_DEBUG_TRACE, "%s: called\n", __func__);

    retval = thd72_get_freq_item(rig, vfo, 16, 9, &tsinx);

    if (retval != RIG_OK)
    {
        return retval;
    }

    *ts = thd72tuningstep[tsinx];
    return RIG_OK;
}

static int thd72_set_ctcss_tone(RIG *rig, vfo_t vfo, tone_t tone)
{
    int retval, tinx;
    char buf[64], tmp[4];

    rig_debug(RIG_DEBUG_TRACE, "%s: called\n", __func__);

    tinx = 0;   /* default */

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

    retval = thd72_get_freq_info(rig, vfo, buf);

    if (retval != RIG_OK)
    {
        return retval;
    }

    buf[22] = (tone == 0) ? '0' : '1';
    SNPRINTF(tmp, sizeof(tmp), "%02d", tinx);
    memcpy(buf + 30, tmp, 2);
    return kenwood_simple_transaction(rig, buf, 52);
}

static int thd72_get_ctcss_tone(RIG *rig, vfo_t vfo, tone_t *tone)
{
    int retval, tinx;
    char buf[64];

    rig_debug(RIG_DEBUG_TRACE, "%s: called\n", __func__);

    retval = thd72_get_freq_info(rig, vfo, buf);

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

static int thd72_set_dcs_code(RIG *rig, vfo_t vfo, tone_t code)
{
    int retval, cinx;
    char buf[64], tmp[4];

    rig_debug(RIG_DEBUG_TRACE, "%s: called\n", __func__);

    cinx = 0;   /* default */

    if (code != 0)
    {
        for (cinx = 0; cinx < 104; cinx++)
        {
            if (code == thd72dcs_list[cinx])
            {
                break;
            }
        }

        if (cinx >= 104)
        {
            return -RIG_EINVAL;
        }
    }

    retval = thd72_get_freq_info(rig, vfo, buf);

    if (retval != RIG_OK)
    {
        return retval;
    }

    buf[26] = (code == 0) ? '0' : '1';
    SNPRINTF(tmp, sizeof(tmp), "%03d", cinx);
    memcpy(buf + 36, tmp, 3);
    return kenwood_simple_transaction(rig, buf, 52);
}

static int thd72_get_dcs_code(RIG *rig, vfo_t vfo, tone_t *code)
{
    int retval, cinx;
    char buf[64];

    rig_debug(RIG_DEBUG_TRACE, "%s: called\n", __func__);

    retval = thd72_get_freq_info(rig, vfo, buf);

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
        *code = thd72dcs_list[cinx];
    }

    return RIG_OK;
}

static int thd72_set_ctcss_sql(RIG *rig, vfo_t vfo, tone_t tone)
{
    int retval, tinx;
    char buf[64], tmp[4];

    rig_debug(RIG_DEBUG_TRACE, "%s: called\n", __func__);

    tinx = 0;   /* default */

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

    retval = thd72_get_freq_info(rig, vfo, buf);

    if (retval != RIG_OK)
    {
        return retval;
    }

    buf[24] = (tone == 0) ? '0' : '1';
    SNPRINTF(tmp, sizeof(tmp), "%02d", tinx);
    memcpy(buf + 33, tmp, 2);
    return kenwood_simple_transaction(rig, buf, 52);
}

static int thd72_get_ctcss_sql(RIG *rig, vfo_t vfo, tone_t *tone)
{
    int retval, tinx;
    char buf[64];

    rig_debug(RIG_DEBUG_TRACE, "%s: called\n", __func__);

    retval = thd72_get_freq_info(rig, vfo, buf);

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

static int thd72_get_menu_info(RIG *rig, char *buf)
{
    int retval;

    rig_debug(RIG_DEBUG_TRACE, "%s: called\n", __func__);

    retval = kenwood_transaction(rig, "MU", buf, 41);

    if (retval != RIG_OK)
    {
        return retval;
    }

    if (strlen(buf) != 40)
    {
        return -RIG_ERJCTED;
    }

    return RIG_OK;
}

/* each menu item is a single hex digit */
static int thd72_get_menu_item(RIG *rig, int item, int hi, int *val)
{
    int retval, lval;
    char c, buf[48];

    retval = thd72_get_menu_info(rig, buf);

    if (retval != RIG_OK)
    {
        return retval;
    }

    c = buf[3 + 2 * item]; /* "MU 0,1,2 ... */

    if (c >= '0' && c <= '9') { lval = c - '0'; }
    else if (c >= 'A' && c <= 'F') { lval = c - 'A' + 10; }
    else
    {
        return -RIG_EPROTO;
    }

    if (lval > hi)
    {
        return -RIG_EPROTO;
    }

    *val = lval;
    return RIG_OK;
}

static int thd72_set_menu_item(RIG *rig, int item, int val)
{
    int retval;
    char c, buf[48];

    retval = thd72_get_menu_info(rig, buf);

    if (retval != RIG_OK)
    {
        return retval;
    }

    if (val < 10) { c = val + '0'; }
    else { c = val - 10 + 'A'; }

    buf[3 + 2 * item] = c;
    return kenwood_simple_transaction(rig, buf, 40);
}

static int thd72_set_level(RIG *rig, vfo_t vfo, setting_t level, value_t val)
{
    int retval, lvl;
    char c, lvlc, cmd[10];

    rig_debug(RIG_DEBUG_TRACE, "%s: called VFO=%s, level=%s, val=%g\n", __func__,
              rig_strvfo(vfo), rig_strlevel(level), val.f);

    retval = thd72_vfoc(rig, vfo, &c);

    if (retval != RIG_OK)
    {
        return retval;
    }

    switch (level)
    {
    case RIG_LEVEL_RFPOWER:
        if (val.f <= 0.01) { lvlc = '2'; }
        else if (val.f <= 0.10) { lvlc = '1'; }
        else { lvlc = '0'; }

        SNPRINTF(cmd, sizeof(cmd), "PC %c,%c", c, lvlc);
        return kenwood_simple_transaction(rig, cmd, 6);

    case RIG_LEVEL_VOXGAIN:
        return thd72_set_menu_item(rig, 8, (int)(val.f * 10.0 - 0.5));

    case RIG_LEVEL_VOXDELAY:
        if (val.i > 20000) { lvl = 6; }
        else if (val.i > 10000) { lvl = val.i / 10000 + 3; }
        else { lvl = val.i / 2500; }

        return thd72_set_menu_item(rig, 9, lvl);

    case RIG_LEVEL_SQL:
        lvlc = '0' + (int)(val.f * 5);
        SNPRINTF(cmd, sizeof(cmd), "PC %c,%c", c, lvlc);
        return kenwood_simple_transaction(rig, cmd, 6);

    case RIG_LEVEL_BALANCE:
        /* FIXME - is balance 0.0 .. 1.0 or -1.0 .. 1.0? */
        lvl = (int)(val.f * 4.0);
        return thd72_set_menu_item(rig, 13, lvl);

    default:
        rig_debug(RIG_DEBUG_ERR, "%s: Unsupported Level %s\n", __func__,
                  rig_strlevel(level));
        return -RIG_EINVAL;
    }

    return retval;
}

static int thd72_get_level(RIG *rig, vfo_t vfo, setting_t level, value_t *val)
{
    int retval, v, l;
    char c, cmd[10], buf[48];

    rig_debug(RIG_DEBUG_TRACE, "%s: called\n", __func__);

    retval = thd72_vfoc(rig, vfo, &c);

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
        case 0: val->f = 1.00; break; /* 5.0 W */

        case 1: val->f = 0.10; break; /* 500 mW */

        case 2: val->f = 0.01; break; /* 50 mW */
        }

        break;

    case RIG_LEVEL_VOXGAIN:
        retval = thd72_get_menu_item(rig, 8, 9, &l);

        if (retval != RIG_OK)
        {
            return retval;
        }

        /* FIXME - if VOX is off, what do we return */
        val->f = l / 9.0;
        break;

    case RIG_LEVEL_VOXDELAY:
        retval = thd72_get_menu_item(rig, 9, 7, &l);

        if (retval != RIG_OK)
        {
            return retval;
        }

        /* FIXME - if VOX is off, what do we return */
        val->i = thd72voxdelay[l];
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

        val->f = thd72sqlevel[l];
        break;

    case RIG_LEVEL_BALANCE:
        retval = thd72_get_menu_item(rig, 13, 4, &l);

        if (retval != RIG_OK)
        {
            return retval;
        }

        /* FIXME - is balance 0.0 .. 1.0 or -1.0 .. 1.0? */
        val->f = (float)(l) / 4.0;
        break;

    default:
        rig_debug(RIG_DEBUG_ERR, "%s: Unsupported Level %s\n", __func__,
                  rig_strlevel(level));
        return -RIG_EINVAL;
    }

    return RIG_OK;
}

static int thd72_set_func(RIG *rig, vfo_t vfo, setting_t func, int status)
{
    int retval;
    char c;

    rig_debug(RIG_DEBUG_TRACE, "%s: called\n", __func__);

    switch (func)
    {
    case RIG_FUNC_AIP:
        retval = thd72_vfoc(rig, vfo, &c);

        if (retval != RIG_OK)
        {
            return retval;
        }

        return thd72_set_menu_item(rig, c == '0' ? 5 : 6, status);

    case RIG_FUNC_ARO:
        return thd72_set_menu_item(rig, 18, status);

    case RIG_FUNC_TONE:
        return thd72_set_freq_item(rig, vfo, 22, status);

    case RIG_FUNC_TSQL:
        return thd72_set_freq_item(rig, vfo, 24, status);

    default:
        return -RIG_EINVAL;
    }

    return RIG_OK;
}

static int thd72_get_func(RIG *rig, vfo_t vfo, setting_t func, int *status)
{
    int retval, f = -1;
    char c;

    rig_debug(RIG_DEBUG_TRACE, "%s: called\n", __func__);

    switch (func)
    {
    case RIG_FUNC_AIP:
        retval = thd72_vfoc(rig, vfo, &c);

        if (retval != RIG_OK)
        {
            return retval;
        }

        retval = thd72_get_menu_item(rig, c == '0' ? 5 : 6, 1, &f);
        break;

    case RIG_FUNC_ARO:
        retval = thd72_get_menu_item(rig, 18, 1, &f);
        break;

    case RIG_FUNC_TONE:
        retval = thd72_get_freq_item(rig, vfo, 22, 1, &f);
        break;

    case RIG_FUNC_TSQL:
        retval = thd72_get_freq_item(rig, vfo, 24, 1, &f);
        break;

    default:
        retval = -RIG_EINVAL;
    }

    if (retval != RIG_OK)
    {
        return retval;
    }

    *status = f;
    return RIG_OK;
}

static int thd72_set_parm(RIG *rig, setting_t parm, value_t val)
{
    int l;

    rig_debug(RIG_DEBUG_TRACE, "%s: called\n", __func__);

    switch (parm)
    {
    case RIG_PARM_APO:
        if (val.i == 0) { l = 0; }
        else if (val.i <= 15) { l = 1; }
        else if (val.i <= 30) { l = 2; }
        else { l = 3; }

        return thd72_set_menu_item(rig, 3, l);

    case RIG_PARM_TIME:
    default:
        return -RIG_EINVAL;
    }

    return RIG_OK;
}

static int thd72_get_parm(RIG *rig, setting_t parm, value_t *val)
{
    int retval, l, hh, mm, ss;
    char buf[48];

    rig_debug(RIG_DEBUG_TRACE, "%s: called\n", __func__);

    switch (parm)
    {
    case RIG_PARM_APO:
        retval = thd72_get_menu_item(rig, 3, 3, &l);

        if (retval != RIG_OK)
        {
            return retval;
        }

        val->i = thd72apo[l];
        break;

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

static int thd72_set_mem(RIG *rig, vfo_t vfo, int ch)
{
    int retval;
    char c, cmd[10];

    rig_debug(RIG_DEBUG_TRACE, "%s: called\n", __func__);

    retval = thd72_vfoc(rig, vfo, &c);

    if (retval != RIG_OK)
    {
        return retval;
    }

    SNPRINTF(cmd, sizeof(cmd), "MR %c,%03d", c, ch);
    return kenwood_simple_transaction(rig, cmd, 10);
}

static int thd72_get_mem(RIG *rig, vfo_t vfo, int *ch)
{
    int retval;
    char c, cmd[10], buf[10];

    rig_debug(RIG_DEBUG_TRACE, "%s: called\n", __func__);

    retval = thd72_vfoc(rig, vfo, &c);

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

    sscanf(buf + 5, "%d", ch);
    return RIG_OK;
}

static int thd72_set_channel(RIG *rig, vfo_t vfo, const channel_t *chan)
{
    rig_debug(RIG_DEBUG_TRACE, "%s: called\n", __func__);

    return -RIG_EINVAL;
}

static int thd72_parse_channel(int kind, const char *buf, channel_t *chan)
{
    int tmp;
    char c;
    const char *data;

    if (kind == 0) { data = buf + 5; }
    else { data = buf + 7; }

    sscanf(data, "%"SCNfreq, &chan->freq);
    c = data[46]; // mode

    if (c >= '0' && c <= '2')
    {
        chan->mode = thd72_mode_table[c - '0'];
        chan->width = thd72_width_table[c - '0'];
    }

    c = data[11]; // tuning step

    if (c >= '0' && c <= '9')
    {
        chan->tuning_step = thd72tuningstep[c - '0'];
    }

    c = data[13]; // repeater shift

    if (c >= '0' && c <= '2')
    {
        chan->rptr_shift = thd72_rshf_table[c - '0'];
    }

    sscanf(data + 37, "%ld", &chan->rptr_offs);
    c = data[17]; // Tone status

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

    c = data[19]; // TSQL status

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

    c = data[21]; // DCS status

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

static int thd72_get_channel(RIG *rig, vfo_t vfo, channel_t *chan,
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

        retval = thd72_parse_channel(1, buf, chan);

        if (retval != RIG_OK)
        {
            return retval;
        }

        cmd[1] = 'N'; /* change ME to MN */
        retval = kenwood_transaction(rig, cmd, buf, sizeof(buf));

        if (retval != RIG_OK)
        {
            return retval;
        }

        len = strlen(buf);
        memcpy(chan->channel_desc, buf + 7, len - 7);
    }
    else            /* current channel */
    {
        retval = thd72_get_freq_info(rig, chan->vfo, buf);

        if (retval != RIG_OK)
        {
            return retval;
        }

        return thd72_parse_channel(0, buf, chan);
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

#ifdef false  /* not working */
#define CMD_SZ 5
#define BLOCK_SZ 256
#define BLOCK_COUNT 256
#define CHAN_PER_BLOCK 4

static int thd72_get_block(RIG *rig, int block_num, char *block)
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
int thd72_get_chan_all_cb(RIG *rig, chan_cb_t chan_cb, rig_ptr_t arg)
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
    rig_flush(rp); /* flush any remaining data */
    ret = ser_set_rts(rp, 1); /* setRTS or Hardware flow control? */

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

        ret = thd72_get_block(rig, i, block);

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
 * th-d72a rig capabilities.
 */
const struct rig_caps thd72a_caps =
{
    RIG_MODEL(RIG_MODEL_THD72A),
    .model_name = "TH-D72A",
    .mfg_name =  "Kenwood",
    .version =  TH_VER ".1",
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
    .serial_handshake =  RIG_HANDSHAKE_HARDWARE,
    .write_delay =  0,
    .post_write_delay =  0,
    .timeout =  500,
    .retry =  3,
    .has_get_func =  THD72_FUNC_ALL,
    .has_set_func =  THD72_FUNC_ALL,
    .has_get_level =  THD72_LEVEL_ALL,
    .has_set_level =  RIG_LEVEL_SET(THD72_LEVEL_ALL),
    .has_get_parm =  THD72_PARMS,
    .has_set_parm =  THD72_PARMS,    /* FIXME: parms */
    .level_gran =
    {
#include "level_gran_kenwood.h"
    },
    .parm_gran =  {},
    .ctcss_list =  kenwood42_ctcss_list,
    .dcs_list =  thd72dcs_list,
    .preamp =   { RIG_DBLST_END, },
    .attenuator =   { RIG_DBLST_END, },
    .max_rit =  Hz(0),
    .max_xit =  Hz(0),
    .max_ifshift =  Hz(0),
    .vfo_ops =  THD72_VFO_OP,
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
        {MHz(118), MHz(174), THD72_MODES, -1, -1, THD72_VFO},
        {MHz(320), MHz(524), THD72_MODES, -1, -1, THD72_VFO},
        RIG_FRNG_END,
    },
    .tx_range_list2 =  {
        {MHz(144), MHz(148), THD72_MODES_TX, W(0.05), W(5), THD72_VFO},
        {MHz(430), MHz(440), THD72_MODES_TX, W(0.05), W(5), THD72_VFO},
        RIG_FRNG_END,
    },
    .tuning_steps =  {
        {THD72_MODES, kHz(5)},
        {THD72_MODES, kHz(6.25)},
        {THD72_MODES, kHz(8.33)},
        {THD72_MODES, kHz(10)},
        {THD72_MODES, kHz(12.5)},
        {THD72_MODES, kHz(15)},
        {THD72_MODES, kHz(20)},
        {THD72_MODES, kHz(25)},
        {THD72_MODES, kHz(30)},
        {THD72_MODES, kHz(50)},
        RIG_TS_END,
    },
    .filters =  { /* mode/filter list, remember: order matters! */
        {RIG_MODE_FM, kHz(14)},
        {RIG_MODE_FMN, kHz(7)},
        {RIG_MODE_AM, kHz(9)},
        RIG_FLT_END,
    },
    .priv = (void *)& thd72_priv_caps,
    .rig_init = kenwood_init,
    .rig_cleanup = kenwood_cleanup,
    .rig_open = thd72_open,
    .set_freq = thd72_set_freq,
    .get_freq = thd72_get_freq,
    .set_mode = thd72_set_mode,
    .get_mode = thd72_get_mode,
    .set_vfo =  thd72_set_vfo,
    .get_vfo =  thd72_get_vfo,
    .set_ptt = thd72_set_ptt,
    .set_split_vfo = thd72_set_split_vfo,
    .get_split_vfo = thd72_get_split_vfo,
    .set_rptr_shift = thd72_set_rptr_shft,
    .get_rptr_shift = thd72_get_rptr_shft,
    .set_rptr_offs = thd72_set_rptr_offs,
    .get_rptr_offs = thd72_get_rptr_offs,
    .set_ts =    thd72_set_ts,
    .get_ts =    thd72_get_ts,
    .set_ctcss_tone =  thd72_set_ctcss_tone,
    .get_ctcss_tone =  thd72_get_ctcss_tone,
    .set_dcs_code = thd72_set_dcs_code,
    .get_dcs_code = thd72_get_dcs_code,
    .set_ctcss_sql = thd72_set_ctcss_sql,
    .get_ctcss_sql = thd72_get_ctcss_sql,
    .set_level = thd72_set_level,
    .get_level = thd72_get_level,
    .set_func = thd72_set_func,
    .get_func = thd72_get_func,
    .set_parm = thd72_set_parm,
    .get_parm = thd72_get_parm,
    .set_mem  = thd72_set_mem,
    .get_mem  = thd72_get_mem,
    .set_channel = thd72_set_channel,
    .get_channel = thd72_get_channel,
//.get_chan_all_cb = thd72_get_chan_all_cb, this doesn't work yet
    .get_info =  th_get_info,
    .hamlib_check_rig_caps = HAMLIB_CHECK_RIG_CAPS
};
