
/*
 *  Hamlib FLRig backend - main file
 *  Copyright (c) 2017 by Michael Black W9MDB
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

#define MAXCMDLEN 8192

#define DEFAULTPATH "localhost:12345"

#define FLRIG_VFOS (RIG_VFO_A|RIG_VFO_B|RIG_VFO_TX)

#define FLRIG_MODES (RIG_MODE_AM | RIG_MODE_CW | RIG_MODE_RTTY | \
                     RIG_MODE_SSB | RIG_MODE_FM)

#define RIG_DEBUG_TRACE RIG_DEBUG_VERBOSE

static int flrig_init(RIG *rig);
//int flrig_cleanup(RIG *rig);
static int flrig_set_freq(RIG *rig, vfo_t vfo, freq_t freq);
static int flrig_get_freq(RIG *rig, vfo_t vfo, freq_t *freq);
static int flrig_set_ptt(RIG *rig, vfo_t vfo, ptt_t ptt);
static int flrig_set_mode(RIG *rig, vfo_t vfo, rmode_t mode,
                          pbwidth_t width);
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

struct flrig_priv_data {
    vfo_t vfo_curr;
};

const struct rig_caps flrig_caps = {
    .rig_model = RIG_MODEL_FLRIG,
    .model_name = "FLRig",
    .mfg_name = "FLRig",
    .version = BACKEND_VER,
    .copyright = "LGPL",
    .status = RIG_STATUS_ALPHA,
    .rig_type = RIG_TYPE_TRANSCEIVER,
    .targetable_vfo = 0,
    .ptt_type = RIG_PTT_RIG,
    .port_type = RIG_PORT_NETWORK,
    .write_delay = 0,
    .post_write_delay = 50,
    .timeout = 1000,
    .retry = 3,

    .has_get_func = RIG_FUNC_NONE,
    .has_set_func = RIG_FUNC_NONE,
    .has_get_level = (RIG_LEVEL_RAWSTR | RIG_LEVEL_STRENGTH),
    .has_set_level = RIG_LEVEL_NONE,
    .has_get_parm = RIG_PARM_NONE,
    .has_set_parm = RIG_PARM_NONE,
//  .level_gran =          { [LVL_CWPITCH] = { .step = { .i = 10 } } },
//  .ctcss_list =          common_ctcss_list,
//  .dcs_list =            full_dcs_list,
//  .chan_list =   {
//                        {   0,  18, RIG_MTYPE_MEM, DUMMY_MEM_CAP },
//                        {  19,  19, RIG_MTYPE_CALL },
//                        {  20,  NB_CHAN-1, RIG_MTYPE_EDGE },
//                        RIG_CHAN_END,
//                 },
//  .vfo_ops =     DUMMY_VFO_OP,
    .transceive = RIG_TRN_RIG,
//  .attenuator =     { 10, 20, 30, RIG_DBLST_END, },
//  .preamp =              { 10, RIG_DBLST_END, },
//    .rx_range_list1 = {{.start = kHz(150),.end = MHz(1500),.modes = FLRIG_MODES,
//                        .low_power = -1,.high_power = -1, FLRIG_VFOS, RIG_ANT_1 | RIG_ANT_2},
//                       RIG_FRNG_END,},
//    .tx_range_list1 = {RIG_FRNG_END,},
//    .rx_range_list2 = {{.start = kHz(150),.end = MHz(1500),.modes = FLRIG_MODES,
//                        .low_power = -1,.high_power = -1, FLRIG_VFOS, RIG_ANT_1 | RIG_ANT_2},
//                       RIG_FRNG_END,},
//    .tx_range_list2 = {RIG_FRNG_END,},
//  .tuning_steps =  { {DUMMY_MODES,1}, {DUMMY_MODES,RIG_TS_ANY}, RIG_TS_END, },
//    .filters = {
//                {RIG_MODE_SSB | RIG_MODE_CW | RIG_MODE_RTTY, kHz(2.4)},
//                {RIG_MODE_CW, Hz(500)},
//                {RIG_MODE_AM, kHz(8)},
//                {RIG_MODE_AM, kHz(2.4)},
//                {RIG_MODE_FM, kHz(15)},
//                {RIG_MODE_FM, kHz(8)},
//                {RIG_MODE_WFM, kHz(230)},
//                RIG_FLT_END,
//                },
//    .max_rit = 9990,
//    .max_xit = 9990,
//    .max_ifshift = 10000,
    .priv = NULL,               /* priv */

