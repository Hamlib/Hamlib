/*
 *  Hamlib TAPR backend - DSP-10 description
 *  Copyright (c) 2003 by Stephane Fillod
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

#include "tapr.h"


#define DSP10_MODES (RIG_MODE_CW|RIG_MODE_SSB|RIG_MODE_FM)

#define DSP10_FUNC (RIG_FUNC_NONE)

#define DSP10_LEVEL_ALL (RIG_LEVEL_NONE)

#define DSP10_PARM_ALL (RIG_PARM_NONE)

#define DSP10_VFO (RIG_VFO_A)


/*
 * DSP-10 rig capabilities.
 *
 * This backend is not working, and is just a call for someone motivated.
 * Note: The DSP has to be loaded beforehand!
 *
 * protocol is documented at (version 2)
 *  http://www.proaxis.com/~boblark/pc_dsp2.txt
 *
 * See also:
 *  http://home.teleport.com/~oldaker/dsp10_repository.htm
 *  http://www.proaxis.com/~boblark/dsp10.htm
 *  http://www.tapr.org/tapr/html/Fdsp10.html
 *
 *
 */
const struct rig_caps dsp10_caps =
{
    RIG_MODEL(RIG_MODEL_DSP10),
    .model_name = "DSP-10",
    .mfg_name =  "TAPR",
    .version =  "20061007.0",
    .copyright =  "LGPL",
    .status =  RIG_STATUS_BETA,
    .rig_type =  RIG_TYPE_TRANSCEIVER,
    .ptt_type =  RIG_PTT_NONE,
    .dcd_type =  RIG_DCD_NONE,
    .port_type =  RIG_PORT_SERIAL,
    .serial_rate_min =  9600,
    .serial_rate_max =  9600,
    .serial_data_bits =  8,
    .serial_stop_bits =  1,
    .serial_parity =  RIG_PARITY_NONE,
    .serial_handshake =  RIG_HANDSHAKE_NONE,
    .write_delay =  0,
    .post_write_delay =  0,
    .timeout =  200,
    .retry =  3,

    .has_get_func =  DSP10_FUNC,
    .has_set_func =  DSP10_FUNC,
    .has_get_level =  DSP10_LEVEL_ALL,
    .has_set_level =  RIG_LEVEL_SET(DSP10_LEVEL_ALL),
    .has_get_parm =  DSP10_PARM_ALL,
    .has_set_parm =  RIG_PARM_SET(DSP10_PARM_ALL),
    .level_gran =  {},                 /* FIXME: granularity */
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

    .chan_list =  {
        RIG_CHAN_END,
    },

    .rx_range_list1 =  { RIG_FRNG_END, },    /* FIXME: enter region 1 setting */
    .tx_range_list1 =  { RIG_FRNG_END, },
    .rx_range_list2 =  {
        {MHz(144), MHz(148), DSP10_MODES, -1, -1, DSP10_VFO},
        RIG_FRNG_END,
    },
    .tx_range_list2 =  {
        {MHz(144), MHz(148), DSP10_MODES, mW(20), mW(20), DSP10_VFO},
        RIG_FRNG_END,
    },
    .tuning_steps =  {
        {DSP10_MODES, 1},
        RIG_TS_END,
    },
    /* mode/filter list, remember: order matters! */
    .filters =  {
        {RIG_MODE_SSB | RIG_MODE_CW, kHz(2.7)},
        {RIG_MODE_FM, kHz(8)},
        RIG_FLT_END,
    },
    .priv = (void *)NULL,

    .set_freq =  tapr_set_freq,
    .set_mode =  tapr_set_mode,
    .hamlib_check_rig_caps = HAMLIB_CHECK_RIG_CAPS
};

