/*
 * ft1000.c - (C) Stephane Fillod 2002-2005 (fillods@users.sourceforge.net)
 *
 * This shared library provides an API for communicating
 * via serial interface to an FT-1000MP using the "CAT" interface
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
 * Right now, this FT-1000MP implementation is a big mess.
 * This is actually a fast copy past (from ft920.c),
 * just to make get_freq/set_freq and co to work for a friend of mine.
 * I wouln't mind if someone could take over the maintenance
 * of this piece of code, and eventually rewrite it.
 * '02, Stephane
 */


#include <hamlib/config.h>

#include <stdlib.h>
#include <string.h>  /* String function definitions */
#include <math.h>

#include "hamlib/rig.h"
#include "bandplan.h"
#include "serial.h"
#include "misc.h"
#include "cal.h"
#include "yaesu.h"
#include "ft1000mp.h"

/*
 * Native FT1000MP functions. More to come :-)
 *
 */

enum ft1000mp_native_cmd_e
{
    FT1000MP_NATIVE_SPLIT_OFF = 0,
    FT1000MP_NATIVE_SPLIT_ON,            // 1
    FT1000MP_NATIVE_RECALL_MEM,          // 2
    FT1000MP_NATIVE_VFO_TO_MEM,          // 3
    FT1000MP_NATIVE_VFO_A,               // 4
    FT1000MP_NATIVE_VFO_B,               // 5
    FT1000MP_NATIVE_M_TO_VFO,            // 6
    FT1000MP_NATIVE_RIT_ON,              // 7
    FT1000MP_NATIVE_RIT_OFF,             // 8
    FT1000MP_NATIVE_XIT_ON,              // 9
    FT1000MP_NATIVE_XIT_OFF,             // 10
    FT1000MP_NATIVE_RXIT_SET,            // 11
    FT1000MP_NATIVE_FREQA_SET,           // 12
    FT1000MP_NATIVE_FREQB_SET,           // 13
    FT1000MP_NATIVE_MODE_SET_LSB,        // 14
    FT1000MP_NATIVE_MODE_SET_USB,        // 15
    FT1000MP_NATIVE_MODE_SET_CW,         // 16
    FT1000MP_NATIVE_MODE_SET_CWR,        // 17
    FT1000MP_NATIVE_MODE_SET_AM,         // 18
    FT1000MP_NATIVE_MODE_SET_AMS,        // 19
    FT1000MP_NATIVE_MODE_SET_FM,         // 20
    FT1000MP_NATIVE_MODE_SET_FMW,        // 21
    FT1000MP_NATIVE_MODE_SET_RTTY_LSB,   // 22
    FT1000MP_NATIVE_MODE_SET_RTTY_USB,   // 23
    FT1000MP_NATIVE_MODE_SET_DATA_LSB,   // 24
    FT1000MP_NATIVE_MODE_SET_DATA_FM,    // 25
    FT1000MP_NATIVE_MODE_SET_LSB_B,      // 26
    FT1000MP_NATIVE_MODE_SET_USB_B,      // 27
    FT1000MP_NATIVE_MODE_SET_CW_B,       // 28
    FT1000MP_NATIVE_MODE_SET_CWR_B,      // 29
    FT1000MP_NATIVE_MODE_SET_AM_B,       // 30
    FT1000MP_NATIVE_MODE_SET_AMS_B,      // 31
    FT1000MP_NATIVE_MODE_SET_FM_B,       // 32
    FT1000MP_NATIVE_MODE_SET_FMW_B,      // 33
    FT1000MP_NATIVE_MODE_SET_RTTY_LSB_B, // 34
    FT1000MP_NATIVE_MODE_SET_RTTY_USB_B, // 35
    FT1000MP_NATIVE_MODE_SET_DATA_LSB_B, // 36
    FT1000MP_NATIVE_MODE_SET_DATA_FM_B,  // 37
    FT1000MP_NATIVE_PACING,              // 38
    FT1000MP_NATIVE_PTT_OFF,             // 39
    FT1000MP_NATIVE_PTT_ON,              // 40
    FT1000MP_NATIVE_VFO_UPDATE,          // 41
    FT1000MP_NATIVE_CURR_VFO_UPDATE,     // 42
    FT1000MP_NATIVE_UPDATE,              // 43
    FT1000MP_NATIVE_AB,                  // 44
    FT1000MP_NATIVE_SIZE            /* end marker, value indicates number of */
    /* native cmd entries */

};

/*
 * API local implementation
 *
 */

static int ft1000mp_init(RIG *rig);
static int ft1000mp_cleanup(RIG *rig);
static int ft1000mp_open(RIG *rig);
//static int ft1000mp_close(RIG *rig);
static int ft1000mp_set_freq(RIG *rig, vfo_t vfo, freq_t freq);
static int ft1000mp_get_freq(RIG *rig, vfo_t vfo, freq_t *freq);
static int ft1000mp_set_split_freq(RIG *rig, vfo_t vfo, freq_t tx_freq);
static int ft1000mp_set_split_mode(RIG *rig, vfo_t vfo, rmode_t tx_mode,
                                   pbwidth_t tx_width);
static int ft1000mp_get_split_mode(RIG *rig, vfo_t vfo, rmode_t *tx_mode,
                                   pbwidth_t *tx_width);
static int ft1000mp_set_split_freq_mode(RIG *rig, vfo_t vfo, freq_t freq,
                                        rmode_t mode, pbwidth_t width);
static int ft1000mp_get_split_freq_mode(RIG *rig, vfo_t vfo, freq_t *freq,
                                        rmode_t *mode, pbwidth_t *width);
static int ft1000mp_get_split_freq(RIG *rig, vfo_t vfo, freq_t *tx_freq);
static int ft1000mp_set_split_vfo(RIG *rig, vfo_t vfo, split_t split,
                                  vfo_t tx_vfo);
static int ft1000mp_get_split_vfo(RIG *rig, vfo_t vfo, split_t *split,
                                  vfo_t *tx_vfo);
static int ft1000mp_set_mode(RIG *rig, vfo_t vfo, rmode_t mode,
                             pbwidth_t width); /* select mode */
static int ft1000mp_get_mode(RIG *rig, vfo_t vfo, rmode_t *mode,
                             pbwidth_t *width); /* get mode */
static int ft1000mp_set_vfo(RIG *rig, vfo_t vfo); /* select vfo */
static int ft1000mp_get_vfo(RIG *rig, vfo_t *vfo); /* get vfo */
static int ft1000mp_set_rit(RIG *rig, vfo_t vfo, shortfreq_t rit);
static int ft1000mp_set_xit(RIG *rig, vfo_t vfo, shortfreq_t rit);
static int ft1000mp_set_rxit(RIG *rig, vfo_t vfo, shortfreq_t rit);
static int ft1000mp_get_rxit(RIG *rig, vfo_t vfo, shortfreq_t *rit);
static int ft1000mp_get_level(RIG *rig, vfo_t vfo, setting_t level,
                              value_t *val);
static int ft1000mp_set_ptt(RIG *rig, vfo_t vfo, ptt_t ptt);
static int ft1000mp_set_func(RIG *rig, vfo_t vfo, setting_t func, int status);
static int ft1000mp_get_func(RIG *rig, vfo_t vfo, setting_t func, int *status);


/*
 * Differences between FT1000MP:
 * The FT1000MP MARK-V Field appears to be identical to FT1000MP,
 * whereas the FT1000MP MARK-V is a FT1000MP with 200W. TBC.
 */

/* Private helper function prototypes */

static int ft1000mp_get_update_data(RIG *rig, unsigned char ci,
                                    unsigned char rl);
static int ft1000mp_send_priv_cmd(RIG *rig, unsigned char ci);

/*
 * Native ft1000mp cmd set prototypes. These are READ ONLY as each
 * rig instance will copy from these and modify if required.
 * Complete sequences (1) can be read and used directly as a cmd sequence.
 * Incomplete sequences (0) must be completed with extra parameters
 * eg: mem number, or freq etc..
 *
 */

