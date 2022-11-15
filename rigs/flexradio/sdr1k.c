/*
 *  Hamlib backend - SDR-1000
 *  Copyright (c) 2003-2010 by Stephane Fillod
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
#include <math.h>

#include "hamlib/rig.h"
#include "parallel.h"
#include "misc.h"
#include "bandplan.h"
#include "register.h"

#include "flexradio.h"

static int sdr1k_set_freq(RIG *rig, vfo_t vfo, freq_t freq);
static int sdr1k_get_freq(RIG *rig, vfo_t vfo, freq_t *freq);
static int sdr1k_reset(RIG *rig, reset_t reset);
static int sdr1k_init(RIG *rig);
static int sdr1k_open(RIG *rig);
static int sdr1k_close(RIG *rig);
static int sdr1k_cleanup(RIG *rig);
static int sdr1k_set_ptt(RIG *rig, vfo_t vfo, ptt_t ptt);
static int sdr1k_set_level(RIG *rig, vfo_t vfo, setting_t level, value_t val);

typedef enum { L_EXT = 0, L_BAND = 1, L_DDS0 = 2, L_DDS1 = 3 } latch_t;

#define TR  0x40
#define MUTE    0x80
#define GAIN    0x80
#define WRB 0x40
#define RESET   0x80

/* DDS Control Constants */
#define COMP_PD     0x10    /* DDS Comparator power down */
#define DIG_PD      0x01    /* DDS Digital Power down */
#define BYPASS_PLL  0x20    /* Bypass DDS PLL */
#define INT_IOUD    0x01    /* Internal IO Update */
#define OSK_EN      0x20    /* Offset Shift Keying enable */
#define OSK_INT     0x10    /* Offset Shift Keying */
#define BYPASS_SINC 0x40    /* Bypass Inverse Sinc Filter */
#define PLL_RANGE   0x40    /* Set PLL Range */

static int write_latch(RIG *rig, latch_t which, unsigned value, unsigned mask);
static int dds_write_reg(RIG *rig, unsigned addr, unsigned data);
static int set_bit(RIG *rig, latch_t reg, unsigned bit, unsigned state);


#define DEFAULT_XTAL MHz(200)
#define DEFAULT_PLL_MULT 1
#define DEFAULT_DAC_MULT 4095

struct sdr1k_priv_data
{
    unsigned  shadow[4];  /* shadow latches */
    freq_t    dds_freq;   /* current freq */
    freq_t    xtal;       /* base XTAL */
    int   pll_mult;       /* PLL mult */
};


#define SDR1K_FUNC  RIG_FUNC_MUTE
#define SDR1K_LEVEL RIG_LEVEL_PREAMP
#define SDR1K_PARM  RIG_PARM_NONE

#define SDR1K_MODES (RIG_MODE_USB|RIG_MODE_CW)

#define SDR1K_VFO RIG_VFO_A

#define SDR1K_ANTS 0


/* ************************************************************************* */
/*
 * http://www.flex-radio.com
 * SDR-1000 rig capabilities.
 *
 *
 * TODO: RIG_FUNC_MUTE, set_external_pin?
 *
 *    def set_mute (self, mute = 1):
 *      self.set_bit(1, 7, mute)
 *
 *    def set_unmute (self):
 *      self.set_bit(1, 7, 0)
 *
 *    def set_external_pin (self, pin, on = 1):
 *      assert (pin < 8 and pin > 0), "Out of range 1..7"
 *      self.set_bit(0, pin-1, on)
 *
 *    def read_input_pin
 *
 *    set_conf(XTAL,PLL_mult,spur_red)
 *
 *    What about IOUD_Clock?
 */

