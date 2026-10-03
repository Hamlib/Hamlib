/*
 *  Hamlib QRP Labs backend - QMX description (extracted from ts480.c)
 *  Copyright (c) 2000-2004 by Stephane Fillod and Juergen Rinas
 *  Copyright (c) 2021 by Mikael Nousiainen
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

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "hamlib/rig.h"
#include "idx_builtin.h"
#include "misc.h"
#include "kenwood.h"
#include "cache.h"

#define QMX_ALL_MODES (RIG_MODE_SSB|RIG_MODE_AM|RIG_MODE_CW|RIG_MODE_CWR|RIG_MODE_PKTUSB|RIG_MODE_PKTLSB)
#define QMX_LEVEL_GET (RIG_LEVEL_SWR|RIG_LEVEL_RFPOWER_METER|RIG_LEVEL_RFPOWER_METER_WATTS)
/*
 * Full scale of the QMX power meter, used to scale RFPOWER_METER to 0.0..1.0.
 * 6W is the stock value; a radio carrying the PA0RDT 4+4 BS170 PA modification
 * reports 12W instead.  qrplabs_qmx_open asks the radio which it is, so this is
 * only the fallback for firmware too old to answer.
 */
#define QMX_DEFAULT_MAX_POWER 6.0f
/* Menu Manager path to the setting, see the QMX CAT manual */
#define QMX_POWER_FULLSCALE_CMD \
    "MMDisplay/controls|Pwr/SWR display|Power fullscale"

#define QMX_VFO (RIG_VFO_A|RIG_VFO_B)

/* Shared QRP Labs helpers, also used by the QDX/QRPLabs driver. */
int qrplabs_open(RIG *rig)
{
    int retval;
    char buf[64];
    struct kenwood_priv_data *priv = (struct kenwood_priv_data *) STATE(rig)->priv;
    ENTERFUNC;
    retval = kenwood_open(rig);

    if (retval != RIG_OK)
    {
        RETURNFUNC(retval);
    }

    retval = kenwood_transaction(rig, "VN", buf, sizeof(buf));

    if (retval == RIG_OK)
    {
        strtok(buf, ";");
        rig_debug(RIG_DEBUG_VERBOSE, "%s: firmware version %s\n", __func__, &buf[2]);
    }

    priv->is_emulation = 1;
    RETURNFUNC(retval);
}

int qrplabs_get_clock(RIG *rig, int *year, int *month, int *day, int *hour,
                      int *min, int *sec, double *msec, int *utc_offset)
{
    char tm_cmd[32];
    char tm_buf[32];
    *year = *month = *day = *hour = *min = *sec = *msec = *utc_offset = 0;
    *month = 0;
    *day = 0;
    sprintf(tm_cmd, "TM;");
    int retval = kenwood_transaction(rig, tm_cmd, tm_buf, sizeof(tm_buf));

    if (retval == RIG_OK && strlen(tm_buf) >= 8) { sscanf(tm_buf, "TM%02d%02d%02d", hour, min, sec); }

    return retval;
}

int qrplabs_set_clock(RIG *rig, int year, int month, int day, int hour, int min,
                      int sec, double msec, int utc_offset)
{
    char tm_cmd[32];
    sprintf(tm_cmd, "TM%02d%02d%02d;", hour, min, sec);
    int retval = kenwood_transaction(rig, tm_cmd, NULL, 0);

    if (retval != RIG_OK)
    {
        rig_debug(RIG_DEBUG_ERR, "%s: error setting time: %s\n", __func__,
                  rigerror(retval));
    }

    return retval;
}



/* Protocol reference: https://qrp-labs.com/images/qmx/manuals/cat_1_04_004.pdf
 * MD8 starts SWR tune; never expose it as a normal operating mode.
 * QMX FSK uses USB; FSK reverse uses LSB.
 * A private table also avoids other emulations changing our mode mapping.
 */
static rmode_t qrplabs_qmx_modes[KENWOOD_MODE_TABLE_MAX] =
{
    [1] = RIG_MODE_LSB,
    [2] = RIG_MODE_USB,
    [3] = RIG_MODE_CW,
    [5] = RIG_MODE_AM,
    [6] = RIG_MODE_PKTUSB,
    [7] = RIG_MODE_CWR,
    [9] = RIG_MODE_PKTLSB,
};

static struct kenwood_priv_caps qrplabs_qmx_priv_caps =
{
    .cmdtrm = EOM_KEN,
    .mode_table = qrplabs_qmx_modes,
};

