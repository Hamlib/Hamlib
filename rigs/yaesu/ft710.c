/*
 * hamlib - (C) Frank Singleton 2000 (javabear at users.sourceforge.net)
 *
 * ft710.c - (C) Nate Bargmann 2007 (n0nb at arrl.net)
 *             (C) Stephane Fillod 2008-2010
 *             (C) Terry Embry 2008-2009
 *             (C) Mikael Nousiainen 2023
 *
 * This shared library provides an API for communicating
 * via serial interface to an FT-710 using the "CAT" interface
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

#include <stddef.h>

#include "hamlib/rig.h"
#include "bandplan.h"
#include "tones.h"
#include "newcat.h"
#include "yaesu.h"
#include "ft710.h"

const struct newcat_priv_caps ft710_priv_caps =
{
    .roofing_filter_count = 0,
    .roofing_filters = {}
};

const struct confparams ft710_ext_levels[] =
{
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

int ft710_ext_tokens[] =
{
    TOK_KEYER, TOK_APF_FREQ, TOK_APF_WIDTH,
    TOK_CONTOUR, TOK_CONTOUR_FREQ, TOK_CONTOUR_LEVEL, TOK_CONTOUR_WIDTH,
    TOK_BACKEND_NONE
};

struct rig_caps ft710_caps =
{
    RIG_MODEL(RIG_MODEL_FT710),
    .model_name =         "FT-710",
    .mfg_name =           "Yaesu",
    .version =            NEWCAT_VER ".7",
    .copyright =          "LGPL",
    .status =             RIG_STATUS_STABLE,
    .rig_type =           RIG_TYPE_TRANSCEIVER,
    .ptt_type =           RIG_PTT_RIG,
    .dcd_type =           RIG_DCD_NONE,
    .port_type =          RIG_PORT_SERIAL,
    .serial_rate_min =    4800,
    .serial_rate_max =    115200,
    .serial_data_bits =   8,
    .serial_stop_bits =   1,
    .serial_parity =      RIG_PARITY_NONE,
    .serial_handshake =   RIG_HANDSHAKE_NONE,
    .write_delay =        0,
    .post_write_delay =   20,
    .timeout =            1000,
    .retry =              3,
    .has_get_func =       FT710_FUNCS,
    .has_set_func =       FT710_FUNCS,
    .has_get_level =      FT710_LEVELS,
    .has_set_level =      RIG_LEVEL_SET(FT710_LEVELS),
    .has_get_parm =       RIG_PARM_BANDSELECT,
    .has_set_parm =       RIG_PARM_BANDSELECT,
    .level_gran =
    {
#define NO_LVL_MICGAIN
#define NO_LVL_SQL
#define NO_LVL_MONITOR_GAIN
#define NO_LVL_RFPOWER
#include "level_gran_yaesu.h"
#undef NO_LVL_MICGAIN
#undef NO_LVL_SQL
#undef NO_LVL_MONITOR_GAIN
#undef NO_LVL_RFPOWER
        [LVL_MICGAIN] = { .min = { .f = 0 }, .max = { .f = 1.0 }, .step = { .f = 1.0f / 100.0f } },
        [LVL_SQL] = { .min = { .f = 0 }, .max = { .f = 1.0 }, .step = { .f = 1.0f / 100.0f } },
        [LVL_MONITOR_GAIN] = { .min = { .f = 0 }, .max = { .f = 1.0 }, .step = { .f = 1.0f / 100.0f } },
        [LVL_RFPOWER] = { .min = { .f = .05 }, .max = { .f = 1.0 }, .step = { .f = 1.0f / 100.0f } },
    },
    .parm_gran =  {
        [PARM_BANDSELECT] = {.min = {.f = 0.0f}, .max = {.f = 1.0f}, .step = {.s = "BAND160M,BAND80M,BAND60M,BAND40M,BAND30M,BAND20M,BAND17M,BAND15M,BAND12M,BAND10M,BAND6M,BAND4M"}}
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
    .vfo_ops =            FT710_VFO_OPS,
    .scan_ops =           RIG_SCAN_VFO,
    .targetable_vfo =     RIG_TARGETABLE_FREQ,
    .transceive =         RIG_TRN_OFF, /* May enable later as the FT-710 has an Auto Info command */
    .bank_qty =           0,
    .chan_desc_sz =       0,
    .rfpower_meter_cal =  FT710_RFPOWER_METER_CAL,
    .swr_cal =            FT710_SWR_CAL,
    .str_cal =            FT710_STR_CAL,
    .id_meter_cal =       FT710_ID_CAL,
    .vd_meter_cal =       FT710_VD_CAL,
    .comp_meter_cal =     FT710_COMP_CAL,
    .chan_list =          {
        {   1,  99, RIG_MTYPE_MEM,  NEWCAT_MEM_CAP },
        {   1,  5,  RIG_MTYPE_MORSE },
        RIG_CHAN_END,
    },

    .rx_range_list1 =     {
        /* General coverage + ham, ANT_5 is RX only antenna */
        {kHz(30), MHz(60), FT710_ALL_RX_MODES, -1, -1, FT710_VFO_ALL, FT710_TX_ANTS, "USA"},
        RIG_FRNG_END,
    },

    .tx_range_list1 =     { /* the 101DX is 100W and the MP is 200W */
        FRQ_RNG_HF(1, FT710_OTHER_TX_MODES, W(5), W(200), FT710_VFO_ALL, FT710_TX_ANTS),
        FRQ_RNG_HF(1, FT710_AM_TX_MODES, W(2), W(75), FT710_VFO_ALL, FT710_TX_ANTS),   /* AM class */
        FRQ_RNG_6m(1, FT710_OTHER_TX_MODES, W(5), W(200), FT710_VFO_ALL, FT710_TX_ANTS),
        FRQ_RNG_6m(1, FT710_AM_TX_MODES, W(2), W(75), FT710_VFO_ALL, FT710_TX_ANTS),   /* AM class */

        RIG_FRNG_END,
    },

    .rx_range_list2 =     {
        {kHz(30), MHz(60), FT710_ALL_RX_MODES, -1, -1, FT710_VFO_ALL, FT710_TX_ANTS, "EUR"},
        RIG_FRNG_END,
    },

    .tx_range_list2 =     {
        FRQ_RNG_HF(2, FT710_OTHER_TX_MODES, W(5), W(200), FT710_VFO_ALL, FT710_TX_ANTS),
        FRQ_RNG_HF(2, FT710_AM_TX_MODES, W(2), W(75), FT710_VFO_ALL, FT710_TX_ANTS),   /* AM class */
        FRQ_RNG_6m(2, FT710_OTHER_TX_MODES, W(5), W(200), FT710_VFO_ALL, FT710_TX_ANTS),
        FRQ_RNG_6m(2, FT710_AM_TX_MODES, W(2), W(75), FT710_VFO_ALL, FT710_TX_ANTS),   /* AM class */
        FRQ_RNG_4m_REGION2(FT710_OTHER_TX_MODES, W(5), W(200), FT710_VFO_ALL, FT710_TX_ANTS),
        FRQ_RNG_4m_REGION2(FT710_AM_TX_MODES, W(2), W(75), FT710_VFO_ALL, FT710_TX_ANTS),   /* AM class */

        RIG_FRNG_END,
    },

    .tuning_steps =       {
        {FT710_SSB_CW_RX_MODES, Hz(10)},    /* Normal */
        {FT710_SSB_CW_RX_MODES, Hz(100)},   /* Fast */

        {FT710_AM_RX_MODES,     Hz(100)},   /* Normal */
        {FT710_AM_RX_MODES,     kHz(1)},    /* Fast */

        {FT710_FM_RX_MODES,     Hz(100)},   /* Normal */
        {FT710_FM_RX_MODES,     kHz(1)},    /* Fast */

        RIG_TS_END,

    },

    /* mode/filter list, remember that order matters! */
    .filters =            {
        {FT710_CW_RTTY_PKT_RX_MODES,  Hz(600)},   /* Normal CW, RTTY, PKT/USER */
        {FT710_CW_RTTY_PKT_RX_MODES,  Hz(300)},    /* Narrow CW, RTTY, PKT/USER */
        {FT710_CW_RTTY_PKT_RX_MODES,  Hz(2400)},   /* Wide   CW, RTTY, PKT/USER */
        {FT710_CW_RTTY_PKT_RX_MODES,  Hz(1200)},   /* Normal CW, RTTY, PKT/USER */
        {RIG_MODE_SSB,                 Hz(2400)},   /* Normal SSB */
        {RIG_MODE_SSB,                 Hz(1800)},   /* Narrow SSB */
        {RIG_MODE_SSB,                 Hz(3000)},   /* Wide   SSB */
        {RIG_MODE_AM,                  Hz(9000)},   /* Normal AM  */
        {RIG_MODE_AMN,                 Hz(6000)},   /* Narrow AM  */
        {RIG_MODE_FM | RIG_MODE_PKTFM, Hz(16000)},  /* Normal FM  */
        {RIG_MODE_FMN | RIG_MODE_PKTFMN, Hz(9000)},   /* Narrow FM  */
        {FT710_CW_RTTY_PKT_RX_MODES | RIG_MODE_SSB, RIG_FLT_ANY},

        RIG_FLT_END,
    },

    .ext_tokens =         ft710_ext_tokens,
    .extlevels =          ft710_ext_levels,

    .priv =               &ft710_priv_caps,

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
    .set_rit =            newcat_set_clarifier_frequency,
    .get_rit =            newcat_get_clarifier_frequency,
    .set_xit =            newcat_set_clarifier_frequency,
    .get_xit =            newcat_get_clarifier_frequency,
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
    .morse_qsize =        50,
    .hamlib_check_rig_caps = HAMLIB_CHECK_RIG_CAPS
};
