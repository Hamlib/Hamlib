/*
 *  Hamlib TenTenc backend - TT-588 description
 *  Copyright (c) 2003-2009 by Stephane Fillod
 *  Modifications 2014 by Michael Black W9MDB for 1.036 firmware
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
#include <string.h>
#include <stdlib.h>
#include <math.h>

#include <serial.h>
#include <hamlib/rig.h>
#include "tentec.h"
#include "tentec2.h"
#include "bandplan.h"

struct tt588_priv_data
{
    int ch;     /* mem */
    vfo_t vfo_curr;
};


#define TT588_MODES (RIG_MODE_FM|RIG_MODE_CW|RIG_MODE_CWR|RIG_MODE_USB|RIG_MODE_LSB|RIG_MODE_AM)
#define TT588_RXMODES (TT588_MODES)

#define TT588_FUNCS (RIG_FUNC_NR|RIG_FUNC_ANF)

#define TT588_LEVELS (  RIG_LEVEL_STRENGTH| \
            RIG_LEVEL_SQL| \
            RIG_LEVEL_SWR| \
            RIG_LEVEL_RF| \
            RIG_LEVEL_AF| \
            RIG_LEVEL_AGC| \
            RIG_LEVEL_PREAMP| \
            RIG_LEVEL_SWR| \
            RIG_LEVEL_ATT \
            /*RIG_LEVEL_NB| */ \
            /*RIG_LEVEL_IF| */ \
            /*RIG_LEVEL_RFPOWER| */ \
            /*RIG_LEVEL_KEYSPD| */ \
            /*RIG_LEVEL_ANF| */ \
            /*RIG_LEVEL_MICGAIN|*/ \
            /*RIG_LEVEL_NR| */ \
            /*RIG_LEVEL_VOXGAIN| */ \
            /*RIG_LEVEL_VOX| */\
            /*RIG_LEVEL_COMP|*/ \
            )

#define TT588_ANTS (RIG_ANT_1|RIG_ANT_2)

#define TT588_PARMS (RIG_PARM_NONE)

#define TT588_VFO (RIG_VFO_A|RIG_VFO_B)

#define TT588_VFO_OPS (RIG_OP_TO_VFO|RIG_OP_FROM_VFO)

#define TT588_AM  '0'
#define TT588_USB '1'
#define TT588_LSB '2'
#define TT588_CW  '3'
#define TT588_FM  '4'
#define TT588_CWR '5'
// What would FSK mode match in hamlib?  Not implemented.
// #define TT588_FSK '6'
#define EOM "\015"      /* CR */
#define FALSE 0
#define TRUE 1

static int tt588_init(RIG *rig);
static int tt588_reset(RIG *rig, reset_t reset);
static int tt588_get_freq(RIG *rig, vfo_t vfo, freq_t *freq);
static int tt588_set_freq(RIG *rig, vfo_t vfo, freq_t freq);
static int tt588_set_split_freq(RIG *rig, vfo_t vfo, freq_t tx_freq);
static int tt588_get_split_freq(RIG *rig, vfo_t vfo, freq_t *tx_freq);
static int tt588_set_vfo(RIG *rig, vfo_t vfo);
static int tt588_get_vfo(RIG *rig, vfo_t *vfo);
static int tt588_get_mode(RIG *rig, vfo_t vfo, rmode_t *mode, pbwidth_t *width);
static int tt588_set_mode(RIG *rig, vfo_t vfo, rmode_t mode, pbwidth_t width);
static char which_vfo(const RIG *rig, vfo_t vfo);
static int tt588_get_level(RIG *rig, vfo_t vfo, setting_t level, value_t *val);
static int tt588_set_level(RIG *rig, vfo_t vfo, setting_t level, value_t val);
static int tt588_set_split_vfo(RIG *rig, vfo_t vfo, split_t split,
                               vfo_t tx_vfo);
static int tt588_get_split_vfo(RIG *rig, vfo_t vfo, split_t *split,
                               vfo_t *tx_vfo);
static int tt588_set_split_mode(RIG *rig, vfo_t vfo, rmode_t tx_mode,
                                pbwidth_t tx_width);
static int tt588_get_split_mode(RIG *rig, vfo_t vfo, rmode_t *tx_mode,
                                pbwidth_t *tx_width);
static int tt588_set_ptt(RIG *rig, vfo_t vfo, ptt_t ptt);
static int tt588_reset(RIG *rig, reset_t reset);
static const char *tt588_get_info(RIG *rig);
static int tt588_get_xit(RIG *rig, vfo_t vfo, shortfreq_t *xit);
static int tt588_set_xit(RIG *rig, vfo_t vfo, shortfreq_t xit);
static int tt588_set_rit(RIG *rig, vfo_t vfo, shortfreq_t rit);
#if 0 // these are example prototypes for remote operation
static int tt588_get_ant(RIG *rig, vfo_t vfo, ant_t *ant);
static int tt588_set_ant(RIG *rig, vfo_t vfo, ant_t ant);
#endif
/*
 * tt588 transceiver capabilities.
 *
 * Protocol is documented at the tentec site
 */
