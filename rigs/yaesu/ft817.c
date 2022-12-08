/*
 * hamlib - (C) Frank Singleton 2000,2001 (vk3fcs@ix.netcom.com)
 *          (C) Stephane Fillod 2000-2009
 *
 * ft817.c - (C) Chris Karpinsky 2001 (aa1vl@arrl.net)
 * This shared library provides an API for communicating
 * via serial interface to an FT-817 using the "CAT" interface.
 * The starting point for this code was Frank's ft847 implementation.
 *
 * Then, Tommi OH2BNS improved the code a lot in the framework of the
 * FT-857 backend. These improvements have now (August 2005) been
 * copied back and adopted for the FT-817.
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
 * Unimplemented features supported by the FT-817:
 *
 *   - RX status command returns info that is not used:
 *      - discriminator centered (yes/no flag)
 *      - received ctcss/dcs matched (yes/no flag)                     TBC
 *
 *   - TX status command returns info that is not used:
 *      - high swr flag
 *
 * Todo / tocheck list (oz9aec):
 * - test get_dcd; rigctl does not support it?
 * - squelch
 * - the many "fixme" stuff around
 */

#include <hamlib/config.h>

#include <stdlib.h>
#include <string.h>     /* String function definitions */
#include <stdbool.h>

#ifdef HAVE_SYS_TIME_H
#include <sys/time.h>
#endif

#include "hamlib/rig.h"
#include "serial.h"
#include "yaesu.h"
#include "ft817.h"
#include "misc.h"
#include "tones.h"
#include "bandplan.h"
#include "cal.h"

enum ft817_native_cmd_e
{
    FT817_NATIVE_CAT_LOCK_ON = 0,
    FT817_NATIVE_CAT_LOCK_OFF,
    FT817_NATIVE_CAT_PTT_ON,
    FT817_NATIVE_CAT_PTT_OFF,
    FT817_NATIVE_CAT_SET_FREQ,
    FT817_NATIVE_CAT_SET_MODE_LSB,
    FT817_NATIVE_CAT_SET_MODE_USB,
    FT817_NATIVE_CAT_SET_MODE_CW,
    FT817_NATIVE_CAT_SET_MODE_CWR,
    FT817_NATIVE_CAT_SET_MODE_AM,
    FT817_NATIVE_CAT_SET_MODE_FM,
    FT817_NATIVE_CAT_SET_MODE_FM_N,
    FT817_NATIVE_CAT_SET_MODE_DIG,
    FT817_NATIVE_CAT_SET_MODE_PKT,
    FT817_NATIVE_CAT_CLAR_ON,
    FT817_NATIVE_CAT_CLAR_OFF,
    FT817_NATIVE_CAT_SET_CLAR_FREQ,
    FT817_NATIVE_CAT_SET_VFOAB,
    FT817_NATIVE_CAT_SPLIT_ON,
    FT817_NATIVE_CAT_SPLIT_OFF,
    FT817_NATIVE_CAT_SET_RPT_SHIFT_MINUS,
    FT817_NATIVE_CAT_SET_RPT_SHIFT_PLUS,
    FT817_NATIVE_CAT_SET_RPT_SHIFT_SIMPLEX,
    FT817_NATIVE_CAT_SET_RPT_OFFSET,
    FT817_NATIVE_CAT_SET_DCS_ON,
    FT817_NATIVE_CAT_SET_CTCSS_ON,
    FT817_NATIVE_CAT_SET_CTCSS_ENC_ON,
    FT817_NATIVE_CAT_SET_CTCSS_DCS_OFF,
    FT817_NATIVE_CAT_SET_CTCSS_FREQ,
    FT817_NATIVE_CAT_SET_DCS_CODE,
    FT817_NATIVE_CAT_GET_RX_STATUS,
    FT817_NATIVE_CAT_GET_TX_STATUS,
    FT817_NATIVE_CAT_GET_FREQ_MODE_STATUS,
    FT817_NATIVE_CAT_PWR_WAKE,
    FT817_NATIVE_CAT_PWR_ON,
    FT817_NATIVE_CAT_PWR_OFF,
    FT817_NATIVE_CAT_EEPROM_READ,
    FT817_NATIVE_CAT_GET_TX_METERING,
    FT817_NATIVE_SIZE       /* end marker */
};

struct ft817_priv_data
{
    /* rx status */
    struct timeval rx_status_tv;
    unsigned char rx_status;

    /* tx status */
    struct timeval tx_status_tv;
    unsigned char tx_status; /* Raw data from rig. Highest bit 0 = PTT */

    /* tx levels */
    struct timeval tx_level_tv;
    unsigned char swr_level;
    unsigned char alc_level;
    unsigned char mod_level;
    unsigned char pwr_level; /* TX power level */

    /* freq & mode status */
    struct timeval fm_status_tv;
    unsigned char fm_status[5]; /* 5 bytes, NOT related to YAESU_CMD_LENGTH */
    /* Digi mode is not part of regular fm_status response.
     * So keep track of it in a separate variable. */
    unsigned char dig_mode;
};

static int ft817_init(RIG *rig);
static int ft817_open(RIG *rig);
static int ft817_cleanup(RIG *rig);
static int ft817_close(RIG *rig);
static int ft817_get_vfo(RIG *rig, vfo_t *vfo);
static int ft817_set_vfo(RIG *rig, vfo_t vfo);
static int ft817_set_freq(RIG *rig, vfo_t vfo, freq_t freq);
static int ft817_get_freq(RIG *rig, vfo_t vfo, freq_t *freq);
static int ft817_set_mode(RIG *rig, vfo_t vfo, rmode_t mode, pbwidth_t width);
static int ft817_get_mode(RIG *rig, vfo_t vfo, rmode_t *mode, pbwidth_t *width);
static int ft817_set_ptt(RIG *rig, vfo_t vfo, ptt_t ptt);
static int ft817_get_ptt(RIG *rig, vfo_t vfo, ptt_t *ptt);
static int ft817_get_level(RIG *rig, vfo_t vfo, setting_t level, value_t *val);
static int ft817_set_func(RIG *rig, vfo_t vfo, setting_t func, int status);
static int ft817_set_dcs_code(RIG *rig, vfo_t vfo, tone_t code);
static int ft817_set_ctcss_tone(RIG *rig, vfo_t vfo, tone_t tone);
static int ft817_set_dcs_sql(RIG *rig, vfo_t vfo, tone_t code);
static int ft817_set_ctcss_sql(RIG *rig, vfo_t vfo, tone_t tone);
static int ft817_set_rptr_shift(RIG *rig, vfo_t vfo, rptr_shift_t shift);
static int ft817_set_rptr_offs(RIG *rig, vfo_t vfo, shortfreq_t offs);
static int ft817_set_rit(RIG *rig, vfo_t vfo, shortfreq_t rit);
static int ft817_get_dcd(RIG *rig, vfo_t vfo, dcd_t *dcd);
static int ft817_vfo_op(RIG *rig, vfo_t vfo, vfo_op_t op);
static int ft817_get_split_vfo(RIG *rig, vfo_t vfo, split_t *split,
                               vfo_t *tx_vfo);
static int ft817_set_split_vfo(RIG *rig, vfo_t vfo, split_t split,
                               vfo_t tx_vfo);
static int ft817_power2mW(RIG *rig, unsigned int *mwpower, float power,
                          freq_t freq, rmode_t mode);
static int ft817_mW2power(RIG *rig, float *power, unsigned int mwpower,
                          freq_t freq, rmode_t mode);
static int ft817_get_ant(RIG *rig, vfo_t vfo, ant_t ant, value_t *option,
                         ant_t *ant_curr, ant_t *ant_tx, ant_t *ant_rx);
static int ft818_get_ant(RIG *rig, vfo_t vfo, ant_t ant, value_t *option,
                         ant_t *ant_curr, ant_t *ant_tx, ant_t *ant_rx);

/* Native ft817 cmd set prototypes. These are READ ONLY as each */
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
    { 1, { 0x2a, 0x00, 0x00, 0x00, 0x0a } }, /* set CTCSS on */
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
    { 1, { 0x00, 0x00, 0x00, 0x00, 0xbd } }, /* get TX metering levels (PWR, SWR, MOD, ALC) */
};

enum ft817_digi
{
    FT817_DIGI_RTTY = 0,
    FT817_DIGI_PSK_L,
    FT817_DIGI_PSK_U,
    FT817_DIGI_USER_L,
    FT817_DIGI_USER_U,
};

