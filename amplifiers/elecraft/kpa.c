/*
 *  Hamlib Elecraft amplifier backend - low level communication routines
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
#include "kpa.h"

struct fault_list
{
    int code;
    char *errmsg;
};
const struct fault_list kpa_fault_list [] =
{
    {0, "No fault condition"},
    {0x10, "Watchdog Timer was reset"},
    {0x20, "PA Current is too high"},
    {0x40, "Temperature is too high"},
    {0x60, "Input power is too high"},
    {0x61, "Gain is too low"},
    {0x70, "Invalid frequency"},
    {0x80, "50V supply voltage too low or too high"},
    {0x81, "5V supply voltage too low or too high"},
    {0x82, "10V supply voltage too low or too high"},
    {0x83, "12V supply voltage too low or too high"},
    {0x84, "-12V supply voltage too low or too high"},
    {0x85, "5V or 400V LPF board supply voltages not detected"},
    {0x90, "Reflected power is too high"},
    {0x91, "SWR very high"},
    {0x92, "ATU no match"},
    {0xB0, "Dissipated power too high"},
    {0xC0, "Forward power too high"},
    {0xE0, "Forward power too high for current setting"},
    {0xF0, "Gain is too high"},
    {0, NULL}
};

/*
 * Initialize data structures
 */

int kpa_init(AMP *amp)
{
    rig_debug(RIG_DEBUG_VERBOSE, "%s called\n", __func__);

    if (!amp)
    {
        return -RIG_EINVAL;
    }

    amp->state.priv = (struct kpa_priv_data *)
                      calloc(1, sizeof(struct kpa_priv_data));

    if (!amp->state.priv)
    {
        return -RIG_ENOMEM;
    }

    amp->state.ampport.type.rig = RIG_PORT_SERIAL;

    return RIG_OK;
}

int kpa_close(AMP *amp)
{
    rig_debug(RIG_DEBUG_VERBOSE, "%s called\n", __func__);

    if (amp->state.priv) { free(amp->state.priv); }

    amp->state.priv = NULL;

    return RIG_OK;
}

int kpa_flushbuffer(AMP *amp)
{
    struct amp_state *rs;

    rig_debug(RIG_DEBUG_VERBOSE, "%s called\n", __func__);

    rs = &amp->state;

    return rig_flush(&rs->ampport);
}

