/*
 *  Hamlib R&S backend - main file
 *  Copyright (c) 2009-2010 by Stéphane Fillod
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
#include "register.h"
#include "num_stdio.h"

#include "rs.h"
#include "gp2000.h"
#include "ek89x.h"


#define BUFSZ 64
#define RESPSZ 64

#define LF "\x0a"
#define CR "\x0d"
#define BOM CR
#define EOM CR

/*
 * R&S GB2 protocol ?
 */


/*
 * rs_transaction
 * We assume that rig!=NULL, rig->state!= NULL, data!=NULL, data_len!=NULL
 */
int rs_transaction(RIG *rig, const char *cmd, int cmd_len, char *data,
                   int *data_len)
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


    /* no data expected */
    if (!data || !data_len)
    {
        return RIG_OK;
    }

    retval = read_string(&rs->rigport, (unsigned char *) data, BUFSZ, CR, 1, 0, 1);

    if (retval < 0)
    {
        return retval;
    }

    *data_len = retval;

    return RIG_OK;
}

/*
 * rs_set_freq
 * Assumes rig!=NULL
 */
int rs_set_freq(RIG *rig, vfo_t vfo, freq_t freq)
{
    char freqbuf[32];
    int retval;

    // cppcheck-suppress *
    SNPRINTF(freqbuf, sizeof(freqbuf), BOM "FREQ %"PRIll EOM, (int64_t)freq);
    retval = rs_transaction(rig, freqbuf, strlen(freqbuf), NULL, NULL);

    return retval;
}

/*
 * rs_get_freq
 * Assumes rig!=NULL
 */
int rs_get_freq(RIG *rig, vfo_t vfo, freq_t *freq)
{
    char buf[RESPSZ];
    int len, retval;

#define FREQ_QUERY  BOM "FREQ?" EOM

    retval = rs_transaction(rig, FREQ_QUERY, strlen(FREQ_QUERY), buf, &len);

    if (retval < 0)
    {
        return retval;
    }

    retval = (sscanf(buf, "%"SCNfreq, freq) == 1) ? RIG_OK : -RIG_EPROTO;

    return retval;
}

/*
 * rs_set_mode
 * Assumes rig!=NULL
 */
int rs_set_mode(RIG *rig, vfo_t vfo, rmode_t mode, pbwidth_t width)
{
    char buf[32], *smode;
    int retval;

    switch (mode)
    {
    case RIG_MODE_AM: smode = "AM"; break;

    case RIG_MODE_WFM:
    case RIG_MODE_FM: smode = "FM"; break;

    case RIG_MODE_CW: smode = "CW"; break;

    case RIG_MODE_USB: smode = "USB"; break;

    case RIG_MODE_LSB: smode = "LSB"; break;

    default:
        return -RIG_EINVAL;
    }

    SNPRINTF(buf, sizeof(buf), BOM "DEM %s" EOM, smode);
    retval = rs_transaction(rig, buf, strlen(buf), NULL, NULL);

    if (retval < 0)
    {
        return retval;
    }

    if (width == RIG_PASSBAND_NOCHANGE) { return retval; }

    if (width == RIG_PASSBAND_NORMAL)
    {
        width = rig_passband_normal(rig, mode);
    }

    if (width > 0)
    {
        SNPRINTF(buf, sizeof(buf), BOM "BAND %d" EOM, (int) width);
        retval = rs_transaction(rig, buf, strlen(buf), NULL, NULL);
    }

    return retval;
}

/*
 * rs_get_mode
 * Assumes rig!=NULL
 */
int rs_get_mode(RIG *rig, vfo_t vfo, rmode_t *mode, pbwidth_t *width)
{
    char buf[RESPSZ];
    int buf_len, retval;

#define DEM_QUERY   BOM "DEM?" EOM

    retval = rs_transaction(rig, DEM_QUERY, strlen(DEM_QUERY), buf, &buf_len);

    if (retval < 0)
    {
        return retval;
    }

    *mode = rig_parse_mode(buf);

#define BAND_QUERY   BOM "BAND?" EOM
    retval = rs_transaction(rig, BAND_QUERY, strlen(BAND_QUERY), buf, &buf_len);

    if (retval < 0)
    {
        return retval;
    }

    *width = atoi(buf);

    return retval;
}


int rs_set_func(RIG *rig, vfo_t vfo, setting_t func, int status)
{
    char buf[32], *sfunc;
    int retval;

    switch (func)
    {
    case RIG_FUNC_AFC: sfunc = "FREQ:AFC"; break;

    case RIG_FUNC_SQL: sfunc = "OUTP:SQU"; break;

    case RIG_FUNC_LOCK: sfunc = "DISP:ENAB"; break;

    default:
        return -RIG_EINVAL;
    }

    SNPRINTF(buf, sizeof(buf), BOM "%s %s" EOM, sfunc, status ? "ON" : "OFF");
    retval = rs_transaction(rig, buf, strlen(buf), NULL, NULL);

    return retval;
}

