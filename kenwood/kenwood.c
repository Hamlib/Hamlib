/*
 *  Hamlib Kenwood backend - main file
 *  Copyright (c) 2000,2001 by Stephane Fillod
 *
 *		$Id: kenwood.c,v 1.16 2001-10-18 20:43:13 f4cfe Exp $
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
#include <math.h>

#if defined(__CYGWIN__)
#  undef HAMLIB_DLL
#  include <hamlib/rig.h>
#  include <serial.h>
#  include <misc.h>
#  define HAMLIB_DLL
#  include <hamlib/rig_dll.h>
#else
#  include <hamlib/rig.h>
#  include <serial.h>
#  include <misc.h>
#endif

#include "kenwood.h"


#define EOM ';'

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
#define MD_CWR	'7'
#define MD_FSKR	'9'

struct kenwood_id {
	rig_model_t model;
	int id;
};

struct kenwood_id_string {
	rig_model_t model;
	const char *id;
};


#define UNKNOWN_ID -1

/*
 * Identification number as returned by "ID;"
 * Please, if the model number of your rig is listed as UNKNOWN_ID,
 * send the value to <f4cfe@users.sourceforge.net> for inclusion. Thanks --SF
 *
 * TODO: sort this list with most frequent rigs first.
 */
static const struct kenwood_id kenwood_id_list[] = {
	{ RIG_MODEL_R5000, 5 },
	{ RIG_MODEL_TS870S, 15 },
	{ RIG_MODEL_TS570D, 17 },
	{ RIG_MODEL_TS570S, 18 },
	{ RIG_MODEL_TS2000, 19 },
	{ RIG_MODEL_NONE, UNKNOWN_ID },	/* end marker */
};

static const struct kenwood_id_string kenwood_id_string_list[] = {
	{ RIG_MODEL_THD7A,  "TH-D7" },
	{ RIG_MODEL_THD7AG, "TH-D7G" },
	{ RIG_MODEL_NONE, NULL },	/* end marker */
};


/*
 * 38 CTCSS sub-audible tones
  */
const int kenwood38_ctcss_list[] = {
	 670,  719,  744,  770,  797,  825,  854,  885,  915,  948,
	 974, 1000, 1035, 1072, 1109, 1148, 1188, 1230, 1273, 1318,
	1365, 1413, 1462, 1514, 1567, 1622, 1679, 1738, 1799, 1862,
	1928, 2035, 2107, 2181, 2257, 2336, 2418, 2503, 17500,
	0,
};

/*
 * port_transaction
 * called by kenwood_transaction and probe_kenwood
 * We assume that port!=NULL, data!=NULL, data_len!=NULL
 * Otherwise, you'll get a nice seg fault. You've been warned!
 * return value: RIG_OK if everything's fine, negative value otherwise
 * TODO: error case handling, error codes from the rig, ..
 */
static int port_transaction(port_t *port, const char *cmd, int cmd_len, char *data, int *data_len)
{
	int i, retval;

	retval = write_block(port, cmd, cmd_len);
	if (retval != RIG_OK)
		return retval;

	/*
	 * buffered read are quite helpful here!
	 * However, an automate with a state model would be more efficient..
	 */
	i = 0;
	do {
		retval = fread_block(port, data+i, 1);
		if (retval == 0)
				continue;	/* huh!? */
		if (retval < 0)
				return retval;
	} while (data[i++] != EOM);

	*data_len = i;

	return RIG_OK;
}

/*
 * kenwood_transaction
 * We assume that rig!=NULL, rig->state!= NULL, data!=NULL, data_len!=NULL
 * Otherwise, you'll get a nice seg fault. You've been warned!
 * return value: RIG_OK if everything's fine, negative value otherwise
 * TODO: error case handling, error codes from the rig, ..
 *
 * Note: right now, kenwood_transaction is a simple call to port_transaction.
 * kenwood_transaction could have been written to accept a port_t instead
 * of RIG, but who knows if we will need some priv stuff in the future?
 */
