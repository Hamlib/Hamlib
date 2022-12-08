/*
 *  Hamlib Kenwood backend - TM-V7 description
 *  Copyright (c) 2004-2010 by Stephane Fillod
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
#include <string.h>  /* String function definitions */

#include <hamlib/rig.h>
#include "kenwood.h"
#include "th.h"
#include "misc.h"
#include "num_stdio.h"

#if 1
#define RIG_ASSERT(x)   if (!(x)) { rig_debug(RIG_DEBUG_ERR, "Assertion failed on line %i\n",__LINE__); abort(); }
#else
#define RIG_ASSERT(x)
#endif

#define TMV7_FUNC_ALL (\
                       RIG_FUNC_TBURST \
                       )

#define TMV7_LEVEL_ALL (\
            RIG_LEVEL_RAWSTR| \
                        RIG_LEVEL_SQL| \
                        RIG_LEVEL_AF| \
                        RIG_LEVEL_RFPOWER\
            )

#define TMV7_CHANNEL_CAPS \
.freq=1,\
.tx_freq=1,\
.mode=1,\
.tuning_step=1,\
.rptr_shift=1,\
.ctcss_tone=1,\
.ctcss_sql=1,\
.channel_desc=1

#ifndef RIG_TONEMAX
#define RIG_TONEMAX     38
#endif

#define RIG_VFO_A_OP (RIG_OP_UP|RIG_OP_DOWN|RIG_OP_TO_VFO)

#define ACKBUF_LEN 128

static rmode_t tmv7_mode_table[KENWOOD_MODE_TABLE_MAX] =
{
    [0] = RIG_MODE_FM,
    [1] = RIG_MODE_AM,
};

static struct kenwood_priv_caps  tmv7_priv_caps  =
{
    .cmdtrm =  EOM_TH,   /* Command termination character */
    .mode_table = tmv7_mode_table,
};


/* tmv7 procs */
static int tmv7_decode_event(RIG *rig);
static int tmv7_set_vfo(RIG *rig, vfo_t vfo);
static int tmv7_get_mode(RIG *rig, vfo_t vfo, rmode_t *mode, pbwidth_t *width);
static int tmv7_get_powerstat(RIG *rig, powerstat_t *status);
static int tmv7_get_channel(RIG *rig, vfo_t vfo, channel_t *chan,
                            int read_only);
static int tmv7_set_channel(RIG *rig, vfo_t vfo, const channel_t *chan);

/*
 * tm-v7 rig capabilities.
 */
