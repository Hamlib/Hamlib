/*
 *  Hamlib Icom CI-V frame tests
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

/* Tests for the CI-V frame helpers shared by every Icom rig (rigs/icom/frame.c),
 * serial and network alike: how a reply is matched to the command it answers,
 * what becomes of a frame the radio sends on its own in the middle of a
 * transaction, and that reading a frame keeps to the caller's buffer. */

#ifdef HAVE_CONFIG_H
#include "hamlib/config.h"
#endif

#include "acutest.h"
#include "test_debug.h"

#include <string.h>

#ifndef _WIN32
#include <sys/socket.h>
#include <unistd.h>
#endif

#include "hamlib/rig.h"
#include "hamlib/port.h"
#include "cache.h"
#include "frame.h"
#include "icom.h"
#include "icom_defs.h"

#define RADIO 0x56   /* the IC-746's default CI-V address */

/* The reply to a get echoes the command and subcommand as sent, then carries
 * the data. Built here with make_cmd_frame(), addresses swapped as the radio
 * sends them, so the matcher is checked against the encoder it must agree
 * with -- for one-, two- and three-byte subcommands alike. */
void test_reply_to_own_command_matches(void)
{
    static const int subcmds[] =
    {
        -1, 0x00, 0x05, 0xff, 0x501, 0x502, 0x514, 0x575, 0x010203
    };
    static const unsigned char data[] = { 0x80, 0x01 };
    unsigned char frame[MAXFRAMELEN];
    size_t i;

    for (i = 0; i < sizeof(subcmds) / sizeof(subcmds[0]); i++)
    {
        int len = make_cmd_frame(frame, CTRLID, RADIO, C_CTL_MEM, subcmds[i],
                                 data, sizeof(data));

        TEST_CHECK_(icom_frame_matches_cmd(C_CTL_MEM, subcmds[i], frame, len),
                    "reply to subcmd 0x%x not matched", subcmds[i]);
    }
}

/* The regression: an IC-746 S_MEM_LCD_CONT (0x501) get. The reply carries the
 * subcommand as the two bytes 05 01, and comparing 0x501 with the single byte
 * 0x05 rejected every such reply until the read timed out. */
void test_multibyte_subcmd_reply_matches(void)
{
    static const unsigned char reply[] =
    {
        PR, PR, CTRLID, RADIO, C_CTL_MEM, 0x05, 0x01, 0x01, 0x28, FI
    };

    TEST_CHECK(icom_frame_matches_cmd(C_CTL_MEM, 0x501, reply,
                                      sizeof(reply)));
}

/* A frame answering something else is skipped, so the transaction reads on:
 * another command, a different subcommand, or a different first byte of a
 * multi-byte one. */
void test_other_replies_do_not_match(void)
{
    unsigned char frame[MAXFRAMELEN];
    int len;

    len = make_cmd_frame(frame, CTRLID, RADIO, C_CTL_MEM, 0x502, NULL, 0);
    TEST_CHECK(!icom_frame_matches_cmd(C_CTL_MEM, 0x501, frame, len));
    TEST_CHECK(!icom_frame_matches_cmd(C_CTL_MEM, 0x02, frame, len));
    TEST_CHECK(!icom_frame_matches_cmd(C_CTL_MEM, 0x602, frame, len));
    TEST_CHECK(!icom_frame_matches_cmd(C_RD_FREQ, -1, frame, len));

    len = make_cmd_frame(frame, CTRLID, RADIO, C_CTL_MEM, 0x05, NULL, 0);
    TEST_CHECK(!icom_frame_matches_cmd(C_CTL_MEM, 0x06, frame, len));
}

/* No subcommand to compare: the command alone decides. */
void test_no_subcmd_matches_on_cmd(void)
{
    unsigned char frame[MAXFRAMELEN];
    static const unsigned char freq[] = { 0x00, 0x00, 0x07, 0x14, 0x00 };
    int len = make_cmd_frame(frame, CTRLID, RADIO, C_RD_FREQ, -1, freq,
                             sizeof(freq));

    TEST_CHECK(icom_frame_matches_cmd(C_RD_FREQ, -1, frame, len));
}

