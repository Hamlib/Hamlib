/*
 *  Hamlib Icom network backend - configuration tokens
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

/* Shared configuration tokens for the Icom network (RS-BA1/LAN) backends. */
/* Credentials, radio selection, audio codecs, rates and latencies. */

#include "hamlib/config.h"

#include <string.h>
#include <stdlib.h>
#include <stdio.h>

#include "hamlib/rig.h"
#include "icom.h"
#include "icom_defs.h"
#include "network_proto.h"
#include "network_conf.h"

/* Codec/sample-rate combo choices map an index (the value passed for a COMBO
 * conf token) to the protocol wire value. The combo label lists below must stay
 * in the same order as these tables. */
static const uint8_t net_rx_codec_wire[] =
{
    ICOM_NETWORK_CODEC_LPCM16,  ICOM_NETWORK_CODEC_LPCM8,
    ICOM_NETWORK_CODEC_PCMU,    ICOM_NETWORK_CODEC_LPCM16S,
    ICOM_NETWORK_CODEC_PCMUS,   ICOM_NETWORK_CODEC_LPCM8S,
    ICOM_NETWORK_CODEC_ADPCM
};

static const uint8_t net_tx_codec_wire[] =
{
    ICOM_NETWORK_CODEC_LPCM16, ICOM_NETWORK_CODEC_LPCM8,
    ICOM_NETWORK_CODEC_PCMU,   ICOM_NETWORK_CODEC_ADPCM
};

static const int net_sample_rate_hz[] = ICOM_NETWORK_SUPPORTED_RATES;

/* confparams carries its default as a string; derive it from the numeric
 * constant so the declared default and the code default cannot drift apart. */
#define ICOM_NETWORK_STR_(x) #x
#define ICOM_NETWORK_STR(x)  ICOM_NETWORK_STR_(x)

/* The combo tokens (net_rx_codec, net_tx_codec, net_sample_rate) carry an
 * INDEX into their choice list, not the value itself: net_sample_rate=0 selects
 * the first rate, and net_sample_rate=48000 is not a rate but an index far
 * outside the list. That is the one genuinely surprising thing in this
 * interface, so an index outside the list is refused rather than substituted:
 * running at a rate the caller did not ask for, silently, is worse than
 * refusing the setting.
 *
 * value_to_index is the inverse for get_conf and cannot fail -- it is only ever
 * given a value this file put there. */
static int index_to_value(const void *table, int is_byte, size_t n, int index,
                          int *value)
{
    if (index < 0 || (size_t)index >= n)
    {
        return -RIG_EINVAL;
    }

    *value = is_byte ? ((const uint8_t *)table)[index]
             : ((const int *)table)[index];
    return RIG_OK;
}

static int value_to_index(const void *table, int is_byte, size_t n, int val)
{
    size_t i;

    for (i = 0; i < n; i++)
    {
        int v = is_byte ? ((const uint8_t *)table)[i] : ((const int *)table)[i];

        if (v == val) { return (int)i; }
    }

    return 0;
}

