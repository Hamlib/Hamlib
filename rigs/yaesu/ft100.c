/*
 * hamlib - (C) Frank Singleton 2000-2003
 *          (C) Stephane Fillod 2000-2010
 *
 * ft100.c - (C) Chris Karpinsky 2001 (aa1vl@arrl.net)
 * This shared library provides an API for communicating
 * via serial interface to an FT-100 using the "CAT" interface.
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

#include <hamlib/config.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>     /* String function definitions */
#include <math.h>

#include "hamlib/rig.h"
#include "serial.h"
#include "yaesu.h"
#include "ft100.h"
#include "misc.h"
#include "bandplan.h"

enum ft100_native_cmd_e
{

    FT100_NATIVE_CAT_LOCK_ON = 0,
    FT100_NATIVE_CAT_LOCK_OFF,
    FT100_NATIVE_CAT_PTT_ON,
    FT100_NATIVE_CAT_PTT_OFF,
    FT100_NATIVE_CAT_SET_FREQ,
    FT100_NATIVE_CAT_SET_MODE_LSB,
    FT100_NATIVE_CAT_SET_MODE_USB,
    FT100_NATIVE_CAT_SET_MODE_CW,
    FT100_NATIVE_CAT_SET_MODE_CWR,
    FT100_NATIVE_CAT_SET_MODE_AM,
    FT100_NATIVE_CAT_SET_MODE_FM,
    FT100_NATIVE_CAT_SET_MODE_DIG,
    FT100_NATIVE_CAT_SET_MODE_WFM,
    FT100_NATIVE_CAT_CLAR_ON,
    FT100_NATIVE_CAT_CLAR_OFF,
    FT100_NATIVE_CAT_SET_CLAR_FREQ,
    FT100_NATIVE_CAT_SET_VFOAB,
    FT100_NATIVE_CAT_SET_VFOA,
    FT100_NATIVE_CAT_SET_VFOB,
    FT100_NATIVE_CAT_SPLIT_ON,
    FT100_NATIVE_CAT_SPLIT_OFF,
    FT100_NATIVE_CAT_SET_RPT_SHIFT_MINUS,
    FT100_NATIVE_CAT_SET_RPT_SHIFT_PLUS,
    FT100_NATIVE_CAT_SET_RPT_SHIFT_SIMPLEX,
    FT100_NATIVE_CAT_SET_RPT_OFFSET,
    /* fix me */
    FT100_NATIVE_CAT_SET_DCS_ON,
    FT100_NATIVE_CAT_SET_CTCSS_ENC_ON,
    FT100_NATIVE_CAT_SET_CTCSS_ENC_DEC_ON,
    FT100_NATIVE_CAT_SET_CTCSS_DCS_OFF,
    /* em xif */
    FT100_NATIVE_CAT_SET_CTCSS_FREQ,
    FT100_NATIVE_CAT_SET_DCS_CODE,
    FT100_NATIVE_CAT_GET_RX_STATUS,
    FT100_NATIVE_CAT_GET_TX_STATUS,
    FT100_NATIVE_CAT_GET_FREQ_MODE_STATUS,
    FT100_NATIVE_CAT_PWR_WAKE,
    FT100_NATIVE_CAT_PWR_ON,
    FT100_NATIVE_CAT_PWR_OFF,
    FT100_NATIVE_CAT_READ_STATUS,
    FT100_NATIVE_CAT_READ_METERS,
    FT100_NATIVE_CAT_READ_FLAGS,
    FT100_NATIVE_SIZE     /* end marker */
};

/*
 *  we are able to get way more info
 *  than we can set
 *
 */
typedef struct
{
    unsigned char band_no;
    unsigned char freq[4];
    unsigned char mode;
    unsigned char ctcss;
    unsigned char dcs;
    unsigned char flag1;
    unsigned char flag2;
    unsigned char clarifier[2];
    unsigned char not_used;
    unsigned char step1;
    unsigned char step2;
    unsigned char filter;

    unsigned char stuffing[16];
}
FT100_STATUS_INFO;


typedef struct
{
    unsigned char mic_switch_1;
    unsigned char tx_fwd_power;
    unsigned char tx_rev_power;
    unsigned char s_meter;
    unsigned char mic_level;
    unsigned char squelch_level;
    unsigned char mic_switch_2;
    unsigned char final_temp;
    unsigned char alc_level;
}
FT100_METER_INFO;

typedef struct
{
    unsigned char byte[8];
}
FT100_FLAG_INFO;


static int ft100_init(RIG *rig);
static int ft100_open(RIG *rig);
static int ft100_cleanup(RIG *rig);
static int ft100_close(RIG *rig);

static int ft100_set_freq(RIG *rig, vfo_t vfo, freq_t freq);
static int ft100_get_freq(RIG *rig, vfo_t vfo, freq_t *freq);

static int ft100_set_mode(RIG *rig, vfo_t vfo, rmode_t mode, pbwidth_t width);
static int ft100_get_mode(RIG *rig, vfo_t vfo, rmode_t *mode, pbwidth_t *width);

static int ft100_set_vfo(RIG *rig, vfo_t vfo);
static int ft100_get_vfo(RIG *rig, vfo_t *vfo);

static int ft100_set_ptt(RIG *rig, vfo_t vfo, ptt_t ptt);
static int ft100_get_ptt(RIG *rig, vfo_t vfo, ptt_t *ptt);
static int ft100_get_level(RIG *rig, vfo_t vfo, setting_t level, value_t *val);
#if 0
static int ft100_set_level(RIG *rig, vfo_t vfo, setting_t level, value_t val);

static int ft100_set_func(RIG *rig, vfo_t vfo, setting_t func, int status);
static int ft100_get_func(RIG *rig, vfo_t vfo, setting_t func, int *status);

static int ft100_set_parm(RIG *rig, setting_t parm, value_t val);
static int ft100_get_parm(RIG *rig, setting_t parm, value_t *val);
#endif