int kenwood_transaction(RIG *rig, const char *cmd, int cmd_len, char *data, int *data_len)
{
	return port_transaction(&rig->state.rigport, cmd, cmd_len, data, data_len);
}

/*
 * kenwood_set_vfo
 * Assumes rig!=NULL
 */
int kenwood_set_vfo(RIG *rig, vfo_t vfo)
{
		unsigned char cmdbuf[16], ackbuf[16];
		int ack_len, retval;
		char vfo_function;

			/*
			 * FIXME: vfo==RIG_VFO_CURR
			 */

		switch (vfo) {
		case RIG_VFO_VFO:
		case RIG_VFO_A: vfo_function = '0'; break;
		case RIG_VFO_B: vfo_function = '1'; break;
		case RIG_VFO_MEM: vfo_function = '2'; break;
		/* TODO : case RIG_VFO_C: */ 
		default: 
			rig_debug(RIG_DEBUG_ERR,"kenwood_set_vfo: unsupported VFO %d\n",
								vfo);
			return -RIG_EINVAL;
		}
		cmdbuf[0] = 'F';
		cmdbuf[1] = 'R';
		cmdbuf[2] = vfo_function;
		cmdbuf[3] = EOM;

		/* set RX VFO */
		retval = kenwood_transaction (rig, cmdbuf, 4, ackbuf, &ack_len);
		if (retval != RIG_OK)
			return retval;

		/* set TX VFO */
		cmdbuf[1] = 'T';
		retval = kenwood_transaction (rig, cmdbuf, 4, ackbuf, &ack_len);

		return retval;
}

/*
 * kenwood_get_vfo
 * Assumes rig!=NULL, !vfo
 */
int kenwood_get_vfo(RIG *rig, vfo_t *vfo)
{
		unsigned char vfobuf[16];
		int vfo_len, retval;


		/* query RX VFO */
		retval = kenwood_transaction (rig, "FR;", 3, vfobuf, &vfo_len);
		if (retval != RIG_OK)
			return retval;

		if (vfo_len != 4 || vfobuf[1] != 'R') {
			rig_debug(RIG_DEBUG_ERR,"kenwood_get_vfo: unexpected answer %s, "
							"len=%d\n", vfobuf, vfo_len);
			return -RIG_ERJCTED;
		}

		/* TODO: replace 0,1,2,.. constants by defines */
		switch (vfobuf[2]) {
		case '0': *vfo = RIG_VFO_A; break;
		case '1': *vfo = RIG_VFO_B; break;
		case '2': *vfo = RIG_VFO_MEM; break;
		default: 
			rig_debug(RIG_DEBUG_ERR,"kenwood_get_vfo: unsupported VFO %c\n",
								vfobuf[2]);
			return -RIG_EPROTO;
		}
		return RIG_OK;
}

/*
 * kenwood_set_freq
 * Assumes rig!=NULL
 */
int kenwood_set_freq(RIG *rig, vfo_t vfo, freq_t freq)
{
		unsigned char freqbuf[16], ackbuf[16];
		int freq_len, ack_len, retval;
		char vfo_letter;

			/*
			 * better FIXME: vfo==RIG_VFO_CURR
			 */
		if (vfo == RIG_VFO_CURR) {
			retval = kenwood_get_vfo(rig, &vfo);
			if (retval != RIG_OK)
					return retval;
		}

		switch (vfo) {
		case RIG_VFO_A: vfo_letter = 'A'; break;
		case RIG_VFO_B: vfo_letter = 'B'; break;
		case RIG_VFO_C: vfo_letter = 'C'; break;
		default: 
			rig_debug(RIG_DEBUG_ERR,"kenwood_set_freq: unsupported VFO %d\n",
								vfo);
			return -RIG_EINVAL;
		}
		freq_len = sprintf(freqbuf,"F%c%011Ld;", vfo_letter, freq);

		retval = kenwood_transaction (rig, freqbuf, freq_len, ackbuf, &ack_len);

		return retval;
}

/*
 * kenwood_get_freq
 * Assumes rig!=NULL, freq!=NULL
 */
