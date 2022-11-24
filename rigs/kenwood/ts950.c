/*
 *  Hamlib Kenwood backend - TS950 description
 *  Copyright (c) 2002-2012 by Stephane Fillod
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

/*
 * Edited by Martin Ewing AA6E, March, 2012
 * The '950 has a relatively small command set.
 */

#include <hamlib/config.h>

#include <stdlib.h>

#include <hamlib/rig.h>
#include "kenwood.h"


#define TS950_ALL_MODES (RIG_MODE_AM|RIG_MODE_CW|RIG_MODE_USB|RIG_MODE_LSB|RIG_MODE_FM|RIG_MODE_RTTY)
#define TS950_OTHER_TX_MODES (RIG_MODE_CW|RIG_MODE_USB|RIG_MODE_LSB|RIG_MODE_FM|RIG_MODE_RTTY)
#define TS950_AM_TX_MODES RIG_MODE_AM

#define TS950_VFO (RIG_VFO_A|RIG_VFO_B)

#define TS950_GET_LEVEL (RIG_LEVEL_RAWSTR)

// STR_CAL borrowed from TS850
#define TS950_STR_CAL { 4, \
    { \
        {   0, -54 }, \
        {  15, 0 }, \
        {  22,30 }, \
        {  30, 66}, \
    } }


#define cmd_trm(rig) ((struct kenwood_priv_caps *)(rig)->caps->priv)->cmdtrm
static struct kenwood_priv_caps  ts950_priv_caps  =
{
    .cmdtrm =  EOM_KEN,
};

/*
 * This backend should work with all models in the TS-950 series (TS-950S, SDX, etc.)
 * There are minor differences between the SDX and other models, but they are
 * in commands not implemented here.
 *
 * Reference: TS-950 series External Control Instruction Manual (1992)
 */
