/*
*  Hamlib rigctld backend - works with GQRX's remote control command set
*  Based upon SDRSharp backend, Copyright (c) 2023 by Michael Black W9MDB
*  Copyright (c) 2025 Mark J. Fine
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
#include <math.h>
#include <stdbool.h>

#include <hamlib/rig.h>
#include <serial.h>
#include <misc.h>
#include <cal.h>
#include "idx_builtin.h"

#define BACKEND_VER "20250718.2"

#define TRUE 1
#define FALSE 0


#define MAXCMDLEN 128
#define MAXXMLLEN 128
#define MAXARGLEN 128
#define MAXBANDWIDTHLEN 4096

#define DEFAULTPATH "127.0.0.1:7356"

#define GQRX_MODES (RIG_MODE_AM | RIG_MODE_AMS | RIG_MODE_SSB | RIG_MODE_CW | RIG_MODE_CWR | RIG_MODE_FM | RIG_MODE_WFM | RIG_MODE_WFMS)
#define GQRX_LEVEL_ALL (RIG_LEVEL_AF | RIG_LEVEL_SQL | RIG_LEVEL_RAWSTR | RIG_LEVEL_STRENGTH )
#define GQRX_ANTS (RIG_ANT_1)
#define GQRX_VFOS (RIG_VFO_A)

#define GQRX_STR_CAL { 17, { \
        {-100, -60 }, \
        { -92, -54 }, \
        { -84, -48 }, \
        { -78, -42 }, \
        { -72, -36 }, \
        { -66, -30 }, \
        { -60, -24 }, \
        { -54, -18 }, \
        { -48, -12 }, \
        { -42, -6 }, \
        { -36,  0 }, \
        { -30, 10 }, \
        { -24, 20 }, \
        { -18, 30 }, \
        { -12, 40 }, \
        {  -6, 50 }, \
        {   0, 60 }, \
    } }

struct gqrx_priv_data
{
    freq_t	curr_freq;
    rmode_t	curr_mode;
    pbwidth_t	curr_width;
    int		curr_power;
    int		curr_meter;
    float       curr_af;
    float       curr_sql;
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
    char *terminator = "\n";

    ENTERFUNC;

    retry = 2;
    delims = "\n";
    xml[0] = 0;

    do
    {
        char tmp_buf[MAXXMLLEN];        // plenty big for expected responses hopefully

        if (retry < 2)
        {
            rig_debug(RIG_DEBUG_WARN, "%s: retry needed? retry=%d\n", __func__, retry);
        }

        int len = read_string(RIGPORT(rig), (unsigned char *) tmp_buf, sizeof(tmp_buf),
                              delims,
                              strlen(delims), 0, 1);

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


static int gqrx_transaction(RIG *rig, char *cmd, char *value,
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
            if (retval == -RIG_EIO)
            {
                set_transaction_inactive(rig); RETURNFUNC(retval);
            }

            hl_usleep(50 * 1000); // 50ms sleep if error
        }

        if (value)
        {
            read_transaction(rig, xml, sizeof(xml));    // this might time out -- that's OK
            strncpy(value, xml, value_len);
        }

    }
    while (((value && strlen(value) == 0))
            && retry--); // we'll do retries if needed

    if (value && strlen(value) == 0)
    {
        rig_debug(RIG_DEBUG_ERR, "%s: no value returned\n", __func__);
        
        set_transaction_inactive(rig);
        
        RETURNFUNC(-RIG_EPROTO);
    }

    ELAPSED2;
    set_transaction_inactive(rig);
    RETURNFUNC(RIG_OK);
}


/*
* gqrx_init
* Assumes rig!=NULL
*/
static int gqrx_init(RIG *rig)
{
    struct gqrx_priv_data *priv;
    hamlib_port_t *rp = RIGPORT(rig);

    ENTERFUNC;
    rig_debug(RIG_DEBUG_TRACE, "%s version %s\n", __func__, rig->caps->version);

    STATE(rig)->priv  = (struct gqrx_priv_data *)calloc(1, sizeof(struct gqrx_priv_data));

    if (!STATE(rig)->priv)
    {
        RETURNFUNC(-RIG_ENOMEM);
    }

    priv = STATE(rig)->priv;

    memset(priv, 0, sizeof(struct gqrx_priv_data));

    /*
     * set arbitrary initial status
     */
    STATE(rig)->current_vfo = RIG_VFO_A;
    priv->curr_freq = 0.0;
    priv->curr_mode = RIG_MODE_NONE;
    priv->curr_width = RIG_PASSBAND_NORMAL;
    priv->curr_power = false;  //assume off
    priv->curr_meter = -100;   //assume no signal
    priv->curr_af = -10.0;     //assume mid-way
    priv->curr_sql = -150.0;   //assume none

    if (!rig->caps)
    {
        RETURNFUNC(-RIG_EINVAL);
    }

    strncpy(rp->pathname, DEFAULTPATH, sizeof(rp->pathname));

    rig_debug(RIG_DEBUG_TRACE, "%s pathanme %s\n", __func__, rp->pathname);

    RETURNFUNC(RIG_OK);
}


