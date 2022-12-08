/*
 * hamlib - (C) Frank Singleton 2000-2003
 *          (C) Stephane Fillod 2000-2010
 *
 * ft600.c -(C) Kārlis Millers YL3ALK 2019
 *
 * This shared library provides an API for communicating
 * via serial interface to an FT-600 using the "CAT" interface.
 * The starting point for this code was Chris Karpinsky's ft100 implementation.
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
#include <string.h>

#include "hamlib/rig.h"
#include "serial.h"
#include "yaesu.h"
#include "ft600.h"
#include "misc.h"
#include "bandplan.h"

enum ft600_native_cmd_e
{

    FT600_NATIVE_CAT_LOCK_ON = 0,
    FT600_NATIVE_CAT_LOCK_OFF,
    FT600_NATIVE_CAT_PTT_ON,
    FT600_NATIVE_CAT_PTT_OFF,
    FT600_NATIVE_CAT_SET_FREQ,
    FT600_NATIVE_CAT_SET_MODE_LSB,
    FT600_NATIVE_CAT_SET_MODE_USB,
    FT600_NATIVE_CAT_SET_MODE_DIG,
    FT600_NATIVE_CAT_SET_MODE_CW,
    FT600_NATIVE_CAT_SET_MODE_AM,
    FT600_NATIVE_CAT_CLAR_ON,
    FT600_NATIVE_CAT_CLAR_OFF,
    FT600_NATIVE_CAT_SET_CLAR_FREQ,
    FT600_NATIVE_CAT_SET_VFOAB,
    FT600_NATIVE_CAT_SET_VFOA,
    FT600_NATIVE_CAT_SET_VFOB,
    FT600_NATIVE_CAT_SPLIT_ON,
    FT600_NATIVE_CAT_SPLIT_OFF,
    FT600_NATIVE_CAT_SET_RPT_SHIFT_MINUS,
    FT600_NATIVE_CAT_SET_RPT_SHIFT_PLUS,
    FT600_NATIVE_CAT_SET_RPT_SHIFT_SIMPLEX,
    FT600_NATIVE_CAT_SET_RPT_OFFSET,
    /* fix me */
    FT600_NATIVE_CAT_SET_DCS_ON,
    FT600_NATIVE_CAT_SET_CTCSS_ENC_ON,
    FT600_NATIVE_CAT_SET_CTCSS_ENC_DEC_ON,
    FT600_NATIVE_CAT_SET_CTCSS_DCS_OFF,
    /* em xif */
    FT600_NATIVE_CAT_SET_CTCSS_FREQ,
    FT600_NATIVE_CAT_SET_DCS_CODE,
    FT600_NATIVE_CAT_GET_RX_STATUS,
    FT600_NATIVE_CAT_GET_TX_STATUS,
    FT600_NATIVE_CAT_GET_FREQ_MODE_STATUS,
    FT600_NATIVE_CAT_PWR_WAKE,
    FT600_NATIVE_CAT_PWR_ON,
    FT600_NATIVE_CAT_PWR_OFF,
    FT600_NATIVE_CAT_READ_STATUS,
    FT600_NATIVE_CAT_READ_METERS,
    FT600_NATIVE_CAT_READ_FLAGS
};


/*
 *  we are able to get way more info
 *  than we can set
 *
 */
typedef struct
{
    unsigned char band_no;
    unsigned char freq[16];
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

// cppcheck-suppress *
    unsigned char stuffing[16];
}
FT600_STATUS_INFO;


typedef struct
{
    unsigned char byte[8];
}
// cppcheck-suppress *
FT600_FLAG_INFO;


static int ft600_init(RIG *rig);
static int ft600_open(RIG *rig);
static int ft600_cleanup(RIG *rig);
static int ft600_close(RIG *rig);

static int ft600_set_freq(RIG *rig, vfo_t vfo, freq_t freq);
static int ft600_get_freq(RIG *rig, vfo_t vfo, freq_t *freq);

static int ft600_set_mode(RIG *rig, vfo_t vfo, rmode_t mode, pbwidth_t width);
static int ft600_get_mode(RIG *rig, vfo_t vfo, rmode_t *mode, pbwidth_t *width);

