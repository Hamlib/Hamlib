/*
 *  Hamlib ELAD backend - FDM_DUO description
 *  Copyright (c) 2000-2004 by Stephane Fillod and Juergen Rinas
 *  Copyright (c) 2018 by Giovanni Franza HB9EIK
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

#include <stdio.h>

#include <hamlib/rig.h>
#include "idx_builtin.h"
#include "elad.h"

#define FDM_DUO_ALL_MODES (RIG_MODE_AM|RIG_MODE_CW|RIG_MODE_CWR|RIG_MODE_SSB|RIG_MODE_FM|RIG_MODE_RTTY|RIG_MODE_RTTYR)
#define FDM_DUO_OTHER_TX_MODES (RIG_MODE_CW|RIG_MODE_SSB|RIG_MODE_FM|RIG_MODE_RTTY)
#define FDM_DUO_AM_TX_MODES RIG_MODE_AM
#define FDM_DUO_VFO (RIG_VFO_A|RIG_VFO_B)

#define FDM_DUO_LEVEL_ALL (RIG_LEVEL_RFPOWER|RIG_LEVEL_AF|RIG_LEVEL_RF|RIG_LEVEL_SQL|RIG_LEVEL_AGC)
#define FDM_DUO_FUNC_ALL (RIG_FUNC_NB|RIG_FUNC_COMP|RIG_FUNC_VOX|RIG_FUNC_NR|RIG_FUNC_NR|RIG_FUNC_BC)


/*
 * elad_fdm_duo_get_info
 * Assumes rig!=NULL
 */
static const char *
elad_fdm_duo_get_info(RIG *rig)
{
    char firmbuf[50];
    int retval;
    size_t firm_len;

    retval = elad_transaction(rig, "TY", firmbuf, sizeof(firmbuf));

    if (retval != RIG_OK)
    {
        return NULL;
    }

    firm_len = strlen(firmbuf);

    if (firm_len != 5)
    {
        rig_debug(RIG_DEBUG_ERR, "elad_get_info: wrong answer len=%d\n", (int)firm_len);
        return NULL;
    }

    switch (firmbuf[4])
    {
    case '0':
        return "FDM-DUOHX (200W)";

    case '1':
        return "FDM-DUOSAT (100W + AT)";

    case '2':
        return "Japanese 50W type";

    case '3':
        return "Japanese 20W type";

    default:
        return "Firmware: unknown";
    }
}


/*
 * elad_fdm_duo_set_level
 * Assumes rig!=NULL
 *
 * set levels of most functions
 *
 * WARNING: the commands differ slightly from the general versions in elad.c
 * e.g.: "SQ"=>"SQ0" , "AG"=>"AG0"
 */
int
elad_fdm_duo_set_level(RIG *rig, vfo_t vfo, setting_t level, value_t val)
{
    char levelbuf[16];
    int elad_val;

    switch (level)
    {
    case RIG_LEVEL_RFPOWER:
        elad_val = val.f * 100;   /* level for FDM_DUOSAT is from 0.. 100W in SSB */
        SNPRINTF(levelbuf, sizeof(levelbuf), "PC%03d", elad_val);
        break;

    case RIG_LEVEL_AF:
        elad_val = val.f * 255;   /* possible values for FDM_DUO are 000.. 255 */
        SNPRINTF(levelbuf, sizeof(levelbuf), "AG0%03d", elad_val);
        break;

    case RIG_LEVEL_RF:
        elad_val = val.f * 100;   /* possible values for FDM_DUO are 000.. 100 */
        SNPRINTF(levelbuf, sizeof(levelbuf), "RG%03d", elad_val);
        break;

    case RIG_LEVEL_SQL:
        elad_val = val.f * 255;   /* possible values for FDM_DUO are 000.. 255 */
        SNPRINTF(levelbuf, sizeof(levelbuf), "SQ0%03d", elad_val);
        break;

    case RIG_LEVEL_AGC: /* possible values for FDM_DUO 000(=off), 001(=fast), 002(=slow) */

        /* hamlib argument is int, possible values rig.h:enum agc_level_e */
        switch (val.i)
        {
        case RIG_AGC_OFF:
            elad_val = 0;
            break;

        case RIG_AGC_FAST:
            elad_val = 1;
            break;

        case RIG_AGC_SLOW:
            elad_val = 2;
            break;

        default:
            rig_debug(RIG_DEBUG_ERR, "%s: Unsupported agc value", __func__);
            return -RIG_EINVAL;
        };

        SNPRINTF(levelbuf, sizeof(levelbuf), "GT%03d", elad_val);
        break;

    default:
        rig_debug(RIG_DEBUG_ERR, "%s: Unsupported set_level %s", __func__,
                  rig_strlevel(level));
        return -RIG_EINVAL;
    }

    return elad_transaction(rig, levelbuf, NULL, 0);
}