#define FT817_ALL_RX_MODES      (RIG_MODE_AM|RIG_MODE_CW|RIG_MODE_CWR|RIG_MODE_PKTFM|\
                                 RIG_MODE_USB|RIG_MODE_LSB|RIG_MODE_RTTY|RIG_MODE_FM|RIG_MODE_PKTUSB)
#define FT817_SSB_CW_RX_MODES   (RIG_MODE_CW|RIG_MODE_CWR|RIG_MODE_USB|RIG_MODE_LSB|RIG_MODE_RTTY)
#define FT817_CWN_RX_MODES      (RIG_MODE_CW|RIG_MODE_CWR|RIG_MODE_RTTY)
#define FT817_AM_FM_RX_MODES    (RIG_MODE_AM|RIG_MODE_FM|RIG_MODE_PKTFM)

#define FT817_OTHER_TX_MODES    (RIG_MODE_CW|RIG_MODE_CWR|RIG_MODE_USB|\
                                 RIG_MODE_LSB|RIG_MODE_RTTY|RIG_MODE_FM|RIG_MODE_PKTUSB)
#define FT817_AM_TX_MODES       (RIG_MODE_AM)

#define FT817_VFO_ALL           (RIG_VFO_A|RIG_VFO_B)
#define FT817_ANT_FRONT         (RIG_ANT_1)
#define FT817_ANT_REAR          (RIG_ANT_2)
#define FT817_ANTS              (FT817_ANT_FRONT | FT817_ANT_REAR)

#define FT817_STR_CAL { 16, \
                { \
                    { 0x00, -54 }, /* S0 */ \
                    { 0x01, -48 }, \
                    { 0x02, -42 }, \
                    { 0x03, -36 }, \
                    { 0x04, -30 }, \
                    { 0x05, -24 }, \
                    { 0x06, -18 }, \
                    { 0x07, -12 }, \
                    { 0x08, -6 }, \
                    { 0x09,  0 },  /* S9 */ \
                    { 0x0A,  10 }, /* +10 */ \
                    { 0x0B,  20 }, /* +20 */ \
                    { 0x0C,  30 }, /* +30 */ \
                    { 0x0D,  40 }, /* +40 */ \
                    { 0x0E,  50 }, /* +50 */ \
                    { 0x0F,  60 }  /* +60 */ \
                } }

// Thanks to Olivier Schmitt sc.olivier@gmail.com for these tables
#define FT817_PWR_CAL { 9, \
                { \
                    { 0x00, 0 }, \
                    { 0x01, 10 }, \
                    { 0x02, 14 }, \
                    { 0x03, 20 }, \
                    { 0x04, 34 }, \
                    { 0x05, 50 }, \
                    { 0x06, 66 }, \
                    { 0x07, 82 }, \
                    { 0x08, 100 } \
                } }

#define FT817_ALC_CAL { 6, \
                { \
                    { 0x00, 0 }, \
                    { 0x01, 20 }, \
                    { 0x02, 40 }, \
                    { 0x03, 60 }, \
                    { 0x04, 80 }, \
                    { 0x05, 100 } \
                } }

#define FT817_SWR_CAL { 2, \
                { \
                    { 0, 0 }, \
                    { 15, 100 } \
                } }


const struct rig_caps ft817_caps =
{
    RIG_MODEL(RIG_MODEL_FT817),
    .model_name =          "FT-817",
    .mfg_name =            "Yaesu",
    .version =             "20220419.0",
    .copyright =           "LGPL",
    .status =              RIG_STATUS_STABLE,
    .rig_type =            RIG_TYPE_TRANSCEIVER,
    .ptt_type =            RIG_PTT_RIG,
    .dcd_type =            RIG_DCD_RIG,
    .port_type =           RIG_PORT_SERIAL,
    .serial_rate_min =     4800,
    .serial_rate_max =     38400,
    .serial_data_bits =    8,
    .serial_stop_bits =    2,
    .serial_parity =       RIG_PARITY_NONE,
    .serial_handshake =    RIG_HANDSHAKE_NONE,
    .write_delay =         FT817_WRITE_DELAY,
    .post_write_delay =    FT817_POST_WRITE_DELAY,
    .timeout =             FT817_TIMEOUT,
    .retry =               5,
    .has_get_func =        RIG_FUNC_NONE,
    .has_set_func =        RIG_FUNC_LOCK | RIG_FUNC_TONE | RIG_FUNC_TSQL | RIG_FUNC_CSQL | RIG_FUNC_RIT,
    .has_get_level =
    RIG_LEVEL_STRENGTH | RIG_LEVEL_RAWSTR | RIG_LEVEL_RFPOWER |
    RIG_LEVEL_ALC | RIG_LEVEL_SWR,
    .has_set_level =       RIG_LEVEL_BAND_SELECT,
    .has_get_parm =        RIG_PARM_NONE,
    .has_set_parm =        RIG_PARM_NONE,
    .level_gran =
    {
#include "level_gran_yaesu.h"
    },
    .parm_gran =           {},
    .ctcss_list =          common_ctcss_list,
    .dcs_list =            common_dcs_list,   /* only 104 out of 106 supported */
    .preamp =              { RIG_DBLST_END, },
    .attenuator =          { RIG_DBLST_END, },
    .max_rit =             Hz(9990),
    .max_xit =             Hz(0),
    .max_ifshift =         Hz(0),
    .vfo_ops =             RIG_OP_TOGGLE,
    .targetable_vfo =      0,
    .transceive =          RIG_TRN_OFF,
    .bank_qty =            0,
    .chan_desc_sz =        0,
    .chan_list =           { RIG_CHAN_END, },

    .rx_range_list1 =  {
        {kHz(100), MHz(56), FT817_ALL_RX_MODES,  -1, -1, FT817_VFO_ALL, FT817_ANTS},
        {MHz(76), MHz(108), RIG_MODE_WFM,        -1, -1, FT817_VFO_ALL, FT817_ANTS},
        {MHz(118), MHz(164), FT817_ALL_RX_MODES, -1, -1, FT817_VFO_ALL, FT817_ANTS},
        {MHz(420), MHz(470), FT817_ALL_RX_MODES, -1, -1, FT817_VFO_ALL, FT817_ANTS},
        RIG_FRNG_END,
    },
    .tx_range_list1 =  {
        FRQ_RNG_HF(1, FT817_OTHER_TX_MODES, W(0.5), W(5), FT817_VFO_ALL, FT817_ANTS),
        FRQ_RNG_HF(1, FT817_AM_TX_MODES, W(0.5), W(1.5), FT817_VFO_ALL, FT817_ANTS),

        FRQ_RNG_6m(1, FT817_OTHER_TX_MODES, W(0.5), W(5), FT817_VFO_ALL, FT817_ANTS),
        FRQ_RNG_6m(1, FT817_AM_TX_MODES, W(0.5), W(1.5), FT817_VFO_ALL, FT817_ANTS),

        FRQ_RNG_2m(1, FT817_OTHER_TX_MODES, W(0.5), W(5), FT817_VFO_ALL, FT817_ANTS),
        FRQ_RNG_2m(1, FT817_AM_TX_MODES, W(0.5), W(1.5), FT817_VFO_ALL, FT817_ANTS),

        FRQ_RNG_70cm(1, FT817_OTHER_TX_MODES, W(0.5), W(5), FT817_VFO_ALL, FT817_ANTS),
        FRQ_RNG_70cm(1, FT817_AM_TX_MODES, W(0.5), W(1.5), FT817_VFO_ALL, FT817_ANTS),

        RIG_FRNG_END,
    },


    .rx_range_list2 =  {
        {kHz(100), MHz(56), FT817_ALL_RX_MODES,  -1, -1, FT817_VFO_ALL, FT817_ANTS},
        {MHz(76), MHz(108), RIG_MODE_WFM,        -1, -1, FT817_VFO_ALL, FT817_ANTS},
        {MHz(118), MHz(164), FT817_ALL_RX_MODES, -1, -1, FT817_VFO_ALL, FT817_ANTS},
        {MHz(420), MHz(470), FT817_ALL_RX_MODES, -1, -1, FT817_VFO_ALL, FT817_ANTS},
        RIG_FRNG_END,
    },

    .tx_range_list2 =  {
        FRQ_RNG_HF(2, FT817_OTHER_TX_MODES, W(0.5), W(5), FT817_VFO_ALL, FT817_ANTS),
        FRQ_RNG_HF(2, FT817_AM_TX_MODES, W(0.5), W(1.5), FT817_VFO_ALL, FT817_ANTS),
        /* FIXME: 60 meters in US version */

        FRQ_RNG_6m(2, FT817_OTHER_TX_MODES, W(0.5), W(5), FT817_VFO_ALL, FT817_ANTS),
        FRQ_RNG_6m(2, FT817_AM_TX_MODES, W(0.5), W(1.5), FT817_VFO_ALL, FT817_ANTS),

        FRQ_RNG_2m(2, FT817_OTHER_TX_MODES, W(0.5), W(5), FT817_VFO_ALL, FT817_ANTS),
        FRQ_RNG_2m(2, FT817_AM_TX_MODES, W(0.5), W(1.5), FT817_VFO_ALL, FT817_ANTS),

        FRQ_RNG_70cm(2, FT817_OTHER_TX_MODES, W(0.5), W(5), FT817_VFO_ALL, FT817_ANTS),
        FRQ_RNG_70cm(2, FT817_AM_TX_MODES, W(0.5), W(1.5), FT817_VFO_ALL, FT817_ANTS),

        RIG_FRNG_END,
    },

    .tuning_steps =  {
        {FT817_SSB_CW_RX_MODES, Hz(10)},
        {FT817_AM_FM_RX_MODES | RIG_MODE_WFM, Hz(100)},
        RIG_TS_END,
    },

    .filters = {
        {FT817_SSB_CW_RX_MODES, kHz(2.2)},  /* normal passband */
        {FT817_CWN_RX_MODES, 500},          /* CW and RTTY narrow */
        {RIG_MODE_AM, kHz(6)},              /* AM normal */
        {RIG_MODE_FM | RIG_MODE_PKTFM, kHz(9)},
        {RIG_MODE_WFM, kHz(15)},
        RIG_FLT_END,
    },

    .str_cal =          FT817_STR_CAL,
    .swr_cal =          FT817_SWR_CAL,
    .alc_cal =          FT817_ALC_CAL,
    .rfpower_meter_cal = FT817_PWR_CAL,

    .rig_init =         ft817_init,
    .rig_cleanup =      ft817_cleanup,
    .rig_open =         ft817_open,
    .rig_close =        ft817_close,
    .get_vfo =          ft817_get_vfo,
    .set_vfo =          ft817_set_vfo,
    .set_freq =         ft817_set_freq,
    .get_freq =         ft817_get_freq,
    .set_mode =         ft817_set_mode,
    .get_mode =         ft817_get_mode,
    .set_ptt =          ft817_set_ptt,
    .get_ptt =          ft817_get_ptt,
    .get_dcd =          ft817_get_dcd,
    .set_rptr_shift =   ft817_set_rptr_shift,
    .set_rptr_offs =    ft817_set_rptr_offs,
    .set_split_vfo =    ft817_set_split_vfo,
    .get_split_vfo =    ft817_get_split_vfo,
    .set_rit =          ft817_set_rit,
    .set_dcs_code =     ft817_set_dcs_code,
    .set_ctcss_tone =   ft817_set_ctcss_tone,
    .set_dcs_sql =      ft817_set_dcs_sql,
    .set_ctcss_sql =    ft817_set_ctcss_sql,
    .power2mW =         ft817_power2mW,
    .mW2power =         ft817_mW2power,
    .set_powerstat =    ft817_set_powerstat,
    .get_ant =          ft817_get_ant,
    .get_level =        ft817_get_level,
    .set_func =         ft817_set_func,
    .vfo_op =           ft817_vfo_op,
    .hamlib_check_rig_caps = HAMLIB_CHECK_RIG_CAPS
};

