/*
 *  Hamlib Kenwood backend - TM-D710(G) description
 *  Copyright (c) 2011 by Charles Suprin
 *  Copyright (c) 2018 Mikael Nousiainen
 *
 *  Command set specification available in:
 *  - https://github.com/LA3QMA/TM-V71_TM-D710-Kenwood
 *  - http://kd7dvd.us/equipment/tm-d710a/manuals/control_commands.pdf
 *
 *  Limitations:
 *  - It is possible to set frequency only inside the selected frequency band of the current VFO,
 *    since there is no command to change the frequency band of a VFO
 *
 *  Features not yet implemented:
 *  - DTMF send
 *  - Tone burst frequency setting
 *  - Call channel settings
 *  - Change between dual-band/single-band mode
 *  - Several miscellaneous settings available via the MU menu command, which could be exposed via extparms/extlevels
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

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <stdlib.h>
#include <math.h>
#include <ctype.h>

#include "hamlib/rig.h"
#include "kenwood.h"
#include "th.h"
#include "tones.h"
#include "num_stdio.h"
#include "misc.h"

static int tmd710_open(RIG *rig);
static int tmd710_do_get_freq(RIG *rig, vfo_t vfo, freq_t *freq);
static int tmd710_do_set_freq(RIG *rig, vfo_t vfo, freq_t freq);
static int tmd710_get_freq(RIG *rig, vfo_t vfo, freq_t *freq);
static int tmd710_set_freq(RIG *rig, vfo_t vfo, freq_t freq);
static int tmd710_get_split_freq(RIG *rig, vfo_t vfo, freq_t *freq);
static int tmd710_set_split_freq(RIG *rig, vfo_t vfo, freq_t freq);
static int tmd710_set_vfo(RIG *rig, vfo_t vfo);
static int tmd710_get_vfo(RIG *rig, vfo_t *vfo);
static int tmd710_set_split_vfo(RIG *rig, vfo_t vfo, split_t split,
                                vfo_t txvfo);
static int tmd710_get_split_vfo(RIG *rig, vfo_t vfo, split_t *split,
                                vfo_t *txvfo);
static int tmd710_set_ts(RIG *rig, vfo_t vfo, shortfreq_t ts);
static int tmd710_get_ts(RIG *rig, vfo_t vfo, shortfreq_t *ts);
static int tmd710_set_ctcss_tone(RIG *rig, vfo_t vfo, tone_t tone);
static int tmd710_get_ctcss_tone(RIG *rig, vfo_t vfo, tone_t *tone);
static int tmd710_set_ctcss_sql(RIG *rig, vfo_t vfo, tone_t tone);
static int tmd710_get_ctcss_sql(RIG *rig, vfo_t vfo, tone_t *tone);
static int tmd710_set_mode(RIG *rig, vfo_t vfo, rmode_t mode, pbwidth_t width);
static int tmd710_get_mode(RIG *rig, vfo_t vfo, rmode_t *mode,
                           pbwidth_t *width);
static int tmd710_set_rptr_shift(RIG *rig, vfo_t vfo, rptr_shift_t shift);
static int tmd710_get_rptr_shift(RIG *rig, vfo_t vfo, rptr_shift_t *shift);
static int tmd710_set_rptr_offs(RIG *rig, vfo_t vfo, shortfreq_t offset);
static int tmd710_get_rptr_offs(RIG *rig, vfo_t vfo, shortfreq_t *offset);
static int tmd710_get_mem(RIG *rig, vfo_t vfo, int *ch);
static int tmd710_set_mem(RIG *rig, vfo_t vfo, int ch);
static int tmd710_set_dcs_sql(RIG *rig, vfo_t vfo, tone_t code);
static int tmd710_get_dcs_sql(RIG *rig, vfo_t vfo, tone_t *code);
static int tmd710_set_channel(RIG *rig, vfo_t vfo, const channel_t *chan);
static int tmd710_get_channel(RIG *rig, vfo_t vfo, channel_t *chan,
                              int read_only);
static int tmd710_set_ptt(RIG *rig, vfo_t vfo, ptt_t ptt);
static int tmd710_get_dcd(RIG *rig, vfo_t vfo, dcd_t *dcd);
static int tmd710_vfo_op(RIG *rig, vfo_t vfo, vfo_op_t op);
static int tmd710_set_level(RIG *rig, vfo_t vfo, setting_t level, value_t val);
static int tmd710_get_level(RIG *rig, vfo_t vfo, setting_t level, value_t *val);
static int tmd710_get_func(RIG *rig, vfo_t vfo, setting_t func, int *status);
static int tmd710_set_func(RIG *rig, vfo_t vfo, setting_t func, int status);
static int tmd710_get_parm(RIG *rig, setting_t parm, value_t *val);
static int tmd710_set_parm(RIG *rig, setting_t parm, value_t val);
static int tmd710_get_ext_level(RIG *rig, vfo_t vfo, token_t token,
                                value_t *val);
static int tmd710_set_ext_level(RIG *rig, vfo_t vfo, token_t token,
                                value_t val);

#define TMD710_MODES     (RIG_MODE_FM|RIG_MODE_FMN|RIG_MODE_AM)
#define TMD710_MODES_FM  (RIG_MODE_FM|RIG_MODE_FMN)
#define TMD710_MODES_TX  (RIG_MODE_FM|RIG_MODE_FMN)

#define TMD710_FUNC_GET (RIG_FUNC_TSQL|   \
                       RIG_FUNC_TONE|   \
                       RIG_FUNC_REV|    \
                       RIG_FUNC_LOCK|   \
                       RIG_FUNC_ARO|    \
                       RIG_FUNC_AIP|    \
                       RIG_FUNC_RESUME)
#define TMD710_FUNC_SET (RIG_FUNC_TSQL|   \
                       RIG_FUNC_TONE|   \
                       RIG_FUNC_TBURST| \
                       RIG_FUNC_REV|    \
                       RIG_FUNC_LOCK|   \
                       RIG_FUNC_ARO|    \
                       RIG_FUNC_AIP|    \
                       RIG_FUNC_RESUME)

#define TMD710_LEVEL_ALL (RIG_LEVEL_SQL| \
                        RIG_LEVEL_RFPOWER)

#define TMD710_PARMS  (RIG_PARM_BACKLIGHT|\
                        RIG_PARM_BEEP|\
                        RIG_PARM_APO)

#define TMD710_VFO_OP (RIG_OP_UP|RIG_OP_DOWN)

#define TMD710_CHANNEL_CAPS   \
            TH_CHANNEL_CAPS,\
            .flags=1,   \
            .dcs_code=1,    \
            .dcs_sql=1,

#define TMD710_CHANNEL_CAPS_WO_LO \
            TH_CHANNEL_CAPS,\
            .dcs_code=1,    \
            .dcs_sql=1,

#define TOKEN_BACKEND(t) (t)

#define TOK_LEVEL_EXT_DATA_BAND TOKEN_BACKEND(100)

// TM-D710 protocol definitions

#define TMD710_BAND_A 0
#define TMD710_BAND_B 1

#define TMD710_BAND_MODE_VFO 0
#define TMD710_BAND_MODE_MEMORY 1
#define TMD710_BAND_MODE_CALL 2
#define TMD710_BAND_MODE_WX 3

#define TMD710_RF_POWER_MIN 0
#define TMD710_RF_POWER_MAX 2

#define TMD710_SQL_MIN 0
#define TMD710_SQL_MAX 0x1F

// TM-D710 MU command value tables

#define TMD710_ANNOUNCE_OFF 0
#define TMD710_ANNOUNCE_AUTO 1
#define TMD710_ANNOUNCE_MANUAL 2

#define TMD710_LANGUAGE_ENGLISH 0
#define TMD710_LANGUAGE_JAPANESE 1

#define TMD710_SMETER_HANG_UP_TIME_OFF 0
#define TMD710_SMETER_HANG_UP_TIME_125 1
#define TMD710_SMETER_HANG_UP_TIME_250 2
#define TMD710_SMETER_HANG_UP_TIME_500 3

#define TMD710_MUTE_HANG_UP_TIME_OFF 0
#define TMD710_MUTE_HANG_UP_TIME_125 1
#define TMD710_MUTE_HANG_UP_TIME_250 2
#define TMD710_MUTE_HANG_UP_TIME_500 3
#define TMD710_MUTE_HANG_UP_TIME_750 4
#define TMD710_MUTE_HANG_UP_TIME_1000 5

#define TMD710_TIMEOUT_TIMER_3MIN 0
#define TMD710_TIMEOUT_TIMER_5MIN 1
#define TMD710_TIMEOUT_TIMER_10MIN 2

#define TMD710_RECALL_METHOD_ALL 0
#define TMD710_RECALL_METHOD_CURRENT 1

#define TMD710_ECHOLINK_SPEED_FAST 0
#define TMD710_ECHOLINK_SPEED_SLOW 1

#define TMD710_DTMF_SPEED_FAST 0
#define TMD710_DTMF_SPEED_SLOW 1

#define TMD710_DTMF_PAUSE_100 0
#define TMD710_DTMF_PAUSE_250 1
#define TMD710_DTMF_PAUSE_500 2
#define TMD710_DTMF_PAUSE_750 3
#define TMD710_DTMF_PAUSE_1000 4
#define TMD710_DTMF_PAUSE_1500 5
#define TMD710_DTMF_PAUSE_2000 6

#define TMD710_BACKLIGHT_COLOR_AMBER 0
#define TMD710_BACKLIGHT_COLOR_GREEN 1

#define TMD710_SCAN_RESUME_TIME 0
#define TMD710_SCAN_RESUME_CARRIER 1
#define TMD710_SCAN_RESUME_SEEK 2

#define TMD710_AUTO_POWER_OFF_OFF 0
#define TMD710_AUTO_POWER_OFF_30MIN 1
#define TMD710_AUTO_POWER_OFF_60MIN 2
#define TMD710_AUTO_POWER_OFF_90MIN 3
#define TMD710_AUTO_POWER_OFF_120MIN 4
#define TMD710_AUTO_POWER_OFF_180MIN 5

#define TMD710_EXT_DATA_BAND_A 0
#define TMD710_EXT_DATA_BAND_B 1
#define TMD710_EXT_DATA_BAND_TXA_RXB 2
#define TMD710_EXT_DATA_BAND_TXB_RXA 3

#define TMD710_EXT_DATA_SPEED_1200 0
#define TMD710_EXT_DATA_SPEED_9600 1

#define TMD710_SQC_SOURCE_OFF 0
#define TMD710_SQC_SOURCE_BUSY 1
#define TMD710_SQC_SOURCE_SQL 2
#define TMD710_SQC_SOURCE_TX 3
#define TMD710_SQC_SOURCE_BUSY_OR_TX 4
#define TMD710_SQC_SOURCE_SQL_OR_TX 5

static rmode_t tmd710_mode_table[KENWOOD_MODE_TABLE_MAX] =
{
    [0] = RIG_MODE_FM,
    [1] = RIG_MODE_FMN,
    [2] = RIG_MODE_AM,
};

static struct kenwood_priv_caps  tmd710_priv_caps  =
{
    .cmdtrm =  EOM_TH,   /* Command termination character */
    .mode_table = tmd710_mode_table,
};

/* Private TM-D710 extra levels definitions
 *
 * Token definitions for .cfgparams in rig_caps
 * See enum rig_conf_e and struct confparams in rig.h
 */
const struct confparams tmd710_ext_levels[] =
{
    {
        TOK_LEVEL_EXT_DATA_BAND, "EXTDATABAND", "External data band", "External data band",
        NULL, RIG_CONF_COMBO, { .c = { .combostr = { "A", "B", "TXA-RXB", "TXB-RXA", NULL } } }
    },
    { RIG_CONF_END, NULL, }
};

