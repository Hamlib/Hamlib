/*
 *  Hamlib Kenwood backend - Elecraft XG3 description
 *  Copyright (c) 2002-2009 by Stephane Fillod
 *  Copyright (c) 2010 by Nate Bargmann, n0nb@arrl.net
 *  Copyright (c) 2014 by Michael BlacK, W9MDB, mdblack98@yahoo.com
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
 *  See the file 'COPYING.LIB' in the main Hamlib distribution directory for
 *  the complete text of the GNU Lesser Public License version 2.1.
 *
 */

#include <hamlib/config.h>

#include <stdlib.h>
#include <string.h>

#include <hamlib/rig.h>
#include "serial.h"
#include "kenwood.h"
#include "elecraft.h"


#define XG3_LEVEL_ALL (RIG_LEVEL_RFPOWER)

#define XG3_PARM_ALL (RIG_PARM_BACKLIGHT)

#define XG3_VFO (RIG_VFO_A|RIG_VFO_MEM)

#define XG3_CHANNEL_CAPS { \
        .freq=1\
        }


#define NB_CHAN 12              /* see caps->chan_list */

struct xg3_priv_data
{
    /* current vfo already in rig_state ? */
    vfo_t last_vfo;
    ptt_t ptt;
    powerstat_t powerstat;
    value_t parms[RIG_SETTING_MAX];

    channel_t *curr;            /* points to vfo_a, vfo_b or mem[] */

    channel_t vfo_a;
    channel_t mem[NB_CHAN];

    char *magic_conf;
};

/* kenwood_transaction() will add this to command strings
 * sent to the rig and remove it from strings returned from
 * the rig, so no need to append ';' manually to command strings.
 */
static struct kenwood_priv_caps xg3_priv_caps =
{
    .cmdtrm = EOM_KEN,
};


/* XG3 specific rig_caps API function declarations */
int xg3_init(RIG *rig);
int xg3_open(RIG *rig);
int xg3_set_vfo(RIG *rig, vfo_t vfo);
int xg3_get_vfo(RIG *rig, vfo_t *vfo);
int xg3_set_freq(RIG *rig, vfo_t vfo, freq_t freq);
int xg3_get_freq(RIG *rig, vfo_t vfo, freq_t *freq);
int xg3_get_ptt(RIG *rig, vfo_t vfo, ptt_t *ptt);
int xg3_set_ptt(RIG *rig, vfo_t vfo, ptt_t ptt);
int xg3_set_level(RIG *rig, vfo_t vfo, setting_t level, value_t val);
int xg3_get_level(RIG *rig, vfo_t vfo, setting_t level, value_t *val);
int xg3_set_powerstat(RIG *rig, powerstat_t status);
int xg3_get_powerstat(RIG *rig, powerstat_t *status);
int xg3_set_mem(RIG *rig, vfo_t vfo, int ch);
int xg3_get_mem(RIG *rig, vfo_t vfo, int *ch);
int xg3_set_parm(RIG *rig, setting_t parm, value_t val);
int xg3_get_parm(RIG *rig, setting_t parm, value_t *val);


/*
 * XG3 rig capabilities.
 * This kit can recognize a small subset of TS-570 commands.
 *
 * Part of info comes from http://www.elecraft.com/K2_Manual_Download_Page.htm#K2
 * look for KIO2 Programmer's Reference PDF
 */
