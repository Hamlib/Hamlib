/*
*  Hamlib TCI 1.X backend - main file
*  Copyright (c) 2021 by Michael Black W9MDB
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
#include <string.h>             /* String function definitions */

#include <hamlib/rig.h>
#include <serial.h>
#include <misc.h>
#include <cal.h>
#include <token.h>
#include <register.h>
#include <network.h>

#include "dummy_common.h"

#define DEBUG 1
#define DEBUG_TRACE DEBUG_VERBOSE

#define MAXCMDLEN 8192
#define MAXBUFLEN 8192
#define MAXARGLEN 128
#define MAXBANDWIDTHLEN 4096

#define DEFAULTPATH "127.0.0.1:50001"

#define FALSE 0
#define TRUE (!FALSE)

#define TCI_VFOS (RIG_VFO_A|RIG_VFO_B)

#define TCI1X_MODES (RIG_MODE_USB | RIG_MODE_LSB | RIG_MODE_FM | RIG_MODE_AM)

#define TCI1X_LEVELS (RIG_LEVEL_AF | RIG_LEVEL_RF | RIG_LEVEL_MICGAIN | RIG_LEVEL_STRENGTH | RIG_LEVEL_RFPOWER_METER | RIG_LEVEL_RFPOWER_METER_WATTS | RIG_LEVEL_RFPOWER)

#define TCI1X_PARM (TOK_TCI1X_VERIFY_FREQ|TOK_TCI1X_VERIFY_PTT)

#define streq(s1,s2) (strcmp(s1,s2)==0)

static int tci1x_init(RIG *rig);
static int tci1x_open(RIG *rig);
static int tci1x_close(RIG *rig);
static int tci1x_cleanup(RIG *rig);
static int tci1x_set_freq(RIG *rig, vfo_t vfo, freq_t freq);
static int tci1x_get_freq(RIG *rig, vfo_t vfo, freq_t *freq);
static int tci1x_set_ptt(RIG *rig, vfo_t vfo, ptt_t ptt);
static int tci1x_set_mode(RIG *rig, vfo_t vfo, rmode_t mode, pbwidth_t width);
static int tci1x_set_split_mode(RIG *rig, vfo_t vfo, rmode_t mode,
                                pbwidth_t width);
static int tci1x_get_mode(RIG *rig, vfo_t vfo, rmode_t *mode, pbwidth_t *width);
static int tci1x_get_vfo(RIG *rig, vfo_t *vfo);
static int tci1x_set_vfo(RIG *rig, vfo_t vfo);
static int tci1x_set_ptt(RIG *rig, vfo_t vfo, ptt_t ptt);
static int tci1x_get_ptt(RIG *rig, vfo_t vfo, ptt_t *ptt);
static int tci1x_set_split_freq(RIG *rig, vfo_t vfo, freq_t tx_freq);
static int tci1x_get_split_freq(RIG *rig, vfo_t vfo, freq_t *tx_freq);
static int tci1x_set_split_vfo(RIG *rig, vfo_t vfo, split_t split,
                               vfo_t tx_vfo);
static int tci1x_get_split_vfo(RIG *rig, vfo_t vfo, split_t *split,
                               vfo_t *tx_vfo);
static int tci1x_set_split_freq_mode(RIG *rig, vfo_t vfo, freq_t freq,
                                     rmode_t mode, pbwidth_t width);
static int tci1x_get_split_freq_mode(RIG *rig, vfo_t vfo, freq_t *freq,
                                     rmode_t *mode, pbwidth_t *width);
#ifdef XXNOTIMPLEMENTED
static int tci1x_set_level(RIG *rig, vfo_t vfo, setting_t level, value_t val);
static int tci1x_get_level(RIG *rig, vfo_t vfo, setting_t level, value_t *val);

static int tci1x_set_ext_parm(RIG *rig, token_t token, value_t val);
static int tci1x_get_ext_parm(RIG *rig, token_t token, value_t *val);
#endif

static const char *tci1x_get_info(RIG *rig);
static int tci1x_power2mW(RIG *rig, unsigned int *mwpower, float power,
                          freq_t freq, rmode_t mode);
static int tci1x_mW2power(RIG *rig, float *power, unsigned int mwpower,
                          freq_t freq, rmode_t mode);

struct tci1x_priv_data
{
    vfo_t curr_vfo;
    char bandwidths[MAXBANDWIDTHLEN]; /* pipe delimited set returned from tci1x */
    int nbandwidths;
    char info[8192];
    ptt_t ptt;
    split_t split;
    rmode_t curr_modeA;
    rmode_t curr_modeB;
    freq_t curr_freqA;
    freq_t curr_freqB;
    pbwidth_t curr_widthA;
    pbwidth_t curr_widthB;
    int has_get_modeA; /* True if this function is available */
    int has_get_bwA; /* True if this function is available */
    int has_verify_cmds; // has the verify cmd in FLRig 1.3.54.1 or higher
    float powermeter_scale;  /* So we can scale power meter to 0-1 */
    value_t parms[RIG_SETTING_MAX];
    struct ext_list *ext_parms;
};

/* level's and parm's tokens */
#define TOK_TCI1X_VERIFY_FREQ    TOKEN_BACKEND(1)
#define TOK_TCI1X_VERIFY_PTT     TOKEN_BACKEND(2)

static const struct confparams tci1x_ext_parms[] =
{
    {
        TOK_TCI1X_VERIFY_FREQ, "VERIFY_FREQ", "Verify set_freq", "If true will verify set_freq otherwise is fire and forget", "0", RIG_CONF_CHECKBUTTON, {}
    },
    {
        TOK_TCI1X_VERIFY_PTT, "VERIFY_PTT", "Verify set_ptt", "If true will verify set_ptt otherwise set_ptt is fire and forget", "0", RIG_CONF_CHECKBUTTON, {}
    },
    { RIG_CONF_END, NULL, }
};

const struct rig_caps tci1x_caps =
{
    RIG_MODEL(RIG_MODEL_TCI1X),
    .model_name = "TCI1.X",
    .mfg_name = "Expert Elec",
    .version = "20211125.0",
    .copyright = "LGPL",
    .status = RIG_STATUS_ALPHA,
    .rig_type = RIG_TYPE_TRANSCEIVER,
    .targetable_vfo =  RIG_TARGETABLE_FREQ | RIG_TARGETABLE_MODE,
    .ptt_type = RIG_PTT_RIG,
    .port_type = RIG_PORT_SERIAL,
    .write_delay = 0,
    .post_write_delay = 0,
    .timeout = 1000,
    .retry = 1,

    .has_get_func = RIG_FUNC_NONE,
    .has_set_func = RIG_FUNC_NONE,
    .has_get_level = TCI1X_LEVELS,
    .has_set_level = RIG_LEVEL_SET(TCI1X_LEVELS),
    .has_get_parm =    TCI1X_PARM,
    .has_set_parm =    RIG_PARM_SET(TCI1X_PARM),

    .filters =  {
        {RIG_MODE_ALL, RIG_FLT_ANY},
        RIG_FLT_END
    },

    .rx_range_list1 = {{
            .startf = kHz(1), .endf = GHz(10), .modes = TCI1X_MODES,
            .low_power = -1, .high_power = -1, TCI_VFOS, RIG_ANT_1
        },
        RIG_FRNG_END,
    },
    .tx_range_list1 = {RIG_FRNG_END,},
    .rx_range_list2 = {{
            .startf = kHz(1), .endf = GHz(10), .modes = TCI1X_MODES,
            .low_power = -1, .high_power = -1, TCI_VFOS, RIG_ANT_1
        },
        RIG_FRNG_END,
    },
    .tx_range_list2 = {RIG_FRNG_END,},
    .tuning_steps =  { {TCI1X_MODES, 1}, {TCI1X_MODES, RIG_TS_ANY}, RIG_TS_END, },
    .priv = NULL,               /* priv */

    .extparms =     tci1x_ext_parms,

    .rig_init = tci1x_init,
    .rig_open = tci1x_open,
    .rig_close = tci1x_close,
    .rig_cleanup = tci1x_cleanup,

    .set_freq = tci1x_set_freq,
    .get_freq = tci1x_get_freq,
    .set_mode = tci1x_set_mode,
    .get_mode = tci1x_get_mode,
    .set_vfo = tci1x_set_vfo,
    .get_vfo = tci1x_get_vfo,
    .get_info =      tci1x_get_info,
    .set_ptt = tci1x_set_ptt,
    .get_ptt = tci1x_get_ptt,
    .set_split_mode = tci1x_set_split_mode,
    .set_split_freq = tci1x_set_split_freq,
    .get_split_freq = tci1x_get_split_freq,
    .set_split_vfo = tci1x_set_split_vfo,
    .get_split_vfo = tci1x_get_split_vfo,
    .set_split_freq_mode = tci1x_set_split_freq_mode,
    .get_split_freq_mode = tci1x_get_split_freq_mode,
#ifdef XXNOTIMPLEMENTED
    .set_level = tci1x_set_level,
    .get_level = tci1x_get_level,
    .set_ext_parm =  tci1x_set_ext_parm,
    .get_ext_parm =  tci1x_get_ext_parm,
#endif
    .power2mW =   tci1x_power2mW,
    .mW2power =   tci1x_mW2power,
    .hamlib_check_rig_caps = HAMLIB_CHECK_RIG_CAPS
};

//Structure for mapping tci1x dynmamic modes to hamlib modes
//tci1x displays modes as the rig displays them
struct s_modeMap
{
    rmode_t mode_hamlib;
    char *mode_tci1x;
};

