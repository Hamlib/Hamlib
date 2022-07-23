/*
 *  Hamlib Dorji DRA818 backend
 *  Copyright (c) 2017 by Jeroen Vreeken
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
#include <stdbool.h>

#include "hamlib/rig.h"
#include "bandplan.h"
#include "serial.h"
#include "register.h"
#include "tones.h"

#include "dra818.h"
#include "dorji.h"

static const char *dra818_handshake_cmd = "AT+DMOCONNECT\r\n";
static const char *dra818_handshake_res = "+DMOCONNECT:0\r\n";
static const char *dra818_setgroup_res = "+DMOSETGROUP:0\r\n";
static const char *dra818_setvolume_res = "+DMOSETVOLUME:0\r\n";

struct dra818_priv
{
    shortfreq_t tx_freq;
    shortfreq_t rx_freq;
    shortfreq_t bw;
    split_t split;
    tone_t ctcss_tone;
    tone_t ctcss_sql;
    tone_t dcs_code;
    tone_t dcs_sql;
    int sql;
    int vol;
};

static int dra818_response(RIG *rig, const char *expected)
{
    char response[80];
    int r = read_string(&rig->state.rigport, (unsigned char *) response,
                        sizeof(response),
                        "\n", 1, 0, 1);

    if (r != strlen(expected))
    {
        return -RIG_EIO;
    }

    if (strcmp(expected, response) != 0)
    {
        rig_debug(RIG_DEBUG_VERBOSE, "dra818: response: %s\n", response);
        return -RIG_ERJCTED;
    }

    return RIG_OK;
}

static void dra818_subaudio(RIG *rig, char *subaudio, int subaudio_len,
                            tone_t tone, tone_t code)
{
    if (code)
    {
        SNPRINTF(subaudio, subaudio_len, "%03uI", code % 10000);
        return;
    }
    else if (tone)
    {
        int i;

        for (i = 0; rig->caps->ctcss_list[i]; i++)
        {
            if (rig->caps->ctcss_list[i] == tone)
            {
                SNPRINTF(subaudio, subaudio_len, "%04d", (i + 1) % 10000);
                return;
            }
        }
    }

    subaudio[0] = '0';
    subaudio[1] = '0';
    subaudio[2] = '0';
    subaudio[3] = '0';
}

static int dra818_setgroup(RIG *rig)
{
    struct dra818_priv *priv = rig->state.priv;
    char cmd[80];
    char subtx[8] = { 0 };
    char subrx[8] = { 0 };

    dra818_subaudio(rig, subtx, sizeof(subtx), priv->ctcss_tone, priv->dcs_code);
    dra818_subaudio(rig, subrx, sizeof(subrx), priv->ctcss_sql, priv->dcs_sql);

    SNPRINTF(cmd, sizeof(cmd),
             "AT+DMOSETGROUP=%1d,%03d.%04d,%03d.%04d,%4s,%1d,%4s\r\n",
             priv->bw == 12500 ? 0 : 1,
             (int)(priv->tx_freq / 1000000), (int)((priv->tx_freq % 1000000) / 100),
             (int)(priv->rx_freq / 1000000), (int)((priv->rx_freq % 1000000) / 100),
             subtx, priv->sql, subrx);
    write_block(&rig->state.rigport, (unsigned char *) cmd, strlen(cmd));

    return dra818_response(rig, dra818_setgroup_res);
}

static int dra818_setvolume(RIG *rig)
{
    struct dra818_priv *priv = rig->state.priv;
    char cmd[80];

    SNPRINTF(cmd, sizeof(cmd), "AT+DMOSETVOLUME=%1d\r\n", priv->vol);
    write_block(&rig->state.rigport, (unsigned char *) cmd, strlen(cmd));

    return dra818_response(rig, dra818_setvolume_res);
}

int dra818_init(RIG *rig)
{
    struct dra818_priv *priv;

    rig_debug(RIG_DEBUG_VERBOSE, "%s: dra818_init called\n", __func__);

    rig->state.priv = calloc(sizeof(*priv), 1);

    if (!rig->state.priv)
    {
        return -RIG_ENOMEM;
    }

    priv = rig->state.priv;

    switch (rig->caps->rig_model)
    {
    case RIG_MODEL_DORJI_DRA818V:
        priv->rx_freq = 145000000;
        break;

    case RIG_MODEL_DORJI_DRA818U:
        priv->rx_freq = 435000000;
        break;
    }

    priv->tx_freq = priv->rx_freq;

    priv->bw = 12500;
    priv->split = RIG_SPLIT_OFF;
    priv->ctcss_tone = 0;
    priv->ctcss_sql = 0;
    priv->dcs_code = 0;
    priv->dcs_sql = 0;
    priv->sql = 4;
    priv->vol = 6;

    return RIG_OK;
}

int dra818_cleanup(RIG *rig)
{
    rig_debug(RIG_DEBUG_VERBOSE, "%s: dra818_cleanup called\n", __func__);

    free(rig->state.priv);

    return RIG_OK;
}

int dra818_open(RIG *rig)
{
    int i;
    int r = -1;

    for (i = 0; i < 3; i++)
    {
        write_block(&rig->state.rigport, (unsigned char *) dra818_handshake_cmd,
                    strlen(dra818_handshake_cmd));

        r = dra818_response(rig, dra818_handshake_res);

        if (r == RIG_OK)
        {
            break;
        }
    }

    if (r != RIG_OK)
    {
        return r;
    }

    r = dra818_setvolume(rig);

    if (r != RIG_OK)
    {
        return r;
    }

    return dra818_setgroup(rig);
}

int dra818_set_freq(RIG *rig, vfo_t vfo, freq_t freq)
{
    struct dra818_priv *priv = rig->state.priv;

    /* Nearest channel */
    shortfreq_t sfreq = ((freq + priv->bw / 2) / priv->bw);
    sfreq *= priv->bw;

    rig_debug(RIG_DEBUG_VERBOSE,
              "dra818: requested freq = %"PRIfreq" Hz, set freq = %d Hz\n",
              freq, (int)sfreq);

    if (vfo == RIG_VFO_RX)
    {
        priv->rx_freq = sfreq;

        if (priv->split == RIG_SPLIT_OFF)
        {
            priv->tx_freq = sfreq;
        }
    }
    else if (vfo == RIG_VFO_TX)
    {
        priv->tx_freq = sfreq;

        if (priv->split == RIG_SPLIT_OFF)
        {
            priv->rx_freq = sfreq;
        }
    }
    else
    {
        return -RIG_EINVAL;
    }

    return dra818_setgroup(rig);
}

