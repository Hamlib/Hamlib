/*
 *  Hamlib backend - SmartSDR TCP on port 4952
 *  Copyright (c) 2024 by Michael Black W9MDB
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

#include <stdint.h>
#include <stdlib.h>
#include <math.h>

#include "hamlib/rig.h"
#include "parallel.h"
#include "misc.h"
#include "bandplan.h"
#include "cache.h"

static int smartsdr_set_freq(RIG *rig, vfo_t vfo, freq_t freq);
static int smartsdr_get_freq(RIG *rig, vfo_t vfo, freq_t *freq);
//static int smartsdr_reset(RIG *rig, reset_t reset);
static int smartsdr_init(RIG *rig);
static int smartsdr_open(RIG *rig);
static int smartsdr_close(RIG *rig);
static int smartsdr_cleanup(RIG *rig);
static int smartsdr_set_ptt(RIG *rig, vfo_t vfo, ptt_t ptt);
//static int smartsdr_set_level(RIG *rig, vfo_t vfo, setting_t level, value_t val);

struct smartsdr_priv_data
{
    int slice; // slice 0-7 maps to A-H
    int seqnum;
    int ptt;
};


#define DEFAULTPATH "127.0.0.1:4952"

#define SMARTSDR_FUNC  RIG_FUNC_MUTE
#define SMARTSDR_LEVEL RIG_LEVEL_PREAMP
#define SMARTSDR_PARM  RIG_PARM_NONE

#define SMARTSDR_MODES (RIG_MODE_USB|RIG_MODE_LSB|RIG_MODE_PKTUSB|RIG_MODE_PKTLSB|RIG_MODE_CW|RIG_MODE_AM|RIG_MODE_FM|RIG_MODE_FMN)

#define SMARTSDR_VFO (RIG_VFO_A|RIG_VFO_B)

#define SMARTSDR_ANTS 3


struct rig_caps smartsdr_a_rig_caps =
{
    RIG_MODEL(RIG_MODEL_SMARTSDR_A),
    .model_name =     "SmartSDR Slice A",
#include "smartsdr_caps.c"
};

struct rig_caps smartsdr_b_rig_caps =
{
    RIG_MODEL(RIG_MODEL_SMARTSDR_B),
    .model_name =     "SmartSDR Slice B",
#include "smartsdr_caps.c"
};

struct rig_caps smartsdr_c_rig_caps =
{
    RIG_MODEL(RIG_MODEL_SMARTSDR_C),
    .model_name =     "SmartSDR Slice C",
#include "smartsdr_caps.c"
};

struct rig_caps smartsdr_d_rig_caps =
{
    RIG_MODEL(RIG_MODEL_SMARTSDR_D),
    .model_name =     "SmartSDR Slice D",
#include "smartsdr_caps.c"
};

struct rig_caps smartsdr_e_rig_caps =
{
    RIG_MODEL(RIG_MODEL_SMARTSDR_E),
    .model_name =     "SmartSDR Slice E",
#include "smartsdr_caps.c"
};

struct rig_caps smartsdr_f_rig_caps =
{
    RIG_MODEL(RIG_MODEL_SMARTSDR_F),
    .model_name =     "SmartSDR Slice F",
#include "smartsdr_caps.c"
};

struct rig_caps smartsdr_g_rig_caps =
{
    RIG_MODEL(RIG_MODEL_SMARTSDR_G),
    .model_name =     "SmartSDR Slice G",
#include "smartsdr_caps.c"
};

struct rig_caps smartsdr_h_rig_caps =
{
    RIG_MODEL(RIG_MODEL_SMARTSDR_H),
    .model_name =     "SmartSDR Slice H",
#include "smartsdr_caps.c"
};


/* ************************************************************************* */

