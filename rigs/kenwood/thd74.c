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

#include <stdint.h>
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>

#include "hamlib/rig.h"
#include "hamlib/rig_state.h"
#include "kenwood.h"
#include "th.h"
#include "thd7x.h"
#include "misc.h"

#define THD74_MODES (RIG_MODE_FM|RIG_MODE_DSTAR|RIG_MODE_AM|RIG_MODE_LSB|RIG_MODE_USB|RIG_MODE_CW|RIG_MODE_FMN|RIG_MODE_WFM|RIG_MODE_CWR)
#define THD74_MODES_TX  (RIG_MODE_FM)

#define THD75_BAND_A_MODES (RIG_MODE_FM|RIG_MODE_FMN|RIG_MODE_DSTAR)
#define THD75_BAND_B_MODES (THD74_MODES)
#define THD75_MODES_TX (RIG_MODE_FM|RIG_MODE_FMN|RIG_MODE_DSTAR)

#define THD74_FUNC_ALL (RIG_FUNC_TSQL|   \
                       RIG_FUNC_TONE)

#define THD75_FUNC_ALL (THD74_FUNC_ALL|RIG_FUNC_VOX)

#define THD74_LEVEL_ALL (RIG_LEVEL_RFPOWER|\
            RIG_LEVEL_SQL|\
            RIG_LEVEL_ATT|\
            RIG_LEVEL_VOXGAIN|\
                        RIG_LEVEL_VOXDELAY)

#define THD75_LEVEL_ALL (RIG_LEVEL_RFPOWER|RIG_LEVEL_SQL|RIG_LEVEL_AF|\
                         RIG_LEVEL_VOXGAIN|RIG_LEVEL_VOXDELAY|\
                         RIG_LEVEL_RAWSTR)

#define THD74_PARMS (RIG_PARM_TIME)

#define THD74_VFO_OP (RIG_OP_NONE)

#define THD74_VFO (RIG_VFO_A|RIG_VFO_B)

#define THD75_CHANNEL_CAPS \
    .freq = 1, \
    .tx_freq = 1, \
    .split = 1, \
    .mode = 1, \
    .width = 1, \
    .tuning_step = 1, \
    .rptr_shift = 1, \
    .rptr_offs = 1, \
    .funcs = RIG_FUNC_REV, \
    .ctcss_tone = 1, \
    .ctcss_sql = 1, \
    .dcs_code = 1, \
    .dcs_sql = 1, \
    .flags = 1

static rmode_t thd74_mode_table[KENWOOD_MODE_TABLE_MAX] =
{
    [0] = RIG_MODE_FM,  /* normal, but narrow compared to broadcast */
    [1] = RIG_MODE_DSTAR,
    [2] = RIG_MODE_AM,
    [3] = RIG_MODE_LSB,
    [4] = RIG_MODE_USB,
    [5] = RIG_MODE_CW,
    [6] = RIG_MODE_FMN,  /* what kenwood calls narrow */
    [7] = RIG_MODE_DSTAR,
    [8] = RIG_MODE_WFM,
    [9] = RIG_MODE_CWR,
};

