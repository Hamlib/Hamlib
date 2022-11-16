/*
 * hamlib - (C) Frank Singleton 2000,2001 (vk3fcs@ix.netcom.com)
 *          (C) Stephane Fillod 2000-2010
 *
 * ft847.c - (C) Frank Singleton 2000 (vk3fcs@ix.netcom.com)
 *           (C) Stephane Fillod 2000-2010
 *
 * This shared library provides an API for communicating
 * via serial interface to an FT-847 using the "CAT" interface.
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
 * TODO - Remove static stuff, see ft747 for new style [started]
 *      - create yaesu.h for common command structure etc..[started]
 *      - add mode set before freq set to avoid prior mode offset (eg: CW)
 *
 */

/*
 * Notes on limitations in RIG control capabilities. These are
 * related to the Yaesu's FT847 design, not my program :-)
 *
 * 1. Rig opcodes allow only 10Hz resolution.
 * 2. Cannot select VFO B
 * 3. Using CAT and Tuner controls simultaneously  can
 *    cause problems.
 *
 */

#include <hamlib/config.h>

#include <stdlib.h>
#include <string.h>  /* String function definitions */

#include "hamlib/rig.h"
#include "serial.h"
#include "yaesu.h"
#include "ft847.h"
#include "misc.h"
#include "bandplan.h"
#include "tones.h"

/*
 * Native FT847 functions. This is what I have to work with :-)
 *
 */

enum ft847_native_cmd_e
{

    /* Set commands to the rig */

    FT_847_NATIVE_CAT_ON = 0,
    FT_847_NATIVE_CAT_OFF,

    FT_847_NATIVE_CAT_PTT_ON,
    FT_847_NATIVE_CAT_PTT_OFF,

    FT_847_NATIVE_CAT_SAT_MODE_ON,
    FT_847_NATIVE_CAT_SAT_MODE_OFF,

    FT_847_NATIVE_CAT_SET_FREQ_MAIN,
    FT_847_NATIVE_CAT_SET_FREQ_SAT_RX_VFO,
    FT_847_NATIVE_CAT_SET_FREQ_SAT_TX_VFO,

    FT_847_NATIVE_CAT_SET_MODE_MAIN_LSB, /* MAIN VFO */
    FT_847_NATIVE_CAT_SET_MODE_MAIN_USB,
    FT_847_NATIVE_CAT_SET_MODE_MAIN_CW,
    FT_847_NATIVE_CAT_SET_MODE_MAIN_CWR,
    FT_847_NATIVE_CAT_SET_MODE_MAIN_AM,
    FT_847_NATIVE_CAT_SET_MODE_MAIN_FM,
    FT_847_NATIVE_CAT_SET_MODE_MAIN_CWN,
    FT_847_NATIVE_CAT_SET_MODE_MAIN_CWRN,
    FT_847_NATIVE_CAT_SET_MODE_MAIN_AMN,
    FT_847_NATIVE_CAT_SET_MODE_MAIN_FMN,

    FT_847_NATIVE_CAT_SET_MODE_SAT_RX_LSB, /* SAT RX VFO */
    FT_847_NATIVE_CAT_SET_MODE_SAT_RX_USB,
    FT_847_NATIVE_CAT_SET_MODE_SAT_RX_CW,
    FT_847_NATIVE_CAT_SET_MODE_SAT_RX_CWR,
    FT_847_NATIVE_CAT_SET_MODE_SAT_RX_AM,
    FT_847_NATIVE_CAT_SET_MODE_SAT_RX_FM,
    FT_847_NATIVE_CAT_SET_MODE_SAT_RX_CWN,
    FT_847_NATIVE_CAT_SET_MODE_SAT_RX_CWRN,
    FT_847_NATIVE_CAT_SET_MODE_SAT_RX_AMN,
    FT_847_NATIVE_CAT_SET_MODE_SAT_RX_FMN,

    FT_847_NATIVE_CAT_SET_MODE_SAT_TX_LSB, /* SAT TX VFO */
    FT_847_NATIVE_CAT_SET_MODE_SAT_TX_USB,
    FT_847_NATIVE_CAT_SET_MODE_SAT_TX_CW,
    FT_847_NATIVE_CAT_SET_MODE_SAT_TX_CWR,
    FT_847_NATIVE_CAT_SET_MODE_SAT_TX_AM,
    FT_847_NATIVE_CAT_SET_MODE_SAT_TX_FM,
    FT_847_NATIVE_CAT_SET_MODE_SAT_TX_CWN,
    FT_847_NATIVE_CAT_SET_MODE_SAT_TX_CWRN,
    FT_847_NATIVE_CAT_SET_MODE_SAT_TX_AMN,
    FT_847_NATIVE_CAT_SET_MODE_SAT_TX_FMN,

    FT_847_NATIVE_CAT_SET_DCS_ON_MAIN, /* MAIN CTCSS/DCS */
    FT_847_NATIVE_CAT_SET_CTCSS_ENC_DEC_ON_MAIN,
    FT_847_NATIVE_CAT_SET_CTCSS_ENC_ON_MAIN,
    FT_847_NATIVE_CAT_SET_CTCSS_DCS_OFF_MAIN,

    FT_847_NATIVE_CAT_SET_DCS_ON_SAT_RX, /* SAT RX CTCSS/DCS */
    FT_847_NATIVE_CAT_SET_CTCSS_ENC_DEC_ON_SAT_RX,
    FT_847_NATIVE_CAT_SET_CTCSS_ENC_ON_SAT_RX,
    FT_847_NATIVE_CAT_SET_CTCSS_DCS_OFF_SAT_RX,

    FT_847_NATIVE_CAT_SET_DCS_ON_SAT_TX, /* SAT TX CTCSS/DCS */
    FT_847_NATIVE_CAT_SET_CTCSS_ENC_DEC_ON_SAT_TX,
    FT_847_NATIVE_CAT_SET_CTCSS_ENC_ON_SAT_TX,
    FT_847_NATIVE_CAT_SET_CTCSS_DCS_OFF_SAT_TX,

    FT_847_NATIVE_CAT_SET_CTCSS_FREQ_MAIN, /* CTCSS Freq */
    FT_847_NATIVE_CAT_SET_CTCSS_FREQ_SAT_RX,
    FT_847_NATIVE_CAT_SET_CTCSS_FREQ_SAT_TX,

    FT_847_NATIVE_CAT_SET_DCS_CODE_MAIN, /* DCS code */
    FT_847_NATIVE_CAT_SET_DCS_CODE_SAT_RX,
    FT_847_NATIVE_CAT_SET_DCS_CODE_SAT_TX,

    FT_847_NATIVE_CAT_SET_RPT_SHIFT_MINUS, /* RPT SHIFT */
    FT_847_NATIVE_CAT_SET_RPT_SHIFT_PLUS,
    FT_847_NATIVE_CAT_SET_RPT_SHIFT_SIMPLEX,

    FT_847_NATIVE_CAT_SET_RPT_OFFSET, /* RPT Offset frequency */

    /* Get info from the rig */

    FT_847_NATIVE_CAT_GET_RX_STATUS,
    FT_847_NATIVE_CAT_GET_TX_STATUS,

    FT_847_NATIVE_CAT_GET_FREQ_MODE_STATUS_MAIN,
    FT_847_NATIVE_CAT_GET_FREQ_MODE_STATUS_SAT_RX,
    FT_847_NATIVE_CAT_GET_FREQ_MODE_STATUS_SAT_TX,

    FT_847_NATIVE_SIZE        /* end marker, value indicates number of */
    /* native cmd entries */

};

typedef enum ft847_native_cmd_e ft847_native_cmd_t;



/*
 * API local implementation
 */

static int ft847_init(RIG *rig);
static int ft847_open(RIG *rig);

static int ft847_cleanup(RIG *rig);
static int ft847_close(RIG *rig);

static int ft847_set_freq(RIG *rig, vfo_t vfo, freq_t freq);
static int ft847_get_freq(RIG *rig, vfo_t vfo, freq_t *freq);

static int ft847_set_mode(RIG *rig, vfo_t vfo, rmode_t mode,
                          pbwidth_t width); /* select mode */
static int ft847_get_mode(RIG *rig, vfo_t vfo, rmode_t *mode,
                          pbwidth_t *width); /* get mode */

static int ft847_set_split_vfo(RIG *rig, vfo_t vfo, split_t split,
                               vfo_t tx_vfo);
static int ft847_get_split_vfo(RIG *rig, vfo_t vfo, split_t *split,
                               vfo_t *tx_vfo);

static int ft847_set_split_freq(RIG *rig, vfo_t vfo, freq_t freq);
static int ft847_get_split_freq(RIG *rig, vfo_t vfo, freq_t *freq);

static int ft847_set_split_mode(RIG *rig, vfo_t vfo, rmode_t mode,
                                pbwidth_t width);
static int ft847_get_split_mode(RIG *rig, vfo_t vfo, rmode_t *mode,
                                pbwidth_t *width);

static int ft847_set_ptt(RIG *rig, vfo_t vfo, ptt_t ptt);
static int ft847_get_ptt(RIG *rig, vfo_t vfo, ptt_t *ptt);
static int ft847_get_dcd(RIG *rig, vfo_t vfo, dcd_t *dcd);

static int ft847_get_level(RIG *rig, vfo_t vfo, setting_t level, value_t *val);

static int ft847_set_func(RIG *rig, vfo_t vfo, setting_t func, int status);
static int ft847_set_ctcss_tone(RIG *rig, vfo_t vfo, tone_t tone);
static int ft847_set_ctcss_sql(RIG *rig, vfo_t vfo, tone_t tone);
static int ft847_set_dcs_sql(RIG *rig, vfo_t vfo, tone_t code);
static int ft847_set_rptr_shift(RIG *rig, vfo_t vfo, rptr_shift_t rptr_shift);
static int ft847_set_rptr_offs(RIG *rig, vfo_t vfo, shortfreq_t rptr_offs);

/*
 * ft847 instance - private data
 *
 */

struct ft847_priv_data
{
    split_t sat_mode;

    unsigned char rx_status;  /* tx returned data */
    unsigned char tx_status;  /* rx returned data */
    /* for early ft847's we keep our own memory items */
    /* Early rigs are one-way com to the rig */
    freq_t freqA, freqB;
    mode_t mode;
    pbwidth_t width;
    ptt_t ptt;
};


/* prototypes */
static int ft847_send_priv_cmd(RIG *rig, int cmd_index);


