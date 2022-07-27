/*
 *  Hamlib Gemini amplifier backend - low level communication routines
 *  Copyright (c) 2019 by Michael Black W9MDB
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
#include <string.h>
#include "misc.h"
#include "gemini.h"

struct fault_list
{
    int code;
    char *errmsg;
};

/*
 * Initialize data structures
 */

int gemini_init(AMP *amp)
{
    rig_debug(RIG_DEBUG_VERBOSE, "%s called\n", __func__);

    if (!amp)
    {
        return -RIG_EINVAL;
    }

    amp->state.priv = (struct gemini_priv_data *)
                      calloc(1, sizeof(struct gemini_priv_data));

    if (!amp->state.priv)
    {
        return -RIG_ENOMEM;
    }

    amp->state.ampport.type.rig = RIG_PORT_NETWORK;

    return RIG_OK;
}

int gemini_close(AMP *amp)
{
    rig_debug(RIG_DEBUG_VERBOSE, "%s called\n", __func__);

    if (amp->state.priv) { free(amp->state.priv); }

    amp->state.priv = NULL;

    return RIG_OK;
}

int gemini_flushbuffer(AMP *amp)
{
    struct amp_state *rs;

    rig_debug(RIG_DEBUG_VERBOSE, "%s called\n", __func__);

    rs = &amp->state;

    return rig_flush(&rs->ampport);
}

int gemini_transaction(AMP *amp, const char *cmd, char *response,
                       int response_len)
{

    struct amp_state *rs;
    int err;
    int len = 0;

    rig_debug(RIG_DEBUG_VERBOSE, "%s called, cmd=%s\n", __func__, cmd);

    if (!amp) { return -RIG_EINVAL; }

    gemini_flushbuffer(amp);

    rs = &amp->state;

    // Now send our command
    err = write_block(&rs->ampport, (unsigned char *) cmd, strlen(cmd));

    if (err != RIG_OK) { return err; }

    if (response) // if response expected get it
    {
        response[0] = 0;
        len = read_string(&rs->ampport, (unsigned char *) response, response_len, "\n",
                          1, 0, 1);

        if (len < 0)
        {
            rig_debug(RIG_DEBUG_VERBOSE, "%s called, error=%s\n", __func__,
                      rigerror(len));
            return len;
        }

        rig_debug(RIG_DEBUG_VERBOSE, "%s called, response='%s'\n", __func__,
                  response);
    }

    return RIG_OK;
}

/*
 * Get Info
 * returns the model name string
 */
const char *gemini_get_info(AMP *amp)
{
    const struct amp_caps *rc;

    rig_debug(RIG_DEBUG_VERBOSE, "%s called\n", __func__);

    if (!amp) { return (const char *) - RIG_EINVAL; }

    rc = amp->caps;

    return rc->model_name;
}

int gemini_status_parse(AMP *amp)
{
    int retval, n = 0;
    char *p;
    char responsebuf[GEMINIBUFSZ];
    struct gemini_priv_data *priv = amp->state.priv;

    retval = gemini_transaction(amp, "S\n", responsebuf, sizeof(responsebuf));

    if (retval != RIG_OK)
    {
        rig_debug(RIG_DEBUG_ERR, "%s: error sending command 'S'\n", __func__);
    }

    p = strtok(responsebuf, ",\n");
    rig_debug(RIG_DEBUG_VERBOSE, "%s: responsebuf=%s\n", __func__, responsebuf);

    while (p)
    {
        char tmp[8];
        double freq;
        n += sscanf(p, "BAND=%lf%s", &freq, tmp);

        if (tmp[0] == 'K') { priv->band = freq * 1000; }

        if (tmp[0] == 'M') { priv->band = freq * 1000000; }

        n += sscanf(p, "ANTENNA=%c", &priv->antenna);
        n += sscanf(p, "POWER=%dW%d", &priv->power_current, &priv->power_peak);
        n += sscanf(p, "VSWR=%lf", &priv->vswr);
        n += sscanf(p, "CURRENT=%d", &priv->current);
        n += sscanf(p, "TEMPERATURE=%d", &priv->temperature);
        n += sscanf(p, "STATE=%s", priv->state);
        n += sscanf(p, "PTT=%s", tmp);
        priv->ptt = tmp[0] == 'T';
        n += sscanf(p, "TRIP=%s", priv->trip);

        if (n == 0)
        {
            rig_debug(RIG_DEBUG_ERR, "%s: unknown status item=%s\n", __func__, p);
        }
    }

    if (n == 0) { return -RIG_EPROTO; }

    return RIG_OK;
}

int gemini_get_freq(AMP *amp, freq_t *freq)
{
    int retval;
    struct gemini_priv_data *priv = amp->state.priv;

    rig_debug(RIG_DEBUG_VERBOSE, "%s called\n", __func__);

    if (!amp) { return -RIG_EINVAL; }

    retval = gemini_status_parse(amp);

    if (retval != RIG_OK) { return retval; }

    *freq = priv->band;
    return RIG_OK;
}

