/*
 *  Hamlib Kenwood backend - TS-790 description
 *  Copyright (c) 2000-2006 by Stephane Fillod
 *
 *	$Id: ts790.c,v 1.16 2007-08-20 17:26:25 y32kn Exp $
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

#include <hamlib/rig.h>
#include "kenwood.h"

/*
 * modes in use by the "MD" command
 */
#define MD_NONE	'0'
#define MD_LSB	'1'
#define MD_USB	'2'
#define MD_CW	'3'
#define MD_FM	'4'
#define MD_CWR	'7'

#define TS790_ALL_MODES (RIG_MODE_CW|RIG_MODE_CWR|RIG_MODE_SSB|RIG_MODE_FM)
#define TS790_HI_MODES (RIG_MODE_CW|RIG_MODE_FM)
#define TS790_LO_MODES (RIG_MODE_SSB)

/* func and levels to be checked */
#define TS790_FUNC_ALL (RIG_FUNC_TSQL|RIG_FUNC_LOCK|RIG_FUNC_MUTE)

#define TS790_LEVEL_ALL (RIG_LEVEL_STRENGTH)

#define TS790_VFO (RIG_VFO_A|RIG_VFO_B)

#define TS790_VFO_OP (RIG_OP_UP|RIG_OP_DOWN)
/*
 * Function definitions below
 */
static int 
ts790_get_mode(RIG *rig, vfo_t vfo, rmode_t *mode, pbwidth_t *width)
{
  char infobuf[50];
  size_t info_len;
  int retval;

  info_len = 50;
  retval = kenwood_transaction (rig, "IF;", 3, infobuf, &info_len);
  if (retval != RIG_OK)
  return retval;

  if (info_len != 38 || infobuf[1] != 'F') {
    rig_debug(RIG_DEBUG_ERR,"ts790_get_mode: wrong answer len=%d\n",
      info_len);
    return -RIG_ERJCTED;
  }

  switch (infobuf[29]) {
    case MD_CW:		*mode = RIG_MODE_CW; break;
    case MD_USB:	*mode = RIG_MODE_USB; break;
    case MD_LSB:	*mode = RIG_MODE_LSB; break;
    case MD_FM:		*mode = RIG_MODE_FM; break;
    case MD_CWR:	*mode = RIG_MODE_CWR; break;
    case MD_NONE:	*mode = RIG_MODE_NONE; break;
    default:
      rig_debug(RIG_DEBUG_ERR,"ts790_get_mode: "
	"unsupported mode '%c'\n", infobuf[29]);
    return -RIG_EINVAL;
  }

  /* this rig does not have a command for reading IF bandwidth */
  *width = rig_passband_normal(rig, *mode);

  return RIG_OK;
}

static int 
ts790_get_vfo(RIG *rig, vfo_t *vfo)
{
  char infobuf[50];
  size_t info_len;
  int retval;

  info_len = 50;
  retval = kenwood_transaction (rig, "IF;", 3, infobuf, &info_len);
  if (retval != RIG_OK)
  return retval;

  if (info_len != 38 || infobuf[1] != 'F') {
    rig_debug(RIG_DEBUG_ERR,"ts790_get_vfo: wrong answer len=%d\n",
      info_len);
    return -RIG_ERJCTED;
  }

  switch (infobuf[30]) {
    case '0': *vfo = RIG_VFO_A; break;
    case '1': *vfo = RIG_VFO_B; break;
    case '2': *vfo = RIG_VFO_MEM; break;
    default: 
      rig_debug(RIG_DEBUG_ERR,"ts790_get_vfo: unsupported VFO %c\n", 
        infobuf[30]);
      return -RIG_EPROTO;
  }
  return RIG_OK;
}


static const struct kenwood_priv_caps  ts790_priv_caps  = {
		.cmdtrm =  EOM_KEN,
};

/*
 * ts790 rig capabilities.
 *
 * TODO: ts790_set_channel, ts790_get_channel
 * get_split, set_split, get_ts, scan, ctcss_sql, set_rptr_shift
 *
 * part of infos comes from http://www.kenwood.net/
 */
