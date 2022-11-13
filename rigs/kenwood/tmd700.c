/*
 *  Hamlib Kenwood backend - TM-D700 description
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
#include "kenwood.h"
#include "th.h"
#include "tones.h"


#define TMD700_MODES      (RIG_MODE_FM|RIG_MODE_AM)
#define TMD700_MODES_TX (RIG_MODE_FM)

/* TBC */
#define TMD700_FUNC_ALL (RIG_FUNC_TSQL|   \
                       RIG_FUNC_AIP|    \
                       RIG_FUNC_MON|    \
                       RIG_FUNC_MUTE|   \
                       RIG_FUNC_SQL|    \
                       RIG_FUNC_TONE|   \
                       RIG_FUNC_TBURST| \
                       RIG_FUNC_REV|    \
                       RIG_FUNC_LOCK|   \
                       RIG_FUNC_ARO)

#define TMD700_LEVEL_ALL (RIG_LEVEL_STRENGTH| \
                        RIG_LEVEL_SQL| \
                        RIG_LEVEL_AF| \
                        RIG_LEVEL_RF|\
                        RIG_LEVEL_MICGAIN)

#define TMD700_PARMS    (RIG_PARM_BACKLIGHT|\
                        RIG_PARM_BEEP|\
                        RIG_PARM_APO)

#define TMD700_VFO_OP (RIG_OP_UP|RIG_OP_DOWN)

#define TMD700_CHANNEL_CAPS   \
            TH_CHANNEL_CAPS,\
            .flags=1,   \
            .dcs_code=1,    \
            .dcs_sql=1,

#define TMD700_CHANNEL_CAPS_WO_LO \
            TH_CHANNEL_CAPS,\
            .dcs_code=1,    \
            .dcs_sql=1,

/*
 * TODO: Band A & B
 */
#define TMD700_VFO (RIG_VFO_A|RIG_VFO_B)

static rmode_t tmd700_mode_table[KENWOOD_MODE_TABLE_MAX] =
{
    [0] = RIG_MODE_FM,
    [1] = RIG_MODE_AM,
};

static struct kenwood_priv_caps  tmd700_priv_caps  =
{
    .cmdtrm =  EOM_TH,   /* Command termination character */
    .mode_table = tmd700_mode_table,
};



/*
 * TM-D700 rig capabilities.
 *
 * specs: http://www.geocities.jp/hkwatarin/TM-D700/English/spec.htm
 * protocol: http://www.qsl.net/k/k7jar//pages/D700Cmds.html
 */
