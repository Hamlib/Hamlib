/*
 *  Hamlib CI-V backend - IC-R7000 and IC-R7100 descriptions
 *  Copyright (c) 2000-2004 by Stephane Fillod
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
#include "serial.h"
#include "misc.h"
#include "idx_builtin.h"

#include "icom.h"
#include "icom_defs.h"
#include "frame.h"


#define ICR7000_MODES (RIG_MODE_AM|RIG_MODE_SSB|RIG_MODE_FM|RIG_MODE_WFM)

#define ICR7000_OPS (RIG_OP_FROM_VFO|RIG_OP_MCL)

static int r7000_set_freq(RIG *rig, vfo_t vfo, freq_t freq);

#define ICR7100_FUNCS (RIG_FUNC_VSC)
#define ICR7100_LEVELS (RIG_LEVEL_ATT|RIG_LEVEL_AF|RIG_LEVEL_SQL|RIG_LEVEL_RAWSTR)
#define ICR7100_PARMS (RIG_PARM_ANN)
#define ICR7100_SCAN_OPS (RIG_SCAN_MEM) /* TBC */

/* FIXME: S-Meter measurements */
#define ICR7100_STR_CAL UNKNOWN_IC_STR_CAL

static struct icom_priv_caps icr7000_priv_caps =
{
    0x08,   /* default address */
    0,      /* 731 mode */
    0,      /* no XCHG */
    r7100_ts_sc_list
};
/*
 * ICR7000 rigs capabilities.
 */
const struct rig_caps icr7000_caps =
{
    RIG_MODEL(RIG_MODEL_ICR7000),
    .model_name = "IC-R7000",
    .mfg_name =  "Icom",
    .version =  BACKEND_VER ".0",
    .copyright =  "LGPL",
    .status =  RIG_STATUS_BETA,
    .rig_type =  RIG_TYPE_RECEIVER,
    .ptt_type =  RIG_PTT_NONE,
    .dcd_type =  RIG_DCD_NONE,
    .port_type =  RIG_PORT_SERIAL,
    .serial_rate_min =  300,
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
    .ctcss_list =  NULL,
    .dcs_list =  NULL,
    .preamp =   { RIG_DBLST_END, },
    .attenuator =   { RIG_DBLST_END, },
    .max_rit =  Hz(0),
    .max_xit =  Hz(0),
    .max_ifshift =  Hz(0),
    .targetable_vfo =  0,
    .vfo_ops =  ICR7000_OPS,
    .transceive =  RIG_TRN_RIG,
    .bank_qty =   0,
    .chan_desc_sz =  0,

    .chan_list =  {
        {   1,  99, RIG_MTYPE_MEM },
        RIG_CHAN_END,
    },

    .rx_range_list1 =  {
        {MHz(25), MHz(1000), ICR7000_MODES, -1, -1, RIG_VFO_A},
        {MHz(1025), MHz(2000), ICR7000_MODES, -1, -1, RIG_VFO_A},
        RIG_FRNG_END,
    },
    .tx_range_list1 =  { RIG_FRNG_END, },

    .rx_range_list2 =  {
        {MHz(25), MHz(1000), ICR7000_MODES, -1, -1, RIG_VFO_A},
        {MHz(1025), MHz(2000), ICR7000_MODES, -1, -1, RIG_VFO_A},
        RIG_FRNG_END,
    },
    .tx_range_list2 =  { RIG_FRNG_END, },   /* no TX ranges, this is a receiver */

    .tuning_steps =  {
        {ICR7000_MODES, 100},  /* resolution */
#if 0
        {ICR7000_MODES, kHz(1)},
        {ICR7000_MODES, kHz(5)},
        {ICR7000_MODES, kHz(10)},
        {ICR7000_MODES, 12500},
        {ICR7000_MODES, kHz(25)},
#endif
        RIG_TS_END,
    },
    /* mode/filter list, remember: order matters! */
    .filters =  {
        {RIG_MODE_SSB, kHz(2.8)},
        {RIG_MODE_FM | RIG_MODE_AM, kHz(15)},
        {RIG_MODE_FM | RIG_MODE_AM, kHz(6)}, /* narrow */
        {RIG_MODE_WFM, kHz(150)},
        RIG_FLT_END,
    },

    .cfgparams =  icom_cfg_params,
    .set_conf =  icom_set_conf,
    .get_conf =  icom_get_conf,

    .priv = (void *)& icr7000_priv_caps,
    .rig_init =   icom_init,
    .rig_cleanup =   icom_cleanup,
    .rig_open =  icom_rig_open,
    .rig_close =  icom_rig_close,

    .set_freq =  r7000_set_freq,
    .get_freq =  icom_get_freq,
    .set_mode =  icom_set_mode,
    .get_mode =  icom_get_mode,
    .set_vfo =  icom_set_vfo,

    .decode_event =  icom_decode_event,
    .set_mem =  icom_set_mem,
    .vfo_op =  icom_vfo_op,
    .hamlib_check_rig_caps = HAMLIB_CHECK_RIG_CAPS
};


static struct icom_priv_caps icr7100_priv_caps =
{
    0x34,   /* default address */
    0,      /* 731 mode */
    0,      /* no XCHG */
    r7100_ts_sc_list
};
/*
 * ICR7100A rig capabilities.
 */
