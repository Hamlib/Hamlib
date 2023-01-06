/*
 *  Hamlib Netrigctl backend - main file
 *  Copyright (c) 2001-2010 by Stephane Fillod
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
#include <string.h>  /* String function definitions */
#include <unistd.h>  /* UNIX standard function definitions */
#include <errno.h>

#include "hamlib/rig.h"
#include "network.h"
#include "serial.h"
#include "iofunc.h"
#include "misc.h"
#include "num_stdio.h"

#include "dummy.h"

#define CMD_MAX 64
#define BUF_MAX 1024

#define CHKSCN1ARG(a) if ((a) != 1) return -RIG_EPROTO; else do {} while(0)

struct netrigctl_priv_data
{
    vfo_t vfo_curr;
    int rigctld_vfo_mode;
    vfo_t rx_vfo;
    vfo_t tx_vfo;
};

int netrigctl_get_vfo_mode(RIG *rig)
{
    struct netrigctl_priv_data *priv;
    priv = (struct netrigctl_priv_data *)rig->state.priv;
    return priv->rigctld_vfo_mode;
}

/*
 * Helper function with protocol return code parsing
 */
static int netrigctl_transaction(RIG *rig, char *cmd, int len, char *buf)
{
    int ret;

    rig_debug(RIG_DEBUG_VERBOSE, "%s: called len=%d\n", __func__, len);

    /* flush anything in the read buffer before command is sent */
    rig_flush(&rig->state.rigport);

    ret = write_block(&rig->state.rigport, (unsigned char *) cmd, len);

    if (ret != RIG_OK)
    {
        return ret;
    }

    ret = read_string(&rig->state.rigport, (unsigned char *) buf, BUF_MAX, "\n", 1,
                      0, 1);

    if (ret < 0)
    {
        return ret;
    }

    if (strncmp(buf, NETRIGCTL_RET, strlen(NETRIGCTL_RET)) == 0)
    {
        return atoi(buf + strlen(NETRIGCTL_RET));
    }

    return ret;
}

/* this will fill vfostr with the vfo value if the vfo mode is enabled
 * otherwise string will be null terminated
 * this allows us to use the string in snprintf in either mode
 */
static int netrigctl_vfostr(RIG *rig, char *vfostr, int len, vfo_t vfo)
{
    struct netrigctl_priv_data *priv;

    rig_debug(RIG_DEBUG_TRACE, "%s: called vfo=%s\n", __func__, rig_strvfo(vfo));

    if (len < 5)
    {
        rig_debug(RIG_DEBUG_ERR, "%s: len must be >=5, len=%d\n", __func__, len);
        return -RIG_EPROTO;
    }

    vfostr[0] = 0;

    priv = (struct netrigctl_priv_data *)rig->state.priv;

    if (vfo == RIG_VFO_CURR)
    {
        rig_debug(RIG_DEBUG_TRACE, "%s: vfo==RIG_VFO_CURR, curr=%s\n", __func__,
                  rig_strvfo(priv->vfo_curr));
        vfo = priv->vfo_curr;

        if (vfo == RIG_VFO_NONE) { vfo = RIG_VFO_A; }
    }
    else if (vfo == RIG_VFO_RX) { vfo = priv->rx_vfo; }
    else if (vfo == RIG_VFO_TX) { vfo = priv->tx_vfo; }

    rig_debug(RIG_DEBUG_TRACE, "%s: vfo_opt=%d\n", __func__, rig->state.vfo_opt);

    if (rig->state.vfo_opt || priv->rigctld_vfo_mode)
    {
        rig_debug(RIG_DEBUG_TRACE, "%s: vfo_opt vfo=%u\n", __func__, vfo);
        char *myvfo;

        switch (vfo)
        {
        case RIG_VFO_B: myvfo = "VFOB"; break;

        case RIG_VFO_C: myvfo = "VFOC"; break;

        case RIG_VFO_MAIN: myvfo = "Main"; break;

        case RIG_VFO_MAIN_A: myvfo = "MainA"; break;

        case RIG_VFO_MAIN_B: myvfo = "MainB"; break;

        case RIG_VFO_SUB: myvfo = "Sub"; break;

        case RIG_VFO_SUB_A: myvfo = "SubA"; break;

        case RIG_VFO_SUB_B: myvfo = "SubB"; break;

        case RIG_VFO_MEM: myvfo = "MEM"; break;

        default: myvfo = "VFOA";
        }

        SNPRINTF(vfostr, len, " %s", myvfo);
    }

    return RIG_OK;
}

static int netrigctl_init(RIG *rig)
{
    // cppcheck says leak here but it's freed in cleanup
    struct netrigctl_priv_data *priv;

    if (!rig || !rig->caps)
    {
        return -RIG_EINVAL;
    }

    rig->state.priv = (struct netrigctl_priv_data *)calloc(1, sizeof(
                          struct netrigctl_priv_data));

    if (!rig->state.priv)
    {
        return -RIG_ENOMEM;
    }

    priv = rig->state.priv;
    memset(priv, 0, sizeof(struct netrigctl_priv_data));

    rig_debug(RIG_DEBUG_TRACE, "%s version %s\n", __func__, rig->caps->version);

    /*
     * set arbitrary initial status
     * VFO will be updated in open call
     */
    priv->vfo_curr = RIG_VFO_A;
    priv->rigctld_vfo_mode = 0;

    return RIG_OK;
}

static int netrigctl_cleanup(RIG *rig)
{
    if (rig->state.priv) { free(rig->state.priv); }

    rig->state.priv = NULL;
    return RIG_OK;
}

int parse_array_int(const char *s, const char *delim, int *array, int array_len)
{
    char *p;
    char *dup = strdup(s);
    char *rest = dup;
    int n = 0;

    while ((p = strtok_r(rest, delim, &rest)))
    {
        if (n == array_len)   // too many items
        {
            return n;
        }

        array[n] = atoi(p);
        //printf("%d\n", array[n]);
        ++n;
    }

    free(dup);
    return n;
}

int parse_array_double(const char *s, const char *delim, double *array,
                       int array_len)
{
    char *p;
    char *dup = strdup(s);
    char *rest = dup;
    int n = 0;

    while ((p = strtok_r(rest, delim, &rest)))
    {
        if (n == array_len)   // too many items
        {
            return n;
        }

        array[n] = atof(p);
        //printf("%f\n", array[n]);
        ++n;
    }

    free(dup);
    return n;
}



