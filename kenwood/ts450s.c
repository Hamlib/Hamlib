/*
 *  Hamlib Kenwood backend - TS450S description
 *  Copyright (c) 2000-2004 by Stephane Fillod
 *
 *	$Id: ts450s.c,v 1.24 2006-03-14 20:06:46 pa4tu Exp $
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

#include <hamlib/rig.h>
#include "kenwood.h"

#include "bandplan.h"

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

#define TS450S_ALL_MODES (RIG_MODE_AM|RIG_MODE_CW|RIG_MODE_CWR|RIG_MODE_SSB|RIG_MODE_FM|RIG_MODE_RTTY|RIG_MODE_RTTYR)
#define TS450S_OTHER_TX_MODES (RIG_MODE_CW|RIG_MODE_CWR|RIG_MODE_SSB|RIG_MODE_FM|RIG_MODE_RTTY|RIG_MODE_RTTYR)
#define TS450S_AM_TX_MODES RIG_MODE_AM
#define TS450S_FUNC_ALL (RIG_FUNC_LOCK|RIG_FUNC_AIP|RIG_FUNC_TONE)
#define TS450S_LEVEL_ALL (RIG_LEVEL_STRENGTH|RIG_LEVEL_CWPITCH|RIG_LEVEL_AGC|RIG_LEVEL_METER|RIG_LEVEL_SWR|RIG_LEVEL_ALC)
#define TS450S_VFO (RIG_VFO_A|RIG_VFO_B)

#define TS450S_PARMS (RIG_PARM_ANN)	/* optional */

#define TS450S_VFO_OPS (RIG_OP_UP|RIG_OP_DOWN)
#define TS450S_SCAN_OPS (RIG_SCAN_VFO)

#define TS450S_CHANNEL_CAPS { \
	.freq=1,\
	.mode=1,\
	.tx_freq=1,\
	.tx_mode=1,\
	.split=1,\
	.funcs=RIG_FUNC_TONE, \
	.flags=RIG_CHFLAG_SKIP \
	}


static const struct kenwood_priv_caps  ts450_priv_caps  = {
		.cmdtrm =  EOM_KEN,
};

/*
 * Function definitions
 */
static int 
ts450s_get_mode(RIG *rig, vfo_t vfo, rmode_t *mode, pbwidth_t *width)
{
  char infobuf[50];
  size_t info_len;
  int retval;

  info_len = 50;
  retval = kenwood_transaction (rig, "IF;", 3, infobuf, &info_len);
  if (retval != RIG_OK)
  return retval;

  if (info_len != 38 || infobuf[1] != 'F') {
    rig_debug(RIG_DEBUG_ERR,"ts450s_get_mode: wrong answer len=%d\n",
      info_len);
    return -RIG_ERJCTED;
  }

  switch (infobuf[29]) {
    case MD_CW:		*mode = RIG_MODE_CW; break;
    case MD_USB:	*mode = RIG_MODE_USB; break;
    case MD_LSB:	*mode = RIG_MODE_LSB; break;
    case MD_FM:		*mode = RIG_MODE_FM; break;
    case MD_AM:		*mode = RIG_MODE_AM; break;
    case MD_FSK:        *mode = RIG_MODE_RTTY; break;
    case MD_CWR:	*mode = RIG_MODE_CWR; break;
    case MD_FSKR:	*mode = RIG_MODE_RTTY; break;
    case MD_NONE:	*mode = RIG_MODE_NONE; break;
    default:
      rig_debug(RIG_DEBUG_ERR,"ts450s_get_mode: "
	"unsupported mode '%c'\n", infobuf[29]);
    return -RIG_EINVAL;
  }

  /* this rig does not have a command for reading IF bandwidth */
  *width = rig_passband_normal(rig, *mode);

  return RIG_OK;
}

static int 
ts450s_get_vfo(RIG *rig, vfo_t *vfo)
{
  char infobuf[50];
  size_t info_len;
  int retval;

  info_len = 50;
  retval = kenwood_transaction (rig, "IF;", 3, infobuf, &info_len);
  if (retval != RIG_OK)
  return retval;

  if (info_len != 38 || infobuf[1] != 'F') {
    rig_debug(RIG_DEBUG_ERR,"ts450s_get_vfo: wrong answer len=%d\n",
      info_len);
    return -RIG_ERJCTED;
  }

  switch (infobuf[30]) {
    case '0': *vfo = RIG_VFO_A; break;
    case '1': *vfo = RIG_VFO_B; break;
    case '2': *vfo = RIG_VFO_MEM; break;
    default: 
      rig_debug(RIG_DEBUG_ERR,"ts450s_get_vfo: unsupported VFO %c\n", 
        infobuf[30]);
      return -RIG_EPROTO;
  }
  return RIG_OK;
}


