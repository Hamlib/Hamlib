/*
 *  Hamlib Kenwood backend - TH-D7 description
 *  Copyright (c) 2000,2001 by Stephane Fillod
 *
 *		$Id: thd7.c,v 1.4 2002-01-02 23:41:44 fillods Exp $
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
#include <hamlib/riglist.h>
#include "kenwood.h"
#include "th.h"

#if 1
#define RIG_ASSERT(x)	if (!(x)) { rig_debug(RIG_DEBUG_ERR, "Assertion failed on line %i\n",__LINE__); abort(); }
#else
#define RIG_ASSERT(x)
#endif


static int thd7_set_vfo(RIG *rig, vfo_t vfo);
static int thd7_get_vfo(RIG *rig, vfo_t *vfo);
static int thd7_set_mode(RIG *rig, vfo_t vfo, rmode_t mode, pbwidth_t width);
static int thd7_get_mode(RIG *rig, vfo_t vfo, rmode_t *mode, pbwidth_t *width);
static int thd7_set_ctcss_tone(RIG *rig, vfo_t vfo, tone_t tone);
static int thd7_get_ctcss_tone(RIG *rig, vfo_t vfo, tone_t *tone);

#define THD7_MODES	  (RIG_MODE_FM|RIG_MODE_AM)
#define THD7_MODES_TX (RIG_MODE_FM)

#define THD7_FUNC_ALL (RIG_FUNC_TSQL|RIG_FUNC_AIP|RIG_FUNC_SQL)

#define THD7_LEVEL_ALL (RIG_LEVEL_SQL|RIG_LEVEL_SQLSTAT|RIG_LEVEL_STRENGTH|RIG_LEVEL_AF|RIG_LEVEL_RF|RIG_LEVEL_MICGAIN)

#define THD7_VFO_OP (RIG_OP_UP|RIG_OP_DOWN)

/*
 * TODO: Band A & B
 */
#define THD7_VFO (RIG_VFO_A|RIG_VFO_C)

/*
 * th-d7a rig capabilities.
 */
