/*
 *  Hamlib Skanti backend - main file
 *  Copyright (c) 2004-2005 by Stephane Fillod
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
#include <serial.h>
#include <misc.h>
#include <cal.h>
#include <register.h>

#include "skanti.h"


/* Line Feed */
#define EOM "\x0d"
#define LF "\x0a"
#define PROMPT ">"

#define BUFSZ 32

/*
 * modes
 */
#define MD_LSB  "L"
#define MD_USB  "J"
#define MD_CW   "A1"
#define MD_MCW  "A2"
#define MD_AM   "H"
#define MD_RTTY "F"
#define MD_R3E  "R"


/*
 * skanti_transaction
 * We assume that rig!=NULL, rig->state!= NULL, data!=NULL, data_len!=NULL
 * Otherwise, you'll get a nice seg fault. You've been warned!
 * TODO: error case handling
 */
static int skanti_transaction(RIG *rig, const char *cmd, int cmd_len,
                              char *data, int *data_len)
{
    int retval;
    struct rig_state *rs;

    rs = &rig->state;

    rig_flush(&rs->rigport);

    retval = write_block(&rs->rigport, (unsigned char *) cmd, cmd_len);

    if (retval != RIG_OK)
    {
        return retval;
    }


    /* no data expected, check for OK returned */
    if (!data || !data_len)
    {
        /*
        * Transceiver sends back ">"
        */
        char retbuf[BUFSZ + 1];
        retval = read_string(&rs->rigport, (unsigned char *) retbuf, BUFSZ, PROMPT,
                             strlen(PROMPT), 0, 1);

        if (retval < 0)
        {
            return retval;
        }

        retbuf[retval] = '\0';

        if (strstr(retbuf, PROMPT))
        {
            return RIG_OK;
        }
        else
        {
            return -RIG_ERJCTED;
        }
    }

    retval = read_string(&rs->rigport, (unsigned char *) data, BUFSZ, LF,
                         strlen(LF), 0, 1);

    if (retval == -RIG_ETIMEOUT)
    {
        retval = 0;
    }

    if (retval < 0)
    {
        return retval;
    }

    *data_len = retval;

    /* strip CR/LF from string
     */
    *data_len -= 2;
    data[*data_len] = 0;
    return RIG_OK;
}


/*
 * skanti_reset
 * Assumes rig!=NULL
 */
int skanti_reset(RIG *rig, reset_t reset)
{
    int retval;

    /*
     * master reset
     *
     * returned data: *x1A345SF
     *   whatever this means? unit serial #?
     */

    retval = skanti_transaction(rig, "0" EOM, strlen("0" EOM), NULL, NULL);

    if (retval != RIG_OK)
    {
        return retval;
    }

    return RIG_OK;
}



/*
 * skanti_set_freq
 * Assumes rig!=NULL
 */
int skanti_set_freq(RIG *rig, vfo_t vfo, freq_t freq)
{
    char freqbuf[BUFSZ];

    /* 6 digits */
    SNPRINTF(freqbuf, sizeof(freqbuf), "Z%06ld" EOM, (long)(freq / 100));

    return skanti_transaction(rig, freqbuf, strlen(freqbuf), NULL, NULL);
}

/*
 * skanti_set_mode
 * Assumes rig!=NULL
 */
int skanti_set_mode(RIG *rig, vfo_t vfo, rmode_t mode, pbwidth_t width)
{
    int retval;
    char *sk_mode, *sk_filter;
    pbwidth_t passband_normal;

    switch (mode)
    {
    /* TODO: MCW, R3E */
    case RIG_MODE_CW:       sk_mode = MD_CW EOM; break;

    case RIG_MODE_USB:      sk_mode = MD_USB EOM; break;

    case RIG_MODE_LSB:      sk_mode = MD_LSB EOM; break;

    case RIG_MODE_RTTY:     sk_mode = MD_RTTY EOM; break;

    case RIG_MODE_AM:       sk_mode = MD_AM EOM; break;

    default:
        rig_debug(RIG_DEBUG_ERR, "%s: unsupported mode %s\n",
                  __func__, rig_strrmode(mode));
        return -RIG_EINVAL;
    }

    retval = skanti_transaction(rig, sk_mode, strlen(sk_mode), NULL, NULL);

    if (retval != RIG_OK)
    {
        return retval;
    }

    if (RIG_PASSBAND_NOCHANGE == width) { return retval; }

    /*
     * TODO: please sk8000 owners, check this, I'm not sure
     *          which passband is default!
     */
    passband_normal = rig_passband_normal(rig, mode);

    if (width == RIG_PASSBAND_NORMAL ||
            width == passband_normal)
    {
        sk_filter = "I" EOM;
    }
    else if (width < passband_normal)
    {
        sk_filter = width < 1000 ? "V" EOM : "N" EOM;
    }
    else
    {
        sk_filter = "W" EOM;
    }

    retval = skanti_transaction(rig, sk_filter, strlen(sk_filter), NULL, NULL);

    return retval;
}