/* Native ft847 cmd set prototypes. These are READ ONLY as each */
/* rig instance will copy from these and modify if required . */
/* Complete sequences (1) can be read and used directly as a cmd sequence . */
/* Incomplete sequences (0) must be completed with extra parameters */
/* eg: mem number, or freq etc.. */

static const yaesu_cmd_set_t ncmd[] =
{
    { 1, { 0x00, 0x00, 0x00, 0x00, 0x00 } }, /* CAT = On */
    { 1, { 0x00, 0x00, 0x00, 0x00, 0x80 } }, /* CAT = Off */
    { 1, { 0x00, 0x00, 0x00, 0x00, 0x08 } }, /* ptt on */
    { 1, { 0x00, 0x00, 0x00, 0x01, 0x88 } }, /* ptt off */
    { 1, { 0x00, 0x00, 0x00, 0x00, 0x4e } }, /* sat mode on */
    { 1, { 0x00, 0x00, 0x00, 0x00, 0x8e } }, /* sat mode off */

    { 0, { 0x00, 0x00, 0x00, 0x00, 0x01 } }, /* set freq main */
    { 0, { 0x00, 0x00, 0x00, 0x00, 0x11 } }, /* set freq sat rx */
    { 0, { 0x00, 0x00, 0x00, 0x00, 0x21 } }, /* set freq sat tx */

    { 1, { 0x00, 0x00, 0x00, 0x00, 0x07 } }, /* mode set main LSB */
    { 1, { 0x01, 0x00, 0x00, 0x00, 0x07 } }, /* mode set main USB */
    { 1, { 0x02, 0x00, 0x00, 0x00, 0x07 } }, /* mode set main CW */
    { 1, { 0x03, 0x00, 0x00, 0x00, 0x07 } }, /* mode set main CWR */
    { 1, { 0x04, 0x00, 0x00, 0x00, 0x07 } }, /* mode set main AM */
    { 1, { 0x08, 0x00, 0x00, 0x00, 0x07 } }, /* mode set main FM */
    { 1, { 0x82, 0x00, 0x00, 0x00, 0x07 } }, /* mode set main CWN */
    { 1, { 0x83, 0x00, 0x00, 0x00, 0x07 } }, /* mode set main CWRN */
    { 1, { 0x84, 0x00, 0x00, 0x00, 0x07 } }, /* mode set main AMN */
    { 1, { 0x88, 0x00, 0x00, 0x00, 0x07 } }, /* mode set main FMN */

    { 1, { 0x00, 0x00, 0x00, 0x00, 0x17 } }, /* mode set sat rx LSB */
    { 1, { 0x01, 0x00, 0x00, 0x00, 0x17 } }, /* mode set sat rx USB */
    { 1, { 0x02, 0x00, 0x00, 0x00, 0x17 } }, /* mode set sat rx CW */
    { 1, { 0x03, 0x00, 0x00, 0x00, 0x17 } }, /* mode set sat rx CWR */
    { 1, { 0x04, 0x00, 0x00, 0x00, 0x17 } }, /* mode set sat rx AM */
    { 1, { 0x08, 0x00, 0x00, 0x00, 0x17 } }, /* mode set sat rx FM */
    { 1, { 0x82, 0x00, 0x00, 0x00, 0x17 } }, /* mode set sat rx CWN */
    { 1, { 0x83, 0x00, 0x00, 0x00, 0x17 } }, /* mode set sat rx CWRN */
    { 1, { 0x84, 0x00, 0x00, 0x00, 0x17 } }, /* mode set sat rx AMN */
    { 1, { 0x88, 0x00, 0x00, 0x00, 0x17 } }, /* mode set sat rx FMN */

    { 1, { 0x00, 0x00, 0x00, 0x00, 0x27 } }, /* mode set sat tx LSB */
    { 1, { 0x01, 0x00, 0x00, 0x00, 0x27 } }, /* mode set sat tx USB */
    { 1, { 0x02, 0x00, 0x00, 0x00, 0x27 } }, /* mode set sat tx CW */
    { 1, { 0x03, 0x00, 0x00, 0x00, 0x27 } }, /* mode set sat tx CWR */
    { 1, { 0x04, 0x00, 0x00, 0x00, 0x27 } }, /* mode set sat tx AM */
    { 1, { 0x08, 0x00, 0x00, 0x00, 0x27 } }, /* mode set sat tx FM */
    { 1, { 0x82, 0x00, 0x00, 0x00, 0x27 } }, /* mode set sat tx CWN */
    { 1, { 0x83, 0x00, 0x00, 0x00, 0x27 } }, /* mode set sat tx CWRN */
    { 1, { 0x84, 0x00, 0x00, 0x00, 0x27 } }, /* mode set sat tx AMN */
    { 1, { 0x88, 0x00, 0x00, 0x00, 0x27 } }, /* mode set sat tx FMN */

    { 1, { 0x0a, 0x00, 0x00, 0x00, 0x0a } }, /* set DCS on, main */
    { 1, { 0x2a, 0x00, 0x00, 0x00, 0x0a } }, /* set CTCSS/DCS enc/dec on, main */
    { 1, { 0x4a, 0x00, 0x00, 0x00, 0x0a } }, /* set CTCSS/DCS enc on, main */
    { 1, { 0x8a, 0x00, 0x00, 0x00, 0x0a } }, /* set CTCSS/DCS off, main */

    { 1, { 0x0a, 0x00, 0x00, 0x00, 0x1a } }, /* set DCS on, sat rx */
    { 1, { 0x2a, 0x00, 0x00, 0x00, 0x1a } }, /* set CTCSS/DCS enc/dec on, sat rx */
    { 1, { 0x4a, 0x00, 0x00, 0x00, 0x1a } }, /* set CTCSS/DCS enc on, sat rx */
    { 1, { 0x8a, 0x00, 0x00, 0x00, 0x1a } }, /* set CTCSS/DCS off, sat rx */

    { 1, { 0x0a, 0x00, 0x00, 0x00, 0x2a } }, /* set DCS on, sat tx */
    { 1, { 0x2a, 0x00, 0x00, 0x00, 0x2a } }, /* set CTCSS/DCS enc/dec on, sat tx */
    { 1, { 0x4a, 0x00, 0x00, 0x00, 0x2a } }, /* set CTCSS/DCS enc on, sat tx */
    { 1, { 0x8a, 0x00, 0x00, 0x00, 0x2a } }, /* set CTCSS/DCS off, sat tx */

    { 0, { 0x00, 0x00, 0x00, 0x00, 0x0b } }, /* set CTCSS  freq, main */
    { 0, { 0x00, 0x00, 0x00, 0x00, 0x1b } }, /* set CTCSS  freq, sat rx */
    { 0, { 0x00, 0x00, 0x00, 0x00, 0x2b } }, /* set CTCSS  freq, sat tx */

    { 0, { 0x00, 0x00, 0x00, 0x00, 0x0c } }, /* set DCS code, main */
    { 0, { 0x00, 0x00, 0x00, 0x00, 0x1c } }, /* set DCS code, sat rx */
    { 0, { 0x00, 0x00, 0x00, 0x00, 0x2c } }, /* set DCS code, sat tx */

    { 0, { 0x00, 0x00, 0x00, 0x00, 0x09 } }, /* set RPT shift MINUS */
    { 0, { 0x00, 0x00, 0x00, 0x00, 0x49 } }, /* set RPT shift PLUS */
    { 0, { 0x00, 0x00, 0x00, 0x00, 0x89 } }, /* set RPT shift SIMPLEX */

    { 0, { 0x00, 0x00, 0x00, 0x00, 0xf9 } }, /* set RPT offset freq */

    { 1, { 0x00, 0x00, 0x00, 0x00, 0xe7 } }, /* get RX status  */
    { 1, { 0x00, 0x00, 0x00, 0x00, 0xf7 } }, /* get TX status  */

    { 1, { 0x00, 0x00, 0x00, 0x00, 0x03 } }, /* get FREQ and MODE status, main  */
    { 1, { 0x00, 0x00, 0x00, 0x00, 0x13 } }, /* get FREQ and MODE status, sat rx  */
    { 1, { 0x00, 0x00, 0x00, 0x00, 0x23 } }, /* get FREQ and MODE status, sat tx  */

};


/*
 * Receiver caps
 */

#define UNIDIRECTIONAL (rig->caps->rig_model == RIG_MODEL_FT847UNI || rig->caps->rig_model == RIG_MODEL_FT650)
#define FT847_ALL_RX_MODES (RIG_MODE_AM|RIG_MODE_CW|RIG_MODE_CWR|RIG_MODE_SSB|RIG_MODE_FM)
#define FT847_SSB_CW_RX_MODES (RIG_MODE_CW|RIG_MODE_CWR|RIG_MODE_SSB)
#define FT847_AM_FM_RX_MODES (RIG_MODE_AM|RIG_MODE_FM)

/* tx doesn't have WFM.
 * 100W in 160-6m (25 watts AM carrier)
 * 50W in 2m/70cm (12.5 watts AM carrier)
 */

#define FT847_OTHER_TX_MODES (RIG_MODE_AM|RIG_MODE_CW|RIG_MODE_SSB|RIG_MODE_FM)
#define FT847_AM_TX_MODES (RIG_MODE_AM)

#define FT847_FUNC_ALL (RIG_FUNC_TONE|RIG_FUNC_TSQL)

#define FT847_LEVEL_ALL (RIG_LEVEL_RAWSTR|RIG_LEVEL_STRENGTH|RIG_LEVEL_ALC)

#define FT847_VFOS (RIG_VFO_MAIN|RIG_VFO_SUB)

/* FT-847 has different antennas connectors, but no rig_set_ant() ability */
#define FT847_ANTS RIG_ANT_1

#define FT847_STR_CAL { 3, \
    { \
        {   0, -60 }, /* S0 */ \
        {  16,   0 }, /* S9 */ \
        {  31,  60 }, /* +60 dB */ \
    } }

/*
 * 39 CTCSS sub-audible tones
 * c.f. ft847_set_ctcss_tone()
 */
static tone_t ft847_ctcss_list[] =
{
    670,  693,  719,  744,  770,  797,  825,  854,  885,  915,
    948,  974, 1000, 1035, 1072, 1109, 1148, 1188, 1230, 1273,
    1318, 1365, 1413, 1462, 1514, 1567, 1622, 1679, 1738, 1799,
    1862, 1928, 2035, 2107, 2181, 2257, 2336, 2418, 2503,
    0
};
#if 0
static tone_t ft650_ctcss_list[] =
{
    670,  719,  744,  770,  797,  825,  854,  885,  915,
    948,  974, 1000, 1035, 1072, 1109, 1148, 1188, 1230, 1273,
    1318, 1365, 1413, 1462, 1514, 1567, 1622, 1679, 1738, 1799,
    1862, 1928, 2035, 2107, 2181, 2257, 2336, 2418, 2503,
    0
};
#endif


