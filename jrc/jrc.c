/*
 *  Hamlib JRC backend - main file
 *  Copyright (c) 2001 by Stephane Fillod
 *
 *		$Id: jrc.c,v 1.2 2001-12-20 08:02:01 fillods Exp $
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
#include <serial.h>
#include <misc.h>
#include <cal.h>

#include "jrc.h"


/*
 * Carriage return
 */
#define EOM "\r"

/*
 * modes in use by the "2G" command
 */
#define MD_RTTY	'0'
#define MD_CW	'1'
#define MD_USB	'2'
#define MD_LSB	'3'
#define MD_AM	'4'
#define MD_FM 	'5'
#define MD_AMS	'6'
#define MD_ECCS_USB	'7'
#define MD_ECCS_LSB	'8'
#define MD_WFM	'9'


/*
 * jrc_transaction
 * We assume that rig!=NULL, rig->state!= NULL, data!=NULL, data_len!=NULL
 * Otherwise, you'll get a nice seg fault. You've been warned!
 * TODO: error case handling
 */
int jrc_transaction(RIG *rig, const char *cmd, int cmd_len, char *data, int *data_len)
{
	int i, count, retval;
	struct rig_state *rs;

	rs = &rig->state;

	serial_flush(&rs->rigport);

	retval = write_block(&rs->rigport, cmd, cmd_len);
	if (retval != RIG_OK)
			return retval;

	/* no data expected, TODO: flush input? */
	if (!data || !data_len)
			return 0;
	/*
	 * buffered read are quite helpful here!
	 * However, an automate with a state model would be more efficient..
	 */
	i = 0;
	do {
		count = fread_block(&rs->rigport, data+i, 1);
		if (count > 0)
				i += count;
		else if (count < 0)
				return count;
	} while (count > 0 && data[i-1] != EOM[0]);

	*data_len = i;

	return RIG_OK;
}


int jrc_open(RIG *rig)
{
  int retval;

  /* Turning computer control ON */

  retval = jrc_transaction(rig, "H1" EOM, 3, NULL, NULL);
  return retval;
}

int jrc_close(RIG *rig)
{
  int retval;

  /* Turning computer control OFF */

  retval = jrc_transaction(rig, "H0" EOM, 3, NULL, NULL);
  return retval;
}


/*
 * jrc_set_freq
 * Assumes rig!=NULL
 */
int jrc_set_freq(RIG *rig, vfo_t vfo, freq_t freq)
{
		char freqbuf[16];
		int freq_len;

		/* max 10 digits */
		if (freq >= GHz(10))
				return -RIG_EINVAL;

		freq_len = sprintf(freqbuf, "F%10Ld" EOM, freq);

		return jrc_transaction (rig, freqbuf, freq_len, NULL, NULL);
}

/*
 * jrc_get_freq
 * Assumes rig!=NULL, freq!=NULL
 */
int jrc_get_freq(RIG *rig, vfo_t vfo, freq_t *freq)
{
		int freq_len, retval;
		char freqbuf[32];

		retval = jrc_transaction (rig, "F" EOM, 2, freqbuf, &freq_len);
		if (retval != RIG_OK)
			return retval;

		if (freq_len != 12) {
			rig_debug(RIG_DEBUG_ERR,"jrc_get_freq: wrong answer %s, "
							"len=%d\n", freqbuf, freq_len);
			return -RIG_ERJCTED;
		}
		freqbuf[freq_len-1] = '\0';

		/* extract freq */
		sscanf(freqbuf+1, "%Lu", freq);

		return RIG_OK;
}

/*
 * jrc_set_mode
 * Assumes rig!=NULL
 */