static const yaesu_cmd_set_t ncmd[] =
{
    { 1, { 0x00, 0x00, 0x00, 0x00, 0x01 } }, /* 0 split = off */
    { 1, { 0x00, 0x00, 0x00, 0x01, 0x01 } }, /* 1 split = on */
    { 0, { 0x00, 0x00, 0x00, 0x00, 0x02 } }, /* 2 recall memory */
    { 0, { 0x00, 0x00, 0x00, 0x00, 0x03 } }, /* 3 memory operations */
    { 1, { 0x00, 0x00, 0x00, 0x00, 0x05 } }, /* 4 select vfo A */
    { 1, { 0x00, 0x00, 0x00, 0x01, 0x05 } }, /* 5 select vfo B */
    { 0, { 0x00, 0x00, 0x00, 0x00, 0x06 } }, /* 6 copy memory data to vfo A */
    { 1, { 0x00, 0x00, 0x00, 0x01, 0x09 } }, /* 7 RX clarifier on */
    { 1, { 0x00, 0x00, 0x00, 0x00, 0x09 } }, /* 8 RX clarifier off */
    { 1, { 0x00, 0x00, 0x00, 0x81, 0x09 } }, /* 9 TX clarifier on */
    { 1, { 0x00, 0x00, 0x00, 0x80, 0x09 } }, /* 10 TX clarifier off */
    { 0, { 0x00, 0x00, 0x00, 0xFF, 0x09 } }, /* 11 set clarifier offset */

    { 0, { 0x00, 0x00, 0x00, 0x00, 0x0a } }, /* 12 set VFOA freq */
    { 0, { 0x00, 0x00, 0x00, 0x00, 0x8a } }, /* 13 set VFOB freq */

    { 1, { 0x00, 0x00, 0x00, 0x00, 0x0c } }, /* 14 vfo A mode set LSB */
    { 1, { 0x00, 0x00, 0x00, 0x01, 0x0c } }, /* 15 vfo A mode set USB */
    { 1, { 0x00, 0x00, 0x00, 0x02, 0x0c } }, /* 16 vfo A mode set CW-USB */
    { 1, { 0x00, 0x00, 0x00, 0x03, 0x0c } }, /* 17 vfo A mode set CW-LSB */
    { 1, { 0x00, 0x00, 0x00, 0x04, 0x0c } }, /* 18 vfo A mode set AM */
    { 1, { 0x00, 0x00, 0x00, 0x05, 0x0c } }, /* 19 vfo A mode set AM sync */
    { 1, { 0x00, 0x00, 0x00, 0x06, 0x0c } }, /* 20 vfo A mode set FM */
    { 1, { 0x00, 0x00, 0x00, 0x07, 0x0c } }, /* 21 vfo A mode set FMW? */
    { 1, { 0x00, 0x00, 0x00, 0x08, 0x0c } }, /* 22 vfo A mode set RTTY-LSB */
    { 1, { 0x00, 0x00, 0x00, 0x09, 0x0c } }, /* 23 vfo A mode set RTTY-USB */
    { 1, { 0x00, 0x00, 0x00, 0x0a, 0x0c } }, /* 24 vfo A mode set DATA-LSB */
    { 1, { 0x00, 0x00, 0x00, 0x0b, 0x0c } }, /* 25 vfo A mode set DATA-FM */

    { 1, { 0x00, 0x00, 0x00, 0x80, 0x0c } }, /* 26 vfo B mode set LSB */
    { 1, { 0x00, 0x00, 0x00, 0x81, 0x0c } }, /* 27 vfo B mode set USB */
    { 1, { 0x00, 0x00, 0x00, 0x82, 0x0c } }, /* 28 vfo B mode set CW-USB */
    { 1, { 0x00, 0x00, 0x00, 0x83, 0x0c } }, /* 29 vfo B mode set CW-LSB */
    { 1, { 0x00, 0x00, 0x00, 0x84, 0x0c } }, /* 30 vfo B mode set AM */
    { 1, { 0x00, 0x00, 0x00, 0x85, 0x0c } }, /* 31 vfo B mode set AM */
    { 1, { 0x00, 0x00, 0x00, 0x86, 0x0c } }, /* 32 vfo B mode set FM */
    { 1, { 0x00, 0x00, 0x00, 0x87, 0x0c } }, /* 33 vfo B mode set FMN */
    { 1, { 0x00, 0x00, 0x00, 0x88, 0x0c } }, /* 34 vfo B mode set DATA-LSB */
    { 1, { 0x00, 0x00, 0x00, 0x89, 0x0c } }, /* 35 vfo B mode set DATA-LSB */
    { 1, { 0x00, 0x00, 0x00, 0x8a, 0x0c } }, /* 36 vfo B mode set DATA-USB */
    { 1, { 0x00, 0x00, 0x00, 0x8b, 0x0c } }, /* 37 vfo B mode set DATA-FM */

    { 0, { 0x00, 0x00, 0x00, 0x00, 0x0e } }, /* 38 update interval/pacing */
    { 1, { 0x00, 0x00, 0x00, 0x00, 0x0F } }, /* 39 PTT OFF */
    { 1, { 0x00, 0x00, 0x00, 0x01, 0x0F } }, /* 40 PTT ON */
    { 1, { 0x00, 0x00, 0x00, 0x03, 0x10 } }, /* 41 status update VFO A & B update */
    { 1, { 0x00, 0x00, 0x00, 0x02, 0x10 } }, /* 42 status update operating data */
    // We only ask for the 1st 3 status bytes
    // The MARK-V was not recognizing the 6-byte request
    // This should be all we need as we're only getting the VFO
    { 1, { 0x00, 0x00, 0x00, 0x00, 0xFA } },        /* 43 Read status flags */
    { 1, { 0x00, 0x00, 0x00, 0x00, 0x85 } }, /* 44 A>B */
    /*  { 0, { 0x00, 0x00, 0x00, 0x00, 0x70 } }, */ /* 45 keyer commands */
    /*  { 1, { 0x00, 0x00, 0x00, 0x00, 0x81 } }, */ /* 46 tuner off */
    /*  { 1, { 0x00, 0x00, 0x00, 0x01, 0x81 } }, */ /* 47 tuner on */
    /*  { 1, { 0x00, 0x00, 0x00, 0x00, 0x82 } }, */ /* 48 tuner start*/

};



#define FT1000MP_ALL_RX_MODES (RIG_MODE_CW|RIG_MODE_SSB|RIG_MODE_RTTY|RIG_MODE_AM|RIG_MODE_FM)

/*
 * TX caps
 */

#define FT1000MP_OTHER_TX_MODES (RIG_MODE_CW| RIG_MODE_SSB|RIG_MODE_RTTY|RIG_MODE_FM) /* 100 W class */
#define FT1000MP_AM_TX_MODES (RIG_MODE_AM )    /* set 25W max */

#define FT1000MP_FUNC_ALL (RIG_FUNC_FAGC|RIG_FUNC_NB|RIG_FUNC_COMP|RIG_FUNC_VOX|RIG_FUNC_TONE|RIG_FUNC_TSQL \
                          |RIG_FUNC_SBKIN|RIG_FUNC_FBKIN|RIG_FUNC_LOCK \
                          |RIG_FUNC_RIT|RIG_FUNC_XIT \
                          /* |RIG_FUNC_TUNER */) /* FIXME */

#define FT1000MP_LEVEL_GET (RIG_LEVEL_RAWSTR|RIG_LEVEL_ALC|RIG_LEVEL_SWR|RIG_LEVEL_RFPOWER|RIG_LEVEL_COMP| \
                            RIG_LEVEL_MICGAIN|RIG_LEVEL_CWPITCH)

#define FT1000MP_VFOS (RIG_VFO_A|RIG_VFO_B)
#define FT1000MP_ANTS 0     /* FIXME: declare antenna connectors: ANT-A, ANT-B, RX ANT */

#define FT1000MP_VFO_OPS (RIG_OP_TO_VFO|RIG_OP_FROM_VFO|RIG_OP_CPY|RIG_OP_UP|RIG_OP_DOWN)

/**
 * 33 CTCSS sub-audible tones
 */
static tone_t ft1000mp_ctcss_list[] =
{
    670,        719,        770,        825,        885,
    948,       1000, 1035, 1072, 1109, 1148, 1188,       1230, 1273,
    1318, 1365, 1413, 1462, 1514, 1567, 1598, 1622,       1679,
    1738,       1799,       1862,       1928,
    2035,       2107, 2181, 2257,       2336, 2418, 2503,
    0,
};

#define FT1000MP_MEM_CAP {    \
        .freq = 1,      \
        .mode = 1,      \
        .width = 1,     \
        .ant = 1,      \
        .rit = 1,      \
        .xit = 1,      \
        .rptr_shift = 1,        \
        .flags = 1, \
    }


#define FT1000MP_STR_CAL { 12, \
    { \
        {   0, -60 }, \
        {  17, -54 }, /* S0 */ \
        {  17, -48 }, \
        {  34, -42 }, \
        {  51, -36 }, \
        {  68, -30 }, \
        {  85, -24 }, \
        { 102, -18 }, \
        { 119, -12 }, \
        { 136, -6 }, \
        { 160, 0 },  /* S9 */ \
        { 255, 60 }  /* +60 */ \
    } }

/*
 * future - private data
 *
 */

struct ft1000mp_priv_data
{
    unsigned char pacing;                     /* pacing value */
    unsigned char
    p_cmd[YAESU_CMD_LENGTH];    /* private copy of 1 constructed CAT cmd */
    unsigned char update_data[2 *
                                FT1000MP_STATUS_UPDATE_LENGTH]; /* returned data--max value, some are less */
};


/*
 * ft1000mp rigs capabilities.
 * Also this struct is READONLY!
 *
 */