/*
* gqrx_get_freq
* Assumes rig!=NULL, STATE(rig)->priv!=NULL, freq!=NULL
*/
static int gqrx_get_freq(RIG *rig, vfo_t vfo, freq_t *freq)
{
    char value[MAXARGLEN];
    struct gqrx_priv_data *priv = (struct gqrx_priv_data *) STATE(rig)->priv;

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
        vfo = STATE(rig)->current_vfo;
        rig_debug(RIG_DEBUG_TRACE, "%s: get_freq vfo=%s\n",
                  __func__, rig_strvfo(vfo));
    }

    char *cmd = "f\n";
    int retval;

    retval = gqrx_transaction(rig, cmd, value, sizeof(value));

    if (retval != RIG_OK)
    {
        rig_debug(RIG_DEBUG_ERR, "%s: READBMF failed retval=%s\n", __func__,
                  rigerror(retval));
        RETURNFUNC(retval);
    }

    *freq = 0;

    sscanf(value, "%lf", freq);

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
        priv->curr_freq = *freq;
    }

    RETURNFUNC(RIG_OK);
}


/*
* gqrx_open
* Assumes rig!=NULL
*/
static int gqrx_open(RIG *rig)
{
    int retval;
    char value[MAXARGLEN];

    ENTERFUNC;
    value[0] = '?';
    value[1] = 0;

    freq_t freq;
    retval = gqrx_get_freq(rig, RIG_VFO_CURR, &freq);

    if (retval != RIG_OK)
    {
        rig_debug(RIG_DEBUG_ERR, "%s: gqrx_get_freq not working!!\n", __func__);
        RETURNFUNC(-RIG_EPROTO);
    }

    STATE(rig)->current_vfo = RIG_VFO_A;
    rig_debug(RIG_DEBUG_TRACE, "%s: currvfo=%s value=%s\n", __func__,
              rig_strvfo(STATE(rig)->current_vfo), value);

    RETURNFUNC(retval);
}


/*
* gqrx_close
* Assumes rig!=NULL
*/
static int gqrx_close(RIG *rig)
{
    ENTERFUNC;

    RETURNFUNC(RIG_OK);
}


/*
* gqrx_cleanup
* Assumes rig!=NULL, STATE(rig)->priv!=NULL
*/
static int gqrx_cleanup(RIG *rig)
{
    ENTERFUNC;
    rig_debug(RIG_DEBUG_TRACE, "%s\n", __func__);

    if (!rig)
    {
        RETURNFUNC2(-RIG_EINVAL);
    }

    free(STATE(rig)->priv);

    STATE(rig)->priv = NULL;

    // we really don't need to free this up as it's only done once
    // was causing problem when cleanup was followed by rig_open
    // model_gqrx was not getting refilled
    // if we can figure out that one we can re-enable this
#if 0
    int i;

    for (i = 0; modeMap[i].mode_hamlib != 0; ++i)
    {
        if (modeMap[i].mode_gqrx)
        {
            free(modeMap[i].mode_gqrx);
            modeMap[i].mode_gqrx = NULL;
            modeMap[i].mode_hamlib = 0;
        }

    }

#endif

    RETURNFUNC2(RIG_OK);
}


/*
* gqrx_get_vfo
* assumes rig!=NULL, vfo != NULL
*/
static int gqrx_get_vfo(RIG *rig, vfo_t *vfo)
{
    ENTERFUNC;

    *vfo = RIG_VFO_A;

    RETURNFUNC(RIG_OK);
}


