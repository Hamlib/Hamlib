/*
 *  Hamlib Drake backend - main file
 *  Copyright (c) 2001-2004 by Stephane Fillod
 *
 *	$Id: drake.c,v 1.8 2004-03-10 23:33:48 fillods Exp $
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

#include "drake.h"


/*
 * Protocol information available at http://www.rldrake.com/swl/R8B.pdf
 */


#define BUFSZ 64

#define CR "\x0d"
#define LF "\x0a"
#define EOM CR

#define MD_USB	'1'
#define MD_LSB	'2'
#define MD_RTTY	'3'
#define MD_CW	'4'
#define MD_FM	'5'
#define MD_AM	'6'


/*
 * drake_transaction
 * We assume that rig!=NULL, rig->state!= NULL, data!=NULL, data_len!=NULL
 *
 * FIXME: this is a skeleton, rewrite me for Drake protocol
 */
int drake_transaction(RIG *rig, const char *cmd, int cmd_len, char *data, int *data_len)
{
	int retval;
	struct rig_state *rs;

	rs = &rig->state;

	serial_flush(&rs->rigport);

	retval = write_block(&rs->rigport, cmd, cmd_len);
	if (retval != RIG_OK)
		return retval;


	/* no data expected, TODO: flush input? */
	if (!data || !data_len)
			return 0;

	*data_len = read_string(&rs->rigport, data, BUFSZ, LF, 1);

	return RIG_OK;
}

int drake_init(RIG *rig)
{
	struct drake_priv_data *priv;
	priv = malloc(sizeof(struct drake_priv_data));

	if (!priv)
		return -RIG_ENOMEM;

	priv->curr_ch = 0;
	rig->state.priv = priv;

	return RIG_OK;
}

int drake_cleanup(RIG *rig)
{
	struct drake_priv_data *priv = rig->state.priv;

	free(priv);

	return RIG_OK;
}

/*
 * drake_set_freq
 * Assumes rig!=NULL
 */
int drake_set_freq(RIG *rig, vfo_t vfo, freq_t freq)
{
	unsigned char freqbuf[16], ackbuf[16];
	int freq_len, ack_len, retval;

	/* 
	 * 10Hz resolution 
	 * TODO: round nearest?
	 */
	freq_len = sprintf(freqbuf,"F%07d", (unsigned int)freq/10);
	retval = drake_transaction(rig, freqbuf, freq_len, ackbuf, &ack_len);

	return retval;
}


/*
 * drake_set_vfo
 * Assumes rig!=NULL
 */
int drake_set_vfo(RIG *rig, vfo_t vfo)
{
	unsigned char cmdbuf[16], ackbuf[16];
	int cmd_len, ack_len, retval;
	char vfo_function;

	switch (vfo) {
	case RIG_VFO_VFO:
	case RIG_VFO_A: vfo_function = 'A'; break;
	case RIG_VFO_B: vfo_function = 'B'; break;
	default: 
		rig_debug(RIG_DEBUG_ERR,"drake_set_vfo: unsupported VFO %d\n",
							vfo);
		return -RIG_EINVAL;
	}

	cmd_len = sprintf(cmdbuf, "V%c" EOM, vfo_function);

	retval = drake_transaction (rig, cmdbuf, cmd_len, ackbuf, &ack_len);
	return retval;
}


/*
 * drake_set_mode
 * Assumes rig!=NULL
 */