static int ft600_get_vfo(RIG *rig, vfo_t *vfo);

static int ft600_set_ptt(RIG *rig, vfo_t vfo, ptt_t ptt);
static int ft600_get_level(RIG *rig, vfo_t vfo, setting_t level, value_t *val);

struct ft600_priv_data
{
    FT600_STATUS_INFO status;
    FT600_FLAG_INFO flags;
    unsigned char s_meter;

};


/* prototypes */
static int ft600_send_priv_cmd(RIG *rig, unsigned char cmd_index);

static const yaesu_cmd_set_t ncmd[] =
{
    { 0, { 0x00, 0x00, 0x00, 0x00, 0x00 } }, /* lock on */
    { 0, { 0x00, 0x00, 0x00, 0x00, 0x00 } }, /* lock off */

    { 1, { 0x00, 0x00, 0x00, 0x01, 0x0f } }, /* ptt on */
    { 1, { 0x00, 0x00, 0x00, 0x00, 0x0f } }, /* ptt off */

    { 0, { 0x00, 0x00, 0x00, 0x00, 0x0a } }, /* set freq */

    { 1, { 0x00, 0x00, 0x00, 0x00, 0x0c } }, /* mode set main LSB */
    { 1, { 0x00, 0x00, 0x00, 0x01, 0x0c } }, /* mode set main USB */
    { 1, { 0x00, 0x00, 0x00, 0x09, 0x0c } }, /* mode set main DIG */
    { 1, { 0x00, 0x00, 0x00, 0x03, 0x0c } }, /* mode set main CW */
    { 1, { 0x00, 0x00, 0x00, 0x04, 0x0c } }, /* mode set main AM */

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

    { 0, { 0x00, 0x00, 0x00, 0x02, 0x10 } }, /* get FREQ and MODE status */

    { 0, { 0x00, 0x00, 0x00, 0x00, 0x00 } }, /* pwr wakeup sequence */
    { 0, { 0x00, 0x00, 0x00, 0x00, 0x00 } }, /* pwr on */
    { 0, { 0x00, 0x00, 0x00, 0x00, 0x00 } }, /* pwr off */

    { 1, { 0x00, 0x00, 0x00, 0x02, 0x10 } }, /* read status block */
    { 1, { 0x00, 0x00, 0x00, 0x00, 0xf7 } }, /* read meter block */
    { 1, { 0x00, 0x00, 0x00, 0x01, 0xfa } }  /* read flags block */
};

#define FT600_GET_RIG_LEVELS (RIG_LEVEL_RAWSTR)
#define FT600_ALL_RX_MODES (RIG_MODE_LSB|RIG_MODE_USB|RIG_MODE_PKTUSB|RIG_MODE_CW|RIG_MODE_AM)
#define FT600_SSB_CW_RX_MODES (RIG_MODE_CW|RIG_MODE_USB|RIG_MODE_LSB)
#define FT600_OTHER_TX_MODES (RIG_MODE_LSB|RIG_MODE_USB|RIG_MODE_PKTUSB|RIG_MODE_CW)
#define FT600_AM_TX_MODES (RIG_MODE_AM)

#define FT600_VFO_ALL (RIG_VFO_A)
#define FT600_ANT (RIG_ANT_1)

#define FT600_DUMMY_S_METER_VALUE 0; //FOR TESTS
/* S-meter calibration, ascending order of RAW values */
#define FT600_STR_CAL { 16, \
            { \
            {   0, -54 }, /* S0 */ \
            {  11, -48 }, \
            {  21, -42 }, \
            {  34, -36 }, \
            {  50, -30 }, \
            {  59, -24 }, \
            {  75, -18 }, \
            {  93, -12 }, \
            { 103,  -6 }, \
            { 124,   0 }, /* S9 */ \
            { 145,  10 }, \
            { 160,  20 }, \
            { 183,  30 }, \
            { 204,  40 }, \
            { 222,  50 }, \
            { 246,  60 } /* S9+60dB */  \
}}