int gemini_set_freq(AMP *amp, freq_t freq)
{
    int retval;
    char *cmd;

    rig_debug(RIG_DEBUG_VERBOSE, "%s called\n", __func__);

    if (freq < 1.0) { cmd = "B472KHZ\n"; }
    else if (freq < 2.0) { cmd = "B1.8MHZ\n"; }
    else if (freq < 4.0) { cmd = "B3.5MHZ\n"; }
    else if (freq < 6.0) { cmd = "B50MHZ\n"; }
    else if (freq < 9.0) { cmd = "B70MHZ\n"; }
    else if (freq < 12.0) { cmd = "B10MHZ\n"; }
    else if (freq < 16.0) { cmd = "B14MHZ\n"; }
    else if (freq < 19.0) { cmd = "B18MHZ\n"; }
    else if (freq < 22.0) { cmd = "B21MHZ\n"; }
    else if (freq < 26.0) { cmd = "B24MHZ\n"; }
    else { cmd = "B50MHZ\n"; }

    retval = gemini_transaction(amp, cmd, NULL, 0);

    if (retval != RIG_OK) { return retval; }

    return RIG_OK;
}

int gemini_get_level(AMP *amp, setting_t level, value_t *val)
{
    int retval;
    struct gemini_priv_data *priv = amp->state.priv;


    rig_debug(RIG_DEBUG_VERBOSE, "%s called\n", __func__);

    retval = gemini_status_parse(amp);

    if (retval != RIG_OK) { return retval; }

    switch (level)
    {
    case AMP_LEVEL_SWR:
        val->f = priv->vswr;
        return RIG_OK;

    case AMP_LEVEL_PWR_FWD:
        val->i = priv->power_peak;
        return RIG_OK;

    case AMP_LEVEL_PWR_PEAK:
        val->i = priv->power_peak;
        return RIG_OK;

    case AMP_LEVEL_FAULT:
        val->s = priv->trip;
        return RIG_OK;

    default:
        rig_debug(RIG_DEBUG_ERR, "%s unknown level=%s\n", __func__,
                  rig_strlevel(level));

    }

    return -RIG_EINVAL;
}

int gemini_set_level(AMP *amp, setting_t level, value_t val)
{
    char *cmd = "?";
    int retval;

    rig_debug(RIG_DEBUG_VERBOSE, "%s called\n", __func__);

    switch (level)
    {
    case AMP_LEVEL_PWR: // 0-.33 = low, .34-.67 = medium, .68-1.0 = high
        cmd = "PH\n";

        if (val.f < .33) { cmd = "PL\n"; }

        if (val.f < .67) { cmd = "PM\n"; }

        return RIG_OK;
        break;
    }

    retval = gemini_transaction(amp, cmd, NULL, 0);

    if (retval != RIG_OK) { return retval; }

    rig_debug(RIG_DEBUG_ERR, "%s: Unknown level=%s\n", __func__,
              rig_strlevel(level));
    return -RIG_EINVAL;

}

int gemini_get_powerstat(AMP *amp, powerstat_t *status)
{
    char responsebuf[GEMINIBUFSZ];
    int retval;
    int ampon;
    int nargs;


    rig_debug(RIG_DEBUG_VERBOSE, "%s called\n", __func__);

    *status = RIG_POWER_UNKNOWN;

    if (!amp) { return -RIG_EINVAL; }

    retval = gemini_transaction(amp, "R\n", responsebuf, sizeof(responsebuf));

    if (retval != RIG_OK) { return retval; }

    nargs = sscanf(responsebuf, "%d", &ampon);

    if (nargs != 1)
    {
        rig_debug(RIG_DEBUG_VERBOSE, "%s Error: ^ON response='%s'\n", __func__,
                  responsebuf);
        return -RIG_EPROTO;
    }

    switch (ampon)
    {
    case 0: *status = RIG_POWER_STANDBY; return RIG_OK;

    case 1: *status = RIG_POWER_ON; break;

    default:
        rig_debug(RIG_DEBUG_VERBOSE, "%s Error: 'R' unknown response='%s'\n", __func__,
                  responsebuf);
        return -RIG_EPROTO;
    }

    return RIG_OK;
}

int gemini_set_powerstat(AMP *amp, powerstat_t status)
{
    int retval;
    char *cmd = NULL;

    rig_debug(RIG_DEBUG_VERBOSE, "%s called\n", __func__);

    if (!amp) { return -RIG_EINVAL; }

    switch (status)
    {
    case RIG_POWER_UNKNOWN: break;

    case RIG_POWER_OFF: cmd = "R0\n"; break;

    case RIG_POWER_ON:  cmd = "LP1\n"; break;

    case RIG_POWER_OPERATE: cmd = "R1\n"; break;

    case RIG_POWER_STANDBY: cmd = "R0\n"; break;


    default:
        rig_debug(RIG_DEBUG_ERR, "%s invalid status=%d\n", __func__, status);

    }

    retval = gemini_transaction(amp, cmd, NULL, 0);

    if (retval != RIG_OK) { return retval; }

    return RIG_OK;
}

int gemini_reset(AMP *amp, amp_reset_t reset)
{
    int retval;

    rig_debug(RIG_DEBUG_VERBOSE, "%s called\n", __func__);

    retval = gemini_transaction(amp, "T\n", NULL, 0);

    if (retval != RIG_OK)
    {
        rig_debug(RIG_DEBUG_ERR, "%s: error setting RIG_POWER_STANDBY '%s'\n", __func__,
                  strerror(retval));
    }

    // toggling from standby to operate perhaps does a reset
    retval = gemini_set_powerstat(amp, RIG_POWER_STANDBY);

    if (retval != RIG_OK)
    {
        rig_debug(RIG_DEBUG_ERR, "%s: error setting RIG_POWER_STANDBY '%s'\n", __func__,
                  strerror(retval));
    }

    return  gemini_set_powerstat(amp, RIG_POWER_OPERATE);
}
