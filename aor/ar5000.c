/*
 *  Hamlib AOR backend - AR5000 description
 *  Copyright (c) 2000-2004 by Stephane Fillod
 *
 *	$Id: ar5000.c,v 1.5 2004-09-07 20:40:20 fillods Exp $
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
#include "aor.h"


#define AR5000_MODES (RIG_MODE_AM|RIG_MODE_CW|RIG_MODE_SSB|RIG_MODE_FM|RIG_MODE_WFM)

#define AR5000_FUNC_ALL (RIG_FUNC_TSQL|RIG_FUNC_ABM|RIG_FUNC_AFC)

#define AR5000_LEVEL (RIG_LEVEL_ATT|RIG_LEVEL_AGC|RIG_LEVEL_SQL|RIG_LEVEL_RAWSTR)

#define AR5000_PARM (RIG_PARM_APO|RIG_PARM_BACKLIGHT|RIG_PARM_BEEP)

#define AR5000_VFO_OPS (RIG_OP_UP|RIG_OP_DOWN)

#define AR5000_VFO (RIG_VFO_A|RIG_VFO_B)

/*
 * Data was obtained from AR5000 pdf on http://www.aoruk.com
 */
#define dBm2S9(x) ((x)+73)

#define AR5000_REAL_STR_CAL { 33, \
	{ \
		{  0x02, dBm2S9(-120) }, \
		{  0x06, dBm2S9(-111) }, \
		{  0x0b, dBm2S9(-110) }, \
		{  0x14, dBm2S9(-109) }, \
		{  0x1d, dBm2S9(-108) }, \
		{  0x25, dBm2S9(-107) }, \
		{  0x2b, dBm2S9(-106) }, \
		{  0x32, dBm2S9(-105) }, \
		{  0x38, dBm2S9(-104) }, \
		{  0x3d, dBm2S9(-103) }, \
		{  0x43, dBm2S9(-102) }, \
		{  0x49, dBm2S9(-101) }, \
		{  0x4e, dBm2S9(-100) }, \
		{  0x63, dBm2S9(-95) }, \
		{  0x71, dBm2S9(-90) }, \
		{  0x7f, dBm2S9(-85) }, \
		{  0x8b, dBm2S9(-80) }, \
		{  0x96, dBm2S9(-75) }, \
		{  0xa0, dBm2S9(-70) }, \
		{  0xaa, dBm2S9(-65) }, \
		{  0xb3, dBm2S9(-60) }, \
		{  0xbd, dBm2S9(-55) }, \
		{  0xc6, dBm2S9(-50) }, \
		{  0xce, dBm2S9(-45) }, \
		{  0xd7, dBm2S9(-40) }, \
		{  0xe0, dBm2S9(-35) }, \
		{  0xe8, dBm2S9(-30) }, \
		{  0xf1, dBm2S9(-25) }, \
		{  0xf8, dBm2S9(-20) }, \
		{  0xfa, dBm2S9(-15) }, \
		{  0xfd, dBm2S9(0) } \
	} }

static int ar5k_set_mode(RIG *rig, vfo_t vfo, rmode_t mode, pbwidth_t width);
static int ar5k_get_mode(RIG *rig, vfo_t vfo, rmode_t *mode, pbwidth_t *width);

/*
 * ar5000 rig capabilities.
 * Notice that some rigs share the same functions.
 * Also this struct is READONLY!
 *
 * part of info from http://www.aoruk.com/5000.htm
 */