const struct rig_caps icr7100_caps =
{
    RIG_MODEL(RIG_MODEL_ICR7100),
    .model_name = "IC-R7100",
    .mfg_name =  "Icom",
    .version =  BACKEND_VER ".0",
    .copyright =  "LGPL",
    .status =  RIG_STATUS_ALPHA,
    .rig_type =  RIG_TYPE_RECEIVER,
    .ptt_type =  RIG_PTT_NONE,
    .dcd_type =  RIG_DCD_RIG,
    .port_type =  RIG_PORT_SERIAL,
    .serial_rate_min =  300,
    .serial_rate_max =  1200,
    .serial_data_bits =  8,
    .serial_stop_bits =  1,
    .serial_parity =  RIG_PARITY_NONE,
    .serial_handshake =  RIG_HANDSHAKE_NONE,
    .write_delay =  0,
    .post_write_delay =  0,
    .timeout =  1000,
    .retry =  3,

    .has_get_func =  ICR7100_FUNCS,
    .has_set_func =  ICR7100_FUNCS,
    .has_get_level =  ICR7100_LEVELS,
    .has_set_level =  RIG_LEVEL_SET(ICR7100_LEVELS),
    .has_get_parm =  ICR7100_PARMS,
    .has_set_parm =  RIG_PARM_SET(ICR7100_PARMS),
    .level_gran = {
        // cppcheck-suppress *
        [LVL_RAWSTR] = { .min = { .i = 0 }, .max = { .i = 255 } },
    },
    .parm_gran =  {},
    .ctcss_list =  NULL,
    .dcs_list =  NULL,
    .preamp =   { RIG_DBLST_END },
    .attenuator =   { 20, RIG_DBLST_END },
    .max_rit =  Hz(0),
    .max_xit =  Hz(0),
    .max_ifshift =  Hz(0),
    .targetable_vfo =  0,
    .vfo_ops =  ICR7000_OPS,
    .scan_ops =  ICR7100_SCAN_OPS,
    .transceive =  RIG_TRN_RIG,
    .bank_qty =   0,
    .chan_desc_sz =  0,

    .chan_list =  {
        {     1,   99, RIG_MTYPE_MEM },     /* TBC */
        { 0x0900, 0x0909, RIG_MTYPE_EDGE }, /* 2 by 2 */
        { 0x0910, 0x0919, RIG_MTYPE_EDGE }, /* 2 by 2 */
        RIG_CHAN_END,
    },

    .rx_range_list1 =  {
        {MHz(25), MHz(1999.9999), ICR7000_MODES, -1, -1, RIG_VFO_A},
        RIG_FRNG_END,
    },
    .tx_range_list1 =  { RIG_FRNG_END, },

    .rx_range_list2 =  {
        {MHz(25), MHz(1999.9999), ICR7000_MODES, -1, -1, RIG_VFO_A},
        RIG_FRNG_END,
    },
    .tx_range_list2 =  { RIG_FRNG_END, },   /* no TX ranges, this is a receiver */

    .tuning_steps =  {
        {ICR7000_MODES, 100},  /* resolution */
        {ICR7000_MODES, kHz(1)},
        {ICR7000_MODES, kHz(5)},
        {ICR7000_MODES, kHz(10)},
        {ICR7000_MODES, 12500},
        {ICR7000_MODES, kHz(20)},
        {ICR7000_MODES, kHz(25)},
        {ICR7000_MODES, kHz(100)},
        RIG_TS_END,
    },
    /* mode/filter list, remember: order matters! */
    .filters =  {
        {RIG_MODE_SSB, kHz(2.4)},
        {RIG_MODE_FM | RIG_MODE_AM, kHz(15)},
        {RIG_MODE_FM | RIG_MODE_AM, kHz(6)}, /* narrow */
        {RIG_MODE_WFM, kHz(150)},
        RIG_FLT_END,
    },
    .str_cal = ICR7100_STR_CAL,

    .cfgparams =  icom_cfg_params,
    .set_conf =  icom_set_conf,
    .get_conf =  icom_get_conf,

    .priv = (void *)& icr7100_priv_caps,
    .rig_init =   icom_init,
    .rig_cleanup =   icom_cleanup,
    .rig_open =  NULL,
    .rig_close =  NULL,

    .set_freq =  r7000_set_freq,    /* TBC for R7100 */
    .get_freq =  icom_get_freq,
    .set_mode =  icom_set_mode,
    .get_mode =  icom_get_mode,
    .set_vfo =  icom_set_vfo,

    .set_ts =  icom_set_ts,
    .get_ts =  icom_get_ts,
    .set_func =  icom_set_func,
    .get_func =  icom_get_func,
    .set_level =  icom_set_level,
    .get_level =  icom_get_level,
#ifdef XXREMOVEDXX
    .set_parm =  icom_set_parm,
    .get_parm =  icom_get_parm,
#endif

    .decode_event =  icom_decode_event,
    .set_mem =  icom_set_mem,
    .vfo_op =  icom_vfo_op,
    .scan =  icom_scan,
    .get_dcd =  icom_get_dcd,
};


/*
 * Function definitions below
 */


/*
 * r7000_set_freq
 */
static int r7000_set_freq(RIG *rig, vfo_t vfo, freq_t freq)
{
    long long f = (long long)freq;

    /*
     * The R7000 cannot set frequencies higher than 1GHz,
     * this is done by flipping a switch on the front panel and
     * stripping the most significant digit.
     * This is the only change with the common icom_set_freq
     */
    f %= (long long)GHz(1);

    return icom_set_freq(rig, vfo, (freq_t)f);
}

