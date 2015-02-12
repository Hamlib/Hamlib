/*
 *  Hamlib Kenwood backend - Elecraft K3 description
 *  Copyright (c) 2002-2009 by Stephane Fillod
 *  Copyright (C) 2010,2011,2012,2013 by Nate Bargmann, n0nb@arrl.net
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
 *  See the file 'COPYING.LIB' in the main Hamlib distribution directory for
 *  the complete text of the GNU Lesser Public License version 2.1.
 *
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <stdlib.h>

#include <hamlib/rig.h>
#include "kenwood.h"
#include "bandplan.h"
#include "elecraft.h"
#include "token.h"
#include "cal.h"

#define K3_MODES (RIG_MODE_CW|RIG_MODE_CWR|RIG_MODE_SSB|\
	RIG_MODE_RTTY|RIG_MODE_RTTYR|RIG_MODE_FM|RIG_MODE_AM|RIG_MODE_PKTUSB|\
	RIG_MODE_PKTLSB)

#define K3_FUNC_ALL (RIG_FUNC_NB|RIG_FUNC_LOCK|RIG_FUNC_RIT|RIG_FUNC_XIT)

#define K3_LEVEL_ALL (RIG_LEVEL_ATT|RIG_LEVEL_PREAMP|RIG_LEVEL_AGC|RIG_LEVEL_SQL|\
	RIG_LEVEL_STRENGTH|RIG_LEVEL_RFPOWER|RIG_LEVEL_KEYSPD|\
	RIG_LEVEL_AF|RIG_LEVEL_RF|RIG_LEVEL_MICGAIN|RIG_LEVEL_RAWSTR)

#define K3_VFO (RIG_VFO_A|RIG_VFO_B)
#define K3_VFO_OP (RIG_OP_UP|RIG_OP_DOWN)

#define K3_ANTS (RIG_ANT_1|RIG_ANT_2)


/* kenwood_transaction() will add this to command strings
 * sent to the rig and remove it from strings returned from
 * the rig, so no need to append ';' manually to command strings.
 */
static struct kenwood_priv_caps k3_priv_caps  = {
	.cmdtrm =  EOM_KEN,
};


/* K3 specific function declarations */
int k3_get_mode(RIG *rig, vfo_t vfo, rmode_t *mode, pbwidth_t *width);
int k3_set_mode(RIG *rig, vfo_t vfo, rmode_t mode, pbwidth_t width);
int k3_set_vfo(RIG *rig, vfo_t vfo);
int k3_set_ext_level(RIG *rig, vfo_t vfo, token_t token, value_t val);
int k3_get_ext_level(RIG *rig, vfo_t vfo, token_t token, value_t *val);
int k3_set_rit(RIG * rig, vfo_t vfo, shortfreq_t rit);
int k3_set_xit(RIG * rig, vfo_t vfo, shortfreq_t rit);
int k3_set_split_mode(RIG *rig, vfo_t vfo, rmode_t tx_mode, pbwidth_t tx_width);
int k3_get_split_mode(RIG *rig, vfo_t vfo, rmode_t *tx_mode, pbwidth_t *tx_width);
int k3_get_level(RIG *rig, vfo_t vfo, setting_t level, value_t *val);
int k3_get_func(RIG *rig, vfo_t vfo, setting_t func, int *status);

/* Private helper functions */
int set_rit_xit(RIG *rig, shortfreq_t rit);


/*
 * K3 rig capabilities.
 * This kit can recognize a large subset of TS-570/K2 commands and has many
 * extensions of its own.  Extension backend functions to standard Kenwood
 * command are defined in elecraft.c (shared with K2) and in this file.
 *
 * Part of info comes from http://www.elecraft.com/K2_Manual_Download_Page.htm#K3
 * look for K3 Programmer's Reference PDF
 *
 * Wayne Burdick, N6KR, advises that the forthcoming KX3 (prototype shown at
 * Dayton Hamvention 2011) will have an identical command set to the K3.
 */
