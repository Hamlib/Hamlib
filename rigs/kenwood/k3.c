/*
 *  Hamlib Kenwood backend - Elecraft K3 description
 *  Copyright (c) 2002-2009 by Stephane Fillod
 *  Copyright (C) 2010,2011,2012,2013 by Nate Bargmann, n0nb@arrl.net
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
 *  See the file 'COPYING.LIB' in the main Hamlib distribution directory for
 *  the complete text of the GNU Lesser Public License version 2.1.
 *
 */

#include <hamlib/config.h>

#include <stdlib.h>
#include <math.h>

#include <hamlib/rig.h>
#include "idx_builtin.h"
#include "kenwood.h"
#include "bandplan.h"
#include "elecraft.h"
#include "token.h"
#include "cal.h"
#include "iofunc.h"

#define K3_MODES (RIG_MODE_CW|RIG_MODE_CWR|RIG_MODE_SSB|\
    RIG_MODE_RTTY|RIG_MODE_RTTYR|RIG_MODE_FM|RIG_MODE_AM|RIG_MODE_PKTUSB|\
    RIG_MODE_PKTLSB)

#define K3_FUNC_ALL (RIG_FUNC_NB|RIG_FUNC_VOX|RIG_FUNC_APF|\
    RIG_FUNC_DUAL_WATCH|RIG_FUNC_DIVERSITY|\
    RIG_FUNC_LOCK|RIG_FUNC_RIT|RIG_FUNC_XIT|RIG_FUNC_SEND_MORSE)

#define K4_FUNC_ALL (K3_FUNC_ALL|RIG_FUNC_MUTE)

#define K3_LEVEL_ALL (RIG_LEVEL_ATT|RIG_LEVEL_PREAMP|RIG_LEVEL_AGC|RIG_LEVEL_SQL|\
    RIG_LEVEL_STRENGTH|RIG_LEVEL_ALC|RIG_LEVEL_RFPOWER|RIG_LEVEL_KEYSPD|\
    RIG_LEVEL_AF|RIG_LEVEL_RF|RIG_LEVEL_MICGAIN|RIG_LEVEL_COMP|\
    RIG_LEVEL_NR|RIG_LEVEL_MONITOR_GAIN|RIG_LEVEL_RAWSTR|RIG_LEVEL_RFPOWER_METER|RIG_LEVEL_RFPOWER_METER_WATTS)

#define K3_VFO (RIG_VFO_A|RIG_VFO_B)
#define K3_VFO_OP (RIG_OP_UP|RIG_OP_DOWN)

#define K3_ANTS (RIG_ANT_1|RIG_ANT_2)
#define K4_ANTS (RIG_ANT_1|RIG_ANT_2|RIG_ANT_3|RIG_ANT_4)

#define KX3_FUNC_ALL (RIG_FUNC_NB|RIG_FUNC_VOX|RIG_FUNC_APF|\
    RIG_FUNC_DUAL_WATCH|RIG_FUNC_LOCK|RIG_FUNC_RIT|RIG_FUNC_XIT)

#define KX3_LEVEL_ALL (RIG_LEVEL_ATT|RIG_LEVEL_PREAMP|RIG_LEVEL_AGC|RIG_LEVEL_SQL|\
    RIG_LEVEL_STRENGTH|RIG_LEVEL_RFPOWER|RIG_LEVEL_KEYSPD|\
    RIG_LEVEL_AF|RIG_LEVEL_RF|RIG_LEVEL_MICGAIN|RIG_LEVEL_COMP|\
    RIG_LEVEL_NR|RIG_LEVEL_MONITOR_GAIN|RIG_LEVEL_RAWSTR|RIG_LEVEL_RFPOWER_METER|RIG_LEVEL_RFPOWER_METER_WATTS)

/*
 * Elecraft K3/K3S extra level definitions
 *
 * Token definitions for .cfgparams in rig_caps
 * See enum rig_conf_e and struct confparams in rig.h
 */
const struct confparams k3_ext_levels[] =
{
    {
        TOK_IF_FREQ, "ifctr", "IF freq", "IF center frequency",
        NULL, RIG_CONF_NUMERIC, { .n = { 0, 9990, 10 } }
    },
    {
        TOK_TX_STAT, "txst", "TX status", "TX status",
        NULL, RIG_CONF_CHECKBUTTON, { { } },
    },
    {
        TOK_RIT_CLR, "ritclr", "RIT clear", "RIT clear",
        NULL, RIG_CONF_BUTTON, { { } },
    },
    {
        TOK_ESSB, "essb", "ESSB", "Extended SSB frequency response",
        NULL, RIG_CONF_CHECKBUTTON, { { } },
    },
    {
        TOK_RX_ANT, "rx_ant", "RX ANT", "RX antenna",
        NULL, RIG_CONF_CHECKBUTTON, { { } },
    },
    {
        TOK_LINK_VFOS, "link_vfos", "Link VFOs", "Link VFOs",
        NULL, RIG_CONF_CHECKBUTTON, { { } },
    },
    {
        TOK_TX_METER, "tx_meter", "TX meter", "Transmit meter mode",
        NULL, RIG_CONF_COMBO, { .c = { .combostr = { "SWR", "ALC", NULL } } }
    },
    {
        TOK_IF_NB, "if_nb", "IF NB", "IF noise blanker level",
        NULL, RIG_CONF_NUMERIC, { .n = { 0, 21, 1 } },
    },
    { RIG_CONF_END, NULL, }
};

/*
 * Elecraft KX3/KX2 extra level definitions
 *
 * Token definitions for .cfgparams in rig_caps
 * See enum rig_conf_e and struct confparams in rig.h
 */
const struct confparams kx3_ext_levels[] =
{
    {
        TOK_TX_STAT, "txst", "TX status", "TX status",
        NULL, RIG_CONF_CHECKBUTTON, { { } },
    },
    {
        TOK_RIT_CLR, "ritclr", "RIT clear", "RIT clear",
        NULL, RIG_CONF_BUTTON, { { } },
    },
    {
        TOK_ESSB, "essb", "ESSB", "Extended SSB frequency response",
        NULL, RIG_CONF_CHECKBUTTON, { { } },
    },
    { RIG_CONF_END, NULL, }
};

/* kenwood_transaction() will add this to command strings
 * sent to the rig and remove it from strings returned from
 * the rig, so no need to append ';' manually to command strings.
 */
static struct kenwood_priv_caps k3_priv_caps  =
{
    .cmdtrm =  EOM_KEN,
};


/* K3 specific function declarations */
int k3_set_freq(RIG *rig, vfo_t vfo, freq_t freq);
int k3_get_mode(RIG *rig, vfo_t vfo, rmode_t *mode, pbwidth_t *width);
int k3_set_mode(RIG *rig, vfo_t vfo, rmode_t mode, pbwidth_t width);
int k3_get_vfo(RIG *rig, vfo_t *vfo);
int k3_set_vfo(RIG *rig, vfo_t vfo);
int k3_set_ext_level(RIG *rig, vfo_t vfo, token_t token, value_t val);
int k3_get_ext_level(RIG *rig, vfo_t vfo, token_t token, value_t *val);
int k3_set_rit(RIG *rig, vfo_t vfo, shortfreq_t rit);
int k3_set_xit(RIG *rig, vfo_t vfo, shortfreq_t rit);
int k3_set_split_mode(RIG *rig, vfo_t vfo, rmode_t tx_mode, pbwidth_t tx_width);
int k3_get_split_mode(RIG *rig, vfo_t vfo, rmode_t *tx_mode,
                      pbwidth_t *tx_width);
int k3_set_level(RIG *rig, vfo_t vfo, setting_t level, value_t val);
int k3_get_level(RIG *rig, vfo_t vfo, setting_t level, value_t *val);
int kx3_set_level(RIG *rig, vfo_t vfo, setting_t level, value_t val);
int kx3_get_level(RIG *rig, vfo_t vfo, setting_t level, value_t *val);
int k3_set_func(RIG *rig, vfo_t vfo, setting_t func, int status);
int k3_get_func(RIG *rig, vfo_t vfo, setting_t func, int *status);
int k3_power2mW(RIG *rig, unsigned int *mwpower, float power, freq_t freq,
                rmode_t mode);

/* Private helper functions */
int set_rit_xit(RIG *rig, shortfreq_t rit);
int k3_set_nb_level(RIG *rig, float dsp_nb, float if_nb);
int k3_get_nb_level(RIG *rig, float *dsp_nb, float *if_nb);
int k3_get_bar_graph_level(RIG *rig, float *smeter, float *pwr, float *alc,
                           int *mode_tx);
int kx3_get_bar_graph_level(RIG *rig, float *level);

/* K4 functions */
int k4_get_ptt(RIG *rig, vfo_t vfo, ptt_t *ptt);
int k4_set_ptt(RIG *rig, vfo_t vfo, ptt_t ptt);

/*
 * K3 rig capabilities.
 * This kit can recognize a large subset of TS-570/K2 commands and has many
 * extensions of its own.  Extension backend functions to standard Kenwood
 * command are defined in elecraft.c (shared with K2) and in this file.
 *
 * Part of info comes from http://www.elecraft.com/K2_Manual_Download_Page.htm#K3
 * look for K3 Programmer's Reference PDF
 *
 */
