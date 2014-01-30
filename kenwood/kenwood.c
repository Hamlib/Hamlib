/*
 *  Hamlib Kenwood backend - main file
 *  Copyright (c) 2000-2011 by Stephane Fillod
 *  Copyright (C) 2009,2010 Alessandro Zummo <a.zummo@towertech.it>
 *  Copyright (C) 2009,2010,2011,2012,2013 by Nate Bargmann, n0nb@n0nb.us
 *
 *
 *   This library is free software; you can redistribute it and/or
 *   modify it under the terms of the GNU Lesser General Public
 *   License as published by the Free Software Foundation; either
 *   version 2.1 of the License, or (at your option) any later version.
 *
 *   This library is distributed in the hope that it will be useful,
 *   but WITHOUT ANY WARRANTY; without even the implied warranty of
 *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 *   Lesser General Public License for more details.
 *
 *   You should have received a copy of the GNU Lesser General Public
 *   License along with this library; if not, write to the Free Software
 *   Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA
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
#include <ctype.h>

#include "hamlib/rig.h"
#include "serial.h"
#include "misc.h"
#include "register.h"
#include "cal.h"

#include "kenwood.h"

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
	{ RIG_MODEL_TS950SDX, 8 },	/* reported as RIG_MODEL_TS950SD originally */
	{ RIG_MODEL_TS850, 9 },
	{ RIG_MODEL_TS450S, 10 },
	{ RIG_MODEL_TS690S, 11 },
	{ RIG_MODEL_TS870S, 15 },
	{ RIG_MODEL_TRC80, 16 },
	{ RIG_MODEL_TS570D, 17 },	/* Elecraft K2|K3 also returns 17 */
	{ RIG_MODEL_TS570S, 18 },
	{ RIG_MODEL_TS2000, 19 },
	{ RIG_MODEL_TS480, 20 },
	{ RIG_MODEL_TS590S, 21 },    /* TBC */
	{ RIG_MODEL_NONE, UNKNOWN_ID },	/* end marker */
};

/* XXX numeric ids have been tested only with the TS-450 */
static const struct kenwood_id_string kenwood_id_string_list[] = {
	{ RIG_MODEL_TS940, 	"001" },
	{ RIG_MODEL_TS811, 	"002" },
	{ RIG_MODEL_TS711, 	"003" },
	{ RIG_MODEL_TS440, 	"004" },
	{ RIG_MODEL_R5000, 	"005" },
	{ RIG_MODEL_TS790, 	"007" },
	{ RIG_MODEL_TS950SDX,	"008" },	/* reported as RIG_MODEL_TS950SD originally */
	{ RIG_MODEL_TS850,	"009" },
	{ RIG_MODEL_TS450S,	"010" },
	{ RIG_MODEL_TS690S,	"011" },
	{ RIG_MODEL_TS870S,	"015" },
	{ RIG_MODEL_TS570D,	"017" },	/* Elecraft K2|K3 also returns 17 */
	{ RIG_MODEL_TS570S,	"018" },
	{ RIG_MODEL_TS2000,	"019" },
	{ RIG_MODEL_TS480,	"020" },
	{ RIG_MODEL_TS590S,	"021" },    /* TBC */
	{ RIG_MODEL_THD7A,	"TH-D7" },
	{ RIG_MODEL_THD7AG,	"TH-D7G" },
	{ RIG_MODEL_TMD700,	"TM-D700" },
	{ RIG_MODEL_TMD710,	"TM-D710" },
	{ RIG_MODEL_THD72A,	"TH-D72" },
	{ RIG_MODEL_TMV7,	"TM-V7" },
	{ RIG_MODEL_TMV71,	"TM-V71" },
	{ RIG_MODEL_THF6A,	"TH-F6" },
	{ RIG_MODEL_THF7E,	"TH-F7" },
	{ RIG_MODEL_THG71,	"TH-G71" },
	{ RIG_MODEL_NONE,	NULL },	/* end marker */
};

rmode_t kenwood_mode_table[KENWOOD_MODE_TABLE_MAX] = {
	 [0] = RIG_MODE_NONE,
	 [1] = RIG_MODE_LSB,
	 [2] = RIG_MODE_USB,
	 [3] = RIG_MODE_CW,
	 [4] = RIG_MODE_FM,
	 [5] = RIG_MODE_AM,
	 [6] = RIG_MODE_RTTY,
	 [7] = RIG_MODE_CWR,
	 [8] = RIG_MODE_NONE,	/* TUNE mode */
	 [9] = RIG_MODE_RTTYR
};

/*
 * 38 CTCSS sub-audible tones
 */
const tone_t kenwood38_ctcss_list[] = {
	 670,  719,  744,  770,  797,  825,  854,  885,  915,  948,
	 974, 1000, 1035, 1072, 1109, 1148, 1188, 1230, 1273, 1318,
	1365, 1413, 1462, 1514, 1567, 1622, 1679, 1738, 1799, 1862,
	1928, 2035, 2107, 2181, 2257, 2336, 2418, 2503,
	0,
};


/*
 * 42 CTCSS sub-audible tones
 */
const tone_t kenwood42_ctcss_list[] = {
         670,  693,  719,  744,  770,  797,  825,  854,  885,  915,  948,
	 974, 1000, 1035, 1072, 1109, 1148, 1188, 1230, 1273, 1318,
	1365, 1413, 1462, 1514, 1567, 1622, 1679, 1738, 1799, 1862,
        1928, 2035, 2065, 2107, 2181, 2257, 2291, 2336, 2418, 2503, 2541,
	0,
};


/* Token definitions for .cfgparams in rig_caps
 *
 * See enum rig_conf_e and struct confparams in rig.h
 */
const struct confparams kenwood_cfg_params[] = {
	{ TOK_FINE, "fine", "Fine", "Fine step mode",
		NULL, RIG_CONF_CHECKBUTTON, { } },
	{ TOK_VOICE, "voice", "Voice", "Voice recall",
		NULL, RIG_CONF_BUTTON, { } },
	{ TOK_XIT, "xit", "XIT", "XIT",
		NULL, RIG_CONF_CHECKBUTTON, { } },
	{ TOK_RIT, "rit", "RIT", "RIT",
		NULL, RIG_CONF_CHECKBUTTON, { } },
	{ RIG_CONF_END, NULL, }
};


/**
 * kenwood_transaction
 * Assumes rig!=NULL rig->state!=NULL rig->caps!=NULL
 *
 * Parameters:
 * cmdstr:		Command to be sent to the rig. cmdstr can also be NULL,
 * 				indicating that only a reply is needed (nothing will be sent).
 * cmd_len:		Not used
 * data:		Buffer for reply string.  Can be NULL, indicating that no reply
 * 				is needed and will return with RIG_OK after command was sent.
 * datasize: in: Size of buffer. It is the caller's responsibily to provide
 * 				 a large enough buffer for all possible replies for a command.
 * 			out: Location where to store number of bytes read.
 *
 * returns:
 *   RIG_OK -		if no error occured.
 *   RIG_EIO -		if an I/O error occured while sending/receiving data.
 *   RIG_ETIMEOUT -	if timeout expires without any characters received.
 *   RIG_REJECTED -	if a negative acknowledge was received or command not
 * 					recognized by rig.
 */
int kenwood_transaction(RIG *rig, const char *cmdstr, int cmd_len,
				char *data, size_t *datasize)
{
	rig_debug(RIG_DEBUG_VERBOSE, "%s called\n", __func__);

	if (!rig || !datasize)
		return -RIG_EINVAL;

	struct kenwood_priv_caps *caps = kenwood_caps(rig);
	struct rig_state *rs;
	int retval;
	char cmdtrm[2];  /* Default Command/Reply termination char */
	int retry_read = 0;

	rs = &rig->state;
	rs->hold_decode = 1;

	rig_debug(RIG_DEBUG_TRACE, "%s: cmdstr = %s\n", __func__, cmdstr);

	cmdtrm[0] = caps->cmdtrm;
	cmdtrm[1] = '\0';

transaction_write:

	serial_flush(&rs->rigport);

	if (cmdstr) {

	char *cmd;
	int len = strlen(cmdstr);

	cmd = malloc(len + 2);
	if (cmd == NULL) {
		retval = -RIG_ENOMEM;
		goto transaction_quit;
	}

	memcpy(cmd, cmdstr, len);

	/* XXX the if is temporary, until all invocations are fixed */
	if (cmdstr[len - 1] != ';' && cmdstr[len - 1] != '\r') {
		cmd[len] = caps->cmdtrm;
		len++;
	}

	retval = write_block(&rs->rigport, cmd, len);

	free(cmd);

	if (retval != RIG_OK)
		goto transaction_quit;
	}

	if (data == NULL || *datasize <= 0) {
		rig->state.hold_decode = 0;
		return RIG_OK;  /* don't want a reply */
	}

	memset(data,0,*datasize);
	retval = read_string(&rs->rigport, data, *datasize, cmdtrm, strlen(cmdtrm));
	if (retval < 0) {
		if (retry_read++ < rig->state.rigport.retry)
			goto transaction_write;
		goto transaction_quit;
	}
	else
		*datasize = retval;

	/* Check that command termination is correct */
	if (strchr(cmdtrm, data[strlen(data)-1])==NULL) {
		rig_debug(RIG_DEBUG_ERR, "%s: Command is not correctly terminated '%s'\n", __func__, data);
		if (retry_read++ < rig->state.rigport.retry)
			goto transaction_write;
		retval = -RIG_EPROTO;
		goto transaction_quit;
	}

	if (strlen(data) == 2) {
	switch (data[0]) {
	case 'N':
		/* Command recognised by rig but invalid data entered. */
		rig_debug(RIG_DEBUG_VERBOSE, "%s: NegAck for '%s'\n", __func__, cmdstr);
		retval = -RIG_ENAVAIL;
		goto transaction_quit;
	case 'O':
		/* Too many characters sent without a carriage return */
		rig_debug(RIG_DEBUG_VERBOSE, "%s: Overflow for '%s'\n", __func__, cmdstr);
		if (retry_read++ < rig->state.rigport.retry)
			goto transaction_write;
		retval = -RIG_EPROTO;
		goto transaction_quit;
	case 'E':
		/* Communication error */
		rig_debug(RIG_DEBUG_VERBOSE, "%s: Communication error for '%s'\n", __func__, cmdstr);
		if (retry_read++ < rig->state.rigport.retry)
			goto transaction_write;
		retval = -RIG_EIO;
		goto transaction_quit;
	case '?':
			/* Command not understood by rig */
		rig_debug(RIG_DEBUG_ERR, "%s: Unknown command '%s'\n", __func__, cmdstr);
		if (retry_read++ < rig->state.rigport.retry)
			goto transaction_write;
		retval = -RIG_ERJCTED;
		goto transaction_quit;
	}
	}

	/* always give back a null terminated string without
	 * the command terminator.
	 */
	if (strlen(data) > 0)
		data[strlen(data) - 1] = '\0';  /* not elegant, but works */
	else
		data[0] = '\0';

	/*
	 * Check that we received the correct reply. The first two characters
	 * should be the same as command.
	 */
	if (cmdstr && (data[0] != cmdstr[0] || data[1] != cmdstr[1])) {
		/*
		 * TODO: When RIG_TRN is enabled, we can pass the string
		 * to the decoder for callback. That way we don't ignore
		 * any commands.
		 */
		rig_debug(RIG_DEBUG_ERR, "%s: wrong reply %c%c for command %c%c\n",
			__func__, data[0], data[1], cmdstr[0], cmdstr[1]);

		if (retry_read++ < rig->state.rigport.retry)
			goto transaction_write;

		retval =  -RIG_EPROTO;
		goto transaction_quit;
	}

	retval = RIG_OK;

transaction_quit:

	rs->hold_decode = 0;
	return retval;
}