int drake_set_mode(RIG *rig, vfo_t vfo, rmode_t mode, pbwidth_t width)
{
	unsigned char mdbuf[16], ackbuf[16];
	unsigned char mode_sel, width_sel;
	int mdbuf_len, ack_len, retval;

	switch (mode) {
	case RIG_MODE_CW:       mode_sel = MD_CW; break;
	case RIG_MODE_USB:      mode_sel = MD_USB; break;
	case RIG_MODE_LSB:      mode_sel = MD_LSB; break;
	case RIG_MODE_FM:       mode_sel = MD_FM; break;
	case RIG_MODE_AMS:
	case RIG_MODE_AM:       mode_sel = MD_AM; break;
	case RIG_MODE_RTTY:     mode_sel = MD_RTTY; break;
	default:
		rig_debug(RIG_DEBUG_ERR,"drake_set_mode: "
						"unsupported mode %d\n", mode);
		return -RIG_EINVAL;
	}

	mdbuf_len = sprintf(mdbuf, "M%c" EOM, mode_sel);
	retval = drake_transaction (rig, mdbuf, mdbuf_len, ackbuf, &ack_len);

	if (retval != RIG_OK)
		return retval;

	if (mode != RIG_MODE_FM) {

		if (width == RIG_PASSBAND_NORMAL)
			width = rig_passband_normal(rig, mode);

		switch (width) {
		case 500: width_sel = '0'; break;
		case 1800: width_sel = '1'; break;
		case 2300: width_sel = '2'; break;
		case 4000: width_sel = '4'; break;
		case 6000: width_sel = '6'; break;
		default:
			rig_debug(RIG_DEBUG_ERR,"drake_set_mode: "
						"unsupported width %d\n", width);
		return -RIG_EINVAL;
		}
		mdbuf_len = sprintf(mdbuf, "W%c" EOM, width_sel);
		retval = drake_transaction (rig, mdbuf, mdbuf_len, ackbuf, &ack_len);
	}

	if (mode == RIG_MODE_AMS || mode == RIG_MODE_AM) {
		mdbuf_len = sprintf(mdbuf, "S%c" EOM, mode==RIG_MODE_AMS?'O':'F');
		retval = drake_transaction (rig, mdbuf, mdbuf_len, ackbuf, &ack_len);
	}

	return retval;
}


/*
 * drake_set_ant
 * Assumes rig!=NULL
 */
int drake_set_ant(RIG *rig, vfo_t vfo, ant_t ant)
{
	unsigned char buf[16], ackbuf[16];
	int len, ack_len, retval;

	len = sprintf(buf,"A%c"EOM, ant==RIG_ANT_1?'1':(ant==RIG_ANT_2?'2':'C'));

	retval = drake_transaction(rig, buf, len, ackbuf, &ack_len);

	return retval;
}

/*
 * drake_set_mem
 * Assumes rig!=NULL
 */
int drake_set_mem(RIG *rig, vfo_t vfo, int ch)
{
	struct drake_priv_data *priv = rig->state.priv;

	priv->curr_ch = ch;

	return RIG_OK;
}

/*
 * drake_vfo_op
 * Assumes rig!=NULL
 */
int drake_vfo_op(RIG *rig, vfo_t vfo, vfo_op_t op)
{
	struct drake_priv_data *priv = rig->state.priv;
	unsigned char buf[16], ackbuf[16];
	int len, ack_len, retval;

	switch(op) {
	case RIG_OP_UP:
		len = sprintf(buf,"U");
		break;
	case RIG_OP_DOWN:
		len = sprintf(buf,"D");
		break;
	case RIG_OP_CPY:
		len = sprintf(buf,"A E B"EOM);
		break;
	case RIG_OP_TO_VFO:
		len = sprintf(buf,"C%03d", priv->curr_ch);
		break;
	case RIG_OP_MCL:
		len = sprintf(buf,"EC%03d"EOM, priv->curr_ch);
		break;
	case RIG_OP_FROM_VFO:
		len = sprintf(buf,"PR"EOM"%03d"EOM, priv->curr_ch);
		break;
	default:
		return -RIG_EINVAL;
	}

	retval = drake_transaction(rig, buf, len, buf[len-1]==0x0d ?ackbuf:NULL, &ack_len);

	return retval;
}

/*
 * drake_set_func
 * Assumes rig!=NULL
 */
int drake_set_func(RIG *rig, vfo_t vfo, setting_t func, int status)
{
	unsigned char buf[16], ackbuf[16];
	int len, ack_len, retval;

	switch(func) {
	case RIG_FUNC_MN:
		len = sprintf(buf,"N%c"EOM, status?'O':'F');
		break;
	case RIG_FUNC_LOCK:
		len = sprintf(buf,"L%c"EOM, status?'O':'F');
		break;
	case RIG_FUNC_NB:
		/* TODO: NB narrow */
		len = sprintf(buf,"B%c"EOM, status?'W':'F');
		break;
	default:
		return -RIG_EINVAL;
	}

	retval = drake_transaction(rig, buf, len, ackbuf, &ack_len);

	return retval;
}