/* Keep the per-rig normal width in sync with readback. The frontend uses it
 * to populate its cache after set_mode(..., RIG_PASSBAND_NORMAL).
 */
static int qrplabs_qmx_read_width(RIG *rig, rmode_t mode, pbwidth_t *width)
{
    char buf[16];
    int retval = kenwood_safe_transaction(rig, "FW", buf, sizeof(buf), 6);

    if (retval != RIG_OK) { return retval; }
    if (strspn(buf + 2, "0123456789") != 4 || atoi(buf + 2) == 0)
    { return -RIG_EPROTO; }
    *width = atoi(buf + 2);

    for (int i = 0; i < HAMLIB_FLTLSTSIZ && STATE(rig)->filters[i].modes; i++)
    {
        if (STATE(rig)->filters[i].modes & mode)
        {
            /* Keep all advertised choices when promoting a new normal width. */
            for (int j = i + 1; j < HAMLIB_FLTLSTSIZ && STATE(rig)->filters[j].modes; j++)
            {
                if ((STATE(rig)->filters[j].modes & mode)
                        && STATE(rig)->filters[j].width == *width)
                {
                    STATE(rig)->filters[j].width = STATE(rig)->filters[i].width;
                    break;
                }
            }
            STATE(rig)->filters[i].width = *width;
            break;
        }
    }
    return RIG_OK;
}

static int qrplabs_qmx_set_mode(RIG *rig, vfo_t vfo, rmode_t mode,
                               pbwidth_t width)
{
    char cmd[64], buf[16];
    int kmode, retval;
    int ssb = mode == RIG_MODE_USB || mode == RIG_MODE_LSB;
    int set_width = width != RIG_PASSBAND_NORMAL && width != RIG_PASSBAND_NOCHANGE;
    int write_filter = 0;
    pbwidth_t actual_width;

    if (mode == RIG_MODE_NONE) { return -RIG_EINVAL; }
    kmode = rmode2kenwood(mode, qrplabs_qmx_modes);
    if (kmode < 0) { return -RIG_EINVAL; }

    if (set_width)
    {
        if (ssb)
        {
            if (width != 2500 && width != 2700 && width != 2900 && width != 3200)
            { return -RIG_EINVAL; }

            /* MM writes persist. Avoid rewriting an unchanged setting. */
            retval = kenwood_safe_transaction(rig, "MMSSB|Filter RX", buf, sizeof(buf), 6);
            if (retval != RIG_OK) { return retval; }
            if (strspn(buf + 2, "0123456789") != 4) { return -RIG_EPROTO; }
            write_filter = atoi(buf + 2) != width;
        }
        else if (width != rig_passband_normal(rig, mode))
        { return -RIG_ENAVAIL; }
    }

    SNPRINTF(cmd, sizeof(cmd), "MD%d", kmode);
    retval = kenwood_transaction(rig, cmd, NULL, 0);
    if (retval != RIG_OK) { return retval; }

    /* Mode is shared by both VFOs, including on subsequent command failure. */
    rig_invalidate_cache_mode(rig, RIG_VFO_ALL);

    if (write_filter)
    {
        SNPRINTF(cmd, sizeof(cmd), "MMSSB|Filter RX=%d", (int)width);
        retval = kenwood_transaction(rig, cmd, NULL, 0);
        if (retval != RIG_OK) { return retval; }
    }

    if (ssb && set_width)
    {
        /* Apply also when the stored value was already correct but MM Effect
         * is On demand and the active configuration has not been reloaded.
         * Filter TX is independent and is deliberately not changed here.
         */
        retval = kenwood_transaction(rig, "MU", NULL, 0);
        if (retval != RIG_OK) { return retval; }
    }

    retval = qrplabs_qmx_read_width(rig, mode, &actual_width);
    if (retval != RIG_OK) { return retval; }
    if (set_width && actual_width != width) { return -RIG_ENAVAIL; }
    return RIG_OK;
}

static int qrplabs_qmx_get_mode(RIG *rig, vfo_t vfo, rmode_t *mode,
                               pbwidth_t *width)
{
    char buf[16];
    int retval;

    retval = kenwood_safe_transaction(rig, "MD", buf, sizeof(buf), 3);
    if (retval != RIG_OK) { return retval; }
    if (buf[2] == '8') { return -RIG_ENAVAIL; } /* SWR tune */
    if (buf[2] < '0' || buf[2] > '9'
            || qrplabs_qmx_modes[buf[2] - '0'] == RIG_MODE_NONE)
    { return -RIG_EPROTO; }
    *mode = qrplabs_qmx_modes[buf[2] - '0'];

    return qrplabs_qmx_read_width(rig, *mode, width);
}

