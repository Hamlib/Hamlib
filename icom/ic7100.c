/*
 *  Hamlib CI-V backend - description of IC-9100 (HF/VHF/UHF All-Mode Tranceiver)
 *  Copyright (c) 2000-2011 by Stephane Fillod
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

#include <hamlib/rig.h>
#include "icom.h"
#include "icom_defs.h"
#include "frame.h"
#include "idx_builtin.h"
#include "bandplan.h"


#define IC7100_MODES (RIG_MODE_SSB|RIG_MODE_CW|RIG_MODE_CWR|\
        RIG_MODE_AM|RIG_MODE_FM|RIG_MODE_RTTY|RIG_MODE_RTTYR|\
        RIG_MODE_PKTLSB|RIG_MODE_PKTUSB|RIG_MODE_PKTAM|RIG_MODE_PKTFM)

#define IC7100_OTHER_TX_MODES ((IC7100_MODES) & ~(RIG_MODE_AM|RIG_MODE_PKTAM))

#define IC7100_VFO_ALL (RIG_VFO_A|RIG_VFO_B|RIG_VFO_MAIN|RIG_VFO_SUB|RIG_VFO_MEM)

#define IC7100_SCAN_OPS (RIG_SCAN_VFO|RIG_SCAN_MEM|RIG_SCAN_SLCT|RIG_SCAN_PRIO)

#define IC7100_VFO_OPS      (RIG_OP_FROM_VFO| \
                            RIG_OP_TO_VFO| \
                            RIG_OP_CPY| \
                            RIG_OP_MCL| \
                            RIG_OP_XCHG| \
                            RIG_OP_TUNE)

#define IC7100_FUNC_ALL     (RIG_FUNC_NB| \
                            RIG_FUNC_NR| \
                            RIG_FUNC_ANF| \
                            RIG_FUNC_TONE| \
                            RIG_FUNC_TSQL| \
                            RIG_FUNC_COMP| \
                            RIG_FUNC_VOX| \
                            RIG_FUNC_FBKIN| \
                            RIG_FUNC_AFC| \
                            RIG_FUNC_SATMODE| \
                            RIG_FUNC_VSC| \
                            RIG_FUNC_MN| \
                            RIG_FUNC_LOCK| \
                            RIG_FUNC_SCOPE| \
                            RIG_FUNC_TUNER)

#define IC7100_LEVEL_ALL    (RIG_LEVEL_AF| \
                            RIG_LEVEL_RF| \
                            RIG_LEVEL_SQL| \
                            RIG_LEVEL_IF| \
                            RIG_LEVEL_NR| \
                            RIG_LEVEL_CWPITCH| \
                            RIG_LEVEL_RFPOWER| \
                            RIG_LEVEL_MICGAIN| \
                            RIG_LEVEL_KEYSPD| \
                            RIG_LEVEL_COMP| \
                            RIG_LEVEL_VOXGAIN| \
                            RIG_LEVEL_VOXDELAY| \
                            RIG_LEVEL_ANTIVOX| \
                            RIG_LEVEL_APF| \
                            RIG_LEVEL_AGC| \
                            RIG_LEVEL_PBT_IN| \
                            RIG_LEVEL_PBT_OUT| \
                            RIG_LEVEL_NOTCHF_RAW| \
                            RIG_LEVEL_ATT| \
                            RIG_LEVEL_PREAMP| \
                            RIG_LEVEL_RAWSTR| \
                            RIG_LEVEL_STRENGTH| \
                            RIG_LEVEL_SWR| \
                            RIG_LEVEL_ALC| \
                            RIG_LEVEL_RFPOWER_METER| \
                            RIG_LEVEL_COMP_METER| \
                            RIG_LEVEL_VD_METER| \
                            RIG_LEVEL_ID_METER| \
                            RIG_LEVEL_MONITOR_GAIN| \
                            RIG_LEVEL_NB)

#define IC7100_PARM_ALL (RIG_PARM_ANN|RIG_PARM_BACKLIGHT)

// IC-7100 S-meter calibration data based on manual
#define IC7100_STR_CAL { 14, \
	{ \
		{   0, -54 }, \
		{ 120,  0 }, \
		{ 241,  60 } \
	} }

#define IC7100_SWR_CAL { 5, \
	{ \
		 { 0, 1.0f }, \
		 { 48, 1.5f }, \
		 { 80, 2.0f }, \
		 { 120, 3.0f }, \
		 { 240, 6.0f } \
	} }

#define IC7100_ALC_CAL { 2, \
	{ \
		 { 0, 0.0f }, \
		 { 120, 1.0f } \
	} }

#define IC7100_RFPOWER_METER_CAL { 3, \
	{ \
		 { 0, 0.0f }, \
		 { 143, 0.5f }, \
		 { 213, 1.0f } \
	} }

#define IC7100_COMP_METER_CAL { 3, \
	{ \
		 { 0, 0.0f }, \
		 { 130, 15.0f }, \
		 { 241, 30.0f } \
	} }

#define IC7100_VD_METER_CAL { 3, \
	{ \
		 { 0, 0.0f }, \
		 { 13, 10.0f }, \
		 { 241, 16.0f } \
	} }

#define IC7100_ID_METER_CAL { 4, \
	{ \
		 { 0, 0.0f }, \
		 { 97, 10.0f }, \
		 { 146, 15.0f }, \
		 { 241, 25.0f } \
	} }

#define IC7100_HF_ANTS (RIG_ANT_1|RIG_ANT_2)

int ic7100_set_level(RIG *rig, vfo_t vfo, setting_t level, value_t val);
int ic7100_get_level(RIG *rig, vfo_t vfo, setting_t level, value_t *val);

/*
 * IC-7100 rig capabilities.
 */