const struct rig_caps k3_caps = {
	.rig_model =		RIG_MODEL_K3,
	.model_name =		"K3/KX3",
	.mfg_name =		"Elecraft",
	.version =		"20130118",
	.copyright =		"LGPL",
	.status =		RIG_STATUS_BETA,
	.rig_type =		RIG_TYPE_TRANSCEIVER,
	.ptt_type =		RIG_PTT_RIG,
	.dcd_type =		RIG_DCD_RIG,
	.port_type =		RIG_PORT_SERIAL,
	.serial_rate_min =	4800,
	.serial_rate_max =	38400,
	.serial_data_bits =	8,
	.serial_stop_bits =	1,
	.serial_parity =	RIG_PARITY_NONE,
	.serial_handshake =	RIG_HANDSHAKE_NONE,
	.write_delay =		0,	/* Timing between bytes */
	.post_write_delay =	100,	/* Timing between command strings */
	.timeout =		600,	/* FA and FB make take up to 500 ms on band change */
	.retry =		3,

	.has_get_func =		K3_FUNC_ALL,
	.has_set_func =		K3_FUNC_ALL,
	.has_get_level =	K3_LEVEL_ALL,
	.has_set_level =	RIG_LEVEL_SET(K3_LEVEL_ALL),
	.has_get_parm =		RIG_PARM_NONE,
	.has_set_parm =		RIG_PARM_NONE,	/* FIXME: parms */
	.level_gran =		{},		/* FIXME: granularity */
	.parm_gran =		{},
	.extlevels = 		elecraft_ext_levels,
	.extparms =		kenwood_cfg_params,
	.preamp =		{ 14, RIG_DBLST_END, },
	.attenuator =		{ 10, RIG_DBLST_END, },
	.max_rit =		Hz(9990),
	.max_xit =		Hz(9990),
	.max_ifshift =		Hz(0),
	.vfo_ops =		K3_VFO_OP,
	.targetable_vfo =	RIG_TARGETABLE_FREQ,
	.transceive =		RIG_TRN_RIG,
	.bank_qty =		0,
	.chan_desc_sz =		0,

	.chan_list =		{ RIG_CHAN_END },

	.rx_range_list1 =  {
		{kHz(500), MHz(30), K3_MODES, -1, -1, K3_VFO, K3_ANTS},
		{ MHz(48), MHz(54), K3_MODES, -1,- 1, K3_VFO, K3_ANTS},
		RIG_FRNG_END,
	  }, /* rx range */
	.tx_range_list1 =  {
		FRQ_RNG_HF(1, K3_MODES, mW(10), W(10), K3_VFO, K3_ANTS),
		FRQ_RNG_6m(1, K3_MODES, mW(10), W(10), K3_VFO, K3_ANTS),
		RIG_FRNG_END,
	  }, /* tx range */

	.rx_range_list2 =  {
		{kHz(500), MHz(30), K3_MODES, -1, -1, K3_VFO, K3_ANTS},
		{ MHz(48), MHz(54), K3_MODES, -1, -1, K3_VFO, K3_ANTS},
		RIG_FRNG_END,
	  }, /* rx range */
	.tx_range_list2 =  {
		FRQ_RNG_HF(2, K3_MODES, mW(10), W(10), K3_VFO, K3_ANTS),
		FRQ_RNG_6m(2, K3_MODES, mW(10), W(10), K3_VFO, K3_ANTS),
		RIG_FRNG_END,
	  }, /* tx range */
	.tuning_steps =  {
		 {K3_MODES, 1},
		 RIG_TS_END,
		},

	/* mode/filter list, remember: order matters! */
	/* Values are arbitrary based on common K3 filter options. */
	.filters =  {
		{RIG_MODE_SSB, kHz(2.7)},
		{RIG_MODE_SSB, kHz(2.8)},
		{RIG_MODE_SSB, kHz(1.8)},
		{RIG_MODE_SSB, RIG_FLT_ANY},
		{RIG_MODE_CW|RIG_MODE_CWR, kHz(1)},
		{RIG_MODE_CW|RIG_MODE_CWR, kHz(2.8)},
		{RIG_MODE_CW|RIG_MODE_CWR, Hz(50)},
		{RIG_MODE_CW|RIG_MODE_CWR, RIG_FLT_ANY},
		{RIG_MODE_RTTY|RIG_MODE_RTTYR, kHz(2)},
		{RIG_MODE_RTTY|RIG_MODE_RTTYR, kHz(2.7)},
		{RIG_MODE_RTTY|RIG_MODE_RTTYR, Hz(500)},
		{RIG_MODE_RTTY|RIG_MODE_RTTYR, RIG_FLT_ANY},
		{RIG_MODE_PKTUSB|RIG_MODE_PKTLSB, kHz(2.7)},
		{RIG_MODE_PKTUSB|RIG_MODE_PKTLSB, kHz(2.8)},
		{RIG_MODE_PKTUSB|RIG_MODE_PKTLSB, Hz(50)},
		{RIG_MODE_PKTUSB|RIG_MODE_PKTLSB, RIG_FLT_ANY},
		{RIG_MODE_AM, kHz(6)},
		{RIG_MODE_AM, kHz(13)},
		{RIG_MODE_AM, kHz(2.7)},
		{RIG_MODE_AM, RIG_FLT_ANY},
		{RIG_MODE_FM, kHz(13)}, /* TBC */
		RIG_FLT_END,
	},
	.priv =  (void *)&k3_priv_caps,

	.rig_init =		kenwood_init,
	.rig_cleanup =		kenwood_cleanup,
	.rig_open =		elecraft_open,
	.set_freq =		kenwood_set_freq,
	.get_freq =		kenwood_get_freq,
	.set_mode =		k3_set_mode,
	.get_mode =		k3_get_mode,
	.set_vfo =		k3_set_vfo,
	.get_vfo =		kenwood_get_vfo_if,
	.set_split_mode =	k3_set_split_mode,
	.get_split_mode =	k3_get_split_mode,
	.set_split_vfo =	kenwood_set_split_vfo,
	.get_split_vfo =	kenwood_get_split_vfo_if,
	.set_rit =		k3_set_rit,
	.get_rit =		kenwood_get_rit,
	.set_xit =		k3_set_xit,
	.get_xit =		kenwood_get_xit,
	.get_ptt =		kenwood_get_ptt,
	.set_ptt =		kenwood_set_ptt,
	.get_dcd =		kenwood_get_dcd,
	.set_func =		kenwood_set_func,
	.get_func =		k3_get_func,
	.set_ext_parm =		kenwood_set_ext_parm,
	.get_ext_parm =		kenwood_get_ext_parm,
	.set_level =		kenwood_set_level,
	.get_level =		k3_get_level,
	.set_ext_level =	k3_set_ext_level,
	.get_ext_level =	k3_get_ext_level,
	.vfo_op =		kenwood_vfo_op,
	.set_trn =		kenwood_set_trn,
	.get_trn =		kenwood_get_trn,
	.set_powerstat =	kenwood_set_powerstat,
	.get_powerstat =	kenwood_get_powerstat,
	.set_ant =		kenwood_set_ant_no_ack,
	.get_ant =		kenwood_get_ant,
	.send_morse =		kenwood_send_morse,

};


