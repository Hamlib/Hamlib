/*
 i  Hamlib TRXManager backend - main file
 *  Copyright (c) 2018 by Michael Black W9MDB
 *  Derived from flrig.c
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
#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <stdio.h>
#include <stdlib.h>
#include <string.h>             /* String function definitions */
#include <unistd.h>             /* UNIX standard function definitions */
#include <math.h>

#include <hamlib/rig.h>
#include <serial.h>
#include <misc.h>
#include <cal.h>
#include <token.h>
#include <register.h>
#include <network.h>

#include "trxmanager.h"

#define DEBUG_TRACE DEBUG_VERBOSE

#define MAXCMDLEN 64

#define DEFAULTPATH "127.0.0.1:1003"

#define TRXMANAGER_VFOS (RIG_VFO_A|RIG_VFO_B)

#define TRXMANAGER_MODES (RIG_MODE_AM | RIG_MODE_CW | RIG_MODE_CWR |\
                     RIG_MODE_RTTY | RIG_MODE_RTTYR |\
                     RIG_MODE_PKTLSB | RIG_MODE_PKTUSB |\
                     RIG_MODE_USB | RIG_MODE_LSB | RIG_MODE_FM)

#define streq(s1,s2) (strcmp(s1,s2)==0)

static int trxmanager_init(RIG *rig);
static int trxmanager_open(RIG *rig);
static int trxmanager_close(RIG *rig);
static int trxmanager_cleanup(RIG *rig);
static int trxmanager_set_freq(RIG *rig, vfo_t vfo, freq_t freq);
static int trxmanager_get_freq(RIG *rig, vfo_t vfo, freq_t *freq);
static int trxmanager_set_ptt(RIG *rig, vfo_t vfo, ptt_t ptt);
static int trxmanager_set_mode(RIG *rig, vfo_t vfo, rmode_t mode, pbwidth_t width);
static int trxmanager_set_split_mode(RIG *rig, vfo_t vfo, rmode_t mode, pbwidth_t width);
static int trxmanager_get_mode(RIG *rig, vfo_t vfo, rmode_t *mode, pbwidth_t *width);
static int trxmanager_get_vfo(RIG *rig, vfo_t *vfo);
static int trxmanager_set_vfo(RIG *rig, vfo_t vfo);
static int trxmanager_set_ptt(RIG *rig, vfo_t vfo, ptt_t ptt);
static int trxmanager_get_ptt(RIG *rig, vfo_t vfo, ptt_t *ptt);
static int trxmanager_set_split_freq(RIG *rig, vfo_t vfo, freq_t tx_freq);
static int trxmanager_get_split_freq(RIG *rig, vfo_t vfo, freq_t *tx_freq);
static int trxmanager_set_split_vfo(RIG *rig, vfo_t vfo, split_t split,
                                    vfo_t tx_vfo);
static int trxmanager_get_split_vfo(RIG *rig, vfo_t vfo, split_t *split,
                                    vfo_t *tx_vfo);
static int trxmanager_set_split_freq_mode(RIG *rig, vfo_t vfo, freq_t freq, rmode_t mode, pbwidth_t width);
static int trxmanager_get_split_freq_mode(RIG *rig, vfo_t vfo, freq_t *freq, rmode_t *mode, pbwidth_t *width);

static const char *trxmanager_get_info(RIG *rig);

struct trxmanager_priv_data {
    vfo_t vfo_curr;
    char info[100];
    split_t split;
};

struct rig_caps trxmanager_caps = {
    .rig_model = RIG_MODEL_TRXMANAGER_RIG,
    .model_name = "5.7.630+",
    .mfg_name = "TRXManager",
    .version = BACKEND_VER,
    .copyright = "LGPL",
    .status = RIG_STATUS_STABLE,
    .rig_type = RIG_TYPE_TRANSCEIVER,
    .targetable_vfo =  RIG_TARGETABLE_FREQ,
    .ptt_type = RIG_PTT_RIG,
    .port_type = RIG_PORT_NETWORK,
    .write_delay = 0,
    .post_write_delay = 0,
    .timeout = 10000, // long timeout to allow for antenna tuning and such
    .retry = 3,

    .has_get_func = RIG_FUNC_NONE,
    .has_set_func = RIG_FUNC_NONE,
    .has_get_level = RIG_LEVEL_NONE,
    .has_set_level = RIG_LEVEL_NONE,
    .has_get_parm = RIG_PARM_NONE,
    .has_set_parm = RIG_PARM_NONE,
    .filters =  {
        RIG_FLT_END
    },

