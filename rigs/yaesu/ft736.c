/*
 * ft736.c - (C) Stephane Fillod 2004-2010
 *
 * This shared library provides an API for communicating
 * via serial interface to an FT-736R using the "CAT" interface
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

#include "hamlib/rig.h"
#include "serial.h"
#include "misc.h"
#include "yaesu.h"
#include "tones.h"
#include "cache.h"



#define FT736_MODES (RIG_MODE_CW|RIG_MODE_SSB|RIG_MODE_FM|RIG_MODE_FMN|RIG_MODE_CWN)

#define FT736_VFOS (RIG_VFO_A)

/* Measurement by Ron W6FM using a signal generator.
 * Raw values supposed to be between 0x30 and 0xad according to manual.
 */
#define FT736_STR_CAL { 3, { \
        { 0x1d, -54 }, /* S0 */ \
        { 0x54,   0 }, /* S9 */ \
        { 0x9d,  60 }  /* +60 */ \
        } }

struct ft736_priv_data
{
    split_t split;
};

/* Private helper function prototypes */

static int ft736_open(RIG *rig);
static int ft736_close(RIG *rig);

static int ft736_set_freq(RIG *rig, vfo_t vfo, freq_t freq);
static int ft736_get_freq(RIG *rig, vfo_t vfo, freq_t *freq); // cached answer
static int ft736_set_mode(RIG *rig, vfo_t vfo, rmode_t mode, pbwidth_t width);
static int ft736_set_split_vfo(RIG *rig, vfo_t vfo, split_t split,
                               vfo_t tx_vfo);
static int ft736_set_split_freq(RIG *rig, vfo_t vfo, freq_t freq);
static int ft736_set_split_mode(RIG *rig, vfo_t vfo, rmode_t mode,
                                pbwidth_t width);
static int ft736_set_ptt(RIG *rig, vfo_t vfo, ptt_t ptt);
static int ft736_get_dcd(RIG *rig, vfo_t vfo, dcd_t *dcd);
static int ft736_get_level(RIG *rig, vfo_t vfo, setting_t level, value_t *val);
static int ft736_set_rptr_shift(RIG *rig, vfo_t vfo, rptr_shift_t shift);
static int ft736_set_rptr_offs(RIG *rig, vfo_t vfo, shortfreq_t offs);
static int ft736_set_func(RIG *rig, vfo_t vfo, setting_t func, int status);
static int ft736_set_ctcss_tone(RIG *rig, vfo_t vfo, tone_t tone);
static int ft736_set_ctcss_sql(RIG *rig, vfo_t vfo, tone_t tone);

/* Some tones are present twice, the second value is
 * higher Q (80), according to manual.
 */
static tone_t ft736_ctcss_list[] =
{
    670,  719,  770,  825,  885,  948, 1000, 1035, 1072, 1109,
    1148, 1188, 1230, 1273, 1318, 1365, 1413, 1462, 1514, 1567,
    1622, 1679, 1738, 1799, 1862, 1928, 2035, 2107, 2181, 2257,
    2336, 2418, 2503,  670,  719,  744,  770,  797,  825,  854,
    885,  915, 0
};
#define FT736_CTCSS_NB 42

/*
 * ft736 rigs capabilities.
 *
 * TODO:
 *  - AQS
 */

