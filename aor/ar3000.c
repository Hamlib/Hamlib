/*
 *  Hamlib AOR backend - AR3000 description
 *  Copyright (c) 2000-2003 by Stephane Fillod
 *
 *	$Id: ar3000.c,v 1.3 2003-10-20 22:15:01 fillods Exp $
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
#include <string.h>

#include <hamlib/rig.h>
#include <serial.h>
#include <misc.h>

#include "aor.h"

int ar3k_set_freq(RIG *rig, vfo_t vfo, freq_t freq);
int ar3k_get_freq(RIG *rig, vfo_t vfo, freq_t *freq);
int ar3k_set_mode(RIG *rig, vfo_t vfo, rmode_t mode, pbwidth_t width);
int ar3k_get_mode(RIG *rig, vfo_t vfo, rmode_t *mode, pbwidth_t *width);
int ar3k_set_ts(RIG *rig, vfo_t vfo, shortfreq_t ts);
int ar3k_get_ts(RIG *rig, vfo_t vfo, shortfreq_t *ts);


#define AR3000A_MODES (RIG_MODE_AM|RIG_MODE_CW|RIG_MODE_SSB|RIG_MODE_FM|RIG_MODE_WFM)

#define AR3000A_FUNC_ALL (RIG_FUNC_MUTE)

#define AR3000A_LEVEL (RIG_LEVEL_ATT|RIG_LEVEL_SQLSTAT|RIG_LEVEL_STRENGTH)

#define AR3000A_VFO_OPS (RIG_OP_NONE)

#define AR3000A_VFO (RIG_VFO_A)

/*
 * AR3000A rig capabilities.
 *
 * info coming from A3000A pdf manual from http://www.aoruk.com/
 *
 * TODO:
 * set_mem, set_channel, get_channel, set_level ATT, get_level ATT,STR,
 * set_bank, ..
 */
const struct rig_caps ar3000a_caps = {
.rig_model =  RIG_MODEL_AR3000A,
.model_name = "AR3000A",
.mfg_name =  "AOR",
.version =  "0.1",
.copyright =  "LGPL",
.status =  RIG_STATUS_UNTESTED,
.rig_type =  RIG_TYPE_SCANNER,
.ptt_type =  RIG_PTT_NONE,
.dcd_type =  RIG_DCD_NONE,
.port_type =  RIG_PORT_SERIAL,
.serial_rate_min =  4800,
.serial_rate_max =  9600,
.serial_data_bits =  8,
.serial_stop_bits =  2,
.serial_parity =  RIG_PARITY_NONE,
.serial_handshake =  RIG_HANDSHAKE_HARDWARE,
.write_delay =  0,
.post_write_delay =  0,
.timeout =  200,
.retry =  3,
.has_get_func =  RIG_FUNC_NONE,
.has_set_func =  AR3000A_FUNC_ALL,
.has_get_level =  AR3000A_LEVEL,
.has_set_level =  RIG_LEVEL_SET(AR3000A_LEVEL),
.has_get_parm =  RIG_PARM_NONE,
.has_set_parm =  RIG_PARM_NONE,
.level_gran =  {},                 /* FIXME: granularity */
.parm_gran =  {},
.ctcss_list =  NULL,
.dcs_list =  NULL,
.preamp =   { RIG_DBLST_END, },
.attenuator =   { 20, RIG_DBLST_END, },	/* TBC */
.max_rit =  Hz(0),
.max_xit =  Hz(0),
.max_ifshift =  Hz(0),
.targetable_vfo =  0,
.transceive =  RIG_TRN_OFF,
.bank_qty =   4,
.chan_desc_sz =  0,
.vfo_ops =  AR3000A_VFO_OPS,

.chan_list =  { RIG_CHAN_END, },	/* FIXME: memory channel list: 4x100 memories */

.rx_range_list1 =  {
	{kHz(100),MHz(2036),AR3000A_MODES,-1,-1,AR3000A_VFO},
	RIG_FRNG_END,
	},
.tx_range_list1 =  { RIG_FRNG_END, },

.rx_range_list2 =  {
	{kHz(100),MHz(2036),AR3000A_MODES,-1,-1,AR3000A_VFO},
	RIG_FRNG_END,
  }, /* rx range */
.tx_range_list2 =  { RIG_FRNG_END, },	/* no tx range, this is a scanner! */

.tuning_steps =  {
	 {AR3000A_MODES,50},
	 {AR3000A_MODES,kHz(999.95)},
#if 0 
	 {AR3000A_MODES,0},	/* any tuning step */
#endif
	 RIG_TS_END,
	},
        /* mode/filter list, .remember =  order matters! */
.filters =  {
        /* mode/filter list, .remember =  order matters! */
		{RIG_MODE_SSB|RIG_MODE_CW, kHz(2.4)}, 
		{RIG_MODE_FM|RIG_MODE_AM, kHz(12)}, 
		{RIG_MODE_WFM, kHz(180)},
		RIG_FLT_END,
	},

.priv =  NULL,
.rig_init =  NULL,
.rig_cleanup =  NULL,
.rig_open =  NULL,
.rig_close =  NULL,

.set_freq =  ar3k_set_freq,
.get_freq =  ar3k_get_freq,
.set_mode =  ar3k_set_mode,
.get_mode =  ar3k_get_mode,

.set_ts =  ar3k_set_ts,
.get_ts =  ar3k_get_ts,

};

