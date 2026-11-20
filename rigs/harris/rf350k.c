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

/*
 * The RF-350K (RT-1446/URC) over its J9 remote port, through the A1A19
 * board or the KO6NO adapter.  The protocol is the one NJ7C published in
 * 2006 for firmware REV 604; it shares nothing with the PRC-138 in harris.c
 * or the RF-5800H in rf5800.c.  Every line the radio sends is classified
 * by rf350k_feed; rf350k_transaction is the only code that reads or writes
 * the port (rf350k_open's single rig_flush aside).
 */

#include <errno.h>
#include <math.h>
#include <stdlib.h>
#include <string.h>
#include <sys/time.h>

#include "hamlib/rig.h"
#include "hamlib/port.h"
#include "hamlib/rig_state.h"
#include "iofunc.h"
#include "rf350k.h"

#define RF350K_MODES (RIG_MODE_USB | RIG_MODE_LSB | RIG_MODE_CW | RIG_MODE_AM)

void rf350k_reply_init(struct rf350k_reply *r, const char *expect)
{
    memset(r, 0, sizeof(*r));
    r->expect = expect;
    r->status = -1;
    r->remote = -1;
    r->key = -1;
    r->chan_last = -2;
    r->route = -1;
    r->pttena = -1;
    r->tune = -1;
    r->done = expect[0] == '\0';
}

/* exactly n decimal digits and nothing after them */
static int rf350k_digits(const char *s, size_t n)
{
    size_t i;

    for (i = 0; i < n; i++)
    {
        if (s[i] < '0' || s[i] > '9')
        {
            return 0;
        }
    }

    return s[n] == '\0';
}

static int rf350k_bare(const char *line, char c)
{
    return line[0] == c && line[1] == '\0';
}

/*
 * One completed line.  A stale line, from the drain or arriving
 * after the expected reply has completed (it belongs to whatever the next
 * transaction drains), updates the side fields only.  A fresh line is also
 * stored and may advance expect.
 */
static void rf350k_line(struct rf350k_reply *r, const char *line, int stale)
{
    const unsigned char c0 = (unsigned char) line[0];
    int fresh = !stale && !r->done;
    char want;

    /* the empty line; a non-printable first byte never gets here (rf350k_feed) */
    if (c0 == '\0')
    {
        return;
    }

    if (c0 == 'X' && rf350k_digits(line + 1, 3))
    {
        r->status = atoi(line + 1);

        if (r->status != 0)
        {
            rig_debug(RIG_DEBUG_WARN, "%s: the radio reports %s (deadman lapsed)\n",
                      __func__, line);
        }
    }
    else if (strcmp(line, "R0") == 0 || strcmp(line, "R1") == 0)
    {
        r->remote = line[1] - '0';
    }
    else if ((c0 == '"' || c0 == '\'') && (line[1] == '0' || line[1] == '1')
             && line[2] == '\0')
    {
        /* the audio routing and the PTT enable, echoed or pushed */
        *(c0 == '"' ? &r->route : &r->pttena) = line[1] - '0';
    }
    else if (rf350k_bare(line, 'U') || rf350k_bare(line, 'K'))
    {
        r->key = c0 == 'K';
    }
    else if (strcmp(line, "C;;") == 0)
    {
        r->chan_last = -1;
    }
    else if (c0 == 'C' && rf350k_digits(line + 1, 2))
    {
        r->chan_last = atoi(line + 1);
    }
    else if (strcmp(line, "*0") == 0 || strcmp(line, "*1") == 0)
    {
        /* the coupler's tune state; a *0 is kept apart, so a *0 then a *1
         * (a tune ended and a new one began) is not read as one tune */
        r->tune = line[1] - '0';
        r->tune_zero |= r->tune == 0;
    }

    if (!fresh)
    {
        return;
    }

    if (r->nlines < RF350K_MAX_LINES)
    {
        strcpy(r->lines[r->nlines++], line);
    }
    else
    {
        rig_debug(RIG_DEBUG_VERBOSE, "%s: more than %d lines, dropped '%s'\n",
                  __func__, RF350K_MAX_LINES, line);
    }

    /* "U" in expect is the sync's answer, a bare U or K line.  A quote or
     * apostrophe completes only on its exact '1 / "1, and C;; is always
     * unsolicited (measured on the radio), so a C completes only on C +
     * digits */
    want = r->expect[r->expect_pos];

    if (want == 'U' ? rf350k_bare(line, 'U') || rf350k_bare(line, 'K')
            : want == '.' ? rf350k_bare(line, '.')
            : want == '"' || want == '\'' ? (char) c0 == want && strcmp(line + 1, "1") == 0
            : want == 'C' ? c0 == 'C' && rf350k_digits(line + 1, 2)
            : (char) c0 == want)
    {
        r->expect_pos++;
        r->done = r->expect[r->expect_pos] == '\0';
    }
}

/*
 * The tokenizer.  A LF ends a line; this radio sends no CR (measured on the
 * radio), so a CR at a line's end stays part of it.  Every decision is made
 * byte by byte, so the grouping of the bytes never changes the result.  A
 * line is classified with the provenance of its first byte.
 */
int rf350k_feed(struct rf350k_reply *r, const char *bytes, size_t len,
                int from_carry)
{
    const size_t full = sizeof(r->partial) - 1;
    char line[RF350K_LINE_LEN];
    size_t i;

    for (i = 0; i < len; i++)
    {
        const char ch = bytes[i];
        const size_t n = strlen(r->partial);
        int stale = n > 0 ? r->partial_from_carry : from_carry;

        /* the tail of an overlong line, up to and including its LF */
        if (r->discard)
        {
            r->discard = ch != '\n';
            continue;
        }

        if (ch == '\n')
        {
            strcpy(line, r->partial);
            r->partial[0] = '\0';
            r->partial_from_carry = 0;
            rf350k_line(r, line, stale);
            continue;
        }

        /* a line whose first byte is not printable (CR included) is
         * dropped through its LF; a NUL anywhere drops its line too, as
         * the bytes are assembled as a C string */
        if (ch == '\0' || (n == 0 && ((unsigned char) ch < 0x20
                                      || (unsigned char) ch > 0x7e)))
        {
            rig_debug(RIG_DEBUG_VERBOSE, "%s: dropped a line with byte 0x%02x\n",
                      __func__, (unsigned char) ch);
            r->partial[0] = '\0';
            r->partial_from_carry = 0;
            r->discard = 1;
            continue;
        }

        if (n == full)
        {
            strcpy(line, r->partial);
            r->partial[0] = '\0';
            r->partial_from_carry = 0;
            r->discard = 1;
            rig_debug(RIG_DEBUG_VERBOSE, "%s: a line longer than %d bytes, kept '%s'\n",
                      __func__, (int) full, line);
            rf350k_line(r, line, stale);
            continue;
        }

        if (n == 0)
        {
            r->partial_from_carry = from_carry;
        }

        r->partial[n] = ch;
        r->partial[n + 1] = '\0';
    }

    return RIG_OK;
}

static int rf350k_bad_field(const char *line)
{
    rig_debug(RIG_DEBUG_ERR,
              "%s: the radio's status dump has a malformed '%c' field '%s' - check the baud rate and that the radio runs REV 604 firmware\n",
              __func__, line[0], line);
    return -RIG_EPROTO;
}

/* From the stored (fresh) lines only, never the side fields. */
int rf350k_parse_dump(const struct rf350k_reply *r, struct rf350k_status *st)
{
    static const rmode_t modes[] =
    {
        RIG_MODE_USB, RIG_MODE_LSB, RIG_MODE_AM, RIG_MODE_CW,
    };
    static const int agcs[] =
    {
        RIG_AGC_OFF, RIG_AGC_SLOW, RIG_AGC_MEDIUM, RIG_AGC_FAST,
    };
    int have_f = 0;
    int have_end = 0;
    int i;

    st->hz = -1;
    st->mode = RIG_MODE_NONE;
    st->remote = -1;
    st->agc = -1;
    st->gain = -1;
    st->audio_on = -1;
    st->channel_present = 0;
    st->channel = -1;
    st->key = -1;
    st->status = -1;
    st->route = -1;
    st->pttena = -1;
    st->adapt = 0;
    st->panel = 0;

    for (i = 0; i < r->nlines; i++)
    {
        const char *line = r->lines[i];
        const char *v = line + 1;

        switch (line[0])
        {
        case 'F':
            if (!rf350k_digits(v, 7))
            {
                return rf350k_bad_field(line);
            }

            /* kHz with two decimals: units of 10 Hz */
            st->hz = (freq_t) atol(v) * 10;
            have_f = 1;
            break;

        case 'M':
            if (v[0] < '1' || v[0] > '4' || v[1] != '\0')
            {
                return rf350k_bad_field(line);
            }

            st->mode = modes[v[0] - '1'];
            break;

        case 'A':
            if (v[0] < '1' || v[0] > '4' || v[1] != '\0')
            {
                return rf350k_bad_field(line);
            }

            st->agc = agcs[v[0] - '1'];
            break;

        case 'G':
            if (!rf350k_digits(v, 2) || atoi(v) > 63)
            {
                return rf350k_bad_field(line);
            }

            st->gain = atoi(v);
            break;

        case '(':
            if ((v[0] != '0' && v[0] != '1') || v[1] != '\0')
            {
                return rf350k_bad_field(line);
            }

            st->audio_on = v[0] - '0';
            break;

        case 'R':
            if ((v[0] != '0' && v[0] != '1') || v[1] != '\0')
            {
                return rf350k_bad_field(line);
            }

            st->remote = v[0] - '0';
            break;

        case '"':
        case '\'':
            if ((v[0] != '0' && v[0] != '1') || v[1] != '\0')
            {
                return rf350k_bad_field(line);
            }

            *(line[0] == '"' ? &st->route : &st->pttena) = v[0] - '0';
            break;

        /* which status format answered; the open decides */
        case '^':
            st->adapt = 1;
            break;

        case 'Q':
            st->panel |= v[0] == 'T';
            break;

        case 'X':
            if (!rf350k_digits(v, 3))
            {
                return rf350k_bad_field(line);
            }

            st->status = atoi(v);
            break;

        case 'C':
            if (strcmp(v, ";;") == 0)
            {
                st->channel = -1;
            }
            else if (rf350k_digits(v, 2))
            {
                st->channel = atoi(v);
            }
            else
            {
                return rf350k_bad_field(line);
            }

            st->channel_present = 1;
            break;

        /* the bare U/K line is the key state; other U/K lines are not dump
         * fields */
        case 'U':
        case 'K':
            if (v[0] == '\0')
            {
                st->key = line[0] == 'K';
            }

            break;

        case '.':
            have_end |= v[0] == '\0';
            break;

        default:
            break;
        }
    }

    if (!have_f || st->mode == RIG_MODE_NONE || st->remote < 0)
    {
        rig_debug(RIG_DEBUG_ERR,
                  "%s: the radio did not send a status dump (an F, M or R field is missing) - check that this is an RF-350K with its remote board fitted\n",
                  __func__);
        return -RIG_EPROTO;
    }

    if (!have_end)
    {
        rig_debug(RIG_DEBUG_ERR,
                  "%s: the radio's status dump has no '.' terminator - check the cable and the baud rate\n",
                  __func__);
        return -RIG_EPROTO;
    }

    return RIG_OK;
}