const struct rig_caps tmd710_caps =
{
    .rig_model =  RIG_MODEL_TMD710,
    .model_name = "TM-D710(G)",
    .mfg_name =  "Kenwood",
    .version =  BACKEND_VER ".5",
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

    .has_get_func =  TMD710_FUNC_GET,
    .has_set_func =  TMD710_FUNC_SET,
    .has_get_level =  TMD710_LEVEL_ALL,
    .has_set_level =  RIG_LEVEL_SET(TMD710_LEVEL_ALL),
    .has_get_parm =  TMD710_PARMS,
    .has_set_parm =  TMD710_PARMS,
    .level_gran =
    {
#include "level_gran_kenwood.h"
    },
    .parm_gran =  {},
    .ctcss_list =  kenwood42_ctcss_list,
    .dcs_list =  common_dcs_list,
    .preamp =   {RIG_DBLST_END,},
    .attenuator =   {RIG_DBLST_END,},
    .max_rit =  Hz(0),
    .max_xit =  Hz(0),
    .max_ifshift =  Hz(0),
    .vfo_ops =  TMD710_VFO_OP,
    .scan_ops = RIG_SCAN_NONE,
    .targetable_vfo =  RIG_TARGETABLE_FREQ | RIG_TARGETABLE_MODE,
    .transceive =  RIG_TRN_OFF,
    .bank_qty =   0,
    .chan_desc_sz =  8,

    .chan_list = {
        {0, 199, RIG_MTYPE_MEM, {TMD710_CHANNEL_CAPS}},  /* normal MEM */
        {200, 219, RIG_MTYPE_EDGE, {TMD710_CHANNEL_CAPS}}, /* U/L MEM */
        {221, 222, RIG_MTYPE_CALL, {TMD710_CHANNEL_CAPS_WO_LO}},  /* Call 0/1 */
        RIG_CHAN_END,
    },
    /*
     * TODO: Japan & TM-D700S, and Taiwan models
     */
    .rx_range_list1 =  {
        {MHz(118), MHz(470), TMD710_MODES, -1, -1, RIG_VFO_A | RIG_VFO_MEM},
        {MHz(136), MHz(174), TMD710_MODES_FM, -1, -1, RIG_VFO_A | RIG_VFO_B | RIG_VFO_MEM},
        {MHz(300), MHz(524), TMD710_MODES_FM, -1, -1, RIG_VFO_A | RIG_VFO_B | RIG_VFO_MEM},
        {MHz(800), MHz(1300), TMD710_MODES_FM, -1, -1, RIG_VFO_B | RIG_VFO_MEM},
        RIG_FRNG_END,
    }, /* rx range */
    .tx_range_list1 =  {
        {MHz(144), MHz(146), TMD710_MODES_TX, W(5), W(50), RIG_VFO_A | RIG_VFO_B | RIG_VFO_MEM},
        {MHz(430), MHz(440), TMD710_MODES_TX, W(5), W(35), RIG_VFO_A | RIG_VFO_B | RIG_VFO_MEM},
        RIG_FRNG_END,
    }, /* tx range */

    .rx_range_list2 =  {
        {MHz(118), MHz(470), TMD710_MODES, -1, -1, RIG_VFO_A | RIG_VFO_MEM},
        {MHz(136), MHz(174), TMD710_MODES_FM, -1, -1, RIG_VFO_A | RIG_VFO_B | RIG_VFO_MEM},
        {MHz(300), MHz(524), TMD710_MODES_FM, -1, -1, RIG_VFO_A | RIG_VFO_B | RIG_VFO_MEM},
        {MHz(800), MHz(1300), TMD710_MODES_FM, -1, -1, RIG_VFO_B | RIG_VFO_MEM},  /* TODO: cellular blocked */
        RIG_FRNG_END,
    }, /* rx range */
    .tx_range_list2 =  {
        {MHz(144), MHz(148), TMD710_MODES_TX, W(5), W(50), RIG_VFO_A | RIG_VFO_B | RIG_VFO_MEM},
        {MHz(430), MHz(450), TMD710_MODES_TX, W(5), W(35), RIG_VFO_A | RIG_VFO_B | RIG_VFO_MEM},
        RIG_FRNG_END,
    }, /* tx range */

    .tuning_steps =  {
        {TMD710_MODES, kHz(5)},
        {TMD710_MODES, kHz(6.25)},
        {TMD710_MODES, kHz(8.33)},
        {TMD710_MODES, kHz(10)},
        {TMD710_MODES, kHz(12.5)},
        {TMD710_MODES, kHz(15)},
        {TMD710_MODES, kHz(20)},
        {TMD710_MODES, kHz(25)},
        {TMD710_MODES, kHz(30)},
        {TMD710_MODES, kHz(50)},
        {TMD710_MODES, kHz(100)},
        RIG_TS_END,
    },
    /* mode/filter list, remember: order matters! */
    .filters =  {
        {RIG_MODE_FM, kHz(15)},
        {RIG_MODE_FMN, kHz(5)},
        {RIG_MODE_AM, kHz(4)},
        RIG_FLT_END,
    },
    .priv = (void *)& tmd710_priv_caps,

    .rig_init = kenwood_init,
    .rig_open = tmd710_open,
    .rig_cleanup = kenwood_cleanup,
    .set_freq =  tmd710_set_freq,
    .get_freq =  tmd710_get_freq,
    .set_split_freq =  tmd710_set_split_freq,
    .get_split_freq =  tmd710_get_split_freq,
    .set_mode =  tmd710_set_mode,
    .get_mode =  tmd710_get_mode,
    .set_vfo =  tmd710_set_vfo,
    .get_vfo =  tmd710_get_vfo,
    .set_ts = tmd710_set_ts,
    .get_ts = tmd710_get_ts,
    .set_ctcss_tone =  tmd710_set_ctcss_tone,
    .get_ctcss_tone =  tmd710_get_ctcss_tone,
    .set_ctcss_sql =  tmd710_set_ctcss_sql,
    .get_ctcss_sql =  tmd710_get_ctcss_sql,
    .set_split_vfo =  tmd710_set_split_vfo,
    .get_split_vfo =  tmd710_get_split_vfo,
    .set_dcs_sql =  tmd710_set_dcs_sql,
    .get_dcs_sql =  tmd710_get_dcs_sql,
    .set_mem =  tmd710_set_mem,
    .get_mem =  tmd710_get_mem,
    .set_channel =  tmd710_set_channel,
    .get_channel =  tmd710_get_channel,
    //.set_trn =  th_set_trn,
    //.get_trn =  th_get_trn,

    .set_func =  tmd710_set_func,
    .get_func =  tmd710_get_func,
    .set_level =  tmd710_set_level,
    .get_level =  tmd710_get_level,
    .set_parm =  tmd710_set_parm,
    .get_parm =  tmd710_get_parm,
    //.get_info =  th_get_info,
    .get_dcd =  tmd710_get_dcd,
    .set_ptt =  tmd710_set_ptt,
    .vfo_op =  tmd710_vfo_op,
    //.scan   =  th_scan,
    .set_ext_level = tmd710_set_ext_level,
    .get_ext_level = tmd710_get_ext_level,

    .extlevels = tmd710_ext_levels,

    .set_rptr_shift = tmd710_set_rptr_shift,
    .get_rptr_shift = tmd710_get_rptr_shift,
    .set_rptr_offs = tmd710_set_rptr_offs,
    .get_rptr_offs = tmd710_get_rptr_offs,

    .decode_event =  th_decode_event,
};

const struct rig_caps tmv71_caps =
{
    RIG_MODEL(RIG_MODEL_TMV71),
    .model_name = "TM-V71(A)",
    .mfg_name =  "Kenwood",
    .version =  BACKEND_VER ".1",
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

    .has_get_func =  TMD710_FUNC_GET,
    .has_set_func =  TMD710_FUNC_SET,
    .has_get_level =  TMD710_LEVEL_ALL,
    .has_set_level =  RIG_LEVEL_SET(TMD710_LEVEL_ALL),
    .has_get_parm =  TMD710_PARMS,
    .has_set_parm =  TMD710_PARMS,
    .level_gran =
    {
#include "level_gran_kenwood.h"
    },
    .parm_gran =  {},
    .ctcss_list =  kenwood42_ctcss_list,
    .dcs_list =  common_dcs_list,
    .preamp =   {RIG_DBLST_END,},
    .attenuator =   {RIG_DBLST_END,},
    .max_rit =  Hz(0),
    .max_xit =  Hz(0),
    .max_ifshift =  Hz(0),
    .vfo_ops =  TMD710_VFO_OP,
    .scan_ops = RIG_SCAN_NONE,
    .targetable_vfo =  RIG_TARGETABLE_NONE,
    .transceive =  RIG_TRN_OFF,
    .bank_qty =   0,
    .chan_desc_sz =  8,

    .chan_list = {
        {0, 199, RIG_MTYPE_MEM, {TMD710_CHANNEL_CAPS}},  /* normal MEM */
        {200, 219, RIG_MTYPE_EDGE, {TMD710_CHANNEL_CAPS}}, /* U/L MEM */
        {221, 222, RIG_MTYPE_CALL, {TMD710_CHANNEL_CAPS_WO_LO}},  /* Call 0/1 */
        RIG_CHAN_END,
    },
    /*
     * TODO: Japan & TM-D700S, and Taiwan models
     */
    .rx_range_list1 =  {
        {MHz(118), MHz(470), TMD710_MODES, -1, -1, RIG_VFO_A | RIG_VFO_MEM},
        {MHz(136), MHz(174), TMD710_MODES_FM, -1, -1, RIG_VFO_A | RIG_VFO_B | RIG_VFO_MEM},
        {MHz(300), MHz(524), TMD710_MODES_FM, -1, -1, RIG_VFO_A | RIG_VFO_B | RIG_VFO_MEM},
        {MHz(800), MHz(1300), TMD710_MODES_FM, -1, -1, RIG_VFO_B | RIG_VFO_MEM},
        RIG_FRNG_END,
    }, /* rx range */
    .tx_range_list1 =  {
        {MHz(144), MHz(146), TMD710_MODES_TX, W(5), W(50), RIG_VFO_A | RIG_VFO_B | RIG_VFO_MEM},
        {MHz(430), MHz(440), TMD710_MODES_TX, W(5), W(35), RIG_VFO_A | RIG_VFO_B | RIG_VFO_MEM},
        RIG_FRNG_END,
    }, /* tx range */

    .rx_range_list2 =  {
        {MHz(118), MHz(470), TMD710_MODES, -1, -1, RIG_VFO_A | RIG_VFO_MEM},
        {MHz(136), MHz(174), TMD710_MODES_FM, -1, -1, RIG_VFO_A | RIG_VFO_B | RIG_VFO_MEM},
        {MHz(300), MHz(524), TMD710_MODES_FM, -1, -1, RIG_VFO_A | RIG_VFO_B | RIG_VFO_MEM},
        {MHz(800), MHz(1300), TMD710_MODES_FM, -1, -1, RIG_VFO_B | RIG_VFO_MEM},  /* TODO: cellular blocked */
        RIG_FRNG_END,
    }, /* rx range */
    .tx_range_list2 =  {
        {MHz(144), MHz(148), TMD710_MODES_TX, W(5), W(50), RIG_VFO_A | RIG_VFO_B | RIG_VFO_MEM},
        {MHz(430), MHz(450), TMD710_MODES_TX, W(5), W(35), RIG_VFO_A | RIG_VFO_B | RIG_VFO_MEM},
        RIG_FRNG_END,
    }, /* tx range */

    .tuning_steps =  {
        {TMD710_MODES, kHz(5)},
        {TMD710_MODES, kHz(6.25)},
        {TMD710_MODES, kHz(8.33)},
        {TMD710_MODES, kHz(10)},
        {TMD710_MODES, kHz(12.5)},
        {TMD710_MODES, kHz(15)},
        {TMD710_MODES, kHz(20)},
        {TMD710_MODES, kHz(25)},
        {TMD710_MODES, kHz(30)},
        {TMD710_MODES, kHz(50)},
        {TMD710_MODES, kHz(100)},
        RIG_TS_END,
    },
    /* mode/filter list, remember: order matters! */
    .filters =  {
        {RIG_MODE_FM, kHz(15)},
        {RIG_MODE_FMN, kHz(5)},
        {RIG_MODE_AM, kHz(4)},
        RIG_FLT_END,
    },
    .priv = (void *)& tmd710_priv_caps,

    .rig_init = kenwood_init,
    .rig_open = kenwood_open,
    .rig_close = kenwood_close,
    .rig_cleanup = kenwood_cleanup,
    .set_freq =  tmd710_set_freq,
    .get_freq =  tmd710_get_freq,
    .set_mode =  tmd710_set_mode,
    .get_mode =  tmd710_get_mode,
    .set_vfo =  tmd710_set_vfo,
    .get_vfo =  tmd710_get_vfo,
    .set_ts = tmd710_set_ts,
    .get_ts = tmd710_get_ts,
    .set_ctcss_tone =  tmd710_set_ctcss_tone,
    .get_ctcss_tone =  tmd710_get_ctcss_tone,
    .set_ctcss_sql =  tmd710_set_ctcss_sql,
    .get_ctcss_sql =  tmd710_get_ctcss_sql,
    //.set_split_vfo =  th_set_split_vfo,
    //.get_split_vfo =  th_get_split_vfo,
    .set_dcs_sql =  tmd710_set_dcs_sql,
    .get_dcs_sql =  tmd710_get_dcs_sql,
    .set_mem =  tmd710_set_mem,
    .get_mem =  tmd710_get_mem,
    .set_channel =  tmd710_set_channel,
    .get_channel =  tmd710_get_channel,
    //.set_trn =  th_set_trn,
    //.get_trn =  th_get_trn,

    .set_func =  tmd710_set_func,
    .get_func =  tmd710_get_func,
    .set_level =  tmd710_set_level,
    .get_level =  tmd710_get_level,
    .set_parm =  tmd710_set_parm,
    .get_parm =  tmd710_get_parm,
    //.get_info =  th_get_info,
    .get_dcd =  tmd710_get_dcd,
    .set_ptt =  tmd710_set_ptt,
    .vfo_op =  tmd710_vfo_op,
    //.scan   =  th_scan,
    .set_ext_level = tmd710_set_ext_level,
    .get_ext_level = tmd710_get_ext_level,

    .extlevels = tmd710_ext_levels,

    .set_rptr_shift = tmd710_set_rptr_shift,
    .get_rptr_shift = tmd710_get_rptr_shift,
    .set_rptr_offs = tmd710_set_rptr_offs,
    .get_rptr_offs = tmd710_get_rptr_offs,

    .decode_event =  th_decode_event,
    .hamlib_check_rig_caps = HAMLIB_CHECK_RIG_CAPS
};