int kenwood_get_freq(RIG *rig, vfo_t vfo, freq_t *freq)
{
		unsigned char freqbuf[16];
		unsigned char cmdbuf[4];
		int freq_len, retval;
		char vfo_letter;

			/*
			 * better FIXME: vfo==RIG_VFO_CURR
			 */
		if (vfo == RIG_VFO_CURR) {
			retval = kenwood_get_vfo(rig, &vfo);
			if (retval != RIG_OK)
				return retval;
		}

		switch (vfo) {
		case RIG_VFO_A: vfo_letter = 'A'; break;
		case RIG_VFO_B: vfo_letter = 'B'; break;
		case RIG_VFO_C: vfo_letter = 'C'; break;
		default: 
			rig_debug(RIG_DEBUG_ERR,"kenwood_set_freq: unsupported VFO %d\n",
								vfo);
			return -RIG_EINVAL;
		}
		cmdbuf[0] = 'F';
		cmdbuf[1] = vfo_letter;
		cmdbuf[2] = EOM;

		retval = kenwood_transaction (rig, cmdbuf, 3, freqbuf, &freq_len);
		if (retval != RIG_OK)
			return retval;

		sscanf(freqbuf,"%Ld", freq);

		return RIG_OK;
}

/*
 * kenwood_set_mode
 * Assumes rig!=NULL
 */
int kenwood_set_mode(RIG *rig, vfo_t vfo, rmode_t mode, pbwidth_t width)
{
		unsigned char mdbuf[16],ackbuf[16];
		int mdbuf_len, ack_len, kmode, retval;

		switch (mode) {
			case RIG_MODE_CW:       kmode = MD_CW; break;
			case RIG_MODE_USB:      kmode = MD_USB; break;
			case RIG_MODE_LSB:      kmode = MD_LSB; break;
			case RIG_MODE_FM:       kmode = MD_FM; break;
			case RIG_MODE_AM:       kmode = MD_AM; break;
			case RIG_MODE_RTTY:     kmode = MD_FSK; break;
			default:
				rig_debug(RIG_DEBUG_ERR,"kenwood_set_mode: "
								"unsupported mode %d\n", mode);
				return -RIG_EINVAL;
		}

		mdbuf_len = sprintf(mdbuf, "MD%c;", kmode);
		retval = kenwood_transaction (rig, mdbuf, mdbuf_len, ackbuf, &ack_len);

		return retval;
}

/*
 * kenwood_get_mode
 * Assumes rig!=NULL, mode!=NULL
 */
int kenwood_get_mode(RIG *rig, vfo_t vfo, rmode_t *mode, pbwidth_t *width)
{
		unsigned char ackbuf[16];
		int ack_len, retval;


		retval = kenwood_transaction (rig, "MD;", 2, ackbuf, &ack_len);
		if (retval != RIG_OK)
			return retval;

		if (ack_len != 4) {
				rig_debug(RIG_DEBUG_ERR,"kenwood_get_mode: ack NG, len=%d\n",
								ack_len);
				return -RIG_ERJCTED;
		}

		*width = RIG_PASSBAND_NORMAL;	/* FIXME */
		switch (ackbuf[0]) {
			case MD_CW:		*mode = RIG_MODE_CW; break;
			case MD_USB:	*mode = RIG_MODE_USB; break;
			case MD_LSB:	*mode = RIG_MODE_LSB; break;
			case MD_FM:		*mode = RIG_MODE_FM; break;
			case MD_AM:		*mode = RIG_MODE_AM; break;
			case MD_FSK:		*mode = RIG_MODE_RTTY; break;
#ifdef RIG_MODE_CWR
			case MD_CWR:		*mode = RIG_MODE_CWR; break;
#endif
#ifdef RIG_MODE_RTTYR
			case MD_FSKR:		*mode = RIG_MODE_RTTY; break;
#endif
			case MD_NONE:		*mode = RIG_MODE_NONE; break;
			default:
				rig_debug(RIG_DEBUG_ERR,"kenwood_get_mode: "
								"unsupported mode %d\n", ackbuf[0]);
				return -RIG_EINVAL;
		}

		return RIG_OK;
}