int jrc_set_mode(RIG *rig, vfo_t vfo, rmode_t mode, pbwidth_t width)
{
		char mdbuf[16];
		int retval, mdbuf_len;
		char amode;
		const char *bandwidth;

		switch (mode) {
				/* FIXME: WFM, MD_AMS and ECSS modes */
			case RIG_MODE_CW:       amode = MD_CW; break;
			case RIG_MODE_USB:      amode = MD_USB; break;
			case RIG_MODE_LSB:      amode = MD_LSB; break;
			case RIG_MODE_FM:       amode = MD_FM; break;
			case RIG_MODE_AM:       amode = MD_AM; break;
			case RIG_MODE_RTTY:		amode = MD_RTTY; break;
			default:
				rig_debug(RIG_DEBUG_ERR,
								"jrc_set_mode: unsupported mode %d\n",
								mode);
				return -RIG_EINVAL;
		}

		mdbuf_len = sprintf(mdbuf, "D" "%c" EOM, amode);
		retval = jrc_transaction (rig, mdbuf, mdbuf_len, NULL, NULL);
		if (retval != RIG_OK)
			return retval;

		/*
		 * determine passband
		 */
		if (width != RIG_PASSBAND_NORMAL && 
						width < rig_passband_normal(rig, mode))
				bandwidth = "B2" EOM;	/* narrow */
		else if (width != RIG_PASSBAND_NORMAL && 
						width > rig_passband_normal(rig, mode))
				bandwidth = "B0" EOM;	/* wide */
		else 
				bandwidth = "B1" EOM;	/* intermediate */

		retval = jrc_transaction (rig, bandwidth, 3, NULL, NULL);

		return retval;
}

/*
 * jrc_set_func
 * Assumes rig!=NULL
 */
int jrc_set_func(RIG *rig, vfo_t vfo, setting_t func, int status)
{
		int cmd_len;
		char cmdbuf[32];

		/* Optimize:
		 *   sort the switch cases with the most frequent first
		 */
		switch (func) {
		case RIG_FUNC_FAGC:
				/* FIXME: FAGC levels */
			cmd_len = sprintf(cmdbuf, "G%d" EOM, status?0:2);

			return jrc_transaction (rig, cmdbuf, cmd_len, NULL, NULL);

		case RIG_FUNC_NB:
			/* FIXME: NB1 and NB2 */
			cmd_len = sprintf(cmdbuf, "N%d" EOM, status?1:0);

			return jrc_transaction (rig, cmdbuf, cmd_len, NULL, NULL);

			/*
			 * FIXME: which BB mode for NR and BC at same time ?
			 */
		case RIG_FUNC_NR:
			cmd_len = sprintf(cmdbuf, "BB%d" EOM, status?1:0);

			return jrc_transaction (rig, cmdbuf, cmd_len, NULL, NULL);

		case RIG_FUNC_BC:
			cmd_len = sprintf(cmdbuf, "BB%d" EOM, status?2:0);

			return jrc_transaction (rig, cmdbuf, cmd_len, NULL, NULL);

		case RIG_FUNC_LOCK:
			cmd_len = sprintf(cmdbuf, "DD%d" EOM, status?1:0);

			return jrc_transaction (rig, cmdbuf, cmd_len, NULL, NULL);

		default:
			rig_debug(RIG_DEBUG_ERR,"Unsupported set_func %d\n", func);
			return -RIG_EINVAL;
		}

		return RIG_OK;
}


/*
 * jrc_get_func
 * Assumes rig!=NULL, status!=NULL
 */