    .rx_range_list1 = {{
            .start = kHz(1),.end = GHz(10),.modes = TRXMANAGER_MODES,
            .low_power = -1,.high_power = -1, TRXMANAGER_VFOS, RIG_ANT_1
        },
        RIG_FRNG_END,
    },
    .tx_range_list1 = {RIG_FRNG_END,},
    .rx_range_list2 = {{
            .start = kHz(1),.end = GHz(10),.modes = TRXMANAGER_MODES,
            .low_power = -1,.high_power = -1, TRXMANAGER_VFOS, RIG_ANT_1
        },
        RIG_FRNG_END,
    },
    .tx_range_list2 = {RIG_FRNG_END,},
    .tuning_steps =  { {TRXMANAGER_MODES,1}, {TRXMANAGER_MODES,RIG_TS_ANY}, RIG_TS_END, },
    .priv = NULL,               /* priv */

    .rig_init = trxmanager_init,
    .rig_open = trxmanager_open,
    .rig_close = trxmanager_close,
    .rig_cleanup = trxmanager_cleanup,

    .set_freq = trxmanager_set_freq,
    .get_freq = trxmanager_get_freq,
    .set_mode = trxmanager_set_mode,
    .get_mode = trxmanager_get_mode,
    .set_vfo = trxmanager_set_vfo,
    .get_vfo = trxmanager_get_vfo,
    .get_info = trxmanager_get_info,
    .set_ptt = trxmanager_set_ptt,
    .get_ptt = trxmanager_get_ptt,
    .set_split_mode = trxmanager_set_split_mode,
    .set_split_freq = trxmanager_set_split_freq,
    .get_split_freq = trxmanager_get_split_freq,
    .set_split_vfo = trxmanager_set_split_vfo,
    .get_split_vfo = trxmanager_get_split_vfo,
    .set_split_freq_mode = trxmanager_set_split_freq_mode,
    .get_split_freq_mode = trxmanager_get_split_freq_mode
};

/*
 * vfo_curr
 * No assumptions
 */
static int check_vfo(vfo_t vfo)
{
    switch (vfo) {
    case RIG_VFO_A:
        break;

    case RIG_VFO_TX:
    case RIG_VFO_B:
        break;

    case RIG_VFO_CURR:
        break;                  // will default to A in which_vfo

    default:
        return FALSE;
    }

    return TRUE;
}

/*
 * vfo_curr
 * Assumes rig!=NULL
 */
static int vfo_curr(RIG *rig, vfo_t vfo)
{
    int retval = 0;
    vfo_t vfocurr;
    struct trxmanager_priv_data *priv = (struct trxmanager_priv_data *) rig->state.priv;

    // get the current VFO from trxmanager in case user changed it
    if ((retval = trxmanager_get_vfo(rig, &vfocurr)) != RIG_OK) {
        return retval;
    }
    priv->vfo_curr = vfocurr;
    retval = (vfo == vfocurr);
    return retval;
}

/*
 * read_transaction
 * Assumes rig!=NULL, response!=NULL, response_len>=MAXCMDLEN
 */
static int read_transaction(RIG *rig, char *response, int response_len)
{
    rig_debug(RIG_DEBUG_TRACE, "%s\n", __FUNCTION__);

    struct rig_state *rs = &rig->state;

    char *delims="\n";
    int len = read_string(&rs->rigport, response, response_len, delims, strlen(delims));
    if (len <= 0) {
        rig_debug(RIG_DEBUG_ERR,"%s: read_string error=%d\n",__FUNCTION__,len);
        return -RIG_EPROTO;
    }
    return RIG_OK;
}

/*
 * trxmanager_init
 * Assumes rig!=NULL
 */
static int trxmanager_init(RIG *rig)
{

    rig_debug(RIG_DEBUG_TRACE, "%s version %s\n", __FUNCTION__, BACKEND_VER);

    struct trxmanager_priv_data *priv = (struct trxmanager_priv_data *)malloc(sizeof(struct trxmanager_priv_data));

    if (!priv) {
        return -RIG_ENOMEM;
    }

    memset(priv, 0, sizeof(struct trxmanager_priv_data));

    /*
     * set arbitrary initial status
     */
    rig->state.priv = (rig_ptr_t) priv;
    priv->vfo_curr = RIG_VFO_A;
    priv->split = 0;

    if (!rig || !rig->caps) {
        return -RIG_EINVAL;
    }

    strncpy(rig->state.rigport.pathname, DEFAULTPATH,
            sizeof(rig->state.rigport.pathname));

    return RIG_OK;
}

