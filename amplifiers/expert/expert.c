/*
 *  Hamlib Expert amplifier backend - low level communication routines
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

#include <stdlib.h>
#include <string.h>
#include "expert.h"
#include "register.h"
#include "misc.h"

struct fault_list
{
    int code;
    char *errmsg;
};
const struct fault_list expert_fault_list [] =
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

int expert_init(AMP *amp)
{
    rig_debug(RIG_DEBUG_VERBOSE, "%s called\n", __func__);

    if (!amp)
    {
        return -RIG_EINVAL;
    }

    amp->state.priv = (struct expert_priv_data *)
                      calloc(1, sizeof(struct expert_priv_data));

    if (!amp->state.priv)
    {
        return -RIG_ENOMEM;
    }

    amp->state.ampport.type.rig = RIG_PORT_SERIAL;

    return RIG_OK;
}

int expert_close(AMP *amp)
{
    rig_debug(RIG_DEBUG_VERBOSE, "%s called\n", __func__);

    if (amp->state.priv) { free(amp->state.priv); }

    amp->state.priv = NULL;

    return RIG_OK;
}

int expert_flushbuffer(AMP *amp)
{
    struct amp_state *rs;

    rig_debug(RIG_DEBUG_VERBOSE, "%s called\n", __func__);

    rs = &amp->state;

    return rig_flush(&rs->ampport);
}

int expert_transaction(AMP *amp, const unsigned char *cmd, int cmd_len, unsigned char *response, int response_len)
{
    struct amp_state *rs;
    int err;
    int len = 0;
    int loop;
    char cmdbuf[64];
    int checksum=0;
    int bytes = 0;

    rig_debug(RIG_DEBUG_VERBOSE, "%s called, cmd=%s\n", __func__, cmd);

    if (!amp) { return -RIG_EINVAL; }

    expert_flushbuffer(amp);

    rs = &amp->state;

    cmdbuf[0] = cmdbuf[1] = cmdbuf[2] = 0x55;
    memcpy(&cmdbuf,cmd,cmd_len);
    for(int i=0;i<cmd_len;++i) checksum += cmd[i];
    checksum = checksum % 256;
    cmdbuf[3] = cmd_len;
    cmdbuf[3+cmd_len+1] = checksum;

    // Now send our command
    err = write_block(&rs->ampport, (unsigned char *) cmdbuf, 3+cmd_len+2);

    if (err != RIG_OK) { return err; }

    if (response) // if response expected get it
    {
        response[0] = 0;
        // read the 4-byte header x55x55x55xXX where XX is the hex # of bytes
        len = read_block_direct(&rs->ampport, (unsigned  char *) response, 4);
        rig_debug(RIG_DEBUG_ERR, "%s: len=%d, bytes=%02x\n", __func__, len, response[3]);

        if (len < 0)
        {
            rig_debug(RIG_DEBUG_VERBOSE, "%s: error=%s\n", __func__,
                      rigerror(len));
            return len;
        }
        if (len == 4) bytes = response[3];
        rig_debug(RIG_DEBUG_ERR, "%s: bytes=%d\n", __func__, bytes);
        len = read_block_direct(&rs->ampport, (unsigned  char *) response, bytes-3 );
        dump_hex(response,len);
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
const char *expert_get_info(AMP *amp)
{
    const struct amp_caps *rc;

    rig_debug(RIG_DEBUG_VERBOSE, "%s called\n", __func__);

    if (!amp) { return (const char *) - RIG_EINVAL; }

    rc = amp->caps;

    return rc->model_name;
}

int expert_get_freq(AMP *amp, freq_t *freq)
{
    char responsebuf[KPABUFSZ];
    int retval;
    unsigned long tfreq;
    int nargs;

    rig_debug(RIG_DEBUG_VERBOSE, "%s called\n", __func__);

    if (!amp) { return -RIG_EINVAL; }

    retval = expert_transaction(amp, NULL,0, NULL, sizeof(responsebuf));

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

int expert_set_freq(AMP *amp, freq_t freq)
{
    char responsebuf[KPABUFSZ];
    int retval;
    unsigned long tfreq;
    int nargs;
    unsigned char cmd[KPABUFSZ];

    rig_debug(RIG_DEBUG_VERBOSE, "%s called, freq=%"PRIfreq"\n", __func__, freq);

    if (!amp) { return -RIG_EINVAL; }

//    SNPRINTF(cmd, sizeof(cmd), "^FR%05ld;", (long)freq / 1000);
    retval = expert_transaction(amp, cmd, 0, NULL, 0);

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

int expert_get_level(AMP *amp, setting_t level, value_t *val)
{
    char responsebuf[KPABUFSZ];
    unsigned char cmd[8];
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
    struct expert_priv_data *priv = amp->state.priv;


    rig_debug(RIG_DEBUG_VERBOSE, "%s called\n", __func__);

    // get the current antenna selected
    cmd[0] = 0x00;
    retval = expert_transaction(amp, cmd, 0, NULL, sizeof(responsebuf));

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
        cmd[0] = 0x00;
        break;

    case AMP_LEVEL_NH:
        cmd[0] = 0x00;
        break;

    case AMP_LEVEL_PF:
        cmd[0] = 0x00;
        break;

    case AMP_LEVEL_PWR_INPUT:
        cmd[0] = 0x00;
        break;

    case AMP_LEVEL_PWR_FWD:
        cmd[0] = 0x00;
        break;

    case AMP_LEVEL_PWR_REFLECTED:
        cmd[0] = 0x00;
        break;

    case AMP_LEVEL_PWR_PEAK:
        cmd[0] = 0x00;
        break;

    case AMP_LEVEL_FAULT:
        cmd[0] = 0x00;
        break;
    }

    retval = expert_transaction(amp, cmd, 0, NULL, sizeof(responsebuf));

    if (retval != RIG_OK) { return retval; }

    switch (level)
    {
    case AMP_LEVEL_SWR:
        //nargs = sscanf(responsebuf, "^SW%f", &float_value);

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
        //nargs = sscanf(responsebuf, "^DF%d,%d", &int_value, &int_value2);

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
        cmd[0] = 0x00;
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
        cmd[0] = 0x00;
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
        cmd[0] = 0x00;
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
        cmd[0] = 0x00;
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
        cmd[0] = 0x00;
        nargs = sscanf(responsebuf, "^SW%d", &fault);

        if (nargs != 1)
        {
            rig_debug(RIG_DEBUG_ERR, "%s invalid value %s='%s'\n", __func__, cmd,
                      responsebuf);
            return -RIG_EPROTO;
        }

        for (i = 0; expert_fault_list[i].errmsg != NULL; ++i)
        {
            if (expert_fault_list[i].code == fault)
            {
                val->s =  expert_fault_list[i].errmsg;
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

int expert_get_powerstat(AMP *amp, powerstat_t *status)
{
    unsigned char responsebuf[KPABUFSZ];
    int retval;
    int operate = 0;
    int ampon = 0;
    int nargs = 0;


    rig_debug(RIG_DEBUG_VERBOSE, "%s called\n", __func__);

    *status = RIG_POWER_UNKNOWN;

    if (!amp) { return -RIG_EINVAL; }

    retval = expert_transaction(amp, NULL, 0, responsebuf, sizeof(responsebuf));

    if (retval != RIG_OK) { return retval; }

    //nargs = sscanf(responsebuf, "^ON%d", &ampon);

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

    retval = expert_transaction(amp, NULL, 0, responsebuf, sizeof(responsebuf));

    if (retval != RIG_OK) { return retval; }

    //nargs = sscanf(responsebuf, "^ON%d", &operate);

    if (nargs != 1)
    {
        rig_debug(RIG_DEBUG_VERBOSE, "%s Error: ^ON response='%s'\n", __func__,
                  responsebuf);
        return -RIG_EPROTO;
    }

    *status = operate == 1 ? RIG_POWER_OPERATE : RIG_POWER_STANDBY;

    return RIG_OK;
}

int expert_set_powerstat(AMP *amp, powerstat_t status)
{
    int retval;
    unsigned char cmd[8];
    int cmd_len = 1;

    rig_debug(RIG_DEBUG_VERBOSE, "%s called\n", __func__);

    if (!amp) { return -RIG_EINVAL; }

    switch (status)
    {
    case RIG_POWER_UNKNOWN: break;

    case RIG_POWER_OFF: cmd[0] = 0x0a; break;

    case RIG_POWER_ON:  cmd[0] = 0x0b; break;

    case RIG_POWER_OPERATE: cmd[0] = 0x0d; break;

    case RIG_POWER_STANDBY: cmd[0] = 0x0a; break;


    default:
        rig_debug(RIG_DEBUG_ERR, "%s invalid status=%d\n", __func__, status);

    }

    retval = expert_transaction(amp, cmd, cmd_len, NULL, 0);

    if (retval != RIG_OK) { return retval; }

    return RIG_OK;
}

int expert_reset(AMP *amp, amp_reset_t reset)
{
    int retval;

    rig_debug(RIG_DEBUG_VERBOSE, "%s called\n", __func__);

    // toggling from standby to operate supposed to reset
    retval = expert_set_powerstat(amp, RIG_POWER_STANDBY);

    if (retval != RIG_OK)
    {
        rig_debug(RIG_DEBUG_ERR, "%s: error setting RIG_POWER_STANDBY '%s'\n", __func__,
                  strerror(retval));

    }

    return  expert_set_powerstat(amp, RIG_POWER_OPERATE);
}


struct expert_priv_data *expert_priv;
/*
 * API local implementation
 *
 */

