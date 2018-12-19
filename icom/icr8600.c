/*
 *  Hamlib CI-V backend - description of IC-R75
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

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <stdlib.h>
#include <string.h>

#include "hamlib/rig.h"
#include "misc.h"
#include "idx_builtin.h"

#include "icom.h"
#include "icom_defs.h"
#include "frame.h"


/*
 * IC-R8600
 *
 * TODO:
 *  RIG_FUNC
    + what about RF (TPF)? RIG_FUNC_RF is not an audio filter, r8600 has an audio TPF. Use FUNC_RF? RIG_FUNC_ is full...
    + add a gazillion of other, new functions, but how when RIG_FUNC_ is full?
 *  RIG_LEVEL_
    + add a gazillion of other, new level settings
 *  check _PARM_ALL features, what is supported and what not.
 *  correct ICR8600_STR_CAL data. Currently its just a copy of the IC-R75 data
 *  improve ICR8600_MEM_CAP, Currently its just a copy of the IC-R75 data, but the IC-R8600 can save much more data
 *  check icr8600_caps.lvl_gran for correctness, currently just copied data from IC-R75
 *  extend icr8600_caps.parm_gran for date
 *  correct filters in caps struct
 *  check LEVEL AGC, rig responds with 1:slow, 3:mid, 5:fast, docs says 1,2,3 Also AGC is mode specific. Was this alsways the case? Do we bother?
 *  calibrate LEVEL STRENGTH
 */

/*
	Notes by DF4OR, while trying to understand hamlib
	* RIG_FUNC_ is a 32 bit word, not extensible, how to add new functions/capabilities?
	* TSQL: RIG_FUNC_TSQL is Tsql on/off function (TSQL status 0x16 0x43).
	        Actual TSQL frequency is set with icom_set_ctcss_sql() (0x1b 0x01)
	        icom_set_ctcss_tone() is for transmit ctrcss tone (not applicable here)
	        same for DCS
	* Scan resume settingsare the old style (0x0e 0xd0-0xd3) and incomplete, implement better with scan funcs

*/


#define ICR8600_MODES (RIG_MODE_LSB|RIG_MODE_USB|RIG_MODE_AM|RIG_MODE_CW|RIG_MODE_RTTY|RIG_MODE_FM|RIG_MODE_WFM|RIG_MODE_CWR|RIG_MODE_RTTYR|RIG_MODE_SAM|RIG_MODE_SAL|RIG_MODE_SAH|RIG_MODE_P25|RIG_MODE_DSTAR|RIG_MODE_DPMR|RIG_MODE_NXDNVN|RIG_MODE_NXDN_N|RIG_MODE_DCR)

#define ICR8600_FUNC_ALL (RIG_FUNC_NB|RIG_FUNC_ANF|RIG_FUNC_MN|RIG_FUNC_NR|RIG_FUNC_AIP|RIG_FUNC_LOCK|RIG_FUNC_VSC|RIG_FUNC_RESUME|RIG_FUNC_TSQL)

#define ICR8600_LEVEL_ALL (RIG_LEVEL_ATT|RIG_LEVEL_AF|RIG_LEVEL_RF|RIG_LEVEL_SQL|RIG_LEVEL_NR|RIG_LEVEL_PBT_IN|RIG_LEVEL_PBT_OUT|RIG_LEVEL_CWPITCH|RIG_LEVEL_PREAMP|RIG_LEVEL_AGC|RIG_LEVEL_RAWSTR|RIG_LEVEL_STRENGTH)

#define ICR8600_PARM_ALL (RIG_PARM_ANN|RIG_PARM_BACKLIGHT|RIG_PARM_BEEP|RIG_PARM_TIME|RIG_PARM_KEYLIGHT)

#define ICR8600_VFO_ALL (RIG_VFO_VFO|RIG_VFO_MEM)

#define ICR8600_VFO_OPS (RIG_OP_FROM_VFO|RIG_OP_TO_VFO|RIG_OP_MCL)
#define ICR8600_SCAN_OPS (RIG_SCAN_MEM|RIG_SCAN_VFO|RIG_SCAN_SLCT|RIG_SCAN_PRIO|RIG_SCAN_PRIO|RIG_SCAN_DELTA|RIG_SCAN_STOP)

#define ICR8600_ANTS_HF (RIG_ANT_1|RIG_ANT_2|RIG_ANT_3)
#define ICR8600_ANTS_VHF (RIG_ANT_1)

