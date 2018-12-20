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

#include "flrig.h"

#define DEBUG 1
#define DEBUG_TRACE DEBUG_VERBOSE

#define MAXCMDLEN 8192
#define MAXXMLLEN 8192
#define MAXBANDWIDTHLEN 4096

#define DEFAULTPATH "127.0.0.1:12345"

#define FLRIG_VFOS (RIG_VFO_A|RIG_VFO_B)

#define FLRIG_MODES (RIG_MODE_AM | RIG_MODE_PKTAM | RIG_MODE_CW | RIG_MODE_CWR |\
                     RIG_MODE_RTTY | RIG_MODE_RTTYR |\
                     RIG_MODE_PKTLSB | RIG_MODE_PKTUSB |\
                     RIG_MODE_SSB | RIG_MODE_LSB |\
		     RIG_MODE_FM | RIG_MODE_WFM | RIG_MODE_FMN |RIG_MODE_PKTFM )

#define streq(s1,s2) (strcmp(s1,s2)==0)

static int flrig_init(RIG *rig);
static int flrig_open(RIG *rig);
static int flrig_close(RIG *rig);
static int flrig_cleanup(RIG *rig);
static int flrig_set_freq(RIG *rig, vfo_t vfo, freq_t freq);
static int flrig_get_freq(RIG *rig, vfo_t vfo, freq_t *freq);
static int flrig_set_ptt(RIG *rig, vfo_t vfo, ptt_t ptt);
static int flrig_set_mode(RIG *rig, vfo_t vfo, rmode_t mode, pbwidth_t width);
static int flrig_set_split_mode(RIG *rig, vfo_t vfo, rmode_t mode, pbwidth_t width);
static int flrig_get_mode(RIG *rig, vfo_t vfo, rmode_t *mode, pbwidth_t *width);
static int flrig_get_vfo(RIG *rig, vfo_t *vfo);
static int flrig_set_vfo(RIG *rig, vfo_t vfo);
static int flrig_set_ptt(RIG *rig, vfo_t vfo, ptt_t ptt);
static int flrig_get_ptt(RIG *rig, vfo_t vfo, ptt_t *ptt);
static int flrig_set_split_freq(RIG *rig, vfo_t vfo, freq_t tx_freq);
static int flrig_get_split_freq(RIG *rig, vfo_t vfo, freq_t *tx_freq);
static int flrig_set_split_vfo(RIG *rig, vfo_t vfo, split_t split,
                               vfo_t tx_vfo);
static int flrig_get_split_vfo(RIG *rig, vfo_t vfo, split_t *split,
                               vfo_t *tx_vfo);
static int flrig_set_split_freq_mode(RIG *rig, vfo_t vfo, freq_t freq, rmode_t mode, pbwidth_t width);
static int flrig_get_split_freq_mode(RIG *rig, vfo_t vfo, freq_t *freq, rmode_t *mode, pbwidth_t *width);

static const char *flrig_get_info(RIG *rig);

struct flrig_priv_data {
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
};

const struct rig_caps flrig_caps = {
    .rig_model = RIG_MODEL_FLRIG,
    .model_name = "FLRig",
    .mfg_name = "FLRig",
    .version = BACKEND_VER,
    .copyright = "LGPL",
    .status = RIG_STATUS_STABLE,
    .rig_type = RIG_TYPE_TRANSCEIVER,
    .targetable_vfo =  RIG_TARGETABLE_FREQ|RIG_TARGETABLE_MODE,
    .ptt_type = RIG_PTT_RIG,
    .port_type = RIG_PORT_NETWORK,
    .write_delay = 0,
    .post_write_delay = 0,
    .timeout = 1000,
    .retry = 5,

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
            .start = kHz(1),.end = GHz(10),.modes = FLRIG_MODES,
            .low_power = -1,.high_power = -1, FLRIG_VFOS, RIG_ANT_1
        },
        RIG_FRNG_END,
    },
    .tx_range_list1 = {RIG_FRNG_END,},
    .rx_range_list2 = {{
            .start = kHz(1),.end = GHz(10),.modes = FLRIG_MODES,
            .low_power = -1,.high_power = -1, FLRIG_VFOS, RIG_ANT_1
        },
        RIG_FRNG_END,
    },
    .tx_range_list2 = {RIG_FRNG_END,},
    .tuning_steps =  { {FLRIG_MODES,1}, {FLRIG_MODES,RIG_TS_ANY}, RIG_TS_END, },
    .priv = NULL,               /* priv */

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
    .get_split_freq_mode = flrig_get_split_freq_mode
};

// Structure for mapping flrig dynmamic modes to hamlib modes
// flrig displays modes as the rig displays them
struct s_modeMap {
    int mode_hamlib;
    char *mode_flrig;
};

// FLRig will provide us the modes for the selected rig
// We will then put them in this struct
static struct s_modeMap modeMap[]= {
    {RIG_MODE_USB,NULL},
    {RIG_MODE_LSB,NULL},
    {RIG_MODE_PKTUSB,NULL},
    {RIG_MODE_PKTLSB,NULL},
    {RIG_MODE_PKTUSB,NULL},
    {RIG_MODE_PKTLSB,NULL},
    {RIG_MODE_AM,NULL},
    {RIG_MODE_FM,NULL},
    {RIG_MODE_FMN,NULL},
    {RIG_MODE_WFM,NULL},
    {RIG_MODE_CW,NULL},
    {RIG_MODE_CW,NULL},
    {RIG_MODE_CWR,NULL},
    {RIG_MODE_CWR,NULL},
    {RIG_MODE_RTTY,NULL},
    {RIG_MODE_RTTYR,NULL},
    {0,NULL}
};

/*
 * check_vfo
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

/* Rather than use some huge XML library we only need a few things
 * So we'll hand craft them
 * xml_build takes a value and return an xml string for FLRig
 */
static char *xml_build(char *cmd, char *value, char *xmlbuf,int xmllen)
{
    char xml[MAXXMLLEN];
    // We want at least a 4K buf to play with
    if (xmllen < 4096) {
        rig_debug(RIG_DEBUG_ERR, "%s: xmllen < 4096\n");
        return NULL;
    }
    sprintf(xmlbuf,
            "POST /RPC2 HTTP/1.1\r\n" "User-Agent: XMLRPC++ 0.8\r\n"
            "Host: 127.0.0.1:12345\r\n" "Content-type: text/xml\r\n");
    sprintf(xml, "<?xml version=\"1.0\"?>\r\n");
    strcat(xml, "<methodCall><methodName>");
    strcat(xml, cmd);
    strcat(xml, "</methodName>\r\n");

    if (value && strlen(value) > 0) {
        strcat(xml, value);
    }

    strcat(xml, "</methodCall>\r\n");
    strcat(xmlbuf, "Content-length: ");
    char tmp[32];
    sprintf(tmp, "%d\r\n\r\n", (int)strlen(xml));
    strcat(xmlbuf, tmp);
    strcat(xmlbuf, xml);
    return xmlbuf;
}

/* This is a very crude xml parse specific to what we need from FLRig
 * This works for strings, doubles, I4-type values, and arrays
 * Arrays are returned pipe delimited
 */