static const char *qrplabs_qmx_get_info(RIG *rig)
{
    static char firmware[64];

    if (kenwood_transaction(rig, "VN", firmware, sizeof(firmware)) != RIG_OK)
    { return NULL; }
    return firmware + 2;
}

/* QMX CAT 1.04_004: FR/FT setters select A, B, or A-RX/B-TX split.
 * They are not independent receive/transmit VFO selectors as on Kenwood.
 */
static int qrplabs_qmx_get_vfo(RIG *rig, vfo_t *vfo)
{
    char buf[8];
    int retval = kenwood_safe_transaction(rig, "FR", buf, sizeof(buf), 3);

    if (retval != RIG_OK) { return retval; }
    if (buf[2] != '0' && buf[2] != '1') { return -RIG_EPROTO; }

    *vfo = buf[2] == '0' ? RIG_VFO_A : RIG_VFO_B;
    return RIG_OK;
}

static int qrplabs_qmx_set_vfo(RIG *rig, vfo_t vfo)
{
    struct kenwood_priv_data *priv = STATE(rig)->priv;
    int retval;

    if (vfo == RIG_VFO_CURR) { return RIG_OK; }
    if (vfo != RIG_VFO_A && vfo != RIG_VFO_B) { return -RIG_EINVAL; }

    retval = kenwood_transaction(rig, vfo == RIG_VFO_A ? "FR0" : "FR1",
                                 NULL, 0);
    if (retval != RIG_OK) { return retval; }

    priv->split = RIG_SPLIT_OFF;
    priv->tx_vfo = vfo;
    rig_set_current_vfo_state(rig, vfo);
    rig_set_split_routing_state(rig, RIG_SPLIT_OFF, vfo, vfo);
    return RIG_OK;
}

static int qrplabs_qmx_set_split_vfo(RIG *rig, vfo_t vfo, split_t split,
                                    vfo_t txvfo)
{
    struct kenwood_priv_data *priv = STATE(rig)->priv;
    int retval;

    if (split != RIG_SPLIT_OFF && split != RIG_SPLIT_ON) { return -RIG_EINVAL; }
    if (vfo == RIG_VFO_CURR)
    {
        retval = qrplabs_qmx_get_vfo(rig, &vfo);
        if (retval != RIG_OK) { return retval; }
    }

    if (split == RIG_SPLIT_OFF) { return qrplabs_qmx_set_vfo(rig, vfo); }

    /* Reverse split is not supported. Reject it before changing the radio. */
    if (vfo != RIG_VFO_A || txvfo != RIG_VFO_B) { return -RIG_EINVAL; }

    retval = kenwood_transaction(rig, "FR2", NULL, 0);
    if (retval != RIG_OK) { return retval; }

    priv->split = RIG_SPLIT_ON;
    priv->tx_vfo = RIG_VFO_B;
    rig_set_current_vfo_state(rig, RIG_VFO_A);
    rig_set_split_routing_state(rig, RIG_SPLIT_ON, RIG_VFO_A, RIG_VFO_B);
    return RIG_OK;
}

static int qrplabs_qmx_get_split_vfo(RIG *rig, vfo_t vfo, split_t *split,
                                    vfo_t *txvfo)
{
    struct kenwood_priv_data *priv = STATE(rig)->priv;
    char buf[8];
    int retval = kenwood_safe_transaction(rig, "SP", buf, sizeof(buf), 3);

    if (retval != RIG_OK) { return retval; }
    if (buf[2] != '0' && buf[2] != '1') { return -RIG_EPROTO; }
    *split = buf[2] == '1' ? RIG_SPLIT_ON : RIG_SPLIT_OFF;

    retval = kenwood_safe_transaction(rig, "FT", buf, sizeof(buf), 3);
    if (retval != RIG_OK) { return retval; }
    if (buf[2] != '0' && buf[2] != '1') { return -RIG_EPROTO; }
    *txvfo = buf[2] == '0' ? RIG_VFO_A : RIG_VFO_B;
    priv->split = *split;
    priv->tx_vfo = *txvfo;
    return RIG_OK;
}

/*
 * The QMX power meter has a configurable full scale reading: 6W for a stock
 * radio, 12W for one carrying the PA0RDT 4+4 BS170 PA modification.  Ask the
 * radio which it is, so RIG_LEVEL_RFPOWER_METER scales against the right value,
 * and keep the answer in level_gran where get_level can find it again.
 * Firmware predating the Menu Manager command answers "?;" -- in that case the
 * default declared in the caps stands.
 */
