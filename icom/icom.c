/*
 * hamlib - (C) Frank Singleton 2000 (vk3fcs@ix.netcom.com)
 *
 * icom.c - Copyright (C) 2000,2001 Stephane Fillod
 * This shared library provides an API for communicating
 * via serial interface to an ICOM using the "CI-V" interface.
 *
 *
 * $Id: icom.c,v 1.31 2001-06-20 06:08:21 f4cfe Exp $  
 *
 *
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 * 
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 * 
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.
 * 
 */

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
#  include <cal.h>
#  define HAMLIB_DLL
#  include <hamlib/rig_dll.h>
#else
#  include <hamlib/rig.h>
#  include <serial.h>
#  include <misc.h>
#  include <cal.h>
#endif

#include "icom.h"
#include "icom_defs.h"
#include "frame.h"

const struct ts_sc_list r8500_ts_sc_list[] = {
		{ 10, 0x00 },
		{ 50, 0x01 },
		{ 100, 0x02 },
		{ kHz(1), 0x03 },
		{ 12500, 0x04 },
		{ kHz(5), 0x05 },
		{ kHz(9), 0x06 },
		{ kHz(10), 0x07 },
		{ 12500, 0x08 },
		{ kHz(20), 0x09 },
		{ kHz(25), 0x10 },
		{ kHz(100), 0x11 },
		{ MHz(1), 0x12 },
		{ 0, 0 },	/* programmable tuning step not supported */
};

const struct ts_sc_list ic737_ts_sc_list[] = {
		{ 10, 0x00 },
		{ kHz(1), 0x01 },
		{ kHz(2), 0x02 },
		{ kHz(3), 0x03 },
		{ kHz(4), 0x04 },
		{ kHz(5), 0x05 },
		{ kHz(6), 0x06 },
		{ kHz(7), 0x07 },
		{ kHz(8), 0x08 },
		{ kHz(9), 0x09 },
		{ kHz(10), 0x10 },
		{ 0, 0 },
};

const struct ts_sc_list r75_ts_sc_list[] = {
		{ 10, 0x00 },
		{ 100, 0x01 },
		{ kHz(1), 0x02 },
		{ kHz(5), 0x03 },
		{ 6250, 0x04 },
		{ kHz(9), 0x05 },
		{ kHz(10), 0x06 },
		{ 12500, 0x07 },
		{ kHz(20), 0x08 },
		{ kHz(25), 0x09 },
		{ kHz(100), 0x10 },
		{ MHz(1), 0x11 },
		{ 0, 0 },
};

const struct ts_sc_list r7100_ts_sc_list[] = {
		{ 100, 0x00 },
		{ kHz(1), 0x01 },
		{ kHz(5), 0x02 },
		{ kHz(10), 0x03 },
		{ 12500, 0x04 },
		{ kHz(20), 0x05 },
		{ kHz(25), 0x06 },
		{ kHz(100), 0x07 },
		{ 0, 0 },
};

const struct ts_sc_list ic756_ts_sc_list[] = {
		{ 10, 0x00 },
		{ kHz(1), 0x01 },
		{ kHz(5), 0x02 },
		{ kHz(9), 0x03 },
		{ kHz(10), 0x04 },
		{ 0, 0 },
};

const struct ts_sc_list ic706_ts_sc_list[] = {
		{ 10, 0x00 },
		{ 100, 0x01 },
		{ kHz(1), 0x02 },
		{ kHz(5), 0x03 },
		{ kHz(9), 0x04 },
		{ kHz(10), 0x05 },
		{ 12500, 0x06 },
		{ kHz(20), 0x07 },
		{ kHz(25), 0x08 },
		{ kHz(100), 0x09 },
		{ 0, 0 },
};



struct icom_addr {
		rig_model_t model;
		unsigned char re_civ_addr;
};

/*
 * Please, if the default CI-V address of your rig is listed as UNKNOWN_ADDR,
 * send the value to <f4cfe@users.sourceforge.net> for inclusion. Thanks --SF
 *
 * TODO: sort this list with most frequent rigs first.
 */
static const struct icom_addr icom_addr_list[] = {
		{ RIG_MODEL_IC706, 0x48 },
		{ RIG_MODEL_IC706MKII, 0x4e },
		{ RIG_MODEL_IC706MKIIG, 0x58 },
		{ RIG_MODEL_IC271, 0x20 },
		{ RIG_MODEL_IC275, 0x10 },
		{ RIG_MODEL_IC375, 0x12 },
		{ RIG_MODEL_IC471, 0x22 },
		{ RIG_MODEL_IC475, 0x14 },
		{ RIG_MODEL_IC575, 0x16 },
		{ RIG_MODEL_IC707, 0x3e },
		{ RIG_MODEL_IC725, 0x28 },
		{ RIG_MODEL_IC726, 0x30 },
		{ RIG_MODEL_IC728, 0x38 },
		{ RIG_MODEL_IC729, 0x3a },
		{ RIG_MODEL_IC731, 0x02 },	/* need confirmation */
		{ RIG_MODEL_IC735, 0x04 },
		{ RIG_MODEL_IC736, 0x40 },
		{ RIG_MODEL_IC746, 0x56 },
		{ RIG_MODEL_IC737, 0x3c },
		{ RIG_MODEL_IC738, 0x44 },
		{ RIG_MODEL_IC751, 0x1c },
		{ RIG_MODEL_IC751A, 0x1c },
		{ RIG_MODEL_IC756, 0x50 },
		{ RIG_MODEL_IC756PRO, 0x5c },
		{ RIG_MODEL_IC761, 0x1e },
		{ RIG_MODEL_IC765, 0x2c },
		{ RIG_MODEL_IC775, 0x46 },
		{ RIG_MODEL_IC781, 0x26 },
		{ RIG_MODEL_IC820, 0x42 },
		{ RIG_MODEL_IC821, 0x4c },
		{ RIG_MODEL_IC821H, 0x4c },
		{ RIG_MODEL_IC970, 0x2e },
		{ RIG_MODEL_IC1271, 0x24 },
		{ RIG_MODEL_IC1275, 0x18 },
		{ RIG_MODEL_ICR71, 0x1a },
		{ RIG_MODEL_ICR72, 0x32 },
		{ RIG_MODEL_ICR75, 0x5a },
		{ RIG_MODEL_ICR7000, 0x08 },
		{ RIG_MODEL_ICR7100, 0x34 },
		{ RIG_MODEL_ICR8500, 0x4a },
		{ RIG_MODEL_ICR9000, 0x2a },
		{ RIG_MODEL_MINISCOUT, 0x94 },
		{ RIG_MODEL_IC718, 0x36 },	/* need confirmation */
		{ RIG_MODEL_NONE, 0 },
};

/*
 * This is a generic icom_init function.
 * You might want to define yours, so you can customize it for your rig
 *
 * Basically, it sets up *priv 
 * REM: serial port is already open (rig->state.rigport.fd)
 */
int icom_init(RIG *rig)
{
		struct icom_priv_data *priv;
		const struct icom_priv_caps *priv_caps;
		const struct rig_caps *caps;

		if (!rig || !rig->caps)
				return -RIG_EINVAL;

		caps = rig->caps;

		if (!caps->priv)
				return -RIG_ECONF;

		priv_caps = (const struct icom_priv_caps *) caps->priv;

		priv = (struct icom_priv_data*)malloc(sizeof(struct icom_priv_data));
		if (!priv) {
				/* whoops! memory shortage! */
				return -RIG_ENOMEM;
		}

		rig->state.priv = (void*)priv;

		/* TODO: CI-V address should be customizable */

		/*
		 * init the priv_data from static struct 
		 *          + override with preferences
		 */

		priv->re_civ_addr = 0x00;

		priv->re_civ_addr = priv_caps->re_civ_addr;
		priv->civ_731_mode = priv_caps->civ_731_mode;
		memcpy(&priv->str_cal, &priv_caps->str_cal, sizeof(cal_table_t));

		return RIG_OK;
}

/*
 * ICOM Generic icom_cleanup routine
 * the serial port is closed by the frontend
 */
int icom_cleanup(RIG *rig)
{
		if (!rig)
				return -RIG_EINVAL;

		if (rig->state.priv)
				free(rig->state.priv);
		rig->state.priv = NULL;

		return RIG_OK;
}


/*
 * icom_set_freq
 * Assumes rig!=NULL, rig->state.priv!=NULL
 */
int icom_set_freq(RIG *rig, vfo_t vfo, freq_t freq)
{
		struct icom_priv_data *priv;
		struct rig_state *rs;
		unsigned char freqbuf[16], ackbuf[16];
		int freq_len, ack_len, retval;

		rs = &rig->state;
		priv = (struct icom_priv_data*)rs->priv;


		freq_len = priv->civ_731_mode ? 4:5;
		/*	
		 * to_bcd requires nibble len
		 */
		to_bcd(freqbuf, freq, freq_len*2);

		retval = icom_transaction (rig, C_SET_FREQ, -1, freqbuf, freq_len, 
						ackbuf, &ack_len);
		if (retval != RIG_OK)
				return retval;

		if (ack_len != 1 || ackbuf[0] != ACK) {
				rig_debug(RIG_DEBUG_ERR,"icom_set_freq: ack NG (%#.2x), "
								"len=%d\n", ackbuf[0],ack_len);
				return -RIG_ERJCTED;
		}

		return RIG_OK;
}