const struct rig_caps thd7a_caps = {
rig_model: RIG_MODEL_THD7A,
model_name:"TH-D7A",
mfg_name: "Kenwood",
version: "0.1",
copyright: "LGPL",
status: RIG_STATUS_ALPHA,
rig_type: RIG_TYPE_HANDHELD|RIG_FLAG_APRS|RIG_FLAG_TNC|RIG_FLAG_DXCLUSTER,
ptt_type: RIG_PTT_RIG,
dcd_type: RIG_DCD_RIG,
port_type: RIG_PORT_SERIAL,
serial_rate_min: 9600,
serial_rate_max: 9600,
serial_data_bits: 8,
serial_stop_bits: 1,
serial_parity: RIG_PARITY_NONE,
serial_handshake: RIG_HANDSHAKE_NONE,
write_delay: 0,
post_write_delay: 0,
timeout: 200,
retry: 3,

has_get_func: THD7_FUNC_ALL,
has_set_func: THD7_FUNC_ALL,
has_get_level: THD7_LEVEL_ALL,
has_set_level: RIG_LEVEL_SET(THD7_LEVEL_ALL),
has_get_parm: RIG_PARM_NONE,
has_set_parm: RIG_PARM_NONE,    /* FIXME: parms */
level_gran: {},                 /* FIXME: granularity */
parm_gran: {},
ctcss_list: kenwood38_ctcss_list,
dcs_list: NULL,
preamp:  { RIG_DBLST_END, },
attenuator:  { RIG_DBLST_END, },
max_rit: Hz(0),
max_xit: Hz(0),
max_ifshift: Hz(0),
vfo_ops: THD7_VFO_OP,
targetable_vfo: RIG_TARGETABLE_FREQ,
transceive: RIG_TRN_RIG,
bank_qty:  0,
chan_desc_sz: 0,


chan_list: { {  1,  200, RIG_MTYPE_MEM, 0 },
			 RIG_CHAN_END,
		   },	/* FIXME: memory channel list: 200 memories */

rx_range_list1: { RIG_FRNG_END, },    /* FIXME: enter region 1 setting */
tx_range_list1: { RIG_FRNG_END, },
rx_range_list2: {
    {MHz(144),MHz(148),THD7_MODES,-1,-1,THD7_VFO},
    {MHz(430),MHz(440),THD7_MODES,-1,-1,THD7_VFO},
	RIG_FRNG_END,
  }, /* rx range */
tx_range_list2: {
    {MHz(144),MHz(148),THD7_MODES_TX,W(5.5),W(5.5),THD7_VFO},
    {MHz(430),MHz(440),THD7_MODES_TX,W(5.5),W(5.5),THD7_VFO},
	RIG_FRNG_END,
  }, /* tx range */
tuning_steps: {
	 {THD7_MODES,kHz(5)},
	 {THD7_MODES,kHz(6.25)},
	 {THD7_MODES,kHz(10)},
	 {THD7_MODES,kHz(12.5)},
	 {THD7_MODES,kHz(15)},
	 {THD7_MODES,kHz(20)},
	 {THD7_MODES,kHz(25)},
	 {THD7_MODES,kHz(30)},
	 {THD7_MODES,kHz(50)},
	 {THD7_MODES,kHz(100)},
	 RIG_TS_END,
	},
        /* mode/filter list, remember: order matters! */
filters: {
		{RIG_MODE_FM, kHz(14)},
		{RIG_MODE_AM, kHz(9)},
		RIG_FLT_END,
	},
priv: NULL,

set_freq: th_set_freq,
get_freq: th_get_freq,
set_mode: thd7_set_mode,
get_mode: thd7_get_mode,
set_vfo: thd7_set_vfo,
get_vfo: thd7_get_vfo,
set_ctcss_tone: thd7_set_ctcss_tone,
get_ctcss_tone: thd7_get_ctcss_tone,

/* Untested and will most probably not work! *
set_ptt: kenwood_set_ptt,
get_dcd: kenwood_get_dcd,
vfo_op: kenwood_vfo_op,
set_mem: kenwood_set_mem,
get_mem: kenwood_get_mem,
 */

};

#define EOM "\r"


/*
 * thd7_set_vfo
 * Assumes rig!=NULL
 */
static int
thd7_set_vfo(RIG *rig, vfo_t vfo)
{
	unsigned char vsel, vfobuf[16], ackbuf[24];
	int retval, vfo_len, ack_len = 0;

	rig_debug(RIG_DEBUG_TRACE, __FUNCTION__" called\n");
	RIG_ASSERT(rig != NULL);

	switch (vfo) {
	  case RIG_VFO_A: vsel = '0'; break;
	  case RIG_VFO_B: vsel = '1'; break;
	  default:
		rig_debug(RIG_DEBUG_ERR, __FUNCTION__": unsupported VFO %d\n", vfo);
		return -RIG_EINVAL;
	}

	vfo_len = sprintf(vfobuf, "BC %c" EOM, vsel);
	retval = kenwood_transaction (rig, vfobuf, vfo_len, ackbuf, &ack_len);
	if (retval != RIG_OK)
		return retval;

	if (ackbuf[0] == 'N' && ackbuf[1] == '\n') {
		return -RIG_ERJCTED;
	}
	return RIG_OK;
}

/*
 * thd7_get_vfo
 * Assumes rig!=NULL
 */
