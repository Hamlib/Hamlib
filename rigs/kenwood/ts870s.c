/*
 *  Hamlib Kenwood backend - TS870S description
 *  Copyright (c) 2000-2008 by Stephane Fillod
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
#include <stdio.h>

#include <hamlib/rig.h>
#include "kenwood.h"
#include "bandplan.h"

#define TS870S_ALL_MODES    \
(RIG_MODE_AM|RIG_MODE_CW|RIG_MODE_CWR|RIG_MODE_SSB      \
|RIG_MODE_FM|RIG_MODE_RTTY|RIG_MODE_RTTYR)
#define TS870S_OTHER_TX_MODES   \
(RIG_MODE_CW|RIG_MODE_SSB|RIG_MODE_FM|RIG_MODE_RTTY)
#define TS870S_AM_TX_MODES RIG_MODE_AM

#define TS870S_FUNC_ALL \
(RIG_FUNC_NB|RIG_FUNC_COMP|RIG_FUNC_VOX|RIG_FUNC_NR     \
|RIG_FUNC_BC|RIG_FUNC_ANF|RIG_FUNC_LOCK)

#define TS870S_LEVEL_SET    \
(RIG_LEVEL_ATT|RIG_LEVEL_SQL|RIG_LEVEL_AGC|RIG_LEVEL_RFPOWER        \
|RIG_LEVEL_AF|RIG_LEVEL_RF|RIG_LEVEL_MICGAIN|RIG_LEVEL_PREAMP)

#define TS870S_LEVEL_GET    \
(RIG_LEVEL_ATT|RIG_LEVEL_SQL|RIG_LEVEL_STRENGTH|RIG_LEVEL_SWR       \
|RIG_LEVEL_COMP|RIG_LEVEL_ALC|RIG_LEVEL_AGC|RIG_LEVEL_RFPOWER       \
|RIG_LEVEL_AF|RIG_LEVEL_RF|RIG_LEVEL_MICGAIN|RIG_LEVEL_PREAMP)

#define TS870S_VFO (RIG_VFO_A|RIG_VFO_B)
#define TS870S_ANTS (RIG_ANT_1|RIG_ANT_2)

static struct kenwood_priv_caps  ts870s_priv_caps  =
{
    .cmdtrm =  EOM_KEN,
};

/* only the ts870s and ts2000 support get_vfo with the 'FR;' command
   NOTE: using byte 31 in 'IF' will also work. TODO: check other rigs */
static int ts870s_get_vfo(RIG *rig, vfo_t *vfo)
{
    char vfobuf[50];
    size_t vfo_len;
    int retval;


    /* query RX VFO */
    retval = kenwood_transaction(rig, "FR", vfobuf, sizeof(vfobuf));

    if (retval != RIG_OK)
    {
        return retval;
    }

    vfo_len = strlen(vfobuf);

    if (vfo_len != 3 || vfobuf[1] != 'R')
    {
        rig_debug(RIG_DEBUG_ERR, "%s: unexpected answer %s, len=%d\n", __func__, vfobuf,
                  (int)vfo_len);
        return -RIG_ERJCTED;
    }

    /* TODO: replace 0,1,2,.. constants by defines */
    switch (vfobuf[2])
    {
    case '0': *vfo = RIG_VFO_A; break;

    case '1': *vfo = RIG_VFO_B; break;

    case '2': *vfo = RIG_VFO_MEM; break;

    default:
        rig_debug(RIG_DEBUG_ERR, "%s: unsupported VFO %c\n",
                  __func__, vfobuf[2]);
        return -RIG_EPROTO;
    }

    return RIG_OK;
}