/*
 * icom_get_freq
 * Assumes rig!=NULL, rig->state.priv!=NULL, freq!=NULL
 * Note: old rig may return less than 4/5 bytes for get_freq
 */
int icom_get_freq(RIG *rig, vfo_t vfo, freq_t *freq)
{
		struct icom_priv_data *priv;
		struct rig_state *rs;
		unsigned char freqbuf[16];
		int freq_len, retval;

		rs = &rig->state;
		priv = (struct icom_priv_data*)rs->priv;

		retval = icom_transaction (rig, C_RD_FREQ, -1, NULL, 0, 
						freqbuf, &freq_len);
		if (retval != RIG_OK)
				return retval;

		/*
		 * freqbuf should contain Cn,Data area
		 */
		freq_len--;

		/*
		 * is it a blank mem channel ?
		 */
		if (freq_len == 1 && freqbuf[0] == 0xff) {
			*freq = RIG_FREQ_NONE;

			return RIG_OK;
		}

		/*
		 * TODO: older rig may return less than 4 or 5 bytes (for low freqs)!
		 *  also if freq is undefined (i.e. blank memory) set freq to RIG_FREQ_NONE
		 */
		if (freq_len != (priv->civ_731_mode ? 4:5)) {
				rig_debug(RIG_DEBUG_ERR,"icom_get_freq: wrong frame len=%d\n",
								freq_len);
				return -RIG_ERJCTED;
		}

		/*	
		 * from_bcd requires nibble len
		 */
		*freq = from_bcd(freqbuf+1, freq_len*2);

		return RIG_OK;
}

/*
 * icom_set_mode
 * Assumes rig!=NULL, rig->state.priv!=NULL
 */
int icom_set_mode(RIG *rig, vfo_t vfo, rmode_t mode, pbwidth_t width)
{
		struct icom_priv_data *priv;
		struct rig_state *rs;
		unsigned char ackbuf[16],icmode_ext[1];
		int ack_len, icmode, retval;

		rs = &rig->state;
		priv = (struct icom_priv_data*)rs->priv;

		icmode = rig2icom_mode(rig, mode,width);

		icmode_ext[0] = (icmode>>8) & 0xff;
		retval = icom_transaction (rig, C_SET_MODE, icmode & 0xff, icmode_ext, 
						icmode_ext[0]?1:0, ackbuf, &ack_len);
		if (retval != RIG_OK)
				return retval;

		if (ack_len != 1 || ackbuf[0] != ACK) {
				rig_debug(RIG_DEBUG_ERR,"icom_set_mode: ack NG (%#.2x), "
								"len=%d\n", ackbuf[0],ack_len);
				return -RIG_ERJCTED;
		}

		return RIG_OK;
}

/*
 * icom_get_mode
 * Assumes rig!=NULL, rig->state.priv!=NULL, mode!=NULL, width!=NULL
 *
 * TODO: some IC781's, when sending mode info, in wide filter mode, no
 * 	width data is send along, making the frame 1 byte short.
 * 	(Thank to Mel, VE2DC for this info)
 */
int icom_get_mode(RIG *rig, vfo_t vfo, rmode_t *mode, pbwidth_t *width)
{
		struct icom_priv_data *priv;
		struct rig_state *rs;
		unsigned char modebuf[16];
		int mode_len, retval;

		rs = &rig->state;
		priv = (struct icom_priv_data*)rs->priv;

		retval = icom_transaction (rig, C_RD_MODE, -1, NULL, 0, 
						modebuf, &mode_len);
		if (retval != RIG_OK)
				return retval;

		/*
		 * modebuf should contain Cn,Data area
		 */
		mode_len--;
		if (mode_len != 2 && mode_len != 1) {
				rig_debug(RIG_DEBUG_ERR,"icom_get_mode: wrong frame len=%d\n",
								mode_len);
				return -RIG_ERJCTED;
		}

		icom2rig_mode(rig, modebuf[1]| modebuf[2]<<8, mode, width);

		return RIG_OK;
}

/*
 * icom_set_vfo
 * Assumes rig!=NULL, rig->state.priv!=NULL
 */
int icom_set_vfo(RIG *rig, vfo_t vfo)
{
		struct icom_priv_data *priv;
		struct rig_state *rs;
		unsigned char ackbuf[16];
		int ack_len, icvfo, retval;

		rs = &rig->state;
		priv = (struct icom_priv_data*)rs->priv;

		switch(vfo) {
		case RIG_VFO_A: icvfo = S_VFOA; break;
		case RIG_VFO_B: icvfo = S_VFOB; break;
#ifdef RIG_VFO_VFO
		case RIG_VFO_VFO: 
			retval = icom_transaction (rig, C_SET_VFO, -1, NULL, 0, 
							ackbuf, &ack_len);
			if (retval != RIG_OK)
				return retval;
			if (ack_len != 1 || ackbuf[0] != ACK) {
				rig_debug(RIG_DEBUG_ERR,"icom_set_vfo: ack NG (%#.2x), "
								"len=%d\n", ackbuf[0],ack_len);
				return -RIG_ERJCTED;
			}
			return RIG_OK;
#endif
#ifdef RIG_VFO_MEM
		case RIG_VFO_MEM: 
			retval = icom_transaction (rig, C_SET_MEM, -1, NULL, 0, 
							ackbuf, &ack_len);
			if (retval != RIG_OK)
				return retval;
			if (ack_len != 1 || ackbuf[0] != ACK) {
				rig_debug(RIG_DEBUG_ERR,"icom_set_vfo: ack NG (%#.2x), "
								"len=%d\n", ackbuf[0],ack_len);
				return -RIG_ERJCTED;
			}
			return RIG_OK;
#endif
		default:
						rig_debug(RIG_DEBUG_ERR,"icom: Unsupported VFO %d\n",
										vfo);
						return -RIG_EINVAL;
		}
		retval = icom_transaction (rig, C_SET_VFO, icvfo, NULL, 0, 
						ackbuf, &ack_len);
		if (retval != RIG_OK)
				return retval;

		if (ack_len != 1 || ackbuf[0] != ACK) {
				rig_debug(RIG_DEBUG_ERR,"icom_set_vfo: ack NG (%#.2x), "
								"len=%d\n", ackbuf[0],ack_len);
				return -RIG_ERJCTED;
		}

		return RIG_OK;
}

/*
 * icom_set_level
 * Assumes rig!=NULL, rig->state.priv!=NULL
 */
int icom_set_level(RIG *rig, vfo_t vfo, setting_t level, value_t val)
{
		struct icom_priv_data *priv;
		struct rig_state *rs;
		unsigned char lvlbuf[16], ackbuf[16];
		int ack_len, lvl_len;
		int lvl_cn, lvl_sc;		/* Command Number, Subcommand */
		int icom_val;
		int i, retval;

		rs = &rig->state;
		priv = (struct icom_priv_data*)rs->priv;


		/*
		 * So far, levels of float type are in [0.0..1.0] range
		 */
		if (RIG_LEVEL_IS_FLOAT(level))
				icom_val = val.f * 255;
		else
				icom_val = val.i;

		/*	
		 * Most of the time, the data field is a 3 digit BCD,
		 * but in *big endian* order: 0000..0255
		 * (from_bcd is little endian)
		 */
		lvl_len = 2;
		to_bcd_be(lvlbuf, (long long)icom_val, lvl_len*2);

		/* Optimize:
		 *   sort the switch cases with the most frequent first
		 */
		switch (level) {
		case RIG_LEVEL_PREAMP:
			lvl_cn = C_CTL_FUNC;
			lvl_sc = S_FUNC_PAMP;
			lvl_len = 1;
			if (val.i == 0) {
				lvlbuf[0] = 0;	/* 0=OFF */
				break;
			}
			for (i=0; i<MAXDBLSTSIZ; i++) {
					if (rs->preamp[i] == val.i)
							break;
			}
			if (i==MAXDBLSTSIZ || rs->preamp[i] == 0) {
				rig_debug(RIG_DEBUG_ERR,"Unsupported preamp set_level %ddB", 
								val.i);
				return -RIG_EINVAL;
			}
			lvlbuf[0] = i+1;	/* 1=P.AMP1, 2=P.AMP2 */
			break;
		case RIG_LEVEL_ATT:
			lvl_cn = C_CTL_ATT;
			lvl_sc = val.i;
			lvl_len = 0;
			break;
		case RIG_LEVEL_AF:
			lvl_cn = C_CTL_LVL;
			lvl_sc = S_LVL_AF;
			break;
		case RIG_LEVEL_RF:
			lvl_cn = C_CTL_LVL;
			lvl_sc = S_LVL_RF;
			break;
		case RIG_LEVEL_SQL:
			lvl_cn = C_CTL_LVL;
			lvl_sc = S_LVL_SQL;
			break;
		case RIG_LEVEL_IF:
			lvl_cn = C_CTL_LVL;
			lvl_sc = S_LVL_IF;
			break;
		case RIG_LEVEL_APF:
			lvl_cn = C_CTL_LVL;
			lvl_sc = S_LVL_APF;
			break;
		case RIG_LEVEL_NR:
			lvl_cn = C_CTL_LVL;
			lvl_sc = S_LVL_NR;
			break;
		case RIG_LEVEL_PBT_IN:
			lvl_cn = C_CTL_LVL;
			lvl_sc = S_LVL_PBTIN;
			break;
		case RIG_LEVEL_PBT_OUT:
			lvl_cn = C_CTL_LVL;
			lvl_sc = S_LVL_PBTOUT;
			break;
		case RIG_LEVEL_CWPITCH:
			lvl_cn = C_CTL_LVL;
			lvl_sc = S_LVL_CWPITCH;
			break;
		case RIG_LEVEL_RFPOWER:
			lvl_cn = C_CTL_LVL;
			lvl_sc = S_LVL_RFPOWER;
			break;
		case RIG_LEVEL_MICGAIN:
			lvl_cn = C_CTL_LVL;
			lvl_sc = S_LVL_MICGAIN;
			break;
		case RIG_LEVEL_KEYSPD:
			lvl_cn = C_CTL_LVL;
			lvl_sc = S_LVL_KEYSPD;
			break;
		case RIG_LEVEL_NOTCHF:
			lvl_cn = C_CTL_LVL;
			lvl_sc = S_LVL_NOTCHF;
			break;
		case RIG_LEVEL_COMP:
			lvl_cn = C_CTL_LVL;
			lvl_sc = S_LVL_COMP;
			break;
		case RIG_LEVEL_AGC:
			lvl_cn = C_CTL_FUNC;
			lvl_sc = S_FUNC_AGC;
			break;
		case RIG_LEVEL_BKINDL:
			lvl_cn = C_CTL_LVL;
			lvl_sc = S_LVL_BKINDL;
			break;
		case RIG_LEVEL_BALANCE:
			lvl_cn = C_CTL_LVL;
			lvl_sc = S_LVL_BALANCE;
			break;

		default:
			rig_debug(RIG_DEBUG_ERR,"Unsupported set_level %d", level);
			return -RIG_EINVAL;
		}

		retval = icom_transaction (rig, lvl_cn, lvl_sc, lvlbuf, lvl_len, 
						ackbuf, &ack_len);
		if (retval != RIG_OK)
				return retval;

		if (ack_len != 1 || ackbuf[0] != ACK) {
				rig_debug(RIG_DEBUG_ERR,"icom_set_level: ack NG (%#.2x), "
								"len=%d\n", ackbuf[0], ack_len);
				return -RIG_ERJCTED;
		}

		return RIG_OK;
}

