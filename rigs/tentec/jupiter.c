/*
 *  Hamlib TenTenc backend - TT-538 description
 *  Copyright (c) 2003-2012 by Stephane Fillod, Martin Ewing
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

/* Extended and corrected by Martin Ewing AA6E 2/2012
 * This backend tested with firmware v 1.330.
 * Firmware version >=1.18 is probably required.
 * Reference: Jupiter Model 538 Programmer's Reference Guide Rev. 1.1
 * v 0.7 - 2012-07-15 - correct RAWSTR processing, add cal table for RIG_LEVEL_STRENGTH
 *    2012-08-02 - Add support for "IF" (passband tuning), NB, NR, ANF
 *    2012-12-04 - Revise reported bandwidth code
 */

/* to do:
 * implement dual VFO & split capability
 */

#include <hamlib/config.h>

#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#include <hamlib/rig.h>
#include "tentec2.h"
#include "tentec.h"
#include "bandplan.h"

struct tt538_priv_data
{
    int ch;     /* mem */
    vfo_t vfo_curr;
};


#define TT538_MODES (RIG_MODE_FM|RIG_MODE_CW|RIG_MODE_SSB|RIG_MODE_AM)
#define TT538_RXMODES (TT538_MODES)

#define TT538_FUNCS (RIG_FUNC_NR|RIG_FUNC_ANF|RIG_FUNC_NB)

#define TT538_LEVELS (RIG_LEVEL_RAWSTR| \
                RIG_LEVEL_SQL| \
                RIG_LEVEL_RF|RIG_LEVEL_IF| \
                RIG_LEVEL_AF|RIG_LEVEL_AGC| \
                RIG_LEVEL_SWR|RIG_LEVEL_ATT)

#define TT538_LEVELS_SET (RIG_LEVEL_SQL|RIG_LEVEL_RF| \
                RIG_LEVEL_AF|RIG_LEVEL_IF| \
                RIG_LEVEL_AGC|RIG_LEVEL_ATT)

#define TT538_ANTS (RIG_ANT_1)

#define TT538_PARMS (RIG_PARM_NONE)

#define TT538_VFO (RIG_VFO_A|RIG_VFO_B)

#define TT538_VFO_OPS (RIG_OP_TO_VFO|RIG_OP_FROM_VFO)

#define TT538_AM  '0'
#define TT538_USB '1'
#define TT538_LSB '2'
#define TT538_CW  '3'
#define TT538_FM  '4'
#define EOM "\015"      /* CR */

/* Jupiter's RAWSTR is S-meter reading in S value + fractional S value, times 256 */
#define TT538_STR_CAL  { 18, {  \
    { 256, -48 }, \
    { 512, -42 }, \
    { 768, -36 }, \
    { 1024, -30 }, \
    { 1280, -24 }, \
    { 1536, -18 }, \
    { 1792, -12 }, \
    { 2048, -6 }, \
    { 2304, 0 }, \
    { 2560, 6 }, \
    { 2816, 12 }, \
    { 3072, 18 }, \
    { 3328, 24 }, \
    { 3584, 30 }, \
    { 3840, 36 }, \
    { 4096, 42 }, \
    { 4352, 48 }, \
    { 4608, 54 }, \
    } }

static int tt538_init(RIG *rig);
static int tt538_reset(RIG *rig, reset_t reset);
static int tt538_get_freq(RIG *rig, vfo_t vfo, freq_t *freq);
static int tt538_set_freq(RIG *rig, vfo_t vfo, freq_t freq);
static int tt538_set_vfo(RIG *rig, vfo_t vfo);
static int tt538_get_vfo(RIG *rig, vfo_t *vfo);
static int tt538_set_split_vfo(RIG *rig, vfo_t vfo, split_t, vfo_t tx_vfo);
static int tt538_get_split_vfo(RIG *rig, vfo_t vfo, split_t *, vfo_t *tx_vfo);
static int tt538_get_mode(RIG *rig, vfo_t vfo, rmode_t *mode, pbwidth_t *width);
static int tt538_set_mode(RIG *rig, vfo_t vfo, rmode_t mode, pbwidth_t width);
static char which_vfo(const RIG *rig, vfo_t vfo);
static int tt538_set_ptt(RIG *rig, vfo_t vfo, ptt_t ptt);
static int tt538_get_level(RIG *rig, vfo_t vfo, setting_t level, value_t *val);
static int tt538_set_level(RIG *rig, vfo_t vfo, setting_t level, value_t val);
static int tt538_get_func(RIG *rig, vfo_t vfo, setting_t func, int *status);
static int tt538_set_func(RIG *rig, vfo_t vfo, setting_t func, int status);

/*
 * tt538 transceiver capabilities.
 *
 * Protocol is documented at
 *      http://www.rfsquared.com/
 */
