/*
 *  Hamlib Icom network backend config tests
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

/* Unit tests for the Icom network backend config tokens. */
/* Covers defaults, COMBO index mapping, ranges, and unsupported-codec rejection. */

#ifdef HAVE_CONFIG_H
#include "hamlib/config.h"
#endif

#include "acutest.h"

#include <hamlib/rig.h>
#include <hamlib/riglist.h>

#include "icom.h"
#include "network_proto.h"
#include "icom_defs.h"

#include <string.h>

/* Index of ADPCM in the codec COMBO tables (last entry in both). */
#define RX_CODEC_IDX_ADPCM 6
#define TX_CODEC_IDX_ADPCM 3

static RIG *conf_rig_init(void)
{
    RIG *rig = rig_init(RIG_MODEL_IC7610NET);

    TEST_ASSERT(rig != NULL);
    return rig;
}

void test_defaults_after_init(void)
{
    RIG *rig = conf_rig_init();
    char val[64];

    /* TX audio path is reserved by default on a TX-capable model */
    TEST_CHECK(rig_get_conf2(rig, TOK_NET_TX_ENABLE, val, sizeof(val)) == RIG_OK);
    TEST_CHECK(strcmp(val, "1") == 0);
    TEST_MSG("net_tx_enable=%s", val);

    /* wire frame duration defaults to the radio's native 20 ms framing */
    TEST_CHECK(rig_get_conf2(rig, TOK_NET_TX_FRAME_MS, val, sizeof(val)) == RIG_OK);
    TEST_CHECK(strcmp(val, "20") == 0);
    TEST_MSG("net_tx_frame_ms=%s", val);

    /* no radio index selected: the radio is picked by name instead */
    TEST_CHECK(rig_get_conf2(rig, TOK_NET_RADIO_INDEX, val, sizeof(val)) == RIG_OK);
    TEST_CHECK(strcmp(val, "-1") == 0);
    TEST_MSG("net_radio_index=%s", val);

    /* no explicit radio name: the model's own name is used */
    TEST_CHECK(rig_get_conf2(rig, TOK_NET_RADIO_NAME, val, sizeof(val)) == RIG_OK);
    TEST_CHECK(strcmp(val, "") == 0);
    TEST_MSG("net_radio_name='%s'", val);

    rig_cleanup(rig);
}

void test_codec_index_roundtrip(void)
{
    RIG *rig = conf_rig_init();
    char val[64];

    /* RX index 3 = LPCM16S; TX index 2 = PCMU */
    TEST_CHECK(rig_set_conf(rig, TOK_NET_RX_CODEC, "3") == RIG_OK);
    TEST_CHECK(rig_get_conf2(rig, TOK_NET_RX_CODEC, val, sizeof(val)) == RIG_OK);
    TEST_CHECK(strcmp(val, "3") == 0);

    TEST_CHECK(rig_set_conf(rig, TOK_NET_TX_CODEC, "2") == RIG_OK);
    TEST_CHECK(rig_get_conf2(rig, TOK_NET_TX_CODEC, val, sizeof(val)) == RIG_OK);
    TEST_CHECK(strcmp(val, "2") == 0);

    rig_cleanup(rig);
}

void test_adpcm_rejected(void)
{
    RIG *rig = conf_rig_init();
    char index[8];

    snprintf(index, sizeof(index), "%d", RX_CODEC_IDX_ADPCM);
    TEST_CHECK(rig_set_conf(rig, TOK_NET_RX_CODEC, index) == -RIG_ECONF);

    snprintf(index, sizeof(index), "%d", TX_CODEC_IDX_ADPCM);
    TEST_CHECK(rig_set_conf(rig, TOK_NET_TX_CODEC, index) == -RIG_ECONF);

    rig_cleanup(rig);
}

/* The combo tokens carry a choice index, and the value they select is not the
 * number the caller passed. Substituting a default for an out-of-range index
 * would run the radio at a rate or codec nobody asked for, and the caller would
 * never learn of it -- passing the rate in Hz is the obvious way to trip this. */
