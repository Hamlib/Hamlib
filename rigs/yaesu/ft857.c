/* -*- mode: c; c-basic-offset: 2; indent-tabs-mode: nil; -*-
 *
 * hamlib - (C) Frank Singleton 2000,2001 (vk3fcs@ix.netcom.com)
 *          (C) Stephane Fillod 2000-2009
 *
 * ft857.h - (C) Tomi Manninen 2003 (oh2bns@sral.fi)
 *
 *  ...derived but heavily modified from:
 *
 * ft817.h - (C) Chris Karpinsky 2001 (aa1vl@arrl.net)
 *
 * This shared library provides an API for communicating
 * via serial interface to an FT-857 using the "CAT" interface.
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
 * Unimplemented features supported by the FT-857:
 *
 *   - DCS encoder/squelch ON/OFF, similar to RIG_FUNC_TONE/TSQL.
 *     Needs frontend support.
 *
 *   - RX status command returns info that is not used:
 *
 *      - discriminator centered (yes/no flag)
 *      - received ctcss/dcs matched (yes/no flag)
 *
 *   - TX status command returns info that is not used:
 *
 *      - high swr flag
 *
 * The manual also indicates that CTCSS and DCS codes can be set
 * separately for tx and rx, but this doesn't seem to work. It
 * doesn't work from front panel either.
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
#include "ft857.h"
#include "ft817.h" /* We use functions from the 817 code */
#include "misc.h"
#include "tones.h"
#include "bandplan.h"

enum ft857_native_cmd_e
{
    FT857_NATIVE_CAT_LOCK_ON = 0,
    FT857_NATIVE_CAT_LOCK_OFF,
    FT857_NATIVE_CAT_PTT_ON,
    FT857_NATIVE_CAT_PTT_OFF,
    FT857_NATIVE_CAT_SET_FREQ,
    FT857_NATIVE_CAT_SET_MODE_LSB,
    FT857_NATIVE_CAT_SET_MODE_USB,
    FT857_NATIVE_CAT_SET_MODE_CW,
    FT857_NATIVE_CAT_SET_MODE_CWR,
    FT857_NATIVE_CAT_SET_MODE_AM,
    FT857_NATIVE_CAT_SET_MODE_FM,
    FT857_NATIVE_CAT_SET_MODE_FM_N,
    FT857_NATIVE_CAT_SET_MODE_DIG,
    FT857_NATIVE_CAT_SET_MODE_PKT,
    FT857_NATIVE_CAT_CLAR_ON,
    FT857_NATIVE_CAT_CLAR_OFF,
    FT857_NATIVE_CAT_SET_CLAR_FREQ,
    FT857_NATIVE_CAT_SET_VFOAB,
    FT857_NATIVE_CAT_SPLIT_ON,
    FT857_NATIVE_CAT_SPLIT_OFF,
    FT857_NATIVE_CAT_SET_RPT_SHIFT_MINUS,
    FT857_NATIVE_CAT_SET_RPT_SHIFT_PLUS,
    FT857_NATIVE_CAT_SET_RPT_SHIFT_SIMPLEX,
    FT857_NATIVE_CAT_SET_RPT_OFFSET,
    FT857_NATIVE_CAT_SET_DCS_ON,
    FT857_NATIVE_CAT_SET_DCS_DEC_ON,
    FT857_NATIVE_CAT_SET_DCS_ENC_ON,
    FT857_NATIVE_CAT_SET_CTCSS_ON,
    FT857_NATIVE_CAT_SET_CTCSS_DEC_ON,
    FT857_NATIVE_CAT_SET_CTCSS_ENC_ON,
    FT857_NATIVE_CAT_SET_CTCSS_DCS_OFF,
    FT857_NATIVE_CAT_SET_CTCSS_FREQ,
    FT857_NATIVE_CAT_SET_DCS_CODE,
    FT857_NATIVE_CAT_GET_RX_STATUS,
    FT857_NATIVE_CAT_GET_TX_STATUS,
    FT857_NATIVE_CAT_GET_FREQ_MODE_STATUS,
    FT857_NATIVE_CAT_PWR_WAKE,
    FT857_NATIVE_CAT_PWR_ON,
    FT857_NATIVE_CAT_PWR_OFF,
    FT857_NATIVE_CAT_EEPROM_READ,
    FT857_NATIVE_SIZE     /* end marker */
};

static int ft857_init(RIG *rig);
static int ft857_open(RIG *rig);
static int ft857_cleanup(RIG *rig);
static int ft857_close(RIG *rig);
static int ft857_set_freq(RIG *rig, vfo_t vfo, freq_t freq);
static int ft857_get_freq(RIG *rig, vfo_t vfo, freq_t *freq);
static int ft857_set_mode(RIG *rig, vfo_t vfo, rmode_t mode, pbwidth_t width);
static int ft857_get_mode(RIG *rig, vfo_t vfo, rmode_t *mode, pbwidth_t *width);
static int ft857_set_split_freq_mode(RIG *rig, vfo_t vfo, freq_t freq,
                                     rmode_t mode, pbwidth_t width);
static int ft857_get_split_freq_mode(RIG *rig, vfo_t vfo, freq_t *freq,
                                     rmode_t *mode, pbwidth_t *width);
static int ft857_set_split_vfo(RIG *rig, vfo_t vfo, split_t split,
                               vfo_t tx_vfo);