#define ICR8600_STR_CAL { 17, { \
		{   0, -60 }, \
		{  37, -54 }, \
		{  52, -48 }, \
		{  61, -42 }, \
		{  72, -36 }, \
		{  86, -30 }, \
		{  95, -24 }, \
		{ 109, -18 }, \
		{ 124, -12 }, \
		{ 128, -6 }, \
		{ 146,  0 }, \
		{ 166, 10 }, \
		{ 186, 20 }, \
		{ 199, 30 }, \
		{ 225, 40 }, \
		{ 233, 50 }, \
		{ 255, 60 }, \
	} }

/*
 * channel caps.
 */
#define ICR8600_MEM_CAP {	\
	.freq = 1,	\
	.mode = 1,	\
	.width = 1,	\
	.ant = 1,	\
	.levels = RIG_LEVEL_ATT|RIG_LEVEL_PREAMP,	\
	.channel_desc = 1,	\
	.flags = 1,	\
}

static int icr8600_set_channel(RIG *rig, const channel_t *chan);
static int icr8600_get_channel(RIG *rig, channel_t *chan);

static const struct icom_priv_caps icr8600_priv_caps = {
		0x96,	                  /* default address */
		0,		                  /* 731 mode */
		0,                        /* no XCHG */
		r8600_ts_sc_list,         /* list of tuning steps */
		.civ_version = 1          /* modifies behaviour in various icom.c functions */
};