int kpa_transaction(AMP *amp, const char *cmd, char *response, int response_len)
{
    struct amp_state *rs;
    int err;
    int len = 0;
    int loop;

    rig_debug(RIG_DEBUG_VERBOSE, "%s called, cmd=%s\n", __func__, cmd);

    if (!amp) { return -RIG_EINVAL; }

    kpa_flushbuffer(amp);

    rs = &amp->state;

    loop = 3;

    do   // wake up the amp by sending ; until we receive ;
    {
        char c = ';';
        rig_debug(RIG_DEBUG_VERBOSE, "%s waiting for ;\n", __func__);
        err = write_block(&rs->ampport, (unsigned char *) &c, 1);

        if (err != RIG_OK) { return err; }

        len = read_string(&rs->ampport, (unsigned char *) response, response_len, ";",
                          1, 0, 1);

        if (len < 0) { return len; }
    }
    while (--loop > 0 && (len != 1 || response[0] != ';'));

    // Now send our command
    err = write_block(&rs->ampport, (unsigned char *) cmd, strlen(cmd));

    if (err != RIG_OK) { return err; }

    if (response) // if response expected get it
    {
        response[0] = 0;
        len = read_string(&rs->ampport, (unsigned char *) response, response_len, ";",
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
    else   // if no response expected try to get one
    {
        char responsebuf[KPABUFSZ];
        responsebuf[0] = 0;
        loop = 3;

        do
        {
            char c = ';';
            rig_debug(RIG_DEBUG_VERBOSE, "%s waiting for ;\n", __func__);
            err = write_block(&rs->ampport, (unsigned char *) &c, 1);

            if (err != RIG_OK) { return err; }

            len = read_string(&rs->ampport, (unsigned char *) responsebuf, KPABUFSZ, ";", 1,
                              0, 1);

            if (len < 0) { return len; }
        }
        while (--loop > 0 && (len != 1 || responsebuf[0] != ';'));
    }

    return RIG_OK;
}

/*
 * Get Info
 * returns the model name string
 */
const char *kpa_get_info(AMP *amp)
{
    const struct amp_caps *rc;

    rig_debug(RIG_DEBUG_VERBOSE, "%s called\n", __func__);

    if (!amp) { return (const char *) - RIG_EINVAL; }

    rc = amp->caps;

    return rc->model_name;
}

int kpa_get_freq(AMP *amp, freq_t *freq)
{
    char responsebuf[KPABUFSZ];
    int retval;
    unsigned long tfreq;
    int nargs;

    rig_debug(RIG_DEBUG_VERBOSE, "%s called\n", __func__);

    if (!amp) { return -RIG_EINVAL; }

    retval = kpa_transaction(amp, "^FR;", responsebuf, sizeof(responsebuf));

    if (retval != RIG_OK) { return retval; }

    nargs = sscanf(responsebuf, "^FR%lu", &tfreq);

    if (nargs != 1)
    {
        rig_debug(RIG_DEBUG_VERBOSE, "%s Error: ^FR response='%s'\n", __func__,
                  responsebuf);
        return -RIG_EPROTO;
    }

    *freq = tfreq * 1000;
    return RIG_OK;
}

int kpa_set_freq(AMP *amp, freq_t freq)
{
    char responsebuf[KPABUFSZ];
    int retval;
    unsigned long tfreq;
    int nargs;
    char cmd[KPABUFSZ];

    rig_debug(RIG_DEBUG_VERBOSE, "%s called, freq=%"PRIfreq"\n", __func__, freq);

    if (!amp) { return -RIG_EINVAL; }

    SNPRINTF(cmd, sizeof(cmd), "^FR%05ld;", (long)freq / 1000);
    retval = kpa_transaction(amp, cmd, NULL, 0);

    if (retval != RIG_OK) { return retval; }

    nargs = sscanf(responsebuf, "^FR%lu", &tfreq);

    if (nargs != 1)
    {
        rig_debug(RIG_DEBUG_ERR, "%s Error: ^FR response='%s'\n", __func__,
                  responsebuf);
        return -RIG_EPROTO;
    }

    if (tfreq * 1000 != freq)
    {
        rig_debug(RIG_DEBUG_ERR,
                  "%s Error setting freq: ^FR freq!=freq2, %f=%lu '%s'\n", __func__,
                  freq, tfreq * 1000, responsebuf);
        return -RIG_EPROTO;
    }

    return RIG_OK;
}

int kpa_get_level(AMP *amp, setting_t level, value_t *val)
{
    char responsebuf[KPABUFSZ];
    char *cmd;
    int retval;
    int fault;
    int i;
    int nargs;
    int antenna;
    int pwrpeak;
    int pwrref;
    int pwrfwd;
    int pwrinput;
    float float_value = 0;
    int int_value = 0, int_value2 = 0;
    struct amp_state *rs = &amp->state;
    struct kpa_priv_data *priv = amp->state.priv;


    rig_debug(RIG_DEBUG_VERBOSE, "%s called\n", __func__);

    // get the current antenna selected
    cmd = "^AE;";
    retval = kpa_transaction(amp, cmd, responsebuf, sizeof(responsebuf));

    if (retval != RIG_OK) { return retval; }

    antenna = 0;
    nargs = sscanf(responsebuf, "^AE%d", &antenna);

    if (nargs != 1)
    {
        rig_debug(RIG_DEBUG_ERR, "%s: invalid value %s='%s'\n", __func__, cmd,
                  responsebuf);
        return -RIG_EPROTO;
    }

    rig_debug(RIG_DEBUG_VERBOSE, "%s: cmd=%s, antenna=%d\n", __func__, cmd,
              antenna);

    switch (level)
    {
    case AMP_LEVEL_SWR:
        cmd = "^SW;";
        break;

    case AMP_LEVEL_NH:
        cmd = "^DF;";
        break;

    case AMP_LEVEL_PF:
        cmd = "^DF;";
        break;

    case AMP_LEVEL_PWR_INPUT:
        cmd = "^PWI;";
        break;

    case AMP_LEVEL_PWR_FWD:
        cmd = "^PWF;";
        break;

    case AMP_LEVEL_PWR_REFLECTED:
        cmd = "^PWR;";
        break;

    case AMP_LEVEL_PWR_PEAK:
        cmd = "^PWK;";
        break;

    case AMP_LEVEL_FAULT:
        cmd = "^SF;";
        break;
    }

    retval = kpa_transaction(amp, cmd, responsebuf, sizeof(responsebuf));

    if (retval != RIG_OK) { return retval; }

    switch (level)
    {
    case AMP_LEVEL_SWR:
        nargs = sscanf(responsebuf, "^SW%f", &float_value);

        if (nargs != 1)
        {
            rig_debug(RIG_DEBUG_ERR, "%s invalid value %s='%s'\n", __func__, cmd,
                      responsebuf);
            return -RIG_EPROTO;
        }

        val->f = float_value / 10.0f;
        return RIG_OK;

    case AMP_LEVEL_NH:
    case AMP_LEVEL_PF:
        nargs = sscanf(responsebuf, "^DF%d,%d", &int_value, &int_value2);

        if (nargs != 2)
        {
            rig_debug(RIG_DEBUG_ERR, "%s invalid value %s='%s'\n", __func__, cmd,
                      responsebuf);
            return -RIG_EPROTO;
        }

        rig_debug(RIG_DEBUG_VERBOSE, "%s freq range=%dKHz,%dKHz\n", __func__,
                  int_value, int_value2);

        //
        do
        {
            retval = read_string(&rs->ampport, (unsigned char *) responsebuf,
                                 sizeof(responsebuf), ";", 1, 0,
                                 1);

            if (retval != RIG_OK) { return retval; }

            if (strstr(responsebuf, "BYPASS") != 0)
            {
                int antenna2 = 0;
                nargs = sscanf(responsebuf, "AN%d Side TX %d %*s %*s %d", &antenna2, &int_value,
                               &int_value2);
                rig_debug(RIG_DEBUG_VERBOSE, "%s response='%s'\n", __func__, responsebuf);

                if (nargs != 3)
                {
                    rig_debug(RIG_DEBUG_ERR, "%s invalid value %s='%s'\n", __func__, cmd,
                              responsebuf);
                    return -RIG_EPROTO;
                }

                rig_debug(RIG_DEBUG_VERBOSE, "%s antenna=%d,nH=%d\n", __func__, antenna2,
                          int_value);

                val->i = level == AMP_LEVEL_NH ? int_value : int_value2;
                return RIG_OK;
            }
        }
        while (strstr(responsebuf, "BYPASS"));


        break;


    case AMP_LEVEL_PWR_INPUT:
        cmd = "^PWI;";
        nargs = sscanf(responsebuf, "^SW%d", &pwrinput);

        if (nargs != 1)
        {
            rig_debug(RIG_DEBUG_ERR, "%s invalid value %s='%s'\n", __func__, cmd,
                      responsebuf);
            return -RIG_EPROTO;
        }

        val->i = pwrinput;
        return RIG_OK;

        break;

    case AMP_LEVEL_PWR_FWD:
        cmd = "^PWF;";
        nargs = sscanf(responsebuf, "^SW%d", &pwrfwd);

        if (nargs != 1)
        {
            rig_debug(RIG_DEBUG_ERR, "%s invalid value %s='%s'\n", __func__, cmd,
                      responsebuf);
            return -RIG_EPROTO;
        }

        val->i = pwrfwd;
        return RIG_OK;

        break;

    case AMP_LEVEL_PWR_REFLECTED:
        cmd = "^PWR;";
        nargs = sscanf(responsebuf, "^SW%d", &pwrref);

        if (nargs != 1)
        {
            rig_debug(RIG_DEBUG_ERR, "%s invalid value %s='%s'\n", __func__, cmd,
                      responsebuf);
            return -RIG_EPROTO;
        }

        val->i = pwrref;
        return RIG_OK;

        break;

    case AMP_LEVEL_PWR_PEAK:
        cmd = "^PWK;";
        nargs = sscanf(responsebuf, "^SW%d", &pwrpeak);

        if (nargs != 1)
        {
            rig_debug(RIG_DEBUG_ERR, "%s invalid value %s='%s'\n", __func__, cmd,
                      responsebuf);
            return -RIG_EPROTO;
        }

        val->i = pwrpeak;
        return RIG_OK;

        break;

    case AMP_LEVEL_FAULT:
        cmd = "^SF;";
        nargs = sscanf(responsebuf, "^SW%d", &fault);

        if (nargs != 1)
        {
            rig_debug(RIG_DEBUG_ERR, "%s invalid value %s='%s'\n", __func__, cmd,
                      responsebuf);
            return -RIG_EPROTO;
        }

        for (i = 0; kpa_fault_list[i].errmsg != NULL; ++i)
        {
            if (kpa_fault_list[i].code == fault)
            {
                val->s =  kpa_fault_list[i].errmsg;
                return RIG_OK;
            }
        }

        rig_debug(RIG_DEBUG_ERR, "%s unknown fault from %s\n", __func__, responsebuf);
        SNPRINTF(priv->tmpbuf, sizeof(priv->tmpbuf), "Unknown fault code=0x%02x",
                 fault);
        val->s = priv->tmpbuf;
        return RIG_OK;

        break;

    default:
        rig_debug(RIG_DEBUG_ERR, "%s unknown level=%s\n", __func__,
                  rig_strlevel(level));

    }

    return -RIG_EINVAL;
}

int kpa_get_powerstat(AMP *amp, powerstat_t *status)
{
    char responsebuf[KPABUFSZ];
    int retval;
    int operate;
    int ampon;
    int nargs;


    rig_debug(RIG_DEBUG_VERBOSE, "%s called\n", __func__);

    *status = RIG_POWER_UNKNOWN;

    if (!amp) { return -RIG_EINVAL; }

    retval = kpa_transaction(amp, "^ON;", responsebuf, sizeof(responsebuf));

    if (retval != RIG_OK) { return retval; }

    nargs = sscanf(responsebuf, "^ON%d", &ampon);

    if (nargs != 1)
    {
        rig_debug(RIG_DEBUG_VERBOSE, "%s Error: ^ON response='%s'\n", __func__,
                  responsebuf);
        return -RIG_EPROTO;
    }

    switch (ampon)
    {
    case 0: *status = RIG_POWER_OFF; return RIG_OK;

    case 1: *status = RIG_POWER_ON; break;

    default:
        rig_debug(RIG_DEBUG_VERBOSE, "%s Error: ^ON unknown response='%s'\n", __func__,
                  responsebuf);
        return -RIG_EPROTO;
    }

    retval = kpa_transaction(amp, "^OP;", responsebuf, sizeof(responsebuf));

    if (retval != RIG_OK) { return retval; }

    nargs = sscanf(responsebuf, "^ON%d", &operate);

    if (nargs != 1)
    {
        rig_debug(RIG_DEBUG_VERBOSE, "%s Error: ^ON response='%s'\n", __func__,
                  responsebuf);
        return -RIG_EPROTO;
    }

    *status = operate == 1 ? RIG_POWER_OPERATE : RIG_POWER_STANDBY;

    return RIG_OK;
}

int kpa_set_powerstat(AMP *amp, powerstat_t status)
{
    int retval;
    char *cmd = NULL;

    rig_debug(RIG_DEBUG_VERBOSE, "%s called\n", __func__);

    if (!amp) { return -RIG_EINVAL; }

    switch (status)
    {
    case RIG_POWER_UNKNOWN: break;

    case RIG_POWER_OFF: cmd = "^ON0;"; break;

    case RIG_POWER_ON:  cmd = "^ON1;"; break;

    case RIG_POWER_OPERATE: cmd = "^OS1;"; break;

    case RIG_POWER_STANDBY: cmd = "^OS0;"; break;


    default:
        rig_debug(RIG_DEBUG_ERR, "%s invalid status=%d\n", __func__, status);

    }

    retval = kpa_transaction(amp, cmd, NULL, 0);

    if (retval != RIG_OK) { return retval; }

    return RIG_OK;
}

int kpa_reset(AMP *amp, amp_reset_t reset)
{
    int retval;

    rig_debug(RIG_DEBUG_VERBOSE, "%s called\n", __func__);

    // toggling from standby to operate supposed to reset
    retval = kpa_set_powerstat(amp, RIG_POWER_STANDBY);

    if (retval != RIG_OK)
    {
        rig_debug(RIG_DEBUG_ERR, "%s: error setting RIG_POWER_STANDBY '%s'\n", __func__,
                  strerror(retval));

    }

    return  kpa_set_powerstat(amp, RIG_POWER_OPERATE);
}


