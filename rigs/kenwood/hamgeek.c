/*
 *  Hamlib Hamgeek uSDX support using supported Kenwood commands
 *  Copyright (c) 2026 Leo Pizzolante IU7TUY, ciao@iu7tuy.it
 *
 *  Currently supports:
 *    - Hamgeek uSDX: Arduino-based HF transceiver, TS-480 CAT subset over
 *      Bluetooth.
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
 *  See the file 'COPYING.LIB' in the main Hamlib distribution directory for
 *  the complete text of the GNU Lesser Public License version 2.1.
 *
 */
/* SPDX-License-Identifier: LGPL-2.1-or-later */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "hamlib/rig.h"
#include "kenwood.h"
#include "hamgeek.h"

/* ======================================================================
 * Hamgeek uSDX
 *
 * Arduino-based HF transceiver with optional Bluetooth connectivity that
 * implements a subset of the Kenwood TS-480 CAT protocol.
 *
 * Firmware mode encoding: MD1=LSB MD2=USB MD3=CW MD4=FM MD5=AM (1-indexed).
 *
 * Supported CAT commands:
 *   FA<freq>;  FA;   - VFO A frequency
 *   MD<n>;     MD;   - mode (1=LSB 2=USB 3=CW 4=FM 5=AM)
 *   TX;  TX0;  RX;   - PTT on/off (TX sends no reply; RX replies "RX0;")
 *   IF;               - full status (38-char response, same layout as TS-480;
 *                       mode at position [29])
 *   PS;               - power status (replies PS1;)
 *   ID;               - replies "ID020;" (TS-480 ID)
 *
 * Not supported (returns '?;'):
 *   FB;  - VFO B frequency (only VFO A accessible via CAT)
 *   FR;  - VFO select (FR0;/FR1; not implemented)
 *   TQ;  - transmit query (TX state not readable via CAT)
 * ====================================================================== */

/* Modes confirmed by hardware test: MD1=LSB MD2=USB MD3=CW MD4=FM MD5=AM */
#define USDX_ALL_MODES (RIG_MODE_LSB | RIG_MODE_USB | RIG_MODE_CW | \
                        RIG_MODE_CWR | RIG_MODE_AM | RIG_MODE_FM | \
                        RIG_MODE_PKTUSB | RIG_MODE_PKTLSB)
#define USDX_TX_MODES  (RIG_MODE_LSB | RIG_MODE_USB | RIG_MODE_CW | \
                        RIG_MODE_AM | RIG_MODE_FM | RIG_MODE_PKTUSB | \
                        RIG_MODE_PKTLSB)
/* Only VFO A is present on uSDX hardware (FB; returns '?;') */
#define USDX_VFO       RIG_VFO_A

#define BACKEND_VER "20260423"

/*
 * Minimal TS-480 subset: no slope filters, no special filter width table.
 * mode_table=NULL so kenwood_get_mode / kenwood_set_mode fall back to the
 * default kenwood_mode_table; PKTUSB/PKTLSB are remapped in hamgeek_usdx_set_mode.
 */
static struct kenwood_priv_caps hamgeek_usdx_priv_caps =
{
    .cmdtrm    = EOM_KEN,
    .mode_table = NULL,
};

/*
 * hamgeek_usdx_open - rig_open callback
 *
 * Calls kenwood_open() then disables the post-open ID verification probe
 * (no_id=1).  The uSDX firmware replies '?;' to ID;, and after TX; it sends
 * an unsolicited "TX0;" echo that would corrupt the reply to ID; if the probe
 * were left enabled.
 */
static int hamgeek_usdx_open(RIG *rig)
{
    struct kenwood_priv_data *priv;
    int retval;

    rig_debug(RIG_DEBUG_VERBOSE, "%s called\n", __func__);

    retval = kenwood_open(rig);

    if (retval != RIG_OK)
    {
        return retval;
    }

    priv = (struct kenwood_priv_data *)STATE(rig)->priv;
    priv->no_id = 1;    /* ID; not implemented in uSDX firmware */

    return RIG_OK;
}

/*
 * hamgeek_usdx_set_mode - kenwood_set_mode wrapper
 *
 * The uSDX firmware accepts MD1..MD5 only (LSB/USB/CW/FM/AM).
 * Map unsupported Hamlib modes to the nearest implemented firmware mode:
 *   PKTUSB  -> USB   (MD2)  digital ops via USB audio
 *   PKTLSB  -> LSB   (MD1)
 *   CWR     -> CW    (MD3)  firmware has no reverse-CW mode
 *   RTTY/R  -> USB/LSB
 */