const struct rig_caps tmv7_caps =
{
    RIG_MODEL(RIG_MODEL_TMV7),
    .model_name = "TM-V7",
    .mfg_name =  "Kenwood",
    .version =  TH_VER ".0",
    .copyright =  "LGPL",
    .status =  RIG_STATUS_STABLE,
    .rig_type =  RIG_TYPE_MOBILE,
    .ptt_type =  RIG_PTT_RIG,
    .dcd_type =  RIG_DCD_RIG,
    .port_type =  RIG_PORT_SERIAL,
    .serial_rate_min =  9600,
    .serial_rate_max =  9600,
    .serial_data_bits =  8,
    .serial_stop_bits =  1,
    .serial_parity =  RIG_PARITY_NONE,
    .serial_handshake =  RIG_HANDSHAKE_NONE,
    .write_delay =  1,
    .post_write_delay =  0,
    .timeout =  500,
    .retry =  3,

    .has_set_func =  TMV7_FUNC_ALL,
    .has_get_level =  TMV7_LEVEL_ALL,
    .has_set_level =  RIG_LEVEL_SET(TMV7_LEVEL_ALL),
    .level_gran =
    {
#include "level_gran_kenwood.h"
    },
    .parm_gran =  {},
    .ctcss_list =  kenwood38_ctcss_list,
    .dcs_list =  NULL,
    .preamp =   { RIG_DBLST_END, },
    .attenuator =   { RIG_DBLST_END, },
    .max_rit =  Hz(0),
    .max_xit =  Hz(0),
    .max_ifshift =  Hz(0),
    .vfo_ops =  RIG_VFO_A_OP,
    .targetable_vfo =  RIG_TARGETABLE_NONE,
    .transceive =  RIG_TRN_RIG,
    .bank_qty =   0,
    .chan_desc_sz =  6,


    .chan_list =  {
        {  1, 90, RIG_MTYPE_MEM, {TMV7_CHANNEL_CAPS}},    /* normal MEM VHF */
        { 101, 190, RIG_MTYPE_MEM, {TMV7_CHANNEL_CAPS}},   /* normal MEM UHF */
        {  201, 206, RIG_MTYPE_EDGE, {TMV7_CHANNEL_CAPS}}, /* L MEM */
        {  211, 216, RIG_MTYPE_EDGE, {TMV7_CHANNEL_CAPS}}, /* U MEM */
        {  221, 222, RIG_MTYPE_CALL, {TMV7_CHANNEL_CAPS}}, /* Call V/U */
        RIG_CHAN_END,
    },

    .rx_range_list1 =  {
        {MHz(118), MHz(174), RIG_MODE_AM | RIG_MODE_FM, -1, -1, RIG_VFO_A},
        {MHz(300), MHz(470), RIG_MODE_FM, -1, -1, RIG_VFO_B},
        RIG_FRNG_END,
    }, /* rx range */

    .tx_range_list1 =  {
        {MHz(118), MHz(174), RIG_MODE_FM, W(5), W(50), RIG_VFO_A},
        {MHz(300), MHz(470), RIG_MODE_FM, W(5), W(35), RIG_VFO_B},
        RIG_FRNG_END,
    }, /* tx range */

    .rx_range_list2 =  {
        {MHz(118), MHz(174), RIG_MODE_AM | RIG_MODE_FM, -1, -1, RIG_VFO_A},
        {MHz(300), MHz(470), RIG_MODE_FM, -1, -1, RIG_VFO_B},
        RIG_FRNG_END,
    }, /* rx range */

    .tx_range_list2 =  {
        {MHz(118), MHz(174), RIG_MODE_FM, W(5), W(50), RIG_VFO_A},
        {MHz(300), MHz(470), RIG_MODE_FM, W(5), W(35), RIG_VFO_B},
        RIG_FRNG_END,
    }, /* tx range */

    .tuning_steps =  {
        {RIG_MODE_FM, kHz(5)},
        {RIG_MODE_FM, kHz(6.25)},
        {RIG_MODE_FM, kHz(10)},
        {RIG_MODE_FM, kHz(12.5)},
        {RIG_MODE_FM, kHz(15)},
        {RIG_MODE_FM, kHz(25)},
        {RIG_MODE_FM, kHz(50)},
        RIG_TS_END,
    },
    /* mode/filter list, remember: order matters! */
    .filters =  {
        {RIG_MODE_ALL, RIG_FLT_ANY},
        RIG_FLT_END
    },

    .str_cal = { 4, { {0, -60 }, {1, -30,}, {5, 0}, {7, 20}}}, /* rought guess */

    .priv = (void *)& tmv7_priv_caps,
    .rig_init = kenwood_init,
    .rig_cleanup = kenwood_cleanup,
    .rig_open = kenwood_open,
    .rig_close = kenwood_close,

    .set_freq =  th_set_freq,
    .get_freq =  th_get_freq,
    .get_mode =  tmv7_get_mode,
    .set_vfo =  tmv7_set_vfo,
    .get_vfo =  th_get_vfo,
    .set_mem =  th_set_mem,
    .get_mem =  th_get_mem,
    .set_channel =  tmv7_set_channel,
    .get_channel =  tmv7_get_channel,
    .set_trn =  th_set_trn,
    .get_trn =  th_get_trn,

    .set_func =  th_set_func,
    .get_func =  th_get_func,
    .get_level = th_get_level,
    .set_level = th_set_level,
    .get_info =  th_get_info,
    .get_powerstat =  tmv7_get_powerstat,
    .vfo_op = th_vfo_op,
    .set_ptt = th_set_ptt,
    .get_dcd = th_get_dcd,
    .decode_event =  tmv7_decode_event,
    .hamlib_check_rig_caps = HAMLIB_CHECK_RIG_CAPS
};


