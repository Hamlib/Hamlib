/*
 *  Hamlib AOR backend - main file
 *  Copyright (c) 2000-2005 by Stephane Fillod
 *
 *	$Id: aor.c,v 1.33 2005-04-09 16:33:42 fillods Exp $
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

#include <stdio.h>
#include <stdlib.h>
#include <string.h>  /* String function definitions */
#include <unistd.h>  /* UNIX standard function definitions */
#include <math.h>
#include <ctype.h>

#include "hamlib/rig.h"
#include "serial.h"
#include "misc.h"
#include "register.h"
#include "idx_builtin.h"

#include "aor.h"



/*
 * acknowledge is CR
 * Is \r portable enough?
 */
#define CR '\r'
#define EOM "\r"

#define BUFSZ 256

/*
 * modes in use by the "MD" command
 */
#define MD_WFM	'0'
#define MD_NFM	'1'
#define	MD_AM	'2'
#define MD_USB	'3'
#define MD_LSB	'4'
#define MD_CW	'5'
#define MD_SFM	'6'
#define MD_WAM	'7'
#define MD_NAM	'8'


/*
 * aor_transaction
 * We assume that rig!=NULL, rig->state!= NULL, data!=NULL, data_len!=NULL
 * Otherwise, you'll get a nice seg fault. You've been warned!
 * return value: RIG_OK if everything's fine, negative value otherwise
 * TODO: error case handling
 */
int aor_transaction(RIG *rig, const char *cmd, int cmd_len, char *data, int *data_len)
{
	int retval;
	struct rig_state *rs;

	rs = &rig->state;

	serial_flush(&rs->rigport);

	retval = write_block(&rs->rigport, cmd, cmd_len);
	if (retval != RIG_OK)
		return retval;

	/* will flush data on next transaction */
	if (!data || !data_len)
		return RIG_OK;

	*data_len = read_string(&rs->rigport, data, BUFSZ, EOM, strlen(EOM));
		
	if (*data_len < BUFSZ)
		data[*data_len] = '\0';
	else
		data[BUFSZ-1] = '\0';

	return RIG_OK;
}

/*
 * aor_close
 * Assumes rig!=NULL
 */
int aor_close(RIG *rig)
{
	/* terminate remote operation via the RS-232 */

	return aor_transaction (rig, "EX" EOM, 3, NULL, NULL);
}

static int format_freq(char *buf, freq_t freq)
{
	int lowhz;
	long long f = (long long)freq;

	/*
	 * actually, frequency must be like nnnnnnnnm0, 
	 * where m must be 0 or 5 (for 50Hz).
	 */
	lowhz = f % 100;
	f /= 100;
	if (lowhz < 25)
		lowhz = 0;
	else if (lowhz < 75)
		lowhz = 50;
	else 
		lowhz = 100;
	f = f*100 + lowhz;

	return sprintf(buf,"RF%010"PRIll, f);
}

/*
 * aor_set_freq
 * Assumes rig!=NULL
 */
int aor_set_freq(RIG *rig, vfo_t vfo, freq_t freq)
{
	unsigned char freqbuf[BUFSZ];
	int freq_len;

	freq_len = format_freq(freqbuf, freq);
	strcpy(freqbuf+freq_len, EOM);
	freq_len += strlen(EOM);

	return aor_transaction (rig, freqbuf, freq_len, NULL, NULL);
}

/*
 * aor_get_freq
 * Assumes rig!=NULL, freq!=NULL
 */
int aor_get_freq(RIG *rig, vfo_t vfo, freq_t *freq)
{
	char *rfp;
	int freq_len, retval;
	unsigned char freqbuf[BUFSZ];

	retval = aor_transaction (rig, "RX" EOM, 3, freqbuf, &freq_len);
	if (retval != RIG_OK)
		return retval;

	rfp = strstr(freqbuf, "RF");
	if (!rfp) {
		rig_debug(RIG_DEBUG_WARN, "NO RF in returned string in aor_get_freq: '%s'\n",
				freqbuf);
		return -RIG_EPROTO;
	}

	sscanf(rfp+2,"%"SCNfreq, freq);

	return RIG_OK;
}

/*
 * aor_set_vfo
 * Assumes rig!=NULL
 */