const struct confparams icom_network_config_params[] =
{
    {
        TOK_NET_USERNAME, "net_username", "Network username",
        "Username for the Icom network (RS-BA1/LAN) login", "",
        RIG_CONF_STRING, {}
    },
    {
        TOK_NET_PASSWORD, "net_password", "Network password",
        "Password for the Icom network (RS-BA1/LAN) login", "",
        RIG_CONF_STRING, {}
    },
    {
        TOK_NET_CONTROL_PORT, "net_control_port", "Network control port",
        "UDP control port (0 = default 50001)", "0",
        RIG_CONF_NUMERIC, {.n = {0, 65535, 1}}
    },
    {
        TOK_NET_IQ_MODE, "net_iq_mode", "I/Q stream mode",
        "Present the stereo RX audio as an I/Q stream (radio must be set to "
        "I/Q output)", "0", RIG_CONF_CHECKBUTTON, {}
    },
    {
        TOK_NET_RX_CODEC, "net_rx_codec", "RX audio codec",
        "Codec for audio received from the radio", "LPCM 1ch 16bit",
        RIG_CONF_COMBO, {
            .c = {
                .combostr = {
                    "LPCM 1ch 16bit", "LPCM 1ch 8bit", "uLaw 1ch 8bit",
                    "LPCM 2ch 16bit", "uLaw 2ch 8bit", "LPCM 2ch 8bit",
                    "ADPCM 1ch", NULL
                }
            }
        }
    },
    {
        TOK_NET_TX_CODEC, "net_tx_codec", "TX audio codec",
        "Codec for audio sent to the radio", "LPCM 1ch 16bit",
        RIG_CONF_COMBO, {
            .c = {
                .combostr = {
                    "LPCM 1ch 16bit", "LPCM 1ch 8bit", "uLaw 1ch 8bit",
                    "ADPCM 1ch", NULL
                }
            }
        }
    },
    {
        TOK_NET_SAMPLE_RATE, "net_sample_rate", "Audio sample rate",
        "Network audio sample rate (RX and TX share one rate)", "48000",
        RIG_CONF_COMBO, {
            .c = {
                .combostr = {
                    /* Keep in step with ICOM_NETWORK_SUPPORTED_RATES, which
                     * net_sample_rate_hz[] indexes: this list is what the
                     * index means to a user, so a rate missing here is a rate
                     * they cannot find. */
                    "48000", "24000", "16000", "12000", "8000", NULL
                }
            }
        }
    },
    {
        TOK_NET_RX_LATENCY, "net_rx_latency", "RX audio latency (ms)",
        "Receive audio jitter-buffer length in milliseconds",
        ICOM_NETWORK_STR(ICOM_NETWORK_DEFAULT_LATENCY_MS),
        RIG_CONF_NUMERIC, {.n = {10, 1000, 10}}
    },
    {
        TOK_NET_TX_LATENCY, "net_tx_latency", "TX audio latency (ms)",
        "Transmit audio jitter-buffer length in milliseconds",
        ICOM_NETWORK_STR(ICOM_NETWORK_DEFAULT_LATENCY_MS),
        RIG_CONF_NUMERIC, {.n = {10, 1000, 10}}
    },
    {
        TOK_NET_TX_ENABLE, "net_tx_enable", "Enable TX audio",
        "Reserve a TX audio path on TX-capable radios (0 = receive only)", "1",
        RIG_CONF_CHECKBUTTON, {}
    },
    {
        TOK_NET_TX_FRAME_MS, "net_tx_frame_ms", "TX audio frame length",
        "TX audio wire-frame duration in milliseconds", "20",
        RIG_CONF_NUMERIC, { .n = { 5, 100, 1 } }
    },
    {
        TOK_NET_RADIO_INDEX, "net_radio_index", "Radio index",
        "Radio to select from the server's advertised list (-1 = select by name)",
        "-1",
        RIG_CONF_NUMERIC, { .n = { -1, ICOM_NETWORK_MAX_RADIOS - 1, 1 } }
    },
    {
        TOK_NET_RADIO_NAME, "net_radio_name", "Radio name",
        "Radio name to select from the server's advertised list "
        "(empty = this model's own name)", "",
        RIG_CONF_STRING
    },
    {
        TOK_NET_LIVENESS_TIMEOUT, "net_liveness_timeout", "Liveness timeout (ms)",
        "Silence from the radio before the session is declared lost "
        "(0 = never, losses surface as command timeouts; minimum 1000)",
        ICOM_NETWORK_STR(ICOM_NETWORK_DEFAULT_LIVENESS_MS),
        RIG_CONF_NUMERIC, { .n = { 0, 60000, 100 } }
    },
    {
        TOK_NET_AUTO_RECONNECT, "net_auto_reconnect", "Auto reconnect",
        "Re-establish a lost session in the background. Control recovers; open "
        "streams are ended and must be reopened", "0",
        RIG_CONF_CHECKBUTTON, {}
    },
    { RIG_CONF_END, NULL, }
};

/* The confparams table declares a range for each numeric token; rig_set_conf
 * does not police it, so the backend must. Returns RIG_OK or -RIG_EINVAL. */
static int check_range(const char *name, int value, int min, int max)
{
    if (value < min || value > max)
    {
        rig_debug(RIG_DEBUG_ERR, "%s: %s must be %d..%d, got %d\n",
                  __func__, name, min, max, value);
        return -RIG_EINVAL;
    }

    return RIG_OK;
}