int jrc_get_func(RIG *rig, vfo_t vfo, setting_t func, int *status)
{
		int retval, func_len;
		char funcbuf[32];

		/* Optimize:
		 *   sort the switch cases with the most frequent first
		 */
		switch (func) {
		case RIG_FUNC_FAGC:
				/* FIXME: FAGC levels */
			retval = jrc_transaction (rig, "G" EOM, 2, funcbuf, &func_len);
			if (retval != RIG_OK)
				return retval;

			if (func_len != 3 || func_len != 6) {
				rig_debug(RIG_DEBUG_ERR,"jrc_get_func: wrong answer %s, "
							"len=%d\n", funcbuf, func_len);
				return -RIG_ERJCTED;
			}
			*status = funcbuf[1] != '2';

			return RIG_OK;

		case RIG_FUNC_NB:
			/* FIXME: NB1 and NB2 */
			retval = jrc_transaction (rig, "N" EOM, 2, funcbuf, &func_len);
			if (retval != RIG_OK)
				return retval;

			if (func_len != 3) {
				rig_debug(RIG_DEBUG_ERR,"jrc_get_func: wrong answer %s, "
							"len=%d\n", funcbuf, func_len);
				return -RIG_ERJCTED;
			}
			*status = funcbuf[1] != '0';

			return RIG_OK;

			/*
			 * FIXME: which BB mode for NR and BC at same time ?
			 */
		case RIG_FUNC_NR:
			retval = jrc_transaction (rig, "BB" EOM, 3, funcbuf, &func_len);
			if (retval != RIG_OK)
				return retval;

			if (func_len != 3) {
				rig_debug(RIG_DEBUG_ERR,"jrc_get_func: wrong answer %s, "
							"len=%d\n", funcbuf, func_len);
				return -RIG_ERJCTED;
			}
			*status = funcbuf[2] == '1';

			return RIG_OK;

		case RIG_FUNC_BC:
			retval = jrc_transaction (rig, "BB" EOM, 3, funcbuf, &func_len);
			if (retval != RIG_OK)
				return retval;

			if (func_len != 3) {
				rig_debug(RIG_DEBUG_ERR,"jrc_get_func: wrong answer %s, "
							"len=%d\n", funcbuf, func_len);
				return -RIG_ERJCTED;
			}
			*status = funcbuf[2] == '2';

			return RIG_OK;

		case RIG_FUNC_LOCK:
			retval = jrc_transaction (rig, "DD" EOM, 3, funcbuf, &func_len);
			if (retval != RIG_OK)
				return retval;

			if (func_len != 3) {
				rig_debug(RIG_DEBUG_ERR,"jrc_get_func: wrong answer %s, "
							"len=%d\n", funcbuf, func_len);
				return -RIG_ERJCTED;
			}
			*status = funcbuf[1] == '1';

			return RIG_OK;

		default:
			rig_debug(RIG_DEBUG_ERR,"Unsupported get_func %d\n", func);
			return -RIG_EINVAL;
		}

		return RIG_OK;
}

/*
 * jrc_set_level
 * Assumes rig!=NULL
 * FIXME: cannot support PREAMP and ATT both at same time (make sens though)
 */
int jrc_set_level(RIG *rig, vfo_t vfo, setting_t level, value_t val)
{
		int cmd_len;
		char cmdbuf[32];

		/* Optimize:
		 *   sort the switch cases with the most frequent first
		 */
		switch (level) {
		case RIG_LEVEL_ATT:
			cmd_len = sprintf(cmdbuf, "A%d" EOM, val.i?1:0);

			return jrc_transaction (rig, cmdbuf, cmd_len, NULL, NULL);

		case RIG_LEVEL_RF:
			cmd_len = sprintf(cmdbuf, "HH%03d" EOM, (int)(val.f*255.0));

			return jrc_transaction (rig, cmdbuf, cmd_len, NULL, NULL);

		case RIG_LEVEL_AF:
			cmd_len = sprintf(cmdbuf, "JJ%03d" EOM, (int)(val.f*255.0));

			return jrc_transaction (rig, cmdbuf, cmd_len, NULL, NULL);

		case RIG_LEVEL_SQL:
			cmd_len = sprintf(cmdbuf, "JJ%03d" EOM, (int)(val.f*255.0));

			return jrc_transaction (rig, cmdbuf, cmd_len, NULL, NULL);

		case RIG_LEVEL_NOTCHF:
			cmd_len = sprintf(cmdbuf, "FF%+04d" EOM, val.i);

			return jrc_transaction (rig, cmdbuf, cmd_len, NULL, NULL);

		case RIG_LEVEL_AGC:
			cmd_len = sprintf(cmdbuf, "NN%03d" EOM, (int)(val.f*255.0));

			return jrc_transaction (rig, cmdbuf, cmd_len, NULL, NULL);


		default:
			rig_debug(RIG_DEBUG_ERR,"Unsupported set_level %d\n", level);
			return -RIG_EINVAL;
		}

		return RIG_OK;
}


