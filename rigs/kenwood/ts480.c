/*
 *  Hamlib Kenwood backend - TS480 description
 *  Hamlib Kenwood backend - TS890s description
 *  Copyright (c) 2000-2004 by Stephane Fillod and Juergen Rinas
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

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <stdlib.h>
#include <stdio.h>

#include <hamlib/rig.h>
#include "idx_builtin.h"
#include "kenwood.h"

#define TS480_ALL_MODES (RIG_MODE_AM|RIG_MODE_CW|RIG_MODE_CWR|RIG_MODE_SSB|RIG_MODE_FM|RIG_MODE_RTTY|RIG_MODE_RTTYR)
#define PS8000A_ALL_MODES (RIG_MODE_AM|RIG_MODE_AMS|RIG_MODE_CW|RIG_MODE_CWR|RIG_MODE_SSB|RIG_MODE_FM|RIG_MODE_RTTY|RIG_MODE_RTTYR)
#define TS890_ALL_MODES (RIG_MODE_LSB|RIG_MODE_USB|RIG_MODE_CW|RIG_MODE_FM|RIG_MODE_AM|RIG_MODE_RTTY|RIG_MODE_CWR|RIG_MODE_RTTYR|RIG_MODE_PSK|RIG_MODE_PSKR|RIG_MODE_PKTLSB|RIG_MODE_PKTUSB|RIG_MODE_PKTFM|RIG_MODE_PKTAM)

#define TS480_OTHER_TX_MODES (RIG_MODE_CW|RIG_MODE_SSB|RIG_MODE_FM|RIG_MODE_RTTY)
#define TS480_AM_TX_MODES RIG_MODE_AM
#define TS480_VFO (RIG_VFO_A|RIG_VFO_B)

#define TS480_LEVEL_ALL (RIG_LEVEL_RFPOWER|RIG_LEVEL_AF|RIG_LEVEL_RF|RIG_LEVEL_SQL|RIG_LEVEL_AGC)
#define TS480_FUNC_ALL (RIG_FUNC_NB|RIG_FUNC_COMP|RIG_FUNC_VOX|RIG_FUNC_NR|RIG_FUNC_NR|RIG_FUNC_BC|RIG_FUNC_BC2|RIG_FUNC_RIT|RIG_FUNC_XIT)
#define TS890_FUNC_ALL (RIG_FUNC_NB|RIG_FUNC_NB2|RIG_FUNC_COMP|RIG_FUNC_VOX|RIG_FUNC_NR|RIG_FUNC_NR|RIG_FUNC_BC|RIG_FUNC_BC2|RIG_FUNC_RIT|RIG_FUNC_XIT)


/*
 * kenwood_ts480_get_info
 * Assumes rig!=NULL
 */
static const char *
kenwood_ts480_get_info(RIG *rig)
{
    char firmbuf[50];
    int retval;
    size_t firm_len;

    retval = kenwood_transaction(rig, "TY", firmbuf, sizeof(firmbuf));

    if (retval != RIG_OK)
    {
        return NULL;
    }

    firm_len = strlen(firmbuf);

    if (firm_len != 5)
    {
        rig_debug(RIG_DEBUG_ERR, "%s: wrong answer len=%d\n", __func__, (int)firm_len);
        return NULL;
    }

    switch (firmbuf[4])
    {
    case '0':
        return "TS-480HX (200W)";

    case '1':
        return "TS-480SAT (100W + AT)";

    case '2':
        return "Japanese 50W type";

    case '3':
        return "Japanese 20W type";

    default:
        return "Firmware: unknown";
    }
}


/*
 * kenwood_ts480_set_level
 * Assumes rig!=NULL
 *
 * set levels of most functions
 *
 * WARNING: the commands differ slightly from the general versions in kenwood.c
 * e.g.: "SQ"=>"SQ0" , "AG"=>"AG0"
 */