const struct rig_caps k3_caps =
{
    RIG_MODEL(RIG_MODEL_K3),
    .model_name =       "K3",
    .mfg_name =     "Elecraft",
    .version =      BACKEND_VER ".25",
    .copyright =        "LGPL",
    .status =       RIG_STATUS_STABLE,
    .rig_type =     RIG_TYPE_TRANSCEIVER,
    .ptt_type =     RIG_PTT_RIG,
    .dcd_type =     RIG_DCD_RIG,
    .port_type =        RIG_PORT_SERIAL,
    .serial_rate_min =  4800,
    .serial_rate_max =  38400,
    .serial_data_bits = 8,
    .serial_stop_bits = 1,
    .serial_parity =    RIG_PARITY_NONE,
    .serial_handshake = RIG_HANDSHAKE_NONE,
    .write_delay =      0,  /* Timing between bytes */
    .post_write_delay = 10,  /* Timing between command strings */
    .timeout =      1000,   /* FA and FB make take up to 500 ms on band change */
    .retry =        5,

    .has_get_func =     K3_FUNC_ALL,
    .has_set_func =     K3_FUNC_ALL,
    .has_get_level =    K3_LEVEL_ALL,
    .has_set_level =    RIG_LEVEL_SET(K3_LEVEL_ALL),
    .has_get_parm =     RIG_PARM_NONE,
    .has_set_parm =     RIG_PARM_NONE,  /* FIXME: parms */
    .level_gran = {
        // cppcheck-suppress *
        [LVL_KEYSPD] = { .min = { .i = 8 }, .max = { .i = 50 }, .step = { .i = 1 } },
    },
    .parm_gran =        {},
    .extlevels =        k3_ext_levels,
    .extparms =     kenwood_cfg_params,
    .preamp =       { 1, RIG_DBLST_END, },
    .attenuator =       { 10, RIG_DBLST_END, },
    .max_rit =      Hz(9990),
    .max_xit =      Hz(9990),
    .max_ifshift =      Hz(0),
    .vfo_ops =      K3_VFO_OP,
    .targetable_vfo =   RIG_TARGETABLE_FREQ | RIG_TARGETABLE_MODE,
    .transceive =       RIG_TRN_RIG,
    .bank_qty =     0,
    .chan_desc_sz =     0,

    .chan_list =        { RIG_CHAN_END },

    .rx_range_list1 =  {
        {kHz(500), MHz(30), K3_MODES, -1, -1, K3_VFO, K3_ANTS},
        { MHz(48), MHz(54), K3_MODES, -1, - 1, K3_VFO, K3_ANTS},
        RIG_FRNG_END,
    }, /* rx range */
    .tx_range_list1 =  {
        FRQ_RNG_HF(1, K3_MODES, mW(10), W(10), K3_VFO, K3_ANTS),
        FRQ_RNG_6m(1, K3_MODES, mW(10), W(10), K3_VFO, K3_ANTS),
        RIG_FRNG_END,
    }, /* tx range */

    .rx_range_list2 =  {
        {kHz(500), MHz(30), K3_MODES, -1, -1, K3_VFO, K3_ANTS},
        { MHz(48), MHz(54), K3_MODES, -1, -1, K3_VFO, K3_ANTS},
        RIG_FRNG_END,
    }, /* rx range */
    .tx_range_list2 =  {
        FRQ_RNG_HF(2, K3_MODES, mW(10), W(10), K3_VFO, K3_ANTS),
        FRQ_RNG_6m(2, K3_MODES, mW(10), W(10), K3_VFO, K3_ANTS),
        RIG_FRNG_END,
    }, /* tx range */
    .tuning_steps =  {
        {K3_MODES, 1},
        RIG_TS_END,
    },

    /* mode/filter list, remember: order matters! */
    /* Values are arbitrary based on common K3 filter options. */
    .filters =  {
        {RIG_MODE_SSB, kHz(2.7)},
        {RIG_MODE_SSB, kHz(2.8)},
        {RIG_MODE_SSB, kHz(1.8)},
        {RIG_MODE_SSB, kHz(2.4)},
        {RIG_MODE_SSB, RIG_FLT_ANY},
        {RIG_MODE_CW | RIG_MODE_CWR, kHz(1)},
        {RIG_MODE_CW | RIG_MODE_CWR, kHz(2.8)},
        {RIG_MODE_CW | RIG_MODE_CWR, Hz(50)},
        {RIG_MODE_CW | RIG_MODE_CWR, Hz(500)},
        {RIG_MODE_CW | RIG_MODE_CWR, Hz(300)},
        {RIG_MODE_CW | RIG_MODE_CWR, RIG_FLT_ANY},
        {RIG_MODE_RTTY | RIG_MODE_RTTYR, kHz(2)},
        {RIG_MODE_RTTY | RIG_MODE_RTTYR, kHz(2.7)},
        {RIG_MODE_RTTY | RIG_MODE_RTTYR, Hz(500)},
        {RIG_MODE_RTTY | RIG_MODE_RTTYR, Hz(300)},
        {RIG_MODE_RTTY | RIG_MODE_RTTYR, RIG_FLT_ANY},
        {RIG_MODE_PKTUSB | RIG_MODE_PKTLSB, kHz(2.7)},
        {RIG_MODE_PKTUSB | RIG_MODE_PKTLSB, kHz(2.8)},
        {RIG_MODE_PKTUSB | RIG_MODE_PKTLSB, Hz(50)},
        {RIG_MODE_PKTUSB | RIG_MODE_PKTLSB, Hz(2400)},
        {RIG_MODE_PKTUSB | RIG_MODE_PKTLSB, Hz(500)},
        {RIG_MODE_PKTUSB | RIG_MODE_PKTLSB, Hz(300)},
        {RIG_MODE_PKTUSB | RIG_MODE_PKTLSB, RIG_FLT_ANY},
        {RIG_MODE_AM, kHz(6)},
        {RIG_MODE_AM, kHz(13)},
        {RIG_MODE_AM, kHz(2.7)},
        {RIG_MODE_AM, RIG_FLT_ANY},
        {RIG_MODE_FM, kHz(13)}, /* TBC */
        RIG_FLT_END,
    },
    .priv = (void *)& k3_priv_caps,

    .rig_init =     kenwood_init,
    .rig_cleanup =      kenwood_cleanup,
    .rig_open =     elecraft_open,
    .rig_close =        kenwood_close,
    .set_freq =     k3_set_freq,
    .get_freq =     kenwood_get_freq,
    .set_mode =     k3_set_mode,
    .get_mode =     k3_get_mode,
    .set_vfo =      k3_set_vfo,
    .get_vfo =      k3_get_vfo,
    .set_split_mode =   k3_set_split_mode,
    .get_split_mode =   k3_get_split_mode,
    .set_split_vfo =    kenwood_set_split_vfo,
    .get_split_vfo =    kenwood_get_split_vfo_if,
    .set_rit =      k3_set_rit,
    .get_rit =      kenwood_get_rit,
    .set_xit =      k3_set_xit,
    .get_xit =      kenwood_get_xit,
    .get_ptt =      kenwood_get_ptt,
    .set_ptt =      kenwood_set_ptt,
    .get_dcd =      kenwood_get_dcd,
    .set_func =     k3_set_func,
    .get_func =     k3_get_func,
    .set_ext_parm =     kenwood_set_ext_parm,
    .get_ext_parm =     kenwood_get_ext_parm,
    .set_level =        k3_set_level,
    .get_level =        k3_get_level,
    .set_ext_level =    k3_set_ext_level,
    .get_ext_level =    k3_get_ext_level,
    .vfo_op =       kenwood_vfo_op,
    .set_trn =      kenwood_set_trn,
    .get_trn =      kenwood_get_trn,
    .set_powerstat =    kenwood_set_powerstat,
    .get_powerstat =    kenwood_get_powerstat,
    .set_ant =      kenwood_set_ant_no_ack,
    .get_ant =      kenwood_get_ant,
    .send_morse =       kenwood_send_morse,
    .wait_morse =       rig_wait_morse,
    .power2mW =     k3_power2mW,

    .hamlib_check_rig_caps = HAMLIB_CHECK_RIG_CAPS
};

const struct rig_caps k3s_caps =
{
    RIG_MODEL(RIG_MODEL_K3S),
    .model_name =       "K3S",
    .mfg_name =     "Elecraft",
    .version =      BACKEND_VER ".19",
    .copyright =        "LGPL",
    .status =       RIG_STATUS_STABLE,
    .rig_type =     RIG_TYPE_TRANSCEIVER,
    .ptt_type =     RIG_PTT_RIG,
    .dcd_type =     RIG_DCD_RIG,
    .port_type =        RIG_PORT_SERIAL,
    .serial_rate_min =  4800,
    .serial_rate_max =  38400,
    .serial_data_bits = 8,
    .serial_stop_bits = 1,
    .serial_parity =    RIG_PARITY_NONE,
    .serial_handshake = RIG_HANDSHAKE_NONE,
    .write_delay =      0,  /* Timing between bytes */
    .post_write_delay = 10,  /* Timing between command strings */
    .timeout =      1000,   /* FA and FB make take up to 500 ms on band change */
    .retry =        5,

    .has_get_func =     K3_FUNC_ALL,
    .has_set_func =     K3_FUNC_ALL,
    .has_get_level =    K3_LEVEL_ALL,
    .has_set_level =    RIG_LEVEL_SET(K3_LEVEL_ALL),
    .has_get_parm =     RIG_PARM_NONE,
    .has_set_parm =     RIG_PARM_NONE,  /* FIXME: parms */
    .level_gran = {
        [LVL_KEYSPD] = { .min = { .i = 8 }, .max = { .i = 50 }, .step = { .i = 1 } },
    },
    .parm_gran =        {},
    .extlevels =        k3_ext_levels,
    .extparms =     kenwood_cfg_params,
    .preamp =       { 1, RIG_DBLST_END, },
    .attenuator =       { 5, 10, 15, RIG_DBLST_END, },
    .max_rit =      Hz(9990),
    .max_xit =      Hz(9990),
    .max_ifshift =      Hz(0),
    .vfo_ops =      K3_VFO_OP,
    .targetable_vfo =   RIG_TARGETABLE_FREQ | RIG_TARGETABLE_MODE,
    .transceive =       RIG_TRN_RIG,
    .bank_qty =     0,
    .chan_desc_sz =     0,

    .chan_list =        { RIG_CHAN_END },

    .rx_range_list1 =  {
        {kHz(500), MHz(30), K3_MODES, -1, -1, K3_VFO, K3_ANTS},
        { MHz(48), MHz(54), K3_MODES, -1, - 1, K3_VFO, K3_ANTS},
        RIG_FRNG_END,
    }, /* rx range */
    .tx_range_list1 =  {
        FRQ_RNG_HF(1, K3_MODES, mW(10), W(10), K3_VFO, K3_ANTS),
        FRQ_RNG_6m(1, K3_MODES, mW(10), W(10), K3_VFO, K3_ANTS),
        RIG_FRNG_END,
    }, /* tx range */

    .rx_range_list2 =  {
        {kHz(500), MHz(30), K3_MODES, -1, -1, K3_VFO, K3_ANTS},
        { MHz(48), MHz(54), K3_MODES, -1, -1, K3_VFO, K3_ANTS},
        RIG_FRNG_END,
    }, /* rx range */
    .tx_range_list2 =  {
        FRQ_RNG_HF(2, K3_MODES, mW(10), W(10), K3_VFO, K3_ANTS),
        FRQ_RNG_6m(2, K3_MODES, mW(10), W(10), K3_VFO, K3_ANTS),
        RIG_FRNG_END,
    }, /* tx range */
    .tuning_steps =  {
        {K3_MODES, 1},
        RIG_TS_END,
    },

    /* mode/filter list, remember: order matters! */
    /* Values are arbitrary based on common K3 filter options. */
    .filters =  {
        {RIG_MODE_SSB, kHz(2.7)},
        {RIG_MODE_SSB, kHz(2.8)},
        {RIG_MODE_SSB, kHz(1.8)},
        {RIG_MODE_SSB, kHz(2.4)},
        {RIG_MODE_SSB, RIG_FLT_ANY},
        {RIG_MODE_CW | RIG_MODE_CWR, kHz(1)},
        {RIG_MODE_CW | RIG_MODE_CWR, kHz(2.8)},
        {RIG_MODE_CW | RIG_MODE_CWR, Hz(50)},
        {RIG_MODE_CW | RIG_MODE_CWR, Hz(500)},
        {RIG_MODE_CW | RIG_MODE_CWR, Hz(300)},
        {RIG_MODE_CW | RIG_MODE_CWR, RIG_FLT_ANY},
        {RIG_MODE_RTTY | RIG_MODE_RTTYR, kHz(2)},
        {RIG_MODE_RTTY | RIG_MODE_RTTYR, kHz(2.7)},
        {RIG_MODE_RTTY | RIG_MODE_RTTYR, Hz(500)},
        {RIG_MODE_RTTY | RIG_MODE_RTTYR, Hz(300)},
        {RIG_MODE_RTTY | RIG_MODE_RTTYR, RIG_FLT_ANY},
        {RIG_MODE_PKTUSB | RIG_MODE_PKTLSB, kHz(2.7)},
        {RIG_MODE_PKTUSB | RIG_MODE_PKTLSB, kHz(2.8)},
        {RIG_MODE_PKTUSB | RIG_MODE_PKTLSB, Hz(50)},
        {RIG_MODE_PKTUSB | RIG_MODE_PKTLSB, Hz(2400)},
        {RIG_MODE_PKTUSB | RIG_MODE_PKTLSB, Hz(500)},
        {RIG_MODE_PKTUSB | RIG_MODE_PKTLSB, Hz(300)},
        {RIG_MODE_PKTUSB | RIG_MODE_PKTLSB, RIG_FLT_ANY},
        {RIG_MODE_AM, kHz(6)},
        {RIG_MODE_AM, kHz(13)},
        {RIG_MODE_AM, kHz(2.7)},
        {RIG_MODE_AM, RIG_FLT_ANY},
        {RIG_MODE_FM, kHz(13)}, /* TBC */
        RIG_FLT_END,
    },
    .priv = (void *)& k3_priv_caps,

    .rig_init =     kenwood_init,
    .rig_cleanup =      kenwood_cleanup,
    .rig_open =     elecraft_open,
    .rig_close =        kenwood_close,
    .set_freq =     k3_set_freq,
    .get_freq =     kenwood_get_freq,
    .set_mode =     k3_set_mode,
    .get_mode =     k3_get_mode,
    .set_vfo =      k3_set_vfo,
    .get_vfo =      k3_get_vfo,
    .set_split_mode =   k3_set_split_mode,
    .get_split_mode =   k3_get_split_mode,
    .set_split_vfo =    kenwood_set_split_vfo,
    .get_split_vfo =    kenwood_get_split_vfo_if,
    .set_rit =      k3_set_rit,
    .get_rit =      kenwood_get_rit,
    .set_xit =      k3_set_xit,
    .get_xit =      kenwood_get_xit,
    .get_ptt =      kenwood_get_ptt,
    .set_ptt =      kenwood_set_ptt,
    .get_dcd =      kenwood_get_dcd,
    .set_func =     k3_set_func,
    .get_func =     k3_get_func,
    .set_ext_parm =     kenwood_set_ext_parm,
    .get_ext_parm =     kenwood_get_ext_parm,
    .set_level =        k3_set_level,
    .get_level =        k3_get_level,
    .set_ext_level =    k3_set_ext_level,
    .get_ext_level =    k3_get_ext_level,
    .vfo_op =       kenwood_vfo_op,
    .set_trn =      kenwood_set_trn,
    .get_trn =      kenwood_get_trn,
    .set_powerstat =    kenwood_set_powerstat,
    .get_powerstat =    kenwood_get_powerstat,
    .set_ant =      kenwood_set_ant_no_ack,
    .get_ant =      kenwood_get_ant,
    .send_morse =       kenwood_send_morse,
    .wait_morse =       rig_wait_morse,
    .power2mW =     k3_power2mW,
    .hamlib_check_rig_caps = HAMLIB_CHECK_RIG_CAPS
};

