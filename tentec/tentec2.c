/*
 *  Hamlib Tentec backend - Argonaut, Jupiter, RX-350
 *  Copyright (c) 2001-2003 by Stephane Fillod
 *
 *	$Id: tentec2.c,v 1.1 2003-05-12 22:29:59 fillods Exp $
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
#include <string.h>  /* String function definitions */
#include <unistd.h>  /* UNIX standard function definitions */
#include <math.h>

#include "hamlib/rig.h"
#include "serial.h"
#include "misc.h"
#include "cal.h"
#include "register.h"

#include "tentec.h"
#include "tentec2.h"

#define EOM "\015"	/* CR */

#define TT_AM  '0'
#define TT_USB '1'
#define TT_LSB '2'
#define TT_CW  '3'
#define TT_FM  '4'



/*************************************************************************************
 *
 * Specs from http://www.rfsquared.com
 *
 * TODO: [sg]et_split
 * 	[sg]et_level: ATT, NB, PBT, KEYER_SPD, RFPOWER, SWR, SQL, STRENGTH, ..
 * 	vfo_op: TO_VFO, FROM_VFO + emulated set_mem/get_mem
 */


/*
 * tentec_set_freq
 * assumes rig!=NULL, rig->state.priv!=NULL
 * assumes priv->mode in AM,CW,LSB or USB.
 */
int tentec2_set_freq(RIG *rig, vfo_t vfo, freq_t freq)
{
	struct rig_state *rs = &rig->state;
	int freq_len, retval;
	unsigned char freqbuf[16];
	int vfo_val;

	/*
	 * TODO: make use of rig_state.current_vfo
	 */
	if (vfo == RIG_VFO_CURR) {
		retval = tentec2_get_vfo(rig, &vfo);
		if (retval != RIG_OK)
			return retval;
	}

	switch(vfo) {
	case RIG_VFO_A: vfo_val = 'A'; break;
	case RIG_VFO_B: vfo_val = 'B'; break;
	default:
		rig_debug(RIG_DEBUG_ERR,"%s: unsupported VFO %s\n",
				__FUNCTION__, strvfo(vfo));
		return -RIG_EINVAL;
	}
	freq_len = sprintf(freqbuf, "*%c%c%c%c%c" EOM, 
						vfo_val,
						(int)(freq >> 24) & 0xff,
						(int)(freq >> 16) & 0xff,
						(int)(freq >>  8) & 0xff,
						(int)(freq        & 0xff));
	
	retval = write_block(&rs->rigport, freqbuf, freq_len);
	if (retval != RIG_OK)
		return retval;

	return RIG_OK;
}

/*
 * tentec2_get_freq
 * Assumes rig!=NULL, freq!=NULL
 */
int tentec2_get_freq(RIG *rig, vfo_t vfo, freq_t *freq)
{
	int freq_len, retval;
	unsigned char freqbuf[16];
	int vfo_val;

	/*
	 * TODO: make use of rig_state.current_vfo
	 */
	if (vfo == RIG_VFO_CURR) {
		retval = tentec2_get_vfo(rig, &vfo);
		if (retval != RIG_OK)
			return retval;
	}
	
	switch(vfo) {
	case RIG_VFO_A: vfo_val = 'A'; break;
	case RIG_VFO_B: vfo_val = 'B'; break;
	default:
		rig_debug(RIG_DEBUG_ERR,"%s: unsupported VFO %s\n",
				__FUNCTION__, strvfo(vfo));
		return -RIG_EINVAL;
	}
	freq_len = sprintf(freqbuf, "?%c" EOM, vfo_val);
	
	retval = tentec_transaction (rig, freqbuf, freq_len, freqbuf, &freq_len);
	if (retval != RIG_OK)
		return retval;

	if (freq_len != 6)
		return -RIG_EPROTO;

	*freq = (freqbuf[2] << 24) |(freqbuf[3] << 16) | (freqbuf[4] << 8) | freqbuf[5];

	return RIG_OK;
}

/*
 * tentec2_set_vfo
 * Assumes rig!=NULL
 */
