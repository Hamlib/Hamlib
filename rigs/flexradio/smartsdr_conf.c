/*
 *  Hamlib SmartSDR configuration tokens
 *  Copyright (c) 2026 by Mikael Nousiainen OH3BHX
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

/* The settings a SmartSDR rig accepts: which slice it drives, how it reaches */
/* the radio's UDP endpoint, and what it does when the radio goes away.       */
/* Each one is declared once here and read straight into the private data.    */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

#include "hamlib/rig.h"
#include "hamlib/rig_state.h"
#include "smartsdr_priv.h"
#include "misc.h"
#include "smartsdr_conf.h"


const struct confparams smartsdr_config_params[] =
{
    {
        TOK_NAT_TRAVERSAL, "nat_traversal", "NAT traversal",
        "Radio learns the client's NAT-translated UDP address instead of being "
        "told a local port. Needed only when reaching the radio through manual "
        "port forwarding; 0 for a LAN connection.",
        "0", RIG_CONF_CHECKBUTTON, {}
    },
    {
        TOK_AUTO_RECONNECT, "auto_reconnect", "Automatic reconnect",
        "Rebuild the control session after the radio stops answering, retrying "
        "with a backoff from 1 to 30 seconds. Open streams are not restored: "
        "their stream IDs die with the session, so an application must reopen "
        "them.",
        "0", RIG_CONF_CHECKBUTTON, {}
    },
    {
        TOK_TX_AUDIO_SOURCE, "tx_audio_source", "Transmit audio source",
        "What the radio modulates from: mic, acc, pc or dax. The radio holds "
        "the input selection and the DAX flag separately, and they can "
        "contradict each other, so both are written together. Opening a "
        "transmit stream selects dax for as long as it is open, since "
        "streamed audio is discarded otherwise.",
        "mic", RIG_CONF_COMBO, { .c = { .combostr = { "mic", "acc", "pc", "dax", NULL } } }
    },
    {
        TOK_SLICE_MISSING, "slice_missing", "When the slice does not exist",
        "What to do when the configured slice is not on the radio: create "
        "one and remove it again on close, or fail rig_open. The radio "
        "chooses the index of a new slice, so creating one can only satisfy "
        "a rig that did not name a particular slice.",
        "create", RIG_CONF_COMBO, { .c = { .combostr = { "create", "fail", NULL } } }
    },
    {
        TOK_SPECTRUM, "spectrum", "Spectrum scope",
        "Deliver panadapter FFT data as Hamlib spectrum lines. Reads the "
        "slice's own panadapter when it has one, and otherwise creates one "
        "on the radio and removes it again on close.",
        "0", RIG_CONF_CHECKBUTTON, {}
    },
    {
        TOK_SLICE, "slice", "Slice",
        "Slice letter A-H this rig controls. A is the default, and every "
        "other slice is reached by naming it here.",
        "A", RIG_CONF_STRING, {}
    },
    {
        TOK_SPLIT_SLICE, "split_slice", "Split transmit slice",
        "Slice letter A-H to treat as the transmit slice for split, instead of "
        "whichever slice the radio has marked as transmitting. Empty follows "
        "the radio.",
        "", RIG_CONF_STRING, {}
    },
    {
        TOK_STATUS_TIMEOUT, "status_timeout", "Status value timeout",
        "Milliseconds after which a value the radio reported is treated as "
        "stale and reads return unavailable. 0 keeps values indefinitely, "
        "which suits this radio because it pushes status changes.",
        "0", RIG_CONF_NUMERIC, {.n = {0, 3600000, 1}}
    },
    {
        TOK_LIVENESS_TIMEOUT, "liveness_timeout", "Silence before the radio is lost",
        "Milliseconds of complete silence from the radio after which the "
        "session is declared lost. The keepalive pings at half this interval, "
        "up to ten seconds, so a link that is merely slow still proves itself. "
        "Raise it on a link that stalls for seconds at a time.",
        "20000", RIG_CONF_NUMERIC, {.n = {500, 3600000, 1}}
    },
    {
        TOK_VITA_PORT, "vita_port", "Radio's VITA-49 UDP port",
        "UDP port the radio receives transmit data on, and the port the "
        "client prefers for its own socket. Every SmartSDR radio uses 4991, "
        "so this is only worth changing where something else on the host "
        "already holds that port -- a second client, or a test harness "
        "standing in for the radio.",
        "4991", RIG_CONF_NUMERIC, {.n = {1, 65535, 1}}
    },
    { RIG_CONF_END, NULL, }
};


