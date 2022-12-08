/* -*- mode: c; c-basic-offset: 2; indent-tabs-mode: nil; -*-
 *
 * ft897.c - (C) Tomi Manninen 2003 (oh2bns@sral.fi)
 *           (C) Stephane Fillod 2009
 *
 *  ...derived but heavily modified from:
 *
 * ft817.c - (C) Chris Karpinsky 2001 (aa1vl@arrl.net)
 *
 * This shared library provides an API for communicating
 * via serial interface to an FT-897 using the "CAT" interface.
 * The starting point for this code was Frank's ft847 implementation.
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

/*
 * Unimplemented features supported by the FT-897:
 *
 *   - DCS encoder/squelch ON/OFF, similar to RIG_FUNC_TONE/TSQL.
 *     Needs frontend support.
 *
 *   - RX status command returns info that is not used:
 *
 *      - discriminator centered (yes/no flag)
 *      - received ctcss/dcs matched (yes/no flag)
 *
 * The manual also indicates that CTCSS and DCS codes can be set
 * separately for tx and rx, but this doesn't seem to work. It
 * doesn't work from front panel either.
 *
 * DONE (--sf):
 *
 *   - VFO A/B toggle. Needs frontend support (RIG_OP_TOGGLE)
 *
 *   - Split ON/OFF. Maybe some sort of split operation could
 *     be supported with this and the above???
 *
 *   - TX status command returns info that is not used:
 *
 *      - split on/off flag
 *      - high swr flag
 */

#include <hamlib/config.h>

#include <stdlib.h>
#include <string.h>     /* String function definitions */

#ifdef HAVE_SYS_TIME_H
#include <sys/time.h>
#endif

#include "hamlib/rig.h"
#include "serial.h"
#include "yaesu.h"
#include "ft897.h"
#include "ft817.h" /* We use functions from the 817 code */
#include "ft857.h" //Needed for ft857_set_vfo, ft857_get_vfo
#include "misc.h"
#include "tones.h"
#include "bandplan.h"

enum ft897_native_cmd_e
{
    FT897_NATIVE_CAT_LOCK_ON = 0,
    FT897_NATIVE_CAT_LOCK_OFF,
    FT897_NATIVE_CAT_PTT_ON,
    FT897_NATIVE_CAT_PTT_OFF,
    FT897_NATIVE_CAT_SET_FREQ,
    FT897_NATIVE_CAT_SET_MODE_LSB,
    FT897_NATIVE_CAT_SET_MODE_USB,
    FT897_NATIVE_CAT_SET_MODE_CW,
    FT897_NATIVE_CAT_SET_MODE_CWR,
    FT897_NATIVE_CAT_SET_MODE_AM,
    FT897_NATIVE_CAT_SET_MODE_FM,
    FT897_NATIVE_CAT_SET_MODE_FM_N,
    FT897_NATIVE_CAT_SET_MODE_DIG,
    FT897_NATIVE_CAT_SET_MODE_PKT,
    FT897_NATIVE_CAT_CLAR_ON,
    FT897_NATIVE_CAT_CLAR_OFF,
    FT897_NATIVE_CAT_SET_CLAR_FREQ,
    FT897_NATIVE_CAT_SET_VFOAB,
    FT897_NATIVE_CAT_SPLIT_ON,
    FT897_NATIVE_CAT_SPLIT_OFF,
    FT897_NATIVE_CAT_SET_RPT_SHIFT_MINUS,
    FT897_NATIVE_CAT_SET_RPT_SHIFT_PLUS,
    FT897_NATIVE_CAT_SET_RPT_SHIFT_SIMPLEX,
    FT897_NATIVE_CAT_SET_RPT_OFFSET,
    FT897_NATIVE_CAT_SET_DCS_ON,
    FT897_NATIVE_CAT_SET_DCS_DEC_ON,
    FT897_NATIVE_CAT_SET_DCS_ENC_ON,
    FT897_NATIVE_CAT_SET_CTCSS_ON,
    FT897_NATIVE_CAT_SET_CTCSS_DEC_ON,
    FT897_NATIVE_CAT_SET_CTCSS_ENC_ON,
    FT897_NATIVE_CAT_SET_CTCSS_DCS_OFF,
    FT897_NATIVE_CAT_SET_CTCSS_FREQ,
    FT897_NATIVE_CAT_SET_DCS_CODE,
    FT897_NATIVE_CAT_GET_RX_STATUS,
    FT897_NATIVE_CAT_GET_TX_STATUS,
    FT897_NATIVE_CAT_GET_FREQ_MODE_STATUS,
    FT897_NATIVE_CAT_PWR_WAKE,
    FT897_NATIVE_CAT_PWR_ON,
    FT897_NATIVE_CAT_PWR_OFF,
    FT897_NATIVE_CAT_EEPROM_READ,
    FT897_NATIVE_CAT_GET_TX_METER,
    FT897_NATIVE_SIZE     /* end marker */
};

struct ft897_priv_data
{
    /* rx status */
    struct timeval rx_status_tv;
    unsigned char rx_status;

    /* tx status */
    struct timeval tx_status_tv;
    unsigned char tx_status;

    /* freq & mode status */
    struct timeval fm_status_tv;
    unsigned char fm_status[YAESU_CMD_LENGTH + 1];

    /* tx meter status */
    struct timeval tm_status_tv;
    unsigned char tm_status[3];
};



static int ft897_init(RIG *rig);
static int ft897_open(RIG *rig);
static int ft897_cleanup(RIG *rig);
static int ft897_close(RIG *rig);
static int ft897_set_freq(RIG *rig, vfo_t vfo, freq_t freq);
static int ft897_get_freq(RIG *rig, vfo_t vfo, freq_t *freq);
static int ft897_set_mode(RIG *rig, vfo_t vfo, rmode_t mode, pbwidth_t width);
static int ft897_get_mode(RIG *rig, vfo_t vfo, rmode_t *mode, pbwidth_t *width);
static int ft897_set_split_vfo(RIG *rig, vfo_t vfo, split_t split,
                               vfo_t tx_vfo);
static int ft897_get_split_vfo(RIG *rig, vfo_t vfo, split_t *split,
                               vfo_t *tx_vfo);
static int ft897_vfo_op(RIG *rig, vfo_t vfo, vfo_op_t op);
static int ft897_set_ptt(RIG *rig, vfo_t vfo, ptt_t ptt);
static int ft897_get_ptt(RIG *rig, vfo_t vfo, ptt_t *ptt);
// static int ft897_set_level(RIG *rig, vfo_t vfo, setting_t level, value_t val);
static int ft897_get_level(RIG *rig, vfo_t vfo, setting_t level, value_t *val);
static int ft897_set_func(RIG *rig, vfo_t vfo, setting_t func, int status);
// static int ft897_get_func(RIG *rig, vfo_t vfo, setting_t func, int *status);
// static int ft897_set_parm(RIG *rig, setting_t parm, value_t val);
// static int ft897_get_parm(RIG *rig, setting_t parm, value_t *val);
static int ft897_set_dcs_code(RIG *rig, vfo_t vfo, tone_t code);
static int ft897_set_ctcss_tone(RIG *rig, vfo_t vfo, tone_t code);
static int ft897_set_dcs_sql(RIG *rig, vfo_t vfo, tone_t code);
static int ft897_set_ctcss_sql(RIG *rig, vfo_t vfo, tone_t tone);
static int ft897_set_rptr_shift(RIG *rig, vfo_t vfo, rptr_shift_t rptr_shift);
static int ft897_set_rptr_offs(RIG *rig, vfo_t vfo, shortfreq_t offs);
static int ft897_set_rit(RIG *rig, vfo_t vfo, shortfreq_t rit);
static int ft897_get_dcd(RIG *rig, vfo_t vfo, dcd_t *dcd);