const struct rig_caps icr8600_caps = {
.rig_model =  RIG_MODEL_ICR8600,
.model_name = "IC-R8600",
.mfg_name =  "Icom",
.version =  "0.1alpha",
.copyright =  "LGPL",
.status =  RIG_STATUS_ALPHA,
.rig_type =  RIG_TYPE_RECEIVER,
.ptt_type =  RIG_PTT_NONE,
.dcd_type =  RIG_DCD_RIG,
.port_type =  RIG_PORT_SERIAL,
.serial_rate_min =  300,
.serial_rate_max =  19200, // USB can do up to 115000
.serial_data_bits =  8,
.serial_stop_bits =  1,
.serial_parity =  RIG_PARITY_NONE,
.serial_handshake =  RIG_HANDSHAKE_NONE,
.write_delay =  0,
.post_write_delay =  1,
.timeout =  1000,
.retry =  3,
.has_get_func =  ICR8600_FUNC_ALL,
.has_set_func =  ICR8600_FUNC_ALL,
.has_get_level =  ICR8600_LEVEL_ALL,
.has_set_level =  RIG_LEVEL_SET(ICR8600_LEVEL_ALL),
.has_get_parm =  ICR8600_PARM_ALL,
.has_set_parm =  RIG_PARM_SET(ICR8600_PARM_ALL),
.level_gran = {
	[LVL_RAWSTR] = { .min = { .i = 0 }, .max = { .i = 255 } },
	[LVL_PBT_IN] = { .min = { .f = -1280 }, .max = { .f = +1280 }, .step = { .f = 15 } },
	[LVL_PBT_OUT] = { .min = { .f = -1280 }, .max = { .f = +1280 }, .step = { .f = 15 } },
	[LVL_CWPITCH] = { .min = { .i = 300 }, .max = { .i = 900 }, .step = { .i = 10 } },
	[LVL_NR] = { .min = { .f = 0.0 }, .max = { .f = 1.0 }, .step = { .f = 0.066666667 } },
},
.parm_gran =  {
	[PARM_TIME] = { .min = { .i = 0 }, .max = { .i = 86399} },
},
.ctcss_list =  common_ctcss_list,
.dcs_list =  common_dcs_list,
.preamp =   { 20, RIG_DBLST_END, },	/* 20 on HF, 14 on VHF, UHF, same setting */
.attenuator =   { 10, 20, 30, RIG_DBLST_END, },
.max_rit =  Hz(0),
.max_xit =  Hz(0),
.max_ifshift =  Hz(0),
.targetable_vfo =  0,
.vfo_ops =  ICR8600_VFO_OPS,
.scan_ops =  ICR8600_SCAN_OPS,
.transceive =  RIG_TRN_RIG,
.bank_qty =   100,
.chan_desc_sz =  16,

.chan_list =  {
		{   0, 99, RIG_MTYPE_MEM, ICR8600_MEM_CAP  },
		{   0, 99, RIG_MTYPE_EDGE, ICR8600_MEM_CAP  },
		/* to be extended */
		RIG_CHAN_END,
	},

.rx_range_list1 =   {
	{ kHz(10), MHz(3000), ICR8600_MODES, -1, -1, ICR8600_VFO_ALL, ICR8600_ANTS_VHF },
	{ kHz(10), MHz(30),   ICR8600_MODES, -1, -1, ICR8600_VFO_ALL, ICR8600_ANTS_HF },
	RIG_FRNG_END, },
.tx_range_list1 =   { RIG_FRNG_END, },

.rx_range_list2 =   {
	{ kHz(10), MHz(3000), ICR8600_MODES, -1, -1, ICR8600_VFO_ALL, ICR8600_ANTS_VHF },
	{ kHz(10), MHz(30),   ICR8600_MODES, -1, -1, ICR8600_VFO_ALL, ICR8600_ANTS_HF },
	RIG_FRNG_END, },
.tx_range_list2 =   { RIG_FRNG_END, },

.tuning_steps = 	{
	{ICR8600_MODES,Hz(10)},
	{ICR8600_MODES,Hz(100)},
	{ICR8600_MODES,kHz(1)},
	{ICR8600_MODES,Hz(2500)},
	{ICR8600_MODES,Hz(3125)},
	{ICR8600_MODES,kHz(5)},
	{ICR8600_MODES,kHz(6.25)},
	{ICR8600_MODES,Hz(8330)},
	{ICR8600_MODES,kHz(9)},
	{ICR8600_MODES,kHz(10)},
	{ICR8600_MODES,kHz(12.5)},
	{ICR8600_MODES,kHz(20)},
	{ICR8600_MODES,kHz(25)},
	{ICR8600_MODES,kHz(100)},
	RIG_TS_END,
	/* programmable tuning step not implemented */
	},
	/* mode/filter list, remember: order matters! */
.filters = 	{
	{RIG_MODE_SSB|RIG_MODE_CW|RIG_MODE_CWR|RIG_MODE_RTTY|RIG_MODE_RTTYR, kHz(2.4)},
	{RIG_MODE_SSB|RIG_MODE_CW|RIG_MODE_CWR|RIG_MODE_RTTY|RIG_MODE_RTTYR, kHz(1.9)},
	{RIG_MODE_SSB|RIG_MODE_CW|RIG_MODE_CWR|RIG_MODE_RTTY|RIG_MODE_RTTYR, kHz(6)},
	{RIG_MODE_AM|RIG_MODE_AMS, kHz(6)},
	{RIG_MODE_AM|RIG_MODE_AMS, kHz(2.4)},
	{RIG_MODE_AM|RIG_MODE_AMS, kHz(15)},
	{RIG_MODE_FM, kHz(15)},
	{RIG_MODE_FM, kHz(6)},
	RIG_FLT_END,
	},

.str_cal = ICR8600_STR_CAL,

.cfgparams =  icom_cfg_params,

.set_conf =  icom_set_conf,
.get_conf =  icom_get_conf,

.priv =  (void*)&icr8600_priv_caps,
.rig_init =   icom_init,
.rig_cleanup =   icom_cleanup,
.rig_open =  NULL,
.rig_close =  NULL,

.set_freq =  icom_set_freq,
.get_freq =  icom_get_freq,
.set_mode =  icom_set_mode,
.get_mode =  icom_get_mode,
.set_vfo =  icom_set_vfo,
.set_ant =  icom_set_ant,
.get_ant =  icom_get_ant,

.decode_event =  icom_decode_event,
.set_func =  icom_set_func,
.get_func =  icom_get_func,
.set_level =  icom_set_level,
.get_level =  icom_get_level,
.set_parm =  icom_set_parm,
.get_parm =  icom_get_parm,
.get_dcd =  icom_get_dcd,
.set_mem =  icom_set_mem,
.vfo_op =  icom_vfo_op,
.scan =  icom_scan,
.set_ts =  icom_set_ts,
.get_ts =  icom_get_ts,
.set_powerstat = icom_set_powerstat,
.get_powerstat = icom_get_powerstat,
.set_ctcss_sql =  icom_set_ctcss_sql,
.get_ctcss_sql =  icom_get_ctcss_sql,
.set_dcs_sql = icom_set_dcs_code,
.get_dcs_sql = icom_get_dcs_code,
.set_channel = icr8600_set_channel,
.get_channel = icr8600_get_channel,

};