//  .extlevels =    dummy_ext_levels,
//  .extparms =     dummy_ext_parms,
//  .cfgparams =    dummy_cfg_params,

    .rig_init = flrig_init,
//  .rig_cleanup =  dummy_cleanup,
//  .rig_open =     dummy_open,
//  .rig_close =    dummy_close,

//  .set_conf =     dummy_set_conf,
//  .get_conf =     dummy_get_conf,

    .set_freq = flrig_set_freq,
    .get_freq = flrig_get_freq,
    .set_mode = flrig_set_mode,
//  .get_mode =     dummy_get_mode,
    .set_vfo = flrig_set_vfo,
    .get_vfo = flrig_get_vfo,

//  .set_powerstat =  dummy_set_powerstat,
//  .get_powerstat =  dummy_get_powerstat,
//  .set_level =     dummy_set_level,
//  .get_level =     dummy_get_level,
//  .set_func =      dummy_set_func,
//  .get_func =      dummy_get_func,
//  .set_parm =      dummy_set_parm,
//  .get_parm =      dummy_get_parm,
//  .set_ext_level = dummy_set_ext_level,
//  .get_ext_level = dummy_get_ext_level,
//  .set_ext_parm =  dummy_set_ext_parm,
//  .get_ext_parm =  dummy_get_ext_parm,

//  .get_info =      dummy_get_info,

    .set_ptt = flrig_set_ptt,
    .get_ptt = flrig_get_ptt,
    .set_split_freq = flrig_set_split_freq,
    .get_split_freq = flrig_get_split_freq,
//    .set_split_mode = flrig_set_split_mode,
//    .get_split_mode = flrig_get_split_mode,
    .set_split_vfo = flrig_set_split_vfo,
    .get_split_vfo = flrig_get_split_vfo,
//  .set_ant =    dummy_set_ant,
//  .get_ant =    dummy_get_ant,
//  .set_bank =   dummy_set_bank,
//  .set_mem =    dummy_set_mem,
//  .get_mem =    dummy_get_mem,
//  .vfo_op =     dummy_vfo_op,
//  .set_trn =    dummy_set_trn,
//  .get_trn =    dummy_get_trn,
};

DECLARE_INITRIG_BACKEND(flrig)
{

    rig_debug(RIG_DEBUG_TRACE, "flrig: _init called\n");

    rig_register(&flrig_caps);

    return RIG_OK;
}

static int check_vfo(vfo_t vfo)
{
    switch (vfo) {              // Omni VII only has A & B
    case RIG_VFO_A:
        break;

    case RIG_VFO_B:
        break;

    case RIG_VFO_TX:
        break;

    case RIG_VFO_CURR:
        break;                  // will default to A in which_vfo

    default:
        return FALSE;
    }

    return TRUE;
}

static int vfo_curr(RIG *rig, vfo_t vfo)
{
    int retval = 0;
    struct flrig_priv_data *priv =
        (struct flrig_priv_data *) rig->state.priv;
    retval = (vfo == priv->vfo_curr);
    return retval;
}

// Rather than use some huge XML library we only need a few things
// So we'll hand craft them
static char *xml_build(char *cmd, char *value)
{
    char xml[4096];
    char tmp[32];
    static char xmlpost[4096];
    // Standard 50ms sleep borrowed from ts200.c settings
    // Tested with ANAN 100
    usleep(50 * 1000);
    sprintf(xmlpost,
            "POST /RPC2 HTTP/1.1\n" "User-Agent: XMLRPC++ 0.8\n"
            "Host: 127.0.0.1:12345\n" "Content-type: text/xml\n");
    sprintf(xml, "<?xml version=\"1.0\"?>\n");
    strcat(xml, "<methodCall><methodName>");
    strcat(xml, cmd);
    strcat(xml, "</methodName>");

    if (value && strlen(value) > 0) {
        strcat(xml, value);
    }

    strcat(xml, "</methodCall>\n");
    strcat(xmlpost, "Content-length: ");
    sprintf(tmp, "%ld\n\n", strlen(xml));
    strcat(xmlpost, tmp);
    strcat(xmlpost, xml);
    rig_debug(RIG_DEBUG_VERBOSE, "XML:\n%s", xmlpost);
    return xmlpost;
}

