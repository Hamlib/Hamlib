/*
 *  Hamlib Kenwood backend - TH-D7 description
 *  Copyright (c) 2000,2001 by Stephane Fillod
 *
 *		$Id: thd7.c,v 1.1 2001-10-18 20:42:25 f4cfe Exp $
 *
 *   This library is free software; you can redistribute it and/or modify
 *   it under the terms of the GNU Library General Public License as
 *   published by the Free Software Foundation; either version 2 of
 *   the License, or (at your option) any later version.
 *
 *   This program is distributed in the hope that it will be useful,
 *   but WITHOUT ANY WARRANTY; without even the implied warranty of
 *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *   GNU Library General Public License for more details.
 *
 *   You should have received a copy of the GNU Library General Public
 *   License along with this library; if not, write to the Free Software
 *   Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 *
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <stdlib.h>
#include <stdio.h>   /* Standard input/output definitions */
#include <string.h>  /* String function definitions */
#include <unistd.h>  /* UNIX standard function definitions */
#include <fcntl.h>   /* File control definitions */
#include <errno.h>   /* Error number definitions */
#include <termios.h> /* POSIX terminal control definitions */
#include <sys/ioctl.h>

#include <hamlib/rig.h>
#include <hamlib/riglist.h>
#include "kenwood.h"


int thd7_set_vfo(RIG *rig, vfo_t vfo);
int thd7_set_freq(RIG *rig, vfo_t vfo, freq_t freq);
int thd7_get_freq(RIG *rig, vfo_t vfo, freq_t *freq);
int thd7_set_mode(RIG *rig, vfo_t vfo, rmode_t mode, pbwidth_t width);
int thd7_get_mode(RIG *rig, vfo_t vfo, rmode_t *mode, pbwidth_t *width);
int thd7_set_ctcss_tone(RIG *rig, vfo_t vfo, tone_t tone);
int thd7_get_ctcss_tone(RIG *rig, vfo_t vfo, tone_t *tone);


#define THD7_MODES 	  (RIG_MODE_FM|RIG_MODE_AM)
#define THD7_MODES_TX (RIG_MODE_FM)

#define THD7_FUNC_ALL (RIG_FUNC_TSQL|RIG_FUNC_AIP|RIG_FUNC_SQL)

#define THD7_LEVEL_ALL (RIG_LEVEL_SQL|RIG_LEVEL_SQLSTAT|RIG_LEVEL_STRENGTH|RIG_LEVEL_AF|RIG_LEVEL_RF|RIG_LEVEL_MICGAIN)

#define THD7_VFO_OP (RIG_OP_UP|RIG_OP_DOWN)

/*
 * TODO: Band A & B
 */
#define THD7_VFO (RIG_VFO_A|RIG_VFO_C)

/*
 * th-d7a rig capabilities.
 */