// How similar is this to the K3S?
const struct rig_caps k4_caps =
{
    RIG_MODEL(RIG_MODEL_K4),
    .model_name =       "K4",
    .mfg_name =     "Elecraft",
    .version =      BACKEND_VER ".25",
    .copyright =        "LGPL",
    .status =       RIG_STATUS_STABLE,
    .rig_type =     RIG_TYPE_TRANSCEIVER,
    .ptt_type =     RIG_PTT_RIG,
    .dcd_type =     RIG_DCD_RIG,
    .port_type =        RIG_PORT_SERIAL,
    .serial_rate_min =  4800,
    .serial_rate_max =  115200,
    .serial_data_bits = 8,
    .serial_stop_bits = 1,
    .serial_parity =    RIG_PARITY_NONE,
    .serial_handshake = RIG_HANDSHAKE_NONE,
    .write_delay =      0, /* Timing between bytes */
    .post_write_delay = 0,  /* Timing between command strings */
    .timeout =      1000,   /* FA and FB make take up to 500 ms on band change */
    .retry =        5,

    .has_get_func =     K4_FUNC_ALL,
    .has_set_func =     K4_FUNC_ALL,
    .has_get_level =    K3_LEVEL_ALL,
    .has_set_level =    RIG_LEVEL_SET(K3_LEVEL_ALL),
    .has_get_parm =     RIG_PARM_NONE,
    .has_set_parm =     RIG_PARM_NONE,  /* FIXME: parms */
    .level_gran = {
        [LVL_KEYSPD] = { .min = { .i = 8 }, .max = { .i = 50 }, .step = { .i = 1 } },
    },
    .parm_gran =        {},
    .extlevels =        k3_ext_levels,
    .extparms =     kenwood_cfg_params,
    .preamp =       { 1, RIG_DBLST_END, },
    .attenuator =       { 5, 10, 15, RIG_DBLST_END, },
    .max_rit =      Hz(9990),
    .max_xit =      Hz(9990),
    .max_ifshift =      Hz(0),
    .vfo_ops =      K3_VFO_OP,
    .targetable_vfo =   RIG_TARGETABLE_FREQ | RIG_TARGETABLE_MODE,
    .transceive =       RIG_TRN_RIG,
    .bank_qty =     0,
    .chan_desc_sz =     0,

    .chan_list =        { RIG_CHAN_END },

    .rx_range_list1 =  {
        {kHz(500), MHz(30), K3_MODES, -1, -1, K3_VFO, K4_ANTS},
        { MHz(48), MHz(54), K3_MODES, -1, - 1, K3_VFO, K4_ANTS},
        RIG_FRNG_END,
    }, /* rx range */
    .tx_range_list1 =  {
        FRQ_RNG_HF(1, K3_MODES, mW(10), W(100), K3_VFO, K4_ANTS),
        FRQ_RNG_6m(1, K3_MODES, mW(10), W(100), K3_VFO, K4_ANTS),
        RIG_FRNG_END,
    }, /* tx range */

    .rx_range_list2 =  {
        {kHz(500), MHz(30), K3_MODES, -1, -1, K3_VFO, K4_ANTS},
        { MHz(48), MHz(54), K3_MODES, -1, -1, K3_VFO, K4_ANTS},
        RIG_FRNG_END,
    }, /* rx range */
    .tx_range_list2 =  {
        FRQ_RNG_HF(2, K3_MODES, mW(10), W(100), K3_VFO, K4_ANTS),
        FRQ_RNG_6m(2, K3_MODES, mW(10), W(100), K3_VFO, K4_ANTS),
        RIG_FRNG_END,
    }, /* tx range */
    .tuning_steps =  {
        {K3_MODES, 1},
        RIG_TS_END,
    },

    /* mode/filter list, remember: order matters! */
    /* Values are arbitrary based on common K3 filter options. */
    .filters =  {
        {RIG_MODE_SSB, kHz(2.7)},
        {RIG_MODE_SSB, kHz(2.8)},
        {RIG_MODE_SSB, kHz(1.8)},
        {RIG_MODE_SSB, kHz(2.4)},
        {RIG_MODE_SSB, RIG_FLT_ANY},
        {RIG_MODE_CW | RIG_MODE_CWR, kHz(1)},
        {RIG_MODE_CW | RIG_MODE_CWR, kHz(2.8)},
        {RIG_MODE_CW | RIG_MODE_CWR, Hz(50)},
        {RIG_MODE_CW | RIG_MODE_CWR, Hz(500)},
        {RIG_MODE_CW | RIG_MODE_CWR, Hz(300)},
        {RIG_MODE_CW | RIG_MODE_CWR, RIG_FLT_ANY},
        {RIG_MODE_RTTY | RIG_MODE_RTTYR, kHz(2)},
        {RIG_MODE_RTTY | RIG_MODE_RTTYR, kHz(2.7)},
        {RIG_MODE_RTTY | RIG_MODE_RTTYR, Hz(500)},
        {RIG_MODE_RTTY | RIG_MODE_RTTYR, Hz(300)},
        {RIG_MODE_RTTY | RIG_MODE_RTTYR, RIG_FLT_ANY},
        {RIG_MODE_PKTUSB | RIG_MODE_PKTLSB, kHz(2.7)},
        {RIG_MODE_PKTUSB | RIG_MODE_PKTLSB, kHz(2.8)},
        {RIG_MODE_PKTUSB | RIG_MODE_PKTLSB, Hz(50)},
        {RIG_MODE_PKTUSB | RIG_MODE_PKTLSB, Hz(2400)},
        {RIG_MODE_PKTUSB | RIG_MODE_PKTLSB, Hz(500)},
        {RIG_MODE_PKTUSB | RIG_MODE_PKTLSB, Hz(300)},
        {RIG_MODE_PKTUSB | RIG_MODE_PKTLSB, RIG_FLT_ANY},
        {RIG_MODE_AM, kHz(6)},
        {RIG_MODE_AM, kHz(13)},
        {RIG_MODE_AM, kHz(2.7)},
        {RIG_MODE_AM, RIG_FLT_ANY},
        {RIG_MODE_FM, kHz(13)}, /* TBC */
        RIG_FLT_END,
    },
    .priv = (void *)& k3_priv_caps,

    .rig_init =     kenwood_init,
    .rig_cleanup =      kenwood_cleanup,
    .rig_open =     elecraft_open,
    .rig_close =        kenwood_close,
    .set_freq =     kenwood_set_freq,
    .get_freq =     kenwood_get_freq,
    .set_mode =     k3_set_mode,
    .get_mode =     k3_get_mode,
    .set_vfo =      k3_set_vfo,
    .get_vfo =      elecraft_get_vfo_tq,
    .set_split_mode =   k3_set_split_mode,
    .get_split_mode =   k3_get_split_mode,
    .set_split_vfo =    kenwood_set_split_vfo,
    .get_split_vfo =    kenwood_get_split_vfo_if,
    .set_rit =      k3_set_rit,
    .get_rit =      kenwood_get_rit,
    .set_xit =      k3_set_xit,
    .get_xit =      kenwood_get_xit,
    .get_ptt =      k4_get_ptt,
    .set_ptt =      k4_set_ptt,
    .get_dcd =      kenwood_get_dcd,
    .set_func =     k3_set_func,
    .get_func =     k3_get_func,
    .set_ext_parm =     kenwood_set_ext_parm,
    .get_ext_parm =     kenwood_get_ext_parm,
    .set_level =        k3_set_level,
    .get_level =        k3_get_level,
    .set_ext_level =    k3_set_ext_level,
    .get_ext_level =    k3_get_ext_level,
    .vfo_op =       kenwood_vfo_op,
    .set_trn =      kenwood_set_trn,
    .get_trn =      kenwood_get_trn,
    .set_powerstat =    kenwood_set_powerstat,
    .get_powerstat =    kenwood_get_powerstat,
    .set_ant =      kenwood_set_ant_no_ack,
    .get_ant =      kenwood_get_ant,
    .send_morse =       kenwood_send_morse,
    .wait_morse =       rig_wait_morse,
    .power2mW =     k3_power2mW,
    .hamlib_check_rig_caps = HAMLIB_CHECK_RIG_CAPS
};

const struct rig_caps kx3_caps =
{
    RIG_MODEL(RIG_MODEL_KX3),
    .model_name =       "KX3",
    .mfg_name =     "Elecraft",
    .version =      BACKEND_VER ".18",
    .copyright =        "LGPL",
    .status =       RIG_STATUS_STABLE,
    .rig_type =     RIG_TYPE_TRANSCEIVER,
    .ptt_type =     RIG_PTT_RIG,
    .dcd_type =     RIG_DCD_RIG,
    .port_type =        RIG_PORT_SERIAL,
    .serial_rate_min =  4800,
    .serial_rate_max =  38400,
    .serial_data_bits = 8,
    .serial_stop_bits = 1,
    .serial_parity =    RIG_PARITY_NONE,
    .serial_handshake = RIG_HANDSHAKE_NONE,
    .write_delay =      0,  /* Timing between bytes */
    .post_write_delay = 0,  /* Timing between command strings */
    .timeout =      1000,   /* FA and FB make take up to 500 ms on band change */
    .retry =        5,

    .has_get_func =     KX3_FUNC_ALL,
    .has_set_func =     KX3_FUNC_ALL,
    .has_get_level =    KX3_LEVEL_ALL,
    .has_set_level =    RIG_LEVEL_SET(KX3_LEVEL_ALL),
    .has_get_parm =     RIG_PARM_NONE,
    .has_set_parm =     RIG_PARM_NONE,  /* FIXME: parms */
    .level_gran = {
        [LVL_KEYSPD] = { .min = { .i = 8 }, .max = { .i = 50 }, .step = { .i = 1 } },
    },
    .parm_gran =        {},
    .extlevels =        kx3_ext_levels,
    .extparms =     kenwood_cfg_params,
    .preamp =       { 1, RIG_DBLST_END, },
    .attenuator =       { 10, RIG_DBLST_END, },
    .max_rit =      Hz(9990),
    .max_xit =      Hz(9990),
    .max_ifshift =      Hz(0),
    .vfo_ops =      K3_VFO_OP,
    .targetable_vfo =   RIG_TARGETABLE_FREQ | RIG_TARGETABLE_MODE,
    .transceive =       RIG_TRN_RIG,
    .bank_qty =     0,
    .chan_desc_sz =     0,

    .chan_list =        { RIG_CHAN_END },

    .rx_range_list1 =  {
        {kHz(500), MHz(30), K3_MODES, -1, -1, K3_VFO, K3_ANTS},
        { MHz(48), MHz(54), K3_MODES, -1, - 1, K3_VFO, K3_ANTS},
        RIG_FRNG_END,
    }, /* rx range */
    .tx_range_list1 =  {
        FRQ_RNG_HF(1, K3_MODES, mW(10), W(10), K3_VFO, K3_ANTS),
        FRQ_RNG_6m(1, K3_MODES, mW(10), W(10), K3_VFO, K3_ANTS),
        RIG_FRNG_END,
    }, /* tx range */

    .rx_range_list2 =  {
        {kHz(500), MHz(30), K3_MODES, -1, -1, K3_VFO, K3_ANTS},
        { MHz(48), MHz(54), K3_MODES, -1, -1, K3_VFO, K3_ANTS},
        RIG_FRNG_END,
    }, /* rx range */
    .tx_range_list2 =  {
        FRQ_RNG_HF(2, K3_MODES, mW(10), W(10), K3_VFO, K3_ANTS),
        FRQ_RNG_6m(2, K3_MODES, mW(10), W(10), K3_VFO, K3_ANTS),
        RIG_FRNG_END,
    }, /* tx range */
    .tuning_steps =  {
        {K3_MODES, 1},
        RIG_TS_END,
    },

    /* mode/filter list, remember: order matters! */
    /* Values are arbitrary based on common K3 filter options. */
    .filters =  {
        {RIG_MODE_SSB, kHz(2.7)},
        {RIG_MODE_SSB, kHz(2.8)},
        {RIG_MODE_SSB, kHz(1.8)},
        {RIG_MODE_SSB, kHz(2.4)},
        {RIG_MODE_SSB, RIG_FLT_ANY},
        {RIG_MODE_CW | RIG_MODE_CWR, kHz(1)},
        {RIG_MODE_CW | RIG_MODE_CWR, kHz(2.8)},
        {RIG_MODE_CW | RIG_MODE_CWR, Hz(50)},
        {RIG_MODE_CW | RIG_MODE_CWR, Hz(500)},
        {RIG_MODE_CW | RIG_MODE_CWR, Hz(300)},
        {RIG_MODE_CW | RIG_MODE_CWR, RIG_FLT_ANY},
        {RIG_MODE_RTTY | RIG_MODE_RTTYR, kHz(2)},
        {RIG_MODE_RTTY | RIG_MODE_RTTYR, kHz(2.7)},
        {RIG_MODE_RTTY | RIG_MODE_RTTYR, Hz(500)},
        {RIG_MODE_RTTY | RIG_MODE_RTTYR, Hz(300)},
        {RIG_MODE_RTTY | RIG_MODE_RTTYR, RIG_FLT_ANY},
        {RIG_MODE_PKTUSB | RIG_MODE_PKTLSB, kHz(2.7)},
        {RIG_MODE_PKTUSB | RIG_MODE_PKTLSB, kHz(2.8)},
        {RIG_MODE_PKTUSB | RIG_MODE_PKTLSB, Hz(50)},
        {RIG_MODE_PKTUSB | RIG_MODE_PKTLSB, Hz(2400)},
        {RIG_MODE_PKTUSB | RIG_MODE_PKTLSB, Hz(500)},
        {RIG_MODE_PKTUSB | RIG_MODE_PKTLSB, Hz(300)},
        {RIG_MODE_PKTUSB | RIG_MODE_PKTLSB, RIG_FLT_ANY},
        {RIG_MODE_AM, kHz(6)},
        {RIG_MODE_AM, kHz(13)},
        {RIG_MODE_AM, kHz(2.7)},
        {RIG_MODE_AM, RIG_FLT_ANY},
        {RIG_MODE_FM, kHz(13)}, /* TBC */
        RIG_FLT_END,
    },
    .priv = (void *)& k3_priv_caps,

    .rig_init =     kenwood_init,
    .rig_cleanup =      kenwood_cleanup,
    .rig_open =     elecraft_open,
    .rig_close =        kenwood_close,
    .set_freq =     kenwood_set_freq,
    .get_freq =     kenwood_get_freq,
    .set_mode =     k3_set_mode,
    .get_mode =     k3_get_mode,
    .set_vfo =      k3_set_vfo,
    .get_vfo =      k3_get_vfo,
    .set_split_mode =   k3_set_split_mode,
    .get_split_mode =   k3_get_split_mode,
    .set_split_vfo =    kenwood_set_split_vfo,
    .get_split_vfo =    kenwood_get_split_vfo_if,
    .set_rit =      k3_set_rit,
    .get_rit =      kenwood_get_rit,
    .set_xit =      k3_set_xit,
    .get_xit =      kenwood_get_xit,
    .get_ptt =      kenwood_get_ptt,
    .set_ptt =      kenwood_set_ptt,
    .get_dcd =      kenwood_get_dcd,
    .set_func =     k3_set_func,
    .get_func =     k3_get_func,
    .set_ext_parm =     kenwood_set_ext_parm,
    .get_ext_parm =     kenwood_get_ext_parm,
    .set_level =        kx3_set_level,
    .get_level =        kx3_get_level,
    .set_ext_level =    k3_set_ext_level,
    .get_ext_level =    k3_get_ext_level,
    .vfo_op =       kenwood_vfo_op,
    .set_trn =      kenwood_set_trn,
    .get_trn =      kenwood_get_trn,
    .set_powerstat =    kenwood_set_powerstat,
    .get_powerstat =    kenwood_get_powerstat,
    .set_ant =      kenwood_set_ant_no_ack,
    .get_ant =      kenwood_get_ant,
    .send_morse =       kenwood_send_morse,
    .wait_morse =       rig_wait_morse,
    .power2mW =     k3_power2mW,
    .hamlib_check_rig_caps = HAMLIB_CHECK_RIG_CAPS
};

