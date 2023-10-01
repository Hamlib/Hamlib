/*
 *  Hamlib MDS 4710/9710 backend - main file
 *  Copyright (c) 2022 by Michael Black W9MDB
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

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <hamlib/rig.h>
#include "serial.h"
#include "misc.h"
#include "register.h"

#include "mds.h"

#define MAXCMDLEN 32

extern const struct rig_caps mds_4710_caps;
extern const struct rig_caps mds_9710_caps;

DECLARE_INITRIG_BACKEND(mds)
{
    rig_debug(RIG_DEBUG_VERBOSE, "%s: _init called\n", __func__);

    rig_register(&mds_4710_caps);
    rig_register(&mds_9710_caps);
    rig_debug(RIG_DEBUG_VERBOSE, "%s: _init back from rig_register\n", __func__);

    return RIG_OK;
}

int mds_transaction(RIG *rig, char *cmd, int expected, char **result)
{
    char cmd_buf[MAXCMDLEN];
    int retval;
    struct rig_state *rs = &rig->state;
    struct mds_priv_data *priv = rig->state.priv;

    rig_debug(RIG_DEBUG_VERBOSE, "%s: cmd=%s\n", __func__, cmd);

    SNPRINTF(cmd_buf, sizeof(cmd_buf), "%s\n", cmd);

    rig_flush(&rs->rigport);
    retval = write_block(&rs->rigport, (unsigned char *) cmd_buf, strlen(cmd_buf));

    if (retval < 0)
    {
        return retval;
    }

    if (expected == 0)
    {
        return RIG_OK;
    }
    else
    {
        char cmdtrm_str[2];   /* Default Command/Reply termination char */
        cmdtrm_str[0] = 0x0d;
        cmdtrm_str[1] = 0x00;
        retval = read_string(&rs->rigport, (unsigned char *) priv->ret_data,
                             sizeof(priv->ret_data), cmdtrm_str, strlen(cmdtrm_str), 0, expected);

        if (retval < 0)
        {
            rig_debug(RIG_DEBUG_ERR, "%s(%d): error in read_block\n", __func__, __LINE__);
            return retval;
        }
    }

    if (result != NULL)
    {
        rig_debug(RIG_DEBUG_VERBOSE, "%s: setting result\n", __func__);
        *result = &(priv->ret_data[0]);
    }
    else
    {
        rig_debug(RIG_DEBUG_VERBOSE, "%s: no result requested\n", __func__);
    }

    return RIG_OK;
}

int mds_init(RIG *rig)
{
    rig_debug(RIG_DEBUG_VERBOSE, "%s version %s\n", __func__, rig->caps->version);
    // cppcheck claims leak here but it's freed in cleanup
    rig->state.priv = (struct mds_priv_data *)calloc(1,
                      sizeof(struct mds_priv_data));

    if (!rig->state.priv)
    {
        return -RIG_ENOMEM;
    }

    return RIG_OK;
}

/*
 * mds_cleanup
 *
 */