static int ts870s_get_mode(RIG *rig, vfo_t vfo, rmode_t *mode, pbwidth_t *width)
{
    char buf[50];
    size_t buf_len;
    int retval;

    retval = kenwood_transaction(rig, "MD", buf, sizeof(buf));

    if (retval != RIG_OK)
    {
        return retval;
    }

    buf_len = strlen(buf);

    if (buf_len != 3 || buf[1] != 'D')
    {
        rig_debug(RIG_DEBUG_ERR, "%s: unexpected MD answer, len=%d\n",
                  __func__, (int)buf_len);
        return -RIG_ERJCTED;
    }

    switch (buf[2])
    {
    case MD_CW:         *mode = RIG_MODE_CW; break;

    case MD_CWR:    *mode = RIG_MODE_CWR; break;

    case MD_USB:    *mode = RIG_MODE_USB; break;

    case MD_LSB:    *mode = RIG_MODE_LSB; break;

    case MD_FM:         *mode = RIG_MODE_FM; break;

    case MD_AM:         *mode = RIG_MODE_AM; break;

    case MD_FSK:    *mode = RIG_MODE_RTTY; break;

    case MD_FSKR:   *mode = RIG_MODE_RTTYR; break;

    case MD_NONE:   *mode = RIG_MODE_NONE; break;

    default:
        rig_debug(RIG_DEBUG_ERR, "%s: unsupported mode '%c'\n",
                  __func__, buf[2]);
        return -RIG_EINVAL;
    }

    retval = kenwood_transaction(rig, "FW", buf, sizeof(buf));

    if (retval != RIG_OK)
    {
        return retval;
    }

    buf_len = strlen(buf);

    if (buf_len != 6 || buf[1] != 'W')
    {
        rig_debug(RIG_DEBUG_ERR, "%s: unexpected FW answer, len=%d\n",
                  __func__, (int)buf_len);
        return -RIG_ERJCTED;
    }

    *width = 10 * atoi(&buf[2]);

    if (RIG_MODE_USB == *mode || RIG_MODE_LSB == *mode || RIG_MODE_AM == *mode)
    {
        /* we only have HPF value and need LPF as well to calculate bandwidth */

        retval = kenwood_transaction(rig, "IS", buf, sizeof(buf));

        if (retval != RIG_OK)
        {
            return retval;
        }

        buf_len = strlen(buf);

        if (buf_len != 7 || buf[1] != 'S')
        {
            rig_debug(RIG_DEBUG_ERR, "%s: unexpected IS answer, len=%d\n",
                      __func__, (int)buf_len);
            return -RIG_ERJCTED;
        }

        *width = atoi(&buf[3]) - *width;  /* bandwidth <- LPF - HPF */
    }

    return RIG_OK;
}

static int ts870s_set_mode(RIG *rig, vfo_t vfo, rmode_t mode, pbwidth_t width)
{
    char buf[16];
    int kmode, retval;

    switch (mode)
    {
    case RIG_MODE_CW:       kmode = MD_CW; break;

    case RIG_MODE_CWR:      kmode = MD_CWR; break;

    case RIG_MODE_USB:      kmode = MD_USB; break;

    case RIG_MODE_LSB:      kmode = MD_LSB; break;

    case RIG_MODE_FM:       kmode = MD_FM; break;

    case RIG_MODE_AM:       kmode = MD_AM; break;

    case RIG_MODE_RTTY:     kmode = MD_FSK; break;

    case RIG_MODE_RTTYR:    kmode = MD_FSKR; break;

    default:
        rig_debug(RIG_DEBUG_ERR, "%s: unsupported mode %s\n",
                  __func__, rig_strrmode(mode));
        return -RIG_EINVAL;
    }

    SNPRINTF(buf, sizeof(buf), "MD%c", kmode);
    retval = kenwood_transaction(rig, buf, NULL, 0);

    if (retval != RIG_OK) { return retval; }

    if (RIG_PASSBAND_NOCHANGE == width)
    {
        if (RIG_PASSBAND_NORMAL !=
                width) /* leave well alone if default passband requested */
        {
            if (RIG_MODE_USB == mode || RIG_MODE_LSB == mode || RIG_MODE_AM == mode)
            {
                pbwidth_t mode_default_hpf;

                /* we assume the HPF is set to default and set the LPF to
                     give the best approximaation of the requested width */
                if (RIG_MODE_AM == mode)
                {
                    mode_default_hpf = 300;
                }
                else
                {
                    mode_default_hpf = 100;
                }

                SNPRINTF(buf, sizeof(buf), "IS %04d", (int)(width + mode_default_hpf));
                retval = kenwood_transaction(rig, buf, NULL, 0);
            }
            else
            {
                /*
                 * This rig will simply use an IF bandpass which is closest to width,
                 * so we don't need to check the value...
                 */
                SNPRINTF(buf, sizeof(buf), "FW%04d", (int)width / 10);
                retval = kenwood_transaction(rig, buf, NULL, 0);
            }
        }
    }

    return retval;
}

