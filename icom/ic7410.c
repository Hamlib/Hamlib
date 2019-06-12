/*
 *  Hamlib CI-V backend - description of IC-7410
 *  Copyright (c) 2011 by Stephane Fillod
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
#include <string.h>  /* String function definitions */

#include <hamlib/rig.h>
#include "token.h"
#include "idx_builtin.h"

#include "icom.h"
#include "icom_defs.h"
#include "frame.h"
#include "misc.h"
#include "bandplan.h"

#define IC7410_OTHER_TX_MODES (RIG_MODE_CW|RIG_MODE_CWR|RIG_MODE_SSB|RIG_MODE_RTTY|RIG_MODE_RTTYR|RIG_MODE_FM|RIG_MODE_PKTLSB|RIG_MODE_PKTUSB|RIG_MODE_PKTFM)
#define IC7410_AM_TX_MODES (RIG_MODE_AM)
#define IC7410_ALL_RX_MODES IC7410_OTHER_TX_MODES | IC7410_AM_TX_MODES
#define IC7410_1HZ_TS_MODES IC7410_ALL_RX_MODES

#define IC7410_FUNCS (RIG_FUNC_NB|RIG_FUNC_COMP|RIG_FUNC_VOX|RIG_FUNC_TONE|RIG_FUNC_TSQL|RIG_FUNC_SBKIN|RIG_FUNC_FBKIN|RIG_FUNC_NR|RIG_FUNC_MON|RIG_FUNC_MN|RIG_FUNC_ANF|RIG_FUNC_VSC|RIG_FUNC_LOCK|RIG_FUNC_TUNER)

#define IC7410_LEVELS (RIG_LEVEL_PREAMP|RIG_LEVEL_ATT|RIG_LEVEL_AGC|RIG_LEVEL_COMP|RIG_LEVEL_BKINDL|RIG_LEVEL_BALANCE|RIG_LEVEL_NR|RIG_LEVEL_PBT_IN|RIG_LEVEL_PBT_OUT|RIG_LEVEL_CWPITCH|RIG_LEVEL_RFPOWER|RIG_LEVEL_MICGAIN|RIG_LEVEL_KEYSPD|RIG_LEVEL_NOTCHF_RAW|RIG_LEVEL_SQL|RIG_LEVEL_RAWSTR|RIG_LEVEL_STRENGTH|RIG_LEVEL_AF|RIG_LEVEL_RF|RIG_LEVEL_VOXGAIN|RIG_LEVEL_VOXDELAY|RIG_LEVEL_SWR|RIG_LEVEL_ALC|RIG_LEVEL_RFPOWER_METER)

#define IC7410_VFOS (RIG_VFO_A|RIG_VFO_B|RIG_VFO_MEM)
#define IC7410_PARMS (RIG_PARM_ANN|RIG_PARM_BACKLIGHT)

#define IC7410_VFO_OPS (RIG_OP_CPY|RIG_OP_XCHG|RIG_OP_FROM_VFO|RIG_OP_TO_VFO|RIG_OP_MCL|RIG_OP_TUNE)
#define IC7410_SCAN_OPS (RIG_SCAN_MEM|RIG_SCAN_VFO|RIG_SCAN_PROG|RIG_SCAN_DELTA|RIG_SCAN_PRIO)

#define IC7410_ANTS (RIG_ANT_1|RIG_ANT_2)

/*
 * Guess from IC7600
 */
#define IC7410_STR_CAL { 16, \
	{ \
        {   0, -54 }, /* S0 */ \
        { 124,   0 }, /* S9 */ \
        { 246,  60 } /* S9+60dB */  \
	} }

#define IC7410_SWR_CAL { 5, \
	{ \
		 { 0, 1.0f }, \
		 { 48, 1.5f }, \
		 { 80, 2.0f }, \
		 { 120, 3.0f }, \
		 { 240, 6.0f } \
	} }

#define IC7410_ALC_CAL { 2, \
	{ \
		 { 0, 0.0f }, \
		 { 120, 1.0f } \
	} }

#define IC7410_RFPOWER_METER_CAL { 3, \
	{ \
		 { 0, 0.0f }, \
		 { 143, 0.5f }, \
		 { 213, 1.0f } \
	} }

int ic7410_set_level(RIG *rig, vfo_t vfo, setting_t level, value_t val);
int ic7410_get_level(RIG *rig, vfo_t vfo, setting_t level, value_t *val);

/*
 * IC-7410 rig capabilities.
 */
static const struct icom_priv_caps ic7410_priv_caps = {
    0x80,    /* default address */
    0,       /* 731 mode */
    0,       /* no XCHG */
    ic756pro_ts_sc_list,
    .agc_levels_present = 1,
    .agc_levels = {
            { .level = RIG_AGC_OFF, .icom_level = 0 },
            { .level = RIG_AGC_SLOW, .icom_level = 1 },
            { .level = RIG_AGC_MEDIUM, .icom_level = 2 },
            { .level = RIG_AGC_FAST, .icom_level = 3 },
            { .level = -1, .icom_level = 0 },
    },
};