//FLRig will provide us the modes for the selected rig
//We will then put them in this struct
static struct s_modeMap modeMap[] =
{
    {RIG_MODE_USB, NULL},
    {RIG_MODE_LSB, NULL},
    {RIG_MODE_PKTUSB, NULL},
    {RIG_MODE_PKTLSB, NULL},
    {RIG_MODE_AM, NULL},
    {RIG_MODE_FM, NULL},
    {RIG_MODE_FMN, NULL},
    {RIG_MODE_WFM, NULL},
    {RIG_MODE_CW, NULL},
    {RIG_MODE_CWR, NULL},
    {RIG_MODE_RTTY, NULL},
    {RIG_MODE_RTTYR, NULL},
    {RIG_MODE_C4FM, NULL},
    {0, NULL}
};

/*
* check_vfo
* No assumptions
*/
static int check_vfo(vfo_t vfo)
{
    switch (vfo)
    {
    case RIG_VFO_A:
        break;

    case RIG_VFO_TX:
    case RIG_VFO_B:
        break;

    case RIG_VFO_CURR:
        break;                  // will default to A in which_vfo

    default:
        return (FALSE);
    }

    return (TRUE);
}

/*
*
* read_transaction
* Assumes rig!=NULL, buf!=NULL, buf_len big enough to hold response
*/
static int read_transaction(RIG *rig, unsigned char *buf, int buf_len)
{
    int retry;
    struct rig_state *rs = &rig->state;
    char *delims = ";";

    ENTERFUNC;

    retry = 0;

    do
    {
        if (retry < 2)
        {
            rig_debug(RIG_DEBUG_WARN, "%s: retry needed? retry=%d\n", __func__, retry);
        }

        int len = read_string(&rs->rigport, buf, buf_len, delims,
                              strlen(delims), 0, 1);
        rig_debug(RIG_DEBUG_TRACE, "%s: string='%s'\n", __func__, buf);

        if (len <= 0)
        {
            rig_debug(RIG_DEBUG_ERR, "%s: read_string error=%d\n", __func__, len);
            continue;
        }

    }
    while (retry-- > 0 && strlen((char *) buf) == 0);

    if (retry == 0)
    {
        rig_debug(RIG_DEBUG_WARN, "%s: retry timeout\n", __func__);
        RETURNFUNC(-RIG_ETIMEOUT);
    }

    RETURNFUNC(RIG_OK);
}

/*
* write_transaction
* Assumes rig!=NULL, xml!=NULL, xml_len=total size of xml for response
*/
static int write_transaction(RIG *rig, unsigned char *buf, int buf_len)
{

    int try = rig->caps->retry;

    int retval = -RIG_EPROTO;

    struct rig_state *rs = &rig->state;

    ENTERFUNC;

    // This shouldn't ever happen...but just in case
    // We need to avoid an empty write as rigctld replies with blank line
    if (buf_len == 0)
    {
        rig_debug(RIG_DEBUG_ERR, "%s: len==0??\n", __func__);
        RETURNFUNC(retval);
    }

    // appears we can lose sync if we don't clear things out
    // shouldn't be anything for us now anyways
    rig_flush(&rig->state.rigport);

    while (try-- >= 0 && retval != RIG_OK)
        {
            retval = write_block(&rs->rigport, buf, buf_len);

            if (retval  < 0)
            {
                RETURNFUNC(-RIG_EIO);
            }
        }

    RETURNFUNC(retval);
}

static int tci1x_transaction(RIG *rig, char *cmd, char *cmd_arg, char *value,
                             int value_len)
{
    int retry = 0;
    unsigned char frame[1024];

    ENTERFUNC;

    memset(frame, 0, sizeof(frame));

    if (value)
    {
        value[0] = 0;
    }

    frame[0] = 0x81;
    frame[1] = strlen(cmd);
    frame[2] = 0x00;
    frame[3] = 0x00;
    frame[4] = 0x00;
    frame[5] = 0x00;
    frame[6] = 0x00;
    frame[7] = 0x00;
    frame[8] = 0x00;
    frame[9] = 0x00;
    frame[10] = 0x00;
    frame[11] = 0x00;
    strcat((char *) &frame[12], cmd);

    do
    {
        int retval;

        if (retry < 2)
        {
            rig_debug(RIG_DEBUG_VERBOSE, "%s: cmd=%s, retry=%d\n", __func__, cmd, retry);
        }

        retval = write_transaction(rig, frame, strlen(cmd) + 12);

        if (retval != RIG_OK)
        {
            rig_debug(RIG_DEBUG_ERR, "%s: write_transaction error=%d\n", __func__, retval);

            // if we get RIG_EIO the socket has probably disappeared
            // so bubble up the error so port can re re-opened
            if (retval == -RIG_EIO) { RETURNFUNC(retval); }

            hl_usleep(50 * 1000); // 50ms sleep if error
        }

        read_transaction(rig, (unsigned char *) value, value_len);

        rig_debug(RIG_DEBUG_VERBOSE, "%s: value=%s\n", __func__, value);

    }
    while ((value && (strlen(value) == 0))
            && retry--); // we'll do retries if needed

    if (value && strlen(value) == 0) { RETURNFUNC(RIG_EPROTO); }

    RETURNFUNC(RIG_OK);
}

/*
* tci1x_init
* Assumes rig!=NULL
*/
static int tci1x_init(RIG *rig)
{
    struct tci1x_priv_data *priv;

    ENTERFUNC;
    rig_debug(RIG_DEBUG_TRACE, "%s version %s\n", __func__, rig->caps->version);

    rig->state.priv  = (struct tci1x_priv_data *)calloc(1, sizeof(
                           struct tci1x_priv_data));

    if (!rig->state.priv)
    {
        RETURNFUNC(-RIG_ENOMEM);
    }

    priv = rig->state.priv;

    memset(priv, 0, sizeof(struct tci1x_priv_data));
    memset(priv->parms, 0, RIG_SETTING_MAX * sizeof(value_t));

    /*
     * set arbitrary initial status
     */
    rig->state.current_vfo = RIG_VFO_A;
    priv->split = 0;
    priv->ptt = 0;
    priv->curr_modeA = -1;
    priv->curr_modeB = -1;
    priv->curr_widthA = -1;
    priv->curr_widthB = -1;

    if (!rig->caps)
    {
        RETURNFUNC(-RIG_EINVAL);
    }

    strncpy(rig->state.rigport.pathname, DEFAULTPATH,
            sizeof(rig->state.rigport.pathname));

    priv->ext_parms = alloc_init_ext(tci1x_ext_parms);

    if (!priv->ext_parms)
    {
        RETURNFUNC(-RIG_ENOMEM);
    }


    RETURNFUNC(RIG_OK);
}

/*
* modeMapGetTCI
* Assumes mode!=NULL
* Return the string for TCI for the given hamlib mode
*/
static const char *modeMapGetTCI(rmode_t modeHamlib)
{
    int i;

    rig_debug(RIG_DEBUG_TRACE, "%s: called\n", __func__);

    for (i = 0; modeMap[i].mode_hamlib != 0; ++i)
    {
        if (modeMap[i].mode_tci1x == NULL) { continue; }

        rig_debug(RIG_DEBUG_TRACE,
                  "%s: checking modeMap[%d]=%.0f to modeHamlib=%.0f, mode_tci1x='%s'\n", __func__,
                  i, (double)modeMap[i].mode_hamlib, (double)modeHamlib, modeMap[i].mode_tci1x);

        if (modeMap[i].mode_hamlib == modeHamlib && strlen(modeMap[i].mode_tci1x) > 0)
        {
            rig_debug(RIG_DEBUG_TRACE, "%s matched mode=%.0f, returning '%s'\n", __func__,
                      (double)modeHamlib, modeMap[i].mode_tci1x);
            return (modeMap[i].mode_tci1x);
        }
    }

    rig_debug(RIG_DEBUG_ERR, "%s: FlRig does not have mode: %s\n", __func__,
              rig_strrmode(modeHamlib));
    return ("ERROR");
}

/*
* modeMapGetHamlib
* Assumes mode!=NULL
* Return the hamlib mode from the given TCI string
*/
static rmode_t modeMapGetHamlib(const char *modeTCI)
{
    int i;
    char modeTCICheck[MAXBUFLEN + 2];

    SNPRINTF(modeTCICheck, sizeof(modeTCICheck), "|%s|", modeTCI);

    for (i = 0; modeMap[i].mode_hamlib != 0; ++i)
    {
        rig_debug(RIG_DEBUG_TRACE, "%s: find '%s' in '%s'\n", __func__,
                  modeTCICheck, modeMap[i].mode_tci1x);

        if (modeMap[i].mode_tci1x
                && strcmp(modeMap[i].mode_tci1x, modeTCICheck) == 0)
        {
            return (modeMap[i].mode_hamlib);
        }
    }

    rig_debug(RIG_DEBUG_TRACE, "%s: mode requested: %s, not in modeMap\n", __func__,
              modeTCI);
    return (RIG_MODE_NONE);
}


