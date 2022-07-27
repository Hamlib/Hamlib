/*
 *  Hamlib AOR backend - AR3030 description
 *  Copyright (c) 2000-2005 by Stephane Fillod
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
#include <string.h>

#include "hamlib/rig.h"
#include "serial.h"
#include "idx_builtin.h"
#include "misc.h"
#include "aor.h"


static int ar3030_set_vfo(RIG *rig, vfo_t vfo);
static int ar3030_get_vfo(RIG *rig, vfo_t *vfo);
static int ar3030_set_freq(RIG *rig, vfo_t vfo, freq_t freq);
static int ar3030_get_freq(RIG *rig, vfo_t vfo, freq_t *freq);
static int ar3030_set_mode(RIG *rig, vfo_t vfo, rmode_t mode, pbwidth_t width);
static int ar3030_get_mode(RIG *rig, vfo_t vfo, rmode_t *mode,
                           pbwidth_t *width);
static int ar3030_set_mem(RIG *rig, vfo_t vfo, int ch);
static int ar3030_get_mem(RIG *rig, vfo_t vfo, int *ch);
static int ar3030_set_level(RIG *rig, vfo_t vfo, setting_t level, value_t val);
static int ar3030_get_level(RIG *rig, vfo_t vfo, setting_t level, value_t *val);
static int ar3030_get_channel(RIG *rig, vfo_t vfo, channel_t *chan,
                              int read_only);
static int ar3030_init(RIG *rig);
static int ar3030_cleanup(RIG *rig);
static int ar3030_close(RIG *rig);
static int ar3030_vfo_op(RIG *rig, vfo_t vfo, vfo_op_t op);

struct ar3030_priv_data
{
    int curr_ch;
    int curr_vfo;
};

/*
 * TODO:
 *  set_channel(emulated?),rig_vfo_op
 *  rig_reset(RIG_RESET_MCALL)
 *  quit the remote control mode on close?
 */
#define AR3030_MODES (RIG_MODE_AM|RIG_MODE_AMS|RIG_MODE_CW|RIG_MODE_SSB|RIG_MODE_FM|RIG_MODE_FAX)

#define AR3030_FUNC_ALL (RIG_FUNC_NONE)

#define AR3030_LEVEL (RIG_LEVEL_ATT|RIG_LEVEL_AGC|RIG_LEVEL_RAWSTR)

#define AR3030_PARM (RIG_PARM_NONE)

#define AR3030_VFO_OPS (RIG_OP_FROM_VFO|RIG_OP_MCL)

#define AR3030_VFO (RIG_VFO_A|RIG_VFO_MEM)

/*
 * FIXME:
 */
#define AR3030_STR_CAL { 2, \
    { \
        { 0x00, -60 }, \
        { 0x3f, 60 }  \
    } }

#define AR3030_MEM_CAP {        \
    .freq = 1,      \
    .mode = 1,      \
    .width = 1,     \
    .levels = RIG_LEVEL_SET(AR3030_LEVEL),     \
    .flags = 1, \
}

/*
 * Data was obtained from AR3030 pdf on http://www.aoruk.com
 *
 * ar3030 rig capabilities.
 */
