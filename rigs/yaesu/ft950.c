/*
 * hamlib - (C) Frank Singleton 2000 (javabear at users.sourceforge.net)
 *
 * ft950.c - (C) Nate Bargmann 2007 (n0nb at arrl.net)
 *           (C) Stephane Fillod 2008
 *           (C) Terry Embry 2008-2009
 *
 * This shared library provides an API for communicating
 * via serial interface to an FT-950 using the "CAT" interface
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
#include "bandplan.h"
#include "serial.h"
#include "misc.h"
#include "yaesu.h"
#include "newcat.h"
#include "ft950.h"

const struct newcat_priv_caps ft950_priv_caps =
{
    .roofing_filter_count = 7,
    .roofing_filters =
    {
        // The index must match ext level combo index
        { .index = 0, .set_value = '0', .get_value = 0, .width = 15000, .optional = 0 },
        { .index = 1, .set_value = '1', .get_value = '1', .width = 15000, .optional = 0 },
        { .index = 2, .set_value = '2', .get_value = '2', .width = 6000, .optional = 0 },
        { .index = 3, .set_value = '3', .get_value = '3', .width = 3000, .optional = 0 },
        { .index = 4, .set_value = 0, .get_value = '4', .width = 15000, .optional = 0 },
        { .index = 5, .set_value = 0, .get_value = '5', .width = 6000, .optional = 0 },
        { .index = 6, .set_value = 0, .get_value = '6', .width = 3000, .optional = 0 },
    }
};

const struct confparams ft950_ext_levels[] =
{
    {
        TOK_ROOFING_FILTER,
        "ROOFINGFILTER",
        "Roofing filter",
        "Roofing filter",
        NULL,
        RIG_CONF_COMBO,
        {
            .c = {
                .combostr = {
                    "AUTO", "15 kHz", "6 kHz", "3 kHz",
                    "AUTO - 15 kHz", "AUTO - 6 kHz", "AUTO - 3 kHz",
                    NULL
                }
            }
        }
    },
    { RIG_CONF_END, NULL, }
};

int ft950_ext_tokens[] =
{
    TOK_ROOFING_FILTER, TOK_BACKEND_NONE
};

/*
 * FT-950 rig capabilities
 */
