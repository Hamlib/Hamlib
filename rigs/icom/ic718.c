/*
 *  Hamlib CI-V backend - description of IC-718 caps
 *  Copyright (c) 2000-2010 by Stephane Fillod
 *  Caps submitted by Chuck Gilkes WD0FCL/4
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

#include "hamlib/rig.h"
#include "icom.h"
#include "idx_builtin.h"
#include "bandplan.h"


#define IC718_ALL_RX_MODES (RIG_MODE_AM|RIG_MODE_CW|RIG_MODE_CWR|RIG_MODE_SSB|RIG_MODE_RTTY|RIG_MODE_RTTYR)
#define IC718_1MHZ_TS_MODES (RIG_MODE_AM)
#define IC718_1HZ_TS_MODES (RIG_MODE_CW|RIG_MODE_CWR|RIG_MODE_SSB|RIG_MODE_RTTY|RIG_MODE_RTTYR)

/* tx doesn't have WFM.
 * 100W in all modes but AM (40W)
 */
#define IC718_OTHER_TX_MODES (RIG_MODE_AM|RIG_MODE_CW|RIG_MODE_CWR|RIG_MODE_SSB|RIG_MODE_RTTY|RIG_MODE_RTTYR)
#define IC718_AM_TX_MODES (RIG_MODE_AM)

#define IC718_FUNC_ALL (RIG_FUNC_NR|RIG_FUNC_NB|RIG_FUNC_COMP|RIG_FUNC_VOX|RIG_FUNC_TONE|RIG_FUNC_TSQL|RIG_FUNC_SBKIN|RIG_FUNC_FBKIN|RIG_FUNC_ANF)

#define IC718_LEVEL_ALL (RIG_LEVEL_PREAMP|RIG_LEVEL_ATT|RIG_LEVEL_MICGAIN|RIG_LEVEL_NR|RIG_LEVEL_CWPITCH|RIG_LEVEL_KEYSPD|RIG_LEVEL_RFPOWER|RIG_LEVEL_AF|RIG_LEVEL_RF|RIG_LEVEL_RAWSTR|RIG_LEVEL_SQL)

#define IC718_VFO_ALL (RIG_VFO_A|RIG_VFO_B|RIG_VFO_MEM)

#define IC718_VFO_OPS (RIG_OP_CPY|RIG_OP_XCHG|RIG_OP_FROM_VFO|RIG_OP_TO_VFO|RIG_OP_MCL)

#define IC718_SCAN_OPS (RIG_SCAN_MEM)

/*
 * TODO: check whether func and levels are stored in memory
 */
#define IC718_MEM_CAP {    \
        .freq = 1,  \
        .mode = 1,  \
        .width = 1, \
        .tx_freq = 1,   \
        .tx_mode = 1,   \
        .tx_width = 1,  \
}

#define IC718_STR_CAL UNKNOWN_IC_STR_CAL



static const struct icom_priv_caps IC718_priv_caps =
{
    0x5e,   /* default address */
    0,      /* 731 mode */
    0,    /* no XCHG */
    ic718_ts_sc_list
};

