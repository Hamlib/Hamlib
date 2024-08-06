/*
 *  Hamlib backend - SmartSDR TCP on port 4952
 *  See https://github.com/flexradio/smartsdr-api-docs/wiki/SmartSDR-TCPIP-API
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
#include "network.h"

static int smartsdr_set_freq(RIG *rig, vfo_t vfo, freq_t freq);
static int smartsdr_get_freq(RIG *rig, vfo_t vfo, freq_t *freq);
//static int smartsdr_reset(RIG *rig, reset_t reset);
static int smartsdr_init(RIG *rig);
static int smartsdr_open(RIG *rig);
static int smartsdr_close(RIG *rig);
static int smartsdr_cleanup(RIG *rig);
static int smartsdr_set_ptt(RIG *rig, vfo_t vfo, ptt_t ptt);
static int smartsdr_get_ptt(RIG *rig, vfo_t vfo, ptt_t *ptt);
static int smartsdr_set_mode(RIG *rig, vfo_t vfo, rmode_t mode,
                             pbwidth_t width);
static int smartsdr_get_mode(RIG *rig, vfo_t vfo, rmode_t *mode,
                             pbwidth_t *width);
//static int smartsdr_set_level(RIG *rig, vfo_t vfo, setting_t level, value_t val);

struct smartsdr_priv_data
{
    int slicenum; // slice 0-7 maps to A-H
    int seqnum;
    int ptt;
    int tx; // when 1 this slice has PTT control
    double freqA;
    double freqB;
    rmode_t modeA;
    rmode_t modeB;
    int widthA;
    int widthB;
};


#define DEFAULTPATH "127.0.0.1:4992"

#define SMARTSDR_FUNC  RIG_FUNC_MUTE
#define SMARTSDR_LEVEL RIG_LEVEL_PREAMP
#define SMARTSDR_PARM  RIG_PARM_NONE

#define SMARTSDR_MODES (RIG_MODE_USB|RIG_MODE_LSB|RIG_MODE_PKTUSB|RIG_MODE_PKTLSB|RIG_MODE_CW|RIG_MODE_AM|RIG_MODE_FM|RIG_MODE_FMN|RIG_MODE_SAM)

#define SMARTSDR_VFO (RIG_VFO_A)

#define SMARTSDR_ANTS 3

static int smartsdr_parse_S(RIG *rig, char *s);

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

    priv->ptt = 0;

    RETURNFUNC(RIG_OK);
}

// flush any messages in the queue and process them too
// return 0 if OK, otherwise SMARTSDR error code
static int smartsdr_flush(RIG *rig)
{
    char buf[8192];
    int buf_len = 8192;
    char stopset[1] = { 0x0a };
    int len = 0;
    int retval = RIG_OK;
    ENTERFUNC;
#if 0
    // for this flush routine we expect at least one message for sure -- might be more
    len = read_string(RIGPORT(rig), (unsigned char *)buf, buf_len, stopset, 1, 0,
                      1);

    if (buf[0] == 'S') { smartsdr_parse_S(rig, buf); }

    rig_debug(RIG_DEBUG_VERBOSE, "%s: read %d bytes\n", __func__, len);
#endif

    do
    {
        buf[0] = 0;
        len = network_flush2(RIGPORT(rig), (unsigned char *)stopset, buf, buf_len);

        if (buf[0] == 'S') { smartsdr_parse_S(rig, buf); }

        else if (buf[0] == 'R') { sscanf(buf, "R%d", &retval); }

        else if (strlen(buf) > 0) { rig_debug(RIG_DEBUG_WARN, "%s: Unknown packet type=%s\n", __func__, buf); }
    }
    while (len > 0);

    RETURNFUNC(retval);
}

static int smartsdr_transaction(RIG *rig, char *buf)
{
    struct smartsdr_priv_data *priv = (struct smartsdr_priv_data *)STATE(rig)->priv;
    char cmd[4096];
    int retval;

    if (priv->seqnum > 999999) { priv->seqnum = 0; }

    if (buf)
    {
        sprintf(cmd, "C%d|%s%c", priv->seqnum++, buf, 0x0a);
        retval = write_block(RIGPORT(rig), (unsigned char *) cmd, strlen(cmd));

        if (retval != RIG_OK)
        {
            rig_debug(RIG_DEBUG_ERR, "%s: SmartSDR write_block err=0x%x\n", __func__,
                      retval);
        }
    }

    retval = smartsdr_flush(rig);

    if (retval != 0)
    {
        rig_debug(RIG_DEBUG_ERR, "%s: SmartSDR flush err=0x%x\n", __func__, retval);
        retval = -RIG_EPROTO;
    }

    return retval;
}

int smartsdr_open(RIG *rig)
{
    struct smartsdr_priv_data *priv = (struct smartsdr_priv_data *)STATE(rig)->priv;
    char cmd[64];
    int loops = 20;
    ENTERFUNC;
    // Once we've connected and hit here we should have two messages queued from the intial connect

    sprintf(cmd, "sub slice %d", priv->slicenum);
    //sprintf(cmd, "sub slice all");
    smartsdr_transaction(rig, cmd);

    do
    {
        hl_usleep(100 * 1000);
        smartsdr_transaction(rig, NULL);
    }
    while (priv->freqA == 0 && --loops > 0);

    //smartsdr_transaction(rig, "info", buf, sizeof(buf));
    //rig_debug(RIG_DEBUG_VERBOSE, "%s: info=%s", __func__, buf);

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
    fset = 1500 post_demod_bypass = 0 rfgain = 24  tx_ant_list = ANT1, ANT2, XVTA,
    XVTB

}
#endif
#endif

/* Example response to "sub slice 0"
511+511+35
S67319A86|slice 0 in_use=1 sample_rate=24000 RF_frequency=10.137000 client_handle=0x76AF7C73 index_letter=A rit_on=0 rit_freq=0 xit_on=0 xit_freq=0 rxant=ANT2 mode=DIGU wide=0 filter_lo=0 filter_hi=3510 step=10 step_list=1,5,10,20,100,250,500,1000 agc_mode=fast agc_threshold=65 agc_off_level=10 pan=0x40000000 txant=ANT2 loopa=0 loopb=0 qsk=0 dax=1 dax_clients=1 lock=0 tx=1 active=1 audio_level=100 audio_pan=51 audio_mute=1 record=0 play=disabled record_time=0.0 anf=0 anf_level=0 nr=0 nr_level=0 nb=0 nb_lev direct=1 el=50 wnb=0 wnb_level=100 apf=0 apf_level=0 squelch=1 squelch_level=20 diversity=0 diversity_parent=0 diversity_child=0 diversity_index=1342177293 ant_list=ANT1,ANT2,RX_A,RX_B,XVTA,XVTB mode_list=LSB,USB,AM,CW,DIGL,DIGU,SAM,FM,NFM,DFM,RTTY fm_tone_mode=OFF fm_tone_value=67.0 fm_repeater_offset_freq=0.000000 tx_offset_freq=0.000000 repeater_offset_dir=SIMPLEX fm_tone_burst=0 fm_deviation=5000 dfm_pre_de_emphasis=0 post_demod_low=300 post_demod_high=3300 rtty_mark=2125 rtty_shift=170 digl_offset=2210 digu_offset=1500 post_demod_bypass=0 rfgain=24  tx_ant_list=ANT1,ANT2,XVTA,XVTB
S67319A86|waveform installed_list=
R0|0|
*/