static int qrplabs_qmx_open(RIG *rig)
{
    struct kenwood_priv_data *priv = (struct kenwood_priv_data *) STATE(rig)->priv;
    char buf[KENWOOD_MAX_BUF_LEN];
    int retval;
    float fullscale;

    ENTERFUNC;

    retval = qrplabs_open(rig);

    if (retval != RIG_OK)
    {
        RETURNFUNC(retval);
    }

    /* QMX identifies as a TS-480, so kenwood_open's matching-model path
     * does not disable unsolicited IF replies for us.
     */
    retval = kenwood_transaction(rig, "AI0", NULL, 0);
    if (retval != RIG_OK) { RETURNFUNC(retval); }
    priv->is_emulation = 0;

    /* the probe is optional, so take "?;" at face value rather than retrying */
    priv->question_mark_response_means_rejected = 1;
    retval = kenwood_transaction(rig, QMX_POWER_FULLSCALE_CMD, buf, sizeof(buf));
    priv->question_mark_response_means_rejected = 0;

    if (retval != RIG_OK)
    {
        rig_debug(RIG_DEBUG_VERBOSE,
                  "%s: power fullscale unavailable (%s), assuming %.0fW\n", __func__,
                  rigerror(retval), QMX_DEFAULT_MAX_POWER);
        RETURNFUNC(RIG_OK);
    }

    /* the reply carries the value of the setting, e.g. MM6W; or MM12W; */
    if (strlen(buf) <= 2 || sscanf(buf + 2, "%f", &fullscale) != 1
            || fullscale <= 0)
    {
        rig_debug(RIG_DEBUG_ERR,
                  "%s: unable to parse power fullscale from '%s', assuming %.0fW\n",
                  __func__, buf, QMX_DEFAULT_MAX_POWER);
        RETURNFUNC(RIG_OK);
    }

    rig_debug(RIG_DEBUG_VERBOSE, "%s: power meter fullscale %.0fW\n", __func__,
              fullscale);
    STATE(rig)->level_gran[LVL_RFPOWER_METER_WATTS].max.f = fullscale;

    RETURNFUNC(RIG_OK);
}

/*
 * QMX-specific meter readings.
 * SW; returns the SWR in hundredths, e.g. SW121; means 1.21:1
 * PC; returns measured output power in tenths of a watt, e.g. PC45; means 4.5W
 * Both are only measured while transmitting -- in receive the QMX answers with
 * a bare SW; or PC; and we report zero rather than a stale reading.
 */
static int qrplabs_qmx_get_level(RIG *rig, vfo_t vfo, setting_t level,
                                 value_t *val)
{
    char lvlbuf[16];
    int retval;
    int raw;
    float fullscale;

    ENTERFUNC;

    if (!val)
    {
        RETURNFUNC(-RIG_EINVAL);
    }

    switch (level)
    {
    case RIG_LEVEL_SWR:
        retval = kenwood_transaction(rig, "SW", lvlbuf, sizeof(lvlbuf));

        if (retval != RIG_OK)
        {
            RETURNFUNC(retval);
        }

        if (strlen(lvlbuf) <= 2)   /* not transmitting, no reading available */
        {
            val->f = 0.0;
            RETURNFUNC(RIG_OK);
        }

        if (sscanf(lvlbuf + 2, "%d", &raw) != 1)
        {
            rig_debug(RIG_DEBUG_ERR, "%s: unable to parse SWR from '%s'\n", __func__,
                      lvlbuf);
            RETURNFUNC(-RIG_EPROTO);
        }

        val->f = raw / 100.0f;
        RETURNFUNC(RIG_OK);

    case RIG_LEVEL_RFPOWER_METER:
    case RIG_LEVEL_RFPOWER_METER_WATTS:
        retval = kenwood_transaction(rig, "PC", lvlbuf, sizeof(lvlbuf));

        if (retval != RIG_OK)
        {
            RETURNFUNC(retval);
        }

        if (strlen(lvlbuf) <= 2)   /* not transmitting, no reading available */
        {
            val->f = 0.0;
            RETURNFUNC(RIG_OK);
        }

        if (sscanf(lvlbuf + 2, "%d", &raw) != 1)
        {
            rig_debug(RIG_DEBUG_ERR, "%s: unable to parse power from '%s'\n", __func__,
                      lvlbuf);
            RETURNFUNC(-RIG_EPROTO);
        }

        val->f = raw / 10.0f;   /* watts */

        if (level == RIG_LEVEL_RFPOWER_METER)
        {
            /* full scale as discovered by qrplabs_qmx_open */
            fullscale = STATE(rig)->level_gran[LVL_RFPOWER_METER_WATTS].max.f;

            if (fullscale <= 0) { fullscale = QMX_DEFAULT_MAX_POWER; }

            val->f /= fullscale;    /* 0.0..1.0 */
        }

        RETURNFUNC(RIG_OK);

    default:
        RETURNFUNC(-RIG_EINVAL);
    }
}