// This is a very crude xml parse specific to what we need from FLRig
// This will not handle array returns for example yet...only simple values
// It simply grabs the first element before the first closing tag
// This works for strings, doubles, and I4-type values
char *xml_parse2(char *xml, char *value, int valueLen)
{
    char *pstart = strchr(xml, '<');

    while (pstart[0] == '<' && pstart[1] != '/') {
        char *p2 = strchr(pstart, '>') + 1;
        pstart = strchr(p2, '<');
        strncpy(value, p2, pstart - p2);
        value[pstart - p2] = 0;
    }

    return value;
}

static char *xml_parse(char *xml, char *value, int value_len)
{
    /* first off we should have an OK on the 1st line */
    if (strstr(xml, " 200 OK") == NULL) {
        return NULL;
    }

    // find the xml skipping the other stuff above it
    char *pxml = strstr(xml, "<?xml");

    if (pxml == NULL) {
        return NULL;
    }

    char *next = strchr(pxml + 1, '<');
    char value2[16384];
    xml_parse2(next, value2, sizeof(value2));
    strcpy(value, value2);
    return value;
}

static int read_transaction(RIG *rig, char *xml, int xml_len)
{

    struct rig_state *rs = &rig->state;
    int retval;
    char cmd_buf[16384];        // plenty big for expected flrig responses
    char delim[1];
    delim[0] = 0x0a;
    xml[0] = cmd_buf[0] = 0;

    do {
        retval =
            read_string(&rs->rigport, cmd_buf, sizeof(cmd_buf), delim,
                        sizeof(delim));

        if (strlen(xml) > 8192 - retval) {
            return -RIG_EINVAL;
        }

        if (retval > 0) {
            strcat(xml, cmd_buf);
        }
    } while (retval > 0 && strstr(cmd_buf, "</methodResponse>") == NULL);

    return RIG_OK;
}

int flrig_init(RIG *rig)
{
    rig_debug(RIG_DEBUG_TRACE, "%s\n", __FUNCTION__);

    struct flrig_priv_data *priv = NULL;
    memset(priv, 0, sizeof(struct flrig_priv_data));
    /*
     * set arbitrary initial status
     */
    priv->vfo_curr = RIG_VFO_A;
    rig->state.priv = (rig_ptr_t) priv;

    if (!rig || !rig->caps) {
        return -RIG_EINVAL;
    }

    strncpy(rig->state.rigport.pathname, DEFAULTPATH,
            sizeof(rig->state.rigport.pathname));
    return RIG_OK;
}

/*
 * flrig_get_freq
 * Assumes rig!=NULL, rig->state.priv!=NULL, freq!=NULL
 */
int flrig_get_freq(RIG *rig, vfo_t vfo, freq_t *freq)
{
    char value[MAXCMDLEN];
    char *pxml;
    int retval;
    struct rig_state *rs;
    char xml[8192];

    rig_debug(RIG_DEBUG_TRACE, "%s: vfo=%s\n", __FUNCTION__,
              rig_strvfo(vfo));
    rs = &rig->state;

    if (check_vfo(vfo) == FALSE) {
        rig_debug(RIG_DEBUG_ERR, "%s: unsupported VFO %s\n",
                  __FUNCTION__, rig_strvfo(vfo));
        return -RIG_EINVAL;
    }

    if (vfo == RIG_VFO_CURR) {
        vfo = RIG_VFO_A;
    }

    flrig_set_vfo(rig, vfo);

    pxml = xml_build("rig.get_vfo", NULL);

    retval = write_block(&rs->rigport, pxml, strlen(pxml));

    if (retval < 0) {
        return retval;
    }

    read_transaction(rig, xml, sizeof(xml));
    xml_parse(xml, value, sizeof(value));
    *freq = atof(value);
    rig_debug(RIG_DEBUG_VERBOSE, "%s: '%s'\n", __FUNCTION__, value);

    return RIG_OK;
}

/*
 * flrig_set_freq
 * assumes rig!=NULL, rig->state.priv!=NULL
 */
