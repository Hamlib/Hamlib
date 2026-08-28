/*
 * Hamlib IC-7760 capability tests
 * Copyright (c) 2026 by Hamlib Team
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 */

/*
 * Expected values are taken from the IC-7760 CI-V REFERENCE GUIDE,
 * revision A7788-8EX-2 (May 2025).  Commands are cited by number rather
 * than by page, because Icom repaginates between revisions.
 */

#include <math.h>
#include <stdio.h>
#include <string.h>

#include "cal.h"
#include "hamlib/rig_state.h"
#include "icom.h"
#include "icom_defs.h"
#include "idx_builtin.h"

static const struct cmdparams *find_level_cmd(const struct cmdparams *cmds,
        setting_t level)
{
    int i;

    for (i = 0; cmds != NULL && cmds[i].id.s != 0; i++)
    {
        if (cmds[i].cmdparamtype == CMD_PARAM_TYPE_LEVEL
                && cmds[i].id.s == level)
        {
            return &cmds[i];
        }
    }

    return NULL;
}

/* VOX delay is 1A 05 03 65, data 00 ~ 20 in 0.1 s steps */
static int test_vox_delay_subcommand(void)
{
    const struct icom_priv_caps *priv = (const struct icom_priv_caps *)
                                        ic7760_caps.priv;
    const struct cmdparams *cmd = find_level_cmd(priv->extcmds,
                                  RIG_LEVEL_VOXDELAY);

    if (cmd == NULL)
    {
        fprintf(stderr, "VOX delay: no extcmds entry\n");
        return 1;
    }

    if (cmd->command != 0x1a || cmd->subcmd != 0x05 || cmd->sublen != 2
            || cmd->subext[0] != 0x03 || cmd->subext[1] != 0x65)
    {
        fprintf(stderr,
                "VOX delay: expected 1A 05 03 65, got %02X %02X %02X %02X (sublen %d)\n",
                cmd->command, cmd->subcmd, cmd->subext[0], cmd->subext[1],
                cmd->sublen);
        return 1;
    }

    return 0;
}

/*
 * Capabilities the IC-7760 has no CI-V command for.  Command 14 skips
 * subcommand 10 and command 16 skips subcommand 4C, so neither the
 * dual-watch balance nor voice squelch control exists.
 */
static int test_absent_capabilities(void)
{
    static const struct
    {
        const char *name;
        setting_t level;
        setting_t func;
    } absent[] =
    {
        { "BALANCE (14 10)", RIG_LEVEL_BALANCE, 0 },
        { "VSC (16 4C)", 0, RIG_FUNC_VSC },
    };
    size_t i;
    int fail = 0;

    for (i = 0; i < sizeof(absent) / sizeof(absent[0]); i++)
    {
        if ((ic7760_caps.has_get_level & absent[i].level) != 0
                || (ic7760_caps.has_set_level & absent[i].level) != 0
                || (ic7760_caps.has_get_func & absent[i].func) != 0
                || (ic7760_caps.has_set_func & absent[i].func) != 0)
        {
            fprintf(stderr, "%s is advertised but the rig has no such command\n",
                    absent[i].name);
            fail = 1;
        }
    }

    return fail;
}

/*
 * Capabilities the rig documents and the backend has to advertise.
 * Dual watch is 07 C0, 07 C1 and 07 C2; the CI-V transceive switch is
 * 1A 05 01 50.
 */
static int test_present_capabilities(void)
{
    static const struct
    {
        const char *name;
        setting_t func;
    } present[] =
    {
        { "DUAL_WATCH (07 C2)", RIG_FUNC_DUAL_WATCH },
        { "TRANSCEIVE (1A 05 01 50)", RIG_FUNC_TRANSCEIVE },
    };
    size_t i;
    int fail = 0;

    for (i = 0; i < sizeof(present) / sizeof(present[0]); i++)
    {
        if ((ic7760_caps.has_get_func & present[i].func) == 0
                || (ic7760_caps.has_set_func & present[i].func) == 0)
        {
            fprintf(stderr, "%s is supported by the rig but not advertised\n",
                    present[i].name);
            fail = 1;
        }
    }

    return fail;
}