/**
 * kenwood_safe_transaction
 * A wrapper for kenwood_transaction to check returned data against
 * expected length,
 *
 * Parameters:
 * 	cmd			Same as kenwood_transaction() cmdstr
 * 	buf			Same as kenwwod_transaction() data
 * 	buf_size	Same as kenwood_transaction() datasize
 * 	expected	Value of expected string length
 *
 * Returns:
 *   RIG_OK -		if no error occured.
 *   RIG_EPROTO		if returned string and expected are not equal
 *   Error from kenwood_transaction() if any
 *
 */
int kenwood_safe_transaction(RIG *rig, const char *cmd, char *buf,
			size_t buf_size, size_t expected)
{
	rig_debug(RIG_DEBUG_VERBOSE, "%s called\n", __func__);

	if (!rig || !buf)
		return -RIG_EINVAL;

	int err;

	if (expected == 0)
		buf_size = 0;

	err = kenwood_transaction(rig, cmd, strlen(cmd), buf, &buf_size);
	if (err != RIG_OK)
		return err;

	if (buf_size != expected) {
		rig_debug(RIG_DEBUG_ERR, "%s: wrong answer; len for cmd %s: "
					"expected = %d, got %d\n",
					__func__, cmd, expected, buf_size);
		return -RIG_EPROTO;
	}

	return RIG_OK;
}

rmode_t kenwood2rmode(unsigned char mode, const rmode_t mode_table[])
{
	rig_debug(RIG_DEBUG_VERBOSE, "%s called\n", __func__);

	if (mode >= KENWOOD_MODE_TABLE_MAX)
		return RIG_MODE_NONE;
	return mode_table[mode];
}

char rmode2kenwood(rmode_t mode, const rmode_t mode_table[])
{
	int i;

	rig_debug(RIG_DEBUG_VERBOSE, "%s called\n", __func__);

	for(i = 0; i < KENWOOD_MODE_TABLE_MAX; i++) {
		if (mode_table[i] == mode)
			return i;
	}
	return -1;
}

int kenwood_init(RIG *rig)
{
	rig_debug(RIG_DEBUG_VERBOSE, "%s called\n", __func__);

	if (!rig)
		return -RIG_EINVAL;

	struct kenwood_priv_data *priv;
	struct kenwood_priv_caps *caps = kenwood_caps(rig);

	priv = malloc(sizeof(struct kenwood_priv_data));
	if (priv == NULL)
		return -RIG_ENOMEM;

	memset(priv, 0x00, sizeof(struct kenwood_priv_data));

	priv->split = RIG_SPLIT_OFF;

	rig->state.priv = priv;

	/* default mode_table */
	if (caps->mode_table == NULL)
		caps->mode_table = kenwood_mode_table;

	/* default if_len */
	if (caps->if_len == 0)
		caps->if_len = 38;

	rig_debug(RIG_DEBUG_TRACE, "%s: if_len = %d\n", __func__, caps->if_len);

	return RIG_OK;
}

int kenwood_cleanup(RIG *rig)
{
	rig_debug(RIG_DEBUG_VERBOSE, "%s called\n", __func__);

	if (!rig)
		return -RIG_EINVAL;

	free(rig->state.priv);
	rig->state.priv = NULL;

	return RIG_OK;
}

int kenwood_open(RIG *rig)
{
	rig_debug(RIG_DEBUG_VERBOSE, "%s called\n", __func__);

	if (!rig)
		return -RIG_EINVAL;

	int err, i;
	char *idptr;
	char id[KENWOOD_MAX_BUF_LEN];

	/* get id in buffer, will be null terminated */
	err = kenwood_get_id(rig, id);
	if (err != RIG_OK) {
		rig_debug(RIG_DEBUG_ERR, "%s: cannot get identification\n", __func__);
		return err;
	}

	/* id is something like 'IDXXX' or 'ID XXX' */
	if (strlen(id) < 5) {
		rig_debug(RIG_DEBUG_ERR, "%s: unknown id type (%s)\n", __func__, id);
		return -RIG_EPROTO;
	}

	/* check for a white space and skip it */
	idptr = &id[2];
	if (*idptr == ' ')
		idptr++;

	/* compare id string */
	for (i = 0; kenwood_id_string_list[i].model != RIG_MODEL_NONE; i++) {
		if (strcmp(kenwood_id_string_list[i].id, idptr) != 0)
			continue;

		/* found matching id, verify driver */
		rig_debug(RIG_DEBUG_TRACE, "%s: found match %s\n",
				__func__, kenwood_id_string_list[i].id);

		if (kenwood_id_string_list[i].model == rig->caps->rig_model)
			return RIG_OK;

		/* driver mismatch */
		rig_debug(RIG_DEBUG_ERR,
			"%s: wrong driver selected (%d instead of %d)\n",
			__func__, rig->caps->rig_model,
			kenwood_id_string_list[i].model);

		return -RIG_EINVAL;
	}

	rig_debug(RIG_DEBUG_ERR, "%s: your rig (%s) is unknown\n",
					__func__, id);

	return -RIG_EPROTO;
}


/* ID
 *  Reads transceiver ID number
 *
 *  caller must give a buffer of KENWOOD_MAX_BUF_LEN size
 *
 */
int kenwood_get_id(RIG *rig, char *buf)
{
	rig_debug(RIG_DEBUG_VERBOSE, "%s called\n", __func__);

	if (!rig)
		return -RIG_EINVAL;

	size_t size = KENWOOD_MAX_BUF_LEN;

	return kenwood_transaction(rig, "ID", 2, buf, &size);
}


/* IF
 *  Retrieves the transceiver status
 *
 */
static int kenwood_get_if(RIG *rig)
{
	rig_debug(RIG_DEBUG_VERBOSE, "%s called\n", __func__);

	if (!rig)
		return -RIG_EINVAL;

	struct kenwood_priv_data *priv = rig->state.priv;
	struct kenwood_priv_caps *caps = kenwood_caps(rig);

	return kenwood_safe_transaction(rig, "IF", priv->info,
					KENWOOD_MAX_BUF_LEN, caps->if_len);
}


/* FN FR FT
 *  Sets the RX/TX VFO or M.CH mode of the transceiver, does not set split
 *  VFO, but leaves it unchanged if in split VFO mode.
 *
 */
int kenwood_set_vfo(RIG *rig, vfo_t vfo)
{
	rig_debug(RIG_DEBUG_VERBOSE, "%s called\n", __func__);

	if (!rig)
		return -RIG_EINVAL;

	struct kenwood_priv_data *priv = rig->state.priv;
	char cmdbuf[6];
	int retval;
	char vfo_function;

	switch (vfo) {
	case RIG_VFO_VFO:
	case RIG_VFO_A:
		vfo_function = '0';
		break;

	case RIG_VFO_B:
		vfo_function = '1';
		break;

	case RIG_VFO_MEM:
		vfo_function = '2';
		break;

	case RIG_VFO_CURR:
		return RIG_OK;

	default:
		rig_debug(RIG_DEBUG_ERR, "%s: unsupported VFO %d\n", __func__, vfo);
		return -RIG_EINVAL;
	}

	sprintf(cmdbuf, "FR%c", vfo_function);

	if (rig->caps->rig_model == RIG_MODEL_TS50)
	  {
	    cmdbuf[1] = 'N';
	  }

	/* set RX VFO */
	retval = kenwood_simple_cmd(rig, cmdbuf);
	if (retval != RIG_OK)
		return retval;

	/* if FN command then there's no FT or FR */
	/* If split mode on, the don't change TxVFO */
	if ('N' == cmdbuf[1] || priv->split != RIG_SPLIT_OFF)
		return RIG_OK;

	/* set TX VFO */
	cmdbuf[1] = 'T';
	return kenwood_simple_cmd(rig, cmdbuf);
}


