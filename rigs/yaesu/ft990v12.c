/*
 * hamlib - (C) Stephane Fillod 2002-2010 (fillods at users.sourceforge.net)
 *           (C) Terry Embry 2009
 *
 * ft990.c - (C) Berndt Josef Wulf (wulf at ping.net.au)
 *
 * This shared library provides an API for communicating
 * via serial interface to an FT-990 using the "CAT" interface
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

/* THIS FILE WAS MODIFIED IN DECEMBER 2016 TO REMOVE ANY REFERENCE TO THE FT-1000/D. SEPARATE ft1000d.c and .h FILES
*  WERE CREATED TO HANDLE FT-1000/D COMMANDS AND PROVIDE THE FULL RANGE OF FUNCTIONS AVAILABLE ON THE FT-1000/D
*  TO MAXIMISE COMPATIBILITY WITH RIGCTL.
*  G0OAN
*/
/* MODIFIED VERSION for FT-990 with ROM v1.2 : June 2022
 *   The standard version was written for FT-990 with ROM v1.3 and as the CAT spec was different to ROM v1.2 CAT
 *   would not work with the older ROM. This version enables ROM v1.2 to work although it is necessary to accept
 *   that frequent polling functionality is not feasible with this older ROM. With ROM v1.2 polling fetches 1492
 *   bytes which at 4800 Baud takes about 3.8 seconds during which the FT-990 has a CAT blackout. The longest poll
 *   interval available in WSJT-X is 99 seconds.
 *   Collaboration between M0EZP David Brewerton and K1MMI Edmund Hajjar
 */

#include <hamlib/config.h>

#include <stdlib.h>
#include <string.h>  /* String function definitions */

#include "hamlib/rig.h"
#include "bandplan.h"
#include "serial.h"
#include "misc.h"
#include "yaesu.h"
#include "ft990v12.h"

// FT990 native commands
enum ft990v12_native_cmd_e
{
    FT990_NATIVE_SPLIT_OFF = 0,
    FT990_NATIVE_SPLIT_ON,
    FT990_NATIVE_RECALL_MEM,
    FT990_NATIVE_VFO_TO_MEM,
    FT990_NATIVE_LOCK_OFF,
    FT990_NATIVE_LOCK_ON,
    FT990_NATIVE_VFO_A,
    FT990_NATIVE_VFO_B,
    FT990_NATIVE_MEM_TO_VFO,
    FT990_NATIVE_VFO_STEP_UP,
    FT990_NATIVE_VFO_STEP_UP_FAST,
    FT990_NATIVE_VFO_STEP_DOWN,
    FT990_NATIVE_VFO_STEP_DOWN_FAST,
    FT990_NATIVE_RX_CLARIFIER_OFF,
    FT990_NATIVE_RX_CLARIFIER_ON,
    FT990_NATIVE_TX_CLARIFIER_OFF,
    FT990_NATIVE_TX_CLARIFIER_ON,
    FT990_NATIVE_CLEAR_CLARIFIER_OFFSET,
    FT990_NATIVE_CLARIFIER_OPS,
    FT990_NATIVE_FREQ_SET,
    FT990_NATIVE_MODE_SET_LSB,
    FT990_NATIVE_MODE_SET_USB,
    FT990_NATIVE_MODE_SET_CW_W,
    FT990_NATIVE_MODE_SET_CW_N,
    FT990_NATIVE_MODE_SET_AM_W,
    FT990_NATIVE_MODE_SET_AM_N,
    FT990_NATIVE_MODE_SET_FM,
    FT990_NATIVE_MODE_SET_RTTY_LSB,
    FT990_NATIVE_MODE_SET_RTTY_USB,
    FT990_NATIVE_MODE_SET_PKT_LSB,
    FT990_NATIVE_MODE_SET_PKT_FM,
    FT990_NATIVE_PACING,
    FT990_NATIVE_PTT_OFF,
    FT990_NATIVE_PTT_ON,
    FT990_NATIVE_UPDATE_ALL_DATA,
    FT990_NATIVE_UPDATE_MEM_CHNL,
    FT990_NATIVE_UPDATE_OP_DATA,
    FT990_NATIVE_UPDATE_VFO_DATA,
    FT990_NATIVE_UPDATE_MEM_CHNL_DATA,
    FT990_NATIVE_TUNER_OFF,
    FT990_NATIVE_TUNER_ON,
    FT990_NATIVE_TUNER_START,
    FT990_NATIVE_RPTR_SHIFT_NONE,
    FT990_NATIVE_RPTR_SHIFT_MINUS,
    FT990_NATIVE_RPTR_SHIFT_PLUS,
    FT990_NATIVE_VFO_TO_VFO,
    FT990_NATIVE_BANDWIDTH,
    FT990_NATIVE_OP_FREQ_STEP_UP,
    FT990_NATIVE_OP_FREQ_STEP_DOWN,
    FT990_NATIVE_READ_METER,
    FT990_NATIVE_DIM_LEVEL,
    FT990_NATIVE_RPTR_OFFSET,
    FT990_NATIVE_READ_FLAGS,
    FT990_NATIVE_SIZE
};


/* HAMLIB API implementation */
static int ft990v12_init(RIG *rig);
static int ft990v12_cleanup(RIG *rig);
static int ft990v12_open(RIG *rig);
static int ft990v12_close(RIG *rig);
static int ft990v12_set_freq(RIG *rig, vfo_t vfo, freq_t freq);
static int ft990v12_get_freq(RIG *rig, vfo_t vfo, freq_t *freq);
static int ft990v12_set_mode(RIG *rig, vfo_t vfo, rmode_t mode,
                             pbwidth_t width);
static int ft990v12_get_mode(RIG *rig, vfo_t vfo, rmode_t *mode,
                             pbwidth_t *width);
static int ft990v12_set_vfo(RIG *rig, vfo_t vfo);
static int ft990v12_get_vfo(RIG *rig, vfo_t *vfo);
static int ft990v12_set_ptt(RIG *rig, vfo_t vfo, ptt_t ptt);
static int ft990v12_get_ptt(RIG *rig, vfo_t vfo, ptt_t *ptt);
static int ft990v12_set_rptr_shift(RIG *rig, vfo_t vfo,
                                   rptr_shift_t rptr_shift);
static int ft990v12_get_rptr_shift(RIG *rig, vfo_t vfo,
                                   rptr_shift_t *rptr_shift);
static int ft990v12_set_rptr_offs(RIG *rig, vfo_t vfo, shortfreq_t offs);
static int ft990v12_set_split_vfo(RIG *rig, vfo_t vfo, split_t split,
                                  vfo_t tx_vfo);
static int ft990v12_get_split_vfo(RIG *rig, vfo_t vfo, split_t *split,
                                  vfo_t *tx_vfo);
static int ft990v12_set_rit(RIG *rig, vfo_t vfo, shortfreq_t rit);
static int ft990v12_get_rit(RIG *rig, vfo_t vfo, shortfreq_t *rit);
static int ft990v12_set_func(RIG *rig, vfo_t vfo, setting_t func, int status);
static int ft990v12_get_func(RIG *rig, vfo_t vfo, setting_t func, int *status);
static int ft990v12_set_parm(RIG *rig, setting_t parm, value_t val);
static int ft990v12_set_xit(RIG *rig, vfo_t vfo, shortfreq_t xit);
static int ft990v12_get_xit(RIG *rig, vfo_t vfo, shortfreq_t *xit);
static int ft990v12_get_level(RIG *rig, vfo_t vfo, setting_t level,
                              value_t *val);
static int ft990v12_vfo_op(RIG *rig, vfo_t vfo, vfo_op_t op);
static int ft990v12_set_mem(RIG *rig, vfo_t vfo, int ch);
static int ft990v12_get_mem(RIG *rig, vfo_t vfo, int *ch);
static int ft990v12_set_channel(RIG *rig, vfo_t vfo, const channel_t *chan);
static int ft990v12_get_channel(RIG *rig, vfo_t vfo, channel_t *chan,
                                int read_only);





/* Private helper function prototypes */
static int ft990v12_get_update_data(RIG *rig, unsigned char ci,
                                    unsigned short ch);
static int ft990v12_send_static_cmd(RIG *rig, unsigned char ci);
static int ft990v12_send_dynamic_cmd(RIG *rig, unsigned char ci,
                                     unsigned char p1, unsigned char p2,
                                     unsigned char p3, unsigned char p4);
static int ft990v12_send_dial_freq(RIG *rig, unsigned char ci, freq_t freq);
static int ft990v12_send_rit_freq(RIG *rig, unsigned char ci, shortfreq_t rit);

static const yaesu_cmd_set_t ncmd[] =
{
    /*    ci */
    { 1, { 0x00, 0x00, 0x00, 0x00, 0x01 } }, /* 00 00 Split (OFF) */
    { 1, { 0x00, 0x00, 0x00, 0x01, 0x01 } }, /* 01 01 Split (On)  */
    { 0, { 0x00, 0x00, 0x00, 0x00, 0x02 } }, /* 02 02 Recall Memory */
    { 0, { 0x00, 0x00, 0x00, 0x00, 0x03 } }, /* 03 03 Memory Operations */
    { 1, { 0x00, 0x00, 0x00, 0x00, 0x04 } }, /* 04 04 Lock (OFF) */
    { 1, { 0x00, 0x00, 0x00, 0x01, 0x04 } }, /* 05 05 Lock (ON) */
    { 1, { 0x00, 0x00, 0x00, 0x00, 0x05 } }, /* 06 06 Select VFO (A) */
    { 1, { 0x00, 0x00, 0x00, 0x01, 0x05 } }, /* 07 07 Select VFO (B) */
    { 0, { 0x00, 0x00, 0x00, 0x00, 0x06 } }, /* 08 08 Copy Memory Data to VFO A */
    { 1, { 0x00, 0x00, 0x00, 0x00, 0x07 } }, /* 09 09 OP Freq Up 0.1MHz */
    { 1, { 0x00, 0x00, 0x01, 0x00, 0x07 } }, /* 10 0a OP Freq Up 1MHz */
    { 1, { 0x00, 0x00, 0x00, 0x00, 0x08 } }, /* 11 0b OP Freq Down 0.1MHz */
    { 1, { 0x00, 0x00, 0x01, 0x00, 0x08 } }, /* 12 0c OP Freq Down 1MHz */
    { 1, { 0x00, 0x00, 0x00, 0x00, 0x09 } }, /* 13 0d RX Clarifier (OFF) */
    { 1, { 0x00, 0x00, 0x00, 0x01, 0x09 } }, /* 14 0e RX Clarifier (ON) */
    { 1, { 0x00, 0x00, 0x00, 0x80, 0x09 } }, /* 15 0f TX Clarifier (OFF) */
    { 1, { 0x00, 0x00, 0x00, 0x81, 0x09 } }, /* 16 10 TX Clarifier (ON) */
    { 1, { 0x00, 0x00, 0x00, 0xff, 0x09 } }, /* 17 11 Clear Clarifier Offset */
    { 0, { 0x00, 0x00, 0x00, 0x00, 0x09 } }, /* 18 12 Clarifier */
    { 0, { 0x00, 0x00, 0x00, 0x00, 0x0a } }, /* 19 13 Set Op Freq */
    { 1, { 0x00, 0x00, 0x00, 0x00, 0x0c } }, /* 20 14 OP Mode Set LSB */
    { 1, { 0x00, 0x00, 0x00, 0x01, 0x0c } }, /* 21 15 OP Mode Set USB */
    { 1, { 0x00, 0x00, 0x00, 0x02, 0x0c } }, /* 22 16 OP Mode Set CW 2.4KHz */
    { 1, { 0x00, 0x00, 0x00, 0x03, 0x0c } }, /* 23 17 OP Mode Set CW 500Hz */
    { 1, { 0x00, 0x00, 0x00, 0x04, 0x0c } }, /* 24 18 OP Mode Set AM 6KHz */
    { 1, { 0x00, 0x00, 0x00, 0x05, 0x0c } }, /* 25 19 OP Mode Set AM 2.4KHz */
    { 1, { 0x00, 0x00, 0x00, 0x06, 0x0c } }, /* 26 1a OP Mode Set FM */
    { 1, { 0x00, 0x00, 0x00, 0x08, 0x0c } }, /* 27 1b OP Mode Set RTTY LSB */
    { 1, { 0x00, 0x00, 0x00, 0x09, 0x0c } }, /* 28 1c OP Mode Set RTTY USB */
    { 1, { 0x00, 0x00, 0x00, 0x0a, 0x0c } }, /* 29 1d OP Mode Set PKT LSB */
    { 1, { 0x00, 0x00, 0x00, 0x0b, 0x0c } }, /* 30 1e OP Mode Set PKT FM */
    { 0, { 0x00, 0x00, 0x00, 0x00, 0x0e } }, /* 31 1f Pacing */
    { 1, { 0x00, 0x00, 0x00, 0x00, 0x0f } }, /* 32 20 PTT (OFF) */
    { 1, { 0x00, 0x00, 0x00, 0x01, 0x0f } }, /* 33 21 PTT (ON) */
    { 1, { 0x00, 0x00, 0x00, 0x00, 0x10 } }, /* 34 22 Update All Data (1492 bytes) */
    { 1, { 0x00, 0x00, 0x00, 0x01, 0x10 } }, /* 35 23 Update Memory Ch Number M0EZP: 2ndByte was 0x01 */
    { 1, { 0x00, 0x00, 0x00, 0x02, 0x10 } }, /* 36 24 Update Op Data M0EZP: 2ndByte was 0x02 */
    { 1, { 0x00, 0x00, 0x00, 0x03, 0x10 } }, /* 37 25 Update VFO Data M0EZP: 2ndByte was 0x03 */
    { 0, { 0x00, 0x00, 0x00, 0x04, 0x10 } }, /* 38 26 Update Memory Ch Data M0EZP: 2ndByte was 0x04 */
    { 1, { 0x00, 0x00, 0x00, 0x00, 0x81 } }, /* 39 27 Tuner (OFF) */
    { 1, { 0x00, 0x00, 0x00, 0x01, 0x81 } }, /* 40 28 Tuner (ON) */
    { 1, { 0x00, 0x00, 0x00, 0x00, 0x82 } }, /* 41 29 Tuner (Start) */
    { 1, { 0x00, 0x00, 0x00, 0x00, 0x84 } }, /* 42 2a Repeater Mode (OFF) */
    { 1, { 0x00, 0x00, 0x00, 0x01, 0x84 } }, /* 43 2b Repeater Mode (Minus) */
    { 1, { 0x00, 0x00, 0x00, 0x02, 0x84 } }, /* 44 2c Repeater Mode (Plus) */
    { 1, { 0x00, 0x00, 0x00, 0x00, 0x85 } }, /* 45 2d Copy displayed VFO (A=B || B=A) */
    { 0, { 0x00, 0x00, 0x00, 0x00, 0x8C } }, /* 46 2e Select Bandwidth */
    { 1, { 0x00, 0x00, 0x00, 0x00, 0x8E } }, /* 47 2f Step Operating Frequency Up */
    { 1, { 0x00, 0x00, 0x00, 0x01, 0x8E } }, /* 48 30 Step Operating Frequency Down */
    { 1, { 0x00, 0x00, 0x00, 0x00, 0xf7 } }, /* 49 31 Read Meter */
    { 0, { 0x00, 0x00, 0x00, 0x00, 0xf8 } }, /* 50 32 DIM Level */
    { 0, { 0x00, 0x00, 0x00, 0x00, 0xf9 } }, /* 51 33 Set Offset for Repeater Shift */
    { 1, { 0x00, 0x00, 0x00, 0x00, 0xfa } }, /* 52 34 Read Status Flags */
};