const struct rig_caps ts950s_caps =
{
    RIG_MODEL(RIG_MODEL_TS950S),
    .model_name = "TS-950S",
    .mfg_name =  "Kenwood",
    .version =  BACKEND_VER ".1",
    .copyright =  "LGPL",
    .status =  RIG_STATUS_STABLE,
    .rig_type =  RIG_TYPE_TRANSCEIVER,
    .ptt_type =  RIG_PTT_RIG,
    .dcd_type =  RIG_DCD_RIG,
    .port_type =  RIG_PORT_SERIAL,
    .serial_rate_min =  1200,
    .serial_rate_max =  4800,
    .serial_data_bits =  8,
    .serial_stop_bits =  2,
    .serial_parity =  RIG_PARITY_NONE,
    .serial_handshake =  RIG_HANDSHAKE_NONE,
    .write_delay =  0,
    .post_write_delay =  50,
    .timeout =  600, // this rig takes over 250ms to respond an IF command
    .retry =  10,

    .has_get_func =  RIG_FUNC_NONE,
    .has_set_func =  RIG_FUNC_NONE,

    .has_get_level =  TS950_GET_LEVEL,
    .has_set_level =  RIG_LEVEL_NONE,
    .has_get_parm =  RIG_PARM_NONE,
    .has_set_parm =  RIG_PARM_NONE,

    .level_gran =
    {
#include "level_gran_kenwood.h"
    },
    .parm_gran =  {},
    .ctcss_list =  kenwood38_ctcss_list,
    .dcs_list =  NULL,
    .preamp =   { RIG_DBLST_END, },
    /* atten settings are not available in CAT interface */
    .attenuator =   { 6, 12, 18, RIG_DBLST_END, },
    .max_rit =  kHz(9.99),
    .max_xit =  kHz(9.99),
    .max_ifshift =  Hz(0),
    .targetable_vfo =  RIG_TARGETABLE_FREQ,
    .transceive =  RIG_TRN_RIG,
    // No AGC levels
    .bank_qty =   0,
    .chan_desc_sz =  0,

    .chan_list =  {
        {  0, 89, RIG_MTYPE_MEM },  /* TBC */
        { 90, 99, RIG_MTYPE_EDGE },
        RIG_CHAN_END,
    },

    .rx_range_list1 =  { RIG_FRNG_END, },    /* FIXME: enter region 1 setting */
    .tx_range_list1 =  { RIG_FRNG_END, },
    .rx_range_list2 =  {
        {kHz(100), MHz(30), TS950_ALL_MODES, -1, -1, TS950_VFO},
        RIG_FRNG_END,
    },
    .tx_range_list2 =  {
        {kHz(1800), MHz(2) - 1, TS950_OTHER_TX_MODES, 5000, W(150), TS950_VFO}, /* 100W class */
        {kHz(1800), MHz(2) - 1, TS950_AM_TX_MODES, 2000, W(40), TS950_VFO}, /* 25W class */
        {kHz(3500), MHz(4) - 1, TS950_OTHER_TX_MODES, 5000, W(150), TS950_VFO},
        {kHz(3500), MHz(4) - 1, TS950_AM_TX_MODES, 2000, W(40), TS950_VFO},
        {MHz(7), kHz(7300), TS950_OTHER_TX_MODES, 5000, W(150), TS950_VFO},
        {MHz(7), kHz(7300), TS950_AM_TX_MODES, 2000, W(40), TS950_VFO},
        {kHz(10100), kHz(10150), TS950_OTHER_TX_MODES, 5000, W(150), TS950_VFO},
        {kHz(10100), kHz(10150), TS950_AM_TX_MODES, 2000, W(40), TS950_VFO},
        {MHz(14), kHz(14350), TS950_OTHER_TX_MODES, 5000, W(150), TS950_VFO},
        {MHz(14), kHz(14350), TS950_AM_TX_MODES, 2000, W(40), TS950_VFO},
        {kHz(18068), kHz(18168), TS950_OTHER_TX_MODES, 5000, W(150), TS950_VFO},
        {kHz(18068), kHz(18168), TS950_AM_TX_MODES, 2000, W(40), TS950_VFO},
        {MHz(21), kHz(21450), TS950_OTHER_TX_MODES, 5000, W(150), TS950_VFO},
        {MHz(21), kHz(21450), TS950_AM_TX_MODES, 2000, W(40), TS950_VFO},
        {kHz(24890), kHz(24990), TS950_OTHER_TX_MODES, 5000, W(150), TS950_VFO},
        {kHz(24890), kHz(24990), TS950_AM_TX_MODES, 2000, W(40), TS950_VFO},
        {MHz(28), kHz(29700), TS950_OTHER_TX_MODES, 5000, W(150), TS950_VFO},
        {MHz(28), kHz(29700), TS950_AM_TX_MODES, 2000, W(40), TS950_VFO},
        RIG_FRNG_END,
    }, /* tx range */
    .tuning_steps =  {
        {TS950_ALL_MODES, 50},
        {TS950_ALL_MODES, 100},
        {TS950_ALL_MODES, kHz(1)},
        {TS950_ALL_MODES, kHz(5)},
        {TS950_ALL_MODES, kHz(9)},
        {TS950_ALL_MODES, kHz(10)},
        {TS950_ALL_MODES, 12500},
        {TS950_ALL_MODES, kHz(20)},
        {TS950_ALL_MODES, kHz(25)},
        {TS950_ALL_MODES, kHz(100)},
        {TS950_ALL_MODES, MHz(1)},
        {TS950_ALL_MODES, 0},  /* any tuning step */
        RIG_TS_END,
    },
    /* mode/filter list, remember: order matters! */
    .filters =  {
        {RIG_MODE_SSB, kHz(2.4)},
        {RIG_MODE_CW, Hz(200)},
        {RIG_MODE_RTTY, Hz(500)},
        {RIG_MODE_AM, kHz(9)},
        {RIG_MODE_FM, kHz(14)},
        RIG_FLT_END,
    },
    .str_cal = TS950_STR_CAL,
    .priv = (void *)& ts950_priv_caps,

    .rig_init = kenwood_init,
    .rig_open = kenwood_open,
    .rig_cleanup = kenwood_cleanup,
    .set_freq =  kenwood_set_freq,
    .get_freq =  kenwood_get_freq,
    .set_rit =  kenwood_set_rit,
    .get_rit =  kenwood_get_rit,
    .set_xit =  kenwood_set_xit,
    .get_xit =  kenwood_get_xit,
    .set_mode =  kenwood_set_mode,
    .get_mode =  kenwood_get_mode_if,
    .set_vfo =  kenwood_set_vfo,
    .get_vfo =  kenwood_get_vfo_if,
    .set_ctcss_tone =  kenwood_set_ctcss_tone,
    .get_ctcss_tone =  kenwood_get_ctcss_tone,
    .get_ptt =  kenwood_get_ptt,
    .set_ptt =  kenwood_set_ptt,
    .get_dcd =  kenwood_get_dcd,
    /* Things that the '950 doesn't do ...
     * .set_func =  kenwood_set_func,
     * .get_func =  kenwood_get_func,
     * .set_level =  kenwood_set_level,
     */
    .get_level =  kenwood_get_level,
    /*
     * .send_morse =  kenwood_send_morse,
     */
    .vfo_op =  kenwood_vfo_op,
    .set_mem =  kenwood_set_mem,
    .get_mem =  kenwood_get_mem,
    .set_trn =  kenwood_set_trn,
    .get_trn =  kenwood_get_trn,
    .reset =  kenwood_reset,
    .hamlib_check_rig_caps = HAMLIB_CHECK_RIG_CAPS
};