/*
 * trxmanager_open
 * Assumes rig!=NULL, rig->state.priv!=NULL
 */
static int trxmanager_open(RIG *rig) {
    int retval;
    char response[MAXCMDLEN] = "";

    rig_debug(RIG_DEBUG_VERBOSE, "%s version %s\n", __FUNCTION__, BACKEND_VER);

    struct rig_state *rs = &rig->state;
    struct trxmanager_priv_data *priv = (struct trxmanager_priv_data *) rig->state.priv;

    rs->rigport.timeout = 10000; // long timeout for antenna switching/tuning
    retval = read_transaction(rig, response, sizeof(response));
    if (retval != RIG_OK) {
        rig_debug(RIG_DEBUG_ERR,"%s read_transaction failed\n", __FUNCTION__);
    }
    rig_debug(RIG_DEBUG_VERBOSE,"%s connected to %s\n", __FUNCTION__, response);
    if (strlen(response)==0) {
        rig_debug(RIG_DEBUG_ERR,"%s response len==0\n", __FUNCTION__);
        return -RIG_EPROTO;
    }

    // Should have rig info now
    strtok(response,";\r\n");
    strncpy(priv->info,&response[2],sizeof(priv->info));

    // Turn off active messages
    char *cmd = "AI0;";
    retval = write_block(&rs->rigport, cmd, strlen(cmd));
    if (retval < 0) {
        return retval;
    }
    retval = read_transaction(rig, response, sizeof(response));
    if (retval != RIG_OK) {
        rig_debug(RIG_DEBUG_ERR,"%s read_transaction failed\n", __FUNCTION__);
    }
    if (strncmp("AI0;",response,4)!=0) {
        rig_debug(RIG_DEBUG_ERR,"%s AI invalid response=%s\n", __FUNCTION__, response);
        return -RIG_EINVAL;
    }
    rig_debug(RIG_DEBUG_VERBOSE,"%s AI response=%s\n", __FUNCTION__, response);

    cmd = "FN;";
    retval = write_block(&rs->rigport, cmd, strlen(cmd));
    if (retval != RIG_OK) {
        rig_debug(RIG_DEBUG_ERR,"%s FN; write failed=%s\n", __FUNCTION__);
    }
    retval = read_transaction(rig, response, sizeof(response));
    if (retval != RIG_OK) {
        rig_debug(RIG_DEBUG_ERR,"%s read_transaction failed\n", __FUNCTION__);
    }

    rig_debug(RIG_DEBUG_VERBOSE,"%s FN response=%s\n", __FUNCTION__, response);
    priv->vfo_curr = RIG_VFO_A;

    return retval;
}

/*
 * trxmanager_close
 * Assumes rig!=NULL
 */
static int trxmanager_close(RIG *rig) {
    rig_debug(RIG_DEBUG_TRACE, "%s\n", __FUNCTION__);
    return RIG_OK;
}

/*
 * trxmanager_cleanup
 * Assumes rig!=NULL, rig->state.priv!=NULL
 */
static int trxmanager_cleanup(RIG *rig) {

    if (!rig)
        return -RIG_EINVAL;

    free(rig->state.priv);
    rig->state.priv = NULL;

    return RIG_OK;
}

/*
 * trxmanager_get_freq
 * Assumes rig!=NULL, rig->state.priv!=NULL, freq!=NULL
 */
static int trxmanager_get_freq(RIG *rig, vfo_t vfo, freq_t *freq)
{
    int retval;

    rig_debug(RIG_DEBUG_TRACE, "%s: vfo=%s\n", __FUNCTION__,
              rig_strvfo(vfo));

    struct rig_state *rs = &rig->state;
    struct trxmanager_priv_data *priv = (struct trxmanager_priv_data *) rig->state.priv;

    if (check_vfo(vfo) == FALSE) {
        rig_debug(RIG_DEBUG_ERR, "%s: unsupported VFO %s\n",
                  __FUNCTION__, rig_strvfo(vfo));
        return -RIG_EINVAL;
    }

    if (vfo == RIG_VFO_CURR) {
        if ((retval = trxmanager_get_vfo(rig, &vfo)) != RIG_OK) {
            return retval;
        }
        priv->vfo_curr = vfo;

        rig_debug(RIG_DEBUG_VERBOSE, "%s: get_freq2 vfo=%s\n",
                  __FUNCTION__, rig_strvfo(vfo));
    }

    char cmd[MAXCMDLEN];
    char response[MAXCMDLEN]="";
    char vfoab = vfo == RIG_VFO_A ? 'R' : 'T';
    snprintf(cmd,sizeof(cmd),"X%c;",vfoab);
    retval = write_block(&rs->rigport, cmd, strlen(cmd));
    if (retval < 0) {
        return retval;
    }

    retval = read_transaction(rig, response, sizeof(response));
    if (retval != RIG_OK) {
        rig_debug(RIG_DEBUG_ERR,"%s read_transaction failed\n", __FUNCTION__);
    }
    *freq = 0;
    int n = sscanf(&response[2],"%lg",freq);
    if (n != 1) {
        rig_debug(RIG_DEBUG_ERR, "%s: can't parse freq from %s", __FUNCTION__,response);
    }
    if (*freq == 0) {
        rig_debug(RIG_DEBUG_ERR, "%s: freq==0??\n", __FUNCTION__);
        return -RIG_EPROTO;
    }
    return retval;
}

