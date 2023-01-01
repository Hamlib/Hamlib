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

#include <hamlib/config.h>

#include <stdio.h>

#include <hamlib/rig.h>
#include "kenwood.h"
#include "cal.h"

// TODO: Copied from TS-480, to be verified
#define TS890_VFO (RIG_VFO_A|RIG_VFO_B)

#define TS890_ALL_MODES (RIG_MODE_LSB|RIG_MODE_USB|RIG_MODE_CW|RIG_MODE_FM|RIG_MODE_AM|RIG_MODE_RTTY|RIG_MODE_CWR|RIG_MODE_RTTYR|RIG_MODE_PSK|RIG_MODE_PSKR|RIG_MODE_PKTLSB|RIG_MODE_PKTUSB|RIG_MODE_PKTFM|RIG_MODE_PKTAM)

// TODO: Copied from TS-480, to be verified
#define TS890_OTHER_TX_MODES (RIG_MODE_CW|RIG_MODE_SSB|RIG_MODE_FM|RIG_MODE_RTTY)
// TODO: Copied from TS-480, to be verified
#define TS890_AM_TX_MODES RIG_MODE_AM

// TODO: Copied from TS-480, to be verified
#define TS890_LEVEL_SET (RIG_LEVEL_RFPOWER|RIG_LEVEL_AF|RIG_LEVEL_RF|RIG_LEVEL_SQL|RIG_LEVEL_AGC|RIG_LEVEL_KEYSPD|RIG_LEVEL_CWPITCH)
#define TS890_LEVEL_GET (RIG_LEVEL_RFPOWER|RIG_LEVEL_AF|RIG_LEVEL_RF|RIG_LEVEL_SQL|RIG_LEVEL_AGC|RIG_LEVEL_KEYSPD|RIG_LEVEL_ALC|RIG_LEVEL_SWR|RIG_LEVEL_COMP_METER|RIG_LEVEL_ID_METER|RIG_LEVEL_VD_METER|RIG_LEVEL_TEMP_METER|RIG_LEVEL_CWPITCH|RIG_LEVEL_RAWSTR|RIG_LEVEL_STRENGTH)
#define TS890_FUNC_ALL (RIG_FUNC_NB|RIG_FUNC_NB2|RIG_FUNC_COMP|RIG_FUNC_VOX|RIG_FUNC_NR|RIG_FUNC_BC|RIG_FUNC_BC2|RIG_FUNC_RIT|RIG_FUNC_XIT|RIG_FUNC_TUNER|RIG_FUNC_SEND_MORSE)

#define TS890_VFO_OPS (RIG_OP_UP|RIG_OP_DOWN|RIG_OP_BAND_UP|RIG_OP_BAND_DOWN|RIG_OP_CPY|RIG_OP_TUNE)

int kenwood_ts890_set_level(RIG *rig, vfo_t vfo, setting_t level, value_t val)
{
    char levelbuf[16];
    int kenwood_val;

    rig_debug(RIG_DEBUG_TRACE, "%s called\n", __func__);

    switch (level)
    {
    case RIG_LEVEL_RF:
        kenwood_val = val.f * 255;
        SNPRINTF(levelbuf, sizeof(levelbuf), "RG%03d", kenwood_val);
        break;

    case RIG_LEVEL_SQL:
        kenwood_val = val.f * 255;
        SNPRINTF(levelbuf, sizeof(levelbuf), "SQ%03d", kenwood_val);
        break;

    case RIG_LEVEL_AGC:
        /* hamlib argument is int, possible values rig.h:enum agc_level_e */
        /* possible values for TS890 0(=off), 1(=slow), 2(=mid), 3(=fast), 4(=off/Last) */
        rig_debug(RIG_DEBUG_VERBOSE, "%s TS890S RIG_LEVEL_AGC\n", __func__);

        switch (val.i)
        {
        case RIG_AGC_OFF:
            kenwood_val = 0;
            break;

        case RIG_AGC_SLOW:
            kenwood_val = 1;
            break;

        case RIG_AGC_MEDIUM:
            kenwood_val = 2;
            break;

        case RIG_AGC_FAST:
            kenwood_val = 3;
            break;

        case RIG_AGC_AUTO:
            kenwood_val = 4;
            break;

        default:
            rig_debug(RIG_DEBUG_ERR, "%s: unsupported agc value", __func__);
            return -RIG_EINVAL;
        }

        SNPRINTF(levelbuf, sizeof(levelbuf), "GC%d", kenwood_val);
        break;

    case RIG_LEVEL_CWPITCH:

        // TODO: Merge this and formatting difference into kenwood.c
        if (val.i < 300 || val.i > 1100)
        {
            return -RIG_EINVAL;
        }

        /* 300 - 1100 Hz -> 000 - 160 */
        kenwood_val = (val.i - 298) / 5; /* Round to nearest 5Hz */
        SNPRINTF(levelbuf, sizeof(levelbuf), "PT%03d", kenwood_val);
        break;

    default:
        return kenwood_set_level(rig, vfo, level, val);
    }

    return kenwood_transaction(rig, levelbuf, NULL, 0);
}