const struct rig_caps ft818_caps =
{
    RIG_MODEL(RIG_MODEL_FT818),
    .model_name =          "FT-818",
    .mfg_name =            "Yaesu",
    .version =             "20220419.0",
    .copyright =           "LGPL",
    .status =              RIG_STATUS_STABLE,
    .rig_type =            RIG_TYPE_TRANSCEIVER,
    .ptt_type =            RIG_PTT_RIG,
    .dcd_type =            RIG_DCD_RIG,
    .port_type =           RIG_PORT_SERIAL,
    .serial_rate_min =     4800,
    .serial_rate_max =     38400,
    .serial_data_bits =    8,
    .serial_stop_bits =    2,
    .serial_parity =       RIG_PARITY_NONE,
    .serial_handshake =    RIG_HANDSHAKE_NONE,
    .write_delay =         FT817_WRITE_DELAY,
    .post_write_delay =    FT817_POST_WRITE_DELAY,
    .timeout =             FT817_TIMEOUT,
    .retry =               5,
    .has_get_func =        RIG_FUNC_NONE,
    .has_set_func =        RIG_FUNC_LOCK | RIG_FUNC_TONE | RIG_FUNC_TSQL | RIG_FUNC_RIT,
    .has_get_level =
    RIG_LEVEL_STRENGTH | RIG_LEVEL_RAWSTR | RIG_LEVEL_RFPOWER |
    RIG_LEVEL_ALC | RIG_LEVEL_SWR,
    .has_set_level =       RIG_LEVEL_BAND_SELECT,
    .has_get_parm =        RIG_PARM_NONE,
    .has_set_parm =        RIG_PARM_NONE,
    .level_gran =
    {
#include "level_gran_yaesu.h"
    },
    .parm_gran =           {},
    .ctcss_list =          common_ctcss_list,
    .dcs_list =            common_dcs_list,   /* only 104 out of 106 supported */
    .preamp =              { RIG_DBLST_END, },
    .attenuator =          { RIG_DBLST_END, },
    .max_rit =             Hz(9990),
    .max_xit =             Hz(0),
    .max_ifshift =         Hz(0),
    .vfo_ops =             RIG_OP_TOGGLE,
    .targetable_vfo =      0,
    .transceive =          RIG_TRN_OFF,
    .bank_qty =            0,
    .chan_desc_sz =        0,
    .chan_list =           { RIG_CHAN_END, },

    .rx_range_list1 =  {
        {kHz(100), MHz(56), FT817_ALL_RX_MODES,  -1, -1, FT817_VFO_ALL, FT817_ANTS},
        {MHz(76), MHz(108), RIG_MODE_WFM,        -1, -1, FT817_VFO_ALL, FT817_ANTS},
        {MHz(118), MHz(164), FT817_ALL_RX_MODES, -1, -1, FT817_VFO_ALL, FT817_ANTS},
        {MHz(420), MHz(470), FT817_ALL_RX_MODES, -1, -1, FT817_VFO_ALL, FT817_ANTS},
        RIG_FRNG_END,
    },
    .tx_range_list1 =  {
        FRQ_RNG_HF(1, FT817_OTHER_TX_MODES, W(0.5), W(5), FT817_VFO_ALL, FT817_ANTS),
        FRQ_RNG_HF(1, FT817_AM_TX_MODES, W(0.5), W(1.5), FT817_VFO_ALL, FT817_ANTS),

        /* One of the key differences between 817 and 818: the 818 has 60m! */
        FRQ_RNG_60m(1, FT817_OTHER_TX_MODES, W(0.5), W(5), FT817_VFO_ALL, FT817_ANTS),
        FRQ_RNG_60m(1, FT817_AM_TX_MODES, W(0.5), W(1.5), FT817_VFO_ALL, FT817_ANTS),

        FRQ_RNG_6m(1, FT817_OTHER_TX_MODES, W(0.5), W(5), FT817_VFO_ALL, FT817_ANTS),
        FRQ_RNG_6m(1, FT817_AM_TX_MODES, W(0.5), W(1.5), FT817_VFO_ALL, FT817_ANTS),

        FRQ_RNG_2m(1, FT817_OTHER_TX_MODES, W(0.5), W(5), FT817_VFO_ALL, FT817_ANTS),
        FRQ_RNG_2m(1, FT817_AM_TX_MODES, W(0.5), W(1.5), FT817_VFO_ALL, FT817_ANTS),

        FRQ_RNG_70cm(1, FT817_OTHER_TX_MODES, W(0.5), W(5), FT817_VFO_ALL, FT817_ANTS),
        FRQ_RNG_70cm(1, FT817_AM_TX_MODES, W(0.5), W(1.5), FT817_VFO_ALL, FT817_ANTS),

        RIG_FRNG_END,
    },


    .rx_range_list2 =  {
        {kHz(100), MHz(56), FT817_ALL_RX_MODES,  -1, -1, FT817_VFO_ALL, FT817_ANTS},
        {MHz(76), MHz(108), RIG_MODE_WFM,        -1, -1, FT817_VFO_ALL, FT817_ANTS},
        {MHz(118), MHz(164), FT817_ALL_RX_MODES, -1, -1, FT817_VFO_ALL, FT817_ANTS},
        {MHz(420), MHz(470), FT817_ALL_RX_MODES, -1, -1, FT817_VFO_ALL, FT817_ANTS},
        RIG_FRNG_END,
    },

    .tx_range_list2 =  {
        FRQ_RNG_HF(2, FT817_OTHER_TX_MODES, W(0.5), W(5), FT817_VFO_ALL, FT817_ANTS),
        FRQ_RNG_HF(2, FT817_AM_TX_MODES, W(0.5), W(1.5), FT817_VFO_ALL, FT817_ANTS),

        /* One of the key differences between 817 and 818: the 818 has 60m! */
        FRQ_RNG_60m(2, FT817_OTHER_TX_MODES, W(0.5), W(5), FT817_VFO_ALL, FT817_ANTS),
        FRQ_RNG_60m(2, FT817_AM_TX_MODES, W(0.5), W(1.5), FT817_VFO_ALL, FT817_ANTS),

        FRQ_RNG_6m(2, FT817_OTHER_TX_MODES, W(0.5), W(5), FT817_VFO_ALL, FT817_ANTS),
        FRQ_RNG_6m(2, FT817_AM_TX_MODES, W(0.5), W(1.5), FT817_VFO_ALL, FT817_ANTS),

        FRQ_RNG_2m(2, FT817_OTHER_TX_MODES, W(0.5), W(5), FT817_VFO_ALL, FT817_ANTS),
        FRQ_RNG_2m(2, FT817_AM_TX_MODES, W(0.5), W(1.5), FT817_VFO_ALL, FT817_ANTS),

        FRQ_RNG_70cm(2, FT817_OTHER_TX_MODES, W(0.5), W(5), FT817_VFO_ALL, FT817_ANTS),
        FRQ_RNG_70cm(2, FT817_AM_TX_MODES, W(0.5), W(1.5), FT817_VFO_ALL, FT817_ANTS),

        RIG_FRNG_END,
    },

    .tuning_steps =  {
        {FT817_SSB_CW_RX_MODES, Hz(10)},
        {FT817_AM_FM_RX_MODES | RIG_MODE_WFM, Hz(100)},
        RIG_TS_END,
    },

    .filters = {
        {FT817_SSB_CW_RX_MODES, kHz(2.2)},  /* normal passband */
        {FT817_CWN_RX_MODES, 500},          /* CW and RTTY narrow */
        {RIG_MODE_AM, kHz(6)},              /* AM normal */
        {RIG_MODE_FM | RIG_MODE_PKTFM, kHz(9)},
        {RIG_MODE_WFM, kHz(15)},
        RIG_FLT_END,
    },

    .str_cal =          FT817_STR_CAL,
    .swr_cal =          FT817_SWR_CAL,
    .alc_cal =          FT817_ALC_CAL,
    .rfpower_meter_cal = FT817_PWR_CAL,

    .rig_init =         ft817_init,
    .rig_cleanup =      ft817_cleanup,
    .rig_open =         ft817_open,
    .rig_close =        ft817_close,
    .get_vfo =          ft817_get_vfo,
    .set_vfo =          ft817_set_vfo,
    .set_freq =         ft817_set_freq,
    .get_freq =         ft817_get_freq,
    .set_mode =         ft817_set_mode,
    .get_mode =         ft817_get_mode,
    .set_ptt =          ft817_set_ptt,
    .get_ptt =          ft817_get_ptt,
    .get_dcd =          ft817_get_dcd,
    .set_rptr_shift =   ft817_set_rptr_shift,
    .set_rptr_offs =    ft817_set_rptr_offs,
    .set_split_vfo =    ft817_set_split_vfo,
    .get_split_vfo =    ft817_get_split_vfo,
    .set_rit =          ft817_set_rit,
    .set_dcs_code =     ft817_set_dcs_code,
    .set_ctcss_tone =   ft817_set_ctcss_tone,
    .set_dcs_sql =      ft817_set_dcs_sql,
    .set_ctcss_sql =    ft817_set_ctcss_sql,
    .power2mW =         ft817_power2mW,
    .mW2power =         ft817_mW2power,
    .set_powerstat =    ft817_set_powerstat,
    .get_ant =          ft818_get_ant,
    .get_level =        ft817_get_level,
    .set_func =         ft817_set_func,
    .vfo_op =           ft817_vfo_op,
    .hamlib_check_rig_caps = HAMLIB_CHECK_RIG_CAPS
};