const struct rig_caps sdr1k_rig_caps =
{
    RIG_MODEL(RIG_MODEL_SDR1000),
    .model_name =     "SDR-1000",
    .mfg_name =       "Flex-radio",
    .version =        "20200323.0",
    .copyright =      "LGPL",
    .status =         RIG_STATUS_ALPHA,
    .rig_type =       RIG_TYPE_TUNER,
    .targetable_vfo =      0,
    .ptt_type =       RIG_PTT_RIG,
    .dcd_type =       RIG_DCD_NONE,
    .port_type =      RIG_PORT_PARALLEL,

    .has_get_func =   SDR1K_FUNC,
    .has_set_func =   SDR1K_FUNC,
    .has_get_level =  SDR1K_LEVEL,
    .has_set_level =  RIG_LEVEL_SET(SDR1K_LEVEL),
    .has_get_parm =    SDR1K_PARM,
    .has_set_parm =    RIG_PARM_SET(SDR1K_PARM),
    .chan_list =   {
        RIG_CHAN_END,
    },
    .scan_ops =    RIG_SCAN_NONE,
    .vfo_ops =     RIG_OP_NONE,
    .transceive =     RIG_TRN_OFF,
    .attenuator =     { RIG_DBLST_END, },
    .preamp =      { 14, RIG_DBLST_END, },

    .rx_range_list1 =  { {
            .startf = Hz(1), .endf = MHz(65), .modes = SDR1K_MODES,
            .low_power = -1, .high_power = -1, SDR1K_VFO
        },
        RIG_FRNG_END,
    },
    .tx_range_list1 =  {
        /* restricted to ham band */
        FRQ_RNG_HF(1, SDR1K_MODES, W(1), W(1), SDR1K_VFO, SDR1K_ANTS),
        FRQ_RNG_6m(1, SDR1K_MODES, W(1), W(1), SDR1K_VFO, SDR1K_ANTS),
        RIG_FRNG_END,
    },

    .rx_range_list2 =  { {
            .startf = Hz(1), .endf = MHz(65), .modes = SDR1K_MODES,
            .low_power = -1, .high_power = -1, SDR1K_VFO
        },
        RIG_FRNG_END,
    },
    .tx_range_list2 =  {
        /* restricted to ham band */
        FRQ_RNG_HF(2, SDR1K_MODES, W(1), W(1), SDR1K_VFO, SDR1K_ANTS),
        FRQ_RNG_6m(2, SDR1K_MODES, W(1), W(1), SDR1K_VFO, SDR1K_ANTS),
        RIG_FRNG_END,
    },

    .tuning_steps =  { {SDR1K_MODES, 1},
        RIG_TS_END,
    },
    .filters =  {
        {RIG_MODE_ALL, RIG_FLT_ANY},
        RIG_FLT_END
    },
    .priv =  NULL,    /* priv */

    .rig_init =     sdr1k_init,
    .rig_open =     sdr1k_open,
    .rig_close =    sdr1k_close,
    .rig_cleanup =  sdr1k_cleanup,

    .set_freq =     sdr1k_set_freq,
    .get_freq =     sdr1k_get_freq,
    .set_ptt  =     sdr1k_set_ptt,
    .reset    =     sdr1k_reset,
    .set_level =     sdr1k_set_level,
//  .set_func =     sdr1k_set_func,
    .hamlib_check_rig_caps = HAMLIB_CHECK_RIG_CAPS
};


/* ************************************************************************* */

int sdr1k_init(RIG *rig)
{
    struct sdr1k_priv_data *priv;

    rig->state.priv = (struct sdr1k_priv_data *)calloc(1, sizeof(
                          struct sdr1k_priv_data));

    if (!rig->state.priv)
    {
        /* whoops! memory shortage! */
        return -RIG_ENOMEM;
    }

    priv = rig->state.priv;

    priv->dds_freq = RIG_FREQ_NONE;
    priv->xtal = DEFAULT_XTAL;
    priv->pll_mult = DEFAULT_PLL_MULT;

    return RIG_OK;
}

static void pdelay(RIG *rig)
{
    unsigned char r;
    par_read_data(&rig->state.rigport, &r); /* ~1us */
}

int sdr1k_open(RIG *rig)
{
    struct sdr1k_priv_data *priv = (struct sdr1k_priv_data *)rig->state.priv;

    priv->shadow[0] = 0;
    priv->shadow[1] = 0;
    priv->shadow[2] = 0;
    priv->shadow[3] = 0;

    sdr1k_reset(rig, 1);

    return RIG_OK;
}