int smartsdr_init(RIG *rig)
{
    struct smartsdr_priv_data *priv;
    struct rig_state *rs = STATE(rig);
    hamlib_port_t *rp = RIGPORT(rig);

    ENTERFUNC;

    rs->priv = (struct smartsdr_priv_data *)calloc(1, sizeof(
                   struct smartsdr_priv_data));

    if (!rs->priv)
    {
        /* whoops! memory shortage! */
        RETURNFUNC(-RIG_ENOMEM);
    }

    priv = rs->priv;

    strncpy(rp->pathname, DEFAULTPATH, sizeof(rp->pathname));

    switch (rs->rig_model)
    {
    case RIG_MODEL_SMARTSDR_A: priv->slice = 0; break;

    case RIG_MODEL_SMARTSDR_B: priv->slice = 1; break;

    case RIG_MODEL_SMARTSDR_C: priv->slice = 2; break;

    case RIG_MODEL_SMARTSDR_D: priv->slice = 3; break;

    case RIG_MODEL_SMARTSDR_E: priv->slice = 4; break;

    case RIG_MODEL_SMARTSDR_F: priv->slice = 5; break;

    case RIG_MODEL_SMARTSDR_G: priv->slice = 6; break;

    case RIG_MODEL_SMARTSDR_H: priv->slice = 7; break;

    default:
        rig_debug(RIG_DEBUG_ERR, "%s: unknown rig model=%s\n", __func__,
                  rs->model_name);
        RETURNFUNC(-RIG_ENIMPL);
    }

    RETURNFUNC(RIG_OK);
}

int smartsdr_open(RIG *rig)
{
    hamlib_port_t *rp = RIGPORT(rig);
    struct smartsdr_priv_data *priv = (struct smartsdr_priv_data *)STATE(rig)->priv;
    char buf[4096];
    char *p;
    int nslices = -1;
    int retval;
    char stopset[1] = { 0x0a };
//struct smartsdr_priv_data *priv = (struct smartsdr_priv_data *)STATE(rig)->priv;
    ENTERFUNC;
    // Once we've connected and hit here we should have two messages queued from the intial connect

    read_string(rp, (unsigned char *)buf, sizeof(buf), stopset, 1, 0, 64);
    rig_debug(RIG_DEBUG_VERBOSE, "%s: Flex msg#1: %s", __func__, buf);
    read_string(rp, (unsigned char *)buf, sizeof(buf), stopset, 1, 0, 64);
    rig_debug(RIG_DEBUG_VERBOSE, "%s: Flex msg#2: %s", __func__, buf);

    if ((p = strstr(buf, "radio slices")))
    {
        sscanf(p, "radio slices=%d", &nslices);
        rig_debug(RIG_DEBUG_VERBOSE, "%s: radio_slices=%d\n", __func__, nslices);
    }
    sprintf(buf, "C%d|info\n", priv->seqnum++);
    retval = write_block(rp, (unsigned char *) buf, strlen(buf));

    if (retval < 0)
    {
        return -RIG_EIO;
    }
    read_string(rp, (unsigned char *)buf, sizeof(buf), stopset, 1, 0, 64);
    rig_debug(RIG_DEBUG_VERBOSE, "%s: info=%s\n", __func__, buf);

    RETURNFUNC(RIG_OK);
}

int smartsdr_close(RIG *rig)
{
    ENTERFUNC;

    RETURNFUNC(RIG_OK);
}

int smartsdr_cleanup(RIG *rig)
{
    struct smartsdr_priv_data *priv = (struct smartsdr_priv_data *)STATE(rig)->priv;

    ENTERFUNC;

    if (priv)
    {
        free(priv);
    }

    STATE(rig)->priv = NULL;

    RETURNFUNC(RIG_OK);
}

int smartsdr_set_freq(RIG *rig, vfo_t vfo, freq_t freq)
{
    struct smartsdr_priv_data *priv = (struct smartsdr_priv_data *)STATE(rig)->priv;
    hamlib_port_t *rp = RIGPORT(rig);
    char buf[4096];
    int retval;
    char stopset[1] = { 0x0a };
    ENTERFUNC;
    sprintf(buf, "C%d|slice tune 0 %.6f\n", priv->seqnum++, freq / 1e6);
    retval = write_block(rp, (unsigned char *) buf, strlen(buf));

    if (retval < 0)
    {
        return -RIG_EIO;
    }

    read_string(rp, (unsigned char *)buf, sizeof(buf), stopset, 1, 0, 64);
    rig_debug(RIG_DEBUG_VERBOSE, "%s: set_freq answer: %s", __func__, buf);
    rig_set_cache_freq(rig, vfo, freq);
    RETURNFUNC(RIG_OK);
}