int rs_get_func(RIG *rig, vfo_t vfo, setting_t func, int *status)
{
    char buf[RESPSZ], *sfunc;
    int buf_len, retval;

    switch (func)
    {
    case RIG_FUNC_AFC: sfunc = BOM "FREQ:AFC?" EOM; break;

    case RIG_FUNC_SQL: sfunc = BOM "OUTP:SQU?" EOM; break;

    case RIG_FUNC_LOCK: sfunc = BOM "DISP:ENAB?" EOM; break;

    default:
        return -RIG_EINVAL;
    }

    retval = rs_transaction(rig, sfunc, strlen(sfunc), buf, &buf_len);

    if (retval < 0)
    {
        return retval;
    }

    *status = (!memcmp(buf, "ON", 2) || !memcmp(buf, "1", 1)) ? 1 : 0;

    return retval;
}

int rs_set_level(RIG *rig, vfo_t vfo, setting_t level, value_t val)
{
    char buf[32];
    int retval;

    switch (level)
    {
    case RIG_LEVEL_ATT:
        SNPRINTF(buf, sizeof(buf), BOM "INP:ATT:STAT %s" EOM, val.i ? "ON" : "OFF");
        break;

    case RIG_LEVEL_SQL:
        /* dBuV */
        SNPRINTF(buf, sizeof(buf), BOM "OUTP:SQU:THR %d" EOM, (int)(20 + val.f * 20));
        break;

    case RIG_LEVEL_AF:
        num_snprintf(buf, sizeof(buf), BOM "SYST:AUD:VOL %.1f" EOM, val.f);
        break;

    case RIG_LEVEL_AGC:
    case RIG_LEVEL_RF:
        return -RIG_ENIMPL;

    default:
        return -RIG_EINVAL;
    }

    retval = rs_transaction(rig, buf, strlen(buf), NULL, NULL);

    return retval;
}

int rs_get_level(RIG *rig, vfo_t vfo, setting_t level, value_t *val)
{
    char buf[RESPSZ], *slevel;
    int buf_len, retval;

    switch (level)
    {
    case RIG_LEVEL_STRENGTH: slevel = BOM "SENS:DATA? \"VOLT:AC\"" EOM; break;

    case RIG_LEVEL_ATT: slevel = BOM "INP:ATT:STAT?" EOM; break;

    case RIG_LEVEL_AF: slevel = BOM "SYST:AUD:VOL?" EOM; break;

    case RIG_LEVEL_SQL:
    case RIG_LEVEL_AGC:
    case RIG_LEVEL_RF:
        return -RIG_ENIMPL;

    default:
        return -RIG_EINVAL;
    }

    retval = rs_transaction(rig, slevel, strlen(slevel), buf, &buf_len);

    if (retval < 0)
    {
        return retval;
    }

    switch (level)
    {
    case RIG_LEVEL_STRENGTH:
        /* assumes FORMAat:DATA ASCii
         * result in dBuV, keep only integer part
         */
        sscanf(buf, "%d", &val->i);
        val->i -= 34;
        break;

    case RIG_LEVEL_ATT:
        val->i = (!memcmp(buf, "ON", 2)
                  || !memcmp(buf, "1", 1)) ? rig->state.attenuator[0] : 0;
        break;

    case RIG_LEVEL_AF:
        if (num_sscanf(buf, "%f", &val->f) != 1)
        {
            return -RIG_EPROTO;
        }

        break;

    default:
        return -RIG_EINVAL;
    }

    return retval;
}

const char *rs_get_info(RIG *rig)
{
    static char infobuf[128];
    int info_len, retval;

#define ID_QUERY BOM "*IDN?" EOM

    retval = rs_transaction(rig, ID_QUERY, strlen(ID_QUERY), infobuf, &info_len);

    if (retval < 0)
    {
        return NULL;
    }

    return infobuf;
}

int rs_reset(RIG *rig, reset_t reset)
{
    int retval;

#define RST_CMD BOM "*RST" EOM

    retval = rs_transaction(rig, RST_CMD, strlen(RST_CMD), NULL, NULL);

    return retval;
}


/*
 * initrigs_rs is called by rig_backend_load
 */
DECLARE_INITRIG_BACKEND(rs)
{
    rig_debug(RIG_DEBUG_VERBOSE, "%s: _init called\n", __func__);

    rig_register(&esmc_caps);
    rig_register(&eb200_caps);
    rig_register(&xk2100_caps);
    rig_register(&ek89x_caps);

    return RIG_OK;
}