/*
 * ft847 rig capabilities.
 * Notice that some rigs share the same functions.
 */

const struct rig_caps ft847_caps =
{
    RIG_MODEL(RIG_MODEL_FT847),
    .model_name = "FT-847",
    .mfg_name =  "Yaesu",
    .version =  "20220525.0",
    .copyright =  "LGPL",
    .status =  RIG_STATUS_STABLE,
    .rig_type =  RIG_TYPE_TRANSCEIVER,
    .ptt_type =  RIG_PTT_RIG,
    .dcd_type =  RIG_DCD_RIG,
    .port_type =  RIG_PORT_SERIAL,
    .serial_rate_min =  4800,
    .serial_rate_max =  57600,
    .serial_data_bits =  8,
    .serial_stop_bits =  2,
    .serial_parity =  RIG_PARITY_NONE,
    .serial_handshake =  RIG_HANDSHAKE_NONE,
    .write_delay =  FT847_WRITE_DELAY,
    .post_write_delay =  FT847_POST_WRITE_DELAY,
    .timeout =  1000,
    .retry =  0,

    .has_get_func =  RIG_FUNC_NONE,
    .has_set_func =  FT847_FUNC_ALL,
    .has_get_level =  FT847_LEVEL_ALL,
    .has_set_level =  RIG_LEVEL_BAND_SELECT,
    .has_get_parm =  RIG_PARM_NONE,
    .has_set_parm =  RIG_PARM_NONE,
    .level_gran =
    {
#include "level_gran_yaesu.h"
    },
    .parm_gran =  {},
    .ctcss_list =  ft847_ctcss_list,
    .dcs_list =  common_dcs_list,
    .preamp =   { RIG_DBLST_END, }, /* no preamp/att in CAT */
    .attenuator =   { RIG_DBLST_END, },
    .max_rit =  Hz(0),
    .max_xit =  Hz(0),
    .max_ifshift =  Hz(0),
    .targetable_vfo =  RIG_TARGETABLE_FREQ | RIG_TARGETABLE_MODE | RIG_TARGETABLE_TONE | RIG_TARGETABLE_FUNC,
    .transceive =  RIG_TRN_OFF,
    .bank_qty =   0,
    .chan_desc_sz =  0,
    .str_cal = FT847_STR_CAL,

    .chan_list =  { RIG_CHAN_END, }, /* FIXME: memory chan list: 78, but only in clonable mode? */

    .rx_range_list1 =  {
        {kHz(100), MHz(30), FT847_ALL_RX_MODES, -1, -1, FT847_VFOS, FT847_ANTS, "EUR"}, /* rx range begin */
        {MHz(36), MHz(76), FT847_ALL_RX_MODES, -1, -1, FT847_VFOS, FT847_ANTS, "EUR"},
        {MHz(108), MHz(174), FT847_ALL_RX_MODES, -1, -1, FT847_VFOS, FT847_ANTS, "EUR"},
        {MHz(420), MHz(512), FT847_ALL_RX_MODES, -1, -1, FT847_VFOS, FT847_ANTS, "EUR"},

        RIG_FRNG_END,
    }, /* rx range end */

    .tx_range_list1 =  {
        FRQ_RNG_HF(1, FT847_OTHER_TX_MODES, W(5), W(100), FT847_VFOS, FT847_ANTS),
        FRQ_RNG_HF(1, FT847_AM_TX_MODES, W(1), W(25), FT847_VFOS, FT847_ANTS),

        FRQ_RNG_6m(1, FT847_OTHER_TX_MODES, W(5), W(100), FT847_VFOS, FT847_ANTS),
        FRQ_RNG_6m(1, FT847_AM_TX_MODES, W(1), W(25), FT847_VFOS, FT847_ANTS),

        FRQ_RNG_4m(1, FT847_OTHER_TX_MODES, W(1), W(50), FT847_VFOS, FT847_ANTS),
        FRQ_RNG_4m(1, FT847_AM_TX_MODES, W(1), W(12.5), FT847_VFOS, FT847_ANTS),

        FRQ_RNG_2m(1, FT847_OTHER_TX_MODES, W(1), W(50), FT847_VFOS, FT847_ANTS),
        FRQ_RNG_2m(1, FT847_AM_TX_MODES, W(1), W(12.5), FT847_VFOS, FT847_ANTS),

        FRQ_RNG_70cm(1, FT847_OTHER_TX_MODES, W(1), W(50), FT847_VFOS, FT847_ANTS),
        FRQ_RNG_70cm(1, FT847_AM_TX_MODES, W(1), W(12.5), FT847_VFOS, FT847_ANTS),

        RIG_FRNG_END,
    }, /* tx range end */

    .rx_range_list2 =  {
        {kHz(100), MHz(30), FT847_ALL_RX_MODES, -1, -1, FT847_VFOS, FT847_ANTS, "USA"}, /* rx range begin */
        {MHz(36), MHz(76), FT847_ALL_RX_MODES, -1, -1, FT847_VFOS, FT847_ANTS, "USA"},
        {MHz(108), MHz(174), FT847_ALL_RX_MODES, -1, -1, FT847_VFOS, FT847_ANTS, "USA"},
        {MHz(420), MHz(512), FT847_ALL_RX_MODES, -1, -1, FT847_VFOS, FT847_ANTS, "USA"},

        RIG_FRNG_END,
    }, /* rx range end */

    .tx_range_list2 =  {
        FRQ_RNG_HF(2, FT847_OTHER_TX_MODES, W(5), W(100), FT847_VFOS, FT847_ANTS),
        FRQ_RNG_HF(2, FT847_AM_TX_MODES, W(1), W(25), FT847_VFOS, FT847_ANTS),

        FRQ_RNG_6m(2, FT847_OTHER_TX_MODES, W(5), W(100), FT847_VFOS, FT847_ANTS),
        FRQ_RNG_6m(2, FT847_AM_TX_MODES, W(1), W(25), FT847_VFOS, FT847_ANTS),

        FRQ_RNG_2m(2, FT847_OTHER_TX_MODES, W(1), W(50), FT847_VFOS, FT847_ANTS),
        FRQ_RNG_2m(2, FT847_AM_TX_MODES, W(1), W(12.5), FT847_VFOS, FT847_ANTS),

        FRQ_RNG_70cm(2, FT847_OTHER_TX_MODES, W(1), W(50), FT847_VFOS, FT847_ANTS),
        FRQ_RNG_70cm(2, FT847_AM_TX_MODES, W(1), W(12.5), FT847_VFOS, FT847_ANTS),

        RIG_FRNG_END,
    }, /* tx range end */

    .tuning_steps =  { {FT847_SSB_CW_RX_MODES, 1}, /* normal */
        {FT847_SSB_CW_RX_MODES, 10}, /* fast */
        {FT847_SSB_CW_RX_MODES, 100}, /* faster */


        {FT847_AM_FM_RX_MODES, 10}, /* normal */
        {FT847_AM_FM_RX_MODES, 100}, /* fast  */

        RIG_TS_END,
    },
    /* mode/filter list, .remember =  order matters! */
    .filters =  {
        {RIG_MODE_SSB | RIG_MODE_CW | RIG_MODE_CWR, kHz(2.2)},
        {RIG_MODE_CW | RIG_MODE_CWR, Hz(500)},
        {RIG_MODE_AM, kHz(9)},
        {RIG_MODE_AM, kHz(2.2)},
        {RIG_MODE_FM, kHz(15)},
        {RIG_MODE_FM, kHz(9)},
        RIG_FLT_END,
    },

    .priv =   NULL,
    .rig_init =   ft847_init,
    .rig_cleanup =  ft847_cleanup,
    .rig_open =   ft847_open,
    .rig_close =  ft847_close,

    .set_freq =   ft847_set_freq,       /* set freq */
    .get_freq =  ft847_get_freq,        /* get freq */
    .set_mode =  ft847_set_mode,        /* set mode */
    .get_mode =  ft847_get_mode,        /* get mode */
    .set_split_vfo  = ft847_set_split_vfo,
    .get_split_vfo  = ft847_get_split_vfo,
    .set_split_freq = ft847_set_split_freq,
    .get_split_freq = ft847_get_split_freq,
    .set_split_mode = ft847_set_split_mode,
    .get_split_mode = ft847_get_split_mode,
    .set_ptt =  ft847_set_ptt,      /* set ptt */
    .get_ptt =  ft847_get_ptt,      /* get ptt */
    .get_dcd =  ft847_get_dcd,      /* get dcd */
    .get_level = ft847_get_level,           /* get level */

    .set_func       = ft847_set_func,
    .set_ctcss_tone = ft847_set_ctcss_tone,
    .set_ctcss_sql  = ft847_set_ctcss_sql,
    .set_dcs_sql    = ft847_set_dcs_sql,
    .set_rptr_shift = ft847_set_rptr_shift,
    .set_rptr_offs  = ft847_set_rptr_offs,
    .hamlib_check_rig_caps = HAMLIB_CHECK_RIG_CAPS
};

/*
 * ft600 rigs capabilities.
 * Notice that some rigs share the same functions.
 */

