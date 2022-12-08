/*
 *  GomSpace backend and the GS100 radio control module
 *
 *  Created in 2022 by Richard Linhart OK1CTR OK1CTR@gmail.com
 *  and used during VZLUSAT-2 satellite mission.
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

/* Includes ------------------------------------------------------------------*/

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>

#include "hamlib/rig.h"
#include "serial.h"
#include "parallel.h"
#include "cm108.h"
#include "gpio.h"
#include "misc.h"
#include "tones.h"
#include "idx_builtin.h"
#include "register.h"

#include "gs100.h"

/* Private Defines -----------------------------------------------------------*/

// If defined, commands are software simulated only
#undef _LOCAL_SIMULATION_

// If true, configuration table switching is minimized
#define PARAM_MEM_MINIMAL                                                  1

// Buffer size for serial transfers
#define BUFSZ                                                            256
// Last character on each line received from RIG
#define GOM_STOPSET                                                     "\n"
// Exact character sequence on the RIG's command prompt
#define GOM_PROMPT         "\x1B[1;32mnanocom-ax\x1B[1;30m # \x1B[0m\x1B[0m"
// Maximum number of lines parsed from GS100 response
#define GOM_MAXLINES                                                      20

// RIG's parametric table number for receive
#define GOM_CONFIG_TAB_RX                                                  1
// RIG's parametric table number for transmit
#define GOM_CONFIG_TAB_TX                                                  5

/* Private Typedefs ----------------------------------------------------------*/

/**
 * GS100 rig private data structure
 */
struct gs100_priv_data
{
    freq_t freq_rx;     ///< currently just for backup and TRX emulation
    freq_t freq_tx;     ///< currently just for backup and TRX emulation
    int param_mem;      ///< last value of configuration table selection
};

/* Imported Functions --------------------------------------------------------*/

struct ext_list *alloc_init_ext(const struct confparams *cfp);
struct ext_list *find_ext(struct ext_list *elp, token_t token);

/* Private function prototypes -----------------------------------------------*/

/**
 * Set variable in the GS100 configuration table
 */
static int gomx_set(RIG *rig, int table, char *varname, char *varvalue);

/**
 * Get variable from the GS100 configuration table
 */
static int gomx_get(RIG *rig, int table, char *varname, char *varvalue);

/**
 * Sends a message to the GS100 and parses response lines
 */
static int gomx_transaction(RIG *rig, char *message, char *response);

/* Functions -----------------------------------------------------------------*/

/* GS100 transceiver control init */
static int gs100_init(RIG *rig)
{
    __attribute__((unused)) struct gs100_priv_data *priv = (struct gs100_priv_data
            *)rig->state.priv;

    ENTERFUNC;

    if (!rig || !rig->caps)
    {
        RETURNFUNC(-RIG_EINVAL);
    }

    priv = (struct gs100_priv_data *)calloc(1, sizeof(struct gs100_priv_data));

    if (!priv)
    {
        RETURNFUNC(-RIG_ENOMEM);
    }

    rig->state.priv = (void *)priv;

#ifdef _LOCAL_SIMULATION_
    rig->state.rigport.type.rig = RIG_PORT_NONE;  // just simulation
    priv->freq_rx = rig->caps->rx_range_list1->startf;
    priv->freq_tx = rig->caps->tx_range_list1->startf;
#endif

    priv->param_mem = -1;  // means undefined last selection

    RETURNFUNC(RIG_OK);
}


/* GS100 transceiver control deinit */
static int gs100_cleanup(RIG *rig)
{
    __attribute__((unused)) struct gs100_priv_data *priv = (struct gs100_priv_data
            *)rig->state.priv;

    ENTERFUNC;

    if (rig->state.priv)
    {
        free(rig->state.priv);
    }

    rig->state.priv = NULL;

    RETURNFUNC(RIG_OK);
}


/* GS100 transceiver open */
static int gs100_open(RIG *rig)
{
    ENTERFUNC;

    if (rig->caps->rig_model == RIG_MODEL_GS100)
    {
        rig_debug(RIG_DEBUG_VERBOSE, "%s: OPENING'\n", __func__);
    }

    RETURNFUNC(RIG_OK);
}


/* GS100 transceiver close */
static int gs100_close(RIG *rig)
{
    ENTERFUNC;

    if (rig->caps->rig_model == RIG_MODEL_GS100)
    {
        rig_debug(RIG_DEBUG_VERBOSE, "%s: CLOSING'\n", __func__);
    }

    RETURNFUNC(RIG_OK);
}