/* Too short to hold a command byte: never a match, and never read past. */
void test_short_frame_does_not_match(void)
{
    static const unsigned char frame[] = { PR, PR, CTRLID, RADIO, C_CTL_MEM };

    TEST_CHECK(!icom_frame_matches_cmd(C_CTL_MEM, -1, frame, 4));
    TEST_CHECK(icom_frame_matches_cmd(C_CTL_MEM, -1, frame, 5));
}

/* A socket pair stands in for the radio's USB port; Windows has no
 * socketpair(). */
#ifndef _WIN32

#define IC7300 0x94  /* the IC-7300's default CI-V address */
#define SCOPE_LINE_LENGTH 475   /* IC-7300 over USB: 9 x 50 + 25 */

struct spectrum_seen
{
    int lines;
    size_t length;
    unsigned char data[SCOPE_LINE_LENGTH];
};

static int on_spectrum(RIG *rig, struct rig_spectrum_line *line, rig_ptr_t arg)
{
    struct spectrum_seen *seen = arg;

    (void) rig;
    seen->lines++;
    seen->length = line->spectrum_data_length;

    if (line->spectrum_data_length <= sizeof(seen->data))
    {
        memcpy(seen->data, line->spectrum_data, line->spectrum_data_length);
    }

    return RIG_OK;
}

/* One `27 00` division to the controller: main scope, division `division` of
 * `max_division`. The first carries the center mode and 14.100 MHz +/- 25 kHz;
 * every division also carries `length` bytes starting at `data`. */
static size_t scope_division(unsigned char *frame, int division,
                             int max_division, const unsigned char *data,
                             size_t length)
{
    static const unsigned char header[] =
    {
        0x00,                           /* center mode */
        0x00, 0x00, 0x10, 0x14, 0x00,   /* 14.100000 MHz */
        0x00, 0x50, 0x02, 0x00, 0x00,   /* +/- 25 kHz */
        0x00                            /* in range */
    };
    size_t n = 0;

    frame[n++] = PR;
    frame[n++] = PR;
    frame[n++] = CTRLID;
    frame[n++] = IC7300;
    frame[n++] = C_CTL_SCP;
    frame[n++] = S_SCP_DAT;
    frame[n++] = 0x00;                                  /* main scope */
    frame[n++] = (unsigned char)((division / 10) << 4 | division % 10);
    frame[n++] = (unsigned char)((max_division / 10) << 4 | max_division % 10);

    if (division == 1)
    {
        memcpy(frame + n, header, sizeof(header));
        n += sizeof(header);
    }

    if (length > 0)
    {
        memcpy(frame + n, data, length);
        n += length;
    }

    frame[n++] = FI;
    return n;
}

/* What the radio returns for an `03` frequency read on a port that echoes:
 * the command itself, then the answer, 14.100000 MHz. */
static const unsigned char freq_echo[] =
{
    PR, PR, IC7300, CTRLID, C_RD_FREQ, FI
};
static const unsigned char freq_answer[] =
{
    PR, PR, CTRLID, IC7300, C_RD_FREQ, 0x00, 0x00, 0x10, 0x14, 0x00, FI
};

/* An IC-7300 whose USB port echoes, on one end of a socket pair. */
static RIG *echoing_ic7300(int fd)
{
    RIG *rig = rig_init(RIG_MODEL_IC7300);
    struct icom_priv_data *priv;

    if (rig == NULL)
    {
        return NULL;
    }

    STATE(rig)->comm_state = 1;
    rig_set_current_vfo_state(rig, RIG_VFO_A);
    RIGPORT(rig)->fd = fd;
    RIGPORT(rig)->type.rig = RIG_PORT_NETWORK;
    RIGPORT(rig)->retry = 0;
    RIGPORT(rig)->timeout = 100;
    priv = STATE(rig)->priv;
    priv->serial_USB_echo_off = 0;
    return rig;
}