int dra818_set_mode(RIG *rig, vfo_t vfo, rmode_t mode, pbwidth_t width)
{
    struct dra818_priv *priv = rig->state.priv;

    if (width > 12500)
    {
        priv->bw = 25000;
    }
    else
    {
        priv->bw = 12500;
    }

    rig_debug(RIG_DEBUG_VERBOSE, "dra818: bandwidth: %d\n", (int)priv->bw);

    return dra818_setgroup(rig);
}

int dra818_get_mode(RIG *rig, vfo_t vfo, rmode_t *mode, pbwidth_t *width)
{
    struct dra818_priv *priv = rig->state.priv;

    *mode = RIG_MODE_FM;
    *width = priv->bw;

    return RIG_OK;
}

int dra818_get_dcd(RIG *rig, vfo_t vfo, dcd_t *dcd)
{
    struct dra818_priv *priv = rig->state.priv;
    char cmd[80];
    char response[8];
    int r;

    SNPRINTF(cmd, sizeof(cmd), "S+%03d.%04d\r\n",
             (int)(priv->rx_freq / 1000000), (int)((priv->rx_freq % 1000000) / 100));
    write_block(&rig->state.rigport, (unsigned char *) cmd, strlen(cmd));

    r = read_string(&rig->state.rigport, (unsigned char *) response,
                    sizeof(response),
                    "\n", 1, 0, 1);

    if (r != 5)
    {
        return -RIG_EIO;
    }

    if (response[3] == 1)
    {
        *dcd = RIG_DCD_OFF;
    }
    else
    {
        *dcd = RIG_DCD_ON;
    }

    return RIG_OK;
}

int dra818_get_freq(RIG *rig, vfo_t vfo, freq_t *freq)
{
    struct dra818_priv *priv = rig->state.priv;

    switch (vfo)
    {
    case RIG_VFO_RX:
        *freq = priv->rx_freq;
        break;

    case RIG_VFO_TX:
        *freq = priv->tx_freq;
        break;

    default:
        return -RIG_EINVAL;
    }

    return RIG_OK;
}

int dra818_set_split_vfo(RIG *rig, vfo_t vfo, split_t split, vfo_t tx_vfo)
{
    struct dra818_priv *priv = rig->state.priv;

    priv->split = split;

    if (split == RIG_SPLIT_OFF)
    {
        priv->tx_freq = priv->rx_freq;
    }

    return dra818_setgroup(rig);
}