/* Native ft897 cmd set prototypes. These are READ ONLY as each */
/* rig instance will copy from these and modify if required . */
/* Complete sequences (1) can be read and used directly as a cmd sequence . */
/* Incomplete sequences (0) must be completed with extra parameters */
/* eg: mem number, or freq etc.. */

static const yaesu_cmd_set_t ncmd[] =
{
    { 1, { 0x00, 0x00, 0x00, 0x00, 0x00 } }, /* lock on */
    { 1, { 0x00, 0x00, 0x00, 0x00, 0x80 } }, /* lock off */
    { 1, { 0x00, 0x00, 0x00, 0x00, 0x08 } }, /* ptt on */
    { 1, { 0x00, 0x00, 0x00, 0x00, 0x88 } }, /* ptt off */
    { 0, { 0x00, 0x00, 0x00, 0x00, 0x01 } }, /* set freq */
    { 1, { 0x00, 0x00, 0x00, 0x00, 0x07 } }, /* mode set main LSB */
    { 1, { 0x01, 0x00, 0x00, 0x00, 0x07 } }, /* mode set main USB */
    { 1, { 0x02, 0x00, 0x00, 0x00, 0x07 } }, /* mode set main CW */
    { 1, { 0x03, 0x00, 0x00, 0x00, 0x07 } }, /* mode set main CWR */
    { 1, { 0x04, 0x00, 0x00, 0x00, 0x07 } }, /* mode set main AM */
    { 1, { 0x08, 0x00, 0x00, 0x00, 0x07 } }, /* mode set main FM */
    { 1, { 0x88, 0x00, 0x00, 0x00, 0x07 } }, /* mode set main FM-N */
    { 1, { 0x0a, 0x00, 0x00, 0x00, 0x07 } }, /* mode set main DIG */
    { 1, { 0x0c, 0x00, 0x00, 0x00, 0x07 } }, /* mode set main PKT */
    { 1, { 0x00, 0x00, 0x00, 0x00, 0x05 } }, /* clar on */
    { 1, { 0x00, 0x00, 0x00, 0x00, 0x85 } }, /* clar off */
    { 0, { 0x00, 0x00, 0x00, 0x00, 0xf5 } }, /* set clar freq */
    { 1, { 0x00, 0x00, 0x00, 0x00, 0x81 } }, /* toggle vfo a/b */
    { 1, { 0x00, 0x00, 0x00, 0x00, 0x02 } }, /* split on */
    { 1, { 0x00, 0x00, 0x00, 0x00, 0x82 } }, /* split off */
    { 1, { 0x09, 0x00, 0x00, 0x00, 0x09 } }, /* set RPT shift MINUS */
    { 1, { 0x49, 0x00, 0x00, 0x00, 0x09 } }, /* set RPT shift PLUS */
    { 1, { 0x89, 0x00, 0x00, 0x00, 0x09 } }, /* set RPT shift SIMPLEX */
    { 0, { 0x00, 0x00, 0x00, 0x00, 0xf9 } }, /* set RPT offset freq */
    { 1, { 0x0a, 0x00, 0x00, 0x00, 0x0a } }, /* set DCS on */
    { 1, { 0x0b, 0x00, 0x00, 0x00, 0x0a } }, /* set DCS decoder on */
    { 1, { 0x0c, 0x00, 0x00, 0x00, 0x0a } }, /* set DCS encoder on */
    { 1, { 0x2a, 0x00, 0x00, 0x00, 0x0a } }, /* set CTCSS on */
    { 1, { 0x3a, 0x00, 0x00, 0x00, 0x0a } }, /* set CTCSS decoder on */
    { 1, { 0x4a, 0x00, 0x00, 0x00, 0x0a } }, /* set CTCSS encoder on */
    { 1, { 0x8a, 0x00, 0x00, 0x00, 0x0a } }, /* set CTCSS/DCS off */
    { 0, { 0x00, 0x00, 0x00, 0x00, 0x0b } }, /* set CTCSS tone */
    { 0, { 0x00, 0x00, 0x00, 0x00, 0x0c } }, /* set DCS code */
    { 1, { 0x00, 0x00, 0x00, 0x00, 0xe7 } }, /* get RX status  */
    { 1, { 0x00, 0x00, 0x00, 0x00, 0xf7 } }, /* get TX status  */
    { 1, { 0x00, 0x00, 0x00, 0x00, 0x03 } }, /* get FREQ and MODE status */
    { 1, { 0xff, 0xff, 0xff, 0xff, 0xff } }, /* pwr wakeup sequence */
    { 1, { 0x00, 0x00, 0x00, 0x00, 0x0f } }, /* pwr on */
    { 1, { 0x00, 0x00, 0x00, 0x00, 0x8f } }, /* pwr off */
    { 0, { 0x00, 0x00, 0x00, 0x00, 0xbb } }, /* eeprom read */
    { 1, { 0x00, 0x00, 0x00, 0x00, 0xbd } }, /* tx meter status, i.e ALC, MOD, PWR, SWR */
};

enum ft897_digi
{
    FT897_DIGI_RTTY_L = 0,
    FT897_DIGI_RTTY_U,
    FT897_DIGI_PSK_L,
    FT897_DIGI_PSK_U,
    FT897_DIGI_USER_L,
    FT897_DIGI_USER_U,
};

#define FT897_ALL_RX_MODES      (RIG_MODE_AM|RIG_MODE_CW|RIG_MODE_CWR|RIG_MODE_USB|\
                                 RIG_MODE_LSB|RIG_MODE_RTTY|RIG_MODE_FM|RIG_MODE_PKTUSB)
#define FT897_SSB_CW_RX_MODES   (RIG_MODE_CW|RIG_MODE_CWR|RIG_MODE_USB|RIG_MODE_LSB)
#define FT897_AM_FM_RX_MODES    (RIG_MODE_AM|RIG_MODE_FM)

#define FT897_OTHER_TX_MODES    (RIG_MODE_AM|RIG_MODE_CW|RIG_MODE_USB|\
                                 RIG_MODE_LSB|RIG_MODE_RTTY|RIG_MODE_FM|RIG_MODE_PKTUSB)
#define FT897_AM_TX_MODES       (RIG_MODE_AM)

#define FT897_VFO_ALL           (RIG_VFO_A|RIG_VFO_B)
#define FT897_ANTS              0

