/*
 *  Hamlib Kenwood backend - TS-890S description
 *  Copyright (c) 2000-2004 by Stephane Fillod and Juergen Rinas
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
/* SPDX-License-Identifier: LGPL-2.1-or-later */

#include <stdio.h>
#include <string.h>
#include <math.h>

#include "hamlib/rig.h"
#include "kenwood.h"
#include "cal.h"
#include "misc.h"

#define TS890_VFO (RIG_VFO_A|RIG_VFO_B)

#define TS890_ALL_MODES (RIG_MODE_LSB|RIG_MODE_USB|RIG_MODE_CW|RIG_MODE_FM|RIG_MODE_AM|RIG_MODE_RTTY|RIG_MODE_CWR|RIG_MODE_RTTYR|RIG_MODE_PSK|RIG_MODE_PSKR|RIG_MODE_PKTLSB|RIG_MODE_PKTUSB|RIG_MODE_PKTFM|RIG_MODE_PKTAM)

#define TS890_OTHER_TX_MODES (RIG_MODE_CW|RIG_MODE_SSB|RIG_MODE_FM|RIG_MODE_RTTY)
#define TS890_AM_TX_MODES RIG_MODE_AM

#define TS890_LEVEL_SET (RIG_LEVEL_RFPOWER|RIG_LEVEL_AF|RIG_LEVEL_RF|RIG_LEVEL_SQL|RIG_LEVEL_AGC|RIG_LEVEL_KEYSPD|RIG_LEVEL_CWPITCH|RIG_LEVEL_ATT|RIG_LEVEL_USB_AF|RIG_LEVEL_USB_AF_INPUT)
#define TS890_LEVEL_GET (RIG_LEVEL_RFPOWER|RIG_LEVEL_AF|RIG_LEVEL_RF|RIG_LEVEL_SQL|RIG_LEVEL_AGC|RIG_LEVEL_KEYSPD|RIG_LEVEL_ALC|RIG_LEVEL_SWR|RIG_LEVEL_COMP_METER|RIG_LEVEL_ID_METER|RIG_LEVEL_VD_METER|RIG_LEVEL_TEMP_METER|RIG_LEVEL_CWPITCH|RIG_LEVEL_RAWSTR|RIG_LEVEL_STRENGTH|RIG_LEVEL_ATT|RIG_LEVEL_USB_AF|RIG_LEVEL_USB_AF_INPUT|RIG_LEVEL_RFPOWER_METER_WATTS)
#define TS890_FUNC_ALL (RIG_FUNC_NB|RIG_FUNC_NB2|RIG_FUNC_COMP|RIG_FUNC_VOX|RIG_FUNC_NR|RIG_FUNC_BC|RIG_FUNC_BC2|RIG_FUNC_RIT|RIG_FUNC_XIT|RIG_FUNC_TUNER|RIG_FUNC_SEND_MORSE|RIG_FUNC_TONE|RIG_FUNC_TSQL)

#define TS890_VFO_OPS (RIG_OP_UP|RIG_OP_DOWN|RIG_OP_BAND_UP|RIG_OP_BAND_DOWN|RIG_OP_CPY|RIG_OP_TUNE)

static int kenwood_ts890_set_level(RIG *rig, vfo_t vfo, setting_t level,
                                   value_t val)
{
    char levelbuf[16];
    const char *command_string;
    int kenwood_val, retval;
    gran_t *level_info;

    rig_debug(RIG_DEBUG_TRACE, "%s called\n", __func__);

    retval = check_level_param(rig, level, val, &level_info);

    if (retval != RIG_OK)
    {
        return retval;
    }

    switch (level)
    {
    case RIG_LEVEL_AGC:
        /* hamlib argument is int, possible values rig.h:enum agc_level_e */
        /* possible values for TS890 0(=off), 1(=slow), 2(=mid), 3(=fast), 4(=off/Last) */
        rig_debug(RIG_DEBUG_VERBOSE, "%s TS890S RIG_LEVEL_AGC\n", __func__);

        kenwood_val = -1; /* Flag invalid value */

        for (int j = 0; j < rig->caps->agc_level_count; j++)
        {
            if (val.i == rig->caps->agc_levels[j])
            {
                kenwood_val = j;
                break;
            }
        }

        if (kenwood_val < 0)
        {
            rig_debug(RIG_DEBUG_ERR, "%s: unsupported agc value:%d\n", __func__, val.i);
            return -RIG_EINVAL;
        }

        SNPRINTF(levelbuf, sizeof(levelbuf), "GC%d", kenwood_val);
        return kenwood_transaction(rig, levelbuf, NULL, 0); /* Odd man out */

    case RIG_LEVEL_RF:
        command_string = "RG%03d";
        break;

    case RIG_LEVEL_SQL:
        command_string = "SQ%03d";
        break;

    case RIG_LEVEL_USB_AF:
        command_string = "EX00708 %03d";
        break;

    case RIG_LEVEL_USB_AF_INPUT:
        command_string = "EX00706 %03d";
        break;

    default:
        return kenwood_set_level(rig, vfo, level, val);
    }

    //TODO: Add use of RIG_LEVEL_IS_FLOAT() if need to handle int level
    kenwood_val = val.f / level_info->step.f + 0.5;
    SNPRINTF(levelbuf, sizeof(levelbuf), command_string, kenwood_val);
    return kenwood_transaction(rig, levelbuf, NULL, 0);
}