/*
 * icom_get_level
 * Assumes rig!=NULL, rig->state.priv!=NULL, val!=NULL
 */
int icom_get_level(RIG *rig, vfo_t vfo, setting_t level, value_t *val)
{
		struct icom_priv_data *priv;
		struct rig_state *rs;
		unsigned char lvlbuf[16];
		int lvl_len;
		int lvl_cn, lvl_sc;		/* Command Number, Subcommand */
		int icom_val;
		int cmdhead;
		int retval;

		rs = &rig->state;
		priv = (struct icom_priv_data*)rs->priv;


		/* Optimize:
		 *   sort the switch cases with the most frequent first
		 */
		switch (level) {
		case RIG_LEVEL_STRENGTH:
			lvl_cn = C_RD_SQSM;
			lvl_sc = S_SML;
			break;
		case RIG_LEVEL_SQLSTAT:
			lvl_cn = C_RD_SQSM;
			lvl_sc = S_SQL;
			break;

		case RIG_LEVEL_PREAMP:
			lvl_cn = C_CTL_FUNC;
			lvl_sc = S_FUNC_PAMP;
			break;
		case RIG_LEVEL_ATT:
			lvl_cn = C_CTL_ATT;
			lvl_sc = -1;
			break;
		case RIG_LEVEL_AF:
			lvl_cn = C_CTL_LVL;
			lvl_sc = S_LVL_AF;
			break;
		case RIG_LEVEL_RF:
			lvl_cn = C_CTL_LVL;
			lvl_sc = S_LVL_RF;
			break;
		case RIG_LEVEL_SQL:
			lvl_cn = C_CTL_LVL;
			lvl_sc = S_LVL_SQL;
			break;
		case RIG_LEVEL_IF:
			lvl_cn = C_CTL_LVL;
			lvl_sc = S_LVL_IF;
			break;
		case RIG_LEVEL_APF:
			lvl_cn = C_CTL_LVL;
			lvl_sc = S_LVL_APF;
			break;
		case RIG_LEVEL_NR:
			lvl_cn = C_CTL_LVL;
			lvl_sc = S_LVL_NR;
			break;
		case RIG_LEVEL_PBT_IN:
			lvl_cn = C_CTL_LVL;
			lvl_sc = S_LVL_PBTIN;
			break;
		case RIG_LEVEL_PBT_OUT:
			lvl_cn = C_CTL_LVL;
			lvl_sc = S_LVL_PBTOUT;
			break;
		case RIG_LEVEL_CWPITCH:
			lvl_cn = C_CTL_LVL;
			lvl_sc = S_LVL_CWPITCH;
			break;
		case RIG_LEVEL_RFPOWER:
			lvl_cn = C_CTL_LVL;
			lvl_sc = S_LVL_RFPOWER;
			break;
		case RIG_LEVEL_MICGAIN:
			lvl_cn = C_CTL_LVL;
			lvl_sc = S_LVL_MICGAIN;
			break;
		case RIG_LEVEL_KEYSPD:
			lvl_cn = C_CTL_LVL;
			lvl_sc = S_LVL_KEYSPD;
			break;
		case RIG_LEVEL_NOTCHF:
			lvl_cn = C_CTL_LVL;
			lvl_sc = S_LVL_NOTCHF;
			break;
		case RIG_LEVEL_COMP:
			lvl_cn = C_CTL_LVL;
			lvl_sc = S_LVL_COMP;
			break;
		case RIG_LEVEL_AGC:
			lvl_cn = C_CTL_FUNC;
			lvl_sc = S_FUNC_AGC;
			break;
		case RIG_LEVEL_BKINDL:
			lvl_cn = C_CTL_LVL;
			lvl_sc = S_LVL_BKINDL;
			break;
		case RIG_LEVEL_BALANCE:
			lvl_cn = C_CTL_LVL;
			lvl_sc = S_LVL_BALANCE;
			break;

		default:
			rig_debug(RIG_DEBUG_ERR,"Unsupported get_level %d", level);
			return -RIG_EINVAL;
		}

		retval = icom_transaction (rig, lvl_cn, lvl_sc, NULL, 0, 
						lvlbuf, &lvl_len);
		if (retval != RIG_OK)
				return retval;

		/*
		 * strbuf should contain Cn,Sc,Data area
		 */
		cmdhead = (lvl_sc == -1) ? 1:2;
		lvl_len -= cmdhead;

		if (lvlbuf[0] != ACK && lvlbuf[0] != lvl_cn) {
				rig_debug(RIG_DEBUG_ERR,"icom_get_level: ack NG (%#.2x), "
								"len=%d\n", lvlbuf[0],lvl_len);
				return -RIG_ERJCTED;
		}

		/*	
		 * The result is a 3 digit BCD, but in *big endian* order: 0000..0255
		 * (from_bcd is little endian)
		 */
		icom_val = from_bcd_be(lvlbuf+cmdhead, lvl_len*2);


		switch (level) {
		case RIG_LEVEL_STRENGTH:
			val->i = rig_raw2val(icom_val, &priv->str_cal);
			break;
		case RIG_LEVEL_SQLSTAT:
			/*
			 * 0x00=sql closed, 0x01=sql open
			 */ 
			val->i = icom_val;
			break;

		case RIG_LEVEL_PREAMP:
			if (icom_val == 0) {
					val->i = 0;
					break;
			}
			if (icom_val > MAXDBLSTSIZ || rs->preamp[icom_val-1]==0) {
				rig_debug(RIG_DEBUG_ERR,"Unsupported preamp get_level %ddB", 
								icom_val);
				return -RIG_EPROTO;
			}
			val->i = rs->preamp[icom_val-1];
			break;
		/* RIG_LEVEL_ATT: returned value is already an integer in dB ! */
		default:
			if (RIG_LEVEL_IS_FLOAT(level))
				val->f = (float)icom_val/255;
			else
				val->i = icom_val;
		}

		rig_debug(RIG_DEBUG_VERBOSE,"get_level: %d %d %d %f\n", lvl_len, 
						icom_val, val->i, val->f);

		return RIG_OK;
}

/*
 * icom_set_ptt
 * Assumes rig!=NULL, rig->state.priv!=NULL
 */
int icom_set_ptt(RIG *rig, vfo_t vfo, ptt_t ptt)
{
		struct icom_priv_data *priv;
		struct rig_state *rs;
		unsigned char ackbuf[16], ptt_sc;
		int ack_len, retval;

		rs = &rig->state;
		priv = (struct icom_priv_data*)rs->priv;

		ptt_sc = ptt == RIG_PTT_ON ? S_PTT_ON:S_PTT_OFF;

		retval = icom_transaction (rig, C_CTL_PTT, ptt_sc, NULL, 0, 
						ackbuf, &ack_len);
		if (retval != RIG_OK)
				return retval;

		if (ack_len != 1 || ackbuf[0] != ACK) {
				rig_debug(RIG_DEBUG_ERR,"icom_set_ptt: ack NG (%#.2x), "
								"len=%d\n", ackbuf[0],ack_len);
				return -RIG_ERJCTED;
		}

		return RIG_OK;
}

