/*
 *  Hamlib Kenwood backend - TH handheld primitives
 *  Copyright (c) 2001 by Stephane Fillod
 *
 *		$Id: th.c,v 1.3 2002-01-02 23:41:44 fillods Exp $
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
#include "th.h"


int th_set_vfo(RIG *rig, vfo_t vfo);
int th_set_freq(RIG *rig, vfo_t vfo, freq_t freq);
int th_get_freq(RIG *rig, vfo_t vfo, freq_t *freq);
int th_set_mode(RIG *rig, vfo_t vfo, rmode_t mode, pbwidth_t width);
int th_get_mode(RIG *rig, vfo_t vfo, rmode_t *mode, pbwidth_t *width);
int th_set_ctcss_tone(RIG *rig, vfo_t vfo, tone_t tone);
int th_get_ctcss_tone(RIG *rig, vfo_t vfo, tone_t *tone);

#define EOM "\r"

/*
 * modes in use by the "MD" command
 */
#define MD_FM	'0'
#define MD_WFM	'1'		/* or is it AM on the THD7? */
#define MD_AM	'2'
#define MD_LSB	'3'
#define MD_USB	'4'
#define MD_CW	'5'

int th_set_vfo(RIG *rig, vfo_t vfo)
{
		unsigned char ackbuf[24];
		int retval, ack_len = 0;

			/*
			 * FIXME: vfo==RIG_VFO_CURR
			 */

		switch (vfo) {
		case RIG_VFO_A:	/* FIXME */
		case RIG_VFO_B:
		case RIG_VFO_VFO:
			retval = kenwood_transaction (rig, "VMC0,0" EOM, 7, ackbuf, &ack_len);
			break;
		case RIG_VFO_MEM:
			retval = kenwood_transaction (rig, "VMC0,2" EOM, 7, ackbuf, &ack_len);
			break;
		default: 
			rig_debug(RIG_DEBUG_ERR,"th_set_vfo: unsupported VFO %d\n",
								vfo);
			return -RIG_EINVAL;
		}

		return retval;
}

/*
 * th_set_freq
 * Assumes rig!=NULL
 */
int th_set_freq(RIG *rig, vfo_t vfo, freq_t freq)
{
		unsigned char freqbuf[24], ackbuf[24];
		int retval, freq_len, ack_len = 0;
		memset(ackbuf, 0, sizeof(ackbuf)); /* for alpha debugging only */

		freq_len = sprintf(freqbuf,"FQ %011Ld,0" EOM, freq);
		retval = kenwood_transaction (rig, freqbuf, freq_len, ackbuf, &ack_len);

		rig_debug(RIG_DEBUG_TRACE, "Ack msg: (%i) %s\n", ack_len, ackbuf); /* for alpha debugging only */

		/* FIXME: ack_len returns with zero even when many data is read. 
		 * 		  (something wrong with kenwook_transaction)  2001-12-30 fgr.
		 */
		if ((ackbuf[0] == 'N') && (ackbuf[1] == '\n')) {
			return -RIG_ERJCTED;
		}

		return retval;
}

/*
 * th_get_freq
 * Assumes rig!=NULL, freq!=NULL
 */
int th_get_freq(RIG *rig, vfo_t vfo, freq_t *freq)
{
		unsigned char freqbuf[24];
		int retval, freq_len = 0;
		rig_debug(RIG_DEBUG_TRACE, __FUNCTION__" called\n");

		retval = kenwood_transaction (rig, "FQ" EOM, 3, freqbuf, &freq_len);
		if (retval != RIG_OK)
			return retval;

		sscanf(freqbuf+3, "%lld", freq);

		return RIG_OK;
}

/*
 * th_set_mode
 * Assumes rig!=NULL
 */
int th_set_mode(RIG *rig, vfo_t vfo, rmode_t mode, pbwidth_t width)
{
		unsigned char mdbuf[24],ackbuf[24];
		int retval, kmode, mdbuf_len, ack_len = 0;

		switch (mode) {
			case RIG_MODE_FM:	kmode = MD_FM; break;
			case RIG_MODE_WFM:	kmode = MD_WFM; break;
			case RIG_MODE_AM:	kmode = MD_AM; break;
			case RIG_MODE_USB:	kmode = MD_USB; break;
			case RIG_MODE_LSB:	kmode = MD_LSB; break;
			case RIG_MODE_CW:	kmode = MD_CW; break;
			default:
				rig_debug(RIG_DEBUG_ERR,"th_set_mode: "
								"unsupported mode %d\n", mode);
				return -RIG_EINVAL;
		}

		mdbuf_len = sprintf(mdbuf, "MD %c" EOM, kmode);
		retval = kenwood_transaction (rig, mdbuf, mdbuf_len, ackbuf, &ack_len);

		return retval;
}