const struct rig_caps ft650_caps =
{
    RIG_MODEL(RIG_MODEL_FT650),
    .model_name = "FT-650",
    .mfg_name =  "Yaesu",
    .version =  "20220525.0",
    .copyright =  "LGPL",
    .status =  RIG_STATUS_STABLE,
    .rig_type =  RIG_TYPE_TRANSCEIVER,
    .ptt_type =  RIG_PTT_RIG,
    .dcd_type =  RIG_DCD_RIG,
    .port_type =  RIG_PORT_SERIAL,
    .serial_rate_min =  4800,
    .serial_rate_max =  4800,
    .serial_data_bits =  8,
    .serial_stop_bits =  2,
    .serial_parity =  RIG_PARITY_NONE,
    .serial_handshake =  RIG_HANDSHAKE_NONE,
    .write_delay =  FT847_WRITE_DELAY,
    .post_write_delay =  FT847_POST_WRITE_DELAY,
    .timeout =  1000,
    .retry =  0,

    .has_get_func =  RIG_FUNC_NONE,
    .has_set_func =  FT847_FUNC_ALL,
    //.has_get_level =  FT847_LEVEL_ALL,
    //.has_set_level =  RIG_LEVEL_BAND_SELECT,
    .has_get_parm =  RIG_PARM_NONE,
    .has_set_parm =  RIG_PARM_NONE,
    .level_gran =
    {
#include "level_gran_yaesu.h"
    },
    .parm_gran =  {},
    //.ctcss_list =  ft847_ctcss_list,
    .preamp =   { RIG_DBLST_END, }, /* no preamp/att in CAT */
    .attenuator =   { RIG_DBLST_END, },
    .max_rit =  Hz(0),
    .max_xit =  Hz(0),
    .max_ifshift =  Hz(0),
    //.targetable_vfo =  RIG_TARGETABLE_FREQ | RIG_TARGETABLE_MODE | RIG_TARGETABLE_TONE | RIG_TARGETABLE_FUNC,
    .transceive =  RIG_TRN_OFF,
    .bank_qty =   0,
    .chan_desc_sz =  0,

    .chan_list =  { RIG_CHAN_END, }, /* FIXME: memory chan list: 78, but only in clonable mode? */

    .rx_range_list1 =  {
        {MHz(24.5), MHz(56), FT847_ALL_RX_MODES, -1, -1, FT847_VFOS, FT847_ANTS, "EUR"},
        RIG_FRNG_END,
    }, /* rx range end */

    .tx_range_list1 =  {
        FRQ_RNG_HF(1, FT847_OTHER_TX_MODES, W(5), W(100), FT847_VFOS, FT847_ANTS),
        FRQ_RNG_HF(1, FT847_AM_TX_MODES, W(1), W(25), FT847_VFOS, FT847_ANTS),

        FRQ_RNG_6m(1, FT847_OTHER_TX_MODES, W(5), W(100), FT847_VFOS, FT847_ANTS),
        FRQ_RNG_6m(1, FT847_AM_TX_MODES, W(1), W(25), FT847_VFOS, FT847_ANTS),

        FRQ_RNG_4m(1, FT847_OTHER_TX_MODES, W(1), W(50), FT847_VFOS, FT847_ANTS),
        FRQ_RNG_4m(1, FT847_AM_TX_MODES, W(1), W(12.5), FT847_VFOS, FT847_ANTS),

        FRQ_RNG_2m(1, FT847_OTHER_TX_MODES, W(1), W(50), FT847_VFOS, FT847_ANTS),
        FRQ_RNG_2m(1, FT847_AM_TX_MODES, W(1), W(12.5), FT847_VFOS, FT847_ANTS),

        FRQ_RNG_70cm(1, FT847_OTHER_TX_MODES, W(1), W(50), FT847_VFOS, FT847_ANTS),
        FRQ_RNG_70cm(1, FT847_AM_TX_MODES, W(1), W(12.5), FT847_VFOS, FT847_ANTS),

        RIG_FRNG_END,
    }, /* tx range end */

    .rx_range_list2 =  {
        {MHz(24.5), MHz(56), FT847_ALL_RX_MODES, -1, -1, FT847_VFOS, FT847_ANTS, "USA"},

        RIG_FRNG_END,
    }, /* rx range end */

    .tx_range_list2 =  {
        {kHz(24500), kHz(25000), FT847_OTHER_TX_MODES, W(10), W(100), FT847_VFOS, FT847_ANTS},
        {kHz(24500), kHz(25000), FT847_AM_TX_MODES, W(10), W(100), FT847_VFOS, FT847_ANTS},
        {kHz(28000), kHz(29700), FT847_OTHER_TX_MODES, W(10), W(100), FT847_VFOS, FT847_ANTS},
        {kHz(28000), kHz(29700), FT847_AM_TX_MODES, W(10), W(100), FT847_VFOS, FT847_ANTS},
        {kHz(50000), kHz(54000), FT847_OTHER_TX_MODES, W(10), W(100), FT847_VFOS, FT847_ANTS},
        {kHz(50000), kHz(54000), FT847_AM_TX_MODES, W(10), W(100), FT847_VFOS, FT847_ANTS},
        RIG_FRNG_END,
    }, /* tx range end */

    .tuning_steps =  {
        {RIG_MODE_ALL, 1},
        RIG_TS_END,
    },

    /* mode/filter list, .remember =  order matters! */
    .filters =  {
        {RIG_MODE_SSB | RIG_MODE_CW, kHz(2.4)},
        {RIG_MODE_SSB, kHz(2.2)},
        {RIG_MODE_SSB, kHz(2.0)},
        {RIG_MODE_SSB, kHz(1.8)},
        {RIG_MODE_CW, Hz(1200)},
        {RIG_MODE_CW, Hz(600)},
        {RIG_MODE_CW, Hz(300)},
        {RIG_MODE_AM, kHz(9)},
        {RIG_MODE_AM, kHz(2.2)},
        {RIG_MODE_FM, kHz(15)},
        {RIG_MODE_FM, kHz(9)},
        RIG_FLT_END,
    },

    .priv =   NULL,
    .rig_init =   ft847_init,
    .rig_cleanup =  ft847_cleanup,
    .rig_open =   ft847_open,
    .rig_close =  ft847_close,

    .set_freq =   ft847_set_freq,       /* set freq */
    .get_freq =  ft847_get_freq,        /* get freq */
    .set_mode =  ft847_set_mode,        /* set mode */
    .get_mode =  ft847_get_mode,        /* get mode */
//    .set_split_vfo  = ft847_set_split_vfo,
//    .get_split_vfo  = ft847_get_split_vfo,
//    .set_split_freq = ft847_set_split_freq,
//    .get_split_freq = ft847_get_split_freq,
//    .set_split_mode = ft847_set_split_mode,
//    .get_split_mode = ft847_get_split_mode,
    .set_ptt =  ft847_set_ptt,      /* set ptt */
    .get_ptt =  ft847_get_ptt,      /* get ptt */
//    .get_dcd =  ft847_get_dcd,      /* get dcd */
//    .get_level = ft847_get_level,           /* get level */

    .set_func       = ft847_set_func,
    .set_ctcss_tone = ft847_set_ctcss_tone,
//    .set_ctcss_sql  = ft847_set_ctcss_sql,
//    .set_dcs_sql    = ft847_set_dcs_sql,
//    .set_rptr_shift = ft847_set_rptr_shift,
//    .set_rptr_offs  = ft847_set_rptr_offs,
    .hamlib_check_rig_caps = HAMLIB_CHECK_RIG_CAPS
};

