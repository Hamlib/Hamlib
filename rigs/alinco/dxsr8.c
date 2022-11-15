/*
 *  Hamlib Alinco backend - DXSR8 description
 *  Copyright (c) 2020 by Wes Bustraan (W8WJB)
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

#include <stdlib.h>
#include <string.h>  /* String function definitions */

#include <hamlib/rig.h>
#include <serial.h>
#include <misc.h>
#include <iofunc.h>
#include <num_stdio.h>
#include "idx_builtin.h"
#include "alinco.h"


#define DXSR8_ALL_MODES (RIG_MODE_USB|RIG_MODE_LSB|RIG_MODE_CW|RIG_MODE_CWR|RIG_MODE_AM|RIG_MODE_FM)
#define DXSR8_OTHER_TX_MODES (RIG_MODE_CW|RIG_MODE_SSB|RIG_MODE_FM)
#define DXSR8_AM_TX_MODES RIG_MODE_AM

#define DXSR8_FUNC (RIG_FUNC_FAGC|RIG_FUNC_NB)

#define DXSR8_LEVEL_ALL (RIG_LEVEL_RFPOWER|RIG_LEVEL_RF)

#define DXSR8_PARM_ALL RIG_PARM_NONE

#define DXSR8_VFO RIG_VFO_A

/* Line Feed */
#define EOM "\r\n"
#define LF "\n"

#define MD_USB  0
#define MD_LSB  1
#define MD_CWU  2
#define MD_CWL  3
#define MD_AM   4
#define MD_FM   5

int dxsr8_set_freq(RIG *rig, vfo_t vfo, freq_t freq);
int dxsr8_get_freq(RIG *rig, vfo_t vfo, freq_t *freq);
int dxsr8_set_mode(RIG *rig, vfo_t vfo, rmode_t mode, pbwidth_t width);
int dxsr8_get_mode(RIG *rig, vfo_t vfo, rmode_t *mode, pbwidth_t *width);
int dxsr8_set_func(RIG *rig, vfo_t vfo, setting_t func, int status);
int dxsr8_get_func(RIG *rig, vfo_t vfo, setting_t func, int *status);
int dxsr8_set_level(RIG *rig, vfo_t vfo, setting_t level, value_t val);
int dxsr8_get_level(RIG *rig, vfo_t vfo, setting_t level, value_t *val);
int dxsr8_get_ptt(RIG *rig, vfo_t vfo, ptt_t *ptt);
int dxsr8_set_ptt(RIG *rig, vfo_t vfo, ptt_t ptt);


/*
 * DX-SR8 rig capabilities.
 *
 * thanks to
 *      https://yo5ptd.wordpress.com/2017/02/12/alinco-dx-sr8/
 * for a partially documented protocol
 */