static int 
ts450s_get_level(RIG *rig, vfo_t vfo, setting_t level, value_t *val)
{
  char lvlbuf[50];
  size_t lvl_len;
  int retval;

  lvl_len = 50;
  switch (level) 
  {
    case RIG_LEVEL_STRENGTH:
      retval = kenwood_transaction (rig, "SM;", 3, lvlbuf, &lvl_len);
      if (retval != RIG_OK) return retval;
      if (lvl_len != 7 || lvlbuf[1] != 'M') 
        {
          rig_debug(RIG_DEBUG_ERR,"ts450s_get_level: "
            "wrong answer len=%d\n", lvl_len);
          return -RIG_ERJCTED;
	}
      sscanf(lvlbuf+2, "%d", &val->i);
	val->i = (val->i * 4) - 54;
    break;
    case RIG_LEVEL_CWPITCH:
      retval = kenwood_transaction (rig, "PT;", 3, lvlbuf, &lvl_len);
      if (retval != RIG_OK) return retval;
      if (lvl_len != 5 || lvlbuf[1] != 'T') 
        {
          rig_debug(RIG_DEBUG_ERR,"ts450s_get_level: "
            "wrong answer len=%d\n", lvl_len);
          return -RIG_ERJCTED;
	}
      sscanf(lvlbuf+2, "%d", &val->i);
	val->i = (val->i * 1000) + 1000; /*00 - 08*/
    break;
    case RIG_LEVEL_AGC:
    case RIG_LEVEL_MICGAIN:
    case RIG_LEVEL_SQL:
    case RIG_LEVEL_RF:
    case RIG_LEVEL_AF:
    case RIG_LEVEL_RFPOWER:
    case RIG_LEVEL_ATT:
    case RIG_LEVEL_PREAMP:
    case RIG_LEVEL_IF:
    case RIG_LEVEL_APF:
    case RIG_LEVEL_NR:
    case RIG_LEVEL_PBT_IN:
    case RIG_LEVEL_PBT_OUT:
    case RIG_LEVEL_KEYSPD:
    case RIG_LEVEL_NOTCHF:
    case RIG_LEVEL_COMP:
    case RIG_LEVEL_BKINDL:
    case RIG_LEVEL_BALANCE:
	return -RIG_ENAVAIL;
    default:
	rig_debug(RIG_DEBUG_ERR,"Unsupported get_level %d", level);
	return -RIG_EINVAL;
  }

  return RIG_OK;
}

static int 
ts450s_set_level(RIG *rig, vfo_t vfo, setting_t level, value_t val)
{
  char levelbuf[16], ackbuf[50];
  size_t level_len, ack_len;
  int kenwood_val, retval;

  if (RIG_LEVEL_IS_FLOAT(level))
    kenwood_val = val.f * 255;
  else
    kenwood_val = val.i;

  switch (level) 
  {
    case RIG_LEVEL_CWPITCH:
      level_len = sprintf(levelbuf, "PT%02d;", (int)kenwood_val/30);
      break;
    case RIG_LEVEL_STRENGTH:
    case RIG_LEVEL_AGC:
    case RIG_LEVEL_MICGAIN:
    case RIG_LEVEL_SQL:
    case RIG_LEVEL_RF:
    case RIG_LEVEL_AF:
    case RIG_LEVEL_RFPOWER:
    case RIG_LEVEL_ATT:
    case RIG_LEVEL_PREAMP:
    case RIG_LEVEL_IF:
    case RIG_LEVEL_APF:
    case RIG_LEVEL_NR:
    case RIG_LEVEL_PBT_IN:
    case RIG_LEVEL_PBT_OUT:
    case RIG_LEVEL_KEYSPD:
    case RIG_LEVEL_NOTCHF:
    case RIG_LEVEL_COMP:
    case RIG_LEVEL_BKINDL:
    case RIG_LEVEL_BALANCE:
      return -RIG_ENAVAIL;
    default:
      rig_debug(RIG_DEBUG_ERR,"Unsupported set_level %d", level);
      return -RIG_EINVAL;
  }

  ack_len = 0;
  retval = kenwood_transaction (rig, levelbuf, level_len, ackbuf, &ack_len);
  if (retval != RIG_OK)	return retval;

  return RIG_OK;
}