static int kenwood_ts890_get_level(RIG *rig, vfo_t vfo, setting_t level,
                                   value_t *val)
{
    char ackbuf[50];
    size_t ack_len, ack_len_expected, len;
    int levelint;
    int retval;
    const char *command_string;
    gran_t *level_info;

    level_info = &rig->caps->level_gran[rig_setting2idx(level)];
    rig_debug(RIG_DEBUG_TRACE, "%s called\n", __func__);

    switch (level)
    {
    // TODO: This doesn't appear in TS890_LEVEL_GET - should it?
    case RIG_LEVEL_VOXDELAY:
        retval = kenwood_safe_transaction(rig, "VD0", ackbuf, sizeof(ackbuf), 6);

        if (retval != RIG_OK)
        {
            return retval;
        }

        sscanf(ackbuf + 3, "%d", &levelint);
        val->i = levelint * 3 /
                 2;               /* 150ms units converted to 100ms units */
        return RIG_OK;

    case RIG_LEVEL_RF:
        retval = kenwood_transaction(rig, "RG", ackbuf, sizeof(ackbuf));

        if (RIG_OK != retval)
        {
            return retval;
        }

        ack_len = strlen(ackbuf);

        if (5 != ack_len)
        {
            return -RIG_EPROTO;
        }

        if (1 != sscanf(&ackbuf[2], "%d", &levelint))
        {
            return -RIG_EPROTO;
        }

        val->f = levelint / (float) 255;
        return RIG_OK;

    case RIG_LEVEL_SQL:
        retval = kenwood_transaction(rig, "SQ", ackbuf, sizeof(ackbuf));
        ack_len_expected = 5;

        if (RIG_OK != retval)
        {
            return retval;
        }

        ack_len = strlen(ackbuf);

        if (ack_len != ack_len_expected)
        {
            return -RIG_EPROTO;
        }

        if (1 != sscanf(&ackbuf[ack_len_expected - 3], "%d", &levelint))
        {
            return -RIG_EPROTO;
        }

        val->f = (float) levelint * level_info->step.f;
        return RIG_OK;

    case RIG_LEVEL_AGC:
        retval = kenwood_safe_transaction(rig, "GC", ackbuf, sizeof(ackbuf), 3);

        if (RIG_OK != retval)
        {
            return retval;
        }

        levelint = ackbuf[2] - '0';  /* atoi */

        if (levelint < 0 || levelint >= rig->caps->agc_level_count)
        {
            rig_debug(RIG_DEBUG_ERR, "%s: unknown agc value: %s\n", __func__, ackbuf);
            return -RIG_EPROTO;
        }

        val->i = rig->caps->agc_levels[levelint];
        return RIG_OK;

    case RIG_LEVEL_ALC:
        retval = get_kenwood_meter_reading(rig, '1', &levelint);

        if (retval != RIG_OK)
        {
            return retval;
        }

        val->f = (float)levelint / 35.0;  /* Half scale [0,35] -> [0.0,1.0] */
        return RIG_OK;

    case RIG_LEVEL_SWR:
        retval = get_kenwood_meter_reading(rig, '2', &levelint);

        if (retval != RIG_OK)
        {
            return retval;
        }

        if (rig->caps->swr_cal.size)
        {
            val->f = rig_raw2val_float(levelint, &rig->caps->swr_cal);
        }
        else
        {
            /* Linear approximations of a very non-linear function */
            if (levelint < 12) { val->f = 1.0 + (float)levelint / 22.0; }
            else if (levelint < 24) { val->f = 1.5 + (float)(levelint - 11) / 24.0; }
            else if (levelint < 36) { val->f = 2.0 + (float)(levelint - 23) / 12.0; }
            else { val->f = 3.0 + (float)(levelint - 35) / 6.0; }
        }

        return RIG_OK;

    case RIG_LEVEL_COMP_METER:
        retval = get_kenwood_meter_reading(rig, '3', &levelint);

        if (retval != RIG_OK)
        {
            return retval;
        }

        if (levelint < 21) { val->f = (float)levelint / 2.0; }   /* db */
        else if (levelint < 51) { val->f = 10.0 + (float)(levelint - 20) / 3.0; }
        else { val->f = 20.0 + (float)(levelint - 50) / 4.0; }

        return RIG_OK;

    case RIG_LEVEL_ID_METER:
        retval = get_kenwood_meter_reading(rig, '4', &levelint);

        if (retval != RIG_OK)
        {
            return retval;
        }

        val->f = (20.0 * (float)levelint) / 70.0;   /* amperes */
        return RIG_OK;

    case RIG_LEVEL_VD_METER:
        retval = get_kenwood_meter_reading(rig, '5', &levelint);

        if (retval != RIG_OK)
        {
            return retval;
        }

        val->f = (15.0 * (float)levelint) / 65.0;   /* volts */
        return RIG_OK;

    case RIG_LEVEL_TEMP_METER:
#if 0
        retval = get_kenwood_meter_reading(rig, '6', &levelint);

        if (retval != RIG_OK)
        {
            return retval;
        }

#endif
        return -RIG_ENIMPL;

    case RIG_LEVEL_STRENGTH:
    case RIG_LEVEL_RFPOWER_METER_WATTS:
    {
        const cal_table_float_t *table;
        ptt_t ptt = RIG_PTT_OFF;
        /* Values taken from the TS-890S In-Depth Manual (IDM), p. 8
         * 0.03 - 21.5 MHz, Preamp 1
         */
        /* Meter Type 1 - Kenwood specific, factory default */
        static const cal_table_float_t meter_type1 =
        {
            9, { { 0, -28.4f}, { 3, -26}, {11, -19.5f},
                {19, -13}, {27, -6.5f}, {35, 0},
                {48, 20}, {59, 40}, {70, 60}
            }
        };
        /* Meter Type 2 - IARU recommended */
        static const cal_table_float_t meter_type2 =
        {
            9, { { 0, -54}, { 3, -48}, {11, -36},
                {19, -24}, {27, -12}, {35, 0},
                {48, 20}, {59, 40}, {70, 60}
            }
        };
        static cal_table_t power_meter =
        {
            7, { { 0, 0}, { 5, 5}, { 10, 10}, {19, 25},
                { 35, 50}, { 59, 100}, { 70, 150}
            }
        };

        /* Make sure we're asking the right question */
        kenwood_get_ptt(rig, vfo, &ptt);

        if ((ptt == RIG_PTT_OFF) != (level == RIG_LEVEL_STRENGTH))
        {
            /* We're sorry, the number you have dialed is not in service */
            if (level == RIG_LEVEL_RFPOWER_METER_WATTS)
            {
                val->f = 0;
            }
            else
            {
                val->i = 0;
            }

            return RIG_OK;
        }

        /* Find out which meter type is in use */
        retval = kenwood_safe_transaction(rig, "EX00011", ackbuf, sizeof(ackbuf), 11);

        if (retval != RIG_OK)
        {
            return retval;
        }

        if (strncmp(ackbuf + 8, "000", 3) == 0)
        {
            table = &meter_type1;
        }
        else if (strncmp(ackbuf + 8, "001", 3) == 0)
        {
            table = &meter_type2;
        }
        else
        {
            rig_debug(RIG_DEBUG_ERR, "%s: Unexpected meter type: %s\n",
                      __func__, ackbuf);
            return -RIG_EPROTO;
        }

        retval = kenwood_safe_transaction(rig, "SM", ackbuf, 10, 6);

        if (retval != RIG_OK)
        {
            return retval;
        }

        sscanf(ackbuf + 2, "%d", &levelint);

        if (level == RIG_LEVEL_RFPOWER_METER_WATTS)
        {
            val->f = roundf(rig_raw2val(levelint, &power_meter));
        }
        else
        {
            /* Convert reading back to dB (rounded) */
            val->i = (int)floorf(rig_raw2val_float(levelint, table) + 0.5f);
        }

        return RIG_OK;
    }

    case RIG_LEVEL_USB_AF:
    case RIG_LEVEL_USB_AF_INPUT:
        if (level == RIG_LEVEL_USB_AF)
        { command_string = "EX00708"; }
        else
        { command_string = "EX00706"; }

        len = strlen(command_string);
        retval = kenwood_safe_transaction(rig, command_string, ackbuf, sizeof(ackbuf),
                                          len + 4);

        if (retval != RIG_OK) { return retval; }

        sscanf(&ackbuf[len + 1], "%3d", &levelint);  /* Skip the extra space */
        val->f = levelint * level_info->step.f;
        return RIG_OK;

    default:
        return kenwood_get_level(rig, vfo, level, val);
    }

    return -RIG_EINTERNAL;
}