/*
 * icom_get_ptt
 * Assumes rig!=NULL, rig->state.priv!=NULL, ptt!=NULL
 */
int icom_get_ptt(RIG *rig, vfo_t vfo, ptt_t *ptt)
{
		struct icom_priv_data *priv;
		struct rig_state *rs;
		unsigned char pttbuf[16];
		int ptt_len, retval;

		rs = &rig->state;
		priv = (struct icom_priv_data*)rs->priv;

		retval = icom_transaction (rig, C_CTL_PTT, -1, NULL, 0, 
						pttbuf, &ptt_len);
		if (retval != RIG_OK)
				return retval;

		/*
		 * freqbuf should contain Cn,Data area
		 */
		ptt_len--;
		if (ptt_len != 1) {
				rig_debug(RIG_DEBUG_ERR,"icom_get_ptt: wrong frame len=%d\n",
								ptt_len);
				return -RIG_ERJCTED;
		}

		*ptt = pttbuf[1] == S_PTT_ON ? RIG_PTT_ON : RIG_PTT_OFF;

		return RIG_OK;
}

/*
 * icom_get_dcd
 * Assumes rig!=NULL, rig->state.priv!=NULL, ptt!=NULL
 */
int icom_get_dcd(RIG *rig, vfo_t vfo, dcd_t *dcd)
{
		struct icom_priv_data *priv;
		struct rig_state *rs;
		unsigned char dcdbuf[16];
		int dcd_len, retval;
		int icom_val;

		rs = &rig->state;
		priv = (struct icom_priv_data*)rs->priv;

		retval = icom_transaction (rig, C_RD_SQSM, S_SQL, NULL, 0, 
						dcdbuf, &dcd_len);
		if (retval != RIG_OK)
				return retval;

		/*
		 * freqbuf should contain Cn,Data area
		 */
		dcd_len -= 2;
		if (dcd_len != 2) {
				rig_debug(RIG_DEBUG_ERR,"icom_get_dcd: wrong frame len=%d\n",
								dcd_len);
				return -RIG_ERJCTED;
		}

		/*	
		 * The result is a 3 digit BCD, but in *big endian* order: 0000..0255
		 * (from_bcd is little endian)
		 */
		icom_val = from_bcd_be(dcdbuf+2, dcd_len*2);
			/*
			 * 0x00=sql closed, 0x01=sql open
			 *
			 * TODO: replace icom_val by dcdbuf[2] ?
			 */ 
		*dcd = (icom_val==0x01) ? RIG_DCD_ON : RIG_DCD_OFF;

		return RIG_OK;
}
/*
 * icom_set_rptr_shift
 * Assumes rig!=NULL, rig->state.priv!=NULL
 */
int icom_set_rptr_shift(RIG *rig, vfo_t vfo, rptr_shift_t rptr_shift)
{
		struct icom_priv_data *priv;
		struct rig_state *rs;
		unsigned char ackbuf[16];
		int ack_len, retval;
		int rptr_sc;

		rs = &rig->state;
		priv = (struct icom_priv_data*)rs->priv;

		switch (rptr_shift) {
		case RIG_RPT_SHIFT_NONE:
			rptr_sc = S_DUP_OFF;	/* Simplex mode */
			break;
		case RIG_RPT_SHIFT_MINUS:
			rptr_sc = S_DUP_M;		/* Duplex - mode */
			break;
		case RIG_RPT_SHIFT_PLUS:
			rptr_sc = S_DUP_P;		/* Duplex + mode */
			break;
		default:
			rig_debug(RIG_DEBUG_ERR,"Unsupported shift %d", rptr_shift);
			return -RIG_EINVAL;
		}

		retval = icom_transaction (rig, C_CTL_SPLT, rptr_sc, NULL, 0, 
						ackbuf, &ack_len);
		if (retval != RIG_OK)
				return retval;

		if (ack_len != 1 || ackbuf[0] != ACK) {
				rig_debug(RIG_DEBUG_ERR,"icom_set_rptr_shift: ack NG (%#.2x), "
								"len=%d\n", ackbuf[0],ack_len);
				return -RIG_ERJCTED;
		}

		return RIG_OK;
}


/*
 * icom_get_rptr_shift
 * Assumes rig!=NULL, rig->state.priv!=NULL, rptr_shift!=NULL
 * NOTE: seems not to work (tested on IC-706MkIIG), please report --SF
 */
int icom_get_rptr_shift(RIG *rig, vfo_t vfo, rptr_shift_t *rptr_shift)
{
		struct icom_priv_data *priv;
		struct rig_state *rs;
		unsigned char rptrbuf[16];
		int rptr_len, retval;

		rs = &rig->state;
		priv = (struct icom_priv_data*)rs->priv;

		retval = icom_transaction (rig, C_CTL_SPLT, -1, NULL, 0, 
						rptrbuf, &rptr_len);
		if (retval != RIG_OK)
				return retval;

		/*
		 * rptrbuf should contain Cn,Sc
		 */
		rptr_len--;
		if (rptr_len != 1) {
				rig_debug(RIG_DEBUG_ERR,"icom_get_rptr_shift: wrong frame len=%d\n",
								rptr_len);
				return -RIG_ERJCTED;
		}

		switch (rptrbuf[1]) {
		case S_DUP_OFF:
			*rptr_shift = RIG_RPT_SHIFT_NONE;	/* Simplex mode */
			break;
		case S_DUP_M:
			*rptr_shift = RIG_RPT_SHIFT_MINUS;		/* Duples - mode */
			break;
		case S_DUP_P:
			*rptr_shift = RIG_RPT_SHIFT_PLUS;		/* Duplex + mode */
			break;
		default:
			rig_debug(RIG_DEBUG_ERR,"Unsupported shift %d", rptrbuf[1]);
			return -RIG_EPROTO;
		}

		return RIG_OK;
}

/*
 * icom_set_rptr_offs
 * Assumes rig!=NULL, rig->state.priv!=NULL
 */
int icom_set_rptr_offs(RIG *rig, vfo_t vfo, shortfreq_t rptr_offs)
{
		struct icom_priv_data *priv;
		struct rig_state *rs;
		unsigned char offsbuf[16],ackbuf[16];
		int ack_len, retval;

		rs = &rig->state;
		priv = (struct icom_priv_data*)rs->priv;

		/*
		 * Icoms are using a 100Hz unit (at least on 706MKIIg) -- SF 
		 */
		to_bcd(offsbuf, rptr_offs/100, OFFS_LEN*2);

		retval = icom_transaction (rig, C_SET_OFFS, -1, offsbuf, OFFS_LEN, 
						ackbuf, &ack_len);
		if (retval != RIG_OK)
				return retval;

		if (ack_len != 1 || ackbuf[0] != ACK) {
				rig_debug(RIG_DEBUG_ERR,"icom_set_rptr_offs: ack NG (%#.2x), "
								"len=%d\n", ackbuf[0],ack_len);
				return -RIG_ERJCTED;
		}

		return RIG_OK;
}


/*
 * icom_get_rptr_offs
 * Assumes rig!=NULL, rig->state.priv!=NULL, rptr_offs!=NULL
 */
int icom_get_rptr_offs(RIG *rig, vfo_t vfo, shortfreq_t *rptr_offs)
{
		struct icom_priv_data *priv;
		struct rig_state *rs;
		unsigned char offsbuf[16];
		int offs_len, retval;

		rs = &rig->state;
		priv = (struct icom_priv_data*)rs->priv;

		retval = icom_transaction (rig, C_RD_OFFS, -1, NULL, 0, 
						offsbuf, &offs_len);
		if (retval != RIG_OK)
				return retval;

		/*
		 * offsbuf should contain Cn
		 */
		offs_len--;
		if (offs_len != OFFS_LEN) {
				rig_debug(RIG_DEBUG_ERR,"icom_get_rptr_offs: "
								"wrong frame len=%d\n", offs_len);
				return -RIG_ERJCTED;
		}

		/*
		 * Icoms are using a 100Hz unit (at least on 706MKIIg) -- SF 
		 */
		*rptr_offs = from_bcd(offsbuf+1, offs_len*2)*100;

		return RIG_OK;
}


/*
 * icom_set_split_freq
 * Assumes rig!=NULL, rig->state.priv!=NULL, 
 * 	icom_set_vfo,icom_set_freq works for this rig
 * FIXME: status
 */
int icom_set_split_freq(RIG *rig, vfo_t vfo, freq_t rx_freq, freq_t tx_freq)
{
		int status;

		status = icom_set_vfo(rig, RIG_VFO_B);
		status |= icom_set_freq(rig, RIG_VFO_CURR, tx_freq);
		status |= icom_set_vfo(rig, RIG_VFO_A);
		status |= icom_set_freq(rig, RIG_VFO_CURR, rx_freq);

		return status;
}

