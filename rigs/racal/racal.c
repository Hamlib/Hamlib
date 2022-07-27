/*
 *  Hamlib Racal backend - main file
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
#include <stdlib.h>
#include <string.h>  /* String function definitions */

#include "hamlib/rig.h"
#include "serial.h"
#include "misc.h"
#include "cal.h"
#include "register.h"
#include "token.h"

#include "racal.h"


const struct confparams racal_cfg_params[] =
{
    {
        TOK_RIGID, "receiver_id", "receiver ID", "receiver ID",
        "0", RIG_CONF_NUMERIC, { .n = { 0, 99, 1 } }
    },
    { RIG_CONF_END, NULL, }
};



#define BUFSZ 32

#define SOM "$"
#define EOM "\x0d"  /* CR */

/*
 * modes
 */
#define MD_AM   1
#define MD_FM   2
#define MD_MCW  3   /* variable BFO */
#define MD_CW   4   /* BFO center */
#define MD_ISB  5   /* option */
#define MD_LSB  6
#define MD_USB  7


/*
 * racal_transaction
 * We assume that rig!=NULL, rig->state!= NULL
 *
 * TODO: Status Response handling with G/T commands
 */
static int racal_transaction(RIG *rig, const char *cmd, char *data,
                             int *data_len)
{
    struct racal_priv_data *priv = (struct racal_priv_data *)rig->state.priv;
    struct rig_state *rs = &rig->state;
    char cmdbuf[BUFSZ + 1];
    int retval;

    SNPRINTF(cmdbuf, sizeof(cmdbuf), SOM "%u%s" EOM, priv->receiver_id, cmd);

    rig_flush(&rs->rigport);

    retval = write_block(&rs->rigport, (unsigned char *) cmdbuf, strlen(cmdbuf));

    if (retval != RIG_OK)
    {
        return retval;
    }


    /* no data expected */
    if (!data || !data_len)
    {
        return retval;
    }

    retval = read_string(&rs->rigport, (unsigned char *) data, BUFSZ, EOM,
                         strlen(EOM), 0, 1);

    if (retval <= 0)
    {
        return retval;
    }

    /* strip CR from string
     */
    if (data[retval - 1] == '\x0d')
    {
        data[--retval] = '\0';  /* chomp */
    }

    *data_len = retval;
    return RIG_OK;
}


int racal_init(RIG *rig)
{
    struct racal_priv_data *priv;

    if (!rig || !rig->caps)
    {
        return -RIG_EINVAL;
    }

    rig->state.priv = (struct racal_priv_data *)calloc(1, sizeof(
                          struct racal_priv_data));

    if (!rig->state.priv)
    {
        /* whoops! memory shortage! */
        return -RIG_ENOMEM;
    }

    priv = rig->state.priv;

    priv->receiver_id = 0;
    priv->bfo = 0;
    priv->threshold = 0;

    return RIG_OK;
}

/*
 */
int racal_cleanup(RIG *rig)
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
 * Assumes rig!=NULL, rig->state.priv!=NULL
 */
int racal_set_conf(RIG *rig, token_t token, const char *val)
{
    struct racal_priv_data *priv = (struct racal_priv_data *)rig->state.priv;

    switch (token)
    {
    case TOK_RIGID:
        priv->receiver_id = atoi(val);
        break;

    default:
        return -RIG_EINVAL;
    }

    return RIG_OK;
}

/*
 * assumes rig!=NULL,
 * Assumes rig!=NULL, rig->state.priv!=NULL
 *  and val points to a buffer big enough to hold the conf value.
 */
int racal_get_conf2(RIG *rig, token_t token, char *val, int val_len)
{
    struct racal_priv_data *priv = (struct racal_priv_data *)rig->state.priv;

    switch (token)
    {
    case TOK_RIGID:
        SNPRINTF(val, val_len, "%u", priv->receiver_id);
        break;

    default:
        return -RIG_EINVAL;
    }

    return RIG_OK;
}

int racal_get_conf(RIG *rig, token_t token, char *val)
{
    return racal_get_conf2(rig, token, val, 128);
}

/*
 * racal_open
 * Assumes rig!=NULL
 */
int racal_open(RIG *rig)
{
    /* Set Receiver to remote control
     *
     * TODO: Perform the BITE routine (1mn!) at each open?
     * TODO: "S5" request values of IF bandwidth filters found?
     */
    return racal_transaction(rig, "S2", NULL, NULL);
}


/*
 * racal_close
 * Assumes rig!=NULL
 */
int racal_close(RIG *rig)
{
    /* Set Receiver to local control */
    return racal_transaction(rig, "S1", NULL, NULL);
}