/* GS100 transceiver set configuration */
static int gs100_set_conf(RIG *rig, token_t token, const char *val)
{
    __attribute__((unused)) struct gs100_priv_data *priv = (struct gs100_priv_data
            *)rig->state.priv;

    ENTERFUNC;

    priv = (struct gs100_priv_data *)rig->state.priv;

    switch (token)
    {
    case 0:
        break;

    case 1:
        break;

    default:
        RETURNFUNC(-RIG_EINVAL);
    }

    RETURNFUNC(RIG_OK);
}


/* GS100 transceiver get configuration */
static int gs100_get_conf(RIG *rig, token_t token, char *val)
{
    __attribute__((unused)) struct gs100_priv_data *priv = (struct gs100_priv_data
            *)rig->state.priv;

    ENTERFUNC;

    priv = (struct gs100_priv_data *)rig->state.priv;

    switch (token)
    {
    case 0:
        break;

    default:
        RETURNFUNC(-RIG_EINVAL);
    }

    RETURNFUNC(RIG_OK);
}


/* GS100 transceiver set receiver frequency */
static int gs100_set_freq(RIG *rig, vfo_t vfo, freq_t freq)
{
    __attribute__((unused)) struct gs100_priv_data *priv = (struct gs100_priv_data
            *)rig->state.priv;
    char fstr[20], value[20];
    int retval;

    ENTERFUNC;

    // reporting
    sprintf_freq(fstr, sizeof(fstr), freq);
    rig_debug(RIG_DEBUG_VERBOSE, "%s: fstr = '%s'\n", __func__, fstr);

#ifdef _LOCAL_SIMULATION_
    priv->freq_rx = freq;
#endif

    // range check - GS100 don't do it!
    if (freq < rig->caps->rx_range_list1->startf
            || freq > rig->caps->rx_range_list1->endf) { RETURNFUNC(-RIG_EDOM); }

    // perform set command
    sprintf(value, "%1.0lf", freq);
    retval = gomx_set(rig, GOM_CONFIG_TAB_RX, "freq", value);

    if (retval != RIG_OK) { RETURNFUNC(retval); }

    RETURNFUNC(retval);
}


/* GS100 transceiver get receiver frequency */
static int gs100_get_freq(RIG *rig, vfo_t vfo, freq_t *freq)
{
    __attribute__((unused)) struct gs100_priv_data *priv = (struct gs100_priv_data
            *)rig->state.priv;
    char resp[20];
    int retval;
    freq_t f;

    ENTERFUNC;

    // perform the get command
    retval = gomx_get(rig, GOM_CONFIG_TAB_RX, "freq", resp);

    if (retval != RIG_OK) { RETURNFUNC(retval); }

#ifdef _LOCAL_SIMULATION_
    *freq = priv->freq_rx;
#else
    retval = sscanf(resp, "%lf", &f);
#endif

    // relevance check
    if (retval != 1) { RETURNFUNC(-RIG_EPROTO); }

    if (f < rig->caps->rx_range_list1->startf
            || f > rig->caps->rx_range_list1->endf) { RETURNFUNC(-RIG_EDOM); }

    *freq = f;

    RETURNFUNC(RIG_OK);
}


/* GS100 transceiver set transmitter frequency */
static int gs100_set_tx_freq(RIG *rig, vfo_t vfo, freq_t freq)
{
    __attribute__((unused)) struct gs100_priv_data *priv = (struct gs100_priv_data
            *)rig->state.priv;
    char fstr[20], value[20];
    int retval;

    ENTERFUNC;

    // reporting
    sprintf_freq(fstr, sizeof(fstr), freq);
    rig_debug(RIG_DEBUG_VERBOSE, "%s: fstr = '%s'\n", __func__, fstr);

#ifdef _LOCAL_SIMULATION_
    priv->freq_tx = freq;
#endif

    // range check - GS100 don't do it!
    if (freq < rig->caps->tx_range_list1->startf
            || freq > rig->caps->tx_range_list1->endf) { RETURNFUNC(-RIG_EDOM); }

    // perform set command
    sprintf(value, "%1.0lf", freq);
    retval = gomx_set(rig, GOM_CONFIG_TAB_TX, "freq", value);

    if (retval != RIG_OK) { RETURNFUNC(retval); }

    RETURNFUNC(RIG_OK);
}