const struct rig_caps kx2_caps =
{
    RIG_MODEL(RIG_MODEL_KX2),
    .model_name =       "KX2",
    .mfg_name =     "Elecraft",
    .version =      BACKEND_VER ".17",
    .copyright =        "LGPL",
    .status =       RIG_STATUS_STABLE,
    .rig_type =     RIG_TYPE_TRANSCEIVER,
    .ptt_type =     RIG_PTT_RIG,
    .dcd_type =     RIG_DCD_RIG,
    .port_type =        RIG_PORT_SERIAL,
    .serial_rate_min =  4800,
    .serial_rate_max =  38400,
    .serial_data_bits = 8,
    .serial_stop_bits = 1,
    .serial_parity =    RIG_PARITY_NONE,
    .serial_handshake = RIG_HANDSHAKE_NONE,
    .write_delay =      0,  /* Timing between bytes */
    .post_write_delay = 0,  /* Timing between command strings */
    .timeout =      1000,   /* FA and FB make take up to 500 ms on band change */
    .retry =        5,

    .has_get_func =     KX3_FUNC_ALL,
    .has_set_func =     KX3_FUNC_ALL,
    .has_get_level =    KX3_LEVEL_ALL,
    .has_set_level =    RIG_LEVEL_SET(KX3_LEVEL_ALL),
    .has_get_parm =     RIG_PARM_NONE,
    .has_set_parm =     RIG_PARM_NONE,  /* FIXME: parms */
    .level_gran = {
        [LVL_KEYSPD] = { .min = { .i = 8 }, .max = { .i = 50 }, .step = { .i = 1 } },
    },
    .parm_gran =        {},
    .extlevels =        kx3_ext_levels,
    .extparms =     kenwood_cfg_params,
    .preamp =       { 1, RIG_DBLST_END, },
    .attenuator =       { 10, RIG_DBLST_END, },
    .max_rit =      Hz(9990),
    .max_xit =      Hz(9990),
    .max_ifshift =      Hz(0),
    .vfo_ops =      K3_VFO_OP,
    .targetable_vfo =   RIG_TARGETABLE_FREQ | RIG_TARGETABLE_MODE,
    .transceive =       RIG_TRN_RIG,
    .bank_qty =     0,
    .chan_desc_sz =     0,

    .chan_list =        { RIG_CHAN_END },

    .rx_range_list1 =  {
        {kHz(500), MHz(30), K3_MODES, -1, -1, K3_VFO, K3_ANTS},
        { MHz(48), MHz(54), K3_MODES, -1, - 1, K3_VFO, K3_ANTS},
        RIG_FRNG_END,
    }, /* rx range */
    .tx_range_list1 =  {
        FRQ_RNG_HF(1, K3_MODES, mW(10), W(12), K3_VFO, K3_ANTS),
        FRQ_RNG_6m(1, K3_MODES, mW(10), W(12), K3_VFO, K3_ANTS),
        RIG_FRNG_END,
    }, /* tx range */

    .rx_range_list2 =  {
        {kHz(500), MHz(30), K3_MODES, -1, -1, K3_VFO, K3_ANTS},
        { MHz(48), MHz(54), K3_MODES, -1, -1, K3_VFO, K3_ANTS},
        RIG_FRNG_END,
    }, /* rx range */
    .tx_range_list2 =  {
        FRQ_RNG_HF(2, K3_MODES, mW(10), W(12), K3_VFO, K3_ANTS),
        FRQ_RNG_6m(2, K3_MODES, mW(10), W(12), K3_VFO, K3_ANTS),
        RIG_FRNG_END,
    }, /* tx range */
    .tuning_steps =  {
        {K3_MODES, 1},
        RIG_TS_END,
    },

    /* mode/filter list, remember: order matters! */
    /* Values are arbitrary based on common K3 filter options. */
    .filters =  {
        {RIG_MODE_SSB, kHz(2.7)},
        {RIG_MODE_SSB, kHz(2.8)},
        {RIG_MODE_SSB, kHz(1.8)},
        {RIG_MODE_SSB, kHz(2.4)},
        {RIG_MODE_SSB, RIG_FLT_ANY},
        {RIG_MODE_CW | RIG_MODE_CWR, kHz(1)},
        {RIG_MODE_CW | RIG_MODE_CWR, kHz(2.8)},
        {RIG_MODE_CW | RIG_MODE_CWR, Hz(50)},
        {RIG_MODE_CW | RIG_MODE_CWR, Hz(500)},
        {RIG_MODE_CW | RIG_MODE_CWR, Hz(300)},
        {RIG_MODE_CW | RIG_MODE_CWR, RIG_FLT_ANY},
        {RIG_MODE_RTTY | RIG_MODE_RTTYR, kHz(2)},
        {RIG_MODE_RTTY | RIG_MODE_RTTYR, kHz(2.7)},
        {RIG_MODE_RTTY | RIG_MODE_RTTYR, Hz(500)},
        {RIG_MODE_RTTY | RIG_MODE_RTTYR, Hz(300)},
        {RIG_MODE_RTTY | RIG_MODE_RTTYR, RIG_FLT_ANY},
        {RIG_MODE_PKTUSB | RIG_MODE_PKTLSB, kHz(2.7)},
        {RIG_MODE_PKTUSB | RIG_MODE_PKTLSB, kHz(2.8)},
        {RIG_MODE_PKTUSB | RIG_MODE_PKTLSB, Hz(50)},
        {RIG_MODE_PKTUSB | RIG_MODE_PKTLSB, Hz(2400)},
        {RIG_MODE_PKTUSB | RIG_MODE_PKTLSB, Hz(500)},
        {RIG_MODE_PKTUSB | RIG_MODE_PKTLSB, Hz(300)},
        {RIG_MODE_PKTUSB | RIG_MODE_PKTLSB, RIG_FLT_ANY},
        {RIG_MODE_AM, kHz(6)},
        {RIG_MODE_AM, kHz(13)},
        {RIG_MODE_AM, kHz(2.7)},
        {RIG_MODE_AM, RIG_FLT_ANY},
        {RIG_MODE_FM, kHz(13)}, /* TBC */
        RIG_FLT_END,
    },
    .priv = (void *)& k3_priv_caps,

    .rig_init =     kenwood_init,
    .rig_cleanup =      kenwood_cleanup,
    .rig_open =     elecraft_open,
    .rig_close =        kenwood_close,
    .set_freq =     kenwood_set_freq,
    .get_freq =     kenwood_get_freq,
    .set_mode =     k3_set_mode,
    .get_mode =     k3_get_mode,
    .set_vfo =      k3_set_vfo,
    .get_vfo =      k3_get_vfo,
    .set_split_mode =   k3_set_split_mode,
    .get_split_mode =   k3_get_split_mode,
    .set_split_vfo =    kenwood_set_split_vfo,
    .get_split_vfo =    kenwood_get_split_vfo_if,
    .set_rit =      k3_set_rit,
    .get_rit =      kenwood_get_rit,
    .set_xit =      k3_set_xit,
    .get_xit =      kenwood_get_xit,
    .get_ptt =      kenwood_get_ptt,
    .set_ptt =      kenwood_set_ptt,
    .get_dcd =      kenwood_get_dcd,
    .set_func =     k3_set_func,
    .get_func =     k3_get_func,
    .set_ext_parm =     kenwood_set_ext_parm,
    .get_ext_parm =     kenwood_get_ext_parm,
    .set_level =        kx3_set_level,
    .get_level =        kx3_get_level,
    .set_ext_level =    k3_set_ext_level,
    .get_ext_level =    k3_get_ext_level,
    .vfo_op =       kenwood_vfo_op,
    .set_trn =      kenwood_set_trn,
    .get_trn =      kenwood_get_trn,
    .set_powerstat =    kenwood_set_powerstat,
    .get_powerstat =    kenwood_get_powerstat,
    .set_ant =      kenwood_set_ant_no_ack,
    .get_ant =      kenwood_get_ant,
    .send_morse =       kenwood_send_morse,
    .wait_morse =       rig_wait_morse,
    .power2mW =     k3_power2mW,
    .hamlib_check_rig_caps = HAMLIB_CHECK_RIG_CAPS
};

/*
 * K3 extension function definitions follow
 */


/* k3_get_mode()
 *
 * The K3 supports a new command, DT, to query the data submode so
 * RIG_MODE_PKTUSB and RIG_MODE_PKTLSB can be supported.
 */