static int ft100_set_split_vfo(RIG *rig, vfo_t vfo, split_t split,
                               vfo_t tx_vfo);
static int ft100_get_split_vfo(RIG *rig, vfo_t vfo, split_t *split,
                               vfo_t *tx_vfo);

static int ft100_set_rptr_shift(RIG *rig, vfo_t vfo, rptr_shift_t shift);
static int ft100_get_rptr_shift(RIG *rig, vfo_t vfo, rptr_shift_t *shift);

static int ft100_set_dcs_code(RIG *rig, vfo_t vfo, tone_t code);
static int ft100_get_dcs_code(RIG *rig, vfo_t vfo, tone_t *code);

static int ft100_set_ctcss_tone(RIG *rig, vfo_t vfo, tone_t tone);
static int ft100_get_ctcss_tone(RIG *rig, vfo_t vfo, tone_t *tone);

//static int ft100_get_info(RIG *rig, FT100_STATUS_INFO *ft100_status, FT100_METER_INFO *ft100_meter, FT100_FLAG_INFO *ft100_flags);

struct ft100_priv_data
{
    /* TODO: make use of cached data */
    FT100_STATUS_INFO status;
    FT100_FLAG_INFO flags;
};


/* prototypes */

static int ft100_send_priv_cmd(RIG *rig, unsigned char cmd_index);


/* Native ft100 cmd set prototypes. These are READ ONLY as each */
/* rig instance will copy from these and modify if required . */
/* Complete sequences (1) can be read and used directly as a cmd sequence . */
/* Incomplete sequences (0) must be completed with extra parameters */
/* eg: mem number, or freq etc.. */


/* kc2ivl - what works on a ft100    as of 02/27/2002        */
/*          ptt on/off                                       */
/*          set mode AM,CW,USB,LSB,FM use PKTUSB for DIG mode*/
/*          set split on/off                                 */
/*          set repeater +, - splx                           */
/*          set frequency of current vfo                     */

static const yaesu_cmd_set_t ncmd[] =
{
    { 0, { 0x00, 0x00, 0x00, 0x00, 0x00 } }, /* lock on */
    { 0, { 0x00, 0x00, 0x00, 0x00, 0x00 } }, /* lock off */

    { 1, { 0x00, 0x00, 0x00, 0x01, 0x0f } }, /* ptt on */
    { 1, { 0x00, 0x00, 0x00, 0x00, 0x0f } }, /* ptt off */

    { 0, { 0x00, 0x00, 0x00, 0x00, 0x0a } }, /* set freq */

    { 1, { 0x00, 0x00, 0x00, 0x00, 0x0c } }, /* mode set main LSB */
    { 1, { 0x00, 0x00, 0x00, 0x01, 0x0c } }, /* mode set main USB */
    { 1, { 0x00, 0x00, 0x00, 0x02, 0x0c } }, /* mode set main CW */
    { 1, { 0x00, 0x00, 0x00, 0x03, 0x0c } }, /* mode set main CWR */
    { 1, { 0x00, 0x00, 0x00, 0x04, 0x0c } }, /* mode set main AM */
    { 1, { 0x00, 0x00, 0x00, 0x06, 0x0c } }, /* mode set main FM */
    { 1, { 0x00, 0x00, 0x00, 0x05, 0x0c } }, /* mode set main DIG */
    { 1, { 0x00, 0x00, 0x00, 0x07, 0x0c } }, /* mode set main WFM */

    { 0, { 0x00, 0x00, 0x00, 0x00, 0x00 } }, /* clar on */
    { 0, { 0x00, 0x00, 0x00, 0x00, 0x00 } }, /* clar off */
    { 0, { 0x00, 0x00, 0x00, 0x00, 0x00 } }, /* set clar freq */

    { 0, { 0x00, 0x00, 0x00, 0x00, 0x05 } }, /* toggle vfo a/b */
    { 1, { 0x00, 0x00, 0x00, 0x00, 0x05 } }, /* select vfo a   */
    { 1, { 0x00, 0x00, 0x00, 0x01, 0x05 } }, /* select vfo b   */

    { 1, { 0x00, 0x00, 0x00, 0x01, 0x01 } }, /* split on */
    { 1, { 0x00, 0x00, 0x00, 0x00, 0x01 } }, /* split off */

    { 1, { 0x00, 0x00, 0x00, 0x01, 0x84 } }, /* set RPT shift MINUS */
    { 1, { 0x00, 0x00, 0x00, 0x02, 0x84 } }, /* set RPT shift PLUS */
    { 1, { 0x00, 0x00, 0x00, 0x00, 0x84 } }, /* set RPT shift SIMPLEX */
    { 0, { 0x00, 0x00, 0x00, 0x00, 0x00 } }, /* set RPT offset freq */

    /* fix me */
    { 1, { 0x00, 0x00, 0x00, 0x03, 0x92 } }, /* set DCS on */
    { 1, { 0x00, 0x00, 0x00, 0x02, 0x92 } }, /* set CTCSS/DCS enc/dec on */
    { 1, { 0x00, 0x00, 0x00, 0x01, 0x92 } }, /* set CTCSS/DCS enc on */
    { 1, { 0x00, 0x00, 0x00, 0x00, 0x92 } }, /* set CTCSS/DCS off */
    /* em xif */

    { 0, { 0x00, 0x00, 0x00, 0x00, 0x90 } }, /* set CTCSS tone */
    { 0, { 0x00, 0x00, 0x00, 0x00, 0x91 } }, /* set DCS code */

    { 0, { 0x00, 0x00, 0x00, 0x00, 0x00 } }, /* get RX status  */
    { 0, { 0x00, 0x00, 0x00, 0x00, 0x00 } }, /* get TX status  */

    { 0, { 0x00, 0x00, 0x00, 0x00, 0x10 } }, /* get FREQ and MODE status */

    { 0, { 0x00, 0x00, 0x00, 0x00, 0x00 } }, /* pwr wakeup sequence */
    { 0, { 0x00, 0x00, 0x00, 0x00, 0x00 } }, /* pwr on */
    { 0, { 0x00, 0x00, 0x00, 0x00, 0x00 } }, /* pwr off */

    { 1, { 0x00, 0x00, 0x00, 0x00, 0x10 } }, /* read status block */
    { 1, { 0x00, 0x00, 0x00, 0x00, 0xf7 } }, /* read meter block */
    { 1, { 0x00, 0x00, 0x00, 0x01, 0xfa } }  /* read flags block */
};