const struct rig_caps ar5000_caps = {
.rig_model =  RIG_MODEL_AR5000,
.model_name = "AR5000",
.mfg_name =  "AOR",
.version =  BACKEND_VER ".1",
.copyright =  "LGPL",
.status =  RIG_STATUS_BETA,
.rig_type =  RIG_TYPE_SCANNER,
.ptt_type =  RIG_PTT_NONE,
.dcd_type =  RIG_DCD_RIG,
.port_type =  RIG_PORT_SERIAL,
.serial_rate_min =  4800,
.serial_rate_max =  19200,
.serial_data_bits =  8,
.serial_stop_bits =  2,
.serial_parity =  RIG_PARITY_NONE,
.serial_handshake =  RIG_HANDSHAKE_XONXOFF,
.write_delay =  0,
.post_write_delay =  0,
.timeout =  200,
.retry =  3,
.has_get_func =  RIG_FUNC_NONE,
.has_set_func =  AR5000_FUNC_ALL,
.has_get_level =  AR5000_LEVEL,
.has_set_level =  RIG_LEVEL_SET(AR5000_LEVEL),
.has_get_parm =  RIG_PARM_NONE,
.has_set_parm =  RIG_PARM_NONE,    /* FIXME: parms */
.level_gran =  {},                 /* FIXME: granularity */
.parm_gran =  {},
.ctcss_list =  NULL,				/* FIXME: CTCSS list */
.dcs_list =  NULL,
.preamp =   { RIG_DBLST_END, },
.attenuator =   { 10, 20, RIG_DBLST_END, },
.max_rit =  Hz(0),
.max_xit =  Hz(0),
.max_ifshift =  Hz(0),
.targetable_vfo =  0,
.transceive =  RIG_TRN_RIG,
.bank_qty =   20,
.chan_desc_sz =  12,
.vfo_ops =  AR5000_VFO_OPS,
.str_cal = AR5000_REAL_STR_CAL,

.chan_list =  { RIG_CHAN_END, },	/* FIXME: memory channel list: 1000 memories */

.rx_range_list1 =  {
	{kHz(10),MHz(2600),AR5000_MODES,-1,-1,AR5000_VFO},
	RIG_FRNG_END,
	},
.tx_range_list1 =  { RIG_FRNG_END, },

.rx_range_list2 =  {
	{kHz(10),MHz(2600),AR5000_MODES,-1,-1,AR5000_VFO},
	RIG_FRNG_END,
  }, /* rx range */
.tx_range_list2 =  { RIG_FRNG_END, },	/* no tx range, this is a scanner! */

.tuning_steps =  {
	 {AR5000_MODES,50},
	 {AR5000_MODES,100},
	 {AR5000_MODES,200},
	 {AR5000_MODES,500},
	 {AR5000_MODES,kHz(1)},
	 {AR5000_MODES,kHz(2)},
	 {AR5000_MODES,kHz(5)},
	 {AR5000_MODES,kHz(6.25)},
	 {AR5000_MODES,kHz(9)},
	 {AR5000_MODES,kHz(10)},
	 {AR5000_MODES,12500},
	 {AR5000_MODES,kHz(20)},
	 {AR5000_MODES,kHz(25)},
	 {AR5000_MODES,kHz(30)},
	 {AR5000_MODES,kHz(50)},
	 {AR5000_MODES,kHz(100)},
	 {AR5000_MODES,kHz(200)},
	 {AR5000_MODES,kHz(250)},
	 {AR5000_MODES,kHz(500)},
#if 0 
	 {AR5000_MODES,0},	/* any tuning step */
#endif
	 RIG_TS_END,
	},
        /* mode/filter list, .remember =  order matters! */
.filters =  {
        /* mode/filter list, .remember =  order matters! */
		{RIG_MODE_SSB|RIG_MODE_CW, kHz(3)}, 
		{RIG_MODE_FM|RIG_MODE_AM, kHz(15)}, 
		{RIG_MODE_FM|RIG_MODE_AM, kHz(6)}, 	/* narrow */
		{RIG_MODE_FM|RIG_MODE_AM, kHz(30)}, /* wide */
		{RIG_MODE_WFM, kHz(220)},
		{RIG_MODE_WFM, kHz(120)},
		RIG_FLT_END,
	},

.priv =  NULL,
.rig_init =  NULL,
.rig_cleanup =  NULL,
.rig_open =  NULL,
.rig_close =  aor_close,

.set_freq =  aor_set_freq,
.get_freq =  aor_get_freq,
.set_mode =  ar5k_set_mode,
.get_mode =  ar5k_get_mode,
.set_vfo =  aor_set_vfo,
.get_vfo =  aor_get_vfo,

.set_level =  aor_set_level,
.get_level =  aor_get_level,
.get_dcd = aor_get_dcd,

.set_ts =  aor_set_ts,
.set_powerstat =  aor_set_powerstat,
.vfo_op =  aor_vfo_op,
.get_info =  aor_get_info,

};

/*
 * Function definitions below
 */


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
#define MD_FM	'0'
#define	MD_AM	'1'
#define MD_LSB	'2'
#define MD_USB	'3'
#define MD_CW	'4'


extern int aor_transaction(RIG *rig, const char *cmd, int cmd_len, char *data, int *data_len);

/*
 * aor_set_mode
 * Assumes rig!=NULL
 */
int ar5k_set_mode(RIG *rig, vfo_t vfo, rmode_t mode, pbwidth_t width)
{
	unsigned char mdbuf[BUFSZ],ackbuf[BUFSZ];
	int mdbuf_len, ack_len, aormode, retval;

	switch (mode) {
	case RIG_MODE_FM:       aormode = MD_FM; break;
	case RIG_MODE_AM:       aormode = MD_AM; break;
	case RIG_MODE_CW:       aormode = MD_CW; break;
	case RIG_MODE_USB:      aormode = MD_USB; break;
	case RIG_MODE_LSB:      aormode = MD_LSB; break;
	default:
		rig_debug(RIG_DEBUG_ERR,"%s: unsupported mode %d\n",
					__FUNCTION__,mode);
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
int ar5k_get_mode(RIG *rig, vfo_t vfo, rmode_t *mode, pbwidth_t *width)
{
	unsigned char ackbuf[BUFSZ];
	int ack_len, retval;
	char *rfp;


	retval = aor_transaction (rig, "MD" EOM, strlen("MD" EOM), ackbuf, &ack_len);
	if (retval != RIG_OK)
		return retval;

	rfp = strstr(ackbuf, "MD");
	if (!rfp) {
		rig_debug(RIG_DEBUG_WARN, "No MD in returned string in %s: '%s'\n",
				__FUNCTION__, ackbuf);
		return -RIG_EPROTO;
	}

	*width = RIG_PASSBAND_NORMAL;
	switch (rfp[2]) {
		case MD_AM:	*mode = RIG_MODE_AM; break;
		case MD_CW:	*mode = RIG_MODE_CW; break;
		case MD_FM:	*mode = RIG_MODE_FM; break;
		case MD_USB:	*mode = RIG_MODE_USB; break;
		case MD_LSB:	*mode = RIG_MODE_LSB; break;
		default:
			rig_debug(RIG_DEBUG_ERR,"%s: unsupported mode %d\n",
							__FUNCTION__, rfp[2]);
			return -RIG_EINVAL;
	}
	if (*width == RIG_PASSBAND_NORMAL)
		*width = rig_passband_normal(rig, *mode);

	return RIG_OK;
}