const struct rig_caps ft897_caps =
{
    RIG_MODEL(RIG_MODEL_FT897),
    .model_name =     "FT-897",
    .mfg_name =       "Yaesu",
    .version =        "20220404.0",
    .copyright =      "LGPL",
    .status =         RIG_STATUS_STABLE,
    .rig_type =       RIG_TYPE_TRANSCEIVER,
    .ptt_type =       RIG_PTT_RIG,
    .dcd_type =       RIG_DCD_RIG,
    .port_type =      RIG_PORT_SERIAL,
    .serial_rate_min =    4800,
    .serial_rate_max =    38400,
    .serial_data_bits =   8,
    .serial_stop_bits =   2,
    .serial_parity =  RIG_PARITY_NONE,
    .serial_handshake =   RIG_HANDSHAKE_NONE,
    .write_delay =    FT897_WRITE_DELAY,
    .post_write_delay =   FT897_POST_WRITE_DELAY,
    .timeout =        FT897_TIMEOUT,
    .retry =      0,
    .has_get_func =       RIG_FUNC_NONE,
    .has_set_func =   RIG_FUNC_LOCK | RIG_FUNC_TONE | RIG_FUNC_TSQL | RIG_FUNC_CSQL | RIG_FUNC_RIT,
    .has_get_level =  RIG_LEVEL_STRENGTH | RIG_LEVEL_RFPOWER | RIG_LEVEL_SWR | RIG_LEVEL_RAWSTR | RIG_LEVEL_ALC,
    .has_set_level =  RIG_LEVEL_BAND_SELECT,
    .has_get_parm =   RIG_PARM_NONE,
    .has_set_parm =   RIG_PARM_NONE,
    .level_gran =
    {
#include "level_gran_yaesu.h"
    },
    .parm_gran =      {},
    .ctcss_list =     common_ctcss_list,
    .dcs_list =       common_dcs_list,   /* only 104 supported */
    .preamp =         { RIG_DBLST_END, },
    .attenuator =     { RIG_DBLST_END, },
    .max_rit =        Hz(9990),
    .max_xit =        Hz(0),
    .max_ifshift =    Hz(0),
    .targetable_vfo =     0,
    .transceive =     RIG_TRN_OFF,
    .bank_qty =       0,
    .chan_desc_sz =   0,
    .chan_list =          { RIG_CHAN_END, },
    .vfo_ops =        RIG_OP_TOGGLE,

    .rx_range_list1 =  {
        {kHz(100), MHz(56), FT897_ALL_RX_MODES, -1, -1},
        {MHz(76), MHz(108), RIG_MODE_WFM,      -1, -1},
        {MHz(118), MHz(164), FT897_ALL_RX_MODES, -1, -1},
        {MHz(420), MHz(470), FT897_ALL_RX_MODES, -1, -1},
        RIG_FRNG_END,
    },
    .tx_range_list1 =  {
        FRQ_RNG_HF(1, FT897_OTHER_TX_MODES, W(10), W(100), FT897_VFO_ALL, FT897_ANTS),
        FRQ_RNG_6m(1, FT897_OTHER_TX_MODES, W(10), W(100), FT897_VFO_ALL, FT897_ANTS),

        /* AM class */
        FRQ_RNG_HF(1, FT897_AM_TX_MODES, W(2.5), W(25), FT897_VFO_ALL, FT897_ANTS),
        FRQ_RNG_6m(1, FT897_AM_TX_MODES, W(2.5), W(25), FT897_VFO_ALL, FT897_ANTS),
        FRQ_RNG_2m(1, FT897_OTHER_TX_MODES, W(5), W(50), FT897_VFO_ALL, FT897_ANTS),
        /* AM class */
        FRQ_RNG_2m(1, FT897_AM_TX_MODES, W(2.5), W(25), FT897_VFO_ALL, FT897_ANTS),
        FRQ_RNG_70cm(1, FT897_OTHER_TX_MODES, W(2), W(20), FT897_VFO_ALL, FT897_ANTS),
        /* AM class */
        FRQ_RNG_70cm(1, FT897_AM_TX_MODES, W(0.5), W(5), FT897_VFO_ALL, FT897_ANTS),
        RIG_FRNG_END,
    },


    .rx_range_list2 =  {
        {kHz(100), MHz(56), FT897_ALL_RX_MODES, -1, -1},
        {MHz(76), MHz(108), RIG_MODE_WFM,      -1, -1},
        {MHz(118), MHz(164), FT897_ALL_RX_MODES, -1, -1},
        {MHz(420), MHz(470), FT897_ALL_RX_MODES, -1, -1},
        RIG_FRNG_END,
    },
    .tx_range_list2 =  {
        FRQ_RNG_HF(2, FT897_OTHER_TX_MODES, W(10), W(100), FT897_VFO_ALL, FT897_ANTS),
        /* AM class */
        FRQ_RNG_HF(2, FT897_AM_TX_MODES, W(2.5), W(25), FT897_VFO_ALL, FT897_ANTS),
        FRQ_RNG_2m(2, FT897_OTHER_TX_MODES, W(5), W(50), FT897_VFO_ALL, FT897_ANTS),
        /* AM class */
        FRQ_RNG_2m(2, FT897_AM_TX_MODES, W(2.5), W(25), FT897_VFO_ALL, FT897_ANTS),
        FRQ_RNG_70cm(2, FT897_OTHER_TX_MODES, W(2), W(20), FT897_VFO_ALL, FT897_ANTS),
        /* AM class */
        FRQ_RNG_70cm(2, FT897_AM_TX_MODES, W(0.5), W(5), FT897_VFO_ALL, FT897_ANTS),
        RIG_FRNG_END,
    },

    .tuning_steps =  {
        {FT897_SSB_CW_RX_MODES, 10},
        {FT897_SSB_CW_RX_MODES, 100},
        {FT897_AM_FM_RX_MODES, 10},
        {FT897_AM_FM_RX_MODES, 100},
        RIG_TS_END,
    },

    /* filter selection is not supported by CAT functions
     * per testing by Rich Newsom, WA4SXZ
     */
    .filters =  {
        {RIG_MODE_ALL, RIG_FLT_ANY},
//        {RIG_MODE_SSB, kHz(2.2)},
//        {RIG_MODE_CW, kHz(2.2)},
//        {RIG_MODE_CWR, kHz(2.2)},
//        {RIG_MODE_RTTY, kHz(2.2)},
//        {RIG_MODE_AM, kHz(6)},
//        {RIG_MODE_FM, kHz(15)},
//        {RIG_MODE_PKTFM, kHz(15)},
//        {RIG_MODE_FM, kHz(9)},
//        {RIG_MODE_PKTFM, kHz(9)},
//        {RIG_MODE_WFM, kHz(230)}, /* ?? */
        RIG_FLT_END,
    },

    .rig_init =       ft897_init,
    .rig_cleanup =    ft897_cleanup,
    .rig_open =       ft897_open,
    .rig_close =      ft897_close,
//    .get_vfo =        ft857_get_vfo,
    // set_vfo not working on serial# 5n660296
//    .set_vfo =        ft857_set_vfo,
    .set_vfo =        NULL,
    .set_freq =       ft897_set_freq,
    .get_freq =       ft897_get_freq,
    .set_mode =       ft897_set_mode,
    .get_mode =       ft897_get_mode,
    .set_ptt =        ft897_set_ptt,
    .get_ptt =        ft897_get_ptt,
    .get_dcd =        ft897_get_dcd,
    .set_rptr_shift =     ft897_set_rptr_shift,
    .set_rptr_offs =  ft897_set_rptr_offs,
    .set_split_vfo =  ft897_set_split_vfo,
    .get_split_vfo =  ft897_get_split_vfo,
    .set_rit =        ft897_set_rit,
    .set_dcs_code =   ft897_set_dcs_code,
    .set_ctcss_tone =     ft897_set_ctcss_tone,
    .set_dcs_sql =    ft897_set_dcs_sql,
    .set_ctcss_sql =  ft897_set_ctcss_sql,
    .set_powerstat =    ft817_set_powerstat,
    .get_level =      ft897_get_level,
    .set_func =       ft897_set_func,
    .vfo_op =     ft897_vfo_op,
    .hamlib_check_rig_caps = HAMLIB_CHECK_RIG_CAPS
};