static int netrigctl_open(RIG *rig)
{
    int ret, i;
    struct rig_state *rs = &rig->state;
    int prot_ver;
    char cmd[CMD_MAX];
    char buf[BUF_MAX];
    struct netrigctl_priv_data *priv;


    ENTERFUNC;

    priv = (struct netrigctl_priv_data *)rig->state.priv;
    priv->rx_vfo = RIG_VFO_A;
    priv->tx_vfo = RIG_VFO_B;

    SNPRINTF(cmd, sizeof(cmd), "\\chk_vfo\n");
    ret = netrigctl_transaction(rig, cmd, strlen(cmd), buf);

    if (sscanf(buf, "CHKVFO %d", &priv->rigctld_vfo_mode) == 1)
    {
        rig->state.vfo_opt = 1;
        rig_debug(RIG_DEBUG_TRACE, "%s: chkvfo=%d\n", __func__, priv->rigctld_vfo_mode);
    }
    else if (ret == 2)
    {
        if (buf[0]) { sscanf(buf, "%d", &priv->rigctld_vfo_mode); }
    }
    else if (ret < 0)
    {
        rig_debug(RIG_DEBUG_WARN, "%s: chk_vfo error: %s\n", __func__,
                  rigerror(ret));
    }
    else
    {
        rig_debug(RIG_DEBUG_ERR, "%s:  unknown return from netrigctl_transaction=%d\n",
                  __func__, ret);
    }

    rig_debug(RIG_DEBUG_VERBOSE, "%s: vfo_mode=%d\n", __func__,
              priv->rigctld_vfo_mode);

    SNPRINTF(cmd, sizeof(cmd), "\\dump_state\n");

    ret = netrigctl_transaction(rig, cmd, strlen(cmd), buf);

    if (ret <= 0)
    {
        RETURNFUNC((ret < 0) ? ret : -RIG_EPROTO);
    }

    prot_ver = atoi(buf);
#define RIGCTLD_PROT_VER 0

    if (prot_ver < RIGCTLD_PROT_VER)
    {
        RETURNFUNC(-RIG_EPROTO);
    }

    ret = read_string(&rig->state.rigport, (unsigned char *) buf, BUF_MAX, "\n", 1,
                      0, 1);

    if (ret <= 0)
    {
        RETURNFUNC((ret < 0) ? ret : -RIG_EPROTO);
    }

    ret = read_string(&rig->state.rigport, (unsigned char *) buf, BUF_MAX, "\n", 1,
                      0, 1);

    if (ret <= 0)
    {
        RETURNFUNC((ret < 0) ? ret : -RIG_EPROTO);
    }

    rs->deprecated_itu_region = atoi(buf);

    for (i = 0; i < HAMLIB_FRQRANGESIZ; i++)
    {
        ret = read_string(&rig->state.rigport, (unsigned char *) buf, BUF_MAX, "\n", 1,
                          0, 1);

        if (ret <= 0)
        {
            RETURNFUNC((ret < 0) ? ret : -RIG_EPROTO);
        }

        ret = num_sscanf(buf, "%"SCNfreq"%"SCNfreq"%"SCNXll"%d%d%x%x",
                         &(rs->rx_range_list[i].startf),
                         &(rs->rx_range_list[i].endf),
                         &(rs->rx_range_list[i].modes),
                         &(rs->rx_range_list[i].low_power),
                         &(rs->rx_range_list[i].high_power),
                         &(rs->rx_range_list[i].vfo),
                         &(rs->rx_range_list[i].ant)
                        );

        if (ret != 7)
        {
            RETURNFUNC(-RIG_EPROTO);
        }

        if (RIG_IS_FRNG_END(rs->rx_range_list[i]))
        {
            break;
        }
    }

    for (i = 0; i < HAMLIB_FRQRANGESIZ; i++)
    {
        ret = read_string(&rig->state.rigport, (unsigned char *) buf, BUF_MAX, "\n", 1,
                          0, 1);

        if (ret <= 0)
        {
            RETURNFUNC((ret < 0) ? ret : -RIG_EPROTO);
        }

        ret = num_sscanf(buf, "%"SCNfreq"%"SCNfreq"%"SCNXll"%d%d%x%x",
                         &rs->tx_range_list[i].startf,
                         &rs->tx_range_list[i].endf,
                         &rs->tx_range_list[i].modes,
                         &rs->tx_range_list[i].low_power,
                         &rs->tx_range_list[i].high_power,
                         &rs->tx_range_list[i].vfo,
                         &rs->tx_range_list[i].ant
                        );

        if (ret != 7)
        {
            RETURNFUNC(-RIG_EPROTO);
        }

        if (RIG_IS_FRNG_END(rs->tx_range_list[i]))
        {
            break;
        }

        switch (i)
        {
        }

        rig->caps->tx_range_list1->startf = rs->tx_range_list[i].startf;
        rig->caps->tx_range_list1->endf = rs->tx_range_list[i].endf;
        rig->caps->tx_range_list1->modes = rs->tx_range_list[i].modes;
        rig->caps->tx_range_list1->low_power = rs->tx_range_list[i].low_power;
        rig->caps->tx_range_list1->high_power = rs->tx_range_list[i].high_power;
        rig->caps->tx_range_list1->vfo = rs->tx_range_list[i].vfo;
        rig->caps->tx_range_list1->ant = rs->tx_range_list[i].ant;
    }

    for (i = 0; i < HAMLIB_TSLSTSIZ; i++)
    {
        ret = read_string(&rig->state.rigport, (unsigned char *) buf, BUF_MAX, "\n", 1,
                          0, 1);

        if (ret <= 0)
        {
            RETURNFUNC((ret < 0) ? ret : -RIG_EPROTO);
        }

        ret = sscanf(buf, "%"SCNXll"%ld",
                     &rs->tuning_steps[i].modes,
                     &rs->tuning_steps[i].ts);

        if (ret != 2)
        {
            RETURNFUNC(-RIG_EPROTO);
        }

        if (RIG_IS_TS_END(rs->tuning_steps[i]))
        {
            break;
        }
    }

    for (i = 0; i < HAMLIB_FLTLSTSIZ; i++)
    {
        ret = read_string(&rig->state.rigport, (unsigned char *) buf, BUF_MAX, "\n", 1,
                          0, 1);

        if (ret <= 0)
        {
            RETURNFUNC((ret < 0) ? ret : -RIG_EPROTO);
        }

        ret = sscanf(buf, "%"SCNXll"%ld",
                     &rs->filters[i].modes,
                     &rs->filters[i].width);

        if (ret != 2)
        {
            RETURNFUNC(-RIG_EPROTO);
        }

        if (RIG_IS_FLT_END(rs->filters[i]))
        {
            break;
        }
    }

#if 0
    /* TODO */
    chan_t chan_list[HAMLIB_CHANLSTSIZ]; /*!< Channel list, zero ended */
#endif

    ret = read_string(&rig->state.rigport, (unsigned char *) buf, BUF_MAX, "\n", 1,
                      0, 1);

    if (ret <= 0)
    {
        RETURNFUNC((ret < 0) ? ret : -RIG_EPROTO);
    }

    rig->caps->max_rit = rs->max_rit = atol(buf);

    ret = read_string(&rig->state.rigport, (unsigned char *) buf, BUF_MAX, "\n", 1,
                      0, 1);

    if (ret <= 0)
    {
        RETURNFUNC((ret < 0) ? ret : -RIG_EPROTO);
    }

    rig->caps->max_xit = rs->max_xit = atol(buf);

    ret = read_string(&rig->state.rigport, (unsigned char *) buf, BUF_MAX, "\n", 1,
                      0, 1);

    if (ret <= 0)
    {
        RETURNFUNC((ret < 0) ? ret : -RIG_EPROTO);
    }

    rig->caps->max_ifshift = rs->max_ifshift = atol(buf);

    ret = read_string(&rig->state.rigport, (unsigned char *) buf, BUF_MAX, "\n", 1,
                      0, 1);

    if (ret <= 0)
    {
        RETURNFUNC((ret < 0) ? ret : -RIG_EPROTO);
    }

    rs->announces = atoi(buf);

    ret = read_string(&rig->state.rigport, (unsigned char *) buf, BUF_MAX, "\n", 1,
                      0, 1);

    if (ret <= 0)
    {
        RETURNFUNC((ret < 0) ? ret : -RIG_EPROTO);
    }

    ret = sscanf(buf, "%d%d%d%d%d%d%d",
                 &rs->preamp[0], &rs->preamp[1],
                 &rs->preamp[2], &rs->preamp[3],
                 &rs->preamp[4], &rs->preamp[5],
                 &rs->preamp[6]);
    rig->caps->preamp[0] = rs->preamp[0];
    rig->caps->preamp[1] = rs->preamp[1];
    rig->caps->preamp[2] = rs->preamp[2];
    rig->caps->preamp[3] = rs->preamp[3];
    rig->caps->preamp[4] = rs->preamp[4];
    rig->caps->preamp[5] = rs->preamp[5];
    rig->caps->preamp[6] = rs->preamp[6];

    if (ret < 0 || ret >= HAMLIB_MAXDBLSTSIZ)
    {
        ret = 0;
    }

    rig->caps->preamp[ret] = rs->preamp[ret] = RIG_DBLST_END;

    ret = read_string(&rig->state.rigport, (unsigned char *) buf, BUF_MAX, "\n", 1,
                      0, 1);

    if (ret <= 0)
    {
        RETURNFUNC((ret < 0) ? ret : -RIG_EPROTO);
    }

    ret = sscanf(buf, "%d%d%d%d%d%d%d",
                 &rs->attenuator[0], &rs->attenuator[1],
                 &rs->attenuator[2], &rs->attenuator[3],
                 &rs->attenuator[4], &rs->attenuator[5],
                 &rs->attenuator[6]);
    rig->caps->attenuator[0] = rs->attenuator[0];
    rig->caps->attenuator[1] = rs->attenuator[1];
    rig->caps->attenuator[2] = rs->attenuator[2];
    rig->caps->attenuator[3] = rs->attenuator[3];
    rig->caps->attenuator[4] = rs->attenuator[4];
    rig->caps->attenuator[5] = rs->attenuator[5];
    rig->caps->attenuator[6] = rs->attenuator[6];

    if (ret < 0 || ret >= HAMLIB_MAXDBLSTSIZ)
    {
        ret = 0;
    }

    rig->caps->attenuator[ret] = rs->attenuator[ret] = RIG_DBLST_END;

    ret = read_string(&rig->state.rigport, (unsigned char *) buf, BUF_MAX, "\n", 1,
                      0, 1);

    if (ret <= 0)
    {
        RETURNFUNC((ret < 0) ? ret : -RIG_EPROTO);
    }

    rig->caps->has_get_func = rs->has_get_func = strtoll(buf, NULL, 0);

    ret = read_string(&rig->state.rigport, (unsigned char *) buf, BUF_MAX, "\n", 1,
                      0, 1);

    if (ret <= 0)
    {
        RETURNFUNC((ret < 0) ? ret : -RIG_EPROTO);
    }

    rig->caps->has_set_func = rs->has_set_func = strtoll(buf, NULL, 0);

    ret = read_string(&rig->state.rigport, (unsigned char *) buf, BUF_MAX, "\n", 1,
                      0, 1);

    if (ret <= 0)
    {
        RETURNFUNC((ret < 0) ? ret : -RIG_EPROTO);
    }

    rig->caps->has_get_level = rs->has_get_level = strtoll(buf, NULL, 0);

#if 0 // don't think we need this anymore

    if (rs->has_get_level & RIG_LEVEL_RAWSTR)
    {
        /* include STRENGTH because the remote rig may be able to
           provide a front end emulation, if it can't then an
           -RIG_EINVAL will be returned */
        rs->has_get_level |= RIG_LEVEL_STRENGTH;
        rig->caps->has_get_level |= RIG_LEVEL_STRENGTH;
    }

#endif

    ret = read_string(&rig->state.rigport, (unsigned char *) buf, BUF_MAX, "\n", 1,
                      0, 1);

    if (ret <= 0)
    {
        RETURNFUNC((ret < 0) ? ret : -RIG_EPROTO);
    }

    rig->caps->has_set_level = rs->has_set_level = strtoll(buf, NULL, 0);

    ret = read_string(&rig->state.rigport, (unsigned char *) buf, BUF_MAX, "\n", 1,
                      0, 1);

    if (ret <= 0)
    {
        RETURNFUNC((ret < 0) ? ret : -RIG_EPROTO);
    }

    rs->has_get_parm = strtoll(buf, NULL, 0);

    ret = read_string(&rig->state.rigport, (unsigned char *) buf, BUF_MAX, "\n", 1,
                      0, 1);

    if (ret <= 0)
    {
        RETURNFUNC((ret < 0) ? ret : -RIG_EPROTO);
    }

    rig->caps->has_set_parm = rs->has_set_parm = strtoll(buf, NULL, 0);

#if 0
    gran_t level_gran[RIG_SETTING_MAX];   /*!< level granularity */
    gran_t parm_gran[RIG_SETTING_MAX];    /*!< parm granularity */
#endif

    for (i = 0; i < HAMLIB_FRQRANGESIZ
            && !RIG_IS_FRNG_END(rs->rx_range_list[i]); i++)
    {
        rs->mode_list |= rs->rx_range_list[i].modes;
        rs->vfo_list |= rs->rx_range_list[i].vfo;
    }

    for (i = 0; i < HAMLIB_FRQRANGESIZ
            && !RIG_IS_FRNG_END(rs->tx_range_list[i]); i++)
    {
        rs->mode_list |= rs->tx_range_list[i].modes;
        rs->vfo_list |= rs->tx_range_list[i].vfo;
    }

    if (rs->vfo_list == 0)
    {
        rig_debug(RIG_DEBUG_VERBOSE, "%s: vfo_list empty, defaulting to A/B\n",
                  __func__);
        rs->vfo_list = RIG_VFO_A | RIG_VFO_B;
    }

    if (prot_ver == 0) { RETURNFUNC(RIG_OK); }

    // otherwise we continue reading protocol 1 fields


    do
    {
        char setting[32], value[1024];
        ret = read_string(&rig->state.rigport, (unsigned char *) buf, BUF_MAX, "\n", 1,
                          0, 1);
        strtok(buf, "\r\n"); // chop the EOL

        if (ret <= 0)
        {
            RETURNFUNC((ret < 0) ? ret : -RIG_EPROTO);
        }

        if (strncmp(buf, "done", 4) == 0) { RETURNFUNC(RIG_OK); }

        if (sscanf(buf, "%31[^=]=%1023[^\t\n]", setting, value) == 2)
        {
            if (strcmp(setting, "vfo_ops") == 0)
            {
                rig->caps->vfo_ops = strtoll(value, NULL, 0);
                rig_debug(RIG_DEBUG_TRACE, "%s: %s set to %d\n", __func__, setting,
                          rig->caps->vfo_ops);
            }
            else if (strcmp(setting, "ptt_type") == 0)
            {
                ptt_type_t temp = (ptt_type_t)strtol(value, NULL, 0);
                rig_debug(RIG_DEBUG_VERBOSE, "%s: ptt_type='%s'(%d)\n", __func__, value, temp);

                if (RIG_PTT_RIG_MICDATA == rig->state.pttport.type.ptt
                        || temp == RIG_PTT_RIG_MICDATA)
                {
                    /*
                     * remote PTT must always be RIG_PTT_RIG_MICDATA
                     * if there is any PTT capability and we have not
                     * locally overridden it
                     */
                    rig->state.pttport.type.ptt = RIG_PTT_RIG_MICDATA;
                    rig->caps->ptt_type = RIG_PTT_RIG_MICDATA;
                    rig_debug(RIG_DEBUG_TRACE, "%s: %s set to %d\n", __func__, setting,
                              rig->state.pttport.type.ptt);
                }
                else
                {
                    rig_debug(RIG_DEBUG_VERBOSE, "%s: ptt_type= %d\n", __func__, temp);
                    rig->state.pttport.type.ptt = temp;
                    rig->caps->ptt_type = temp;
                }
            }

            else if (strcmp(setting, "targetable_vfo") == 0)
            {
                rig->caps->targetable_vfo = strtol(value, NULL, 0);
                rig_debug(RIG_DEBUG_VERBOSE, "%s: targetable_vfo=0x%2x\n", __func__,
                          rig->caps->targetable_vfo);
            }

            else if (strcmp(setting, "has_set_vfo") == 0)
            {
                int has = strtol(value, NULL, 0);

                if (!has) { rig->caps->set_vfo = NULL; }
            }
            else if (strcmp(setting, "has_get_vfo") == 0)
            {
                int has = strtol(value, NULL, 0);

                if (!has) { rig->caps->get_vfo = NULL; }
            }
            else if (strcmp(setting, "has_set_freq") == 0)
            {
                int has = strtol(value, NULL, 0);

                if (!has) { rig->caps->set_freq = NULL; }
            }
            else if (strcmp(setting, "has_get_freq") == 0)
            {
                int has = strtol(value, NULL, 0);

                if (!has) { rig->caps->get_freq = NULL; }
            }
            else if (strcmp(setting, "has_set_conf") == 0)
            {
                int has = strtol(value, NULL, 0);

                if (!has) { rig->caps->set_conf = NULL; }
            }
            else if (strcmp(setting, "has_get_conf") == 0)
            {
                int has = strtol(value, NULL, 0);

                if (!has) { rig->caps->get_conf = NULL; }
            }

#if 0 // for the future
            else if (strcmp(setting, "has_set_trn") == 0)
            {
                int has = strtol(value, NULL, 0);

                if (!has) { rig->caps->set_trn = NULL; }
            }
            else if (strcmp(setting, "has_get_trn") == 0)
            {
                int has = strtol(value, NULL, 0);

                if (!has) { rig->caps->get_trn = NULL; }
            }

#endif
            else if (strcmp(setting, "has_power2mW") == 0)
            {
                int has = strtol(value, NULL, 0);

                if (!has) { rig->caps->power2mW = NULL; }
            }
            else if (strcmp(setting, "has_mW2power") == 0)
            {
                int has = strtol(value, NULL, 0);

                if (!has) { rig->caps->mW2power = NULL; }
            }
            else if (strcmp(setting, "timeout") == 0)
            {
                // use the rig's timeout value plus 500ms for potential network delays
                rig->caps->timeout = strtol(value, NULL, 0) + 500;
                rig_debug(RIG_DEBUG_TRACE, "%s: timeout value = '%s', final timeout=%d\n",
                          __func__, value, rig->caps->timeout);
            }
            else if (strcmp(setting, "rig_model") == 0)
            {
                rig_debug(RIG_DEBUG_TRACE, "%s: rig_model=%s\n", __func__, value);
            }
            else if (strcmp(setting, "rigctld_version") == 0)
            {
                rig_debug(RIG_DEBUG_TRACE, "%s: rigctld_version=%s\n", __func__, value);
            }
            else if (strcmp(setting, "ctcss_list") == 0)
            {
                int n;
                double ctcss[CTCSS_LIST_SIZE];
                rig->caps->ctcss_list = calloc(CTCSS_LIST_SIZE, sizeof(tone_t));
                n = parse_array_double(value, " \n\r", ctcss, CTCSS_LIST_SIZE);

                for (i = 0; i < CTCSS_LIST_SIZE && ctcss[i] != 0; ++i) { rig->caps->ctcss_list[i] = ctcss[i] * 10; }

                if (n < CTCSS_LIST_SIZE) { rig->caps->ctcss_list[n] = 0; }
            }
            else if (strcmp(setting, "dcs_list") == 0)
            {
                int n;
                int dcs[DCS_LIST_SIZE + 1];
                rig->caps->dcs_list = calloc(DCS_LIST_SIZE, sizeof(tone_t));
                n = parse_array_int(value, " \n\r", dcs, DCS_LIST_SIZE);

                for (i = 0; i < DCS_LIST_SIZE && dcs[i] != 0; i++) { rig->caps->dcs_list[i] = dcs[i]; }

                if (n < DCS_LIST_SIZE) { rig->caps->dcs_list[n] = 0; }
            }
            else if (strcmp(setting, "agc_levels") == 0)
            {
                int i = 0;
                char *p = strtok(value, " ");
                rig->caps->agc_levels[0] = RIG_AGC_NONE; // default value gets overwritten
                rig->caps->agc_level_count = 1;

                while (p)
                {
                    int agc_code;
                    char agc_string[32];
                    int n = sscanf(p, "%d=%s\n", &agc_code, agc_string);

                    if (n == 2)
                    {
                        rig->caps->agc_levels[i++] = agc_code;
                        rig->caps->agc_level_count++;
                        rig_debug(RIG_DEBUG_VERBOSE, "%s: rig has agc code=%d, level=%s\n", __func__,
                                  agc_code, agc_string);
                    }
                    else
                    {
                        rig_debug(RIG_DEBUG_ERR, "%s did not parse code=agc from '%s'\n", __func__, p);
                    }

                    rig_debug(RIG_DEBUG_VERBOSE, "%d=%s\n", agc_code, agc_string);
                    p = strtok(NULL, " ");
                }
            }
            else
            {
                // not an error -- just a warning for backward compatibility
                rig_debug(RIG_DEBUG_ERR, "%s: unknown setting='%s'\n", __func__, buf);
            }
        }
        else
        {
            rig_debug(RIG_DEBUG_ERR,
                      "%s: invalid dumpcaps line, expected 'setting=value', got '%s'\n", __func__,
                      buf);
        }


    }
    while (1);

    RETURNFUNC(RIG_OK);
}