/* FR FT
 *  Sets the split RX/TX VFO or M.CH mode of the transceiver.
 *
 */
int kenwood_set_split_vfo(RIG *rig, vfo_t vfo, split_t split, vfo_t txvfo)
{
	rig_debug(RIG_DEBUG_VERBOSE, "%s called\n", __func__);

	if (!rig)
		return -RIG_EINVAL;

	struct kenwood_priv_data *priv = rig->state.priv;
	char cmdbuf[6];
	int retval;
	unsigned char vfo_function;

	if(vfo != RIG_VFO_CURR) {
		switch (vfo) {
		case RIG_VFO_VFO:
		case RIG_VFO_A: vfo_function = '0'; break;
		case RIG_VFO_B: vfo_function = '1'; break;
		case RIG_VFO_MEM: vfo_function = '2'; break;
		default:
			rig_debug(RIG_DEBUG_ERR, "%s: unsupported VFO %d\n", __func__, vfo);
			return -RIG_EINVAL;
		}

		/* set RX VFO */
		sprintf(cmdbuf, "FR%c", vfo_function);
		retval = kenwood_simple_cmd(rig, cmdbuf);
		if (retval != RIG_OK)
			return retval;
	}

	/* Split off means Rx and Tx are the same */
	if (split == RIG_SPLIT_OFF) {
		txvfo = vfo;
		if (txvfo == RIG_VFO_CURR) {
			retval = rig_get_vfo(rig, &txvfo);
			if (retval != RIG_OK)
				return retval;
		}
	}

	switch (txvfo) {
	case RIG_VFO_VFO:
	case RIG_VFO_A: vfo_function = '0'; break;
	case RIG_VFO_B: vfo_function = '1'; break;
	case RIG_VFO_MEM: vfo_function = '2'; break;
	default:
		rig_debug(RIG_DEBUG_ERR, "%s: unsupported VFO %d\n", __func__, txvfo);
		return -RIG_EINVAL;
	}
	/* set TX VFO */
	sprintf(cmdbuf, "FT%c", vfo_function);
	retval = kenwood_simple_cmd(rig, cmdbuf);
	if (retval != RIG_OK)
		return retval;

	/* Remember whether split is on, for kenwood_set_vfo */
	priv->split = split;

	return RIG_OK;
}


/* SP
 *  Sets the split mode of the transceivers that have the FN command.
 *
 */
int kenwood_set_split(RIG *rig, vfo_t vfo, split_t split, vfo_t txvfo)
{
	rig_debug(RIG_DEBUG_VERBOSE, "%s called\n", __func__);

	if (!rig)
		return -RIG_EINVAL;

	struct kenwood_priv_data *priv = rig->state.priv;
	char cmdbuf[6];
	int retval;

	sprintf(cmdbuf, "SP%c", RIG_SPLIT_ON == split ? '1' : '0');
	retval = kenwood_simple_cmd(rig, cmdbuf);
	if (retval != RIG_OK)
		return retval;

	/* Remember whether split is on, for kenwood_set_vfo */
	priv->split = split;

	return RIG_OK;
}


/* IF
 *  Gets split VFO status from kenwood_get_if()
 *
 */
int kenwood_get_split_vfo_if(RIG *rig, vfo_t rxvfo, split_t *split, vfo_t *txvfo)
{
	rig_debug(RIG_DEBUG_VERBOSE, "%s called\n", __func__);

	if (!rig || !split || !txvfo)
		return -RIG_EINVAL;

	struct kenwood_priv_data *priv = rig->state.priv;
	int retval;

	retval = kenwood_get_if(rig);
	if (retval != RIG_OK)
		return retval;

	switch (priv->info[32]) {
	case '0':
		*split = RIG_SPLIT_OFF;
		break;

	case '1':
		*split = RIG_SPLIT_ON;
		break;

	default:
		rig_debug(RIG_DEBUG_ERR, "%s: unsupported split %c\n",
					__func__, priv->info[32]);
		return -RIG_EPROTO;
	}

	/* Remember whether split is on, for kenwood_set_vfo */
	priv->split = *split;

	/* TODO: find where is the txvfo.. */

	return RIG_OK;
}


/*
 * kenwood_get_vfo_if using byte 31 of the IF information field
 */
int kenwood_get_vfo_if(RIG *rig, vfo_t *vfo)
{
	rig_debug(RIG_DEBUG_VERBOSE, "%s called\n", __func__);

	if (!rig || !vfo)
		return -RIG_EINVAL;

	int retval;
	struct kenwood_priv_data *priv = rig->state.priv;

	retval = kenwood_get_if(rig);
	if (retval != RIG_OK)
		return retval;

	switch (priv->info[30]) {
	case '0':
		*vfo = RIG_VFO_A;
		break;

	case '1':
		*vfo = RIG_VFO_B;
		break;

	case '2':
		*vfo = RIG_VFO_MEM;
		break;

	default:
		rig_debug(RIG_DEBUG_ERR, "%s: unsupported VFO %c\n",
					__func__, priv->info[30]);
		return -RIG_EPROTO;
	}
	return RIG_OK;
}


/*
 * kenwood_set_freq
 */
int kenwood_set_freq(RIG *rig, vfo_t vfo, freq_t freq)
{
	rig_debug(RIG_DEBUG_VERBOSE, "%s called\n", __func__);

	if (!rig)
		return -RIG_EINVAL;

	char freqbuf[16];
	unsigned char vfo_letter;
	vfo_t tvfo;

	tvfo = (vfo==RIG_VFO_CURR || vfo==RIG_VFO_VFO) ? rig->state.current_vfo : vfo;

	switch (tvfo) {
	case RIG_VFO_A: vfo_letter = 'A'; break;
	case RIG_VFO_B: vfo_letter = 'B'; break;
	case RIG_VFO_C: vfo_letter = 'C'; break;
	default:
		rig_debug(RIG_DEBUG_ERR, "%s: unsupported VFO %d\n", __func__, vfo);
		return -RIG_EINVAL;
	}
	sprintf(freqbuf, "F%c%011ld", vfo_letter, (long)freq);

	return kenwood_simple_cmd(rig, freqbuf);
}

int kenwood_get_freq_if(RIG *rig, vfo_t vfo, freq_t *freq)
{
	rig_debug(RIG_DEBUG_VERBOSE, "%s called\n", __func__);

	if (!rig || !freq)
		return -RIG_EINVAL;

	struct kenwood_priv_data *priv = rig->state.priv;
	char freqbuf[50];
	int retval;

	retval = kenwood_get_if(rig);
	if (retval != RIG_OK)
		return retval;

	memcpy(freqbuf, priv->info, 15);
	freqbuf[14] = '\0';
	sscanf(freqbuf + 2, "%"SCNfreq, freq);

	return RIG_OK;
}

/*
 * kenwood_get_freq
 */
int kenwood_get_freq(RIG *rig, vfo_t vfo, freq_t *freq)
{
	rig_debug(RIG_DEBUG_VERBOSE, "%s called\n", __func__);

	if (!rig || !freq)
		return -RIG_EINVAL;

	char freqbuf[50];
	char cmdbuf[4];
	int retval;
	unsigned char vfo_letter;
	vfo_t tvfo;

	tvfo = (vfo == RIG_VFO_CURR || vfo==RIG_VFO_VFO) ? rig->state.current_vfo : vfo;

	/* memory frequency cannot be read with an Fx command, use IF */
	if (tvfo == RIG_VFO_MEM) {

		return kenwood_get_freq_if(rig, vfo, freq);
	}

	switch (tvfo) {
	case RIG_VFO_A: vfo_letter = 'A'; break;
	case RIG_VFO_B: vfo_letter = 'B'; break;
	case RIG_VFO_C: vfo_letter = 'C'; break;
	default:
		rig_debug(RIG_DEBUG_ERR, "%s: unsupported VFO %d\n",
				__func__, vfo);
		return -RIG_EINVAL;
	}

	sprintf(cmdbuf, "F%c", vfo_letter);

	retval = kenwood_safe_transaction(rig, cmdbuf, freqbuf, 50, 14);
	if (retval != RIG_OK)
		return retval;

	sscanf(freqbuf+2, "%"SCNfreq, freq);

	return RIG_OK;
}

int kenwood_get_rit(RIG *rig, vfo_t vfo, shortfreq_t *rit)
{
	rig_debug(RIG_DEBUG_VERBOSE, "%s called\n", __func__);

	if (!rig || !rit)
		return -RIG_EINVAL;

	int retval;
	char buf[6];
	struct kenwood_priv_data *priv = rig->state.priv;

	retval = kenwood_get_if(rig);
	if (retval != RIG_OK)
		return retval;

	memcpy(buf, &priv->info[18], 5);

	buf[5] = '\0';
	*rit = atoi(buf);

	return RIG_OK;
}

/*
 * rit can only move up/down by 10 Hz, so we use a loop...
 */
