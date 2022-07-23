/*
 *  Hamlib Lowe backend - main file
 *  Copyright (c) 2003-2005 by Stephane Fillod
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
#include <unistd.h>  /* UNIX standard function definitions */

#include "hamlib/rig.h"
#include "serial.h"
#include "misc.h"
#include "register.h"

#include "lowe.h"


#define BUFSZ 64

#define CR "\x0d"
#define EOM CR

#define MD_USB  "USB"
#define MD_LSB  "LSB"
#define MD_FAX  "FAX"
#define MD_CW   "CW"
#define MD_FM   "FM"
#define MD_AM   "AM"
#define MD_AMS  "AMS"


/*
 * lowe_transaction
 * We assume that rig!=NULL, rig->state!= NULL, data!=NULL, data_len!=NULL
 */
int lowe_transaction(RIG *rig, const char *cmd, int cmd_len, char *data,
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


    /* no data expected, TODO: flush input? */
    if (!data || !data_len)
    {
        return 0;
    }

    retval = read_string(&rs->rigport, (unsigned char *) data, BUFSZ, CR, 1, 0, 1);

    if (retval == -RIG_ETIMEOUT)
    {
        retval = 0;
    }

    if (retval < 0)
    {
        return retval;
    }

    *data_len = retval;

    return RIG_OK;
}

/*
 * lowe_set_freq
 * Assumes rig!=NULL
 */
int lowe_set_freq(RIG *rig, vfo_t vfo, freq_t freq)
{
    char freqbuf[16], ackbuf[16];
    int ack_len, retval;

    /*
     */
    SNPRINTF(freqbuf, sizeof(freqbuf), "FRQ%f" EOM, (float)freq / 1000);
    retval = lowe_transaction(rig, freqbuf, strlen(freqbuf), ackbuf, &ack_len);

    return retval;
}

/*
 * lowe_get_freq
 * Assumes rig!=NULL
 */
int lowe_get_freq(RIG *rig, vfo_t vfo, freq_t *freq)
{
    char freqbuf[16];
    int freq_len, retval;
    float f_freq;

    retval = lowe_transaction(rig, "FRQ?" EOM, 5, freqbuf, &freq_len);

    if (retval != RIG_OK)
    {
        return retval;
    }

    freqbuf[freq_len < 16 ? freq_len : 15] = '\0';

    sscanf(freqbuf + 1, "%f", &f_freq);
    *freq = f_freq * 1000;

    return retval;
}


/*
 * lowe_set_mode
 * Assumes rig!=NULL
 */
int lowe_set_mode(RIG *rig, vfo_t vfo, rmode_t mode, pbwidth_t width)
{
    char mdbuf[16], ackbuf[16];
    char *mode_sel;
    int ack_len, retval;

    switch (mode)
    {
    case RIG_MODE_CW:       mode_sel = MD_CW; break;

    case RIG_MODE_USB:      mode_sel = MD_USB; break;

    case RIG_MODE_LSB:      mode_sel = MD_LSB; break;

    case RIG_MODE_FM:       mode_sel = MD_FM; break;

    case RIG_MODE_AM:       mode_sel = MD_AM; break;

    case RIG_MODE_FAX:     mode_sel = MD_FAX; break;

    case RIG_MODE_AMS:     mode_sel = MD_AMS; break;

    default:
        rig_debug(RIG_DEBUG_ERR, "%s: unsupported mode %s\n", __func__,
                  rig_strrmode(mode));
        return -RIG_EINVAL;
    }

    SNPRINTF(mdbuf, sizeof(mdbuf), "MOD%s" EOM, mode_sel);
    retval = lowe_transaction(rig, mdbuf, strlen(mdbuf), ackbuf, &ack_len);

    return retval;
}


/*
 * lowe_get_mode
 * Assumes rig!=NULL
 */
int lowe_get_mode(RIG *rig, vfo_t vfo, rmode_t *mode, pbwidth_t *width)
{
    char mdbuf[16];
    int mdbuf_len, retval;

    retval = lowe_transaction(rig, "MOD?" EOM, 5, mdbuf, &mdbuf_len);

    if (retval != RIG_OK)
    {
        return retval;
    }

    if (!strcmp(mdbuf + 1, MD_CW))
    {
        *mode = RIG_MODE_CW;
    }
    else if (!strcmp(mdbuf + 1, MD_USB))
    {
        *mode = RIG_MODE_USB;
    }
    else if (!strcmp(mdbuf + 1, MD_LSB))
    {
        *mode = RIG_MODE_LSB;
    }
    else if (!strcmp(mdbuf + 1, MD_FM))
    {
        *mode = RIG_MODE_FM;
    }
    else if (!strcmp(mdbuf + 1, MD_FAX))
    {
        *mode = RIG_MODE_FAX;
    }
    else if (!strcmp(mdbuf + 1, MD_AMS))
    {
        *mode = RIG_MODE_AMS;
    }
    else if (!strcmp(mdbuf + 1, MD_AM))
    {
        *mode = RIG_MODE_AM;
    }
    else
    {
        rig_debug(RIG_DEBUG_WARN, "%s: unknown mode '%s'\n",
                  __func__, mdbuf);
        return -RIG_EPROTO;
    }

    *width = RIG_PASSBAND_NORMAL;

    return retval;
}

