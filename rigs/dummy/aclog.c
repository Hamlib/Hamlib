/*
*  Hamlib ACLog backend - main file
*  Copyright (c) 2023 by Michael Black W9MDB
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
#include <serial.h>
#include <misc.h>

#define DEBUG 1
#define DEBUG_TRACE DEBUG_VERBOSE
#define TRUE 1
#define FALSE 0


#define MAXCMDLEN 8192
#define MAXXMLLEN 8192
#define MAXARGLEN 128
#define MAXBANDWIDTHLEN 4096

#define DEFAULTPATH "127.0.0.1:1100"

#define ACLOG_VFOS (RIG_VFO_A)

#define ACLOG_MODES (RIG_MODE_AM | RIG_MODE_PKTAM | RIG_MODE_CW | RIG_MODE_CWR |\
                    RIG_MODE_RTTY | RIG_MODE_RTTYR |\
                    RIG_MODE_PKTLSB | RIG_MODE_PKTUSB |\
                    RIG_MODE_SSB | RIG_MODE_LSB | RIG_MODE_USB |\
                    RIG_MODE_FM | RIG_MODE_WFM | RIG_MODE_FMN | RIG_MODE_PKTFM |\
                    RIG_MODE_C4FM)

#define streq(s1,s2) (strcmp(s1,s2)==0)

struct aclog_priv_data
{
    vfo_t curr_vfo;
    char bandwidths[MAXBANDWIDTHLEN]; /* pipe delimited set returned from aclog */
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
    float powermeter_scale;  /* So we can scale power meter to 0-1 */
    value_t parms[RIG_SETTING_MAX];
    struct ext_list *ext_parms;
};

//Structure for mapping aclog dynmamic modes to hamlib modes
//aclog displays modes as the rig displays them
struct s_modeMap
{
    rmode_t mode_hamlib;
    char *mode_aclog;
};