static tone_t ft100_ctcss_list[] =
{
    670,  693,  719,  744,  770,  797,  825,  854,  885,  915, \
    948,  974, 1000, 1035, 1072, 1109, 1148, 1188, 1230, 1273, \
    1318, 1365, 1413, 1462, 1514, 1567, 1622, 1679, 1738, 1799, \
    1738, 1799, 1862, 1928, 2035, 2107, 2181, 2257, 2336, 2418, \
    2503, 0
};

static tone_t ft100_dcs_list[] =
{
    23,  25,  26,  31,  32,  36,  43,  47,  51,  53, \
    54,  65,  71,  72,  73,  74, 114, 115, 116, 122, 125, 131, \
    132, 134, 143, 145, 152, 155, 156, 162, 165, 172, 174, 205, \
    212, 223, 225, 226, 243, 244, 245, 246, 251, 252, 255, 261, \
    263, 265, 266, 271, 274, 306, 311, 315, 325, 331, 332, 343, \
    346, 351, 356, 364, 365, 371, 411, 412, 413, 423, 431, 432, \
    445, 446, 452, 454, 455, 462, 464, 465, 466, 503, 506, 516, \
    523, 526, 532, 546, 565, 606, 612, 624, 627, 631, 632, 654, \
    662, 664, 703, 712, 723, 731, 732, 734, 743, 754, \
    0,
};
#define FT100_ALL_RX_MODES (RIG_MODE_AM|RIG_MODE_CW|RIG_MODE_CWR|RIG_MODE_USB|RIG_MODE_LSB|RIG_MODE_PKTUSB|RIG_MODE_FM)
#define FT100_SSB_CW_RX_MODES (RIG_MODE_CW|RIG_MODE_CWR|RIG_MODE_USB|RIG_MODE_LSB)
#define FT100_AM_FM_RX_MODES (RIG_MODE_AM|RIG_MODE_FM)

#define FT100_OTHER_TX_MODES (RIG_MODE_CW|RIG_MODE_CWR|RIG_MODE_USB|RIG_MODE_LSB|RIG_MODE_PKTUSB|RIG_MODE_FM)
#define FT100_AM_TX_MODES (RIG_MODE_AM)
#define FT100_GET_RIG_LEVELS (RIG_LEVEL_RAWSTR|RIG_LEVEL_RFPOWER|\
        RIG_LEVEL_SWR|RIG_LEVEL_ALC|RIG_LEVEL_MICGAIN|RIG_LEVEL_SQL)
#define FT100_FUNC_ALL (RIG_FUNC_TONE|RIG_FUNC_TSQL)

#define FT100_VFO_ALL (RIG_VFO_A|RIG_VFO_B)
#define FT100_ANT (RIG_ANT_1)

/* S-meter calibration, ascending order of RAW values */
#define FT100_STR_CAL { 9, \
            { \
            {  90,  60 }, /* +60 */ \
            { 105,  40 }, /* +40 */ \
            { 115,  20 }, /* +20 */ \
            { 120,   0 }, /*  S9 */ \
            { 130,  -6 }, /*  S8 */ \
            { 140, -12 }, /*  S7 */ \
            { 160, -18 }, /*  S6 */ \
            { 180, -24 }, /*  S5 */ \
            { 200, -54 }  /*  S0 */ \
            } }