/*
 * trxmanager_set_freq
 * assumes rig!=NULL, rig->state.priv!=NULL
 */
static int trxmanager_set_freq(RIG *rig, vfo_t vfo, freq_t freq)
{
    int retval;

    rig_debug(RIG_DEBUG_TRACE, "%s: vfo=%s freq=%.1f\n", __FUNCTION__,
              rig_strvfo(vfo), freq);

    struct rig_state *rs = &rig->state;
    struct trxmanager_priv_data *priv = (struct trxmanager_priv_data *) rig->state.priv;

    if (check_vfo(vfo) == FALSE) {
        rig_debug(RIG_DEBUG_ERR, "%s: unsupported VFO %s\n",
                  __FUNCTION__, rig_strvfo(vfo));
        return -RIG_EINVAL;
    }

    if (vfo == RIG_VFO_CURR) {
        if ((retval = trxmanager_get_vfo(rig, &vfo)) != RIG_OK) {
            return retval;
        }
    }
    else if (vfo == RIG_VFO_TX && priv->split) {
        vfo = RIG_VFO_B; // if split always TX on VFOB
    }

    char cmd[MAXCMDLEN];
    char response[MAXCMDLEN]="";
    char vfoab = vfo == RIG_VFO_A ? 'A' : 'B';
    snprintf(cmd,sizeof(cmd), "F%c%011ld;", vfoab, (unsigned long)freq);
    retval = write_block(&rs->rigport, cmd, strlen(cmd));
    if (retval < 0) {
        return retval;
    }

    retval = read_transaction(rig, response, sizeof(response)); // get response but don't care
    if (retval != RIG_OK) {
        rig_debug(RIG_DEBUG_ERR,"%s read_transaction failed\n", __FUNCTION__);
    }

    return RIG_OK;
}

/*
 * trxmanager_set_ptt
 * Assumes rig!=NULL
 */
static int trxmanager_set_ptt(RIG *rig, vfo_t vfo, ptt_t ptt)
{
    int retval;

    rig_debug(RIG_DEBUG_TRACE, "%s: ptt=%d\n", __FUNCTION__, ptt);

    struct rig_state *rs = &rig->state;

    if (check_vfo(vfo) == FALSE) {
        rig_debug(RIG_DEBUG_ERR, "%s: unsupported VFO %s\n",
                  __FUNCTION__, rig_strvfo(vfo));
        return -RIG_EINVAL;
    }

    char cmd[MAXCMDLEN];
    char response[MAXCMDLEN]="";

    snprintf(cmd,sizeof(cmd),"%s;",ptt==1?"TX":"RX");
    retval = write_block(&rs->rigport, cmd, strlen(cmd));

    if (retval < 0) {
        return retval;
    }

    retval = read_transaction(rig, response, sizeof(response));
    if (retval != RIG_OK) {
        rig_debug(RIG_DEBUG_ERR,"%s read_transaction failed\n", __FUNCTION__);
    }
    if (strlen(response)!=5 || strstr(response, cmd)==NULL) {
        rig_debug(RIG_DEBUG_ERR, "%s invalid response='%s'\n", __FUNCTION__, response);
        return -RIG_EPROTO;
    }

    return RIG_OK;
}

/*
 * trxmanager_get_ptt
 * Assumes rig!=NULL ptt!=NULL
 */
