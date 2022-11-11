/*
 * vr5000.c - (C) Stephane Fillod and Jacob Heder 2005
 *
 * This shared library provides an API for communicating
 * via serial interface to an VR-5000 using the "CAT" interface
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
 * Undocumented notes on the VR5000.
 *
 * There are some mishaps in the manual. The CAT serial delay times seems
 * not to be correct. More correct estimates are 70 mS write_delay (
 * between bytes), 200 mS post_write_delay (between command), but when
 * reading s-meter, the delay must be over 500 mS.
 *
 * The read s-meter CAT command seems only to return 17 to 23 depending
 * on the strength of the signal. Setting the RF attenuator on with no
 * attenna on does not decrease the level below 17. If you wish to read
 * the s-meter on a specific frequency, set the frequency and wait a
 * 500-1000 mS before reading it.

 * The vr5000 has two vfo, but only 1 native. The second vfo is a following
 * vfo which only can tune into the frequency range of VFO_A (+,-) 20 Mhz.
 *
 * The vr5000 has no CAT commands for reading the frequency, ts nor mode.
 * These function are emulated, because the vr5000 thunkates the input
 * frequency. Secondly when changing the mode, ts will change, and since
 * ts it the one that decides how the frequency is thunkated, the frequency
 * will change.
 *
 * True receiver range was not specified correctly in manual. No all
 * mode allow to go down to 100 Khz. Therefore the minimum frequency
 * which will be allowed is 101.5 kKz. Maximum is 2599.99 Mhz.
 *
 *
 * Supported : VFO_A, 101.5 Khz to 2599.99 Mhz.
 */


#include <hamlib/config.h>

// cppcheck-suppress *
#include <stdlib.h>

#include "hamlib/rig.h"
#include "serial.h"
#include "misc.h"
#include "yaesu.h"

#define VR5000_MODES (RIG_MODE_CW|RIG_MODE_SSB|RIG_MODE_AM|RIG_MODE_FM|RIG_MODE_WFM)
#define VR5000_VFOS RIG_VFO_A
#define VR5000_ANTS 0

#define MODE_LSB    0x00
#define MODE_USB    0x01
#define MODE_CW     0x02
#define MODE_AM     0x04
#define MODE_AMW    0x44
#define MODE_AMN    0x84
#define MODE_WFM    0x48
#define MODE_FMN    0x88

#define SAFETY_WRITE_DELAY      70  /* security value for beta version, ok to set lower later */
#define SAFETY_POST_DELAY       210 /* security value for beta version, */


/* TODO: get real measure numbers */
#define VR5000_STR_CAL { 2, { \
        {  0, -60 }, /* S0 -6dB */ \
        { 63,  60 }  /* +60 */ \
        } }

/* Private helper function prototypes */

static int vr5000_init(RIG *rig);
static int vr5000_cleanup(RIG *rig);
static int vr5000_open(RIG *rig);
static int vr5000_close(RIG *rig);

static int vr5000_set_freq(RIG *rig, vfo_t vfo, freq_t freq);
static int vr5000_get_freq(RIG *rig, vfo_t vfo, freq_t *freq);
static int vr5000_set_mode(RIG *rig, vfo_t vfo, rmode_t mode, pbwidth_t width);
static int vr5000_get_mode(RIG *rig, vfo_t vfo, rmode_t *mode,
                           pbwidth_t *width);
static int vr5000_get_level(RIG *rig, vfo_t vfo, setting_t level, value_t *val);
static int vr5000_get_dcd(RIG *rig, vfo_t vfo, dcd_t *dcd);
static int vr5000_set_ts(RIG *rig, vfo_t vfo, shortfreq_t ts);
static int vr5000_get_ts(RIG *rig, vfo_t vfo, shortfreq_t *ts);


/*
 * Private helper function prototypes.
 */

static int  set_vr5000(RIG *rig, vfo_t vfo, freq_t freq, rmode_t mode,
                       pbwidth_t width, shortfreq_t ts);
static int  mode2rig(RIG *rig, vfo_t vfo, rmode_t mode, pbwidth_t width);
static void correct_frequency(RIG *rig, vfo_t vfo, freq_t curr_freq,
                              freq_t *freq);
static int  find_tuning_step(RIG *rig, vfo_t vfo, rmode_t mode,
                             shortfreq_t *ts);
static int  check_tuning_step(RIG *rig, vfo_t vfo, rmode_t mode,
                              shortfreq_t ts);