int
kenwood_ts480_set_level(RIG *rig, vfo_t vfo, setting_t level, value_t val)
{
    char levelbuf[16];
    int kenwood_val;
    int rf_max_level = 100; /* 100 for TS-480 and 255 for TS-890S */

    rig_debug(RIG_DEBUG_TRACE, "%s called\n", __func__);

    switch (level)
    {
    case RIG_LEVEL_RFPOWER:
        kenwood_val = val.f * 100;    /* level for TS480SAT is from 0.. 100W in SSB */
        sprintf(levelbuf, "PC%03d", kenwood_val);
        break;

    case RIG_LEVEL_AF:
        kenwood_val = val.f * 255;    /* possible values for TS480 are 000.. 255 */

        if (rig->caps->rig_model == RIG_MODEL_TS890S)
        {
            sprintf(levelbuf, "AG%03d", kenwood_val);
        }
        else
        {
            sprintf(levelbuf, "AG0%03d", kenwood_val);
        }

        break;

    case RIG_LEVEL_RF:
        if (rig->caps->rig_model == RIG_MODEL_TS890S)
        {
            rf_max_level = 255;
        }

        kenwood_val = val.f *
                      rf_max_level;   /* possible values for TS480 are 000.. 100 */
        sprintf(levelbuf, "RG%03d", kenwood_val);
        break;

    case RIG_LEVEL_SQL:
        kenwood_val = val.f * 255;    /* possible values for TS480 are 000.. 255 */

        if (rig->caps->rig_model == RIG_MODEL_TS890S)
        {
            sprintf(levelbuf, "SQ%03d", kenwood_val);
        }
        else
        {
            sprintf(levelbuf, "SQ0%03d", kenwood_val);
        }

        break;

    case RIG_LEVEL_AGC:

        /* hamlib argument is int, possible values rig.h:enum agc_level_e */
        /* possible values for TS480 000(=off), 001(=fast), 002(=slow) */
        /* possible values for TS890 0(=off), 1(=slow), 2(=mid), 3(=fast), 4(=off/Last) */
        if (rig->caps->rig_model == RIG_MODEL_TS890S)
        {
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
        }
        else
        {
            rig_debug(RIG_DEBUG_VERBOSE, "%s TS480 RIG_LEVEL_AGC\n", __func__);

            switch (val.i)
            {
            case RIG_AGC_OFF:
                kenwood_val = 0;
                break;

            case RIG_AGC_FAST:
                kenwood_val = 1;
                break;

            case RIG_AGC_SLOW:
                kenwood_val = 2;
                break;

            default:
                rig_debug(RIG_DEBUG_ERR, "%s: unsupported agc value", __func__);
                return -RIG_EINVAL;
            }

            sprintf(levelbuf, "GT%03d", kenwood_val);
        }

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
 * Assumes rig!=NULL, val!=NULL
 */
int
kenwood_ts480_get_level(RIG *rig, vfo_t vfo, setting_t level, value_t *val)
{
    char ackbuf[50];
    size_t ack_len, ack_len_expected;
    int levelint;
    int offset_level = 3; // default offset for the level return value
    int retval;
    int rf_max_level;

    rig_debug(RIG_DEBUG_TRACE, "%s called\n", __func__);

    if (rig->caps->rig_model == RIG_MODEL_TS890S)
    {
        rf_max_level = 255;
    }
    else
    {
        rf_max_level = 100;
    }

    switch (level)
    {
    case RIG_LEVEL_RFPOWER:
        retval = kenwood_transaction(rig, "PC", ackbuf, sizeof(ackbuf));

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

        val->f = (float) levelint / 100.;
        return RIG_OK;

    case RIG_LEVEL_AF:
        if (rig->caps->rig_model == RIG_MODEL_TS890S)
        {
            retval = kenwood_transaction(rig, "AG", ackbuf, sizeof(ackbuf));
            offset_level = 2;
        }
        else
        {
            retval = kenwood_transaction(rig, "AG0", ackbuf, sizeof(ackbuf));
            offset_level = 3;
        }

        if (RIG_OK != retval)
        {
            return retval;
        }

        ack_len = strlen(ackbuf);

        if (offset_level + 3 != ack_len)
        {
            return -RIG_EPROTO;
        }

        if (1 != sscanf(&ackbuf[offset_level], "%d", &levelint))
        {
            return -RIG_EPROTO;
        }

        val->f = levelint / (float) rf_max_level;
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

        val->f = levelint / (float) rf_max_level;
        return RIG_OK;

    case RIG_LEVEL_SQL:
        if (rig->caps->rig_model == RIG_MODEL_TS890S)
        {
            retval = kenwood_transaction(rig, "SQ", ackbuf, sizeof(ackbuf));
            ack_len_expected = 5;
        }
        else
        {
            retval = kenwood_transaction(rig, "SQ0", ackbuf, sizeof(ackbuf));
            ack_len_expected = 6;
        }

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
        if (rig->caps->rig_model == RIG_MODEL_TS890S)
        {
            retval = kenwood_transaction(rig, "GC", ackbuf, sizeof(ackbuf));
            ack_len_expected = 3;
        }
        else
        {
            retval = kenwood_transaction(rig, "GT", ackbuf, sizeof(ackbuf));
            ack_len_expected = 5;
        }

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
            if (rig->caps->rig_model == RIG_MODEL_TS890S)
            {
                val->i = RIG_AGC_SLOW;
            }
            else
            {
                val->i = RIG_AGC_FAST;
            }

            break;

        case '2':
            if (rig->caps->rig_model == RIG_MODEL_TS890S)
            {
                val->i = RIG_AGC_MEDIUM;
            }
            else
            {
                val->i = RIG_AGC_SLOW;
            }

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

    case RIG_LEVEL_MICGAIN:
    case RIG_LEVEL_PREAMP:
    case RIG_LEVEL_IF:
    case RIG_LEVEL_APF:
    case RIG_LEVEL_NR:
    case RIG_LEVEL_PBT_IN:
    case RIG_LEVEL_PBT_OUT:
    case RIG_LEVEL_CWPITCH:
    case RIG_LEVEL_KEYSPD:
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

    return RIG_OK;        /* never reached */
}

static struct kenwood_priv_caps ts480_priv_caps =
{
    .cmdtrm = EOM_KEN,
};

static struct kenwood_priv_caps ts890s_priv_caps =
{
    .cmdtrm = EOM_KEN,
};


/*
 * ts480 rig capabilities.
 * Notice that some rigs share the same functions.
 * Also this struct is READONLY!
 */
const struct rig_caps ts480_caps =
{
    RIG_MODEL(RIG_MODEL_TS480),
    .model_name = "TS-480",
    .mfg_name = "Kenwood",
    .version = BACKEND_VER ".0",
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
        {kHz(100),   Hz(59999999), TS480_ALL_MODES, -1, -1, TS480_VFO},
        RIG_FRNG_END,
    }, /*!< Receive frequency range list for ITU region 1 */
    .tx_range_list1 = {
        {kHz(1810),  kHz(1850),  TS480_OTHER_TX_MODES, 5000, 100000, TS480_VFO},   /* 100W class */
        {kHz(1810),  kHz(1850),  TS480_AM_TX_MODES, 5000, 25000, TS480_VFO},       /* 25W class */
        {kHz(3500),  kHz(3800),  TS480_OTHER_TX_MODES, 5000, 100000, TS480_VFO},
        {kHz(3500),  kHz(3800),  TS480_AM_TX_MODES, 5000, 25000, TS480_VFO},
        {MHz(7),     kHz(7200),  TS480_OTHER_TX_MODES, 5000, 100000, TS480_VFO},
        {MHz(7),     kHz(7200),  TS480_AM_TX_MODES, 5000, 25000, TS480_VFO},
        {kHz(10100), kHz(10150), TS480_OTHER_TX_MODES, 5000, 100000, TS480_VFO},
        {kHz(10100), kHz(10150), TS480_AM_TX_MODES, 5000, 25000, TS480_VFO},
        {MHz(14),    kHz(14350), TS480_OTHER_TX_MODES, 5000, 100000, TS480_VFO},
        {MHz(14),    kHz(14350), TS480_AM_TX_MODES, 5000, 25000, TS480_VFO},
        {kHz(18068), kHz(18168), TS480_OTHER_TX_MODES, 5000, 100000, TS480_VFO},
        {kHz(18068), kHz(18168), TS480_AM_TX_MODES, 5000, 25000, TS480_VFO},
        {MHz(21),    kHz(21450), TS480_OTHER_TX_MODES, 5000, 100000, TS480_VFO},
        {MHz(21),    kHz(21450), TS480_AM_TX_MODES, 5000, 25000, TS480_VFO},
        {kHz(24890), kHz(24990), TS480_OTHER_TX_MODES, 5000, 100000, TS480_VFO},
        {kHz(24890), kHz(24990), TS480_AM_TX_MODES, 5000, 25000, TS480_VFO},
        {MHz(28),    kHz(29700), TS480_OTHER_TX_MODES, 5000, 100000, TS480_VFO},
        {MHz(28),    kHz(29700), TS480_AM_TX_MODES, 5000, 25000, TS480_VFO},
        {MHz(50),    kHz(52000), TS480_OTHER_TX_MODES, 5000, 100000, TS480_VFO},
        {MHz(50),    kHz(52000), TS480_AM_TX_MODES, 5000, 25000, TS480_VFO},
        RIG_FRNG_END,
    },  /*!< Transmit frequency range list for ITU region 1 */
    .rx_range_list2 = {
        {kHz(100),   Hz(59999999), TS480_ALL_MODES, -1, -1, TS480_VFO},
        RIG_FRNG_END,
    },  /*!< Receive frequency range list for ITU region 2 */
    .tx_range_list2 = {
        {kHz(1800),  MHz(2) - 1, TS480_OTHER_TX_MODES, 5000, 100000, TS480_VFO},  /* 100W class */
        {kHz(1800),  MHz(2) - 1, TS480_AM_TX_MODES, 5000, 25000, TS480_VFO},      /* 25W class */
        {kHz(3500),  MHz(4) - 1, TS480_OTHER_TX_MODES, 5000, 100000, TS480_VFO},
        {kHz(3500),  MHz(4) - 1, TS480_AM_TX_MODES, 5000, 25000, TS480_VFO},
        {kHz(5250),  kHz(5450),  TS480_OTHER_TX_MODES, 5000, 100000, TS480_VFO},
        {kHz(5250),  kHz(5450),  TS480_AM_TX_MODES, 5000, 25000, TS480_VFO},
        {MHz(7),     kHz(7300),  TS480_OTHER_TX_MODES, 5000, 100000, TS480_VFO},
        {MHz(7),     kHz(7300),  TS480_AM_TX_MODES, 5000, 25000, TS480_VFO},
        {kHz(10100), kHz(10150), TS480_OTHER_TX_MODES, 5000, 100000, TS480_VFO},
        {kHz(10100), kHz(10150), TS480_AM_TX_MODES, 5000, 25000, TS480_VFO},
        {MHz(14),    kHz(14350), TS480_OTHER_TX_MODES, 5000, 100000, TS480_VFO},
        {MHz(14),    kHz(14350), TS480_AM_TX_MODES, 5000, 25000, TS480_VFO},
        {kHz(18068), kHz(18168), TS480_OTHER_TX_MODES, 5000, 100000, TS480_VFO},
        {kHz(18068), kHz(18168), TS480_AM_TX_MODES, 5000, 25000, TS480_VFO},
        {MHz(21),    kHz(21450), TS480_OTHER_TX_MODES, 5000, 100000, TS480_VFO},
        {MHz(21),    kHz(21450), TS480_AM_TX_MODES, 5000, 25000, TS480_VFO},
        {kHz(24890), kHz(24990), TS480_OTHER_TX_MODES, 5000, 100000, TS480_VFO},
        {kHz(24890), kHz(24990), TS480_AM_TX_MODES, 5000, 25000, TS480_VFO},
        {MHz(28),    kHz(29700), TS480_OTHER_TX_MODES, 5000, 100000, TS480_VFO},
        {MHz(28),    kHz(29700), TS480_AM_TX_MODES, 5000, 25000, TS480_VFO},
        {MHz(50),    kHz(52000), TS480_OTHER_TX_MODES, 5000, 100000, TS480_VFO},
        {MHz(50),    kHz(52000), TS480_AM_TX_MODES, 5000, 25000, TS480_VFO},
        RIG_FRNG_END,
    }, /*!< Transmit frequency range list for ITU region 2 */
    .tuning_steps =  {
        {TS480_ALL_MODES, kHz(1)},
        {TS480_ALL_MODES, Hz(2500)},
        {TS480_ALL_MODES, kHz(5)},
        {TS480_ALL_MODES, Hz(6250)},
        {TS480_ALL_MODES, kHz(10)},
        {TS480_ALL_MODES, Hz(12500)},
        {TS480_ALL_MODES, kHz(15)},
        {TS480_ALL_MODES, kHz(20)},
        {TS480_ALL_MODES, kHz(25)},
        {TS480_ALL_MODES, kHz(30)},
        {TS480_ALL_MODES, kHz(100)},
        {TS480_ALL_MODES, kHz(500)},
        {TS480_ALL_MODES, MHz(1)},
        {TS480_ALL_MODES, 0},  /* any tuning step */
        RIG_TS_END,
    },
    /* mode/filter list, remember: order matters! */
    .filters =  {
        {RIG_MODE_SSB, kHz(2.4)},
        {RIG_MODE_CW, Hz(200)},
        {RIG_MODE_RTTY, Hz(500)},
        {RIG_MODE_AM, kHz(9)},
        {RIG_MODE_FM, kHz(14)},
        RIG_FLT_END,
    },
    .priv = (void *)& ts480_priv_caps,
    .rig_init = kenwood_init,
    .rig_open = kenwood_open,
    .rig_cleanup = kenwood_cleanup,
    .set_freq = kenwood_set_freq,
    .get_freq = kenwood_get_freq,
    .set_rit = kenwood_set_rit,   /*  FIXME should this switch to rit mode or just set the frequency? */
    .get_rit = kenwood_get_rit,
    .set_xit = kenwood_set_xit,   /* FIXME should this switch to xit mode or just set the frequency?  */
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
    .get_info = kenwood_ts480_get_info,
    .reset = kenwood_reset,
    .set_ant = kenwood_set_ant,
    .get_ant = kenwood_get_ant,
    .scan = kenwood_scan,     /* not working, invalid arguments using rigctl; kenwood_scan does only support on/off and not tone and CTCSS scan */
    .has_set_level = TS480_LEVEL_ALL,
    .has_get_level = TS480_LEVEL_ALL,
    .set_level = kenwood_ts480_set_level,
    .get_level = kenwood_ts480_get_level,
    .has_get_func = TS480_FUNC_ALL,
    .has_set_func = TS480_FUNC_ALL,
    .set_func = kenwood_set_func,
    .get_func = kenwood_get_func,
};

/*
 * Hilberling PS8000A TS480 emulation
 * Notice that some rigs share the same functions.
 * Also this struct is READONLY!
 */
const struct rig_caps pt8000a_caps =
{
    RIG_MODEL(RIG_MODEL_PT8000A),
    .model_name = "PT-8000A",
    .mfg_name = "Hilberling",
    .version = BACKEND_VER ".0",
    .copyright = "LGPL",
    .status = RIG_STATUS_BETA,
    .rig_type = RIG_TYPE_TRANSCEIVER,
    .ptt_type = RIG_PTT_RIG_MICDATA,
    .dcd_type = RIG_DCD_RIG,
    .port_type = RIG_PORT_SERIAL,
    .serial_rate_min = 9600,
    .serial_rate_max = 57600,
    .serial_data_bits = 8,
    .serial_stop_bits = 1,
    .serial_parity = RIG_PARITY_NONE,
    .serial_handshake = RIG_HANDSHAKE_NONE,
    .write_delay = 0,
    .post_write_delay = 20,
    .timeout = 200,
    .retry = 10,
    .preamp = {12, RIG_DBLST_END,},
    .attenuator = {12, RIG_DBLST_END,},
    .max_rit = kHz(9.99),
    .max_xit = kHz(9.99),
    .max_ifshift = Hz(0),
    .targetable_vfo = RIG_TARGETABLE_FREQ,
    .transceive = RIG_TRN_RIG,


    .rx_range_list1 = { // not region specific
        {kHz(9),   MHz(30), PS8000A_ALL_MODES, -1, -1, TS480_VFO, RIG_ANT_2 | RIG_ANT_3, "Generic"},
        {MHz(50),   MHz(54), PS8000A_ALL_MODES, -1, -1, TS480_VFO, RIG_ANT_1, "Generic"},
        {MHz(69.9),   MHz(70.5), PS8000A_ALL_MODES, -1, -1, TS480_VFO, RIG_ANT_1, "Generic"},
        {MHz(110),   MHz(143.99), PS8000A_ALL_MODES, -1, -1, TS480_VFO, RIG_ANT_1, "Generic"},
        {MHz(144),   MHz(148), PS8000A_ALL_MODES, -1, -1, TS480_VFO, RIG_ANT_1, "Generic"},
        RIG_FRNG_END,
    }, /*!< Receive frequency range list for ITU region 1 */
    .tx_range_list1 = {
        {MHz(1.8),  MHz(30),  TS480_OTHER_TX_MODES, 1000, 200000, TS480_VFO, RIG_ANT_2 | RIG_ANT_3, "Generic"}, /* 200W class */
        {MHz(1.8),  MHz(30),  TS480_AM_TX_MODES | RIG_MODE_AMS, 5000, 50000, TS480_VFO, RIG_ANT_2 | RIG_ANT_3, "Generic"},   /* 50W class */
        {MHz(50),  MHz(54),  TS480_OTHER_TX_MODES, 1000, 100000, TS480_VFO, RIG_ANT_1, "Generic"},   /* 100W class */
        {MHz(50),  MHz(54),  TS480_AM_TX_MODES | RIG_MODE_AMS, 5000, 25000, TS480_VFO, RIG_ANT_1, "Generic"},     /* 25W class */
        {MHz(69.9),  MHz(70.5),  TS480_OTHER_TX_MODES, 1000, 100000, TS480_VFO, RIG_ANT_1, "Generic"},   /* 100W class */
        {MHz(69.9),  MHz(70.5),  TS480_AM_TX_MODES | RIG_MODE_AMS, 5000, 25000, TS480_VFO, RIG_ANT_1, "Generic"},     /* 25W class */
        {MHz(144),  MHz(148),  TS480_OTHER_TX_MODES, 1000, 100000, TS480_VFO, RIG_ANT_1, "Generic"},   /* 100W class */
        {MHz(144),  MHz(148),  TS480_AM_TX_MODES | RIG_MODE_AMS, 5000, 25000, TS480_VFO, RIG_ANT_1, "Generic"},     /* 25W class */
        RIG_FRNG_END,
    },
    .tuning_steps =  {
        {PS8000A_ALL_MODES, Hz(1)},
        {PS8000A_ALL_MODES, Hz(10)},
        {PS8000A_ALL_MODES, Hz(100)},
        {PS8000A_ALL_MODES, Hz(1000)},
        RIG_TS_END,
    },
    /* mode/filter list, remember: order matters! */
    .filters =  {
        {RIG_MODE_SSB, kHz(1.0)},
        {RIG_MODE_SSB, kHz(1.2)},
        {RIG_MODE_SSB, kHz(1.4)},
        {RIG_MODE_SSB, kHz(1.6)},
        {RIG_MODE_SSB, kHz(1.8)},
        {RIG_MODE_SSB, kHz(1.9)},
        {RIG_MODE_SSB, kHz(2.0)},
        {RIG_MODE_SSB, kHz(2.1)},
        {RIG_MODE_SSB, kHz(2.2)},
        {RIG_MODE_SSB, kHz(2.3)},
        {RIG_MODE_SSB, kHz(2.4)},
        {RIG_MODE_SSB, kHz(2.5)},
        {RIG_MODE_SSB, kHz(2.6)},
        {RIG_MODE_SSB, kHz(2.7)},
        {RIG_MODE_SSB, kHz(2.8)},
        {RIG_MODE_SSB, kHz(2.9)},
        {RIG_MODE_SSB, kHz(3.0)},
        {RIG_MODE_SSB, kHz(3.1)},
        {RIG_MODE_SSB, kHz(3.2)},
        {RIG_MODE_SSB, kHz(3.3)},
        {RIG_MODE_SSB, kHz(3.4)},
        {RIG_MODE_SSB, kHz(3.5)},
        {RIG_MODE_SSB, kHz(4.6)},
        {RIG_MODE_SSB, kHz(6.0)},
        {RIG_MODE_CW, Hz(50)},
        {RIG_MODE_CW, Hz(100)},
        {RIG_MODE_CW, Hz(200)},
        {RIG_MODE_CW, Hz(400)},
        {RIG_MODE_CW, Hz(500)},
        {RIG_MODE_AM, kHz(2.4)},
        {RIG_MODE_AM, kHz(2.5)},
        {RIG_MODE_AM, kHz(2.6)},
        {RIG_MODE_AM, kHz(2.7)},
        {RIG_MODE_AM, kHz(2.8)},
        {RIG_MODE_AM, kHz(2.9)},
        {RIG_MODE_AM, kHz(3.0)},
        {RIG_MODE_AM, kHz(3.1)},
        {RIG_MODE_AM, kHz(3.2)},
        {RIG_MODE_AM, kHz(3.3)},
        {RIG_MODE_AM, kHz(3.4)},
        {RIG_MODE_AM, kHz(3.5)},
        {RIG_MODE_AM, kHz(3.5)},
        {RIG_MODE_AM, kHz(4.6)},
        {RIG_MODE_AM, kHz(6.0)},
        {RIG_MODE_FM, kHz(2.4)},
        {RIG_MODE_FM, kHz(2.5)},
        {RIG_MODE_FM, kHz(2.6)},
        {RIG_MODE_FM, kHz(2.7)},
        {RIG_MODE_FM, kHz(2.8)},
        {RIG_MODE_FM, kHz(2.9)},
        {RIG_MODE_FM, kHz(3.0)},
        {RIG_MODE_FM, kHz(3.1)},
        {RIG_MODE_FM, kHz(3.2)},
        {RIG_MODE_FM, kHz(3.3)},
        {RIG_MODE_FM, kHz(3.4)},
        {RIG_MODE_FM, kHz(3.5)},
        {RIG_MODE_FM, kHz(3.5)},
        {RIG_MODE_FM, kHz(4.6)},
        {RIG_MODE_FM, kHz(6.0)},
        RIG_FLT_END,
    },
    .priv = (void *)& ts480_priv_caps,
    .rig_init = kenwood_init,
    .rig_open = kenwood_open,
    .rig_cleanup = kenwood_cleanup,
    .set_freq = kenwood_set_freq,
    .get_freq = kenwood_get_freq,
    .set_rit = kenwood_set_rit,   /*  FIXME should this switch to rit mode or just set the frequency? */
    .get_rit = kenwood_get_rit,
    .set_xit = kenwood_set_xit,   /* FIXME should this switch to xit mode or just set the frequency?  */
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
    .get_info = kenwood_ts480_get_info,
    .reset = kenwood_reset,
    .set_ant = kenwood_set_ant,
    .get_ant = kenwood_get_ant,
    .scan = kenwood_scan,     /* not working, invalid arguments using rigctl; kenwood_scan does only support on/off and not tone and CTCSS scan */
    .has_set_level = TS480_LEVEL_ALL,
    .has_get_level = TS480_LEVEL_ALL,
    .set_level = kenwood_ts480_set_level,
    .get_level = kenwood_ts480_get_level,
    .has_get_func = TS480_FUNC_ALL,
    .has_set_func = TS480_FUNC_ALL,
    .set_func = kenwood_set_func,
    .get_func = kenwood_get_func,
};

/*
 * ts890s rig capabilities.
 * Copied from ts480_caps
 * Where you see TS480 in this the values have not been verified
 * Notice that some rigs share the same functions.
 * Also this struct is READONLY!
 */
const struct rig_caps ts890s_caps =
{
    RIG_MODEL(RIG_MODEL_TS890S),
    .model_name = "TS-890S",
    .mfg_name = "Kenwood",
    .version = BACKEND_VER ".0",
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
        {kHz(100),   Hz(59999999), TS890_ALL_MODES, -1, -1, TS480_VFO},
        RIG_FRNG_END,
    }, /*!< Receive frequency range list for ITU region 1 */
    .tx_range_list1 = {
        {kHz(1810),  kHz(1850),  TS480_OTHER_TX_MODES, 5000, 100000, TS480_VFO},   /* 100W class */
        {kHz(1810),  kHz(1850),  TS480_AM_TX_MODES, 5000, 25000, TS480_VFO},       /* 25W class */
        {kHz(3500),  kHz(3800),  TS480_OTHER_TX_MODES, 5000, 100000, TS480_VFO},
        {kHz(3500),  kHz(3800),  TS480_AM_TX_MODES, 5000, 25000, TS480_VFO},
        {MHz(7),     kHz(7200),  TS480_OTHER_TX_MODES, 5000, 100000, TS480_VFO},
        {MHz(7),     kHz(7200),  TS480_AM_TX_MODES, 5000, 25000, TS480_VFO},
        {kHz(10100), kHz(10150), TS480_OTHER_TX_MODES, 5000, 100000, TS480_VFO},
        {kHz(10100), kHz(10150), TS480_AM_TX_MODES, 5000, 25000, TS480_VFO},
        {MHz(14),    kHz(14350), TS480_OTHER_TX_MODES, 5000, 100000, TS480_VFO},
        {MHz(14),    kHz(14350), TS480_AM_TX_MODES, 5000, 25000, TS480_VFO},
        {kHz(18068), kHz(18168), TS480_OTHER_TX_MODES, 5000, 100000, TS480_VFO},
        {kHz(18068), kHz(18168), TS480_AM_TX_MODES, 5000, 25000, TS480_VFO},
        {MHz(21),    kHz(21450), TS480_OTHER_TX_MODES, 5000, 100000, TS480_VFO},
        {MHz(21),    kHz(21450), TS480_AM_TX_MODES, 5000, 25000, TS480_VFO},
        {kHz(24890), kHz(24990), TS480_OTHER_TX_MODES, 5000, 100000, TS480_VFO},
        {kHz(24890), kHz(24990), TS480_AM_TX_MODES, 5000, 25000, TS480_VFO},
        {MHz(28),    kHz(29700), TS480_OTHER_TX_MODES, 5000, 100000, TS480_VFO},
        {MHz(28),    kHz(29700), TS480_AM_TX_MODES, 5000, 25000, TS480_VFO},
        {MHz(50),    kHz(52000), TS480_OTHER_TX_MODES, 5000, 100000, TS480_VFO},
        {MHz(50),    kHz(52000), TS480_AM_TX_MODES, 5000, 25000, TS480_VFO},
        RIG_FRNG_END,
    },  /*!< Transmit frequency range list for ITU region 1 */
    .rx_range_list2 = {
        {kHz(100),   Hz(59999999), TS890_ALL_MODES, -1, -1, TS480_VFO},
        RIG_FRNG_END,
    },  /*!< Receive frequency range list for ITU region 2 */
    .tx_range_list2 = {
        {kHz(1800),  MHz(2) - 1, TS480_OTHER_TX_MODES, 5000, 100000, TS480_VFO},  /* 100W class */
        {kHz(1800),  MHz(2) - 1, TS480_AM_TX_MODES, 5000, 25000, TS480_VFO},      /* 25W class */
        {kHz(3500),  MHz(4) - 1, TS480_OTHER_TX_MODES, 5000, 100000, TS480_VFO},
        {kHz(3500),  MHz(4) - 1, TS480_AM_TX_MODES, 5000, 25000, TS480_VFO},
        {kHz(5250),  kHz(5450),  TS480_OTHER_TX_MODES, 5000, 100000, TS480_VFO},
        {kHz(5250),  kHz(5450),  TS480_AM_TX_MODES, 5000, 25000, TS480_VFO},
        {MHz(7),     kHz(7300),  TS480_OTHER_TX_MODES, 5000, 100000, TS480_VFO},
        {MHz(7),     kHz(7300),  TS480_AM_TX_MODES, 5000, 25000, TS480_VFO},
        {kHz(10100), kHz(10150), TS480_OTHER_TX_MODES, 5000, 100000, TS480_VFO},
        {kHz(10100), kHz(10150), TS480_AM_TX_MODES, 5000, 25000, TS480_VFO},
        {MHz(14),    kHz(14350), TS480_OTHER_TX_MODES, 5000, 100000, TS480_VFO},
        {MHz(14),    kHz(14350), TS480_AM_TX_MODES, 5000, 25000, TS480_VFO},
        {kHz(18068), kHz(18168), TS480_OTHER_TX_MODES, 5000, 100000, TS480_VFO},
        {kHz(18068), kHz(18168), TS480_AM_TX_MODES, 5000, 25000, TS480_VFO},
        {MHz(21),    kHz(21450), TS480_OTHER_TX_MODES, 5000, 100000, TS480_VFO},
        {MHz(21),    kHz(21450), TS480_AM_TX_MODES, 5000, 25000, TS480_VFO},
        {kHz(24890), kHz(24990), TS480_OTHER_TX_MODES, 5000, 100000, TS480_VFO},
        {kHz(24890), kHz(24990), TS480_AM_TX_MODES, 5000, 25000, TS480_VFO},
        {MHz(28),    kHz(29700), TS480_OTHER_TX_MODES, 5000, 100000, TS480_VFO},
        {MHz(28),    kHz(29700), TS480_AM_TX_MODES, 5000, 25000, TS480_VFO},
        {MHz(50),    kHz(52000), TS480_OTHER_TX_MODES, 5000, 100000, TS480_VFO},
        {MHz(50),    kHz(52000), TS480_AM_TX_MODES, 5000, 25000, TS480_VFO},
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
    .priv = (void *)& ts890s_priv_caps,
    .rig_init = kenwood_init,
    .rig_open = kenwood_open,
    .rig_cleanup = kenwood_cleanup,
    .set_freq = kenwood_set_freq,
    .get_freq = kenwood_get_freq,
    .set_rit = kenwood_set_rit,   /*  FIXME should this switch to rit mode or just set the frequency? */
    .get_rit = kenwood_get_rit,
    .set_xit = kenwood_set_xit,   /* FIXME should this switch to xit mode or just set the frequency?  */
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
    .get_info = kenwood_ts480_get_info,
    .reset = kenwood_reset,
    .set_ant = kenwood_set_ant,
    .get_ant = kenwood_get_ant,
    .scan = kenwood_scan,     /* not working, invalid arguments using rigctl; kenwood_scan does only support on/off and not tone and CTCSS scan */
    .has_set_level = TS480_LEVEL_ALL,
    .has_get_level = TS480_LEVEL_ALL,
    .set_level = kenwood_ts480_set_level,
    .get_level = kenwood_ts480_get_level,
    .has_get_func = TS890_FUNC_ALL,
    .has_set_func = TS890_FUNC_ALL,
    .set_func = kenwood_set_func,
    .get_func = kenwood_get_func,
};