/*
 * jrc_get_level
 * Assumes rig!=NULL, val!=NULL
 */
int jrc_get_level(RIG *rig, vfo_t vfo, setting_t level, value_t *val)
{
 		struct jrc_priv_caps *priv;
		int retval, lvl_len, lvl;
		char lvlbuf[32];

		priv = (struct jrc_priv_caps*)rig->caps->priv;

		/* Optimize:
		 *   sort the switch cases with the most frequent first
		 */
		switch (level) {
		case RIG_LEVEL_STRENGTH:
				/* read A/D converted value */
			retval = jrc_transaction (rig, "M" EOM, 2, lvlbuf, &lvl_len);
			if (retval != RIG_OK)
				return retval;

			if (lvl_len != 5) {
				rig_debug(RIG_DEBUG_ERR,"jrc_get_level: wrong answer"
								"len=%d\n", lvl_len);
				return -RIG_ERJCTED;
			}

			lvlbuf[4] = '\0';
			val->i = rig_raw2val(atoi(lvlbuf+1), &priv->str_cal);
			break;

		case RIG_LEVEL_SQLSTAT:
			return -RIG_ENIMPL;	/* get_dcd ? */

		case RIG_LEVEL_ATT:
			retval = jrc_transaction (rig, "A" EOM, 2, lvlbuf, &lvl_len);
			if (retval != RIG_OK)
				return retval;

			if (lvl_len != 3) {
				rig_debug(RIG_DEBUG_ERR,"jrc_get_level: wrong answer"
								"len=%d\n", lvl_len);
				return -RIG_ERJCTED;
			}

			val->i = lvlbuf[1] == '1' ? 20 : 0;
			break;

		case RIG_LEVEL_RF:
			retval = jrc_transaction (rig, "HH" EOM, 3, lvlbuf, &lvl_len);
			if (retval != RIG_OK)
				return retval;

			if (lvl_len != 6) {
				rig_debug(RIG_DEBUG_ERR,"jrc_get_level: wrong answer"
								"len=%d\n", lvl_len);
				return -RIG_ERJCTED;
			}
			/*
			 * 000..255
			 */
			sscanf(lvlbuf+2, "%u", &lvl);
			val->f = (float)lvl/255.0;
			break;

		case RIG_LEVEL_AF:
			retval = jrc_transaction (rig, "JJ" EOM, 3, lvlbuf, &lvl_len);
			if (retval != RIG_OK)
				return retval;

			if (lvl_len != 6) {
				rig_debug(RIG_DEBUG_ERR,"jrc_get_level: wrong answer"
								"len=%d\n", lvl_len);
				return -RIG_ERJCTED;
			}
			/*
			 * 000..255
			 */
			sscanf(lvlbuf+2, "%u", &lvl);
			val->f = (float)lvl/255.0;
			break;

		case RIG_LEVEL_SQL:
			retval = jrc_transaction (rig, "LL" EOM, 3, lvlbuf, &lvl_len);
			if (retval != RIG_OK)
				return retval;

			if (lvl_len != 6) {
				rig_debug(RIG_DEBUG_ERR,"jrc_get_level: wrong answer"
								"len=%d\n", lvl_len);
				return -RIG_ERJCTED;
			}
			/*
			 * 000..255
			 */
			sscanf(lvlbuf+2, "%u", &lvl);
			val->f = (float)lvl/255.0;
			break;

		case RIG_LEVEL_NOTCHF:
			retval = jrc_transaction (rig, "GG" EOM, 3, lvlbuf, &lvl_len);
			if (retval != RIG_OK)
				return retval;

			if (lvl_len != 8) {
				rig_debug(RIG_DEBUG_ERR,"jrc_get_level: wrong answer"
								"len=%d\n", lvl_len);
				return -RIG_ERJCTED;
			}
			/*
			 * 000..255
			 */
			sscanf(lvlbuf+2, "%d", &lvl);
			val->f = (float)lvl/255.0;
			break;

		default:
			rig_debug(RIG_DEBUG_ERR,"Unsupported get_level %d\n", level);
			return -RIG_EINVAL;
		}

		return RIG_OK;
}