int k3_get_mode(RIG *rig, vfo_t vfo, rmode_t *mode, pbwidth_t *width)
{
    char buf[KENWOOD_MAX_BUF_LEN];
    int err;
    rmode_t temp_m;
    pbwidth_t temp_w;
    char *cmd_mode = "DT";
    char *cmd_bw = "BW";
    int cmd_bw_len = 6;

    rig_debug(RIG_DEBUG_VERBOSE, "%s called vfo=%s\n", __func__, rig_strvfo(vfo));

    if (vfo == RIG_VFO_B && rig->caps->rig_model == RIG_MODEL_K4)
    {
        cmd_mode = "DT$";
        cmd_bw = "BW$";
        cmd_bw_len = 7;

    }

    if (!mode || !width)
    {
        return -RIG_EINVAL;
    }

    if (vfo == RIG_VFO_CURR)
    {
        vfo = rig->state.current_vfo;
    }

    err = kenwood_get_mode(rig, vfo, &temp_m, &temp_w);

    if (err != RIG_OK)
    {
        return err;
    }

    if (temp_m == RIG_MODE_RTTY)
    {
        err = kenwood_safe_transaction(rig, cmd_mode, buf, KENWOOD_MAX_BUF_LEN,
                                       strlen(cmd_mode) + 1);

        if (err != RIG_OK)
        {
            rig_debug(RIG_DEBUG_VERBOSE, "%s: Cannot read K3 DT value\n",
                      __func__);
            return err;
        }

        switch (atoi(&buf[2]))
        {
        case K3_MODE_DATA_A:
        case K3_MODE_PSK_D:
            *mode = RIG_MODE_PKTUSB;
            break;

        case K3_MODE_AFSK_A:
            *mode = RIG_MODE_PKTLSB;
            break;

        default:
            *mode = temp_m;
            break;
        }
    }
    else if (temp_m == RIG_MODE_RTTYR)
    {
        err = kenwood_safe_transaction(rig, cmd_mode, buf, KENWOOD_MAX_BUF_LEN,
                                       strlen(cmd_mode) + 1);

        if (err != RIG_OK)
        {
            rig_debug(RIG_DEBUG_VERBOSE, "%s: Cannot read K3 DT value\n",
                      __func__);
            return err;
        }

        switch (atoi(&buf[2]))
        {
        case K3_MODE_DATA_A:
        case K3_MODE_PSK_D:
            *mode = RIG_MODE_PKTUSB;
            break;

        case K3_MODE_AFSK_A:
            *mode = RIG_MODE_PKTLSB;
            break;

        case K3_MODE_FSK_D:
        default:
            *mode = temp_m;
            break;
        }
    }
    else
    {
        *mode = temp_m;
    }

    /* The K3 is not limited to specific filter widths so we can query
     * the actual bandwidth using the BW command
     */
    err = kenwood_safe_transaction(rig, cmd_bw, buf, KENWOOD_MAX_BUF_LEN,
                                   cmd_bw_len);

    if (err != RIG_OK)
    {
        rig_debug(RIG_DEBUG_VERBOSE, "%s: Cannot read K3 BW value\n", __func__);
        return err;
    }

    *width = atoi(&buf[cmd_bw_len - 4]) * 10;

    return RIG_OK;
}


/* k3_set_mode()
 *
 * As with k3_get_mode(), the K3 can also set the data sub-modes which
 * allows use of RIG_MODE_PKTUSB and RIG_MODE_PKTLSB.
 *
 * The K3 supports AFSK & FSK sub-modes and for the D versions it also
 * has an internal RTTY and PSK31 decoder. The decoder sub-modes are
 * reported as FSK (RTTY) and the AFSK sub-modes are reported as
 * PKT(USB & LSB). The Submode determines if MD6 starts off in USB
 * or LSB. To get the reverse of that, you send MD9 and the the submode.
 * On KX3 it's
 *
 * DT0 defaults MD6 to USB
 * DT1 defaults MD6 to LSB
 * DT2 defaults MD6 to LSB
 * DT3 defaults MD6 to USB
 *
 * So to inverse that DT0 for LSB, you'd send MD9 then DT0.
 *
 * For mode set the data sub-modes are set as follows:
 *
 * PKTUSB = sets the rig to DATA mode submode Data A (DT0)
 * PKTLSB = sets the rig to DATA REV mode submode Data A (DT0)
 * RTTY = sets the rig to AFSK A 45 bps rtty (DT1)
 * RTTYR = sets the rig to FSK D 45 bps rtty (DT2)
 * PSK = sets the rig to PSK D (DT3)

 * Not all data sub-mode combinations are possible but the above
 * mapping seems most likely to cover the user requirements.
 */

int k3_set_mode(RIG *rig, vfo_t vfo, rmode_t mode, pbwidth_t width)
{
    int err, err2;
    char cmd_m[5];
    char buf[KENWOOD_MAX_BUF_LEN];
    char *dtcmd;
    struct kenwood_priv_caps *caps = kenwood_caps(rig);
    struct kenwood_priv_data *priv = rig->state.priv;

    rig_debug(RIG_DEBUG_VERBOSE, "%s called vfo=%s mode=%s width=%d\n", __func__,
              rig_strvfo(vfo), rig_strrmode(mode), (int)width);

    if (vfo == RIG_VFO_CURR)
    {
        vfo = rig->state.current_vfo;
    }

    rmode_t tmodeA, tmodeB;
    pbwidth_t twidth;
    err = k3_get_mode(rig, RIG_VFO_A, &tmodeA, &twidth);
    err2 = k3_get_mode(rig, RIG_VFO_B, &tmodeB, &twidth);

    // we keep both vfos in the same mode -- any reason they should ever be different?  If so, fix this
    // if we change mode on one VFO we'll also change the other
    if (err == RIG_OK && err2 == RIG_OK && tmodeA == mode && tmodeB == mode
            && width == RIG_PASSBAND_NOCHANGE)
    {
        rig_debug(RIG_DEBUG_TRACE, "%s(%d): mode/width no change, skipping\n", __FILE__,
                  __LINE__);
        return RIG_OK;
    }
    else
    {
        rig_debug(RIG_DEBUG_TRACE,
                  "%s(%d): changing oldmode=A=%s B=%s, to mode=%s, oldwidth=%ld, to width=%ld\n",
                  __FILE__,
                  __LINE__, rig_strrmode(tmodeA), rig_strrmode(tmodeB), rig_strrmode(mode),
                  twidth, width);
    }

    dtcmd = "DT";

    if ((priv->is_k4 || priv->is_k4d || priv->is_k4hd)  && vfo == RIG_VFO_B)
    {
        dtcmd = "DT$";
    }

    switch (mode)
    {
    case RIG_MODE_PKTLSB:
        mode = RIG_MODE_RTTYR; // in "DT0" Subband RIG_MODE_RTTYR = USB and RIG_MODE_RTTY = LSB
        SNPRINTF(cmd_m, sizeof(cmd_m),
                 "%s0", dtcmd); /* DATA A mode - DATA (REV) on LSB optimized for HF Packet, VFO dial is suppressed carrier QRG */
        break;

    case RIG_MODE_PKTUSB:
        mode = RIG_MODE_RTTY; // in "DT0" Subband RIG_MODE_RTTYR = USB and RIG_MODE_RTTY = LSB
        SNPRINTF(cmd_m, sizeof(cmd_m),
                 "%s0", dtcmd); /* DATA A mode - DATA on USB general, VFO dial is suppressed carrier QRG */
        break;

    case RIG_MODE_RTTY:
        mode = RIG_MODE_RTTY; // in "DT1" Subband RIG_MODE_RTTY = LSB and RIG_MODE_RTTYR = USB
        SNPRINTF(cmd_m, sizeof(cmd_m),
                 "%s2", dtcmd); /* FSK D mode - direct FSK on LSB optimized for RTTY, VFO dial is MARK */
        break;

    case RIG_MODE_RTTYR:
        mode = RIG_MODE_RTTYR; // in "DT2" Subband RIG_MODE_RTTY = LSB and RIG_MODE_RTTYR = USB
        SNPRINTF(cmd_m, sizeof(cmd_m),
                 "%s1", dtcmd); /* FSK D mode - direct FSK keying, LSB is "normal", VFO dial is MARK */
        break;

    case RIG_MODE_PSK:
        mode = RIG_MODE_PSK; // in "DT3" subband RIG_MODE_PSK = USB # kenwood.c mode but may need kenwwod.c mode table review.
        SNPRINTF(cmd_m, sizeof(cmd_m),
                 "%s3", dtcmd); /* PSK D Mode - direct PSK keying, USB is "normal", VFO dial is MARK */
        break;

    default:
        break;
    }

    int kmode;
    int c;
    kmode = rmode2kenwood(mode, caps->mode_table);

    if (kmode < 0)
    {
        rig_debug(RIG_DEBUG_WARN, "%s: unsupported mode '%s'\n",
                  __func__, rig_strrmode(mode));
        RETURNFUNC(-RIG_EINVAL);
    }

    if (kmode <= 9)
    {
        c = '0' + kmode;
    }
    else
    {
        c = 'A' + kmode - 10;
    }

    rig_debug(RIG_DEBUG_VERBOSE, "%s: kmode=%d, cmode=%c\n", __func__, kmode, c);

    if (vfo == RIG_VFO_B)
    {
        SNPRINTF(buf, sizeof(buf), "MD$%c", c);
    }
    else
    {
        SNPRINTF(buf, sizeof(buf), "MD%c", c);
    }

    if (priv->split)
    {
        // then we keep both VFOS in the same mode
        SNPRINTF(buf, sizeof(buf), "MD%c;MD$%c", c, c);
    }

    err = kenwood_transaction(rig, buf, NULL, 0);

    if (err != RIG_OK) { RETURNFUNC(err); }

    if (width != RIG_PASSBAND_NOCHANGE)
    {
        char cmd_s[64];
        /* and set the requested bandwidth.  On my K3, the bandwidth is rounded
         * down to the nearest 50 Hz, i.e. sending BW0239; will cause the bandwidth
         * to be set to 2.350 kHz.  As the width must be divided by 10, 10 Hz values
         * between 0 and 4 round down to the nearest 100 Hz and values between 5
         * and 9 round down to the nearest 50 Hz.
         *
         * width string value must be padded with leading '0' to equal four
         * characters.
         */

        /* passband widths vary by mode so gather lower and upper limits */
        //pbwidth_t pb_nar = rig_passband_narrow(rig, mode);
        //pbwidth_t pb_wid = rig_passband_wide(rig, mode);

        if (width < 0)
        {
            width = labs(width);
        }

        if (width == RIG_PASSBAND_NORMAL)
        {
            width = rig_passband_normal(rig, mode);
        }

#if 0
        else if (width < pb_nar)
        {
            width = pb_nar;
        }
        else if (width > pb_wid)
        {
            width = pb_wid;
        }

#endif
        width += 9;  // rounds to 10Hz

        if (width > 99999) { width = 99999; }

        if (vfo == RIG_VFO_B)
        {
            SNPRINTF(cmd_s, sizeof(cmd_s), "BW$%04ld", width / 10);
        }
        else
        {
            SNPRINTF(cmd_s, sizeof(cmd_s), "BW%04ld", width / 10);
        }

        err = kenwood_transaction(rig, cmd_s, NULL, 0);

        if (err != RIG_OK)
        {
            return err;
        }
    }

    /* Now set data sub-mode.  K3 needs to be in a DATA mode before setting
     * the sub-mode.
     */
    if (mode == RIG_MODE_PKTLSB || mode == RIG_MODE_PKTUSB
            || mode == RIG_MODE_RTTY || mode == RIG_MODE_RTTYR)
    {
        err = kenwood_transaction(rig, cmd_m, NULL, 0);

        if (err != RIG_OK)
        {
            return err;
        }
    }

    return RIG_OK;
}

/* Elecraft rigs don't really know about swappings vfos.
 * We just emulate them so rigctl can work correctly.
 */

int k3_set_vfo(RIG *rig, vfo_t vfo)
{
    ENTERFUNC;

    // we emulate vfo selection for Elecraft
    rig->state.current_vfo = vfo;

    RETURNFUNC(RIG_OK);
}

int k3_get_vfo(RIG *rig, vfo_t *vfo)
{
    ENTERFUNC;

    *vfo = rig->state.current_vfo;

    RETURNFUNC(RIG_OK);
}

/* Support the RC command for clearing RIT/XIT,
 *
 * token    Defined in elecraft.h or this file
 * val      Type depends on token type from confparams structure:
 *      NUMERIC: val.f
 *      COMBO: val.i, starting from 0 Index to a string table.
 *      STRING: val.cs for set, val.s for get
 *      CHECKBUTTON: val.i 0/1
 *
 * See Private Elecraft extra levels definitions in elecraft.c and
 * private token #define in elecraft.h
 */
int k3_set_ext_level(RIG *rig, vfo_t vfo, token_t token, value_t val)
{
    char buf[10];

    rig_debug(RIG_DEBUG_VERBOSE, "%s called\n", __func__);

    switch (token)
    {
    case TOK_RIT_CLR:
        return kenwood_transaction(rig, "RC", NULL, 0);

    case TOK_ESSB:
        SNPRINTF(buf, sizeof(buf), "ES%c", (val.i == 0) ? '0' : '1');
        return kenwood_transaction(rig, buf, NULL, 0);

    case TOK_RX_ANT:
        SNPRINTF(buf, sizeof(buf), "AR%c", (val.i == 0) ? '0' : '1');
        return kenwood_transaction(rig, buf, NULL, 0);

    case TOK_LINK_VFOS:
        SNPRINTF(buf, sizeof(buf), "LN%c", (val.i == 0) ? '0' : '1');
        return kenwood_transaction(rig, buf, NULL, 0);

    case TOK_TX_METER:
        SNPRINTF(buf, sizeof(buf), "TM%c", val.i + '0');
        return kenwood_transaction(rig, buf, NULL, 0);

    case TOK_IF_NB:
        return k3_set_nb_level(rig, -1, val.f / 21.0f);

    default:
        rig_debug(RIG_DEBUG_WARN, "%s: Unsupported set_ext_level %s\n", __func__,
                  rig_strlevel(token));
        return -RIG_EINVAL;
    }
}


