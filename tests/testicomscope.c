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
#include "hamlib/port.h"
#include "hamlib/rig_state.h"
#include "icom.h"
#include "icom_defs.h"
#include "cache.h"
#include "frame.h"
#include "testicomsock.h"

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


struct fixed_edge_case
{
    const char *name;
    rig_model_t model;
    unsigned char range;
    size_t edge_bytes;
    unsigned char tuned[6];
    unsigned char edges[12];
    setting_t level;
    int expected;
    int expected_result;
    int truncate_reply;
    int new_high_khz;
    unsigned char new_high[6];
};

struct fixed_edge_peer
{
    const struct fixed_edge_case *test;
    int fd;
    unsigned char address;
    int bad_frame;
    int writes;
};

static void *run_fixed_edge_peer(void *arg)
{
    struct fixed_edge_peer *peer = arg;
    const struct fixed_edge_case *test = peer->test;
    const unsigned char preset[] = { C_CTL_SCP, S_SCP_EDG, SCOPE_MAIN };
    const unsigned char query[] = { C_CTL_SCP, S_SCP_FEF, test->range, 0x02 };
    unsigned char edges[sizeof(test->edges)];
    unsigned char request[MAXFRAMELEN];
    size_t length;

    memcpy(edges, test->edges, test->edge_bytes * 2);

    while (read_frame(peer->fd, request, sizeof(request), &length) == 0)
    {
        unsigned char reply[32] = { 0xfe, 0xfe, CTRLID, peer->address };
        const unsigned char *payload = request + 4;
        size_t count = 4;

        if (length < 6 || request[0] != 0xfe || request[1] != 0xfe
                || request[2] != peer->address || request[3] != CTRLID)
        {
            peer->bad_frame = 1;
            reply[count++] = NAK;
        }
        else if ((length == 6 && payload[0] == C_RD_FREQ)
                 || (length == 7 && payload[0] == C_SEND_SEL_FREQ))
        {
            size_t command_bytes = length - 5;
            memcpy(reply + count, payload, command_bytes);
            count += command_bytes;
            memcpy(reply + count, test->tuned, test->edge_bytes);
            count += test->edge_bytes;
        }
        else if (length == sizeof(preset) + 5
                 && memcmp(payload, preset, sizeof(preset)) == 0)
        {
            memcpy(reply + count, preset, sizeof(preset));
            count += sizeof(preset);
            reply[count++] = query[3];
        }
        else if (length >= sizeof(query) + 5
                 && memcmp(payload, query, sizeof(query)) == 0)
        {
            if (length == sizeof(query) + 5)
            {
                size_t edge_bytes = 2 * test->edge_bytes - test->truncate_reply;
                memcpy(reply + count, query, sizeof(query));
                count += sizeof(query);
                memcpy(reply + count, edges, edge_bytes);
                count += edge_bytes;
            }
            else if (test->new_high_khz != 0
                     && length == sizeof(query) + 5 + 2 * test->edge_bytes
                     && memcmp(payload + sizeof(query), test->edges,
                               test->edge_bytes) == 0
                     && memcmp(payload + sizeof(query) + test->edge_bytes,
                               test->new_high, test->edge_bytes) == 0)
            {
                memcpy(edges + test->edge_bytes, test->new_high, test->edge_bytes);
                peer->writes++;
                reply[count++] = ACK;
            }
            else
            {
                peer->bad_frame = 1;
                reply[count++] = NAK;
            }
        }
        else
        {
            peer->bad_frame = 1;
            reply[count++] = NAK;
        }

        reply[count++] = 0xfd;

        if (write_all(peer->fd, reply, count) != 0)
        {
            break;
        }
    }

    return NULL;
}

static int test_fixed_edge_case(const struct fixed_edge_case *test)
{
    RIG *rig = rig_init(test->model);
    int sockets[2];
    pthread_t thread;
    int failed = 0;

    if (rig == NULL)
    {
        fprintf(stderr, "%s: rig_init failed\n", test->name);
        return 1;
    }

    if (open_test_connection(sockets) != 0)
    {
        fprintf(stderr, "%s: test socket setup failed\n", test->name);
        rig_cleanup(rig);
        return 1;
    }

    const struct icom_priv_caps *caps = rig->caps->priv;
    struct fixed_edge_peer peer = { .test = test, .fd = sockets[1],
                                    .address = caps->re_civ_addr
                                  };

    if (pthread_create(&thread, NULL, run_fixed_edge_peer, &peer) != 0)
    {
        fprintf(stderr, "%s: test peer setup failed\n", test->name);
        close_test_socket(sockets[0]);
        close_test_socket(sockets[1]);
        rig_cleanup(rig);
        return 1;
    }

    RIGPORT(rig)->fd = sockets[0];
    RIGPORT(rig)->type.rig = RIG_PORT_NETWORK;
    RIGPORT(rig)->timeout = 2000;
    RIGPORT(rig)->retry = 0;
    ((struct icom_priv_data *) STATE(rig)->priv)->serial_USB_echo_off = 1;
    STATE(rig)->comm_state = 1;
    rig_set_current_vfo_state(rig, RIG_VFO_A);

    value_t value = { .i = 0 };
    int rc = rig_get_level(rig, RIG_VFO_CURR, test->level, &value);

    if (rc != test->expected_result
            || (rc == RIG_OK && value.i != test->expected))
    {
        fprintf(stderr, "%s: get returned %d, value %d; expected %d, value %d\n",
                test->name, rc, value.i, test->expected_result, test->expected);
        failed = 1;
    }

    if (test->new_high_khz != 0)
    {
        value.i = test->new_high_khz;
        rc = rig_set_level(rig, RIG_VFO_CURR, RIG_LEVEL_SPECTRUM_EDGE_HIGH_KHZ, value);

        if (rc != RIG_OK)
        {
            fprintf(stderr, "%s: set high edge failed: %s\n",
                    test->name, rigerror(rc));
            failed = 1;
        }

        value.i = 0;
        rc = rig_get_level(rig, RIG_VFO_CURR, RIG_LEVEL_SPECTRUM_EDGE_HIGH_KHZ, &value);

        if (rc != RIG_OK || value.i != test->new_high_khz)
        {
            fprintf(stderr, "%s: high-edge readback returned %d, value %d; expected %d kHz\n",
                    test->name, rc, value.i, test->new_high_khz);
            failed = 1;
        }
    }

    stop_test_peer(sockets, thread);

    if (peer.bad_frame || peer.writes != (test->new_high_khz != 0))
    {
        fprintf(stderr, "%s: unexpected CI-V request or missing complete edge write\n",
                test->name);
        failed = 1;
    }

    STATE(rig)->comm_state = 0;
    RIGPORT(rig)->fd = -1;
    rig_cleanup(rig);
    return failed;
}