/*
* modeMapAdd
* Assumes modes!=NULL
*/
static void modeMapAdd(rmode_t *modes, rmode_t mode_hamlib, char *mode_tci1x)
{
    int i;
    int len1;

    rig_debug(RIG_DEBUG_TRACE, "%s:mode_tci1x=%s\n", __func__, mode_tci1x);

    // if we already have it just return
    // We get ERROR if the mode is not known so non-ERROR is OK
    if (modeMapGetHamlib(mode_tci1x) != RIG_MODE_NONE) { return; }

    len1 = strlen(mode_tci1x) + 3; /* bytes needed for allocating */

    for (i = 0; modeMap[i].mode_hamlib != 0; ++i)
    {
        if (modeMap[i].mode_hamlib == mode_hamlib)
        {
            int len2;
            *modes |= modeMap[i].mode_hamlib;

            /* we will pipe delimit all the entries for easier matching */
            /* all entries will have pipe symbol on both sides */
            if (modeMap[i].mode_tci1x == NULL)
            {
                modeMap[i].mode_tci1x = calloc(1, len1);

                if (modeMap[i].mode_tci1x == NULL)
                {
                    rig_debug(RIG_DEBUG_ERR, "%s: error allocating memory for modeMap\n",
                              __func__);
                    return;
                }
            }

            len2 = strlen(modeMap[i].mode_tci1x); /* current len w/o null */
            modeMap[i].mode_tci1x = realloc(modeMap[i].mode_tci1x,
                                            strlen(modeMap[i].mode_tci1x) + len1);

            if (strlen(modeMap[i].mode_tci1x) == 0) { modeMap[i].mode_tci1x[0] = '|'; }

            strncat(modeMap[i].mode_tci1x, mode_tci1x, len1 + len2);
            strncat(modeMap[i].mode_tci1x, "|", len1 + len2);
            rig_debug(RIG_DEBUG_TRACE, "%s: Adding mode=%s, index=%d, result=%s\n",
                      __func__, mode_tci1x, i, modeMap[i].mode_tci1x);
            return;
        }
    }
}

/*
* tci1x_open
* Assumes rig!=NULL, rig->state.priv!=NULL
*/
static int tci1x_open(RIG *rig)
{
    int retval;
    int trx_count = 0;
    char value[MAXBUFLEN];
    char arg[MAXBUFLEN];
    rmode_t modes;
    char *p;
    char *pr;
    //struct tci1x_priv_data *priv = (struct tci1x_priv_data *) rig->state.priv;

    rig_debug(RIG_DEBUG_VERBOSE, "%s: version %s\n", __func__, rig->caps->version);
    char *websocket =
        "GET / HTTP/1.1\r\nHost: localhost:50001\r\nUpgrade: websocket\r\nConnection: Upgrade\r\nSec-WebSocket-Key: TnwnvtFT6akIBYQC7nh3vA==\r\nSec-WebSocket-Version: 13\r\n\r\n";

    write_transaction(rig, (unsigned char *) websocket, strlen(websocket));

    do
    {
        retval = read_transaction(rig, (unsigned char *) value, sizeof(value));
        rig_debug(RIG_DEBUG_VERBOSE, "%s: value=%s\n", __func__, value);
    }
    while (retval == RIG_OK && strlen(value) > 0);

    retval = tci1x_transaction(rig, "device;", NULL, value, sizeof(value));
    dump_hex((unsigned char *)value, strlen(value));

    if (retval != RIG_OK)
    {
        rig_debug(RIG_DEBUG_ERR, "%s: DEVICE failed: %s\n", __func__,
                  rigerror(retval));
        // we fall through and assume old version
    }

    sscanf(&value[2], "device:%s", value);

    rig_debug(RIG_DEBUG_VERBOSE, "%s: TCI Device is %s\n", __func__, arg);

    // Receive only
    retval = tci1x_transaction(rig, "receive_only;", NULL, value, sizeof(value));

    if (retval != RIG_OK)
    {
        rig_debug(RIG_DEBUG_ERR, "%s: RECEIVE_ONLY failed: %s\n", __func__,
                  rigerror(retval));
    }

    sscanf(&value[2], "receive_only:%s", value);
    rig_debug(RIG_DEBUG_VERBOSE, "%s: readonly is %s\n", __func__, arg);

    // TRX count
    retval = tci1x_transaction(rig, "trx_count;", NULL, value, sizeof(value));

    if (retval != RIG_OK)
    {
        rig_debug(RIG_DEBUG_ERR, "%s: TRX_COUNT failed..not fatal: %s\n", __func__,
                  rigerror(retval));
    }

    sscanf(&value[2], "trx_count:%d", &trx_count);
    rig_debug(RIG_DEBUG_VERBOSE, "Trx count=%d\n", trx_count);

    freq_t freq;
    retval = tci1x_get_freq(rig, RIG_VFO_CURR, &freq);

    if (retval != RIG_OK)
    {
        rig_debug(RIG_DEBUG_ERR, "%s: tci1x_get_freq not working!!\n", __func__);
    }

    rig->state.current_vfo = RIG_VFO_A;
    rig_debug(RIG_DEBUG_TRACE, "%s: currvfo=%s value=%s\n", __func__,
              rig_strvfo(rig->state.current_vfo), value);
    //tci1x_get_split_vfo(rig, vfo, &priv->split, &vfo_tx);
    RETURNFUNC2(RIG_OK);

    /* find out available widths and modes */
    retval = tci1x_transaction(rig, "modulations_list;", NULL, value,
                               sizeof(value));

    if (retval != RIG_OK) { RETURNFUNC2(retval); }

    sscanf(&value[2], "modulations_list:%s", arg);
    rig_debug(RIG_DEBUG_VERBOSE, "%s: modes=%s\n", __func__, arg);
    modes = 0;
    pr = value;

    /* The following modes in TCI are not implemented yet
        A1A
        AM-2
        AM6.0
        AM-D1 -- doesn't appear to be read/set
        AM-D2 -- doesn't appear to be read/set
        AM-D3 -- doesn't appear to be read/set
        AMW -- don't have mode in rig.h
        CW2.4 -- could be CW
        CW500 -- could be CWN but CWN not in rig.h
        CW-N -- could be CWN but CWN not in rig.h
        CWN -- dcould be CWN but CWN not in rig.h
        CW-NR -- don't have mode in rig.h
        DATA2-LSB
        DV
        DV-R
        F1B
        FM-D1 -- doesn't appear to be read/set
        FM-D2 -- doesn't appear to be read/set
        FM-D3 -- doesn't appear to be read/set
        H3E
        M11
        USB-D -- doesn't appear to be read/set
        USB-D1 -- doesn't appear to be read/set
        USB-D2 -- doesn't appear to be read/set
        USB-D3 -- doesn't appear to be read/set
        USER-L -- doesn't appear to be read/set
        USER-U -- doesn't appear to be read/set
    */

    for (p = strtok_r(value, ",", &pr); p != NULL; p = strtok_r(NULL, ",", &pr))
    {
        if (streq(p, "AM-D")) { modeMapAdd(&modes, RIG_MODE_PKTAM, p); }
        else if (streq(p, "AM")) { modeMapAdd(&modes, RIG_MODE_AM, p); }
        else if (streq(p, "AM-N")) { modeMapAdd(&modes, RIG_MODE_AMN, p); }
        else if (streq(p, "AMN")) { modeMapAdd(&modes, RIG_MODE_AMN, p); }
        else if (streq(p, "CW")) { modeMapAdd(&modes, RIG_MODE_CW, p); }
        else if (streq(p, "CW-L")) { modeMapAdd(&modes, RIG_MODE_CWR, p); }
        else if (streq(p, "CW-LSB")) { modeMapAdd(&modes, RIG_MODE_CWR, p); }
        else if (streq(p, "CW-R")) { modeMapAdd(&modes, RIG_MODE_CWR, p); }
        else if (streq(p, "CW-U")) { modeMapAdd(&modes, RIG_MODE_CW, p); }
        else if (streq(p, "CW-USB")) { modeMapAdd(&modes, RIG_MODE_CW, p); }
        else if (streq(p, "CWL")) { modeMapAdd(&modes, RIG_MODE_CWR, p); }
        else if (streq(p, "CWU")) { modeMapAdd(&modes, RIG_MODE_CW, p); }
        else if (streq(p, "D-LSB")) { modeMapAdd(&modes, RIG_MODE_PKTLSB, p); }
        else if (streq(p, "D-USB")) { modeMapAdd(&modes, RIG_MODE_PKTUSB, p); }
        else if (streq(p, "DATA")) { modeMapAdd(&modes, RIG_MODE_PKTUSB, p); }
        else if (streq(p, "DATA-FM")) { modeMapAdd(&modes, RIG_MODE_PKTFM, p); }
        else if (streq(p, "DATA-L")) { modeMapAdd(&modes, RIG_MODE_PKTLSB, p); }
        else if (streq(p, "DATA-R")) { modeMapAdd(&modes, RIG_MODE_PKTLSB, p); }
        else if (streq(p, "DATA-LSB")) { modeMapAdd(&modes, RIG_MODE_PKTLSB, p); }
        else if (streq(p, "DATA-USB")) { modeMapAdd(&modes, RIG_MODE_PKTUSB, p); }
        else if (streq(p, "DATA-U")) { modeMapAdd(&modes, RIG_MODE_PKTUSB, p); }
        else if (streq(p, "DIG")) { modeMapAdd(&modes, RIG_MODE_PKTUSB, p); }
        else if (streq(p, "DIGI")) { modeMapAdd(&modes, RIG_MODE_PKTUSB, p); }
        else if (streq(p, "DIGL")) { modeMapAdd(&modes, RIG_MODE_PKTLSB, p); }
        else if (streq(p, "DIGU")) { modeMapAdd(&modes, RIG_MODE_PKTUSB, p); }
        else if (streq(p, "DSB")) { modeMapAdd(&modes, RIG_MODE_DSB, p); }
        else if (streq(p, "FM")) { modeMapAdd(&modes, RIG_MODE_FM, p); }
        else if (streq(p, "FM-D")) { modeMapAdd(&modes, RIG_MODE_PKTFM, p); }
        else if (streq(p, "FMN")) { modeMapAdd(&modes, RIG_MODE_FMN, p); }
        else if (streq(p, "FM-N")) { modeMapAdd(&modes, RIG_MODE_FMN, p); }
        else if (streq(p, "FMW")) { modeMapAdd(&modes, RIG_MODE_WFM, p); }
        else if (streq(p, "FSK")) { modeMapAdd(&modes, RIG_MODE_RTTY, p); }
        else if (streq(p, "FSK-R")) { modeMapAdd(&modes, RIG_MODE_RTTYR, p); }
        else if (streq(p, "LCW")) { modeMapAdd(&modes, RIG_MODE_CWR, p); }
        else if (streq(p, "LSB")) { modeMapAdd(&modes, RIG_MODE_LSB, p); }
        else if (streq(p, "LSB-D")) { modeMapAdd(&modes, RIG_MODE_PKTLSB, p); }
        else if (streq(p, "LSB-D1")) { modeMapAdd(&modes, RIG_MODE_PKTLSB, p); }
        else if (streq(p, "LSB-D2")) { modeMapAdd(&modes, RIG_MODE_PKTLSB, p); }
        else if (streq(p, "LSB-D3")) { modeMapAdd(&modes, RIG_MODE_PKTLSB, p); }
        else if (streq(p, "NFM")) { modeMapAdd(&modes, RIG_MODE_FMN, p); }
        else if (streq(p, "PKT")) { modeMapAdd(&modes, RIG_MODE_RTTY, p); }
        else if (streq(p, "PKT-FM")) { modeMapAdd(&modes, RIG_MODE_PKTFM, p); }
        else if (streq(p, "PKT-L")) { modeMapAdd(&modes, RIG_MODE_RTTYR, p); }
        else if (streq(p, "PKT-U")) { modeMapAdd(&modes, RIG_MODE_RTTY, p); }
        else if (streq(p, "PKT(L)")) { modeMapAdd(&modes, RIG_MODE_RTTYR, p); }
        else if (streq(p, "PKT(U)")) { modeMapAdd(&modes, RIG_MODE_RTTY, p); }
        else if (streq(p, "PSK")) { modeMapAdd(&modes, RIG_MODE_RTTY, p); }
        else if (streq(p, "PSK-L")) { modeMapAdd(&modes, RIG_MODE_RTTYR, p); }
        else if (streq(p, "PSK-R")) { modeMapAdd(&modes, RIG_MODE_RTTYR, p); }
        else if (streq(p, "PSK-U")) { modeMapAdd(&modes, RIG_MODE_RTTY, p); }
        else if (streq(p, "RTTY")) { modeMapAdd(&modes, RIG_MODE_RTTY, p); }
        else if (streq(p, "RTTY-L")) { modeMapAdd(&modes, RIG_MODE_RTTYR, p); }
        else if (streq(p, "RTTY-R")) { modeMapAdd(&modes, RIG_MODE_RTTYR, p); }
        else if (streq(p, "RTTY-U")) { modeMapAdd(&modes, RIG_MODE_RTTY, p); }
        else if (streq(p, "RTTY(U)")) { modeMapAdd(&modes, RIG_MODE_RTTY, p); }
        else if (streq(p, "RTTY(R")) { modeMapAdd(&modes, RIG_MODE_RTTYR, p); }
        else if (streq(p, "SAH")) { modeMapAdd(&modes, RIG_MODE_SAH, p); }
        else if (streq(p, "SAL")) { modeMapAdd(&modes, RIG_MODE_SAL, p); }
        else if (streq(p, "SAM")) { modeMapAdd(&modes, RIG_MODE_SAM, p); }
        else if (streq(p, "USB")) { modeMapAdd(&modes, RIG_MODE_USB, p); }
        else if (streq(p, "USB-D")) { modeMapAdd(&modes, RIG_MODE_PKTUSB, p); }
        else if (streq(p, "USB-D1")) { modeMapAdd(&modes, RIG_MODE_PKTUSB, p); }
        else if (streq(p, "USB-D2")) { modeMapAdd(&modes, RIG_MODE_PKTUSB, p); }
        else if (streq(p, "USB-D3")) { modeMapAdd(&modes, RIG_MODE_PKTUSB, p); }
        else if (streq(p, "USER-U")) { modeMapAdd(&modes, RIG_MODE_PKTUSB, p); }
        else if (streq(p, "USER-L")) { modeMapAdd(&modes, RIG_MODE_PKTLSB, p); }
        else if (streq(p, "W-FM")) { modeMapAdd(&modes, RIG_MODE_WFM, p); }
        else if (streq(p, "WFM")) { modeMapAdd(&modes, RIG_MODE_WFM, p); }
        else if (streq(p, "UCW")) { modeMapAdd(&modes, RIG_MODE_CW, p); }
        else if (streq(p, "C4FM")) { modeMapAdd(&modes, RIG_MODE_C4FM, p); }
        else if (streq(p, "SPEC")) { modeMapAdd(&modes, RIG_MODE_SPEC, p); }
        else if (streq(p, "DRM")) // we don't support DRM yet (or maybe ever)
        {
            rig_debug(RIG_DEBUG_VERBOSE, "%s: no mapping for mode %s\n", __func__, p);
        }
        else { rig_debug(RIG_DEBUG_ERR, "%s: Unknown mode (new?) for this rig='%s'\n", __func__, p); }
    }

    rig->state.mode_list = modes;

    retval = rig_strrmodes(modes, value, sizeof(value));

    if (retval != RIG_OK)   // we might get TRUNC but we can still print the debug
    {
        rig_debug(RIG_DEBUG_VERBOSE, "%s: %s\n", __func__, rigerror(retval));
    }

    rig_debug(RIG_DEBUG_VERBOSE, "%s: hamlib modes=%s\n", __func__, value);

    RETURNFUNC2(retval);
}

