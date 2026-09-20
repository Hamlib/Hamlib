/*
 * Hamlib cache polling concurrency tests
 * Copyright (c) 2026 by Hamlib Team
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 */

#include <stdio.h>

#include "hamlib/rig.h"
#include "hamlib/riglist.h"
#include "hamlib/rig_state.h"
#include "cache.h"
#include "event.h"
#include "misc.h"

#define POLL_INTERVAL_MS 50
#define DEFAULT_POLL_INTERVAL_MS 1000
#define DEFAULT_CACHE_TIMEOUT_MS 1000
#define POLL_START_SENTINEL_MS 137
#define ITERATION_COUNT 12

struct expected_vfo_state
{
    vfo_t vfo;
    freq_t freq;
    rmode_t mode;
    pbwidth_t width;
};

static int check_status(const char *operation, int status)
{
    if (status == RIG_OK)
    {
        return 0;
    }

    fprintf(stderr, "%s failed: %s\n", operation, rigerror(status));
    return 1;
}

static int check_cache(RIG *rig, const struct expected_vfo_state *expected)
{
    freq_t freq;
    rmode_t mode;
    pbwidth_t width;
    int freq_ms;
    int mode_ms;
    int width_ms;
    int status;

    status = rig_get_cache(rig, expected->vfo, &freq, &freq_ms, &mode,
                           &mode_ms, &width, &width_ms);

    if (check_status("rig_get_cache", status))
    {
        return 1;
    }

    if (freq != expected->freq || mode != expected->mode
            || width != expected->width)
    {
        fprintf(stderr,
                "%s cache mismatch: got %.0f/%s/%d, expected %.0f/%s/%d\n",
                rig_strvfo(expected->vfo), freq, rig_strrmode(mode),
                (int)width, expected->freq, rig_strrmode(expected->mode),
                (int)expected->width);
        return 1;
    }

    if (freq_ms < 0 || mode_ms < 0 || width_ms < 0)
    {
        fprintf(stderr, "%s cache has invalid ages: %d/%d/%d ms\n",
                rig_strvfo(expected->vfo), freq_ms, mode_ms, width_ms);
        return 1;
    }

    return 0;
}

static int check_split_query_preserves_vfo_age(RIG *rig)
{
    vfo_t cached_vfo;
    vfo_t tx_vfo;
    split_t split;
    int age_before;
    int age_after;
    int timeout_ms;
    int i;

    if (check_status("rig_set_cache_timeout_ms(0)",
                     rig_set_cache_timeout_ms(rig, HAMLIB_CACHE_ALL, 0))
            || check_status("rig_get_vfo(age baseline)",
                            rig_get_vfo(rig, &cached_vfo)))
    {
        return 1;
    }

    hl_usleep(20 * 1000);
    rig_get_cached_vfo(rig, &cached_vfo, &age_before, &timeout_ms);

    for (i = 0; i < 3; i++)
    {
        if (check_status("rig_get_split_vfo(age check)",
                         rig_get_split_vfo(rig, RIG_VFO_CURR, &split,
                                           &tx_vfo)))
        {
            return 1;
        }

        hl_usleep(2 * 1000);
    }

    rig_get_cached_vfo(rig, &cached_vfo, &age_after, &timeout_ms);

    if (age_after < age_before)
    {
        fprintf(stderr,
                "split query refreshed VFO cache age: before=%dms, after=%dms\n",
                age_before, age_after);
        return 1;
    }

    return 0;
}