/* 
 * assumes f!=NULL
 */
static int get_kenwood_level(RIG *rig, const char *cmd, int cmd_len, float *f)
{
	unsigned char lvlbuf[16];
	int lvl_len, retval;
	int lvl;

	retval = kenwood_transaction (rig, cmd, cmd_len, lvlbuf, &lvl_len);
	if (retval != RIG_OK)
		return retval;

	if (lvl_len != 4) {
		rig_debug(RIG_DEBUG_ERR,"kenwood_get_level: wrong answer len=%d\n",
				lvl_len);
		return -RIG_ERJCTED;
	}

	/*
	 * 000..255
	 */
	sscanf(lvlbuf+2, "%d", &lvl);
	*f = (float)lvl/255.0;

	return RIG_OK;
};


/*
 * kenwood_get_level
 * Assumes rig!=NULL, val!=NULL
 */
int kenwood_get_level(RIG *rig, vfo_t vfo, setting_t level, value_t *val)
{
		unsigned char lvlbuf[16];
		int lvl_len, retval;
		int lvl;
		int i;

		/* Optimize:
		 *   sort the switch cases with the most frequent first
		 */
		switch (level) {
		case RIG_LEVEL_STRENGTH:
			retval = kenwood_transaction (rig, "SM;", 3, lvlbuf, &lvl_len);
			if (retval != RIG_OK)
				return retval;

			if (lvl_len != 4) {
				rig_debug(RIG_DEBUG_ERR,"kenwood_get_level: "
								"wrong answer len=%d\n", lvl_len);
				return -RIG_ERJCTED;
			}

			/* FIXME: should be in dB ! */
			sscanf(lvlbuf+2, "%d", &val->i);
			break;

		case RIG_LEVEL_SQLSTAT:
			return -RIG_ENIMPL;	/* get_dcd ? */

		case RIG_LEVEL_PREAMP:
			return -RIG_ENIMPL;

		case RIG_LEVEL_ATT:
			retval = kenwood_transaction (rig, "RA;", 3, lvlbuf, &lvl_len);
			if (retval != RIG_OK)
				return retval;

			if (lvl_len != 5) {
				rig_debug(RIG_DEBUG_ERR,"kenwood_get_level: "
								"unexpected answer len=%d\n", lvl_len);
				return -RIG_ERJCTED;
			}

			sscanf(lvlbuf+2, "%d", &lvl);
			if (lvl == 0) {
					val->i = 0;
			} else {
					for (i=0; i<lvl && i<MAXDBLSTSIZ; i++)
							if (rig->state.attenuator[i] == 0) {
								rig_debug(RIG_DEBUG_ERR,"kenwood_get_level: "
											"unexpected att level %d\n", lvl);
									return -RIG_EPROTO;
									}
					if (i != lvl)
							return -RIG_EINTERNAL;
					val->i = rig->state.attenuator[i-1];
			}
			break;
		case RIG_LEVEL_AF:
			return get_kenwood_level(rig, "AG;", 3, &val->f);

		case RIG_LEVEL_RF:
			return get_kenwood_level(rig, "RG;", 3, &val->f);

		case RIG_LEVEL_SQL:
			return get_kenwood_level(rig, "SQ;", 3, &val->f);

		case RIG_LEVEL_MICGAIN:
			return get_kenwood_level(rig, "MG;", 3, &val->f);

		case RIG_LEVEL_IF:
		case RIG_LEVEL_APF:
		case RIG_LEVEL_NR:
		case RIG_LEVEL_PBT_IN:
		case RIG_LEVEL_PBT_OUT:
		case RIG_LEVEL_CWPITCH:
		case RIG_LEVEL_KEYSPD:
		case RIG_LEVEL_NOTCHF:
		case RIG_LEVEL_COMP:
		case RIG_LEVEL_AGC:
		case RIG_LEVEL_BKINDL:
		case RIG_LEVEL_BALANCE:
			return -RIG_ENIMPL;

		default:
			rig_debug(RIG_DEBUG_ERR,"Unsupported get_level %d", level);
			return -RIG_EINVAL;
		}

		return RIG_OK;
}