/*
* gqrx_set_freq
* assumes rig!=NULL, STATE(rig)->priv!=NULL
*/
static int gqrx_set_freq(RIG *rig, vfo_t vfo, freq_t freq)
{
    int retval;
    char cmd[MAXARGLEN];
    char value[1024];

    ENTERFUNC;
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
        vfo = STATE(rig)->current_vfo;
        rig_debug(RIG_DEBUG_TRACE, "%s: set_freq vfo=%s\n",
                  __func__, rig_strvfo(vfo));
    }

    SNPRINTF(cmd, sizeof(cmd), "F %lf\n", freq);

    retval = gqrx_transaction(rig, cmd, value, sizeof(value));

    if (retval != RIG_OK)
    {
        RETURNFUNC2(retval);
    }

    sscanf(value, "RPRT %d", &retval);

    RETURNFUNC2(retval);
}


/*
* gqrx_set_mode
* assumes rig!=NULL, STATE(rig)->priv!=NULL
*/
static int gqrx_set_mode(RIG *rig, vfo_t vfo, rmode_t mode, pbwidth_t width)
{
    int retval;
    char cmd[MAXARGLEN];
    char value[1024];
    char* mode_sel;
    struct gqrx_priv_data *priv = (struct gqrx_priv_data *) STATE(rig)->priv;

    ENTERFUNC;
    rig_debug(RIG_DEBUG_TRACE, "%s\n", __func__);
    rig_debug(RIG_DEBUG_TRACE, "%s: vfo=%s mode=%llu\n", __func__,
              rig_strvfo(vfo), (long long unsigned int)mode);

    if (check_vfo(vfo) == FALSE)
    {
        rig_debug(RIG_DEBUG_ERR, "%s: unsupported VFO %s\n",
                  __func__, rig_strvfo(vfo));
        RETURNFUNC2(-RIG_EINVAL);
    }

    if (vfo == RIG_VFO_CURR)
    {
        vfo = STATE(rig)->current_vfo;
        rig_debug(RIG_DEBUG_TRACE, "%s: set_mode vfo=%s\n",
                  __func__, rig_strvfo(vfo));
    }

    switch (mode)
    {
    case RIG_MODE_AM:       mode_sel = strdup("AM"); break;

    case RIG_MODE_AMS:      mode_sel = strdup("AMS"); break;

    case RIG_MODE_LSB:      mode_sel = strdup("LSB"); break;

    case RIG_MODE_USB:      mode_sel = strdup("USB"); break;

    case RIG_MODE_CW:       mode_sel = strdup("CWU"); break;

    case RIG_MODE_CWR:      mode_sel = strdup("CWL"); break;

    case RIG_MODE_FM:       mode_sel = strdup("FM"); break;

    case RIG_MODE_WFM:      mode_sel = strdup("WFM"); break;

    case RIG_MODE_WFMS:     mode_sel = strdup("WFM_ST"); break;

    default:
        rig_debug(RIG_DEBUG_ERR, "gqrx_set_mode: "
                  "unsupported mode %s\n", rig_strrmode(mode));
        return -RIG_EINVAL;
    }

    // Only change width if it's different
    // Otherwise, just change the mode and
    // let GQRX change the width on it's own
    if (width != priv->curr_width)
    {
        SNPRINTF(cmd, sizeof(cmd), "M %s %ld\n", mode_sel, width);
    }
    else
    {
        SNPRINTF(cmd, sizeof(cmd), "M %s\n", mode_sel);
    }

    retval = gqrx_transaction(rig, cmd, value, sizeof(value));

    free(mode_sel);
    
    if (retval != RIG_OK)
    {
        RETURNFUNC2(retval);
    }

    sscanf(value, "RPRT %d", &retval);

    RETURNFUNC2(retval);
}


/*
 * gqrx_get_mode
 * Assumes rig!=NULL
 */