int ts870s_set_level(RIG *rig, vfo_t vfo, setting_t level, value_t val)
{
    char levelbuf[16];
    int intval;

    switch (level)
    {
    case RIG_LEVEL_RFPOWER:
        intval = val.f * 100;
        SNPRINTF(levelbuf, sizeof(levelbuf), "PC%03d", intval);
        return kenwood_transaction(rig, levelbuf, NULL, 0);
        break;

    default:
        return kenwood_set_level(rig, vfo, level, val);
    }

    return RIG_OK;
}

static int ts870s_get_level(RIG *rig, vfo_t vfo, setting_t level, value_t *val)
{
    char lvlbuf[50];
    size_t lvl_len;
    int lvl, retval;
    int i, ret, agclevel;

    switch (level)
    {
    case RIG_LEVEL_STRENGTH:
        retval = kenwood_transaction(rig, "SM", lvlbuf, sizeof(lvlbuf));

        if (retval != RIG_OK)
        {
            return retval;
        }

        lvl_len = strlen(lvlbuf);

        if (lvl_len != 6 || lvlbuf[1] != 'M')
        {
            rig_debug(RIG_DEBUG_ERR, "ts870s_get_level: wrong answer len=%d\n",
                      (int)lvl_len);
            return -RIG_ERJCTED;
        }

        /* Frontend expects:  -54 = S0, 0 = S9  */
        sscanf(lvlbuf + 2, "%d", &val->i);
        val->i = (val->i * 3.6) - 54;
        break;

    case RIG_LEVEL_SWR:
        retval = kenwood_transaction(rig, "RM", lvlbuf, sizeof(lvlbuf));

        if (retval != RIG_OK)
        {
            return retval;
        }

        /* set meter to SWR if needed */
        if (lvlbuf[2] != '1')
        {
            retval = kenwood_transaction(rig, "RM1", NULL, 0);

            if (retval != RIG_OK)
            {
                return retval;
            }

            retval = kenwood_transaction(rig, "RM", lvlbuf, sizeof(lvlbuf));

            if (retval != RIG_OK)
            {
                return retval;
            }
        }

        i = atoi(&lvlbuf[3]);

        if (i == 30)
        {
            val->f = 150.0;    /* infinity :-) */
        }
        else
        {
            val->f = 60.0 / (30.0 - (float)i) - 1.0;
        }

        break;

    case RIG_LEVEL_COMP:
        retval = kenwood_transaction(rig, "RM", lvlbuf, sizeof(lvlbuf));

        if (retval != RIG_OK)
        {
            return retval;
        }

        /* set meter to COMP if needed */
        if (lvlbuf[2] != '2')
        {
            retval = kenwood_transaction(rig, "RM2", NULL, 0);

            if (retval != RIG_OK)
            {
                return retval;
            }

            retval = kenwood_transaction(rig, "RM", lvlbuf, sizeof(lvlbuf));

            if (retval != RIG_OK)
            {
                return retval;
            }
        }

        val->f = (float)atoi(&lvlbuf[3]) / 30.0;
        break;

    case RIG_LEVEL_ALC:
        retval = kenwood_transaction(rig, "RM", lvlbuf, sizeof(lvlbuf));

        if (retval != RIG_OK)
        {
            return retval;
        }

        /* set meter to ALC if needed */
        if (lvlbuf[2] != '3')
        {
            retval = kenwood_transaction(rig, "RM3", NULL, 0);

            if (retval != RIG_OK)
            {
                return retval;
            }

            retval = kenwood_transaction(rig, "RM", lvlbuf, sizeof(lvlbuf));

            if (retval != RIG_OK)
            {
                return retval;
            }
        }

        val->f = (float)atoi(&lvlbuf[3]) / 30.0;
        break;

    case RIG_LEVEL_ATT:
        retval = kenwood_transaction(rig, "RA", lvlbuf, sizeof(lvlbuf));

        if (retval != RIG_OK)
        {
            return retval;
        }

        lvl_len = strlen(lvlbuf);

        if (lvl_len != 4)
        {
            rig_debug(RIG_DEBUG_ERR, "%s: unexpected answer len=%d\n", __func__,
                      (int)lvl_len);
            return -RIG_ERJCTED;
        }

        sscanf(lvlbuf + 2, "%d", &lvl);

        if (lvl == 0)
        {
            val->i = 0;
        }
        else
        {
            for (i = 0; i < lvl && i < HAMLIB_MAXDBLSTSIZ; i++)
                if (rig->state.attenuator[i] == 0)
                {
                    rig_debug(RIG_DEBUG_ERR, "ts870s_get_level: "
                              "unexpected att level %d\n", lvl);
                    return -RIG_EPROTO;
                }

            if (i != lvl)
            {
                return -RIG_EINTERNAL;
            }

            val->i = rig->state.attenuator[i - 1];
        }

        break;

    case RIG_LEVEL_RFPOWER:
        /* RFPOWER is 0..100 and not 0..255 like all the other levels*/
        retval = kenwood_transaction(rig, "PC", lvlbuf, sizeof(lvlbuf));

        if (retval != RIG_OK)
        {
            return retval;
        }

        lvl_len = strlen(lvlbuf);

        if (lvl_len != 5 || lvlbuf[1] != 'C')
        {
            rig_debug(RIG_DEBUG_ERR, "ts870s_get_level: wrong answer len=%d\n",
                      (int)lvl_len);
            return -RIG_ERJCTED;
        }

        sscanf(lvlbuf + 2, "%d", &lvl);
        val->f = lvl / 100.;
        break;


    case RIG_LEVEL_AF:
        return get_kenwood_level(rig, "AG", &val->f, NULL);

    case RIG_LEVEL_RF:
        return get_kenwood_level(rig, "RG", &val->f, NULL);

    case RIG_LEVEL_SQL:
        return get_kenwood_level(rig, "SQ", &val->f, NULL);

    case RIG_LEVEL_MICGAIN:
        return get_kenwood_level(rig, "MG", &val->f, NULL);

    case RIG_LEVEL_AGC:
        ret = get_kenwood_level(rig, "GT", &val->f, NULL);
        agclevel = 255 * val->f;

        if (agclevel == 0) { val->i = 0; }
        else if (agclevel < 85) { val->i = 1; }
        else if (agclevel < 170) { val->i = 2; }
        else if (agclevel <= 255) { val->i = 3; }

        return ret;

    case RIG_LEVEL_IF:
    case RIG_LEVEL_APF:
    case RIG_LEVEL_NR:
    case RIG_LEVEL_PBT_IN:
    case RIG_LEVEL_PBT_OUT:
    case RIG_LEVEL_CWPITCH:
    case RIG_LEVEL_KEYSPD:
    case RIG_LEVEL_NOTCHF:
    case RIG_LEVEL_BKINDL:
    case RIG_LEVEL_BALANCE:
        return -RIG_ENIMPL;

    case RIG_LEVEL_PREAMP:
        return -RIG_ENAVAIL;

    default:
        rig_debug(RIG_DEBUG_ERR, "%s: unsupported get_level %s", __func__,
                  rig_strlevel(level));
        return -RIG_EINVAL;
    }

    return RIG_OK;
}