const struct rig_caps dxsr8_caps =
{
    RIG_MODEL(RIG_MODEL_DXSR8),
    .model_name =       "DX-SR8",
    .mfg_name =         "Alinco",
    .version =          BACKEND_VER ".0",
    .copyright =        "LGPL",
    .status =           RIG_STATUS_STABLE,
    .rig_type =         RIG_TYPE_TRANSCEIVER,
    .ptt_type =         RIG_PTT_RIG,
    .dcd_type =         RIG_DCD_RIG,
    .port_type =        RIG_PORT_SERIAL,
    .serial_rate_min =  9600,
    .serial_rate_max =  9600,
    .serial_data_bits = 8,
    .serial_stop_bits = 1,
    .serial_parity =    RIG_PARITY_NONE,
    .serial_handshake = RIG_HANDSHAKE_NONE,
    .write_delay =      0,
    .post_write_delay = 0,
    .timeout =          200,
    .retry =            3,

    .has_get_func =     DXSR8_FUNC,
    .has_set_func =     DXSR8_FUNC,
    .has_get_level =    DXSR8_LEVEL_ALL,
    .has_set_level =    RIG_LEVEL_SET(DXSR8_LEVEL_ALL),
    .has_get_parm =     RIG_PARM_NONE,
    .has_set_parm =     RIG_PARM_NONE,
    .level_gran =
    {
        [LVL_RAWSTR] =  { .min = { .i = 0 }, .max = { .i = 255 } },
    },
    .parm_gran =        {},
    .ctcss_list =       common_ctcss_list,
    .dcs_list =         NULL,
    .preamp =           { 10, RIG_DBLST_END },
    .attenuator =       { 10, 20, RIG_DBLST_END },
    .max_rit =          kHz(1.2),
    .max_xit =          kHz(1.2),
    .max_ifshift =      kHz(1.5),
    .targetable_vfo =   0,
    .transceive =       RIG_TRN_OFF,
    .bank_qty =         0,
    .chan_desc_sz =     0,

    .chan_list =
    {
        { 0, 199, RIG_MTYPE_MEM },
        { 0, 199, RIG_MTYPE_MEM },
        RIG_CHAN_END,
    },

    .rx_range_list1 =
    {
        {kHz(135), MHz(30), DXSR8_ALL_MODES, -1, -1, DXSR8_VFO, 0, "DX-SR8T"},
        RIG_FRNG_END,
    },
    .tx_range_list1 =
    {
        {kHz(1800), MHz(2) - 100, DXSR8_OTHER_TX_MODES, W(10), W(100), DXSR8_VFO, 0, "DX-SR8T"},
        {kHz(1800), MHz(2) - 100, DXSR8_AM_TX_MODES, W(4), W(40), DXSR8_VFO, 0, "DX-SR8T"},
        {kHz(3500), MHz(4) - 100, DXSR8_OTHER_TX_MODES, W(10), W(100), DXSR8_VFO, 0, "DX-SR8T"},
        {kHz(3500), MHz(4) - 100, DXSR8_AM_TX_MODES, W(4), W(40), DXSR8_VFO, 0, "DX-SR8T"},
        {5330500, 5330500, DXSR8_AM_TX_MODES, W(1), W(50), DXSR8_VFO, 0, "DX-SR8T"},
        {5346500, 5346500, DXSR8_AM_TX_MODES, W(1), W(50), DXSR8_VFO, 0, "DX-SR8T"},
        {5366500, 5366500, DXSR8_AM_TX_MODES, W(1), W(50), DXSR8_VFO, 0, "DX-SR8T"},
        {5371500, 5371500, DXSR8_AM_TX_MODES, W(1), W(50), DXSR8_VFO, 0, "DX-SR8T"},
        {5403500, 5403500, DXSR8_AM_TX_MODES, W(1), W(50), DXSR8_VFO, 0, "DX-SR8T"},
        {MHz(7), kHz(7300), DXSR8_OTHER_TX_MODES, W(10), W(100), DXSR8_VFO, 0, "DX-SR8T"},
        {MHz(7), kHz(7300), DXSR8_AM_TX_MODES, W(4), W(40), DXSR8_VFO, 0, "DX-SR8T"},
        {kHz(10100), kHz(10150), DXSR8_OTHER_TX_MODES, W(10), W(100), DXSR8_VFO, 0, "DX-SR8T"},
        {kHz(10100), kHz(10150), DXSR8_AM_TX_MODES, W(4), W(40), DXSR8_VFO, 0, "DX-SR8T"},
        {MHz(14), kHz(14350), DXSR8_OTHER_TX_MODES, W(10), W(100), DXSR8_VFO, 0, "DX-SR8T"},
        {MHz(14), kHz(14350), DXSR8_AM_TX_MODES, W(4), W(40), DXSR8_VFO, 0, "DX-SR8T"},
        {kHz(18068), kHz(18168), DXSR8_OTHER_TX_MODES, W(10), W(100), DXSR8_VFO, 0, "DX-SR8T"},
        {kHz(18068), kHz(18168), DXSR8_AM_TX_MODES, W(4), W(40), DXSR8_VFO, 0, "DX-SR8T"},
        {MHz(21), kHz(21450), DXSR8_OTHER_TX_MODES, W(10), W(100), DXSR8_VFO, 0, "DX-SR8T"},
        {MHz(21), kHz(21450), DXSR8_AM_TX_MODES, W(4), W(40), DXSR8_VFO, 0, "DX-SR8T"},
        {kHz(24890), kHz(24990), DXSR8_OTHER_TX_MODES, W(10), W(100), DXSR8_VFO, 0, "DX-SR8T"},
        {kHz(24890), kHz(24990), DXSR8_AM_TX_MODES, W(4), W(40), DXSR8_VFO, 0, "DX-SR8T"},
        {MHz(28), kHz(29700), DXSR8_OTHER_TX_MODES, W(10), W(100), DXSR8_VFO, 0, "DX-SR8T"},
        {MHz(28), kHz(29700), DXSR8_AM_TX_MODES, W(4), W(40), DXSR8_VFO, 0, "DX-SR8T"},
        RIG_FRNG_END,
    },
    .rx_range_list2 =
    {
        {kHz(135), MHz(30), DXSR8_ALL_MODES, -1, -1, DXSR8_VFO, 0, "DX-SR8E"},
        RIG_FRNG_END,
    },
    .tx_range_list2 =
    {
        {kHz(1800), MHz(2) - 100, DXSR8_OTHER_TX_MODES, W(10), W(100), DXSR8_VFO, 0, "DX-SR8E"},
        {kHz(1800), MHz(2) - 100, DXSR8_AM_TX_MODES, W(4), W(40), DXSR8_VFO, 0, "DX-SR8E"},
        {kHz(3500), MHz(4) - 100, DXSR8_OTHER_TX_MODES, W(10), W(100), DXSR8_VFO, 0, "DX-SR8E"},
        {kHz(3500), MHz(4) - 100, DXSR8_AM_TX_MODES, W(4), W(40), DXSR8_VFO, 0, "DX-SR8E"},
        {kHz(6900), kHz(7500), DXSR8_OTHER_TX_MODES, W(10), W(100), DXSR8_VFO, 0, "DX-SR8E"},
        {kHz(6900), kHz(7500), DXSR8_AM_TX_MODES, W(4), W(40), DXSR8_VFO, 0, "DX-SR8E"},
        {kHz(9900), kHz(10500), DXSR8_OTHER_TX_MODES, W(10), W(100), DXSR8_VFO, 0, "DX-SR8E"},
        {kHz(9900), kHz(10500), DXSR8_AM_TX_MODES, W(4), W(40), DXSR8_VFO, 0, "DX-SR8E"},
        {kHz(13900), kHz(14500), DXSR8_OTHER_TX_MODES, W(10), W(100), DXSR8_VFO, 0, "DX-SR8E"},
        {kHz(13900), kHz(14500), DXSR8_AM_TX_MODES, W(4), W(40), DXSR8_VFO, 0, "DX-SR8E"},
        {kHz(17900), kHz(18500), DXSR8_OTHER_TX_MODES, W(10), W(100), DXSR8_VFO, 0, "DX-SR8E"},
        {kHz(17900), kHz(18500), DXSR8_AM_TX_MODES, W(4), W(40), DXSR8_VFO, 0, "DX-SR8E"},
        {kHz(20900), kHz(21500), DXSR8_OTHER_TX_MODES, W(10), W(100), DXSR8_VFO, 0, "DX-SR8E"},
        {kHz(20900), kHz(21500), DXSR8_AM_TX_MODES, W(4), W(40), DXSR8_VFO, 0, "DX-SR8E"},
        {kHz(24400), kHz(25099), DXSR8_OTHER_TX_MODES, W(10), W(100), DXSR8_VFO, 0, "DX-SR8E"},
        {kHz(24400), kHz(25099), DXSR8_AM_TX_MODES, W(4), W(40), DXSR8_VFO, 0, "DX-SR8E"},
        {MHz(28), MHz(30), DXSR8_OTHER_TX_MODES, W(10), W(100), DXSR8_VFO, 0, "DX-SR8E"},
        {MHz(28), MHz(30), DXSR8_AM_TX_MODES, W(4), W(40), DXSR8_VFO, 0, "DX-SR8E"},
        RIG_FRNG_END,
    },
    .tuning_steps =
    {
        {DXSR8_ALL_MODES, 10},  /* FIXME: add other ts */
        RIG_TS_END,
    },
    /* mode/filter list, remember: order matters! */
    .filters =
    {
        {RIG_MODE_CW, kHz(1)},
        {RIG_MODE_CW, kHz(0.5)},
        {RIG_MODE_SSB, kHz(2.4)},
        {RIG_MODE_SSB, kHz(1)},
        {RIG_MODE_AM | RIG_MODE_FM, kHz(9)},
        {RIG_MODE_AM, kHz(2.4)},
        RIG_FLT_END,
    },
    .set_freq =         dxsr8_set_freq, // AL~RW_RXF14212000
    .get_freq =         dxsr8_get_freq, // AL~RR_RXF
    .set_mode =         dxsr8_set_mode, // AL~RW_RFM00, AL~RW_NAR00
    .get_mode =         dxsr8_get_mode, // AL~RR_RFM, AL~RR_NAR
    .get_ptt =          dxsr8_get_ptt, // AL~RR_PTT
    .set_ptt =          dxsr8_set_ptt, // AL~RW_PTT00
    .set_func =         dxsr8_set_func, // AL~RW_AGC00, AL~RW_NZB00
    .get_func =         dxsr8_get_func, // AL~RR_AGC, AL~RR_NZB
    .set_level =        dxsr8_set_level, // AL~RW_RFG00, AL~RW_PWR00
    .get_level =        dxsr8_get_level, // AL~RR_RFG, AL~RR_PWR
    .hamlib_check_rig_caps = HAMLIB_CHECK_RIG_CAPS
};