/*
 * K3 extension function definitions follow
 */


/* k3_get_mode()
 *
 * The K3 supports a new command, DT, to query the data submode so
 * RIG_MODE_PKTUSB and RIG_MODE_PKTLSB can be supported.
 */

int k3_get_mode(RIG *rig, vfo_t vfo, rmode_t *mode, pbwidth_t *width)
{
	rig_debug(RIG_DEBUG_VERBOSE, "%s called\n", __func__);

	if (!rig || !mode || !width)
		return -RIG_EINVAL;

	char buf[KENWOOD_MAX_BUF_LEN];
	int err;
	rmode_t temp_m;
	pbwidth_t temp_w;

	err = kenwood_get_mode(rig, vfo, &temp_m, &temp_w);
	if (err != RIG_OK)
		return err;

	if (temp_m == RIG_MODE_RTTY)
	  {
	    err = kenwood_safe_transaction(rig, "DT", buf, KENWOOD_MAX_BUF_LEN, 3);
	    if (err != RIG_OK)
	      {
		rig_debug(RIG_DEBUG_VERBOSE, "%s: Cannot read K3 DT value\n",
			  __func__);
		return err;
	      }
	    switch(atoi(&buf[2]))
	      {
	      case K3_MODE_DATA_A:
	      case K3_MODE_PSK_D:
		*mode = RIG_MODE_PKTUSB;
		break;
	      case K3_MODE_AFSK_A:
		*mode = RIG_MODE_PKTLSB;
		break;
	      default:
		*mode = temp_m;
		break;
	      }
	  }
	else if (temp_m == RIG_MODE_RTTYR)
	  {
	    err = kenwood_safe_transaction(rig, "DT", buf, KENWOOD_MAX_BUF_LEN, 3);
	    if (err != RIG_OK)
	      {
		rig_debug(RIG_DEBUG_VERBOSE, "%s: Cannot read K3 DT value\n",
			  __func__);
		return err;
	      }
	    switch(atoi(&buf[2]))
	      {
	      case K3_MODE_DATA_A:
	      case K3_MODE_PSK_D:
		*mode = RIG_MODE_PKTLSB;
		break;
	      case K3_MODE_AFSK_A:
		*mode = RIG_MODE_PKTUSB;
		break;
	      case K3_MODE_FSK_D:
	      default:
		*mode = temp_m;
		break;
	      }
	  }
	else
	  {
		*mode = temp_m;
	  }

	/* The K3 is not limited to specific filter widths so we can query
	 * the actual bandwidth using the BW command
	 */
	err = kenwood_safe_transaction(rig, "BW", buf, KENWOOD_MAX_BUF_LEN, 6);
	if (err != RIG_OK) {
		rig_debug(RIG_DEBUG_VERBOSE, "%s: Cannot read K3 BW value\n", __func__);
		return err;
	}
	*width = atoi(&buf[2]) * 10;

	return RIG_OK;
}


