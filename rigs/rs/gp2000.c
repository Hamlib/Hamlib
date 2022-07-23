/*
 *  Hamlib R&S GP2000 backend - main file
 *  Reused from rs.c
 *  Copyright (c) 2018 by Michael Black W9MDB
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

/*
 * Looks like the GP2000 could be reused in other rigs so
 * we implement that and then the XK2100 uses this interface
 */
#include <hamlib/config.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>     /* String function definitions */

#include "hamlib/rig.h"
#include "serial.h"
#include "misc.h"
#include "register.h"
#include "num_stdio.h"

#include "gp2000.h"

#define RESPSZ 64

#define LF "\x0a"
#define CR "\x0d"
#define BOM LF
#define EOM CR

/*
 * R&S GB2 protocol ?
 */


/*
 * gp2000
 * We assume that rig!=NULL, rig->state!= NULL, data!=NULL, data_len!=NULL
 */
int
gp2000_transaction(RIG *rig, const char *cmd, int cmd_len, char *data,
                   int *data_len)
{
    int retval;
    struct rig_state *rs;

    rig_debug(RIG_DEBUG_VERBOSE, "%s: len=%d,cmd=%s\n", __func__, cmd_len,
              cmd);

    rs = &rig->state;

    rig_flush(&rs->rigport);

    rig_debug(RIG_DEBUG_VERBOSE, "gp2000_transaction: len=%d,cmd=%s\n",
              cmd_len, cmd);
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

    retval = read_string(&rs->rigport, (unsigned char *) data, RESPSZ,
                         CR, 1, 0, 1);

    if (retval < 0)
    {
        return retval;
    }

    *data_len = retval;

    return RIG_OK;
}

/*
 * gp2000_set_freq
 * Assumes rig!=NULL
 */
int
gp2000_set_freq(RIG *rig, vfo_t vfo, freq_t freq)
{
    char freqbuf[32];
    int retval;
    // cppcheck-suppress *
    char *fmt = BOM "F%" PRIll ",%" PRIll EOM;

    rig_debug(RIG_DEBUG_VERBOSE, "%s: vfo=%s,freq=%.0f\n", __func__,
              rig_strvfo(vfo), freq);

    SNPRINTF(freqbuf, sizeof(freqbuf), fmt, (int64_t) freq, (int64_t) freq);
    retval = gp2000_transaction(rig, freqbuf, strlen(freqbuf), NULL, NULL);

    return retval;
}

/*
 * gp2000_get_freq
 * Assumes rig!=NULL
 */
int
gp2000_get_freq(RIG *rig, vfo_t vfo, freq_t *freq)
{
    char buf[RESPSZ];
    int len, retval;

    rig_debug(RIG_DEBUG_VERBOSE, "%s: vfo=%s\n", __func__, rig_strvfo(vfo));

#define FREQ_QUERY  BOM "F?" EOM

    retval =
        gp2000_transaction(rig, FREQ_QUERY, strlen(FREQ_QUERY), buf, &len);

    if (retval < 0)
    {
        return retval;
    }

    retval = (sscanf(buf, "%*cF%" SCNfreq, freq) == 1) ? RIG_OK : -RIG_EPROTO;

    return retval;
}

/*
 * gp2000_set_mode
 * Assumes rig!=NULL
 */
int
gp2000_set_mode(RIG *rig, vfo_t vfo, rmode_t mode, pbwidth_t width)
{
    char buf[32], *smode;
    int retval;

    rig_debug(RIG_DEBUG_VERBOSE, "%s: vfo=%s, mode=%s, width=%d\n", __func__,
              rig_strvfo(vfo), rig_strvfo(mode), (int)width);

    switch (mode)
    {
    case RIG_MODE_AM:
        smode = "1";
        break;

    case RIG_MODE_USB:
        smode = "2";
        break;

    case RIG_MODE_LSB:
        smode = "3";
        break;

    case RIG_MODE_CW:
        smode = "5";
        break;

    case RIG_MODE_FM:
        smode = "9";
        break;

    case RIG_MODE_PKTUSB:
        smode = "13"; // use the 2700 bandwidth for packet
        break;

    case RIG_MODE_PKTLSB:
        smode = "14"; // use the 2700 bandwidth for packet
        break;

    default:
        return -RIG_EINVAL;
    }

    SNPRINTF(buf, sizeof(buf), BOM "I%s" EOM, smode);
    retval = gp2000_transaction(rig, buf, strlen(buf), NULL, NULL);

    if (retval < 0)
    {
        return retval;
    }

    if (width == RIG_PASSBAND_NOCHANGE)
    {
        return retval;
    }

    if (width == RIG_PASSBAND_NORMAL)
    {
        width = rig_passband_normal(rig, mode);
    }

    if (width > 0)
    {
        SNPRINTF(buf, sizeof(buf), BOM "W%d" EOM, (int) width);
        retval = gp2000_transaction(rig, buf, strlen(buf), NULL, NULL);
    }

    return retval;
}

