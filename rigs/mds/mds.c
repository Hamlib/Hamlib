/*
 *  Hamlib MDS 4710/9710 backend - main file
 *  Copyright (c) 2022 by Michael Black W9MDB
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
#include <string.h>

#include <hamlib/rig.h>
#include "serial.h"
#include "misc.h"
#include "cal.h"
#include "token.h"
#include "register.h"

#include "mds.h"

#define MAXCMDLEN 32

#define MDS_VFOS (RIG_VFO_A)

#define MDS_MODES (RIG_MODE_NONE)

#define MDS_LEVELS (RIG_LEVEL_NONE)


const struct rig_caps mds_caps;

DECLARE_INITRIG_BACKEND(mds)
{
    rig_debug(RIG_DEBUG_VERBOSE, "%s: _init called\n", __func__);

    rig_register(&mds_caps);
    rig_debug(RIG_DEBUG_VERBOSE, "%s: _init back from rig_register\n", __func__);

    return RIG_OK;
}

int mds_transaction(RIG *rig, char *cmd, int expected, char **result)
{
    char cmd_buf[MAXCMDLEN];
    int retval;
    struct rig_state *rs = &rig->state;
    struct mds_priv_data *priv = rig->state.priv;

    rig_debug(RIG_DEBUG_VERBOSE, "%s: cmd=%s\n", __func__, cmd);

    SNPRINTF(cmd_buf, sizeof(cmd_buf), "%s\n", cmd);

    rig_flush(&rs->rigport);
    retval = write_block(&rs->rigport, (unsigned char *) cmd_buf, strlen(cmd_buf));

    if (retval < 0)
    {
        return retval;
    }

    if (expected == 0)
    {
        return RIG_OK;
    }
    else
    {
        char cmdtrm_str[2];   /* Default Command/Reply termination char */
        cmdtrm_str[0] = 0x0d;
        cmdtrm_str[1] = 0x00;
        retval = read_string(&rs->rigport, (unsigned char *) priv->ret_data,
                             sizeof(priv->ret_data), cmdtrm_str, strlen(cmdtrm_str), 0, expected);

        if (retval < 0)
        {
            rig_debug(RIG_DEBUG_ERR, "%s(%d): error in read_block\n", __func__, __LINE__);
            return retval;
        }
    }

    if (result != NULL)
    {
        rig_debug(RIG_DEBUG_VERBOSE, "%s: setting result\n", __func__);
        *result = &(priv->ret_data[0]);
    }
    else
    {
        rig_debug(RIG_DEBUG_VERBOSE, "%s: no result requested\n", __func__);
    }

    return RIG_OK;
}

int mds_init(RIG *rig)
{
    rig_debug(RIG_DEBUG_VERBOSE, "%s version %s\n", __func__, rig->caps->version);
    // cppcheck claims leak here but it's freed in cleanup
    rig->state.priv = (struct mds_priv_data *)calloc(1,
                      sizeof(struct mds_priv_data));

    if (!rig->state.priv)
    {
        return -RIG_ENOMEM;
    }

    return RIG_OK;
}

/*
 * mds_cleanup
 *
 */

int mds_cleanup(RIG *rig)
{

    rig_debug(RIG_DEBUG_VERBOSE, "%s called\n", __func__);

    if (!rig)
    {
        return -RIG_EINVAL;
    }

    if (rig->state.priv)
    {
        free(rig->state.priv);
    }

    rig->state.priv = NULL;

    return RIG_OK;
}

/*
 * mds_get_freq
 * Assumes rig!=NULL, rig->state.priv!=NULL, freq!=NULL
 */
int mds_get_freq(RIG *rig, vfo_t vfo, freq_t *freq)
{
    int retval;
    char *response = NULL;

    rig_debug(RIG_DEBUG_VERBOSE, "%s: vfo=%s\n", __func__, rig_strvfo(vfo));
    *freq = 0;

    retval = mds_transaction(rig, "TX", 16, &response);

    if (retval != RIG_OK)
    {
        rig_debug(RIG_DEBUG_ERR, "%s: invalid response=%s\n", __func__, response);
        return retval;
    }

    retval = sscanf(response, "%lg", freq);

    if (retval != 1)
    {
        rig_debug(RIG_DEBUG_ERR, "%s: Unable to parse response\n", __func__);
        return -RIG_EPROTO;
    }

    return RIG_OK;
}

// TC command does not work on 4050 -- not implemented as of 2022-01-12
/*
 * mds_set_freq
 * assumes rig!=NULL, rig->state.priv!=NULL
 */
