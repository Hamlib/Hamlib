/*
 *  Hamlib Kenwood backend - TS870S description
 *  Copyright (c) 2000-2004 by Stephane Fillod
 *
 *	$Id: ts870s.c,v 1.42 2005-02-03 20:22:21 pa4tu Exp $
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
#include <stdio.h>

#include <hamlib/rig.h>
#include "kenwood.h"

#define TS870S_ALL_MODES	\
(RIG_MODE_AM|RIG_MODE_CW|RIG_MODE_CWR|RIG_MODE_SSB		\
|RIG_MODE_FM|RIG_MODE_RTTY|RIG_MODE_RTTYR)
#define TS870S_OTHER_TX_MODES 	\
(RIG_MODE_CW|RIG_MODE_SSB|RIG_MODE_FM|RIG_MODE_RTTY)
#define TS870S_AM_TX_MODES RIG_MODE_AM

#define TS870S_FUNC_ALL	\
(RIG_FUNC_NB|RIG_FUNC_COMP|RIG_FUNC_VOX|RIG_FUNC_NR		\
|RIG_FUNC_BC|RIG_FUNC_ANF|RIG_FUNC_LOCK)

#define TS870S_LEVEL_ALL	\
(RIG_LEVEL_ATT|RIG_LEVEL_SQL|RIG_LEVEL_STRENGTH|RIG_LEVEL_SWR		\
|RIG_LEVEL_COMP|RIG_LEVEL_ALC|RIG_LEVEL_AGC|RIG_LEVEL_RFPOWER		\
|RIG_LEVEL_AF|RIG_LEVEL_RF|RIG_LEVEL_MICGAIN|RIG_LEVEL_PREAMP)

#define TS870S_VFO (RIG_VFO_A|RIG_VFO_B)

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

static const struct kenwood_priv_caps  ts870s_priv_caps  = {
		.cmdtrm =  EOM_KEN,
};

/* only the ts870s and ts2000 support get_vfo with the 'FR;' command 
   NOTE: using byte 31 in 'IF' will also work. TODO: check other rigs */
int ts870s_get_vfo(RIG *rig, vfo_t *vfo)
{
		unsigned char vfobuf[50];
		int vfo_len, retval;


		/* query RX VFO */
		vfo_len = 50;
		retval = kenwood_transaction (rig, "FR;", 3, vfobuf, &vfo_len);
		if (retval != RIG_OK)
			return retval;

		if (vfo_len != 4 || vfobuf[1] != 'R') {
			rig_debug(RIG_DEBUG_ERR,"%s: unexpected answer %s, "
						"len=%d\n",__FUNCTION__,vfobuf, vfo_len);
			return -RIG_ERJCTED;
		}

		/* TODO: replace 0,1,2,.. constants by defines */
		switch (vfobuf[2]) {
		case '0': *vfo = RIG_VFO_A; break;
		case '1': *vfo = RIG_VFO_B; break;
		case '2': *vfo = RIG_VFO_MEM; break;
		default: 
			rig_debug(RIG_DEBUG_ERR,"%s: unsupported VFO %c\n",
							__FUNCTION__,vfobuf[2]);
			return -RIG_EPROTO;
		}
		return RIG_OK;
}

int ts870s_get_mode(RIG *rig, vfo_t vfo, rmode_t *mode, pbwidth_t *width)
{
  unsigned char buf[50];
  int buf_len, retval;


  buf_len = 50;
  retval = kenwood_transaction (rig, "MD;", 3, buf, &buf_len);
  if (retval != RIG_OK)
    return retval;

  if (buf_len != 4 || buf[1] != 'D')
  {
    rig_debug(RIG_DEBUG_ERR,"%s: unexpected MD answer, len=%d\n",
      __FUNCTION__,buf_len);
    return -RIG_ERJCTED;
  }

  switch (buf[2]) 
  {
    case MD_CW:         *mode = RIG_MODE_CW; break;
    case MD_CWR:	*mode = RIG_MODE_CWR; break;
    case MD_USB:	*mode = RIG_MODE_USB; break;
    case MD_LSB:	*mode = RIG_MODE_LSB; break;
    case MD_FM:	        *mode = RIG_MODE_FM; break;
    case MD_AM:	        *mode = RIG_MODE_AM; break;
    case MD_FSK:	*mode = RIG_MODE_RTTY; break;
    case MD_FSKR:	*mode = RIG_MODE_RTTYR; break;
    case MD_NONE:	*mode = RIG_MODE_NONE; break;
    default:
      rig_debug(RIG_DEBUG_ERR,"%s: unsupported mode '%c'\n",
		      __FUNCTION__,buf[2]);
      return -RIG_EINVAL;
  }

  buf_len = 50;
  retval = kenwood_transaction (rig, "FW;", 3, buf, &buf_len);
  if (retval != RIG_OK)
    return retval;

  if (buf_len != 7 || buf[1] != 'W')
  {
    rig_debug(RIG_DEBUG_ERR,"%s: unexpected FW answer, len=%d\n",
			__FUNCTION__,buf_len);
    return -RIG_ERJCTED;
  }
  
  *width = 10 * atoi(&buf[2]);

  return RIG_OK;
}