const struct rig_caps xg3_caps =
{
    RIG_MODEL(RIG_MODEL_XG3),
    .model_name = "XG3",
    .mfg_name = "Elecraft",
    .version = "20200613.0",
    .copyright = "LGPL",
    .status = RIG_STATUS_STABLE,
    .rig_type = RIG_TYPE_TRANSCEIVER,
    .ptt_type = RIG_PTT_RIG,
    .dcd_type = RIG_DCD_RIG,
    .port_type = RIG_PORT_SERIAL,
    .serial_rate_min = 9600,
    .serial_rate_max = 9600,
    .serial_data_bits = 8,
    .serial_stop_bits = 1,
    .serial_parity = RIG_PARITY_NONE,
    .serial_handshake = RIG_HANDSHAKE_NONE,
    .write_delay = 0,           /* Timing between bytes */
    .post_write_delay = 25,     /* Timing between command strings */
    .timeout = 25,
    .retry = 3,

    .has_get_level = XG3_LEVEL_ALL,
    .has_set_level = RIG_LEVEL_SET(XG3_LEVEL_ALL),
    .has_get_parm = XG3_PARM_ALL,
    .has_set_parm = XG3_PARM_ALL,
    .level_gran = {},           /* FIXME: granularity */
    .parm_gran = {},
    .extparms = kenwood_cfg_params,
    .max_ifshift = Hz(0),
    .targetable_vfo = RIG_TARGETABLE_FREQ,
    .transceive = RIG_TRN_RIG,
    .bank_qty = 0,
    .chan_desc_sz = 0,

    .chan_list = {
        {0, 11, RIG_MTYPE_MEM, XG3_CHANNEL_CAPS},
        RIG_CHAN_END,
    },
    .tx_range_list1 =  { // lo power is actually 5e-14W which can't be represented in hamlib as of 20200325
        {kHz(1500), MHz(200), RIG_MODE_CW, 0,  2, RIG_VFO_A, RIG_ANT_1},
        RIG_FRNG_END,
    },
    .tuning_steps =  {
        {RIG_MODE_ALL, 0},
        RIG_TS_END
    },
    .filters =  {
        {RIG_MODE_ALL, RIG_FLT_ANY},
        RIG_FLT_END
    },



    .priv = (void *)& xg3_priv_caps,

    .rig_init = xg3_init,
    .rig_cleanup = kenwood_cleanup,
    .rig_open = xg3_open,
    .set_freq = xg3_set_freq,
    .get_freq = xg3_get_freq,
    .set_mem = xg3_set_mem,
    .get_mem = xg3_get_mem,
    .set_vfo = xg3_set_vfo,
    .get_vfo = xg3_get_vfo,
    .get_ptt = xg3_get_ptt,
    .set_ptt = xg3_set_ptt,
    .set_level = xg3_set_level,
    .get_level = xg3_get_level,
    .set_powerstat = xg3_set_powerstat,
    .get_powerstat = xg3_get_powerstat,
    .set_parm = xg3_set_parm,
    .get_parm = xg3_get_parm,
    .hamlib_check_rig_caps = HAMLIB_CHECK_RIG_CAPS
};


/*
 * xg3_init()
 */
int xg3_init(RIG *rig)
{
    struct xg3_priv_data *priv;
    int i;

    rig_debug(RIG_DEBUG_VERBOSE, "%s called\n", __func__);

    priv = (struct xg3_priv_data *)calloc(1, sizeof(struct xg3_priv_data));

    if (!priv)
    {
        return -RIG_ENOMEM;
    }

    rig->state.priv = (void *)priv;
    rig->state.rigport.type.rig = RIG_PORT_SERIAL;
// Tried set_trn to turn transceiver on/off but turning it on isn't enabled in hamlib for some reason
// So we use PTT instead
//  rig->state.transceive = RIG_TRN_RIG; // this allows xg3_set_trn to be called
    rig->state.current_vfo = RIG_VFO_A;
    priv->last_vfo = RIG_VFO_A;
    priv->ptt = RIG_PTT_ON;
    priv->powerstat = RIG_POWER_ON;
    memset(priv->parms, 0, RIG_SETTING_MAX * sizeof(value_t));

    for (i = 0; i < NB_CHAN; i++)
    {
        priv->mem[i].channel_num = i;
        priv->mem[i].vfo = RIG_VFO_MEM;
    }

    return RIG_OK;
}

/*
 * XG3 extension function definitions follow
 */

/*
 * xg3_open()
 */
int xg3_open(RIG *rig)
{
    int err;
    ptt_t ptt;

    rig_debug(RIG_DEBUG_VERBOSE, "%s called\n", __func__);

    err = elecraft_open(rig);

    if (err != RIG_OK)
    {
        return err;
    }

    xg3_get_ptt(rig, RIG_VFO_A, &ptt);  // set our PTT status

    return RIG_OK;
}


int xg3_set_level(RIG *rig, vfo_t vfo, setting_t level, value_t val)
{
    char levelbuf[16];

    rig_debug(RIG_DEBUG_VERBOSE, "%s called\n", __func__);

    switch (level)
    {
    case RIG_LEVEL_RFPOWER:
        if (val.f < 0 || val.f > 3)
        {
            return -RIG_EINVAL;
        }

        /* XXX check level range */
        SNPRINTF(levelbuf, sizeof(levelbuf), "L,%02d", (int)val.f);
        break;

    default:
        rig_debug(RIG_DEBUG_ERR, "%s: unsupported set_level %s", __func__,
                  rig_strlevel(level));
        return -RIG_EINVAL;
    }

    return kenwood_transaction(rig, levelbuf, NULL, 0);
}