/*
 * Function definitions below
 */


/*
 * dxsr8_transaction
 * We assume that rig!=NULL, rig->state!= NULL, data!=NULL, data_len!=NULL
 * Otherwise, you'll get a nice seg fault. You've been warned!
 * TODO: error case handling
 */
int dxsr8_transaction(RIG *rig,
                      const char *cmd,
                      int cmd_len,
                      char *data,
                      int *data_len)
{

    int retval;
    struct rig_state *rs;
    char replybuf[BUFSZ + 1];
    int reply_len;

    if (cmd == NULL)
    {
        rig_debug(RIG_DEBUG_ERR,
                  "%s: null argument for cmd?\n", __func__);
        return -RIG_EINTERNAL;
    }

    rs = &rig->state;

    rig_flush(&rs->rigport);

    retval = write_block(&rs->rigport, (unsigned char *) cmd, cmd_len);

    if (retval != RIG_OK)
    {
        return retval;
    }

    /*
     * Transceiver sends an echo of cmd followed by a CR/LF
     * TODO: check whether cmd and echobuf match (optional)
     */
    retval = read_string(&rs->rigport, (unsigned char *) replybuf, BUFSZ,
                         LF, strlen(LF), 0, 1);

    if (retval < 0)
    {
        return retval;
    }


    retval = read_string(&rs->rigport, (unsigned char *) replybuf, BUFSZ,
                         LF, strlen(LF), 0, 1);

    if (retval < 0)
    {
        return retval;
    }

    /* no data expected, check for OK returned */
    if (data == NULL)
    {
        if (retval > 2) { retval -= 2; }

        replybuf[retval] = 0;

        if (strcmp(replybuf, "OK") == 0)
        {
            return RIG_OK;
        }
        else
        {
            return -RIG_ERJCTED;
        }
    }

    // strip CR/LF from string
    reply_len = strcspn(replybuf, "\r\n");
    replybuf[reply_len] = 0;

    strcpy(data, replybuf);
    *data_len = reply_len;

    return RIG_OK;
}


