/*
 *  Hamlib ICOM Marine backend - model IC-M803 (derived from IC-M802)
 *  Modifications by Blaine Kubesh  (k5kub)
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

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <stdlib.h>

#include "hamlib/rig.h"
#include "icmarine.h"
#include "idx_builtin.h"
#include "bandplan.h"

#define ICM803_MODES (RIG_MODE_SSB|RIG_MODE_CW|RIG_MODE_RTTY)
#define ICM803_RX_MODES (ICM803_MODES|RIG_MODE_AM)

#define ICM803_FUNC_ALL (RIG_FUNC_NB)

#define ICM803_LEVEL_ALL (RIG_LEVEL_RFPOWER|RIG_LEVEL_AF|RIG_LEVEL_RF|RIG_LEVEL_AGC|RIG_LEVEL_RAWSTR)

#define ICM803_VFO_ALL (RIG_VFO_A)

#define ICM803_VFO_OPS (RIG_OP_TUNE)

#define ICM803_SCAN_OPS (RIG_SCAN_NONE)

/*
 * TODO calibrate the real values
 */
#define ICM803_STR_CAL { 2, {{ 0, -60}, { 8, 60}} }


static const struct icmarine_priv_caps icm803_priv_caps =
{
    .default_remote_id = 20,  /* default address */
};