int dra818_get_split_vfo(RIG *rig, vfo_t vfo, split_t *split, vfo_t *tx_vfo)
{
    struct dra818_priv *priv = rig->state.priv;

    *split = priv->split;

    if (priv->split == RIG_SPLIT_ON)
    {
        *tx_vfo = RIG_VFO_TX;
    }
    else
    {
        *tx_vfo = RIG_VFO_RX;
    }

    return RIG_OK;
}

int dra818_get_level(RIG *rig, vfo_t vfo, setting_t level, value_t *val)
{
    struct dra818_priv *priv = rig->state.priv;

    switch (level)
    {
    case RIG_LEVEL_SQL:
        /* SQL range: 0..8 (0=monitor) */
        val->f = (priv->sql / 8.0);
        break;

    case RIG_LEVEL_AF:
        /* AF range: 1..8 */
        val->f = (priv->vol / 8.0);
        break;

    default:
        return -RIG_EINVAL;
    }

    return RIG_OK;
}

int dra818_set_level(RIG *rig, vfo_t vfo, setting_t level, value_t val)
{
    struct dra818_priv *priv = rig->state.priv;

    switch (level)
    {
    case RIG_LEVEL_SQL:
        /* SQL range: 0..8 (0=monitor) */
        priv->sql = val.f * 8;

        if (priv->sql < 0)
        {
            priv->sql = 0;
        }

        if (priv->sql > 8)
        {
            priv->sql = 8;
        }

        return dra818_setgroup(rig);

    case RIG_LEVEL_AF:
        /* AF range: 1..8 */
        priv->vol = val.f * 8;

        if (priv->vol < 1)
        {
            priv->vol = 1;
        }

        if (priv->vol > 8)
        {
            priv->vol = 8;
        }

        return dra818_setvolume(rig);

    default:
        break;
    }

    return -RIG_EINVAL;
}

int dra818_set_dcs_code(RIG *rig, vfo_t vfo, tone_t code)
{
    struct dra818_priv *priv = rig->state.priv;

    priv->dcs_code = code;

    if (code)
    {
        priv->ctcss_tone = 0;
    }

    return dra818_setgroup(rig);
}

int dra818_set_ctcss_tone(RIG *rig, vfo_t vfo, tone_t tone)
{
    struct dra818_priv *priv = rig->state.priv;

    priv->ctcss_tone = tone;

    if (tone)
    {
        priv->dcs_code = 0;
    }

    return dra818_setgroup(rig);
}

int dra818_set_dcs_sql(RIG *rig, vfo_t vfo, tone_t code)
{
    struct dra818_priv *priv = rig->state.priv;

    priv->dcs_sql = code;

    if (code)
    {
        priv->ctcss_sql = 0;
    }

    return dra818_setgroup(rig);
}

int dra818_set_ctcss_sql(RIG *rig, vfo_t vfo, tone_t tone)
{
    struct dra818_priv *priv = rig->state.priv;

    priv->ctcss_sql = tone;

    if (tone)
    {
        priv->dcs_sql = 0;
    }

    return dra818_setgroup(rig);
}

int dra818_get_ctcss_sql(RIG *rig, vfo_t vfo, tone_t *tone)
{
    struct dra818_priv *priv = rig->state.priv;

    *tone = priv->ctcss_sql;
    return RIG_OK;
}

int dra818_get_dcs_sql(RIG *rig, vfo_t vfo, tone_t *code)
{
    struct dra818_priv *priv = rig->state.priv;

    *code = priv->dcs_sql;
    return RIG_OK;
}

int dra818_get_dcs_code(RIG *rig, vfo_t vfo, tone_t *code)
{
    struct dra818_priv *priv = rig->state.priv;

    *code = priv->dcs_code;
    return RIG_OK;
}

int dra818_get_ctcss_tone(RIG *rig, vfo_t vfo, tone_t *tone)
{
    struct dra818_priv *priv = rig->state.priv;

    *tone = priv->ctcss_tone;
    return RIG_OK;
}