const struct rig_caps ft897d_caps =
{
    RIG_MODEL(RIG_MODEL_FT897D),
    .model_name =     "FT-897D",
    .mfg_name =       "Yaesu",
    .version =        "20220407.0",
    .copyright =      "LGPL",
    .status =         RIG_STATUS_STABLE,
    .rig_type =       RIG_TYPE_TRANSCEIVER,
    .ptt_type =       RIG_PTT_RIG,
    .dcd_type =       RIG_DCD_RIG,
    .port_type =      RIG_PORT_SERIAL,
    .serial_rate_min =    4800,
    .serial_rate_max =    38400,
    .serial_data_bits =   8,
    .serial_stop_bits =   2,
    .serial_parity =  RIG_PARITY_NONE,
    .serial_handshake =   RIG_HANDSHAKE_NONE,
    .write_delay =    FT897_WRITE_DELAY,
    .post_write_delay =   FT897_POST_WRITE_DELAY,
    .timeout =        FT897_TIMEOUT,
    .retry =      0,
    .has_get_func =       RIG_FUNC_NONE,
    .has_set_func =   RIG_FUNC_LOCK | RIG_FUNC_TONE | RIG_FUNC_TSQL,
    .has_get_level =  RIG_LEVEL_STRENGTH | RIG_LEVEL_RFPOWER | RIG_LEVEL_SWR | RIG_LEVEL_RAWSTR | RIG_LEVEL_ALC,
    .has_set_level =  RIG_LEVEL_BAND_SELECT,
    .has_get_parm =   RIG_PARM_NONE,
    .has_set_parm =   RIG_PARM_NONE,
    .level_gran =
    {
#include "level_gran_yaesu.h"
    },
    .parm_gran =      {},
    .ctcss_list =     common_ctcss_list,
    .dcs_list =       common_dcs_list,   /* only 104 supported */
    .preamp =         { RIG_DBLST_END, },
    .attenuator =     { RIG_DBLST_END, },
    .max_rit =        Hz(9990),
    .max_xit =        Hz(0),
    .max_ifshift =    Hz(0),
    .targetable_vfo =     0,
    .transceive =     RIG_TRN_OFF,
    .bank_qty =       0,
    .chan_desc_sz =   0,
    .chan_list =          { RIG_CHAN_END, },
    .vfo_ops =        RIG_OP_TOGGLE,

    .rx_range_list1 =  {
        {kHz(100), MHz(56), FT897_ALL_RX_MODES, -1, -1},
        {MHz(76), MHz(108), RIG_MODE_WFM,      -1, -1},
        {MHz(118), MHz(164), FT897_ALL_RX_MODES, -1, -1},
        {MHz(420), MHz(470), FT897_ALL_RX_MODES, -1, -1},
        RIG_FRNG_END,
    },
    .tx_range_list1 =  {
        FRQ_RNG_HF(1, FT897_OTHER_TX_MODES, W(10), W(100), FT897_VFO_ALL, FT897_ANTS),
        FRQ_RNG_6m(1, FT897_OTHER_TX_MODES, W(10), W(100), FT897_VFO_ALL, FT897_ANTS),

        /* AM class */
        FRQ_RNG_HF(1, FT897_AM_TX_MODES, W(2.5), W(25), FT897_VFO_ALL, FT897_ANTS),
        FRQ_RNG_6m(1, FT897_AM_TX_MODES, W(2.5), W(25), FT897_VFO_ALL, FT897_ANTS),
        FRQ_RNG_2m(1, FT897_OTHER_TX_MODES, W(5), W(50), FT897_VFO_ALL, FT897_ANTS),
        /* AM class */
        FRQ_RNG_2m(1, FT897_AM_TX_MODES, W(2.5), W(25), FT897_VFO_ALL, FT897_ANTS),
        FRQ_RNG_70cm(1, FT897_OTHER_TX_MODES, W(2), W(20), FT897_VFO_ALL, FT897_ANTS),
        /* AM class */
        FRQ_RNG_70cm(1, FT897_AM_TX_MODES, W(0.5), W(5), FT897_VFO_ALL, FT897_ANTS),
        RIG_FRNG_END,
    },


    .rx_range_list2 =  {
        {kHz(100), MHz(56), FT897_ALL_RX_MODES, -1, -1},
        {MHz(76), MHz(108), RIG_MODE_WFM,      -1, -1},
        {MHz(118), MHz(164), FT897_ALL_RX_MODES, -1, -1},
        {MHz(420), MHz(470), FT897_ALL_RX_MODES, -1, -1},
        RIG_FRNG_END,
    },
    .tx_range_list2 =  {
        FRQ_RNG_HF(2, FT897_OTHER_TX_MODES, W(10), W(100), FT897_VFO_ALL, FT897_ANTS),
        /* AM class */
        FRQ_RNG_HF(2, FT897_AM_TX_MODES, W(2.5), W(25), FT897_VFO_ALL, FT897_ANTS),
        FRQ_RNG_2m(2, FT897_OTHER_TX_MODES, W(5), W(50), FT897_VFO_ALL, FT897_ANTS),
        /* AM class */
        FRQ_RNG_2m(2, FT897_AM_TX_MODES, W(2.5), W(25), FT897_VFO_ALL, FT897_ANTS),
        FRQ_RNG_70cm(2, FT897_OTHER_TX_MODES, W(2), W(20), FT897_VFO_ALL, FT897_ANTS),
        /* AM class */
        FRQ_RNG_70cm(2, FT897_AM_TX_MODES, W(0.5), W(5), FT897_VFO_ALL, FT897_ANTS),
        RIG_FRNG_END,
    },

    .tuning_steps =  {
        {FT897_SSB_CW_RX_MODES, 10},
        {FT897_SSB_CW_RX_MODES, 100},
        {FT897_AM_FM_RX_MODES, 10},
        {FT897_AM_FM_RX_MODES, 100},
        RIG_TS_END,
    },

    /* filter selection is not supported by CAT functions
     * per testing by Rich Newsom, WA4SXZ
     */
    .filters =  {
        {RIG_MODE_ALL, RIG_FLT_ANY},
//        {RIG_MODE_SSB, kHz(2.2)},
//        {RIG_MODE_CW, kHz(2.2)},
//        {RIG_MODE_CWR, kHz(2.2)},
//        {RIG_MODE_RTTY, kHz(2.2)},
//        {RIG_MODE_AM, kHz(6)},
//        {RIG_MODE_FM, kHz(15)},
//        {RIG_MODE_PKTFM, kHz(15)},
//        {RIG_MODE_FM, kHz(9)},
//        {RIG_MODE_PKTFM, kHz(9)},
//        {RIG_MODE_WFM, kHz(230)}, /* ?? */
        RIG_FLT_END,
    },

    .rig_init =       ft897_init,
    .rig_cleanup =    ft897_cleanup,
    .rig_open =       ft897_open,
    .rig_close =      ft897_close,
    .get_vfo =        ft857_get_vfo,
    .set_vfo =        ft857_set_vfo,
    .set_freq =       ft897_set_freq,
    .get_freq =       ft897_get_freq,
    .set_mode =       ft897_set_mode,
    .get_mode =       ft897_get_mode,
    .set_ptt =        ft897_set_ptt,
    .get_ptt =        ft897_get_ptt,
    .get_dcd =        ft897_get_dcd,
    .set_rptr_shift =     ft897_set_rptr_shift,
    .set_rptr_offs =  ft897_set_rptr_offs,
    .set_split_vfo =  ft897_set_split_vfo,
    .get_split_vfo =  ft897_get_split_vfo,
    .set_rit =        ft897_set_rit,
    .set_dcs_code =   ft897_set_dcs_code,
    .set_ctcss_tone =     ft897_set_ctcss_tone,
    .set_dcs_sql =    ft897_set_dcs_sql,
    .set_ctcss_sql =  ft897_set_ctcss_sql,
    .set_powerstat =    ft817_set_powerstat,
    .get_level =      ft897_get_level,
    .set_func =       ft897_set_func,
    .vfo_op =     ft897_vfo_op,
    .hamlib_check_rig_caps = HAMLIB_CHECK_RIG_CAPS
};