int kenwood_set_rit(RIG * rig, vfo_t vfo, shortfreq_t rit)
{
	rig_debug(RIG_DEBUG_VERBOSE, "%s called\n", __func__);

	if (!rig)
		return -RIG_EINVAL;

	char buf[4];
	int retval, i;

	if (rit == 0)
		return kenwood_simple_cmd(rig, "RC");

	sprintf(buf, "R%c", (rit > 0) ? 'U' : 'D');

	retval = kenwood_simple_cmd(rig, "RC");
	if (retval != RIG_OK)
		return retval;

	for (i = 0; i < abs(rint(rit/10)); i++)
		retval = kenwood_simple_cmd(rig, buf);

	return retval;
}

/*
 * rit and xit are the same
 */
int kenwood_get_xit(RIG *rig, vfo_t vfo, shortfreq_t *rit)
{
	rig_debug(RIG_DEBUG_VERBOSE, "%s called\n", __func__);

	if (!rig || !rit)
		return -RIG_EINVAL;

	return kenwood_get_rit(rig, vfo, rit);
}

int kenwood_set_xit(RIG *rig, vfo_t vfo, shortfreq_t rit)
{
	rig_debug(RIG_DEBUG_VERBOSE, "%s called\n", __func__);

	if (!rig)
		return -RIG_EINVAL;

	return kenwood_set_rit(rig, vfo, rit);
}

int kenwood_scan(RIG *rig, vfo_t vfo, scan_t scan, int ch)
{
	rig_debug(RIG_DEBUG_VERBOSE, "%s called\n", __func__);

	if (!rig)
		return -RIG_EINVAL;

	return kenwood_simple_cmd(rig, scan == RIG_SCAN_STOP ? "SC0" : "SC1");
}

/*
 *	000 No select
 *	002 FM Wide
 *	003 FM Narrow
 *	005 AM
 *	007 SSB
 *	009 CW
 *	010 CW NARROW
 */

/* XXX revise */
static int kenwood_set_filter(RIG *rig, pbwidth_t width)
{
	rig_debug(RIG_DEBUG_VERBOSE, "%s called\n", __func__);

	if (!rig)
		return -RIG_EINVAL;

	char *cmd;

	if (width <= Hz(250))
		cmd = "FL010009";
	else if(width <= Hz(500))
		cmd = "FL009009";
	else if(width <= kHz(2.7))
		cmd = "FL007007";
	else if(width <= kHz(6))
		cmd = "FL005005";
	else
		cmd = "FL002002";

	return kenwood_simple_cmd(rig, cmd);
}

/*
 * kenwood_set_mode
 */
int kenwood_set_mode(RIG *rig, vfo_t vfo, rmode_t mode, pbwidth_t width)
{
	rig_debug(RIG_DEBUG_VERBOSE, "%s called\n", __func__);

	if (!rig)
		return -RIG_EINVAL;

	struct kenwood_priv_caps *caps = kenwood_caps(rig);
	char buf[6];
	char kmode;
	int err;

	kmode = rmode2kenwood(mode, caps->mode_table);
	if (kmode < 0 ) {
		rig_debug(RIG_DEBUG_WARN, "%s: unsupported mode '%s'\n",
				__func__, rig_strrmode(mode));
		return -RIG_EINVAL;
	}

	sprintf(buf, "MD%c", '0' + kmode);
	err = kenwood_simple_cmd(rig, buf);
	if (err != RIG_OK)
		return err;

	if (rig->caps->rig_model == RIG_MODEL_TS450S
		|| rig->caps->rig_model == RIG_MODEL_TS690S
		|| rig->caps->rig_model == RIG_MODEL_TS850
		|| rig->caps->rig_model == RIG_MODEL_TS950SDX) {

		err = kenwood_set_filter(rig, width);
		/* non fatal */
	}

	return RIG_OK;
}

static int kenwood_get_filter(RIG *rig, pbwidth_t *width)
{
	rig_debug(RIG_DEBUG_VERBOSE, "%s called\n", __func__);

	if (!rig || !width)
		return -RIG_EINVAL;

	int err, f, f1, f2;
	char buf[10];

	err = kenwood_safe_transaction(rig, "FL", buf, sizeof(buf), 9);
	if (err != RIG_OK)
		return err;

	buf[8] = '\0';
	f2 = atoi(&buf[5]);

	buf[5] = '\0';
	f1 = atoi(&buf[2]);

	if (f2 > f1)
		f = f2;
	else
		f = f1;

	switch (f) {
	case 2:
		*width = kHz(12);
	break;
	case 3:
	case 5:
		*width = kHz(6);
	break;
	case 7:
		*width = kHz(2.7);
	break;
	case 9:
		*width = Hz(500);
	break;
	case 10:
		*width = Hz(250);
	break;
	}

	return RIG_OK;
}

/*
 * kenwood_get_mode
 */
int kenwood_get_mode(RIG *rig, vfo_t vfo, rmode_t *mode, pbwidth_t *width)
{
	rig_debug(RIG_DEBUG_VERBOSE, "%s called\n", __func__);

	if (!rig || !mode || !width)
		return -RIG_EINVAL;

	struct kenwood_priv_caps *caps = kenwood_caps(rig);
	char modebuf[10];
	int retval;

	retval = kenwood_safe_transaction(rig, "MD", modebuf, 6, 4);
	if (retval != RIG_OK)
		return retval;

	*mode = kenwood2rmode(modebuf[2] - '0', caps->mode_table);

	/* XXX ? */
	*width = rig_passband_normal(rig, *mode);

	return RIG_OK;
}

/* This is used when the radio does not support MD; for mode reading */
int kenwood_get_mode_if(RIG *rig, vfo_t vfo, rmode_t *mode, pbwidth_t *width)
{
	rig_debug(RIG_DEBUG_VERBOSE, "%s called\n", __func__);

	if (!rig || !mode || !width)
		return -RIG_EINVAL;

	int err;
	struct kenwood_priv_caps *caps = kenwood_caps(rig);
	struct kenwood_priv_data *priv = rig->state.priv;

	err = kenwood_get_if(rig);
	if (err != RIG_OK)
		return err;

	*mode = kenwood2rmode(priv->info[29] - '0', caps->mode_table);

	*width = rig_passband_normal(rig, *mode);

	if (rig->caps->rig_model == RIG_MODEL_TS450S
		|| rig->caps->rig_model == RIG_MODEL_TS690S
		|| rig->caps->rig_model == RIG_MODEL_TS850
		|| rig->caps->rig_model == RIG_MODEL_TS950SDX) {

		err = kenwood_get_filter(rig, width);
		/* non fatal */
	}

	return RIG_OK;
}

int kenwood_set_level(RIG *rig, vfo_t vfo, setting_t level, value_t val)
{
	rig_debug(RIG_DEBUG_VERBOSE, "%s called\n", __func__);

	if (!rig)
		return -RIG_EINVAL;

	char levelbuf[16];
	int i, kenwood_val;

	if (RIG_LEVEL_IS_FLOAT(level))
		kenwood_val = val.f * 255;
	else
		kenwood_val = val.i;

	switch (level) {
	case RIG_LEVEL_RFPOWER:
		/* XXX check level range */
		sprintf(levelbuf, "PC%03d", kenwood_val);
		break;

	case RIG_LEVEL_AF:
		sprintf(levelbuf, "AG%03d", kenwood_val);
		break;

	case RIG_LEVEL_RF:
		/* XXX check level range */
		sprintf(levelbuf, "RG%03d", kenwood_val);
		break;

	case RIG_LEVEL_SQL:
		sprintf(levelbuf, "SQ%03d", kenwood_val);
		break;

	case RIG_LEVEL_AGC:
		if (kenwood_val > 3)
			kenwood_val = 3; /* 0.. 255 */
		sprintf(levelbuf, "GT%03d", 84*kenwood_val);
		break;

	case RIG_LEVEL_ATT:
		/* set the attenuator if a correct value is entered */
		if (val.i == 0)
			sprintf(levelbuf, "RA00");
		else {
			for (i=0; i<MAXDBLSTSIZ && rig->state.attenuator[i]; i++) {
				if (val.i == rig->state.attenuator[i])
				{
					sprintf(levelbuf, "RA%02d", i+1);
					break;
				}
			}
			if (val.i != rig->state.attenuator[i])
				return -RIG_EINVAL;
		}
		break;

	case RIG_LEVEL_PREAMP:
		/* set the preamp if a correct value is entered */
		if (val.i == 0)
			sprintf(levelbuf, "PA0");
		else {
			for (i=0; i<MAXDBLSTSIZ && rig->state.preamp[i]; i++) {
				if (val.i == rig->state.preamp[i])
				{
					sprintf(levelbuf, "PA%01d", i+1);
					break;
				}
			}
			if (val.i != rig->state.preamp[i])
				return -RIG_EINVAL;
		}
		break;

	case RIG_LEVEL_SLOPE_HIGH:
		if(val.i>20 || val.i < 0)
			return -RIG_EINVAL;
		sprintf(levelbuf, "SH%02d",(val.i));
		break;

	case RIG_LEVEL_SLOPE_LOW:
		if(val.i>20 || val.i < 0)
			return -RIG_EINVAL;
		sprintf(levelbuf, "SL%02d",(val.i));
		break;

	case RIG_LEVEL_CWPITCH:
		if(val.i > 1000 || val.i < 400)
			return -RIG_EINVAL;
		sprintf(levelbuf, "PT%02d", (val.i / 50) - 8);
		break;

	case RIG_LEVEL_KEYSPD:
		if(val.i > 50 || val.i < 5)
			return -RIG_EINVAL;
		sprintf(levelbuf, "KS%03d", val.i);
		break;

	default:
		rig_debug(RIG_DEBUG_ERR, "Unsupported set_level %d", level);
		return -RIG_EINVAL;
	}

	return kenwood_simple_cmd(rig, levelbuf);
}