/*
 * vr5000 rigs capabilities.
 */

const struct rig_caps vr5000_caps =
{
    RIG_MODEL(RIG_MODEL_VR5000),
    .model_name =         "VR-5000",
    .mfg_name =           "Yaesu",
    .version =            "20200505.0",
    .copyright =          "LGPL",
    .status =             RIG_STATUS_STABLE,
    .rig_type =           RIG_TYPE_RECEIVER,
    .ptt_type =           RIG_PTT_NONE,
    .dcd_type =           RIG_DCD_RIG,
    .port_type =          RIG_PORT_SERIAL,
    .serial_rate_min =    4800,
    .serial_rate_max =    57600,
    .serial_data_bits =   8,
    .serial_stop_bits =   2,
    .serial_parity =      RIG_PARITY_NONE,
    .serial_handshake =   RIG_HANDSHAKE_NONE,
    .write_delay =        SAFETY_WRITE_DELAY,
    .post_write_delay =   SAFETY_POST_DELAY,
    .timeout =            1000,
    .retry =              0,
    .has_get_func =       RIG_FUNC_NONE,
    .has_set_func =       RIG_FUNC_NONE,
    .has_get_level =      RIG_LEVEL_RAWSTR,
    .has_set_level =      RIG_LEVEL_BAND_SELECT,
    .has_get_parm =       RIG_PARM_NONE,
    .has_set_parm =       RIG_PARM_NONE,
    .level_gran =
    {
#include "level_gran_yaesu.h"
    },
    .vfo_ops =            RIG_OP_NONE,
    .preamp =             { RIG_DBLST_END, },
    .attenuator =         { RIG_DBLST_END, },
    .max_rit =            Hz(0),
    .max_xit =            Hz(0),
    .max_ifshift =        Hz(0),
    .targetable_vfo =     RIG_TARGETABLE_FREQ | RIG_TARGETABLE_MODE,
    .transceive =         RIG_TRN_OFF,
    .bank_qty =           0,
    .chan_desc_sz =       0,
    .chan_list =          { },
    .rx_range_list1 =     {
        {kHz(101) + 500, GHz(2.6) - 1000, VR5000_MODES, -1, -1, RIG_VFO_A, VR5000_ANTS },
        RIG_FRNG_END,
    }, /* api supported region 1 rx ranges */

    .tx_range_list1 =     {
        RIG_FRNG_END,
    },    /* region 1 TX ranges */

    .rx_range_list2 =     {
        {kHz(101) + 500, GHz(2.6) - 1000, VR5000_MODES, -1, -1, RIG_VFO_A, VR5000_ANTS },
        RIG_FRNG_END,
    }, /* api supported region 2 rx ranges */

    .tx_range_list2 =     {
        RIG_FRNG_END,
    },    /* region 2 TX ranges */

    .tuning_steps =       {
        {RIG_MODE_SSB | RIG_MODE_CW, Hz(20)},
        {RIG_MODE_SSB | RIG_MODE_CW, Hz(100)},
        {RIG_MODE_SSB | RIG_MODE_CW, Hz(500)},
        {RIG_MODE_AM | RIG_MODE_SSB | RIG_MODE_CW, kHz(1)},
        {RIG_MODE_FM | RIG_MODE_SSB | RIG_MODE_CW | RIG_MODE_AM, kHz(5)},
        {RIG_MODE_FM, kHz(6.25)},
        {RIG_MODE_AM, kHz(9)},
        {RIG_MODE_AM | RIG_MODE_WFM | RIG_MODE_FM, kHz(10)},
        {RIG_MODE_FM, kHz(12.5)},
        {RIG_MODE_AM | RIG_MODE_FM, kHz(20)},
        {RIG_MODE_AM | RIG_MODE_FM, kHz(25)},
        {RIG_MODE_AM | RIG_MODE_WFM | RIG_MODE_FM, kHz(50)},
        {RIG_MODE_AM | RIG_MODE_WFM | RIG_MODE_FM, kHz(100)},
        {RIG_MODE_AM | RIG_MODE_WFM | RIG_MODE_FM, kHz(500)},
        RIG_TS_END,
    },

    /* mode/filter list, remember: order matters! */
    .filters =            {
        {RIG_MODE_AM,   kHz(6)},
        {RIG_MODE_SSB | RIG_MODE_CW | RIG_MODE_AM,  kHz(2.4)},
        {RIG_MODE_AM | RIG_MODE_FM,   kHz(15)},
        {RIG_MODE_WFM,  kHz(230)},
        RIG_FLT_END,
    },

    .str_cal =        VR5000_STR_CAL,

    .rig_init =       vr5000_init,
    .rig_cleanup =    vr5000_cleanup,
    .rig_open =       vr5000_open,
    .rig_close =      vr5000_close,

    .get_level =      vr5000_get_level,
    .get_dcd =        vr5000_get_dcd,
    .set_freq =       vr5000_set_freq,
    .get_freq =       vr5000_get_freq,
    .set_mode =       vr5000_set_mode,
    .get_mode =       vr5000_get_mode,
    .set_ts =         vr5000_set_ts,
    .get_ts =         vr5000_get_ts,
    .hamlib_check_rig_caps = HAMLIB_CHECK_RIG_CAPS
};