static char *xml_parse2(char *xml, char *value, int valueLen)
{
    char* delims = "<>\r\n ";
    char *xmltmp = strdup(xml);
    //rig_debug(RIG_DEBUG_TRACE, "%s: xml='%s'\n", __FUNCTION__,xml);
    char *p = strtok(xmltmp,delims);
    value[0]=0;
    while(p) {
        if (streq(p,"value")) {
            p=strtok(NULL,delims);
            if (streq(p,"array")) continue;
            if (streq(p,"/value")) continue; // empty value
            if (streq(p,"i4") || streq(p,"double")) {
                p = strtok(NULL,delims);
            }
            else if (streq(p,"array")) {
                p = strtok(NULL,delims);
                p = strtok(NULL,delims);
            }
            if (strlen(value)+strlen(p)+1 < valueLen) {
                if (value[0]!=0) strcat(value,"|");
                strcat(value,p);
            }
            else { // we'll just stop adding stuff
                rig_debug(RIG_DEBUG_ERR, "%s: max value length exceeded\n", __FUNCTION__);
            }
        }
        else {
            p=strtok(NULL,delims);
        }
    }
    rig_debug(RIG_DEBUG_TRACE, "%s: value returned='%s'\n", __FUNCTION__,value);
    if (rig_need_debug(RIG_DEBUG_WARN) && value != NULL && strlen(value)==0) {
        rig_debug(RIG_DEBUG_ERR, "%s: xml='%s'\n", __FUNCTION__,xml);
    }
    return value;
}

/*
 * xml_parse
 * Assumes xml!=NULL, value!=NULL, value_len big enough
 * returns the string value contained in the xml string
 */
static char *xml_parse(char *xml, char *value, int value_len)
{
    /* first off we should have an OK on the 1st line */
    if (strstr(xml, " 200 OK") == NULL) {
        return NULL;
    }
    rig_debug(RIG_DEBUG_TRACE, "%s XML:\n%s\n", __FUNCTION__, xml);

    // find the xml skipping the other stuff above it
    char *pxml = strstr(xml, "<?xml");

    if (pxml == NULL) {
        return NULL;
    }

    char *next = strchr(pxml + 1, '<');
    if (value != NULL) {
        xml_parse2(next, value, value_len);
    }
    if (strstr(value,"faultString")) {
        rig_debug(RIG_DEBUG_ERR, "%s error:\n%s\n", __FUNCTION__, value);
        value[0]=0; /* truncate to give empty response */
    }
    return value;
}

/*
 * read_transaction
 * Assumes rig!=NULL, xml!=NULL, xml_len>=MAXXMLLEN
 */
static int read_transaction(RIG *rig, char *xml, int xml_len)
{
    int retval;
    char *terminator = "</methodResponse>";

    rig_debug(RIG_DEBUG_TRACE, "%s\n", __FUNCTION__);

    struct rig_state *rs = &rig->state;
    rs->rigport.timeout = 1000; // 2 second read string timeout

    int retry=5;
    char *delims="\n";
    xml[0]=0;
    do {
        char tmp_buf[MAXXMLLEN];        // plenty big for expected flrig responses
        int len = read_string(&rs->rigport, tmp_buf, sizeof(tmp_buf), delims, strlen(delims));
        rig_debug(RIG_DEBUG_VERBOSE,"%s: string='%s'",__FUNCTION__,tmp_buf);
        if (len > 0) retry = 3;
        if (len <= 0) {
            rig_debug(RIG_DEBUG_ERR,"%s: read_string error=%d\n",__FUNCTION__,len);
            return -(100+RIG_EPROTO);
        }
        strcat(xml,tmp_buf);
    } while (retry-- > 0 && strstr(xml,terminator)==NULL);
    if (retry == 0) {
        rig_debug(RIG_DEBUG_WARN,"%s: retry timeout\n",__FUNCTION__);
    }
    if (strstr(xml,terminator)) {
        rig_debug(RIG_DEBUG_VERBOSE,"%s: got %s\n",__FUNCTION__,terminator);
        // Slow down just a bit -- not sure this is needed anymore but not a big deal here
        usleep(2*1000);
        retval = RIG_OK;
    }
    else {
        rig_debug(RIG_DEBUG_VERBOSE,"%s: did not get %s\n",__FUNCTION__,terminator);
        retval = -(101+RIG_EPROTO);
    }
    return retval;
}

/*
 * write_transaction
 * Assumes rig!=NULL, xml!=NULL, xml_len=total size of xml for response
 */
static int write_transaction(RIG *rig, char *xml, int xml_len)
{
    int try=rig->caps->retry;
    int retval=-RIG_EPROTO;
    char xmltmp[MAXXMLLEN];

    struct rig_state *rs = &rig->state;

    // This shouldn't ever happen...but just in case
    // We need to avoid and empty write as rigctld replies with blank line
    if (xml_len == 0) {
        rig_debug(RIG_DEBUG_ERR,"%s: len==0??\n",__FUNCTION__);
    }

    while(try-- >= 0 && retval != RIG_OK) {
            retval = write_block(&rs->rigport, xml, strlen(xml));
            if (retval  < 0) {
                return -RIG_EIO;
            }
        }
    strcpy(xml,xmltmp);
    return retval;
}

/*
 * flrig_init
 * Assumes rig!=NULL
 */
static int flrig_init(RIG *rig)
{

    rig_debug(RIG_DEBUG_TRACE, "%s version %s\n", __FUNCTION__, BACKEND_VER);

    struct flrig_priv_data *priv = (struct flrig_priv_data *)malloc(sizeof(struct flrig_priv_data));

    if (!priv) {
        return -RIG_ENOMEM;
    }

    memset(priv, 0, sizeof(struct flrig_priv_data));

    /*
     * set arbitrary initial status
     */
    rig->state.priv = (rig_ptr_t) priv;
    priv->curr_vfo = RIG_VFO_A;
    priv->split = 0;
    priv->ptt = 0;
    priv->curr_modeA = -1;
    priv->curr_modeB = -1;
    priv->curr_widthA = -1;
    priv->curr_widthB = -1;

    if (!rig || !rig->caps) {
        return -RIG_EINVAL;
    }

    strncpy(rig->state.rigport.pathname, DEFAULTPATH,
            sizeof(rig->state.rigport.pathname));

    return RIG_OK;
}

/*
 * modeMapGetFLRig
 * Assumes mode!=NULL
 * Return the string for FLRig for the given hamlib mode
 */
static char * modeMapGetFLRig(unsigned int modeHamlib)
{
    int i;
    for(i=0; modeMap[i].mode_hamlib!=0; ++i) {
        if (modeMap[i].mode_hamlib==modeHamlib) {
            return modeMap[i].mode_flrig;
        }
    }
    rig_debug(RIG_DEBUG_ERR,"%s: Unknown mode requested: %s\n",__FUNCTION__,rig_strrmode(modeHamlib));
    return "ERROR";
}

/*
 * modeMapGetHamlib
 * Assumes mode!=NULL
 * Return the hamlib mode from the given FLRig string
 */
