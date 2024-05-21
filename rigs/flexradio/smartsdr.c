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
#include <errno.h>

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
static int smartsdr_get_ptt(RIG *rig, vfo_t vfo, ptt_t *ptt);
static int smartsdr_set_mode(RIG *rig, vfo_t vfo, rmode_t mode, pbwidth_t width);
//static int smartsdr_set_level(RIG *rig, vfo_t vfo, setting_t level, value_t val);

struct smartsdr_priv_data
{
    int slicenum; // slice 0-7 maps to A-H
    int seqnum;
    int ptt;
};


#define DEFAULTPATH "127.0.0.1:4992"

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
#include "smartsdr_caps.h"
};

struct rig_caps smartsdr_b_rig_caps =
{
    RIG_MODEL(RIG_MODEL_SMARTSDR_B),
    .model_name =     "SmartSDR Slice B",
#include "smartsdr_caps.h"
};

struct rig_caps smartsdr_c_rig_caps =
{
    RIG_MODEL(RIG_MODEL_SMARTSDR_C),
    .model_name =     "SmartSDR Slice C",
#include "smartsdr_caps.h"
};

struct rig_caps smartsdr_d_rig_caps =
{
    RIG_MODEL(RIG_MODEL_SMARTSDR_D),
    .model_name =     "SmartSDR Slice D",
#include "smartsdr_caps.h"
};

struct rig_caps smartsdr_e_rig_caps =
{
    RIG_MODEL(RIG_MODEL_SMARTSDR_E),
    .model_name =     "SmartSDR Slice E",
#include "smartsdr_caps.h"
};

struct rig_caps smartsdr_f_rig_caps =
{
    RIG_MODEL(RIG_MODEL_SMARTSDR_F),
    .model_name =     "SmartSDR Slice F",
#include "smartsdr_caps.h"
};

struct rig_caps smartsdr_g_rig_caps =
{
    RIG_MODEL(RIG_MODEL_SMARTSDR_G),
    .model_name =     "SmartSDR Slice G",
#include "smartsdr_caps.h"
};

struct rig_caps smartsdr_h_rig_caps =
{
    RIG_MODEL(RIG_MODEL_SMARTSDR_H),
    .model_name =     "SmartSDR Slice H",
#include "smartsdr_caps.h"
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
    case RIG_MODEL_SMARTSDR_A: priv->slicenum = 0; break;

    case RIG_MODEL_SMARTSDR_B: priv->slicenum = 1; break;

    case RIG_MODEL_SMARTSDR_C: priv->slicenum = 2; break;

    case RIG_MODEL_SMARTSDR_D: priv->slicenum = 3; break;

    case RIG_MODEL_SMARTSDR_E: priv->slicenum = 4; break;

    case RIG_MODEL_SMARTSDR_F: priv->slicenum = 5; break;

    case RIG_MODEL_SMARTSDR_G: priv->slicenum = 6; break;

    case RIG_MODEL_SMARTSDR_H: priv->slicenum = 7; break;

    default:
        rig_debug(RIG_DEBUG_ERR, "%s: unknown rig model=%s\n", __func__,
                  rs->model_name);
        RETURNFUNC(-RIG_ENIMPL);
    }

    RETURNFUNC(RIG_OK);
}

static int smartsdr_transaction(RIG *rig, char *buf, char *reply, int reply_len)
{
    struct smartsdr_priv_data *priv = (struct smartsdr_priv_data *)STATE(rig)->priv;
    hamlib_port_t *rp = RIGPORT(rig);
    char stopset[1] = { 0x0a };
    char cmd[4096];
    int retval;
    int nbytes;
    if (priv->seqnum > 10000) priv->seqnum = 0;
    sprintf(cmd,"C%d|%s%c", priv->seqnum++, buf, 0x0a);
    rig_flush(rp);
    retval = write_block(rp, (unsigned char *) cmd, strlen(cmd));
    nbytes = read_string(rp, (unsigned char *)reply, reply_len, stopset, 1, 0, sizeof(buf)-1);
    retval = nbytes > 0 ? RIG_OK : retval;
    return retval;
}