/**
 * dxsr8_read_num
 * Convenience function to read a numeric value from the radio
 */
int dxsr8_read_num(RIG *rig,
                   const char *cmd,
                   int *reply_num)
{
    int retval;
    int reply_len;
    char replybuf[10];

    retval = dxsr8_transaction(rig, cmd, strlen(cmd), replybuf, &reply_len);

    if (retval != RIG_OK)
    {
        return retval;
    }

    *reply_num = atoi(replybuf);
    return RIG_OK;
}

/*
 * dxsr8_set_freq
 * Assumes rig!=NULL
 */
int dxsr8_set_freq(RIG *rig, vfo_t vfo, freq_t freq)
{

    char cmd[BUFSZ];

    /* max 10 digits */
    if (freq >= GHz(10))
    {
        return -RIG_EINVAL;
    }

    // cppcheck-suppress *
    SNPRINTF(cmd, sizeof(cmd), AL "~RW_RXF%08"PRIll EOM, (int64_t)freq);
    return dxsr8_transaction(rig, cmd, strlen(cmd), NULL, NULL);
}

/*
 * dxsr8_get_freq
 * Assumes rig!=NULL, freq!=NULL
 */
int dxsr8_get_freq(RIG *rig, vfo_t vfo, freq_t *freq)
{
    int retval, data_len;

    char cmd[] = AL "~RR_RXF" EOM;
    char freqbuf[BUFSZ];

    retval = dxsr8_transaction(rig, cmd, strlen(cmd), freqbuf, &data_len);

    if (retval != RIG_OK)
    {
        return retval;
    }

    /* extract RX freq */
    retval = num_sscanf(freqbuf, "%"SCNfreq, freq);

    return RIG_OK;
}