/* GS100 transceiver get transmitter frequency */
static int gs100_get_tx_freq(RIG *rig, vfo_t vfo, freq_t *freq)
{
    __attribute__((unused)) struct gs100_priv_data *priv = (struct gs100_priv_data
            *)rig->state.priv;
    char resp[20];
    int retval;
    freq_t f;

    ENTERFUNC;

    // perform the get command
    retval = gomx_get(rig, GOM_CONFIG_TAB_TX, "freq", resp);

    if (retval != RIG_OK) { RETURNFUNC(retval); }

#ifdef _LOCAL_SIMULATION_
    *freq = priv->freq_tx;
#else
    retval = sscanf(resp, "%lf", &f);
#endif

    // relevance check
    if (retval != 1) { RETURNFUNC(-RIG_EPROTO); }

    if (f < rig->caps->tx_range_list1->startf
            || f > rig->caps->tx_range_list1->endf) { RETURNFUNC(-RIG_EDOM); }

    *freq = f;

    RETURNFUNC(RIG_OK);
}


/* GS100 transceiver get info */
static const char *gs100_get_info(RIG *rig)
{
    return ("Gomspace Ground Station Transceiver GS100");
}

/* The HAMLIB RIG Capabilities Structure Definition --------------------------*/

#define GS100_FUNC  (0)
#define GS100_LEVEL (0)
#define GS100_PARM  (0)
#define GS100_VFO_OPS  RIG_OP_TUNE
#define GS100_VFOS (RIG_VFO_A)
#define GS100_MODES (RIG_MODE_PKTFM)

struct rig_caps GS100_caps =
{
    RIG_MODEL(RIG_MODEL_GS100),
    .model_name = "GS100",
    .mfg_name = "GOMSPACE",
    .version = "20211117.0",
    .copyright = "LGPL",
    .status = RIG_STATUS_BETA,
    .rig_type = RIG_TYPE_TRANSCEIVER,
    .targetable_vfo = 0,
    .ptt_type = RIG_PTT_NONE,
    .dcd_type = RIG_DCD_NONE,
    .port_type = RIG_PORT_SERIAL,
    .serial_rate_min = 115200,
    .serial_rate_max = 500000,
    .serial_data_bits = 8,
    .serial_stop_bits = 1,
    .serial_parity = RIG_PARITY_NONE,
    .serial_handshake = RIG_HANDSHAKE_NONE,
    .write_delay = 1,
    .post_write_delay = 1,
    .timeout = 250,
    .retry = 0,
    .has_get_func = GS100_FUNC,
    .has_set_func = GS100_FUNC,
    .has_get_level = GS100_LEVEL,
    .has_set_level = GS100_LEVEL,
    .has_get_parm = GS100_PARM,
    .has_set_parm = GS100_PARM,
    .vfo_ops = GS100_VFO_OPS,
    .rx_range_list1 =  { {
            .startf = MHz(430), .endf = MHz(440), .modes = GS100_MODES,
            .low_power = 0, .high_power = 0, GS100_VFOS,
            .label = "GS100#1"
        }, RIG_FRNG_END,
    },
    .tx_range_list1 =  { {
            .startf = MHz(430), .endf = MHz(440), .modes = GS100_MODES,
            .low_power = 0, .high_power = 0, GS100_VFOS,
            .label = "GS100#1"
        }, RIG_FRNG_END,
    },
    .tuning_steps = { {GS100_MODES, Hz(10)}, RIG_TS_END, },
    .filters =  {
        {RIG_MODE_ALL, RIG_FLT_ANY},
        RIG_FLT_END
    },
    .priv = NULL,
    .rig_init = gs100_init,
    .rig_cleanup = gs100_cleanup,
    .rig_open = gs100_open,
    .rig_close = gs100_close,
    .set_conf = gs100_set_conf,
    .get_conf = gs100_get_conf,
    .set_freq = gs100_set_freq,
    .get_freq = gs100_get_freq,
    .set_split_freq = gs100_set_tx_freq,
    .get_split_freq = gs100_get_tx_freq,
    .get_info = gs100_get_info,
};

/* Private functions ---------------------------------------------------------*/