int smartsdr_open(RIG *rig)
{
    //struct smartsdr_priv_data *priv = (struct smartsdr_priv_data *)STATE(rig)->priv;
    hamlib_port_t *rp = RIGPORT(rig);
    char buf[4096];
    char cmd[64];
    char *p;
    int nslices = -1;
    char stopset[1] = { 0x0a };
    ENTERFUNC;
    // Once we've connected and hit here we should have two messages queued from the intial connect

    read_string(rp, (unsigned char *)buf, sizeof(buf), stopset, 1, 0, sizeof(buf)-1);
    rig_debug(RIG_DEBUG_VERBOSE, "%s: Flex msg#1: %s", __func__, buf);
    read_string(rp, (unsigned char *)buf, sizeof(buf), stopset, 1, 0, sizeof(buf)-1);
    rig_debug(RIG_DEBUG_VERBOSE, "%s: Flex msg#2: %s", __func__, buf);

    if ((p = strstr(buf, "radio slices")))
    {
        sscanf(p, "radio slices=%d", &nslices);
        rig_debug(RIG_DEBUG_VERBOSE, "%s: radio_slices=%d", __func__, nslices);
    }
    //sprintf(cmd,"sub slice %d", priv->slicenum);
    sprintf(cmd,"sub slice all");
    smartsdr_transaction(rig, cmd , buf, sizeof(buf));
    smartsdr_transaction(rig, "info", buf, sizeof(buf));
    rig_debug(RIG_DEBUG_VERBOSE, "%s: info=%s", __func__, buf);

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

#if 0
#if defined(HAVE_PTHREAD)
typedef struct smartsdr_data_handler_args_s
{
    RIG *rig;
} smartsdr_data_handler_args;

typedef struct smartsdr_data_handler_priv_data_s
{
    pthread_t thread_id;
    smartsdr_data_handler_args args;
    int smartsdr_data_handler_thread_run;
} smartsdr_data_handler_priv_data;

void *smartsdr_data_handler(void *arg)
{
    struct smartsdr_priv_data *priv;
    struct smartsdr_data_handler_args_s *args =
        (struct smartsdr_data_handler_args_s *) arg;
    smartsdr_data_handler_priv_data *smartsdr_data_handler_priv;
    //RIG *rig = args->rig;
    //const struct rig_state *rs = STATE(rig);
    //int result;

    rig_debug(RIG_DEBUG_VERBOSE, "%s: Starting morse data handler thread\n",
              __func__);

    while (priv->smartsdr_data_handler_priv_data->smartsdr_data_handler_thread_run)
    {
    }
    pthread_exit(NULL);
    return NULL;
}

static int smartsdr_data_handler_start(RIG *rig)
{
    struct smartsdr_priv_data *priv;
    smartsdr_data_handler_priv_data *smartsdr_data_handler_priv;

    ENTERFUNC;

    priv->smartsdr_data_handler_thread_run = 1;
    priv->smartsdr_data_handler_priv_data = calloc(1,
                                       sizeof(smartsdr_data_handler_priv_data));

    if (priv->smartsdr_data_handler_priv_data == NULL)
    {
        RETURNFUNC(-RIG_ENOMEM);
    }

    smartsdr_data_handler_priv = (smartsdr_data_handler_priv_data *)
                              priv->smartsdr_data_handler_priv_data;
    smartsdr_data_handler_priv->args.rig = rig;
    int err = pthread_create(&smartsdr_data_handler_priv->thread_id, NULL,
                             smartsdr_data_handler, &smartsdr_data_handler_priv->args);

    if (err)
    {
        rig_debug(RIG_DEBUG_ERR, "%s: pthread_create error: %s\n", __func__,
                  strerror(errno));
        RETURNFUNC(-RIG_EINTERNAL);
    }

    RETURNFUNC(RIG_OK);
}
#endif
#endif


int smartsdr_set_freq(RIG *rig, vfo_t vfo, freq_t freq)
{
    struct smartsdr_priv_data *priv = (struct smartsdr_priv_data *)STATE(rig)->priv;
    char buf[4096];
    char cmd[64];
    ENTERFUNC;
    sprintf(cmd, "slice tune %d %.6f", priv->slicenum, freq / 1e6);
    smartsdr_transaction(rig, cmd, buf, sizeof(buf));
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
    char buf[4096];
    char cmd[64];
    ENTERFUNC;

    if (ptt)
    {
        sprintf(cmd, "dax audio set %d tx=1", priv->slicenum+1);
        smartsdr_transaction(rig, cmd, buf, sizeof(buf));
        rig_debug(RIG_DEBUG_VERBOSE, "%s: slice set answer: %s", __func__, buf);
    }
    sprintf(cmd, "slice set %d tx=%d", priv->slicenum);
    smartsdr_transaction(rig, cmd, buf, sizeof(buf));
    sprintf(cmd, "xmit %d", ptt);
    smartsdr_transaction(rig, cmd, buf, sizeof(buf));
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

int smartsdr_set_mode(RIG *rig, vfo_t vfo, rmode_t mode, pbwidth_t width)
{
    struct smartsdr_priv_data *priv = (struct smartsdr_priv_data *)STATE(rig)->priv;
    char buf[4096];
    char cmd[64];
    char *rmode=RIG_MODE_NONE;
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

    case RIG_MODE_SAM: rmode = "SAM"; break;

    default:
        rig_debug(RIG_DEBUG_ERR, "%s: unknown mode=%s\n", __func__, rig_strrmode(mode));
    }

    sprintf(cmd, "slice set %d mode=%s", priv->slicenum, rmode);
    smartsdr_transaction(rig, cmd, buf, sizeof(buf));

    if (width != RIG_PASSBAND_NOCHANGE)
    {
        sprintf(cmd, "filt %d 0 %ld", priv->slicenum, width);
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