void test_combo_index_out_of_range_rejected(void)
{
    RIG *rig = conf_rig_init();

    TEST_CHECK(rig_set_conf(rig, TOK_NET_SAMPLE_RATE, "48000") == -RIG_EINVAL);
    TEST_MSG("a rate in Hz is not a choice index and must be refused");

    TEST_CHECK(rig_set_conf(rig, TOK_NET_SAMPLE_RATE, "99") == -RIG_EINVAL);
    TEST_CHECK(rig_set_conf(rig, TOK_NET_SAMPLE_RATE, "-1") == -RIG_EINVAL);
    TEST_CHECK(rig_set_conf(rig, TOK_NET_RX_CODEC, "99") == -RIG_EINVAL);
    TEST_CHECK(rig_set_conf(rig, TOK_NET_TX_CODEC, "99") == -RIG_EINVAL);

    /* A valid index still works, and the refusals above left it untouched. */
    TEST_CHECK(rig_set_conf(rig, TOK_NET_SAMPLE_RATE, "0") == RIG_OK);

    rig_cleanup(rig);
}

void test_tx_frame_ms_range(void)
{
    RIG *rig = conf_rig_init();
    char val[64];

    TEST_CHECK(rig_set_conf(rig, TOK_NET_TX_FRAME_MS, "50") == RIG_OK);
    TEST_CHECK(rig_get_conf2(rig, TOK_NET_TX_FRAME_MS, val, sizeof(val)) == RIG_OK);
    TEST_CHECK(strcmp(val, "50") == 0);

    TEST_CHECK(rig_set_conf(rig, TOK_NET_TX_FRAME_MS, "4") == -RIG_EINVAL);
    TEST_CHECK(rig_set_conf(rig, TOK_NET_TX_FRAME_MS, "101") == -RIG_EINVAL);

    /* rejected values must not overwrite the stored one */
    TEST_CHECK(rig_get_conf2(rig, TOK_NET_TX_FRAME_MS, val, sizeof(val)) == RIG_OK);
    TEST_CHECK(strcmp(val, "50") == 0);

    rig_cleanup(rig);
}

void test_sample_rate_index_roundtrip(void)
{
    RIG *rig = conf_rig_init();
    char val[64];

    /* index 1 = 24000 Hz */
    TEST_CHECK(rig_set_conf(rig, TOK_NET_SAMPLE_RATE, "1") == RIG_OK);
    TEST_CHECK(rig_get_conf2(rig, TOK_NET_SAMPLE_RATE, val, sizeof(val)) == RIG_OK);
    TEST_CHECK(strcmp(val, "1") == 0);

    rig_cleanup(rig);
}

void test_latency_and_tx_enable_set_get(void)
{
    RIG *rig = conf_rig_init();
    char val[64];

    TEST_CHECK(rig_set_conf(rig, TOK_NET_TX_LATENCY, "80") == RIG_OK);
    TEST_CHECK(rig_get_conf2(rig, TOK_NET_TX_LATENCY, val, sizeof(val)) == RIG_OK);
    TEST_CHECK(strcmp(val, "80") == 0);

    TEST_CHECK(rig_set_conf(rig, TOK_NET_RX_LATENCY, "120") == RIG_OK);
    TEST_CHECK(rig_get_conf2(rig, TOK_NET_RX_LATENCY, val, sizeof(val)) == RIG_OK);
    TEST_CHECK(strcmp(val, "120") == 0);

    TEST_CHECK(rig_set_conf(rig, TOK_NET_TX_ENABLE, "0") == RIG_OK);
    TEST_CHECK(rig_get_conf2(rig, TOK_NET_TX_ENABLE, val, sizeof(val)) == RIG_OK);
    TEST_CHECK(strcmp(val, "0") == 0);

    rig_cleanup(rig);
}