const struct rig_caps ar3030_caps =
{
    RIG_MODEL(RIG_MODEL_AR3030),
    .model_name = "AR3030",
    .mfg_name =  "AOR",
    .version =  "20200113.0",
    .copyright =  "LGPL",
    .status =  RIG_STATUS_STABLE,
    .rig_type =  RIG_TYPE_RECEIVER,
    .ptt_type =  RIG_PTT_NONE,
    .dcd_type =  RIG_DCD_NONE,
    .port_type =  RIG_PORT_SERIAL,
    .serial_rate_min =  4800,
    .serial_rate_max =  9600,
    .serial_data_bits =  8,
    .serial_stop_bits =  2,
    .serial_parity =  RIG_PARITY_NONE,
    .serial_handshake =  RIG_HANDSHAKE_HARDWARE,
    .write_delay =  0,
    .post_write_delay =  50,    /* ms */
    .timeout =  500,
    .retry =  0,
    .has_get_func =  AR3030_FUNC_ALL,
    .has_set_func =  AR3030_FUNC_ALL,
    .has_get_level =  AR3030_LEVEL,
    .has_set_level =  RIG_LEVEL_SET(AR3030_LEVEL),
    .has_get_parm =  AR3030_PARM,
    .has_set_parm =  RIG_PARM_NONE,
    .level_gran =  {},                 /* FIXME: granularity */
    .parm_gran =  {},
    .ctcss_list =  NULL,
    .dcs_list =  NULL,
    .preamp =   { RIG_DBLST_END, },
    .attenuator =   { 10, 20, RIG_DBLST_END, },
    .max_rit =  Hz(0),
    .max_xit =  Hz(0),
    .max_ifshift =  Hz(0),
    .targetable_vfo =  0,
    .transceive =  RIG_TRN_OFF,
    .bank_qty =   0,
    .chan_desc_sz =  0,
    .vfo_ops =  AR3030_VFO_OPS,
    .str_cal = AR3030_STR_CAL,

    .chan_list =  {
        {   0,  99, RIG_MTYPE_MEM, AR3030_MEM_CAP },
        RIG_CHAN_END,
    },

    .rx_range_list1 =  {
        {kHz(30), MHz(30), AR3030_MODES, -1, -1, AR3030_VFO},
        RIG_FRNG_END,
    },
    .tx_range_list1 =  { RIG_FRNG_END, },

    .rx_range_list2 =  {
        {kHz(30), MHz(30), AR3030_MODES, -1, -1, AR3030_VFO},
        RIG_FRNG_END,
    }, /* rx range */
    .tx_range_list2 =  { RIG_FRNG_END, },   /* no tx range, this is a receiver! */

    .tuning_steps =  {
        {AR3030_MODES, 10},
        {AR3030_MODES, 100},
        {AR3030_MODES, kHz(1)},
        {AR3030_MODES, MHz(1)},
        RIG_TS_END,
    },
    /* mode/filter list, .remember =  order matters! */
    .filters =  {
        {RIG_MODE_AM, kHz(6)},
        {RIG_MODE_SSB | RIG_MODE_CW | RIG_MODE_AM, kHz(2.4)},
        {RIG_MODE_CW, 500},
        {RIG_MODE_FM, kHz(15)},
        RIG_FLT_END,
    },

    .rig_init = ar3030_init,
    .rig_cleanup = ar3030_cleanup,
    .rig_close = ar3030_close,

    .set_freq =  ar3030_set_freq,
    .get_freq =  ar3030_get_freq,
    .set_mode =  ar3030_set_mode,
    .get_mode =  ar3030_get_mode,
    .set_vfo =  ar3030_set_vfo,
    .get_vfo =  ar3030_get_vfo,

    .set_level =  ar3030_set_level,
    .get_level =  ar3030_get_level,

    .set_mem =  ar3030_set_mem,
    .get_mem =  ar3030_get_mem,

    .get_channel = ar3030_get_channel,

    .vfo_op = ar3030_vfo_op,
    .hamlib_check_rig_caps = HAMLIB_CHECK_RIG_CAPS
};

/*
 * Function definitions below
 */




/* is LF really needed? */
#define CR "\x0d"
#define EOM "\x0d\x0a"

#define BUFSZ 64

/*
 * ar3030_transaction
 * We assume that rig!=NULL, rig->state!= NULL
 * Otherwise, you'll get a nice seg fault. You've been warned!
 * return value: RIG_OK if everything's fine, negative value otherwise
 */