/* Slice letter A-H to its number, or -1 when it names no slice. */
static int smartsdr_slice_letter_to_num(const char *val)
{
    int n;

    if (val == NULL || val[0] == '\0')
    {
        return -1;
    }

    n = toupper((unsigned char)val[0]) - 'A';

    return (n >= 0 && n < SMARTSDR_MAX_SLICES) ? n : -1;
}


int smartsdr_set_conf(RIG *rig, hamlib_token_t token, const char *val)
{
    struct smartsdr_priv_data *priv = (struct smartsdr_priv_data *)STATE(rig)->priv;

    switch (token)
    {
    case TOK_NAT_TRAVERSAL:
        priv->nat_traversal = (uint8_t)(atoi(val) != 0);
        return RIG_OK;

    case TOK_STATUS_TIMEOUT:
    {
        long ms = val != NULL ? strtol(val, NULL, 10) : -1;

        /* The range the parameter advertises is only meaningful if it is
         * enforced. */
        if (ms < 0 || ms > SMARTSDR_STATUS_TIMEOUT_MAX_MS)
        {
            rig_debug(RIG_DEBUG_ERR,
                      "%s: status_timeout must be 0..%d ms, got \"%s\"\n",
                      __func__, SMARTSDR_STATUS_TIMEOUT_MAX_MS,
                      val ? val : "");
            return -RIG_EINVAL;
        }

        priv->status_timeout_ms = (int)ms;
        return RIG_OK;
    }

    case TOK_LIVENESS_TIMEOUT:
    {
        long ms = val != NULL ? strtol(val, NULL, 10) : -1;

        if (ms < SMARTSDR_LIVENESS_TIMEOUT_MIN_MS
                || ms > SMARTSDR_LIVENESS_TIMEOUT_MAX_MS)
        {
            rig_debug(RIG_DEBUG_ERR,
                      "%s: liveness_timeout must be %d..%d ms, got \"%s\"\n",
                      __func__, SMARTSDR_LIVENESS_TIMEOUT_MIN_MS,
                      SMARTSDR_LIVENESS_TIMEOUT_MAX_MS, val ? val : "");
            return -RIG_EINVAL;
        }

        priv->liveness_timeout_ms = (int)ms;
        return RIG_OK;
    }

    case TOK_VITA_PORT:
    {
        long port = val != NULL ? strtol(val, NULL, 10) : -1;

        if (port < 1 || port > 65535)
        {
            rig_debug(RIG_DEBUG_ERR,
                      "%s: vita_port must be 1..65535, got \"%s\"\n",
                      __func__, val ? val : "");
            return -RIG_EINVAL;
        }

        priv->vita_port = (int)port;
        return RIG_OK;
    }

    case TOK_AUTO_RECONNECT:
        priv->auto_reconnect = (uint8_t)(atoi(val) != 0);
        return RIG_OK;

    case TOK_SPECTRUM:
        priv->spectrum_enabled = (uint8_t)(atoi(val) != 0);
        return RIG_OK;

    case TOK_TX_AUDIO_SOURCE:
        if (val == NULL
                || (strcmp(val, "mic") != 0 && strcmp(val, "acc") != 0
                    && strcmp(val, "pc") != 0 && strcmp(val, "dax") != 0))
        {
            rig_debug(RIG_DEBUG_ERR,
                      "%s: tx_audio_source must be mic, acc, pc or dax, "
                      "got \"%s\"\n", __func__, val ? val : "");
            return -RIG_EINVAL;
        }

        strncpy(priv->tx_audio_source, val, sizeof(priv->tx_audio_source) - 1);
        priv->tx_audio_source[sizeof(priv->tx_audio_source) - 1] = '\0';
        return RIG_OK;

    case TOK_SLICE_MISSING:
        if (val != NULL && strcmp(val, "fail") == 0)
        {
            priv->slice_missing_fails = 1;
            return RIG_OK;
        }

        if (val != NULL && strcmp(val, "create") == 0)
        {
            priv->slice_missing_fails = 0;
            return RIG_OK;
        }

        rig_debug(RIG_DEBUG_ERR,
                  "%s: slice_missing must be create or fail, got \"%s\"\n",
                  __func__, val ? val : "");
        return -RIG_EINVAL;

    case TOK_SLICE:
    {
        int n = smartsdr_slice_letter_to_num(val);

        if (n < 0)
        {
            rig_debug(RIG_DEBUG_ERR, "%s: slice must be A-H, got \"%s\"\n",
                      __func__, val ? val : "");
            return -RIG_EINVAL;
        }

        priv->slicenum = n;
        priv->slice_explicit = 1;
        return RIG_OK;
    }

    case TOK_SPLIT_SLICE:
        if (val == NULL || val[0] == '\0')
        {
            priv->split_slice_override = -1;
            return RIG_OK;
        }

        {
            int n = smartsdr_slice_letter_to_num(val);

            if (n < 0)
            {
                rig_debug(RIG_DEBUG_ERR, "%s: split_slice must be A-H\n",
                          __func__);
                return -RIG_EINVAL;
            }

            if (n == priv->slicenum)
            {
                rig_debug(RIG_DEBUG_ERR,
                          "%s: split_slice must differ from the rig's own "
                          "slice\n", __func__);
                return -RIG_EINVAL;
            }

            priv->split_slice_override = n;
        }

        return RIG_OK;

    default:
        return -RIG_EINVAL;
    }
}