int aor_set_vfo(RIG *rig, vfo_t vfo)
{
	char *vfocmd;

	switch (vfo) {
	case RIG_VFO_VFO: vfocmd = "VF" EOM; break;
	case RIG_VFO_A: vfocmd = "VA" EOM; break;
	case RIG_VFO_B: vfocmd = "VB" EOM; break;
	case RIG_VFO_C: vfocmd = "VC" EOM; break;
	case RIG_VFO_N(3): vfocmd = "VD" EOM; break;
	case RIG_VFO_N(4): vfocmd = "VE" EOM; break;
	case RIG_VFO_MEM: vfocmd = "MR" EOM; break;

	default:
		rig_debug(RIG_DEBUG_ERR,"aor_set_vfo: unsupported vfo %d\n",
						vfo);
		return -RIG_EINVAL;
	}

	return aor_transaction (rig, vfocmd, strlen(vfocmd), NULL, NULL);
}

/*
 * aor_get_vfo
 * Assumes rig!=NULL, freq!=NULL
 */
int aor_get_vfo(RIG *rig, vfo_t *vfo)
{
	int vfo_len, retval;
	unsigned char vfobuf[BUFSZ];

	retval = aor_transaction (rig, "RX" EOM, 3, vfobuf, &vfo_len);
	if (retval != RIG_OK)
		return retval;

	switch (vfobuf[1]) {
	case 'S':
	case 'V':
	case 'F': *vfo = RIG_VFO_VFO; break;
	case 'A': *vfo = RIG_VFO_A; break;
	case 'B': *vfo = RIG_VFO_B; break;
	case 'C': *vfo = RIG_VFO_C; break;
	case 'D': *vfo = RIG_VFO_N(3); break;
	case 'E': *vfo = RIG_VFO_N(4); break;
	case 'R': *vfo = RIG_VFO_MEM; break;
	default:
		rig_debug(RIG_DEBUG_ERR,"aor_get_vfo: unknown vfo %c\n",
						vfobuf[1]);
		return -RIG_EINVAL;
	}

	return RIG_OK;
}

static int format_mode(char *buf, rmode_t mode, pbwidth_t width)
{
	int aormode;

	switch (mode) {
	case RIG_MODE_AM:       
		switch(width) {
			case RIG_PASSBAND_NORMAL:
			case s_kHz(9): aormode = MD_AM; break;

			case s_kHz(12): aormode = MD_WAM; break;
			case s_kHz(3): aormode = MD_NAM; break;
			default:
				rig_debug(RIG_DEBUG_ERR,
					"%s: unsupported passband %d %d\n",
					__FUNCTION__,
					mode, width);
			return -RIG_EINVAL;
		}
		break;
	case RIG_MODE_CW:       aormode = MD_CW; break;
	case RIG_MODE_USB:      aormode = MD_USB; break;
	case RIG_MODE_LSB:      aormode = MD_LSB; break;
	case RIG_MODE_WFM:      aormode = MD_WFM; break;
	case RIG_MODE_FM:
		switch(width) {
			case RIG_PASSBAND_NORMAL:
			case s_kHz(12): aormode = MD_NFM; break;

			case s_kHz(9): aormode = MD_SFM; break;
			default:
				rig_debug(RIG_DEBUG_ERR,
					"%s: unsupported passband %d %d\n",
					__FUNCTION__,
					mode, width);
			return -RIG_EINVAL;
		}
		break;
	default:
		rig_debug(RIG_DEBUG_ERR,"%s: unsupported mode %d\n",
				__FUNCTION__, mode);
		return -RIG_EINVAL;
	}

	return sprintf(buf, "MD%c", aormode);
}

/*
 * aor_set_mode
 * Assumes rig!=NULL
 */
int aor_set_mode(RIG *rig, vfo_t vfo, rmode_t mode, pbwidth_t width)
{
	unsigned char mdbuf[BUFSZ];
	int mdbuf_len, retval;

	mdbuf_len = format_mode(mdbuf, mode, width);

	strcpy(mdbuf+mdbuf_len, EOM);
	mdbuf_len += strlen(EOM);

	retval = aor_transaction (rig, mdbuf, mdbuf_len, NULL, NULL);

	return retval;
}

