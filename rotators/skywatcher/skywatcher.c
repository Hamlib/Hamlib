/*
 *  Hamlib Rotator backend - SkyWatcher
 *  Copyright (c) 2024 by Andrey Rodionov <dernasherbrezon@gmail.com>
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

#include <stdlib.h>
#include "skywatcher.h"
#include "register.h"
#include "serial.h"

#define SKYWATCHER_ERROR_CODES_LENGTH 9
#define ERROR_CHECK(x)           \
  do {                           \
    int __err_rc = (x);          \
    if (__err_rc != RIG_OK) { \
      return __err_rc;           \
    }                            \
  } while (0)

static const char *skywatcher_error_codes[SKYWATCHER_ERROR_CODES_LENGTH] =
{
    "Unknown Command",
    "Command Length Error",
    "Motor not Stopped",
    "Invalid Character",
    "Not Initialized",
    "Driver Sleeping",
    "Unknown",
    "PEC Training is running",
    "No Valid PEC data"
};

typedef struct
{
    uint32_t cpr[2];
} skywatcher_priv_data;

static int skywatcher_cmd(ROT *rot, const char *cmd, char *response,
                          size_t response_len)
{
    hamlib_port_t *port = ROTPORT(rot);
    rig_flush(port);
    size_t cmd_len = strlen(cmd);
    ERROR_CHECK(write_block(port, (unsigned char *) cmd, cmd_len));
    // echo from the device
    int code = read_string(port, (unsigned char *) response, response_len, "\r", 1,
                           0, 1);

    if (code < 0)
    {
        return -code;
    }

    // the actual response
    code = read_string(port, (unsigned char *) response, response_len, "\r", 1, 0,
                       1);

    if (code < 0)
    {
        return -code;
    }

    // nullify last \r
    response[strlen(response) - 1] = '\0';

    if (response[0] == '!')
    {
        code = atoi(&response[1]);

        if (code < SKYWATCHER_ERROR_CODES_LENGTH)
        {
            rig_debug(RIG_DEBUG_ERR, "Error code: %d Message: '%s'\n", code,
                      skywatcher_error_codes[code]);
        }
        else
        {
            rig_debug(RIG_DEBUG_ERR, "Error response: '%s'\n", response);
        }

        return RIG_EPROTO;
    }

    // remove leading '='
    memmove(response, response + 1, strlen(response) - 1);
    response[strlen(response) - 1] = '\0';
    return RIG_OK;
}

uint32_t skywatcher_convert24bit(long input)
{
    return ((input & 0x0000FF) << 16) | (input & 0x00FF00) | ((
                input & 0xFF0000) >> 16);
}

static const char *skywatcher_get_info(ROT *rot)
{
    static char info[32];
    char str[16];

    rig_debug(RIG_DEBUG_TRACE, "%s called\n", __func__);

    // Read from 1st motor only. In practice both motors have the same version
    if (skywatcher_cmd(rot, ":e1\r", str, sizeof(str)) != RIG_OK)
    {
        return NULL;
    }

    SNPRINTF(info, sizeof(info), "V%s", str);

    return info;
}

int skywatcher_get_spr(ROT *rot, int motor_index, uint32_t *result)
{
    skywatcher_priv_data *priv = ROTSTATE(rot)->priv;

    if (priv->cpr[motor_index - 1] != 0)
    {
        *result = priv->cpr[motor_index - 1];
        return RIG_OK;
    }

    char str[16];
    char req[16];
    SNPRINTF(req, sizeof(req), ":a%d\r", motor_index);
    ERROR_CHECK(skywatcher_cmd(rot, req, str, sizeof(str)));
    priv->cpr[motor_index - 1] = skywatcher_convert24bit(strtol(str, NULL, 16));
    *result = priv->cpr[motor_index - 1];
    return RIG_OK;
}

int skywatcher_get_motor_position(ROT *rot, int motor_index, float *result)
{
    char str[16];
    char req[16];
    SNPRINTF(req, sizeof(req), ":j%d\r", motor_index);
    ERROR_CHECK(skywatcher_cmd(rot, req, str, sizeof(str)));
    long motor_ticks = skywatcher_convert24bit(strtol(str, NULL, 16)) ^ 0x800000;
    uint32_t cpr;
    ERROR_CHECK(skywatcher_get_spr(rot, motor_index, &cpr));
    double ticks_per_angle = (double) cpr / 360.0;
    *result = (float)((double) motor_ticks / ticks_per_angle);
    return RIG_OK;
}

static int skywatcher_get_position(ROT *rot, azimuth_t *az, elevation_t *el)
{
    rig_debug(RIG_DEBUG_TRACE, "%s called\n", __func__);

    ERROR_CHECK(skywatcher_get_motor_position(rot, 1, (float *) az));

    // normalize to 0 ~ 360
    if (*az < 0.0f)
    {
        *az = 360.0f + *az;
    }

    ERROR_CHECK(skywatcher_get_motor_position(rot, 2, (float *) el));

    rig_debug(RIG_DEBUG_TRACE, "%s: (az, el) = (%.1f, %.1f)\n", __func__, *az, *el);
    return RIG_OK;
}

static int skywatcher_stop(ROT *rot)
{
    rig_debug(RIG_DEBUG_TRACE, "%s called\n", __func__);
    char str[16];
    // don't wait for actual stop. Operation can be async
    ERROR_CHECK(skywatcher_cmd(rot, ":K1\r", str, sizeof(str)));
    ERROR_CHECK(skywatcher_cmd(rot, ":K2\r", str, sizeof(str)));
    return RIG_OK;
}

int skywatcher_set_motor_position(ROT *rot, int motor_index, float angle)
{
    char str[16];
    char req[16];
    hamlib_port_t *rotp = ROTPORT(rot);
    SNPRINTF(req, sizeof(req), ":f%d\r", motor_index);
    int current_retry = 0;
    int stopped = 0;

    while (current_retry < rotp->retry)
    {
        ERROR_CHECK(skywatcher_cmd(rot, req, str, sizeof(str)));
        int status = str[1] - '0';

        if (status & 0b10)
        {
            rig_debug(RIG_DEBUG_ERR, "%s: motor is blocked\n", __func__);
            return RIG_EPROTO;
        }

        if (status & 0b1)
        {
            current_retry++;
            //10ms sleep just to make sure motor is fully stopped
            hl_usleep(10000);
            continue;
        }

        stopped = 1;
        break;
    }

    if (!stopped)
    {
        return RIG_EPROTO;
    }

    SNPRINTF(req, sizeof(req), ":G%d00\r", motor_index);
    ERROR_CHECK(skywatcher_cmd(rot, req, str, sizeof(str)));
    uint32_t cpr;
    ERROR_CHECK(skywatcher_get_spr(rot, motor_index, &cpr));
    double ticks_per_angle = (double) cpr / 360.0;
    uint32_t value = ((uint32_t)(ticks_per_angle * angle)) & 0xFFFFFF;
    value = value | 0x800000;
    SNPRINTF(req, sizeof(req), ":S%d%02X%02X%02X\r", motor_index, (value & 0xFF),
             (value & 0xFF00) >> 8, (value & 0xFF0000) >> 16);
    ERROR_CHECK(skywatcher_cmd(rot, req, str, sizeof(str)));
    // start motion, don't wait until it completes
    SNPRINTF(req, sizeof(req), ":J%d\r", motor_index);
    ERROR_CHECK(skywatcher_cmd(rot, req, str, sizeof(str)));
    return RIG_OK;
}

static int skywatcher_set_position(ROT *rot, azimuth_t az, elevation_t el)
{
    rig_debug(RIG_DEBUG_TRACE, "%s called: %f %f\n", __func__, az, el);
    ERROR_CHECK(skywatcher_stop(rot));
    // stop is called for 1 then for 2 motor. do motor position in reverse because it awaits "stop motor" state.
    // potentially saving 1 call to :f
    ERROR_CHECK(skywatcher_set_motor_position(rot, 2, el));
    ERROR_CHECK(skywatcher_set_motor_position(rot, 1, az));
    return RIG_OK;
}

static int skywatcher_init(ROT *rot)
{
    rig_debug(RIG_DEBUG_VERBOSE, "%s called\n", __func__);
    skywatcher_priv_data *result = calloc(1, sizeof(skywatcher_priv_data));

    if (result == NULL)
    {
        return -RIG_ENOMEM;
    }

    memset(result->cpr, 0, sizeof(result->cpr));
    ROTSTATE(rot)->priv = result;
    return RIG_OK;
}

static int skywatcher_cleanup(ROT *rot)
{
    skywatcher_priv_data *priv = ROTSTATE(rot)->priv;

    if (priv == NULL)
    {
        return RIG_OK;
    }

    free(priv);
    return RIG_OK;
}

/* ************************************************************************* */
/*
 * Protocol documentation:
 *  https://inter-static.skywatcher.com/downloads/skywatcher_motor_controller_command_set.pdf
 */
