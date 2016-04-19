/*
 *  Hamlib Kenwood backend - TS2000 description
 *  Copyright (c) 2000-2002 by Stephane Fillod
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

/*
 * This code is has been substatiantially altered from the original
 * author's.  Also, many new functions have been added and are
 * (C) Copyrighted 2002 by Dale E. Edmons (KD7ENI).  The license
 * is unchanged, and fitness disclaimers still apply.
 */

/*
 * Copied kenwood.c to end of this file.  When I change a function for the
 * ts2000 I will already have the kenwood version with the function prefix
 * changed to ts2k.  Unique functions will be pointed to in this file,
 * whereas the unmodified version will be in kenwood.c.  This simplifies
 * things during development and unused ts2k_() functions should go away
 * as soon as possible.
 *
 *	Dale KD7ENI
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <ctype.h>
#include <hamlib/rig.h>
#include "kenwood.h"
#include "ts2k.h"


/*
 * I just read in kenwood.c and will modify the functions here.  This way,
 * kenwood functions that actually work on other rigs won't be broken by
 * my hacks.  Anything that works for all can be sent back over to kenwood.c
 *
 * Note: due to my compulsive laziness, I often abbreviate Kenwood TS-2000
 *	as simply ts2k, especially for code!
 *
 * Dale kd7eni
 */

/*
 *  Hamlib Kenwood backend - main file
 *  Copyright (c) 2000-2002 by Stephane Fillod
 *
 */


#include <stdlib.h>
#include <stdio.h>		/* Standard input/output definitions */
#include <string.h>		/* String function definitions */
#include <unistd.h>		/* UNIX standard function definitions */
#include <fcntl.h>		/* File control definitions */
#include <errno.h>		/* Error number definitions */
#include <termios.h>		/* POSIX terminal control definitions */
#include <sys/ioctl.h>
#include <math.h>

#include <serial.h>
#include <misc.h>


// Added the following two lists --Dale, kd7eni
// FIXME: RIG_MODE_[FSKR|CWR] undefined in rig.h
const int ts2k_mode_list[] = {
	RIG_MODE_NONE, RIG_MODE_LSB, RIG_MODE_USB, RIG_MODE_CW,
	RIG_MODE_FM, RIG_MODE_AM, RIG_MODE_RTTY, RIG_MODE_CW,
	RIG_MODE_RTTY
};

long int ts2k_steps[2][10] = {
	{1000, 2500, 5000, 10000, 0, 0, 0, 0, 0, 0},	// ssb, cw, fsk
	{5000, 6250, 10000, 12500, 15000, 20000,
	 25000, 30000, 50000, 100000}	// am/fm
};


struct ts2k_id {
	rig_model_t model;
	int id;
};