static int hamgeek_usdx_set_mode(RIG *rig, vfo_t vfo, rmode_t mode,
                                  pbwidth_t width)
{
    rig_debug(RIG_DEBUG_VERBOSE, "%s called\n", __func__);

    if (mode == RIG_MODE_PKTUSB)
    {
        mode = RIG_MODE_USB;
    }
    else if (mode == RIG_MODE_PKTLSB)
    {
        mode = RIG_MODE_LSB;
    }
    else if (mode == RIG_MODE_CWR)
    {
        /* MD7 would be out-of-range; send CW (MD3) instead */
        mode = RIG_MODE_CW;
    }
    else if (mode == RIG_MODE_RTTY || mode == RIG_MODE_RTTYR)
    {
        mode = (mode == RIG_MODE_RTTYR) ? RIG_MODE_LSB : RIG_MODE_USB;
    }

    return kenwood_set_mode(rig, vfo, mode, width);
}

/*
 * hamgeek_usdx_set_vfo - set_vfo wrapper
 *
 * The uSDX firmware does not implement FR0;/FR1; (Kenwood VFO select);
 * only VFO A is accessible via CAT.  VFO_A and VFO_CURR are accepted
 * silently; all other targets return -RIG_ENTARGET.
 */
static int hamgeek_usdx_set_vfo(RIG *rig, vfo_t vfo)
{
    rig_debug(RIG_DEBUG_VERBOSE, "%s called vfo=%s\n", __func__,
              rig_strvfo(vfo));

    if (vfo == RIG_VFO_B || vfo == RIG_VFO_SUB || vfo == RIG_VFO_MEM)
    {
        return -RIG_ENTARGET;  /* VFO B / memory not accessible via CAT */
    }

    return RIG_OK;  /* VFO_A or VFO_CURR: already selected, nothing to send */
}

/*
 * hamgeek_usdx_set_ptt - set_ptt wrapper
 *
 * PTT ON:  TX; / TX0; / TX1; — firmware sends NO serial reply.
 * PTT OFF: RX;               — firmware replies with "RX0;".
 *
 * kenwood_set_ptt calls kenwood_transaction with datasize=0 (no reply read).
 * For PTT OFF the stray "RX0;" would accumulate in the serial buffer and
 * corrupt the next response, so we read it explicitly here.
 */
static int hamgeek_usdx_set_ptt(RIG *rig, vfo_t vfo, ptt_t ptt)
{
    char buf[8];
    int retval;

    rig_debug(RIG_DEBUG_VERBOSE, "%s called ptt=%d\n", __func__, ptt);

    if (ptt == RIG_PTT_OFF)
    {
        /* RX; — firmware replies "RX0;" — read and discard to keep buffer clean */
        retval = kenwood_transaction(rig, "RX", buf, sizeof(buf));
    }
    else
    {
        /* TX / TX0 (mic) / TX1 (data) — no reply from firmware */
        const char *cmd = (ptt == RIG_PTT_ON_MIC)  ? "TX0" :
                          (ptt == RIG_PTT_ON_DATA) ? "TX1" : "TX";
        retval = kenwood_transaction(rig, cmd, NULL, 0);
    }

    return retval;
}

/*
 * hamgeek_usdx_get_ptt - get_ptt wrapper
 *
 * TQ; is not supported by the uSDX firmware (returns '?;').  The IF; field
 * at position [29] carries the mode, not TX state; the TX status byte is
 * always '0' regardless of PTT.  Return RIG_PTT_OFF unconditionally to avoid
 * protocol errors.
 */
static int hamgeek_usdx_get_ptt(RIG *rig, vfo_t vfo, ptt_t *ptt)
{
    rig_debug(RIG_DEBUG_VERBOSE, "%s called\n", __func__);

    *ptt = RIG_PTT_OFF;  /* TQ; not supported by uSDX firmware */
    return RIG_OK;
}

/* Hamgeek uSDX rig capabilities */
struct rig_caps hamgeek_usdx_caps =
{
    RIG_MODEL(RIG_MODEL_HAMGEEK_USDX),
    .model_name =       "uSDX",
    .mfg_name =         "Hamgeek",
    .version =          BACKEND_VER ".0",
    .copyright =        "LGPL",
    .status =           RIG_STATUS_ALPHA,
    .rig_type =         RIG_TYPE_TRANSCEIVER,
    .ptt_type =         RIG_PTT_RIG,
    .dcd_type =         RIG_DCD_RIG,
    .port_type =        RIG_PORT_SERIAL,
    .serial_rate_min =  9600,
    .serial_rate_max =  115200,
    .serial_data_bits = 8,
    .serial_stop_bits = 1,
    .serial_parity =    RIG_PARITY_NONE,
    .serial_handshake = RIG_HANDSHAKE_NONE,
    .write_delay =      0,
    .post_write_delay = 50,
    .timeout =          2000,
    .retry =            3,