const struct rig_caps thd7a_caps = {
rig_model: RIG_MODEL_THD7A,
model_name:"TH-D7A",
mfg_name: "Kenwood",
version: "0.1",
copyright: "LGPL",
status: RIG_STATUS_UNTESTED,
rig_type: RIG_TYPE_HANDHELD|RIG_FLAG_APRS|RIG_FLAG_TNC|RIG_FLAG_DXCLUSTER,
ptt_type: RIG_PTT_RIG,
dcd_type: RIG_DCD_RIG,
port_type: RIG_PORT_SERIAL,
serial_rate_min: 9600,
serial_rate_max: 9600,
serial_data_bits: 8,
serial_stop_bits: 1,
serial_parity: RIG_PARITY_NONE,
serial_handshake: RIG_HANDSHAKE_NONE,
write_delay: 0,
post_write_delay: 0,
timeout: 200,
retry: 3,

has_get_func: THD7_FUNC_ALL,
has_set_func: THD7_FUNC_ALL,
has_get_level: THD7_LEVEL_ALL,
has_set_level: RIG_LEVEL_SET(THD7_LEVEL_ALL),
has_get_parm: RIG_PARM_NONE,
has_set_parm: RIG_PARM_NONE,    /* FIXME: parms */
level_gran: {},                 /* FIXME: granularity */
parm_gran: {},
ctcss_list: kenwood38_ctcss_list,
dcs_list: NULL,
preamp:  { RIG_DBLST_END, },
attenuator:  { RIG_DBLST_END, },
max_rit: Hz(0),
max_xit: Hz(0),
max_ifshift: Hz(0),
vfo_ops: THD7_VFO_OP,
targetable_vfo: 0,
transceive: RIG_TRN_RIG,
bank_qty:  0,
chan_desc_sz: 0,


chan_list: { {  1,  200, RIG_MTYPE_MEM, 0 },
			 RIG_CHAN_END,
		   },	/* FIXME: memory channel list: 200 memories */

rx_range_list1: { RIG_FRNG_END, },    /* FIXME: enter region 1 setting */
tx_range_list1: { RIG_FRNG_END, },
rx_range_list2: {
    {MHz(144),MHz(148),THD7_MODES,-1,-1,THD7_VFO},
    {MHz(430),MHz(440),THD7_MODES,-1,-1,THD7_VFO},
	RIG_FRNG_END,
  }, /* rx range */
tx_range_list2: {
    {MHz(144),MHz(148),THD7_MODES_TX,W(5.5),W(5.5),THD7_VFO},
    {MHz(430),MHz(440),THD7_MODES_TX,W(5.5),W(5.5),THD7_VFO},
	RIG_FRNG_END,
  }, /* tx range */
tuning_steps: {
	 {THD7_MODES,kHz(5)},
	 {THD7_MODES,kHz(6.25)},
	 {THD7_MODES,kHz(10)},
	 {THD7_MODES,kHz(12.5)},
	 {THD7_MODES,kHz(15)},
	 {THD7_MODES,kHz(20)},
	 {THD7_MODES,kHz(25)},
	 {THD7_MODES,kHz(30)},
	 {THD7_MODES,kHz(50)},
	 {THD7_MODES,kHz(100)},
	 RIG_TS_END,
	},
        /* mode/filter list, remember: order matters! */
filters: {
		{RIG_MODE_FM, kHz(14)},
		{RIG_MODE_AM, kHz(9)},
		RIG_FLT_END,
	},
priv: NULL,

set_freq: thd7_set_freq,
get_freq: thd7_get_freq,
set_mode: thd7_set_mode,
get_mode: thd7_get_mode,
set_vfo: thd7_set_vfo,
set_ctcss_tone: thd7_set_ctcss_tone,
get_ctcss_tone: thd7_get_ctcss_tone,
set_ptt: kenwood_set_ptt,
get_dcd: kenwood_get_dcd,
vfo_op: kenwood_vfo_op,
set_mem: kenwood_set_mem,
get_mem: kenwood_get_mem,

};

#define EOM ';'

/*
 * modes in use by the "MD" command
 */
#define MD_FM	'0'
#define MD_AM	'1'

int thd7_set_vfo(RIG *rig, vfo_t vfo)
{
		unsigned char ackbuf[16];
		int ack_len, retval;

			/*
			 * FIXME: vfo==RIG_VFO_CURR
			 */

		switch (vfo) {
		case RIG_VFO_A:	/* FIXME */
		case RIG_VFO_B:
		case RIG_VFO_VFO:
			retval = kenwood_transaction (rig, "VMC0,0;", 7, ackbuf, &ack_len);
			break;
		case RIG_VFO_MEM:
			retval = kenwood_transaction (rig, "VMC0,2;", 7, ackbuf, &ack_len);
			break;
		default: 
			rig_debug(RIG_DEBUG_ERR,"thd7_set_vfo: unsupported VFO %d\n",
								vfo);
			return -RIG_EINVAL;
		}

		return retval;
}

/*
 * thd7_set_freq
 * Assumes rig!=NULL
 */
int thd7_set_freq(RIG *rig, vfo_t vfo, freq_t freq)
{
		unsigned char freqbuf[16], ackbuf[16];
		int freq_len, ack_len, retval;

		freq_len = sprintf(freqbuf,"FQ%011Ld,0;", freq);

		retval = kenwood_transaction (rig, freqbuf, freq_len, ackbuf, &ack_len);

		return retval;
}

/*
 * thd7_get_freq
 * Assumes rig!=NULL, freq!=NULL
 */
int thd7_get_freq(RIG *rig, vfo_t vfo, freq_t *freq)
{
		unsigned char freqbuf[16];
		int freq_len, retval;

		retval = kenwood_transaction (rig, "FQ;", 3, freqbuf, &freq_len);
		if (retval != RIG_OK)
			return retval;

		sscanf(freqbuf,"%Ld", freq);

		return RIG_OK;
}

/*
 * thd7_set_mode
 * Assumes rig!=NULL
 */