/* Helper to get and parse meter values using RM
 * Note that we turn readings on, but nothing off.
 * 'pips' is the number of LED bars lit in the digital meter, max=70
 */
static
int ts890_get_meter_reading(RIG *rig, char meter, int *pips)
{
    char reading[9];   /* 8 char + '\0' */
    int retval;
    char target[] = "RMx1"; /* Turn on reading this meter */

    target[2] = meter;
    retval = kenwood_transaction(rig, target, NULL, 0);

    if (retval != RIG_OK)
    {
        return retval;
    }

    /* Read the first value */
    retval = kenwood_transaction(rig, "RM", reading, sizeof(reading));

    if (retval != RIG_OK)
    {
        return retval;
    }

    /* Find the one we want */
    while (strncmp(reading, target, 3) != 0)
    {
        /* That wasn't it, get the next one */
        retval = kenwood_transaction(rig, NULL, reading, sizeof(reading));

        if (retval != RIG_OK)
        {
            return retval;
        }

        if (reading[0] != target[0] || reading[1] != target[1])
        {
            /* Somebody else's data, bail */
            return -RIG_EPROTO;
        }
    }

    sscanf(reading + 3, "%4d", pips);
    return RIG_OK;

}

int kenwood_ts890_get_level(RIG *rig, vfo_t vfo, setting_t level, value_t *val)
{
    char ackbuf[50];
    size_t ack_len, ack_len_expected;
    int levelint;
    int retval;

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

        val->f = (float) levelint / 255.;
        return RIG_OK;

    case RIG_LEVEL_AGC:
        retval = kenwood_transaction(rig, "GC", ackbuf, sizeof(ackbuf));
        ack_len_expected = 3;

        if (RIG_OK != retval)
        {
            return retval;
        }

        ack_len = strlen(ackbuf);

        if (ack_len != ack_len_expected)
        {
            return -RIG_EPROTO;
        }

        switch (ackbuf[ack_len_expected - 1])
        {
        case '0':
            val->i = RIG_AGC_OFF;
            break;

        case '1':
            val->i = RIG_AGC_SLOW;

            break;

        case '2':
            val->i = RIG_AGC_MEDIUM;
            break;

        case '3':
            val->i = RIG_AGC_FAST;
            break;

        case '4':
            val->i = RIG_AGC_AUTO;
            break;

        default:
            return -RIG_EPROTO;
        }

        return RIG_OK;

    case RIG_LEVEL_ALC:
        retval = ts890_get_meter_reading(rig, '1', &levelint);

        if (retval != RIG_OK)
        {
            return retval;
        }

        val->f = (float)levelint / 35.0;  /* Half scale [0,35] -> [0.0,1.0] */
        return RIG_OK;

    case RIG_LEVEL_SWR:
        retval = ts890_get_meter_reading(rig, '2', &levelint);

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
        retval = ts890_get_meter_reading(rig, '3', &levelint);

        if (retval != RIG_OK)
        {
            return retval;
        }

        if (levelint < 21) { val->f = (float)levelint / 2.0; }   /* db */
        else if (levelint < 51) { val->f = 10.0 + (float)(levelint - 20) / 3.0; }
        else { val->f = 20.0 + (float)(levelint - 50) / 4.0; }

        return RIG_OK;

    case RIG_LEVEL_ID_METER:
        retval = ts890_get_meter_reading(rig, '4', &levelint);

        if (retval != RIG_OK)
        {
            return retval;
        }

        val->f = (20.0 * (float)levelint) / 70.0;   /* amperes */
        return RIG_OK;

    case RIG_LEVEL_VD_METER:
        retval = ts890_get_meter_reading(rig, '5', &levelint);

        if (retval != RIG_OK)
        {
            return retval;
        }

        val->f = (15.0 * (float)levelint) / 65.0;   /* volts */
        return RIG_OK;

    case RIG_LEVEL_TEMP_METER:
#if 0
        retval = ts890_get_meter_reading(rig, '6', &levelint);

        if (retval != RIG_OK)
        {
            return retval;
        }

#endif
        return -RIG_ENIMPL;

    default:
        return kenwood_get_level(rig, vfo, level, val);
    }

    return -RIG_EINTERNAL;
}