const struct rig_caps tt588_caps =
{
    RIG_MODEL(RIG_MODEL_TT588),
    .model_name = "TT-588 Omni VII",
    .mfg_name =  "Ten-Tec",
    .version =  "20220718.0",
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

    .has_get_func =  TT588_FUNCS,
    .has_set_func =  TT588_FUNCS,
    .has_get_level =  TT588_LEVELS,
    .has_set_level =  RIG_LEVEL_SET(TT588_LEVELS),
    .has_get_parm =  TT588_PARMS,
    .has_set_parm =  TT588_PARMS,
    .level_gran =  {},                 /* FIXME: granularity */
    .parm_gran =  {},
    .ctcss_list =  NULL,
    .dcs_list =  NULL,
    .preamp =   { 10, RIG_DBLST_END }, /* FIXME: real value */
    .attenuator =   { 6, 12, 18, RIG_DBLST_END },
    .max_rit =  Hz(8192),
    .max_xit =  Hz(8192),
    .max_ifshift =  kHz(2),
    .targetable_vfo =  RIG_TARGETABLE_FREQ | RIG_TARGETABLE_MODE,
    .transceive =  RIG_TRN_OFF,
    .bank_qty =   0,
    .chan_desc_sz =  0,

    .chan_list =  {
        {   0, 127, RIG_MTYPE_MEM, TT_MEM_CAP },
    },

    .rx_range_list1 =  {
        {kHz(500), MHz(30), TT588_RXMODES, -1, -1, TT588_VFO, TT588_ANTS},
        {MHz(48), MHz(54), TT588_RXMODES, -1, -1, TT588_VFO, TT588_ANTS},
        RIG_FRNG_END,
    },
    .tx_range_list1 =  {
        FRQ_RNG_HF(1, TT588_MODES, W(5), W(100), TT588_VFO, TT588_ANTS),
        FRQ_RNG_6m(1, TT588_MODES, W(5), W(100), TT588_VFO, TT588_ANTS),
        RIG_FRNG_END,
    },

    .rx_range_list2 =  {
        {kHz(500), MHz(30), TT588_RXMODES, -1, -1, TT588_VFO, TT588_ANTS},
        {MHz(48), MHz(54), TT588_RXMODES, -1, -1, TT588_VFO, TT588_ANTS},
        RIG_FRNG_END,
    },
    .tx_range_list2 =  {
        FRQ_RNG_HF(2, TT588_MODES, W(5), W(100), TT588_VFO, TT588_ANTS),
        {MHz(5.25), MHz(5.40), TT588_MODES, W(5), W(100), TT588_VFO, TT588_ANTS},
        FRQ_RNG_6m(2, TT588_MODES, W(5), W(100), TT588_VFO, TT588_ANTS),
        RIG_FRNG_END,
    },

    .tuning_steps =  {
        {TT588_RXMODES, 1},
        {TT588_RXMODES, 10},
        {TT588_RXMODES, 100},
        {TT588_RXMODES, kHz(1)},
        {TT588_RXMODES, kHz(10)},
        {TT588_RXMODES, kHz(100)},
        RIG_TS_END,
    },
    /* mode/filter list, remember: order matters! */
    .filters =  {
        {RIG_MODE_CW | RIG_MODE_USB | RIG_MODE_AM, kHz(2.4)},
        {RIG_MODE_CW | RIG_MODE_USB | RIG_MODE_AM, 300},
        {RIG_MODE_CW | RIG_MODE_USB | RIG_MODE_AM, kHz(8)},
        {RIG_MODE_CW | RIG_MODE_USB | RIG_MODE_AM, 0}, /* 34 filters */
        {RIG_MODE_FM, kHz(15)}, /* TBC */
        RIG_FLT_END,
    },
    .priv = (void *) NULL,

    .rig_init =  tt588_init,
    .set_freq =  tt588_set_freq,
    .get_freq =  tt588_get_freq,
    .set_vfo =  tt588_set_vfo,
    .get_vfo =  tt588_get_vfo,
    .set_mode =  tt588_set_mode,
    .get_mode =  tt588_get_mode,
    .get_level =  tt588_get_level,
    .set_level =  tt588_set_level,
    .set_split_freq = tt588_set_split_freq,
    .get_split_freq = tt588_get_split_freq,
    .set_split_mode = tt588_set_split_mode,
    .get_split_mode = tt588_get_split_mode,
    .set_split_vfo =  tt588_set_split_vfo,
    .get_split_vfo =  tt588_get_split_vfo,
    .set_ptt =  tt588_set_ptt,
    .reset =  tt588_reset,
    .get_info =  tt588_get_info,
    .get_xit = tt588_get_xit,
    .set_xit = tt588_set_xit,
    .get_rit = tt588_get_xit,
    .set_rit = tt588_set_rit,
// Antenna functions only in remote mode -- prototypes provided
//.get_ant = tt588_get_ant,
//.set_ant = tt588_set_ant
    .hamlib_check_rig_caps = HAMLIB_CHECK_RIG_CAPS
};

/* Filter table for 588 reciver support. */
static int tt588_rxFilter[] =
{
    12000, 9000, 8000, 7500, 7000, 6500, 6000, 5500, 5000, 4500, 4000, 3800, 3600, 3400, 3200,
    3000, 2800, 2600, 2500, 2400, 2200, 2000, 1800, 1600, 1400,
    1200, 1000, 900, 800, 700, 600, 500, 450, 400, 350, 300, 250, 200
};

/*
 * Function definitions below
 */

/* I frequently see the Omni VII and my laptop get out of sync.  A
   response from the 538 isn't seen by the laptop.  A few "XX"s
   sometimes get things going again, hence this hack, er, function. */
/* Note: data should be at least data_len+1 long for null byte insertion */
static int tt588_transaction(RIG *rig, const char *cmd, int cmd_len, char *data,
                             const int *data_len)
{
    int i, retval = -RIG_EINTERNAL;
    struct  rig_state *rs = &rig->state;

    // The original file had "A few XX's" due to sync problems
    // So I put this in a try loop which should, hopefully, never be seen
    for (i = 0; i < 3; ++i) // We'll try 3 times
    {
        char xxbuf[32];
        rig_flush(&rs->rigport);

        // We add 1 to data_len here for the null byte inserted by read_string eventually
        // That way all the callers can use the expected response length for the cmd_len parameter here
        // Callers all need to ensure they have enough room in data for this
        retval = write_block(&rs->rigport, (unsigned char *) cmd, cmd_len);

        if (retval == RIG_OK)
        {
            // All responses except from "XX" terminate with EOM (i.e. \r) so that is our stop char
            char *term = EOM;

            if (cmd[0] ==
                    'X') // we'll let the timeout take care of this as it shouldn't happen anyways
            {
                term = "";
            }

            if (data)
            {
                retval = read_string(&rs->rigport, (unsigned char *) data, (*data_len) + 1,
                                     term, strlen(term), 0,
                                     1);

                if (retval != -RIG_ETIMEOUT)
                {
                    return RIG_OK;
                }

                if (retval == -RIG_ETIMEOUT) { return retval; }

                rig_debug(RIG_DEBUG_ERR, "%s: read_string failed, try#%d\n", __func__, i + 1);
            }
            else
            {
                return RIG_OK; // no data wanted so just return
            }
        }
        else
        {
            rig_debug(RIG_DEBUG_ERR, "%s: write_block failed, try#%d\n", __func__, i + 1);
        }

        write_block(&rs->rigport, (unsigned char *) "XX" EOM,
                    3); // we wont' worry about the response here
        retval = read_string(&rs->rigport, (unsigned char *) xxbuf, sizeof(xxbuf), "",
                             0, 0, 1); // this should timeout

        if (retval != RIG_OK)
        {
            rig_debug(RIG_DEBUG_ERR, "%s: XX command failed, try#%d\n", __func__, i + 1);
        }
    }

    return retval;
}

/*
 * tt588_init:
 * Basically, it just sets up *priv
 */
int tt588_init(RIG *rig)
{
    struct tt588_priv_data *priv;

    rig_debug(RIG_DEBUG_VERBOSE, "%s:\n", __func__);
    rig->state.priv = (struct tt588_priv_data *) calloc(1, sizeof(
                          struct tt588_priv_data));

    if (!rig->state.priv)
    {
        /* whoops! memory shortage! */
        return -RIG_ENOMEM;
    }

    priv = rig->state.priv;

    memset(priv, 0, sizeof(struct tt588_priv_data));

    /*
     * set arbitrary initial status
     */
    priv->ch = 0;
    priv->vfo_curr = RIG_VFO_A;

    return RIG_OK;
}

