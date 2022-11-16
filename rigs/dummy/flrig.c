/*
*  Hamlib FLRig backend - main file
*  Copyright (c) 2017 by Michael Black W9MDB
*  Copyright (c) 2018 by Michael Black W9MDB
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
#include "flrig.h"

#define DEBUG 1
#define DEBUG_TRACE DEBUG_VERBOSE

#define MAXCMDLEN 8192
#define MAXXMLLEN 8192
#define MAXARGLEN 128
#define MAXBANDWIDTHLEN 4096

#define DEFAULTPATH "127.0.0.1:12345"

#define FLRIG_VFOS (RIG_VFO_A|RIG_VFO_B)

#define FLRIG_MODES (RIG_MODE_AM | RIG_MODE_PKTAM | RIG_MODE_CW | RIG_MODE_CWR |\
                    RIG_MODE_RTTY | RIG_MODE_RTTYR |\
                    RIG_MODE_PKTLSB | RIG_MODE_PKTUSB |\
                    RIG_MODE_SSB | RIG_MODE_LSB | RIG_MODE_USB |\
                    RIG_MODE_FM | RIG_MODE_WFM | RIG_MODE_FMN | RIG_MODE_PKTFM |\
                    RIG_MODE_C4FM)

#define FLRIG_LEVELS (RIG_LEVEL_AF | RIG_LEVEL_RF | RIG_LEVEL_MICGAIN | RIG_LEVEL_STRENGTH | RIG_LEVEL_RFPOWER_METER | RIG_LEVEL_RFPOWER_METER_WATTS | RIG_LEVEL_RFPOWER)

#define FLRIG_PARM (TOK_FLRIG_VERIFY_FREQ|TOK_FLRIG_VERIFY_PTT)

#define streq(s1,s2) (strcmp(s1,s2)==0)

static int flrig_init(RIG *rig);
static int flrig_open(RIG *rig);
static int flrig_close(RIG *rig);
static int flrig_cleanup(RIG *rig);
static int flrig_set_freq(RIG *rig, vfo_t vfo, freq_t freq);
static int flrig_get_freq(RIG *rig, vfo_t vfo, freq_t *freq);
static int flrig_set_ptt(RIG *rig, vfo_t vfo, ptt_t ptt);
static int flrig_set_mode(RIG *rig, vfo_t vfo, rmode_t mode, pbwidth_t width);
static int flrig_set_split_mode(RIG *rig, vfo_t vfo, rmode_t mode,
                                pbwidth_t width);
static int flrig_get_mode(RIG *rig, vfo_t vfo, rmode_t *mode, pbwidth_t *width);
static int flrig_get_vfo(RIG *rig, vfo_t *vfo);
static int flrig_set_vfo(RIG *rig, vfo_t vfo);
static int flrig_set_ptt(RIG *rig, vfo_t vfo, ptt_t ptt);
static int flrig_get_ptt(RIG *rig, vfo_t vfo, ptt_t *ptt);
static int flrig_set_func(RIG *rig, vfo_t vfo, setting_t setting, int status);
static int flrig_set_split_freq(RIG *rig, vfo_t vfo, freq_t tx_freq);
static int flrig_get_split_freq(RIG *rig, vfo_t vfo, freq_t *tx_freq);
static int flrig_set_split_vfo(RIG *rig, vfo_t vfo, split_t split,
                               vfo_t tx_vfo);
static int flrig_get_split_vfo(RIG *rig, vfo_t vfo, split_t *split,
                               vfo_t *tx_vfo);
static int flrig_set_split_freq_mode(RIG *rig, vfo_t vfo, freq_t freq,
                                     rmode_t mode, pbwidth_t width);
static int flrig_get_split_freq_mode(RIG *rig, vfo_t vfo, freq_t *freq,
                                     rmode_t *mode, pbwidth_t *width);
static int flrig_set_level(RIG *rig, vfo_t vfo, setting_t level, value_t val);
static int flrig_get_level(RIG *rig, vfo_t vfo, setting_t level, value_t *val);

static int flrig_set_ext_parm(RIG *rig, token_t token, value_t val);
static int flrig_get_ext_parm(RIG *rig, token_t token, value_t *val);

static const char *flrig_get_info(RIG *rig);
static int flrig_power2mW(RIG *rig, unsigned int *mwpower, float power,
                          freq_t freq, rmode_t mode);
static int flrig_mW2power(RIG *rig, float *power, unsigned int mwpower,
                          freq_t freq, rmode_t mode);

struct flrig_priv_data
{
    vfo_t curr_vfo;
    char bandwidths[MAXBANDWIDTHLEN]; /* pipe delimited set returned from flrig */
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
    int has_set_bwA; /* True if this function is available */
    int has_verify_cmds; // has the verify cmd in FLRig 1.3.54.1 or higher
    float powermeter_scale;  /* So we can scale power meter to 0-1 */
    value_t parms[RIG_SETTING_MAX];
    struct ext_list *ext_parms;
};

/* level's and parm's tokens */
#define TOK_FLRIG_VERIFY_FREQ    TOKEN_BACKEND(1)
#define TOK_FLRIG_VERIFY_PTT     TOKEN_BACKEND(2)

static const struct confparams flrig_ext_parms[] =
{
    {
        TOK_FLRIG_VERIFY_FREQ, "VERIFY_FREQ", "Verify set_freq", "If true will verify set_freq otherwise is fire and forget", "0", RIG_CONF_CHECKBUTTON, {}
    },
    {
        TOK_FLRIG_VERIFY_PTT, "VERIFY_PTT", "Verify set_ptt", "If true will verify set_ptt otherwise set_ptt is fire and forget", "0", RIG_CONF_CHECKBUTTON, {}
    },
    { RIG_CONF_END, NULL, }
};