int mds_set_freq(RIG *rig, vfo_t vfo, freq_t freq)
{
    char cmd_buf[MAXCMDLEN];
    int retval;
    freq_t tfreq;

    rig_debug(RIG_DEBUG_VERBOSE, "%s: vfo=%s freq=%.0f\n", __func__,
              rig_strvfo(vfo), freq);

    retval = rig_get_freq(rig, vfo, &tfreq);

    if (retval != RIG_OK)
    {
        rig_debug(RIG_DEBUG_VERBOSE, "%s: get_freq failed: %s\n", __func__,
                  strerror(retval));
        return retval;
    }

    if (tfreq == freq)
    {
        rig_debug(RIG_DEBUG_VERBOSE, "%s: freq not changing\n", __func__);
        return RIG_OK;
    }

    // If we are not explicitly asking for VFO_B then we'll set the receive side also
    if (vfo != RIG_VFO_B)
    {
        char *response = NULL;
        SNPRINTF((char *) cmd_buf, sizeof(cmd_buf), "TX%.4f", freq / 1e6);
        retval = mds_transaction(rig, cmd_buf, 0, &response);

        if (retval < 0)
        {
            rig_debug(RIG_DEBUG_ERR, "%s: TX failed\n", __func__);
            return retval;
        }

        SNPRINTF((char *) cmd_buf, sizeof(cmd_buf), "RX%.4f", freq / 1e6);
        retval = mds_transaction(rig, cmd_buf, 0, &response);

        if (retval < 0)
        {
            rig_debug(RIG_DEBUG_ERR, "%s: RX failed\n", __func__);
            return retval;
        }
    }

    return RIG_OK;
}

/*
 * mds_set_ptt
 * Assumes rig!=NULL
 */
int mds_set_ptt(RIG *rig, vfo_t vfo, ptt_t ptt)
{
    int retval;
    char cmd_buf[MAXCMDLEN];
    char *response = NULL;

    rig_debug(RIG_DEBUG_VERBOSE, "%s: ptt=%d\n", __func__, ptt);

    SNPRINTF(cmd_buf, sizeof(cmd_buf), "%s", ptt ? "KEY" : "DKEY");
    retval = mds_transaction(rig, cmd_buf, 0, &response);

    if (retval < 0)
    {
        rig_debug(RIG_DEBUG_ERR, "%s: invalid response=%s\n", __func__, response);
        return retval;
    }

    if (strncmp(response, "OK", 2) != 0)
    {
        rig_debug(RIG_DEBUG_ERR, "%s: Expected OK, got '%s'\n", __func__, response);
        return -RIG_EINVAL;
    }

    rig_debug(RIG_DEBUG_VERBOSE, "%s: cmd:IP result=%s\n", __func__, response);

    return RIG_OK;
}

/*
 * mds_get_ptt
 * Assumes rig!=NULL
 */
int mds_get_ptt(RIG *rig, vfo_t vfo, ptt_t *ptt)
{
    int retval;
    char *response = NULL;
    char c;

    rig_debug(RIG_DEBUG_VERBOSE, "%s: vfo=%s\n", __func__, rig_strvfo(vfo));

    retval = mds_transaction(rig, "IP", 0, &response);

    if (retval != RIG_OK)
    {
        rig_debug(RIG_DEBUG_ERR, "%s: error response?='%s'\n", __func__, response);
        return retval;
    }

    c = response[0];

    if (c == '1' || c == '0')
    {
        *ptt = c - '0';
    }
    else
    {
        rig_debug(RIG_DEBUG_ERR, "%s: error response='%s'\n", __func__, response);
        return -RIG_EPROTO;
    }

    return RIG_OK;
}

/*
 * mds_set_mode
 * Assumes rig!=NULL
 * Note that 2050 does not have set or get width
 */
int mds_set_mode(RIG *rig, vfo_t vfo, rmode_t mode, pbwidth_t width)
{
    char cmd_buf[32], ttmode;
    int retval;
    rmode_t tmode;
    pbwidth_t twidth;

    //struct tt588_priv_data *priv = (struct tt588_priv_data *) rig->state.priv;

    rig_debug(RIG_DEBUG_VERBOSE, "%s: vfo=%s mode=%s width=%d\n", __func__,
              rig_strvfo(vfo), rig_strrmode(mode), (int)width);

    retval = rig_get_mode(rig, vfo, &tmode, &twidth);

    if (retval != RIG_OK)
    {
        rig_debug(RIG_DEBUG_ERR, "%s: get_mode failed %s\n", __func__,
                  strerror(retval));
    }

    if (tmode == mode)
    {
        rig_debug(RIG_DEBUG_VERBOSE, "%s: already mode %s so not changing\n", __func__,
                  rig_strrmode(mode));
        return RIG_OK;
    }

    switch (mode)
    {
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

    case RIG_MODE_RTTY:
        ttmode = 'F';
        break;

    default:
        rig_debug(RIG_DEBUG_ERR, "%s: unsupported mode %s\n", __func__,
                  rig_strrmode(mode));
        return -RIG_EINVAL;
    }

    SNPRINTF((char *) cmd_buf, sizeof(cmd_buf), "TB%c\n", ttmode);

    retval = mds_transaction(rig, cmd_buf, 0, NULL);

    if (retval < 0)
    {
        return retval;
    }

    return RIG_OK;
}