int smartsdr_set_freq(RIG *rig, vfo_t vfo, freq_t freq)
{
    struct smartsdr_priv_data *priv = (struct smartsdr_priv_data *)STATE(rig)->priv;
    char buf[4096];
    char cmd[64];
    ENTERFUNC;
    sprintf(cmd, "slice tune %d %.6f autopan=1", priv->slicenum, freq / 1e6);
    smartsdr_transaction(rig, cmd);
    rig_debug(RIG_DEBUG_VERBOSE, "%s: set_freq answer: %s", __func__, buf);
    rig_set_cache_freq(rig, vfo, freq);

    if (vfo == RIG_VFO_A)
    {
        priv->freqA = freq;
    }
    else
    {
        priv->freqB = freq;
    }

    RETURNFUNC(RIG_OK);
}

static int smartsdr_parse_S(RIG *rig, char *s)
{
    struct smartsdr_priv_data *priv = (struct smartsdr_priv_data *)STATE(rig)->priv;
    freq_t freq;
    char mode[16];
    char state[16];
    char *s2 = strdup(s);
    char *sep = "| \n";
    char *p = strtok(s2, sep);
    int gotFreq = 0, gotMode = 0;

    do
    {
        rig_debug(RIG_DEBUG_VERBOSE, "%s: parsing '%s'\n", __func__, p);

        if (sscanf(p, "RF_frequency=%lf", &freq) == 1)
        {
            priv->freqA = freq * 1e6;
            rig_debug(RIG_DEBUG_VERBOSE, "%s: got freq=%.0f\n", __func__, priv->freqA);
            gotFreq = 1;
            rig_set_cache_freq(rig, RIG_VFO_A, priv->freqA);
        }
        else if (sscanf(p, "filter_hi=%d\n", &priv->widthA) == 1)
        {
            rig_debug(RIG_DEBUG_VERBOSE, "%s: got width=%d\n", __func__, priv->widthA);
            rig_set_cache_mode(rig, RIG_VFO_A, priv->modeA, priv->widthA);
        }
        else if (sscanf(p, "mode=%s\n", mode) == 1)
        {
            if (strcmp(mode, "USB") == 0) { priv->modeA = RIG_MODE_USB; }
            else if (strcmp(mode, "LSB") == 0) { priv->modeA = RIG_MODE_LSB; }
            else if (strcmp(mode, "DIGU") == 0) { priv->modeA = RIG_MODE_PKTUSB; }
            else if (strcmp(mode, "DIGL") == 0) { priv->modeA = RIG_MODE_PKTLSB; }
            else if (strcmp(mode, "AM") == 0) { priv->modeA = RIG_MODE_AM; }
            else if (strcmp(mode, "CW") == 0) { priv->modeA = RIG_MODE_CW; }
            else if (strcmp(mode, "SAM") == 0) { priv->modeA = RIG_MODE_SAM; }
            else if (strcmp(mode, "FM") == 0) { priv->modeA = RIG_MODE_FM; }
            else if (strcmp(mode, "FMN") == 0) { priv->modeA = RIG_MODE_FMN; }
            else if (strcmp(mode, "RTTY") == 0) { priv->modeA = RIG_MODE_RTTY; }
            else
            {
                priv->modeA = RIG_MODE_NONE;
                rig_debug(RIG_DEBUG_ERR, "%s: unknown mode=%s\n", __func__, mode);
                return -RIG_EPROTO;
            }

            rig_set_cache_mode(rig, RIG_VFO_A, priv->modeA, priv->widthA);
            gotMode = 1;
            rig_debug(RIG_DEBUG_VERBOSE, "%s: got mode=%s\n", __func__,
                      rig_strrmode(priv->modeA));
        }
        else if (sscanf(p, "state=%s\n", state) == 1)
        {
            if (strcmp(state, "TRANSMITTING") == 0) { priv->ptt = 1; }
            else { priv->ptt = 0; }

            rig_debug(RIG_DEBUG_VERBOSE, "%s: PTT state=%s, ptt=%d\n", __func__, state,
                      priv->ptt);
        }
        else if (sscanf(p, "tx=%d\n", &priv->tx))
        {
            rig_debug(RIG_DEBUG_VERBOSE, "%s: tx=%d\n", __func__, priv->tx);
        }
    }
    while ((p = strtok(NULL, sep)));

    free(s2);

    rig_debug(RIG_DEBUG_VERBOSE, "%s gotFreq=%d, gotMode=%d\n", __func__, gotFreq,
              gotMode);
    return RIG_OK;
}

