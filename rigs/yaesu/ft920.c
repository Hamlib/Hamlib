/*
 * hamlib - (C) Frank Singleton 2000 (javabear at users.sourceforge.net)
 *
 * ft920.c - (C) Frank Singleton 2000 (javabear at users.sourceforge.net)
 *           (C) Nate Bargmann 2002-2005 (n0nb at arrl.net)
 *           (C) Stephane Fillod 2002-2010 (fillods at users.sourceforge.net)
 *
 * This shared library provides an API for communicating
 * via serial interface to an FT-920 using the "CAT" interface
 * Documentation can be found online at:
 * http://www.yaesu.com/amateur/pdf/manuals/ft_920.pdf
 * pages 86 to 90
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

#include <stdlib.h>
#include <string.h> /* String function definitions */

#include "hamlib/rig.h"
#include "bandplan.h"
#include "serial.h"
#include "misc.h"
#include "yaesu.h"
#include "ft920.h"


/*
 * Functions considered to be Stable (2010-01-29):
 * set_vfo
 * get_vfo
 * set_freq
 * get_freq
 * set_mode
 * get_mode
 * set_split
 * get_split
 * set_split_freq
 * get_split_freq
 * set_split_mode
 * get_split_mode
 * set_rit
 * get_rit
 * set_xit
 * get_xit
 * set_ptt
 * get_ptt
 */

/*
 * Native FT920 functions. More to come :-)
 *
 */

enum ft920_native_cmd_e
{
    FT920_NATIVE_SPLIT_OFF = 0,
    FT920_NATIVE_SPLIT_ON,
    FT920_NATIVE_RECALL_MEM,
    FT920_NATIVE_VFO_TO_MEM,
    FT920_NATIVE_VFO_A,
    FT920_NATIVE_VFO_B,
    FT920_NATIVE_MEM_TO_VFO,
    FT920_NATIVE_CLARIFIER_OPS,
    FT920_NATIVE_VFO_A_FREQ_SET,
    FT920_NATIVE_MODE_SET,
    FT920_NATIVE_PACING,
    FT920_NATIVE_PTT_OFF,
    FT920_NATIVE_PTT_ON,
    FT920_NATIVE_MEM_CHNL,
    FT920_NATIVE_OP_DATA,
    FT920_NATIVE_VFO_DATA,
    FT920_NATIVE_MEM_CHNL_DATA,
    FT920_NATIVE_TUNER_BYPASS,
    FT920_NATIVE_TUNER_INLINE,
    FT920_NATIVE_TUNER_START,
    FT920_NATIVE_VFO_B_FREQ_SET,
    FT920_NATIVE_VFO_A_PASSBAND_WIDE,
    FT920_NATIVE_VFO_A_PASSBAND_NAR,
    FT920_NATIVE_VFO_B_PASSBAND_WIDE,
    FT920_NATIVE_VFO_B_PASSBAND_NAR,
    FT920_NATIVE_STATUS_FLAGS,
    FT920_NATIVE_SIZE   /* end marker, value indicates number of */
    /* native cmd entries */
};

/*
 * Internal MODES - when setting modes via FT920_NATIVE_MODE_SET
 *
 */

/* VFO A */
#define MODE_SET_A_LSB      0x00
#define MODE_SET_A_USB      0x01
#define MODE_SET_A_CW_U     0x02
#define MODE_SET_A_CW_L     0x03
#define MODE_SET_A_AM_W     0x04
#define MODE_SET_A_AM_N     0x05
#define MODE_SET_A_FM_W     0x06
#define MODE_SET_A_FM_N     0x07
#define MODE_SET_A_DATA_L   0x08
#define MODE_SET_A_DATA_U   0x0a
#define MODE_SET_A_DATA_F   0x0b

/* VFO B */
#define MODE_SET_B_LSB      0x80
#define MODE_SET_B_USB      0x81
#define MODE_SET_B_CW_U     0x82
#define MODE_SET_B_CW_L     0x83
#define MODE_SET_B_AM_W     0x84
#define MODE_SET_B_AM_N     0x85
#define MODE_SET_B_FM_W     0x86
#define MODE_SET_B_FM_N     0x87
#define MODE_SET_B_DATA_L   0x88
#define MODE_SET_B_DATA_U   0x8a
#define MODE_SET_B_DATA_F   0x8b


/*
 * Internal Clarifier parms - when setting clarifier via
 * FT920_NATIVE_CLARIFIER_OPS
 *
 */

/* P1 values */
#define CLAR_RX_OFF     0x00
#define CLAR_RX_ON      0x01
#define CLAR_TX_OFF     0x80
#define CLAR_TX_ON      0x81
#define CLAR_SET_FREQ   0xff

/* P2 values */
#define CLAR_OFFSET_PLUS    0x00
#define CLAR_OFFSET_MINUS   0xff


/* Tuner status values used to set the
 * tuner state and indicate tuner status.
 */
#define TUNER_BYPASS    0
#define TUNER_INLINE    1
#define TUNER_TUNING    2

/*
 * Local VFO CMD's, according to spec
 *
 */

//#define FT920_VFO_A   0x00
//#define FT920_VFO_B   0x01


/*
 * Some useful offsets in the status update flags (offset)
 * SUMO--Status Update Memory Offset?
 *
 * SF_ bit tests are now grouped with flag bytes for ease of reference
 *
 * FIXME: complete flags and bits
 *
 * CAT command 0xFA, P1 = 01 requests the FT-920 to return its status flags.
 * These flags consist of 8 bytes and are documented in the FT-920 manual
 * on page 89.
 *
 */

#define FT920_SUMO_DISPLAYED_STATUS_0   0x00    /* Status flag byte 0 */
#define SF_VFOA     0x00    /* bits 0 & 1, VFO A TX/RX == 0 */
#define SF_SPLITA   (1<<0)  /* Split operation with VFO-B on TX */
#define SF_SPLITB   (1<<1)  /* Split operation with VFO-B on RX */
#define SF_VFOB     (SF_SPLITA|SF_SPLITB)   /* bits 0 & 1, VFO B TX/RX  == 3 */
#define SF_TUNER_TUNE   (1<<2)  /* Antenna tuner On and Tuning for match*/
#define SF_PTT_OFF  (0<<7)  /* Receive mode (PTT line open) */
#define SF_PTT_ON   (1<<7)  /* Transmission in progress (PTT line grounded) */
#define SF_PTT_MASK (SF_PTT_ON)

#define FT920_SUMO_DISPLAYED_STATUS_1   0x01    /* Status flag byte 1 */
#define SF_QMB      (1<<3)  /* Quick Memory Bank (QMB) selected */
#define SF_MT       (1<<4)  /* Memory Tuning in progress */
#define SF_VFO      (1<<5)  /* VFO operation selected */
#define SF_MR       (1<<6)  /* Memory Mode selected */
#define SF_GC       (1<<7)  /* General Coverage Reception selected */
#define SF_VFO_MASK (SF_QMB|SF_MT|SF_VFO|SF_MR)

#define FT920_SUMO_DISPLAYED_STATUS_2   0x02    /* Status flag byte 2 */
#define SF_TUNER_INLINE (1<<1)  /* Antenna tuner is inline or bypass */
#define SF_VFOB_LOCK    (1<<2)  /* VFO B tuning lock status */
#define SF_VFOA_LOCK    (1<<3)  /* VFO A tuning lock status */

/*
 * Offsets for VFO record retrieved via 0x10 P1 = 02, 03
 *
 * The FT-920 returns frequency and mode data via three separate commands.
 * CAT command 0x10, P1 = 02 returns the current main and sub displays' data (28 bytes)
 * CAT command 0x10, P1 = 03 returns VFO A data and the sub display data (sub display is always VFO B) (28 bytes)
 * CAT command 0x10, P1 = 04, P4 = 0x00-0x89 returns memory channel data (14 bytes)
 * In all cases the format is (from the FT-920 manual page 90):
 *
 * Offset   Value
 * 0x00     Band Selection      (not documented!)
 * 0x01     Operating Frequency (Hex value of display--Not BCD!)
 * 0x05     Clarifier Offset    (Hex value)
 * 0x07     Mode Data
 * 0x08     Flag
 * 0x09     Filter Data 1
 * 0x0a     Filter Data 2
 * 0x0b     CTCSS Encoder Data
 * 0x0c     CTCSS Decoder Data
 * 0x0d     Memory recall Flag
 *
 * Memory Channel data has the same layout and offsets
 * VFO B data has the same layout, but the offset starts at 0x0e and
 * continues through 0x1b
 *
 */

#define FT920_SUMO_DISPLAYED_FREQ   0x01    /* Current main display, can be VFO A, Memory data, Memory tune */
#define FT920_SUMO_VFO_A_FREQ       0x01    /* VFO A frequency, not necessarily currently displayed! */
#define FT920_SUMO_DISPLAYED_CLAR   0x05    /* RIT/XIT offset -- current display */
#define FT920_SUMO_VFO_A_CLAR       0x05    /* RIT/XIT offset -- VFO A */
#define FT920_SUMO_DISPLAYED_MODE   0x07    /* Current main display mode */
#define FT920_SUMO_VFO_A_MODE       0x07    /* VFO A mode, not necessarily currently displayed! */
#define FT920_SUMO_VFO_B_FREQ       0x0f    /* Current sub display && VFO B */
#define FT920_SUMO_VFO_B_CLAR       0x13    /* RIT/XIT offset -- VFO B */
#define FT920_SUMO_VFO_B_MODE       0x15    /* Current sub display && VFO B */

/*
 * Mode Bitmap from offset 0x07 or 0x16 in VFO Record.
 * Bits 5 and 6 ignored
 * used when READING modes from FT-920
 *
 */

#define MODE_LSB        0x00
#define MODE_CW_L       0x01    /* CW listening on LSB */
#define MODE_AM         0x02
#define MODE_FM         0x03
#define MODE_DATA_L     0x04    /* DATA on LSB */
#define MODE_DATA_U     0x05    /* DATA on USB (who does that? :) */
#define MODE_DATA_F     0x06    /* DATA on FM */
#define MODE_USB        0x40
#define MODE_CW_U       0x41    /* CW listening on USB */
/* Narrow filter selected */
#define MODE_LSBN       0x80    /* Not sure this actually exists */
#define MODE_CW_LN      0x81
#define MODE_AMN        0x82
#define MODE_FMN        0x83
#define MODE_DATA_LN    0x84
#define MODE_DATA_UN    0x85
#define MODE_DATA_FN    0x86
#define MODE_USBN       0xc0    /* Not sure this actually exists */
#define MODE_CW_UN      0xc1

/* All relevant bits */
#define MODE_MASK       0xc7


/*
 * Command string parameter offsets
 */

#define P1  3
#define P2  2
#define P3  1
#define P4  0


/*
 * API local implementation
 *
 */

static int ft920_init(RIG *rig);
static int ft920_cleanup(RIG *rig);
static int ft920_open(RIG *rig);
static int ft920_close(RIG *rig);

static int ft920_set_freq(RIG *rig, vfo_t vfo, freq_t freq);
static int ft920_get_freq(RIG *rig, vfo_t vfo, freq_t *freq);

static int ft920_set_mode(RIG *rig, vfo_t vfo, rmode_t mode, pbwidth_t width);
static int ft920_get_mode(RIG *rig, vfo_t vfo, rmode_t *mode, pbwidth_t *width);

static int ft920_set_vfo(RIG *rig, vfo_t vfo);
static int ft920_get_vfo(RIG *rig, vfo_t *vfo);

static int ft920_set_split_vfo(RIG *rig, vfo_t vfo, split_t split,
                               vfo_t tx_vfo);