const struct rig_caps dra818u_caps =
{
    RIG_MODEL(RIG_MODEL_DORJI_DRA818U),
    .model_name =       "DRA818U",
    .mfg_name =     "Dorji",
    .version =      "20191209.0",
    .copyright =        "LGPL",
    .status =       RIG_STATUS_ALPHA,
    .rig_type =     RIG_TYPE_TRANSCEIVER,
    .ptt_type =     RIG_PTT_NONE,
    .dcd_type =     RIG_DCD_RIG,
    .port_type =        RIG_PORT_SERIAL,
    .serial_rate_min =      9600,
    .serial_rate_max =      9600,
    .serial_data_bits =     8,
    .serial_stop_bits =     1,
    .serial_parity =        RIG_PARITY_NONE,
    .serial_handshake =     RIG_HANDSHAKE_NONE,
    .write_delay =      0,
    .post_write_delay = 0,
    .timeout =      1000,
    .retry =        0,

    .has_get_func =     RIG_FUNC_TONE | RIG_FUNC_TSQL | RIG_FUNC_SQL,
    .has_set_func =     RIG_FUNC_TONE | RIG_FUNC_TSQL | RIG_FUNC_SQL,
    .has_get_level =    RIG_LEVEL_AF | RIG_LEVEL_SQL,
    .has_set_level =    RIG_LEVEL_AF | RIG_LEVEL_SQL,
    .has_get_parm =     RIG_PARM_NONE,
    .has_set_parm =     RIG_PARM_NONE,
    .level_gran =       {},
    .parm_gran =        {},
    .ctcss_list =       /* 38 according to doc, are they all correct? */
    (tone_t[])
    {
        670,  719,  744,  770,  797,  825,  854,
        885,  915,  948,  974, 1000, 1035, 1072,
        1109, 1148, 1188, 1230, 1273, 1318, 1365,
        1413, 1462, 1514, 1567, 1622, 1679, 1738,
        1799, 1862, 1928, 2035, 2107, 2181, 2257,
        2336, 2418, 2503, 0
    },
    .dcs_list =     common_dcs_list,
    .preamp =       { RIG_DBLST_END, },
    .attenuator =       { RIG_DBLST_END, },
    .max_rit =      Hz(0),
    .max_xit =      Hz(0),
    .max_ifshift =      Hz(0),
    .targetable_vfo =   0,
    .transceive =       RIG_TRN_OFF,
    .bank_qty =     0,
    .chan_desc_sz =     0,

    .chan_list =        { RIG_CHAN_END, },

    .rx_range_list1 = {
        {MHz(400), MHz(480), RIG_MODE_FM, -1, -1, RIG_VFO_RX },
        RIG_FRNG_END,
    },
    .rx_range_list2 = {
        {MHz(400), MHz(480), RIG_MODE_FM, -1, -1, RIG_VFO_RX },
        RIG_FRNG_END,
    },
    .tx_range_list1 = {
        FRQ_RNG_70cm(1, RIG_MODE_FM, W(0.5), W(1), RIG_VFO_TX, RIG_ANT_1),
        RIG_FRNG_END,
    },
    .tx_range_list2 = {
        FRQ_RNG_70cm(2, RIG_MODE_FM, W(0.5), W(1), RIG_VFO_TX, RIG_ANT_1),
        RIG_FRNG_END,
    },

    .tuning_steps = {
        {RIG_MODE_FM, Hz(12500)},
        RIG_TS_END,
    },
    .filters = {
        {RIG_MODE_FM, Hz(12500)},
        {RIG_MODE_FM, Hz(25000)},
        RIG_FLT_END,
    },

    .rig_init =     dra818_init,
    .rig_cleanup =      dra818_cleanup,
    .rig_open =             dra818_open,
    .set_freq =     dra818_set_freq,
    .get_freq =             dra818_get_freq,
    .set_split_vfo =        dra818_set_split_vfo,
    .get_split_vfo =        dra818_get_split_vfo,
    .set_mode =             dra818_set_mode,
    .get_mode =             dra818_get_mode,
    .get_dcd =      dra818_get_dcd,
    .set_level =        dra818_set_level,
    .get_level =        dra818_get_level,
    .set_dcs_code =         dra818_set_dcs_code,
    .set_ctcss_tone =       dra818_set_ctcss_tone,
    .set_dcs_sql =          dra818_set_dcs_sql,
    .set_ctcss_sql =        dra818_set_ctcss_sql,
    .get_dcs_code =         dra818_get_dcs_code,
    .get_ctcss_tone =       dra818_get_ctcss_tone,
    .get_dcs_sql =          dra818_get_dcs_sql,
    .get_ctcss_sql =        dra818_get_ctcss_sql,
    .hamlib_check_rig_caps = HAMLIB_CHECK_RIG_CAPS
};