static int
thd7_get_vfo(RIG *rig, vfo_t *vfo)
{
	unsigned char ackbuf[16];
	int retval, ack_len = 0;

	rig_debug(RIG_DEBUG_TRACE, __FUNCTION__" called\n");
	RIG_ASSERT(rig != NULL);

	retval = kenwood_transaction (rig, "BC" EOM, 3, ackbuf, &ack_len);
	if (retval != RIG_OK)
		return retval;

//	if (ack_len != 5) {	/* for some reason ack_len is zero */
	if (ackbuf[0] != 'B' || ackbuf[1] != 'C' || ackbuf[2] != ' ') {
		rig_debug(RIG_DEBUG_ERR, __FUNCTION__": unexpected reply "
		                        "'%s', len=%d\n", ackbuf, ack_len);
		return -RIG_ERJCTED;
	}

	switch (ackbuf[3]) {
	  case '0': *vfo = RIG_VFO_A; break;
	  case '1':	*vfo = RIG_VFO_B; break;
	  default:
		rig_debug(RIG_DEBUG_ERR, __FUNCTION__": unexpected VFO value %c\n",
								ackbuf[3]);
		return -RIG_ERJCTED;
	}

	return RIG_OK;
}

/*
 * thd7_set_mode
 * Assumes rig!=NULL
 */
static int
thd7_set_mode(RIG *rig, vfo_t vfo, rmode_t mode, pbwidth_t width)
{
	unsigned char mdbuf[24], ackbuf[24];
    int retval, kmode, mdbuf_len, ack_len = 0;

	rig_debug(RIG_DEBUG_TRACE, __FUNCTION__" called\n");
    RIG_ASSERT(rig != NULL);
	
	switch (mode) {
	case RIG_MODE_FM:   kmode = '0'; break;
	case RIG_MODE_AM:   kmode = '1'; break;
	default:
		rig_debug(RIG_DEBUG_ERR,__FUNCTION__": unsupported mode %d\n", mode);
		return -RIG_EINVAL;
	}
	mdbuf_len = sprintf(mdbuf, "MD %c" EOM, kmode);
	retval = kenwood_transaction (rig, mdbuf, mdbuf_len, ackbuf, &ack_len);
	if (retval != RIG_OK)
		return retval;

	if (ackbuf[0] == 'N' && ackbuf[1] == '\n') {
		rig_debug(RIG_DEBUG_ERR, __FUNCTION__": ack NG, len=%d\n", ack_len);
		return -RIG_ERJCTED;
	}

	return retval;
}

/*
 * thd7_get_mode
 * Assumes rig!=NULL, mode!=NULL
 */
static int
thd7_get_mode(RIG *rig, vfo_t vfo, rmode_t *mode, pbwidth_t *width)
{
	int retval, ack_len = 0;
	unsigned char ackbuf[24];

	rig_debug(RIG_DEBUG_TRACE, __FUNCTION__": called\n");
	RIG_ASSERT(rig != NULL);
	RIG_ASSERT(mode != NULL);
	memset(ackbuf, 0, sizeof(ackbuf)); /* for terminating zero of string */

	retval = kenwood_transaction (rig, "MD" EOM, 3, ackbuf, &ack_len);
	if (retval != RIG_OK)
		return retval;

	/*if (ack_len != 5) { */  /* ack_len does not work. */
	if (ackbuf[0] == 'N' && ackbuf[1] == '\n') {
		rig_debug(RIG_DEBUG_ERR, __FUNCTION__": ack NG, len=%d\n", ack_len);
		return -RIG_ERJCTED;
	}

	if (ackbuf[0] != 'M' || ackbuf[1] != 'D' || ackbuf[2] != ' ') {
		rig_debug(RIG_DEBUG_ERR, __FUNCTION__": unknown replay (%i) %s\n",
                                             ack_len, ackbuf);
        return -RIG_ERJCTED;
    }

	if (width)
		*width = RIG_PASSBAND_NORMAL;

	switch (ackbuf[3]) {
	  case '0':	*mode = RIG_MODE_FM; break;
	  case '1': *mode = RIG_MODE_AM; break;
	  default:
		rig_debug(RIG_DEBUG_ERR, __FUNCTION__": unsupported mode %c\n", ackbuf[3]);
		return -RIG_EINVAL;
	}
	return RIG_OK;
}