void test_radio_index_range(void)
{
    RIG *rig = conf_rig_init();
    char val[64];

    TEST_CHECK(rig_set_conf(rig, TOK_NET_RADIO_INDEX, "0") == RIG_OK);
    TEST_CHECK(rig_get_conf2(rig, TOK_NET_RADIO_INDEX, val, sizeof(val)) == RIG_OK);
    TEST_CHECK(strcmp(val, "0") == 0);

    TEST_CHECK(rig_set_conf(rig, TOK_NET_RADIO_INDEX, "7") == RIG_OK);
    TEST_CHECK(rig_get_conf2(rig, TOK_NET_RADIO_INDEX, val, sizeof(val)) == RIG_OK);
    TEST_CHECK(strcmp(val, "7") == 0);

    /* -1 is the "select by name" sentinel, not an out-of-range value */
    TEST_CHECK(rig_set_conf(rig, TOK_NET_RADIO_INDEX, "-1") == RIG_OK);

    /* past the end of the decodable list, and below the sentinel */
    TEST_CHECK(rig_set_conf(rig, TOK_NET_RADIO_INDEX, "8") != RIG_OK);
    TEST_CHECK(rig_set_conf(rig, TOK_NET_RADIO_INDEX, "-2") != RIG_OK);

    /* a rejected value must leave the previous one intact */
    TEST_CHECK(rig_get_conf2(rig, TOK_NET_RADIO_INDEX, val, sizeof(val)) == RIG_OK);
    TEST_CHECK(strcmp(val, "-1") == 0);

    rig_cleanup(rig);
}

void test_radio_name_roundtrip(void)
{
    RIG *rig = conf_rig_init();
    char val[64];

    TEST_CHECK(rig_set_conf(rig, TOK_NET_RADIO_NAME, "IC-9700") == RIG_OK);
    TEST_CHECK(rig_get_conf2(rig, TOK_NET_RADIO_NAME, val, sizeof(val)) == RIG_OK);
    TEST_CHECK(strcmp(val, "IC-9700") == 0);

    /* clearing it returns to selecting by the model's own name */
    TEST_CHECK(rig_set_conf(rig, TOK_NET_RADIO_NAME, "") == RIG_OK);
    TEST_CHECK(rig_get_conf2(rig, TOK_NET_RADIO_NAME, val, sizeof(val)) == RIG_OK);
    TEST_CHECK(strcmp(val, "") == 0);

    rig_cleanup(rig);
}

/* Every model the Icom network backend registers. Each is a clone of its
 * serial base model with the LAN transport and streaming wired in. */
static const struct
{
    rig_model_t model;
    const char *name;
} net_models[] =
{
    { RIG_MODEL_IC7610NET,     "IC-7610 (Network)"    },
    { RIG_MODEL_IC9700NET,     "IC-9700 (Network)"    },
    { RIG_MODEL_IC705NET,      "IC-705 (Network)"     },
    { RIG_MODEL_IC905NET,      "IC-905 (Network)"     },
    { RIG_MODEL_IC7760NET,     "IC-7760 (Network)"    },
    { RIG_MODEL_IC7300MK2NET,  "IC-7300MK2 (Network)" },
};

#define NET_MODEL_COUNT (sizeof(net_models) / sizeof(net_models[0]))

/* Each model must be registered, own the transport, and offer the same
 * config surface -- a model registered but not wired up would otherwise only
 * fail at open, against hardware. */
void test_all_net_models_registered(void)
{
    size_t i;

    for (i = 0; i < NET_MODEL_COUNT; i++)
    {
        RIG *rig = rig_init(net_models[i].model);
        char val[64];

        TEST_ASSERT(rig != NULL);
        TEST_MSG("model %u", (unsigned)net_models[i].model);

        TEST_CHECK(strcmp(rig->caps->model_name, net_models[i].name) == 0);
        TEST_MSG("model_name='%s' expected '%s'", rig->caps->model_name,
                 net_models[i].name);

        /* the backend owns its three UDP sockets; the core must not touch them */
        TEST_CHECK(rig->caps->port_type == RIG_PORT_CUSTOM);

        /* the shared network config tokens must reach every model */
        TEST_CHECK(rig_set_conf(rig, TOK_NET_USERNAME, "user") == RIG_OK);
        TEST_CHECK(rig_set_conf(rig, TOK_NET_RADIO_NAME, "X") == RIG_OK);
        TEST_CHECK(rig_get_conf2(rig, TOK_NET_RADIO_INDEX, val,
                                 sizeof(val)) == RIG_OK);
        TEST_CHECK(strcmp(val, "-1") == 0);

        rig_cleanup(rig);
    }
}