/* structure for handling FO radio command */
typedef struct
{
    int vfo;       // P1
    freq_t freq;   // P2
    int step;      // P3
    int shift;     // P4
    int reverse;   // P5
    int tone;      // P6
    int ct;        // P7
    int dcs;       // P8
    int tone_freq; // P9
    int ct_freq;   // P10
    int dcs_val;   // P11
    int offset;    // P12
    int mode;      // P13
} tmd710_fo;

/* structure for handling ME radio command */
typedef struct
{
    int channel;   // P1
    freq_t freq;   // P2
    int step;      // P3
    int shift;     // P4
    int reverse;   // P5
    int tone;      // P6
    int ct;        // P7
    int dcs;       // P8
    int tone_freq; // P9
    int ct_freq;   // P10
    int dcs_val;   // P11
    int offset;    // P12
    int mode;      // P13
    freq_t tx_freq;   // P14
    int p15_unknown;  // P15
    int lockout;   // P16
} tmd710_me;

/* structure for handling MU (menu) radio command */
typedef struct
{
    int beep; // P1 0/1
    int beep_volume; // P2 (1-7)
    int ext_speaker_mode; // P3
    int announce; // P4
    int language; // P5
    int voice_volume; // P6 (0-7)
    int voice_speed; // P7 (0-4)
    int playback_repeat; // P8 0/1
    int playback_repeat_interval; // P9 (00-60)
    int continuous_recording; // P10 0/1
    int vhf_aip; // P11 0/1
    int uhf_aip; // P12 0/1
    int smeter_sql_hang_up_time; // P13
    int mute_hang_up_time; // P14
    int beat_shift; // P15 0/1
    int timeout_timer; // P16
    int recall_method; // P17
    int echolink_speed; // P18
    int dtmf_hold; // P19 0/1
    int dtmf_speed; // P20
    int dtmf_pause; // P21
    int dtmf_key_lock; // P22 0/1
    int auto_repeater_offset; // P23 0/1
    int tone_1750_tx_hold; // P24 0/1
    int p25_unknown; // TODO
    int brightness_level; // P26 (0-8)
    int auto_brightness; // P27 0/1
    int backlight_color; // P28
    int pf1_key; // P29
    int pf2_key; // P30
    int mic_pf1_key; // P31
    int mic_pf2_key; // P32
    int mic_pf3_key; // P33
    int mic_pf4_key; // P34
    int mic_key_lock; // P35 0/1
    int scan_resume; // P36
    int auto_power_off; // P37
    int ext_data_band; // P38
    int ext_data_speed; // P39
    int sqc_source; // P40
    int auto_pm_store; // P41 0/1
    int display_partition_bar; // P42 0/1
} tmd710_mu;

static int tmd710_open(RIG *rig)
{
    split_t split;
    vfo_t vfo;

    rig_debug(RIG_DEBUG_TRACE, "%s: called\n", __func__);

    rig->state.tx_vfo = RIG_VFO_A;

    // Get current RX and TX VFO state, do not care if we succeed or not
    tmd710_get_vfo(rig, &vfo);
    tmd710_get_split_vfo(rig, RIG_VFO_CURR, &split, &vfo);

    rig_debug(RIG_DEBUG_TRACE, "rig->state.tx_vfo: %s\n",
              rig_strvfo(rig->state.tx_vfo));

    return 0;
}

static int tmd710_get_vfo_num(RIG *rig, int *vfonum, vfo_t *vfo)
{
    char buf[10];
    int retval, ctrlnum, pttnum;

    rig_debug(RIG_DEBUG_TRACE, "%s: called\n", __func__);

    retval = kenwood_transaction(rig, "BC", buf, sizeof(buf));

    if (retval != RIG_OK)
    {
        return retval;
    }

    retval = sscanf(buf, "BC %d,%d", &ctrlnum, &pttnum);

    if (retval != 2)
    {
        rig_debug(RIG_DEBUG_ERR, "Unable to parse '%s', expected 'BC c,p'\n", buf);
        return -RIG_EPROTO;
    }

    switch (ctrlnum)
    {
    case TMD710_BAND_A:
        if (vfo != NULL)
        {
            *vfo = RIG_VFO_A;
        }

        break;

    case TMD710_BAND_B:
        if (vfo != NULL)
        {
            *vfo = RIG_VFO_B;
        }

        break;

    default:
        rig_debug(RIG_DEBUG_ERR, "%s: Unexpected VFO value '%c'\n", __func__, buf[3]);
        return -RIG_EVFO;
    }

    if (vfonum != NULL)
    {
        *vfonum = ctrlnum;
    }

    return RIG_OK;
}

static int tmd710_get_vfo_and_mode(RIG *rig, vfo_t *vfo, int *vfomode)
{
    char cmdbuf[10], buf[10];
    int retval, vfonum, vfomodenum;

    rig_debug(RIG_DEBUG_TRACE, "%s: called\n", __func__);

    /* Get VFO band */

    retval = tmd710_get_vfo_num(rig, &vfonum, vfo);

    if (retval != RIG_OK)
    {
        return retval;
    }

    /* Get mode of the VFO band */

    snprintf(cmdbuf, sizeof(cmdbuf), "VM %d", vfonum);

    retval = kenwood_safe_transaction(rig, cmdbuf, buf, 10, 6);

    if (retval != RIG_OK)
    {
        return retval;
    }

    retval = sscanf(buf, "VM %d,%d", &vfonum, &vfomodenum);

    if (retval != 2)
    {
        rig_debug(RIG_DEBUG_ERR, "Unable to parse '%s', expected 'VM c,m'\n", buf);
        return -RIG_EPROTO;
    }

    if (vfomode != NULL)
    {
        *vfomode = vfomodenum;
    }

    return RIG_OK;
}

static int tmd710_resolve_vfo(RIG *rig, vfo_t vfo, vfo_t *resolved_vfo,
                              int *resolved_vfonum)
{
    switch (vfo)
    {
    case RIG_VFO_CURR:
    case RIG_VFO_MEM:
        return tmd710_get_vfo_num(rig, resolved_vfonum, resolved_vfo);

    case RIG_VFO_A:
        if (resolved_vfo != NULL)
        {
            *resolved_vfo = RIG_VFO_A;
        }

        if (resolved_vfonum != NULL)
        {
            *resolved_vfonum = TMD710_BAND_A;
        }

        break;

    case RIG_VFO_B:
        if (resolved_vfo != NULL)
        {
            *resolved_vfo = RIG_VFO_B;
        }

        if (resolved_vfonum != NULL)
        {
            *resolved_vfonum = TMD710_BAND_B;
        }

        break;

    default:
        return -RIG_ENTARGET;
    }

    return RIG_OK;
}

static int tmd710_scan_me(char *buf, tmd710_me *me_struct)
{
    int retval;

    retval = num_sscanf(buf,
                        "ME %x,%"SCNfreq",%x,%x,%x,%x,%x,%x,%d,%d,%d,%d,%d,%"SCNfreq",%d,%d",
                        &me_struct->channel, &me_struct->freq,
                        &me_struct->step, &me_struct->shift,
                        &me_struct->reverse, &me_struct->tone,
                        &me_struct->ct, &me_struct->dcs,
                        &me_struct->tone_freq, &me_struct->ct_freq,
                        &me_struct->dcs_val, &me_struct->offset,
                        &me_struct->mode, &me_struct->tx_freq,
                        &me_struct->p15_unknown, &me_struct->lockout);

    if (retval != 16)
    {
        rig_debug(RIG_DEBUG_ERR, "%s: Unexpected reply '%s'\n", __func__, buf);
        return -RIG_ERJCTED;
    }

    return RIG_OK;
}

/*
 * The TM-D710(G) has a single command ME that queries and sets many values.
 * This function pulls that string from the radio given a memory channel.
 * Push/pull naming is used inside the backend rather than get/set.
 * There is one unknown field.
 */
int tmd710_pull_me(RIG *rig, int ch, tmd710_me *me_struct)
{
    char cmdbuf[8];
    char buf[80];
    int retval;

    rig_debug(RIG_DEBUG_TRACE, "%s: called\n", __func__);

    snprintf(cmdbuf, sizeof(cmdbuf), "ME %03d", ch);
    retval = kenwood_transaction(rig, cmdbuf, buf, sizeof(buf));

    if (retval != RIG_OK)
    {
        return retval;
    }

    retval = tmd710_scan_me(buf, me_struct);

    if (retval != RIG_OK)
    {
        return retval;
    }

    return RIG_OK;
}