/* ---------------------------------------------------------------------- */

int ft897_init(RIG *rig)
{
    rig_debug(RIG_DEBUG_VERBOSE, "%s: called\n", __func__);

    if ((rig->state.priv = calloc(1, sizeof(struct ft897_priv_data))) == NULL)
    {
        return -RIG_ENOMEM;
    }

    return RIG_OK;
}

int ft897_cleanup(RIG *rig)
{
    rig_debug(RIG_DEBUG_VERBOSE, "%s: called\n", __func__);

    if (rig->state.priv)
    {
        free(rig->state.priv);
    }

    rig->state.priv = NULL;

    return RIG_OK;
}

int ft897_open(RIG *rig)
{
    rig_debug(RIG_DEBUG_VERBOSE, "%s: called\n", __func__);

    return RIG_OK;
}

int ft897_close(RIG *rig)
{
    rig_debug(RIG_DEBUG_VERBOSE, "%s: called\n", __func__);

    return RIG_OK;
}

/* ---------------------------------------------------------------------- */

static inline long timediff(struct timeval *tv1, struct timeval *tv2)
{
    struct timeval tv;

    tv.tv_usec = tv1->tv_usec - tv2->tv_usec;
    tv.tv_sec  = tv1->tv_sec  - tv2->tv_sec;

    return ((tv.tv_sec * 1000L) + (tv.tv_usec / 1000L));
}

static int check_cache_timeout(struct timeval *tv)
{
    struct timeval curr;
    long t;

    if (tv->tv_sec == 0 && tv->tv_usec == 0)
    {
        rig_debug(RIG_DEBUG_VERBOSE, "%s: cache invalid\n", __func__);
        return 1;
    }

    gettimeofday(&curr, NULL);

    if ((t = timediff(&curr, tv)) < FT897_CACHE_TIMEOUT)
    {
        rig_debug(RIG_DEBUG_VERBOSE, "%s: using cache (%ld ms)\n", __func__, t);
        return 0;
    }
    else
    {
        rig_debug(RIG_DEBUG_VERBOSE, "%s: cache timed out (%ld ms)\n", __func__, t);
        return 1;
    }
}

static int ft897_read_eeprom(RIG *rig, unsigned short addr, unsigned char *out)
{
    unsigned char data[YAESU_CMD_LENGTH];
    int n;

    rig_debug(RIG_DEBUG_VERBOSE, "%s: called\n", __func__);
    memcpy(data, (char *)ncmd[FT897_NATIVE_CAT_EEPROM_READ].nseq,
           YAESU_CMD_LENGTH);

    data[0] = addr >> 8;
    data[1] = addr & 0xfe;

    write_block(&rig->state.rigport, data, YAESU_CMD_LENGTH);

    if ((n = read_block(&rig->state.rigport, data, 2)) < 0)
    {
        return n;
    }

    if (n != 2)
    {
        return -RIG_EIO;
    }

    *out = data[addr % 2];

    return RIG_OK;
}

static int ft897_get_status(RIG *rig, int status)
{
    struct ft897_priv_data *p = (struct ft897_priv_data *) rig->state.priv;
    struct timeval *tv;
    unsigned char *data;
    int len;
    int n;

    rig_debug(RIG_DEBUG_VERBOSE, "%s: called\n", __func__);

    switch (status)
    {
    case FT897_NATIVE_CAT_GET_FREQ_MODE_STATUS:
        data = p->fm_status;
        len  = YAESU_CMD_LENGTH;
        tv   = &p->fm_status_tv;
        break;

    case FT897_NATIVE_CAT_GET_RX_STATUS:
        data = &p->rx_status;
        len  = 1;
        tv   = &p->rx_status_tv;
        break;

    case FT897_NATIVE_CAT_GET_TX_STATUS:
        data = &p->tx_status;
        len  = 1;
        tv   = &p->tx_status_tv;
        break;

    case FT897_NATIVE_CAT_GET_TX_METER:
        data = p->tm_status;
        len = 2;
        tv = &p->tm_status_tv;
        break;


    default:
        rig_debug(RIG_DEBUG_ERR, "%s: internal error!\n", __func__);
        return -RIG_EINTERNAL;
    }

    rig_flush(&rig->state.rigport);

    write_block(&rig->state.rigport, ncmd[status].nseq,
                YAESU_CMD_LENGTH);

    if ((n = read_block(&rig->state.rigport, data, len)) < 0)
    {
        return n;
    }

    if (n != len)
    {
        return -RIG_EIO;
    }

    if (status == FT897_NATIVE_CAT_GET_FREQ_MODE_STATUS)
    {
        if ((n = ft897_read_eeprom(rig, 0x0078, &p->fm_status[5])) < 0)
        {
            return n;
        }

        p->fm_status[5] >>= 5;
    }

    gettimeofday(tv, NULL);

    return RIG_OK;
}

/* ---------------------------------------------------------------------- */

int ft897_get_freq(RIG *rig, vfo_t vfo, freq_t *freq)
{
    struct ft897_priv_data *p = (struct ft897_priv_data *) rig->state.priv;

    rig_debug(RIG_DEBUG_VERBOSE, "%s: called\n", __func__);

    if (check_cache_timeout(&p->fm_status_tv))
    {
        int n;

        if ((n = ft897_get_status(rig, FT897_NATIVE_CAT_GET_FREQ_MODE_STATUS)) < 0)
        {
            return n;
        }
    }

    *freq = from_bcd_be(p->fm_status, 8) * 10;

    return RIG_OK;
}

int ft897_get_mode(RIG *rig, vfo_t vfo, rmode_t *mode, pbwidth_t *width)
{
    struct ft897_priv_data *p = (struct ft897_priv_data *) rig->state.priv;

    rig_debug(RIG_DEBUG_VERBOSE, "%s: called\n", __func__);

    if (check_cache_timeout(&p->fm_status_tv))
    {
        int n;

        if ((n = ft897_get_status(rig, FT897_NATIVE_CAT_GET_FREQ_MODE_STATUS)) < 0)
        {
            return n;
        }
    }

    switch (p->fm_status[4] & 0x7f)
    {
    case 0x00:
        *mode = RIG_MODE_LSB;
        break;

    case 0x01:
        *mode = RIG_MODE_USB;
        break;

    case 0x02:
        *mode = RIG_MODE_CW;
        break;

    case 0x03:
        *mode = RIG_MODE_CWR;
        break;

    case 0x04:
        *mode = RIG_MODE_AM;
        break;

    case 0x06:
        *mode = RIG_MODE_WFM;
        break;

    case 0x08:
        *mode = RIG_MODE_FM;
        break;

    case 0x0a:
        switch (p->fm_status[5])
        {
        case FT897_DIGI_RTTY_L: *mode = RIG_MODE_RTTY; break;

        case FT897_DIGI_RTTY_U: *mode = RIG_MODE_RTTYR; break;

        case FT897_DIGI_PSK_L: *mode = RIG_MODE_PKTLSB; break;

        case FT897_DIGI_PSK_U: *mode = RIG_MODE_PKTUSB; break;

        case FT897_DIGI_USER_L: *mode = RIG_MODE_PKTLSB; break;

        case FT897_DIGI_USER_U: *mode = RIG_MODE_PKTUSB; break;
        }

        break;

    case 0x0c:
        *mode = RIG_MODE_PKTFM;
        break;

    default:
        *mode = RIG_MODE_NONE;
    }

    if (p->fm_status[4] & 0x80)     /* narrow */
    {
        *width = rig_passband_narrow(rig, *mode);
    }
    else
    {
        *width = RIG_PASSBAND_NORMAL;
    }

    return RIG_OK;
}

