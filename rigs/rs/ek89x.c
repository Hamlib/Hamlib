/*
 *  Hamlib R&S EK895/896 backend - main file
 *  Reused from p2000.c
 *  Copyright (c) 2022 by Michael Black W9MDB
 *  Copyright (c) 2018 by Michael Black W9MDB
 *  Copyright (c) 2009-2010 by Stéphane Fillod
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

/*
 * Looks like the GP2000 could be reused in other rigs so
 * we implement that and then the EK89X uses this interface
 */
#include <hamlib/config.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>     /* String function definitions */

#include "hamlib/rig.h"
#include "serial.h"
#include "misc.h"
#include "register.h"
#include "num_stdio.h"

#include "ek89x.h"

#define RESPSZ 64

#define LF "\x0a"
#define CR "\x0d"
#define BOM LF
#define EOM CR

/*
 * ek89x
 * We assume that rig!=NULL, rig->state!= NULL, data!=NULL, data_len!=NULL
 */
int
ek89x_transaction(RIG *rig, const char *cmd, int cmd_len, char *data,
                  int *data_len)
{
    int retval;
    struct rig_state *rs;

    rig_debug(RIG_DEBUG_VERBOSE, "%s: len=%d,cmd=%s\n", __func__, cmd_len,
              cmd);

    rs = &rig->state;

    rig_flush(&rs->rigport);

    rig_debug(RIG_DEBUG_VERBOSE, "ek89x_transaction: len=%d,cmd=%s\n",
              cmd_len, cmd);
    retval = write_block(&rs->rigport, (unsigned char *) cmd, cmd_len);

    if (retval != RIG_OK)
    {
        return retval;
    }


    /* no data expected */
    if (!data || !data_len)
    {
        return RIG_OK;
    }

    retval = read_string(&rs->rigport, (unsigned char *) data, RESPSZ,
                         CR, 1, 0, 1);

    if (retval < 0)
    {
        return retval;
    }

    *data_len = retval;

    return RIG_OK;
}

/*
 * ek89x_set_freq
 * Assumes rig!=NULL
 */
int
ek89x_set_freq(RIG *rig, vfo_t vfo, freq_t freq)
{
    char freqbuf[32];
    int retval;
    // cppcheck-suppress *
    char *fmt = BOM "F%" PRIll EOM;

    rig_debug(RIG_DEBUG_VERBOSE, "%s: vfo=%s,freq=%.0f\n", __func__,
              rig_strvfo(vfo), freq);

    SNPRINTF(freqbuf, sizeof(freqbuf), fmt, (int64_t) freq, (int64_t) freq);
    retval = ek89x_transaction(rig, freqbuf, strlen(freqbuf), NULL, NULL);

    return retval;
}

/*
 * ek89x_get_freq
 * Assumes rig!=NULL
 */
int
ek89x_get_freq(RIG *rig, vfo_t vfo, freq_t *freq)
{
    char buf[RESPSZ];
    int len, retval;

    rig_debug(RIG_DEBUG_VERBOSE, "%s: vfo=%s\n", __func__, rig_strvfo(vfo));

#define FREQ_QUERY  BOM "F?" EOM

    retval =
        ek89x_transaction(rig, FREQ_QUERY, strlen(FREQ_QUERY), buf, &len);

    if (retval < 0)
    {
        return retval;
    }

    retval = (sscanf(buf, "%*cF%" SCNfreq, freq) == 1) ? RIG_OK : -RIG_EPROTO;

    return retval;
}

/*
 * ek89x_set_mode
 * Assumes rig!=NULL
 */
