/*
 *  Hamlib CI-V backend - description of IC-910 (VHF/UHF All-Mode Tranceiver)
 *  Contributed by Francois Retief <fgretief@sun.ac.za>
 *  Copyright (c) 2000-2002 by Stephane Fillod
 *
 *      $Id: ic910.c,v 1.3 2002-08-16 17:43:01 fillods Exp $
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
#include "icom.h"

static int ic910_set_mode(RIG *rig, vfo_t vfo, rmode_t mode, pbwidth_t width)
{
    /* FIX: The IC-910 has "Set FM" = 4, which is RTTY in for other radios */
    if (mode == RIG_MODE_FM) {
        mode = RIG_MODE_RTTY;
    }
    return icom_set_mode(rig, vfo, mode, width);
}

static int ic910_get_mode(RIG *rig, vfo_t vfo, rmode_t *mode, pbwidth_t *width)
{
    /* FIX: The IC-910 has "Set FM" = 4, which is RTTY in for other radios */
    int retval = icom_get_mode(rig, vfo, mode, width);
    if (*mode == RIG_MODE_RTTY) {
        *mode = RIG_MODE_FM;
    }
    return retval;
}

#define IC910_MODES (RIG_MODE_SSB|RIG_MODE_CW|RIG_MODE_FM)

#define IC910_VFO_ALL (RIG_VFO_A|RIG_VFO_C)

#define IC910_SCAN_OPS (RIG_SCAN_MEM)

#define IC910_VFO_OPS      (RIG_OP_FROM_VFO| \
                            RIG_OP_TO_VFO| \
                            RIG_OP_CPY| \
                            RIG_OP_MCL)

#define IC910_FUNC_ALL     (RIG_FUNC_FAGC| \
                            RIG_FUNC_NB| \
                            RIG_FUNC_NR| \
                            RIG_FUNC_ANF| \
                            RIG_FUNC_TONE| \
                            RIG_FUNC_TSQL| \
                            RIG_FUNC_COMP| \
                            RIG_FUNC_VOX| \
                            RIG_FUNC_FBKIN| \
                            RIG_FUNC_AFC| \
                            RIG_FUNC_SATMODE| \
                            RIG_FUNC_SCOPE)

#define IC910_LEVEL_ALL    (RIG_LEVEL_AF| \
                            RIG_LEVEL_RF| \
                            RIG_LEVEL_SQL| \
                            RIG_LEVEL_IF| \
                            RIG_LEVEL_NR| \
                            RIG_LEVEL_CWPITCH| \
                            RIG_LEVEL_RFPOWER| \
                            RIG_LEVEL_MICGAIN| \
                            RIG_LEVEL_KEYSPD| \
                            RIG_LEVEL_COMP| \
                            RIG_LEVEL_VOXGAIN| \
                            RIG_LEVEL_VOXDELAY| \
                            RIG_LEVEL_ANTIVOX| \
                            RIG_LEVEL_ATT)

#define IC910_STR_CAL { 0, { } }
/*
 */
static const struct icom_priv_caps ic910_priv_caps = {
    0x60,           /* default address */
    1,              /* 731 mode */
    ic910_ts_sc_list,
    IC910_STR_CAL
};