/*
 * icom_get_split_freq
 * Assumes rig!=NULL, rig->state.priv!=NULL, rx_freq!=NULL, tx_freq!=NULL
 * 	icom_set_vfo,icom_get_freq works for this rig
 * FIXME: status
 */
int icom_get_split_freq(RIG *rig, vfo_t vfo, freq_t *rx_freq, freq_t *tx_freq)
{
		int status;

		status = icom_set_vfo(rig, RIG_VFO_B);
		status |= icom_get_freq(rig, RIG_VFO_CURR, tx_freq);
		status |= icom_set_vfo(rig, RIG_VFO_A);
		status |= icom_get_freq(rig, RIG_VFO_CURR, rx_freq);

		return status;
}

/*
 * icom_set_split_mode
 * Assumes rig!=NULL, rig->state.priv!=NULL, 
 * 	icom_set_vfo,icom_set_mode works for this rig
 * FIXME: status
 */
int icom_set_split_mode(RIG *rig, vfo_t vfo, rmode_t rx_mode, pbwidth_t rx_width, rmode_t tx_mode, pbwidth_t tx_width)
{
		int status;

		status = icom_set_vfo(rig, RIG_VFO_B);
		status |= icom_set_mode(rig, RIG_VFO_CURR, tx_mode, tx_width);
		status |= icom_set_vfo(rig, RIG_VFO_A);
		status |= icom_set_mode(rig, RIG_VFO_CURR, rx_mode, rx_width);

		return status;
}

/*
 * icom_get_split_mode
 * Assumes rig!=NULL, rig->state.priv!=NULL, 
 *  rx_mode!=NULL, rx_width!=NULL, tx_mode!=NULL, tx_width!=NULL
 * 	icom_set_vfo,icom_get_mode works for this rig
 * FIXME: status
 */
int icom_get_split_mode(RIG *rig, vfo_t vfo, rmode_t *rx_mode, pbwidth_t *rx_width, rmode_t *tx_mode, pbwidth_t *tx_width)
{
		int status;

		status = icom_set_vfo(rig, RIG_VFO_B);
		status |= icom_get_mode(rig, RIG_VFO_CURR, tx_mode, tx_width);
		status |= icom_set_vfo(rig, RIG_VFO_A);
		status |= icom_get_mode(rig, RIG_VFO_CURR, rx_mode, rx_width);

		return status;
}


/*
 * icom_set_split
 * Assumes rig!=NULL, rig->state.priv!=NULL
 */
int icom_set_split(RIG *rig, vfo_t vfo, split_t split)
{
		struct icom_priv_data *priv;
		struct rig_state *rs;
		unsigned char ackbuf[16];
		int ack_len, retval;
		int split_sc;

		rs = &rig->state;
		priv = (struct icom_priv_data*)rs->priv;

		switch (split) {
		case RIG_SPLIT_OFF:
			split_sc = S_SPLT_OFF;
			break;
		case RIG_SPLIT_ON:
			split_sc = S_SPLT_ON;
			break;
		default:
			rig_debug(RIG_DEBUG_ERR,"Unsupported split %d", split);
			return -RIG_EINVAL;
		}

		retval = icom_transaction (rig, C_CTL_SPLT, split_sc, NULL, 0, 
						ackbuf, &ack_len);
		if (retval != RIG_OK)
				return retval;

		if (ack_len != 1 || ackbuf[0] != ACK) {
				rig_debug(RIG_DEBUG_ERR,"icom_set_split: ack NG (%#.2x), "
								"len=%d\n", ackbuf[0],ack_len);
				return -RIG_ERJCTED;
		}

		return RIG_OK;
}


/*
 * icom_get_split
 * Assumes rig!=NULL, rig->state.priv!=NULL, split!=NULL
 */
int icom_get_split(RIG *rig, vfo_t vfo, split_t *split)
{
		struct icom_priv_data *priv;
		struct rig_state *rs;
		unsigned char splitbuf[16];
		int split_len, retval;

		rs = &rig->state;
		priv = (struct icom_priv_data*)rs->priv;

		retval = icom_transaction (rig, C_CTL_SPLT, -1, NULL, 0, 
						splitbuf, &split_len);
		if (retval != RIG_OK)
				return retval;

		/*
		 * splitbuf should contain Cn,Sc
		 */
		split_len--;
		if (split_len != 1) {
				rig_debug(RIG_DEBUG_ERR,"icom_get_split: wrong frame len=%d\n",
								split_len);
				return -RIG_ERJCTED;
		}

		switch (splitbuf[1]) {
		case S_SPLT_OFF:
			*split = RIG_SPLIT_OFF;
			break;
		case S_SPLT_ON:
			*split = RIG_SPLIT_ON;
			break;
		default:
			rig_debug(RIG_DEBUG_ERR,"Unsupported split %d", splitbuf[1]);
			return -RIG_EPROTO;
		}

		return RIG_OK;
}

/*
 * icom_set_ts
 * Assumes rig!=NULL, rig->caps->priv!=NULL
 */
int icom_set_ts(RIG *rig, vfo_t vfo, shortfreq_t ts)
{
		const struct icom_priv_caps *priv_caps;
		unsigned char ackbuf[16];
		int i, ack_len, retval;
		int ts_sc = 0;

		priv_caps = (const struct icom_priv_caps*)rig->caps->priv;

		for (i=0; i<TSLSTSIZ; i++) {
				if (priv_caps->ts_sc_list[i].ts == ts) {
						ts_sc = priv_caps->ts_sc_list[i].sc;
						break;
				}
		}
		if (i >= TSLSTSIZ) {
				return -RIG_EINVAL;	/* not found, unsupported */
		}

		retval = icom_transaction (rig, C_SET_TS, ts_sc, NULL, 0, 
						ackbuf, &ack_len);
		if (retval != RIG_OK)
				return retval;

		if (ack_len != 1 || ackbuf[0] != ACK) {
				rig_debug(RIG_DEBUG_ERR,"icom_set_ts: ack NG (%#.2x), "
								"len=%d\n", ackbuf[0],ack_len);
				return -RIG_ERJCTED;
		}

		return RIG_OK;
}

/*
 * icom_get_ts
 * Assumes rig!=NULL, rig->caps->priv!=NULL, ts!=NULL
 * NOTE: seems not to work (tested on IC-706MkIIG), please report --SF
 */
int icom_get_ts(RIG *rig, vfo_t vfo, shortfreq_t *ts)
{
		const struct icom_priv_caps *priv_caps;
		unsigned char tsbuf[16];
		int ts_len, i, retval;

		priv_caps = (const struct icom_priv_caps*)rig->caps->priv;

		retval = icom_transaction (rig, C_SET_TS, -1, NULL, 0, tsbuf, &ts_len);
		if (retval != RIG_OK)
				return retval;

		/*
		 * tsbuf should contain Cn,Sc
		 */
		ts_len--;
		if (ts_len != 1) {
				rig_debug(RIG_DEBUG_ERR,"icom_get_ts: wrong frame len=%d\n",
								ts_len);
				return -RIG_ERJCTED;
		}

		for (i=0; i<TSLSTSIZ; i++) {
				if (priv_caps->ts_sc_list[i].sc == tsbuf[1]) {
						*ts = priv_caps->ts_sc_list[i].ts;
						break;
				}
		}
		if (i >= TSLSTSIZ) {
				return -RIG_EPROTO;	/* not found, unsupported */
		}

		return RIG_OK;
}


/*
 * icom_set_func
 * Assumes rig!=NULL, rig->state.priv!=NULL
 */
int icom_set_func(RIG *rig, vfo_t vfo, setting_t func, int status)
{
		unsigned char fctbuf[16], ackbuf[16];
		int fct_len, acklen, retval;
		int fct_cn, fct_sc;		/* Command Number, Subcommand */

		/*
		 * except for IC-R8500
		 */
		fctbuf[0] = status? 0x00:0x01;
		fct_len = rig->caps->rig_model == RIG_MODEL_ICR8500 ? 0 : 1;

		/* Optimize:
		 *   sort the switch cases with the most frequent first
		 */
		switch (func) {
		case RIG_FUNC_FAGC:
			fct_cn = C_CTL_FUNC;
			fct_sc = S_FUNC_AGC;	/* default to 0x01 super-fast */
			break;
		case RIG_FUNC_NB:
			fct_cn = C_CTL_FUNC;
			fct_sc = S_FUNC_NB;
			break;
		case RIG_FUNC_COMP:
			fct_cn = C_CTL_FUNC;
			fct_sc = S_FUNC_COMP;
			break;
		case RIG_FUNC_VOX:
			fct_cn = C_CTL_FUNC;
			fct_sc = S_FUNC_VOX;
			break;
		case RIG_FUNC_TONE:		/* repeater tone */
			fct_cn = C_CTL_FUNC;
			fct_sc = S_FUNC_TONE;
			break;
		case RIG_FUNC_TSQL:
			fct_cn = C_CTL_FUNC;
			fct_sc = S_FUNC_TSQL;
			break;
		case RIG_FUNC_SBKIN:		/* FIXME ? */
		case RIG_FUNC_FBKIN:
			fct_cn = C_CTL_FUNC;
			fct_sc = S_FUNC_BKIN;
			break;
		case RIG_FUNC_ANF:
			fct_cn = C_CTL_FUNC;
			fct_sc = S_FUNC_ANF;
			break;
		case RIG_FUNC_NR:
			fct_cn = C_CTL_FUNC;
			fct_sc = S_FUNC_NR;
			break;
		case RIG_FUNC_APF:
			fct_cn = C_CTL_FUNC;
			fct_sc = S_FUNC_APF;
			break;
		case RIG_FUNC_MON:
			fct_cn = C_CTL_FUNC;
			fct_sc = S_FUNC_MON;
			break;
		case RIG_FUNC_MN:
			fct_cn = C_CTL_FUNC;
			fct_sc = S_FUNC_MN;
			break;
		case RIG_FUNC_RNF:
			fct_cn = C_CTL_FUNC;
			fct_sc = S_FUNC_RNF;
			break;

		default:
			rig_debug(RIG_DEBUG_ERR,"Unsupported set_func %d", func);
			return -RIG_EINVAL;
		}

		retval = icom_transaction(rig, fct_cn, fct_sc, fctbuf, fct_len, 
						ackbuf, &acklen);
		if (retval != RIG_OK)
				return retval;

		if (fct_len != 2) {
				rig_debug(RIG_DEBUG_ERR,"icom_set_func: wrong frame len=%d\n",
								fct_len);
				return -RIG_EPROTO;
		}

		return RIG_OK;
}