/*
* tci1x_close
* Assumes rig!=NULL
*/
static int tci1x_close(RIG *rig)
{
    ENTERFUNC;

    RETURNFUNC(RIG_OK);
}

/*
* tci1x_cleanup
* Assumes rig!=NULL, rig->state.priv!=NULL
*/
static int tci1x_cleanup(RIG *rig)
{
    struct tci1x_priv_data *priv;

    ENTERFUNC;

    if (!rig)
    {
        RETURNFUNC(-RIG_EINVAL);
    }

    priv = (struct tci1x_priv_data *)rig->state.priv;

    free(priv->ext_parms);
    free(rig->state.priv);

    rig->state.priv = NULL;

    RETURNFUNC(RIG_OK);
}

/*
* tci1x_get_freq
* Assumes rig!=NULL, rig->state.priv!=NULL, freq!=NULL
*/
static int tci1x_get_freq(RIG *rig, vfo_t vfo, freq_t *freq)
{
    char value[MAXARGLEN];
    struct tci1x_priv_data *priv = (struct tci1x_priv_data *) rig->state.priv;

    ENTERFUNC;
    rig_debug(RIG_DEBUG_TRACE, "%s: vfo=%s\n", __func__,
              rig_strvfo(vfo));


    if (check_vfo(vfo) == FALSE)
    {
        rig_debug(RIG_DEBUG_ERR, "%s: unsupported VFO %s\n",
                  __func__, rig_strvfo(vfo));
        RETURNFUNC(-RIG_EINVAL);
    }

    if (vfo == RIG_VFO_CURR)
    {
        vfo = rig->state.current_vfo;
        rig_debug(RIG_DEBUG_TRACE, "%s: get_freq2 vfo=%s\n",
                  __func__, rig_strvfo(vfo));
    }

    char *cmd = vfo == RIG_VFO_A ? "vfo:0:0;" : "vfo:0:1:";
    int retval;
    int n;

    retval = tci1x_transaction(rig, cmd, NULL, value, sizeof(value));

    if (retval != RIG_OK)
    {
        rig_debug(RIG_DEBUG_ERR, "%s: tci1x_transaction failed retval=%s\n", __func__,
                  rigerror(retval));
        RETURNFUNC(retval);
    }

    n = sscanf(&value[2], "vfo:%*d,%*d,%lf", freq);
    rig_debug(RIG_DEBUG_VERBOSE, "%s: got '%s', scanned %d items\n", __func__,
              value, n);

    if (*freq == 0)
    {
        rig_debug(RIG_DEBUG_ERR, "%s: freq==0??\nvalue=%s\n", __func__,
                  value);
        RETURNFUNC(-RIG_EPROTO);
    }
    else
    {
        rig_debug(RIG_DEBUG_TRACE, "%s: freq=%.0f\n", __func__, *freq);
    }

    if (vfo == RIG_VFO_A)
    {
        priv->curr_freqA = *freq;
    }
    else
    {
        priv->curr_freqB = *freq;
    }

    RETURNFUNC(RIG_OK);
}

/*
* tci1x_set_freq
* assumes rig!=NULL, rig->state.priv!=NULL
*/
static int tci1x_set_freq(RIG *rig, vfo_t vfo, freq_t freq)
{
    int retval;
    char cmd_arg[MAXARGLEN];
    char *cmd;
    struct tci1x_priv_data *priv = (struct tci1x_priv_data *) rig->state.priv;

    ENTERFUNC;
    rig_debug(RIG_DEBUG_TRACE, "%s: vfo=%s freq=%.0f\n", __func__,
              rig_strvfo(vfo), freq);

    if (check_vfo(vfo) == FALSE)
    {
        rig_debug(RIG_DEBUG_ERR, "%s: unsupported VFO %s\n",
                  __func__, rig_strvfo(vfo));
        RETURNFUNC(-RIG_EINVAL);
    }

    if (vfo == RIG_VFO_CURR)
    {
        vfo = rig->state.current_vfo;
    }
    else if (vfo == RIG_VFO_TX && priv->split)
    {
        vfo = RIG_VFO_B; // if split always TX on VFOB
    }

    SNPRINTF(cmd_arg, sizeof(cmd_arg),
             "<params><param><value><double>%.0f</double></value></param></params>", freq);

    value_t val;
    rig_get_ext_parm(rig, TOK_TCI1X_VERIFY_FREQ, &val);
    rig_debug(RIG_DEBUG_VERBOSE, "%s: set_verify_vfoA/B=%d\n", __func__, val.i);

    if (vfo == RIG_VFO_A)
    {
        cmd = "rig.set_vfoA";

        if (val.i) { cmd = "rig.set_verify_vfoA"; }

        rig_debug(RIG_DEBUG_TRACE, "%s %.0f\n", cmd, freq);
        priv->curr_freqA = freq;
    }
    else
    {
        cmd = "rig.set_vfoB";

        if (val.i) { cmd = "rig.set_verify_vfoB"; }

        rig_debug(RIG_DEBUG_TRACE, "%s %.0f\n", cmd, freq);
        priv->curr_freqB = freq;
    }

    retval = tci1x_transaction(rig, cmd, cmd_arg, NULL, 0);

    if (retval != RIG_OK)
    {
        RETURNFUNC(retval);
    }

    RETURNFUNC(RIG_OK);
}