const struct rig_caps ft950_caps =
{
    RIG_MODEL(RIG_MODEL_FT950),
    .model_name =         "FT-950",
    .mfg_name =           "Yaesu",
    .version =            NEWCAT_VER ".4",
    .copyright =          "LGPL",
    .status =             RIG_STATUS_STABLE,
    .rig_type =           RIG_TYPE_TRANSCEIVER,
    .ptt_type =           RIG_PTT_RIG,
    .dcd_type =           RIG_DCD_NONE,
    .port_type =          RIG_PORT_SERIAL,
    .serial_rate_min =    4800,         /* Default rate per manual */
    .serial_rate_max =    38400,
    .serial_data_bits =   8,
    .serial_stop_bits =   2,            /* Assumed since manual makes no mention */
    .serial_parity =      RIG_PARITY_NONE,
    .serial_handshake =   RIG_HANDSHAKE_HARDWARE,
    .write_delay =        FT950_WRITE_DELAY,
    .post_write_delay =   FT950_POST_WRITE_DELAY,
    .timeout =            2000,
    .retry =              3,
    .has_get_func =       FT950_FUNCS,
    .has_set_func =       FT950_FUNCS,
    .has_get_level =      FT950_LEVELS,
    .has_set_level =      RIG_LEVEL_SET(FT950_LEVELS),
    .has_get_parm =       RIG_PARM_NONE,
    .has_set_parm =       RIG_PARM_NONE,
    .level_gran =
    {
#include "level_gran_yaesu.h"
        // cppcheck-suppress *
        [LVL_RAWSTR] = { .min = { .i = 0 }, .max = { .i = 255 } },
        [LVL_CWPITCH] = { .min = { .i = 300 }, .max = { .i = 1050 }, .step = { .i = 50 } },
        [LVL_KEYSPD] = { .min = { .i = 4 }, .max = { .i = 60 }, .step = { .i = 1 } },
        [LVL_NOTCHF] = { .min = { .i = 1 }, .max = { .i = 3000 }, .step = { .i = 10 } },
    },
    .ctcss_list =         common_ctcss_list,
    .dcs_list =           NULL,
    .preamp =             { 10, 17, RIG_DBLST_END, },
    .attenuator =         { 6, 12, 18, RIG_DBLST_END, },
    .max_rit =            Hz(9999),
    .max_xit =            Hz(9999),
    .max_ifshift =        Hz(1000),
    .agc_level_count =    5,
    .agc_levels =         { RIG_AGC_OFF, RIG_AGC_FAST, RIG_AGC_MEDIUM, RIG_AGC_SLOW, RIG_AGC_AUTO },
    .vfo_ops =            FT950_VFO_OPS,
    .scan_ops =           RIG_SCAN_VFO,
    .targetable_vfo =     RIG_TARGETABLE_FREQ,
    .transceive =         RIG_TRN_OFF,        /* May enable later as the 950 has an Auto Info command */
    .bank_qty =           0,
    .chan_desc_sz =       0,
    .rfpower_meter_cal =  FT950_RFPOWER_METER_CAL,
    .str_cal =            FT950_STR_CAL,
    .chan_list =          {
        {   1,  99, RIG_MTYPE_MEM,  NEWCAT_MEM_CAP },
        { 100, 117, RIG_MTYPE_EDGE, NEWCAT_MEM_CAP },    /* two by two */
        { 125, 128, RIG_MTYPE_BAND, NEWCAT_MEM_CAP },    /* 60M Channels U51-U54 or US1-US4, if available */
        { 130, 130, RIG_MTYPE_BAND, NEWCAT_MEM_CAP },    /* 60M Channel U55 or US5, if available */
        { 131, 131, RIG_MTYPE_BAND, NEWCAT_MEM_CAP },    /* EU5, 5167.5 KHz Alaska Emergency Freq, if available */

        RIG_CHAN_END,
    },

    .rx_range_list1 =     {
        {kHz(30), MHz(56), FT950_ALL_RX_MODES, -1, -1, FT950_VFO_ALL, FT950_ANTS},   /* General coverage + ham */
        RIG_FRNG_END,
    }, /* FIXME:  Are these the correct Region 1 values? */

    .tx_range_list1 =     {
        FRQ_RNG_HF(1, FT950_OTHER_TX_MODES, W(5), W(100), FT950_VFO_ALL, FT950_ANTS),
        FRQ_RNG_HF(1, FT950_AM_TX_MODES, W(2), W(25), FT950_VFO_ALL, FT950_ANTS),   /* AM class */
        FRQ_RNG_6m(1, FT950_OTHER_TX_MODES, W(5), W(100), FT950_VFO_ALL, FT950_ANTS),
        FRQ_RNG_6m(1, FT950_AM_TX_MODES, W(2), W(25), FT950_VFO_ALL, FT950_ANTS),   /* AM class */

        RIG_FRNG_END,
    },

    .rx_range_list2 =     {
        {kHz(30), MHz(56), FT950_ALL_RX_MODES, -1, -1, FT950_VFO_ALL, FT950_ANTS},
        RIG_FRNG_END,
    },

    .tx_range_list2 =     {
        FRQ_RNG_HF(2, FT950_OTHER_TX_MODES, W(5), W(100), FT950_VFO_ALL, FT950_ANTS),
        FRQ_RNG_HF(2, FT950_AM_TX_MODES, W(2), W(25), FT950_VFO_ALL, FT950_ANTS),   /* AM class */
        FRQ_RNG_6m(2, FT950_OTHER_TX_MODES, W(5), W(100), FT950_VFO_ALL, FT950_ANTS),
        FRQ_RNG_6m(2, FT950_AM_TX_MODES, W(2), W(25), FT950_VFO_ALL, FT950_ANTS),   /* AM class */

        RIG_FRNG_END,
    },

    .tuning_steps =       {
        {FT950_SSB_CW_RX_MODES, Hz(10)},    /* Normal */
        {FT950_SSB_CW_RX_MODES, Hz(100)},   /* Fast */

        {FT950_AM_RX_MODES,     Hz(100)},   /* Normal */
        {FT950_AM_RX_MODES,     kHz(1)},    /* Fast */

        {FT950_FM_RX_MODES,     Hz(100)},   /* Normal */
        {FT950_FM_RX_MODES,     kHz(1)},    /* Fast */

        RIG_TS_END,
    },

    /* mode/filter list, .remember =  order matters! */
    .filters =            {
        {FT950_CW_RTTY_PKT_RX_MODES,  Hz(1700)},    /* Normal CW, RTTY, PKT */
        {FT950_CW_RTTY_PKT_RX_MODES,  Hz(500)},     /* Narrow CW, RTTY, PKT */
        {FT950_CW_RTTY_PKT_RX_MODES,  Hz(2400)},    /* Wide   CW, RTTY, PKT */
        {FT950_CW_RTTY_PKT_RX_MODES,  Hz(2000)},    /*        CW, RTTY, PKT */
        {FT950_CW_RTTY_PKT_RX_MODES,  Hz(1400)},    /*        CW, RTTY, PKT */
        {FT950_CW_RTTY_PKT_RX_MODES,  Hz(1200)},    /*        CW, RTTY, PKT */
        {FT950_CW_RTTY_PKT_RX_MODES,  Hz(800)},     /*        CW, RTTY, PKT */
        {FT950_CW_RTTY_PKT_RX_MODES,  Hz(400)},     /*        CW, RTTY, PKT */
        {FT950_CW_RTTY_PKT_RX_MODES,  Hz(300)},     /*        CW, RTTY, PKT */
        {FT950_CW_RTTY_PKT_RX_MODES,  Hz(200)},     /*        CW, RTTY, PKT */
        {FT950_CW_RTTY_PKT_RX_MODES,  Hz(100)},     /*        CW, RTTY, PKT */
        {RIG_MODE_SSB,                Hz(2400)},    /* Normal SSB */
        {RIG_MODE_SSB,                Hz(1800)},    /* Narrow SSB */
        {RIG_MODE_SSB,                Hz(3000)},    /* Wide   SSB */
        {RIG_MODE_SSB,                Hz(2900)},    /*        SSB */
        {RIG_MODE_SSB,                Hz(2800)},    /*        SSB */
        {RIG_MODE_SSB,                Hz(2700)},    /*        SSB */
        {RIG_MODE_SSB,                Hz(2600)},    /*        SSB */
        {RIG_MODE_SSB,                Hz(2500)},    /*        SSB */
        {RIG_MODE_SSB,                Hz(2250)},    /*        SSB */
        {RIG_MODE_SSB,                Hz(2100)},    /*        SSB */
        {RIG_MODE_SSB,                Hz(1950)},    /*        SSB */
        {RIG_MODE_SSB,                Hz(1650)},    /*        SSB */
        {RIG_MODE_SSB,                Hz(1500)},    /*        SSB */
        {RIG_MODE_SSB,                Hz(1350)},    /*        SSB */
        {RIG_MODE_SSB,                Hz(1100)},    /*        SSB */
        {RIG_MODE_SSB,                Hz(850)},     /*        SSB */
        {RIG_MODE_SSB,                Hz(600)},     /*        SSB */
        {RIG_MODE_SSB,                Hz(400)},     /*        SSB */
        {RIG_MODE_SSB,                Hz(200)},     /*        SSB */
        {RIG_MODE_AM,                 Hz(9000)},    /* Normal AM */
        {RIG_MODE_AM,                 Hz(6000)},    /* Narrow AM */
        {FT950_FM_WIDE_RX_MODES,      Hz(16000)},   /* Normal FM */
        {FT950_FM_WIDE_RX_MODES,      Hz(9000)},    /* Narrow FM */
        {RIG_MODE_FMN,                Hz(9000)},    /* Narrow FM */
        {FT950_CW_RTTY_PKT_RX_MODES | RIG_MODE_SSB, RIG_FLT_ANY},

        RIG_FLT_END,
    },

    .ext_tokens =         ft950_ext_tokens,
    .extlevels =          ft950_ext_levels,

    .priv =               &ft950_priv_caps,

    .rig_init =           newcat_init,
    .rig_cleanup =        newcat_cleanup,
    .rig_open =           newcat_open,     /* port opened */
    .rig_close =          newcat_close,    /* port closed */

    .cfgparams =          newcat_cfg_params,
    .set_conf =           newcat_set_conf,
    .get_conf2 =          newcat_get_conf2,
    .set_freq =           newcat_set_freq,
    .get_freq =           newcat_get_freq,
    .set_mode =           newcat_set_mode,
    .get_mode =           newcat_get_mode,
    .set_vfo =            newcat_set_vfo,
    .get_vfo =            newcat_get_vfo,
    .set_ptt =            newcat_set_ptt,
    .get_ptt =            newcat_get_ptt,
    .set_split_vfo =      newcat_set_split_vfo,
    .get_split_vfo =      newcat_get_split_vfo,
    .set_rit =            newcat_set_rit,
    .get_rit =            newcat_get_rit,
    .set_xit =            newcat_set_xit,
    .get_xit =            newcat_get_xit,
    .set_ant =            newcat_set_ant,
    .get_ant =            newcat_get_ant,
    .get_func =           newcat_get_func,
    .set_func =           newcat_set_func,
    .get_level =          newcat_get_level,
    .set_level =          newcat_set_level,
    .get_mem =            newcat_get_mem,
    .set_mem =            newcat_set_mem,
    .vfo_op =             newcat_vfo_op,
    .get_info =           newcat_get_info,
    .power2mW =           newcat_power2mW,
    .mW2power =           newcat_mW2power,
    .set_rptr_shift =     newcat_set_rptr_shift,
    .get_rptr_shift =     newcat_get_rptr_shift,
    .set_rptr_offs =      newcat_set_rptr_offs,
    .get_rptr_offs =      newcat_get_rptr_offs,
    .set_ctcss_tone =     newcat_set_ctcss_tone,
    .get_ctcss_tone =     newcat_get_ctcss_tone,
    .set_ctcss_sql  =     newcat_set_ctcss_sql,
    .get_ctcss_sql  =     newcat_get_ctcss_sql,
    .set_powerstat =      newcat_set_powerstat,
    .get_powerstat =      newcat_get_powerstat,
    .set_ts =             newcat_set_ts,
    .get_ts =             newcat_get_ts,
    .set_trn =            newcat_set_trn,
    .get_trn =            newcat_get_trn,
    .set_channel =        newcat_set_channel,
    .get_channel =        newcat_get_channel,
    .set_ext_level =      newcat_set_ext_level,
    .get_ext_level =      newcat_get_ext_level,
    .send_morse =         newcat_send_morse,
    .wait_morse =         rig_wait_morse,
    .scan =               newcat_scan,
    .hamlib_check_rig_caps = HAMLIB_CHECK_RIG_CAPS
};