int ft897_get_ptt(RIG *rig, vfo_t vfo, ptt_t *ptt)
{
    struct ft897_priv_data *p = (struct ft897_priv_data *) rig->state.priv;

    rig_debug(RIG_DEBUG_VERBOSE, "%s: called\n", __func__);

    if (check_cache_timeout(&p->tx_status_tv))
    {
        int n;

        if ((n = ft897_get_status(rig, FT897_NATIVE_CAT_GET_TX_STATUS)) < 0)
        {
            return n;
        }
    }

    *ptt = ((p->tx_status & 0x80) == 0);

    return RIG_OK;
}

static int ft897_get_pometer_level(RIG *rig, value_t *val)
{
    struct ft897_priv_data *p = (struct ft897_priv_data *) rig->state.priv;

    rig_debug(RIG_DEBUG_VERBOSE, "%s: called\n", __func__);

    if (check_cache_timeout(&p->tx_status_tv))
    {
        int n;

        if ((n = ft897_get_status(rig, FT897_NATIVE_CAT_GET_TX_STATUS)) < 0)
        {
            return n;
        }
    }

    /* Valid only if PTT is on */
    if ((p->tx_status & 0x80) == 0)
    {
        val->f = ((p->tx_status & 0x0F) / 15.0);
    }
    else
    {
        val->f = 0.0;
    }

    return RIG_OK;
}

static int ft897_get_swr_level(RIG *rig, value_t *val)
{
    struct ft897_priv_data *p = (struct ft897_priv_data *) rig->state.priv;

    rig_debug(RIG_DEBUG_VERBOSE, "%s: called\n", __func__);

    if (check_cache_timeout(&p->tx_status_tv))
    {
        int n;

        if ((n = ft897_get_status(rig, FT897_NATIVE_CAT_GET_TX_STATUS)) < 0)
        {
            return n;
        }
    }

    /* Valid only if PTT is on */
    if ((p->tx_status & 0x80) == 0)
    {
        val->f = (p->tx_status & 0x40) ? 30.0 : 1.0;    /* or infinity? */
    }
    else
    {
        val->f = 0.0;
    }

    return RIG_OK;
}

static int ft897_get_smeter_level(RIG *rig, value_t *val)
{
    struct ft897_priv_data *p = (struct ft897_priv_data *) rig->state.priv;
    int n;

    rig_debug(RIG_DEBUG_VERBOSE, "%s: called\n", __func__);

    if (check_cache_timeout(&p->rx_status_tv))
        if ((n = ft897_get_status(rig, FT897_NATIVE_CAT_GET_RX_STATUS)) < 0)
        {
            return n;
        }

    n = (p->rx_status & 0x0F) - 9;

    val->i = n * ((n > 0) ? 10 : 6);

    return RIG_OK;
}

static int ft897_get_rawstr_level(RIG *rig, value_t *val)
{
    struct ft897_priv_data *p = (struct ft897_priv_data *) rig->state.priv;

    rig_debug(RIG_DEBUG_VERBOSE, "%s: called\n", __func__);

    if (check_cache_timeout(&p->rx_status_tv))
    {
        int n;

        if ((n = ft897_get_status(rig, FT897_NATIVE_CAT_GET_RX_STATUS)) < 0)
        {
            return n;
        }
    }

    val->i = p->rx_status & 0x0F;

    return RIG_OK;
}

static int ft897_get_alc_level(RIG *rig, value_t *val)
{
    struct ft897_priv_data *p = (struct ft897_priv_data *) rig->state.priv;

    rig_debug(RIG_DEBUG_VERBOSE, "%s: called\n", __func__);

    /* have to check PTT first - only 1 byte (0xff) returned in RX
       and two bytes returned in TX */
    if ((p->tx_status & 0x80) == 0)
    {
        if (check_cache_timeout(&p->tm_status_tv))
        {
            int n;

            if ((n = ft897_get_status(rig, FT897_NATIVE_CAT_GET_TX_METER)) < 0)
            {
                return n;
            }
        }

        /* returns 2 bytes when in TX mode:
           byte[0]: bits 7:4 --> power
           byte[0]: bits 3:0 --> ALC
           byte[1]: bits 7:4 --> SWR
           byte[1]: bits 3:0 --> MOD */

        val->f = p->tm_status[0] >> 4;
    }
    else
    {
        val->f = 0;
    }

    return RIG_OK;
}

int ft897_get_level(RIG *rig, vfo_t vfo, setting_t level, value_t *val)
{
    rig_debug(RIG_DEBUG_VERBOSE, "%s: called\n", __func__);

    switch (level)
    {
    case RIG_LEVEL_STRENGTH:
        return ft897_get_smeter_level(rig, val);

    case RIG_LEVEL_RAWSTR:
        return ft897_get_rawstr_level(rig, val);

    case RIG_LEVEL_RFPOWER:
        return ft897_get_pometer_level(rig, val);

    case RIG_LEVEL_SWR:
        return ft897_get_swr_level(rig, val);

    case RIG_LEVEL_ALC:
        return ft897_get_alc_level(rig, val);

    default:
        return -RIG_EINVAL;
    }

    return RIG_OK;
}

int ft897_get_dcd(RIG *rig, vfo_t vfo, dcd_t *dcd)
{
    struct ft897_priv_data *p = (struct ft897_priv_data *) rig->state.priv;

    rig_debug(RIG_DEBUG_VERBOSE, "%s: called\n", __func__);

    if (check_cache_timeout(&p->rx_status_tv))
    {
        int n;

        if ((n = ft897_get_status(rig, FT897_NATIVE_CAT_GET_RX_STATUS)) < 0)
        {
            return n;
        }
    }

    /* TODO: consider bit 6 too ??? (CTCSS/DCS code match) */
    if (p->rx_status & 0x80)
    {
        *dcd = RIG_DCD_OFF;
    }
    else
    {
        *dcd = RIG_DCD_ON;
    }

    return RIG_OK;
}

/*
 * private helper function to send a private command sequence.
 * Must only be complete sequences.
 */
static int ft897_send_cmd(RIG *rig, int index)
{
    rig_debug(RIG_DEBUG_VERBOSE, "%s: called\n", __func__);

    if (ncmd[index].ncomp == 0)
    {
        rig_debug(RIG_DEBUG_VERBOSE, "%s: incomplete sequence\n", __func__);
        return -RIG_EINTERNAL;
    }

    write_block(&rig->state.rigport, ncmd[index].nseq, YAESU_CMD_LENGTH);
    return ft817_read_ack(rig);
}

/*
 * The same for incomplete commands.
 */
static int ft897_send_icmd(RIG *rig, int index, unsigned char *data)
{
    unsigned char cmd[YAESU_CMD_LENGTH];

    rig_debug(RIG_DEBUG_VERBOSE, "%s: called\n", __func__);

    if (ncmd[index].ncomp == 1)
    {
        rig_debug(RIG_DEBUG_VERBOSE, "%s: Complete sequence\n", __func__);
        return -RIG_EINTERNAL;
    }

    cmd[YAESU_CMD_LENGTH - 1] = ncmd[index].nseq[YAESU_CMD_LENGTH - 1];
    memcpy(cmd, data, YAESU_CMD_LENGTH - 1);

    write_block(&rig->state.rigport, cmd, YAESU_CMD_LENGTH);
    return ft817_read_ack(rig);
}