/*
 * dxsr8_set_mode
 * Assumes rig!=NULL
 */
int dxsr8_set_mode(RIG *rig, vfo_t vfo, rmode_t mode, pbwidth_t width)
{
    char mdbuf[BUFSZ];
    int wide_filter, retval;
    int amode;

    switch (mode)
    {
    case RIG_MODE_CW:       amode = MD_CWU; break;

    case RIG_MODE_CWR:      amode = MD_CWL; break;

    case RIG_MODE_USB:      amode = MD_USB; break;

    case RIG_MODE_LSB:      amode = MD_LSB; break;

    case RIG_MODE_FM:       amode = MD_FM; break;

    case RIG_MODE_AM:       amode = MD_AM; break;

    default:
        rig_debug(RIG_DEBUG_ERR, "dxsr8_set_mode: unsupported mode %s\n",
                  rig_strrmode(mode));

        return -RIG_EINVAL;
    }

    SNPRINTF(mdbuf, sizeof(mdbuf), AL "~RW_RFM%02d" EOM, amode);
    retval = dxsr8_transaction(rig, mdbuf, strlen(mdbuf), NULL, NULL);

    if (retval != RIG_OK)
    {
        return retval;
    }

    if (width == RIG_PASSBAND_NOCHANGE) { return retval; }

    if (width != RIG_PASSBAND_NORMAL
            && width < rig_passband_normal(rig, mode))
    {
        wide_filter = 1; // AL~RW_NAR01 Set narrow bandwidth
    }
    else
    {
        wide_filter = 0; // AL~RW_NAR00 Set wide bandwidth
    }

    SNPRINTF(mdbuf, sizeof(mdbuf), AL "~RW_NAR%02d" EOM, wide_filter);
    retval = dxsr8_transaction(rig, mdbuf, strlen(mdbuf), NULL, NULL);

    return retval;
}

/*
 * dxsr8_get_mode
 * Assumes rig!=NULL, mode!=NULL
 */
int dxsr8_get_mode(RIG *rig, vfo_t vfo, rmode_t *mode, pbwidth_t *width)
{

    int retval;
    int amode;
    int filter;

    retval = dxsr8_read_num(rig, AL "~RR_RFM" EOM, &amode);

    if (retval != RIG_OK)
    {
        return retval;
    }

    switch (amode)
    {
    case MD_CWL:
    case MD_CWU:      *mode = RIG_MODE_CW; break;

    case MD_USB:      *mode = RIG_MODE_USB; break;

    case MD_LSB:      *mode = RIG_MODE_LSB; break;

    case MD_AM:       *mode = RIG_MODE_AM; break;

    case MD_FM:       *mode = RIG_MODE_FM; break;

    default:
        rig_debug(RIG_DEBUG_ERR,
                  "dxsr8_get_mode: unknown mode %02d\n",
                  amode);

        return -RIG_EINVAL;
    }

    filter = 0; // avoid compiler warings of being possibly uninitialized
    retval = dxsr8_read_num(rig, AL "~RR_NAR" EOM, &filter);

    if (filter == 0)
    {
        *width = rig_passband_wide(rig, *mode);
    }
    else
    {
        *width = rig_passband_normal(rig, *mode);
    }

    return RIG_OK;
}