const struct rig_caps ft100_caps =
{
    RIG_MODEL(RIG_MODEL_FT100),
    .model_name =     "FT-100",
    .mfg_name =       "Yaesu",
    .version =        "20211111.0",
    .copyright =      "LGPL",
    .status =         RIG_STATUS_STABLE,
    .rig_type =       RIG_TYPE_TRANSCEIVER,
    .ptt_type =       RIG_PTT_RIG,
    .dcd_type =       RIG_DCD_NONE,
    .port_type =      RIG_PORT_SERIAL,
    .serial_rate_min =    4800,                /* ft100/d 4800 only */
    .serial_rate_max =    4800,                /* no others allowed */
    .serial_data_bits =   8,
    .serial_stop_bits =   2,
    .serial_parity =  RIG_PARITY_NONE,
    .serial_handshake =   RIG_HANDSHAKE_NONE,
    .write_delay =    FT100_WRITE_DELAY,
    .post_write_delay =   FT100_POST_WRITE_DELAY,
    .timeout =        100,
    .retry =      0,
    .has_get_func =   RIG_FUNC_NONE,
    .has_set_func =   FT100_FUNC_ALL,
    .has_get_level =  FT100_GET_RIG_LEVELS,
    .has_set_level =  RIG_LEVEL_BAND_SELECT,
    .has_get_parm =   RIG_PARM_NONE,
    .has_set_parm =   RIG_PARM_NONE,  /* FIXME: parms */
    .level_gran =
    {
#include "level_gran_yaesu.h"
    },
    .parm_gran =      {},
    .ctcss_list =     ft100_ctcss_list,
    .dcs_list =       ft100_dcs_list,
    .preamp =         { RIG_DBLST_END, },
    .attenuator =     { RIG_DBLST_END, },
    .max_rit =        Hz(0),
    .max_xit =        Hz(0),
    .max_ifshift =    Hz(0),
    .targetable_vfo =     RIG_TARGETABLE_NONE,
    .transceive =     RIG_TRN_OFF,
    .bank_qty =       0,
    .chan_desc_sz =   0,

    .chan_list =  { RIG_CHAN_END, },  /* FIXME: memory chan .list =  78 */

    .rx_range_list1 =  {
        {kHz(100), MHz(56), FT100_ALL_RX_MODES, -1, -1, FT100_VFO_ALL},
        {MHz(76), MHz(108), RIG_MODE_WFM,      -1, -1, FT100_VFO_ALL},
        {MHz(108), MHz(154), FT100_ALL_RX_MODES, -1, -1, FT100_VFO_ALL},
        {MHz(420), MHz(470), FT100_ALL_RX_MODES, -1, -1, FT100_VFO_ALL},
        RIG_FRNG_END,
    },
    .tx_range_list1 =  {
        FRQ_RNG_HF(1, FT100_OTHER_TX_MODES, W(0.5), W(100), FT100_VFO_ALL, FT100_ANT),
        FRQ_RNG_HF(1, FT100_AM_TX_MODES,    W(0.5), W(25), FT100_VFO_ALL, FT100_ANT),
        FRQ_RNG_6m(1, FT100_OTHER_TX_MODES, W(0.5), W(100), FT100_VFO_ALL, FT100_ANT),
        FRQ_RNG_6m(1, FT100_AM_TX_MODES,    W(0.5), W(25), FT100_VFO_ALL, FT100_ANT),
        FRQ_RNG_2m(1, FT100_OTHER_TX_MODES, W(0.5), W(50), FT100_VFO_ALL, FT100_ANT),
        FRQ_RNG_2m(1, FT100_AM_TX_MODES,    W(0.5), W(12.5), FT100_VFO_ALL, FT100_ANT),
        FRQ_RNG_70cm(1, FT100_OTHER_TX_MODES, W(0.5), W(20), FT100_VFO_ALL, FT100_ANT),
        FRQ_RNG_70cm(1, FT100_AM_TX_MODES,    W(0.5), W(5),  FT100_VFO_ALL, FT100_ANT),
    },
    .rx_range_list2 =  {
        {kHz(100), MHz(56), FT100_ALL_RX_MODES, -1, -1, FT100_VFO_ALL},
        {MHz(76), MHz(108), RIG_MODE_WFM,      -1, -1, FT100_VFO_ALL},
        {MHz(108), MHz(154), FT100_ALL_RX_MODES, -1, -1, FT100_VFO_ALL},
        {MHz(420), MHz(470), FT100_ALL_RX_MODES, -1, -1, FT100_VFO_ALL},
        RIG_FRNG_END,
    },

    .tx_range_list2 =  {
        FRQ_RNG_HF(2, FT100_OTHER_TX_MODES, W(0.5), W(100), FT100_VFO_ALL, FT100_ANT),
        FRQ_RNG_HF(2, FT100_AM_TX_MODES,    W(0.5), W(25), FT100_VFO_ALL, FT100_ANT),
        FRQ_RNG_6m(2, FT100_OTHER_TX_MODES, W(0.5), W(100), FT100_VFO_ALL, FT100_ANT),
        FRQ_RNG_6m(2, FT100_AM_TX_MODES,    W(0.5), W(25), FT100_VFO_ALL, FT100_ANT),
        FRQ_RNG_2m(2, FT100_OTHER_TX_MODES, W(0.5), W(50), FT100_VFO_ALL, FT100_ANT),
        FRQ_RNG_2m(2, FT100_AM_TX_MODES,    W(0.5), W(12.5), FT100_VFO_ALL, FT100_ANT),
        FRQ_RNG_70cm(2, FT100_OTHER_TX_MODES, W(0.5), W(20), FT100_VFO_ALL, FT100_ANT),
        FRQ_RNG_70cm(2, FT100_AM_TX_MODES,    W(0.5), W(5),  FT100_VFO_ALL, FT100_ANT),
        RIG_FRNG_END,
    },

    .tuning_steps =  {
        /* FIXME */
        {FT100_SSB_CW_RX_MODES, 10},
        {FT100_SSB_CW_RX_MODES, 100},
        {FT100_AM_FM_RX_MODES, 10},
        {FT100_AM_FM_RX_MODES, 100},
        {RIG_MODE_WFM, kHz(5)},
        {RIG_MODE_WFM, kHz(50)},
        RIG_TS_END,
    },

    .filters =  {
        {RIG_MODE_SSB | RIG_MODE_CW | RIG_MODE_CWR | RIG_MODE_PKTUSB, kHz(2.4)},
        {RIG_MODE_SSB | RIG_MODE_CW | RIG_MODE_CWR | RIG_MODE_PKTUSB, Hz(300)},
        {RIG_MODE_SSB | RIG_MODE_CW | RIG_MODE_CWR | RIG_MODE_PKTUSB, Hz(500)},
        {RIG_MODE_AM | RIG_MODE_FM, kHz(6)},
        {RIG_MODE_WFM, kHz(230)},
        RIG_FLT_END,
    },
    .str_cal = FT100_STR_CAL,

    .priv =       NULL,
    .rig_init =       ft100_init,
    .rig_cleanup =    ft100_cleanup,
    .rig_open =       ft100_open,
    .rig_close =      ft100_close,
    .set_freq =       ft100_set_freq,
    .get_freq =       ft100_get_freq,
    .set_mode =       ft100_set_mode,
    .get_mode =       ft100_get_mode,
    .set_vfo =        ft100_set_vfo,
    .get_vfo =        ft100_get_vfo,
    .set_ptt =        ft100_set_ptt,
    .get_ptt =        ft100_get_ptt,
    .get_dcd =        NULL,
    .set_rptr_shift = ft100_set_rptr_shift,
    .get_rptr_shift = ft100_get_rptr_shift,
    .set_rptr_offs =  NULL,
    .get_rptr_offs =  NULL,
    .set_split_freq = NULL,
    .get_split_freq = NULL,
    .set_split_mode = NULL,
    .get_split_mode = NULL,
    .set_split_vfo =  ft100_set_split_vfo,
    .get_split_vfo =  ft100_get_split_vfo,
    .set_rit =        NULL,
    .get_rit =        NULL,
    .set_xit =        NULL,
    .get_xit =        NULL,
    .set_ts =         NULL,
    .get_ts =         NULL,
    .set_dcs_code =   ft100_set_dcs_code,
    .get_dcs_code =   ft100_get_dcs_code,
    .set_ctcss_tone = ft100_set_ctcss_tone,
    .get_ctcss_tone = ft100_get_ctcss_tone,
    .set_dcs_sql =    NULL,
    .get_dcs_sql =    NULL,
    .set_ctcss_sql =  NULL,
    .get_ctcss_sql =  NULL,
    .set_powerstat =  NULL,
    .get_powerstat =  NULL,
    .reset =          NULL,
    .set_ant =        NULL,
    .get_ant =        NULL,
    .set_level =      NULL,
    .get_level =      ft100_get_level,
    .set_func =       NULL,
    .get_func =       NULL,
    .set_parm =       NULL,
    .get_parm =       NULL,
    .hamlib_check_rig_caps = HAMLIB_CHECK_RIG_CAPS
};