/* ---------------------------------------------------------------------- */

static int ft817_init(RIG *rig)
{
    rig_debug(RIG_DEBUG_VERBOSE, "%s: called, version %s\n", __func__,
              rig->caps->version);

    if ((rig->state.priv = calloc(1, sizeof(struct ft817_priv_data))) == NULL)
    {
        return -RIG_ENOMEM;
    }

    return RIG_OK;
}

static int ft817_cleanup(RIG *rig)
{
    rig_debug(RIG_DEBUG_VERBOSE, "%s: called\n", __func__);

    free(rig->state.priv);

    rig->state.priv = NULL;

    return RIG_OK;
}

static int ft817_open(RIG *rig)
{
    rig_debug(RIG_DEBUG_VERBOSE, "%s: called \n", __func__);

    return RIG_OK;
}

static int ft817_close(RIG *rig)
{
    rig_debug(RIG_DEBUG_VERBOSE, "%s: called \n", __func__);

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

    if ((t = timediff(&curr, tv)) < FT817_CACHE_TIMEOUT)
    {
        rig_debug(RIG_DEBUG_VERBOSE, "ft817: using cache (%ld ms)\n", t);
        return 0;
    }
    else
    {
        rig_debug(RIG_DEBUG_VERBOSE, "ft817: cache timed out (%ld ms)\n", t);
        return 1;
    }
}

