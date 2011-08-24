/*
 *  Hamlib TenTenc backend - RX-350 description
 *  Copyright (c) 2003-2004 by Stephane Fillod
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

#include <hamlib/rig.h>
#include "tentec2.h"
#include "bandplan.h"


#define RX350_MODES (RIG_MODE_FM|RIG_MODE_CW|RIG_MODE_SSB|\
			RIG_MODE_AM|RIG_MODE_AMS)

#define RX350_FUNCS (RIG_FUNC_NR|RIG_FUNC_ANF)

#define RX350_LEVELS (RIG_LEVEL_RAWSTR|/*RIG_LEVEL_NB|*/ \
				RIG_LEVEL_RF|RIG_LEVEL_IF| \
				RIG_LEVEL_AF|RIG_LEVEL_AGC| \
				RIG_LEVEL_SQL|RIG_LEVEL_ATT)

#define RX350_ANTS (RIG_ANT_1)

#define RX350_PARMS (RIG_PARM_TIME)

#define RX350_VFO (RIG_VFO_A|RIG_VFO_B)

#define RX350_VFO_OPS (RIG_OP_TO_VFO|RIG_OP_FROM_VFO)


/*
 * RX350 receiver capabilities.
 *
 * Protocol is documented at
 *		http://www.rfsquared.com/
 *
 * Only set_freq is supposed to work.
 * This is a skelton.
 */
const struct rig_caps rx350_caps = {
.rig_model =  RIG_MODEL_RX350,
.model_name = "RX-350",
.mfg_name =  "Ten-Tec",
.version =  "0.1",
.copyright =  "LGPL",
.status =  RIG_STATUS_UNTESTED,
.rig_type =  RIG_TYPE_RECEIVER,
.ptt_type =  RIG_PTT_NONE,
.dcd_type =  RIG_DCD_NONE,
.port_type =  RIG_PORT_SERIAL,
.serial_rate_min =  57600,
.serial_rate_max =  57600,
.serial_data_bits =  8,
.serial_stop_bits =  1,
.serial_parity =  RIG_PARITY_NONE,
.serial_handshake =  RIG_HANDSHAKE_HARDWARE,
.write_delay =  0,
.post_write_delay =  0,
.timeout =  400,
.retry =  3,

.has_get_func =  RX350_FUNCS,
.has_set_func =  RX350_FUNCS,
.has_get_level =  RX350_LEVELS,
.has_set_level =  RIG_LEVEL_SET(RX350_LEVELS),
.has_get_parm =  RX350_PARMS,
.has_set_parm =  RX350_PARMS,
.level_gran =  {},                 /* FIXME: granularity */
.parm_gran =  {},
.ctcss_list =  NULL,
.dcs_list =  NULL,
.preamp =   { RIG_DBLST_END },
.attenuator =   { 20, RIG_DBLST_END },
.max_rit =  Hz(0),
.max_xit =  Hz(0),
.max_ifshift =  kHz(2),
.targetable_vfo =  RIG_TARGETABLE_FREQ|RIG_TARGETABLE_MODE,
.transceive =  RIG_TRN_OFF,
.bank_qty =   8,
.chan_desc_sz =  15,

.chan_list =  {
		{   0, 127, RIG_MTYPE_MEM, TT_MEM_CAP },
		},

.rx_range_list1 =  {
	{kHz(100),MHz(30),RX350_MODES,-1,-1,RX350_VFO,RX350_ANTS},
	RIG_FRNG_END,
  },
.tx_range_list1 =  {
	RIG_FRNG_END,
  },

.rx_range_list2 =  {
	{kHz(100),MHz(30),RX350_MODES,-1,-1,RX350_VFO,RX350_ANTS},
	RIG_FRNG_END,
  },
.tx_range_list2 =  {
	RIG_FRNG_END,
  },

.tuning_steps =  {
	 {RX350_MODES,1},
	 RIG_TS_END,
	},
        /* mode/filter list, remember: order matters! */
.filters =  {
		{RX350_MODES, kHz(2.4)},
		{RX350_MODES, 300},
		{RX350_MODES, kHz(8)},
		{RX350_MODES, 0}, /* 34 filters */
		RIG_FLT_END,
	},
.priv =  (void*)NULL,

.set_freq =  tentec2_set_freq,
.get_freq =  tentec2_get_freq,
.set_vfo =  tentec2_set_vfo,
.get_vfo =  tentec2_get_vfo,
.set_mode =  tentec2_set_mode,
.get_mode =  tentec2_get_mode,
.reset =  tentec2_reset,
.get_info =  tentec2_get_info,

};

/*
 * Function definitions below
 */