const struct rig_caps icm803_caps =
{
    RIG_MODEL(RIG_MODEL_IC_M803),
    .model_name = "IC-M803",
    .mfg_name =  "Icom",
    .version =  "20200524.0",
    .copyright =  "LGPL",
    .status =  RIG_STATUS_STABLE,
    .rig_type =   RIG_TYPE_TRANSCEIVER,
    .ptt_type =  RIG_PTT_RIG,
    .dcd_type =  RIG_DCD_RIG,
    .port_type =  RIG_PORT_SERIAL,
    .serial_rate_min =  4800,
    .serial_rate_max =  4800,
    .serial_data_bits =  8,
    .serial_stop_bits =  1,
    .serial_parity =  RIG_PARITY_NONE,
    .serial_handshake =  RIG_HANDSHAKE_NONE,
    .write_delay =  0,
    .post_write_delay =  0,
    .timeout =  500,
    .retry =  0,
    .has_get_func =  ICM803_FUNC_ALL,
    .has_set_func =  ICM803_FUNC_ALL,
    .has_get_level =  ICM803_LEVEL_ALL,
    .has_set_level =  RIG_LEVEL_SET(ICM803_LEVEL_ALL),
    .has_get_parm =  RIG_PARM_NONE,
    .has_set_parm =  RIG_PARM_NONE,
    .level_gran = {
        [LVL_RAWSTR] = { .min = { .i = 0 }, .max = { .i = 8 } },
    },
    .parm_gran =  {},
    .str_cal = ICM803_STR_CAL,
    .ctcss_list =  NULL,
    .dcs_list =  NULL,
    .preamp =   { RIG_DBLST_END, },
    .attenuator =   { RIG_DBLST_END, },
    .max_rit =  Hz(0),
    .max_xit =  Hz(0),
    .max_ifshift =  Hz(0),
    .targetable_vfo =  0,
    .vfo_ops =  ICM803_VFO_OPS,
    .scan_ops =  ICM803_SCAN_OPS,
    .transceive =  RIG_TRN_OFF,
    .bank_qty =   0,
    .chan_desc_sz =  0,

    .chan_list =  {
        RIG_CHAN_END,
    },

    .rx_range_list1 =   { {kHz(500), MHz(30) - 100, ICM803_RX_MODES, -1, -1, ICM803_VFO_ALL},
        RIG_FRNG_END,
    },
    .tx_range_list1 =   {
        {kHz(1600), MHz(3) - 100, ICM803_MODES, W(20), W(150), ICM803_VFO_ALL, RIG_ANT_1},
        {MHz(4),   MHz(5) - 100, ICM803_MODES, W(20), W(150), ICM803_VFO_ALL, RIG_ANT_1},
        {MHz(6),   MHz(7) - 100, ICM803_MODES, W(20), W(150), ICM803_VFO_ALL, RIG_ANT_1},
        {MHz(8),   MHz(9) - 100, ICM803_MODES, W(20), W(150), ICM803_VFO_ALL, RIG_ANT_1},
        {MHz(12), MHz(14) - 100, ICM803_MODES, W(20), W(150), ICM803_VFO_ALL, RIG_ANT_1},
        {MHz(16), MHz(18) - 100, ICM803_MODES, W(20), W(150), ICM803_VFO_ALL, RIG_ANT_1},
        {MHz(18), MHz(20) - 100, ICM803_MODES, W(20), W(150), ICM803_VFO_ALL, RIG_ANT_1},
        {MHz(22), MHz(23) - 100, ICM803_MODES, W(20), W(150), ICM803_VFO_ALL, RIG_ANT_1},
        {MHz(25), MHz(27.500), ICM803_MODES, W(20), W(150), ICM803_VFO_ALL, RIG_ANT_1},
        RIG_FRNG_END,
    },

    .rx_range_list2 =   { {kHz(500), MHz(30) - 100, ICM803_RX_MODES, -1, -1, ICM803_VFO_ALL},
        RIG_FRNG_END,
    },
    .tx_range_list2 =   {
        {kHz(1600), MHz(3) - 100, ICM803_MODES, W(20), W(150), ICM803_VFO_ALL, RIG_ANT_1},
        {MHz(4),   MHz(5) - 100, ICM803_MODES, W(20), W(150), ICM803_VFO_ALL, RIG_ANT_1},
        {MHz(6),   MHz(7) - 100, ICM803_MODES, W(20), W(150), ICM803_VFO_ALL, RIG_ANT_1},
        {MHz(8),   MHz(9) - 100, ICM803_MODES, W(20), W(150), ICM803_VFO_ALL, RIG_ANT_1},
        {MHz(12), MHz(14) - 100, ICM803_MODES, W(20), W(150), ICM803_VFO_ALL, RIG_ANT_1},
        {MHz(16), MHz(18) - 100, ICM803_MODES, W(20), W(150), ICM803_VFO_ALL, RIG_ANT_1},
        {MHz(18), MHz(20) - 100, ICM803_MODES, W(20), W(150), ICM803_VFO_ALL, RIG_ANT_1},
        {MHz(22), MHz(23) - 100, ICM803_MODES, W(20), W(150), ICM803_VFO_ALL, RIG_ANT_1},
        {MHz(25), MHz(27.500), ICM803_MODES, W(20), W(150), ICM803_VFO_ALL, RIG_ANT_1},
        RIG_FRNG_END,
    },

    .tuning_steps =     {
        {ICM803_RX_MODES, Hz(1)},
        RIG_TS_END,
    },
    /* mode/filter list, remember: order matters! */
    .filters =  {
        {RIG_MODE_SSB | RIG_MODE_CW | RIG_MODE_RTTY, kHz(2.3)},
        {RIG_MODE_AM, kHz(14)},
        RIG_FLT_END,
    },

    .cfgparams =  icmarine_cfg_params,
    .set_conf =  icmarine_set_conf,
    .get_conf =  icmarine_get_conf,

    .priv = (void *)& icm803_priv_caps,
    .rig_init =   icmarine_init,
    .rig_cleanup =   icmarine_cleanup,
    .rig_open =  NULL,
    .rig_close =  NULL,

    .set_freq =  icmarine_set_freq,
    .get_freq =  icmarine_get_freq,
    .set_split_freq =  icmarine_set_tx_freq,
    .get_split_freq =  icmarine_get_tx_freq,
    .set_split_vfo = icmarine_set_split_vfo,
    .get_split_vfo = icmarine_get_split_vfo,
    .set_mode =  icmarine_set_mode,
    .get_mode =  icmarine_get_mode,

    .set_ptt =  icmarine_set_ptt,
    .get_ptt =  icmarine_get_ptt,
    .get_dcd =  icmarine_get_dcd,
    .vfo_op =  icmarine_vfo_op,

    .set_level =  icmarine_set_level,
    .get_level =  icmarine_get_level,
    .set_func =  icmarine_set_func,
    .get_func =  icmarine_get_func,

};