/* --------------------------------------------------------------------- */
int tmv7_decode_event(RIG *rig)
{
    char asyncbuf[ACKBUF_LEN];
    int retval;
    rig_debug(RIG_DEBUG_TRACE, "%s: called\n", __func__);

    retval = kenwood_transaction(rig, NULL, asyncbuf, sizeof(asyncbuf));

    if (retval != RIG_OK)
    {
        return retval;
    }

    rig_debug(RIG_DEBUG_TRACE, "%s: Decoding message\n", __func__);

    if (asyncbuf[0] == 'B' && asyncbuf[1] == 'U' && asyncbuf[2] == 'F')
    {

        freq_t freq, offset;
        int step, shift, rev, tone, ctcss, tonefq, ctcssfq;

        retval = num_sscanf(asyncbuf,
                            "BUF 0,%"SCNfreq",%d,%d,%d,%d,%d,,%d,,%d,%"SCNfreq,
                            &freq, &step, &shift, &rev, &tone,
                            &ctcss, &tonefq, &ctcssfq, &offset);

        if (retval != 11)
        {
            rig_debug(RIG_DEBUG_ERR, "%s: Unexpected BUF message '%s'\n", __func__,
                      asyncbuf);
            return -RIG_ERJCTED;
        }

        rig_debug(RIG_DEBUG_TRACE, "%s: Buffer (freq %"PRIfreq" Hz)\n", __func__, freq);

        /* Callback execution */
        if (rig->callbacks.vfo_event)
        {
            rig->callbacks.vfo_event(rig, RIG_VFO_A, rig->callbacks.vfo_arg);
        }

        if (rig->callbacks.freq_event)
        {
            rig->callbacks.freq_event(rig, RIG_VFO_A, freq, rig->callbacks.freq_arg);
        }

        /*
            if (rig->callbacks.mode_event) {
                rig->callbacks.mode_event(rig, RIG_VFO_A, mode, RIG_PASSBAND_NORMAL,
                                rig->callbacks.mode_arg);
            }
        */

        /* --------------------------------------------------------------------- */
    }
    else if (asyncbuf[0] == 'S' && asyncbuf[1] == 'M')
    {

        int lev;
        retval = sscanf(asyncbuf, "SM 0,%d", &lev);

        if (retval != 2)
        {
            rig_debug(RIG_DEBUG_ERR, "%s: Unexpected SM message '%s'\n", __func__,
                      asyncbuf);
            return -RIG_ERJCTED;
        }

        rig_debug(RIG_DEBUG_TRACE, "%s: Signal strength event - signal = %.3f\n",
                  __func__, (float)(lev / 5.0));

        /* Callback execution */
#if STILLHAVETOADDCALLBACK

        if (rig->callbacks.strength_event)
            rig->callbacks.strength_event(rig, RIG_VFO_0, (float)(lev / 5.0),
                                          rig->callbacks.strength_arg);

#endif

        /* --------------------------------------------------------------------- */
    }
    else if (asyncbuf[0] == 'B' && asyncbuf[1] == 'Y')
    {

        int busy;

        retval = sscanf(asyncbuf, "BY 0,%d", &busy);

        if (retval != 2)
        {
            rig_debug(RIG_DEBUG_ERR, "%s: Unexpected BY message '%s'\n", __func__,
                      asyncbuf);
            return -RIG_ERJCTED;
        }

        rig_debug(RIG_DEBUG_TRACE, "%s: Busy event - status = '%s'\n",
                  __func__, (busy == 0) ? "OFF" : "ON");
        return -RIG_ENIMPL;
        /* This event does not have a callback. */

        /* --------------------------------------------------------------------- */
    }
    else if (asyncbuf[0] == 'V' && asyncbuf[1] == 'M' && asyncbuf[2] == 'C')
    {

        vfo_t bandmode;

        retval = sscanf(asyncbuf, "VMC 0,%u", &bandmode);

        if (retval != 1)
        {
            rig_debug(RIG_DEBUG_ERR, "%s: Unexpected VMC message '%s'\n", __func__,
                      asyncbuf);
            return -RIG_ERJCTED;
        }

        switch (bandmode)
        {
        case 0:     bandmode = RIG_VFO_VFO;  break;

        case 2:     bandmode = RIG_VFO_MEM;  break;

        /*  case 3:     bandmode = RIG_VFO_CALL; break; */
        default:    bandmode = RIG_VFO_CURR; break;
        }

        rig_debug(RIG_DEBUG_TRACE, "%s: Mode of Band event -  %u\n", __func__,
                  bandmode);

        /* TODO: This event does not have a callback! */
        return -RIG_ENIMPL;
        /* --------------------------------------------------------------------- */
    }
    else
    {

        rig_debug(RIG_DEBUG_ERR, "%s: Unsupported transceive cmd '%s'\n", __func__,
                  asyncbuf);
        return -RIG_ENIMPL;
    }

    return RIG_OK;
}


