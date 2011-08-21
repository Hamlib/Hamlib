/*
 *  Hamlib PRM80 backend - PRM8060 description
 *  Copyright (c) 2010 by Stephane Fillod
 *
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
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <stdlib.h>
#include <string.h>

#include "hamlib/rig.h"
#include "idx_builtin.h"
#include "prm80.h"


#define PRM8060_ALL_MODES (RIG_MODE_FM)

#define PRM8060_FUNC (RIG_FUNC_NONE)

#define PRM8060_LEVEL_ALL (RIG_LEVEL_AF|RIG_LEVEL_SQL)

#define PRM8060_PARM_ALL (RIG_PARM_NONE)

#define PRM8060_VFO_OPS (RIG_OP_NONE)

#define PRM8060_VFO (RIG_VFO_MEM)

/*
 * PRM 8060 rig capabilities.
 * http://prm80.sourceforge.net/
 *
 */
const struct rig_caps prm8060_caps = {
.rig_model =  RIG_MODEL_PRM8060,
.model_name = "PRM8060",
.mfg_name =  "Philips/Simoco",
.version =  "0.1",
.copyright =  "LGPL",
.status =  RIG_STATUS_ALPHA,
.rig_type =  RIG_TYPE_TRANSCEIVER,
.ptt_type =  RIG_PTT_NONE,
.dcd_type =  RIG_DCD_NONE,
.port_type =  RIG_PORT_SERIAL,
.serial_rate_min =  4800,
.serial_rate_max =  4800,
.serial_data_bits =  7,
.serial_stop_bits =  1,
.serial_parity =  RIG_PARITY_SPACE,
.serial_handshake =  RIG_HANDSHAKE_NONE,
.write_delay =  0,
.post_write_delay =  0,
.timeout =  2000,
.retry =  3,

.has_get_func =  PRM8060_FUNC,
.has_set_func =  PRM8060_FUNC,
.has_get_level =  PRM8060_LEVEL_ALL,
.has_set_level =  RIG_LEVEL_SET(PRM8060_LEVEL_ALL),
.has_get_parm =  PRM8060_PARM_ALL,
.has_set_parm =  RIG_PARM_SET(PRM8060_PARM_ALL),
.vfo_ops =  PRM8060_VFO_OPS,
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
    {   0,  99, RIG_MTYPE_MEM, PRM80_MEM_CAP },
    RIG_CHAN_END,
},

.rx_range_list1 =  {
	{MHz(144),MHz(146)-kHz(12.5),PRM8060_ALL_MODES,-1,-1,PRM8060_VFO},
	RIG_FRNG_END,
  },
.tx_range_list1 =  {
	{MHz(144),MHz(146)-kHz(12.5),PRM8060_ALL_MODES,W(5),W(25),PRM8060_VFO},
	RIG_FRNG_END,
  },
.rx_range_list2 =  {
	{MHz(144),MHz(148)-kHz(12.5),PRM8060_ALL_MODES,-1,-1,PRM8060_VFO},
	RIG_FRNG_END,
  },
.tx_range_list2 =  {
	{MHz(144),MHz(148)-kHz(12.5),PRM8060_ALL_MODES,W(5),W(25),PRM8060_VFO},
	RIG_FRNG_END,
  },
.tuning_steps =  {
	 {PRM8060_ALL_MODES,kHz(12.5)},
	 RIG_TS_END,
	},
        /* mode/filter list, remember: order matters! */
.filters =  {
		/* rough guesses */
		{PRM8060_ALL_MODES, kHz(12.5)},
		RIG_FLT_END,
	},

.set_freq =   prm80_set_freq,
.get_freq =   prm80_get_freq,
.set_channel =  prm80_set_channel,
.get_channel =  prm80_get_channel,
.set_mem =  prm80_set_mem,
.get_mem =  prm80_get_mem,
.set_level =  prm80_set_level,
.get_level =  prm80_get_level,
.reset =  prm80_reset,
.get_info =  prm80_get_info,

};

/*
 * Function definitions below
 */