static int netrigctl_close(RIG *rig)
{
    int ret;
    char buf[BUF_MAX];

    rig_debug(RIG_DEBUG_VERBOSE, "%s called\n", __func__);

    ret = netrigctl_transaction(rig, "q\n", 2, buf);

    if (ret != RIG_OK)
    {
        rig_debug(RIG_DEBUG_ERR, "%s: close error %s\n", __func__, rigerror(ret));
        return ret;
    }

    rig_debug(RIG_DEBUG_ERR, "%s: done\n", __func__);
    usleep(10 * 1000);

    return RIG_OK;
}

static int netrigctl_set_freq(RIG *rig, vfo_t vfo, freq_t freq)
{
    int ret;
    char cmd[CMD_MAX];
    char buf[BUF_MAX];
    char vfostr[16] = "";

    rig_debug(RIG_DEBUG_VERBOSE, "%s called\n", __func__);

#if 1 // implement set_freq VFO later if it can be detected
    ret = netrigctl_vfostr(rig, vfostr, sizeof(vfostr), vfo);

    if (ret != RIG_OK) { return ret; }

    SNPRINTF(cmd, sizeof(cmd), "F%s %"FREQFMT"\n", vfostr, freq);
#else
    SNPRINTF(cmd, sizeof(cmd), "F %"FREQFMT"\n", freq);
#endif

    ret = netrigctl_transaction(rig, cmd, strlen(cmd), buf);
    rig_debug(RIG_DEBUG_TRACE, "%s: cmd=%s\n", __func__, strtok(cmd, "\r\n"));

    if (ret > 0)
    {
        return -RIG_EPROTO;
    }
    else
    {
        return ret;
    }
}

static int netrigctl_get_freq(RIG *rig, vfo_t vfo, freq_t *freq)
{
    int ret;
    char cmd[CMD_MAX];
    char buf[BUF_MAX];
    char vfostr[16] = "";
#if 0 // disable until we figure out if we can do this without breaking backwards compatibility
    char vfotmp[16];
#endif

    rig_debug(RIG_DEBUG_VERBOSE, "%s called, vfo=%s\n", __func__,
              rig_strvfo(vfo));

    ret = netrigctl_vfostr(rig, vfostr, sizeof(vfostr), vfo);

    if (ret != RIG_OK) { return ret; }

    SNPRINTF(cmd, sizeof(cmd), "f%s\n", vfostr);

    ret = netrigctl_transaction(rig, cmd, strlen(cmd), buf);

    rig_debug(RIG_DEBUG_TRACE, "%s: cmd=%s, reply=%s\n", __func__, strtok(cmd,
              "\r\n"), buf);

    if (ret <= 0)
    {
        return (ret < 0) ? ret : -RIG_EPROTO;
    }

    CHKSCN1ARG(num_sscanf(buf, "%"SCNfreq, freq));

#if 0 // implement set_freq VFO later if it can be detected
    ret = read_string(&rig->state.rigport, buf, BUF_MAX, "\n", 1, 0, 1);

    if (ret <= 0)
    {
        return (ret < 0) ? ret : -RIG_EPROTO;
    }

    *vfotmp = rig_parse_vfo(buf);
#endif

    return RIG_OK;
}


static int netrigctl_set_mode(RIG *rig, vfo_t vfo, rmode_t mode,
                              pbwidth_t width)
{
    int ret;
    char cmd[CMD_MAX];
    char buf[BUF_MAX];
    char vfostr[16] = "";

    rig_debug(RIG_DEBUG_VERBOSE, "%s called, vfo=%s\n", __func__, rig_strvfo(vfo));

    ret = netrigctl_vfostr(rig, vfostr, sizeof(vfostr), vfo);

    if (ret != RIG_OK) { return ret; }

    SNPRINTF(cmd, sizeof(cmd), "M%s %s %li\n",
             vfostr, rig_strrmode(mode), width);

    ret = netrigctl_transaction(rig, cmd, strlen(cmd), buf);

    if (ret > 0)
    {
        return -RIG_EPROTO;
    }
    else
    {
        return ret;
    }
}