int sdr1k_close(RIG *rig)
{
    /* TODO: release relays? */

    return RIG_OK;
}

int sdr1k_cleanup(RIG *rig)
{
    struct sdr1k_priv_data *priv = (struct sdr1k_priv_data *)rig->state.priv;

    if (priv)
    {
        free(priv);
    }

    rig->state.priv = NULL;

    return RIG_OK;
}

static int set_band(RIG *rig, freq_t freq)
{
    int band, ret;

    /* set_band */
    if (freq <= MHz(2.25))
    {
        band = 0;
    }
    else if (freq <= MHz(5.5))
    {
        band = 1;
    }
    else if (freq <= MHz(11))
    {
        band = 3;    /* due to wiring mistake on board */
    }
    else if (freq <= MHz(22))
    {
        band = 2;    /* due to wiring mistake on board */
    }
    else if (freq <= MHz(37.5))
    {
        band = 4;
    }
    else
    {
        band = 5;
    }

    ret = write_latch(rig, L_BAND, 1 << band, 0x3f);

    // cppcheck-suppress *
    rig_debug(RIG_DEBUG_VERBOSE, "%s %"PRIll" band %d\n", __func__, (int64_t)freq,
              band);

    return ret;
}

/*
 * set DDS frequency.
 * NB: due to spur reduction, effective frequency might not be the expected one
 */
int sdr1k_set_freq(RIG *rig, vfo_t vfo, freq_t freq)
{
    struct sdr1k_priv_data *priv = (struct sdr1k_priv_data *)rig->state.priv;
    int i;
    double ftw;
    double DDS_step_size;
    freq_t frqval;
// why is spur_red always true?
//    int spur_red = 1;
#define spur_red 1
    int ret;

    ret = set_band(rig, freq);

    if (ret != RIG_OK)
    {
        return ret;
    }

    /* Calculate DDS step for spu reduction
     * DDS steps = 3051.7578125Hz
     */
    DDS_step_size = ((double)priv->xtal * priv->pll_mult) / 65536;
    rig_debug(RIG_DEBUG_VERBOSE, "%s DDS step size %g %g %g\n", __func__,
              DDS_step_size, (double)freq / DDS_step_size,
              rint((double)freq / DDS_step_size));

    // why is spur_red always true?
    if (spur_red)
    {
        frqval = (freq_t)(DDS_step_size * rint((double)freq / DDS_step_size));
    }
    else
    {
        frqval = freq;
    }

    rig_debug(RIG_DEBUG_VERBOSE, "%s curr %"PRIll" frqval %"PRIll"\n", __func__,
              (int64_t)freq, (int64_t)frqval);

    if (priv->dds_freq == frqval)
    {
        return RIG_OK;
    }

    /*** */
    ftw = (double)frqval / priv->xtal ;

    for (i = 0; i < 6; i++)
    {
        unsigned word;

        if (spur_red && i == 2)
        {
            word = 128;
        }
        else if (spur_red && i > 2)
        {
            word = 0;
        }
        else
        {
            word = (unsigned)(ftw * 256);
            ftw = ftw * 256 - word;
        }

        rig_debug(RIG_DEBUG_TRACE, "DDS %d [%02x]\n", i, word);

        ret = dds_write_reg(rig, 4 + i, word);

        if (ret != RIG_OK)
        {
            return ret;
        }
    }

    priv->dds_freq = frqval;

    return ret;
}

int sdr1k_get_freq(RIG *rig, vfo_t vfo, freq_t *freq)
{
    struct sdr1k_priv_data *priv = (struct sdr1k_priv_data *)rig->state.priv;

    *freq = priv->dds_freq;
    rig_debug(RIG_DEBUG_TRACE, "%s: %"PRIll"\n", __func__, (int64_t)priv->dds_freq);

    return RIG_OK;
}