/*
 * kenwood_get_level
 */
int xg3_get_level(RIG *rig, vfo_t vfo, setting_t level, value_t *val)
{
    char cmdbuf[32], replybuf[32];
    int retval;
    size_t replysize = sizeof(replybuf);
    struct rig_state *rs = &rig->state;

    rig_debug(RIG_DEBUG_VERBOSE, "%s called\n", __func__);

    switch (level)
    {
    case RIG_LEVEL_RFPOWER:
        SNPRINTF(cmdbuf, sizeof(cmdbuf), "L;");
        retval = write_block(&rs->rigport, (unsigned char *) cmdbuf, strlen(cmdbuf));

        if (retval != RIG_OK)
        {
            rig_debug(RIG_DEBUG_VERBOSE, "%s write_block failed\n",
                      __func__);
            return retval;
        }

        retval = read_string(&rs->rigport, (unsigned char *) replybuf, replysize,
                             ";", 1, 0, 1);

        if (retval < 0)
        {
            rig_debug(RIG_DEBUG_VERBOSE, "%s read_string failed\n",
                      __func__);
            return retval;
        }

        sscanf(replybuf, "L,%f", &val->f);
        return RIG_OK;

    case RIG_LEVEL_RAWSTR:
    case RIG_LEVEL_AF:
    case RIG_LEVEL_RF:
    case RIG_LEVEL_SQL:
    case RIG_LEVEL_MICGAIN:
    case RIG_LEVEL_AGC:
    case RIG_LEVEL_SLOPE_LOW:
    case RIG_LEVEL_SLOPE_HIGH:
    case RIG_LEVEL_CWPITCH:
    case RIG_LEVEL_KEYSPD:
    case RIG_LEVEL_IF:
    case RIG_LEVEL_APF:
    case RIG_LEVEL_NR:
    case RIG_LEVEL_PBT_IN:
    case RIG_LEVEL_PBT_OUT:
    case RIG_LEVEL_NOTCHF:
    case RIG_LEVEL_COMP:
    case RIG_LEVEL_BKINDL:
    case RIG_LEVEL_BALANCE:
        return -RIG_ENIMPL;

    default:
        rig_debug(RIG_DEBUG_ERR, "%s: unsupported get_level %s", __func__,
                  rig_strlevel(level));
        return -RIG_EINVAL;
    }

    return RIG_OK;
}

/*
 * xg3_get_vfo
 */
int xg3_get_vfo(RIG *rig, vfo_t *vfo)
{
    rig_debug(RIG_DEBUG_VERBOSE, "%s called\n", __func__);

    if (!vfo)
    {
        return -RIG_EINVAL;
    }

    *vfo = rig->state.current_vfo;      // VFOA or MEM
    return RIG_OK;
}

/*
 * xg3_set_vfo
 */
int xg3_set_vfo(RIG *rig, vfo_t vfo)
{
    rig_debug(RIG_DEBUG_VERBOSE, "%s called\n", __func__);

    if (!vfo)
    {
        return -RIG_EINVAL;
    }

    if (vfo != RIG_VFO_A) { return -RIG_EINVAL; }

    // We don't actually set the vfo on the XG3
    // But we need this so we can set frequencies on the band buttons
    rig->state.current_vfo = vfo;
    return RIG_OK;
}

/*
 * xg3_set_freq
 */
int xg3_set_freq(RIG *rig, vfo_t vfo, freq_t freq)
{
    int err;
    vfo_t tvfo;
    char cmdbuf[20];

    rig_debug(RIG_DEBUG_VERBOSE, "%s called\n", __func__);

    tvfo = (vfo == RIG_VFO_CURR ||
            vfo == RIG_VFO_VFO) ? rig->state.current_vfo : vfo;

    switch (tvfo)
    {
    case RIG_VFO_A:
        break;

    case RIG_VFO_MEM:
        break;

    default:
        rig_debug(RIG_DEBUG_ERR, "%s: unsupported VFO %s\n", __func__, rig_strvfo(vfo));
        return -RIG_EINVAL;
    }

    if (tvfo == RIG_VFO_MEM)
    {
        int ch;
        xg3_get_mem(rig, vfo, &ch);
        SNPRINTF(cmdbuf, sizeof(cmdbuf), "M,%02d,%011ld", ch, (long)freq);
    }
    else
    {
        SNPRINTF(cmdbuf, sizeof(cmdbuf), "F,%011ld", (long)freq);
    }

    err = kenwood_transaction(rig, cmdbuf, NULL, 0);

    return err;
}