const struct rig_caps tt538_caps =
{
    RIG_MODEL(RIG_MODEL_TT538),
    .model_name = "TT-538 Jupiter",
    .mfg_name =  "Ten-Tec",
    .version =  "20221205.0",
    .copyright =  "LGPL",
    .status =  RIG_STATUS_STABLE,
    .rig_type =  RIG_TYPE_TRANSCEIVER,
    .ptt_type =  RIG_PTT_RIG,
    .dcd_type =  RIG_DCD_NONE,
    .port_type =  RIG_PORT_SERIAL,
    .serial_rate_min =  57600,
    .serial_rate_max =  57600,
    .serial_data_bits =  8,
    .serial_stop_bits =  1,
    .serial_parity =  RIG_PARITY_NONE,
    .serial_handshake =  RIG_HANDSHAKE_HARDWARE,
    .write_delay =  0,
    .post_write_delay =  0,
    .timeout =  400,
    .retry =  3,

    .has_get_func =  TT538_FUNCS,
    .has_set_func =  TT538_FUNCS,
    .has_get_level =  TT538_LEVELS,
    .has_set_level =  TT538_LEVELS_SET,
    .has_get_parm =  TT538_PARMS,
    .has_set_parm =  TT538_PARMS,
    .level_gran =  {},                 /* FIXME: granularity */
    .parm_gran =  {},
    .ctcss_list =  NULL,
    .dcs_list =  NULL,
    .preamp =   { RIG_DBLST_END },
    .attenuator =   { 15, RIG_DBLST_END },
    .max_rit =  Hz(0),
    .max_xit =  Hz(0),
    .max_ifshift =  kHz(2),
    .targetable_vfo =  RIG_TARGETABLE_FREQ | RIG_TARGETABLE_MODE,
    .transceive =  RIG_TRN_OFF,
    .bank_qty =   0,
    .chan_desc_sz =  0,

    .chan_list =  {
        {   0, 127, RIG_MTYPE_MEM, TT_MEM_CAP },
    },

    .rx_range_list1 =  {
        {kHz(100), MHz(30), TT538_RXMODES, -1, -1, TT538_VFO, TT538_ANTS},
        RIG_FRNG_END,
    },
    .tx_range_list1 =  {
        FRQ_RNG_HF(1, TT538_MODES, W(5), W(100), TT538_VFO, TT538_ANTS),
        RIG_FRNG_END,
    },

    .rx_range_list2 =  {
        {kHz(100), MHz(30), TT538_RXMODES, -1, -1, TT538_VFO, TT538_ANTS},
        RIG_FRNG_END,
    },
    .tx_range_list2 =  {
        FRQ_RNG_HF(2, TT538_MODES, W(5), W(100), TT538_VFO, TT538_ANTS),
        {MHz(5.25), MHz(5.40), TT538_MODES, W(5), W(100), TT538_VFO, TT538_ANTS},
        RIG_FRNG_END,
    },

    .tuning_steps =  {
        {TT538_RXMODES, 1},
        {TT538_RXMODES, 10},
        {TT538_RXMODES, 100},
        {TT538_RXMODES, kHz(1)},
        {TT538_RXMODES, kHz(10)},
        {TT538_RXMODES, kHz(100)},
        RIG_TS_END,
    },
    /* mode/filter list, remember: order matters! */
    .filters =  {
        {RIG_MODE_CW | RIG_MODE_SSB | RIG_MODE_AM, kHz(2.4)},
        {RIG_MODE_CW | RIG_MODE_SSB | RIG_MODE_AM, 300},
        {RIG_MODE_CW | RIG_MODE_SSB | RIG_MODE_AM, kHz(8)},
        {RIG_MODE_CW | RIG_MODE_SSB | RIG_MODE_AM, 0}, /* 34 filters */
        {RIG_MODE_FM, kHz(15)}, /* TBC */
        RIG_FLT_END,
    },
    .priv = (void *) NULL,

    .rig_init =  tt538_init,
    .set_freq =  tt538_set_freq,
    .get_freq =  tt538_get_freq,
    .set_vfo =  tt538_set_vfo,
    .get_vfo =  tt538_get_vfo,
    .set_mode =  tt538_set_mode,
    .get_mode =  tt538_get_mode,
    .get_level =  tt538_get_level,
    .set_level = tt538_set_level,
    .get_func = tt538_get_func,
    .set_func = tt538_set_func,
    .set_split_vfo =  tt538_set_split_vfo,
    .get_split_vfo =  tt538_get_split_vfo,
    .set_ptt =  tt538_set_ptt,
    .reset =  tt538_reset,
    .get_info =  tentec2_get_info,
    .str_cal = TT538_STR_CAL,   // This signals front-end support of level STRENGTH
    .hamlib_check_rig_caps = HAMLIB_CHECK_RIG_CAPS
};

/* Filter table for 538 reciver support. */
static int tt538_rxFilter[] =
{
    8000, 6000, 5700, 5400, 5100, 4800, 4500, 4200, 3900, 3600, 3300,
    3000, 2850, 2700, 2550, 2400, 2250, 2100, 1950, 1800, 1650, 1500,
    1350, 1200, 1050,  900,  750,  675,  600,  525,  450,  375,  330,
    300,  260,  225,  180,  165,  150
};