static int gqrx_get_mode(RIG *rig, vfo_t vfo, rmode_t *mode, pbwidth_t *width)
{
    int retval;
    char value[MAXARGLEN];
    char modeStr[MAXARGLEN];
    int pbwidth;
    struct gqrx_priv_data *priv = (struct gqrx_priv_data *) STATE(rig)->priv;

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
        vfo = STATE(rig)->current_vfo;
        rig_debug(RIG_DEBUG_TRACE, "%s: get_mode vfo=%s\n",
                  __func__, rig_strvfo(vfo));
    }

    char *cmd = "m\n";

    retval = gqrx_transaction(rig, cmd, value, sizeof(value));

    if (retval != RIG_OK)
    {
        rig_debug(RIG_DEBUG_ERR, "%s: READBMF failed retval=%s\n", __func__,
                  rigerror(retval));
        RETURNFUNC(retval);
    }

    sscanf(value, "%s\n%d", &modeStr[0], &pbwidth);

    if (strcmp(&modeStr[0], "AM") == 0)
    {
        *mode = RIG_MODE_AM;
    }
    else if (strcmp(&modeStr[0], "AMS") == 0)
    {
        *mode = RIG_MODE_AMS;
    }
    else if (strcmp(&modeStr[0], "LSB") == 0)
    {
        *mode = RIG_MODE_LSB;
    }
    else if (strcmp(&modeStr[0], "USB") == 0)
    {
        *mode = RIG_MODE_USB;
    }
    else if (strcmp(&modeStr[0], "CWU") == 0)
    {
        *mode = RIG_MODE_CW;
    }
    else if (strcmp(&modeStr[0], "CWL") == 0)
    {
        *mode = RIG_MODE_CWR;
    }
    else if (strcmp(&modeStr[0], "FM") == 0)
    {
        *mode = RIG_MODE_FM;
    }
    else if (strcmp(&modeStr[0], "WFM") == 0)
    {
        *mode = RIG_MODE_WFM;
    }
    else if (strcmp(&modeStr[0], "WFM_ST") == 0)
    {
        *mode = RIG_MODE_WFMS;
    }
    else
    {
        rig_debug(RIG_DEBUG_ERR, "%s: value=%s\n", __func__,
                  value);
        RETURNFUNC(-RIG_EPROTO);
    }
    
    rig_debug(RIG_DEBUG_TRACE, "%s: mode=%s\n", __func__, modeStr);

    if (vfo == RIG_VFO_A)
    {
        *width = pbwidth;
        priv->curr_mode = *mode;
        priv->curr_width = pbwidth;
    }

    RETURNFUNC(RIG_OK);
}


/*
* gqrx_set_level
* assumes rig!=NULL, STATE(rig)->priv!=NULL
*/
static int gqrx_set_level(RIG *rig, vfo_t vfo, setting_t level, value_t val)
{
    int retval;
    char cmd[MAXARGLEN];
    char value[1024];

    ENTERFUNC;
    rig_debug(RIG_DEBUG_TRACE, "%s\n", __func__);
    rig_debug(RIG_DEBUG_TRACE, "%s: vfo=%s level=%llu\n", __func__,
              rig_strvfo(vfo), (long long unsigned int)level);

    if (check_vfo(vfo) == FALSE)
    {
        rig_debug(RIG_DEBUG_ERR, "%s: unsupported VFO %s\n",
                  __func__, rig_strvfo(vfo));
        RETURNFUNC2(-RIG_EINVAL);
    }

    if (vfo == RIG_VFO_CURR)
    {
        vfo = STATE(rig)->current_vfo;
        rig_debug(RIG_DEBUG_TRACE, "%s: set_level vfo=%s\n",
                  __func__, rig_strvfo(vfo));
    }

    switch (level)
    {
    case RIG_LEVEL_AF:
        SNPRINTF(cmd, sizeof(cmd), "L AF %f\n", val.f);
        break;

    case RIG_LEVEL_SQL:
        SNPRINTF(cmd, sizeof(cmd), "L SQL %f\n", val.f);
        break;

    default:
        return -RIG_EINVAL;
    }

    retval = gqrx_transaction(rig, cmd, value, sizeof(value));

    if (retval != RIG_OK)
    {
        RETURNFUNC2(retval);
    }

    sscanf(value, "RPRT %d", &retval);

    RETURNFUNC2(retval);
}


/*
 * gqrx_get_level
 * Assumes rig!=NULL
 */