const struct rig_caps mchfqrp_caps =
{
    RIG_MODEL(RIG_MODEL_MCHFQRP),
    .model_name = "mcHF QRP",
    .mfg_name =  "M0NKA",
    .version =  "20220525.0",
    .copyright =  "LGPL",
    .status =  RIG_STATUS_STABLE,
    .rig_type =  RIG_TYPE_TRANSCEIVER,
    .ptt_type =  RIG_PTT_RIG,
    .dcd_type =  RIG_DCD_RIG,
    .port_type =  RIG_PORT_SERIAL,
    .serial_rate_min =  4800,
    .serial_rate_max =  115200,
    .serial_data_bits =  8,
    .serial_stop_bits =  2,
    .serial_parity =  RIG_PARITY_NONE,
    .serial_handshake =  RIG_HANDSHAKE_NONE,
    .write_delay =  FT847_WRITE_DELAY,
    .post_write_delay =  FT847_POST_WRITE_DELAY,
    .timeout =  1000,
    .retry =  0,

    .has_get_func =  RIG_FUNC_NONE,
    .has_set_func =  FT847_FUNC_ALL,
    .has_get_level =  FT847_LEVEL_ALL,
    .has_set_level =  RIG_LEVEL_BAND_SELECT,
    .has_get_parm =  RIG_PARM_NONE,
    .has_set_parm =  RIG_PARM_NONE,
    .level_gran =
    {
#include "level_gran_yaesu.h"
    },
    .parm_gran =  {},
    .ctcss_list =  ft847_ctcss_list,
    .dcs_list =  common_dcs_list,
    .preamp =   { RIG_DBLST_END, }, /* no preamp/att in CAT */
    .attenuator =   { RIG_DBLST_END, },
    .max_rit =  Hz(0),
    .max_xit =  Hz(0),
    .max_ifshift =  Hz(0),
    .targetable_vfo =  RIG_TARGETABLE_FREQ | RIG_TARGETABLE_MODE | RIG_TARGETABLE_TONE | RIG_TARGETABLE_FUNC,
    .transceive =  RIG_TRN_OFF,
    .bank_qty =   0,
    .chan_desc_sz =  0,
    .str_cal = FT847_STR_CAL,

    .chan_list =  { RIG_CHAN_END, }, /* FIXME: memory chan list: 78, but only in clonable mode? */

    .rx_range_list1 =  {
        {kHz(100), MHz(30), FT847_ALL_RX_MODES, -1, -1, FT847_VFOS, FT847_ANTS, "EUR"}, /* rx range begin */
        // {MHz(36), MHz(76), FT847_ALL_RX_MODES, -1, -1, FT847_VFOS, FT847_ANTS, "EUR"},
        // {MHz(108), MHz(174), FT847_ALL_RX_MODES, -1, -1, FT847_VFOS, FT847_ANTS, "EUR"},
        // {MHz(420), MHz(512), FT847_ALL_RX_MODES, -1, -1, FT847_VFOS, FT847_ANTS, "EUR"},

        RIG_FRNG_END,
    }, /* rx range end */

    .tx_range_list1 =  {
        FRQ_RNG_HF(1, FT847_OTHER_TX_MODES, W(5), W(100), FT847_VFOS, FT847_ANTS),
        FRQ_RNG_HF(1, FT847_AM_TX_MODES, W(1), W(25), FT847_VFOS, FT847_ANTS),

        // FRQ_RNG_6m(1, FT847_OTHER_TX_MODES, W(5), W(100), FT847_VFOS, FT847_ANTS),
        // FRQ_RNG_6m(1, FT847_AM_TX_MODES, W(1), W(25), FT847_VFOS, FT847_ANTS),

        // FRQ_RNG_4m(1, FT847_OTHER_TX_MODES, W(1), W(50), FT847_VFOS, FT847_ANTS),
        // FRQ_RNG_4m(1, FT847_AM_TX_MODES, W(1), W(12.5), FT847_VFOS, FT847_ANTS),

        // FRQ_RNG_2m(1, FT847_OTHER_TX_MODES, W(1), W(50), FT847_VFOS, FT847_ANTS),
        // FRQ_RNG_2m(1, FT847_AM_TX_MODES, W(1), W(12.5), FT847_VFOS, FT847_ANTS),

        // FRQ_RNG_70cm(1, FT847_OTHER_TX_MODES, W(1), W(50), FT847_VFOS, FT847_ANTS),
        // FRQ_RNG_70cm(1, FT847_AM_TX_MODES, W(1), W(12.5), FT847_VFOS, FT847_ANTS),

        RIG_FRNG_END,
    }, /* tx range end */

    .rx_range_list2 =  {
        {kHz(100), MHz(30), FT847_ALL_RX_MODES, -1, -1, FT847_VFOS, FT847_ANTS, "USA"}, /* rx range begin */
        // {MHz(36), MHz(76), FT847_ALL_RX_MODES, -1, -1, FT847_VFOS, FT847_ANTS, "USA"},
        // {MHz(108), MHz(174), FT847_ALL_RX_MODES, -1, -1, FT847_VFOS, FT847_ANTS, "USA"},
        // {MHz(420), MHz(512), FT847_ALL_RX_MODES, -1, -1, FT847_VFOS, FT847_ANTS, "USA"},

        RIG_FRNG_END,
    }, /* rx range end */

    .tx_range_list2 =  {
        FRQ_RNG_HF(2, FT847_OTHER_TX_MODES, W(5), W(100), FT847_VFOS, FT847_ANTS),
        FRQ_RNG_HF(2, FT847_AM_TX_MODES, W(1), W(25), FT847_VFOS, FT847_ANTS),

        // FRQ_RNG_6m(2, FT847_OTHER_TX_MODES, W(5), W(100), FT847_VFOS, FT847_ANTS),
        // FRQ_RNG_6m(2, FT847_AM_TX_MODES, W(1), W(25), FT847_VFOS, FT847_ANTS),

        // FRQ_RNG_2m(2, FT847_OTHER_TX_MODES, W(1), W(50), FT847_VFOS, FT847_ANTS),
        // FRQ_RNG_2m(2, FT847_AM_TX_MODES, W(1), W(12.5), FT847_VFOS, FT847_ANTS),

        // FRQ_RNG_70cm(2, FT847_OTHER_TX_MODES, W(1), W(50), FT847_VFOS, FT847_ANTS),
        // FRQ_RNG_70cm(2, FT847_AM_TX_MODES, W(1), W(12.5), FT847_VFOS, FT847_ANTS),

        RIG_FRNG_END,
    }, /* tx range end */

    .tuning_steps =  { {FT847_SSB_CW_RX_MODES, 1}, /* normal */
        {FT847_SSB_CW_RX_MODES, 10}, /* fast */
        {FT847_SSB_CW_RX_MODES, 100}, /* faster */


        {FT847_AM_FM_RX_MODES, 10}, /* normal */
        {FT847_AM_FM_RX_MODES, 100}, /* fast  */

        RIG_TS_END,
    },
    /* mode/filter list, .remember =  order matters! */
    .filters =  {
        {RIG_MODE_SSB | RIG_MODE_CW | RIG_MODE_CWR, kHz(2.2)},
        {RIG_MODE_CW | RIG_MODE_CWR, Hz(500)},
        {RIG_MODE_AM, kHz(9)},
        {RIG_MODE_AM, kHz(2.2)},
        {RIG_MODE_FM, kHz(15)},
        {RIG_MODE_FM, kHz(9)},
        RIG_FLT_END,
    },

    .priv =   NULL,
    .rig_init =   ft847_init,
    .rig_cleanup =  ft847_cleanup,
    .rig_open =   ft847_open,
    .rig_close =  ft847_close,

    .set_freq =   ft847_set_freq,       /* set freq */
    .get_freq =  ft847_get_freq,        /* get freq */
    .set_mode =  ft847_set_mode,        /* set mode */
    .get_mode =  ft847_get_mode,        /* get mode */
    .set_split_vfo  = ft847_set_split_vfo,
    .get_split_vfo  = ft847_get_split_vfo,
    .set_split_freq = ft847_set_split_freq,
    .get_split_freq = ft847_get_split_freq,
    .set_split_mode = ft847_set_split_mode,
    .get_split_mode = ft847_get_split_mode,
    .set_ptt =  ft847_set_ptt,      /* set ptt */
    .get_ptt =  ft847_get_ptt,      /* get ptt */
    .get_dcd =  ft847_get_dcd,      /* get dcd */
    .get_level = ft847_get_level,           /* get level */

    .set_func       = ft847_set_func,
    .set_ctcss_tone = ft847_set_ctcss_tone,
    .set_ctcss_sql  = ft847_set_ctcss_sql,
    .set_dcs_sql    = ft847_set_dcs_sql,
    .set_rptr_shift = ft847_set_rptr_shift,
    .set_rptr_offs  = ft847_set_rptr_offs,
    .hamlib_check_rig_caps = HAMLIB_CHECK_RIG_CAPS
};

/*
 * ft847uni rigs capabilities.
 * Notice that some rigs share the same functions.
 * Also this struct is READONLY!
 Yaesu appears to use the following format for serial numbers on their
amateur products:

Year of manufacture-Month of manufacture-Production Run-Individual Unit
number, where the month of manufacture is offset by 2, so "C" means January,
"D" means February, "E" means March, and so forth.

Example: 8G051234 = 1998, May (fifth month, or "G"), Production Run 05, unit
#1234 in this run.

One key serial number range is 8G05. This seems to be the point at which
Yaesu had corrected the bi-directional CAT issue and made some other
improvements. This version was made in May 1998. Later serial numbers (e.g.,
8L09nnnn) all seem to have incorporated the earlier improvements plus new
ones...."
 */
const struct rig_caps ft847uni_caps =
{
    RIG_MODEL(RIG_MODEL_FT847UNI),
    .model_name = "FT-847UNI",
    .mfg_name =  "Yaesu",
    .version =  "20210221.0",
    .copyright =  "LGPL",
    .status =  RIG_STATUS_STABLE,
    .rig_type =  RIG_TYPE_TRANSCEIVER,
    .ptt_type =  RIG_PTT_RIG,
    .dcd_type =  RIG_DCD_RIG,
    .port_type =  RIG_PORT_SERIAL,
    .serial_rate_min =  4800,
    .serial_rate_max =  57600,
    .serial_data_bits =  8,
    .serial_stop_bits =  2,
    .serial_parity =  RIG_PARITY_NONE,
    .serial_handshake =  RIG_HANDSHAKE_NONE,
    .write_delay =  FT847_WRITE_DELAY,
    .post_write_delay =  FT847_POST_WRITE_DELAY,
    .timeout =  1000,
    .retry =  0,

    .has_get_func =  RIG_FUNC_NONE,
    .has_set_func =  FT847_FUNC_ALL,
    .has_get_level =  FT847_LEVEL_ALL,
    .has_set_level =  RIG_LEVEL_BAND_SELECT,
    .has_get_parm =  RIG_PARM_NONE,
    .has_set_parm =  RIG_PARM_NONE,
    .level_gran =
    {
#include "level_gran_yaesu.h"
    },
    .parm_gran =  {},
    .ctcss_list =  ft847_ctcss_list,
    .dcs_list =  common_dcs_list,
    .preamp =   { RIG_DBLST_END, }, /* no preamp/att in CAT */
    .attenuator =   { RIG_DBLST_END, },
    .max_rit =  Hz(0),
    .max_xit =  Hz(0),
    .max_ifshift =  Hz(0),
    .targetable_vfo =  RIG_TARGETABLE_FREQ | RIG_TARGETABLE_MODE | RIG_TARGETABLE_TONE | RIG_TARGETABLE_FUNC,
    .transceive =  RIG_TRN_OFF,
    .bank_qty =   0,
    .chan_desc_sz =  0,
    .str_cal = FT847_STR_CAL,

    .chan_list =  { RIG_CHAN_END, }, /* FIXME: memory chan list: 78, but only in clonable mode? */

    .rx_range_list1 =  {
        {kHz(100), MHz(30), FT847_ALL_RX_MODES, -1, -1, FT847_VFOS, FT847_ANTS, "EUR"}, /* rx range begin */
        {MHz(36), MHz(76), FT847_ALL_RX_MODES, -1, -1, FT847_VFOS, FT847_ANTS, "EUR"},
        {MHz(108), MHz(174), FT847_ALL_RX_MODES, -1, -1, FT847_VFOS, FT847_ANTS, "EUR"},
        {MHz(420), MHz(512), FT847_ALL_RX_MODES, -1, -1, FT847_VFOS, FT847_ANTS, "EUR"},

        RIG_FRNG_END,
    }, /* rx range end */

    .tx_range_list1 =  {
        FRQ_RNG_HF(1, FT847_OTHER_TX_MODES, W(5), W(100), FT847_VFOS, FT847_ANTS),
        FRQ_RNG_HF(1, FT847_AM_TX_MODES, W(1), W(25), FT847_VFOS, FT847_ANTS),

        FRQ_RNG_6m(2, FT847_OTHER_TX_MODES, W(5), W(100), FT847_VFOS, FT847_ANTS),
        FRQ_RNG_6m(2, FT847_AM_TX_MODES, W(1), W(25), FT847_VFOS, FT847_ANTS),

        FRQ_RNG_2m(2, FT847_OTHER_TX_MODES, W(1), W(50), FT847_VFOS, FT847_ANTS),
        FRQ_RNG_2m(2, FT847_AM_TX_MODES, W(1), W(12.5), FT847_VFOS, FT847_ANTS),

        FRQ_RNG_70cm(2, FT847_OTHER_TX_MODES, W(1), W(50), FT847_VFOS, FT847_ANTS),
        FRQ_RNG_70cm(2, FT847_AM_TX_MODES, W(1), W(12.5), FT847_VFOS, FT847_ANTS),

        RIG_FRNG_END,
    }, /* tx range end */

    .rx_range_list2 =
    {   {kHz(100), MHz(30), FT847_ALL_RX_MODES, -1, -1, FT847_VFOS, FT847_ANTS, "USA"}, /* rx range begin */
        {MHz(36), MHz(76), FT847_ALL_RX_MODES, -1, -1, FT847_VFOS, FT847_ANTS, "USA"},
        {MHz(108), MHz(174), FT847_ALL_RX_MODES, -1, -1, FT847_VFOS, FT847_ANTS, "USA"},
        {MHz(420), MHz(512), FT847_ALL_RX_MODES, -1, -1, FT847_VFOS, FT847_ANTS, "USA"},

        RIG_FRNG_END,
    }, /* rx range end */

    .tx_range_list2 =
    {
        FRQ_RNG_HF(2, FT847_OTHER_TX_MODES, W(5), W(100), FT847_VFOS, FT847_ANTS),
        FRQ_RNG_HF(2, FT847_AM_TX_MODES, W(1), W(25), FT847_VFOS, FT847_ANTS),

        FRQ_RNG_6m(2, FT847_OTHER_TX_MODES, W(5), W(100), FT847_VFOS, FT847_ANTS),
        FRQ_RNG_6m(2, FT847_AM_TX_MODES, W(1), W(25), FT847_VFOS, FT847_ANTS),

        FRQ_RNG_2m(2, FT847_OTHER_TX_MODES, W(1), W(50), FT847_VFOS, FT847_ANTS),
        FRQ_RNG_2m(2, FT847_AM_TX_MODES, W(1), W(12.5), FT847_VFOS, FT847_ANTS),

        FRQ_RNG_70cm(2, FT847_OTHER_TX_MODES, W(1), W(50), FT847_VFOS, FT847_ANTS),
        FRQ_RNG_70cm(2, FT847_AM_TX_MODES, W(1), W(12.5), FT847_VFOS, FT847_ANTS),

        RIG_FRNG_END,
    },

    .tuning_steps =  { {FT847_SSB_CW_RX_MODES, 1}, /* normal */
        {FT847_SSB_CW_RX_MODES, 10}, /* fast */
        {FT847_SSB_CW_RX_MODES, 100}, /* faster */


        {FT847_AM_FM_RX_MODES, 10}, /* normal */
        {FT847_AM_FM_RX_MODES, 100}, /* fast  */

        RIG_TS_END,
    },
    /* mode/filter list, .remember =  order matters! */
    .filters =  {
        {RIG_MODE_SSB | RIG_MODE_CW | RIG_MODE_CWR, kHz(2.2)},
        {RIG_MODE_CW | RIG_MODE_CWR, Hz(500)},
        {RIG_MODE_AM, kHz(9)},
        {RIG_MODE_AM, kHz(2.2)},
        {RIG_MODE_FM, kHz(15)},
        {RIG_MODE_FM, kHz(9)},
        RIG_FLT_END,
    },

    .priv =   NULL,
    .rig_init =   ft847_init,
    .rig_cleanup =  ft847_cleanup,
    .rig_open =   ft847_open,
    .rig_close =  ft847_close,

    .set_freq =   ft847_set_freq,       /* set freq */
    .get_freq =  ft847_get_freq,        /* get freq */
    .set_mode =  ft847_set_mode,        /* set mode */
    .get_mode =  ft847_get_mode,        /* get mode */
    .set_split_vfo  = ft847_set_split_vfo,
    .get_split_vfo  = ft847_get_split_vfo,
    .set_split_freq = ft847_set_split_freq,
    .get_split_freq = ft847_get_split_freq,
    .set_split_mode = ft847_set_split_mode,
    .get_split_mode = ft847_get_split_mode,
    .set_ptt =  ft847_set_ptt,      /* set ptt */
    .get_ptt =  ft847_get_ptt,      /* get ptt */
    .get_dcd =  ft847_get_dcd,      /* get dcd */
    .get_level = ft847_get_level,           /* get level */

    .set_func       = ft847_set_func,
    .set_ctcss_tone = ft847_set_ctcss_tone,
    .set_ctcss_sql  = ft847_set_ctcss_sql,
    .set_dcs_sql    = ft847_set_dcs_sql,
    .set_rptr_shift = ft847_set_rptr_shift,
    .set_rptr_offs  = ft847_set_rptr_offs,
    .hamlib_check_rig_caps = HAMLIB_CHECK_RIG_CAPS
};