/*
 * jrc_get_dcd
 * Assumes rig!=NULL, dcd!=NULL
 */
int jrc_get_dcd(RIG *rig, vfo_t vfo, dcd_t *dcd)
{
		char dcdbuf[16];
		int dcd_len, retval;

		retval = jrc_transaction (rig, "Q" EOM, 2, dcdbuf, &dcd_len);
		if (retval != RIG_OK)
			return retval;

		if (dcd_len != 3) {
			rig_debug(RIG_DEBUG_ERR,"jrc_get_dcd: wrong answer %s, "
							"len=%d\n", dcdbuf, dcd_len);
			return -RIG_ERJCTED;
		}
		*dcd = dcdbuf[1] == '0' ? RIG_DCD_ON : RIG_DCD_OFF;

		return RIG_OK;
}

/*
 * jrc_set_trn
 * Assumes rig!=NULL
 */
int jrc_set_trn(RIG *rig, int trn)
{
		unsigned char trnbuf[16];
		int trn_len;

		trn_len = sprintf(trnbuf, "I%d" EOM, trn==RIG_TRN_RIG?1:0);

		return jrc_transaction (rig, trnbuf, trn_len, NULL, NULL);
}

/*
 * jrc_set_powerstat
 * Assumes rig!=NULL
 */
int jrc_set_powerstat(RIG *rig, powerstat_t status)
{
		unsigned char pwrbuf[16];
		int pwr_len;

		pwr_len = sprintf(pwrbuf, "T%d" EOM, status==RIG_POWER_ON?1:0);

		return jrc_transaction (rig, pwrbuf, pwr_len, NULL, NULL);
}

/*
 * jrc_reset
 * Assumes rig!=NULL
 */
int jrc_reset(RIG *rig, reset_t reset)
{
		unsigned char rstbuf[16];
		int rst_len;
		char rst;

		switch(reset) {
			case RIG_RESET_MCALL: rst='1'; break;	/* mem clear */
			case RIG_RESET_VFO: rst='2'; break;		/* user setup default */
			case RIG_RESET_MASTER: rst='3'; break;	/* 1 + 2 */
			default: 
				rig_debug(RIG_DEBUG_ERR,"jrc_reset: unsupported reset %d\n",
								reset);
				return -RIG_EINVAL;
		}
		rst_len = sprintf(rstbuf, "Z%c" EOM, rst);

		return jrc_transaction (rig, rstbuf, rst_len, NULL, NULL);
}

/*
 * jrc_set_mem
 * Assumes rig!=NULL
 */
int jrc_set_mem(RIG *rig, vfo_t vfo, int ch)
{
		char cmdbuf[16];
		char membuf[16];
		int cmd_len, mem_len;

		if (ch < 0 || ch > 999)
				return -RIG_EINVAL;

		cmd_len = sprintf(cmdbuf, "C%03u" EOM, ch);

		/* don't care about the Automatic reponse from receiver */

		return jrc_transaction (rig, cmdbuf, cmd_len, membuf, &mem_len);
}


/*
 * init_jrc is called by rig_backend_load
 */
int init_jrc(void *be_handle)
{
		rig_debug(RIG_DEBUG_VERBOSE, "jrc: _init called\n");

		rig_register(&nrd545_caps);

		return RIG_OK;
}



