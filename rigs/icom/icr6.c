/*
 *  Hamlib CI-V backend - description of IC-R6
 *  Copyright (c) 2017 Malcolm Herring
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


#include "hamlib/rig.h"
#include "icom.h"
#include "idx_builtin.h"

#define ICR6_MODES (RIG_MODE_AM|RIG_MODE_FM|RIG_MODE_WFM)

#define ICR6_FUNC_ALL (RIG_FUNC_TSQL|RIG_FUNC_VSC|RIG_FUNC_CSQL|RIG_FUNC_AFLT|RIG_FUNC_DSQL)

#define ICR6_LEVEL_ALL (RIG_LEVEL_ATT|RIG_LEVEL_AF|RIG_LEVEL_SQL|RIG_LEVEL_RAWSTR|RIG_LEVEL_STRENGTH)

#define ICR6_VFO_ALL (RIG_VFO_A)

#define ICR6_VFO_OPS (RIG_OP_NONE)
#define ICR6_SCAN_OPS (RIG_SCAN_NONE)

#define ICR6_STR_CAL { 2, \
    { \
        {  0, -60 }, /* S0 */ \
        { 255, 60 } /* +60 */ \
    } }

static struct icom_priv_caps icr6_priv_caps =
{
    0x7e,   /* default address */
    0,      /* 731 mode */
    0,    /* no XCHG */
    r8500_ts_sc_list,    /* wrong, but don't have set_ts anyway */
    .antack_len = 2,
    .ant_count = 2
};

const struct rig_caps icr6_caps =
{
    RIG_MODEL(RIG_MODEL_ICR6),
    .model_name = "IC-R6",
    .mfg_name =  "Icom",
    .version =  BACKEND_VER ".0",
    .copyright =  "LGPL",
    .status =  RIG_STATUS_STABLE,
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
    .has_get_func =  ICR6_FUNC_ALL,
    .has_set_func =  ICR6_FUNC_ALL,
    .has_get_level =  ICR6_LEVEL_ALL,
    .has_set_level =  RIG_LEVEL_SET(ICR6_LEVEL_ALL),
    .has_get_parm =  RIG_PARM_NONE,
    .has_set_parm =  RIG_PARM_NONE,
    .level_gran = {
        // cppcheck-suppress *
        [LVL_RAWSTR] = { .min = { .i = 0 }, .max = { .i = 255 } },
    },
    .parm_gran =  {},
    .ctcss_list =  common_ctcss_list,
    .dcs_list =  common_dcs_list,
    .preamp =   { RIG_DBLST_END, },
    .attenuator =   { RIG_DBLST_END, },
    .max_rit =  Hz(0),
    .max_xit =  Hz(0),
    .max_ifshift =  Hz(0),
    .targetable_vfo =  0,
    .vfo_ops =  ICR6_VFO_OPS,
    .scan_ops =  ICR6_SCAN_OPS,
    .transceive =  RIG_TRN_RIG,
    .bank_qty =   0,
    .chan_desc_sz =  0,

    .chan_list =  {RIG_CHAN_END,},

    .rx_range_list1 =   {   /* Other countries but France */
        {kHz(100), GHz(1.309995), ICR6_MODES, -1, -1, ICR6_VFO_ALL},
        RIG_FRNG_END,
    },
    .tx_range_list1 =   { RIG_FRNG_END, },

    .rx_range_list2 =   {   /* USA */
        {kHz(100), MHz(821.995), ICR6_MODES, -1, -1, ICR6_VFO_ALL},
        {MHz(851), MHz(866.995), ICR6_MODES, -1, -1, ICR6_VFO_ALL},
        {MHz(896), GHz(1.309995), ICR6_MODES, -1, -1, ICR6_VFO_ALL},
        RIG_FRNG_END,
    },
    .tx_range_list2 =   { RIG_FRNG_END, },

    .tuning_steps =     {
        {ICR6_MODES, Hz(5000)},
        {ICR6_MODES, Hz(6250)},
        {ICR6_MODES, Hz(8330)}, // Air band only
        {ICR6_MODES, Hz(9000)}, // AM broadcast band only
        {ICR6_MODES, Hz(10000)},
        {ICR6_MODES, Hz(12500)},
        {ICR6_MODES, kHz(15)},
        {ICR6_MODES, kHz(20)},
        {ICR6_MODES, kHz(25)},
        {ICR6_MODES, kHz(30)},
        {ICR6_MODES, kHz(50)},
        {ICR6_MODES, kHz(100)},
        {ICR6_MODES, kHz(125)},
        {ICR6_MODES, kHz(200)},
        RIG_TS_END,
    },

    /* mode/filter list, remember: order matters! */
    .filters =  {
        {RIG_MODE_AM | RIG_MODE_FM, kHz(12)},
        {RIG_MODE_WFM, kHz(150)},
        RIG_FLT_END,
    },
    .str_cal = ICR6_STR_CAL,

    .cfgparams =  icom_cfg_params,
    .set_conf =  icom_set_conf,
    .get_conf =  icom_get_conf,

    .priv = (void *)& icr6_priv_caps,
    .rig_init =   icom_init,
    .rig_cleanup =   icom_cleanup,
    .rig_open =  icom_rig_open,
    .rig_close =  icom_rig_close,

    .set_freq =  icom_set_freq,
    .get_freq =  icom_get_freq,
    .set_mode =  icom_set_mode,
    .get_mode =  icom_get_mode,
    .set_ant =  icom_set_ant,
    .get_ant =  icom_get_ant,
    .decode_event =  icom_decode_event,
    .set_level =  icom_set_level,
    .get_level =  icom_get_level,
    .set_func =  icom_set_func,
    .get_func =  icom_get_func,
    .get_dcd =  icom_get_dcd,
    .set_ctcss_sql =  icom_set_ctcss_sql,
    .get_ctcss_sql =  icom_get_ctcss_sql,
    .set_dcs_sql =  icom_set_dcs_sql,
    .get_dcs_sql =  icom_get_dcs_sql,
    .hamlib_check_rig_caps = HAMLIB_CHECK_RIG_CAPS
};