/*
 * elad_get_level
 * Assumes rig!=NULL, val!=NULL
 */
int
elad_fdm_duo_get_level(RIG *rig, vfo_t vfo, setting_t level, value_t *val)
{
    char ackbuf[50];
    size_t ack_len;
    int levelint;
    int retval;

    switch (level)
    {
    case RIG_LEVEL_RFPOWER:
        retval = elad_transaction(rig, "PC", ackbuf, sizeof(ackbuf));

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
        retval = elad_transaction(rig, "AG0", ackbuf, sizeof(ackbuf));

        if (RIG_OK != retval)
        {
            return retval;
        }

        ack_len = strlen(ackbuf);

        if (6 != ack_len)
        {
            return -RIG_EPROTO;
        }

        if (1 != sscanf(&ackbuf[3], "%d", &levelint))
        {
            return -RIG_EPROTO;
        }

        val->f = (float) levelint / 255.;
        return RIG_OK;

    case RIG_LEVEL_RF:
        retval = elad_transaction(rig, "RG", ackbuf, sizeof(ackbuf));

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

    case RIG_LEVEL_SQL:
        retval = elad_transaction(rig, "SQ0", ackbuf, sizeof(ackbuf));

        if (RIG_OK != retval)
        {
            return retval;
        }

        ack_len = strlen(ackbuf);

        if (6 != ack_len)
        {
            return -RIG_EPROTO;
        }

        if (1 != sscanf(&ackbuf[3], "%d", &levelint))
        {
            return -RIG_EPROTO;
        }

        val->f = (float) levelint / 255.;
        return RIG_OK;

    case RIG_LEVEL_AGC:
        retval = elad_transaction(rig, "GT", ackbuf, sizeof(ackbuf));

        if (RIG_OK != retval)
        {
            return retval;
        }

        ack_len = strlen(ackbuf);

        if (5 != ack_len)
        {
            return -RIG_EPROTO;
        }

        switch (ackbuf[4])
        {
        case '0':
            val->i = RIG_AGC_OFF;
            break;

        case '1':
            val->i = RIG_AGC_FAST;
            break;

        case '2':
            val->i = RIG_AGC_SLOW;
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
        rig_debug(RIG_DEBUG_ERR, "Unsupported get_level %s", rig_strlevel(level));
        return -RIG_EINVAL;
    }

    return RIG_OK;        /* never reached */
}

static struct elad_priv_caps fdm_duo_priv_caps =
{
    .cmdtrm = EOM_KEN,
};


/*
 * fdm_duo rig capabilities.
 * Notice that some rigs share the same functions.
 * Also this struct is READONLY!
 */
const struct rig_caps fdm_duo_caps =
{
    RIG_MODEL(RIG_MODEL_ELAD_FDM_DUO),
    .model_name = "FDM-DUO",
    .mfg_name = "ELAD",
    .version = BACKEND_VER ".0",
    .copyright = "LGPL",
    .status = RIG_STATUS_STABLE,
    .rig_type = RIG_TYPE_TRANSCEIVER,
    .ptt_type = RIG_PTT_RIG_MICDATA,
    .dcd_type = RIG_DCD_RIG,
    .port_type = RIG_PORT_SERIAL,
    .serial_rate_min = 9600,
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
        {kHz(100),   Hz(59999999), FDM_DUO_ALL_MODES, -1, -1, FDM_DUO_VFO},
        RIG_FRNG_END,
    }, /*!< Receive frequency range list for ITU region 1 */
    .tx_range_list1 = {
        {kHz(1810),  kHz(1850),  FDM_DUO_OTHER_TX_MODES, 5000, 100000, FDM_DUO_VFO},   /* 100W class */
        {kHz(1810),  kHz(1850),  FDM_DUO_AM_TX_MODES, 5000, 25000, FDM_DUO_VFO},       /* 25W class */
        {kHz(3500),  kHz(3800),  FDM_DUO_OTHER_TX_MODES, 5000, 100000, FDM_DUO_VFO},
        {kHz(3500),  kHz(3800),  FDM_DUO_AM_TX_MODES, 5000, 25000, FDM_DUO_VFO},
        {MHz(7),     kHz(7200),  FDM_DUO_OTHER_TX_MODES, 5000, 100000, FDM_DUO_VFO},
        {MHz(7),     kHz(7200),  FDM_DUO_AM_TX_MODES, 5000, 25000, FDM_DUO_VFO},
        {kHz(10100), kHz(10150), FDM_DUO_OTHER_TX_MODES, 5000, 100000, FDM_DUO_VFO},
        {kHz(10100), kHz(10150), FDM_DUO_AM_TX_MODES, 5000, 25000, FDM_DUO_VFO},
        {MHz(14),    kHz(14350), FDM_DUO_OTHER_TX_MODES, 5000, 100000, FDM_DUO_VFO},
        {MHz(14),    kHz(14350), FDM_DUO_AM_TX_MODES, 5000, 25000, FDM_DUO_VFO},
        {kHz(18068), kHz(18168), FDM_DUO_OTHER_TX_MODES, 5000, 100000, FDM_DUO_VFO},
        {kHz(18068), kHz(18168), FDM_DUO_AM_TX_MODES, 5000, 25000, FDM_DUO_VFO},
        {MHz(21),    kHz(21450), FDM_DUO_OTHER_TX_MODES, 5000, 100000, FDM_DUO_VFO},
        {MHz(21),    kHz(21450), FDM_DUO_AM_TX_MODES, 5000, 25000, FDM_DUO_VFO},
        {kHz(24890), kHz(24990), FDM_DUO_OTHER_TX_MODES, 5000, 100000, FDM_DUO_VFO},
        {kHz(24890), kHz(24990), FDM_DUO_AM_TX_MODES, 5000, 25000, FDM_DUO_VFO},
        {MHz(28),    kHz(29700), FDM_DUO_OTHER_TX_MODES, 5000, 100000, FDM_DUO_VFO},
        {MHz(28),    kHz(29700), FDM_DUO_AM_TX_MODES, 5000, 25000, FDM_DUO_VFO},
        {MHz(50),    kHz(52000), FDM_DUO_OTHER_TX_MODES, 5000, 100000, FDM_DUO_VFO},
        {MHz(50),    kHz(52000), FDM_DUO_AM_TX_MODES, 5000, 25000, FDM_DUO_VFO},
        RIG_FRNG_END,
    },  /*!< Transmit frequency range list for ITU region 1 */
    .rx_range_list2 = {
        {kHz(100),   Hz(59999999), FDM_DUO_ALL_MODES, -1, -1, FDM_DUO_VFO},
        RIG_FRNG_END,
    },  /*!< Receive frequency range list for ITU region 2 */
    .tx_range_list2 = {
        {kHz(1800),  MHz(2) - 1, FDM_DUO_OTHER_TX_MODES, 5000, 100000, FDM_DUO_VFO},  /* 100W class */
        {kHz(1800),  MHz(2) - 1, FDM_DUO_AM_TX_MODES, 5000, 25000, FDM_DUO_VFO},      /* 25W class */
        {kHz(3500),  MHz(4) - 1, FDM_DUO_OTHER_TX_MODES, 5000, 100000, FDM_DUO_VFO},
        {kHz(3500),  MHz(4) - 1, FDM_DUO_AM_TX_MODES, 5000, 25000, FDM_DUO_VFO},
        {kHz(5250),  kHz(5450),  FDM_DUO_OTHER_TX_MODES, 5000, 100000, FDM_DUO_VFO},
        {kHz(5250),  kHz(5450),  FDM_DUO_AM_TX_MODES, 5000, 25000, FDM_DUO_VFO},
        {MHz(7),     kHz(7300),  FDM_DUO_OTHER_TX_MODES, 5000, 100000, FDM_DUO_VFO},
        {MHz(7),     kHz(7300),  FDM_DUO_AM_TX_MODES, 5000, 25000, FDM_DUO_VFO},
        {kHz(10100), kHz(10150), FDM_DUO_OTHER_TX_MODES, 5000, 100000, FDM_DUO_VFO},
        {kHz(10100), kHz(10150), FDM_DUO_AM_TX_MODES, 5000, 25000, FDM_DUO_VFO},
        {MHz(14),    kHz(14350), FDM_DUO_OTHER_TX_MODES, 5000, 100000, FDM_DUO_VFO},
        {MHz(14),    kHz(14350), FDM_DUO_AM_TX_MODES, 5000, 25000, FDM_DUO_VFO},
        {kHz(18068), kHz(18168), FDM_DUO_OTHER_TX_MODES, 5000, 100000, FDM_DUO_VFO},
        {kHz(18068), kHz(18168), FDM_DUO_AM_TX_MODES, 5000, 25000, FDM_DUO_VFO},
        {MHz(21),    kHz(21450), FDM_DUO_OTHER_TX_MODES, 5000, 100000, FDM_DUO_VFO},
        {MHz(21),    kHz(21450), FDM_DUO_AM_TX_MODES, 5000, 25000, FDM_DUO_VFO},
        {kHz(24890), kHz(24990), FDM_DUO_OTHER_TX_MODES, 5000, 100000, FDM_DUO_VFO},
        {kHz(24890), kHz(24990), FDM_DUO_AM_TX_MODES, 5000, 25000, FDM_DUO_VFO},
        {MHz(28),    kHz(29700), FDM_DUO_OTHER_TX_MODES, 5000, 100000, FDM_DUO_VFO},
        {MHz(28),    kHz(29700), FDM_DUO_AM_TX_MODES, 5000, 25000, FDM_DUO_VFO},
        {MHz(50),    kHz(52000), FDM_DUO_OTHER_TX_MODES, 5000, 100000, FDM_DUO_VFO},
        {MHz(50),    kHz(52000), FDM_DUO_AM_TX_MODES, 5000, 25000, FDM_DUO_VFO},
        RIG_FRNG_END,
    }, /*!< Transmit frequency range list for ITU region 2 */
    .tuning_steps =  {
        {FDM_DUO_ALL_MODES, kHz(1)},
        {FDM_DUO_ALL_MODES, Hz(2500)},
        {FDM_DUO_ALL_MODES, kHz(5)},
        {FDM_DUO_ALL_MODES, Hz(6250)},
        {FDM_DUO_ALL_MODES, kHz(10)},
        {FDM_DUO_ALL_MODES, Hz(12500)},
        {FDM_DUO_ALL_MODES, kHz(15)},
        {FDM_DUO_ALL_MODES, kHz(20)},
        {FDM_DUO_ALL_MODES, kHz(25)},
        {FDM_DUO_ALL_MODES, kHz(30)},
        {FDM_DUO_ALL_MODES, kHz(100)},
        {FDM_DUO_ALL_MODES, kHz(500)},
        {FDM_DUO_ALL_MODES, MHz(1)},
        {FDM_DUO_ALL_MODES, 0},  /* any tuning step */
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
    .priv = (void *)& fdm_duo_priv_caps,
    .rig_init = elad_init,
    .rig_cleanup = elad_cleanup,
    .set_freq = elad_set_freq,
    .get_freq = elad_get_freq,
    .set_rit = elad_set_rit,  /*  FIXME should this switch to rit mode or just set the frequency? */
    .get_rit = elad_get_rit,
    .set_xit = elad_set_xit,  /* FIXME should this switch to xit mode or just set the frequency?  */
    .get_xit = elad_get_xit,
    .set_mode = elad_set_mode,
    .get_mode = elad_get_mode,
    .set_vfo = elad_set_vfo,
    .get_vfo = elad_get_vfo_if,
    .set_split_vfo = elad_set_split_vfo,
    .get_split_vfo = elad_get_split_vfo_if,
    .get_ptt = elad_get_ptt,
    .set_ptt = elad_set_ptt,
    .get_dcd = elad_get_dcd,
    .set_powerstat = elad_set_powerstat,
    .get_powerstat = elad_get_powerstat,
    .get_info = elad_fdm_duo_get_info,
    .reset = elad_reset,
    .set_ant = elad_set_ant,
    .get_ant = elad_get_ant,
    .scan = elad_scan,        /* not working, invalid arguments using rigctl; elad_scan does only support on/off and not tone and CTCSS scan */
    .has_set_level = FDM_DUO_LEVEL_ALL,
    .has_get_level = FDM_DUO_LEVEL_ALL,
    .set_level = elad_fdm_duo_set_level,
    .get_level = elad_fdm_duo_get_level,
    .has_get_func = FDM_DUO_FUNC_ALL,
    .has_set_func = FDM_DUO_FUNC_ALL,
    .set_func = elad_set_func,
    .get_func = elad_get_func,
    .hamlib_check_rig_caps = HAMLIB_CHECK_RIG_CAPS
};


/*
 * my notes:
 *  format with:  indent --line-length 200 fdm_duo.c
 *
 * for the FDM_DUO the function NR and BC have tree state: NR0,1,2 and BC0,1,2
 * this cannot be send through the on/off logic of set_function!
 */


/*
 * Function definitions below
 */