const struct rot_caps skywatcher_rot_caps =
{
    ROT_MODEL(ROT_MODEL_SKYWATCHER),
    .model_name =     "Sky-Watcher",
    .mfg_name =       "Sky-Watcher",
    .version =        "20240825.0",
    .copyright =      "LGPL",
    .status =         RIG_STATUS_STABLE,
    .rot_type =       ROT_TYPE_AZEL,
    .port_type =      RIG_PORT_SERIAL,
    .serial_rate_min  = 9600,
    .serial_rate_max  = 9600,
    .serial_data_bits = 8,
    .serial_stop_bits = 1,
    .serial_parity    = RIG_PARITY_NONE,
    .serial_handshake = RIG_HANDSHAKE_NONE,
    .write_delay      = 0,
    .post_write_delay = 0,
    .timeout          = 3500, /* worst case scenario */
    .retry            = 1,

    .min_az =     0.0f,
    .max_az =     360.0f,
    .min_el =     0.0f,
    .max_el =     180.0f,

    .rot_init     = skywatcher_init,
    .rot_cleanup  = skywatcher_cleanup,
    .get_position = skywatcher_get_position,
    .set_position = skywatcher_set_position,
    .stop         = skywatcher_stop,
    .get_info     = skywatcher_get_info,
};


DECLARE_INITROT_BACKEND(skywatcher)
{
    rig_debug(RIG_DEBUG_VERBOSE, "%s called\n", __func__);
    rot_register(&skywatcher_rot_caps);
    return RIG_OK;
}
