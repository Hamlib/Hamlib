/*
 *  Hamlib Kenwood backend - IC-10 interface for:
 *  			TS-940, TS-811, TS-711, TS-440, and R-5000
 *
 *  Copyright (c) 2000-2004 by Stephane Fillod and others
 *
 *	$Id: ic10.c,v 1.2 2004-05-02 17:18:23 fillods Exp $
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
#include <stdio.h>
#include <string.h>  /* String function definitions */
#include <unistd.h>  /* UNIX standard function definitions */
#include <math.h>

#include "hamlib/rig.h"
#include "serial.h"
#include "misc.h"
#include "register.h"

#include "kenwood.h"
#include "ic10.h"

/*
 * modes in use by the "MD" command
 */
#define MD_NONE	'0'
#define MD_LSB	'1'
#define MD_USB	'2'
#define MD_CW	'3'
#define MD_FM	'4'
#define MD_AM	'5'
#define MD_FSK	'6'



/*
 * ic10_set_vfo
 * Assumes rig!=NULL
 */
int ic10_set_vfo(RIG *rig, vfo_t vfo)
{
	unsigned char cmdbuf[16], ackbuf[16];
	int cmd_len, ack_len, retval;
	char vfo_function;

	switch (vfo) {
	case RIG_VFO_VFO:
	case RIG_VFO_A: vfo_function = '0'; break;
	case RIG_VFO_B: vfo_function = '1'; break;
	case RIG_VFO_MEM: vfo_function = '2'; break;
	case RIG_VFO_CURR: return RIG_OK;
	default: 
		rig_debug(RIG_DEBUG_ERR,"%s: unsupported VFO %d\n",
						__FUNCTION__, vfo);
		return -RIG_EINVAL;
	}

	cmd_len = sprintf(cmdbuf, "FN%c;", vfo_function);

	ack_len = 0;
	retval = kenwood_transaction (rig, cmdbuf, cmd_len, ackbuf, &ack_len);
	return retval;
}

/*
 * ic10_get_vfo
 * Assumes rig!=NULL, !vfo
 */
int ic10_get_vfo(RIG *rig, vfo_t *vfo)
{
	unsigned char vfobuf[50];
	int vfo_len, retval;


	/* query RX VFO */
	vfo_len = 40;
	retval = kenwood_transaction (rig, "IF;", 3, vfobuf, &vfo_len);
	if (retval != RIG_OK)
		return retval;

	if (vfo_len < 30 || vfobuf[0] != 'I' || vfobuf[1] != 'F') {
		rig_debug(RIG_DEBUG_ERR,"%s: unexpected answer %s, "
			"len=%d\n", __FUNCTION__, vfobuf, vfo_len);
		return -RIG_ERJCTED;
	}

	/* IFggmmmkkkhhh snnnzrx yytdfcp */

	switch (vfobuf[26]) {
	case '0': *vfo = RIG_VFO_A; break;
	case '1': *vfo = RIG_VFO_B; break;
	case '2': *vfo = RIG_VFO_MEM; break;
	default: 
		rig_debug(RIG_DEBUG_ERR,"%s: unsupported VFO %c\n",
					__FUNCTION__, vfobuf[26]);
		return -RIG_EPROTO;
	}
	return RIG_OK;
}

int ic10_set_split_vfo(RIG *rig, vfo_t vfo, split_t split, vfo_t txvfo)
{
	unsigned char ackbuf[16];
	int ack_len = 0;

	return kenwood_transaction (rig, split==RIG_SPLIT_ON? "SP1;":"SP0;", 4, 
					ackbuf, &ack_len);
}

int ic10_get_split_vfo(RIG *rig, vfo_t vfo, split_t *split, vfo_t *txvfo)
{
	unsigned char infobuf[50];
	int info_len, retval;

	info_len = 40;
	retval = kenwood_transaction (rig, "IF;", 3, infobuf, &info_len);
	if (retval != RIG_OK)
		return retval;

	if (/*info_len != 38 ||*/ infobuf[1] != 'F') {
		rig_debug(RIG_DEBUG_ERR,"%s: wrong answer len=%d\n",
				__FUNCTION__,info_len);
		return -RIG_ERJCTED;
	}

	/* IFggmmmkkkhhh snnnzrx yytdfcp */

	*split = infobuf[28] == '0' ? RIG_SPLIT_OFF : RIG_SPLIT_ON;

	return RIG_OK;
}