int smartsdr_get_freq(RIG *rig, vfo_t vfo, freq_t *freq)
{
    struct smartsdr_priv_data *priv = (struct smartsdr_priv_data *)STATE(rig)->priv;
    //char cmd[64];
    ENTERFUNC;
    //int retval = -RIG_EINTERNAL;
    // doing the sub slice causes audio problems
    //sprintf(cmd, "sub slice %d", priv->slicenum);
    //sprintf(cmd, "info");
    smartsdr_transaction(rig, NULL);

    if (vfo == RIG_VFO_A || vfo == RIG_VFO_CURR)
    {
        *freq = priv->freqA;
    }
    else
    {
        *freq = priv->freqB;
    }

    RETURNFUNC(RIG_OK);
}

int smartsdr_reset(RIG *rig, reset_t reset)
{
    return -RIG_ENIMPL;
}

int smartsdr_set_ptt(RIG *rig, vfo_t vfo, ptt_t ptt)
{
    struct smartsdr_priv_data *priv = (struct smartsdr_priv_data *)STATE(rig)->priv;
    char cmd[64];
    char slicechar[] = { '?','A','B','C','D','E','F','G','H' };
    ENTERFUNC;

    if (priv->ptt)
    {
        rig_debug(RIG_DEBUG_ERR, "%s: abort PTT on slice %c, another slice has PTT control\n", __func__, slicechar[priv->slicenum]);
        return -RIG_ENTARGET;
    }
    if (ptt)
    {
        sprintf(cmd, "dax audio set %d tx=1", priv->slicenum + 1);
        smartsdr_transaction(rig, cmd);
        rig_debug(RIG_DEBUG_VERBOSE, "%s: slice set answer: %s", __func__, cmd);
    }

    sprintf(cmd, "slice set %d tx=1", priv->slicenum);
    smartsdr_transaction(rig, cmd); 
    sprintf(cmd, "xmit %d", ptt);
    smartsdr_transaction(rig, cmd);
    if (!ptt) hl_usleep(100*1000); // need a little time for PTT to actually turn off
    priv->ptt = ptt;
    RETURNFUNC(RIG_OK);
}

