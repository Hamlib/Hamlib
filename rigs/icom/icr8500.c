/*
 *  Hamlib CI-V backend - IC-R8500 description
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

#include "hamlib/rig.h"
#include "idx_builtin.h"
#include "icom.h"
#include "icom_defs.h"

#define ICR8500_MODES (RIG_MODE_AM|RIG_MODE_CW|RIG_MODE_SSB|RIG_MODE_RTTY|RIG_MODE_FM|RIG_MODE_WFM)
#define ICR8500_1MHZ_TS_MODES (RIG_MODE_AM|RIG_MODE_FM|RIG_MODE_WFM)

#define ICR8500_FUNC_ALL (RIG_FUNC_FAGC|RIG_FUNC_NB|RIG_FUNC_TSQL|RIG_FUNC_APF)

#define ICR8500_LEVEL_ALL (RIG_LEVEL_ATT|RIG_LEVEL_APF|RIG_LEVEL_SQL|RIG_LEVEL_IF|RIG_LEVEL_RAWSTR)

#define ICR8500_OPS (RIG_OP_CPY|RIG_OP_XCHG|RIG_OP_FROM_VFO|RIG_OP_TO_VFO|RIG_OP_MCL)

#define ICR8500_SCAN_OPS (RIG_SCAN_MEM|RIG_SCAN_VFO|RIG_SCAN_SLCT|RIG_SCAN_PRIO)

/* FIXME: real measure */
#define ICR8500_STR_CAL { 16,  { \
        {   0, -54 },   /* S0 */ \
        {  10, -48 }, \
        {  32, -42 }, \
        {  46, -36 }, \
        {  62, -30 }, \
        {  82, -24 }, \
        {  98, -18 }, \
        { 112, -12 }, \
        { 124,  -6 }, \
        { 134,   0 }, /* S9 */ \
        { 156,  10 }, \
        { 177,  20 }, \
        { 192,  30 }, \
        { 211,  40 }, \
        { 228,  50 }, \
        { 238,  60 }, \
    } }

int icr8500_set_func(RIG *rig, vfo_t vfo, setting_t func, int status);

static struct icom_priv_caps icr8500_priv_caps =
{
    0x4a,   /* default address */
    0,      /* 731 mode */
    0,      /* no XCHG */
    r8500_ts_sc_list
};

/*
 * IC-R8500 rig capabilities.
 */