static int ts890_set_func(RIG *rig, vfo_t vfo, setting_t func, int status)
{
    int mask, retval;
    char current[4];

    switch (func)
    {
    case RIG_FUNC_TONE:
        mask = 1;
        break;

    case RIG_FUNC_TSQL:
        mask = 2;
        break;

    default:
        return (kenwood_set_func(rig, vfo, func, status));
    }

    retval = kenwood_safe_transaction(rig, "TO", current, sizeof(current), 3);

    if (retval != RIG_OK)
    {
        return (retval);
    }

    current[2] &= ~mask;
    current[2] |= status == 0 ? 0 : mask;
    return kenwood_transaction(rig, current, NULL, 0);
}

static int ts890_get_func(RIG *rig, vfo_t vfo, setting_t func, int *status)
{
    int mask, retval;
    char current[4];

    switch (func)
    {
    case RIG_FUNC_TONE:
        mask = 1;
        break;

    case RIG_FUNC_TSQL:
        mask = 2;
        break;

    default:
        return (kenwood_get_func(rig, vfo, func, status));
    }

    retval = kenwood_safe_transaction(rig, "TO", current, sizeof(current), 3);

    if (retval != RIG_OK)
    {
        return retval;
    }

    *status = (current[2] & mask) ? 1 : 0;
    return RIG_OK;
}