/*
 * ic10_get_mode
 * Assumes rig!=NULL, !vfo
 */
int ic10_get_mode(RIG *rig, vfo_t vfo, rmode_t *mode, pbwidth_t *width)
{
	unsigned char modebuf[50];
	int mode_len, retval;


	/* query RX VFO */
	mode_len = 40;
	retval = kenwood_transaction (rig, "IF;", 3, modebuf, &mode_len);
	if (retval != RIG_OK)
		return retval;

	if (mode_len < 30 || modebuf[0] != 'I' || modebuf[1] != 'F') {
		rig_debug(RIG_DEBUG_ERR,"%s: unexpected answer %s, "
			"len=%d\n", __FUNCTION__, modebuf, mode_len);
		return -RIG_ERJCTED;
	}

	/* IFggmmmkkkhhh snnnzrx yytdfcp */

	switch (modebuf[25]) {
	case MD_CW:	*mode = RIG_MODE_CW; break;
	case MD_USB:	*mode = RIG_MODE_USB; break;
	case MD_LSB:	*mode = RIG_MODE_LSB; break;
	case MD_FM:	*mode = RIG_MODE_FM; break;
	case MD_AM:	*mode = RIG_MODE_AM; break;
	case MD_FSK:	*mode = RIG_MODE_RTTY; break;
	case MD_NONE:	*mode = RIG_MODE_NONE; break;
	default:
		rig_debug(RIG_DEBUG_ERR,"%s: unsupported mode '%c'\n",
				__FUNCTION__,modebuf[25]);
		return -RIG_EINVAL;
	}

	*width = rig_passband_normal(rig, *mode);

	return RIG_OK;
}


/*
 * ic10_get_freq
 * Assumes rig!=NULL, freq!=NULL
 */
int ic10_get_freq(RIG *rig, vfo_t vfo, freq_t *freq)
{
	unsigned char infobuf[50];
	int info_len, retval;
	long long f;

       	if (vfo != RIG_VFO_CURR) {
		/* targeted freq retrieval */
		return kenwood_get_freq(rig, vfo, freq);
	}

	info_len = 40;
	retval = kenwood_transaction (rig, "IF;", 3, infobuf, &info_len);
	if (retval != RIG_OK)
		return retval;

	if (/*info_len != 38 || */ infobuf[1] != 'F') {
		rig_debug(RIG_DEBUG_ERR,"%s: unexpected answer %s, "
				"len=%d\n", __FUNCTION__,infobuf, info_len);
		return -RIG_ERJCTED;
	}

	/* IFggmmmkkkhhh snnnzrx yytdfcp */

	sscanf(infobuf+2, "%lld", &f);
	*freq = (freq_t)f;

	return RIG_OK;
}

/*
 * ic10_get_ptt
 * Assumes rig!=NULL, ptt!=NULL
 */
int ic10_get_ptt(RIG *rig, vfo_t vfo, ptt_t *ptt)
{
	unsigned char infobuf[50];
	int info_len, retval;

	info_len = 40;
	retval = kenwood_transaction (rig, "IF;", 3, infobuf, &info_len);
	if (retval != RIG_OK)
		return retval;

	if (/*info_len != 38 ||*/ infobuf[1] != 'F') {
		rig_debug(RIG_DEBUG_ERR,"%s: wrong answer len=%d\n",
				__FUNCTION__,info_len);
		return -RIG_ERJCTED;
	}

	/* IFggmmmkkkhhh snnnzrx yytdfcp */

	*ptt = infobuf[24] == '0' ? RIG_PTT_OFF : RIG_PTT_ON;

	return RIG_OK;
}


/*
 * ic10_get_mem
 * Assumes rig!=NULL
 */
int ic10_get_mem(RIG *rig, vfo_t vfo, int *ch)
{
	unsigned char membuf[50];
	int retval, mem_len;

	mem_len = 40;
	retval = kenwood_transaction (rig, "IF;", 3, membuf, &mem_len);
	if (retval != RIG_OK)
		return retval;

	if (mem_len < 30 || membuf[0] != 'I' || membuf[1] != 'F') {
		rig_debug(RIG_DEBUG_ERR,"%s: unexpected answer %s, "
			"len=%d\n", __FUNCTION__, membuf, mem_len);
		return -RIG_ERJCTED;
	}
	/* IFggmmmkkkhhh snnnzrx yytdfcp */
	membuf[24] = '\0';
	*ch = atoi(membuf+22);

	return RIG_OK;
}