/*
* tci1x_set_ptt
* Assumes rig!=NULL
*/
static int tci1x_set_ptt(RIG *rig, vfo_t vfo, ptt_t ptt)
{
    int retval;
    char cmd_arg[MAXARGLEN];
    struct tci1x_priv_data *priv = (struct tci1x_priv_data *) rig->state.priv;

    ENTERFUNC;
    rig_debug(RIG_DEBUG_TRACE, "%s: ptt=%d\n", __func__, ptt);


    if (check_vfo(vfo) == FALSE)
    {
        rig_debug(RIG_DEBUG_ERR, "%s: unsupported VFO %s\n",
                  __func__, rig_strvfo(vfo));
        RETURNFUNC(-RIG_EINVAL);
    }

    SNPRINTF(cmd_arg, sizeof(cmd_arg),
             "<params><param><value><i4>%d</i4></value></param></params>",
             ptt);

    value_t val;
    char *cmd = "rig.set_ptt";
    rig_get_ext_parm(rig, TOK_TCI1X_VERIFY_FREQ, &val);
    rig_debug(RIG_DEBUG_VERBOSE, "%s: fast_set_ptt=%d\n", __func__, val.i);

    if (val.i) { cmd = "rig.set_ptt_fast"; }

    retval = tci1x_transaction(rig, cmd, cmd_arg, NULL, 0);

    if (retval != RIG_OK)
    {
        RETURNFUNC(retval);
    }

    priv->ptt = ptt;

    RETURNFUNC(RIG_OK);
}

/*
* tci1x_get_ptt
* Assumes rig!=NUL, ptt!=NULL
*/
static int tci1x_get_ptt(RIG *rig, vfo_t vfo, ptt_t *ptt)
{
    char value[MAXCMDLEN];
    struct tci1x_priv_data *priv = (struct tci1x_priv_data *) rig->state.priv;

    ENTERFUNC;
    rig_debug(RIG_DEBUG_TRACE, "%s: vfo=%s\n", __func__,
              rig_strvfo(vfo));

    int retval;

    retval = tci1x_transaction(rig, "rig.get_ptt", NULL, value, sizeof(value));

    if (retval != RIG_OK)
    {
        RETURNFUNC(retval);
    }

    *ptt = atoi(value);
    rig_debug(RIG_DEBUG_TRACE, "%s: '%s'\n", __func__, value);

    priv->ptt = *ptt;

    RETURNFUNC(RIG_OK);
}

/*
* tci1x_set_split_mode
* Assumes rig!=NULL
*/
static int tci1x_set_split_mode(RIG *rig, vfo_t vfo, rmode_t mode,
                                pbwidth_t width)
{
    int retval;
    struct tci1x_priv_data *priv = (struct tci1x_priv_data *) rig->state.priv;

    ENTERFUNC;
    rig_debug(RIG_DEBUG_TRACE, "%s: vfo=%s mode=%s width=%d\n",
              __func__, rig_strvfo(vfo), rig_strrmode(mode), (int)width);

    switch (vfo)
    {
    case RIG_VFO_CURR:
        vfo = rig->state.current_vfo;
        break;

    case RIG_VFO_TX:
        vfo = RIG_VFO_B;
        break;
    }

    // If no change don't do it...modes are kept up to date by client calls
    // to get_mode and set_mode so should be current here
    rig_debug(RIG_DEBUG_TRACE, "%s: vfoa privmode=%s\n", __func__,
              rig_strrmode(priv->curr_modeA));
    rig_debug(RIG_DEBUG_TRACE, "%s: vfob privmode=%s\n", __func__,
              rig_strrmode(priv->curr_modeB));

    // save some VFO swapping .. may replace with VFO specific calls that won't cause VFO change
    if (vfo == RIG_VFO_A && mode == priv->curr_modeA) { RETURNFUNC(RIG_OK); }

    if (vfo == RIG_VFO_B && mode == priv->curr_modeB) { RETURNFUNC(RIG_OK); }

    retval = tci1x_set_mode(rig, vfo, mode, width);
    rig_debug(RIG_DEBUG_TRACE, "%s: set mode=%s\n", __func__,
              rig_strrmode(mode));
    RETURNFUNC(retval);
}

/*
* tci1x_set_mode
* Assumes rig!=NULL
*/
static int tci1x_set_mode(RIG *rig, vfo_t vfo, rmode_t mode, pbwidth_t width)
{
    int retval;
    int needBW;
    int vfoSwitched;
    char cmd_arg[MAXCMDLEN];
    char *p;
    char *pttmode;
    char *ttmode;
    struct tci1x_priv_data *priv = (struct tci1x_priv_data *) rig->state.priv;

    ENTERFUNC;
    rig_debug(RIG_DEBUG_TRACE, "%s: vfo=%s mode=%s width=%d\n",
              __func__, rig_strvfo(vfo), rig_strrmode(mode), (int)width);


    // if ptt is on do not set mode
    if (priv->ptt)
    {
        rig_debug(RIG_DEBUG_TRACE, "%s: returning because priv->ptt=%d\n", __func__,
                  (int)priv->ptt);
        RETURNFUNC(RIG_OK);
    }

    if (vfo == RIG_VFO_CURR)
    {
        vfo = rig->state.current_vfo;
    }

    if (check_vfo(vfo) == FALSE)
    {
        rig_debug(RIG_DEBUG_ERR, "%s: unsupported VFO %s\n",
                  __func__, rig_strvfo(vfo));
        RETURNFUNC(-RIG_EINVAL);
    }

    if (priv->ptt)
    {
        rig_debug(RIG_DEBUG_VERBOSE, "%s set_mode call not made as PTT=1\n", __func__);
        RETURNFUNC(RIG_OK);  // just return OK and ignore this
    }

    // Switch to VFOB if appropriate since we can't set mode directly
    // MDB
    vfoSwitched = 0;
    rig_debug(RIG_DEBUG_TRACE, "%s: curr_vfo = %s\n", __func__,
              rig_strvfo(rig->state.current_vfo));

    // If we don't have the get_bwA call we have to switch VFOs ourself
    if (!priv->has_get_bwA && vfo == RIG_VFO_B
            && rig->state.current_vfo != RIG_VFO_B)
    {
        vfoSwitched = 1;
        rig_debug(RIG_DEBUG_TRACE, "%s: switch to VFOB = %d\n", __func__,
                  vfoSwitched);
    }

    if (vfoSwitched)   // swap to B and we'll swap back later
    {
        rig_debug(RIG_DEBUG_TRACE, "%s: switching to VFOB = %d\n", __func__,
                  vfoSwitched);
        retval = tci1x_set_vfo(rig, RIG_VFO_B);

        if (retval < 0)
        {
            RETURNFUNC(retval);
        }
    }

    // Set the mode
    if (modeMapGetTCI(mode))
    {
        ttmode = strdup(modeMapGetTCI(mode));
    }
    else
    {
        rig_debug(RIG_DEBUG_ERR, "%s: modeMapGetFlRig failed on mode=%d\n", __func__,
                  (int)mode);
        RETURNFUNC(-RIG_EINVAL);
    }

    rig_debug(RIG_DEBUG_TRACE, "%s: got ttmode = %s\n", __func__,
              ttmode == NULL ? "NULL" : ttmode);

    if (ttmode == NULL)
    {
        rig_debug(RIG_DEBUG_ERR, "%s: strdup failed\n", __func__);
        RETURNFUNC(-RIG_EINTERNAL);
    }

//   if (strncmp(ttmode,"ERROR",5)==0) RETURNFUNC(-RIG_EINTERN);

    pttmode = ttmode;

    if (ttmode[0] == '|') { pttmode = &ttmode[1]; } // remove first pipe symbol

    p = strchr(pttmode, '|');

    if (p) { *p = 0; } // remove any other pipe

    SNPRINTF(cmd_arg, sizeof(cmd_arg),
             "<params><param><value>%s</value></param></params>", pttmode);
    free(ttmode);

    if (!priv->has_get_modeA)
    {
        retval = tci1x_transaction(rig, "rig.set_mode", cmd_arg, NULL, 0);
    }
    else
    {
        char *cmd = "rig.set_modeA";

        if (vfo == RIG_VFO_B)
        {
            cmd = "rig.set_modeB";
        }
        else
        {
            // we make VFO_B mode unknown so it expires the cache
            priv->curr_modeB = RIG_MODE_NONE;
        }

        retval = tci1x_transaction(rig, cmd, cmd_arg, NULL, 0);
    }


    if (retval != RIG_OK)
    {
        rig_debug(RIG_DEBUG_ERR, "%s: failed: %s\n", __func__,
                  rigerror(retval));
        RETURNFUNC(retval);
    }

    // Determine if we need to update the bandwidth
    needBW = 0;

    if (vfo == RIG_VFO_A)
    {
        needBW = priv->curr_widthA != width;
        rig_debug(RIG_DEBUG_TRACE, "%s: bw change on VFOA, curr width=%d  needBW=%d\n",
                  __func__, (int)width, needBW);
    }
    else if (vfo == RIG_VFO_B)
    {
        needBW = priv->curr_widthB != width;
        rig_debug(RIG_DEBUG_TRACE, "%s: bw change on VFOB, curr width=%d  needBW=%d\n",
                  __func__, (int)width, needBW);
    }
    else
    {
        rig_debug(RIG_DEBUG_TRACE, "%s: needBW unknown vfo=%s\n", __func__,
                  rig_strvfo(vfo));
    }

    // Need to update the bandwidth
    if (width > 0 && needBW)
    {
        SNPRINTF(cmd_arg, sizeof(cmd_arg),
                 "<params><param><value><i4>%ld</i4></value></param></params>",
                 width);

        retval = tci1x_transaction(rig, "rig.set_bandwidth", cmd_arg, NULL, 0);

        if (retval < 0)
        {
            RETURNFUNC(retval);
        }
    }

    // Return to VFOA if needed
    rig_debug(RIG_DEBUG_TRACE, "%s: switch to VFOA? = %d\n", __func__,
              vfoSwitched);

    if (vfoSwitched)
    {
        rig_debug(RIG_DEBUG_TRACE, "%s: switching to VFOA\n", __func__);
        retval = tci1x_set_vfo(rig, RIG_VFO_A);

        if (retval < 0)
        {
            RETURNFUNC(retval);
        }
    }

    if (vfo == RIG_VFO_A)
    {
        priv->curr_modeA = mode;
        priv->curr_widthA = width;
    }
    else
    {
        priv->curr_modeB = mode;
        priv->curr_widthB = width;
    }

    rig_debug(RIG_DEBUG_TRACE,
              "%s: return modeA=%s, widthA=%d\n,modeB=%s, widthB=%d\n", __func__,
              rig_strrmode(priv->curr_modeA), (int)priv->curr_widthA,
              rig_strrmode(priv->curr_modeB), (int)priv->curr_widthB);
    RETURNFUNC(RIG_OK);
}