/* 
 * assumes status!=NULL
 * works for any 'format 1' command
 */
static int get_kenwood_func(RIG *rig, const char *cmd, int cmd_len, int *status)
{
	unsigned char fctbuf[16];
	int fct_len, retval;

	retval = kenwood_transaction (rig, cmd, cmd_len, fctbuf, &fct_len);
	if (retval != RIG_OK)
		return retval;

	if (fct_len != 4) {
		rig_debug(RIG_DEBUG_ERR,"kenwood_get_func: wrong answer len=%d\n",
				fct_len);
		return -RIG_ERJCTED;
	}

	*status = fctbuf[2] == '0' ? 0 : 1;

	return RIG_OK;
};


/*
 * kenwood_get_func
 * Assumes rig!=NULL, val!=NULL
 */
int kenwood_get_func(RIG *rig, vfo_t vfo, setting_t func, int *status)
{
		unsigned char fctbuf[16];
		int fct_len, retval;

		/* Optimize:
		 *   sort the switch cases with the most frequent first
		 */
		switch (func) {
		case RIG_FUNC_FAGC:
			retval = kenwood_transaction (rig, "GT;", 3, fctbuf, &fct_len);
			if (retval != RIG_OK)
				return retval;

			if (fct_len != 6) {
				rig_debug(RIG_DEBUG_ERR,"kenwood_get_func: "
								"wrong answer len=%d\n", fct_len);
				return -RIG_ERJCTED;
			}

			*status = fctbuf[4] != '4' ? 1 : 0;
			break;

		case RIG_FUNC_NB:
			return get_kenwood_func(rig, "NB;", 3, status);

		case RIG_FUNC_ABM:
			return get_kenwood_func(rig, "AM;", 3, status);

		case RIG_FUNC_COMP:
			return get_kenwood_func(rig, "PR;", 3, status);

		case RIG_FUNC_TONE:
			return get_kenwood_func(rig, "TO;", 3, status);

		case RIG_FUNC_TSQL:
			return get_kenwood_func(rig, "CT;", 3, status);

		case RIG_FUNC_VOX:
			return get_kenwood_func(rig, "VX;", 3, status);

		case RIG_FUNC_NR:
			return get_kenwood_func(rig, "NR;", 3, status);

 			/* FIXME on TS2000 */
		case RIG_FUNC_BC:
			return get_kenwood_func(rig, "BC;", 3, status);

		case RIG_FUNC_ANF:
			return get_kenwood_func(rig, "NT;", 3, status);

		case RIG_FUNC_LOCK:
			return get_kenwood_func(rig, "LK;", 3, status);

		default:
			rig_debug(RIG_DEBUG_ERR,"Unsupported get_func %#x", func);
			return -RIG_EINVAL;
		}

		return RIG_OK;
}

/*
 * kenwood_set_ctcss_tone
 * Assumes rig!=NULL, rig->caps->ctcss_list != NULL
 *
 * Warning! This is untested stuff! May work at least on TS-870S
 * 	Please owners report to me <f4cfe@users.sourceforge.net>, thanks. --SF

 * TODO: TS-2000 uses CN/CT
 */
int kenwood_set_ctcss_tone(RIG *rig, vfo_t vfo, tone_t tone)
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
	tone_len = sprintf(tonebuf,"EX%03d%04d;", 57, i+1);
	
	return kenwood_transaction (rig, tonebuf, tone_len, ackbuf, &ack_len);
}

/*
 * kenwood_get_ctcss_tone
 * Assumes rig!=NULL, rig->state.priv!=NULL
 */
