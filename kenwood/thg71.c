/*
 *  Hamlib Kenwood backend - TH-G71 description
 *  Copyright (c) 2003 by Stephane Fillod
 *
 *	$Id: thg71.c,v 1.2 2003-10-01 19:31:58 fillods Exp $
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
#include "th.h"

#if 1
#define RIG_ASSERT(x)	if (!(x)) { rig_debug(RIG_DEBUG_ERR, "Assertion failed on line %i\n",__LINE__); abort(); }
#else
#define RIG_ASSERT(x)
#endif


#define THG71_MODES	  (RIG_MODE_FM|RIG_MODE_AM)
#define THG71_MODES_TX (RIG_MODE_FM)

#define THG71_FUNC_ALL (RIG_FUNC_TSQL|   \
                       RIG_FUNC_AIP|    \
                       RIG_FUNC_MON|    \
                       RIG_FUNC_SQL|    \
                       RIG_FUNC_TONE|   \
                       RIG_FUNC_REV|    \
                       RIG_FUNC_LOCK|   \
                       RIG_FUNC_ARO)

#define THG71_LEVEL_ALL (RIG_LEVEL_STRENGTH| \
                        RIG_LEVEL_SQL| \
                        RIG_LEVEL_SQLSTAT| \
                        RIG_LEVEL_AF| \
                        RIG_LEVEL_RF|\
                        RIG_LEVEL_MICGAIN)

#define THG71_PARMS	(RIG_PARM_BACKLIGHT)

#define THG71_VFO_OP (RIG_OP_UP|RIG_OP_DOWN)

/*
 * TODO: Band A & B
 */
#define THG71_VFO (RIG_VFO_A|RIG_VFO_B)

const struct kenwood_priv_caps  thg71_priv_caps  = {
    .cmdtrm =  EOM_TH,   /* Command termination character */
};


/*
 * th-g71 rig capabilities.
 */
const struct rig_caps thg71_caps = {
.rig_model =  RIG_MODEL_THG71,
.model_name = "TH-G71",
.mfg_name =  "Kenwood",
.version =  "0.1",
.copyright =  "LGPL",
.status =  RIG_STATUS_NEW,
.rig_type =  RIG_TYPE_HANDHELD,
.ptt_type =  RIG_PTT_RIG,
.dcd_type =  RIG_DCD_RIG,
.port_type =  RIG_PORT_SERIAL,
.serial_rate_min =  9600,
.serial_rate_max =  9600,
.serial_data_bits =  8,
.serial_stop_bits =  1,
.serial_parity =  RIG_PARITY_NONE,
.serial_handshake =  RIG_HANDSHAKE_NONE,
.write_delay =  0,
.post_write_delay =  0,
.timeout =  200,
.retry =  3,

.has_get_func =  THG71_FUNC_ALL,
.has_set_func =  THG71_FUNC_ALL,
.has_get_level =  THG71_LEVEL_ALL,
.has_set_level =  RIG_LEVEL_SET(THG71_LEVEL_ALL),
.has_get_parm =  THG71_PARMS,
.has_set_parm =  THG71_PARMS,    /* FIXME: parms */
.level_gran =  {},                 /* FIXME: granularity */
.parm_gran =  {},
.ctcss_list =  kenwood38_ctcss_list,
.dcs_list =  NULL,
.preamp =   { RIG_DBLST_END, },
.attenuator =   { RIG_DBLST_END, },
.max_rit =  Hz(0),
.max_xit =  Hz(0),
.max_ifshift =  Hz(0),
.vfo_ops =  THG71_VFO_OP,
.targetable_vfo =  RIG_TARGETABLE_FREQ,
.transceive =  RIG_TRN_RIG,
.bank_qty =   0,
.chan_desc_sz =  0,


.chan_list =  { {  1,  200, RIG_MTYPE_MEM },
			 RIG_CHAN_END,
		   },	/* FIXME: memory channel list: 200 memories */

/* region 1 */
.rx_range_list1 =  {
    {MHz(118),MHz(174),THG71_MODES,-1,-1,THG71_VFO},
    {MHz(400),MHz(470),THG71_MODES,-1,-1,THG71_VFO},
	RIG_FRNG_END,
  }, /* rx range */
.tx_range_list1 =  {
    {MHz(144),MHz(146),THG71_MODES_TX,mW(50),W(6),THG71_VFO},
    {MHz(430),MHz(440),THG71_MODES_TX,mW(50),W(5.5),THG71_VFO},
	RIG_FRNG_END,
  }, /* tx range */

/* region 2 */
.rx_range_list2 =  {
    {MHz(118),MHz(174),THG71_MODES,-1,-1,THG71_VFO},
    {MHz(400),MHz(470),THG71_MODES,-1,-1,THG71_VFO},
	RIG_FRNG_END,
  }, /* rx range */
.tx_range_list2 =  {
    {MHz(144),MHz(148),THG71_MODES_TX,mW(50),W(6),THG71_VFO},
    {MHz(430),MHz(440),THG71_MODES_TX,mW(50),W(5.5),THG71_VFO},
	RIG_FRNG_END,
  }, /* tx range */

.tuning_steps =  {
	 {THG71_MODES,kHz(5)},
	 {THG71_MODES,kHz(6.25)},
	 {THG71_MODES,kHz(10)},
	 {THG71_MODES,kHz(12.5)},
	 {THG71_MODES,kHz(15)},
	 {THG71_MODES,kHz(20)},
	 {THG71_MODES,kHz(25)},
	 {THG71_MODES,kHz(30)},
	 {THG71_MODES,kHz(50)},
	 {THG71_MODES,kHz(100)},
	 RIG_TS_END,
	},
        /* mode/filter list, remember: order matters! */
.filters =  {
		{RIG_MODE_FM, kHz(12)},
		{RIG_MODE_AM, kHz(9)},
		RIG_FLT_END,
	},
.priv =  (void *)&thg71_priv_caps,
.rig_init =  kenwood_init,
.rig_cleanup =  kenwood_cleanup,
.rig_open =  NULL,
.rig_close =  NULL,

.set_freq =  th_set_freq,
.get_freq =  th_get_freq,
.set_mode =  th_set_mode,
.get_mode =  th_get_mode,
.set_vfo =  th_set_vfo,
.get_vfo =  th_get_vfo,
.set_ctcss_tone =  th_set_ctcss_tone,
.get_ctcss_tone =  th_get_ctcss_tone,
.set_mem =  th_set_mem,
.get_mem =  th_get_mem,
.set_trn =  th_set_trn,
.get_trn =  th_get_trn,

.get_func =  th_get_func,
.get_level =  th_get_level,
.get_parm =  th_get_parm,
.get_info =  th_get_info,

.decode_event =  th_decode_event,
};


/* end of file */