int mds_cleanup(RIG *rig)
{

    rig_debug(RIG_DEBUG_VERBOSE, "%s called\n", __func__);

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
 * mds_get_freq
 * Assumes rig!=NULL, rig->state.priv!=NULL, freq!=NULL
 */
int mds_get_freq(RIG *rig, vfo_t vfo, freq_t *freq)
{
    int retval;
    char *response = NULL;

    rig_debug(RIG_DEBUG_VERBOSE, "%s: vfo=%s\n", __func__, rig_strvfo(vfo));
    *freq = 0;

    retval = mds_transaction(rig, "TX", 16, &response);

    if (retval != RIG_OK)
    {
        rig_debug(RIG_DEBUG_ERR, "%s: invalid response=%s\n", __func__, response);
        return retval;
    }

    retval = sscanf(response, "%lg", freq);

    if (retval != 1)
    {
        rig_debug(RIG_DEBUG_ERR, "%s: Unable to parse response\n", __func__);
        return -RIG_EPROTO;
    }

    return RIG_OK;
}

// TC command does not work on 4050 -- not implemented as of 2022-01-12
/*
 * mds_set_freq
 * assumes rig!=NULL, rig->state.priv!=NULL
 */
int mds_set_freq(RIG *rig, vfo_t vfo, freq_t freq)
{
    int retval;
    freq_t tfreq;

    rig_debug(RIG_DEBUG_VERBOSE, "%s: vfo=%s freq=%.0f\n", __func__,
              rig_strvfo(vfo), freq);

    retval = rig_get_freq(rig, vfo, &tfreq);

    if (retval != RIG_OK)
    {
        rig_debug(RIG_DEBUG_VERBOSE, "%s: get_freq failed: %s\n", __func__,
                  strerror(retval));
        return retval;
    }

    if (tfreq == freq)
    {
        rig_debug(RIG_DEBUG_VERBOSE, "%s: freq not changing\n", __func__);
        return RIG_OK;
    }

    // If we are not explicitly asking for VFO_B then we'll set the receive side also
    if (vfo != RIG_VFO_B)
    {
        char cmd_buf[MAXCMDLEN];
        char *response = NULL;
        SNPRINTF((char *) cmd_buf, sizeof(cmd_buf), "TX%.4f", freq / 1e6);
        retval = mds_transaction(rig, cmd_buf, 0, &response);

        if (retval < 0)
        {
            rig_debug(RIG_DEBUG_ERR, "%s: TX failed\n", __func__);
            return retval;
        }

        SNPRINTF((char *) cmd_buf, sizeof(cmd_buf), "RX%.4f", freq / 1e6);
        retval = mds_transaction(rig, cmd_buf, 0, &response);

        if (retval < 0)
        {
            rig_debug(RIG_DEBUG_ERR, "%s: RX failed\n", __func__);
            return retval;
        }
    }

    return RIG_OK;
}

/*
 * mds_set_ptt
 * Assumes rig!=NULL
 */
int mds_set_ptt(RIG *rig, vfo_t vfo, ptt_t ptt)
{
    int retval;
    char cmd_buf[MAXCMDLEN];
    char *response = NULL;

    rig_debug(RIG_DEBUG_VERBOSE, "%s: ptt=%d\n", __func__, ptt);

    SNPRINTF(cmd_buf, sizeof(cmd_buf), "%s", ptt ? "KEY" : "DKEY");
    retval = mds_transaction(rig, cmd_buf, 0, &response);

    if (retval < 0)
    {
        rig_debug(RIG_DEBUG_ERR, "%s: invalid response=%s\n", __func__, response);
        return retval;
    }

    if (strncmp(response, "OK", 2) != 0)
    {
        rig_debug(RIG_DEBUG_ERR, "%s: Expected OK, got '%s'\n", __func__, response);
        return -RIG_EINVAL;
    }

    rig_debug(RIG_DEBUG_VERBOSE, "%s: cmd:IP result=%s\n", __func__, response);

    return RIG_OK;
}

/*
 * mds_get_ptt
 * Assumes rig!=NULL
 */
int mds_get_ptt(RIG *rig, vfo_t vfo, ptt_t *ptt)
{
    int retval;
    char *response = NULL;
    char c;

    rig_debug(RIG_DEBUG_VERBOSE, "%s: vfo=%s\n", __func__, rig_strvfo(vfo));

    retval = mds_transaction(rig, "IP", 0, &response);

    if (retval != RIG_OK)
    {
        rig_debug(RIG_DEBUG_ERR, "%s: error response?='%s'\n", __func__, response);
        return retval;
    }

    c = response[0];

    if (c == '1' || c == '0')
    {
        *ptt = c - '0';
    }
    else
    {
        rig_debug(RIG_DEBUG_ERR, "%s: error response='%s'\n", __func__, response);
        return -RIG_EPROTO;
    }

    return RIG_OK;
}

/*
 * mds_set_mode
 * Assumes rig!=NULL
 * Note that 2050 does not have set or get width
 */
int mds_set_mode(RIG *rig, vfo_t vfo, rmode_t mode, pbwidth_t width)
{
    char cmd_buf[32], ttmode;
    int retval;
    rmode_t tmode;
    pbwidth_t twidth;

    //struct tt588_priv_data *priv = (struct tt588_priv_data *) rig->state.priv;

    rig_debug(RIG_DEBUG_VERBOSE, "%s: vfo=%s mode=%s width=%d\n", __func__,
              rig_strvfo(vfo), rig_strrmode(mode), (int)width);

    retval = rig_get_mode(rig, vfo, &tmode, &twidth);

    if (retval != RIG_OK)
    {
        rig_debug(RIG_DEBUG_ERR, "%s: get_mode failed %s\n", __func__,
                  strerror(retval));
    }

    if (tmode == mode)
    {
        rig_debug(RIG_DEBUG_VERBOSE, "%s: already mode %s so not changing\n", __func__,
                  rig_strrmode(mode));
        return RIG_OK;
    }

    switch (mode)
    {
    case RIG_MODE_USB:
        ttmode = 'U';
        break;

    case RIG_MODE_LSB:
        ttmode = 'L';
        break;

    case RIG_MODE_CW:
        ttmode = 'C';
        break;

    case RIG_MODE_AM:
        ttmode = 'A';
        break;

    case RIG_MODE_RTTY:
        ttmode = 'F';
        break;

    default:
        rig_debug(RIG_DEBUG_ERR, "%s: unsupported mode %s\n", __func__,
                  rig_strrmode(mode));
        return -RIG_EINVAL;
    }

    SNPRINTF((char *) cmd_buf, sizeof(cmd_buf), "TB%c\n", ttmode);

    retval = mds_transaction(rig, cmd_buf, 0, NULL);

    if (retval < 0)
    {
        return retval;
    }

    return RIG_OK;
}

/*
 * mds_get_mode
 * Assumes rig!=NULL
 * Note that 2050 does not have set or get width
 */
int mds_get_mode(RIG *rig, vfo_t vfo, rmode_t *mode, pbwidth_t *width)
{
    char *result = NULL;
    int retval;

    rig_debug(RIG_DEBUG_VERBOSE, "%s: vfo=%s\n", __func__, rig_strvfo(vfo));

    retval = mds_transaction(rig, "IB", 0, &result);

    if (retval != RIG_OK)
    {
        rig_debug(RIG_DEBUG_ERR, "%s: bad response=%s\n", __func__, result);
        return retval;
    }

    //dump_hex((unsigned char *)result,strlen(result));
    switch (result[1])
    {
    case 'L':
        *mode = RIG_MODE_LSB;
        break;

    case 'U':
        *mode = RIG_MODE_USB;
        break;

    case 'A':
        *mode = RIG_MODE_AM;
        break;

    case 'F':
        *mode = RIG_MODE_RTTY;
        break;

    case 'C':
        *mode = RIG_MODE_CW;
        break;

    default:
        rig_debug(RIG_DEBUG_ERR, "%s: Unknown mode='%c%c'\n", __func__,  result[0],
                  result[1]);
        return -RIG_EPROTO;
    }

    *width = 3000; // we'll default this to 3000 for now
    rig_debug(RIG_DEBUG_VERBOSE, "%s: vfo=%s mode=%s width=%d\n", __func__,
              rig_strvfo(vfo), rig_strrmode(*mode), (int)*width);

    return RIG_OK;
}

#if 0
int mds_get_vfo(RIG *rig, vfo_t *vfo)
{
    *vfo = RIG_VFO_A;

    if (check_vfo(*vfo) == FALSE)
    {
        rig_debug(RIG_DEBUG_ERR, "%s: unsupported VFO %s\n", __func__,
                  rig_strvfo(*vfo));
        return -RIG_EINVAL;
    }

    rig_debug(RIG_DEBUG_VERBOSE, "%s: vfo=%s\n", __func__, rig_strvfo(*vfo));

    return RIG_OK;
}
#endif

/*
 * mds_get_level
 */
int mds_get_level(RIG *rig, vfo_t vfo, setting_t level, value_t *val)
{
    int retval = 0;
    char *response = NULL;

    switch (level)
    {
        int strength;
        int n;

    case RIG_LEVEL_STRENGTH:
        retval = mds_transaction(rig, "IAL", 0, &response);

        if (retval < 0)
        {
            rig_debug(RIG_DEBUG_ERR, "%s: invalid response=%s\n", __func__,
                      response);
            return retval;
        }

        n = sscanf(response, "%2d", &strength);

        if (n == 1)
        {
            val->i = strength;
        }
        else
        {
            rig_debug(RIG_DEBUG_ERR, "%s: unable to parse STRENGTH from %s\n",
                      __func__, response);
            return -RIG_EPROTO;
        }

        break;

    default:
        rig_debug(RIG_DEBUG_ERR, "%s: unsupported level %s\n", __func__,
                  rig_strlevel(level));
        return -RIG_EINVAL;
    }

    rig_debug(RIG_DEBUG_VERBOSE, "%s: vfo=%s level=%s val=%s\n", __func__,
              rig_strvfo(vfo), rig_strlevel(level), response);

    return RIG_OK;
}

/*
 * mds_get_info
 */
const char *mds_get_info(RIG *rig)
{
    char *response = NULL;
    int retval;

    rig_debug(RIG_DEBUG_VERBOSE, "%s called\n", __func__);

    retval = mds_transaction(rig, "MODEL", 16, &response);

    if (retval != RIG_OK)
    {
        rig_debug(RIG_DEBUG_WARN, "%s: MODEL command failed: %s\n", __func__,
                  strerror(retval));
    }
    else
    {
        rig_debug(RIG_DEBUG_VERBOSE, "Model: %s\n", response);
    }

    response = NULL;
    retval = mds_transaction(rig, "SER", 16, &response);

    if (retval != RIG_OK)
    {
        rig_debug(RIG_DEBUG_WARN, "%s: SER command failed: %s\n", __func__,
                  strerror(retval));
    }
    else
    {
        rig_debug(RIG_DEBUG_VERBOSE, "Serial# %s\n", response);
    }

    response = NULL;
    retval = mds_transaction(rig, "SREV", 16, &response);

    if (retval != RIG_OK)
    {
        rig_debug(RIG_DEBUG_WARN, "%s: SREV command failed: %s\n", __func__,
                  strerror(retval));
    }
    else
    {
        rig_debug(RIG_DEBUG_VERBOSE, "Firmware %s\n", response);
    }

    response = NULL;
    retval = mds_transaction(rig, "SHOW DC", 16, &response);

    if (retval != RIG_OK)
    {
        rig_debug(RIG_DEBUG_ERR, "%s: SHOW DC failed result=%s\n", __func__, response);
    }
    else
    {
        rig_debug(RIG_DEBUG_VERBOSE, "DC %s\n", response);
    }

    return response;
}

int mds_open(RIG *rig)
{
    char *response = NULL;
    int retval;

    ENTERFUNC;
    mds_get_info(rig);
    retval = mds_transaction(rig, "MODEM NONE", 0, &response);
    retval = mds_transaction(rig, "PTT 0", 0, &response);
    RETURNFUNC(retval);
}