/*
 * Function definitions below
 */




/*
 * acknowledge is CR
 * Is \r portable enough?
 */
#define CR '\r'
#define EOM "\x0a\x0d"

#define BUFSZ 64

/*
 * ar3k_transaction
 * We assume that rig!=NULL, rig->state!= NULL
 * Otherwise, you'll get a nice seg fault. You've been warned!
 * return value: RIG_OK if everything's fine, negative value otherwise
 * TODO: error case handling
 */
int ar3k_transaction(RIG *rig, const char *cmd, int cmd_len, char *data, int *data_len)
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
 * ar3k_set_freq
 * Assumes rig!=NULL
 */
int ar3k_set_freq(RIG *rig, vfo_t vfo, freq_t freq)
{
	unsigned char freqbuf[BUFSZ];
	int freq_len, retval;
	unsigned lowhz;

	/*
	 * actually, frequency must be like nnnn.nnnnm, 
	 * where m must be 0 or 5 (for 50Hz).
	 */
	lowhz = ((unsigned)freq) % 100;
	freq /= 100;
	if (lowhz < 25)
		lowhz = 0;
	else if (lowhz < 75)
		lowhz = 50;
	else 
		lowhz = 100;
	freq = freq*100 + lowhz;

	freq_len = sprintf(freqbuf,"%04.5f" EOM, ((double)freq)/MHz(1));

	retval = ar3k_transaction (rig, freqbuf, freq_len, NULL, NULL);
	if (retval != RIG_OK)
		return retval;

	return RIG_OK;
}

/*
 * ar3k_get_freq
 * Assumes rig!=NULL, freq!=NULL
 */
int ar3k_get_freq(RIG *rig, vfo_t vfo, freq_t *freq)
{
	char *rfp;
	int freq_len, retval;
	unsigned char freqbuf[BUFSZ];

	/*
	 * #--J0WZ-----Y---------Q
	 *      X                R,S,T,U,V
	 */
	retval = ar3k_transaction (rig, "D" EOM, 3, freqbuf, &freq_len);
	if (retval != RIG_OK)
		return retval;

	rfp = strchr(freqbuf, 'Y');
	if (!rfp)
		return -RIG_EPROTO;
	sscanf(rfp+1,"%"FREQFMT, freq);
	*freq *= 10;

	return RIG_OK;
}

/*
 * ar3k_set_mode
 * Assumes rig!=NULL
 */