static int ar3030_transaction(RIG *rig, const char *cmd, int cmd_len,
                              char *data, int *data_len)
{
    int retval;
    struct rig_state *rs;
    int retry = 3;
    char tmpdata[BUFSZ];

    rs = &rig->state;

    if (data == NULL)
    {
        data = tmpdata;
    }

    rig_flush(&rs->rigport);

    do
    {
        retval = write_block(&rs->rigport, (unsigned char *) cmd, cmd_len);

        if (retval != RIG_OK)
        {
            rig_debug(RIG_DEBUG_ERR, "%s: write_block error=%d\n", __func__, retval);
            return retval;
        }

        if (data)
        {
            /* expecting 0x0d0x0a on all commands so wait for the 0x0a */
            retval = read_string(&rs->rigport, (unsigned char *) data, BUFSZ,
                                 "\x0a", 1, 0, 1);

            if (retval == -RIG_ETIMEOUT)
            {
                rig_debug(RIG_DEBUG_ERR, "%s:timeout retry=%d\n", __func__, retry);
                hl_usleep(50000);
            }
        }

        rig_debug(RIG_DEBUG_TRACE, "%s: retval=%d retry=%d\n", __func__, retval, retry);
    }
    while ((retval <= 0) && (--retry > 0));

    hl_usleep(1000); // 1ms sleep per manual

    if (data_len != NULL && retval > 0)
    {
        *data_len = 0;

        /* only set data_len non-zero if not a command response */
        if (data[0] != 0x00 && data[0] != 0x0d)
        {
            *data_len = retval;
        }
    }

    rig_debug(RIG_DEBUG_TRACE, "%s: return data_len=%d retry=%d\n", __func__,
              data_len ? *data_len : 0, retry);

    return RIG_OK;
}


int ar3030_init(RIG *rig)
{
    struct ar3030_priv_data *priv;

    rig->state.priv = calloc(1, sizeof(struct ar3030_priv_data));

    if (!rig->state.priv)
    {
        return -RIG_ENOMEM;
    }

    priv = rig->state.priv;

    priv->curr_ch = 99; /* huh! FIXME: get_mem in open() ? */
    priv->curr_vfo = RIG_VFO_A;

    return RIG_OK;
}

int ar3030_cleanup(RIG *rig)
{
    struct ar3030_priv_data *priv = rig->state.priv;

    free(priv);

    return RIG_OK;
}

int ar3030_close(RIG *rig)
{
    int retval;
    struct rig_state *rs;

    rig_debug(RIG_DEBUG_TRACE, "%s:\n", __func__);

    rs = &rig->state;
    rig_flush(&rs->rigport);

    retval = ar3030_transaction(rig, "Q" CR, strlen("Q" CR), NULL, NULL);
    rig_debug(RIG_DEBUG_TRACE, "%s: retval=%d\n", __func__, retval);

    return retval;
}

int ar3030_set_vfo(RIG *rig, vfo_t vfo)
{
    struct ar3030_priv_data *priv = (struct ar3030_priv_data *)rig->state.priv;
    char *cmd = "";
    int retval;

    switch (vfo)
    {
    case RIG_VFO_CURR:
        return RIG_OK;

    case RIG_VFO_VFO:
    case RIG_VFO_A:
        cmd = "D" CR;
        break;

    case RIG_VFO_MEM:
        cmd = "M" CR;
        break;

    default:
        return -RIG_EINVAL;
    }

    retval = ar3030_transaction(rig, cmd, strlen(cmd), NULL, NULL);

    if (retval == RIG_OK)
    {
        priv->curr_vfo = vfo;
    }

    return retval;
}

int ar3030_get_vfo(RIG *rig, vfo_t *vfo)
{
    struct ar3030_priv_data *priv = (struct ar3030_priv_data *)rig->state.priv;

    *vfo = priv->curr_vfo;

    return RIG_OK;
}


/*
 * ar3030_set_freq
 * Assumes rig!=NULL
 */
int ar3030_set_freq(RIG *rig, vfo_t vfo, freq_t freq)
{
    struct ar3030_priv_data *priv = (struct ar3030_priv_data *)rig->state.priv;
    char freqbuf[BUFSZ];
    int retval;

    SNPRINTF(freqbuf, sizeof(freqbuf), "%03.6f" CR, ((double)freq) / MHz(1));

    retval = ar3030_transaction(rig, freqbuf, strlen(freqbuf), NULL, NULL);

    if (retval != RIG_OK)
    {
        return retval;
    }

    priv->curr_vfo = RIG_VFO_A;

    return RIG_OK;
}

