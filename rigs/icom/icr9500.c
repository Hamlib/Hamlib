/*
 *  Hamlib CI-V backend - IC-R9500 descriptions
 *  Copyright (c) 2000-2011 by Stephane Fillod
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


#include <hamlib/rig.h>
#include "serial.h"
#include "misc.h"
#include "idx_builtin.h"

#include "icom.h"
#include "icom_defs.h"
#include "frame.h"


#define ICR9500_MODES (RIG_MODE_AM|RIG_MODE_AMS|\
        RIG_MODE_SSB|RIG_MODE_FM|RIG_MODE_RTTY|RIG_MODE_RTTYR|\
        RIG_MODE_CW|RIG_MODE_CWR|RIG_MODE_WFM)

#define ICR9500_OPS (RIG_OP_FROM_VFO|RIG_OP_TO_VFO|RIG_OP_MCL)

#define ICR9500_FUNCS (RIG_FUNC_NB|RIG_FUNC_TSQL|RIG_FUNC_NR|RIG_FUNC_MN|RIG_FUNC_ANF|RIG_FUNC_VSC|RIG_FUNC_LOCK)
#define ICR9500_LEVELS (RIG_LEVEL_PREAMP|RIG_LEVEL_ATT|RIG_LEVEL_AGC|RIG_LEVEL_NR|RIG_LEVEL_PBT_IN|RIG_LEVEL_PBT_OUT|RIG_LEVEL_CWPITCH|RIG_LEVEL_NOTCHF_RAW|RIG_LEVEL_SQL|RIG_LEVEL_RAWSTR|RIG_LEVEL_AF|RIG_LEVEL_RF|RIG_LEVEL_APF)
#define ICR9500_PARMS (RIG_PARM_ANN|RIG_PARM_BACKLIGHT)
#define ICR9500_SCAN_OPS (RIG_SCAN_MEM|RIG_SCAN_VFO|RIG_SCAN_SLCT|RIG_SCAN_PRIO)    /* TBC */

#define ICR9500_HF_ANTS (RIG_ANT_1|RIG_ANT_2|RIG_ANT_3)

#define ICR9500_VFOS (RIG_VFO_A|RIG_VFO_MEM)

#define ICR9500_MEM_CAP { /* FIXME */   \
        .freq = 1,  \
        .mode = 1,  \
        .width = 1, \
        .levels = RIG_LEVEL_ATT, \
}


/* TODO: S-Meter measurements */
/* This is just a copy if IC9700_STR_CAL */
/* Hope it's correct */
#define ICR9500_STR_CAL { 7, \
    { \
        {   0, -54 }, \
        {  10, -48 }, \
        {  30, -36 }, \
        {  60, -24 }, \
        {  90, -12 }, \
        { 120,  0 }, \
        { 241,  64 } \
    } }


static struct icom_priv_caps icr9500_priv_caps =
{
    0x72,   /* default address */
    0,      /* 731 mode */
    0,      /* no XCHG */
    .ts_sc_list = r9500_ts_sc_list,
    .antack_len = 2,
    .ant_count = 3
};

/*
 * ICR9500A rig capabilities.
 */