static int trxmanager_get_ptt(RIG *rig, vfo_t vfo, ptt_t *ptt)
{
    int retval;

    rig_debug(RIG_DEBUG_TRACE, "%s: vfo=%s\n", __FUNCTION__,
              rig_strvfo(vfo));

    struct rig_state *rs = &rig->state;

    char cmd[MAXCMDLEN];
    char response[MAXCMDLEN]="";
    snprintf(cmd,sizeof(cmd),"IF;");
    retval = write_block(&rs->rigport, cmd, strlen(cmd));
    if (retval < 0) {
        return retval;
    }

    retval = read_transaction(rig, response, sizeof(response));
    if (retval != RIG_OK) {
        rig_debug(RIG_DEBUG_ERR,"%s read_transaction failed\n", __FUNCTION__);
    }
    if (strlen(response)!= 40) {
        rig_debug(RIG_DEBUG_ERR, "%s: invalid response='%s'\n", __FUNCTION__, response);
        return -RIG_EPROTO;
    }
    rig_debug(RIG_DEBUG_VERBOSE, "%s: IF response len='%d'\n", __FUNCTION__, strlen(response));
    char cptt = response[28];
    *ptt = cptt == '0' ? 0 : 1;

    return RIG_OK;
}

/*
 * trxmanager_set_split_mode
 * Assumes rig!=NULL
 */
static int trxmanager_set_split_mode(RIG *rig, vfo_t vfo, rmode_t mode, pbwidth_t width)
{
    int retval;
    rig_debug(RIG_DEBUG_TRACE, "%s: vfo=%s mode=%s width=%d\n",
              __FUNCTION__, rig_strvfo(vfo), rig_strrmode(mode), width);
    retval = trxmanager_set_mode(rig,RIG_VFO_B,mode,width);
    return retval;
}

/*
 * trxmanager_set_mode
 * Assumes rig!=NULL
 */
static int trxmanager_set_mode(RIG *rig, vfo_t vfo, rmode_t mode, pbwidth_t width)
{
    int retval;

    rig_debug(RIG_DEBUG_TRACE, "%s: vfo=%s mode=%s width=%d\n",
              __FUNCTION__, rig_strvfo(vfo), rig_strrmode(mode), width);

    struct rig_state *rs = &rig->state;

    if (check_vfo(vfo) == FALSE) {
        rig_debug(RIG_DEBUG_ERR, "%s: unsupported VFO %s\n",
                  __FUNCTION__, rig_strvfo(vfo));
        return -RIG_EINVAL;
    }

    int ttmode=0;
    switch(mode) {
    case RIG_MODE_LSB:
        ttmode=1;
        break;
    case RIG_MODE_USB:
        ttmode=2;
        break;
    case RIG_MODE_CW:
        ttmode=3;
        break;
    case RIG_MODE_FM:
        ttmode=4;
        break;
    case RIG_MODE_AM:
        ttmode=5;
        break;
    case RIG_MODE_RTTY:
        ttmode=6;
        break;
    case RIG_MODE_CWR:
        ttmode=7;
        break;
    case RIG_MODE_RTTYR:
        ttmode=9;
        break;
    case RIG_MODE_PKTLSB:
        ttmode=9;
        break;
    case RIG_MODE_PKTUSB:
        ttmode=9;
        break;
    default:
        rig_debug(RIG_DEBUG_ERR, "%s: unsupported mode %s\n",__FUNCTION__,rig_strrmode(mode));
        return -RIG_EINVAL;

    }

    char cmd[MAXCMDLEN];
    char response[MAXCMDLEN]="";
    snprintf(cmd,sizeof(cmd), "MD%d;", ttmode);
    retval = write_block(&rs->rigport, cmd, strlen(cmd));
    if (retval < 0) {
        return retval;
    }

    // Get the response
    retval = read_transaction(rig, response, sizeof(response));
    if (retval != RIG_OK) {
        rig_debug(RIG_DEBUG_ERR,"%s read_transaction failed\n", __FUNCTION__);
    }
    rig_debug(RIG_DEBUG_VERBOSE, "%s: response=%s\n", __FUNCTION__,response);

    // Can't set BW on TRXManger as of 20180427 -- can only read it

    return RIG_OK;
}

/*
 * trxmanager_get_mode
 * Assumes rig!=NULL, rig->state.priv!=NULL, mode!=NULL
 */
