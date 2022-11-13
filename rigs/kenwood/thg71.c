/*
 *  Hamlib Kenwood backend - TH-G71 description
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
#include <stdio.h>
#include <string.h>  /* String function definitions */

#include <hamlib/rig.h>
#include "kenwood.h"
#include "th.h"

#if 1
#define RIG_ASSERT(x)   if (!(x)) { rig_debug(RIG_DEBUG_ERR, "Assertion failed on line %i\n",__LINE__); abort(); }
#else
#define RIG_ASSERT(x)
#endif

#define THG71_VFO (RIG_VFO_A)
#define THG71_MODES (RIG_MODE_FM)

#define THG71_FUNC_ALL (\
                       RIG_FUNC_TBURST \
                       )

#define THG71_LEVEL_ALL (\
            RIG_LEVEL_RAWSTR| \
                        RIG_LEVEL_SQL| \
                        RIG_LEVEL_RFPOWER\
            )

#ifndef RIG_TONEMAX
#define RIG_TONEMAX     38
#endif

#define RIG_VFO_A_OP (RIG_OP_UP|RIG_OP_DOWN)

#define ACKBUF_LEN 128

static rmode_t thg71_mode_table[KENWOOD_MODE_TABLE_MAX] =
{
    [0] = RIG_MODE_FM,
    [1] = RIG_MODE_AM,
};

static struct kenwood_priv_caps  thg71_priv_caps  =
{
    .cmdtrm =  EOM_TH,   /* Command termination character */
    .mode_table = thg71_mode_table,
};


/* thg71 procs */
static int thg71_open(RIG *rig);
static int thg71_decode_event(RIG *rig);
static int thg71_get_mode(RIG *rig, vfo_t vfo, rmode_t *mode, pbwidth_t *width);
static int thg71_set_vfo(RIG *rig, vfo_t vfo);
static int thg71_get_vfo(RIG *rig, vfo_t *vfo);
static int thg71_set_func(RIG *rig, vfo_t vfo, setting_t func, int status);

/*
 * th-g71 rig capabilities.
 *
 * http://www.iw5edi.com/ham-radio/files/TH-G71_Serial_Protocol.pdf
 */
const struct rig_caps thg71_caps =
{
    RIG_MODEL(RIG_MODEL_THG71),
    .model_name = "TH-G71",
    .mfg_name =  "Kenwood",
    .version =  TH_VER ".0",
    .copyright =  "LGPL",
    .status =  RIG_STATUS_STABLE,
    .rig_type =  RIG_TYPE_HANDHELD,
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

    .has_set_func =  THG71_FUNC_ALL,
    .has_get_level =  THG71_LEVEL_ALL,
    .has_set_level =  RIG_LEVEL_SET(THG71_LEVEL_ALL),
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
        {  1,  199, RIG_MTYPE_MEM, {TH_CHANNEL_CAPS}},   /* normal MEM */
        {  200, 209, RIG_MTYPE_EDGE, {TH_CHANNEL_CAPS}}, /* L MEM */
        {  210, 219, RIG_MTYPE_EDGE, {TH_CHANNEL_CAPS}}, /* U MEM */
        {  220, 220, RIG_MTYPE_PRIO, {TH_CHANNEL_CAPS}}, /* Priority */
        {  221, 222, RIG_MTYPE_CALL, {TH_CHANNEL_CAPS}}, /* Call 0/1 */
        {  223, 231, RIG_MTYPE_BAND, {TH_CHANNEL_CAPS}}, /* Band VFO */
        RIG_CHAN_END,
    },

    /* no rx/tx_range_list */
    .rx_range_list1 =  { RIG_FRNG_END, },    /* FIXME: enter region 1 setting */
    .tx_range_list1 =  { RIG_FRNG_END, },
    .rx_range_list2 =  {
        {MHz(118), MHz(174), THG71_MODES, -1, -1, THG71_VFO},
        {MHz(400), MHz(470), THG71_MODES, -1, -1, THG71_VFO},
        RIG_FRNG_END,
    },
    .tx_range_list2 =  {
        {MHz(144), MHz(148), THG71_MODES, W(0.05), W(5), THG71_VFO},
        {MHz(430), MHz(450), THG71_MODES, W(0.05), W(5), THG71_VFO},
        RIG_FRNG_END,
    },

    /* computed in thg71_open */

    .tuning_steps =  {
        {RIG_MODE_FM, kHz(5)},
        {RIG_MODE_FM, kHz(6.25)},
        {RIG_MODE_FM, kHz(10)},
        {RIG_MODE_FM, kHz(12.5)},
        {RIG_MODE_FM, kHz(15)},
        {RIG_MODE_FM, kHz(20)},
        {RIG_MODE_FM, kHz(25)},
        {RIG_MODE_FM, kHz(30)},
        {RIG_MODE_FM, kHz(50)},
        {RIG_MODE_FM, kHz(100)},
        RIG_TS_END,
    },
    /* mode/filter list, remember: order matters! */
    .filters =  {
        {RIG_MODE_FM, kHz(12)},
        {RIG_MODE_AM, kHz(9)},
        RIG_FLT_END,
    },

    .str_cal = { 3, { { 0, -60 }, {1, -30 }, {5, -13}}}, /* guessed from technical manual */

    .priv = (void *)& thg71_priv_caps,
    .rig_init = kenwood_init,
    .rig_cleanup = kenwood_cleanup,
    .rig_open =  thg71_open,
    .rig_close =  kenwood_close,

    .set_freq =  th_set_freq,
    .get_freq =  th_get_freq,
    .get_mode =  thg71_get_mode,
    .set_vfo =  thg71_set_vfo,
    .get_vfo =  thg71_get_vfo,
    .set_mem =  th_set_mem,
    .get_mem =  th_get_mem,
    .set_channel =  th_set_channel,
    .get_channel =  th_get_channel,
    .set_trn =  th_set_trn,
    .get_trn =  th_get_trn,

    .set_func =  thg71_set_func,
    .get_level = th_get_level,
    .set_level = th_set_level,
    .get_info =  th_get_info,
    .vfo_op = th_vfo_op,
    .set_ptt = th_set_ptt,
    .get_dcd = th_get_dcd,
    .decode_event =  thg71_decode_event,
    .hamlib_check_rig_caps = HAMLIB_CHECK_RIG_CAPS
};