static unsigned int modeMapGetHamlib(const char *modeFLRig)
{
    int i;
    char modeFLRigCheck[64];
    snprintf(modeFLRigCheck,sizeof(modeFLRigCheck),"|%.32s|",modeFLRig);
    rig_debug(RIG_DEBUG_VERBOSE,"%s: get hamlib mode from %s\n",__FUNCTION__,modeFLRig);
    for(i=0; modeMap[i].mode_hamlib!=0; ++i) {
        if (modeMap[i].mode_flrig && strstr(modeMap[i].mode_flrig,modeFLRigCheck)) {
            rig_debug(RIG_DEBUG_VERBOSE,"%s: got hamlib mode %s\n",__FUNCTION__,rig_strrmode(modeMap[i].mode_hamlib));
            return modeMap[i].mode_hamlib;
        }
    }
    rig_debug(RIG_DEBUG_ERR,"%s: Unknown mode requested: %s\n",__FUNCTION__,modeFLRig);
    return -RIG_EINVAL;
}


/*
 * modeMapAdd
 * Assumes modes!=NULL
 */
static void modeMapAdd(unsigned int *modes,int mode_hamlib,char *mode_flrig)
{
    int i;
    rig_debug(RIG_DEBUG_TRACE,"%s:mode_flrig=%s\n",__FUNCTION__,mode_flrig);
    int len1 = strlen(mode_flrig)+3; /* bytes needed for allocating */
    for(i=0; modeMap[i].mode_hamlib!=0; ++i) {
        if (modeMap[i].mode_hamlib==mode_hamlib) {
            *modes |= modeMap[i].mode_hamlib;
	    /* we will pipe delimit all the entries for easier matching */
	    /* all entries will have pipe symbol on both sides */
	    if (modeMap[i].mode_flrig == NULL) {
	        modeMap[i].mode_flrig = calloc(1,len1);
		if (modeMap[i].mode_flrig == NULL) {
                    rig_debug(RIG_DEBUG_ERR,"%s: error allocating memory for modeMap\n",__FUNCTION__);
		    return;
		}
	    }
            int len2 = strlen(modeMap[i].mode_flrig); /* current len w/o null */
	    modeMap[i].mode_flrig = realloc(modeMap[i].mode_flrig,strlen(modeMap[i].mode_flrig)+len1);
	    if (strlen(modeMap[i].mode_flrig)==0) modeMap[i].mode_flrig[0]='|';
            strncat(modeMap[i].mode_flrig,mode_flrig,len1+len2);
            strncat(modeMap[i].mode_flrig,"|",len1+len2);
            rig_debug(RIG_DEBUG_VERBOSE,"%s: Adding mode=%s at %d, index=%d, result=%s\n",__FUNCTION__,mode_flrig, mode_hamlib, i, modeMap[i].mode_flrig);
            return;
        }
    }
}

/*
 * flrig_open
 * Assumes rig!=NULL, rig->state.priv!=NULL
 */