static int ft920_get_split_vfo(RIG *rig, vfo_t vfo, split_t *split,
                               vfo_t *tx_vfo);

static int ft920_set_split_freq(RIG *rig, vfo_t vfo, freq_t tx_freq);
static int ft920_get_split_freq(RIG *rig, vfo_t vfo, freq_t *tx_freq);

static int ft920_set_split_mode(RIG *rig, vfo_t vfo, rmode_t tx_mode,
                                pbwidth_t tx_width);
static int ft920_get_split_mode(RIG *rig, vfo_t vfo, rmode_t *tx_mode,
                                pbwidth_t *tx_width);

static int ft920_set_rit(RIG *rig, vfo_t vfo, shortfreq_t rit);
static int ft920_get_rit(RIG *rig, vfo_t vfo, shortfreq_t *rit);

static int ft920_set_xit(RIG *rig, vfo_t vfo, shortfreq_t xit);
static int ft920_get_xit(RIG *rig, vfo_t vfo, shortfreq_t *xit);

/* not documented in my FT-920 manual, but it works! - N0NB */
static int ft920_set_ptt(RIG *rig, vfo_t vfo, ptt_t ptt);
static int ft920_get_ptt(RIG *rig, vfo_t vfo, ptt_t *ptt);

static int ft920_set_func(RIG *rig, vfo_t vfo, setting_t func, int status);
static int ft920_get_func(RIG *rig, vfo_t vfo, setting_t func, int *status);


/* Private helper function prototypes */

static int ft920_get_update_data(RIG *rig, unsigned char ci, unsigned char rl);
static int ft920_send_static_cmd(RIG *rig, unsigned char ci);
static int ft920_send_dynamic_cmd(RIG *rig, unsigned char ci, unsigned char p1,
                                  unsigned char p2, unsigned char p3, unsigned char p4);
static int ft920_send_dial_freq(RIG *rig, unsigned char ci, freq_t freq);
static int ft920_send_rit_freq(RIG *rig, unsigned char ci, shortfreq_t rit);

/*
 * Native ft920 cmd set prototypes. These are READ ONLY as each
 * rig instance will copy from these and modify if required.
 * Complete sequences (1) can be read and used directly as a cmd sequence.
 * Incomplete sequences (0) must be completed with extra parameters
 * eg: mem number, or freq etc..
 *
 * TODO: Shorten this static array with parameter substitution -N0NB
 *
 */

static const yaesu_cmd_set_t ncmd[] =
{
    { 1, { 0x00, 0x00, 0x00, 0x00, 0x01 } },    /* split = off */
    { 1, { 0x00, 0x00, 0x00, 0x01, 0x01 } },    /* split = on */
    { 0, { 0x00, 0x00, 0x00, 0x00, 0x02 } },    /* recall memory */
    { 0, { 0x00, 0x00, 0x00, 0x00, 0x03 } },    /* memory operations */
    { 1, { 0x00, 0x00, 0x00, 0x00, 0x05 } },    /* select vfo A */
    { 1, { 0x00, 0x00, 0x00, 0x01, 0x05 } },    /* select vfo B */
    { 0, { 0x00, 0x00, 0x00, 0x00, 0x06 } },    /* copy memory data to vfo A */
    { 0, { 0x00, 0x00, 0x00, 0x00, 0x09 } },    /* clarifier operations */
    { 0, { 0x00, 0x00, 0x00, 0x00, 0x0a } },    /* set vfo A freq */
    { 0, { 0x00, 0x00, 0x00, 0x00, 0x0c } },    /* mode set */
    { 0, { 0x00, 0x00, 0x00, 0x00, 0x0e } },    /* update interval/pacing */
    { 1, { 0x00, 0x00, 0x00, 0x00, 0x0f } },    /* PTT off */
    { 1, { 0x00, 0x00, 0x00, 0x01, 0x0f } },    /* PTT on */
    { 1, { 0x00, 0x00, 0x00, 0x01, 0x10 } },    /* Status Update Data--Memory Channel Number (1 byte) */
    { 1, { 0x00, 0x00, 0x00, 0x02, 0x10 } },    /* Status Update Data--Current operating data for VFO/Memory (28 bytes) */
    { 1, { 0x00, 0x00, 0x00, 0x03, 0x10 } },    /* Status Update DATA--VFO A and B Data (28 bytes) */
    { 0, { 0x00, 0x00, 0x00, 0x04, 0x10 } },    /* Status Update Data--Memory Channel Data (14 bytes) P4 = 0x00-0x89 Memory Channel Number */
    { 1, { 0x00, 0x00, 0x00, 0x00, 0x81 } },    /* Tuner bypass */
    { 1, { 0x00, 0x00, 0x00, 0x01, 0x81 } },    /* Tuner inline */
    { 1, { 0x00, 0x00, 0x00, 0x00, 0x82 } },    /* Tuner start tuning for match */
    { 0, { 0x00, 0x00, 0x00, 0x00, 0x8a } },    /* set vfo B frequency */
    { 1, { 0x00, 0x00, 0x00, 0x00, 0x8c } },    /* VFO A wide filter */
    { 1, { 0x00, 0x00, 0x00, 0x02, 0x8c } },    /* VFO A narrow filter */
    { 1, { 0x00, 0x00, 0x00, 0x80, 0x8c } },    /* VFO B wide filter */
    { 1, { 0x00, 0x00, 0x00, 0x82, 0x8c } },    /* VFO B narrow filter */
    { 1, { 0x00, 0x00, 0x00, 0x01, 0xFA } },    /* Read status flags */
    /*  { 0, { 0x00, 0x00, 0x00, 0x00, 0x70 } }, */ /* keyer commands */
};


/*
 * future - private data
 *
 * FIXME: Does this need to be exposed to the application/frontend through
 * ft920_caps.priv?  I'm guessing not as it's private to the backend.  -N0NB
 */

struct ft920_priv_data
{
    unsigned char pacing;                       /* pacing value */
    vfo_t current_vfo;                          /* active VFO from last cmd */
    vfo_t split_vfo;                            /* TX VFO in split mode */
    split_t split;                              /* split active or not */
    unsigned char
    p_cmd[YAESU_CMD_LENGTH];      /* private copy of 1 constructed CAT cmd */
    unsigned char
    update_data[FT920_VFO_DATA_LENGTH];   /* returned data--max value, some are less */
};

/*
 * ft920 rigs capabilities.
 * Also this struct is READONLY!
 *
 */

const struct rig_caps ft920_caps =
{
    RIG_MODEL(RIG_MODEL_FT920),
    .model_name =       "FT-920",
    .mfg_name =         "Yaesu",
    .version =          "20220529.0",           /* YYYYMMDD */
    .copyright =        "LGPL",
    .status =           RIG_STATUS_STABLE,
    .rig_type =         RIG_TYPE_TRANSCEIVER,
    .ptt_type =         RIG_PTT_RIG,
    .dcd_type =         RIG_DCD_NONE,
    .port_type =        RIG_PORT_SERIAL,
    .serial_rate_min =  4800,
    .serial_rate_max =  4800,
    .serial_data_bits = 8,
    .serial_stop_bits = 2,
    .serial_parity =    RIG_PARITY_NONE,
    .serial_handshake = RIG_HANDSHAKE_NONE,
    .write_delay =      FT920_WRITE_DELAY,
    .post_write_delay = FT920_POST_WRITE_DELAY,
    .timeout =          2000,
    .retry =            0,
    .has_get_func =     FT920_FUNC_ALL,
    .has_set_func =     RIG_FUNC_TUNER,
    .has_get_level =    RIG_LEVEL_BAND_SELECT,
    .has_set_level =    RIG_LEVEL_BAND_SELECT,
    .has_get_parm =     RIG_PARM_NONE,
    .has_set_parm =     RIG_PARM_NONE,
    .level_gran =
    {
#include "level_gran_yaesu.h"
    },
    .ctcss_list =       NULL,
    .dcs_list =         NULL,
    .preamp =           { RIG_DBLST_END, },
    .attenuator =       { RIG_DBLST_END, },
    .max_rit =          Hz(9999),
    .max_xit =          Hz(9999),
    .max_ifshift =      Hz(0),
    .announces =        RIG_ANN_NONE,
    .vfo_ops =          RIG_OP_NONE,
    .scan_ops =         RIG_SCAN_NONE,
    .targetable_vfo =   RIG_TARGETABLE_ALL,
    .transceive =       RIG_TRN_OFF,        /* Yaesus have to be polled, sigh */
    .bank_qty =         0,
    .chan_desc_sz =     0,
    .chan_list =        { RIG_CHAN_END, },  /* FIXME: memory channel list: 122 (!) */

    .rx_range_list1 = {
        {kHz(100), MHz(30), FT920_ALL_RX_MODES, -1, -1, FT920_VFO_ALL, FT920_ANTS}, /* General coverage + ham */
        {MHz(48), MHz(56), FT920_ALL_RX_MODES, -1, -1, FT920_VFO_ALL, FT920_ANTS},  /* 6m! */
        RIG_FRNG_END,
    },  /* FIXME:  Are these the correct Region 1 values? */

    .tx_range_list1 = {
        FRQ_RNG_HF(1, FT920_OTHER_TX_MODES, W(5), W(100), FT920_VFO_ALL, FT920_ANTS),
        FRQ_RNG_HF(1, FT920_AM_TX_MODES, W(2), W(25), FT920_VFO_ALL, FT920_ANTS),   /* AM class */

        FRQ_RNG_6m(1, FT920_OTHER_TX_MODES, W(5), W(100), FT920_VFO_ALL, FT920_ANTS),
        FRQ_RNG_6m(1, FT920_AM_TX_MODES, W(2), W(25), FT920_VFO_ALL, FT920_ANTS),   /* AM class */
        RIG_FRNG_END,
    },

    .rx_range_list2 = {
        {kHz(100), MHz(30), FT920_ALL_RX_MODES, -1, -1, FT920_VFO_ALL, FT920_ANTS},
        {MHz(48), MHz(56), FT920_ALL_RX_MODES, -1, -1, FT920_VFO_ALL, FT920_ANTS},
        RIG_FRNG_END,
    },

    .tx_range_list2 = {
        FRQ_RNG_HF(2, FT920_OTHER_TX_MODES, W(5), W(100), FT920_VFO_ALL, FT920_ANTS),
        FRQ_RNG_HF(2, FT920_AM_TX_MODES, W(2), W(25), FT920_VFO_ALL, FT920_ANTS),   /* AM class */

        FRQ_RNG_6m(2, FT920_OTHER_TX_MODES, W(5), W(100), FT920_VFO_ALL, FT920_ANTS),
        FRQ_RNG_6m(2, FT920_AM_TX_MODES, W(2), W(25), FT920_VFO_ALL, FT920_ANTS),   /* AM class */
        RIG_FRNG_END,
    },

    .tuning_steps = {
        {FT920_SSB_CW_RX_MODES, Hz(10)},    /* Normal */
        {FT920_SSB_CW_RX_MODES, Hz(100)},   /* Fast */

        {FT920_AM_RX_MODES,     Hz(100)},   /* Normal */
        {FT920_AM_RX_MODES,     kHz(1)},    /* Fast */

        {FT920_FM_RX_MODES,     Hz(100)},   /* Normal */
        {FT920_FM_RX_MODES,     kHz(1)},    /* Fast */

        RIG_TS_END,

        /*
         * The FT-920 has a Fine tuning step which increments in 1 Hz steps
         * for SSB_CW_RX_MODES, and 10 Hz steps for AM_RX_MODES and
         * FM_RX_MODES.  It doesn't appear that anything finer than 10 Hz
         * is available through the CAT interface, however. -N0NB
         *
         */
    },

    /* mode/filter list, .remember =  order matters! */
    .filters = {
        {RIG_MODE_SSB,  kHz(2.4)},  /* standard SSB filter bandwidth */
        {RIG_MODE_CW,   kHz(2.4)},  /* normal CW filter */
        {RIG_MODE_CW,   kHz(0.5)},  /* CW filter with narrow selection (must be installed!) */
        {RIG_MODE_AM,   kHz(15)},   /* normal AM filter (stock radio has no AM filter!) */
        {RIG_MODE_AM,   kHz(2.4)},  /* AM filter with narrow selection (SSB filter switched in) */
        {RIG_MODE_FM,   kHz(12)},   /* FM with optional FM unit */
        {RIG_MODE_WFM,  kHz(12)},   /* WideFM, with optional FM unit. */
        {RIG_MODE_PKTLSB, kHz(1.8)},/* Alias of MODE_DATA_L */
        {RIG_MODE_PKTLSB, kHz(0.5)},/* Alias of MODE_DATA_LN */
        {RIG_MODE_PKTUSB, kHz(2.4)}, /* Alias for MODE DATA_U */
        {RIG_MODE_PKTUSB, kHz(0.5)},/* Alias of MODE_DATA_UN */
        {RIG_MODE_PKTFM, kHz(12)},  /* Alias for MODE_DATA _F */
        {RIG_MODE_PKTFM, kHz(6)},   /* Alias for MODE_DATA_FN */

        RIG_FLT_END,
    },

    .str_cal =          EMPTY_STR_CAL,
    .cfgparams =        NULL,
    .priv =             NULL,           /* private data FIXME: ?? */

    .rig_init =         ft920_init,
    .rig_cleanup =      ft920_cleanup,
    .rig_open =         ft920_open,     /* port opened */
    .rig_close =        ft920_close,    /* port closed */

    .set_freq =         ft920_set_freq,
    .get_freq =         ft920_get_freq,
    .set_mode =         ft920_set_mode,
    .get_mode =         ft920_get_mode,
    .set_vfo =          ft920_set_vfo,
    .get_vfo =          ft920_get_vfo,
    .set_ptt =          ft920_set_ptt,
    .get_ptt =          ft920_get_ptt,
    .get_dcd =          NULL,
    .set_rptr_shift =   NULL,
    .get_rptr_shift =   NULL,
    .set_rptr_offs =    NULL,
    .get_rptr_offs =     NULL,
    .set_split_freq =   ft920_set_split_freq,
    .get_split_freq =   ft920_get_split_freq,
    .set_split_mode =   ft920_set_split_mode,
    .get_split_mode =   ft920_get_split_mode,
    .set_split_vfo =    ft920_set_split_vfo,
    .get_split_vfo =    ft920_get_split_vfo,
    .set_rit =          ft920_set_rit,
    .get_rit =          ft920_get_rit,
    .set_xit =          ft920_set_xit,
    .get_xit =          ft920_get_xit,
    .set_ts =           NULL,
    .get_ts =           NULL,
    .set_dcs_code =     NULL,
    .get_dcs_code =     NULL,
    .set_tone =         NULL,
    .get_tone =         NULL,
    .set_ctcss_tone =   NULL,
    .get_ctcss_tone =   NULL,
    .set_dcs_sql =      NULL,
    .get_dcs_sql =      NULL,
    .set_tone_sql =     NULL,
    .get_tone_sql =     NULL,
    .set_ctcss_sql =    NULL,
    .get_ctcss_sql =    NULL,
    .power2mW =         NULL,
    .mW2power =         NULL,
    .set_powerstat =    NULL,
    .get_powerstat =    NULL,
    .reset =            NULL,
    .set_ant =          NULL,
    .get_ant =          NULL,
    .set_level =        NULL,
    .get_level =        NULL,
    .set_func =         ft920_set_func,
    .get_func =         ft920_get_func,
    .set_parm =         NULL,
    .get_parm =         NULL,
    .set_ext_level =    NULL,
    .get_ext_level =    NULL,
    .set_ext_parm =     NULL,
    .get_ext_parm =     NULL,
    .set_conf =         NULL,
    .get_conf =         NULL,
    .send_dtmf =        NULL,
    .recv_dtmf =        NULL,
    .send_morse =       NULL,
    .set_bank =         NULL,
    .set_mem =          NULL,
    .get_mem =          NULL,
    .vfo_op =           NULL,
    .scan =             NULL,
    .set_trn =          NULL,
    .get_trn =          NULL,
    .decode_event =     NULL,
    .set_channel =      NULL,
    .get_channel =      NULL,
    .get_info =         NULL,
    .set_chan_all_cb =  NULL,
    .get_chan_all_cb =  NULL,
    .set_mem_all_cb =   NULL,
    .get_mem_all_cb =   NULL,
    .clone_combo_set =  NULL,
    .clone_combo_get =  NULL,
    .hamlib_check_rig_caps = HAMLIB_CHECK_RIG_CAPS
};


