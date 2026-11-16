/*
 *  Hamlib Harris backend - RF-5800H (AN/PRC-150 type) model
 *  Copyright (c) 2026 by Cam Hosken, W6HOS
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

#ifndef _RF5800_H
#define _RF5800_H

#include <stddef.h>

#include "hamlib/rig.h"

#define RF5800_MAX_LINES 32
#define RF5800_LINE_LEN  80

/* every builder needs a buffer of at least this; else -RIG_EINVAL */
#define RF5800_CMD_MIN 32

/* the radio reports at most 60 MHz; a parsed frequency beyond this is a
 * malformed reply, not a frequency */
#define RF5800_PARSE_HZ_MAX 100000000

/* the wait of each read that drains stale bytes before a write; 0 would
 * never see queued bytes on a Windows COM port (win32_serial_select) */
#define RF5800_DRAIN_WAIT_MS 1

/* the SSB mode word from ALE>, HOP> or 3G> and a tune each answer only
 * when they end, so their budgets are their own, not a read's 3 s */
#define RF5800_SSB_MS    30000
#define RF5800_RETUNE_MS 60000

/* rf5800_classify_open verdicts */
#define RF5800_OPEN_OK          0
#define RF5800_OPEN_PRC138      1
#define RF5800_OPEN_ECHO_OFF    2
#define RF5800_OPEN_NO_IDENTITY 3

struct rf5800_reply
{
    const char
    *cmd;                 /* set by the caller: the command sent, no CR */
    char  partial[RF5800_LINE_LEN];  /* the line being assembled across chunks */
    char  lines[RF5800_MAX_LINES][RF5800_LINE_LEN]; /* data lines, in order */
    int   nlines;                    /* lines kept; excess lines are dropped and counted */
    int   dropped;
    char  prompt[8];                 /* "SSB>" ... once the prompt line arrived, else "" */
    int   done;                      /* 1 once prompt[] is set */
    int   echo_seen;                 /* the echo line was consumed */
    int   echo_missing;              /* a data line arrived before any echo */
    int   error;                     /* ERROR: nnnn -> nnnn, else 0 */
    char  error_text[64];
    int   list_closed;               /* END-OF-LIST seen */
    int   push_channel;              /* "* CURRENT CHAN n:" -> n, else -1 */
    int   push_manual;               /* "* PRESET CHANGED TO MANUAL" -> 1, other name -> 0, none -> -1 */
    int   retune;                    /* "* RETUNE ID: n COMPLETE" -> 1, FAILED -> 2, none -> 0 */
    int   exception;                 /* "* EXCEPTION: nnnn" -> nnnn, else 0 */
    int   overlong;                  /* lines truncated to RF5800_LINE_LEN-1 */
    int   discard;                   /* dropping an overlong line's tail up to the next \n */
    int   partial_from_carry;        /* partial's first byte was fed with from_carry */
};

/* agc: RIG_AGC_* value or -1; number -1 when the row is missing */
struct rf5800_chan
{
    freq_t rx_hz, tx_hz;
    rmode_t mode;
    pbwidth_t width;
    int agc;
    int number;
};

/* squelch 0/1, sq_level 0 LOW 1 MED 2 HIGH, rfgain 0..100; -1 = row missing */
struct rf5800_radio
{
    int squelch;
    int sq_level;
    int rfgain;
};

/* from PRESET CURRENT NAME / CHAN; -1 = missing */
struct rf5800_preset
{
    int manual;
    int channel;
};

struct rf5800_priv
{
    char prompt[8];                  /* the last completing prompt */
    int  channel;                    /* last known channel, -1 unknown */
    int  manual;                     /* last known MANUAL state, -1 unknown */
    char identity[32];
    char carry[RF5800_LINE_LEN];     /* text after the last prompt, seeds the next partial */
    int  need_sync;                  /* a blank-line Sync precedes the next command */
    int  carry_ms;                   /* longest wait for SSB> after the SSB word, ms */
    int  carry_poll_ms;              /* the sleep before each blank of that wait, ms */
};

/* One row of the command grammar: the fixed words, then the argument pattern
 * (NULL when the line is the fixed words alone). */
struct rf5800_grammar
{
    const char *verb;
    const char *args;
};

extern const struct rf5800_grammar rf5800_command_list[];
extern const int rf5800_command_count;

extern struct rig_caps rf5800_caps;

/* The functions below are not static because the unit tests of these models
 * call them. */

/* Pure functions: no I/O, no RIG * */
int rf5800_reply_init(struct rf5800_reply *r, const char *cmd);
int rf5800_feed(struct rf5800_reply *r, const char *bytes, size_t len,
                int from_carry);
int rf5800_format_freq(freq_t hz, char *buf, size_t n);
int rf5800_cmd_freq(freq_t hz, char *buf, size_t n);
int rf5800_cmd_txfreq(freq_t hz, char *buf, size_t n);
int rf5800_cmd_mode(rmode_t mode, char *buf, size_t n);
int rf5800_snap_width(rmode_t mode, pbwidth_t width, int *hz);
int rf5800_cmd_bandwidth(int hz, char *buf, size_t n);
int rf5800_cmd_agc(int rig_agc, char *buf, size_t n);
int rf5800_agc_from_word(const char *word, int *rig_agc);
int rf5800_cmd_power(float level, char *buf, size_t n);
int rf5800_power_from_word(const char *word, float *level);
int rf5800_cmd_squelch(int on, char *buf, size_t n);
int rf5800_cmd_sq_level(float level, char *buf, size_t n);
int rf5800_cmd_rfgain(float level, char *buf, size_t n);
int rf5800_parse_chan(const struct rf5800_reply *r, struct rf5800_chan *c);
int rf5800_parse_radio(const struct rf5800_reply *r, struct rf5800_radio *rd);
int rf5800_parse_power(const struct rf5800_reply *r, float *level);
int rf5800_parse_preset(const struct rf5800_reply *r,
                        struct rf5800_preset *p);
int rf5800_parse_identity(const struct rf5800_reply *r, char *out, size_t n);
int rf5800_classify_open(const struct rf5800_reply *identity_reply,
                         int *verdict);

/* The one I/O function; the only code that touches the port. */
int rf5800_transaction(RIG *rig, const char *cmd, int deadline_ms,
                       struct rf5800_reply *out);

/* The guard before every write. */
int rf5800_write_gate(RIG *rig, int channel_scoped);

#endif /* _RF5800_H */
