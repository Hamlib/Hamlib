/*
 *  Hamlib CODAN backend - main file
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
#include <string.h>

#include <hamlib/rig.h>
#include "serial.h"
#include "misc.h"
#include "cal.h"
#include "token.h"
#include "register.h"

#include "codan.h"

#define MAXCMDLEN 64

#define CODAN_VFOS (RIG_VFO_A|RIG_VFO_B)

#define CODAN_MODES (RIG_MODE_USB)

int codan_set_ptt(RIG *rig, vfo_t vfo, ptt_t ptt);
static int codan_get_ptt(RIG *rig, vfo_t vfo, ptt_t *ptt);
int codan_set_mode(RIG *rig, vfo_t vfo, rmode_t mode, pbwidth_t width);
int codan_get_mode(RIG *rig, vfo_t vfo, rmode_t *mode,
                   pbwidth_t *width);

int codan_transaction(RIG *rig, char *cmd, int expected, char **result)
{
    char cmd_buf[MAXCMDLEN];
    int retval;
    struct rig_state *rs = &rig->state;
    struct codan_priv_data *priv = rig->state.priv;
    //int retry = 3;

    rig_debug(RIG_DEBUG_VERBOSE, "%s: cmd=%s\n", __func__, cmd);

    SNPRINTF(cmd_buf, sizeof(cmd_buf), "%s%s", cmd, EOM);

    rig_flush(&rs->rigport);
    retval = write_block(&rs->rigport, (unsigned char *) cmd_buf, strlen(cmd_buf));
    hl_usleep(rig->caps->post_write_delay);

    if (retval < 0)
    {
        return retval;
    }

    if (expected == 0)
    {
        // response format is response...0x0d0x0a
        retval = read_string(&rs->rigport, (unsigned char *) priv->ret_data,
                             sizeof(priv->ret_data),
                             "\x0a", 1, 0, 1);
        rig_debug(RIG_DEBUG_VERBOSE, "%s: result=%s, resultlen=%d\n", __func__,
                  priv->ret_data, (int)strlen(priv->ret_data));

        if (retval < 0)
        {
            return retval;
        }
    }
    else
    {
        retval = read_string(&rs->rigport, (unsigned char *) priv->ret_data,
                             sizeof(priv->ret_data),
                             "\x0a", 1, 0, 1);

        if (retval < 0)
        {
            return retval;
        }

        if (strncmp(priv->ret_data, "LEVELS:", 7) == 0)
        {
            rig_debug(RIG_DEBUG_VERBOSE, "%s: %s\n", __func__, priv->ret_data);
            retval = read_string(&rs->rigport, (unsigned char *) priv->ret_data,
                                 sizeof(priv->ret_data),
                                 "\x0a", 1, 0, 1);
            rig_debug(RIG_DEBUG_VERBOSE, "%s: %s\n", __func__, priv->ret_data);
        }
    }

#if 0
    int hr, min, sec;

    while (--retry >= 0 && strncmp(priv->ret_data, "OK", 2) != 0
            && sscanf(priv->ret_data, "%d:%d:%d", &hr, &min, &sec) != 3)
    {
        char tmpbuf[256];
        retval = read_string(&rs->rigport, (unsigned char *) tmpbuf,
                             sizeof(priv->ret_data),
                             "\x0a", 1, 0, 1);

        dump_hex((unsigned char *)priv->ret_data, strlen(priv->ret_data));
        rig_debug(RIG_DEBUG_VERBOSE, "%s: %s\n", __func__, priv->ret_data);
    }

#endif

    rig_debug(RIG_DEBUG_VERBOSE, "%s: retval=%d\n", __func__, retval);
    rig_debug(RIG_DEBUG_VERBOSE, "%s: %s\n", __func__, priv->ret_data);

    if (result != NULL)
    {
        *result = &(priv->ret_data[0]);
        rig_debug(RIG_DEBUG_VERBOSE, "%s: returning result=%s\n", __func__,
                  *result);
    }
    else
    {
        rig_debug(RIG_DEBUG_VERBOSE, "%s: no result requested\n", __func__);
    }

    return RIG_OK;
}

int codan_init(RIG *rig)
{
    rig_debug(RIG_DEBUG_VERBOSE, "%s version %s\n", __func__,
              rig->caps->version);
    // cppcheck claims leak here but it's freed in cleanup
    rig->state.priv = (struct codan_priv_data *)calloc(1,
                      sizeof(struct codan_priv_data));

    if (!rig->state.priv)
    {
        return -RIG_ENOMEM;
    }

    RETURNFUNC2(RIG_OK);
}

int codan_open(RIG *rig)
{
    char *results = NULL;
    codan_transaction(rig, "scan off\r", 1, &results);
    codan_transaction(rig, "echo off", 1, &results);
    //codan_transaction(rig, "prompt time", 1, &results);
    codan_transaction(rig, "login", 1, &results);

    if (!strstr(results, "admin"))
    {
        codan_transaction(rig, "login admin ''", 0, NULL);
    }

    codan_transaction(rig, "login", 1, &results);
    codan_set_freq(rig, RIG_VFO_A, 14074000.0);

    RETURNFUNC2(RIG_OK);
}

int codan_close(RIG *rig)
{
    char *results = NULL;
    codan_transaction(rig, "logout admin\rfreq", 1, &results);
    RETURNFUNC2(RIG_OK);
}

int codan_cleanup(RIG *rig)
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
 * codan_get_mode
 * Assumes rig!=NULL
 */
