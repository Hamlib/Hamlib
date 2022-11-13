/*
 *  Hamlib Kenwood backend - TH-F7 description
 *  Copyright (c) 2001-2009 by Stephane Fillod
 *
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

#include <hamlib/rig.h>
#include "tones.h"
#include "kenwood.h"
#include "th.h"

#define THF7_MODES_TX (RIG_MODE_FM)
#define THF7_HIGH_MODES (RIG_MODE_FM|RIG_MODE_AM|RIG_MODE_WFM)
#define THF7_ALL_MODES (THF7_HIGH_MODES|RIG_MODE_SSB|RIG_MODE_CW)

#define THF7_FUNC_ALL (RIG_FUNC_ARO|RIG_FUNC_LOCK|RIG_FUNC_BC)


/*
 * How incredible, there's no RIG_LEVEL_STRENGTH!
 */
#define THF7_LEVEL_ALL (RIG_LEVEL_SQL|RIG_LEVEL_RFPOWER|RIG_LEVEL_ATT|\
        RIG_LEVEL_BALANCE|RIG_LEVEL_VOXGAIN|RIG_LEVEL_VOXDELAY)

#define THF7_PARMS (RIG_PARM_APO|RIG_PARM_BEEP|RIG_PARM_BACKLIGHT)

#define THF7_VFO_OP (RIG_OP_UP|RIG_OP_DOWN)

#define THF7_ANTS (RIG_ANT_1|RIG_ANT_2)

/*
 * TODO: * scan_group can only be get.  scan_group=channel_num%50;
 */
#define THF7_CHANNEL_CAPS   \
    TH_CHANNEL_CAPS,\
    .flags=1,   \
    .dcs_code=1,    \
    .dcs_sql=1,

#define THF7_CHANNEL_CAPS_WO_LO \
    TH_CHANNEL_CAPS,\
    .dcs_code=1,    \
    .dcs_sql=1,

/* CTCSS 01..42 */
static tone_t thf7_ctcss_list[] =
{
    670,  693,  719,  744,  770,  797,  825,  854,  885,  915,
    948,  974, 1000, 1035, 1072, 1109, 1148, 1188, 1230, 1273,
    1318, 1365, 1413, 1462, 1514, 1567, 1622, 1679, 1738, 1799,
    1862, 1928, 2035, 2065, 2107, 2181, 2257, 2291, 2336, 2418,
    2503, 2541,
    0
};

static rmode_t thf7_mode_table[KENWOOD_MODE_TABLE_MAX] =
{
    [0] = RIG_MODE_FM,
    [1] = RIG_MODE_WFM,
    [2] = RIG_MODE_AM,
    [3] = RIG_MODE_LSB,
    [4] = RIG_MODE_USB,
    [5] = RIG_MODE_CW
};


/*
 * Band A & B
 */
#define THF7_VFO (RIG_VFO_A|RIG_VFO_B)

static struct kenwood_priv_caps  thf7_priv_caps  =
{
    .cmdtrm =  EOM_TH,   /* Command termination character */
    .mode_table = thf7_mode_table,
};

static int thf7e_init(RIG *rig);
static int thf7e_open(RIG *rig);
static int thf7e_get_vfo(RIG *rig, vfo_t *vfo);
static int thf7e_vfo_op(RIG *rig, vfo_t vfo, vfo_op_t op);

/*
 * TH-F7E rig capabilities.
 *
 * Manual, thanks to K6MAY: http://www.k6may.com/KenwoodTHF6Tip1.shtml
 *
 * TODO:
 * - set/get_ctcss_tone/sql through set/get_channel() and VR/VW
 * - emulate RIG_FUNC_TONE|RIG_FUNC_TSQL by setting ctcss_tone/sql to 0/non zero?
 */