/*
 * Private data
 */
// M0EZP: status 0=uni first call, 1=uni after first call
int ft990uni_get_freq_state = 0;

struct ft990v12_priv_data
{
    unsigned char pacing;                     /* pacing value */
    vfo_t current_vfo;                        /* active VFO from last cmd */
    unsigned char p_cmd[YAESU_CMD_LENGTH];    /* private copy of CAT cmd */
    ft990v12_update_data_t update_data;          /* returned data */
};

/*
 * ft990 rigs capabilities.
 */
#define FT990_MEM_CAP {          \
                .freq = 1,       \
                .mode = 1,       \
                .width = 1,      \
                .rit = 1,        \
                .xit = 1,        \
                .rptr_shift = 1, \
                .flags = 1,      \
}

// Old FT990 ROM has to read all 1492 to get frequency
// So for this model we just use the cache to read freq
const struct rig_caps ft990uni_caps =
{
    RIG_MODEL(RIG_MODEL_FT990UNI),
    .model_name =         "FT-990 Old Rom",
    .mfg_name =           "Yaesu",
    .version =            "20220628.0",
    .copyright =          "LGPL",
    .status =             RIG_STATUS_STABLE,
    .rig_type =           RIG_TYPE_TRANSCEIVER,
    .ptt_type =           RIG_PTT_RIG,
    .dcd_type =           RIG_DCD_NONE,
    .port_type =          RIG_PORT_SERIAL,
    .serial_rate_min =    4800,
    .serial_rate_max =    4800,
    .serial_data_bits =   8,
    .serial_stop_bits =   2,
    .serial_parity =      RIG_PARITY_NONE,
    .serial_handshake =   RIG_HANDSHAKE_NONE,
    .write_delay =        FT990_WRITE_DELAY,
    .post_write_delay =   FT990_POST_WRITE_DELAY,
    .timeout =            2000,
    .retry =              0,
    .has_get_func =       RIG_FUNC_LOCK | RIG_FUNC_TUNER | RIG_FUNC_MON,
    .has_set_func =       RIG_FUNC_LOCK | RIG_FUNC_TUNER,
    .has_get_level =      RIG_LEVEL_STRENGTH | RIG_LEVEL_SWR | RIG_LEVEL_ALC | \
    RIG_LEVEL_RFPOWER | RIG_LEVEL_COMP,
    .has_set_level =      RIG_LEVEL_BAND_SELECT,
    .has_get_parm =       RIG_PARM_NONE,
    .has_set_parm =       RIG_PARM_BACKLIGHT,
    .ctcss_list =         NULL,
    .dcs_list =           NULL,
    .preamp =             { RIG_DBLST_END, },
    .attenuator =         { RIG_DBLST_END, },
    .max_rit =            Hz(9999),
    .max_xit =            Hz(9999),
    .max_ifshift =        Hz(1200),
    .vfo_ops =            RIG_OP_CPY | RIG_OP_FROM_VFO | RIG_OP_TO_VFO |
    RIG_OP_UP | RIG_OP_DOWN | RIG_OP_TUNE | RIG_OP_TOGGLE,
    .targetable_vfo =     RIG_TARGETABLE_ALL,
    .transceive =         RIG_TRN_OFF,        /* Yaesus have to be polled, sigh */
    .bank_qty =           0,
    .chan_desc_sz =       0,
    .chan_list =          {
        {1, 90, RIG_MTYPE_MEM, FT990_MEM_CAP},
        RIG_CHAN_END,
    },
    .rx_range_list1 =     {
        {kHz(100), MHz(30), FT990_ALL_RX_MODES, -1, -1, FT990_VFO_ALL, FT990_ANTS},   /* General coverage + ham */
        RIG_FRNG_END,
    },

    .tx_range_list1 =     {
        FRQ_RNG_HF(1, FT990_OTHER_TX_MODES, W(5), W(100), FT990_VFO_ALL, FT990_ANTS),
        FRQ_RNG_HF(1, FT990_AM_TX_MODES, W(2), W(25), FT990_VFO_ALL, FT990_ANTS), /* AM class */
        RIG_FRNG_END,
    },

    .rx_range_list2 =     {
        {kHz(100), MHz(30), FT990_ALL_RX_MODES, -1, -1, FT990_VFO_ALL, FT990_ANTS},
        RIG_FRNG_END,
    },

    .tx_range_list2 =     {
        FRQ_RNG_HF(2, FT990_OTHER_TX_MODES, W(5), W(100), FT990_VFO_ALL, FT990_ANTS),
        FRQ_RNG_HF(2, FT990_AM_TX_MODES, W(2), W(25), FT990_VFO_ALL, FT990_ANTS), /* AM class */

        RIG_FRNG_END,
    },

    .tuning_steps =       {
        {FT990_SSB_CW_RX_MODES, Hz(10)},    /* Normal */
        {FT990_SSB_CW_RX_MODES, Hz(100)},   /* Fast */

        {FT990_AM_RX_MODES,     Hz(100)},   /* Normal */
        {FT990_AM_RX_MODES,     kHz(1)},    /* Fast */

        {FT990_FM_RX_MODES,     Hz(100)},   /* Normal */
        {FT990_FM_RX_MODES,     kHz(1)},    /* Fast */

        {FT990_RTTY_RX_MODES,   Hz(10)},    /* Normal */
        {FT990_RTTY_RX_MODES,   Hz(100)},   /* Fast */

        RIG_TS_END,

    },

    /* mode/filter list, .remember =  order matters! */
    .filters =            {
        {RIG_MODE_SSB,  RIG_FLT_ANY}, /* Enable all filters for SSB */
        {RIG_MODE_CW,   RIG_FLT_ANY}, /* Enable all filters for CW */
        {RIG_MODE_RTTY, RIG_FLT_ANY}, /* Enable all filters for RTTY */
        {RIG_MODE_RTTYR, RIG_FLT_ANY}, /* Enable all filters for Reverse RTTY */
        {RIG_MODE_PKTLSB, RIG_FLT_ANY}, /* Enable all filters for Packet Radio LSB */
        {RIG_MODE_AM,   kHz(6)},      /* normal AM filter */
        {RIG_MODE_AM,   kHz(2.4)},    /* narrow AM filter */
        {RIG_MODE_FM,   kHz(8)},      /* FM standard filter */
        {RIG_MODE_PKTFM, kHz(8)},     /* FM standard filter for Packet Radio FM */
        RIG_FLT_END,
    },

    .priv =               NULL,           /* private data FIXME: */

    .rig_init =           ft990v12_init,
    .rig_cleanup =        ft990v12_cleanup,
    .rig_open =           ft990v12_open,     /* port opened */
    .rig_close =          ft990v12_close,    /* port closed */

    .set_freq =           ft990v12_set_freq,
    .get_freq =           ft990v12_get_freq,
    .set_mode =           ft990v12_set_mode,
    .get_mode =           ft990v12_get_mode,
    .set_vfo =            ft990v12_set_vfo,
    .get_vfo =            ft990v12_get_vfo,
    .set_ptt =            ft990v12_set_ptt,
    .get_ptt =            ft990v12_get_ptt,
    .set_rptr_shift =     ft990v12_set_rptr_shift,
    .get_rptr_shift =     ft990v12_get_rptr_shift,
    .set_rptr_offs =      ft990v12_set_rptr_offs,
    .set_split_vfo =      ft990v12_set_split_vfo,
    .get_split_vfo =      ft990v12_get_split_vfo,
    .set_rit =            ft990v12_set_rit,
    .get_rit =            ft990v12_get_rit,
    .set_xit =            ft990v12_set_xit,
    .get_xit =            ft990v12_get_xit,
    .set_func =           ft990v12_set_func,
    .get_func =           ft990v12_get_func,
    .set_parm =           ft990v12_set_parm,
    .get_level =          ft990v12_get_level,
    .set_mem =            ft990v12_set_mem,
    .get_mem =            ft990v12_get_mem,
    .vfo_op =             ft990v12_vfo_op,
    .set_channel =        ft990v12_set_channel,
    .get_channel =        ft990v12_get_channel,
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
 * rig_init
 */
int ft990v12_init(RIG *rig)
{
    struct ft990v12_priv_data *priv;

    rig_debug(RIG_DEBUG_VERBOSE, "%s called\n", __func__);

    if (!rig)
    {
        return -RIG_EINVAL;
    }

    rig->state.priv = (struct ft990v12_priv_data *) calloc(1,
                      sizeof(struct ft990v12_priv_data));

    if (!rig->state.priv)
    {
        return -RIG_ENOMEM;
    }

    priv = rig->state.priv;

    // Set default pacing value
    priv->pacing = FT990_PACING_DEFAULT_VALUE;

    // Set operating vfo mode to current VFO
    priv->current_vfo =  RIG_VFO_MAIN;


    return RIG_OK;
}

/*
 * rig_cleanup
 */
int ft990v12_cleanup(RIG *rig)
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
 * rig_open
 */
int  ft990v12_open(RIG *rig)
{
    struct rig_state *rig_s;
    struct ft990v12_priv_data *priv;
    int err;

    rig_debug(RIG_DEBUG_VERBOSE, "%s called\n", __func__);

    if (!rig)
    {
        return -RIG_EINVAL;
    }

    priv = (struct ft990v12_priv_data *)rig->state.priv;
    rig_s = &rig->state;

    rig_debug(RIG_DEBUG_TRACE, "%s: write_delay = %i msec\n",
              __func__, rig_s->rigport.write_delay);
    rig_debug(RIG_DEBUG_TRACE, "%s: post_write_delay = %i msec\n",
              __func__, rig_s->rigport.post_write_delay);
    rig_debug(RIG_DEBUG_TRACE,
              "%s: read pacing = %i\n", __func__, priv->pacing);

    err = ft990v12_send_dynamic_cmd(rig, FT990_NATIVE_PACING, priv->pacing, 0, 0,
                                    0);

    if (err != RIG_OK)
    {
        return err;
    }

    // Get current rig settings and status
//    err = ft990v12_get_update_data(rig, FT990_NATIVE_UPDATE_OP_DATA, 0);
//  M0EZP read flags instead of update
    err = ft990v12_get_update_data(rig, FT990_NATIVE_READ_FLAGS, 0);

    if (err != RIG_OK)
    {
        return err;
    }

    return RIG_OK;
}

/*
 * rig_close
 */
int ft990v12_close(RIG *rig)
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
 * Set frequency for a given VFO
 *
 * Parameter     | Type   | Accepted/Expected Values
 * -------------------------------------------------------------------------
 *   RIG *      | input  | pointer to private data
 *   vfo        | input  | currVFO, VFOA, VFOB, MEM
 *   freq       | input  | 100000 - 30000000
 * -------------------------------------------------------------------------
 * Returns RIG_OK on success or an error code on failure
 *
 * Comments: Passing currVFO to vfo will use the currently selected VFO
 *           obtained from the priv->current_vfo data structure.
 *           In all other cases the passed vfo is selected if it differs
 *           from the currently selected VFO.
 */
int ft990v12_set_freq(RIG *rig, vfo_t vfo, freq_t freq)
{
    struct ft990v12_priv_data *priv;
    int err;
    vfo_t vfo_save;

    rig_debug(RIG_DEBUG_VERBOSE, "%s called\n", __func__);

    if (!rig)
    {
        return -RIG_EINVAL;
    }

    rig_debug(RIG_DEBUG_TRACE, "%s: passed vfo = 0x%02x\n", __func__, vfo);
    rig_debug(RIG_DEBUG_TRACE, "%s: passed freq = %"PRIfreq" Hz\n", __func__, freq);

    // Frequency range sanity check
    if (freq < 100000 || freq > 30000000)
    {
        return -RIG_EINVAL;
    }

    priv = (struct ft990v12_priv_data *)rig->state.priv;
    vfo_save = priv->current_vfo;

    // Set to selected VFO
    if (vfo == RIG_VFO_CURR)
    {
        vfo = priv->current_vfo;
        rig_debug(RIG_DEBUG_TRACE, "%s: priv->current.vfo = 0x%02x\n",
                  __func__, vfo);
    }
    else
    {
        if (vfo != vfo_save)
        {
            err = ft990v12_set_vfo(rig, vfo);

            if (err != RIG_OK)
            {
                return err;
            }
        }
    }

    err = ft990v12_send_dial_freq(rig, FT990_NATIVE_FREQ_SET, freq);

    if (err != RIG_OK)
    {
        return err;
    }

    if (vfo != vfo_save)
    {
        err = ft990v12_set_vfo(rig, vfo_save);

        if (err != RIG_OK)
        {
            return err;
        }
    }

    return RIG_OK;
}

/*
 * rig_get_freq*
 *
 * Get frequency for a given VFO
 *
 * Parameter     | Type   | Accepted/Expected Values
 * -------------------------------------------------------------------------
 *   RIG *      | input  | pointer to private data
 *   vfo        | input  | currVFO, Main, VFO, VFOA, VFOB, MEM
 *   freq *     | output | 100000 - 30000000
 * -------------------------------------------------------------------------
 * Returns RIG_OK on success or an error code on failure
 *
 * Comments: Passing currVFO to vfo will use the currently selected VFO
 *           obtained from the priv->current_vfo data structure.
 *           In all other cases the passed vfo is selected if it differs
 *           from the currently selected VFO.
 */
int ft990v12_get_freq(RIG *rig, vfo_t vfo, freq_t *freq)
{
    struct ft990v12_priv_data *priv;
    unsigned char *p;
    freq_t f;
    int err;
    int ci;

    rig_debug(RIG_DEBUG_VERBOSE, "%s called\n", __func__);
    rig_debug(RIG_DEBUG_TRACE, "%s: passed vfo = 0x%02x\n", __func__, vfo);
    rig_debug(RIG_DEBUG_TRACE, "%s: ft990uni_get_freq_state = 0x%02x\n", __func__,
              ft990uni_get_freq_state);

    if (ft990uni_get_freq_state < 2)
    {
        // M0EZP: UNI first call needs UPDATE_ALL
        ft990uni_get_freq_state = ft990uni_get_freq_state + 1;

        if (!rig)
        {
            return -RIG_EINVAL;
        }

        priv = (struct ft990v12_priv_data *)rig->state.priv;

        if (vfo == RIG_VFO_CURR)
        {
            vfo = priv->current_vfo;
            rig_debug(RIG_DEBUG_TRACE, "%s: priv->current.vfo = 0x%02x\n",
                      __func__, vfo);
        }

        switch (vfo)
        {
        case RIG_VFO_A:
        case RIG_VFO_VFO:
            p = priv->update_data.vfoa.basefreq;
            ci = FT990_NATIVE_UPDATE_VFO_DATA;
            break;

        case RIG_VFO_B:
            p = priv->update_data.vfob.basefreq;
            ci = FT990_NATIVE_UPDATE_VFO_DATA;
            break;

        case RIG_VFO_MEM:
        case RIG_VFO_MAIN:
            p = priv->update_data.current_front.basefreq;
            ci = FT990_NATIVE_UPDATE_OP_DATA;
            break;

        default:
            return -RIG_EINVAL;
        }

        ci = FT990_NATIVE_UPDATE_ALL_DATA; /* M0EZP: inserted to override CI */
        err = ft990v12_get_update_data(rig, ci, 0);

        if (err != RIG_OK)
        {
            return err;
        }

        /* big endian integer */
        f = ((((p[0] << 8) + p[1]) << 8) + p[2]) * 10;

        rig_debug(RIG_DEBUG_TRACE, "%s: p0=0x%02x p1=0x%02x p2=0x%02x\n",
                  __func__, p[0], p[1], p[2]);
        rig_debug(RIG_DEBUG_TRACE,
                  "%s: freq = %"PRIfreq" Hz for vfo 0x%02x\n", __func__, f, vfo);

        // Frequency sanity check
        if (f < 100000 || f > 30000000)
        {
            return -RIG_EINVAL;
        }

        *freq = f;

        return RIG_OK;
    }
    else
    {
        // M0EZP: Uni use cache
// *freq = vfo == RIG_VFO_A ? rig->state.cache.freqMainA : rig->state.cache.freqMainB;
        return (RIG_OK);
    }
}

/*
 * rig_set_ptt*
 *
 * Control PTT for a given VFO
 *
 * Parameter     | Type   | Accepted/Expected Values
 * -------------------------------------------------------------------------
 *   RIG *      | input  | pointer to private data
 *   vfo        | input  | currVFO, VFOA, VFOB, MEM
 *   ptt        | input  | 0 = off, 1 = off
 * -------------------------------------------------------------------------
 * Returns RIG_OK on success or an error code on failure
 *
 * Comments: Passing currVFO to vfo will use the currently selected VFO
 *           obtained from the priv->current_vfo data structure.
 *           In all other cases the passed vfo is selected if it differs
 *           from the currently selected VFO.
 */
int ft990v12_set_ptt(RIG *rig, vfo_t vfo, ptt_t ptt)
{
    struct ft990v12_priv_data *priv;
    int err;
    unsigned char ci;

    rig_debug(RIG_DEBUG_VERBOSE, "%s called\n", __func__);

    if (!rig)
    {
        return -RIG_EINVAL;
    }

    rig_debug(RIG_DEBUG_TRACE, "%s: passed vfo = 0x%02x\n", __func__, vfo);
    rig_debug(RIG_DEBUG_TRACE, "%s: passed ptt = 0x%02x\n", __func__, ptt);

    priv = (struct ft990v12_priv_data *) rig->state.priv;

    // Set to selected VFO
    if (vfo == RIG_VFO_CURR)
    {
        vfo = priv->current_vfo;
        rig_debug(RIG_DEBUG_TRACE, "%s: priv->current.vfo = 0x%02x\n",
                  __func__, vfo);
    }
    else
    {
        if (vfo != priv->current_vfo)
        {
            err = ft990v12_set_vfo(rig, vfo);

            if (err != RIG_OK)
            {
                return err;
            }
        }
    }

    switch (ptt)
    {
    case RIG_PTT_ON:
        ci = FT990_NATIVE_PTT_ON;
        break;

    case RIG_PTT_OFF:
        ci = FT990_NATIVE_PTT_OFF;
        break;

    default:
        return -RIG_EINVAL;
    }

    err = ft990v12_send_static_cmd(rig, ci);

    if (err != RIG_OK)
    {
        return err;
    }

    return RIG_OK;
}

/*
 * rig_get_ptt*
 *
 * Get PTT line status
 *
 * Parameter     | Type   | Accepted/Expected Values
 * -------------------------------------------------------------------------
 *   RIG *      | input  | pointer to private data
 *   vfo        | input  | currVFO, Main, VFO, VFOA, VFOB, MEM
 *   ptt *      | output | 0 = off, 1 = on
 * -------------------------------------------------------------------------
 * Returns RIG_OK on success or an error code on failure
 *
 * Comments: The passed value for the vfo is ignored since the PTT status
 *           is independent from the VFO selection.
 */
int ft990v12_get_ptt(RIG *rig, vfo_t vfo, ptt_t *ptt)
{
    struct ft990v12_priv_data *priv;
    int err;

    rig_debug(RIG_DEBUG_VERBOSE, "%s called\n", __func__);

    if (!rig)
    {
        return -RIG_EINVAL;
    }

    rig_debug(RIG_DEBUG_TRACE, "%s: passed vfo = 0x%02x\n", __func__, vfo);

    priv = (struct ft990v12_priv_data *) rig->state.priv;

    err = ft990v12_get_update_data(rig, FT990_NATIVE_READ_FLAGS, 0);

    if (err != RIG_OK)
    {
        return err;
    }

    *ptt = ((priv->update_data.flag1 & FT990_SF_XMIT) != 0);

    rig_debug(RIG_DEBUG_TRACE, "%s: set ptt = 0x%02x\n", __func__, *ptt);

    return RIG_OK;
}

/*
 * rig_set_rptr_shift*
 *
 * Set repeater shift for a given VFO
 *
 * Parameter     | Type   | Accepted/Expected Values
 * -------------------------------------------------------------------------
 *   RIG *      | input  | pointer to private data
 *   vfo        | input  | currVFO, VFOA, VFOB, MEM
 *   freq       | input  | - = negative repeater shift,
 *              |        | + = positive repeater shift,
 *              |        | any other character = simplex (is this a bug?)
 * -------------------------------------------------------------------------
 * Returns RIG_OK on success or an error code on failure
 *
 * Comments: Passing currVFO to vfo will use the currently selected VFO
 *           obtained from the priv->current_vfo data structure.
 *           In all other cases the passed vfo is selected if it differs
 *           from the currently selected VFO.
 *           Repeater shift can only be set when in FM mode.
 */
int ft990v12_set_rptr_shift(RIG *rig, vfo_t vfo, rptr_shift_t rptr_shift)
{
    struct ft990v12_priv_data *priv;
    unsigned char ci;
    char *p;
    int err;

    rig_debug(RIG_DEBUG_VERBOSE, "%s called\n", __func__);

    if (!rig)
    {
        return -RIG_EINVAL;
    }

    rig_debug(RIG_DEBUG_TRACE, "%s: passed vfo = 0x%02x\n", __func__, vfo);
    rig_debug(RIG_DEBUG_TRACE, "%s: passed rptr_shift = 0x%02x\n", __func__,
              rptr_shift);

    priv = (struct ft990v12_priv_data *) rig->state.priv;

    // Set to selected VFO
    if (vfo == RIG_VFO_CURR)
    {
        vfo = priv->current_vfo;
        rig_debug(RIG_DEBUG_TRACE, "%s: priv->current.vfo = 0x%02x\n",
                  __func__, vfo);
    }
    else
    {
        if (vfo != priv->current_vfo)
        {
            err = ft990v12_set_vfo(rig, vfo);

            if (err != RIG_OK)
            {
                return err;
            }
        }
    }

    // Construct update query
    switch (vfo)
    {
    case RIG_VFO_A:
        p = (char *) &priv->update_data.vfoa.mode;
        ci = FT990_NATIVE_UPDATE_VFO_DATA;
        break;

    case RIG_VFO_B:
        p = (char *) &priv->update_data.vfob.mode;
        ci = FT990_NATIVE_UPDATE_VFO_DATA;
        break;

    case RIG_VFO_MEM:
        p = (char *) &priv->update_data.current_front.mode;
        ci = FT990_NATIVE_UPDATE_OP_DATA;
        break;

    default:
        return -RIG_EINVAL;
    }

    // Get update for selected VFO
    err = ft990v12_get_update_data(rig, ci, 0);

    if (err != RIG_OK)
    {
        return err;
    }

    rig_debug(RIG_DEBUG_TRACE, "%s: set mode = 0x%02x\n", __func__, *p);

    // Shift mode settings are only valid in FM mode
    if ((*p & FT990_MODE_FM) == 0)
    {
        return -RIG_EINVAL;
    }

    // Construct repeater shift command
    switch (rptr_shift)
    {
    case RIG_RPT_SHIFT_NONE:
        ci = FT990_NATIVE_RPTR_SHIFT_NONE;
        break;

    case RIG_RPT_SHIFT_MINUS:
        ci = FT990_NATIVE_RPTR_SHIFT_MINUS;
        break;

    case RIG_RPT_SHIFT_PLUS:
        ci = FT990_NATIVE_RPTR_SHIFT_PLUS;
        break;

    default:
        return -RIG_EINVAL;
    }

    // Set repeater shift
    err = ft990v12_send_static_cmd(rig, ci);

    if (err != RIG_OK)
    {
        return err;
    }

    return RIG_OK;
}

/*
 * rig_get_rptr_shift*
 *
 * Get repeater shift setting for a given VFO
 *
 * Parameter     | Type   | Accepted/Expected Values
 * -------------------------------------------------------------------------
 *   RIG *      | input  | pointer to private data
 *   vfo        | input  | currVFO, Main, VFO, VFOA, VFOB, MEM
 *   shift *    | output | 0 = simplex
 *              |        | 1 = negative repeater shift
 *              |        | 2 = positive repeater shift
 * -------------------------------------------------------------------------
 * Returns RIG_OK on success or an error code on failure
 *
 * Comments: Passing currVFO to vfo will use the currently selected VFO
 *           obtained from the priv->current_vfo data structure.
 *           In all other cases the passed vfo is selected if it differs
 *           from the currently selected VFO.
 *           Repeater shift can only be obtained when in FM mode.
 */
int ft990v12_get_rptr_shift(RIG *rig, vfo_t vfo, rptr_shift_t *rptr_shift)
{
    struct ft990v12_priv_data *priv;
    ft990v12_op_data_t *p;
    unsigned char ci;
    int err;

    rig_debug(RIG_DEBUG_VERBOSE, "%s called\n", __func__);

    if (!rig)
    {
        return -RIG_EINVAL;
    }

    rig_debug(RIG_DEBUG_TRACE, "%s: passed vfo = 0x%02x\n", __func__, vfo);

    priv = (struct ft990v12_priv_data *) rig->state.priv;

    if (vfo == RIG_VFO_CURR)
    {
        vfo = priv->current_vfo;
        rig_debug(RIG_DEBUG_TRACE, "%s: priv->current.vfo = 0x%02x\n",
                  __func__, vfo);
    }

    // Construct update query
    switch (vfo)
    {
    case RIG_VFO_A:
    case RIG_VFO_VFO:
        p = &priv->update_data.vfoa;
        ci = FT990_NATIVE_UPDATE_VFO_DATA;
        break;

    case RIG_VFO_B:
        p = &priv->update_data.vfob;
        ci = FT990_NATIVE_UPDATE_VFO_DATA;
        break;

    case RIG_VFO_MEM:
    case RIG_VFO_MAIN:
        p = &priv->update_data.current_front;
        ci = FT990_NATIVE_UPDATE_OP_DATA;
        break;

    default:
        return -RIG_EINVAL;
    }

    // Get update for selected VFO
    err = ft990v12_get_update_data(rig, ci, 0);

    if (err != RIG_OK)
    {
        return err;
    }

    rig_debug(RIG_DEBUG_TRACE, "%s: set mode = 0x%02x\n", __func__, p->mode);

    // Shift mode settings are only valid in FM mode
    if (p->mode & FT990_MODE_FM)
    {
        *rptr_shift = (p->status & FT990_RPT_MASK) >> 2;
    }
    else
    {
        return -RIG_EINVAL;
    }

    rig_debug(RIG_DEBUG_TRACE, "%s: set rptr shift = 0x%02x\n", __func__,
              *rptr_shift);

    return RIG_OK;
}

/*
 * rig_set_rptr_offs*
 *
 * Set repeater frequency offset for a given VFO
 *
 * Parameter     | Type   | Accepted/Expected Values
 * -------------------------------------------------------------------------
 *   RIG *      | input  | pointer to private data
 *   vfo        | input  | currVFO, VFOA, VFOB, MEM
 *   off        | input  | 0 - 199999
 * -------------------------------------------------------------------------
 * Returns RIG_OK on success or an error code on failure
 *
 * Comments: The passed value for the vfo is ignored since the
 *           repeater frequency offset is independent from the VFO selection.
 */
int ft990v12_set_rptr_offs(RIG *rig, vfo_t vfo, shortfreq_t offs)
{
    unsigned char bcd[(int) FT990_BCD_RPTR_OFFSET / 2];
    int err;

    rig_debug(RIG_DEBUG_VERBOSE, "%s called\n", __func__);

    if (!rig)
    {
        return -RIG_EINVAL;
    }

    rig_debug(RIG_DEBUG_TRACE, "%s: passed vfo = %s\n", __func__, rig_strvfo(vfo));
    rig_debug(RIG_DEBUG_TRACE, "%s: passed offs = %d\n", __func__, (int)offs);

    // Check for valid offset
    if (offs < 0 || offs > 199999)
    {
        return -RIG_EINVAL;
    }

    to_bcd(bcd, offs / 10, FT990_BCD_RPTR_OFFSET);

    rig_debug(RIG_DEBUG_TRACE,
              "%s: set bcd[0] = 0x%02x, bcd[1] = 0x%02x, bcd[2] = 0x%02x\n",
              __func__, bcd[0], bcd[1], bcd[2]);

    err = ft990v12_send_dynamic_cmd(rig, FT990_NATIVE_RPTR_OFFSET, 0,
                                    bcd[2], bcd[1], bcd[0]);

    if (err != RIG_OK)
    {
        return err;
    }

    return RIG_OK;
}

/*
 * rig_set_split_vfo*
 *
 * Set split operation for a given VFO
 *
 * Parameter     | Type   | Accepted/Expected Values
 * -------------------------------------------------------------------------
 *   RIG *      | input  | pointer to private data
 *   vfo        | input  | currVFO, VFOA, VFOB, MEM
 *   split      | input  | 0 = off, 1 = on
 *   tx_vfo     | input  | currVFO, VFOA, VFOB
 * -------------------------------------------------------------------------
 * Returns RIG_OK on success or an error code on failure
 *
 * Comments: Passing currVFO to vfo or tx_vfo will use the currently
 *           selected VFO obtained from the priv->current_vfo data structure.
 *           Only VFOA and VFOB are valid assignments for the tx_vfo.
 *           The tx_vfo is loaded first when assigning MEM to vfo to ensure
 *           the correct TX VFO is selected by the rig in split mode.
 *           An error is returned if vfo and tx_vfo are the same.
 */
int ft990v12_set_split_vfo(RIG *rig, vfo_t vfo, split_t split, vfo_t tx_vfo)
{
    struct ft990v12_priv_data *priv;
    unsigned char ci;
    int err;

    rig_debug(RIG_DEBUG_VERBOSE, "%s called\n", __func__);

    if (!rig)
    {
        return -RIG_EINVAL;
    }

    rig_debug(RIG_DEBUG_TRACE, "%s: passed vfo = 0x%02x\n", __func__, vfo);
    rig_debug(RIG_DEBUG_TRACE, "%s: passed split = 0x%02x\n", __func__, split);
    rig_debug(RIG_DEBUG_TRACE, "%s: passed tx_vfo = 0x%02x\n", __func__, tx_vfo);

    priv = (struct ft990v12_priv_data *) rig->state.priv;

    if (vfo == RIG_VFO_CURR)
    {
        vfo = priv->current_vfo;
        rig_debug(RIG_DEBUG_TRACE, "%s: vfo = priv->current.vfo = 0x%02x\n",
                  __func__, vfo);
    }

    if (tx_vfo == RIG_VFO_CURR)
    {
        tx_vfo = priv->current_vfo;
        rig_debug(RIG_DEBUG_TRACE, "%s: tx_vfo = priv->current.vfo = 0x%02x\n",
                  __func__, tx_vfo);
    }

    // RX VFO and TX VFO cannot be the same, no support for MEM as TX VFO
    if (vfo == tx_vfo || tx_vfo == RIG_VFO_MEM)
    {
        return -RIG_ENTARGET;
    }

    // Set TX VFO first if RIG_VFO_MEM selected for RX VFO
    if (vfo == RIG_VFO_MEM)
    {
        err = ft990v12_set_vfo(rig, tx_vfo);

        if (err != RIG_OK)
        {
            return err;
        }
    }

    // Set RX VFO
    err = ft990v12_set_vfo(rig, vfo);

    if (err != RIG_OK)
    {
        return err;
    }

    switch (split)
    {
    case RIG_SPLIT_ON:
        ci = FT990_NATIVE_SPLIT_ON;
        break;

    case RIG_SPLIT_OFF:
        ci = FT990_NATIVE_SPLIT_OFF;
        break;

    default:
        return -RIG_EINVAL;
    }

    err = ft990v12_send_static_cmd(rig, ci);

    if (err != RIG_OK)
    {
        return err;
    }

    return RIG_OK;
}

/*
 * rig_get_split_vfo*
 *
 * Get split mode status for a given VFO
 *
 * Parameter     | Type   | Accepted/Expected Values
 * -------------------------------------------------------------------------
 *   RIG *      | input  | pointer to private data
 *   vfo        | input  | currVFO, Main, VFO, VFOA, VFOB, MEM
 *   split *    | output | 0 = on, 1 = off
 *   tx_vfo *   | output | VFOA, VFOB
 * -------------------------------------------------------------------------
 * Returns RIG_OK on success or an error code on failure
 *
 * Comments: The passed value for the vfo is ignored in order to
 *           preserve the current split vfo system settings.
 */
int ft990v12_get_split_vfo(RIG *rig, vfo_t vfo, split_t *split, vfo_t *tx_vfo)
{
    struct ft990v12_priv_data *priv;
    int err;

    rig_debug(RIG_DEBUG_VERBOSE, "%s called\n", __func__);

    if (!rig)
    {
        return -RIG_EINVAL;
    }

    rig_debug(RIG_DEBUG_TRACE, "%s: passed vfo = 0x%02x\n", __func__, vfo);

    priv = (struct ft990v12_priv_data *) rig->state.priv;

    // Read status flags
    err = ft990v12_get_update_data(rig, FT990_NATIVE_READ_FLAGS, 0);

    if (err != RIG_OK)
    {
        return err;
    }

    // Get split mode status
    *split = priv->update_data.flag1 & FT990_SF_SPLIT;

    rig_debug(RIG_DEBUG_TRACE, "%s: set split = 0x%02x\n", __func__,
              priv->update_data.flag1);
    rig_debug(RIG_DEBUG_TRACE, "%s: set split = 0x%02x\n", __func__, *split);

    // Get transmit vfo
    switch (priv->current_vfo)
    {
    case RIG_VFO_A:
        *tx_vfo = RIG_VFO_B;
        break;

    case RIG_VFO_B:
        *tx_vfo = RIG_VFO_A;
        break;

    case RIG_VFO_MEM:
        if (priv->update_data.flag1 & FT990_SF_VFOB)
        {
            *tx_vfo = RIG_VFO_B;
        }
        else
        {
            *tx_vfo = RIG_VFO_A;
        }

        break;

    default:
        return -RIG_EINVAL;
    }

    rig_debug(RIG_DEBUG_TRACE, "%s: set tx_vfo = 0x%02x\n", __func__, *tx_vfo);

    return RIG_OK;
}

/*
 * rig_set_rit*
 *
 * Set receiver clarifier offset for a given VFO
 *
 * Parameter     | Type   | Accepted/Expected Values
 * -------------------------------------------------------------------------
 *   RIG *      | input  | pointer to private data
 *   vfo        | input  | currVFO, VFOA, VFOB, MEM
 *   rit        | input  | -9999 - 9999
 * -------------------------------------------------------------------------
 * Returns RIG_OK on success or an error code on failure
 *
 * Comments: Passing currVFO to vfo will use the currently selected VFO
 *           obtained from the priv->current_vfo data structure.
 *           In all other cases the passed vfo is selected if it differs
 *           from the currently selected VFO.
 *
 *           The following conditions are checked:
 *
 *           rit = 0 && xit enabled   -> disable rit
 *           rit = 0 && xit disabled  -> disable rit and set frequency = 0
 */
int ft990v12_set_rit(RIG *rig, vfo_t vfo, shortfreq_t rit)
{
    struct ft990v12_priv_data *priv;
    int err;

    rig_debug(RIG_DEBUG_VERBOSE, "%s called\n", __func__);

    if (!rig)
    {
        return -RIG_EINVAL;
    }

    rig_debug(RIG_DEBUG_TRACE, "%s: passed vfo = %s\n", __func__, rig_strvfo(vfo));
    rig_debug(RIG_DEBUG_TRACE, "%s: passed rit = %d\n", __func__, (int)rit);

    // Check for valid clarifier offset frequency
    if (rit < -9999 || rit > 9999)
    {
        return -RIG_EINVAL;
    }

    priv = (struct ft990v12_priv_data *) rig->state.priv;

    // Set to selected VFO
    if (vfo == RIG_VFO_CURR)
    {
        vfo = priv->current_vfo;
        rig_debug(RIG_DEBUG_TRACE, "%s: priv->current.vfo = 0x%02x\n",
                  __func__, vfo);
    }
    else
    {
        if (vfo != priv->current_vfo)
        {
            err = ft990v12_set_vfo(rig, vfo);

            if (err != RIG_OK)
            {
                return err;
            }
        }
    }

    // If rit = 0 disable RX clarifier
    if (rit == 0)
    {
        err = ft990v12_get_update_data(rig, FT990_NATIVE_UPDATE_OP_DATA, 0);

        if (err != RIG_OK)
        {
            return err;
        }

        if ((priv->update_data.current_front.status & FT990_CLAR_TX_EN) == 0)
        {
            err = ft990v12_send_static_cmd(rig, FT990_NATIVE_CLEAR_CLARIFIER_OFFSET);

            if (err != RIG_OK)
            {
                return err;
            }
        }

        // Disable RX Clarifier
        err = ft990v12_send_static_cmd(rig, FT990_NATIVE_RX_CLARIFIER_OFF);

        if (err != RIG_OK)
        {
            return err;
        }
    }
    else
    {

        // Enable RX Clarifier
        err = ft990v12_send_static_cmd(rig, FT990_NATIVE_RX_CLARIFIER_ON);

        if (err != RIG_OK)
        {
            return err;
        }

        // Set RX clarifier offset
        err = ft990v12_send_rit_freq(rig, FT990_NATIVE_CLARIFIER_OPS, rit);

        if (err != RIG_OK)
        {
            return err;
        }
    }

    return RIG_OK;
}

/*
 * rig_get_rit*
 *
 * Get receiver clarifier offset for a given VFO
 *
 * Parameter     | Type   | Accepted/Expected Values
 * -------------------------------------------------------------------------
 *   RIG *      | input  | pointer to private data
 *   vfo        | input  | currVFO, VFOA, VFOB, MEM
 *   rit *      | output | -9999 - 9999
 * -------------------------------------------------------------------------
 * Returns RIG_OK on success or an error code on failure
 *
 * Comments: Passing currVFO to vfo will use the currently selected VFO
 *           obtained from the priv->current_vfo data structure.
 *           In all other cases the passed vfo is selected if it differs
 *           from the currently selected VFO.
 */
int ft990v12_get_rit(RIG *rig, vfo_t vfo, shortfreq_t *rit)
{
    struct ft990v12_priv_data *priv;
    unsigned char ci;
    ft990v12_op_data_t *p;
    int err;

    rig_debug(RIG_DEBUG_VERBOSE, "%s called\n", __func__);

    if (!rig)
    {
        return -RIG_EINVAL;
    }

    rig_debug(RIG_DEBUG_TRACE, "%s: passed vfo = 0x%02x\n", __func__, vfo);

    priv = (struct ft990v12_priv_data *) rig->state.priv;

    if (vfo == RIG_VFO_CURR)
    {
        vfo = priv->current_vfo;
        rig_debug(RIG_DEBUG_TRACE, "%s: priv->current_vfo = 0x%02x\n",
                  __func__, vfo);
    }

    // Construct update query
    switch (vfo)
    {
    case RIG_VFO_A:
    case RIG_VFO_VFO:
        ci = FT990_NATIVE_UPDATE_VFO_DATA;
        p = (ft990v12_op_data_t *) &priv->update_data.vfoa;
        break;

    case RIG_VFO_B:
        ci = FT990_NATIVE_UPDATE_VFO_DATA;
        p = (ft990v12_op_data_t *) &priv->update_data.vfob;
        break;

    case RIG_VFO_MEM:
    case RIG_VFO_MAIN:
        ci = FT990_NATIVE_UPDATE_OP_DATA;
        p = (ft990v12_op_data_t *) &priv->update_data.current_front;
        break;

    default:
        return -RIG_EINVAL;
    }

    // Get update for selected VFO/MEM
    err = ft990v12_get_update_data(rig, ci, 0);

    if (err != RIG_OK)
    {
        return err;
    }

    // Clarifier offset is only returned when enabled
    if (p->status & FT990_CLAR_RX_EN)
    {
        *rit = (short)((p->coffset[0] << 8) | p->coffset[1]) * 10;
    }
    else
    {
        *rit = 0;
    }

    rig_debug(RIG_DEBUG_TRACE, "%s: rit freq = %li Hz\n", __func__, *rit);

    return RIG_OK;
}

/*
 * rig_set_xit*
 *
 * Set transmitter clarifier offset for a given VFO
 *
 * Parameter     | Type   | Accepted/Expected Values
 * -------------------------------------------------------------------------
 *   RIG *      | input  | pointer to private data
 *   vfo        | input  | currVFO, VFOA, VFOB, MEM
 *   xit        | input  | -9999 - 9999
 * -------------------------------------------------------------------------
 * Returns RIG_OK on success or an error code on failure
 *
 * Comments: Passing currVFO to vfo will use the currently selected VFO
 *           obtained from the priv->current_vfo data structure.
 *           In all other cases the passed vfo is selected if it differs
 *           from the currently selected VFO.
 *
 *           The following conditions are checked:
 *
 *           xit = 0 && rit enabled   -> disable xit
 *           xit = 0 && rit disabled  -> disable xit and set frequency = 0
 */
int ft990v12_set_xit(RIG *rig, vfo_t vfo, shortfreq_t xit)
{
    struct ft990v12_priv_data *priv;
    int err;

    rig_debug(RIG_DEBUG_VERBOSE, "%s called\n", __func__);

    if (!rig)
    {
        return -RIG_EINVAL;
    }

    rig_debug(RIG_DEBUG_TRACE, "%s: passed vfo = %s\n", __func__, rig_strvfo(vfo));
    rig_debug(RIG_DEBUG_TRACE, "%s: passed rit = %d\n", __func__, (int)xit);

    if (xit < -9999 || xit > 9999)
    {
        return -RIG_EINVAL;
    }

    priv = (struct ft990v12_priv_data *) rig->state.priv;

    // Set to selected VFO
    if (vfo == RIG_VFO_CURR)
    {
        vfo = priv->current_vfo;
        rig_debug(RIG_DEBUG_TRACE, "%s: priv->current.vfo = 0x%02x\n",
                  __func__, vfo);
    }
    else
    {
        if (vfo != priv->current_vfo)
        {
            err = ft990v12_set_vfo(rig, vfo);

            if (err != RIG_OK)
            {
                return err;
            }
        }
    }

    // Disable TX clarifier and return if xit = 0
    if (xit == 0)
    {
        err = ft990v12_get_update_data(rig, FT990_NATIVE_UPDATE_OP_DATA, 0);

        if (err != RIG_OK)
        {
            return err;
        }

        if ((priv->update_data.current_front.status & FT990_CLAR_RX_EN) == 0)
        {
            err = ft990v12_send_static_cmd(rig, FT990_NATIVE_CLEAR_CLARIFIER_OFFSET);

            if (err != RIG_OK)
            {
                return err;
            }
        }

        err = ft990v12_send_static_cmd(rig, FT990_NATIVE_TX_CLARIFIER_OFF);

        if (err != RIG_OK)
        {
            return err;
        }
    }
    else
    {

        // Enable TX Clarifier
        err = ft990v12_send_static_cmd(rig, FT990_NATIVE_TX_CLARIFIER_ON);

        if (err != RIG_OK)
        {
            return err;
        }

        // Set TX clarifier offset
        err = ft990v12_send_rit_freq(rig, FT990_NATIVE_CLARIFIER_OPS, xit);

        if (err != RIG_OK)
        {
            return err;
        }
    }

    return RIG_OK;
}

/*
 * rig_get_xit*
 *
 * Get transmitter clarifier offset for a given VFO
 *
 * Parameter     | Type   | Accepted/Expected Values
 * -------------------------------------------------------------------------
 *   RIG *      | input  | pointer to private data
 *   vfo        | input  | currVFO, VFOA, VFOB, MEM
 *   xit *      | output | -9999 - 9999
 * -------------------------------------------------------------------------
 * Returns RIG_OK on success or an error code on failure
 *
 * Comments: Passing currVFO to vfo will use the currently selected VFO
 *           obtained from the priv->current_vfo data structure.
 *           In all other cases the passed vfo is selected if it differs
 *           from the currently selected VFO.
 */
int ft990v12_get_xit(RIG *rig, vfo_t vfo, shortfreq_t *xit)
{
    struct ft990v12_priv_data *priv;
    unsigned char ci;
    ft990v12_op_data_t *p;
    int err;

    rig_debug(RIG_DEBUG_VERBOSE, "%s called\n", __func__);

    if (!rig)
    {
        return -RIG_EINVAL;
    }

    rig_debug(RIG_DEBUG_TRACE, "%s: passed vfo = 0x%02x\n", __func__, vfo);

    priv = (struct ft990v12_priv_data *) rig->state.priv;

    if (vfo == RIG_VFO_CURR)
    {
        vfo = priv->current_vfo;
        rig_debug(RIG_DEBUG_TRACE, "%s: priv->current_vfo = 0x%02x\n",
                  __func__, vfo);
    }

    switch (vfo)
    {
    case RIG_VFO_A:
    case RIG_VFO_VFO:
        ci = FT990_NATIVE_UPDATE_VFO_DATA;
        p = (ft990v12_op_data_t *) &priv->update_data.vfoa;
        break;

    case RIG_VFO_B:
        ci = FT990_NATIVE_UPDATE_VFO_DATA;
        p = (ft990v12_op_data_t *) &priv->update_data.vfob;
        break;

    case RIG_VFO_MEM:
    case RIG_VFO_MAIN:
        ci = FT990_NATIVE_UPDATE_OP_DATA;
        p = (ft990v12_op_data_t *) &priv->update_data.current_front;
        break;

    default:
        return -RIG_EINVAL;
    }

    err = ft990v12_get_update_data(rig, ci, 0);

    if (err != RIG_OK)
    {
        return err;
    }

    // Clarifier offset is only returned when enabled
    if (p->status & FT990_CLAR_TX_EN)
    {
        *xit = (short)((p->coffset[0] << 8) | p->coffset[1]) * 10;
    }
    else
    {
        *xit = 0;
    }

    rig_debug(RIG_DEBUG_TRACE, "%s: read freq = %li Hz\n", __func__, *xit);

    return RIG_OK;
}

/*
 * rig_set_func*
 *
 * Set rig function
 *
 * Parameter     | Type   | Accepted/Expected Values
 * -------------------------------------------------------------------------
 *   RIG *      | input  | pointer to private data
 *   vfo        | input  | currVFO, VFOA, VFOB, MEM
 *   func       | input  | LOCK, TUNER
 *   status     | input  | 0 = off, 1 = off
 * -------------------------------------------------------------------------
 * Returns RIG_OK on success or an error code on failure
 *
 * Comments: The passed value for the vfo is ignored since the
 *           the status of rig functions are vfo independent.
 */
int ft990v12_set_func(RIG *rig, vfo_t vfo, setting_t func, int status)
{
    unsigned char ci;
    int err;

    rig_debug(RIG_DEBUG_VERBOSE, "%s called\n", __func__);

    if (!rig)
    {
        return -RIG_EINVAL;
    }

    rig_debug(RIG_DEBUG_TRACE, "%s: passed vfo = %s\n", __func__, rig_strvfo(vfo));
    rig_debug(RIG_DEBUG_TRACE, "%s: passed func = %s\n", __func__,
              rig_strfunc(func));
    rig_debug(RIG_DEBUG_TRACE, "%s: passed status = %d\n", __func__, status);

    switch (func)
    {
    case RIG_FUNC_LOCK:
        if (status)
        {
            ci = FT990_NATIVE_LOCK_ON;
        }
        else
        {
            ci = FT990_NATIVE_LOCK_OFF;
        }

        break;

    case RIG_FUNC_TUNER:
        if (status)
        {
            ci = FT990_NATIVE_TUNER_ON;
        }
        else
        {
            ci = FT990_NATIVE_TUNER_OFF;
        }

        break;

    default:
        return -RIG_EINVAL;
    }

    err = ft990v12_send_static_cmd(rig, ci);

    if (err != RIG_OK)
    {
        return err;
    }

    return RIG_OK;
}

/*
 * rig_get_func*
 *
 * Get status of a rig function
 *
 * Parameter     | Type   | Accepted/Expected Values
 * -------------------------------------------------------------------------
 *   RIG *      | input  | pointer to private data
 *   vfo        | input  | currVFO, Main, VFO, VFOA, VFOB, MEM
 *   func       | input  | LOCK, TUNER, MON
 *   status *   | output | 0 = off, 1 = on
 * -------------------------------------------------------------------------
 * Returns RIG_OK on success or an error code on failure
 *
 * Comments: The passed value for the vfo is ignored since the
 *           the status of rig function are vfo independent.
 */
int ft990v12_get_func(RIG *rig, vfo_t vfo, setting_t func, int *status)
{
    struct ft990v12_priv_data *priv;
    int err;

    rig_debug(RIG_DEBUG_VERBOSE, "%s called\n", __func__);

    if (!rig)
    {
        return -RIG_EINVAL;
    }

    rig_debug(RIG_DEBUG_TRACE, "%s: passed func = %s\n", __func__,
              rig_strfunc(func));

    priv = (struct ft990v12_priv_data *)rig->state.priv;

    err = ft990v12_get_update_data(rig, FT990_NATIVE_READ_FLAGS, 0);

    if (err != RIG_OK)
    {
        return err;
    }

    switch (func)
    {
    case RIG_FUNC_LOCK:
        *status = ((priv->update_data.flag2 & FT990_SF_LOCKED) != 0);
        break;

    case RIG_FUNC_TUNER:
        *status = ((priv->update_data.flag3 & FT990_SF_TUNER_ON) != 0);
        break;

    case RIG_FUNC_MON:
        *status = ((priv->update_data.flag3 & FT990_SF_XMIT_MON) != 0);
        break;

    default:
        return -RIG_EINVAL;
    }

    return RIG_OK;
}

/*
 * rig_set_parm*
 *
 * Set rig parameters that are not VFO specific
 *
 * Parameter     | Type   | Accepted/Expected Values
 * -------------------------------------------------------------------------
 *   RIG *      | input  | pointer to private data
 *   parm       | input  | BACKLIGHT
 *   val        | input  | 0.0..1.0
 * -------------------------------------------------------------------------
 * Returns RIG_OK on success or an error code on failure
 *
 * Comments:
 */
int ft990v12_set_parm(RIG *rig, setting_t parm, value_t val)
{
    int err;

    rig_debug(RIG_DEBUG_VERBOSE, "%s called\n", __func__);

    if (!rig)
    {
        return -RIG_EINVAL;
    }

    rig_debug(RIG_DEBUG_TRACE, "%s: passed parm = %s\n", __func__,
              rig_strparm(parm));
    rig_debug(RIG_DEBUG_TRACE, "%s: passed val = %f\n", __func__, val.f);

    switch (parm)
    {
    case RIG_PARM_BACKLIGHT:
        err = ft990v12_send_dynamic_cmd(rig, FT990_NATIVE_DIM_LEVEL,
                                        (unsigned char)(0x0d * val.f), 0, 0, 0);
        break;

    default:
        return -RIG_EINVAL;
    }

    if (err != RIG_OK)
    {
        return err;
    }

    return RIG_OK;
}

/*
 * rig_set_mode*
 *
 * Set operating mode and passband for a given VFO
 *
 * Parameter     | Type   | Accepted/Expected Values
 * -------------------------------------------------------------------------
 *   RIG *      | input  | pointer to private data
 *   vfo        | input  | currVFO, VFOA, VFOB, MEM
 *   mode       | input  | USB, LSB, CW, AM, FM, RTTY, RTTYR, PKTLSB, PKTFM
 *   width      | input  | 2400, 2000, 500, 250 (USB)
 *              |        | 2400, 2000, 500, 250 (LSB)
 *              |        | 2400, 2000, 500, 250 (CW)
 *              |        | 2400, 2000, 500, 250 (RTTY)
 *              |        | 2400, 2000, 500, 250 (RTTYR)
 *              |        | 2400, 2000, 500, 250 (PKTLSB)
 *              |        | 6000, 2400           (AM)
 *              |        | 8000                 (FM)
 *              |        | 8000                 (PKTFM)
 * -------------------------------------------------------------------------
 * Returns RIG_OK on success or an error code on failure
 *
 * Comments: Passing currVFO to vfo will use the currently selected VFO
 *           obtained from the priv->current_vfo data structure.
 *           In all other cases the passed vfo is selected if it differs
 *           from the currently selected VFO.
 */
int ft990v12_set_mode(RIG *rig, vfo_t vfo, rmode_t mode, pbwidth_t width)
{
    struct ft990v12_priv_data *priv;
    unsigned char bw;
    unsigned char ci;
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

    priv = (struct ft990v12_priv_data *)rig->state.priv;

    // Set to selected VFO
    if (vfo == RIG_VFO_CURR)
    {
        vfo = priv->current_vfo;
        rig_debug(RIG_DEBUG_TRACE, "%s: priv->current.vfo = 0x%02x\n",
                  __func__, vfo);
    }
    else
    {
        if (vfo != priv->current_vfo)
        {
            err = ft990v12_set_vfo(rig, vfo);

            if (err != RIG_OK)
            {
                return err;
            }
        }
    }

    switch (mode)
    {
    case RIG_MODE_AM:
        if (width == rig_passband_narrow(rig, mode))
        {
            ci = FT990_NATIVE_MODE_SET_AM_N;
        }
        else if (width == rig_passband_normal(rig, mode))
        {
            ci = FT990_NATIVE_MODE_SET_AM_W;
        }
        else
        {
            return -RIG_EINVAL;
        }

        break;

    case RIG_MODE_CW:
        ci = FT990_NATIVE_MODE_SET_CW_W;
        break;

    case RIG_MODE_USB:
        ci = FT990_NATIVE_MODE_SET_USB;
        break;

    case RIG_MODE_LSB:
        ci = FT990_NATIVE_MODE_SET_LSB;
        break;

    case RIG_MODE_RTTY:
        ci = FT990_NATIVE_MODE_SET_RTTY_LSB;
        break;

    case RIG_MODE_RTTYR:
        ci = FT990_NATIVE_MODE_SET_RTTY_USB;
        break;

    case RIG_MODE_FM:
        ci = FT990_NATIVE_MODE_SET_FM;
        break;

    case RIG_MODE_PKTLSB:
        ci = FT990_NATIVE_MODE_SET_PKT_LSB;
        break;

    case RIG_MODE_PKTFM:
        ci = FT990_NATIVE_MODE_SET_PKT_FM;
        break;

    default:
        return -RIG_EINVAL;
    }

    err = ft990v12_send_static_cmd(rig, ci);

    if (err != RIG_OK)
    {
        return err;
    }

    if (ci == FT990_NATIVE_MODE_SET_AM_N ||
            ci == FT990_NATIVE_MODE_SET_AM_W ||
            ci == FT990_NATIVE_MODE_SET_FM  ||
            ci == FT990_NATIVE_MODE_SET_PKT_FM)
    {
        return RIG_OK;
    }

    if (width <= 250) { bw = FT990_BW_F250; }
    else if (width <= 500) { bw = FT990_BW_F500; }
    else if (width <= 2000) { bw = FT990_BW_F2000; }
    else { bw = FT990_BW_F2400; }

    rig_debug(RIG_DEBUG_TRACE, "%s: set bw = 0x%02x\n", __func__, bw);

    err = ft990v12_send_dynamic_cmd(rig, FT990_NATIVE_BANDWIDTH, bw, 0, 0, 0);

    if (err != RIG_OK)
    {
        return err;
    }

    return RIG_OK;
}

/*
 * rig_get_mode*
 *
 * Get operating mode and passband for a given VFO
 *
 * Parameter     | Type   | Accepted/Expected Values
 * -------------------------------------------------------------------------
 *   RIG *      | input  | pointer to private data
 *   vfo        | input  | currVFO, VFOA, VFOB, MEM
 *   mode       | input  | USB, LSB, CW, AM, FM, RTTY, RTTYR, PKTLSB, PKTFM
 *   width *    | output | 2400, 2000, 500, 250 (USB)
 *              |        | 2400, 2000, 500, 250 (LSB)
 *              |        | 2400, 2000, 500, 250 (CW)
 *              |        | 2400, 2000, 500, 250 (RTTY)
 *              |        | 2400, 2000, 500, 250 (RTTYR)
 *              |        | 2400, 2000, 500, 250 (PKTLSB)
 *              |        | 6000, 2400           (AM)
 *              |        | 8000                 (FM)
 *              |        | 8000                 (PKTFM)
 * -------------------------------------------------------------------------
 * Returns RIG_OK on success or an error code on failure
 *
 * Comments: Passing currVFO to vfo will use the currently selected VFO
 *           obtained from the priv->current_vfo data structure.
 *           In all other cases the passed vfo is selected if it differs
 *           from the currently selected VFO.
 */
int ft990v12_get_mode(RIG *rig, vfo_t vfo, rmode_t *mode, pbwidth_t *width)
{
    struct ft990v12_priv_data *priv;
    unsigned char *p;
    unsigned char *fl;
    unsigned char ci;
    int err;

    rig_debug(RIG_DEBUG_VERBOSE, "%s called\n", __func__);

    if (!rig)
    {
        return -RIG_EINVAL;
    }

    rig_debug(RIG_DEBUG_TRACE, "%s: passed vfo = 0x%02x\n", __func__, vfo);

    priv = (struct ft990v12_priv_data *)rig->state.priv;

    if (vfo == RIG_VFO_CURR)
    {
        vfo = priv->current_vfo;
        rig_debug(RIG_DEBUG_TRACE,
                  "%s: priv->current_vfo = 0x%02x\n", __func__, vfo);
    }

    switch (vfo)
    {
    case RIG_VFO_A:
    case RIG_VFO_VFO:
        p = &priv->update_data.vfoa.mode;
        ci = FT990_NATIVE_UPDATE_VFO_DATA;
        fl = &priv->update_data.vfoa.filter;
        break;

    case RIG_VFO_B:
        p = &priv->update_data.vfob.mode;
        ci = FT990_NATIVE_UPDATE_VFO_DATA;
        fl = &priv->update_data.vfob.filter;
        break;

    case RIG_VFO_MEM:
    case RIG_VFO_MAIN:
        p = &priv->update_data.current_front.mode;
        ci = FT990_NATIVE_UPDATE_OP_DATA;
        fl = &priv->update_data.current_front.filter;
        break;

    default:
        return -RIG_EINVAL;
    }

    // Get update for selected VFO
    err = ft990v12_get_update_data(rig, ci, 0);

    if (err != RIG_OK)
    {
        return err;
    }

    rig_debug(RIG_DEBUG_TRACE, "%s: fl = 0x%02x\n", __func__, *fl);
    rig_debug(RIG_DEBUG_TRACE, "%s: current mode = 0x%02x\n", __func__, *p);

    switch (*p)
    {
    case FT990_MODE_LSB:
        *mode = RIG_MODE_LSB;
        break;

    case FT990_MODE_USB:
        *mode = RIG_MODE_USB;
        break;

    case FT990_MODE_CW:
        *mode = RIG_MODE_CW;
        break;

    case FT990_MODE_AM:
        *mode = RIG_MODE_AM;
        break;

    case FT990_MODE_FM:
        *mode = RIG_MODE_FM;
        break;

    case FT990_MODE_RTTY:
        if (*fl & FT990_BW_FMPKTRTTY)
        {
            *mode = RIG_MODE_RTTYR;
        }
        else
        {
            *mode = RIG_MODE_RTTY;
        }

        break;

    case FT990_MODE_PKT:
        if (*fl & FT990_BW_FMPKTRTTY)
        {
            *mode = RIG_MODE_PKTFM;
        }
        else
        {
            *mode = RIG_MODE_PKTLSB;
        }

        break;

    default:
        return -RIG_EINVAL;
    }

    rig_debug(RIG_DEBUG_TRACE, "%s: get mode = %s\n", __func__,
              rig_strrmode(*mode));

    // The FT990 firmware appears to have a bug since the
    // AM bandwidth for 2400Hz and 6000Hz are interchanged.
    switch (*fl & (~FT990_BW_FMPKTRTTY))
    {
    case FT990_BW_F2400:
        if (*mode == RIG_MODE_FM || *mode == RIG_MODE_PKTFM)
        {
            *width = 8000;
        }
        else if (*mode == RIG_MODE_AM) // <- FT990 firmware bug?
        {
            *width = 6000;
        }
        else
        {
            *width = 2400;
        }

        break;

    case FT990_BW_F2000:
        *width = 2000;
        break;

    case FT990_BW_F500:
        *width = 500;
        break;

    case FT990_BW_F250:
        *width = 250;
        break;

    case FT990_BW_F6000:
        *width = 2400;                 // <- FT990 firmware bug?
        break;

    default:
        return -RIG_EINVAL;
    }

    rig_debug(RIG_DEBUG_TRACE, "%s: get width = %li Hz\n", __func__, *width);

    return RIG_OK;
}

/*
 * rig_set_vfo*
 *
 * Set operational VFO
 *
 * Parameter     | Type   | Accepted/Expected Values
 * -------------------------------------------------------------------------
 *   RIG *      | input  | pointer to private data
 *   vfo        | input  | currVFO, VFOA, VFOB, MEM
 * -------------------------------------------------------------------------
 * Returns RIG_OK on success or an error code on failure
 *
 * Comments: Passing currVFO to vfo will use the currently selected VFO
 *           obtained from the priv->current_vfo data structure.
 *           In all other cases the passed vfo is selected if it differs
 *           from the currently selected VFO.
 */
int ft990v12_set_vfo(RIG *rig, vfo_t vfo)
{
    struct ft990v12_priv_data *priv;
    unsigned char ci;
    int err;

    rig_debug(RIG_DEBUG_VERBOSE, "%s called\n", __func__);

    if (!rig)
    {
        return -RIG_EINVAL;
    }

    rig_debug(RIG_DEBUG_TRACE, "%s: passed vfo = 0x%02x\n", __func__, vfo);

    priv = (struct ft990v12_priv_data *)rig->state.priv;

    if (vfo == RIG_VFO_CURR)
    {
        vfo = priv->current_vfo;
        rig_debug(RIG_DEBUG_TRACE,
                  "%s: priv->current_vfo = 0x%02x\n", __func__, vfo);
    }

    switch (vfo)
    {
    case RIG_VFO_A:
        ci = FT990_NATIVE_VFO_A;
        break;

    case RIG_VFO_B:
        ci = FT990_NATIVE_VFO_B;
        break;

    case RIG_VFO_MEM:
        ci = FT990_NATIVE_RECALL_MEM;
        break;

    default:
        return -RIG_EINVAL;
    }

    rig_debug(RIG_DEBUG_TRACE, "%s: set ci = %i\n", __func__, ci);

    if (vfo == RIG_VFO_MEM)
    {
        err = ft990v12_send_dynamic_cmd(rig, ci,
                                        priv->update_data.channelnumber + 1, 0, 0, 0);

        rig_debug(RIG_DEBUG_TRACE, "%s: set mem channel = 0x%02x\n",
                  __func__, priv->update_data.channelnumber + 1);
    }
    else
    {
        err = ft990v12_send_static_cmd(rig, ci);
    }

    if (err != RIG_OK)
    {
        return err;
    }

    priv->current_vfo = vfo;

    return RIG_OK;
}

/*
 * rig_get_vfo*
 *
 * Get operational VFO
 *
 * Parameter     | Type   | Accepted/Expected Values
 * -------------------------------------------------------------------------
 *   RIG *      | input  | pointer to private data
 *   vfo *      | output | VFOA, VFOB, MEM
 * -------------------------------------------------------------------------
 * Returns RIG_OK on success or an error code on failure
 *
 * Comments: Passing currVFO to vfo will use the currently selected VFO
 *           obtained from the priv->current_vfo data structure.
 *           In all other cases the passed vfo is selected if it differs
 *           from the currently selected VFO.
 *           The result is stored in the priv->current_vfo data structure
 *           for later retrieval.
 */
int ft990v12_get_vfo(RIG *rig, vfo_t *vfo)
{
    struct ft990v12_priv_data *priv;

    rig_debug(RIG_DEBUG_VERBOSE, "%s called\n", __func__);

    if (!rig)
    {
        return -RIG_EINVAL;
    }

    priv = (struct ft990v12_priv_data *)rig->state.priv;

    /* Get flags for VFO status
    err = ft990v12_get_update_data(rig, FT990_NATIVE_READ_FLAGS, 0);

    if (err != RIG_OK)
    {
        return err;
    }
    */

    if (priv->update_data.flag2 & FT990_SF_MEM ||
            priv->update_data.flag2 & FT990_SF_MTUNE)
    {
        priv->current_vfo = RIG_VFO_MEM;
    }
    else  if (priv->update_data.flag1 & FT990_SF_VFOB)
    {
        priv->current_vfo = RIG_VFO_B;
    }
    else
    {
        priv->current_vfo = RIG_VFO_A;
    }

    rig_debug(RIG_DEBUG_TRACE,
              "%s: vfo status_1 = 0x%02x\n", __func__,
              priv->update_data.flag1);
    rig_debug(RIG_DEBUG_TRACE,
              "%s: vfo status_2 = 0x%02x\n", __func__,
              priv->update_data.flag2);
    rig_debug(RIG_DEBUG_TRACE,
              "%s: stat_vfo = 0x%02x\n", __func__, priv->current_vfo);

    *vfo = priv->current_vfo;

    return RIG_OK;
}

/*
 * rig_get_level
 *
 * This function will read the meter level.The data
 * is processed depending upon selection of the level
 * parameter. The following are the currently supported
 * levels and returned value range:
 *
 * Parameter    | Type   | Accepted/Expected Values
 * -------------------------------------------------------------------------
 *   RIG *      | input  | pointer to private data
 *   vfo        | input  | currVFO, Main, VFO, VFOA, VFOB, MEM
 *   level      | input  | STRENGTH, ALC, COMP, RFPOWER, SWR
 *   value *    | output | see table below
 * -------------------------------------------------------------------------
 * Returns RIG_OK on success or an error code on failure
 *
 *     ----------------------------------------------------------
 *       level  | Description         | Returned Value |  Units |
 *     ----------------------------------------------------------
 *     STRENGTH | Signal Strength     | -54 .. +60     |  db    |
 *     COMP     | Compression         | 0.0 .. 1.0     | %/100  |
 *     RFPOWER  | RF Power Output     | 0.0 .. 1.0     | %/100  |
 *     SWR      | Standing Wave Ratio | 0.0 .. 1.0     | %/100  |
 *     ----------------------------------------------------------
 *
 * Comments: Passing currVFO to vfo will use the currently selected VFO
 *           obtained from the priv->current_vfo data structure.
 *           In all other cases the passed vfo is selected if it differs
 *           from the currently selected VFO.
 */
int ft990v12_get_level(RIG *rig, vfo_t vfo, setting_t level, value_t *value)
{
    struct ft990v12_priv_data *priv;
    unsigned char mdata[YAESU_CMD_LENGTH];
    int err;

    rig_debug(RIG_DEBUG_VERBOSE, "%s called\n", __func__);

    if (!rig)
    {
        return -RIG_EINVAL;
    }

    rig_debug(RIG_DEBUG_TRACE, "%s: passed vfo %s\n", __func__, rig_strvfo(vfo));
    rig_debug(RIG_DEBUG_TRACE, "%s: passed level %s\n", __func__,
              rig_strlevel(level));

    priv = (struct ft990v12_priv_data *) rig->state.priv;

    if (vfo == RIG_VFO_CURR)
    {
        vfo = priv->current_vfo;
        rig_debug(RIG_DEBUG_TRACE, "%s: priv->current_vfo 0x%02x\n",
                  __func__, vfo);
    }
    else
    {
        if (vfo != priv->current_vfo)
        {
            err = ft990v12_set_vfo(rig, vfo);

            if (err != RIG_OK)
            {
                return err;
            }
        }
    }

    err = ft990v12_send_static_cmd(rig, FT990_NATIVE_READ_METER);

    if (err != RIG_OK)
    {
        return err;
    }

    err = read_block(&rig->state.rigport, mdata, FT990_READ_METER_LENGTH);

    if (err < 0)
    {
        return err;
    }

    rig_debug(RIG_DEBUG_TRACE, "%s: meter data %d\n", __func__, mdata[0]);

    switch (level)
    {
    case RIG_LEVEL_STRENGTH:
        value->i = mdata[0] / 2.246 - 54;
        rig_debug(RIG_DEBUG_TRACE, "%s: meter level %d\n", __func__, value->i);
        break;

    case RIG_LEVEL_ALC:
    case RIG_LEVEL_COMP:
    case RIG_LEVEL_RFPOWER:
    case RIG_LEVEL_SWR:
        value->f = (float) mdata[0] / 255;
        rig_debug(RIG_DEBUG_TRACE, "%s: meter level %f\n", __func__, value->f);
        break;

    default:
        return -RIG_EINVAL;
    }

    return RIG_OK;
}

/*
 * rig_vfo_op*
 *
 * Perform vfo operations
 *
 * Parameter    | Type   | Accepted/Expected Values
 * -------------------------------------------------------------------------
 *   RIG *      | input  | pointer to private data
 *   vfo        | input  | VFOA, VFOB, MEM
 *   op         | input  | CPY       = copy from VFO to VFO
 *              |        | FROM_VFO  = copy from VFO to MEM
 *              |        | TO_VFO    = copy from MEM to VFO
 *              |        | UP        = step dial frequency up
 *              |        | DOWN      = step dial frequency down
 *              |        | TUNE      = start antenna tuner
 *              |        | TOGGLE    = toggle between VFOA and VFOB
 * -------------------------------------------------------------------------
 * Returns RIG_OK on success or an error code on failure
 *
 * Comments: Passing currVFO to vfo will use the currently selected VFO
 *           obtained from the priv->current_vfo data structure.
 *           In all other cases the passed vfo is selected if it differs
 *           from the currently selected VFO.
 */
int ft990v12_vfo_op(RIG *rig, vfo_t vfo, vfo_op_t op)
{
    struct ft990v12_priv_data *priv;
    unsigned char ci;
    int err;

    rig_debug(RIG_DEBUG_VERBOSE, "%s called\n", __func__);

    if (!rig)
    {
        return -RIG_EINVAL;
    }

    rig_debug(RIG_DEBUG_TRACE, "%s: passed vfo %s\n", __func__, rig_strvfo(vfo));
    rig_debug(RIG_DEBUG_TRACE, "%s: passed op %s\n", __func__, rig_strvfop(op));

    priv = (struct ft990v12_priv_data *) rig->state.priv;

    if (vfo == RIG_VFO_CURR)
    {
        vfo = priv->current_vfo;
        rig_debug(RIG_DEBUG_TRACE, "%s: priv->current_vfo 0x%02x\n",
                  __func__, vfo);
    }
    else
    {
        if (vfo != priv->current_vfo)
        {
            err = ft990v12_set_vfo(rig, vfo);

            if (err != RIG_OK)
            {
                return err;
            }
        }
    }

    switch (op)
    {
    case RIG_OP_CPY:
        ci = FT990_NATIVE_VFO_TO_VFO;
        break;

    case RIG_OP_FROM_VFO:
        ci = FT990_NATIVE_VFO_TO_MEM;
        break;

    case RIG_OP_TO_VFO:
        ci = FT990_NATIVE_MEM_TO_VFO;
        break;

    case RIG_OP_UP:
        ci = FT990_NATIVE_OP_FREQ_STEP_UP;
        break;

    case RIG_OP_DOWN:
        ci = FT990_NATIVE_OP_FREQ_STEP_DOWN;
        break;

    case RIG_OP_TUNE:
        ci = FT990_NATIVE_TUNER_START;
        break;

    case RIG_OP_TOGGLE:
        switch (vfo)
        {
        case RIG_VFO_A:
            ci = FT990_NATIVE_VFO_B;
            vfo = RIG_VFO_B;
            break;

        case RIG_VFO_B:
            ci = FT990_NATIVE_VFO_A;
            vfo = RIG_VFO_A;
            break;

        default:
            return -RIG_EINVAL;
        }

        break;

    default:
        return -RIG_EINVAL;
    }

    if (op == RIG_OP_TO_VFO || op == RIG_OP_FROM_VFO)
        err = ft990v12_send_dynamic_cmd(rig, ci,
                                        priv->update_data.channelnumber + 1, 0, 0, 0);
    else
    {
        err = ft990v12_send_static_cmd(rig, ci);
    }

    if (err != RIG_OK)
    {
        return err;
    }

    if (op == RIG_OP_TOGGLE)
    {
        priv->current_vfo = vfo;
    }

    return RIG_OK;
}

/*
 * rig_set_mem*
 *
 * Set main vfo to selected memory channel number
 *
 * Parameter     | Type   | Accepted/Expected Values
 * -------------------------------------------------------------------------
 *   RIG *      | input  | pointer to private data
 *   vfo        | input  | currVFO, VFOA, VFOB, MEM
 *   ch         | input  | 1 - 90
 * -------------------------------------------------------------------------
 * Returns RIG_OK on success or an error code on failure
 *
 * Comments: The passed value for the vfo is ignored since the
 *           the channel selection is vfo independent.
 */
int ft990v12_set_mem(RIG *rig, vfo_t vfo, int ch)
{
    struct ft990v12_priv_data *priv;
    int err;

    rig_debug(RIG_DEBUG_VERBOSE, "%s called\n", __func__);

    if (!rig)
    {
        return -RIG_EINVAL;
    }

    rig_debug(RIG_DEBUG_TRACE, "%s: passed ch = %i\n", __func__, ch);

    priv = (struct ft990v12_priv_data *) rig->state.priv;

    // Check for valid channel number
    if (ch < 1 || ch > 90)
    {
        return -RIG_EINVAL;
    }

    // Recall selected memory channel
    err = ft990v12_send_dynamic_cmd(rig, FT990_NATIVE_RECALL_MEM, ch, 0, 0, 0);

    if (err != RIG_OK)
    {
        return err;
    }

    priv->current_vfo = RIG_VFO_MEM;
    priv->update_data.channelnumber = ch - 1;

    return RIG_OK;
}

/*
 * rig_get_mem*
 *
 * Get memory channel number used by main vfo
 *
 * Parameter     | Type   | Accepted/Expected Values
 * -------------------------------------------------------------------------
 *   RIG *      | input  | pointer to private data
 *   vfo        | input  | currVFO, VFOA, VFOB, MEM
 *   ch *       | output | 1 - 90
 * -------------------------------------------------------------------------
 * Returns RIG_OK on success or an error code on failure
 *
 * Comments: The passed value for the vfo is ignored since
 *           the channel selection is vfo independent.
 */
int ft990v12_get_mem(RIG *rig, vfo_t vfo, int *ch)
{
    struct ft990v12_priv_data *priv;
    int err;

    rig_debug(RIG_DEBUG_VERBOSE, "%s called\n", __func__);

    if (!rig)
    {
        return -RIG_EINVAL;
    }

    rig_debug(RIG_DEBUG_TRACE, "%s: passed vfo = 0x%02x\n", __func__, vfo);

    priv = (struct ft990v12_priv_data *) rig->state.priv;

    if (vfo == RIG_VFO_CURR)
    {
        vfo = priv->current_vfo;
        rig_debug(RIG_DEBUG_TRACE, "%s: priv->current_vfo = 0x%02x\n",
                  __func__, vfo);
    }

    err = ft990v12_get_update_data(rig, FT990_NATIVE_UPDATE_MEM_CHNL, 0);

    if (err != RIG_OK)
    {
        return err;
    }

    rig_debug(RIG_DEBUG_TRACE, "%s: channel number %i\n", __func__,
              priv->update_data.channelnumber + 1);

    *ch = priv->update_data.channelnumber + 1;

    // Check for valid channel number
    if (*ch < 1 || *ch > 90)
    {
        return -RIG_EINVAL;
    }

    return RIG_OK;
}

/*
 * rig_set_channel*
 *
 * Set memory channel parameters and attributes
 *
 * Parameter    | Type   | Accepted/Expected Values
 * -------------------------------------------------------------------------
 *   RIG *      | input  | pointer to private data
 *   chan *     | input  | channel attribute data structure
 * -------------------------------------------------------------------------
 * Returns RIG_OK on success or an error code on failure
 */
int ft990v12_set_channel(RIG *rig, vfo_t vfo, const channel_t *chan)
{
    rig_debug(RIG_DEBUG_VERBOSE, "%s called\n", __func__);

    if (!rig)
    {
        return -RIG_EINVAL;
    }

    return -RIG_ENIMPL;
}

/*
 * rig_get_channel*
 *
 * Get memory channel parameters and attributes
 *
 * Parameter    | Type   | Accepted/Expected Values
 * -------------------------------------------------------------------------
 *   RIG *      | input  | pointer to private data
 *   chan *     | input  | (chan->vfo) currVFO, VFOA, VFOB, MEM
 *              |        | (chan->channel_num) 0 - 90
 *   chan *     | output | channel attributes data structure
 * -------------------------------------------------------------------------
 * Returns RIG_OK on success or an error code on failure
 *
 * Comments: Passing a memory channel number of 0 returns information on
 *           the current channel or channel last in use.
 *
 *           Status for split operation, active rig functions and tuning steps
 *           are only relevant for currVFO
 */
int ft990v12_get_channel(RIG *rig, vfo_t vfo, channel_t *chan, int read_only)
{
    struct ft990v12_priv_data *priv;
    ft990v12_op_data_t *p;
    char ci;
    int err;
    channel_t _chan;

    rig_debug(RIG_DEBUG_VERBOSE, "%s called\n", __func__);

    if (!rig)
    {
        return -RIG_EINVAL;
    }

    rig_debug(RIG_DEBUG_TRACE, "%s: passed chan->vfo = %s\n",
              __func__, rig_strvfo(chan->vfo));
    rig_debug(RIG_DEBUG_TRACE, "%s: passed chan->channel_num = %i\n",
              __func__, chan->channel_num);

    priv = (struct ft990v12_priv_data *) rig->state.priv;

    if (chan->channel_num < 0 || chan->channel_num > 90)
    {
        return -RIG_EINVAL;
    }

    /*
     * Get a clean slate so we don't have to assign value to
     * variables that are not relevant to this equipment
     */
    _chan.channel_num = chan->channel_num;
    _chan.vfo = chan->vfo;
    memset(chan, 0, sizeof(channel_t));
    chan->channel_num = _chan.channel_num;
    chan->vfo = _chan.vfo;

    if (chan->channel_num == 0)
    {
        switch (chan->vfo)
        {
        // Current or last selected memory channel
        case RIG_VFO_MEM:
            err = ft990v12_get_update_data(rig, FT990_NATIVE_UPDATE_MEM_CHNL, 0);

            if (err != RIG_OK)
            {
                return err;
            }

            chan->channel_num = priv->update_data.channelnumber + 1;
            p = (ft990v12_op_data_t *) &priv->update_data.channel[chan->channel_num];
            ci = FT990_NATIVE_UPDATE_MEM_CHNL_DATA;
            break;

        case RIG_VFO_A:
            p = (ft990v12_op_data_t *) &priv->update_data.vfoa;
            ci = FT990_NATIVE_UPDATE_VFO_DATA;
            break;

        case RIG_VFO_B:
            p = (ft990v12_op_data_t *) &priv->update_data.vfob;
            ci = FT990_NATIVE_UPDATE_VFO_DATA;
            break;

        case RIG_VFO_CURR:
            p = (ft990v12_op_data_t *) &priv->update_data.current_front;
            ci = FT990_NATIVE_UPDATE_OP_DATA;
            break;

        default:
            return -RIG_EINVAL;
        }
    }
    else
    {
        p = (ft990v12_op_data_t *) &priv->update_data.channel[chan->channel_num];
        ci = FT990_NATIVE_UPDATE_MEM_CHNL_DATA;
        chan->vfo = RIG_VFO_MEM;
    }

    /*
     * Get data for selected VFO/MEM
     */
    err = ft990v12_get_update_data(rig, ci, chan->channel_num);

    if (err != RIG_OK)
    {
        return err;
    }

    // Blanked memory, nothing to report
    if (p->bpf & FT990_EMPTY_MEM)
    {
        return RIG_OK;
    }

    /*
     * Get RX frequency
     */
    chan->freq = ((((p->basefreq[0] << 8) + p->basefreq[1]) << 8) +
                  p->basefreq[2]) * 10;

    /*
     * Get RX operating mode
     */
    switch (p->mode)
    {
    case FT990_MODE_LSB:
        chan->mode = RIG_MODE_LSB;
        break;

    case FT990_MODE_USB:
        chan->mode = RIG_MODE_USB;
        break;

    case FT990_MODE_CW:
        chan->mode = RIG_MODE_CW;
        break;

    case FT990_MODE_AM:
        chan->mode = RIG_MODE_AM;
        break;

    case FT990_MODE_FM:
        chan->mode = RIG_MODE_FM;
        break;

    case FT990_MODE_RTTY:
        if (p->filter & FT990_BW_FMPKTRTTY)
        {
            chan->mode = RIG_MODE_RTTYR;
        }
        else
        {
            chan->mode = RIG_MODE_RTTY;
        }

        break;

    case FT990_MODE_PKT:
        if (p->filter & FT990_BW_FMPKTRTTY)
        {
            chan->mode = RIG_MODE_PKTFM;
        }
        else
        {
            chan->mode = RIG_MODE_PKTLSB;
        }

        break;

    default:
        return -RIG_EINVAL;
    }

    rig_debug(RIG_DEBUG_TRACE, "%s: mode = 0x%02x\n", __func__, p->mode);
    rig_debug(RIG_DEBUG_TRACE, "%s: filter = 0x%02x\n", __func__, p->filter);

    /*
     * Get RX bandwidth selection
     *
     * The FT990 firmware appears to have a bug since the
     * AM bandwidth for 2400Hz and 6000Hz are interchanged.
     */
    switch (p->filter & (~FT990_BW_FMPKTRTTY))
    {
    case FT990_BW_F2400:
        if (chan->mode == RIG_MODE_FM || chan->mode == RIG_MODE_PKTFM)
        {
            chan->width = 8000;
        }
        else if (chan->mode == RIG_MODE_AM) // <- FT990 firmware bug?
        {
            chan->width = 6000;
        }
        else
        {
            chan->width = 2400;
        }

        break;

    case FT990_BW_F2000:
        chan->width = 2000;
        break;

    case FT990_BW_F500:
        chan->width = 500;
        break;

    case FT990_BW_F250:
        chan->width = 250;
        break;

    case FT990_BW_F6000:
        chan->width = 2400;                 // <- FT990 firmware bug?
        break;

    default:
        return -RIG_EINVAL;
    }

    err = ft990v12_get_update_data(rig, FT990_NATIVE_READ_FLAGS, 0);

    if (err != RIG_OK)
    {
        return err;
    }

    rig_debug(RIG_DEBUG_TRACE, "%s: set status = %i\n", __func__,
              priv->update_data.flag1);

    /*
     * Status for split operation, active rig functions and tuning steps
     * are only relevant for currVFO
     */
    if (chan->vfo & RIG_VFO_CURR)
    {
        chan->split = (priv->update_data.flag1 & FT990_SF_SPLIT);

        if (priv->update_data.flag1 & FT990_SF_XMIT_MON)
        {
            chan->funcs |= RIG_FUNC_MON;
        }

        if (priv->update_data.flag1 & FT990_SF_TUNER_ON)
        {
            chan->funcs |= RIG_FUNC_TUNER;
        }

        if (priv->update_data.flag1 & FT990_SF_FAST)
        {
            if (chan->mode & (FT990_AM_RX_MODES | FT990_FM_RX_MODES))
            {
                chan->tuning_step = 1000;
            }
            else
            {
                chan->tuning_step = 100;
            }
        }
        else
        {
            if (chan->mode & (FT990_AM_RX_MODES | FT990_FM_RX_MODES))
            {
                chan->tuning_step = 100;
            }
            else
            {
                chan->tuning_step = 10;
            }
        }
    }

    /*
     *  Get RIT frequencies
     */
    if (p->status & FT990_CLAR_RX_EN)
    {
        chan->rit = (short)((p->coffset[0] << 8) | p->coffset[1]) * 10;
    }

    if (chan->split & RIG_SPLIT_ON)
    {
        // Get data for the transmit VFO
        p = (ft990v12_op_data_t *)
            &priv->update_data.current_front; /* M0EZP: was current_rear */


        /* FT1000D
        * if (RIG_MODEL_FT1000D == rig->caps->rig_model)
        *  p = (ft990v12_op_data_t *) &priv->update_data.vfob;
        *  chan->tx_freq = ((((p->basefreq[0] << 8) + p->basefreq[1]) << 8) +
        *  p->basefreq[2]) * 10;
        *
        * THIS SECTION WAS REMOVED IN DECEMBER 2016. SEE SEPARATE ft1000d.c and .h FILES
        */


        /* Get RX operating mode */
        switch (p->mode)
        {
        case FT990_MODE_LSB:
            chan->tx_mode = RIG_MODE_LSB;
            break;

        case FT990_MODE_USB:
            chan->tx_mode = RIG_MODE_USB;
            break;

        case FT990_MODE_CW:
            chan->tx_mode = RIG_MODE_CW;
            break;

        case FT990_MODE_AM:
            chan->tx_mode = RIG_MODE_AM;
            break;

        case FT990_MODE_FM:
            chan->tx_mode = RIG_MODE_FM;
            break;

        case FT990_MODE_RTTY:
            if (p->filter & FT990_BW_FMPKTRTTY)
            {
                chan->tx_mode = RIG_MODE_RTTYR;
            }
            else
            {
                chan->tx_mode = RIG_MODE_RTTY;
            }

            break;

        case FT990_MODE_PKT:
            if (p->filter & FT990_BW_FMPKTRTTY)
            {
                chan->tx_mode = RIG_MODE_PKTFM;
            }
            else
            {
                chan->tx_mode = RIG_MODE_PKTLSB;
            }

            break;

        default:
            return -RIG_EINVAL;
        }

        rig_debug(RIG_DEBUG_TRACE, "%s: set tx mode = %s\n", __func__,
                  rig_strrmode(chan->mode));
        rig_debug(RIG_DEBUG_TRACE, "%s: tx filter = 0x%02x\n", __func__, p->filter);

        /*
         * Get RX bandwidth selection
         *
         * The FT990 firmware appears to have a bug since the
         * AM bandwidth for 2400Hz and 6000Hz are interchanged.
         */
        switch (p->filter & (~FT990_BW_FMPKTRTTY))
        {
        case FT990_BW_F2400:
            if (chan->tx_mode == RIG_MODE_FM || chan->mode == RIG_MODE_PKTFM)
            {
                chan->tx_width = 8000;
            }
            else if (chan->tx_mode == RIG_MODE_AM) // <- FT990 firmware bug?
            {
                chan->tx_width = 6000;
            }
            else
            {
                chan->tx_width = 2400;
            }

            break;

        case FT990_BW_F2000:
            chan->tx_width = 2000;
            break;

        case FT990_BW_F500:
            chan->tx_width = 500;
            break;

        case FT990_BW_F250:
            chan->tx_width = 250;
            break;

        case FT990_BW_F6000:
            chan->tx_width = 2400;                 // <- FT990 firmware bug?
            break;

        default:
            return -RIG_EINVAL;
        }

        if (priv->update_data.flag1 & FT990_SF_VFOB)
        {
            if (chan->tx_vfo & (RIG_VFO_A | RIG_VFO_MEM))
            {
                chan->tx_vfo = RIG_VFO_B;
            }
            else if (chan->vfo & RIG_VFO_MEM)
            {
                chan->tx_vfo = RIG_VFO_A;
            }
            else
            {
                chan->tx_vfo = RIG_VFO_MEM;
            }
        }
        else
        {
            if (chan->vfo & RIG_VFO_A)
            {
                chan->tx_vfo = RIG_VFO_MEM;
            }
            else
            {
                chan->tx_vfo = RIG_VFO_A;
            }
        }

        /*
         *  Get XIT frequencies
         */
        if (p->status & FT990_CLAR_TX_EN)
        {
            chan->xit = (short)((p->coffset[0] << 8) | p->coffset[1]) * 10;
        }

    }
    else
    {
        /*
         *  RX/TX frequency, mode, bandwidth and vfo are identical in simplex mode
         */
        chan->tx_freq  = chan->freq;
        chan->tx_mode  = chan->mode;
        chan->tx_width = chan->width;
        chan->tx_vfo   = chan->vfo;

        /*
         *  Get XIT frequencies
         */
        if (p->status & FT990_CLAR_TX_EN)
        {
            chan->xit = (short)((p->coffset[0] << 8) | p->coffset[1]) * 10;
        }
    }

    rig_debug(RIG_DEBUG_TRACE, "%s: set status = %i\n", __func__, p->status);

    /*
     * Repeater shift only possible if transmit mode is FM
     */
    if (chan->tx_mode & RIG_MODE_FM)
    {
        chan->rptr_shift = (p->status & FT990_RPT_MASK) >> 2;
    }

    /*
     * Check for skip channel for memory channels
     */
    if (chan->vfo & RIG_VFO_MEM)
    {
        chan->flags |= RIG_CHFLAG_SKIP;
    }

    if (!read_only)
    {
        // Set rig to channel values
        rig_debug(RIG_DEBUG_ERR,
                  "%s: please contact hamlib mailing list to implement this\n", __func__);
        rig_debug(RIG_DEBUG_ERR,
                  "%s: need to know if rig updates when channel read or not\n", __func__);
        return -RIG_ENIMPL;
    }

    return RIG_OK;
}

/*
 * Private helper function. Retrieves update data from rig.
 * using pacing value and buffer indicated in *priv struct.
 * Extended to be command agnostic as 990 has several ways to
 * get data and several ways to return it.
 *
 * Need to use this when doing ft990_get_* stuff
 *
 * Arguments:   *rig    Valid RIG instance
 *              ci      command index
 *              rl      expected length of returned data in octets
 *
 * Returns:     RIG_OK if all called functions are successful,
 *              otherwise returns error from called functiion
 */
int ft990v12_get_update_data(RIG *rig, unsigned char ci, unsigned short ch)
{
    struct ft990v12_priv_data *priv;
    int n;
    int err = -RIG_EINTERNAL;
    int rl;
    unsigned char temp[5];
    unsigned char *p;

    rig_debug(RIG_DEBUG_VERBOSE, "%s called\n", __func__);
    rig_debug(RIG_DEBUG_TRACE, "%s: passed ci 0x%02x\n", __func__, ci);
    rig_debug(RIG_DEBUG_TRACE, "%s: passed ch 0x%02x\n", __func__, ch);

    if (!rig)
    {
        return -RIG_EINVAL;
    }

    n = 0; // K1MMI: Initialise as the only time n will be updated is for the FT990_NATIVE_ALL_DATA AND FT990_READ_FLAGS

    priv = (struct ft990v12_priv_data *)rig->state.priv;

    switch (ci)
    {
    case FT990_NATIVE_UPDATE_ALL_DATA:
    case FT990_NATIVE_UPDATE_MEM_CHNL:
    case FT990_NATIVE_UPDATE_OP_DATA:
    case FT990_NATIVE_UPDATE_VFO_DATA:
    case FT990_NATIVE_UPDATE_MEM_CHNL_DATA:
        if (ft990uni_get_freq_state < 2)
        {
            //
            if (ci == FT990_NATIVE_UPDATE_MEM_CHNL_DATA)
                // P4 = 0x01 to 0x5a for channel 1 - 90
            {
                /* err = ft990v12_send_dynamic_cmd(rig, ci, 4, 0, 0, ch);
                M0EZP: dont send command, rely on the assignment from memory below*/
            }
            else
            {
                // err = RIG_OK;  K1MMI
                err = ft990v12_send_static_cmd(rig,
                                               ci); // K1MMI: only send for ALL DATA 1492 bytes or READ FLAGS 5 bytes
            }


            if (err != RIG_OK)
            {
                return err;
            }

            switch (ci)
            {
            case FT990_NATIVE_UPDATE_ALL_DATA:
                rl = FT990_ALL_DATA_LENGTH; // K1MMI: prepare to receive 1492 bytes back
                p = (unsigned char *)
                    &priv->update_data; // K1MMI: This seems like 1492 will be saved here

                n = read_block(&rig->state.rigport, p, rl); /* M0EZP: copied here from below */
                return RIG_OK;
                break;

            case FT990_NATIVE_UPDATE_MEM_CHNL:
                // we already have the channelnumber in the previously saved 1492 bytes
                p = (unsigned char *) &priv->update_data.channelnumber;
                rl = FT990_MEM_CHNL_LENGTH; // 1
                break;

            case FT990_NATIVE_UPDATE_OP_DATA:
                // we already have the current OP and VFOA in the 1492 bytes
                p = (unsigned char *) &priv->update_data.current_front;
                rl = FT990_OP_DATA_LENGTH; // 32
                break;

            case FT990_NATIVE_UPDATE_VFO_DATA:
                // we already have the VFOA and VFOB in the 1492 bytes
                p = (unsigned char *) &priv->update_data.vfoa;
                rl = FT990_VFO_DATA_LENGTH; // 32
                break;

            case FT990_NATIVE_UPDATE_MEM_CHNL_DATA:
                // we already have the 16 structure for the memory channel number
                p = (unsigned char *) &priv->update_data.channel[ch];
                rl = FT990_MEM_CHNL_DATA_LENGTH; // 16
                break;

            default:
                // M0EZP: shouldn't be here!
                rig_debug(RIG_DEBUG_TRACE, "%s: Default clause ci 0x%02x\n", __func__,
                          ci); // M0EZP
                return -RIG_EINVAL;
            }

            ft990uni_get_freq_state = ft990uni_get_freq_state + 1;

            if (n < 0)
            {
                return n;    /* die returning read_block error */
            }

            rig_debug(RIG_DEBUG_TRACE, "%s: read %i bytes\n", __func__, n);

            memcpy(&priv->update_data, p, FT990_ALL_DATA_LENGTH);
            return RIG_OK;
        }
        else
        {
            return RIG_OK;
        }

    case FT990_NATIVE_READ_FLAGS:
        rig_debug(RIG_DEBUG_TRACE, "%s: passed ci 0x%02x\n", __func__, ci);
        err = ft990v12_send_static_cmd(rig,
                                       ci); // K1MMI: only send for ALL DATA 1492 bytes

        if (err != RIG_OK)
        {
            return err;
        }

        p = (unsigned char *)&priv->update_data;
        rl = FT990_STATUS_FLAGS_LENGTH; // 5
        n = read_block(&rig->state.rigport, (unsigned char *)&temp,
                       rl); /* M0EZP: copied here from below */

        if (n < 0)
        {
            return n;    /* die returning read_block error */
        }

        rig_debug(RIG_DEBUG_TRACE, "%s: read %i bytes\n", __func__, n);

        memcpy(&priv->update_data, p,
               FT990_STATUS_FLAGS_LENGTH - 2); /* just overwrite first 3 bytes */

        return RIG_OK;
        break;

    default:
        // M0EZP: shouldn't be here!
        rig_debug(RIG_DEBUG_TRACE, "%s: Default clause ci 0x%02x\n", __func__,
                  ci); // M0EZP
        return -RIG_EINVAL;
    }

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
int ft990v12_send_static_cmd(RIG *rig, unsigned char ci)
{
    int err;

    rig_debug(RIG_DEBUG_VERBOSE, "%s called\n", __func__);

    if (!rig)
    {
        return -RIG_EINVAL;
    }

    if (!ncmd[ci].ncomp)
    {
        rig_debug(RIG_DEBUG_TRACE,
                  "%s: Attempt to send incomplete sequence\n", __func__);
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
int ft990v12_send_dynamic_cmd(RIG *rig, unsigned char ci,
                              unsigned char p1, unsigned char p2,
                              unsigned char p3, unsigned char p4)
{
    struct ft990v12_priv_data *priv;
    int err;

    rig_debug(RIG_DEBUG_VERBOSE, "%s called\n", __func__);

    if (!rig)
    {
        return -RIG_EINVAL;
    }

    rig_debug(RIG_DEBUG_TRACE, "%s: passed ci = 0x%02x\n", __func__, ci);
    rig_debug(RIG_DEBUG_TRACE,
              "%s: passed p1 = 0x%02x, p2 = 0x%02x, p3 = 0x%02x, p4 = 0x%02x,\n",
              __func__, p1, p2, p3, p4);

    priv = (struct ft990v12_priv_data *)rig->state.priv;

    if (ncmd[ci].ncomp)
    {
        rig_debug(RIG_DEBUG_TRACE,
                  "%s: Attempt to modify complete sequence\n", __func__);
        return -RIG_EINVAL;
    }

    memcpy(&priv->p_cmd, &ncmd[ci].nseq, YAESU_CMD_LENGTH);

    priv->p_cmd[3] = p1;
    priv->p_cmd[2] = p2;
    priv->p_cmd[1] = p3;
    priv->p_cmd[0] = p4;

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
 * change the display frequency.
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
int ft990v12_send_dial_freq(RIG *rig, unsigned char ci, freq_t freq)
{
    struct ft990v12_priv_data *priv;
    int err;
    // cppcheck-suppress *
    char *fmt = "%s: requested freq after conversion = %"PRIll" Hz\n";

    rig_debug(RIG_DEBUG_VERBOSE, "%s called\n", __func__);

    if (!rig)
    {
        return -RIG_EINVAL;
    }

    rig_debug(RIG_DEBUG_TRACE, "%s: passed ci = 0x%02x\n", __func__, ci);
    rig_debug(RIG_DEBUG_TRACE, "%s: passed freq = %"PRIfreq" Hz\n", __func__, freq);

    priv = (struct ft990v12_priv_data *)rig->state.priv;

    if (ncmd[ci].ncomp)
    {
        rig_debug(RIG_DEBUG_TRACE,
                  "%s: Attempt to modify complete sequence\n", __func__);
        return -RIG_EINVAL;
    }

    /* Copy native cmd freq_set to private cmd storage area */
    memcpy(&priv->p_cmd, &ncmd[ci].nseq, YAESU_CMD_LENGTH);

    /* store bcd format in in p_cmd */
    to_bcd(priv->p_cmd, freq / 10, FT990_BCD_DIAL);

    rig_debug(RIG_DEBUG_TRACE, fmt, __func__, (int64_t)from_bcd(priv->p_cmd,
              FT990_BCD_DIAL) * 10);

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
 * change the rit frequency.
 *
 * Arguments:   *rig    Valid RIG instance
 *              ci      Command index of the ncmd table
 *              rit    shortfreq_t frequency value
 *
 * Returns:     RIG_OK if all called functions are successful,
 *              otherwise returns error from called functiion
 */
int ft990v12_send_rit_freq(RIG *rig, unsigned char ci, shortfreq_t rit)
{
    struct ft990v12_priv_data *priv;
    int err;

    rig_debug(RIG_DEBUG_VERBOSE, "%s called\n", __func__);

    if (!rig)
    {
        return -RIG_EINVAL;
    }

    rig_debug(RIG_DEBUG_TRACE, "%s: passed ci = 0x%02x\n", __func__, ci);
    rig_debug(RIG_DEBUG_TRACE, "%s: passed rit = %li Hz\n", __func__, rit);

    priv = (struct ft990v12_priv_data *) rig->state.priv;

    if (ncmd[ci].ncomp)
    {
        rig_debug(RIG_DEBUG_TRACE, "%s: Attempt to modify complete sequence\n",
                  __func__);
        return -RIG_EINVAL;
    }

    // Copy native command into privat command storage area
    memcpy(&priv->p_cmd, &ncmd[ci].nseq, YAESU_CMD_LENGTH);

    // Reset current clarifier offset
    priv->p_cmd[3] = FT990_CLAR_CLEAR;

    // Check and set tuning direction - up or down
    if (rit < 0)
    {
        priv->p_cmd[2] = FT990_CLAR_TUNE_DOWN;
    }
    else
    {
        priv->p_cmd[2] = FT990_CLAR_TUNE_UP;
    }

    // Store bcd format into privat command storage area
    to_bcd(priv->p_cmd, labs(rit) / 10, FT990_BCD_RIT);

    err = write_block(&rig->state.rigport, (unsigned char *) &priv->p_cmd,
                      YAESU_CMD_LENGTH);

    if (err != RIG_OK)
    {
        return err;
    }

    return RIG_OK;
}