/*
 * Keyer memory content (1A 02) addresses channels 01=M1 to 08=M8, and
 * the Voice TX memory (28 00) transmits 01=T1 to 08=T8.
 */
static int test_memory_channel_counts(void)
{
    static const struct
    {
        const char *name;
        chan_type_t type;
        int startc;
        int endc;
    } expected[] =
    {
        { "memory", RIG_MTYPE_MEM, 1, 99 },
        { "scan edge", RIG_MTYPE_EDGE, 100, 101 },
        { "keyer memory", RIG_MTYPE_MORSE, 1, 8 },
        { "voice memory", RIG_MTYPE_VOICE, 1, 8 },
    };
    size_t i;
    int j;
    int fail = 0;

    for (i = 0; i < sizeof(expected) / sizeof(expected[0]); i++)
    {
        int found = 0;

        for (j = 0; j < HAMLIB_CHANLSTSIZ
                && ic7760_caps.chan_list[j].type != RIG_MTYPE_NONE; j++)
        {
            const struct chan_list *chan = &ic7760_caps.chan_list[j];

            if (chan->type != expected[i].type) { continue; }

            found = 1;

            if (chan->startc != expected[i].startc
                    || chan->endc != expected[i].endc)
            {
                fprintf(stderr, "%s channels: expected %d..%d, got %d..%d\n",
                        expected[i].name, expected[i].startc, expected[i].endc,
                        chan->startc, chan->endc);
                fail = 1;
            }
        }

        if (!found)
        {
            fprintf(stderr, "%s channels are not listed at all\n",
                    expected[i].name);
            fail = 1;
        }
    }

    return fail;
}

/*
 * Default IF filter widths, from the IC-7760 basic manual.  rig2icom_mode()
 * picks FIL1 for a width wider than the normal passband, FIL2 for the
 * normal one and FIL3 for a narrower one, so the caps list has to hold
 * each mode's FIL2, FIL3 and FIL1 width in that order.
 */
static int test_filter_defaults(RIG *rig)
{
    static const struct
    {
        const char *name;
        rmode_t mode;
        pbwidth_t fil2;
        pbwidth_t fil3;
        pbwidth_t fil1;
    } filters[] =
    {
        { "SSB", RIG_MODE_USB, 2400, 1800, 3000 },
        { "SSB-D", RIG_MODE_PKTUSB, 1200, 500, 3000 },
        { "CW", RIG_MODE_CW, 500, 250, 1200 },
        { "PSK", RIG_MODE_PSK, 500, 250, 1200 },
        { "RTTY", RIG_MODE_RTTY, 500, 250, 2400 },
        { "AM", RIG_MODE_AM, 6000, 3000, 9000 },
        { "FM", RIG_MODE_FM, 10000, 7000, 15000 },
    };
    size_t i;
    int fail = 0;

    for (i = 0; i < sizeof(filters) / sizeof(filters[0]); i++)
    {
        pbwidth_t normal = rig_passband_normal(rig, filters[i].mode);
        pbwidth_t narrow = rig_passband_narrow(rig, filters[i].mode);
        pbwidth_t wide = rig_passband_wide(rig, filters[i].mode);

        if (normal != filters[i].fil2 || narrow != filters[i].fil3
                || wide != filters[i].fil1)
        {
            fprintf(stderr,
                    "%s filters: expected FIL2/FIL3/FIL1 %ld/%ld/%ld, got %ld/%ld/%ld\n",
                    filters[i].name, (long) filters[i].fil2, (long) filters[i].fil3,
                    (long) filters[i].fil1, (long) normal, (long) narrow, (long) wide);
            fail = 1;
        }
    }

    return fail;
}