/* --------------------------------------------------------------------- */
int tmv7_set_vfo(RIG *rig, vfo_t vfo)
{
    char vfobuf[16], ackbuf[ACKBUF_LEN];
    int retval;

    rig_debug(RIG_DEBUG_TRACE, "%s: called %s\n", __func__, rig_strvfo(vfo));

    switch (vfo)
    {
    case RIG_VFO_A:
    case RIG_VFO_VFO:
        SNPRINTF(vfobuf, sizeof(vfobuf), "VMC 0,0");
        break;

    case RIG_VFO_B:
        SNPRINTF(vfobuf, sizeof(vfobuf), "VMC 1,0");
        break;

    case RIG_VFO_MEM:
        SNPRINTF(vfobuf, sizeof(vfobuf), "BC");
        retval = kenwood_transaction(rig, vfobuf, ackbuf, sizeof(ackbuf));

        if (retval != RIG_OK) { return retval; }

        SNPRINTF(vfobuf, sizeof(vfobuf), "VMC %c,2", ackbuf[3]);
        break;

    default:
        rig_debug(RIG_DEBUG_ERR, "%s: Unsupported VFO %s\n", __func__,
                  rig_strvfo(vfo));
        return -RIG_EVFO;
    }

    retval = kenwood_transaction(rig, vfobuf, NULL, 0);

    if (retval != RIG_OK)
    {
        rig_debug(RIG_DEBUG_ERR, "%s: bad return \n", __func__);
        return retval;
    }

    rig_debug(RIG_DEBUG_TRACE, "%s: next %s\n", __func__, rig_strvfo(vfo));

    switch (vfo)
    {
    case RIG_VFO_A:
    case RIG_VFO_VFO:
        SNPRINTF(vfobuf, sizeof(vfobuf), "BC 0,0");
        break;

    case RIG_VFO_B:
        SNPRINTF(vfobuf,  sizeof(vfobuf), "BC 1,1");
        break;

    case RIG_VFO_MEM:
        return RIG_OK;

    default:
        return RIG_OK;
    }

    rig_debug(RIG_DEBUG_TRACE, "%s: next2\n", __func__);
    retval = kenwood_transaction(rig, vfobuf, NULL, 0);

    if (retval != RIG_OK)
    {
        return retval;
    }

    return RIG_OK;
}