static int flrig_open(RIG *rig) {
    int retval;
    char xml[MAXXMLLEN];
    char value[MAXXMLLEN];

    rig_debug(RIG_DEBUG_TRACE, "%s version %s\n", __FUNCTION__, BACKEND_VER);

    struct flrig_priv_data *priv = (struct flrig_priv_data *) rig->state.priv;

    char *pxml = xml_build("rig.get_xcvr", NULL,xml,sizeof(xml));
    retval = write_transaction(rig, pxml, strlen(pxml));
    if (retval < 0) {
        return retval;
    }
    read_transaction(rig, xml, sizeof(xml));
    xml_parse(xml, value, sizeof(value));
    strncpy(priv->info,value,sizeof(priv->info));
    rig_debug(RIG_DEBUG_VERBOSE,"Transceiver=%s\n", value);

    /* see if get_modeA is available */
    pxml = xml_build("rig.get_modeA", NULL, xml, sizeof(xml));
    retval = write_transaction(rig, pxml, strlen(pxml));
    read_transaction(rig, xml, sizeof(xml));
    xml_parse(xml, value, sizeof(value));
    if (strlen(value)>0) { /* must have it since we got an answer */
        priv->has_get_modeA = 1;
        rig_debug(RIG_DEBUG_VERBOSE,"%s: getmodeA is available=%s\n", __FUNCTION__, value);
    }
    else {
        rig_debug(RIG_DEBUG_VERBOSE,"%s: getmodeA is not available\n",__FUNCTION__);
    }
    /* see if get_bwA is available */
    pxml = xml_build("rig.get_bwA", NULL, xml, sizeof(xml));
    retval = write_transaction(rig, pxml, strlen(pxml));
    read_transaction(rig, xml, sizeof(xml));
    xml_parse(xml, value, sizeof(value));
    if (strlen(value)>0) { /* must have it since we got an answer */
        priv->has_get_bwA = 1;
        rig_debug(RIG_DEBUG_VERBOSE,"%s: get_bwA is available=%s\n", __FUNCTION__, value);
    }
    else {
        rig_debug(RIG_DEBUG_VERBOSE,"%s: get_bwA is not available\n",__FUNCTION__);
    }

    pxml = xml_build("rig.get_AB", value, xml, sizeof(xml));
    retval = write_transaction(rig, pxml, strlen(pxml));
    read_transaction(rig, xml, sizeof(xml));
    xml_parse(xml, value, sizeof(value));
    if (streq(value,"A")) {
        priv->curr_vfo = RIG_VFO_A;
    }
    else {
        priv->curr_vfo = RIG_VFO_B;
    }
    rig_debug(RIG_DEBUG_VERBOSE, "%s: currvfo=%s value=%s\n",__FUNCTION__, rig_strvfo(priv->curr_vfo), value);
    //vfo_t vfo=RIG_VFO_A;
    //vfo_t vfo_tx=RIG_VFO_B; // split is always VFOB
    //flrig_get_split_vfo(rig, vfo, &priv->split, &vfo_tx);

    /* find out available widths and modes */
    pxml = xml_build("rig.get_modes", NULL,xml,sizeof(xml));
    retval = write_transaction(rig, pxml, strlen(pxml));
    if (retval < 0) {
        return retval;
    }
    read_transaction(rig, xml, sizeof(xml));
    xml_parse(xml, value, sizeof(value));
    rig_debug(RIG_DEBUG_VERBOSE, "%s: modes=%s\n",__FUNCTION__, value);
    unsigned int modes = 0;
    char *p;
    for(p=strtok(value,"|"); p!=NULL; p=strtok(NULL,"|")) {
        if (streq(p,"USB"))           modeMapAdd(&modes,RIG_MODE_USB,p);
        else if (streq(p,"LSB"))      modeMapAdd(&modes,RIG_MODE_LSB,p);
        else if (streq(p,"USB-D"))    modeMapAdd(&modes,RIG_MODE_PKTUSB,p);
        else if (streq(p,"USB-D1"))   modeMapAdd(&modes,RIG_MODE_PKTUSB,p);
        else if (streq(p,"USB-D2"))   modeMapAdd(&modes,RIG_MODE_PKTUSB,p);
        else if (streq(p,"USB-D3"))   modeMapAdd(&modes,RIG_MODE_PKTUSB,p);
        else if (streq(p,"LSB-D"))    modeMapAdd(&modes,RIG_MODE_PKTLSB,p);
        else if (streq(p,"LSB-D1"))   modeMapAdd(&modes,RIG_MODE_PKTLSB,p);
        else if (streq(p,"LSB-D2"))   modeMapAdd(&modes,RIG_MODE_PKTLSB,p);
        else if (streq(p,"LSB-D3"))   modeMapAdd(&modes,RIG_MODE_PKTLSB,p);
        else if (streq(p,"DATA-USB")) modeMapAdd(&modes,RIG_MODE_PKTUSB,p);
        else if (streq(p,"D-USB"))    modeMapAdd(&modes,RIG_MODE_PKTUSB,p);
        else if (streq(p,"DATA-U"))   modeMapAdd(&modes,RIG_MODE_PKTUSB,p);
        else if (streq(p,"DATA-LSB")) modeMapAdd(&modes,RIG_MODE_PKTLSB,p);
        else if (streq(p,"D-LSB"))    modeMapAdd(&modes,RIG_MODE_PKTLSB,p);
        else if (streq(p,"DATA-L"))   modeMapAdd(&modes,RIG_MODE_PKTLSB,p);
        else if (streq(p,"DATA-R"))   modeMapAdd(&modes,RIG_MODE_PKTLSB,p);
        else if (streq(p,"PKT"))      modeMapAdd(&modes,RIG_MODE_PKTUSB,p);
        else if (streq(p,"PKT-U"))    modeMapAdd(&modes,RIG_MODE_PKTUSB,p);
        else if (streq(p,"PKT(U)"))   modeMapAdd(&modes,RIG_MODE_PKTUSB,p);
        else if (streq(p,"PKT-L"))    modeMapAdd(&modes,RIG_MODE_PKTLSB,p);
        else if (streq(p,"PKT(L)"))   modeMapAdd(&modes,RIG_MODE_PKTLSB,p);
        else if (streq(p,"FSK"))      modeMapAdd(&modes,RIG_MODE_PKTUSB,p);
        else if (streq(p,"FSK-R"))    modeMapAdd(&modes,RIG_MODE_PKTLSB,p);
        else if (streq(p,"PSK"))      modeMapAdd(&modes,RIG_MODE_PKTUSB,p);
        else if (streq(p,"PSK-R"))    modeMapAdd(&modes,RIG_MODE_PKTLSB,p);
        else if (streq(p,"PSK-U"))    modeMapAdd(&modes,RIG_MODE_PKTUSB,p);
        else if (streq(p,"PSK-L"))    modeMapAdd(&modes,RIG_MODE_PKTLSB,p);
        else if (streq(p,"AM"))       modeMapAdd(&modes,RIG_MODE_AM,p);
        else if (streq(p,"FM"))       modeMapAdd(&modes,RIG_MODE_FM,p);
        else if (streq(p,"AM-D"))     modeMapAdd(&modes,RIG_MODE_PKTAM,p);
        else if (streq(p,"FM-D"))     modeMapAdd(&modes,RIG_MODE_PKTFM,p);
        else if (streq(p,"FMN"))      modeMapAdd(&modes,RIG_MODE_FMN,p);
        else if (streq(p,"FM-N"))     modeMapAdd(&modes,RIG_MODE_FMN,p);
        else if (streq(p,"FMW"))      modeMapAdd(&modes,RIG_MODE_WFM,p);
        else if (streq(p,"WFM"))      modeMapAdd(&modes,RIG_MODE_WFM,p);
        else if (streq(p,"W-FM"))     modeMapAdd(&modes,RIG_MODE_WFM,p);
        else if (streq(p,"CW"))       modeMapAdd(&modes,RIG_MODE_CW,p);
        else if (streq(p,"CWU"))      modeMapAdd(&modes,RIG_MODE_CW,p);
        else if (streq(p,"CW-USB"))   modeMapAdd(&modes,RIG_MODE_CW,p);
        else if (streq(p,"CW-U"))     modeMapAdd(&modes,RIG_MODE_CW,p);
        else if (streq(p,"CW-LSB"))   modeMapAdd(&modes,RIG_MODE_CWR,p);
        else if (streq(p,"CW-L"))     modeMapAdd(&modes,RIG_MODE_CWR,p);
        else if (streq(p,"CW-R"))     modeMapAdd(&modes,RIG_MODE_CWR,p);
        else if (streq(p,"CWL"))      modeMapAdd(&modes,RIG_MODE_CWR,p);
        else if (streq(p,"RTTY"))     modeMapAdd(&modes,RIG_MODE_RTTY,p);
        else if (streq(p,"RTTY-U"))   modeMapAdd(&modes,RIG_MODE_RTTY,p);
        else if (streq(p,"RTTY-R"))   modeMapAdd(&modes,RIG_MODE_RTTYR,p);
        else if (streq(p,"RTTY-L"))   modeMapAdd(&modes,RIG_MODE_RTTYR,p);
        else if (streq(p,"RTTY(U)"))  modeMapAdd(&modes,RIG_MODE_RTTY,p);
        else if (streq(p,"RTTY(R"))   modeMapAdd(&modes,RIG_MODE_RTTYR,p);
        else if (streq(p,"DIG"))      modeMapAdd(&modes,RIG_MODE_PKTUSB,p);
        else rig_debug(RIG_DEBUG_ERR,"%s: Unknown mode for this rig='%s'\n",__FUNCTION__,p);
    }
    rig->state.mode_list = modes;
    rig_debug(RIG_DEBUG_VERBOSE, "%s: hamlib modes=0x%08x\n",__FUNCTION__, modes);

    return RIG_OK;
}

/*
 * flrig_close
 * Assumes rig!=NULL
 */
static int flrig_close(RIG *rig) {
    rig_debug(RIG_DEBUG_TRACE, "%s\n", __FUNCTION__);
    return RIG_OK;
}

/*
 * flrig_cleanup
 * Assumes rig!=NULL, rig->state.priv!=NULL
 */
static int flrig_cleanup(RIG *rig) {

    if (!rig)
        return -RIG_EINVAL;

    free(rig->state.priv);
    rig->state.priv = NULL;

    return RIG_OK;
}

/*
 * flrig_get_freq
 * Assumes rig!=NULL, rig->state.priv!=NULL, freq!=NULL
 */