/*
 * ar3030_get_freq
 * Assumes rig!=NULL, freq!=NULL
 */
int ar3030_get_freq(RIG *rig, vfo_t vfo, freq_t *freq)
{
    struct ar3030_priv_data *priv = (struct ar3030_priv_data *)rig->state.priv;
    char *rfp;
    int freq_len, retval;
    char freqbuf[BUFSZ];
    long lfreq;

    /*
     * D Rn Gn Bn Tn Fnnnnnnnn C
    * Note: spaces are transmitted.
     */
    retval = ar3030_transaction(rig, "D" CR, 2, freqbuf, &freq_len);

    if (retval != RIG_OK)
    {
        return retval;
    }

    priv->curr_vfo = RIG_VFO_A;
    rfp = strchr(freqbuf, 'F');

    if (!rfp)
    {
        return -RIG_EPROTO;
    }

    sscanf(rfp + 1, "%ld", &lfreq);
    *freq = lfreq;
    rig_debug(RIG_DEBUG_ERR, "%s: read lfreq=%ld, freq=%.6f\n", __func__, lfreq,
              *freq);

    return RIG_OK;
}

/*
 * ar3030_set_mode
 * Assumes rig!=NULL
 */
int ar3030_set_mode(RIG *rig, vfo_t vfo, rmode_t mode, pbwidth_t width)
{
    char mdbuf[BUFSZ];
    int aormode, retval;

    switch (mode)
    {
    case RIG_MODE_AM:
        aormode = 'A';
        break;

    case RIG_MODE_CW:
        aormode = 'C';
        break;

    case RIG_MODE_USB:
        aormode = 'U';
        break;

    case RIG_MODE_LSB:
        aormode = 'L';
        break;

    case RIG_MODE_FM:
        aormode = 'N';
        break;

    case RIG_MODE_AMS:
        aormode = 'S';
        break;

    case RIG_MODE_FAX:
        aormode = 'X';
        break;

    default:
        rig_debug(RIG_DEBUG_ERR, "%s: unsupported mode %s\n",
                  __func__, rig_strrmode(mode));
        return -RIG_EINVAL;
    }

    if (width != RIG_PASSBAND_NOCHANGE)
    {
        SNPRINTF(mdbuf, sizeof(mdbuf), "%c" CR, aormode);
    }
    else
    {
        SNPRINTF(mdbuf, sizeof(mdbuf), "%dB%c" CR,
                 width < rig_passband_normal(rig, mode) ? 1 : 0,
                 aormode);
    }

    retval = ar3030_transaction(rig, mdbuf, strlen(mdbuf), NULL, NULL);

    return retval;
}

/*
 * ar3030_get_mode
 * Assumes rig!=NULL, mode!=NULL
 */
int ar3030_get_mode(RIG *rig, vfo_t vfo, rmode_t *mode, pbwidth_t *width)
{
    struct ar3030_priv_data *priv = (struct ar3030_priv_data *)rig->state.priv;
    int buf_len, retval;
    char buf[BUFSZ];

    /*
     * D Rn Gn Bn Tn Fnnnnnnnn C
     * Note: spaces are transmitted
     */
    retval = ar3030_transaction(rig, "D" CR, 2, buf, &buf_len);

    if (retval != RIG_OK)
    {
        return retval;
    }

    priv->curr_vfo = RIG_VFO_A;

    switch (buf[25])
    {
    case 'A':
        *mode = RIG_MODE_AM;
        break;

    case 'L':
        *mode = RIG_MODE_LSB;
        break;

    case 'U':
        *mode = RIG_MODE_USB;
        break;

    case 'C':
        *mode = RIG_MODE_CW;
        break;

    case 'S':
        *mode = RIG_MODE_AMS;
        break;

    case 'N':
        *mode = RIG_MODE_FM;
        break;

    case 'X':
        *mode = RIG_MODE_FAX;
        break;

    default:
        rig_debug(RIG_DEBUG_ERR, "%s: unsupported mode '%c'\n",
                  __func__, buf[25]);
        return -RIG_EPROTO;
    }

    *width = buf[9] == '1' ? rig_passband_narrow(rig, *mode) :
             rig_passband_normal(rig, *mode);

    return RIG_OK;
}


