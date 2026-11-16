/*
 *  Hamlib Harris backend - RF-350K (RT-1446/URC) model
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

#ifndef _RF350K_H
#define _RF350K_H

#include <stddef.h>
#include <pthread.h>
#include <sys/time.h>

#include "hamlib/rig.h"

/* the front-panel status dump (KO6NO's capture) has 31 lines, so 16 would
 * lose fields */
#define RF350K_MAX_LINES    48
/* letter + value (<= 14) + NUL; longer lines are truncated */
#define RF350K_LINE_LEN     16
/* every builder needs a buffer of at least this; else -RIG_EINVAL */
#define RF350K_CMD_MIN      24
/* 0 would never see queued bytes on a Windows COM port (win32_serial_select) */
#define RF350K_DRAIN_WAIT_MS 1
#define RF350K_DRAIN_MAX    256
#define RF350K_WAIT_MS      500
#define RF350K_READ_MS      2000
#define RF350K_SYNC_MS      1000
/* one lost sync must still arrive inside the deadman, so the period is
 * 3 s (the RF-7210 and KO6NO's probe program also use 3 s) */
#define RF350K_SYNC_PERIOD_MS 3000
/* the radio's own deadman, measured on the radio (REV 604M: X002 pushed
 * ~10.5 s after the last command); the NJ7C document says ~15 s */
#define RF350K_DEADMAN_MS   10000
#define RF350K_HZ_MIN       1600000
#define RF350K_HZ_MAX       29999990
/* the highest channel the backend sends (C50-C99 get no reply on REV 604M,
 * measured on the radio); incoming C lines are still read as 0..99 */
#define RF350K_CHAN_MAX     49
/* "busy" (a setter refused while the backend holds PTT, nothing sent).
 * Hamlib has no RIG_EBUSY (rig.h, enum rig_errcode_e); the code must be a
 * soft one (RIG_IS_SOFT_ERRCODE), because rigctl and rigctld close and
 * reopen the rig on any other error, which would unkey it.  The radio itself
 * rejects these changes while keyed (TM para 4-12). */
#define RF350K_EBUSY        RIG_ERJCTED

/* rf350k_transaction's flags */
/* the write needs REMOTE: nothing is written while the radio is in local */
#define RF350K_NEEDS_REMOTE 1
/* this write is a question; repeat it while the radio sends nothing at all */
#define RF350K_ASK          2
/* for U: as RF350K_ASK, but only if the radio is in REMOTE once the
 * transaction's drain has been read */
#define RF350K_ASK_REMOTE   4
/* the further status reads before a gain is refused.  On this radio each is
 * the pause and about 255 ms (measured on the radio), so five end about
 * 2.3 s after the G write */
#define RF350K_GAIN_READS   5
/* ms of silence before a transaction that follows a writing one.  Measured
 * on the radio, back-to-back gain changes: 100 ms once per cycle gave 40 of
 * 40 and 39 of 40, 50 ms 39 of 40, none 22 and 8 of 40; half as much again.
 * Sized on 40 changes per value, one radio, one evening */
#define RF350K_PAUSE_MS     150
/* writes at most when a transaction asks.  Measured on the radio: every
 * lost write on record that was written again was answered by the first
 * rewrite */
#define RF350K_ASK_WRITES   3
/* this write is allowed while a coupler tune is believed (the pure status
 * reads and the one in-tune U) */
#define RF350K_IN_TUNE      8
/* a timeout of this call is expected (the in-tune U), so it is traced
 * with its own sentence, not as a fault */
#define RF350K_QUIET        16
/* ms: the longest the driver believes a tune runs, and the deadline of a
 * key-off's wait for one to end (the call can return about 3 s after it:
 * one more status read and the unkey).  It is longer than the radio's own
 * keep-alive, 10.5 s, and a little over twice the longest tune measured on
 * the radio, 7.1 s, every one into a dummy load: a margin, not a measured
 * limit */
#define RF350K_TUNE_WAIT_MS 15000
/* ms: the rest between two status reads of a key-off that waits for a
 * tune's end */
#define RF350K_TUNE_POLL_MS 500