/*
 * Private helper function prototypes
 */

/* *************************************
 *
 * Separate model capabilities
 *
 * *************************************
 */


/*
 * Expert 1.3K-FA, 1.5K-FA, and 2K-FA
 */

const struct amp_caps expert_amp_caps =
{
    AMP_MODEL(AMP_MODEL_EXPERT_FA),
    .model_name =   "1.3K-FA/1.5K-FA/2K-FA",
    .mfg_name =     "Expert",
    .version =      "20230320.0",
    .copyright =    "LGPL",
    .status =     RIG_STATUS_ALPHA,
    .amp_type =     AMP_TYPE_OTHER,
    .port_type =    RIG_PORT_SERIAL,
    .serial_rate_min =  9600,
    .serial_rate_max =  115200,
    .serial_data_bits = 8,
    .serial_stop_bits = 1,
    .serial_parity =  RIG_PARITY_NONE,
    .serial_handshake = RIG_HANDSHAKE_NONE,
    .write_delay =    0,
    .post_write_delay = 0,
    .timeout =      2000,
    .retry =      2,
    .has_get_level = AMP_LEVEL_SWR | AMP_LEVEL_NH | AMP_LEVEL_PF | AMP_LEVEL_PWR_INPUT | AMP_LEVEL_PWR_FWD | AMP_LEVEL_PWR_REFLECTED | AMP_LEVEL_FAULT,
    .has_set_level = 0,

    .amp_open = NULL,
    .amp_init = expert_init,
    .amp_close = expert_close,
    .reset = expert_reset,
    .get_info = expert_get_info,
    .get_powerstat = expert_get_powerstat,
    .set_powerstat = expert_set_powerstat,
    .set_freq = expert_set_freq,
    .get_freq = expert_get_freq,
    .get_level = expert_get_level,
};


/* ************************************
 *
 * API functions
 *
 * ************************************
 */

/*
 *
 */

#if 0 // not implemented yet
/*
 * Send command string to amplifier
 */

static int expert_send_priv_cmd(AMP *amp, const char *cmdstr)
{
    struct amp_state *rs;
    int err;

    rig_debug(RIG_DEBUG_VERBOSE, "%s called\n", __func__);

    if (!amp)
    {
        return -RIG_EINVAL;
    }

    rs = &amp->state;
    err = write_block(&rs->ampport, cmdstr, strlen(cmdstr));

    if (err != RIG_OK)
    {
        return err;
    }

    return RIG_OK;
}
#endif

/*
 * Initialize backend
 */

DECLARE_INITAMP_BACKEND(expert)
{
    rig_debug(RIG_DEBUG_VERBOSE, "%s called\n", __func__);

    amp_register(&expert_amp_caps);

    return RIG_OK;
}