/*
 * ic10_decode_event is called by sa_sigio, when some asynchronous
 * data has been received from the rig.
 */
int ic10_decode_event (RIG *rig)
{
	char asyncbuf[128];
	int retval,async_len=128;
	vfo_t vfo;
	long long freq;
	rmode_t mode;
	ptt_t ptt;

	rig_debug(RIG_DEBUG_TRACE, "%s: called\n", __FUNCTION__);

	retval = kenwood_transaction(rig, NULL, 0, asyncbuf, &async_len);
	if (retval != RIG_OK)
        	return retval;

	rig_debug(RIG_DEBUG_TRACE, "%s: Decoding message\n", __FUNCTION__);



    /* --------------------------------------------------------------------- */
	if (async_len<30 || asyncbuf[0] != 'I' || asyncbuf[1] != 'F') {

        	rig_debug(RIG_DEBUG_ERR, "%s: Unsupported transceive cmd '%s'\n",
			__FUNCTION__, asyncbuf);
	        return -RIG_ENIMPL;
	}

	/* IFggmmmkkkhhh snnnzrx yytdfcp */

	switch (asyncbuf[26]) {
	case '0': vfo = RIG_VFO_A; break;
	case '1': vfo = RIG_VFO_B; break;
	case '2': vfo = RIG_VFO_MEM; break;
	default: 
		rig_debug(RIG_DEBUG_ERR,"%s: unsupported VFO %c\n",
					__FUNCTION__, asyncbuf[26]);
		return -RIG_EPROTO;
	}

	sscanf(asyncbuf+2, "%lld", &freq);

	switch (asyncbuf[25]) {
	case MD_CW:	mode = RIG_MODE_CW; break;
	case MD_USB:	mode = RIG_MODE_USB; break;
	case MD_LSB:	mode = RIG_MODE_LSB; break;
	case MD_FM:	mode = RIG_MODE_FM; break;
	case MD_AM:	mode = RIG_MODE_AM; break;
	case MD_FSK:	mode = RIG_MODE_RTTY; break;
	case MD_NONE:	mode = RIG_MODE_NONE; break;
	default:
		rig_debug(RIG_DEBUG_ERR,"%s: unsupported mode '%c'\n",
				__FUNCTION__,asyncbuf[25]);
		return -RIG_EINVAL;
	}

	ptt = asyncbuf[24] == '0' ? RIG_PTT_OFF : RIG_PTT_ON;

        /* Callback execution */
        if (rig->callbacks.vfo_event) {
            rig->callbacks.vfo_event(rig, vfo, rig->callbacks.vfo_arg);
        }
        if (rig->callbacks.freq_event) {
            rig->callbacks.freq_event(rig, vfo, (freq_t)freq, rig->callbacks.freq_arg);
        }
        if (rig->callbacks.mode_event) {
            rig->callbacks.mode_event(rig, vfo, mode, RIG_PASSBAND_NORMAL,
							rig->callbacks.mode_arg);
        }
        if (rig->callbacks.ptt_event) {
            rig->callbacks.ptt_event(rig, vfo, ptt, rig->callbacks.ptt_arg);
        }

	return RIG_OK;
}