static int gqrx_get_level(RIG *rig, vfo_t vfo, setting_t level, value_t *val)
{
    char* cmd;
    char value[MAXARGLEN];
    int retval;
    struct gqrx_priv_data *priv = (struct gqrx_priv_data *) STATE(rig)->priv;

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
        vfo = STATE(rig)->current_vfo;
        rig_debug(RIG_DEBUG_TRACE, "%s: get_mode vfo=%s\n",
                  __func__, rig_strvfo(vfo));
    }

    switch(level)
    {
    case RIG_LEVEL_AF:
       cmd = strdup("l AF\n");
       break;
       
    case RIG_LEVEL_SQL:
       cmd = strdup("l SQL\n");
       break;

    case RIG_LEVEL_STRENGTH:
    case RIG_LEVEL_RAWSTR:
       cmd = strdup("l STRENGTH\n");
       break;

    default:
        RETURNFUNC(-RIG_EPROTO);
    }
    
    retval = gqrx_transaction(rig, cmd, value, sizeof(value));

    if (retval != RIG_OK)
    {
        rig_debug(RIG_DEBUG_ERR, "%s: READBMF failed retval=%s\n", __func__,
                  rigerror(retval));
        RETURNFUNC(retval);
    }

    sscanf(value, "%f\n", &val->f);

    if (vfo == RIG_VFO_A)
    {
        int tempi;
        switch(level)
        {
        case RIG_LEVEL_AF:
            priv->curr_af = val->f;
            break;
            
        case RIG_LEVEL_SQL:
            priv->curr_sql = val->f;
            break;
            
        case RIG_LEVEL_STRENGTH:
            // Overlapping read/write from one member of a union to another
            //   is technically undefined behavior, according to the C
            //   standard; use a temp to avoid cppcheck error.
            tempi = round(rig_raw2val(round(val->f), &rig->caps->str_cal));
            val->i = tempi;
            priv->curr_meter = val->i;
            break;

        case RIG_LEVEL_RAWSTR:
            tempi = round(val->f);
            val->i = tempi;
            break;
            
        default :
            break;
        }
    }

    RETURNFUNC(RIG_OK);
}


/*
* gqrx_set_powerstat
* assumes rig!=NULL, STATE(rig)->priv!=NULL
*/
static int gqrx_set_powerstat(RIG *rig, powerstat_t status)
{
    int retval;
    char cmd[MAXARGLEN];
    char value[1024];

    rig_debug(RIG_DEBUG_TRACE, "%s\n", __func__);

    SNPRINTF(cmd, sizeof(cmd), "U DSP %d\n", status ? 1 : 0);

    retval = gqrx_transaction(rig, cmd, value, sizeof(value));

    if (retval != RIG_OK)
    {
        RETURNFUNC2(retval);
    }

    sscanf(value, "RPRT %d", &retval);

    RETURNFUNC2(retval);
}


/*
* gqrx_get_powerstat
* Assumes rig!=NULL, STATE(rig)->priv!=NULL, freq!=NULL
*/
static int gqrx_get_powerstat(RIG *rig, powerstat_t *status)
{
    char value[MAXARGLEN];
    struct gqrx_priv_data *priv = (struct gqrx_priv_data *) STATE(rig)->priv;

    ENTERFUNC;

    char *cmd = "u DSP\n";
    int retval;

    retval = gqrx_transaction(rig, cmd, value, sizeof(value));

    if (retval != RIG_OK)
    {
        *status = false;
    }
    else
    {
        *status = (value[0] == '1');
    }
   
    rig_debug(RIG_DEBUG_TRACE, "%s: status=%d\n", __func__, *status);

    priv->curr_power = *status;

    RETURNFUNC(RIG_OK);
}


struct rig_caps gqrx_caps =
{
    RIG_MODEL(RIG_MODEL_GQRX),
    .model_name = "GQRX",
    .mfg_name = "GQRX",
    .version = BACKEND_VER,
    .copyright = "LGPL",
    .status = RIG_STATUS_NEW,
    .rig_type = RIG_TYPE_RECEIVER,
    .ptt_type = RIG_PTT_NONE,
    .dcd_type =  RIG_DCD_NONE,
    .port_type = RIG_PORT_NETWORK,
    .write_delay = 1,
    .post_write_delay = 100,
    .timeout = 1000,
    .retry = 2,