static int check_routing_snapshot(RIG *rig)
{
    struct rig_cache_routing_snapshot routing;

    rig_set_vfo_state(rig, RIG_VFO_A, RIG_VFO_A);
    rig_set_rx_vfo_state(rig, RIG_VFO_A);
    rig_set_cache_ptt(rig, RIG_PTT_ON);
    rig_set_cache_satmode(rig, 1);
    rig_set_split_routing_state(
        rig, RIG_SPLIT_ON, RIG_VFO_A, RIG_VFO_B);
    rig_get_cache_routing_snapshot(rig, &routing);

    if (routing.current_vfo != RIG_VFO_A
            || routing.rx_vfo != RIG_VFO_A
            || routing.tx_vfo != RIG_VFO_B
            || routing.split != RIG_SPLIT_ON
            || routing.split_vfo != RIG_VFO_B
            || routing.satmode != 1
            || routing.ptt != RIG_PTT_ON)
    {
        fprintf(stderr,
                "routing snapshot mismatch: current=%s rx=%s tx=%s "
                "split=%d split_vfo=%s satmode=%d ptt=%d\n",
                rig_strvfo(routing.current_vfo),
                rig_strvfo(routing.rx_vfo), rig_strvfo(routing.tx_vfo),
                routing.split, rig_strvfo(routing.split_vfo),
                routing.satmode, routing.ptt);
        return 1;
    }

    if (check_status("rig_set_cache_mode_only(OTHER)",
                     rig_set_cache_mode_only(
                         rig, RIG_VFO_OTHER, RIG_MODE_USB)))
    {
        return 1;
    }

    rig_set_cache_ptt(rig, RIG_PTT_OFF);
    rig_set_split_routing_state(
        rig, RIG_SPLIT_OFF, RIG_VFO_A, RIG_VFO_A);
    return 0;
}

static int check_explicit_split_mapping(void)
{
    RIG *rig = rig_init(RIG_MODEL_DUMMY);
    vfo_t fixed_vfo;
    int result = 1;

    if (rig == NULL)
    {
        fprintf(stderr, "failed to initialize mapping test rig\n");
        return 1;
    }

    if (rig_get_poll_interval(rig) != DEFAULT_POLL_INTERVAL_MS
            || rig_get_cache_timeout_ms(rig, HAMLIB_CACHE_ALL)
            != DEFAULT_CACHE_TIMEOUT_MS)
    {
        fprintf(stderr,
                "default cache configuration mismatch: poll=%dms, "
                "cache=%dms; expected %dms/%dms\n",
                rig_get_poll_interval(rig),
                rig_get_cache_timeout_ms(rig, HAMLIB_CACHE_ALL),
                DEFAULT_POLL_INTERVAL_MS, DEFAULT_CACHE_TIMEOUT_MS);
        goto cleanup;
    }

    STATE(rig)->vfo_list = RIG_VFO_A | RIG_VFO_B;
    rig_set_vfo_state(rig, RIG_VFO_A, RIG_VFO_B);

    fixed_vfo = vfo_fixup(rig, RIG_VFO_TX, RIG_SPLIT_OFF);

    if (fixed_vfo != RIG_VFO_A)
    {
        fprintf(stderr, "explicit simplex fixup mismatch: got %s\n",
                rig_strvfo(fixed_vfo));
        goto cleanup;
    }

    fixed_vfo = vfo_fixup(rig, RIG_VFO_TX, RIG_SPLIT_ON);

    if (fixed_vfo != RIG_VFO_B)
    {
        fprintf(stderr, "explicit split fixup mismatch: got %s\n",
                rig_strvfo(fixed_vfo));
        goto cleanup;
    }

    result = 0;

cleanup:
    rig_cleanup(rig);
    return result;
}