/*
 * VR-5000 backend needs priv data to handle composite cmds
 */

struct vr5000_priv_data
{

    vfo_t curr_vfo;
    shortfreq_t curr_ts;
    freq_t curr_freq;
    rmode_t curr_mode;
    pbwidth_t curr_width;
};


int vr5000_init(RIG *rig)
{
    rig->state.priv = (struct vr5000_priv_data *) calloc(1,
                      sizeof(struct vr5000_priv_data));

    if (!rig->state.priv) { return -RIG_ENOMEM; }

    return RIG_OK;
}


int vr5000_cleanup(RIG *rig)
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

    return RIG_OK;
}


/*
 * vr5000_open  : Set CAT on and set tuner into known mode
 */
int vr5000_open(RIG *rig)
{
    struct vr5000_priv_data *priv = rig->state.priv;
    unsigned char cmd[YAESU_CMD_LENGTH]   = { 0x00, 0x00, 0x00, 0x00, 0x00};
    unsigned char b_off[YAESU_CMD_LENGTH] = { 0x00, 0x00, 0x00, 0x00, 0x31};

    int retval;

    /* CAT write command on */
    retval =  write_block(&rig->state.rigport, cmd, YAESU_CMD_LENGTH);

    if (retval != RIG_OK)
    {
        return retval;
    }

    /* disable RIG_VFO_B  (only on display) */
    retval =  write_block(&rig->state.rigport, b_off, YAESU_CMD_LENGTH);

    if (retval != RIG_OK)
    {
        return retval;
    }

    /* set RIG_VFO_A to 10 Mhz normal AM, step 10 kHz */
    priv->curr_vfo  = RIG_VFO_A;
    priv->curr_mode = RIG_MODE_WFM;
    priv->curr_width = RIG_PASSBAND_NORMAL;
    priv->curr_ts   = kHz(10);
    priv->curr_freq = kHz(10000);

    retval = set_vr5000(rig, priv->curr_vfo, priv->curr_freq, priv->curr_mode,
                        priv->curr_width, priv->curr_ts);

    if (retval != RIG_OK)
    {
        return retval;
    }

    return RIG_OK;

}


int vr5000_close(RIG *rig)
{
    unsigned char cmd[YAESU_CMD_LENGTH] = { 0x00, 0x00, 0x00, 0x00, 0x80};

    return write_block(&rig->state.rigport, cmd, YAESU_CMD_LENGTH);
}


int vr5000_set_freq(RIG *rig, vfo_t vfo, freq_t freq)
{
    struct vr5000_priv_data *priv = rig->state.priv;

    return set_vr5000(rig, vfo, freq, priv->curr_mode, priv->curr_width,
                      priv->curr_ts);
}


int vr5000_get_freq(RIG *rig, vfo_t vfo, freq_t *freq)
{

    struct vr5000_priv_data *priv = rig->state.priv;
    *freq = priv->curr_freq;

    return RIG_OK;
}


int vr5000_set_mode(RIG *rig, vfo_t vfo, rmode_t mode, pbwidth_t width)
{
    struct vr5000_priv_data *priv = rig->state.priv;

    if (check_tuning_step(rig, vfo, mode, priv->curr_ts) != RIG_OK)
    {
        find_tuning_step(rig, vfo, mode, &priv->curr_ts);
    }

    priv->curr_mode = mode;
    return set_vr5000(rig, vfo, priv->curr_freq, mode, width, priv->curr_ts);
}