/*
* tci1x_get_mode
* Assumes rig!=NULL, rig->state.priv!=NULL, mode!=NULL
*/
static int tci1x_get_mode(RIG *rig, vfo_t vfo, rmode_t *mode, pbwidth_t *width)
{
    int retval;
    int vfoSwitched;
    char value[MAXCMDLEN];
    char *cmdp;
    vfo_t curr_vfo;
    rmode_t my_mode;
    struct tci1x_priv_data *priv = (struct tci1x_priv_data *) rig->state.priv;

    ENTERFUNC;
    rig_debug(RIG_DEBUG_TRACE, "%s: vfo=%s\n", __func__,
              rig_strvfo(vfo));

    if (check_vfo(vfo) == FALSE)
    {
        rig_debug(RIG_DEBUG_ERR, "%s: unsupported VFO %s\n",
                  __func__, rig_strvfo(vfo));
        RETURNFUNC(-RIG_EINVAL);
    }

    curr_vfo = rig->state.current_vfo;

    if (vfo == RIG_VFO_CURR)
    {
        vfo = rig->state.current_vfo;
    }

    rig_debug(RIG_DEBUG_TRACE, "%s: using vfo=%s\n", __func__,
              rig_strvfo(vfo));

    if (priv->ptt)
    {
        if (vfo == RIG_VFO_A) { *mode = priv->curr_modeA; }
        else { *mode = priv->curr_modeB; }

        rig_debug(RIG_DEBUG_VERBOSE, "%s call not made as PTT=1\n", __func__);
        RETURNFUNC(RIG_OK);  // just return OK and ignore this
    }

    // Switch to VFOB if appropriate
    vfoSwitched = 0;

    if (priv->has_get_modeA == 0 && vfo == RIG_VFO_B && curr_vfo != RIG_VFO_B)
    {
        vfoSwitched = 1;
    }

    if (vfoSwitched)
    {
        rig_debug(RIG_DEBUG_TRACE, "%s switch to VFOB=%d\n", __func__,
                  priv->has_get_modeA);
        retval = tci1x_set_vfo(rig, RIG_VFO_B);

        if (retval < 0)
        {
            RETURNFUNC(retval);
        }
    }

    cmdp = "rig.get_mode"; /* default to old way */

    if (priv->has_get_modeA)   /* change to new way if we can */
    {
        /* calling this way reduces VFO swapping */
        /* we get the cached value in tci1x */
        /* vfo B may not be getting polled though in TCI */
        /* so we may not be 100% accurate if op is twiddling knobs */
        cmdp = "rig.get_modeA";

        if (vfo == RIG_VFO_B) { cmdp = "rig.get_modeB"; }
    }

    retval = tci1x_transaction(rig, cmdp, NULL, value, sizeof(value));

    if (retval != RIG_OK)
    {
        rig_debug(RIG_DEBUG_ERR, "%s: %s failed: %s\n", __func__, cmdp,
                  rigerror(retval));
        RETURNFUNC(retval);
    }

    my_mode = modeMapGetHamlib(value);
    *mode = my_mode;
    rig_debug(RIG_DEBUG_TRACE, "%s: mode='%s'\n", __func__,
              rig_strrmode(*mode));

    if (vfo == RIG_VFO_A)
    {
        priv->curr_modeA = *mode;
    }
    else
    {
        priv->curr_modeB = *mode;
    }


    /* Get the bandwidth */
    cmdp = "rig.get_bw"; /* default to old way */

    if (priv->has_get_bwA)   /* change to new way if we can */
    {
        /* calling this way reduces VFO swapping */
        /* we get the cached value in tci1x */
        /* vfo B may not be getting polled though in TCI */
        /* so we may not be 100% accurate if op is twiddling knobs */
        cmdp = "rig.get_bwA";

        if (vfo == RIG_VFO_B) { cmdp = "rig.get_bwB"; }
    }

    retval = tci1x_transaction(rig, cmdp, NULL, value, sizeof(value));

    if (retval != RIG_OK)
    {
        RETURNFUNC(retval);
    }

    rig_debug(RIG_DEBUG_TRACE, "%s: mode=%s width='%s'\n", __func__,
              rig_strrmode(*mode), value);

    // we get 2 entries pipe separated for bandwidth, lower and upper
    if (strlen(value) > 0)
    {
        char *p = value;

        /* we might get two values and then we want the 2nd one */
        if (strchr(value, '|') != NULL) { p = strchr(value, '|') + 1; }

        *width = atoi(p);
    }

    if (vfo == RIG_VFO_A)
    {
        priv->curr_widthA = *width;
    }
    else
    {
        priv->curr_widthB = *width;
    }

    // Return to VFOA if needed
    if (vfoSwitched)
    {
        retval = tci1x_set_vfo(rig, RIG_VFO_A);

        if (retval != RIG_OK)
        {
            RETURNFUNC(retval);
        }
    }

    RETURNFUNC(RIG_OK);
}

/*
* tci1x_set_vfo
* assumes rig!=NULL
*/
static int tci1x_set_vfo(RIG *rig, vfo_t vfo)
{
    int retval;
    char cmd_arg[MAXBUFLEN];
    struct rig_state *rs = &rig->state;
    struct tci1x_priv_data *priv = (struct tci1x_priv_data *) rig->state.priv;

    ENTERFUNC;
    rig_debug(RIG_DEBUG_TRACE, "%s: vfo=%s\n", __func__,
              rig_strvfo(vfo));


    if (check_vfo(vfo) == FALSE)
    {
        rig_debug(RIG_DEBUG_ERR, "%s: unsupported VFO %s\n",
                  __func__, rig_strvfo(vfo));
        RETURNFUNC(-RIG_EINVAL);
    }

    if (vfo == RIG_VFO_TX)
    {
        rig_debug(RIG_DEBUG_TRACE, "%s: RIG_VFO_TX used\n", __func__);
        vfo = RIG_VFO_B; // always TX on VFOB
    }

    if (vfo == RIG_VFO_CURR)
    {
        vfo = rig->state.current_vfo;
    }

    SNPRINTF(cmd_arg, sizeof(cmd_arg),
             "<params><param><value>%s</value></param></params>",
             vfo == RIG_VFO_A ? "A" : "B");
    retval = tci1x_transaction(rig, "rig.set_AB", cmd_arg, NULL, 0);

    if (retval != RIG_OK)
    {
        rig_debug(RIG_DEBUG_ERR, "%s: rig.set_AB failed: %s\n", __func__,
                  rigerror(retval));
        RETURNFUNC(retval);
    }

    rig->state.current_vfo = vfo;
    rs->tx_vfo = RIG_VFO_B; // always VFOB

    /* for some rigs TCI turns off split when VFOA is selected */
    /* so if we are in split and asked for A we have to turn split back on */
    if (priv->split && vfo == RIG_VFO_A)
    {
        SNPRINTF(cmd_arg, sizeof(cmd_arg),
                 "<params><param><value><i4>%d</i4></value></param></params>",
                 priv->split);
        retval = tci1x_transaction(rig, "rig.set_split", cmd_arg, NULL, 0);

        if (retval < 0)
        {
            RETURNFUNC(retval);
        }
    }

    RETURNFUNC(RIG_OK);
}