int thd7_set_mode(RIG *rig, vfo_t vfo, rmode_t mode, pbwidth_t width)
{
		unsigned char mdbuf[16],ackbuf[16];
		int mdbuf_len, ack_len, kmode, retval;

		switch (mode) {
			case RIG_MODE_FM:       kmode = MD_FM; break;
			case RIG_MODE_AM:       kmode = MD_AM; break;
			default:
				rig_debug(RIG_DEBUG_ERR,"thd7_set_mode: "
								"unsupported mode %d\n", mode);
				return -RIG_EINVAL;
		}

		mdbuf_len = sprintf(mdbuf, "MD%c;", kmode);
		retval = kenwood_transaction (rig, mdbuf, mdbuf_len, ackbuf, &ack_len);

		return retval;
}

/*
 * thd7_get_mode
 * Assumes rig!=NULL, mode!=NULL
 */
int thd7_get_mode(RIG *rig, vfo_t vfo, rmode_t *mode, pbwidth_t *width)
{
		unsigned char ackbuf[16];
		int ack_len, retval;

		retval = kenwood_transaction (rig, "MD;", 2, ackbuf, &ack_len);
		if (retval != RIG_OK)
			return retval;

		if (ack_len != 4) {
				rig_debug(RIG_DEBUG_ERR,"thd7_get_mode: ack NG, len=%d\n",
								ack_len);
				return -RIG_ERJCTED;
		}

		*width = RIG_PASSBAND_NORMAL;	/* FIXME */
		switch (ackbuf[0]) {
			case MD_FM:		*mode = RIG_MODE_FM; break;
			case MD_AM:		*mode = RIG_MODE_AM; break;
			default:
				rig_debug(RIG_DEBUG_ERR,"thd7_get_mode: "
								"unsupported mode %d\n", ackbuf[0]);
				return -RIG_EINVAL;
		}

		return RIG_OK;
}

/*
 * thd7_set_ctcss_tone
 * Assumes rig!=NULL, rig->caps->ctcss_list != NULL
 *
 * Warning! This is untested stuff!
 * 	Please owners report to me <f4cfe@users.sourceforge.net>, thanks. --SF
 */
int thd7_set_ctcss_tone(RIG *rig, vfo_t vfo, tone_t tone)
{
	const struct rig_caps *caps;
	unsigned char tonebuf[16], ackbuf[16];
	int tone_len, ack_len;
	int i;
								 
	caps = rig->caps;

/* TODO: replace 200 by something like RIGTONEMAX */
	for (i = 0; caps->ctcss_list[i] != 0 && i<200; i++) {
			if (caps->ctcss_list[i] == tone)
					break;
	}
	if (caps->ctcss_list[i] != tone)
			return -RIG_EINVAL;

/* TODO: replace menu no 57 by a define */
	tone_len = sprintf(tonebuf,"TN%02d;", i+1);
	
	return kenwood_transaction (rig, tonebuf, tone_len, ackbuf, &ack_len);
}

/*
 * thd7_get_ctcss_tone
 * Assumes rig!=NULL, rig->state.priv!=NULL
 */
int thd7_get_ctcss_tone(RIG *rig, vfo_t vfo, tone_t *tone)
{
	const struct rig_caps *caps;
	unsigned char tonebuf[16];
	int tone_len, i, retval;
	unsigned int tone_idx;
								 
	caps = rig->caps;

	/* TODO: replace menu no 57 by a define */
	
	retval = kenwood_transaction (rig, "TN;", 3, tonebuf, &tone_len);
	if (retval != RIG_OK)
		return retval;

	if (tone_len != 10) {
			rig_debug(RIG_DEBUG_ERR,"thd7_get_ctcss_tone: unexpected reply "
							"'%s', len=%d\n", tonebuf, tone_len);
			return -RIG_ERJCTED;
	}

	sscanf(tonebuf+3, "%u", (int*)&tone_idx);

	if (tone_idx == 0) {
			rig_debug(RIG_DEBUG_ERR,"thd7_get_ctcss_tone: Unexpected CTCSS "
							"no (%04d)\n", tone_idx);
			return -RIG_EPROTO;
	}
		
	/* check this tone exists. That's better than nothing. */
	for (i = 0; i<tone_idx; i++) {
		if (caps->ctcss_list[i] == 0) {
			rig_debug(RIG_DEBUG_ERR,"thd7_get_ctcss_tone: CTCSS NG "
					"(%04d)\n", tone_idx);
			return -RIG_EPROTO;
		}
	}
	*tone = caps->ctcss_list[tone_idx-1];

	return RIG_OK;
}