    .has_get_func =  false,
    .has_set_func =  false,
    .has_get_level = GQRX_LEVEL_ALL,
    .has_set_level = RIG_LEVEL_SET(RIG_LEVEL_AF | RIG_LEVEL_SQL),
    .has_get_parm =  false,
    .has_set_parm =  false,
    .level_gran = {
        [LVL_RAWSTR] = { .min = { .f = -100.0 }, .max = { .f = 1.0 } },
        [LVL_AF] = { .min = { .f = -80.0 }, .max = { .f = 50.0 } },
        [LVL_SQL] = { .min = { .f = -150.0 }, .max = { .f = 1.0 } },
    },
    .parm_gran =  {},
    .ctcss_list =  NULL,
    .dcs_list =  NULL,
    .max_rit =  Hz(0),
    .max_xit =  Hz(0),
    .max_ifshift =  Hz(0),
    .targetable_vfo =  0,
    .transceive =  RIG_TRN_OFF,
    .vfo_ops =  0,
    .scan_ops = 0,
    .bank_qty = 0,
    .chan_desc_sz = 0,
    .priv =  NULL,

    .chan_list =  {RIG_CHAN_END,},

    .rx_range_list1 = {
        { Hz(1), GHz(2), GQRX_MODES, -1, -1, GQRX_VFOS, GQRX_ANTS},
        RIG_FRNG_END,
    },
    .tx_range_list1 = {RIG_FRNG_END,},
    .rx_range_list2 = {
        { Hz(1), GHz(2), GQRX_MODES, -1, -1, GQRX_VFOS, GQRX_ANTS},
        RIG_FRNG_END,
    },
    .tx_range_list2 = {RIG_FRNG_END,},
    .tuning_steps =  {
        {GQRX_MODES, 1},
        {GQRX_MODES, RIG_TS_ANY},
        RIG_TS_END,
    },
    //note: wide, normal, narrow, user
    .filters =  {
        {RIG_MODE_WFM | RIG_MODE_WFMS, kHz(160)}, //normal
        {RIG_MODE_WFM | RIG_MODE_WFMS, kHz(200)}, //wide
        {RIG_MODE_WFM | RIG_MODE_WFMS, kHz(120)}, //narrow
        {RIG_MODE_FM, kHz(10)}, //normal
        {RIG_MODE_FM, kHz(20)}, //wide
        {RIG_MODE_FM, kHz(5)},  //narrow
        {RIG_MODE_AM | RIG_MODE_AMS, kHz(10)}, //normal
        {RIG_MODE_AM | RIG_MODE_AMS, kHz(20)}, //wide
        {RIG_MODE_AM | RIG_MODE_AMS, kHz(5)},  //narrow
        {RIG_MODE_SSB, kHz(2.7)}, //normal
        {RIG_MODE_SSB, kHz(3.9)}, //wide
        {RIG_MODE_SSB, kHz(1.4)}, //narrow
        {RIG_MODE_CW | RIG_MODE_CWR, Hz(500)},  //normal
        {RIG_MODE_CW | RIG_MODE_CWR, kHz(2.0)}, //wide
        {RIG_MODE_CW | RIG_MODE_CWR, Hz(200)},  //narrow
        {RIG_MODE_WFM | RIG_MODE_WFMS | RIG_MODE_FM | RIG_MODE_AM | RIG_MODE_AMS | RIG_MODE_SSB | RIG_MODE_CW | RIG_MODE_CWR, RIG_FLT_ANY}, //user
        RIG_FLT_END,
    },
    .str_cal = GQRX_STR_CAL,

    .rig_init = gqrx_init,
    .rig_open = gqrx_open,
    .rig_close = gqrx_close,
    .rig_cleanup = gqrx_cleanup,

    .get_vfo = gqrx_get_vfo, //always RIG_VFO_A
    .set_freq = gqrx_set_freq, //F <frequency Hz>\n
    .get_freq = gqrx_get_freq, //f\n
    .set_mode =  gqrx_set_mode, //M <mode> [passband Hz]\n
    .get_mode =  gqrx_get_mode, //m\n
    .set_level = gqrx_set_level, //AF:L AF <dB>, SQL:L SQL <dBFS>\n
    .get_level = gqrx_get_level, //AF:l AF\n, SWL:l SQL\n, RAWSTR:l STRENGTH\n
    .set_powerstat = gqrx_set_powerstat, //U DSP <status>\n
    .get_powerstat = gqrx_get_powerstat, //u DSP\n
    .hamlib_check_rig_caps = HAMLIB_CHECK_RIG_CAPS
};
