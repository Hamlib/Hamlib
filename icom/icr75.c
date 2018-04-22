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
 * IC-R75
 *
 * TODO:
 *  $1A command:
 *  - set_parm, set_trn, IF filter setting, etc.
 */

#define ICR75_MODES (RIG_MODE_AM|RIG_MODE_CW|RIG_MODE_CWR|RIG_MODE_SSB|RIG_MODE_FM|RIG_MODE_RTTY|RIG_MODE_RTTYR|RIG_MODE_AMS)

#define ICR75_FUNC_ALL (RIG_FUNC_NB|RIG_FUNC_ANF|RIG_FUNC_NR)

#define ICR75_LEVEL_ALL (RIG_LEVEL_ATT|RIG_LEVEL_AF|RIG_LEVEL_RF|RIG_LEVEL_SQL|RIG_LEVEL_NR|RIG_LEVEL_PBT_IN|RIG_LEVEL_PBT_OUT|RIG_LEVEL_CWPITCH|RIG_LEVEL_PREAMP|RIG_LEVEL_AGC|RIG_LEVEL_RAWSTR|RIG_LEVEL_STRENGTH)

#define ICR75_PARM_ALL (RIG_PARM_ANN|RIG_PARM_APO|RIG_PARM_BACKLIGHT|RIG_PARM_BEEP|RIG_PARM_TIME)

#define ICR75_VFO_ALL (RIG_VFO_VFO|RIG_VFO_MEM)

#define ICR75_VFO_OPS (RIG_OP_FROM_VFO|RIG_OP_TO_VFO|RIG_OP_MCL)
#define ICR75_SCAN_OPS (RIG_SCAN_MEM|RIG_SCAN_VFO)

#define ICR75_ANTS (RIG_ANT_1|RIG_ANT_2)

#define ICR75_STR_CAL { 17, { \
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
#define ICR75_MEM_CAP {	\
	.freq = 1,	\
	.mode = 1,	\
	.width = 1,	\
	.ant = 1,	\
	.levels = RIG_LEVEL_ATT|RIG_LEVEL_PREAMP,	\
	.channel_desc = 1,	\
	.flags = 1,	\
}

static int icr75_set_channel(RIG *rig, const channel_t *chan);
static int icr75_get_channel(RIG *rig, channel_t *chan);

static const struct icom_priv_caps icr75_priv_caps = {
		0x5a,	/* default address */
		0,		/* 731 mode */
    0,    /* no XCHG */
		r75_ts_sc_list
};