const struct rig_caps ts790_caps = {
.rig_model =  RIG_MODEL_TS790,
.model_name = "TS-790",
.mfg_name =  "Kenwood",
.version =  BACKEND_VER ".1",
.copyright =  "LGPL",
.status =  RIG_STATUS_ALPHA,
.rig_type =  RIG_TYPE_TRANSCEIVER,
.ptt_type =  RIG_PTT_RIG,
.dcd_type =  RIG_DCD_RIG,
.port_type =  RIG_PORT_SERIAL,
.serial_rate_min =  4800,
.serial_rate_max =  4800,
.serial_data_bits =  8,
.serial_stop_bits =  2,
.serial_parity =  RIG_PARITY_NONE,
.serial_handshake =  RIG_HANDSHAKE_HARDWARE,
.write_delay =  0,
.post_write_delay =  0,
.timeout =  1000,
.retry =  3,

.has_get_func =  TS790_FUNC_ALL,
.has_set_func =  TS790_FUNC_ALL,
.has_get_level =  TS790_LEVEL_ALL,
.has_set_level =  RIG_LEVEL_SET(TS790_LEVEL_ALL),
.has_get_parm =  RIG_PARM_NONE,
.has_set_parm =  RIG_PARM_NONE,
.level_gran =  {},                 /* FIXME: granularity */
.parm_gran =  {},
.vfo_ops =  TS790_VFO_OP,
.ctcss_list =  kenwood38_ctcss_list,
.preamp =   { RIG_DBLST_END, },
.attenuator =   { /* 12, */ RIG_DBLST_END, },
.max_rit =  kHz(9.9),	/* this is for FM, SSB/CW is 1.9kHz */
.max_xit =  0,
.max_ifshift =  0,
.targetable_vfo =  RIG_TARGETABLE_FREQ,
.transceive =  RIG_TRN_RIG,
.bank_qty =   0,
.chan_desc_sz =  0,

	/* FIXME: split memories, call channel, etc. */
.chan_list =  { {  1, 59, RIG_MTYPE_MEM },
			 RIG_CHAN_END,
		   },

.rx_range_list1 =  {
	{MHz(144),MHz(146),TS790_ALL_MODES,-1,-1,TS790_VFO},
	{MHz(430),MHz(440),TS790_ALL_MODES,-1,-1,TS790_VFO},
#if 0
	/* 23 cm option */
	{MHz(1240),MHz(1300),TS790_ALL_MODES,-1,-1,TS790_VFO},
#endif
	RIG_FRNG_END,
  }, /* rx range */
.tx_range_list1 =  {
    {MHz(144),MHz(146),TS790_LO_MODES,W(5),W(35),TS790_VFO},
    {MHz(144),MHz(146),TS790_HI_MODES,W(5),W(45),TS790_VFO},
    {MHz(430),MHz(440),TS790_LO_MODES,W(5),W(30),TS790_VFO},
    {MHz(430),MHz(440),TS790_HI_MODES,W(5),W(40),TS790_VFO},
#if 0
	{MHz(1240),MHz(1300),TS790_ALL_MODES,W(5),W(10),TS790_VFO},
#endif
	RIG_FRNG_END,
  }, /* tx range */

.rx_range_list2 =  {
	{MHz(144),MHz(148),TS790_ALL_MODES,-1,-1,TS790_VFO},
	{MHz(430),MHz(450),TS790_ALL_MODES,-1,-1,TS790_VFO},
#if 0
	{MHz(1240),MHz(1300),TS790_ALL_MODES,-1,-1,TS790_VFO},
#endif
	RIG_FRNG_END,
  }, /* rx range */
.tx_range_list2 =  {
    {MHz(144),MHz(148),TS790_LO_MODES,W(5),W(35),TS790_VFO},
    {MHz(144),MHz(148),TS790_HI_MODES,W(5),W(45),TS790_VFO},
    {MHz(430),MHz(450),TS790_LO_MODES,W(5),W(30),TS790_VFO},
    {MHz(430),MHz(450),TS790_HI_MODES,W(5),W(40),TS790_VFO},
#if 0
	{MHz(1240),MHz(1300),TS790_ALL_MODES,W(5),W(10),TS790_VFO},
#endif
	RIG_FRNG_END,
  }, /* tx range */


.tuning_steps =  {
	 {TS790_ALL_MODES,50},
	 {TS790_ALL_MODES,100},
	 {TS790_ALL_MODES,kHz(1)},
	 {TS790_ALL_MODES,kHz(5)},
	 {TS790_ALL_MODES,kHz(9)},
	 {TS790_ALL_MODES,kHz(10)},
	 {TS790_ALL_MODES,12500},
	 {TS790_ALL_MODES,kHz(20)},
	 {TS790_ALL_MODES,kHz(25)},
	 {TS790_ALL_MODES,kHz(100)},
	 {TS790_ALL_MODES,MHz(1)},
	 {TS790_ALL_MODES,0},	/* any tuning step */
	 RIG_TS_END,
	},
        /* mode/filter list, remember: order matters! */
.filters =  {
		{RIG_MODE_SSB|RIG_MODE_CW, kHz(2.1)},
		{RIG_MODE_CWR, Hz(500)},
		{RIG_MODE_FM, kHz(12)},
		RIG_FLT_END,
	},
.priv =  (void *)&ts790_priv_caps,

.set_freq =  kenwood_set_freq,
.get_freq =  kenwood_get_freq,
.set_rit =  kenwood_set_rit,
.get_rit =  kenwood_get_rit,
.set_mode =  kenwood_set_mode,
.get_mode =  ts790_get_mode,
.set_vfo =  kenwood_set_vfo,
.get_vfo =  ts790_get_vfo,
.set_ctcss_tone =  kenwood_set_ctcss_tone,
.get_ctcss_tone =  kenwood_get_ctcss_tone,
.get_ptt =  kenwood_get_ptt,
.set_ptt =  kenwood_set_ptt,
.get_dcd =  kenwood_get_dcd,
.set_func =  kenwood_set_func,
.get_func =  kenwood_get_func,
.set_level =  kenwood_set_level,
.get_level =  kenwood_get_level,
.vfo_op =  kenwood_vfo_op,
.set_mem =  kenwood_set_mem,
.get_mem =  kenwood_get_mem,
.set_trn =  kenwood_set_trn,
.get_trn =  kenwood_get_trn,
.get_info =  kenwood_get_info,

};