/*
 * The spectrum scope is commands 27 00 to 27 20.  A waveform is 689
 * points ranging 00 to C8, sent over USB in fifteen pieces of which the
 * first carries only the header, leaving 50 points per piece.  The span
 * is held here as the full width, because the backend halves it before
 * putting it on the wire.
 */
static int test_spectrum_scope(void)
{
    static const struct
    {
        int range_id;
        freq_t low;
        freq_t high;
    } ranges[] =
    {
        {  1,    30000,  1600000 },
        {  2,  1600000,  2000000 },
        {  3,  2000000,  6000000 },
        {  4,  6000000,  8000000 },
        {  5,  8000000, 11000000 },
        {  6, 11000000, 15000000 },
        {  7, 15000000, 20000000 },
        {  8, 20000000, 22000000 },
        {  9, 22000000, 26000000 },
        { 10, 26000000, 30000000 },
        { 11, 30000000, 45000000 },
        { 12, 45000000, 60000000 },
    };
    static const freq_t spans[] =
    {
        5000, 10000, 20000, 50000, 100000, 200000, 500000, 1000000, 0
    };
    const struct icom_priv_caps *priv = (const struct icom_priv_caps *)
                                        ic7760_caps.priv;
    const struct icom_spectrum_scope_caps *scope = &priv->spectrum_scope_caps;
    const gran_t *ref = &ic7760_caps.level_gran[LVL_SPECTRUM_REF];
    size_t i;
    int fail = 0;

    if ((ic7760_caps.has_get_func & RIG_FUNC_SPECTRUM) == 0
            || (ic7760_caps.has_get_level & RIG_LEVEL_SPECTRUM_SPAN) == 0)
    {
        fprintf(stderr, "the spectrum scope is not advertised\n");
        return 1;
    }

    /*
     * The rig outputs waveform data only while both the scope itself
     * (27 10) and the data output (27 11) are on, so a client needs the
     * scope switch as well before it can start streaming.
     */
    if ((ic7760_caps.has_get_func & RIG_FUNC_SCOPE) == 0
            || (ic7760_caps.has_set_func & RIG_FUNC_SCOPE) == 0)
    {
        fprintf(stderr, "the scope on/off switch (27 10) is not advertised\n");
        fail = 1;
    }

    if (scope->spectrum_line_length != 689
            || scope->single_frame_data_length != 50
            || scope->data_level_min != 0 || scope->data_level_max != 200)
    {
        fprintf(stderr, "scope waveform: expected 689 points of 0..200 in"
                " pieces of 50, got %d points of %d..%d in pieces of %d\n",
                scope->spectrum_line_length, scope->data_level_min,
                scope->data_level_max, scope->single_frame_data_length);
        fail = 1;
    }

    for (i = 0; i < sizeof(ranges) / sizeof(ranges[0]); i++)
    {
        const struct icom_spectrum_edge_frequency_range *r =
            &priv->spectrum_edge_frequency_ranges[i];

        if (r->range_id != ranges[i].range_id || r->low_freq != ranges[i].low
                || r->high_freq != ranges[i].high)
        {
            fprintf(stderr, "edge range %d: expected %.0f-%.0f Hz, got id %d"
                    " %.0f-%.0f Hz\n", ranges[i].range_id, ranges[i].low,
                    ranges[i].high, r->range_id, r->low_freq, r->high_freq);
            fail = 1;
        }
    }

    for (i = 0; i < sizeof(spans) / sizeof(spans[0]); i++)
    {
        if (ic7760_caps.spectrum_spans[i] != spans[i])
        {
            fprintf(stderr, "span %d: expected %.0f Hz, got %.0f Hz\n",
                    (int) i, spans[i], ic7760_caps.spectrum_spans[i]);
            fail = 1;
        }
    }

    /* The reference level runs -30.0 to +10.0 dB in 0.5 dB steps */
    if (ref->min.f != -30.0f || ref->max.f != 10.0f || ref->step.f != 0.5f)
    {
        fprintf(stderr, "scope reference level: expected -30.0..10.0 step 0.5,"
                " got %.1f..%.1f step %.1f\n", ref->min.f, ref->max.f,
                ref->step.f);
        fail = 1;
    }

    return fail;
}