int ar3k_set_mode(RIG *rig, vfo_t vfo, rmode_t mode, pbwidth_t width)
{
	unsigned char mdbuf[BUFSZ];
	int mdbuf_len, aormode, retval;

	switch (mode) {
	case RIG_MODE_AM:       aormode = 'A'; break;
	case RIG_MODE_CW:       aormode = 'C'; break;
	case RIG_MODE_USB:      aormode = 'L'; break;
	case RIG_MODE_LSB:      aormode = 'L'; break;
	case RIG_MODE_WFM:      aormode = 'W'; break;
	case RIG_MODE_FM:	aormode = 'N'; break;
	default:
		rig_debug(RIG_DEBUG_ERR,"ar3k_set_mode: unsupported mode %d\n",
						mode);
		return -RIG_EINVAL;
	}

	mdbuf_len = sprintf(mdbuf, "%c" EOM, aormode);
	retval = ar3k_transaction (rig, mdbuf, mdbuf_len, NULL, NULL);

	return retval;
}

/*
 * ar3k_get_mode
 * Assumes rig!=NULL, mode!=NULL
 */
int ar3k_get_mode(RIG *rig, vfo_t vfo, rmode_t *mode, pbwidth_t *width)
{
	char *rfp;
	int buf_len, retval;
	unsigned char buf[BUFSZ];

	/*
	 * #--J0WZ-----Y---------Q
	 *      X                R,S,T,U,V
	 */
	retval = ar3k_transaction (rig, "D" EOM, 3, buf, &buf_len);
	if (retval != RIG_OK)
		return retval;

	rfp = strchr(buf, 'Y');
	if (!rfp)
		return -RIG_EPROTO;
	rfp += 10;

	switch (*rfp) {
	case 'Q':	*mode = RIG_MODE_FM; break;
	case 'R':	*mode = RIG_MODE_WFM; break;
	case 'S':	*mode = RIG_MODE_AM; break;
	case 'T':	*mode = RIG_MODE_LSB; break;
	case 'U':	*mode = RIG_MODE_USB; break;
	case 'V':	*mode = RIG_MODE_CW; break;
	default:
		rig_debug(RIG_DEBUG_ERR,"ar3k_get_mode: unsupported mode '%c'\n",
							*rfp);
		return -RIG_EPROTO;
	}
	*width = rig_passband_normal(rig, *mode);

	return RIG_OK;
}

/*
 * ar3k_set_ts
 * Assumes rig!=NULL
 */
int ar3k_set_ts(RIG *rig, vfo_t vfo, shortfreq_t ts)
{
	unsigned char freqbuf[BUFSZ];
	int freq_len, retval;
	int lowhz;

	/*
	 * actually, frequency must be like nnn.nm, 
	 * where m must be 0 or 5 (for 50Hz).
	 */
	lowhz = ts % 100;
	ts /= 100;
	if (lowhz < 25)
		lowhz = 0;
	else if (lowhz < 75)
		lowhz = 50;
	else 
		lowhz = 100;
	ts = ts*100 + lowhz;

	freq_len = sprintf(freqbuf,"%03.2f" EOM, ((double)ts)/kHz(1));

	retval = ar3k_transaction (rig, freqbuf, freq_len, NULL, NULL);
	if (retval != RIG_OK)
		return retval;

	return RIG_OK;
}

/*
 * ar3k_get_ts
 * Assumes rig!=NULL, ts!=NULL
 */
int ar3k_get_ts(RIG *rig, vfo_t vfo, shortfreq_t *ts)
{
	char *rfp;
	int freq_len, retval;
	unsigned char freqbuf[BUFSZ];

	/*
	 * #--J0WZ-----Y---------Q
	 *      X                R,S,T,U,V
	 */
	retval = ar3k_transaction (rig, "D" EOM, 3, freqbuf, &freq_len);
	if (retval != RIG_OK)
		return retval;

	rfp = strchr(freqbuf, 'Z');
	if (!rfp)
		return -RIG_EPROTO;
	sscanf(rfp+1,"%ld", ts);
	*ts *= 10;

	return RIG_OK;
}