int kenwood_get_ctcss_tone(RIG *rig, vfo_t vfo, tone_t *tone)
{
	const struct rig_caps *caps;
	unsigned char tonebuf[16];
	int tone_len, i, retval;
	unsigned int tone_idx;
								 
	caps = rig->caps;

	/* TODO: replace menu no 57 by a define */
	
	retval = kenwood_transaction (rig, "EX057;", 6, tonebuf, &tone_len);
	if (retval != RIG_OK)
		return retval;

	if (tone_len != 10) {
			rig_debug(RIG_DEBUG_ERR,"kenwood_get_ctcss_tone: unexpected reply "
							"'%s', len=%d\n", tonebuf, tone_len);
			return -RIG_ERJCTED;
	}

	sscanf(tonebuf+5, "%u", (int*)&tone_idx);

	if (tone_idx == 0) {
			rig_debug(RIG_DEBUG_ERR,"kenwood_get_ctcss_tone: Unexpected CTCSS "
							"no (%04d)\n", tone_idx);
			return -RIG_EPROTO;
	}
		
	/* check this tone exists. That's better than nothing. */
	for (i = 0; i<tone_idx; i++) {
		if (caps->ctcss_list[i] == 0) {
			rig_debug(RIG_DEBUG_ERR,"kenwood_get_ctcss_tone: CTCSS NG "
					"(%04d)\n", tone_idx);
			return -RIG_EPROTO;
		}
	}
	*tone = caps->ctcss_list[tone_idx-1];

	return RIG_OK;
}

/*
 * kenwood_set_ptt
 * Assumes rig!=NULL
 * TODO: kenwood_get_ptt reading P8 field returned by IF;
 */
int kenwood_set_ptt(RIG *rig, vfo_t vfo, ptt_t ptt)
{
		unsigned char ackbuf[16];
		int ack_len;

		return kenwood_transaction (rig, ptt==RIG_PTT_ON? "TX;":"RX;", 3, 
						ackbuf, &ack_len);
}


/*
 * kenwood_get_dcd
 * Assumes rig!=NULL, dcd!=NULL
 */
int kenwood_get_dcd(RIG *rig, vfo_t vfo, dcd_t *dcd)
{
	unsigned char busybuf[16];
	int busy_len, retval;
								 
	retval = kenwood_transaction (rig, "BY;", 3, busybuf, &busy_len);
	if (retval != RIG_OK)
		return retval;

	if (busy_len != 4) {
		rig_debug(RIG_DEBUG_ERR,"kenwood_get_dcd: wrong answer len=%d\n",
						busy_len);
		return -RIG_ERJCTED;
	}

	*dcd = (busybuf[2] == 0x01) ? RIG_DCD_ON : RIG_DCD_OFF;

	return RIG_OK;
}

/*
 * kenwood_set_trn
 * Assumes rig!=NULL
 */
int kenwood_set_trn(RIG *rig, vfo_t vfo, int trn)
{
		unsigned char trnbuf[16], ackbuf[16];
		int trn_len,ack_len;

		trn_len = sprintf(trnbuf,"AI%c;", trn==RIG_TRN_RIG?'1':'0');

		return kenwood_transaction (rig, trnbuf, trn_len, ackbuf, &ack_len);
}

/*
 * kenwood_get_trn
 * Assumes rig!=NULL, trn!=NULL
 */
int kenwood_get_trn(RIG *rig, vfo_t vfo, int *trn)
{
		unsigned char trnbuf[16];
		int trn_len, retval;

		retval = kenwood_transaction (rig, "AI;", 3, trnbuf, &trn_len);
		if (retval != RIG_OK)
			return retval;

		if (trn_len != 4) {
				rig_debug(RIG_DEBUG_ERR,"kenwood_get_trn: wrong answer"
								"len=%d\n", trn_len);
				return -RIG_ERJCTED;
		}
		*trn = trnbuf[2] != '0' ? RIG_TRN_RIG : RIG_TRN_OFF;

		return RIG_OK;
}

/*
 * kenwood_set_powerstat
 * Assumes rig!=NULL
 */
int kenwood_set_powerstat(RIG *rig, powerstat_t status)
{
		unsigned char pwrbuf[16], ackbuf[16];
		int pwr_len,ack_len;

		pwr_len = sprintf(pwrbuf,"PS%c;", status==RIG_POWER_ON?'1':'0');

		return kenwood_transaction (rig, pwrbuf, pwr_len, ackbuf, &ack_len);
}