int smartsdr_get_ptt(RIG *rig, vfo_t vfo, ptt_t *ptt)
{
    struct smartsdr_priv_data *priv = (struct smartsdr_priv_data *)STATE(rig)->priv;
    ENTERFUNC;
    smartsdr_transaction(rig, NULL);
    *ptt = 0;

    if (priv->tx)
    {
        *ptt = priv->ptt;
    }

    rig_debug(RIG_DEBUG_VERBOSE, "%s: ptt=%d\n", __func__, *ptt);
    RETURNFUNC(RIG_OK);
}

int smartsdr_set_mode(RIG *rig, vfo_t vfo, rmode_t mode, pbwidth_t width)
{
    struct smartsdr_priv_data *priv = (struct smartsdr_priv_data *)STATE(rig)->priv;
    char cmd[64];
    char *rmode = RIG_MODE_NONE;
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
    smartsdr_transaction(rig, cmd);

    if (width != RIG_PASSBAND_NOCHANGE)
    {
        sprintf(cmd, "filt %d 0 %ld", priv->slicenum, width);
    }

    RETURNFUNC(RIG_OK);
}

int smartsdr_get_mode(RIG *rig, vfo_t vfo, rmode_t *mode, pbwidth_t *width)
{
    struct smartsdr_priv_data *priv = (struct smartsdr_priv_data *)STATE(rig)->priv;
    ENTERFUNC;
    *mode = priv->modeA;
    *width = priv->widthA;
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