/*
 * icom_get_func
 * Assumes rig!=NULL, rig->state.priv!=NULL
 * FIXME: IC8500 and no-sc, any support?
 */
int icom_get_func(RIG *rig, vfo_t vfo, setting_t func, int *status)
{
		unsigned char ackbuf[16];
		int ack_len, retval;
		int fct_cn, fct_sc;		/* Command Number, Subcommand */

		/* Optimize:
		 *   sort the switch cases with the most frequent first
		 */
		switch (func) {
		case RIG_FUNC_FAGC:
			fct_cn = C_CTL_FUNC;
			fct_sc = S_FUNC_AGC;	/* default to 0x01 super-fast */
			break;
		case RIG_FUNC_NB:
			fct_cn = C_CTL_FUNC;
			fct_sc = S_FUNC_NB;
			break;
		case RIG_FUNC_COMP:
			fct_cn = C_CTL_FUNC;
			fct_sc = S_FUNC_COMP;
			break;
		case RIG_FUNC_VOX:
			fct_cn = C_CTL_FUNC;
			fct_sc = S_FUNC_VOX;
			break;
		case RIG_FUNC_TONE:		/* repeater tone */
			fct_cn = C_CTL_FUNC;
			fct_sc = S_FUNC_TONE;
			break;
		case RIG_FUNC_TSQL:
			fct_cn = C_CTL_FUNC;
			fct_sc = S_FUNC_TSQL;
			break;
		case RIG_FUNC_SBKIN:		/* FIXME ? */
		case RIG_FUNC_FBKIN:
			fct_cn = C_CTL_FUNC;
			fct_sc = S_FUNC_BKIN;
			break;
		case RIG_FUNC_ANF:
			fct_cn = C_CTL_FUNC;
			fct_sc = S_FUNC_ANF;
			break;
		case RIG_FUNC_NR:
			fct_cn = C_CTL_FUNC;
			fct_sc = S_FUNC_NR;
			break;
		case RIG_FUNC_APF:
			fct_cn = C_CTL_FUNC;
			fct_sc = S_FUNC_APF;
			break;
		case RIG_FUNC_MON:
			fct_cn = C_CTL_FUNC;
			fct_sc = S_FUNC_MON;
			break;
		case RIG_FUNC_MN:
			fct_cn = C_CTL_FUNC;
			fct_sc = S_FUNC_MN;
			break;
		case RIG_FUNC_RNF:
			fct_cn = C_CTL_FUNC;
			fct_sc = S_FUNC_RNF;
			break;

		default:
			rig_debug(RIG_DEBUG_ERR,"Unsupported get_func %d", func);
			return -RIG_EINVAL;
		}

		retval = icom_transaction (rig, fct_cn, fct_sc, NULL, 0, 
						ackbuf, &ack_len);
		if (retval != RIG_OK)
				return retval;

		if (ack_len != 3) {
				rig_debug(RIG_DEBUG_ERR,"icom_get_func: wrong frame len=%d\n",
								ack_len);
				return -RIG_EPROTO;
		}

		*status = ackbuf[2];

		return RIG_OK;
}

/*
 * icom_set_ctcss
 * Assumes rig!=NULL, rig->state.priv!=NULL
 *
 * Warning! This is untested stuff! May work at least on 756PRO and IC746.
 * 	Please owners report to me <f4cfe@users.sourceforge.net>, thanks. --SF
 */
int icom_set_ctcss(RIG *rig, vfo_t vfo, unsigned int tone)
{
		const struct rig_caps *caps;
		unsigned char tonebuf[16], ackbuf[16];
		int tone_len, ack_len, retval;
		int i;

		caps = rig->caps;

		/*
		 * I don't have documentation for this function,
		 * and I can't experiment (no hardware), so let's guess.
		 * Most probably, it might be the index of the CTCSS subaudible
		 * tone, and not the tone itself, starting from zero. 
		 *
		 * Something in the range of 00..51, BCD big endian
		 * Please someone let me know if it works this way. --SF
		 */
		for (i = 0; caps->ctcss_list[i] != 0 && i<200; i++) {
				if (caps->ctcss_list[i] == tone)
						break;
		}
		if (caps->ctcss_list[i] != tone)
				return -RIG_EINVAL;

		tone_len = 1;
		to_bcd_be(tonebuf, (long long)i, tone_len*2);

		retval = icom_transaction(rig, C_SET_TONE, S_TONE_RPTR, 
						tonebuf, tone_len, ackbuf, &ack_len);
		if (retval != RIG_OK)
				return retval;

		if (ack_len != 1 || ackbuf[0] != ACK) {
				rig_debug(RIG_DEBUG_ERR,"icom_set_ctcss: ack NG (%#.2x), "
								"len=%d\n", ackbuf[0], ack_len);
				return -RIG_ERJCTED;
		}

		return RIG_OK;
}

/*
 * icom_get_ctcss
 * Assumes rig!=NULL, rig->state.priv!=NULL
 */
int icom_get_ctcss(RIG *rig, vfo_t vfo, unsigned int *tone)
{
		const struct rig_caps *caps;
		unsigned char tonebuf[16];
		int tone_len, tone_idx, retval;
		int i;

		caps = rig->caps;

		/*
		 * see icom_set_ctcss for discussion on the untested status!
		 */

		retval = icom_transaction(rig, C_SET_TONE, S_TONE_RPTR, NULL, 0, 
												tonebuf, &tone_len);
		if (retval != RIG_OK)
				return retval;

		if (tone_len != 3) {
				rig_debug(RIG_DEBUG_ERR,"icom_get_ctcss: ack NG (%#.2x), "
								"len=%d\n", tonebuf[0], tone_len);
				return -RIG_ERJCTED;
		}

		tone_len -= 2;
		tone_idx = from_bcd_be(tonebuf, tone_len*2);

		/* check this tone exists. That's better than nothing. */
		for (i = 0; i<=tone_idx; i++) {
				if (caps->ctcss_list[i] == 0) {
						rig_debug(RIG_DEBUG_ERR,"icom_get_ctcss: CTCSS NG "
								"(%#.2x)\n", tonebuf[2]);
						return -RIG_EPROTO;
				}
		}
		*tone = caps->ctcss_list[tone_idx];

		return RIG_OK;
}

/*
 * icom_set_ctcss_sql
 * Assumes rig!=NULL, rig->state.priv!=NULL
 *
 * Warning! This is untested stuff! May work at least on 756PRO and IC746.
 * 	Please owners report to me <f4cfe@users.sourceforge.net>, thanks. --SF
 */
int icom_set_ctcss_sql(RIG *rig, vfo_t vfo, unsigned int tone)
{
		const struct rig_caps *caps;
		unsigned char tonebuf[16], ackbuf[16];
		int tone_len, ack_len, retval;
		int i;

		caps = rig->caps;

		/*
		 * see icom_set_ctcss for discussion on the untested status!
		 */

		for (i = 0; caps->ctcss_list[i] != 0 && i<200; i++) {
				if (caps->ctcss_list[i] == tone)
						break;
		}
		if (caps->ctcss_list[i] != tone)
				return -RIG_EINVAL;

		tone_len = 1;
		to_bcd_be(tonebuf, (long long)i, tone_len*2);

		retval = icom_transaction(rig, C_SET_TONE, S_TONE_SQL, 
						tonebuf, tone_len, ackbuf, &ack_len);
		if (retval != RIG_OK)
				return retval;

		if (ack_len != 1 || ackbuf[0] != ACK) {
				rig_debug(RIG_DEBUG_ERR,"icom_set_ctcss_sql: ack NG (%#.2x), "
								"len=%d\n", ackbuf[0], ack_len);
				return -RIG_ERJCTED;
		}

		return RIG_OK;
}

/*
 * icom_get_ctcss_sql
 * Assumes rig!=NULL, rig->state.priv!=NULL
 */