int codan_get_mode(RIG *rig, vfo_t vfo, rmode_t *mode, pbwidth_t *width)
{
    char *result = NULL;
    char modeA[8], modeB[8];
    int widthA, center;
    int retval;

    rig_debug(RIG_DEBUG_VERBOSE, "%s: vfo=%s\n", __func__, rig_strvfo(vfo));

    retval = codan_transaction(rig, "mode", 0, &result);

    if (retval != RIG_OK)
    {
        rig_debug(RIG_DEBUG_ERR, "%s: bad response=%s\n", __func__, result);
        return retval;
    }

    rig_debug(RIG_DEBUG_VERBOSE, "%s: result=%s", __func__, result);
    int n = sscanf(result, "MODE: %[A-Z], %[A-Z], %d, %d", modeA, modeB, &center,
                   &widthA);

    if (n != 4)
    {
        rig_debug(RIG_DEBUG_ERR, "%s: sscanf expected 4, got %d, %s\n", __func__, n,
                  result);
        return -RIG_EPROTO;
    }

    if (strncmp(modeA, "USB", 3) == 0) { *mode = RIG_MODE_USB; }
    else if (strncmp(modeA, "LSB", 3) == 0) { *mode = RIG_MODE_LSB; }
    else
    {
        rig_debug(RIG_DEBUG_ERR, "%s: Unknown mode=%s'\n", __func__,  modeA);
        return -RIG_EPROTO;
    }

    *width = widthA; // we'll default this to 3000 for now
    rig_debug(RIG_DEBUG_VERBOSE, "%s: vfo=%s mode=%s width=%d\n", __func__,
              rig_strvfo(vfo), rig_strrmode(*mode), (int)*width);

    return RIG_OK;
}

/*
 * codan_set_mode
 * Assumes rig!=NULL
 */
int codan_set_mode(RIG *rig, vfo_t vfo, rmode_t mode, pbwidth_t width)
{
    char cmd_buf[32], *ttmode;
    char *response = NULL;
    int retval;

    rig_debug(RIG_DEBUG_VERBOSE, "%s: vfo=%s mode=%s width=%d\n", __func__,
              rig_strvfo(vfo), rig_strrmode(mode), (int)width);

    switch (mode)
    {
    case RIG_MODE_USB:
        ttmode = "USBW";
        break;

    case RIG_MODE_LSB:
        ttmode = "LSBW";
        break;

    default:
        rig_debug(RIG_DEBUG_ERR, "%s: unsupported mode %s\n", __func__,
                  rig_strrmode(mode));
        return -RIG_EINVAL;
    }

    SNPRINTF((char *) cmd_buf, sizeof(cmd_buf), "mode %s", ttmode);

    retval = codan_transaction(rig, cmd_buf, 0, &response);

    if (retval < 0)
    {
        return retval;
    }

    return RIG_OK;
}


/*
 * codan_set_freq
 * assumes rig!=NULL, rig->state.priv!=NULL
 */
int codan_set_freq(RIG *rig, vfo_t vfo, freq_t freq)
{
    char cmd_buf[MAXCMDLEN];
    int retval;

    rig_debug(RIG_DEBUG_VERBOSE, "%s: vfo=%s freq=%.0f\n", __func__,
              rig_strvfo(vfo), freq);

    // Purportedly can't do split so we just set VFOB=VFOA
    SNPRINTF(cmd_buf, sizeof(cmd_buf), "connect tcvr rf %.0f %.0f\rfreq", freq,
             freq);

    char *response = NULL;
    retval = codan_transaction(rig, cmd_buf, 0, &response);

    if (retval < 0)
    {
        return retval;
    }

    return retval;
}