static int trxmanager_get_mode(RIG *rig, vfo_t vfo, rmode_t *mode, pbwidth_t *width)
{
    int retval;

    rig_debug(RIG_DEBUG_TRACE, "%s: vfo=%s\n", __FUNCTION__,
              rig_strvfo(vfo));

    struct rig_state *rs = &rig->state;
    struct trxmanager_priv_data *priv = (struct trxmanager_priv_data *) rig->state.priv;

    if (check_vfo(vfo) == FALSE) {
        rig_debug(RIG_DEBUG_ERR, "%s: unsupported VFO %s\n",
                  __FUNCTION__, rig_strvfo(vfo));
        return -RIG_EINVAL;
    }

    if (vfo == RIG_VFO_CURR) {
        if ((retval = trxmanager_get_vfo(rig, &vfo)) != RIG_OK) {
            return retval;
        }
        priv->vfo_curr = vfo;
    }
    rig_debug(RIG_DEBUG_TRACE, "%s: using vfo=%s\n", __FUNCTION__,
              rig_strvfo(vfo));

    char cmd[MAXCMDLEN];
    char response[MAXCMDLEN]="";
    snprintf(cmd,sizeof(cmd),"MD;");
    retval = write_block(&rs->rigport, cmd, strlen(cmd));
    if (retval < 0) {
        return retval;
    }

    retval = read_transaction(rig, response, sizeof(response));
    if (retval != RIG_OK) {
        rig_debug(RIG_DEBUG_ERR,"%s read_transaction failed\n", __FUNCTION__);
    }
    char tmode;
    int n = sscanf(response,"MD%c;",&tmode);
    if (n != 1 || strlen(response)!=6) {
        rig_debug(RIG_DEBUG_ERR, "%s: invalid response='%s'\n", __FUNCTION__, response);
        return -RIG_EPROTO;
    }
    switch(tmode) {
    case '1':
        *mode=RIG_MODE_LSB;
        break;
    case '2':
        *mode=RIG_MODE_USB;
        break;
    case '3':
        *mode=RIG_MODE_CW;
        break;
    case '4':
        *mode=RIG_MODE_FM;
        break;
    case '5':
        *mode=RIG_MODE_AM;
        break;
    case '6':
        *mode=RIG_MODE_RTTY;
        break;
    case '7':
        *mode=RIG_MODE_CWR;
        break;
    case '9':
        *mode=RIG_MODE_RTTYR;
        break;
    case 'A':
        *mode=RIG_MODE_PKTLSB;
        break;
    case 'B':
        *mode=RIG_MODE_PKTUSB;
        break;
    default:
        rig_debug(RIG_DEBUG_ERR, "%s: unknown mode='%c'\n", __FUNCTION__, tmode);
        return -RIG_EINVAL;
    }
    rig_debug(RIG_DEBUG_VERBOSE, "%s: mode='%s'\n", __FUNCTION__, rig_strrmode(*mode));

    // now get the bandwidth
    snprintf(cmd,sizeof(cmd),"BW;");
    retval = write_block(&rs->rigport, cmd, strlen(cmd));
    if (retval < 0) {
        return retval;
    }

    retval = read_transaction(rig, response, sizeof(response));
    if (retval != RIG_OK) {
        rig_debug(RIG_DEBUG_ERR,"%s read_transaction failed\n", __FUNCTION__);
    }

    if (strncmp(response,"BW",2)!=0) {
        rig_debug(RIG_DEBUG_ERR, "%s: invalid response='%s'\n", __FUNCTION__, response);
        return -RIG_EPROTO;
    }
    long iwidth = 0;
    n = sscanf(response,"BW%ld;",&iwidth);
    if (n != 1) {
        rig_debug(RIG_DEBUG_ERR,"%s bandwidth scan failed 's'\n", __FUNCTION__,strtok(response,"\r\n"));
        return -RIG_EPROTO;
    }
    *width=iwidth;
    printf("Bandwidth=%ld\n",*width);
    return RIG_OK;
}

static int trxmanager_set_vfo(RIG *rig, vfo_t vfo)
{
    int retval;

    rig_debug(RIG_DEBUG_TRACE, "%s: vfo=%s\n", __FUNCTION__,
              rig_strvfo(vfo));

    struct rig_state *rs = &rig->state;
    struct trxmanager_priv_data *priv = (struct trxmanager_priv_data *) rig->state.priv;

    if (check_vfo(vfo) == FALSE) {
        rig_debug(RIG_DEBUG_ERR, "%s: unsupported VFO %s\n",
                  __FUNCTION__, rig_strvfo(vfo));
        return -RIG_EINVAL;
    }
    if (vfo == RIG_VFO_TX) {
        rig_debug(RIG_DEBUG_VERBOSE, "%s: RIG_VFO_TX used\n");
        vfo = RIG_VFO_B; // always TX on VFOB
    }

    if (vfo == RIG_VFO_CURR) {
        vfo = priv->vfo_curr;
    }

    char cmd[MAXCMDLEN];
    char response[MAXCMDLEN]="";
    snprintf(cmd,sizeof(cmd), "FN%d;",vfo ==RIG_VFO_A ? 0 : 1);
    retval = write_block(&rs->rigport, cmd, strlen(cmd));
    if (retval < 0) {
        return retval;
    }

    priv->vfo_curr = vfo;
    rs->tx_vfo = RIG_VFO_B; // always VFOB
    retval = read_transaction(rig, response, sizeof(response));
    if (retval != RIG_OK) {
        rig_debug(RIG_DEBUG_ERR,"%s read_transaction failed\n", __FUNCTION__);
    }

    return RIG_OK;
}