int icom_get_ctcss_sql(RIG *rig, vfo_t vfo, unsigned int *tone)
{
		const struct rig_caps *caps;
		unsigned char tonebuf[16];
		int tone_len, tone_idx, retval;
		int i;

		caps = rig->caps;

		/*
		 * see icom_set_ctcss for discussion on the untested status!
		 */

		retval = icom_transaction(rig, C_SET_TONE, S_TONE_SQL, NULL, 0, 
												tonebuf, &tone_len);
		if (retval != RIG_OK)
				return retval;

		if (tone_len != 3) {
				rig_debug(RIG_DEBUG_ERR,"icom_get_ctcss_sql: ack NG (%#.2x), "
								"len=%d\n", tonebuf[0], tone_len);
				return -RIG_ERJCTED;
		}

		tone_len -= 2;
		tone_idx = from_bcd_be(tonebuf, tone_len*2);

		/* check this tone exists. That's better than nothing. */
		for (i = 0; i<=tone_idx; i++) {
				if (caps->ctcss_list[i] == 0) {
						rig_debug(RIG_DEBUG_ERR,"icom_get_ctcss_sql: CTCSS NG "
								"(%#.2x)\n", tonebuf[2]);
						return -RIG_EPROTO;
				}
		}
		*tone = caps->ctcss_list[tone_idx];

		return RIG_OK;
}

/*
 * icom_set_channel
 * Assumes rig!=NULL, rig->state.priv!=NULL, chan!=NULL
 * TODO: still a WIP --SF
 */
