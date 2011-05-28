/*
 *  Hamlib Kenwood backend - TH-D72 description
 *                            cloned after TH-D7
 *  Copyright (c) 2000-2011 by Stephane Fillod
 *
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

#include "hamlib/rig.h"
#include "kenwood.h"
#include "th.h"
#include "num_stdio.h"

#if 1
#define RIG_ASSERT(x)	if (!(x)) { rig_debug(RIG_DEBUG_ERR, "Assertion failed on line %i\n",__LINE__); abort(); }
#else
#define RIG_ASSERT(x)
#endif


#define THD72_MODES	  (RIG_MODE_FM|RIG_MODE_AM)
#define THD72_MODES_TX (RIG_MODE_FM)

#define THD72_FUNC_ALL (RIG_FUNC_TSQL|   \
                       RIG_FUNC_AIP|    \
                       RIG_FUNC_MON|    \
                       RIG_FUNC_SQL|    \
                       RIG_FUNC_TONE|   \
                       RIG_FUNC_REV|    \
                       RIG_FUNC_LOCK|   \
                       RIG_FUNC_ARO)

#define THD72_LEVEL_ALL (RIG_LEVEL_STRENGTH| \
                        RIG_LEVEL_SQL| \
                        RIG_LEVEL_AF| \
                        RIG_LEVEL_RF|\
                        RIG_LEVEL_MICGAIN)

#define THD72_PARMS	(RIG_PARM_BACKLIGHT)

#define THD72_VFO_OP (RIG_OP_UP|RIG_OP_DOWN)

/*
 * TODO: Band A & B
 */
#define THD72_VFO (RIG_VFO_A|RIG_VFO_B)

static rmode_t td72_mode_table[KENWOOD_MODE_TABLE_MAX] = {
    [0] = RIG_MODE_FM,
    [1] = RIG_MODE_AM,
};

static struct kenwood_priv_caps  thd72_priv_caps  = {
    .cmdtrm =  EOM_TH,   /* Command termination character */
    .mode_table = td72_mode_table,
};

static int thd72_open(RIG *rig);

/*
 * th-d72a rig capabilities.
 */
const struct rig_caps thd72a_caps = {
.rig_model =  RIG_MODEL_THD72A,
.model_name = "TH-D72A",
.mfg_name =  "Kenwood",
.version =  TH_VER,
.copyright =  "LGPL",
.status =  RIG_STATUS_UNTESTED,
.rig_type =  RIG_TYPE_HANDHELD|RIG_FLAG_APRS|RIG_FLAG_TNC|RIG_FLAG_DXCLUSTER,
.ptt_type =  RIG_PTT_RIG,
.dcd_type =  RIG_DCD_RIG,
.port_type =  RIG_PORT_SERIAL,
.serial_rate_min =  9600,
.serial_rate_max =  9600,
.serial_data_bits =  8,
.serial_stop_bits =  1,
.serial_parity =  RIG_PARITY_NONE,
.serial_handshake =  RIG_HANDSHAKE_XONXOFF,
.write_delay =  0,
.post_write_delay =  0,
.timeout =  250,
.retry =  3,

.has_get_func =  THD72_FUNC_ALL,
.has_set_func =  THD72_FUNC_ALL,
.has_get_level =  THD72_LEVEL_ALL,
.has_set_level =  RIG_LEVEL_SET(THD72_LEVEL_ALL),
.has_get_parm =  THD72_PARMS,
.has_set_parm =  THD72_PARMS,    /* FIXME: parms */
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
.vfo_ops =  THD72_VFO_OP,
.targetable_vfo =  RIG_TARGETABLE_FREQ,
.transceive =  RIG_TRN_RIG,
.bank_qty =   0,
.chan_desc_sz =  6, /* TBC */

.chan_list =  {
                {  0,  999, RIG_MTYPE_MEM , {TH_CHANNEL_CAPS}},  /* TBC MEM */
                RIG_CHAN_END,
                   },
                                                                                                    
.rx_range_list1 =  { RIG_FRNG_END, },    /* FIXME: enter region 1 setting */
.tx_range_list1 =  { RIG_FRNG_END, },
.rx_range_list2 =  {
    {MHz(118),MHz(174),THD72_MODES,-1,-1,THD72_VFO},
    {MHz(320),MHz(524),THD72_MODES,-1,-1,THD72_VFO},
	RIG_FRNG_END,
  }, /* rx range */
.tx_range_list2 =  {
    {MHz(144),MHz(148),THD72_MODES_TX,W(0.05),W(5),THD72_VFO},
    {MHz(430),MHz(440),THD72_MODES_TX,W(0.05),W(5),THD72_VFO},
	RIG_FRNG_END,
  }, /* tx range */

.tuning_steps =  {
	 {THD72_MODES,kHz(5)},
	 {THD72_MODES,kHz(6.25)},
     /* kHz(8.33)  ?? */
	 {THD72_MODES,kHz(10)},
	 {THD72_MODES,kHz(12.5)},
	 {THD72_MODES,kHz(15)},
	 {THD72_MODES,kHz(20)},
	 {THD72_MODES,kHz(25)},
	 {THD72_MODES,kHz(30)},
	 {THD72_MODES,kHz(50)},
	 {THD72_MODES,kHz(100)},
	 RIG_TS_END,
	},
        /* mode/filter list, remember: order matters! */
.filters =  {
		{RIG_MODE_FM, kHz(14)},
		{RIG_MODE_AM, kHz(9)},
		RIG_FLT_END,
	},
.priv =  (void *)&thd72_priv_caps,

.rig_init = kenwood_init,
.rig_cleanup = kenwood_cleanup,
.rig_open = thd72_open,
.set_freq =  th_set_freq,
.get_freq =  th_get_freq,
.set_mode =  th_set_mode,
.get_mode =  th_get_mode,
.set_vfo =  th_set_vfo,
.get_vfo =  th_get_vfo,
.set_ctcss_tone =  th_set_ctcss_tone,
.get_ctcss_tone =  th_get_ctcss_tone,
.set_ctcss_sql =  th_set_ctcss_sql,
.get_ctcss_sql =  th_get_ctcss_sql,
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

.get_dcd =  kenwood_get_dcd,

.decode_event =  th_decode_event,
};


int thd72_open(RIG *rig)
{
    int ret;

    kenwood_simple_cmd(rig, "");

    ret = kenwood_simple_cmd(rig, "TC1");
    if (ret != RIG_OK)
        return ret;

    return RIG_OK;
}