const struct rig_caps ft1000mp_caps =
{
    RIG_MODEL(RIG_MODEL_FT1000MP),
    .model_name =         "FT-1000MP",
    .mfg_name =           "Yaesu",
    .version =            "20230104.0",
    .copyright =          "LGPL",
    .status =             RIG_STATUS_STABLE,
    .rig_type =           RIG_TYPE_TRANSCEIVER,
    .ptt_type =           RIG_PTT_RIG,
    .dcd_type =           RIG_DCD_RIG,
    .port_type =          RIG_PORT_SERIAL,
    .serial_rate_min =    4800,
    .serial_rate_max =    4800,
    .serial_data_bits =   8,
    .serial_stop_bits =   2,
    .serial_parity =      RIG_PARITY_NONE,
    .serial_handshake =   RIG_HANDSHAKE_NONE,
    .write_delay =        FT1000MP_WRITE_DELAY,
    .post_write_delay =   FT1000MP_POST_WRITE_DELAY,
    .timeout =            400, // was 2000 -- see https://github.com/Hamlib/Hamlib/issues/308
    .retry =              6,
    .has_get_func =       FT1000MP_FUNC_ALL,
    .has_set_func =       FT1000MP_FUNC_ALL,
    .has_get_level =      FT1000MP_LEVEL_GET,
    .has_set_level =      RIG_LEVEL_BAND_SELECT, /* as strange as it could be */
    .has_get_parm =       RIG_PARM_NONE,
    .has_set_parm =       RIG_PARM_NONE,
    .level_gran =
    {
#include "level_gran_yaesu.h"
    },
    .ctcss_list =         ft1000mp_ctcss_list,
    .dcs_list =           NULL,
    .vfo_ops =        FT1000MP_VFO_OPS,
    .preamp =             { RIG_DBLST_END, },
    .attenuator =         { RIG_DBLST_END, },
    .max_rit =            Hz(9999),
    .max_xit =            Hz(9999),
    .max_ifshift =        kHz(1.12),
    .targetable_vfo =     RIG_TARGETABLE_FREQ,
    .transceive =         RIG_TRN_OFF,
    .bank_qty =           0,
    .chan_desc_sz =       0,
    .chan_list =          {
        {   1,  99, RIG_MTYPE_MEM, FT1000MP_MEM_CAP },
        { 100, 108, RIG_MTYPE_EDGE },   /* P1 .. P9 */
        { 109, 113, RIG_MTYPE_MEMOPAD }, /* Q1 .. Q5 */
        RIG_CHAN_END,
    },
    .rx_range_list1 =     {
        {kHz(100), MHz(30), FT1000MP_ALL_RX_MODES, -1, -1, FT1000MP_VFOS, FT1000MP_ANTS },   /* General coverage + ham */
        RIG_FRNG_END,
    }, /* Region 1 rx ranges */

    .tx_range_list1 =     {
        FRQ_RNG_HF(1, FT1000MP_OTHER_TX_MODES, W(5), W(100), FT1000MP_VFOS, FT1000MP_ANTS),
        FRQ_RNG_HF(1, FT1000MP_AM_TX_MODES, W(2), W(25), FT1000MP_VFOS, FT1000MP_ANTS), /* AM class */
        RIG_FRNG_END,
    },    /* region 1 TX ranges */

    .rx_range_list2 =     {
        {kHz(100), MHz(30), FT1000MP_ALL_RX_MODES, -1, -1, FT1000MP_VFOS, FT1000MP_ANTS },   /* General coverage + ham */
        RIG_FRNG_END,
    }, /* Region 2 rx ranges */

    .tx_range_list2 =     {
        FRQ_RNG_HF(1, FT1000MP_OTHER_TX_MODES, W(5), W(100), FT1000MP_VFOS, FT1000MP_ANTS),
        FRQ_RNG_HF(1, FT1000MP_AM_TX_MODES, W(2), W(25), FT1000MP_VFOS, FT1000MP_ANTS), /* AM class */
        RIG_FRNG_END,
    },    /* region 2 TX ranges */

    .tuning_steps =       {
        {RIG_MODE_CW | RIG_MODE_SSB | RIG_MODE_RTTY, Hz(10)}, /* Normal */
        {RIG_MODE_CW | RIG_MODE_SSB | RIG_MODE_RTTY, Hz(100)}, /* Fast */

        {RIG_MODE_AM,     Hz(100)},   /* Normal */
        {RIG_MODE_AM,     kHz(1)},    /* Fast */

        {RIG_MODE_FM,     Hz(100)},   /* Normal */
        {RIG_MODE_FM,     kHz(1)},    /* Fast */

        RIG_TS_END,

        /*
         * The FT-1000MP has a Fine tuning step which increments in 1 Hz steps
         * for SSB_CW_RX_MODES, and 10 Hz steps for AM_RX_MODES and
         * FM_RX_MODES.  It doesn't appear that anything finer than 10 Hz
         * is available through the CAT interface, however. -N0NB
         *
         */
    },

    /* mode/filter list, .remember =  order matters! */
    .filters =            {
        {RIG_MODE_SSB | RIG_MODE_CW | RIG_MODE_RTTY | RIG_MODE_AM,  kHz(2.4)},
        {RIG_MODE_SSB | RIG_MODE_CW | RIG_MODE_RTTY,  kHz(2.0)},
        {RIG_MODE_CW | RIG_MODE_RTTY,   Hz(500)},
        {RIG_MODE_CW | RIG_MODE_RTTY,   Hz(250)},
        {RIG_MODE_AM,   kHz(5)},    /* wide */
        {RIG_MODE_FM,   kHz(8)},   /* FM */

        RIG_FLT_END,
    },
    .str_cal = FT1000MP_STR_CAL,

    .priv =               NULL,           /* private data */

    .rig_init =           ft1000mp_init,
    .rig_cleanup =        ft1000mp_cleanup,
    .rig_open =           ft1000mp_open,     /* port opened */

    .set_freq =           ft1000mp_set_freq, /* set freq */
    .get_freq =           ft1000mp_get_freq, /* get freq */
    .set_mode =           ft1000mp_set_mode, /* set mode */
    .get_mode =           ft1000mp_get_mode, /* get mode */
    .set_vfo =            ft1000mp_set_vfo,  /* set vfo */
    .get_vfo =            ft1000mp_get_vfo,  /* get vfo */

    .set_split_freq =     ft1000mp_set_split_freq,
    .get_split_freq =     ft1000mp_get_split_freq,
    .set_split_mode =     ft1000mp_set_split_mode,
    .get_split_mode =     ft1000mp_get_split_mode,
    .set_split_freq_mode = ft1000mp_set_split_freq_mode,
    .get_split_freq_mode = ft1000mp_get_split_freq_mode,
    .set_split_vfo =      ft1000mp_set_split_vfo,
    .get_split_vfo =      ft1000mp_get_split_vfo,

    .get_rit =            ft1000mp_get_rxit,
    .set_rit =            ft1000mp_set_rit,
    .get_xit =            ft1000mp_get_rxit,
    .set_xit =            ft1000mp_set_xit,

    .get_level =          ft1000mp_get_level,
    .set_ptt =            ft1000mp_set_ptt,

    .set_func =           ft1000mp_set_func,
    .get_func =           ft1000mp_get_func,

    /* TODO: the remaining ... */
    .hamlib_check_rig_caps = HAMLIB_CHECK_RIG_CAPS
};