/*
 *  Gets split VFO status
 *
 */
static int ts890s_get_split_vfo(RIG *rig, vfo_t rxvfo, split_t *split,
                                vfo_t *txvfo)
{
    char buf[4];
    int retval;
    struct rig_state *rs = STATE(rig);
    struct kenwood_priv_data *priv = rs->priv;

    if (RIG_OK == (retval = kenwood_safe_transaction(rig, "FT", buf, sizeof(buf),
                                3)))
    {
        vfo_t tvfo;
        if ('0' == buf[2])
        {
            tvfo = RIG_VFO_A;
        }
        else if ('1' == buf[2])
        {
            tvfo = RIG_VFO_B;
        }
        else if ('3' == buf[2])
        {
            tvfo = RIG_VFO_MEM;
        }
        else
        {
            rig_debug(RIG_DEBUG_ERR, "%s: Unknown VFO - %s\n", __func__, buf);
            return -RIG_EPROTO;
        }

        *txvfo = priv->tx_vfo = rs->tx_vfo = tvfo;
	// Now get split status
	retval = kenwood_safe_transaction(rig, "TB", buf, sizeof buf, 3);
	if (RIG_OK != retval) {return retval;}
        *split = priv->split = buf[2] == '1';
    }

    return retval;
}


static struct kenwood_priv_caps ts890s_priv_caps =
{
    .cmdtrm = EOM_KEN,
    .tone_table_base = 0,
};

/* SWR meter calibration table */
/* The full scale value reads infinity, so arbitrary */
#define TS890_SWR_CAL { 5, \
      { \
    { 0, 1.0 }, \
    { 11, 1.5 }, \
    { 23, 2.0 }, \
    { 35, 3.0 }, \
    { 70, 15.0 } \
      } }

/*
 * TS-890S rig capabilities
 * Notice that some rigs share the same functions.
 */