/*
 * ************************************
 *
 * Hamlib API functions
 *
 * ************************************
 */

/*
 * rig_init*
 *
 */

static int ft920_init(RIG *rig)
{
    struct ft920_priv_data *priv;

    rig_debug(RIG_DEBUG_VERBOSE, "%s called\n", __func__);

    if (!rig)
    {
        return -RIG_EINVAL;
    }

    rig->state.priv = (struct ft920_priv_data *) calloc(1,
                      sizeof(struct ft920_priv_data));

    if (!rig->state.priv)
    {
        return -RIG_ENOMEM;    /* whoops! memory shortage! */
    }

    priv = rig->state.priv;

    /* TODO: read pacing from preferences */
    priv->pacing =
        FT920_PACING_DEFAULT_VALUE;              /* set pacing to minimum for now */
    priv->current_vfo =  RIG_VFO_A;                         /* default to VFO_A */

    return RIG_OK;
}


/*
 * rig_cleanup*
 *
 * the serial port is closed by the frontend
 *
 */

static int ft920_cleanup(RIG *rig)
{
    rig_debug(RIG_DEBUG_VERBOSE, "%s called\n", __func__);

    if (!rig)
    {
        return -RIG_EINVAL;
    }

    if (rig->state.priv)
    {
        free(rig->state.priv);
    }

    rig->state.priv = NULL;

    return RIG_OK;
}


/*
 * rig_open*
 *
 */

static int ft920_open(RIG *rig)
{
    struct rig_state *rig_s;
    struct ft920_priv_data *priv;
    int err;

    rig_debug(RIG_DEBUG_VERBOSE, "%s called\n", __func__);

    if (!rig)
    {
        return -RIG_EINVAL;
    }

    priv = (struct ft920_priv_data *)rig->state.priv;
    rig_s = &rig->state;

    rig_debug(RIG_DEBUG_TRACE, "%s: write_delay = %i msec\n",
              __func__, rig_s->rigport.write_delay);
    rig_debug(RIG_DEBUG_TRACE, "%s: post_write_delay = %i msec\n",
              __func__, rig_s->rigport.post_write_delay);

    /* Copy native cmd PACING to private cmd storage area */
    memcpy(&priv->p_cmd, &ncmd[FT920_NATIVE_PACING].nseq, YAESU_CMD_LENGTH);

    /* get pacing value, and store in private cmd */
    priv->p_cmd[P1] = priv->pacing;

    rig_debug(RIG_DEBUG_TRACE, "%s: read pacing = %i\n", __func__, priv->pacing);

    err = write_block(&rig->state.rigport, priv->p_cmd, YAESU_CMD_LENGTH);

    if (err != RIG_OK)
    {
        return err;
    }

    /* TODO: more initialization as necessary */

    return RIG_OK;
}


/*
 * rig_close*
 *
 */

static int ft920_close(RIG *rig)
{
    rig_debug(RIG_DEBUG_VERBOSE, "%s called\n", __func__);

    if (!rig)
    {
        return -RIG_EINVAL;
    }

    return RIG_OK;
}


/*
 * rig_set_freq*
 *
 * Set freq for a given VFO
 *
 * Parameter    | Type      | Accepted/expected values
 * ------------------------------------------------------------------
 * *rig         | input     | pointer to private data
 * vfo          | input     | RIG_VFO_A, RIG_VFO_B, RIG_VFO_MEM
 * freq         | input     | frequency to passed VFO
 * ------------------------------------------------------------------
 * Returns RIG_OK on success or an error code on failure
 *
 * Comments:    If vfo is set to RIG_VFO_CUR then vfo from
 *              priv_data is used.
 *
 */

static int ft920_set_freq(RIG *rig, vfo_t vfo, freq_t freq)
{
    struct ft920_priv_data *priv;
    int err, cmd_index;

    rig_debug(RIG_DEBUG_VERBOSE, "%s called\n", __func__);

    if (!rig)
    {
        return -RIG_EINVAL;
    }

    priv = (struct ft920_priv_data *)rig->state.priv;

    rig_debug(RIG_DEBUG_TRACE, "%s: passed vfo = 0x%02x\n", __func__, vfo);
    rig_debug(RIG_DEBUG_TRACE, "%s: passed freq = %"PRIfreq" Hz\n", __func__, freq);

    if (vfo == RIG_VFO_CURR)
    {
        vfo = priv->current_vfo;        /* from previous vfo cmd */
        rig_debug(RIG_DEBUG_TRACE, "%s: priv->current_vfo = 0x%02x\n", __func__, vfo);
    }

    switch (vfo)
    {
    case RIG_VFO_A:                     /* force main display to VFO */
    case RIG_VFO_VFO:
        err = ft920_set_vfo(rig, RIG_VFO_A);

        if (err != RIG_OK)
        {
            return err;
        }

    case RIG_VFO_MEM:                   /* MEM TUNE or user doesn't care */
    case RIG_VFO_MAIN:
        cmd_index = FT920_NATIVE_VFO_A_FREQ_SET;
        break;

    case RIG_VFO_B:
    case RIG_VFO_SUB:
        cmd_index = FT920_NATIVE_VFO_B_FREQ_SET;
        break;

    default:
        return -RIG_EINVAL;             /* sorry, unsupported VFO */
    }

    rig_debug(RIG_DEBUG_TRACE, "%s: set cmd_index = 0x%02x\n", __func__, cmd_index);

    err = ft920_send_dial_freq(rig, cmd_index, freq);

    if (err != RIG_OK)
    {
        return err;
    }

    return RIG_OK;
}


/*
 * rig_get_freq*
 *
 * Return Freq for a given VFO
 *
 * Parameter    | Type      | Accepted/expected values
 * ------------------------------------------------------------------
 * *rig         | input     | pointer to private data
 * vfo          | input     | RIG_VFO_A, RIG_VFO_B, RIG_VFO_MEM
 * *freq        | output    | displayed frequency based on passed VFO
 * ------------------------------------------------------------------
 * Returns RIG_OK on success or an error code on failure
 *
 */