const struct rig_caps ft600_caps =
{
    RIG_MODEL(RIG_MODEL_FT600),
    .model_name =     "FT-600",
    .mfg_name =       "Yaesu",
    .version =        "20201009.0",
    .copyright =      "LGPL",
    .status =         RIG_STATUS_STABLE,
    .rig_type =       RIG_TYPE_TRANSCEIVER,
    .ptt_type =       RIG_PTT_RIG,
    .dcd_type =       RIG_DCD_NONE,
    .port_type =      RIG_PORT_SERIAL,
    .serial_rate_min =    4800,
    .serial_rate_max =    4800,
    .serial_data_bits =   8,
    .serial_stop_bits =   2,
    .serial_parity =  RIG_PARITY_NONE,
    .serial_handshake =   RIG_HANDSHAKE_NONE,
    .write_delay =    FT600_WRITE_DELAY,
    .post_write_delay =   FT600_POST_WRITE_DELAY,
    .timeout =        100,
    .retry =      0,
    .has_get_func =   RIG_FUNC_NONE,
    .has_set_func =   RIG_FUNC_NONE,
    .has_get_level =  FT600_GET_RIG_LEVELS,
    .has_set_level =  RIG_LEVEL_BAND_SELECT,
    .has_get_parm =   RIG_PARM_NONE,
    .has_set_parm =   RIG_PARM_NONE,  /* FIXME: parms */
    .level_gran =
    {
#include "level_gran_yaesu.h"
    },
    .parm_gran =      {},
    .ctcss_list =     RIG_FUNC_NONE,
    .dcs_list =       RIG_FUNC_NONE,
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
        {kHz(50), kHz(29999), FT600_ALL_RX_MODES, -1, -1, FT600_VFO_ALL},
        RIG_FRNG_END,
    },
    .tx_range_list1 =  {
        FRQ_RNG_HF(1, FT600_OTHER_TX_MODES, W(40), W(100), FT600_VFO_ALL, FT600_ANT),
        FRQ_RNG_HF(1, FT600_AM_TX_MODES,    W(25), W(25),  FT600_VFO_ALL, FT600_ANT),
    },
    .tuning_steps =  {
        {FT600_ALL_RX_MODES, 10},
        {FT600_ALL_RX_MODES, 100},
        {FT600_ALL_RX_MODES, 1000},
        RIG_TS_END,
    },
    .filters =  {
        {RIG_MODE_ALL, RIG_FLT_ANY},
        RIG_FLT_END
    },

    .str_cal = FT600_STR_CAL,
    .priv =       NULL,
    .rig_init =       ft600_init,
    .rig_cleanup =    ft600_cleanup,
    .rig_open =       ft600_open,
    .rig_close =      ft600_close,
    .set_freq =       ft600_set_freq,
    .get_freq =       ft600_get_freq,
    .set_mode =       ft600_set_mode,
    .get_mode =       ft600_get_mode,
    .set_vfo =        NULL,
    .get_vfo =        ft600_get_vfo,
    .set_ptt =            ft600_set_ptt,
    .get_ptt =            NULL,
    .get_level =  ft600_get_level,
    .set_level =      NULL,
    .get_dcd =        NULL,
    .set_rptr_shift =     NULL,
    .get_rptr_shift =     NULL,
    .set_rptr_offs =  NULL,
    .get_rptr_offs =  NULL,
    .set_split_freq =     NULL,
    .get_split_freq =     NULL,
    .set_split_mode =     NULL,
    .get_split_mode =     NULL,
    .set_split_vfo =  NULL,
    .get_split_vfo =  NULL,
    .set_rit =        NULL,
    .get_rit =        NULL,
    .set_xit =        NULL,
    .get_xit =        NULL,
    .set_ts =         NULL,
    .get_ts =         NULL,
    .set_dcs_code =   NULL,
    .get_dcs_code =   NULL,
    .set_ctcss_tone =     NULL,
    .get_ctcss_tone =     NULL,
    .set_dcs_sql =    NULL,
    .get_dcs_sql =    NULL,
    .set_ctcss_sql =  NULL,
    .get_ctcss_sql =  NULL,
    .set_powerstat =  NULL,
    .get_powerstat =  NULL,
    .reset =      NULL,
    .set_ant =        NULL,
    .get_ant =        NULL,
    .set_func =       NULL,
    .get_func =       NULL,
    .set_parm =       NULL,
    .get_parm =       NULL,
    .hamlib_check_rig_caps = HAMLIB_CHECK_RIG_CAPS
};