//ACLog will provide us the modes for the selected rig
//We will then put them in this struct
static struct s_modeMap modeMap[] =
{
    {RIG_MODE_USB, "|USB|"},
    {RIG_MODE_USB, "|SSB|"},
    {RIG_MODE_LSB, "|LSB|"},
    {RIG_MODE_PKTUSB, NULL},
    {RIG_MODE_PKTLSB, NULL},
    {RIG_MODE_AM, "|AM|"},
    {RIG_MODE_FM, "|FM|"},
    {RIG_MODE_FMN, NULL},
    {RIG_MODE_WFM, NULL},
    {RIG_MODE_CW, "|CW|"},
    {RIG_MODE_CWR, "|CWR|"},
    {RIG_MODE_RTTY, "|RTTY|"},
    {RIG_MODE_RTTYR, "|RTTYR|"},
    {RIG_MODE_C4FM, "|C4FM|"},
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
* read_transaction
* Assumes rig!=NULL, xml!=NULL, xml_len>=MAXXMLLEN
*/
static int read_transaction(RIG *rig, char *xml, int xml_len)
{
    int retval;
    int retry;
    char *delims;
    char *terminator = "</CMD>\r\n";

    ENTERFUNC;

    retry = 2;
    delims = "\n";
    xml[0] = 0;

    do
    {
        char tmp_buf[MAXXMLLEN];        // plenty big for expected aclog responses hopefully

        if (retry < 2)
        {
            rig_debug(RIG_DEBUG_WARN, "%s: retry needed? retry=%d\n", __func__, retry);
        }

        int len = read_string(RIGPORT(rig), (unsigned char *) tmp_buf, sizeof(tmp_buf),
                              delims,
                              strlen(delims), 0, 1);
        rig_debug(RIG_DEBUG_TRACE, "%s: string='%s'\n", __func__, tmp_buf);

        // if our first response we should see the HTTP header
        if (strlen(xml) == 0 && strstr(tmp_buf, "<CMD>") == NULL)
        {
            rig_debug(RIG_DEBUG_ERR, "%s: Expected '</CMD>', got '%s'\n", __func__,
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
    hamlib_port_t *rp = RIGPORT(rig);

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
    rig_flush(rp);

    while (try-- >= 0 && retval != RIG_OK)
        {
            retval = write_block(rp, (unsigned char *) xml, strlen(xml));

            if (retval  < 0)
            {
                RETURNFUNC(-RIG_EIO);
            }
        }

    RETURNFUNC(retval);
}

static int aclog_transaction(RIG *rig, char *cmd, char *value,
                             int value_len)
{
    char xml[MAXXMLLEN];
    int retry = 3;

    ENTERFUNC;
    ELAPSED1;
    strcpy(xml, "UNKNOWN");

    set_transaction_active(rig);

    if (value)
    {
        value[0] = 0;
    }

    do
    {
        int retval;

        if (retry != 3)
        {
            rig_debug(RIG_DEBUG_VERBOSE, "%s: cmd=%s, retry=%d\n", __func__, cmd, retry);
        }

        retval = write_transaction(rig, cmd, strlen(cmd));

        if (retval != RIG_OK)
        {
            rig_debug(RIG_DEBUG_ERR, "%s: write_transaction error=%d\n", __func__, retval);

            // if we get RIG_EIO the socket has probably disappeared
            // so bubble up the error so port can re re-opened
            if (retval == -RIG_EIO) { set_transaction_inactive(rig); RETURNFUNC(retval); }

            hl_usleep(50 * 1000); // 50ms sleep if error
        }

        if (value)
        {
            read_transaction(rig, xml, sizeof(xml));    // this might time out -- that's OK
        }

        // we get an unknown response if function does not exist
        if (strstr(xml, "UNKNOWN")) { set_transaction_inactive(rig); RETURNFUNC(RIG_ENAVAIL); }

        if (value) { strncpy(value, xml, value_len); }

    }
    while (((value && strlen(value) == 0))
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
* aclog_init
* Assumes rig!=NULL
*/
static int aclog_init(RIG *rig)
{
    struct aclog_priv_data *priv;
    hamlib_port_t *rp = RIGPORT(rig);

    ENTERFUNC;
    rig_debug(RIG_DEBUG_TRACE, "%s version %s\n", __func__, rig->caps->version);

    rig->state.priv  = (struct aclog_priv_data *)calloc(1, sizeof(
                           struct aclog_priv_data));

    if (!rig->state.priv)
    {
        RETURNFUNC(-RIG_ENOMEM);
    }

    priv = rig->state.priv;

    memset(priv, 0, sizeof(struct aclog_priv_data));
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

    strncpy(rp->pathname, DEFAULTPATH, sizeof(rp->pathname));

    RETURNFUNC(RIG_OK);
}

/*
* modeMapGet
* Assumes mode!=NULL
* Return the string for ACLog for the given hamlib mode
*/
static const char *modeMapGet(rmode_t modeHamlib)
{
    int i;

    rig_debug(RIG_DEBUG_TRACE, "%s: called\n", __func__);

    for (i = 0; modeMap[i].mode_hamlib != 0; ++i)
    {
        if (modeMap[i].mode_aclog == NULL) { continue; }

        rig_debug(RIG_DEBUG_TRACE,
                  "%s: checking modeMap[%d]=%.0f to modeHamlib=%.0f, mode_aclog='%s'\n", __func__,
                  i, (double)modeMap[i].mode_hamlib, (double)modeHamlib, modeMap[i].mode_aclog);

        if (modeMap[i].mode_hamlib == modeHamlib && strlen(modeMap[i].mode_aclog) > 0)
        {
            rig_debug(RIG_DEBUG_TRACE, "%s matched mode=%.0f, returning '%s'\n", __func__,
                      (double)modeHamlib, modeMap[i].mode_aclog);
            return (modeMap[i].mode_aclog);
        }
    }

    rig_debug(RIG_DEBUG_ERR, "%s: ACLog does not have mode: %s\n", __func__,
              rig_strrmode(modeHamlib));
    return ("ERROR");
}

/*
* modeMapGetHamlib
* Assumes mode!=NULL
* Return the hamlib mode from the given ACLog string
*/
static rmode_t modeMapGetHamlib(const char *modeACLog)
{
    int i;
    char modeCheck[64];

    SNPRINTF(modeCheck, sizeof(modeCheck), "|%s|", modeACLog);

    for (i = 0; modeMap[i].mode_hamlib != 0; ++i)
    {
        rig_debug(RIG_DEBUG_TRACE, "%s: find '%s' in '%s'\n", __func__,
                  modeCheck, modeMap[i].mode_aclog);

        if (modeMap[i].mode_aclog
                && strcmp(modeMap[i].mode_aclog, modeCheck) == 0)
        {
            return (modeMap[i].mode_hamlib);
        }
    }

    rig_debug(RIG_DEBUG_TRACE, "%s: mode requested: %s, not in modeMap\n", __func__,
              modeACLog);
    return (RIG_MODE_NONE);
}

/*
* aclog_get_freq
* Assumes rig!=NULL, rig->state.priv!=NULL, freq!=NULL
*/
static int aclog_get_freq(RIG *rig, vfo_t vfo, freq_t *freq)
{
    char value[MAXARGLEN];
    struct aclog_priv_data *priv = (struct aclog_priv_data *) rig->state.priv;

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

    char *cmd = "<CMD><READBMF></CMD>\r\n";
    int retval;

    retval = aclog_transaction(rig, cmd, value, sizeof(value));

    if (retval != RIG_OK)
    {
        rig_debug(RIG_DEBUG_ERR, "%s: READBMF failed retval=%s\n", __func__,
                  rigerror(retval));
        RETURNFUNC(retval);
    }

    char *p = strstr(value, "<FREQ>");
    *freq = 0;

    if (p) { sscanf(p, "<FREQ>%lf", freq); }

    *freq *= 1e6; // convert from MHz to Hz

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
    else // future support in ACLOG maybe?
    {
        priv->curr_freqB = *freq;
    }

    RETURNFUNC(RIG_OK);
}

/*
* aclog_get_mode
* Assumes rig!=NULL, rig->state.priv!=NULL, mode!=NULL
*/
static int aclog_get_mode(RIG *rig, vfo_t vfo, rmode_t *mode, pbwidth_t *width)
{
    int retval;
    char value[MAXCMDLEN];
    char *cmdp;
    struct aclog_priv_data *priv = (struct aclog_priv_data *) rig->state.priv;

    ENTERFUNC;
    rig_debug(RIG_DEBUG_TRACE, "%s: vfo=%s\n", __func__,
              rig_strvfo(vfo));

    if (check_vfo(vfo) == FALSE)
    {
        rig_debug(RIG_DEBUG_ERR, "%s: unsupported VFO %s\n",
                  __func__, rig_strvfo(vfo));
        RETURNFUNC(-RIG_EINVAL);
    }

    cmdp = "<CMD><READBMF></CMD>\r\n"; /* default to old way */

    retval = aclog_transaction(rig, cmdp, value, sizeof(value));

    if (retval != RIG_OK)
    {
        rig_debug(RIG_DEBUG_ERR, "%s: %s failed: %s\n", __func__, cmdp,
                  rigerror(retval));
        RETURNFUNC(retval);
    }

    char *p = strstr(value, "<MODE>");
    char modetmp[32];
    modetmp[0] = 0;

    if (p)
    {
        *mode = RIG_MODE_NONE;
        int n = sscanf(p, "<MODE>%31[^<]", modetmp);

        if (n) { *mode = modeMapGetHamlib(modetmp); }
        else
        {
            rig_debug(RIG_DEBUG_ERR, "%s: Unable to parse <MODE> from '%s'\n", __func__,
                      value);
            *mode = RIG_MODE_USB; // just default to USB if we fail parsing
        }
    }

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

    *width = 2400; // just default to 2400 for now

    RETURNFUNC(RIG_OK);
}
/*
* aclog_open
* Assumes rig!=NULL, rig->state.priv!=NULL
*/
static int aclog_open(RIG *rig)
{
    int retval;
    char value[MAXARGLEN];
    char *p;

    //;struct aclog_priv_data *priv = (struct aclog_priv_data *) rig->state.priv;

    ENTERFUNC;
    rig_debug(RIG_DEBUG_VERBOSE, "%s version %s\n", __func__, rig->caps->version);

    retval = aclog_transaction(rig, "<CMD><PROGRAM></CMD>\r\n", value,
                               sizeof(value));

    if (retval != RIG_OK)
    {
        rig_debug(RIG_DEBUG_ERR,
                  "%s: PROGRAM failed: %s", __func__, rigerror(retval));
    }

    rig_debug(RIG_DEBUG_TRACE, "%s: returned value=%s\n", __func__, value);
    char version_pgm[64];
    sscanf(value,
           "<CMD><PROGRAMRESPONSE><PGM>N3FJP's Amateur Contact Log</PGM><VER>%63[^<]",
           version_pgm);
    rig_debug(RIG_DEBUG_VERBOSE, "%s: ACLog version=%s\n", __func__, version_pgm);

    double version_api = 0;
    p = strstr(value, "<APIVER>");

    if (p) { sscanf(strstr(value, "<APIVER>"), "<APIVER>%lf", &version_api); }

    rig_debug(RIG_DEBUG_VERBOSE, "%s ACLog API version %.1lf\n", __func__,
              version_api);

    retval = aclog_transaction(rig, "<CMD><RIGENABLED></CMD>\r\n", value,
                               sizeof(value));

    if (retval != RIG_OK)
    {
        rig_debug(RIG_DEBUG_ERR, "%s: RIGENABLED failed,,,not fatal: %s\n", __func__,
                  rigerror(retval));
    }

    p = strstr(value, "<RIG>");
    char transceiver[64];
    strcpy(transceiver, "Unknown");

    if (p) { sscanf(p, "<RIG>%63[^<]", transceiver); }

    rig_debug(RIG_DEBUG_VERBOSE, "Transceiver=%s\n", transceiver);

    freq_t freq;
    retval = aclog_get_freq(rig, RIG_VFO_CURR, &freq);

    if (retval != RIG_OK)
    {
        rig_debug(RIG_DEBUG_ERR, "%s: aclog_get_freq not working!!\n", __func__);
        RETURNFUNC(RIG_EPROTO);
    }

    rig->state.current_vfo = RIG_VFO_A;
    rig_debug(RIG_DEBUG_TRACE, "%s: currvfo=%s value=%s\n", __func__,
              rig_strvfo(rig->state.current_vfo), value);

    RETURNFUNC(retval);
}

/*
* aclog_close
* Assumes rig!=NULL
*/
static int aclog_close(RIG *rig)
{
    ENTERFUNC;

    RETURNFUNC(RIG_OK);
}

/*
* aclog_cleanup
* Assumes rig!=NULL, rig->state.priv!=NULL
*/
static int aclog_cleanup(RIG *rig)
{
    struct aclog_priv_data *priv;

    rig_debug(RIG_DEBUG_TRACE, "%s\n", __func__);

    if (!rig)
    {
        RETURNFUNC2(-RIG_EINVAL);
    }

    priv = (struct aclog_priv_data *)rig->state.priv;

    free(priv->ext_parms);
    free(rig->state.priv);

    rig->state.priv = NULL;

    // we really don't need to free this up as it's only done once
    // was causing problem when cleanup was followed by rig_open
    // model_aclog was not getting refilled
    // if we can figure out that one we can re-enable this
#if 0
    int i;

    for (i = 0; modeMap[i].mode_hamlib != 0; ++i)
    {
        if (modeMap[i].mode_aclog)
        {
            free(modeMap[i].mode_aclog);
            modeMap[i].mode_aclog = NULL;
            modeMap[i].mode_hamlib = 0;
        }

    }

#endif

    RETURNFUNC2(RIG_OK);
}


/*
* aclog_set_freq
* assumes rig!=NULL, rig->state.priv!=NULL
*/
static int aclog_set_freq(RIG *rig, vfo_t vfo, freq_t freq)
{
    int retval;
    char cmd[MAXARGLEN];
    char value[1024];

    //struct aclog_priv_data *priv = (struct aclog_priv_data *) rig->state.priv;

    rig_debug(RIG_DEBUG_TRACE, "%s\n", __func__);
    rig_debug(RIG_DEBUG_TRACE, "%s: vfo=%s freq=%.0f\n", __func__,
              rig_strvfo(vfo), freq);

    if (check_vfo(vfo) == FALSE)
    {
        rig_debug(RIG_DEBUG_ERR, "%s: unsupported VFO %s\n",
                  __func__, rig_strvfo(vfo));
        RETURNFUNC2(-RIG_EINVAL);
    }

#if 0

    if (vfo == RIG_VFO_CURR)
    {
        vfo = rig->state.current_vfo;
    }

#endif

    SNPRINTF(cmd, sizeof(cmd),
             "<CMD><CHANGEFREQ><VALUE>%lf</VALUE><SUPPRESSMODEDEFAULT>TRUE</SUPPRESSMODEDEFAULT></CMD>\r\n",
             freq / 1e6);

    retval = aclog_transaction(rig, cmd, value, sizeof(value));

    if (retval != RIG_OK)
    {
        RETURNFUNC2(retval);
    }

    RETURNFUNC2(RIG_OK);
}


/*
* aclog_set_mode
* Assumes rig!=NULL
*/
static int aclog_set_mode(RIG *rig, vfo_t vfo, rmode_t mode, pbwidth_t width)
{
    int retval;
    char cmd[MAXCMDLEN];
    char *p;
    char *pttmode;
    char *ttmode = NULL;
    struct aclog_priv_data *priv = (struct aclog_priv_data *) rig->state.priv;

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
    rig_debug(RIG_DEBUG_TRACE, "%s: curr_vfo = %s\n", __func__,
              rig_strvfo(rig->state.current_vfo));

    // Set the mode
    if (strstr(modeMapGet(mode), "ERROR") == NULL)
    {
        ttmode = strdup(modeMapGet(mode));
    }
    else
    {
        rig_debug(RIG_DEBUG_ERR, "%s: modeMapGet failed on mode=%d\n", __func__,
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

    pttmode = ttmode;

    if (ttmode[0] == '|') { pttmode = &ttmode[1]; } // remove first pipe symbol

    p = strchr(pttmode, '|');

    if (p) { *p = 0; } // remove any other pipe

    SNPRINTF(cmd, sizeof(cmd),
             "<CMD><CHANGEMODE><VALUE>%s</VALUE></CMD>\r\n", pttmode);
    free(ttmode);

    retval = aclog_transaction(rig, cmd, NULL, 0);

    if (retval != RIG_OK)
    {
        rig_debug(RIG_DEBUG_ERR, "%s: failed: %s\n", __func__,
                  rigerror(retval));
        RETURNFUNC(retval);
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
* aclog_get_vfo
* assumes rig!=NULL, vfo != NULL
*/
static int aclog_get_vfo(RIG *rig, vfo_t *vfo)
{
    ENTERFUNC;

    *vfo = RIG_VFO_A;

    RETURNFUNC(RIG_OK);
}

/*
* aclog_get_info
* assumes rig!=NULL
*/
static const char *aclog_get_info(RIG *rig)
{
    const struct aclog_priv_data *priv = (struct aclog_priv_data *) rig->state.priv;

    return (priv->info);
}

static int aclog_power2mW(RIG *rig, unsigned int *mwpower, float power,
                          freq_t freq, rmode_t mode)
{
    const struct aclog_priv_data *priv = (struct aclog_priv_data *) rig->state.priv;
    ENTERFUNC;
    rig_debug(RIG_DEBUG_TRACE, "%s: passed power = %f\n", __func__, power);
    rig_debug(RIG_DEBUG_TRACE, "%s: passed freq = %"PRIfreq" Hz\n", __func__, freq);
    rig_debug(RIG_DEBUG_TRACE, "%s: passed mode = %s\n", __func__,
              rig_strrmode(mode));

    power *= priv->powermeter_scale;
    *mwpower = (power * 100000);

    RETURNFUNC(RIG_OK);
}

static int aclog_mW2power(RIG *rig, float *power, unsigned int mwpower,
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

/*
* aclog_set_ptt
* Assumes rig!=NULL
*/
static int aclog_set_ptt(RIG *rig, vfo_t vfo, ptt_t ptt)
{
    int retval;
    char cmd[MAXCMDLEN];
    struct aclog_priv_data *priv = (struct aclog_priv_data *) rig->state.priv;

    ENTERFUNC;
    rig_debug(RIG_DEBUG_TRACE, "%s: ptt=%d\n", __func__, ptt);


    if (check_vfo(vfo) == FALSE)
    {
        rig_debug(RIG_DEBUG_ERR, "%s: unsupported VFO %s\n",
                  __func__, rig_strvfo(vfo));
        RETURNFUNC(-RIG_EINVAL);
    }

    snprintf(cmd, sizeof(cmd),
             ptt == RIG_PTT_ON ? "<CMD><RIGTX></CMD>\r\n" : "<CMD><RIGRX></CMD>\r\n");

    retval = aclog_transaction(rig, cmd, NULL, 0);

    if (retval != RIG_OK)
    {
        RETURNFUNC(retval);
    }

    priv->ptt = ptt;

    RETURNFUNC(RIG_OK);
}


struct rig_caps aclog_caps =
{
    RIG_MODEL(RIG_MODEL_ACLOG),
    .model_name = "ACLog",
    .mfg_name = "N3FJP",
    .version = "20230120.0",
    .copyright = "LGPL",
    .status = RIG_STATUS_STABLE,
    .rig_type = RIG_TYPE_TRANSCEIVER,
    //.targetable_vfo =  RIG_TARGETABLE_FREQ | RIG_TARGETABLE_MODE,
    .ptt_type = RIG_PTT_RIG,
    .port_type = RIG_PORT_NETWORK,
    .write_delay = 0,
    .post_write_delay = 0,
    .timeout = 1000,
    .retry = 2,

    .filters =  {
        {RIG_MODE_ALL, RIG_FLT_ANY},
        RIG_FLT_END
    },

    .rx_range_list1 = {{
            .startf = kHz(1), .endf = GHz(10), .modes = ACLOG_MODES,
            .low_power = -1, .high_power = -1, ACLOG_VFOS, RIG_ANT_1
        },
        RIG_FRNG_END,
    },
    .tx_range_list1 = {RIG_FRNG_END,},
    .rx_range_list2 = {{
            .startf = kHz(1), .endf = GHz(10), .modes = ACLOG_MODES,
            .low_power = -1, .high_power = -1, ACLOG_VFOS, RIG_ANT_1
        },
        RIG_FRNG_END,
    },
    .tx_range_list2 = {RIG_FRNG_END,},
    .tuning_steps =  { {ACLOG_MODES, 1}, {ACLOG_MODES, RIG_TS_ANY}, RIG_TS_END, },
    .priv = NULL,               /* priv */

    .rig_init = aclog_init,
    .rig_open = aclog_open,
    .rig_close = aclog_close,
    .rig_cleanup = aclog_cleanup,

    .set_freq = aclog_set_freq,
    .get_freq = aclog_get_freq,
    .get_vfo = aclog_get_vfo,
    .set_mode = aclog_set_mode,
    .get_mode = aclog_get_mode,
    .get_info =      aclog_get_info,
    .set_ptt = aclog_set_ptt,
    //.get_ptt = aclog_get_ptt,
    .power2mW =   aclog_power2mW,
    .mW2power =   aclog_mW2power,
    .hamlib_check_rig_caps = HAMLIB_CHECK_RIG_CAPS
};