const struct rig_caps ic7410_caps = {
.rig_model =  RIG_MODEL_IC7410,
.model_name = "IC-7410",
.mfg_name =  "Icom",
.version =  BACKEND_VER,
.copyright =  "LGPL",
.status =  RIG_STATUS_UNTESTED,
.rig_type =  RIG_TYPE_TRANSCEIVER,
.ptt_type =  RIG_PTT_RIG,
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
.has_get_func =  IC7410_FUNCS,
.has_set_func =  IC7410_FUNCS,
.has_get_level =  IC7410_LEVELS,
.has_set_level =  RIG_LEVEL_SET(IC7410_LEVELS),
.has_get_parm =  IC7410_PARMS,
.has_set_parm =  RIG_PARM_SET(IC7410_PARMS),	/* FIXME: parms */
.level_gran = {
	[LVL_RAWSTR] = { .min = { .i = 0 }, .max = { .i = 255 } },
	[LVL_VOXDELAY] = { .min = { .i = 0 }, .max = { .i = 20 }, .step = { .i = 1 } },
	[LVL_KEYSPD] = { .min = { .i = 6 }, .max = { .i = 48 }, .step = { .i = 1 } },
	[LVL_CWPITCH] = { .min = { .i = 300 }, .max = { .i = 900 }, .step = { .i = 1 } },
},
.parm_gran =  {},
.ctcss_list =  common_ctcss_list,
.dcs_list =  NULL,
.preamp =   { 10, 20, RIG_DBLST_END, },	/* FIXME: TBC */
.attenuator =   { 6, 12, 18, RIG_DBLST_END, },
.max_rit =  Hz(9999),
.max_xit =  Hz(9999),
.max_ifshift =  Hz(0),
.targetable_vfo =  0,
.vfo_ops =  IC7410_VFO_OPS,
.scan_ops =  IC7410_SCAN_OPS,
.transceive =  RIG_TRN_RIG,
.bank_qty =   0,
.chan_desc_sz =  0,

.chan_list =  {
	   {   1,  99, RIG_MTYPE_MEM  },
	   { 100, 101, RIG_MTYPE_EDGE },    /* two by two */
	   RIG_CHAN_END,
	},

.rx_range_list1 =   { {kHz(30),MHz(60),IC7410_ALL_RX_MODES,-1,-1,IC7410_VFOS,IC7410_ANTS},
	RIG_FRNG_END, },
.tx_range_list1 =   {
	FRQ_RNG_HF(1,IC7410_OTHER_TX_MODES, W(2),W(100),IC7410_VFOS,IC7410_ANTS),
	FRQ_RNG_6m(1,IC7410_OTHER_TX_MODES, W(2),W(100),IC7410_VFOS,IC7410_ANTS),
	FRQ_RNG_HF(1,IC7410_AM_TX_MODES, W(1),W(27),IC7410_VFOS,IC7410_ANTS),   /* AM class */
	FRQ_RNG_6m(1,IC7410_AM_TX_MODES, W(1),W(27),IC7410_VFOS,IC7410_ANTS),   /* AM class */
    	RIG_FRNG_END, },

.rx_range_list2 =   { {kHz(30),MHz(60),IC7410_ALL_RX_MODES,-1,-1,IC7410_VFOS,IC7410_ANTS},
	RIG_FRNG_END, },
.tx_range_list2 =  {
	FRQ_RNG_HF(2,IC7410_OTHER_TX_MODES, W(2),W(100),IC7410_VFOS,IC7410_ANTS),
	FRQ_RNG_6m(2,IC7410_OTHER_TX_MODES, W(2),W(100),IC7410_VFOS,IC7410_ANTS),
	FRQ_RNG_HF(2,IC7410_AM_TX_MODES, W(2),W(27),IC7410_VFOS,IC7410_ANTS),   /* AM class */
	FRQ_RNG_6m(2,IC7410_AM_TX_MODES, W(2),W(27),IC7410_VFOS,IC7410_ANTS),   /* AM class */
    /* USA only, TBC: end of range and modes */
	{MHz(5.33050),MHz(5.33350),IC7410_OTHER_TX_MODES,W(2),W(100),IC7410_VFOS,IC7410_ANTS}, /* USA only */
	{MHz(5.34650),MHz(5.34950),IC7410_OTHER_TX_MODES,W(2),W(100),IC7410_VFOS,IC7410_ANTS}, /* USA only */
	{MHz(5.36650),MHz(5.36950),IC7410_OTHER_TX_MODES,W(2),W(100),IC7410_VFOS,IC7410_ANTS}, /* USA only */
	{MHz(5.37150),MHz(5.37450),IC7410_OTHER_TX_MODES,W(2),W(100),IC7410_VFOS,IC7410_ANTS}, /* USA only */
	{MHz(5.40350),MHz(5.40650),IC7410_OTHER_TX_MODES,W(2),W(100),IC7410_VFOS,IC7410_ANTS}, /* USA only */
    	RIG_FRNG_END, },

.tuning_steps = 	{
	 {IC7410_1HZ_TS_MODES,1},
	 {IC7410_ALL_RX_MODES,Hz(100)},
	 {IC7410_ALL_RX_MODES,kHz(1)},
	 {IC7410_ALL_RX_MODES,kHz(5)},
	 {IC7410_ALL_RX_MODES,kHz(9)},
	 {IC7410_ALL_RX_MODES,kHz(10)},
	 {IC7410_ALL_RX_MODES,kHz(12.5)},
	 {IC7410_ALL_RX_MODES,kHz(20)},
	 {IC7410_ALL_RX_MODES,kHz(25)},
	 RIG_TS_END,
	},
	/* mode/filter list, remember: order matters! */
.filters = 	{
	{RIG_MODE_SSB|RIG_MODE_RTTY|RIG_MODE_RTTYR, kHz(2.4)},
	{RIG_MODE_CW|RIG_MODE_CWR, Hz(500)},
	{RIG_MODE_CW|RIG_MODE_CWR, Hz(350)},
	{RIG_MODE_AM, kHz(6)},
	{RIG_MODE_AM, kHz(2.4)},
	{RIG_MODE_FM, kHz(12)},
	{RIG_MODE_FM, kHz(8)}, /* TBC */
	RIG_FLT_END,
	},
.str_cal = IC7410_STR_CAL,
.swr_cal = IC7410_SWR_CAL,
.alc_cal = IC7410_ALC_CAL,
.rfpower_meter_cal = IC7410_RFPOWER_METER_CAL,

.cfgparams =  icom_cfg_params,
.set_conf =  icom_set_conf,
.get_conf =  icom_get_conf,

.priv =  (void*)&ic7410_priv_caps,
.rig_init =   icom_init,
.rig_cleanup =   icom_cleanup,
.rig_open =  NULL,
.rig_close =  NULL,

.set_freq =  icom_set_freq,
.get_freq =  icom_get_freq,
.set_mode =  icom_set_mode_with_data,
.get_mode =  icom_get_mode_with_data,
.set_vfo =  icom_set_vfo,
.set_ant =  icom_set_ant,
.get_ant =  icom_get_ant,

.set_rit =  icom_set_rit,

.decode_event =  icom_decode_event,
.set_level =  ic7410_set_level,
.get_level =  ic7410_get_level,
.set_func =  icom_set_func,
.get_func =  icom_get_func,
.set_parm =  icom_set_parm,
.get_parm =  icom_get_parm,
.set_mem =  icom_set_mem,
.vfo_op =  icom_vfo_op,
.scan =  icom_scan,
.set_ptt =  icom_set_ptt,
.get_ptt =  icom_get_ptt,
.get_dcd =  icom_get_dcd,
.set_ts =  icom_set_ts,
.get_ts =  icom_get_ts,
.set_rptr_shift =  icom_set_rptr_shift,
.get_rptr_shift =  icom_get_rptr_shift,
.set_rptr_offs =  icom_set_rptr_offs,
.get_rptr_offs =  icom_get_rptr_offs,
.set_ctcss_tone =  icom_set_ctcss_tone,
.get_ctcss_tone =  icom_get_ctcss_tone,
.set_ctcss_sql =  icom_set_ctcss_sql,
.get_ctcss_sql =  icom_get_ctcss_sql,
.set_split_freq =  icom_set_split_freq,
.get_split_freq =  icom_get_split_freq,
.set_split_mode =  icom_set_split_mode,
.get_split_mode =  icom_get_split_mode,
.set_split_vfo =  icom_set_split_vfo,
.get_split_vfo =  icom_mem_get_split_vfo,

};

int ic7410_set_level(RIG *rig, vfo_t vfo, setting_t level, value_t val)
{
  unsigned char cmdbuf[MAXFRAMELEN];

  rig_debug(RIG_DEBUG_VERBOSE, "%s called\n", __func__);

  switch (level) {
    case RIG_LEVEL_VOXDELAY:
      cmdbuf[0] = 0x75;
      return icom_set_level_raw(rig, level, C_CTL_MEM, 0x05, 1, cmdbuf, 1, val);
    default:
      return icom_set_level(rig, vfo, level, val);
  }
}

int ic7410_get_level(RIG *rig, vfo_t vfo, setting_t level, value_t *val)
{
  unsigned char cmdbuf[MAXFRAMELEN];

  rig_debug(RIG_DEBUG_VERBOSE, "%s called\n", __func__);

  switch (level) {
    case RIG_LEVEL_VOXDELAY:
      cmdbuf[0] = 0x75;
      return icom_get_level_raw(rig, level, C_CTL_MEM, 0x05, 1, cmdbuf, val);
    default:
      return icom_get_level(rig, vfo, level, val);
  }
}
