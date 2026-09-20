/*
 * Hamlib Icom spectrum scope frame tests
 * Copyright (c) 2026 by Hamlib Team
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 */

/*
 * icom_parse_spectrum_frame() decodes the 27 00 waveform data for every
 * Icom with a scope.  Driven here through the IC-7300, whose caps list
 * the Fixed and SCROLL modes, with the frame sizes taken from its caps
 * rather than written out, so the same frames fit any rig sharing the
 * parser.
 */

#include <stdio.h>
#include <string.h>

#include "hamlib/rig.h"
#include "hamlib/rig_state.h"
#include "icom.h"
#include "icom_defs.h"

struct spectrum_capture
{
    int events;
    int length;
    freq_t center;
    freq_t span;
    freq_t low_edge;
    freq_t high_edge;
};

static int record_spectrum_line(RIG *rig, struct rig_spectrum_line *line,
                                rig_ptr_t data)
{
    struct spectrum_capture *capture = (struct spectrum_capture *) data;

    capture->events++;
    capture->length = line->spectrum_data_length;
    capture->center = line->center_freq;
    capture->span = line->span_freq;
    capture->low_edge = line->low_edge_freq;
    capture->high_edge = line->high_edge_freq;

    return 0;
}

/* FE FE E0 <rig> 27 00 <scope> <division> <divisions> <data...> FD */
static size_t build_scope_frame(RIG *rig, unsigned char *frame, int division,
                                int divisions, const unsigned char *data,
                                size_t data_len)
{
    const struct icom_priv_caps *priv_caps = rig->caps->priv;
    size_t len = 0;
    size_t i;

    frame[len++] = 0xfe;
    frame[len++] = 0xfe;
    frame[len++] = CTRLID;
    frame[len++] = priv_caps->re_civ_addr;
    frame[len++] = C_CTL_SCP;
    frame[len++] = S_SCP_DAT;
    frame[len++] = SCOPE_MAIN;
    frame[len++] = (unsigned char)(((division / 10) << 4) | (division % 10));
    frame[len++] = (unsigned char)(((divisions / 10) << 4) | (divisions % 10));

    for (i = 0; i < data_len; i++) { frame[len++] = data[i]; }

    frame[len++] = 0xfd;

    return len;
}

/*
 * In the Fixed and SCROLL modes the header carries the two edges instead
 * of centre and span, and the guide marks a negative lower edge with an
 * F in the 1 GHz digit, which is the high nibble of the fifth byte.  A
 * scope scrolled below 0 Hz therefore reports, for example, an edge of
 * -10 kHz as 00 00 01 00 F0.
 *
 * Over USB a sweep is a header frame followed by the points in frames
 * of single_frame_data_length; over LAN the whole line arrives in one
 * frame with a division count of one.  Returns 0 when exactly one
 * spectrum line was delivered.
 */
static int run_negative_edge_sweep(RIG *rig, int one_frame,
                                   struct spectrum_capture *capture)
{
    static const unsigned char header[] =
    {
        SCOPE_MODE_SCROLL_F,
        0x00, 0x00, 0x01, 0x00, 0xf0,   /* lower edge -10 kHz */
        0x00, 0x00, 0x04, 0x00, 0x00,   /* higher edge 40 kHz */
        0x00                            /* in range */
    };
    const struct icom_priv_caps *priv_caps = rig->caps->priv;
    const int line_length = priv_caps->spectrum_scope_caps.spectrum_line_length;
    const int per_frame = priv_caps->spectrum_scope_caps.single_frame_data_length;
    unsigned char data[HAMLIB_MAX_SPECTRUM_DATA];
    unsigned char frame[HAMLIB_MAX_SPECTRUM_DATA + 32];
    size_t len;

    memset(capture, 0, sizeof(*capture));
    memset(data, 1, sizeof(data));

    STATE(rig)->comm_state = 1;
    rig_set_spectrum_callback(rig, record_spectrum_line, capture);

    if (one_frame)
    {
        unsigned char single[sizeof(header) + HAMLIB_MAX_SPECTRUM_DATA];

        memcpy(single, header, sizeof(header));
        memcpy(single + sizeof(header), data, line_length);
        len = build_scope_frame(rig, frame, 1, 1, single,
                                sizeof(header) + line_length);
        rig->caps->process_async_frame(rig, len, frame);
    }
    else
    {
        const int divisions = 1 + (line_length + per_frame - 1) / per_frame;
        int division;
        int sent = 0;

        len = build_scope_frame(rig, frame, 1, divisions, header, sizeof(header));
        rig->caps->process_async_frame(rig, len, frame);

        for (division = 2; division <= divisions; division++)
        {
            int points = line_length - sent;

            if (points > per_frame) { points = per_frame; }

            len = build_scope_frame(rig, frame, division, divisions, data, points);
            rig->caps->process_async_frame(rig, len, frame);
            sent += points;
        }
    }

    rig_set_spectrum_callback(rig, NULL, NULL);
    STATE(rig)->comm_state = 0;

    return capture->events == 1 ? 0 : 1;
}

static int test_spectrum_negative_lower_edge(RIG *rig)
{
    static const struct { const char *path; int one_frame; } cases[] =
    {
        { "USB", 0 }, { "LAN", 1 }
    };
    const struct icom_priv_caps *priv_caps = rig->caps->priv;
    const int line_length = priv_caps->spectrum_scope_caps.spectrum_line_length;
    struct spectrum_capture capture;
    int fail = 0;
    size_t i;

    for (i = 0; i < sizeof(cases) / sizeof(cases[0]); i++)
    {
        if (run_negative_edge_sweep(rig, cases[i].one_frame, &capture) != 0)
        {
            fprintf(stderr, "%s: a SCROLL-F sweep with a negative lower edge"
                    " raised %d spectrum events, expected one\n",
                    cases[i].path, capture.events);
            fail = 1;
            continue;
        }

        if (capture.low_edge != -10000.0 || capture.high_edge != 40000.0)
        {
            fprintf(stderr, "%s: edges read %.0f/%.0f Hz, expected"
                    " -10000/40000\n", cases[i].path, capture.low_edge,
                    capture.high_edge);
            fail = 1;
        }

        if (capture.span != 50000.0 || capture.center != 15000.0)
        {
            fprintf(stderr, "%s: span/centre read %.0f/%.0f Hz, expected"
                    " 50000/15000\n", cases[i].path, capture.span,
                    capture.center);
            fail = 1;
        }

        if (capture.length != line_length)
        {
            fprintf(stderr, "%s: the sweep carried %d points, expected %d\n",
                    cases[i].path, capture.length, line_length);
            fail = 1;
        }
    }

    return fail;
}

int main(void)
{
    RIG *rig;
    int fail = 0;

    rig_register(&ic7300_caps);
    rig = rig_init(RIG_MODEL_IC7300);

    if (rig == NULL)
    {
        fprintf(stderr, "rig_init failed\n");
        return 1;
    }

    fail |= test_spectrum_negative_lower_edge(rig);

    rig_cleanup(rig);

    return fail;
}