/*
 * Function definitions below
 */

/*
 * setup *priv
 * serial port is already open (rig->state->fd)
 */

static int ft847_init(RIG *rig)
{
    struct ft847_priv_data *priv;

    rig_debug(RIG_DEBUG_VERBOSE, "%s called \n", __func__);

    if (!rig)
    {
        return -RIG_EINVAL;
    }

    rig->state.priv = (struct ft847_priv_data *) calloc(1,
                      sizeof(struct ft847_priv_data));

    if (!rig->state.priv)
    {
        /* whoops! memory shortage! */
        return -RIG_ENOMEM;
    }

    priv = rig->state.priv;


    priv->sat_mode = RIG_SPLIT_OFF;

    /* for early FT-847's we have our own memory items due to one way comm*/
    /* until these are set we won't know what the values are */
    priv->freqA = 1;  /* 1Hz default */
    priv->freqB = 1;  /* 1Hz default */
    priv->mode =
        RIG_MODE_USB; /* mode USB by default to avoid users not setting mode */
    priv->width = 1; /* 1Hz default */

    return RIG_OK;
}


/*
 * ft847_cleanup routine
 * the serial port is closed by the frontend
 */

static int ft847_cleanup(RIG *rig)
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

    rig_debug(RIG_DEBUG_VERBOSE, "%s: called \n", __func__);

    return RIG_OK;
}


/*
 * ft847_open  routine
 *
 */

static int ft847_open(RIG *rig)
{
    int retval;
    /* Good time to set CAT ON */

    rig_debug(RIG_DEBUG_VERBOSE, "%s: called\n", __func__);

    retval = ft847_send_priv_cmd(rig, FT_847_NATIVE_CAT_ON);
    RETURNFUNC2(retval);
}

/*
 * ft847_close  routine
 *
 */

static int ft847_close(RIG *rig)
{

    /* Good time to set CAT OFF */

    rig_debug(RIG_DEBUG_VERBOSE, "%s: called\n", __func__);

    /* don't care about return value */
    ft847_send_priv_cmd(rig, FT_847_NATIVE_CAT_OFF);

    return RIG_OK;
}

/*
 * private helper function to send a private command
 * sequence . Must only be complete sequences.
 *
 */

static int ft847_send_priv_cmd(RIG *rig, int cmd_index)
{
    if (! ncmd[cmd_index].ncomp)
    {
        rig_debug(RIG_DEBUG_VERBOSE, "%s: attempt to send incomplete sequence\n",
                  __func__);
        return -RIG_EINVAL;
    }

    return write_block(&rig->state.rigport, ncmd[cmd_index].nseq,
                       YAESU_CMD_LENGTH);
}


/*
 * opcode_vfo() copy into cmd the 5 byte command designated by cmd_index,
 * and patch the opcode with VFO targeting (MAIN 0x0-, SAT RX 0x1-, SAT TX 0x2-)
 */
static int opcode_vfo(RIG *rig, unsigned char *cmd, int cmd_index, vfo_t vfo)
{
    struct ft847_priv_data *p = (struct ft847_priv_data *)rig->state.priv;

    memcpy(cmd, &ncmd[cmd_index].nseq, YAESU_CMD_LENGTH);

    /* If the sat_mode is not enabled,
     * then leave the OpCode untouched (MAIN VFO) */

    if (p->sat_mode == RIG_SPLIT_ON)
    {
        switch (vfo)
        {
        case RIG_VFO_CURR:
        case RIG_VFO_MAIN:
            cmd[4] &= 0x0f;
            cmd[4] |= 0x10;  /* MAIN VFO -> SAT RX VFO */
            break;

        case RIG_VFO_SUB:
        case RIG_VFO_TX:
            cmd[4] &= 0x0f;
            cmd[4] |= 0x20;  /* SAT TX VFO */
            break;

        default:
            rig_debug(RIG_DEBUG_WARN, "%s: Unsupported VFO %s\n", __func__,
                      rig_strvfo(vfo));
            return -RIG_EINVAL;     /* sorry, wrong VFO */
        }
    }

    return RIG_OK;
}


/*
 * Set frequency to freq Hz. Note 10 Hz resolution -- YUK -- FS
 *
 */


static int ft847_set_freq(RIG *rig, vfo_t vfo, freq_t freq)
{
    unsigned char p_cmd[YAESU_CMD_LENGTH]; /* sequence to send */
    int ret;
    // cppcheck-suppress *
    char *fmt = "%s: requested freq after conversion = %"PRIll" Hz \n";

    if (!rig)
    {
        return -RIG_EINVAL;
    }

    rig_debug(RIG_DEBUG_VERBOSE, "ft847: requested freq = %"PRIfreq" Hz, vfo=%s\n",
              freq, rig_strvfo(vfo));

    ret = opcode_vfo(rig, p_cmd, FT_847_NATIVE_CAT_SET_FREQ_MAIN, vfo);

    if (ret != RIG_OK)
    {
        return ret;
    }

    to_bcd_be(p_cmd, freq / 10, 8); /* store bcd format in in p_cmd */

    rig_debug(RIG_DEBUG_VERBOSE, fmt, __func__,
              (int64_t)from_bcd_be(p_cmd, 8) * 10);

    if (UNIDIRECTIONAL)
    {
        struct ft847_priv_data *priv = (struct ft847_priv_data *)rig->state.priv;

        if (vfo == RIG_VFO_MAIN)
        {
            priv->freqA = freq;
            rig_debug(RIG_DEBUG_TRACE, "%s: freqA=%"PRIfreq"\n", __func__, priv->freqA);
        }
        else
        {
            priv->freqB = freq;
            rig_debug(RIG_DEBUG_TRACE, "%s: freqB=%"PRIfreq"\n", __func__, priv->freqB);
        }
    }

    return write_block(&rig->state.rigport, p_cmd, YAESU_CMD_LENGTH);
}

#define MD_LSB  0x00
#define MD_USB  0x01
#define MD_CW   0x02
#define MD_CWR  0x03
#define MD_AM   0x04
#define MD_FM   0x08
#define MD_CWN  0x82
#define MD_CWNR 0x83
#define MD_AMN  0x84
#define MD_FMN  0x88