int flrig_set_freq(RIG *rig, vfo_t vfo, freq_t freq)
{
    char value[MAXCMDLEN];
    char *pxml;
    int retval;
    struct rig_state *rs;
    char xml[8192];

    rig_debug(RIG_DEBUG_TRACE, "%s: vfo=%s freq=%.1f\n", __FUNCTION__,
              rig_strvfo(vfo), freq);
    rs = &rig->state;

    if (check_vfo(vfo) == FALSE) {
        rig_debug(RIG_DEBUG_ERR, "%s: unsupported VFO %s\n",
                  __FUNCTION__, rig_strvfo(vfo));
        return -RIG_EINVAL;
    }

    if (vfo == RIG_VFO_CURR) {
        if ((retval = flrig_get_vfo(rig, &vfo)) != RIG_OK) {
            return retval;
        }

        rig_debug(RIG_DEBUG_VERBOSE, "%s: set_freq2 vfo=%s\n",
                  __FUNCTION__, rig_strvfo(vfo));
    }

    if (!vfo_curr(rig, vfo)) {
        flrig_set_vfo(rig, vfo);
    }

    sprintf(value,
            "<params><param><value><double>%.6f</double></value></param></params>",
            freq);
    pxml = xml_build("rig.set_vfo", value);
    retval = write_block(&rs->rigport, pxml, strlen(pxml));

    if (retval < 0) {
        return retval;
    }

    read_transaction(rig, xml, sizeof(xml));
    //don't care about the response right now
    //xml_parse(xml, value, sizeof(value));
    flrig_set_vfo(rig, vfo);

    return RIG_OK;
}

/*
 * flrig_set_ptt
 * Assumes rig!=NULL
 */
int flrig_set_ptt(RIG *rig, vfo_t vfo, ptt_t ptt)
{
    int retval;
    char cmd_buf[MAXCMDLEN];
    char *pxml;
    char xml[8192];
    struct rig_state *rs;

    rig_debug(RIG_DEBUG_TRACE, "%s: ptt=%d\n", __FUNCTION__, ptt);
    rs = &rig->state;

    if (check_vfo(vfo) == FALSE) {
        rig_debug(RIG_DEBUG_ERR, "%s: unsupported VFO %s\n",
                  __FUNCTION__, rig_strvfo(vfo));
        return -RIG_EINVAL;
    }

    if (vfo == RIG_VFO_CURR) {
        vfo = RIG_VFO_A;
    }

    if (!vfo_curr(rig, vfo)) {
        flrig_set_vfo(rig, vfo);
    }

    sprintf(cmd_buf,
            "<params><param><value><i4>%d</i4></value></param></params>",
            ptt);
    pxml = xml_build("rig.set_ptt", cmd_buf);
    retval = write_block(&rs->rigport, pxml, strlen(pxml));

    if (retval < 0) {
        return retval;
    }

    read_transaction(rig, xml, sizeof(xml));
    xml_parse(xml, cmd_buf, sizeof(cmd_buf));
    return RIG_OK;
}

int flrig_get_ptt(RIG *rig, vfo_t vfo, ptt_t *ptt)
{
    char value[MAXCMDLEN];
    char *pxml;
    int retval;
    struct rig_state *rs;
    char xml[8192];

    rig_debug(RIG_DEBUG_TRACE, "%s: vfo=%s\n", __FUNCTION__,
              rig_strvfo(vfo));
    rs = &rig->state;

    pxml = xml_build("rig.get_ptt", NULL);

    retval = write_block(&rs->rigport, pxml, strlen(pxml));

    if (retval < 0) {
        return retval;
    }

    read_transaction(rig, xml, sizeof(xml));
    xml_parse(xml, value, sizeof(value));
    *ptt = atoi(value);
    rig_debug(RIG_DEBUG_VERBOSE, "%s: '%s'\n", __FUNCTION__, value);

    return RIG_OK;
}

/*
 * flrig_set_mode
 * Assumes rig!=NULL
 */