static struct kenwood_priv_caps ts890s_priv_caps =
{
    .cmdtrm = EOM_KEN,
};

/* S-meter calibration table
 * The TS-890S has two distinct S-meter curves, selectable
 * by menu option.  Define both, but since Hamlib has only
 * one slot, use the the IARU one.
 * Values taken from TS-890S In-Depth Manual, p. 8
 */
/* Meter Type 1 - Kenwood specific (default) */
#define TS890_SM_CAL2 { 9, \
  { \
    { 0, -28 }, \
    { 3, -26 }, \
    { 11, -20 }, \
    { 19, -13 }, \
    { 27, -7 }, \
    { 35, 0 }, \
    { 48, 20 }, \
    { 59, 40 }, \
    { 70, 60 }, \
  } }
/* Meter Type 2 - IARU Standard */
#define TS890_SM_CAL1 { 9, \
  { \
    { 0, -54 }, \
    { 3, -48 }, \
    { 11, -36 }, \
    { 19, -24 }, \
    { 27, -12 }, \
    { 35, 0 }, \
    { 48, 20 }, \
    { 59, 40 }, \
    { 70, 60 }, \
  } }

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
 * Copied from ts480_caps, many of the values have not been verified.
 * Notice that some rigs share the same functions.
 */
const struct rig_caps ts890s_caps =
{
    RIG_MODEL(RIG_MODEL_TS890S),
    .model_name = "TS-890S",
    .mfg_name = "Kenwood",
    .version = BACKEND_VER ".10",
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
    .timeout = 500,
    .retry = 10,
    .preamp = {12, RIG_DBLST_END,},
    .attenuator = {12, RIG_DBLST_END,},
    .max_rit = kHz(9.99),
    .max_xit = kHz(9.99),
    .max_ifshift = Hz(0),
    .targetable_vfo = RIG_TARGETABLE_FREQ | RIG_TARGETABLE_MODE,
    .transceive = RIG_TRN_RIG,
    .agc_level_count = 5,
    .agc_levels = { RIG_AGC_OFF, RIG_AGC_SLOW, RIG_AGC_MEDIUM, RIG_AGC_FAST, RIG_AGC_ON },

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

    .str_cal = TS890_SM_CAL1,
    .swr_cal = TS890_SWR_CAL,

    .priv = (void *)& ts890s_priv_caps,
    .rig_init = kenwood_init,
    .rig_open = kenwood_open,
    .rig_cleanup = kenwood_cleanup,
    .set_freq = kenwood_set_freq,
    .get_freq = kenwood_get_freq,
    .set_rit = kenwood_set_rit,
    .get_rit = kenwood_get_rit,
    .set_xit = kenwood_set_xit,
    .get_xit = kenwood_get_xit,
    .set_mode = kenwood_set_mode,
    .get_mode = kenwood_get_mode,
    .set_vfo = kenwood_set_vfo,
    .get_vfo = kenwood_get_vfo_if,
    .set_split_vfo = kenwood_set_split_vfo,
    .get_split_vfo = kenwood_get_split_vfo_if,
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
    .wait_morse =  rig_wait_morse,
    .scan = kenwood_scan,     /* not working, invalid arguments using rigctl; kenwood_scan does only support on/off and not tone and CTCSS scan */
    .has_set_level = TS890_LEVEL_SET,
    .has_get_level = TS890_LEVEL_GET,
    .set_level = kenwood_ts890_set_level,
    .get_level = kenwood_ts890_get_level,
    .level_gran =
    {
#include "level_gran_kenwood.h"
    },
    .has_get_func = TS890_FUNC_ALL,
    .has_set_func = TS890_FUNC_ALL,
    .set_func = kenwood_set_func,
    .get_func = kenwood_get_func,
    .hamlib_check_rig_caps = HAMLIB_CHECK_RIG_CAPS
};