/*
 * gp2000_get_mode
 * Assumes rig!=NULL
 */
int
gp2000_get_mode(RIG *rig, vfo_t vfo, rmode_t *mode, pbwidth_t *width)
{
    char buf[RESPSZ];
    int buf_len, retval;
    int nmode;
    char *pmode = "UNKNOWN";
    int n;


    rig_debug(RIG_DEBUG_VERBOSE, "%s: vfo=%s\n", __func__, rig_strvfo(vfo));

#define DEM_QUERY   BOM "I?" EOM

    retval =
        gp2000_transaction(rig, DEM_QUERY, strlen(DEM_QUERY), buf, &buf_len);

    if (retval < 0)
    {
        return retval;
    }

    n = sscanf(buf, "%*cI%d", &nmode);

    if (n != 1)
    {
        rig_debug(RIG_DEBUG_ERR, "%s: unable to parse mode from '%s'\n", __func__, buf);
        return -RIG_EPROTO;
    }

    switch (nmode)
    {
    case 1: pmode = "AM"; break;

    case 2: pmode = "USB"; break;

    case 3: pmode = "LSB"; break;

    case 5: pmode = "CW"; break;

    case 9: pmode = "FM"; break;

    case 13: pmode = "PKTUSB"; break;

    case 14: pmode = "PKTLSB"; break;
    }

    *mode = rig_parse_mode(pmode);

#define BAND_QUERY   BOM "W?" EOM
    retval =
        gp2000_transaction(rig, BAND_QUERY, strlen(BAND_QUERY), buf, &buf_len);

    if (retval < 0)
    {
        return retval;
    }

    *width = atoi(&buf[2]);

    return retval;
}


#ifdef XXREMOVEDXX
// Not referenced anywhere
int
gp2000_set_func(RIG *rig, vfo_t vfo, setting_t func, int status)
{
    char buf[32], *sfunc;
    int len, retval;

    rig_debug(RIG_DEBUG_VERBOSE, "%s: vfo=%s\n", __func__, rig_strvfo(vfo));

    switch (func)
    {
    case RIG_FUNC_SQL:
        sfunc = "SQ00";
        break;

    default:
        return -RIG_EINVAL;
    }

    SNPRINTF(buf, sizeof(buf), BOM "%s %s" EOM, sfunc, status ? "1" : "0");
    retval = gp2000_transaction(rig, buf, strlen(buf), NULL, NULL);

    return retval;
}
#endif

#ifdef XXREMOVEDXX
// Not referenced anywhere
int
gp2000_get_func(RIG *rig, vfo_t vfo, setting_t func, int *status)
{
    char buf[RESPSZ], *sfunc;
    int buf_len, retval;

    rig_debug(RIG_DEBUG_VERBOSE, "%s: vfo=%s\n", __func__, rig_strvfo(vfo));

    switch (func)
    {
    case RIG_FUNC_SQL:
        sfunc = BOM "SQ00?" EOM;
        break;

    default:
        return -RIG_EINVAL;
    }

    retval = gp2000_transaction(rig, sfunc, strlen(sfunc), buf, &buf_len);

    if (retval < 0)
    {
        return retval;
    }

    // we expected LF+"X" where X is the status
    *status = buf[2] == 1 ? 1 : 0;

    return retval;
}
#endif