int tentec2_set_vfo(RIG *rig, vfo_t vfo)
{
	struct rig_state *rs = &rig->state;
	int vfo_len, retval;
	unsigned char vfobuf[16];
	int vfo_val;

	/*
	 * TODO: make use of rig_state.current_vfo
	 */
	if ((vfo & ~RIG_VFO_MEM) == RIG_VFO_NONE || vfo == RIG_VFO_VFO) {
		vfo_t cvfo;
		retval = tentec2_get_vfo(rig, &cvfo);
		if (retval != RIG_OK)
			return retval;
		vfo = (cvfo&(RIG_VFO_A|RIG_VFO_B)) | (vfo & RIG_VFO_MEM);
	}

	switch(vfo & ~RIG_VFO_MEM) {
	case RIG_VFO_A: vfo_val = 'A'; break;
	case RIG_VFO_B: vfo_val = 'B'; break;
	default:
		rig_debug(RIG_DEBUG_ERR,"%s: unsupported VFO %s\n",
				__FUNCTION__, strvfo(vfo));
		return -RIG_EINVAL;
	}
	vfo_len = sprintf(vfobuf, "*E%c%c" EOM, 
						vfo_val,
						vfo & RIG_VFO_MEM ? 'M' : 'V'
						);
	
	retval = write_block(&rs->rigport, vfobuf, vfo_len);
	if (retval != RIG_OK)
		return retval;

	return RIG_OK;
}

/*
 * tentec2_get_vfo
 * Assumes rig!=NULL
 */
int tentec2_get_vfo(RIG *rig, vfo_t *vfo)
{
	int vfo_len, retval;
	unsigned char vfobuf[16];

	retval = tentec_transaction (rig, "?E" EOM, 3, vfobuf, &vfo_len);
	if (retval != RIG_OK)
		return retval;

	if (vfo_len != 4)
		return -RIG_EPROTO;

	*vfo = vfobuf[3] == 'A' ? RIG_VFO_A : RIG_VFO_B;
	if (vfobuf[2] == 'M')
		*vfo |= RIG_VFO_MEM;

	return RIG_OK;
}



/*
 * tentec2_set_split_vfo
 * Assumes rig!=NULL
 */
int tentec2_set_split_vfo(RIG *rig, vfo_t vfo, split_t split, vfo_t tx_vfo)
{
	struct rig_state *rs = &rig->state;

	/*
	 * TODO: handle tx_vfo
	 */
	return write_block(&rs->rigport, split==RIG_SPLIT_ON? "*O\1"EOM:"*O\0"EOM, 4);
}

/*
 * tentec2_get_split_vfo
 * Assumes rig!=NULL
 */
int tentec2_get_split_vfo(RIG *rig, vfo_t vfo, split_t *split, vfo_t *tx_vfo)
{
	int split_len, retval;
	unsigned char splitbuf[16];

	/*
	 * TODO: handle tx_vfo
	 */
	retval = tentec_transaction (rig, "?O"EOM, 3, splitbuf, &split_len);
	if (retval != RIG_OK)
		return retval;

	if (split_len != 3) {
		return -RIG_EPROTO;
	}

	*split = splitbuf[2] == '\0' ? RIG_SPLIT_OFF : RIG_SPLIT_ON;

	return RIG_OK;
}



/*
 * tentec2_set_mode
 * Assumes rig!=NULL
 */
int tentec2_set_mode(RIG *rig, vfo_t vfo, rmode_t mode, pbwidth_t width)
{
	struct rig_state *rs = &rig->state;
	unsigned char ttmode, ttmode_a, ttmode_b;
	int mdbuf_len, ttfilter, retval;
	unsigned char mdbuf[32];

	switch (mode) {
		case RIG_MODE_USB:      ttmode = TT_USB; break;
		case RIG_MODE_LSB:      ttmode = TT_LSB; break;
		case RIG_MODE_CW:       ttmode = TT_CW; break;
		case RIG_MODE_AM:       ttmode = TT_AM; break;
		case RIG_MODE_FM:       ttmode = TT_FM; break;
		default:
			rig_debug(RIG_DEBUG_ERR, "%s: unsupported mode %d\n",
					__FUNCTION__, mode);
			return -RIG_EINVAL;
	}

	if (width == RIG_PASSBAND_NORMAL)
			width = rig_passband_normal(rig, mode);

	/*
	 * Filter  0:  200
	 *              ..
	 * Filter 16: 1000
	 *              ..
	 * Filter 36: 3000
	 */
	if (width < 1000)
		ttfilter = (width / 50) - 4;
	else
		ttfilter = (width / 100) + 6;

	retval = tentec_transaction (rig, "?M"EOM, 3, mdbuf, &mdbuf_len);
	if (retval != RIG_OK)
		return retval;

	if (mdbuf_len != 4) {
		/* return -RIG_EPROTO; */
	}
	ttmode_a = mdbuf[2];
	ttmode_b = mdbuf[3];

	/*
	 * TODO: make use of rig_state.current_vfo
	 */
	if (vfo == RIG_VFO_CURR) {
		retval = tentec2_get_vfo(rig, &vfo);
		if (retval != RIG_OK)
			return retval;
	}
	
	switch(vfo) {
	case RIG_VFO_A: ttmode_a = ttmode; break;
	case RIG_VFO_B: ttmode_b = ttmode; break;
	default:
		rig_debug(RIG_DEBUG_ERR,"%s: unsupported VFO %s\n",
				__FUNCTION__, strvfo(vfo));
		return -RIG_EINVAL;
	}

	/*
	 * FIXME: the 'W' command is not VFO targetable
	 */
	mdbuf_len = sprintf(mdbuf, "*W%c" EOM "*M%c%c" EOM,
						ttfilter, 
						ttmode_a,
						ttmode_b
						);
	retval = write_block(&rs->rigport, mdbuf, mdbuf_len);
	if (retval != RIG_OK) {
		return retval;
	}

	return RIG_OK;
}