static int get_freq_and_mode(RIG *rig, vfo_t vfo, freq_t *freq, rmode_t *mode,
                             pbwidth_t *width)
{
    struct rig_state *rs = &rig->state;
    unsigned char p_cmd[YAESU_CMD_LENGTH]; /* sequence to send */
    unsigned char cmd_index;  /* index of sequence to send */
    unsigned char data[8];
    int n;
    struct ft847_priv_data *priv = (struct ft847_priv_data *)rig->state.priv;

    rig_debug(RIG_DEBUG_VERBOSE, "%s: vfo =%s \n",
              __func__, rig_strvfo(vfo));

    if (UNIDIRECTIONAL)
    {
        if (vfo == RIG_VFO_MAIN)
        {
            *freq = priv->freqA;
            rig_debug(RIG_DEBUG_TRACE, "%s: freqA=%"PRIfreq"\n", __func__, priv->freqA);
        }
        else
        {
            *freq = priv->freqB;
            rig_debug(RIG_DEBUG_TRACE, "%s: freqB=%"PRIfreq"\n", __func__, priv->freqB);
        }

        *mode = priv->mode;
        *width = priv->width;
        return RIG_OK;
    }

    cmd_index = FT_847_NATIVE_CAT_GET_FREQ_MODE_STATUS_MAIN;

    memcpy(p_cmd, &ncmd[cmd_index].nseq, YAESU_CMD_LENGTH);

    /* change opcode according to vfo */
    n = opcode_vfo(rig, p_cmd, cmd_index, vfo);

    if (n != RIG_OK)
    {
        return n;
    }

    n = write_block(&rs->rigport, p_cmd, YAESU_CMD_LENGTH);

    if (n < 0)
    {
        return n;
    }

    n = read_block(&rs->rigport, data, YAESU_CMD_LENGTH);

    if (n != YAESU_CMD_LENGTH)
    {
        rig_debug(RIG_DEBUG_ERR, "ft847: read_block returned %d\n", n);
        return n < 0 ? n : -RIG_EPROTO;
    }

    /* Remember, this is 10Hz resolution */
    *freq = 10 * from_bcd_be(data, 8);

    *width = RIG_PASSBAND_NORMAL;

    switch (data[4])
    {
    case MD_LSB: *mode = RIG_MODE_LSB; break;

    case MD_USB: *mode = RIG_MODE_USB; break;

    case MD_CWN:
        *width = rig_passband_narrow(rig, RIG_MODE_CW);

    case MD_CW:
        *mode = RIG_MODE_CW;
        break;

    case MD_CWNR:
        *width = rig_passband_narrow(rig, RIG_MODE_CW);

    case MD_CWR:
        *mode = RIG_MODE_CWR;
        break;

    case MD_AMN:
        *width = rig_passband_narrow(rig, RIG_MODE_AM);

    case MD_AM:
        *mode = RIG_MODE_AM;
        break;

    case MD_FMN:
        *width = rig_passband_narrow(rig, RIG_MODE_FM);

    case MD_FM:
        *mode = RIG_MODE_FM;
        break;

    default:
        *mode = RIG_MODE_NONE;
        rig_debug(RIG_DEBUG_VERBOSE, "ft847: Unknown mode %02x\n", data[4]);
    }

    if (*width == RIG_PASSBAND_NORMAL)
    {
        *width = rig_passband_normal(rig, *mode);
    }

    return RIG_OK;
}

/*
 * Note taken from http://www.supercontrol.de/cat/ft847faq/page4.htm#pollingcodes
 * The FT-847, as originally delivered, could not poll the radio for frequency
 * and mode information. This was added beginning with the 8G05 production
 * runs. The Operating Manual does not show the codes for polling the radio.
 * Note that you cannot query the sub-VFO, nor can you swap VFOs via software.
 */
static int ft847_get_freq(RIG *rig, vfo_t vfo, freq_t *freq)
{
    rmode_t mode;
    pbwidth_t width;

    return get_freq_and_mode(rig, vfo, freq, &mode, &width);
}




static int ft847_set_mode(RIG *rig, vfo_t vfo, rmode_t mode, pbwidth_t width)
{
    unsigned char cmd_index;  /* index of sequence to send */
    struct rig_state *rs = &rig->state;
    unsigned char p_cmd[YAESU_CMD_LENGTH]; /* sequence to send */
    int ret;

    /*
     * translate mode from generic to ft847 specific
     */

    rig_debug(RIG_DEBUG_VERBOSE, "%s: generic mode = %s\n", __func__,
              rig_strrmode(mode));

    if (UNIDIRECTIONAL)
    {
        struct ft847_priv_data *priv = (struct ft847_priv_data *)rig->state.priv;
        priv->mode = mode;
        priv->width = width;
    }


    switch (mode)
    {
    case RIG_MODE_AM:
        cmd_index = FT_847_NATIVE_CAT_SET_MODE_MAIN_AM;
        break;

    case RIG_MODE_CW:
        cmd_index = FT_847_NATIVE_CAT_SET_MODE_MAIN_CW;
        break;

    case RIG_MODE_CWR:
        cmd_index = FT_847_NATIVE_CAT_SET_MODE_MAIN_CWR;
        break;

    case RIG_MODE_USB:
        cmd_index = FT_847_NATIVE_CAT_SET_MODE_MAIN_USB;
        break;

    case RIG_MODE_LSB:
        cmd_index = FT_847_NATIVE_CAT_SET_MODE_MAIN_LSB;
        break;

    case RIG_MODE_FM:
        cmd_index = FT_847_NATIVE_CAT_SET_MODE_MAIN_FM;
        break;

    default:
        return -RIG_EINVAL;     /* sorry, wrong MODE */
    }


    /*
     * Now set width
     */
    if (width != RIG_PASSBAND_NOCHANGE)
    {
        if (width == rig_passband_narrow(rig, mode))
        {
            switch (mode)
            {
            case RIG_MODE_AM:
                cmd_index = FT_847_NATIVE_CAT_SET_MODE_MAIN_AMN;
                break;

            case RIG_MODE_FM:
                cmd_index = FT_847_NATIVE_CAT_SET_MODE_MAIN_FMN;
                break;

            case RIG_MODE_CW:
                cmd_index = FT_847_NATIVE_CAT_SET_MODE_MAIN_CWN;
                break;

            case RIG_MODE_CWR:
                cmd_index = FT_847_NATIVE_CAT_SET_MODE_MAIN_CWRN;
                break;

            case RIG_MODE_USB:
            case RIG_MODE_LSB:
                break;

            default:
                rig_debug(RIG_DEBUG_ERR, "%s: unsupported mode/width: %s/%d, narrow: %d\n",
                          __func__, rig_strrmode(mode), (int)width,
                          (int)rig_passband_narrow(rig, mode));
                return -RIG_EINVAL;     /* sorry, wrong MODE/WIDTH combo  */
            }
        }
        else
        {
            if (width != RIG_PASSBAND_NORMAL &&
                    width != rig_passband_normal(rig, mode))
            {
                rig_debug(RIG_DEBUG_ERR, "%s: unsupported mode/width: %s/%d, narrow: %d\n",
                          __func__, rig_strrmode(mode), (int)width,
                          (int)rig_passband_narrow(rig, mode));
                return -RIG_EINVAL;     /* sorry, wrong MODE/WIDTH combo  */
            }
        }
    }

    /*
     * Now send the command
     */
    ret = opcode_vfo(rig, p_cmd, cmd_index, vfo);

    if (ret != RIG_OK)
    {
        return ret;
    }

    return write_block(&rs->rigport, p_cmd, YAESU_CMD_LENGTH);
}

static int ft847_get_mode(RIG *rig, vfo_t vfo, rmode_t *mode, pbwidth_t *width)
{
    freq_t freq;

    return get_freq_and_mode(rig, vfo, &freq, mode, width);
}


/*
 * Not exactly Split mode, this will set *SAT* Mode
 */
static int ft847_set_split_vfo(RIG *rig, vfo_t vfo, split_t split, vfo_t tx_vfo)
{
    struct ft847_priv_data *priv = (struct ft847_priv_data *)rig->state.priv;
    unsigned char cmd_index;  /* index of sequence to send */
    int ret;

    rig_debug(RIG_DEBUG_VERBOSE, "%s called\n", __func__);

    switch (split)
    {
    case RIG_SPLIT_ON:
        cmd_index = FT_847_NATIVE_CAT_SAT_MODE_ON;
        break;

    case RIG_SPLIT_OFF:
        cmd_index = FT_847_NATIVE_CAT_SAT_MODE_OFF;
        break;

    default:
        return -RIG_EINVAL;     /* sorry, wrong split range */
    }

    ret = ft847_send_priv_cmd(rig, cmd_index);

    if (ret == RIG_OK)
    {
        priv->sat_mode = split;
    }

    return ret;
}

static int ft847_get_split_vfo(RIG *rig, vfo_t vfo, split_t *split,
                               vfo_t *tx_vfo)
{
    struct ft847_priv_data *priv = (struct ft847_priv_data *)rig->state.priv;

    rig_debug(RIG_DEBUG_VERBOSE, "%s called\n", __func__);

    *split = priv->sat_mode;
    *tx_vfo = RIG_VFO_SUB;

    return RIG_OK;
}

static int ft847_set_split_freq(RIG *rig, vfo_t vfo, freq_t freq)
{
    int retval = rig_set_split_vfo(rig, RIG_VFO_A, RIG_SPLIT_ON, RIG_VFO_B);

    if (retval != RIG_OK) { RETURNFUNC2(retval); }

    return ft847_set_freq(rig, RIG_VFO_TX, freq);
}

static int ft847_get_split_freq(RIG *rig, vfo_t vfo, freq_t *freq)
{
    return ft847_get_freq(rig, RIG_VFO_TX, freq);
}

static int ft847_set_split_mode(RIG *rig, vfo_t vfo, rmode_t mode,
                                pbwidth_t width)
{
    return ft847_set_mode(rig, RIG_VFO_TX, mode, width);
}

static int ft847_get_split_mode(RIG *rig, vfo_t vfo, rmode_t *mode,
                                pbwidth_t *width)
{
    return ft847_get_mode(rig, RIG_VFO_TX, mode, width);
}

/*
 * _set_ptt
 *
 */


static int ft847_set_ptt(RIG *rig, vfo_t vfo, ptt_t ptt)
{
    unsigned char cmd_index;  /* index of sequence to send */

    rig_debug(RIG_DEBUG_VERBOSE, "%s: called \n", __func__);

    if (UNIDIRECTIONAL)
    {
        struct ft847_priv_data *priv = (struct ft847_priv_data *)rig->state.priv;
        priv->ptt = ptt;
    }

    switch (ptt)
    {
    case RIG_PTT_ON:
        cmd_index = FT_847_NATIVE_CAT_PTT_ON;
        break;

    case RIG_PTT_OFF:
        cmd_index = FT_847_NATIVE_CAT_PTT_OFF;
        break;

    default:
        return -RIG_EINVAL;     /* sorry, wrong ptt range */
    }

    /*
     * phew! now send cmd to rig
     */

    return ft847_send_priv_cmd(rig, cmd_index);

}

static int ft847_get_status(RIG *rig, int status_ci)
{
    struct ft847_priv_data *p = (struct ft847_priv_data *) rig->state.priv;
    unsigned char *data;
    int len;
    int n;

    if (UNIDIRECTIONAL)
    {
        return -RIG_ENIMPL;
    }

    switch (status_ci)
    {
    case FT_847_NATIVE_CAT_GET_RX_STATUS:
        data = &p->rx_status;
        len  = 1;
        break;

    case FT_847_NATIVE_CAT_GET_TX_STATUS:
        data = &p->tx_status;
        len  = 1;
        break;

    default:
        rig_debug(RIG_DEBUG_ERR, "%s: Internal error!\n", __func__);
        return -RIG_EINTERNAL;
    }

    rig_flush(&rig->state.rigport);

    n = write_block(&rig->state.rigport, ncmd[status_ci].nseq,
                    YAESU_CMD_LENGTH);

    if (n < 0)
    {
        return n;
    }

    n = read_block(&rig->state.rigport, data, len);

    if (n < 0)
    {
        return n;
    }

    if (n != len)
    {
        return -RIG_EPROTO;
    }

    return RIG_OK;
}