#define JUPITER_TT538_RXFILTERS         ( sizeof(tt538_rxFilter) / sizeof(tt538_rxFilter[0]) )


/*
 * Function definitions below
 */

/* I frequently see the Jupiter and my laptop get out of sync.  A
   response from the 538 isn't seen by the laptop.  A few "XX"s
   sometimes get things going again, hence this hack, er, function. */

static int tt538_transaction(RIG *rig, const char *cmd, int cmd_len,
                             char *data, int *data_len)
{
    char    reset_buf[32];
    int i, reset_len, retval;

    retval = tentec_transaction(rig, cmd, cmd_len, data, data_len);

    if (retval == RIG_OK)
    {
        return retval;
    }

    /* Try a few times to do a DSP reset to resync things. */
    for (i = 0; i < 3; i++)
    {
        reset_len = 32;
        retval = tentec_transaction(rig, "XX" EOM, 3, reset_buf, &reset_len);

        if (retval != RIG_OK)
        {
            continue;    /* Try again.  This 1 didn't work. */
        }

        if (strstr(reset_buf, "RADIO START"))
        {
            break;    /* DSP reset successful! */
        }
    }

    /* Try real command one last time... */
    return tentec_transaction(rig, cmd, cmd_len, data, data_len);
}

/*
 * tt538_init:
 * Basically, it just sets up *priv
 */
int tt538_init(RIG *rig)
{
    struct tt538_priv_data *priv;

    rig->state.priv = (struct tt538_priv_data *) calloc(1, sizeof(
                          struct tt538_priv_data));

    if (!rig->state.priv)
    {
        /* whoops! memory shortage! */
        return -RIG_ENOMEM;
    }

    priv = rig->state.priv;

    memset(priv, 0, sizeof(struct tt538_priv_data));

    /*
     * set arbitrary initial status
     */
    priv->ch = 0;
    priv->vfo_curr = RIG_VFO_A;

    return RIG_OK;
}

static char which_vfo(const RIG *rig, vfo_t vfo)
{
    struct tt538_priv_data *priv = (struct tt538_priv_data *)rig->state.priv;

    if (vfo == RIG_VFO_CURR)
    {
        vfo = priv->vfo_curr;
    }

    switch (vfo)
    {
    case RIG_VFO_A: return 'A';

    case RIG_VFO_B: return 'B';

    case RIG_VFO_NONE: return 'N';

    default:
        rig_debug(RIG_DEBUG_ERR, "%s: unsupported VFO %s\n",
                  __func__, rig_strvfo(vfo));
        return -RIG_EINVAL;
    }
}

int tt538_get_vfo(RIG *rig, vfo_t *vfo)
{

    struct tt538_priv_data *priv = (struct tt538_priv_data *) rig->state.priv;
    *vfo = priv->vfo_curr;
    return RIG_OK;
}

/*
 * tt538_set_vfo
 * Assumes rig!=NULL
 */
int tt538_set_vfo(RIG *rig, vfo_t vfo)
{
    struct tt538_priv_data *priv = (struct tt538_priv_data *)rig->state.priv;

    if (vfo == RIG_VFO_CURR)
    {
        return RIG_OK;
    }

    priv->vfo_curr = vfo;
    return RIG_OK;
}

/*
 * Software restart
 */
int tt538_reset(RIG *rig, reset_t reset)
{
    int retval, reset_len;
    char reset_buf[32];

    reset_len = 32;
    retval = tt538_transaction(rig, "XX" EOM, 3, reset_buf, &reset_len);

    if (retval != RIG_OK)
    {
        return retval;
    }

    if (!strstr(reset_buf, "RADIO START"))
    {
        rig_debug(RIG_DEBUG_ERR, "%s: unexpected answer '%s'\n",
                  __func__, reset_buf);
        return -RIG_EPROTO;
    }

    return RIG_OK;
}

/*
 * tt538_get_freq
 * Assumes rig!=NULL, freq!=NULL
 */
int tt538_get_freq(RIG *rig, vfo_t vfo, freq_t *freq)
{

    char    curVfo;
    int resp_len, retval;
    unsigned char cmdbuf[16], respbuf[32];

    SNPRINTF((char *) cmdbuf, sizeof(cmdbuf), "?%c" EOM, which_vfo(rig, vfo));
    resp_len = 7;
    retval = tt538_transaction(rig, (char *) cmdbuf, strlen((char *)cmdbuf),
                               (char *) respbuf,
                               &resp_len);

    if (retval != RIG_OK)
    {
        return retval;
    }

    curVfo = which_vfo(rig, vfo);

    if (respbuf[0] != curVfo)
    {
        rig_debug(RIG_DEBUG_ERR, "%s: unexpected answer '%s'\n",
                  __func__, respbuf);
        return -RIG_EPROTO;
    }

    if (resp_len != 6)
    {
        rig_debug(RIG_DEBUG_ERR, "%s: unexpected length '%d'\n",
                  __func__, resp_len);
        return -RIG_EPROTO;
    }

    *freq = (respbuf[1] << 24)
            + (respbuf[2] << 16)
            + (respbuf[3] << 8)
            + respbuf[4];

    return RIG_OK;
}