/* ---------------------------------------------------------------------- */

int ft897_set_freq(RIG *rig, vfo_t vfo, freq_t freq)
{
    unsigned char data[YAESU_CMD_LENGTH - 1];

    rig_debug(RIG_DEBUG_VERBOSE, "%s: called\n", __func__);

    rig_debug(RIG_DEBUG_VERBOSE, "%s: requested freq = %"PRIfreq" Hz\n", __func__,
              freq);

    /* fill in the frequency */
    to_bcd_be(data, (freq + 5) / 10, 8);

    /*invalidate frequency cache*/
    rig_force_cache_timeout(&((struct ft897_priv_data *)
                              rig->state.priv)->fm_status_tv);

    return ft897_send_icmd(rig, FT897_NATIVE_CAT_SET_FREQ, data);
}

int ft897_set_mode(RIG *rig, vfo_t vfo, rmode_t mode, pbwidth_t width)
{
    int index;    /* index of sequence to send */

    rig_debug(RIG_DEBUG_VERBOSE, "%s: called\n", __func__);

    rig_debug(RIG_DEBUG_VERBOSE, "%s: generic mode = %s\n", __func__,
              rig_strrmode(mode));

    switch (mode)
    {
    case RIG_MODE_AM:
        index = FT897_NATIVE_CAT_SET_MODE_AM;
        break;

    case RIG_MODE_CW:
        index = FT897_NATIVE_CAT_SET_MODE_CW;
        break;

    case RIG_MODE_USB:
        index = FT897_NATIVE_CAT_SET_MODE_USB;
        break;

    case RIG_MODE_LSB:
        index = FT897_NATIVE_CAT_SET_MODE_LSB;
        break;

    case RIG_MODE_RTTY:
    case RIG_MODE_PKTUSB:
        /* user has to have correct DIG mode setup on rig */
        index = FT897_NATIVE_CAT_SET_MODE_DIG;
        break;

    case RIG_MODE_FM:
        index = FT897_NATIVE_CAT_SET_MODE_FM;
        break;

    case RIG_MODE_CWR:
        index = FT897_NATIVE_CAT_SET_MODE_CWR;
        break;

    case RIG_MODE_PKTFM:
        index = FT897_NATIVE_CAT_SET_MODE_PKT;
        break;

    default:
        return -RIG_EINVAL;
    }

    if (width != RIG_PASSBAND_NOCHANGE && width != RIG_PASSBAND_NORMAL)
    {
        return -RIG_EINVAL;
    }

    rig_force_cache_timeout(&((struct ft897_priv_data *)
                              rig->state.priv)->fm_status_tv);

    return ft897_send_cmd(rig, index);
}

int ft897_set_ptt(RIG *rig, vfo_t vfo, ptt_t ptt)
{
    int index, n;

    rig_debug(RIG_DEBUG_VERBOSE, "%s: called\n", __func__);

    switch (ptt)
    {
    case RIG_PTT_ON:
        index = FT897_NATIVE_CAT_PTT_ON;
        break;

    case RIG_PTT_OFF:
        index = FT897_NATIVE_CAT_PTT_OFF;
        break;

    default:
        return -RIG_EINVAL;
    }

    n = ft897_send_cmd(rig, index);

    rig_force_cache_timeout(&((struct ft897_priv_data *)
                              rig->state.priv)->tx_status_tv);

    if (n < 0 && n != -RIG_ERJCTED)
    {
        return n;
    }

    return RIG_OK;
}

int ft897_vfo_op(RIG *rig, vfo_t vfo, vfo_op_t op)
{
    int index, n;

    rig_debug(RIG_DEBUG_VERBOSE, "%s: called\n", __func__);

    switch (op)
    {
    case RIG_OP_TOGGLE:
        rig_force_cache_timeout(&((struct ft897_priv_data *)
                                  rig->state.priv)->tx_status_tv);
        index = FT897_NATIVE_CAT_SET_VFOAB;
        break;

    default:
        return -RIG_EINVAL;
    }

    n = ft897_send_cmd(rig, index);

    if (n < 0 && n != -RIG_ERJCTED)
    {
        return n;
    }

    return RIG_OK;
}

int ft897_set_split_vfo(RIG *rig, vfo_t vfo, split_t split, vfo_t tx_vfo)
{
    int index, n;

    rig_debug(RIG_DEBUG_VERBOSE, "%s: called\n", __func__);

    switch (split)
    {
    case RIG_SPLIT_OFF:
        index = FT897_NATIVE_CAT_SPLIT_OFF;
        break;

    case RIG_SPLIT_ON:
        index = FT897_NATIVE_CAT_SPLIT_ON;
        break;

    default:
        return -RIG_EINVAL;
    }

    n = ft897_send_cmd(rig, index);

    rig_force_cache_timeout(&((struct ft897_priv_data *)
                              rig->state.priv)->tx_status_tv);

    if (n < 0 && n != -RIG_ERJCTED)
    {
        return n;
    }

    return RIG_OK;
}

int ft897_get_split_vfo(RIG *rig, vfo_t vfo, split_t *split, vfo_t *tx_vfo)
{
    struct ft897_priv_data *p = (struct ft897_priv_data *) rig->state.priv;
    int n;

    rig_debug(RIG_DEBUG_VERBOSE, "%s: called\n", __func__);

    if (check_cache_timeout(&p->tx_status_tv))
        if ((n = ft897_get_status(rig, FT897_NATIVE_CAT_GET_TX_STATUS)) < 0)
        {
            return n;
        }

    if (p->tx_status & 0x80)
    {
        // TX status not valid when in RX
        unsigned char c;

        if ((n = ft897_read_eeprom(rig, 0x008d, &c)) < 0) /* get split status */
        {
            return n;
        }

        *split = (c & 0x80) ? RIG_SPLIT_ON : RIG_SPLIT_OFF;
    }
    else
    {
        *split = (p->tx_status & 0x20) ? RIG_SPLIT_ON : RIG_SPLIT_OFF;
    }

    return RIG_OK;
}

int ft897_set_func(RIG *rig, vfo_t vfo, setting_t func, int status)
{
    rig_debug(RIG_DEBUG_VERBOSE, "%s: called\n", __func__);

    switch (func)
    {
    case RIG_FUNC_LOCK:
        if (status)
        {
            return ft897_send_cmd(rig, FT897_NATIVE_CAT_LOCK_ON);
        }
        else
        {
            return ft897_send_cmd(rig, FT897_NATIVE_CAT_LOCK_OFF);
        }

    case RIG_FUNC_TONE:
        if (status)
        {
            return ft897_send_cmd(rig, FT897_NATIVE_CAT_SET_CTCSS_ENC_ON);
        }
        else
        {
            return ft897_send_cmd(rig, FT897_NATIVE_CAT_SET_CTCSS_DCS_OFF);
        }

    case RIG_FUNC_TSQL:
        if (status)
        {
            return ft897_send_cmd(rig, FT897_NATIVE_CAT_SET_CTCSS_ON);
        }
        else
        {
            return ft897_send_cmd(rig, FT897_NATIVE_CAT_SET_CTCSS_DCS_OFF);
        }

    case RIG_FUNC_CSQL:
        if (status)
        {
            return ft897_send_cmd(rig, FT897_NATIVE_CAT_SET_DCS_ON);
        }
        else
        {
            return ft897_send_cmd(rig, FT897_NATIVE_CAT_SET_CTCSS_DCS_OFF);
        }

    case RIG_FUNC_RIT:
        if (status)
        {
            return ft897_send_cmd(rig, FT897_NATIVE_CAT_CLAR_ON);
        }
        else
        {
            return ft897_send_cmd(rig, FT897_NATIVE_CAT_CLAR_OFF);
        }

#if 0

    case RIG_FUNC_CODE:   /* this doesn't exist */
        if (status)
        {
            return ft897_send_cmd(rig, FT897_NATIVE_CAT_SET_DCS_ENC_ON);
        }
        else
        {
            return ft897_send_cmd(rig, FT897_NATIVE_CAT_SET_CTCSS_DCS_OFF);
        }

#endif

    default:
        return -RIG_EINVAL;
    }
}