struct spectrum_capture
{
    int events;
    int length;
    int first_point;
    int last_point;
    freq_t center;
    freq_t span;
};

static int record_spectrum_line(RIG *rig, struct rig_spectrum_line *line,
                                rig_ptr_t data)
{
    struct spectrum_capture *capture = (struct spectrum_capture *) data;

    capture->events++;
    capture->length = line->spectrum_data_length;
    capture->center = line->center_freq;
    capture->span = line->span_freq;

    if (line->spectrum_data_length > 0)
    {
        capture->first_point = line->spectrum_data[0];
        capture->last_point =
            line->spectrum_data[line->spectrum_data_length - 1];
    }

    return 0;
}

/* FE FE E0 B2 27 00 <scope> <division> <divisions> <data...> FD */
static size_t build_scope_frame(unsigned char *frame, int division,
                                int divisions, const unsigned char *data,
                                size_t data_len)
{
    size_t len = 0;
    size_t i;

    frame[len++] = 0xfe;
    frame[len++] = 0xfe;
    frame[len++] = CTRLID;
    frame[len++] = 0xb2;
    frame[len++] = C_CTL_SCP;
    frame[len++] = S_SCP_DAT;
    frame[len++] = 0x00;                    /* main scope */
    frame[len++] = (unsigned char)(((division / 10) << 4) | (division % 10));
    frame[len++] = (unsigned char)(((divisions / 10) << 4) | (divisions % 10));

    for (i = 0; i < data_len; i++) { frame[len++] = data[i]; }

    frame[len++] = 0xfd;

    return len;
}

/*
 * Waveform data is unsolicited, so 27 00 reaches hamlib only through the
 * asynchronous frame path.  Over USB one sweep is fifteen frames: a
 * header, thirteen carrying 50 points each and a last carrying 39, which
 * is the 689 point line the scope caps declare.  In centre mode the rig
 * sends half the span, so 25 kHz on the wire is a 50 kHz line.
 */
static int test_spectrum_streaming(RIG *rig)
{
    static const unsigned char header[] =
    {
        SCOPE_MODE_CENTER,
        0x00, 0x00, 0x10, 0x14, 0x00,   /* centre 14.100000 MHz */
        0x00, 0x50, 0x02, 0x00, 0x00,   /* span 25 kHz */
        0x00                            /* in range */
    };
    unsigned char data[50];
    unsigned char frame[60];
    struct spectrum_capture capture = { 0, 0, -1, -1, 0, 0 };
    size_t len;
    int division;
    int fail = 0;

    if (!ic7760_caps.async_data_supported
            || ic7760_caps.read_frame_direct == NULL
            || ic7760_caps.is_async_frame == NULL
            || ic7760_caps.process_async_frame == NULL)
    {
        fprintf(stderr, "waveform data arrives unsolicited, but the"
                " asynchronous frame path is not wired up\n");
        return 1;
    }

    /* the callback API is only open once the rig counts as connected */
    STATE(rig)->comm_state = 1;
    rig_set_spectrum_callback(rig, record_spectrum_line, &capture);

    len = build_scope_frame(frame, 1, 15, header, sizeof(header));

    if (!ic7760_caps.is_async_frame(rig, len, frame))
    {
        fprintf(stderr, "a 27 00 frame was not taken for asynchronous data\n");
        fail = 1;
    }

    ic7760_caps.process_async_frame(rig, len, frame);

    memset(data, 1, sizeof(data));

    for (division = 2; division <= 15; division++)
    {
        size_t points = (division == 15) ? 39 : 50;

        if (division == 15) { memset(data, 200, sizeof(data)); }

        len = build_scope_frame(frame, division, 15, data, points);
        ic7760_caps.process_async_frame(rig, len, frame);
    }

    rig_set_spectrum_callback(rig, NULL, NULL);
    STATE(rig)->comm_state = 0;

    if (capture.events != 1)
    {
        fprintf(stderr, "a fifteen frame sweep raised %d spectrum events,"
                " expected one\n", capture.events);
        return fail | 1;
    }

    if (capture.length != 689)
    {
        fprintf(stderr, "a fifteen frame sweep carried %d points, expected"
                " 689\n", capture.length);
        fail = 1;
    }

    if (capture.first_point != 1 || capture.last_point != 200)
    {
        fprintf(stderr, "sweep points run %d..%d, expected the second frame's"
                " 1 and the last frame's 200\n", capture.first_point,
                capture.last_point);
        fail = 1;
    }

    if (capture.center != 14100000.0 || capture.span != 50000.0)
    {
        fprintf(stderr, "sweep centre/span read %.0f/%.0f Hz, expected"
                " 14100000/50000\n", capture.center, capture.span);
        fail = 1;
    }

    return fail;
}