int icom_set_channel(RIG *rig, const channel_t *chan)
{
		struct icom_priv_data *priv;
		struct rig_state *rs;
		unsigned char chanbuf[24], ackbuf[16];
		int chan_len, freq_len, ack_len, retval;
		int icmode;

		rs = &rig->state;
		priv = (struct icom_priv_data*)rs->priv;

		to_bcd_be(chanbuf,chan->channel_num,4);

		chanbuf[2] = S_MEM_CNTNT_SLCT;

		freq_len = priv->civ_731_mode ? 4:5;
		/*	
		 * to_bcd requires nibble len
		 */
		to_bcd(chanbuf+3, chan->freq, freq_len*2);

		chan_len = 3+freq_len+1;

		icmode = rig2icom_mode(rig, chan->mode, RIG_PASSBAND_NORMAL);/* FIXME */
		chanbuf[chan_len++] = icmode&0xff;
		chanbuf[chan_len++] = icmode>>8;

		to_bcd_be(chanbuf+chan_len++,
						chan->levels[rig_setting2idx(RIG_LEVEL_ATT)].i, 2);
		to_bcd_be(chanbuf+chan_len++,
						chan->levels[rig_setting2idx(RIG_LEVEL_PREAMP)].i, 2);

		to_bcd_be(chanbuf+chan_len++,chan->ant,2);
		memset(chanbuf+chan_len, 0, 8);
		strncpy(chanbuf+chan_len, chan->channel_desc, 8);
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
 * icom_get_channel
 * Assumes rig!=NULL, rig->state.priv!=NULL, chan!=NULL
 * TODO: still a WIP --SF
 */
int icom_get_channel(RIG *rig, channel_t *chan)
{
		struct icom_priv_data *priv;
		struct rig_state *rs;
		unsigned char chanbuf[24];
		int chan_len, freq_len, retval;
		pbwidth_t width; 	/* FIXME */

		rs = &rig->state;
		priv = (struct icom_priv_data*)rs->priv;

		to_bcd_be(chanbuf,chan->channel_num,4);
		chan_len = 2;

		freq_len = priv->civ_731_mode ? 4:5;

		retval = icom_transaction (rig, C_CTL_MEM, S_MEM_CNTNT, 
						chanbuf, chan_len, chanbuf, &chan_len);
		if (retval != RIG_OK)
				return retval;

		/*
		 * freqbuf should contain Cn,Data area
		 */
		chan_len--;
		if (freq_len != freq_len+16) {
				rig_debug(RIG_DEBUG_ERR,"icom_get_channel: wrong frame len=%d\n",
								chan_len);
				return -RIG_ERJCTED;
		}

		/*	
		 * from_bcd requires nibble len
		 */
		chan->freq = from_bcd(chanbuf+4, freq_len*2);

		chan_len = 4+freq_len+1;

		icom2rig_mode(rig, chanbuf[chan_len] | chanbuf[chan_len+1], 
						&chan->mode, &width);
		chan_len += 2;
		chan->levels[rig_setting2idx(RIG_LEVEL_ATT)].i = 
				from_bcd_be(chanbuf+chan_len++,2);
		chan->levels[rig_setting2idx(RIG_LEVEL_PREAMP)].i = 
				from_bcd_be(chanbuf+chan_len++,2);
		chan->ant = from_bcd_be(chanbuf+chan_len++,2);
		strncpy(chan->channel_desc, chanbuf+chan_len, 8);

		return RIG_OK;
}


/*
 * icom_set_powerstat
 * Assumes rig!=NULL, rig->state.priv!=NULL
 */
int icom_set_powerstat(RIG *rig, powerstat_t status)
{
		unsigned char ackbuf[16];
		int ack_len, retval;
		int pwr_sc;

		pwr_sc = status==RIG_POWER_ON ? S_PWR_ON:S_PWR_OFF;

		retval = icom_transaction(rig, C_SET_PWR, pwr_sc, NULL, 0, 
						ackbuf, &ack_len);
		if (retval != RIG_OK)
				return retval;

		if (ack_len != 1 || ackbuf[0] != ACK) {
				rig_debug(RIG_DEBUG_ERR,"icom_set_powerstat: ack NG (%#.2x), "
								"len=%d\n", ackbuf[0],ack_len);
				return -RIG_ERJCTED;
		}

		return RIG_OK;
}

/*
 * icom_get_powerstat
 * Assumes rig!=NULL, rig->state.priv!=NULL
 */
int icom_get_powerstat(RIG *rig, powerstat_t *status)
{
		unsigned char ackbuf[16];
		int ack_len, retval;

		retval = icom_transaction(rig, C_SET_PWR, -1, NULL, 0, 
						ackbuf, &ack_len);
		if (retval != RIG_OK)
				return retval;

		if (ack_len != 1 || ackbuf[0] != ACK) {
				rig_debug(RIG_DEBUG_ERR,"icom_get_powerstat: ack NG (%#.2x), "
								"len=%d\n", ackbuf[0],ack_len);
				return -RIG_ERJCTED;
		}
		*status = ackbuf[1] == S_PWR_ON ? RIG_POWER_ON : RIG_POWER_OFF;

		return RIG_OK;
}


/*
 * icom_set_mem
 * Assumes rig!=NULL, rig->state.priv!=NULL
 */
int icom_set_mem(RIG *rig, vfo_t vfo, int ch)
{
		struct icom_priv_data *priv;
		struct rig_state *rs;
		unsigned char membuf[2];
		unsigned char ackbuf[16];
		int ack_len, retval;

		rs = &rig->state;
		priv = (struct icom_priv_data*)rs->priv;

		to_bcd_be(membuf, ch, CHAN_NB_LEN*2);
		retval = icom_transaction (rig, C_SET_MEM, -1, membuf, CHAN_NB_LEN, 
						ackbuf, &ack_len);
		if (retval != RIG_OK)
				return retval;

		if (ack_len != 1 || ackbuf[0] != ACK) {
				rig_debug(RIG_DEBUG_ERR,"icom_set_mem: ack NG (%#.2x), "
								"len=%d\n", ackbuf[0], ack_len);
				return -RIG_ERJCTED;
		}

		return RIG_OK;
}

/*
 * icom_set_bank
 * Assumes rig!=NULL, rig->state.priv!=NULL
 */
int icom_set_bank(RIG *rig, vfo_t vfo, int bank)
{
		struct icom_priv_data *priv;
		struct rig_state *rs;
		unsigned char bankbuf[2];
		unsigned char ackbuf[16];
		int ack_len, retval;

		rs = &rig->state;
		priv = (struct icom_priv_data*)rs->priv;

		to_bcd_be(bankbuf, bank, BANK_NB_LEN*2);
		retval = icom_transaction (rig, C_SET_MEM, S_BANK, 
						bankbuf, CHAN_NB_LEN, ackbuf, &ack_len);
		if (retval != RIG_OK)
				return retval;

		if (ack_len != 1 || ackbuf[0] != ACK) {
				rig_debug(RIG_DEBUG_ERR,"icom_set_bank: ack NG (%#.2x), "
								"len=%d\n", ackbuf[0], ack_len);
				return -RIG_ERJCTED;
		}

		return RIG_OK;
}

#ifdef WANT_OLD_VFO_TO_BE_REMOVED
/*
 * icom_mv_ctl, Mem/VFO operation
 * Assumes rig!=NULL, rig->state.priv!=NULL
 */
int icom_mv_ctl(RIG *rig, vfo_t vfo, mv_op_t op)
{
		struct icom_priv_data *priv;
		struct rig_state *rs;
		unsigned char mvbuf[16];
		unsigned char ackbuf[16];
		int mv_len, ack_len;
		int mv_cn, mv_sc;

		rs = &rig->state;
		priv = (struct icom_priv_data*)rs->priv;

		mv_len = 0;

		switch(op) {
			case RIG_MVOP_VFO_MODE:
				mv_cn = C_SET_VFO;
				mv_sc = -1;
				break;
			case RIG_MVOP_MEM_MODE:
				mv_cn = C_SET_MEM;
				mv_sc = -1;
				break;
			case RIG_MVOP_VFO_CPY:
				mv_cn = C_SET_VFO;
				mv_sc = S_BTOA;
				break;
			case RIG_MVOP_VFO_XCHG:
				mv_cn = C_SET_VFO;
				mv_sc = S_XCHNG;
				break;
			case RIG_MVOP_DUAL_OFF:
				mv_cn = C_SET_VFO;
				mv_sc = S_DUAL_OFF;
				break;
			case RIG_MVOP_DUAL_ON:
				mv_cn = C_SET_VFO;
				mv_sc = S_DUAL_ON;
				break;
			case RIG_MVOP_FROM_VFO:
				mv_cn = C_WR_MEM;
				mv_sc = -1;
				break;
			case RIG_MVOP_TO_VFO:
				mv_cn = C_MEM2VFO;
				mv_sc = -1;
				break;
			case RIG_MVOP_MCL:
				mv_cn = C_CLR_MEM;
				mv_sc = -1;
				break;
			default:
				rig_debug(RIG_DEBUG_ERR,"Unsupported mem/vfo op %d", op);
				return -RIG_EINVAL;
		}

		retval = icom_transaction (rig, mv_cn, mv_sc, mvbuf, mv_len, 
						ackbuf, &ack_len);
		if (retval != RIG_OK)
				return retval;

		if (ack_len != 1 || ackbuf[0] != ACK) {
				rig_debug(RIG_DEBUG_ERR,"icom_mv_ctl: ack NG (%#.2x), "
								"len=%d\n", ackbuf[0], ack_len);
				return -RIG_ERJCTED;
		}

		return RIG_OK;
}

#else
/*
 * icom_vfo_op, Mem/VFO operation
 * Assumes rig!=NULL, rig->state.priv!=NULL
 */
int icom_vfo_op(RIG *rig, vfo_t vfo, vfo_op_t op)
{
		struct icom_priv_data *priv;
		struct rig_state *rs;
		unsigned char mvbuf[16];
		unsigned char ackbuf[16];
		int mv_len, ack_len, retval;
		int mv_cn, mv_sc;

		rs = &rig->state;
		priv = (struct icom_priv_data*)rs->priv;

		mv_len = 0;

		switch(op) {
#if 0
			case RIG_MVOP_VFO_MODE:
				mv_cn = C_SET_VFO;
				mv_sc = -1;
				break;
			case RIG_MVOP_MEM_MODE:
				mv_cn = C_SET_MEM;
				mv_sc = -1;
				break;
#endif
			case RIG_OP_CPY:
				mv_cn = C_SET_VFO;
				mv_sc = S_BTOA;
				break;
			case RIG_OP_XCHG:
				mv_cn = C_SET_VFO;
				mv_sc = S_XCHNG;
				break;
#if 0
			case RIG_OP_DUAL_OFF:
				mv_cn = C_SET_VFO;
				mv_sc = S_DUAL_OFF;
				break;
			case RIG_OP_DUAL_ON:
				mv_cn = C_SET_VFO;
				mv_sc = S_DUAL_ON;
				break;
#endif
			case RIG_OP_FROM_VFO:
				mv_cn = C_WR_MEM;
				mv_sc = -1;
				break;
			case RIG_OP_TO_VFO:
				mv_cn = C_MEM2VFO;
				mv_sc = -1;
				break;
			case RIG_OP_MCL:
				mv_cn = C_CLR_MEM;
				mv_sc = -1;
				break;
			default:
				rig_debug(RIG_DEBUG_ERR,"Unsupported mem/vfo op %#x", op);
				return -RIG_EINVAL;
		}

		retval = icom_transaction (rig, mv_cn, mv_sc, mvbuf, mv_len, 
						ackbuf, &ack_len);
		if (retval != RIG_OK)
				return retval;

		if (ack_len != 1 || ackbuf[0] != ACK) {
				rig_debug(RIG_DEBUG_ERR,"icom_vfo_op: ack NG (%#.2x), "
								"len=%d\n", ackbuf[0], ack_len);
				return -RIG_ERJCTED;
		}

		return RIG_OK;
}
#endif

/*
 * icom_decode is called by sa_sigio, when some asynchronous
 * data has been received from the rig
 */
int icom_decode_event(RIG *rig)
{
		struct icom_priv_data *priv;
		struct rig_state *rs;
		unsigned char buf[32];
		int frm_len;
		freq_t freq;
		rmode_t mode;
		pbwidth_t width; 

		rig_debug(RIG_DEBUG_VERBOSE, "icom: icom_decode called\n");

		rs = &rig->state;
		priv = (struct icom_priv_data*)rs->priv;

		frm_len = read_icom_frame(&rs->rigport, buf);
		if (frm_len < 0)
				return frm_len;

		if (buf[3] != BCASTID && buf[3] != priv->re_civ_addr) {
			rig_debug(RIG_DEBUG_WARN, "icom_decode: CI-V %#x called for %#x!\n",
							priv->re_civ_addr, buf[3]);
		}

		/*
		 * the first 2 bytes must be 0xfe
		 * the 3rd one the emitter
		 * the 4rd one 0x00 since this is transceive mode
		 * then the command number
		 * the rest is data
		 * and don't forget one byte at the end for the EOM
		 */
		switch (buf[4]) {
		case C_SND_FREQ:
				/*
				 * TODO: the freq length might be less than 4 or 5 bytes
				 * 			on older rigs!
				 */
				if (rig->callbacks.freq_event) {
					freq = from_bcd(buf+5, (priv->civ_731_mode ? 4:5)*2);
					return rig->callbacks.freq_event(rig,RIG_VFO_CURR,freq);
				} else
						return -RIG_ENAVAIL;
				break;
		case C_SND_MODE:
				if (rig->callbacks.mode_event) {
					icom2rig_mode(rig, buf[5]| buf[6]<<8, &mode, &width);
					return rig->callbacks.mode_event(rig,RIG_VFO_CURR,mode,width);
				} else
						return -RIG_ENAVAIL;
				break;
		default:
				rig_debug(RIG_DEBUG_VERBOSE,"icom_decode: tranceive cmd "
									"unsupported %#2.2x\n",buf[4]);
				return -RIG_ENIMPL;
		}

		return RIG_OK;
}

/*
 * init_icom is called by rig_probe_all (register.c)
 *
 * TODO: probe_icom will only report the _first_ device on the CI-V bus.
 * 		more devices could be found on the bus.
 */
rig_model_t probe_icom(port_t *p)
{
		unsigned char buf[16], civ_addr, civ_id;
		int frm_len, i;
		int retval;

		if (!p)
				return RIG_MODEL_NONE;

		p->write_delay = p->post_write_delay = 0;
		p->timeout = 50;
		p->retry = 1;

		retval = serial_open(p);
		if (retval != RIG_OK)
				return RIG_MODEL_NONE;

		/* try all possible addresses on the CI-V bus */
		for (civ_addr=0x01; civ_addr<=0x7f; civ_addr++) {

			frm_len = make_cmd_frame(buf, civ_addr, C_RD_TRXID, S_RD_TRXID, 
							NULL, 0);

			write_block(p, buf, frm_len);

			/* read out the bytes we just sent 
		 	* TODO: check this is what we expect 
		 	*/
			frm_len = read_icom_block(p, buf, frm_len);

			/* this is the reply */
			frm_len = read_icom_frame(p, buf);

			/* timeout.. nobody's there */
			if (frm_len <= 0)
					continue;

			if (buf[7] != FI && buf[5] != FI) {
					/* protocol error, unexpected reply.
					 * is this a CI-V device? 
					 */
					close(p->fd);
					return RIG_MODEL_NONE;
			} else if (buf[4] == NAK) {
					/* 
				 	* this is an Icom, but it does not support transceiver ID
				 	* try to guess from the return address
				 	*/
					civ_id = buf[3];
			} else {
					civ_id = buf[6];
			}

			for (i=0; icom_addr_list[i].model != RIG_MODEL_NONE; i++) {
					if (icom_addr_list[i].re_civ_addr == civ_id) {
							close(p->fd);
							rig_debug(RIG_DEBUG_VERBOSE,"probe_icom: found %#x"
											" at %#x\n", civ_id, buf[3]);
							return icom_addr_list[i].model;
					}
			}
			/*
			 * not found in known table.... 
			 * update icom_addr_list[]!
			 */
			rig_debug(RIG_DEBUG_WARN,"probe_icom: found unknown device "
							"with CI-V ID %#x, please report to Hamlib "
							"developers.\n", civ_id);
		}


		close(p->fd);
		return RIG_MODEL_NONE;
}

/*
 * init_icom is called by rig_backend_load
 */
int init_icom(void *be_handle)
{
		rig_debug(RIG_DEBUG_VERBOSE, "icom: _init called\n");

		rig_register(&ic706_caps);
		rig_register(&ic706mkii_caps);
		rig_register(&ic706mkiig_caps);

		rig_register(&icr8500_caps);

		rig_register(&icall_caps);

		return RIG_OK;
}