const struct rig_caps ic718_caps =
{
    RIG_MODEL(RIG_MODEL_IC718),
    .model_name = "IC-718",
    .mfg_name =  "Icom",
    .version =  BACKEND_VER ".1",
    .copyright =  "LGPL",
    .status =  RIG_STATUS_STABLE,
    .rig_type =   RIG_TYPE_TRANSCEIVER,
    .ptt_type =  RIG_PTT_NONE,
    .dcd_type =  RIG_DCD_RIG,
    .port_type =  RIG_PORT_SERIAL,
    .serial_rate_min =  300,
    .serial_rate_max =  19200,
    .serial_data_bits =  8,
    .serial_stop_bits =  1,
    .serial_parity =  RIG_PARITY_NONE,
    .serial_handshake =  RIG_HANDSHAKE_NONE,
    .write_delay =  0,
    .post_write_delay =  0,
    .timeout =  1000,
    .retry =  3,
    .has_get_func =  IC718_FUNC_ALL,
    .has_set_func =  IC718_FUNC_ALL,
    .has_get_level =  IC718_LEVEL_ALL,
    .has_set_level =  RIG_LEVEL_SET(IC718_LEVEL_ALL),
    .has_get_parm =  RIG_PARM_NONE,
    .has_set_parm =  RIG_PARM_NONE,
    .level_gran =
    {
#include "level_gran_icom.h"
        // cppcheck-suppress *
        [LVL_RAWSTR] = { .min = { .i = 0 }, .max = { .i = 255 } },
    },
    .parm_gran =  {},
    .str_cal = IC718_STR_CAL,
    .ctcss_list =  NULL,
    .dcs_list =  NULL,
    .preamp =   { 10, RIG_DBLST_END, },
    .attenuator =   { 20, RIG_DBLST_END, },
    .max_rit =  Hz(0),
    .max_xit =  Hz(0),
    .max_ifshift =  Hz(0),
    .targetable_vfo =  0,
    .vfo_ops =  IC718_VFO_OPS,
    .scan_ops =  IC718_SCAN_OPS,
    .transceive =  RIG_TRN_RIG,
    .bank_qty =   0,
    .chan_desc_sz =  0,

    .chan_list =  {
        {   1,  99, RIG_MTYPE_MEM,  IC718_MEM_CAP },
        { 100, 101, RIG_MTYPE_EDGE, IC718_MEM_CAP },    /* two by two */
        RIG_CHAN_END,
    },

    .rx_range_list1 =   { {kHz(30), MHz(30) - 1, IC718_ALL_RX_MODES, -1, -1, IC718_VFO_ALL},
        RIG_FRNG_END,
    },
    .tx_range_list1 =   {
        FRQ_RNG_HF(1, IC718_OTHER_TX_MODES, W(5), W(100), IC718_VFO_ALL, RIG_ANT_1),
        FRQ_RNG_HF(1, IC718_AM_TX_MODES,    W(2), W(40), IC718_VFO_ALL, RIG_ANT_1), /* AM class */
        RIG_FRNG_END,
    },

    .rx_range_list2 =   { {kHz(30), MHz(30) - 1, IC718_ALL_RX_MODES, -1, -1, IC718_VFO_ALL},
        RIG_FRNG_END,
    },
    .tx_range_list2 =  {
        FRQ_RNG_HF(2, IC718_OTHER_TX_MODES, W(5), W(100), IC718_VFO_ALL, RIG_ANT_1),
        FRQ_RNG_HF(2, IC718_AM_TX_MODES,    W(2), W(40), IC718_VFO_ALL, RIG_ANT_1), /* AM class */
        RIG_FRNG_END,
    },

    .tuning_steps =     {
        {IC718_1HZ_TS_MODES, 1},
        {IC718_ALL_RX_MODES, 10},
        {IC718_ALL_RX_MODES, 100},
        {IC718_ALL_RX_MODES, kHz(1)},
        {IC718_ALL_RX_MODES, kHz(5)},
        {IC718_ALL_RX_MODES, kHz(9)},
        {IC718_ALL_RX_MODES, kHz(10)},
        {IC718_ALL_RX_MODES, kHz(100)},
        RIG_TS_END,
    },
    /* mode/filter list, remember: order matters! */
    .filters =  {
        {RIG_MODE_SSB | RIG_MODE_CW | RIG_MODE_RTTY, kHz(2.1)}, /* builtin */
        {RIG_MODE_CW | RIG_MODE_RTTY, Hz(500)},         /* FL-52A */
        {RIG_MODE_CW | RIG_MODE_RTTY, Hz(250)},         /* FL-53A */
        {RIG_MODE_SSB, kHz(2.8)},               /* FL-96  */
        {RIG_MODE_SSB | RIG_MODE_CW | RIG_MODE_CWR | RIG_MODE_RTTY | RIG_MODE_RTTYR, kHz(1.8)}, /* FL-222 */
        {RIG_MODE_AM, kHz(6)},                  /* mid w/ builtin FL-94 */
        {RIG_MODE_AM, kHz(2.4)},                /* narrow w/ builtin FL-272 */
        RIG_FLT_END,
    },

    .cfgparams =  icom_cfg_params,
    .set_conf =  icom_set_conf,
    .get_conf =  icom_get_conf,

    .priv = (void *)& IC718_priv_caps,
    .rig_init =   icom_init,
    .rig_cleanup =   icom_cleanup,
    .rig_open =  icom_rig_open,
    .rig_close =  icom_rig_close,

    .set_freq =  icom_set_freq,
    .get_freq =  icom_get_freq,
    .set_mode =  icom_set_mode,
    .get_mode =  icom_get_mode,
    .set_vfo =  icom_set_vfo,
    .get_vfo =  icom_get_vfo,

    .decode_event =  icom_decode_event,
    .set_level =  icom_set_level,
    .get_level =  icom_get_level,
    .set_func =  icom_set_func,
    .get_func =  icom_get_func,
    .set_mem =  icom_set_mem,
    .vfo_op =  icom_vfo_op,
    .scan =  icom_scan,
    .get_dcd =  icom_get_dcd,
    .set_ts =  icom_set_ts,
    .get_ts =  icom_get_ts,
    .set_split_freq =  icom_set_split_freq,
    .get_split_freq =  icom_get_split_freq,
    .set_split_mode =  icom_set_split_mode,
    .get_split_mode =  icom_get_split_mode,
    .set_split_vfo =  icom_set_split_vfo,
    .get_split_vfo =  icom_mem_get_split_vfo,

    .hamlib_check_rig_caps = HAMLIB_CHECK_RIG_CAPS
};