static int trxmanager_get_vfo(RIG *rig, vfo_t *vfo)
{
    // TRXManager does not swap vfos
    // So we maintain our own internal state during set_vfo
    // This keeps the hamlib interface consistent with other rigs

    rig_debug(RIG_DEBUG_TRACE, "%s\n", __FUNCTION__);

    struct trxmanager_priv_data *priv = (struct trxmanager_priv_data *) rig->state.priv;

    char vfoab = priv->vfo_curr;

    switch (vfoab) {
    case RIG_VFO_A:
        *vfo = RIG_VFO_A;
        break;

    case RIG_VFO_B:
        *vfo = RIG_VFO_B;
        break;

    default:
        priv->vfo_curr = *vfo;
        *vfo = RIG_VFO_CURR;
        return -RIG_EINVAL;
    }

    if (check_vfo(*vfo) == FALSE) {
        rig_debug(RIG_DEBUG_ERR, "%s: unsupported VFO %s\n",
                  __FUNCTION__, rig_strvfo(*vfo));
        return -RIG_EINVAL;
    }

    priv->vfo_curr = *vfo;

    rig_debug(RIG_DEBUG_VERBOSE, "%s: vfo=%s\n", __FUNCTION__,
              rig_strvfo(*vfo));

    return RIG_OK;
}

/*
 * trxmanager_set_split_freq
 */
static int trxmanager_set_split_freq(RIG *rig, vfo_t vfo, freq_t tx_freq)
{
    int retval;

    rig_debug(RIG_DEBUG_TRACE, "%s: vfo=%s freq=%.1f\n", __FUNCTION__,
              rig_strvfo(vfo), tx_freq);

    struct rig_state *rs = &rig->state;

    if (check_vfo(vfo) == FALSE) {
        rig_debug(RIG_DEBUG_ERR, "%s: unsupported VFO %s\n",
                  __FUNCTION__, rig_strvfo(vfo));
        return -RIG_EINVAL;
    }

    char cmd[MAXCMDLEN];
    char response[MAXCMDLEN]="";
    snprintf(cmd,sizeof(cmd),"XT%011ld;", (unsigned long) tx_freq);
    retval = write_block(&rs->rigport, cmd, strlen(cmd));
    if (retval < 0) {
        return retval;
    }
    retval = read_transaction(rig, response, sizeof(response)); // get response but don't care
    if (retval != RIG_OK) {
        rig_debug(RIG_DEBUG_ERR,"%s read_transaction failed\n", __FUNCTION__);
    }

    return RIG_OK;
}

/*
 * trxmanager_get_split_freq
 * assumes rig!=NULL, tx_freq!=NULL
 */
static int trxmanager_get_split_freq(RIG *rig, vfo_t vfo, freq_t *tx_freq)
{
    rig_debug(RIG_DEBUG_TRACE, "%s: vfo=%s\n", __FUNCTION__,
              rig_strvfo(vfo));
    int retval = trxmanager_get_freq(rig, RIG_VFO_B, tx_freq);
    return retval;
}

/*
 * trxmanager_set_split_vfo
 * assumes rig!=NULL, tx_freq!=NULL
 */
static int trxmanager_set_split_vfo(RIG *rig, vfo_t vfo, split_t split, vfo_t tx_vfo)
{
    int retval;

    rig_debug(RIG_DEBUG_TRACE, "%s: tx_vfo=%s\n", __FUNCTION__,
              rig_strvfo(tx_vfo));

    struct rig_state *rs = &rig->state;

    if (tx_vfo == RIG_VFO_SUB || tx_vfo == RIG_VFO_TX) {
        tx_vfo = RIG_VFO_B;
    }

    /* for flrig we have to be on VFOA when we set split for VFOB Tx */
    /* we can keep the rig on VFOA since we can set freq by VFO now */
    if (!vfo_curr(rig, RIG_VFO_A)) {
        trxmanager_set_vfo(rig, RIG_VFO_A);
    }
    char cmd[MAXCMDLEN];
    char response[MAXCMDLEN]="";
    snprintf(cmd,sizeof(cmd),"SP%c;", split ? '1' : '0');
    retval = write_block(&rs->rigport, cmd, strlen(cmd));

    if (retval < 0) {
        return retval;
    }

    retval = read_transaction(rig, response, sizeof(response)); // get response but don't care
    if (retval != RIG_OK) {
        rig_debug(RIG_DEBUG_ERR,"%s read_transaction failed\n", __FUNCTION__);
    }

    if (strlen(response)!=6 || strstr(response, cmd)==NULL) {
        rig_debug(RIG_DEBUG_ERR, "%s invalid response='%s'\n", __FUNCTION__, response);
        return -RIG_EPROTO;
    }

    return RIG_OK;
}