struct rig_caps ts890s_caps =
{
    RIG_MODEL(RIG_MODEL_TS890S),
    .model_name = "TS-890S",
    .mfg_name = "Kenwood",
    .version = BACKEND_VER ".16",
    .copyright = "LGPL",
    .status = RIG_STATUS_STABLE,
    .rig_type = RIG_TYPE_TRANSCEIVER,
    .ptt_type = RIG_PTT_RIG_MICDATA,
    .dcd_type = RIG_DCD_RIG,
    .port_type = RIG_PORT_SERIAL,
    .serial_rate_min = 4800,
    .serial_rate_max = 115200,
    .serial_data_bits = 8,
    .serial_stop_bits = 1,
    .serial_parity = RIG_PARITY_NONE,
    .serial_handshake = RIG_HANDSHAKE_NONE,
    .write_delay = 0,
    .post_write_delay = 0,
    .timeout = 200,
    .retry = 1,
    .preamp = {12, RIG_DBLST_END,},
    .attenuator = {6, 12, 18, RIG_DBLST_END,},
    .max_rit = kHz(9.99),
    .max_xit = kHz(9.99),
    .max_ifshift = Hz(0),
    .targetable_vfo = RIG_TARGETABLE_FREQ | RIG_TARGETABLE_MODE,
    .transceive = RIG_TRN_RIG,
    .agc_level_count = 5,
    .agc_levels = { RIG_AGC_OFF, RIG_AGC_SLOW, RIG_AGC_MEDIUM, RIG_AGC_FAST, RIG_AGC_ON },
    .chan_list =  {
        { 1,   6, RIG_MTYPE_VOICE },
        { 1,   8, RIG_MTYPE_MORSE },
        RIG_CHAN_END,
    },
    .rx_range_list1 = {
        {kHz(100),   Hz(59999999), TS890_ALL_MODES, -1, -1, TS890_VFO},
        RIG_FRNG_END,
    }, /*!< Receive frequency range list for ITU region 1 */
    .tx_range_list1 = {
        {kHz(1810),  kHz(1850),  TS890_OTHER_TX_MODES, 5000, 100000, TS890_VFO},   /* 100W class */
        {kHz(1810),  kHz(1850),  TS890_AM_TX_MODES, 5000, 25000, TS890_VFO},       /* 25W class */
        {kHz(3500),  kHz(3800),  TS890_OTHER_TX_MODES, 5000, 100000, TS890_VFO},
        {kHz(3500),  kHz(3800),  TS890_AM_TX_MODES, 5000, 25000, TS890_VFO},
        {MHz(7),     kHz(7200),  TS890_OTHER_TX_MODES, 5000, 100000, TS890_VFO},
        {MHz(7),     kHz(7200),  TS890_AM_TX_MODES, 5000, 25000, TS890_VFO},
        {kHz(10100), kHz(10150), TS890_OTHER_TX_MODES, 5000, 100000, TS890_VFO},
        {kHz(10100), kHz(10150), TS890_AM_TX_MODES, 5000, 25000, TS890_VFO},
        {MHz(14),    kHz(14350), TS890_OTHER_TX_MODES, 5000, 100000, TS890_VFO},
        {MHz(14),    kHz(14350), TS890_AM_TX_MODES, 5000, 25000, TS890_VFO},
        {kHz(18068), kHz(18168), TS890_OTHER_TX_MODES, 5000, 100000, TS890_VFO},
        {kHz(18068), kHz(18168), TS890_AM_TX_MODES, 5000, 25000, TS890_VFO},
        {MHz(21),    kHz(21450), TS890_OTHER_TX_MODES, 5000, 100000, TS890_VFO},
        {MHz(21),    kHz(21450), TS890_AM_TX_MODES, 5000, 25000, TS890_VFO},
        {kHz(24890), kHz(24990), TS890_OTHER_TX_MODES, 5000, 100000, TS890_VFO},
        {kHz(24890), kHz(24990), TS890_AM_TX_MODES, 5000, 25000, TS890_VFO},
        {MHz(28),    kHz(29700), TS890_OTHER_TX_MODES, 5000, 100000, TS890_VFO},
        {MHz(28),    kHz(29700), TS890_AM_TX_MODES, 5000, 25000, TS890_VFO},
        {MHz(50),    kHz(52000), TS890_OTHER_TX_MODES, 5000, 100000, TS890_VFO},
        {MHz(50),    kHz(52000), TS890_AM_TX_MODES, 5000, 25000, TS890_VFO},
        RIG_FRNG_END,
    },  /*!< Transmit frequency range list for ITU region 1 */
    .rx_range_list2 = {
        {kHz(100),   Hz(59999999), TS890_ALL_MODES, -1, -1, TS890_VFO},
        RIG_FRNG_END,
    },  /*!< Receive frequency range list for ITU region 2 */
    .tx_range_list2 = {
        {kHz(1800),  MHz(2) - 1, TS890_OTHER_TX_MODES, 5000, 100000, TS890_VFO},  /* 100W class */
        {kHz(1800),  MHz(2) - 1, TS890_AM_TX_MODES, 5000, 25000, TS890_VFO},      /* 25W class */
        {kHz(3500),  MHz(4) - 1, TS890_OTHER_TX_MODES, 5000, 100000, TS890_VFO},
        {kHz(3500),  MHz(4) - 1, TS890_AM_TX_MODES, 5000, 25000, TS890_VFO},
        {kHz(5250),  kHz(5450),  TS890_OTHER_TX_MODES, 5000, 100000, TS890_VFO},
        {kHz(5250),  kHz(5450),  TS890_AM_TX_MODES, 5000, 25000, TS890_VFO},
        {MHz(7),     kHz(7300),  TS890_OTHER_TX_MODES, 5000, 100000, TS890_VFO},
        {MHz(7),     kHz(7300),  TS890_AM_TX_MODES, 5000, 25000, TS890_VFO},
        {kHz(10100), kHz(10150), TS890_OTHER_TX_MODES, 5000, 100000, TS890_VFO},
        {kHz(10100), kHz(10150), TS890_AM_TX_MODES, 5000, 25000, TS890_VFO},
        {MHz(14),    kHz(14350), TS890_OTHER_TX_MODES, 5000, 100000, TS890_VFO},
        {MHz(14),    kHz(14350), TS890_AM_TX_MODES, 5000, 25000, TS890_VFO},
        {kHz(18068), kHz(18168), TS890_OTHER_TX_MODES, 5000, 100000, TS890_VFO},
        {kHz(18068), kHz(18168), TS890_AM_TX_MODES, 5000, 25000, TS890_VFO},
        {MHz(21),    kHz(21450), TS890_OTHER_TX_MODES, 5000, 100000, TS890_VFO},
        {MHz(21),    kHz(21450), TS890_AM_TX_MODES, 5000, 25000, TS890_VFO},
        {kHz(24890), kHz(24990), TS890_OTHER_TX_MODES, 5000, 100000, TS890_VFO},
        {kHz(24890), kHz(24990), TS890_AM_TX_MODES, 5000, 25000, TS890_VFO},
        {MHz(28),    kHz(29700), TS890_OTHER_TX_MODES, 5000, 100000, TS890_VFO},
        {MHz(28),    kHz(29700), TS890_AM_TX_MODES, 5000, 25000, TS890_VFO},
        {MHz(50),    kHz(52000), TS890_OTHER_TX_MODES, 5000, 100000, TS890_VFO},
        {MHz(50),    kHz(52000), TS890_AM_TX_MODES, 5000, 25000, TS890_VFO},
        RIG_FRNG_END,
    }, /*!< Transmit frequency range list for ITU region 2 */
    .tuning_steps =  {
        {TS890_ALL_MODES, kHz(1)},
        {TS890_ALL_MODES, Hz(2500)},
        {TS890_ALL_MODES, kHz(5)},
        {TS890_ALL_MODES, Hz(6250)},
        {TS890_ALL_MODES, kHz(10)},
        {TS890_ALL_MODES, Hz(12500)},
        {TS890_ALL_MODES, kHz(15)},
        {TS890_ALL_MODES, kHz(20)},
        {TS890_ALL_MODES, kHz(25)},
        {TS890_ALL_MODES, kHz(30)},
        {TS890_ALL_MODES, kHz(100)},
        {TS890_ALL_MODES, kHz(500)},
        {TS890_ALL_MODES, MHz(1)},
        {TS890_ALL_MODES, 0},  /* any tuning step */
        RIG_TS_END,
    },
    /* mode/filter list, remember: order matters! */
    .filters =  {
        {RIG_MODE_SSB, kHz(2.4)},
        {RIG_MODE_CW, Hz(200)},
        {RIG_MODE_RTTY, Hz(500)},
        {RIG_MODE_AM, kHz(9)},
        {RIG_MODE_FM, kHz(15)},
        RIG_FLT_END,
    },
    .vfo_ops = TS890_VFO_OPS,
    .ctcss_list = kenwood51_ctcss_list,