/* Every model streams RX audio, TX audio and I/Q; all of them expose the
 * radio's 48 kHz IF over the same audio path. */
void test_all_net_models_stream_caps(void)
{
    size_t i;

    for (i = 0; i < NET_MODEL_COUNT; i++)
    {
        RIG *rig = rig_init(net_models[i].model);
        int found_audio_rx = 0, found_audio_tx = 0, found_iq_rx = 0;
        int n, j;

        TEST_ASSERT(rig != NULL);
        TEST_MSG("model %s", net_models[i].name);

        n = rig_stream_caps_count(rig);
        TEST_CHECK(n == 3);
        TEST_MSG("%s: %d stream caps", net_models[i].name, n);

        for (j = 0; j < n; j++)
        {
            const struct rig_stream_caps *c = rig_stream_caps_at(rig, j);

            TEST_ASSERT(c != NULL);

            switch (c->type)
            {
            case RIG_STREAM_TYPE_AUDIO_RX: found_audio_rx = 1; break;

            case RIG_STREAM_TYPE_AUDIO_TX: found_audio_tx = 1; break;

            case RIG_STREAM_TYPE_IQ_RX:    found_iq_rx = 1; break;

            default: break;
            }
        }

        TEST_CHECK(found_audio_rx && found_audio_tx && found_iq_rx);
        TEST_MSG("%s: rx=%d tx=%d iq=%d", net_models[i].name,
                 found_audio_rx, found_audio_tx, found_iq_rx);

        rig_cleanup(rig);
    }
}

/* A network model is a clone: everything except the transport and the name
 * must match the serial model it derives from. */
void test_net_model_matches_base(void)
{
    static const struct { rig_model_t net, base; } pairs[] =
    {
        { RIG_MODEL_IC7610NET,    RIG_MODEL_IC7610    },
        { RIG_MODEL_IC9700NET,    RIG_MODEL_IC9700    },
        { RIG_MODEL_IC705NET,     RIG_MODEL_IC705     },
        { RIG_MODEL_IC905NET,     RIG_MODEL_IC905     },
        { RIG_MODEL_IC7760NET,    RIG_MODEL_IC7760    },
        { RIG_MODEL_IC7300MK2NET, RIG_MODEL_IC7300MK2 },
    };
    size_t i;

    for (i = 0; i < sizeof(pairs) / sizeof(pairs[0]); i++)
    {
        RIG *net = rig_init(pairs[i].net);
        RIG *base = rig_init(pairs[i].base);

        TEST_ASSERT(net != NULL && base != NULL);

        TEST_CHECK(net->caps->has_get_level == base->caps->has_get_level);
        TEST_CHECK(net->caps->has_set_level == base->caps->has_set_level);
        TEST_CHECK(net->caps->has_get_func == base->caps->has_get_func);
        TEST_CHECK(net->caps->has_set_func == base->caps->has_set_func);
        TEST_CHECK(net->caps->vfo_ops == base->caps->vfo_ops);
        TEST_CHECK(net->caps->targetable_vfo == base->caps->targetable_vfo);
        TEST_CHECK(net->caps->rig_type == base->caps->rig_type);
        TEST_MSG("model %u vs base %u", (unsigned)pairs[i].net,
                 (unsigned)pairs[i].base);

        /* and the parts that must differ */
        TEST_CHECK(net->caps->port_type != base->caps->port_type);
        TEST_CHECK(strcmp(net->caps->model_name, base->caps->model_name) != 0);

        rig_cleanup(net);
        rig_cleanup(base);
    }
}

/* Rigs that reject a bare power-status read infer it from a frequency read
 * instead. The flag lives in the model's icom_priv_caps, which a network model
 * shares with its base model -- so the network variant must agree, and both
 * must actually expose get_powerstat for the flag to mean anything. */