int tmd710_push_me(RIG *rig, tmd710_me *me_struct)
{
    char cmdbuf[80];
    char buf[80];

    rig_debug(RIG_DEBUG_TRACE, "%s: called\n", __func__);

    snprintf(cmdbuf, sizeof(cmdbuf),
             "ME %03d,%010.0f,%1d,%1d,%1d,%1d,%1d,%1d,%02d,%02d,%03d,%08d,%1d,%010.0f,%1d,%1d",
             me_struct->channel, me_struct->freq,
             me_struct->step, me_struct->shift,
             me_struct->reverse, me_struct->tone,
             me_struct->ct, me_struct->dcs,
             me_struct->tone_freq, me_struct->ct_freq,
             me_struct->dcs_val, me_struct->offset,
             me_struct->mode, me_struct->tx_freq,
             me_struct->p15_unknown, me_struct->lockout);

    return kenwood_transaction(rig, cmdbuf, buf, sizeof(buf));
}

int tmd710_get_memory_name(RIG *rig, int ch, char *name)
{
    char cmdbuf[8];
    char buf[80];
    int retval;

    rig_debug(RIG_DEBUG_TRACE, "%s: called on channel %d\n", __func__, ch);

    snprintf(cmdbuf, sizeof(cmdbuf), "MN %03d", ch);
    retval = kenwood_transaction(rig, cmdbuf, buf, sizeof(buf));

    if (retval != RIG_OK)
    {
        return retval;
    }

    retval = num_sscanf(buf, "MN %d,%s", &ch, name);

    if (retval != 2)
    {
        rig_debug(RIG_DEBUG_ERR, "%s: Unexpected reply '%s'\n", __func__, buf);
        return -RIG_ERJCTED;
    }

    return RIG_OK;
}

int tmd710_set_memory_name(RIG *rig, int ch, char *name)
{
    char cmdbuf[32];
    char buf[80];
    int retval;

    rig_debug(RIG_DEBUG_TRACE, "%s: called on channel %d with name %s\n", __func__,
              ch, name);

    snprintf(cmdbuf, sizeof(cmdbuf), "MN %03d,%s", ch, name);
    retval = kenwood_transaction(rig, cmdbuf, buf, sizeof(buf));

    if (retval != RIG_OK)
    {
        return retval;
    }

    return RIG_OK;
}

/*
 * The TM-D710(G) has a single command FO that queries and sets many values.
 * This function pulls that string from the radio given a VFO.
 * Push/pull language is used inside the backend rather than get/set.
 */
int tmd710_pull_fo(RIG *rig, vfo_t vfo, tmd710_fo *fo_struct)
{
    char cmdbuf[8];
    char buf[80];
    int vfonum;
    int retval;

    rig_debug(RIG_DEBUG_TRACE, "%s: called with VFO %08X\n", __func__, vfo);

    retval = tmd710_resolve_vfo(rig, vfo, NULL, &vfonum);

    if (retval != RIG_OK)
    {
        return retval;
    }

    snprintf(cmdbuf, sizeof(cmdbuf), "FO %1d", vfonum);
    retval = kenwood_safe_transaction(rig, cmdbuf, buf, sizeof(buf), 48);

    if (retval != RIG_OK)
    {
        return retval;
    }

    retval = num_sscanf(buf, "FO %x,%"SCNfreq",%x,%x,%x,%x,%x,%x,%d,%d,%d,%d,%d",
                        &fo_struct->vfo, &fo_struct->freq,
                        &fo_struct->step, &fo_struct->shift,
                        &fo_struct->reverse, &fo_struct->tone,
                        &fo_struct->ct, &fo_struct->dcs,
                        &fo_struct->tone_freq, &fo_struct->ct_freq,
                        &fo_struct->dcs_val, &fo_struct->offset,
                        &fo_struct->mode);

    if (retval != 13)
    {
        rig_debug(RIG_DEBUG_ERR, "%s: Unexpected reply '%s'\n", __func__, buf);
        return -RIG_ERJCTED;
    }

    return RIG_OK;
}

int tmd710_push_fo(RIG *rig, vfo_t vfo, tmd710_fo *fo_struct)
{
    char cmdbuf[80];
    char buf[80];
    int retval;

    rig_debug(RIG_DEBUG_TRACE, "%s: called\n", __func__);

    snprintf(cmdbuf, sizeof(cmdbuf),
             "FO %1d,%010.0f,%1d,%1d,%1d,%1d,%1d,%1d,%02d,%02d,%03d,%08d,%1d",
             fo_struct->vfo, fo_struct->freq,
             fo_struct->step, fo_struct->shift,
             fo_struct->reverse, fo_struct->tone,
             fo_struct->ct, fo_struct->dcs,
             fo_struct->tone_freq, fo_struct->ct_freq,
             fo_struct->dcs_val, fo_struct->offset,
             fo_struct->mode);

    retval = kenwood_safe_transaction(rig, cmdbuf, buf, sizeof(buf), 48);

    if (retval != RIG_OK)
    {
        return retval;
    }

    retval = num_sscanf(buf, "FO %x,%"SCNfreq",%x,%x,%x,%x,%x,%x,%d,%d,%d,%d,%d",
                        &fo_struct->vfo, &fo_struct->freq,
                        &fo_struct->step, &fo_struct->shift,
                        &fo_struct->reverse, &fo_struct->tone,
                        &fo_struct->ct, &fo_struct->dcs,
                        &fo_struct->tone_freq, &fo_struct->ct_freq,
                        &fo_struct->dcs_val, &fo_struct->offset,
                        &fo_struct->mode);

    if (retval != 13)
    {
        rig_debug(RIG_DEBUG_ERR, "%s: Unexpected reply '%s'\n", __func__, buf);
        return -RIG_ERJCTED;
    }

    return RIG_OK;
}

int tmd710_scan_mu(char *buf, tmd710_mu *mu_struct)
{
    int retval;

    retval = num_sscanf(buf,
                        "MU %d,%d,%d,%d,%d,%d,%d,%d,%d,%d,"
                        "%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,"
                        "%d,%d,%d,%d,%d,%d,%d,%d,%X,%X,"
                        "%X,%X,%X,%X,%d,%d,%d,%d,%d,%d,"
                        "%d,%d",
                        &mu_struct->beep,
                        &mu_struct->beep_volume,
                        &mu_struct->ext_speaker_mode,
                        &mu_struct->announce,
                        &mu_struct->language,
                        &mu_struct->voice_volume,
                        &mu_struct->voice_speed,
                        &mu_struct->playback_repeat,
                        &mu_struct->playback_repeat_interval,
                        &mu_struct->continuous_recording,
                        &mu_struct->vhf_aip,
                        &mu_struct->uhf_aip,
                        &mu_struct->smeter_sql_hang_up_time,
                        &mu_struct->mute_hang_up_time,
                        &mu_struct->beat_shift,
                        &mu_struct->timeout_timer,
                        &mu_struct->recall_method,
                        &mu_struct->echolink_speed,
                        &mu_struct->dtmf_hold,
                        &mu_struct->dtmf_speed,
                        &mu_struct->dtmf_pause,
                        &mu_struct->dtmf_key_lock,
                        &mu_struct->auto_repeater_offset,
                        &mu_struct->tone_1750_tx_hold,
                        &mu_struct->p25_unknown,
                        &mu_struct->brightness_level,
                        &mu_struct->auto_brightness,
                        &mu_struct->backlight_color,
                        &mu_struct->pf1_key,
                        &mu_struct->pf2_key,
                        &mu_struct->mic_pf1_key,
                        &mu_struct->mic_pf2_key,
                        &mu_struct->mic_pf3_key,
                        &mu_struct->mic_pf4_key,
                        &mu_struct->mic_key_lock,
                        &mu_struct->scan_resume,
                        &mu_struct->auto_power_off,
                        &mu_struct->ext_data_band,
                        &mu_struct->ext_data_speed,
                        &mu_struct->sqc_source,
                        &mu_struct->auto_pm_store,
                        &mu_struct->display_partition_bar
                       );

    if (retval != 42)
    {
        rig_debug(RIG_DEBUG_ERR, "%s: Unexpected reply '%s'\n", __func__, buf);
        return -RIG_ERJCTED;
    }

    return RIG_OK;
}

int tmd710_pull_mu(RIG *rig, tmd710_mu *mu_struct)
{
    char buf[128];
    int retval;

    rig_debug(RIG_DEBUG_TRACE, "%s: called\n", __func__);

    retval = kenwood_transaction(rig, "MU", buf, sizeof(buf));

    if (retval != RIG_OK)
    {
        return retval;
    }

    retval = tmd710_scan_mu(buf, mu_struct);

    if (retval != RIG_OK)
    {
        return retval;
    }

    return RIG_OK;
}

int tmd710_push_mu(RIG *rig, tmd710_mu *mu_struct)
{
    char cmdbuf[128];
    char buf[128];
    int retval;

    rig_debug(RIG_DEBUG_TRACE, "%s: called\n", __func__);

    // we re-use fo_struct->vfo for the channel#
    snprintf(cmdbuf, sizeof(cmdbuf),
             "MU %1d,%1d,%1d,%1d,%1d,%1d,%1d,%1d,%02d,%1d,"
             "%1d,%1d,%1d,%1d,%1d,%1d,%1d,%1d,%1d,%1d,"
             "%1d,%1d,%1d,%1d,%1d,%1d,%1d,%1d,%02X,%02X,"
             "%02X,%02X,%02X,%02X,%1d,%1d,%1d,%1d,%1d,%1d,"
             "%1d,%1d",
             mu_struct->beep,
             mu_struct->beep_volume,
             mu_struct->ext_speaker_mode,
             mu_struct->announce,
             mu_struct->language,
             mu_struct->voice_volume,
             mu_struct->voice_speed,
             mu_struct->playback_repeat,
             mu_struct->playback_repeat_interval,
             mu_struct->continuous_recording,
             mu_struct->vhf_aip,
             mu_struct->uhf_aip,
             mu_struct->smeter_sql_hang_up_time,
             mu_struct->mute_hang_up_time,
             mu_struct->beat_shift,
             mu_struct->timeout_timer,
             mu_struct->recall_method,
             mu_struct->echolink_speed,
             mu_struct->dtmf_hold,
             mu_struct->dtmf_speed,
             mu_struct->dtmf_pause,
             mu_struct->dtmf_key_lock,
             mu_struct->auto_repeater_offset,
             mu_struct->tone_1750_tx_hold,
             mu_struct->p25_unknown,
             mu_struct->brightness_level,
             mu_struct->auto_brightness,
             mu_struct->backlight_color,
             mu_struct->pf1_key,
             mu_struct->pf2_key,
             mu_struct->mic_pf1_key,
             mu_struct->mic_pf2_key,
             mu_struct->mic_pf3_key,
             mu_struct->mic_pf4_key,
             mu_struct->mic_key_lock,
             mu_struct->scan_resume,
             mu_struct->auto_power_off,
             mu_struct->ext_data_band,
             mu_struct->ext_data_speed,
             mu_struct->sqc_source,
             mu_struct->auto_pm_store,
             mu_struct->display_partition_bar
            );

    retval = kenwood_transaction(rig, cmdbuf, buf, sizeof(buf));

    if (retval != RIG_OK)
    {
        return retval;
    }

    retval = tmd710_scan_mu(buf, mu_struct);

    if (retval != RIG_OK)
    {
        return retval;
    }

    return RIG_OK;
}

/*
 * tmd710_set_freq
 * Assumes rig!=NULL
 * Common function for getting the main and split frequency.
 */