/* --------------------------------------------------------------------- */
int thg71_decode_event(RIG *rig)
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

        retval = sscanf(asyncbuf, "BUF 0,%"SCNfreq",%d,%d,%d,%d,%d,,%d,,%d,%"SCNfreq,
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
int thg71_get_mode(RIG *rig, vfo_t vfo, rmode_t *mode, pbwidth_t *width)
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

    sscanf(ackbuf, "FQ %"SCNfreq",%d", &freq, &step);

    if (freq < MHz(136))
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

/* --------------------------------------------------------------------- */
int thg71_set_vfo(RIG *rig, vfo_t vfo)
{
    char vfobuf[16];
    int retval;

    rig_debug(RIG_DEBUG_TRACE, "%s: called\n", __func__);

    switch (vfo)
    {
    case RIG_VFO_A:
    case RIG_VFO_VFO:
        SNPRINTF(vfobuf, sizeof(vfobuf), "VMC 0,0");
        break;

    case RIG_VFO_MEM:
        SNPRINTF(vfobuf, sizeof(vfobuf), "VMC 0,2");
        break;

    default:
        rig_debug(RIG_DEBUG_ERR, "%s: Unsupported VFO %s\n", __func__, rig_strvfo(vfo));
        return -RIG_EVFO;
    }

    retval = kenwood_transaction(rig, vfobuf, NULL, 0);

    if (retval != RIG_OK)
    {
        return retval;
    }

    return RIG_OK;
}

/* --------------------------------------------------------------------- */
int thg71_get_vfo(RIG *rig, vfo_t *vfo)
{
    char ackbuf[ACKBUF_LEN];
    int retval;
    int vch;

    retval = kenwood_transaction(rig, "VMC 0", ackbuf, sizeof(ackbuf));

    if (retval != RIG_OK)
    {
        return retval;
    }

    sscanf(ackbuf, "VMC 0,%d", &vch);

    switch (vch)
    {
    case 0:
        *vfo = RIG_VFO_A;
        break;

    case 1:
    case 2:
        *vfo = RIG_VFO_MEM;
        break;

    default:
        rig_debug(RIG_DEBUG_ERR, "%s: Unsupported VFO %s\n", __func__,
                  rig_strvfo(*vfo));
        return -RIG_EVFO;
    }

    return RIG_OK;
}

/* --------------------------------------------------------------------- */
int thg71_set_func(RIG *rig, vfo_t vfo, setting_t func, int status)
{
    if (func != RIG_FUNC_TBURST)
    {
        return -RIG_EINVAL;
    }

    if (status == 1)
    {
        int retval = kenwood_transaction(rig, "TT", NULL, 0);

        if (retval != RIG_OK)
        {
            return retval;
        }

        return RIG_OK;
    }

    if (status == 0)
    {
        return rig_set_ptt(rig, vfo, RIG_PTT_OFF);
    }

    return -RIG_EINVAL;
}

/* --------------------------------------------------------------------- */
int thg71_open(RIG *rig)
{
    char ackbuf[ACKBUF_LEN];
    int retval, i;
    const freq_range_t frend = RIG_FRNG_END;

    /* this will check the model id */
    retval = kenwood_open(rig);

    if (retval != RIG_OK)
    {
        return retval;
    }

    /* fill state.rx/tx range_list */
    retval = kenwood_transaction(rig, "FL", ackbuf, sizeof(ackbuf));

    if (retval != RIG_OK)
    {
        return retval;
    }

    strtok(ackbuf, " ");

    for (i = 0; i < HAMLIB_FRQRANGESIZ - 1; i++)
    {
        freq_range_t frng;
        char *strl, *stru;

        strl = strtok(NULL, ",");
        stru = strtok(NULL, ",");

        if (strl == NULL && stru == NULL)
        {
            break;
        }

        frng.startf = MHz(atoi(strl));
        frng.endf = MHz(atoi(stru));
        frng.vfo = RIG_VFO_A;
        frng.ant = 0;

        if (frng.endf <= MHz(135))
        {
            frng.modes = RIG_MODE_AM;
        }
        else
        {
            frng.modes = RIG_MODE_FM;
        }

        frng.high_power = -1;
        frng.low_power = -1;
        frng.label = "";
        rig->state.rx_range_list[i] = frng;

        if (frng.startf > MHz(200))
        {
            frng.high_power = mW(5500);
        }
        else
        {
            frng.high_power = mW(6000);
        }

        frng.low_power = mW(50);
        rig->state.tx_range_list[i] = frng;
    }

    rig->state.rx_range_list[i] = frend;
    rig->state.tx_range_list[i] = frend;
    rig->state.vfo_list = RIG_VFO_A | RIG_VFO_MEM ;

    return RIG_OK;
}