int get_kenwood_level(RIG *rig, const char *cmd, int cmd_len, float *f)
{
	rig_debug(RIG_DEBUG_VERBOSE, "%s called\n", __func__);

	if (!rig || !cmd || !f)
		return -RIG_EINVAL;

	char lvlbuf[10];
	int retval;
	int lvl;

	retval = kenwood_safe_transaction(rig, cmd, lvlbuf, 10, 6);
	if (retval != RIG_OK)
		return retval;

	/* 000..255 */
	sscanf(lvlbuf+2, "%d", &lvl);
	*f = (float)lvl/255.0;

	return RIG_OK;
};


/*
 * kenwood_get_level
 */
int kenwood_get_level(RIG *rig, vfo_t vfo, setting_t level, value_t *val)
{
	rig_debug(RIG_DEBUG_VERBOSE, "%s called\n", __func__);

	if (!rig || !val)
		return -RIG_EINVAL;

	char lvlbuf[50];
	int retval;
	int lvl;
	int i, ret, agclevel;
	size_t lvl_len;

	switch (level) {
	case RIG_LEVEL_RAWSTR:
		retval = kenwood_safe_transaction(rig, "SM", lvlbuf, 10, 7);
		if (retval != RIG_OK)
			return retval;

		/* XXX atoi ? */
		sscanf(lvlbuf+2, "%d", &val->i);	/* rawstr */
		break;

	case RIG_LEVEL_STRENGTH:
		retval = kenwood_safe_transaction(rig, "SM", lvlbuf, 10, 7);
		if (retval != RIG_OK)
			return retval;

		sscanf(lvlbuf+2, "%d", &val->i);	/* rawstr */

		if (rig->caps->str_cal.size)
			val->i = (int) rig_raw2val(val->i, &rig->caps->str_cal);
		else
			val->i = (val->i * 4) - 54;
		break;

	case RIG_LEVEL_ATT:
		retval = kenwood_safe_transaction(rig, "RA", lvlbuf, 50, 5);
		if (retval != RIG_OK)
			return retval;

		sscanf(lvlbuf+2, "%d", &lvl);
		if (lvl == 0) {
			val->i = 0;
		} else {
			for (i=0; i<lvl && i<MAXDBLSTSIZ; i++) {
				if (rig->state.attenuator[i] == 0) {
					rig_debug(RIG_DEBUG_ERR, "%s: "
							"unexpected att level %d\n",
							__func__, lvl);
					return -RIG_EPROTO;
				}
			}
			if (i != lvl)
				return -RIG_EINTERNAL;
			val->i = rig->state.attenuator[i-1];
		}
		break;

	case RIG_LEVEL_PREAMP:
		retval = kenwood_safe_transaction(rig, "PA", lvlbuf, 50, 4);
		if (retval != RIG_OK)
			return retval;

		if (lvlbuf[2] == '0')
			val->i = 0;
		else if (isdigit((int)lvlbuf[2])) {
			lvl = lvlbuf[2]-'0';
			for (i=0; i<lvl && i<MAXDBLSTSIZ; i++) {
				if (rig->state.preamp[i] == 0) {
					rig_debug(RIG_DEBUG_ERR,"%s: "
							"unexpected preamp level %d\n",
							__func__, lvl);
					return -RIG_EPROTO;
				}
			}
			if (i != lvl)
				return -RIG_EINTERNAL;
			val->i = rig->state.preamp[i-1];
		} else {
			rig_debug(RIG_DEBUG_ERR,"%s: "
					"unexpected preamp char '%c'\n",
					__func__, lvlbuf[2]);
			return -RIG_EPROTO;
		}
		break;

	case RIG_LEVEL_RFPOWER:
		return get_kenwood_level(rig, "PC", 3, &val->f);

	case RIG_LEVEL_AF:
		return get_kenwood_level(rig, "AG", 3, &val->f);

	case RIG_LEVEL_RF:
		return get_kenwood_level(rig, "RG", 3, &val->f);

	case RIG_LEVEL_SQL:
		return get_kenwood_level(rig, "SQ", 3, &val->f);

	case RIG_LEVEL_MICGAIN:
		return get_kenwood_level(rig, "MG", 3, &val->f);

	case RIG_LEVEL_AGC:
		ret = get_kenwood_level(rig, "GT", 3, &val->f);
		agclevel = 255 * val->f;
		if (agclevel == 0) val->i = 0;
		else if (agclevel < 85) val->i = 1;
		else if (agclevel < 170) val->i = 2;
		else if (agclevel <= 255) val->i = 3;
		return ret;

	case RIG_LEVEL_SLOPE_LOW:
		lvl_len = 50;
		retval = kenwood_transaction (rig, "SL", 3, lvlbuf, &lvl_len);
		if (retval != RIG_OK)
			return retval;
		lvlbuf[4]='\0';
		val->i=atoi(&lvlbuf[2]);
		break;

	case RIG_LEVEL_SLOPE_HIGH:
		lvl_len = 50;
		retval = kenwood_transaction (rig, "SH", 3, lvlbuf, &lvl_len);
		if (retval != RIG_OK)
			return retval;
		lvlbuf[4]='\0';
		val->i=atoi(&lvlbuf[2]);
		break;

	case RIG_LEVEL_CWPITCH:
		retval = kenwood_safe_transaction(rig, "PT", lvlbuf, 50, 5);
		if (retval != RIG_OK)
			return retval;

		sscanf(lvlbuf+2, "%d", &val->i);
		val->i = (val->i * 1000) + 1000; /* 00 - 08 */
		break;

	case RIG_LEVEL_KEYSPD:
		retval = kenwood_safe_transaction(rig, "KS", lvlbuf, 50, 6);
		if (retval != RIG_OK)
			return retval;

		sscanf(lvlbuf+2, "%d", &val->i);
		break;

	case RIG_LEVEL_IF:
	case RIG_LEVEL_APF:
	case RIG_LEVEL_NR:
	case RIG_LEVEL_PBT_IN:
	case RIG_LEVEL_PBT_OUT:
	case RIG_LEVEL_NOTCHF:
	case RIG_LEVEL_COMP:
	case RIG_LEVEL_BKINDL:
	case RIG_LEVEL_BALANCE:
		return -RIG_ENIMPL;

	default:
		rig_debug(RIG_DEBUG_ERR, "Unsupported get_level %d", level);
		return -RIG_EINVAL;
	}

	return RIG_OK;
}

int kenwood_set_func(RIG *rig, vfo_t vfo, setting_t func, int status)
{
	rig_debug(RIG_DEBUG_VERBOSE, "%s called\n", __func__);

	if (!rig)
		return -RIG_EINVAL;

	char buf[6]; /* longest cmd is GTxxx */

	switch (func) {
	case RIG_FUNC_NB:
		sprintf(buf, "NB%c", (status == 0) ? '0' : '1');
		return kenwood_simple_cmd(rig, buf);

	case RIG_FUNC_ABM:
		sprintf(buf, "AM%c", (status == 0) ? '0' : '1');
		return kenwood_simple_cmd(rig, buf);

	case RIG_FUNC_COMP:
		sprintf(buf, "PR%c", (status == 0) ? '0' : '1');
		return kenwood_simple_cmd(rig, buf);

	case RIG_FUNC_TONE:
		sprintf(buf, "TO%c", (status == 0) ? '0' : '1');
		return kenwood_simple_cmd(rig, buf);

	case RIG_FUNC_TSQL:
		sprintf(buf, "CT%c", (status == 0) ? '0' : '1');
		return kenwood_simple_cmd(rig, buf);

	case RIG_FUNC_VOX:
		sprintf(buf, "VX%c", (status == 0) ? '0' : '1');
		return kenwood_simple_cmd(rig, buf);

	case RIG_FUNC_FAGC:
		sprintf(buf, "GT00%c", (status == 0) ? '4' : '2');
		return kenwood_simple_cmd(rig, buf);

	case RIG_FUNC_NR:
		sprintf(buf, "NR%c", (status == 0) ? '0' : '1');
		return kenwood_simple_cmd(rig, buf);

	case RIG_FUNC_BC:
		sprintf(buf, "BC%c", (status == 0) ? '0' : '1');
		return kenwood_simple_cmd(rig, buf);

	case RIG_FUNC_ANF:
		sprintf(buf, "NT%c", (status == 0) ? '0' : '1');
		return kenwood_simple_cmd(rig, buf);

	case RIG_FUNC_LOCK:
		sprintf(buf, "LK%c", (status == 0) ? '0' : '1');
		return kenwood_simple_cmd(rig, buf);

	case RIG_FUNC_AIP:
		sprintf(buf, "MX%c", (status == 0) ? '0' : '1');
		return kenwood_simple_cmd(rig, buf);

	case RIG_FUNC_RIT:
		sprintf(buf, "RT%c", (status == 0) ? '0' : '1');
		return kenwood_simple_cmd(rig, buf);

	case RIG_FUNC_XIT:
		sprintf(buf, "XT%c", (status == 0) ? '0' : '1');
		return kenwood_simple_cmd(rig, buf);

	default:
		rig_debug(RIG_DEBUG_ERR, "Unsupported set_func %#x", func);
		return -RIG_EINVAL;
	}

	return -RIG_EINVAL;
}

/*
 * works for any 'format 1' command
 * answer is always 4 bytes: two byte command id, status and terminator
 */
int get_kenwood_func(RIG *rig, const char *cmd, int *status)
{
	rig_debug(RIG_DEBUG_VERBOSE, "%s called\n", __func__);

	if (!rig || !cmd || !status)
		return -RIG_EINVAL;

	int retval;
	char buf[10];

	retval = kenwood_safe_transaction(rig, cmd, buf, 10, 4);
	if (retval != RIG_OK)
		return retval;

	*status = buf[2] == '0' ? 0 : 1;

	return RIG_OK;
};