static int parse_aor_mode(RIG *rig, char aormode, rmode_t *mode, pbwidth_t *width)
{
	*width = RIG_PASSBAND_NORMAL;
	switch (aormode) {
		case MD_AM:		*mode = RIG_MODE_AM; break;
		case MD_NAM:	
			*mode = RIG_MODE_AM;
			*width = rig_passband_narrow(rig, *mode); 
			break;
		case MD_WAM:	
			*mode = RIG_MODE_AM;
			*width = rig_passband_wide(rig, *mode); 
			break;
		case MD_CW:		*mode = RIG_MODE_CW; break;
		case MD_USB:	*mode = RIG_MODE_USB; break;
		case MD_LSB:	*mode = RIG_MODE_LSB; break;
		case MD_WFM:	*mode = RIG_MODE_WFM; break;
		case MD_NFM:	*mode = RIG_MODE_FM; break;
		case MD_SFM:	
			*mode = RIG_MODE_FM;
			*width = rig_passband_narrow(rig, *mode); 
			break;
		default:
			rig_debug(RIG_DEBUG_ERR,"%s: unsupported mode '%c'\n",
					__FUNCTION__, aormode);
			return -RIG_EINVAL;
	}
	if (*width == RIG_PASSBAND_NORMAL)
		*width = rig_passband_normal(rig, *mode);

	return RIG_OK;
}

/*
 * aor_get_mode
 * Assumes rig!=NULL, mode!=NULL
 */
int aor_get_mode(RIG *rig, vfo_t vfo, rmode_t *mode, pbwidth_t *width)
{
	unsigned char ackbuf[BUFSZ], *mdp;
	int ack_len, retval;


	retval = aor_transaction (rig, "MD" EOM, 3, ackbuf, &ack_len);
	if (retval != RIG_OK)
		return retval;

	/*
	 * search MD, because on the AR5000, AU is also returned 
	 * by MD request
	 */
	mdp = strstr(ackbuf, "MD");
	if (!mdp) {
		rig_debug(RIG_DEBUG_ERR, "%s: no MD in returned string: '%s'\n",
				__FUNCTION__, ackbuf);
		return -RIG_EPROTO;
	}

	retval = parse_aor_mode(rig, mdp[2], mode, width);

	return retval;
}

/*
 * aor_set_ts
 * Assumes rig!=NULL
 */
int aor_set_ts(RIG *rig, vfo_t vfo, shortfreq_t ts)
{
	unsigned char tsbuf[BUFSZ];
	int ts_len;

	/*
	 * actually, tuning step must be like nnnnm0, 
	 * where m must be 0 or 5 (for 50Hz).
	 */
	ts_len = sprintf(tsbuf,"ST%06ld" EOM, ts);

	return aor_transaction (rig, tsbuf, ts_len, NULL, NULL);
}


/*
 * aor_set_level
 * Assumes rig!=NULL, rig->state.priv!=NULL
 */
int aor_set_level(RIG *rig, vfo_t vfo, setting_t level, value_t val)
{
	struct aor_priv_data *priv;
	struct rig_state *rs;
	unsigned char lvlbuf[BUFSZ];
	int lvl_len;
	unsigned i;
	int agc;

	rs = &rig->state;
	priv = (struct aor_priv_data*)rs->priv;


	switch (level) {
	case RIG_LEVEL_ATT:
		{
		unsigned att = 0;
		for (i=0; i<MAXDBLSTSIZ && !RIG_IS_DBLST_END(rs->attenuator[i]); i++) {
			if (rs->attenuator[i] == val.i) {
				att = i+1;
				break;
			}
		}
		/* should be catched by the front end */
		if (i>=MAXDBLSTSIZ || RIG_IS_DBLST_END(rs->attenuator[i]))
			return -RIG_EINVAL;

		lvl_len = sprintf(lvlbuf, "AT%u" EOM, att);
		break;
		}
	case RIG_LEVEL_AGC:	/* AR5000 */
		switch(val.i) {
		case RIG_AGC_FAST: agc = '0'; break;
		case RIG_AGC_MEDIUM: agc = '1'; break;
		case RIG_AGC_SLOW: agc = '2'; break;
		case RIG_AGC_OFF:
		default: agc = 'F';
		}
		lvl_len = sprintf(lvlbuf,"AC%c" EOM, agc);
		break;

	default:
		rig_debug(RIG_DEBUG_ERR,"Unsupported aor_set_level %d\n", level);
		return -RIG_EINVAL;
	}

	return aor_transaction (rig, lvlbuf, lvl_len, NULL, NULL);
}