const struct rig_caps ft736_caps =
{
    RIG_MODEL(RIG_MODEL_FT736R),
    .model_name =         "FT-736R",
    .mfg_name =           "Yaesu",
    .version =            "20221218.0",
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
    .serial_handshake =   RIG_HANDSHAKE_NONE, /* RTS low, DTR high, and CTS low. */
    .write_delay =        30, /* 50ms to be real safe */
    .post_write_delay =   0,
    .timeout =            2000,
    .retry =              0,
    .has_get_func =       RIG_FUNC_NONE,
    .has_set_func =       RIG_FUNC_TONE | RIG_FUNC_TSQL,
    .has_get_level =      RIG_LEVEL_RAWSTR,
    .has_set_level =      RIG_LEVEL_BAND_SELECT,
    .has_get_parm =       RIG_PARM_NONE,
    .has_set_parm =       RIG_PARM_NONE,
    .level_gran =
    {
#include "level_gran_yaesu.h"
    },
    .vfo_ops =            RIG_OP_NONE,
    .ctcss_list =         ft736_ctcss_list,
    .preamp =             { RIG_DBLST_END, },
    .attenuator =         { RIG_DBLST_END, },
    .max_rit =            Hz(0),
    .max_xit =            Hz(0),
    .max_ifshift =        Hz(0),
    .targetable_vfo =     RIG_TARGETABLE_FREQ | RIG_TARGETABLE_MODE,
    .transceive =         RIG_TRN_OFF,
    .bank_qty =           0,
    .chan_desc_sz =       0,
    .chan_list =          {
        RIG_CHAN_END,
    },
    .rx_range_list1 =     {
        {MHz(50), MHz(53.99999), FT736_MODES, -1, -1, FT736_VFOS },
        {MHz(144), MHz(145.99999), FT736_MODES, -1, -1, FT736_VFOS },
        {MHz(430), MHz(439.99999), FT736_MODES, -1, -1, FT736_VFOS },
        {MHz(1240), MHz(1299.99999), FT736_MODES, -1, -1, FT736_VFOS },
        RIG_FRNG_END,
    }, /* Region 1 rx ranges */

    .tx_range_list1 =     {
        {MHz(50), MHz(53.99999), FT736_MODES, W(5), W(30), FT736_VFOS },
        {MHz(144), MHz(145.99999), FT736_MODES, W(5), W(60), FT736_VFOS },
        {MHz(430), MHz(439.99999), FT736_MODES, W(5), W(60), FT736_VFOS },
        {MHz(1240), MHz(1299.99999), FT736_MODES, W(5), W(45), FT736_VFOS },
        RIG_FRNG_END,
    },    /* region 1 TX ranges */

    .rx_range_list2 =     {
        {MHz(50), MHz(53.99999), FT736_MODES, -1, -1, FT736_VFOS },
        {MHz(144), MHz(147.99999), FT736_MODES, -1, -1, FT736_VFOS },
        {MHz(220), MHz(224.99999), FT736_MODES, -1, -1, FT736_VFOS },
        {MHz(430), MHz(449.99999), FT736_MODES, -1, -1, FT736_VFOS },
        {MHz(1240), MHz(1299.99999), FT736_MODES, -1, -1, FT736_VFOS },
        RIG_FRNG_END,
    }, /* Region 2 rx ranges */

    .tx_range_list2 =     {
        {MHz(50), MHz(53.99999), FT736_MODES, W(5), W(30), FT736_VFOS },
        {MHz(144), MHz(147.99999), FT736_MODES, W(5), W(60), FT736_VFOS },
        {MHz(220), MHz(224.99999), FT736_MODES, W(5), W(60), FT736_VFOS },
        {MHz(430), MHz(449.99999), FT736_MODES, W(5), W(60), FT736_VFOS },
        {MHz(1240), MHz(1299.99999), FT736_MODES, W(5), W(45), FT736_VFOS },
        RIG_FRNG_END,
    },    /* region 2 TX ranges */


    .tuning_steps =       {
        {FT736_MODES, Hz(10)},
        RIG_TS_END,
    },

    /* mode/filter list, remember: order matters! */
    .filters =            {
        {RIG_MODE_CW | RIG_MODE_SSB,    kHz(2.2)},
        {RIG_MODE_FM,    kHz(12)},
        {RIG_MODE_FMN,   kHz(8)},
        {RIG_MODE_CWN,   Hz(600)},

        RIG_FLT_END,
    },

    .str_cal = FT736_STR_CAL,

    .rig_open =       ft736_open,
    .rig_close =      ft736_close,

    .set_freq =           ft736_set_freq,
    .get_freq =           ft736_get_freq,
    .set_mode =           ft736_set_mode,
    .set_ptt =            ft736_set_ptt,
    .get_dcd =            ft736_get_dcd,
    .get_level =          ft736_get_level,

    .set_split_vfo =  ft736_set_split_vfo,
    .set_split_freq =     ft736_set_split_freq,
    .set_split_mode =     ft736_set_split_mode,

    .set_rptr_shift =     ft736_set_rptr_shift,
    .set_rptr_offs =  ft736_set_rptr_offs,

    .set_func =       ft736_set_func,
    .set_ctcss_tone = ft736_set_ctcss_tone,
    .set_ctcss_sql =  ft736_set_ctcss_sql,
    .hamlib_check_rig_caps = HAMLIB_CHECK_RIG_CAPS
};