const struct rig_caps ft1000mpmkv_caps =
{
    RIG_MODEL(RIG_MODEL_FT1000MPMKV),
    .model_name =         "MARK-V FT-1000MP",
    .mfg_name =           "Yaesu",
    .version =            "20230104.0",
    .copyright =          "LGPL",
    .status =             RIG_STATUS_STABLE,
    .rig_type =           RIG_TYPE_TRANSCEIVER,
    .ptt_type =           RIG_PTT_RIG,
    .dcd_type =           RIG_DCD_RIG,
    .port_type =          RIG_PORT_SERIAL,
    .serial_rate_min =    4800,
    .serial_rate_max =    4800,
    .serial_data_bits =   8,
    .serial_stop_bits =   2,
    .serial_parity =      RIG_PARITY_NONE,
    .serial_handshake =   RIG_HANDSHAKE_NONE,
    .write_delay =        FT1000MP_WRITE_DELAY,
    .post_write_delay =   FT1000MP_POST_WRITE_DELAY,
    .timeout =            400,
    .retry =              6,
    .has_get_func =       FT1000MP_FUNC_ALL,
    .has_set_func =       FT1000MP_FUNC_ALL,
    .has_get_level =      FT1000MP_LEVEL_GET,
    .has_set_level =      RIG_LEVEL_BAND_SELECT, /* as strange as it could be */
    .has_get_parm =       RIG_PARM_NONE,
    .has_set_parm =       RIG_PARM_NONE,
    .level_gran =
    {
#include "level_gran_yaesu.h"
    },
    .ctcss_list =         ft1000mp_ctcss_list,
    .dcs_list =           NULL,
    .vfo_ops =        FT1000MP_VFO_OPS,
    .preamp =             { RIG_DBLST_END, },
    .attenuator =         { RIG_DBLST_END, },
    .max_rit =            Hz(9999),
    .max_xit =            Hz(9999),
    .max_ifshift =        kHz(1.12),
    .targetable_vfo =     RIG_TARGETABLE_FREQ,
    .transceive =         RIG_TRN_OFF,
    .bank_qty =           0,
    .chan_desc_sz =       0,
    .chan_list =          {
        {   1,  99, RIG_MTYPE_MEM, FT1000MP_MEM_CAP },
        { 100, 108, RIG_MTYPE_EDGE },   /* P1 .. P9 */
        { 109, 113, RIG_MTYPE_MEMOPAD }, /* Q1 .. Q5 */
        RIG_CHAN_END,
    },
    .rx_range_list1 =     {
        {kHz(100), MHz(30), FT1000MP_ALL_RX_MODES, -1, -1, FT1000MP_VFOS, FT1000MP_ANTS },   /* General coverage + ham */
        RIG_FRNG_END,
    }, /* Region 1 rx ranges */

    .tx_range_list1 =     {
        FRQ_RNG_HF(1, FT1000MP_OTHER_TX_MODES, W(5), W(200), FT1000MP_VFOS, FT1000MP_ANTS),
        FRQ_RNG_HF(1, FT1000MP_AM_TX_MODES, W(2), W(50), FT1000MP_VFOS, FT1000MP_ANTS), /* AM class */
        RIG_FRNG_END,
    },    /* region 1 TX ranges */

    .rx_range_list2 =     {
        {kHz(100), MHz(30), FT1000MP_ALL_RX_MODES, -1, -1, FT1000MP_VFOS, FT1000MP_ANTS },   /* General coverage + ham */
        RIG_FRNG_END,
    }, /* Region 2 rx ranges */

    .tx_range_list2 =     {
        FRQ_RNG_HF(1, FT1000MP_OTHER_TX_MODES, W(5), W(200), FT1000MP_VFOS, FT1000MP_ANTS),
        FRQ_RNG_HF(1, FT1000MP_AM_TX_MODES, W(2), W(50), FT1000MP_VFOS, FT1000MP_ANTS), /* AM class */
        RIG_FRNG_END,
    },    /* region 2 TX ranges */

    .tuning_steps =       {
        {RIG_MODE_CW | RIG_MODE_CWR | RIG_MODE_SSB | RIG_MODE_RTTY | RIG_MODE_RTTYR | RIG_MODE_PKTLSB, Hz(10)}, /* Normal */
        {RIG_MODE_CW | RIG_MODE_CWR | RIG_MODE_SSB | RIG_MODE_RTTY | RIG_MODE_RTTYR | RIG_MODE_PKTLSB, Hz(100)}, /* Fast */

        {RIG_MODE_AM | RIG_MODE_SAL,     Hz(100)}, /* Normal */
        {RIG_MODE_AM | RIG_MODE_SAL,     kHz(1)},  /* Fast */

        {RIG_MODE_FM | RIG_MODE_PKTFM,     Hz(100)}, /* Normal */
        {RIG_MODE_FM | RIG_MODE_PKTFM,     kHz(1)},  /* Fast */

        RIG_TS_END,

        /*
         * The FT-1000MP has a Fine tuning step which increments in 1 Hz steps
         * for SSB_CW_RX_MODES, and 10 Hz steps for AM_RX_MODES and
         * FM_RX_MODES.  It doesn't appear that anything finer than 10 Hz
         * is available through the CAT interface, however. -N0NB
         *
         */
    },

    /* mode/filter list, .remember =  order matters! */
    .filters =            {
        {RIG_MODE_SSB | RIG_MODE_CW | RIG_MODE_CWR | RIG_MODE_RTTY | RIG_MODE_RTTYR | RIG_MODE_AM | RIG_MODE_SAL | RIG_MODE_PKTLSB,  kHz(2.4)},
        {RIG_MODE_SSB | RIG_MODE_CW | RIG_MODE_CWR | RIG_MODE_RTTY | RIG_MODE_RTTYR | RIG_MODE_AM | RIG_MODE_SAL | RIG_MODE_PKTLSB,  kHz(2.0)},
        {RIG_MODE_CW | RIG_MODE_CWR | RIG_MODE_RTTY | RIG_MODE_RTTYR,   Hz(500)},
        {RIG_MODE_CW | RIG_MODE_CWR | RIG_MODE_RTTY | RIG_MODE_RTTYR,   Hz(250)},
        {RIG_MODE_AM | RIG_MODE_SAL,   kHz(5)}, /* wide */
        {RIG_MODE_FM | RIG_MODE_PKTFM,   kHz(8)}, /* FM */

        RIG_FLT_END,
    },
    .str_cal = FT1000MP_STR_CAL,

    .priv =               NULL,           /* private data */

    .rig_init =           ft1000mp_init,
    .rig_cleanup =        ft1000mp_cleanup,
    .rig_open =           ft1000mp_open,     /* port opened */

    .set_freq =           ft1000mp_set_freq, /* set freq */
    .get_freq =           ft1000mp_get_freq, /* get freq */
    .set_mode =           ft1000mp_set_mode, /* set mode */
    .get_mode =           ft1000mp_get_mode, /* get mode */
    .set_vfo =            ft1000mp_set_vfo,  /* set vfo */
    .get_vfo =            ft1000mp_get_vfo,  /* get vfo */

    .set_split_freq =     ft1000mp_set_split_freq,
    .get_split_freq =     ft1000mp_get_split_freq,
    .set_split_mode =     ft1000mp_set_split_mode,
    .get_split_mode =     ft1000mp_get_split_mode,
    .set_split_freq_mode = ft1000mp_set_split_freq_mode,
    .get_split_freq_mode = ft1000mp_get_split_freq_mode,
    .set_split_vfo =      ft1000mp_set_split_vfo,
    .get_split_vfo =      ft1000mp_get_split_vfo,

    .get_rit =            ft1000mp_get_rxit,
    .set_rit =            ft1000mp_set_rit,
    .get_xit =            ft1000mp_get_rxit,
    .set_xit =            ft1000mp_set_xit,

    .get_level =          ft1000mp_get_level,
    .set_ptt =            ft1000mp_set_ptt,

    .set_func =           ft1000mp_set_func,
    .get_func =           ft1000mp_get_func,

    .hamlib_check_rig_caps = HAMLIB_CHECK_RIG_CAPS
    /* TODO: the remaining ... */
};

const struct rig_caps ft1000mpmkvfld_caps =
{
    RIG_MODEL(RIG_MODEL_FT1000MPMKVFLD),
    .model_name =         "MARK-V Field FT-1000MP",
    .mfg_name =           "Yaesu",
    .version =            "20230104.0",
    .copyright =          "LGPL",
    .status =             RIG_STATUS_STABLE,
    .rig_type =           RIG_TYPE_TRANSCEIVER,
    .ptt_type =           RIG_PTT_RIG,
    .dcd_type =           RIG_DCD_RIG,
    .port_type =          RIG_PORT_SERIAL,
    .serial_rate_min =    4800,
    .serial_rate_max =    4800,
    .serial_data_bits =   8,
    .serial_stop_bits =   2,
    .serial_parity =      RIG_PARITY_NONE,
    .serial_handshake =   RIG_HANDSHAKE_NONE,
    .write_delay =        FT1000MP_WRITE_DELAY,
    .post_write_delay =   FT1000MP_POST_WRITE_DELAY,
    .timeout =            400,
    .retry =              6,
    .has_get_func =       FT1000MP_FUNC_ALL,
    .has_set_func =       FT1000MP_FUNC_ALL,
    .has_get_level =      FT1000MP_LEVEL_GET,
    .has_set_level =      RIG_LEVEL_BAND_SELECT, /* as strange as it could be */
    .has_get_parm =       RIG_PARM_NONE,
    .has_set_parm =       RIG_PARM_NONE,
    .level_gran =
    {
#include "level_gran_yaesu.h"
    },
    .ctcss_list =         ft1000mp_ctcss_list,
    .dcs_list =           NULL,
    .vfo_ops =        FT1000MP_VFO_OPS,
    .preamp =             { RIG_DBLST_END, },
    .attenuator =         { RIG_DBLST_END, },
    .max_rit =            Hz(9999),
    .max_xit =            Hz(9999),
    .max_ifshift =        kHz(1.12),
    .targetable_vfo =     RIG_TARGETABLE_FREQ,
    .transceive =         RIG_TRN_OFF,
    .bank_qty =           0,
    .chan_desc_sz =       0,
    .chan_list =          {
        {   1,  99, RIG_MTYPE_MEM, FT1000MP_MEM_CAP },
        { 100, 108, RIG_MTYPE_EDGE },   /* P1 .. P9 */
        { 109, 113, RIG_MTYPE_MEMOPAD }, /* Q1 .. Q5 */
        RIG_CHAN_END,
    },
    .rx_range_list1 =     {
        {kHz(100), MHz(30), FT1000MP_ALL_RX_MODES, -1, -1, FT1000MP_VFOS, FT1000MP_ANTS },   /* General coverage + ham */
        RIG_FRNG_END,
    }, /* Region 1 rx ranges */

    .tx_range_list1 =     {
        FRQ_RNG_HF(1, FT1000MP_OTHER_TX_MODES, W(5), W(100), FT1000MP_VFOS, FT1000MP_ANTS),
        FRQ_RNG_HF(1, FT1000MP_AM_TX_MODES, W(2), W(25), FT1000MP_VFOS, FT1000MP_ANTS), /* AM class */
        RIG_FRNG_END,
    },    /* region 1 TX ranges */

    .rx_range_list2 =     {
        {kHz(100), MHz(30), FT1000MP_ALL_RX_MODES, -1, -1, FT1000MP_VFOS, FT1000MP_ANTS },   /* General coverage + ham */
        RIG_FRNG_END,
    }, /* Region 2 rx ranges */

    .tx_range_list2 =     {
        FRQ_RNG_HF(1, FT1000MP_OTHER_TX_MODES, W(5), W(100), FT1000MP_VFOS, FT1000MP_ANTS),
        FRQ_RNG_HF(1, FT1000MP_AM_TX_MODES, W(2), W(25), FT1000MP_VFOS, FT1000MP_ANTS), /* AM class */
        RIG_FRNG_END,
    },    /* region 2 TX ranges */

    .tuning_steps =       {
        {RIG_MODE_CW | RIG_MODE_CWR | RIG_MODE_SSB | RIG_MODE_RTTY | RIG_MODE_RTTYR | RIG_MODE_PKTLSB, Hz(10)}, /* Normal */
        {RIG_MODE_CW | RIG_MODE_CWR | RIG_MODE_SSB | RIG_MODE_RTTY | RIG_MODE_RTTYR | RIG_MODE_PKTLSB, Hz(100)}, /* Fast */

        {RIG_MODE_AM | RIG_MODE_SAL,     Hz(100)}, /* Normal */
        {RIG_MODE_AM | RIG_MODE_SAL,     kHz(1)},  /* Fast */

        {RIG_MODE_FM | RIG_MODE_PKTFM,     Hz(100)}, /* Normal */
        {RIG_MODE_FM | RIG_MODE_PKTFM,     kHz(1)},  /* Fast */

        RIG_TS_END,

        /*
         * The FT-1000MP has a Fine tuning step which increments in 1 Hz steps
         * for SSB_CW_RX_MODES, and 10 Hz steps for AM_RX_MODES and
         * FM_RX_MODES.  It doesn't appear that anything finer than 10 Hz
         * is available through the CAT interface, however. -N0NB
         *
         */
    },

    /* mode/filter list, .remember =  order matters! */
    .filters =            {
        {RIG_MODE_SSB | RIG_MODE_CW | RIG_MODE_CWR | RIG_MODE_RTTY | RIG_MODE_RTTYR | RIG_MODE_AM | RIG_MODE_SAL | RIG_MODE_PKTLSB,  kHz(2.4)},
        {RIG_MODE_SSB | RIG_MODE_CW | RIG_MODE_CWR | RIG_MODE_RTTY | RIG_MODE_RTTYR | RIG_MODE_AM | RIG_MODE_SAL | RIG_MODE_PKTLSB,  kHz(2.0)},
        {RIG_MODE_CW | RIG_MODE_CWR | RIG_MODE_RTTY | RIG_MODE_RTTYR,   Hz(500)},
        {RIG_MODE_CW | RIG_MODE_CWR | RIG_MODE_RTTY | RIG_MODE_RTTYR,   Hz(250)},
        {RIG_MODE_AM | RIG_MODE_SAL,   kHz(5)}, /* wide */
        {RIG_MODE_FM | RIG_MODE_PKTFM,   kHz(8)}, /* FM */

        RIG_FLT_END,
    },
    .str_cal = FT1000MP_STR_CAL,

    .priv =               NULL,           /* private data */

    .rig_init =           ft1000mp_init,
    .rig_cleanup =        ft1000mp_cleanup,
    .rig_open =           ft1000mp_open,     /* port opened */

    .set_freq =           ft1000mp_set_freq, /* set freq */
    .get_freq =           ft1000mp_get_freq, /* get freq */
    .set_mode =           ft1000mp_set_mode, /* set mode */
    .get_mode =           ft1000mp_get_mode, /* get mode */
    .set_vfo =            ft1000mp_set_vfo,  /* set vfo */
    .get_vfo =            ft1000mp_get_vfo,  /* get vfo */

    .set_split_freq =     ft1000mp_set_split_freq,
    .get_split_freq =     ft1000mp_get_split_freq,
    .set_split_mode =     ft1000mp_set_split_mode,
    .get_split_mode =     ft1000mp_get_split_mode,
    .set_split_freq_mode = ft1000mp_set_split_freq_mode,
    .get_split_freq_mode = ft1000mp_get_split_freq_mode,
    .set_split_vfo =      ft1000mp_set_split_vfo,
    .get_split_vfo =      ft1000mp_get_split_vfo,

    .get_rit =            ft1000mp_get_rxit,
    .set_rit =            ft1000mp_set_rit,
    .get_xit =            ft1000mp_get_rxit,
    .set_xit =            ft1000mp_set_xit,

    .get_level =          ft1000mp_get_level,
    .set_ptt =            ft1000mp_set_ptt,

    .set_func =           ft1000mp_set_func,
    .get_func =           ft1000mp_get_func,

    .hamlib_check_rig_caps = HAMLIB_CHECK_RIG_CAPS
    /* TODO: the remaining ... */
};