static int rf350k_buffer_ok(size_t n, const char *func)
{
    if (n >= RF350K_CMD_MIN)
    {
        return 1;
    }

    rig_debug(RIG_DEBUG_ERR,
              "%s: a %d-byte command buffer is too small (need %d) - this is a backend bug, please report it\n",
              func, (int) n, RF350K_CMD_MIN);
    return 0;
}

/* The F value: 10 Hz units, rounded half away from zero; the window of
 * the radio's manual applies to the rounded value. */
static int rf350k_units(freq_t hz, long *units, const char *func)
{
    long v;

    if (!isfinite(hz))
    {
        rig_debug(RIG_DEBUG_ERR, "%s: the frequency is not a number - give it in Hz\n",
                  func);
        return -RIG_EINVAL;
    }

    v = lround(hz / 10.0);

    if (v < RF350K_HZ_MIN / 10 || v > RF350K_HZ_MAX / 10)
    {
        rig_debug(RIG_DEBUG_ERR,
                  "%s: %.0f Hz is outside the radio's 1.6-29.99999 MHz range - choose a frequency inside it\n",
                  func, hz);
        return -RIG_EINVAL;
    }

    *units = v;
    return RIG_OK;
}

int rf350k_cmd_freq(freq_t hz, char *buf, size_t n)
{
    long v;
    int ret;

    if (!rf350k_buffer_ok(n, __func__))
    {
        return -RIG_EINVAL;
    }

    ret = rf350k_units(hz, &v, __func__);

    if (ret != RIG_OK)
    {
        return ret;
    }

    SNPRINTF(buf, n, "F%07ld\n", v);
    return RIG_OK;
}

int rf350k_cmd_mode_freq(rmode_t mode, freq_t hz, char *buf, size_t n)
{
    int m;
    long v;
    int ret;

    if (!rf350k_buffer_ok(n, __func__))
    {
        return -RIG_EINVAL;
    }

    switch (mode)
    {
    case RIG_MODE_USB: m = 1; break;

    case RIG_MODE_LSB: m = 2; break;

    /* AME, the radio's compatible AM */
    case RIG_MODE_AM: m = 3; break;

    case RIG_MODE_CW: m = 4; break;

    default:
        rig_debug(RIG_DEBUG_ERR,
                  "%s: the RF-350K has no mode %s - use USB, LSB, AM or CW\n",
                  __func__, rig_strrmode(mode));
        return -RIG_EINVAL;
    }

    ret = rf350k_units(hz, &v, __func__);

    if (ret != RIG_OK)
    {
        return ret;
    }

    /* 604 firmware: M takes effect only when an F follows (NJ7C document) */
    SNPRINTF(buf, n, "M%d\nF%07ld\n", m, v);
    return RIG_OK;
}

int rf350k_cmd_agc_freq(int rig_agc, freq_t hz, char *buf, size_t n)
{
    int a;
    long v;
    int ret;

    if (!rf350k_buffer_ok(n, __func__))
    {
        return -RIG_EINVAL;
    }

    switch (rig_agc)
    {
    case RIG_AGC_OFF: a = 1; break;

    case RIG_AGC_SLOW: a = 2; break;

    case RIG_AGC_MEDIUM: a = 3; break;

    case RIG_AGC_FAST: a = 4; break;

    default:
        rig_debug(RIG_DEBUG_ERR,
                  "%s: the RF-350K has no AGC setting %d - use OFF, SLOW, MEDIUM or FAST\n",
                  __func__, rig_agc);
        return -RIG_EINVAL;
    }

    ret = rf350k_units(hz, &v, __func__);

    if (ret != RIG_OK)
    {
        return ret;
    }

    /* 604 firmware: A takes effect only when an F follows (NJ7C document) */
    SNPRINTF(buf, n, "A%d\nF%07ld\n", a, v);
    return RIG_OK;
}

/* NJ7C: G00 is the maximum gain, G63 the minimum */
int rf350k_cmd_rfgain(float level, char *buf, size_t n)
{
    double g;

    if (!rf350k_buffer_ok(n, __func__))
    {
        return -RIG_EINVAL;
    }

    if (!isfinite(level))
    {
        rig_debug(RIG_DEBUG_ERR, "%s: the RF gain is not a number - give 0.0 to 1.0\n",
                  __func__);
        return -RIG_EINVAL;
    }

    g = (1.0 - (double) level) * 63.0;
    g = g < 0.0 ? 0.0 : g > 63.0 ? 63.0 : g;
    SNPRINTF(buf, n, "G%02ld\n", lround(g));
    return RIG_OK;
}

float rf350k_gain_to_level(int g)
{
    return 1.0f - (float) g / 63.0f;
}

int rf350k_cmd_chan(int ch, char *buf, size_t n)
{
    if (!rf350k_buffer_ok(n, __func__))
    {
        return -RIG_EINVAL;
    }

    /* REV 604M answers C00-C49 and is silent above (measured on the radio) */
    if (ch < 0 || ch > RF350K_CHAN_MAX)
    {
        rig_debug(RIG_DEBUG_ERR, "%s: channel %d is outside 0-49\n", __func__, ch);
        return -RIG_EINVAL;
    }

    SNPRINTF(buf, n, "C%02d\n", ch);
    return RIG_OK;
}

/* "(0" is audio OFF (NJ7C document), so MUTE on sends (0 */
int rf350k_cmd_mute(int on, char *buf, size_t n)
{
    if (!rf350k_buffer_ok(n, __func__))
    {
        return -RIG_EINVAL;
    }

    SNPRINTF(buf, n, "(%d\n", on ? 0 : 1);
    return RIG_OK;
}

/* entering or leaving CW appends S and H lines to the confirmation */
const char *rf350k_expect_mode(rmode_t old_mode, rmode_t new_mode)
{
    return (old_mode == RIG_MODE_CW) != (new_mode == RIG_MODE_CW) ? "MFSH" : "MF";
}

/* its callers measure a few seconds at most: a try's own clock and
 * priv->last_sync, which the open sets and the thread renews every period */
static long rf350k_elapsed_ms(const struct timeval *t0)
{
    struct timeval t1;

    gettimeofday(&t1, NULL);
    return (t1.tv_sec - t0->tv_sec) * 1000L + (t1.tv_usec - t0->tv_usec) / 1000L;
}

/*
 * "A coupler tune is believed to run": tuning is 1 and less than
 * tune_wait_ms has passed since tune_since (after that, the tune's hard end
 * is past).  A zero tune_since means the hard end is past (rf350k_sync sets
 * it when REMOTE is left while keyed).  Whole seconds are compared first, as
 * the pause does, so no product can overflow; a time since the stamp that
 * comes out negative (the PC's clock was put back) is past every limit.
 * Past the hard end the gate holds no write back, although tuning stays 1
 * until a *0 is read; while the key is held the keep-alive still stays the
 * status read until a whole dump reading *0 has been read (rf350k_sync).
 * The caller holds priv->mutex.
 */
static int rf350k_tune_believed(const struct rf350k_priv *priv)
{
    struct timeval now;
    long s, ms;

    if (!priv->tuning
            || (priv->tune_since.tv_sec == 0 && priv->tune_since.tv_usec == 0))
    {
        return 0;
    }

    gettimeofday(&now, NULL);
    s = now.tv_sec - priv->tune_since.tv_sec;

    if (s < 0 || s > priv->tune_wait_ms / 1000 + 1)
    {
        return 0;
    }

    ms = s * 1000L + (now.tv_usec - priv->tune_since.tv_usec) / 1000L;
    return ms >= 0 && ms < priv->tune_wait_ms;
}

