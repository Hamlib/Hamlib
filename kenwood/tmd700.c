/*
 *  Hamlib Kenwood backend - TM-D700 description
 *  Copyright (c) 2000-2004 by Stephane Fillod
 *
 *	$Id: tmd700.c,v 1.6 2005-04-03 20:14:26 fillods Exp $
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


#define TMD700_MODES	  (RIG_MODE_FM|RIG_MODE_AM)
#define TMD700_MODES_TX (RIG_MODE_FM)

/* TBC */
#define TMD700_FUNC_ALL (RIG_FUNC_TSQL|   \
                       RIG_FUNC_AIP|    \
                       RIG_FUNC_MON|    \
                       RIG_FUNC_SQL|    \
                       RIG_FUNC_TONE|   \
                       RIG_FUNC_REV|    \
                       RIG_FUNC_LOCK|   \
                       RIG_FUNC_ARO)

#define TMD700_LEVEL_ALL (RIG_LEVEL_STRENGTH| \
                        RIG_LEVEL_SQL| \
                        RIG_LEVEL_AF| \
                        RIG_LEVEL_RF|\
                        RIG_LEVEL_MICGAIN)

#define TMD700_PARMS	(RIG_PARM_BACKLIGHT)

#define TMD700_VFO_OP (RIG_OP_UP|RIG_OP_DOWN)

/*
 * TODO: Band A & B
 */
#define TMD700_VFO (RIG_VFO_A|RIG_VFO_B)

const struct kenwood_priv_caps  tmd700_priv_caps  = {
    .cmdtrm =  EOM_TH,   /* Command termination character */
};


/*
 * TM-D700 rig capabilities.
 *
 * specs from http://www.geocities.jp/hkwatarin/TM-D700/English/spec.htm
 */
const struct rig_caps tmd700_caps = {
.rig_model =  RIG_MODEL_TMD700,
.model_name = "TM-D700",
.mfg_name =  "Kenwood",
.version =  TH_VER,
.copyright =  "LGPL",
.status =  RIG_STATUS_NEW,
.rig_type =  RIG_TYPE_MOBILE|RIG_FLAG_APRS|RIG_FLAG_TNC,
.ptt_type =  RIG_PTT_RIG,
.dcd_type =  RIG_DCD_RIG,
.port_type =  RIG_PORT_SERIAL,
.serial_rate_min =  9600,
.serial_rate_max =  57600,
.serial_data_bits =  8,
.serial_stop_bits =  1,
.serial_parity =  RIG_PARITY_NONE,
.serial_handshake =  RIG_HANDSHAKE_NONE,
.write_delay =  0,
.post_write_delay =  0,
.timeout =  200,
.retry =  3,

.has_get_func =  TMD700_FUNC_ALL,
.has_set_func =  TMD700_FUNC_ALL,
.has_get_level =  TMD700_LEVEL_ALL,
.has_set_level =  RIG_LEVEL_SET(TMD700_LEVEL_ALL),
.has_get_parm =  TMD700_PARMS,
.has_set_parm =  TMD700_PARMS,    /* FIXME: parms */
.level_gran = {
        [LVL_RAWSTR] = { .min = { .i = 0 }, .max = { .i = 5 } },
        [LVL_SQL] = { .min = { .i = 0 }, .max = { .i = 5 } },
        [LVL_RFPOWER] = { .min = { .i = 3 }, .max = { .i = 0 } },
},
.parm_gran =  {},
.ctcss_list =  kenwood38_ctcss_list,
.dcs_list =  NULL,
.preamp =   { RIG_DBLST_END, },
.attenuator =   { RIG_DBLST_END, },
.max_rit =  Hz(0),
.max_xit =  Hz(0),
.max_ifshift =  Hz(0),
.vfo_ops =  TMD700_VFO_OP,
.targetable_vfo =  RIG_TARGETABLE_FREQ,
.transceive =  RIG_TRN_RIG,
.bank_qty =   0,
.chan_desc_sz =  0,

.chan_list =  {
                {  1,  199, RIG_MTYPE_MEM , {TH_CHANNEL_CAPS}},  /* normal MEM */
                {  200,219, RIG_MTYPE_EDGE , {TH_CHANNEL_CAPS}}, /* U/L MEM */
                {  221,222, RIG_MTYPE_MEM , {TH_CHANNEL_CAPS}},  /* Call 0/1 */
                RIG_CHAN_END,
                   },
/*
 * TODO: Japan & TM-D700S, and Taiwan models
 */
.rx_range_list1 =  {
    {MHz(118),MHz(470),TMD700_MODES,-1,-1,RIG_VFO_A},
    {MHz(136),MHz(174),RIG_MODE_FM,-1,-1,RIG_VFO_B},
    {MHz(300),MHz(524),RIG_MODE_FM,-1,-1,RIG_VFO_B},
    {MHz(800),MHz(1300),RIG_MODE_FM,-1,-1,RIG_VFO_B},
	RIG_FRNG_END,
  }, /* rx range */
.tx_range_list1 =  {
    {MHz(144),MHz(146),TMD700_MODES_TX,W(5),W(50),RIG_VFO_A},
    {MHz(430),MHz(440),TMD700_MODES_TX,W(5),W(35),RIG_VFO_A},
	RIG_FRNG_END,
  }, /* tx range */

.rx_range_list2 =  {
    {MHz(118),MHz(470),TMD700_MODES,-1,-1,RIG_VFO_A},
    {MHz(136),MHz(174),RIG_MODE_FM,-1,-1,RIG_VFO_B},
    {MHz(300),MHz(524),RIG_MODE_FM,-1,-1,RIG_VFO_B},
    {MHz(800),MHz(1300),RIG_MODE_FM,-1,-1,RIG_VFO_B},	/* TODO: cellular blocked */
	RIG_FRNG_END,
  }, /* rx range */
.tx_range_list2 =  {
    {MHz(144),MHz(148),TMD700_MODES_TX,W(5),W(50),RIG_VFO_A},
    {MHz(430),MHz(450),TMD700_MODES_TX,W(5),W(35),RIG_VFO_A},
	RIG_FRNG_END,
  }, /* tx range */

/* TBC */
.tuning_steps =  {
	 {TMD700_MODES,kHz(5)},
	 {TMD700_MODES,kHz(6.25)},
	 {TMD700_MODES,kHz(10)},
	 {TMD700_MODES,kHz(12.5)},
	 {TMD700_MODES,kHz(15)},
	 {TMD700_MODES,kHz(20)},
	 {TMD700_MODES,kHz(25)},
	 {TMD700_MODES,kHz(30)},
	 {TMD700_MODES,kHz(50)},
	 {TMD700_MODES,kHz(100)},
	 RIG_TS_END,
	},
        /* mode/filter list, remember: order matters! */
.filters =  {
		{RIG_MODE_FM, kHz(12)},
		{RIG_MODE_AM, kHz(9)},	/* TBC */
		RIG_FLT_END,
	},
.priv =  (void *)&tmd700_priv_caps,

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
.set_channel =  th_set_channel,
.get_channel =  th_get_channel,
.set_trn =  th_set_trn,
.get_trn =  th_get_trn,

.get_func =  th_get_func,
.get_level =  th_get_level,
.get_parm =  th_get_parm,
.get_info =  th_get_info,
.get_dcd =  th_get_dcd,

.decode_event =  th_decode_event,
};


/* end of file */