static int netrigctl_get_mode(RIG *rig, vfo_t vfo, rmode_t *mode,
                              pbwidth_t *width)
{
    int ret;
    char cmd[CMD_MAX];
    char buf[BUF_MAX];
    char vfostr[16] = "";

    rig_debug(RIG_DEBUG_VERBOSE, "%s called, vfo=%s\n", __func__, rig_strvfo(vfo));

    ret = netrigctl_vfostr(rig, vfostr, sizeof(vfostr), vfo);

    if (ret != RIG_OK) { return ret; }

    SNPRINTF(cmd, sizeof(cmd), "m%s\n", vfostr);

    ret = netrigctl_transaction(rig, cmd, strlen(cmd), buf);

    if (ret <= 0)
    {
        return (ret < 0) ? ret : -RIG_EPROTO;
    }

    if (buf[ret - 1] == '\n') { buf[ret - 1] = '\0'; } /* chomp */

    *mode = rig_parse_mode(buf);

    ret = read_string(&rig->state.rigport, (unsigned char *) buf, BUF_MAX, "\n", 1,
                      0, 1);

    if (ret <= 0)
    {
        return (ret < 0) ? ret : -RIG_EPROTO;
    }

    *width = atoi(buf);

    return RIG_OK;
}


static int netrigctl_set_vfo(RIG *rig, vfo_t vfo)
{
    int ret;
    char cmd[CMD_MAX];
    char buf[BUF_MAX];
    char vfostr[16] = "";
    struct netrigctl_priv_data *priv;

    rig_debug(RIG_DEBUG_VERBOSE, "%s called\n", __func__);

    priv = (struct netrigctl_priv_data *)rig->state.priv;

    SNPRINTF(cmd, sizeof(cmd), "V%s %s\n", vfostr, rig_strvfo(vfo));
    rig_debug(RIG_DEBUG_VERBOSE, "%s: cmd='%s'\n", __func__, cmd);
    ret = netrigctl_transaction(rig, cmd, strlen(cmd), buf);

    if (ret > 0)
    {
        return -RIG_EPROTO;
    }

    priv->vfo_curr = vfo; // remember our vfo
    rig->state.current_vfo = vfo;
    return ret;
}


static int netrigctl_get_vfo(RIG *rig, vfo_t *vfo)
{
    int ret;
    char cmd[CMD_MAX];
    char buf[BUF_MAX];
    struct netrigctl_priv_data *priv;

    rig_debug(RIG_DEBUG_VERBOSE, "%s called\n", __func__);

    priv = (struct netrigctl_priv_data *)rig->state.priv;

    SNPRINTF(cmd, sizeof(cmd), "v\n");

    ret = netrigctl_transaction(rig, cmd, strlen(cmd), buf);

    if (ret == -RIG_ENAVAIL || ret == -RIG_ENIMPL)
    {
        // for rigs without get_vfo we'll use our saved vfo
        *vfo = priv->vfo_curr;
        return RIG_OK;
    }

    if (ret <= 0)
    {
        return (ret < 0) ? ret : -RIG_EPROTO;
    }

    if (buf[ret - 1] == '\n') { buf[ret - 1] = '\0'; } /* chomp */

    *vfo = rig_parse_vfo(buf);

    priv->vfo_curr = *vfo;

    return RIG_OK;
}


static int netrigctl_set_ptt(RIG *rig, vfo_t vfo, ptt_t ptt)
{
    int ret;
    char cmd[CMD_MAX];
    char buf[BUF_MAX];
    char vfostr[16] = "";

    rig_debug(RIG_DEBUG_VERBOSE, "%s called vfo=%s, ptt=%d, ptt_type=%d\n",
              __func__,
              rig_strvfo(vfo), ptt, rig->state.pttport.type.ptt);

    if (rig->state.pttport.type.ptt == RIG_PTT_NONE) { return RIG_OK; }

    ret = netrigctl_vfostr(rig, vfostr, sizeof(vfostr), RIG_VFO_A);

    if (ret != RIG_OK) { return ret; }

    SNPRINTF(cmd, sizeof(cmd), "T%s %d\n", vfostr, ptt);

    rig_debug(RIG_DEBUG_TRACE, "%s: cmd=%s", __func__, cmd);

    ret = netrigctl_transaction(rig, cmd, strlen(cmd), buf);

    if (ret > 0)
    {
        return -RIG_EPROTO;
    }
    else
    {
        return ret;
    }
}


static int netrigctl_get_ptt(RIG *rig, vfo_t vfo, ptt_t *ptt)
{
    int ret;
    char cmd[CMD_MAX];
    char buf[BUF_MAX];
    char vfostr[16] = "";

    rig_debug(RIG_DEBUG_VERBOSE, "%s called\n", __func__);

    ret = netrigctl_vfostr(rig, vfostr, sizeof(vfostr), RIG_VFO_A);

    if (ret != RIG_OK) { return ret; }

    SNPRINTF(cmd, sizeof(cmd), "t%s\n", vfostr);

    ret = netrigctl_transaction(rig, cmd, strlen(cmd), buf);

    if (ret <= 0)
    {
        return (ret < 0) ? ret : -RIG_EPROTO;
    }

    *ptt = atoi(buf);

    return RIG_OK;
}

static int netrigctl_get_dcd(RIG *rig, vfo_t vfo, dcd_t *dcd)
{
    int ret;
    char cmd[CMD_MAX];
    char buf[BUF_MAX];
    char vfostr[16] = "";

    rig_debug(RIG_DEBUG_VERBOSE, "%s called\n", __func__);

    ret = netrigctl_vfostr(rig, vfostr, sizeof(vfostr), RIG_VFO_A);

    if (ret != RIG_OK) { return ret; }

    SNPRINTF(cmd, sizeof(cmd), "\\get_dcd%s\n", vfostr);  /* FIXME */

    ret = netrigctl_transaction(rig, cmd, strlen(cmd), buf);

    if (ret <= 0)
    {
        return (ret < 0) ? ret : -RIG_EPROTO;
    }

    *dcd = atoi(buf);

    return RIG_OK;
}


static int netrigctl_set_rptr_shift(RIG *rig, vfo_t vfo,
                                    rptr_shift_t rptr_shift)
{
    int ret;
    char cmd[CMD_MAX];
    char buf[BUF_MAX];
    char vfostr[16] = "";

    rig_debug(RIG_DEBUG_VERBOSE, "%s called\n", __func__);

    ret = netrigctl_vfostr(rig, vfostr, sizeof(vfostr), RIG_VFO_A);

    if (ret != RIG_OK) { return ret; }

    SNPRINTF(cmd, sizeof(cmd), "R%s %s\n", vfostr, rig_strptrshift(rptr_shift));

    ret = netrigctl_transaction(rig, cmd, strlen(cmd), buf);

    if (ret > 0)
    {
        return -RIG_EPROTO;
    }
    else
    {
        return ret;
    }
}


static int netrigctl_get_rptr_shift(RIG *rig, vfo_t vfo,
                                    rptr_shift_t *rptr_shift)
{
    int ret;
    char cmd[CMD_MAX];
    char buf[BUF_MAX];
    char vfostr[16] = "";

    rig_debug(RIG_DEBUG_VERBOSE, "%s called\n", __func__);

    ret = netrigctl_vfostr(rig, vfostr, sizeof(vfostr), RIG_VFO_A);

    if (ret != RIG_OK) { return ret; }

    SNPRINTF(cmd, sizeof(cmd), "r%s\n", vfostr);

    ret = netrigctl_transaction(rig, cmd, strlen(cmd), buf);

    if (ret <= 0)
    {
        return (ret < 0) ? ret : -RIG_EPROTO;
    }

    if (buf[ret - 1] == '\n') { buf[ret - 1] = '\0'; } /* chomp */

    *rptr_shift = rig_parse_rptr_shift(buf);

    return RIG_OK;
}


static int netrigctl_set_rptr_offs(RIG *rig, vfo_t vfo, shortfreq_t rptr_offs)
{
    int ret;
    char cmd[CMD_MAX];
    char buf[BUF_MAX];
    char vfostr[16] = "";

    rig_debug(RIG_DEBUG_VERBOSE, "%s called\n", __func__);

    ret = netrigctl_vfostr(rig, vfostr, sizeof(vfostr), RIG_VFO_A);

    if (ret != RIG_OK) { return ret; }

    SNPRINTF(cmd, sizeof(cmd), "O%s %ld\n", vfostr, rptr_offs);

    ret = netrigctl_transaction(rig, cmd, strlen(cmd), buf);

    if (ret > 0)
    {
        return -RIG_EPROTO;
    }
    else
    {
        return ret;
    }
}


static int netrigctl_get_rptr_offs(RIG *rig, vfo_t vfo, shortfreq_t *rptr_offs)
{
    int ret;
    char cmd[CMD_MAX];
    char buf[BUF_MAX];
    char vfostr[16] = "";

    rig_debug(RIG_DEBUG_VERBOSE, "%s called\n", __func__);

    ret = netrigctl_vfostr(rig, vfostr, sizeof(vfostr), RIG_VFO_A);

    if (ret != RIG_OK) { return ret; }

    SNPRINTF(cmd, sizeof(cmd), "o%s\n", vfostr);

    ret = netrigctl_transaction(rig, cmd, strlen(cmd), buf);

    if (ret <= 0)
    {
        return (ret < 0) ? ret : -RIG_EPROTO;
    }

    *rptr_offs = atoi(buf);

    return RIG_OK;
}


static int netrigctl_set_ctcss_tone(RIG *rig, vfo_t vfo, tone_t tone)
{
    int ret;
    char cmd[CMD_MAX];
    char buf[BUF_MAX];
    char vfostr[16] = "";

    rig_debug(RIG_DEBUG_VERBOSE, "%s called\n", __func__);

    ret = netrigctl_vfostr(rig, vfostr, sizeof(vfostr), RIG_VFO_A);

    if (ret != RIG_OK) { return ret; }

    SNPRINTF(cmd, sizeof(cmd), "C%s %u\n", vfostr, tone);

    ret = netrigctl_transaction(rig, cmd, strlen(cmd), buf);

    if (ret > 0)
    {
        return -RIG_EPROTO;
    }
    else
    {
        return ret;
    }
}


static int netrigctl_get_ctcss_tone(RIG *rig, vfo_t vfo, tone_t *tone)
{
    int ret;
    char cmd[CMD_MAX];
    char buf[BUF_MAX];
    char vfostr[16] = "";

    rig_debug(RIG_DEBUG_VERBOSE, "%s called\n", __func__);

    ret = netrigctl_vfostr(rig, vfostr, sizeof(vfostr), RIG_VFO_A);

    if (ret != RIG_OK) { return ret; }

    SNPRINTF(cmd, sizeof(cmd), "c%s\n", vfostr);

    ret = netrigctl_transaction(rig, cmd, strlen(cmd), buf);

    if (ret <= 0)
    {
        return (ret < 0) ? ret : -RIG_EPROTO;
    }

    *tone = atoi(buf);

    return RIG_OK;
}


static int netrigctl_set_dcs_code(RIG *rig, vfo_t vfo, tone_t code)
{
    int ret;
    char cmd[CMD_MAX];
    char buf[BUF_MAX];
    char vfostr[16] = "";

    rig_debug(RIG_DEBUG_VERBOSE, "%s called\n", __func__);

    ret = netrigctl_vfostr(rig, vfostr, sizeof(vfostr), RIG_VFO_A);

    if (ret != RIG_OK) { return ret; }

    SNPRINTF(cmd, sizeof(cmd), "D%s %u\n", vfostr, code);

    ret = netrigctl_transaction(rig, cmd, strlen(cmd), buf);

    if (ret > 0)
    {
        return -RIG_EPROTO;
    }
    else
    {
        return ret;
    }
}