/*
 * ft736_open  routine
 *
 */
int ft736_open(RIG *rig)
{
    unsigned char cmd[YAESU_CMD_LENGTH] = { 0x00, 0x00, 0x00, 0x00, 0x00};
    struct ft736_priv_data *priv;
    int ret;

    rig_debug(RIG_DEBUG_TRACE, "%s called\n", __func__);

    rig->state.priv = (struct ft736_priv_data *) calloc(1,
                      sizeof(struct ft736_priv_data));

    if (!rig->state.priv)
    {
        return -RIG_ENOMEM;
    }

    priv = rig->state.priv;

    priv->split = RIG_SPLIT_OFF;


    /* send Ext Cntl ON: Activate CAT */
    ret = write_block(&rig->state.rigport, cmd, YAESU_CMD_LENGTH);

    if (ret != RIG_OK)
    {
        free(priv);
    }

    return ret;
}

int ft736_close(RIG *rig)
{
    unsigned char cmd[YAESU_CMD_LENGTH] = { 0x80, 0x80, 0x80, 0x80, 0x80};

    rig_debug(RIG_DEBUG_TRACE, "%s called\n", __func__);

    free(rig->state.priv);

    /* send Ext Cntl OFF: Deactivate CAT */
    return write_block(&rig->state.rigport, cmd, YAESU_CMD_LENGTH);

}


int ft736_set_freq(RIG *rig, vfo_t vfo, freq_t freq)
{
    unsigned char cmd[YAESU_CMD_LENGTH] = { 0x00, 0x00, 0x00, 0x00, 0x01};
    struct ft736_priv_data *priv = (struct ft736_priv_data *)rig->state.priv;
    int retval;

    // we will assume requesting to set VFOB is split mode
    if (vfo == RIG_VFO_B) { return rig_set_split_freq(rig, vfo, freq); }

    if (priv->split == RIG_SPLIT_ON)
    {
        cmd[4] = 0x1e;
    }

    /* store bcd format in cmd (MSB) */
    to_bcd_be(cmd, freq / 10, 8);

    /* special case for 1.2GHz band */
    if (freq > GHz(1.2))
    {
        cmd[0] = (cmd[0] & 0x0f) | 0xc0;
    }

    retval = write_block(&rig->state.rigport, cmd, YAESU_CMD_LENGTH);

    if (retval == RIG_OK) { rig_set_cache_freq(rig, vfo, freq); }

    return retval;
}

int ft736_get_freq(RIG *rig, vfo_t vfo, freq_t *freq)
{
    rig_debug(RIG_DEBUG_VERBOSE, "%s: called\n", __func__);

    if (vfo == RIG_VFO_A || vfo == RIG_VFO_MAIN) { *freq = rig->state.cache.freqMainA; }
    else { rig_get_cache_freq(rig, vfo, freq, NULL); }

    return RIG_OK;
}




#define MD_LSB  0x00
#define MD_USB  0x01
#define MD_CW   0x02
#define MD_CWN  0x82
#define MD_FM   0x08
#define MD_FMN  0x88

int ft736_set_mode(RIG *rig, vfo_t vfo, rmode_t mode, pbwidth_t width)
{
    unsigned char cmd[YAESU_CMD_LENGTH] = { 0x00, 0x00, 0x00, 0x00, 0x07};
    unsigned char md;
    struct ft736_priv_data *priv = (struct ft736_priv_data *)rig->state.priv;

    if (vfo == RIG_VFO_B) { return ft736_set_split_mode(rig, vfo, mode, width); }

    if (priv->split == RIG_SPLIT_ON)
    {
        cmd[4] = 0x17;
    }

    /*
     * translate mode from generic to ft736 specific
     */
    switch (mode)
    {
    case RIG_MODE_CW:    md = MD_CW; break;

    case RIG_MODE_CWN:    md = MD_CWN; break;

    case RIG_MODE_USB:    md = MD_USB; break;

    case RIG_MODE_LSB:    md = MD_LSB; break;

    case RIG_MODE_FM:    md = MD_FM; break;

    case RIG_MODE_FMN:    md = MD_FMN; break;

    default:
        return -RIG_EINVAL;         /* sorry, wrong MODE */
    }

    if (width != RIG_PASSBAND_NOCHANGE
            && width != RIG_PASSBAND_NORMAL
            && width < rig_passband_normal(rig, mode))
    {
        md |= 0x80;
    }

    cmd[0] = md;

    /* Mode set */
    return write_block(&rig->state.rigport, cmd, YAESU_CMD_LENGTH);
}