/*
 * skanti_set_split_freq
 * Assumes rig!=NULL
 */
int skanti_set_split_freq(RIG *rig, vfo_t vfo, freq_t tx_freq)
{
    char freqbuf[BUFSZ];

    /* 6 digits */
    SNPRINTF(freqbuf, sizeof(freqbuf), "T%06ld" EOM, (long)(tx_freq / 100));

    return skanti_transaction(rig, freqbuf, strlen(freqbuf), NULL, NULL);
}



/*
 * skanti_set_level
 * Assumes rig!=NULL
 * FIXME: cannot support PREAMP and ATT both at same time (make sense though)
 */
int skanti_set_level(RIG *rig, vfo_t vfo, setting_t level, value_t val)
{
    char cmdbuf[BUFSZ], *agc;

    /* Optimize:
     *   sort the switch cases with the most frequent first
     */
    switch (level)
    {
    case RIG_LEVEL_PREAMP:
        SNPRINTF(cmdbuf, sizeof(cmdbuf), "R%c" EOM, val.i ? 'F' : 'O');

        return skanti_transaction(rig, cmdbuf, strlen(cmdbuf), NULL, NULL);

    case RIG_LEVEL_ATT:
        SNPRINTF(cmdbuf, sizeof(cmdbuf), "A%c" EOM, val.i ? 'T' : 'O');

        return skanti_transaction(rig, cmdbuf, strlen(cmdbuf), NULL, NULL);

    case RIG_LEVEL_RFPOWER:
        SNPRINTF(cmdbuf, sizeof(cmdbuf), "M%cO" EOM,
                 val.f < 0.33 ? 'L' : (val.f < 0.66 ? 'M' : 'F'));

        return skanti_transaction(rig, cmdbuf, strlen(cmdbuf), NULL, NULL);

    case RIG_LEVEL_AGC:
        switch (val.i)
        {
        case RIG_AGC_SLOW: agc = "AS"EOM; break;

        case RIG_AGC_FAST: agc = "AA"EOM; break;

        case RIG_AGC_OFF: agc = "AF"EOM; break;

        default: return -RIG_EINVAL;
        }

        return skanti_transaction(rig, agc, strlen(agc), NULL, NULL);

    default:
        rig_debug(RIG_DEBUG_ERR, "%s: unsupported set_level %s\n", __func__,
                  rig_strlevel(level));
        return -RIG_EINVAL;
    }


    return RIG_OK;
}


/*
 * skanti_set_ptt
 * Assumes rig!=NULL, ptt!=NULL
 */
int skanti_set_ptt(RIG *rig, vfo_t vfo, ptt_t ptt)
{
    char cmdbuf[BUFSZ];

    SNPRINTF(cmdbuf, sizeof(cmdbuf), "X%c" EOM, ptt == RIG_PTT_ON ? 'N' : 'F');

    return skanti_transaction(rig, cmdbuf, strlen(cmdbuf), NULL, NULL);
}


int skanti_vfo_op(RIG *rig, vfo_t vfo, vfo_op_t op)
{
    if (op != RIG_OP_TUNE)
    {
        return -RIG_EINVAL;
    }

    return skanti_transaction(rig, "XT"EOM, strlen("XT"EOM), NULL, NULL);
}



/*
 * initrigs_skanti is called by rig_backend_load
 */
DECLARE_INITRIG_BACKEND(skanti)
{
    rig_debug(RIG_DEBUG_VERBOSE, "%s: _init called\n", __func__);

    rig_register(&trp8000_caps);
    rig_register(&trp8255_caps);

    return RIG_OK;
}