/* k3_set_mode()
 *
 * As with k3_get_mode(), the K3 can also set the data sub-modes which
 * allows use of RIG_MODE_PKTUSB and RIG_MODE_PKTLSB.
 *
 * The K3 supports AFSK & FSK sub-modes and for the D versions it also
 * has an internal RTTY and PSK31 decoder. The decoder sub-modes are
 * reported as FSK (RTTY) and the AFSK sub-modes are reported as
 * PKT(USB & LSB). LSB modes are assumed to be RTTY and USB modes are
 * assumed to be PKT(PSK, WS modes etc.).
 *
 * For mode set the data sub-modes are set as follows:
 *
 * RTTY -> FSK D normal (LSB) VFO shows MARK QRG
 * RTTYR -> FSK D reversed (USB) VFO shows MARK QRG
 * PKTUSB -> DATA A normal (USB) VFO shows suppressed carrier QRG
 * PKTLSB -> AFSK A normal (LSB) optimised for RTTY VFO shows MARK QRG

 * Not all data sub-mode combinations are possible but the above
 * mapping seems most likely to cover the user requirements.
 */

int k3_set_mode(RIG *rig, vfo_t vfo, rmode_t mode, pbwidth_t width)
{
	rig_debug(RIG_DEBUG_VERBOSE, "%s called\n", __func__);

	if (!rig)
		return -RIG_EINVAL;

	int err;
	char cmd_m[4];
	char cmd_s[16];

	switch (mode) {
	case RIG_MODE_PKTLSB:
		mode = RIG_MODE_RTTY;
		strncpy(cmd_m, "DT1", 4); /* AFSK A mode - AFSK on LSB optimised for RTTY, VFO dial is MARK */
		break;
	case RIG_MODE_PKTUSB:
		mode = RIG_MODE_RTTY;
		strncpy(cmd_m, "DT0", 4); /* DATA A mode - AFSK on USB general, VFO dial is suppressed carrier QRG */
		break;
	case RIG_MODE_RTTY:
	case RIG_MODE_RTTYR:
		strncpy(cmd_m, "DT2", 4); /* FSK D mode - direct FSK keying, LSB is "normal", VFO dial is MARK */
		break;
	default:
		break;
	}

	/* kenwood_set_mode() ignores width value for K2/K3/TS-570 */
	err = kenwood_set_mode(rig, vfo, mode, width);
	if (err != RIG_OK)
		return err;

	/* and set the requested bandwidth.  On my K3, the bandwidth is rounded
	 * down to the nearest 50 Hz, i.e. sending BW0239; will cause the bandwidth
	 * to be set to 2.350 kHz.  As the width must be divided by 10, 10 Hz values
	 * between 0 and 4 round down to the nearest 100 Hz and values between 5
	 * and 9 round down to the nearest 50 Hz.
	 *
	 * width string value must be padded with leading '0' to equal four
	 * characters.
	 */

	/* passband widths vary by mode so gather lower and upper limits */
	pbwidth_t pb_nar = rig_passband_narrow(rig, mode);
	pbwidth_t pb_wid = rig_passband_wide(rig, mode);

	if (width < 0)
		width = labs(width);

	if (width == RIG_PASSBAND_NORMAL)
		width = rig_passband_normal(rig, mode);
	else if (width < pb_nar)
		width = pb_nar;
	else if (width > pb_wid)
		width = pb_wid;

	sprintf(cmd_s, "BW%04ld", width / 10);
	err = kenwood_simple_cmd(rig, cmd_s);
	if (err != RIG_OK)
		return err;

	/* Now set data sub-mode.  K3 needs to be in a DATA mode before setting
	 * the sub-mode.
	 */
	if (mode == RIG_MODE_PKTLSB || mode == RIG_MODE_PKTUSB
	    || mode == RIG_MODE_RTTY || mode == RIG_MODE_RTTYR)
	  {
	    err = kenwood_simple_cmd(rig, cmd_m);
	    if (err != RIG_OK)
	      return err;
	  }

	return RIG_OK;
}