static int ft857_get_split_vfo(RIG *rig, vfo_t vfo, split_t *split,
                               vfo_t *tx_vfo);
// static int ft857_set_vfo(RIG *rig, vfo_t vfo);
// static int ft857_get_vfo(RIG *rig, vfo_t *vfo);
static int ft857_set_ptt(RIG *rig, vfo_t vfo, ptt_t ptt);
static int ft857_get_ptt(RIG *rig, vfo_t vfo, ptt_t *ptt);
// static int ft857_set_level(RIG *rig, vfo_t vfo, setting_t level, value_t val);
static int ft857_get_level(RIG *rig, vfo_t vfo, setting_t level, value_t *val);
static int ft857_set_func(RIG *rig, vfo_t vfo, setting_t func, int status);
// static int ft857_get_func(RIG *rig, vfo_t vfo, setting_t func, int *status);
// static int ft857_set_parm(RIG *rig, setting_t parm, value_t val);
// static int ft857_get_parm(RIG *rig, setting_t parm, value_t *val);
static int ft857_set_dcs_code(RIG *rig, vfo_t vfo, tone_t code);
static int ft857_set_ctcss_tone(RIG *rig, vfo_t vfo, tone_t code);
static int ft857_set_dcs_sql(RIG *rig, vfo_t vfo, tone_t code);
static int ft857_set_ctcss_sql(RIG *rig, vfo_t vfo, tone_t tone);
static int ft857_set_rptr_shift(RIG *rig, vfo_t vfo, rptr_shift_t rptr_shift);
static int ft857_set_rptr_offs(RIG *rig, vfo_t vfo, shortfreq_t offs);
static int ft857_set_rit(RIG *rig, vfo_t vfo, shortfreq_t rit);
static int ft857_get_dcd(RIG *rig, vfo_t vfo, dcd_t *dcd);
static int ft857_vfo_op(RIG *rig, vfo_t vfo, vfo_op_t op);
// static int ft857_set_powerstat(RIG *rig, powerstat_t status);

struct ft857_priv_data
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
};



/* Native ft857 cmd set prototypes. These are READ ONLY as each */
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
};

enum ft857_digi
{
    FT857_DIGI_RTTY_L = 0,
    FT857_DIGI_RTTY_U,
    FT857_DIGI_PSK_L,
    FT857_DIGI_PSK_U,
    FT857_DIGI_USER_L,
    FT857_DIGI_USER_U,
};

#define FT857_ALL_RX_MODES      (RIG_MODE_AM|RIG_MODE_CW|RIG_MODE_CWR|RIG_MODE_USB|\
                                 RIG_MODE_LSB|RIG_MODE_RTTY|RIG_MODE_FM|RIG_MODE_PKTUSB)
#define FT857_SSB_CW_RX_MODES   (RIG_MODE_CW|RIG_MODE_CWR|RIG_MODE_USB|RIG_MODE_LSB)
#define FT857_AM_FM_RX_MODES    (RIG_MODE_AM|RIG_MODE_FM)

#define FT857_OTHER_TX_MODES    (RIG_MODE_AM|RIG_MODE_CW|RIG_MODE_USB|\
                                 RIG_MODE_LSB|RIG_MODE_RTTY|RIG_MODE_FM|RIG_MODE_PKTUSB)
#define FT857_AM_TX_MODES       (RIG_MODE_AM)

#define FT857_VFO_ALL           (RIG_VFO_A|RIG_VFO_B)
#define FT857_ANTS              0

static int ft857_send_icmd(RIG *rig, int index, unsigned char *data);