/*
 * lowe_get_level
 * Assumes rig!=NULL
 */
int lowe_get_level(RIG *rig, vfo_t vfo, setting_t level, value_t *val)
{
    char lvlbuf[16];
    int lvl_len, retval;

    if (level != RIG_LEVEL_STRENGTH)
    {
        return -RIG_EINVAL;
    }

    retval = lowe_transaction(rig, "RSS?" EOM, 5, lvlbuf, &lvl_len);

    if (retval != RIG_OK)
    {
        return retval;
    }

    lvlbuf[lvl_len < 16 ? lvl_len : 15] = '\0';

    sscanf(lvlbuf + 1, "%d", &val->i);

    val->i += 60;   /* dBm */

    return retval;
}


/*
 * lowe_reset
 * Assumes rig!=NULL
 */
int lowe_reset(RIG *rig, reset_t reset)
{
    static char ackbuf[BUFSZ];
    int retval, ack_len;

    retval = lowe_transaction(rig, "RES" EOM, 4, ackbuf, &ack_len);

    return retval;
}


/*
 * lowe_get_info
 * Assumes rig!=NULL
 */
const char *lowe_get_info(RIG *rig)
{
    static char idbuf[BUFSZ];
    int retval, id_len;

    /* hack: no idea what INF is for */
    retval = lowe_transaction(rig, "INF?" EOM, 5, idbuf, &id_len);

    if (retval != RIG_OK)
    {
        rig_debug(RIG_DEBUG_VERBOSE, "%s: INF didn't work\n", __func__);
        // non-fatal
    }

    /* this is the real one */
    retval = lowe_transaction(rig, "TYP?" EOM, 5, idbuf, &id_len);

    if (retval != RIG_OK)
    {
        return NULL;
    }

    idbuf[id_len] = '\0';

    return idbuf;
}

/*
 * probe_lowe(port_t *port, rig_probe_func_t cfunc, rig_ptr_t data)
 */
DECLARE_PROBERIG_BACKEND(lowe)
{
    static char idbuf[BUFSZ];
    int retval, id_len;

    if (!port)
    {
        return RIG_MODEL_NONE;
    }

    if (port->type.rig != RIG_PORT_SERIAL)
    {
        return RIG_MODEL_NONE;
    }

    port->parm.serial.rate = hf235_caps.serial_rate_max;
    port->write_delay = port->post_write_delay = 0;
    port->timeout = 50;
    port->retry = 1;

    retval = serial_open(port);

    if (retval != RIG_OK)
    {
        return RIG_MODEL_NONE;
    }

    retval = write_block(port, (unsigned char *) "TYP?" EOM, 4);
    id_len = read_string(port, (unsigned char *) idbuf, BUFSZ, CR, 2, 0, 1);

    close(port->fd);

    if (retval != RIG_OK || id_len <= 0 || id_len >= BUFSZ)
    {
        return RIG_MODEL_NONE;
    }

    idbuf[id_len] = '\0';

    if (!strcmp(idbuf, "HF-235"))
    {
        if (cfunc)
        {
            (*cfunc)(port, RIG_MODEL_HF235, data);
        }

        return RIG_MODEL_HF235;
    }

    /*
     * not found...
     */
    if (memcmp(idbuf, "ID" EOM, 3)) /* catch loopback serial */
        rig_debug(RIG_DEBUG_VERBOSE, "probe_lowe: found unknown device "
                  "with ID '%s', please report to Hamlib "
                  "developers.\n", idbuf);

    return RIG_MODEL_NONE;
}


/*
 * initrigs_lowe is called by rig_backend_load
 */
DECLARE_INITRIG_BACKEND(lowe)
{
    rig_debug(RIG_DEBUG_VERBOSE, "%s: _init called\n", __func__);

    rig_register(&hf235_caps);

    return RIG_OK;
}