/*
 * tt538_set_freq
 * assumes rig!=NULL, rig->state.priv!=NULL
 * assumes priv->mode in AM,CW,LSB or USB.
 */

int tt538_set_freq(RIG *rig, vfo_t vfo, freq_t freq)
{
    unsigned char bytes[4];
    unsigned char cmdbuf[16];

    /* Freq is 4 bytes long, MSB sent first. */
    bytes[3] = ((unsigned int) freq >> 24) & 0xff;
    bytes[2] = ((unsigned int) freq >> 16) & 0xff;
    bytes[1] = ((unsigned int) freq >>  8) & 0xff;
    bytes[0] = ((unsigned int) freq      ) & 0xff;

    SNPRINTF((char *) cmdbuf, sizeof(cmdbuf), "*%c%c%c%c%c" EOM,
             which_vfo(rig, vfo),
             bytes[3], bytes[2], bytes[1], bytes[0]);

    return tt538_transaction(rig, (char *) cmdbuf, 6,  NULL,
                             NULL);
}

/*
 * tt538_set_split_vfo
 * assumes rig!=NULL
 */

int tt538_set_split_vfo(RIG *rig, vfo_t vfo, split_t split, vfo_t tx_vfo)
{
    return tentec_transaction(rig, RIG_SPLIT_ON == split ? "*O1\r" : "*O0\r", 4,
                              NULL, NULL);
}

/*
 * tt538_get_split_vfo
 * assumes rig!=NULL
 */

int tt538_get_split_vfo(RIG *rig, vfo_t vfo, split_t *split, vfo_t *tx_vfo)
{
    int retval, ret_len;
    char buf[4] = "?O\r";

    ret_len = 4;
    retval = tentec_transaction(rig, buf, 3, buf, &ret_len);

    if (retval != RIG_OK)
    {
        return retval;
    }

    if (ret_len != 3)
    {
        return -RIG_EPROTO;
    }

    *split = buf[1] == '0' ? RIG_SPLIT_OFF : RIG_SPLIT_ON;
    *tx_vfo = RIG_VFO_A;

    return RIG_OK;
}

/*
 * tt538_get_mode
 * Assumes rig!=NULL, mode!=NULL
 */
int tt538_get_mode(RIG *rig, vfo_t vfo, rmode_t *mode, pbwidth_t *width)
{
    int resp_len, retval;
    int rpb;
    unsigned char cmdbuf[16], respbuf[32];
    char ttmode;
    /* Find bandwidth according to response from table. */
    const static int pbwidth[39] =
    {
        8000, 6000, 5700, 5400, 5100, 4800, 4500, 4200,
        3900, 3600, 3300, 3000, 2850, 2700, 2550, 2400,
        2250, 2100, 1950, 1800, 1650, 1500, 1350, 1200,
        1050,  900,  750,  675,  600,  525,  450,  375,
        330,  300,  260,  225,  180,  165,  150
    };

    /* Query mode */
    SNPRINTF((char *) cmdbuf, sizeof(cmdbuf), "?M" EOM);
    resp_len = 5;
    retval = tt538_transaction(rig, (char *) cmdbuf, strlen((char *)cmdbuf),
                               (char *) respbuf,
                               &resp_len);

    if (retval != RIG_OK)
    {
        return retval;
    }

    if (respbuf[0] != 'M' || resp_len != 4)
    {
        rig_debug(RIG_DEBUG_ERR, "%s: unexpected answer '%s'\n",
                  __func__, respbuf);
        return -RIG_EPROTO;
    }

    switch (which_vfo(rig, vfo))
    {
    case 'A':
        ttmode = respbuf[1];
        break;

    case 'B':
        ttmode = respbuf[2];
        break;

    default:
        rig_debug(RIG_DEBUG_ERR, "%s: unsupported VFO %s\n",
                  __func__, rig_strvfo(vfo));
        return -RIG_EINVAL;
        break;
    }

    switch (ttmode)
    {
    case TT538_AM:  *mode = RIG_MODE_AM;  break;

    case TT538_USB: *mode = RIG_MODE_USB; break;

    case TT538_LSB: *mode = RIG_MODE_LSB; break;

    case TT538_CW: *mode = RIG_MODE_CW;  break;

    case TT538_FM: *mode = RIG_MODE_FM;  break;

    default:
        rig_debug(RIG_DEBUG_ERR, "%s: unsupported mode '%c'\n",
                  __func__, ttmode);
        return -RIG_EPROTO;
    }

    /* Query passband width (filter) */
    SNPRINTF((char *) cmdbuf, sizeof(cmdbuf), "?W" EOM);
    resp_len = 4;
    retval = tt538_transaction(rig, (char *) cmdbuf, strlen((char *)cmdbuf),
                               (char *) respbuf,
                               &resp_len);

    if (retval != RIG_OK)
    {
        return retval;
    }

    if (respbuf[0] != 'W' && resp_len != 3)
    {
        rig_debug(RIG_DEBUG_ERR, "%s: unexpected answer '%s'\n",
                  __func__, respbuf);
        return -RIG_EPROTO;
    }

    rpb = respbuf[1];

    if (rpb <= 38)
    {
        *width = pbwidth[rpb];
    }
    else
    {
        rig_debug(RIG_DEBUG_ERR, "%s: unexpected bandwidth '%c'\n",
                  __func__, respbuf[1]);
        return -RIG_EPROTO;
    }

    return RIG_OK;
}