const struct rig_caps flrig_caps =
{
    RIG_MODEL(RIG_MODEL_FLRIG),
    .model_name = "FLRig",
    .mfg_name = "FLRig",
    .version = "20221109.0",
    .copyright = "LGPL",
    .status = RIG_STATUS_STABLE,
    .rig_type = RIG_TYPE_TRANSCEIVER,
    .targetable_vfo =  RIG_TARGETABLE_FREQ | RIG_TARGETABLE_MODE,
    .ptt_type = RIG_PTT_RIG,
    .port_type = RIG_PORT_NETWORK,
    .write_delay = 0,
    .post_write_delay = 0,
    .timeout = 5000,
    .retry = 2,

    .has_get_func = RIG_FUNC_NONE,
    .has_set_func = RIG_FUNC_TUNER,
    .set_func = flrig_set_func,
    .has_get_level = FLRIG_LEVELS,
    .has_set_level = RIG_LEVEL_SET(FLRIG_LEVELS),
    .has_get_parm =    FLRIG_PARM,
    .has_set_parm =    RIG_PARM_SET(FLRIG_PARM),

    .filters =  {
        {RIG_MODE_ALL, RIG_FLT_ANY},
        RIG_FLT_END
    },

    .rx_range_list1 = {{
            .startf = kHz(1), .endf = GHz(10), .modes = FLRIG_MODES,
            .low_power = -1, .high_power = -1, FLRIG_VFOS, RIG_ANT_1
        },
        RIG_FRNG_END,
    },
    .tx_range_list1 = {RIG_FRNG_END,},
    .rx_range_list2 = {{
            .startf = kHz(1), .endf = GHz(10), .modes = FLRIG_MODES,
            .low_power = -1, .high_power = -1, FLRIG_VFOS, RIG_ANT_1
        },
        RIG_FRNG_END,
    },
    .tx_range_list2 = {RIG_FRNG_END,},
    .tuning_steps =  { {FLRIG_MODES, 1}, {FLRIG_MODES, RIG_TS_ANY}, RIG_TS_END, },
    .priv = NULL,               /* priv */

    .extparms =     flrig_ext_parms,

    .rig_init = flrig_init,
    .rig_open = flrig_open,
    .rig_close = flrig_close,
    .rig_cleanup = flrig_cleanup,

    .set_freq = flrig_set_freq,
    .get_freq = flrig_get_freq,
    .set_mode = flrig_set_mode,
    .get_mode = flrig_get_mode,
    .set_vfo = flrig_set_vfo,
    .get_vfo = flrig_get_vfo,
    .get_info =      flrig_get_info,
    .set_ptt = flrig_set_ptt,
    .get_ptt = flrig_get_ptt,
    .set_split_mode = flrig_set_split_mode,
    .set_split_freq = flrig_set_split_freq,
    .get_split_freq = flrig_get_split_freq,
    .set_split_vfo = flrig_set_split_vfo,
    .get_split_vfo = flrig_get_split_vfo,
    .set_split_freq_mode = flrig_set_split_freq_mode,
    .get_split_freq_mode = flrig_get_split_freq_mode,
    .set_level = flrig_set_level,
    .get_level = flrig_get_level,
    .set_ext_parm =  flrig_set_ext_parm,
    .get_ext_parm =  flrig_get_ext_parm,
    .power2mW =   flrig_power2mW,
    .mW2power =   flrig_mW2power,
    .hamlib_check_rig_caps = HAMLIB_CHECK_RIG_CAPS
};

