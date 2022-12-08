/*
 *  Hamlib CI-V backend - description of IC-R20
 *  Copyright (c) 2004 by Stephane Fillod
 *  Copyright (c) 2004 by Malcolm Herring
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

#define ICR20_MODES (RIG_MODE_AM|RIG_MODE_CW|RIG_MODE_SSB|RIG_MODE_FM|RIG_MODE_WFM)

#define ICR20_FUNC_ALL (RIG_FUNC_NONE)

#define ICR20_LEVEL_ALL (RIG_LEVEL_RAWSTR)

#define ICR20_VFO_ALL (RIG_VFO_A)

#define ICR20_VFO_OPS (RIG_OP_NONE)
#define ICR20_SCAN_OPS (RIG_SCAN_NONE)

#define ICR20_STR_CAL { 2, \
  { \
    {  0, -60 }, /* S0 */ \
    { 255, 60 } /* +60 */ \
  } }

static struct icom_priv_caps icr20_priv_caps =
{
    0x6c, /* default address */
    0,    /* 731 mode */
    0,    /* no XCHG */
    r8500_ts_sc_list  /* wrong, but don't have set_ts anyway */
};

const struct rig_caps icr20_caps =
{
    RIG_MODEL(RIG_MODEL_ICR20),
    .model_name = "IC-R20",
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
    .has_get_func =  ICR20_FUNC_ALL,
    .has_set_func =  ICR20_FUNC_ALL,
    .has_get_level = ICR20_LEVEL_ALL,
    .has_set_level = RIG_LEVEL_NONE,
    .has_get_parm =  RIG_PARM_NONE,
    .has_set_parm =  RIG_PARM_NONE, /* FIXME: parms */
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
    .vfo_ops =  ICR20_VFO_OPS,
    .scan_ops =  ICR20_SCAN_OPS,
    .transceive =  RIG_TRN_RIG,
    .bank_qty =   0,
    .chan_desc_sz =  0,

    /* Only through cloning mode OPC-1382 */
    .chan_list =  {
        {    1,  999, RIG_MTYPE_MEM  },   /* TBC */
        { 1000, 1199, RIG_MTYPE_MEM },    /* auto-write */
        { 1200, 1249, RIG_MTYPE_EDGE },   /* two by two */
        RIG_CHAN_END,
    },

    .rx_range_list1 =   {   /* Other countries but France */
        {kHz(150), GHz(3.304999), ICR20_MODES, -1, -1, ICR20_VFO_ALL},
        RIG_FRNG_END,
    },
    .tx_range_list1 =   { RIG_FRNG_END, },

    .rx_range_list2 =   {   /* USA */
        {kHz(150), MHz(821.999), ICR20_MODES, -1, -1, ICR20_VFO_ALL},
        {MHz(851), MHz(866.999), ICR20_MODES, -1, -1, ICR20_VFO_ALL},
        {MHz(896), GHz(1.304999), ICR20_MODES, -1, -1, ICR20_VFO_ALL},
        {GHz(1.305), GHz(3.304999), ICR20_MODES, -1, -1, ICR20_VFO_ALL},
        RIG_FRNG_END,
    },
    .tx_range_list2 =   { RIG_FRNG_END, },

    .tuning_steps =     {
        {ICR20_MODES, Hz(100)},
        RIG_TS_END,
    },
    /* mode/filter list, remember: order matters! */
    .filters =  {
        {RIG_MODE_SSB | RIG_MODE_CW, kHz(1.8)},
        {RIG_MODE_AM | RIG_MODE_FM, kHz(12)},
        {RIG_MODE_WFM, kHz(150)},
        RIG_FLT_END,
    },
    .str_cal = ICR20_STR_CAL,

    .cfgparams =  icom_cfg_params,
    .set_conf =  icom_set_conf,
    .get_conf =  icom_get_conf,

    .priv = (void *)& icr20_priv_caps,
    .rig_init =   icom_init,
    .rig_cleanup =   icom_cleanup,
    .rig_open =  icom_rig_open,
    .rig_close =  icom_rig_close,

    .set_freq =  icom_set_freq,
    .get_freq =  icom_get_freq,
    .set_mode =  icom_set_mode, /* TODO: do not pass bandwidth data */
    .get_mode =  icom_get_mode,
    /* .set_vfo =  icom_set_vfo, */

    .decode_event =  icom_decode_event,
    .get_level =  icom_get_level,
    .get_dcd =  icom_get_dcd,

    .hamlib_check_rig_caps = HAMLIB_CHECK_RIG_CAPS
};