int tmd710_do_set_freq(RIG *rig, vfo_t vfo, freq_t freq)
{
    int retval;
    tmd710_fo fo_struct;
    long freq5, freq625, freq_sent;
    int step;

    rig_debug(RIG_DEBUG_TRACE, "%s: called\n", __func__);

    retval = tmd710_pull_fo(rig, vfo, &fo_struct);

    if (retval != RIG_OK)
    {
        return retval;
    }

    freq5 = round(freq / 5000) * 5000;
    freq625 = round(freq / 6250) * 6250;

    if (fabs(freq5 - freq) < fabs(freq625 - freq))
    {
        step = 0;
        freq_sent = freq5;
    }
    else
    {
        step = 1;
        freq_sent = freq625;
    }

    /* Step needs to be at least 10kHz on higher band, otherwise 5 kHz */
    fo_struct.step = freq_sent >= MHz(470) ? 4 : step;
    fo_struct.freq = freq_sent >= MHz(470) ? (round(freq_sent / 10000) * 10000) :
                     freq_sent;

    //return tmd710_push_fo(rig, vfo, &fo_struct);
    return tmd710_push_fo(rig, vfo, &fo_struct);
}

/*
 * tmd710_do_get_freq
 * Assumes rig!=NULL, freq!=NULL
 * Common function for getting the main and split frequency.
 */
int tmd710_do_get_freq(RIG *rig, vfo_t vfo, freq_t *freq)
{
    tmd710_fo fo_struct;
    int retval;

    rig_debug(RIG_DEBUG_TRACE, "%s: called for vfo: %s(%d)\n", __func__,
              rig_strvfo(vfo), vfo);

    retval = tmd710_pull_fo(rig, vfo, &fo_struct);

    if (retval == RIG_OK)
    {
        *freq = fo_struct.freq;
    }

    return retval;
}

/*
 * tmd710_set_freq
 * Assumes rig!=NULL, freq!=NULL
 */
int tmd710_set_freq(RIG *rig, vfo_t vfo, freq_t freq)
{

    rig_debug(RIG_DEBUG_TRACE, "%s: called for vfo: %s(%d)\n", __func__,
              rig_strvfo(vfo), vfo);

    return tmd710_do_set_freq(rig, vfo, freq);
}

/*
 * tmd710_get_freq
 * Assumes rig!=NULL, freq!=NULL
 */
int tmd710_get_freq(RIG *rig, vfo_t vfo, freq_t *freq)
{

    rig_debug(RIG_DEBUG_TRACE, "%s: called\n", __func__);

    return tmd710_do_get_freq(rig, vfo, freq);
}

/*
 * tmd710_set_split_freq
 * Assumes rig!=NULL, freq!=NULL
 */
int tmd710_set_split_freq(RIG *rig, vfo_t vfo, freq_t freq)
{

    rig_debug(RIG_DEBUG_TRACE, "%s: called\n", __func__);

    // Use the TX VFO for split
    return tmd710_do_set_freq(rig, rig->state.tx_vfo, freq);
}

/*
 * tmd710_get_split_freq
 * Assumes rig!=NULL, freq!=NULL
 */
int tmd710_get_split_freq(RIG *rig, vfo_t vfo, freq_t *freq)
{

    rig_debug(RIG_DEBUG_TRACE, "%s: called\n", __func__);

    // Use the TX VFO for split
    return tmd710_do_get_freq(rig, rig->state.tx_vfo, freq);
}

static int tmd710_find_ctcss_index(RIG *rig, tone_t tone, int *ctcss_index)
{
    int k, stepind = -1;

    for (k = 0; k < 42; k++)
    {
        if (rig->caps->ctcss_list[k] == tone)
        {
            stepind = k;
            break;
        }
    }

    if (stepind == -1)
    {
        rig_debug(RIG_DEBUG_ERR, "%s: Unsupported tone value '%d'\n", __func__, tone);
        return -RIG_EINVAL;
    }

    *ctcss_index = stepind;

    return RIG_OK;
}

/*
 * tmd710_set_ctcss_tone
 * Assumes rig!=NULL, freq!=NULL
 */
static int tmd710_set_ctcss_tone(RIG *rig, vfo_t vfo, tone_t tone)
{
    int retval, stepind;
    tmd710_fo fo_struct;

    rig_debug(RIG_DEBUG_TRACE, "%s: called\n", __func__);

    retval = tmd710_find_ctcss_index(rig, tone, &stepind);

    if (retval != RIG_OK)
    {
        return retval;
    }

    retval = tmd710_pull_fo(rig, vfo, &fo_struct);

    if (retval != RIG_OK)
    {
        return retval;
    }

    fo_struct.tone_freq = stepind;

    return tmd710_push_fo(rig, vfo, &fo_struct);
}

/*
 * tmd710_get_ctcss_tone
 * Assumes rig!=NULL, freq!=NULL
 */
int tmd710_get_ctcss_tone(RIG *rig, vfo_t vfo, tone_t *tone)
{
    tmd710_fo fo_struct;
    int retval;

    const struct rig_caps *caps;
    caps = rig->caps;

    rig_debug(RIG_DEBUG_TRACE, "%s: called\n", __func__);

    retval = tmd710_pull_fo(rig, vfo, &fo_struct);

    if (retval == RIG_OK)
    {
        *tone = caps->ctcss_list[fo_struct.tone_freq];
    }

    return retval;
}

/*
 * tmd710_set_ctcss_sql
 * Assumes rig!=NULL, freq!=NULL
 */
static int tmd710_set_ctcss_sql(RIG *rig, vfo_t vfo, tone_t tone)
{
    int retval, stepind;
    tmd710_fo fo_struct;

    rig_debug(RIG_DEBUG_TRACE, "%s: called\n", __func__);

    retval = tmd710_find_ctcss_index(rig, tone, &stepind);

    if (retval != RIG_OK)
    {
        return retval;
    }

    retval = tmd710_pull_fo(rig, vfo, &fo_struct);

    if (retval != RIG_OK)
    {
        return retval;
    }

    fo_struct.ct_freq = stepind;

    return tmd710_push_fo(rig, vfo, &fo_struct);
}

/*
 * tmd710_get_ctcss_sql
 * Assumes rig!=NULL, freq!=NULL
 */
int tmd710_get_ctcss_sql(RIG *rig, vfo_t vfo, tone_t *tone)
{
    tmd710_fo fo_struct;
    int retval;

    const struct rig_caps *caps;
    caps = rig->caps;

    rig_debug(RIG_DEBUG_TRACE, "%s: called\n", __func__);

    retval = tmd710_pull_fo(rig, vfo, &fo_struct);

    if (retval == RIG_OK)
    {
        *tone = caps->ctcss_list[fo_struct.ct_freq];
    }

    return retval;
}

/*
 *      status: ok, no vfo checks
 */
int tmd710_get_dcs_sql(RIG *rig, vfo_t vfo, tone_t *code)
{
    int retval;
    tmd710_fo fo_struct;

    if (!rig || !code)
    {
        return -RIG_EINVAL;
    }

    retval = tmd710_pull_fo(rig, vfo, &fo_struct);

    if (retval != RIG_OK)
    {
        return retval;
    }

    if (fo_struct.dcs)
    {
        tone_t *dcs_list = common_dcs_list;
        *code = dcs_list[fo_struct.dcs_val];
    }
    else
    {
        *code = 0;
    }

    return RIG_OK;
}

static int tmd710_find_dcs_index(tone_t code, int *dcs_index)
{
    int i = 0;

    // we only allow exact matches here
    tone_t *dcs_list = common_dcs_list;

    while (code != dcs_list[i])
    {
        if (dcs_list[i] == 0)
        {
            return -RIG_EINVAL;
        }

        i++;
    }

    *dcs_index = i;

    return RIG_OK;
}

/*
 *      status: ok, no vfo checks
 */
int tmd710_set_dcs_sql(RIG *rig, vfo_t vfo, tone_t code)
{
    int retval, dcs_index, dcs_enable;
    tmd710_fo fo_struct;

    if (code == 0)
    {
        dcs_index = 0;
        dcs_enable = 0;
    }
    else
    {
        retval = tmd710_find_dcs_index(code, &dcs_index);

        if (retval != RIG_OK)
        {
            return retval;
        }

        dcs_enable = 1;
    }

    retval = tmd710_pull_fo(rig, vfo, &fo_struct);

    if (retval != RIG_OK)
    {
        return retval;
    }

    fo_struct.dcs = dcs_enable;
    fo_struct.dcs_val = dcs_index;

    return tmd710_push_fo(rig, vfo, &fo_struct);
}

static int tmd710_get_mode_hamlib_values(int tmd710_mode, rmode_t *mode,
        pbwidth_t *width)
{
    switch (tmd710_mode)
    {
    case 0:
        *mode = RIG_MODE_FM;
        *width = 15000;
        break;

    case 1:
        *mode = RIG_MODE_FMN;
        *width = 5000;
        break;

    case 2:
        *mode = RIG_MODE_AM;
        *width = 4000;
        break;

    default:
        rig_debug(RIG_DEBUG_ERR, "%s: Illegal value from radio '%ld'\n", __func__,
                  (long)tmd710_mode);
        return -RIG_EINVAL;
    }

    return RIG_OK;
}

/*
 * tmd710_get_mode
 * Assumes rig!=NULL, freq!=NULL
 */
int tmd710_get_mode(RIG *rig, vfo_t vfo, rmode_t *mode, pbwidth_t *width)
{
    tmd710_fo fo_struct;
    int retval;

    rig_debug(RIG_DEBUG_TRACE, "%s: called\n", __func__);

    retval = tmd710_pull_fo(rig, vfo, &fo_struct);

    if (retval != RIG_OK)
    {
        return retval;
    }

    retval = tmd710_get_mode_hamlib_values(fo_struct.mode, mode, width);

    if (retval != RIG_OK)
    {
        return retval;
    }

    return RIG_OK;
}

static int tmd710_get_mode_tmd710_value(rmode_t mode, int *tmd710_mode)
{
    if (mode == RIG_MODE_FM)
    {
        *tmd710_mode = 0;
    }
    else if (mode == RIG_MODE_FMN)
    {
        *tmd710_mode = 1;
    }
    else if (mode == RIG_MODE_AM)
    {
        *tmd710_mode = 2;
    }
    else
    {
        rig_debug(RIG_DEBUG_ERR, "%s: Illegal value from radio '%ld'\n", __func__,
                  (long)mode);
        return -RIG_EINVAL;
    }

    return RIG_OK;
}

/*
 * tmd710_set_mode
 * Assumes rig!=NULL, freq!=NULL
 */
static int tmd710_set_mode(RIG *rig, vfo_t vfo, rmode_t mode, pbwidth_t width)
{
    int retval, tmd710_mode = RIG_MODE_NONE;
    tmd710_fo fo_struct;

    rig_debug(RIG_DEBUG_TRACE, "%s: called\n", __func__);

    retval = tmd710_get_mode_tmd710_value(mode, &tmd710_mode);

    if (retval != RIG_OK)
    {
        return retval;
    }

    retval = tmd710_pull_fo(rig, vfo, &fo_struct);

    if (retval != RIG_OK)
    {
        return retval;
    }

    fo_struct.mode = tmd710_mode;

    return tmd710_push_fo(rig, vfo, &fo_struct);
}

static int tmd710_find_tuning_step_index(RIG *rig, shortfreq_t ts,
        int *step_index)
{
    int k, stepind = -1;

    for (k = 0; rig->state.tuning_steps[k].ts != 0; ++k)
    {

        if ((rig->caps->tuning_steps[k].modes == RIG_MODE_NONE)
                && (rig->caps->tuning_steps[k].ts == 0))
        {
            break;
        }
        else if (rig->caps->tuning_steps[k].ts == ts)
        {
            stepind = k;
            break;
        }
    }

    if (stepind == -1)
    {
        rig_debug(RIG_DEBUG_ERR, "%s: Unsupported tuning step value '%ld'\n", __func__,
                  ts);
        return -RIG_EINVAL;
    }

    *step_index = stepind;

    return RIG_OK;
}

/*
 * tmd710_set_ts
 * Assumes rig!=NULL, freq!=NULL
 */