/*
 * _init
 *
 */

static int ft1000mp_init(RIG *rig)
{
    struct ft1000mp_priv_data *priv;

    ENTERFUNC;

    rig->state.priv = (struct ft1000mp_priv_data *) calloc(1,
                      sizeof(struct ft1000mp_priv_data));

    if (!rig->state.priv)                       /* whoops! memory shortage! */
    {
        RETURNFUNC(-RIG_ENOMEM);
    }

    priv = rig->state.priv;

    /* TODO: read pacing from preferences */
    priv->pacing =
        FT1000MP_PACING_DEFAULT_VALUE; /* set pacing to minimum for now */

    RETURNFUNC(RIG_OK);
}


/*
 * ft1000mp_cleanup routine
 * the serial port is closed by the frontend
 *
 */

static int ft1000mp_cleanup(RIG *rig)
{
    ENTERFUNC;

    if (rig->state.priv)
    {
        free(rig->state.priv);
    }

    rig->state.priv = NULL;

    RETURNFUNC(RIG_OK);
}


/*
 * ft1000mp_open  routine
 *
 */

static int ft1000mp_open(RIG *rig)
{
    struct rig_state *rig_s;
    struct ft1000mp_priv_data *p;
    unsigned char *cmd;           /* points to sequence to send */

    ENTERFUNC;

    rig_s = &rig->state;
    p = (struct ft1000mp_priv_data *)rig_s->priv;

    rig_debug(RIG_DEBUG_TRACE, "%s: rig_open: write_delay = %i msec \n", __func__,
              rig_s->rigport.write_delay);
    rig_debug(RIG_DEBUG_TRACE, "%s: rig_open: post_write_delay = %i msec \n",
              __func__,
              rig_s->rigport.post_write_delay);

    /*
     * Copy native cmd PACING  to private cmd storage area
     */
    memcpy(&p->p_cmd, &ncmd[FT1000MP_NATIVE_PACING].nseq, YAESU_CMD_LENGTH);
    p->p_cmd[3] = p->pacing;      /* get pacing value, and store in private cmd */
    rig_debug(RIG_DEBUG_TRACE, "%s: read pacing = %i\n", __func__, p->pacing);

    /* send PACING cmd to rig  */
    cmd = p->p_cmd;
    write_block(&rig->state.rigport, cmd, YAESU_CMD_LENGTH);

    ft1000mp_get_vfo(rig, &rig->state.current_vfo);
    /* TODO */

    RETURNFUNC(RIG_OK);
}



static int ft1000mp_set_freq(RIG *rig, vfo_t vfo, freq_t freq)
{
    struct ft1000mp_priv_data *p;
    unsigned char *cmd;           /* points to sequence to send */
    int cmd_index = 0;

    ENTERFUNC;

    p = (struct ft1000mp_priv_data *)rig->state.priv;

    rig_debug(RIG_DEBUG_TRACE, "%s: requested freq on %s = %"PRIfreq" Hz \n",
              __func__,
              rig_strvfo(vfo), freq);

    if (vfo == RIG_VFO_CURR)
    {
        vfo = rig->state.current_vfo;
    }

    switch (vfo)
    {
    case RIG_VFO_A:
        cmd_index = FT1000MP_NATIVE_FREQA_SET;
        break;

    case RIG_VFO_B:
        cmd_index = FT1000MP_NATIVE_FREQB_SET;
        break;

    case RIG_VFO_MEM:
        /* TODO, hint: store current memory number */
        RETURNFUNC(-RIG_ENIMPL);

    default:
        rig_debug(RIG_DEBUG_WARN, "%s: unknown VFO %0x\n", __func__, vfo);
        RETURNFUNC(-RIG_EINVAL);
    }

    /*
     * Copy native cmd freq_set to private cmd storage area
     */
    memcpy(&p->p_cmd, &ncmd[cmd_index].nseq, YAESU_CMD_LENGTH);

    // round freq to 10Hz intervals due to rig restriction
    freq = round(freq / 10.0) * 10.0;

    to_bcd(p->p_cmd, freq / 10, 8); /* store bcd format in in p_cmd */

    rig_debug(RIG_DEBUG_TRACE, "%s: freq = %"PRIfreq" Hz\n", __func__,
              (freq_t)from_bcd(p->p_cmd, 8) * 10);

    cmd = p->p_cmd;               /* get native sequence */
    write_block(&rig->state.rigport, cmd, YAESU_CMD_LENGTH);

    RETURNFUNC(RIG_OK);
}

static int ft1000mp_get_vfo_data(RIG *rig, vfo_t vfo)
{
    int cmd_index, len, retval;

    ENTERFUNC;

    if (vfo == RIG_VFO_A || vfo == RIG_VFO_B)
    {
        cmd_index = FT1000MP_NATIVE_VFO_UPDATE;
        len = 2 * FT1000MP_STATUS_UPDATE_LENGTH;
    }
    else
    {
        /* RIG_VFO_CURR or RIG_VFO_MEM */
        cmd_index = FT1000MP_NATIVE_CURR_VFO_UPDATE;
        len = FT1000MP_STATUS_UPDATE_LENGTH;
    }

    /*
     * get record from rig
     */
    retval = ft1000mp_get_update_data(rig, cmd_index, len);

    RETURNFUNC(retval);
}
/*
 * Return Freq for a given VFO
 *
 */