const struct rig_caps tmd700_caps =
{
    RIG_MODEL(RIG_MODEL_TMD700),
    .model_name = "TM-D700",
    .mfg_name =  "Kenwood",
    .version =  TH_VER ".0",
    .copyright =  "LGPL",
    .status =  RIG_STATUS_STABLE,
    .rig_type =  RIG_TYPE_MOBILE | RIG_FLAG_APRS | RIG_FLAG_TNC,
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
    .timeout =  1000,
    .retry =  3,

    .has_get_func =  TMD700_FUNC_ALL,
    .has_set_func =  TMD700_FUNC_ALL,
    .has_get_level =  TMD700_LEVEL_ALL,
    .has_set_level =  RIG_LEVEL_SET(TMD700_LEVEL_ALL),
    .has_get_parm =  TMD700_PARMS,
    .has_set_parm =  TMD700_PARMS,    /* FIXME: parms */
    .level_gran =
    {
#include "level_gran_kenwood.h"
    },
    .parm_gran =  {},
    .ctcss_list =  kenwood38_ctcss_list,
    .dcs_list =  common_dcs_list,
    .preamp =   { RIG_DBLST_END, },
    .attenuator =   { RIG_DBLST_END, },
    .max_rit =  Hz(0),
    .max_xit =  Hz(0),
    .max_ifshift =  Hz(0),
    .vfo_ops =  TMD700_VFO_OP,
    .scan_ops = RIG_SCAN_VFO,
    .targetable_vfo =  RIG_TARGETABLE_NONE,
    .transceive =  RIG_TRN_RIG,
    .bank_qty =   0,
    .chan_desc_sz =  8,

    .chan_list = {
        {  1,  199, RIG_MTYPE_MEM,  {TMD700_CHANNEL_CAPS}},   /* normal MEM */
        {  200, 219, RIG_MTYPE_EDGE, {TMD700_CHANNEL_CAPS}}, /* U/L MEM */
        {  221, 222, RIG_MTYPE_CALL,  {TMD700_CHANNEL_CAPS_WO_LO}}, /* Call 0/1 */
        RIG_CHAN_END,
    },
    /*
     * TODO: Japan & TM-D700S, and Taiwan models
     */
    .rx_range_list1 =  {
        {MHz(118), MHz(470), TMD700_MODES, -1, -1, RIG_VFO_A},
        {MHz(136), MHz(174), RIG_MODE_FM, -1, -1, RIG_VFO_B},
        {MHz(300), MHz(524), RIG_MODE_FM, -1, -1, RIG_VFO_B},
        {MHz(800), MHz(1300), RIG_MODE_FM, -1, -1, RIG_VFO_B},
        RIG_FRNG_END,
    }, /* rx range */
    .tx_range_list1 =  {
        {MHz(144), MHz(146), TMD700_MODES_TX, W(5), W(50), RIG_VFO_A},
        {MHz(430), MHz(440), TMD700_MODES_TX, W(5), W(35), RIG_VFO_A},
        RIG_FRNG_END,
    }, /* tx range */

    .rx_range_list2 =  {
        {MHz(118), MHz(470), TMD700_MODES, -1, -1, RIG_VFO_A},
        {MHz(136), MHz(174), RIG_MODE_FM, -1, -1, RIG_VFO_B},
        {MHz(300), MHz(524), RIG_MODE_FM, -1, -1, RIG_VFO_B},
        {MHz(800), MHz(1300), RIG_MODE_FM, -1, -1, RIG_VFO_B}, /* TODO: cellular blocked */
        RIG_FRNG_END,
    }, /* rx range */
    .tx_range_list2 =  {
        {MHz(144), MHz(148), TMD700_MODES_TX, W(5), W(50), RIG_VFO_A},
        {MHz(430), MHz(450), TMD700_MODES_TX, W(5), W(35), RIG_VFO_A},
        RIG_FRNG_END,
    }, /* tx range */

    .tuning_steps =  {
        {TMD700_MODES, kHz(5)},
        {TMD700_MODES, kHz(6.25)},
        {TMD700_MODES, kHz(10)},
        {TMD700_MODES, kHz(12.5)},
        {TMD700_MODES, kHz(15)},
        {TMD700_MODES, kHz(20)},
        {TMD700_MODES, kHz(25)},
        {TMD700_MODES, kHz(30)},
        {TMD700_MODES, kHz(50)},
        {TMD700_MODES, kHz(100)},
        RIG_TS_END,
    },
    /* mode/filter list, remember: order matters! */
    .filters =  {
        {RIG_MODE_FM, kHz(12)},
        {RIG_MODE_AM, kHz(9)},  /* TBC */
        RIG_FLT_END,
    },
    .priv = (void *)& tmd700_priv_caps,

    .rig_init = kenwood_init,
    .rig_open = kenwood_open,
    .rig_close = kenwood_close,
    .rig_cleanup = kenwood_cleanup,
    .set_freq =  th_set_freq,
    .get_freq =  th_get_freq,
    .set_mode =  th_set_mode,
    .get_mode =  th_get_mode,
    .set_vfo =  tm_set_vfo_bc2,
    .get_vfo =  th_get_vfo,
    .set_split_vfo =  th_set_split_vfo,
    .get_split_vfo =  th_get_split_vfo,
    .set_ctcss_tone =  th_set_ctcss_tone,
    .get_ctcss_tone =  th_get_ctcss_tone,
    .set_ctcss_sql =  th_set_ctcss_sql,
    .get_ctcss_sql =  th_get_ctcss_sql,
    .set_dcs_sql =  th_set_dcs_sql,
    .get_dcs_sql =  th_get_dcs_sql,
    .set_mem =  th_set_mem,
    .get_mem =  th_get_mem,
    .set_channel =  th_set_channel,
    .get_channel =  th_get_channel,
    .set_trn =  th_set_trn,
    .get_trn =  th_get_trn,

    .set_func =  th_set_func,
    .get_func =  th_get_func,
    .set_level =  th_set_level,
    .get_level =  th_get_level,
    .set_parm =  th_set_parm,
    .get_parm =  th_get_parm,
    .get_info =  th_get_info,
    .get_dcd =  th_get_dcd,
    .set_ptt =  th_set_ptt,
    .vfo_op =  th_vfo_op,
    .scan   =  th_scan,

    .decode_event =  th_decode_event,
    .hamlib_check_rig_caps = HAMLIB_CHECK_RIG_CAPS
};


/* end of file */