void test_powerstat_read_flag_inherited(void)
{
    static const struct { rig_model_t net, base; int expect_flag; } pairs[] =
    {
        { RIG_MODEL_IC7610NET,    RIG_MODEL_IC7610,    1 },
        { RIG_MODEL_IC9700NET,    RIG_MODEL_IC9700,    1 },
        { RIG_MODEL_IC705NET,     RIG_MODEL_IC705,     1 },
        { RIG_MODEL_IC905NET,     RIG_MODEL_IC905,     1 },
        { RIG_MODEL_IC7300MK2NET, RIG_MODEL_IC7300MK2, 1 },
        /* not established for the IC-7760, so it keeps the plain read */
        { RIG_MODEL_IC7760NET,    RIG_MODEL_IC7760,    0 },
    };
    size_t i;

    for (i = 0; i < sizeof(pairs) / sizeof(pairs[0]); i++)
    {
        RIG *net = rig_init(pairs[i].net);
        RIG *base = rig_init(pairs[i].base);
        const struct icom_priv_caps *net_priv, *base_priv;

        TEST_ASSERT(net != NULL && base != NULL);

        net_priv = net->caps->priv;
        base_priv = base->caps->priv;
        TEST_ASSERT(net_priv != NULL && base_priv != NULL);

        TEST_CHECK(base_priv->power_status_read_not_supported
                   == pairs[i].expect_flag);
        TEST_MSG("%s flag=%d expected %d", base->caps->model_name,
                 base_priv->power_status_read_not_supported, pairs[i].expect_flag);

        /* the network model clones the base caps, priv pointer included */
        TEST_CHECK(net_priv == base_priv);
        TEST_CHECK(net_priv->power_status_read_not_supported
                   == base_priv->power_status_read_not_supported);

        if (pairs[i].expect_flag)
        {
            TEST_CHECK(base->caps->get_powerstat != NULL);
            TEST_CHECK(net->caps->get_powerstat != NULL);
            TEST_MSG("%s must expose get_powerstat", base->caps->model_name);
        }

        rig_cleanup(net);
        rig_cleanup(base);
    }
}

/* Credentials round-trip, and anything the wire cannot carry is refused up
 * front rather than silently truncated into a login failure. */
void test_credentials(void)
{
    RIG *rig = conf_rig_init();
    char val[64];
    char long_name[64];

    TEST_CHECK(rig_set_conf(rig, TOK_NET_USERNAME, "oh3aa") == RIG_OK);
    TEST_CHECK(rig_get_conf2(rig, TOK_NET_USERNAME, val, sizeof(val)) == RIG_OK);
    TEST_CHECK(strcmp(val, "oh3aa") == 0);

    TEST_CHECK(rig_set_conf(rig, TOK_NET_PASSWORD, "secret") == RIG_OK);
    TEST_CHECK(rig_get_conf2(rig, TOK_NET_PASSWORD, val, sizeof(val)) == RIG_OK);
    TEST_CHECK(strcmp(val, "secret") == 0);

    /* exactly the wire limit is fine */
    memset(long_name, 'x', ICOM_NETWORK_PASSCODE_MAX);
    long_name[ICOM_NETWORK_PASSCODE_MAX] = '\0';
    TEST_CHECK(rig_set_conf(rig, TOK_NET_PASSWORD, long_name) == RIG_OK);
    TEST_CHECK(rig_get_conf2(rig, TOK_NET_PASSWORD, val, sizeof(val)) == RIG_OK);
    TEST_CHECK(strlen(val) == ICOM_NETWORK_PASSCODE_MAX);

    /* One over must be rejected rather than truncated: a truncated password
     * reaches the radio as a different password, and the login rejection that
     * follows is indistinguishable from simply getting it wrong. */
    memset(long_name, 'x', ICOM_NETWORK_PASSCODE_MAX + 1);
    long_name[ICOM_NETWORK_PASSCODE_MAX + 1] = '\0';
    TEST_CHECK(rig_set_conf(rig, TOK_NET_PASSWORD, long_name) == -RIG_EINVAL);
    TEST_CHECK(rig_set_conf(rig, TOK_NET_USERNAME, long_name) == -RIG_EINVAL);

    /* the rejected value must not have overwritten the good one */
    TEST_CHECK(rig_get_conf2(rig, TOK_NET_PASSWORD, val, sizeof(val)) == RIG_OK);
    TEST_CHECK(strlen(val) == ICOM_NETWORK_PASSCODE_MAX);

    rig_cleanup(rig);
}