static void release_ic7300(RIG *rig, int sockets[2])
{
    STATE(rig)->comm_state = 0;
    RIGPORT(rig)->fd = -1;
    rig_cleanup(rig);
    close(sockets[0]);
    close(sockets[1]);
}

/* Run the `03` read and check it got its own answer. */
static void check_freq_read(RIG *rig)
{
    unsigned char reply[MAXFRAMELEN];
    int reply_len = 0;

    TEST_CHECK(icom_transaction(rig, C_RD_FREQ, -1, NULL, 0, reply,
                                &reply_len) == RIG_OK);
    TEST_CHECK_(reply_len == 6 && memcmp(reply, freq_answer + 4, 6) == 0,
                "answer of %d bytes", reply_len);
}

/* The regression: with "CI-V USB Echo Back" on, the radio can push a whole
 * scope line between a command and its echo. Each division read there was
 * parsed with the length of the frame sent instead of the one read; after a
 * 6-byte command such as the `03` frequency read that length wraps, and the
 * third division copied about SIZE_MAX bytes. The line must come through
 * whole, and the read must still get its own answer. */
void test_scope_line_during_echo_read(void)
{
    unsigned char line[SCOPE_LINE_LENGTH];
    unsigned char frame[MAXFRAMELEN];
    struct spectrum_seen seen;
    int sockets[2];
    int division;
    size_t i;
    RIG *rig;

    for (i = 0; i < sizeof(line); i++)
    {
        line[i] = (unsigned char)(i % 0xa0);
    }

    TEST_ASSERT(socketpair(AF_UNIX, SOCK_STREAM, 0, sockets) == 0);

    for (division = 1; division <= 11; division++)
    {
        size_t offset = division > 1 ? (size_t)(division - 2) * 50 : 0;
        size_t length = division == 1 ? 0 : division == 11 ? 25 : 50;
        size_t n = scope_division(frame, division, 11, line + offset, length);

        TEST_ASSERT(write(sockets[1], frame, n) == (ssize_t) n);
    }

    TEST_ASSERT(write(sockets[1], freq_echo, sizeof(freq_echo))
                == sizeof(freq_echo));
    TEST_ASSERT(write(sockets[1], freq_answer, sizeof(freq_answer))
                == sizeof(freq_answer));

    rig = echoing_ic7300(sockets[0]);
    TEST_ASSERT(rig != NULL);
    memset(&seen, 0, sizeof(seen));
    TEST_ASSERT(rig_set_spectrum_callback(rig, on_spectrum, &seen) == RIG_OK);

    check_freq_read(rig);

    TEST_CHECK(seen.lines == 1);
    TEST_CHECK_(seen.length == sizeof(line), "line length %zu", seen.length);
    TEST_CHECK(seen.length == sizeof(line)
               && memcmp(seen.data, line, sizeof(line)) == 0);

    release_ic7300(rig, sockets);
}

/* A line that fits in one division still carries its samples after the
 * metadata, and must still be published. */