/*
 * Command 16 12 selects the AGC time constant and takes 01=FAST,
 * 02=MID or 03=SLOW.  It has no 00: the rig answers FA to 16 12 00.
 * Switching the AGC off is reached through 1A 04 instead, whose 00 is
 * OFF, and hamlib exposes that as an AGC time of zero.
 */
static int test_agc_levels(void)
{
    static const enum agc_level_e expected[] =
    {
        RIG_AGC_FAST, RIG_AGC_MEDIUM, RIG_AGC_SLOW
    };
    const struct icom_priv_caps *priv = (const struct icom_priv_caps *)
                                        ic7760_caps.priv;
    int count = (int)(sizeof(expected) / sizeof(expected[0]));
    int i;
    int fail = 0;

    if (ic7760_caps.agc_level_count != count)
    {
        fprintf(stderr, "agc_level_count: expected %d, got %d\n", count,
                ic7760_caps.agc_level_count);
        fail = 1;
    }

    for (i = 0; i < count; i++)
    {
        if (ic7760_caps.agc_levels[i] != expected[i])
        {
            fprintf(stderr, "agc_levels[%d]: expected %d, got %d\n", i,
                    (int) expected[i], (int) ic7760_caps.agc_levels[i]);
            fail = 1;
        }
    }

    for (i = 0; i <= HAMLIB_MAX_AGC_LEVELS
            && priv->agc_levels[i].level != RIG_AGC_LAST; i++)
    {
        if (priv->agc_levels[i].level == RIG_AGC_OFF)
        {
            fprintf(stderr, "AGC OFF maps to 16 12 %02X, which the rig"
                    " rejects\n", priv->agc_levels[i].icom_level);
            fail = 1;
        }

        if (priv->agc_levels[i].icom_level < 1
                || priv->agc_levels[i].icom_level > 3)
        {
            fprintf(stderr, "AGC level %d maps to %d, outside the 01..03 the"
                    " command accepts\n", (int) priv->agc_levels[i].level,
                    priv->agc_levels[i].icom_level);
            fail = 1;
        }
    }

    return fail;
}

/*
 * Command 0E starts the scans: 01 programmed or memory, 02 programmed,
 * 03 delta-f, 12 and 13 their fine variants, 22 memory and 23 select
 * memory.  There is no priority scan among them.
 */
static int test_scan_operations(void)
{
    scan_t expected = RIG_SCAN_MEM | RIG_SCAN_VFO | RIG_SCAN_PROG
                      | RIG_SCAN_DELTA | RIG_SCAN_SLCT;

    if (ic7760_caps.scan_ops == expected) { return 0; }

    fprintf(stderr, "scan_ops: expected %#x, got %#x%s\n",
            (unsigned) expected, (unsigned) ic7760_caps.scan_ops,
            (ic7760_caps.scan_ops & RIG_SCAN_PRIO)
            ? " (priority scan is advertised, but command 0E has no such"
            " subcommand)" : "");
    return 1;
}

/*
 * Command 10 carries the tuning step as 00 to 08, and the backend turns
 * a step in hertz into that byte through its own ts_sc_list.  Every step
 * the caps advertise has to survive that conversion, and every code the
 * rig accepts has to be advertised, or set_ts and get_ts disagree with
 * what the rig was told it can do.
 */