struct ts2k_id_string {
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
static const struct ts2k_id ts2k_id_list[] = {
	{RIG_MODEL_R5000, 5},
	{RIG_MODEL_TS870S, 15},
	{RIG_MODEL_TS570D, 17},
	{RIG_MODEL_TS570S, 18},
	{RIG_MODEL_TS2000, 19},	/* correct --kd7eni */
	{RIG_MODEL_NONE, UNKNOWN_ID},	/* end marker */
};

static const struct ts2k_id_string ts2k_id_string_list[] = {
	{RIG_MODEL_THD7A, "TH-D7"},
	{RIG_MODEL_THD7AG, "TH-D7G"},
	{RIG_MODEL_THF6A, "TH-F6"},
	{RIG_MODEL_THF7E, "TH-F7"},
	{RIG_MODEL_NONE, NULL},	/* end marker */
};


/*
 * 38 CTCSS sub-audible tones  (17500 invalid for ctcss --kd7eni)
  */
const int ts2k_ctcss_list[] = {
	670, 719, 744, 770, 797, 825, 854, 885, 915, 948,
	974, 1000, 1035, 1072, 1109, 1148, 1188, 1230, 1273, 1318,
	1365, 1413, 1462, 1514, 1567, 1622, 1679, 1738, 1799, 1862,
	1928, 2035, 2107, 2181, 2257, 2336, 2418, 2503, // 17500,
	/* Note: 17500 is not available as ctcss, only tone. --kd7eni */
	0,
};

#define cmd_trm(rig) ((struct ts2k_priv_caps *)(rig)->caps->priv)->cmdtrm
#define ta_quit	rs->hold_decode = 0; return retval

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

/* FIXME: cmd_len appears to change and needs set every invocation? --Dale */

int
ts2k_transaction(RIG * rig, const char *cmdstr, int cmd_len,
		 char *data, size_t * datasize)
{
//      return kenwood_transaction(rig, cmdstr, data, datasize);

	struct rig_state *rs;
	int retval;
	const char *cmdtrm = EOM_KEN;	/* Default Command/Reply termination char */
	int retry_read = 0;
	char *errtxt;

#define MAX_RETRY_READ 5

	rs = &rig->state;
	rs->hold_decode = 1;

	serial_flush(&rs->rigport);

	cmdtrm = cmd_trm(rig);

	if (cmdstr != NULL) {
		//      rig_debug(RIG_DEBUG_ERR, __func__": 1) sending '%s'\n\n", cmdstr);
		retval = write_block(&rs->rigport, cmdstr, strlen(cmdstr));
		if (retval != RIG_OK)
			{ ta_quit; }
#undef	TH_ADD_CMDTRM
#ifdef	TH_ADD_CMDTRM
		//      rig_debug(RIG_DEBUG_ERR, __func__": 2) sending '%s'\n\n", cmdtrm);
		retval = write_block(&rs->rigport, cmdtrm, strlen(cmdtrm));
		if (retval != RIG_OK)
			{ ta_quit; }
#endif
	}

/* FIXME: Everything below this line wants to be completely rewritten!!!! */

	if (data == NULL || datasize <= 0) {
		rig->state.hold_decode = 0;
		return RIG_OK;	/* don't want a reply */
	}

      transaction_read:
	/* FIXME : TS-2000 gets alot of 'timedout' on read_string()! */
	//rig_debug(RIG_DEBUG_ERR, __func__": 3a) reading %u bytes...\n", *datasize);
	retval =
	    read_string(&rs->rigport, data, *datasize, cmdtrm,
			strlen(cmdtrm));
	//rig_debug(RIG_DEBUG_ERR, __func__": 3b) read '%s', retval=%u\n\n", data, retval);
	*datasize = retval;
	if (retval > 0) {
		//rig_debug(RIG_DEBUG_ERR, __func__": 3b) read cmd '%s', retval=%u\n\n", data, retval);
		retval = RIG_OK;
		goto transaction_check;
	}

	/* Check that command termination is correct */
	if (!strchr(cmdtrm, data[strlen(data)])) {
		if (retry_read++ < MAX_RETRY_READ)
			goto transaction_read;
		rig_debug(RIG_DEBUG_ERR,
			  __func__
			  ": Command is not correctly terminated '%s'\n",
			  data);
		retval = -RIG_EPROTO;
		ta_quit;
	}

	/* Errors */
	if (strlen(data) == 2 && data[0] == 'E') {
		switch (data[0]) {
		case 'E':
			rig_debug(RIG_DEBUG_ERR,
				  __func__
				  ": Communication Error for '%s'\n",
				  cmdstr);
			break;

		case 'O':
			rig_debug(RIG_DEBUG_ERR,
				  __func__
				  ": Communication Error for '%s'\n",
				  cmdstr);
			break;

		case '?':
			rig_debug(RIG_DEBUG_ERR,
				  __func__
				  ": Communication Error for '%s'\n",
				  cmdstr);
			break;

		default:
			rig_debug(RIG_DEBUG_ERR,
				  __func__ ": Hamlib Error for '%s'\n",
				  cmdstr);
			break;

		}
		retval = -RIG_ERJCTED;
		ta_quit;
	}
#undef	CONFIG_STRIP_CMDTRM
#ifdef	CONFIG_STRIP_CMDTRM
	if (strlen(data) > 0)
		data[strlen(data) - 1] = '\0';	/* not very elegant, but should work. */
	else
		data[0] = '\0';
#endif
	/*
	 * Check that received the correct reply. The first two characters
	 * should be the same as command.
	 */
      transaction_check:
	if (cmdstr && (toupper(data[0]) != toupper(cmdstr[0])
		       || toupper(data[1]) != toupper(cmdstr[1]))) {
		/*
		 * TODO: When RIG_TRN is enabled, we can pass the string
		 *       to the decoder for callback. That way we don't ignore
		 *       any commands.
		 */
		if (retry_read++ < MAX_RETRY_READ)
			goto transaction_read;

		rig_debug(RIG_DEBUG_ERR,
			  __func__ ": Unexpected reply '%s'\n", data);
		retval = -RIG_EPROTO;
		{ ta_quit; }
	}

	retval = RIG_OK;

      transaction_quit:
	{ ta_quit; }
}

/*
 * kenwood_set_vfo
 * Assumes rig!=NULL
 *
 *	status:		VFOA, VFOB, VFOC, Main, Sub,
 *			MEMA, MEMC, CALLA, CALLC
 *			VFO_AB, VFO_BA, ...
 *		They all work!		--Dale
 */
int ts2k_set_vfo(RIG * rig, vfo_t vfo)
{
	unsigned char cmdbuf[10];
	int ptt, ctrl, v,cmd_len, retval;
	static int sat_on;	// temporary! to be removed!
	char vfo_function;

	// trivial case, but needs checked if enabled
/*//	if( (vfo == RIG_CTRL_MODE(RIG_CTRL_MAIN,RIG_VFO_ALL))
//		|| (vfo == RIG_CTRL_MODE(RIG_CTRL_SUB,RIG_VFO_ALL)) ) {
//		rig_debug(RIG_DEBUG_ERR, __func__ \
//			  ": Geez, you can't set *all* VFO's!\n");
//		return -RIG_EINVAL;
//	}
*/
	cmd_len = 10;
	ptt = ctrl = v = 0;

	// be optimistic (and ensure initialization)
	retval = RIG_OK;

	// Main/Sub Active Transceiver
	switch (vfo) {
	case RIG_VFO_A:
	case RIG_VFO_B:
	case RIG_VFO_AB:	// split
	case RIG_VFO_BA:
	case RIG_CTRL_SAT:	// FIXME: Not even close to correct
	case RIG_VFO_MAIN:
	case RIG_VFO_MEM_A:
	case RIG_VFO_CALL_A:
		ctrl = TS2K_CTRL_ON_MAIN;	// FIXME : these are independent!
		ptt = TS2K_PTT_ON_MAIN;
		break;
	case RIG_VFO_C:
	case RIG_VFO_SUB:
	case RIG_VFO_MEM_C:
	case RIG_VFO_CALL_C:
		ctrl = TS2K_CTRL_ON_SUB;
		ptt = TS2K_PTT_ON_SUB;
		break;

	default:
		break;
	}

	// set now so "ft...;" and "fr...;" don't fail
	retval = ts2k_set_ctrl(rig, ptt, ctrl);
	if (retval != RIG_OK)
		return -RIG_EINVAL;

	// check if we need to skip the remainder
	v  = (vfo == RIG_VFO_SUB)
		|| (vfo == RIG_VFO_MAIN)
		|| (vfo == RIG_VFO_CURR)
		|| (vfo == RIG_VFO_VFO)
//		|| (vfo == RIG_VFO_ALL)		// yea, I know
		/* bit mask checks */
		|| (vfo & RIG_CTRL_SAT)	// "fr...;", "ft...;" won't do!
		;

	rig_debug(RIG_DEBUG_ERR, __func__ \
		  ": starting check.... vfo = 0x%X, v=%d\n", vfo, v);

	if (!v) {	// start check

		// FIXME: this is a speed-up kludge but won't *always* work!
		if(sat_on) {
			retval = ts2k_sat_off(rig, vfo);	// we gotta do it.
			if(retval != RIG_OK)
				return retval;
			sat_on = 0;
		}

		// RX Active Tuning
		switch (vfo) {
		case RIG_VFO_AB:	// TX is opposite
		case RIG_VFO_A:
		case RIG_VFO_C:
			vfo_function = '0';
			break;
		case RIG_VFO_BA:	// TX is opposite
		case RIG_VFO_B:
			vfo_function = '1';
			break;
		case RIG_VFO_MEM_A:
		case RIG_VFO_MEM_C:
			vfo_function = '2';
			break;
		case RIG_VFO_CALL_A:
		case RIG_VFO_CALL_C:
			vfo_function = '3';
			break;

		default:
			rig_debug(RIG_DEBUG_ERR, __func__
				  ": unsupported VFO %u\n", vfo);
			return -RIG_EINVAL;
			break;
		}

		// ack_len is tmp
		cmd_len =
		    sprintf(cmdbuf, "fr%c%s", vfo_function, cmd_trm(rig));

		/* set RX VFO */
		retval =
		    ts2k_transaction(rig, cmdbuf, cmd_len, NULL, NULL);
		if (retval != RIG_OK)
			return -RIG_EINVAL;

		// TX Active tuning
		switch (vfo) {
		case RIG_VFO_A:
		case RIG_VFO_C:
		case RIG_VFO_BA:	// opposite of above
			vfo_function = '0';
			break;
		case RIG_VFO_AB:	// opposite of above
		case RIG_VFO_B:
			vfo_function = '1';
			break;
		case RIG_VFO_MEM_A:
		case RIG_VFO_MEM_C:	// FIXME: need to handle vfo/mem split
			vfo_function = '2';
			break;
		case RIG_VFO_CALL_A:
		case RIG_VFO_CALL_C:
			vfo_function = '3';
			break;

		default:
			rig_debug(RIG_DEBUG_ERR, __func__
				  ": unsupported VFO %u\n",
				  vfo);
			return -RIG_EINVAL;
		}

		/* set TX VFO */
		cmdbuf[1] = 't';

		// removing this causes split to not function!!!!
		cmd_len =
		    sprintf(cmdbuf, "ft%c%s", vfo_function, cmd_trm(rig));

		retval =
		    ts2k_transaction(rig, cmdbuf, cmd_len, NULL, NULL);
		if (retval != RIG_OK)
			return retval;

	} else {	// Check further for special modes not using "fr...;", "ft...;"
		if(vfo & RIG_CTRL_SAT) {	// test the SAT bit
			retval = ts2k_sat_on(rig, vfo);
				if (retval != RIG_OK)
					return retval;
			sat_on = 1;
		} else {
			rig_debug(RIG_DEBUG_ERR, __func__ \
				  ": VFO not changed, only PTT/CTRL\n");
		}
	}
/*
 * FIXME: some items like scan, satellite need checked and turned off here.
 *	I've got a simple kludge to turn SAT *off* but it wants to be done
 *	here.  It's a bit expensive though and I'm trying to find a better
 *	way to do SAT as well as others.  ts2k_sat_off() reads the current,
 *	sets it off, then writes it back so the user selected stuff don't
 *	change unexpectedly.  Now, SAT won't get turned off if first turned
 *	on via the front panel.  MEM and SCAN have similar quirks.
 *							--Dale
 */
	return retval;
}

/* we just turn SAT on here. It'll take some doing to run it! */
int ts2k_sat_on(RIG *rig, vfo_t vfo)
{
	char cmd[20], ack[20];
	int cmdlen, acklen;

	acklen = 20;

	if(!(vfo & RIG_CTRL_SAT))
		return -RIG_EINTERNAL;	// All right.  Who called us!?

//	cmdlen = sprintf(cmd, "sa%07u;", 0);	// Initial string to modify
	acklen = ts2k_transaction(rig, "sa;", 3, ack, &acklen);

	// Sat mode ON
	ack[2] = '1';	// Everything below is *nice*, this is *required*

	goto STest;	// testing

/* cmd is already full of '0's, but we set them again explicitly */
	// SAT_VFO or SAT_MEM?
	if(vfo & RIG_CTRL_MEM)
		ack[8] = '1';	// sat mem ch 0-9
	else
		ack[8] = '0';	// sat vfo

	/* Main or Sub as uplink? */
	// Note: if both are set, Main is still uplink
	if(vfo & RIG_CTRL_MAIN) {
		ack[4] = '0';	// sat main=uplink
	} else if(vfo & RIG_CTRL_SUB) {
		ack[4] = '1';	// sat sub=uplink
	}

	// FIXME: Add Sat Trace here!

	// Trace REV
	if(vfo & RIG_CTRL_REV)
		ack[7] = '1';	// sat trace REV
	else
		ack[7] = '0';

	// CTRL to main or sub?
	if ((vfo & RIG_VFO_CTRL) && (vfo & RIG_CTRL_SUB))
		ack[5] = '1';	// sat CTRL on sub
	else
		ack[5] = '0';	// sat CTRL on main

STest:
	rig_debug(RIG_DEBUG_ERR, __func__ \
		  ": sat = %s, vfo = 0x%X\n", cmd, vfo);
	// of coure, this is *required* too!
	return ts2k_transaction(rig, ack, acklen, NULL, NULL);
}

int ts2k_sat_off(RIG *rig, vfo_t vfo)
{
	char cmd[20], ack[20];
	int cmdlen, acklen, retval;

	acklen = 10;

	cmdlen = sprintf(cmd, "sa;");
	retval = ts2k_transaction(rig, cmd, cmdlen, ack, &acklen);
	if(retval != RIG_OK)
		return retval;

	ack[2] = '0';
	cmdlen = 20;
	return ts2k_transaction(rig, ack, acklen, NULL, NULL);
}

/*
 * kenwood_get_vfo_if
 * Assumes rig!=NULL, !vfo
 *
 *	status:	works perfect for implemented modes!  --Dale
 *		code's getting a little ugly.  okay, real ugly.
 */
int ts2k_get_vfo(RIG * rig, vfo_t * vfo)
{
	char vfobuf[50], r_vfo;
	char *ctrl_ptt;
	int vfo_len, retval, tmp;

	/*
	 * FIXME: if FR != FT && rcvr==main, the mode = split!  --kd7eni
	 */

	// Check which receiver so VFO_C may be detected (PTT/CTRL)

	ctrl_ptt = ts2k_get_ctrl(rig);
	if (ctrl_ptt == NULL)
		return -RIG_EINVAL;
//	rig_debug(RIG_DEBUG_ERR, "ts2k_get_vfo: PTT/CTRL is %s\n", ctrl_ptt);

	/* query RX VFO */
	vfo_len = 50;
	rig_debug(RIG_DEBUG_ERR, __func__
		": sending fr; cmd/checking SAT.  Expect TIMEDOUT if in SAT mode!\n");
	retval = ts2k_transaction(rig, "fr;", 3, vfobuf, &vfo_len);

							/* "fr;" fails in satellite mode; interesting... */
	if (retval != RIG_OK) {
		rig_debug(RIG_DEBUG_ERR, __func__": kenwood/ts2k.c\n"
			"FIXME: ts2k.c,\tThis is timeout cannot be prevented.\n");
		tmp = retval;
		retval = ts2k_transaction(rig, "sa;", 3, vfobuf, &vfo_len);
		if (retval == RIG_OK) {
			rig_debug(RIG_DEBUG_ERR, __func__": SAT=%s\n", vfobuf);
			if(vfobuf[2] == '1') {
							/* yes, we're in satellite mode! */
				*vfo = RIG_CTRL_SAT;	// FIXME: set the rest!
							/* TODO: write get_sat() and let it do the work */
				return RIG_OK;
			}
		}
		return tmp;	// return original "fr;" error!
	}

//	rig_debug(RIG_DEBUG_ERR, __func__": checking fr; cmd.\n");
	r_vfo = vfobuf[2];

	//if (vfo_len != 4 || vfobuf[1] != 'R') {
	if (vfobuf[1] != 'R') {
		rig_debug(RIG_DEBUG_ERR, __func__
			  ": unexpected answer %s, "
			  "len=%u\n", vfobuf, vfo_len);
		return -RIG_ERJCTED;
	}

	rig_debug(RIG_DEBUG_ERR, __func__": sending ft; cmd.\n");
	vfo_len = 50;
	retval = ts2k_transaction(rig, "ft;", 3, vfobuf, &vfo_len);
	if (retval != RIG_OK)
		return retval;

	rig_debug(RIG_DEBUG_ERR, __func__": checking ft; cmd.\n");
	if (vfobuf[2] == r_vfo) {	// check most common first
		rig_debug(RIG_DEBUG_ERR, "ts2k_get_vfo: Non-Split.\n");

		/* TODO: replace 0,1,2,.. constants by defines */
		/* FIXME: return based on RIG_PTT_ON_???? or RIG_CTRL_ON_????
		 * may be different.  We need to specify actual status.
		 * Right now we pretend things are simpler. --kd7eni
		 */
		switch (vfobuf[2]) {
		case '0':
			if (ctrl_ptt[3] == '0')	// we use CTRL as Active Transceiver
				*vfo = RIG_VFO_A;
			else if (ctrl_ptt[3] == '1')
				*vfo = RIG_VFO_C;
			else {	// "There be errors here!"
				rig_debug(RIG_DEBUG_ERR,
					  "ts2k_get_vfo: VFO1 on erroneous xcvr %c\n",
					  vfobuf[2]);
				return -RIG_EPROTO;
			}
			break;
		case '1':
			// only valid on Main--no checks required.
			*vfo = RIG_VFO_B;
			break;
		case '2':
			if (ctrl_ptt[3] == '0')	// we use CTRL as Active Transceiver.
				*vfo = RIG_VFO_MEM_A;
			else if (ctrl_ptt[3] == '1')
				*vfo = RIG_VFO_MEM_C;
			else
				return -RIG_EPROTO;
			break;
		case '3':
			if (ctrl_ptt[3] == '0')	// we use CTRL as current
				*vfo = RIG_VFO_CALL_A;
			else if (ctrl_ptt[3] == '1')	// we use CTRL as current
				*vfo = RIG_VFO_CALL_C;
			else
				return -RIG_EPROTO;
			break;

		default:	// Different or newer rig types...
			rig_debug(RIG_DEBUG_ERR,
				  "ts2k_get_vfo: unsupported VFO %c\n",
				  vfobuf[2]);
			return -RIG_EPROTO;

		}		// end switch
	} else {		// end rx == tx; start split checks.
		rig_debug(RIG_DEBUG_ERR, "ts2k_get_vfo: Split.\n");

		if (r_vfo == '0' && vfobuf[2] == '1')
			*vfo = RIG_VFO_AB;
		else if (r_vfo == '1' && vfobuf[2] == '0')
			*vfo = RIG_VFO_BA;
		else {		// FIXME: need vfo <--> mem split
			rig_debug(RIG_DEBUG_ERR,
				  __func__
				  ":FIXME: vfo<->mem split! -kd7eni!\n");
			return -RIG_EPROTO;
		}
	}

	return RIG_OK;
}

/*
 * kenwood_set_freq
 * Assumes rig!=NULL
 *
 *	status:	correctly sets FA, FB, FC	--Dale
 */
int ts2k_set_freq(RIG * rig, vfo_t vfo, freq_t freq)
{
	unsigned char freqbuf[16];
	int freq_len, ack_len = 0, retval;
	char vfo_letter;

	/*
	 * better FIXME: vfo==RIG_VFO_CURR
	 */
	if (vfo == RIG_VFO_CURR) {
		retval = ts2k_get_vfo(rig, &vfo);
		if (retval != RIG_OK)
			return retval;
	}

	switch (vfo) {
	case RIG_VFO_A:
		vfo_letter = 'A';
		break;
	case RIG_VFO_B:
		vfo_letter = 'B';
		break;
	case RIG_VFO_C:
		vfo_letter = 'C';
		break;
	default:
		rig_debug(RIG_DEBUG_ERR,
			  "ts2k_set_freq: unsupported VFO %u\n", vfo);
		return -RIG_EINVAL;
	}
	freq_len = sprintf(freqbuf, "F%c%011"PRIll";", vfo_letter, freq);

	ack_len = 14;
	retval = ts2k_transaction(rig, freqbuf, freq_len, NULL, NULL);

	return retval;
}

/*
 * kenwood_get_freq
 * Assumes rig!=NULL, freq!=NULL
 */
int ts2k_get_freq(RIG * rig, vfo_t vfo, freq_t * freq)
{
	unsigned char freqbuf[50];
	unsigned char cmdbuf[4];
	int cmd_len, freq_len, retval;
	char vfo_letter;

	/*
	 * FIXME: need to handle RIG_VFO_MEM, etc...
	 */
	if (vfo == RIG_VFO_CURR) {
		retval = ts2k_get_vfo(rig, &vfo);
		if (retval != RIG_OK)
			return retval;
	}

	switch (vfo) {
	case RIG_VFO_A:
		vfo_letter = 'a';
		break;
	case RIG_VFO_B:
		vfo_letter = 'b';
		break;
	case RIG_VFO_C:
		vfo_letter = 'c';
		break;
	default:
		rig_debug(RIG_DEBUG_ERR,
			  __func__": unsupported VFO %u\n", vfo);
		return -RIG_EPROTO;
	}

	cmd_len = sprintf(cmdbuf, "f%c%s", vfo_letter, cmd_trm(rig));

	freq_len = 15;
	retval =
	    ts2k_transaction(rig, cmdbuf, cmd_len, freqbuf, &freq_len);
	//rig_debug(RIG_DEBUG_ERR,"__func__: received %s\n", cmdbuf);
	if (retval != RIG_OK)
		return retval;

	//if (freq_len != 14 || freqbuf[0] != 'F') {
	if (freqbuf[0] != 'F') {
		rig_debug(RIG_DEBUG_ERR,
			  __func__": unexpected answer '%s', "
			  "len=%u\n", freqbuf, freq_len);
		return -RIG_ERJCTED;
	}

	sscanf(freqbuf + 2, "%"SCNll, freq);

	return RIG_OK;
}

/*
 * kenwood_set_mode
 * Assumes rig!=NULL
 */
int ts2k_set_mode(RIG * rig, vfo_t vfo, rmode_t mode, pbwidth_t width)
{
	unsigned char mdbuf[16], ackbuf[16];
	int mdbuf_len, ack_len = 0, kmode, retval;

	switch (mode) {
	case RIG_MODE_CW:
		kmode = MD_CW;
		break;
	case RIG_MODE_USB:
		kmode = MD_USB;
		break;
	case RIG_MODE_LSB:
		kmode = MD_LSB;
		break;
	case RIG_MODE_FM:
		kmode = MD_FM;
		break;
	case RIG_MODE_AM:
		kmode = MD_AM;
		break;
	case RIG_MODE_RTTY:
		kmode = MD_FSK;
		break;
	default:
		rig_debug(RIG_DEBUG_ERR, "ts2k_set_mode: "
			  "unsupported mode %u\n", mode);
		return -RIG_EINVAL;
	}
	mdbuf_len = sprintf(mdbuf, "MD%c;", kmode);
	ack_len = 16;
	rig_debug(RIG_DEBUG_ERR, "ts2k_set_mode: sending %s\n", mdbuf);
	retval = ts2k_transaction(rig, mdbuf, mdbuf_len, NULL, NULL);

	return retval;
}

/*
 * kenwood_get_mode
 * Assumes rig!=NULL, mode!=NULL
 */
int ts2k_get_mode(RIG * rig, vfo_t vfo, rmode_t * mode, pbwidth_t * width)
{
	unsigned char modebuf[50];
	int mode_len, retval;


	mode_len = 50;
	retval = ts2k_transaction(rig, "MD;", 3, modebuf, &mode_len);
	if (retval != RIG_OK)
		return retval;

	if (mode_len != 4 || modebuf[1] != 'D') {
		rig_debug(RIG_DEBUG_ERR,
			  "ts2k_get_mode: unexpected answer, len=%u\n",
			  mode_len);
		return -RIG_ERJCTED;
	}

	*width = RIG_PASSBAND_NORMAL;	/* FIXME */
	switch (modebuf[2]) {
	case MD_CW:
		*mode = RIG_MODE_CW;
		break;
	case MD_USB:
		*mode = RIG_MODE_USB;
		break;
	case MD_LSB:
		*mode = RIG_MODE_LSB;
		break;
	case MD_FM:
		*mode = RIG_MODE_FM;
		break;
	case MD_AM:
		*mode = RIG_MODE_AM;
		break;
	case MD_FSK:
		*mode = RIG_MODE_RTTY;
		break;
#ifdef RIG_MODE_CWR
	case MD_CWR:
		*mode = RIG_MODE_CWR;
		break;
#endif
#ifdef RIG_MODE_RTTYR
	case MD_FSKR:
		*mode = RIG_MODE_RTTY;
		break;
#endif
	case MD_NONE:
		*mode = RIG_MODE_NONE;
		break;
	default:
		rig_debug(RIG_DEBUG_ERR, "ts2k_get_mode: "
			  "unsupported mode '%c'\n", modebuf[2]);
		return -RIG_EINVAL;
	}

	return RIG_OK;
}

/* added a whole buch of stuff --Dale */
int ts2k_set_level(RIG * rig, vfo_t vfo, setting_t level, value_t val)
{
	unsigned char levelbuf[16], ackbuf[16], ctrl;
	char *dc;	// to be replaced.
//	char dc[10];	// use this when ts2k_get_ctrl() is fixed.
//	int dc_len;	// use this when ts2k_get_ctrl() is fixed.
	int level_len, ack_len = 0, retval, reply;
	int ts2k_val;

	if (RIG_LEVEL_IS_FLOAT(level))
		ts2k_val = val.f * 255;
	else
		ts2k_val = val.i;

/* Several commands need to know status of CTRL */

//	dc_len = 10;	// these are to be used when ts2k_get_ctrl() is changed.
//	if( ts2k_get_ctrl(rig, dc, 10) != RIG_OK)
//		return -RIG_EINVAL;
// we shouldn't really change dc; its a static and will get changed.
	dc = ts2k_get_ctrl(rig);	// will get fixed
//	m = dc[2]; s = dc[3];		// set defaults

// FIXME: Just handles simple stuff RIG_VFO_CURR,VFOA,VFOB,VFOC; *may* work as intended.
	ctrl = '0';			// Assume Main
	if(vfo & RIG_CTRL_SUB)		// Change only if sub (bitwise, not logical)
		ctrl = '1';

	// assume no replies
	reply = 0;
	switch (level) {
	case RIG_LEVEL_RFPOWER:
		level_len = sprintf(levelbuf, "pc%03d;", ts2k_val);
		break;

	case RIG_LEVEL_AF:	// I call this volume.  :)
		level_len = sprintf(levelbuf, "ag%c%03u;", ctrl, ts2k_val);
		break;

	case RIG_LEVEL_RF:
		level_len = sprintf(levelbuf, "rg%03d;", ts2k_val);
		break;

	case RIG_LEVEL_SQL:	// fixed: uses main/sub	--Dale
		level_len = sprintf(levelbuf, "sq%c%03u;", ctrl, ts2k_val);
		break;


	/* I added the following levels --Dale */
	case RIG_LEVEL_PREAMP:
		level_len = sprintf(levelbuf, "pa%c;", (ts2k_val==0)? '0' : '1');
		break;

	case RIG_LEVEL_ATT:
		if(ts2k_val<1 || ts2k_val>2)	// only 1 or 2, 0=read
			return -RIG_EINVAL;
		level_len = sprintf(levelbuf, "an%01u;", ts2k_val);
		break;

	case RIG_LEVEL_NR:
		if(ts2k_val<0 || ts2k_val>2)	// only 1 or 2, 0=read
			return -RIG_EINVAL;
		level_len = sprintf(levelbuf, "nr%01u;", ts2k_val);
		break;

	/* FIXME: FM mic gain is low/mid/high; cmd="ex0410000n;" 0=low --Dale */
	case RIG_LEVEL_MICGAIN:
		level_len = sprintf(levelbuf, "mg%03u;", (ts2k_val>100)? 100 : ts2k_val);
		break;

	/* no rig error on invalid values */
	case RIG_LEVEL_KEYSPD:
		if(ts2k_val<10 || ts2k_val>60)	// only 1 or 2, 0=read
			return -RIG_EINVAL;
		level_len = sprintf(levelbuf, "ks%03u;", ts2k_val);
		break;

	case RIG_LEVEL_NOTCHF:	// actually, this is autonotch--same thing?
		level_len = sprintf(levelbuf, "al%03u;", (ts2k_val>4)? 4 : ts2k_val);
		break;

	case RIG_LEVEL_AGC:
		level_len = sprintf(levelbuf, "gt%03u;", (ts2k_val>20)? 20 : ts2k_val);
		break;

	case RIG_LEVEL_BKINDL:	// 0-1000ms in 50ms steps; this'll be good 'nuff.
		level_len = sprintf(levelbuf, "sd%04u;",(int)((float) ts2k_val*1000.0/255.0));
		break;

	case RIG_LEVEL_VOXGAIN:
		level_len = sprintf(levelbuf, "vg%03u;",(ts2k_val>9)? 9 : ts2k_val);
// alternate
//		level_len = sprintf(levelbuf, "vg%03u;",(int)((float) ts2k_val*9.0/255.0));
		break;

//	case RIG_LEVEL_VOX:	// header file declares these the same
	case RIG_LEVEL_VOXDELAY:
		level_len = sprintf(levelbuf, "vg%03u;",(int)((float) ts2k_val*3000.0/255.0));
		break;
// vox on/off
//		level_len = sprintf(levelbuf, "vx%c;", (ts2k_val==0)? '0' : '1');
//		break;

	/* Check unsupported just so we can complain if one is found.  :) */
	case RIG_LEVEL_APF:

	/* readonly */
	case RIG_LEVEL_SWR:
	case RIG_LEVEL_ALC:
	case RIG_LEVEL_STRENGTH:
		return -RIG_EINTERNAL;	// and complain loud!

	/* not currently implemented */
	case RIG_LEVEL_METER:	// readonly + needs rechecked.
	case RIG_LEVEL_PBT_IN:	// nnn
	case RIG_LEVEL_PBT_OUT: // NNN; "plnnnNNN;"
	case RIG_LEVEL_IF:	// hmmmm....
	case RIG_LEVEL_CWPITCH:	// "ex0310000n;" and a fair bit of programming...
	case RIG_LEVEL_COMP:	// available?, processor's not the same is it?
	case RIG_LEVEL_BALANCE:	// AF R/L balance?  See "ex01600002;" for similar(?)
	case RIG_LEVEL_SQLSTAT:	// need more info; avail?
		return -RIG_ENIMPL;

	default:
		rig_debug(RIG_DEBUG_ERR,
			  "Unsupported set_level %u", level);
		return -RIG_EINVAL;
	}
	// The following might make a nifty macro	--Dale
	ack_len = reply? 16 : 0;	// This'd work by itself wouldn't it?  --Dale
	retval = ts2k_transaction(rig, levelbuf, level_len,
			reply? ackbuf : NULL, reply? &ack_len : NULL);

	if (retval != RIG_OK)
		return retval;

	return RIG_OK;
}

/*
 * assumes f!=NULL
 */
static int
get_ts2k_level(RIG * rig, const char *cmd, int cmd_len, float *f)
{
	unsigned char lvlbuf[50];
	int lvl_len, retval;
	int lvl;

	lvl_len = 50;
	retval = ts2k_transaction(rig, cmd, cmd_len, lvlbuf, &lvl_len);
	if (retval != RIG_OK)
		return retval;

	if (lvl_len != 6) {
		rig_debug(RIG_DEBUG_ERR,
			  "ts2k_get_level: wrong answer len=%u\n",
			  lvl_len);
		return -RIG_ERJCTED;
	}

	/*
	 * 000..255
	 */
	sscanf(lvlbuf + 2, "%u", &lvl);
	*f = (float) lvl / 255.0;

	return RIG_OK;
};


/*
 * kenwood_get_level
 * Assumes rig!=NULL, val!=NULL
 */
int ts2k_get_level(RIG * rig, vfo_t vfo, setting_t level, value_t * val)
{
	unsigned char lvlbuf[50];
	int lvl_len, retval;
	int lvl;
	int i;

	lvl_len = 50;
	/* Optimize:
	 *   sort the switch cases with the most frequent first
	 */
	switch (level) {
	case RIG_LEVEL_STRENGTH:
		// fixme: "sm0;" VFO must be specified!  --kd7eni
		retval = ts2k_transaction(rig, "SM;", 3, lvlbuf, &lvl_len);
		if (retval != RIG_OK)
			return retval;

		if (lvl_len != 7 || lvlbuf[1] != 'M') {
			rig_debug(RIG_DEBUG_ERR, "ts2k_get_level: "
				  "wrong answer len=%u\n", lvl_len);
			return -RIG_ERJCTED;
		}

		/*
		 * Frontend expects:
		 *    -54 = S0
		 *              0 = S9
		 */
		sscanf(lvlbuf + 2, "%u", &val->i);
		val->i = (val->i * 4) - 54;
		break;

	case RIG_LEVEL_SQLSTAT:
		return -RIG_ENIMPL;	/* get_dcd ? */

	case RIG_LEVEL_PREAMP:
		return -RIG_ENIMPL;

	case RIG_LEVEL_ATT:
		retval = ts2k_transaction(rig, "RA;", 3, lvlbuf, &lvl_len);
		if (retval != RIG_OK)
			return retval;

		if (lvl_len != 5) {
			rig_debug(RIG_DEBUG_ERR, "ts2k_get_level: "
				  "unexpected answer len=%u\n", lvl_len);
			return -RIG_ERJCTED;
		}

		sscanf(lvlbuf + 2, "%u", &lvl);
		if (lvl == 0) {
			val->i = 0;
		} else {
			for (i = 0; i < lvl && i < MAXDBLSTSIZ; i++)
				if (rig->state.attenuator[i] == 0) {
					rig_debug(RIG_DEBUG_ERR,
						  "ts2k_get_level: "
						  "unexpected att level %u\n",
						  lvl);
					return -RIG_EPROTO;
				}
			if (i != lvl)
				return -RIG_EINTERNAL;
			val->i = rig->state.attenuator[i - 1];
		}
		break;
	case RIG_LEVEL_RFPOWER:
		return get_ts2k_level(rig, "PC;", 3, &val->f);

	case RIG_LEVEL_AF:
		return get_ts2k_level(rig, "AG;", 3, &val->f);

	case RIG_LEVEL_RF:
		return get_ts2k_level(rig, "RG;", 3, &val->f);

	case RIG_LEVEL_SQL:
		return get_ts2k_level(rig, "SQ;", 3, &val->f);

	case RIG_LEVEL_MICGAIN:
		return get_ts2k_level(rig, "MG;", 3, &val->f);

	case RIG_LEVEL_AGC:
		return get_ts2k_level(rig, "GT;", 3, &val->f);

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
		rig_debug(RIG_DEBUG_ERR,
			  "Unsupported get_level %u", level);
		return -RIG_EINVAL;
	}

	return RIG_OK;
}


int ts2k_set_func(RIG * rig, vfo_t vfo, setting_t func, int status)
{
	unsigned char fctbuf[16], ackbuf[16];
	int fct_len, ack_len = 16;

	/* Optimize:
	 *   sort the switch cases with the most frequent first
	 */
	switch (func) {
	case RIG_FUNC_NB:
		fct_len =
		    sprintf(fctbuf, "NB%c;",
			    status == RIG_FUNC_NB ? '0' : '1');
		return ts2k_transaction(rig, fctbuf, fct_len, NULL, NULL);

	case RIG_FUNC_ABM:
		fct_len =
		    sprintf(fctbuf, "AM%c;",
			    status == RIG_FUNC_ABM ? '0' : '1');
		return ts2k_transaction(rig, fctbuf, fct_len, NULL, NULL);

	case RIG_FUNC_COMP:
		fct_len =
		    sprintf(fctbuf, "PR%c;",
			    status == RIG_FUNC_COMP ? '0' : '1');
		return ts2k_transaction(rig, fctbuf, fct_len, NULL, NULL);

	case RIG_FUNC_TONE:
		fct_len =
		    sprintf(fctbuf, "TO%c;",
			    status == RIG_FUNC_TONE ? '0' : '1');
		return ts2k_transaction(rig, fctbuf, fct_len, NULL, NULL);

	case RIG_FUNC_TSQL:
		// fixme: see ts2000.doc and follow the proceedure!  --kdeni
		// rigbug!
		fct_len =
		    sprintf(fctbuf, "CT%c;",
			    status == RIG_FUNC_TSQL ? '0' : '1');
		return ts2k_transaction(rig, fctbuf, fct_len,
					ackbuf, &ack_len);

	case RIG_FUNC_VOX:
		fct_len =
		    sprintf(fctbuf, "VX%c;",
			    status == RIG_FUNC_VOX ? '0' : '1');
		return ts2k_transaction(rig, fctbuf, fct_len, NULL, NULL);

	case RIG_FUNC_NR:
		fct_len =
		    sprintf(fctbuf, "NR%c;",
			    status == RIG_FUNC_NR ? '0' : '1');
		return ts2k_transaction(rig, fctbuf, fct_len, NULL, NULL);

	case RIG_FUNC_BC:
		fct_len =
		    sprintf(fctbuf, "BC%c;",
			    status == RIG_FUNC_BC ? '0' : '1');
		return ts2k_transaction(rig, fctbuf, fct_len, NULL, NULL);

	case RIG_FUNC_ANF:
		fct_len =
		    sprintf(fctbuf, "NT%c;",
			    status == RIG_FUNC_ANF ? '0' : '1');
		return ts2k_transaction(rig, fctbuf, fct_len, NULL, NULL);

	case RIG_FUNC_LOCK:
		fct_len =
		    sprintf(fctbuf, "LK%c0;",
			    status == RIG_FUNC_LOCK ? '0' : '1');
		return ts2k_transaction(rig, fctbuf, fct_len, NULL, NULL);

	default:
		rig_debug(RIG_DEBUG_ERR, "Unsupported set_func %#x", func);
		return -RIG_EINVAL;
	}
	return RIG_OK;
}


/*
 * assumes status!=NULL
 * works for any 'format 1' command
 */
static int
get_ts2k_func(RIG * rig, const char *cmd, int cmd_len, int *status)
{
	unsigned char fctbuf[50];
	int fct_len, retval;

	fct_len = 50;
	retval = ts2k_transaction(rig, cmd, cmd_len, fctbuf, &fct_len);
	if (retval != RIG_OK)
		return retval;

	if (fct_len != 4) {
		rig_debug(RIG_DEBUG_ERR,
			  __func__": wrong answer len=%u\n", fct_len);
		return -RIG_ERJCTED;
	}

	*status = fctbuf[2] == '0' ? 0 : 1;

	return RIG_OK;
};


/*
 * kenwood_get_func
 * Assumes rig!=NULL, val!=NULL
 */
int ts2k_get_func(RIG * rig, vfo_t vfo, setting_t func, int *status)
{
	unsigned char fctbuf[50];
	int fct_len, retval;

	fct_len = 50;
	/* Optimize:
	 *   sort the switch cases with the most frequent first
	 */
	switch (func) {
	case RIG_FUNC_FAGC:
		retval = ts2k_transaction(rig, "GT;", 3, fctbuf, &fct_len);
		if (retval != RIG_OK)
			return retval;

		if (fct_len != 6) {
			rig_debug(RIG_DEBUG_ERR, "ts2k_get_func: "
				  "wrong answer len=%u\n", fct_len);
			return -RIG_ERJCTED;
		}

		*status = fctbuf[4] != '4' ? 1 : 0;
		break;

	case RIG_FUNC_NB:
		return get_ts2k_func(rig, "NB;", 3, status);

	case RIG_FUNC_ABM:
		return get_ts2k_func(rig, "AM;", 3, status);

	case RIG_FUNC_COMP:
		return get_ts2k_func(rig, "PR;", 3, status);

	case RIG_FUNC_TONE:
		return get_ts2k_func(rig, "TO;", 3, status);

	case RIG_FUNC_TSQL:
		return get_ts2k_func(rig, "CT;", 3, status);

	case RIG_FUNC_VOX:
		return get_ts2k_func(rig, "VX;", 3, status);

	case RIG_FUNC_NR:
		return get_ts2k_func(rig, "NR;", 3, status);

		/* FIXME on TS2000 */
	case RIG_FUNC_BC:
		return get_ts2k_func(rig, "BC;", 3, status);

	case RIG_FUNC_ANF:
		return get_ts2k_func(rig, "NT;", 3, status);

	case RIG_FUNC_LOCK:
		return get_ts2k_func(rig, "LK;", 3, status);

	default:
		rig_debug(RIG_DEBUG_ERR, "Unsupported get_func %#x", func);
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
 *
 * TODO: TS-2000 uses CN/CT
 *	ex057 menu is AutoPower off for TS-2000	--kd7eni
 */
int ts2k_set_ctcss_tone(RIG * rig, vfo_t vfo, tone_t tone)
{
	return ts2k_set_Tones(rig, vfo, tone, (char)'c');
}

int ts2k_set_tone(RIG * rig, vfo_t vfo, tone_t tone)
{
	return ts2k_set_Tones(rig, vfo, tone, (char)'t');
}

int ts2k_set_Tones(RIG * rig, vfo_t vfo, tone_t tone, const char ct)
{
	const struct rig_caps *caps;
	unsigned char tonebuf[16], ackbuf[16];
	int tone_len, ack_len = 0;
	int i;

	caps = rig->caps;

/* TODO: replace 200 by something like RIGTONEMAX */
	for (i = 0; ts2k_ctcss_list[i] != 0 && i < 38; i++) {
		if ((ts2k_ctcss_list[i] >= tone)
			&& (ts2k_ctcss_list[i-1] < tone))	// at least get close
			break;
	}
	if (ts2k_ctcss_list[i-1] == tone)
		i--;
	if (ts2k_ctcss_list[i] > tone)
		return -RIG_EINVAL;

	tone_len = sprintf(tonebuf, "%cn%02u;", ct, i+1);

	ack_len = 16;
	return ts2k_transaction(rig, tonebuf, tone_len, NULL, NULL);
	rig_debug(RIG_DEBUG_ERR, __func__": sent %s", tonebuf);
}

/*
 * kenwood_get_ctcss_tone
 * Assumes rig!=NULL, rig->state.priv!=NULL
 */
int ts2k_get_ctcss_tone(RIG * rig, vfo_t vfo, tone_t * tone)
{
	return ts2k_get_Tones(rig, vfo, tone, "cn;");
}

int ts2k_get_tone(RIG * rig, vfo_t vfo, tone_t * tone)
{
	return ts2k_get_Tones(rig, vfo, tone, "tn;");
}

int ts2k_get_Tones(RIG * rig, vfo_t vfo, tone_t * tone, const char *ct)
{
	const struct rig_caps *caps;
	unsigned char tonebuf[10];
	int tone_len, i, retval;
	unsigned int tone_idx;

	caps = rig->caps;

	tone_len = 10;
	retval = ts2k_transaction(rig, ct, 3, tonebuf, &tone_len);
	if (retval != RIG_OK)
		return retval;

	if (tone_len != 5) {
		rig_debug(RIG_DEBUG_ERR,
			  __func__": unexpected reply "
			  "'%s', len=%u\n", tonebuf, tone_len);
		return -RIG_ERJCTED;
	}

	sscanf(tonebuf + 2, "%u", (int *) &tone_idx);

	if (tone_idx == 0) {
		rig_debug(RIG_DEBUG_ERR,
			  __func__": Unexpected Tone "
			  "no (%04d)\n", tone_idx);
		return -RIG_EPROTO;
	}

	/* check this tone exists. That's better than nothing. */
	for (i = 0; i < tone_idx; i++) {
		if (caps->ctcss_list[i] == 0) {
			rig_debug(RIG_DEBUG_ERR,
				  __func__": Tone NG "
				  "(%04d)\n", tone_idx);
			return -RIG_EPROTO;
		}
	}
	*tone = caps->ctcss_list[tone_idx - 1];

	return RIG_OK;
}

/*
 * kenwood_get_ptt
 * Assumes rig!=NULL, ptt!=NULL
 */
int ts2k_get_ptt(RIG * rig, vfo_t vfo, ptt_t * ptt)
{
	unsigned char infobuf[50];
	int info_len, retval;

	info_len = 50;
	retval = ts2k_transaction(rig, "IF;", 3, infobuf, &info_len);
	if (retval != RIG_OK)
		return retval;

	if (info_len != 38 || infobuf[1] != 'F') {
		rig_debug(RIG_DEBUG_ERR,
			  "ts2k_get_ptt: wrong answer len=%u\n", info_len);
		return -RIG_ERJCTED;
	}

	*ptt = infobuf[28] == '0' ? RIG_PTT_OFF : RIG_PTT_ON;

	return RIG_OK;
}

/*
 * kenwood_set_ptt
 * Assumes rig!=NULL
 */
int ts2k_set_ptt(RIG * rig, vfo_t vfo, ptt_t ptt)
{
	unsigned char ackbuf[16];
	int ack_len = 16;


	return ts2k_transaction(rig,
				ptt ==
				RIG_PTT_ON ? "TX;" : "RX;", 3, NULL, NULL);
}
/*
 * kenwood_get_dcd
 * Assumes rig!=NULL, dcd!=NULL
 */
int ts2k_get_dcd(RIG * rig, vfo_t vfo, dcd_t * dcd)
{
	unsigned char busybuf[50];
	int busy_len, retval;

	busy_len = 50;
	retval = ts2k_transaction(rig, "BY;", 3, busybuf, &busy_len);
	if (retval != RIG_OK)
		return retval;

	if (busy_len != 4) {
		rig_debug(RIG_DEBUG_ERR,
			  "ts2k_get_dcd: wrong answer len=%u\n", busy_len);
		return -RIG_ERJCTED;
	}

	*dcd = (busybuf[2] == 0x01) ? RIG_DCD_ON : RIG_DCD_OFF;

	return RIG_OK;
}

/*
 * kenwood_set_trn
 * Assumes rig!=NULL
 */
int ts2k_set_trn(RIG * rig, int trn)
{
	unsigned char trnbuf[16], ackbuf[16];
	int trn_len, ack_len = 16;

	/* changed to TS-2000 --D.E. kd7eni */

	trn_len = sprintf(trnbuf, "AI%c;", trn == RIG_TRN_RIG ? '2' : '0');

	return ts2k_transaction(rig, trnbuf, trn_len, NULL, NULL);
	// No reply on "ai2;"--how quaint!
}
/*
 * kenwood_get_trn
 * Assumes rig!=NULL, trn!=NULL
 */ int ts2k_get_trn(RIG * rig, int *trn)
{
	unsigned char trnbuf[50];
	int trn_len, retval;

	trn_len = 50;
	retval = ts2k_transaction(rig, "AI;", 3, trnbuf, &trn_len);
	if (retval != RIG_OK)
		return retval;

	if (trn_len != 4) {
		rig_debug(RIG_DEBUG_ERR,
			  "ts2k_get_trn: wrong answer" "len=%u\n",
			  trn_len);
		return -RIG_ERJCTED;
	}
	*trn = trnbuf[2] != '0' ? RIG_TRN_RIG : RIG_TRN_OFF;

	return RIG_OK;
}

/*
 * kenwood_set_powerstat
 * Assumes rig!=NULL
 */
int ts2k_set_powerstat(RIG * rig, powerstat_t status)
{
	unsigned char pwrbuf[16], ackbuf[16];
	int pwr_len, ack_len = 16;

	pwr_len =
	    sprintf(pwrbuf, "PS%c;", status == RIG_POWER_ON ? '1' : '0');

	return ts2k_transaction(rig, pwrbuf, pwr_len, ackbuf, &ack_len);
}
/*
 * kenwood_get_powerstat
 * Assumes rig!=NULL, trn!=NULL
 */
int ts2k_get_powerstat(RIG * rig, powerstat_t * status)
{
	unsigned char pwrbuf[50];
	int pwr_len = 50, retval;

	// No reply when powered off, 1=power on.  Geez!
	retval = ts2k_transaction(rig, "PS;", 3, pwrbuf, &pwr_len);
	if (retval != RIG_OK)
		return retval;

	if (pwr_len != 4) {
		rig_debug(RIG_DEBUG_ERR,
			  "ts2k_get_powerstat: wrong answer "
			  "len=%u\n", pwr_len);
		return -RIG_ERJCTED;
	}
	*status = pwrbuf[2] == '0' ? RIG_POWER_OFF : RIG_POWER_ON;

	return RIG_OK;
}

/*
 * kenwood_reset
 * Assumes rig!=NULL
 */
int ts2k_reset(RIG * rig, reset_t reset)
{
	unsigned char rstbuf[16], ackbuf[16];
	int rst_len, ack_len = 16;
	char rst;

	switch (reset) {
	case RIG_RESET_VFO:
		rst = '1';
		break;
	case RIG_RESET_MASTER:
		rst = '2';
		break;
	default:
		rig_debug(RIG_DEBUG_ERR,
			  "ts2k_reset: unsupported reset %u\n", reset);
		return -RIG_EINVAL;
	}
	rst_len = sprintf(rstbuf, "SR%c;", rst);

	// Largely untested!    ;)      --kd7eni
	return ts2k_transaction(rig, rstbuf, rst_len, ackbuf, &ack_len);
}

/*
 * kenwood_send_morse
 * Assumes rig!=NULL
 */
int ts2k_send_morse(RIG * rig, vfo_t vfo, const char *msg)
{
	unsigned char morsebuf[30], ackbuf[16];
	int morse_len, ack_len = 0;
	int msg_len, buff_len, retval;
	const char *p;

	p = msg;
	msg_len = strlen(msg);

	while (msg_len > 0) {
		/*
		 * TODO: check with "KY;" if char buffer is available.
		 *              if not, sleep.
		 */
		buff_len = msg_len > 24 ? 24 : msg_len;

		strcpy(morsebuf, "KY ");
		strncat(morsebuf, p, buff_len);
		strcat(morsebuf, ";");
		morse_len = 4 + buff_len;
		ack_len = 16;
		retval =
		    ts2k_transaction(rig, morsebuf, morse_len,
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
int ts2k_vfo_op(RIG * rig, vfo_t vfo, vfo_op_t op)
{
	unsigned char *cmd, ackbuf[16];
	int ack_len = 0;

	switch (op) {
	case RIG_OP_UP:
		cmd = "UP;";
		break;
	case RIG_OP_DOWN:
		cmd = "DN;";
		break;
	case RIG_OP_BAND_UP:
		cmd = "BD;";
		break;
	case RIG_OP_BAND_DOWN:
		cmd = "BU;";
		break;
	default:
		rig_debug(RIG_DEBUG_ERR,
			  "ts2k_vfo_op: unsupported op %#x\n", op);
		return -RIG_EINVAL;
	}
	ack_len = 16;
	return ts2k_transaction(rig, cmd, 3, NULL, NULL);
}

/*
 * kenwood_set_mem
 * Assumes rig!=NULL
 */
int ts2k_set_mem(RIG * rig, vfo_t vfo, int ch)
{
	unsigned char membuf[16], ackbuf[16];
	int mem_len, ack_len = 16;

	/*
	 * "MCbmm;"
	 * where b is the bank number, mm the memory number.
	 * b can be a space ( *not*! manual wrong. --kd7eni
	 */
	mem_len = sprintf(membuf, "MC%03d;", ch);

	return ts2k_transaction(rig, membuf, mem_len, NULL, NULL);
}
/*
 * kenwood_get_mem
 * Assumes rig!=NULL
 */ int ts2k_get_mem(RIG * rig, vfo_t vfo, int *ch)
{
	unsigned char membuf[50];
	int retval, mem_len;

	/*
	 * "MCbmm;"
	 * where b is the bank number, mm the memory number.
	 * b can be a space (*not* --kd7eni)
	 */

	mem_len = 10;
	retval = ts2k_transaction(rig, "MC;", 3, membuf, &mem_len);
	if (retval != RIG_OK)
		return retval;

	if (mem_len != 6) {
		rig_debug(RIG_DEBUG_ERR,
			  "ts2k_get_mem: wrong answer " "len=%u\n",
			  mem_len);
		return -RIG_ERJCTED;
	}
	membuf[5] = '\0';
	*ch = atoi(membuf + 2);

	return RIG_OK;
}

/*
 * kenwood_get_info
 * supposed to work only for TS2000...
 * Assumes rig!=NULL
 */
const char *ts2k_get_info(RIG * rig)
{
	unsigned char firmbuf[50];
	int firm_len, retval;

	firm_len = 50;
	retval = ts2k_transaction(rig, "TY;", 3, firmbuf, &firm_len);
	if (retval != RIG_OK)
		return NULL;

	if (firm_len != 6) {
		rig_debug(RIG_DEBUG_ERR,
			  "ts2k_get_info: wrong answer len=%u\n",
			  firm_len);
		return NULL;
	}

	switch (firmbuf[4]) {
	case '0':
		return "Firmware: Overseas type";
	case '1':
		return "Firmware: Japanese 100W type";
	case '2':
		return "Firmware: Japanese 20W type";
	default:
		return "Firmware: unknown";
	}
}

#define IDBUFSZ 16

/*
 * probe_kenwood
 */
rig_model_t probe_ts2k(port_t * port)
{
	unsigned char idbuf[IDBUFSZ];
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

	retval = write_block(port, "ID;", 3);
	id_len = read_string(port, idbuf, IDBUFSZ, EOM_KEN EOM_TH, 2);

	close(port->fd);

	if (retval != RIG_OK)
		return RIG_MODEL_NONE;

	/*
	 * reply should be something like 'IDxxx;'
	 */
	if (id_len != 5 && id_len != 6) {
		idbuf[7] = '\0';
		rig_debug(RIG_DEBUG_VERBOSE,
			  "probe_ts2k: protocol error,"
			  " expected %u, received %u: %s\n", 6,
			  id_len, idbuf);
		return RIG_MODEL_NONE;
	}


	/* first, try ID string */
	for (i = 0; ts2k_id_string_list[i].model != RIG_MODEL_NONE; i++) {
		if (!strncmp(ts2k_id_string_list[i].id, idbuf + 2, 16)) {
			rig_debug(RIG_DEBUG_VERBOSE,
				  "probe_ts2k: " "found %s\n", idbuf + 2);
			return ts2k_id_string_list[i].model;
		}
	}

	/* then, try ID numbers */

	k_id = atoi(idbuf + 2);

	for (i = 0; ts2k_id_list[i].model != RIG_MODEL_NONE; i++) {
		if (ts2k_id_list[i].id == k_id) {
			rig_debug(RIG_DEBUG_VERBOSE,
				  "probe_ts2k: " "found %03d\n", k_id);
			return ts2k_id_list[i].model;
		}
	}
	/*
	 * not found in known table....
	 * update ts2k_id_list[]!
	 */
	rig_debug(RIG_DEBUG_WARN,
		  "probe_ts2k: found unknown device "
		  "with ID %03d, please report to Hamlib "
		  "developers.\n", k_id);

	return RIG_MODEL_NONE;
}


/* kenwood_init
 *
 * Basically, it sets up *priv
 * REM: serial port is already open (rig->state.rigport.fd)
 */
int ts2k_init(RIG * rig)
{
	const struct rig_caps *caps;
	const struct ts2k_priv_caps *priv_caps;
	rig_debug(RIG_DEBUG_TRACE, __func__ ": called\n");

	if (!rig || !rig->caps)
		return -RIG_EINVAL;

	caps = rig->caps;
	if (!caps->priv)
		return -RIG_ECONF;

	priv_caps = (const struct ts2k_priv_caps *) caps->priv;

#if 0				/* No private data for Kenwood backends */
	priv = (struct ts2k_priv_data *)
	    malloc(sizeof(struct ts2k_priv_data));
	if (!priv) {
		/* whoops! memory shortage! */
		return -RIG_ENOMEM;
	}

	rig->state.priv = (void *) priv;
	/* Assign default values */
	priv->dummy = -1;	// placeholder for real entries.
#endif

	return RIG_OK;
}

/* kenwood_cleanup
 * the serial port is closed by the frontend
 */
int ts2k_cleanup(RIG * rig)
{
	rig_debug(RIG_DEBUG_TRACE, __func__ ": called\n");

	if (!rig)
		return -RIG_EINVAL;

	if (rig->state.priv)
		free(rig->state.priv);
	rig->state.priv = NULL;

	return RIG_OK;
}
/*
 * initrigs_kenwood is called by rig_backend_load
 */ int initrigs_ts2k(void *be_handle)
{
	rig_debug(RIG_DEBUG_VERBOSE, "ts2k: _init called\n");

	rig_register(&ts950sdx_caps);
	rig_register(&ts50s_caps);
	rig_register(&ts450s_caps);
	rig_register(&ts570d_caps);
	rig_register(&ts570s_caps);
	rig_register(&ts790_caps);
	rig_register(&ts850_caps);
	rig_register(&ts870s_caps);
	rig_register(&ts2000_caps);
	rig_register(&thd7a_caps);
	rig_register(&thf7e_caps);

	return RIG_OK;
}

/*****************************************************************************
	Added the following functions.
 	(C) Copyright 2002 by Dale E. Edmons.   All rights Reserved.
	License:	Identical to all other Hamlib code.
*****************************************************************************/

#define CHKERR(c)	if((c) != RIG_OK) return c
#define STUFF(c)	int retval, acklen=(c); char ack[c]
//#define STUFF(c)	static int retval, acklen=(c); static char ack[c]

// The following two are expensive but convenient!

int ncpy(char *tmp, char *src, int cnt)
{
	strncpy(tmp, src, cnt);
	tmp[cnt] = '\0';
	return RIG_OK;
}

int int_n(char *tmp, char *src, const int cnt)
{
	ncpy(tmp, src, cnt);
	return atoi(tmp);
}

/*
 * ts2k_get_ctrl() ts2000 transceiver check.  Tests and returns the value of the current
 *	PTT/CTRL (using "dc;") for main and sub transceivers.  The settings are:
 *
 *	    PTT	__    __ CTRL	'0' = main; '1' = sub
 *		   \ /
 *		"dc00;"	PTT && CTRL both on main
 *		"dc01;"	PTT on main; CTRL on sub
 *		"dc10;"	PTT on sub; CTRL both on main
 *		"dc11;"	PTT && CTRL both on sub
 */
//int ts2k_get_ctrl(RIG * rig, char *dc_buf, int dc_len)	// use this when static removed.
char *ts2k_get_ctrl(RIG * rig)
{
	// FIXME: I guess this shouldn't be static for re-entrancy --Dale
	static char dc_buf[16];
	static int dc_len;

	static int retval;
	dc_len = 16;

 //	rig_debug(RIG_DEBUG_VERBOSE, "ts2k_get_ctrl: getting PTT/CTRL bytes\n");

	retval = ts2k_transaction(rig, "dc;", 3, dc_buf, &dc_len);
	if (retval != RIG_OK) {
		rig_debug(RIG_DEBUG_VERBOSE,
			  "ts2k_get_ctrl:error: retval=%u, dc_buf=%s\n",
			  retval, dc_buf);
		return NULL;
	}
//      rig_debug(RIG_DEBUG_VERBOSE, "ts2k_get_ctrl: returning %u PTT/CTRL bytes\n", dc_len);

//	return RIG_OK;	// use this when static variable removed and all other code changed.
	return dc_buf;
}

/* NOTE: PTT/CTRL enable is set only if ptt != 0 (or ctrl) */
int ts2k_set_ctrl(RIG * rig, int ptt, int ctrl)
{
	STUFF(10);
	char *buf;
	int buflen = 10;

	buf = ts2k_get_ctrl(rig);
	if(buf==NULL) {
		rig_debug(RIG_DEBUG_VERBOSE, __func__ \
			": returned NULL!\n");
		return -RIG_EINVAL;
	}
	rig_debug(RIG_DEBUG_VERBOSE, __func__": curr='%s',"
			"ptt=%u, ctrl=%u\n", buf, ptt, ctrl);

	if (ptt != 0)
		buf[2] = (TS2K_PTT_ON_SUB == ptt) ? '1' : '0';
	if (ctrl != 0)
		buf[3] = (TS2K_CTRL_ON_SUB == ctrl) ? '1' : '0';

	buf[4] = ';';
	buf[5] = '\0';	// just for printing debug

	acklen=10;
	retval = ts2k_transaction(rig, buf, 5, NULL, NULL);
	CHKERR(retval);

	return RIG_OK;
}

/*
 * FIXME: simple ascii to integer converter--expensive!
 *	Prevents trashing original string, otherwise
 *	just drop a (char)0 where you need it.
 */
int ts2k_get_int(char *src, int cnt)
{
	static char buf[20];

	strncpy(buf, src, cnt);
	buf[cnt] = (char) 0;

	return atoi(buf);
}

int ts2k_get_rit(RIG * rig, vfo_t vfo, shortfreq_t * rit)
{
	int retval, acklen;
	char ack[40];
	char *ctrl;

	ctrl = ts2k_get_ctrl(rig);
	if (ctrl == NULL)	// do we have valid "dc;" ?
		return -RIG_EINVAL;	// don't know proper errors yet!

	// sub shows main's rit!  We return 0 if subreceiver.
	if (ctrl[3] == '1') {
		*rit = 0;
		return RIG_OK;	// not really, but it's paid for...
	}
	retval = ts2k_transaction(rig, "if;", 3, ack, &acklen);
	CHKERR(retval);

	ack[23] = (char) 0;
	*rit = atoi(&ack[17]);

	return RIG_OK;
}

/*
 * ts2k_get_xit()  We just call ts2k_get_rit()
 * On the ts2k, they're the same.  The rig
 * acts this way.
*/
int ts2k_get_xit(RIG * rig, vfo_t vfo, shortfreq_t * rit)
{
	return ts2k_get_rit(rig, vfo, rit);
}

int ts2k_set_rit(RIG * rig, vfo_t vfo, shortfreq_t rit)
{
	char buf[40], c;
	int retval, i, len;

	// Clear current rit/xit.
	retval = ts2k_transaction(rig, "rc;", 3, NULL, NULL);
	CHKERR(retval);

	// Execute up/down request.
	if (rit > 0) {
		c = 'u';
		i = rit;
	} else {
		c = 'd';
		i = -rit;
	}

	len = sprintf(buf, "r%c%6d;", c, i);

	return ts2k_transaction(rig, buf, len, NULL, NULL);
}

int ts2k_set_xit(RIG * rig, vfo_t vfo, shortfreq_t rit)
{
	return ts2k_set_rit(rig, vfo, rit);
}

int ts2k_get_ts(RIG * rig, vfo_t vfo, shortfreq_t * ts)
{
	STUFF(10);
	int m, s;

	retval = ts2k_transaction(rig, "ST;", 5, ack, &acklen);
	CHKERR(retval);

	ack[4] = '\0';
	s = atoi(&ack[2]);

	rig_debug(RIG_DEBUG_VERBOSE, __func__": received: '%s', %u\n", ack, s);

	retval = ts2k_transaction(rig, "MD;", 5, ack, &acklen);
	CHKERR(retval);

	// fm or am mode selects 1
	m = (ack[2] == '4' || ack[2] == '5') ? 1 : 0;

	rig_debug(RIG_DEBUG_VERBOSE, __func__": received: '%s', %u\n", ack, m);

	*ts = ts2k_steps[m][s];
	return RIG_OK;
}

// FIXME: should get nearest, not easiest.  :)
/*
 *	status:	working.  fixed timeout.	--Dale
 */
int ts2k_set_ts(RIG * rig, vfo_t vfo, shortfreq_t ts)
{
	STUFF(10);
	char st[10];
	int m, s, k;
	long int aver, diff[2];

	retval = ts2k_transaction(rig, "md;", 5, ack, &acklen);
	CHKERR(retval);

	// fm or am mode selects 1
	m = (ack[2] == '4' || ack[2] == '5') ? 1 : 0;

	// fm or am selects 10 step freq. else 4.
	k = ( m )? 10 : 4;

	s=0;
	if( ts < ts2k_steps[m][0] ) {
		k=s;
	}

	if( ts > ts2k_steps[m][9] ) {
		s=9;
	}

	for ( s=1 ; s<k; s++) {
		if ( ts2k_steps[m][s-1] <= ts && ts <= ts2k_steps[m][s]) {
			diff[0] = ts - ts2k_steps[m][s-1];
			diff[1] = ts2k_steps[m][s] - ts;
			if( diff[0] > diff[1] ) {	// closer to [s]
				break;
			} else {			// closer to [s-1]
				s--;
				break;
			}
		}
	}
	// m is tmp now
	m = sprintf(st, "st0%1u;", s);

	// no reply!
	retval = ts2k_transaction(rig, st, m, NULL, NULL);
	CHKERR(retval);

	return RIG_OK;
}

/* Rig truncates in kHz(50) steps, so we don't. */
int ts2k_get_rptr_offs(RIG * rig, vfo_t vfo, shortfreq_t * rptr_offs)
{
	STUFF(20);

	retval = ts2k_transaction(rig, "of;", 3, ack, &acklen);
	CHKERR(retval);

	if (ack[0] != 'O' || ack[1] != 'F')
		return -RIG_EINVAL;
	if (acklen != 12)
		return -RIG_EINVAL;

	ack[11] = '\0';
	*rptr_offs = atoi(&ack[2]);

	return RIG_OK;
}

/* Rig truncates in kHz(50) steps, so we don't. */
int ts2k_set_rptr_offs(RIG * rig, vfo_t vfo, shortfreq_t rptr_offs)
{
	STUFF(20);
	char buf[20];
	int buflen = 20, i;

	i = sprintf(buf, "of%09u;", (unsigned int) rptr_offs);

	retval = ts2k_transaction(rig, buf, i, NULL, NULL);
	CHKERR(retval);

	return RIG_OK;
}

/*
 *	status:	working, leaves vfo intact.
 */
int ts2k_get_rptr_shift(RIG * rig, vfo_t vfo, rptr_shift_t * rptr_shift)
{
	STUFF(20);
	vfo_t vfo_tmp;

	// FIXME: I don't know if I should change back to currVFO, but I do.
	retval = ts2k_get_vfo(rig, &vfo_tmp);
	CHKERR(retval);

	retval = ts2k_transaction(rig, "os;", 3, ack, &acklen);
	CHKERR(retval);

	retval = ts2k_vfo_ctrl(rig, vfo);

	if (ack[0] != 'O' || ack[1] != 'S')
		return -RIG_EINVAL;
	//if(acklen != 4) return -RIG_EINVAL;

	switch (ack[2]) {
	case '0':
		*rptr_shift = RIG_RPT_SHIFT_NONE;
		break;
	case '1':
		*rptr_shift = RIG_RPT_SHIFT_MINUS;
		break;
	case '2':
		*rptr_shift = RIG_RPT_SHIFT_PLUS;
		break;
	case '3':
		*rptr_shift = RIG_RPT_SHIFT_1750;
		break;

	default:
		return -RIG_EINVAL;
		break;
	}

	// FIXME: I don't know if I should change back to currVFO, but I do.
	retval = ts2k_set_vfo(rig, vfo_tmp);
	CHKERR(retval);

	return RIG_OK;
}

/*
 * ts2k_set_rptr_shift()
 *
 *	status:		doesn't check VFO yet	--Dale
 */
int ts2k_set_rptr_shift(RIG * rig, vfo_t vfo, rptr_shift_t rptr_shift)
{
	STUFF(10);
	char c;

	switch (rptr_shift) {
	case RIG_RPT_SHIFT_NONE:
		c = '0';
		break;
	case RIG_RPT_SHIFT_PLUS:
		c = '1';
		break;
	case RIG_RPT_SHIFT_MINUS:
		c = '2';
		break;
	case RIG_RPT_SHIFT_1750:	// FIXME: invalid for mine! (non-Etype)
		c = '3';
		break;

	default:
		return -RIG_EINVAL;
		break;
	}
	acklen = sprintf(ack, "os%c;", c);

	retval = ts2k_transaction(rig, ack, 4, NULL, NULL);
	CHKERR(retval);

	return RIG_OK;
}

int ts2k_get_split(RIG * rig, vfo_t vfo, split_t * split)
{
	STUFF(10);
	char ack2[10];
	int ack2len = 10;

	retval = ts2k_transaction(rig, "fr;", 3, ack, &acklen);
	CHKERR(retval);

	retval = ts2k_transaction(rig, "ft;", 3, ack2, &ack2len);
	CHKERR(retval);

	if (ack[2] != ack2[2])
		*split = RIG_SPLIT_ON;
	else
		*split = RIG_SPLIT_OFF;

	return RIG_OK;
}

/* no VFO check, and no mem.  Yet. */
int ts2k_set_split(RIG * rig, vfo_t vfo, split_t split)
{
	STUFF(10);
	char ack2[10];
	int ack2len = 10;

	retval = ts2k_transaction(rig, "fr;", 3, ack, &acklen);
	CHKERR(retval);

	retval = ts2k_transaction(rig, "ft;", 3, ack2, &ack2len);
	CHKERR(retval);

	if (split == RIG_SPLIT_ON) {	// RX/TX on different vfo's
		if (ack[2] == '0')
			ack2[2] = '1';
		else if (ack[2] == '1')
			ack2[2] = '0';

		/* FIXME: mem split.  mem/vfo split must enable menu 6a
		 * with the "ex...;" or mem/vfo won't work.  A split
		 * memory operates simply by storing a memory with the
		 * freq with RX != TX then recalling it.  Don't know if
		 * if two separate memories can be used (e.g. 100/101)
		 */
		else
			return -RIG_EINVAL;
	} else {		// RX/TX on same vfo (or mem etc...)
		ack2[2] = ack[2];
	}

	// now, just send back the rig's strings :)
	retval = ts2k_transaction(rig, ack, acklen, NULL, NULL);
	CHKERR(retval);

	retval = ts2k_transaction(rig, ack2, ack2len, NULL, NULL);
	CHKERR(retval);

	return RIG_OK;
}

int ts2k_get_split_freq(RIG * rig, vfo_t vfo, freq_t * tx_freq)
{
	// FIXME : This makes too many assumptions--I'll fix it later...d PTT/CTRL bytes\n", dc_len);*//* The following are functions I've added for the TS-2000.  (C) 2002 D. Edmons and all that. *//*-------------------------------------------------------------------------------------------*/
	return ts2k_get_freq(rig, vfo, tx_freq);
}

int ts2k_set_split_freq(RIG * rig, vfo_t vfo, freq_t tx_freq)
{
	// FIXME : This makes too many assumptions--I'll fix it later...d PTT/CTRL bytes\n", dc_len);*//* The following are functions I've added for the TS-2000.  (C) 2002 D. Edmons and all that. *//*-------------------------------------------------------------------------------------------*/
	return ts2k_set_freq(rig, vfo, tx_freq);
}

/* ts2k_get_channel()
 *
 *	status:	working!  reading unset memory is an error.  this
 *		is the rig.  vfo_t is not needed for regular memory
 *		and we don't check it now.  when we start to access
 *		other memory we'll split into multiple functions
 *		each dedicated to the unique type (e.g. quick/tmp).
 *
 *		memory range not checked.
 *
 *	Note: the manual erroneously states bank should be ' ' if
 *		memory is <100.  The "correct" way is to *never*
 *		send a ' ' or you'll get a "?;" response.  Always
 *		set memory with (e.g.) "mc1020;".	--kd7eni
 */
int ts2k_get_channel(RIG * rig, channel_t *chan)
{
//	channel_t tch;	// needed?
	char rxtx, mrtxt[2][60], mrcmd[15], ack[60], tmp[20];
	int i, check, retval, curr_mem, mrtxt_len, mrcmd_len, ack_len;
	vfo_t curr_vfo;
#ifdef _USEVFO
	vfo_t v;

	v = vfo;
	// check the memory bit
	if( !(v & RIG_VFO_MEM) ) {
		rig_debug(RIG_DEBUG_ERR, __func__
			": Non Memory VFO='%u', v=%u\n", vfo, v);
		return -RIG_EINVAL;	// not a channel!
	}

// get needed info if rig's mem pointers used
	if( (	vfo == RIG_VFO_MEM_A
		|| vfo == RIG_VFO_MEM_C ) ) {
		rig_debug(RIG_DEBUG_ERR, __func__": using rig's ptr\n");
		retval = ts2k_get_vfo(rig, &curr_vfo);
		CHKERR(retval);
		retval = ts2k_get_mem(rig, curr_vfo, &curr_mem);
		CHKERR(retval);
		chan->channel_num = curr_mem;
	}
#endif

	mrtxt_len = ack_len = 60;
	mrcmd_len=15;

	if(chan==NULL)
		return -RIG_EINVAL;

// send request for both rx mem and tx mem
	for(i=0; i<2; i++) {	// 0=rx; 1=tx

		mrcmd_len = sprintf(mrcmd, "mr%01d%03u;", i, chan->channel_num);
		ack_len = 60;	// must be reset inside loop!
		retval = ts2k_transaction(rig, mrcmd, mrcmd_len, ack, &ack_len);
		CHKERR(retval);

		rig_debug(RIG_DEBUG_ERR, __func__": read \n\t'%s'\n", ack);

		ack[50] = '\0';		// May be too far, but helps.

		// watch out.  |= and != look the same!
		// Perform checks on data.
		check = (ack[0] != 'M');
		check |= (ack[1] != 'R');
		check |= (ack[2] != ((i == 0)? '0' : '1'));
		check |= (chan->channel_num != int_n(tmp, &ack[3], 3));
		if( check ) {
			rig_debug(RIG_DEBUG_ERR, __func__
				":check failed!\n");
			return -RIG_EINVAL;	// correct error type?
		}

		// all is well, save string
		strncpy( &mrtxt[i][0], ack, 52);
	}
	// FIXME: handle mem split
	// final check on data. (if RX!=TX mem split, or limit!)
	if( strncmp( &mrtxt[0][3], &mrtxt[1][3], 41-3) != 0 ) {
		rig_debug(RIG_DEBUG_ERR, "\n"__func__
			": MEM split and band limits not yet supported!\n");
		return -RIG_EINVAL;	// FIXME: sending proper error?
	}

	// FIXME: 1) Since chan is not an array, we fudge and only do TX!
	//		even if split!!!!!!!!
	// FIXME: 2) we only handle regular memories, not everything
	// FIXME: 3) I store only data ts2k actually saves in memory
	//		some values are the ts2k value, not the hamlib value!

	// (keep same order as channel struct to ease debugging!)
//	chan->channel_num = ;	// already set?

// The following may be used to indicate we're reading limits (290-299).
// At any rate, it's currently unused.
	chan->bank_num = 0;	// I merge the two--do not use! --Dale

	chan->lock = int_n(tmp, &mrtxt[0][18], 1);
	chan->freq = int_n(tmp, &mrtxt[0][06], 11);
	chan->mode = ts2k_mode_list[ int_n(tmp, &mrtxt[0][17], 1) ];
	if(chan->mode == RIG_MODE_AM || chan->tx_mode == RIG_MODE_FM)
		i = 1;
	else
		i = 0;
	chan->width = 0;
	chan->tx_freq = int_n(tmp, &mrtxt[1][06], 11);
	chan->tx_mode = ts2k_mode_list[ int_n(tmp, &mrtxt[1][17], 1)];
	chan->tx_width = 0;
	chan->split = 0;
	chan->rptr_shift = int_n(tmp, &mrtxt[1][28], 1);
	chan->rptr_offs = int_n(tmp, &mrtxt[1][29], 9);
	if(chan->tx_mode == RIG_MODE_AM || chan->tx_mode == RIG_MODE_FM)
		i = 1;
	else
		i = 0;
	chan->tuning_step = ts2k_steps[i][ int_n(tmp,& mrtxt[1][38], 2) ];
	chan->rit = 0;
	chan->xit = 0;
	chan->funcs = 0;
	for( i=0; i<RIG_SETTING_MAX; i++) {
		// the following shamelessly stolen from rigctl.c  --Dale
		setting_t level = rig_idx2setting(i);	// now, I understand
		if(RIG_LEVEL_IS_FLOAT(level))
			chan->levels[i].f = 0.0;	// I'd figured this out.
		else
			chan->levels[i].f = 0.0;
	}
	chan->ctcss_tone = ts2k_ctcss_list[ int_n(tmp, &mrtxt[1][22], 2) + 1 ];
	chan->ctcss_sql = int_n(tmp, &mrtxt[1][19], 1);
	chan->dcs_code = ts2k_dcs_list[ int_n(tmp, &mrtxt[1][24], 3) ];
	chan->dcs_sql = int_n(tmp, &mrtxt[1][19], 1);
	chan->scan_group = int_n(tmp, &mrtxt[1][40], 1);
//	chan->flags = curr_vfo;	// n/a
	// FIXME : The following may have trailing garbage
	strncpy( chan->channel_desc, &mrtxt[1][41], 8);
	chan->channel_desc[8] = '\0';

#ifdef _USEVFO
// if curr mem is changed at top, this'll restore it
	if( (	vfo == RIG_VFO_MEM_A
		|| vfo == RIG_VFO_MEM_C ) ) {
	}

	rig_debug(RIG_DEBUG_ERR, __func__": restoring mem=%i\n", curr_mem);
	retval = ts2k_set_mem(rig, curr_vfo, curr_mem);
	CHKERR(retval);
#endif

	return RIG_OK;
}

/*
 * ts2k_set_channel()	Here I read a channel_t and memory data.  I'm
 *	assuming this is correct.  Anyway, this is a useful function.
 *	The only quirk is that the TS-2000 saves both TX as well
 *	as RX data separately.  This function can easily be modified
 *	to write as if there were 600 memories but that would lead
 *	to confusion and the rig certainly don't operate that way.
 *	I hope to channel_t *chan changed to channel_t *chan[2].
 *	(As well as vfo_t too.)  Much thanks to Stephane for already
 *	having the most important stuff working from the start
 *	(mostly anyway).
 *
 *	From here on out I'm gonna try to use hamlib-1.1.0.pdf
 *	for the design document and hopefully I'll know which way
 *	to go when things aren't the way they should be.  We'll
 *	see how things go.
 *						--Dale kd7e
 */
int ts2k_set_channel(RIG * rig, const channel_t *chan)
{
	char mrtxt[2][60], mrcmd[10], ack[60];
	int retval, i, j, mr_len[2], ack_len;

	// the following are the actual memory data to be written.
	unsigned int
		p1,	// RX/TX (bool)
		p2p3,	// this is not a bug--I combine bank/channel
		p4,	// freq
		p5,	// mode
		p6,	// lockout
		p7,	// tone, ctcss, dcs
		p8,	// tone #
		p9,	// CTCSS #
		p10,	// DCS code
		p11,	// revers status
		p12,	// repeater shift
		p13,	// offset freq
		p14,	// step size
		p15;	// memory group (0-9)
	char	*p16;	// 8 char + 1 null byte

	ack_len = 10;
	/*
	 * Write everthing in order.  We only set things that
	 * the rig actually puts in memory.  This way, a set
	 * followed by a get will match exactly.  Otherwise,
	 * we'll have to fudge and won't know when we have
	 * a valid save.  (FIXME: delete all this useless
	 * comment stuff after the code is working. --Dale)
	 */

	/* FIXME: we are required to have RX/TX match */
	if(chan->freq != chan->tx_freq)
		return -RIG_EINVAL;	// should be 'unimplemented'

	for(i=0; i<2; i++) {
		p1 = (unsigned int) i; // 0=RX, 1=TX

		p2p3 = (unsigned int) chan->channel_num;	// we'll compare 'em later
		if( i == 0 )
			p4 = (unsigned int) chan->freq;
		else
			p4 = (unsigned int) chan->tx_freq;

		for(j=0; j<(sizeof(ts2k_mode_list)/sizeof(int)); j++) {
			if(chan->mode == ts2k_mode_list[j])
				break;
		}
		p5 = (unsigned int) j;	// FIXME: either not found, or last!
		p6 = (unsigned int) chan->lock;
		p7 = 0;	// FIXME: to lazy to sort this out right now
		p8 = 0; //	"	"	"	"	"
		p9 = 0; //	"	"	"	"	"
		p10 = 0; //	"	"	"	"	"
		p11 = 0; //	"	"	"	"	"
		p12 = 0; //	"	"	"	"	"
		p13 = 0; //	"	"	"	"	"
		p14 = 0; //	"	"	"	"	"
		p15 = (unsigned int) chan->scan_group;
		p16 = &(chan->channel_desc[0]);

		mr_len[i] = sprintf( &(mrtxt[i][0]),
			"mr%1u%3u%11u%1u%1u%1u%2u%2u%3u%1u%1u%9u%2u%1u%s;",
			p1, p2p3, p4, p5, p6, p7, p8, p9, p10, p11, p12, p13,
			p14, p15,
			p16
			);	// yikes!
		retval = ts2k_transaction(rig, &mrtxt[i][0], mr_len[i],
				 ack, &ack_len);
		CHKERR(retval);

		// FIXME: now readback the string and make sure it worked!
	}

	return retval;
}

/*
 * ts2k_vfo_ctrl()	set PTT/CTRL based on VFO arg
 *
 *	(Taken from my revision of ts2k_set_vfo())
 *
 *	status:		VFOA, VFOB, VFOC, Main, Sub,
 *			MEMA, MEMC, CALLA, CALLC
 *			VFO_AB, VFO_BA, ...
 *		They all work!		--Dale
 */
int ts2k_vfo_ctrl(RIG * rig, vfo_t vfo)
{
	int ptt, ctrl;
	int retval;

	ptt = ctrl = 0;

	// Main/Sub Active Transceiver
	switch (vfo) {
	case RIG_VFO_A:
	case RIG_VFO_B:
	case RIG_VFO_AB:	// split
	case RIG_VFO_BA:
	case RIG_CTRL_SAT:	// Should be PTT on main CTRL on sub (?)
	case RIG_VFO_MAIN:
	case RIG_VFO_MEM_A:
	case RIG_VFO_CALL_A:
		ctrl = TS2K_CTRL_ON_MAIN;	// FIXME : these are independent!
		ptt = TS2K_PTT_ON_MAIN;
		break;
	case RIG_VFO_C:
	case RIG_VFO_SUB:
	case RIG_VFO_MEM_C:
	case RIG_VFO_CALL_C:
		ctrl = TS2K_CTRL_ON_SUB;
		ptt = TS2K_PTT_ON_SUB;
		break;

	default:
		break;
	}

	// set PTT/CTRL
	retval = ts2k_set_ctrl(rig, ptt, ctrl);
	if (retval != RIG_OK)
		return -RIG_EINVAL;

	return retval;
}

/*
 *	status:	ok, no vfo checks
 */
int ts2k_get_dcs_code(RIG * rig, vfo_t vfo, tone_t *code)
{
	char ack[10], tmp[10];
	int retval, acklen, i;

	acklen = 10;
	retval = ts2k_transaction(rig, "qc;", 6, ack, &acklen);
	CHKERR(retval);

	i = int_n(tmp, &ack[2], 3);
	*code = ts2k_dcs_list[i];

	return RIG_OK;
}

/*
 *	status:	ok, no vfo checks
 */
int ts2k_set_dcs_code(RIG * rig, vfo_t vfo, tone_t code)
{
	char ack[10], cmd[10], tmp[10];
	int retval, acklen, cmdlen, i;

	// we only allow exact matches here
	i=0;
	while(code != ts2k_dcs_list[i]) {
		if(ts2k_dcs_list[i] == 0)
			return -RIG_EINVAL;
		i++;
	}
	cmdlen = sprintf(cmd, "qc%03u;", i);
	return ts2k_transaction(rig, cmd, cmdlen, NULL, NULL);
}

/*
 *	status:	new, all guesses and assumptions!
 *		know nothing about txwidth (which one?)
 *		or tx = rx + txwidth?
 */
int ts2k_set_split_mode(RIG * rig,
	vfo_t vfo, rmode_t txmode, pbwidth_t txwidth)
{
	vfo_t vtmp;

	switch(vfo) {
	case RIG_VFO_AB:
		vtmp = RIG_VFO_B; break;
	case RIG_VFO_BA:
		vtmp = RIG_VFO_A; break;
	default:
		return -RIG_EINVAL;
	}
	return ts2k_set_mode(rig, vtmp, txmode, txwidth);
}

int ts2k_get_split_mode(RIG *rig,
	vfo_t vfo, rmode_t *txmode, pbwidth_t *txwidth)
{
	vfo_t vtmp;
	switch(vfo) {
	case RIG_VFO_AB:
		vtmp = RIG_VFO_B; break;
	case RIG_VFO_BA:
		vtmp = RIG_VFO_A; break;
	default:
		return -RIG_EINVAL;
	}
	return ts2k_get_mode(rig, vtmp, txmode, txwidth);
}

/*
 *	status:	new
 */
int ts2k_scan(RIG *rig, vfo_t vfo, scan_t scan, int ch)
{
	int retval;
	vfo_t v;

	rig_debug(RIG_DEBUG_ERR, __func__": starting...\n");

	if(vfo==RIG_VFO_CURR) {
		retval = ts2k_get_vfo(rig, &v);
		CHKERR(retval);
	} else
		v = vfo;

	// hopefully, this'll work.  rig does nothing if already in scan!
	retval = ts2k_scan_off(rig);
	CHKERR(retval);

	rig_debug(RIG_DEBUG_ERR, __func__": got VFO = %x\n", v);
	// set proper vfo first (already done?)
	switch(v) {
	case RIG_VFO_MEM:	// Currently selected Main/Sub
	case RIG_VFO_MEM_A:	// Main
	case RIG_VFO_MEM_C:	// Sub
		// FIXME: we should set the group and fall through
		/* nobreak */
//	case RIG_VFO_VFO:	// Currently selected Main/Sub
	case RIG_VFO_A:		// Main
	case RIG_VFO_B:		// Main
	case RIG_VFO_C:		// Sub
		retval = ts2k_set_vfo(rig, v);	// already set?
		CHKERR(retval);
		break;

	case RIG_VFO_CALL_A:	//
	case RIG_VFO_CALL_C:
	default:
		rig_debug(RIG_DEBUG_ERR, __func__": vfo 'defaulted'\n");
		return -RIG_ENIMPL;	// unimplemented, but valid scan
	}

	rig_debug(RIG_DEBUG_ERR, __func__": VFO set!\n");
	retval = ts2k_scan_off(rig);
	CHKERR(retval);

	switch(scan) {

	case RIG_SCAN_STOP:
		return ts2k_scan_off(rig);
		break;

	case RIG_SCAN_PROG:
		if(ch<290)
			return -RIG_EINVAL;
		retval = ts2k_set_mem(rig, vfo, ch);
		CHKERR(retval);
		/* nobreak */
	case RIG_SCAN_MEM:
		/* nobreak */
	case RIG_SCAN_VFO:
		return ts2k_scan_on(rig, '1');	// Look Ma, I'm scanning!
		break;

	case RIG_SCAN_SLCT:
		/* nobreak */
	case RIG_SCAN_PRIO:
		/* nobreak */
	default:
		rig_debug(RIG_DEBUG_ERR, __func__": scan 'defaulted'\n");
		return -RIG_ENIMPL;	// unimplemented, but valid scan
	}
}

int ts2k_scan_on(RIG *rig, char sc)
{
	char cmd[]="sc0;", ack[10];
	int retval, cmdlen, acklen, i;

	rig_debug(RIG_DEBUG_ERR, __func__": turning scan on\n");
	cmd[2] = sc;
	retval = ts2k_transaction(rig, cmd, 4, NULL, NULL);
	CHKERR(retval);

	rig_debug(RIG_DEBUG_ERR, __func__": turned scan on\n");
	// force reply when turning scan on
	if(sc != '0') {
		cmd[2] = ';'; cmd[3] = '\0';
		retval = ts2k_transaction(rig, cmd, 3, ack, &acklen);
		CHKERR(retval);

		if(ack[2] != sc)
			return -RIG_EINVAL;
	}

	return RIG_OK;
}

int ts2k_scan_off(RIG *rig)
{
	rig_debug(RIG_DEBUG_ERR, __func__": turning scan off\n");
	return ts2k_scan_on(rig, '0');
}


int ts2k_get_parm(RIG *rig, setting_t parm, value_t *val)
{
	int retval, acklen, cmdlen;
	char ack[30], cmd[30];

	switch( parm ) {
	case RIG_PARM_BEEP:
		cmdlen = sprintf(cmd, "ex0120000;"); break;
	case RIG_PARM_BACKLIGHT:
		cmdlen = sprintf(cmd, "ex0000000;"); break;
	case RIG_PARM_KEYLIGHT:
		cmdlen = sprintf(cmd, "ex0010000;"); break;
	case RIG_PARM_APO:
		cmdlen = sprintf(cmd, "ex0570000;"); break;
	case RIG_PARM_ANN:
		return -RIG_ENIMPL;
	case RIG_PARM_TIME:
		return -RIG_ENAVAIL;
	default:
		return -RIG_EINTERNAL;
	}
	acklen = 30;
	retval = ts2k_transaction(rig, cmd, cmdlen, ack, &acklen);
	if(retval == RIG_OK) {
		switch( parm ) {
		case RIG_PARM_BEEP:
			val->i = (int)(ack[9] - '0');
			break;
		case RIG_PARM_BACKLIGHT:
		case RIG_PARM_KEYLIGHT:
			val->f = (float)(ack[9] - '0');
			break;
		case RIG_PARM_APO:
			val->i = (int) int_n(cmd, &ack[9], 2);	// cmd is TMP now
			break;
		default:
			return -RIG_EINTERNAL;
		}
	}
	return retval;
}

int ts2k_set_parm(RIG *rig, setting_t parm, value_t val)
{
	char cmd[30];
	int retval, cmdlen, i;

	rig_debug(RIG_DEBUG_ERR, __func__": val.i = %u\n", val.i);
	rig_debug(RIG_DEBUG_ERR, __func__": val.f = %f\n", val.f);

	switch( parm ) {
	case RIG_PARM_APO:
		cmdlen = sprintf(cmd, "ex0570000%01u;",
					(val.i>180)? 3: val.i/60);
		break;
	case RIG_PARM_BEEP:
		cmdlen = sprintf(cmd, "ex0120000%01u;", (val.i>9)? 9: val.i);
		break;

	case RIG_PARM_BACKLIGHT:
		cmdlen = sprintf(cmd, "ex0000000%01u;",
				(int) ((val.f>1.0)? 4.0 : val.f*4.0) );
		break;

	case RIG_PARM_KEYLIGHT:
		cmdlen = sprintf(cmd, "ex0010000%01u;", (val.i==0)? 0: 1);
		break;

	case RIG_PARM_ANN:
		return -RIG_ENIMPL;

	case RIG_PARM_TIME:
	case RIG_PARM_BAT:
		return -RIG_ENAVAIL;

	default:
		return -RIG_EINTERNAL;
	}

	retval = ts2k_transaction(rig, cmd, cmdlen, NULL, NULL);

	return retval;
}

#undef CHKERR
#undef STUFF

// End