/*
 * trxmanager_get_split_vfo
 * assumes rig!=NULL, tx_freq!=NULL
 */
static int trxmanager_get_split_vfo(RIG *rig, vfo_t vfo, split_t *split, vfo_t *tx_vfo)
{
    int retval;

    rig_debug(RIG_DEBUG_TRACE, "%s\n", __FUNCTION__);
    struct rig_state *rs = &rig->state;
    struct trxmanager_priv_data *priv = (struct trxmanager_priv_data *) rig->state.priv;

    char cmd[MAXCMDLEN];
    char response[MAXCMDLEN]="";
    snprintf(cmd,sizeof(cmd),"SP;");
    retval = write_block(&rs->rigport, cmd, strlen(cmd));
    if (retval < 0) {
        return retval;
    }
    retval = read_transaction(rig, response, sizeof(response)); // get response but don't care
    if (retval != RIG_OK) {
        rig_debug(RIG_DEBUG_ERR,"%s read_transaction failed\n", __FUNCTION__);
    }

    *tx_vfo = RIG_VFO_B;
    int tsplit=0;
    int n = sscanf(response,"SP%d",&tsplit);
    if (n == 0) {
        rig_debug(RIG_DEBUG_ERR, "%s error getting split from '%s'\n", __FUNCTION__,response);
    }
    *split = tsplit;
    priv->split = *split;
    return RIG_OK;
}

/*
 * trxmanager_set_split_freq_mode
 * assumes rig!=NULL
 */
static int trxmanager_set_split_freq_mode(RIG *rig, vfo_t vfo, freq_t freq, rmode_t mode, pbwidth_t width)
{
    int retval;

    rig_debug(RIG_DEBUG_TRACE, "%s\n", __FUNCTION__);

    struct rig_state *rs = &rig->state;
    struct trxmanager_priv_data *priv = (struct trxmanager_priv_data *) rig->state.priv;

    if (vfo != RIG_VFO_CURR && vfo != RIG_VFO_TX)
        return -RIG_ENTARGET;

    // assume split is on B
    //
    char cmd[MAXCMDLEN];
    char response[MAXCMDLEN]="";
    snprintf(cmd,sizeof(cmd),"XT%011ld;",(unsigned long)freq);
    retval = write_block(&rs->rigport, cmd, strlen(cmd));
    if (retval < 0) {
        return retval;
    }
    retval = read_transaction(rig, response, sizeof(response));
    if (retval != RIG_OK) {
        rig_debug(RIG_DEBUG_ERR,"%s read_transaction failed\n", __FUNCTION__);
    }
    if (strlen(response)!=16 || strstr(response, cmd)==NULL) {
        rig_debug(RIG_DEBUG_ERR, "%s invalid response='%s'\n", __FUNCTION__, response);
        FILE *fp=fopen("debug.txt","w+");
        fprintf(fp,"XT response=%s\n",response);
        fclose(fp);
        return -RIG_EPROTO;
    }
    priv->split = 1; // XT command also puts rig in split

    return retval;
}

/*
 * trxmanager_get_split_freq_mode
 * assumes rig!=NULL, freq!=NULL, mode!=NULL, width!=NULL
 */
static int trxmanager_get_split_freq_mode(RIG *rig, vfo_t vfo, freq_t *freq, rmode_t *mode, pbwidth_t *width)
{
    int retval;

    if (vfo != RIG_VFO_CURR && vfo != RIG_VFO_TX)
        return -RIG_ENTARGET;

    retval = trxmanager_get_freq (rig, RIG_VFO_B, freq);
    if (RIG_OK == retval) {
        retval = trxmanager_get_mode(rig,vfo,mode,width);
    }

    return retval;
}


static const char *trxmanager_get_info(RIG *rig)
{
    struct trxmanager_priv_data *priv = (struct trxmanager_priv_data *) rig->state.priv;
    rig_debug(RIG_DEBUG_VERBOSE, "%s called\n", __FUNCTION__);

    return priv->info;
}