int vr5000_get_mode(RIG *rig, vfo_t vfo, rmode_t *mode, pbwidth_t *width)
{
    struct vr5000_priv_data *priv = rig->state.priv;
    *mode = priv->curr_mode;
    *width = priv->curr_width;

    return RIG_OK;
}


int vr5000_set_ts(RIG *rig, vfo_t vfo, shortfreq_t ts)
{
    struct vr5000_priv_data *priv = rig->state.priv;
    int retval;

    retval = check_tuning_step(rig, vfo, priv->curr_mode, ts);

    if (retval != RIG_OK)
    {
        return retval;
    }

    priv->curr_ts = ts;
    return set_vr5000(rig, vfo, priv->curr_freq, priv->curr_mode, priv->curr_width,
                      ts);
}


int vr5000_get_ts(RIG *rig, vfo_t vfo, shortfreq_t *ts)
{
    struct vr5000_priv_data *priv = rig->state.priv;
    *ts = priv->curr_ts;

    return RIG_OK;
}


int vr5000_get_level(RIG *rig, vfo_t vfo, setting_t level, value_t *val)
{
    unsigned char cmd[YAESU_CMD_LENGTH] = { 0x00, 0x00, 0x00, 0x00, 0xe7};
    int retval;

    if (level != RIG_LEVEL_RAWSTR)
    {
        return -RIG_EINVAL;
    }

    rig_flush(&rig->state.rigport);

    /* send READ STATUS(Meter only) cmd to rig  */
    retval = write_block(&rig->state.rigport, cmd, YAESU_CMD_LENGTH);

    if (retval < 0)
    {
        return retval;
    }

    /* read back the 1 byte */
    retval = read_block(&rig->state.rigport, cmd, 1);

    if (retval < 1)
    {
        rig_debug(RIG_DEBUG_ERR, "%s: read meter failed %d\n",
                  __func__, retval);

        return retval < 0 ? retval : -RIG_EIO;
    }

    val->i = cmd[0] & 0x3f;
    rig_debug(RIG_DEBUG_ERR, "Read(%x) RawValue(%x): \n", cmd[0], val->i);


    return RIG_OK;
}


int vr5000_get_dcd(RIG *rig, vfo_t vfo, dcd_t *dcd)
{
    unsigned char cmd[YAESU_CMD_LENGTH] = { 0x00, 0x00, 0x00, 0x00, 0xe7};
    int retval;

    rig_flush(&rig->state.rigport);

    /* send READ STATUS(Meter only) cmd to rig  */
    retval = write_block(&rig->state.rigport, cmd, YAESU_CMD_LENGTH);

    if (retval < 0)
    {
        return retval;
    }

    /* read back the 1 byte */
    retval = read_block(&rig->state.rigport, cmd, 1);

    if (retval < 1)
    {
        rig_debug(RIG_DEBUG_ERR, "%s: read meter failed %d\n",
                  __func__, retval);

        return retval < 0 ? retval : -RIG_EIO;
    }

    *dcd = (cmd[0] & 0x80) ? RIG_DCD_ON : RIG_DCD_OFF;

    return RIG_OK;
}


int mode2rig(RIG *rig, vfo_t vfo, rmode_t mode, pbwidth_t width)
{
    int md;

    /*
     * translate mode from generic to vr5000 specific
     */

    switch (mode)
    {
    case RIG_MODE_USB:    md = MODE_USB; break;

    case RIG_MODE_LSB:    md = MODE_LSB; break;

    case RIG_MODE_CW:     md = MODE_CW; break;

    case RIG_MODE_WFM:    md = MODE_WFM; break;

    case RIG_MODE_FM:     md = MODE_FMN; break;

    case RIG_MODE_AM:
        if (width != RIG_PASSBAND_NOCHANGE
                && width != RIG_PASSBAND_NORMAL
                && width < rig_passband_normal(rig, mode))
        {
            md = MODE_AMN;
        }
        else if (width != RIG_PASSBAND_NORMAL && width > rig_passband_normal(rig, mode))
        {
            md = MODE_AMW;
        }
        else
        {
            md = MODE_AM;
        }

        break;

    default:
        return -RIG_EINVAL;         /* sorry, wrong MODE */
    }

    return md;
}


/*
 * This function corrects the frequency
 */