static int ft1000mp_get_freq(RIG *rig, vfo_t vfo, freq_t *freq)
{
    struct ft1000mp_priv_data *priv;
    unsigned char *p;
    freq_t f;
    int retval;

    ENTERFUNC;

    if (vfo == RIG_VFO_CURR)
    {
        rig_debug(RIG_DEBUG_TRACE, "%s: current_vfo=%s\n", __func__,
                  rig_strvfo(rig->state.current_vfo));
        vfo = rig->state.current_vfo;
    }

    retval = ft1000mp_get_vfo_data(rig, vfo);


    if (retval < 0)
    {
        RETURNFUNC(retval);
    }

    priv = (struct ft1000mp_priv_data *)rig->state.priv;

    if (vfo == RIG_VFO_B)
    {
        p = &priv->update_data[FT1000MP_SUMO_VFO_B_FREQ];
    }
    else
    {
        p = &priv->update_data[FT1000MP_SUMO_VFO_A_FREQ];    /* CURR_VFO has VFOA offset */
    }

    /* big endian integer, kinda */
    f = ((((((p[0] << 8) + p[1]) << 8) + p[2]) << 8) + p[3]) * 10 / 16;

    rig_debug(RIG_DEBUG_TRACE, "%s: freq = %"PRIfreq" Hz for VFO [%x]\n", __func__,
              f,
              vfo);

    *freq = f;                    /* return displayed frequency */

    RETURNFUNC(RIG_OK);
}


/*
 * set mode : eg AM, CW etc for a given VFO
 *
 */

static int ft1000mp_set_mode(RIG *rig, vfo_t vfo, rmode_t mode, pbwidth_t width)
{
    unsigned char cmd_index = 0;      /* index of sequence to send */

    ENTERFUNC;


    /* frontend sets VFO for us */

    rig_debug(RIG_DEBUG_TRACE, "%s: generic mode = %s\n", __func__,
              rig_strrmode(mode));

    if (vfo == RIG_VFO_CURR)
    {
        rig_debug(RIG_DEBUG_TRACE, "%s: current_vfo=%s\n", __func__,
                  rig_strvfo(rig->state.current_vfo));
        vfo = rig->state.current_vfo;
    }

    /*
     * translate mode from generic to ft1000mp specific
     */
    switch (mode)
    {
    case RIG_MODE_AM:
        cmd_index = FT1000MP_NATIVE_MODE_SET_AM;

        if (vfo == RIG_VFO_B) { cmd_index = FT1000MP_NATIVE_MODE_SET_AM_B; }

        break;

    case RIG_MODE_CW:
        cmd_index = FT1000MP_NATIVE_MODE_SET_CWR;

        if (vfo == RIG_VFO_B) { cmd_index = FT1000MP_NATIVE_MODE_SET_CWR_B; }

        break;

    case RIG_MODE_CWR:
        cmd_index = FT1000MP_NATIVE_MODE_SET_CW;

        if (vfo == RIG_VFO_B) { cmd_index = FT1000MP_NATIVE_MODE_SET_CW_B; }

        break;

    case RIG_MODE_USB:
        cmd_index = FT1000MP_NATIVE_MODE_SET_USB;

        if (vfo == RIG_VFO_B) { cmd_index = FT1000MP_NATIVE_MODE_SET_USB_B; }

        break;

    case RIG_MODE_LSB:
        cmd_index = FT1000MP_NATIVE_MODE_SET_LSB;

        if (vfo == RIG_VFO_B) { cmd_index = FT1000MP_NATIVE_MODE_SET_LSB_B; }

        break;

    case RIG_MODE_FM:
        cmd_index = FT1000MP_NATIVE_MODE_SET_FM;

        if (vfo == RIG_VFO_B) { cmd_index = FT1000MP_NATIVE_MODE_SET_FM_B; }

        break;

    case RIG_MODE_RTTY:
        cmd_index = FT1000MP_NATIVE_MODE_SET_RTTY_LSB;

        if (vfo == RIG_VFO_B) { cmd_index = FT1000MP_NATIVE_MODE_SET_RTTY_LSB_B; }

        break;

    case RIG_MODE_PKTUSB:
    case RIG_MODE_RTTYR:
        cmd_index = FT1000MP_NATIVE_MODE_SET_RTTY_USB;

        if (vfo == RIG_VFO_B) { cmd_index = FT1000MP_NATIVE_MODE_SET_RTTY_USB_B; }

        break;

    case RIG_MODE_PKTLSB:
        cmd_index = FT1000MP_NATIVE_MODE_SET_DATA_LSB;

        if (vfo == RIG_VFO_B) { cmd_index = FT1000MP_NATIVE_MODE_SET_DATA_LSB_B; }

        break;

    case RIG_MODE_PKTFM:
        cmd_index = FT1000MP_NATIVE_MODE_SET_DATA_FM;

        if (vfo == RIG_VFO_B) { cmd_index = FT1000MP_NATIVE_MODE_SET_FM_B; }

        break;

    default:
        RETURNFUNC(-RIG_EINVAL);         /* sorry, wrong MODE * */
    }


    /*
     * Now set width
     * FIXME: so far setting passband is buggy, only 0 is accepted
     */


    /*
     * phew! now send cmd to rig
     */
    ft1000mp_send_priv_cmd(rig, cmd_index);

    rig_debug(RIG_DEBUG_TRACE, "%s: cmd_index = %i\n", __func__, cmd_index);

    RETURNFUNC(RIG_OK);                /* good */

}


/*
 * get mode : eg AM, CW etc for a given VFO
 *
 */

static int ft1000mp_get_mode(RIG *rig, vfo_t vfo, rmode_t *mode,
                             pbwidth_t *width)
{
    struct ft1000mp_priv_data *priv;
    unsigned char mymode;         /* ft1000mp mode */
    unsigned char mymode_ext; /* ft1000mp extra mode bit mode */
    int retval;

    ENTERFUNC;

    if (vfo == RIG_VFO_CURR)
    {
        rig_debug(RIG_DEBUG_TRACE, "%s: current_vfo=%s\n", __func__,
                  rig_strvfo(rig->state.current_vfo));
        vfo = rig->state.current_vfo;
    }

    retval = ft1000mp_get_vfo_data(rig, vfo);

    if (retval < 0)
    {
        RETURNFUNC(retval);
    }

    priv = (struct ft1000mp_priv_data *)rig->state.priv;

    if (vfo == RIG_VFO_B)
    {
        mymode = priv->update_data[FT1000MP_SUMO_VFO_B_MODE];
        mymode_ext = priv->update_data[FT1000MP_SUMO_VFO_B_IF] & IF_MODE_MASK;
    }
    else
    {
        mymode = priv->update_data[FT1000MP_SUMO_VFO_A_MODE]; /* CURR_VFO is VFOA offset */
        mymode_ext = priv->update_data[FT1000MP_SUMO_VFO_A_IF] & IF_MODE_MASK;
    }


    rig_debug(RIG_DEBUG_TRACE, "%s: mymode = %x (before)\n", __func__, mymode);
    mymode &= MODE_MASK;

    rig_debug(RIG_DEBUG_TRACE, "%s: mymode = %x (after)\n", __func__, mymode);

    /*
     * translate mode from ft1000mp to generic.
     * TODO: Add DATA, and Narrow modes.  CW on LSB? -N0NB
     */
    switch (mymode)
    {
    case MODE_CW:
        *mode = mymode_ext ? RIG_MODE_CW : RIG_MODE_CWR;
        break;

    case MODE_USB:
        *mode = RIG_MODE_USB;
        break;

    case MODE_LSB:
        *mode = RIG_MODE_LSB;
        break;

    case MODE_AM:
        *mode = mymode_ext ? RIG_MODE_SAL : RIG_MODE_AM;
        break;

    case MODE_FM:
        *mode = RIG_MODE_FM;
        break;

    case MODE_RTTY:
        *mode = mymode_ext ? RIG_MODE_RTTYR : RIG_MODE_RTTY;
        break;

    case MODE_PKT:
        *mode = mymode_ext ? RIG_MODE_PKTFM : RIG_MODE_PKTLSB;
        break;

    default:
        RETURNFUNC(-RIG_EINVAL);         /* sorry, wrong mode */
        break;
    }

    rig_debug(RIG_DEBUG_TRACE, "%s: mode = %s\n", __func__, rig_strrmode(*mode));

    /* TODO: set real IF filter selection */
    *width = RIG_PASSBAND_NORMAL;

    RETURNFUNC(RIG_OK);
}


/*
 * set vfo and store requested vfo for later RIG_VFO_CURR
 * requests.
 *
 */

static int ft1000mp_set_vfo(RIG *rig, vfo_t vfo)
{
    //unsigned char cmd_index = 0;      /* index of sequence to send */

    ENTERFUNC;
    rig_debug(RIG_DEBUG_TRACE, "%s: called %s\n", __func__,
              rig_strvfo(vfo));

    /*
     * RIG_VFO_VFO/RIG_VFO_MEM are not available
     * so try to emulate them.
     * looks like there's no RIG_VFO_MEM, maybe setting mem#
     * switch to it automatically?
     */

    if (vfo == RIG_VFO_VFO)
    {
        vfo = rig->state.current_vfo;
    }

#if 0 // seems switching VFOs like this changes the frequencies in the response

    switch (vfo)
    {
    case RIG_VFO_A:
        cmd_index = FT1000MP_NATIVE_VFO_A;
        rig->state.current_vfo = vfo;       /* update active VFO */
        rig_debug(RIG_DEBUG_TRACE, "%s: vfo == RIG_VFO_A\n", __func__);
        break;

    case RIG_VFO_B:
        cmd_index = FT1000MP_NATIVE_VFO_B;
        rig->state.current_vfo = vfo;       /* update active VFO */
        rig_debug(RIG_DEBUG_TRACE, "%s: vfo == RIG_VFO_B\n", __func__);
        break;

    case RIG_VFO_CURR:
        /* do nothing, we're already at it! */
        RETURNFUNC(RIG_OK);

    default:
        rig_debug(RIG_DEBUG_VERBOSE, "%s: Unknown default VFO %d\n", __func__, vfo);
        RETURNFUNC(-RIG_EINVAL);         /* sorry, wrong VFO */
    }

    /*
     * phew! now send cmd to rig
     */
    ft1000mp_send_priv_cmd(rig, cmd_index);
#endif

    // we just store the requested vfo in our internal state
    rig->state.current_vfo = vfo;

    RETURNFUNC(RIG_OK);

}