int ar3030_set_mem(RIG *rig, vfo_t vfo, int ch)
{
    struct ar3030_priv_data *priv = (struct ar3030_priv_data *)rig->state.priv;
    int retval = RIG_OK;

    if (priv->curr_vfo == RIG_VFO_MEM)
    {
        char cmdbuf[BUFSZ];
        SNPRINTF(cmdbuf, sizeof(cmdbuf), "%02dM" CR, ch);
        retval = ar3030_transaction(rig, cmdbuf, strlen(cmdbuf), NULL, NULL);
    }

    if (retval == RIG_OK)
    {
        priv->curr_ch = ch;
    }

    return retval;
}

int ar3030_get_mem(RIG *rig, vfo_t vfo, int *ch)
{
    struct ar3030_priv_data *priv = (struct ar3030_priv_data *)rig->state.priv;
    char infobuf[BUFSZ];
    int info_len, retval;

    if (priv->curr_vfo != RIG_VFO_MEM)
    {
        *ch = priv->curr_ch;
    }

    retval = ar3030_transaction(rig, "M" CR, 2, infobuf, &info_len);

    if (retval != RIG_OK)
    {
        return retval;
    }

    /*
     * MnnPnRnGnBnTnFnnnnnnnnC
     */
    if (infobuf[0] != 'M')
    {
        return -RIG_EPROTO;
    }

    /*
     * Is it a blank mem channel ?
     */
    if (infobuf[1] == '-' && infobuf[2] == '-')
    {
        *ch = -1;   /* FIXME: return error instead? */
        return RIG_OK;
    }

    *ch = priv->curr_ch = atoi(infobuf + 1);

    return RIG_OK;
}


int ar3030_set_level(RIG *rig, vfo_t vfo, setting_t level, value_t val)
{
    char *cmd;
    int retval;

    switch (level)
    {
    case RIG_LEVEL_AGC:
        /* SLOW otherwise */
        cmd = val.i == RIG_AGC_FAST ? "1G" CR : "0G" CR;
        break;

    case RIG_LEVEL_ATT:
        cmd = val.i == 0 ? "0R" CR :
              (val.i == 1 ? "1R" CR : "2R" CR);
        break;

    default:
        return -RIG_EINVAL;
    }

    retval = ar3030_transaction(rig, cmd, strlen(cmd), NULL, NULL);

    if (retval != RIG_OK)
    {
        return retval;
    }

    return RIG_OK;
}


int ar3030_get_level(RIG *rig, vfo_t vfo, setting_t level, value_t *val)
{
    struct ar3030_priv_data *priv = (struct ar3030_priv_data *)rig->state.priv;
    int info_len, retval;
    char infobuf[BUFSZ], *p;

    switch (level)
    {
    case RIG_LEVEL_ATT:
        /*
         * DRnGnBnTnFnnnnnnnnC
         */
        retval = ar3030_transaction(rig, "D" CR, 2, infobuf, &info_len);

        if (retval != RIG_OK)
        {
            return retval;
        }

        priv->curr_vfo = RIG_VFO_A;
        p = strchr(infobuf, 'R');

        if (!p)
        {
            return -RIG_EPROTO;
        }

        val->i = p[1] == '0' ? 0 : rig->caps->attenuator[p[1] - '1'];
        return RIG_OK;

    case RIG_LEVEL_AGC:
        /*
         * DRnGnBnTnFnnnnnnnnC
         */
        retval = ar3030_transaction(rig, "D" CR, 2, infobuf, &info_len);

        if (retval != RIG_OK)
        {
            return retval;
        }

        priv->curr_vfo = RIG_VFO_A;
        p = strchr(infobuf, 'G');

        if (!p)
        {
            return -RIG_EPROTO;
        }

        val->i = p[1] == '0' ? RIG_AGC_SLOW : RIG_AGC_FAST;
        return RIG_OK;

    case RIG_LEVEL_RAWSTR:
        retval = ar3030_transaction(rig, "Y" CR, 2, infobuf, &info_len);

        if (retval != RIG_OK)
        {
            return retval;
        }

        infobuf[3] = '\0';
        val->i = strtol(infobuf, (char **)NULL, 16);
        return RIG_OK;

    default:
        return -RIG_EINVAL;
    }

    return RIG_OK;
}