/* The K3 changes "VFOs" by swapping the contents of
 * the upper display with the lower display.  This function
 * accomplishes this by sending the emulation command, SWT11;
 * to the K3 to emulate a tap of the A/B button.
 */

int k3_set_vfo(RIG *rig, vfo_t vfo)
{
	rig_debug(RIG_DEBUG_VERBOSE, "%s called\n", __func__);

	if (!rig)
		return -RIG_EINVAL;

	int err;

	switch (vfo) {
	case RIG_VFO_B:
		err = kenwood_simple_cmd(rig, "SWT11");
		if (err != RIG_OK)
			return err;
		break;
	default:
		break;
	}

	return RIG_OK;
}


/* Support the RC command for clearing RIT/XIT,
 *
 * token	Defined in elecraft.h or this file
 * val		Type depends on token type from confparams structure:
 * 		NUMERIC: val.f
 * 		COMBO: val.i, starting from 0 Index to a string table.
 * 		STRING: val.cs for set, val.s for get
 * 		CHECKBUTTON: val.i 0/1
 *
 * See Private Elecraft extra levels definitions in elecraft.c and
 * private token #define in elecraft.h
 */
int k3_set_ext_level(RIG *rig, vfo_t vfo, token_t token, value_t val)
{
	rig_debug(RIG_DEBUG_VERBOSE, "%s called\n", __func__);

	if (!rig)
		return -RIG_EINVAL;

	int err;

	switch(token) {
	case TOK_RIT_CLR:
		/* Clear offset */
		err = kenwood_simple_cmd(rig, "RC");
		if (err != RIG_OK)
			return err;

		/* val is ignored for RC command */
		break;
	default:
		rig_debug(RIG_DEBUG_WARN, "%s: Unsupported set_ext_level %d\n",
			__func__, token);
		return -RIG_EINVAL;
	}

	return RIG_OK;
}


/* Support the FI command for reading the IF center frequency,
 * useful for panadapters and such that need to know the IF center.
 * TQ command is a quick transmit status query--K2/K3 only.
 *
 * token	Defined in elecraft.h or this file
 * val		Type depends on token type from confparams structure:
 * 		NUMERIC: val.f
 * 		COMBO: val.i, starting from 0 Index to a string table.
 * 		STRING: val.cs for set, val.s for get
 * 		CHECKBUTTON: val.i 0/1
 */