#ifndef RIG_TONEMAX
#define RIG_TONEMAX	38
#endif

/*
 * thd7_set_ctcss_tone
 * Assumes rig!=NULL, rig->caps->ctcss_list != NULL
 */
static int
thd7_set_ctcss_tone(RIG *rig, vfo_t vfo, tone_t tone)
{
	const struct rig_caps *caps;
	unsigned char tonebuf[24], ackbuf[24];
	int retval, tone_len, ack_len = 0;
	int i;

	rig_debug(RIG_DEBUG_TRACE, __FUNCTION__": called\n");
	RIG_ASSERT(rig != NULL);
	RIG_ASSERT(rig->caps != NULL && rig->caps->ctcss_list != NULL);
	memset(ackbuf, 0, sizeof(ackbuf));

	caps = rig->caps;

	for (i = 0; caps->ctcss_list[i] != 0 && i < RIG_TONEMAX; i++) {
		if (caps->ctcss_list[i] == tone)
			break;
	}
	if (caps->ctcss_list[i] != tone)
		return -RIG_EINVAL;

	i += (i == 0) ? 1 : 2;	/* Correct for TH-7D anomally */
	tone_len = sprintf(tonebuf, "CTN %02d" EOM, i);
	retval = kenwood_transaction (rig, tonebuf, tone_len, ackbuf, &ack_len);
	if (retval != RIG_OK)
		return retval;

	/* error/result checking */
	if (ackbuf[0] == 'N' && ackbuf[1] == '\n') {
		rig_debug(RIG_DEBUG_ERR, __FUNCTION__": negative acknowledgment\n");
		return -RIG_ERJCTED;
	}
	if (ackbuf[0] != 'C' || ackbuf[1] != 'T' || ackbuf[2] != 'N') {
		rig_debug(RIG_DEBUG_ERR, __FUNCTION__": unexpected reply "
			                   "'%s', len=%d\n", tonebuf, tone_len);
		return -RIG_ERJCTED;
	}
	return RIG_OK;
}

/*
 * thd7_get_ctcss_tone
 * Assumes rig!=NULL, rig->caps!=NULL
 */
static int
thd7_get_ctcss_tone(RIG *rig, vfo_t vfo, tone_t *tone)
{
	const struct rig_caps *caps;
	unsigned char tonebuf[24];
	int retval, tone_len = 0;
	unsigned int tone_idx;

	rig_debug(RIG_DEBUG_TRACE, __FUNCTION__": called\n");
	RIG_ASSERT(rig != NULL);
	RIG_ASSERT(rig->caps != NULL && rig->caps->ctcss_list != NULL);
	memset(tonebuf, 0, sizeof(tonebuf));    /* for terminating zero of string */

	caps = rig->caps;

	retval = kenwood_transaction (rig, "CTN" EOM, 4, tonebuf, &tone_len);
	if (retval != RIG_OK)
		return retval;

	if (tonebuf[0] != 'C' || tonebuf[1] != 'T' || tonebuf[2] != 'N') {
		rig_debug(RIG_DEBUG_ERR, __FUNCTION__": unexpected reply "
								"'%s', len=%d\n", tonebuf, tone_len);
		return -RIG_ERJCTED;
	}

	sscanf(tonebuf+3, "%u", (int*)&tone_idx);

    /* verify tone index for TH-7D rig */
	if (tone_idx <= 0 || tone_idx == 2 || tone_idx > 39) {
		rig_debug(RIG_DEBUG_ERR, __FUNCTION__
						": Unexpected CTCSS no (%04d)\n", tone_idx);
		return -RIG_EPROTO;
	}
	tone_idx -= (tone_idx == 1) ? 1 : 2; /* Correct for TH-7D anomaly */
	*tone = caps->ctcss_list[tone_idx];
	return RIG_OK;
}