static int netrigctl_get_dcs_code(RIG *rig, vfo_t vfo, tone_t *code)
{
    int ret;
    char cmd[CMD_MAX];
    char buf[BUF_MAX];
    char vfostr[16] = "";

    rig_debug(RIG_DEBUG_VERBOSE, "%s called\n", __func__);

    ret = netrigctl_vfostr(rig, vfostr, sizeof(vfostr), RIG_VFO_A);

    if (ret != RIG_OK) { return ret; }

    SNPRINTF(cmd, sizeof(cmd), "d%s\n", vfostr);

    ret = netrigctl_transaction(rig, cmd, strlen(cmd), buf);

    if (ret <= 0)
    {
        return (ret < 0) ? ret : -RIG_EPROTO;
    }

    *code = atoi(buf);

    return RIG_OK;
}


static int netrigctl_set_ctcss_sql(RIG *rig, vfo_t vfo, tone_t tone)
{
    int ret;
    char cmd[CMD_MAX];
    char buf[BUF_MAX];
    char vfostr[16] = "";

    rig_debug(RIG_DEBUG_VERBOSE, "%s called\n", __func__);

    ret = netrigctl_vfostr(rig, vfostr, sizeof(vfostr), RIG_VFO_A);

    if (ret != RIG_OK) { return ret; }

    SNPRINTF(cmd, sizeof(cmd), "\\set_ctcss_sql%s %u\n", vfostr, tone);

    ret = netrigctl_transaction(rig, cmd, strlen(cmd), buf);

    if (ret > 0)
    {
        return -RIG_EPROTO;
    }
    else
    {
        return ret;
    }
}


static int netrigctl_get_ctcss_sql(RIG *rig, vfo_t vfo, tone_t *tone)
{
    int ret;
    char cmd[CMD_MAX];
    char buf[BUF_MAX];
    char vfostr[16] = "";

    rig_debug(RIG_DEBUG_VERBOSE, "%s called\n", __func__);

    ret = netrigctl_vfostr(rig, vfostr, sizeof(vfostr), RIG_VFO_A);

    if (ret != RIG_OK) { return ret; }

    SNPRINTF(cmd, sizeof(cmd), "\\get_ctcss_sql%s\n", vfostr);

    ret = netrigctl_transaction(rig, cmd, strlen(cmd), buf);

    if (ret <= 0)
    {
        return (ret < 0) ? ret : -RIG_EPROTO;
    }

    *tone = atoi(buf);

    return RIG_OK;
}


static int netrigctl_set_dcs_sql(RIG *rig, vfo_t vfo, unsigned int code)
{
    int ret;
    char cmd[CMD_MAX];
    char buf[BUF_MAX];
    char vfostr[16] = "";

    rig_debug(RIG_DEBUG_VERBOSE, "%s called\n", __func__);

    ret = netrigctl_vfostr(rig, vfostr, sizeof(vfostr), RIG_VFO_A);

    if (ret != RIG_OK) { return ret; }

    SNPRINTF(cmd, sizeof(cmd), "\\set_dcs_sql%s %u\n", vfostr, code);

    ret = netrigctl_transaction(rig, cmd, strlen(cmd), buf);

    if (ret > 0)
    {
        return -RIG_EPROTO;
    }
    else
    {
        return ret;
    }
}

static int netrigctl_get_dcs_sql(RIG *rig, vfo_t vfo, unsigned int *code)
{
    int ret;
    char cmd[CMD_MAX];
    char buf[BUF_MAX];
    char vfostr[16] = "";

    rig_debug(RIG_DEBUG_VERBOSE, "%s called\n", __func__);

    ret = netrigctl_vfostr(rig, vfostr, sizeof(vfostr), RIG_VFO_A);

    if (ret != RIG_OK) { return ret; }

    SNPRINTF(cmd, sizeof(cmd), "\\get_dcs_sql%s\n", vfostr);

    ret = netrigctl_transaction(rig, cmd, strlen(cmd), buf);

    if (ret <= 0)
    {
        return (ret < 0) ? ret : -RIG_EPROTO;
    }

    *code = atoi(buf);

    return RIG_OK;
}


static int netrigctl_set_split_freq(RIG *rig, vfo_t vfo, freq_t tx_freq)
{
    int ret;
    char cmd[CMD_MAX];
    char buf[BUF_MAX];
    char vfostr[16] = "";

    rig_debug(RIG_DEBUG_VERBOSE, "%s called\n", __func__);

    ret = netrigctl_vfostr(rig, vfostr, sizeof(vfostr), RIG_VFO_A);

    if (ret != RIG_OK) { return ret; }

    SNPRINTF(cmd, sizeof(cmd), "I%s %"FREQFMT"\n", vfostr, tx_freq);

    ret = netrigctl_transaction(rig, cmd, strlen(cmd), buf);

    if (ret > 0)
    {
        return -RIG_EPROTO;
    }
    else
    {
        return ret;
    }
}


static int netrigctl_get_split_freq(RIG *rig, vfo_t vfo, freq_t *tx_freq)
{
    int ret;
    char cmd[CMD_MAX];
    char buf[BUF_MAX];
    char vfostr[16] = "";

    rig_debug(RIG_DEBUG_VERBOSE, "%s called\n", __func__);

    ret = netrigctl_vfostr(rig, vfostr, sizeof(vfostr), RIG_VFO_A);

    if (ret != RIG_OK) { return ret; }

    SNPRINTF(cmd, sizeof(cmd), "i%s\n", vfostr);

    ret = netrigctl_transaction(rig, cmd, strlen(cmd), buf);

    if (ret <= 0)
    {
        return (ret < 0) ? ret : -RIG_EPROTO;
    }

    CHKSCN1ARG(num_sscanf(buf, "%"SCNfreq, tx_freq));

    return RIG_OK;
}

static int netrigctl_set_split_mode(RIG *rig, vfo_t vfo, rmode_t tx_mode,
                                    pbwidth_t tx_width)
{
    int ret;
    char cmd[CMD_MAX];
    char buf[BUF_MAX];
    char vfostr[16] = "";

    rig_debug(RIG_DEBUG_VERBOSE, "%s called\n", __func__);

    ret = netrigctl_vfostr(rig, vfostr, sizeof(vfostr), RIG_VFO_A);

    if (ret != RIG_OK) { return ret; }

    SNPRINTF(cmd, sizeof(cmd), "X%s %s %li\n",
             vfostr, rig_strrmode(tx_mode), tx_width);

    ret = netrigctl_transaction(rig, cmd, strlen(cmd), buf);

    if (ret > 0)
    {
        return -RIG_EPROTO;
    }
    else
    {
        return ret;
    }
}

static int netrigctl_get_split_mode(RIG *rig, vfo_t vfo, rmode_t *tx_mode,
                                    pbwidth_t *tx_width)
{
    int ret;
    char cmd[CMD_MAX];
    char buf[BUF_MAX];
    char vfostr[16] = "";

    rig_debug(RIG_DEBUG_VERBOSE, "%s called\n", __func__);

    ret = netrigctl_vfostr(rig, vfostr, sizeof(vfostr), RIG_VFO_A);

    if (ret != RIG_OK) { return ret; }

    SNPRINTF(cmd, sizeof(cmd), "x%s\n", vfostr);

    ret = netrigctl_transaction(rig, cmd, strlen(cmd), buf);

    if (ret <= 0)
    {
        return (ret < 0) ? ret : -RIG_EPROTO;
    }

    if (buf[ret - 1] == '\n') { buf[ret - 1] = '\0'; } /* chomp */

    *tx_mode = rig_parse_mode(buf);

    ret = read_string(&rig->state.rigport, (unsigned char *) buf, BUF_MAX, "\n", 1,
                      0, 1);

    if (ret <= 0)
    {
        return (ret < 0) ? ret : -RIG_EPROTO;
    }

    *tx_width = atoi(buf);

    return RIG_OK;
}

static int netrigctl_set_split_vfo(RIG *rig, vfo_t vfo, split_t split,
                                   vfo_t tx_vfo)
{
    int ret;
    char cmd[CMD_MAX];
    char buf[BUF_MAX];
    char vfostr[16] = "";

    rig_debug(RIG_DEBUG_VERBOSE, "%s called vfo=%s, vfotx=%s, split=%d\n", __func__,
              rig_strvfo(vfo), rig_strvfo(tx_vfo), split);

    ret = netrigctl_vfostr(rig, vfostr, sizeof(vfostr), RIG_VFO_A);

    if (ret != RIG_OK) { return ret; }

    SNPRINTF(cmd, sizeof(cmd), "S%s %d %s\n", vfostr, split, rig_strvfo(tx_vfo));

    ret = netrigctl_transaction(rig, cmd, strlen(cmd), buf);

    if (ret > 0)
    {
        return -RIG_EPROTO;
    }
    else
    {
        return ret;
    }
}


static int netrigctl_get_split_vfo(RIG *rig, vfo_t vfo, split_t *split,
                                   vfo_t *tx_vfo)
{
    int ret;
    char cmd[CMD_MAX];
    char buf[BUF_MAX];
    char vfostr[16] = "";

    rig_debug(RIG_DEBUG_VERBOSE, "%s called\n", __func__);

    ret = netrigctl_vfostr(rig, vfostr, sizeof(vfostr), RIG_VFO_A);

    if (ret != RIG_OK) { return ret; }

    SNPRINTF(cmd, sizeof(cmd), "s%s\n", vfostr);

    ret = netrigctl_transaction(rig, cmd, strlen(cmd), buf);

    if (ret <= 0)
    {
        return (ret < 0) ? ret : -RIG_EPROTO;
    }

    *split = atoi(buf);

    ret = read_string(&rig->state.rigport, (unsigned char *) buf, BUF_MAX, "\n", 1,
                      0, 1);

    if (ret <= 0)
    {
        return (ret < 0) ? ret : -RIG_EPROTO;
    }


    if (buf[ret - 1] == '\n') { buf[ret - 1] = '\0'; } /* chomp */

    *tx_vfo = rig_parse_vfo(buf);

    return RIG_OK;
}

static int netrigctl_set_rit(RIG *rig, vfo_t vfo, shortfreq_t rit)
{
    int ret;
    char cmd[CMD_MAX];
    char buf[BUF_MAX];
    char vfostr[16] = "";

    rig_debug(RIG_DEBUG_VERBOSE, "%s called\n", __func__);

    ret = netrigctl_vfostr(rig, vfostr, sizeof(vfostr), vfo);

    if (ret != RIG_OK) { return ret; }

    SNPRINTF(cmd, sizeof(cmd), "J%s %ld\n", vfostr, rit);

    ret = netrigctl_transaction(rig, cmd, strlen(cmd), buf);

    if (ret > 0)
    {
        return -RIG_EPROTO;
    }
    else
    {
        return ret;
    }
}


