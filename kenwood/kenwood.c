/*
 *  Hamlib Kenwood backend - main file
 *  Copyright (c) 2000-2003 by Stephane Fillod and others
 *
 *	$Id: kenwood.c,v 1.58 2003-03-10 08:26:09 fillods Exp $
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

#include <hamlib/rig.h>
#include <serial.h>
#include <misc.h>

#include "kenwood.h"

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
 * send the value to <fillods@users.sourceforge.net> for inclusion. Thanks --SF
 *
 * TODO: sort this list with most frequent rigs first.
 */
static const struct kenwood_id kenwood_id_list[] = {
	{ RIG_MODEL_TS940, 1 },
	{ RIG_MODEL_TS811, 2 },
	{ RIG_MODEL_TS711, 3 },
	{ RIG_MODEL_TS440, 4 },
	{ RIG_MODEL_R5000, 5 },
	{ RIG_MODEL_TS790, 7 },
	{ RIG_MODEL_TS450S, 10 },
	{ RIG_MODEL_TS690S, 11 },
	{ RIG_MODEL_TS870S, 15 },
	{ RIG_MODEL_TS570D, 17 },	/* Elecraft K2 also returns 17 */
	{ RIG_MODEL_TS570S, 18 },
	{ RIG_MODEL_TS2000, 19 },
	{ RIG_MODEL_NONE, UNKNOWN_ID },	/* end marker */
};