static int ft920_get_freq(RIG *rig, vfo_t vfo, freq_t *freq)
{
    struct ft920_priv_data *priv;
    unsigned char *p;
    unsigned char offset;
    freq_t f;
    int err, cmd_index;

    rig_debug(RIG_DEBUG_VERBOSE, "%s called\n", __func__);
    rig_debug(RIG_DEBUG_TRACE, "%s: passed vfo = 0x%02x\n", __func__, vfo);

    if (!rig)
    {
        return -RIG_EINVAL;
    }

    priv = (struct ft920_priv_data *)rig->state.priv;

    if (vfo == RIG_VFO_CURR)
    {
        vfo = priv->current_vfo;        /* from previous vfo cmd */
        rig_debug(RIG_DEBUG_TRACE, "%s: priv->current_vfo = 0x%02x\n", __func__, vfo);
    }

    switch (vfo)
    {
    case RIG_VFO_A:
    case RIG_VFO_VFO:
        cmd_index = FT920_NATIVE_VFO_DATA;
        offset = FT920_SUMO_VFO_A_FREQ;
        break;

    case RIG_VFO_B:
    case RIG_VFO_SUB:
        cmd_index = FT920_NATIVE_OP_DATA;
        offset = FT920_SUMO_VFO_B_FREQ;
        break;

    case RIG_VFO_MEM:
    case RIG_VFO_MAIN:
        cmd_index = FT920_NATIVE_OP_DATA;
        offset = FT920_SUMO_DISPLAYED_FREQ;
        break;

    default:
        return -RIG_EINVAL;             /* sorry, wrong VFO */
    }

    err = ft920_get_update_data(rig, cmd_index, FT920_VFO_DATA_LENGTH);

    if (err != RIG_OK)
    {
        return err;
    }

    p = &priv->update_data[offset];

    /* big endian integer */
    f = (((((p[0] << 8) + p[1]) << 8) + p[2]) << 8) + p[3];

    rig_debug(RIG_DEBUG_TRACE, "%s: freq = %"PRIfreq" Hz for vfo 0x%02x\n",
              __func__, f, vfo);

    *freq = f;                          /* return displayed frequency */

    return RIG_OK;
}


/*
 * rig_set_mode*
 *
 * Set mode and passband: eg AM, CW etc for a given VFO
 *
 * Parameter    | Type      | Accepted/expected values
 * ------------------------------------------------------------------
 * *rig         | input     | pointer to private data
 * vfo          | input     | RIG_VFO_A, RIG_VFO_B, RIG_VFO_MEM
 * mode         | input     | supported modes (see ft920.h)
 * width        | input     | supported widths (see ft920.h)
 * ------------------------------------------------------------------
 * Returns RIG_OK on success or an error code on failure
 *
 * Comments:    If vfo is set to RIG_VFO_CUR then vfo from
 *              priv_data is used.
 *
 */

static int ft920_set_mode(RIG *rig, vfo_t vfo, rmode_t mode, pbwidth_t width)
{
    struct ft920_priv_data *priv;
    unsigned char cmd_index =
        FT920_NATIVE_VFO_A_PASSBAND_WIDE; /* index of sequence to send */
    unsigned char mode_parm;            /* mode parameter */
    int err;

    rig_debug(RIG_DEBUG_VERBOSE, "%s called\n", __func__);

    if (!rig)
    {
        return -RIG_EINVAL;
    }

    rig_debug(RIG_DEBUG_TRACE, "%s: passed vfo = %s\n", __func__, rig_strvfo(vfo));
    rig_debug(RIG_DEBUG_TRACE, "%s: passed mode = %s\n", __func__,
              rig_strrmode(mode));
    rig_debug(RIG_DEBUG_TRACE, "%s: passed width = %d Hz\n", __func__, (int)width);

    priv = (struct ft920_priv_data *)rig->state.priv;

    if (vfo == RIG_VFO_CURR)
    {
        vfo = priv->current_vfo;        /* from previous vfo cmd */
        rig_debug(RIG_DEBUG_TRACE,
                  "%s: priv->current_vfo  = 0x%02x\n", __func__, vfo);
    }

    /* translate mode from generic to ft920 specific */
    switch (vfo)
    {
    case RIG_VFO_A:                     /* force to VFO */
    case RIG_VFO_VFO:
        err = ft920_set_vfo(rig, RIG_VFO_A);

        if (err != RIG_OK)
        {
            return err;
        }

    case RIG_VFO_MEM:                   /* MEM TUNE or user doesn't care */
    case RIG_VFO_MAIN:
        switch (mode)
        {
        case RIG_MODE_AM:
            mode_parm = MODE_SET_A_AM_W;
            break;

        case RIG_MODE_CW:
            mode_parm = MODE_SET_A_CW_U;
            break;

        case RIG_MODE_USB:
            mode_parm = MODE_SET_A_USB;
            break;

        case RIG_MODE_LSB:
            mode_parm = MODE_SET_A_LSB;
            break;

        case RIG_MODE_FM:
            mode_parm = MODE_SET_A_FM_W;
            break;

        case RIG_MODE_RTTY:
            mode_parm = MODE_SET_A_DATA_L;
            break;

        case RIG_MODE_PKTLSB:
            mode_parm = MODE_SET_A_DATA_L;
            break;

        case RIG_MODE_PKTUSB:
            mode_parm = MODE_SET_A_DATA_U;
            break;

        case RIG_MODE_PKTFM:
            mode_parm = MODE_SET_A_DATA_F;
            break;

        default:
            return -RIG_EINVAL;         /* sorry, wrong MODE */
        }

        break;

    /* Now VFO B */
    case RIG_VFO_B:
    case RIG_VFO_SUB:
        switch (mode)
        {
        case RIG_MODE_AM:
            mode_parm = MODE_SET_B_AM_W;
            break;

        case RIG_MODE_CW:
            mode_parm = MODE_SET_B_CW_U;
            break;

        case RIG_MODE_USB:
            mode_parm = MODE_SET_B_USB;
            break;

        case RIG_MODE_LSB:
            mode_parm = MODE_SET_B_LSB;
            break;

        case RIG_MODE_FM:
            mode_parm = MODE_SET_B_FM_W;
            break;

        case RIG_MODE_RTTY:
            mode_parm = MODE_SET_B_DATA_L;
            break;

        case RIG_MODE_PKTLSB:
            mode_parm = MODE_SET_B_DATA_L;
            break;

        case RIG_MODE_PKTUSB:
            mode_parm = MODE_SET_B_DATA_U;
            break;

        case RIG_MODE_PKTFM:
            mode_parm = MODE_SET_B_DATA_F;
            break;

        default:
            return -RIG_EINVAL;
        }

        break;

    default:
        return -RIG_EINVAL;             /* sorry, wrong VFO */
    }

    /*
     * Now set width (shamelessly stolen from ft847.c and then butchered :)
     * The FT-920 doesn't appear to support narrow width in USB or LSB modes
     *
     * Yeah, it's ugly... -N0NB
     *
     */
    if (width != RIG_PASSBAND_NOCHANGE)
    {
        if (width == RIG_PASSBAND_NORMAL || width == rig_passband_normal(rig, mode))
        {
            switch (vfo)
            {
            case RIG_VFO_A:
            case RIG_VFO_VFO:
            case RIG_VFO_MEM:
            case RIG_VFO_MAIN:
                cmd_index = FT920_NATIVE_VFO_A_PASSBAND_WIDE;
                break;

            case RIG_VFO_B:
            case RIG_VFO_SUB:
                cmd_index = FT920_NATIVE_VFO_B_PASSBAND_WIDE;
                break;
            }
        }
        else
        {
            if (width == rig_passband_narrow(rig, mode))
            {
                switch (mode)
                {
                case RIG_MODE_CW:
                case RIG_MODE_AM:
                case RIG_MODE_FM:
                case RIG_MODE_PKTFM:
                case RIG_MODE_RTTY:
                    switch (vfo)
                    {
                    case RIG_VFO_A:
                    case RIG_VFO_VFO:
                    case RIG_VFO_MEM:
                    case RIG_VFO_MAIN:
                        cmd_index = FT920_NATIVE_VFO_A_PASSBAND_NAR;
                        break;

                    case RIG_VFO_B:
                    case RIG_VFO_SUB:
                        cmd_index = FT920_NATIVE_VFO_B_PASSBAND_NAR;
                        break;
                    }

                    break;

                default:
                    return -RIG_EINVAL;     /* Invalid mode; how can caller know? */
                }
            }
            else
            {
                if (width != RIG_PASSBAND_NORMAL && width != rig_passband_normal(rig, mode))
                {
                    return -RIG_EINVAL;     /* Invalid width; how can caller know? */
                }
            }
        }
    }

    rig_debug(RIG_DEBUG_TRACE, "%s: set mode_parm = 0x%02x\n", __func__, mode_parm);
    rig_debug(RIG_DEBUG_TRACE, "%s: set cmd_index = %i\n", __func__, cmd_index);

    err = ft920_send_dynamic_cmd(rig, FT920_NATIVE_MODE_SET, mode_parm, 0, 0, 0);

    if (err != RIG_OK)
    {
        return err;
    }

    err = ft920_send_static_cmd(rig, cmd_index);

    if (err != RIG_OK)
    {
        return err;
    }

    return RIG_OK;                      /* Whew! */
}


/*
 * rig_get_mode*
 *
 * Get mode and passband: eg AM, CW etc for a given VFO
 *
 * Parameter    | Type      | Accepted/expected values
 * ------------------------------------------------------------------
 * *rig         | input     | pointer to private data
 * vfo          | input     | RIG_VFO_A, RIG_VFO_B, RIG_VFO_MEM
 * *mode        | output    | supported modes (see ft920.h)
 * *width       | output    | supported widths (see ft920.h)
 * ------------------------------------------------------------------
 * Returns RIG_OK on success or an error code on failure
 *
 */

static int ft920_get_mode(RIG *rig, vfo_t vfo, rmode_t *mode, pbwidth_t *width)
{
    struct ft920_priv_data *priv;
    unsigned char mymode, offset;       /* ft920 mode, flag offset */
    int err, cmd_index, norm;

    rig_debug(RIG_DEBUG_VERBOSE, "%s called\n", __func__);

    if (!rig)
    {
        return -RIG_EINVAL;
    }

    rig_debug(RIG_DEBUG_TRACE, "%s: passed vfo = 0x%02x\n", __func__, vfo);

    priv = (struct ft920_priv_data *)rig->state.priv;

    if (vfo == RIG_VFO_CURR)
    {
        vfo = priv->current_vfo;        /* from previous vfo cmd */
        rig_debug(RIG_DEBUG_TRACE, "%s: priv->current_vfo = 0x%02x\n", __func__, vfo);
    }

    switch (vfo)
    {
    case RIG_VFO_A:
    case RIG_VFO_VFO:
        cmd_index = FT920_NATIVE_VFO_DATA;
        offset = FT920_SUMO_DISPLAYED_MODE;
        break;

    case RIG_VFO_B:
    case RIG_VFO_SUB:
        cmd_index = FT920_NATIVE_VFO_DATA;
        offset = FT920_SUMO_VFO_B_MODE;
        break;

    case RIG_VFO_MEM:
    case RIG_VFO_MAIN:
        cmd_index = FT920_NATIVE_OP_DATA;
        offset = FT920_SUMO_DISPLAYED_MODE;
        break;

    default:
        return -RIG_EINVAL;
    }

    err = ft920_get_update_data(rig, cmd_index, FT920_VFO_DATA_LENGTH);

    if (err != RIG_OK)
    {
        return err;
    }

    mymode = priv->update_data[offset];
    mymode &= MODE_MASK;

    rig_debug(RIG_DEBUG_TRACE, "%s: mymode = 0x%02x\n", __func__, mymode);

    /*
     * translate mode from ft920 to generic.
     *
     * FIXME: FT-920 has 3 DATA modes, LSB, USB, and FM
     * do we need more bit fields in rmode_t? -N0NB
     *
     */
    switch (mymode)
    {
    case MODE_USBN:                     /* not sure this even exists */
        *mode = RIG_MODE_USB;
        norm = FALSE;
        break;

    case MODE_USB:
        *mode = RIG_MODE_USB;
        norm = TRUE;
        break;

    case MODE_LSBN:                     /* not sure this even exists */
        *mode = RIG_MODE_LSB;
        norm = FALSE;
        break;

    case MODE_LSB:
        *mode = RIG_MODE_LSB;
        norm = TRUE;
        break;

    case MODE_CW_UN:
    case MODE_CW_LN:
        *mode = RIG_MODE_CW;
        norm = FALSE;
        break;

    case MODE_CW_U:
    case MODE_CW_L:
        *mode = RIG_MODE_CW;
        norm = TRUE;
        break;

    case MODE_AMN:
        *mode = RIG_MODE_AM;
        norm = FALSE;
        break;

    case MODE_AM:
        *mode = RIG_MODE_AM;
        norm = TRUE;
        break;

    case MODE_FMN:
        *mode = RIG_MODE_FM;
        norm = FALSE;
        break;

    case MODE_FM:
        *mode = RIG_MODE_FM;
        norm = TRUE;
        break;

    case MODE_DATA_LN:
        *mode = RIG_MODE_PKTLSB;
        norm = FALSE;
        break;

    case MODE_DATA_L:
        *mode = RIG_MODE_PKTLSB;
        norm = TRUE;
        break;

    case MODE_DATA_UN:
        *mode = RIG_MODE_PKTUSB;
        norm = FALSE;
        break;

    case MODE_DATA_U:
        *mode = RIG_MODE_PKTUSB;
        norm = TRUE;
        break;

    case MODE_DATA_F:
        *mode = RIG_MODE_PKTFM;
        norm = TRUE;
        break;

    case MODE_DATA_FN:
        *mode = RIG_MODE_PKTFM;
        norm = FALSE;
        break;

    default:
        return -RIG_EINVAL;             /* Oops! file bug report */
    }

    if (norm)
    {
        *width = rig_passband_normal(rig, *mode);
    }
    else
    {
        *width = rig_passband_narrow(rig, *mode);
    }

    rig_debug(RIG_DEBUG_TRACE, "%s: set mode = %s\n", __func__,
              rig_strrmode(*mode));
    rig_debug(RIG_DEBUG_TRACE, "%s: set width = %d Hz\n", __func__, (int)*width);

    return RIG_OK;
}