static pbwidth_t thd74_width_table[10] =
{
    [0] = 14000,
    [1] = 6000,
    [2] = 9000,
    [3] = 2700,
    [4] = 2700,
    [5] = 500,
    [6] = 7000,
    [7] = 6000,
    [8] = 150000,
    [9] = 500,
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

static int thd74tuningstep[12] =
{
    [0] = 5000,
    [1] = 6250,
    [2] = 8330,
    [3] = 9000,
    [4] = 10000,
    [5] = 12500,
    [6] = 15000,
    [7] = 20000,
    [8] = 25000,
    [9] = 30000,
    [10] = 50000,
    [11] = 100000,
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

static int thd75voxdelay[7] =
{
    [0] = 3,
    [1] = 5,
    [2] = 8,
    [3] = 10,
    [4] = 15,
    [5] = 20,
    [6] = 30
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
    //struct kenwood_priv_data *priv = STATE(rig)->priv;
    // this is already done in kenwood_init
    //strcpy(priv->verify_cmd, "ID\r");
    //priv->verify_cmd_len = 3;

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
    vfo = (vfo == RIG_VFO_CURR) ? STATE(rig)->current_vfo : vfo;

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

static int thd74_pull_fo(RIG *rig, vfo_t vfo,
                         struct thd7x_fo_record *record)
{
    char band, cmd[8], reply[THD7X_COMMAND_BUFSIZE];
    int retval;

    retval = thd74_vfoc(rig, vfo, &band);

    if (retval != RIG_OK)
    {
        return retval;
    }

    SNPRINTF(cmd, sizeof(cmd), "FO %c", band);
    retval = kenwood_transaction(rig, cmd, reply, sizeof(reply));

    if (retval != RIG_OK)
    {
        return retval;
    }

    retval = thd7x_parse_fo(reply, strlen(reply), record);

    if (retval != RIG_OK || record->band != (uint8_t)(band - '0'))
    {
        rig_debug(RIG_DEBUG_ERR, "%s: Unexpected reply '%s'\n", __func__, reply);
        return -RIG_EPROTO;
    }

    return RIG_OK;
}

static int thd74_push_fo(RIG *rig, struct thd7x_fo_record *record)
{
    struct thd7x_fo_record acknowledged;
    char command[THD7X_COMMAND_BUFSIZE], reply[THD7X_COMMAND_BUFSIZE];
    int retval;

    retval = thd7x_serialize_fo(record, command, sizeof(command), NULL);

    if (retval != RIG_OK)
    {
        return retval;
    }

    retval = kenwood_transaction(rig, command, reply, sizeof(reply));

    if (retval != RIG_OK)
    {
        return retval;
    }

    retval = thd7x_parse_fo(reply, strlen(reply), &acknowledged);

    if (retval != RIG_OK || acknowledged.band != record->band)
    {
        rig_debug(RIG_DEBUG_ERR, "%s: Unexpected reply '%s'\n", __func__, reply);
        return -RIG_EPROTO;
    }

    *record = acknowledged;
    return RIG_OK;
}

static int thd74_pull_me(RIG *rig, int channel,
                         struct thd7x_me_record *record)
{
    char command[16], reply[THD7X_COMMAND_BUFSIZE];
    int retval;

    if (channel < 0 || channel > 999)
    {
        return -RIG_EINVAL;
    }

    SNPRINTF(command, sizeof(command), "ME %03d", channel);
    retval = kenwood_transaction(rig, command, reply, sizeof(reply));

    if (retval != RIG_OK)
    {
        return retval;
    }

    retval = thd7x_parse_me(reply, strlen(reply), record);

    if (retval != RIG_OK || record->channel != channel)
    {
        rig_debug(RIG_DEBUG_ERR, "%s: Unexpected reply '%s'\n", __func__, reply);
        return -RIG_EPROTO;
    }

    return RIG_OK;
}

static int thd74_push_me(RIG *rig, const struct thd7x_me_record *record)
{
    struct thd7x_me_record acknowledged;
    char command[THD7X_COMMAND_BUFSIZE], reply[THD7X_COMMAND_BUFSIZE];
    int retval;

    retval = thd7x_serialize_me(record, command, sizeof(command), NULL);

    if (retval != RIG_OK)
    {
        return retval;
    }

    retval = kenwood_transaction(rig, command, reply, sizeof(reply));

    if (retval != RIG_OK)
    {
        return retval;
    }

    retval = thd7x_parse_me(reply, strlen(reply), &acknowledged);

    if (retval != RIG_OK || acknowledged.channel != record->channel)
    {
        rig_debug(RIG_DEBUG_ERR, "%s: Unexpected reply '%s'\n", __func__, reply);
        return -RIG_EPROTO;
    }

    return RIG_OK;
}

static int thd75_erase_me(RIG *rig, int channel)
{
    struct thd7x_me_record record;
    char command[16], expected[16], reply[THD7X_COMMAND_BUFSIZE];
    int retval;

    if (channel < 0 || channel > 999)
    {
        return -RIG_EINVAL;
    }

    SNPRINTF(command, sizeof(command), "ME %03d,", channel);
    SNPRINTF(expected, sizeof(expected), "ME %03d", channel);
    retval = kenwood_transaction(rig, command, reply, sizeof(reply));

    if (retval != RIG_OK)
    {
        return retval;
    }

    if (strcmp(reply, expected) != 0)
    {
        rig_debug(RIG_DEBUG_ERR, "%s: Unexpected reply '%s'\n", __func__, reply);
        return -RIG_EPROTO;
    }

    retval = thd74_pull_me(rig, channel, &record);
    return retval == -RIG_ENAVAIL ? RIG_OK :
           (retval == RIG_OK ? -RIG_EPROTO : retval);
}

static int thd74_record_ts(const struct thd7x_fo_record *record,
                           shortfreq_t *ts)
{
    if (record->fine_enabled)
    {
        *ts = thd74tuningstep_fine[record->fine_step];
    }
    else
    {
        *ts = thd74tuningstep[record->rx_step];
    }

    return RIG_OK;
}

static int thd74_get_ts(RIG *rig, vfo_t vfo, shortfreq_t *ts)
{
    struct thd7x_fo_record record;
    int retval;

    rig_debug(RIG_DEBUG_TRACE, "%s: called\n", __func__);

    retval = thd74_pull_fo(rig, vfo, &record);

    if (retval != RIG_OK)
    {
        return retval;
    }

    return thd74_record_ts(&record, ts);
}

static freq_t thd74_round_freq(freq_t freq, shortfreq_t ts)
{
    return (freq_t)(round((double)freq / (double)ts) * ts);
}

static int thd74_set_freq(RIG *rig, vfo_t vfo, freq_t freq)
{
    struct kenwood_priv_data *priv = STATE(rig)->priv;
    struct thd7x_fo_record record;
    shortfreq_t ts;
    int retval;

    rig_debug(RIG_DEBUG_TRACE, "%s: called\n", __func__);

    if (priv->split == RIG_SPLIT_ON)
    {
        vfo = RIG_VFO_B;
    }

    retval = thd74_pull_fo(rig, vfo, &record);

    if (retval != RIG_OK)
    {
        return retval;
    }

    thd74_record_ts(&record, &ts);
    freq = thd74_round_freq(freq, ts);

    if (freq < 0.0 || freq > 9999999999.0)
    {
        return -RIG_EINVAL;
    }

    record.frequency_hz = (uint64_t)llround(freq);
    return thd74_push_fo(rig, &record);
}

static int thd74_get_freq(RIG *rig, vfo_t vfo, freq_t *freq)
{
    struct kenwood_priv_data *priv = STATE(rig)->priv;
    struct thd7x_fo_record record;
    int retval;

    rig_debug(RIG_DEBUG_TRACE, "%s: called\n", __func__);

    if (priv->split == RIG_SPLIT_ON)
    {
        vfo = RIG_VFO_B;
    }

    retval = thd74_pull_fo(rig, vfo, &record);

    if (retval != RIG_OK)
    {
        return retval;
    }

    *freq = (freq_t)record.frequency_hz;
    return RIG_OK;
}

// setting the mode via FO leads to response 'N.' from the handset
int thd74_set_mode(RIG *rig, vfo_t vfo, rmode_t mode, pbwidth_t width)
{
    char mdbuf[8], replybuf[8], v;
    int kmode, retval;
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
    rig_debug(RIG_DEBUG_TRACE, "%s: mdbuf: %s\n", __func__, mdbuf);

    retval = kenwood_transaction(rig, mdbuf, replybuf, 7);
    rig_debug(RIG_DEBUG_TRACE, "%s: retval: %d\n", __func__, retval);

    if (retval != RIG_OK)
    {
        return retval;
    }

    return RIG_OK;
}

static int thd74_get_mode(RIG *rig, vfo_t vfo, rmode_t *mode, pbwidth_t *width)
{
    char band, command[8], reply[8];
    int consumed, parsed_band, parsed_mode, retval;

    rig_debug(RIG_DEBUG_TRACE, "%s: called\n", __func__);

    retval = thd74_vfoc(rig, vfo, &band);

    if (retval != RIG_OK)
    {
        return retval;
    }

    SNPRINTF(command, sizeof(command), "MD %c", band);
    retval = kenwood_transaction(rig, command, reply, sizeof(reply));

    if (retval != RIG_OK)
    {
        return retval;
    }

    consumed = 0;
    retval = sscanf(reply, "MD %d,%d%n", &parsed_band, &parsed_mode, &consumed);

    if (retval != 2 || reply[consumed] != '\0' || parsed_band != band - '0'
            || parsed_mode < 0 || parsed_mode >= 10
            || thd74_mode_table[parsed_mode] == RIG_MODE_NONE)
    {
        rig_debug(RIG_DEBUG_ERR, "%s: Unexpected reply '%s'\n", __func__, reply);
        return -RIG_EPROTO;
    }

    *mode = thd74_mode_table[parsed_mode];
    *width = thd74_width_table[parsed_mode];
    return RIG_OK;
}

static int thd74_set_rptr_shft(RIG *rig, vfo_t vfo, rptr_shift_t rptr_shift)
{
    struct thd7x_fo_record record;
    int retval;

    rig_debug(RIG_DEBUG_TRACE, "%s: called\n", __func__);

    retval = thd74_pull_fo(rig, vfo, &record);

    if (retval != RIG_OK)
    {
        return retval;
    }

    switch (rptr_shift)
    {
    case RIG_RPT_SHIFT_NONE:  record.shift = 0; break;

    case RIG_RPT_SHIFT_PLUS:  record.shift = 1; break;

    case RIG_RPT_SHIFT_MINUS: record.shift = 2; break;

    default:
        return  -RIG_EINVAL;
    }

    return thd74_push_fo(rig, &record);
}

static int thd74_get_rptr_shft(RIG *rig, vfo_t vfo, rptr_shift_t *rptr_shift)
{
    struct thd7x_fo_record record;
    int retval;

    rig_debug(RIG_DEBUG_TRACE, "%s: called\n", __func__);

    retval = thd74_pull_fo(rig, vfo, &record);

    if (retval != RIG_OK)
    {
        return retval;
    }

    *rptr_shift = record.shift == 3 ? RIG_RPT_SHIFT_NONE :
                  thd74_rshf_table[record.shift];
    return RIG_OK;
}


static int thd74_set_rptr_offs(RIG *rig, vfo_t vfo, shortfreq_t offs)
{
    struct thd7x_fo_record record;
    int retval;

    rig_debug(RIG_DEBUG_TRACE, "%s: called\n", __func__);

    if (offs < 0)
    {
        return -RIG_EINVAL;
    }

    retval = thd74_pull_fo(rig, vfo, &record);

    if (retval != RIG_OK)
    {
        return retval;
    }

    record.offset_hz = (uint64_t)offs;
    return thd74_push_fo(rig, &record);
}

static int thd74_get_rptr_offs(RIG *rig, vfo_t vfo, shortfreq_t *offs)
{
    struct thd7x_fo_record record;
    int retval;

    rig_debug(RIG_DEBUG_TRACE, "%s: called\n", __func__);

    retval = thd74_pull_fo(rig, vfo, &record);

    if (retval != RIG_OK)
    {
        return retval;
    }

    if (record.offset_hz > (uint64_t)LONG_MAX)
    {
        return -RIG_EPROTO;
    }

    *offs = (shortfreq_t)record.offset_hz;
    return RIG_OK;
}

static int thd74_set_ts(RIG *rig, vfo_t vfo, shortfreq_t ts)
{
    struct thd7x_fo_record record;
    int retval;
    int tsinx;

    rig_debug(RIG_DEBUG_TRACE, "%s: called\n", __func__);

    for (tsinx = 0; tsinx < 4; tsinx++)
    {
        if (thd74tuningstep_fine[tsinx] == ts)
        {
            retval = thd74_pull_fo(rig, vfo, &record);

            if (retval != RIG_OK)
            {
                return retval;
            }

            record.fine_enabled = 1;
            record.fine_step = (uint8_t)tsinx;
            return thd74_push_fo(rig, &record);
        }
    }

    for (tsinx = 0; tsinx < 12; tsinx++)
    {
        if (thd74tuningstep[tsinx] == ts)
        {
            retval = thd74_pull_fo(rig, vfo, &record);

            if (retval != RIG_OK)
            {
                return retval;
            }

            record.fine_enabled = 0;
            record.rx_step = (uint8_t)tsinx;
            return thd74_push_fo(rig, &record);
        }
    }

    return -RIG_EINVAL;
}

static int thd74_set_ctcss_tone(RIG *rig, vfo_t vfo, tone_t tone)
{
    struct thd7x_fo_record record;
    int retval, tinx;

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

    retval = thd74_pull_fo(rig, vfo, &record);

    if (retval != RIG_OK)
    {
        return retval;
    }

    record.tone_enabled = tone != 0;

    if (tone != 0)
    {
        record.tone_index = (uint8_t)tinx;
    }

    return thd74_push_fo(rig, &record);
}

static int thd74_get_ctcss_tone(RIG *rig, vfo_t vfo, tone_t *tone)
{
    struct thd7x_fo_record record;
    int retval;

    rig_debug(RIG_DEBUG_TRACE, "%s: called\n", __func__);

    retval = thd74_pull_fo(rig, vfo, &record);

    if (retval != RIG_OK)
    {
        return retval;
    }

    if (!record.tone_enabled)
    {
        *tone = 0;
    }
    else
    {
        *tone = kenwood42_ctcss_list[record.tone_index];
    }

    return RIG_OK;
}

static int thd74_set_dcs_code(RIG *rig, vfo_t vfo, tone_t code)
{
    struct thd7x_fo_record record;
    int retval, cinx;

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

    retval = thd74_pull_fo(rig, vfo, &record);

    if (retval != RIG_OK)
    {
        return retval;
    }

    record.dcs_enabled = code != 0;

    if (code != 0)
    {
        record.dcs_index = (uint8_t)cinx;
    }

    return thd74_push_fo(rig, &record);
}

static int thd74_get_dcs_code(RIG *rig, vfo_t vfo, tone_t *code)
{
    struct thd7x_fo_record record;
    int retval;

    rig_debug(RIG_DEBUG_TRACE, "%s: called\n", __func__);

    retval = thd74_pull_fo(rig, vfo, &record);

    if (retval != RIG_OK)
    {
        return retval;
    }

    if (!record.dcs_enabled)
    {
        *code = 0;
    }
    else
    {
        *code = thd74dcs_list[record.dcs_index];
    }

    return RIG_OK;
}

static int thd74_set_ctcss_sql(RIG *rig, vfo_t vfo, tone_t tone)
{
    struct thd7x_fo_record record;
    int retval, tinx;

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

    retval = thd74_pull_fo(rig, vfo, &record);

    if (retval != RIG_OK)
    {
        return retval;
    }

    record.ctcss_enabled = tone != 0;

    if (tone != 0)
    {
        record.ctcss_index = (uint8_t)tinx;
    }

    return thd74_push_fo(rig, &record);
}

static int thd74_get_ctcss_sql(RIG *rig, vfo_t vfo, tone_t *tone)
{
    struct thd7x_fo_record record;
    int retval;

    rig_debug(RIG_DEBUG_TRACE, "%s: called\n", __func__);

    retval = thd74_pull_fo(rig, vfo, &record);

    if (retval != RIG_OK)
    {
        return retval;
    }

    if (!record.ctcss_enabled)
    {
        *tone = 0;
    }
    else
    {
        *tone = kenwood42_ctcss_list[record.ctcss_index];
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

static int thd75_parse_band_digit(const char *reply, const char *command,
                                  char expected_band, int maximum, int *value)
{
    if (strlen(reply) != 6 || reply[0] != command[0]
            || reply[1] != command[1] || reply[2] != ' '
            || reply[3] != expected_band || reply[4] != ','
            || reply[5] < '0' || reply[5] > '0' + maximum)
    {
        return -RIG_EPROTO;
    }

    *value = reply[5] - '0';
    return RIG_OK;
}

static int thd75_parse_global_digit(const char *reply, const char *command,
                                    int maximum, int *value)
{
    if (strlen(reply) != 4 || reply[0] != command[0]
            || reply[1] != command[1] || reply[2] != ' '
            || reply[3] < '0' || reply[3] > '0' + maximum)
    {
        return -RIG_EPROTO;
    }

    *value = reply[3] - '0';
    return RIG_OK;
}

static int thd75_parse_audio_gain(const char *reply, int *value)
{
    if (strlen(reply) != 6 || strncmp(reply, "AG ", 3) != 0
            || reply[3] < '0' || reply[3] > '9'
            || reply[4] < '0' || reply[4] > '9'
            || reply[5] < '0' || reply[5] > '9')
    {
        return -RIG_EPROTO;
    }

    *value = 100 * (reply[3] - '0') + 10 * (reply[4] - '0')
             + reply[5] - '0';
    return *value <= 200 ? RIG_OK : -RIG_EPROTO;
}

static int thd75_nearest_vox_delay(int tenths)
{
    int nearest = 0;
    int difference = abs(tenths - thd75voxdelay[0]);

    for (int i = 1; i < 7; i++)
    {
        int candidate = abs(tenths - thd75voxdelay[i]);

        if (candidate < difference)
        {
            nearest = i;
            difference = candidate;
        }
    }

    return nearest;
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
        if (val.f < 0.0 || val.f > 1.0)
        {
            return -RIG_EINVAL;
        }

        lvl = (int)round(val.f * 5.0);
        SNPRINTF(cmd, sizeof(cmd), "SQ %c,%d", c, lvl);
        return kenwood_simple_transaction(rig, cmd, 6);

    case RIG_LEVEL_ATT:
        SNPRINTF(cmd, sizeof(cmd), "RA %c,%d", c, val.i ? 1 : 0);
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
        retval = kenwood_transaction(rig, cmd, buf, sizeof(buf));

        if (retval != RIG_OK)
        {
            return retval;
        }

        retval = sscanf(buf, "RA %d,%d", &v, &l);

        if (retval != 2 || l < 0 || l > 1)
        {
            rig_debug(RIG_DEBUG_ERR, "%s: Unexpected reply '%s'\n", __func__, buf);
            return -RIG_ERJCTED;
        }

        val->i = l;
        break;

    default:
        rig_debug(RIG_DEBUG_ERR, "%s: unsupported level %s\n", __func__,
                  rig_strlevel(level));
        return -RIG_EINVAL;
    }

    return RIG_OK;
}

static int thd75_set_level(RIG *rig, vfo_t vfo, setting_t level, value_t val)
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
        if (!isfinite(val.f) || val.f < 0.0f || val.f > 1.0f)
        {
            return -RIG_EINVAL;
        }

        if (val.f <= 0.01) { lvlc = '3'; }
        else if (val.f <= 0.1) { lvlc = '2'; }
        else if (val.f <= 0.4) { lvlc = '1'; }
        else { lvlc = '0'; }

        SNPRINTF(cmd, sizeof(cmd), "PC %c,%c", c, lvlc);
        return kenwood_simple_transaction(rig, cmd, 6);

    case RIG_LEVEL_VOXGAIN:
        if (!isfinite(val.f) || val.f < 0.0f || val.f > 1.0f)
        {
            return -RIG_EINVAL;
        }

        SNPRINTF(cmd, sizeof(cmd), "VG %d", (int)lroundf(val.f * 9.0f));
        return kenwood_simple_transaction(rig, cmd, 4);

    case RIG_LEVEL_VOXDELAY:
        if (val.i < thd75voxdelay[0] || val.i > thd75voxdelay[6])
        {
            return -RIG_EINVAL;
        }

        SNPRINTF(cmd, sizeof(cmd), "VD %d", thd75_nearest_vox_delay(val.i));
        return kenwood_simple_transaction(rig, cmd, 4);

    case RIG_LEVEL_AF:
        if (!isfinite(val.f) || val.f < 0.0f || val.f > 1.0f)
        {
            return -RIG_EINVAL;
        }

        lvl = (int)lroundf(val.f * 200.0f);
        SNPRINTF(cmd, sizeof(cmd), "AG %03d", lvl);
        return kenwood_simple_transaction(rig, cmd, 6);

    case RIG_LEVEL_SQL:
        if (!isfinite(val.f) || val.f < 0.0f || val.f > 1.0f)
        {
            return -RIG_EINVAL;
        }

        lvl = (int)round(val.f * 5.0);
        SNPRINTF(cmd, sizeof(cmd), "SQ %c,%d", c, lvl);
        return kenwood_simple_transaction(rig, cmd, 6);

    case RIG_LEVEL_ATT:
        SNPRINTF(cmd, sizeof(cmd), "RA %c,%d", c, val.i ? 1 : 0);
        return kenwood_simple_transaction(rig, cmd, 6);

    default:
        rig_debug(RIG_DEBUG_ERR, "%s: unsupported level %s\n", __func__,
                  rig_strlevel(level));
        return -RIG_EINVAL;
    }

    return retval;
}

static int thd75_get_level(RIG *rig, vfo_t vfo, setting_t level, value_t *val)
{
    int retval, l;
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

        retval = thd75_parse_band_digit(buf, "PC", c, 3, &l);

        if (retval != RIG_OK)
        {
            rig_debug(RIG_DEBUG_ERR, "%s: Unexpected reply '%s'\n", __func__, buf);
            return retval;
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
        retval = kenwood_transaction(rig, "VG", buf, sizeof(buf));

        if (retval != RIG_OK)
        {
            return retval;
        }

        retval = thd75_parse_global_digit(buf, "VG", 9, &l);

        if (retval != RIG_OK)
        {
            rig_debug(RIG_DEBUG_ERR, "%s: Unexpected reply '%s'\n", __func__, buf);
            return retval;
        }

        val->f = l / 9.0f;
        break;

    case RIG_LEVEL_VOXDELAY:
        retval = kenwood_transaction(rig, "VD", buf, sizeof(buf));

        if (retval != RIG_OK)
        {
            return retval;
        }

        retval = thd75_parse_global_digit(buf, "VD", 6, &l);

        if (retval != RIG_OK)
        {
            rig_debug(RIG_DEBUG_ERR, "%s: Unexpected reply '%s'\n", __func__, buf);
            return retval;
        }

        val->i = thd75voxdelay[l];
        break;

    case RIG_LEVEL_AF:
        retval = kenwood_transaction(rig, "AG", buf, sizeof(buf));

        if (retval != RIG_OK)
        {
            return retval;
        }

        retval = thd75_parse_audio_gain(buf, &l);

        if (retval != RIG_OK)
        {
            rig_debug(RIG_DEBUG_ERR, "%s: Unexpected reply '%s'\n", __func__, buf);
            return retval;
        }

        val->f = l / 200.0f;
        break;

    case RIG_LEVEL_RAWSTR:
        SNPRINTF(cmd, sizeof(cmd), "SM %c", c);
        retval = kenwood_transaction(rig, cmd, buf, sizeof(buf));

        if (retval != RIG_OK)
        {
            return retval;
        }

        retval = thd75_parse_band_digit(buf, "SM", c, 5, &l);

        if (retval != RIG_OK)
        {
            rig_debug(RIG_DEBUG_ERR, "%s: Unexpected reply '%s'\n", __func__, buf);
            return retval;
        }

        val->i = l;
        break;

    case RIG_LEVEL_SQL:
        SNPRINTF(cmd, sizeof(cmd), "SQ %c", c);
        retval = kenwood_transaction(rig, cmd, buf, sizeof(buf));

        if (retval != RIG_OK)
        {
            return retval;
        }

        retval = thd75_parse_band_digit(buf, "SQ", c, 5, &l);

        if (retval != RIG_OK)
        {
            rig_debug(RIG_DEBUG_ERR, "%s: Unexpected reply '%s'\n", __func__, buf);
            return retval;
        }

        val->f = thd74sqlevel[l];
        break;

    case RIG_LEVEL_ATT:
        SNPRINTF(cmd, sizeof(cmd), "RA %c", c);
        retval = kenwood_transaction(rig, cmd, buf, sizeof(buf));

        if (retval != RIG_OK)
        {
            return retval;
        }

        retval = thd75_parse_band_digit(buf, "RA", c, 1, &l);

        if (retval != RIG_OK)
        {
            rig_debug(RIG_DEBUG_ERR, "%s: Unexpected reply '%s'\n", __func__, buf);
            return retval;
        }

        val->i = l;
        break;

    default:
        rig_debug(RIG_DEBUG_ERR, "%s: unsupported level %s\n", __func__,
                  rig_strlevel(level));
        return -RIG_EINVAL;
    }

    return RIG_OK;
}

static int thd75_get_dcd(RIG *rig, vfo_t vfo, dcd_t *dcd)
{
    char band, command[8], reply[16];
    int retval, busy;

    retval = thd74_vfoc(rig, vfo, &band);

    if (retval != RIG_OK)
    {
        return retval;
    }

    SNPRINTF(command, sizeof(command), "BY %c", band);
    retval = kenwood_transaction(rig, command, reply, sizeof(reply));

    if (retval != RIG_OK)
    {
        return retval;
    }

    retval = thd75_parse_band_digit(reply, "BY", band, 1, &busy);

    if (retval != RIG_OK)
    {
        rig_debug(RIG_DEBUG_ERR, "%s: Unexpected reply '%s'\n", __func__, reply);
        return retval;
    }

    *dcd = busy ? RIG_DCD_ON : RIG_DCD_OFF;
    return RIG_OK;
}

static int thd74_set_func(RIG *rig, vfo_t vfo, setting_t func, int status)
{
    struct thd7x_fo_record record;
    int retval;

    rig_debug(RIG_DEBUG_TRACE, "%s: called\n", __func__);

    if (status != 0 && status != 1)
    {
        return -RIG_EINVAL;
    }

    retval = thd74_pull_fo(rig, vfo, &record);

    if (retval != RIG_OK)
    {
        return retval;
    }

    switch (func)
    {
    case RIG_FUNC_TONE:
        record.tone_enabled = (uint8_t)status;
        break;

    case RIG_FUNC_TSQL:
        record.ctcss_enabled = (uint8_t)status;
        break;

    default:
        return -RIG_EINVAL;
    }

    return thd74_push_fo(rig, &record);
}

static int thd74_get_func(RIG *rig, vfo_t vfo, setting_t func, int *status)
{
    struct thd7x_fo_record record;
    int retval;

    rig_debug(RIG_DEBUG_TRACE, "%s: called\n", __func__);

    retval = thd74_pull_fo(rig, vfo, &record);

    if (retval != RIG_OK)
    {
        return retval;
    }

    switch (func)
    {
    case RIG_FUNC_TONE:
        *status = record.tone_enabled;
        break;

    case RIG_FUNC_TSQL:
        *status = record.ctcss_enabled;
        break;

    default:
        return -RIG_EINVAL;
    }

    return RIG_OK;
}

static int thd75_set_func(RIG *rig, vfo_t vfo, setting_t func, int status)
{
    char command[5], reply[16];
    int retval;

    if (func != RIG_FUNC_VOX)
    {
        return thd74_set_func(rig, vfo, func, status);
    }

    if (status != 0 && status != 1)
    {
        return -RIG_EINVAL;
    }

    SNPRINTF(command, sizeof(command), "VX %d", status);
    retval = kenwood_transaction(rig, command, reply, sizeof(reply));

    if (retval != RIG_OK)
    {
        return retval;
    }

    return strcmp(command, reply) == 0 ? RIG_OK : -RIG_EPROTO;
}

static int thd75_get_func(RIG *rig, vfo_t vfo, setting_t func, int *status)
{
    char reply[16];
    int retval, value;

    if (func != RIG_FUNC_VOX)
    {
        return thd74_get_func(rig, vfo, func, status);
    }

    retval = kenwood_transaction(rig, "VX", reply, sizeof(reply));

    if (retval != RIG_OK)
    {
        return retval;
    }

    retval = thd75_parse_global_digit(reply, "VX", 1, &value);

    if (retval != RIG_OK)
    {
        rig_debug(RIG_DEBUG_ERR, "%s: Unexpected reply '%s'\n", __func__, reply);
        return retval;
    }

    *status = value;
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

struct thd75_clock
{
    int year;
    int month;
    int day;
    int hour;
    int minute;
    int second;
};

static int thd75_days_in_month(int year, int month)
{
    static const int month_lengths[] =
    {
        31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31
    };
    int days;

    if (year < 2000 || year > 2099 || month < 1 || month > 12)
    {
        return 0;
    }

    days = month_lengths[month - 1];

    if (month == 2
            && (year % 400 == 0 || (year % 4 == 0 && year % 100 != 0)))
    {
        days++;
    }

    return days;
}

static int thd75_validate_clock(const struct thd75_clock *clock)
{
    int days = thd75_days_in_month(clock->year, clock->month);

    return days != 0 && clock->day >= 1 && clock->day <= days
           && clock->hour >= 0 && clock->hour <= 23
           && clock->minute >= 0 && clock->minute <= 59
           && clock->second >= 0 && clock->second <= 59
           ? RIG_OK : -RIG_EINVAL;
}

static int thd75_parse_pair(const char *digits)
{
    return 10 * (digits[0] - '0') + digits[1] - '0';
}

static int thd75_parse_clock(const char *reply, struct thd75_clock *clock)
{
    if (strlen(reply) != 15 || strncmp(reply, "RT ", 3) != 0)
    {
        return -RIG_EPROTO;
    }

    for (int i = 3; i < 15; i++)
    {
        if (reply[i] < '0' || reply[i] > '9')
        {
            return -RIG_EPROTO;
        }
    }

    clock->year = 2000 + thd75_parse_pair(reply + 3);
    clock->month = thd75_parse_pair(reply + 5);
    clock->day = thd75_parse_pair(reply + 7);
    clock->hour = thd75_parse_pair(reply + 9);
    clock->minute = thd75_parse_pair(reply + 11);
    clock->second = thd75_parse_pair(reply + 13);

    return thd75_validate_clock(clock) == RIG_OK ? RIG_OK : -RIG_EPROTO;
}

static int thd75_set_clock(RIG *rig, int year, int month, int day, int hour,
                           int minute, int second, double msec, int utc_offset)
{
    const struct thd75_clock requested =
    {
        year, month, day, hour, minute, second
    };
    struct thd75_clock acknowledged;
    char command[16], reply[48];
    int retval;

    if (!isfinite(msec)
            || (msec != -1.0 && (msec < 0.0 || msec >= 1000.0))
            || thd75_validate_clock(&requested) != RIG_OK)
    {
        return -RIG_EINVAL;
    }

    if (utc_offset != 0)
    {
        return -RIG_ENAVAIL;
    }

    SNPRINTF(command, sizeof(command), "RT %02d%02d%02d%02d%02d%02d",
             year % 100, month, day, hour, minute, second);
    retval = kenwood_transaction(rig, command, reply, sizeof(reply));

    if (retval != RIG_OK)
    {
        return retval;
    }

    retval = thd75_parse_clock(reply, &acknowledged);

    if (retval != RIG_OK || strcmp(command, reply) != 0)
    {
        return -RIG_EPROTO;
    }

    return RIG_OK;
}

static int thd75_get_clock(RIG *rig, int *year, int *month, int *day,
                           int *hour, int *minute, int *second, double *msec,
                           int *utc_offset)
{
    struct thd75_clock clock;
    char reply[48];
    int retval;

    if (year == NULL || month == NULL || day == NULL || hour == NULL
            || minute == NULL || second == NULL || msec == NULL
            || utc_offset == NULL)
    {
        return -RIG_EINVAL;
    }

    retval = kenwood_transaction(rig, "RT", reply, sizeof(reply));

    if (retval != RIG_OK)
    {
        return retval;
    }

    retval = thd75_parse_clock(reply, &clock);

    if (retval != RIG_OK)
    {
        return retval;
    }

    *year = clock.year;
    *month = clock.month;
    *day = clock.day;
    *hour = clock.hour;
    *minute = clock.minute;
    *second = clock.second;
    *msec = 0.0;
    *utc_offset = 0;

    return RIG_OK;
}

static int thd74_set_mem(RIG *rig, vfo_t vfo, int ch)
{
    int retval;
    char c, cmd[10];

    rig_debug(RIG_DEBUG_TRACE, "%s: called\n", __func__);

    if (ch < 0 || ch > 999)
    {
        return -RIG_EINVAL;
    }

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
    int consumed, parsed_band, retval;
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

    consumed = 0;
    retval = sscanf(buf, "MR %d,%d%n", &parsed_band, ch, &consumed);

    if (retval != 2 || buf[consumed] != '\0' || parsed_band != c - '0'
            || *ch < 0 || *ch > 999)
    {
        rig_debug(RIG_DEBUG_ERR, "%s: Unexpected reply '%s'\n", __func__, buf);
        return -RIG_EPROTO;
    }

    return RIG_OK;
}

static int thd75_channel_mode_to_me(rmode_t mode,
                                    struct thd7x_me_record *record)
{
    if (record->mode == 7 && mode == RIG_MODE_DSTAR)
    {
        return RIG_OK;
    }

    for (int i = 0; i < 10; i++)
    {
        if (thd74_mode_table[i] == mode)
        {
            record->mode = (uint8_t)i;
            return RIG_OK;
        }
    }

    return -RIG_EINVAL;
}

static int thd75_channel_step_to_me(shortfreq_t tuning_step,
                                    int is_new_record,
                                    struct thd7x_me_record *record)
{
    for (int i = 0; i < 4; i++)
    {
        if (tuning_step == thd74tuningstep_fine[i])
        {
            record->fine_enabled = 1;
            record->fine_step = (uint8_t)i;
            return RIG_OK;
        }
    }

    for (int i = 0; i < 12; i++)
    {
        if (tuning_step == thd74tuningstep[i])
        {
            record->fine_enabled = 0;
            record->rx_step = (uint8_t)i;

            if (is_new_record)
            {
                record->tx_step = record->rx_step;
            }

            return RIG_OK;
        }
    }

    return -RIG_EINVAL;
}

static int thd75_channel_split_to_me(const channel_t *chan,
                                     struct thd7x_me_record *record)
{
    if (chan->split == RIG_SPLIT_ON)
    {
        if (!isfinite(chan->tx_freq)
                || chan->tx_freq <= 0.0 || chan->tx_freq > 9999999999.0
                || (chan->tx_mode != RIG_MODE_NONE
                    && chan->tx_mode != chan->mode)
                || (chan->funcs & RIG_FUNC_REV) != 0)
        {
            return -RIG_EINVAL;
        }

        record->odd_split_enabled = 1;
        record->offset_hz = (uint64_t)llround(chan->tx_freq);
        record->shift = 0;
        return RIG_OK;
    }

    if (chan->split != RIG_SPLIT_OFF || chan->tx_freq != RIG_FREQ_NONE)
    {
        return -RIG_EINVAL;
    }

    record->odd_split_enabled = 0;
    record->offset_hz = (uint64_t)chan->rptr_offs;

    switch (chan->rptr_shift)
    {
    case RIG_RPT_SHIFT_NONE: record->shift = 0; break;
    case RIG_RPT_SHIFT_PLUS: record->shift = 1; break;
    case RIG_RPT_SHIFT_MINUS: record->shift = 2; break;
    default: return -RIG_EINVAL;
    }

    return RIG_OK;
}

static int thd75_tone_index(tone_t tone, const tone_t *tones, size_t count,
                            int *index)
{
    *index = -1;

    if (tone == 0)
    {
        return RIG_OK;
    }

    for (size_t i = 0; i < count; i++)
    {
        if (tones[i] == tone)
        {
            *index = (int)i;
            return RIG_OK;
        }
    }

    return -RIG_EINVAL;
}

static int thd75_channel_tones_to_me(const channel_t *chan,
                                     struct thd7x_me_record *record)
{
    int tone_index, ctcss_index, dcs_code_index, dcs_sql_index;

    if (thd75_tone_index(chan->ctcss_tone, kenwood42_ctcss_list, 42,
                         &tone_index) != RIG_OK
            || thd75_tone_index(chan->ctcss_sql, kenwood42_ctcss_list, 42,
                                &ctcss_index) != RIG_OK
            || thd75_tone_index(chan->dcs_code, thd74dcs_list, 104,
                                &dcs_code_index) != RIG_OK
            || thd75_tone_index(chan->dcs_sql, thd74dcs_list, 104,
                                &dcs_sql_index) != RIG_OK)
    {
        return -RIG_EINVAL;
    }

    record->tone_enabled = 0;
    record->ctcss_enabled = 0;
    record->dcs_enabled = 0;
    record->cross_enabled = 0;
    record->cross_selector = 0;

    if (tone_index >= 0)
    {
        record->tone_index = (uint8_t)tone_index;
    }

    if (ctcss_index >= 0)
    {
        record->ctcss_index = (uint8_t)ctcss_index;
    }

    if (tone_index >= 0 && ctcss_index >= 0
            && dcs_code_index < 0 && dcs_sql_index < 0)
    {
        record->cross_enabled = 1;
        record->cross_selector = 3;
    }
    else if (tone_index >= 0 && dcs_sql_index >= 0
             && ctcss_index < 0 && dcs_code_index < 0)
    {
        record->cross_enabled = 1;
        record->cross_selector = 1;
        record->dcs_index = (uint8_t)dcs_sql_index;
    }
    else if (dcs_code_index >= 0 && ctcss_index >= 0
             && tone_index < 0 && dcs_sql_index < 0)
    {
        record->cross_enabled = 1;
        record->cross_selector = 2;
        record->dcs_index = (uint8_t)dcs_code_index;
    }
    else if (dcs_code_index >= 0 && dcs_sql_index < 0
             && tone_index < 0 && ctcss_index < 0)
    {
        record->cross_enabled = 1;
        record->cross_selector = 0;
        record->dcs_index = (uint8_t)dcs_code_index;
    }
    else if (dcs_sql_index >= 0
             && (dcs_code_index < 0 || dcs_code_index == dcs_sql_index)
             && tone_index < 0 && ctcss_index < 0)
    {
        record->dcs_enabled = 1;
        record->dcs_index = (uint8_t)dcs_sql_index;
    }
    else if (tone_index >= 0 && ctcss_index < 0
             && dcs_code_index < 0 && dcs_sql_index < 0)
    {
        record->tone_enabled = 1;
    }
    else if (ctcss_index >= 0 && tone_index < 0
             && dcs_code_index < 0 && dcs_sql_index < 0)
    {
        record->ctcss_enabled = 1;
    }
    else if (tone_index >= 0 || ctcss_index >= 0
             || dcs_code_index >= 0 || dcs_sql_index >= 0)
    {
        return -RIG_EINVAL;
    }

    return RIG_OK;
}

static int thd74_set_channel(RIG *rig, vfo_t vfo, const channel_t *chan)
{
    struct thd7x_me_record record;
    int is_new_record = 0, retval;

    rig_debug(RIG_DEBUG_TRACE, "%s: called\n", __func__);
    (void)vfo;

    if (!RIG_IS_THD75)
    {
        return -RIG_ENIMPL;
    }

    if (chan == NULL || chan->vfo != RIG_VFO_MEM
            || chan->channel_num < 0 || chan->channel_num > 999)
    {
        return -RIG_EINVAL;
    }

    if (chan->freq == RIG_FREQ_NONE)
    {
        return thd75_erase_me(rig, chan->channel_num);
    }

    if (!isfinite(chan->freq)
            || chan->freq < 0.0 || chan->freq > 9999999999.0
            || chan->rptr_offs < 0
            || chan->channel_desc[0] != '\0'
            || (chan->funcs & ~RIG_FUNC_REV) != 0
            || (chan->flags & ~RIG_CHFLAG_SKIP) != 0)
    {
        return -RIG_EINVAL;
    }

    retval = thd74_pull_me(rig, chan->channel_num, &record);

    if (retval == -RIG_ENAVAIL)
    {
        memset(&record, 0, sizeof(record));
        record.channel = (uint16_t)chan->channel_num;
        record.tone_index = 8;
        record.ctcss_index = 8;
        strcpy(record.urcall, "CQCQCQ");
        is_new_record = 1;
    }
    else if (retval != RIG_OK)
    {
        return retval;
    }

    record.frequency_hz = (uint64_t)llround(chan->freq);
    record.reverse_enabled = (chan->funcs & RIG_FUNC_REV) != 0;
    record.lockout_enabled = (chan->flags & RIG_CHFLAG_SKIP) != 0;

    retval = thd75_channel_mode_to_me(chan->mode, &record);

    if (retval == RIG_OK && chan->width != 0
            && chan->width != thd74_width_table[record.mode])
    {
        retval = -RIG_EINVAL;
    }

    if (retval == RIG_OK)
    {
        retval = thd75_channel_step_to_me(chan->tuning_step, is_new_record,
                                          &record);
    }

    if (retval == RIG_OK)
    {
        retval = thd75_channel_split_to_me(chan, &record);
    }

    if (retval == RIG_OK)
    {
        retval = thd75_channel_tones_to_me(chan, &record);
    }

    if (retval != RIG_OK)
    {
        return retval;
    }

    return thd74_push_me(rig, &record);
}

static void thd74_channel_tones(channel_t *chan, uint8_t tone_enabled,
                                uint8_t ctcss_enabled, uint8_t dcs_enabled,
                                uint8_t cross_enabled, uint8_t tone_index,
                                uint8_t ctcss_index, uint8_t dcs_index,
                                uint8_t cross_selector)
{
    chan->ctcss_tone = 0;
    chan->ctcss_sql = 0;
    chan->dcs_code = 0;
    chan->dcs_sql = 0;

    if (cross_enabled)
    {
        switch (cross_selector)
        {
        case 0:
            chan->dcs_code = thd74dcs_list[dcs_index];
            break;

        case 1:
            chan->ctcss_tone = kenwood42_ctcss_list[tone_index];
            chan->dcs_sql = thd74dcs_list[dcs_index];
            break;

        case 2:
            chan->dcs_code = thd74dcs_list[dcs_index];
            chan->ctcss_sql = kenwood42_ctcss_list[ctcss_index];
            break;

        case 3:
            chan->ctcss_tone = kenwood42_ctcss_list[tone_index];
            chan->ctcss_sql = kenwood42_ctcss_list[ctcss_index];
            break;
        }

        return;
    }

    if (tone_enabled)
    {
        chan->ctcss_tone = kenwood42_ctcss_list[tone_index];
    }

    if (ctcss_enabled)
    {
        chan->ctcss_sql = kenwood42_ctcss_list[ctcss_index];
    }

    if (dcs_enabled)
    {
        chan->dcs_code = thd74dcs_list[dcs_index];
        chan->dcs_sql = thd74dcs_list[dcs_index];
    }
}

static int thd74_channel_mode(channel_t *chan, uint8_t mode)
{
    if (mode >= 10 || thd74_mode_table[mode] == RIG_MODE_NONE)
    {
        return -RIG_EPROTO;
    }

    chan->mode = thd74_mode_table[mode];
    chan->width = thd74_width_table[mode];
    chan->tx_mode = chan->mode;
    chan->tx_width = chan->width;
    return RIG_OK;
}

static int thd74_channel_from_fo(const struct thd7x_fo_record *record,
                                 channel_t *chan)
{
    int retval;

    if (record->offset_hz > (uint64_t)LONG_MAX)
    {
        return -RIG_EPROTO;
    }

    retval = thd74_channel_mode(chan, record->mode);

    if (retval != RIG_OK)
    {
        return retval;
    }

    chan->freq = (freq_t)record->frequency_hz;
    chan->tx_freq = 0;
    chan->split = RIG_SPLIT_OFF;
    chan->rptr_shift = record->shift == 3 ? RIG_RPT_SHIFT_NONE :
                       thd74_rshf_table[record->shift];
    chan->rptr_offs = (shortfreq_t)record->offset_hz;
    chan->tuning_step = record->fine_enabled ?
                        thd74tuningstep_fine[record->fine_step] :
                        thd74tuningstep[record->rx_step];
    chan->funcs = record->reverse_enabled ? RIG_FUNC_REV : 0;
    chan->flags = RIG_CHFLAG_NONE;
    chan->channel_desc[0] = '\0';
    thd74_channel_tones(chan, record->tone_enabled, record->ctcss_enabled,
                        record->dcs_enabled, record->cross_enabled,
                        record->tone_index, record->ctcss_index,
                        record->dcs_index, record->cross_selector);
    return RIG_OK;
}

static int thd74_channel_from_me(const struct thd7x_me_record *record,
                                 channel_t *chan)
{
    int retval;

    if (record->offset_hz > (uint64_t)LONG_MAX)
    {
        return -RIG_EPROTO;
    }

    retval = thd74_channel_mode(chan, record->mode);

    if (retval != RIG_OK)
    {
        return retval;
    }

    chan->channel_num = record->channel;
    chan->freq = (freq_t)record->frequency_hz;
    chan->tuning_step = record->fine_enabled ?
                        thd74tuningstep_fine[record->fine_step] :
                        thd74tuningstep[record->rx_step];
    chan->funcs = record->reverse_enabled ? RIG_FUNC_REV : 0;
    chan->flags = record->lockout_enabled ? RIG_CHFLAG_SKIP : RIG_CHFLAG_NONE;
    chan->channel_desc[0] = '\0';

    if (record->odd_split_enabled)
    {
        chan->split = RIG_SPLIT_ON;
        chan->tx_freq = (freq_t)record->offset_hz;
        chan->rptr_shift = RIG_RPT_SHIFT_NONE;
        chan->rptr_offs = 0;
    }
    else
    {
        chan->split = RIG_SPLIT_OFF;
        chan->tx_freq = 0;
        chan->rptr_shift = record->shift == 3 ? RIG_RPT_SHIFT_NONE :
                           thd74_rshf_table[record->shift];
        chan->rptr_offs = (shortfreq_t)record->offset_hz;
    }

    thd74_channel_tones(chan, record->tone_enabled, record->ctcss_enabled,
                        record->dcs_enabled, record->cross_enabled,
                        record->tone_index, record->ctcss_index,
                        record->dcs_index, record->cross_selector);
    return RIG_OK;
}

static int thd74_get_channel(RIG *rig, vfo_t vfo, channel_t *chan,
                             int read_only)
{
    struct thd7x_fo_record fo_record;
    struct thd7x_me_record me_record;
    int retval;

    rig_debug(RIG_DEBUG_TRACE, "%s: called\n", __func__);
    (void)read_only;

    if (chan->vfo == RIG_VFO_MEM)   /* memory channel */
    {
        retval = thd74_pull_me(rig, chan->channel_num, &me_record);

        return retval == RIG_OK ? thd74_channel_from_me(&me_record, chan) : retval;
    }
    else                    /* current channel */
    {
        retval = thd74_pull_fo(rig, chan->vfo, &fo_record);

        if (retval != RIG_OK)
        {
            return retval;
        }

        return thd74_channel_from_fo(&fo_record, chan);
    }
}

int thd74_set_split_vfo(RIG *rig, vfo_t vfo, split_t split, vfo_t txvfo)
{
    struct kenwood_priv_data *priv = STATE(rig)->priv;

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
    struct kenwood_priv_data *priv = STATE(rig)->priv;

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
    struct kenwood_priv_data *priv = STATE(rig)->priv;
    struct thd7x_fo_record record;
    int retval;

    rig_debug(RIG_DEBUG_TRACE, "%s: called\n", __func__);

    if (priv->split == RIG_SPLIT_ON)
    {
        vfo = RIG_VFO_A;
    }
    else
    {
        return -RIG_EINVAL;
    }

    retval = thd74_pull_fo(rig, vfo, &record);

    if (retval != RIG_OK)
    {
        return retval;
    }

    *tx_freq = (freq_t)record.frequency_hz;
    return RIG_OK;
}

/*
if priv->split is RIG_SPLIT_ON set freq of VFOA to txfreq and return RIG_OK
otherwise return -RIG_EPROTO
*/
int thd74_set_split_freq(RIG *rig, vfo_t vfo, freq_t tx_freq)
{
    struct kenwood_priv_data *priv = STATE(rig)->priv;

    rig_debug(RIG_DEBUG_TRACE, "%s: called\n", __func__);

    if (priv->split == RIG_SPLIT_ON)
    {
        struct thd7x_fo_record record;
        shortfreq_t ts;
        int retval = thd74_pull_fo(rig, RIG_VFO_A, &record);

        if (retval != RIG_OK)
        {
            return retval;
        }

        thd74_record_ts(&record, &ts);
        tx_freq = thd74_round_freq(tx_freq, ts);

        if (tx_freq < 0.0 || tx_freq > 9999999999.0)
        {
            return -RIG_EINVAL;
        }

        record.frequency_hz = (uint64_t)llround(tx_freq);
        return thd74_push_fo(rig, &record);
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
    hamlib_port_t *rp = RIGPORT(rig);
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
    hamlib_port_t *rp = RIGPORT(rig);
    channel_t *chan;
    chan_t *chan_list = STATE(rig)->chan_list;
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
struct rig_caps thd74_caps =
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
    .parm_gran =  {
        [PARM_TIME] = {.min = {.i = 0}, .max = {.i = 86399}, .step = {.i = 1}},
    },



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

struct rig_caps thd75_caps =
{
    RIG_MODEL(RIG_MODEL_THD75),
    .model_name = "TH-D75",
    .mfg_name = "Kenwood",
    .version = BACKEND_VER ".3",
    .copyright = "LGPL",
    .status = RIG_STATUS_BETA,
    .rig_type = RIG_TYPE_HANDHELD | RIG_FLAG_APRS | RIG_FLAG_TNC | RIG_FLAG_DXCLUSTER,
    .ptt_type = RIG_PTT_RIG,
    .dcd_type = RIG_DCD_RIG,
    .port_type = RIG_PORT_SERIAL,
    .serial_rate_min = 9600,
    .serial_rate_max = 9600,
    .serial_data_bits = 8,
    .serial_stop_bits = 1,
    .serial_parity = RIG_PARITY_NONE,
    .serial_handshake = RIG_HANDSHAKE_NONE,
    .write_delay = 0,
    .post_write_delay = 0,
    .timeout = 500,
    .retry = 3,

    .has_get_func = THD75_FUNC_ALL,
    .has_set_func = THD75_FUNC_ALL,
    .has_get_level = THD75_LEVEL_ALL,
    .has_set_level = RIG_LEVEL_SET(THD75_LEVEL_ALL),
    .has_get_parm = RIG_PARM_NONE,
    .has_set_parm = RIG_PARM_NONE,
    .level_gran =
    {
        [LVL_SQL] = {
            .min = { .f = 0.0f },
            .max = { .f = 1.0f },
            .step = { .f = 0.2f },
        },
        [LVL_RFPOWER] = {
            .min = { .f = 0.01f },
            .max = { .f = 1.0f },
            .step = { .f = 0.0f },
        },
        [LVL_AF] = {
            .min = { .f = 0.0f },
            .max = { .f = 1.0f },
            .step = { .f = 1.0f / 200.0f },
        },
        [LVL_VOXGAIN] = {
            .min = { .f = 0.0f },
            .max = { .f = 1.0f },
            .step = { .f = 1.0f / 9.0f },
        },
        [LVL_VOXDELAY] = {
            .min = { .i = 3 }, .max = { .i = 30 }, .step = { .i = 1 },
        },
        [LVL_RAWSTR] = {
            .min = { .i = 0 }, .max = { .i = 5 }, .step = { .i = 1 },
        },
    },
    .ctcss_list = kenwood42_ctcss_list,
    .dcs_list = thd74dcs_list,
    .preamp = { RIG_DBLST_END, },
    .attenuator = { RIG_DBLST_END, },
    .max_rit = Hz(0),
    .max_xit = Hz(0),
    .max_ifshift = Hz(0),
    .vfo_ops = THD74_VFO_OP,
    .targetable_vfo = RIG_TARGETABLE_FREQ | RIG_TARGETABLE_LEVEL,
    .transceive = RIG_TRN_OFF,
    .bank_qty = 0,
    .chan_desc_sz = 0,
    .chan_list =
    {
        { 0, 999, RIG_MTYPE_MEM, {THD75_CHANNEL_CAPS}},
        RIG_CHAN_END,
    },
    .rx_range_list1 =
    {
        {MHz(136), MHz(174), THD75_BAND_A_MODES, -1, -1, RIG_VFO_A},
        {MHz(410), MHz(470), THD75_BAND_A_MODES, -1, -1, RIG_VFO_A},
        {kHz(100), MHz(76), THD75_BAND_B_MODES, -1, -1, RIG_VFO_B},
        {MHz(76), MHz(108), RIG_MODE_WFM, -1, -1, RIG_VFO_B},
        {MHz(108), MHz(524), THD75_BAND_B_MODES, -1, -1, RIG_VFO_B},
        RIG_FRNG_END,
    },
    .tx_range_list1 =
    {
        {MHz(144), MHz(146), THD75_MODES_TX, W(0.05), W(5), RIG_VFO_A},
        {MHz(430), MHz(440), THD75_MODES_TX, W(0.05), W(5), RIG_VFO_A},
        RIG_FRNG_END,
    },
    .rx_range_list2 =
    {
        {MHz(136), MHz(174), THD75_BAND_A_MODES, -1, -1, RIG_VFO_A},
        {MHz(216), MHz(260), THD75_BAND_A_MODES, -1, -1, RIG_VFO_A},
        {MHz(410), MHz(470), THD75_BAND_A_MODES, -1, -1, RIG_VFO_A},
        {kHz(100), MHz(76), THD75_BAND_B_MODES, -1, -1, RIG_VFO_B},
        {MHz(76), MHz(108), RIG_MODE_WFM, -1, -1, RIG_VFO_B},
        {MHz(108), MHz(524), THD75_BAND_B_MODES, -1, -1, RIG_VFO_B},
        RIG_FRNG_END,
    },
    .tx_range_list2 =
    {
        {MHz(144), MHz(148), THD75_MODES_TX, W(0.05), W(5), RIG_VFO_A},
        {MHz(222), MHz(225), THD75_MODES_TX, W(0.05), W(5), RIG_VFO_A},
        {MHz(430), MHz(450), THD75_MODES_TX, W(0.05), W(5), RIG_VFO_A},
        RIG_FRNG_END,
    },

    .tuning_steps =
    {
        {THD75_BAND_B_MODES, Hz(20)},
        {THD75_BAND_B_MODES, Hz(100)},
        {THD75_BAND_B_MODES, Hz(500)},
        {THD75_BAND_B_MODES, kHz(1)},
        {THD75_BAND_A_MODES | THD75_BAND_B_MODES, kHz(5)},
        {THD75_BAND_A_MODES | THD75_BAND_B_MODES, kHz(6.25)},
        {THD75_BAND_A_MODES | THD75_BAND_B_MODES, Hz(8330)},
        {THD75_BAND_A_MODES | THD75_BAND_B_MODES, kHz(9)},
        {THD75_BAND_A_MODES | THD75_BAND_B_MODES, kHz(10)},
        {THD75_BAND_A_MODES | THD75_BAND_B_MODES, kHz(12.5)},
        {THD75_BAND_A_MODES | THD75_BAND_B_MODES, kHz(15)},
        {THD75_BAND_A_MODES | THD75_BAND_B_MODES, kHz(20)},
        {THD75_BAND_A_MODES | THD75_BAND_B_MODES, kHz(25)},
        {THD75_BAND_A_MODES | THD75_BAND_B_MODES, kHz(30)},
        {THD75_BAND_A_MODES | THD75_BAND_B_MODES, kHz(50)},
        {THD75_BAND_A_MODES | THD75_BAND_B_MODES, kHz(100)},
        RIG_TS_END,
    },
    .filters =
    {
        {RIG_MODE_FM, kHz(14)},
        {RIG_MODE_FMN, kHz(7)},
        {RIG_MODE_DSTAR, kHz(6)},
        {RIG_MODE_AM, kHz(9)},
        {RIG_MODE_LSB | RIG_MODE_USB, Hz(2700)},
        {RIG_MODE_CW | RIG_MODE_CWR, Hz(500)},
        {RIG_MODE_WFM, kHz(150)},
        RIG_FLT_END,
    },
    .priv = (void *)&thd74_priv_caps,

    .rig_init = kenwood_init,
    .rig_cleanup = kenwood_cleanup,
    .rig_open = kenwood_open,
    .set_freq = thd74_set_freq,
    .get_freq = thd74_get_freq,
    .set_mode = thd74_set_mode,
    .get_mode = thd74_get_mode,
    .set_vfo = thd74_set_vfo,
    .get_vfo = thd74_get_vfo,
    .set_ptt = thd74_set_ptt,
    .get_dcd = thd75_get_dcd,
    .set_rptr_shift = thd74_set_rptr_shft,
    .get_rptr_shift = thd74_get_rptr_shft,
    .set_rptr_offs = thd74_set_rptr_offs,
    .get_rptr_offs = thd74_get_rptr_offs,
    .set_ts = thd74_set_ts,
    .get_ts = thd74_get_ts,
    .set_ctcss_tone = thd74_set_ctcss_tone,
    .get_ctcss_tone = thd74_get_ctcss_tone,
    .set_dcs_code = thd74_set_dcs_code,
    .get_dcs_code = thd74_get_dcs_code,
    .set_ctcss_sql = thd74_set_ctcss_sql,
    .get_ctcss_sql = thd74_get_ctcss_sql,
    .set_level = thd75_set_level,
    .get_level = thd75_get_level,
    .set_func = thd75_set_func,
    .get_func = thd75_get_func,
    .set_clock = thd75_set_clock,
    .get_clock = thd75_get_clock,
    .set_mem = thd74_set_mem,
    .get_mem = thd74_get_mem,
    .set_channel = thd74_set_channel,
    .get_channel = thd74_get_channel,
    .get_info = th_get_info,
    .hamlib_check_rig_caps = HAMLIB_CHECK_RIG_CAPS
};