/* The confparams table declares a range for each numeric token; rig_set_conf
 * does not police it, so the backend must. */
void test_numeric_ranges_enforced(void)
{
    RIG *rig = conf_rig_init();
    char val[64];

    TEST_CHECK(rig_set_conf(rig, TOK_NET_CONTROL_PORT, "50001") == RIG_OK);
    TEST_CHECK(rig_get_conf2(rig, TOK_NET_CONTROL_PORT, val, sizeof(val)) == RIG_OK);
    TEST_CHECK(strcmp(val, "50001") == 0);
    TEST_CHECK(rig_set_conf(rig, TOK_NET_CONTROL_PORT, "0") == RIG_OK);
    TEST_CHECK(rig_set_conf(rig, TOK_NET_CONTROL_PORT, "65535") == RIG_OK);
    /* out of range would wrap silently when cast to uint16_t at connect */
    TEST_CHECK(rig_set_conf(rig, TOK_NET_CONTROL_PORT, "65536") == -RIG_EINVAL);
    TEST_CHECK(rig_set_conf(rig, TOK_NET_CONTROL_PORT, "-1") == -RIG_EINVAL);

    TEST_CHECK(rig_set_conf(rig, TOK_NET_RX_LATENCY, "10") == RIG_OK);
    TEST_CHECK(rig_set_conf(rig, TOK_NET_RX_LATENCY, "1000") == RIG_OK);
    TEST_CHECK(rig_set_conf(rig, TOK_NET_RX_LATENCY, "9") == -RIG_EINVAL);
    TEST_CHECK(rig_set_conf(rig, TOK_NET_RX_LATENCY, "1001") == -RIG_EINVAL);

    TEST_CHECK(rig_set_conf(rig, TOK_NET_TX_LATENCY, "10") == RIG_OK);
    TEST_CHECK(rig_set_conf(rig, TOK_NET_TX_LATENCY, "1000") == RIG_OK);
    TEST_CHECK(rig_set_conf(rig, TOK_NET_TX_LATENCY, "9") == -RIG_EINVAL);
    TEST_CHECK(rig_set_conf(rig, TOK_NET_TX_LATENCY, "1001") == -RIG_EINVAL);

    rig_cleanup(rig);
}

/* I/Q mode selects the stereo wire codec, so it has to survive a round trip. */
void test_iq_mode(void)
{
    RIG *rig = conf_rig_init();
    char val[64];

    TEST_CHECK(rig_get_conf2(rig, TOK_NET_IQ_MODE, val, sizeof(val)) == RIG_OK);
    TEST_CHECK(strcmp(val, "0") == 0);

    TEST_CHECK(rig_set_conf(rig, TOK_NET_IQ_MODE, "1") == RIG_OK);
    TEST_CHECK(rig_get_conf2(rig, TOK_NET_IQ_MODE, val, sizeof(val)) == RIG_OK);
    TEST_CHECK(strcmp(val, "1") == 0);

    TEST_CHECK(rig_set_conf(rig, TOK_NET_IQ_MODE, "0") == RIG_OK);
    TEST_CHECK(rig_get_conf2(rig, TOK_NET_IQ_MODE, val, sizeof(val)) == RIG_OK);
    TEST_CHECK(strcmp(val, "0") == 0);

    rig_cleanup(rig);
}

/* The combo token indexes a rate list that the stream caps also advertise;
 * both come from ICOM_NETWORK_SUPPORTED_RATES and must offer the same set. The
 * two orders differ on purpose: the token list is the radio's preference order,
 * which its published indices freeze, while a caps rate list is ascending. */
