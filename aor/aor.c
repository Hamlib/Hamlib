/*
 *  Hamlib AOR backend - main file
 *  Copyright (c) 2000-2004 by Stephane Fillod
 *
 *	$Id: aor.c,v 1.28 2004-06-14 21:10:11 fillods Exp $
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

#include "hamlib/rig.h"
#include "serial.h"
#include "misc.h"
#include "register.h"

#include "aor.h"



/*
 * acknowledge is CR
 * Is \r portable enough?
 */
#define CR '\r'
#define EOM "\r"

#define BUFSZ 64

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


/*
 * aor_set_freq
 * Assumes rig!=NULL
 */
int aor_set_freq(RIG *rig, vfo_t vfo, freq_t freq)
{
		unsigned char freqbuf[BUFSZ], ackbuf[BUFSZ];
		int freq_len, ack_len, retval;
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

		freq_len = sprintf(freqbuf,"RF%010lld" EOM, f);

		retval = aor_transaction (rig, freqbuf, freq_len, ackbuf, &ack_len);
		if (retval != RIG_OK)
			return retval;

		return RIG_OK;
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
		long long f;

		retval = aor_transaction (rig, "RX" EOM, 3, freqbuf, &freq_len);
		if (retval != RIG_OK)
			return retval;

		rfp = strstr(freqbuf, "RF");
		if (!rfp) {
			rig_debug(RIG_DEBUG_WARN, "NO RF in returned string in aor_get_freq: '%s'\n",
					freqbuf);
			return -RIG_EPROTO;
		}

		sscanf(rfp+2,"%lld", &f);
		*freq = f;

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
		case 'R': *vfo = RIG_VFO_MEM; break;
		default:
				rig_debug(RIG_DEBUG_ERR,"aor_get_vfo: unknown vfo %c\n",
								vfobuf[1]);
				return -RIG_EINVAL;
		}

		return RIG_OK;
}


/*
 * aor_set_mode
 * Assumes rig!=NULL
 */
int aor_set_mode(RIG *rig, vfo_t vfo, rmode_t mode, pbwidth_t width)
{
		unsigned char mdbuf[BUFSZ],ackbuf[BUFSZ];
		int mdbuf_len, ack_len, aormode, retval;

		switch (mode) {
			case RIG_MODE_AM:       
					switch(width) {
						case RIG_PASSBAND_NORMAL:
						case s_kHz(9): aormode = MD_AM; break;

						case s_kHz(12): aormode = MD_WAM; break;
						case s_kHz(3): aormode = MD_NAM; break;
						default:
							rig_debug(RIG_DEBUG_ERR,
								"aor_set_mode: unsupported passband %d %d\n",
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
								"aor_set_mode: unsupported passband %d %d\n",
								mode, width);
						return -RIG_EINVAL;
					}
					break;
			default:
				rig_debug(RIG_DEBUG_ERR,"aor_set_mode: unsupported mode %d\n",
								mode);
				return -RIG_EINVAL;
		}

		mdbuf_len = sprintf(mdbuf, "MD%c" EOM, aormode);
		retval = aor_transaction (rig, mdbuf, mdbuf_len, ackbuf, &ack_len);

		return retval;
}

/*
 * aor_get_mode
 * Assumes rig!=NULL, mode!=NULL
 */
int aor_get_mode(RIG *rig, vfo_t vfo, rmode_t *mode, pbwidth_t *width)
{
		unsigned char ackbuf[BUFSZ];
		int ack_len, retval;


		retval = aor_transaction (rig, "MD" EOM, 3, ackbuf, &ack_len);
		if (retval != RIG_OK)
				return retval;

		if (ack_len != 2 || ackbuf[1] != CR) {
				rig_debug(RIG_DEBUG_ERR,"aor_get_mode: ack NG, len=%d\n",
								ack_len);
				return -RIG_ERJCTED;
		}

		*width = RIG_PASSBAND_NORMAL;
		switch (ackbuf[0]) {
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
				rig_debug(RIG_DEBUG_ERR,"aor_get_mode: unsupported mode %d\n",
								ackbuf[0]);
				return -RIG_EINVAL;
		}
		if (*width == RIG_PASSBAND_NORMAL)
				*width = rig_passband_normal(rig, *mode);

		return RIG_OK;
}

/*
 * aor_set_ts
 * Assumes rig!=NULL
 */
int aor_set_ts(RIG *rig, vfo_t vfo, shortfreq_t ts)
{
		unsigned char tsbuf[BUFSZ],ackbuf[BUFSZ];
		int ts_len, ack_len;

		/*
		 * actually, tuning step must be like nnnnm0, 
		 * where m must be 0 or 5 (for 50Hz).
		 */
		ts_len = sprintf(tsbuf,"ST%06ld" EOM, ts);

		return aor_transaction (rig, tsbuf, ts_len, ackbuf, &ack_len);
}


/*
 * aor_set_level
 * Assumes rig!=NULL, rig->state.priv!=NULL
 */
int aor_set_level(RIG *rig, vfo_t vfo, setting_t level, value_t val)
{
	struct aor_priv_data *priv;
	struct rig_state *rs;
	unsigned char lvlbuf[BUFSZ],ackbuf[BUFSZ];
	int lvl_len, ack_len;
	unsigned i;

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
	default:
		rig_debug(RIG_DEBUG_ERR,"Unsupported aor_set_level %d", level);
		return -RIG_EINVAL;
	}

	return aor_transaction (rig, lvlbuf, lvl_len, ackbuf, &ack_len);
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
	case RIG_LEVEL_ATT:
		lvl_len = sprintf(lvlbuf, "AT" EOM);
		break;
	default:
		rig_debug(RIG_DEBUG_ERR,"Unsupported aor_set_level %d", level);
		return -RIG_EINVAL;
	}

	retval = aor_transaction (rig, lvlbuf, lvl_len, ackbuf, &ack_len);

	if (retval != RIG_OK)
			return retval;

	switch (level) {
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
			rig_debug(RIG_DEBUG_ERR,"Unsupported att aor_get_level %d",
							att);
			return -RIG_EPROTO;
		}
		val->i = rs->attenuator[att-1];
		break;
		}
	default:
		rig_debug(RIG_DEBUG_ERR,"Unsupported aor_get_level %d", level);
		return -RIG_EINVAL;
	}

	return RIG_OK;
}


/*
 * aor_set_powerstat
 * Assumes rig!=NULL, rig->state.priv!=NULL
 */
int aor_set_powerstat(RIG *rig, powerstat_t status)
{
		if (status != RIG_POWER_OFF)
				return -RIG_EINVAL;

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
		int ack_len;
		char ackbuf[BUFSZ];

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

		return aor_transaction (rig, aorcmd, strlen(aorcmd), ackbuf, &ack_len);
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