/* Support the FI command for reading the IF center frequency,
 * useful for panadapters and such that need to know the IF center.
 * TQ command is a quick transmit status query--K2/K3 only.
 *
 * token    Defined in elecraft.h or this file
 * val      Type depends on token type from confparams structure:
 *      NUMERIC: val.f
 *      COMBO: val.i, starting from 0 Index to a string table.
 *      STRING: val.cs for set, val.s for get
 *      CHECKBUTTON: val.i 0/1
 */
int k3_get_ext_level(RIG *rig, vfo_t vfo, token_t token, value_t *val)
{
    char buf[KENWOOD_MAX_BUF_LEN];
    int err;

    rig_debug(RIG_DEBUG_VERBOSE, "%s called\n", __func__);

    if (!val)
    {
        return -RIG_EINVAL;
    }

    switch (token)
    {
    case TOK_IF_FREQ:
        err = kenwood_safe_transaction(rig, "FI", buf, KENWOOD_MAX_BUF_LEN, 6);

        if (err != RIG_OK)
        {
            return err;
        }

        val->f = 8210000.0 + (float) atoi(&buf[2]);
        break;

    case TOK_TX_STAT:
        return get_kenwood_func(rig, "TQ", &val->i);

    case TOK_ESSB:
        return get_kenwood_func(rig, "ES", &val->i);

    case TOK_RX_ANT:
        return get_kenwood_func(rig, "AR", &val->i);

    case TOK_LINK_VFOS:
        return get_kenwood_func(rig, "LN", &val->i);

    case TOK_TX_METER:
        return get_kenwood_func(rig, "TM", &val->i);

    case TOK_IF_NB:
    {
        float if_nb;
        err = k3_get_nb_level(rig, NULL, &if_nb);

        if (err != RIG_OK)
        {
            return err;
        }

        val->f = (float)((int)(if_nb * 21.0f));
        break;
    }

    default:
        rig_debug(RIG_DEBUG_WARN, "%s: Unsupported get_ext_level %s\n", __func__,
                  rig_strlevel(token));
        return -RIG_EINVAL;
    }

    return RIG_OK;
}


/*
 * k3_set_rit() -- Differs from from generic Kenwood function as K3 can set
 * RIT to an arbitrary offset.  When rit == 0, the RIT offset is cleared.
 */
int k3_set_rit(RIG *rig, vfo_t vfo, shortfreq_t rit)
{
    int err;

    rig_debug(RIG_DEBUG_VERBOSE, "%s called\n", __func__);

    err = set_rit_xit(rig, rit);

    if (err != RIG_OK)
    {
        return err;
    }

    return RIG_OK;
}


/*
 * k3_set_xit() -- Differs from from generic Kenwood function as K3 can set
 * XIT to an arbitrary offset.  When rit == 0, the XIT offset is cleared.
 */
int k3_set_xit(RIG *rig, vfo_t vfo, shortfreq_t rit)
{
    int err;

    rig_debug(RIG_DEBUG_VERBOSE, "%s called\n", __func__);

    err = set_rit_xit(rig, rit);

    if (err != RIG_OK)
    {
        return err;
    }

    return RIG_OK;
}


/*
 * The K3 *always* uses VFOB for TX.
 */
int k3_set_split_mode(RIG *rig, vfo_t vfo, rmode_t tx_mode, pbwidth_t tx_width)
{
    struct kenwood_priv_caps *caps = kenwood_caps(rig);
    char buf[32];
    char kmode;
    int err;
    char cmd_m[16];
    struct kenwood_priv_data *priv = rig->state.priv;

    rig_debug(RIG_DEBUG_VERBOSE, "%s called\n", __func__);

    switch (tx_mode)
    {
    case RIG_MODE_PKTLSB:
        tx_mode = RIG_MODE_RTTYR; // "DT0" RIG_MODE_RTTY = LSB
        SNPRINTF(cmd_m, sizeof(cmd_m),
                 "DT0;"); /* DATA A mode - DATA-R LSB, suppressed carrier */

        if (priv->is_k4d || priv->is_k4hd)
        {
            strcat(cmd_m, "DT$0;");
        }

        break;

    case RIG_MODE_PKTUSB:
        tx_mode = RIG_MODE_RTTY; // "DT0" RIG_MODE_RTTYR = USB
        SNPRINTF(cmd_m, sizeof(cmd_m),
                 "DT0;"); /* DATA A mode - DATA on USB, suppressed carrier */

        if (priv->is_k4d || priv->is_k4hd)
        {
            strcat(cmd_m, "DT$0;");
        }

        break;

    case RIG_MODE_RTTY:
        tx_mode = RIG_MODE_RTTY; // DT1" RIG_MODE_RTTY = LSB and RIG_MODE_RTTYR = USB
        SNPRINTF(cmd_m, sizeof(cmd_m),
                 "DT2;"); /* FSK D mode - direct FSK on LSB optimized for RTTY, VFO dial is MARK */

        if (priv->is_k4d || priv->is_k4hd)
        {
            strcat(cmd_m, "DT$2;");
        }

        break;

    case RIG_MODE_RTTYR:
        tx_mode = RIG_MODE_RTTYR; // "DT2" RIG_MODE_RTTY = USB and RIG_MODE_RTTYR = USB
        SNPRINTF(cmd_m, sizeof(cmd_m),
                 "DT1;"); /* FSK D mode - direct FSK on USB optimized for RTTY, VFO dial is MARK */

        if (priv->is_k4d || priv->is_k4hd)
        {
            strcat(cmd_m, "DT$1;");
        }

        break;

    case RIG_MODE_PSK:
        tx_mode = RIG_MODE_PSK;
        SNPRINTF(cmd_m, sizeof(cmd_m),
                 "DT3;FT1;"); /* PSK D Mode - direct PSK keying, USB is "normal", VFO dial is MARK */

        if (priv->is_k4d || priv->is_k4hd)
        {
            strcat(cmd_m, "DT$3;");
        }

        break;

    default:
        break;
    }

// Enabling this clause for just the K4 for now
#if 1

    if (priv->is_k4d || priv->is_k4hd)
    {
        // split can get turned off when modes are changing
        // so if the rig did this independently of us we turn it back on
        // even if the rig changes the split status should be the last thing we did
        if (priv->split) { strcat(cmd_m, "FT1;"); }

        /* Set data sub-mode.  K3 needs to be in a DATA mode before setting
         * the sub-mode or switching to VFOB so we do this before the MD$ command.
         */
        if (tx_mode == RIG_MODE_PKTLSB || tx_mode == RIG_MODE_PKTUSB
                || tx_mode == RIG_MODE_RTTY || tx_mode == RIG_MODE_RTTYR)
        {
            err = kenwood_transaction(rig, cmd_m, NULL, 0);

            if (err != RIG_OK)
            {
                return err;
            }
        }
    }

#endif

    kmode = rmode2kenwood(tx_mode, caps->mode_table);

    if (kmode < 0)
    {
        rig_debug(RIG_DEBUG_WARN, "%s: unsupported mode '%s'\n",
                  __func__, rig_strrmode(tx_mode));
        return -RIG_EINVAL;
    }

    SNPRINTF(buf, sizeof(buf), "MD$%c", '0' + kmode);
    err = kenwood_transaction(rig, buf, NULL, 0);

    if (err != RIG_OK)
    {
        return err;
    }

    if (tx_width != RIG_PASSBAND_NOCHANGE)
    {
        char cmd_s[32];
        /* and set the requested bandwidth.  On my K3, the bandwidth is rounded
         * down to the nearest 50 Hz, i.e. sending BW0239; will cause the bandwidth
         * to be set to 2.350 kHz.  As the width must be divided by 10, 10 Hz values
         * between 0 and 4 round down to the nearest 100 Hz and values between 5
         * and 9 round down to the nearest 50 Hz.
         *
         * tx_width string value must be padded with leading '0' to equal four
         * characters.
         */

        /* passband widths vary by mode so gather lower and upper limits */
        //pbwidth_t pb_nar = rig_passband_narrow(rig, tx_mode);
        //pbwidth_t pb_wid = rig_passband_wide(rig, tx_mode);

        if (tx_width < 0)
        {
            tx_width = labs(tx_width);
        }

        if (tx_width == RIG_PASSBAND_NORMAL)
        {
            tx_width = rig_passband_normal(rig, tx_mode);
        }

#if 0
        else if (tx_width < pb_nar)
        {
            tx_width = pb_nar;
        }
        else if (tx_width > pb_wid)
        {
            tx_width = pb_wid;
        }

#endif

        SNPRINTF(cmd_s, sizeof(cmd_s), "BW$%04ld", tx_width / 10);
        err = kenwood_transaction(rig, cmd_s, NULL, 0);

        if (err != RIG_OK)
        {
            return err;
        }
    }


    return RIG_OK;
}


/* The K3 *always* uses VFOB for TX.
 */
int k3_get_split_mode(RIG *rig, vfo_t vfo, rmode_t *tx_mode,
                      pbwidth_t *tx_width)
{
    char buf[KENWOOD_MAX_BUF_LEN];
    int err;
    rmode_t temp_m;
    struct kenwood_priv_caps *caps = kenwood_caps(rig);

    rig_debug(RIG_DEBUG_VERBOSE, "%s called\n", __func__);

    if (!tx_mode || !tx_width)
    {
        return -RIG_EINVAL;
    }

    err = kenwood_safe_transaction(rig, "MD$", buf, KENWOOD_MAX_BUF_LEN, 4);

    if (err != RIG_OK)
    {
        return err;
    }

    temp_m = kenwood2rmode(buf[3] - '0', caps->mode_table);

    if (temp_m == RIG_MODE_RTTY)
    {
        err = kenwood_safe_transaction(rig, "DT", buf, KENWOOD_MAX_BUF_LEN, 3);

        if (err != RIG_OK)
        {
            rig_debug(RIG_DEBUG_VERBOSE, "%s: Cannot read K3 DT value\n",
                      __func__);
            return err;
        }

        switch (atoi(&buf[2]))
        {
        case K3_MODE_DATA_A:
        case K3_MODE_PSK_D:
            *tx_mode = RIG_MODE_PKTUSB;
            break;

        case K3_MODE_AFSK_A:
            *tx_mode = RIG_MODE_PKTLSB;
            break;

        default:
            *tx_mode = temp_m;
            break;
        }
    }
    else if (temp_m == RIG_MODE_RTTYR)
    {
        err = kenwood_safe_transaction(rig, "DT", buf, KENWOOD_MAX_BUF_LEN, 3);

        if (err != RIG_OK)
        {
            rig_debug(RIG_DEBUG_VERBOSE, "%s: Cannot read K3 DT value\n",
                      __func__);
            return err;
        }

        switch (atoi(&buf[2]))
        {
        case K3_MODE_DATA_A:
        case K3_MODE_PSK_D:
            *tx_mode = RIG_MODE_PKTLSB;
            break;

        case K3_MODE_AFSK_A:
            *tx_mode = RIG_MODE_PKTUSB;
            break;

        case K3_MODE_FSK_D:
            break;

        default:
            *tx_mode = temp_m;
            break;
        }
    }
    else
    {
        *tx_mode = temp_m;
    }

    /* The K3 is not limited to specific filter widths so we can query
     * the actual bandwidth using the BW$ command
     */
    err = kenwood_safe_transaction(rig, "BW$", buf, KENWOOD_MAX_BUF_LEN, 7);

    if (err != RIG_OK)
    {
        rig_debug(RIG_DEBUG_VERBOSE, "%s: Cannot read K3 BW$ value\n", __func__);
        return err;
    }

    *tx_width = atoi(&buf[3]) * 10;

    return RIG_OK;
}

static int k3_get_maxpower(RIG *rig)
{
    //int retval;
    int maxpower = 15; // K3 default power level
    //char levelbuf[KENWOOD_MAX_BUF_LEN];
    struct kenwood_priv_data *priv = rig->state.priv;

    // default range is 0-15 if there is no KPA3 installed
    if (priv->has_kpa3 || priv->has_kpa100)
    {
        maxpower = 110;
    }

// Elecraft makes max power pretty variable
// So we will stick with 15W or 110W and scale everything to that
#if 0
    else if (RIG_IS_KX2 || RIG_IS_KX3)
    {

        int bandnum = -1;
        retval = kenwood_safe_transaction(rig, "BN", levelbuf, KENWOOD_MAX_BUF_LEN, 4);

        if (retval != RIG_OK) { return retval; }

        sscanf(levelbuf, "BN%d", &bandnum);

        switch (bandnum)
        {
        case 1:
        case 2:
        case 3:
        case 4:
        case 5:
            maxpower = 15;
            break;

        case 0: // 160M
        case 6: // 17M
        case 7: // 15M
        case 8: // 12M
        case 9: // 10M
            maxpower = 12;
            break;

        case 10: // 6M
            maxpower = 10;
            break;

        default: // are transverters all limited to 3W??
            maxpower = 3;
            break;
        }
    }

#endif

    rig_debug(RIG_DEBUG_TRACE, "%s: maxpower=%d\n", __func__, maxpower);
    return maxpower;
}