/*
 * rig_set_vfo*
 *
 * Get active VFO
 *
 * Parameter    | Type      | Accepted/expected values
 * ------------------------------------------------------------------
 * *rig         | input     | pointer to private data
 * vfo          | input     | RIG_VFO_A, RIG_VFO_B
 * ------------------------------------------------------------------
 * Returns RIG_OK on success or an error code on failure
 *
 * Comments:    Set vfo and store requested vfo for later
 *              RIG_VFO_CURR requests.
 *
 */

static int ft920_set_vfo(RIG *rig, vfo_t vfo)
{
    struct ft920_priv_data *priv;
    unsigned char cmd_index;            /* index of sequence to send */
    int err;

    rig_debug(RIG_DEBUG_VERBOSE, "%s called\n", __func__);

    if (!rig)
    {
        return -RIG_EINVAL;
    }

    rig_debug(RIG_DEBUG_TRACE, "%s: passed vfo = 0x%02x\n", __func__, vfo);

    priv = (struct ft920_priv_data *)rig->state.priv;

    if (vfo == RIG_VFO_CURR)
    {
        vfo = priv->current_vfo;        /* from previous vfo cmd */
        rig_debug(RIG_DEBUG_TRACE, "%s: priv->current_vfo = 0x%02x\n", __func__, vfo);
    }

    switch (vfo)
    {
    case RIG_VFO_A:
    case RIG_VFO_VFO:
    case RIG_VFO_MAIN:
        cmd_index = FT920_NATIVE_VFO_A;
        priv->current_vfo = vfo;        /* update active VFO */
        break;

    case RIG_VFO_B:
    case RIG_VFO_SUB:
        cmd_index = FT920_NATIVE_VFO_B;
        priv->current_vfo = vfo;
        break;

    default:
        return -RIG_EINVAL;             /* sorry, wrong VFO */
    }

    rig_debug(RIG_DEBUG_TRACE, "%s: set cmd_index = %i\n", __func__, cmd_index);

    err = ft920_send_static_cmd(rig, cmd_index);

    if (err != RIG_OK)
    {
        return err;
    }

    return RIG_OK;
}


/*
 * rig_get_vfo*
 *
 * Get active VFO
 *
 * Parameter    | Type      | Accepted/expected values
 * ------------------------------------------------------------------
 * *rig         | input     | pointer to private data
 * *vfo         | output    | RIG_VFO_A, RIG_VFO_B, RIG_VFO_MEM
 * ------------------------------------------------------------------
 * Returns RIG_OK on success or an error code on failure
 *
 * Comments:    Get current RX vfo/mem and store requested vfo for
 *              later RIG_VFO_CURR requests plus pass the tested
 *              vfo/mem back to the frontend.
 *
 */

static int ft920_get_vfo(RIG *rig, vfo_t *vfo)
{
    struct ft920_priv_data *priv;
    unsigned char status_0;             /* ft920 status flag 0 */
    unsigned char status_1;             /* ft920 status flag 1 */
    int err;

    rig_debug(RIG_DEBUG_VERBOSE, "%s called\n", __func__);

    if (!rig)
    {
        return -RIG_EINVAL;
    }

    priv = (struct ft920_priv_data *)rig->state.priv;

    /* Get flags for VFO status */
    err = ft920_get_update_data(rig, FT920_NATIVE_STATUS_FLAGS,
                                FT920_STATUS_FLAGS_LENGTH);

    if (err != RIG_OK)
    {
        return err;
    }

    status_0 = priv->update_data[FT920_SUMO_DISPLAYED_STATUS_0];
    status_0 &= SF_VFOB;                /* get VFO B (sub display) active bits */

    status_1 = priv->update_data[FT920_SUMO_DISPLAYED_STATUS_1];
    status_1 &= SF_VFO_MASK;            /* get VFO/MEM (main display) active bits */

    rig_debug(RIG_DEBUG_TRACE, "%s: vfo status_0 = 0x%02x\n", __func__, status_0);
    rig_debug(RIG_DEBUG_TRACE, "%s: vfo status_1 = 0x%02x\n", __func__, status_1);

    /*
     * translate vfo status from ft920 to generic.
     *
     * Figuring out whether VFO B is the active RX vfo is tough as
     * Status Flag 0 bits 0 & 1 contain this information.  Testing
     * Status Flag 1 only gives us the state of the main display.
     *
     */
    switch (status_0)
    {
    case SF_VFOB:
        *vfo = RIG_VFO_B;
        priv->current_vfo = RIG_VFO_B;
        break;

    case SF_SPLITB:                     /* Split operation, RX on VFO B */
        *vfo = RIG_VFO_B;
        priv->current_vfo = RIG_VFO_B;
        break;
    }

    /*
     * Okay now test for the active MEM/VFO status of the main display
     *
     */
    switch (status_1)
    {
    case SF_QMB:
    case SF_MT:
    case SF_MR:
        *vfo = RIG_VFO_MEM;
        priv->current_vfo = RIG_VFO_MEM;
        break;

    case SF_VFO:
        switch (status_0)
        {
        case SF_SPLITA:                 /* Split operation, RX on VFO A */
            *vfo = RIG_VFO_A;
            priv->current_vfo = RIG_VFO_A;
            break;

        case SF_VFOA:
            *vfo = RIG_VFO_A;
            priv->current_vfo = RIG_VFO_A;
            break;
        }

        break;

    default:                            /* Oops! */
        return -RIG_EINVAL;             /* sorry, wrong current VFO */
    }

    rig_debug(RIG_DEBUG_TRACE, "%s: set vfo = 0x%02x\n", __func__, *vfo);

    return RIG_OK;
}


/*
 * rig_set_split_vfo*
 *
 * Set the '920 into split TX/RX mode
 *
 * Parameter    | Type      | Accepted/expected values
 * ------------------------------------------------------------------
 * *rig         | input     | pointer to private data
 * vfo          | input     | not used
 * split        | input     | RIG_SPLIT_ON, RIG_SPLIT_OFF
 * tx_vfo       | input     | VFO to use for TX (not used)
 * ------------------------------------------------------------------
 * Returns RIG_OK on success or an error code on failure
 *
 * Comments:    VFO cannot be set as the set split_on command only
 *              changes the TX to the sub display.  Setting split off
 *              returns the TX to the main display.
 *
 */

static int ft920_set_split_vfo(RIG *rig, vfo_t vfo, split_t split,
                               vfo_t tx_vfo)
{
    unsigned char cmd_index;
    int err;

    rig_debug(RIG_DEBUG_VERBOSE, "%s called\n", __func__);

    if (!rig)
    {
        return -RIG_EINVAL;
    }

    rig_debug(RIG_DEBUG_TRACE, "%s: passed vfo = 0x%02x\n", __func__, vfo);
    rig_debug(RIG_DEBUG_TRACE, "%s: passed split = 0x%02x\n", __func__, split);
    rig_debug(RIG_DEBUG_TRACE, "%s: passed tx_vfo = 0x%02x\n", __func__, tx_vfo);

    switch (tx_vfo)
    {
    case RIG_VFO_A:
    case RIG_VFO_MAIN:
    case RIG_VFO_VFO:
        break;

    case RIG_VFO_B:
    case RIG_VFO_SUB:
        break;

    default:
        return -RIG_EINVAL;
    }

    switch (split)
    {
    case RIG_SPLIT_OFF:
        cmd_index = FT920_NATIVE_SPLIT_OFF;
        break;

    case RIG_SPLIT_ON:
        cmd_index = FT920_NATIVE_SPLIT_ON;
        break;

    default:
        return -RIG_EINVAL;
    }

    err = ft920_send_static_cmd(rig, cmd_index);

    if (err != RIG_OK)
    {
        return err;
    }

    return RIG_OK;
}


/*
 * rig_get_split_vfo*
 *
 * Get whether the '920 is in split mode
 *
 * Parameter    | Type      | Accepted/expected values
 * ------------------------------------------------------------------
 * *rig         | input     | pointer to private data
 * vfo          | input     | not used
 * *split       | output    | RIG_SPLIT_ON, RIG_SPLIT_OFF
 * *tx_vfo      | output    | not used
 * ------------------------------------------------------------------
 * Returns RIG_OK on success or an error code on failure
 *
 */

static int ft920_get_split_vfo(RIG *rig, vfo_t vfo, split_t *split,
                               vfo_t *tx_vfo)
{
    struct ft920_priv_data *priv;
    unsigned char status_0;
    int err;

    rig_debug(RIG_DEBUG_VERBOSE, "%s called\n", __func__);

    if (!rig)
    {
        return -RIG_EINVAL;
    }

    rig_debug(RIG_DEBUG_TRACE, "%s: passed vfo = 0x%02x\n", __func__, vfo);

    priv = (struct ft920_priv_data *)rig->state.priv;

    /* Get flags for VFO split status */
    err = ft920_get_update_data(rig, FT920_NATIVE_STATUS_FLAGS,
                                FT920_STATUS_FLAGS_LENGTH);

    if (err != RIG_OK)
    {
        return err;
    }

    status_0 = priv->update_data[FT920_SUMO_DISPLAYED_STATUS_0];
    status_0 &= SF_VFOB;                /* get VFO B (sub display) active bits */

    rig_debug(RIG_DEBUG_TRACE, "%s: split status_0 = 0x%02x\n", __func__, status_0);

    switch (status_0)
    {
    case SF_SPLITA:         /* VFOB (sub display) is TX  Got that? */
        *tx_vfo = RIG_VFO_B;
        *split = RIG_SPLIT_ON;
        break;

    case SF_SPLITB:         /* VFOA is TX */
        *tx_vfo = RIG_VFO_A;
        *split = RIG_SPLIT_ON;
        break;

    case SF_VFOA:
        *tx_vfo = RIG_VFO_A;
        *split = RIG_SPLIT_OFF;
        break;

    case SF_VFOB:
        *tx_vfo = RIG_VFO_B;
        *split = RIG_SPLIT_OFF;
        break;

    default:
        return -RIG_EINVAL;
    }

    return RIG_OK;
}