static int test_tuning_steps_are_reachable(void)
{
    const struct icom_priv_caps *priv = (const struct icom_priv_caps *)
                                        ic7760_caps.priv;
    unsigned char sc;
    shortfreq_t ts;
    int i;
    int fail = 0;

    for (i = 0; i < HAMLIB_TSLSTSIZ
            && !RIG_IS_TS_END(ic7760_caps.tuning_steps[i]); i++)
    {
        shortfreq_t step = ic7760_caps.tuning_steps[i].ts;

        if (icom_ts_to_sc(priv->ts_sc_list, step, &sc) != RIG_OK)
        {
            fprintf(stderr, "tuning step %ld Hz is advertised but has no"
                    " command 10 code\n", (long) step);
            fail = 1;
        }
    }

    for (sc = 0x00; sc <= 0x08; sc++)
    {
        int advertised = 0;

        if (icom_sc_to_ts(priv->ts_sc_list, sc, &ts) != RIG_OK)
        {
            fprintf(stderr, "command 10 code %02X has no tuning step\n", sc);
            fail = 1;
            continue;
        }

        for (i = 0; i < HAMLIB_TSLSTSIZ
                && !RIG_IS_TS_END(ic7760_caps.tuning_steps[i]); i++)
        {
            if (ic7760_caps.tuning_steps[i].ts == ts) { advertised = 1; }
        }

        if (!advertised)
        {
            fprintf(stderr, "command 10 code %02X is %ld Hz, which the caps do"
                    " not advertise\n", sc, (long) ts);
            fail = 1;
        }
    }

    return fail;
}

/*
 * CW pitch is 14 09, documented as 300 Hz to 900 Hz in 5 Hz steps, and
 * keying speed is 14 0C, documented as 6 to 48 WPM.  The keyer range is
 * narrower than the icom default of 4 to 60, so it stays overridden.
 */
static int test_level_granularity(void)
{
    static const struct
    {
        const char *name;
        int idx;
        int min;
        int max;
        int step;
    } grans[] =
    {
        { "CW pitch", LVL_CWPITCH, 300, 900, 5 },
        { "keying speed", LVL_KEYSPD, 6, 48, 1 },
    };
    size_t i;
    int fail = 0;

    for (i = 0; i < sizeof(grans) / sizeof(grans[0]); i++)
    {
        const gran_t *gran = &ic7760_caps.level_gran[grans[i].idx];

        if (gran->min.i != grans[i].min || gran->max.i != grans[i].max
                || gran->step.i != grans[i].step)
        {
            fprintf(stderr,
                    "%s: expected min %d, max %d, step %d; got %d, %d, %d\n",
                    grans[i].name, grans[i].min, grans[i].max, grans[i].step,
                    gran->min.i, gran->max.i, gran->step.i);
            fail = 1;
        }
    }

    return fail;
}

/*
 * The specifications give the transmit output power as 1 to 200 W in
 * SSB, CW, FM, RTTY and PSK, and 0.25 to 50 W in AM.  Every transmit
 * range has to say so, in both region lists.
 */
static int check_power_range(const freq_range_t *range, const char *list)
{
    int is_am = (range->modes & (RIG_MODE_AM | RIG_MODE_PKTAM)) != 0;
    int low = is_am ? mW(250) : W(1);
    int high = is_am ? W(50) : W(200);

    if (range->low_power == low && range->high_power == high) { return 0; }

    fprintf(stderr,
            "%s %.0f-%.0f Hz (%s): expected %d..%d mW, got %d..%d mW\n",
            list, range->startf, range->endf, is_am ? "AM" : "other",
            low, high, range->low_power, range->high_power);
    return 1;
}