void test_single_division_scope_line_is_published(void)
{
    unsigned char line[SCOPE_LINE_LENGTH];
    unsigned char frame[MAXFRAMELEN];
    struct spectrum_seen seen;
    int sockets[2];
    size_t i;
    size_t n;
    RIG *rig;

    for (i = 0; i < sizeof(line); i++)
    {
        line[i] = (unsigned char)(i % 0xa0);
    }

    TEST_ASSERT(socketpair(AF_UNIX, SOCK_STREAM, 0, sockets) == 0);

    n = scope_division(frame, 1, 1, line, 50);
    TEST_ASSERT(write(sockets[1], frame, n) == (ssize_t) n);
    TEST_ASSERT(write(sockets[1], freq_echo, sizeof(freq_echo))
                == sizeof(freq_echo));
    TEST_ASSERT(write(sockets[1], freq_answer, sizeof(freq_answer))
                == sizeof(freq_answer));

    rig = echoing_ic7300(sockets[0]);
    TEST_ASSERT(rig != NULL);
    memset(&seen, 0, sizeof(seen));
    TEST_ASSERT(rig_set_spectrum_callback(rig, on_spectrum, &seen) == RIG_OK);

    check_freq_read(rig);

    TEST_CHECK(seen.lines == 1);
    TEST_CHECK_(seen.length == 50, "line length %zu", seen.length);
    TEST_CHECK(seen.length == 50 && memcmp(seen.data, line, 50) == 0);

    release_ic7300(rig, sockets);
}

/* A multipart scope header cannot also carry data. It previously produced a
 * negative destination offset and wrote before the spectrum buffer. */
void test_scope_data_in_first_division_is_rejected(void)
{
    unsigned char line[SCOPE_LINE_LENGTH] = { 0 };
    unsigned char frame[MAXFRAMELEN];
    struct spectrum_seen seen;
    int sockets[2];
    int division;
    RIG *rig;

    TEST_ASSERT(socketpair(AF_UNIX, SOCK_STREAM, 0, sockets) == 0);

    for (division = 1; division <= 11; division++)
    {
        size_t offset = division > 1 ? (size_t)(division - 2) * 50 : 0;
        size_t length = division == 11 ? 25 : 50;
        size_t n = scope_division(frame, division, 11, line + offset, length);

        TEST_ASSERT(write(sockets[1], frame, n) == (ssize_t) n);
    }

    TEST_ASSERT(write(sockets[1], freq_echo, sizeof(freq_echo))
                == sizeof(freq_echo));
    TEST_ASSERT(write(sockets[1], freq_answer, sizeof(freq_answer))
                == sizeof(freq_answer));

    rig = echoing_ic7300(sockets[0]);
    TEST_ASSERT(rig != NULL);
    memset(&seen, 0, sizeof(seen));
    TEST_ASSERT(rig_set_spectrum_callback(rig, on_spectrum, &seen) == RIG_OK);

    check_freq_read(rig);

    TEST_CHECK(seen.lines == 0);

    release_ic7300(rig, sockets);
}

/* A lone FC, the jam byte of a CI-V collision, read before the echo. Taken
 * with the length of the command sent, it passed for a broadcast frequency
 * frame and set the frequency to 0 Hz. It is no frame at all: the read must
 * go on to the echo and the answer and leave the frequency alone. */
void test_collision_byte_during_echo_read(void)
{
    static const unsigned char collision[] = { COL };
    freq_t freq = 0;
    int cache_ms = 0;
    int sockets[2];
    RIG *rig;

    TEST_ASSERT(socketpair(AF_UNIX, SOCK_STREAM, 0, sockets) == 0);
    TEST_ASSERT(write(sockets[1], collision, sizeof(collision))
                == sizeof(collision));
    TEST_ASSERT(write(sockets[1], freq_echo, sizeof(freq_echo))
                == sizeof(freq_echo));
    TEST_ASSERT(write(sockets[1], freq_answer, sizeof(freq_answer))
                == sizeof(freq_answer));

    rig = echoing_ic7300(sockets[0]);
    TEST_ASSERT(rig != NULL);
    TEST_ASSERT(rig_set_cache_freq(rig, RIG_VFO_A, 14100000) == RIG_OK);

    check_freq_read(rig);

    TEST_CHECK(rig_get_cache_freq(rig, RIG_VFO_A, &freq, &cache_ms) == RIG_OK);
    TEST_CHECK_(freq == 14100000, "cached frequency %.0f Hz", freq);

    release_ic7300(rig, sockets);
}