int ft100_init(RIG *rig)
{
    rig_debug(RIG_DEBUG_VERBOSE, "%s called\n", __func__);

    rig->state.priv = (struct ft100_priv_data *) calloc(1,
                      sizeof(struct ft100_priv_data));

    if (!rig->state.priv) { return -RIG_ENOMEM; }

    return RIG_OK;
}

int ft100_cleanup(RIG *rig)
{
    if (!rig)
    {
        return -RIG_EINVAL;
    }

    if (rig->state.priv)
    {
        free(rig->state.priv);
    }

    rig->state.priv = NULL;

    rig_debug(RIG_DEBUG_VERBOSE, "%s: called\n", __func__);

    return RIG_OK;
}

int ft100_open(RIG *rig)
{

    rig_debug(RIG_DEBUG_VERBOSE, "%s: called\n", __func__);

    return RIG_OK;
}

int ft100_close(RIG *rig)
{

    rig_debug(RIG_DEBUG_VERBOSE, "%s: called\n", __func__);

    return RIG_OK;
}

/*
 * private helper function to send a private command
 * sequence . Must only be complete sequences.
 *
 */

static int ft100_send_priv_cmd(RIG *rig, unsigned char cmd_index)
{
    rig_debug(RIG_DEBUG_VERBOSE, "%s called (%d)\n", __func__, cmd_index);

    if (!rig) { return -RIG_EINVAL; }

    return write_block(&rig->state.rigport, (unsigned char *) &ncmd[cmd_index].nseq,
                       YAESU_CMD_LENGTH);
}

static int ft100_read_status(RIG *rig)
{
    struct ft100_priv_data *priv;
    int ret;

    rig_debug(RIG_DEBUG_VERBOSE, "%s called\n", __func__);

    priv = (struct ft100_priv_data *)rig->state.priv;

    rig_flush(&rig->state.rigport);

    ret = ft100_send_priv_cmd(rig, FT100_NATIVE_CAT_READ_STATUS);

    if (ret != RIG_OK)
    {
        return ret;
    }

    ret = read_block(&rig->state.rigport, (unsigned char *) &priv->status,
                     sizeof(FT100_STATUS_INFO));
    rig_debug(RIG_DEBUG_VERBOSE, "%s: read status=%i \n", __func__, ret);

    if (ret < 0)
    {
        return ret;
    }

    return RIG_OK;
}

static int ft100_read_flags(RIG *rig)
{
    struct ft100_priv_data *priv = (struct ft100_priv_data *)rig->state.priv;
    int ret;

    rig_debug(RIG_DEBUG_VERBOSE, "%s called\n", __func__);

    rig_flush(&rig->state.rigport);

    ret = ft100_send_priv_cmd(rig, FT100_NATIVE_CAT_READ_FLAGS);

    if (ret != RIG_OK)
    {
        return ret;
    }

    ret = read_block(&rig->state.rigport, (unsigned char *) &priv->flags,
                     sizeof(FT100_FLAG_INFO));
    rig_debug(RIG_DEBUG_VERBOSE, "%s: read flags=%i \n", __func__, ret);

    if (ret < 0)
    {
        return ret;
    }

    return RIG_OK;
}

int ft100_set_freq(RIG *rig, vfo_t vfo, freq_t freq)
{
    unsigned char p_cmd[YAESU_CMD_LENGTH];
    unsigned char cmd_index;  /* index of sequence to send */

    if (!rig) { return -RIG_EINVAL; }

    rig_debug(RIG_DEBUG_VERBOSE, "ft100: requested freq = %"PRIfreq" Hz \n", freq);

    cmd_index = FT100_NATIVE_CAT_SET_FREQ;

    memcpy(p_cmd, &ncmd[cmd_index].nseq, YAESU_CMD_LENGTH);

    /* fixed 10Hz bug by OH2MMY */
    freq = (int) freq / 10;
    to_bcd(p_cmd, freq, 8); /* store bcd format in in p_cmd */

    return write_block(&rig->state.rigport, p_cmd, YAESU_CMD_LENGTH);
}