/*
* tci1x_get_vfo
* assumes rig!=NULL, vfo != NULL
*/
static int tci1x_get_vfo(RIG *rig, vfo_t *vfo)
{
    char value[MAXCMDLEN];

    ENTERFUNC;


    int retval;
    retval = tci1x_transaction(rig, "rig.get_AB", NULL, value, sizeof(value));

    if (retval < 0)
    {
        RETURNFUNC(retval);
    }

    rig_debug(RIG_DEBUG_TRACE, "%s: vfo value=%s\n", __func__, value);

    switch (value[0])
    {
    case 'A':
        *vfo = RIG_VFO_A;
        break;

    case 'B':
        *vfo = RIG_VFO_B;
        break;

    default:
        *vfo = RIG_VFO_CURR;
        RETURNFUNC(-RIG_EINVAL);
    }

    if (check_vfo(*vfo) == FALSE)
    {
        rig_debug(RIG_DEBUG_ERR, "%s: unsupported VFO %s\n",
                  __func__, rig_strvfo(*vfo));
        RETURNFUNC(-RIG_EINVAL);
    }

    rig->state.current_vfo = *vfo;

    rig_debug(RIG_DEBUG_TRACE, "%s: vfo=%s\n", __func__,
              rig_strvfo(*vfo));

    RETURNFUNC(RIG_OK);
}

/*
* tci1x_set_split_freq
* assumes rig!=NULL
*/
static int tci1x_set_split_freq(RIG *rig, vfo_t vfo, freq_t tx_freq)
{
    int retval;
    char cmd_arg[MAXBUFLEN];
    freq_t qtx_freq;
    struct tci1x_priv_data *priv = (struct tci1x_priv_data *) rig->state.priv;

    ENTERFUNC;
    rig_debug(RIG_DEBUG_TRACE, "%s: vfo=%s freq=%.1f\n", __func__,
              rig_strvfo(vfo), tx_freq);

    if (check_vfo(vfo) == FALSE)
    {
        rig_debug(RIG_DEBUG_ERR, "%s: unsupported VFO %s\n",
                  __func__, rig_strvfo(vfo));
        RETURNFUNC(-RIG_EINVAL);
    }

    // we always split on VFOB so if no change just return
    retval = tci1x_get_freq(rig, RIG_VFO_B, &qtx_freq);

    if (retval != RIG_OK) { RETURNFUNC(retval); }

    if (tx_freq == qtx_freq) { RETURNFUNC(RIG_OK); }

    SNPRINTF(cmd_arg, sizeof(cmd_arg),
             "<params><param><value><double>%.6f</double></value></param></params>",
             tx_freq);
    retval = tci1x_transaction(rig, "rig.set_vfoB", cmd_arg, NULL, 0);

    if (retval < 0)
    {
        RETURNFUNC(retval);
    }

    priv->curr_freqB = tx_freq;

    RETURNFUNC(RIG_OK);
}

/*
* tci1x_get_split_freq
* assumes rig!=NULL, tx_freq!=NULL
*/
static int tci1x_get_split_freq(RIG *rig, vfo_t vfo, freq_t *tx_freq)
{
    int retval;
    struct tci1x_priv_data *priv = (struct tci1x_priv_data *) rig->state.priv;

    ENTERFUNC;
    rig_debug(RIG_DEBUG_TRACE, "%s: vfo=%s\n", __func__,
              rig_strvfo(vfo));

    retval = tci1x_get_freq(rig, RIG_VFO_B, tx_freq);
    priv->curr_freqB = *tx_freq;
    RETURNFUNC(retval);
}

/*
* tci1x_set_split_vfo
* assumes rig!=NULL, tx_freq!=NULL
*/
static int tci1x_set_split_vfo(RIG *rig, vfo_t vfo, split_t split, vfo_t tx_vfo)
{
    int retval;
    vfo_t qtx_vfo;
    split_t qsplit;
    struct tci1x_priv_data *priv = (struct tci1x_priv_data *) rig->state.priv;
    char cmd_arg[MAXBUFLEN];

    ENTERFUNC;
    rig_debug(RIG_DEBUG_TRACE, "%s: tx_vfo=%s\n", __func__,
              rig_strvfo(tx_vfo));

    retval = tci1x_get_split_vfo(rig, RIG_VFO_A, &qsplit, &qtx_vfo);

    if (retval != RIG_OK) { RETURNFUNC(retval); }

    if (split == qsplit) { RETURNFUNC(RIG_OK); }

    if (priv->ptt)
    {
        rig_debug(RIG_DEBUG_VERBOSE, "%s call not made as PTT=1\n", __func__);
        RETURNFUNC(RIG_OK);  // just return OK and ignore this
    }

    SNPRINTF(cmd_arg, sizeof(cmd_arg),
             "<params><param><value><i4>%d</i4></value></param></params>",
             split);
    retval = tci1x_transaction(rig, "rig.set_split", cmd_arg, NULL, 0);

    if (retval < 0)
    {
        RETURNFUNC(retval);
    }

    priv->split = split;

    RETURNFUNC(RIG_OK);
}

/*
* tci1x_get_split_vfo
* assumes rig!=NULL, tx_freq!=NULL
*/
static int tci1x_get_split_vfo(RIG *rig, vfo_t vfo, split_t *split,
                               vfo_t *tx_vfo)
{
    char value[MAXCMDLEN];
    struct tci1x_priv_data *priv = (struct tci1x_priv_data *) rig->state.priv;

    ENTERFUNC;

    int retval;
    retval = tci1x_transaction(rig, "rig.get_split", NULL, value, sizeof(value));

    if (retval < 0)
    {
        RETURNFUNC(retval);
    }

    *tx_vfo = RIG_VFO_B;
    *split = atoi(value);
    priv->split = *split;
    rig_debug(RIG_DEBUG_TRACE, "%s tx_vfo=%s, split=%d\n", __func__,
              rig_strvfo(*tx_vfo), *split);
    RETURNFUNC(RIG_OK);
}

/*
* tci1x_set_split_freq_mode
* assumes rig!=NULL
*/
static int tci1x_set_split_freq_mode(RIG *rig, vfo_t vfo, freq_t freq,
                                     rmode_t mode, pbwidth_t width)
{
    int retval;
    rmode_t qmode;
    pbwidth_t qwidth;
    struct tci1x_priv_data *priv = (struct tci1x_priv_data *) rig->state.priv;

    ENTERFUNC;

    // we always do split on VFOB
    retval = tci1x_set_freq(rig, RIG_VFO_B, freq);

    if (retval != RIG_OK)
    {
        rig_debug(RIG_DEBUG_ERR, "%s tci1x_set_freq failed\n", __func__);
        RETURNFUNC(retval);
    }

    // Make VFOB mode match VFOA mode, keep VFOB width
    retval = tci1x_get_mode(rig, RIG_VFO_B, &qmode, &qwidth);

    if (retval != RIG_OK) { RETURNFUNC(retval); }

    if (qmode == priv->curr_modeA) { RETURNFUNC(RIG_OK); }

    if (priv->ptt)
    {
        rig_debug(RIG_DEBUG_VERBOSE, "%s set_mode call not made as PTT=1\n", __func__);
        RETURNFUNC(RIG_OK);  // just return OK and ignore this
    }

    retval = tci1x_set_mode(rig, RIG_VFO_B, priv->curr_modeA, width);

    if (retval != RIG_OK)
    {
        rig_debug(RIG_DEBUG_ERR, "%s tci1x_set_mode failed\n", __func__);
        RETURNFUNC(retval);
    }

    retval = tci1x_set_vfo(rig, RIG_VFO_A);

    RETURNFUNC(retval);
}

/*
* tci1x_get_split_freq_mode
* assumes rig!=NULL, freq!=NULL, mode!=NULL, width!=NULL
*/
static int tci1x_get_split_freq_mode(RIG *rig, vfo_t vfo, freq_t *freq,
                                     rmode_t *mode, pbwidth_t *width)
{
    int retval;

    ENTERFUNC;

    if (vfo != RIG_VFO_CURR && vfo != RIG_VFO_TX)
    {
        RETURNFUNC(-RIG_ENTARGET);
    }

    retval = tci1x_get_freq(rig, RIG_VFO_B, freq);

    if (RIG_OK == retval)
    {
        retval = tci1x_get_mode(rig, vfo, mode, width);
    }

    RETURNFUNC(retval);
}

#ifdef XXNOTIMPLEMENTED
static int tci1x_set_level(RIG *rig, vfo_t vfo, setting_t level, value_t val)
{
    int retval;
    char cmd_arg[MAXARGLEN];
    char *cmd;
    char *param_type = "i4";

    ENTERFUNC;
    rig_debug(RIG_DEBUG_TRACE, "%s: vfo=%s level=%d, val=%f\n", __func__,
              rig_strvfo(vfo), (int)level, val.f);

    if (check_vfo(vfo) == FALSE)
    {
        rig_debug(RIG_DEBUG_ERR, "%s: unsupported VFO %s\n",
                  __func__, rig_strvfo(vfo));
        RETURNFUNC(-RIG_EINVAL);
    }

    switch (level)
    {
    case RIG_LEVEL_RF: cmd = "rig.set_rfgain"; val.f *= 100; break;

    case RIG_LEVEL_AF: cmd = "rig.set_volume"; val.f *= 100; break;

    case RIG_LEVEL_MICGAIN: cmd = "rig.set_micgain"; val.f *= 100; break;

    case RIG_LEVEL_RFPOWER: cmd = "rig.set_power"; val.f *= 100; break;

    default:
        rig_debug(RIG_DEBUG_ERR, "%s: invalid level=%d\n", __func__, (int)level);
        RETURNFUNC(-RIG_EINVAL);
    }

    SNPRINTF(cmd_arg, sizeof(cmd_arg),
             "<params><param><value><%s>%d</%s></value></param></params>",
             param_type, (int)val.f, param_type);


    retval = tci1x_transaction(rig, cmd, cmd_arg, NULL, 0);

    if (retval < 0)
    {
        RETURNFUNC(retval);
    }

    RETURNFUNC(RIG_OK);
}