const struct rig_caps icr8500_caps =
{
    RIG_MODEL(RIG_MODEL_ICR8500),
    .model_name = "ICR-8500",
    .mfg_name =  "Icom",
    .version =  BACKEND_VER ".0",
    .copyright =  "LGPL",
    .status =  RIG_STATUS_STABLE,
    .rig_type =  RIG_TYPE_RECEIVER,
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

    .has_get_func =  RIG_FUNC_NONE,
    .has_set_func =  ICR8500_FUNC_ALL,
    .has_get_level =  ICR8500_LEVEL_ALL,
    .has_set_level =  RIG_LEVEL_SET(ICR8500_LEVEL_ALL | RIG_LEVEL_AF),
    .has_get_parm =  RIG_PARM_NONE,
    .has_set_parm =  RIG_PARM_NONE,    /* FIXME: parms */
    .level_gran =
    {
#include "level_gran_icom.h"
        // cppcheck-suppress *
        [LVL_RAWSTR] = { .min = { .i = 0 }, .max = { .i = 255 } },
        [LVL_IF] = { .min = { .i = 0 }, .max = { .i = 255 }, .step = { .i = 1 } },
    },
    .parm_gran =  {},
    .ctcss_list =  NULL,    /* FIXME: CTCSS/DCS list */
    .dcs_list =  NULL,
    .preamp =   { RIG_DBLST_END, },
    .attenuator =   { 10, 20, 30, RIG_DBLST_END, },
    .max_rit =  Hz(9999),
    .max_xit =  Hz(0),
    .max_ifshift =  kHz(1.2),
    .targetable_vfo =  0,
    .vfo_ops =  ICR8500_OPS,
    .scan_ops =  ICR8500_SCAN_OPS,
    .transceive =  RIG_TRN_RIG,
    .bank_qty =   12,
    .chan_desc_sz =  0,
    .str_cal = ICR8500_STR_CAL,

    .chan_list =  {     /* FIXME: memory channel list */
        {   1, 999, RIG_MTYPE_MEM, IC_MIN_MEM_CAP },
        RIG_CHAN_END,
    },


    .rx_range_list1 =  {
        {kHz(100), MHz(824) - 10, ICR8500_MODES, -1, -1, RIG_VFO_A},
        {MHz(849) + 10, MHz(869) - 10, ICR8500_MODES, -1, -1, RIG_VFO_A},
        {MHz(894) + 10, GHz(2) - 10, ICR8500_MODES, -1, -1, RIG_VFO_A},
        RIG_FRNG_END,
    },
    .tx_range_list1 =  { RIG_FRNG_END, },

    .rx_range_list2 =  {
        {kHz(100), MHz(824) - 10, ICR8500_MODES, -1, -1, RIG_VFO_A},
        {MHz(849) + 10, MHz(869) - 10, ICR8500_MODES, -1, -1, RIG_VFO_A},
        {MHz(894) + 10, GHz(2) - 10, ICR8500_MODES, -1, -1, RIG_VFO_A},
        RIG_FRNG_END,
    },
    .tx_range_list2 =  { RIG_FRNG_END, },   /* no TX ranges, this is a receiver */

    .tuning_steps =  {
        {ICR8500_MODES, 10},
        {ICR8500_MODES, 50},
        {ICR8500_MODES, 100},
        {ICR8500_MODES, kHz(1)},
        {ICR8500_MODES, 2500},
        {ICR8500_MODES, kHz(5)},
        {ICR8500_MODES, kHz(9)},
        {ICR8500_MODES, kHz(10)},
        {ICR8500_MODES, 12500},
        {ICR8500_MODES, kHz(20)},
        {ICR8500_MODES, kHz(25)},
        {ICR8500_MODES, kHz(100)},
        {ICR8500_1MHZ_TS_MODES, MHz(1)},
        RIG_TS_END,
    },
    /* mode/filter list, remember: order matters! */
    .filters =  {
        /* FIXME: To be confirmed! --SF */
        {RIG_MODE_SSB | RIG_MODE_CW | RIG_MODE_RTTY, kHz(2.4)},
        {RIG_MODE_AM, kHz(8)},
        {RIG_MODE_AM, kHz(2.4)},    /* narrow */
        {RIG_MODE_AM, kHz(15)},     /* wide */
        {RIG_MODE_FM, kHz(15)},
        {RIG_MODE_FM, kHz(8)},  /* narrow */
        {RIG_MODE_WFM, kHz(230)},
        RIG_FLT_END,
    },

    .cfgparams =  icom_cfg_params,
    .set_conf =  icom_set_conf,
    .get_conf =  icom_get_conf,

    .priv = (void *)& icr8500_priv_caps,
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
    .set_level =  icom_set_level,
    .get_level =  icom_get_level,
    .set_func =  icr8500_set_func,
    .set_mem =  icom_set_mem,
    .vfo_op =  icom_vfo_op,
    .scan =  icom_scan,
    .set_ts =  icom_set_ts,
    .get_ts =  icom_get_ts,
    .get_dcd =  icom_get_dcd,
    .hamlib_check_rig_caps = HAMLIB_CHECK_RIG_CAPS
};

int icr8500_set_func(RIG *rig, vfo_t vfo, setting_t func, int status)
{
    switch (func)
    {
    case RIG_FUNC_NB:
        return icom_set_raw(rig, C_CTL_FUNC, status ? S_FUNC_NBON : S_FUNC_NBOFF, 0,
                            NULL, 0, 0);

    case RIG_FUNC_FAGC:
        return icom_set_raw(rig, C_CTL_FUNC, status ? S_FUNC_AGCON : S_FUNC_AGCOFF, 0,
                            NULL, 0, 0);

    case RIG_FUNC_APF:
        return icom_set_raw(rig, C_CTL_FUNC, status ? S_FUNC_APFON : S_FUNC_APFOFF, 0,
                            NULL, 0, 0);

    default:
        return icom_set_func(rig, vfo, func, status);
    }
}