/*
 * get vfo and store requested vfo for later RIG_VFO_CURR
 * requests.
 *
 */

static int ft1000mp_get_vfo(RIG *rig, vfo_t *vfo)
{
    struct ft1000mp_priv_data *p;
    int retval;

    ENTERFUNC;

    p = (struct ft1000mp_priv_data *)rig->state.priv;

    /* Get flags for VFO status */
    retval = ft1000mp_get_update_data(rig, FT1000MP_NATIVE_UPDATE,
                                      FT1000MP_STATUS_FLAGS_LENGTH);

    if (retval < 0)
    {
        RETURNFUNC(retval);
    }

    if (p->update_data[1] & 0x40)
    {
        *vfo = RIG_VFO_MEM;
    }
    else // we are emulating vfo status
    {
        *vfo = rig->state.current_vfo;

        if (*vfo == RIG_VFO_CURR)
        {
            rig_debug(RIG_DEBUG_TRACE, "%s: no get_vfo, defaulting to VFOA\n", __func__);
            *vfo = RIG_VFO_A;
        }
    }

#if 0
    else if (p->update_data[FT1000MP_SUMO_DISPLAYED_STATUS] & SF_VFOAB)
    {
        *vfo = rig->state.current_vfo = RIG_VFO_B;
    }
    else
    {
        *vfo = rig->state.current_vfo = RIG_VFO_A;
    }

#endif

    rig_debug(RIG_DEBUG_TRACE, "%s: vfo status = %x %x\n", __func__,
              p->update_data[0], p->update_data[1]);

    RETURNFUNC(RIG_OK);
}

static int ft1000mp_set_func(RIG *rig, vfo_t vfo, setting_t func, int status)
{
    struct ft1000mp_priv_data *priv;
    struct rig_state *rs;
    unsigned char *cmd;

    ENTERFUNC;
    rs = &rig->state;
    priv = (struct ft1000mp_priv_data *)rig->state.priv;

    switch (func)
    {
    case RIG_FUNC_RIT:
        if (status)
        {
            memcpy(&priv->p_cmd, &ncmd[FT1000MP_NATIVE_RIT_ON].nseq, YAESU_CMD_LENGTH);
        }
        else
        {
            memcpy(&priv->p_cmd, &ncmd[FT1000MP_NATIVE_RIT_OFF].nseq, YAESU_CMD_LENGTH);
        }

        cmd = priv->p_cmd;

        write_block(&rs->rigport, cmd, YAESU_CMD_LENGTH);
        RETURNFUNC(RIG_OK);

    case RIG_FUNC_XIT:
        if (status)
        {
            memcpy(&priv->p_cmd, &ncmd[FT1000MP_NATIVE_XIT_ON].nseq, YAESU_CMD_LENGTH);
        }
        else
        {
            memcpy(&priv->p_cmd, &ncmd[FT1000MP_NATIVE_XIT_OFF].nseq, YAESU_CMD_LENGTH);
        }

        cmd = priv->p_cmd;

        write_block(&rs->rigport, cmd, YAESU_CMD_LENGTH);
        RETURNFUNC(RIG_OK);

    default:
        rig_debug(RIG_DEBUG_ERR, "%s: Unsupported set_func %s", __func__,
                  rig_strfunc(func));
    }

    RETURNFUNC(-RIG_EINVAL);
}

static int ft1000mp_get_func(RIG *rig, vfo_t vfo, setting_t func, int *status)
{
    int retval;
    struct ft1000mp_priv_data *priv;
    unsigned char *p;

    ENTERFUNC;
    priv = (struct ft1000mp_priv_data *)rig->state.priv;

    if (!status)
    {
        RETURNFUNC(-RIG_EINVAL);
    }

    switch (func)
    {
    case RIG_FUNC_RIT:
    {
        retval = ft1000mp_get_vfo_data(rig, vfo);

        if (retval < 0)
        {
            RETURNFUNC(retval);
        }

        if (vfo == RIG_VFO_B)
        {
            p = &priv->update_data[FT1000MP_SUMO_VFO_B_MEM];
        }
        else
        {
            p = &priv->update_data[FT1000MP_SUMO_VFO_A_MEM];    /* CURR_VFO has VFOA offset */
        }

        *status = (*p & 2) ? 1 : 0;
        RETURNFUNC(RIG_OK);
    }


    case RIG_FUNC_XIT:
    {
        retval = ft1000mp_get_vfo_data(rig, vfo);

        if (retval < 0)
        {
            RETURNFUNC(retval);
        }

        if (vfo == RIG_VFO_B)
        {
            p = &priv->update_data[FT1000MP_SUMO_VFO_B_MEM];
        }
        else
        {
            p = &priv->update_data[FT1000MP_SUMO_VFO_A_MEM];    /* CURR_VFO has VFOA offset */
        }

        *status = (*p & 1) ? 1 : 0;
        RETURNFUNC(RIG_OK);
    }



    default:
        rig_debug(RIG_DEBUG_ERR, "%s: Unsupported get_func %s", __func__,
                  rig_strfunc(func));
        RETURNFUNC(-RIG_EINVAL);
    }

    RETURNFUNC(-RIG_EINVAL);
}

/*
 * set_rit only support vfo = RIG_VFO_CURR
 */
static int ft1000mp_set_rit(RIG *rig, vfo_t vfo, shortfreq_t rit)
{
    ENTERFUNC;

    if (rit != 0)
    {
        ft1000mp_set_func(rig, vfo, RIG_FUNC_RIT, 1);
    }

    RETURNFUNC(ft1000mp_set_rxit(rig, vfo, rit));
}

static int ft1000mp_set_xit(RIG *rig, vfo_t vfo, shortfreq_t rit)
{
    ENTERFUNC;
    RETURNFUNC(ft1000mp_set_rxit(rig, vfo, rit));
}

static int ft1000mp_set_rxit(RIG *rig, vfo_t vfo, shortfreq_t rit)
{
    struct rig_state *rs;
    struct ft1000mp_priv_data *priv;
    unsigned char *cmd;           /* points to sequence to send */
    int direction = 0;

    ENTERFUNC;

    rs = &rig->state;
    priv = (struct ft1000mp_priv_data *)rs->priv;

    rig_debug(RIG_DEBUG_TRACE, "%s: requested freq = %d Hz\n", __func__, (int)rit);

    /*
     * Copy native cmd freq_set to private cmd storage area
     */
    memcpy(&priv->p_cmd, &ncmd[FT1000MP_NATIVE_RXIT_SET].nseq, YAESU_CMD_LENGTH);

    if (rit < 0)
    {
        direction = 0xff;
        rit = -rit;
    }

    unsigned char rit_freq[10];

    // yes, it really is this nasty and complicated!

    to_bcd_be(rit_freq, (rit - (rit / 1000) * 1000) / 10, 2);
    priv->p_cmd[0] = rit_freq[0];   // 10 hz

    to_bcd_be(rit_freq, rit / 1000, 2);
    priv->p_cmd[1] = rit_freq[0];   // Khz

    priv->p_cmd[2] = direction;

    cmd = priv->p_cmd;               /* get native sequence */
    write_block(&rs->rigport, cmd, YAESU_CMD_LENGTH);

    RETURNFUNC(RIG_OK);
}


/*
 * Return RIT for a given VFO
 *
 */

static int ft1000mp_get_rxit(RIG *rig, vfo_t vfo, shortfreq_t *rit)
{
    struct ft1000mp_priv_data *priv;
    unsigned char *p;
    shortfreq_t f;
    int retval;

    ENTERFUNC;

    priv = (struct ft1000mp_priv_data *)rig->state.priv;

    retval = ft1000mp_get_vfo_data(rig, vfo);

    if (retval < 0)
    {
        RETURNFUNC(retval);
    }

    if (vfo == RIG_VFO_B)
    {
        p = &priv->update_data[FT1000MP_SUMO_VFO_B_CLAR];
    }
    else
    {
        p = &priv->update_data[FT1000MP_SUMO_VFO_A_CLAR];    /* CURR_VFO has VFOA offset */
    }

    f = (p[0] << 8) + p[1];

    if (p[0] & 0x80)
    {
        f = ~(f - 1) & 0x7fff; // two's complement
        f = -f;
    }

    f = f * 10 / 16;

    rig_debug(RIG_DEBUG_TRACE, "%s: freq = %d Hz for VFO [%s]\n", __func__, (int)f,
              rig_strvfo(vfo));

    *rit = f;     /* return displayed frequency */

    RETURNFUNC(RIG_OK);
}