/*
 * racal_set_freq
 * Assumes rig!=NULL
 */
int racal_set_freq(RIG *rig, vfo_t vfo, freq_t freq)
{
    char freqbuf[BUFSZ];

    SNPRINTF(freqbuf, sizeof(freqbuf), "F%0g", (double)(freq / MHz(1)));

    return racal_transaction(rig, freqbuf, NULL, NULL);
}

int racal_get_freq(RIG *rig, vfo_t vfo, freq_t *freq)
{
    char freqbuf[BUFSZ];
    int retval, len;
    double f;

    retval = racal_transaction(rig, "TF", freqbuf, &len);

    if (retval < RIG_OK)
    {
        return retval;
    }

    if (len < 2 || freqbuf[0] != 'F')
    {
        return -RIG_EPROTO;
    }

    sscanf(freqbuf + 1, "%lf", &f);
    *freq = (freq_t)f * MHz(1);

    return RIG_OK;
}

/*
 * racal_set_mode
 * Assumes rig!=NULL
 */
int racal_set_mode(RIG *rig, vfo_t vfo, rmode_t mode, pbwidth_t width)
{
    struct racal_priv_data *priv = (struct racal_priv_data *)rig->state.priv;
    int ra_mode;
    char buf[BUFSZ];

    switch (mode)
    {
    case RIG_MODE_CW:   ra_mode = (priv->bfo != 0) ? MD_MCW : MD_CW; break;

    case RIG_MODE_USB:  ra_mode = MD_USB; break;

    case RIG_MODE_LSB:  ra_mode = MD_LSB; break;

    case RIG_MODE_AM:   ra_mode = MD_AM; break;

    case RIG_MODE_AMS:  ra_mode = MD_ISB; break;    /* TBC */

    case RIG_MODE_FM:   ra_mode = MD_FM; break;

    default:
        rig_debug(RIG_DEBUG_ERR, "%s: unsupported mode %s\n",
                  __func__, rig_strrmode(mode));
        return -RIG_EINVAL;
    }

    if (width != RIG_PASSBAND_NOCHANGE)
    {
        if (width == RIG_PASSBAND_NORMAL)
        {
            width = rig_passband_normal(rig, mode);
        }

        SNPRINTF(buf, sizeof(buf), "D%dI%.0f", ra_mode, (double)(width / kHz(1)));
    }
    else
    {
        SNPRINTF(buf, sizeof(buf), "D%d", ra_mode);
    }

    return racal_transaction(rig, buf, NULL, NULL);
}

int racal_get_mode(RIG *rig, vfo_t vfo, rmode_t *mode, pbwidth_t *width)
{
    char resbuf[BUFSZ], *p;
    int retval, len;
    double f;

    retval = racal_transaction(rig, "TDI", resbuf, &len);

    if (retval < RIG_OK)
    {
        return retval;
    }

    p = strchr(resbuf, 'I');

    if (len < 3 || resbuf[0] != 'D' || !p)
    {
        return -RIG_EPROTO;
    }

    switch (resbuf[1] - '0')
    {
    case MD_MCW:
    case MD_CW: *mode = RIG_MODE_CW; break;

    case MD_LSB: *mode = RIG_MODE_LSB; break;

    case MD_USB: *mode = RIG_MODE_USB; break;

    case MD_ISB: *mode = RIG_MODE_AMS; break;   /* TBC */

    case MD_FM: *mode = RIG_MODE_FM; break;

    case MD_AM: *mode = RIG_MODE_AM; break;

    default:
        rig_debug(RIG_DEBUG_ERR, "%s: unsupported mode %s\n",
                  __func__, rig_strrmode(*mode));
        return -RIG_EPROTO;
    }

    sscanf(p + 1, "%lf", &f);
    *width = (pbwidth_t)(f * kHz(1));

    return RIG_OK;
}



/*
 * racal_set_level
 * Assumes rig!=NULL
 */