/*
 * codan_get_freq
 * Assumes rig!=NULL, rig->state.priv!=NULL, freq!=NULL
 */
int codan_get_freq(RIG *rig, vfo_t vfo, freq_t *freq)
{
    int retval;
    char *response = NULL;

    rig_debug(RIG_DEBUG_VERBOSE, "%s: vfo=%s\n", __func__, rig_strvfo(vfo));
    *freq = 0;

    retval = codan_transaction(rig, "freq", 0, &response);

    if (retval != RIG_OK)
    {
        rig_debug(RIG_DEBUG_ERR, "%s: invalid response=%s\n", __func__, response);
        return retval;
    }

    retval = sscanf(response, "FREQ: %lg", freq);

    *freq *= 1000; // returne freq is in kHz

    if (retval != 1)
    {
        rig_debug(RIG_DEBUG_ERR, "%s: Unable to parse response\n", __func__);
        return -RIG_EPROTO;
    }

    return RIG_OK;
}

/*
 * codan_get_ptt
 * Assumes rig!=NULL
 */
int codan_get_ptt(RIG *rig, vfo_t vfo, ptt_t *ptt)
{
    int retval;
    char *response = NULL;

    rig_debug(RIG_DEBUG_VERBOSE, "%s: vfo=%s\n", __func__, rig_strvfo(vfo));

    retval = codan_transaction(rig, "connect tcvr rf ptt", 0, &response);

    if (retval != RIG_OK)
    {
        rig_debug(RIG_DEBUG_ERR, "%s: error response?='%s'\n", __func__, response);
        return retval;
    }

    char *p = strstr(response, "Ptt");

    if (p)
    {
        if (strcmp(p, "Ptt=Off") == 0) { *ptt = 0; }
        else { *ptt = 1; }
    }
    else
    {
        rig_debug(RIG_DEBUG_ERR, "%s: unable to find Ptt in %s\n", __func__, response);
        return -RIG_EPROTO;
    }

    return RIG_OK;
}

/*
 * codan_set_ptt
 * Assumes rig!=NULL
 */
int codan_set_ptt(RIG *rig, vfo_t vfo, ptt_t ptt)
{
    int retval;
    char cmd_buf[MAXCMDLEN];
    char *response;

    rig_debug(RIG_DEBUG_VERBOSE, "%s: ptt=%d\n", __func__, ptt);

    SNPRINTF(cmd_buf, sizeof(cmd_buf), "connect tcvr rf ptt %s\rptt",
             ptt == 0 ? "off" : "on");
    response = NULL;
    retval = codan_transaction(rig, cmd_buf, 0, &response);

    if (retval < 0)
    {
        rig_debug(RIG_DEBUG_ERR, "%s: invalid response=%s\n", __func__, response);
        return retval;
    }

    rig_debug(RIG_DEBUG_VERBOSE, "%s: cmd result=%s\n", __func__, response);

    return RIG_OK;
}




const struct rig_caps envoy_caps =
{
    RIG_MODEL(RIG_MODEL_CODAN_ENVOY),
    .model_name =       "Envoy",
    .mfg_name =         "CODAN",
    .version =          BACKEND_VER ".0",
    .copyright =        "LGPL",
    .status =           RIG_STATUS_STABLE,
    .rig_type =         RIG_TYPE_TRANSCEIVER,
    .targetable_vfo =   RIG_TARGETABLE_FREQ,
    .ptt_type =         RIG_PTT_RIG,
    .dcd_type =         RIG_DCD_NONE,
    .port_type =        RIG_PORT_SERIAL,
    .serial_rate_min =  9600,
    .serial_rate_max =  115200,
    .serial_data_bits = 8,
    .serial_stop_bits = 1,
    .serial_parity =    RIG_PARITY_NONE,
    .serial_handshake = RIG_HANDSHAKE_NONE,
    .write_delay =      0,
    .post_write_delay = 50,
    .timeout =          1000,
    .retry =            3,

    .has_get_func =     RIG_FUNC_NONE,
    .has_set_func =     RIG_FUNC_NONE,
    .has_get_level =    RIG_LEVEL_NONE,
    .has_set_level =    RIG_LEVEL_NONE,
    .has_get_parm =     RIG_PARM_NONE,
    .has_set_parm =     RIG_PARM_NONE,
    .transceive =       RIG_TRN_RIG,
    .rx_range_list1 = {{
            .startf = kHz(1600), .endf = MHz(30), .modes = CODAN_MODES,
            .low_power = -1, .high_power = -1, CODAN_VFOS, RIG_ANT_1
        },
        RIG_FRNG_END,
    },
    .rx_range_list2 = {RIG_FRNG_END,},
    .tx_range_list1 = {RIG_FRNG_END,},
    .tx_range_list2 = {RIG_FRNG_END,},
    .tuning_steps =  { {CODAN_MODES, 1}, {CODAN_MODES, RIG_TS_ANY},  RIG_TS_END, },
    .filters = {
        {RIG_MODE_SSB, kHz(2.4)},
        {RIG_MODE_SSB, kHz(0.5)},
        {RIG_MODE_SSB, kHz(2.7)},
        {RIG_MODE_SSB, kHz(3.0)},
        RIG_FLT_END,
    },
    .priv = NULL,

    .rig_init =     codan_init,
    .rig_open =     codan_open,
    .rig_close =     codan_close,
    .rig_cleanup =  codan_cleanup,

    .set_freq = codan_set_freq,
    .get_freq = codan_get_freq,
    .set_mode = codan_set_mode,
    .get_mode = codan_get_mode,

    .set_ptt =      codan_set_ptt,
    .get_ptt =      codan_get_ptt,
    .hamlib_check_rig_caps = HAMLIB_CHECK_RIG_CAPS
};