int flrig_set_mode(RIG *rig, vfo_t vfo, rmode_t mode, pbwidth_t width)
{
    char cmd_buf[32], ttmode;
    int cmd_len, retval;
    struct rig_state *rs;

    //struct tt588_priv_data *priv = (struct tt588_priv_data *) rig->state.priv;

    rig_debug(RIG_DEBUG_TRACE, "%s: vfo=%s mode=%d width=%d\n",
              __FUNCTION__, rig_strvfo(vfo), mode, width);
    rs = &rig->state;

    if (check_vfo(vfo) == FALSE) {
        rig_debug(RIG_DEBUG_ERR, "%s: unsupported VFO %s\n",
                  __FUNCTION__, rig_strvfo(vfo));
        return -RIG_EINVAL;
    }

    switch (mode) {
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

    //case RIG_MODE_FSK:      ttmode = 'F'; break;
    default:
        rig_debug(RIG_DEBUG_ERR, "%s: unsupported mode %d\n",
                  __FUNCTION__, mode);
        return -RIG_EINVAL;
    }

    cmd_len = sprintf((char *) cmd_buf, "XB%c" EOM, ttmode);

    retval = write_block(&rs->rigport, cmd_buf, cmd_len);

    if (retval < 0) {
        return retval;
    }

    return RIG_OK;

}

#if 0
static int flrig_flush(RIG *rig, vfo_t vfo)
{
    char value[MAXCMDLEN];
    char *pxml;
    int retval;
    struct rig_state *rs;
    char xml[8192];

    rig_debug(RIG_DEBUG_TRACE, "%s: vfo=%s\n", __FUNCTION__,
              rig_strvfo(vfo));
    rs = &rig->state;

    sprintf(value, "<params><param><value>%s</value></param></params>",
            vfo == RIG_VFO_A ? "A" : "B");
    pxml = xml_build("rig.flush", value);
    network_flush(&rs->rigport);
    retval = write_block(&rs->rigport, pxml, strlen(pxml));

    if (retval < 0) {
        return retval;
    }

    read_transaction(rig, xml, sizeof(xml));
    return RIG_OK;
}
#endif

int flrig_set_vfo(RIG *rig, vfo_t vfo)
{
    char value[MAXCMDLEN];
    char *pxml;
    int retval;
    struct rig_state *rs;
    char xml[8192];

    rig_debug(RIG_DEBUG_TRACE, "%s: vfo=%s\n", __FUNCTION__,
              rig_strvfo(vfo));
    rs = &rig->state;

    if (check_vfo(vfo) == FALSE) {
        rig_debug(RIG_DEBUG_ERR, "%s: unsupported VFO %s\n",
                  __FUNCTION__, rig_strvfo(vfo));
        return -RIG_EINVAL;
    }

    if (vfo == RIG_VFO_CURR) {
        struct flrig_priv_data *priv =
            (struct flrig_priv_data *) rig->state.priv;
        vfo = priv->vfo_curr;
    }

    sprintf(value, "<params><param><value>%s</value></param></params>",
            vfo == RIG_VFO_A ? "A" : "B");
    pxml = xml_build("rig.set_AB", value);
    retval = write_block(&rs->rigport, pxml, strlen(pxml));

    if (retval < 0) {
        return retval;
    }

    read_transaction(rig, xml, sizeof(xml));
    //xml_parse(xml,value,sizeof(value));
    vfo_t vfotmp;
    flrig_get_vfo(rig, &vfotmp);

    return RIG_OK;

}

int flrig_get_vfo(RIG *rig, vfo_t *vfo)
{
    char value[MAXCMDLEN];
    char *pxml;
    int retval;
    struct rig_state *rs;
    char xml[8192];

    rig_debug(RIG_DEBUG_TRACE, "%s\n", __FUNCTION__);
    rs = &rig->state;

    pxml = xml_build("rig.get_AB", NULL);
    retval = write_block(&rs->rigport, pxml, strlen(pxml));

    if (retval < 0) {
        return retval;
    }

    read_transaction(rig, xml, sizeof(xml));
    xml_parse(xml, value, sizeof(value));

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

    struct flrig_priv_data *priv =
        (struct flrig_priv_data *) rig->state.priv;

    priv->vfo_curr = *vfo;

    rig_debug(RIG_DEBUG_VERBOSE, "%s: vfo=%s\n", __FUNCTION__,
              rig_strvfo(*vfo));

    return RIG_OK;
}

/*
 * flrig_set_split_freq
 * Note that split doesn't work for FLRig models that don't have reply codes
 * Like most Yaesu rigs.  The commands will overrun FLRig in those cases.
 * Rigs that do have replies for all cat commands will work with split
 */