int ts870s_set_mode(RIG *rig, vfo_t vfo, rmode_t mode, pbwidth_t width)
{
  unsigned char buf[16],ackbuf[16];
  int buf_len, ack_len, kmode, retval;

  switch (mode) 
  {
    case RIG_MODE_CW:       kmode = MD_CW; break;
    case RIG_MODE_CWR:      kmode = MD_CWR; break;
    case RIG_MODE_USB:      kmode = MD_USB; break;
    case RIG_MODE_LSB:      kmode = MD_LSB; break;
    case RIG_MODE_FM:       kmode = MD_FM; break;
    case RIG_MODE_AM:       kmode = MD_AM; break;
    case RIG_MODE_RTTY:     kmode = MD_FSK; break;
    case RIG_MODE_RTTYR:    kmode = MD_FSKR; break;
    default:
      rig_debug(RIG_DEBUG_ERR,"%s: unsupported mode %d\n",
		      __FUNCTION__,mode);
      return -RIG_EINVAL;
  }

  buf_len = sprintf(buf, "MD%c;", kmode);
  ack_len = 0;
  retval = kenwood_transaction (rig, buf, buf_len, ackbuf, &ack_len);
  if (retval != RIG_OK) return retval;

/* 
 * This rig will simply use an IF bandpass which is closest to width, 
 * so we don't need to check the value...
 */
  buf_len = sprintf(buf, "FW%04d;", (int)width/10);
  ack_len = 0;
  retval = kenwood_transaction (rig, buf, buf_len, ackbuf, &ack_len);
  if (retval != RIG_OK) return retval;

  return RIG_OK;
}