static int netrigctl_get_rit(RIG *rig, vfo_t vfo, shortfreq_t *rit)
{
    int ret;
    char cmd[CMD_MAX];
    char buf[BUF_MAX];
    char vfostr[16] = "";

    rig_debug(RIG_DEBUG_VERBOSE, "%s called\n", __func__);

    ret = netrigctl_vfostr(rig, vfostr, sizeof(vfostr), vfo);

    if (ret != RIG_OK) { return ret; }

    SNPRINTF(cmd, sizeof(cmd), "j%s\n", vfostr);

    ret = netrigctl_transaction(rig, cmd, strlen(cmd), buf);

    if (ret <= 0)
    {
        return (ret < 0) ? ret : -RIG_EPROTO;
    }

    *rit = atoi(buf);

    return RIG_OK;
}


static int netrigctl_set_xit(RIG *rig, vfo_t vfo, shortfreq_t xit)
{
    int ret;
    char cmd[CMD_MAX];
    char buf[BUF_MAX];
    char vfostr[16] = "";

    rig_debug(RIG_DEBUG_VERBOSE, "%s called\n", __func__);

    ret = netrigctl_vfostr(rig, vfostr, sizeof(vfostr), vfo);

    if (ret != RIG_OK) { return ret; }

    SNPRINTF(cmd, sizeof(cmd), "Z%s %ld\n", vfostr, xit);

    ret = netrigctl_transaction(rig, cmd, strlen(cmd), buf);

    if (ret > 0)
    {
        return -RIG_EPROTO;
    }
    else
    {
        return ret;
    }
}


static int netrigctl_get_xit(RIG *rig, vfo_t vfo, shortfreq_t *xit)
{
    int ret;
    char cmd[CMD_MAX];
    char buf[BUF_MAX];
    char vfostr[16] = "";

    rig_debug(RIG_DEBUG_VERBOSE, "%s called\n", __func__);

    ret = netrigctl_vfostr(rig, vfostr, sizeof(vfostr), vfo);

    if (ret != RIG_OK) { return ret; }

    SNPRINTF(cmd, sizeof(cmd), "z%s\n", vfostr);

    ret = netrigctl_transaction(rig, cmd, strlen(cmd), buf);

    if (ret <= 0)
    {
        return (ret < 0) ? ret : -RIG_EPROTO;
    }

    *xit = atoi(buf);

    return RIG_OK;
}


static int netrigctl_set_ts(RIG *rig, vfo_t vfo, shortfreq_t ts)
{
    int ret;
    char cmd[CMD_MAX];
    char buf[BUF_MAX];
    char vfostr[16] = "";

    rig_debug(RIG_DEBUG_VERBOSE, "%s called\n", __func__);

    ret = netrigctl_vfostr(rig, vfostr, sizeof(vfostr), RIG_VFO_A);

    if (ret != RIG_OK) { return ret; }

    SNPRINTF(cmd, sizeof(cmd), "N%s %ld\n", vfostr, ts);

    ret = netrigctl_transaction(rig, cmd, strlen(cmd), buf);

    if (ret > 0)
    {
        return -RIG_EPROTO;
    }
    else
    {
        return ret;
    }
}


static int netrigctl_get_ts(RIG *rig, vfo_t vfo, shortfreq_t *ts)
{
    int ret;
    char cmd[CMD_MAX];
    char buf[BUF_MAX];
    char vfostr[16] = "";

    rig_debug(RIG_DEBUG_VERBOSE, "%s called\n", __func__);

    ret = netrigctl_vfostr(rig, vfostr, sizeof(vfostr), RIG_VFO_A);

    if (ret != RIG_OK) { return ret; }

    SNPRINTF(cmd, sizeof(cmd), "n%s\n", vfostr);

    ret = netrigctl_transaction(rig, cmd, strlen(cmd), buf);

    if (ret <= 0)
    {
        return (ret < 0) ? ret : -RIG_EPROTO;
    }

    *ts = atoi(buf);

    return RIG_OK;
}


static int netrigctl_set_func(RIG *rig, vfo_t vfo, setting_t func, int status)
{
    int ret;
    char cmd[CMD_MAX];
    char buf[BUF_MAX];
    char vfostr[16] = "";

    rig_debug(RIG_DEBUG_VERBOSE, "%s called\n", __func__);

    ret = netrigctl_vfostr(rig, vfostr, sizeof(vfostr), vfo);

    if (ret != RIG_OK) { return ret; }

    SNPRINTF(cmd, sizeof(cmd), "U%s %s %i\n", vfostr, rig_strfunc(func), status);

    ret = netrigctl_transaction(rig, cmd, strlen(cmd), buf);

    if (ret > 0)
    {
        return -RIG_EPROTO;
    }
    else
    {
        return ret;
    }
}


static int netrigctl_get_func(RIG *rig, vfo_t vfo, setting_t func, int *status)
{
    int ret;
    char cmd[CMD_MAX];
    char buf[BUF_MAX];
    char vfostr[16] = "";

    rig_debug(RIG_DEBUG_VERBOSE, "%s called\n", __func__);

    ret = netrigctl_vfostr(rig, vfostr, sizeof(vfostr), vfo);

    if (ret != RIG_OK) { return ret; }

    if (strlen(rig_strfunc(func)) == 0) { return -RIG_ENAVAIL; }

    SNPRINTF(cmd, sizeof(cmd), "u%s %s\n", vfostr, rig_strfunc(func));

    ret = netrigctl_transaction(rig, cmd, strlen(cmd), buf);

    if (ret <= 0)
    {
        return (ret < 0) ? ret : -RIG_EPROTO;
    }

    *status = atoi(buf);

    return RIG_OK;
}


static int netrigctl_set_level(RIG *rig, vfo_t vfo, setting_t level,
                               value_t val)
{
    int ret;
    char cmd[CMD_MAX];
    char buf[BUF_MAX];
    char lstr[32];
    char vfostr[16] = "";

    rig_debug(RIG_DEBUG_VERBOSE, "%s called\n", __func__);

    if (RIG_LEVEL_IS_FLOAT(level))
    {
        SNPRINTF(lstr, sizeof(lstr), "%f", val.f);
    }
    else
    {
        SNPRINTF(lstr, sizeof(lstr), "%d", val.i);
    }

    ret = netrigctl_vfostr(rig, vfostr, sizeof(vfostr), vfo);

    if (ret != RIG_OK) { return ret; }

    SNPRINTF(cmd, sizeof(cmd), "L%s %s %s\n", vfostr, rig_strlevel(level),
             lstr);

    ret = netrigctl_transaction(rig, cmd, strlen(cmd), buf);

    if (ret > 0)
    {
        return -RIG_EPROTO;
    }
    else
    {
        return ret;
    }

}


static int netrigctl_get_level(RIG *rig, vfo_t vfo, setting_t level,
                               value_t *val)
{
    int ret;
    char cmd[CMD_MAX];
    char buf[BUF_MAX];
    char vfostr[16] = "";

    rig_debug(RIG_DEBUG_VERBOSE, "%s called\n", __func__);

    ret = netrigctl_vfostr(rig, vfostr, sizeof(vfostr), vfo);

    if (ret != RIG_OK) { return ret; }

    SNPRINTF(cmd, sizeof(cmd), "l%s %s\n", vfostr, rig_strlevel(level));

    ret = netrigctl_transaction(rig, cmd, strlen(cmd), buf);

    if (ret <= 0)
    {
        return (ret < 0) ? ret : -RIG_EPROTO;
    }

    if (RIG_LEVEL_IS_FLOAT(level))
    {
        val->f = atof(buf);
    }
    else
    {
        val->i = atoi(buf);
    }

    return RIG_OK;
}


static int netrigctl_set_powerstat(RIG *rig, powerstat_t status)
{
    int ret;
    char cmd[CMD_MAX];
    char buf[BUF_MAX];

    rig_debug(RIG_DEBUG_VERBOSE, "%s called\n", __func__);

    SNPRINTF(cmd, sizeof(cmd), "\\set_powerstat %d\n", status);

    ret = netrigctl_transaction(rig, cmd, strlen(cmd), buf);

    if (ret > 0)
    {
        return -RIG_EPROTO;
    }
    else
    {
        return ret;
    }
}


static int netrigctl_get_powerstat(RIG *rig, powerstat_t *status)
{
    int ret;
    char cmd[CMD_MAX];
    char buf[BUF_MAX];

    rig_debug(RIG_DEBUG_VERBOSE, "%s called\n", __func__);

    SNPRINTF(cmd, sizeof(cmd), "\\get_powerstat\n");

    ret = netrigctl_transaction(rig, cmd, strlen(cmd), buf);

    if (ret == 0)
    {
        *status = atoi(buf);
    }
    else
    {
        // was causing problems with sdr++ since it does not have PS command
        // a return of 1 should indicate there is no powerstat command available
        // so we fake the ON status
        // also a problem with Flex 6xxx and Log4OM not working due to lack of PS command
        rig_debug(RIG_DEBUG_VERBOSE,
                  "%s: PS command failed (ret=%d) so returning RIG_POWER_ON\n", __func__, ret);
        *status = RIG_POWER_ON;
    }

    return RIG_OK; // always return RIG_OK
}


static int netrigctl_set_parm(RIG *rig, setting_t parm, value_t val)
{
    int ret;
    char cmd[CMD_MAX];
    char buf[BUF_MAX];
    char pstr[32];

    rig_debug(RIG_DEBUG_VERBOSE, "%s called\n", __func__);

    if (RIG_PARM_IS_FLOAT(parm))
    {
        SNPRINTF(pstr, sizeof(pstr), "%f", val.f);
    }
    else
    {
        SNPRINTF(pstr, sizeof(pstr), "%d", val.i);
    }

    SNPRINTF(cmd, sizeof(cmd), "P %s %s\n", rig_strparm(parm), pstr);

    ret = netrigctl_transaction(rig, cmd, strlen(cmd), buf);

    if (ret > 0)
    {
        return -RIG_EPROTO;
    }
    else
    {
        return ret;
    }
}


static int netrigctl_get_parm(RIG *rig, setting_t parm, value_t *val)
{
    int ret;
    char cmd[CMD_MAX];
    char buf[BUF_MAX];

    rig_debug(RIG_DEBUG_VERBOSE, "%s called\n", __func__);

    SNPRINTF(cmd, sizeof(cmd), "p %s\n", rig_strparm(parm));

    ret = netrigctl_transaction(rig, cmd, strlen(cmd), buf);

    if (ret <= 0)
    {
        return (ret < 0) ? ret : -RIG_EPROTO;
    }

    if (RIG_PARM_IS_FLOAT(parm))
    {
        val->f = atoi(buf);
    }
    else
    {
        val->i = atoi(buf);
    }

    return RIG_OK;
}


