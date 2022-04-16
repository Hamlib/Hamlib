/*
 *  Hamlib CI-V backend - description of IC-820H (VHF/UHF All-Mode Transceiver)
 *  Contributed by Francois Retief <fgretief@sun.ac.za>
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

#include <hamlib/rig.h>
#include "icom.h"


#define IC820H_MODES (RIG_MODE_SSB|RIG_MODE_CW|RIG_MODE_FM)

#define IC820H_VFO_ALL (RIG_VFO_A|RIG_VFO_C|RIG_VFO_MEM)
/* FIXME: What about MAIN/SUB mode? And satellite mode? */

#define IC820H_VFO_OPS (RIG_OP_FROM_VFO|RIG_OP_TO_VFO|RIG_OP_CPY|RIG_OP_MCL)

#define IC820H_SCAN_OPS (RIG_SCAN_MEM)
/* FIXME: Manual talks about 3 modes: Programmed scan, Memory scan and
 * Mode select memory scan operation. How do i encode these?
 */


#define IC820H_STR_CAL { 0, { } }
/*
 */
static const struct icom_priv_caps ic820h_priv_caps =
{
    0x42,           /* default address */
    1,          /* 731 mode */
    0,          /* no XCHG */
    ic737_ts_sc_list
};

const struct rig_caps ic820h_caps =
{
    RIG_MODEL(RIG_MODEL_IC820),
    .model_name = "IC-820H",
    .mfg_name =  "Icom",
    .version =  BACKEND_VER ".0",
    .copyright =  "LGPL",
    .status =  RIG_STATUS_BETA,
    .rig_type =  RIG_TYPE_TRANSCEIVER,
    .ptt_type =  RIG_PTT_NONE,
    .dcd_type =  RIG_DCD_NONE,
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
    .has_get_func =  RIG_FUNC_NONE,
    .has_set_func =  RIG_FUNC_NONE,
    .has_get_level =  RIG_LEVEL_NONE,
    .has_set_level =  RIG_LEVEL_NONE,
    .has_get_parm =  RIG_PARM_NONE,
    .has_set_parm =  RIG_PARM_NONE,
    .level_gran =  {},
    .parm_gran =  {},
    .ctcss_list =  NULL,
    .dcs_list =  NULL,
    .preamp =   { RIG_DBLST_END, },
    .attenuator =   { RIG_DBLST_END, },    /* Attanuator 15dB for each band. manual button */
    .max_rit =  Hz(0),     /* SSB,CW: +-1.0kHz  FM: +-5.0kHz */
    .max_xit =  Hz(0),
    .max_ifshift =  Hz(0),  /* 1.2kHz manual knob */
    .targetable_vfo =  0,
    .vfo_ops =  IC820H_VFO_OPS,
    .scan_ops =  IC820H_SCAN_OPS,
    .transceive =  RIG_TRN_RIG,
    .bank_qty =   0,
    .chan_desc_sz =  0,

    .chan_list =  {
        /* FIXME: Each band has 80 channels (2*80) ?? */
        {   1,  80, RIG_MTYPE_MEM  },
        {  81,  82, RIG_MTYPE_EDGE },
        RIG_CHAN_END,
    },

    .rx_range_list1 =   {
        {MHz(136), MHz(174), IC820H_MODES, -1, -1, IC820H_VFO_ALL},
        {MHz(430), MHz(450), IC820H_MODES, -1, -1, IC820H_VFO_ALL},
        RIG_FRNG_END,
    },
    .tx_range_list1 =  {
        {MHz(144), MHz(146), RIG_MODE_SSB, W(6), W(35), IC820H_VFO_ALL},
        {MHz(144), MHz(146), RIG_MODE_FM | RIG_MODE_CW, W(6), W(45), IC820H_VFO_ALL},
        {MHz(430), MHz(440), RIG_MODE_SSB, W(6), W(30), IC820H_VFO_ALL},
        {MHz(430), MHz(440), RIG_MODE_FM | RIG_MODE_CW, W(6), W(40), IC820H_VFO_ALL},
        RIG_FRNG_END,
    },

    .rx_range_list2 =   {
        {MHz(136), MHz(174), IC820H_MODES, -1, -1, IC820H_VFO_ALL},
        {MHz(430), MHz(450), IC820H_MODES, -1, -1, IC820H_VFO_ALL},
        RIG_FRNG_END,
    },
    /*
     *   From manual:    VHF                UHF
     * USA           144.0-148.0 MHz    430.0-450.0 MHz
     * Europe        144.0-146.0 MHz    430.0-440.0 MHz
     * Australia     144.0-148.0 MHz    430.0-450.0 MHz
     * Sweden        144.0-146.0 MHz    432.0-438.0 MHz
     */
    .tx_range_list2 =  {
        {MHz(144), MHz(148), RIG_MODE_SSB, W(6), W(35), IC820H_VFO_ALL},
        {MHz(144), MHz(148), RIG_MODE_FM | RIG_MODE_CW, W(6), W(45), IC820H_VFO_ALL},
        {MHz(430), MHz(450), RIG_MODE_SSB, W(6), W(30), IC820H_VFO_ALL},
        {MHz(430), MHz(450), RIG_MODE_FM | RIG_MODE_CW, W(6), W(40), IC820H_VFO_ALL},
        RIG_FRNG_END,
    },

    .tuning_steps =     {
        {RIG_MODE_SSB | RIG_MODE_CW, 1},
        {RIG_MODE_SSB | RIG_MODE_CW, 10},
        {RIG_MODE_SSB | RIG_MODE_CW, 50},
        {RIG_MODE_SSB | RIG_MODE_CW, 100},
        {RIG_MODE_FM, kHz(5)},
        RIG_TS_END,
    },
    /* mode/filter list, remember: order matters! */
    .filters =  {
        {RIG_MODE_CW | RIG_MODE_SSB, kHz(2.3)}, /* builtin */
        {RIG_MODE_FM, kHz(15)},                 /* builtin */
        RIG_FLT_END,
    },

    .cfgparams =  icom_cfg_params,
    .set_conf =  icom_set_conf,
    .get_conf =  icom_get_conf,

    .priv = (void *)& ic820h_priv_caps,
    .rig_init =   icom_init,
    .rig_cleanup =   icom_cleanup,
    .rig_open =  icom_rig_open,
    .rig_close =  icom_rig_close,

    .set_freq =  icom_set_freq,
    .get_freq =  icom_get_freq,
    .set_mode =  icom_set_mode,
    .get_mode =  icom_get_mode,
    .set_vfo =  icom_set_vfo,

    .decode_event =  icom_decode_event,
    .set_mem =  icom_set_mem,
    .vfo_op =  icom_vfo_op,
    .scan =  icom_scan,

    .hamlib_check_rig_caps = HAMLIB_CHECK_RIG_CAPS
};