/*
 * aor_get_level
 * Assumes rig!=NULL, rig->state.priv!=NULL, val!=NULL
 */
int aor_get_level(RIG *rig, vfo_t vfo, setting_t level, value_t *val)
{
	struct aor_priv_data *priv;
	struct rig_state *rs;
	unsigned char lvlbuf[BUFSZ],ackbuf[BUFSZ];
	int lvl_len, ack_len, retval;

	rs = &rig->state;
	priv = (struct aor_priv_data*)rs->priv;

	switch (level) {
	case RIG_LEVEL_RAWSTR:
		lvl_len = sprintf(lvlbuf, "LM" EOM);
		break;
	case RIG_LEVEL_ATT:
		lvl_len = sprintf(lvlbuf, "AT" EOM);
		break;
	case RIG_LEVEL_AGC:	/* AR5000 */
		lvl_len = sprintf(lvlbuf, "AC" EOM);
		break;
	default:
		rig_debug(RIG_DEBUG_ERR,"Unsupported %s %d\n", __FUNCTION__, level);
		return -RIG_EINVAL;
	}

	retval = aor_transaction (rig, lvlbuf, lvl_len, ackbuf, &ack_len);

	if (retval != RIG_OK)
			return retval;

	switch (level) {
	case RIG_LEVEL_RAWSTR:
		if (ack_len < 4 || ackbuf[0] != 'L' || ackbuf[1] != 'M')
			return -RIG_EPROTO;
		sscanf(ackbuf+(ackbuf[2]=='%'?3:2), "%x", &val->i);
		break;

	case RIG_LEVEL_ATT:
		{
		unsigned att;
		if (ack_len < 4 || ackbuf[0] != 'A' || ackbuf[1] != 'T')
			return -RIG_EPROTO;
		att = ackbuf[3]-'0';
		if (att == 0) {
			val->i = 0;
			break;
		}
		if (att > MAXDBLSTSIZ || rs->attenuator[att-1]==0) {
			rig_debug(RIG_DEBUG_ERR,"Unsupported att %s %d\n",
							__FUNCTION__, att);
			return -RIG_EPROTO;
		}
		val->i = rs->attenuator[att-1];
		break;
		}
	case RIG_LEVEL_AGC:
		if (ack_len < 3 || ackbuf[0] != 'A' || ackbuf[1] != 'C')
			return -RIG_EPROTO;
		switch(ackbuf[3]) {
		case '0': val->i = RIG_AGC_FAST; break;
		case '1': val->i = RIG_AGC_MEDIUM; break;
		case '2': val->i = RIG_AGC_SLOW; break;
		case 'F':
		default: val->i = RIG_AGC_OFF;
		}
		break;

	default:
		rig_debug(RIG_DEBUG_ERR,"Unsupported %s %d\n", __FUNCTION__, level);
		return -RIG_EINVAL;
	}

	return RIG_OK;
}

/*
 * aor_get_dcd
 * Assumes rig!=NULL, rig->state.priv!=NULL, val!=NULL
 */
int aor_get_dcd(RIG *rig, vfo_t vfo, dcd_t *dcd)
{
	unsigned char ackbuf[BUFSZ];
	int  ack_len, retval;

	retval = aor_transaction (rig, "LM" EOM, 3, ackbuf, &ack_len);
	if (retval != RIG_OK)
		return retval;

	if (ack_len < 2 || ackbuf[0] != 'L' || ackbuf[1] != 'M')
		return -RIG_EPROTO;

	*dcd = ackbuf[2]=='%' ? RIG_DCD_OFF:RIG_DCD_ON;

	return RIG_OK;
}


/*
 * aor_set_powerstat
 * Assumes rig!=NULL, rig->state.priv!=NULL
 */
int aor_set_powerstat(RIG *rig, powerstat_t status)
{
	if (status == RIG_POWER_ON)
		return aor_transaction (rig, "X" EOM, 2, NULL, NULL);

	/* turn off power */
	return aor_transaction (rig, "QP" EOM, 3, NULL, NULL);
}

/*
 * aor_vfo_op
 * Assumes rig!=NULL
 */