static int ft600_init(RIG *rig)
{
    rig_debug(RIG_DEBUG_VERBOSE, "%s called\n", __func__);

    rig->state.priv = (struct ft600_priv_data *) calloc(1,
                      sizeof(struct ft600_priv_data));

    if (!rig->state.priv) { return -RIG_ENOMEM; }

    return RIG_OK;
}

static int ft600_cleanup(RIG *rig)
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

static int ft600_open(RIG *rig)
{

    rig_debug(RIG_DEBUG_VERBOSE, "%s: called\n", __func__);

    return RIG_OK;
}

static int ft600_close(RIG *rig)
{

    rig_debug(RIG_DEBUG_VERBOSE, "%s:called\n", __func__);

    return RIG_OK;
}

static int ft600_send_priv_cmd(RIG *rig, unsigned char cmd_index)
{
    rig_debug(RIG_DEBUG_VERBOSE, "%s called (%d)\n", __func__, cmd_index);

    if (!rig) { return -RIG_EINVAL; }

    return write_block(&rig->state.rigport, (unsigned char *) &ncmd[cmd_index].nseq,
                       YAESU_CMD_LENGTH);
}

static int ft600_read_status(RIG *rig)
{
    struct ft600_priv_data *priv;
    int ret;

    rig_debug(RIG_DEBUG_VERBOSE, "%s called\n", __func__);

    priv = (struct ft600_priv_data *)rig->state.priv;

    rig_flush(&rig->state.rigport);

    ret = ft600_send_priv_cmd(rig, FT600_NATIVE_CAT_READ_STATUS);

    if (ret != RIG_OK)
    {
        return ret;
    }


    ret = read_block(&rig->state.rigport,
                     (unsigned char *) &priv->status, FT600_STATUS_UPDATE_DATA_LENGTH);

    rig_debug(RIG_DEBUG_VERBOSE, "%s: read status=%i \n", __func__, ret);

    if (ret < 0)
    {
        return ret;
    }

    return RIG_OK;
}

static int ft600_get_level(RIG *rig, vfo_t vfo, setting_t level, value_t *val)
{

    struct ft600_priv_data *priv;
    int ret;

    rig_debug(RIG_DEBUG_VERBOSE, "%s called\n", __func__);

    priv = (struct ft600_priv_data *)rig->state.priv;

    ret = ft600_send_priv_cmd(rig, FT600_NATIVE_CAT_READ_METERS);

    if (ret != RIG_OK)
    {
        return ret;
    }

    rig_debug(RIG_DEBUG_VERBOSE, "%s: read tx status=%i \n", __func__, ret);

    ret = read_block(&rig->state.rigport, &priv->s_meter, 5);

    if (ret < 0)
    {
        return ret;
    }

    rig_debug(RIG_DEBUG_VERBOSE, "S_METER: %u ", priv->s_meter);

    //val->i = FT600_DUMMY_S_METER_VALUE; //DUMMY
    val->i = priv->s_meter;

    return RIG_OK;
}

static int ft600_set_freq(RIG *rig, vfo_t vfo, freq_t freq)
{
    unsigned char p_cmd[YAESU_CMD_LENGTH];
    unsigned char cmd_index;  /* index of sequence to send */

    if (!rig) { return -RIG_EINVAL; }

    rig_debug(RIG_DEBUG_VERBOSE, "ft600: requested freq = %"PRIfreq" Hz \n", freq);

    cmd_index = FT600_NATIVE_CAT_SET_FREQ;

    memcpy(p_cmd, &ncmd[cmd_index].nseq, YAESU_CMD_LENGTH);

    freq = (int)freq / 10;
    to_bcd(p_cmd, freq, 8); /* store bcd format in in p_cmd */

    return write_block(&rig->state.rigport, p_cmd, YAESU_CMD_LENGTH);
}