/* --------------------------------------------------------------------- */
int tmv7_get_mode(RIG *rig, vfo_t vfo, rmode_t *mode, pbwidth_t *width)
{
    char ackbuf[ACKBUF_LEN];
    int retval;
    int step;
    freq_t freq;

    rig_debug(RIG_DEBUG_TRACE, "%s: called\n", __func__);

    switch (vfo)
    {
    case RIG_VFO_CURR: break;

    case RIG_VFO_A: break;

    default:
        rig_debug(RIG_DEBUG_ERR, "%s: Unsupported VFO %s\n", __func__, rig_strvfo(vfo));
        return -RIG_EVFO;
    }

    /* try to guess from frequency */
    retval = kenwood_transaction(rig, "FQ", ackbuf, sizeof(ackbuf));

    if (retval != RIG_OK)
    {
        return retval;
    }

    num_sscanf(ackbuf, "FQ %"SCNfreq",%d", &freq, &step);

    if (freq < MHz(137))
    {
        *mode = RIG_MODE_AM;
        *width = kHz(9);
    }
    else
    {
        *mode = RIG_MODE_FM;
        *width = kHz(12);
    }

    return RIG_OK;
}

int tmv7_get_powerstat(RIG *rig, powerstat_t *status)
{
    /* dummy func to make sgcontrol happy */

    *status = RIG_POWER_ON;
    return RIG_OK;
}