/*
 * drake_set_level
 * Assumes rig!=NULL
 */
int drake_set_level(RIG *rig, vfo_t vfo, setting_t level, value_t val)
{
	unsigned char buf[16], ackbuf[16];
	int len, ack_len, retval;

	switch(level) {
	case RIG_LEVEL_PREAMP:
		len = sprintf(buf,"G%c"EOM, val.i?'+':'0');
		break;
	case RIG_LEVEL_ATT:
		len = sprintf(buf,"G%c"EOM, val.i?'-':'0');
		break;
	case RIG_LEVEL_AGC:
		len = sprintf(buf,"A%c"EOM,
				val.i==RIG_AGC_OFF?'O':
				(val.i==RIG_AGC_FAST?'F':'S'));
		break;
	default:
		return -RIG_EINVAL;
	}

	retval = drake_transaction(rig, buf, len, ackbuf, &ack_len);

	return retval;
}

int drake_set_powerstat (RIG * rig, powerstat_t status)
{
	unsigned char buf[16], ackbuf[16];
	int len, ack_len, retval;

	len = sprintf(buf,"P%c"EOM, status==RIG_POWER_OFF?'F':'O');

	retval = drake_transaction(rig, buf, len, ackbuf, &ack_len);

	return retval;
}



/*
 * drake_set_freq
 * Assumes rig!=NULL
 */
const char *drake_get_info(RIG *rig)
{
	static unsigned char idbuf[BUFSZ];
	int retval, id_len;

	retval = drake_transaction(rig, "ID" EOM, 3, idbuf, &id_len);
	if (retval != RIG_OK)
		return NULL;

	idbuf[id_len] = '\0';

	return idbuf;
}

/*
 * initrigs_drake is called by rig_backend_load
 */
DECLARE_INITRIG_BACKEND(drake)
{
	rig_debug(RIG_DEBUG_VERBOSE, "drake: _init called\n");

	rig_register(&r8a_caps);
	rig_register(&r8b_caps);

	return RIG_OK;
}

/*
 * probe_drake(port_t *port, rig_probe_func_t cfunc, rig_ptr_t data)
 */
DECLARE_PROBERIG_BACKEND(drake)
{
	static unsigned char idbuf[BUFSZ];
	int retval, id_len;

	if (!port)
		return RIG_MODEL_NONE;

	if (port->type.rig != RIG_PORT_SERIAL)
		return RIG_MODEL_NONE;

	port->parm.serial.rate = r8b_caps.serial_rate_max;
	port->write_delay = port->post_write_delay = 0;
	port->timeout = 50;
	port->retry = 1;

	retval = serial_open(port);
	if (retval != RIG_OK)
		return RIG_MODEL_NONE;

	retval = write_block(port, "ID" EOM, 3);
	id_len = read_string(port, idbuf, BUFSZ, LF, 1);

	close(port->fd);

	if (retval != RIG_OK || id_len <= 0 || id_len >= BUFSZ)
		return RIG_MODEL_NONE;

	idbuf[id_len] = '\0';

	if (!strcmp(idbuf, "R8B")) {
		if (cfunc)
			(*cfunc)(port, RIG_MODEL_DKR8B, data);
		return RIG_MODEL_DKR8B;
	}
	if (!strcmp(idbuf, "R8A")) {	/* TBC */
		if (cfunc)
			(*cfunc)(port, RIG_MODEL_DKR8A, data);
		return RIG_MODEL_DKR8A;
	}

	/*
	 * not found... 
	 */
	if (memcmp(idbuf, "ID" EOM, 3)) /* catch loopback serial */
		rig_debug(RIG_DEBUG_VERBOSE,"probe_drake: found unknown device "
				"with ID '%s', please report to Hamlib "
				"developers.\n", idbuf);

	return RIG_MODEL_NONE;
}


