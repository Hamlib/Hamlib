/*
 *  Hamlib Uniden backend - BC895 description
 *  Copyright (c) 2001-2003 by Stephane Fillod
 *
 *	$Id: bc895.c,v 1.5 2005-04-03 20:23:19 fillods Exp $
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
#include "uniden.h"


#define BC895_MODES (RIG_MODE_AM|RIG_MODE_FM|RIG_MODE_WFM)

#define BC895_FUNC (RIG_FUNC_SQL)

#define BC895_LEVEL_ALL (RIG_LEVEL_NONE)

#define BC895_PARM_ALL (RIG_PARM_NONE)

#define BC895_VFO RIG_VFO_A

/*
 * bc895 rig capabilities.
 *
 * TODO: check this with manual or web site.
 */
const struct rig_caps bc895_caps = {
.rig_model =  RIG_MODEL_BC895,
.model_name = "BC895xlt",
.mfg_name =  "Uniden",
.version =  BACKEND_VER,
.copyright =  "LGPL",
.status =  RIG_STATUS_UNTESTED,
.rig_type =  RIG_TYPE_TRUNKSCANNER,
.ptt_type =  RIG_PTT_NONE,
.dcd_type =  RIG_DCD_NONE,
.port_type =  RIG_PORT_SERIAL,
.serial_rate_min =  9600,
.serial_rate_max =  9600,
.serial_data_bits =  8,
.serial_stop_bits =  1,
.serial_parity =  RIG_PARITY_NONE,
.serial_handshake =  RIG_HANDSHAKE_NONE,
.write_delay =  0,
.post_write_delay =  1,
.timeout =  200,
.retry =  3,

.has_get_func =  BC895_FUNC,
.has_set_func =  BC895_FUNC,
.has_get_level =  BC895_LEVEL_ALL,
.has_set_level =  RIG_LEVEL_SET(BC895_LEVEL_ALL),
.has_get_parm =  BC895_PARM_ALL,
.has_set_parm =  RIG_PARM_SET(BC895_PARM_ALL),
.level_gran =  {},                 /* FIXME: granularity */
.parm_gran =  {},
.ctcss_list =  NULL,	/* FIXME: CTCSS list? */
.dcs_list =  NULL,
.preamp =   { RIG_DBLST_END },
.attenuator =   { RIG_DBLST_END },
.max_rit =  Hz(0),
.max_xit =  Hz(0),
.max_ifshift =  Hz(0),
.targetable_vfo =  0,
.transceive =  RIG_TRN_OFF,
.bank_qty =   0,
.chan_desc_sz =  0,

.chan_list =  {
		{ 1, 300, RIG_MTYPE_MEM },
		RIG_CHAN_END,
	},

.rx_range_list1 =  { RIG_FRNG_END, },    /* FIXME: enter region 1 setting */
.tx_range_list1 =  { RIG_FRNG_END, },
.rx_range_list2 =  {
	{MHz(29),MHz(956),BC895_MODES,-1,-1,BC895_VFO},
	RIG_FRNG_END,
  },
.tx_range_list2 =  { RIG_FRNG_END, },
.tuning_steps =  {
	 {BC895_MODES,10},	/* FIXME: add other ts */
	 RIG_TS_END,
	},
        /* mode/filter list, remember: order matters! */
.filters =  {
		{RIG_MODE_AM|RIG_MODE_FM, kHz(8)},
		{RIG_MODE_WFM, kHz(230)},
		RIG_FLT_END,
	},
.priv =  NULL,

.set_freq =  uniden_set_freq,
.set_mem =  uniden_set_mem,

};

/*
 * Function definitions below
 */

