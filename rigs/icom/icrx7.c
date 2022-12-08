/*
 *  Hamlib CI-V backend - description of IC-RX7
 *  Copyright (c) 2011 by Stephane Fillod
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

#define ICRX7_MODES (RIG_MODE_AM|RIG_MODE_FM|RIG_MODE_WFM)

#define ICRX7_FUNC_ALL (RIG_FUNC_NONE)

#define ICRX7_LEVEL_ALL (RIG_LEVEL_RAWSTR)

#define ICRX7_VFO_ALL (RIG_VFO_A)

#define ICRX7_VFO_OPS (RIG_OP_NONE)
#define ICRX7_SCAN_OPS (RIG_SCAN_NONE)

/*
 * FIXME: S-meter measurement
 */
#define ICRX7_STR_CAL UNKNOWN_IC_STR_CAL

static struct icom_priv_caps icrx7_priv_caps =
{
    0x78,   /* default address */
    0,      /* 731 mode */
    0,    /* no XCHG */
    r8500_ts_sc_list    /* wrong, but don't have set_ts anyway */
};

const struct rig_caps icrx7_caps =
{
    RIG_MODEL(RIG_MODEL_ICRX7),
    .model_name = "IC-RX7",
    .mfg_name =  "Icom",
    .version =  BACKEND_VER ".0",
    .copyright =  "LGPL",
    .status =  RIG_STATUS_ALPHA,
    .rig_type =  RIG_TYPE_RECEIVER | RIG_FLAG_HANDHELD,
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
    .has_get_func =  ICRX7_FUNC_ALL,
    .has_set_func =  ICRX7_FUNC_ALL,
    .has_get_level =  ICRX7_LEVEL_ALL,
    .has_set_level =  RIG_LEVEL_SET(ICRX7_LEVEL_ALL),
    .has_get_parm =  RIG_PARM_NONE,
    .has_set_parm =  RIG_PARM_NONE,
    .level_gran =
    {
#include "level_gran_icom.h"
        // cppcheck-suppress *
        [LVL_RAWSTR] = { .min = { .i = 0 }, .max = { .i = 255 } },
    },
    .parm_gran =  {},
    .ctcss_list =  NULL,
    .dcs_list =  NULL,
    .preamp =   { RIG_DBLST_END, },
    .attenuator =   { RIG_DBLST_END, },
    .max_rit =  Hz(0),
    .max_xit =  Hz(0),
    .max_ifshift =  Hz(0),
    .targetable_vfo =  0,
    .vfo_ops =  ICRX7_VFO_OPS,
    .scan_ops =  ICRX7_SCAN_OPS,
    .transceive =  RIG_TRN_RIG,
    .bank_qty =   0,
    .chan_desc_sz =  0,

    .chan_list =  {
        /* Unfortunately, not accessible through CI-V */
        RIG_CHAN_END,
    },

    .rx_range_list1 =   {
        {kHz(150), GHz(1.3), ICRX7_MODES, -1, -1, ICRX7_VFO_ALL},
        RIG_FRNG_END,
    },
    .tx_range_list1 =   { RIG_FRNG_END, },

    .rx_range_list2 =   {
        {kHz(150), MHz(821.995), ICRX7_MODES, -1, -1, ICRX7_VFO_ALL},
        {MHz(851), MHz(866.995), ICRX7_MODES, -1, -1, ICRX7_VFO_ALL},
        {MHz(896), GHz(1.3), ICRX7_MODES, -1, -1, ICRX7_VFO_ALL},
        RIG_FRNG_END,
    },
    .tx_range_list2 =   { RIG_FRNG_END, },

    .tuning_steps =     {
        {ICRX7_MODES, Hz(100)},
        RIG_TS_END,
    },
    .filters =  {
        {RIG_MODE_AM | RIG_MODE_FM, kHz(15)},
        {RIG_MODE_WFM, kHz(150)},
        RIG_FLT_END,
    },
    .str_cal = ICRX7_STR_CAL,

    .cfgparams =  icom_cfg_params,
    .set_conf =  icom_set_conf,
    .get_conf =  icom_get_conf,

    .priv = (void *)& icrx7_priv_caps,
    .rig_init =   icom_init,
    .rig_cleanup =   icom_cleanup,
    .rig_open =  icom_rig_open,
    .rig_close =  icom_rig_close,

    .set_freq =  icom_set_freq,
    .get_freq =  icom_get_freq,
    .set_mode =  icom_set_mode, /* TODO: do not pass bandwidth data */
    .get_mode =  icom_get_mode,

    .decode_event =  icom_decode_event,
    .get_level =  icom_get_level,
    .get_dcd =  icom_get_dcd,

    .hamlib_check_rig_caps = HAMLIB_CHECK_RIG_CAPS
};