/*
 * kenwood_get_func
 */
int kenwood_get_func(RIG *rig, vfo_t vfo, setting_t func, int *status)
{
	rig_debug(RIG_DEBUG_VERBOSE, "%s called\n", __func__);

	if (!rig || !status)
		return -RIG_EINVAL;

	char fctbuf[20];
	int retval;

	switch (func) {
	case RIG_FUNC_FAGC:
		retval = kenwood_safe_transaction(rig, "GT", fctbuf, 20, 6);
		if (retval != RIG_OK)
			return retval;

		*status = fctbuf[4] != '4' ? 1 : 0;
		return RIG_OK;

	case RIG_FUNC_NB:
		return get_kenwood_func(rig, "NB", status);

	case RIG_FUNC_ABM:
		return get_kenwood_func(rig, "AM", status);

	case RIG_FUNC_COMP:
		return get_kenwood_func(rig, "PR", status);

	case RIG_FUNC_TONE:
		return get_kenwood_func(rig, "TO", status);

	case RIG_FUNC_TSQL:
		return get_kenwood_func(rig, "CT", status);

	case RIG_FUNC_VOX:
		return get_kenwood_func(rig, "VX", status);

	case RIG_FUNC_NR:
		return get_kenwood_func(rig, "NR", status);

 	/* FIXME on TS2000 */
	case RIG_FUNC_BC:
		return get_kenwood_func(rig, "BC", status);

	case RIG_FUNC_ANF:
		return get_kenwood_func(rig, "NT", status);

	case RIG_FUNC_LOCK:
		return get_kenwood_func(rig, "LK", status);

	case RIG_FUNC_AIP:
		return get_kenwood_func(rig, "MX", status);

	default:
		rig_debug(RIG_DEBUG_ERR, "Unsupported get_func %#x", func);
		return -RIG_EINVAL;
	}

	return -RIG_EINVAL;
}

/*
 * kenwood_set_ctcss_tone
 * Assumes rig->caps->ctcss_list != NULL
 *
 * Warning! This is untested stuff! May work at least on TS-870S
 * 	Please owners report to me <fillods@users.sourceforge.net>, thanks. --SF
 */
int kenwood_set_ctcss_tone(RIG *rig, vfo_t vfo, tone_t tone)
{
	rig_debug(RIG_DEBUG_VERBOSE, "%s called\n", __func__);

	if (!rig)
		return -RIG_EINVAL;

	const struct rig_caps *caps;
	char tonebuf[16];
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
	sprintf(tonebuf, "EX%03d%04d", 57, i+1);

	return kenwood_simple_cmd(rig, tonebuf);
}

int kenwood_set_ctcss_tone_tn(RIG *rig, vfo_t vfo, tone_t tone)
{
	rig_debug(RIG_DEBUG_VERBOSE, "%s called\n", __func__);

	if (!rig)
		return -RIG_EINVAL;

	const struct rig_caps *caps = rig->caps;
	char buf[6];
	int i;

	/* XXX 40 is a fixed constant */
	for (i = 0; caps->ctcss_list[i] != 0 && i < 40; i++) {
		if (tone == caps->ctcss_list[i])
			break;
	}

	if (tone != caps->ctcss_list[i])
		return -RIG_EINVAL;

	sprintf(buf, "TN%02d", i + 1);

	return kenwood_simple_cmd(rig, buf);
}

/*
 * kenwood_get_ctcss_tone
 * Assumes rig->state.priv != NULL
 */
int kenwood_get_ctcss_tone(RIG *rig, vfo_t vfo, tone_t *tone)
{
	rig_debug(RIG_DEBUG_VERBOSE, "%s called\n", __func__);

	if (!rig || !tone)
		return -RIG_EINVAL;

	struct kenwood_priv_data *priv = rig->state.priv;
	const struct rig_caps *caps;
	char tonebuf[3];
	int i, retval;
	unsigned int tone_idx;

	caps = rig->caps;

	retval = kenwood_get_if(rig);
	if (retval != RIG_OK)
		return retval;

	memcpy(tonebuf, &priv->info[34], 2);

	tonebuf[2] = '\0';
	tone_idx = atoi(tonebuf);

	if (tone_idx == 0) {
		rig_debug(RIG_DEBUG_ERR, "%s: CTCSS tone is zero (%s)\n",
					__func__, tonebuf);
		return -RIG_EPROTO;
	}

	/* check this tone exists. That's better than nothing. */
	for (i = 0; i<tone_idx; i++) {
		if (caps->ctcss_list[i] == 0) {
			rig_debug(RIG_DEBUG_ERR, "%s: CTCSS NG (%04d)\n",
						__func__, tone_idx);
			return -RIG_EPROTO;
		}
	}
	*tone = caps->ctcss_list[tone_idx-1];

	return RIG_OK;
}

int kenwood_set_ctcss_sql(RIG *rig, vfo_t vfo, tone_t tone)
{
	rig_debug(RIG_DEBUG_VERBOSE, "%s called\n", __func__);

	if (!rig)
		return -RIG_EINVAL;

	const struct rig_caps *caps = rig->caps;
	char buf[6];
	int i;

	for (i = 0; caps->ctcss_list[i] != 0 && i < 40; i++) {
		if (tone == caps->ctcss_list[i])
			break;
	}

	if (tone != caps->ctcss_list[i])
		return -RIG_EINVAL;

	sprintf(buf, "CN%02d", i + 1);

	return kenwood_simple_cmd(rig, buf);
}

int kenwood_get_ctcss_sql(RIG *rig, vfo_t vfo, tone_t *tone)
{
	rig_debug(RIG_DEBUG_VERBOSE, "%s called\n", __func__);

	if (!rig || !tone)
		return -RIG_EINVAL;

	const struct rig_caps *caps;
	char tonebuf[6];
	int i, retval;
	unsigned int tone_idx;

	caps = rig->caps;

	retval = kenwood_safe_transaction(rig, "CT", tonebuf, 6, 5);
	if (retval != RIG_OK)
		return retval;

	tone_idx = atoi(tonebuf+2);

	if (tone_idx == 0) {
		rig_debug(RIG_DEBUG_ERR, "%s: CTCSS is zero (%s)\n",
					__func__, tonebuf);
		return -RIG_EPROTO;
	}

	/* check this tone exists. That's better than nothing. */
	for (i = 0; i<tone_idx; i++) {
		if (caps->ctcss_list[i] == 0) {
			rig_debug(RIG_DEBUG_ERR, "%s: CTCSS NG (%04d)\n",
						__func__, tone_idx);
			return -RIG_EPROTO;
		}
	}
	*tone = caps->ctcss_list[tone_idx-1];

	return RIG_OK;
}


/*
 * set the aerial/antenna to use
 */
int kenwood_set_ant(RIG * rig, vfo_t vfo, ant_t ant)
{
	rig_debug(RIG_DEBUG_VERBOSE, "%s called\n", __func__);

	if (!rig)
		return -RIG_EINVAL;

	const char *cmd;

	switch (ant) {
	case RIG_ANT_1:
		cmd = "AN1";
		break;
	case RIG_ANT_2:
		cmd = "AN2";
		break;
	case RIG_ANT_3:
		cmd = "AN3";
		break;
	case RIG_ANT_4:
		cmd = "AN4";
		break;
	default:
		return -RIG_EINVAL;
	}

	return kenwood_simple_transaction(rig, cmd, 4);
}

int kenwood_set_ant_no_ack(RIG * rig, vfo_t vfo, ant_t ant)
{
	rig_debug(RIG_DEBUG_VERBOSE, "%s called\n", __func__);

	if (!rig)
		return -RIG_EINVAL;

	const char *cmd;

	switch (ant) {
	case RIG_ANT_1:
		cmd = "AN1";
		break;
	case RIG_ANT_2:
		cmd = "AN2";
		break;
	case RIG_ANT_3:
		cmd = "AN3";
		break;
	case RIG_ANT_4:
		cmd = "AN4";
		break;
	default:
		return -RIG_EINVAL;
	}

	return kenwood_simple_cmd(rig, cmd);
}

/*
 * get the aerial/antenna in use
 */
int kenwood_get_ant (RIG *rig, vfo_t vfo, ant_t *ant)
{
	rig_debug(RIG_DEBUG_VERBOSE, "%s called\n", __func__);

	if (!rig || !ant)
		return -RIG_EINVAL;

	char ackbuf[6];
	int retval;

	retval = kenwood_safe_transaction(rig, "AN", ackbuf, 6, 4);
	if (retval != RIG_OK)
		return retval;

	if (ackbuf[2] < '1' || ackbuf[2] > '9')
		return -RIG_EPROTO;

	*ant = RIG_ANT_N(ackbuf[2]-'1');

	/* XXX check that the returned antenna is valid for the current rig */

	return RIG_OK;
}

/*
 * kenwood_get_ptt
 */
int kenwood_get_ptt(RIG *rig, vfo_t vfo, ptt_t *ptt)
{
	rig_debug(RIG_DEBUG_VERBOSE, "%s called\n", __func__);

	if (!rig || !ptt)
		return -RIG_EINVAL;

	struct kenwood_priv_data *priv = rig->state.priv;
	int retval;

	retval = kenwood_get_if(rig);
	if (retval != RIG_OK)
		return retval;

	*ptt = priv->info[28] == '0' ? RIG_PTT_OFF : RIG_PTT_ON;

	return RIG_OK;
}