/*
 * kenwood_get_powerstat
 * Assumes rig!=NULL, trn!=NULL
 */
int kenwood_get_powerstat(RIG *rig, powerstat_t *status)
{
		unsigned char pwrbuf[16];
		int pwr_len, retval;

		retval = kenwood_transaction (rig, "PS;", 3, pwrbuf, &pwr_len);
		if (retval != RIG_OK)
			return retval;

		if (pwr_len != 4) {
				rig_debug(RIG_DEBUG_ERR,"kenwood_get_powerstat: wrong answer "
								"len=%d\n", pwr_len);
				return -RIG_ERJCTED;
		}
		*status = pwrbuf[2] == '0' ? RIG_POWER_OFF : RIG_POWER_ON;

		return RIG_OK;
}

/*
 * kenwood_reset
 * Assumes rig!=NULL
 */
int kenwood_reset(RIG *rig, reset_t reset)
{
		unsigned char rstbuf[16], ackbuf[16];
		int rst_len,ack_len;
		char rst;

		switch(reset) {
			case RIG_RESET_VFO: rst='1'; break;
			case RIG_RESET_MASTER: rst='2'; break;
			default: 
				rig_debug(RIG_DEBUG_ERR,"kenwood_reset: unsupported reset %d\n",
								reset);
				return -RIG_EINVAL;
		}
		rst_len = sprintf(rstbuf,"SR%c;", rst);

		return kenwood_transaction (rig, rstbuf, rst_len, ackbuf, &ack_len);
}

/*
 * kenwood_send_morse
 * Assumes rig!=NULL
 */
int kenwood_send_morse(RIG *rig, vfo_t vfo, const char *msg)
{
		unsigned char morsebuf[30], ackbuf[16];
		int morse_len, ack_len;
		int msg_len, buff_len, retval;
		const char *p;

		p = msg;
		msg_len = strlen(msg);

		while(msg_len > 0) {
				/*
				 * TODO: check with "KY;" if char buffer is available.
				 * 		if not, sleep.
				 */
				buff_len = msg_len > 24 ? 24 : msg_len;

				strcpy(morsebuf, "KY ");
				strncat(morsebuf, p, buff_len);
				strcat(morsebuf, ";");
				morse_len = 4 + buff_len;
				retval = kenwood_transaction (rig, morsebuf, morse_len, 
								ackbuf, &ack_len);
				if (retval != RIG_OK)
					return retval;

				msg_len -= buff_len;
				p += buff_len;
		}

		return RIG_OK;
}

/*
 * kenwood_vfo_op
 * Assumes rig!=NULL
 */
int kenwood_vfo_op(RIG *rig, vfo_t vfo, vfo_op_t op)
{
		unsigned char *cmd, ackbuf[16];
		int ack_len;

		switch(op) {
			case RIG_OP_UP: cmd="UP;"; break;
			case RIG_OP_DOWN: cmd="DN;"; break;
			case RIG_OP_BAND_UP: cmd="BD;"; break;
			case RIG_OP_BAND_DOWN: cmd="BU;"; break;
			default: 
				rig_debug(RIG_DEBUG_ERR,"kenwood_vfo_op: unsupported op %#x\n",
								op);
				return -RIG_EINVAL;
		}

		return kenwood_transaction (rig, cmd, 3, ackbuf, &ack_len);
}

/*
 * kenwood_set_mem
 * Assumes rig!=NULL
 */
int kenwood_set_mem(RIG *rig, vfo_t vfo, int ch)
{
		unsigned char membuf[16], ackbuf[16];
		int mem_len, ack_len;

		/*
		 * "MCbmm;"
		 * where b is the bank number, mm the memory number.
		 * b can be a space
		 */
		mem_len = sprintf(membuf, "MC %02d;", ch);

		return kenwood_transaction (rig, membuf, mem_len, ackbuf, &ack_len);
}

/*
 * kenwood_get_mem
 * Assumes rig!=NULL
 */