const struct rig_caps ts950sdx_caps =
{
    RIG_MODEL(RIG_MODEL_TS950SDX),
    .model_name = "TS-950SDX",
    .mfg_name =  "Kenwood",
    .version =  BACKEND_VER ".1",
    .copyright =  "LGPL",
    .status =  RIG_STATUS_STABLE,
    .rig_type =  RIG_TYPE_TRANSCEIVER,
    .ptt_type =  RIG_PTT_RIG,
    .dcd_type =  RIG_DCD_RIG,
    .port_type =  RIG_PORT_SERIAL,
    .serial_rate_min =  1200,
    .serial_rate_max =  4800,
    .serial_data_bits =  8,
    .serial_stop_bits =  2,
    .serial_parity =  RIG_PARITY_NONE,
    .serial_handshake =  RIG_HANDSHAKE_NONE,
    .write_delay =  0,
    .post_write_delay =  50,
    .timeout =  600, // this rig takes over 250ms to respond an IF command
    .retry =  10,

    .has_get_func =  RIG_FUNC_NONE,
    .has_set_func =  RIG_FUNC_NONE,

    .has_get_level =  TS950_GET_LEVEL,
    .has_set_level =  RIG_LEVEL_NONE,
    .has_get_parm =  RIG_PARM_NONE,
    .has_set_parm =  RIG_PARM_NONE,

    .level_gran =
    {
#include "level_gran_kenwood.h"
    },
    .parm_gran =  {},
    .ctcss_list =  kenwood38_ctcss_list,
    .dcs_list =  NULL,
    .preamp =   { RIG_DBLST_END, },
    /* atten settings are not available in CAT interface */
    .attenuator =   { 6, 12, 18, RIG_DBLST_END, },
    .max_rit =  kHz(9.99),
    .max_xit =  kHz(9.99),
    .max_ifshift =  Hz(0),
    .targetable_vfo =  RIG_TARGETABLE_FREQ,
    .transceive =  RIG_TRN_RIG,
    .bank_qty =   0,
    .chan_desc_sz =  0,

    .chan_list =  {
        {  0, 89, RIG_MTYPE_MEM },  /* TBC */
        { 90, 99, RIG_MTYPE_EDGE },
        RIG_CHAN_END,
    },

    .rx_range_list1 =  { RIG_FRNG_END, },    /* FIXME: enter region 1 setting */
    .tx_range_list1 =  { RIG_FRNG_END, },
    .rx_range_list2 =  {
        {kHz(100), MHz(30), TS950_ALL_MODES, -1, -1, TS950_VFO},
        RIG_FRNG_END,
    },
    .tx_range_list2 =  {
        {kHz(1800), MHz(2) - 1, TS950_OTHER_TX_MODES, 5000, W(150), TS950_VFO}, /* 100W class */
        {kHz(1800), MHz(2) - 1, TS950_AM_TX_MODES, 2000, W(40), TS950_VFO}, /* 25W class */
        {kHz(3500), MHz(4) - 1, TS950_OTHER_TX_MODES, 5000, W(150), TS950_VFO},
        {kHz(3500), MHz(4) - 1, TS950_AM_TX_MODES, 2000, W(40), TS950_VFO},
        {MHz(7), kHz(7300), TS950_OTHER_TX_MODES, 5000, W(150), TS950_VFO},
        {MHz(7), kHz(7300), TS950_AM_TX_MODES, 2000, W(40), TS950_VFO},
        {kHz(10100), kHz(10150), TS950_OTHER_TX_MODES, 5000, W(150), TS950_VFO},
        {kHz(10100), kHz(10150), TS950_AM_TX_MODES, 2000, W(40), TS950_VFO},
        {MHz(14), kHz(14350), TS950_OTHER_TX_MODES, 5000, W(150), TS950_VFO},
        {MHz(14), kHz(14350), TS950_AM_TX_MODES, 2000, W(40), TS950_VFO},
        {kHz(18068), kHz(18168), TS950_OTHER_TX_MODES, 5000, W(150), TS950_VFO},
        {kHz(18068), kHz(18168), TS950_AM_TX_MODES, 2000, W(40), TS950_VFO},
        {MHz(21), kHz(21450), TS950_OTHER_TX_MODES, 5000, W(150), TS950_VFO},
        {MHz(21), kHz(21450), TS950_AM_TX_MODES, 2000, W(40), TS950_VFO},
        {kHz(24890), kHz(24990), TS950_OTHER_TX_MODES, 5000, W(150), TS950_VFO},
        {kHz(24890), kHz(24990), TS950_AM_TX_MODES, 2000, W(40), TS950_VFO},
        {MHz(28), kHz(29700), TS950_OTHER_TX_MODES, 5000, W(150), TS950_VFO},
        {MHz(28), kHz(29700), TS950_AM_TX_MODES, 2000, W(40), TS950_VFO},
        RIG_FRNG_END,
    }, /* tx range */
    .tuning_steps =  {
        {TS950_ALL_MODES, 50},
        {TS950_ALL_MODES, 100},
        {TS950_ALL_MODES, kHz(1)},
        {TS950_ALL_MODES, kHz(5)},
        {TS950_ALL_MODES, kHz(9)},
        {TS950_ALL_MODES, kHz(10)},
        {TS950_ALL_MODES, 12500},
        {TS950_ALL_MODES, kHz(20)},
        {TS950_ALL_MODES, kHz(25)},
        {TS950_ALL_MODES, kHz(100)},
        {TS950_ALL_MODES, MHz(1)},
        {TS950_ALL_MODES, 0},  /* any tuning step */
        RIG_TS_END,
    },
    /* mode/filter list, remember: order matters! */
    .filters =  {
        {RIG_MODE_SSB, kHz(2.4)},
        {RIG_MODE_CW, Hz(200)},
        {RIG_MODE_RTTY, Hz(500)},
        {RIG_MODE_AM, kHz(9)},
        {RIG_MODE_FM, kHz(14)},
        RIG_FLT_END,
    },
    .str_cal = TS950_STR_CAL,
    .priv = (void *)& ts950_priv_caps,

    .rig_init = kenwood_init,
    .rig_open = kenwood_open,
    .rig_cleanup = kenwood_cleanup,
    .set_freq =  kenwood_set_freq,
    .get_freq =  kenwood_get_freq,
    .set_rit =  kenwood_set_rit,
    .get_rit =  kenwood_get_rit,
    .set_xit =  kenwood_set_xit,
    .get_xit =  kenwood_get_xit,
    .set_mode =  kenwood_set_mode,
    .get_mode =  kenwood_get_mode_if,
    .set_vfo =  kenwood_set_vfo,
    .get_vfo =  kenwood_get_vfo_if,
    .set_ctcss_tone =  kenwood_set_ctcss_tone,
    .get_ctcss_tone =  kenwood_get_ctcss_tone,
    .get_ptt =  kenwood_get_ptt,
    .set_ptt =  kenwood_set_ptt,
    .get_dcd =  kenwood_get_dcd,
    /* Things that the '950 doesn't do ...
     * .set_func =  kenwood_set_func,
     * .get_func =  kenwood_get_func,
     * .set_level =  kenwood_set_level,
     */
    .get_level =  kenwood_get_level,
    /*
     * .send_morse =  kenwood_send_morse,
     */
    .vfo_op =  kenwood_vfo_op,
    .set_mem =  kenwood_set_mem,
    .get_mem =  kenwood_get_mem,
    .set_trn =  kenwood_set_trn,
    .get_trn =  kenwood_get_trn,
    .reset =  kenwood_reset,

    .hamlib_check_rig_caps = HAMLIB_CHECK_RIG_CAPS
};