    .swr_cal = TS890_SWR_CAL,

    .priv = (void *)& ts890s_priv_caps,
    .rig_init = kenwood_init,
    .rig_open = kenwood_open,
    .rig_cleanup = kenwood_cleanup,
    .set_freq = kenwood_set_freq,
    .get_freq = kenwood_get_freq,
    .set_rit = kenwood_set_rit_new,
    .get_rit = kenwood_get_rit_new,
    .set_xit = kenwood_set_rit_new,  // Same routines as for RIT
    .get_xit = kenwood_get_rit_new,  // Same
    .set_mode = kenwood_set_mode,
    .get_mode = kenwood_get_mode,
    .set_vfo = kenwood_set_vfo,
    .get_vfo = kenwood_get_vfo_if,
    .vfo_op = kenwood_vfo_op,
    .set_split_vfo = kenwood_set_split_vfo,
    .get_split_vfo = ts890s_get_split_vfo,
    .set_ctcss_tone = kenwood_set_ctcss_tone_tn,
    .get_ctcss_tone = kenwood_get_ctcss_tone,
    .set_ctcss_sql = kenwood_set_ctcss_sql,
    .get_ctcss_sql = kenwood_get_ctcss_sql,
    .get_ptt = kenwood_get_ptt,
    .set_ptt = kenwood_set_ptt,
    .get_dcd = kenwood_get_dcd,
    .set_powerstat = kenwood_set_powerstat,
    .get_powerstat = kenwood_get_powerstat,
    .get_info = NULL,
    .reset = kenwood_reset,
    .set_ant = kenwood_set_ant,
    .get_ant = kenwood_get_ant,
    .send_morse =  kenwood_send_morse,
    .stop_morse =  kenwood_stop_morse,
    .send_voice_mem = kenwood_send_voice_mem,
    .stop_voice_mem = kenwood_stop_voice_mem,
    .wait_morse =  rig_wait_morse,
    .scan = kenwood_scan,     /* not working, invalid arguments using rigctl; kenwood_scan does only support on/off and not tone and CTCSS scan */
    .has_set_level = TS890_LEVEL_SET,
    .has_get_level = TS890_LEVEL_GET,
    .set_level = kenwood_ts890_set_level,
    .get_level = kenwood_ts890_get_level,
    .level_gran =
    {
#define NO_LVL_ATT
#define NO_LVL_CWPITCH
#define NO_LVL_SQL
#define NO_LVL_USB_AF
#define NO_LVL_USB_AF_INPUT
#include "level_gran_kenwood.h"
#undef NO_LVL_ATT
#undef NO_LVL_CWPITCH
#undef NO_LVL_SQL
#undef NO_LVL_USB_AF
#undef NO_LVL_USB_AF_INPUT
        [LVL_ATT]     = { .min = { .i = 0 }, .max = { .i = 18 }, .step = { .i = 6 } },
        [LVL_CWPITCH] = { .min = { .i = 300 }, .max = { .i = 1100 }, .step = { .i = 5 } },
        [LVL_SQL] = { .min = { .f = 0 }, .max = { .f = 1.0f }, .step = { .f = 1.0 / 255.0 } },
        [LVL_USB_AF] = { .min = { .f = 0 }, .max = { .f = 1.0f }, .step = { .f = 1.0 / 100.0 } },
        [LVL_USB_AF_INPUT] = { .min = { .f = 0 }, .max = { .f = 1.0f }, .step = { .f = 1.0 / 100.0 } },
    },
    .has_get_func = TS890_FUNC_ALL,
    .has_set_func = TS890_FUNC_ALL,
    .set_func = ts890_set_func,
    .get_func = ts890_get_func,
    .get_clock = kenwood_get_clock,
    .set_clock = kenwood_set_clock,
    .morse_qsize = 24,
    .hamlib_check_rig_caps = HAMLIB_CHECK_RIG_CAPS
};