int
ek89x_set_mode(RIG *rig, vfo_t vfo, rmode_t mode, pbwidth_t width)
{
    char buf[32], *smode;
    int retval;

    rig_debug(RIG_DEBUG_VERBOSE, "%s: vfo=%s, mode=%s, width=%d\n", __func__,
              rig_strvfo(vfo), rig_strvfo(mode), (int)width);

    switch (mode)
    {
    case RIG_MODE_USB:
        smode = "15";
        break;

    case RIG_MODE_LSB:
        smode = "16";
        break;

    case RIG_MODE_ISBUSB:
        smode = "17";
        break;

    case RIG_MODE_ISBLSB:
        smode = "18";
        break;

    default:
        return -RIG_EINVAL;
    }

    SNPRINTF(buf, sizeof(buf), BOM "I%s" EOM, smode);
    retval = ek89x_transaction(rig, buf, strlen(buf), NULL, NULL);

    if (retval < 0)
    {
        return retval;
    }

    if (width == RIG_PASSBAND_NOCHANGE)
    {
        return retval;
    }

    if (width == RIG_PASSBAND_NORMAL)
    {
        width = rig_passband_normal(rig, mode);
    }

    if (width > 0)
    {
        if (width <= 150) { width = 1; }
        else if (width <= 300) { width = 3; }
        else if (width <= 600) { width = 6; }
        else if (width <= 1000) { width = 10; }
        else if (width <= 1500) { width = 15; }
        else if (width <= 2100) { width = 21; }
        else if (width <= 2400) { width = 24; }
        else if (width <= 2700) { width = 27; }
        else if (width <= 3100) { width = 31; }
        else if (width <= 4000) { width = 40; }
        else if (width <= 4800) { width = 48; }
        else if (width <= 6000) { width = 60; }
        else if (width <= 8000) { width = 80; }

        SNPRINTF(buf, sizeof(buf), BOM "W%d" EOM, (int) width);
        retval = ek89x_transaction(rig, buf, strlen(buf), NULL, NULL);
    }

    return retval;
}

/*
 * ek89x_get_mode
 * Assumes rig!=NULL
 */
int
ek89x_get_mode(RIG *rig, vfo_t vfo, rmode_t *mode, pbwidth_t *width)
{
    char buf[RESPSZ];
    int buf_len, retval;
    int nmode;
    char *pmode = "UNKNOWN";
    int n;


    rig_debug(RIG_DEBUG_VERBOSE, "%s: vfo=%s\n", __func__, rig_strvfo(vfo));

#define DEM_QUERY   BOM "I?" EOM

    retval =
        ek89x_transaction(rig, DEM_QUERY, strlen(DEM_QUERY), buf, &buf_len);

    if (retval < 0)
    {
        return retval;
    }

    n = sscanf(buf, "%*cI%d", &nmode);

    if (n != 1)
    {
        rig_debug(RIG_DEBUG_ERR, "%s: unable to parse mode from '%s'\n", __func__, buf);
        return -RIG_EPROTO;
    }

    switch (nmode)
    {
    case 15: pmode = "USB"; break;

    case 16: pmode = "LSB"; break;
    }

    *mode = rig_parse_mode(pmode);

#define BAND_QUERY   BOM "FIB?" EOM
    retval =
        ek89x_transaction(rig, BAND_QUERY, strlen(BAND_QUERY), buf, &buf_len);

    if (retval < 0)
    {
        return retval;
    }

    int twidth;
    sscanf(buf, "%*cFIB%d", &twidth);

    if (twidth == 1) { *width = 150; }
    else { *width = twidth * 100; }

    return retval;
}


#ifdef XXREMOVEDXX
// Not referenced anywhere
int
ek89x_set_func(RIG *rig, vfo_t vfo, setting_t func, int status)
{
    char buf[32], *sfunc;
    int len, retval;

    rig_debug(RIG_DEBUG_VERBOSE, "%s: vfo=%s\n", __func__, rig_strvfo(vfo));

    switch (func)
    {
    case RIG_FUNC_SQL:
        sfunc = "SQ00";
        break;

    default:
        return -RIG_EINVAL;
    }

    SNPRINTF(buf, sizeof(buf), BOM "%s %s" EOM, sfunc, status ? "1" : "0");
    retval = ek89x_transaction(rig, buf, strlen(buf), NULL, NULL);

    return retval;
}
#endif

#ifdef XXREMOVEDXX
// Not referenced anywhere
int
ek89x_get_func(RIG *rig, vfo_t vfo, setting_t func, int *status)
{
    char buf[RESPSZ], *sfunc;
    int buf_len, retval;

    rig_debug(RIG_DEBUG_VERBOSE, "%s: vfo=%s\n", __func__, rig_strvfo(vfo));

    switch (func)
    {
    case RIG_FUNC_SQL:
        sfunc = BOM "SQ00?" EOM;
        break;

    default:
        return -RIG_EINVAL;
    }

    retval = ek89x_transaction(rig, sfunc, strlen(sfunc), buf, &buf_len);

    if (retval < 0)
    {
        return retval;
    }

    // we expected LF+"X" where X is the status
    *status = buf[2] == 1 ? 1 : 0;

    return retval;
}
#endif