int ft100_get_freq(RIG *rig, vfo_t vfo, freq_t *freq)
{

    struct ft100_priv_data *priv = (struct ft100_priv_data *)rig->state.priv;
    freq_t d1, d2;
    char freq_str[10];
    int ret;

    rig_debug(RIG_DEBUG_VERBOSE, "%s:\n", __func__);

    if (!freq) { return -RIG_EINVAL; }

    ret = ft100_read_status(rig);

    if (ret != RIG_OK)
    {
        return ret;
    }

    rig_debug(RIG_DEBUG_VERBOSE, "%s: Freq= %3i %3i %3i %3i \n",
              __func__,
              (int)priv->status.freq[0],
              (int)priv->status.freq[1],
              (int)priv->status.freq[2],
              (int)priv->status.freq[3]);

    /* now convert it .... */

    SNPRINTF(freq_str, sizeof(freq_str), "%02X%02X%02X%02X",
             priv->status.freq[0],
             priv->status.freq[1],
             priv->status.freq[2],
             priv->status.freq[3]);

    d1 = strtol(freq_str, NULL, 16);
    d2 = (d1 * 1.25);    /* fixed 10Hz bug by OH2MMY */

    rig_debug(RIG_DEBUG_VERBOSE, "ft100: d1=%"PRIfreq" d2=%"PRIfreq"\n", d1, d2);

    // cppcheck-suppress *
    rig_debug(RIG_DEBUG_VERBOSE, "ft100: get_freq= %8"PRIll" \n", (int64_t)d2);

    *freq = d2;

    return RIG_OK;
}

int ft100_set_mode(RIG *rig, vfo_t vfo, rmode_t mode, pbwidth_t width)
{
    unsigned char cmd_index;  /* index of sequence to send */
    int ret;

    rig_debug(RIG_DEBUG_VERBOSE, "%s: generic mode = %s, width %d\n", __func__,
              rig_strrmode(mode), (int)width);

    switch (mode)
    {
    case RIG_MODE_AM:
        cmd_index = FT100_NATIVE_CAT_SET_MODE_AM;
        break;

    case RIG_MODE_CW:
        cmd_index = FT100_NATIVE_CAT_SET_MODE_CW;
        break;

    case RIG_MODE_CWR:
        cmd_index = FT100_NATIVE_CAT_SET_MODE_CWR;
        break;

    case RIG_MODE_USB:
        cmd_index = FT100_NATIVE_CAT_SET_MODE_USB;
        break;

    case RIG_MODE_LSB:
        cmd_index = FT100_NATIVE_CAT_SET_MODE_LSB;
        break;

    case RIG_MODE_FM:
        cmd_index = FT100_NATIVE_CAT_SET_MODE_FM;
        break;

    case RIG_MODE_PKTUSB:
        cmd_index = FT100_NATIVE_CAT_SET_MODE_DIG;
        break;

    case RIG_MODE_WFM:
        cmd_index = FT100_NATIVE_CAT_SET_MODE_WFM;
        break;

    default:
        return -RIG_EINVAL;
    }

    ret = ft100_send_priv_cmd(rig, cmd_index);

    if (ret != RIG_OK)
    {
        return ret;
    }

    if (RIG_PASSBAND_NOCHANGE == width) { return ret; }

#if 1

    if (mode != RIG_MODE_FM && mode != RIG_MODE_WFM && width <= kHz(6))
    {
        unsigned char p_cmd[YAESU_CMD_LENGTH];
        p_cmd[0] = 0x00;
        p_cmd[1] = 0x00;
        p_cmd[2] = 0x00;
        p_cmd[3] = 0x00; /* to be filled in */
        p_cmd[4] = 0x8C; /* Op: filter selection */

        if (width == RIG_PASSBAND_NORMAL)
        {
            width = rig_passband_normal(rig, mode);
        }

        if (width <= 300) { p_cmd[3] = 0x03; }
        else if (width <= 500) { p_cmd[3] = 0x02; }
        else if (width <= 2400) { p_cmd[3] = 0x00; }
        else { p_cmd[3] = 0x01; }

        ret = write_block(&rig->state.rigport, p_cmd, YAESU_CMD_LENGTH);

        if (ret != RIG_OK)
        {
            return ret;
        }
    }

#endif

    return RIG_OK;
}



/*  ft100_get_mode fixed by OH2MMY
 *  Still answers wrong if on AM. Bug in rig's firmware?
 *  The rig answers something weird then, not what the manual says
 *  and the answer is different every time. Other modes do work.
 */

int ft100_get_mode(RIG *rig, vfo_t vfo, rmode_t *mode, pbwidth_t *width)
{

    struct ft100_priv_data *priv = (struct ft100_priv_data *)rig->state.priv;
    int ret;

    if (!mode || !width) { return -RIG_EINVAL; }

    ret = ft100_read_status(rig);

    if (ret < 0)
    {
        return ret;
    }

    switch (priv->status.mode & 0x0f)
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

    case 0x05:
        *mode = RIG_MODE_PKTUSB;
        break;

    case 0x06:
        *mode = RIG_MODE_FM;
        break;

    case 0x07:
        *mode = RIG_MODE_WFM;
        break;

    default:
        *mode = RIG_MODE_NONE;
    };

    switch (priv->status.mode >> 4)
    {
    case 0x00:
        *width = Hz(6000);
        break;

    case 0x01:
        *width = Hz(2400);
        break;

    case 0x02:
        *width = Hz(500);
        break;

    case 0x03:
        *width = Hz(300);
        break;

    default:
        *width = RIG_PASSBAND_NORMAL;
    };

    return RIG_OK;
}



/*  Function ft100_set_vfo fixed by OH2MMY
 */

int ft100_set_vfo(RIG *rig, vfo_t vfo)
{

    int ret;

    rig_debug(RIG_DEBUG_VERBOSE, "%s called vfo=%s\n", __func__, rig_strvfo(vfo));

    switch (vfo)
    {
    case RIG_VFO_A:
        ret = ft100_send_priv_cmd(rig, FT100_NATIVE_CAT_SET_VFOA);

        if (ret != RIG_OK)
        {
            return ret;
        }

        break;

    case RIG_VFO_B:
        ret = ft100_send_priv_cmd(rig, FT100_NATIVE_CAT_SET_VFOB);

        if (ret != RIG_OK)
        {
            return ret;
        }

        break;

    default:
        return -RIG_EINVAL;
    }

    return RIG_OK;
}



/*  Function ft100_get_vfo written again by OH2MMY
 *  Tells the right answer if in VFO mode.
 *  TODO: handle memory modes.
 */