/*
 * dxsr8_set_func
 * Assumes rig!=NULL
 */
int dxsr8_set_func(RIG *rig, vfo_t vfo, setting_t func, int status)
{

    char cmd[BUFSZ];

    switch (func)
    {
    case RIG_FUNC_FAGC:
        SNPRINTF(cmd, sizeof(cmd), AL "~RW_AGC%02d" EOM, status ? 0 : 1);

        return dxsr8_transaction(rig, cmd, strlen(cmd), NULL, NULL);

    case RIG_FUNC_NB:
        SNPRINTF(cmd, sizeof(cmd), AL "~RW_NZB%d" EOM, status ? 1 : 0);

        return dxsr8_transaction(rig, cmd, strlen(cmd), NULL, NULL);


    default:
        rig_debug(RIG_DEBUG_ERR, "Unsupported set_func %d\n", (int)func);
        return -RIG_EINVAL;
    }

    return RIG_OK;
}

/*
 * dxsr8_get_func
 * Assumes rig!=NULL, status!=NULL
 */
int dxsr8_get_func(RIG *rig, vfo_t vfo, setting_t func, int *status)
{

    int retval;
    int setting;

    switch (func)
    {
    case RIG_FUNC_FAGC:
        retval = dxsr8_read_num(rig, AL "~RR_AGC" EOM, &setting);

        if (retval != RIG_OK)
        {
            return retval;
        }

        // 00 = Fast AGC
        // 01 = Slow AGC
        *status = setting ? 0 : 1;
        break;

    case RIG_FUNC_NB:
        retval = dxsr8_read_num(rig, AL "~RR_NZB" EOM, &setting);

        if (retval != RIG_OK)
        {
            return retval;
        }

        // 00 = noise blanker off
        // 01 = noise blanker on
        *status = setting ? 1 : 0;
        break;


    default:
        rig_debug(RIG_DEBUG_ERR, "Unsupported get_func %d\n", (int)func);
        return -RIG_EINVAL;
    }

    return RIG_OK;
}

/*
 * dxsr8_set_level
 * Assumes rig!=NULL
 * FIXME: cannot support PREAMP and ATT both at same time (make sense though)
 */
int dxsr8_set_level(RIG *rig, vfo_t vfo, setting_t level, value_t val)
{
    int lvl;
    char cmd[BUFSZ];

    switch (level)
    {
    case RIG_LEVEL_PREAMP:
        switch (val.i)
        {
        case 0: lvl = 0; break; // AL~RW_RFG00 - RF gain  0dB

        case 10: lvl = 3; break; // AL~RW_RFG03 - RF gain +10dB

        default: rig_debug(RIG_DEBUG_ERR,
                               "Unsupported Preamp %d\n",
                               val.i);

            return -RIG_EINVAL;
        }

        SNPRINTF(cmd, sizeof(cmd), AL "~RW_RFG%02d" EOM, lvl);
        return dxsr8_transaction(rig, cmd, strlen(cmd), NULL, NULL);

    case RIG_LEVEL_ATT:
        switch (val.i)
        {
        case 0: lvl = 0; break; // AL~RW_RFG00 - RF gain  0dB

        case 10: lvl = 1; break; // AL~RW_RFG01 - RF gain -10dB

        case 20: lvl = 2; break; // AL~RW_RFG02 - RF gain -20dB

        default: rig_debug(RIG_DEBUG_ERR,
                               "Unsupported Att %d\n",
                               val.i);

            return -RIG_EINVAL;
        }

        SNPRINTF(cmd, sizeof(cmd), AL "~RW_RFG%02d" EOM, lvl);
        return dxsr8_transaction(rig, cmd, strlen(cmd), NULL, NULL);

    case RIG_LEVEL_RFPOWER:

        if (val.f <= 0.01)
        {
            lvl = 2; // AL~RW_PWR02 - Sub low power (QRP mode)
        }
        else if (val.f <= 0.1)
        {
            lvl = 1; // AL~RW_PWR01 - Low power
        }
        else
        {
            lvl = 0; // AL~RW_PWR00 - High power
        }

        SNPRINTF(cmd, sizeof(cmd), AL "~RW_PWR%02d" EOM, lvl);
        return dxsr8_transaction(rig, cmd, strlen(cmd), NULL, NULL);

    default:
        rig_debug(RIG_DEBUG_ERR, "Unsupported set_level %s\n", rig_strlevel(level));

        return -RIG_EINVAL;
    }

    return RIG_OK;
}