/* The regression: once the `03` probe of the USB echo check gets a one-byte
 * answer, the check takes it for an echo and reads the next frame into a
 * 16-byte buffer, while a frame can be as long as a scope division. The
 * frame must be read whole, so the next command gets its own reply. */
void test_echo_check_reads_whole_frame(void)
{
    /* The probe's echo with a preamble byte lost: the transaction restores
     * the byte and returns the 03 as a one-byte answer. */
    static const unsigned char probe_echo[] =
    {
        PR, IC7300, CTRLID, C_RD_FREQ, FI
    };
    /* The frequency read that follows, on the now echoing port. */
    static const unsigned char x25_echo[] =
    {
        PR, PR, IC7300, CTRLID, C_SEND_SEL_FREQ, 0x00, FI
    };
    static const unsigned char x25_answer[] =
    {
        PR, PR, CTRLID, IC7300, C_SEND_SEL_FREQ, 0x00,
        0x00, 0x00, 0x10, 0x14, 0x00, FI
    };
    unsigned char line[50] = { 0 };
    unsigned char frame[MAXFRAMELEN];
    struct icom_priv_data *priv;
    int sockets[2];
    freq_t freq = 0;
    size_t n;
    RIG *rig;

    TEST_ASSERT(socketpair(AF_UNIX, SOCK_STREAM, 0, sockets) == 0);
    TEST_ASSERT(write(sockets[1], probe_echo, sizeof(probe_echo))
                == sizeof(probe_echo));
    n = scope_division(frame, 2, 11, line, sizeof(line));
    TEST_ASSERT(n > 16);
    TEST_ASSERT(write(sockets[1], frame, n) == (ssize_t) n);
    TEST_ASSERT(write(sockets[1], x25_echo, sizeof(x25_echo))
                == sizeof(x25_echo));
    TEST_ASSERT(write(sockets[1], x25_answer, sizeof(x25_answer))
                == sizeof(x25_answer));

    rig = echoing_ic7300(sockets[0]);
    TEST_ASSERT(rig != NULL);
    priv = STATE(rig)->priv;
    priv->serial_USB_echo_off = -1;   /* not known yet */

    TEST_CHECK(icom_get_freq(rig, RIG_VFO_CURR, &freq) == RIG_OK);
    TEST_CHECK_(freq == 14100000, "frequency %.0f Hz", freq);

    release_ic7300(rig, sockets);
}

/* Room around a buffer handed to the frame reader, filled with a marker so
 * that any byte written past the buffer shows. */
#define AROUND 1100
#define MARKER 0xa5

static int untouched(const unsigned char *p, size_t n)
{
    size_t i;

    for (i = 0; i < n; i++)
    {
        if (p[i] != MARKER) { return 0; }
    }

    return 1;
}

/* The regression: the reader let each read fill MAXFRAMELEN bytes whatever
 * buffer it was given, so a 16-byte buffer had the 184 bytes behind it
 * cleared. A frame that fits must come through whole, and nothing past the
 * buffer may change. */
void test_frame_read_into_small_buffer(void)
{
    unsigned char area[AROUND];
    int sockets[2];
    int n;
    RIG *rig;

    TEST_ASSERT(socketpair(AF_UNIX, SOCK_STREAM, 0, sockets) == 0);
    TEST_ASSERT(write(sockets[1], freq_answer, sizeof(freq_answer))
                == sizeof(freq_answer));
    rig = echoing_ic7300(sockets[0]);
    TEST_ASSERT(rig != NULL);

    memset(area, MARKER, sizeof(area));
    n = read_icom_frame(RIGPORT(rig), area, 16);

    TEST_CHECK_(n == sizeof(freq_answer), "read %d bytes", n);
    TEST_CHECK(memcmp(area, freq_answer, sizeof(freq_answer)) == 0);
    TEST_CHECK(untouched(area + 16, sizeof(area) - 16));

    release_ic7300(rig, sockets);
}