/* --------------------------------------------------------------------- */
int tmv7_get_channel(RIG *rig, vfo_t vfo, channel_t *chan, int read_only)
{
    char membuf[64], ackbuf[ACKBUF_LEN];
    int retval;
    freq_t freq;
    char req[32], scf[128];
    int step, shift, rev, tone, ctcss, tonefq, ctcssfq;

    if (chan->channel_num < 100)
    {
        SNPRINTF(req, sizeof(req), "MR 0,0,%03d", chan->channel_num);
    }
    else if (chan->channel_num < 200)
    {
        SNPRINTF(req, sizeof(req), "MR 1,0,%03d", chan->channel_num - 100);
    }
    else if (chan->channel_num < 204)
    {
        SNPRINTF(req, sizeof(req), "MR 0,0,L%01d", chan->channel_num - 200);
        SNPRINTF(chan->channel_desc, sizeof(chan->channel_desc), "L%01d/V",
                 chan->channel_num - 200);
    }
    else if (chan->channel_num < 211)
    {
        SNPRINTF(req, sizeof(req), "MR 1,0,L%01d", chan->channel_num - 203);
        SNPRINTF(chan->channel_desc, sizeof(chan->channel_desc), "L%01d/U",
                 chan->channel_num - 203);
    }
    else if (chan->channel_num < 214)
    {
        SNPRINTF(req, sizeof(req), "MR 0,0,U%01d", chan->channel_num - 210);
        SNPRINTF(chan->channel_desc, sizeof(chan->channel_desc), "U%01d/V",
                 chan->channel_num - 210);
    }
    else if (chan->channel_num < 220)
    {
        SNPRINTF(req, sizeof(req), "MR 1,0,U%01d", chan->channel_num - 213);
        SNPRINTF(chan->channel_desc, sizeof(chan->channel_desc), "U%01d/U",
                 chan->channel_num - 213);
    }
    else if (chan->channel_num < 223)
    {
        if (chan->channel_num == 221)
        {
            SNPRINTF(req, sizeof(req), "CR 0,0");
            SNPRINTF(chan->channel_desc, sizeof(chan->channel_desc), "Call V");
        }

        if (chan->channel_num == 222)
        {
            SNPRINTF(req, sizeof(req), "CR 1,0");
            SNPRINTF(chan->channel_desc, sizeof(chan->channel_desc), "Call U");
        }
    }
    else
    {
        return -RIG_EINVAL;
    }

    SNPRINTF(membuf, sizeof(membuf), "%s", req);
    retval = kenwood_transaction(rig, membuf, ackbuf, sizeof(ackbuf));

    if (retval != RIG_OK)
    {
        return retval;
    }

    strcpy(scf, req);
    strcat(scf, ",%"SCNfreq",%d,%d,%d,%d,0,%d,%d,000,%d,,0");
    retval = num_sscanf(ackbuf, scf,
                        &freq, &step, &shift, &rev, &tone,
                        &ctcss, &tonefq, &ctcssfq);

    chan->freq = freq;
    chan->vfo = RIG_VFO_MEM;
    chan->tuning_step = rig->state.tuning_steps[step].ts;

    if (freq < MHz(138))
    {
        chan->mode = RIG_MODE_AM;
    }
    else
    {
        chan->mode = RIG_MODE_FM;
    }

    switch (shift)
    {
    case 0 :
        chan->rptr_shift = RIG_RPT_SHIFT_NONE;
        break;

    case 1 :
        chan->rptr_shift = RIG_RPT_SHIFT_PLUS;
        break;

    case 2 :
        chan->rptr_shift = RIG_RPT_SHIFT_MINUS;
        break;
    }

    if (tone)
    {
        chan->ctcss_tone = rig->caps->ctcss_list[tonefq == 1 ? 0 : tonefq - 2];
    }
    else
    {
        chan->ctcss_tone = 0;
    }

    if (ctcss)
    {
        chan->ctcss_sql = rig->caps->ctcss_list[ctcssfq == 1 ? 0 : ctcssfq - 2];
    }
    else
    {
        chan->ctcss_sql = 0;
    }

    chan->tx_freq = RIG_FREQ_NONE;

    if (chan->channel_num < 223 && shift == 0)
    {
        req[5] = '1';
        SNPRINTF(membuf, sizeof(membuf), "%s", req);
        retval = kenwood_transaction(rig, membuf, ackbuf, sizeof(ackbuf));

        if (retval == RIG_OK)
        {
            strcpy(scf, req);
            strcat(scf, ",%"SCNfreq",%d");
            num_sscanf(ackbuf, scf, &freq, &step);
            chan->tx_freq = freq;
        }
    }

    if (chan->channel_num < 200)
    {
        if (chan->channel_num < 100)
        {
            SNPRINTF(membuf, sizeof(membuf), "MNA 0,%03d", chan->channel_num);
        }
        else
        {
            SNPRINTF(membuf, sizeof(membuf), "MNA 1,%03d", chan->channel_num - 100);
        }

        retval = kenwood_transaction(rig, membuf, ackbuf, sizeof(ackbuf));

        if (retval != RIG_OK)
        {
            return retval;
        }

        memcpy(chan->channel_desc, &ackbuf[10], 7);
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
/* --------------------------------------------------------------------- */
int tmv7_set_channel(RIG *rig, vfo_t vfo, const channel_t *chan)
{
    char membuf[ACKBUF_LEN];
    int retval;
    char req[64];
    long freq;
    int step, shift, tone, ctcss, tonefq, ctcssfq;

    freq = (long)chan->freq;

    for (step = 0; rig->state.tuning_steps[step].ts != 0; step++)
        if (chan->tuning_step == rig->state.tuning_steps[step].ts) { break; }

    switch (chan->rptr_shift)
    {
    case RIG_RPT_SHIFT_NONE :
        shift = 0;
        break;

    case  RIG_RPT_SHIFT_PLUS:
        shift = 1;
        break;

    case RIG_RPT_SHIFT_MINUS:
        shift = 2;
        break;

    default:
        rig_debug(RIG_DEBUG_ERR, "%s: not supported shift\n", __func__);
        return -RIG_EINVAL;
    }

    if (chan->ctcss_tone == 0)
    {
        tone = 0; tonefq = 9;
    }
    else
    {
        tone = 1;

        for (tonefq = 0; rig->caps->ctcss_list[tonefq] != 0; tonefq++)
        {
            if (rig->caps->ctcss_list[tonefq] == chan->ctcss_tone)
            {
                break;
            }
        }

        tonefq = tonefq == 0 ? 1 : tonefq + 2;
    }

    if (chan->ctcss_sql == 0)
    {
        ctcss = 0; ctcssfq = 9;
    }
    else
    {
        ctcss = 1;

        for (ctcssfq = 0; rig->caps->ctcss_list[ctcssfq] != 0; ctcssfq++)
        {
            if (rig->caps->ctcss_list[ctcssfq] == chan->ctcss_sql)
            {
                break;
            }
        }

        ctcssfq = ctcssfq == 0 ? 1 : ctcssfq + 2;
    }

    if (chan->channel_num < 100)
    {
        SNPRINTF(req, sizeof(req), "MW 0,0,%03d", chan->channel_num);
    }
    else if (chan->channel_num < 200)
    {
        SNPRINTF(req, sizeof(req), "MW 1,0,%03d", chan->channel_num - 100);
    }
    else if (chan->channel_num < 204)
    {
        SNPRINTF(req, sizeof(req), "MW 0,0,L%01d", chan->channel_num - 200);
    }
    else if (chan->channel_num < 211)
    {
        SNPRINTF(req, sizeof(req), "MW 1,0,L%01d", chan->channel_num - 203);
    }
    else if (chan->channel_num < 214)
    {
        SNPRINTF(req, sizeof(req), "MW 0,0,U%01d", chan->channel_num - 210);
    }
    else if (chan->channel_num < 220)
    {
        SNPRINTF(req, sizeof(req), "MW 1,0,U%01d", chan->channel_num - 213);
    }
    else if (chan->channel_num < 223)
    {
        if (chan->channel_num == 221)
        {
            SNPRINTF(req, sizeof(req), "CW 0,0");
        }

        if (chan->channel_num == 222)
        {
            SNPRINTF(req, sizeof(req), "CW 1,0");
        }
    }
    else
    {
        return -RIG_EINVAL;
    }

    if (chan->channel_num < 221)
    {
        SNPRINTF(membuf, sizeof(membuf),
                 "%s,%011ld,%01d,%01d,0,%01d,%01d,0,%02d,000,%02d,0,0",
                 req, (long)freq, step, shift, tone,
                 ctcss, tonefq, ctcssfq);
    }
    else
    {
        SNPRINTF(membuf, sizeof(membuf),
                 "%s,%011ld,%01d,%01d,0,%01d,%01d,0,%02d,000,%02d,",
                 req, (long)freq, step, shift, tone,
                 ctcss, tonefq, ctcssfq);
    }

    retval = kenwood_transaction(rig, membuf, NULL, 0);

    if (retval != RIG_OK)
    {
        return retval;
    }

    if (chan->tx_freq != RIG_FREQ_NONE)
    {
        req[5] = '1';
        // cppcheck-suppress *
        SNPRINTF(membuf, sizeof(membuf), "%s,%011"PRIll",%01d", req,
                 (int64_t)chan->tx_freq, step);
        retval = kenwood_transaction(rig, membuf, NULL, 0);

        if (retval != RIG_OK)
        {
            return retval;
        }
    }

    if (chan->channel_num < 200)
    {
        if (chan->channel_num < 100)
        {
            SNPRINTF(membuf, sizeof(membuf), "MNA 0,%03d,%s", chan->channel_num,
                     chan->channel_desc);
        }
        else
        {
            SNPRINTF(membuf, sizeof(membuf), "MNA 1,%03d,%s", chan->channel_num - 100,
                     chan->channel_desc);
        }

        retval = kenwood_transaction(rig, membuf, NULL, 0);

        if (retval != RIG_OK)
        {
            return retval;
        }
    }

    return RIG_OK;
}