const struct rig_caps thf7e_caps =
{
    RIG_MODEL(RIG_MODEL_THF7E),
    .model_name = "TH-F7E",
    .mfg_name =  "Kenwood",
    .version =  TH_VER ".0",
    .copyright =  "LGPL",
    .status =  RIG_STATUS_STABLE,
    .rig_type =  RIG_TYPE_HANDHELD,
    .ptt_type =  RIG_PTT_RIG,
    .dcd_type =  RIG_DCD_RIG,
    .port_type =  RIG_PORT_SERIAL,
    .serial_rate_min =  9600,
    .serial_rate_max =  9600,
    .serial_data_bits =  8,
    .serial_stop_bits =  1,
    .serial_parity =  RIG_PARITY_NONE,
    .serial_handshake =  RIG_HANDSHAKE_NONE,
    .write_delay =  0,
    .post_write_delay =  0,
    .timeout =  500,
    .retry =  3,

    .has_get_func =  THF7_FUNC_ALL,
    .has_set_func =  THF7_FUNC_ALL | RIG_FUNC_TBURST,
    .has_get_level =  THF7_LEVEL_ALL,
    .has_set_level =  RIG_LEVEL_SET(THF7_LEVEL_ALL),
    .has_get_parm =  THF7_PARMS,
    .has_set_parm =  THF7_PARMS,
    .level_gran =
    {
#include "level_gran_kenwood.h"
    },
    .parm_gran =  {},
    .ctcss_list =  thf7_ctcss_list,
    .dcs_list =  common_dcs_list,
    .preamp =   { RIG_DBLST_END, },
    .attenuator =   { 20, RIG_DBLST_END, },
    .max_rit =  Hz(0),
    .max_xit =  Hz(0),
    .max_ifshift =  Hz(0),
    .vfo_ops =  THF7_VFO_OP,
    .targetable_vfo =  RIG_TARGETABLE_FREQ,
    .transceive =  RIG_TRN_RIG, /* TBC */
    .bank_qty =   0,
    .chan_desc_sz =  8,


    .chan_list =  {
        {  0,  399, RIG_MTYPE_MEM, {THF7_CHANNEL_CAPS}},   /* normal MEM */
        {  400, 409, RIG_MTYPE_EDGE, {THF7_CHANNEL_CAPS}}, /* L0-L9 lower scan limit */
        {  410, 419, RIG_MTYPE_EDGE, {THF7_CHANNEL_CAPS}}, /* U0-U9 upper scan limit */
        {  420, 429, RIG_MTYPE_MEM,  {THF7_CHANNEL_CAPS}}, /* I0-I9 info */
        {  430, 431, RIG_MTYPE_PRIO, {THF7_CHANNEL_CAPS}}, /* PR0,PR1 priority */
        {  432, 434, RIG_MTYPE_CALL, {THF7_CHANNEL_CAPS_WO_LO}}, /* Call (for each ham band) */
        {  435, 449, RIG_MTYPE_BAND, {THF7_CHANNEL_CAPS_WO_LO}}, /* VFO */
        /* 3 A-band VFO */
        /* 11 B-band VFO */
        /* TODO: 10 DTMF */
        RIG_CHAN_END,
    },

    .rx_range_list1 = {
        /* RIG_ANT_2 is internal bar antenna */
        {MHz(144), MHz(146), THF7_MODES_TX, -1, -1, RIG_VFO_MEM | RIG_VFO_A, RIG_ANT_1},
        {MHz(430), MHz(440), THF7_MODES_TX, -1, -1, RIG_VFO_MEM | RIG_VFO_A, RIG_ANT_1},
        {kHz(100), MHz(470), THF7_ALL_MODES, -1, -1, RIG_VFO_MEM | RIG_VFO_B, RIG_ANT_1 | RIG_ANT_2},
        {MHz(470), GHz(1.3), THF7_HIGH_MODES, -1, -1, RIG_VFO_MEM | RIG_VFO_B, RIG_ANT_1},
        RIG_FRNG_END
    },
    .tx_range_list1 = {
        /* power actually depends on DC power supply */
        {MHz(144), MHz(146), THF7_MODES_TX, W(0.05), W(5), RIG_VFO_MEM | RIG_VFO_A, RIG_ANT_1},
        {MHz(430), MHz(440), THF7_MODES_TX, W(0.05), W(5), RIG_VFO_MEM | RIG_VFO_A, RIG_ANT_1},
        RIG_FRNG_END
    },

    /* region 2 is model TH-F6A in fact */
    .rx_range_list2 = {
        /* RIG_ANT_2 is internal bar antenna */
        {MHz(144), MHz(148), THF7_MODES_TX, -1, -1, RIG_VFO_MEM | RIG_VFO_A, RIG_ANT_1},
        {MHz(222), MHz(225), THF7_MODES_TX, -1, -1, RIG_VFO_MEM | RIG_VFO_A, RIG_ANT_1},
        {MHz(430), MHz(450), THF7_MODES_TX, -1, -1, RIG_VFO_MEM | RIG_VFO_A, RIG_ANT_1},
        {kHz(100), MHz(470), THF7_ALL_MODES, -1, -1, RIG_VFO_MEM | RIG_VFO_B, RIG_ANT_1 | RIG_ANT_2},
        {MHz(470), GHz(1.3), THF7_HIGH_MODES, -1, -1, RIG_VFO_MEM | RIG_VFO_B, RIG_ANT_1},
        RIG_FRNG_END
    },
    .tx_range_list2 = {
        /* power actually depends on DC power supply */
        {MHz(144), MHz(148), THF7_MODES_TX, W(0.05), W(5), RIG_VFO_MEM | RIG_VFO_A, RIG_ANT_1},
        {MHz(222), MHz(225), THF7_MODES_TX, W(0.05), W(5), RIG_VFO_MEM | RIG_VFO_A, RIG_ANT_1},
        {MHz(430), MHz(450), THF7_MODES_TX, W(0.05), W(5), RIG_VFO_MEM | RIG_VFO_A, RIG_ANT_1},
        RIG_FRNG_END
    },

    .tuning_steps =  {
        /* This table is ordered according to protocol, from '0' to 'b' */
        /* The steps are not available on every band/frequency limit 470MHz */
        {THF7_ALL_MODES, kHz(5)},
        {THF7_ALL_MODES, kHz(6.25)},
        {THF7_ALL_MODES, kHz(8.33)},
        {THF7_ALL_MODES, kHz(9)},
        {THF7_ALL_MODES, kHz(10)},
        {THF7_ALL_MODES, kHz(12.5)},
        {THF7_ALL_MODES, kHz(15)},
        {THF7_ALL_MODES, kHz(20)},
        {THF7_ALL_MODES, kHz(25)},
        {THF7_ALL_MODES, kHz(30)},
        {THF7_ALL_MODES, kHz(50)},
        {THF7_ALL_MODES, kHz(100)},
        RIG_TS_END,
    },

    /* mode/filter list, remember: order matters! */
    .filters =  {
        /* real width to be checked (missing specs) */
        {RIG_MODE_FM, kHz(12)},
        {RIG_MODE_FM, kHz(6)},      /* narrow FM */
        {RIG_MODE_AM, kHz(9)},
        {RIG_MODE_WFM, kHz(150)},   /* or 230? */
        {RIG_MODE_SSB | RIG_MODE_CW, kHz(3)},
        RIG_FLT_END,
    },

    .priv       = (void *)& thf7_priv_caps,

    .rig_init   = thf7e_init,
    .rig_cleanup    = kenwood_cleanup,
    .rig_open   = thf7e_open,
    .rig_close  = kenwood_close,

    .set_freq   = th_set_freq,
    .get_freq   = th_get_freq,
    .set_mode   = th_set_mode,
    .get_mode   = th_get_mode,
    .set_vfo    = th_set_vfo,
    .get_vfo    = thf7e_get_vfo,
    .set_ptt    = th_set_ptt,
    .get_dcd    = th_get_dcd,
    .vfo_op     = thf7e_vfo_op,
    .set_mem    = th_set_mem,
    .get_mem    = th_get_mem,

    .set_func   = th_set_func,
    .get_func   = th_get_func,

    .set_level  = th_set_level,
    .get_level  = th_get_level,

    .get_parm   = th_get_parm,
    .set_parm   = th_set_parm,

    .get_info   = th_get_info,

    .set_channel    = th_set_channel,
    .get_channel    = th_get_channel,
    .set_ant    = th_set_ant,
    .get_ant    = th_get_ant,

    .reset      = th_reset,
    .hamlib_check_rig_caps = HAMLIB_CHECK_RIG_CAPS
};


