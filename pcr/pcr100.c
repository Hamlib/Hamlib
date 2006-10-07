/*
 *  Hamlib PCR backend - PCR-100 description
 *  Copyright (c) 2001-2004 by Stephane Fillod
 *
 *	$Id: pcr100.c,v 1.7 2006-10-07 16:42:19 csete Exp $
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

#include "pcr.h"


#define PCR100_MODES (RIG_MODE_AM|RIG_MODE_FM|RIG_MODE_WFM)

#define PCR100_FUNC (RIG_FUNC_FAGC|RIG_FUNC_NB|RIG_FUNC_TSQL|RIG_FUNC_ANF|RIG_FUNC_NR)

#define PCR100_LEVEL (RIG_LEVEL_ATT|RIG_LEVEL_AGC|RIG_LEVEL_SQL|RIG_LEVEL_AF|RIG_LEVEL_STRENGTH)

/*
 * IC PCR100 rigs capabilities.
 */
const struct rig_caps pcr100_caps = {
.rig_model =  RIG_MODEL_PCR100,
.model_name = "IC-PCR100",
.mfg_name =  "Icom",
.version =  BACKEND_VER,
.copyright =  "LGPL",
.status =  RIG_STATUS_UNTESTED,
.rig_type =  RIG_TYPE_PCRECEIVER,
.ptt_type =  RIG_PTT_NONE,
.dcd_type =  RIG_DCD_RIG,
.port_type =  RIG_PORT_SERIAL,
.serial_rate_min =  300,
.serial_rate_max =  38400,
.serial_data_bits =  8,
.serial_stop_bits =  1,
.serial_parity =  RIG_PARITY_NONE,
.serial_handshake =  RIG_HANDSHAKE_NONE,
.write_delay =  0,
.post_write_delay =  0,
.timeout =  200,
.retry =  3,

.has_get_func =  RIG_FUNC_NONE,
.has_set_func =  PCR100_FUNC,
.has_get_level =  PCR100_LEVEL,
.has_set_level =  RIG_LEVEL_SET(PCR100_LEVEL),
.has_get_parm =  RIG_PARM_NONE,
.has_set_parm =  RIG_PARM_NONE,    /* FIXME: parms */
.level_gran =  {},                 /* FIXME: granularity */
.parm_gran =  {},
.ctcss_list = pcr1_ctcss_list,
.dcs_list =  NULL,
.preamp =   { RIG_DBLST_END, },
.attenuator =   { 20, RIG_DBLST_END, },
.max_rit =  Hz(0),
.max_xit =  Hz(0),
.max_ifshift =  Hz(1.2),
.targetable_vfo =  0,
.transceive =  RIG_TRN_RIG,
.bank_qty =   0,
.chan_desc_sz =  0,

.chan_list =  { RIG_CHAN_END, },	/* no memory channel list: this is a PC receiver */

.rx_range_list1 =  { {kHz(10),GHz(1.3),PCR100_MODES,-1,-1,RIG_VFO_A},
 	RIG_FRNG_END, },
.tx_range_list1 =  { RIG_FRNG_END, },
.rx_range_list2 =  { {kHz(10),MHz(824)-10,PCR100_MODES,-1,-1,RIG_VFO_A},
    {MHz(849)+10,MHz(869)-10,PCR100_MODES,-1,-1,RIG_VFO_A},
    {MHz(894)+10,GHz(1.3),PCR100_MODES,-1,-1,RIG_VFO_A},
 	RIG_FRNG_END, },
.tx_range_list2 =  { RIG_FRNG_END, },	/* no TX ranges, this is a receiver */

.tuning_steps =  {
	  { PCR100_MODES,Hz(1) },
	  RIG_TS_END,
	},
	      /* mode/filter list, remember: order matters! */
.filters =  {
		{RIG_MODE_FM|RIG_MODE_AM, kHz(6)},
		{RIG_MODE_FM|RIG_MODE_AM, kHz(15)},
		{RIG_MODE_FM|RIG_MODE_AM, kHz(50)},
		{RIG_MODE_WFM, kHz(230)},
		{RIG_MODE_WFM, kHz(50)},
		RIG_FLT_END,
  },
.priv =  NULL,	/* priv */

.rig_init =  pcr_init,
.rig_cleanup =  pcr_cleanup,
.rig_open =  NULL,
.rig_close =  NULL,

.set_freq =  pcr_set_freq,
.get_freq =  pcr_get_freq,
.set_mode =  pcr_set_mode,
.get_mode =  pcr_get_mode,

.get_info = 	pcr_get_info,

  /*
   * TODO:
   * set_trn, set_powerstat,
   * set_level AF,SQL,IF,AF,ATT
   * set_func: AGC,NB,TSQL, VSC?
   * get_level, get_dcd, dtmf, ..
   * decode_event, set_ctcss, set_ctcss_sql
   * and also all the associated get_ functions.
   */
};