const struct rig_caps ngs_caps =
{
    RIG_MODEL(RIG_MODEL_CODAN_NGT),
    .model_name =       "NGT",
    .mfg_name =         "CODAN",
    .version =          BACKEND_VER ".0",
    .copyright =        "LGPL",
    .status =           RIG_STATUS_STABLE,
    .rig_type =         RIG_TYPE_TRANSCEIVER,
    .targetable_vfo =   RIG_TARGETABLE_FREQ,
    .ptt_type =         RIG_PTT_RIG,
    .dcd_type =         RIG_DCD_NONE,
    .port_type =        RIG_PORT_SERIAL,
    .serial_rate_min =  9600,
    .serial_rate_max =  115200,
    .serial_data_bits = 8,
    .serial_stop_bits = 1,
    .serial_parity =    RIG_PARITY_NONE,
    .serial_handshake = RIG_HANDSHAKE_NONE,
    .write_delay =      0,
    .post_write_delay = 50,
    .timeout =          1000,
    .retry =            3,

    .has_get_func =     RIG_FUNC_NONE,
    .has_set_func =     RIG_FUNC_NONE,
    .has_get_level =    RIG_LEVEL_NONE,
    .has_set_level =    RIG_LEVEL_NONE,
    .has_get_parm =     RIG_PARM_NONE,
    .has_set_parm =     RIG_PARM_NONE,
    .transceive =       RIG_TRN_RIG,
    .rx_range_list1 = {{
            .startf = kHz(1600), .endf = MHz(30), .modes = CODAN_MODES,
            .low_power = -1, .high_power = -1, CODAN_VFOS, RIG_ANT_1
        },
        RIG_FRNG_END,
    },
    .rx_range_list2 = {RIG_FRNG_END,},
    .tx_range_list1 = {RIG_FRNG_END,},
    .tx_range_list2 = {RIG_FRNG_END,},
    .tuning_steps =  { {CODAN_MODES, 1}, {CODAN_MODES, RIG_TS_ANY},  RIG_TS_END, },
    .filters = {
        {RIG_MODE_SSB, kHz(2.4)},
        {RIG_MODE_SSB, kHz(0.5)},
        {RIG_MODE_SSB, kHz(2.7)},
        {RIG_MODE_SSB, kHz(3.0)},
        RIG_FLT_END,
    },
    .priv = NULL,

    .rig_init =     codan_init,
    .rig_cleanup =  codan_cleanup,

    .set_freq = codan_set_freq,
    .get_freq = codan_get_freq,
    .set_mode = codan_set_mode,
    .get_mode = codan_get_mode,

    .set_ptt =      codan_set_ptt,
    .get_ptt =      codan_get_ptt,
    .hamlib_check_rig_caps = HAMLIB_CHECK_RIG_CAPS
};



DECLARE_INITRIG_BACKEND(codan)
{
    rig_debug(RIG_DEBUG_VERBOSE, "%s: _init called\n", __func__);

    rig_register(&envoy_caps);
    rig_register(&ngs_caps);
    rig_debug(RIG_DEBUG_VERBOSE, "%s: _init back from rig_register\n", __func__);

    return RIG_OK;
}