/*
* tci1x_get_level
* Assumes rig!=NULL, rig->state.priv!=NULL, val!=NULL
*/
static int tci1x_get_level(RIG *rig, vfo_t vfo, setting_t level, value_t *val)
{
    char value[MAXARGLEN];
    char *cmd;
    int retval;
    struct tci1x_priv_data *priv = (struct tci1x_priv_data *) rig->state.priv;

    ENTERFUNC;
    rig_debug(RIG_DEBUG_TRACE, "%s: vfo=%s\n", __func__,
              rig_strvfo(vfo));


    switch (level)
    {
    case RIG_LEVEL_AF: cmd = "rig.get_volume"; break;

    case RIG_LEVEL_RF: cmd = "rig.get_rfgain"; break;

    case RIG_LEVEL_MICGAIN: cmd = "rig.get_micgain"; break;

    case RIG_LEVEL_STRENGTH: cmd = "rig.get_smeter"; break;

    case RIG_LEVEL_RFPOWER: cmd = "rig.get_power"; break;

    case RIG_LEVEL_RFPOWER_METER_WATTS:
    case RIG_LEVEL_RFPOWER_METER: cmd = "rig.get_pwrmeter"; break;

    default:
        rig_debug(RIG_DEBUG_ERR, "%s: unknown level=%d\n", __func__, (int)level);
        RETURNFUNC(-RIG_EINVAL);
    }

    retval = tci1x_transaction(rig, cmd, NULL, value, sizeof(value));

    if (retval != RIG_OK)
    {
        rig_debug(RIG_DEBUG_ERR, "%s: tci1x_transaction failed retval=%s\n", __func__,
                  rigerror(retval));
        RETURNFUNC(retval);
    }

    // most levels are 0-100 -- may have to allow for different ranges
    switch (level)
    {
    case RIG_LEVEL_STRENGTH:
        val->i = atoi(value) - 54;
        //if (val->i > 0) val->i /= 10;
        rig_debug(RIG_DEBUG_TRACE, "%s: val.i='%s'(%d)\n", __func__, value, val->i);
        break;

    case RIG_LEVEL_RFPOWER:
        val->f = atof(value) / 100.0 * priv->powermeter_scale;
        rig_debug(RIG_DEBUG_TRACE, "%s: val.f='%s'(%g)\n", __func__, value, val->f);
        break;

    case RIG_LEVEL_RFPOWER_METER:
        val->f = atof(value) / 100.0 * priv->powermeter_scale;
        rig_debug(RIG_DEBUG_TRACE, "%s: val.f='%s'(%g)\n", __func__, value, val->f);
        break;

    case RIG_LEVEL_RFPOWER_METER_WATTS:
        val->f = atof(value) * priv->powermeter_scale;
        rig_debug(RIG_DEBUG_TRACE, "%s: val.f='%s'(%g)\n", __func__, value, val->f);
        break;

    default:
        val->f = atof(value) / 100;
        rig_debug(RIG_DEBUG_TRACE, "%s: val.f='%s'(%f)\n", __func__, value, val->f);
    }


    RETURNFUNC(RIG_OK);
}
#endif

/*
* tci1x_get_info
* assumes rig!=NULL
*/
static const char *tci1x_get_info(RIG *rig)
{
    struct tci1x_priv_data *priv = (struct tci1x_priv_data *) rig->state.priv;

    return (priv->info);
}

static int tci1x_power2mW(RIG *rig, unsigned int *mwpower, float power,
                          freq_t freq, rmode_t mode)
{
    struct tci1x_priv_data *priv = (struct tci1x_priv_data *) rig->state.priv;
    ENTERFUNC;
    rig_debug(RIG_DEBUG_TRACE, "%s: passed power = %f\n", __func__, power);
    rig_debug(RIG_DEBUG_TRACE, "%s: passed freq = %"PRIfreq" Hz\n", __func__, freq);
    rig_debug(RIG_DEBUG_TRACE, "%s: passed mode = %s\n", __func__,
              rig_strrmode(mode));

    power *= priv->powermeter_scale;
    *mwpower = (power * 100000);

    RETURNFUNC(RIG_OK);
}

static int tci1x_mW2power(RIG *rig, float *power, unsigned int mwpower,
                          freq_t freq, rmode_t mode)
{
    ENTERFUNC;
    rig_debug(RIG_DEBUG_TRACE, "%s: passed mwpower = %u\n", __func__, mwpower);
    rig_debug(RIG_DEBUG_TRACE, "%s: passed freq = %"PRIfreq" Hz\n", __func__, freq);
    rig_debug(RIG_DEBUG_TRACE, "%s: passed mode = %s\n", __func__,
              rig_strrmode(mode));

    *power = ((float)mwpower / 100000);

    RETURNFUNC(RIG_OK);

}

#ifdef XXNOTIMPLEMENTED
static int tci1x_set_ext_parm(RIG *rig, token_t token, value_t val)
{
    struct tci1x_priv_data *priv = (struct tci1x_priv_data *)rig->state.priv;
    char lstr[64];
    const struct confparams *cfp;
    struct ext_list *epp;

    ENTERFUNC;
    cfp = rig_ext_lookup_tok(rig, token);

    if (!cfp)
    {
        RETURNFUNC(-RIG_EINVAL);
    }

    switch (token)
    {
    case TOK_TCI1X_VERIFY_FREQ:
    case TOK_TCI1X_VERIFY_PTT:
        if (val.i && !priv->has_verify_cmds)
        {
            rig_debug(RIG_DEBUG_ERR,
                      "%s: FLRig version 1.3.54.18 or higher needed to support fast functions\n",
                      __func__);
            RETURNFUNC(-RIG_EINVAL);
        }

        break;

    default:
        RETURNFUNC(-RIG_EINVAL);
    }

    switch (cfp->type)
    {
    case RIG_CONF_STRING:
        strcpy(lstr, val.s);
        break;


    case RIG_CONF_COMBO:
        SNPRINTF(lstr, sizeof(lstr), "%d", val.i);
        break;

    case RIG_CONF_NUMERIC:
        SNPRINTF(lstr, sizeof(lstr), "%f", val.f);
        break;

    case RIG_CONF_CHECKBUTTON:
        SNPRINTF(lstr, sizeof(lstr), "%s", val.i ? "ON" : "OFF");
        break;

    case RIG_CONF_BUTTON:
        lstr[0] = '\0';
        break;

    default:
        RETURNFUNC(-RIG_EINTERNAL);
    }

    epp = find_ext(priv->ext_parms, token);

    if (!epp)
    {
        RETURNFUNC(-RIG_EINTERNAL);
    }

    /* store value */
    epp->val = val;


    rig_debug(RIG_DEBUG_VERBOSE, "%s called: %s %s\n", __func__,
              cfp->name, lstr);

    RETURNFUNC(RIG_OK);
}

static int tci1x_get_ext_parm(RIG *rig, token_t token, value_t *val)
{
    struct tci1x_priv_data *priv = (struct tci1x_priv_data *)rig->state.priv;
    const struct confparams *cfp;
    struct ext_list *epp;

    ENTERFUNC;
    /* TODO: load value from priv->ext_parms */

    cfp = rig_ext_lookup_tok(rig, token);

    if (!cfp)
    {
        RETURNFUNC(-RIG_EINVAL);
    }

    switch (token)
    {
    case TOK_TCI1X_VERIFY_FREQ:
    case TOK_TCI1X_VERIFY_PTT:
        break;

    default:
        RETURNFUNC(-RIG_EINVAL);
    }

    epp = find_ext(priv->ext_parms, token);

    if (!epp)
    {
        RETURNFUNC(-RIG_EINTERNAL);
    }

    /* load value */
    *val = epp->val;

    rig_debug(RIG_DEBUG_VERBOSE, "%s called: %s\n", __func__,
              cfp->name);

    RETURNFUNC(RIG_OK);
}


static int tci1x_set_ext_parm(RIG *rig, setting_t parm, value_t val)
{
    struct tci1x_priv_data *priv = (struct tci1x_priv_data *)rig->state.priv;
    int idx;
    char pstr[32];

    ENTERFUNC;
    idx = rig_setting2idx(parm);

    if (idx >= RIG_SETTING_MAX)
    {
        RETURNFUNC(-RIG_EINVAL);
    }

    if (RIG_PARM_IS_FLOAT(parm))
    {
        SNPRINTF(pstr, sizeof(pstr), "%f", val.f);
    }
    else
    {
        SNPRINTF(pstr, sizeof(pstr), "%d", val.i);
    }

    rig_debug(RIG_DEBUG_VERBOSE, "%s called: %s %s\n", __func__,
              rig_strparm(parm), pstr);
    priv->parms[idx] = val;

    RETURNFUNC(RIG_OK);
}

static int tci1x_get_ext_parm(RIG *rig, setting_t parm, value_t *val)
{
    struct tci1x_priv_data *priv = (struct tci1x_priv_data *)rig->state.priv;
    int idx;

    ENTERFUNC;
    idx = rig_setting2idx(parm);

    if (idx >= RIG_SETTING_MAX)
    {
        RETURNFUNC(-RIG_EINVAL);
    }

    *val = priv->parms[idx];
    rig_debug(RIG_DEBUG_VERBOSE, "%s called %s\n", __func__,
              rig_strparm(parm));

    RETURNFUNC(RIG_OK);
}
#endif