/*
 * th_get_mode
 * Assumes rig!=NULL, mode!=NULL
 */
int th_get_mode(RIG *rig, vfo_t vfo, rmode_t *mode, pbwidth_t *width)
{
		int retval, ack_len = 0;
		unsigned char ackbuf[24];

		retval = kenwood_transaction (rig, "MD" EOM, 3, ackbuf, &ack_len);
		if (retval != RIG_OK)
			return retval;

//		if (ack_len != 5) {  /* ack_len does not work. */
		if (ackbuf[0] == 'N' && ackbuf[1] == '\n') {
			rig_debug(RIG_DEBUG_ERR,"th_get_mode: ack NG, len=%d\n",ack_len);
			return -RIG_ERJCTED;
		}

		if (ackbuf[0] != 'M' || ackbuf[1] != 'D' || ackbuf[2] != ' ') {
			rig_debug(RIG_DEBUG_ERR, __FUNCTION__": unknown replay (%i) %s\n",
													ack_len, ackbuf);
			return -RIG_ERJCTED;
		}

		*width = RIG_PASSBAND_NORMAL;	/* FIXME */
		switch (ackbuf[3]) {
			case MD_FM:		*mode = RIG_MODE_FM; break;
			case MD_WFM:	*mode = RIG_MODE_WFM; break;
			case MD_AM:		*mode = RIG_MODE_AM; break;
			case MD_USB:	*mode = RIG_MODE_USB; break;
			case MD_LSB:	*mode = RIG_MODE_LSB; break;
			case MD_CW:		*mode = RIG_MODE_CW; break;
			default:
				rig_debug(RIG_DEBUG_ERR,"th_get_mode: "
								"unsupported mode '%c'\n", ackbuf[3]);
				return -RIG_EINVAL;
		}

		return RIG_OK;
}

/*
 * th_set_ctcss_tone
 * Assumes rig!=NULL, rig->caps->ctcss_list != NULL
 *
 * Warning! This is untested stuff!
 * 	Please owners report to me <f4cfe@users.sourceforge.net>, thanks. --SF
 */
int th_set_ctcss_tone(RIG *rig, vfo_t vfo, tone_t tone)
{
	const struct rig_caps *caps;
	unsigned char tonebuf[24], ackbuf[24];
	int tone_len, ack_len = 0;
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
	tone_len = sprintf(tonebuf,"TN%02d" EOM, i+1);
	
	return kenwood_transaction (rig, tonebuf, tone_len, ackbuf, &ack_len);
}

/*
 * th_get_ctcss_tone
 * Assumes rig!=NULL, rig->state.priv!=NULL
 */
int th_get_ctcss_tone(RIG *rig, vfo_t vfo, tone_t *tone)
{
	const struct rig_caps *caps;
	unsigned char tonebuf[24];
	int i, retval, tone_len = 0;
	unsigned int tone_idx;

	memset(tonebuf, 0, sizeof(tonebuf));    /* for terminating zero of string */

	caps = rig->caps;

	/* TODO: replace menu no 57 by a define */

	retval = kenwood_transaction (rig, "TN" EOM, 3, tonebuf, &tone_len);
	if (retval != RIG_OK)
		return retval;

	//if (tone_len != 10) {
	if (tonebuf[0] != 'T' || tonebuf[1] != 'N') {
			rig_debug(RIG_DEBUG_ERR,"th_get_ctcss_tone: unexpected reply "
							"'%s', len=%d\n", tonebuf, tone_len);
			return -RIG_ERJCTED;
	}

	sscanf(tonebuf+3, "%u", (int*)&tone_idx);

	if (tone_idx == 0) {
			rig_debug(RIG_DEBUG_ERR,"th_get_ctcss_tone: Unexpected CTCSS "
							"no (%04d)\n", tone_idx);
			return -RIG_EPROTO;
	}

	/* check this tone exists. That's better than nothing. */
	for (i = 0; i < tone_idx; i++) {
		if (caps->ctcss_list[i] == 0) {
			rig_debug(RIG_DEBUG_ERR,"th_get_ctcss_tone: CTCSS NG "
					"(%04d)\n", tone_idx);
			return -RIG_EPROTO;
		}
	}
	*tone = caps->ctcss_list[tone_idx-1];

	return RIG_OK;
}