const struct rig_caps ft857_caps =
{
    RIG_MODEL(RIG_MODEL_FT857),
    .model_name =     "FT-857",
    .mfg_name =       "Yaesu",
    .version =        "20220712.0",
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
    .write_delay =    FT857_WRITE_DELAY,
    .post_write_delay =   FT857_POST_WRITE_DELAY,
    .timeout =        FT857_TIMEOUT,
    .retry =      0,
    .has_get_func =       RIG_FUNC_NONE,
    .has_set_func =   RIG_FUNC_LOCK | RIG_FUNC_TONE | RIG_FUNC_TSQL | RIG_FUNC_CSQL | RIG_FUNC_RIT,
    .has_get_level =  RIG_LEVEL_STRENGTH | RIG_LEVEL_RFPOWER,
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
    .vfo_ops =            RIG_OP_TOGGLE,

    .rx_range_list1 =  {
        {kHz(100), MHz(56), FT857_ALL_RX_MODES, -1, -1},
        {MHz(76), MHz(108), RIG_MODE_WFM,      -1, -1},
        {MHz(118), MHz(164), FT857_ALL_RX_MODES, -1, -1},
        {MHz(420), MHz(470), FT857_ALL_RX_MODES, -1, -1},
        RIG_FRNG_END,
    },
    .tx_range_list1 =  {
        FRQ_RNG_HF(1, FT857_OTHER_TX_MODES, W(10), W(100), FT857_VFO_ALL, FT857_ANTS),
        FRQ_RNG_6m(1, FT857_OTHER_TX_MODES, W(10), W(100), FT857_VFO_ALL, FT857_ANTS),
        /* AM class */
        FRQ_RNG_HF(1, FT857_AM_TX_MODES, W(2.5), W(25), FT857_VFO_ALL, FT857_ANTS),
        FRQ_RNG_6m(1, FT857_AM_TX_MODES, W(2.5), W(25), FT857_VFO_ALL, FT857_ANTS),
        FRQ_RNG_2m(1, FT857_OTHER_TX_MODES, W(5), W(50), FT857_VFO_ALL, FT857_ANTS),
        /* AM class */
        FRQ_RNG_2m(1, FT857_AM_TX_MODES, W(2.5), W(25), FT857_VFO_ALL, FT857_ANTS),
        FRQ_RNG_70cm(1, FT857_OTHER_TX_MODES, W(2), W(20), FT857_VFO_ALL, FT857_ANTS),
        /* AM class */
        FRQ_RNG_70cm(1, FT857_AM_TX_MODES, W(0.5), W(5), FT857_VFO_ALL, FT857_ANTS),
        RIG_FRNG_END,
    },


    .rx_range_list2 =  {
        {kHz(100), MHz(56), FT857_ALL_RX_MODES, -1, -1},
        {MHz(76), MHz(108), RIG_MODE_WFM,      -1, -1},
        {MHz(118), MHz(164), FT857_ALL_RX_MODES, -1, -1},
        {MHz(420), MHz(470), FT857_ALL_RX_MODES, -1, -1},
        RIG_FRNG_END,
    },
    .tx_range_list2 =  {
        FRQ_RNG_HF(2, FT857_OTHER_TX_MODES, W(10), W(100), FT857_VFO_ALL, FT857_ANTS),
        /* AM class */
        FRQ_RNG_HF(2, FT857_AM_TX_MODES, W(2.5), W(25), FT857_VFO_ALL, FT857_ANTS),
        FRQ_RNG_2m(2, FT857_OTHER_TX_MODES, W(5), W(50), FT857_VFO_ALL, FT857_ANTS),
        /* AM class */
        FRQ_RNG_2m(2, FT857_AM_TX_MODES, W(2.5), W(25), FT857_VFO_ALL, FT857_ANTS),
        FRQ_RNG_70cm(2, FT857_OTHER_TX_MODES, W(2), W(20), FT857_VFO_ALL, FT857_ANTS),
        /* AM class */
        FRQ_RNG_70cm(2, FT857_AM_TX_MODES, W(0.5), W(5), FT857_VFO_ALL, FT857_ANTS),
        RIG_FRNG_END,
    },

    .tuning_steps =  {
        {FT857_SSB_CW_RX_MODES, 10},
        {FT857_SSB_CW_RX_MODES, 100},
        {FT857_AM_FM_RX_MODES, 10},
        {FT857_AM_FM_RX_MODES, 100},
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

    .rig_init =       ft857_init,
    .rig_cleanup =    ft857_cleanup,
    .rig_open =       ft857_open,
    .rig_close =      ft857_close,
    .get_vfo =          ft857_get_vfo,
    .set_vfo =          ft857_set_vfo,
    .set_freq =       ft857_set_freq,
    .get_freq =       ft857_get_freq,
    .set_mode =       ft857_set_mode,
    .get_mode =       ft857_get_mode,
    .set_ptt =        ft857_set_ptt,
    .get_ptt =        ft857_get_ptt,
    .get_dcd =        ft857_get_dcd,
    .set_rptr_shift =     ft857_set_rptr_shift,
    .set_rptr_offs =  ft857_set_rptr_offs,
    .set_split_freq_mode =    ft857_set_split_freq_mode,
    .get_split_freq_mode =    ft857_get_split_freq_mode,
    .set_split_vfo =  ft857_set_split_vfo,
    .get_split_vfo =  ft857_get_split_vfo,
    .set_rit =        ft857_set_rit,
    .set_dcs_code =   ft857_set_dcs_code,
    .set_ctcss_tone =     ft857_set_ctcss_tone,
    .set_dcs_sql =    ft857_set_dcs_sql,
    .set_ctcss_sql =  ft857_set_ctcss_sql,
    .set_powerstat =    ft817_set_powerstat,

    .get_level =      ft857_get_level,
    .set_func =       ft857_set_func,
    .vfo_op =             ft857_vfo_op,
    .hamlib_check_rig_caps = HAMLIB_CHECK_RIG_CAPS
};

/* ---------------------------------------------------------------------- */

int ft857_init(RIG *rig)
{
    rig_debug(RIG_DEBUG_VERBOSE, "%s: called \n", __func__);

    if ((rig->state.priv = calloc(1, sizeof(struct ft857_priv_data))) == NULL)
    {
        return -RIG_ENOMEM;
    }

    return RIG_OK;
}

int ft857_cleanup(RIG *rig)
{
    rig_debug(RIG_DEBUG_VERBOSE, "%s: called\n", __func__);

    if (rig->state.priv)
    {
        free(rig->state.priv);
    }

    rig->state.priv = NULL;

    return RIG_OK;
}

int ft857_open(RIG *rig)
{
    rig_debug(RIG_DEBUG_VERBOSE, "%s: called\n", __func__);

    return RIG_OK;
}

int ft857_close(RIG *rig)
{
    rig_debug(RIG_DEBUG_VERBOSE, "%s:called\n", __func__);

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

    if ((t = timediff(&curr, tv)) < FT857_CACHE_TIMEOUT)
    {
        rig_debug(RIG_DEBUG_VERBOSE, "ft857: using cache (%ld ms)\n", t);
        return 0;
    }
    else
    {
        rig_debug(RIG_DEBUG_VERBOSE, "ft857: cache timed out (%ld ms)\n", t);
        return 1;
    }
}

static int ft857_read_eeprom(RIG *rig, unsigned short addr, unsigned char *out)
{
    unsigned char data[YAESU_CMD_LENGTH];
    int n;

    rig_debug(RIG_DEBUG_VERBOSE, "%s: called \n", __func__);
    memcpy(data, (char *)ncmd[FT857_NATIVE_CAT_EEPROM_READ].nseq,
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

static int ft857_get_status(RIG *rig, int status)
{
    struct ft857_priv_data *p = (struct ft857_priv_data *) rig->state.priv;
    struct timeval *tv;
    unsigned char *data;
    int len;
    int n;

    rig_debug(RIG_DEBUG_VERBOSE, "%s: called \n", __func__);

    switch (status)
    {
    case FT857_NATIVE_CAT_GET_FREQ_MODE_STATUS:
        data = p->fm_status;
        len  = YAESU_CMD_LENGTH;
        tv   = &p->fm_status_tv;
        break;

    case FT857_NATIVE_CAT_GET_RX_STATUS:
        data = &p->rx_status;
        len  = 1;
        tv   = &p->rx_status_tv;
        break;

    case FT857_NATIVE_CAT_GET_TX_STATUS:
        data = &p->tx_status;
        len  = 1;
        tv   = &p->tx_status_tv;
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

    if (status == FT857_NATIVE_CAT_GET_FREQ_MODE_STATUS)
    {
        if ((n = ft857_read_eeprom(rig, 0x0078, &p->fm_status[5])) < 0)
        {
            return n;
        }

        p->fm_status[5] >>= 5;
    }

    gettimeofday(tv, NULL);

    return RIG_OK;
}

/*
 * private helper function to send a private command sequence.
 * Must only be complete sequences.
 */
static int ft857_send_cmd(RIG *rig, int index)
{
    rig_debug(RIG_DEBUG_VERBOSE, "%s: called \n", __func__);

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
static int ft857_send_icmd(RIG *rig, int index, unsigned char *data)
{
    unsigned char cmd[YAESU_CMD_LENGTH];

    rig_debug(RIG_DEBUG_VERBOSE, "%s: called \n", __func__);

    if (ncmd[index].ncomp == 1)
    {
        rig_debug(RIG_DEBUG_VERBOSE, "%s: complete sequence\n", __func__);
        return -RIG_EINTERNAL;
    }

    cmd[YAESU_CMD_LENGTH - 1] = ncmd[index].nseq[YAESU_CMD_LENGTH - 1];
    memcpy(cmd, data, YAESU_CMD_LENGTH - 1);

    write_block(&rig->state.rigport, cmd, YAESU_CMD_LENGTH);
    return ft817_read_ack(rig);
}

/* ---------------------------------------------------------------------- */

int ft857_get_vfo(RIG *rig, vfo_t *vfo)
{
    unsigned char c;
    *vfo = RIG_VFO_B;

    rig_debug(RIG_DEBUG_VERBOSE, "%s: called \n", __func__);

    if (ft857_read_eeprom(rig, 0x0068, &c) < 0)   /* get vfo status */
    {
        return -RIG_EPROTO;
    }

    if ((c & 0x1) == 0) { *vfo = RIG_VFO_A; }

    return RIG_OK;
}

int ft857_set_vfo(RIG *rig, vfo_t vfo)
{
    vfo_t curvfo;
    int retval =  ft857_get_vfo(rig, &curvfo);

    rig_debug(RIG_DEBUG_VERBOSE, "%s: called \n", __func__);

    if (retval != RIG_OK)
    {
        rig_debug(RIG_DEBUG_ERR, "%s: error get_vfo '%s'\n", __func__,
                  rigerror(retval));
        return retval;
    }

    if (curvfo == vfo)
    {
        return RIG_OK;
    }

    return ft857_send_cmd(rig, FT857_NATIVE_CAT_SET_VFOAB);
}

int ft857_get_freq(RIG *rig, vfo_t vfo, freq_t *freq)
{
    struct ft857_priv_data *p = (struct ft857_priv_data *) rig->state.priv;

    rig_debug(RIG_DEBUG_VERBOSE, "%s: called \n", __func__);

    if (check_cache_timeout(&p->fm_status_tv))
    {
        int n;

        if ((n = ft857_get_status(rig, FT857_NATIVE_CAT_GET_FREQ_MODE_STATUS)) < 0)
        {
            return n;
        }
    }

    *freq = from_bcd_be(p->fm_status, 8) * 10;

    return -RIG_OK;
}

static void get_mode(RIG *rig, struct ft857_priv_data *priv, rmode_t *mode,
                     pbwidth_t *width)
{
    rig_debug(RIG_DEBUG_VERBOSE, "%s: called \n", __func__);

    switch (priv->fm_status[4] & 0x7f)
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
        switch (priv->fm_status[5])
        {
        case FT857_DIGI_RTTY_L: *mode = RIG_MODE_RTTY; break;

        case FT857_DIGI_RTTY_U: *mode = RIG_MODE_RTTYR; break;

        case FT857_DIGI_PSK_L: *mode = RIG_MODE_PKTLSB; break;

        case FT857_DIGI_PSK_U: *mode = RIG_MODE_PKTUSB; break;

        case FT857_DIGI_USER_L: *mode = RIG_MODE_PKTLSB; break;

        case FT857_DIGI_USER_U: *mode = RIG_MODE_PKTUSB; break;
        }

        break;

    case 0x0c:
        *mode = RIG_MODE_PKTFM;
        break;

    default:
        *mode = RIG_MODE_NONE;
    }

    if (priv->fm_status[4] & 0x80) /* narrow */
    {
        *width = rig_passband_narrow(rig, *mode);
    }
    else
    {
        *width = RIG_PASSBAND_NORMAL;
    }
}

int ft857_get_mode(RIG *rig, vfo_t vfo, rmode_t *mode, pbwidth_t *width)
{
    struct ft857_priv_data *p = (struct ft857_priv_data *) rig->state.priv;

    rig_debug(RIG_DEBUG_VERBOSE, "%s: called \n", __func__);

    if (check_cache_timeout(&p->fm_status_tv))
    {
        int n;

        if ((n = ft857_get_status(rig, FT857_NATIVE_CAT_GET_FREQ_MODE_STATUS)) < 0)
        {
            return n;
        }
    }

    get_mode(rig, p, mode, width);

    return RIG_OK;
}

int ft857_get_split_freq_mode(RIG *rig, vfo_t vfo, freq_t *freq, rmode_t *mode,
                              pbwidth_t *width)
{
    int retcode;

    rig_debug(RIG_DEBUG_VERBOSE, "%s: called \n", __func__);

    retcode = ft857_send_cmd(rig, FT857_NATIVE_CAT_SET_VFOAB);

    if (RIG_OK != retcode)
    {
        return retcode;
    }

    retcode = ft857_get_freq(rig, RIG_VFO_CURR, freq);

    if (RIG_OK == retcode)
    {
        get_mode(rig, (struct ft857_priv_data *)rig->state.priv, mode, width);
    }

    ft857_send_cmd(rig, FT857_NATIVE_CAT_SET_VFOAB); /* always try and
                                                      return to orig VFO */
    return retcode;
}

int ft857_get_split_vfo(RIG *rig, vfo_t vfo, split_t *split, vfo_t *tx_vfo)
{
    struct ft857_priv_data *p = (struct ft857_priv_data *) rig->state.priv;
    int n;

    rig_debug(RIG_DEBUG_VERBOSE, "%s: called \n", __func__);

    if (check_cache_timeout(&p->tx_status_tv))
        if ((n = ft857_get_status(rig, FT857_NATIVE_CAT_GET_TX_STATUS)) < 0)
        {
            return n;
        }

    if (p->tx_status & 0x80)
    {
        // TX status not valid when in RX
        unsigned char c;

        if ((n = ft857_read_eeprom(rig, 0x008d, &c)) < 0) /* get split status */
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

int ft857_get_ptt(RIG *rig, vfo_t vfo, ptt_t *ptt)
{
    struct ft857_priv_data *p = (struct ft857_priv_data *) rig->state.priv;

    rig_debug(RIG_DEBUG_VERBOSE, "%s: called \n", __func__);

    if (check_cache_timeout(&p->tx_status_tv))
    {
        int n;

        if ((n = ft857_get_status(rig, FT857_NATIVE_CAT_GET_TX_STATUS)) < 0)
        {
            return n;
        }
    }

    *ptt = ((p->tx_status & 0x80) == 0);

    return RIG_OK;
}

static int ft857_get_pometer_level(RIG *rig, value_t *val)
{
    struct ft857_priv_data *p = (struct ft857_priv_data *) rig->state.priv;
    rig_debug(RIG_DEBUG_VERBOSE, "%s: called \n", __func__);

    if (check_cache_timeout(&p->tx_status_tv))
    {
        int n;

        if ((n = ft857_get_status(rig, FT857_NATIVE_CAT_GET_TX_STATUS)) < 0)
        {
            return n;
        }
    }

    /* Valid only if PTT is on */
    if ((p->tx_status & 0x80) == 0)
    {
        rig_debug(RIG_DEBUG_TRACE, "%s: bars=%d\n", __func__, p->tx_status & 0x0F);
        // does rig have 10 bars or 15?
        val->f = (p->tx_status & 0x0F) / 10.0;
    }
    else
    {
        val->f = 0;    // invalid value return
    }

    return RIG_OK;
}

static int ft857_get_smeter_level(RIG *rig, value_t *val)
{
    struct ft857_priv_data *p = (struct ft857_priv_data *) rig->state.priv;
    int n;

    rig_debug(RIG_DEBUG_VERBOSE, "%s: called \n", __func__);

    if (check_cache_timeout(&p->rx_status_tv))
        if ((n = ft857_get_status(rig, FT857_NATIVE_CAT_GET_RX_STATUS)) < 0)
        {
            return n;
        }

    n = (p->rx_status & 0x0F);  // S level returned

    if (n >= 9) { val->i = (n - 9) * 10; }
    else { val->i = n * 6 - 54; }

    return RIG_OK;
}

int ft857_get_level(RIG *rig, vfo_t vfo, setting_t level, value_t *val)
{
    rig_debug(RIG_DEBUG_VERBOSE, "%s: called \n", __func__);

    switch (level)
    {
    case RIG_LEVEL_STRENGTH:
        return ft857_get_smeter_level(rig, val);

    case RIG_LEVEL_RFPOWER:
        return ft857_get_pometer_level(rig, val);

    default:
        return -RIG_EINVAL;
    }

    return RIG_OK;
}

int ft857_get_dcd(RIG *rig, vfo_t vfo, dcd_t *dcd)
{
    struct ft857_priv_data *p = (struct ft857_priv_data *) rig->state.priv;

    rig_debug(RIG_DEBUG_VERBOSE, "%s: called \n", __func__);

    if (check_cache_timeout(&p->rx_status_tv))
    {
        int n;

        if ((n = ft857_get_status(rig, FT857_NATIVE_CAT_GET_RX_STATUS)) < 0)
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

int ft857_set_freq(RIG *rig, vfo_t vfo, freq_t freq)
{
    unsigned char data[YAESU_CMD_LENGTH - 1];
    int retval;
    int i;
    ptt_t ptt = RIG_PTT_ON;

    rig_debug(RIG_DEBUG_VERBOSE, "%s: called \n", __func__);

    rig_debug(RIG_DEBUG_VERBOSE, "ft857: requested freq = %"PRIfreq" Hz\n", freq);

    // cannot set freq while PTT is on
    for (i = 0; i < 10 && ptt == RIG_PTT_ON; ++i)
    {
        retval = ft857_get_ptt(rig, vfo, &ptt);

        if (retval != RIG_OK) { return retval; }

        hl_usleep(100 * 1000);
    }

    /* fill in the frequency */
    to_bcd_be(data, (freq + 5) / 10, 8);

    rig_force_cache_timeout(&((struct ft857_priv_data *)
                              rig->state.priv)->fm_status_tv);

    return ft857_send_icmd(rig, FT857_NATIVE_CAT_SET_FREQ, data);
}

int ft857_set_mode(RIG *rig, vfo_t vfo, rmode_t mode, pbwidth_t width)
{
    int index;    /* index of sequence to send */

    rig_debug(RIG_DEBUG_VERBOSE, "%s: called \n", __func__);

    rig_debug(RIG_DEBUG_VERBOSE, "%s: generic mode = %s\n", __func__,
              rig_strrmode(mode));

    switch (mode)
    {
    case RIG_MODE_AM:
        index = FT857_NATIVE_CAT_SET_MODE_AM;
        break;

    case RIG_MODE_CW:
        index = FT857_NATIVE_CAT_SET_MODE_CW;
        break;

    case RIG_MODE_USB:
        index = FT857_NATIVE_CAT_SET_MODE_USB;
        break;

    case RIG_MODE_LSB:
        index = FT857_NATIVE_CAT_SET_MODE_LSB;
        break;

    case RIG_MODE_RTTY:
    case RIG_MODE_PKTUSB:
        /* user has to have correct DIG mode setup on rig */
        index = FT857_NATIVE_CAT_SET_MODE_DIG;
        break;

    case RIG_MODE_FM:
        index = FT857_NATIVE_CAT_SET_MODE_FM;
        break;

    case RIG_MODE_WFM:
        index = FT857_NATIVE_CAT_SET_MODE_FM;
        break;

    case RIG_MODE_CWR:
        index = FT857_NATIVE_CAT_SET_MODE_CWR;
        break;

    case RIG_MODE_PKTFM:
        index = FT857_NATIVE_CAT_SET_MODE_PKT;
        break;

    default:
        return -RIG_EINVAL;
    }

    if (width != RIG_PASSBAND_NOCHANGE && width != RIG_PASSBAND_NORMAL)
    {
        return -RIG_EINVAL;
    }

    rig_force_cache_timeout(&((struct ft857_priv_data *)
                              rig->state.priv)->fm_status_tv);

    return ft857_send_cmd(rig, index);
}

int ft857_set_split_freq_mode(RIG *rig, vfo_t vfo, freq_t freq, rmode_t mode,
                              pbwidth_t width)
{
    int retcode;

    rig_debug(RIG_DEBUG_VERBOSE, "%s: called \n", __func__);

    retcode = rig_set_split_vfo(rig, RIG_VFO_A, RIG_SPLIT_ON, RIG_VFO_B);

    if (retcode != RIG_OK) { RETURNFUNC(retcode); }


    retcode = ft857_send_cmd(rig, FT857_NATIVE_CAT_SET_VFOAB);

    if (RIG_OK != retcode)
    {
        return retcode;
    }

    retcode = ft857_set_freq(rig, RIG_VFO_CURR, freq);

    if (RIG_OK == retcode)
    {
        retcode = ft857_set_mode(rig, RIG_VFO_CURR, mode, width);
    }

    ft857_send_cmd(rig, FT857_NATIVE_CAT_SET_VFOAB); /* always try and
                                                      return to orig VFO */
    return retcode;
}

int ft857_set_split_vfo(RIG *rig, vfo_t vfo, split_t split, vfo_t tx_vfo)
{
    int index, n;

    rig_debug(RIG_DEBUG_VERBOSE, "%s: called \n", __func__);

    switch (split)
    {
    case RIG_SPLIT_ON:
        index = FT857_NATIVE_CAT_SPLIT_ON;
        break;

    case RIG_SPLIT_OFF:
        index = FT857_NATIVE_CAT_SPLIT_OFF;
        break;

    default:
        return -RIG_EINVAL;
    }

    n = ft857_send_cmd(rig, index);

    rig_force_cache_timeout(&((struct ft857_priv_data *)
                              rig->state.priv)->tx_status_tv);

    if (n < 0 && n != -RIG_ERJCTED)
    {
        return n;
    }

    return RIG_OK;
}

int ft857_set_ptt(RIG *rig, vfo_t vfo, ptt_t ptt)
{
    int index, n;

    rig_debug(RIG_DEBUG_VERBOSE, "%s: called \n", __func__);

    switch (ptt)
    {
    case RIG_PTT_ON:
        index = FT857_NATIVE_CAT_PTT_ON;
        break;

    case RIG_PTT_OFF:
        index = FT857_NATIVE_CAT_PTT_OFF;
        break;

    default:
        return -RIG_EINVAL;
    }

    n = ft857_send_cmd(rig, index);

    if (ptt == RIG_PTT_OFF) { hl_usleep(200 * 1000); } // FT857 takes a bit to come out of PTT

    rig_force_cache_timeout(&((struct ft857_priv_data *)
                              rig->state.priv)->tx_status_tv);

    if (n < 0 && n != -RIG_ERJCTED)
    {
        return n;
    }

    return RIG_OK;
}

int ft857_set_func(RIG *rig, vfo_t vfo, setting_t func, int status)
{
    rig_debug(RIG_DEBUG_VERBOSE, "%s: called \n", __func__);

    switch (func)
    {
    case RIG_FUNC_LOCK:
        if (status)
        {
            return ft857_send_cmd(rig, FT857_NATIVE_CAT_LOCK_ON);
        }
        else
        {
            return ft857_send_cmd(rig, FT857_NATIVE_CAT_LOCK_OFF);
        }

    case RIG_FUNC_TONE:
        if (status)
        {
            return ft857_send_cmd(rig, FT857_NATIVE_CAT_SET_CTCSS_ENC_ON);
        }
        else
        {
            return ft857_send_cmd(rig, FT857_NATIVE_CAT_SET_CTCSS_DCS_OFF);
        }

    case RIG_FUNC_TSQL:
        if (status)
        {
            return ft857_send_cmd(rig, FT857_NATIVE_CAT_SET_CTCSS_ON);
        }
        else
        {
            return ft857_send_cmd(rig, FT857_NATIVE_CAT_SET_CTCSS_DCS_OFF);
        }

    case RIG_FUNC_CSQL:
        if (status)
        {
            return ft857_send_cmd(rig, FT857_NATIVE_CAT_SET_DCS_ON);
        }
        else
        {
            return ft857_send_cmd(rig, FT857_NATIVE_CAT_SET_CTCSS_DCS_OFF);
        }

    case RIG_FUNC_RIT:
        if (status)
        {
            return ft857_send_cmd(rig, FT857_NATIVE_CAT_CLAR_ON);
        }
        else
        {
            return ft857_send_cmd(rig, FT857_NATIVE_CAT_CLAR_OFF);
        }

#if 0

    case RIG_FUNC_CODE:   /* this doesn't exist */
        if (status)
        {
            return ft857_send_cmd(rig, FT857_NATIVE_CAT_SET_DCS_ENC_ON);
        }
        else
        {
            return ft857_send_cmd(rig, FT857_NATIVE_CAT_SET_CTCSS_DCS_OFF);
        }

#endif

    default:
        return -RIG_EINVAL;
    }
}

int ft857_set_dcs_code(RIG *rig, vfo_t vfo, tone_t code)
{
    unsigned char data[YAESU_CMD_LENGTH - 1];
    int n;

    rig_debug(RIG_DEBUG_VERBOSE, "%s: called \n", __func__);

    rig_debug(RIG_DEBUG_VERBOSE, "ft857: set DCS code (%u)\n", code);

    if (code == 0)
    {
        return ft857_send_cmd(rig, FT857_NATIVE_CAT_SET_CTCSS_DCS_OFF);
    }

    /* fill in the DCS code - the rig doesn't support separate codes... */
    to_bcd_be(data,     code, 4);
    to_bcd_be(data + 2, code, 4);

    if ((n = ft857_send_icmd(rig, FT857_NATIVE_CAT_SET_DCS_CODE, data)) < 0)
    {
        return n;
    }

    return ft857_send_cmd(rig, FT857_NATIVE_CAT_SET_DCS_ENC_ON);
}

int ft857_set_ctcss_tone(RIG *rig, vfo_t vfo, tone_t tone)
{
    unsigned char data[YAESU_CMD_LENGTH - 1];
    int n;

    rig_debug(RIG_DEBUG_VERBOSE, "%s: called \n", __func__);

    rig_debug(RIG_DEBUG_VERBOSE, "ft857: set CTCSS tone (%.1f)\n", tone / 10.0);

    if (tone == 0)
    {
        return ft857_send_cmd(rig, FT857_NATIVE_CAT_SET_CTCSS_DCS_OFF);
    }

    /* fill in the CTCSS freq - the rig doesn't support separate tones... */
    to_bcd_be(data,     tone, 4);
    to_bcd_be(data + 2, tone, 4);

    if ((n = ft857_send_icmd(rig, FT857_NATIVE_CAT_SET_CTCSS_FREQ, data)) < 0)
    {
        return n;
    }

    return ft857_send_cmd(rig, FT857_NATIVE_CAT_SET_CTCSS_ENC_ON);
}

int ft857_set_dcs_sql(RIG *rig, vfo_t vfo, tone_t code)
{
    unsigned char data[YAESU_CMD_LENGTH - 1];
    int n;

    rig_debug(RIG_DEBUG_VERBOSE, "%s: called \n", __func__);

    rig_debug(RIG_DEBUG_VERBOSE, "ft857: set DCS sql (%u)\n", code);

    if (code == 0)
    {
        return ft857_send_cmd(rig, FT857_NATIVE_CAT_SET_CTCSS_DCS_OFF);
    }

    /* fill in the DCS code - the rig doesn't support separate codes... */
    to_bcd_be(data,     code, 4);
    to_bcd_be(data + 2, code, 4);

    if ((n = ft857_send_icmd(rig, FT857_NATIVE_CAT_SET_DCS_CODE, data)) < 0)
    {
        return n;
    }

    return ft857_send_cmd(rig, FT857_NATIVE_CAT_SET_DCS_ON);
}

int ft857_set_ctcss_sql(RIG *rig, vfo_t vfo, tone_t tone)
{
    unsigned char data[YAESU_CMD_LENGTH - 1];
    int n;

    rig_debug(RIG_DEBUG_VERBOSE, "%s: called \n", __func__);

    rig_debug(RIG_DEBUG_VERBOSE, "ft857: set CTCSS sql (%.1f)\n", tone / 10.0);

    if (tone == 0)
    {
        return ft857_send_cmd(rig, FT857_NATIVE_CAT_SET_CTCSS_DCS_OFF);
    }

    /* fill in the CTCSS freq - the rig doesn't support separate tones... */
    to_bcd_be(data,     tone, 4);
    to_bcd_be(data + 2, tone, 4);

    if ((n = ft857_send_icmd(rig, FT857_NATIVE_CAT_SET_CTCSS_FREQ, data)) < 0)
    {
        return n;
    }

    return ft857_send_cmd(rig, FT857_NATIVE_CAT_SET_CTCSS_ON);
}

int ft857_set_rptr_shift(RIG *rig, vfo_t vfo, rptr_shift_t shift)
{
    rig_debug(RIG_DEBUG_VERBOSE, "%s: called \n", __func__);

    rig_debug(RIG_DEBUG_VERBOSE, "ft857: set repeter shift = %i\n", shift);

    switch (shift)
    {
    case RIG_RPT_SHIFT_NONE:
        return ft857_send_cmd(rig, FT857_NATIVE_CAT_SET_RPT_SHIFT_SIMPLEX);

    case RIG_RPT_SHIFT_MINUS:
        return ft857_send_cmd(rig, FT857_NATIVE_CAT_SET_RPT_SHIFT_MINUS);

    case RIG_RPT_SHIFT_PLUS:
        return ft857_send_cmd(rig, FT857_NATIVE_CAT_SET_RPT_SHIFT_PLUS);
    }

    return -RIG_EINVAL;
}

int ft857_set_rptr_offs(RIG *rig, vfo_t vfo, shortfreq_t offs)
{
    unsigned char data[YAESU_CMD_LENGTH - 1];

    rig_debug(RIG_DEBUG_VERBOSE, "%s: called \n", __func__);

    rig_debug(RIG_DEBUG_VERBOSE, "ft857: set repeter offs = %li\n", offs);

    /* fill in the offset freq */
    to_bcd_be(data, offs / 10, 8);

    return ft857_send_icmd(rig, FT857_NATIVE_CAT_SET_RPT_OFFSET, data);
}

int ft857_set_rit(RIG *rig, vfo_t vfo, shortfreq_t rit)
{
    unsigned char data[YAESU_CMD_LENGTH - 1];
    int n;

    rig_debug(RIG_DEBUG_VERBOSE, "%s: called \n", __func__);

    rig_debug(RIG_DEBUG_VERBOSE, "ft857: set rit = %li)\n", rit);

    /* fill in the RIT freq */
    data[0] = (rit < 0) ? 255 : 0;
    data[1] = 0;
    to_bcd_be(data + 2, labs(rit) / 10, 4);

    if ((n = ft857_send_icmd(rig, FT857_NATIVE_CAT_SET_CLAR_FREQ, data)) < 0)
    {
        return n;
    }

    /* the rig rejects if these are repeated - don't confuse user with retcode */

    /* not used anymore, RIG_FUNC_RIT implemented
    if (rit == 0)
    {
        ft857_send_cmd(rig, FT857_NATIVE_CAT_CLAR_OFF);
    }
    else
    {
        ft857_send_cmd(rig, FT857_NATIVE_CAT_CLAR_ON);
    }*/

    return RIG_OK;
}

int ft857_vfo_op(RIG *rig, vfo_t vfo, vfo_op_t op)
{
    rig_debug(RIG_DEBUG_VERBOSE, "%s: called \n", __func__);

    switch (op)
    {
    case RIG_OP_TOGGLE:
        return ft857_send_cmd(rig, FT857_NATIVE_CAT_SET_VFOAB);

    default:
        return -RIG_EINVAL;
    }

    return -RIG_EINVAL;
}

/* ---------------------------------------------------------------------- */