int kenwood_set_ptt(RIG *rig, vfo_t vfo, ptt_t ptt)
{
	const char *ptt_cmd;

	rig_debug(RIG_DEBUG_VERBOSE, "%s called\n", __func__);

	if (!rig)
		return -RIG_EINVAL;

	switch (ptt) {
		case RIG_PTT_ON:      ptt_cmd = "TX"; break;
		case RIG_PTT_ON_MIC:  ptt_cmd = "TX0"; break;
		case RIG_PTT_ON_DATA: ptt_cmd = "TX1"; break;
		case RIG_PTT_OFF: ptt_cmd = "RX"; break;
		default: return -RIG_EINVAL;
	}
	return kenwood_simple_cmd(rig, ptt_cmd);
}

int kenwood_set_ptt_safe(RIG *rig, vfo_t vfo, ptt_t ptt)
{
	rig_debug(RIG_DEBUG_VERBOSE, "%s called\n", __func__);

	if (!rig)
		return -RIG_EINVAL;

	int err;
	ptt_t current_ptt;

	err = kenwood_get_ptt(rig, vfo, &current_ptt);
	if (err != RIG_OK)
		return err;

	if (current_ptt == ptt)
		return RIG_OK;

	return kenwood_simple_cmd(rig,
		(ptt == RIG_PTT_ON) ? "TX" : "RX");
}


/*
 * kenwood_get_dcd
 */
int kenwood_get_dcd(RIG *rig, vfo_t vfo, dcd_t *dcd)
{
	rig_debug(RIG_DEBUG_VERBOSE, "%s called\n", __func__);

	if (!rig || !dcd)
		return -RIG_EINVAL;

	char busybuf[10];
	int retval;

	retval = kenwood_safe_transaction(rig, "BY", busybuf, 10, 4);
	if (retval != RIG_OK)
		return retval;

	*dcd = (busybuf[2] == '1') ? RIG_DCD_ON : RIG_DCD_OFF;

	return RIG_OK;
}

/*
 * kenwood_set_trn
 */
int kenwood_set_trn(RIG *rig, int trn)
{
	rig_debug(RIG_DEBUG_VERBOSE, "%s called\n", __func__);

	if (!rig)
		return -RIG_EINVAL;

	return kenwood_simple_transaction(rig,
		(trn == RIG_TRN_RIG) ? "AI1" : "AI0", 4);
}

/*
 * kenwood_get_trn
 */
int kenwood_get_trn(RIG *rig, int *trn)
{
	rig_debug(RIG_DEBUG_VERBOSE, "%s called\n", __func__);

	if (!rig || !trn)
		return -RIG_EINVAL;

	char trnbuf[6];
	int retval;

	retval = kenwood_safe_transaction(rig, "AI", trnbuf, 6, 4);
	if (retval != RIG_OK)
		return retval;

	*trn = trnbuf[2] != '0' ? RIG_TRN_RIG : RIG_TRN_OFF;

	return RIG_OK;
}

/*
 * kenwood_set_powerstat
 */
int kenwood_set_powerstat(RIG *rig, powerstat_t status)
{
	rig_debug(RIG_DEBUG_VERBOSE, "%s called\n", __func__);

	if (!rig)
		return -RIG_EINVAL;

	return kenwood_simple_transaction(rig,
		(status == RIG_POWER_ON) ? "PS1" : "PS0", 4);
}

/*
 * kenwood_get_powerstat
 */
int kenwood_get_powerstat(RIG *rig, powerstat_t *status)
{
	rig_debug(RIG_DEBUG_VERBOSE, "%s called\n", __func__);

	if (!rig || !status)
		return -RIG_EINVAL;

	char pwrbuf[6];
	int retval;

	retval = kenwood_safe_transaction(rig, "PS", pwrbuf, 6, 4);
	if (retval != RIG_OK)
		return retval;

	*status = pwrbuf[2] == '0' ? RIG_POWER_OFF : RIG_POWER_ON;

	return RIG_OK;
}

/*
 * kenwood_reset
 */
int kenwood_reset(RIG *rig, reset_t reset)
{
	rig_debug(RIG_DEBUG_VERBOSE, "%s called\n", __func__);

	if (!rig)
		return -RIG_EINVAL;

	char rstbuf[6];
	char rst;

	switch(reset) {
	case RIG_RESET_VFO:
		rst = '1';
		break;

	case RIG_RESET_MASTER:
		rst = '2';
		break;

	default:
		rig_debug(RIG_DEBUG_ERR, "%s: unsupported reset %d\n",
					__func__, reset);
		return -RIG_EINVAL;
	}

	sprintf(rstbuf, "SR%c", rst);

	/* this command has no answer */
	return kenwood_simple_cmd(rig, rstbuf);
}

/*
 * kenwood_send_morse
 */
int kenwood_send_morse(RIG *rig, vfo_t vfo, const char *msg)
{
	rig_debug(RIG_DEBUG_VERBOSE, "%s called\n", __func__);

	if (!rig || !msg)
		return -RIG_EINVAL;

	char morsebuf[30], m2[30];
	int msg_len, buff_len, retval;
	const char *p;

	p = msg;
	msg_len = strlen(msg);

	while(msg_len > 0) {
		/*
		 * TODO: check with "KY" if char buffer is available.
		 * 		if not, sleep.
		 *
		 * Make the total message segments 28 characters
		 * in length because Kenwood demands it.
		 * Spaces fill in the message end.
		 */
		buff_len = msg_len > 24 ? 24 : msg_len;

		strncpy(m2, p, 24);
		m2[24] = '\0';

		/* the command must consist of 28 bytes */
		sprintf(morsebuf, "KY %-24s", m2);
		retval = kenwood_simple_cmd(rig, morsebuf);
		if (retval != RIG_OK)
			return retval;

		msg_len -= buff_len;
		p += buff_len;
	}

	return RIG_OK;
}

/*
 * kenwood_vfo_op
 */
int kenwood_vfo_op(RIG *rig, vfo_t vfo, vfo_op_t op)
{
	rig_debug(RIG_DEBUG_VERBOSE, "%s called\n", __func__);

	if (!rig)
		return -RIG_EINVAL;

	switch(op) {
	case RIG_OP_UP:
		return kenwood_simple_cmd(rig, "UP");

	case RIG_OP_DOWN:
		return kenwood_simple_cmd(rig, "DN");

	case RIG_OP_BAND_UP:
		return kenwood_simple_cmd(rig, "BU");

	case RIG_OP_BAND_DOWN:
		return kenwood_simple_cmd(rig, "BD");

	default:
		rig_debug(RIG_DEBUG_ERR, "%s: unsupported op %#x\n",
					__func__, op);
		return -RIG_EINVAL;
	}
}

/*
 * kenwood_set_mem
 */
int kenwood_set_mem(RIG *rig, vfo_t vfo, int ch)
{
	rig_debug(RIG_DEBUG_VERBOSE, "%s called\n", __func__);

	if (!rig)
		return -RIG_EINVAL;

	char buf[8];
	/*
	 * "MCbmm;"
	 * where b is the bank number, mm the memory number.
	 * b can be a space
	 */
	sprintf(buf, "MC %02d", ch);

	return kenwood_simple_cmd(rig, buf);
}

/*
 * kenwood_get_mem
 */
int kenwood_get_mem(RIG *rig, vfo_t vfo, int *ch)
{
	rig_debug(RIG_DEBUG_VERBOSE, "%s called\n", __func__);

	if (!rig || !ch)
		return -RIG_EINVAL;

	char membuf[10];
	int retval;

	/*
	 * "MCbmm;"
	 * where b is the bank number, mm the memory number.
	 * b can be a space
	 */

	retval = kenwood_safe_transaction(rig, "MC", membuf, 10, 6);
	if (retval != RIG_OK)
		return retval;

	*ch = atoi(membuf+2);

	return RIG_OK;
}

int kenwood_get_mem_if(RIG *rig, vfo_t vfo, int *ch)
{
	rig_debug(RIG_DEBUG_VERBOSE, "%s called\n", __func__);

	if (!rig || !ch)
		return -RIG_EINVAL;

	int err;
	char buf[4];
	struct kenwood_priv_data *priv = rig->state.priv;

	err = kenwood_get_if(rig);
	if (err != RIG_OK)
		return err;

	memcpy(buf, &priv->info[26], 2);
	buf[2] = '\0';

	*ch = atoi(buf);

	return RIG_OK;
}

int kenwood_get_channel(RIG *rig, channel_t *chan)
{
	rig_debug(RIG_DEBUG_VERBOSE, "%s called\n", __func__);

	if (!rig || !chan)
		return -RIG_EINVAL;

	int err;
	char buf[26];
	char cmd[8];
	struct kenwood_priv_caps *caps = kenwood_caps(rig);

	/* put channel num in the command string */
	sprintf(cmd, "MR0 %02d", chan->channel_num);

	err = kenwood_safe_transaction(rig, cmd, buf, 26, 24);
	if (err != RIG_OK)
		return err;

	memset(chan, 0x00, sizeof(channel_t));

	chan->vfo = RIG_VFO_VFO;

	/* MR0 1700005890000510   ;
	 * MRs ccfffffffffffMLTtt ;
	 */

	/* parse from right to left */

	/* XXX based on the available documentation, there is no command
	 * to read out the filters of a given memory channel. The rig, however,
	 * stores this information.
	 */

	if (buf[19] == '0' || buf[19] == ' ')
		chan->ctcss_tone = 0;
	else {
		buf[22] = '\0';
		if (rig->caps->ctcss_list)
			chan->ctcss_tone = rig->caps->ctcss_list[atoi(&buf[20])];
	}

	/* memory lockout */
	if (buf[18] == '1')
		chan->flags |= RIG_CHFLAG_SKIP;

	chan->mode = kenwood2rmode(buf[17] - '0', caps->mode_table);

	buf[17] = '\0';
	chan->freq = atoi(&buf[6]);

	if (chan->freq == RIG_FREQ_NONE)
		return -RIG_ENAVAIL;

	buf[6] = '\0';
	chan->channel_num = atoi(&buf[4]);


	/* split freq */
	cmd[2] = '1';
	err = kenwood_safe_transaction(rig, cmd, buf, 26, 24);
	if (err != RIG_OK)
		return err;

	chan->tx_mode = kenwood2rmode(buf[17] - '0', caps->mode_table);

	buf[17] = '\0';
	chan->tx_freq = atoi(&buf[6]);

	if (chan->freq == chan->tx_freq) {
		chan->tx_freq = RIG_FREQ_NONE;
		chan->tx_mode = RIG_MODE_NONE;
		chan->split = RIG_SPLIT_OFF;
	} else
		chan->split = RIG_SPLIT_ON;

	return RIG_OK;
}

