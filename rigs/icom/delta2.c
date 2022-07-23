/*
 *  Hamlib CI-V backend - description of the TenTenc DeltaII
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
 */

/*
 * Delta II (aka TT 536) cloned after Omni VI.
 * Needs RS-232 Serial Level Converter Model 305 or equivalent.
 *
 * For changing CI-V address of the rig, see:
 *   http://www.tentecwiki.org/doku.php?id=536addr
 */

/* Known problems:
 *
 * To Do: get the datasheet, and testing on real hardware!!
 */

#include <hamlib/config.h>


#include <hamlib/rig.h>
#include "icom.h"
#include "icom_defs.h"
#include "frame.h"
#include "misc.h"

#define DELTAII_VFO_ALL (RIG_VFO_A|RIG_VFO_B|RIG_VFO_MEM)

#define DELTAII_OTHER_TX_MODES (RIG_MODE_CW|RIG_MODE_SSB|RIG_MODE_FM)

#define DELTAII_ALL_RX_MODES (DELTAII_OTHER_TX_MODES|RIG_MODE_AM)

#define DELTAII_VFO_OPS (RIG_OP_FROM_VFO|RIG_OP_TO_VFO)
#define DELTAII_STR_CAL { 0, { } }


static const struct icom_priv_caps delta2_priv_caps =
{
    0x01,   /* default address */
    1,      /* 731 mode */
    0,    /* no XCHG */
    ic737_ts_sc_list  /* TODO: ts_sc_list */
};

const struct rig_caps delta2_caps =
{
    RIG_MODEL(RIG_MODEL_DELTAII),
    .model_name = "Delta II",
    .mfg_name =  "Ten-Tec",
    .version =  BACKEND_VER ".0",
    .copyright =  "LGPL",
    .status =  RIG_STATUS_ALPHA,
    .rig_type =  RIG_TYPE_TRANSCEIVER,
    .ptt_type =  RIG_PTT_NONE,
    .dcd_type =  RIG_DCD_NONE,
    .port_type =  RIG_PORT_SERIAL,
    .serial_rate_min =  1200,
    .serial_rate_max =  1200,
    .serial_data_bits =  8,
    .serial_stop_bits =  1,
    .serial_parity =  RIG_PARITY_NONE,
    .serial_handshake =  RIG_HANDSHAKE_NONE,
    .write_delay =  0,
    .post_write_delay =  0,
    .timeout =  1000,
    .retry =  3,
    .has_get_func =  RIG_FUNC_NONE,
    .has_set_func =  RIG_FUNC_NONE,
    .has_get_level =  RIG_LEVEL_NONE,
    .has_set_level =  RIG_LEVEL_NONE,
    .has_get_parm =  RIG_PARM_NONE,
    .has_set_parm =  RIG_PARM_NONE,
    .level_gran =  {},
    .parm_gran =  {},
    .preamp =   { RIG_DBLST_END, },
    .attenuator =   { RIG_DBLST_END, },
    .max_rit =  Hz(0),
    .max_xit =  Hz(0),
    .max_ifshift =  Hz(0),
    .targetable_vfo =  0,
    .vfo_ops =  DELTAII_VFO_OPS,
    .scan_ops =  RIG_SCAN_NONE,
    .str_cal =  DELTAII_STR_CAL,
    .transceive =  RIG_TRN_RIG,
    .bank_qty =   0,
    .chan_desc_sz =  0,

    .chan_list =  {
        /* TODO: 32 simplex, 16 duplex */
        { 0, 47, RIG_MTYPE_MEM, IC_MIN_MEM_CAP },
        RIG_CHAN_END,
    },
    .rx_range_list1 =   {
        RIG_FRNG_END,
    },
    .tx_range_list1 =  { RIG_FRNG_END, },

    /* These limits measured on Omni VI SN 1A10473 */
    .rx_range_list2 =   {
        {kHz(100), kHz(29999), DELTAII_ALL_RX_MODES, -1, -1, DELTAII_VFO_ALL},
        RIG_FRNG_END,
    },

    /* Note: There is no AM mode. */
    .tx_range_list2 =  {
        {kHz(1800), MHz(2) - 1, DELTAII_OTHER_TX_MODES, 5000, 100000, DELTAII_VFO_ALL},
        {kHz(3500), MHz(4) - 1, DELTAII_OTHER_TX_MODES, 5000, 100000, DELTAII_VFO_ALL},
        {MHz(7), kHz(7300), DELTAII_OTHER_TX_MODES, 5000, 100000, DELTAII_VFO_ALL},
        {kHz(10100), kHz(10150), DELTAII_OTHER_TX_MODES, 5000, 100000, DELTAII_VFO_ALL},
        {MHz(14), kHz(14350), DELTAII_OTHER_TX_MODES, 5000, 100000, DELTAII_VFO_ALL},
        {kHz(18068), kHz(18168), DELTAII_OTHER_TX_MODES, 5000, 100000, DELTAII_VFO_ALL},
        {MHz(21), kHz(21450), DELTAII_OTHER_TX_MODES, 5000, 100000, DELTAII_VFO_ALL},
        {kHz(24890), kHz(24990), DELTAII_OTHER_TX_MODES, 5000, 100000, DELTAII_VFO_ALL},
        {MHz(28), kHz(29700), DELTAII_OTHER_TX_MODES, 5000, 100000, DELTAII_VFO_ALL},
        RIG_FRNG_END,
    },

    .tuning_steps =     {
        {DELTAII_ALL_RX_MODES, Hz(10)},        // This radio has 10 Hz steps.
        RIG_TS_END,
    },
    /* mode/filter list, remember: order matters!
     */
    .filters =  {
        {RIG_MODE_SSB | RIG_MODE_CW | RIG_MODE_RTTY, kHz(2.4)},
        {RIG_MODE_AM, kHz(6)},
        {RIG_MODE_FM, kHz(15)},
        RIG_FLT_END,
    },
    .cfgparams =  icom_cfg_params,
    .set_conf =  icom_set_conf,
    .get_conf =  icom_get_conf,

    .priv = (void *)& delta2_priv_caps,
    .rig_init =   icom_init,
    .rig_cleanup =   icom_cleanup,

    .set_freq =  icom_set_freq,
    .get_freq =  icom_get_freq,
    .set_mode =  icom_set_mode,
    .get_mode =  icom_get_mode,
    .set_vfo =  icom_set_vfo,       // icom.c has no get_vfo

    .decode_event =  icom_decode_event,
    .set_mem =  icom_set_mem,
    .vfo_op =  icom_vfo_op,
    .hamlib_check_rig_caps = HAMLIB_CHECK_RIG_CAPS
};


