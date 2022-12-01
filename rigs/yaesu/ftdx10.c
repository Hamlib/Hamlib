/*
 * hamlib - (C) Frank Singleton 2000 (javabear at users.sourceforge.net)
 *
 * ftdx10.c - (C) Nate Bargmann 2007 (n0nb at arrl.net)
 *             (C) Stephane Fillod 2008-2010
 *             (C) Terry Embry 2008-2009
 *             (C) Mikael Nousiainen 2020
 *
 * This shared library provides an API for communicating
 * via serial interface to an FTDX10(D/MP) using the "CAT" interface
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
#include "tones.h"
#include "newcat.h"
#include "yaesu.h"
#include "ftdx10.h"

const struct newcat_priv_caps ftdx10_priv_caps =
{
    .roofing_filter_count = 6,
    .roofing_filters =
    {
        // The index must match ext level combo index
        { .index = 0, .set_value = '0', .get_value = 0, .width = 12000, .optional = 0 },
        { .index = 1, .set_value = '1', .get_value = '6', .width = 12000, .optional = 0 },
        { .index = 2, .set_value = '2', .get_value = '7', .width = 3000, .optional = 0 },
//        { .index = 3, .set_value = '3', .get_value = '8', .width = 1200, .optional = 1 },
        { .index = 4, .set_value = '4', .get_value = '9', .width = 500, .optional = 0 },
        { .index = 5, .set_value = '5', .get_value = 'A', .width = 300, .optional = 0 },
    }
};

const struct confparams ftdx10_ext_levels[] =
{
    {
        TOK_ROOFING_FILTER,
        "ROOFINGFILTER",
        "Roofing filter",
        "Roofing filter",
        NULL,
        RIG_CONF_COMBO,
        { .c = { .combostr = { "AUTO", "12 kHz", "3 kHz", "500 Hz", "300 Hz (optional)", NULL } } }
    },
    {
        TOK_KEYER,
        "KEYER",
        "Keyer",
        "Keyer on/off",
        NULL,
        RIG_CONF_CHECKBUTTON,
    },
    {
        TOK_APF_FREQ,
        "APF_FREQ",
        "APF frequency",
        "Audio peak filter frequency",
        NULL,
        RIG_CONF_NUMERIC,
        { .n = { .min = -250, .max = 250, .step = 10 } },
    },
    {
        TOK_APF_WIDTH,
        "APF_WIDTH",
        "APF width",
        "Audio peak filter width",
        NULL,
        RIG_CONF_COMBO,
        { .c = { .combostr = { "Narrow", "Medium", "Wide", NULL } } },
    },
    {
        TOK_CONTOUR,
        "CONTOUR",
        "Contour",
        "Contour on/off",
        NULL,
        RIG_CONF_CHECKBUTTON,
    },
    {
        TOK_CONTOUR_FREQ,
        "CONTOUR_FREQ",
        "Contour frequency",
        "Contour frequency",
        NULL,
        RIG_CONF_NUMERIC,
        { .n = { .min = 10, .max = 3200, .step = 1 } },
    },
    {
        TOK_CONTOUR_LEVEL,
        "CONTOUR_LEVEL",
        "Contour level",
        "Contour level (dB)",
        NULL,
        RIG_CONF_NUMERIC,
        { .n = { .min = -40, .max = 20, .step = 1 } },
    },
    {
        TOK_CONTOUR_WIDTH,
        "CONTOUR_WIDTH",
        "Contour width",
        "Contour width",
        NULL,
        RIG_CONF_NUMERIC,
        { .n = { .min = 1, .max = 11, .step = 1 } },
    },
    { RIG_CONF_END, NULL, }
};

int ftdx10_ext_tokens[] =
{
    TOK_ROOFING_FILTER, TOK_KEYER, TOK_APF_FREQ, TOK_APF_WIDTH,
    TOK_CONTOUR, TOK_CONTOUR_FREQ, TOK_CONTOUR_LEVEL, TOK_CONTOUR_WIDTH,
    TOK_BACKEND_NONE
};

const struct rig_caps ftdx10_caps =
{
    RIG_MODEL(RIG_MODEL_FTDX10),
    .model_name =         "FTDX-10",
    .mfg_name =           "Yaesu",
    .version =            NEWCAT_VER ".5",
    .copyright =          "LGPL",
    .status =             RIG_STATUS_STABLE,
    .rig_type =           RIG_TYPE_TRANSCEIVER,
    .ptt_type =           RIG_PTT_RIG,
    .dcd_type =           RIG_DCD_NONE,
    .port_type =          RIG_PORT_SERIAL,
    .serial_rate_min =    4800,
    .serial_rate_max =    38400,
    .serial_data_bits =   8,
    .serial_stop_bits =   2,
    .serial_parity =      RIG_PARITY_NONE,
    .serial_handshake =   RIG_HANDSHAKE_HARDWARE,
    .write_delay =        FTDX10_WRITE_DELAY,
    .post_write_delay =   FTDX10_POST_WRITE_DELAY,
    .timeout =            2000,
    .retry =              3,
    .has_get_func =       FTDX10_FUNCS,
    .has_set_func =       FTDX10_FUNCS,
    .has_get_level =      FTDX10_LEVELS,
    .has_set_level =      RIG_LEVEL_SET(FTDX10_LEVELS),
    .has_get_parm =       RIG_PARM_NONE,
    .has_set_parm =       RIG_PARM_NONE,
    .level_gran =
    {
#include "level_gran_yaesu.h"
        [LVL_RAWSTR] = { .min = { .i = 0 }, .max = { .i = 255 } },
        [LVL_CWPITCH] = { .min = { .i = 300 }, .max = { .i = 1050 }, .step = { .i = 10 } },
        [LVL_KEYSPD] = { .min = { .i = 4 }, .max = { .i = 60 }, .step = { .i = 1 } },
        [LVL_NOTCHF] = { .min = { .i = 1 }, .max = { .i = 3200 }, .step = { .i = 10 } },
    },
    .ctcss_list =         common_ctcss_list,
    .dcs_list =           NULL,
    .preamp =             { 10, 20, RIG_DBLST_END, },
    .attenuator =         { 6, 12, 18, RIG_DBLST_END, },
    .max_rit =            Hz(9999),
    .max_xit =            Hz(9999),
    .max_ifshift =        Hz(1200),
    .agc_level_count =    5,
    .agc_levels =         { RIG_AGC_OFF, RIG_AGC_FAST, RIG_AGC_MEDIUM, RIG_AGC_SLOW, RIG_AGC_AUTO },
    .vfo_ops =            FTDX10_VFO_OPS,
    .scan_ops =           RIG_SCAN_VFO,
    .targetable_vfo =     RIG_TARGETABLE_FREQ | RIG_TARGETABLE_MODE | RIG_TARGETABLE_FUNC | RIG_TARGETABLE_LEVEL | RIG_TARGETABLE_COMMON | RIG_TARGETABLE_TONE,
    .transceive =         RIG_TRN_OFF, /* May enable later as the FTDX10 has an Auto Info command */
    .bank_qty =           0,
    .chan_desc_sz =       0,
    .rfpower_meter_cal =  FTDX10_RFPOWER_METER_CAL,
    .swr_cal =            FTDX10_SWR_CAL,
    .chan_list =          {
        {   1,  99, RIG_MTYPE_MEM,  NEWCAT_MEM_CAP },
        RIG_CHAN_END,
    },

    .rx_range_list1 =     {
        /* General coverage + ham, ANT_5 is RX only antenna */
        {kHz(30), MHz(60), FTDX10_ALL_RX_MODES, -1, -1, FTDX10_VFO_ALL, FTDX10_TX_ANTS, "USA"},
        RIG_FRNG_END,
    },

    .tx_range_list1 =     { /* the 101DX is 100W and the MP is 200W */
        FRQ_RNG_HF(1, FTDX10_OTHER_TX_MODES, W(5), W(200), FTDX10_VFO_ALL, FTDX10_TX_ANTS),
        FRQ_RNG_HF(1, FTDX10_AM_TX_MODES, W(2), W(75), FTDX10_VFO_ALL, FTDX10_TX_ANTS),   /* AM class */
        FRQ_RNG_6m(1, FTDX10_OTHER_TX_MODES, W(5), W(200), FTDX10_VFO_ALL, FTDX10_TX_ANTS),
        FRQ_RNG_6m(1, FTDX10_AM_TX_MODES, W(2), W(75), FTDX10_VFO_ALL, FTDX10_TX_ANTS),   /* AM class */

        RIG_FRNG_END,
    },

    .rx_range_list2 =     {
        {kHz(30), MHz(60), FTDX10_ALL_RX_MODES, -1, -1, FTDX10_VFO_ALL, FTDX10_TX_ANTS, "EUR"},
        RIG_FRNG_END,
    },

    .tx_range_list2 =     {
        FRQ_RNG_HF(2, FTDX10_OTHER_TX_MODES, W(5), W(200), FTDX10_VFO_ALL, FTDX10_TX_ANTS),
        FRQ_RNG_HF(2, FTDX10_AM_TX_MODES, W(2), W(75), FTDX10_VFO_ALL, FTDX10_TX_ANTS),   /* AM class */
        FRQ_RNG_6m(2, FTDX10_OTHER_TX_MODES, W(5), W(200), FTDX10_VFO_ALL, FTDX10_TX_ANTS),
        FRQ_RNG_6m(2, FTDX10_AM_TX_MODES, W(2), W(75), FTDX10_VFO_ALL, FTDX10_TX_ANTS),   /* AM class */
        FRQ_RNG_4m_REGION2(FTDX10_OTHER_TX_MODES, W(5), W(200), FTDX10_VFO_ALL, FTDX10_TX_ANTS),
        FRQ_RNG_4m_REGION2(FTDX10_AM_TX_MODES, W(2), W(75), FTDX10_VFO_ALL, FTDX10_TX_ANTS),   /* AM class */

        RIG_FRNG_END,
    },

    .tuning_steps =       {
        {FTDX10_SSB_CW_RX_MODES, Hz(10)},    /* Normal */
        {FTDX10_SSB_CW_RX_MODES, Hz(100)},   /* Fast */

        {FTDX10_AM_RX_MODES,     Hz(100)},   /* Normal */
        {FTDX10_AM_RX_MODES,     kHz(1)},    /* Fast */

        {FTDX10_FM_RX_MODES,     Hz(100)},   /* Normal */
        {FTDX10_FM_RX_MODES,     kHz(1)},    /* Fast */

        RIG_TS_END,

    },

    /* mode/filter list, remember that order matters! */
    .filters =            {
        {FTDX10_CW_RTTY_PKT_RX_MODES,  Hz(600)},   /* Normal CW, RTTY, PKT/USER */
        {FTDX10_CW_RTTY_PKT_RX_MODES,  Hz(300)},    /* Narrow CW, RTTY, PKT/USER */
        {FTDX10_CW_RTTY_PKT_RX_MODES,  Hz(2400)},   /* Wide   CW, RTTY, PKT/USER */
        {FTDX10_CW_RTTY_PKT_RX_MODES,  Hz(1200)},   /* Normal CW, RTTY, PKT/USER */
        {RIG_MODE_SSB,                 Hz(2400)},   /* Normal SSB */
        {RIG_MODE_SSB,                 Hz(1800)},   /* Narrow SSB */
        {RIG_MODE_SSB,                 Hz(3000)},   /* Wide   SSB */
        {RIG_MODE_AM,                  Hz(9000)},   /* Normal AM  */
        {RIG_MODE_AMN,                 Hz(6000)},   /* Narrow AM  */
        {RIG_MODE_FM | RIG_MODE_PKTFM, Hz(16000)},  /* Normal FM  */
        {RIG_MODE_FMN | RIG_MODE_PKTFMN, Hz(9000)},   /* Narrow FM  */
        {FTDX10_CW_RTTY_PKT_RX_MODES | RIG_MODE_SSB, RIG_FLT_ANY},

        RIG_FLT_END,
    },

    .ext_tokens =         ftdx10_ext_tokens,
    .extlevels =          ftdx10_ext_levels,

    .priv =               &ftdx10_priv_caps,

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
    .set_ant =            NULL,
    .get_ant =            NULL,
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
    .get_ts =             newcat_get_ts,
    .set_ts =             newcat_set_ts,
    .set_trn =            newcat_set_trn,
    .get_trn =            newcat_get_trn,
    .set_channel =        newcat_set_channel,
    .get_channel =        newcat_get_channel,
    .set_ext_level =      newcat_set_ext_level,
    .get_ext_level =      newcat_get_ext_level,
    .send_morse =         newcat_send_morse,
    .wait_morse =         rig_wait_morse,
    .set_clock =          newcat_set_clock,
    .get_clock =          newcat_get_clock,
    .scan =               newcat_scan,
    .hamlib_check_rig_caps = HAMLIB_CHECK_RIG_CAPS
};