/*
 * mds_get_mode
 * Assumes rig!=NULL
 * Note that 2050 does not have set or get width
 */
int mds_get_mode(RIG *rig, vfo_t vfo, rmode_t *mode, pbwidth_t *width)
{
    char *result = NULL;
    int retval;

    rig_debug(RIG_DEBUG_VERBOSE, "%s: vfo=%s\n", __func__, rig_strvfo(vfo));

    retval = mds_transaction(rig, "IB", 0, &result);

    if (retval != RIG_OK)
    {
        rig_debug(RIG_DEBUG_ERR, "%s: bad response=%s\n", __func__, result);
        return retval;
    }

    //dump_hex((unsigned char *)result,strlen(result));
    switch (result[1])
    {
    case 'L':
        *mode = RIG_MODE_LSB;
        break;

    case 'U':
        *mode = RIG_MODE_USB;
        break;

    case 'A':
        *mode = RIG_MODE_AM;
        break;

    case 'F':
        *mode = RIG_MODE_RTTY;
        break;

    case 'C':
        *mode = RIG_MODE_CW;
        break;

    default:
        rig_debug(RIG_DEBUG_ERR, "%s: Unknown mode='%c%c'\n", __func__,  result[0],
                  result[1]);
        return -RIG_EPROTO;
    }

    *width = 3000; // we'll default this to 3000 for now
    rig_debug(RIG_DEBUG_VERBOSE, "%s: vfo=%s mode=%s width=%d\n", __func__,
              rig_strvfo(vfo), rig_strrmode(*mode), (int)*width);

    return RIG_OK;
}

#if 0
int mds_get_vfo(RIG *rig, vfo_t *vfo)
{
    *vfo = RIG_VFO_A;

    if (check_vfo(*vfo) == FALSE)
    {
        rig_debug(RIG_DEBUG_ERR, "%s: unsupported VFO %s\n", __func__,
                  rig_strvfo(*vfo));
        return -RIG_EINVAL;
    }

    rig_debug(RIG_DEBUG_VERBOSE, "%s: vfo=%s\n", __func__, rig_strvfo(*vfo));

    return RIG_OK;
}
#endif

/*
 * mds_get_level
 */
int mds_get_level(RIG *rig, vfo_t vfo, setting_t level, value_t *val)
{
    int retval = 0;
    char *response = NULL;

    switch (level)
    {
        int strength;
        int n;

    case RIG_LEVEL_STRENGTH:
        retval = mds_transaction(rig, "IAL", 0, &response);

        if (retval < 0)
        {
            rig_debug(RIG_DEBUG_ERR, "%s: invalid response=%s\n", __func__,
                      response);
            return retval;
        }

        n = sscanf(response, "%2d", &strength);

        if (n == 1)
        {
            val->i = strength;
        }
        else
        {
            rig_debug(RIG_DEBUG_ERR, "%s: unable to parse STRENGTH from %s\n",
                      __func__, response);
            return -RIG_EPROTO;
        }

        break;

    default:
        rig_debug(RIG_DEBUG_ERR, "%s: unsupported level %s\n", __func__,
                  rig_strlevel(level));
        return -RIG_EINVAL;
    }

    rig_debug(RIG_DEBUG_VERBOSE, "%s: vfo=%s level=%s val=%s\n", __func__,
              rig_strvfo(vfo), rig_strlevel(level), response);

    return RIG_OK;
}

/*
 * mds_get_info
 */
const char *mds_get_info(RIG *rig)
{
    char *response = NULL;
    int retval;

    rig_debug(RIG_DEBUG_VERBOSE, "%s called\n", __func__);

    retval = mds_transaction(rig, "MODEL", 16, &response);

    if (retval != RIG_OK)
    {
        rig_debug(RIG_DEBUG_WARN, "%s: MODEL command failed: %s\n", __func__,
                  strerror(retval));
    }
    else
    {
        rig_debug(RIG_DEBUG_VERBOSE, "Model: %s\n", response);
    }

    response = NULL;
    retval = mds_transaction(rig, "SER", 16, &response);

    if (retval != RIG_OK)
    {
        rig_debug(RIG_DEBUG_WARN, "%s: SER command failed: %s\n", __func__,
                  strerror(retval));
    }
    else
    {
        rig_debug(RIG_DEBUG_VERBOSE, "Serial# %s\n", response);
    }

    response = NULL;
    retval = mds_transaction(rig, "SREV", 16, &response);

    if (retval != RIG_OK)
    {
        rig_debug(RIG_DEBUG_WARN, "%s: SREV command failed: %s\n", __func__,
                  strerror(retval));
    }
    else
    {
        rig_debug(RIG_DEBUG_VERBOSE, "Firmware %s\n", response);
    }

    response = NULL;
    retval = mds_transaction(rig, "SHOW DC", 16, &response);

    if (retval != RIG_OK)
    {
        rig_debug(RIG_DEBUG_ERR, "%s: SHOW DC failed result=%s\n", __func__, response);
    }
    else
    {
        rig_debug(RIG_DEBUG_VERBOSE, "DC %s\n", response);
    }

    return response;
}

