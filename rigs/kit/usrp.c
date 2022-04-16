/*
 *  Hamlib KIT backend - Universal Software Radio Peripheral description
 *  Copyright (c) 2005 by Stephane Fillod
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

/*
 * Compile only this model if usrp is available
 */
#if defined(HAVE_USRP)


#include <stdlib.h>
#include <stdio.h>
#include <errno.h>
#include "hamlib/rig.h"

#include "usrp_impl.h"
#include "token.h"


#define USRP_MODES (RIG_MODE_NONE)

#define USRP_FUNC (RIG_FUNC_NONE)

#define USRP_LEVEL_ALL (RIG_LEVEL_NONE)

#define USRP_PARM_ALL (RIG_PARM_NONE)

#define USRP_VFO (RIG_VFO_A|RIG_VFO_B|RIG_VFO_C|RIG_VFO_N(3))


static const struct confparams usrp_cfg_params[] =
{
    {
        TOK_IFMIXFREQ, "if_mix_freq", "IF", "IF mixing frequency in Hz",
        "45000000", RIG_CONF_NUMERIC, { .n = { 0, MHz(400), 1 } }
    },
    { RIG_CONF_END, NULL }
};


/*
 * GNU Radio Universal Software Radio Peripheral
 *
 */

const struct rig_caps usrp_caps =
{
    RIG_MODEL(RIG_MODEL_USRP),
    .model_name = "USRP",
    .mfg_name =  "GNU Radio",
    .version =  "0.1",
    .copyright =  "LGPL",
    .status =  RIG_STATUS_BETA,
    .rig_type =  RIG_TYPE_TUNER,
    .ptt_type =  RIG_PTT_NONE,
    .dcd_type =  RIG_DCD_NONE,
    .port_type =  RIG_PORT_NONE,
    .serial_rate_min =  9600,   /* don't care */
    .serial_rate_max =  9600,
    .serial_data_bits =  8,
    .serial_stop_bits =  1,
    .serial_parity =  RIG_PARITY_NONE,
    .serial_handshake =  RIG_HANDSHAKE_NONE,
    .write_delay =  0,
    .post_write_delay =  0,
    .timeout =  200,
    .retry = 0,

    .has_get_func =  USRP_FUNC,
    .has_set_func =  USRP_FUNC,
    .has_get_level =  USRP_LEVEL_ALL,
    .has_set_level =  RIG_LEVEL_SET(USRP_LEVEL_ALL),
    .has_get_parm =  USRP_PARM_ALL,
    .has_set_parm =  RIG_PARM_SET(USRP_PARM_ALL),
    .level_gran =  {},
    .parm_gran =  {},
    .ctcss_list =  NULL,
    .dcs_list =  NULL,
    .preamp =   { RIG_DBLST_END },
    .attenuator =   { RIG_DBLST_END },
    .max_rit =  Hz(0),
    .max_xit =  Hz(0),
    .max_ifshift =  Hz(0),
    .targetable_vfo =  0,
    .transceive =  RIG_TRN_OFF,
    .bank_qty =   0,
    .chan_desc_sz =  0,

    .chan_list =  { RIG_CHAN_END, },

    .rx_range_list1 =  {
        {kHz(150), MHz(30), USRP_MODES, -1, -1, USRP_VFO},
        {kHz(87.5), MHz(108), RIG_MODE_WFM, -1, -1, USRP_VFO},
        RIG_FRNG_END,
    },
    .tx_range_list1 =  { RIG_FRNG_END, },
    .rx_range_list2 =  {
        {kHz(150), MHz(30), USRP_MODES, -1, -1, USRP_VFO},
        {kHz(87.5), MHz(108), RIG_MODE_WFM, -1, -1, USRP_VFO},
        RIG_FRNG_END,
    },
    .tx_range_list2 =  { RIG_FRNG_END, },
    .tuning_steps =  {
        {USRP_MODES, 1},
        RIG_TS_END,
    },
    /* mode/filter list, remember: order matters! */
    .filters =  {
        {USRP_MODES, kHz(40)},  /* FIXME */
        RIG_FLT_END,
    },
    .cfgparams =  usrp_cfg_params,

    .rig_init =  usrp_init,
    .rig_cleanup =  usrp_cleanup,
    .rig_open =  usrp_open,
    .rig_close =  usrp_close,

    .set_conf =  usrp_set_conf,
    .get_conf =  usrp_get_conf,

    .set_freq =  usrp_set_freq,
    .get_freq =  usrp_get_freq,
    .get_info =  usrp_get_info,
    .hamlib_check_rig_caps = HAMLIB_CHECK_RIG_CAPS
};

#endif  /* HAVE_USRP */