const struct rig_caps ic910_caps = {
.rig_model =  RIG_MODEL_IC910,
.model_name = "IC-910",
.mfg_name =  "Icom",
.version =  "0.1",
.copyright =  "LGPL",
.status =  RIG_STATUS_ALPHA,
.rig_type =  RIG_TYPE_TRANSCEIVER,
.ptt_type =  RIG_PTT_NONE,
.dcd_type =  RIG_DCD_NONE,
.port_type =  RIG_PORT_SERIAL,
.serial_rate_min =  300,
.serial_rate_max =  19200,
.serial_data_bits =  8,
.serial_stop_bits =  1,
.serial_parity =  RIG_PARITY_NONE,
.serial_handshake =  RIG_HANDSHAKE_NONE,
.write_delay =  0,
.post_write_delay =  0,
.timeout =  200,
.retry =  3,
.has_get_func =  IC910_FUNC_ALL,
.has_set_func =  IC910_FUNC_ALL | RIG_FUNC_RESUME,
.has_get_level =  IC910_LEVEL_ALL | (RIG_LEVEL_SQLSTAT|RIG_LEVEL_STRENGTH),
.has_set_level =  IC910_LEVEL_ALL,
.has_get_parm =  RIG_PARM_NONE,
.has_set_parm =  RIG_PARM_NONE,
.level_gran =  {},
.parm_gran =  {},
.ctcss_list =  NULL,
.dcs_list =  NULL,
.preamp =   { RIG_DBLST_END, },
.attenuator =   { RIG_DBLST_END, },
.max_rit =  Hz(0),     /* SSB,CW: +-1.0kHz  FM: +-5.0kHz */
.max_xit =  Hz(0),
.max_ifshift =  Hz(0),	/* 1.2kHz manual knob */
.targetable_vfo =  0,
.vfo_ops =  IC910_VFO_OPS,
.scan_ops =  IC910_SCAN_OPS,
.transceive =  RIG_TRN_RIG,
.bank_qty =   0,
.chan_desc_sz =  0,

.chan_list =  {
    {   1,  99, RIG_MTYPE_MEM,   0 },
    { 100, 105, RIG_MTYPE_EDGE,  0 },
    { 106, 106, RIG_MTYPE_CALL,  0 },
    RIG_CHAN_END, },

.rx_range_list1 =   {  /* USA */
    {MHz(144),MHz(148),IC910_MODES,-1,-1,IC910_VFO_ALL},
    {MHz(430),MHz(450),IC910_MODES,-1,-1,IC910_VFO_ALL},
	RIG_FRNG_END, },
.tx_range_list1 =  {
    {MHz(144),MHz(148),IC910_MODES,W(5),W(100),IC910_VFO_ALL},
    {MHz(430),MHz(450),IC910_MODES,W(5),W(75),IC910_VFO_ALL},
	RIG_FRNG_END, },

.rx_range_list2 =   { /* Europe */
    {MHz(144),MHz(146),IC910_MODES,-1,-1,IC910_VFO_ALL},
    {MHz(430),MHz(440),IC910_MODES,-1,-1,IC910_VFO_ALL},
	RIG_FRNG_END, },
.tx_range_list2 =  {
    {MHz(144),MHz(146),IC910_MODES,W(5),W(100),IC910_VFO_ALL},
    {MHz(430),MHz(440),IC910_MODES,W(5),W(75),IC910_VFO_ALL},
	RIG_FRNG_END, },

.tuning_steps = 	{
    {RIG_MODE_SSB|RIG_MODE_CW,1},
    {RIG_MODE_SSB|RIG_MODE_CW,10},
    {RIG_MODE_SSB|RIG_MODE_CW,50},
    {RIG_MODE_SSB|RIG_MODE_CW,100},
    {RIG_MODE_FM,kHz(0.1)},
    {RIG_MODE_FM,kHz(5)},
    {RIG_MODE_FM,kHz(6.25)},
    {RIG_MODE_FM,kHz(10)},
    {RIG_MODE_FM,kHz(12.5)},
    {RIG_MODE_FM,kHz(20)},
    {RIG_MODE_FM,kHz(25)},
    {RIG_MODE_FM,kHz(100)},
    RIG_TS_END, },
	/* mode/filter list, remember: order matters! */
.filters = 	{
    {RIG_MODE_CW|RIG_MODE_SSB, kHz(2.3)},   /* buildin */
    {RIG_MODE_FM, kHz(15)},                 /* buildin */
     RIG_FLT_END, },

.priv =  (void*)&ic910_priv_caps,
.rig_init =   icom_init,
.rig_cleanup =   icom_cleanup,
.rig_open =  NULL,
.rig_close =  NULL,

.cfgparams =  icom_cfg_params,
.set_conf =  icom_set_conf,
.get_conf =  icom_get_conf,

.get_freq =  icom_get_freq,
.set_freq =  icom_set_freq,
.get_mode =  ic910_get_mode,
.set_mode =  ic910_set_mode,
.get_vfo =  NULL,
.set_vfo =  icom_set_vfo,
.get_ts =  icom_get_ts,
.set_ts =  icom_set_ts,
.get_func =  icom_get_func,
.set_func =  icom_set_func,
.get_level =  icom_get_level,
.set_level =  icom_set_level,

.set_mem =  icom_set_mem,
.vfo_op =  icom_vfo_op,
.scan =  icom_scan,
.decode_event =  icom_decode_event,
};

/* end of file */