/* Find rx filter index of bandwidth the same or larger as requested. */
static int tt538_filter_number(int width)
{
    int i;

    for (i = JUPITER_TT538_RXFILTERS - 1; i >= 0; i--)
    {
        if (width <= tt538_rxFilter[i])
        {
            return i;
        }
    }

    return 0; /* Widest filter, 8 kHz. */
}


/*
 * tt538_set_mode
 * Assumes rig!=NULL
 */
int tt538_set_mode(RIG *rig, vfo_t vfo, rmode_t mode, pbwidth_t width)
{
    unsigned char cmdbuf[32], respbuf[32], ttmode;
    int resp_len, retval;

    struct tt538_priv_data *priv = (struct tt538_priv_data *) rig->state.priv;

    /* Query mode for both VFOs. */
    SNPRINTF((char *) cmdbuf, sizeof(cmdbuf), "?M" EOM);
    resp_len = 5;
    retval = tt538_transaction(rig, (char *) cmdbuf, strlen((char *)cmdbuf),
                               (char *) respbuf,
                               &resp_len);

    if (retval != RIG_OK)
    {
        return retval;
    }

    if (respbuf[0] != 'M' || resp_len != 4)
    {
        rig_debug(RIG_DEBUG_ERR, "%s: unexpected answer '%s'\n",
                  __func__, respbuf);
        return -RIG_EPROTO;
    }

    switch (mode)
    {
    case RIG_MODE_USB:  ttmode = TT538_USB; break;

    case RIG_MODE_LSB:  ttmode = TT538_LSB; break;

    case RIG_MODE_CW:   ttmode = TT538_CW; break;

    case RIG_MODE_AM:   ttmode = TT538_AM; break;

    case RIG_MODE_FM:   ttmode = TT538_FM; break;

    default:
        rig_debug(RIG_DEBUG_ERR, "%s: unsupported mode %s\n",
                  __func__, rig_strrmode(mode));
        return -RIG_EINVAL;
    }

    /* Set mode for both VFOs. */
    if (vfo == RIG_VFO_CURR)
    {
        vfo = priv->vfo_curr;
    }

    switch (vfo)
    {
    case RIG_VFO_A:
        SNPRINTF((char *) cmdbuf, sizeof(cmdbuf), "*M%c%c" EOM, ttmode, respbuf[2]);
        break;

    case RIG_VFO_B:
        SNPRINTF((char *) cmdbuf, sizeof(cmdbuf), "*M%c%c" EOM, respbuf[1], ttmode);
        break;

    default:
        rig_debug(RIG_DEBUG_ERR, "%s: unsupported VFO %s\n",
                  __func__, rig_strvfo(vfo));
        return -RIG_EINVAL;
    }

    retval = tt538_transaction(rig, (char *) cmdbuf, strlen((char *)cmdbuf), NULL,
                               NULL);

    if (retval != RIG_OK)
    {
        return retval;
    }

    if (RIG_PASSBAND_NOCHANGE == width) { return retval; }

    if (RIG_PASSBAND_NORMAL == width)
    {
        width = rig_passband_normal(rig, mode);
    }

    /* Set rx filter bandwidth. */
    width = tt538_filter_number((int) width);

    SNPRINTF((char *) cmdbuf, sizeof(cmdbuf), "*W%c" EOM, (unsigned char) width);
    return tt538_transaction(rig, (char *) cmdbuf, 4, NULL,
                             NULL);

    return RIG_OK;
}


/*
 * tt538_set_ptt
 * Assumes rig!=NULL
 */
int tt538_set_ptt(RIG *rig, vfo_t vfo, ptt_t ptt)
{
    return tentec_transaction(rig,
                              ptt == RIG_PTT_ON ? "Q1\r" : "Q0\r", 3,
                              NULL, NULL);
}


/*
 * tt538_get_level
 * Assumes rig!=NULL, val!=NULL
 */
