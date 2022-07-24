/*
 *  Hamlib Racal backend - RA3702 description
 *  Copyright (c) 2010 by Stephane Fillod
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
#include "idx_builtin.h"
#include "ra37xx.h"


/* TODO: ISB */
#define RA3702_MODES (RIG_MODE_AM|RIG_MODE_CW|RIG_MODE_SSB|RIG_MODE_FM|RIG_MODE_RTTY)

#define RA3702_FUNC (RIG_FUNC_MUTE)

#define RA3702_LEVEL_ALL (RIG_LEVEL_RF|RIG_LEVEL_AF|RIG_LEVEL_AGC|\
        RIG_LEVEL_CWPITCH|RIG_LEVEL_PREAMP|RIG_LEVEL_SQL|RIG_LEVEL_RAWSTR)

#define RA3702_PARM_ALL (RIG_PARM_NONE)

#define RA3702_VFO (RIG_VFO_A)

#define RA3702_VFO_OPS (RIG_OP_FROM_VFO|RIG_OP_TO_VFO)
#define RA3702_SCAN_OPS (RIG_SCAN_STOP|RIG_SCAN_VFO|RIG_SCAN_MEM)

/*
 * ra3702 rig capabilities.
 *
 * Required A6A1 serial asynchronous interface
 *
 */
const struct rig_caps ra3702_caps =
{
    RIG_MODEL(RIG_MODEL_RA3702),
    .model_name = "RA3702",
    .mfg_name =  "Racal",
    .version =  BACKEND_VER ".0",
    .copyright =  "LGPL",
    .status =  RIG_STATUS_BETA,
    .rig_type =  RIG_TYPE_RECEIVER,
    .ptt_type =  RIG_PTT_NONE,
    .dcd_type =  RIG_DCD_NONE,
    .port_type =  RIG_PORT_SERIAL,
    .serial_rate_min =  50,
    .serial_rate_max =  9600,
    .serial_data_bits =  7,
    .serial_stop_bits =  1,
    .serial_parity =  RIG_PARITY_EVEN,
    .serial_handshake =  RIG_HANDSHAKE_XONXOFF,
    .write_delay =  0,
    .post_write_delay =  0,
    .timeout =  RA37XX_TIMEOUT,
    .retry =  2,

    .has_get_func =  RA3702_FUNC,
    .has_set_func =  RA3702_FUNC,
    .has_get_level =  RA3702_LEVEL_ALL,
    .has_set_level =  RIG_LEVEL_SET(RA3702_LEVEL_ALL),
    .has_get_parm =  RA3702_PARM_ALL,
    .has_set_parm =  RIG_PARM_SET(RA3702_PARM_ALL),
    .level_gran = {
        // cppcheck-suppress *
        [LVL_RAWSTR] = { .min = { .i = 0 }, .max = { .i = 255 } },
    },
    .vfo_ops =  RA3702_VFO_OPS,
    .scan_ops =  RA3702_SCAN_OPS,
    .preamp =   { 20, RIG_DBLST_END }, /* FIXME */
    .attenuator =   { RIG_DBLST_END },
    .max_rit =  Hz(0),
    .max_xit =  Hz(0),
    .max_ifshift =  Hz(0),
    .targetable_vfo =  0,
    .transceive =  RIG_TRN_OFF,
    .bank_qty =   0,
    .chan_desc_sz =  0,
    .str_cal = RA37XX_STR_CAL,

    .chan_list =  {
        {   0,  99, RIG_MTYPE_MEM, RA37XX_MEM_CAP },
        RIG_CHAN_END,
    },

    .rx_range_list1 =  {
        {kHz(15), MHz(30) - 1, RA3702_MODES, -1, -1, RA3702_VFO},
        RIG_FRNG_END,
    },
    .tx_range_list1 =  { RIG_FRNG_END, },
    .rx_range_list2 =  {
        {kHz(15), MHz(30) - 1, RA3702_MODES, -1, -1, RA3702_VFO},
        RIG_FRNG_END,
    },
    .tx_range_list2 =  { RIG_FRNG_END, },
    .tuning_steps =  {
        {RA3702_MODES, 1},
        {RA3702_MODES, 10},
        RIG_TS_END,
    },
    /* mode/filter list, remember: order matters! */
    .filters =  {
        /* at -3dB */
        {RIG_MODE_SSB | RIG_MODE_CW | RIG_MODE_RTTY, kHz(2.7)},
        {RA3702_MODES, kHz(12)},
        {RA3702_MODES, kHz(6)},
        {RA3702_MODES, Hz(300)},
        {RA3702_MODES, kHz(1)},
        {RA3702_MODES, kHz(2.7)},
        //{RA3702_MODES, 0},    /* accept any BW */
        RIG_FLT_END,
    },

    .cfgparams = ra37xx_cfg_params,

    .rig_init =  ra37xx_init,
    .rig_cleanup =  ra37xx_cleanup,
    .rig_open =  ra37xx_open,
    .rig_close =  ra37xx_close,
    .set_conf =  ra37xx_set_conf,
    .get_conf =  ra37xx_get_conf,

    .set_freq =  ra37xx_set_freq,
    .get_freq =  ra37xx_get_freq,
    .set_mode =  ra37xx_set_mode,
    .get_mode =  ra37xx_get_mode,
    .set_func =  ra37xx_set_func,
    .get_func =  ra37xx_get_func,
    .set_level =  ra37xx_set_level,
    .get_level =  ra37xx_get_level,

    .set_ant =  ra37xx_set_ant,
    .get_ant =  ra37xx_get_ant,
    .set_mem =  ra37xx_set_mem,
    .get_mem =  ra37xx_get_mem,

    .scan = ra37xx_scan,
    .vfo_op = ra37xx_vfo_op,
    .get_info = ra37xx_get_info,
    .hamlib_check_rig_caps = HAMLIB_CHECK_RIG_CAPS
};

/*
 * Function definitions below
 */