static int ft847_get_ptt(RIG *rig, vfo_t vfo, ptt_t *ptt)
{

    struct ft847_priv_data *p = (struct ft847_priv_data *) rig->state.priv;
    int n;

    if (UNIDIRECTIONAL)
    {
        struct ft847_priv_data *priv = (struct ft847_priv_data *)rig->state.priv;
        *ptt = priv->ptt;
        return RIG_OK;
    }

    n = ft847_get_status(rig, FT_847_NATIVE_CAT_GET_TX_STATUS);

    if (n < 0)
    {
        return n;
    }

    *ptt = (p->tx_status & 0x80) ? RIG_PTT_OFF : RIG_PTT_ON;

    /* The CAT query above lies when PTT is asserted via the PTT pin on
       the rear PACKET socket, it always returns Rx status. Given that
       CAT PTT does not take audio from DATA IN on the rear PACKET
       socket DTR or RTS PTT on the rear PACKET PTT pin is likely. So we
       override if we know PTT was asserted via rig_set_ptt for any type
       of PTT */
    if (RIG_PTT_OFF == *ptt && rig->state.transmit) { *ptt = RIG_PTT_ON; }

    return RIG_OK;
}


static int ft847_get_dcd(RIG *rig, vfo_t vfo, dcd_t *dcd)
{

    struct ft847_priv_data *p = (struct ft847_priv_data *) rig->state.priv;
    int n;

    if (UNIDIRECTIONAL)
    {
        return -RIG_ENIMPL;
    }

    n = ft847_get_status(rig, FT_847_NATIVE_CAT_GET_RX_STATUS);

    if (n < 0)
    {
        return n;
    }

    *dcd = (p->rx_status & 0x80) ? RIG_DCD_OFF : RIG_DCD_ON;

    return RIG_OK;
}



/*
 * Get the 'raw' signal strength
 * This number corresponds to the number of 'dots' in
 * the FT-847 display
 */
static int ft847_get_rawstr_level(RIG *rig, value_t *val)
{
    struct ft847_priv_data *p = (struct ft847_priv_data *) rig->state.priv;
    int n;

    if (UNIDIRECTIONAL)
    {
        return -RIG_ENIMPL;
    }

    n = ft847_get_status(rig, FT_847_NATIVE_CAT_GET_RX_STATUS);

    if (n < 0)
    {
        return n;
    }

    n = (p->rx_status & 0x1F);

    val->i = n;

    return RIG_OK;
}


/*
 * Get S-meter reading (in dB)
*/
static int ft847_get_smeter_level(RIG *rig, value_t *val)
{
    int n;

    if (UNIDIRECTIONAL)
    {
        return -RIG_ENIMPL;
    }

    n = ft847_get_rawstr_level(rig, val);

    if (n < 0)
    {
        return n;
    }

    /*
     * The FT-847 S-meter readings over CAT returns
     * an integer that corresponds to the number of
     * 'dots' lit in the display. Use a conversion
     * function to convert the raw signal strength to dB
     */
    n = val->i;

    if (n < 4)  /* <= S1 */
    {
        val->i = -54 + (n * 2);
    }
    else if (n < 20) /* S1 - S9 */
    {
        val->i = -48 + ((n - 3) * 3);
    }
    else   /* > S9 */
    {
        n -= 19;
        val->i = (n * 5);
    }

    return RIG_OK;
}

/*
 * Get the PO/ALC Meter Data
 */
static int ft847_get_alc_level(RIG *rig, value_t *val)
{
    struct ft847_priv_data *p = (struct ft847_priv_data *) rig->state.priv;
    int n;

    if (UNIDIRECTIONAL)
    {
        return -RIG_ENIMPL;
    }

    n = ft847_get_status(rig, FT_847_NATIVE_CAT_GET_TX_STATUS);

    if (n < 0)
    {
        return n;
    }

    n = (p->tx_status & 0x1F);

    val->f = (float)n / 0x1F;

    return RIG_OK;
}


/*
 * Get "level" data from rig.
 * The 847 supports S-meter readings in receive mode
 * and PO/ALC in transmit mode.  There is no way
 * to determine whether it's PO or ALC, unfortunately.
 */
static int ft847_get_level(RIG *rig, vfo_t vfo, setting_t level, value_t *val)
{
    if (UNIDIRECTIONAL)
    {
        return -RIG_ENIMPL;
    }

    switch (level)
    {
    case RIG_LEVEL_STRENGTH:
        return ft847_get_smeter_level(rig, val);

    case RIG_LEVEL_RAWSTR:
        return ft847_get_rawstr_level(rig, val);

    case RIG_LEVEL_ALC:
        return ft847_get_alc_level(rig, val);

    default:
        return -RIG_EINVAL;
    }

    return RIG_OK;

}

static int ft847_set_func(RIG *rig, vfo_t vfo, setting_t func, int status)
{
    unsigned char p_cmd[YAESU_CMD_LENGTH]; /* sequence to send */
    int ret;
    ft847_native_cmd_t fcmd;

    if (!rig)
    {
        return -RIG_EINVAL;
    }

    switch (func)
    {
    case RIG_FUNC_TONE:
        fcmd = status ? FT_847_NATIVE_CAT_SET_CTCSS_ENC_ON_MAIN :
               FT_847_NATIVE_CAT_SET_CTCSS_DCS_OFF_MAIN;
        break;

    case RIG_FUNC_TSQL:
        fcmd = status ? FT_847_NATIVE_CAT_SET_CTCSS_ENC_DEC_ON_MAIN :
               FT_847_NATIVE_CAT_SET_CTCSS_DCS_OFF_MAIN;
        break;

    default:
        return -RIG_EINVAL;
    }

    ret = opcode_vfo(rig, p_cmd, fcmd, vfo);

    if (ret != RIG_OK)
    {
        return ret;
    }

    return write_block(&rig->state.rigport, p_cmd, YAESU_CMD_LENGTH);
}


static int ft847_set_ctcss_tone(RIG *rig, vfo_t vfo, tone_t tone)
{
    unsigned char p_cmd[YAESU_CMD_LENGTH]; /* sequence to send */
    int i, ret;

    /*
     * 39 CTCSS CAT codes corresponding to ft847_ctcss_list
     */
    static const unsigned char ft847_ctcss_cat[] =
    {
        0x3F, 0x39, 0x1F, 0x3E, 0x0F, 0x3D, 0x1E, 0x3C, 0x0E, 0x3B,
        0x1D, 0x3A, 0x0D, 0x1C, 0x0C, 0x1B, 0x0B, 0x1A, 0x0A, 0x19,
        0x09, 0x18, 0x08, 0x17, 0x07, 0x16, 0x06, 0x15, 0x05, 0x14,
        0x04, 0x13, 0x03, 0x12, 0x02, 0x11, 0x01, 0x10, 0x00,
    };

    ret = opcode_vfo(rig, p_cmd, FT_847_NATIVE_CAT_SET_CTCSS_FREQ_MAIN, vfo);

    if (ret != RIG_OK)
    {
        return ret;
    }

#define FT847_CTCSS_NB 39

    for (i = 0; i < FT847_CTCSS_NB; i++)
    {
        if (ft847_ctcss_list[i] == tone)
        {
            break;
        }
    }

    if (i == FT847_CTCSS_NB)
    {
        return -RIG_EINVAL;
    }

    /* get associated CAT code */
    p_cmd[0] = ft847_ctcss_cat[i];

    return write_block(&rig->state.rigport, p_cmd, YAESU_CMD_LENGTH);
}

static int ft847_set_ctcss_sql(RIG *rig, vfo_t vfo, tone_t tone)
{
    /* same opcode as tone */
    return ft847_set_ctcss_tone(rig, vfo, tone);
}

static int ft847_set_dcs_sql(RIG *rig, vfo_t vfo, tone_t code)
{
    unsigned char p_cmd[YAESU_CMD_LENGTH]; /* sequence to send */
    int ret;

    ret = opcode_vfo(rig, p_cmd, FT_847_NATIVE_CAT_SET_DCS_CODE_MAIN, vfo);

    if (ret != RIG_OK)
    {
        return ret;
    }

    /* TODO: FT_847_NATIVE_CAT_SET_DCS_ON_MAIN here or with new RIG_FUNC_DCS? */

    /* DCS Code # (i.e. 07, 54=DCS Code 754) */
    to_bcd_be(p_cmd, code, 4); /* store bcd format in in p_cmd */

    return write_block(&rig->state.rigport, p_cmd, YAESU_CMD_LENGTH);

}

static int ft847_set_rptr_shift(RIG *rig, vfo_t vfo, rptr_shift_t rptr_shift)
{
    unsigned char cmd_index;  /* index of sequence to send */

    switch (rptr_shift)
    {
    case RIG_RPT_SHIFT_NONE:
        cmd_index = FT_847_NATIVE_CAT_SET_RPT_SHIFT_SIMPLEX;
        break;

    case RIG_RPT_SHIFT_MINUS:
        cmd_index = FT_847_NATIVE_CAT_SET_RPT_SHIFT_MINUS;
        break;

    case RIG_RPT_SHIFT_PLUS:
        cmd_index = FT_847_NATIVE_CAT_SET_RPT_SHIFT_PLUS;
        break;

    default:
        return -RIG_EINVAL;     /* sorry, wrong shift */
    }

    return ft847_send_priv_cmd(rig, cmd_index);
}

static int ft847_set_rptr_offs(RIG *rig, vfo_t vfo, shortfreq_t rptr_offs)
{
    unsigned char p_cmd[YAESU_CMD_LENGTH]; /* sequence to send */

    if (UNIDIRECTIONAL)
    {
        return -RIG_ENIMPL;
    }

    memcpy(p_cmd, &ncmd[FT_847_NATIVE_CAT_SET_RPT_OFFSET].nseq, YAESU_CMD_LENGTH);


    to_bcd_be(p_cmd, rptr_offs / 10, 8); /* store bcd format in in p_cmd */

    return write_block(&rig->state.rigport, p_cmd, YAESU_CMD_LENGTH);
}