int k3_power2mW(RIG *rig,
                unsigned int *mwpower,
                float power,
                freq_t freq,
                rmode_t mode)
{
    *mwpower = power * k3_get_maxpower(rig) * 1000;
    return RIG_OK;
}

int k3_set_level(RIG *rig, vfo_t vfo, setting_t level, value_t val)
{
    char levelbuf[16];
    int kenwood_val;

    rig_debug(RIG_DEBUG_VERBOSE, "%s called\n", __func__);

    if (RIG_LEVEL_IS_FLOAT(level))
    {
        kenwood_val = val.f * 255;
    }
    else
    {
        kenwood_val = val.i;
    }

    switch (level)
    {
    case RIG_LEVEL_AGC:
        switch (val.i)
        {
        case RIG_AGC_OFF:
            kenwood_val = 0;
            break;

        case RIG_AGC_SUPERFAST:
        case RIG_AGC_FAST:
            kenwood_val = 2;
            break;

        case RIG_AGC_MEDIUM:
        case RIG_AGC_SLOW:
            kenwood_val = 4;
            break;

        case RIG_AGC_USER:
        case RIG_AGC_AUTO:
            return -RIG_EINVAL;
        }

        SNPRINTF(levelbuf, sizeof(levelbuf), "GT%03d", kenwood_val);
        break;

    case RIG_LEVEL_ATT:
        if (val.i == 0)
        {
            SNPRINTF(levelbuf, sizeof(levelbuf), "RA00");
        }
        else if (val.i == 10)
        {
            SNPRINTF(levelbuf, sizeof(levelbuf), "RA01");
        }
        else
        {
            int i;
            int foundit = 0;

            for (i = 0; i < HAMLIB_MAXDBLSTSIZ && rig->state.attenuator[i]; i++)
            {
                if (val.i == rig->state.attenuator[i])
                {
                    SNPRINTF(levelbuf, sizeof(levelbuf), "RA%02d", i + 1);
                    foundit = 1;
                    break;
                }
            }

            if (!foundit)
            {
                return -RIG_EINVAL;
            }
        }

        break;

    case RIG_LEVEL_MICGAIN:
        SNPRINTF(levelbuf, sizeof(levelbuf), "MG%03d", (int)(val.f * 60.0f));
        break;

    case RIG_LEVEL_COMP:
        SNPRINTF(levelbuf, sizeof(levelbuf), "CP%03d", (int)(val.f * 40.0f));
        break;

    case RIG_LEVEL_SQL:
        SNPRINTF(levelbuf, sizeof(levelbuf), "SQ%03d", (int)(val.f * 29.0f));
        break;

    case RIG_LEVEL_AF:
        SNPRINTF(levelbuf, sizeof(levelbuf), "AG%03d", (int)(val.f * 250.0f));
        break;

    case RIG_LEVEL_RF:
        SNPRINTF(levelbuf, sizeof(levelbuf), "RG%03d", (int)(val.f * 250.0f));
        break;

    case RIG_LEVEL_NR:
        return k3_set_nb_level(rig, val.f, -1);

    case RIG_LEVEL_MONITOR_GAIN:
        SNPRINTF(levelbuf, sizeof(levelbuf), "ML%03d", (int)(val.f * 60.0f));
        break;

    case RIG_LEVEL_RFPOWER:
        kenwood_val = (int)(val.f * k3_get_maxpower(rig));
        SNPRINTF(levelbuf, sizeof(levelbuf), "PC%03d%s", kenwood_val,
                 kenwood_val > 15 ? "1" : "0");
        break;

    default:
        return kenwood_set_level(rig, vfo, level, val);
    }

    return kenwood_transaction(rig, levelbuf, NULL, 0);
}

/*
 * Handle S-meter (SM, SMH) level locally and pass rest to kenwood_get_level()
 */
int k3_get_level(RIG *rig, vfo_t vfo, setting_t level, value_t *val)
{
    char levelbuf[16];
    int retval;
    int lvl;
    size_t len;
    struct kenwood_priv_data *priv = rig->state.priv;

    rig_debug(RIG_DEBUG_VERBOSE, "%s called\n", __func__);

    if (!val)
    {
        return -RIG_EINVAL;
    }

    switch (level)
    {
        float firmware_have;
        float firmware_need;

    case RIG_LEVEL_STRENGTH:
        /* As of FW rev 4.37 the K3 supports an 'SMH' command that
         * offers a higher resolution, 0-100 (mine went to 106),
         * rawstr value for more precise S-meter reporting.
         */
        firmware_have = 0;

        if (priv->fw_rev != NULL) { sscanf(priv->fw_rev, "%f", &firmware_have); }

        sscanf("4.37", "%f", &firmware_need);

        if (firmware_have < firmware_need)
        {
            cal_table_t str_cal = K3_SM_CAL;

            retval = kenwood_safe_transaction(rig, "SM", levelbuf, sizeof(levelbuf), 6);

            if (retval != RIG_OK)
            {
                return retval;
            }

            sscanf(levelbuf + 2, "%d", &val->i);  /* rawstr */

            val->i = (int) rig_raw2val(val->i, &str_cal);
        }
        else
        {
            cal_table_t str_cal = K3_SMH_CAL;

            retval = kenwood_safe_transaction(rig, "SMH", levelbuf, sizeof(levelbuf), 6);

            if (retval != RIG_OK)
            {
                return retval;
            }

            sscanf(levelbuf + 3, "%d", &val->i);  /* rawstr */

            val->i = (int) rig_raw2val(val->i, &str_cal);
        }

        break;

    case RIG_LEVEL_ALC:
    {
        int tx_mode;
        float alc;

        retval = k3_get_bar_graph_level(rig, NULL, NULL, &alc, &tx_mode);

        if (retval != RIG_OK)
        {
            return retval;
        }

        if (!tx_mode)
        {
            val->f = 0.0f;
            return RIG_OK;
        }

        if (alc < 0)
        {
            return -RIG_EINVAL;
        }

        val->f = alc;
        break;
    }

    case RIG_LEVEL_RFPOWER_METER:
    case RIG_LEVEL_RFPOWER_METER_WATTS:
    {
        int tx_mode;
        float pwr;

        retval = k3_get_bar_graph_level(rig, NULL, &pwr, NULL, &tx_mode);

        if (retval != RIG_OK)
        {
            return retval;
        }

        if (!tx_mode)
        {
            val->f = 0.0f;
            return RIG_OK;
        }

        if (pwr < 0)
        {
            return -RIG_EINVAL;
        }

        val->f = pwr;

        if (level == RIG_LEVEL_RFPOWER_METER_WATTS)
        {
            val->f *= 100;
        }

        break;
    }

    case RIG_LEVEL_AGC:
        retval = kenwood_safe_transaction(rig, "GT", levelbuf, sizeof(levelbuf), 5);

        if (retval != RIG_OK)
        {
            return retval;
        }

        sscanf(levelbuf + 2, "%d", &lvl);

        if (lvl == 0)
        {
            val->i = RIG_AGC_OFF;
        }
        else if (lvl == 2)
        {
            val->i = RIG_AGC_FAST;
        }
        else if (lvl == 4)
        {
            val->i = RIG_AGC_SLOW;
        }
        else
        {
            return -RIG_EPROTO;
        }

        break;

    case RIG_LEVEL_ATT:
        retval = kenwood_safe_transaction(rig, "RA", levelbuf, sizeof(levelbuf), 4);

        if (retval != RIG_OK)
        {
            return retval;
        }

        sscanf(levelbuf + 2, "%d", &lvl);

        if (lvl == 0)
        {
            val->i = 0;
        }
        else if (lvl == 1)
        {
            val->i = 10;
        }
        else
        {
            int i;

            for (i = 0; i < lvl && i < HAMLIB_MAXDBLSTSIZ; i++)
            {
                if (rig->state.attenuator[i] == 0)
                {
                    rig_debug(RIG_DEBUG_ERR, "%s: unexpected att level %d\n", __func__, lvl);
                    return -RIG_EPROTO;
                }
            }

            if (i != lvl)
            {
                return -RIG_EINTERNAL;
            }

            val->i = rig->state.attenuator[i - 1];
        }

        break;

    case RIG_LEVEL_MICGAIN:
        retval = kenwood_safe_transaction(rig, "MG", levelbuf, sizeof(levelbuf), 5);

        if (retval != RIG_OK)
        {
            return retval;
        }

        sscanf(levelbuf + 2, "%d", &lvl);
        val->f = (float) lvl / 60.0f;
        break;

    case RIG_LEVEL_COMP:
        retval = kenwood_safe_transaction(rig, "CP", levelbuf, sizeof(levelbuf), 5);

        if (retval != RIG_OK)
        {
            return retval;
        }

        sscanf(levelbuf + 2, "%d", &lvl);
        val->f = (float) lvl / 40.0f;
        break;

    case RIG_LEVEL_SQL:
        retval = kenwood_safe_transaction(rig, "SQ", levelbuf, sizeof(levelbuf), 5);

        if (retval != RIG_OK)
        {
            return retval;
        }

        sscanf(levelbuf + 2, "%d", &lvl);
        val->f = (float) lvl / 29.0f;
        break;

    case RIG_LEVEL_RF:
        retval = kenwood_safe_transaction(rig, "RG", levelbuf, sizeof(levelbuf), 5);

        if (retval != RIG_OK)
        {
            return retval;
        }

        sscanf(levelbuf + 2, "%d", &lvl);
        val->f = (float) lvl / 250.0f;
        break;

    case RIG_LEVEL_AF:
        retval = kenwood_safe_transaction(rig, "AG", levelbuf, sizeof(levelbuf), 5);

        if (retval != RIG_OK)
        {
            return retval;
        }

        sscanf(levelbuf + 2, "%d", &lvl);
        val->f = (float) lvl / 250.0f;
        break;

    case RIG_LEVEL_NR:
        return k3_get_nb_level(rig, &val->f, NULL);

    case RIG_LEVEL_MONITOR_GAIN:
        retval = kenwood_safe_transaction(rig, "ML", levelbuf, sizeof(levelbuf), 5);

        if (retval != RIG_OK)
        {
            return retval;
        }

        sscanf(levelbuf + 2, "%d", &lvl);
        val->f = (float) lvl / 60.0f;
        break;

    case RIG_LEVEL_RFPOWER:
        retval = kenwood_transaction(rig, "PC", levelbuf, sizeof(levelbuf));

        if (retval != RIG_OK)
        {
            return retval;
        }

        len = strlen(levelbuf);

        if (len == 5 || len == 6)
        {
            sscanf(levelbuf + 2, "%3d", &lvl);
        }
        else
        {
            return RIG_EPROTO;
        }

        // extended K22 format PCnnnx where 0=.1W units and 1=1W units
        if (len == 6 && levelbuf[5] == '0')
        {
            val->f = (float) lvl / 10.0 / k3_get_maxpower(rig);
        }
        else
        {
            val->f = (float) lvl / k3_get_maxpower(rig);
        }

        break;

    default:
        return kenwood_get_level(rig, vfo, level, val);
    }

    return RIG_OK;
}

int kx3_set_level(RIG *rig, vfo_t vfo, setting_t level, value_t val)
{
    int ival;
    char cmdbuf[32];

    rig_debug(RIG_DEBUG_VERBOSE, "%s called\n", __func__);

    if (val.f > 1.0 || val.f < 0) { return -RIG_EINVAL; }

    switch (level)
    {
    case RIG_LEVEL_RF:
        ival = val.f * (250.0 - 190.0) + 190.0;
        SNPRINTF(cmdbuf, sizeof(cmdbuf) - 1, "RG%03d", ival);
        return kenwood_transaction(rig, cmdbuf, NULL, 0);

    case RIG_LEVEL_AF:
        // manual says 0-255 as of Rev G5 but experiment says 0-60
        SNPRINTF(cmdbuf, sizeof(cmdbuf), "AG%03d", (int)(val.f * 60.0f));
        return kenwood_transaction(rig, cmdbuf, NULL, 0);

    case RIG_LEVEL_MICGAIN:
        // manual says 0-255 as of Rev G5 but experiment says 0-80
        SNPRINTF(cmdbuf, sizeof(cmdbuf), "MG%03d", (int)(val.f * 80.0f));
        return kenwood_transaction(rig, cmdbuf, NULL, 0);
    }

    return k3_set_level(rig, vfo, level, val);
}