int smartsdr_get_conf2(RIG *rig, hamlib_token_t token, char *val,
                       int val_len)
{
    struct smartsdr_priv_data *priv = (struct smartsdr_priv_data *)STATE(rig)->priv;

    switch (token)
    {
    case TOK_NAT_TRAVERSAL:
        SNPRINTF(val, val_len, "%d", priv->nat_traversal ? 1 : 0);
        return RIG_OK;

    case TOK_STATUS_TIMEOUT:
        SNPRINTF(val, val_len, "%d", priv->status_timeout_ms);
        return RIG_OK;

    case TOK_LIVENESS_TIMEOUT:
        SNPRINTF(val, val_len, "%d", priv->liveness_timeout_ms);
        return RIG_OK;

    case TOK_VITA_PORT:
        SNPRINTF(val, val_len, "%d", priv->vita_port);
        return RIG_OK;

    case TOK_AUTO_RECONNECT:
        SNPRINTF(val, val_len, "%d", priv->auto_reconnect ? 1 : 0);
        return RIG_OK;

    case TOK_SPECTRUM:
        SNPRINTF(val, val_len, "%d", priv->spectrum_enabled ? 1 : 0);
        return RIG_OK;

    case TOK_TX_AUDIO_SOURCE:
        SNPRINTF(val, val_len, "%s", priv->tx_audio_source[0] ? priv->tx_audio_source : "mic");
        return RIG_OK;

    case TOK_SLICE_MISSING:
        SNPRINTF(val, val_len, "%s", priv->slice_missing_fails ? "fail" : "create");
        return RIG_OK;

    case TOK_SLICE:
        SNPRINTF(val, val_len, "%c", 'A' + priv->slicenum);
        return RIG_OK;

    case TOK_SPLIT_SLICE:
        if (priv->split_slice_override < 0)
        {
            val[0] = '\0';
        }
        else
        {
            SNPRINTF(val, val_len, "%c", 'A' + priv->split_slice_override);
        }

        return RIG_OK;

    default:
        return -RIG_EINVAL;
    }
}