int thf7e_init(RIG *rig)
{
    return kenwood_init(rig);
}

int thf7e_open(RIG *rig)
{
    return kenwood_open(rig);
}

/*
 * th_get_vfo
 * Assumes rig!=NULL
 */
int
thf7e_get_vfo(RIG *rig, vfo_t *vfo)
{
    char vfoch;
    int retval;

    rig_debug(RIG_DEBUG_TRACE, "%s: called\n", __func__);

    retval = th_get_vfo_char(rig, vfo, &vfoch);

    if (retval != RIG_OK)
    {
        return retval;
    }

    switch (vfoch)
    {
    case '0' :
    case '3' :  /* Fine Step Enable */
        break;

    case '1' :  /* MR */
    case '2' :  /* CALL */
    case '4' :  /* INFO */
        *vfo = RIG_VFO_MEM;
        break;

    default:
        rig_debug(RIG_DEBUG_ERR, "%s: Unexpected VFO value '%c'\n", __func__, vfoch);
        return -RIG_EVFO;
    }

    return RIG_OK;
}

/*
 * thf7e_vfo_op
 */
int thf7e_vfo_op(RIG *rig, vfo_t vfo, vfo_op_t op)
{
    rig_debug(RIG_DEBUG_VERBOSE, "%s called\n", __func__);

    if (!rig)
    {
        return -RIG_EINVAL;
    }

    switch (op)
    {
    case RIG_OP_UP:
        return kenwood_transaction(rig, "UP", NULL, 0);

    case RIG_OP_DOWN:
        return kenwood_transaction(rig, "DW", NULL, 0);

    /*  Not implemented!
        case RIG_OP_BAND_UP:
            return kenwood_transaction(rig, "BU", NULL, 0);

        case RIG_OP_BAND_DOWN:
            return kenwood_transaction(rig, "BD", NULL, 0);
    */
    default:
        rig_debug(RIG_DEBUG_ERR, "%s: unsupported op %#x\n",
                  __func__, op);
        return -RIG_EINVAL;
    }
}