static int flrig_get_freq(RIG *rig, vfo_t vfo, freq_t *freq)
{
    int retval;

    rig_debug(RIG_DEBUG_TRACE, "%s: vfo=%s\n", __FUNCTION__,
              rig_strvfo(vfo));

    struct flrig_priv_data *priv = (struct flrig_priv_data *) rig->state.priv;

    if (check_vfo(vfo) == FALSE) {
        rig_debug(RIG_DEBUG_ERR, "%s: unsupported VFO %s\n",
                  __FUNCTION__, rig_strvfo(vfo));
        return -RIG_EINVAL;
    }

    if (vfo == RIG_VFO_CURR) {
        vfo = priv->curr_vfo;
        rig_debug(RIG_DEBUG_VERBOSE, "%s: get_freq2 vfo=%s\n",
                  __FUNCTION__, rig_strvfo(vfo));
    }

    int retries=10;
    char xml[MAXXMLLEN];
    char value[MAXCMDLEN];
    do {
        char *pxml;
        if (vfo == RIG_VFO_A) {
            pxml = xml_build("rig.get_vfoA", NULL,xml,sizeof(xml));
        }
        else {
            pxml = xml_build("rig.get_vfoB", NULL,xml,sizeof(xml));
        }
        retval = write_transaction(rig, pxml, strlen(pxml));
        if (retval < 0) {
            return retval;
        }

        read_transaction(rig, xml, sizeof(xml));
        xml_parse(xml, value, sizeof(value));
        if (strlen(value)==0) {
            rig_debug(RIG_DEBUG_ERR, "%s: retries=%d\n",__FUNCTION__, retries);
            //usleep(10*1000);
        }
    } while (--retries && strlen(value)==0);

    *freq = atof(value);
    if (*freq == 0) {
        rig_debug(RIG_DEBUG_ERR, "%s: freq==0??\nvalue=%s\nxml=%s\n", __FUNCTION__,value,xml);
        return -(102+RIG_EPROTO);
    }
    else {
        rig_debug(RIG_DEBUG_VERBOSE, "%s: freq=%.0f\n", __FUNCTION__,*freq);
    }
    if (vfo == RIG_VFO_A) {
        priv->curr_freqA = *freq;
    }
    else {
        priv->curr_freqB = *freq;
    }
    return RIG_OK;
}

/*
 * flrig_set_freq
 * assumes rig!=NULL, rig->state.priv!=NULL
 */
static int flrig_set_freq(RIG *rig, vfo_t vfo, freq_t freq)
{
    int retval;

    rig_debug(RIG_DEBUG_TRACE, "%s: vfo=%s freq=%.0f\n", __FUNCTION__,
              rig_strvfo(vfo), freq);

    struct flrig_priv_data *priv = (struct flrig_priv_data *) rig->state.priv;

    if (check_vfo(vfo) == FALSE) {
        rig_debug(RIG_DEBUG_ERR, "%s: unsupported VFO %s\n",
                  __FUNCTION__, rig_strvfo(vfo));
        return -RIG_EINVAL;
    }

    if (vfo == RIG_VFO_CURR) {
        vfo = priv->curr_vfo;
    }
    else if (vfo == RIG_VFO_TX && priv->split) {
        vfo = RIG_VFO_B; // if split always TX on VFOB
    }

    char value[MAXXMLLEN];
    sprintf(value, "<params><param><value><double>%.0f</double></value></param></params>", freq);
    char xml[MAXXMLLEN];
    char *pxml;
    if (vfo == RIG_VFO_B) {
        pxml = xml_build("rig.set_vfoB", value, xml, sizeof(xml));
        rig_debug(RIG_DEBUG_VERBOSE,"rig.set_vfoB %s",value);
        priv->curr_freqB = freq;
    }
    else {
        pxml = xml_build("rig.set_vfoA", value, xml, sizeof(xml));
        rig_debug(RIG_DEBUG_VERBOSE,"rig.set_vfoA %s",value);
        priv->curr_freqA = freq;
    }
    retval = write_transaction(rig, pxml, strlen(pxml));
    if (retval < 0) {
        return retval;
    }
    if (vfo == RIG_VFO_B) {
        priv->curr_freqB = freq;
    }
    else {
        priv->curr_freqA = freq;
    }

    read_transaction(rig, xml, sizeof(xml)); // get response but don't care

    return RIG_OK;
}

/*
 * flrig_set_ptt
 * Assumes rig!=NULL
 */
static int flrig_set_ptt(RIG *rig, vfo_t vfo, ptt_t ptt)
{
    int retval;

    rig_debug(RIG_DEBUG_TRACE, "%s: ptt=%d\n", __FUNCTION__, ptt);

    struct flrig_priv_data *priv = (struct flrig_priv_data *) rig->state.priv;

    if (check_vfo(vfo) == FALSE) {
        rig_debug(RIG_DEBUG_ERR, "%s: unsupported VFO %s\n",
                  __FUNCTION__, rig_strvfo(vfo));
        return -RIG_EINVAL;
    }

    char cmd_buf[MAXCMDLEN];
    sprintf(cmd_buf,
            "<params><param><value><i4>%d</i4></value></param></params>",
            ptt);
    char xml[MAXXMLLEN];
    char *pxml = xml_build("rig.set_ptt", cmd_buf, xml, sizeof(xml));
    retval = write_transaction(rig, pxml, strlen(pxml));

    if (retval < 0) {
        return retval;
    }

    read_transaction(rig, xml, sizeof(xml)); // get response but don't care

    priv->ptt = ptt;

    return RIG_OK;
}

/*
 * flrig_get_ptt
 * Assumes rig!=NUL, ptt!=NULL
 */
static int flrig_get_ptt(RIG *rig, vfo_t vfo, ptt_t *ptt)
{
    int retval;

    rig_debug(RIG_DEBUG_TRACE, "%s: vfo=%s\n", __FUNCTION__,
              rig_strvfo(vfo));

    struct flrig_priv_data *priv = (struct flrig_priv_data *) rig->state.priv;

    char xml[MAXXMLLEN];
    char *pxml = xml_build("rig.get_ptt", NULL, xml, sizeof(xml));

    retval = write_transaction(rig, pxml, strlen(pxml));

    if (retval < 0) {
        return retval;
    }

    read_transaction(rig, xml, sizeof(xml));
    char value[MAXCMDLEN];
    xml_parse(xml, value, sizeof(value));
    *ptt = atoi(value);
    rig_debug(RIG_DEBUG_VERBOSE, "%s: '%s'\n", __FUNCTION__, value);

    priv->ptt = *ptt;

    return RIG_OK;
}

/*
 * flrig_set_split_mode
 * Assumes rig!=NULL
 */
static int flrig_set_split_mode(RIG *rig, vfo_t vfo, rmode_t mode, pbwidth_t width)
{
    int retval;
    rig_debug(RIG_DEBUG_TRACE, "%s: vfo=%s mode=%s width=%d\n",
              __FUNCTION__, rig_strvfo(vfo), rig_strrmode(mode), width);

    struct flrig_priv_data *priv = (struct flrig_priv_data *) rig->state.priv;

    switch(vfo) {
    case RIG_VFO_CURR:
        vfo = priv->curr_vfo;
        break;
    case RIG_VFO_TX:
        vfo = RIG_VFO_B;
        break;
    }
    // If no change don't do it...modes are kept up to date by client calls
    // to get_mode and set_mode so should be current here
    rig_debug(RIG_DEBUG_TRACE, "%s: vfoa privmode=%s\n",__FUNCTION__,rig_strrmode(priv->curr_modeA));
    rig_debug(RIG_DEBUG_TRACE, "%s: vfob privmode=%s\n",__FUNCTION__,rig_strrmode(priv->curr_modeB));
    // save some VFO swapping .. may replace with VFO specific calls that won't cause VFO change
    if (vfo==RIG_VFO_A && mode==priv->curr_modeA) return RIG_OK;
    if (vfo==RIG_VFO_B && mode==priv->curr_modeB) return RIG_OK;
    retval = flrig_set_mode(rig,vfo,mode,width);
    rig_debug(RIG_DEBUG_TRACE, "%s: set mode=%s\n",__FUNCTION__,rig_strrmode(mode));
    return retval;
}