int tt538_get_level(RIG *rig, vfo_t vfo, setting_t level, value_t *val)
{
    float   fwd, refl;
    float   ratio, swr;
    int     retval, lvl_len;
    unsigned char cmdbuf[16], lvlbuf[32];


    /* Optimize:
     *   sort the switch cases with the most frequent first
     */
    switch (level)
    {
    case RIG_LEVEL_SWR:
        /* Get forward power. */
        lvl_len = 4;
        retval = tt538_transaction(rig, "?F" EOM, 3, (char *) lvlbuf, &lvl_len);

        if (retval != RIG_OK)
        {
            return retval;
        }

        if (lvlbuf[0] != 'F' || lvl_len != 3)
        {
            rig_debug(RIG_DEBUG_ERR, "%s: unexpected answer '%s'\n",
                      __func__, lvlbuf);
            return -RIG_EPROTO;
        }

        fwd = (float) lvlbuf[1];

        /* Get reflected power. */
        lvl_len = 4;
        retval = tt538_transaction(rig, "?R" EOM, 3, (char *) lvlbuf, &lvl_len);

        if (retval != RIG_OK)
        {
            return retval;
        }

        if (lvlbuf[0] != 'R' || lvl_len != 3)
        {
            rig_debug(RIG_DEBUG_ERR, "%s: unexpected answer '%s'\n",
                      __func__, lvlbuf);
            return -RIG_EPROTO;
        }

        refl = (float) lvlbuf[1];
        ratio = refl / fwd;

        if (ratio > 0.9)
        {
            swr = 10.0;    /* practical maximum SWR, avoid div by 0 */
        }
        else
        {
            swr = 1.0 / (1.0 - ratio);
        }

        val->f = swr;
        break;

    case RIG_LEVEL_RAWSTR:
        lvl_len = 7;
        retval = tt538_transaction(rig, "?S" EOM, 3, (char *) lvlbuf, &lvl_len);

        if (retval != RIG_OK)
        {
            return retval;
        }

        if (lvlbuf[0] != 'S' || lvl_len != 6)
        {
            rig_debug(RIG_DEBUG_ERR, "%s: unexpected answer '%s'\n",
                      __func__, lvlbuf);
            return -RIG_EPROTO;
        }

        /* Jupiter returns actual S value in 1/256s of an S unit, in
        ascii hex digits.  We convert those digits to binary and return
        that integer (S units * 256) */
        {
            char hex[5];
            int i;
            unsigned int ival;

            for (i = 0; i < 4; i++) { hex[i] = lvlbuf[i + 1]; }

            hex[4] = '\0';
            sscanf(hex, "%4x", &ival);
            val->i = ival;      /* S-units+fract * 256 */
        }
        break;

    case RIG_LEVEL_AGC:

        /* Read rig's AGC level setting. */
        SNPRINTF((char *) cmdbuf, sizeof(cmdbuf), "?G" EOM);
        lvl_len = 4;
        retval = tt538_transaction(rig, (char *) cmdbuf, strlen((char *)cmdbuf),
                                   (char *) lvlbuf,
                                   &lvl_len);

        if (retval != RIG_OK)
        {
            return retval;
        }

        if (lvlbuf[0] != 'G' || lvl_len != 3)
        {
            rig_debug(RIG_DEBUG_ERR, "%s: unexpected answer '%s'\n",
                      __func__, lvlbuf);
            return -RIG_EPROTO;
        }

        switch (lvlbuf[1] & 0xf)
        {
        /* Prog. Man. claims Jupiter returns '1', '2', and '3', but not so if
           AGC was set by program! So look at 2nd hex digit only. */
        case 1: val->i = RIG_AGC_SLOW; break;

        case 2: val->i = RIG_AGC_MEDIUM; break;

        case 3: val->i = RIG_AGC_FAST; break;

        default: return -RIG_EPROTO;
        }

        break;

    case RIG_LEVEL_AF:

        /* Volume returned as single byte. */
        SNPRINTF((char *) cmdbuf, sizeof(cmdbuf), "?U" EOM);
        lvl_len = 4;
        retval = tt538_transaction(rig, (char *) cmdbuf, strlen((char *)cmdbuf),
                                   (char *) lvlbuf,
                                   &lvl_len);

        if (retval != RIG_OK)
        {
            return retval;
        }

        if (lvlbuf[0] != 'U' || lvl_len != 3)
        {
            rig_debug(RIG_DEBUG_ERR, "%s: unexpected answer '%s'\n",
                      __func__, lvlbuf);
            return -RIG_EPROTO;
        }

        val->f = (float) lvlbuf[1] / 127;
        break;

    case RIG_LEVEL_RF:

        SNPRINTF((char *) cmdbuf, sizeof(cmdbuf), "?I" EOM);
        lvl_len = 4;
        retval = tt538_transaction(rig, (char *) cmdbuf, strlen((char *)cmdbuf),
                                   (char *) lvlbuf,
                                   &lvl_len);

        if (retval != RIG_OK)
        {
            return retval;
        }

        if (lvlbuf[0] != 'I' || lvl_len != 3)
        {
            rig_debug(RIG_DEBUG_ERR, "%s: unexpected answer '%s'\n",
                      __func__, lvlbuf);
            return -RIG_EPROTO;
        }

        /* Note: Any RF gain over "50%" on front panel
            returns 1.00 (firmware 1.281) on test rig.
            However RF set level seems OK. Firmware problem? -AA6E */
        val->f = 1 - (float) lvlbuf[1] / 0xff;
        break;

    case RIG_LEVEL_IF:  /* IF passband tuning, Hz */

        SNPRINTF((char *) cmdbuf, sizeof(cmdbuf), "?P" EOM);
        lvl_len = 5;
        retval = tt538_transaction(rig, (char *) cmdbuf, strlen((char *)cmdbuf),
                                   (char *) lvlbuf,
                                   & lvl_len);

        if (retval != RIG_OK)
        {
            return retval;
        }

        if (lvlbuf[0] != 'P' || lvl_len != 4)
        {
            rig_debug(RIG_DEBUG_ERR, "%s: unexpected answer '%s'\n",
                      __func__, lvlbuf);
            return -RIG_EPROTO;
        }

        val->i = (int) lvlbuf[1] * 256 + (int) lvlbuf[2];
        break;

    case RIG_LEVEL_ATT:

        SNPRINTF((char *) cmdbuf, sizeof(cmdbuf), "?J" EOM);
        lvl_len = 4;
        retval = tt538_transaction(rig, (char *) cmdbuf, strlen((char *)cmdbuf),
                                   (char *) lvlbuf,
                                   &lvl_len);

        if (retval != RIG_OK)
        {
            return retval;
        }

        if (lvlbuf[0] != 'J' || lvl_len != 3)
        {
            rig_debug(RIG_DEBUG_ERR, "%s: unexpected answer '%s'\n",
                      __func__, lvlbuf);
            return -RIG_EPROTO;
        }

        val->i = lvlbuf[1];
        break;

    case RIG_LEVEL_SQL:

        SNPRINTF((char *) cmdbuf, sizeof(cmdbuf), "?H" EOM);
        lvl_len = 4;
        retval = tt538_transaction(rig, (char *) cmdbuf, strlen((char *)cmdbuf),
                                   (char *) lvlbuf,
                                   &lvl_len);

        if (retval != RIG_OK)
        {
            return retval;
        }

        if (lvlbuf[0] != 'H' || lvl_len != 3)
        {
            rig_debug(RIG_DEBUG_ERR, "%s: unexpected answer '%s'\n",
                      __func__, lvlbuf);
            return -RIG_EPROTO;
        }

        val->f = ((float) lvlbuf[1] / 127);
        break;

    default:
        rig_debug(RIG_DEBUG_ERR, "%s: unsupported level %s\n",
                  __func__, rig_strlevel(level));
        return -RIG_EINVAL;
    }

    return RIG_OK;
}

