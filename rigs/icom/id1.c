/*
 *  Hamlib CI-V backend - description of ID-1 and variations
 *  Copyright (c) 2000-2010 by Stephane Fillod
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

#include <hamlib/config.h>


#include "hamlib/rig.h"
#include "idx_builtin.h"
#include "icom.h"

#define ID1_MODES (RIG_MODE_FM /* |RIG_MODE_DIGVOICE|RIG_MODE_DIGDATA*/ )

#define ID1_FUNC_ALL (RIG_FUNC_MUTE|RIG_FUNC_MON|RIG_FUNC_TONE|RIG_FUNC_TSQL|RIG_FUNC_LOCK|RIG_FUNC_AFC)

#define ID1_LEVEL_ALL (RIG_LEVEL_AF|RIG_LEVEL_SQL|RIG_LEVEL_RFPOWER|RIG_LEVEL_PREAMP|RIG_LEVEL_ATT|RIG_LEVEL_RAWSTR)

#define ID1_PARM_ALL (RIG_PARM_BEEP|RIG_PARM_BACKLIGHT /* |RIG_PARM_FAN */)

#define ID1_VFO_ALL (RIG_VFO_A|RIG_VFO_MEM)

#define ID1_VFO_OPS (RIG_OP_FROM_VFO|RIG_OP_TO_VFO|RIG_OP_MCL)
#define ID1_SCAN_OPS (RIG_SCAN_VFO|RIG_SCAN_MEM)

/*
 * FIXME: real measurement
 */
#define ID1_STR_CAL UNKNOWN_IC_STR_CAL


const struct ts_sc_list id1_ts_sc_list[] =
{
    { kHz(5), 0x00 },
    { kHz(10), 0x01 },
    { 12500, 0x02 },
    { kHz(20), 0x03 },
    { kHz(25), 0x04 },
    { kHz(50), 0x05 },
    { kHz(100), 0x06 },
    { kHz(6.25), 0x07 },
    { 0, 0 },
};


/*
 */
static struct icom_priv_caps id1_priv_caps =
{
    0x01,   /* default address */
    0,      /* 731 mode */
    0,    /* no XCHG */
    id1_ts_sc_list
};

const struct rig_caps id1_caps =
{
    RIG_MODEL(RIG_MODEL_ICID1),
    .model_name = "IC ID-1",
    .mfg_name =  "Icom",
    .version =  BACKEND_VER ".0",
    .copyright =  "LGPL",
    .status =  RIG_STATUS_ALPHA,
    .rig_type =  RIG_TYPE_MOBILE,
    .ptt_type =  RIG_PTT_RIG,
    .dcd_type =  RIG_DCD_RIG,
    .port_type =  RIG_PORT_SERIAL,
    .serial_rate_min =  19200,
    .serial_rate_max =  19200,
    .serial_data_bits =  8,
    .serial_stop_bits =  1,
    .serial_parity =  RIG_PARITY_NONE,
    .serial_handshake =  RIG_HANDSHAKE_NONE,
    .write_delay =  0,
    .post_write_delay =  0,
    .timeout =  1000,
    .retry =  3,
    .has_get_func =  ID1_FUNC_ALL,
    .has_set_func =  ID1_FUNC_ALL,
    .has_get_level =  ID1_LEVEL_ALL,
    .has_set_level =  RIG_LEVEL_SET(ID1_LEVEL_ALL),
    .has_get_parm =  ID1_PARM_ALL,
    .has_set_parm =  ID1_PARM_ALL,
    .level_gran =
    {
#include "level_gran_icom.h"
        // cppcheck-suppress *
        [LVL_RAWSTR] = { .min = { .i = 0 }, .max = { .i = 255 } },
    },
    .parm_gran =  {},
    .ctcss_list =  common_ctcss_list,
    .dcs_list =  full_dcs_list,
    .preamp =   { RIG_DBLST_END, },
    .attenuator =   { RIG_DBLST_END, },
    .max_rit =  Hz(0),
    .max_xit =  Hz(0),
    .max_ifshift =  Hz(0),
    .targetable_vfo =  0,
    .vfo_ops =  ID1_VFO_OPS,
    .scan_ops =  ID1_SCAN_OPS,
    .transceive =  RIG_TRN_RIG,
    .bank_qty =   0,
    .chan_desc_sz =  0,

    .chan_list =  {
        {   1,  99, RIG_MTYPE_MEM },
        { 100, 101, RIG_MTYPE_EDGE },    /* two by two */
        { 102, 104, RIG_MTYPE_CALL },
        RIG_CHAN_END,
    },

    .rx_range_list1 =   {
        {GHz(1.240), GHz(1.3), ID1_MODES, -1, -1, ID1_VFO_ALL},
        RIG_FRNG_END,
    },
    .tx_range_list1 =  {
        {GHz(1.240), GHz(1.3), ID1_MODES, W(1), W(10), ID1_VFO_ALL},
        RIG_FRNG_END,
    },

    .rx_range_list2 =   {
        {GHz(1.240), GHz(1.3), ID1_MODES, -1, -1, ID1_VFO_ALL},
        RIG_FRNG_END,
    },
    .tx_range_list2 =  {
        {GHz(1.240), GHz(1.3), ID1_MODES, W(1), W(10), ID1_VFO_ALL},
        RIG_FRNG_END,
    },

    .tuning_steps =     {
        {ID1_MODES, kHz(5)},
        {ID1_MODES, kHz(6.25)},
        {ID1_MODES, kHz(10)},
        {ID1_MODES, kHz(12.5)},
        {ID1_MODES, kHz(20)},
        {ID1_MODES, kHz(25)},
        {ID1_MODES, kHz(25)},
        {ID1_MODES, kHz(100)},
        RIG_TS_END,
    },
    /* mode/filter list, remember: order matters! */
    .filters =  {
        {RIG_MODE_FM, kHz(12)},
#if 0
        {RIG_MODE_DIGVOICE, kHz(6)},
        {RIG_MODE_DIGDATA, kHz(140)},
#endif
        RIG_FLT_END,
    },
    .str_cal = ID1_STR_CAL,

    .cfgparams =  icom_cfg_params,
    .set_conf =  icom_set_conf,
    .get_conf =  icom_get_conf,

    .priv = (void *)& id1_priv_caps,
    .rig_init =   icom_init,
    .rig_cleanup =   icom_cleanup,
    .rig_open =  icom_rig_open,
    .rig_close =  icom_rig_open,

    .set_freq =  icom_set_freq,
    .get_freq =  icom_get_freq,
    .set_mode =  icom_set_mode,
    .get_mode =  icom_get_mode,
    .set_vfo =  icom_set_vfo,

    .decode_event =  icom_decode_event,
    .set_level =  icom_set_level,
    .get_level =  icom_get_level,
    .set_func =  icom_set_func,
    .get_func =  icom_get_func,
    .set_mem =  icom_set_mem,
    .vfo_op =  icom_vfo_op,
    .scan =  icom_scan,
    .set_ptt =  icom_set_ptt,
    .get_ptt =  icom_get_ptt,
    .get_dcd =  icom_get_dcd,
    .set_ts =  icom_set_ts,
    .set_rptr_shift =  icom_set_rptr_shift,
    .set_rptr_offs =  icom_set_rptr_offs,
    .get_rptr_offs =  icom_get_rptr_offs,

    .hamlib_check_rig_caps = HAMLIB_CHECK_RIG_CAPS
};