int aor_vfo_op(RIG *rig, vfo_t vfo, vfo_op_t op)
{
	char *aorcmd;

	switch (op) {
	case RIG_OP_UP: aorcmd = "\x1e" EOM; break;
	case RIG_OP_DOWN: aorcmd = "\x1f" EOM; break;
	case RIG_OP_RIGHT: aorcmd = "\x1c" EOM; break;
	case RIG_OP_LEFT: aorcmd = "\x1d" EOM; break;
	case RIG_OP_MCL: aorcmd = "MQ" EOM; break;
	default:
		rig_debug(RIG_DEBUG_ERR,"aor_vfo_op: unsupported op %d\n",
						op);
		return -RIG_EINVAL;
	}

	return aor_transaction (rig, aorcmd, strlen(aorcmd), NULL, NULL);
}

/*
 * aor_set_mem
 * Assumes rig!=NULL
 */
int aor_set_mem(RIG *rig, vfo_t vfo, int ch)
{
	unsigned char membuf[BUFSZ];
	int mem_len;

	/* FIXME: bank# */
	mem_len = sprintf(membuf,"MR%c%02d" EOM, 'A'+ch/100,ch%100);

	return aor_transaction (rig, membuf, mem_len, NULL, NULL);
}

/*
 * aor_get_mem
 * Assumes rig!=NULL, freq!=NULL
 */
int aor_get_mem(RIG *rig, vfo_t vfo, int *ch)
{
	int mem_len, retval;
	unsigned char membuf[BUFSZ];

	retval = aor_transaction (rig, "MR" EOM, 3, membuf, &mem_len);
	if (retval != RIG_OK)
		return retval;

	if (membuf[0] == '?' || membuf[2] == '?')
		return -RIG_ENAVAIL;

	sscanf(membuf+3,"%d", ch);

	/* FIXME: bank# */
	*ch += 100 * (toupper(membuf[2])-'A');

	return RIG_OK;
}

/*
 * aor_set_bank
 * Assumes rig!=NULL
 */
int aor_set_bank(RIG *rig, vfo_t vfo, int bank)
{
	unsigned char membuf[BUFSZ];
	int mem_len;

	/* FIXME: bank# */
	mem_len = sprintf(membuf,"MR%c" EOM, (bank%10)+(bank<10 ? 'A':'a'));

	return aor_transaction (rig, membuf, mem_len, NULL, NULL);
}


int aor_set_channel(RIG *rig, const channel_t *chan)
{
	char aorcmd[BUFSZ];
	int cmd_len;

	cmd_len = sprintf(aorcmd, "MX%c%02d ", 
			chan->bank_num, chan->channel_num%100);

	cmd_len += format_freq(aorcmd+cmd_len, chan->freq);

	/*
	 * FIXME: automode
	 */
	cmd_len += sprintf(aorcmd+cmd_len, " AU%d ST%06d ", 
			0, (int)chan->tuning_step);

	cmd_len += format_mode(aorcmd+cmd_len, chan->mode, chan->width);

	cmd_len += sprintf(aorcmd+cmd_len, " AT%d TM%12s"EOM, 
			chan->levels[LVL_ATT].i ? 1:0, chan->channel_desc);

	return aor_transaction (rig, aorcmd, cmd_len, NULL, NULL);
}

