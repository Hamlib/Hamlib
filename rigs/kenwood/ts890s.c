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

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <stdio.h>

#include <hamlib/rig.h>
#include "kenwood.h"

// TODO: Copied from TS-480, to be verified
#define TS890_VFO (RIG_VFO_A|RIG_VFO_B)

#define TS890_ALL_MODES (RIG_MODE_LSB|RIG_MODE_USB|RIG_MODE_CW|RIG_MODE_FM|RIG_MODE_AM|RIG_MODE_RTTY|RIG_MODE_CWR|RIG_MODE_RTTYR|RIG_MODE_PSK|RIG_MODE_PSKR|RIG_MODE_PKTLSB|RIG_MODE_PKTUSB|RIG_MODE_PKTFM|RIG_MODE_PKTAM)

// TODO: Copied from TS-480, to be verified
#define TS890_OTHER_TX_MODES (RIG_MODE_CW|RIG_MODE_SSB|RIG_MODE_FM|RIG_MODE_RTTY)
// TODO: Copied from TS-480, to be verified
#define TS890_AM_TX_MODES RIG_MODE_AM

// TODO: Copied from TS-480, to be verified
#define TS890_LEVEL_ALL (RIG_LEVEL_RFPOWER|RIG_LEVEL_AF|RIG_LEVEL_RF|RIG_LEVEL_SQL|RIG_LEVEL_AGC)
#define TS890_FUNC_ALL (RIG_FUNC_NB|RIG_FUNC_NB2|RIG_FUNC_COMP|RIG_FUNC_VOX|RIG_FUNC_NR|RIG_FUNC_NR|RIG_FUNC_BC|RIG_FUNC_BC2|RIG_FUNC_RIT|RIG_FUNC_XIT|RIG_FUNC_TUNER)

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
        sprintf(levelbuf, "RG%03d", kenwood_val);
        break;

    case RIG_LEVEL_SQL:
        kenwood_val = val.f * 255;
        sprintf(levelbuf, "SQ%03d", kenwood_val);
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

        sprintf(levelbuf, "GC%d", kenwood_val);
        break;

    default:
        return kenwood_set_level(rig, vfo, level, val);
    }

    return kenwood_transaction(rig, levelbuf, NULL, 0);
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

    default:
        return kenwood_get_level(rig, vfo, level, val);
    }
    return -RIG_EINTERNAL;
}

static struct kenwood_priv_caps ts890s_priv_caps =
{
    .cmdtrm = EOM_KEN,
};

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
    .version = BACKEND_VER ".1",
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
    .retry = 10,
    .preamp = {12, RIG_DBLST_END,},
    .attenuator = {12, RIG_DBLST_END,},
    .max_rit = kHz(9.99),
    .max_xit = kHz(9.99),
    .max_ifshift = Hz(0),
    .targetable_vfo = RIG_TARGETABLE_FREQ,
    .transceive = RIG_TRN_RIG,

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
    .scan = kenwood_scan,     /* not working, invalid arguments using rigctl; kenwood_scan does only support on/off and not tone and CTCSS scan */
    .has_set_level = TS890_LEVEL_ALL,
    .has_get_level = TS890_LEVEL_ALL,
    .set_level = kenwood_ts890_set_level,
    .get_level = kenwood_ts890_get_level,
    .has_get_func = TS890_FUNC_ALL,
    .has_set_func = TS890_FUNC_ALL,
    .set_func = kenwood_set_func,
    .get_func = kenwood_get_func,
};