int ft100_get_vfo(RIG *rig, vfo_t *vfo)
{

    struct ft100_priv_data *priv = (struct ft100_priv_data *)rig->state.priv;
    int ret;

    rig_debug(RIG_DEBUG_VERBOSE, "%s called\n", __func__);

    if (!vfo) { return -RIG_EINVAL; }

    ret = ft100_read_flags(rig);

    if (ret < 0)
    {
        return ret;
    }

    if ((priv->flags.byte[1] & 0x04) == 0x04)
    {
        *vfo = RIG_VFO_B;
    }
    else
    {
        *vfo = RIG_VFO_A;
    }

    return RIG_OK;
}


int ft100_set_ptt(RIG *rig, vfo_t vfo, ptt_t ptt)
{

    unsigned char cmd_index;
    int split = rig->state.cache.split;

    rig_debug(RIG_DEBUG_VERBOSE, "%s called\n", __func__);

    switch (ptt)
    {
    case RIG_PTT_ON:
        cmd_index = FT100_NATIVE_CAT_PTT_ON;

        if (split) { rig_set_vfo(rig, RIG_VFO_B); }

        break;

    case RIG_PTT_OFF:
        cmd_index = FT100_NATIVE_CAT_PTT_OFF;

        if (split) { rig_set_vfo(rig, RIG_VFO_A); }

        hl_usleep(100 *
                  1000); // give ptt some time to do its thing -- fake it was not resetting freq after tx
        break;

    default:
        return -RIG_EINVAL;
    }

    return ft100_send_priv_cmd(rig, cmd_index);
}

int ft100_get_ptt(RIG *rig, vfo_t vfo, ptt_t *ptt)
{

    struct ft100_priv_data *priv = (struct ft100_priv_data *)rig->state.priv;
    int ret;

    if (!ptt) { return -RIG_EINVAL; }

    ret = ft100_read_flags(rig);

    if (ret < 0)
    {
        return ret;
    }

    *ptt = (priv->flags.byte[0] & 0x80) == 0x80 ? RIG_PTT_ON : RIG_PTT_OFF;

    return RIG_OK;
}

/*
 * Rem: The FT-100(D) has no set_level ability
 */
int ft100_get_level(RIG *rig, vfo_t vfo, setting_t level, value_t *val)
{

    int ret;
    int split = rig->state.cache.split;
    int ptt = rig->state.cache.ptt;

    FT100_METER_INFO ft100_meter;

    if (!rig) { return -RIG_EINVAL; }

    if (!val) { return -RIG_EINVAL; }

    rig_debug(RIG_DEBUG_VERBOSE, "%s: %s\n", __func__, rig_strlevel(level));

    // if in split have to switch to VFOB to read power and back to VFOA
    if (split && ptt) { rig_set_vfo(rig, RIG_VFO_B); }

    ret = ft100_send_priv_cmd(rig, FT100_NATIVE_CAT_READ_METERS);

    if (split && ptt) { rig_set_vfo(rig, RIG_VFO_A); }

    if (ret != RIG_OK)
    {
        return ret;
    }

    ret = read_block(&rig->state.rigport, (unsigned char *) &ft100_meter,
                     sizeof(FT100_METER_INFO));
    rig_debug(RIG_DEBUG_VERBOSE, "%s: read meters=%d\n", __func__, ret);

    if (ret < 0)
    {
        return ret;
    }

    switch (level)
    {

    case RIG_LEVEL_RAWSTR:
        val->i = ft100_meter.s_meter;
        break;

    case RIG_LEVEL_RFPOWER:
        val->f = (float)ft100_meter.tx_fwd_power / 0xff;
        break;

    case RIG_LEVEL_SWR:
        if (ft100_meter.tx_fwd_power == 0)
        {
            val->f = 0;
        }
        else
        {
            float f;
            f = sqrt((float)ft100_meter.tx_rev_power / (float)ft100_meter.tx_fwd_power);
            val->f = (1 + f) / (1 - f);
        }

        break;

    case RIG_LEVEL_ALC:
        /* need conversion ? */
        val->f = (float)ft100_meter.alc_level / 0xff;
        break;

    case RIG_LEVEL_MICGAIN:
        val->f = (float)ft100_meter.mic_level / 0xff;
        break;

    case RIG_LEVEL_SQL:
        val->f = (float)ft100_meter.squelch_level / 0xff;
        break;

    default:
        return -RIG_EINVAL;
    }

    return RIG_OK;
}

#if 0
/* TODO: all of this */

int ft100_set_func(RIG *rig, vfo_t vfo, setting_t func, int status)
{
    return -RIG_ENIMPL;
}


int ft100_get_func(RIG *rig, vfo_t vfo, setting_t func, int *status)
{
    return -RIG_ENIMPL;
}


int ft100_set_parm(RIG *rig, setting_t parm, value_t val)
{
    return -RIG_ENIMPL;
}


int ft100_get_parm(RIG *rig, setting_t parm, value_t *val)
{
    return -RIG_ENIMPL;
}
#endif


/* kc2ivl */
int ft100_set_split_vfo(RIG *rig, vfo_t vfo, split_t split, vfo_t tx_vfo)
{

    unsigned char cmd_index;

    rig_debug(RIG_DEBUG_VERBOSE, "%s called\n", __func__);

    switch (split)
    {
    case RIG_SPLIT_ON:
        cmd_index = FT100_NATIVE_CAT_SPLIT_ON;
        break;

    case RIG_SPLIT_OFF:
        cmd_index = FT100_NATIVE_CAT_SPLIT_OFF;
        break;

    default:
        return -RIG_EINVAL;
    }

    return ft100_send_priv_cmd(rig, cmd_index);
}