    .preamp =           { RIG_DBLST_END },
    .attenuator =       { RIG_DBLST_END },
    .max_rit =          Hz(0),
    .max_xit =          Hz(0),
    .max_ifshift =      Hz(0),
    .targetable_vfo =   RIG_TARGETABLE_FREQ,
    .transceive =       RIG_TRN_OFF,
    .agc_level_count =  0,

    .rx_range_list1 = {
        {kHz(1800), MHz(30), USDX_ALL_MODES, -1, -1, RIG_VFO_A},
        RIG_FRNG_END,
    },
    .tx_range_list1 = {
        {kHz(1810), kHz(1850),   USDX_TX_MODES, 100, 5000, RIG_VFO_A},
        {kHz(3500), kHz(3800),   USDX_TX_MODES, 100, 5000, RIG_VFO_A},
        {kHz(5351), kHz(5367),   USDX_TX_MODES, 100, 5000, RIG_VFO_A},
        {MHz(7),    kHz(7200),   USDX_TX_MODES, 100, 5000, RIG_VFO_A},
        {kHz(10100), kHz(10150), USDX_TX_MODES, 100, 5000, RIG_VFO_A},
        {MHz(14),   kHz(14350),  USDX_TX_MODES, 100, 5000, RIG_VFO_A},
        {kHz(18068), kHz(18168), USDX_TX_MODES, 100, 5000, RIG_VFO_A},
        {MHz(21),   kHz(21450),  USDX_TX_MODES, 100, 5000, RIG_VFO_A},
        {kHz(24890), kHz(24990), USDX_TX_MODES, 100, 5000, RIG_VFO_A},
        {MHz(28),   kHz(29700),  USDX_TX_MODES, 100, 5000, RIG_VFO_A},
        RIG_FRNG_END,
    },
    .rx_range_list2 = {
        {kHz(1800), MHz(30), USDX_ALL_MODES, -1, -1, RIG_VFO_A},
        RIG_FRNG_END,
    },
    .tx_range_list2 = {
        {kHz(1800),  MHz(2) - 1,   USDX_TX_MODES, 100, 5000, RIG_VFO_A},
        {kHz(3500),  MHz(4) - 1,   USDX_TX_MODES, 100, 5000, RIG_VFO_A},
        {kHz(5351),  kHz(5367),    USDX_TX_MODES, 100, 5000, RIG_VFO_A},
        {MHz(7),     kHz(7300),    USDX_TX_MODES, 100, 5000, RIG_VFO_A},
        {kHz(10100), kHz(10150),   USDX_TX_MODES, 100, 5000, RIG_VFO_A},
        {MHz(14),    kHz(14350),   USDX_TX_MODES, 100, 5000, RIG_VFO_A},
        {kHz(18068), kHz(18168),   USDX_TX_MODES, 100, 5000, RIG_VFO_A},
        {MHz(21),    kHz(21450),   USDX_TX_MODES, 100, 5000, RIG_VFO_A},
        {kHz(24890), kHz(24990),   USDX_TX_MODES, 100, 5000, RIG_VFO_A},
        {MHz(28),    kHz(29700),   USDX_TX_MODES, 100, 5000, RIG_VFO_A},
        RIG_FRNG_END,
    },
    .tuning_steps = {
        {USDX_ALL_MODES, Hz(10)},
        {USDX_ALL_MODES, Hz(100)},
        {USDX_ALL_MODES, kHz(1)},
        {USDX_ALL_MODES, kHz(5)},
        {USDX_ALL_MODES, kHz(10)},
        {USDX_ALL_MODES, kHz(100)},
        {USDX_ALL_MODES, MHz(1)},
        {USDX_ALL_MODES, 0},          /* any step */
        RIG_TS_END,
    },
    .filters = {
        {RIG_MODE_USB | RIG_MODE_LSB | RIG_MODE_PKTUSB | RIG_MODE_PKTLSB, kHz(2.4)},
        {RIG_MODE_CW  | RIG_MODE_CWR,  Hz(500)},
        {RIG_MODE_AM,                  kHz(6)},
        {RIG_MODE_FM,                  kHz(12)},
        RIG_FLT_END,
    },

    .priv =         (void *)&hamgeek_usdx_priv_caps,
    .rig_init =     kenwood_init,
    .rig_open =     hamgeek_usdx_open,
    .rig_cleanup =  kenwood_cleanup,

    .set_freq =     kenwood_set_freq,
    .get_freq =     kenwood_get_freq,

    .set_mode =     hamgeek_usdx_set_mode,
    .get_mode =     kenwood_get_mode,

    .set_vfo =      hamgeek_usdx_set_vfo,
    .get_vfo =      kenwood_get_vfo_if,

    .set_ptt =      hamgeek_usdx_set_ptt,
    .get_ptt =      hamgeek_usdx_get_ptt,

    .get_info =     kenwood_get_info,

    .hamlib_check_rig_caps = HAMLIB_CHECK_RIG_CAPS
};