int kenwood_set_channel(RIG *rig, const channel_t *chan)
{
	rig_debug(RIG_DEBUG_VERBOSE, "%s called\n", __func__);

	if (!rig || !chan)
		return -RIG_EINVAL;

	char buf[26];
	char mode, tx_mode = 0;
	int err;
	int tone = 0;

	struct kenwood_priv_caps *caps = kenwood_caps(rig);

	mode = rmode2kenwood(chan->mode, caps->mode_table);
		if (mode < 0 ) {
				rig_debug(RIG_DEBUG_ERR, "%s: unsupported mode '%s'\n",
								   __func__, rig_strrmode(chan->mode));
				return -RIG_EINVAL;
		}

	if (chan->split == RIG_SPLIT_ON) {
		tx_mode = rmode2kenwood(chan->tx_mode, caps->mode_table);
			if (tx_mode < 0 ) {
					rig_debug(RIG_DEBUG_ERR, "%s: unsupported mode '%s'\n",
	   							   __func__, rig_strrmode(chan->tx_mode));
					return -RIG_EINVAL;
			}

	}

	/* find tone */
	if (chan->ctcss_tone) {

		for (tone = 0; rig->caps->ctcss_list[tone] != 0; tone++) {
			if (chan->ctcss_tone == rig->caps->ctcss_list[tone])
				break;
		}

		if (chan->ctcss_tone != rig->caps->ctcss_list[tone])
			tone = 0;
	}

	sprintf(buf, "MW0 %02d%011d%c%c%c%02d ", /* note the space at the end */
		chan->channel_num,
		(int) chan->freq,
		'0' + mode,
		(chan->flags & RIG_CHFLAG_SKIP) ? '1' : '0',
		chan->ctcss_tone ? '1' : '0',
		chan->ctcss_tone ? (tone + 1) : 0);

	err = kenwood_simple_cmd(rig, buf);
	if (err != RIG_OK)
		return err;

	sprintf(buf, "MW1 %02d%011d%c%c%c%02d ",
		chan->channel_num,
		(chan->split == RIG_SPLIT_ON) ? ((int) chan->tx_freq) : 0,
		(chan->split == RIG_SPLIT_ON) ? ('0' + tx_mode) : '0',
		(chan->flags & RIG_CHFLAG_SKIP) ? '1' : '0',
		chan->ctcss_tone ? '1' : '0',
		chan->ctcss_tone ? (tone + 1) : 0);

	return kenwood_simple_cmd(rig, buf);
}

int kenwood_set_ext_parm(RIG *rig, token_t token, value_t val)
{
	rig_debug(RIG_DEBUG_VERBOSE, "%s called\n", __func__);

	if (!rig)
		return -RIG_EINVAL;

	char buf[4];

	switch (token) {
	case TOK_VOICE:
		return kenwood_simple_cmd(rig, "VR");

	case TOK_FINE:
		sprintf(buf, "FS%c", (val.i == 0) ? '0' : '1');
		return kenwood_simple_cmd(rig, buf);

	case TOK_XIT:
		sprintf(buf, "XT%c", (val.i == 0) ? '0' : '1');
		return kenwood_simple_cmd(rig, buf);

	case TOK_RIT:
		sprintf(buf, "RT%c", (val.i == 0) ? '0' : '1');
		return kenwood_simple_cmd(rig, buf);
	}

	return -RIG_EINVAL;
}

int kenwood_get_ext_parm(RIG *rig, token_t token, value_t *val)
{
	rig_debug(RIG_DEBUG_VERBOSE, "%s called\n", __func__);

	if (!rig || !val)
		return -RIG_EINVAL;

	int err;
	struct kenwood_priv_data *priv = rig->state.priv;

	switch (token) {
	case TOK_FINE:
		return get_kenwood_func(rig, "FS", &val->i);

	case TOK_XIT:
		err = kenwood_get_if(rig);
		if (err != RIG_OK)
			return err;

		val->i = (priv->info[24] == '1') ? 1 : 0;
		return RIG_OK;

	case TOK_RIT:
		err = kenwood_get_if(rig);
		if (err != RIG_OK)
			return err;

		val->i = (priv->info[23] == '1') ? 1 : 0;
		return RIG_OK;
	}

	return -RIG_ENIMPL;
}

/*
 * kenwood_get_info
 * supposed to work only for TS2000...
 */
const char* kenwood_get_info(RIG *rig)
{
	rig_debug(RIG_DEBUG_VERBOSE, "%s called\n", __func__);

	if (!rig)
		return "*rig == NULL";

	char firmbuf[10];
	int retval;

	retval = kenwood_safe_transaction(rig, "TY", firmbuf, 10, 6);
	if (retval != RIG_OK)
		return NULL;

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
 *
 * rig_model_t probeallrigs_kenwood(port_t *port, rig_probe_func_t cfunc, rig_ptr_t data)
 */
DECLARE_PROBERIG_BACKEND(kenwood)
{
	rig_debug(RIG_DEBUG_VERBOSE, "%s called\n", __func__);

	char idbuf[IDBUFSZ];
	int id_len=-1, i, k_id;
	int retval=-1;
	int rates[] = { 115200, 57600, 38400, 19200, 9600, 4800, 1200, 0 };	/* possible baud rates */
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
		id_len = read_string(port, idbuf, IDBUFSZ, ";\r", 2);
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
		rig_debug(RIG_DEBUG_VERBOSE, "probe_kenwood: protocol error, "
					" expected %d, received %d: %s\n",
					6, id_len, idbuf);
		return RIG_MODEL_NONE;
	}


	/* first, try ID string */
	for (i=0; kenwood_id_string_list[i].model != RIG_MODEL_NONE; i++) {
		if (!strncmp(kenwood_id_string_list[i].id, idbuf+2, 16)) {
			rig_debug(RIG_DEBUG_VERBOSE, "probe_kenwood: "
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
			id_len = read_string(port, idbuf, IDBUFSZ, ";\r", 2);
		close(port->fd);
		if (retval != RIG_OK)
			return RIG_MODEL_NONE;
		/*
		 * reply should be something like 'K2n;'
		 */
		if (id_len == 4 || !strcmp(idbuf, "K2")) {
			rig_debug(RIG_DEBUG_VERBOSE, "probe_kenwood: found K2\n");
			if (cfunc)
				(*cfunc)(port, RIG_MODEL_K2, data);
			return RIG_MODEL_K2;
		}
	}

	for (i=0; kenwood_id_list[i].model != RIG_MODEL_NONE; i++) {
		if (kenwood_id_list[i].id == k_id) {
			rig_debug(RIG_DEBUG_VERBOSE, "probe_kenwood: "
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
	rig_debug(RIG_DEBUG_WARN, "probe_kenwood: found unknown device "
				"with ID %03d, please report to Hamlib "
				"developers.\n", k_id);

	return RIG_MODEL_NONE;
}


/*
 * initrigs_kenwood is called by rig_backend_load
 */
DECLARE_INITRIG_BACKEND(kenwood)
{
	rig_debug(RIG_DEBUG_VERBOSE, "%s called\n", __func__);

//	rig_debug(RIG_DEBUG_VERBOSE, "kenwood: _init called\n");

	rig_register(&ts950sdx_caps);
	rig_register(&ts50s_caps);
	rig_register(&ts140_caps);
	rig_register(&ts450s_caps);
	rig_register(&ts570d_caps);
	rig_register(&ts570s_caps);
	rig_register(&ts680s_caps);
	rig_register(&ts690s_caps);
	rig_register(&ts790_caps);
	rig_register(&ts850_caps);
	rig_register(&ts870s_caps);
	rig_register(&ts930_caps);
	rig_register(&ts2000_caps);
	rig_register(&trc80_caps);
	rig_register(&k2_caps);
	rig_register(&k3_caps);

	rig_register(&ts440_caps);
	rig_register(&ts940_caps);
	rig_register(&ts711_caps);
	rig_register(&ts811_caps);
	rig_register(&r5000_caps);

	rig_register(&tmd700_caps);
	rig_register(&thd7a_caps);
	rig_register(&thd72a_caps);
	rig_register(&thf7e_caps);
	rig_register(&thg71_caps);
	rig_register(&tmv7_caps);
	rig_register(&tmd710_caps);

	rig_register(&ts590_caps);
	rig_register(&ts590_caps);
	rig_register(&ts480_caps);
	rig_register(&thf6a_caps);

	rig_register(&transfox_caps);

	return RIG_OK;
}
