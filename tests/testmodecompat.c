/*
 * Hamlib generic and rig-specific mode compatibility tests
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

static struct rig_caps *dummy_caps;
static int selected_only;
static int query_error;
static int write_error;
static int restore_error;

#define REQUIRE_OK(expression) do { \
    int result = (expression); \
    if (result != RIG_OK) { \
        fprintf(stderr, "%s: %s: %s\n", __func__, #expression, rigerror(result)); \
        return 1; \
    } \
} while (0)

static int fixture_get_mode(RIG *rig, vfo_t vfo, rmode_t *mode,
                            pbwidth_t *width)
{
    if (query_error) { return query_error; }

    if (selected_only)
    {
        /* A non-targetable radio observes its selected VFO, not the argument. */
        int retval = dummy_caps->get_vfo(rig, &vfo);
        if (retval != RIG_OK) { return retval; }
    }

    return dummy_caps->get_mode(rig, vfo, mode, width);
}

static int fixture_set_mode(RIG *rig, vfo_t vfo, rmode_t mode,
                            pbwidth_t width)
{
    if (write_error) { return write_error; }

    if (selected_only)
    {
        int retval = dummy_caps->get_vfo(rig, &vfo);
        if (retval != RIG_OK) { return retval; }
    }

    return dummy_caps->set_mode(rig, vfo, mode, width);
}

static int fixture_set_vfo(RIG *rig, vfo_t vfo)
{
    if (restore_error && vfo == RIG_VFO_A) { return restore_error; }
    return dummy_caps->set_vfo(rig, vfo);
}

static int expect_state(RIG *rig, const char *scenario, vfo_t vfo,
                        rmode_t expected_mode, pbwidth_t expected_width)
{
    rmode_t mode = RIG_MODE_NONE;
    pbwidth_t width = RIG_PASSBAND_NORMAL;
    int cached;

    for (cached = 0; cached < 2; cached++)
    {
        int retval = cached ? rig_get_mode(rig, vfo, &mode, &width)
                     : dummy_caps->get_mode(rig, vfo, &mode, &width);

        if (retval != RIG_OK || mode != expected_mode || width != expected_width)
        {
            fprintf(stderr, "%s: %s %s read returned %s, %s/%ld; expected %s/%ld\n",
                    scenario, rig_strvfo(vfo), cached ? "public" : "backend",
                    rigerror(retval), rig_strrmode(mode), (long)width,
                    rig_strrmode(expected_mode), (long)expected_width);
            return 1;
        }
    }

    return 0;
}

static int expect_selected(RIG *rig, vfo_t expected)
{
    vfo_t actual;
    REQUIRE_OK(dummy_caps->get_vfo(rig, &actual));

    if (actual != expected)
    {
        fprintf(stderr, "%s: selected %s, expected %s\n", __func__,
                rig_strvfo(actual), rig_strvfo(expected));
        return 1;
    }

    return 0;
}

static int expect_error(const char *scenario, int actual, int expected)
{
    if (actual != expected)
    {
        fprintf(stderr, "%s: returned %s (%d), expected %s (%d)\n", scenario,
                rigerror(actual), actual, rigerror(expected), expected);
        return 1;
    }

    return 0;
}

static int test_profiles(RIG *rig)
{
    static const struct
    {
        rmode_t profile;
        rmode_t generic;
    } modes[] =
    {
        {RIG_MODE_USBD1, RIG_MODE_PKTUSB},
        {RIG_MODE_USBD2, RIG_MODE_PKTUSB},
        {RIG_MODE_USBD3, RIG_MODE_PKTUSB},
        {RIG_MODE_LSBD1, RIG_MODE_PKTLSB},
        {RIG_MODE_LSBD2, RIG_MODE_PKTLSB},
        {RIG_MODE_LSBD3, RIG_MODE_PKTLSB}
    };
    static const pbwidth_t widths[] =
    {
        RIG_PASSBAND_NOCHANGE, 2400, 1800
    };
    size_t i, j;

    for (i = 0; i < sizeof(modes) / sizeof(modes[0]); i++)
    {
        for (j = 0; j < sizeof(widths) / sizeof(widths[0]); j++)
        {
            char scenario[96];
            pbwidth_t expected = widths[j] == RIG_PASSBAND_NOCHANGE ? 2400 : widths[j];
            snprintf(scenario, sizeof(scenario), "%s requested as %s, width %ld",
                     rig_strrmode(modes[i].profile), rig_strrmode(modes[i].generic),
                     (long)widths[j]);
            REQUIRE_OK(rig_set_mode(rig, RIG_VFO_A, modes[i].profile, 2400));
            REQUIRE_OK(rig_set_mode(rig, RIG_VFO_A, modes[i].generic, widths[j]));

            if (expect_state(rig, scenario, RIG_VFO_A, modes[i].profile, expected))
            {
                return 1;
            }
        }
    }

    return 0;
}