/*
 * icr8600_set_channel
 * Assumes rig!=NULL, rig->state.priv!=NULL, chan!=NULL
 * Derived from IC-R75 set_channel function
 * TODO: adjust to IC-R8600
 */
int icr8600_set_channel(RIG *rig, const channel_t *chan)
{
		unsigned char chanbuf[MAXFRAMELEN], ackbuf[MAXFRAMELEN];
		int chan_len, freq_len, ack_len, retval;
		unsigned char icmode;
		signed char icmode_ext;
		int err;

		// octets 0,1: group number, bcd
		to_bcd_be(chanbuf, chan->bank_num, 4);

		// octets 2,3: channel number, bcd
		to_bcd_be(chanbuf+2, chan->channel_num, 4);

		// octet 4: skip/select
		// hi nibble: skip off, skip or pskip
		// lo nibble: 0-9 scan group
		chanbuf[4] = S_MEM_SKIP_SLCT_OFF;
		if (chan->flags & RIG_CHFLAG_SKIP)
			chanbuf[4] = S_MEM_SKIP_SLCT_ON;
		if (chan->flags & RIG_CHFLAG_PSKIP)
			chanbuf[4] = S_MEM_PSKIP_SLCT_ON;
		chanbuf[4] |= (chan->scan_group & 0x0f);


		// octets 5,6,7,8,9: rx frequency
		// IC-R8600 knows only 5 byte len frequencies, as do all Icom memory frequencies, no need to cater for varying lengths
		freq_len = 5;
		to_bcd(chanbuf+5, chan->freq, freq_len*2); // to_bcd needs nibble len

		// octets 10, 11: rx mode
		err = rig2icom_mode(rig, chan->mode, chan->width, &icmode, &icmode_ext);
		if (err != RIG_OK)
			return err;

		chanbuf[10] = icmode;
		chanbuf[11] = icmode_ext;

		// octet 12: duplex setting TODO: fixed at off, I have to better understand channel struct and what I can modify and what not.
		chanbuf[12] = S_MEM_DUP_OFF;

		// octets 13,14,15,16: offset freq
		to_bcd(chanbuf+13, chan->rptr_offs, 4*2); // to_bcd needs nibble len

		// octets 17,18: tuning steps
		if (chan->tuning_step > 0) {
			chanbuf[17] = S_MEM_TSTEP_ON;
			int i;
			for (i=0; i<TSLSTSIZ; i++) {
				if (r8600_ts_sc_list[i].ts == chan->tuning_step) {
						chanbuf[18] = r8600_ts_sc_list[i].sc;
						break;
						}
				}
			if (i >= TSLSTSIZ)
				return -RIG_EINVAL;	/* not found, unsupported */
		}
		else {
			chanbuf[17] = S_MEM_TSTEP_OFF;
			chanbuf[18] = 0; // docs unclear what is allowed here, test.
		};

		// octets 19,20: programmable tuning step, 4 dig BCD
		to_bcd_be(chanbuf+19, chan->channel_num, 4);

		// octet 21: attenuator
		to_bcd_be(chanbuf+21, chan->levels[rig_setting2idx(RIG_LEVEL_ATT)].i, 2);

		// octet 22: preamp on/off. THis is no level, but an on/off function, need to adjust. TODO
		to_bcd_be(chanbuf+22, chan->levels[rig_setting2idx(RIG_LEVEL_PREAMP)].i, 2);

		// octet 23: antenna 0,1,2
		to_bcd_be(chanbuf+23,chan->ant,2);

		// octet 24: IP+ setting, on/off
		if ((chan->funcs & RIG_FUNC_AIP) > 0) // IP+. The tern AIP is slightly misleading, but RIG_FUNC_ is full (32 bit), so I use AIP.
			chanbuf[24] = 0x01;
		else
			chanbuf[24] = 0x00;

		// octets 25-40: name
		memset(chanbuf+25, 0x20, 16); // space
		snprintf((char *) (chanbuf+25), 17, "%.16s", chan->channel_desc);

		chan_len = 41;

		retval = icom_transaction (rig, C_CTL_MEM, S_MEM_CNTNT, chanbuf, chan_len, ackbuf, &ack_len);
		if (retval != RIG_OK)
				return retval;

		if (ack_len != 1 || ackbuf[0] != ACK) {
				rig_debug(RIG_DEBUG_ERR,"icom_set_channel: ack NG (%#.2x), "
								"len=%d\n", ackbuf[0],ack_len);
				return -RIG_ERJCTED;
		}

		return RIG_OK;
}