int icom_network_set_conf(RIG *rig, hamlib_token_t token, const char *val)
{
    struct icom_priv_data *priv = (struct icom_priv_data *)STATE(rig)->priv;

    switch (token)
    {
    case TOK_NET_USERNAME:
        if (strlen(val) > ICOM_NETWORK_PASSCODE_MAX)
        {
            rig_debug(RIG_DEBUG_ERR, "%s: net_username is limited to %d characters\n",
                      __func__, ICOM_NETWORK_PASSCODE_MAX);
            return -RIG_EINVAL;
        }

        strncpy(priv->net_username, val, sizeof(priv->net_username) - 1);
        return RIG_OK;

    case TOK_NET_PASSWORD:
        if (strlen(val) > ICOM_NETWORK_PASSCODE_MAX)
        {
            /* Silently truncating here produced a login rejection that reads
             * as a wrong password, so refuse it up front instead. */
            rig_debug(RIG_DEBUG_ERR, "%s: net_password is limited to %d characters\n",
                      __func__, ICOM_NETWORK_PASSCODE_MAX);
            return -RIG_EINVAL;
        }

        strncpy(priv->net_password, val, sizeof(priv->net_password) - 1);
        return RIG_OK;

    case TOK_NET_CONTROL_PORT:
    {
        int port = atoi(val);
        int ret = check_range("net_control_port", port, 0, 65535);

        if (ret != RIG_OK) { return ret; }

        priv->net_control_port = port;
        return RIG_OK;
    }

    case TOK_NET_IQ_MODE:
        priv->net_iq_mode = atoi(val) ? 1 : 0;
        return RIG_OK;

    case TOK_NET_RX_CODEC:
    {
        int codec;
        int ret = index_to_value(net_rx_codec_wire, 1,
                                 sizeof(net_rx_codec_wire), atoi(val), &codec);

        if (ret != RIG_OK)
        {
            rig_debug(RIG_DEBUG_ERR,
                      "%s: net_rx_codec takes a choice index 0..%d, not %s\n",
                      __func__, (int)sizeof(net_rx_codec_wire) - 1, val);
            return ret;
        }

        if (codec == ICOM_NETWORK_CODEC_ADPCM)
        {
            rig_debug(RIG_DEBUG_ERR,
                      "%s: the ADPCM audio codec is not supported\n", __func__);
            return -RIG_ECONF;
        }

        priv->net_rx_codec = codec;
        return RIG_OK;
    }

    case TOK_NET_TX_CODEC:
    {
        int codec;
        int ret = index_to_value(net_tx_codec_wire, 1,
                                 sizeof(net_tx_codec_wire), atoi(val), &codec);

        if (ret != RIG_OK)
        {
            rig_debug(RIG_DEBUG_ERR,
                      "%s: net_tx_codec takes a choice index 0..%d, not %s\n",
                      __func__, (int)sizeof(net_tx_codec_wire) - 1, val);
            return ret;
        }

        if (codec == ICOM_NETWORK_CODEC_ADPCM)
        {
            rig_debug(RIG_DEBUG_ERR,
                      "%s: the ADPCM audio codec is not supported\n", __func__);
            return -RIG_ECONF;
        }

        priv->net_tx_codec = codec;
        return RIG_OK;
    }

    case TOK_NET_SAMPLE_RATE:
    {
        const size_t rates = sizeof(net_sample_rate_hz)
                             / sizeof(net_sample_rate_hz[0]);
        int rate;
        int ret = index_to_value(net_sample_rate_hz, 0, rates, atoi(val), &rate);

        if (ret != RIG_OK)
        {
            /* The most likely mistake is passing the rate in Hz, so name it. */
            rig_debug(RIG_DEBUG_ERR,
                      "%s: net_sample_rate takes a choice index 0..%d, not %s "
                      "(index 0 is %d Hz)\n", __func__, (int)rates - 1, val,
                      net_sample_rate_hz[0]);
            return ret;
        }

        priv->net_sample_rate = rate;
        return RIG_OK;
    }

    case TOK_NET_RX_LATENCY:
    {
        int ms = atoi(val);
        int ret = check_range("net_rx_latency", ms, 10, 1000);

        if (ret != RIG_OK) { return ret; }

        priv->net_rx_latency = ms;
        return RIG_OK;
    }

    case TOK_NET_TX_LATENCY:
    {
        int ms = atoi(val);
        int ret = check_range("net_tx_latency", ms, 10, 1000);

        if (ret != RIG_OK) { return ret; }

        priv->net_tx_latency = ms;
        return RIG_OK;
    }

    case TOK_NET_TX_ENABLE:
        priv->net_tx_enable = atoi(val) ? 1 : 0;
        return RIG_OK;

    case TOK_NET_RADIO_INDEX:
    {
        int index = atoi(val);

        if (index < -1 || index >= ICOM_NETWORK_MAX_RADIOS)
        {
            return -RIG_EINVAL;
        }

        priv->net_radio_index = index;
        return RIG_OK;
    }

    case TOK_NET_RADIO_NAME:
        SNPRINTF(priv->net_radio_name, sizeof(priv->net_radio_name), "%s", val);
        return RIG_OK;

    case TOK_NET_LIVENESS_TIMEOUT:
    {
        int ms = atoi(val);
        int ret = check_range("net_liveness_timeout", ms, 0, 60000);

        if (ret != RIG_OK) { return ret; }

        /* 0 disables the check; anything else has to clear the floor, or the
         * session would be declared lost by ordinary keepalive jitter. */
        if (ms != 0 && ms < ICOM_NETWORK_MIN_LIVENESS_MS)
        {
            rig_debug(RIG_DEBUG_ERR,
                      "%s: net_liveness_timeout must be 0 or at least %d ms\n",
                      __func__, ICOM_NETWORK_MIN_LIVENESS_MS);
            return -RIG_EINVAL;
        }

        priv->net_liveness_timeout = ms;
        return RIG_OK;
    }

    case TOK_NET_AUTO_RECONNECT:
        priv->net_auto_reconnect = atoi(val) ? 1 : 0;
        return RIG_OK;

    case TOK_NET_TX_FRAME_MS:
    {
        int frame_ms = atoi(val);

        if (frame_ms < 5 || frame_ms > 100)
        {
            rig_debug(RIG_DEBUG_ERR,
                      "%s: net_tx_frame_ms %d out of range 5..100\n",
                      __func__, frame_ms);
            return -RIG_EINVAL;
        }

        priv->net_tx_frame_ms = frame_ms;
        return RIG_OK;
    }

    default:
        return icom_set_conf(rig, token, val);
    }
}