/* Set variable in the GS100 configuration table */
static int gomx_set(RIG *rig, int table, char *varname, char *varvalue)
{
    __attribute__((unused)) struct gs100_priv_data *priv = (struct gs100_priv_data
            *)rig->state.priv;
    int retval;
    char msg[BUFSZ], resp[BUFSZ];

    assert(rig != NULL);
    assert(varname != NULL);
    assert(varvalue != NULL);

    rig_debug(RIG_DEBUG_TRACE, "%s: table=%d, '%s' = '%s'\n", __func__, table,
              varname, varvalue);

    if (!PARAM_MEM_MINIMAL || table != priv->param_mem)
    {
        // select the configuration table
        priv->param_mem = table;
        sprintf(msg, "param mem %d\n", table);
        retval = gomx_transaction(rig, msg, resp);

        if (retval != RIG_OK) { return (retval); }
    }

    // set the variable
    sprintf(msg, "param set %s %s\n", varname, varvalue);
    retval = gomx_transaction(rig, msg, resp);

    if (retval != RIG_OK) { return (retval); }

    // check response
    if (strlen(resp) > 0) { return (-RIG_EPROTO); }

    return (RIG_OK);
}


/* Get variable from the GS100 configuration table */
static int gomx_get(RIG *rig, int table, char *varname, char *varvalue)
{
    __attribute__((unused)) struct gs100_priv_data *priv = (struct gs100_priv_data
            *)rig->state.priv;
    int retval;
    char msg[BUFSZ], resp[BUFSZ], *c;

    assert(rig != NULL);
    assert(varname != NULL);
    assert(varvalue != NULL);

    rig_debug(RIG_DEBUG_TRACE, "%s: table=%d, '%s'\n", __func__, table, varname);

    if (!PARAM_MEM_MINIMAL || table != priv->param_mem)
    {
        // select the configuration table
        priv->param_mem = table;
        sprintf(msg, "param mem %d\n", table);
        retval = gomx_transaction(rig, msg, resp);

        if (retval != RIG_OK) { return (retval); }
    }

    // get the variable
    sprintf(msg, "param get %s\n", varname);
    retval = gomx_transaction(rig, msg, resp);

    if (retval != RIG_OK) { return (retval); }

    // check response and extract the value
    if ((c = strchr(resp, '=')) == NULL) { return (-RIG_EPROTO); }

    if (sscanf(c + 1, "%s", varvalue) != 1) { return (-RIG_EPROTO); }

    return (RIG_OK);
}


/* Sends a message to the GS100 and parses response lines */
static int gomx_transaction(RIG *rig, char *message, char *response)
{
    __attribute__((unused)) struct gs100_priv_data *priv = (struct gs100_priv_data
            *)rig->state.priv;
    struct rig_state *rs;
    int retval, n = 0;
    char buf[BUFSZ];

    assert(rig != NULL);
    assert(message != NULL);
    assert(response != NULL);

    rig_debug(RIG_DEBUG_TRACE, "%s: msg='%s'\n", __func__,
              message == NULL ? "NULL" : message);

    // access to private variables
    rs = &rig->state;
    priv = (struct gs100_priv_data *)rs->priv;

    // send message to the transceiver
    rig_flush(&rs->rigport);
    retval = write_block(&rs->rigport, (uint8_t *)message, strlen(message));

    if (retval != RIG_OK) { return (retval); }

    while (1)
    {
        // read the response line
        retval = read_string(&rs->rigport, (unsigned char *)buf, BUFSZ,
                             (const char *)GOM_STOPSET, 0, strlen(GOM_STOPSET), 0);

        if (retval < 0) { return (retval); }

        if (retval == 0) { return (-RIG_ETIMEOUT); }

        n++;

        // prompt is always the last line
        if (strcmp(buf, GOM_PROMPT) == 0) { break; }

        // before last line would be the response
        if (n > 1) { strcpy(response, buf); }
        else { *response = '\0'; }  // don't return command echo

        if (n > GOM_MAXLINES) { return (-RIG_EPROTO); }
    }

    // report the response
    rig_debug(RIG_DEBUG_VERBOSE, "%s: returning response='%s'\n", __func__,
              response == NULL ? "NULL" : response);
    return (RIG_OK);
}

/* System Integration Functions ----------------------------------------------*/

/* Init RIG backend function */
DECLARE_INITRIG_BACKEND(gomspace)
{
    rig_debug(RIG_DEBUG_VERBOSE, "%s: _init called\n", __func__);
    rig_register(&GS100_caps);
    return (RIG_OK);
}


/* Probe RIG backend function */
DECLARE_PROBERIG_BACKEND(gomspace)
{
    return (RIG_MODEL_GS100);
}

/*----------------------------------------------------------------------------*/