/*
 * xg3_get_freq
 */
int xg3_get_freq(RIG *rig, vfo_t vfo, freq_t *freq)
{
    struct rig_state *rs;
    char freqbuf[50];
    int freqsize = sizeof(freqbuf);
    char cmdbuf[16];
    int retval;
    int offset;
    vfo_t tvfo;

    rig_debug(RIG_DEBUG_VERBOSE, "%s called\n", __func__);

    if (!freq)
    {
        return -RIG_EINVAL;
    }

    tvfo = (vfo == RIG_VFO_CURR ||
            vfo == RIG_VFO_VFO) ? rig->state.current_vfo : vfo;
    rs = &rig->state;

    switch (tvfo)
    {
    case RIG_VFO_A:
        break;

    case RIG_VFO_MEM:
        break;

    default:
        rig_debug(RIG_DEBUG_ERR, "%s: unsupported VFO %s\n", __func__, rig_strvfo(vfo));
        return -RIG_EINVAL;
    }

    if (tvfo == RIG_VFO_MEM)
    {
        int ch;
        xg3_get_mem(rig, vfo, &ch);
        SNPRINTF(cmdbuf, sizeof(cmdbuf), "M,%02d;", ch);
    }
    else
    {
        SNPRINTF(cmdbuf, sizeof(cmdbuf), "F;");
    }

    retval = write_block(&rs->rigport, (unsigned char *) cmdbuf, strlen(cmdbuf));

    if (retval != RIG_OK)
    {
        rig_debug(RIG_DEBUG_VERBOSE, "%s write_block failed\n", __func__);
        return retval;
    }

    retval = read_string(&rs->rigport, (unsigned char *) freqbuf, freqsize,
                         ";", 1, 0, 1);

    if (retval < 0)
    {
        rig_debug(RIG_DEBUG_VERBOSE, "%s read_string failed\n", __func__);
        return retval;
    }

    offset = tvfo == RIG_VFO_A ? 2 : 5;

    sscanf(freqbuf + offset, "%" SCNfreq, freq);

    return RIG_OK;
}

/*
 * xg3_set_powerstat
 */
int xg3_set_powerstat(RIG *rig, powerstat_t status)
{
    struct xg3_priv_data *priv = (struct xg3_priv_data *)rig->state.priv;

    rig_debug(RIG_DEBUG_VERBOSE, "%s called\n", __func__);

    if (status == RIG_POWER_OFF)
    {
        const char *cmd = "X";

        priv->powerstat = RIG_POWER_OFF;
        return kenwood_transaction(rig, cmd, NULL, 0);
    }

    rig_debug(RIG_DEBUG_VERBOSE, "%s invalid powerstat request status=%d\n",
              __func__, status);
    return -RIG_EINVAL;
}

/*
 * xg3_get_powerstat
 */
int xg3_get_powerstat(RIG *rig, powerstat_t *status)
{
    const char *cmd = "G";      // any command to test will do
    int retval = kenwood_transaction(rig, cmd, NULL, 0);
    struct rig_state *rs = &rig->state;
    struct xg3_priv_data *priv = (struct xg3_priv_data *)rig->state.priv;

    rig_debug(RIG_DEBUG_VERBOSE, "%s called\n", __func__);

    if (retval == RIG_OK)
    {
        char reply[32];
        retval = read_string(&rs->rigport, (unsigned char *) reply, sizeof(reply),
                             ";", 1, 0, 1);
        *status = RIG_POWER_ON;
        priv->powerstat = RIG_POWER_ON;
    }

    if (retval != RIG_OK)
    {
        *status = RIG_POWER_OFF;    // Error indicates power is off
        rig_debug(RIG_DEBUG_VERBOSE, "%s read_string failed\n", __func__);
        priv->powerstat = RIG_POWER_OFF;
    }

    return RIG_OK;              // Always OK since it's a binary state
}

/*
 * xg3_set_mem
 */
int xg3_set_mem(RIG *rig, vfo_t vfo, int ch)
{
    char cmdbuf[32];
    int retval;

    rig_debug(RIG_DEBUG_VERBOSE, "%s called\n", __func__);

    if (ch < 0 || ch > 11)
    {
        rig_debug(RIG_DEBUG_VERBOSE, "%s invalid channel#%02d\n", __func__, ch);
        return -RIG_EINVAL;
    }

    SNPRINTF(cmdbuf, sizeof(cmdbuf), "C,%02d;", ch);
    retval = kenwood_transaction(rig, cmdbuf, NULL, 0);

    if (retval != RIG_OK)
    {
        rig_debug(RIG_DEBUG_VERBOSE, "%s invalid set_mem cmd=%s\n", __func__,
                  cmdbuf);
        return -RIG_EINVAL;
    }

    return RIG_OK;
}