int icom_network_get_conf(RIG *rig, hamlib_token_t token, char *val)
{
    struct icom_priv_data *priv = (struct icom_priv_data *)STATE(rig)->priv;

    switch (token)
    {
    case TOK_NET_USERNAME:
        strcpy(val, priv->net_username);
        return RIG_OK;

    case TOK_NET_PASSWORD:
        strcpy(val, priv->net_password);
        return RIG_OK;

    case TOK_NET_CONTROL_PORT:
        sprintf(val, "%d", priv->net_control_port);
        return RIG_OK;

    case TOK_NET_IQ_MODE:
        sprintf(val, "%d", priv->net_iq_mode);
        return RIG_OK;

    case TOK_NET_RX_CODEC:
        sprintf(val, "%d", value_to_index(net_rx_codec_wire, 1,
                                          sizeof(net_rx_codec_wire),
                                          priv->net_rx_codec));
        return RIG_OK;

    case TOK_NET_TX_CODEC:
        sprintf(val, "%d", value_to_index(net_tx_codec_wire, 1,
                                          sizeof(net_tx_codec_wire),
                                          priv->net_tx_codec));
        return RIG_OK;

    case TOK_NET_SAMPLE_RATE:
        sprintf(val, "%d", value_to_index(net_sample_rate_hz, 0,
                                          sizeof(net_sample_rate_hz)
                                          / sizeof(net_sample_rate_hz[0]),
                                          priv->net_sample_rate));
        return RIG_OK;

    case TOK_NET_RX_LATENCY:
        sprintf(val, "%d", priv->net_rx_latency);
        return RIG_OK;

    case TOK_NET_TX_LATENCY:
        sprintf(val, "%d", priv->net_tx_latency);
        return RIG_OK;

    case TOK_NET_TX_ENABLE:
        sprintf(val, "%d", priv->net_tx_enable);
        return RIG_OK;

    case TOK_NET_TX_FRAME_MS:
        sprintf(val, "%d", priv->net_tx_frame_ms);
        return RIG_OK;

    case TOK_NET_RADIO_INDEX:
        sprintf(val, "%d", priv->net_radio_index);
        return RIG_OK;

    case TOK_NET_RADIO_NAME:
        strcpy(val, priv->net_radio_name);
        return RIG_OK;

    case TOK_NET_LIVENESS_TIMEOUT:
        sprintf(val, "%d", priv->net_liveness_timeout);
        return RIG_OK;

    case TOK_NET_AUTO_RECONNECT:
        sprintf(val, "%d", priv->net_auto_reconnect);
        return RIG_OK;

    default:
        return icom_get_conf(rig, token, val);
    }
}