static int test_fixed_edge_levels(void)
{
    static const struct fixed_edge_case cases[] =
    {
        {
            .name = "IC-7300 legacy Hz",
            .model = RIG_MODEL_IC7300, .range = 0x06, .edge_bytes = 5,
            .tuned = { 0x00, 0x00, 0x10, 0x14, 0x00 },
            .edges = { 0x23, 0x01, 0x00, 0x14, 0x00, 0x87, 0x09, 0x35, 0x14, 0x00 },
            .level = RIG_LEVEL_SPECTRUM_EDGE_LOW, .expected = 14000123,
        },
        {
            .name = "IC-905 144 MHz legacy Hz",
            .model = RIG_MODEL_IC905, .range = 0x01, .edge_bytes = 5,
            .tuned = { 0x00, 0x00, 0x00, 0x45, 0x01 },
            .edges = { 0x00, 0x00, 0x10, 0x44, 0x01, 0x00, 0x90, 0x99, 0x47, 0x01 },
            .level = RIG_LEVEL_SPECTRUM_EDGE_HIGH, .expected = 147999000,
        },
        {
            .name = "IC-905 2.4 GHz five-byte kHz",
            .model = RIG_MODEL_IC905, .range = 0x04, .edge_bytes = 5,
            .tuned = { 0x00, 0x00, 0x10, 0x00, 0x24 },
            .edges = { 0x00, 0x00, 0x05, 0x00, 0x24, 0x00, 0x30, 0x20, 0x00, 0x24 },
            .level = RIG_LEVEL_SPECTRUM_EDGE_HIGH_KHZ, .expected = 2400203,
        },
        {
            .name = "IC-905 2.4 GHz legacy Hz overflow",
            .model = RIG_MODEL_IC905, .range = 0x04, .edge_bytes = 5,
            .tuned = { 0x00, 0x00, 0x10, 0x00, 0x24 },
            .edges = { 0x00, 0x00, 0x05, 0x00, 0x24, 0x00, 0x30, 0x20, 0x00, 0x24 },
            .level = RIG_LEVEL_SPECTRUM_EDGE_HIGH, .expected_result = -RIG_EDOM,
        },
        {
            .name = "IC-905 10 GHz six-byte kHz",
            .model = RIG_MODEL_IC905, .range = 0x06, .edge_bytes = 6,
            .tuned = { 0x00, 0x00, 0x00, 0x10, 0x00, 0x01 },
            .edges = { 0x00, 0x20, 0x01, 0x00, 0x00, 0x01,
                       0x00, 0x40, 0x23, 0x25, 0x00, 0x01
                     },
            .level = RIG_LEVEL_SPECTRUM_EDGE_LOW_KHZ, .expected = 10000012,
            .new_high_khz = 10025321,
            .new_high = { 0x00, 0x10, 0x32, 0x25, 0x00, 0x01 },
        },
        {
            .name = "IC-905 short 10 GHz edge reply",
            .model = RIG_MODEL_IC905, .range = 0x06, .edge_bytes = 6,
            .tuned = { 0x00, 0x00, 0x00, 0x10, 0x00, 0x01 },
            .edges = { 0x00, 0x20, 0x01, 0x00, 0x00, 0x01,
                       0x00, 0x40, 0x23, 0x25, 0x00, 0x01
                     },
            .level = RIG_LEVEL_SPECTRUM_EDGE_HIGH_KHZ,
            .expected_result = -RIG_EPROTO, .truncate_reply = 1,
        },
    };
    int failed = 0;

#ifdef _WIN32
    WSADATA wsa_data;

    if (WSAStartup(MAKEWORD(2, 2), &wsa_data) != 0)
    {
        fprintf(stderr, "WSAStartup failed\n");
        return 1;
    }

#endif

    rig_register(&ic905_caps);

    for (size_t i = 0; i < sizeof(cases) / sizeof(cases[0]); i++)
    {
        failed |= test_fixed_edge_case(&cases[i]);
    }

#ifdef _WIN32
    WSACleanup();
#endif
    return failed;
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
    fail |= test_fixed_edge_levels();

    return fail;
}
