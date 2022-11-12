/*
 *  Hamlib CI-V backend - description of IC-751 and variations
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
#include "bandplan.h"
#include "icom.h"
#include "idx_builtin.h"

/*
 * FM is an option on the Icom IC-751, and built-in in the Icom IC-751A
 */
#define IC751_ALL_RX_MODES (RIG_MODE_AM|RIG_MODE_CW|RIG_MODE_SSB|RIG_MODE_RTTY|RIG_MODE_FM)

/*
 * 200W in all modes but AM (40W)
 */
#define IC751_OTHER_TX_MODES (RIG_MODE_CW|RIG_MODE_SSB|RIG_MODE_RTTY|RIG_MODE_FM)
#define IC751_AM_TX_MODES (RIG_MODE_AM)

#define IC751_VFO_ALL (RIG_VFO_A|RIG_VFO_MEM)

#define IC751_VFO_OPS (RIG_OP_FROM_VFO|RIG_OP_TO_VFO)

#define IC751_SCAN_OPS (RIG_SCAN_NONE)

#define IC751_ANTS RIG_ANT_1

/*
 * S-Meter measurements
 * (Only the Piexx UX-14px rev.2 and up has an S-meter option.)
 * Values based on the readings of my IC-751A S-meter, i.e. not
 * actual signal strength. -- Lars, sm6rpz
 */
#define IC751_STR_CAL { 16,  { \
            {   3, -52 }, /* S0.5 */ \
            {  12, -48 }, /* S1 */ \
            {  33, -42 }, /* S2 */ \
            {  45, -36 }, /* S3 */ \
            {  60, -30 }, /* S4 */ \
            {  73, -24 }, /* S5 */ \
            {  86, -18 }, /* S6 */ \
            { 100, -12 }, /* S7 */ \
            { 115,  -6 }, /* S8 */ \
            { 129,   0 }, /* S9 */ \
            { 160,  10 }, /* S9+10 */ \
            { 186,  20 }, /* S9+20 */ \
            { 208,  30 }, /* S9+30 */ \
            { 226,  40 }, /* S9+40 */ \
            { 241,  50 }, /* S9+50 */ \
            { 255,  60 }  /* S9+60 */ \
                } }

/*
 */
static const struct icom_priv_caps ic751_priv_caps =
{
    0x1c,   /* default address */
    0,      /* 731 mode */
    0,    /* no XCHG */
    ic737_ts_sc_list
};