int ts870s_get_level(RIG *rig, vfo_t vfo, setting_t level, value_t *val)
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
				rig_debug(RIG_DEBUG_ERR,"ts870s_get_level: "
								"wrong answer len=%d\n", lvl_len);
				return -RIG_ERJCTED;
			}

			/* Frontend expects:  -54 = S0, 0 = S9  */
			sscanf(lvlbuf+2, "%d", &val->i);
			val->i = (val->i * 4) - 54;
			break;

		case RIG_LEVEL_SWR:
			lvl_len = 50;
			retval = kenwood_transaction (rig, "RM;", 3, lvlbuf, &lvl_len);
			if (retval != RIG_OK)
				return retval;
			/* set meter to SWR if needed */
			if (lvlbuf[2] != '1')
			{
				lvl_len = 0;
				retval = kenwood_transaction (rig, "RM1;", 4, lvlbuf, &lvl_len);
				if (retval != RIG_OK)
					return retval;
				lvl_len = 50;
				retval = kenwood_transaction (rig, "RM;", 3, lvlbuf, &lvl_len);
				if (retval != RIG_OK)
					return retval;
			}

			lvlbuf[7]='\0';
			i=atoi(&lvlbuf[3]);
			if(i == 30) 
				val->f = 150.0; /* infinity :-) */
			else
				val->f = 60.0/(30.0-(float)i)-1.0;
			break;

		case RIG_LEVEL_COMP:
			lvl_len = 50;
			retval = kenwood_transaction (rig, "RM;", 3, lvlbuf, &lvl_len);
			if (retval != RIG_OK)
				return retval;
			/* set meter to COMP if needed */
			if (lvlbuf[2] != '2')
			{
				lvl_len = 0;
				retval = kenwood_transaction (rig, "RM2;", 4, lvlbuf, &lvl_len);
				if (retval != RIG_OK)
					return retval;
				lvl_len = 50;
				retval = kenwood_transaction (rig, "RM;", 4, lvlbuf, &lvl_len);
				if (retval != RIG_OK)
					return retval;
			}

			lvlbuf[7]='\0';
			val->f=(float)atoi(&lvlbuf[3])/30.0;
			break;

		case RIG_LEVEL_ALC:
			lvl_len = 50;
			retval = kenwood_transaction (rig, "RM;", 3, lvlbuf, &lvl_len);
			if (retval != RIG_OK)
				return retval;
			/* set meter to ALC if needed */
			if (lvlbuf[2] != '3')
			{
				lvl_len = 0;
				retval = kenwood_transaction (rig, "RM3;", 4, lvlbuf, &lvl_len);
				if (retval != RIG_OK)
					return retval;
				lvl_len = 50;
				retval = kenwood_transaction (rig, "RM;", 4, lvlbuf, &lvl_len);
				if (retval != RIG_OK)
					return retval;
			}

			lvlbuf[7]='\0';
			val->f=(float)atoi(&lvlbuf[3])/30.0;
			break;

		case RIG_LEVEL_ATT:
			retval = kenwood_transaction (rig, "RA;", 3, lvlbuf, &lvl_len);
			if (retval != RIG_OK)
				return retval;

			if (lvl_len != 5) {
				rig_debug(RIG_DEBUG_ERR,"ts870s_get_level: "
								"unexpected answer len=%d\n", lvl_len);
				return -RIG_ERJCTED;
			}

			sscanf(lvlbuf+2, "%d", &lvl);
			if (lvl == 0) {
					val->i = 0;
			} else {
					for (i=0; i<lvl && i<MAXDBLSTSIZ; i++)
							if (rig->state.attenuator[i] == 0) {
								rig_debug(RIG_DEBUG_ERR,"ts870s_get_level: "
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

		case RIG_LEVEL_IF:
		case RIG_LEVEL_APF:
		case RIG_LEVEL_NR:
		case RIG_LEVEL_PBT_IN:
		case RIG_LEVEL_PBT_OUT:
		case RIG_LEVEL_CWPITCH:
		case RIG_LEVEL_KEYSPD:
		case RIG_LEVEL_NOTCHF:
		case RIG_LEVEL_BKINDL:
		case RIG_LEVEL_BALANCE:
			return -RIG_ENIMPL;

		case RIG_LEVEL_PREAMP:
			return -RIG_ENAVAIL;

		default:
			rig_debug(RIG_DEBUG_ERR,"Unsupported get_level %d", level);
			return -RIG_EINVAL;
		}

		return RIG_OK;
}

/*
 * ts870s rig capabilities.
 * Notice that some rigs share the same functions.
 * Also this struct is READONLY!
 * RIT: Variable Range ±9.99 kHz
 *
 * part of infos comes from .http = //www.kenwood.net/
 */
const struct rig_caps ts870s_caps = {
.rig_model =  RIG_MODEL_TS870S,
.model_name = "TS-870S",
.mfg_name =  "Kenwood",
.version =  "0.4.1",
.copyright =  "LGPL",
.status =  RIG_STATUS_BETA,
.rig_type =  RIG_TYPE_TRANSCEIVER,
.ptt_type =  RIG_PTT_RIG,
.dcd_type =  RIG_DCD_RIG,
.port_type =  RIG_PORT_SERIAL,
.serial_rate_min =  1200,
.serial_rate_max =  57600,
.serial_data_bits =  8,
.serial_stop_bits =  1,
.serial_parity =  RIG_PARITY_NONE,
.serial_handshake =  RIG_HANDSHAKE_NONE,
.write_delay =  0,
.post_write_delay =  0,
.timeout =  200,
.retry =  3,

.has_get_func =  TS870S_FUNC_ALL,
.has_set_func =  TS870S_FUNC_ALL,
.has_get_level =  TS870S_LEVEL_ALL,
.has_set_level =  RIG_LEVEL_SET(TS870S_LEVEL_ALL),
.has_get_parm =  RIG_PARM_NONE,
.has_set_parm =  RIG_PARM_NONE,    /* FIXME: parms */
.level_gran =  {},                 /* FIXME: granularity */
.parm_gran =  {},
.ctcss_list =  kenwood38_ctcss_list,
.dcs_list =  NULL,
.preamp =   { RIG_DBLST_END, },	/* FIXME: preamp list */
.attenuator =   { 6, 12, 18, RIG_DBLST_END, },
.max_rit =  kHz(9.99),
.max_xit =  kHz(9.99),
.max_ifshift =  Hz(0),
.targetable_vfo =  RIG_TARGETABLE_FREQ,
.transceive =  RIG_TRN_RIG,
.bank_qty =   0,
.chan_desc_sz =  0,


.chan_list =  {
			{  0, 89, RIG_MTYPE_MEM  },	/* TBC */
			{ 90, 99, RIG_MTYPE_EDGE },
			RIG_CHAN_END,
		},

.rx_range_list1 =  { 
	{kHz(100),MHz(30),TS870S_ALL_MODES,-1,-1,TS870S_VFO},
	RIG_FRNG_END,
  }, /* rx range */
.tx_range_list1 =  { 
    {kHz(1810),kHz(1850),TS870S_OTHER_TX_MODES,5000,100000,TS870S_VFO},	/* 100W class */
    {kHz(1810),kHz(1850),TS870S_AM_TX_MODES,2000,25000,TS870S_VFO},		/* 25W class */
    {kHz(3500),kHz(3800),TS870S_OTHER_TX_MODES,5000,100000,TS870S_VFO},
    {kHz(3500),kHz(3800),TS870S_AM_TX_MODES,2000,25000,TS870S_VFO},
    {MHz(7),kHz(7100),TS870S_OTHER_TX_MODES,5000,100000,TS870S_VFO},
    {MHz(7),kHz(7100),TS870S_AM_TX_MODES,2000,25000,TS870S_VFO},
    {kHz(10100),kHz(10150),TS870S_OTHER_TX_MODES,5000,100000,TS870S_VFO},
    {kHz(10100),kHz(10150),TS870S_AM_TX_MODES,2000,25000,TS870S_VFO},
    {MHz(14),kHz(14350),TS870S_OTHER_TX_MODES,5000,100000,TS870S_VFO},
    {MHz(14),kHz(14350),TS870S_AM_TX_MODES,2000,25000,TS870S_VFO},
    {kHz(18068),kHz(18168),TS870S_OTHER_TX_MODES,5000,100000,TS870S_VFO},
    {kHz(18068),kHz(18168),TS870S_AM_TX_MODES,2000,25000,TS870S_VFO},
    {MHz(21),kHz(21450),TS870S_OTHER_TX_MODES,5000,100000,TS870S_VFO},
    {MHz(21),kHz(21450),TS870S_AM_TX_MODES,2000,25000,TS870S_VFO},
    {kHz(24890),kHz(24990),TS870S_OTHER_TX_MODES,5000,100000,TS870S_VFO},
    {kHz(24890),kHz(24990),TS870S_AM_TX_MODES,2000,25000,TS870S_VFO},
    {MHz(28),kHz(29700),TS870S_OTHER_TX_MODES,5000,100000,TS870S_VFO},
    {MHz(28),kHz(29700),TS870S_AM_TX_MODES,2000,25000,TS870S_VFO},
	RIG_FRNG_END,
  },

.rx_range_list2 =  {
	{kHz(100),MHz(30),TS870S_ALL_MODES,-1,-1,TS870S_VFO},
	RIG_FRNG_END,
  }, /* rx range */
.tx_range_list2 =  {
    {kHz(1800),MHz(2)-1,TS870S_OTHER_TX_MODES,5000,100000,TS870S_VFO},	/* 100W class */
    {kHz(1800),MHz(2)-1,TS870S_AM_TX_MODES,2000,25000,TS870S_VFO},		/* 25W class */
    {kHz(3500),MHz(4)-1,TS870S_OTHER_TX_MODES,5000,100000,TS870S_VFO},
    {kHz(3500),MHz(4)-1,TS870S_AM_TX_MODES,2000,25000,TS870S_VFO},
    {MHz(7),kHz(7300),TS870S_OTHER_TX_MODES,5000,100000,TS870S_VFO},
    {MHz(7),kHz(7300),TS870S_AM_TX_MODES,2000,25000,TS870S_VFO},
    {kHz(10100),kHz(10150),TS870S_OTHER_TX_MODES,5000,100000,TS870S_VFO},
    {kHz(10100),kHz(10150),TS870S_AM_TX_MODES,2000,25000,TS870S_VFO},
    {MHz(14),kHz(14350),TS870S_OTHER_TX_MODES,5000,100000,TS870S_VFO},
    {MHz(14),kHz(14350),TS870S_AM_TX_MODES,2000,25000,TS870S_VFO},
    {kHz(18068),kHz(18168),TS870S_OTHER_TX_MODES,5000,100000,TS870S_VFO},
    {kHz(18068),kHz(18168),TS870S_AM_TX_MODES,2000,25000,TS870S_VFO},
    {MHz(21),kHz(21450),TS870S_OTHER_TX_MODES,5000,100000,TS870S_VFO},
    {MHz(21),kHz(21450),TS870S_AM_TX_MODES,2000,25000,TS870S_VFO},
    {kHz(24890),kHz(24990),TS870S_OTHER_TX_MODES,5000,100000,TS870S_VFO},
    {kHz(24890),kHz(24990),TS870S_AM_TX_MODES,2000,25000,TS870S_VFO},
    {MHz(28),kHz(29700),TS870S_OTHER_TX_MODES,5000,100000,TS870S_VFO},
    {MHz(28),kHz(29700),TS870S_AM_TX_MODES,2000,25000,TS870S_VFO},
	RIG_FRNG_END,
  }, /* tx range */
.tuning_steps =  {
	 {TS870S_ALL_MODES,50},
	 {TS870S_ALL_MODES,100},
	 {TS870S_ALL_MODES,kHz(1)},
	 {TS870S_ALL_MODES,kHz(5)},
	 {TS870S_ALL_MODES,kHz(9)},
	 {TS870S_ALL_MODES,kHz(10)},
	 {TS870S_ALL_MODES,12500},
	 {TS870S_ALL_MODES,kHz(20)},
	 {TS870S_ALL_MODES,kHz(25)},
	 {TS870S_ALL_MODES,kHz(100)},
	 {TS870S_ALL_MODES,MHz(1)},
	 {TS870S_ALL_MODES,0},	/* any tuning step */
	 RIG_TS_END,
	},
        /* mode/filter list, remember: order matters! */
.filters =  {
		{RIG_MODE_SSB, Hz(200)},
		{RIG_MODE_SSB, Hz(0)},
		{RIG_MODE_SSB, Hz(600)},
		{RIG_MODE_CW, Hz(400)},
		{RIG_MODE_CW, Hz(100)},
		{RIG_MODE_CW, Hz(1000)},
		{RIG_MODE_RTTY, Hz(1000)},
		{RIG_MODE_RTTY, Hz(500)},
		{RIG_MODE_RTTY, Hz(1500)},
		{RIG_MODE_AM, Hz(200)},
		{RIG_MODE_AM, Hz(0)},
		{RIG_MODE_AM, Hz(500)},
		{RIG_MODE_FM, kHz(8)},
		{RIG_MODE_FM, kHz(5)},
		{RIG_MODE_FM, kHz(14)},
		RIG_FLT_END,
	},
.priv =  (void *)&ts870s_priv_caps,

.set_freq =  kenwood_set_freq,
.get_freq =  kenwood_get_freq,
.set_rit =  kenwood_set_rit,
.get_rit =  kenwood_get_rit,
.set_xit =  kenwood_set_xit,
.get_xit =  kenwood_get_xit,
.set_mode =  ts870s_set_mode,
.get_mode =  ts870s_get_mode,
.set_vfo =  kenwood_set_vfo,
.get_vfo =  ts870s_get_vfo,
.set_ctcss_tone =  kenwood_set_ctcss_tone,
.get_ctcss_tone =  kenwood_get_ctcss_tone,
.get_ptt =  kenwood_get_ptt,
.set_ptt =  kenwood_set_ptt,
.get_dcd =  kenwood_get_dcd,
.set_func =  kenwood_set_func,
.get_func =  kenwood_get_func,
.set_level =  kenwood_set_level,
.get_level =  ts870s_get_level,
.send_morse =  kenwood_send_morse,
.vfo_op =  kenwood_vfo_op,
.set_mem =  kenwood_set_mem,
.get_mem =  kenwood_get_mem,
.set_trn =  kenwood_set_trn,
.get_trn =  kenwood_get_trn,
.set_powerstat =  kenwood_set_powerstat,
.get_powerstat =  kenwood_get_powerstat,
.reset =  kenwood_reset,

};

/*
 * Function definitions below
 */