/* what any line, drained or fresh, says about the radio.  The tune belief
 * changes here only (a *0 ends it, then a *1 starts it; rf350k_set_ptt also
 * starts one on a K's &0), and the reply's two tune fields are then set
 * back, so each * line counts once */
static void rf350k_note_state(struct rf350k_priv *priv,
                              struct rf350k_reply *r)
{
    /* a *0 is the only line that ends the belief */
    if (r->tune_zero)
    {
        /* ended with the key held: the sync's K waits for a whole *0 dump */
        priv->tune_confirm |= priv->tuning && priv->ptt_held;
        priv->tuning = 0;
        priv->tune_unkey_sent = 0;
    }

    /* a *1 starts it; none restarts it while tuning is 1, so a tune past its
     * hard end is not believed again because the radio goes on saying *1 */
    if (r->tune == 1 && priv->tuning == 0)
    {
        priv->tuning = 1;
        gettimeofday(&priv->tune_since, NULL);
        priv->tune_unkey_sent = 0;
    }

    r->tune = -1;
    r->tune_zero = 0;

    if (r->remote >= 0)
    {
        priv->remote = r->remote;
    }

    if (r->key >= 0)
    {
        priv->ext_key = r->key;
    }

    if (r->chan_last != -2)
    {
        priv->channel = r->chan_last;
    }

    if (r->route >= 0)
    {
        priv->route = r->route;
    }

    if (r->pttena >= 0)
    {
        priv->pttena = r->pttena;
    }
}

/* the wire with its LFs spelled out, for the log */
static void rf350k_printable(const char *wire, char *out, size_t n)
{
    size_t i, k = 0;

    for (i = 0; wire[i] != '\0' && k + 3 < n; i++)
    {
        if (wire[i] == '\n')
        {
            out[k++] = '\\';
            out[k++] = 'n';
        }
        else
        {
            out[k++] = wire[i];
        }
    }

    out[k] = '\0';
}

/*
 * The only function that reads or writes the port (rf350k_open's single
 * rig_flush aside).  Its skeleton is rf5800_transaction's; the completion
 * rule is the expected reply letters, and there is no prompt to resync to.
 * With an asking flag, a write the radio sent nothing at all for is written
 * again (measured on the radio: it sometimes misses a write and hears the
 * next one).
 */
int rf350k_transaction(RIG *rig, const char *wire, const char *expect,
                       int deadline_ms, int flags, struct rf350k_reply *out)
{
    hamlib_port_t *rp = RIGPORT(rig);
    struct rf350k_priv *priv = STATE(rig)->priv;
    const int timeout_save = rp->timeout;
    char shown[2 * RF350K_CMD_MIN + 1];
    unsigned char b[2];
    struct timeval t0;
    int written = 0;
    int writes_max;
    int heard = 0;
    int asking;
    int drained;
    int ret;
    int n;

    rf350k_printable(wire, shown, sizeof(shown));
    rig_debug(RIG_DEBUG_TRACE, "%s: '%s', expect '%s', %d ms\n", __func__, shown,
              expect, deadline_ms);

    /* the silence the radio needs after the end of the last
     * transaction that got past its gate; only what is left of it is slept.
     * Whole seconds are compared first, so no product can overflow a 32-bit
     * long, and the rest is in microseconds, so nothing is rounded away */
    if (priv->pause_ms > 0 && priv->last_end.tv_sec != 0)
    {
        const long full = priv->pause_ms * 1000L;
        struct timeval now;
        long s, left;

        gettimeofday(&now, NULL);
        s = now.tv_sec - priv->last_end.tv_sec;

        if (s < 0)
        {
            /* the PC's clock was put back */
            left = full;
        }
        else if (s > 1)
        {
            left = 0;
        }
        else
        {
            left = full - (s * 1000000L + (now.tv_usec - priv->last_end.tv_usec));

            if (left > full)
            {
                left = full;
            }
        }

        if (left > 0)
        {
            hl_usleep(left);
        }
    }

    /* 1: a half line the last transaction left is dropped */
    rf350k_reply_init(out, expect);

    /* 2: whatever is already waiting arrived before this command was
     * written, so it is stale.  The wait is 1 ms, not 0: on a Windows COM
     * port win32_serial_select (lib/termios.c) checks the input queue only
     * inside its while (timeout_usec > 0) loop. */
    rp->timeout = RF350K_DRAIN_WAIT_MS;

    for (drained = 0; drained < RF350K_DRAIN_MAX; drained++)
    {
        b[0] = '\0';
        errno = 0;
        n = read_string(rp, b, sizeof(b), "\n", 1, 1, 1);

        if (n <= 0 && b[0] == '\0')
        {
            break;
        }

        rf350k_feed(out, (const char *) b, 1, 1);
    }

    if (drained == RF350K_DRAIN_MAX)
    {
        rig_debug(RIG_DEBUG_WARN,
                  "%s: drained %d stale bytes before '%s' and stopped there\n",
                  __func__, RF350K_DRAIN_MAX, shown);
    }

    rf350k_note_state(priv, out);

    /* decided once, after the drain: a queued R0 it read means the radio,
     * in local, never answers U (measured on the radio), so there is
     * nothing to wait for */
    asking = (flags & RF350K_ASK)
             || ((flags & RF350K_ASK_REMOTE) && priv->remote == 1);
    /* one count for every asking call */
    writes_max = asking && priv->ask_writes > 1 ? priv->ask_writes : 1;

    /* the tune gate: while a tune is believed only a status read or the one
     * in-tune U is written; tested before the REMOTE gate, so a call that
     * meets both takes the tune path.  A *1 this call's drain read counts */
    if (!(flags & RF350K_IN_TUNE) && rf350k_tune_believed(priv))
    {
        out->refused_tune = 1;
        rig_debug(RIG_DEBUG_VERBOSE,
                  "%s: the coupler is tuning, so '%s' was not sent\n", __func__, shown);
        ret = -RF350K_EBUSY;
    }
    /* 3: the REMOTE gate sees a queued R0 before anything is written */
    else if ((flags & RF350K_NEEDS_REMOTE) && priv->remote != 1)
    {
        out->refused_remote = 1;
        rig_debug(RIG_DEBUG_WARN,
                  "%s: the radio is not in REMOTE, so '%s' was not sent\n",
                  __func__, shown);
        ret = -RIG_ENAVAIL;
    }
    else
    {
        /* 4 */
        gettimeofday(&t0, NULL);
        written = write_block(rp, (const unsigned char *) wire, strlen(wire)) >= 0;
        ret = written ? RIG_OK : -RIG_EIO;
    }

    /* 5 */
    if (written)
    {
        for (;;)
        {
            const long spent = rf350k_elapsed_ms(&t0);
            /* a clock put back gives a negative time: the budget is spent */
            long left = spent < 0 ? 0 : deadline_ms - spent;

            /* nothing at all came: the radio did not hear the write, so the
             * same bytes are asked again, with nothing drained between */
            if (left <= 0 && !heard && written < writes_max)
            {
                rig_debug(RIG_DEBUG_WARN,
                          "%s: nothing from the radio within %d ms after '%s'; asking again\n",
                          __func__, deadline_ms, shown);
                gettimeofday(&t0, NULL);

                if (write_block(rp, (const unsigned char *) wire, strlen(wire)) < 0)
                {
                    ret = -RIG_EIO;
                    break;
                }

                written++;
                continue;
            }

            /* an elapsed-time budget: a radio that trickles bytes forever
             * still ends here */
            if (left <= 0)
            {
                ret = -RIG_ETIMEOUT;
                break;
            }

            /* one byte per call, so one call is one wait of at most this */
            rp->timeout = left < RF350K_WAIT_MS ? (int) left : RF350K_WAIT_MS;
            b[0] = '\0';
            /* read_string sleeps 5 ms per byte while errno holds a stale EAGAIN */
            errno = 0;
            n = read_string(rp, b, sizeof(b), "\n", 1, 0, 1);

            if (n < 0 && n != -RIG_ETIMEOUT)
            {
                ret = -RIG_EIO;
                break;
            }

            /* a timed-out call may still have consumed a byte */
            if (n > 0 || b[0] != '\0')
            {
                heard = 1;
                rf350k_feed(out, (const char *) b, 1, 0);
            }

            if (out->done)
            {
                ret = RIG_OK;
                break;
            }
        }
    }

    /* 6 */
    rp->timeout = timeout_save;
    rf350k_note_state(priv, out);

    /* the pause counts from here, however the transaction ended, unless a
     * gate wrote nothing */
    if (!out->refused_remote && !out->refused_tune)
    {
        gettimeofday(&priv->last_end, NULL);
    }

    if (ret == -RIG_EIO)
    {
        rig_debug(RIG_DEBUG_ERR,
                  "%s: the link to the radio failed during '%s' - check the cable or connection, then reopen the rig\n",
                  __func__, shown);
    }
    else if (ret == -RIG_ETIMEOUT && (flags & RF350K_QUIET))
    {
        /* the radio answers a U during a tune with nothing (measured on the
         * radio), so this shares no wording with the fault sentences */
        rig_debug(RIG_DEBUG_TRACE,
                  "%s: no answer during the coupler tune (expected)\n", __func__);
    }
    else if (ret == -RIG_ETIMEOUT)
    {
        rig_debug(RIG_DEBUG_ERR,
                  "%s: no complete answer from the radio within %d ms after '%s' - check that it is powered, cabled and at the configured baud rate\n",
                  __func__, deadline_ms, shown);
    }

    return ret;
}