int ft736_set_split_vfo(RIG *rig, vfo_t vfo, split_t split, vfo_t tx_vfo)
{
    unsigned char cmd[YAESU_CMD_LENGTH] = { 0x00, 0x00, 0x00, 0x00, 0x8e};
    struct ft736_priv_data *priv = (struct ft736_priv_data *)rig->state.priv;
    int ret;

    /*
     * this can be misleading as Yaesu call it "Full duplex"
     * or "sat mode", and split Yaesu terms is repeater shift.
     */
    cmd[4] = split == RIG_SPLIT_ON ? 0x0e : 0x8e;

    ret = write_block(&rig->state.rigport, cmd, YAESU_CMD_LENGTH);

    if (ret == RIG_OK)
    {
        priv->split = split;
    }

    return ret;
}

int ft736_set_split_freq(RIG *rig, vfo_t vfo, freq_t freq)
{
    unsigned char cmd[YAESU_CMD_LENGTH] = { 0x00, 0x00, 0x00, 0x00, 0x2e};

    int retval = rig_set_split_vfo(rig, RIG_VFO_A, RIG_SPLIT_ON, RIG_VFO_B);

    if (retval != RIG_OK) { RETURNFUNC(retval); }

    /* store bcd format in cmd (MSB) */
    to_bcd_be(cmd, freq / 10, 8);

    /* special case for 1.2GHz band */
    if (freq > GHz(1.2))
    {
        cmd[0] = (cmd[0] & 0x0f) | 0xc0;
    }

    /* Frequency set */
    return write_block(&rig->state.rigport, cmd, YAESU_CMD_LENGTH);
}


int ft736_set_split_mode(RIG *rig, vfo_t vfo, rmode_t mode, pbwidth_t width)
{
    unsigned char cmd[YAESU_CMD_LENGTH] = { 0x00, 0x00, 0x00, 0x00, 0x27};
    unsigned char md;


    /*
     * translate mode from generic to ft736 specific
     */
    switch (mode)
    {
    case RIG_MODE_CW:     md = MD_CW; break;

    case RIG_MODE_CWN:    md = MD_CWN; break;

    case RIG_MODE_USB:    md = MD_USB; break;

    case RIG_MODE_LSB:    md = MD_LSB; break;

    case RIG_MODE_FM:     md = MD_FM; break;

    case RIG_MODE_FMN:    md = MD_FMN; break;

    default:
        return -RIG_EINVAL;         /* sorry, wrong MODE */
    }

    if (RIG_PASSBAND_NOCHANGE != width
            && width != RIG_PASSBAND_NORMAL
            && width < rig_passband_normal(rig, mode))
    {
        md |= 0x80;
    }

    cmd[0] = md;

    /* Mode set */
    return write_block(&rig->state.rigport, cmd, YAESU_CMD_LENGTH);
}




int ft736_set_ptt(RIG *rig, vfo_t vfo, ptt_t ptt)
{
    unsigned char cmd[YAESU_CMD_LENGTH] = { 0x00, 0x00, 0x00, 0x00, 0x88};

    cmd[4] = ptt == RIG_PTT_ON ? 0x08 : 0x88;

    /* Tx/Rx set */
    return write_block(&rig->state.rigport, cmd, YAESU_CMD_LENGTH);
}