static int ft600_get_freq(RIG *rig, vfo_t vfo, freq_t *freq)
{

    struct ft600_priv_data *priv = (struct ft600_priv_data *)rig->state.priv;
    freq_t f;
    int ret;
    rig_debug(RIG_DEBUG_VERBOSE, "%s: get_freq\n", __func__);

    if (!freq) { return -RIG_EINVAL; }

    ret = ft600_read_status(rig);

    if (ret != RIG_OK)
    {
        return ret;
    }

    f = ((((priv->status.freq[1] << 8) + priv->status.freq[2]) << 8) +
         priv->status.freq[3]) * 10;

    rig_debug(RIG_DEBUG_TRACE, "%s: freq = %"PRIfreq" Hz\n", __func__, f);

    *freq = f;

    return RIG_OK;
}

static int ft600_set_ptt(RIG *rig, vfo_t vfo, ptt_t ptt)
{

    unsigned char cmd_index;

    rig_debug(RIG_DEBUG_VERBOSE, "%s called\n", __func__);

    switch (ptt)
    {
    case RIG_PTT_ON:
        cmd_index = FT600_NATIVE_CAT_PTT_ON;
        break;

    case RIG_PTT_OFF:
        cmd_index = FT600_NATIVE_CAT_PTT_OFF;
        break;

    default:
        return -RIG_EINVAL;
    }

    return ft600_send_priv_cmd(rig, cmd_index);
}

static int ft600_get_mode(RIG *rig, vfo_t vfo, rmode_t *mode, pbwidth_t *width)
{

    struct ft600_priv_data *priv = (struct ft600_priv_data *)rig->state.priv;
    int ret;

    if (!mode)
    {
        return -RIG_EINVAL;
    }

    *width = RIG_PASSBAND_NORMAL;

    ret = ft600_read_status(rig);

    if (ret < 0)
    {
        return ret;
    }

    switch (priv->status.freq[6])
    {
    case 0x00:
        *mode = RIG_MODE_LSB;
        *width = Hz(5000);
        break;

    case 0x01:
        *mode = RIG_MODE_USB;
        *width = Hz(5000);
        break;

    case 0x02:
        *mode = RIG_MODE_CW;
        *width = Hz(1200);
        break;

    case 0x04:
        *mode = RIG_MODE_AM;
        *width = Hz(6000);
        break;

    case 0x05:
        *mode = RIG_MODE_PKTUSB;
        *width = Hz(5000);
        break;

    default:
        *mode = RIG_MODE_NONE;
        *width = RIG_PASSBAND_NORMAL;
    };

    return RIG_OK;
}

static int ft600_set_mode(RIG *rig, vfo_t vfo, rmode_t mode, pbwidth_t width)
{
    unsigned char cmd_index;  /* index of sequence to send */
    int ret;

    rig_debug(RIG_DEBUG_VERBOSE, "%s: generic mode = %s, width %d\n", __func__,
              rig_strrmode(mode), (int)width);

    switch (mode)
    {
    case RIG_MODE_LSB:
        cmd_index = FT600_NATIVE_CAT_SET_MODE_LSB;
        break;

    case RIG_MODE_USB:
        cmd_index = FT600_NATIVE_CAT_SET_MODE_USB;
        break;

    case RIG_MODE_PKTUSB:
        cmd_index = FT600_NATIVE_CAT_SET_MODE_DIG;
        break;

    case RIG_MODE_CW:
        cmd_index = FT600_NATIVE_CAT_SET_MODE_CW;
        break;

    case RIG_MODE_AM:
        cmd_index = FT600_NATIVE_CAT_SET_MODE_AM;
        break;

    default:
        return -RIG_EINVAL;
    }

    ret = ft600_send_priv_cmd(rig, cmd_index);

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

static int ft600_get_vfo(RIG *rig, vfo_t *vfo)
{


    rig_debug(RIG_DEBUG_VERBOSE, "%s called\n", __func__);

    if (!vfo)
    {
        return -RIG_EINVAL;
    }

    *vfo = RIG_VFO_A;

    return RIG_OK;
}