/*
 * One status read during a tune.  The radio answers one whole all through a
 * tune (measured on the radio), so its timeout is a fault.  The key state is
 * taken only from the dump's own lines, those after the LAST fresh #1; a
 * reply without them (the tail of a dump whose head an earlier transaction
 * read) is no dump.  The caller holds priv->mutex.
 */
int rf350k_tune_read(RIG *rig, int *star, int *key)
{
    const struct rf350k_priv *priv = STATE(rig)->priv;
    struct rf350k_reply r;
    int ret = rf350k_transaction(rig, "?\n", ".", priv->sync_timeout_ms,
                                 RF350K_IN_TUNE, &r);
    int head = -1;
    int i;

    *star = -1;
    *key = -1;

    if (ret != RIG_OK)
    {
        return ret;
    }

    /* a full line array may have lost a later dump's lines while its "."
     * still completed the read, so it confirms nothing */
    for (i = 0; r.nlines < RF350K_MAX_LINES && i < r.nlines; i++)
    {
        if (strcmp(r.lines[i], "#1") == 0)
        {
            head = i;
        }
    }

    for (i = head + 1; head >= 0 && i < r.nlines; i++)
    {
        if (strcmp(r.lines[i], "*0") == 0 || strcmp(r.lines[i], "*1") == 0)
        {
            *star = r.lines[i][1] - '0';
        }
        else if (rf350k_bare(r.lines[i], 'U') || rf350k_bare(r.lines[i], 'K'))
        {
            *key = r.lines[i][0] == 'K';
        }
    }

    if (*star < 0 || *key < 0)
    {
        rig_debug(RIG_DEBUG_VERBOSE,
                  "%s: the status read was not a whole dump; it is not used\n", __func__);
        return -RIG_EPROTO;
    }

    return RIG_OK;
}

static int rf350k_init(RIG *rig)
{
    struct rf350k_priv *priv;

    rig_debug(RIG_DEBUG_VERBOSE, "%s called\n", __func__);
    priv = calloc(1, sizeof(*priv));

    if (!priv)
    {
        rig_debug(RIG_DEBUG_ERR, "%s: out of memory - close other programs and retry\n",
                  __func__);
        return -RIG_ENOMEM;
    }

    priv->remote = -1;
    priv->ext_key = -1;
    priv->channel = -1;
    priv->rfgain = -1;
    priv->sync_period_ms = RF350K_SYNC_PERIOD_MS;
    priv->sync_timeout_ms = RF350K_SYNC_MS;
    priv->route = -1;
    priv->pttena = -1;
    priv->pause_ms = RF350K_PAUSE_MS;
    priv->ask_writes = RF350K_ASK_WRITES;
    priv->tune_wait_ms = RF350K_TUNE_WAIT_MS;
    priv->tune_poll_ms = RF350K_TUNE_POLL_MS;
    pthread_mutex_init(&priv->mutex, NULL);
    /* DTR and RTS are left at the OS default (RIG_SIGNAL_UNSET); the state
     * is per rig, never in the shared caps */
    STATE(rig)->priv = priv;
    return RIG_OK;
}

static int rf350k_cleanup(RIG *rig)
{
    struct rf350k_priv *priv = STATE(rig)->priv;

    rig_debug(RIG_DEBUG_VERBOSE, "%s called\n", __func__);

    if (priv)
    {
        /* a frontend failure after rf350k_open returned never calls close */
        if (priv->runflag)
        {
            priv->runflag = 0;
            pthread_join(priv->thread, NULL);
        }

        pthread_mutex_destroy(&priv->mutex);
        free(priv);
    }

    STATE(rig)->priv = NULL;
    return RIG_OK;
}

/* the thread's tick */
#define RF350K_TICK_MS      250
/* the worst case is two periods, each late by one tick, plus one
 * sync budget */
_Static_assert(2 * (RF350K_SYNC_PERIOD_MS + RF350K_TICK_MS) + RF350K_SYNC_MS
               <= RF350K_DEADMAN_MS,
               "one lost sync must still reach the radio inside its deadman");
/* the pause's "more than one whole second ago" shortcut */
_Static_assert(RF350K_PAUSE_MS < 1000,
               "the pause must stay under one second");

static int rf350k_not_remote(const char *func)
{
    rig_debug(RIG_DEBUG_ERR,
              "%s: the radio is not in REMOTE - press 2nd then the down arrow on the front panel, then reconnect\n",
              func);
    return -RIG_ENAVAIL;
}

/* a setter's write the tune gate refused */
static int rf350k_tune_busy(const char *func)
{
    rig_debug(RIG_DEBUG_ERR,
              "%s: the coupler is tuning, so nothing was sent - try again when the tune has ended\n",
              func);
    return -RF350K_EBUSY;
}

/* the deadman sync's failure count: one WARN per outage */
static void rf350k_sync_result(struct rf350k_priv *priv, int ok)
{
    if (ok)
    {
        priv->sync_failures = 0;
        return;
    }

    if (priv->sync_failures++ == 0)
    {
        rig_debug(RIG_DEBUG_WARN,
                  "%s: no answer to the deadman sync - the radio unkeys and lapses after about 10 s without one; check the link\n",
                  __func__);
    }
    else
    {
        rig_debug(RIG_DEBUG_VERBOSE,
                  "%s: no answer to the deadman sync (%d in a row)\n",
                  __func__, priv->sync_failures);
    }
}

enum rf350k_ptt_event
{
    RF350K_PTT_ON,
    RF350K_PTT_OFF,
    RF350K_PTT_TICK,
    RF350K_PTT_CLOSE,
};

/*
 * The one place the PTT state changes, applied after the event's
 * transactions.  held and prior_ext are ptt_held and ext_key before the
 * event; refused is the gate's final refusal of a K (when the backend held
 * the key, the caller has then sent a U); ret and key are the last
 * transaction's code and its bare U (0) or K (1) answer, except after a
 * refused key-on, where rf350k_set_ptt passes the K's code and neither is
 * looked at.  A K answering our own K is not external keying, so ext_key
 * keeps its prior value there.  Returns the event's code.
 */
static int rf350k_ptt_row(struct rf350k_priv *priv, enum rf350k_ptt_event ev,
                          int held, int refused, int ret, int key, int prior_ext)
{
    const int answered = ret == RIG_OK;

    switch (ev)
    {
    case RF350K_PTT_ON:
        if (refused)
        {
            priv->ptt_held = 0;
            priv->tune_confirm = 0;

            if (held)
            {
                rig_debug(RIG_DEBUG_WARN, "%s: REMOTE was left while keyed; unkeyed\n",
                          __func__);
            }

            return -RIG_ENAVAIL;
        }

        if (!answered)
        {
            priv->ptt_held = 0;
            priv->tune_confirm = 0;
            rig_debug(RIG_DEBUG_WARN,
                      "%s: the key state is unknown; the next deadman sync sends U\n", __func__);
            return ret;
        }

        if (key)
        {
            priv->ptt_held = 1;
            priv->ext_key = prior_ext;
            /* a K feeds the radio's deadman as a sync does */
            gettimeofday(&priv->last_sync, NULL);
            return RIG_OK;
        }

        priv->ptt_held = 0;
        priv->tune_confirm = 0;
        rig_debug(RIG_DEBUG_ERR,
                  "%s: the radio answered U to K - PTT enable, REMOTE or the mode refused the key\n",
                  __func__);
        return -RIG_ERJCTED;

    case RF350K_PTT_OFF:
        priv->ptt_held = 0;
        priv->tune_confirm = 0;

        if (answered && key)
        {
            priv->ext_key = 1;
            rig_debug(RIG_DEBUG_WARN, "%s: an external PTT still keys the radio\n",
                      __func__);
        }

        return ret;

    case RF350K_PTT_TICK:
        rf350k_sync_result(priv, answered);

        if (held && refused)
        {
            priv->ptt_held = 0;
            priv->tune_confirm = 0;
            rig_debug(RIG_DEBUG_WARN, "%s: REMOTE was left while keyed; unkeyed\n",
                      __func__);
        }
        else if (held && answered && key)
        {
            priv->ext_key = prior_ext;
        }
        else if (held && answered)
        {
            priv->ptt_held = 0;
            priv->tune_confirm = 0;
            rig_debug(RIG_DEBUG_WARN, "%s: the radio dropped the key\n", __func__);
        }
        else if (!held && answered)
        {
            priv->ext_key = key;
        }

        return ret;

    case RF350K_PTT_CLOSE:
        priv->ptt_held = 0;
        priv->tune_confirm = 0;

        if (!answered)
        {
            rig_debug(RIG_DEBUG_WARN,
                      "%s: the unkey at close was not answered - the radio's deadman unkeys it within about 10 s\n",
                      __func__);
        }

        return ret;
    }

    return -RIG_EINTERNAL;
}

/*
 * The ONLY place a U is written with RF350K_IN_TUNE, once per tune.  The
 * radio keeps it and unkeys at the tune's end (measured on the radio);
 * whatever comes back (silence, a K, a U) proves nothing about the key, so
 * only a failed link is returned.  The caller holds priv->mutex.
 */
static int rf350k_tune_unkey(RIG *rig)
{
    struct rf350k_priv *priv = STATE(rig)->priv;
    struct rf350k_reply r;
    int ret;

    if (priv->tune_unkey_sent)
    {
        return RIG_OK;
    }

    ret = rf350k_transaction(rig, "U", "U", priv->sync_timeout_ms,
                             RF350K_IN_TUNE | RF350K_QUIET, &r);
    priv->tune_unkey_sent = 1;
    return ret == -RIG_EIO ? -RIG_EIO : RIG_OK;
}