/*
 * rig_set_split_freq*
 *
 * Set the '920 split TX freq
 *
 * Parameter    | Type      | Accepted/expected values
 * ------------------------------------------------------------------
 * *rig         | input     | pointer to private data
 * vfo          | input     | currVFO, VFOA, VFOB, MEM
 * tx_freq      | input     | split transmit frequency
 * ------------------------------------------------------------------
 * Returns RIG_OK on success or an error code on failure
 *
 * Comments:    Checks to see if 920 is in split mode and if so sets
 *              the frequency of the TX VFO.  If not in split mode
 *              does nothing and returns.
 *
 */

static int ft920_set_split_freq(RIG *rig, vfo_t vfo, freq_t tx_freq)
{
    struct ft920_priv_data *priv;
    int err;

    rig_debug(RIG_DEBUG_VERBOSE, "%s called\n", __func__);

    if (!rig)
    {
        return -RIG_EINVAL;
    }

    rig_debug(RIG_DEBUG_TRACE, "%s: passed vfo = 0x%02x\n", __func__, vfo);
    rig_debug(RIG_DEBUG_TRACE, "%s: passed freq = %"PRIfreq" Hz\n", __func__,
              tx_freq);

    err = rig_set_split_vfo(rig, RIG_VFO_A, RIG_SPLIT_ON, RIG_VFO_B);

    if (err != RIG_OK) { RETURNFUNC(err); }

    priv = (struct ft920_priv_data *)rig->state.priv;

    err = ft920_get_split_vfo(rig, vfo, &priv->split, &priv->split_vfo);

    if (err != RIG_OK)
    {
        return err;
    }

    switch ((int)priv->split)
    {
    case TRUE:              /* '920 is in split mode */
        err = ft920_set_freq(rig, priv->split_vfo, tx_freq);

        if (err != RIG_OK)
        {
            return err;
        }

        break;

    default:
        break;
    }

    return RIG_OK;
}


/*
 * rig_get_split_freq*
 *
 * Get the '920 split TX freq
 *
 * Parameter    | Type      | Accepted/expected values
 * ------------------------------------------------------------------
 * *rig         | input     | pointer to private data
 * vfo          | input     | currVFO, VFOA, VFOB, MEM
 * *tx_freq     | output    | split transmit frequency
 * ------------------------------------------------------------------
 * Returns RIG_OK on success or an error code on failure
 *
 * Comments:    Checks to see if the 920 is in split mode, if so it
 *              checks which VFO is set for TX and then gets the
 *              frequency of that VFO and stores it into *tx_freq.
 *              If not in split mode returns 0 Hz.
 *
 */

static int ft920_get_split_freq(RIG *rig, vfo_t vfo, freq_t *tx_freq)
{
    struct ft920_priv_data *priv;
    int err;

    rig_debug(RIG_DEBUG_VERBOSE, "%s called\n", __func__);

    if (!rig)
    {
        return -RIG_EINVAL;
    }

    priv = (struct ft920_priv_data *)rig->state.priv;

    err = ft920_get_split_vfo(rig, vfo, &priv->split, &priv->split_vfo);

    if (err != RIG_OK)
    {
        return err;
    }

    switch ((int)priv->split)
    {
    case TRUE:              /* '920 is in split mode */
        err = ft920_get_freq(rig, priv->split_vfo, tx_freq);

        if (err != RIG_OK)
        {
            return err;
        }

        break;

    default:
        *tx_freq = 0;
        break;
    }

    return RIG_OK;
}


/*
 * rig_set_split_mode
 *
 * Set the '920 split TX mode
 *
 * Parameter    | Type      | Accepted/expected values
 * ------------------------------------------------------------------
 * *rig         | input     | pointer to private data
 * vfo          | input     | currVFO, VFOA, VFOB, MEM
 * tx_mode      | input     | supported modes
 * tx_width     | input     | supported widths
 * ------------------------------------------------------------------
 * Returns RIG_OK on success or an error code on failure
 *
 * Comments:    Checks to see if 920 is in split mode and if so sets
 *              the mode and passband of the TX VFO.  If not in split mode
 *              does nothing and returns.
 *
 */

static int ft920_set_split_mode(RIG *rig, vfo_t vfo, rmode_t tx_mode,
                                pbwidth_t tx_width)
{
    struct ft920_priv_data *priv;
    int err;

    rig_debug(RIG_DEBUG_VERBOSE, "%s called\n", __func__);

    if (!rig)
    {
        return -RIG_EINVAL;
    }

    rig_debug(RIG_DEBUG_TRACE, "%s: passed vfo = %s\n", __func__, rig_strvfo(vfo));
    rig_debug(RIG_DEBUG_TRACE, "%s: passed mode = %s\n", __func__,
              rig_strrmode(tx_mode));
    rig_debug(RIG_DEBUG_TRACE, "%s: passed width = %d Hz\n", __func__,
              (int)tx_width);

    priv = (struct ft920_priv_data *)rig->state.priv;

    err = ft920_get_split_vfo(rig, vfo, &priv->split, &priv->split_vfo);

    if (err != RIG_OK)
    {
        return err;
    }

    switch ((int)priv->split)
    {
    case TRUE:              /* '920 is in split mode */
        err = ft920_set_mode(rig, priv->split_vfo, tx_mode, tx_width);

        if (err != RIG_OK)
        {
            return err;
        }

        break;

    default:
        break;
    }

    return RIG_OK;
}


/*
 * rig_get_split_mode*
 *
 * Get the '920 split TX mode
 *
 * Parameter    | Type      | Accepted/expected values
 * ------------------------------------------------------------------
 * *rig         | input     | pointer to private data
 * vfo          | input     | currVFO, VFOA, VFOB, MEM
 * *tx_mode     | output    | supported modes
 * *tx_width    | output    | supported widths
 * ------------------------------------------------------------------
 * Returns RIG_OK on success or an error code on failure
 *
 * Comments:    Checks to see if the 920 is in split mode, if so it
 *              checks which VFO is set for TX and then gets the
 *              mode and passband of that VFO and stores it into *tx_mode
 *              and tx_width respectively.  If not in split mode returns
 *              RIG_MODE_NONE and 0 Hz.
 *
 */

static int ft920_get_split_mode(RIG *rig, vfo_t vfo, rmode_t *tx_mode,
                                pbwidth_t *tx_width)
{
    struct ft920_priv_data *priv;
    int err;

    rig_debug(RIG_DEBUG_VERBOSE, "%s called\n", __func__);

    if (!rig)
    {
        return -RIG_EINVAL;
    }

    priv = (struct ft920_priv_data *)rig->state.priv;

    err = ft920_get_split_vfo(rig, vfo, &priv->split, &priv->split_vfo);

    if (err != RIG_OK)
    {
        return err;
    }

    switch ((int)priv->split)
    {
    case TRUE:              /* '920 is in split mode */
        err = ft920_get_mode(rig, priv->split_vfo, tx_mode, tx_width);

        if (err != RIG_OK)
        {
            return err;
        }

        break;

    default:
        *tx_mode = RIG_MODE_NONE;
        *tx_width = 0;
        break;
    }

    return RIG_OK;
}


/*
 * rig_set_rit*
 *
 * Set the RIT offset
 *
 * Parameter    | Type      | Accepted/expected values
 * ------------------------------------------------------------------
 * *rig         | input     | pointer to private data
 * vfo          | input     | currVFO, VFOA, VFOB, MEM
 * rit          | input     | -9999 to 9999 Hz
 * ------------------------------------------------------------------
 * Returns RIG_OK on success or an error code on failure
 *
 * Comments:    vfo is ignored as RIT cannot be changed on sub VFO
 *
 *              FIXME:  Should rig be forced into VFO mode if RIG_VFO_A
 *                      or RIG_VFO_VFO is received?
 *
 *              VFO and MEM rit values are independent.  The sub display
 *              carries an RIT value only if A<>B button is pressed or
 *              set_vfo is called with RIG_VFO_B and the main display has
 *              an RIT value.
 */

static int ft920_set_rit(RIG *rig, vfo_t vfo, shortfreq_t rit)
{
    unsigned char offset;
    int err;

    rig_debug(RIG_DEBUG_VERBOSE, "%s called\n", __func__);

    if (!rig)
    {
        return -RIG_EINVAL;
    }

    if (rit < -9999 || rit > 9999)
    {
        return -RIG_EINVAL;
    }

    rig_debug(RIG_DEBUG_TRACE, "%s: passed vfo = 0x%02x\n", __func__, vfo);
    rig_debug(RIG_DEBUG_TRACE, "%s: passed rit = %li\n", __func__, rit);

    if (rit == 0)
    {
        offset = CLAR_RX_OFF;
    }
    else
    {
        offset = CLAR_RX_ON;
    }

    rig_debug(RIG_DEBUG_TRACE, "%s: set offset = 0x%02x\n", __func__, offset);

    err = ft920_send_dynamic_cmd(rig, FT920_NATIVE_CLARIFIER_OPS, offset, 0, 0, 0);

    if (err != RIG_OK)
    {
        return err;
    }

    err = ft920_send_rit_freq(rig, FT920_NATIVE_CLARIFIER_OPS, rit);

    if (err != RIG_OK)
    {
        return err;
    }

    return RIG_OK;
}


/*
 * rig_get_rit*
 *
 * Get the RIT offset
 *
 * Parameter    | Type      | Accepted/expected values
 * ------------------------------------------------------------------
 * *rig         | input     | pointer to private data
 * vfo          | input     | currVFO, VFOA, VFOB, MEM
 * *rit         | output    | -9999 to 9999 Hz
 * ------------------------------------------------------------------
 * Returns RIG_OK on success or an error code on failure
 *
 * Comments:    Value of vfo is ignored as it's not needed.
 *
 *              Rig returns offset as hex from 0x0000 to 0x270f for
 *              0 to +9999 Hz and 0xffff to 0xd8f1 for -1 to -9999 Hz
 *
 *              VFO and MEM rit values are independent.  The sub display
 *              carries an RIT value only if A<>B button is pressed or
 *              set_vfo is called with RIG_VFO_B and the main display has
 *              an RIT value.
 */

static int ft920_get_rit(RIG *rig, vfo_t vfo, shortfreq_t *rit)
{
    struct ft920_priv_data *priv;
    unsigned char *p;
    unsigned char offset;
    shortfreq_t f;
    int err, cmd_index;

    rig_debug(RIG_DEBUG_VERBOSE, "%s called\n", __func__);

    if (!rig)
    {
        return -RIG_EINVAL;
    }

    rig_debug(RIG_DEBUG_TRACE, "%s: passed vfo = 0x%02x\n", __func__, vfo);

    priv = (struct ft920_priv_data *)rig->state.priv;

    if (vfo == RIG_VFO_CURR)
    {
        vfo = priv->current_vfo;        /* from previous vfo cmd */
        rig_debug(RIG_DEBUG_TRACE, "%s: priv->current_vfo = 0x%02x\n", __func__, vfo);
    }

    switch (vfo)
    {
    case RIG_VFO_MEM:
    case RIG_VFO_MAIN:
        cmd_index = FT920_NATIVE_OP_DATA;
        offset = FT920_SUMO_DISPLAYED_CLAR;
        break;

    case RIG_VFO_A:
    case RIG_VFO_VFO:
        cmd_index = FT920_NATIVE_VFO_DATA;
        offset = FT920_SUMO_VFO_A_CLAR;
        break;

    case RIG_VFO_B:
    case RIG_VFO_SUB:
        cmd_index = FT920_NATIVE_VFO_DATA;
        offset = FT920_SUMO_VFO_B_CLAR;
        break;

    default:
        return -RIG_EINVAL;
    }

    rig_debug(RIG_DEBUG_TRACE, "%s: set cmd_index = %i\n", __func__, cmd_index);
    rig_debug(RIG_DEBUG_TRACE, "%s: set offset = 0x%02x\n", __func__, offset);

    err = ft920_get_update_data(rig, cmd_index, FT920_VFO_DATA_LENGTH);

    if (err != RIG_OK)
    {
        return err;
    }

    p = &priv->update_data[offset];

    /* big endian integer */
    f = (p[0] << 8) + p[1];

    if (f > 0xd8f0)                     /* 0xd8f1 to 0xffff is negative offset */
    {
        f = ~(0xffff - f);
    }

    rig_debug(RIG_DEBUG_TRACE, "%s: read freq = %li Hz\n", __func__, f);

    *rit = f;                           /* store clarifier frequency */

    return RIG_OK;
}