int k3_get_ext_level(RIG *rig, vfo_t vfo, token_t token, value_t *val)
{
	rig_debug(RIG_DEBUG_VERBOSE, "%s called\n", __func__);

	if (!rig || !val)
		return -RIG_EINVAL;

	char buf[KENWOOD_MAX_BUF_LEN];
	int err;
	const struct confparams *cfp;

	cfp = rig_ext_lookup_tok(rig, token);

	switch(token) {
	case TOK_IF_FREQ:
		err = kenwood_safe_transaction(rig, "FI", buf, KENWOOD_MAX_BUF_LEN, 6);
		if (err != RIG_OK)
			return err;
		if (cfp->type == RIG_CONF_NUMERIC) {
			val->f = 8210000.0 + (float)atoi(&buf[2]);
		} else {
			rig_debug(RIG_DEBUG_ERR, "%s: protocol error, invalid token type\n",
				__func__);
			return -RIG_EPROTO;
		}
		break;
	case TOK_TX_STAT:
		err = kenwood_safe_transaction(rig, "TQ", buf, KENWOOD_MAX_BUF_LEN, 3);
		if (err != RIG_OK)
			return err;
		if (cfp->type == RIG_CONF_CHECKBUTTON) {
			val->i = atoi(&buf[2]);
		} else {
			rig_debug(RIG_DEBUG_ERR, "%s: protocol error, invalid token type\n",
				__func__);
			return -RIG_EPROTO;
		}
		break;
	default:
		rig_debug(RIG_DEBUG_WARN, "%s: Unsupported get_ext_level %d\n",
			__func__, token);
		return -RIG_EINVAL;
	}

	return RIG_OK;
}


/*
 * k3_set_rit() -- Differs from from generic Kenwood function as K3 can set
 * RIT to an arbitrary offset.  When rit == 0, the RIT offset is cleared.
 */
int k3_set_rit(RIG * rig, vfo_t vfo, shortfreq_t rit)
{
	rig_debug(RIG_DEBUG_VERBOSE, "%s called\n", __func__);

	int err;

	if (!rig)
		return -RIG_EINVAL;

	err = set_rit_xit(rig, rit);
	if (err != RIG_OK)
		return err;

	return RIG_OK;
}


/*
 * k3_set_xit() -- Differs from from generic Kenwood function as K3 can set
 * XIT to an arbitrary offset.  When rit == 0, the XIT offset is cleared.
 */
int k3_set_xit(RIG * rig, vfo_t vfo, shortfreq_t rit)
{
	rig_debug(RIG_DEBUG_VERBOSE, "%s called\n", __func__);

	int err;

	if (!rig)
		return -RIG_EINVAL;

	err = set_rit_xit(rig, rit);
	if (err != RIG_OK)
		return err;

	return RIG_OK;
}


/*
 * The K3 *always* uses VFOB for TX.
 */