static const struct icom_priv_caps ic7100_priv_caps = {
    0x88,           /* default address */
    0,              /* 731 mode */
    0,              /* no XCHG */
    ic7100_ts_sc_list,   /* FIXME */
    .agc_levels_present = 1,
    .agc_levels = {
            { .level = RIG_AGC_FAST, .icom_level = 1 },
            { .level = RIG_AGC_MEDIUM, .icom_level = 2 },
            { .level = RIG_AGC_SLOW, .icom_level = 3 },
            { .level = -1, .icom_level = 0 },
    },
};

const struct rig_caps ic7100_caps = {
.rig_model =  RIG_MODEL_IC7100,
.model_name = "IC-7100",
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
.has_get_func =  IC7100_FUNC_ALL,
.has_set_func =  IC7100_FUNC_ALL | RIG_FUNC_RESUME,
.has_get_level =  IC7100_LEVEL_ALL,
.has_set_level =  RIG_LEVEL_SET(IC7100_LEVEL_ALL),
.has_get_parm =  IC7100_PARM_ALL,
.has_set_parm =  IC7100_PARM_ALL,
.level_gran = {
	[LVL_RAWSTR] = { .min = { .i = 0 }, .max = { .i = 255 } },
	[LVL_VOXDELAY] = { .min = { .i = 0 }, .max = { .i = 20 }, .step = { .i = 1 } },
	[LVL_KEYSPD] = { .min = { .i = 6 }, .max = { .i = 48 }, .step = { .i = 1 } },
	[LVL_CWPITCH] = { .min = { .i = 300 }, .max = { .i = 900 }, .step = { .i = 1 } },
},
.parm_gran =  {},
.ctcss_list =  common_ctcss_list,
.dcs_list =  common_dcs_list,
.preamp =   {20, RIG_DBLST_END, },
.attenuator =   {20, RIG_DBLST_END, },
.max_rit =  kHz(9.999),
.max_xit =  kHz(9.999),
.max_ifshift =  Hz(0),
.targetable_vfo =  0,
.vfo_ops =  IC7100_VFO_OPS,
.scan_ops =  IC7100_SCAN_OPS,
.transceive =  RIG_TRN_RIG,
.bank_qty =   0,
.chan_desc_sz =  9, /* TODO */

.chan_list =  { /* TBC */
    {   1, 396, RIG_MTYPE_MEM  },
    { 397, 400, RIG_MTYPE_CALL },
    { 401, 424, RIG_MTYPE_EDGE },
    RIG_CHAN_END, },

.rx_range_list1 =   { /* Europe */
    {kHz(30),MHz(60),IC7100_MODES,-1,-1,IC7100_VFO_ALL,IC7100_HF_ANTS},
    {kHz(136),MHz(174),IC7100_MODES,-1,-1,IC7100_VFO_ALL,RIG_ANT_3},
    {MHz(420),MHz(480),IC7100_MODES,-1,-1,IC7100_VFO_ALL,RIG_ANT_4},
    {MHz(1240),MHz(1320),IC7100_MODES,-1,-1,IC7100_VFO_ALL,RIG_ANT_5},
    RIG_FRNG_END, },
.tx_range_list1 =  {
    FRQ_RNG_HF(1,IC7100_OTHER_TX_MODES, W(2),W(100),IC7100_VFO_ALL,IC7100_HF_ANTS),
    FRQ_RNG_HF(1,RIG_MODE_AM, W(2),W(25),IC7100_VFO_ALL,IC7100_HF_ANTS), /* only HF */
    FRQ_RNG_6m(1,IC7100_OTHER_TX_MODES, W(2),W(100),IC7100_VFO_ALL,IC7100_HF_ANTS),
    FRQ_RNG_2m(1,IC7100_OTHER_TX_MODES, W(2),W(100),IC7100_VFO_ALL,RIG_ANT_3),
    FRQ_RNG_70cm(1,IC7100_OTHER_TX_MODES, W(2),W(75),IC7100_VFO_ALL,RIG_ANT_4),
    /* option */
    FRQ_RNG_23cm_REGION1(IC7100_OTHER_TX_MODES, W(1),W(10),IC7100_VFO_ALL,RIG_ANT_5),
    RIG_FRNG_END, },

.rx_range_list2 =   {  /* USA */
    {kHz(30),MHz(60),IC7100_MODES,-1,-1,IC7100_VFO_ALL,IC7100_HF_ANTS},
    {kHz(136),MHz(174),IC7100_MODES,-1,-1,IC7100_VFO_ALL,RIG_ANT_3},
    {MHz(420),MHz(480),IC7100_MODES,-1,-1,IC7100_VFO_ALL,RIG_ANT_4},
    {MHz(1240),MHz(1320),IC7100_MODES,-1,-1,IC7100_VFO_ALL,RIG_ANT_5},
    RIG_FRNG_END, },
.tx_range_list2 =  {
    FRQ_RNG_HF(2,IC7100_OTHER_TX_MODES, W(2),W(100),IC7100_VFO_ALL,IC7100_HF_ANTS),
    FRQ_RNG_HF(2,RIG_MODE_AM, W(2),W(25),IC7100_VFO_ALL,IC7100_HF_ANTS), /* only HF */
    /* USA only, TBC: end of range and modes */
	{MHz(5.255),MHz(5.405),IC7100_OTHER_TX_MODES,W(2),W(100),IC7100_VFO_ALL,IC7100_HF_ANTS}, /* USA only */
	{MHz(5.255),MHz(5.405),RIG_MODE_AM,W(2),W(100),IC7100_VFO_ALL,IC7100_HF_ANTS}, /* USA only */
    FRQ_RNG_6m(2,IC7100_OTHER_TX_MODES, W(2),W(100),IC7100_VFO_ALL,IC7100_HF_ANTS),
    FRQ_RNG_2m(2,IC7100_OTHER_TX_MODES, W(2),W(100),IC7100_VFO_ALL,RIG_ANT_3),
    FRQ_RNG_70cm(2,IC7100_OTHER_TX_MODES, W(2),W(75),IC7100_VFO_ALL,RIG_ANT_4),
    /* option */
    FRQ_RNG_23cm_REGION2(IC7100_OTHER_TX_MODES, W(1),W(10),IC7100_VFO_ALL,RIG_ANT_5),
    RIG_FRNG_END, },

.tuning_steps =     {
    {RIG_MODE_SSB|RIG_MODE_CW,1},
    {RIG_MODE_SSB|RIG_MODE_CW,10},
    {RIG_MODE_SSB|RIG_MODE_CW,50},
    {RIG_MODE_SSB|RIG_MODE_CW,100},
    {RIG_MODE_FM,kHz(0.1)},
    {RIG_MODE_FM,kHz(5)},
    {RIG_MODE_FM,kHz(6.25)},
    {RIG_MODE_FM,kHz(10)},
    {RIG_MODE_FM,kHz(12.5)},
    {RIG_MODE_FM,kHz(20)},
    {RIG_MODE_FM,kHz(25)},
    {RIG_MODE_FM,kHz(100)},
    RIG_TS_END, },
    /* mode/filter list, remember: order matters! */
.filters =     {
    {RIG_MODE_SSB|RIG_MODE_PKTLSB|RIG_MODE_PKTUSB, kHz(2.4)},
    {RIG_MODE_SSB|RIG_MODE_PKTLSB|RIG_MODE_PKTUSB, kHz(1.8)},
    {RIG_MODE_SSB|RIG_MODE_PKTLSB|RIG_MODE_PKTUSB, kHz(3)},
    {RIG_MODE_FM|RIG_MODE_PKTFM, kHz(10)},
    {RIG_MODE_FM|RIG_MODE_PKTFM, kHz(15)},
    {RIG_MODE_FM|RIG_MODE_PKTFM, kHz(7)},
    {RIG_MODE_CW|RIG_MODE_CWR|RIG_MODE_RTTY|RIG_MODE_RTTYR, Hz(500)},
    {RIG_MODE_CW|RIG_MODE_CWR|RIG_MODE_RTTY|RIG_MODE_RTTYR, Hz(250)},
    {RIG_MODE_CW|RIG_MODE_CWR, kHz(1.2)},
    {RIG_MODE_RTTY|RIG_MODE_RTTYR, kHz(2.4)},
    {RIG_MODE_AM|RIG_MODE_PKTAM, kHz(6)},
    {RIG_MODE_AM|RIG_MODE_PKTAM, kHz(3)},
    {RIG_MODE_AM|RIG_MODE_PKTAM, kHz(9)},
     RIG_FLT_END, },
.str_cal = IC7100_STR_CAL,
.swr_cal = IC7100_SWR_CAL,
.alc_cal = IC7100_ALC_CAL,
.rfpower_meter_cal = IC7100_RFPOWER_METER_CAL,
.comp_meter_cal = IC7100_COMP_METER_CAL,
.vd_meter_cal = IC7100_VD_METER_CAL,
.id_meter_cal = IC7100_ID_METER_CAL,

.priv =  (void*)&ic7100_priv_caps,
.rig_init =   icom_init,
.rig_cleanup =   icom_cleanup,
.rig_open =  NULL,
.rig_close =  NULL,

.cfgparams =  icom_cfg_params,
.set_conf =  icom_set_conf,
.get_conf =  icom_get_conf,

.get_freq =  icom_get_freq,
.set_freq =  icom_set_freq,

.get_mode =  icom_get_mode_with_data,
.set_mode =  icom_set_mode_with_data,

.get_vfo =  NULL,
.set_vfo =  icom_set_vfo,
.set_ant =  icom_set_ant,
.get_ant =  icom_get_ant,
.get_ts =  icom_get_ts,
.set_ts =  icom_set_ts,
.get_func =  icom_get_func,
.set_func =  icom_set_func,
.get_level =  ic7100_get_level,
.set_level =  ic7100_set_level,

.set_ptt =  icom_set_ptt,
.get_ptt =  icom_get_ptt,

.set_rit =  icom_set_rit,

.set_rptr_shift =  icom_set_rptr_shift,
.get_rptr_shift =  icom_get_rptr_shift,
.set_rptr_offs =  icom_set_rptr_offs,
.get_rptr_offs =  icom_get_rptr_offs,
.set_ctcss_tone =  icom_set_ctcss_tone,
.get_ctcss_tone =  icom_get_ctcss_tone,
.set_ctcss_sql =  icom_set_ctcss_sql,
.get_ctcss_sql =  icom_get_ctcss_sql,
.set_dcs_sql =  icom_set_dcs_code,
.get_dcs_sql =  icom_get_dcs_code,

.set_parm =  icom_set_parm,
.get_parm =  icom_get_parm,

.set_mem =  icom_set_mem,
.vfo_op =  icom_vfo_op,
.scan =  icom_scan,
.get_dcd =  icom_get_dcd,
.decode_event =  icom_decode_event,
.set_split_vfo = icom_set_split_vfo,
.get_split_vfo =  icom_get_split_vfo,
.set_split_freq = icom_set_split_freq,
.get_split_freq = icom_get_split_freq,
.set_split_mode = icom_set_split_mode,
.get_split_mode = icom_get_split_mode,
.set_powerstat = icom_set_powerstat,
.get_powerstat = icom_get_powerstat,
.send_morse = icom_send_morse
};

int ic7100_set_level(RIG *rig, vfo_t vfo, setting_t level, value_t val)
{
  unsigned char cmdbuf[MAXFRAMELEN];

  rig_debug(RIG_DEBUG_VERBOSE, "%s called\n", __func__);

  switch (level) {
    case RIG_LEVEL_VOXDELAY:
      cmdbuf[0] = 0x01;
      cmdbuf[1] = 0x65;
      return icom_set_level_raw(rig, level, C_CTL_MEM, 0x05, 2, cmdbuf, 1, val);
    default:
      return icom_set_level(rig, vfo, level, val);
  }
}

int ic7100_get_level(RIG *rig, vfo_t vfo, setting_t level, value_t *val)
{
  unsigned char cmdbuf[MAXFRAMELEN];

  rig_debug(RIG_DEBUG_VERBOSE, "%s called\n", __func__);

  switch (level) {
    case RIG_LEVEL_VOXDELAY:
      cmdbuf[0] = 0x01;
      cmdbuf[1] = 0x65;
      return icom_get_level_raw(rig, level, C_CTL_MEM, 0x05, 2, cmdbuf, val);
    default:
      return icom_get_level(rig, vfo, level, val);
  }
}