int main(void)
{
    struct expected_vfo_state states[] =
    {
        { RIG_VFO_A, 0, RIG_MODE_NONE, 0 },
        { RIG_VFO_B, 0, RIG_MODE_NONE, 0 }
    };
    const char poll_interval[] = "50";
    const char polling_disabled[] = "0";
    RIG *rig = NULL;
    hamlib_token_t poll_token;
    vfo_t current_vfo = RIG_VFO_A;
    struct expected_vfo_state expected_current;
    struct expected_vfo_state expected_tx;
    struct rig_cache_snapshot snapshot;
    int expected_timeout_ms = POLL_START_SENTINEL_MS;
    int opened = 0;
    int result = 1;
    int status;
    int i;

    rig_set_debug(RIG_DEBUG_ERR);

    if (check_status("rig_load_backend", rig_load_backend("dummy")))
    {
        return 1;
    }

    rig = rig_init(RIG_MODEL_DUMMY);

    if (rig == NULL)
    {
        fprintf(stderr, "failed to initialize Dummy rig\n");
        return 1;
    }

    poll_token = rig_token_lookup(rig, "poll_interval");

    if (check_status("rig_set_conf(poll_interval)",
                     rig_set_conf(rig, poll_token, poll_interval))
            || check_status("rig_set_cache_timeout_ms(sentinel)",
                            rig_set_cache_timeout_ms(
                                rig, HAMLIB_CACHE_ALL,
                                POLL_START_SENTINEL_MS)))
    {
        goto cleanup;
    }

    if (rig_get_poll_interval(rig) != POLL_INTERVAL_MS
            || rig_get_cache_timeout_ms(rig, HAMLIB_CACHE_ALL)
            != POLL_START_SENTINEL_MS)
    {
        fprintf(stderr,
                "poll/cache configuration coupled: poll=%dms, cache=%dms\n",
                rig_get_poll_interval(rig),
                rig_get_cache_timeout_ms(rig, HAMLIB_CACHE_ALL));
        goto cleanup;
    }

    status = rig_open(rig);

    if (check_status("rig_open", status))
    {
        goto cleanup;
    }

    opened = 1;

    for (i = 0; i < ITERATION_COUNT; i++)
    {
        int state_index = i % 2 == 0 ? 1 : 0;
        int phase = (i / 2) % 2;
        int timeout_ms;
        vfo_t observed_vfo;
        struct expected_vfo_state *expected = &states[state_index];

        timeout_ms = rig_get_cache_timeout_ms(rig, HAMLIB_CACHE_ALL);

        if (timeout_ms != expected_timeout_ms)
        {
            fprintf(stderr, "cache timeout mismatch: got %d, expected %d\n",
                    timeout_ms, expected_timeout_ms);
            goto cleanup;
        }

        if (rig_get_poll_interval(rig) != POLL_INTERVAL_MS)
        {
            fprintf(stderr,
                    "cache timeout update changed poll interval: got %dms\n",
                    rig_get_poll_interval(rig));
            goto cleanup;
        }

        expected_timeout_ms = i % 2 == 0 ? 20 : 40;

        if (check_status("rig_set_cache_timeout_ms",
                         rig_set_cache_timeout_ms(rig, HAMLIB_CACHE_ALL,
                                 expected_timeout_ms)))
        {
            goto cleanup;
        }

        timeout_ms = rig_get_cache_timeout_ms(rig, HAMLIB_CACHE_ALL);

        if (timeout_ms != expected_timeout_ms)
        {
            fprintf(stderr,
                    "updated cache timeout mismatch: got %d, expected %d\n",
                    timeout_ms, expected_timeout_ms);
            goto cleanup;
        }

        if (check_status("rig_get_vfo", rig_get_vfo(rig, &observed_vfo)))
        {
            goto cleanup;
        }

        if (observed_vfo != current_vfo)
        {
            fprintf(stderr, "current VFO mismatch: got %s, expected %s\n",
                    rig_strvfo(observed_vfo), rig_strvfo(current_vfo));
            goto cleanup;
        }

        expected->freq = (state_index == 0 ? 14074000 : 7074000) + i * 10;
        expected->mode = state_index == 0
                         ? (phase == 0 ? RIG_MODE_USB : RIG_MODE_PKTUSB)
                         : (phase == 0 ? RIG_MODE_LSB : RIG_MODE_PKTLSB);
        expected->width = 2100 + i;

        if (check_status("rig_set_vfo", rig_set_vfo(rig, expected->vfo))
                || check_status("rig_set_freq",
                                rig_set_freq(rig, expected->vfo,
                                             expected->freq))
                || check_status("rig_set_mode",
                                rig_set_mode(rig, expected->vfo,
                                             expected->mode,
                                             expected->width))
                || check_cache(rig, expected))
        {
            goto cleanup;
        }

        current_vfo = expected->vfo;
    }

    if (check_status("rig_get_vfo(final)", rig_get_vfo(rig, &current_vfo)))
    {
        goto cleanup;
    }

    if (current_vfo != states[0].vfo)
    {
        fprintf(stderr, "final VFO mismatch: got %s, expected %s\n",
                rig_strvfo(current_vfo), rig_strvfo(states[0].vfo));
        goto cleanup;
    }

    expected_current = states[1];
    expected_current.vfo = RIG_VFO_CURR;

    if (check_status("dummy set_vfo(B) direct",
                     rig->caps->set_vfo(rig, RIG_VFO_B))
            || check_cache(rig, &expected_current))
    {
        goto cleanup;
    }

    rig_get_cache_snapshot(rig, &snapshot);

    if (snapshot.current_vfo != RIG_VFO_B)
    {
        fprintf(stderr, "poll snapshot VFO mismatch: got %s, expected %s\n",
                rig_strvfo(snapshot.current_vfo), rig_strvfo(RIG_VFO_B));
        goto cleanup;
    }

    expected_current = states[0];
    expected_current.vfo = RIG_VFO_CURR;

    if (check_status("dummy set_vfo(A) direct",
                     rig->caps->set_vfo(rig, RIG_VFO_A))
            || check_cache(rig, &expected_current))
    {
        goto cleanup;
    }

    if (check_status("rig_vfo_op(toggle)",
                     rig_vfo_op(rig, RIG_VFO_CURR, RIG_OP_TOGGLE))
            || check_status("rig_get_vfo(after toggle)",
                            rig_get_vfo(rig, &current_vfo)))
    {
        goto cleanup;
    }

    if (current_vfo != states[1].vfo)
    {
        fprintf(stderr, "toggled VFO mismatch: got %s, expected %s\n",
                rig_strvfo(current_vfo), rig_strvfo(states[1].vfo));
        goto cleanup;
    }

    if (check_status("rig_vfo_op(toggle back)",
                     rig_vfo_op(rig, RIG_VFO_CURR, RIG_OP_TOGGLE))
            || check_status("rig_get_vfo(after toggle back)",
                            rig_get_vfo(rig, &current_vfo)))
    {
        goto cleanup;
    }

    if (current_vfo != states[0].vfo)
    {
        fprintf(stderr, "restored VFO mismatch: got %s, expected %s\n",
                rig_strvfo(current_vfo), rig_strvfo(states[0].vfo));
        goto cleanup;
    }

    expected_tx = states[1];
    expected_tx.vfo = RIG_VFO_TX;

    if (check_status("rig_set_split_vfo(on)",
                     rig_set_split_vfo(rig, RIG_VFO_A, RIG_SPLIT_ON,
                                       RIG_VFO_B))
            || check_cache(rig, &expected_tx))
    {
        goto cleanup;
    }

    expected_tx = states[0];
    expected_tx.vfo = RIG_VFO_TX;

    if (check_status("rig_set_split_vfo(off)",
                     rig_set_split_vfo(rig, RIG_VFO_A, RIG_SPLIT_OFF,
                                       RIG_VFO_A))
            || check_cache(rig, &expected_tx))
    {
        goto cleanup;
    }

    if (check_cache(rig, &states[0]) || check_cache(rig, &states[1]))
    {
        goto cleanup;
    }

    if (check_status("rig_set_conf(poll_interval=0)",
                     rig_set_conf(rig, poll_token, polling_disabled)))
    {
        goto cleanup;
    }

    if (rig_get_poll_interval(rig) != 0
            || rig_get_cache_timeout_ms(rig, HAMLIB_CACHE_ALL)
            != expected_timeout_ms)
    {
        fprintf(stderr,
                "disabling polling changed cache timeout: poll=%dms, cache=%dms\n",
                rig_get_poll_interval(rig),
                rig_get_cache_timeout_ms(rig, HAMLIB_CACHE_ALL));
        goto cleanup;
    }

    hl_usleep(POLL_INTERVAL_MS * 2 * 1000);

    if (check_split_query_preserves_vfo_age(rig))
    {
        goto cleanup;
    }

    if (check_routing_snapshot(rig))
    {
        goto cleanup;
    }

    if (check_explicit_split_mapping())
    {
        goto cleanup;
    }

    result = 0;

cleanup:

    if (opened && check_status("rig_close", rig_close(rig)))
    {
        result = 1;
    }

    if (check_status("rig_cleanup", rig_cleanup(rig)))
    {
        result = 1;
    }

    return result;
}