struct rf350k_reply
{
    char  partial[RF350K_LINE_LEN];                 /* the line being assembled */
    int   partial_from_carry;                       /* its first byte came from carry */
    char  lines[RF350K_MAX_LINES][RF350K_LINE_LEN]; /* every fresh line, in order */
    int   nlines;
    int   discard;          /* dropping an overlong line's tail up to \n */
    const char *expect;     /* the completion rule, set by the transaction */
    int   expect_pos;       /* letters matched so far */
    int   done;             /* every expected letter has arrived */
    int   status;           /* X line -> n; -1 none */
    int   remote;           /* R line -> 0/1; -1 none */
    int   key;              /* bare U -> 0, bare K -> 1; -1 none (the last one wins) */
    int   chan_last;        /* the last C line: -2 none, -1 "C;;", else 0..99 */
    int   refused_remote;   /* the transaction's gate refused the write */
    int   route;            /* the last "0/"1 line (audio routing); -1 none */
    int   pttena;           /* the last '0/'1 line (PTT enable); -1 none */
    /* since the tune belief was last brought up to date: the last *0 or *1
     * line (-1 none), and whether a *0 line was read at all */
    int   tune;
    int   tune_zero;
    int   refused_tune;     /* the transaction's tune gate refused the write */
};

/* -1 = field absent; parse errors are return codes, not values */
struct rf350k_status
{
    freq_t hz;           /* F, required */
    rmode_t mode;        /* M, required */
    int remote;          /* R, required */
    int agc;             /* A -> RIG_AGC_* */
    int gain;            /* G 0..63 */
    int audio_on;        /* ( */
    int channel_present; /* a C line was in the fresh dump */
    int channel;         /* -1 = "C;;" (none selected), else 0..99 */
    int key;             /* bare U/K line: 0/1 */
    int status;          /* X */
    int route;           /* " */
    int pttena;          /* ' */
    int adapt;           /* a ^ line: the ADAPT format */
    int panel;           /* a QT line: the front-panel format */
};

struct rf350k_priv
{
    int   remote, ext_key, ptt_held, channel, rfgain;
    char  info[32];
    pthread_mutex_t mutex;
    pthread_t thread;
    volatile int runflag;
    struct timeval last_sync;
    int   sync_failures;
    /* ms between deadman syncs; rf350k_init sets it from
     * RF350K_SYNC_PERIOD_MS */
    int   sync_period_ms;
    /* ms a deadman sync waits for its answer; rf350k_init sets it from
     * RF350K_SYNC_MS */
    int   sync_timeout_ms;
    int   route, pttena;     /* -1 until a line or a dump says */
    int   assert_failures;   /* one WARN per streak */
    /* ms of silence before a transaction that follows a writing one;
     * rf350k_init sets it from RF350K_PAUSE_MS */
    int   pause_ms;
    /* writes at most when a transaction asks; rf350k_init sets it from
     * RF350K_ASK_WRITES */
    int   ask_writes;
    /* when the last transaction that got past its gate ended; zero until one
     * has, kept across close and reopen */
    struct timeval last_end;
    /* a coupler tune is believed to run (see rf350k_tune_believed);
     * kept across close and reopen, as last_end is */
    int   tuning;
    /* when tuning went to 1; zero: the hard end is past */
    struct timeval tune_since;
    int   tune_unkey_sent;      /* this tune's one U has been written */
    int   tune_wait_ms;         /* rf350k_init sets it: RF350K_TUNE_WAIT_MS */
    int   tune_poll_ms;         /* rf350k_init sets it: RF350K_TUNE_POLL_MS */
    /* a belief ended while the key was held, so the ordinary K
     * waits for one whole dump whose own * line is *0 */
    int   tune_confirm;
};

extern struct rig_caps rf350k_caps;

/* The functions below are not static because the unit tests of these models
 * call them. */

/* pure: no I/O, no RIG * */
void rf350k_reply_init(struct rf350k_reply *r, const char *expect);
int  rf350k_feed(struct rf350k_reply *r, const char *bytes, size_t len,
                 int from_carry);
int  rf350k_parse_dump(const struct rf350k_reply *r, struct rf350k_status *st);
int  rf350k_cmd_freq(freq_t hz, char *buf, size_t n);
int  rf350k_cmd_mode_freq(rmode_t mode, freq_t hz, char *buf, size_t n);
int  rf350k_cmd_agc_freq(int rig_agc, freq_t hz, char *buf, size_t n);
int  rf350k_cmd_rfgain(float level, char *buf, size_t n);
float rf350k_gain_to_level(int g);
int  rf350k_cmd_chan(int ch, char *buf, size_t n);
int  rf350k_cmd_mute(int on, char *buf, size_t n);
const char *rf350k_expect_mode(rmode_t old_mode, rmode_t new_mode);

/* the one I/O function */
int  rf350k_transaction(RIG *rig, const char *wire, const char *expect,
                        int deadline_ms, int flags, struct rf350k_reply *out);
/* one status read during a tune; star and key are its own dump's *n
 * and bare key lines.  The caller holds priv->mutex */
int  rf350k_tune_read(RIG *rig, int *star, int *key);

#endif /* _RF350K_H */