const struct rig_caps dra818v_caps =
{
    RIG_MODEL(RIG_MODEL_DORJI_DRA818V),
    .model_name =       "DRA818V",
    .mfg_name =     "Dorji",
    .version =      "20191209.0",
    .copyright =        "LGPL",
    .status =       RIG_STATUS_ALPHA,
    .rig_type =     RIG_TYPE_TRANSCEIVER,
    .ptt_type =     RIG_PTT_NONE,
    .dcd_type =     RIG_DCD_RIG,
    .port_type =        RIG_PORT_SERIAL,
    .serial_rate_min =      9600,
    .serial_rate_max =      9600,
    .serial_data_bits =     8,
    .serial_stop_bits =     1,
    .serial_parity =        RIG_PARITY_NONE,
    .serial_handshake =     RIG_HANDSHAKE_NONE,
    .write_delay =      0,
    .post_write_delay = 0,
    .timeout =      1000,
    .retry =        0,

    .has_get_func =     RIG_FUNC_TONE | RIG_FUNC_TSQL | RIG_FUNC_SQL,
    .has_set_func =     RIG_FUNC_TONE | RIG_FUNC_TSQL | RIG_FUNC_SQL,
    .has_get_level =    RIG_LEVEL_AF | RIG_LEVEL_SQL,
    .has_set_level =    RIG_LEVEL_AF | RIG_LEVEL_SQL,
    .has_get_parm =     RIG_PARM_NONE,
    .has_set_parm =     RIG_PARM_NONE,
    .level_gran =       {},
    .parm_gran =        {},
    .ctcss_list =       /* 38 according to doc, are they all correct? */
    (tone_t[])
    {
        670,  719,  744,  770,  797,  825,  854,
        885,  915,  948,  974, 1000, 1035, 1072,
        1109, 1148, 1188, 1230, 1273, 1318, 1365,
        1413, 1462, 1514, 1567, 1622, 1679, 1738,
        1799, 1862, 1928, 2035, 2107, 2181, 2257,
        2336, 2418, 2503, 0
    },
    .dcs_list =     common_dcs_list,
    .preamp =       { RIG_DBLST_END, },
    .attenuator =       { RIG_DBLST_END, },
    .max_rit =      Hz(0),
    .max_xit =      Hz(0),
    .max_ifshift =      Hz(0),
    .targetable_vfo =   0,
    .transceive =       RIG_TRN_OFF,
    .bank_qty =     0,
    .chan_desc_sz =     0,

    .chan_list =        { RIG_CHAN_END, },

    .rx_range_list1 = {
        {MHz(134), MHz(174), RIG_MODE_FM, -1, -1, RIG_VFO_RX },
        RIG_FRNG_END,
    },
    .rx_range_list2 = {
        {MHz(134), MHz(174), RIG_MODE_FM, -1, -1, RIG_VFO_RX },
        RIG_FRNG_END,
    },
    .tx_range_list1 = {
        FRQ_RNG_2m(1, RIG_MODE_FM, W(0.5), W(1), RIG_VFO_TX, RIG_ANT_1),
        RIG_FRNG_END,
    },
    .tx_range_list2 = {
        FRQ_RNG_2m(2, RIG_MODE_FM, W(0.5), W(1), RIG_VFO_TX, RIG_ANT_1),
        RIG_FRNG_END,
    },

    .tuning_steps = {
        {RIG_MODE_FM, Hz(12500)},
        RIG_TS_END,
    },
    .filters = {
        {RIG_MODE_FM, Hz(12500)},
        {RIG_MODE_FM, Hz(25000)},
        RIG_FLT_END,
    },

    .rig_init =     dra818_init,
    .rig_cleanup =      dra818_cleanup,
    .rig_open =             dra818_open,
    .set_freq =     dra818_set_freq,
    .get_freq =             dra818_get_freq,
    .set_split_vfo =        dra818_set_split_vfo,
    .get_split_vfo =        dra818_get_split_vfo,
    .set_mode =             dra818_set_mode,
    .get_mode =             dra818_get_mode,
    .get_dcd =      dra818_get_dcd,
    .set_level =        dra818_set_level,
    .get_level =        dra818_get_level,
    .set_dcs_code =         dra818_set_dcs_code,
    .set_ctcss_tone =       dra818_set_ctcss_tone,
    .set_dcs_sql =          dra818_set_dcs_sql,
    .set_ctcss_sql =        dra818_set_ctcss_sql,
    .get_dcs_code =         dra818_get_dcs_code,
    .get_ctcss_tone =       dra818_get_ctcss_tone,
    .get_dcs_sql =          dra818_get_dcs_sql,
    .get_ctcss_sql =        dra818_get_ctcss_sql,
    .hamlib_check_rig_caps = HAMLIB_CHECK_RIG_CAPS
};