int smartsdr_get_freq(RIG *rig, vfo_t vfo, freq_t *freq)
{
    int cache_ms_freq_p;
    ENTERFUNC;

    rig_get_cache_freq(rig, vfo, freq, &cache_ms_freq_p);
    RETURNFUNC(RIG_OK);
}

int smartsdr_reset(RIG *rig, reset_t reset)
{
    return -RIG_ENIMPL;
}

int smartsdr_set_ptt(RIG *rig, vfo_t vfo, ptt_t ptt)
{
    struct smartsdr_priv_data *priv = (struct smartsdr_priv_data *)STATE(rig)->priv;
    hamlib_port_t *rp = RIGPORT(rig);
    char buf[4096];
    int retval;
    char stopset[1] = { 0x0a };
    ENTERFUNC;

    if (ptt)
    {
        sprintf(buf, "C%d|slice set %d tx=1\n", priv->seqnum++, priv->slice);
        retval = write_block(rp, (unsigned char *) buf, strlen(buf));

        if (retval < 0)
        {
            return -RIG_EIO;
        }

        read_string(rp, (unsigned char *)buf, sizeof(buf), stopset, 1, 0, 64);
        rig_debug(RIG_DEBUG_VERBOSE, "%s: slice set answer: %s", __func__, buf);
    }

    sprintf(buf, "C%d|xmit %d\n", priv->seqnum++, ptt);
    read_string(rp, (unsigned char *)buf, sizeof(buf), stopset, 1, 0, 64);
    rig_debug(RIG_DEBUG_VERBOSE, "%s: xmit answer: %s", __func__, buf);
    priv->ptt = ptt;
    RETURNFUNC(RIG_OK);
}

int smartsdr_get_ptt(RIG *rig, vfo_t vfo, ptt_t *ptt)
{
    struct smartsdr_priv_data *priv = (struct smartsdr_priv_data *)STATE(rig)->priv;
    ENTERFUNC;
    *ptt = priv->ptt;
    RETURNFUNC(RIG_OK);
}

int rig_set_mode(RIG *rig, vfo_t vfo, rmode_t mode, pbwidth_t width)
{
    struct smartsdr_priv_data *priv = (struct smartsdr_priv_data *)STATE(rig)->priv;
    hamlib_port_t *rp = RIGPORT(rig);
    char buf[4096];
    char stopset[1] = { 0x0a };
    char *rmode;
    ENTERFUNC;

    switch (mode)
    {
    case RIG_MODE_CW: rmode = "CW"; break;

    case RIG_MODE_USB: rmode = "USB"; break;

    case RIG_MODE_LSB: rmode = "LSB"; break;

    case RIG_MODE_PKTUSB: rmode = "DIGU"; break;

    case RIG_MODE_PKTLSB: rmode = "DIGL"; break;

    case RIG_MODE_AM: rmode = "AM"; break;

    case RIG_MODE_FM: rmode = "FM"; break;

    case RIG_MODE_FMN: rmode = "FMN"; break;

    default:
        rig_debug(RIG_DEBUG_ERR, "%s: unknown mode=%s\n", __func__, rig_strrmode(mode));
    }

    sprintf(buf, "C%d|slice set %d mode=%s\n", priv->seqnum++, priv->slice, rmode);
    read_string(rp, (unsigned char *)buf, sizeof(buf), stopset, 1, 0, 64);

    if (width != RIG_PASSBAND_NOCHANGE)
    {
        sprintf(buf, "C%d|filt %d 0 %ld\n", priv->seqnum++, priv->slice, width);
        read_string(rp, (unsigned char *)buf, sizeof(buf), stopset, 1, 0, 64);
    }

    RETURNFUNC(RIG_OK);
}


#if 0
int sdr1k_set_level(RIG *rig, vfo_t vfo, setting_t level, value_t val)
{
    rig_debug(RIG_DEBUG_TRACE, "%s: %s %d\n", __func__, rig_strlevel(level), val.i);

    switch (level)
    {
    case RIG_LEVEL_PREAMP:
        return set_bit(rig, L_EXT, 7, !(val.i == rig->caps->preamp[0]));
        int smartsdr_set_ptt(RIG * rig, vfo_t vfo, ptt_t ptt)
        break;

    default:
        return -RIG_EINVAL;
    }
}
#endif