/*
 * A key-off asked for while a tune is believed.  The mutex is held
 * throughout, so no sync runs; the status reads stand in its place.  RIG_OK
 * only when the radio's own dump, read after the U was written, shows it
 * unkeyed: no bare K or U line can be told from one the radio pushed for
 * another reason.  ONE deadline (tune_wait_ms) from the path's entry,
 * whatever the belief does meanwhile, tested at the top of every turn; n
 * counts the ordinary unkeys written.  Every exit is through the OFF row.
 */
static int rf350k_tune_off(RIG *rig, int held, int prior_ext)
{
    struct rf350k_priv *priv = STATE(rig)->priv;
    struct rf350k_reply r;
    struct timeval t_call;
    int code, star, key;
    int n = 0;
    long ms;

    gettimeofday(&t_call, NULL);
    code = rf350k_tune_unkey(rig);

    while (code == RIG_OK)
    {
        ms = rf350k_elapsed_ms(&t_call);

        if (ms < 0 || ms >= priv->tune_wait_ms)
        {
            code = -RIG_ETIMEOUT;
            break;
        }

        code = rf350k_tune_read(rig, &star, &key);

        /* unkeyed only on a dump whose own * line is *0; a *1 dump carries
         * no key state (see rf350k_sync) */
        if (code == -RIG_EIO || (code == RIG_OK && key == 0 && star == 0))
        {
            break;
        }

        if (code == RIG_OK && star == 0)
        {
            /* the tune is over and the radio is keyed: the in-tune U was
             * lost, or a handset is pressed; the two cannot be told apart */
            if (n == priv->ask_writes)
            {
                rig_debug(RIG_DEBUG_ERR,
                          "%s: the radio still reads keyed after the tune and the unkeys - release the handset if it is pressed, else check the link\n",
                          __func__);
                code = -RIG_ETIMEOUT;
                break;
            }

            code = rf350k_transaction(rig, "U", "U", priv->sync_timeout_ms, 0, &r);

            if (code == -RIG_EIO)
            {
                break;
            }

            /* refused: its drain found a new *1, and nothing was written */
            n += !r.refused_tune;
            code = RIG_OK;
            continue;
        }

        /* K with *1, a read not answered, or a reply that is no whole dump */
        code = RIG_OK;
        hl_usleep(priv->tune_poll_ms * 1000L);
    }

    return rf350k_ptt_row(priv, RF350K_PTT_OFF, held, 0, code, 0, prior_ext);
}

/*
 * The panel's PTT enable ('1) and the local audio routing ("1, so the
 * source chosen on the panel stays live), each written only when its
 * cached value at the call is 0, the enable first; the second is
 * tried even when the first failed.  Nothing is written while a tune is
 * believed (-RF350K_EBUSY).  The caller holds priv->mutex.
 */
static int rf350k_assert_local(RIG *rig)
{
    struct rf350k_priv *priv = STATE(rig)->priv;
    const int pttena = priv->pttena;
    const int route = priv->route;
    struct rf350k_reply r;
    int ret = RIG_OK;
    int ret2 = RIG_OK;

    if (rf350k_tune_believed(priv))
    {
        return -RF350K_EBUSY;
    }

    if (pttena == 0)
    {
        ret = rf350k_transaction(rig, "'1\n", "'", priv->sync_timeout_ms,
                                 RF350K_NEEDS_REMOTE, &r);

        if (r.refused_tune)
        {
            return -RF350K_EBUSY;
        }
    }

    if (route == 0)
    {
        ret2 = rf350k_transaction(rig, "\"1\n", "\"", priv->sync_timeout_ms,
                                  RF350K_NEEDS_REMOTE, &r);

        if (r.refused_tune)
        {
            return -RF350K_EBUSY;
        }
    }

    return ret != RIG_OK ? ret : ret2;
}

/* One deadman sync: K while the backend holds PTT, else U.
 * While a tune is believed, a status read. */
static void rf350k_sync(RIG *rig)
{
    struct rf350k_priv *priv = STATE(rig)->priv;
    const int held = priv->ptt_held;
    const int prior_ext = priv->ext_key;
    struct rf350k_reply r;
    int tune = rf350k_tune_believed(priv);
    int refused = 0;
    int ret = RIG_OK;
    int key = -1;
    int star;

    /* the attempt's start is the anchor, success or not, so a dead link is
     * retried once per period */
    gettimeofday(&priv->last_sync, NULL);
    rig_debug(RIG_DEBUG_TRACE, "%s: the deadman sync, PTT %s\n", __func__,
              held ? "held" : "not held");

    /* a belief past its hard end while held also waits for a whole *0 dump;
     * until one is read the sync stays the status read, so no K re-keys a
     * radio that ended its tune unkeyed */
    if (held && priv->tuning && !tune)
    {
        priv->tune_confirm = 1;
    }

    tune |= held && priv->tune_confirm;

    if (held && !tune)
    {
        /* the gate runs after the drain, so a queued R0 refuses the K; the
         * thread does not re-read */
        ret = rf350k_transaction(rig, "K", "U", priv->sync_timeout_ms,
                                 RF350K_NEEDS_REMOTE, &r);
        refused = r.refused_remote;
        tune = r.refused_tune;
        key = r.key;
    }

    if ((!held || refused) && !tune)
    {
        ret = rf350k_transaction(rig, "U", "U", priv->sync_timeout_ms, 0, &r);
        tune = r.refused_tune;
        key = r.key;
    }

    if (tune)
    {
        /* the status read IS the sync: the row gets the dump's own key */
        ret = rf350k_tune_read(rig, &star, &key);

        /* a dump whose own * line is *1 carries no key state: at a tune's
         * end one read U and the radio then keyed again by itself (seen on
         * the radio).  The first dump with *0 decides */
        if (ret == RIG_OK && star == 1 && key == 0)
        {
            key = held ? 1 : prior_ext;
        }

        /* the owed *0 dump; its key line decides in the row */
        if (ret == RIG_OK && star == 0)
        {
            priv->tune_confirm = 0;
        }

        /* REMOTE left while the backend holds the key: the belief is given
         * up, and from here the driver acts as it does when no tune runs */
        if (held && priv->remote != 1)
        {
            priv->tune_since.tv_sec = 0;
            priv->tune_since.tv_usec = 0;
            ret = rf350k_transaction(rig, "U", "U", priv->sync_timeout_ms, 0, &r);
            refused = 1;
            key = r.key;
        }
    }

    rf350k_ptt_row(priv, RF350K_PTT_TICK, held, refused, ret, key, prior_ext);

    /* a REMOTE re-entry pushes R1 "0 '0 (measured on the radio), read by
     * this sync's drain; a tune's -RF350K_EBUSY is neither a failed
     * assertion nor a success */
    if (priv->remote == 1 && (priv->pttena == 0 || priv->route == 0))
    {
        ret = rf350k_assert_local(rig);

        if (ret == RIG_OK)
        {
            priv->assert_failures = 0;
        }
        else if (ret != -RF350K_EBUSY && priv->assert_failures++ == 0)
        {
            rig_debug(RIG_DEBUG_WARN,
                      "%s: the PTT enable or local audio command was not confirmed; retrying each sync\n",
                      __func__);
        }
    }
}

/*
 * The deadman thread, after rigs/anytone/anytone.c.
 * Both locks are tried, never waited for: priv->mutex keeps a sync out of a
 * rig function's transactions, and the frontend's mutex_set_transaction
 * keeps it out of a rig_send_raw write (rigctl "w").
 */
static void *rf350k_thread(void *arg)
{
    RIG *rig = arg;
    struct rf350k_priv *priv = STATE(rig)->priv;
    long since;

    while (priv->runflag)
    {
        hl_usleep(RF350K_TICK_MS * 1000);

        /* the transaction_active test is cheap; the trylock is what excludes */
        if (!priv->runflag || STATE(rig)->transaction_active
                || pthread_mutex_trylock(&priv->mutex) != 0)
        {
            continue;
        }

        /* a clock put back gives a negative time: the sync is due */
        since = rf350k_elapsed_ms(&priv->last_sync);

        if ((since < 0 || since >= priv->sync_period_ms)
                && pthread_mutex_trylock(&STATE(rig)->mutex_set_transaction) == 0)
        {
            rf350k_sync(rig);
            pthread_mutex_unlock(&STATE(rig)->mutex_set_transaction);
        }

        pthread_mutex_unlock(&priv->mutex);
    }

    rig_debug(RIG_DEBUG_TRACE, "%s: stopped\n", __func__);
    return NULL;
}

