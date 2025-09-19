/*
*  Hamlib rigctld backend - works with SDR#'s gpredict plugin for example
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
#include <math.h>
#include <stdbool.h>

#include "hamlib/rig.h"
#include "iofunc.h"
#include "misc.h"

#define BACKEND_VER "20230127.0"

#define TRUE 1
#define FALSE 0


#define MAXCMDLEN 128
#define MAXXMLLEN 128
#define MAXARGLEN 128
#define MAXBANDWIDTHLEN 4096

#define DEFAULTPATH "127.0.0.1:4532"

#define SDRSHARP_VFOS (RIG_VFO_A)
#define SDRSHARP_ANTS (RIG_ANT_1)
#define SDRSHARP_MODES (RIG_MODE_NONE)

struct sdrsharp_priv_data
{
    freq_t	curr_freq;
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
        char tmp_buf[MAXXMLLEN];        // plenty big for expected sdrsharp responses hopefully

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


static int sdrsharp_transaction(RIG *rig, char *cmd, char *value,
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
* sdrsharp_init
* Assumes rig!=NULL
*/
static int sdrsharp_init(RIG *rig)
{
    struct sdrsharp_priv_data *priv;
    hamlib_port_t *rp = RIGPORT(rig);

    ENTERFUNC;
    rig_debug(RIG_DEBUG_TRACE, "%s version %s\n", __func__, rig->caps->version);

    STATE(rig)->priv  = (struct sdrsharp_priv_data *)calloc(1, sizeof(struct sdrsharp_priv_data));

    if (!STATE(rig)->priv)
    {
        RETURNFUNC(-RIG_ENOMEM);
    }

    priv = STATE(rig)->priv;

    memset(priv, 0, sizeof(struct sdrsharp_priv_data));

    /*
     * set arbitrary initial status
     */
    STATE(rig)->current_vfo = RIG_VFO_A;
    priv->curr_freq = 0.0;

    if (!rig->caps)
    {
        RETURNFUNC(-RIG_EINVAL);
    }

    strncpy(rp->pathname, DEFAULTPATH, sizeof(rp->pathname));

    rig_debug(RIG_DEBUG_TRACE, "%s pathanme %s\n", __func__, rp->pathname);

    RETURNFUNC(RIG_OK);
}


/*
* sdrsharp_get_freq
* Assumes rig!=NULL, STATE(rig)->priv!=NULL, freq!=NULL
*/
static int sdrsharp_get_freq(RIG *rig, vfo_t vfo, freq_t *freq)
{
    char value[MAXARGLEN];
    struct sdrsharp_priv_data *priv = (struct sdrsharp_priv_data *) STATE(rig)->priv;

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

    retval = sdrsharp_transaction(rig, cmd, value, sizeof(value));

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
* sdrsharp_open
* Assumes rig!=NULL
*/
static int sdrsharp_open(RIG *rig)
{
    int retval;
    char value[MAXARGLEN];

    ENTERFUNC;
    value[0] = '?';
    value[1] = 0;

    freq_t freq;
    retval = sdrsharp_get_freq(rig, RIG_VFO_CURR, &freq);

    if (retval != RIG_OK)
    {
        rig_debug(RIG_DEBUG_ERR, "%s: sdrsharp_get_freq not working!!\n", __func__);
        RETURNFUNC(-RIG_EPROTO);
    }

    STATE(rig)->current_vfo = RIG_VFO_A;
    rig_debug(RIG_DEBUG_TRACE, "%s: currvfo=%s value=%s\n", __func__,
              rig_strvfo(STATE(rig)->current_vfo), value);

    RETURNFUNC(retval);
}


/*
* sdrsharp_close
* Assumes rig!=NULL
*/
static int sdrsharp_close(RIG *rig)
{
    ENTERFUNC;

    RETURNFUNC(RIG_OK);
}


/*
* sdrsharp_cleanup
* Assumes rig!=NULL, STATE(rig)->priv!=NULL
*/
static int sdrsharp_cleanup(RIG *rig)
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
    // model_sdrsharp was not getting refilled
    // if we can figure out that one we can re-enable this
#if 0
    int i;

    for (i = 0; modeMap[i].mode_hamlib != 0; ++i)
    {
        if (modeMap[i].mode_sdrsharp)
        {
            free(modeMap[i].mode_sdrsharp);
            modeMap[i].mode_sdrsharp = NULL;
            modeMap[i].mode_hamlib = 0;
        }

    }

#endif

    RETURNFUNC2(RIG_OK);
}


/*
* sdrsharp_get_vfo
* assumes rig!=NULL, vfo != NULL
*/
static int sdrsharp_get_vfo(RIG *rig, vfo_t *vfo)
{
    ENTERFUNC;

    *vfo = RIG_VFO_A;

    RETURNFUNC(RIG_OK);
}


/*
* sdrsharp_set_freq
* assumes rig!=NULL, STATE(rig)->priv!=NULL
*/
static int sdrsharp_set_freq(RIG *rig, vfo_t vfo, freq_t freq)
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

    retval = sdrsharp_transaction(rig, cmd, value, sizeof(value));

    if (retval != RIG_OK)
    {
        RETURNFUNC2(retval);
    }

    sscanf(value, "RPRT %d", &retval);

    RETURNFUNC2(retval);
}


struct rig_caps sdrsharp_caps =
{
    RIG_MODEL(RIG_MODEL_SDRSHARP),
    .model_name = "SDR#/gpredict",
    .mfg_name = "Airspy",
    .version = BACKEND_VER,
    .copyright = "LGPL",
    .status = RIG_STATUS_STABLE,
    .rig_type = RIG_TYPE_RECEIVER,
    .ptt_type = RIG_PTT_NONE,
    .dcd_type = RIG_DCD_NONE,
    .port_type = RIG_PORT_NETWORK,
    .write_delay = 1,
    .post_write_delay = 100,
    .timeout = 1000,
    .retry = 2,

    .has_get_func =  false,
    .has_set_func =  false,
    .has_get_level = false,
    .has_set_level = false,
    .has_get_parm =  false,
    .has_set_parm =  false,
    .level_gran =  {},
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
        { Hz(1), GHz(10), SDRSHARP_MODES, -1, -1, SDRSHARP_VFOS, SDRSHARP_ANTS},
        RIG_FRNG_END,
    },
    .tx_range_list1 = {RIG_FRNG_END,},
    .rx_range_list2 = {
        { Hz(1), GHz(10), SDRSHARP_MODES, -1, -1, SDRSHARP_VFOS, SDRSHARP_ANTS},
        RIG_FRNG_END,
    },
    .tx_range_list2 = {RIG_FRNG_END,},
    .tuning_steps =  {
        {SDRSHARP_MODES, 1},
        {SDRSHARP_MODES, RIG_TS_ANY},
        RIG_TS_END, },
    .filters =  {
        {RIG_MODE_ALL, RIG_FLT_ANY},
        RIG_FLT_END
    },

    .rig_init = sdrsharp_init,
    .rig_open = sdrsharp_open,
    .rig_close = sdrsharp_close,
    .rig_cleanup = sdrsharp_cleanup,

    .get_vfo = sdrsharp_get_vfo, //always RIG_VFO_A
    .set_freq = sdrsharp_set_freq, //F <frequency Hz>\n
    .get_freq = sdrsharp_get_freq, //f\n
    .hamlib_check_rig_caps = HAMLIB_CHECK_RIG_CAPS
};