static int ft1000mp_get_level(RIG *rig, vfo_t vfo, setting_t level,
                              value_t *val)
{
    struct ft1000mp_priv_data *priv;
    struct rig_state *rs;
    unsigned char lvl_data[YAESU_CMD_LENGTH];
    int m;
    int retval;
    int retry = rig->state.rigport.retry;

    ENTERFUNC;
    rs = &rig->state;
    priv = (struct ft1000mp_priv_data *)rs->priv;


    /* Optimize:
     *   sort the switch cases with the most frequent first
     */
    switch (level)
    {
    case RIG_LEVEL_RAWSTR:
        if (vfo == RIG_VFO_CURR)
        {
            vfo = rig->state.current_vfo;
        }

        m = vfo == RIG_VFO_B ? 0x01 : 0x00;
        break;

    case RIG_LEVEL_RFPOWER:
        m = 0x80;
        break;

    case RIG_LEVEL_ALC:
        m = 0x81;
        break;

    case RIG_LEVEL_COMP:
        m = 0x83;
        break;

    case RIG_LEVEL_SWR:
        m = 0x85;
        break;

    case RIG_LEVEL_MICGAIN: /* not sure ... */
        m = 0x86;
        break;

    case RIG_LEVEL_CWPITCH:
        m = 0xf1;
        break;

    case RIG_LEVEL_IF:  /* not sure ... */
        m = 0xf3;
        break;

    default:
        rig_debug(RIG_DEBUG_ERR, "%s: unsupported get_level %s", __func__,
                  rig_strlevel(level));
        RETURNFUNC(-RIG_EINVAL);
    }

    memset(&priv->p_cmd, m, YAESU_CMD_LENGTH - 1);
    priv->p_cmd[4] = 0xf7;

    do
    {
        write_block(&rs->rigport, priv->p_cmd, YAESU_CMD_LENGTH);

        retval = read_block(&rs->rigport, lvl_data, YAESU_CMD_LENGTH);
    }
    while (retry-- && retval == -RIG_ETIMEOUT);

    if (retval != YAESU_CMD_LENGTH)
    {
        rig_debug(RIG_DEBUG_ERR, "%s: ack NG %d", __func__, retval);
        RETURNFUNC(retval);
    }

    switch (level)
    {
    case RIG_LEVEL_RAWSTR:
        val->i = lvl_data[0];
        break;

    default:
        if (RIG_LEVEL_IS_FLOAT(level))
        {
            val->f = (float)lvl_data[0] / 255;
        }
        else
        {
            val->i = lvl_data[0];
        }
    }

    rig_debug(RIG_DEBUG_TRACE, "%s: %d %d %f\n", __func__, lvl_data[0],
              val->i, val->f);

    RETURNFUNC(RIG_OK);
}

static int ft1000mp_set_ptt(RIG *rig, vfo_t vfo, ptt_t ptt)
{
    unsigned char cmd_index;      /* index of sequence to send */

    ENTERFUNC;
    rig_debug(RIG_DEBUG_TRACE, "%s: called %d\n", __func__, ptt);

    cmd_index = ptt ? FT1000MP_NATIVE_PTT_ON : FT1000MP_NATIVE_PTT_OFF;

    ft1000mp_send_priv_cmd(rig, cmd_index);

    RETURNFUNC(RIG_OK);

}

/*
 * private helper function. Retrieves update data from rig.
 * using buffer indicated in *priv struct.
 * Extended to be command agnostic as 1000mp has several ways to
 * get data and several ways to return it.
 *
 * need to use this when doing ft1000mp_get_* stuff
 *
 * Variables:  ci = command index, rl = read length of returned data
 *
 */

static int ft1000mp_get_update_data(RIG *rig, unsigned char ci,
                                    unsigned char rl)
{
    struct ft1000mp_priv_data *p;
    int n;                        /* for read_  */

    ENTERFUNC;

    p = (struct ft1000mp_priv_data *)rig->state.priv;

    // timeout retries are done in read_block now
    // based on rig backed retry value
    /* send UPDATE command to fetch data*/
    ft1000mp_send_priv_cmd(rig, ci);

    n = read_block(&rig->state.rigport, p->update_data, rl);

    if (n == -RIG_ETIMEOUT)
    {
        rig_debug(RIG_DEBUG_TRACE, "%s: Timeout\n", __func__);
    }

    RETURNFUNC(n);
}


/*
 * private helper function to send a private command
 * sequence . Must only be complete sequences.
 * TODO: place variant of this in yaesu.c
 *
 */

static int ft1000mp_send_priv_cmd(RIG *rig, unsigned char ci)
{
    ENTERFUNC;

    if (! ncmd[ci].ncomp)
    {
        rig_debug(RIG_DEBUG_TRACE, "%s: attempt to send incomplete sequence\n",
                  __func__);
        RETURNFUNC(-RIG_EINVAL);
    }

    write_block(&rig->state.rigport, ncmd[ci].nseq, YAESU_CMD_LENGTH);

    RETURNFUNC(RIG_OK);

}

static int ft1000mp_set_split_vfo(RIG *rig, vfo_t vfo, split_t split,
                                  vfo_t tx_vfo)
{
    unsigned char cmd_index = 0;      /* index of sequence to send */

    ENTERFUNC;
    rig_debug(RIG_DEBUG_TRACE, "%s called rx_vfo=%s, tx_vfo=%s\n", __func__,
              rig_strvfo(vfo), rig_strvfo(tx_vfo));

    switch (split)
    {
    case RIG_SPLIT_OFF:
        cmd_index = FT1000MP_NATIVE_SPLIT_OFF;
        break;

    case RIG_SPLIT_ON:
        cmd_index = FT1000MP_NATIVE_SPLIT_ON;
        break;

    default:
        rig_debug(RIG_DEBUG_VERBOSE, "%s: Unknown split value = %d\n", __func__, split);
        RETURNFUNC(-RIG_EINVAL);         /* sorry, wrong VFO */
    }

    // manual says VFO_A=Tx and VFO_B=Rx but testing shows otherwise
    rig->state.current_vfo = RIG_VFO_A;
    rig->state.rx_vfo = RIG_VFO_B;
    rig->state.tx_vfo = RIG_VFO_B;
    ft1000mp_send_priv_cmd(rig, cmd_index);

    RETURNFUNC(RIG_OK);
}

/*
 * get split
 *
 */

static int ft1000mp_get_split_vfo(RIG *rig, vfo_t vfo, split_t *split,
                                  vfo_t *tx_vfo)
{
    struct ft1000mp_priv_data *p;
    int retval;

    ENTERFUNC;

    p = (struct ft1000mp_priv_data *)rig->state.priv;

    /* Get flags for split status */
    retval = ft1000mp_get_update_data(rig, FT1000MP_NATIVE_UPDATE,
                                      FT1000MP_STATUS_FLAGS_LENGTH);

    if (retval < 0)
    {
        RETURNFUNC(retval);
    }

    if (p->update_data[0] & 0x01)
    {
        *tx_vfo = RIG_VFO_B;
        *split = RIG_SPLIT_ON;
    }
    else
    {
        *tx_vfo = RIG_VFO_A;
        *split = RIG_SPLIT_OFF;
    }

    RETURNFUNC(RIG_OK);
}

static int ft1000mp_set_split_freq(RIG *rig, vfo_t vfo, freq_t tx_freq)
{
    ENTERFUNC;
    int retval = rig_set_split_vfo(rig, vfo, RIG_SPLIT_ON, RIG_VFO_B);

    if (retval != RIG_OK) { RETURNFUNC(retval); }

    RETURNFUNC(ft1000mp_set_freq(rig, RIG_VFO_B, tx_freq));
}

static int ft1000mp_set_split_mode(RIG *rig, vfo_t vfo, rmode_t tx_mode,
                                   pbwidth_t tx_width)
{
    int retval;
    retval = rig_set_mode(rig, RIG_VFO_B, tx_mode, tx_width);
    RETURNFUNC(retval);
}

static int ft1000mp_get_split_mode(RIG *rig, vfo_t vfo, rmode_t *tx_mode,
                                   pbwidth_t *tx_width)
{
    int retval;
    retval = rig_get_mode(rig, RIG_VFO_B, tx_mode, tx_width);
    RETURNFUNC(retval);
}

static int ft1000mp_set_split_freq_mode(RIG *rig, vfo_t vfo, freq_t freq,
                                        rmode_t mode, pbwidth_t width)
{
    int retval;
    retval = rig_set_mode(rig, RIG_VFO_B, mode, width);

    if (retval != RIG_OK)
    {
        rig_debug(RIG_DEBUG_ERR, "%s: rig_set_mode failed: %s\n", __func__,
                  rigerror(retval));
        RETURNFUNC(retval);
    }

    retval = ft1000mp_set_split_freq(rig, vfo, freq);

    if (retval == RIG_OK)
    {
        rig->state.cache.freqMainB = freq;
        rig->state.cache.modeMainB = mode;
    }

    RETURNFUNC(retval);
}

static int ft1000mp_get_split_freq_mode(RIG *rig, vfo_t vfo, freq_t *freq,
                                        rmode_t *mode, pbwidth_t *width)
{
    int retval;
    retval = rig_get_mode(rig, RIG_VFO_B, mode, width);

    if (retval != RIG_OK)
    {
        rig_debug(RIG_DEBUG_ERR, "%s: rig_set_mode failed: %s\n", __func__,
                  rigerror(retval));
        RETURNFUNC(retval);
    }

    retval = ft1000mp_get_split_freq(rig, vfo, freq);

    if (retval == RIG_OK)
    {
        rig->state.cache.freqMainB = *freq;
        rig->state.cache.modeMainB = *mode;
    }

    RETURNFUNC(retval);
}

static int ft1000mp_get_split_freq(RIG *rig, vfo_t vfo, freq_t *tx_freq)
{
    ENTERFUNC;
    RETURNFUNC(ft1000mp_get_freq(rig, RIG_VFO_B, tx_freq));
}