static int rf350k_open(RIG *rig)
{
    struct rf350k_priv *priv = STATE(rig)->priv;
    struct rf350k_reply r;
    struct rf350k_status st;
    int ret;
    int i;

    rig_debug(RIG_DEBUG_VERBOSE, "%s called\n", __func__);

    /* a reopened RIG must not carry a channel the ADAPT dump
     * cannot confirm */
    priv->channel = -1;
    priv->route = -1;
    priv->pttena = -1;
    priv->assert_failures = 0;
    rig_flush(RIGPORT(rig));

    /* asked like any read; every ADAPT dump has its "." (measured on the
     * radio), so a dump cut short is the timeout.  The tune state is not
     * reset here (it is kept across a close and reopen of the same rig, as
     * rigctld makes after an error); this read brings it up to date */
    ret = rf350k_transaction(rig, "?\n", ".", RF350K_READ_MS,
                             RF350K_ASK | RF350K_IN_TUNE, &r);

    /* the whole dump, so a radio with other firmware shows itself */
    for (i = 0; i < r.nlines; i++)
    {
        rig_debug(RIG_DEBUG_VERBOSE, "%s: dump line %d '%s'\n", __func__, i,
                  r.lines[i]);
    }

    if (ret == -RIG_ETIMEOUT)
    {
        rig_debug(RIG_DEBUG_ERR,
                  "%s: no status from the radio - check power, the J9 cable, the baud (19200 for the KO6NO adapter, 9600 for an A1A19) and, on a KO6NO adapter with its J3 handshake jumper at 1-2, that RTS is not forced off (rts_state)\n",
                  __func__);
        return ret;
    }

    if (ret != RIG_OK)
    {
        return ret;
    }

    ret = rf350k_parse_dump(&r, &st);

    if (ret != RIG_OK)
    {
        rig_debug(RIG_DEBUG_ERR,
                  "%s: the radio did not send a status dump - is this an RF-350K with the remote board fitted?\n",
                  __func__);
        return ret;
    }

    /* positive identification of the computer-control
     * format; nothing more is written to a radio in another */
    if (!st.adapt)
    {
        if (st.panel)
        {
            rig_debug(RIG_DEBUG_ERR,
                      "%s: the radio answered in the front-panel status format, not the computer-control (ADAPT) format - set the remote board to ADAPT (KO6NO adapter J2: jumpers on 5-6 and 3-4; Harris A1A19: S1 to 9), power-cycle the radio and reconnect\n",
                      __func__);
        }
        else
        {
            rig_debug(RIG_DEBUG_ERR,
                      "%s: the radio's status has no '^' field, so it is not the computer-control (ADAPT) format this model needs - check that the remote board is set to ADAPT, power-cycle the radio and reconnect\n",
                      __func__);
        }

        return -RIG_EPROTO;
    }

    if (st.remote == 0)
    {
        rig_debug(RIG_DEBUG_ERR,
                  "%s: the radio is not in REMOTE - press 2nd then the down arrow on the front panel, then reconnect\n",
                  __func__);
        return -RIG_ENAVAIL;
    }

    /* the dump is the truth */
    priv->remote = st.remote;

    if (st.channel_present)
    {
        priv->channel = st.channel;
    }

    priv->route = st.route;
    priv->pttena = st.pttena;

    /* the trailing X000 of a lapsed dump is left to the next drain */
    if (st.status == 2)
    {
        rig_debug(RIG_DEBUG_WARN,
                  "%s: the radio's deadman had lapsed before this open (X002); the first sync clears it\n",
                  __func__);
    }

    if (st.status >= 0)
    {
        SNPRINTF(priv->info, sizeof(priv->info), "RF-350K 604M, status X%03d",
                 st.status);
    }
    else
    {
        SNPRINTF(priv->info, sizeof(priv->info), "RF-350K 604M, status unknown");
    }

    /* no thread runs yet; the lock is the helper's contract.  Refused
     * during a tune is success here; the sync asserts later */
    pthread_mutex_lock(&priv->mutex);
    ret = rf350k_assert_local(rig);
    pthread_mutex_unlock(&priv->mutex);

    if (ret == -RF350K_EBUSY)
    {
        ret = RIG_OK;
    }

    if (ret != RIG_OK)
    {
        rig_debug(RIG_DEBUG_ERR,
                  "%s: the radio did not accept the PTT enable or local audio command - check that it is in REMOTE and that the remote board is on ADAPT\n",
                  __func__);
        return ret;
    }

    /* the open's "?" has proved the link; the first sync is due one period
     * from here */
    priv->ptt_held = 0;
    priv->sync_failures = 0;
    gettimeofday(&priv->last_sync, NULL);
    priv->runflag = 1;

    if (pthread_create(&priv->thread, NULL, rf350k_thread, rig) != 0)
    {
        priv->runflag = 0;
        rig_debug(RIG_DEBUG_ERR,
                  "%s: cannot start the deadman thread - close other programs and retry\n",
                  __func__);
        return -RIG_EINTERNAL;
    }

    return RIG_OK;
}

/* The thread first, so no sync follows the unkey. */
static int rf350k_close(RIG *rig)
{
    struct rf350k_priv *priv = STATE(rig)->priv;
    struct rf350k_reply r;
    int ret;

    rig_debug(RIG_DEBUG_VERBOSE, "%s called\n", __func__);

    if (priv->runflag)
    {
        priv->runflag = 0;
        pthread_join(priv->thread, NULL);
    }

    pthread_mutex_lock(&priv->mutex);

    /* a lost unkey is written again (three writes, 3 s on a radio that has
     * gone) */
    if (priv->ptt_held)
    {
        int tune = rf350k_tune_believed(priv);
        int key = -1;

        if (!tune)
        {
            ret = rf350k_transaction(rig, "U", "U", RF350K_SYNC_MS, RF350K_ASK_REMOTE,
                                     &r);
            tune = r.refused_tune;
            key = r.key;
        }

        /* the close does not wait for the tune's end; the radio keeps the U
         * and unkeys then (measured on the radio) */
        if (tune)
        {
            rf350k_tune_unkey(rig);
            ret = -RIG_ETIMEOUT;
        }

        rf350k_ptt_row(priv, RF350K_PTT_CLOSE, 1, 0, ret, key, priv->ext_key);
    }

    pthread_mutex_unlock(&priv->mutex);
    return RIG_OK;
}

/* F and C (and the F inside M F and A F) are never sent while the
 * backend holds PTT */
static int rf350k_busy(const struct rf350k_priv *priv, const char *func)
{
    if (!priv->ptt_held)
    {
        return 0;
    }

    rig_debug(RIG_DEBUG_ERR,
              "%s: PTT is held, so nothing was sent - unkey first (the radio refuses frequency and channel changes while keyed)\n",
              func);
    return 1;
}

/* the dump is the truth; the caller holds priv->mutex */
static int rf350k_read_dump(RIG *rig, struct rf350k_status *st)
{
    struct rf350k_reply r;
    int ret = rf350k_transaction(rig, "?\n", ".", RF350K_READ_MS,
                                 RF350K_ASK | RF350K_IN_TUNE, &r);

    if (ret != RIG_OK)
    {
        return ret;
    }

    if (rf350k_parse_dump(&r, st) != RIG_OK)
    {
        return -RIG_EPROTO;
    }

    return RIG_OK;
}

static int rf350k_absent(char letter, const char *func)
{
    rig_debug(RIG_DEBUG_ERR, "%s: the radio's status dump carries no %c field\n",
              func, letter);
    return -RIG_ENAVAIL;
}

/*
 * A write the gate refused is withheld at once; the next push or status
 * read corrects a backend that wrongly believes the radio is in local.  ask
 * is RF350K_ASK (the gain write only) or 0.  The caller holds priv->mutex.
 */
static int rf350k_gated(RIG *rig, const char *wire, const char *expect,
                        int deadline_ms, int ask, struct rf350k_reply *r,
                        const char *func)
{
    int ret = rf350k_transaction(rig, wire, expect, deadline_ms,
                                 RF350K_NEEDS_REMOTE | ask, r);

    return r->refused_tune ? rf350k_tune_busy(func)
           : r->refused_remote ? rf350k_not_remote(func) : ret;
}

/* an F the radio confirmed has left any channel (measured on the radio),
 * whatever the rest of the reply did; the C;; it pushes later may come too
 * late */
static void rf350k_left_channel(struct rf350k_priv *priv,
                                const struct rf350k_reply *r)
{
    int i;

    for (i = 0; i < r->nlines; i++)
    {
        if (r->lines[i][0] == 'F' && rf350k_digits(r->lines[i] + 1, 7))
        {
            priv->channel = -1;
        }
    }
}

static int rf350k_get_freq(RIG *rig, vfo_t vfo, freq_t *freq)
{
    struct rf350k_priv *priv = STATE(rig)->priv;
    struct rf350k_status st;
    int ret;

    pthread_mutex_lock(&priv->mutex);
    ret = rf350k_read_dump(rig, &st);
    pthread_mutex_unlock(&priv->mutex);

    if (ret == RIG_OK)
    {
        *freq = st.hz;
    }

    return ret;
}

static int rf350k_set_freq(RIG *rig, vfo_t vfo, freq_t freq)
{
    struct rf350k_priv *priv = STATE(rig)->priv;
    struct rf350k_reply r;
    char cmd[RF350K_CMD_MIN];
    int ret = rf350k_cmd_freq(freq, cmd, sizeof(cmd));

    if (ret != RIG_OK)
    {
        return ret;
    }

    pthread_mutex_lock(&priv->mutex);

    /* F works outside REMOTE (NJ7C document) */
    if (rf350k_busy(priv, __func__))
    {
        ret = -RF350K_EBUSY;
    }
    else
    {
        ret = rf350k_transaction(rig, cmd, "F", RF350K_READ_MS, 0, &r);
        rf350k_left_channel(priv, &r);

        if (r.refused_tune)
        {
            ret = rf350k_tune_busy(__func__);
        }
    }

    pthread_mutex_unlock(&priv->mutex);
    return ret;
}

static int rf350k_get_mode(RIG *rig, vfo_t vfo, rmode_t *mode,
                           pbwidth_t *width)
{
    struct rf350k_priv *priv = STATE(rig)->priv;
    struct rf350k_status st;
    int ret;

    pthread_mutex_lock(&priv->mutex);
    ret = rf350k_read_dump(rig, &st);
    pthread_mutex_unlock(&priv->mutex);

    if (ret == RIG_OK)
    {
        *mode = st.mode;
        /* the frontend answers the caps' one filter, 2700 Hz */
        *width = RIG_PASSBAND_NORMAL;
    }

    return ret;
}