int
gp2000_set_level(RIG *rig, vfo_t vfo, setting_t level, value_t val)
{
    char buf[64];
    int retval;

    rig_debug(RIG_DEBUG_VERBOSE, "%s: vfo=%s\n", __func__, rig_strvfo(vfo));

    switch (level)
    {
    case RIG_LEVEL_AF:
        SNPRINTF(buf, sizeof(buf), BOM "SR%02d" EOM, (int)val.f);
        break;

    case RIG_LEVEL_SQL:
        SNPRINTF(buf, sizeof(buf), BOM "SQ%1d" EOM, (int)val.f);
        break;

    case RIG_LEVEL_AGC:
    case RIG_LEVEL_RF:
        return -RIG_ENIMPL;

    default:
        return -RIG_EINVAL;
    }

    retval = gp2000_transaction(rig, buf, strlen(buf), NULL, NULL);

    return retval;
}

int
gp2000_get_level(RIG *rig, vfo_t vfo, setting_t level, value_t *val)
{
    char buf[RESPSZ], *slevel;
    int buf_len, retval, ival;

    rig_debug(RIG_DEBUG_VERBOSE, "%s: vfo=%s\n", __func__, rig_strvfo(vfo));

    switch (level)
    {
    case RIG_LEVEL_AF:
        slevel = BOM "SL?" EOM;
        break;

    case RIG_LEVEL_SQL:
        slevel = BOM "SQ?" EOM;
        break;

    case RIG_LEVEL_STRENGTH:
    case RIG_LEVEL_ATT:
    case RIG_LEVEL_AGC:
    case RIG_LEVEL_RF:
        return -RIG_ENIMPL;

    default:
        return -RIG_EINVAL;
    }

    retval = gp2000_transaction(rig, slevel, strlen(slevel), buf, &buf_len);

    if (retval < 0)
    {
        return retval;
    }

    switch (level)
    {
    case RIG_LEVEL_AF:
        if (num_sscanf(buf, "%*cSL%d", &ival) != 1)
        {
            return -RIG_EPROTO;
        }

        val->f = ival;

        break;

    case RIG_LEVEL_SQL:
        if (num_sscanf(buf, "%*cSQ%1d", &ival) != 1)
        {
            return -RIG_EPROTO;
        }

        val->f = ival;

        break;

    default:
        return -RIG_EINVAL;
    }

    return retval;
}

const char *
gp2000_get_info(RIG *rig)
{
    static char infobuf[128];
    int info_len, retval;
    int addr = -1;
    char type[32] = "unk type";
    char rigid[32] = "unk rigid";
    char sernum[32] = "unk sernum";
    char *p;


    rig_debug(RIG_DEBUG_VERBOSE, "%s\n", __func__);

#define ID_QUERY BOM "IDENT?" EOM

    retval =
        gp2000_transaction(rig, ID_QUERY, strlen(ID_QUERY), infobuf, &info_len);

    if (retval < 0)
    {
        return NULL;
    }

    p = strtok(infobuf, ",");

    while (p)
    {
        switch (p[0])
        {
        case 0x0a:
            sscanf(p, "%*cIDENT%31s", type);
            break;

        case 'i':
            sscanf(p, "id%31s", rigid);
            break;

        case 's':
            sscanf(p, "sn%31s", sernum);
            break;

        default:
            printf("Unknown response: %s\n", p);

        }

        p = strtok(NULL, ",");
    }

    SNPRINTF(infobuf, sizeof(infobuf), "ADDR=%02d\nTYPE=%s\nSER#=%s\nID  =%s\n",
             addr, type, sernum, rigid);

    return infobuf;
}

int
gp2000_set_ptt(RIG *rig, vfo_t vfo, ptt_t ptt)
{
    int retval = 0;
    char cmd[32];

    rig_debug(RIG_DEBUG_VERBOSE, "%s: vfo=%s\n", __func__, rig_strvfo(vfo));

    SNPRINTF(cmd, sizeof(cmd), "X%1d", ptt);
    retval = gp2000_transaction(rig, cmd, strlen(cmd), NULL, NULL);
    return retval;
}

int
gp2000_get_ptt(RIG *rig, vfo_t vfo, ptt_t *ptt)
{
    int retval = 0;
    int len;
    char buf[RESPSZ];
    char *cmd = BOM "X?" EOM;

    rig_debug(RIG_DEBUG_VERBOSE, "%s: vfo=%s\n", __func__, rig_strvfo(vfo));

    retval = gp2000_transaction(rig, cmd, strlen(cmd), buf, &len);

    if (retval < 0)
    {
        return retval;
    }

    retval = (sscanf(buf, "%*cX%1u", ptt) == 1) ? RIG_OK : -RIG_EPROTO;
    return retval;
}