int k3_set_split_mode(RIG * rig, vfo_t vfo, rmode_t tx_mode, pbwidth_t tx_width)
{
  rig_debug(RIG_DEBUG_VERBOSE, "%s called\n", __func__);

  if (!rig)
    return -RIG_EINVAL;

  int err;
  char cmd_m[4];
  char cmd_s[16];

  switch (tx_mode)
    {
    case RIG_MODE_PKTLSB:
      tx_mode = RIG_MODE_RTTY;
      strncpy(cmd_m, "DT1", 4); /* AFSK A mode - AFSK on LSB optimised for RTTY, VFO dial is MARK */
      break;
    case RIG_MODE_PKTUSB:
      tx_mode = RIG_MODE_RTTY;
      strncpy(cmd_m, "DT0", 4); /* DATA A mode - AFSK on USB general, VFO dial is suppressed carrier QRG */
      break;
    case RIG_MODE_RTTY:
    case RIG_MODE_RTTYR:
      strncpy(cmd_m, "DT2", 4); /* FSK D mode - direct FSK keying, LSB is "normal", VFO dial is MARK */
      break;
    default:
      break;
    }

  struct kenwood_priv_caps *caps = kenwood_caps(rig);
  char buf[6];
  char kmode;

  kmode = rmode2kenwood(tx_mode, caps->mode_table);
  if (kmode < 0 ) {
    rig_debug(RIG_DEBUG_WARN, "%s: unsupported mode '%s'\n",
	      __func__, rig_strrmode(tx_mode));
    return -RIG_EINVAL;
  }

  sprintf(buf, "MD$%c", '0' + kmode);
  err = kenwood_simple_cmd(rig, buf);
  if (err != RIG_OK)
    return err;

  /* and set the requested bandwidth.  On my K3, the bandwidth is rounded
   * down to the nearest 50 Hz, i.e. sending BW0239; will cause the bandwidth
   * to be set to 2.350 kHz.  As the width must be divided by 10, 10 Hz values
   * between 0 and 4 round down to the nearest 100 Hz and values between 5
   * and 9 round down to the nearest 50 Hz.
   *
   * tx_width string value must be padded with leading '0' to equal four
   * characters.
   */

  /* passband widths vary by mode so gather lower and upper limits */
  pbwidth_t pb_nar = rig_passband_narrow(rig, tx_mode);
  pbwidth_t pb_wid = rig_passband_wide(rig, tx_mode);

  if (tx_width < 0)
    tx_width = labs(tx_width);

  if (tx_width == RIG_PASSBAND_NORMAL)
    tx_width = rig_passband_normal(rig, tx_mode);
  else if (tx_width < pb_nar)
    tx_width = pb_nar;
  else if (tx_width > pb_wid)
    tx_width = pb_wid;

  sprintf(cmd_s, "BW$%04ld", tx_width / 10);
  err = kenwood_simple_cmd(rig, cmd_s);
  if (err != RIG_OK)
    return err;

  /* Now set data sub-mode.  K3 needs to be in a DATA mode before setting
   * the sub-mode.
   */
  if (tx_mode == RIG_MODE_PKTLSB || tx_mode == RIG_MODE_PKTUSB
      || tx_mode == RIG_MODE_RTTY || tx_mode == RIG_MODE_RTTYR) {
    err = kenwood_simple_cmd(rig, cmd_m);
    if (err != RIG_OK)
      return err;
  }

  return RIG_OK;
}


/* The K3 *always* uses VFOB for TX.
 */
int k3_get_split_mode(RIG *rig, vfo_t vfo, rmode_t *tx_mode, pbwidth_t *tx_width)
{
  rig_debug (RIG_DEBUG_VERBOSE, "%s called\n", __func__);

  if (!rig || !tx_mode || !tx_width)
    return -RIG_EINVAL;

  char buf[KENWOOD_MAX_BUF_LEN];
  int err;
  rmode_t temp_m;

  struct kenwood_priv_caps *caps = kenwood_caps (rig);

  err = kenwood_safe_transaction(rig, "MD$", buf, KENWOOD_MAX_BUF_LEN, 4);
  if (err != RIG_OK)
    return err;

  temp_m = kenwood2rmode (buf[3] - '0', caps->mode_table);

  if (temp_m == RIG_MODE_RTTY) {
    err = kenwood_safe_transaction(rig, "DT", buf, KENWOOD_MAX_BUF_LEN, 3);
    if (err != RIG_OK) {
      rig_debug(RIG_DEBUG_VERBOSE, "%s: Cannot read K3 DT value\n",
		__func__);
      return err;
    }
    switch(atoi(&buf[2]))
      {
      case K3_MODE_DATA_A:
      case K3_MODE_PSK_D:
	*tx_mode = RIG_MODE_PKTUSB;
	break;
      case K3_MODE_AFSK_A:
	*tx_mode = RIG_MODE_PKTLSB;
	break;
      default:
	*tx_mode = temp_m;
	break;
      }
  } else if (temp_m == RIG_MODE_RTTYR) {
    err = kenwood_safe_transaction(rig, "DT", buf, KENWOOD_MAX_BUF_LEN, 3);
    if (err != RIG_OK) {
      rig_debug(RIG_DEBUG_VERBOSE, "%s: Cannot read K3 DT value\n",
		__func__);
      return err;
    }
    switch(atoi(&buf[2]))
      {
      case K3_MODE_DATA_A:
      case K3_MODE_PSK_D:
	*tx_mode = RIG_MODE_PKTLSB;
	break;
      case K3_MODE_AFSK_A:
	*tx_mode = RIG_MODE_PKTUSB;
	break;
      case K3_MODE_FSK_D:
	break;
      default:
	*tx_mode = temp_m;
	break;
      }
  } else {
    *tx_mode = temp_m;
  }

  /* The K3 is not limited to specific filter widths so we can query
   * the actual bandwidth using the BW$ command
   */
  err = kenwood_safe_transaction(rig, "BW$", buf, KENWOOD_MAX_BUF_LEN, 7);
  if (err != RIG_OK) {
    rig_debug(RIG_DEBUG_VERBOSE, "%s: Cannot read K3 BW$ value\n", __func__);
    return err;
  }
  *tx_width = atoi(&buf[3]) * 10;

  return RIG_OK;
}