static int tmd710_set_ts(RIG *rig, vfo_t vfo, shortfreq_t ts)
{
    int retval, stepind;
    tmd710_fo fo_struct;

    rig_debug(RIG_DEBUG_TRACE, "%s: called\n", __func__);

    retval = tmd710_find_tuning_step_index(rig, ts, &stepind);

    if (retval != RIG_OK)
    {
        return retval;
    }

    retval = tmd710_pull_fo(rig, vfo, &fo_struct);

    if (retval != RIG_OK)
    {
        return retval;
    }

    fo_struct.step = stepind;

    return tmd710_push_fo(rig, vfo, &fo_struct);
}

/*
 * tmd710_get_ts
 * Assumes rig!=NULL, freq!=NULL
 */
static int tmd710_get_ts(RIG *rig, vfo_t vfo, shortfreq_t *ts)
{
    int retval;
    tmd710_fo fo_struct;
    rig_debug(RIG_DEBUG_TRACE, "%s: called\n", __func__);

    retval = tmd710_pull_fo(rig, vfo, &fo_struct);

    if (retval == RIG_OK)
    {
        *ts = rig->caps->tuning_steps[fo_struct.step].ts;
    }

    return retval;
}

int tmd710_get_rptr_shift_tmd710_value(rptr_shift_t shift, int *tmd710_shift)
{
    switch (shift)
    {
    case RIG_RPT_SHIFT_NONE:
        *tmd710_shift = 0;
        break;

    case RIG_RPT_SHIFT_PLUS:
        *tmd710_shift = 1;
        break;

    case RIG_RPT_SHIFT_MINUS:
        *tmd710_shift = 2;
        break;

    default:
        rig_debug(RIG_DEBUG_ERR, "%s: Unexpected shift value '%d'\n", __func__, shift);
        return -RIG_EPROTO;
    }

    return RIG_OK;
}

/*
 * tmd710_set_rptr_shift
 * Assumes rig!=NULL, freq!=NULL
 */
int tmd710_set_rptr_shift(RIG *rig, vfo_t vfo, rptr_shift_t shift)
{
    int retval;
    tmd710_fo fo_struct;

    rig_debug(RIG_DEBUG_TRACE, "%s: called\n", __func__);

    retval = tmd710_pull_fo(rig, vfo, &fo_struct);

    if (retval != RIG_OK)
    {
        return retval;
    }

    retval = tmd710_get_rptr_shift_tmd710_value(shift, &fo_struct.shift);

    if (retval != RIG_OK)
    {
        return retval;
    }

    return tmd710_push_fo(rig, vfo, &fo_struct);
}

int tmd710_get_rptr_shift_hamlib_value(int tmd710_shift, rptr_shift_t *shift)
{
    switch (tmd710_shift)
    {
    case 0:
        *shift = RIG_RPT_SHIFT_NONE;
        break;

    case 1:
        *shift = RIG_RPT_SHIFT_PLUS;
        break;

    case 2:
        *shift = RIG_RPT_SHIFT_MINUS;
        break;

    default:
        rig_debug(RIG_DEBUG_ERR, "%s: Unexpected shift value '%d'\n", __func__,
                  tmd710_shift);
        return -RIG_EPROTO;
    }

    return RIG_OK;
}

/*
 * tmd710_get_rptr_shft
 * Assumes rig!=NULL, freq!=NULL
 */
int tmd710_get_rptr_shift(RIG *rig, vfo_t vfo, rptr_shift_t *shift)
{
    int retval;
    tmd710_fo fo_struct;

    rig_debug(RIG_DEBUG_TRACE, "%s: called\n", __func__);

    retval = tmd710_pull_fo(rig, vfo, &fo_struct);

    if (retval != RIG_OK)
    {
        return retval;
    }

    retval = tmd710_get_rptr_shift_hamlib_value(fo_struct.shift, shift);

    if (retval != RIG_OK)
    {
        return retval;
    }

    return RIG_OK;
}

/*
 * tmd710_set_rptr_offs
 * Assumes rig!=NULL
 */
int tmd710_set_rptr_offs(RIG *rig, vfo_t vfo, shortfreq_t freq)
{
    int retval;
    tmd710_fo fo_struct;
    long freq5, freq625, freq_sent;

    rig_debug(RIG_DEBUG_TRACE, "%s: called\n", __func__);

    retval = tmd710_pull_fo(rig, vfo, &fo_struct);

    if (retval != RIG_OK)
    {
        return retval;
    }

    freq5 = round(freq / 5000) * 5000;
    freq625 = round(freq / 6250) * 6250;

    if (abs((int)(freq5 - freq)) < abs((int)(freq625 - freq)))
    {
        freq_sent = freq5;
    }
    else
    {
        freq_sent = freq625;
    }

    /* Step needs to be at least 10kHz on higher band, otherwise 5 kHz */
    fo_struct.offset = freq_sent >= MHz(470) ? (round(freq_sent / 10000) * 10000) :
                       freq_sent;

    return tmd710_push_fo(rig, vfo, &fo_struct);
}



/*
 * tmd710_get_rptr_offs
 * Assumes rig!=NULL, freq!=NULL
 */
int tmd710_get_rptr_offs(RIG *rig, vfo_t vfo, shortfreq_t *rptr_offs)
{
    tmd710_fo fo_struct;
    int retval;

    rig_debug(RIG_DEBUG_TRACE, "%s: called\n", __func__);

    retval = tmd710_pull_fo(rig, vfo, &fo_struct);

    if (retval == RIG_OK)
    {
        *rptr_offs = fo_struct.offset;
    }


    return retval;
}

/*
 * tmd710_get_vfo
 * Assumes rig!=NULL
 */
int tmd710_get_vfo(RIG *rig, vfo_t *vfo)
{
    int vfomode;
    int retval;

    rig_debug(RIG_DEBUG_TRACE, "%s: called\n", __func__);

    retval = tmd710_get_vfo_and_mode(rig, vfo, &vfomode);

    if (retval != RIG_OK)
    {
        return retval;
    }

    switch (vfomode)
    {
    case TMD710_BAND_MODE_VFO:
        break;

    case TMD710_BAND_MODE_MEMORY:
    case TMD710_BAND_MODE_CALL:
        *vfo = RIG_VFO_MEM;
        break;

    default:
        rig_debug(RIG_DEBUG_ERR, "%s: Unexpected VFO mode value '%c'\n", __func__,
                  vfomode);
        return -RIG_EVFO;
    }

    return RIG_OK;
}

/*
 * tmd710_set_vfo
 *
 * Assumes rig!=NULL
 */
int tmd710_set_vfo(RIG *rig, vfo_t vfo)
{
    char vfobuf[16], ackbuf[16];
    int vfonum, vfomode;
    int retval;

    rig_debug(RIG_DEBUG_TRACE, "%s: called %s\n", __func__, rig_strvfo(vfo));

    switch (vfo)
    {
    case RIG_VFO_A:
    case RIG_VFO_VFO:
        vfonum = TMD710_BAND_A;
        vfomode = TMD710_BAND_MODE_VFO;
        break;

    case RIG_VFO_B:
        vfonum = TMD710_BAND_B;
        vfomode = TMD710_BAND_MODE_VFO;
        break;

    case RIG_VFO_MEM:
        /* get current band */
        retval = tmd710_get_vfo_num(rig, &vfonum, NULL);

        if (retval != RIG_OK)
        {
            return retval;
        }

        vfomode = TMD710_BAND_MODE_MEMORY;
        break;

    default:
        rig_debug(RIG_DEBUG_ERR, "%s: Unsupported VFO %d\n", __func__, vfo);
        return -RIG_EVFO;
    }

    snprintf(vfobuf, sizeof(vfobuf), "VM %1d,%1d", vfonum, vfomode);
    retval = kenwood_transaction(rig, vfobuf, ackbuf, sizeof(ackbuf));

    if (retval != RIG_OK)
    {
        return retval;
    }

    if (vfo == RIG_VFO_MEM)
    {
        return RIG_OK;
    }

    snprintf(vfobuf, sizeof(vfobuf), "BC %1d,%1d", vfonum, vfonum);
    retval = kenwood_transaction(rig, vfobuf, ackbuf, sizeof(ackbuf));

    if (retval != RIG_OK)
    {
        return retval;
    }

    return RIG_OK;
}

/*
*   tmd710_set_split_vfo
*
*   This radio has two VFOs, and either one can be the TX/RX.  As such, this function does the following:
*   - Keeps VFO control (CTRL) on the currently active VFO.
*   - Sets PTT control on the specified VFO.
*   - Sets the tx_vfo for use in set_split_freq().
*   - The value of split is ignored, as the radio is always in "split" mode.
*
*/
int tmd710_set_split_vfo(RIG *rig, vfo_t vfo, split_t split, vfo_t txvfo)
{
    char vfobuf[16], ackbuf[16];
    int retval;
    int ctrl_vfo_index;

    rig_debug(RIG_DEBUG_TRACE, "%s: called vfo: %s, txvfo: %s\n", __func__,
              rig_strvfo(vfo), rig_strvfo(txvfo));

    retval = tmd710_get_vfo_num(rig, &ctrl_vfo_index, NULL);

    if (retval != RIG_OK)
    {
        return retval;
    }

    int ptt_vfo_index = txvfo == RIG_VFO_A ? 0 : 1;

    // Keep CTRL VFO as it is and only set the PTT VFO as TX VFO
    sprintf(vfobuf, "BC %d,%d", ctrl_vfo_index, ptt_vfo_index);
    retval = kenwood_transaction(rig, vfobuf, ackbuf, sizeof(ackbuf));

    if (retval != RIG_OK)
    {
        return retval;
    }

    rig->state.tx_vfo = txvfo;

    return RIG_OK;
}

int tmd710_get_split_vfo(RIG *rig, vfo_t vfo, split_t *split, vfo_t *txvfo)
{
    char buf[10];
    int retval;

    rig_debug(RIG_DEBUG_TRACE, "%s: called\n", __func__);

    /* Get VFO band */

    retval = kenwood_safe_transaction(rig, "BC", buf, 10, 6);

    if (retval != RIG_OK)
    {
        return retval;
    }

    switch (buf[5])
    {
    case '0': *txvfo = RIG_VFO_A; break;

    case '1': *txvfo = RIG_VFO_B; break;

    default:
        rig_debug(RIG_DEBUG_ERR, "%s: Unexpected txVFO value '%c'\n", __func__, buf[5]);
        return -RIG_EPROTO;
    }

    rig->state.tx_vfo = *txvfo;

    // Rig is always in "split mode" and VFOs are targetable, so simply check current and TX VFOs
    *split = rig->state.current_vfo == rig->state.tx_vfo ? RIG_SPLIT_OFF :
             RIG_SPLIT_ON;

    return RIG_OK;
}

int tmd710_get_mem(RIG *rig, vfo_t vfo, int *ch)
{
    char cmd[16];
    char membuf[16];
    int retval;
    int vfonum;
    int n;

    rig_debug(RIG_DEBUG_VERBOSE, "%s called\n", __func__);

    if (!rig || !ch)
    {
        return -RIG_EINVAL;
    }

    if (RIG_VFO_CURR == vfo || RIG_VFO_VFO == vfo)
    {
        retval = tmd710_get_vfo_num(rig, &vfonum, NULL);

        if (retval != RIG_OK)
        {
            return retval;
        }
    }
    else
    {
        vfonum = rig->state.current_vfo == RIG_VFO_A ? 0 : 1;
    }

    snprintf(cmd, sizeof(cmd), "MR %d", vfonum);
    retval = kenwood_safe_transaction(rig, cmd, membuf, sizeof(membuf), 8);

    if (retval != RIG_OK)
    {
        return retval;
    }

    n = sscanf(membuf, "MR %*d,%d", ch);

    if (n != 1)
    {
        rig_debug(RIG_DEBUG_ERR, "Unable to parse '%s', expected 'MR v,ccc'\n", membuf);
        return -RIG_EPROTO;
    }

    return RIG_OK;
}