/* A buffer of 0 or 1 byte holds no frame: nothing is read, nothing past it
 * is written, and the frame stays for the next read. */
void test_frame_read_into_tiny_buffer(void)
{
    unsigned char area[AROUND];
    size_t length;
    int sockets[2];
    int n;
    RIG *rig;

    TEST_ASSERT(socketpair(AF_UNIX, SOCK_STREAM, 0, sockets) == 0);
    rig = echoing_ic7300(sockets[0]);
    TEST_ASSERT(rig != NULL);

    for (length = 0; length <= 1; length++)
    {
        TEST_ASSERT(write(sockets[1], freq_answer, sizeof(freq_answer))
                    == sizeof(freq_answer));

        memset(area, MARKER, sizeof(area));
        n = read_icom_frame(RIGPORT(rig), area, length);

        TEST_CHECK_(n == 0, "%zu-byte buffer: read %d bytes", length, n);
        TEST_CHECK_(untouched(area + length, sizeof(area) - length),
                    "%zu-byte buffer: written past it", length);

        n = read_icom_frame(RIGPORT(rig), area, MAXFRAMELEN);
        TEST_CHECK_(n == sizeof(freq_answer)
                    && memcmp(area, freq_answer, sizeof(freq_answer)) == 0,
                    "%zu-byte buffer: the frame after it is %d bytes",
                    length, n);
    }

    release_ic7300(rig, sockets);
}

/* A buffer larger than MAXFRAMELEN takes a longer frame whole; the async
 * reader thread reads into 1024 bytes through the direct path. */
void test_long_frame_read_direct(void)
{
    unsigned char frame[300];
    unsigned char area[AROUND];
    int sockets[2];
    int n;
    RIG *rig;

    memset(frame, 0x11, sizeof(frame));
    frame[0] = PR;
    frame[1] = PR;
    frame[2] = CTRLID;
    frame[3] = IC7300;
    frame[sizeof(frame) - 1] = FI;

    TEST_ASSERT(socketpair(AF_UNIX, SOCK_STREAM, 0, sockets) == 0);
    TEST_ASSERT(write(sockets[1], frame, sizeof(frame)) == sizeof(frame));
    rig = echoing_ic7300(sockets[0]);
    TEST_ASSERT(rig != NULL);

    memset(area, MARKER, sizeof(area));
    n = read_icom_frame_direct(RIGPORT(rig), area, 1024);

    TEST_CHECK_(n == sizeof(frame), "read %d bytes", n);
    TEST_CHECK(memcmp(area, frame, sizeof(frame)) == 0);
    TEST_CHECK(untouched(area + 1024, sizeof(area) - 1024));

    release_ic7300(rig, sockets);
}

#endif /* !_WIN32 */

TEST_LIST =
{
    { "reply_to_own_command_matches", test_reply_to_own_command_matches },
    { "multibyte_subcmd_reply_matches", test_multibyte_subcmd_reply_matches },
    { "other_replies_do_not_match",   test_other_replies_do_not_match },
    { "no_subcmd_matches_on_cmd",     test_no_subcmd_matches_on_cmd },
    { "short_frame_does_not_match",   test_short_frame_does_not_match },
#ifndef _WIN32
    { "scope_line_during_echo_read",  test_scope_line_during_echo_read },
    {
        "single_division_scope_line_is_published",
        test_single_division_scope_line_is_published
    },
    {
        "scope_data_in_first_division_is_rejected",
        test_scope_data_in_first_division_is_rejected
    },
    { "collision_byte_during_echo_read", test_collision_byte_during_echo_read },
    { "echo_check_reads_whole_frame", test_echo_check_reads_whole_frame },
    { "frame_read_into_small_buffer", test_frame_read_into_small_buffer },
    { "frame_read_into_tiny_buffer", test_frame_read_into_tiny_buffer },
    { "long_frame_read_direct",       test_long_frame_read_direct },
#endif
    { NULL, NULL }
};