const struct rig_caps ic751_caps =
{
    RIG_MODEL(RIG_MODEL_IC751),
    .model_name = "IC-751",
    .mfg_name =  "Icom",
    .version =  BACKEND_VER ".0",
    .copyright =  "LGPL",
    .status =  RIG_STATUS_STABLE,
    .rig_type =  RIG_TYPE_TRANSCEIVER,
    .ptt_type =  RIG_PTT_RIG, /* Piexx UX-14px has a PTT option */
    .dcd_type =  RIG_DCD_NONE,
    .port_type =  RIG_PORT_SERIAL,
    .serial_rate_min =  1200,
    .serial_rate_max =  9600, /* Piexx UX-14px can use 9600 */
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
    /* Piexx UX-14px has an S-meter option */
    .has_get_level = (RIG_LEVEL_RAWSTR | RIG_LEVEL_STRENGTH),
    .has_set_level =  RIG_LEVEL_NONE,
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
    .vfo_ops =  IC751_VFO_OPS,
    .scan_ops =  IC751_SCAN_OPS,
    .transceive =  RIG_TRN_RIG,
    .bank_qty =   0,
    .chan_desc_sz =  0,

    .chan_list =  {
        {   1,  32, RIG_MTYPE_MEM, IC_MIN_MEM_CAP },
        RIG_CHAN_END,
    },

    .rx_range_list1 =   {
        {kHz(100), MHz(30), IC751_ALL_RX_MODES, -1, -1, IC751_VFO_ALL},
        RIG_FRNG_END,
    },
    .tx_range_list1 =  {
        {MHz(1.8), MHz(1.99999), IC751_OTHER_TX_MODES, W(10), W(200), IC751_VFO_ALL}, /* 100W class */
        {MHz(1.8), MHz(1.99999), IC751_AM_TX_MODES, W(10), W(40), IC751_VFO_ALL}, /* 40W class */
        {MHz(3.45), MHz(4.09999), IC751_OTHER_TX_MODES, W(10), W(200), IC751_VFO_ALL},
        {MHz(3.45), MHz(4.09999), IC751_AM_TX_MODES, W(10), W(40), IC751_VFO_ALL},
        {MHz(6.95), MHz(7.49999), IC751_OTHER_TX_MODES, W(10), W(200), IC751_VFO_ALL},
        {MHz(6.95), MHz(7.49999), IC751_AM_TX_MODES, W(10), W(40), IC751_VFO_ALL},
        {MHz(9.95), MHz(10.49999), IC751_OTHER_TX_MODES, W(10), W(200), IC751_VFO_ALL},
        {MHz(9.95), MHz(10.49999), IC751_AM_TX_MODES, W(10), W(40), IC751_VFO_ALL},
        {MHz(13.95), MHz(14.49999), IC751_OTHER_TX_MODES, W(10), W(200), IC751_VFO_ALL},
        {MHz(13.95), MHz(14.49999), IC751_AM_TX_MODES, W(10), W(40), IC751_VFO_ALL},
        {MHz(17.95), MHz(18.49999), IC751_OTHER_TX_MODES, W(10), W(200), IC751_VFO_ALL},
        {MHz(17.95), MHz(18.49999), IC751_AM_TX_MODES, W(10), W(40), IC751_VFO_ALL},
        {MHz(20.95), MHz(21.49999), IC751_OTHER_TX_MODES, W(10), W(200), IC751_VFO_ALL},
        {MHz(20.95), MHz(21.49999), IC751_AM_TX_MODES, W(10), W(40), IC751_VFO_ALL},
        {MHz(24.45), MHz(25.09999), IC751_OTHER_TX_MODES, W(10), W(200), IC751_VFO_ALL},
        {MHz(24.45), MHz(25.09999), IC751_AM_TX_MODES, W(10), W(40), IC751_VFO_ALL},
        {MHz(27.95), MHz(30), IC751_OTHER_TX_MODES, W(10), W(200), IC751_VFO_ALL},
        {MHz(27.95), MHz(30), IC751_AM_TX_MODES, W(10), W(40), IC751_VFO_ALL},
        RIG_FRNG_END,
    },

    .rx_range_list2 =   {
        {kHz(100), MHz(30), IC751_ALL_RX_MODES, -1, -1, IC751_VFO_ALL},
        RIG_FRNG_END,
    },
    /* weird transmit ranges ... --sf */
    .tx_range_list2 =   {
        {MHz(1.8), MHz(1.99999), IC751_OTHER_TX_MODES, W(10), W(200), IC751_VFO_ALL}, /* 100W class */
        {MHz(1.8), MHz(1.99999), IC751_AM_TX_MODES, W(10), W(40), IC751_VFO_ALL}, /* 40W class */
        {MHz(3.45), MHz(4.09999), IC751_OTHER_TX_MODES, W(10), W(200), IC751_VFO_ALL},
        {MHz(3.45), MHz(4.09999), IC751_AM_TX_MODES, W(10), W(40), IC751_VFO_ALL},
        {MHz(6.95), MHz(7.49999), IC751_OTHER_TX_MODES, W(10), W(200), IC751_VFO_ALL},
        {MHz(6.95), MHz(7.49999), IC751_AM_TX_MODES, W(10), W(40), IC751_VFO_ALL},
        {MHz(9.95), MHz(10.49999), IC751_OTHER_TX_MODES, W(10), W(200), IC751_VFO_ALL},
        {MHz(9.95), MHz(10.49999), IC751_AM_TX_MODES, W(10), W(40), IC751_VFO_ALL},
        {MHz(13.95), MHz(14.49999), IC751_OTHER_TX_MODES, W(10), W(200), IC751_VFO_ALL},
        {MHz(13.95), MHz(14.49999), IC751_AM_TX_MODES, W(10), W(40), IC751_VFO_ALL},
        {MHz(17.95), MHz(18.49999), IC751_OTHER_TX_MODES, W(10), W(200), IC751_VFO_ALL},
        {MHz(17.95), MHz(18.49999), IC751_AM_TX_MODES, W(10), W(40), IC751_VFO_ALL},
        {MHz(20.95), MHz(21.49999), IC751_OTHER_TX_MODES, W(10), W(200), IC751_VFO_ALL},
        {MHz(20.95), MHz(21.49999), IC751_AM_TX_MODES, W(10), W(40), IC751_VFO_ALL},
        {MHz(24.45), MHz(25.09999), IC751_OTHER_TX_MODES, W(10), W(200), IC751_VFO_ALL},
        {MHz(24.45), MHz(25.09999), IC751_AM_TX_MODES, W(10), W(40), IC751_VFO_ALL},
        {MHz(27.95), MHz(30), IC751_OTHER_TX_MODES, W(10), W(200), IC751_VFO_ALL},
        {MHz(27.95), MHz(30), IC751_AM_TX_MODES, W(10), W(40), IC751_VFO_ALL},
        RIG_FRNG_END,
    },

    .tuning_steps =     {
        {IC751_ALL_RX_MODES, 10}, /* basic resolution, there's no set_ts */
        RIG_TS_END,
    },
    /* mode/filter list, remember: order matters! */
    .filters =  {
        {RIG_MODE_SSB, kHz(2.3)},
        {RIG_MODE_RTTY | RIG_MODE_CW, Hz(500)},
        {RIG_MODE_RTTY | RIG_MODE_CW, Hz(250)},
        {RIG_MODE_AM, kHz(6)},
        {RIG_MODE_FM, kHz(15)},
        RIG_FLT_END,
    },

    .str_cal = IC751_STR_CAL,

    .cfgparams =  icom_cfg_params,
    .set_conf =  icom_set_conf,
    .get_conf =  icom_get_conf,

    .priv = (void *)& ic751_priv_caps,
    .rig_init = icom_init,
    .rig_cleanup =  icom_cleanup,
    .rig_open =  icom_rig_open,
    .rig_close =  icom_rig_open,

    .set_freq =  icom_set_freq,
    .get_freq =  icom_get_freq,
    .set_mode =  icom_set_mode,
    .get_mode =  icom_get_mode,
    .set_vfo =  icom_set_vfo,

    .decode_event =  icom_decode_event,
    .set_mem =  icom_set_mem,
    .vfo_op =  icom_vfo_op,

    .get_level =  icom_get_level,
    .set_ptt =  icom_set_ptt,/* Piexx UX-14px has no get_ptt only set_ptt */
    .hamlib_check_rig_caps = HAMLIB_CHECK_RIG_CAPS
};