static int test_transmit_power_ranges(void)
{
    static const struct
    {
        const freq_range_t *list;
        const char *name;
    } lists[] =
    {
        { ic7760_caps.tx_range_list1, "tx_range_list1" },
        { ic7760_caps.tx_range_list2, "tx_range_list2" },
    };
    size_t i;
    int j;
    int fail = 0;

    for (i = 0; i < sizeof(lists) / sizeof(lists[0]); i++)
    {
        for (j = 0; j < HAMLIB_FRQRANGESIZ
                && !RIG_IS_FRNG_END(lists[i].list[j]); j++)
        {
            fail |= check_power_range(&lists[i].list[j], lists[i].name);
        }
    }

    return fail;
}

/*
 * Reading the Id meter is 15 16, and the command table gives four
 * points along its scale: 00 00 = 0 A, 00 77 = 5 A, 01 65 = 10 A and
 * 02 41 = 15 A.
 */
static int test_id_meter_calibration(void)
{
    static const struct
    {
        int raw;
        float expected;
    } cases[] =
    {
        { 0, 0.0f },
        { 77, 5.0f },
        { 121, 7.5f },  /* halfway between the 5 A and 10 A points */
        { 165, 10.0f },
        { 241, 15.0f },
    };
    size_t i;
    int fail = 0;

    for (i = 0; i < sizeof(cases) / sizeof(cases[0]); i++)
    {
        float actual = rig_raw2val_float(cases[i].raw, &ic7760_caps.id_meter_cal);

        if (fabsf(actual - cases[i].expected) > 0.0001f)
        {
            fprintf(stderr, "Id meter at raw %d: expected %.2f A, got %.2f A\n",
                    cases[i].raw, cases[i].expected, actual);
            fail = 1;
        }
    }

    return fail;
}

/*
 * The CI-V Baud Rate menu offers 4800, 9600, 19200 and Auto, so those
 * are the ends of the range the backend may claim.  The setting applies
 * to the REMOTE jack; the USB port runs at whatever rate the host opens.
 */
static int test_serial_rates(void)
{
    int fail = 0;

    if (ic7760_caps.serial_rate_min != 4800)
    {
        fprintf(stderr, "serial_rate_min: expected 4800, got %d\n",
                ic7760_caps.serial_rate_min);
        fail = 1;
    }

    if (ic7760_caps.serial_rate_max != 19200)
    {
        fprintf(stderr, "serial_rate_max: expected 19200, got %d\n",
                ic7760_caps.serial_rate_max);
        fail = 1;
    }

    return fail;
}

/*
 * Command 16 02 offers two preamplifiers, 01 and 02.  The basic manual
 * gives P.AMP 1 as approximately 12 dB and P.AMP 2 as approximately
 * 20 dB of gain.
 */
static int test_preamp_gains(void)
{
    static const int expected[] = { 12, 20, 0 };
    size_t i;

    for (i = 0; i < sizeof(expected) / sizeof(expected[0]); i++)
    {
        if (ic7760_caps.preamp[i] != expected[i])
        {
            fprintf(stderr, "preamp %d: expected %d dB, got %d dB\n",
                    (int) i + 1, expected[i], ic7760_caps.preamp[i]);
            return 1;
        }
    }

    return 0;
}

/*
 * Command 11 carries the attenuation as BCD dB.  The rig takes 3 to 45
 * in steps of 3 and rejects anything else, which is fifteen settings,
 * and HAMLIB_MAXDBLSTSIZ has room for seven of them.  The list is a
 * subset, then, but every entry has to be a setting the rig accepts and
 * the strongest one has to be reachable.
 */
static int test_attenuator_steps(void)
{
    size_t i;
    int previous = 0;
    int fail = 0;

    for (i = 0; i < HAMLIB_MAXDBLSTSIZ && ic7760_caps.attenuator[i] != 0; i++)
    {
        int att = ic7760_caps.attenuator[i];

        if (att < 3 || att > 45 || att % 3 != 0)
        {
            fprintf(stderr, "attenuator %d dB is not a setting of command 11\n",
                    att);
            fail = 1;
        }

        if (att <= previous)
        {
            fprintf(stderr, "attenuator list is not ascending at %d dB\n", att);
            fail = 1;
        }

        previous = att;
    }

    if (i == HAMLIB_MAXDBLSTSIZ)
    {
        fprintf(stderr, "attenuator list is not terminated\n");
        fail = 1;
    }

    if (previous != 45)
    {
        fprintf(stderr, "strongest attenuator advertised is %d dB, the rig"
                " goes to 45 dB\n", previous);
        fail = 1;
    }

    return fail;
}