int tmd710_set_mem(RIG *rig, vfo_t vfo, int ch)
{
    int retval, vfonum;
    char cmd[16];
    char membuf[16];

    rig_debug(RIG_DEBUG_VERBOSE, "%s called\n", __func__);

    if (!rig)
    {
        return -RIG_EINVAL;
    }

    if (RIG_VFO_CURR == vfo || RIG_VFO_VFO == vfo)
    {
        retval = tmd710_get_vfo_num(rig, &vfonum, NULL);

        if (retval != RIG_OK)
        {
            return retval;
        }
    }
    else
    {
        vfonum = rig->state.current_vfo == RIG_VFO_A ? 0 : 1;
    }

    snprintf(cmd, sizeof(cmd), "MR %d,%03d", vfonum, ch);

    return kenwood_safe_transaction(rig, cmd, membuf, sizeof(membuf), 8);
}

int tmd710_get_channel(RIG *rig, vfo_t vfo, channel_t *chan, int read_only)
{
    int retval;
    tmd710_me me_struct;

    rig_debug(RIG_DEBUG_VERBOSE, "%s called\n", __func__);

    if (!rig || !chan)
    {
        return -RIG_EINVAL;
    }

    retval = tmd710_pull_me(rig, chan->channel_num, &me_struct);

    if (retval != RIG_OK)
    {
        return retval;
    }

    chan->freq = me_struct.freq;
    chan->vfo = RIG_VFO_CURR;

    retval = tmd710_get_mode_hamlib_values(me_struct.mode, &chan->mode,
                                           &chan->width);

    if (retval != RIG_OK)
    {
        return retval;
    }

    chan->tuning_step = rig->caps->tuning_steps[me_struct.step].ts;

    chan->funcs = 0;

    if (me_struct.tone != 0)
    {
        chan->funcs |= RIG_FUNC_TONE;
    }

    if (me_struct.ct != 0)
    {
        chan->funcs |= RIG_FUNC_TSQL;
    }

    if (me_struct.reverse != 0)
    {
        chan->funcs |= RIG_FUNC_REV;
    }

    chan->ctcss_tone = rig->caps->ctcss_list[me_struct.tone_freq];
    chan->ctcss_sql = rig->caps->ctcss_list[me_struct.ct_freq];
    chan->dcs_code = 0;

    if (me_struct.dcs)
    {
        tone_t *dcs_list = common_dcs_list;
        chan->dcs_sql = dcs_list[me_struct.dcs_val];
    }
    else
    {
        chan->dcs_sql = 0;
    }

    retval = tmd710_get_rptr_shift_hamlib_value(me_struct.shift, &chan->rptr_shift);

    if (retval != RIG_OK)
    {
        return retval;
    }

    chan->rptr_offs = me_struct.offset;

    retval = tmd710_get_memory_name(rig, chan->channel_num, chan->channel_desc);

    if (retval != RIG_OK)
    {
        return retval;
    }

    chan->flags = RIG_CHFLAG_NONE;

    if (me_struct.lockout)
    {
        chan->flags |= RIG_CHFLAG_SKIP;
    }

    chan->tx_freq = me_struct.tx_freq;

    // Unsupported features
    chan->bank_num = 0;
    chan->ant = 0;
    chan->split = RIG_SPLIT_OFF;
    chan->tx_vfo = RIG_VFO_NONE;
    chan->tx_mode = RIG_MODE_NONE;
    chan->tx_width = 0;
    chan->rit = 0;
    chan->xit = 0;
    chan->scan_group = 0;
    // TODO: chan->levels
    chan->ext_levels = NULL;

    return RIG_OK;
}

int tmd710_set_channel(RIG *rig, vfo_t vfo, const channel_t *chan)
{
    int retval;
    tmd710_me me_struct;

    rig_debug(RIG_DEBUG_VERBOSE, "%s called\n", __func__);

    if (!rig || !chan)
    {
        return -RIG_EINVAL;
    }

    me_struct.channel = chan->channel_num;
    me_struct.freq = chan->freq;
    me_struct.tx_freq = chan->tx_freq;

    retval = tmd710_find_tuning_step_index(rig, chan->tuning_step, &me_struct.step);

    if (retval != RIG_OK)
    {
        return retval;
    }

    retval = tmd710_get_rptr_shift_tmd710_value(chan->rptr_shift, &me_struct.shift);

    if (retval != RIG_OK)
    {
        return retval;
    }

    me_struct.offset = chan->rptr_offs;

    me_struct.reverse = (chan->funcs & RIG_FUNC_REV) ? 1 : 0;
    me_struct.tone = (chan->funcs & RIG_FUNC_TONE) ? 1 : 0;
    me_struct.ct = (chan->funcs & RIG_FUNC_TSQL) ? 1 : 0;

    if (!me_struct.tone && chan->ctcss_tone == 0)
    {
        me_struct.tone_freq = 0;
    }
    else
    {
        retval = tmd710_find_ctcss_index(rig, chan->ctcss_tone, &me_struct.tone_freq);

        if (retval != RIG_OK)
        {
            return retval;
        }
    }

    if (!me_struct.ct && chan->ctcss_sql == 0)
    {
        me_struct.ct_freq = 0;
    }
    else
    {
        retval = tmd710_find_ctcss_index(rig, chan->ctcss_sql, &me_struct.ct_freq);

        if (retval != RIG_OK)
        {
            return retval;
        }
    }

    if (chan->dcs_sql == 0)
    {
        me_struct.dcs = 0;
        me_struct.dcs_val = 0;
    }
    else
    {
        retval = tmd710_find_dcs_index(chan->dcs_sql, &me_struct.dcs_val);

        if (retval != RIG_OK)
        {
            return retval;
        }

        me_struct.dcs = 1;
    }

    me_struct.lockout = (chan->flags & RIG_CHFLAG_SKIP) ? 1 : 0;

    retval = tmd710_get_mode_tmd710_value(chan->mode, &me_struct.mode);

    if (retval != RIG_OK)
    {
        return retval;
    }

    me_struct.p15_unknown = 0;

    retval = tmd710_push_me(rig, &me_struct);

    if (retval != RIG_OK)
    {
        return retval;
    }

    return tmd710_set_memory_name(rig, me_struct.channel,
                                  (char *) chan->channel_desc);
}

int tmd710_set_ptt(RIG *rig, vfo_t vfo, ptt_t ptt)
{
    char ackbuf[32];

    rig_debug(RIG_DEBUG_TRACE, "%s: called\n", __func__);

    return kenwood_transaction(rig, (ptt == RIG_PTT_ON) ? "TX" : "RX", ackbuf,
                               sizeof(ackbuf));
}

int tmd710_get_dcd(RIG *rig, vfo_t vfo, dcd_t *dcd)
{
    char cmd[8], buf[8];
    int retval;
    int vfonum, dcd_val;

    retval = tmd710_resolve_vfo(rig, vfo, NULL, &vfonum);

    if (retval != RIG_OK)
    {
        return retval;
    }

    snprintf(cmd, sizeof(cmd), "BY %d", vfonum);

    retval = kenwood_safe_transaction(rig, cmd, buf, sizeof(buf), 6);

    if (retval != RIG_OK)
    {
        return retval;
    }

    retval = sscanf(buf, "BY %d,%d", &vfonum, &dcd_val);

    if (retval != 2)
    {
        rig_debug(RIG_DEBUG_ERR, "%s: unexpected reply '%s', len=%ld\n", __func__, buf,
                  (long)strlen(buf));
        return -RIG_EPROTO;
    }

    switch (dcd_val)
    {
    case 0:
        *dcd = RIG_DCD_OFF;
        break;

    case 1:
        *dcd = RIG_DCD_ON;
        break;

    default:
        rig_debug(RIG_DEBUG_ERR, "%s: unexpected reply '%s', len=%ld\n", __func__, buf,
                  (long)strlen(buf));
        return -RIG_ERJCTED;
    }

    return RIG_OK;
}

int tmd710_vfo_op(RIG *rig, vfo_t vfo, vfo_op_t op)
{
    char ackbuf[8];
    rig_debug(RIG_DEBUG_TRACE, "%s: called\n", __func__);

    switch (op)
    {
    case RIG_OP_UP:
        return kenwood_transaction(rig, "UP", ackbuf, sizeof(ackbuf));

    case RIG_OP_DOWN:
        return kenwood_transaction(rig, "DW", ackbuf, sizeof(ackbuf));

    default:
        return -RIG_EINVAL;
    }
}

/*
 * tmd710_get_level
 * Assumes rig!=NULL, val!=NULL
 */
int tmd710_get_level(RIG *rig, vfo_t vfo, setting_t level, value_t *val)
{
    char buf[10], ackbuf[20];
    int retval, v, l;
    int vfonum;

    rig_debug(RIG_DEBUG_TRACE, "%s: called\n", __func__);

    retval = tmd710_resolve_vfo(rig, vfo, NULL, &vfonum);

    if (retval != RIG_OK)
    {
        return retval;
    }

    switch (level)
    {
    case RIG_LEVEL_RFPOWER:
        snprintf(buf, sizeof(buf), "PC %d", vfonum);
        retval = kenwood_transaction(rig, buf, ackbuf, sizeof(ackbuf));

        if (retval != RIG_OK)
        {
            return retval;
        }

        retval = sscanf(ackbuf, "PC %d,%d", &v, &l);

        if (retval != 2 || l < 0 || l > 2)
        {
            rig_debug(RIG_DEBUG_ERR, "%s: Unexpected reply '%s'\n", __func__, ackbuf);
            return -RIG_ERJCTED;
        }

        /* range [0.0 ... 1.0] */
        val->f = (float)(l - TMD710_RF_POWER_MIN) / (float)(TMD710_RF_POWER_MAX -
                 TMD710_RF_POWER_MIN);

        // RF power needs to be inverted
        val->f = 1.0f - val->f;
        break;

    case RIG_LEVEL_SQL:
        snprintf(buf, sizeof(buf), "SQ %d", vfonum);
        retval = kenwood_transaction(rig, buf, ackbuf, sizeof(ackbuf));

        if (retval != RIG_OK)
        {
            return retval;
        }

        retval = sscanf(ackbuf, "SQ %X", &l);

        if (retval != 1 || l < TMD710_SQL_MIN || l > TMD710_SQL_MAX)
        {
            rig_debug(RIG_DEBUG_ERR, "%s: Unexpected reply '%s'\n", __func__, ackbuf);
            return -RIG_ERJCTED;
        }

        /* range [0.0 ... 1.0] */
        val->f = (float)(l - TMD710_SQL_MIN) / (float)(TMD710_SQL_MAX - TMD710_SQL_MIN);
        break;

    default:
        rig_debug(RIG_DEBUG_ERR, "%s: Unsupported Level %ld\n", __func__, (long)level);
        return -RIG_EINVAL;
    }

    return RIG_OK;
}

int tmd710_set_level(RIG *rig, vfo_t vfo, setting_t level, value_t val)
{
    char buf[12], ackbuf[12];
    int vfonum;
    int retval;

    rig_debug(RIG_DEBUG_TRACE, "%s: called\n", __func__);

    retval = tmd710_resolve_vfo(rig, vfo, NULL, &vfonum);

    if (retval != RIG_OK)
    {
        return retval;
    }

    switch (level)
    {
    case RIG_LEVEL_RFPOWER:
        // RF power needs to be inverted
        snprintf(buf, sizeof(buf), "PC %d,%d", vfonum,
                 (int)((1.0f - val.f) * (TMD710_RF_POWER_MAX - TMD710_RF_POWER_MIN) +
                       TMD710_RF_POWER_MIN));

        return kenwood_transaction(rig, buf, ackbuf, sizeof(ackbuf));

    case RIG_LEVEL_SQL:
        snprintf(buf, sizeof(buf), "SQ %d,%02X", vfonum,
                 (int)(val.f * (TMD710_SQL_MAX - TMD710_SQL_MIN) + TMD710_SQL_MIN));
        return kenwood_transaction(rig, buf, ackbuf, sizeof(ackbuf));

    default:
        rig_debug(RIG_DEBUG_ERR, "%s: Unsupported Level %ld\n", __func__, (long)level);
        return -RIG_EINVAL;
    }
}