int aor_get_channel(RIG *rig, channel_t *chan)
{
	char aorcmd[BUFSZ];
	int cmd_len, chan_len;
	char chanbuf[BUFSZ];
	int retval, i;
	char *basep, *tagp;
	channel_cap_t *mem_caps = NULL;
	chan_t *chan_list;

	/*
	 * find mem_caps in caps, we'll need it later
	 */
	chan_list = rig->caps->chan_list;
	for (i=0; i<CHANLSTSIZ && !RIG_IS_CHAN_END(chan_list[i]); i++) {
		if (chan->channel_num >= chan_list[i].start &&
				chan->channel_num <= chan_list[i].end) {
			mem_caps = &chan_list[i].mem_caps;
			break;
		}
	}
	if (!mem_caps)
		return -RIG_EINVAL;

	/* FIXME: I don't understand the MA. It lists only 10 channels,
	 * we can select bank, but not channel number. How to get 
	 * the memories past the first 10 ?
	 * Let's try with a MR first, then MA without bank.
	 */
	cmd_len = sprintf(aorcmd, "MR%c%02d" EOM, 'A' + chan->channel_num/100,
			chan->channel_num%100);
	retval = aor_transaction (rig, aorcmd, cmd_len, chanbuf, &chan_len);
	if (retval != RIG_OK)
		return retval;

	cmd_len = sprintf(aorcmd, "MA" EOM);
	retval = aor_transaction (rig, aorcmd, cmd_len, chanbuf, &chan_len);
	if (retval != RIG_OK)
		return retval;

	/* 
	 * search for attribute tags in the first line.
	 * Using strstr enable support for various models
	 * which may or may not have tag support.
	 */
	basep = chanbuf;

	/* pass */
	tagp = strstr(basep, "MP");
	if (!tagp && mem_caps->flags) {
		rig_debug(RIG_DEBUG_WARN, "%s: no MP in returned string: '%s'\n",
				__FUNCTION__, chanbuf);
		return -RIG_EPROTO;
	}
	chan->flags = tagp[2] == '0' ? 0 : RIG_CHFLAG_SKIP;

	/* frequency */
	tagp = strstr(basep, "RF");
	if (!tagp && mem_caps->freq) {
		rig_debug(RIG_DEBUG_WARN, "%s: no RF in returned string: '%s'\n",
				__FUNCTION__, chanbuf);
		return -RIG_EPROTO;
	}

	sscanf(tagp+2,"%"SCNfreq, &chan->freq);

	/* channel desc */
	tagp = strstr(basep, "ST");
	if (!tagp && mem_caps->tuning_step) {
		rig_debug(RIG_DEBUG_WARN, "%s: no ST in returned string: '%s'\n",
				__FUNCTION__, chanbuf);
		return -RIG_EPROTO;
	}
	sscanf(tagp+2,"%d", (int*)&chan->tuning_step);


	/* mode and width */
	tagp = strstr(basep, "MD");
	if (!tagp && mem_caps->mode && mem_caps->width) {
		rig_debug(RIG_DEBUG_WARN, "%s: no MD in returned string: '%s'\n",
				__FUNCTION__, chanbuf);
		return -RIG_EPROTO;
	}

	retval = parse_aor_mode(rig, tagp[2], &chan->mode, &chan->width);
	if (retval != RIG_OK)
		return retval;

	/* attenuator */
	tagp = strstr(basep, "AT");
	if (!tagp && (mem_caps->levels&LVL_ATT)) {
		rig_debug(RIG_DEBUG_WARN, "%s: no AT in returned string: '%s'\n",
				__FUNCTION__, chanbuf);
		return -RIG_EPROTO;
	}
	chan->levels[LVL_ATT].i = tagp[2] == '0' ? 0 :
		rig->caps->attenuator[tagp[2] - '0' - 1];


	/* channel desc */
	tagp = strstr(basep, "TM");
	if (!tagp && mem_caps->channel_desc) {
		rig_debug(RIG_DEBUG_WARN, "%s: no TM in returned string: '%s'\n",
				__FUNCTION__, chanbuf);
		return -RIG_EPROTO;
	}

	strncpy(chan->channel_desc, tagp+2, 12);
	chan->channel_desc[12] = '\0';


	return RIG_OK;
}


/*
 * aor_get_info
 * Assumes rig!=NULL
 */
const char *aor_get_info(RIG *rig)
{
	static char infobuf[BUFSZ];
	int id_len, frm_len, retval;
	char idbuf[BUFSZ];
	char frmbuf[BUFSZ];

	retval = aor_transaction (rig, "\001" EOM, 2, idbuf, &id_len);
	if (retval != RIG_OK)
		return NULL;

	idbuf[2] = '\0';

	retval = aor_transaction (rig, "VR" EOM, 3, frmbuf, &frm_len);
	if (retval != RIG_OK || frm_len>16)
		return NULL;

	frmbuf[frm_len] = '\0';
	sprintf(infobuf, "Remote ID %c%c, Firmware version %s", 
			idbuf[0], idbuf[1], frmbuf);

	return infobuf;
}


/*
 * initrigs_aor is called by rig_backend_load
 */
DECLARE_INITRIG_BACKEND(aor)
{
	rig_debug(RIG_DEBUG_VERBOSE, "aor: _init called\n");

	rig_register(&ar8200_caps);
	rig_register(&ar8000_caps);
	rig_register(&ar5000_caps);
	rig_register(&ar3000a_caps);
	rig_register(&ar7030_caps);
	rig_register(&ar3030_caps);

	return RIG_OK;
}