struct rig_caps qrplabs_qmx_caps =
{
    RIG_MODEL(RIG_MODEL_QRPLABS_QMX),
    .model_name = "QMX",
    .mfg_name = "QRPLabs",
    .version = BACKEND_VER ".5",
    .copyright = "LGPL",
    .status = RIG_STATUS_BETA,
    .rig_type = RIG_TYPE_TRANSCEIVER,
    .ptt_type = RIG_PTT_RIG,
    .dcd_type = RIG_DCD_NONE,
    .port_type = RIG_PORT_SERIAL,
    .serial_rate_min = 9600,
    .serial_rate_max = 230400,
    .serial_data_bits = 8,
    .serial_stop_bits = 1,
    .serial_parity = RIG_PARITY_NONE,
    .serial_handshake = RIG_HANDSHAKE_NONE,
    .write_delay = 0,
    .post_write_delay = 0,
    .timeout = 500,
    .retry = 3,
    .has_get_level = QMX_LEVEL_GET,
    .has_set_level = RIG_LEVEL_NONE,
    /* MD is global: addressing either VFO must not switch out of split. */
    .targetable_vfo = RIG_TARGETABLE_FREQ | RIG_TARGETABLE_MODE,
    .transceive = RIG_TRN_OFF,

    .rx_range_list1 = {
        {MHz(4), MHz(14), QMX_ALL_MODES, -1, -1, QMX_VFO},
        RIG_FRNG_END,
    }, /*!< Receive frequency range list for ITU region 1 */
    .tx_range_list1 = {
        {MHz(4), MHz(14),  QMX_ALL_MODES, 5000, 100000, QMX_VFO},
        RIG_FRNG_END,
    },  /*!< Transmit frequency range list for ITU region 1 */
    /* mode/filter list, remember: order matters! */
    .filters =  {
        /* Nominal value shown in the manual; refreshed from FW readback. */
        {RIG_MODE_SSB, kHz(2.7)},
        {RIG_MODE_SSB, kHz(2.5)},
        {RIG_MODE_SSB, kHz(2.9)},
        {RIG_MODE_SSB, kHz(3.2)},
        {RIG_MODE_AM | RIG_MODE_PKTUSB | RIG_MODE_PKTLSB, kHz(3.2)},
        {RIG_MODE_CW | RIG_MODE_CWR, Hz(300)},
        RIG_FLT_END,
    },
    .level_gran =
    {
#define NO_LVL_SWR
#define NO_LVL_RFPOWER_METER_WATTS
#include "level_gran_kenwood.h"
#undef NO_LVL_SWR
#undef NO_LVL_RFPOWER_METER_WATTS
        /* SW; and PC; report in hundredths and tenths respectively */
        [LVL_SWR] = {.min = {.f = 0}, .max = {.f = 99.99f}, .step = {.f = 0.01f}},
        /* max is the meter full scale, refined at open time -- see qrplabs_qmx_open */
        [LVL_RFPOWER_METER_WATTS] = {.min = {.f = 0}, .max = {.f = QMX_DEFAULT_MAX_POWER}, .step = {.f = 0.1f}},
    },

    .priv = (void *)&qrplabs_qmx_priv_caps,

    .rig_init = kenwood_init,
    .rig_open = qrplabs_qmx_open,
    .rig_cleanup = kenwood_cleanup,
    .set_freq = kenwood_set_freq,
    .get_freq = kenwood_get_freq,
    .set_mode = qrplabs_qmx_set_mode,
    .get_mode = qrplabs_qmx_get_mode,
    .set_vfo = qrplabs_qmx_set_vfo,
    .get_vfo = qrplabs_qmx_get_vfo,
    .set_split_vfo = qrplabs_qmx_set_split_vfo,
    .get_split_vfo = qrplabs_qmx_get_split_vfo,
    .get_ptt = kenwood_get_ptt,
    .set_ptt = kenwood_set_ptt,
    .get_level = qrplabs_qmx_get_level,
    .get_info = qrplabs_qmx_get_info,
    .get_clock = qrplabs_get_clock,
    .set_clock = qrplabs_set_clock,
    .hamlib_check_rig_caps = HAMLIB_CHECK_RIG_CAPS
};