static int test_mode_changes(RIG *rig)
{
    static const struct
    {
        const char *name;
        rmode_t initial;
        rmode_t request;
    } cases[] =
    {
        {"explicit USB profile", RIG_MODE_USBD2, RIG_MODE_USBD1},
        {"explicit LSB profile", RIG_MODE_LSBD2, RIG_MODE_LSBD3},
        {"USB entering packet mode", RIG_MODE_USB, RIG_MODE_PKTUSB},
        {"LSB entering packet mode", RIG_MODE_LSB, RIG_MODE_PKTLSB},
        {"LSB to USB", RIG_MODE_LSBD2, RIG_MODE_PKTUSB},
        {"USB to LSB", RIG_MODE_USBD3, RIG_MODE_PKTLSB}
    };
    size_t i;

    for (i = 0; i < sizeof(cases) / sizeof(cases[0]); i++)
    {
        REQUIRE_OK(rig_set_mode(rig, RIG_VFO_A, cases[i].initial, 2400));
        REQUIRE_OK(rig_set_mode(rig, RIG_VFO_A, cases[i].request,
                               RIG_PASSBAND_NOCHANGE));

        if (expect_state(rig, cases[i].name, RIG_VFO_A, cases[i].request, 2400))
        {
            return 1;
        }
    }

    return 0;
}

static int test_front_panel_change(RIG *rig)
{
    REQUIRE_OK(rig_set_mode(rig, RIG_VFO_A, RIG_MODE_USBD1, 2400));
    /* A front-panel change bypasses the public setter and its cache updates. */
    REQUIRE_OK(dummy_caps->set_mode(rig, RIG_VFO_A, RIG_MODE_USBD2, 1800));
    REQUIRE_OK(rig_set_mode(rig, RIG_VFO_A, RIG_MODE_PKTUSB,
                           RIG_PASSBAND_NOCHANGE));

    if (expect_state(rig, "fresh profile query refreshes cache", RIG_VFO_A,
                     RIG_MODE_USBD2, 1800)) { return 1; }

    REQUIRE_OK(dummy_caps->set_mode(rig, RIG_VFO_A, RIG_MODE_USBD2, 2100));
    REQUIRE_OK(rig_set_mode(rig, RIG_VFO_A, RIG_MODE_USBD2,
                           RIG_PASSBAND_NOCHANGE));
    return expect_state(rig, "exact-mode query refreshes width cache", RIG_VFO_A,
                        RIG_MODE_USBD2, 2100);
}

static int test_split_fallback(RIG *rig)
{
    freq_t frequency;
    REQUIRE_OK(rig_set_vfo(rig, RIG_VFO_A));
    REQUIRE_OK(rig_set_mode(rig, RIG_VFO_A, RIG_MODE_LSBD3, 1800));
    REQUIRE_OK(rig_set_mode(rig, RIG_VFO_B, RIG_MODE_USBD2, 2400));
    REQUIRE_OK(rig_set_freq(rig, RIG_VFO_A, 7100000));
    REQUIRE_OK(rig_set_freq(rig, RIG_VFO_B, 14074000));
    REQUIRE_OK(rig_set_split_vfo(rig, RIG_VFO_A, RIG_SPLIT_ON, RIG_VFO_B));
    REQUIRE_OK(rig_set_split_mode(rig, RIG_VFO_A, RIG_MODE_PKTUSB, 2100));

    if (expect_state(rig, "split width preserves TX profile", RIG_VFO_B,
                     RIG_MODE_USBD2, 2100)) { return 1; }

    REQUIRE_OK(rig_set_split_freq_mode(rig, RIG_VFO_A, 14075000, RIG_MODE_PKTUSB,
                                      RIG_PASSBAND_NOCHANGE));

    if (expect_state(rig, "combined request preserves TX profile", RIG_VFO_B,
                     RIG_MODE_USBD2, 2100)
            || expect_state(rig, "split leaves RX mode intact", RIG_VFO_A,
                            RIG_MODE_LSBD3, 1800)
            || expect_selected(rig, RIG_VFO_A)) { return 1; }

    REQUIRE_OK(dummy_caps->get_freq(rig, RIG_VFO_B, &frequency));

    if (frequency != 14075000)
    {
        fprintf(stderr, "combined request left TX frequency at %.0f\n", frequency);
        return 1;
    }

    REQUIRE_OK(dummy_caps->get_freq(rig, RIG_VFO_A, &frequency));

    if (frequency != 7100000)
    {
        fprintf(stderr, "split request changed RX frequency to %.0f\n", frequency);
        return 1;
    }

    REQUIRE_OK(rig_set_split_vfo(rig, RIG_VFO_A, RIG_SPLIT_OFF, RIG_VFO_A));
    return 0;
}