int ar3030_get_channel(RIG *rig, vfo_t vfo, channel_t *chan, int read_only)
{
    struct ar3030_priv_data *priv = (struct ar3030_priv_data *)rig->state.priv;
    char cmdbuf[BUFSZ], infobuf[BUFSZ];
    int info_len, retval;


    SNPRINTF(cmdbuf, sizeof(cmdbuf), "%02dM" CR, chan->channel_num);
    retval = ar3030_transaction(rig, cmdbuf, strlen(cmdbuf), infobuf, &info_len);

    if (retval != RIG_OK)
    {
        return retval;
    }

    priv->curr_vfo = RIG_VFO_A;

    /*
     * MnnPnRnGnBnTnFnnnnnnnnC
     */
    if (infobuf[0] != 'M')
    {
        return -RIG_EPROTO;
    }

    /*
     * Is it a blank mem channel ?
     */
    if (infobuf[1] == '-' && infobuf[2] == '-')
    {
        chan->freq = RIG_FREQ_NONE;
        return RIG_OK;
    }

    sscanf(infobuf + 14, "%"SCNfreq, &chan->freq);
    chan->freq *= 10;

    switch (infobuf[22])
    {
    case 'A':
        chan->mode = RIG_MODE_AM;
        break;

    case 'L':
        chan->mode = RIG_MODE_LSB;
        break;

    case 'U':
        chan->mode = RIG_MODE_USB;
        break;

    case 'C':
        chan->mode = RIG_MODE_CW;
        break;

    case 'S':
        chan->mode = RIG_MODE_AMS;
        break;

    case 'N':
        chan->mode = RIG_MODE_FM;
        break;

    case 'X':
        chan->mode = RIG_MODE_FAX;
        break;

    default:
        rig_debug(RIG_DEBUG_ERR, "%s: unsupported mode '%c'\n",
                  __func__, infobuf[22]);
        return -RIG_EPROTO;
    }

    chan->width = infobuf[10] == '1' ?
                  rig_passband_narrow(rig, chan->mode) :
                  rig_passband_normal(rig, chan->mode);


    // cppcheck-suppress *
    chan->levels[LVL_ATT].i = infobuf[6] == '0' ? 0 :
                              rig->caps->attenuator[infobuf[4] - '1'];

    chan->levels[LVL_AGC].i = infobuf[8] == '0' ? RIG_AGC_SLOW : RIG_AGC_FAST;
    chan->flags = infobuf[4] == '1' ? RIG_CHFLAG_SKIP : RIG_CHFLAG_NONE;

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

int ar3030_vfo_op(RIG *rig, vfo_t vfo, vfo_op_t op)
{
    struct ar3030_priv_data *priv = (struct ar3030_priv_data *)rig->state.priv;
    char buf[16];
    int retval;

    switch (op)
    {
    case RIG_OP_MCL:
        SNPRINTF(buf, sizeof(buf), "%02d%%" CR, priv->curr_ch);
        break;

    case RIG_OP_FROM_VFO:
        SNPRINTF(buf, sizeof(buf), "%02dW" CR, priv->curr_ch);
        priv->curr_vfo = RIG_VFO_MEM;
        break;

    default:
        return -RIG_EINVAL;
    }

    retval = ar3030_transaction(rig, buf, strlen(buf), NULL, NULL);

    return retval;
}