int mds_open(RIG *rig)
{
    char *response = NULL;
    int retval;

    ENTERFUNC;
    mds_get_info(rig);
    retval = mds_transaction(rig, "MODEM NONE", 0, &response);
    retval = mds_transaction(rig, "PTT 0", 0, &response);
    RETURNFUNC(retval);
}

const struct rig_caps mds_caps =
{
    RIG_MODEL(RIG_MODEL_MDS4710),
    .model_name =       "4710",
    .mfg_name =         "MDS",
    .version =          "20221114.0",
    .copyright =        "LGPL",
    .status =           RIG_STATUS_BETA,
    .rig_type =         RIG_TYPE_TRANSCEIVER,
//    .targetable_vfo =   RIG_TARGETABLE_FREQ | RIG_TARGETABLE_MODE,
    .ptt_type =         RIG_PTT_RIG,
    .dcd_type =         RIG_DCD_NONE,
    .port_type =        RIG_PORT_SERIAL,
    .serial_rate_min =  110,
    .serial_rate_max =  38400,
    .serial_data_bits = 8,
    .serial_stop_bits = 1,
    .serial_parity =    RIG_PARITY_NONE,
    .serial_handshake = RIG_HANDSHAKE_HARDWARE,
    .write_delay =      0,
    .post_write_delay = 0,
    .timeout =          1000,
    .retry =            3,

    .has_get_func =     RIG_FUNC_NONE,
    .has_set_func =     RIG_FUNC_NONE,
    .has_get_level =    MDS_LEVELS,
    .has_set_level =    RIG_LEVEL_NONE,
    .has_get_parm =     RIG_PARM_NONE,
    .has_set_parm =     RIG_PARM_NONE,
//  .level_gran =          { [LVL_CWPITCH] = { .step = { .i = 10 } } },
//  .ctcss_list =          common_ctcss_list,
//  .dcs_list =            full_dcs_list,
//  2050 does have channels...not implemented yet as no need yet
//  .chan_list =   {
//                        {   0,  18, RIG_MTYPE_MEM, DUMMY_MEM_CAP },
//                        {  19,  19, RIG_MTYPE_CALL },
//                        {  20,  NB_CHAN-1, RIG_MTYPE_EDGE },
//                        RIG_CHAN_END,
//                 },
// .scan_ops =    DUMMY_SCAN,
// .vfo_ops =     DUMMY_VFO_OP,
    .transceive =       RIG_TRN_RIG,
    .rx_range_list1 = {
        {.startf = MHz(380), .endf = MHz(530), .modes = RIG_MODE_ALL,
            .low_power = 0, .high_power = 0, MDS_VFOS, RIG_ANT_1
        },
        RIG_FRNG_END,
    },
    .rx_range_list2 = {RIG_FRNG_END,},
    .tx_range_list1 = { 
    {MHz(380), MHz(530), RIG_MODE_ALL, W(.1), W(5), RIG_VFO_A, RIG_ANT_NONE},
    RIG_FRNG_END,
    },
//    .tx_range_list2 = {RIG_FRNG_END,}
    .tuning_steps =     {
        // Rem: no support for changing tuning step
        {RIG_MODE_ALL, 6250},
        RIG_TS_END,
    },

    .filters =  {
        {RIG_MODE_ALL, RIG_FLT_ANY},
        RIG_FLT_END
    },
    .priv = NULL,

    .rig_init =     mds_init,
    .rig_open =     mds_open,
    .rig_cleanup =  mds_cleanup,

//    .set_conf =     dummy_set_conf,
//    .get_conf =     dummy_get_conf,

    .set_freq = mds_set_freq,
    .get_freq = mds_get_freq,
//    .set_mode = mds_set_mode,
//    .get_mode = mds_get_mode,

//    .set_level =     dummy_set_level,
//    .get_level =    mds_get_level,

    .get_info =     mds_get_info,
    .set_ptt =      mds_set_ptt,
    .get_ptt =      mds_get_ptt,
//  .get_dcd =    dummy_get_dcd,
    .hamlib_check_rig_caps = HAMLIB_CHECK_RIG_CAPS
};