void test_sample_rates_match_caps(void)
{
    static const int expected[] = ICOM_NETWORK_SUPPORTED_RATES;
    RIG *rig = conf_rig_init();
    int n, i, j, k;
    int found = 0;

    n = rig_stream_caps_count(rig);

    for (i = 0; i < n; i++)
    {
        const struct rig_stream_caps *c = rig_stream_caps_at(rig, i);
        int count = 0;

        if (c == NULL || c->type != RIG_STREAM_TYPE_AUDIO_RX) { continue; }

        found = 1;

        while (count < HAMLIB_MAX_STREAM_RATES && c->sample_rates[count] != 0)
        {
            count++;
        }

        TEST_CHECK(count == ICOM_NETWORK_SUPPORTED_RATE_COUNT);
        TEST_MSG("caps advertise %d rates, %d are configurable", count,
                 ICOM_NETWORK_SUPPORTED_RATE_COUNT);

        /* Same set, so neither list can gain a rate the other lacks. */
        for (j = 0; j < ICOM_NETWORK_SUPPORTED_RATE_COUNT; j++)
        {
            int seen = 0;

            for (k = 0; k < count; k++)
            {
                if (c->sample_rates[k] == expected[j]) { seen = 1; break; }
            }

            TEST_CHECK(seen);
            TEST_MSG("configurable rate %d missing from caps", expected[j]);
        }

        /* Ascending, which is what dump_caps enforces on every caps list. */
        for (j = 1; j < count; j++)
        {
            TEST_CHECK(c->sample_rates[j] > c->sample_rates[j - 1]);
            TEST_MSG("caps rate[%d]=%d not above rate[%d]=%d", j,
                     c->sample_rates[j], j - 1, c->sample_rates[j - 1]);
        }
    }

    TEST_CHECK(found);

    /* The combo list is the third copy of this set and the only hand-written
     * one: the caps and the index table are both generated from the macro, so
     * they agree with each other whatever the macro says. This list is what an
     * index *means* to a user reading `rigctl -L`, so a rate missing here is a
     * rate nobody can select even though the backend would serve it. */
    {
        const struct confparams *cfg = rig_confparam_lookup(rig,
                                       "net_sample_rate");
        int count = 0;

        TEST_ASSERT(cfg != NULL);
        TEST_CHECK(cfg->type == RIG_CONF_COMBO);

        while (count < RIG_COMBO_MAX && cfg->u.c.combostr[count] != NULL
                && cfg->u.c.combostr[count][0] != '\0')
        {
            count++;
        }

        TEST_CHECK(count == ICOM_NETWORK_SUPPORTED_RATE_COUNT);
        TEST_MSG("combo offers %d rates, %d are configurable", count,
                 ICOM_NETWORK_SUPPORTED_RATE_COUNT);

        for (j = 0; j < count && j < ICOM_NETWORK_SUPPORTED_RATE_COUNT; j++)
        {
            TEST_CHECK(atoi(cfg->u.c.combostr[j]) == expected[j]);
            TEST_MSG("combo[%d]=%s, index %d selects %d", j,
                     cfg->u.c.combostr[j], j, expected[j]);
        }
    }

    rig_cleanup(rig);
}

/* The liveness timeout is either disabled or comfortably above the keepalive
 * interval; values in between would declare a healthy session lost. */