void correct_frequency(RIG *rig, vfo_t vfo, freq_t curr_freq, freq_t *freq)
{

    struct vr5000_priv_data *priv = rig->state.priv;
    shortfreq_t ts = priv->curr_ts;
    unsigned long long correct_freq = (unsigned long long)curr_freq;

    /* RIG_VFO_A frequency correction */
    if (correct_freq % ts != 0)
    {
        if ((correct_freq % ts) > (ts >> 1))
        {
            correct_freq += (ts - (correct_freq % ts));
        }
        else
        {
            correct_freq -= (correct_freq % ts);
        }
    }

    /* Check for frequencies out on true rx range */
    if ((freq_t)correct_freq < rig->caps->rx_range_list1->startf)
    {
        correct_freq = (unsigned long long)rig->caps->rx_range_list1->startf;

        if (correct_freq % ts != 0)
        {
            correct_freq += (ts - (correct_freq % ts));
        }
    }
    else if ((freq_t)correct_freq > rig->caps->rx_range_list1->endf)
    {
        correct_freq = (unsigned long long)rig->caps->rx_range_list1->endf;

        if (correct_freq % ts != 0)
        {
            correct_freq -= (correct_freq % ts);
        }
    }

    *freq = (freq_t) correct_freq;
    return;
}


/*
 * Set mode and ts, then frequency. Both mode/ts and frequency are set
 * every time one of them changes.
 */
int set_vr5000(RIG *rig, vfo_t vfo, freq_t freq, rmode_t mode, pbwidth_t width,
               shortfreq_t ts)
{
    struct vr5000_priv_data *priv = rig->state.priv;
    unsigned char cmd_mode_ts[YAESU_CMD_LENGTH] = { 0x00, 0x00, 0x00, 0x00, 0x07};
    unsigned char cmd_freq[YAESU_CMD_LENGTH]    = { 0x00, 0x00, 0x00, 0x00, 0x01};
    static const unsigned char steps[] =
    {
        0x21, 0x42, 0x02, 0x03, 0x43, 0x53, 0x63,
        0x04, 0x14, 0x24, 0x35, 0x44, 0x05, 0x45
    };
    unsigned int frq;
    int retval;
    int i;

    if (vfo == RIG_VFO_CURR)
    {
        vfo = priv->curr_vfo;
    }

    retval = mode2rig(rig, vfo, mode, width);

    if (retval < 0)
    {
        return retval;
    }

    /* fill in m1 */
    cmd_mode_ts[0] = retval;

    for (i = 0; i < sizeof(steps); i++)
    {
        if (rig->caps->tuning_steps[i].ts == ts)
        {
            break;
        }
    }

    if (i >= sizeof(steps))
    {
        return -RIG_EINVAL;     /* not found, unsupported */
    }

    /* fill in m2 */
    cmd_mode_ts[1] = steps[i];

    retval =  write_block(&rig->state.rigport, cmd_mode_ts,
                          YAESU_CMD_LENGTH);

    if (retval != RIG_OK)
    {
        return retval;
    }

    /* Correct frequency */
    correct_frequency(rig, vfo, freq, &freq);
    priv->curr_freq = freq;

    frq = (unsigned int)(freq / 10);

    cmd_freq[0] = (frq >> 24) & 0xff;
    cmd_freq[1] = (frq >> 16) & 0xff;
    cmd_freq[2] = (frq >> 8) & 0xff;
    cmd_freq[3] = frq & 0xff;

    /* frequency set */
    return write_block(&rig->state.rigport, cmd_freq, YAESU_CMD_LENGTH);
}


/*
 * find_tuning_step : return the lowest ts for a giving mode
 */
int find_tuning_step(RIG *rig, vfo_t vfo, rmode_t mode, shortfreq_t *ts)
{
    int i;

    for (i = 0; i < HAMLIB_TSLSTSIZ; i++)
    {
        if ((rig->caps->tuning_steps[i].modes & mode) != 0)
        {
            *ts = rig->caps->tuning_steps[i].ts;
            return RIG_OK;
        }
    }

    return -RIG_EINVAL;     /* not found, unsupported */
}

/*
 * check_tuning_step : return RIG_OK if this ts is supported by the mode
 */
int check_tuning_step(RIG *rig, vfo_t vfo, rmode_t mode, shortfreq_t ts)
{
    int i;

    for (i = 0; i < HAMLIB_TSLSTSIZ; i++)
    {
        if (rig->caps->tuning_steps[i].ts == ts &&
                ((rig->caps->tuning_steps[i].modes & mode) != 0))
        {
            return RIG_OK;
        }
    }

    return -RIG_EINVAL;     /* not found, unsupported */
}