static int ft817_read_eeprom(RIG *rig, unsigned short addr, unsigned char *out)
{
    unsigned char data[YAESU_CMD_LENGTH];
    int n;

    rig_debug(RIG_DEBUG_VERBOSE, "%s: called\n", __func__);
    memcpy(data, ncmd[FT817_NATIVE_CAT_EEPROM_READ].nseq,
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

static int ft817_get_status(RIG *rig, int status)
{
    struct ft817_priv_data *p = (struct ft817_priv_data *) rig->state.priv;
    struct timeval *tv;
    unsigned char *data;
    int len;
    int n;
    int retries = rig->state.rigport.retry;
    unsigned char result[2];

    rig_debug(RIG_DEBUG_VERBOSE, "%s: called\n", __func__);

    switch (status)
    {
    case FT817_NATIVE_CAT_GET_FREQ_MODE_STATUS:
        data = p->fm_status;
        /* Answer is 5 long; 4 bytes BCD freq, 1 byte status */
        len  = 5;
        tv   = &p->fm_status_tv;
        break;

    case FT817_NATIVE_CAT_GET_RX_STATUS:
        data = &p->rx_status;
        len  = 1;
        tv   = &p->rx_status_tv;
        break;

    case FT817_NATIVE_CAT_GET_TX_METERING:
        data = result;
        len = sizeof(result) / sizeof(result[0]); /* We expect two bytes */
        tv = &p->tx_level_tv;
        break;

    case FT817_NATIVE_CAT_GET_TX_STATUS:
        data = &p->tx_status;
        len  = 1;
        tv   = &p->tx_status_tv;
        break;


    default:
        rig_debug(RIG_DEBUG_ERR, "%s: Internal error!\n", __func__);
        return -RIG_EINTERNAL;
    }

    do
    {
        rig_flush(&rig->state.rigport);
        write_block(&rig->state.rigport, ncmd[status].nseq,
                    YAESU_CMD_LENGTH);
        n = read_block(&rig->state.rigport, data, len);
    }
    while (retries-- && n < 0);

    if (n < 0)
    {
        return n;
    }

    if (n != len)
    {
        rig_debug(RIG_DEBUG_VERBOSE, "%s: Length mismatch exp %d got %d!\n",
                  __func__, len, n);
        return -RIG_EIO;
    }

    switch (status)
    {
    case FT817_NATIVE_CAT_GET_FREQ_MODE_STATUS:
    {
        /* Only in digimode we need fetch to extra bits from EEPROM.
         * This save communication cycle for all other modes.
         * Because mode and frequency are shared this saves also when
         * getting the frequency. */
        switch (p->fm_status[4] & 0x7f)
        {
            unsigned char dig_mode;

        case 0x0a:
            if ((n = ft817_read_eeprom(rig, 0x0065, &dig_mode)) < 0)
            {
                return n;
            }

            /* Top 3 bit define the digi mode */
            p->dig_mode = dig_mode >> 5;

        default:
            break;
        }
    }
    break;

    case FT817_NATIVE_CAT_GET_TX_METERING:
        /* FT-817 returns 2 bytes with 4 nibbles.
         * Extract raw values here;
         * convert to float when they are requested. */
        p->swr_level = result[0] & 0xF;
        p->pwr_level = result[0] >> 4;
        p->alc_level = result[1] & 0xF;
        p->mod_level = result[1] >> 4;
        rig_debug(RIG_DEBUG_TRACE, "%s: swr: %d, pwr %d, alc %d, mod %d\n",
                  __func__,
                  p->swr_level,
                  p->pwr_level,
                  p->alc_level,
                  p->mod_level);
        break;
    }

    gettimeofday(tv, NULL);

    return RIG_OK;
}

/* ---------------------------------------------------------------------- */

static int ft817_get_freq(RIG *rig, vfo_t vfo, freq_t *freq)
{
    struct ft817_priv_data *p = (struct ft817_priv_data *) rig->state.priv;
    freq_t f1 = 0, f2 = 0;
    int retries = rig->state.rigport.retry +
                  1; // +1 because, because 2 steps are needed even in best scenario

    rig_debug(RIG_DEBUG_VERBOSE, "%s: called, vfo=%s, ptt=%d, split=%d\n", __func__,
              rig_strvfo(vfo), rig->state.cache.ptt, rig->state.cache.split);

    // we can't query VFOB while in transmit and split mode
    if (rig->state.cache.ptt && vfo == RIG_VFO_B && rig->state.cache.split)
    {
        *freq = rig->state.cache.freqMainB;
        return RIG_OK;
    }

    while ((f1 == 0 || f1 != f2) && retries-- > 0)
    {
        int n;
        rig_debug(RIG_DEBUG_TRACE, "%s: retries=%d\n", __func__, retries);

        if ((n = ft817_get_status(rig, FT817_NATIVE_CAT_GET_FREQ_MODE_STATUS)) < 0)
        {
            return n;
        }

        f1 = f2;
        f2 = from_bcd_be(p->fm_status, 8);
        dump_hex(p->fm_status, sizeof(p->fm_status) / sizeof(p->fm_status[0]));
    }

#if 1 // user must be twiddling the VFO
    // usually get_freq is OK but we have to allow that f1 != f2 when knob is moving
    *freq = f2 * 10;
    return RIG_OK;
#else // remove this if no complaints

    if (retries >= 0)
    {
        *freq = f1 * 10;
        return RIG_OK;
    }
    else
    {
        return -RIG_EIO;
    }

#endif

}

static int ft817_get_mode(RIG *rig, vfo_t vfo, rmode_t *mode, pbwidth_t *width)
{
    struct ft817_priv_data *p = (struct ft817_priv_data *) rig->state.priv;

    rig_debug(RIG_DEBUG_VERBOSE, "%s: called\n", __func__);

    if (check_cache_timeout(&p->fm_status_tv))
    {
        int n;

        if ((n = ft817_get_status(rig, FT817_NATIVE_CAT_GET_FREQ_MODE_STATUS)) < 0)
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
        switch (p->dig_mode)
        {
        case FT817_DIGI_RTTY: *mode = RIG_MODE_RTTYR; break;

        case FT817_DIGI_PSK_L: *mode = RIG_MODE_PKTLSB; break;

        case FT817_DIGI_PSK_U: *mode = RIG_MODE_PKTUSB; break;

        case FT817_DIGI_USER_L: *mode = RIG_MODE_PKTLSB; break;

        case FT817_DIGI_USER_U: *mode = RIG_MODE_PKTUSB; break;

        default:
            *mode = RIG_MODE_NONE;
        }

        break;

    case 0x0C:
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

static int ft817_get_split_vfo(RIG *rig, vfo_t vfo, split_t *split,
                               vfo_t *tx_vfo)
{
    struct ft817_priv_data *p = (struct ft817_priv_data *) rig->state.priv;
    ptt_t ptt;
    int n;

    rig_debug(RIG_DEBUG_VERBOSE, "%s: called\n", __func__);

    n = ft817_get_ptt(rig, 0, &ptt);

    if (n != RIG_OK)
    {
        return n;
    }

    /* Check if rig is in TX mode */
    if (ptt == RIG_PTT_OFF)
    {
        // TX status not valid when in RX
        unsigned char c;

        /* Get split status from EEPROM */
        n = ft817_read_eeprom(rig, 0x7a, &c);

        if (n != RIG_OK)
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

static int ft817_get_ptt(RIG *rig, vfo_t vfo, ptt_t *ptt)
{
    struct ft817_priv_data *p = (struct ft817_priv_data *) rig->state.priv;

    rig_debug(RIG_DEBUG_VERBOSE, "%s: called\n", __func__);

    if (check_cache_timeout(&p->tx_status_tv))
    {
        int n;

        if ((n = ft817_get_status(rig, FT817_NATIVE_CAT_GET_TX_STATUS)) < 0)
        {
            return n;
        }
    }

    *ptt = ((p->tx_status & 0x80) == 0);

    return RIG_OK;
}

static int ft817_get_tx_level(RIG *rig, value_t *val, unsigned char *tx_level,
                              const cal_table_float_t *cal)
{
    struct ft817_priv_data *p = (struct ft817_priv_data *) rig->state.priv;

    rig_debug(RIG_DEBUG_VERBOSE, "%s: called\n", __func__);

    if (check_cache_timeout(&p->tx_level_tv))
    {
        int n;
        ptt_t ptt;

        /* Default to not keyed */
        *tx_level = 0;

        /* TX metering is special; it sends 1 byte if not keyed and 2 if keyed.
         * To handle this properly we first verify the rig is keyed.
         * Otherwise we experience at least a full timeout and
         * perhaps pointless retries + timeouts.
         */
        n = ft817_get_ptt(rig, 0, &ptt);

        if (n != RIG_OK)
        {
            return n;
        }

        if (ptt == RIG_PTT_OFF)
        {
            rig_debug(RIG_DEBUG_VERBOSE, "%s: rig not keyed\n", __func__);
            return -RIG_ERJCTED; //Or return OK?
        }

        n = ft817_get_status(rig, FT817_NATIVE_CAT_GET_TX_METERING);

        if (n != RIG_OK)
        {
            return n;
        }
    }

    val->f = rig_raw2val_float(*tx_level, cal);
    rig_debug(RIG_DEBUG_VERBOSE, "%s: level %f\n", __func__, val->f);

    return RIG_OK;
}

/* frontend will always use RAWSTR+cal_table */
static int ft817_get_smeter_level(RIG *rig, value_t *val)
{
    struct ft817_priv_data *p = (struct ft817_priv_data *) rig->state.priv;
    int n;

    rig_debug(RIG_DEBUG_VERBOSE, "%s: called\n", __func__);

    if (check_cache_timeout(&p->rx_status_tv))
        if ((n = ft817_get_status(rig, FT817_NATIVE_CAT_GET_RX_STATUS)) < 0)
        {
            return n;
        }

    //n = (p->rx_status & 0x0F) - 9;

    //val->i = n * ((n > 0) ? 10 : 6);

    /* S-meter value is returned in the lower 4 bits.
       0x00 = S0 (-54dB)
       0x01 = S1
       0x02 = S2
       ...
       0x09 = S9 (0dB)
       0x0A = S9+10 (10dB)
       0x0B = S9+20 and so on
    */
    n = (p->rx_status & 0x0F);

    if (n < 0x0A)
    {
        val->i = (6 * n) - 54;
    }
    else
    {
        val->i = 10 * (n - 9);
    }

    return RIG_OK;
}


static int ft817_get_raw_smeter_level(RIG *rig, value_t *val)
{
    struct ft817_priv_data *p = (struct ft817_priv_data *) rig->state.priv;

    rig_debug(RIG_DEBUG_VERBOSE, "%s: called\n", __func__);

    if (check_cache_timeout(&p->rx_status_tv))
    {
        int n;

        if ((n = ft817_get_status(rig, FT817_NATIVE_CAT_GET_RX_STATUS)) < 0)
        {
            return n;
        }
    }

    val->i = p->rx_status & 0x0F;

    return RIG_OK;
}


static int ft817_get_level(RIG *rig, vfo_t vfo, setting_t level, value_t *val)
{
    struct ft817_priv_data *p = (struct ft817_priv_data *) rig->state.priv;

    switch (level)
    {

    case RIG_LEVEL_STRENGTH:
        /* The front-end will always call for RAWSTR and use the cal_table */
        return ft817_get_smeter_level(rig, val);

    case RIG_LEVEL_RAWSTR:
        return ft817_get_raw_smeter_level(rig, val);

    case RIG_LEVEL_RFPOWER:
        return ft817_get_tx_level(rig, val, &p->pwr_level,
                                  &rig->caps->rfpower_meter_cal);

    case RIG_LEVEL_ALC:
        return ft817_get_tx_level(rig, val, &p->alc_level, &rig->caps->alc_cal);

    case RIG_LEVEL_SWR:
        return ft817_get_tx_level(rig, val, &p->swr_level, &rig->caps->swr_cal);

    default:
        return -RIG_EINVAL;
    }

    return RIG_OK;
}

static int ft817_get_dcd(RIG *rig, vfo_t vfo, dcd_t *dcd)
{
    struct ft817_priv_data *p = (struct ft817_priv_data *) rig->state.priv;

    rig_debug(RIG_DEBUG_VERBOSE, "%s: called\n", __func__);

    if (check_cache_timeout(&p->rx_status_tv))
    {
        int n;

        if ((n = ft817_get_status(rig, FT817_NATIVE_CAT_GET_RX_STATUS)) < 0)
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

static int ft818_817_get_ant(RIG *rig, vfo_t vfo, ant_t ant, value_t *option,
                             ant_t *ant_curr, ant_t *ant_tx, ant_t *ant_rx, bool is817)
{
    /* The FT818/817 has no RIG_TARGETABLE_ALL
     * so rig.c switched the active VFO to the one requested */
    int ret;
    unsigned char eeprom_band, eeprom_ant;

    /* Read eeprom for current 'band' for both VFO's */
    ret = ft817_read_eeprom(rig, 0x59, &eeprom_band);

    if (ret != RIG_OK)
    {
        return ret;
    }

    /* Read eeprom for antenna selection per band.
     * The FT818/817 stores antenna per band not per VFO!
     * So changing antenna will change for both VFO's */
    ret = ft817_read_eeprom(rig, 0x7A, &eeprom_ant);

    if (ret != RIG_OK)
    {
        return ret;
    }

    /* if CURR then get real VFO before parsing EEPROM */
    if (vfo == RIG_VFO_CURR)
    {
        vfo = rig->state.current_vfo;
    }

    /* band info is 4 bit per VFO, for A lower nibble, B is upper nible */
    switch (vfo)
    {
    case RIG_VFO_A:
        eeprom_band &= 0xF;
        break;

    case RIG_VFO_B:
        eeprom_band = eeprom_band >> 4;
        break;

    default:
        rig_debug(RIG_DEBUG_ERR, "%s: Unsupported VFO %0x!\n",
                  __func__, vfo);
        return -RIG_EINTERNAL;
    }

    /* The 818 and the 817 differ in bands: the 818 has 60m.
     * The band selection flags for the 818 and 817 thus differ:
     * 2 means 60m on 818 and 40m for 817.
     * And the rest of the values are shifted.
     *
     * So to make the code simple: if we have a 817 and 2 or higher band then
     * add 1 to the value to align it on the 818 mapping.
     */
    if (is817 && eeprom_band >= 2)
    {
        eeprom_band++;
    }

    /* The 817/818 does not have a antenna selection per VFO but per band.
     * So we read the band for the requested VFO and then map it to the
     * selected antenna.
     */


    switch (eeprom_band)
    {
    case 0:  /* 160M */
    case 1:  /*  80M */
    case 2:  /*  60M, 818 only */
    case 3:  /*  40M */
    case 4:  /*  30M */
    case 5:  /*  20M */
    case 6:  /*  17M */
    case 7:  /*  15M */
    case 8:  /*  12M */
    case 9:  /*  10M */
        /* All HF use the same antenna setting, bit 0 */
        eeprom_ant &= 1 << 0;
        break;

    case 0xA:  /* 6m, bit 1 */
        eeprom_ant &= 1 << 1;
        break;

    case 0xB:  /* FM BCB 76Mhz - 108Mhz, bit 2 */
        eeprom_ant &= 1 << 2;
        break;

    case 0xC:  /* Airband, bit 3 */
        eeprom_ant &= 1 << 3;
        break;

    case 0xD:  /* 2M, bit 4 */
        eeprom_ant &= 1 << 4;
        break;

    case 0xE:  /* 70cm / UHF, bit 5 */
        eeprom_ant &= 1 << 5;
        break;

    case 0xF: /* Free-tuning?, bit 6 */
        eeprom_ant &= 1 << 6;
        break;
    }

    /* We have no split TX/RX capability per VFO.
     * So only set ant_curr and leave rx/tx set to unknown. */
    *ant_curr = eeprom_ant ? FT817_ANT_REAR : FT817_ANT_FRONT;

    return RIG_OK;
}

static int ft817_get_ant(RIG *rig, vfo_t vfo, ant_t ant, value_t *option,
                         ant_t *ant_curr, ant_t *ant_tx, ant_t *ant_rx)
{
    return ft818_817_get_ant(rig, vfo, ant, option, ant_curr, ant_tx, ant_rx,
                             true);

}

static int ft818_get_ant(RIG *rig, vfo_t vfo, ant_t ant, value_t *option,
                         ant_t *ant_curr, ant_t *ant_tx, ant_t *ant_rx)
{
    return ft818_817_get_ant(rig, vfo, ant, option, ant_curr, ant_tx, ant_rx,
                             false);

}
/* ---------------------------------------------------------------------- */

int ft817_read_ack(RIG *rig)
{
    unsigned char dummy;
    rig_debug(RIG_DEBUG_VERBOSE, "%s: called\n", __func__);

    if (rig->state.rigport.post_write_delay == 0)
    {
        if (read_block(&rig->state.rigport, &dummy, 1) < 0)
        {
            rig_debug(RIG_DEBUG_ERR, "%s: error reading ack\n", __func__);
            rig_debug(RIG_DEBUG_ERR, "%s: adjusting post_write_delay to avoid ack\n",
                      __func__);
            rig->state.rigport.post_write_delay =
                10; // arbitrary choice right now of max 100 cmds/sec
            return RIG_OK; // let it continue without checking for ack now
        }

        rig_debug(RIG_DEBUG_TRACE, "%s: ack received (%d)\n", __func__, dummy);

        if (dummy != 0)
        {
            return -RIG_ERJCTED;
        }
    }

    return RIG_OK;
}

/*
 * private helper function to send a private command sequence.
 * Must only be complete sequences.
 */
static int ft817_send_cmd(RIG *rig, int index)
{
    rig_debug(RIG_DEBUG_VERBOSE, "%s: called\n", __func__);

    if (ncmd[index].ncomp == 0)
    {
        rig_debug(RIG_DEBUG_VERBOSE, "%s: Incomplete sequence\n", __func__);
        return -RIG_EINTERNAL;
    }

    rig_flush(&rig->state.rigport);
    write_block(&rig->state.rigport, ncmd[index].nseq, YAESU_CMD_LENGTH);
    return ft817_read_ack(rig);
}

/*
 * The same for incomplete commands.
 */
static int ft817_send_icmd(RIG *rig, int index, unsigned char *data)
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
static int ft817_get_vfo(RIG *rig, vfo_t *vfo)
{
    unsigned char c;

    rig_debug(RIG_DEBUG_VERBOSE, "%s: called \n", __func__);

    if (ft817_read_eeprom(rig, 0x55, &c) < 0)   /* get vfo status */
    {
        return -RIG_EPROTO;
    }

    if ((c & 0x1) == 0)
    {
        *vfo = RIG_VFO_A;
    }
    else
    {
        *vfo = RIG_VFO_B;
    }

    return RIG_OK;
}

static int ft817_set_vfo(RIG *rig, vfo_t vfo)
{
    vfo_t curvfo;
    int retval;

    rig_debug(RIG_DEBUG_VERBOSE, "%s: called \n", __func__);

    retval =  ft817_get_vfo(rig, &curvfo);

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

    return ft817_send_cmd(rig, FT817_NATIVE_CAT_SET_VFOAB);
}

static int ft817_set_freq(RIG *rig, vfo_t vfo, freq_t freq)
{
    unsigned char data[YAESU_CMD_LENGTH - 1];
    int retval;

    rig_debug(RIG_DEBUG_VERBOSE, "ft817: requested freq = %"PRIfreq" Hz\n", freq);

    /* fill in the frequency */
    to_bcd_be(data, (freq + 5) / 10, 8);

    rig_force_cache_timeout(
        &((struct ft817_priv_data *)rig->state.priv)->fm_status_tv);

    retval = ft817_send_icmd(rig, FT817_NATIVE_CAT_SET_FREQ, data);
    hl_usleep(50 * 1000); // FT817 needs a little time after setting freq
    return retval;
}

static int ft817_set_mode(RIG *rig, vfo_t vfo, rmode_t mode, pbwidth_t width)
{
    int index;  /* index of sequence to send */

    rig_debug(RIG_DEBUG_VERBOSE, "%s: generic mode = %s\n", __func__,
              rig_strrmode(mode));

    switch (mode)
    {

    case RIG_MODE_AM:
        index = FT817_NATIVE_CAT_SET_MODE_AM;
        break;

    case RIG_MODE_CW:
        index = FT817_NATIVE_CAT_SET_MODE_CW;
        break;

    case RIG_MODE_USB:
        index = FT817_NATIVE_CAT_SET_MODE_USB;
        break;

    case RIG_MODE_LSB:
        index = FT817_NATIVE_CAT_SET_MODE_LSB;
        break;

    case RIG_MODE_RTTY:
    case RIG_MODE_PKTUSB:
        /* user has to have correct DIG mode setup on rig */
        index = FT817_NATIVE_CAT_SET_MODE_DIG;
        break;

    case RIG_MODE_FM:
        index = FT817_NATIVE_CAT_SET_MODE_FM;
        break;

    case RIG_MODE_CWR:
        index = FT817_NATIVE_CAT_SET_MODE_CWR;
        break;

    case RIG_MODE_PKTFM:
        index = FT817_NATIVE_CAT_SET_MODE_PKT;
        break;

    default:
        return -RIG_EINVAL;
    }

    /* just ignore passband */
    /*  if (width != RIG_PASSBAND_NORMAL) */
    /*      return -RIG_EINVAL; */

    rig_force_cache_timeout(
        &((struct ft817_priv_data *)rig->state.priv)->fm_status_tv);

    return ft817_send_cmd(rig, index);
}

static int ft817_set_ptt(RIG *rig, vfo_t vfo, ptt_t ptt)
{
    int index;
    ptt_t ptt_response = -1;
    int retries = rig->state.rigport.retry;

    rig_debug(RIG_DEBUG_VERBOSE, "%s: called\n", __func__);

    switch (ptt)
    {
    case RIG_PTT_ON:
        index = FT817_NATIVE_CAT_PTT_ON;
        break;

    case RIG_PTT_OFF:
        index = FT817_NATIVE_CAT_PTT_OFF;
        break;

    default:
        return -RIG_EINVAL;
    }


    do
    {
        int n;
        n = ft817_send_cmd(rig, index);

        if (n < 0 && n != -RIG_ERJCTED)
        {
            rig_debug(RIG_DEBUG_ERR, "%s: send ptt cmd failed\n", __func__);
            return n;
        }

        /* Read TX status it contains the PTT flag.
         * Use TX_STATUS instead of ft817_get_ptt to skip the cache. */
        n = ft817_get_status(rig, FT817_NATIVE_CAT_GET_TX_STATUS);

        if (n < 0 && n != -RIG_ERJCTED)
        {
            rig_debug(RIG_DEBUG_ERR, "%s: get ptt cmd failed\n", __func__);
            return n;
        }

        /* Should be in cache now! But if above command was rejected
         * we will still try again here. */
        n = ft817_get_ptt(rig, vfo, &ptt_response);

        if (n < 0 && n != -RIG_ERJCTED)
        {
            rig_debug(RIG_DEBUG_ERR, "%s: get ptt cmd failed\n", __func__);
            return n;
        }

        if (ptt_response != ptt)
        {
            rig_debug(RIG_DEBUG_TRACE, "%s: ptt not requested level, retry\n", __func__);
            hl_usleep(1000l *
                      FT817_RETRY_DELAY); // Wait before next try. Helps with slower rigs cloning FT817 protocol (e.g. MCHF)
        }

    }
    while (ptt_response != ptt && retries-- > 0);

    if (retries >= 0)
    {
        return RIG_OK;
    }
    else
    {
        return -RIG_EIO;
    }

}

static int ft817_set_func(RIG *rig, vfo_t vfo, setting_t func, int status)
{
    rig_debug(RIG_DEBUG_VERBOSE, "%s: called\n", __func__);

    switch (func)
    {
    case RIG_FUNC_LOCK:
        if (status)
        {
            return ft817_send_cmd(rig, FT817_NATIVE_CAT_LOCK_ON);
        }
        else
        {
            return ft817_send_cmd(rig, FT817_NATIVE_CAT_LOCK_OFF);
        }

    case RIG_FUNC_TONE:
        if (status)
        {
            return ft817_send_cmd(rig, FT817_NATIVE_CAT_SET_CTCSS_ENC_ON);
        }
        else
        {
            return ft817_send_cmd(rig, FT817_NATIVE_CAT_SET_CTCSS_DCS_OFF);
        }

    case RIG_FUNC_TSQL:
        if (status)
        {
            return ft817_send_cmd(rig, FT817_NATIVE_CAT_SET_CTCSS_ON);
        }
        else
        {
            return ft817_send_cmd(rig, FT817_NATIVE_CAT_SET_CTCSS_DCS_OFF);
        }

    case RIG_FUNC_CSQL:
        if (status)
        {
            return ft817_send_cmd(rig, FT817_NATIVE_CAT_SET_DCS_ON);
        }
        else
        {
            return ft817_send_cmd(rig, FT817_NATIVE_CAT_SET_CTCSS_DCS_OFF);
        }

    case RIG_FUNC_RIT:
        if (status)
        {
            return ft817_send_cmd(rig, FT817_NATIVE_CAT_CLAR_ON);
        }
        else
        {
            return ft817_send_cmd(rig, FT817_NATIVE_CAT_CLAR_OFF);
        }

    default:
        return -RIG_EINVAL;
    }
}

static int ft817_set_dcs_code(RIG *rig, vfo_t vfo, tone_t code)
{
    unsigned char data[YAESU_CMD_LENGTH - 1];
    /*  int n; */

    rig_debug(RIG_DEBUG_VERBOSE, "ft817: set DCS code (%u)\n", code);

    if (code == 0)
    {
        return ft817_send_cmd(rig, FT817_NATIVE_CAT_SET_CTCSS_DCS_OFF);
    }

    /* fill in the DCS code - the rig doesn't support separate codes... */
    to_bcd_be(data,     code, 4);
    to_bcd_be(data + 2, code, 4);


    /* FT-817 does not have the DCS_ENC_ON command, so we just set the tone here */

    /*  if ((n = ft817_send_icmd(rig, FT817_NATIVE_CAT_SET_DCS_CODE, data)) < 0) */
    /*      return n; */

    /*  return ft817_send_cmd(rig, FT817_NATIVE_CAT_SET_DCS_ENC_ON); */

    return ft817_send_icmd(rig, FT817_NATIVE_CAT_SET_DCS_CODE, data);
}

static int ft817_set_dcs_sql(RIG *rig, vfo_t vfo, tone_t code)
{
    unsigned char data[YAESU_CMD_LENGTH - 1];
    int n;

    rig_debug(RIG_DEBUG_VERBOSE, "ft817: set DCS sql (%u)\n", code);

    if (code == 0)
    {
        return ft817_send_cmd(rig, FT817_NATIVE_CAT_SET_CTCSS_DCS_OFF);
    }

    /* fill in the DCS code - the rig doesn't support separate codes... */
    to_bcd_be(data,     code, 4);
    to_bcd_be(data + 2, code, 4);

    if ((n = ft817_send_icmd(rig, FT817_NATIVE_CAT_SET_DCS_CODE, data)) < 0)
    {
        return n;
    }

    return ft817_send_cmd(rig, FT817_NATIVE_CAT_SET_DCS_ON);
}


static int ft817_set_ctcss_tone(RIG *rig, vfo_t vfo, tone_t tone)
{
    unsigned char data[YAESU_CMD_LENGTH - 1];
    int n;

    rig_debug(RIG_DEBUG_VERBOSE, "ft817: set CTCSS tone (%.1f)\n", tone / 10.0);

    if (tone == 0)
    {
        return ft817_send_cmd(rig, FT817_NATIVE_CAT_SET_CTCSS_DCS_OFF);
    }

    /* fill in the CTCSS freq - the rig doesn't support separate tones... */
    to_bcd_be(data,     tone, 4);
    to_bcd_be(data + 2, tone, 4);

    if ((n = ft817_send_icmd(rig, FT817_NATIVE_CAT_SET_CTCSS_FREQ, data)) < 0)
    {
        return n;
    }

    return ft817_send_cmd(rig, FT817_NATIVE_CAT_SET_CTCSS_ENC_ON);
}


static int ft817_set_ctcss_sql(RIG *rig, vfo_t vfo, tone_t tone)
{
    unsigned char data[YAESU_CMD_LENGTH - 1];
    int n;

    rig_debug(RIG_DEBUG_VERBOSE, "ft817: set CTCSS sql (%.1f)\n", tone / 10.0);

    if (tone == 0)
    {
        return ft817_send_cmd(rig, FT817_NATIVE_CAT_SET_CTCSS_DCS_OFF);
    }

    /* fill in the CTCSS freq - the rig doesn't support separate tones... */
    to_bcd_be(data,     tone, 4);
    to_bcd_be(data + 2, tone, 4);

    if ((n = ft817_send_icmd(rig, FT817_NATIVE_CAT_SET_CTCSS_FREQ, data)) < 0)
    {
        return n;
    }

    return ft817_send_cmd(rig, FT817_NATIVE_CAT_SET_CTCSS_ON);
}

static int ft817_set_rptr_shift(RIG *rig, vfo_t vfo, rptr_shift_t shift)
{
    /* Note: this doesn't have effect unless FT817 is in FM mode
       although the command is accepted in any mode.
    */
    rig_debug(RIG_DEBUG_VERBOSE, "ft817: set repeter shift = %i\n", shift);

    switch (shift)
    {

    case RIG_RPT_SHIFT_NONE:
        return ft817_send_cmd(rig, FT817_NATIVE_CAT_SET_RPT_SHIFT_SIMPLEX);

    case RIG_RPT_SHIFT_MINUS:
        return ft817_send_cmd(rig, FT817_NATIVE_CAT_SET_RPT_SHIFT_MINUS);

    case RIG_RPT_SHIFT_PLUS:
        return ft817_send_cmd(rig, FT817_NATIVE_CAT_SET_RPT_SHIFT_PLUS);

    }

    return -RIG_EINVAL;
}

static int ft817_set_rptr_offs(RIG *rig, vfo_t vfo, shortfreq_t offs)
{
    unsigned char data[YAESU_CMD_LENGTH - 1];

    rig_debug(RIG_DEBUG_VERBOSE, "ft817: set repeter offs = %li\n", offs);

    /* fill in the offset freq */
    to_bcd_be(data, offs / 10, 8);

    return ft817_send_icmd(rig, FT817_NATIVE_CAT_SET_RPT_OFFSET, data);
}

static int ft817_set_rit(RIG *rig, vfo_t vfo, shortfreq_t rit)
{
    unsigned char data[YAESU_CMD_LENGTH - 1];
    int n;

    rig_debug(RIG_DEBUG_VERBOSE, "ft817: set rit = %li)\n", rit);

    /* fill in the RIT freq */
    data[0] = (rit < 0) ? 255 : 0;
    data[1] = 0;
    to_bcd_be(data + 2, labs(rit) / 10, 4);

    if ((n = ft817_send_icmd(rig, FT817_NATIVE_CAT_SET_CLAR_FREQ, data)) < 0)
    {
        return n;
    }

    /* the rig rejects if these are repeated - don't confuse user with retcode */

    /* not used anymore, RIG_FUNC_RIT implemented
    if (rit == 0)
    {
        ft817_send_cmd(rig, FT817_NATIVE_CAT_CLAR_OFF);
    }
    else
    {
        ft817_send_cmd(rig, FT817_NATIVE_CAT_CLAR_ON);
    } */

    return RIG_OK;
}


int ft817_set_powerstat(RIG *rig, powerstat_t status)
{
    rig_debug(RIG_DEBUG_VERBOSE, "%s: called\n", __func__);

    switch (status)
    {
    case RIG_POWER_OFF:
        return ft817_send_cmd(rig, FT817_NATIVE_CAT_PWR_OFF);

    case RIG_POWER_ON:
        // send 5 bytes first, snooze a bit, then PWR_ON
        write_block(&rig->state.rigport,
                    ncmd[FT817_NATIVE_CAT_PWR_WAKE].nseq, YAESU_CMD_LENGTH);
        hl_usleep(200 * 1000);
        write_block(&rig->state.rigport, ncmd[FT817_NATIVE_CAT_PWR_ON].nseq,
                    YAESU_CMD_LENGTH);
        return RIG_OK;

    case RIG_POWER_STANDBY:
    default:
        return -RIG_EINVAL;
    }
}

static int ft817_vfo_op(RIG *rig, vfo_t vfo, vfo_op_t op)
{
    rig_debug(RIG_DEBUG_VERBOSE, "%s: called\n", __func__);

    switch (op)
    {
        int n;

    case RIG_OP_TOGGLE:
        rig_force_cache_timeout(&((struct ft817_priv_data *)
                                  rig->state.priv)->fm_status_tv);
        n = ft817_send_cmd(rig, FT817_NATIVE_CAT_SET_VFOAB);
        hl_usleep(100 * 1000); // rig needs a little time to do this
        return n;

    default:
        return -RIG_EINVAL;
    }
}


/* FIXME: this function silently ignores the vfo args and just turns
   split ON or OFF.
*/
static int ft817_set_split_vfo(RIG *rig, vfo_t vfo, split_t split, vfo_t tx_vfo)
{
    int index, n;

    rig_debug(RIG_DEBUG_VERBOSE, "%s: called\n", __func__);

    switch (split)
    {

    case RIG_SPLIT_ON:
        index = FT817_NATIVE_CAT_SPLIT_ON;
        break;

    case RIG_SPLIT_OFF:
        index = FT817_NATIVE_CAT_SPLIT_OFF;
        break;

    default:
        return -RIG_EINVAL;
    }

    n = ft817_send_cmd(rig, index);

    if (n < 0 && n != -RIG_ERJCTED)
    {
        return n;
    }

    rig->state.cache.split = split;

    return RIG_OK;

}



/* FIXME: currently ignores mode and freq */
/*
   No documentation on how to interpret it but the max number
   of bars on the display is 10 and I measure:
                          8 bars = 5W
                          5 bars = 2.5W
                          3 bars = 1W
                          1 bar  = 0.5W
*/
static int ft817_power2mW(RIG *rig, unsigned int *mwpower, float power,
                          freq_t freq, rmode_t mode)
{
    rig_debug(RIG_DEBUG_VERBOSE, "%s: called\n", __func__);
    *mwpower = (int)(power * 6000);
    return RIG_OK;
}


/* FIXME: currently ignores mode and freq */
static int ft817_mW2power(RIG *rig, float *power, unsigned int mwpower,
                          freq_t freq, rmode_t mode)
{
    rig_debug(RIG_DEBUG_VERBOSE, "%s: called\n", __func__);
    *power = mwpower / 6000.0;
    return RIG_OK;
}


/* ---------------------------------------------------------------------- */