static int check_vfo(vfo_t vfo)
{
    switch (vfo)  // Omni VII only has A & B
    {
    case RIG_VFO_A: break;

    case RIG_VFO_B: break;

    case RIG_VFO_CURR: break; // will default to A in which_vfo

    default: return FALSE;
    }

    return TRUE;
}

// which_vfo returns the only two answers that work for commands
// Each calling routine should call check_vfo() before calling this
// Anything other than RIG_VFO_B will return 'A' since Omni VII only uses B on split
// So 'A' is always the default VFO
static char which_vfo(const RIG *rig, vfo_t vfo)
{
    return RIG_VFO_B == vfo ? 'B' : 'A';
}

int tt588_get_vfo(RIG *rig, vfo_t *vfo)
{
    static int getinfo = TRUE;
    struct tt588_priv_data *priv = (struct tt588_priv_data *) rig->state.priv;

    if (getinfo)   // this is the first call to this package so we do this here
    {
        getinfo = FALSE;
        tt588_get_info(rig);
    }

    *vfo = priv->vfo_curr;

    if (check_vfo(*vfo) == FALSE)
    {
        rig_debug(RIG_DEBUG_ERR, "%s: unsupported VFO %s\n", __func__,
                  rig_strvfo(*vfo));
        return -RIG_EINVAL;
    }

    rig_debug(RIG_DEBUG_VERBOSE, "%s: vfo=%s\n", __func__, rig_strvfo(*vfo));
    return RIG_OK;
}

/*
 * tt588_set_vfo
 * Assumes rig!=NULL
 */