static int rf350k_set_mode(RIG *rig, vfo_t vfo, rmode_t mode, pbwidth_t width)
{
    struct rf350k_priv *priv = STATE(rig)->priv;
    struct rf350k_status st;
    struct rf350k_reply r;
    char cmd[RF350K_CMD_MIN];
    int ret;

    /* the mode is checked before anything is sent */
    ret = rf350k_cmd_mode_freq(mode, RF350K_HZ_MIN, cmd, sizeof(cmd));

    if (ret != RIG_OK)
    {
        return ret;
    }

    if (width != RIG_PASSBAND_NORMAL && width != RIG_PASSBAND_NOCHANGE)
    {
        rig_debug(RIG_DEBUG_VERBOSE,
                  "%s: the RF-350K has one filter; the width %ld Hz is ignored\n", __func__,
                  (long) width);
    }

    pthread_mutex_lock(&priv->mutex);

    if (rf350k_busy(priv, __func__))
    {
        pthread_mutex_unlock(&priv->mutex);
        return -RF350K_EBUSY;
    }

    /* the F that makes 604 firmware act on M resends the radio's own
     * frequency, and the old mode decides the CW extras */
    ret = rf350k_read_dump(rig, &st);

    if (ret == RIG_OK)
    {
        ret = rf350k_cmd_mode_freq(mode, st.hz, cmd, sizeof(cmd));
    }

    if (ret == RIG_OK)
    {
        ret = rf350k_transaction(rig, cmd, rf350k_expect_mode(st.mode, mode),
                                 RF350K_READ_MS, 0, &r);
        rf350k_left_channel(priv, &r);

        if (r.refused_tune)
        {
            ret = rf350k_tune_busy(__func__);
        }
    }

    pthread_mutex_unlock(&priv->mutex);
    return ret;
}

static int rf350k_set_level(RIG *rig, vfo_t vfo, setting_t level, value_t val)
{
    struct rf350k_priv *priv = STATE(rig)->priv;
    struct rf350k_status st;
    struct rf350k_reply r;
    char cmd[RF350K_CMD_MIN];
    int ret;
    int g;
    int n;

    switch (level)
    {
    case RIG_LEVEL_AGC:
        /* the AGC value is checked before anything is sent */
        ret = rf350k_cmd_agc_freq(val.i, RF350K_HZ_MIN, cmd, sizeof(cmd));

        if (ret != RIG_OK)
        {
            return ret;
        }

        pthread_mutex_lock(&priv->mutex);

        if (rf350k_busy(priv, __func__))
        {
            pthread_mutex_unlock(&priv->mutex);
            return -RF350K_EBUSY;
        }

        ret = rf350k_read_dump(rig, &st);

        if (ret == RIG_OK)
        {
            ret = rf350k_cmd_agc_freq(val.i, st.hz, cmd, sizeof(cmd));
        }

        if (ret == RIG_OK)
        {
            ret = rf350k_gated(rig, cmd, "AF", RF350K_READ_MS, 0, &r, __func__);
            rf350k_left_channel(priv, &r);
        }

        pthread_mutex_unlock(&priv->mutex);
        return ret;

    case RIG_LEVEL_RF:
        ret = rf350k_cmd_rfgain(val.f, cmd, sizeof(cmd));

        if (ret != RIG_OK)
        {
            return ret;
        }

        /* G is never acknowledged, so a "?" goes in the same write.
         * Measured on the radio, its dump shows the new G in most writes,
         * a later one within about a second in the rest (25 and 15 of 40
         * lone writes), so the status is read again until it does; the
         * radio drops G while keyed */
        g = atoi(cmd + 1);
        strcat(cmd, "?\n");
        pthread_mutex_lock(&priv->mutex);
        ret = rf350k_gated(rig, cmd, ".", RF350K_READ_MS, RF350K_ASK, &r, __func__);

        if (ret == RIG_OK && rf350k_parse_dump(&r, &st) != RIG_OK)
        {
            ret = -RIG_EPROTO;
        }

        /* the order of the tests is binding: the gain before the key.  No
         * clock: each further read's pause is the pacing */
        for (n = 0; ret == RIG_OK; n++)
        {
            if (st.gain < 0)
            {
                ret = rf350k_absent('G', __func__);
                break;
            }

            if (st.gain == g)
            {
                priv->rfgain = g;
                break;
            }

            if (st.key == 1)
            {
                rig_debug(RIG_DEBUG_ERR,
                          "%s: the radio is keyed and ignores RF gain while keyed (its status still shows G%02d) - unkey, then set the gain again\n",
                          __func__, st.gain);
                ret = -RIG_ERJCTED;
                break;
            }

            if (n == RF350K_GAIN_READS)
            {
                rig_debug(RIG_DEBUG_ERR,
                          "%s: the radio's status shows RF gain G%02d after G%02d was sent\n",
                          __func__, st.gain, g);
                ret = -RIG_ERJCTED;
                break;
            }

            rig_debug(RIG_DEBUG_VERBOSE,
                      "%s: the radio's status shows RF gain G%02d, not yet G%02d; reading it again\n",
                      __func__, st.gain, g);
            ret = rf350k_read_dump(rig, &st);
        }

        pthread_mutex_unlock(&priv->mutex);
        return ret;

    default:
        rig_debug(RIG_DEBUG_ERR, "%s: the RF-350K has no level %s - use AGC or RF\n",
                  __func__, rig_strlevel(level));
        return -RIG_EINVAL;
    }
}

static int rf350k_get_level(RIG *rig, vfo_t vfo, setting_t level,
                            value_t *val)
{
    struct rf350k_priv *priv = STATE(rig)->priv;
    struct rf350k_status st;
    int ret;
    int g;

    if (level != RIG_LEVEL_AGC && level != RIG_LEVEL_RF)
    {
        rig_debug(RIG_DEBUG_ERR, "%s: the RF-350K has no level %s - use AGC or RF\n",
                  __func__, rig_strlevel(level));
        return -RIG_EINVAL;
    }

    pthread_mutex_lock(&priv->mutex);
    ret = rf350k_read_dump(rig, &st);
    g = ret != RIG_OK ? -1 : st.gain >= 0 ? st.gain : priv->rfgain;
    pthread_mutex_unlock(&priv->mutex);

    if (ret != RIG_OK)
    {
        return ret;
    }

    if (level == RIG_LEVEL_AGC)
    {
        if (st.agc < 0)
        {
            return rf350k_absent('A', __func__);
        }

        val->i = st.agc;
        return RIG_OK;
    }

    /* every ADAPT dump in REMOTE carries G; out of REMOTE it has none (both
     * measured on the radio), and then the value last set stands in */
    if (g < 0)
    {
        return rf350k_absent('G', __func__);
    }

    val->f = rf350k_gain_to_level(g);
    return RIG_OK;
}

static int rf350k_set_func(RIG *rig, vfo_t vfo, setting_t func, int status)
{
    struct rf350k_priv *priv = STATE(rig)->priv;
    struct rf350k_reply r;
    char cmd[RF350K_CMD_MIN];
    int ret;

    if (func != RIG_FUNC_MUTE)
    {
        rig_debug(RIG_DEBUG_ERR, "%s: the RF-350K has no function %s - use MUTE\n",
                  __func__, rig_strfunc(func));
        return -RIG_EINVAL;
    }

    ret = rf350k_cmd_mute(status, cmd, sizeof(cmd));

    if (ret != RIG_OK)
    {
        return ret;
    }

    pthread_mutex_lock(&priv->mutex);
    ret = rf350k_gated(rig, cmd, "(", RF350K_READ_MS, 0, &r, __func__);
    pthread_mutex_unlock(&priv->mutex);
    return ret;
}

static int rf350k_get_func(RIG *rig, vfo_t vfo, setting_t func, int *status)
{
    struct rf350k_priv *priv = STATE(rig)->priv;
    struct rf350k_status st;
    int ret;

    if (func != RIG_FUNC_MUTE)
    {
        rig_debug(RIG_DEBUG_ERR, "%s: the RF-350K has no function %s - use MUTE\n",
                  __func__, rig_strfunc(func));
        return -RIG_EINVAL;
    }

    pthread_mutex_lock(&priv->mutex);
    ret = rf350k_read_dump(rig, &st);
    pthread_mutex_unlock(&priv->mutex);

    if (ret != RIG_OK)
    {
        return ret;
    }

    if (st.audio_on < 0)
    {
        return rf350k_absent('(', __func__);
    }

    /* "(0" is audio OFF */
    *status = st.audio_on == 0;
    return RIG_OK;
}