static int netrigctl_set_ant(RIG *rig, vfo_t vfo, ant_t ant, value_t option)
{
    int ret;
    char cmd[CMD_MAX];
    char buf[BUF_MAX];
    char vfostr[16] = "";
    int i_ant = 0;

    rig_debug(RIG_DEBUG_VERBOSE, "%s called, ant=0x%02x, option=%d\n", __func__,
              ant, option.i);

    switch (ant)
    {
    case RIG_ANT_1: i_ant = 0; break;

    case RIG_ANT_2: i_ant = 1; break;

    case RIG_ANT_3: i_ant = 2; break;

    case RIG_ANT_4: i_ant = 3; break;

    default:
        rig_debug(RIG_DEBUG_ERR, "%s: more than 4 antennas? ant=0x%02x\n", __func__,
                  ant);
    }

    ret = netrigctl_vfostr(rig, vfostr, sizeof(vfostr), vfo);

    if (ret != RIG_OK) { return ret; }

    SNPRINTF(cmd, sizeof(cmd), "Y%s %d %d\n", vfostr, i_ant, option.i);

    ret = netrigctl_transaction(rig, cmd, strlen(cmd), buf);

    if (ret > 0)
    {
        return -RIG_EPROTO;
    }
    else
    {
        return ret;
    }
}


static int netrigctl_get_ant(RIG *rig, vfo_t vfo, ant_t ant, value_t *option,
                             ant_t *ant_curr, ant_t *ant_tx, ant_t *ant_rx)
{
    int ret;
    char cmd[CMD_MAX];
    char buf[BUF_MAX];
    char vfostr[16] = "";

    rig_debug(RIG_DEBUG_VERBOSE, "%s called\n", __func__);

    ret = netrigctl_vfostr(rig, vfostr, sizeof(vfostr), vfo);

    if (ret != RIG_OK) { return ret; }

    if (ant == RIG_ANT_CURR)
    {
        SNPRINTF(cmd, sizeof(cmd), "y%s\n", vfostr);
    }
    else
    {
        SNPRINTF(cmd, sizeof(cmd), "y%s %u\n", vfostr, ant);
    }

    ret = netrigctl_transaction(rig, cmd, strlen(cmd), buf);

    if (ret <= 0)
    {
        return (ret < 0) ? ret : -RIG_EPROTO;
    }

    rig_debug(RIG_DEBUG_TRACE, "%s: buf='%s'\n", __func__, buf);
    ret = sscanf(buf, "%u\n", ant_curr);

    if (ret != 1)
    {
        rig_debug(RIG_DEBUG_ERR, "%s: expected 1 ant integer in '%s', got %d\n",
                  __func__, buf,
                  ret);
    }

    if (ant != RIG_ANT_CURR)
    {
        ret = sscanf(buf, "%d\n", &option->i);
    }

    if (ret != 1)
    {
        rig_debug(RIG_DEBUG_ERR, "%s: expected 1 option integer in '%s', got %d\n",
                  __func__, buf,
                  ret);
    }

    ret = read_string(&rig->state.rigport, (unsigned char *) buf, BUF_MAX, "\n", 1,
                      0, 1);

    if (ret <= 0)
    {
        return (ret < 0) ? ret : -RIG_EPROTO;
    }

    ret = sscanf(buf, "%d\n", &(option->i));

    if (ret != 1)
    {
        rig_debug(RIG_DEBUG_ERR, "%s: expected 1 option integer in '%s', got %d\n",
                  __func__, buf,
                  ret);
    }


    return RIG_OK;
}


static int netrigctl_set_bank(RIG *rig, vfo_t vfo, int bank)
{
    int ret;
    char cmd[CMD_MAX];
    char buf[BUF_MAX];

    rig_debug(RIG_DEBUG_VERBOSE, "%s called\n", __func__);

    SNPRINTF(cmd, sizeof(cmd), "B %d\n", bank);

    ret = netrigctl_transaction(rig, cmd, strlen(cmd), buf);

    if (ret > 0)
    {
        return -RIG_EPROTO;
    }
    else
    {
        return ret;
    }
}


static int netrigctl_set_mem(RIG *rig, vfo_t vfo, int ch)
{
    int ret;
    char cmd[CMD_MAX];
    char buf[BUF_MAX];
    char vfostr[16] = "";

    rig_debug(RIG_DEBUG_VERBOSE, "%s called\n", __func__);

    ret = netrigctl_vfostr(rig, vfostr, sizeof(vfostr), vfo);

    if (ret != RIG_OK) { return ret; }

    SNPRINTF(cmd, sizeof(cmd), "E%s %d\n", vfostr, ch);

    ret = netrigctl_transaction(rig, cmd, strlen(cmd), buf);

    if (ret > 0)
    {
        return -RIG_EPROTO;
    }
    else
    {
        return ret;
    }
}


static int netrigctl_get_mem(RIG *rig, vfo_t vfo, int *ch)
{
    int ret;
    char cmd[CMD_MAX];
    char buf[BUF_MAX];
    char vfostr[16] = "";

    rig_debug(RIG_DEBUG_VERBOSE, "%s called\n", __func__);

    ret = netrigctl_vfostr(rig, vfostr, sizeof(vfostr), vfo);

    if (ret != RIG_OK) { return ret; }

    SNPRINTF(cmd, sizeof(cmd), "e %s\n", vfostr);

    ret = netrigctl_transaction(rig, cmd, strlen(cmd), buf);

    if (ret <= 0)
    {
        return (ret < 0) ? ret : -RIG_EPROTO;
    }

    *ch = atoi(buf);

    return RIG_OK;
}

static int netrigctl_scan(RIG *rig, vfo_t vfo, scan_t scan, int ch)
{
    int ret;
    char cmd[CMD_MAX];
    char buf[BUF_MAX];

    rig_debug(RIG_DEBUG_VERBOSE, "%s called\n", __func__);

    SNPRINTF(cmd, sizeof(cmd), "g %s %d\n", rig_strscan(scan), ch);

    ret = netrigctl_transaction(rig, cmd, strlen(cmd), buf);

    if (ret > 0)
    {
        return -RIG_EPROTO;
    }
    else
    {
        return ret;
    }
}

static int netrigctl_vfo_op(RIG *rig, vfo_t vfo, vfo_op_t op)
{
    int ret;
    char cmd[CMD_MAX];
    char buf[BUF_MAX];
    char vfostr[16] = "";

    rig_debug(RIG_DEBUG_VERBOSE, "%s called\n", __func__);

    ret = netrigctl_vfostr(rig, vfostr, sizeof(vfostr), vfo);

    if (ret != RIG_OK) { return ret; }

    SNPRINTF(cmd, sizeof(cmd), "G%s %s\n", vfostr, rig_strvfop(op));

    ret = netrigctl_transaction(rig, cmd, strlen(cmd), buf);

    if (ret > 0)
    {
        return -RIG_EPROTO;
    }
    else
    {
        return ret;
    }
}

static int netrigctl_set_channel(RIG *rig, vfo_t vfo, const channel_t *chan)
{
    return -RIG_ENIMPL;
}


static int netrigctl_get_channel(RIG *rig, vfo_t vfo, channel_t *chan,
                                 int read_only)
{
    return -RIG_ENIMPL;
}


static const char *netrigctl_get_info(RIG *rig)
{
    int ret;
    char cmd[CMD_MAX];
    static char buf[BUF_MAX];

    rig_debug(RIG_DEBUG_VERBOSE, "%s called\n", __func__);

    SNPRINTF(cmd, sizeof(cmd), "_\n");

    ret = netrigctl_transaction(rig, cmd, strlen(cmd), buf);

    if (ret < 0)
    {
        return NULL;
    }

    buf [ret] = '\0';

    return buf;
}



static int netrigctl_send_dtmf(RIG *rig, vfo_t vfo, const char *digits)
{
    int ret, len;
    char *cmdp, cmd[] = "\\send_dtmf ";
    char buf[BUF_MAX];

    rig_debug(RIG_DEBUG_VERBOSE, "%s called\n", __func__);

    // allocate memory for size of (cmd + digits + \n + \0)
    len = strlen(cmd) + strlen(digits) + 2;
    cmdp = calloc(1, len);

    if (cmdp == NULL)
    {
        return -RIG_ENOMEM;
    }

    SNPRINTF(cmdp, len, "%s%s\n", cmd, digits);

    ret = netrigctl_transaction(rig, cmdp, strlen(cmdp), buf);
    free(cmdp);

    if (ret > 0)
    {
        return -RIG_EPROTO;
    }
    else
    {
        return ret;
    }
}

static int netrigctl_recv_dtmf(RIG *rig, vfo_t vfo, char *digits, int *length)
{
    int ret;
    char cmd[CMD_MAX];
    static char buf[BUF_MAX];

    rig_debug(RIG_DEBUG_VERBOSE, "%s called\n", __func__);

    SNPRINTF(cmd, sizeof(cmd), "\\recv_dtmf\n");

    ret = netrigctl_transaction(rig, cmd, strlen(cmd), buf);

    if (ret <= 0)
    {
        return (ret < 0) ? ret : -RIG_EPROTO;
    }

    if (ret > *length)
    {
        ret = *length;
    }

    strncpy(digits, buf, ret);
    *length = ret;
    digits [ret] = '\0';

    return RIG_OK;
}

static int netrigctl_send_voice_mem(RIG *rig, vfo_t vfo, int ch)
{
    int ret;
    char cmd[CMD_MAX];
    char buf[BUF_MAX];

    rig_debug(RIG_DEBUG_VERBOSE, "%s called\n", __func__);

    SNPRINTF(cmd, sizeof(cmd), "\\send_voice_mem %d\n", ch);

    ret = netrigctl_transaction(rig, cmd, strlen(cmd), buf);

    if (ret > 0)
    {
        return -RIG_EPROTO;
    }
    else
    {
        return ret;
    }
}

static int netrigctl_send_morse(RIG *rig, vfo_t vfo, const char *msg)
{
    int ret, len;
    char *cmdp, cmd[] = "\\send_morse ";
    char buf[BUF_MAX];

    rig_debug(RIG_DEBUG_VERBOSE, "%s called\n", __func__);

    // allocate memory for size of (cmd + msg + \n + \0)
    len = strlen(cmd) + strlen(msg) + 2;
    cmdp = calloc(1, len);

    if (cmdp == NULL)
    {
        return -RIG_ENOMEM;
    }

    SNPRINTF(cmdp, len, "%s%s\n", cmd, msg);

    ret = netrigctl_transaction(rig, cmdp, strlen(cmdp), buf);
    free(cmdp);

    if (ret > 0)
    {
        return -RIG_EPROTO;
    }
    else
    {
        return ret;
    }
}

