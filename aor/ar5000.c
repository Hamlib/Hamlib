/*
 *  Hamlib AOR backend - AR5000 description
 *  Copyright (c) 2000-2004 by Stephane Fillod
 *
 *	$Id: ar5000.c,v 1.8 2005-04-15 21:50:27 fillods Exp $
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

#define AR5000_VFO_OPS (RIG_OP_MCL|RIG_OP_UP|RIG_OP_DOWN|RIG_OP_LEFT|RIG_OP_RIGHT)

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

#define AR5000_MEM_CAP {	\
	.freq = 1,	\
	.mode = 1,	\
	.width = 1,	\
	.bank_num = 1,	\
	.tuning_step = 1,	\
	.channel_desc = 1,	\
	.flags = 1,	\
	.levels = RIG_LEVEL_ATT|RIG_LEVEL_AGC,	\
	.funcs = RIG_FUNC_ABM,    \
}


static int format5k_mode(RIG *rig, char *buf, rmode_t mode, pbwidth_t width);
static int parse5k_aor_mode(RIG *rig, char aormode, char aorwidth, rmode_t *mode, pbwidth_t *width);

static const struct aor_priv_caps ar5k_priv_caps = {
	.format_mode = format5k_mode,
	.parse_aor_mode = parse5k_aor_mode,
	.bank_base1 = '0',
	.bank_base2 = '0',
};

/*
 * ar5000 rig capabilities.
 * Notice that some rigs share the same functions.
 * Also this struct is READONLY!
 *
 * part of info from http://www.aoruk.com/5000.htm
 *
 * TODO: retrieve BW info, and rest of commands
 */
const struct rig_caps ar5000_caps = {
.rig_model =  RIG_MODEL_AR5000,
.model_name = "AR5000",
.mfg_name =  "AOR",
.version =  BACKEND_VER,
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
.bank_qty =   10,
.chan_desc_sz =  12,
.vfo_ops =  AR5000_VFO_OPS,
.str_cal = AR5000_REAL_STR_CAL,

.chan_list =  {
	{   0,  999, RIG_MTYPE_MEM, AR5000_MEM_CAP },
        RIG_CHAN_END, },


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
		{RIG_MODE_CW, Hz(500)}, 
		{RIG_MODE_FM|RIG_MODE_AM, kHz(15)}, 
		{RIG_MODE_FM|RIG_MODE_AM, kHz(6)}, 	/* narrow */
		{RIG_MODE_FM|RIG_MODE_AM, kHz(40)}, /* wide */
		{RIG_MODE_WFM, kHz(220)},
		{RIG_MODE_WFM, kHz(120)},
		RIG_FLT_END,
	},

.priv = (void*)&ar5k_priv_caps,

.rig_init =  NULL,
.rig_cleanup =  NULL,
.rig_open =  NULL,
.rig_close =  aor_close,

.set_freq =  aor_set_freq,
.get_freq =  aor_get_freq,
.set_mode =  aor_set_mode,
.get_mode =  aor_get_mode,
.set_vfo =  aor_set_vfo,
.get_vfo =  aor_get_vfo,

.set_level =  aor_set_level,
.get_level =  aor_get_level,
.get_dcd = aor_get_dcd,

.set_ts =  aor_set_ts,
.set_powerstat =  aor_set_powerstat,
.vfo_op =  aor_vfo_op,
.get_info =  aor_get_info,

.set_mem = aor_set_mem,
.get_mem = aor_get_mem,
.set_bank = aor_set_bank,

.set_channel = aor_set_channel,
.get_channel = aor_get_channel,

};

/*
 * Function definitions below
 */


/*
 * modes in use by the "MD" command of AR5000
 */
#define AR5K_FM		'0'
#define	AR5K_AM		'1'
#define AR5K_LSB	'2'
#define AR5K_USB	'3'
#define AR5K_CW		'4'

int format5k_mode(RIG *rig, char *buf, rmode_t mode, pbwidth_t width)
{
	int aormode, aorwidth;

	switch (mode) {
	case RIG_MODE_AM:       aormode = AR5K_AM; break;
	case RIG_MODE_WFM:
	case RIG_MODE_FM:       aormode = AR5K_FM; break;
	case RIG_MODE_LSB:      aormode = AR5K_LSB; break;
	case RIG_MODE_USB:      aormode = AR5K_USB; break;
	case RIG_MODE_CW:       aormode = AR5K_CW; break;
	default:
		rig_debug(RIG_DEBUG_ERR,"%s: unsupported mode %d\n",
				__FUNCTION__, mode);
		return -RIG_EINVAL;
	}

	if (width == RIG_PASSBAND_NORMAL)
		width = rig_passband_normal(rig, mode);

	switch (width) {
	case 500:	aorwidth = '0'; break;
	case s_kHz(3):	aorwidth = '1'; break;
	case s_kHz(6):	aorwidth = '2'; break;
	case s_kHz(15):	aorwidth = '3'; break;
	case s_kHz(40):	aorwidth = '4'; break;
	case s_kHz(110):	aorwidth = '5'; break;
	case s_kHz(220):	aorwidth = '6'; break;
	default:
		rig_debug(RIG_DEBUG_ERR,"%s: unsupported width %d\n",
				__FUNCTION__, width);
		return -RIG_EINVAL;
	}

	return sprintf(buf, "MD%c BW%c", aormode, aorwidth);
}



int parse5k_aor_mode(RIG *rig, char aormode, char aorwidth, rmode_t *mode, pbwidth_t *width)
{
	switch (aormode) {
		case AR5K_FM:	*mode = RIG_MODE_FM; break;
		case AR5K_AM:	*mode = RIG_MODE_AM; break;
		case AR5K_LSB:	*mode = RIG_MODE_LSB; break;
		case AR5K_USB:	*mode = RIG_MODE_USB; break;
		case AR5K_CW:	*mode = RIG_MODE_CW; break;
		default:
			rig_debug(RIG_DEBUG_ERR,"%s: unsupported mode '%c'\n",
					__FUNCTION__, aormode);
			return -RIG_EPROTO;
	}

	switch (aorwidth) {
		case '0':	*width = 500; break;
		case '1':	*width = s_kHz(3); break;
		case '2':	*width = s_kHz(6); break;
		case '3':	*width = s_kHz(15); break;
		case '4':	*width = s_kHz(40); break;
		case '5':	*width = s_kHz(110); break;
		case '6':	*width = s_kHz(220); break;
	default:
		rig_debug(RIG_DEBUG_ERR,"%s: unsupported width %d\n",
				__FUNCTION__, aorwidth);
		return -RIG_EPROTO;
	}

	return RIG_OK;
}

