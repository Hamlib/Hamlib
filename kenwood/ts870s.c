/*
 *  Hamlib Kenwood backend - TS870S description
 *  Copyright (c) 2000-2002 by Stephane Fillod
 *
 *	$Id: ts870s.c,v 1.31 2002-12-20 15:43:31 pa4tu Exp $
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

#include <hamlib/rig.h>
#include "kenwood.h"

#define TS870S_ALL_MODES (RIG_MODE_AM|RIG_MODE_CW|RIG_MODE_SSB|RIG_MODE_FM|RIG_MODE_RTTY)
#define TS870S_OTHER_TX_MODES (RIG_MODE_CW|RIG_MODE_SSB|RIG_MODE_FM|RIG_MODE_RTTY)
#define TS870S_AM_TX_MODES RIG_MODE_AM

#define TS870S_FUNC_ALL (RIG_FUNC_NB|RIG_FUNC_COMP|RIG_FUNC_VOX|RIG_FUNC_NR|RIG_FUNC_BC|RIG_FUNC_ANF|RIG_FUNC_LOCK)

#define TS870S_LEVEL_ALL (RIG_LEVEL_ATT|RIG_LEVEL_SQL|RIG_LEVEL_STRENGTH|RIG_LEVEL_RFPOWER|RIG_LEVEL_AF|RIG_LEVEL_RF|RIG_LEVEL_MICGAIN|RIG_LEVEL_AGC)

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
    rig_debug(RIG_DEBUG_ERR,"ts870s_get_mode: unexpected MD answer, len=%d\n",
      buf_len);
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
      rig_debug(RIG_DEBUG_ERR,"ts870s_get_mode: "
        "unsupported mode '%c'\n", buf[2]);
      return -RIG_EINVAL;
  }

  buf_len = 50;
  retval = kenwood_transaction (rig, "FW;", 3, buf, &buf_len);
  if (retval != RIG_OK)
    return retval;

  if (buf_len != 7 || buf[1] != 'W')
  {
    rig_debug(RIG_DEBUG_ERR,"ts870s_get_mode: unexpected FW answer, len=%d\n",
      buf_len);
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
      rig_debug(RIG_DEBUG_ERR,"ts870s_set_mode: "
	"unsupported mode %d\n", mode);
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
.version =  "0.3.1",
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
		{RIG_MODE_SSB, kHz(2.4)},
		{RIG_MODE_CW, Hz(200)},
		{RIG_MODE_RTTY, Hz(500)},
		{RIG_MODE_AM, kHz(9)},
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
.get_vfo =  kenwood_get_vfo,
.set_ctcss_tone =  kenwood_set_ctcss_tone,
.get_ctcss_tone =  kenwood_get_ctcss_tone,
.get_ptt =  kenwood_get_ptt,
.set_ptt =  kenwood_set_ptt,
.get_dcd =  kenwood_get_dcd,
.set_func =  kenwood_set_func,
.get_func =  kenwood_get_func,
.set_level =  kenwood_set_level,
.get_level =  kenwood_get_level,
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