static int netrigctl_stop_morse(RIG *rig, vfo_t vfo)
{
    int ret;
    char cmd[] = "\\stop_morse\n";
    char buf[BUF_MAX];

    rig_debug(RIG_DEBUG_VERBOSE, "%s called\n", __func__);

    ret = netrigctl_transaction(rig, cmd, strlen(cmd), buf);

    if (ret > 0)
    {
        return -RIG_EPROTO;
    }
    else
    {
        return ret;
    }
}

static int netrigctl_set_vfo_opt(RIG *rig, int status)
{
    char cmdbuf[32];
    char buf[BUF_MAX];
    int ret;

    SNPRINTF(cmdbuf, sizeof(cmdbuf), "\\set_vfo_opt %d\n", status);
    ret = netrigctl_transaction(rig, cmdbuf, strlen(cmdbuf), buf);

    if (ret > 0)
    {
        return -RIG_EPROTO;
    }

    rig->state.vfo_opt = status;
    return RIG_OK;
}

#if 0 // for the futurem -- would have to poll to get the pushed data
static int netrigctl_set_trn(RIG *rig, int trn)
{
    char cmdbuf[32];
    char buf[BUF_MAX];
    int ret;

    SNPRINTF(cmdbuf, sizeof(cmdbuf), "\\set_trn %s\n", trn ? "ON" : "OFF");
    ret = netrigctl_transaction(rig, cmdbuf, strlen(cmdbuf), buf);

    if (ret < 0)
    {
        return -RIG_EPROTO;
    }

    return RIG_OK;
}


static int netrigctl_get_trn(RIG *rig, int *trn)
{
    char cmdbuf[32];
    char buf[BUF_MAX];
    int ret;

    ENTERFUNC;
    SNPRINTF(cmdbuf, sizeof(cmdbuf), "\\get_trn\n");
    ret = netrigctl_transaction(rig, cmdbuf, strlen(cmdbuf), buf);

    if (ret <= 0)
    {
        return -RIG_EPROTO;
    }

    if (strstr(buf, "OFF")) { *trn = RIG_TRN_OFF; }
    else if (strstr(buf, "RIG")) { *trn = RIG_TRN_RIG; }
    else if (strstr(buf, "POLL")) { *trn = RIG_TRN_POLL; }
    else
    {
        rig_debug(RIG_DEBUG_ERR, "%s: Expected OFF, RIG, or POLL, got '%s'\n", __func__,
                  buf);
        ret = -RIG_EINVAL;
    }

    RETURNFUNC(ret);
}
#endif

static int netrigctl_mW2power(RIG *rig, float *power, unsigned int mwpower,
                              freq_t freq, rmode_t mode)
{
    char cmdbuf[32];
    char buf[BUF_MAX];
    int ret;

    ENTERFUNC;

    SNPRINTF(cmdbuf, sizeof(cmdbuf), "\\mW2power %u %.0f %s\n", mwpower, freq,
             rig_strrmode(mode));
    ret = netrigctl_transaction(rig, cmdbuf, strlen(cmdbuf), buf);

    if (ret <= 0)
    {
        return -RIG_EPROTO;
    }

    *power = atof(buf);

    RETURNFUNC(RIG_OK);
}


static int netrigctl_power2mW(RIG *rig, unsigned int *mwpower, float power,
                              freq_t freq, rmode_t mode)
{
    char cmdbuf[64];
    char buf[BUF_MAX];
    int ret;

    ENTERFUNC;

    // we shouldn't need any precision than microwatts
    SNPRINTF(cmdbuf, sizeof(cmdbuf), "\\power2mW %.3f %.0f %s\n", power, freq,
             rig_strrmode(mode));
    ret = netrigctl_transaction(rig, cmdbuf, strlen(cmdbuf), buf);

    if (ret <= 0)
    {
        return -RIG_EPROTO;
    }

    *mwpower = atof(buf);

    RETURNFUNC(RIG_OK);
}

int netrigctl_password(RIG *rig, const char *key1)
{
    char cmdbuf[256];
    char buf[BUF_MAX];
    int retval;

    ENTERFUNC;
    rig_debug(RIG_DEBUG_VERBOSE, "%s: key1=%s\n", __func__, key1);
    SNPRINTF(cmdbuf, sizeof(cmdbuf), "\\password %s\n", key1);
    retval = netrigctl_transaction(rig, cmdbuf, strlen(cmdbuf), buf);

    if (retval != RIG_OK) { retval = -RIG_EPROTO; }

    RETURNFUNC(retval);
}

int netrigctl_set_lock_mode(RIG *rig, int lock)
{
    char cmdbuf[256];
    char buf[BUF_MAX];
    int ret;

    SNPRINTF(cmdbuf, sizeof(cmdbuf), "\\set_lock_mode %d\n", lock);

    ret = netrigctl_transaction(rig, cmdbuf, strlen(cmdbuf), buf);

    if (ret > 0)
    {
        return -RIG_EPROTO;
    }

    return (RIG_OK);
}

int netrigctl_get_lock_mode(RIG *rig, int *lock)
{
    char cmdbuf[256];
    char buf[BUF_MAX];
    int ret;
    SNPRINTF(cmdbuf, sizeof(cmdbuf), "\\get_lock_mode\n");
    ret = netrigctl_transaction(rig, cmdbuf, strlen(cmdbuf), buf);

    if (ret == 0)
    {
        return -RIG_EPROTO;
    }

    sscanf(buf, "%d", lock);
    return (RIG_OK);
}

int netrigctl_send_raw(RIG *rig, char *s)
{
    int ret;
    char buf[BUF_MAX];
    ret = netrigctl_transaction(rig, s, strlen(s), buf);
    return ret;
}

/*
 * Netrigctl rig capabilities.
 */

struct rig_caps netrigctl_caps =
{
    RIG_MODEL(RIG_MODEL_NETRIGCTL),
    .model_name =     "NET rigctl",
    .mfg_name =       "Hamlib",
    .version =        "20230106.0",
    .copyright =      "LGPL",
    .status =         RIG_STATUS_STABLE,
    .rig_type =       RIG_TYPE_OTHER,
    .targetable_vfo =      0,
    .ptt_type =       RIG_PTT_RIG_MICDATA,
    .dcd_type =       RIG_DCD_RIG,
    .port_type =      RIG_PORT_NETWORK,
    .timeout = 10000,  /* enough for the worst rig we have */
    .retry =   5,

    /* following fields updated in rig_state at opening time */
    .has_get_func =   RIG_FUNC_NONE,
    .has_set_func =   RIG_FUNC_NONE,
    .has_get_level =  RIG_LEVEL_NONE,
    .has_set_level =  RIG_LEVEL_NONE,
    .has_get_parm =    RIG_PARM_NONE,
    .has_set_parm =    RIG_PARM_NONE,

    .level_gran =      { },
    .ctcss_list =      NULL,
    .dcs_list =        NULL,
    .chan_list =   { },
    .transceive =     RIG_TRN_OFF,
    .attenuator =     { },
    .preamp =          { },
    .rx_range_list2 =  { RIG_FRNG_END, },
    .tx_range_list2 =  { RIG_FRNG_END, },
    .tuning_steps =  { },
    .filters =  { RIG_FLT_END, },
    .max_rit = 0,
    .max_xit = 0,
    .max_ifshift = 0,
    .priv =  NULL,

    .rig_init =     netrigctl_init,
    .rig_cleanup =  netrigctl_cleanup,
    .rig_open =     netrigctl_open,
    .rig_close =    netrigctl_close,

    .set_freq =     netrigctl_set_freq,
    .get_freq =     netrigctl_get_freq,
    .set_mode =     netrigctl_set_mode,
    .get_mode =     netrigctl_get_mode,
    .set_vfo =      netrigctl_set_vfo,
    .get_vfo =      netrigctl_get_vfo,

    .set_powerstat =  netrigctl_set_powerstat,
    .get_powerstat =  netrigctl_get_powerstat,
    .set_level =     netrigctl_set_level,
    .get_level =     netrigctl_get_level,
    .set_func =      netrigctl_set_func,
    .get_func =      netrigctl_get_func,
    .set_parm =      netrigctl_set_parm,
    .get_parm =      netrigctl_get_parm,

    .get_info =      netrigctl_get_info,


    .set_ptt =    netrigctl_set_ptt,
    .get_ptt =    netrigctl_get_ptt,
    .get_dcd =    netrigctl_get_dcd,
    .set_rptr_shift =     netrigctl_set_rptr_shift,
    .get_rptr_shift =     netrigctl_get_rptr_shift,
    .set_rptr_offs =  netrigctl_set_rptr_offs,
    .get_rptr_offs =  netrigctl_get_rptr_offs,
    .set_ctcss_tone =     netrigctl_set_ctcss_tone,
    .get_ctcss_tone =     netrigctl_get_ctcss_tone,
    .set_dcs_code =   netrigctl_set_dcs_code,
    .get_dcs_code =   netrigctl_get_dcs_code,
    .set_ctcss_sql =  netrigctl_set_ctcss_sql,
    .get_ctcss_sql =  netrigctl_get_ctcss_sql,
    .set_dcs_sql =    netrigctl_set_dcs_sql,
    .get_dcs_sql =    netrigctl_get_dcs_sql,
    .set_split_freq =     netrigctl_set_split_freq,
    .get_split_freq =     netrigctl_get_split_freq,
    .set_split_mode =     netrigctl_set_split_mode,
    .get_split_mode =     netrigctl_get_split_mode,
    .set_split_vfo =  netrigctl_set_split_vfo,
    .get_split_vfo =  netrigctl_get_split_vfo,
    .set_rit =    netrigctl_set_rit,
    .get_rit =    netrigctl_get_rit,
    .set_xit =    netrigctl_set_xit,
    .get_xit =    netrigctl_get_xit,
    .set_ts =     netrigctl_set_ts,
    .get_ts =     netrigctl_get_ts,
    .set_ant =    netrigctl_set_ant,
    .get_ant =    netrigctl_get_ant,
    .set_bank =   netrigctl_set_bank,
    .set_mem =    netrigctl_set_mem,
    .get_mem =    netrigctl_get_mem,
    .vfo_op =     netrigctl_vfo_op,
    .scan =       netrigctl_scan,
    .send_dtmf =  netrigctl_send_dtmf,
    .recv_dtmf =  netrigctl_recv_dtmf,
    .send_morse =  netrigctl_send_morse,
    .send_voice_mem =  netrigctl_send_voice_mem,
    .stop_morse =  netrigctl_stop_morse,
    .set_channel =    netrigctl_set_channel,
    .get_channel =    netrigctl_get_channel,
    .set_vfo_opt = netrigctl_set_vfo_opt,
    //.set_trn =    netrigctl_set_trn,
    //.get_trn =    netrigctl_get_trn,
    .power2mW =   netrigctl_power2mW,
    .mW2power =   netrigctl_mW2power,
    .password =   netrigctl_password,
    .set_lock_mode = netrigctl_set_lock_mode,
    .get_lock_mode = netrigctl_get_lock_mode,

    .hamlib_check_rig_caps = HAMLIB_CHECK_RIG_CAPS
};