int
ek89x_set_level(RIG *rig, vfo_t vfo, setting_t level, value_t val)
{
    char buf[64];
    int retval;

    rig_debug(RIG_DEBUG_VERBOSE, "%s: vfo=%s\n", __func__, rig_strvfo(vfo));

    switch (level)
    {
    case RIG_LEVEL_PREAMP:
        SNPRINTF(buf, sizeof(buf), BOM "PA%d" EOM, (int)val.f);
        break;

    default:
        return -RIG_EINVAL;
    }

    retval = ek89x_transaction(rig, buf, strlen(buf), NULL, NULL);

    return retval;
}

int
ek89x_get_level(RIG *rig, vfo_t vfo, setting_t level, value_t *val)
{
    char buf[RESPSZ], *slevel;
    int buf_len, retval, ival;

    rig_debug(RIG_DEBUG_VERBOSE, "%s: vfo=%s\n", __func__, rig_strvfo(vfo));

    switch (level)
    {
    case RIG_LEVEL_PREAMP:
        slevel = BOM "PA?" EOM;
        break;

    case RIG_LEVEL_STRENGTH:
        slevel = BOM "L?" EOM;
        break;

    default:
        return -RIG_EINVAL;
    }

    retval = ek89x_transaction(rig, slevel, strlen(slevel), buf, &buf_len);

    if (retval < 0)
    {
        return retval;
    }

    switch (level)
    {
    case RIG_LEVEL_PREAMP:
        if (num_sscanf(buf, "%*cPA%d", &ival) != 1)
        {
            return -RIG_EPROTO;
        }

        val->f = ival;

        break;

    case RIG_LEVEL_STRENGTH:
        if (num_sscanf(buf, "%*cL%d", &ival) != 1)
        {
            return -RIG_EPROTO;
        }

        val->f = ival - 34; // approximately

        break;

    default:
        return -RIG_EINVAL;
    }

    return retval;
}

const char *
ek89x_get_info(RIG *rig)
{
    static char infobuf[128];
    int info_len, retval;
    int addr = -1;
    char type[32] = "unk type";
    char rigid[32] = "unk rigid";
    char sernum[32] = "unk sernum";
    char *p;


    rig_debug(RIG_DEBUG_VERBOSE, "%s\n", __func__);

#define ID_QUERY BOM "IDENT?" EOM

    retval =
        ek89x_transaction(rig, ID_QUERY, strlen(ID_QUERY), infobuf, &info_len);

    if (retval < 0)
    {
        return NULL;
    }

    p = strtok(infobuf, ",");

    while (p)
    {
        switch (p[0])
        {
        case 0x0a:
            sscanf(p, "%*cIDENT%31s", type);
            break;

        case 'i':
            sscanf(p, "id%31s", rigid);
            break;

        case 's':
            sscanf(p, "sn%31s", sernum);
            break;

        default:
            printf("Unknown response: %s\n", p);

        }

        p = strtok(NULL, ",");
    }

    SNPRINTF(infobuf, sizeof(infobuf), "ADDR=%02d\nTYPE=%s\nSER#=%s\nID  =%s\n",
             addr, type, sernum, rigid);

    return infobuf;
}

#define EK89X_MODES (RIG_MODE_USB|RIG_MODE_LSB)

#define EK89X_FUNC (RIG_FUNC_SQL)

#define EK89X_LEVEL_ALL (RIG_LEVEL_PREAMP|RIG_LEVEL_STRENGTH)

#define EK89X_PARM_ALL (RIG_PARM_NONE)

#define EK89X_VFO (RIG_VFO_A)

#define EK89X_VFO_OPS (RIG_OP_NONE)

#define EK89X_ANTS (RIG_ANT_1)

#define EK89X_MEM_CAP {    \
        .freq = 1,      \
        .mode = 1,      \
        .width = 1,     \
        .ant = 1,     \
        .funcs = EK89X_FUNC, \
        .levels = RIG_LEVEL_SET(EK89X_LEVEL_ALL), \
        .channel_desc=1, \
        .flags = RIG_CHFLAG_SKIP, \
}


/*
 * EK89X rig capabilities.
 *
 * Had to use NONE for flow control and set RTS high
 * We are not using address mode since we're on RS232 for now
 * If using RS485 should add address capability
 *
 * TODO
 *  - set/get_channels
 */