int ft736_get_dcd(RIG *rig, vfo_t vfo, dcd_t *dcd)
{
    unsigned char cmd[YAESU_CMD_LENGTH] = { 0x00, 0x00, 0x00, 0x00, 0xe7};
    int retval;

    rig_flush(&rig->state.rigport);

    retval = write_block(&rig->state.rigport, cmd, YAESU_CMD_LENGTH);

    if (retval < 0)
    {
        return retval;
    }

    /* read back the 1 byte */
    retval = read_block(&rig->state.rigport, cmd, 5);

    if (retval < 1)
    {
        rig_debug(RIG_DEBUG_ERR, "%s: read squelch failed %d\n",
                  __func__, retval);

        return retval < 0 ? retval : -RIG_EIO;
    }

    *dcd = cmd[0] == 0x00 ? RIG_DCD_OFF : RIG_DCD_ON;

    return RIG_OK;
}

int ft736_get_level(RIG *rig, vfo_t vfo, setting_t level, value_t *val)
{
    unsigned char cmd[YAESU_CMD_LENGTH] = { 0x00, 0x00, 0x00, 0x00, 0xf7};
    int retval;

    if (level != RIG_LEVEL_RAWSTR)
    {
        return -RIG_EINVAL;
    }

    rig_flush(&rig->state.rigport);

    /* send Test S-meter cmd to rig  */
    retval = write_block(&rig->state.rigport, cmd, YAESU_CMD_LENGTH);

    if (retval < 0)
    {
        return retval;
    }

    /* read back the 1 byte */
    retval = read_block(&rig->state.rigport, cmd, 5);

    if (retval < 1)
    {
        rig_debug(RIG_DEBUG_ERR, "%s: read meter failed %d\n",
                  __func__, retval);

        return retval < 0 ? retval : -RIG_EIO;
    }

    val->i = cmd[0];

    return RIG_OK;
}



int ft736_set_rptr_shift(RIG *rig, vfo_t vfo, rptr_shift_t shift)
{
    unsigned char cmd[YAESU_CMD_LENGTH] = { 0x00, 0x00, 0x00, 0x00, 0x89};

    switch (shift)
    {
    case RIG_RPT_SHIFT_NONE:
        /* There's a typo in the manual.
         * "Split Dir. simplex" is in fact 0x89, and 0x88 is PTT off!
         */
        cmd[4] = 0x89;
        break;

    case RIG_RPT_SHIFT_MINUS:
        cmd[4] = 0x09;
        break;

    case RIG_RPT_SHIFT_PLUS:
        cmd[4] = 0x49;
        break;

    default:
        return -RIG_EINVAL;
    }

    return write_block(&rig->state.rigport, cmd, YAESU_CMD_LENGTH);
}

int ft736_set_rptr_offs(RIG *rig, vfo_t vfo, shortfreq_t offs)
{
    unsigned char cmd[YAESU_CMD_LENGTH] = { 0x00, 0x00, 0x00, 0x00, 0xf9};

    /* store bcd format in cmd (MSB) */
    to_bcd_be(cmd, offs / 10, 8);

    /* Offset set */
    return write_block(&rig->state.rigport, cmd, YAESU_CMD_LENGTH);
}

int ft736_set_func(RIG *rig, vfo_t vfo, setting_t func, int status)
{
    unsigned char cmd[YAESU_CMD_LENGTH] = { 0x00, 0x00, 0x00, 0x00, 0x8a};

    switch (func)
    {
    case RIG_FUNC_TONE:
        cmd[4] = status ? 0x4a : 0x8a;
        break;

    case RIG_FUNC_TSQL:
        cmd[4] = status ? 0x0a : 0x8a;
        break;

    default:
        return -RIG_EINVAL;
    }

    return write_block(&rig->state.rigport, cmd, YAESU_CMD_LENGTH);
}

int ft736_set_ctcss_tone(RIG *rig, vfo_t vfo, tone_t tone)
{
    unsigned char cmd[YAESU_CMD_LENGTH] = { 0x00, 0x00, 0x00, 0x00, 0xfa};
    int i;

    for (i = 0; i < FT736_CTCSS_NB; i++)
    {
        if (ft736_ctcss_list[i] == tone)
        {
            break;
        }
    }

    if (i == FT736_CTCSS_NB)
    {
        return -RIG_EINVAL;
    }

    cmd[0] = 0x3e - i;

    return write_block(&rig->state.rigport, cmd, YAESU_CMD_LENGTH);
}

int ft736_set_ctcss_sql(RIG *rig, vfo_t vfo, tone_t tone)
{
    /* same opcode as tone */
    return ft736_set_ctcss_tone(rig, vfo, tone);
}