int kx3_get_level(RIG *rig, vfo_t vfo, setting_t level, value_t *val)
{
    int retval;

    rig_debug(RIG_DEBUG_VERBOSE, "%s called\n", __func__);

    switch (level)
    {
    case RIG_LEVEL_AF:
        retval = get_kenwood_level(rig, "AG", NULL, &val->i);

        if (retval != RIG_OK) { return retval; }

        // manual says 0-255 as of Rev G5 but experiment says 0-60
        val->f = val->i / 60.0;
        return retval;

    case RIG_LEVEL_RF:
        retval = get_kenwood_level(rig, "RG", NULL, &val->i);

        if (retval != RIG_OK) { return retval; }

        val->f = (val->i - 190.0) / (250.0 - 190.0);
        return retval;

    case RIG_LEVEL_MICGAIN:
        retval = get_kenwood_level(rig, "MG", NULL, &val->i);

        if (retval != RIG_OK) { return retval; }

        val->f = val->i / 80.0;
        return retval;

    case RIG_LEVEL_RFPOWER_METER:
    {
        int tx_status = 0;
        float pwr;

        // Return zero RF power when not in TX mode
        retval = get_kenwood_func(rig, "TQ", &tx_status);

        if (retval != RIG_OK)
        {
            return retval;
        }

        if (!tx_status)
        {
            val->f = 0.0f;
            return RIG_OK;
        }

        retval = kx3_get_bar_graph_level(rig, &pwr);

        if (retval != RIG_OK)
        {
            return retval;
        }

        val->f = pwr;
        return retval;
    }

    case RIG_LEVEL_RFPOWER_METER_WATTS:
    {
        struct kenwood_priv_data *priv = rig->state.priv;
        char levelbuf[KENWOOD_MAX_BUF_LEN];
        int pwr;

        retval = kenwood_safe_transaction(rig, "PO", levelbuf, sizeof(levelbuf), 5);

        if (retval != RIG_OK)
        {
            return retval;
        }

        sscanf(levelbuf + 2, "%d", &pwr);
        val->f = priv->has_kpa100 ? pwr : pwr / 10.0;
        return retval;
    }
    }

    return k3_get_level(rig, vfo, level, val);
}


int k3_set_func(RIG *rig, vfo_t vfo, setting_t func, int status)
{
    char buf[10];

    rig_debug(RIG_DEBUG_VERBOSE, "%s called\n", __func__);

    switch (func)
    {
    case RIG_FUNC_APF:
        SNPRINTF(buf, sizeof(buf), "AP%c", (status == 0) ? '0' : '1');
        break;

    case RIG_FUNC_DUAL_WATCH:
        SNPRINTF(buf, sizeof(buf), "SB%c", (status == 0) ? '0' : '1');
        break;

    case RIG_FUNC_DIVERSITY:
        SNPRINTF(buf, sizeof(buf), "DV%c", (status == 0) ? '0' : '1');
        break;

    case RIG_FUNC_SEND_MORSE:
        // Transmit a CW character - K3 does not return any response
        snprintf(buf, sizeof(buf), "KYW%c", status);
        break;

    case RIG_FUNC_MUTE: // K4 Only that we know of
        SNPRINTF(buf, sizeof(buf), "AG%c", (status == 0) ? '/' : '0');
        break;

    default:
        return kenwood_set_func(rig, vfo, func, status);
    }

    return kenwood_transaction(rig, buf, NULL, 0);
}

/*
 * Some functions, notably RIT and XIT On/Off status, can be queried
 * on the K3.  Those functions are handled here and others are passed
 * through to kenwood_get_func().
 */
int k3_get_func(RIG *rig, vfo_t vfo, setting_t func, int *status)
{
    rig_debug(RIG_DEBUG_VERBOSE, "%s called\n", __func__);

    if (!status)
    {
        return -RIG_EINVAL;
    }

    switch (func)
    {
    case RIG_FUNC_RIT:
        return get_kenwood_func(rig, "RT", status);

    case RIG_FUNC_XIT:
        return get_kenwood_func(rig, "XT", status);

    case RIG_FUNC_APF:
        return get_kenwood_func(rig, "AP", status);

    case RIG_FUNC_DUAL_WATCH:
        return get_kenwood_func(rig, "SB", status);

    case RIG_FUNC_DIVERSITY:
        return get_kenwood_func(rig, "DV", status);

    default:
        return kenwood_get_func(rig, vfo, func, status);
    }
}

/* Private K3 helper functions */

/*
 * set_rit_xit() -- Differs from from generic Kenwood function as K3 can set
 * RIT/XIT to an arbitrary offset.  When rit == 0, the RIT/XIT offset is
 * cleared.
 */
int set_rit_xit(RIG *rig, shortfreq_t rit)
{
    int err;

    rig_debug(RIG_DEBUG_VERBOSE, "%s called\n", __func__);

    if (rit == 0)
    {
        /* Clear offset and return */
        err = kenwood_transaction(rig, "RC", NULL, 0);

        if (err != RIG_OK)
        {
            return err;
        }

        return RIG_OK;
    }

    /* Set offset */
    if (rit <= 9999 && rit >= -9999)
    {
        char cmd[16];
        char offs;
        offs = (rit < 0) ? '-' : '+';
        SNPRINTF(cmd, 8, "RO%c%04d", offs, abs((int)rit));

        err = kenwood_transaction(rig, cmd, NULL, 0);

        if (err != RIG_OK)
        {
            return err;
        }

    }
    else
    {
        return -RIG_EINVAL;
    }

    return RIG_OK;
}

int k3_set_nb_level(RIG *rig, float dsp_nb, float if_nb)
{
    char levelbuf[16];
    int dsp_nb_raw = 0;
    int if_nb_raw = 0;

    rig_debug(RIG_DEBUG_VERBOSE, "%s called\n", __func__);

    if (dsp_nb >= 0)
    {
        dsp_nb_raw = (int)(dsp_nb * 21.0f);
    }

    if (if_nb >= 0)
    {
        if_nb_raw = (int)(if_nb * 21.0f);
    }

    if (dsp_nb < 0 || if_nb < 0)
    {
        int current_dsp_nb_raw;
        int current_if_nb_raw;

        int retval = kenwood_safe_transaction(rig, "NL", levelbuf, sizeof(levelbuf), 6);

        if (retval != RIG_OK)
        {
            return retval;
        }

        sscanf(levelbuf + 2, "%02d%02d", &current_dsp_nb_raw, &current_if_nb_raw);

        if (dsp_nb < 0)
        {
            dsp_nb_raw = current_dsp_nb_raw;
        }

        if (if_nb < 0)
        {
            if_nb_raw = current_if_nb_raw;
        }
    }

    SNPRINTF(levelbuf, sizeof(levelbuf), "NL%02d%02d", dsp_nb_raw, if_nb_raw);

    return kenwood_transaction(rig, levelbuf, NULL, 0);
}

int k3_get_nb_level(RIG *rig, float *dsp_nb, float *if_nb)
{
    char levelbuf[16];
    int retval;
    int dsp_nb_raw;
    int if_nb_raw;

    rig_debug(RIG_DEBUG_VERBOSE, "%s called\n", __func__);

    retval = kenwood_safe_transaction(rig, "NL", levelbuf, sizeof(levelbuf), 6);

    if (retval != RIG_OK)
    {
        return retval;
    }

    sscanf(levelbuf + 2, "%02d%02d", &dsp_nb_raw, &if_nb_raw);

    if (dsp_nb != NULL)
    {
        *dsp_nb = (float) dsp_nb_raw / 21.0f;
    }

    if (if_nb != NULL)
    {
        *if_nb = (float) if_nb_raw / 21.0f;
    }

    return RIG_OK;
}

int k3_get_bar_graph_level(RIG *rig, float *smeter, float *pwr, float *alc,
                           int *mode_tx)
{
    char levelbuf[16];
    int retval;
    int tm_raw;
    int bg_raw;
    char mode;

    rig_debug(RIG_DEBUG_VERBOSE, "%s called\n", __func__);

    // Determine transmit metering mode: 0 = RF POWER, 1 = ALC
    retval = get_kenwood_func(rig, "TM", &tm_raw);

    if (retval != RIG_OK)
    {
        return retval;
    }

    retval = kenwood_safe_transaction(rig, "BG", levelbuf, sizeof(levelbuf), 5);

    if (retval != RIG_OK)
    {
        return retval;
    }

    sscanf(levelbuf + 2, "%02d%c", &bg_raw, &mode);

    if (mode == 'R')
    {
        // S-meter: nn is 00 - 21 (CWT off) or 00 - 09 (CWT on)
        if (smeter != NULL)
        {
            *smeter = (float) bg_raw / 21.0f;
        }

        if (pwr != NULL)
        {
            *pwr = -1;
        }

        if (alc != NULL)
        {
            *alc = -1;
        }
    }
    else if (mode == 'T')
    {
        if (tm_raw)
        {
            // ALC: nn is 00 - 07
            if (alc != NULL)
            {
                *alc = (float) bg_raw / 7.0f;
            }

            if (pwr != NULL)
            {
                *pwr = -1;
            }

            if (smeter != NULL)
            {
                *smeter = -1;
            }
        }
        else
        {
            // PWR: nn is 00 - 12
            if (pwr != NULL)
            {
                *pwr = (float) bg_raw / 12.0f;
            }

            if (alc != NULL)
            {
                *alc = -1;
            }

            if (smeter != NULL)
            {
                *smeter = -1;
            }
        }
    }
    else
    {
        return -RIG_EPROTO;
    }

    if (mode_tx != NULL)
    {
        *mode_tx = (mode == 'T');
    }

    return RIG_OK;
}

int kx3_get_bar_graph_level(RIG *rig, float *level)
{
    char levelbuf[16];
    int retval;
    int bg_raw;

    rig_debug(RIG_DEBUG_VERBOSE, "%s called\n", __func__);

    retval = kenwood_safe_transaction(rig, "BG", levelbuf, sizeof(levelbuf), 4);

    if (retval != RIG_OK)
    {
        return retval;
    }

    sscanf(levelbuf + 2, "%02d", &bg_raw);

    if (bg_raw >= 0 && bg_raw <= 10)
    {
        if (level != NULL)
        {
            *level = (float) bg_raw / 10.0f;
        }
    }
    else if (bg_raw >= 12 && bg_raw <= 22)
    {
        if (level != NULL)
        {
            *level = (float)(bg_raw - 12) / 10.0f;
        }
    }
    else
    {
        return -RIG_EPROTO;
    }

    return RIG_OK;
}

int k4_get_ptt(RIG *rig, vfo_t vfo, ptt_t *ptt)
{
    char pttbuf[6];
    int retval;

    rig_debug(RIG_DEBUG_VERBOSE, "%s called\n", __func__);

    if (!ptt)
    {
        return -RIG_EINVAL;
    }

    retval = kenwood_safe_transaction(rig, "TQ", pttbuf, 6, 3);

    if (retval != RIG_OK)
    {
        return retval;
    }

    *ptt = pttbuf[2] == '1' ? RIG_PTT_ON : RIG_PTT_OFF;
    // we're not caching this for now
    return RIG_OK;
}


// The K4 has a problem in Fake It mode where the FA command is ignored
// We will use it's special TQ command to try and ensure PTT is really off
int k4_set_ptt(RIG *rig, vfo_t vfo, ptt_t ptt)
{
    char pttbuf[6];
    int i;
    int retval;
    ptt_t ptt2 = -1;
    char cmd[4];

    rig_debug(RIG_DEBUG_VERBOSE, "%s called\n", __func__);

    SNPRINTF(cmd, sizeof(cmd), "RX");

    if (ptt) { cmd[0] = 'T'; }

    retval = kenwood_transaction(rig, cmd, NULL, 0);

    if (retval != RIG_OK)
    {
        return retval;
    }

    for (i = 0; i < 5 && ptt2 != ptt; ++i)
    {
        retval = kenwood_safe_transaction(rig, "TQ", pttbuf, 6, 3);

        if (retval != RIG_OK)
        {
            return retval;
        }

        ptt2 = pttbuf[2] == '1' ? RIG_PTT_ON : RIG_PTT_OFF;

        if (ptt2 != ptt)
        {
            hl_usleep(100 * 1000);
            rig_debug(RIG_DEBUG_TRACE, "%s: ptt=%d, expected=%d\n", __func__, ptt2, ptt);
        }
    }

    // had one report of Fake It not returning to RX freq after TX -- so a little more time for the K4
    if (ptt == RIG_PTT_OFF) { hl_usleep(100 * 1000); }

    return RIG_OK;
}

// K3S band memory needs some time to do it's thing after freq change
// K3 probably does too
// But what about the K4?
int k3_set_freq(RIG *rig, vfo_t vfo, freq_t freq)
{
    int retval;
    freq_t tfreq;
    retval = kenwood_get_freq(rig, vfo, &tfreq);

    if (retval != RIG_OK) { return retval; }

    retval = kenwood_set_freq(rig, vfo, freq);

    // if more than 1MHz probably a band change so give it some time
    // before continuing
    if (fabs(tfreq - freq) > 1e6)
    {
        hl_usleep(200 * 1000);    // give 200ms for rig to do band switch if needed
    }

    return retval;
}