static int tmd710_tburst(RIG *rig, int status)
{
    char ackbuf[8];
    return kenwood_transaction(rig, (status == 1) ? "TT" : "RX", ackbuf,
                               sizeof(ackbuf));
}

/*
 * tmd710_get_kenwood_func
 * Assumes rig!=NULL, status!=NULL
 */
static int tmd710_get_kenwood_func(RIG *rig, const char *cmd, int *status)
{
    char buf[8];
    int retval, len, expected;

    rig_debug(RIG_DEBUG_TRACE, "%s: called\n", __func__);

    len = strlen(cmd);
    expected = len + 2;

    retval = kenwood_safe_transaction(rig, cmd, buf, sizeof(buf), expected);

    if (retval != RIG_OK)
    {
        return retval;
    }

    if (status)
    {
        *status = (buf[len + 1] == '0') ? 0 : 1;
    }

    return RIG_OK;
};

/*
 * tmd710_set_kenwood_func
 * Assumes rig!=NULL, status!=NULL
 */
static int tmd710_set_kenwood_func(RIG *rig, const char *cmd, int status)
{
    char buf[16], ackbuf[16];

    rig_debug(RIG_DEBUG_TRACE, "%s: cmd = %s, status = %d\n", __func__, cmd,
              status);

    strncpy(buf, cmd, sizeof(buf) - 2);
    buf[sizeof(buf) - 1] = '\0';
    strncat(buf, status ? " 1" : " 0", sizeof(buf) - 1);

    return kenwood_transaction(rig, buf, ackbuf, sizeof(ackbuf));
}

/*
 * tmd710_get_func
 * Assumes rig!=NULL, status!=NULL
 */
int tmd710_get_func(RIG *rig, vfo_t vfo, setting_t func, int *status)
{
    int retval;
    int use_fo = 0, use_mu = 0;
    tmd710_fo fo_struct;
    tmd710_mu mu_struct;

    rig_debug(RIG_DEBUG_TRACE, "%s: called (0x%04lx)\n", __func__, (long)func);

    switch (func)
    {
    case RIG_FUNC_TONE:
    case RIG_FUNC_TSQL:
    case RIG_FUNC_REV:
        use_fo = 1;
        break;

    case RIG_FUNC_ARO:
    case RIG_FUNC_AIP:
    case RIG_FUNC_RESUME:
        use_mu = 1;
        break;
    }

    if (use_fo)
    {
        retval = tmd710_pull_fo(rig, vfo, &fo_struct);

        if (retval != RIG_OK)
        {
            return retval;
        }
    }

    if (use_mu)
    {
        retval = tmd710_pull_mu(rig, &mu_struct);

        if (retval != RIG_OK)
        {
            return retval;
        }
    }

    switch (func)
    {
    case RIG_FUNC_TONE:
        *status = (fo_struct.tone != 0) ? 1 : 0;
        break;

    case RIG_FUNC_TSQL:
        *status = (fo_struct.ct != 0) ? 1 : 0;
        break;

    case RIG_FUNC_REV:
        *status = (fo_struct.reverse != 0) ? 1 : 0;
        break;

    case RIG_FUNC_LOCK:
        return tmd710_get_kenwood_func(rig, "LK", status);

    case RIG_FUNC_ARO:
        *status = (mu_struct.auto_repeater_offset != 0) ? 1 : 0;
        break;

    case RIG_FUNC_AIP:
        *status = ((mu_struct.vhf_aip != 0) || (mu_struct.uhf_aip != 0)) ? 1 : 0;
        break;

    case RIG_FUNC_RESUME:
        *status = (mu_struct.scan_resume == TMD710_SCAN_RESUME_TIME) ? 1 : 0;
        break;

    default:
        rig_debug(RIG_DEBUG_ERR, "%s: Unsupported function %#lx\n", __func__,
                  (long)func);
        return -RIG_EINVAL;
    }

    return RIG_OK;
}

/*
 * tmd710_set_func
 * Assumes rig!=NULL, status!=NULL
 */
int tmd710_set_func(RIG *rig, vfo_t vfo, setting_t func, int status)
{
    int retval;
    int use_fo = 0, use_mu = 0;
    tmd710_fo fo_struct;
    tmd710_mu mu_struct;

    rig_debug(RIG_DEBUG_TRACE, "%s: called (0x%04lx)\n", __func__, (long)func);

    switch (func)
    {
    case RIG_FUNC_TONE:
    case RIG_FUNC_TSQL:
    case RIG_FUNC_REV:
        use_fo = 1;
        break;

    case RIG_FUNC_ARO:
    case RIG_FUNC_AIP:
    case RIG_FUNC_RESUME:
        use_mu = 1;
        break;
    }

    if (use_fo)
    {
        retval = tmd710_pull_fo(rig, vfo, &fo_struct);

        if (retval != RIG_OK)
        {
            return retval;
        }
    }

    if (use_mu)
    {
        retval = tmd710_pull_mu(rig, &mu_struct);

        if (retval != RIG_OK)
        {
            return retval;
        }
    }

    switch (func)
    {
    case RIG_FUNC_TONE:
        fo_struct.tone = status ? 1 : 0;
        break;

    case RIG_FUNC_TSQL:
        fo_struct.ct = status ? 1 : 0;
        break;

    case RIG_FUNC_REV:
        fo_struct.reverse = status ? 1 : 0;
        break;

    case RIG_FUNC_ARO:
        mu_struct.auto_repeater_offset = status ? 1 : 0;
        break;

    case RIG_FUNC_AIP:
        mu_struct.vhf_aip = status ? 1 : 0;
        mu_struct.uhf_aip = status ? 1 : 0;
        break;

    case RIG_FUNC_RESUME:
        mu_struct.scan_resume = status ? TMD710_SCAN_RESUME_TIME :
                                TMD710_SCAN_RESUME_CARRIER;
        break;

    case RIG_FUNC_LOCK:
        return tmd710_set_kenwood_func(rig, "LK", status);

    case RIG_FUNC_TBURST:
        return tmd710_tburst(rig, status);

    default:
        rig_debug(RIG_DEBUG_ERR, "%s: Unsupported function %#lx\n", __func__,
                  (long)func);
        return -RIG_EINVAL;
    }

    if (use_fo)
    {
        return tmd710_push_fo(rig, vfo, &fo_struct);
    }

    if (use_mu)
    {
        return tmd710_push_mu(rig, &mu_struct);
    }

    return -RIG_EINVAL;
}

/*
 * tmd710_get_parm
 * Assumes rig!=NULL, status!=NULL
 */
int tmd710_get_parm(RIG *rig, setting_t parm, value_t *val)
{
    int retval;
    tmd710_mu mu_struct;

    rig_debug(RIG_DEBUG_TRACE, "%s: called (0x%04lx)\n", __func__, (long)parm);

    retval = tmd710_pull_mu(rig, &mu_struct);

    if (retval != RIG_OK)
    {
        return retval;
    }

    switch (parm)
    {
    case RIG_PARM_BEEP:
        val->i = mu_struct.beep ? 1 : 0;
        break;

    case RIG_PARM_APO:
        if (mu_struct.auto_power_off == TMD710_AUTO_POWER_OFF_180MIN)
        {
            val->i = 180;
        }
        else
        {
            val->i = mu_struct.auto_power_off * 30;
        }

        break;

    case RIG_PARM_BACKLIGHT:
        val->f = ((float) mu_struct.brightness_level) / 8.0f;
        break;

    default:
        rig_debug(RIG_DEBUG_ERR, "%s: Unsupported parm %#lx\n", __func__, (long)parm);
        return -RIG_EINVAL;
    }

    return RIG_OK;
}

int tmd710_set_parm(RIG *rig, setting_t parm, value_t val)
{
    int retval;
    tmd710_mu mu_struct;

    rig_debug(RIG_DEBUG_TRACE, "%s: called\n", __func__);

    retval = tmd710_pull_mu(rig, &mu_struct);

    if (retval != RIG_OK)
    {
        return retval;
    }

    switch (parm)
    {
    case RIG_PARM_BEEP:
        mu_struct.beep = val.i ? 1 : 0;
        break;

    case RIG_PARM_BACKLIGHT:
        if (val.f < 0 || val.f > 1)
        {
            return -RIG_EINVAL;
        }

        mu_struct.brightness_level = (int)(val.f * 8);
        break;

    case RIG_PARM_APO:
    {
        int value = 0;

        if (val.i > 120)
        {
            value = TMD710_AUTO_POWER_OFF_180MIN;
        }
        else if (val.i > 90)
        {
            value = TMD710_AUTO_POWER_OFF_120MIN;
        }
        else if (val.i > 60)
        {
            value = TMD710_AUTO_POWER_OFF_90MIN;
        }
        else if (val.i > 30)
        {
            value = TMD710_AUTO_POWER_OFF_60MIN;
        }
        else if (val.i > 0)
        {
            value = TMD710_AUTO_POWER_OFF_30MIN;
        }

        mu_struct.auto_power_off = value;
        break;
    }

    default:
        rig_debug(RIG_DEBUG_ERR, "%s: Unsupported parm %#lx\n", __func__, (long)parm);
        return -RIG_EINVAL;
    }

    return tmd710_push_mu(rig, &mu_struct);
}

/*
 * tmd710_get_ext_level
 * Assumes rig!=NULL, rig->state.priv!=NULL, val!=NULL
 *
 */
int tmd710_get_ext_level(RIG *rig, vfo_t vfo, token_t token, value_t *val)
{
    int retval;
    tmd710_mu mu_struct;

    rig_debug(RIG_DEBUG_VERBOSE, "%s called\n", __func__);

    retval = tmd710_pull_mu(rig, &mu_struct);

    if (retval != RIG_OK)
    {
        return retval;
    }

    switch (token)
    {
    case TOK_LEVEL_EXT_DATA_BAND:
        val->i = mu_struct.ext_data_band;
        break;

    default:
        rig_debug(RIG_DEBUG_ERR, "%s: Unsupported ext level %ld\n", __func__,
                  (long)token);
        return -RIG_EINVAL;
    }

    return RIG_OK;
}

/*
 * tmd710_set_ext_level
 * Assumes rig!=NULL, rig->state.priv!=NULL
 *
 */
int tmd710_set_ext_level(RIG *rig, vfo_t vfo, token_t token, value_t val)
{
    int retval;
    tmd710_mu mu_struct;

    rig_debug(RIG_DEBUG_VERBOSE, "%s called\n", __func__);

    retval = tmd710_pull_mu(rig, &mu_struct);

    if (retval != RIG_OK)
    {
        return retval;
    }

    switch (token)
    {
    case TOK_LEVEL_EXT_DATA_BAND:
    {
        int v = val.i;

        if (v != TMD710_EXT_DATA_BAND_A && v != TMD710_EXT_DATA_BAND_B
                && v != TMD710_EXT_DATA_BAND_TXA_RXB && v != TMD710_EXT_DATA_BAND_TXB_RXA)
        {
            return -RIG_EINVAL;
        }

        mu_struct.ext_data_band = v;
        break;
    }

    default:
        rig_debug(RIG_DEBUG_ERR, "%s: Unsupported ext level %ld\n", __func__,
                  (long)token);
        return -RIG_EINVAL;
    }

    return tmd710_push_mu(rig, &mu_struct);
}