/*
 * xg3_get_mem
 */
int xg3_get_mem(RIG *rig, vfo_t vfo, int *ch)
{
    char cmdbuf[32];
    char reply[32];
    int retval;
    struct rig_state *rs = &rig->state;

    rig_debug(RIG_DEBUG_VERBOSE, "%s called\n", __func__);

    SNPRINTF(cmdbuf, sizeof(cmdbuf), "C;");
    retval = kenwood_transaction(rig, cmdbuf, NULL, 0);

    if (retval != RIG_OK)
    {
        return retval;
    }

    retval = read_string(&rs->rigport, (unsigned char *) reply, sizeof(reply),
                         ";", 1, 0, 1);

    if (retval < 0)
    {
        rig_debug(RIG_DEBUG_VERBOSE, "%s read_string failed\n", __func__);
        return retval;
    }

    sscanf(reply, "C,%d", ch);
    return RIG_OK;
}

/*
 * xg3_set_ptt
 */
int xg3_set_ptt(RIG *rig, vfo_t vfo, ptt_t ptt)
{
    struct xg3_priv_data *priv = (struct xg3_priv_data *)rig->state.priv;
    int retval;

    rig_debug(RIG_DEBUG_VERBOSE, "%s called\n", __func__);

    retval = kenwood_simple_transaction(rig,
                                        (ptt == RIG_PTT_ON) ? "O,01" : "O,00", 0);

    if (retval == RIG_OK)
    {
        priv->ptt = RIG_PTT_ON;
    }

    return retval;
}

/*
 * kenwood_get_ptt
 */
int xg3_get_ptt(RIG *rig, vfo_t vfo, ptt_t *ptt)
{
    char pttbuf[6];
    int retval;
    struct xg3_priv_data *priv = (struct xg3_priv_data *)rig->state.priv;

    rig_debug(RIG_DEBUG_VERBOSE, "%s called\n", __func__);

    if (!ptt)
    {
        return -RIG_EINVAL;
    }

    retval = kenwood_safe_transaction(rig, "O", pttbuf, 6, 4);

    if (retval != RIG_OK)
    {
        return retval;
    }

    *ptt = pttbuf[3] == '1' ? RIG_PTT_ON : RIG_PTT_OFF;
    priv->ptt = *ptt;

    return RIG_OK;
}

/*
 * xg3_set_parm
 */
int xg3_set_parm(RIG *rig, setting_t parm, value_t val)
{
    int ival;
    char cmdbuf[16];
    int retval = -RIG_EINVAL;

    rig_debug(RIG_DEBUG_VERBOSE, "%s called\n", __func__);

    switch (parm)
    {
    case RIG_PARM_BACKLIGHT:
        ival = 3 - (int)(val.f * 3); // gives us 0-3 bright-to-dim
        rig_debug(RIG_DEBUG_ERR, "%s: BACKLIGHT %d\n", __func__, ival);
        SNPRINTF(cmdbuf, sizeof(cmdbuf), "G,%02d", ival);
        retval = kenwood_simple_transaction(rig, cmdbuf, 0);
        break;

    default:
        rig_debug(RIG_DEBUG_ERR, "%s: Unsupported set_parm %s\n", __func__,
                  rig_strparm(parm));
        return -RIG_EINVAL;
    }

    return retval;
}

/*
 * xg3_get_parm
 */
int xg3_get_parm(RIG *rig, setting_t parm, value_t *val)
{
    int ival;
    char replybuf[6];
    int retval = -RIG_EINVAL;

    rig_debug(RIG_DEBUG_VERBOSE, "%s called\n", __func__);

    switch (parm)
    {
    case RIG_PARM_BACKLIGHT:
        retval = kenwood_safe_transaction(rig, "G", replybuf, 6, 4);

        if (retval == RIG_OK)
        {
            sscanf(&replybuf[3], "%d", &ival);
            val->f = (3.0f - (float) ival) / 3.0f;
        }

        break;

    default:
        rig_debug(RIG_DEBUG_ERR, "%s: Unsupported set_parm %s\n", __func__,
                  rig_strparm(parm));
        return -RIG_EINVAL;
    }

    return retval;
}