/* Set DAC multiplier value */
static int DAC_mult(RIG *rig, unsigned mult)
{
    rig_debug(RIG_DEBUG_TRACE, "DAC [%02x,%02x]\n", mult >> 8, mult & 0xff);

    /* Output Shape Key I Mult */
    dds_write_reg(rig, 0x21, mult >> 8);
    dds_write_reg(rig, 0x22, mult & 0xff);

    /* Output Shape Key Q Mult */
    dds_write_reg(rig, 0x23, mult >> 8);
    dds_write_reg(rig, 0x24, mult & 0xff);

    return RIG_OK;
}

int sdr1k_reset(RIG *rig, reset_t reset)
{
    /* Reset all Latches (relays off) */
    write_latch(rig, L_BAND, 0x00, 0xff);
    write_latch(rig, L_DDS1, 0x00, 0xff);
    write_latch(rig, L_DDS0, 0x00, 0xff);
    write_latch(rig, L_EXT,  0x00, 0xff);

    /* Reset DDS */
    write_latch(rig, L_DDS1, RESET | WRB, 0xff);  /* reset the DDS chip */
    write_latch(rig, L_DDS1, WRB, 0xff);      /* leave WRB high */

    dds_write_reg(rig, 0x1d, COMP_PD);        /* Power down comparator */
    /* TODO: add PLL multiplier property and logic */
    dds_write_reg(rig, 0x1e, BYPASS_PLL);     /* Bypass PLL */

    dds_write_reg(rig, 0x20,
                  BYPASS_SINC | OSK_EN); /* Bypass Inverse Sinc and enable DAC */
    DAC_mult(rig, DEFAULT_DAC_MULT);  /* Set DAC multiplier value */

    return RIG_OK;
}

int sdr1k_set_ptt(RIG *rig, vfo_t vfo, ptt_t ptt)
{
    return set_bit(rig, L_BAND, 6, ptt == RIG_PTT_ON);
}


int sdr1k_set_level(RIG *rig, vfo_t vfo, setting_t level, value_t val)
{
    rig_debug(RIG_DEBUG_TRACE, "%s: %s %d\n", __func__, rig_strlevel(level), val.i);

    switch (level)
    {
    case RIG_LEVEL_PREAMP:
        return set_bit(rig, L_EXT, 7, !(val.i == rig->caps->preamp[0]));
        break;

    default:
        return -RIG_EINVAL;
    }
}


int
write_latch(RIG *rig, latch_t which, unsigned value, unsigned mask)
{
    struct sdr1k_priv_data *priv = (struct sdr1k_priv_data *)rig->state.priv;
    hamlib_port_t *pport = &rig->state.rigport;

    if (!(L_EXT <= which && which <= L_DDS1))
    {
        return -RIG_EINVAL;
    }

    par_lock(pport);
    priv->shadow[which] = (priv->shadow[which] & ~mask) | (value & mask);
    par_write_data(pport, priv->shadow[which]);
    pdelay(rig);
    par_write_control(pport, 0x0F ^ (1 << which));
    pdelay(rig);
    par_write_control(pport, 0x0F);
    pdelay(rig);
    par_unlock(pport);

    return RIG_OK;
}


int
dds_write_reg(RIG *rig, unsigned addr, unsigned data)
{
#if 0
    write_latch(rig, L_DDS1, addr & 0x3f, 0x3f);
    write_latch(rig, L_DDS0, data, 0xff);
    write_latch(rig, L_DDS1, 0x40, 0x40);
    write_latch(rig, L_DDS1, 0x00, 0x40);
#else
    /* set up data bits */
    write_latch(rig, L_DDS0, data, 0xff);

    /* set up address bits with WRB high */
    //write_latch (rig, L_DDS1, addr & 0x3f, 0x3f);
    write_latch(rig, L_DDS1, WRB | addr, 0xff);

    /* send write command with WRB low */
    write_latch(rig, L_DDS1, addr, 0xff);

    /* return WRB high */
    write_latch(rig, L_DDS1, WRB, 0xff);
#endif

    return RIG_OK;
}

int
set_bit(RIG *rig, latch_t reg, unsigned bit, unsigned state)
{
    unsigned val;

    val = state ? 1 << bit : 0;

    return write_latch(rig, reg, val, 1 << bit);
}

