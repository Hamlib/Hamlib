/*
 *  Hamlib CI-V backend - description of IC-R72
 *  Copyright (c) 2000-2010 by Stephane Fillod
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

#define ICR72_MODES (RIG_MODE_AM|RIG_MODE_CW|RIG_MODE_SSB|RIG_MODE_FM|RIG_MODE_RTTY)

#define ICR72_FUNC_ALL (RIG_FUNC_NONE)

#define ICR72_LEVEL_ALL (RIG_LEVEL_NONE)

#define ICR72_VFO_OPS (RIG_OP_TO_VFO|RIG_OP_FROM_VFO|RIG_OP_MCL)

#define ICR72_VFO_ALL (RIG_VFO_A|RIG_VFO_MEM)

#define ICR71_VFO_OPS (RIG_OP_FROM_VFO|RIG_OP_TO_VFO|RIG_OP_MCL)
#define ICR72_SCAN_OPS (RIG_SCAN_MEM|RIG_SCAN_VFO|RIG_SCAN_SLCT|RIG_SCAN_PRIO)

static struct icom_priv_caps icr72_priv_caps =
{
    0x32,   /* default address */
    0,      /* 731 mode */
    0,    /* no XCHG */
    ic737_ts_sc_list
};

const struct rig_caps icr72_caps =
{
    RIG_MODEL(RIG_MODEL_ICR72),
    .model_name = "IC-R72",
    .mfg_name =  "Icom",
    .version =  BACKEND_VER ".0",
    .copyright =  "LGPL",
    .status =  RIG_STATUS_ALPHA,
    .rig_type =  RIG_TYPE_RECEIVER,
    .ptt_type =  RIG_PTT_NONE,
    .dcd_type =  RIG_DCD_RIG,
    .port_type =  RIG_PORT_SERIAL,
    .serial_rate_min =  300,
    .serial_rate_max =  9600,
    .serial_data_bits =  8,
    .serial_stop_bits =  1,
    .serial_parity =  RIG_PARITY_NONE,
    .serial_handshake =  RIG_HANDSHAKE_NONE,
    .write_delay =  0,
    .post_write_delay =  0,
    .timeout =  1000,
    .retry =  3,
    .has_get_func =  ICR72_FUNC_ALL,
    .has_set_func =  ICR72_FUNC_ALL,
    .has_get_level =  ICR72_LEVEL_ALL,
    .has_set_level =  RIG_LEVEL_SET(ICR72_LEVEL_ALL),
    .has_get_parm =  RIG_PARM_ANN,
    .has_set_parm =  RIG_PARM_ANN,
    .level_gran =  {},      /* granularity */
    .parm_gran =  {},
    .ctcss_list =  NULL,
    .dcs_list =  NULL,
    .preamp =   { RIG_DBLST_END, },
    .attenuator =   { RIG_DBLST_END, },
    .max_rit =  Hz(0),
    .max_xit =  Hz(0),
    .max_ifshift =  Hz(0),
    .targetable_vfo =  0,
    .vfo_ops =  ICR72_VFO_OPS,
    .scan_ops =  ICR72_SCAN_OPS,
    .transceive =  RIG_TRN_RIG,
    .bank_qty =   0,
    .chan_desc_sz =  0,

    .chan_list =  {
        {   1,  99, RIG_MTYPE_MEM, IC_MIN_MEM_CAP  },
        RIG_CHAN_END,
    },

    .rx_range_list1 =   {
        {kHz(100), MHz(30), ICR72_MODES, -1, -1, ICR72_VFO_ALL},
        RIG_FRNG_END,
    },
    .tx_range_list1 =   { RIG_FRNG_END, },

    .rx_range_list2 =   {
        {kHz(100), MHz(30), ICR72_MODES, -1, -1, ICR72_VFO_ALL},
        RIG_FRNG_END,
    },
    .tx_range_list2 =   { RIG_FRNG_END, },

    .tuning_steps =     {
        {ICR72_MODES, Hz(10)},
        {ICR72_MODES, kHz(1)},
        {ICR72_MODES, kHz(2)},
        {ICR72_MODES, kHz(3)},
        {ICR72_MODES, kHz(4)},
        {ICR72_MODES, kHz(5)},
        {ICR72_MODES, kHz(6)},
        {ICR72_MODES, kHz(7)},
        {ICR72_MODES, kHz(8)},
        {ICR72_MODES, kHz(9)},
        {ICR72_MODES, kHz(10)},
        RIG_TS_END,
    },
    /* mode/filter list, remember: order matters! */
    .filters =  {
        {RIG_MODE_SSB | RIG_MODE_CW | RIG_MODE_RTTY, kHz(2.3)},
        {RIG_MODE_AM, kHz(6)},
        {RIG_MODE_FM, kHz(15)},
        RIG_FLT_END,
    },

    .cfgparams =  icom_cfg_params,
    .set_conf =  icom_set_conf,
    .get_conf =  icom_get_conf,

    .priv = (void *)& icr72_priv_caps,
    .rig_init =   icom_init,
    .rig_cleanup =   icom_cleanup,
    .rig_open =  icom_rig_open,
    .rig_close =  icom_rig_close,

    .set_freq =  icom_set_freq,
    .get_freq =  icom_get_freq,
    .set_mode =  icom_set_mode,
    .get_mode =  icom_get_mode,
    .set_vfo =  icom_set_vfo,

    .decode_event =  icom_decode_event,
    .get_level =  icom_get_level,
    .set_mem =  icom_set_mem,
    .vfo_op =  icom_vfo_op,
    .get_dcd =  icom_get_dcd,
    .set_ts =  icom_set_ts,
    .get_ts =  icom_get_ts,
    .scan =  icom_scan,

    .hamlib_check_rig_caps = HAMLIB_CHECK_RIG_CAPS
};