/*
 * Handle S-meter (SM, SMH) level locally and pass rest to kenwood_get_level()
 */
int k3_get_level(RIG *rig, vfo_t vfo, setting_t level, value_t *val)
{
	rig_debug(RIG_DEBUG_VERBOSE, "%s called\n", __func__);

	if (!rig || !val)
		return -RIG_EINVAL;

	char lvlbuf[50];
	int retval;
	struct kenwood_priv_data *priv = rig->state.priv;

	switch (level) {
	case RIG_LEVEL_STRENGTH:
		/* As of FW rev 4.37 the K3 supports an 'SMH' command that
		 * offers a higher resolution, 0-100 (mine went to 106),
		 * rawstr value for more precise S-meter reporting.
		 */
		retval = strncmp(priv->fw_rev, "4.37", 4);
		if (retval < 0) {
			cal_table_t str_cal = K3_SM_CAL;

			retval = kenwood_safe_transaction(rig, "SM", lvlbuf, 10, 6);
			if (retval != RIG_OK)
				return retval;

			sscanf(lvlbuf+2, "%d", &val->i);	/* rawstr */

			val->i = (int) rig_raw2val(val->i, &str_cal);
		} else if (retval >= 0) {
			cal_table_t str_cal = K3_SMH_CAL;

			retval = kenwood_safe_transaction(rig, "SMH", lvlbuf, 10, 6);
			if (retval != RIG_OK)
				return retval;

			sscanf(lvlbuf+3, "%d", &val->i);	/* rawstr */

			val->i = (int) rig_raw2val(val->i, &str_cal);
		} else {
			rig_debug(RIG_DEBUG_ERR, "%s: Firmware version comparison failed!\n",
				  __func__);
			return -RIG_EINVAL;
		}

		break;

	default:
		retval = kenwood_get_level(rig, vfo, level, val);
		if (retval != RIG_OK)
			return retval;
		break;
	}

	return RIG_OK;
}


/*
 * Some functions, notably RIT and XIT On/Off status, can be queried
 * on the K3.  Those functions are handled here and others are passed
 * through to kenwood_get_func().
 */
int k3_get_func(RIG *rig, vfo_t vfo, setting_t func, int *status)
{
	rig_debug(RIG_DEBUG_VERBOSE, "%s called\n", __func__);

	if (!rig || !status)
		return -RIG_EINVAL;

	switch (func) {
	case RIG_FUNC_RIT:
		return get_kenwood_func(rig, "RT", status);

	case RIG_FUNC_XIT:
		return get_kenwood_func(rig, "XT", status);

	default:
		return kenwood_get_func(rig, vfo, func, status);
	}
}




/* Private K3 helper functions */

/*
 * set_rit_xit() -- Differs from from generic Kenwood function as K3 can set
 * RIT/XIT to an arbitrary offset.  When rit == 0, the RIT/XIT offset is
 * cleared.
 */
int set_rit_xit(RIG *rig, shortfreq_t rit)
{
	rig_debug(RIG_DEBUG_VERBOSE, "%s called\n", __func__);

	int err;
	char offs;
	char cmd[16];

	if (!rig)
		return -RIG_EINVAL;

	if (rit == 0) {
		/* Clear offset and return */
		err = kenwood_simple_cmd(rig, "RC");
		if (err != RIG_OK)
			return err;

		return RIG_OK;
	}

	/* Set offset */
	if (rit <= 9999 && rit >= -9999) {
		offs = (rit < 0) ? '-' : '+';
		snprintf(cmd, 8, "RO%c%04d", offs, abs((int)rit));

		err = kenwood_simple_cmd(rig, cmd);
		if (err != RIG_OK)
			return err;

	} else {
		return -RIG_EINVAL;
	}

	return RIG_OK;
}