//Structure for mapping flrig dynmamic modes to hamlib modes
//flrig displays modes as the rig displays them
struct s_modeMap
{
    rmode_t mode_hamlib;
    char *mode_flrig;
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

/*Rather than use some huge XML library we only need a few things
* So we'll hand craft them
* xml_build takes a value and return an xml string for FLRig
*/
static char *xml_build(RIG *rig, char *cmd, char *value, char *xmlbuf,
                       int xmlbuflen)
{
    char xml[4096]; // we shouldn't need more the 4096 bytes for this
    char tmp[32];
    char *header;

    // We want at least a 4K buf to play with
    if (xmlbuflen < 4096)
    {
        rig_debug(RIG_DEBUG_ERR, "%s: xmllen < 4096\n", __func__);
        return NULL;
    }

    header =
        "POST /RPC2 HTTP/1.1\r\n" "User-Agent: XMLRPC++ 0.8\r\n"
        "Host: 127.0.0.1:12345\r\n" "Content-type: text/xml\r\n";
    SNPRINTF(xmlbuf, xmlbuflen, "%s", header);

    SNPRINTF(xml, sizeof(xml),
             "<?xml version=\"1.0\"?>\r\n<?clientid=\"hamlib(%d)\"?>\r\n",
             rig->state.rigport.client_port);

    strncat(xml, "<methodCall><methodName>", sizeof(xml) - 1);
    strncat(xml, cmd, sizeof(xml) - strlen(xml) - 1);
    strncat(xml, "</methodName>\r\n", sizeof(xml) - strlen(xml) - 1);

    if (value && strlen(value) > 0)
    {
        strncat(xml, value, sizeof(xml) - 1);
    }

    strncat(xml, "</methodCall>\r\n", sizeof(xml) - 1);
    strncat(xmlbuf, "Content-length: ", xmlbuflen - 1);
    SNPRINTF(tmp, sizeof(tmp), "%d\r\n\r\n", (int)strlen(xml));
    strncat(xmlbuf, tmp, xmlbuflen - 1);
    strncat(xmlbuf, xml, xmlbuflen - 1);
    return xmlbuf;
}

/*This is a very crude xml parse specific to what we need from FLRig
* This works for strings, doubles, I4-type values, and arrays
* Arrays are returned pipe delimited
*/
static char *xml_parse2(char *xml, char *value, int valueLen)
{
    char *delims = "<>\r\n ";
    char *xmltmp = strdup(xml);
    //rig_debug(RIG_DEBUG_TRACE, "%s: xml='%s'\n", __func__,xml);
    char *pr = xml;
    char *p = strtok_r(xmltmp, delims, &pr);
    value[0] = 0;

    while (p)
    {
        if (streq(p, "value"))
        {
            p = strtok_r(NULL, delims, &pr);

            if (streq(p, "array")) { continue; }

            if (streq(p, "/value")) { continue; } // empty value

            if (streq(p, "i4") || streq(p, "double") || streq(p, "int")
                    || streq(p, "string"))
            {
                p = strtok_r(NULL, delims, &pr);
            }
            else if (streq(p, "array"))
            {
                strtok_r(NULL, delims, &pr);
                p = strtok_r(NULL, delims, &pr);
            }

            if (strlen(value) + strlen(p) + 1 < valueLen)
            {
                if (value[0] != 0) { strcat(value, "|"); }

                strcat(value, p);
            }
            else   // we'll just stop adding stuff
            {
                rig_debug(RIG_DEBUG_ERR, "%s: max value length exceeded\n", __func__);
            }
        }
        else
        {
            p = strtok_r(NULL, delims, &pr);
        }
    }

    rig_debug(RIG_DEBUG_TRACE, "%s: value returned='%s'\n", __func__, value);

    if (rig_need_debug(RIG_DEBUG_WARN) && value != NULL && strlen(value) == 0)
    {
        rig_debug(RIG_DEBUG_ERR, "%s: xml='%s'\n", __func__, xml);
    }

    free(xmltmp);
    return value;
}

/*
* xml_parse
* Assumes xml!=NULL, value!=NULL, value_len big enough
* returns the string value contained in the xml string
*/
static char *xml_parse(char *xml, char *value, int value_len)
{
    char *next;
    char *pxml;

    /* first off we should have an OK on the 1st line */
    if (strstr(xml, " 200 OK") == NULL)
    {
        return (NULL);
    }

    rig_debug(RIG_DEBUG_TRACE, "%s XML:\n%s\n", __func__, xml);

    // find the xml skipping the other stuff above it
    pxml = strstr(xml, "<?xml");

    if (pxml == NULL)
    {
        return (NULL);
    }

    next = strchr(pxml + 1, '<');

    if (value != NULL)
    {
        xml_parse2(next, value, value_len);
    }

    if (value && strstr(value, "faultString"))
    {
        rig_debug(RIG_DEBUG_ERR, "%s error:\n%s\n", __func__, value);
        value[0] = 0; /* truncate to give empty response */
    }

    return (value);
}

/*
* read_transaction
* Assumes rig!=NULL, xml!=NULL, xml_len>=MAXXMLLEN
*/
static int read_transaction(RIG *rig, char *xml, int xml_len)
{
    int retval;
    int retry;
    char *delims;
    char *terminator = "</methodResponse>";
    struct rig_state *rs = &rig->state;

    ENTERFUNC;

    retry = 2;
    delims = "\n";
    xml[0] = 0;

    do
    {
        char tmp_buf[MAXXMLLEN];        // plenty big for expected flrig responses hopefully

        if (retry < 2)
        {
            rig_debug(RIG_DEBUG_WARN, "%s: retry needed? retry=%d\n", __func__, retry);
        }

        int len = read_string(&rs->rigport, (unsigned char *) tmp_buf, sizeof(tmp_buf),
                              delims,
                              strlen(delims), 0, 1);
        rig_debug(RIG_DEBUG_TRACE, "%s: string='%s'\n", __func__, tmp_buf);

        // if our first response we should see the HTTP header
        if (strlen(xml) == 0 && strstr(tmp_buf, "HTTP/1.1 200 OK") == NULL)
        {
            rig_debug(RIG_DEBUG_ERR, "%s: Expected 'HTTP/1.1 200 OK', got '%s'\n", __func__,
                      tmp_buf);
            continue; // we'll try again
        }

        if (len > 0) { retry = 3; }

        if (len <= 0)
        {
            rig_debug(RIG_DEBUG_ERR, "%s: read_string error=%d\n", __func__, len);
            continue;
        }

        if (strlen(xml) + strlen(tmp_buf) < xml_len - 1)
        {
            strncat(xml, tmp_buf, xml_len - 1);
        }
        else
        {
            rig_debug(RIG_DEBUG_ERR,
                      "%s: xml buffer overflow!!\nTrying to add len=%d\nTo len=%d\n", __func__,
                      (int)strlen(tmp_buf), (int)strlen(xml));
            RETURNFUNC(-RIG_EPROTO);
        }
    }
    while (retry-- > 0 && strstr(xml, terminator) == NULL);

    if (retry == 0)
    {
        rig_debug(RIG_DEBUG_WARN, "%s: retry timeout\n", __func__);
        RETURNFUNC(-RIG_ETIMEOUT);
    }

    if (strstr(xml, terminator))
    {
        rig_debug(RIG_DEBUG_TRACE, "%s: got %s\n", __func__, terminator);
        // Slow down just a bit -- not sure this is needed anymore but not a big deal here
        hl_usleep(2 * 1000);
        retval = RIG_OK;
    }
    else
    {
        rig_debug(RIG_DEBUG_VERBOSE, "%s: did not get %s\n", __func__, terminator);
        retval = -(101 + RIG_EPROTO);
    }

    RETURNFUNC(retval);
}

/*
* write_transaction
* Assumes rig!=NULL, xml!=NULL, xml_len=total size of xml for response
*/
static int write_transaction(RIG *rig, char *xml, int xml_len)
{

    int try = rig->caps->retry;

    int retval = -RIG_EPROTO;

    struct rig_state *rs = &rig->state;

    ENTERFUNC;

    // This shouldn't ever happen...but just in case
    // We need to avoid an empty write as rigctld replies with blank line
    if (xml_len == 0)
    {
        rig_debug(RIG_DEBUG_ERR, "%s: len==0??\n", __func__);
        RETURNFUNC(retval);
    }

    // appears we can lose sync if we don't clear things out
    // shouldn't be anything for us now anyways
    rig_flush(&rig->state.rigport);

    while (try-- >= 0 && retval != RIG_OK)
        {
            retval = write_block(&rs->rigport, (unsigned char *) xml, strlen(xml));

            if (retval  < 0)
            {
                RETURNFUNC(-RIG_EIO);
            }
        }

    RETURNFUNC(retval);
}

static int flrig_transaction(RIG *rig, char *cmd, char *cmd_arg, char *value,
                             int value_len)
{
    char xml[MAXXMLLEN];
    int retry = 3;

    ENTERFUNC;
    ELAPSED1;

    set_transaction_active(rig);

    if (value)
    {
        value[0] = 0;
    }

    do
    {
        char *pxml;
        int retval;

        if (retry != 3)
        {
            rig_debug(RIG_DEBUG_VERBOSE, "%s: cmd=%s, retry=%d\n", __func__, cmd, retry);
        }

        pxml = xml_build(rig, cmd, cmd_arg, xml, sizeof(xml));
        retval = write_transaction(rig, pxml, strlen(pxml));

        if (retval != RIG_OK)
        {
            rig_debug(RIG_DEBUG_ERR, "%s: write_transaction error=%d\n", __func__, retval);

            // if we get RIG_EIO the socket has probably disappeared
            // so bubble up the error so port can re re-opened
            if (retval == -RIG_EIO) { set_transaction_inactive(rig); RETURNFUNC(retval); }

            hl_usleep(50 * 1000); // 50ms sleep if error
        }

        read_transaction(rig, xml, sizeof(xml)); // this might time out -- that's OK

        // we get an unknown response if function does not exist
        if (strstr(xml, "unknown")) { set_transaction_inactive(rig); RETURNFUNC(RIG_ENAVAIL); }

        if (value)
        {
            xml_parse(xml, value, value_len);
        }
    }
    while (((value && strlen(value) == 0) || (strlen(xml) == 0))
            && retry--); // we'll do retries if needed

    if (value && strlen(value) == 0)
    {
        rig_debug(RIG_DEBUG_ERR, "%s: no value returned\n", __func__);
        set_transaction_inactive(rig); RETURNFUNC(RIG_EPROTO);
    }

    ELAPSED2;
    set_transaction_inactive(rig);
    RETURNFUNC(RIG_OK);
}

/*
* flrig_init
* Assumes rig!=NULL
*/
static int flrig_init(RIG *rig)
{
    struct flrig_priv_data *priv;

    ENTERFUNC;
    rig_debug(RIG_DEBUG_TRACE, "%s version %s\n", __func__, rig->caps->version);

    rig->state.priv  = (struct flrig_priv_data *)calloc(1, sizeof(
                           struct flrig_priv_data));

    if (!rig->state.priv)
    {
        RETURNFUNC(-RIG_ENOMEM);
    }

    priv = rig->state.priv;

    memset(priv, 0, sizeof(struct flrig_priv_data));
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

    priv->ext_parms = alloc_init_ext(flrig_ext_parms);

    if (!priv->ext_parms)
    {
        RETURNFUNC(-RIG_ENOMEM);
    }


    RETURNFUNC(RIG_OK);
}

/*
* modeMapGetFLRig
* Assumes mode!=NULL
* Return the string for FLRig for the given hamlib mode
*/
static const char *modeMapGetFLRig(rmode_t modeHamlib)
{
    int i;

    rig_debug(RIG_DEBUG_TRACE, "%s: called\n", __func__);

    for (i = 0; modeMap[i].mode_hamlib != 0; ++i)
    {
        if (modeMap[i].mode_flrig == NULL) { continue; }

        rig_debug(RIG_DEBUG_TRACE,
                  "%s: checking modeMap[%d]=%.0f to modeHamlib=%.0f, mode_flrig='%s'\n", __func__,
                  i, (double)modeMap[i].mode_hamlib, (double)modeHamlib, modeMap[i].mode_flrig);

        if (modeMap[i].mode_hamlib == modeHamlib && strlen(modeMap[i].mode_flrig) > 0)
        {
            rig_debug(RIG_DEBUG_TRACE, "%s matched mode=%.0f, returning '%s'\n", __func__,
                      (double)modeHamlib, modeMap[i].mode_flrig);
            return (modeMap[i].mode_flrig);
        }
    }

    rig_debug(RIG_DEBUG_ERR, "%s: FlRig does not have mode: %s\n", __func__,
              rig_strrmode(modeHamlib));
    return ("ERROR");
}

/*
* modeMapGetHamlib
* Assumes mode!=NULL
* Return the hamlib mode from the given FLRig string
*/
static rmode_t modeMapGetHamlib(const char *modeFLRig)
{
    int i;
    char modeFLRigCheck[64];

    SNPRINTF(modeFLRigCheck, sizeof(modeFLRigCheck), "|%s|", modeFLRig);

    for (i = 0; modeMap[i].mode_hamlib != 0; ++i)
    {
        rig_debug(RIG_DEBUG_TRACE, "%s: find '%s' in '%s'\n", __func__,
                  modeFLRigCheck, modeMap[i].mode_flrig);

        if (modeMap[i].mode_flrig
                && strcmp(modeMap[i].mode_flrig, modeFLRigCheck) == 0)
        {
            return (modeMap[i].mode_hamlib);
        }
    }

    rig_debug(RIG_DEBUG_TRACE, "%s: mode requested: %s, not in modeMap\n", __func__,
              modeFLRig);
    return (RIG_MODE_NONE);
}


/*
* modeMapAdd
* Assumes modes!=NULL
*/
static void modeMapAdd(rmode_t *modes, rmode_t mode_hamlib, char *mode_flrig)
{
    int i;
    int len1;

    rig_debug(RIG_DEBUG_TRACE, "%s:mode_flrig=%s\n", __func__, mode_flrig);

    // if we already have it just return
    // We get ERROR if the mode is not known so non-ERROR is OK
    if (modeMapGetHamlib(mode_flrig) != RIG_MODE_NONE) { return; }

    len1 = strlen(mode_flrig) + 3; /* bytes needed for allocating */

    for (i = 0; modeMap[i].mode_hamlib != 0; ++i)
    {
        if (modeMap[i].mode_hamlib == mode_hamlib)
        {
            int len2;
            *modes |= modeMap[i].mode_hamlib;

            /* we will pipe delimit all the entries for easier matching */
            /* all entries will have pipe symbol on both sides */
            if (modeMap[i].mode_flrig == NULL)
            {
                modeMap[i].mode_flrig = calloc(1, len1);

                if (modeMap[i].mode_flrig == NULL)
                {
                    rig_debug(RIG_DEBUG_ERR, "%s: error allocating memory for modeMap\n",
                              __func__);
                    return;
                }
            }

            len2 = strlen(modeMap[i].mode_flrig); /* current len w/o null */
            modeMap[i].mode_flrig = realloc(modeMap[i].mode_flrig,
                                            strlen(modeMap[i].mode_flrig) + len1);

            if (strlen(modeMap[i].mode_flrig) == 0) { modeMap[i].mode_flrig[0] = '|'; }

            strncat(modeMap[i].mode_flrig, mode_flrig, len1 + len2);
            strncat(modeMap[i].mode_flrig, "|", len1 + len2);
            rig_debug(RIG_DEBUG_TRACE, "%s: Adding mode=%s, index=%d, result=%s\n",
                      __func__, mode_flrig, i, modeMap[i].mode_flrig);
            return;
        }
    }
}

/*
* flrig_open
* Assumes rig!=NULL, rig->state.priv!=NULL
*/
static int flrig_open(RIG *rig)
{
    int retval;
    char value[MAXXMLLEN];
    //char arg[MAXXMLLEN];
    rmode_t modes;
    char *p;
    char *pr;
    split_t split;
    vfo_t tx_vfo;

    struct flrig_priv_data *priv = (struct flrig_priv_data *) rig->state.priv;

    ENTERFUNC;
    rig_debug(RIG_DEBUG_VERBOSE, "%s version %s\n", __func__, rig->caps->version);

    retval = flrig_transaction(rig, "main.get_version", NULL, value, sizeof(value));

    if (retval != RIG_OK)
    {
        rig_debug(RIG_DEBUG_ERR,
                  "%s: get_version failed: %s\nAssuming version < 1.3.54", __func__,
                  rigerror(retval));
        // we fall through and assume old version
    }

    int v1 = 0, v2 = 0, v3 = 0, v4 = 0;
    sscanf(value, "%d.%d.%d.%d", &v1, &v2, &v3, &v4);
    char version[32];
    sprintf(version, "%03d%03d%03d%03d", v1, v2, v3, v4);
    int iversion = 0;
    sscanf(version, "%d", &iversion);
    rig_debug(RIG_DEBUG_VERBOSE, "%s: version='%s'=%d\n", __func__, version,
              iversion);

    priv->has_verify_cmds = 0;

    if (iversion >= 1003054000) // 1.3.54 or greater
    {
        priv->has_verify_cmds = 1;
        rig_debug(RIG_DEBUG_VERBOSE, "%s: set_vfoA/ptt is available\n",
                  __func__);
    }

    rig_debug(RIG_DEBUG_VERBOSE, "%s FlRig version %s\n", __func__, value);

    retval = flrig_transaction(rig, "rig.get_xcvr", NULL, value, sizeof(value));

    if (retval != RIG_OK)
    {
        rig_debug(RIG_DEBUG_ERR, "%s: get_xcvr failed,,,not fatal: %s\n", __func__,
                  rigerror(retval));
    }

    strncpy(priv->info, value, sizeof(priv->info));
    rig_debug(RIG_DEBUG_VERBOSE, "Transceiver=%s\n", value);

    /* see if get_pwrmeter_scale is available */
    retval = flrig_transaction(rig, "rig.get_pwrmeter_scale", NULL, value,
                               sizeof(value));

    priv->powermeter_scale = 1; // default

    if (retval == RIG_OK)
    {
        priv->powermeter_scale = atof(value);
    }

    /* see if get_modeA is available */
    retval = flrig_transaction(rig, "rig.get_modeA", NULL, value, sizeof(value));

    if (retval == RIG_ENAVAIL) // must not have it
    {
        priv->has_get_modeA = 0;
        rig_debug(RIG_DEBUG_VERBOSE, "%s: getmodeA is not available=%s\n", __func__,
                  value);
    }
    else
    {
        priv->has_get_modeA = 1;
        rig_debug(RIG_DEBUG_VERBOSE, "%s: getmodeA is available\n", __func__);
    }

    freq_t freq;
    retval = flrig_get_freq(rig, RIG_VFO_CURR, &freq);

    if (retval != RIG_OK)
    {
        rig_debug(RIG_DEBUG_ERR, "%s: flrig_get_freq not working!!\n", __func__);
        RETURNFUNC(RIG_EPROTO);
    }

    /* see if get_bwA is available */
    retval = flrig_transaction(rig, "rig.get_bwA", NULL, value, sizeof(value));

    if (retval == RIG_ENAVAIL) // must not have it
    {
        priv->has_get_bwA = 0;
        rig_debug(RIG_DEBUG_VERBOSE, "%s: get_bwA is not available=%s\n", __func__,
                  value);
    }
    else
    {
        priv->has_get_bwA = 1;
        rig_debug(RIG_DEBUG_VERBOSE, "%s: get_bwA is available=%s\n", __func__, value);
    }

    /* see if set_bwA is available */
    retval = flrig_transaction(rig, "rig.set_bwA", NULL, value, sizeof(value));

    if (retval == RIG_ENAVAIL) // must not have it
    {
        priv->has_set_bwA = 0;
        rig_debug(RIG_DEBUG_VERBOSE, "%s: set_bwA is not available=%s\n", __func__,
                  value);
    }
    else
    {
        priv->has_set_bwA = 1;
        rig_debug(RIG_DEBUG_VERBOSE, "%s: set_bwA is available=%s\n", __func__, value);
    }

    retval = flrig_transaction(rig, "rig.get_AB", NULL, value, sizeof(value));

    if (retval != RIG_OK) { RETURNFUNC(retval); }

    if (streq(value, "A"))
    {
        rig->state.current_vfo = RIG_VFO_A;
    }
    else
    {
        rig->state.current_vfo = RIG_VFO_B;
    }

    rig_debug(RIG_DEBUG_TRACE, "%s: currvfo=%s value=%s\n", __func__,
              rig_strvfo(rig->state.current_vfo), value);
    //vfo_t vfo=RIG_VFO_A;
    //vfo_t vfo_tx=RIG_VFO_B; // split is always VFOB
    //flrig_get_split_vfo(rig, vfo, &priv->split, &vfo_tx);

    /* find out available widths and modes */
    retval = flrig_transaction(rig, "rig.get_modes", NULL, value, sizeof(value));

    if (retval != RIG_OK) { RETURNFUNC(retval); }

    rig_debug(RIG_DEBUG_VERBOSE, "%s: modes=%s\n", __func__, value);
    modes = 0;
    pr = value;

    /* The following modes in FLRig are not implemented yet
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

    for (p = strtok_r(value, "|", &pr); p != NULL; p = strtok_r(NULL, "|", &pr))
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

    rig_get_split_vfo(rig, RIG_VFO_A, &split, &tx_vfo);

    RETURNFUNC(retval);
}

/*
* flrig_close
* Assumes rig!=NULL
*/
static int flrig_close(RIG *rig)
{
    ENTERFUNC;

    RETURNFUNC(RIG_OK);
}

/*
* flrig_cleanup
* Assumes rig!=NULL, rig->state.priv!=NULL
*/
static int flrig_cleanup(RIG *rig)
{
    struct flrig_priv_data *priv;

    rig_debug(RIG_DEBUG_TRACE, "%s\n", __func__);

    if (!rig)
    {
        RETURNFUNC2(-RIG_EINVAL);
    }

    priv = (struct flrig_priv_data *)rig->state.priv;

    free(priv->ext_parms);
    free(rig->state.priv);

    rig->state.priv = NULL;

    // we really don't need to free this up as it's only done once
    // was causing problem when cleanup was followed by rig_open
    // model_flrig was not getting refilled
    // if we can figure out that one we can re-enable this
#if 0
    int i;

    for (i = 0; modeMap[i].mode_hamlib != 0; ++i)
    {
        if (modeMap[i].mode_flrig)
        {
            free(modeMap[i].mode_flrig);
            modeMap[i].mode_flrig = NULL;
            modeMap[i].mode_hamlib = 0;
        }

    }

#endif

    RETURNFUNC2(RIG_OK);
}

/*
* flrig_get_freq
* Assumes rig!=NULL, rig->state.priv!=NULL, freq!=NULL
*/
static int flrig_get_freq(RIG *rig, vfo_t vfo, freq_t *freq)
{
    char value[MAXARGLEN];
    struct flrig_priv_data *priv = (struct flrig_priv_data *) rig->state.priv;

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

    char *cmd = vfo == RIG_VFO_A ? "rig.get_vfoA" : "rig.get_vfoB";
    int retval;

    retval = flrig_transaction(rig, cmd, NULL, value, sizeof(value));

    if (retval != RIG_OK)
    {
        rig_debug(RIG_DEBUG_ERR, "%s: flrig_transaction failed retval=%s\n", __func__,
                  rigerror(retval));
        RETURNFUNC(retval);
    }

    *freq = atof(value);

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
* flrig_set_freq
* assumes rig!=NULL, rig->state.priv!=NULL
*/
static int flrig_set_freq(RIG *rig, vfo_t vfo, freq_t freq)
{
    int retval;
    char cmd_arg[MAXARGLEN];
    char *cmd;
    struct flrig_priv_data *priv = (struct flrig_priv_data *) rig->state.priv;

    rig_debug(RIG_DEBUG_TRACE, "%s\n", __func__);
    rig_debug(RIG_DEBUG_TRACE, "%s: vfo=%s freq=%.0f\n", __func__,
              rig_strvfo(vfo), freq);

    if (check_vfo(vfo) == FALSE)
    {
        rig_debug(RIG_DEBUG_ERR, "%s: unsupported VFO %s\n",
                  __func__, rig_strvfo(vfo));
        RETURNFUNC2(-RIG_EINVAL);
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
    rig_get_ext_parm(rig, TOK_FLRIG_VERIFY_FREQ, &val);
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

    retval = flrig_transaction(rig, cmd, cmd_arg, NULL, 0);

    if (retval != RIG_OK)
    {
        RETURNFUNC2(retval);
    }

    RETURNFUNC2(RIG_OK);
}

/*
* flrig_set_ptt
* Assumes rig!=NULL
*/
static int flrig_set_ptt(RIG *rig, vfo_t vfo, ptt_t ptt)
{
    int retval;
    char cmd_arg[MAXARGLEN];
    struct flrig_priv_data *priv = (struct flrig_priv_data *) rig->state.priv;

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
    rig_get_ext_parm(rig, TOK_FLRIG_VERIFY_FREQ, &val);
    rig_debug(RIG_DEBUG_VERBOSE, "%s: fast_set_ptt=%d\n", __func__, val.i);

    if (val.i) { cmd = "rig.set_ptt_fast"; }

    retval = flrig_transaction(rig, cmd, cmd_arg, NULL, 0);

    if (retval != RIG_OK)
    {
        RETURNFUNC(retval);
    }

    priv->ptt = ptt;

    RETURNFUNC(RIG_OK);
}

/*
* flrig_get_ptt
* Assumes rig!=NUL, ptt!=NULL
*/
static int flrig_get_ptt(RIG *rig, vfo_t vfo, ptt_t *ptt)
{
    char value[MAXCMDLEN];
    char xml[MAXXMLLEN];
    struct flrig_priv_data *priv = (struct flrig_priv_data *) rig->state.priv;

    ENTERFUNC;
    xml[0] = 0;
    value[0] = 0;
    rig_debug(RIG_DEBUG_TRACE, "%s: vfo=%s\n", __func__,
              rig_strvfo(vfo));

    int retval;

    retval = flrig_transaction(rig, "rig.get_ptt", NULL, value, sizeof(value));

    if (retval != RIG_OK)
    {
        RETURNFUNC(retval);
    }

    if (strlen(value) > 0)
    {
        xml_parse(xml, value, sizeof(value));
        *ptt = atoi(value);
        rig_debug(RIG_DEBUG_TRACE, "%s: '%s'\n", __func__, value);

        priv->ptt = *ptt;
    }

    RETURNFUNC(RIG_OK);
}

/*
* flrig_set_split_mode
* Assumes rig!=NULL
*/
static int flrig_set_split_mode(RIG *rig, vfo_t vfo, rmode_t mode,
                                pbwidth_t width)
{
    int retval;
    struct flrig_priv_data *priv = (struct flrig_priv_data *) rig->state.priv;

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

    retval = flrig_set_mode(rig, vfo, mode, width);
    rig_debug(RIG_DEBUG_TRACE, "%s: set mode=%s\n", __func__,
              rig_strrmode(mode));
    RETURNFUNC(retval);
}

/*
* flrig_set_mode
* Assumes rig!=NULL
*/
static int flrig_set_mode(RIG *rig, vfo_t vfo, rmode_t mode, pbwidth_t width)
{
    int retval;
    int needBW;
    int vfoSwitched;
    char cmd_arg[MAXCMDLEN];
    char *p;
    char *pttmode;
    char *ttmode;
    struct flrig_priv_data *priv = (struct flrig_priv_data *) rig->state.priv;

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
        retval = flrig_set_vfo(rig, RIG_VFO_B);

        if (retval < 0)
        {
            RETURNFUNC(retval);
        }
    }

    // Set the mode
    if (modeMapGetFLRig(mode))
    {
        ttmode = strdup(modeMapGetFLRig(mode));
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
        retval = flrig_transaction(rig, "rig.set_mode", cmd_arg, NULL, 0);
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

        retval = flrig_transaction(rig, cmd, cmd_arg, NULL, 0);
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

        if (vfo == RIG_VFO_A)
        {
            retval = flrig_transaction(rig, "rig.set_bwA", cmd_arg, NULL, 0);
        }
        else
        {
            retval = flrig_transaction(rig, "rig.set_bwB", cmd_arg, NULL, 0);
        }

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
        retval = flrig_set_vfo(rig, RIG_VFO_A);

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
* flrig_get_mode
* Assumes rig!=NULL, rig->state.priv!=NULL, mode!=NULL
*/
static int flrig_get_mode(RIG *rig, vfo_t vfo, rmode_t *mode, pbwidth_t *width)
{
    int retval;
    int vfoSwitched;
    char value[MAXCMDLEN];
    char *cmdp;
    vfo_t curr_vfo;
    rmode_t my_mode;
    struct flrig_priv_data *priv = (struct flrig_priv_data *) rig->state.priv;

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
        retval = flrig_set_vfo(rig, RIG_VFO_B);

        if (retval < 0)
        {
            RETURNFUNC(retval);
        }
    }

    cmdp = "rig.get_mode"; /* default to old way */

    if (priv->has_get_modeA)   /* change to new way if we can */
    {
        /* calling this way reduces VFO swapping */
        /* we get the cached value in flrig */
        /* vfo B may not be getting polled though in FLRig */
        /* so we may not be 100% accurate if op is twiddling knobs */
        cmdp = "rig.get_modeA";

        if (vfo == RIG_VFO_B) { cmdp = "rig.get_modeB"; }
    }

    retval = flrig_transaction(rig, cmdp, NULL, value, sizeof(value));

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
        /* we get the cached value in flrig */
        /* vfo B may not be getting polled though in FLRig */
        /* so we may not be 100% accurate if op is twiddling knobs */
        cmdp = "rig.get_bwA";

        if (vfo == RIG_VFO_B) { cmdp = "rig.get_bwB"; }
    }

    retval = flrig_transaction(rig, cmdp, NULL, value, sizeof(value));

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
        retval = flrig_set_vfo(rig, RIG_VFO_A);

        if (retval != RIG_OK)
        {
            RETURNFUNC(retval);
        }
    }

    RETURNFUNC(RIG_OK);
}

/*
* flrig_set_vfo
* assumes rig!=NULL
*/
static int flrig_set_vfo(RIG *rig, vfo_t vfo)
{
    int retval;
    char cmd_arg[MAXXMLLEN];
    struct rig_state *rs = &rig->state;
    struct flrig_priv_data *priv = (struct flrig_priv_data *) rig->state.priv;

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
    retval = flrig_transaction(rig, "rig.set_AB", cmd_arg, NULL, 0);

    if (retval != RIG_OK)
    {
        rig_debug(RIG_DEBUG_ERR, "%s: rig.set_AB failed: %s\n", __func__,
                  rigerror(retval));
        RETURNFUNC(retval);
    }

    rig->state.current_vfo = vfo;
    rs->tx_vfo = RIG_VFO_B; // always VFOB

    /* for some rigs FLRig turns off split when VFOA is selected */
    /* so if we are in split and asked for A we have to turn split back on */
    if (priv->split && vfo == RIG_VFO_A)
    {
        SNPRINTF(cmd_arg, sizeof(cmd_arg),
                 "<params><param><value><i4>%d</i4></value></param></params>",
                 priv->split);
        retval = flrig_transaction(rig, "rig.set_split", cmd_arg, NULL, 0);

        if (retval < 0)
        {
            RETURNFUNC(retval);
        }
    }

    RETURNFUNC(RIG_OK);
}

/*
* flrig_get_vfo
* assumes rig!=NULL, vfo != NULL
*/
static int flrig_get_vfo(RIG *rig, vfo_t *vfo)
{
    char value[MAXCMDLEN];

    ENTERFUNC;


    int retval;
    retval = flrig_transaction(rig, "rig.get_AB", NULL, value, sizeof(value));

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
* flrig_set_split_freq
* assumes rig!=NULL
*/
static int flrig_set_split_freq(RIG *rig, vfo_t vfo, freq_t tx_freq)
{
    int retval;
    char cmd_arg[MAXXMLLEN];
    freq_t qtx_freq;
    struct flrig_priv_data *priv = (struct flrig_priv_data *) rig->state.priv;

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
    retval = flrig_get_freq(rig, RIG_VFO_B, &qtx_freq);

    if (retval != RIG_OK) { RETURNFUNC(retval); }

    if (tx_freq == qtx_freq) { RETURNFUNC(RIG_OK); }

    SNPRINTF(cmd_arg, sizeof(cmd_arg),
             "<params><param><value><double>%.6f</double></value></param></params>",
             tx_freq);
    retval = flrig_transaction(rig, "rig.set_vfoB", cmd_arg, NULL, 0);

    if (retval < 0)
    {
        RETURNFUNC(retval);
    }

    priv->curr_freqB = tx_freq;

    RETURNFUNC(RIG_OK);
}

/*
* flrig_get_split_freq
* assumes rig!=NULL, tx_freq!=NULL
*/
static int flrig_get_split_freq(RIG *rig, vfo_t vfo, freq_t *tx_freq)
{
    int retval;
    struct flrig_priv_data *priv = (struct flrig_priv_data *) rig->state.priv;

    ENTERFUNC;
    rig_debug(RIG_DEBUG_TRACE, "%s: vfo=%s\n", __func__,
              rig_strvfo(vfo));

    retval = flrig_get_freq(rig, RIG_VFO_B, tx_freq);
    priv->curr_freqB = *tx_freq;
    RETURNFUNC(retval);
}

/*
* flrig_set_split_vfo
* assumes rig!=NULL, tx_freq!=NULL
*/
static int flrig_set_split_vfo(RIG *rig, vfo_t vfo, split_t split, vfo_t tx_vfo)
{
    int retval;
    vfo_t qtx_vfo;
    split_t qsplit;
    struct flrig_priv_data *priv = (struct flrig_priv_data *) rig->state.priv;
    char cmd_arg[MAXXMLLEN];

    ENTERFUNC;
    rig_debug(RIG_DEBUG_TRACE, "%s: tx_vfo=%s\n", __func__,
              rig_strvfo(tx_vfo));

    retval = flrig_get_split_vfo(rig, RIG_VFO_A, &qsplit, &qtx_vfo);

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
    retval = flrig_transaction(rig, "rig.set_split", cmd_arg, NULL, 0);

    if (retval < 0)
    {
        RETURNFUNC(retval);
    }

    priv->split = split;

    RETURNFUNC(RIG_OK);
}

/*
* flrig_get_split_vfo
* assumes rig!=NULL, tx_freq!=NULL
*/
static int flrig_get_split_vfo(RIG *rig, vfo_t vfo, split_t *split,
                               vfo_t *tx_vfo)
{
    char value[MAXCMDLEN];
    struct flrig_priv_data *priv = (struct flrig_priv_data *) rig->state.priv;

    ENTERFUNC;

    int retval;
    retval = flrig_transaction(rig, "rig.get_split", NULL, value, sizeof(value));

    if (retval < 0)
    {
        RETURNFUNC(retval);
    }

    *split = atoi(value);
    priv->split = *split;
    *tx_vfo = *split ? RIG_VFO_B : RIG_VFO_A;
    rig_debug(RIG_DEBUG_TRACE, "%s tx_vfo=%s, split=%d\n", __func__,
              rig_strvfo(*tx_vfo), *split);
    RETURNFUNC(RIG_OK);
}

/*
* flrig_set_split_freq_mode
* assumes rig!=NULL
*/
static int flrig_set_split_freq_mode(RIG *rig, vfo_t vfo, freq_t freq,
                                     rmode_t mode, pbwidth_t width)
{
    int retval;
    rmode_t qmode;
    pbwidth_t qwidth;
    struct flrig_priv_data *priv = (struct flrig_priv_data *) rig->state.priv;

    ENTERFUNC;

    // we always do split on VFOB
    retval = flrig_set_freq(rig, RIG_VFO_B, freq);

    if (retval != RIG_OK)
    {
        rig_debug(RIG_DEBUG_ERR, "%s flrig_set_freq failed\n", __func__);
        RETURNFUNC(retval);
    }

    // Make VFOB mode match VFOA mode, keep VFOB width
    retval = flrig_get_mode(rig, RIG_VFO_B, &qmode, &qwidth);

    if (retval != RIG_OK) { RETURNFUNC(retval); }

    if (qmode == priv->curr_modeA) { RETURNFUNC(RIG_OK); }

    if (priv->ptt)
    {
        rig_debug(RIG_DEBUG_VERBOSE, "%s set_mode call not made as PTT=1\n", __func__);
        RETURNFUNC(RIG_OK);  // just return OK and ignore this
    }

    retval = flrig_set_mode(rig, RIG_VFO_B, priv->curr_modeA, width);

    if (retval != RIG_OK)
    {
        rig_debug(RIG_DEBUG_ERR, "%s flrig_set_mode failed\n", __func__);
        RETURNFUNC(retval);
    }

    retval = flrig_set_vfo(rig, RIG_VFO_A);

    RETURNFUNC(retval);
}

/*
* flrig_get_split_freq_mode
* assumes rig!=NULL, freq!=NULL, mode!=NULL, width!=NULL
*/
static int flrig_get_split_freq_mode(RIG *rig, vfo_t vfo, freq_t *freq,
                                     rmode_t *mode, pbwidth_t *width)
{
    int retval;

    ENTERFUNC;

    if (vfo != RIG_VFO_CURR && vfo != RIG_VFO_TX)
    {
        RETURNFUNC(-RIG_ENTARGET);
    }

    retval = flrig_get_freq(rig, RIG_VFO_B, freq);

    if (RIG_OK == retval)
    {
        retval = flrig_get_mode(rig, vfo, mode, width);
    }

    RETURNFUNC(retval);
}

static int flrig_set_level(RIG *rig, vfo_t vfo, setting_t level, value_t val)
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


    retval = flrig_transaction(rig, cmd, cmd_arg, NULL, 0);

    if (retval < 0)
    {
        RETURNFUNC(retval);
    }

    RETURNFUNC(RIG_OK);
}

/*
* flrig_get_level
* Assumes rig!=NULL, rig->state.priv!=NULL, val!=NULL
*/
static int flrig_get_level(RIG *rig, vfo_t vfo, setting_t level, value_t *val)
{
    char value[MAXARGLEN];
    char *cmd;
    int retval;
    struct flrig_priv_data *priv = (struct flrig_priv_data *) rig->state.priv;

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

    retval = flrig_transaction(rig, cmd, NULL, value, sizeof(value));

    if (retval != RIG_OK)
    {
        rig_debug(RIG_DEBUG_ERR, "%s: flrig_transaction failed retval=%s\n", __func__,
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

/*
* flrig_get_info
* assumes rig!=NULL
*/
static const char *flrig_get_info(RIG *rig)
{
    struct flrig_priv_data *priv = (struct flrig_priv_data *) rig->state.priv;

    return (priv->info);
}

static int flrig_power2mW(RIG *rig, unsigned int *mwpower, float power,
                          freq_t freq, rmode_t mode)
{
    struct flrig_priv_data *priv = (struct flrig_priv_data *) rig->state.priv;
    ENTERFUNC;
    rig_debug(RIG_DEBUG_TRACE, "%s: passed power = %f\n", __func__, power);
    rig_debug(RIG_DEBUG_TRACE, "%s: passed freq = %"PRIfreq" Hz\n", __func__, freq);
    rig_debug(RIG_DEBUG_TRACE, "%s: passed mode = %s\n", __func__,
              rig_strrmode(mode));

    power *= priv->powermeter_scale;
    *mwpower = (power * 100000);

    RETURNFUNC(RIG_OK);
}

static int flrig_mW2power(RIG *rig, float *power, unsigned int mwpower,
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

static int flrig_set_ext_parm(RIG *rig, token_t token, value_t val)
{
    struct flrig_priv_data *priv = (struct flrig_priv_data *)rig->state.priv;
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
    case TOK_FLRIG_VERIFY_FREQ:
    case TOK_FLRIG_VERIFY_PTT:
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

static int flrig_get_ext_parm(RIG *rig, token_t token, value_t *val)
{
    struct flrig_priv_data *priv = (struct flrig_priv_data *)rig->state.priv;
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
    case TOK_FLRIG_VERIFY_FREQ:
    case TOK_FLRIG_VERIFY_PTT:
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

HAMLIB_EXPORT(int) flrig_cat_string(RIG *rig, const char *arg)
{
    int retval;
    char cmd_arg[MAXARGLEN];
    rig_debug(RIG_DEBUG_VERBOSE, "%s called: %s\n", __func__, arg);
    SNPRINTF(cmd_arg, sizeof(cmd_arg),
             "<params><param><value>%s</value></param></params>", arg);
    retval = flrig_transaction(rig, "rig.cat_string", cmd_arg, NULL, 0);
    return retval;
}

HAMLIB_EXPORT(int) flrig_set_func(RIG *rig, vfo_t vfo, setting_t func,
                                  int status)
{
    int retval;
    char cmd_arg[MAXARGLEN];
    rig_debug(RIG_DEBUG_VERBOSE, "%s called: level=%s, status=%d\n", __func__,
              rig_strfunc(func), status);

    switch (func)
    {
    case RIG_FUNC_TUNER:
        SNPRINTF(cmd_arg, sizeof(cmd_arg),
                 "<params><param><value>%d</value></param></params>", status);
        retval = flrig_transaction(rig, "rig.tune", cmd_arg, NULL, 0);

    default:
        retval = -RIG_ENIMPL;
    }

    return retval;
}

#if 0
static int flrig_set_ext_parm(RIG *rig, setting_t parm, value_t val)
{
    struct flrig_priv_data *priv = (struct flrig_priv_data *)rig->state.priv;
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

static int flrig_get_ext_parm(RIG *rig, setting_t parm, value_t *val)
{
    struct flrig_priv_data *priv = (struct flrig_priv_data *)rig->state.priv;
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