struct name_search
{
    const char *wanted;
    int found;
};

static int match_name(RIG *rig, const struct confparams *cfp, rig_ptr_t data)
{
    struct name_search *search = (struct name_search *) data;

    if (cfp != NULL && cfp->name != NULL
            && strcmp(cfp->name, search->wanted) == 0)
    {
        search->found = 1;
    }

    return 1;
}

/*
 * ext_tokens is a whitelist over caps->extlevels and caps->extfuncs, so
 * both tables have to be present for the declared tokens to be
 * enumerable.  DRIVE gain is 14 14, DIGI-SEL is 16 4E with its level on
 * 14 13, and the scope select is 27 12.  TX inhibit is 16 66, which the
 * rig has but the backend does not drive, so it has to stay filtered
 * out - that is what keeps the whitelist honest.
 */
static int test_ext_tokens_are_enumerable(RIG *rig)
{
    static const struct
    {
        const char *name;
        int is_func;
        int expected;
    } tokens[] =
    {
        { "drive_gain", 0, 1 },
        { "digi_sel_level", 0, 1 },
        { "digi_sel", 1, 1 },
        { "SPECTRUM_SELECT", 0, 1 },
        { "TX_INHIBIT", 1, 0 },
    };
    size_t i;
    int fail = 0;

    for (i = 0; i < sizeof(tokens) / sizeof(tokens[0]); i++)
    {
        struct name_search search = { tokens[i].name, 0 };

        if (tokens[i].is_func)
        {
            rig_ext_func_foreach(rig, match_name, &search);
        }
        else
        {
            rig_ext_level_foreach(rig, match_name, &search);
        }

        if (search.found != tokens[i].expected)
        {
            fprintf(stderr, "ext token %s: expected %s, but it is %s\n",
                    tokens[i].name,
                    tokens[i].expected ? "enumerable" : "filtered out",
                    search.found ? "enumerable" : "missing");
            fail = 1;
        }
    }

    /*
     * Listing an ext func is only half of it: rig_set_ext_func() and
     * rig_get_ext_func() answer ENAVAIL unless the backend implements
     * them, which would leave digi_sel visible but unreachable.
     */
    if (ic7760_caps.set_ext_func == NULL || ic7760_caps.get_ext_func == NULL)
    {
        fprintf(stderr, "the ext funcs are listed but cannot be set or read,"
                " so digi_sel cannot be reached\n");
        fail = 1;
    }

    return fail;
}

int main(void)
{
    RIG *rig;
    int fail = 0;

    fail |= test_vox_delay_subcommand();
    fail |= test_absent_capabilities();
    fail |= test_present_capabilities();
    fail |= test_memory_channel_counts();
    fail |= test_preamp_gains();
    fail |= test_attenuator_steps();
    fail |= test_serial_rates();
    fail |= test_id_meter_calibration();
    fail |= test_transmit_power_ranges();
    fail |= test_level_granularity();
    fail |= test_tuning_steps_are_reachable();
    fail |= test_scan_operations();
    fail |= test_agc_levels();
    fail |= test_spectrum_scope();

    rig_register(&ic7760_caps);
    rig = rig_init(RIG_MODEL_IC7760);

    if (rig == NULL)
    {
        fprintf(stderr, "rig_init failed\n");
        return 1;
    }

    fail |= test_filter_defaults(rig);
    fail |= test_ext_tokens_are_enumerable(rig);
    fail |= test_spectrum_streaming(rig);

    rig_cleanup(rig);

    return fail;
}