int ft100_get_split_vfo(RIG *rig, vfo_t vfo, split_t *split, vfo_t *tx_vfo)
{

    struct ft100_priv_data *priv = (struct ft100_priv_data *)rig->state.priv;
    int ret;

    rig_debug(RIG_DEBUG_VERBOSE, "%s called\n", __func__);

    if (!split) { return -RIG_EINVAL; }

    ret = ft100_read_flags(rig);

    if (ret < 0)
    {
        return ret;
    }

    *split = (priv->flags.byte[0] & 0x01) == 0x01 ? RIG_SPLIT_ON : RIG_SPLIT_OFF;

    return RIG_OK;
}

int ft100_set_rptr_shift(RIG *rig, vfo_t vfo, rptr_shift_t shift)
{

    unsigned char cmd_index;

    rig_debug(RIG_DEBUG_VERBOSE, "%s:ft100_set_rptr_shift called\n", __func__);

    rig_debug(RIG_DEBUG_VERBOSE, "%s: + - 0 %3i %3i %3i %3i %c\n", __func__,
              RIG_RPT_SHIFT_PLUS, RIG_RPT_SHIFT_MINUS, RIG_RPT_SHIFT_NONE, shift,
              (char)shift);

    switch (shift)
    {
    case RIG_RPT_SHIFT_PLUS:
        cmd_index = FT100_NATIVE_CAT_SET_RPT_SHIFT_PLUS;
        break;

    case RIG_RPT_SHIFT_MINUS:
        cmd_index = FT100_NATIVE_CAT_SET_RPT_SHIFT_MINUS;
        break;

    case RIG_RPT_SHIFT_NONE:
        cmd_index = FT100_NATIVE_CAT_SET_RPT_SHIFT_SIMPLEX;
        break;

    default:
        return -RIG_EINVAL;
    }

    return ft100_send_priv_cmd(rig, cmd_index);
}

int ft100_get_rptr_shift(RIG *rig, vfo_t vfo, rptr_shift_t *shift)
{
    int ret;
    struct ft100_priv_data *priv = (struct ft100_priv_data *)rig->state.priv;

    ret = ft100_read_status(rig);

    if (ret != RIG_OK)
    {
        return ret;
    }

    *shift = RIG_RPT_SHIFT_NONE;

    if (priv->status.flag1 & (1 << 2)) { *shift = RIG_RPT_SHIFT_MINUS; }
    else if (priv->status.flag1 & (1 << 3)) { *shift = RIG_RPT_SHIFT_PLUS; }

    rig_debug(RIG_DEBUG_VERBOSE, "%s: flag1=0x%02x\n", __func__,
              priv->status.flag1);

    return RIG_OK;
}


/*
 * TODO: enable/disable encoding/decoding
 */
int ft100_set_dcs_code(RIG *rig, vfo_t vfo, tone_t code)
{
    unsigned char p_cmd[YAESU_CMD_LENGTH];
    unsigned char cmd_index;  /* index of sequence to send */
    int pcode;

    for (pcode = 0; pcode < 104 && ft100_dcs_list[pcode] != 0; pcode++)
    {
        if (ft100_dcs_list[pcode] == code)
        {
            break;
        }
    }

    /* there are 104 dcs codes available */
    if (pcode >= 104 || ft100_dcs_list[pcode] == 0)
    {
        return -RIG_EINVAL;
    }

    rig_debug(RIG_DEBUG_VERBOSE, "%s = %03i, n=%d\n",
              __func__, code, pcode);

    cmd_index = FT100_NATIVE_CAT_SET_DCS_CODE;

    memcpy(p_cmd, &ncmd[cmd_index].nseq, YAESU_CMD_LENGTH);

    p_cmd[3] = (char)pcode;

    return write_block(&rig->state.rigport, p_cmd, YAESU_CMD_LENGTH);
}

int ft100_get_dcs_code(RIG *rig, vfo_t vfo, tone_t *code)
{
    int ret;
    struct ft100_priv_data *priv = (struct ft100_priv_data *)rig->state.priv;

    ret = ft100_read_status(rig);

    if (ret != RIG_OK)
    {
        return ret;
    }

    *code = ft100_dcs_list[priv->status.dcs];
    rig_debug(RIG_DEBUG_VERBOSE, "%s: P1=0x%02x, code=%d\n", __func__,
              priv->status.dcs, *code);

    return RIG_OK;
}

/*
 * TODO: enable/disable encoding/decoding
 */
int ft100_set_ctcss_tone(RIG *rig, vfo_t vfo, tone_t tone)
{
    unsigned char p_cmd[YAESU_CMD_LENGTH];
    unsigned char cmd_index;  /* index of sequence to send */
    int ptone;

    for (ptone = 0; ptone < 39 && ft100_ctcss_list[ptone] != 0; ptone++)
    {
        if (ft100_ctcss_list[ptone] == tone)
        {
            break;
        }
    }

    /* there are 39 ctcss tones available */
    if (ptone >= 39 || ft100_ctcss_list[ptone] == 0)
    {
        return -RIG_EINVAL;
    }

    rig_debug(RIG_DEBUG_VERBOSE, "%s = %.1f Hz, n=%d\n", __func__,
              (float)tone / 10, ptone);

    cmd_index = FT100_NATIVE_CAT_SET_CTCSS_FREQ;

    memcpy(p_cmd, &ncmd[cmd_index].nseq, YAESU_CMD_LENGTH);

    p_cmd[3] = (char)ptone;

    return write_block(&rig->state.rigport, p_cmd, YAESU_CMD_LENGTH);
}

int ft100_get_ctcss_tone(RIG *rig, vfo_t vfo, tone_t *tone)
{
    int ret;
    struct ft100_priv_data *priv = (struct ft100_priv_data *)rig->state.priv;

    ret = ft100_read_status(rig);

    if (ret != RIG_OK)
    {
        return ret;
    }

    *tone = ft100_ctcss_list[priv->status.ctcss];
    rig_debug(RIG_DEBUG_VERBOSE, "%s: P1=0x%02x, tone=%.1f\n", __func__,
              priv->status.ctcss, *tone / 10.0);

    return RIG_OK;
}