static int rf350k_set_ptt(RIG *rig, vfo_t vfo, ptt_t ptt)
{
    struct rf350k_priv *priv = STATE(rig)->priv;
    struct rf350k_reply r;
    int held, prior_ext, refused;
    int ret;
    int i;

    pthread_mutex_lock(&priv->mutex);
    held = priv->ptt_held;
    prior_ext = priv->ext_key;

    if (ptt == RIG_PTT_OFF)
    {
        if (rf350k_tune_believed(priv))
        {
            ret = rf350k_tune_off(rig, held, prior_ext);
        }
        else
        {
            ret = rf350k_transaction(rig, "U", "U", RF350K_SYNC_MS, RF350K_ASK_REMOTE,
                                     &r);

            /* this U's own reply began the belief, so it was the tune's one
             * U and its answer proves nothing */
            if (!r.refused_tune && rf350k_tune_believed(priv))
            {
                priv->tune_unkey_sent = 1;
            }

            /* refused: its drain found a *1 */
            ret = r.refused_tune || rf350k_tune_believed(priv)
                  ? rf350k_tune_off(rig, held, prior_ext)
                  : rf350k_ptt_row(priv, RF350K_PTT_OFF, held, 0, ret, r.key, prior_ext);
        }
    }
    else
    {
        /* rf350k_gated's REMOTE gate, without its tune sentence */
        ret = rf350k_transaction(rig, "K", "U", RF350K_SYNC_MS, RF350K_NEEDS_REMOTE,
                                 &r);

        /* no K was written into the tune, and nothing changes; a backend
         * that holds the key holds it still */
        if (r.refused_tune && held)
        {
            ret = RIG_OK;
        }
        else if (r.refused_tune)
        {
            rig_debug(RIG_DEBUG_ERR,
                      "%s: the coupler is tuning - key again when it has ended\n", __func__);
            ret = -RF350K_EBUSY;
        }
        else
        {
            refused = r.refused_remote;

            if (refused)
            {
                ret = rf350k_not_remote(__func__);
            }

            /* REMOTE was left while we held the key; unkey, whatever
             * the U returns */
            if (refused && held)
            {
                rf350k_transaction(rig, "U", "U", RF350K_SYNC_MS, 0, &r);
            }

            ret = rf350k_ptt_row(priv, RF350K_PTT_ON, held, refused, ret, r.key,
                                 prior_ext);

            /* a K is answered only when no tune runs, so an &0 in its answer
             * starts a new one (as in all nine tune starts captured from
             * the radio), whatever tuning was */
            for (i = 0; ret == RIG_OK && i < r.nlines; i++)
            {
                if (strcmp(r.lines[i], "&0") == 0)
                {
                    priv->tuning = 1;
                    gettimeofday(&priv->tune_since, NULL);
                    priv->tune_unkey_sent = 0;
                    break;
                }
            }
        }
    }

    pthread_mutex_unlock(&priv->mutex);
    return ret;
}

/* our own key needs no question; otherwise a U answered K is a key */
static int rf350k_get_ptt(RIG *rig, vfo_t vfo, ptt_t *ptt)
{
    struct rf350k_priv *priv = STATE(rig)->priv;
    struct rf350k_reply r;
    int ret = RIG_OK;

    pthread_mutex_lock(&priv->mutex);

    if (priv->ptt_held)
    {
        *ptt = RIG_PTT_ON;
    }
    else
    {
        int tune = rf350k_tune_believed(priv);
        int star, key;

        if (!tune)
        {
            ret = rf350k_transaction(rig, "U", "U", RF350K_SYNC_MS, RF350K_ASK_REMOTE,
                                     &r);
            tune = r.refused_tune;

            if (!tune && ret == RIG_OK)
            {
                *ptt = r.key ? RIG_PTT_ON : RIG_PTT_OFF;
            }
        }

        /* no U into a tune; the dump's own key */
        if (tune)
        {
            ret = rf350k_tune_read(rig, &star, &key);

            if (ret == RIG_OK)
            {
                *ptt = key ? RIG_PTT_ON : RIG_PTT_OFF;
            }
        }
    }

    pthread_mutex_unlock(&priv->mutex);
    return ret;
}

static int rf350k_set_mem(RIG *rig, vfo_t vfo, int ch)
{
    struct rf350k_priv *priv = STATE(rig)->priv;
    struct rf350k_reply r;
    char cmd[RF350K_CMD_MIN];
    int ret = rf350k_cmd_chan(ch, cmd, sizeof(cmd));

    if (ret != RIG_OK)
    {
        return ret;
    }

    pthread_mutex_lock(&priv->mutex);

    if (rf350k_busy(priv, __func__))
    {
        ret = -RF350K_EBUSY;
    }
    else
    {
        ret = rf350k_gated(rig, cmd, "C", RF350K_READ_MS, 0, &r, __func__);

        if (ret == RIG_OK)
        {
            priv->channel = ch;
        }
    }

    pthread_mutex_unlock(&priv->mutex);
    return ret;
}

/* The dump's C field; the channel last set only when the dump has
 * no C field at all ("C;;" says none is selected). */
static int rf350k_get_mem(RIG *rig, vfo_t vfo, int *ch)
{
    struct rf350k_priv *priv = STATE(rig)->priv;
    struct rf350k_status st;
    int ret;
    int n;

    pthread_mutex_lock(&priv->mutex);
    ret = rf350k_read_dump(rig, &st);
    n = ret != RIG_OK ? -1 : st.channel_present ? st.channel : priv->channel;
    pthread_mutex_unlock(&priv->mutex);

    if (ret != RIG_OK)
    {
        return ret;
    }

    if (n < 0)
    {
        rig_debug(RIG_DEBUG_ERR, "%s: no channel is selected on the radio\n",
                  __func__);
        return -RIG_ENAVAIL;
    }

    *ch = n;
    return RIG_OK;
}

static const char *rf350k_get_info(RIG *rig)
{
    const struct rf350k_priv *priv = STATE(rig)->priv;

    return priv->info;
}

/* The figures are the TM's (IM 10085-0705C, PDF p16-17, printed xiv-xv,
 * "SPECIFICATIONS FOR RF-350/RF-350K/RF-366"): 1.600,00 to 29.999,99 MHz
 * in 10 Hz increments, SSB selectivity 350-3050 Hz at 3 dB, 100 W PEP
 * SSB/CW and 25 W AM carrier.  The TM says 100 channels; chan_list lists
 * the 50 this REV 604M radio answers (C00-C49; C50-C99 are silent,
 * measured on the radio). */
struct rig_caps rf350k_caps =
{
    RIG_MODEL(RIG_MODEL_RF350K),
    .model_name =       "RF-350K 604M",
    .mfg_name =         "Harris",
    .version =          "20261115.0",
    .copyright =        "LGPL",
    .status =           RIG_STATUS_BETA,
    .rig_type =         RIG_TYPE_TRANSCEIVER,
    .targetable_vfo =   RIG_TARGETABLE_NONE,
    .ptt_type =         RIG_PTT_RIG,
    .dcd_type =         RIG_DCD_NONE,
    .port_type =        RIG_PORT_SERIAL,
    /* rig_init opens at serial_rate_max, the KO6NO adapter's rate; an
     * A1A19 board needs 9600 */
    .serial_rate_min =  9600,
    .serial_rate_max =  19200,
    .serial_data_bits = 7,
    .serial_stop_bits = 1,
    .serial_parity =    RIG_PARITY_ODD,
    .serial_handshake = RIG_HANDSHAKE_NONE,
    .write_delay =      0,
    .post_write_delay = 0,
    .timeout =          500,
    .retry =            0,
    /* disables read_string's retry and rig_init's halving of the timeout */
    .timeout_retry =    -1,

    .has_get_func =     RIG_FUNC_MUTE,
    .has_set_func =     RIG_FUNC_MUTE,
    .has_get_level =    RIG_LEVEL_AGC | RIG_LEVEL_RF,
    .has_set_level =    RIG_LEVEL_AGC | RIG_LEVEL_RF,
    .has_get_parm =     RIG_PARM_NONE,
    .has_set_parm =     RIG_PARM_NONE,

    .chan_list = {
        { 0, RF350K_CHAN_MAX, RIG_MTYPE_MEM },
        RIG_CHAN_END,
    },

    .rx_range_list1 = {
        {
            .startf = Hz(RF350K_HZ_MIN), .endf = Hz(RF350K_HZ_MAX), .modes = RF350K_MODES,
            .low_power = -1, .high_power = -1, .vfo = RIG_VFO_A, .ant = RIG_ANT_1
        },
        RIG_FRNG_END,
    },
    .tx_range_list1 = {
        {
            .startf = Hz(RF350K_HZ_MIN), .endf = Hz(RF350K_HZ_MAX),
            .modes = RIG_MODE_USB | RIG_MODE_LSB | RIG_MODE_CW,
            .low_power = W(100), .high_power = W(100), .vfo = RIG_VFO_A, .ant = RIG_ANT_1
        },
        {
            .startf = Hz(RF350K_HZ_MIN), .endf = Hz(RF350K_HZ_MAX), .modes = RIG_MODE_AM,
            .low_power = W(25), .high_power = W(25), .vfo = RIG_VFO_A, .ant = RIG_ANT_1
        },
        RIG_FRNG_END,
    },
    .tuning_steps = { {RF350K_MODES, 10}, RIG_TS_END, },

    /* AME and CW are received through the SSB filter */
    .filters = {
        {RF350K_MODES, Hz(2700)},
        RIG_FLT_END,
    },

    .rig_init =         rf350k_init,
    .rig_cleanup =      rf350k_cleanup,
    .rig_open =         rf350k_open,
    .rig_close =        rf350k_close,
    .set_freq =         rf350k_set_freq,
    .get_freq =         rf350k_get_freq,
    .set_mode =         rf350k_set_mode,
    .get_mode =         rf350k_get_mode,
    .set_level =        rf350k_set_level,
    .get_level =        rf350k_get_level,
    .set_func =         rf350k_set_func,
    .get_func =         rf350k_get_func,
    .set_ptt =          rf350k_set_ptt,
    .get_ptt =          rf350k_get_ptt,
    .set_mem =          rf350k_set_mem,
    .get_mem =          rf350k_get_mem,
    .get_info =         rf350k_get_info,
    .hamlib_check_rig_caps = HAMLIB_CHECK_RIG_CAPS
};