int tt588_set_vfo(RIG *rig, vfo_t vfo)
{
    struct tt588_priv_data *priv = (struct tt588_priv_data *)rig->state.priv;

    rig_debug(RIG_DEBUG_VERBOSE, "%s: vfo=%s\n", __func__, rig_strvfo(vfo));

    if (check_vfo(vfo) == FALSE)
    {
        rig_debug(RIG_DEBUG_ERR, "%s: unsupported VFO %s\n", __func__, rig_strvfo(vfo));
        return -RIG_EINVAL;
    }

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
int tt588_reset(RIG *rig, reset_t reset)
{
    int retval, reset_len;
    char reset_buf[32];

    rig_debug(RIG_DEBUG_VERBOSE, "%s: reset=%d\n", __func__, reset);
    reset_len = 32;
    retval = tt588_transaction(rig, "XX" EOM, 3, reset_buf, &reset_len);

    if (retval != RIG_OK)
    {
        return retval;
    }

    if (!strstr(reset_buf, "RADIO START"))
    {
        rig_debug(RIG_DEBUG_ERR, "%s: unexpected answer '%s'\n", __func__, reset_buf);
        return -RIG_EPROTO;
    }

    return RIG_OK;
}

/*
 * tt588_get_freq
 * Assumes rig!=NULL, freq!=NULL
 */
int tt588_get_freq(RIG *rig, vfo_t vfo, freq_t *freq)
{

    int resp_len, retval;
    unsigned char cmdbuf[16], respbuf[32];
    struct tt588_priv_data *priv = (struct tt588_priv_data *) rig->state.priv;

    if (vfo == RIG_VFO_CURR)
    {
        vfo = priv->vfo_curr;
    }

    if (check_vfo(vfo) == FALSE)
    {
        rig_debug(RIG_DEBUG_ERR, "%s: unsupported VFO %s\n", __func__, rig_strvfo(vfo));
        return -RIG_EINVAL;
    }

    SNPRINTF((char *) cmdbuf, sizeof(cmdbuf), "?%c" EOM, which_vfo(rig, vfo));
    resp_len = 6;
    retval = tt588_transaction(rig, (char *) cmdbuf, strlen((char *)cmdbuf),
                               (char *) respbuf,
                               &resp_len);

    if (retval != RIG_OK)
    {
        return retval;
    }

    if (resp_len != 6)
    {
        rig_debug(RIG_DEBUG_ERR, "%s: unexpected length '%d'\n", __func__, resp_len);
        return -RIG_EPROTO;
    }

    if ((respbuf[0] == 'A' || respbuf[0] == 'B') && respbuf[5] == 0x0d)
    {
        *freq = (respbuf[1] << 24)
                + (respbuf[2] << 16)
                + (respbuf[3] << 8)
                + respbuf[4];
    }
    else
    {
        *freq = 0;
    }

    rig_debug(RIG_DEBUG_VERBOSE, "%s: vfo=%s freq=%g\n", __func__, rig_strvfo(vfo),
              *freq);
    return RIG_OK;
}

/*
 * tt588_set_freq
 * assumes rig!=NULL, rig->state.priv!=NULL
 */
int tt588_set_freq(RIG *rig, vfo_t vfo, freq_t freq)
{
    char    bytes[4];
    unsigned char cmdbuf[16];

    rig_debug(RIG_DEBUG_VERBOSE, "%s: vfo=%s freq=%g\n", __func__, rig_strvfo(vfo),
              freq);

    if (check_vfo(vfo) == FALSE)
    {
        rig_debug(RIG_DEBUG_ERR, "%s: unsupported VFO %s\n", __func__, rig_strvfo(vfo));
        return -RIG_EINVAL;
    }

    if (vfo == RIG_VFO_CURR)
    {
        int retval;

        if ((retval = tt588_get_vfo(rig, &vfo)) != RIG_OK)
        {
            return retval;
        }

        rig_debug(RIG_DEBUG_VERBOSE, "%s: set_freq2 vfo=%s\n", __func__,
                  rig_strvfo(vfo));
    }

    /* Freq is 4 bytes long, MSB sent first. */
    bytes[3] = ((int) freq >> 24) & 0xff;
    bytes[2] = ((int) freq >> 16) & 0xff;
    bytes[1] = ((int) freq >>  8) & 0xff;
    bytes[0] = (int) freq        & 0xff;

    SNPRINTF((char *) cmdbuf, sizeof(cmdbuf), "*%c%c%c%c%c" EOM,
             which_vfo(rig, vfo),
             bytes[3], bytes[2], bytes[1], bytes[0]);

    return tt588_transaction(rig, (char *) cmdbuf, 7, NULL,
                             NULL);
}

/*
 * tt588_set_split_freq
 */
int tt588_set_split_freq(RIG *rig, vfo_t vfo, freq_t tx_freq)
{
    // VFOB is the split VFO
    return tt588_set_freq(rig, RIG_VFO_B, tx_freq);
}

/*
 * tt588_get_split_freq
 * assumes rig!=NULL, tx_freq!=NULL
 */
int tt588_get_split_freq(RIG *rig, vfo_t vfo, freq_t *tx_freq)
{
    // VFOB is the split VFO
    return tt588_get_freq(rig, RIG_VFO_B, tx_freq);
}

/*
 * tt588_set_split_mode
 * assumes rig!=NULL
 */
int tt588_set_split_mode(RIG *rig, vfo_t vfo, rmode_t tx_mode,
                         pbwidth_t tx_width)
{
    // VFOB is the split VFO
    return tt588_set_mode(rig, RIG_VFO_B, tx_mode, tx_width);
}

/*
 * tt588_get_split_mode
 * assumes rig!=NULL, tx_mode!=NULLm, tx_width!=NULL
 */
int tt588_get_split_mode(RIG *rig, vfo_t vfo, rmode_t *tx_mode,
                         pbwidth_t *tx_width)
{
    // VFOB is the split VFO
    return tt588_get_mode(rig, RIG_VFO_B, tx_mode, tx_width);
}

/*
 * tt588_get_mode
 * Assumes rig!=NULL, mode!=NULL
 */
int tt588_get_mode(RIG *rig, vfo_t vfo, rmode_t *mode, pbwidth_t *width)
{
    int resp_len, retval;
    unsigned char cmdbuf[16], respbuf[32];
    char ttmode;
    struct tt588_priv_data *priv = (struct tt588_priv_data *) rig->state.priv;


    rig_debug(RIG_DEBUG_VERBOSE, "%s: vfo=%s\n", __func__, rig_strvfo(vfo));

    if (check_vfo(vfo) == FALSE)
    {
        rig_debug(RIG_DEBUG_ERR, "%s: unsupported VFO %s\n", __func__, rig_strvfo(vfo));
        return -RIG_EINVAL;
    }

    if (vfo == RIG_VFO_CURR)
    {
        vfo = priv->vfo_curr;
    }

    // Query mode
    SNPRINTF((char *) cmdbuf, sizeof(cmdbuf), "?M" EOM);
    resp_len = 4;
    retval = tt588_transaction(rig, (char *) cmdbuf, strlen((char *)cmdbuf),
                               (char *) respbuf,
                               &resp_len);

    if (resp_len > 4)
    {
        resp_len = 4;
        respbuf[4] = 0;
    }

    if (retval != RIG_OK)
    {
        return retval;
    }

    if (respbuf[0] != 'M' || resp_len != 4)
    {
        rig_debug(RIG_DEBUG_ERR, "%s: unexpected answer '%s'\n", __func__, respbuf);
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
        rig_debug(RIG_DEBUG_ERR, "%s: unsupported VFO %s\n", __func__, rig_strvfo(vfo));
        return -RIG_EINVAL;
        break;
    }

    switch (ttmode)
    {
    case TT588_AM:  *mode = RIG_MODE_AM;  break;

    case TT588_USB: *mode = RIG_MODE_USB; break;

    case TT588_LSB: *mode = RIG_MODE_LSB; break;

    case TT588_CW: *mode = RIG_MODE_CW;  break;

    case TT588_CWR: *mode = RIG_MODE_CWR;  break;

    case TT588_FM: *mode = RIG_MODE_FM;  break;

    default:
        rig_debug(RIG_DEBUG_ERR, "%s: unsupported mode '%c'\n", __func__, ttmode);
        return -RIG_EPROTO;
    }

    /* Query passband width (filter) */
    SNPRINTF((char *) cmdbuf, sizeof(cmdbuf), "?W" EOM);
    resp_len = 3;
    retval = tt588_transaction(rig, (char *) cmdbuf, strlen((char *)cmdbuf),
                               (char *) respbuf,
                               &resp_len);

    if (retval != RIG_OK)
    {
        return retval;
    }

    if (respbuf[0] != 'W' && resp_len != 3)
    {
        rig_debug(RIG_DEBUG_ERR, "%s: unexpected answer '%s'\n", __func__, respbuf);
        return -RIG_EPROTO;
    }

    switch (respbuf[1])
    {
    case 0: *width = 12000; break;

    case 1: *width = 9000; break;

    case 2: *width = 8000; break;

    case 3: *width = 7500; break;

    case 4: *width = 7000; break;

    case 5: *width = 6500; break;

    case 6: *width = 6000; break;

    case 7: *width = 5500; break;

    case 8: *width = 5000; break;

    case 9: *width = 4500; break;

    case 10: *width = 4000; break;

    case 11: *width = 3800; break;

    case 12: *width = 3600; break;

    case 13: *width = 3400; break;

    case 14: *width = 3200; break;

    case 15: *width = 3000; break;

    case 16: *width = 2800; break;

    case 17: *width = 2600; break;

    case 18: *width = 2500; break;

    case 19: *width = 2400; break;

    case 20: *width = 2200; break;

    case 21: *width = 2000; break;

    case 22: *width = 1800; break;

    case 23: *width = 1600; break;

    case 24: *width = 1400; break;

    case 25: *width = 1200; break;

    case 26: *width = 1000; break;

    case 27: *width = 900; break;

    case 28: *width = 800; break;

    case 29: *width = 700; break;

    case 30: *width = 600; break;

    case 31: *width = 500; break;

    case 32: *width = 450; break;

    case 33: *width = 400; break;

    case 34: *width = 350; break;

    case 35: *width = 300; break;

    case 36: *width = 250; break;

    case 37: *width = 200; break;

    default:
        rig_debug(RIG_DEBUG_ERR, "%s: unexpected bandwidth '%c'\n", __func__,
                  respbuf[1]);
        return -RIG_EPROTO;
    }

    rig_debug(RIG_DEBUG_VERBOSE, "%s: vfo=%s mode=%s width=%d\n", __func__,
              rig_strvfo(vfo), rig_strrmode(*mode), (int)*width);

    return RIG_OK;
}

/* Find rx filter index of bandwidth the same or larger as requested. */
static int tt588_filter_number(int width)
{
    int i;

    for (i = 34; i >= 0; i--)
        if (width <= tt588_rxFilter[i])
        {
            return i;
        }

    return 0; /* Widest filter, 8 kHz. */
}


/*
 * tt588_set_mode
 * Assumes rig!=NULL
 */
int tt588_set_mode(RIG *rig, vfo_t vfo, rmode_t mode, pbwidth_t width)
{
    unsigned char cmdbuf[32], respbuf[32], ttmode;
    int resp_len, retval;

    struct tt588_priv_data *priv = (struct tt588_priv_data *) rig->state.priv;

    rig_debug(RIG_DEBUG_VERBOSE, "%s: vfo=%s mode=%s width=%d\n", __func__,
              rig_strvfo(vfo), rig_strrmode(mode), (int)width);

    if (check_vfo(vfo) == FALSE)
    {
        rig_debug(RIG_DEBUG_ERR, "%s: unsupported VFO %s\n", __func__, rig_strvfo(vfo));
        return -RIG_EINVAL;
    }

    /* Query mode for both VFOs. */
    SNPRINTF((char *) cmdbuf, sizeof(cmdbuf), "?M" EOM);
    resp_len = 4;
    retval = tt588_transaction(rig, (char *) cmdbuf, strlen((char *)cmdbuf),
                               (char *) respbuf,
                               &resp_len);

    if (retval != RIG_OK)
    {
        return retval;
    }

    if (respbuf[0] != 'M' || respbuf[3] != 0x0d)
    {
        rig_debug(RIG_DEBUG_ERR, "%s: unexpected answer '%s'\n", __func__, respbuf);
        return -RIG_EPROTO;
    }

    switch (mode)
    {
    case RIG_MODE_USB:  ttmode = TT588_USB; break;

    case RIG_MODE_LSB:  ttmode = TT588_LSB; break;

    case RIG_MODE_CW:   ttmode = TT588_CW; break;

    case RIG_MODE_CWR:  ttmode = TT588_CWR; break;

    case RIG_MODE_AM:   ttmode = TT588_AM; break;

    case RIG_MODE_FM:   ttmode = TT588_FM; break;

    default:
        rig_debug(RIG_DEBUG_ERR, "%s: unsupported mode %s\n", __func__,
                  rig_strrmode(mode));
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
        rig_debug(RIG_DEBUG_ERR, "%s: unsupported VFO %s\n", __func__, rig_strvfo(vfo));
        return -RIG_EINVAL;
    }

    retval = tt588_transaction(rig, (char *) cmdbuf, 5, NULL, NULL);

    if (retval != RIG_OK)
    {
        return retval;
    }

    /* Set rx filter bandwidth. */
    if (RIG_PASSBAND_NOCHANGE == width) { return retval; }

    if (RIG_PASSBAND_NORMAL == width)
    {
        width = rig_passband_normal(rig, mode);
    }

    width = tt588_filter_number((int) width);
    SNPRINTF((char *) cmdbuf, sizeof(cmdbuf), "*W%c" EOM, (unsigned char) width);
    return tt588_transaction(rig, (char *) cmdbuf, 4, NULL,
                             NULL);
}

/*
 * tt588_get_level
 * Assumes rig!=NULL, val!=NULL
 */
int tt588_get_level(RIG *rig, vfo_t vfo, setting_t level, value_t *val)
{
    float   fwd, refl;
    int retval, lvl_len;
    unsigned char cmdbuf[16], lvlbuf[32];

    if (check_vfo(vfo) == FALSE)
    {
        rig_debug(RIG_DEBUG_ERR, "%s: unsupported VFO %s\n", __func__, rig_strvfo(vfo));
        return -RIG_EINVAL;
    }

    switch (level)
    {
    case RIG_LEVEL_SWR:
        lvl_len = 4;
        retval = tt588_transaction(rig, "?S" EOM, 3, (char *) lvlbuf, &lvl_len);

        if (retval != RIG_OK)
        {
            return retval;
        }

        // top bit of lvlbuf[1] should be on if transmitting
        if (lvlbuf[0] != 'S' || lvl_len != 4 || lvlbuf[3] != 0x0d
                || ((lvlbuf[1] & 0x80) == 0))
        {
            val->f = 99; // infinity
            rig_debug(RIG_DEBUG_ERR,
                      "%s: unexpected answer len=%d buf=%02x %02x %02x %02x\n",
                      __func__, lvl_len, lvlbuf[0], lvlbuf[1], lvlbuf[2], lvlbuf[3]);
            return -RIG_EPROTO;
        }

        /* forward power. */
        fwd = (float)(lvlbuf[1] & 0x7f);
        /* reflected power. */
        refl = (float) lvlbuf[2];

        if (fwd > 0)
        {
            val->f = refl / fwd; // our ratio
            val->f = (1 + val->f) / (1 - val->f); // SWR formula
        }
        else
        {
            val->f = 99;
        }

        break;

    case RIG_LEVEL_STRENGTH:
        lvl_len = 6;
        retval = tt588_transaction(rig, "?S" EOM, 3, (char *) lvlbuf, &lvl_len);

        if (retval != RIG_OK)
        {
            return retval;
        }

        if (lvlbuf[0] != 'S' || lvl_len != 6)
        {
            rig_debug(RIG_DEBUG_ERR, "%s: unexpected answer '%s'\n", __func__, lvlbuf);
            return -RIG_EPROTO;
        }

        // Reply in the form S0944 for 44 dB over S9 in ASCII
        // S0600 is S6 (0 db over S9)
        // S9=34db S0=-20dB
        // So you can read the exact S-meter from the 1st 2 bytes
        // 2nd set of bytes is S9-relative
        if ((lvlbuf[1] & 0x80) == 0) // then we're not in tx mode so we're good
        {
            // 1st two bytes are the S-level
            sscanf((char *)lvlbuf, "S%02d", &val->i);
            val->i  = (val->i - 9) * 6; // convert S meter to dBS9 relative
            rig_debug(RIG_DEBUG_TRACE, "%s: meter= %ddB\n",  __func__, val->i);
        }
        else
        {
            // transmit reply example S<0x8f><0x01> 0x0f=15 watts, 0x01
            // it appears 0x01 refelected = 0W since 0 means not read yet
            int strength;
            int reflected = (int)lvlbuf[2];
            reflected  = reflected > 0 ? reflected - 1 : 0;
            // computer transmit power
            strength = (int)(lvlbuf[1] & 0x7f) - reflected;
            rig_debug(RIG_DEBUG_TRACE, "%s: strength fwd=%d, rev=%d\n",  __func__, strength,
                      reflected);

            if (strength > 0)   // convert watts to dbM
            {
                val->i = 10 * log10(strength) + 30;
                // now convert to db over 1uV
                val->i += 73;
            }
            else
            {
                val->i = 0;
            }

            rig_debug(RIG_DEBUG_TRACE, "%s: strength= %ddB\n",   __func__, val->i);
        }

        break;

    case RIG_LEVEL_AGC:

        /* Read rig's AGC level setting. */
        SNPRINTF((char *) cmdbuf, sizeof(cmdbuf), "?G" EOM);
        lvl_len = 3;
        retval = tt588_transaction(rig, (char *) cmdbuf, strlen((char *)cmdbuf),
                                   (char *) lvlbuf,
                                   &lvl_len);

        if (retval != RIG_OK)
        {
            return retval;
        }

        if (lvlbuf[0] != 'G' || lvl_len != 3 || lvlbuf[2] != 0x0d)
        {
            rig_debug(RIG_DEBUG_ERR, "%s: unexpected answer '%s'\n", __func__, lvlbuf);
            return -RIG_EPROTO;
        }

        switch (lvlbuf[1])
        {
        case '0': val->i = RIG_AGC_OFF; break;

        case '1': val->i = RIG_AGC_SLOW; break;

        case '2': val->i = RIG_AGC_MEDIUM; break;

        case '3': val->i = RIG_AGC_FAST; break;

        default: return -RIG_EPROTO;
        }

        break;

    case RIG_LEVEL_AF:

        /* Volume returned as single byte. */
        SNPRINTF((char *) cmdbuf, sizeof(cmdbuf), "?U" EOM);
        lvl_len = 3;
        retval = tt588_transaction(rig, (char *) cmdbuf, strlen((char *)cmdbuf),
                                   (char *) lvlbuf,
                                   &lvl_len);

        if (retval != RIG_OK)
        {
            return retval;
        }

        if (lvlbuf[0] != 'U' || lvlbuf[2] != 0x0d)
        {
            rig_debug(RIG_DEBUG_ERR, "%s: unexpected answer '%s'\n", __func__, lvlbuf);
            return -RIG_EPROTO;
        }

        val->f = (float) lvlbuf[1] / 127;
        break;

    case RIG_LEVEL_IF:
        // Omni VII has so such thing
        rig_debug(RIG_DEBUG_ERR, "%s: no RIG_LEVEL_IF on Omni VII\n", __func__);
        val->i = 0;
        break;

    case RIG_LEVEL_RF:
        SNPRINTF((char *) cmdbuf, sizeof(cmdbuf), "?I" EOM);
        lvl_len = 3;
        retval = tt588_transaction(rig, (char *) cmdbuf, strlen((char *)cmdbuf),
                                   (char *) lvlbuf,
                                   &lvl_len);

        if (retval != RIG_OK)
        {
            return retval;
        }

        if (lvlbuf[0] != 'I' || lvlbuf[2] != 0x0d)
        {
            rig_debug(RIG_DEBUG_ERR, "%s: unexpected answer '%s'\n", __func__, lvlbuf);
            return -RIG_EPROTO;
        }

        val->f = lvlbuf[1] / 127.0f;
        break;

    case RIG_LEVEL_ATT:

        SNPRINTF((char *) cmdbuf, sizeof(cmdbuf), "?J" EOM);
        lvl_len = 33;
        retval = tt588_transaction(rig, (char *) cmdbuf, strlen((char *)cmdbuf),
                                   (char *) lvlbuf,
                                   &lvl_len);

        if (retval != RIG_OK)
        {
            return retval;
        }

        if (lvlbuf[0] != 'J' || lvlbuf[2] != 0x0d)
        {
            rig_debug(RIG_DEBUG_ERR, "%s: unexpected answer '%s'\n", __func__, lvlbuf);
            return -RIG_EPROTO;
        }

        val->i = (lvlbuf[1] - '0') * 6; // 1=6, 2=12, 3=18
        break;

#if 0

    case RIG_LEVEL_PREAMP:
        /* Only in remote mode */
        val->i = 0;
        break;
#endif

    case RIG_LEVEL_SQL:

        SNPRINTF((char *) cmdbuf, sizeof(cmdbuf), "?H" EOM);
        lvl_len = 3;
        retval = tt588_transaction(rig, (char *) cmdbuf, strlen((char *)cmdbuf),
                                   (char *) lvlbuf,
                                   &lvl_len);

        if (retval != RIG_OK)
        {
            return retval;
        }

        if (lvlbuf[0] != 'H' || lvlbuf[2] != 0x0d)
        {
            rig_debug(RIG_DEBUG_ERR, "%s: unexpected answer '%s'\n", __func__, lvlbuf);
            return -RIG_EPROTO;
        }

        val->f = lvlbuf[1] / 127.0f;
        break;

#if 0

    case RIG_LEVEL_MICGAIN:
        /* Only in remote mode */
        val->i = 0;
        break;
#endif

#if 0

    case RIG_LEVEL_COMP:
        /* Only in remote mode */
        val->i = 0;
        break;
#endif

    default:
        rig_debug(RIG_DEBUG_ERR, "%s: unsupported level %s\n", __func__,
                  rig_strlevel(level));
        return -RIG_EINVAL;
    }

    rig_debug(RIG_DEBUG_VERBOSE, "%s: vfo=%s level=%s\n", __func__, rig_strvfo(vfo),
              rig_strlevel(level));

    return RIG_OK;
}

/*
 * tt588_set_level
 * Assumes rig!=NULL, val!=NULL
 */
int tt588_set_level(RIG *rig, vfo_t vfo, setting_t level, value_t val)
{
    int retval, ii;
    unsigned char cmdbuf[16], agcmode;

    rig_debug(RIG_DEBUG_VERBOSE, "%s: vfo=%s level=%s\n", __func__, rig_strvfo(vfo),
              rig_strlevel(level));

    if (check_vfo(vfo) == FALSE)
    {
        rig_debug(RIG_DEBUG_ERR, "%s: unsupported VFO %s\n", __func__, rig_strvfo(vfo));
        return -RIG_EINVAL;
    }

    switch (level)
    {
    case RIG_LEVEL_AF:

        /* Volume */
        SNPRINTF((char *) cmdbuf, sizeof(cmdbuf), "*U%c" EOM, (char)(val.f * 127));
        retval = tt588_transaction(rig, (char *) cmdbuf, 3, NULL, NULL);

        if (retval != RIG_OK)
        {
            return retval;
        }

        break;

    case RIG_LEVEL_RF:

        /* RF gain. Omni-VII expects value 0 for full gain, and 127 for lowest gain */
        SNPRINTF((char *) cmdbuf, sizeof(cmdbuf), "*I%c" EOM,
                 127 - (char)(val.f * 127));
        retval = tt588_transaction(rig, (char *) cmdbuf, 3, NULL, NULL);

        if (retval != RIG_OK)
        {
            return retval;
        }

        break;

    case RIG_LEVEL_AGC:

        switch (val.i)
        {
        case RIG_AGC_OFF:    agcmode = '0'; break;

        case RIG_AGC_SLOW:   agcmode = '1'; break;

        case RIG_AGC_MEDIUM: agcmode = '2'; break;

        case RIG_AGC_FAST:   agcmode = '3'; break;

        default: return -RIG_EINVAL;
        }

        SNPRINTF((char *) cmdbuf, sizeof(cmdbuf), "*Gx" EOM);
        cmdbuf[2] = agcmode;
        retval = tt588_transaction(rig, (char *) cmdbuf, strlen((char *)cmdbuf), NULL,
                                   NULL);

        if (retval != RIG_OK)
        {
            return retval;
        }

        break;

    case RIG_LEVEL_ATT:
        /* Attenuation */
        ii = -1;        /* Request 0-5 dB -> 0, 6-11 dB -> 6, etc. */

        while (rig->caps->attenuator[++ii] != RIG_DBLST_END)
        {
            if (rig->caps->attenuator[ii] > val.i) { break; }
        }

        SNPRINTF((char *) cmdbuf, sizeof(cmdbuf), "*J%c" EOM, ii + '0');
        retval = tt588_transaction(rig, (char *) cmdbuf, 4, NULL,
                                   NULL);

        if (retval != RIG_OK)
        {
            return retval;
        }

        break;

    case RIG_LEVEL_SQL:
        /* Squelch level, float 0.0 - 1.0 */
        SNPRINTF((char *) cmdbuf, sizeof(cmdbuf), "*H%c" EOM, (int)(val.f * 127));
        retval = tt588_transaction(rig, (char *) cmdbuf, 3, NULL,
                                   NULL);

        if (retval != RIG_OK)
        {
            return retval;
        }

        break;

    default:
        rig_debug(RIG_DEBUG_ERR, "%s: unsupported level %s\n", __func__,
                  rig_strlevel(level));
        return -RIG_EINVAL;
    }

    return RIG_OK;
}

/*
 * tt588_set_split_vfo
 * Assumes rig!=NULL
 */
int tt588_set_split_vfo(RIG *rig, vfo_t vfo, split_t split, vfo_t tx_vfo)
{
    int retval, resp_len;
    char cmdbuf[16], respbuf[16];

    if (tx_vfo == RIG_VFO_SUB)
    {
        tx_vfo = RIG_VFO_B;
    }

    rig_debug(RIG_DEBUG_VERBOSE, "%s: vfo=%s split=%d tx_vfo=%s\n", __func__,
              rig_strvfo(vfo), split, rig_strvfo(tx_vfo));

    if (check_vfo(vfo) == FALSE)
    {
        rig_debug(RIG_DEBUG_ERR, "%s: unsupported VFO %s\n", __func__, rig_strvfo(vfo));
        return -RIG_EINVAL;
    }

    SNPRINTF(cmdbuf, sizeof(cmdbuf), "*Nx" EOM "?N" EOM);

    //if (split == RIG_SPLIT_ON || tx_vfo==RIG_VFO_B)
    if (split == RIG_SPLIT_ON)
    {
        cmdbuf[2] = 1;
    }
    else
    {
        cmdbuf[2] = 0;
    }

    resp_len = 3;
    retval = tt588_transaction(rig, cmdbuf, 4, respbuf, &resp_len);

    if (retval != RIG_OK)
    {
        return retval;
    }

    if (respbuf[0] != 'N' || respbuf[2] != 0x0d)
    {
        rig_debug(RIG_DEBUG_ERR, "%s: unknown response to *N%d='%s'\n", __func__, split,
                  respbuf);
        return -RIG_EINVAL;
    }

    return RIG_OK;
}

/*
 * tt588_get_split_vfo
 * Assumes rig!=NULL, split!=NULL, tx_vfo!=NULL
 */
int tt588_get_split_vfo(RIG *rig, vfo_t vfo, split_t *split, vfo_t *tx_vfo)
{
    int resp_len, retval;
    char cmdbuf[16], respbuf[16];

    if (check_vfo(vfo) == FALSE)
    {
        rig_debug(RIG_DEBUG_ERR, "%s: unsupported VFO %s\n", __func__, rig_strvfo(vfo));
        return -RIG_EINVAL;
    }

    // get split on/off
    SNPRINTF(cmdbuf, sizeof(cmdbuf), "?N" EOM);
    resp_len = 3;
    retval = tt588_transaction(rig, cmdbuf, strlen(cmdbuf), respbuf, &resp_len);

    if (resp_len != 3)
    {
        rig_debug(RIG_DEBUG_ERR, "%s: bad response length, expected %d, got %d\n",
                  __func__, 3, resp_len);
    }

    // respbuf returns "N0" or "N1" for split off/on
    if (retval != RIG_OK)
    {
        return retval;
    }

    if (respbuf[0] != 'N' || respbuf[2] != 0x0d || (respbuf[1] != 0
            && respbuf[1] != 1))
    {
        return -RIG_EPROTO;
    }

    *split = respbuf[1] == 0 ? RIG_SPLIT_OFF : RIG_SPLIT_ON;

    if (*split == RIG_SPLIT_ON)
    {
        *tx_vfo = RIG_VFO_B;    // Omni VII always transmits on VFO_B when in split
    }
    else
    {
        *tx_vfo = RIG_VFO_A;    // VFO A when not in split
    }

    rig_debug(RIG_DEBUG_VERBOSE, "%s: split=%d tx_vfo=%s\n", __func__, *split,
              rig_strvfo(*tx_vfo));

    return RIG_OK;
}

/*
 * tt588_set_ptt
 * Assumes rig!=NULL
 */
int tt588_set_ptt(RIG *rig, vfo_t vfo, ptt_t ptt)
{
    int retval;
    char cmdbuf[32];

    rig_debug(RIG_DEBUG_VERBOSE, "%s: ptt=%d\n", __func__, ptt);

    if (check_vfo(vfo) == FALSE)
    {
        rig_debug(RIG_DEBUG_ERR, "%s: unsupported VFO %s\n", __func__, rig_strvfo(vfo));
        return -RIG_EINVAL;
    }

    SNPRINTF(cmdbuf, sizeof(cmdbuf), "*Txx" EOM);

    if (ptt)
    {
        cmdbuf[2] = 4;
        cmdbuf[3] = 1; // turn on ethernet RIPing
    }
    else
    {
        cmdbuf[2] = 0;
        cmdbuf[3] = 1; // turn on ethernet RIPing
    }

    retval = tt588_transaction(rig, cmdbuf, 5, NULL,
                               0);  // no response

    if (retval != RIG_OK)
    {
        return retval;
    }

    return RIG_OK;
}

/*
 * tt588_get_info
 * Assumes rig!=NULL
 * Returns statically allocated buffer
 */
const char *tt588_get_info(RIG *rig)
{
    static char cmdbuf[16], firmware[64];
    int firmware_len = sizeof(firmware), retval;

    SNPRINTF(cmdbuf, sizeof(cmdbuf), "?V" EOM);
    memset(firmware, 0, sizeof(firmware));
    rig_debug(RIG_DEBUG_VERBOSE, "%s: firmware_len=%d\n", __func__, firmware_len);
    retval = tt588_transaction(rig, cmdbuf, strlen(cmdbuf), firmware,
                               &firmware_len);

    // Response should be  "VER 1010-588 " plus "RADIO x\r" or "REMOTEx\r"
    // if x=blank ham band transmit only
    // if x='M' MARS transmit only
    if (retval != RIG_OK)
    {
        rig_debug(RIG_DEBUG_ERR, "%s: ack NG, len=%d\n", __func__, firmware_len);
        return NULL;
    }

    rig_debug(RIG_DEBUG_VERBOSE, "%s: %s\n", __func__, firmware);
    return firmware;
}

/*
 * tt588_get_xit
 * tt588_get_rit is linked to this too since it's just one offset and the same command
 * Assumes rig!=NULL
 * Note that ?L can't query RIT/XIT separately...there's only one offset
 */
int tt588_get_xit(RIG *rig, vfo_t vfo, shortfreq_t *xit)
{
    int resp_len, retval;
    char cmdbuf[16], respbuf[16];

    if (check_vfo(vfo) == FALSE)
    {
        rig_debug(RIG_DEBUG_ERR, "%s: unsupported VFO %s\n", __func__, rig_strvfo(vfo));
        return -RIG_EINVAL;
    }

    // get xit
    SNPRINTF(cmdbuf, sizeof(cmdbuf), "?L" EOM);
    resp_len = 5;
    retval = tt588_transaction(rig, cmdbuf, strlen(cmdbuf), respbuf, &resp_len);

    if (resp_len != 5)
    {
        rig_debug(RIG_DEBUG_ERR, "%s: bad response length, expected %d, got %d\n",
                  __func__, 5, resp_len);
    }

    if (retval != RIG_OK)
    {
        return retval;
    }

    if (respbuf[0] != 'L' || respbuf[4] != 0x0d)
    {
        return -RIG_EPROTO;
    }

    *xit = (respbuf[2] * (short)256) | respbuf[3];
    rig_debug(RIG_DEBUG_VERBOSE, "%s: rit=%d\n", __func__, (int)*xit);

    return RIG_OK;
}

// This routine handles both rit and xit setting
// Though we can turn on both (which=3) there's no obvious condition for doing so
// We can only query the one offset and don't really know for which it was set
// And is there any reason to turn on both?  If so, hamblib doesn't seem to support that.
//
static int set_rit_xit(RIG *rig, vfo_t vfo, shortfreq_t rit, int which)
{
    int retval;
    char cmdbuf[16];

    rig_debug(RIG_DEBUG_VERBOSE, "%s: rit=%d\n", __func__, (int)rit);

    if (check_vfo(vfo) == FALSE)
    {
        rig_debug(RIG_DEBUG_ERR, "%s: unsupported VFO %s\n", __func__, rig_strvfo(vfo));
        return -RIG_EINVAL;
    }

    // For some reason need an extra \r on here
    // This is with version 1.036 -- it appears to want 7 chars instead of 6
    SNPRINTF(cmdbuf, sizeof(cmdbuf), "*Lxxx" EOM EOM);
    cmdbuf[2] = which;  // set xit bit. 0=off,1=rit, 2=xit, 3=both
    cmdbuf[3] = rit >> 8;
    cmdbuf[4] = rit & 0xff;
    retval = tt588_transaction(rig, cmdbuf, 6, NULL,
                               0);   // no response

    if (retval != RIG_OK)
    {
        return retval;
    }

    return RIG_OK;
}

/*
 * tt588_set_xit
 * Assumes rig!=NULL
 */
int tt588_set_xit(RIG *rig, vfo_t vfo, shortfreq_t xit)
{
    return set_rit_xit(rig, vfo, xit, 2); // bit 2 is xit
}

/*
 * tt588_set_xit
 * Assumes rig!=NULL
 */
int tt588_set_rit(RIG *rig, vfo_t vfo, shortfreq_t rit)
{
    return set_rit_xit(rig, vfo, rit, 1); // bit 1 is rit
}

#if 0 // commenting out prototypes that are only for remote use
/*
 * This is a prototype function as C1V is only available in remote mode
 * tt588_get_ant
 * Assumes rig!=NULL
 */
int tt588_get_ant(RIG *rig, vfo_t vfo, ant_t *ant)
{
    int resp_len, retval;
    char cmdbuf[16], respbuf[16];

    if (check_vfo(vfo) == FALSE)
    {
        rig_debug(RIG_DEBUG_ERR, "%s: unsupported VFO %s\n", __func__, rig_strvfo(vfo));
        return -RIG_EINVAL;
    }

    // get xit
    SNPRINTF(cmdbuf, sizeof(cmdbuf), "*C1V" EOM);
    resp_len = 5;
    // this should be the only line needing change for remote operation
    retval = tt588_transaction(rig, cmdbuf, strlen(cmdbuf), respbuf, &resp_len);

    if (resp_len != 5)
    {
        rig_debug(RIG_DEBUG_ERR, "%s: bad response length, expected %d, got %d\n",
                  __func__, 5, resp_len);
    }

    if (retval != RIG_OK)
    {
        return retval;
    }

    if (respbuf[0] != 'C' || respbuf[4] != 0x0d)
    {
        return -RIG_EPROTO;
    }

    *ant = respbuf[3];
    rig_debug(RIG_DEBUG_VERBOSE, "%s: rit=%d\n", __func__, *ant);

    return RIG_OK;
}

/*
 * This is a prototype function as C1V is only available in remote mode
 * tt588_set_ant
 * Assumes rig!=NULL
 */
int tt588_set_ant(RIG *rig, vfo_t vfo, ant_t ant)
{
    int retval, cmd_len;
    char cmdbuf[16];

    if (check_vfo(vfo) == FALSE)
    {
        rig_debug(RIG_DEBUG_ERR, "%s: unsupported VFO %s\n", __func__, rig_strvfo(vfo));
        return -RIG_EINVAL;
    }

    rig_debug(RIG_DEBUG_VERBOSE, "%s: ant=%d\n", __func__, ant);

    SNPRINTF(cmdbuf, sizeof(cmdbuf), "*C1Vx" EOM);
    // 0 = RX=TX=ANT1
    // 1 = RX=TX=ANT2
    // 2 = RX=RXAUX, TX=ANT1
    // 3 = RX=RXAUC, TX=ANT2
    cmdbuf[4] = ant;
    // this should be the only line needing change for remote operation
    retval = tt588_transaction(rig, cmdbuf, strlen(cmdbuf), NULL,
                               0);   // no response

    if (retval != RIG_OK)
    {
        return retval;
    }

    return RIG_OK;
}
#endif