const struct rig_caps ek89x_caps =
{
    RIG_MODEL(RIG_MODEL_EK89X),
    .model_name = "EK895/6",
    .mfg_name = "Rohde&Schwarz",
    .version = "20220813.0",
    .copyright = "LGPL",
    .status = RIG_STATUS_BETA,
    .rig_type = RIG_TYPE_RECEIVER,
    .ptt_type = RIG_PTT_NONE,
    // Need to set RTS on for some reason
    // And HANDSHAKE_NONE even though HARDWARE is what is called for
    .dcd_type = RIG_DCD_NONE,
    .port_type = RIG_PORT_SERIAL,
    .serial_rate_min = 9600,
    .serial_rate_max = 38400,
    .serial_data_bits = 7,
    .serial_stop_bits = 1,
    .serial_parity = RIG_PARITY_ODD,
    .serial_handshake = RIG_HANDSHAKE_NONE,
    .write_delay = 0,
    .post_write_delay = 0,
    .timeout = 200,
    .retry = 3,

    .has_get_func = EK89X_FUNC,
    .has_set_func = EK89X_FUNC,
    .has_get_level = EK89X_LEVEL_ALL,
    .has_set_level = RIG_LEVEL_SET(EK89X_LEVEL_ALL),
    .has_get_parm = EK89X_PARM_ALL,
    .has_set_parm = RIG_PARM_SET(EK89X_PARM_ALL),
    .level_gran = {},
    .parm_gran = {},
    .ctcss_list = NULL,
    .dcs_list = NULL,
    .preamp = {RIG_DBLST_END},
    .attenuator = {32, RIG_DBLST_END},
    .max_rit = Hz(0),
    .max_xit = Hz(0),
    .max_ifshift = Hz(0),
    .targetable_vfo = 0,
    .transceive = RIG_TRN_RIG,
    .bank_qty = 0,
    .chan_desc_sz = 7,        /* FIXME */
    //.vfo_ops = XK2100_VFO_OPS,

    .chan_list = {
        {0, 999, RIG_MTYPE_MEM, EK89X_MEM_CAP},
        RIG_CHAN_END,
    },


    .rx_range_list1 = {
        {
            kHz(0), MHz(30), EK89X_MODES, -1, -1, EK89X_VFO,
            EK89X_ANTS
        },
        RIG_FRNG_END,
    },
    .tx_range_list1 = {RIG_FRNG_END,},
    .rx_range_list2 = {
        {
            kHz(0), MHz(30), EK89X_MODES, -1, -1, EK89X_VFO,
            EK89X_ANTS
        },
        RIG_FRNG_END,
    },
    .tx_range_list2 = {RIG_FRNG_END,},

    .tuning_steps =     {
        // Rem: no support for changing tuning step
        {RIG_MODE_ALL, 1},
        RIG_TS_END,
    },

    /*
    .tuning_steps =  {
         {EK89X_MODES,1},
         {EK89X_MODES,10},
         {EK89X_MODES,100},
         {EK89X_MODES,1000},
         RIG_TS_END,
        },
        */

    /* mode/filter list, remember: order matters! */
    .filters = {
        {EK89X_MODES, kHz(8.0)},
        {EK89X_MODES, kHz(6.0)},
        {EK89X_MODES, Hz(4.8)},
        {EK89X_MODES, Hz(4.0)},
        {EK89X_MODES, Hz(3.1)},
        {EK89X_MODES, kHz(2.7)},
        {EK89X_MODES, kHz(2.4)},
        {EK89X_MODES, kHz(1.5)},
        {EK89X_MODES, kHz(1.0)},
        {EK89X_MODES, Hz(600)},
        {EK89X_MODES, Hz(300)},
        {EK89X_MODES, Hz(150)},
        RIG_FLT_END,
    },
    .priv = NULL,

//    .set_ptt = ek89x_set_ptt, receiver only
//    .get_ptt = ek89x_get_ptt, receiver only
    .set_freq = ek89x_set_freq,
    .get_freq = ek89x_get_freq,
    .set_mode =  ek89x_set_mode,
    .get_mode =  ek89x_get_mode,
    .set_level =  ek89x_set_level,
    .get_level =  ek89x_get_level,
//.set_func =  ek89x_set_func,
//.get_func =  ek89x_get_func,
    .get_info =  ek89x_get_info,

#if 0
    /* TODO */
    .rig_open = ek89x_rig_open,
    .set_channel = ek89x_set_channel,
    .get_channel = ek89x_get_channel,
#endif
    .hamlib_check_rig_caps = HAMLIB_CHECK_RIG_CAPS
};