/*
 * flrig_set_mode
 * Assumes rig!=NULL
 */
static int flrig_set_mode(RIG *rig, vfo_t vfo, rmode_t mode, pbwidth_t width)
{
    int retval;

    rig_debug(RIG_DEBUG_TRACE, "%s: vfo=%s mode=%s width=%d\n",
              __FUNCTION__, rig_strvfo(vfo), rig_strrmode(mode), width);

    struct flrig_priv_data *priv = (struct flrig_priv_data *) rig->state.priv;

    // if ptt is on do not set mode
    if (priv->ptt) return RIG_OK;

    if (vfo == RIG_VFO_CURR) {
        vfo = priv->curr_vfo;
    }

    if (check_vfo(vfo) == FALSE) {
        rig_debug(RIG_DEBUG_ERR, "%s: unsupported VFO %s\n",
                  __FUNCTION__, rig_strvfo(vfo));
        return -RIG_EINVAL;
    }
    if (priv->ptt) {
        rig_debug(RIG_DEBUG_WARN,"%s call not made as PTT=1\n",__FUNCTION__);
        return RIG_OK;  // just return OK and ignore this
    }

    // Switch to VFOB if appropriate since we can't set mode directly
    // MDB
    int vfoSwitched = 0;
    rig_debug(RIG_DEBUG_VERBOSE,"%s: curr_vfo = %s\n",__FUNCTION__,rig_strvfo(priv->curr_vfo));
    if (!priv->has_get_bwA && vfo == RIG_VFO_B && priv->curr_vfo != RIG_VFO_B) {
        vfoSwitched = 1;
        rig_debug(RIG_DEBUG_VERBOSE,"%s: switch to VFOB = %d\n",__FUNCTION__,vfoSwitched);
    }

    if (vfoSwitched) { // swap to B and we'll swap back later
        rig_debug(RIG_DEBUG_VERBOSE,"%s: switching to VFOB = %d\n",__FUNCTION__,vfoSwitched);
        retval = flrig_set_vfo(rig, RIG_VFO_B);
        if (retval < 0) {
            return retval;
        }
    }

    // Set the mode
    char *ttmode = modeMapGetFLRig(mode);

    char cmd_buf[MAXCMDLEN];
    sprintf(cmd_buf, "<params><param><value>%s</value></param></params>", ttmode);
    char xml[MAXXMLLEN];
    char *pxml=NULL;
    if (!priv->has_get_modeA) {
        pxml = xml_build("rig.set_mode", cmd_buf, xml, sizeof(xml));
    }
    else {
        char *cmd="rig.set_modeA";
        if (vfo==RIG_VFO_B) {
            cmd="rig.set_modeB";
        }
        pxml = xml_build(cmd, cmd_buf, xml, sizeof(xml));
    }

    retval = write_transaction(rig, pxml, strlen(pxml));
    if (retval < 0) {
        return retval;
    }

    // Get the response
    read_transaction(rig, xml, sizeof(xml));
    rig_debug(RIG_DEBUG_VERBOSE, "%s: response=%s\n", __FUNCTION__,xml);

    // Determine if we need to update the bandwidth
    int needBW=0;
    if (vfo == RIG_VFO_A) needBW = priv->curr_widthA != width;
    else if (vfo == RIG_VFO_B) needBW = priv->curr_widthB != width;
    else rig_debug(RIG_DEBUG_ERR,"%s: needBW unknown vfo=%s\n",__FUNCTION__,rig_strvfo(vfo));
    // Need to update the bandwidth
    if (width > 0 && needBW) {
        sprintf(cmd_buf, "<params><param><value><i4>%ld</i4></value></param></params>", width);
        // if we're not on VFOB but asking for VFOB still have to switch VFOS
        if (!vfoSwitched && vfo==RIG_VFO_B) flrig_set_vfo(rig,RIG_VFO_B);
        if (!vfoSwitched && vfo==RIG_VFO_A) flrig_set_vfo(rig,RIG_VFO_A);
        pxml = xml_build("rig.set_bandwidth", cmd_buf, xml, sizeof(xml));
        retval = write_transaction(rig, pxml, strlen(pxml));
        if (retval < 0) {
            return retval;
        }
        read_transaction(rig, xml, sizeof(xml));
        if (!vfoSwitched && vfo==RIG_VFO_B) flrig_set_vfo(rig,RIG_VFO_A);
        if (!vfoSwitched && vfo==RIG_VFO_A) flrig_set_vfo(rig,RIG_VFO_B);
    }

    // Return to VFOA if needed
    rig_debug(RIG_DEBUG_VERBOSE,"%s: switch to VFOA? = %d\n",__FUNCTION__,vfoSwitched);
    if (vfoSwitched) {
        rig_debug(RIG_DEBUG_VERBOSE,"%s: switching to VFOA\n",__FUNCTION__);
        retval = flrig_set_vfo(rig, RIG_VFO_A);
        if (retval < 0) {
            return retval;
        }
    }

    if (vfo == RIG_VFO_A) {
        priv->curr_modeA = mode;
        priv->curr_widthA = width;
    }
    else {
        priv->curr_modeB = mode;
        priv->curr_widthB = width;
    }

    return RIG_OK;
}

/*
 * flrig_get_mode
 * Assumes rig!=NULL, rig->state.priv!=NULL, mode!=NULL
 */