int racal_set_level(RIG *rig, vfo_t vfo, setting_t level, value_t val)
{
    struct racal_priv_data *priv = (struct racal_priv_data *)rig->state.priv;
    char cmdbuf[BUFSZ];
    int agc;

    switch (level)
    {
    case RIG_LEVEL_RF:
        /* Manually set threshold */
        SNPRINTF(cmdbuf, sizeof(cmdbuf), "A%d", (int)(val.f * 120));
        priv->threshold = val.f;
        break;

    case RIG_LEVEL_IF:
        SNPRINTF(cmdbuf, sizeof(cmdbuf), "B%+0g", ((double)val.i) / kHz(1));
        priv->bfo = val.i;
        break;

    case RIG_LEVEL_AGC:
        switch (val.i)
        {
        case RIG_AGC_FAST: agc = 1; break;

        case RIG_AGC_MEDIUM: agc = 2; break;

        case RIG_AGC_SLOW: agc = 3; break;

        case RIG_AGC_USER: agc = 4; break;

        default: return -RIG_EINVAL;
        }

        if (priv->threshold != 0 && agc != 4)
        {
            agc += 4;    /* with manually set threshold */
        }

        SNPRINTF(cmdbuf, sizeof(cmdbuf), "M%d", agc);
        break;

    default:
        rig_debug(RIG_DEBUG_ERR, "%s: unsupported %s\n",
                  __func__, rig_strlevel(level));
        return -RIG_EINVAL;
    }


    return racal_transaction(rig, cmdbuf, NULL, NULL);
}


/*
 * racal_get_level
 * Assumes rig!=NULL
 */
int racal_get_level(RIG *rig, vfo_t vfo, setting_t level, value_t *val)
{
    struct racal_priv_data *priv = (struct racal_priv_data *)rig->state.priv;
    char resbuf[BUFSZ];
    int retval, len, att;
    double f;

    switch (level)
    {
    case RIG_LEVEL_RF:
        /* Manually set threshold */
        retval = racal_transaction(rig, "TA", resbuf, &len);

        if (retval < RIG_OK)
        {
            return retval;
        }

        if (len < 2 || resbuf[0] != 'A')
        {
            return -RIG_EPROTO;
        }

        sscanf(resbuf + 1, "%d", &att);

        val->f = priv->threshold = (float)att / 120;
        break;

    case RIG_LEVEL_IF:
        retval = racal_transaction(rig, "TB", resbuf, &len);

        if (retval < RIG_OK)
        {
            return retval;
        }

        if (len < 2 || resbuf[0] != 'B')
        {
            return -RIG_EPROTO;
        }

        sscanf(resbuf + 1, "%lf", &f);
        val->i = priv->bfo = (shortfreq_t)(f * kHz(1));
        break;

    case RIG_LEVEL_AGC:
        retval = racal_transaction(rig, "TM", resbuf, &len);

        if (retval < RIG_OK)
        {
            return retval;
        }

        if (len < 2 || resbuf[0] != 'M')
        {
            return -RIG_EPROTO;
        }

        switch (resbuf[1] - '0')
        {
        case 1:
        case 5: val->i = RIG_AGC_FAST; break;

        case 2:
        case 6: val->i = RIG_AGC_MEDIUM; break;

        case 3:
        case 7: val->i = RIG_AGC_SLOW; break;

        case 4: val->i = RIG_AGC_USER; break;

        default: return -RIG_EINVAL;
        }

        break;

    default:
        rig_debug(RIG_DEBUG_ERR, "%s: unsupported %s\n", __func__, rig_strlevel(level));
        return -RIG_EINVAL;
    }

    return RIG_OK;
}

/*
 * Assumes rig!=NULL, rig->state.priv!=NULL
 */
int racal_reset(RIG *rig, reset_t reset)
{
    /* Initiate BITE routine, takes 1 minute! */
    return racal_transaction(rig, "S3", NULL, NULL);
}

const char *racal_get_info(RIG *rig)
{
    static char infobuf[64];
    char bitebuf[BUFSZ];
    char filterbuf[BUFSZ];
    int res_len, retval;

    /* get BITE results */
    retval = racal_transaction(rig, "S6", bitebuf, &res_len);

    if (retval < 0)
    {
        return "IO error";
    }

    if (bitebuf[1] == 'O' && bitebuf[2] == 'K')
    {
        bitebuf[3] = '\0';
    }
    else
    {
        char *p = strstr(bitebuf, "END");

        if (p)
        {
            *p = '\0';
        }
    }

    /* get filters */
    retval = racal_transaction(rig, "S5", filterbuf, &res_len);

    if (retval < 0)
    {
        strcpy(filterbuf, "IO error");
    }

    SNPRINTF(infobuf, sizeof(infobuf), "BITE errors: %s, Filters: %s\n",
             bitebuf + 1, filterbuf);

    return infobuf;
}



/*
 * initrigs_racal is called by rig_backend_load
 */
DECLARE_INITRIG_BACKEND(racal)
{
    rig_debug(RIG_DEBUG_VERBOSE, "%s: _init called\n", __func__);

    rig_register(&ra6790_caps);
    rig_register(&ra3702_caps);

    return RIG_OK;
}