const struct rig_caps icr75_caps = {
.rig_model =  RIG_MODEL_ICR75,
.model_name = "IC-R75",
.mfg_name =  "Icom",
.version =  BACKEND_VER,
.copyright =  "LGPL",
.status =  RIG_STATUS_BETA,
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
.post_write_delay =  1,
.timeout =  1000,
.retry =  3,
.has_get_func =  ICR75_FUNC_ALL,
.has_set_func =  ICR75_FUNC_ALL,
.has_get_level =  ICR75_LEVEL_ALL,
.has_set_level =  RIG_LEVEL_SET(ICR75_LEVEL_ALL),
.has_get_parm =  ICR75_PARM_ALL,
.has_set_parm =  RIG_PARM_SET(ICR75_PARM_ALL),
.level_gran = {
	[LVL_RAWSTR] = { .min = { .i = 0 }, .max = { .i = 255 } },
	[LVL_PBT_IN] = { .min = { .f = -1280 }, .max = { .f = +1280 }, .step = { .f = 15 } },
	[LVL_PBT_OUT] = { .min = { .f = -1280 }, .max = { .f = +1280 }, .step = { .f = 15 } },
	[LVL_CWPITCH] = { .min = { .i = 300 }, .max = { .i = 900 }, .step = { .i = 10 } },
	[LVL_NR] = { .min = { .f = 0.0 }, .max = { .f = 1.0 }, .step = { .f = 0.066666667 } },
},
.parm_gran =  {
	[PARM_APO] = { .min = { .i = 1 }, .max = { .i = 1439} },
	[PARM_TIME] = { .min = { .i = 0 }, .max = { .i = 86399} },
},
.ctcss_list =  NULL,
.dcs_list =  NULL,
.preamp =   { 10, 20, RIG_DBLST_END, },	/* TBC */
.attenuator =   { 20, RIG_DBLST_END, },	/* TBC */
.max_rit =  Hz(0),
.max_xit =  Hz(0),
.max_ifshift =  Hz(0),
.targetable_vfo =  0,
.vfo_ops =  ICR75_VFO_OPS,
.scan_ops =  ICR75_SCAN_OPS,
.transceive =  RIG_TRN_RIG,
.bank_qty =   0,
.chan_desc_sz =  8,

.chan_list =  {
		{   1,  99, RIG_MTYPE_MEM, ICR75_MEM_CAP  },
		{ 100, 101, RIG_MTYPE_EDGE, ICR75_MEM_CAP  },
		RIG_CHAN_END,
	},

.rx_range_list1 =   {
	{kHz(30),MHz(60),ICR75_MODES,-1,-1,ICR75_VFO_ALL,ICR75_ANTS},
	RIG_FRNG_END, },
.tx_range_list1 =   { RIG_FRNG_END, },

.rx_range_list2 =   {
	{kHz(30),MHz(60),ICR75_MODES,-1,-1,ICR75_VFO_ALL,ICR75_ANTS},
	RIG_FRNG_END, },
.tx_range_list2 =   { RIG_FRNG_END, },

.tuning_steps = 	{
	 {ICR75_MODES,Hz(10)},
	 {ICR75_MODES,Hz(100)},
	 {ICR75_MODES,kHz(1)},
	 {ICR75_MODES,kHz(5)},
	 {ICR75_MODES,kHz(6.25)},
	 {ICR75_MODES,kHz(9)},
	 {ICR75_MODES,kHz(10)},
	 {ICR75_MODES,kHz(12.5)},
	 {ICR75_MODES,kHz(20)},
	 {ICR75_MODES,kHz(25)},
	 {ICR75_MODES,kHz(100)},
	 {ICR75_MODES,MHz(1)},
	 RIG_TS_END,
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
.str_cal = ICR75_STR_CAL,

.cfgparams =  icom_cfg_params,
.set_conf =  icom_set_conf,
.get_conf =  icom_get_conf,

.priv =  (void*)&icr75_priv_caps,
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
.set_powerstat = icom_set_powerstat,
.get_powerstat = icom_get_powerstat,

.set_channel = icr75_set_channel,
.get_channel = icr75_get_channel,

};


/*
 * icr75_set_channel
 * Assumes rig!=NULL, rig->state.priv!=NULL, chan!=NULL
 * TODO: still a WIP --SF
 */
int icr75_set_channel(RIG *rig, const channel_t *chan)
{
		struct icom_priv_data *priv;
		struct rig_state *rs;
		unsigned char chanbuf[MAXFRAMELEN], ackbuf[MAXFRAMELEN];
		int chan_len, freq_len, ack_len, retval;
		unsigned char icmode;
		signed char icmode_ext;
		int err;

		rs = &rig->state;
		priv = (struct icom_priv_data*)rs->priv;

		to_bcd_be(chanbuf,chan->channel_num,4);

		chanbuf[2] = S_MEM_CNTNT_SLCT;

		freq_len = priv->civ_731_mode ? 4:5;
		/*
		 * to_bcd requires nibble len
		 */
		to_bcd(chanbuf+3, chan->freq, freq_len*2);

		chan_len = 2+freq_len+1;

		err = rig2icom_mode(rig, chan->mode, chan->width,
						&icmode, &icmode_ext);
		if (err != RIG_OK)
				return err;

		chanbuf[chan_len++] = icmode;
		chanbuf[chan_len++] = icmode_ext;

		to_bcd_be(chanbuf+chan_len++,
					chan->levels[rig_setting2idx(RIG_LEVEL_ATT)].i, 2);
		to_bcd_be(chanbuf+chan_len++,
					chan->levels[rig_setting2idx(RIG_LEVEL_PREAMP)].i, 2);

		to_bcd_be(chanbuf+chan_len++,chan->ant,2);
		memset(chanbuf+chan_len, 0, 8);
		snprintf((char *) (chanbuf+chan_len), 9, "%.8s", chan->channel_desc);
		chan_len += 8;

		retval = icom_transaction (rig, C_CTL_MEM, S_MEM_CNTNT,
						chanbuf, chan_len, ackbuf, &ack_len);
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
 * icr75_get_channel
 * Assumes rig!=NULL, rig->state.priv!=NULL, chan!=NULL
 * TODO: still a WIP --SF
 */
int icr75_get_channel(RIG *rig, channel_t *chan)
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