const struct rig_caps icr9500_caps =
{
    RIG_MODEL(RIG_MODEL_ICR9500),
    .model_name = "IC-R9500",
    .mfg_name =  "Icom",
    .version =  BACKEND_VER ".0",
    .copyright =  "LGPL",
    .status =  RIG_STATUS_STABLE,
    .rig_type =  RIG_TYPE_RECEIVER,
    .ptt_type =  RIG_PTT_NONE,
    .dcd_type =  RIG_DCD_RIG,
    .port_type =  RIG_PORT_SERIAL,
    .serial_rate_min =  300,
    .serial_rate_max =  1200,
    .serial_data_bits =  8,
    .serial_stop_bits =  1,
    .serial_parity =  RIG_PARITY_NONE,
    .serial_handshake =  RIG_HANDSHAKE_NONE,
    .write_delay =  0,
    .post_write_delay =  0,
    .timeout =  1000,
    .retry =  3,

    .has_get_func =  ICR9500_FUNCS,
    .has_set_func =  ICR9500_FUNCS,
    .has_get_level =  ICR9500_LEVELS,
    .has_set_level =  RIG_LEVEL_SET(ICR9500_LEVELS),
    .has_get_parm =  ICR9500_PARMS,
    .has_set_parm =  RIG_PARM_SET(ICR9500_PARMS),
    .level_gran =
    {
#include "level_gran_icom.h"
        // cppcheck-suppress *
        [LVL_RAWSTR] = { .min = { .i = 0 }, .max = { .i = 255 } },
    },
    .parm_gran =  {},
    .ctcss_list =  common_ctcss_list,
    .dcs_list =  common_dcs_list,
    .preamp =   { 10, 20, RIG_DBLST_END },  /* FIXME: TBC */
    .attenuator =   { 6, 10, 12, 18, 20, 24, 30, RIG_DBLST_END },
    .max_rit =  Hz(0),
    .max_xit =  Hz(0),
    .max_ifshift =  Hz(0),
    .targetable_vfo =  0,
    .vfo_ops =  ICR9500_OPS,
    .scan_ops =  ICR9500_SCAN_OPS,
    .transceive =  RIG_TRN_RIG,
    .bank_qty =   12,
    .chan_desc_sz =  0, /* FIXME */

    .chan_list =  {
        {    0,  999, RIG_MTYPE_MEM,  ICR9500_MEM_CAP },    /* TBC */
        { 1000, 1099, RIG_MTYPE_EDGE, IC_MIN_MEM_CAP }, /* Bank-A (Auto) */
        { 1100, 1199, RIG_MTYPE_EDGE, IC_MIN_MEM_CAP }, /* Bank-S (skip) */
        { 1200, 1219, RIG_MTYPE_EDGE, IC_MIN_MEM_CAP }, /* Bank-P (Scan edge), 2 by 2 */
        RIG_CHAN_END,
    },

    .rx_range_list1 =  {
        {kHz(5), MHz(30) - 1, ICR9500_MODES, -1, -1, ICR9500_VFOS, ICR9500_HF_ANTS},
        {MHz(30), MHz(3335), ICR9500_MODES, -1, -1, ICR9500_VFOS, RIG_ANT_4},
        RIG_FRNG_END,
    },
    .tx_range_list1 =  { RIG_FRNG_END, },

    .rx_range_list2 =  {
        {kHz(5), MHz(30) - 1, ICR9500_MODES, -1, -1, ICR9500_VFOS, ICR9500_HF_ANTS},
        {MHz(30), MHz(3335), ICR9500_MODES, -1, -1, ICR9500_VFOS, RIG_ANT_4},
        RIG_FRNG_END,
    },
    .tx_range_list2 =  { RIG_FRNG_END, },   /* no TX ranges, this is a receiver */

    .tuning_steps =  {
        {ICR9500_MODES, 1}, /* resolution */
        {ICR9500_MODES, 10},
        {ICR9500_MODES, 100},
        {ICR9500_MODES, kHz(1)},
        {ICR9500_MODES, kHz(2.5)},
        {ICR9500_MODES, kHz(5)},
        {ICR9500_MODES, kHz(6.25)},
        {ICR9500_MODES, kHz(9)},
        {ICR9500_MODES, kHz(10)},
        {ICR9500_MODES, 12500},
        {ICR9500_MODES, kHz(20)},
        {ICR9500_MODES, kHz(25)},
        {ICR9500_MODES, kHz(100)},
        {ICR9500_MODES, MHz(1)},
        RIG_TS_END,
    },
    /* mode/filter list, remember: order matters! */
    .filters =  {
        {RIG_MODE_SSB | RIG_MODE_CW | RIG_MODE_RTTY, kHz(2.4)},
        {RIG_MODE_CW, Hz(500)},
        {RIG_MODE_AM | RIG_MODE_AMS, kHz(6)},
        {RIG_MODE_FM, kHz(15)},
        {RIG_MODE_WFM, kHz(350)},
        RIG_FLT_END,
    },
    .str_cal = ICR9500_STR_CAL,

    .cfgparams =  icom_cfg_params,
    .set_conf =  icom_set_conf,
    .get_conf =  icom_get_conf,

    .priv = (void *)& icr9500_priv_caps,
    .rig_init =   icom_init,
    .rig_cleanup =   icom_cleanup,
    .rig_open =  icom_rig_open,
    .rig_close =  icom_rig_close,

    .set_freq =  icom_set_freq,
    .get_freq =  icom_get_freq,
    .set_mode =  icom_set_mode,
    .get_mode =  icom_get_mode,
    .set_vfo =  icom_set_vfo,
    .set_ant =  icom_set_ant,
    .get_ant =  icom_get_ant,
    .set_rptr_shift =  icom_set_rptr_shift,
    .get_rptr_shift =  icom_get_rptr_shift,
    .set_rptr_offs =  icom_set_rptr_offs,
    .get_rptr_offs =  icom_get_rptr_offs,
    .set_ctcss_sql =  icom_set_ctcss_sql,
    .get_ctcss_sql =  icom_get_ctcss_sql,
    .set_dcs_sql =  icom_set_dcs_sql,
    .get_dcs_sql =  icom_get_dcs_sql,

    .set_ts =  icom_set_ts,
    .get_ts =  icom_get_ts,
    .set_func =  icom_set_func,
    .get_func =  icom_get_func,
    .set_level =  icom_set_level,
    .get_level =  icom_get_level,
    .set_parm =  icom_set_parm,
    .get_parm =  icom_get_parm,

    .decode_event =  icom_decode_event,
    .set_mem =  icom_set_mem,
    .set_bank =  icom_set_bank,
    .vfo_op =  icom_vfo_op,
    .scan =  icom_scan,
    .get_dcd =  icom_get_dcd,
    .hamlib_check_rig_caps = HAMLIB_CHECK_RIG_CAPS
};


/*
 * Function definitions below
 */