int kenwood_get_mem(RIG *rig, vfo_t vfo, int *ch)
{
		unsigned char membuf[16];
		int retval, mem_len;

		/*
		 * "MCbmm;"
		 * where b is the bank number, mm the memory number.
		 * b can be a space
		 */

		retval = kenwood_transaction (rig, "MC;", 3, membuf, &mem_len);
		if (retval != RIG_OK)
				return retval;

		if (mem_len != 6) {
				rig_debug(RIG_DEBUG_ERR,"kenwood_get_mem: wrong answer "
								"len=%d\n", mem_len);
				return -RIG_ERJCTED;
		}
		*ch = atoi(membuf+2);

		return RIG_OK;
}

/*
 * kenwood_get_info
 * supposed to work only for TS2000...
 * Assumes rig!=NULL
 */
const char* kenwood_get_info(RIG *rig)
{
	unsigned char firmbuf[16];
	int firm_len, retval;
								 
	retval = kenwood_transaction (rig, "TY;", 3, firmbuf, &firm_len);
	if (retval != RIG_OK)
		return NULL;

	if (firm_len != 6) {
		rig_debug(RIG_DEBUG_ERR,"kenwood_get_info: wrong answer len=%d\n",
						firm_len);
		return NULL;
	}

	switch (firmbuf[4]) {
			case '0': return "Firmware: Overseas type";
			case '1': return "Firmware: Japanese 100W type";
			case '2': return "Firmware: Japanese 20W type";
			default: return "Firmware: unknown";
	}
}


/*
 * probe_kenwood
 */
rig_model_t probe_kenwood(port_t *port)
{
	unsigned char idbuf[16];
	int id_len, i, k_id;
	int retval;

	if (!port)
			return RIG_MODEL_NONE;

	port->write_delay = port->post_write_delay = 0;
	port->timeout = 50;
	port->retry = 1;

	retval = serial_open(port);
	if (retval != RIG_OK)
			return RIG_MODEL_NONE;

	retval = port_transaction(port, "ID;", 3, idbuf, &id_len);

	close(port->fd);

	if (retval != RIG_OK)
			return RIG_MODEL_NONE;

	/* 
	 * reply should be something like 'IDxxx;'
	 */
	if (id_len != 5 || id_len != 6) {
			idbuf[7] = '\0';
			rig_debug(RIG_DEBUG_VERBOSE,"probe_kenwood: protocol error,"
							" expected %d, received %d: %s\n",
							6, id_len, idbuf);
			return RIG_MODEL_NONE;
	}


	/* first, try ID string */
	for (i=0; kenwood_id_string_list[i].model != RIG_MODEL_NONE; i++) {
			if (!strncmp(kenwood_id_string_list[i].id, idbuf+2, 16)) {
					rig_debug(RIG_DEBUG_VERBOSE,"probe_kenwood: "
									"found %s\n", idbuf+2);
					return kenwood_id_string_list[i].model;
			}
	}

	/* then, try ID numbers */

	k_id = atoi(idbuf+2);

	for (i=0; kenwood_id_list[i].model != RIG_MODEL_NONE; i++) {
			if (kenwood_id_list[i].id == k_id) {
					rig_debug(RIG_DEBUG_VERBOSE,"probe_kenwood: "
									"found %03d\n", k_id);
					return kenwood_id_list[i].model;
			}
	}
	/*
	 * not found in known table.... 
	 * update kenwood_id_list[]!
	 */
	rig_debug(RIG_DEBUG_WARN,"probe_kenwood: found unknown device "
				"with ID %03d, please report to Hamlib "
				"developers.\n", k_id);

	return RIG_MODEL_NONE;
}


/*
 * init_kenwood is called by rig_backend_load
 */
int init_kenwood(void *be_handle)
{
		rig_debug(RIG_DEBUG_VERBOSE, "kenwood: _init called\n");

		rig_register(&ts450s_caps);
		rig_register(&ts570d_caps);
		rig_register(&ts570s_caps);
		rig_register(&ts870s_caps);
		rig_register(&thd7a_caps);

		return RIG_OK;
}