/*
 * tt538_set_level
 * Assumes rig!=NULL, val!=NULL
 */
int tt538_set_level(RIG *rig, vfo_t vfo, setting_t level, value_t val)
{
    char    cc, cmdbuf[32], c1, c2;
    int retval;
    int len;

    switch (level)
    {

    case RIG_LEVEL_AGC:
        switch (val.i)
        {
        case RIG_AGC_FAST:   cc = '3'; break;

        case RIG_AGC_MEDIUM: cc = '2'; break;

        case RIG_AGC_SLOW:   cc = '1'; break;

        default: cc = '2';
        }

        SNPRINTF(cmdbuf, sizeof(cmdbuf), "*G%c" EOM, cc);
        len = 4;
        break;

    case RIG_LEVEL_AF:
        SNPRINTF(cmdbuf, sizeof(cmdbuf), "*U%c" EOM, (int)(127 * val.f));
        len = 4;
        break;

    case RIG_LEVEL_RF:
        SNPRINTF(cmdbuf, sizeof(cmdbuf), "*I%c" EOM, (int)(127 * val.f));
        len = 4;
        break;

    case RIG_LEVEL_IF:
        c1 = val.i >> 8;
        c2 = val.i & 0xff;
        SNPRINTF(cmdbuf, sizeof(cmdbuf), "*P%c%c" EOM, c1, c2);
        len = 5;
        break;

    case RIG_LEVEL_ATT:
        if (val.i)
        {
            cc = '1';
        }
        else
        {
            cc = '0';
        }

        SNPRINTF(cmdbuf, sizeof(cmdbuf), "*J%c" EOM, cc);
        len = 4;
        break;

    case RIG_LEVEL_SQL:
        SNPRINTF(cmdbuf, sizeof(cmdbuf), "*H%c" EOM, (int)(127 * val.f));
        len = 4;
        break;

    default:
        rig_debug(RIG_DEBUG_ERR, "%s: unsupported level %s\n",
                  __func__, rig_strlevel(level));
        return -RIG_EINVAL;
    }

    retval = tt538_transaction(rig, cmdbuf, len, NULL, NULL);

    if (retval != RIG_OK)
    {
        return retval;
    }

    return RIG_OK;
}