/*
 * rig_set_xit
 *
 * Set the XIT offset
 *
 * Parameter    | Type      | Accepted/expected values
 * ------------------------------------------------------------------
 * *rig         | input     | pointer to private data
 * vfo          | input     | currVFO, VFOA, VFOB, MEM
 * xit          | input     | -9999 to 9999 Hz
 * ------------------------------------------------------------------
 * Returns RIG_OK on success or an error code on failure
 *
 * Comments:    vfo is ignored as XIT cannot be changed on sub VFO
 *
 *              FIXME:  Should rig be forced into VFO mode if RIG_VFO_A
 *                      or RIG_VFO_VFO is received?
 */

static int ft920_set_xit(RIG *rig, vfo_t vfo, shortfreq_t xit)
{
    unsigned char offset;
    int err;

    rig_debug(RIG_DEBUG_VERBOSE, "%s called\n", __func__);

    if (!rig)
    {
        return -RIG_EINVAL;
    }

    if (xit < -9999 || xit > 9999)
    {
        return -RIG_EINVAL;
    }

    rig_debug(RIG_DEBUG_TRACE, "%s: passed vfo = 0x%02x\n", __func__, vfo);
    rig_debug(RIG_DEBUG_TRACE, "%s: passed xit = %li\n", __func__, xit);

    if (xit == 0)
    {
        offset = CLAR_TX_OFF;
    }
    else
    {
        offset = CLAR_TX_ON;
    }

    rig_debug(RIG_DEBUG_TRACE, "%s: set offset = 0x%02x\n", __func__, offset);

    err = ft920_send_dynamic_cmd(rig, FT920_NATIVE_CLARIFIER_OPS, offset, 0, 0, 0);

    if (err != RIG_OK)
    {
        return err;
    }

    err = ft920_send_rit_freq(rig, FT920_NATIVE_CLARIFIER_OPS, xit);

    if (err != RIG_OK)
    {
        return err;
    }

    return RIG_OK;
}


/*
 * rig_get_xit*
 *
 * Get the XIT offset
 *
 * Parameter    | Type      | Accepted/expected values
 * ------------------------------------------------------------------
 * *rig         | input     | pointer to private data
 * vfo          | input     | currVFO, VFOA, VFOB, MEM
 * *xit         | output    | -9999 to 9999 Hz
 * ------------------------------------------------------------------
 * Returns RIG_OK on success or an error code on failure
 *
 * Comments:    Value of vfo is ignored as it's not needed
 *
 *              Rig returns offset as hex from 0x0000 to 0x270f for
 *              0 to +9999 Hz and 0xffff to 0xd8f1 for -1 to -9999 Hz
 */

static int ft920_get_xit(RIG *rig, vfo_t vfo, shortfreq_t *xit)
{
    int err;

    rig_debug(RIG_DEBUG_VERBOSE, "%s called\n", __func__);

    if (!rig)
    {
        return -RIG_EINVAL;
    }

    err = ft920_get_rit(rig, vfo, xit); /* abuse get_rit and store in *xit */

    if (err != RIG_OK)
    {
        return err;
    }

    return RIG_OK;
}


/*
 * rig_set_ptt*
 *
 * Set the '920 into TX mode
 *
 * Parameter    | Type      | Accepted/expected values
 * ------------------------------------------------------------------
 * *rig         | input     | pointer to private data
 * vfo          | input     | currVFO, VFOA, VFOB, MEM
 * ptt          | input     | RIG_PTT_OFF, RIG_PTT_ON
 * ------------------------------------------------------------------
 * Returns RIG_OK on success or an error code on failure
 *
 * Comments:    vfo is respected by calling ft920_set_vfo if
 *              passed vfo != priv->current_vfo
 *
 *              This command is not documented in my '920 manual,
 *              but it works!  -N0NB
 *
 */

static int ft920_set_ptt(RIG *rig, vfo_t vfo, ptt_t ptt)
{
    struct ft920_priv_data *priv;
    unsigned char cmd_index;
    int err;

    rig_debug(RIG_DEBUG_VERBOSE, "%s called\n", __func__);

    if (!rig)
    {
        return -RIG_EINVAL;
    }

    priv = (struct ft920_priv_data *)rig->state.priv;

    rig_debug(RIG_DEBUG_TRACE, "%s: passed vfo = 0x%02x\n", __func__, vfo);
    rig_debug(RIG_DEBUG_TRACE, "%s: passed ptt = 0x%02x\n", __func__, ptt);

    if (vfo == RIG_VFO_CURR)
    {
        vfo = priv->current_vfo;        /* from previous vfo cmd */
        rig_debug(RIG_DEBUG_TRACE,
                  "%s: priv->current_vfo = 0x%02x\n", __func__, vfo);
    }
    else if (vfo != priv->current_vfo)
    {
        ft920_set_vfo(rig, vfo);
    }

    switch (ptt)
    {
    case RIG_PTT_OFF:
        cmd_index = FT920_NATIVE_PTT_OFF;
        break;

    case RIG_PTT_ON:
        cmd_index = FT920_NATIVE_PTT_ON;
        break;

    default:
        return -RIG_EINVAL;             /* wrong PTT state! */
    }

    err = ft920_send_static_cmd(rig, cmd_index);

    if (err != RIG_OK)
    {
        return err;
    }

    return RIG_OK;
}


/*
 * rig_get_ptt*
 *
 * Get current PTT status
 *
 * Parameter    | Type      | Accepted/expected values
 * ------------------------------------------------------------------
 * *rig         | input     | pointer to private data
 * vfo          | input     | currVFO, VFOA, VFOB, MEM
 * *ptt         | output    | RIG_PTT_OFF, RIG_PTT_ON
 * ------------------------------------------------------------------
 * Returns RIG_OK on success or an error code on failure
 *
 * Comments:    Get the PTT state
 */

static int ft920_get_ptt(RIG *rig, vfo_t vfo, ptt_t *ptt)
{
    struct ft920_priv_data *priv;
    unsigned char stat_0;               /* ft920 status flag 0 */
    int err;

    rig_debug(RIG_DEBUG_VERBOSE, "%s called\n", __func__);

    if (!rig)
    {
        return -RIG_EINVAL;
    }

    priv = (struct ft920_priv_data *)rig->state.priv;

    /* Get flags for VFO status */
    err = ft920_get_update_data(rig, FT920_NATIVE_STATUS_FLAGS,
                                FT920_STATUS_FLAGS_LENGTH);

    if (err != RIG_OK)
    {
        return err;
    }

    /*
     * The FT-920 status gives two flags for PTT, one if the PTT
     * line is grounded externally and the other if the PTT line
     * is grounded as the result of a CAT command.  However, the
     * 7th bit of status byte 0 is set or cleared in each case
     * and gives an accurate state of the PTT line.
     */
    stat_0 = priv->update_data[FT920_SUMO_DISPLAYED_STATUS_0];
    stat_0 &= SF_PTT_MASK;              /* get external PTT active bit */

    rig_debug(RIG_DEBUG_TRACE, "%s: stat_0 = 0x%02x\n", __func__, stat_0);

    switch (stat_0)
    {
    case SF_PTT_OFF:
        *ptt = RIG_PTT_OFF;
        break;

    case SF_PTT_ON:
        *ptt = RIG_PTT_ON;
        break;

    default:                            /* Oops! */
        return -RIG_EINVAL;             /* Invalid PTT bit?! */
    }

    return RIG_OK;
}


/*
 * rig_set_func*
 *
 * Set rig function
 *
 * Parameter    | Type      | Accepted/expected values
 * ------------------------------------------------------------------
 * *rig         | input     | pointer to private data
 * vfo          | input     | currVFO, VFOA, VFOB, MEM
 * func         | input     | TUNER
 * status       | input     | 0 = bypass, 1 =inline, 2 = start tuning
 *              |           |                            (toggle)
 * ------------------------------------------------------------------
 * Returns RIG_OK on success or an error code on failure
 *
 * Comments:    Set the tuner to on, off, or start
 */

static int ft920_set_func(RIG *rig, vfo_t vfo, setting_t func, int status)
{
    struct ft920_priv_data *priv;
    unsigned char cmd_index;
    int err;

    rig_debug(RIG_DEBUG_VERBOSE, "%s called\n", __func__);

    if (!rig)
    {
        return -RIG_EINVAL;
    }

    priv = (struct ft920_priv_data *)rig->state.priv;

    rig_debug(RIG_DEBUG_TRACE, "%s: passed vfo = %s, func = %s, status = %d\n",
              __func__, rig_strvfo(vfo), rig_strfunc(func), status);

    if (vfo == RIG_VFO_CURR)
    {
        vfo = priv->current_vfo;        /* from previous vfo cmd */
        rig_debug(RIG_DEBUG_TRACE,
                  "%s: priv->current_vfo = 0x%02x\n", __func__, vfo);
    }
    else if (vfo != priv->current_vfo)
    {
        ft920_set_vfo(rig, vfo);
    }

    switch (func)
    {
    case RIG_FUNC_TUNER:
        switch (status)
        {
        case TUNER_BYPASS:
            cmd_index = FT920_NATIVE_TUNER_BYPASS;
            break;

        case TUNER_INLINE:
            cmd_index = FT920_NATIVE_TUNER_INLINE;
            break;

        case TUNER_TUNING:
            cmd_index = FT920_NATIVE_TUNER_START;
            break;

        default:
            return -RIG_EINVAL;         /* wrong tuner status! */
        }

        break;

    default:
        return -RIG_EINVAL;             /* wrong function! */
    }

    err = ft920_send_static_cmd(rig, cmd_index);

    if (err != RIG_OK)
    {
        return err;
    }

    return RIG_OK;
}


/*
 * rig_get_func*
 *
 * Get rig function
 *
 * Parameter    | Type      | Accepted/expected values
 * ------------------------------------------------------------------
 * *rig         | input     | pointer to private data
 * vfo          | input     | currVFO, VFOA, VFOB, MEM
 * func         | input     | TUNER
 * *status      | output    | 0 = bypass, 1 = inline, 2 = tuning
 * ------------------------------------------------------------------
 * Returns RIG_OK on success or an error code on failure
 *
 * Comments:    Read the tuner status from status flags
 */