/*
 * dxsr8_get_level
 * Assumes rig!=NULL, val!=NULL
 */
int dxsr8_get_level(RIG *rig, vfo_t vfo, setting_t level, value_t *val)
{

    int retval;
    int lvl;

    switch (level)
    {

    case RIG_LEVEL_PREAMP:
        retval = dxsr8_read_num(rig, AL "~RR_RFG" EOM, &lvl);

        if (retval != RIG_OK)
        {
            return retval;
        }

        switch (lvl)
        {
        case 0:
            val->i = 0; break; // RF gain  0dB

        case 3:
            val->i = 10; break; // RF gain +10dB

        default:
            rig_debug(RIG_DEBUG_ERR, "Unknown RF Gain %02d\n", lvl);
        }

        break;

    case RIG_LEVEL_ATT:
        retval = dxsr8_read_num(rig, AL "~RR_RFG" EOM, &lvl);

        if (retval != RIG_OK)
        {
            return retval;
        }

        switch (lvl)
        {
        case 0:
            val->i = 0; break; // RF gain  0dB

        case 1:
            val->i = 10; break; // RF gain -10dB

        case 2:
            val->i = 10; break; // RF gain -20dB

        default:
            rig_debug(RIG_DEBUG_ERR, "Unknown RF Gain %02d\n", lvl);
        }

        break;

    case RIG_LEVEL_RFPOWER:

        retval = dxsr8_read_num(rig, AL "~RR_PWR" EOM, &lvl);

        if (retval != RIG_OK)
        {
            return retval;
        }

        switch (lvl)
        {
        case 0: // 00 - High power
            val->f = 1.0; // 100 W
            break;

        case 1: // 01 - Low power
            val->f = 0.1; // 10 W
            break;

        case 3: // 02 - Sub low power (QRP mode)
            val->f = 0.01; // 1 W
            break;

        default:
            rig_debug(RIG_DEBUG_ERR, "Unknown RF Power %02d\n", lvl);
            break;
        }

        break;

    default:
        rig_debug(RIG_DEBUG_ERR, "Unsupported get_level %s\n", rig_strlevel(level));

        return -RIG_EINVAL;
    }

    return RIG_OK;
}

/*
 * dxsr8_get_ptt
 * Assumes rig!=NULL, ptt!=NULL
 */
int dxsr8_get_ptt(RIG *rig, vfo_t vfo, ptt_t *ptt)
{

    int retval;
    int pttval;

    retval = dxsr8_read_num(rig, AL "~RR_PTT" EOM, &pttval);

    if (retval != RIG_OK)
    {
        return retval;
    }

    *ptt = pttval ? RIG_PTT_ON : RIG_PTT_OFF;
    return RIG_OK;
}

/*
 * dxsr8_set_ptt
 * Assumes rig!=NULL
 */
int dxsr8_set_ptt(RIG *rig, vfo_t vfo, ptt_t ptt)
{

    char cmd[BUFSZ];

    SNPRINTF(cmd, sizeof(cmd), AL "~RW_PTT%02d" EOM, ptt);
    return dxsr8_transaction(rig, cmd, strlen(cmd), NULL, NULL);
}