int tt538_get_func(RIG *rig, vfo_t vfo, setting_t func, int *status)
{
    char frespbuf[32];
    int retval, fresplen;

    switch (func)
    {

    case RIG_FUNC_NR:
        /* ?K gets nb(0-7), an, nr according to prog ref guide,
            but it's really nb, nr, an */
        fresplen = 6;
        retval = tt538_transaction(rig, "?K" EOM, 3, frespbuf, &fresplen);

        if (retval != RIG_OK)
        {
            return retval;
        }

        *status = frespbuf[ 2 ] == 1;
        return RIG_OK;

    case RIG_FUNC_ANF:
        fresplen = 6;
        retval = tt538_transaction(rig, "?K" EOM, 3, frespbuf, &fresplen);

        if (retval != RIG_OK)
        {
            return retval;
        }

        *status = frespbuf[ 3 ] == 1;
        return RIG_OK;

    case RIG_FUNC_NB:

        /* Based on research by AA6E -
         * Data transferred from rig:
         *
         * |__|__|__| (a 3 bit value, 0 - 7 indicating NB "strength"
         *  4  2  1
         *
         * Apparently the "ON" / "OFF" state of the NB is NOT available for reading.  This
         * state is visible in the Jupiter's menu.  Hamlib does not support a "level" for
         * NB.  We only recognize zero (off) or non-zero (on) for this function on read.
         */
        fresplen = 6;
        retval = tt538_transaction(rig, "?K" EOM, 3, frespbuf, &fresplen);

        if (retval != RIG_OK)
        {
            return retval;
        }

        *status = (frespbuf[ 1 ] != 0); /* non-zero value -> "on" */
        return RIG_OK;

    default:
        rig_debug(RIG_DEBUG_ERR, "%s: unsupported get_func %s", __func__,
                  rig_strfunc(func));
        return -RIG_EINVAL;
    }
}

int tt538_set_func(RIG *rig, vfo_t vfo, setting_t func, int status)
{
    char fcmdbuf[32], frespbuf[32];
    int retval, fresplen, i;

    switch (func)
    {

    case RIG_FUNC_NB:
        /* Jupiter combines, nb, nr, and anf in one command, so we need to
        retrieve them all before changing one of them */
        fresplen = 6;
        retval = tt538_transaction(rig, "?K" EOM, 3, frespbuf, &fresplen);

        if (retval != RIG_OK)
        {
            return retval;
        }

        for (i = 0; i < 5; i++)
        {
            fcmdbuf[i + 1] = frespbuf[i];
        }

        fcmdbuf[0] = '*';
        fcmdbuf[2] = status ? 5 : 1;

        /* Based on AA6E research (no thanks to errors in TT Prog Ref Manual!)
         * The "set" function (*K command) uses a different data format from the ?K get function.
         * Data transferred to rig:
         *   +--+--+----------NB value (0-7)
         *   v  v  v  +-------NB "on/off" bit
         * |__|__|__|__|
         *   8  4  2  1
         * The NB on/off bit corresponds to the "Noise Blanker" item in the Jupiter menu.
         * The value is show in the "NB selection" item in the Jupiter menu.
         * Note that if all zeroes are sent, the NB does shut off, but the NB value
         * is unchanged.  If you want to change the NB value, the on/off bit must be set.
         * Because the on/off status cannot (apparently) be read back by software, we will
         * leave NB always on, but set to zero value when NB "off" is desired.  It is not clear
         * if NB on/off makes a difference if the value is zero. (ver 1330-538 firmware)
         */
        /* send data back, with change */
        retval = tt538_transaction(rig, fcmdbuf, 6, NULL, NULL);

        if (retval != RIG_OK)
        {
            return retval;
        }

        return RIG_OK;
        break;

    case RIG_FUNC_NR:
        fresplen = 6;
        retval = tt538_transaction(rig, "?K" EOM, 3, frespbuf, &fresplen);

        if (retval != RIG_OK)
        {
            return retval;
        }

        for (i = 0; i < 5; i++)
        {
            fcmdbuf[i + 1] = frespbuf[i];
        }

        fcmdbuf[0] = '*';
        fcmdbuf[3] = status ? 1 : 0;
        /* send data back, with change */
        retval = tt538_transaction(rig, fcmdbuf, 6, NULL, NULL);

        if (retval != RIG_OK)
        {
            return retval;
        }

        return RIG_OK;
        break;

    case RIG_FUNC_ANF:
        fresplen = 6;
        retval = tt538_transaction(rig, "?K" EOM, 3, frespbuf, &fresplen);

        if (retval != RIG_OK)
        {
            return retval;
        }

        for (i = 0; i < 5; i++)
        {
            fcmdbuf[i + 1] = frespbuf[i];
        }

        fcmdbuf[0] = '*';
        fcmdbuf[4] = status ? 1 : 0;
        /* send data back, with change */
        retval = tt538_transaction(rig, fcmdbuf, 6, NULL, NULL);

        if (retval != RIG_OK)
        {
            return retval;
        }

        return RIG_OK;
        break;

    default:
        rig_debug(RIG_DEBUG_ERR, "%s: unsupported set_func %s", __func__,
                  rig_strfunc(func));
        return -RIG_EINVAL;
    }

}