/*
 * icr8600_get_channel
 * Assumes rig!=NULL, rig->state.priv!=NULL, chan!=NULL
 * TODO: still a WIP --SF
 */
int icr8600_get_channel(RIG *rig, channel_t *chan)
{
		struct icom_priv_data *priv;
		struct rig_state *rs;
		unsigned char chanbuf[24];
		int chan_len, freq_len, retval;

		rs = &rig->state;
		priv = (struct icom_priv_data*)rs->priv;

		to_bcd_be(chanbuf,chan->channel_num,4);
		chan_len = 2;

		freq_len = priv->civ_731_mode ? 4:5;

		retval = icom_transaction (rig, C_CTL_MEM, S_MEM_CNTNT,
						chanbuf, chan_len, chanbuf, &chan_len);
		if (retval != RIG_OK)
				return retval;

		chan->vfo = RIG_VFO_MEM;
		chan->ant = RIG_ANT_NONE;
		chan->freq = 0;
		chan->mode = RIG_MODE_NONE;
		chan->width = RIG_PASSBAND_NORMAL;
		chan->tx_freq = 0;
		chan->tx_mode = RIG_MODE_NONE;
		chan->tx_width = RIG_PASSBAND_NORMAL;
		chan->split = RIG_SPLIT_OFF;
		chan->tx_vfo = RIG_VFO_NONE;
		chan->rptr_shift = RIG_RPT_SHIFT_NONE;
		chan->rptr_offs = 0;
		chan->tuning_step = 0;
		chan->rit = 0;
		chan->xit = 0;
		chan->funcs = 0;
		chan->levels[rig_setting2idx(RIG_LEVEL_PREAMP)].i = 0;
		chan->levels[rig_setting2idx(RIG_LEVEL_ATT)].i = 0;
		chan->levels[rig_setting2idx(RIG_LEVEL_AF)].f = 0;
		chan->levels[rig_setting2idx(RIG_LEVEL_RF)].f = 0;
		chan->levels[rig_setting2idx(RIG_LEVEL_SQL)].f = 0;
		chan->levels[rig_setting2idx(RIG_LEVEL_NR)].f = 0;
		chan->levels[rig_setting2idx(RIG_LEVEL_PBT_IN)].f = 0;
		chan->levels[rig_setting2idx(RIG_LEVEL_PBT_OUT)].f = 0;
		chan->levels[rig_setting2idx(RIG_LEVEL_CWPITCH)].i = 0;
		chan->levels[rig_setting2idx(RIG_LEVEL_AGC)].i = RIG_AGC_OFF;
		chan->ctcss_tone = 0;
		chan->ctcss_sql = 0;
		chan->dcs_code = 0;
		chan->dcs_sql = 0;
		chan->scan_group = 0;
		chan->flags = RIG_CHFLAG_SKIP;
		strcpy(chan->channel_desc, "        ");

		/*
		 * freqbuf should contain Cn,Data area
		 */
		if ((chan_len != freq_len+18) && (chan_len != 5)) {
				rig_debug(RIG_DEBUG_ERR,"icr75_get_channel: wrong frame len=%d\n",
								chan_len);
				return -RIG_ERJCTED;
		}

		/* do this only if not a blank channel */
		if (chan_len != 5) {
		/*
		 * from_bcd requires nibble len
		 */
			chan->flags = RIG_CHFLAG_NONE;

			chan->freq = from_bcd(chanbuf+5, freq_len*2);

			chan_len = 4+freq_len+1;

			icom2rig_mode(rig, chanbuf[chan_len], chanbuf[chan_len+1],
							&chan->mode, &chan->width);
			chan_len += 2;
			if (from_bcd_be(chanbuf+chan_len++,2) != 0)
				chan->levels[rig_setting2idx(RIG_LEVEL_ATT)].i = 20;
			if (from_bcd_be(chanbuf+chan_len++,2) != 0)
				chan->levels[rig_setting2idx(RIG_LEVEL_PREAMP)].i = 20;
			chan->ant = from_bcd_be(chanbuf+chan_len++,2);
			strncpy(chan->channel_desc, (char *) (chanbuf+chan_len), 8);
		}

		return RIG_OK;
}