int ic10_get_channel(RIG *rig, channel_t *chan)
{
	char membuf[16],infobuf[32];
	int retval,info_len,len;
	long long freq;

	len = sprintf(membuf,"MR0 %02d;",chan->channel_num);
	info_len=30;
	retval = kenwood_transaction(rig, membuf, len, infobuf, &info_len);
	if (retval != RIG_OK)
		return retval;

	/* MRn rrggmmmkkkhhhdz    ; */
	switch (infobuf[17]) {
	case MD_CW:	chan->mode = RIG_MODE_CW; break;
	case MD_USB:	chan->mode = RIG_MODE_USB; break;
	case MD_LSB:	chan->mode = RIG_MODE_LSB; break;
	case MD_FM:	chan->mode = RIG_MODE_FM; break;
	case MD_AM:	chan->mode = RIG_MODE_AM; break;
	case MD_FSK:	chan->mode = RIG_MODE_RTTY; break;
	case MD_NONE:	chan->mode = RIG_MODE_NONE; break;
	default:
		rig_debug(RIG_DEBUG_ERR,"%s: unsupported mode '%c'\n",
				__FUNCTION__,infobuf[17]);
		return -RIG_EINVAL;
	}
	chan->width = rig_passband_normal(rig, chan->mode);

	infobuf[17] = ' ';

	sscanf(infobuf+6, "%lld", &freq);
	chan->freq = (freq_t)freq;
	chan->vfo=RIG_VFO_MEM;

	/* TX VFO (Split channel only) */
	len = sprintf(membuf,"MR1 %02d;",chan->channel_num);
	info_len=30;
	retval = kenwood_transaction(rig, membuf, len, infobuf, &info_len);
	if (retval == RIG_OK && info_len > 17) {

		/* MRn rrggmmmkkkhhhdz    ; */
		switch (infobuf[17]) {
		case MD_CW:	chan->tx_mode = RIG_MODE_CW; break;
		case MD_USB:	chan->tx_mode = RIG_MODE_USB; break;
		case MD_LSB:	chan->tx_mode = RIG_MODE_LSB; break;
		case MD_FM:	chan->tx_mode = RIG_MODE_FM; break;
		case MD_AM:	chan->tx_mode = RIG_MODE_AM; break;
		case MD_FSK:	chan->tx_mode = RIG_MODE_RTTY; break;
		case MD_NONE:	chan->tx_mode = RIG_MODE_NONE; break;
		default:
			rig_debug(RIG_DEBUG_ERR,"%s: unsupported mode '%c'\n",
					__FUNCTION__,infobuf[17]);
			return -RIG_EINVAL;
		}
		chan->tx_width = rig_passband_normal(rig, chan->tx_mode);

		infobuf[17] = ' ';

		sscanf(infobuf+6, "%lld", &freq);
		chan->tx_freq = (freq_t)freq;
	}

	return RIG_OK;
}


int ic10_set_channel(RIG *rig, const channel_t *chan)
{
	char membuf[32],ackbuf[32];
	int retval,ack_len,len,md;
	long long freq;

	freq = chan->freq;
	switch (chan->mode) {
	case RIG_MODE_CW:	md = MD_CW; break;
	case RIG_MODE_USB:	md = MD_USB; break;
	case RIG_MODE_LSB:	md = MD_LSB; break;
	case RIG_MODE_FM:	md = MD_FM; break;
	case RIG_MODE_AM:	md = MD_AM; break;
	case RIG_MODE_RTTY:	md = MD_FSK; break;
	case RIG_MODE_NONE:	md = MD_NONE; break;
	default:
		rig_debug(RIG_DEBUG_ERR,"%s: unsupported mode %d\n",
				__FUNCTION__,chan->mode);
		return -RIG_EINVAL;
	}

	/* MWnxrrggmmmkkkhhhdzxxxx; */
	len = sprintf(membuf,"MW0 %02d%011lld%c0    ;",
			chan->channel_num,
			freq,
			md
			);
	ack_len=30;
	retval = kenwood_transaction(rig, membuf, len, ackbuf, &ack_len);
	if (retval != RIG_OK)
		return retval;

	/* TX VFO (Split channel only) */
	freq = chan->tx_freq;
	switch (chan->tx_mode) {
	case RIG_MODE_CW:	md = MD_CW; break;
	case RIG_MODE_USB:	md = MD_USB; break;
	case RIG_MODE_LSB:	md = MD_LSB; break;
	case RIG_MODE_FM:	md = MD_FM; break;
	case RIG_MODE_AM:	md = MD_AM; break;
	case RIG_MODE_RTTY:	md = MD_FSK; break;
	case RIG_MODE_NONE:	md = MD_NONE; break;
	default:
		rig_debug(RIG_DEBUG_ERR,"%s: unsupported mode %d\n",
				__FUNCTION__,chan->tx_mode);
		return -RIG_EINVAL;
	}

	/* MWnxrrggmmmkkkhhhdzxxxx; */
	len = sprintf(membuf,"MW1 %02d%011lld%c0    ;",
			chan->channel_num,
			freq,
			md
			);
	ack_len=30;
	retval = kenwood_transaction(rig, membuf, len, ackbuf, &ack_len);

	return RIG_OK;
}