static int flrig_get_mode(RIG *rig, vfo_t vfo, rmode_t *mode, pbwidth_t *width)
{
    int retval;

    rig_debug(RIG_DEBUG_TRACE, "%s: vfo=%s\n", __FUNCTION__,
              rig_strvfo(vfo));

    struct flrig_priv_data *priv = (struct flrig_priv_data *) rig->state.priv;

    if (check_vfo(vfo) == FALSE) {
        rig_debug(RIG_DEBUG_ERR, "%s: unsupported VFO %s\n",
                  __FUNCTION__, rig_strvfo(vfo));
        return -RIG_EINVAL;
    }

    vfo_t curr_vfo = priv->curr_vfo;
    if (vfo == RIG_VFO_CURR) {
        vfo = priv->curr_vfo;
    }
    rig_debug(RIG_DEBUG_TRACE, "%s: using vfo=%s\n", __FUNCTION__,
              rig_strvfo(vfo));
    if (priv->ptt) {
        if (vfo == RIG_VFO_A) *mode = priv->curr_modeA;
        else *mode = priv->curr_modeB;
        rig_debug(RIG_DEBUG_WARN,"%s call not made as PTT=1\n",__FUNCTION__);
        return RIG_OK;  // just return OK and ignore this
    }

    // Switch to VFOB if appropriate
    int vfoSwitched=0;
    if (priv->has_get_modeA == 0 && vfo == RIG_VFO_B && curr_vfo != RIG_VFO_B) {
        vfoSwitched = 1;
    }

    if (vfoSwitched) {
        rig_debug(RIG_DEBUG_VERBOSE,"%s switch to VFOB=%d\n",__FUNCTION__,priv->has_get_modeA);
        retval = flrig_set_vfo(rig, RIG_VFO_B);
        if (retval < 0) {
            return retval;
        }
    }

    char xml[MAXXMLLEN];
    char *cmdp="rig.get_mode"; /* default to old way */
    if (priv->has_get_modeA) { /* change to new way if we can */
        /* calling this way reduces VFO swapping */
        /* we get the cached value in flrig */
        /* vfo B may not be getting polled though in FLRig */
        /* so we may not be 100% accurate if op is twiddling knobs */
        cmdp = "rig.get_modeA";
        if (vfo==RIG_VFO_B) cmdp = "rig.get_modeB";
    }
    char *pxml = xml_build(cmdp, NULL, xml, sizeof(xml));
    retval = write_transaction(rig, pxml, strlen(pxml));
    if (retval < 0) {
        return retval;
    }

    read_transaction(rig, xml, sizeof(xml));
    char value[MAXCMDLEN];
    xml_parse(xml, value, sizeof(value));
    retval = modeMapGetHamlib(value);
    if (retval < 0 ) {
        return retval;
    }
    *mode = retval;
    rig_debug(RIG_DEBUG_VERBOSE, "%s: mode='%s'\n", __FUNCTION__, rig_strrmode(*mode));
    if (vfo == RIG_VFO_A) {
        priv->curr_modeA = *mode;
    }
    else {
        priv->curr_modeB = *mode;
    }


    /* Get the bandwidth */
    cmdp="rig.get_bw"; /* default to old way */
    if (priv->has_get_bwA) { /* change to new way if we can */
        /* calling this way reduces VFO swapping */
        /* we get the cached value in flrig */
        /* vfo B may not be getting polled though in FLRig */
        /* so we may not be 100% accurate if op is twiddling knobs */
        cmdp = "rig.get_bwA";
        if (vfo==RIG_VFO_B) cmdp = "rig.get_bwB";
    }
    pxml = xml_build(cmdp, NULL, xml, sizeof(xml));
    retval = write_transaction(rig, pxml, strlen(pxml));
    if (retval < 0) {
        return retval;
    }

    read_transaction(rig, xml, sizeof(xml));
    xml_parse(xml, value, sizeof(value));
    rig_debug(RIG_DEBUG_VERBOSE, "%s: mode=%s width='%s'\n", __FUNCTION__, rig_strrmode(*mode), value);
    // we get 2 entries pipe separated for bandwidth, lower and upper
    if(strlen(value)>0) {
        char *p=value;
        /* we might get two values and then we want the 2nd one */
        if (strchr(value,'|')!=NULL) p=strchr(value,'|')+1;
        *width = atoi(p);
    }
    if (vfo == RIG_VFO_A) {
        priv->curr_widthA = *width;
    }
    else {
        priv->curr_widthB = *width;
    }

    // Return to VFOA if needed
    if (vfoSwitched) {
        retval = flrig_set_vfo(rig, RIG_VFO_A);
        if (retval < 0) {
            return retval;
        }
    }

    return RIG_OK;
}

/*
 * flrig_get_vfo
 * assumes rig!=NULL
 */
static int flrig_set_vfo(RIG *rig, vfo_t vfo)
{
    int retval;

    rig_debug(RIG_DEBUG_TRACE, "%s: vfo=%s\n", __FUNCTION__,
              rig_strvfo(vfo));

    struct rig_state *rs = &rig->state;
    struct flrig_priv_data *priv = (struct flrig_priv_data *) rig->state.priv;

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
        vfo = priv->curr_vfo;
    }

    char value[MAXCMDLEN];
    char xml[MAXXMLLEN];
    sprintf(value, "<params><param><value>%s</value></param></params>",
            vfo == RIG_VFO_A ? "A" : "B");
    char *pxml = xml_build("rig.set_AB", value, xml, sizeof(xml));
    retval = write_transaction(rig, pxml, strlen(pxml));

    if (retval < 0) {
        return retval;
    }
    priv->curr_vfo = vfo;
    rs->tx_vfo = RIG_VFO_B; // always VFOB
    read_transaction(rig, xml, sizeof(xml));

    /* for some rigs FLRig turns off split when VFOA is selected */
    /* so if we are in split and asked for A we have to turn split back on */
    if (priv->split && vfo==RIG_VFO_A) {
        char xml[MAXXMLLEN];
        char value[MAXCMDLEN];
        sprintf(value, "<params><param><value><i4>%d</i4></value></param></params>", priv->split);
        char *pxml = xml_build("rig.set_split", value, xml, sizeof(xml));
        retval = write_transaction(rig, pxml, strlen(pxml));
        if (retval < 0) {
            return retval;
        }
        read_transaction(rig, xml, sizeof(xml)); // get response but don't care
    }
    return RIG_OK;
}

/*
 * flrig_get_vfo
 * assumes rig!=NULL, vfo != NULL
 */
static int flrig_get_vfo(RIG *rig, vfo_t *vfo)
{
    int retval;

    rig_debug(RIG_DEBUG_TRACE, "%s\n", __FUNCTION__);

    struct flrig_priv_data *priv = (struct flrig_priv_data *) rig->state.priv;

    char xml[MAXXMLLEN];
    char *pxml = xml_build("rig.get_AB", NULL, xml, sizeof(xml));
    retval = write_transaction(rig, pxml, strlen(pxml));

    if (retval < 0) {
        return retval;
    }

    read_transaction(rig, xml, sizeof(xml));
    char value[MAXCMDLEN];
    xml_parse(xml, value, sizeof(value));
    rig_debug(RIG_DEBUG_VERBOSE, "%s: vfo value=%s\n", __FUNCTION__,value);

    switch (value[0]) {
    case 'A':
        *vfo = RIG_VFO_A;
        break;

    case 'B':
        *vfo = RIG_VFO_B;
        break;

    default:
        *vfo = RIG_VFO_CURR;
        return -RIG_EINVAL;
    }

    if (check_vfo(*vfo) == FALSE) {
        rig_debug(RIG_DEBUG_ERR, "%s: unsupported VFO %s\n",
                  __FUNCTION__, rig_strvfo(*vfo));
        return -RIG_EINVAL;
    }

    priv->curr_vfo = *vfo;

    rig_debug(RIG_DEBUG_VERBOSE, "%s: vfo=%s\n", __FUNCTION__,
              rig_strvfo(*vfo));

    return RIG_OK;
}

/*
 * flrig_set_split_freq
 * assumes rig!=NULL
 */