void test_liveness_timeout_token(void)
{
    RIG *rig = conf_rig_init();
    char val[64];

    TEST_CHECK(rig_get_conf2(rig, TOK_NET_LIVENESS_TIMEOUT, val,
                             sizeof(val)) == RIG_OK);
    TEST_CHECK(atoi(val) == ICOM_NETWORK_DEFAULT_LIVENESS_MS);
    TEST_MSG("default was %s", val);

    TEST_CHECK(rig_set_conf(rig, TOK_NET_LIVENESS_TIMEOUT, "0") == RIG_OK);
    TEST_CHECK(rig_set_conf(rig, TOK_NET_LIVENESS_TIMEOUT, "1000") == RIG_OK);
    TEST_CHECK(rig_set_conf(rig, TOK_NET_LIVENESS_TIMEOUT, "60000") == RIG_OK);

    /* below the floor, and out of range */
    TEST_CHECK(rig_set_conf(rig, TOK_NET_LIVENESS_TIMEOUT, "999") == -RIG_EINVAL);
    TEST_CHECK(rig_set_conf(rig, TOK_NET_LIVENESS_TIMEOUT, "100") == -RIG_EINVAL);
    TEST_CHECK(rig_set_conf(rig, TOK_NET_LIVENESS_TIMEOUT, "60001") == -RIG_EINVAL);

    /* a rejected value leaves the previous one intact */
    TEST_CHECK(rig_get_conf2(rig, TOK_NET_LIVENESS_TIMEOUT, val,
                             sizeof(val)) == RIG_OK);
    TEST_CHECK(strcmp(val, "60000") == 0);

    rig_cleanup(rig);
}

void test_auto_reconnect_token(void)
{
    RIG *rig = conf_rig_init();
    char val[64];

    /* off by default: reconnecting behind the caller's back is opt-in */
    TEST_CHECK(rig_get_conf2(rig, TOK_NET_AUTO_RECONNECT, val,
                             sizeof(val)) == RIG_OK);
    TEST_CHECK(strcmp(val, "0") == 0);

    TEST_CHECK(rig_set_conf(rig, TOK_NET_AUTO_RECONNECT, "1") == RIG_OK);
    TEST_CHECK(rig_get_conf2(rig, TOK_NET_AUTO_RECONNECT, val,
                             sizeof(val)) == RIG_OK);
    TEST_CHECK(strcmp(val, "1") == 0);

    rig_cleanup(rig);
}

/* Every reason must render, and an unknown one must not return garbage. */
void test_comm_reason_strings(void)
{
    TEST_CHECK(strcmp(rig_strcommreason(RIG_COMM_REASON_NONE), "NONE") == 0);
    TEST_CHECK(strcmp(rig_strcommreason(RIG_COMM_REASON_PEER_DISCONNECT),
                      "PEER_DISCONNECT") == 0);
    TEST_CHECK(strcmp(rig_strcommreason(RIG_COMM_REASON_LINK_TIMEOUT),
                      "LINK_TIMEOUT") == 0);
    TEST_CHECK(strcmp(rig_strcommreason(RIG_COMM_REASON_SOCKET_ERROR),
                      "SOCKET_ERROR") == 0);
    TEST_CHECK(strcmp(rig_strcommreason(RIG_COMM_REASON_AUTH_FAILED),
                      "AUTH_FAILED") == 0);
    TEST_CHECK(strcmp(rig_strcommreason(0x99), "") == 0);
}

TEST_LIST =
{
    { "defaults_after_init",          test_defaults_after_init },
    { "codec_index_roundtrip",        test_codec_index_roundtrip },
    { "adpcm_rejected",               test_adpcm_rejected },
    { "combo_index_out_of_range_rejected", test_combo_index_out_of_range_rejected },
    { "tx_frame_ms_range",            test_tx_frame_ms_range },
    { "sample_rate_index_roundtrip",  test_sample_rate_index_roundtrip },
    { "latency_and_tx_enable",        test_latency_and_tx_enable_set_get },
    { "radio_index_range",            test_radio_index_range },
    { "radio_name_roundtrip",         test_radio_name_roundtrip },
    { "all_net_models_registered",    test_all_net_models_registered },
    { "all_net_models_stream_caps",   test_all_net_models_stream_caps },
    { "net_model_matches_base",       test_net_model_matches_base },
    { "powerstat_read_flag_inherited", test_powerstat_read_flag_inherited },
    { "liveness_timeout_token",       test_liveness_timeout_token },
    { "auto_reconnect_token",         test_auto_reconnect_token },
    { "comm_reason_strings",          test_comm_reason_strings },
    { "credentials",                  test_credentials },
    { "numeric_ranges_enforced",      test_numeric_ranges_enforced },
    { "iq_mode",                      test_iq_mode },
    { "sample_rates_match_caps",      test_sample_rates_match_caps },
    { NULL, NULL }
};