static const struct kenwood_id_string kenwood_id_string_list[] = {
	{ RIG_MODEL_THD7A,  "TH-D7" },
	{ RIG_MODEL_THD7AG, "TH-D7G" },
	{ RIG_MODEL_THF6A, "TH-F6" },
	{ RIG_MODEL_THF7E, "TH-F7" },
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

#define cmd_trm(rig) ((struct kenwood_priv_caps *)(rig)->caps->priv)->cmdtrm

/**
 * kenwood_transaction
 * Assumes rig!=NULL rig->state!=NULL rig->caps!=NULL
 *
 * cmdstr - Command to be sent to the rig. Cmdstr can also be NULL, indicating
 *          that only a reply is needed (nothing will be send).
 * data - Buffer for reply string.  Can be NULL, indicating that no reply is
 *        is needed and will return with RIG_OK after command was sent.
 * datasize - in: Size of buffer. It is the caller's responsibily to provide
 *            a large enough buffer for all possible replies for a command.
 *            out: location where to store number of bytes read.
 *
 * returns:
 *   RIG_OK  -  if no error occured.
 *   RIG_EIO  -  if an I/O error occured while sending/receiving data.
 *   RIG_ETIMEOUT  -  if timeout expires without any characters received.
 *   RIG_REJECTED  -  if a negative acknowledge was received or command not
 *                    recognized by rig.
 */
int
kenwood_transaction (RIG *rig, const char *cmdstr, int cmd_len,
				char *data, size_t *datasize)
{
    struct rig_state *rs;
    int retval;
    const char *cmdtrm = EOM_KEN;  /* Default Command/Reply termination char */
    int retry_read = 0;
#define MAX_RETRY_READ  32

    rs = &rig->state;
    rs->hold_decode = 1;

	serial_flush(&rs->rigport);

    cmdtrm = cmd_trm(rig);

    if (cmdstr) {
        retval = write_block(&rs->rigport, cmdstr, strlen(cmdstr));
        if (retval != RIG_OK)
            goto transaction_quit;
#ifdef TH_ADD_CMDTRM
        retval = write_block(&rs->rigport, cmdtrm, strlen(cmdtrm));
        if (retval != RIG_OK)
            goto transaction_quit;
#endif
    }

    if (data == NULL || *datasize <= 0) {
        rig->state.hold_decode = 0;
        return RIG_OK;  /* don't want a reply */
    }

transaction_read:
    retval = read_string(&rs->rigport, data, *datasize, cmdtrm, strlen(cmdtrm));
    if (retval < 0)
        goto transaction_quit;
	else
			*datasize = retval;

    /* Check that command termination is correct */
    if (!strchr(cmdtrm, data[strlen(data)])) {
        if (retry_read++ < MAX_RETRY_READ)
            goto transaction_read;
        rig_debug(RIG_DEBUG_ERR, "%s: Command is not correctly terminated '%s'\n", __FUNCTION__, data);
        retval = -RIG_EPROTO;
        goto transaction_quit;
    }

    /* Command recognised by rig but invalid data entered. */
    if (strlen(data) == 2 && data[0] == 'N') {
        rig_debug(RIG_DEBUG_ERR, "%s: NegAck for '%s'\n", __FUNCTION__, cmdstr);
        retval = -RIG_ERJCTED;
        goto transaction_quit;
    }

    /* Command not understood by rig */
    if (strlen(data) == 2 && data[0] == '?') {
        rig_debug(RIG_DEBUG_ERR, "%s: Unknown command '%s'\n", __FUNCTION__, cmdstr);
        retval = -RIG_ERJCTED;
        goto transaction_quit;
    }

#define CONFIG_STRIP_CMDTRM 1
#ifdef CONFIG_STRIP_CMDTRM
	if (strlen(data) > 0)
    	data[strlen(data)-1] = '\0';  /* not very elegant, but should work. */
	else
    	data[0] = '\0';
#endif
    /*
     * Check that received the correct reply. The first two characters
     * should be the same as command.
     */
    if (cmdstr && (data[0] != cmdstr[0] || data[1] != cmdstr[1])) {
         /*
          * TODO: When RIG_TRN is enabled, we can pass the string
          *       to the decoder for callback. That way we don't ignore
          *       any commands.
          */
        if (retry_read++ < MAX_RETRY_READ)
            goto transaction_read;
        rig_debug(RIG_DEBUG_ERR, "%s: Unexpected reply '%s'\n", __FUNCTION__, data);
        retval =  -RIG_EPROTO;
        goto transaction_quit;
    }

    retval = RIG_OK;
transaction_quit:
    rs->hold_decode = 0;
    return retval;
}


/*
 * kenwood_set_vfo
 * Assumes rig!=NULL
 */
int kenwood_set_vfo(RIG *rig, vfo_t vfo)
{
		unsigned char cmdbuf[16], ackbuf[16];
		int cmd_len, ack_len, retval;
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

		cmd_len = sprintf(cmdbuf, "FR%c%s", vfo_function, cmd_trm(rig));

		/* set RX VFO */
		ack_len = 0;
		retval = kenwood_transaction (rig, cmdbuf, cmd_len, ackbuf, &ack_len);
		if (retval != RIG_OK)
			return retval;

		/* set TX VFO */
		cmdbuf[1] = 'T';
		ack_len = 0;
		retval = kenwood_transaction (rig, cmdbuf, 4, ackbuf, &ack_len);

		return retval;
}


/*
 * kenwood_get_vfo
 * Assumes rig!=NULL, !vfo
 */
int kenwood_get_vfo(RIG *rig, vfo_t *vfo)
{
		unsigned char vfobuf[50];
		int vfo_len, retval;


		/* query RX VFO */
		vfo_len = 50;
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
 * kenwood_old_set_vfo
 * Assumes rig!=NULL
 * for TS-940, TS-811, TS-711 and TS-440
 */
int kenwood_old_set_vfo(RIG *rig, vfo_t vfo)
{
		unsigned char cmdbuf[16], ackbuf[16];
		int cmd_len, ack_len, retval;
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

		cmd_len = sprintf(cmdbuf, "FN%c%s", vfo_function, cmd_trm(rig));

		ack_len = 0;
		retval = kenwood_transaction (rig, cmdbuf, cmd_len, ackbuf, &ack_len);
		return retval;
}

/*
 * kenwood_old_get_vfo
 * Assumes rig!=NULL, !vfo
 * for TS-940, TS-811, TS-711 and TS-440
 */
int kenwood_old_get_vfo(RIG *rig, vfo_t *vfo)
{
		unsigned char vfobuf[50];
		int vfo_len, retval;


		/* query RX VFO */
		vfo_len = 50;
		retval = kenwood_transaction (rig, "FN;", 3, vfobuf, &vfo_len);
		if (retval != RIG_OK)
			return retval;

		if (vfo_len != 4 || vfobuf[1] != 'N') {
			rig_debug(RIG_DEBUG_ERR,"%s: unexpected answer %s, "
				"len=%d\n", __FUNCTION__, vfobuf, vfo_len);
			return -RIG_ERJCTED;
		}

		/* TODO: replace 0,1,2,.. constants by defines */
		switch (vfobuf[2]) {
		case '0': *vfo = RIG_VFO_A; break;
		case '1': *vfo = RIG_VFO_B; break;
		case '2': *vfo = RIG_VFO_MEM; break;
		default: 
			rig_debug(RIG_DEBUG_ERR,"%s: unsupported VFO %c\n",
						__FUNCTION__, vfobuf[2]);
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

		ack_len = 0;
		retval = kenwood_transaction (rig, freqbuf, freq_len, ackbuf, &ack_len);

		return retval;
}

/*
 * kenwood_get_freq
 * Assumes rig!=NULL, freq!=NULL
 */
int kenwood_get_freq(RIG *rig, vfo_t vfo, freq_t *freq)
{
		unsigned char freqbuf[50];
		unsigned char cmdbuf[4];
		int cmd_len, freq_len, retval;
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
			rig_debug(RIG_DEBUG_ERR,"kenwood_get_freq: unsupported VFO %d\n",
								vfo);
			return -RIG_EINVAL;
		}

		cmd_len = sprintf(cmdbuf, "F%c%s", vfo_letter, cmd_trm(rig));

		freq_len = 50;
		retval = kenwood_transaction (rig, cmdbuf, cmd_len, freqbuf, &freq_len);
		if (retval != RIG_OK)
			return retval;

		if (freq_len != 14 || freqbuf[0] != 'F') {
			rig_debug(RIG_DEBUG_ERR,"kenwood_get_freq: unexpected answer %s, "
							"len=%d\n", freqbuf, freq_len);
			return -RIG_ERJCTED;
		}

		sscanf(freqbuf+2, "%lld", freq);

		return RIG_OK;
}

int kenwood_get_rit(RIG *rig, vfo_t vfo, shortfreq_t * rit)
{
		unsigned char infobuf[50];
		int info_len, retval;

                info_len = 50;
		retval = kenwood_transaction (rig, "IF;", 3, infobuf, &info_len);
		if (retval != RIG_OK)
			return retval;

		if (info_len != 38 || infobuf[1] != 'F') {
		rig_debug(RIG_DEBUG_ERR,"kenwood_get_rit: wrong answer len=%d\n",
						info_len);
		return -RIG_ERJCTED;
		}

		infobuf[23] = '\0';
		*rit = atoi(&infobuf[18]);

		return RIG_OK;
}

/*
 * rit can only move up/down by 10 Hz, so we use a loop...
 */
int kenwood_set_rit(RIG * rig, vfo_t vfo, shortfreq_t rit)
{
        unsigned char buf[50], infobuf[50], c;
        int retval, info_len, len, i;

	
	info_len = 0;
        if (rit == 0)
        return kenwood_transaction(rig, "RC;", 3, infobuf, &info_len);

        if (rit > 0)
                c = 'U';
        else
                c = 'D';
        len = sprintf(buf, "R%c;", c);

	info_len = 0;
	retval = kenwood_transaction(rig, "RC;", 3, infobuf, &info_len);
	for (i = 0; i < abs(rint(rit/10)); i++)
	{
		info_len = 0;
		retval = kenwood_transaction(rig, buf, len, infobuf, &info_len);
	}

	return RIG_OK;
}

/* 
 * rit and xit are the same
 */
int kenwood_get_xit(RIG * rig, vfo_t vfo, shortfreq_t * rit)
{
        return kenwood_get_rit(rig, vfo, rit);
}

int kenwood_set_xit(RIG * rig, vfo_t vfo, shortfreq_t rit)
{
        return kenwood_set_rit(rig, vfo, rit);
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
			case RIG_MODE_CWR:      kmode = MD_CWR; break;
			case RIG_MODE_USB:      kmode = MD_USB; break;
			case RIG_MODE_LSB:      kmode = MD_LSB; break;
			case RIG_MODE_FM:       kmode = MD_FM; break;
			case RIG_MODE_AM:       kmode = MD_AM; break;
			case RIG_MODE_RTTY:     kmode = MD_FSK; break;
			case RIG_MODE_RTTYR:    kmode = MD_FSKR; break;
			default:
				rig_debug(RIG_DEBUG_ERR,"kenwood_set_mode: "
								"unsupported mode %d\n", mode);
				return -RIG_EINVAL;
		}

		mdbuf_len = sprintf(mdbuf, "MD%c;", kmode);
		ack_len = 0;
		retval = kenwood_transaction (rig, mdbuf, mdbuf_len, ackbuf, &ack_len);

		return retval;
}

/*
 * kenwood_get_mode
 * Assumes rig!=NULL, mode!=NULL
 */
int kenwood_get_mode(RIG *rig, vfo_t vfo, rmode_t *mode, pbwidth_t *width)
{
		unsigned char modebuf[50];
		int mode_len, retval;


		mode_len = 50;
		retval = kenwood_transaction (rig, "MD;", 3, modebuf, &mode_len);
		if (retval != RIG_OK)
			return retval;

		if (mode_len != 4 || modebuf[1] != 'D') {
				rig_debug(RIG_DEBUG_ERR,"kenwood_get_mode: unexpected answer, len=%d\n",
								mode_len);
				return -RIG_ERJCTED;
		}

		switch (modebuf[2]) {
			case MD_CW:	*mode = RIG_MODE_CW; break;
			case MD_CWR:	*mode = RIG_MODE_CWR; break;
			case MD_USB:	*mode = RIG_MODE_USB; break;
			case MD_LSB:	*mode = RIG_MODE_LSB; break;
			case MD_FM:	*mode = RIG_MODE_FM; break;
			case MD_AM:	*mode = RIG_MODE_AM; break;
			case MD_FSK:	*mode = RIG_MODE_RTTY; break;
			case MD_FSKR:	*mode = RIG_MODE_RTTYR; break;
			case MD_NONE:	*mode = RIG_MODE_NONE; break;
			default:
				rig_debug(RIG_DEBUG_ERR,"kenwood_get_mode: "
								"unsupported mode '%c'\n", modebuf[2]);
				return -RIG_EINVAL;
		}

                *width = rig_passband_normal(rig, *mode);

		return RIG_OK;
}

int kenwood_set_level(RIG *rig, vfo_t vfo, setting_t level, value_t val)
{
		unsigned char levelbuf[16], ackbuf[16];
		int level_len, ack_len, retval;
		int kenwood_val;

		if (RIG_LEVEL_IS_FLOAT(level))
				kenwood_val = val.f * 255;
		else
				kenwood_val = val.i;

		switch (level) {
		case RIG_LEVEL_RFPOWER:
		level_len = sprintf(levelbuf, "PC%03d;", kenwood_val);
		break;

		case RIG_LEVEL_AF:
		level_len = sprintf(levelbuf, "AG%03d;", kenwood_val);
		break;

		case RIG_LEVEL_RF:
		level_len = sprintf(levelbuf, "RG%03d;", kenwood_val);
		break;

		case RIG_LEVEL_SQL:
		level_len = sprintf(levelbuf, "SQ%03d;", kenwood_val);
		break;

		case RIG_LEVEL_AGC:
		if (kenwood_val > 3) kenwood_val = 3; /* 0.. 255 */
		level_len = sprintf(levelbuf, "GT%03d;", 84*kenwood_val);
		break;

		default:
			rig_debug(RIG_DEBUG_ERR,"Unsupported set_level %d", level);
			return -RIG_EINVAL;
		}

		ack_len = 0;
		retval = kenwood_transaction (rig, levelbuf, level_len, ackbuf, &ack_len);

		if (retval != RIG_OK)
				return retval;

		return RIG_OK;
}

/* 
 * assumes f!=NULL
 */
static int get_kenwood_level(RIG *rig, const char *cmd, int cmd_len, float *f)
{
	unsigned char lvlbuf[50];
	int lvl_len, retval;
	int lvl;

	lvl_len = 50;
	retval = kenwood_transaction (rig, cmd, cmd_len, lvlbuf, &lvl_len);
	if (retval != RIG_OK)
		return retval;

	if (lvl_len != 6) {
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
		unsigned char lvlbuf[50];
		int lvl_len, retval;
		int lvl;
		int i, ret, agclevel;

		lvl_len = 50;
		switch (level) {
		case RIG_LEVEL_STRENGTH:
			retval = kenwood_transaction (rig, "SM;", 3, lvlbuf, &lvl_len);
			if (retval != RIG_OK)
				return retval;

			if (lvl_len != 7 || lvlbuf[1] != 'M') {
				rig_debug(RIG_DEBUG_ERR,"kenwood_get_level: "
								"wrong answer len=%d\n", lvl_len);
				return -RIG_ERJCTED;
			}

			/* Frontend expects:  -54 = S0, 0 = S9  */
			sscanf(lvlbuf+2, "%d", &val->i);
			val->i = (val->i * 4) - 54;
			break;

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
		case RIG_LEVEL_RFPOWER:
			return get_kenwood_level(rig, "PC;", 3, &val->f);

		case RIG_LEVEL_AF:
			return get_kenwood_level(rig, "AG;", 3, &val->f);

		case RIG_LEVEL_RF:
			return get_kenwood_level(rig, "RG;", 3, &val->f);

		case RIG_LEVEL_SQL:
			return get_kenwood_level(rig, "SQ;", 3, &val->f);

		case RIG_LEVEL_MICGAIN:
			return get_kenwood_level(rig, "MG;", 3, &val->f);

		case RIG_LEVEL_AGC:
			ret = get_kenwood_level(rig, "GT;", 3, &val->f);
			agclevel = 255 * val->f;
			if (agclevel == 0) val->i = 0;
			else if (agclevel < 85) val->i = 1;
			else if (agclevel < 170) val->i = 2;
			else if (agclevel <= 255) val->i = 3;
			return ret;

		case RIG_LEVEL_SQLSTAT:
		case RIG_LEVEL_PREAMP:
		case RIG_LEVEL_IF:
		case RIG_LEVEL_APF:
		case RIG_LEVEL_NR:
		case RIG_LEVEL_PBT_IN:
		case RIG_LEVEL_PBT_OUT:
		case RIG_LEVEL_CWPITCH:
		case RIG_LEVEL_KEYSPD:
		case RIG_LEVEL_NOTCHF:
		case RIG_LEVEL_COMP:
		case RIG_LEVEL_BKINDL:
		case RIG_LEVEL_BALANCE:
			return -RIG_ENIMPL;

		default:
			rig_debug(RIG_DEBUG_ERR,"Unsupported get_level %d", level);
			return -RIG_EINVAL;
		}

		return RIG_OK;
}


int kenwood_set_func(RIG *rig, vfo_t vfo, setting_t func, int status)
{
		unsigned char fctbuf[16], ackbuf[16];
		int fct_len, ack_len;

		/* Optimize:
		 *   sort the switch cases with the most frequent first
		 */
		ack_len = 0;
		switch (func) {
		case RIG_FUNC_NB:
		fct_len = sprintf(fctbuf,"NB%c;", status==RIG_FUNC_NB?'0':'1');
		return kenwood_transaction (rig, fctbuf, fct_len, ackbuf, &ack_len);

		case RIG_FUNC_ABM:
		fct_len = sprintf(fctbuf,"AM%c;", status==RIG_FUNC_ABM?'0':'1');
		return kenwood_transaction (rig, fctbuf, fct_len, ackbuf, &ack_len);

		case RIG_FUNC_COMP:
		fct_len = sprintf(fctbuf,"PR%c;", status==RIG_FUNC_COMP?'0':'1');
		return kenwood_transaction (rig, fctbuf, fct_len, ackbuf, &ack_len);

		case RIG_FUNC_TONE:
		fct_len = sprintf(fctbuf,"TO%c;", status==RIG_FUNC_TONE?'0':'1');
		return kenwood_transaction (rig, fctbuf, fct_len, ackbuf, &ack_len);

		case RIG_FUNC_TSQL:
		fct_len = sprintf(fctbuf,"CT%c;", status==RIG_FUNC_TSQL?'0':'1');
		return kenwood_transaction (rig, fctbuf, fct_len, ackbuf, &ack_len);

		case RIG_FUNC_VOX:
		fct_len = sprintf(fctbuf,"VX%c;", status==RIG_FUNC_VOX?'0':'1');
		return kenwood_transaction (rig, fctbuf, fct_len, ackbuf, &ack_len);

		case RIG_FUNC_NR:
		fct_len = sprintf(fctbuf,"NR%c;", status==RIG_FUNC_NR?'0':'1');
		return kenwood_transaction (rig, fctbuf, fct_len, ackbuf, &ack_len);

		case RIG_FUNC_BC:
		fct_len = sprintf(fctbuf,"BC%c;", status==RIG_FUNC_BC?'0':'1');
		return kenwood_transaction (rig, fctbuf, fct_len, ackbuf, &ack_len);

		case RIG_FUNC_ANF:
		fct_len = sprintf(fctbuf,"NT%c;", status==RIG_FUNC_ANF?'0':'1');
		return kenwood_transaction (rig, fctbuf, fct_len, ackbuf, &ack_len);

		case RIG_FUNC_LOCK:
		fct_len = sprintf(fctbuf,"LK%c;", status==RIG_FUNC_LOCK?'0':'1');
		return kenwood_transaction (rig, fctbuf, fct_len, ackbuf, &ack_len);

		default:
			rig_debug(RIG_DEBUG_ERR,"Unsupported set_func %#x", func);
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
	unsigned char fctbuf[50];
	int fct_len, retval;

	fct_len = 50;
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
		unsigned char fctbuf[50];
		int fct_len, retval;

		fct_len = 50;
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
 * 	Please owners report to me <fillods@users.sourceforge.net>, thanks. --SF

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
	
	ack_len = 0;
	return kenwood_transaction (rig, tonebuf, tone_len, ackbuf, &ack_len);
}

/*
 * kenwood_get_ctcss_tone
 * Assumes rig!=NULL, rig->state.priv!=NULL
 */
int kenwood_get_ctcss_tone(RIG *rig, vfo_t vfo, tone_t *tone)
{
	const struct rig_caps *caps;
	unsigned char tonebuf[50];
	int tone_len, i, retval;
	unsigned int tone_idx;
								 
	caps = rig->caps;

	tone_len = 50;
	retval = kenwood_transaction (rig, "IF;", 3, tonebuf, &tone_len);
	if (retval != RIG_OK)
		return retval;

	if (tone_len != 38 || tonebuf[1] != 'F') {
		rig_debug(RIG_DEBUG_ERR,"kenwood_get_ctcss_tone: wrong answer len=%d\n",
						tone_len);
		return -RIG_ERJCTED;
	}

	tonebuf[36] = '\0';
	tone_idx = atoi(&tonebuf[34]);

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
 * kenwood_get_ptt
 * Assumes rig!=NULL, ptt!=NULL
 */
int kenwood_get_ptt(RIG *rig, vfo_t vfo, ptt_t *ptt)
{
		unsigned char infobuf[50];
		int info_len, retval;

		info_len = 50;
		retval = kenwood_transaction (rig, "IF;", 3, infobuf, &info_len);
		if (retval != RIG_OK)
			return retval;

		if (info_len != 38 || infobuf[1] != 'F') {
		rig_debug(RIG_DEBUG_ERR,"kenwood_get_ptt: wrong answer len=%d\n",
						info_len);
		return -RIG_ERJCTED;
		}

		*ptt = infobuf[28] == '0' ? RIG_PTT_OFF : RIG_PTT_ON;

		return RIG_OK;
}

/*
 * kenwood_set_ptt
 * Assumes rig!=NULL
 */
int kenwood_set_ptt(RIG *rig, vfo_t vfo, ptt_t ptt)
{
		unsigned char ackbuf[16];
		int ack_len = 0;

		return kenwood_transaction (rig, ptt==RIG_PTT_ON? "TX;":"RX;", 3, 
						ackbuf, &ack_len);
}


/*
 * kenwood_get_dcd
 * Assumes rig!=NULL, dcd!=NULL
 */
int kenwood_get_dcd(RIG *rig, vfo_t vfo, dcd_t *dcd)
{
	unsigned char busybuf[50];
	int busy_len, retval;
								 
	busy_len = 50;
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
int kenwood_set_trn(RIG *rig, int trn)
{
		unsigned char trnbuf[16], ackbuf[16];
		int trn_len, ack_len = 0;

		trn_len = sprintf(trnbuf,"AI%c;", trn==RIG_TRN_RIG?'1':'0');

		return kenwood_transaction (rig, trnbuf, trn_len, ackbuf, &ack_len);
}

/*
 * kenwood_get_trn
 * Assumes rig!=NULL, trn!=NULL
 */
int kenwood_get_trn(RIG *rig, int *trn)
{
		unsigned char trnbuf[50];
		int trn_len, retval;

		trn_len = 50;
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
		int pwr_len, ack_len = 0;

		pwr_len = sprintf(pwrbuf,"PS%c;", status==RIG_POWER_ON?'1':'0');

		return kenwood_transaction (rig, pwrbuf, pwr_len, ackbuf, &ack_len);
}

/*
 * kenwood_get_powerstat
 * Assumes rig!=NULL, trn!=NULL
 */
int kenwood_get_powerstat(RIG *rig, powerstat_t *status)
{
		unsigned char pwrbuf[50];
		int pwr_len = 50, retval;

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
		int rst_len, ack_len = 0;
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
				ack_len = 0;
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
		int ack_len = 0;

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

		ack_len = 16;
		return kenwood_transaction (rig, cmd, 3, ackbuf, &ack_len);
}

/*
 * kenwood_set_mem
 * Assumes rig!=NULL
 */
int kenwood_set_mem(RIG *rig, vfo_t vfo, int ch)
{
		unsigned char membuf[16], ackbuf[16];
		int mem_len, ack_len = 0;

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
		unsigned char membuf[50];
		int retval, mem_len;

		/*
		 * "MCbmm;"
		 * where b is the bank number, mm the memory number.
		 * b can be a space
		 */

		mem_len = 50;
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
	unsigned char firmbuf[50];
	int firm_len, retval;
								 
	firm_len = 50;
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

#define IDBUFSZ 16

/*
 * proberigs_kenwood
 *
 * Notes:
 * There's only one rig possible per port.
 */
rig_model_t probeallrigs_kenwood(port_t *port, rig_probe_func_t cfunc, rig_ptr_t data)
{
	unsigned char idbuf[IDBUFSZ];
	int id_len, i, k_id;
	int retval;
	int rates[] = { 57600, 9600, 4800, 0 };	/* possible baud rates */
	int rates_idx;

	if (!port)
		return RIG_MODEL_NONE;

	if (port->type.rig != RIG_PORT_SERIAL)
		return RIG_MODEL_NONE;

	port->write_delay = port->post_write_delay = 0;
	port->parm.serial.stop_bits = 2;
	port->retry = 1;

	/*
	 * try for all different baud rates
	 */
	for (rates_idx = 0; rates[rates_idx]; rates_idx++) {
		port->parm.serial.rate = rates[rates_idx];
		port->timeout = 2*1000/rates[rates_idx] + 50;
	
		retval = serial_open(port);
		if (retval != RIG_OK)
			return RIG_MODEL_NONE;
	
		retval = write_block(port, "ID;", 3);
		id_len = read_string(port, idbuf, IDBUFSZ, EOM_KEN EOM_TH, strlen(EOM_KEN EOM_TH));
		close(port->fd);

		if (retval != RIG_OK || id_len < 0)
			continue;
	}

	if (retval != RIG_OK || id_len < 0 || !strcmp(idbuf, "ID;"))
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
					if (cfunc)
						(*cfunc)(port, kenwood_id_string_list[i].model, data);
					return kenwood_id_string_list[i].model;
			}
	}

	/* then, try ID numbers */

	k_id = atoi(idbuf+2);

	/*
	 * Elecraft K2 returns same ID as TS570
	 */
	if (k_id == 17) {
		retval = serial_open(port);
		if (retval != RIG_OK)
			return RIG_MODEL_NONE;
		retval = write_block(port, "K2;", 3);
    		id_len = read_string(port, idbuf, IDBUFSZ, EOM_KEN EOM_TH, 2);
		close(port->fd);
		if (retval != RIG_OK)
			return RIG_MODEL_NONE;
		/* 
		 * reply should be something like 'K2n;'
		 */
		if (id_len == 4 || !strcmp(idbuf, "K2")) {
			rig_debug(RIG_DEBUG_VERBOSE,"probe_kenwood: found K2\n");
			if (cfunc)
				(*cfunc)(port, RIG_MODEL_K2, data);
			return RIG_MODEL_K2;
		}
	}

	for (i=0; kenwood_id_list[i].model != RIG_MODEL_NONE; i++) {
		if (kenwood_id_list[i].id == k_id) {
			rig_debug(RIG_DEBUG_VERBOSE,"probe_kenwood: "
							"found %03d\n", k_id);
			if (cfunc)
				(*cfunc)(port, kenwood_id_list[i].model, data);
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


/* kenwood_init
 *
 * Basically, it sets up *priv
 * REM: serial port is already open (rig->state.rigport.fd)
 */
int kenwood_init(RIG *rig)
{
    const struct rig_caps *caps;
    const struct kenwood_priv_caps *priv_caps;
    rig_debug(RIG_DEBUG_TRACE, "%s: called\n", __FUNCTION__);

    if (!rig || !rig->caps)
        return -RIG_EINVAL;

    caps = rig->caps;
    if (!caps->priv)
        return -RIG_ECONF;

    priv_caps = (const struct kenwood_priv_caps *) caps->priv;

#if 0   /* No private data for Kenwood backends */
    priv = (struct kenwood_priv_data *)malloc(sizeof(struct kenwood_priv_data));
    if (!priv) {
        /* whoops! memory shortage! */
        return -RIG_ENOMEM;
    }

    rig->state.priv = (void *)priv;
    /* Assign default values */
    priv->dummy = -1;  // placeholder for real entries.
#endif

    return RIG_OK;
}

/* kenwood_cleanup
 * the serial port is closed by the frontend
 */
int kenwood_cleanup(RIG *rig)
{
    rig_debug(RIG_DEBUG_TRACE, "%s: called\n", __FUNCTION__);

    if (!rig)
        return -RIG_EINVAL;

    if (rig->state.priv)
        free(rig->state.priv);
    rig->state.priv = NULL;

    return RIG_OK;
}

/*
 * initrigs_kenwood is called by rig_backend_load
 */
int initrigs_kenwood(void *be_handle)
{
		rig_debug(RIG_DEBUG_VERBOSE, "kenwood: _init called\n");

		rig_register(&ts950sdx_caps);
		rig_register(&ts50s_caps);
		rig_register(&ts440_caps);
		rig_register(&ts450s_caps);
		rig_register(&ts570d_caps);
		rig_register(&ts570s_caps);
		rig_register(&ts790_caps);
		rig_register(&ts850_caps);
		rig_register(&ts870s_caps);
		rig_register(&ts2000_caps);
		rig_register(&k2_caps);

		rig_register(&thd7a_caps);
		rig_register(&thf7e_caps);

		return RIG_OK;
}