/*
 * tentec2_get_mode
 * Assumes rig!=NULL, mode!=NULL
 */
int tentec2_get_mode(RIG *rig, vfo_t vfo, rmode_t *mode, pbwidth_t *width)
{
	unsigned char ttmode;
	int mdbuf_len, ttfilter, retval;
	unsigned char mdbuf[32];

	/*
	 * TODO: make use of rig_state.current_vfo
	 */
	if (vfo == RIG_VFO_CURR) {
		retval = tentec2_get_vfo(rig, &vfo);
		if (retval != RIG_OK)
			return retval;
	}

	retval = tentec_transaction (rig, "?M"EOM, 3, mdbuf, &mdbuf_len);
	if (retval != RIG_OK)
		return retval;

	if (mdbuf_len != 4) {
		/* return -RIG_EPROTO; */
	}
	
	switch(vfo) {
	case RIG_VFO_A: ttmode = mdbuf[2]; break;
	case RIG_VFO_B: ttmode = mdbuf[3]; break;
	default:
		rig_debug(RIG_DEBUG_ERR,"%s: unsupported VFO %s\n",
				__FUNCTION__, strvfo(vfo));
		return -RIG_EINVAL;
	}
	switch (ttmode) {
		case TT_USB:	*mode = RIG_MODE_USB; break;
		case TT_LSB:	*mode = RIG_MODE_LSB; break;
		case TT_CW:	*mode = RIG_MODE_CW;  break;
		case TT_AM:	*mode = RIG_MODE_AM;  break;
		case TT_FM:	*mode = RIG_MODE_FM;  break;
		default:
			rig_debug(RIG_DEBUG_ERR, "%s: unsupported mode '%c'\n",
					__FUNCTION__, ttmode);
			return -RIG_EPROTO;
	}

	retval = tentec_transaction (rig, "?W"EOM, 3, mdbuf, &mdbuf_len);
	if (retval != RIG_OK)
		return retval;

	if (mdbuf_len != 3) {
		/* return -RIG_EPROTO; */
	}

	/*
	 * Filter  0:  200
	 *              ..
	 * Filter 16: 1000
	 *              ..
	 * Filter 36: 3000
	 */
	ttfilter = mdbuf[2];
	if (ttfilter < 16)
		*width = (ttfilter + 4) * 50;
	else
		*width = (ttfilter - 6) * 100;

	return RIG_OK;
}


/*
 * tentec2_set_ptt
 * Assumes rig!=NULL
 */
int tentec2_set_ptt(RIG *rig, vfo_t vfo, ptt_t ptt)
{
	struct rig_state *rs = &rig->state;

	return write_block(&rs->rigport, ptt==RIG_PTT_ON? "#1"EOM:"#0"EOM, 3);
}


/*
 * Software restart
 */
int tentec2_reset(RIG *rig, reset_t reset)
{
	int retval, reset_len;
	char reset_buf[32];

	retval = tentec_transaction (rig, "*X" EOM, 3, reset_buf, &reset_len);
	if (retval != RIG_OK)
		return retval;

	if (!strstr(reset_buf, "RADIO START"))
		return -RIG_EPROTO;

	return RIG_OK;
}


/*
 * tentec2_get_info
 * Assumes rig!=NULL
 */
const char *tentec2_get_info(RIG *rig)
{
	static char buf[100];	/* FIXME: reentrancy */
	int firmware_len, retval;

	/*
	 * protocol version
	 */
	firmware_len = 7;
	retval = tentec_transaction (rig, "?V" EOM, 3, buf, &firmware_len);

	/* "VER 1010-516" */
	if (retval != RIG_OK || firmware_len != 12) {
			rig_debug(RIG_DEBUG_ERR,"%s: ack NG, len=%d\n",
					__FUNCTION__, firmware_len);
			return NULL;
	}
	buf[firmware_len] = '\0';

	return buf;
}