static int ft920_get_func(RIG *rig, vfo_t vfo, setting_t func, int *status)
{
    struct ft920_priv_data *priv;
    unsigned char stat_0, stat_2;       /* ft920 status flags 0, 2 */
    int err;

    rig_debug(RIG_DEBUG_VERBOSE, "%s called\n", __func__);

    if (!rig)
    {
        return -RIG_EINVAL;
    }

    priv = (struct ft920_priv_data *)rig->state.priv;

    rig_debug(RIG_DEBUG_TRACE, "%s: passed vfo = %s, func = %s\n",
              __func__, rig_strvfo(vfo), rig_strfunc(func));

    if (vfo == RIG_VFO_CURR)
    {
        vfo = priv->current_vfo;        /* from previous vfo cmd */
        rig_debug(RIG_DEBUG_TRACE,
                  "%s: priv->current_vfo = 0x%02x\n", __func__, vfo);
    }
    else if (vfo != priv->current_vfo)
    {
        ft920_set_vfo(rig, vfo);
    }

    /* Get flags for VFO status */
    err = ft920_get_update_data(rig, FT920_NATIVE_STATUS_FLAGS,
                                FT920_STATUS_FLAGS_LENGTH);

    if (err != RIG_OK)
    {
        return err;
    }

    /*
     * The FT-920 status gives three flags for the tuner state,
     * one if the tuner is On/tuning, another if the tuner is
     * "inline" and the last if the "WAIT" light is on.
     *
     * Currently, will only check if tuner is tuning and inline.
     */
    stat_0 = priv->update_data[FT920_SUMO_DISPLAYED_STATUS_0];
//    stat_0 &= SF_TUNER_TUNE;            /* get tuning state */

    stat_2 = priv->update_data[FT920_SUMO_DISPLAYED_STATUS_2];
//    stat_2 &= SF_TUNER_INLINE;          /* get tuner inline state */

    rig_debug(RIG_DEBUG_TRACE, "%s: stat_0 = 0x%02x, stat_2 = 0x%02x\n",
              __func__, stat_0, stat_2);

    switch (func)
    {
    case RIG_FUNC_TUNER:
        if (stat_0 & SF_TUNER_TUNE)
        {
            *status = TUNER_TUNING;
        }
        else if (stat_2 & SF_TUNER_INLINE)
        {
            *status = TUNER_INLINE;
        }
        else
        {
            *status = TUNER_BYPASS;
        }

        break;

    case RIG_FUNC_LOCK:
        switch (vfo)
        {
        case RIG_VFO_A:
            if (stat_2 & SF_VFOA_LOCK)
            {
                *status = TRUE;
            }
            else
            {
                *status = FALSE;
            }

            break;

        case RIG_VFO_B:
            if (stat_2 & SF_VFOB_LOCK)
            {
                *status = TRUE;
            }
            else
            {
                *status = FALSE;
            }

            break;
        }

        break;

    default:
        return -RIG_EINVAL;             /* wrong function! */
    }

    return RIG_OK;
}


/*
 * ************************************
 *
 * Private functions to ft920 backend
 *
 * ************************************
 */


/*
 * Private helper function to retrieve update data from rig.
 * using pacing value and buffer indicated in *priv struct.
 * Extended to be command agnostic as 920 has several ways to
 * get data and several ways to return it.
 *
 * Need to use this when doing ft920_get_* stuff
 *
 * Arguments:   *rig    Valid RIG instance
 *              ci      command index
 *              rl      expected length of returned data in octets
 *
 * Returns:     RIG_OK if all called functions are successful,
 *              otherwise returns error from called functiion
 */

static int ft920_get_update_data(RIG *rig, unsigned char ci, unsigned char rl)
{
    struct ft920_priv_data *priv;
    int n;                              /* for read_  */
    int err;

    rig_debug(RIG_DEBUG_VERBOSE, "%s called\n", __func__);

    if (!rig)
    {
        return -RIG_EINVAL;
    }

    priv = (struct ft920_priv_data *)rig->state.priv;

    err = ft920_send_static_cmd(rig, ci);

    if (err != RIG_OK)
    {
        return err;
    }

    n = read_block(&rig->state.rigport, priv->update_data, rl);

    if (n < 0)
    {
        return n;    /* die returning read_block error */
    }

    rig_debug(RIG_DEBUG_TRACE, "%s: read %i bytes\n", __func__, n);

    return RIG_OK;
}


/*
 * Private helper function to send a complete command sequence.
 *
 * TODO: place variant of this in yaesu.c
 *
 * Arguments:   *rig    Valid RIG instance
 *              ci      Command index of the ncmd table
 *
 * Returns:     RIG_OK if all called functions are successful,
 *              otherwise returns error from called functiion
 */

static int ft920_send_static_cmd(RIG *rig, unsigned char ci)
{
    int err;

    rig_debug(RIG_DEBUG_VERBOSE, "%s called\n", __func__);

    if (!rig)
    {
        return -RIG_EINVAL;
    }

    /*
     * If we've been passed a command index (ci) that is marked
     * as dynamic (0), then bail out.
     */
    if (!ncmd[ci].ncomp)
    {
        rig_debug(RIG_DEBUG_TRACE, "%s: Attempt to send incomplete sequence\n",
                  __func__);
        return -RIG_EINVAL;
    }

    err = write_block(&rig->state.rigport, ncmd[ci].nseq, YAESU_CMD_LENGTH);

    if (err != RIG_OK)
    {
        return err;
    }

    return RIG_OK;
}


/*
 * Private helper function to build and then send a complete command
 * sequence.
 *
 * TODO: place variant of this in yaesu.c
 *
 * Arguments:   *rig    Valid RIG instance
 *              ci      Command index of the ncmd table
 *              p1-p4   Command parameters
 *
 * Returns:     RIG_OK if all called functions are successful,
 *              otherwise returns error from called functiion
 */

static int ft920_send_dynamic_cmd(RIG *rig, unsigned char ci,
                                  unsigned char p1, unsigned char p2,
                                  unsigned char p3, unsigned char p4)
{
    struct ft920_priv_data *priv;
    int err;

    rig_debug(RIG_DEBUG_VERBOSE, "%s called\n", __func__);

    if (!rig)
    {
        return -RIG_EINVAL;
    }

    rig_debug(RIG_DEBUG_TRACE, "%s: passed ci = %i\n", __func__, ci);
    rig_debug(RIG_DEBUG_TRACE,
              "%s: passed p1 = 0x%02x, p2 = 0x%02x, p3 = 0x%02x, p4 = 0x%02x,\n",
              __func__, p1, p2, p3, p4);

    priv = (struct ft920_priv_data *)rig->state.priv;

    /*
     * If we've been passed a command index (ci) that is marked
     * as static (1), then bail out.
     */
    if (ncmd[ci].ncomp)
    {
        rig_debug(RIG_DEBUG_TRACE,
                  "%s: Attempted to modify a complete command sequence: %i\n",
                  __func__, ci);
        return -RIG_EINVAL;
    }

    memcpy(&priv->p_cmd, &ncmd[ci].nseq, YAESU_CMD_LENGTH);

    priv->p_cmd[P1] = p1;               /* ick */
    priv->p_cmd[P2] = p2;
    priv->p_cmd[P3] = p3;
    priv->p_cmd[P4] = p4;

    err = write_block(&rig->state.rigport, (unsigned char *) &priv->p_cmd,
                      YAESU_CMD_LENGTH);

    if (err != RIG_OK)
    {
        return err;
    }

    return RIG_OK;
}


/*
 * Private helper function to build and send a complete command to
 * change the Main or Sub display frequency.
 *
 * TODO: place variant of this in yaesu.c
 *
 * Arguments:   *rig    Valid RIG instance
 *              ci      Command index of the ncmd table
 *              freq    freq_t frequency value
 *
 * Returns:     RIG_OK if all called functions are successful,
 *              otherwise returns error from called functiion
 */

static int ft920_send_dial_freq(RIG *rig, unsigned char ci, freq_t freq)
{
    struct ft920_priv_data *priv;
    int err;
    // cppcheck-suppress *
    char *fmt = "%s: requested freq after conversion = %"PRIll" Hz\n";

    rig_debug(RIG_DEBUG_VERBOSE, "%s called\n", __func__);

    if (!rig)
    {
        return -RIG_EINVAL;
    }

    rig_debug(RIG_DEBUG_TRACE, "%s: passed ci = %i\n", __func__, ci);
    rig_debug(RIG_DEBUG_TRACE, "%s: passed freq = %"PRIfreq" Hz\n", __func__, freq);

    priv = (struct ft920_priv_data *)rig->state.priv;

    /*
     * If we've been passed a command index (ci) that is marked
     * as static (1), then bail out.
     */
    if (ncmd[ci].ncomp)
    {
        rig_debug(RIG_DEBUG_TRACE, "%s: Attempt to modify complete sequence\n",
                  __func__);
        return -RIG_EINVAL;
    }

    /* Copy native cmd freq_set to private cmd storage area */
    memcpy(&priv->p_cmd, &ncmd[ci].nseq, YAESU_CMD_LENGTH);

    /* store bcd format in in p_cmd */
    to_bcd(priv->p_cmd, freq / 10, FT920_BCD_DIAL);

    rig_debug(RIG_DEBUG_TRACE, fmt, __func__, (int64_t)from_bcd(priv->p_cmd,
              FT920_BCD_DIAL) * 10);

    err = write_block(&rig->state.rigport, (unsigned char *) &priv->p_cmd,
                      YAESU_CMD_LENGTH);

    if (err != RIG_OK)
    {
        return err;
    }

    return RIG_OK;
}


/*
 * Private helper function to build and send a complete command to
 * change the RIT/XIT frequency.
 *
 * TODO: place variant of this in yaesu.c
 *
 * Arguments:   *rig    Valid RIG instance
 *              ci      Command index of the ncmd table
 *              rit     shortfreq_t frequency value
 *              p1      P1 value -- CLAR_SET_FREQ
 *              p2      P2 value -- CLAR_OFFSET_PLUS || CLAR_OFFSET_MINUS
 *
 * Returns:     RIG_OK if all called functions are successful,
 *              otherwise returns error from called functiion
 *
 * Assumes:     rit doesn't exceed tuning limits of rig
 */

static int ft920_send_rit_freq(RIG *rig, unsigned char ci, shortfreq_t rit)
{
    struct ft920_priv_data *priv;
    unsigned char p1;
    unsigned char p2;
    int err;

    rig_debug(RIG_DEBUG_VERBOSE, "%s called\n", __func__);

    if (!rig)
    {
        return -RIG_EINVAL;
    }

    rig_debug(RIG_DEBUG_TRACE, "%s: passed ci = %i\n", __func__, ci);
    rig_debug(RIG_DEBUG_TRACE, "%s: passed rit = %li Hz\n", __func__, rit);

    priv = (struct ft920_priv_data *)rig->state.priv;

    /*
     * If we've been passed a command index (ci) that is marked
     * as static (1), then bail out.
     */
    if (ncmd[ci].ncomp)
    {
        rig_debug(RIG_DEBUG_TRACE, "%s: Attempt to modify complete sequence\n",
                  __func__);
        return -RIG_EINVAL;
    }

    p1 = CLAR_SET_FREQ;

    if (rit < 0)
    {
        rit = labs(rit);                /* get absolute value of rit */
        p2 = CLAR_OFFSET_MINUS;
    }
    else
    {
        p2 = CLAR_OFFSET_PLUS;
    }

    /* Copy native cmd clarifier ops to private cmd storage area */
    memcpy(&priv->p_cmd, &ncmd[ci].nseq, YAESU_CMD_LENGTH);

    /* store bcd format in in p_cmd */
    to_bcd(priv->p_cmd, rit / 10, FT920_BCD_RIT);

    rig_debug(RIG_DEBUG_TRACE, "%s: requested rit after conversion = %d Hz\n",
              __func__, (int)from_bcd(priv->p_cmd, FT920_BCD_RIT) * 10);

    priv->p_cmd[P1] = p1;               /* ick */
    priv->p_cmd[P2] = p2;

    err = write_block(&rig->state.rigport, (unsigned char *) &priv->p_cmd,
                      YAESU_CMD_LENGTH);

    if (err != RIG_OK)
    {
        return err;
    }

    return RIG_OK;
}