int ft897_set_dcs_code(RIG *rig, vfo_t vfo, tone_t code)
{
    unsigned char data[YAESU_CMD_LENGTH - 1];
    int n;

    rig_debug(RIG_DEBUG_VERBOSE, "%s: called\n", __func__);

    rig_debug(RIG_DEBUG_VERBOSE, "ft897: set DCS code (%u)\n", code);

    if (code == 0)
    {
        return ft897_send_cmd(rig, FT897_NATIVE_CAT_SET_CTCSS_DCS_OFF);
    }

    /* fill in the DCS code - the rig doesn't support separate codes... */
    to_bcd_be(data,     code, 4);
    to_bcd_be(data + 2, code, 4);

    if ((n = ft897_send_icmd(rig, FT897_NATIVE_CAT_SET_DCS_CODE, data)) < 0)
    {
        return n;
    }

    return ft897_send_cmd(rig, FT897_NATIVE_CAT_SET_DCS_ENC_ON);
}

int ft897_set_ctcss_tone(RIG *rig, vfo_t vfo, tone_t tone)
{
    unsigned char data[YAESU_CMD_LENGTH - 1];
    int n;

    rig_debug(RIG_DEBUG_VERBOSE, "%s: called\n", __func__);

    rig_debug(RIG_DEBUG_VERBOSE, "ft897: set CTCSS tone (%.1f)\n", tone / 10.0);

    if (tone == 0)
    {
        return ft897_send_cmd(rig, FT897_NATIVE_CAT_SET_CTCSS_DCS_OFF);
    }

    /* fill in the CTCSS freq - the rig doesn't support separate tones... */
    to_bcd_be(data,     tone, 4);
    to_bcd_be(data + 2, tone, 4);

    if ((n = ft897_send_icmd(rig, FT897_NATIVE_CAT_SET_CTCSS_FREQ, data)) < 0)
    {
        return n;
    }

    return ft897_send_cmd(rig, FT897_NATIVE_CAT_SET_CTCSS_ENC_ON);
}

int ft897_set_dcs_sql(RIG *rig, vfo_t vfo, tone_t code)
{
    unsigned char data[YAESU_CMD_LENGTH - 1];
    int n;

    rig_debug(RIG_DEBUG_VERBOSE, "%s: called\n", __func__);

    rig_debug(RIG_DEBUG_VERBOSE, "ft897: set DCS sql (%u)\n", code);

    if (code == 0)
    {
        return ft897_send_cmd(rig, FT897_NATIVE_CAT_SET_CTCSS_DCS_OFF);
    }

    /* fill in the DCS code - the rig doesn't support separate codes... */
    to_bcd_be(data,     code, 4);
    to_bcd_be(data + 2, code, 4);

    if ((n = ft897_send_icmd(rig, FT897_NATIVE_CAT_SET_DCS_CODE, data)) < 0)
    {
        return n;
    }

    return ft897_send_cmd(rig, FT897_NATIVE_CAT_SET_DCS_ON);
}

int ft897_set_ctcss_sql(RIG *rig, vfo_t vfo, tone_t tone)
{
    unsigned char data[YAESU_CMD_LENGTH - 1];
    int n;

    rig_debug(RIG_DEBUG_VERBOSE, "%s: called\n", __func__);

    rig_debug(RIG_DEBUG_VERBOSE, "ft897: set CTCSS sql (%.1f)\n", tone / 10.0);

    if (tone == 0)
    {
        return ft897_send_cmd(rig, FT897_NATIVE_CAT_SET_CTCSS_DCS_OFF);
    }

    /* fill in the CTCSS freq - the rig doesn't support separate tones... */
    to_bcd_be(data,     tone, 4);
    to_bcd_be(data + 2, tone, 4);

    if ((n = ft897_send_icmd(rig, FT897_NATIVE_CAT_SET_CTCSS_FREQ, data)) < 0)
    {
        return n;
    }

    return ft897_send_cmd(rig, FT897_NATIVE_CAT_SET_CTCSS_ON);
}

int ft897_set_rptr_shift(RIG *rig, vfo_t vfo, rptr_shift_t shift)
{
    rig_debug(RIG_DEBUG_VERBOSE, "%s: called\n", __func__);

    rig_debug(RIG_DEBUG_VERBOSE, "ft897: set repeter shift = %i\n", shift);

    switch (shift)
    {
    case RIG_RPT_SHIFT_NONE:
        return ft897_send_cmd(rig, FT897_NATIVE_CAT_SET_RPT_SHIFT_SIMPLEX);

    case RIG_RPT_SHIFT_MINUS:
        return ft897_send_cmd(rig, FT897_NATIVE_CAT_SET_RPT_SHIFT_MINUS);

    case RIG_RPT_SHIFT_PLUS:
        return ft897_send_cmd(rig, FT897_NATIVE_CAT_SET_RPT_SHIFT_PLUS);
    }

    return -RIG_EINVAL;
}

int ft897_set_rptr_offs(RIG *rig, vfo_t vfo, shortfreq_t offs)
{
    unsigned char data[YAESU_CMD_LENGTH - 1];

    rig_debug(RIG_DEBUG_VERBOSE, "%s: called\n", __func__);

    rig_debug(RIG_DEBUG_VERBOSE, "ft897: set repeter offs = %li\n", offs);

    /* fill in the offset freq */
    to_bcd_be(data, offs / 10, 8);

    return ft897_send_icmd(rig, FT897_NATIVE_CAT_SET_RPT_OFFSET, data);
}

int ft897_set_rit(RIG *rig, vfo_t vfo, shortfreq_t rit)
{
    unsigned char data[YAESU_CMD_LENGTH - 1];
    int n;

    rig_debug(RIG_DEBUG_VERBOSE, "%s: called\n", __func__);

    rig_debug(RIG_DEBUG_VERBOSE, "ft897: set rit = %li)\n", rit);

    /* fill in the RIT freq */
    data[0] = (rit < 0) ? 255 : 0;
    data[1] = 0;
    to_bcd_be(data + 2, labs(rit) / 10, 4);

    if ((n = ft897_send_icmd(rig, FT897_NATIVE_CAT_SET_CLAR_FREQ, data)) < 0)
    {
        return n;
    }

    /* the rig rejects if these are repeated - don't confuse user with retcode */

    /* not used anymore, RIG_FUNC_RIT implemented
    if (rit == 0)
    {
        ft897_send_cmd(rig, FT897_NATIVE_CAT_CLAR_OFF);
    }
    else
    {
        ft897_send_cmd(rig, FT897_NATIVE_CAT_CLAR_ON);
    }*/

    return RIG_OK;
}

/* ---------------------------------------------------------------------- */