static int test_selected_vfo(RIG *rig, struct rig_caps *caps)
{
    REQUIRE_OK(rig_set_mode(rig, RIG_VFO_A, RIG_MODE_USBD3, 1800));
    REQUIRE_OK(rig_set_mode(rig, RIG_VFO_B, RIG_MODE_USBD3, 2400));
    selected_only = 1;
    caps->targetable_vfo &= ~RIG_TARGETABLE_MODE;
    REQUIRE_OK(rig_set_mode(rig, RIG_VFO_B, RIG_MODE_PKTUSB,
                           RIG_PASSBAND_NOCHANGE));

    if (expect_state(rig, "resolve against selected target", RIG_VFO_B,
                     RIG_MODE_USBD3, 2400)) { return 1; }

    REQUIRE_OK(rig_set_mode(rig, RIG_VFO_B, RIG_MODE_USBD3, 2100));

    if (expect_state(rig, "noncurrent mode still accepts width", RIG_VFO_B,
                     RIG_MODE_USBD3, 2100)
            || expect_state(rig, "noncurrent request preserves RX", RIG_VFO_A,
                            RIG_MODE_USBD3, 1800)
            || expect_selected(rig, RIG_VFO_A)) { return 1; }

    selected_only = 0;
    caps->targetable_vfo = dummy_caps->targetable_vfo;
    return 0;
}

static int test_errors(RIG *rig, struct rig_caps *caps)
{
    REQUIRE_OK(rig_set_mode(rig, RIG_VFO_A, RIG_MODE_USBD2, 2400));
    query_error = -RIG_EIO;

    REQUIRE_OK(rig_set_mode(rig, RIG_VFO_A, RIG_MODE_PKTUSB, 1800));

    query_error = 0;

    if (expect_state(rig, "query failure falls back to setter", RIG_VFO_A,
                     RIG_MODE_PKTUSB, 1800)) { return 1; }

    REQUIRE_OK(rig_set_mode(rig, RIG_VFO_A, RIG_MODE_USBD2, 2400));
    write_error = -RIG_EIO;

    if (expect_error("mode write failure", rig_set_mode(rig, RIG_VFO_A,
                     RIG_MODE_PKTUSB, 1800), -RIG_EIO)) { return 1; }

    write_error = 0;

    if (expect_state(rig, "failed write preserves radio and cache", RIG_VFO_A,
                     RIG_MODE_USBD2, 2400)) { return 1; }

    REQUIRE_OK(rig_set_mode(rig, RIG_VFO_B, RIG_MODE_LSBD2, 2400));
    selected_only = 1;
    caps->targetable_vfo &= ~RIG_TARGETABLE_MODE;
    query_error = -RIG_EIO;

    REQUIRE_OK(rig_set_mode(rig, RIG_VFO_B, RIG_MODE_PKTLSB, 1800));
    if (expect_state(rig, "noncurrent query falls back to setter", RIG_VFO_B,
                     RIG_MODE_PKTLSB, 1800)
            || expect_selected(rig, RIG_VFO_A)) { return 1; }

    query_error = 0;
    write_error = -RIG_EIO;

    if (expect_error("noncurrent write failure", rig_set_mode(rig, RIG_VFO_B,
                     RIG_MODE_PKTLSB, 1800), -RIG_EIO)
            || expect_selected(rig, RIG_VFO_A)) { return 1; }

    restore_error = -RIG_ETIMEOUT;

    if (expect_error("operation error precedes restore error",
                     rig_set_mode(rig, RIG_VFO_B, RIG_MODE_PKTLSB, 1800),
                     -RIG_EIO)) { return 1; }

    restore_error = 0;
    REQUIRE_OK(dummy_caps->set_vfo(rig, RIG_VFO_A));
    write_error = 0;
    restore_error = -RIG_ETIMEOUT;

    if (expect_error("restore failure after successful write",
                     rig_set_mode(rig, RIG_VFO_B, RIG_MODE_PKTLSB, 1800),
                     -RIG_ETIMEOUT)
            || expect_selected(rig, RIG_VFO_B)) { return 1; }

    restore_error = 0;
    REQUIRE_OK(dummy_caps->set_vfo(rig, RIG_VFO_A));
    selected_only = 0;
    caps->targetable_vfo = dummy_caps->targetable_vfo;
    REQUIRE_OK(rig_set_mode(rig, RIG_VFO_A, RIG_MODE_PKTUSB, 1800));
    return expect_state(rig, "setter-only backend remains usable", RIG_VFO_A,
                        RIG_MODE_USBD2, 1800);
}

int main(void)
{
    RIG *rig = rig_init(RIG_MODEL_DUMMY);
    struct rig_caps caps;
    int failed = 0;

    if (rig == NULL)
    {
        fprintf(stderr, "cannot initialize dummy rig\n");
        return 1;
    }

    if (rig_open(rig) != RIG_OK)
    {
        fprintf(stderr, "cannot open dummy rig\n");
        rig_cleanup(rig);
        return 1;
    }

    dummy_caps = rig->caps;
    caps = *dummy_caps;
    caps.get_mode = fixture_get_mode;
    caps.set_mode = fixture_set_mode;
    caps.set_vfo = fixture_set_vfo;
    caps.set_split_mode = NULL;
    caps.set_split_freq_mode = NULL;
    rig->caps = &caps;
    rig_set_cache_timeout_ms(rig, HAMLIB_CACHE_ALL, 60000);

    failed = test_profiles(rig) || test_mode_changes(rig)
             || test_front_panel_change(rig) || test_split_fallback(rig)
             || test_selected_vfo(rig, &caps) || test_errors(rig, &caps);

    rig->caps = dummy_caps;
    rig_close(rig);
    rig_cleanup(rig);
    return failed;
}