static int flrig_set_split_freq(RIG *rig, vfo_t vfo, freq_t tx_freq)
{
    int retval;

    rig_debug(RIG_DEBUG_TRACE, "%s: vfo=%s freq=%.1f\n", __FUNCTION__,
              rig_strvfo(vfo), tx_freq);

    struct flrig_priv_data *priv = (struct flrig_priv_data *) rig->state.priv;

    if (check_vfo(vfo) == FALSE) {
        rig_debug(RIG_DEBUG_ERR, "%s: unsupported VFO %s\n",
                  __FUNCTION__, rig_strvfo(vfo));
        return -RIG_EINVAL;
    }
    // we always split on VFOB so if no change just return
    freq_t qtx_freq;
    retval = flrig_get_freq(rig, RIG_VFO_B, &qtx_freq);
    if (retval != RIG_OK) return retval;
    if (tx_freq == qtx_freq) return RIG_OK;

    char xml[MAXXMLLEN];
    char value[MAXCMDLEN];
    sprintf(value,
            "<params><param><value><double>%.6f</double></value></param></params>", tx_freq);
    char *pxml = xml_build("rig.set_vfoB", value, xml, sizeof(xml));
    retval = write_transaction(rig, pxml, strlen(pxml));
    if (retval < 0) {
        return retval;
    }
    priv->curr_freqB = tx_freq;

    read_transaction(rig, xml, sizeof(xml)); // get response but don't care

    return RIG_OK;
}

/*
 * flrig_get_split_freq
 * assumes rig!=NULL, tx_freq!=NULL
 */
static int flrig_get_split_freq(RIG *rig, vfo_t vfo, freq_t *tx_freq)
{
    rig_debug(RIG_DEBUG_TRACE, "%s: vfo=%s\n", __FUNCTION__,
              rig_strvfo(vfo));

    struct flrig_priv_data *priv = (struct flrig_priv_data *) rig->state.priv;

    int retval = flrig_get_freq(rig, RIG_VFO_B, tx_freq);
    priv->curr_freqB = *tx_freq;
    return retval;
}

/*
 * flrig_set_split_vfo
 * assumes rig!=NULL, tx_freq!=NULL
 */
static int flrig_set_split_vfo(RIG *rig, vfo_t vfo, split_t split, vfo_t tx_vfo)
{
    int retval;

    rig_debug(RIG_DEBUG_TRACE, "%s: tx_vfo=%s\n", __FUNCTION__,
              rig_strvfo(tx_vfo));

    struct flrig_priv_data *priv = (struct flrig_priv_data *) rig->state.priv;

    if (tx_vfo == RIG_VFO_SUB || tx_vfo == RIG_VFO_TX) {
        tx_vfo = RIG_VFO_B;
    }
    vfo_t qtx_vfo;
    split_t qsplit;
    retval = flrig_get_split_vfo(rig, RIG_VFO_A, &qsplit, &qtx_vfo);
    if (retval != RIG_OK) return retval;
    if (split == qsplit) return RIG_OK;
    if (priv->ptt) {
        rig_debug(RIG_DEBUG_WARN,"%s call not made as PTT=1\n",__FUNCTION__);
        return RIG_OK;  // just return OK and ignore this
    }

    char xml[MAXXMLLEN];
    char value[MAXCMDLEN];
    sprintf(value, "<params><param><value><i4>%d</i4></value></param></params>", split);
    char *pxml = xml_build("rig.set_split", value, xml, sizeof(xml));
    retval = write_transaction(rig, pxml, strlen(pxml));
    if (retval < 0) {
        return retval;
    }
    priv->split = split;

    read_transaction(rig, xml, sizeof(xml)); // get response but don't care

    return RIG_OK;
}

/*
 * flrig_get_split_vfo
 * assumes rig!=NULL, tx_freq!=NULL
 */
static int flrig_get_split_vfo(RIG *rig, vfo_t vfo, split_t *split,
                               vfo_t *tx_vfo)
{
    int retval;

    rig_debug(RIG_DEBUG_TRACE, "%s\n", __FUNCTION__);
    struct flrig_priv_data *priv = (struct flrig_priv_data *) rig->state.priv;

    char xml[MAXXMLLEN];
    char *pxml = xml_build("rig.get_split", NULL, xml, sizeof(xml));
    retval = write_transaction(rig, pxml, strlen(pxml));

    if (retval < 0) {
        return retval;
    }

    char value[MAXCMDLEN];
    read_transaction(rig, xml, sizeof(xml));
    xml_parse(xml, value, sizeof(value));

    *tx_vfo = RIG_VFO_B;
    *split = atoi(value);
    priv->split = *split;
    rig_debug(RIG_DEBUG_VERBOSE,"%s tx_vfo=%s, split=%d\n",__FUNCTION__,rig_strvfo(*tx_vfo),*split);
    return RIG_OK;
}

/*
 * flrig_set_split_freq_mode
 * assumes rig!=NULL
 */
static int flrig_set_split_freq_mode(RIG *rig, vfo_t vfo, freq_t freq, rmode_t mode, pbwidth_t width)
{
    int retval;

    rig_debug(RIG_DEBUG_TRACE, "%s\n", __FUNCTION__);

    struct flrig_priv_data *priv = (struct flrig_priv_data *) rig->state.priv;

    if (vfo != RIG_VFO_CURR && vfo != RIG_VFO_TX)
        return -RIG_ENTARGET;

    if (priv->ptt) {
        rig_debug(RIG_DEBUG_WARN,"%s call not made as PTT=1\n",__FUNCTION__);
        return RIG_OK;  // just return OK and ignore this
    }

    retval = flrig_set_freq (rig, RIG_VFO_B, freq);
    if (retval != RIG_OK) {
        rig_debug(RIG_DEBUG_ERR, "%s flrig_set_freq failed\n", __FUNCTION__);
        return retval;
    }
    // Make VFOB mode match VFOA mode, keep VFOB width
    rmode_t qmode;
    pbwidth_t qwidth;
    retval = flrig_get_mode(rig,RIG_VFO_B,&qmode,&qwidth);
    if (retval != RIG_OK) return retval;
    if (qmode == priv->curr_modeA) return RIG_OK;
    retval = flrig_set_mode(rig,RIG_VFO_B,priv->curr_modeA,width);
    if (retval != RIG_OK) {
        rig_debug(RIG_DEBUG_ERR, "%s flrig_set_mode failed\n", __FUNCTION__);
        return retval;
    }

    retval = flrig_set_vfo(rig,RIG_VFO_A);

    return retval;
}

/*
 * flrig_get_split_freq_mode
 * assumes rig!=NULL, freq!=NULL, mode!=NULL, width!=NULL
 */
static int flrig_get_split_freq_mode(RIG *rig, vfo_t vfo, freq_t *freq, rmode_t *mode, pbwidth_t *width)
{
    int retval;

    if (vfo != RIG_VFO_CURR && vfo != RIG_VFO_TX)
        return -RIG_ENTARGET;

    retval = flrig_get_freq (rig, RIG_VFO_B, freq);
    if (RIG_OK == retval) {
        retval = flrig_get_mode(rig,vfo,mode,width);
    }

    return retval;
}

/*
 * flrig_get_info
 * assumes rig!=NULL
 */
static const char *flrig_get_info(RIG *rig)
{
    struct flrig_priv_data *priv = (struct flrig_priv_data *) rig->state.priv;
    rig_debug(RIG_DEBUG_TRACE, "%s called\n", __FUNCTION__);

    return priv->info;
}