/*
 * ts870s rig capabilities.
 * Notice that some rigs share the same functions.
 * Also this struct is READONLY!
 * RIT: Variable Range ±9.99 kHz
 *
 * part of infos comes from .http = //www.kenwood.net/
 */
const struct rig_caps ts870s_caps =
{
    RIG_MODEL(RIG_MODEL_TS870S),
    .model_name = "TS-870S",
    .mfg_name =  "Kenwood",
    .version =  BACKEND_VER ".1",
    .copyright =  "LGPL",
    .status =  RIG_STATUS_STABLE,
    .rig_type =  RIG_TYPE_TRANSCEIVER,
    .ptt_type =  RIG_PTT_RIG,
    .dcd_type =  RIG_DCD_RIG,
    .port_type =  RIG_PORT_SERIAL,
    .serial_rate_min =  1200,
    .serial_rate_max =  57600,
    .serial_data_bits =  8,
    .serial_stop_bits =  1,
    .serial_parity =  RIG_PARITY_NONE,
    .serial_handshake =  RIG_HANDSHAKE_NONE,
    .write_delay =  0,
    .post_write_delay =  0,
    .timeout =  500,
    .retry =  10,

    .has_get_func =  TS870S_FUNC_ALL,
    .has_set_func =  TS870S_FUNC_ALL,
    .has_get_level =  TS870S_LEVEL_GET,
    .has_set_level =  TS870S_LEVEL_SET,
    .has_get_parm =  RIG_PARM_NONE,
    .has_set_parm =  RIG_PARM_NONE,    /* FIXME: parms */
    .level_gran =
    {
#include "level_gran_kenwood.h"
    },
    .parm_gran =  {},
    .ctcss_list =  kenwood38_ctcss_list,
    .dcs_list =  NULL,
    .preamp =   { RIG_DBLST_END, }, /* FIXME: preamp list */
    .attenuator =   { 6, 12, 18, RIG_DBLST_END, },
    .max_rit =  kHz(9.99),
    .max_xit =  kHz(9.99),
    .max_ifshift =  Hz(0),
    .targetable_vfo =  RIG_TARGETABLE_FREQ,
    .transceive =  RIG_TRN_RIG,
    // Has GT command but ranges from 000-005(Off) to 255 max
    // Would take special processing
    .bank_qty =   0,
    .chan_desc_sz =  0,


    .chan_list =  {
        {  0, 89, RIG_MTYPE_MEM  }, /* TBC */
        { 90, 99, RIG_MTYPE_EDGE },
        RIG_CHAN_END,
    },

    .rx_range_list1 =  {
        {kHz(100), MHz(30), TS870S_ALL_MODES, -1, -1, TS870S_VFO, TS870S_ANTS},
        RIG_FRNG_END,
    }, /* rx range */
    .tx_range_list1 =  {
        FRQ_RNG_HF(1, TS870S_OTHER_TX_MODES, W(5), W(100), TS870S_VFO, TS870S_ANTS), /* 100W class */
        FRQ_RNG_HF(1, TS870S_AM_TX_MODES, W(2), W(25), TS870S_VFO, TS870S_ANTS), /* 25W class */
        RIG_FRNG_END,
    },

    .rx_range_list2 =  {
        {kHz(100), MHz(30), TS870S_ALL_MODES, -1, -1, TS870S_VFO, TS870S_ANTS},
        RIG_FRNG_END,
    }, /* rx range */
    .tx_range_list2 =  {
        FRQ_RNG_HF(2, TS870S_OTHER_TX_MODES, W(5), W(100), TS870S_VFO, TS870S_ANTS), /* 100W class */
        FRQ_RNG_HF(2, TS870S_AM_TX_MODES, W(2), W(25), TS870S_VFO, TS870S_ANTS), /* 25W class */
        RIG_FRNG_END,
    }, /* tx range */
    .tuning_steps =  {
        {TS870S_ALL_MODES, 50},
        {TS870S_ALL_MODES, 100},
        {TS870S_ALL_MODES, kHz(1)},
        {TS870S_ALL_MODES, kHz(5)},
        {TS870S_ALL_MODES, kHz(9)},
        {TS870S_ALL_MODES, kHz(10)},
        {TS870S_ALL_MODES, 12500},
        {TS870S_ALL_MODES, kHz(20)},
        {TS870S_ALL_MODES, kHz(25)},
        {TS870S_ALL_MODES, kHz(100)},
        {TS870S_ALL_MODES, MHz(1)},
        {TS870S_ALL_MODES, 0}, /* any tuning step */
        RIG_TS_END,
    },
    /* mode/filter list, remember: order matters! */
    .filters =  {
        {RIG_MODE_SSB, Hz(200)},
        {RIG_MODE_SSB, Hz(0)},
        {RIG_MODE_SSB, Hz(600)},
        {RIG_MODE_CW, Hz(400)},
        {RIG_MODE_CW, Hz(100)},
        {RIG_MODE_CW, Hz(1000)},
        {RIG_MODE_RTTY, Hz(1000)},
        {RIG_MODE_RTTY, Hz(500)},
        {RIG_MODE_RTTY, Hz(1500)},
        {RIG_MODE_AM, Hz(200)},
        {RIG_MODE_AM, Hz(0)},
        {RIG_MODE_AM, Hz(500)},
        {RIG_MODE_FM, kHz(8)},
        {RIG_MODE_FM, kHz(5)},
        {RIG_MODE_FM, kHz(14)},
        RIG_FLT_END,
    },
    .priv = (void *)& ts870s_priv_caps,

    .rig_init = kenwood_init,
    .rig_open = kenwood_open,
    .rig_close = kenwood_close,
    .rig_cleanup = kenwood_cleanup,
    .set_freq =  kenwood_set_freq,
    .get_freq =  kenwood_get_freq,
    .set_rit =  kenwood_set_rit,
    .get_rit =  kenwood_get_rit,
    .set_xit =  kenwood_set_xit,
    .get_xit =  kenwood_get_xit,
    .set_mode =  ts870s_set_mode,
    .get_mode =  ts870s_get_mode,
    .set_vfo =  kenwood_set_vfo,
    .get_vfo =  ts870s_get_vfo,
    .set_split_vfo = kenwood_set_split_vfo,
    .get_split_vfo = kenwood_get_split_vfo_if,
    .set_ctcss_tone =  kenwood_set_ctcss_tone,
    .get_ctcss_tone =  kenwood_get_ctcss_tone,
    .get_ptt =  kenwood_get_ptt,
    .set_ptt =  kenwood_set_ptt,
    .get_dcd =  kenwood_get_dcd,
    .set_func =  kenwood_set_func,
    .get_func =  kenwood_get_func,
    .set_level =  ts870s_set_level,
    .get_level =  ts870s_get_level,
    .set_ant =  kenwood_set_ant,
    .get_ant =  kenwood_get_ant,
    .send_morse =  kenwood_send_morse,
    .wait_morse =  rig_wait_morse,
    .vfo_op =  kenwood_vfo_op,
    .set_mem =  kenwood_set_mem,
    .get_mem =  kenwood_get_mem,
    .set_trn =  kenwood_set_trn,
    .get_trn =  kenwood_get_trn,
    .set_powerstat =  kenwood_set_powerstat,
    .get_powerstat =  kenwood_get_powerstat,
    .reset =  kenwood_reset,
    .hamlib_check_rig_caps = HAMLIB_CHECK_RIG_CAPS
};

/*
 * Function definitions below
 */