int flrig_set_split_freq(RIG *rig, vfo_t vfo, freq_t tx_freq)
{
    char value[MAXCMDLEN];
    char *pxml;
    int retval;
    struct rig_state *rs;
    char xml[8192];

    rig_debug(RIG_DEBUG_TRACE, "%s: vfo=%s freq=%.1f\n", __FUNCTION__,
              rig_strvfo(vfo), tx_freq);
    rs = &rig->state;

    if (vfo == RIG_VFO_SUB) {
        vfo = RIG_VFO_B;
    }

    if (check_vfo(vfo) == FALSE) {
        rig_debug(RIG_DEBUG_ERR, "%s: unsupported VFO %s\n",
                  __FUNCTION__, rig_strvfo(vfo));
        return -RIG_EINVAL;
    }

    if (!vfo_curr(rig, vfo)) {
        flrig_set_vfo(rig, vfo);
    }

    sprintf(value,
            "<params><param><value><double>%.6f</double></value></param></params>",
            tx_freq);
    pxml = xml_build("rig.set_vfo", value);
    retval = write_block(&rs->rigport, pxml, strlen(pxml));

    if (retval < 0) {
        return retval;
    }

    read_transaction(rig, xml, sizeof(xml));
    xml_parse(xml, value, sizeof(value));

    return RIG_OK;
}

/*
 * flrig_get_split_freq
 * assumes rig!=NULL, tx_freq!=NULL
 */
int flrig_get_split_freq(RIG *rig, vfo_t vfo, freq_t *tx_freq)
{
    rig_debug(RIG_DEBUG_TRACE, "%s: vfo=%s\n", __FUNCTION__,
              rig_strvfo(vfo));
    return flrig_get_freq(rig, vfo, tx_freq);
}

/*
 * flrig_set_split_vfo
 * assumes rig!=NULL, tx_freq!=NULL
 */
int flrig_set_split_vfo(RIG *rig, vfo_t vfo, split_t split, vfo_t tx_vfo)
{
    char value[MAXCMDLEN];
    char *pxml;
    int retval;
    struct rig_state *rs;
    char xml[8192];

    rig_debug(RIG_DEBUG_TRACE, "%s: tx_vfo=%s\n", __FUNCTION__,
              rig_strvfo(tx_vfo));
    rs = &rig->state;

    if (tx_vfo == RIG_VFO_SUB) {
        tx_vfo = RIG_VFO_B;
    }

    if (!vfo_curr(rig, vfo)) {
        flrig_set_vfo(rig, vfo);
    }

    sprintf(value,
            "<params><param><value><i4>%d</i4></value></param></params>",
            split);
    pxml = xml_build("rig.set_split", value);
    retval = write_block(&rs->rigport, pxml, strlen(pxml));

    if (retval < 0) {
        return retval;
    }

    read_transaction(rig, xml, sizeof(xml));
    //xml_parse(xml, value, sizeof(value));

    //flrig_set_vfo(rig,RIG_VFO_A);

    return RIG_OK;
}

/*
 * flrig_get_split_vfo
 * assumes rig!=NULL, tx_freq!=NULL
 */
int flrig_get_split_vfo(RIG *rig, vfo_t vfo, split_t *split,
                        vfo_t *tx_vfo)
{
    char value[MAXCMDLEN];
    char *pxml;
    int retval;
    struct rig_state *rs;
    char xml[8192];

    rig_debug(RIG_DEBUG_TRACE, "%s\n", __FUNCTION__);
    rs = &rig->state;
    //flrig_set_vfo(rig,RIG_VFO_B);

    pxml = xml_build("rig.get_split", NULL);
    retval = write_block(&rs->rigport, pxml, strlen(pxml));

    if (retval < 0) {
        return retval;
    }

    read_transaction(rig, xml, sizeof(xml));
    xml_parse(xml, value, sizeof(value));

    //flrig_set_vfo(rig,RIG_VFO_A);

    *tx_vfo = RIG_VFO_B;
    *split = atoi(value);

    return RIG_OK;
}

/*
  .set_split_freq =     flrig_set_split_freq,
  .get_split_freq =     flrig_get_split_freq,
  .set_split_mode =     flrig_set_split_mode,
  .get_split_mode =     flrig_get_split_mode,
*/