/*
 * ts450s rig capabilities.
 * Notice that some rigs share the same functions.
 * Also this struct is READONLY!
 * RIT: Variable Range ±9.99 kHz
 *
 * TODO: protocol to be checked with manual (identical to TS690)
 * 	- get_channel/set_channel: MR/MW
 * 	- how to set_split in vfo mode?
 * 	- ...
 *
 * specs: http://www.qsl.net/sm7vhs/radio/kenwood/ts450/specs.htm
 * infos comes from http://www.cnham.com/ts450/ts_450_ex_control.pdf
 */
const struct rig_caps ts450s_caps = {
.rig_model =  RIG_MODEL_TS450S,
.model_name = "TS-450S",
.mfg_name =  "Kenwood",
.version =  BACKEND_VER,
.copyright =  "LGPL",
.status =  RIG_STATUS_BETA,
.rig_type =  RIG_TYPE_TRANSCEIVER,
.ptt_type =  RIG_PTT_RIG,
.dcd_type =  RIG_DCD_RIG,
.port_type =  RIG_PORT_SERIAL,
.serial_rate_min =  1200,
.serial_rate_max =  4800,
.serial_data_bits =  8,
.serial_stop_bits =  2,
.serial_parity =  RIG_PARITY_NONE,
.serial_handshake =  RIG_HANDSHAKE_HARDWARE,
.write_delay =  0,
.post_write_delay =  100,
.timeout =  1000,
.retry =  3,

.has_get_func =  TS450S_FUNC_ALL,
.has_set_func =  TS450S_FUNC_ALL,
.has_get_level =  TS450S_LEVEL_ALL|RIG_LEVEL_RFPOWER,
.has_set_level =  RIG_LEVEL_SET(TS450S_LEVEL_ALL),
.has_get_parm =  TS450S_PARMS,
.has_set_parm =  TS450S_PARMS,
.level_gran =  {},                 /* FIXME: granularity */
.parm_gran =  {},
.ctcss_list =  kenwood38_ctcss_list,
.dcs_list =  NULL,
.preamp =   { RIG_DBLST_END, },	/* FIXME: preamp list */
.attenuator =   { 6, 12, 18, RIG_DBLST_END, },
.max_rit =  Hz(9999),
.max_xit =  Hz(9999),
.max_ifshift =  Hz(0),
.targetable_vfo =  RIG_TARGETABLE_FREQ,
.transceive =  RIG_TRN_RIG,
.bank_qty =   0,
.chan_desc_sz =  0,
.vfo_ops = TS450S_VFO_OPS,
.scan_ops =  TS450S_SCAN_OPS,

.chan_list =  {
			{  0, 89, RIG_MTYPE_MEM, TS450S_CHANNEL_CAPS },	/* TBC */
			{ 90, 99, RIG_MTYPE_EDGE, TS450S_CHANNEL_CAPS },
			RIG_CHAN_END,
		},

.rx_range_list1 =  { 
	{kHz(500),MHz(30),TS450S_ALL_MODES,-1,-1,TS450S_VFO},
	RIG_FRNG_END,
  }, /* rx range */

.tx_range_list1 =  { 
	FRQ_RNG_HF(1,TS450S_OTHER_TX_MODES, W(5),W(100),TS450S_VFO,0),
	FRQ_RNG_HF(1,TS450S_AM_TX_MODES, W(2),W(40),TS450S_VFO,0),   /* AM class */
	RIG_FRNG_END,
  },

.rx_range_list2 =  {
	{kHz(500),MHz(30),TS450S_ALL_MODES,-1,-1,TS450S_VFO},
	RIG_FRNG_END,
  }, /* rx range */
.tx_range_list2 =  {
    {kHz(1800),MHz(2)-1,TS450S_OTHER_TX_MODES,5000,100000,TS450S_VFO},	/* 100W class */
    {kHz(1800),MHz(2)-1,TS450S_AM_TX_MODES,2000,40000,TS450S_VFO},		/* 40W class */
    {kHz(3500),MHz(4)-1,TS450S_OTHER_TX_MODES,5000,100000,TS450S_VFO},
    {kHz(3500),MHz(4)-1,TS450S_AM_TX_MODES,2000,40000,TS450S_VFO},
    {MHz(7),kHz(7300),TS450S_OTHER_TX_MODES,5000,100000,TS450S_VFO},
    {MHz(7),kHz(7300),TS450S_AM_TX_MODES,2000,40000,TS450S_VFO},
    {kHz(10100),kHz(10150),TS450S_OTHER_TX_MODES,5000,100000,TS450S_VFO},
    {kHz(10100),kHz(10150),TS450S_AM_TX_MODES,2000,40000,TS450S_VFO},
    {MHz(14),kHz(14350),TS450S_OTHER_TX_MODES,5000,100000,TS450S_VFO},
    {MHz(14),kHz(14350),TS450S_AM_TX_MODES,2000,40000,TS450S_VFO},
    {kHz(18068),kHz(18168),TS450S_OTHER_TX_MODES,5000,100000,TS450S_VFO},
    {kHz(18068),kHz(18168),TS450S_AM_TX_MODES,2000,40000,TS450S_VFO},
    {MHz(21),kHz(21450),TS450S_OTHER_TX_MODES,5000,100000,TS450S_VFO},
    {MHz(21),kHz(21450),TS450S_AM_TX_MODES,2000,40000,TS450S_VFO},
    {kHz(24890),kHz(24990),TS450S_OTHER_TX_MODES,5000,100000,TS450S_VFO},
    {kHz(24890),kHz(24990),TS450S_AM_TX_MODES,2000,40000,TS450S_VFO},
    {MHz(28),kHz(29700),TS450S_OTHER_TX_MODES,5000,100000,TS450S_VFO},
    {MHz(28),kHz(29700),TS450S_AM_TX_MODES,2000,40000,TS450S_VFO},
	RIG_FRNG_END,
  }, /* tx range */
.tuning_steps =  {
	 {TS450S_ALL_MODES,1},
	 {TS450S_ALL_MODES,10},
	 RIG_TS_END,
	},
        /* mode/filter list, remember: order matters! */
.filters =  {
		{RIG_MODE_FM, kHz(12)},
		{RIG_MODE_FM|RIG_MODE_AM, kHz(6)},
		{RIG_MODE_SSB|RIG_MODE_CW|RIG_MODE_RTTY|RIG_MODE_CWR|RIG_MODE_RTTYR|RIG_MODE_AM, kHz(2.4)},
		{RIG_MODE_SSB|RIG_MODE_CW|RIG_MODE_RTTY|RIG_MODE_CWR|RIG_MODE_RTTYR|RIG_MODE_AM, Hz(500)},
		{RIG_MODE_SSB|RIG_MODE_CW|RIG_MODE_RTTY|RIG_MODE_CWR|RIG_MODE_RTTYR|RIG_MODE_AM, kHz(12)},
		{RIG_MODE_SSB|RIG_MODE_CW|RIG_MODE_RTTY|RIG_MODE_CWR|RIG_MODE_RTTYR, kHz(6)},
		RIG_FLT_END,
	},
.priv =  (void *)&ts450_priv_caps,

.set_freq =  kenwood_set_freq,
.get_freq =  kenwood_get_freq,
.set_rit =  kenwood_set_rit,
.get_rit =  kenwood_get_rit,
.set_xit =  kenwood_set_xit,
.get_xit =  kenwood_get_xit,
.set_mode =  kenwood_set_mode,
.get_mode =  ts450s_get_mode,
.set_vfo =  kenwood_set_vfo,
.get_vfo =  ts450s_get_vfo,
.set_ctcss_tone =  kenwood_set_ctcss_tone,
.get_ctcss_tone =  kenwood_get_ctcss_tone,
.get_ptt =  kenwood_get_ptt,
.set_ptt =  kenwood_set_ptt,
.get_dcd =  kenwood_get_dcd,
.set_func =  kenwood_set_func,
.get_func =  kenwood_get_func,
.set_level =  ts450s_set_level,
.get_level =  ts450s_get_level,
.vfo_op =  kenwood_vfo_op,
.set_mem =  kenwood_set_mem,
.get_mem =  kenwood_get_mem,
.set_trn =  kenwood_set_trn,
.get_trn =  kenwood_get_trn,
.set_powerstat =  kenwood_set_powerstat,
.get_powerstat =  kenwood_get_powerstat,
.reset =  kenwood_reset,
.scan =  kenwood_scan,

};

