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

/*
 * The RF-5800H (and the AN/PRC-150 type) over its REMOTE ASCII port.  The
 * protocol shares nothing with the PRC-138 in harris.c.  Every line the radio
 * answers is classified by rf5800_feed; rf5800_transaction is the only code
 * that reads or writes the port.
 */

#include <errno.h>
#include <limits.h>
#include <math.h>
#include <stdlib.h>
#include <string.h>
#include <sys/time.h>

#include "hamlib/rig.h"
#include "hamlib/port.h"
#include "hamlib/rig_state.h"
#include "iofunc.h"
#include "rf5800.h"

#define RF5800_MODES (RIG_MODE_USB | RIG_MODE_LSB | RIG_MODE_CW | RIG_MODE_AM | RIG_MODE_FM)
#define RF5800_VFOS  (RIG_VFO_A | RIG_VFO_B)
#define RF5800_LEVELS (RIG_LEVEL_RFPOWER | RIG_LEVEL_SQL | RIG_LEVEL_AGC | RIG_LEVEL_RF)

#define RF5800_BLANK_MS 2000        /* deadline of a blank line (the Sync) */
/* During an ALE call the SSB word answers ALE> and the radio's SSB burst
 * follows unasked, its last byte 250 to 280 ms after that prompt in three
 * captures from the radio.  The first blank line is held back about four
 * times the longest burst seen, so the burst is drained as stale before it;
 * a burst that ended later than this poll would fall into the next
 * command's answer and fail that open. */
#define RF5800_CARRY_POLL_MS 1000   /* ms slept before each blank of the wait */
/* ten polls; the radio answered a blank at SSB> 56 ms after ALE> */
#define RF5800_CARRY_MS 10000       /* ms from the SSB word's return to give up */
#define RF5800_READ_MS  3000        /* deadline of every read and ordinary write */
#define RF5800_WAIT_MS  500         /* longest single wait for one byte */
#define RF5800_DRAIN_MAX 4096      /* most stale bytes drained before a write */
#define RF5800_MIN_HZ   1600000L    /* FREQ 1.6 accepted, 1.5 refused 7001 */
#define RF5800_MAX_HZ   59999990L   /* FREQ 59.9999 accepted, 60 refused 7001 */

/* The only lines the backend can send, each followed by CR */
const struct rf5800_grammar rf5800_command_list[] =
{
    { "", NULL },
    { "IDENTITY", NULL },
    { "SSB", NULL },
    { "PRESET ACTIVE CHAN 0", NULL },
    { "PRESET", NULL },
    { "CHAN", NULL },
    { "RADIO", NULL },
    { "POWER", NULL },
    { "FREQ", "<mhz>" },
    { "TXFREQ", "<mhz>" },
    { "MODE", "USB|LSB|AME|CW|FM" },
    { "BANDWIDTH", "350|500|1000|1500|2000|2400|2700|3000|4000|25000" },
    { "AGC", "OFF|SLOW|MED|FAST|DATA|AUTO" },
    { "POWER", "LOW|MED|HIGH" },
    { "RADIO SQUELCH", "ON|OFF" },
    { "RADIO SQ_LEVEL", "LOW|MED|HIGH" },
    { "RADIO RFGAIN", "<0-100>" },
    { "RETUNE", NULL },
};

const int rf5800_command_count = sizeof(rf5800_command_list) / sizeof(
                                     rf5800_command_list[0]);

static const char *const rf5800_prompts[] =
{
    "SSB>", "ALE>", "HOP>", "3G>", "PRGM>"
};

/* verified on the radio by read-back, ascending */
static const int rf5800_bw_ssb[] = { 2000, 2400, 2700, 3000 };
static const int rf5800_bw_cw[] = { 350, 500, 1000, 1500 };
static const int rf5800_bw_am[] = { 4000 };
static const int rf5800_bw_fm[] = { 25000 };

/* MED is the radio's word for MEDIUM; Hamlib has no DATA, USER stands in */
static const struct
{
    int agc;
    const char *word;
} rf5800_agc_words[] =
{
    { RIG_AGC_OFF, "OFF" },
    { RIG_AGC_SLOW, "SLOW" },
    { RIG_AGC_MEDIUM, "MED" },
    { RIG_AGC_FAST, "FAST" },
    { RIG_AGC_USER, "DATA" },
    { RIG_AGC_AUTO, "AUTO" },
};

static int rf5800_is_prompt(const char *s)
{
    size_t i;

    for (i = 0; i < sizeof(rf5800_prompts) / sizeof(rf5800_prompts[0]); i++)
    {
        if (strcmp(s, rf5800_prompts[i]) == 0)
        {
            return 1;
        }
    }

    return 0;
}

/* Copies s without its leading and trailing blanks; out holds n bytes. */
static void rf5800_trim(const char *s, char *out, size_t n)
{
    size_t len;

    while (*s == ' ' || *s == '\t' || *s == '\r')
    {
        s++;
    }

    len = strlen(s);

    while (len > 0 && (s[len - 1] == ' ' || s[len - 1] == '\t'
                       || s[len - 1] == '\r'))
    {
        len--;
    }

    if (len > n - 1)
    {
        len = n - 1;
    }

    memcpy(out, s, len);
    out[len] = '\0';
}

int rf5800_reply_init(struct rf5800_reply *r, const char *cmd)
{
    memset(r, 0, sizeof(*r));
    r->cmd = cmd ? cmd : "";
    r->push_channel = -1;
    r->push_manual = -1;
    return RIG_OK;
}

/* Only lines that arrived after this command was written are evidence;
 * lines fed from carry never set any of these fields. */
static int rf5800_evidence(const struct rf5800_reply *r)
{
    return r->echo_seen || r->echo_missing || r->nlines > 0 || r->dropped > 0
           || r->error != 0 || r->list_closed || r->cmd[0] == '\0';
}

/* Reads a decimal int at s; 0 when there are no digits, or the value
 * overflows long (ERANGE) or int.  *end is left after the digits. */
static int rf5800_int(const char *s, char **end, int *v)
{
    long x;

    errno = 0;
    x = strtol(s, end, 10);

    if (*end == s || errno == ERANGE || x < INT_MIN || x > INT_MAX)
    {
        return 0;
    }

    *v = (int) x;
    return 1;
}

static void rf5800_push(struct rf5800_reply *r, const char *t)
{
    static const char chan[] = "* CURRENT CHAN ";
    static const char preset[] = "* PRESET CHANGED TO ";
    static const char retune[] = "* RETUNE ID: ";
    static const char exception[] = "* EXCEPTION: ";
    char *end;
    int v;

    rig_debug(RIG_DEBUG_VERBOSE, "%s: %s\n", __func__, t);

    if (strncmp(t, chan, sizeof(chan) - 1) == 0)
    {
        if (rf5800_int(t + sizeof(chan) - 1, &end, &v) && *end == ':')
        {
            r->push_channel = v;
        }
        else
        {
            rig_debug(RIG_DEBUG_WARN, "%s: unreadable channel in '%s', ignored\n",
                      __func__, t);
        }
    }
    else if (strncmp(t, preset, sizeof(preset) - 1) == 0)
    {
        r->push_manual = strcmp(t + sizeof(preset) - 1, "MANUAL") == 0;
    }
    else if (strncmp(t, retune, sizeof(retune) - 1) == 0)
    {
        const char *last = strrchr(t, ' ') + 1;

        if (strcmp(last, "COMPLETE") == 0)
        {
            r->retune = 1;
        }
        else if (strcmp(last, "FAILED") == 0)
        {
            r->retune = 2;
        }
    }
    else if (strncmp(t, exception, sizeof(exception) - 1) == 0)
    {
        if (rf5800_int(t + sizeof(exception) - 1, &end, &v))
        {
            r->exception = v;
        }
        else
        {
            rig_debug(RIG_DEBUG_WARN, "%s: unreadable code in '%s', ignored\n",
                      __func__, t);
        }
    }
}

static void rf5800_prompt(struct rf5800_reply *r, const char *word,
                          int from_carry)
{
    if (from_carry)
    {
        rig_debug(RIG_DEBUG_VERBOSE, "%s: dropped stale prompt %s\n", __func__, word);
        return;
    }

    /* a prompt before any answer belongs to the previous command */
    if (!rf5800_evidence(r))
    {
        rig_debug(RIG_DEBUG_VERBOSE, "%s: ignored %s before the answer to '%s'\n",
                  __func__, word, r->cmd);
        return;
    }

    strcpy(r->prompt, word);
    r->done = 1;
}

static void rf5800_error_line(struct rf5800_reply *r, const char *t)
{
    const char *p = t + strlen("ERROR:");
    const char *q;
    char *end;
    int code;
    size_t len;

    /* a refusal must count even if its code is unreadable or overflows:
     * -1 stands for any such code */
    if (!rf5800_int(p, &end, &code) || code == 0)
    {
        code = -1;
    }

    r->error = code;
    q = strchr(end, '\'');

    if (q != NULL)
    {
        const char *close = strrchr(q + 1, '\'');

        p = q + 1;
        len = close != NULL ? (size_t)(close - p) : strlen(p);
    }
    else
    {
        p = end;

        while (*p == ' ')
        {
            p++;
        }

        len = strlen(p);
    }

    if (len > sizeof(r->error_text) - 1)
    {
        len = sizeof(r->error_text) - 1;
    }

    memcpy(r->error_text, p, len);
    r->error_text[len] = '\0';
}

/* Keeps a data line; a stale one is logged and dropped. */
static void rf5800_data_line(struct rf5800_reply *r, const char *line,
                             int from_carry)
{
    if (from_carry)
    {
        rig_debug(RIG_DEBUG_VERBOSE, "%s: dropped stale line '%s'\n", __func__,
                  line);
        return;
    }

    if (!r->echo_seen)
    {
        r->echo_missing = 1;
    }

    if (r->nlines < RF5800_MAX_LINES)
    {
        strcpy(r->lines[r->nlines++], line);
    }
    else
    {
        r->dropped++;
    }
}

/* Classifies one completed line (its trailing CR already removed). */
static void rf5800_line(struct rf5800_reply *r, const char *line,
                        int from_carry)
{
    char t[RF5800_LINE_LEN];
    char c[RF5800_LINE_LEN];

    rf5800_trim(line, t, sizeof(t));

    if (t[0] == '\0')
    {
        return;
    }

    if (strncmp(t, "* ", 2) == 0)
    {
        rf5800_push(r, t);
        return;
    }

    /* a prompt alone on its line, e.g. the stray SSB> of the power-up burst */
    if (rf5800_is_prompt(t))
    {
        rf5800_prompt(r, t, from_carry);
        return;
    }

    if (from_carry)
    {
        rig_debug(RIG_DEBUG_VERBOSE, "%s: dropped stale line '%s'\n", __func__, t);
        return;
    }

    if (strncmp(t, "ERROR:", 6) == 0)
    {
        rf5800_error_line(r, t);
        return;
    }

    if (strcmp(t, "END-OF-LIST") == 0)
    {
        r->list_closed = 1;
        return;
    }

    if (!r->echo_seen && !r->echo_missing)
    {
        rf5800_trim(r->cmd, c, sizeof(c));

        if (strcmp(t, c) == 0)
        {
            r->echo_seen = 1;
            return;
        }
    }

    rf5800_data_line(r, line, from_carry);
}

/*
 * The tokenizer.  A newline ends a line; a space ends one when the
 * line so far is exactly a prompt word, because the radio prints its prompt
 * as the word and a space with no newline.  Everything is decided byte by
 * byte, so the grouping of the bytes never changes the result.  A line is
 * classified with the provenance of its first byte.  Once the prompt has
 * completed the reply, later bytes are kept in partial for the next
 * transaction's carry.
 */
int rf5800_feed(struct rf5800_reply *r, const char *bytes, size_t len,
                int from_carry)
{
    const size_t full = sizeof(r->partial) - 1;
    char line[RF5800_LINE_LEN];
    size_t i;

    for (i = 0; i < len; i++)
    {
        char ch = bytes[i];
        size_t n = strlen(r->partial);
        int stale = r->partial_from_carry;

        if (r->done)
        {
            if (n < full)
            {
                r->partial[n] = ch;
                r->partial[n + 1] = '\0';
            }

            continue;
        }

        /* the tail of an overlong line, up to and including its newline */
        if (r->discard)
        {
            r->discard = ch != '\n';
            continue;
        }

        /* a NUL damages its whole line: dropped through its newline, so a
         * reply that needs that line fails as incomplete */
        if (ch == '\0')
        {
            rig_debug(RIG_DEBUG_WARN, "%s: a NUL in the line '%s', line dropped\n",
                      __func__, r->partial);
            r->partial[0] = '\0';
            r->partial_from_carry = 0;
            r->discard = 1;
            continue;
        }

        if (ch == '\n')
        {
            if (n > 0 && r->partial[n - 1] == '\r')
            {
                r->partial[n - 1] = '\0';
            }

            strcpy(line, r->partial);
            r->partial[0] = '\0';
            r->partial_from_carry = 0;
            rf5800_line(r, line, n > 0 ? stale : from_carry);
            continue;
        }

        if (ch == ' ' && rf5800_is_prompt(r->partial))
        {
            strcpy(line, r->partial);
            r->partial[0] = '\0';
            r->partial_from_carry = 0;
            rf5800_prompt(r, line, stale);
            continue;
        }

        /* the PRC-138 prints its prompt as CR, word, space; and a CR after a
         * full line is that line's terminator, not an overflow */
        if (ch == '\r' && (n == 0 || n == full))
        {
            continue;
        }

        /* the first 79 bytes are kept as data, never as a prompt or the echo */
        if (n == full)
        {
            strcpy(line, r->partial);
            r->partial[0] = '\0';
            r->partial_from_carry = 0;
            r->overlong++;
            r->discard = 1;
            rf5800_data_line(r, line, stale);
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

static int rf5800_buffer_ok(size_t n, const char *func)
{
    if (n >= RF5800_CMD_MIN)
    {
        return 1;
    }

    rig_debug(RIG_DEBUG_ERR,
              "%s: a %d-byte command buffer is too small (need %d) - this is a backend bug, please report it\n",
              func, (int) n, RF5800_CMD_MIN);
    return 0;
}

int rf5800_format_freq(freq_t hz, char *buf, size_t n)
{
    char tmp[RF5800_CMD_MIN];
    size_t len;
    long v;

    if (!rf5800_buffer_ok(n, __func__))
    {
        return -RIG_EINVAL;
    }

    /* the window applies to the value asked for, before rounding to 1 Hz */
    if (!isfinite(hz) || hz < RF5800_MIN_HZ || hz > RF5800_MAX_HZ)
    {
        rig_debug(RIG_DEBUG_ERR,
                  "%s: %.0f Hz is outside the radio's 1.6-59.99999 MHz range - choose a frequency inside it\n",
                  __func__, hz);
        return -RIG_EINVAL;
    }

    v = lround(hz);
    SNPRINTF(tmp, sizeof(tmp), "%ld.%06ld", v / 1000000L, v % 1000000L);
    len = strlen(tmp);

    /* the shortest spelling, 2500000 -> 2.5, as captured from the radio */
    while (tmp[len - 1] == '0')
    {
        tmp[--len] = '\0';
    }

    if (tmp[len - 1] == '.')
    {
        tmp[--len] = '\0';
    }

    strcpy(buf, tmp);
    return RIG_OK;
}

static int rf5800_cmd_mhz(const char *verb, freq_t hz, char *buf, size_t n,
                          const char *func)
{
    char mhz[RF5800_CMD_MIN];
    int ret;

    if (!rf5800_buffer_ok(n, func))
    {
        return -RIG_EINVAL;
    }

    ret = rf5800_format_freq(hz, mhz, sizeof(mhz));

    if (ret != RIG_OK)
    {
        return ret;
    }

    SNPRINTF(buf, n, "%s %s", verb, mhz);
    return RIG_OK;
}

int rf5800_cmd_freq(freq_t hz, char *buf, size_t n)
{
    return rf5800_cmd_mhz("FREQ", hz, buf, n, __func__);
}

int rf5800_cmd_txfreq(freq_t hz, char *buf, size_t n)
{
    return rf5800_cmd_mhz("TXFREQ", hz, buf, n, __func__);
}

int rf5800_cmd_mode(rmode_t mode, char *buf, size_t n)
{
    const char *word;

    if (!rf5800_buffer_ok(n, __func__))
    {
        return -RIG_EINVAL;
    }

    if (mode == RIG_MODE_USB) { word = "USB"; }
    else if (mode == RIG_MODE_LSB) { word = "LSB"; }
    else if (mode == RIG_MODE_CW) { word = "CW"; }
    /* the radio acknowledges MODE AM as MODE AME (captured) */
    else if (mode == RIG_MODE_AM) { word = "AME"; }
    else if (mode == RIG_MODE_FM) { word = "FM"; }
    else
    {
        rig_debug(RIG_DEBUG_ERR,
                  "%s: mode %s is not offered by the RF-5800H - use USB, LSB, CW, AM or FM\n",
                  __func__, rig_strrmode(mode));
        return -RIG_EINVAL;
    }

    SNPRINTF(buf, n, "MODE %s", word);
    return RIG_OK;
}

static const int *rf5800_widths(rmode_t mode, size_t *count)
{
    if (mode == RIG_MODE_USB || mode == RIG_MODE_LSB)
    {
        *count = sizeof(rf5800_bw_ssb) / sizeof(rf5800_bw_ssb[0]);
        return rf5800_bw_ssb;
    }

    if (mode == RIG_MODE_CW)
    {
        *count = sizeof(rf5800_bw_cw) / sizeof(rf5800_bw_cw[0]);
        return rf5800_bw_cw;
    }

    if (mode == RIG_MODE_AM)
    {
        *count = sizeof(rf5800_bw_am) / sizeof(rf5800_bw_am[0]);
        return rf5800_bw_am;
    }

    if (mode == RIG_MODE_FM)
    {
        *count = sizeof(rf5800_bw_fm) / sizeof(rf5800_bw_fm[0]);
        return rf5800_bw_fm;
    }

    return NULL;
}

int rf5800_snap_width(rmode_t mode, pbwidth_t width, int *hz)
{
    size_t count, i;
    const int *table = rf5800_widths(mode, &count);
    int best;

    if (table == NULL)
    {
        rig_debug(RIG_DEBUG_ERR,
                  "%s: mode %s has no RF-5800H bandwidths - use USB, LSB, CW, AM or FM\n",
                  __func__, rig_strrmode(mode));
        return -RIG_EINVAL;
    }

    /* the bounds come first: pbwidth_t is long, 64-bit on LP64, so no wider
     * type exists to subtract an extreme width in; inside the table's range
     * every distance is table-sized and cannot overflow */
    if (width <= table[0])
    {
        *hz = table[0];
        return RIG_OK;
    }

    if (width >= table[count - 1])
    {
        *hz = table[count - 1];
        return RIG_OK;
    }

    best = table[0];

    /* ascending table and <=: a tie goes to the wider value */
    for (i = 1; i < count; i++)
    {
        if (labs(width - table[i]) <= labs(width - best))
        {
            best = table[i];
        }
    }

    *hz = best;
    return RIG_OK;
}

int rf5800_cmd_bandwidth(int hz, char *buf, size_t n)
{
    static const rmode_t modes[] =
    {
        RIG_MODE_USB, RIG_MODE_CW, RIG_MODE_AM, RIG_MODE_FM
    };
    size_t m, i, count = 0;

    if (!rf5800_buffer_ok(n, __func__))
    {
        return -RIG_EINVAL;
    }

    for (m = 0; m < sizeof(modes) / sizeof(modes[0]); m++)
    {
        const int *table = rf5800_widths(modes[m], &count);

        for (i = 0; i < count; i++)
        {
            if (table[i] == hz)
            {
                SNPRINTF(buf, n, "BANDWIDTH %d", hz);
                return RIG_OK;
            }
        }
    }

    rig_debug(RIG_DEBUG_ERR,
              "%s: %d Hz is not an RF-5800H bandwidth - this is a backend bug, please report it\n",
              __func__, hz);
    return -RIG_EINVAL;
}

int rf5800_cmd_agc(int rig_agc, char *buf, size_t n)
{
    size_t i;

    if (!rf5800_buffer_ok(n, __func__))
    {
        return -RIG_EINVAL;
    }

    for (i = 0; i < sizeof(rf5800_agc_words) / sizeof(rf5800_agc_words[0]); i++)
    {
        if (rf5800_agc_words[i].agc == rig_agc)
        {
            SNPRINTF(buf, n, "AGC %s", rf5800_agc_words[i].word);
            return RIG_OK;
        }
    }

    rig_debug(RIG_DEBUG_ERR,
              "%s: AGC %s is not offered by the RF-5800H - use OFF, SLOW, MEDIUM, FAST, AUTO or USER (DATA)\n",
              __func__, rig_stragclevel(rig_agc));
    return -RIG_EINVAL;
}

int rf5800_agc_from_word(const char *word, int *rig_agc)
{
    size_t i;

    for (i = 0; i < sizeof(rf5800_agc_words) / sizeof(rf5800_agc_words[0]); i++)
    {
        if (strcmp(rf5800_agc_words[i].word, word) == 0)
        {
            *rig_agc = rf5800_agc_words[i].agc;
            return RIG_OK;
        }
    }

    rig_debug(RIG_DEBUG_ERR,
              "%s: the radio reported AGC '%s', which this backend does not know - report it with a -vvvvv trace\n",
              __func__, word);
    return -RIG_EINVAL;
}

/* <=0.33 LOW, <=0.66 MED, else HIGH (float constants: 0.33f as a double
 * is above 0.33) */
static const char *rf5800_third(float level)
{
    if (level <= 0.33f)
    {
        return "LOW";
    }

    if (level <= 0.66f)
    {
        return "MED";
    }

    return "HIGH";
}

static int rf5800_level_ok(float level, const char *func)
{
    if (isfinite(level))
    {
        return 1;
    }

    rig_debug(RIG_DEBUG_ERR,
              "%s: the level is not a finite number - pass a value from 0.0 to 1.0\n",
              func);
    return 0;
}

int rf5800_cmd_power(float level, char *buf, size_t n)
{
    if (!rf5800_buffer_ok(n, __func__) || !rf5800_level_ok(level, __func__))
    {
        return -RIG_EINVAL;
    }

    SNPRINTF(buf, n, "POWER %s", rf5800_third(level));
    return RIG_OK;
}

int rf5800_power_from_word(const char *word, float *level)
{
    if (strcmp(word, "LOW") == 0)
    {
        *level = 0.33f;
    }
    else if (strcmp(word, "MED") == 0)
    {
        *level = 0.66f;
    }
    else if (strcmp(word, "HIGH") == 0)
    {
        *level = 1.0f;
    }
    else
    {
        /* the radio answered POWER 12 after RADIO TXGAIN commands were
         * tried (captured) */
        rig_debug(RIG_DEBUG_ERR,
                  "%s: the radio reported POWER '%s' - set LOW, MED or HIGH on the KDU or with set_level RFPOWER\n",
                  __func__, word);
        return -RIG_EPROTO;
    }

    return RIG_OK;
}

int rf5800_cmd_squelch(int on, char *buf, size_t n)
{
    if (!rf5800_buffer_ok(n, __func__))
    {
        return -RIG_EINVAL;
    }

    SNPRINTF(buf, n, "RADIO SQUELCH %s", on ? "ON" : "OFF");
    return RIG_OK;
}

int rf5800_cmd_sq_level(float level, char *buf, size_t n)
{
    if (!rf5800_buffer_ok(n, __func__) || !rf5800_level_ok(level, __func__))
    {
        return -RIG_EINVAL;
    }

    SNPRINTF(buf, n, "RADIO SQ_LEVEL %s", rf5800_third(level));
    return RIG_OK;
}

int rf5800_cmd_rfgain(float level, char *buf, size_t n)
{
    float scaled;

    if (!rf5800_buffer_ok(n, __func__) || !rf5800_level_ok(level, __func__))
    {
        return -RIG_EINVAL;
    }

    /* clamp before scaling: a huge float would overflow the conversion */
    if (level < 0.0f)
    {
        level = 0.0f;
    }

    if (level > 1.0f)
    {
        level = 1.0f;
    }

    scaled = level * 100.0f;
    SNPRINTF(buf, n, "RADIO RFGAIN %ld", lroundf(scaled));
    return RIG_OK;
}

/* Copies the value of the LAST row that starts with prefix, trimmed (the
 * last of a duplicated row wins); returns 0 when there is no such row. */
static int rf5800_row(const struct rf5800_reply *r, const char *prefix,
                      char *val, size_t n)
{
    size_t plen = strlen(prefix);
    const char *found = NULL;
    int i;

    for (i = 0; i < r->nlines && i < RF5800_MAX_LINES; i++)
    {
        if (strncmp(r->lines[i], prefix, plen) == 0)
        {
            found = r->lines[i] + plen;
        }
    }

    if (found == NULL)
    {
        return 0;
    }

    rf5800_trim(found, val, n);
    return 1;
}

/* a number the whole token does not spell, or one out of int range, is a
 * protocol error */
static int rf5800_long(const char *s, long *v)
{
    char *end;
    int x;

    if (!rf5800_int(s, &end, &x) || *end != '\0')
    {
        return 0;
    }

    *v = x;
    return 1;
}

static int rf5800_mhz_to_hz(const char *s, freq_t *hz)
{
    char *end;
    double mhz = strtod(s, &end);

    double v;

    if (end == s || *end != '\0' || !isfinite(mhz))
    {
        return 0;
    }

    v = round(mhz * 1e6);

    if (!isfinite(v) || v < 0 || v > RF5800_PARSE_HZ_MAX)
    {
        return 0;
    }

    *hz = v;
    return 1;
}

static int rf5800_bad_row(const char *func, const char *what)
{
    rig_debug(RIG_DEBUG_ERR,
              "%s: the radio's answer has no usable %s row - retry, and report it with a -vvvvv trace if it persists\n",
              func, what);
    return -RIG_EPROTO;
}

int rf5800_parse_chan(const struct rf5800_reply *r, struct rf5800_chan *c)
{
    struct rf5800_chan out;
    char v[RF5800_LINE_LEN];
    long num;

    out.width = RIG_PASSBAND_NORMAL;
    out.agc = -1;
    out.number = -1;

    if (!rf5800_row(r, "CHAN CURRENT RXFREQ ", v, sizeof(v))
            || !rf5800_mhz_to_hz(v, &out.rx_hz))
    {
        return rf5800_bad_row(__func__, "CHAN CURRENT RXFREQ");
    }

    if (!rf5800_row(r, "CHAN CURRENT TXFREQ ", v, sizeof(v))
            || !rf5800_mhz_to_hz(v, &out.tx_hz))
    {
        return rf5800_bad_row(__func__, "CHAN CURRENT TXFREQ");
    }

    if (!rf5800_row(r, "CHAN CURRENT MODE ", v, sizeof(v)))
    {
        return rf5800_bad_row(__func__, "CHAN CURRENT MODE");
    }

    if (strcmp(v, "USB") == 0) { out.mode = RIG_MODE_USB; }
    else if (strcmp(v, "LSB") == 0) { out.mode = RIG_MODE_LSB; }
    else if (strcmp(v, "CW") == 0) { out.mode = RIG_MODE_CW; }
    else if (strcmp(v, "AME") == 0 || strcmp(v, "AM") == 0) { out.mode = RIG_MODE_AM; }
    else if (strcmp(v, "FM") == 0) { out.mode = RIG_MODE_FM; }
    else
    {
        return rf5800_bad_row(__func__, "CHAN CURRENT MODE");
    }

    /* the radio pads the width: BANDWIDTH 0350 (captured) */
    if (rf5800_row(r, "CHAN CURRENT BANDWIDTH ", v, sizeof(v)))
    {
        if (!rf5800_long(v, &num))
        {
            return rf5800_bad_row(__func__, "CHAN CURRENT BANDWIDTH");
        }

        out.width = num;
    }

    if (rf5800_row(r, "CHAN CURRENT AGC ", v, sizeof(v))
            && rf5800_agc_from_word(v, &out.agc) != RIG_OK)
    {
        return rf5800_bad_row(__func__, "CHAN CURRENT AGC");
    }

    if (rf5800_row(r, "CHAN CURRENT NUMBER ", v, sizeof(v)))
    {
        if (!rf5800_long(v, &num))
        {
            return rf5800_bad_row(__func__, "CHAN CURRENT NUMBER");
        }

        out.number = (int) num;
    }

    *c = out;
    return RIG_OK;
}

int rf5800_parse_radio(const struct rf5800_reply *r, struct rf5800_radio *rd)
{
    struct rf5800_radio out = { -1, -1, -1 };
    char v[RF5800_LINE_LEN];
    long num;

    if (rf5800_row(r, "RADIO SQUELCH ", v, sizeof(v)))
    {
        if (strcmp(v, "ON") == 0) { out.squelch = 1; }
        else if (strcmp(v, "OFF") == 0) { out.squelch = 0; }
        else { return rf5800_bad_row(__func__, "RADIO SQUELCH"); }
    }

    if (rf5800_row(r, "RADIO SQ_LEVEL ", v, sizeof(v)))
    {
        if (strcmp(v, "LOW") == 0) { out.sq_level = 0; }
        else if (strcmp(v, "MED") == 0) { out.sq_level = 1; }
        else if (strcmp(v, "HIGH") == 0) { out.sq_level = 2; }
        else { return rf5800_bad_row(__func__, "RADIO SQ_LEVEL"); }
    }

    if (rf5800_row(r, "RADIO RFGAIN ", v, sizeof(v)))
    {
        if (!rf5800_long(v, &num) || num < 0 || num > 100)
        {
            return rf5800_bad_row(__func__, "RADIO RFGAIN");
        }

        out.rfgain = (int) num;
    }

    if (out.squelch < 0 && out.sq_level < 0 && out.rfgain < 0)
    {
        return rf5800_bad_row(__func__, "RADIO SQUELCH, SQ_LEVEL or RFGAIN");
    }

    *rd = out;
    return RIG_OK;
}

int rf5800_parse_power(const struct rf5800_reply *r, float *level)
{
    char v[RF5800_LINE_LEN];

    if (!rf5800_row(r, "POWER ", v, sizeof(v)))
    {
        return rf5800_bad_row(__func__, "POWER");
    }

    return rf5800_power_from_word(v, level);
}

int rf5800_parse_preset(const struct rf5800_reply *r,
                        struct rf5800_preset *p)
{
    char v[RF5800_LINE_LEN];
    long num;
    int manual;

    if (!rf5800_row(r, "PRESET CURRENT NAME ", v, sizeof(v)))
    {
        return rf5800_bad_row(__func__, "PRESET CURRENT NAME");
    }

    manual = strcmp(v, "MANUAL") == 0;

    if (!rf5800_row(r, "PRESET CURRENT CHAN ", v, sizeof(v))
            || !rf5800_long(v, &num))
    {
        return rf5800_bad_row(__func__, "PRESET CURRENT CHAN");
    }

    p->manual = manual;
    p->channel = (int) num;
    return RIG_OK;
}

/* The PRC-138 answers IDENTITY with the bare line PRC-138 (captured). */
static int rf5800_says_prc138(const struct rf5800_reply *r)
{
    int i;

    for (i = 0; i < r->nlines && i < RF5800_MAX_LINES; i++)
    {
        char t[RF5800_LINE_LEN];

        rf5800_trim(r->lines[i], t, sizeof(t));

        if (strncmp(t, "PRC-138", 7) == 0 && (t[7] == '\0' || t[7] == ' '))
        {
            return 1;
        }
    }

    return 0;
}

int rf5800_parse_identity(const struct rf5800_reply *r, char *out, size_t n)
{
    char v[RF5800_LINE_LEN];

    if (n == 0)
    {
        return -RIG_EINVAL;
    }

    if (rf5800_says_prc138(r) || !rf5800_row(r, "IDENTITY ", v, sizeof(v))
            || v[0] == '\0')
    {
        return rf5800_bad_row(__func__, "IDENTITY");
    }

    strncpy(out, v, n - 1);
    out[n - 1] = '\0';
    return RIG_OK;
}

int rf5800_classify_open(const struct rf5800_reply *identity_reply,
                         int *verdict)
{
    char v[RF5800_LINE_LEN];

    if (rf5800_says_prc138(identity_reply))
    {
        *verdict = RF5800_OPEN_PRC138;
    }
    else if (!rf5800_row(identity_reply, "IDENTITY ", v, sizeof(v)))
    {
        *verdict = RF5800_OPEN_NO_IDENTITY;
    }
    else if (identity_reply->echo_missing)
    {
        *verdict = RF5800_OPEN_ECHO_OFF;
    }
    /* IDENTITY RF-5800H is the answer captured from the radio; AN/PRC-150(C)
     * is the radio's own name for that type in its RADIO TYPE list, and no
     * IDENTITY answer from one has been captured */
    else if (strncmp(v, "RF-5800", 7) == 0
             || strncmp(v, "AN/PRC-150(C)", 13) == 0)
    {
        *verdict = RF5800_OPEN_OK;
    }
    else
    {
        *verdict = RF5800_OPEN_NO_IDENTITY;
    }

    return RIG_OK;
}

static long rf5800_elapsed_ms(const struct timeval *t0)
{
    struct timeval t1;

    gettimeofday(&t1, NULL);
    return (t1.tv_sec - t0->tv_sec) * 1000L + (t1.tv_usec - t0->tv_usec) / 1000L;
}

/* The rows of a read that tell where the radio is; pushes say it too. */
static void rf5800_note_state(struct rf5800_priv *priv,
                              const struct rf5800_reply *r)
{
    char v[RF5800_LINE_LEN];
    long num;

    if (r->push_channel >= 0)
    {
        priv->channel = r->push_channel;
    }

    if (r->push_manual >= 0)
    {
        priv->manual = r->push_manual;
    }

    if (!r->done)
    {
        return;
    }

    if ((rf5800_row(r, "CHAN CURRENT NUMBER ", v, sizeof(v))
            || rf5800_row(r, "PRESET CURRENT CHAN ", v, sizeof(v)))
            && rf5800_long(v, &num))
    {
        priv->channel = (int) num;
    }

    if (rf5800_row(r, "PRESET CURRENT NAME ", v, sizeof(v)))
    {
        priv->manual = strcmp(v, "MANUAL") == 0;
    }
}

/*
 * The only function that reads or writes the port (rf5800_open's single
 * rig_flush aside).  After a failure the blank
 * Sync is this same function called with an empty command.
 */
int rf5800_transaction(RIG *rig, const char *cmd, int deadline_ms,
                       struct rf5800_reply *out)
{
    hamlib_port_t *rp = RIGPORT(rig);
    struct rf5800_priv *priv = STATE(rig)->priv;
    char wire[RF5800_LINE_LEN + 2];
    int timeout_save = rp->timeout;
    unsigned char b[2];
    struct timeval t0;
    int drained;
    int ret;
    int n;

    rig_debug(RIG_DEBUG_TRACE, "%s: '%s', %d ms\n", __func__, cmd, deadline_ms);

    /* after a failure a blank line must find a prompt first; a blank
     * command is that Sync itself */
    if (priv->need_sync && cmd[0] != '\0')
    {
        struct rf5800_reply sync;

        ret = rf5800_transaction(rig, "", RF5800_BLANK_MS, &sync);

        if (ret == -RIG_ETIMEOUT || ret == -RIG_EIO)
        {
            rf5800_reply_init(out, cmd);
            rig_debug(RIG_DEBUG_ERR,
                      "%s: the radio did not answer the resync blank line, so '%s' was not sent - check the radio, then reopen the rig\n",
                      __func__, cmd);
            return ret;
        }
    }

    rf5800_reply_init(out, cmd);

    /* no flush: it would discard the radio's KDU pushes; stale bytes are
     * classified instead, and only their pushes count */
    if (priv->carry[0] != '\0')
    {
        rf5800_feed(out, priv->carry, strlen(priv->carry), 1);
        priv->carry[0] = '\0';
    }

    /* whatever is already waiting arrived before this command was written,
     * so it is drained as stale.  The wait is 1 ms, not 0: on a
     * Windows COM port win32_serial_select (lib/termios.c) checks the input
     * queue only inside its while (timeout_usec > 0) loop, so a zero timeout
     * returns "nothing" without looking.  serial_setup copies rp->timeout
     * into VTIME once at open (src/serial.c), so this per-call change reaches
     * the select only, which is fine with VMIN = 1. */
    rp->timeout = RF5800_DRAIN_WAIT_MS;

    for (drained = 0; drained < RF5800_DRAIN_MAX; drained++)
    {
        b[0] = '\0';
        errno = 0;
        n = read_string(rp, b, sizeof(b), "\n>", 2, 1, 1);

        if (n <= 0 && b[0] == '\0')
        {
            /* nothing waiting; a transport error shows on the write or read */
            break;
        }

        rf5800_feed(out, (const char *) b, 1, 1);
    }

    if (drained == RF5800_DRAIN_MAX)
    {
        rig_debug(RIG_DEBUG_WARN,
                  "%s: drained %d stale bytes before '%s' and stopped there\n", __func__,
                  RF5800_DRAIN_MAX, cmd);
    }

    gettimeofday(&t0, NULL);
    SNPRINTF(wire, sizeof(wire), "%s\r", cmd);

    if (write_block(rp, (unsigned char *) wire, strlen(wire)) < 0)
    {
        ret = -RIG_EIO;
    }
    else
    {
        for (;;)
        {
            const long spent = rf5800_elapsed_ms(&t0);
            /* a clock put back gives a negative time: the budget is spent */
            long left = spent < 0 ? 0 : deadline_ms - spent;

            /* an elapsed-time budget: a radio that trickles bytes forever
             * still ends here */
            if (left <= 0)
            {
                ret = -RIG_ETIMEOUT;
                break;
            }

            /* one byte per call, so one call is one wait of at most this */
            rp->timeout = left < RF5800_WAIT_MS ? (int) left : RF5800_WAIT_MS;
            b[0] = '\0';
            /* read_string sleeps 5 ms per byte while errno holds a stale EAGAIN */
            errno = 0;
            n = read_string(rp, b, sizeof(b), "\n>", 2, 0, 1);

            if (n < 0 && n != -RIG_ETIMEOUT)
            {
                ret = -RIG_EIO;
                break;
            }

            /* a timed-out call may still have consumed a byte */
            if (n > 0 || b[0] != '\0')
            {
                rf5800_feed(out, (const char *) b, 1, 0);
            }

            if (out->done)
            {
                ret = RIG_OK;
                break;
            }
        }
    }

    rp->timeout = timeout_save;
    rf5800_note_state(priv, out);

    if (ret == -RIG_EIO)
    {
        priv->need_sync = 1;
        rig_debug(RIG_DEBUG_ERR,
                  "%s: the link to the radio failed during '%s' - check the cable or connection, then reopen the rig\n",
                  __func__, cmd);
        return ret;
    }

    if (ret == -RIG_ETIMEOUT)
    {
        priv->need_sync = 1;
        rig_debug(RIG_DEBUG_ERR,
                  "%s: no prompt from the radio within %d ms after '%s' - check that it is powered, cabled and at the configured baud rate\n",
                  __func__, deadline_ms, cmd);
        return ret;
    }

    strcpy(priv->prompt, out->prompt);
    strcpy(priv->carry, out->partial);
    priv->need_sync = 0;

    if (out->error != 0)
    {
        rig_debug(RIG_DEBUG_ERR,
                  "%s: the radio refused '%s' with ERROR %d '%s' - check the radio's mode and the value on the KDU\n",
                  __func__, cmd, out->error, out->error_text);
        return -RIG_ERJCTED;
    }

    return RIG_OK;
}

static int rf5800_init(RIG *rig)
{
    struct rf5800_priv *priv;

    rig_debug(RIG_DEBUG_VERBOSE, "%s called\n", __func__);
    priv = calloc(1, sizeof(*priv));

    if (!priv)
    {
        rig_debug(RIG_DEBUG_ERR, "%s: out of memory - close other programs and retry\n",
                  __func__);
        return -RIG_ENOMEM;
    }

    priv->channel = -1;
    priv->manual = -1;
    /* the two timings of the wait for SSB> after the SSB word, kept per rig;
     * the constants are their defaults */
    priv->carry_ms = RF5800_CARRY_MS;
    priv->carry_poll_ms = RF5800_CARRY_POLL_MS;
    /* per rig, never in the shared caps */
    STATE(rig)->priv = priv;
    return RIG_OK;
}

static int rf5800_cleanup(RIG *rig)
{
    rig_debug(RIG_DEBUG_VERBOSE, "%s called\n", __func__);
    free(STATE(rig)->priv);
    STATE(rig)->priv = NULL;
    return RIG_OK;
}

static int rf5800_at_ssb(const struct rf5800_priv *priv)
{
    return strcmp(priv->prompt, "SSB>") == 0;
}

/* Logs one row of a reply, or says it is missing (the gate and open checks
 * name the row they refused). */
static void rf5800_log_row(const char *func, const struct rf5800_reply *r,
                           const char *prefix)
{
    char v[RF5800_LINE_LEN];

    if (rf5800_row(r, prefix, v, sizeof(v)))
    {
        rig_debug(RIG_DEBUG_ERR, "%s:   row: %s%s\n", func, prefix, v);
    }
    else
    {
        rig_debug(RIG_DEBUG_ERR, "%s:   row: %s(missing)\n", func, prefix);
    }
}

/*
 * The checks after every write, in this precedence.  The radio
 * pushes "* CURRENT CHAN n:" when a write lands on a channel; any n but 0
 * means the operator moved the radio off MANUAL 0 between the gate and the
 * write, so nothing further may be sent.
 */
static int rf5800_after_write(const struct rf5800_reply *r, int ret,
                              const char *func)
{
    if (ret != RIG_OK)
    {
        return ret;
    }

    if (strcmp(r->prompt, "SSB>") != 0)
    {
        rig_debug(RIG_DEBUG_ERR,
                  "%s: the radio has left SSB mode (prompt %s after '%s') - put it back in SSB on the KDU and reconnect\n",
                  func, r->prompt, r->cmd);
        return -RIG_ENAVAIL;
    }

    if (r->push_channel > 0)
    {
        rig_debug(RIG_DEBUG_ERR,
                  "%s: the channel changed to %d during '%s' - leave the KDU on MANUAL channel 0 and retry\n",
                  func, r->push_channel, r->cmd);
        return -RIG_EPROTO;
    }

    return RIG_OK;
}

/* One PRESET read for the gate: it must end at SSB>. */
static int rf5800_gate_read(RIG *rig, struct rf5800_reply *r,
                            struct rf5800_preset *p)
{
    int ret = rf5800_transaction(rig, "PRESET", RF5800_READ_MS, r);

    if (ret != RIG_OK)
    {
        return ret;
    }

    if (strcmp(r->prompt, "SSB>") != 0)
    {
        rig_debug(RIG_DEBUG_ERR,
                  "%s: the radio has left SSB mode (prompt %s) - put it back in SSB on the KDU and reconnect\n",
                  __func__, r->prompt);
        return -RIG_ENAVAIL;
    }

    if (rf5800_parse_preset(r, p) != RIG_OK)
    {
        p->manual = -1;
        p->channel = -1;
    }

    return RIG_OK;
}

/*
 * The one guard before every write.  A device setting
 * needs only the SSB> prompt; a channel-scoped command needs a fresh PRESET
 * read showing MANUAL channel 0, re-armed once with PRESET ACTIVE CHAN 0.
 * The cached channel is never what opens the gate.
 */
int rf5800_write_gate(RIG *rig, int channel_scoped)
{
    struct rf5800_priv *priv = STATE(rig)->priv;
    struct rf5800_reply r;
    struct rf5800_preset p;
    int ret;

    if (!rf5800_at_ssb(priv))
    {
        rig_debug(RIG_DEBUG_ERR,
                  "%s: the radio has left SSB mode (prompt %s), so nothing was sent - put it back in SSB on the KDU and reconnect\n",
                  __func__, priv->prompt);
        return -RIG_ENAVAIL;
    }

    if (!channel_scoped)
    {
        return RIG_OK;
    }

    ret = rf5800_gate_read(rig, &r, &p);

    if (ret != RIG_OK || (p.manual == 1 && p.channel == 0))
    {
        return ret;
    }

    rig_debug(RIG_DEBUG_VERBOSE, "%s: not on MANUAL channel 0, re-arming\n",
              __func__);
    ret = rf5800_transaction(rig, "PRESET ACTIVE CHAN 0", RF5800_READ_MS, &r);

    if (ret != RIG_OK)
    {
        return ret;
    }

    if (!rf5800_at_ssb(priv))
    {
        rig_debug(RIG_DEBUG_ERR,
                  "%s: the radio has left SSB mode (prompt %s) - put it back in SSB on the KDU and reconnect\n",
                  __func__, priv->prompt);
        return -RIG_ENAVAIL;
    }

    ret = rf5800_gate_read(rig, &r, &p);

    if (ret != RIG_OK)
    {
        return ret;
    }

    if (p.manual != 1 || p.channel != 0)
    {
        rig_debug(RIG_DEBUG_ERR,
                  "%s: the radio is still not on MANUAL channel 0 after PRESET ACTIVE CHAN 0, so nothing was written - select MANUAL channel 0 on the KDU and retry\n",
                  __func__);
        rf5800_log_row(__func__, &r, "PRESET CURRENT NAME ");
        rf5800_log_row(__func__, &r, "PRESET CURRENT CHAN ");
        return -RIG_EPROTO;
    }

    return RIG_OK;
}

/* A gated single write and its checks. */
static int rf5800_write(RIG *rig, const char *cmd, int channel_scoped,
                        int deadline_ms, struct rf5800_reply *r)
{
    int ret = rf5800_write_gate(rig, channel_scoped);

    if (ret != RIG_OK)
    {
        return ret;
    }

    ret = rf5800_transaction(rig, cmd, deadline_ms, r);
    return rf5800_after_write(r, ret, __func__);
}

/* A read at any prompt; the radio refuses some at HOP> (7019). */
static int rf5800_read_chan(RIG *rig, struct rf5800_chan *c)
{
    struct rf5800_reply r;
    int ret = rf5800_transaction(rig, "CHAN", RF5800_READ_MS, &r);

    if (ret != RIG_OK)
    {
        return ret;
    }

    return rf5800_parse_chan(&r, c);
}

static int rf5800_read_radio(RIG *rig, struct rf5800_radio *rd)
{
    struct rf5800_reply r;
    int ret = rf5800_transaction(rig, "RADIO", RF5800_READ_MS, &r);

    if (ret != RIG_OK)
    {
        return ret;
    }

    return rf5800_parse_radio(&r, rd);
}

/* Find a prompt, make sure this is an RF-5800H, then SSB on MANUAL
 * channel 0. */
static int rf5800_open(RIG *rig)
{
    struct rf5800_priv *priv = STATE(rig)->priv;
    struct rf5800_reply r;
    struct rf5800_preset p;
    struct rf5800_chan c;
    int verdict;
    int ret;

    rig_debug(RIG_DEBUG_VERBOSE, "%s called\n", __func__);

    /* the only flush */
    rig_flush(RIGPORT(rig));
    ret = rf5800_transaction(rig, "", RF5800_BLANK_MS, &r);

    if (ret == -RIG_ETIMEOUT)
    {
        ret = rf5800_transaction(rig, "", RF5800_BLANK_MS, &r);
    }

    if (ret != RIG_OK)
    {
        rig_debug(RIG_DEBUG_ERR,
                  "%s: no prompt from the radio - check power, cable and baud\n", __func__);
        return ret;
    }

    ret = rf5800_transaction(rig, "IDENTITY", RF5800_READ_MS, &r);

    if (ret != RIG_OK)
    {
        rig_debug(RIG_DEBUG_ERR,
                  "%s: the radio did not answer IDENTITY - check that this is an RF-5800H on its REMOTE port\n",
                  __func__);
        return ret;
    }

    rf5800_classify_open(&r, &verdict);

    switch (verdict)
    {
    case RF5800_OPEN_PRC138:
        rig_debug(RIG_DEBUG_ERR,
                  "%s: the radio answers like a PRC-138 - use model %d\n", __func__,
                  RIG_MODEL_PRC138);
        return -RIG_EPROTO;

    case RF5800_OPEN_ECHO_OFF:
        rig_debug(RIG_DEBUG_ERR,
                  "%s: REMOTE ECHO is OFF - turn it on from the KDU (PGM > CONFIG > PORTS > REMOTE)\n",
                  __func__);
        return -RIG_EPROTO;

    case RF5800_OPEN_NO_IDENTITY:
        rig_debug(RIG_DEBUG_ERR,
                  "%s: the radio did not identify as an RF-5800H or AN/PRC-150(C) - check the model number and the port\n",
                  __func__);
        return -RIG_EPROTO;

    default:
        break;
    }

    rf5800_parse_identity(&r, priv->identity, sizeof(priv->identity));
    rig_debug(RIG_DEBUG_VERBOSE, "%s: %s at %s\n", __func__, priv->identity,
              priv->prompt);

    /* a mode word would commit a stranger's edit and ABORT would
     * discard it, so program mode is refused with nothing sent */
    if (strcmp(priv->prompt, "PRGM>") == 0)
    {
        rig_debug(RIG_DEBUG_ERR,
                  "%s: radio is in program mode - finish or abort the edit on the KDU, then reconnect\n",
                  __func__);
        return -RIG_EPROTO;
    }

    if (!rf5800_at_ssb(priv))
    {
        /* the captured answer is the SSB burst; this ends any ALE link, scan
         * or HOP net by design */
        ret = rf5800_transaction(rig, "SSB", RF5800_SSB_MS, &r);

        if (ret == -RIG_ERJCTED && r.error == 7021)
        {
            rig_debug(RIG_DEBUG_ERR,
                      "%s: radio is busy in ALE (scan or call) - stop it on the KDU and reconnect\n",
                      __func__);
            return ret;
        }

        if (ret != RIG_OK)
        {
            rig_debug(RIG_DEBUG_ERR,
                      "%s: the radio did not change to SSB mode - put it in SSB on the KDU and reconnect\n",
                      __func__);
            return ret;
        }

        /* during an ALE call the word aborts the call and answers ALE>;
         * the radio is in SSB by then and its burst follows unasked.  Only
         * blank lines are sent while waiting (a second mode word during the
         * change is unprobed); each blank's drain takes the burst as stale */
        if (!rf5800_at_ssb(priv))
        {
            struct timeval t_ssb;

            gettimeofday(&t_ssb, NULL);
            rig_debug(RIG_DEBUG_VERBOSE,
                      "%s: the radio answered SSB at %s; waiting for SSB>\n", __func__,
                      priv->prompt);

            /* a clock put back ends the wait */
            while (!rf5800_at_ssb(priv)
                    && rf5800_elapsed_ms(&t_ssb) >= 0
                    && rf5800_elapsed_ms(&t_ssb) < priv->carry_ms)
            {
                hl_usleep((rig_useconds_t) priv->carry_poll_ms * 1000);
                ret = rf5800_transaction(rig, "", RF5800_BLANK_MS, &r);

                if (ret != RIG_OK)
                {
                    rig_debug(RIG_DEBUG_ERR,
                              "%s: the radio did not change to SSB mode - put it in SSB on the KDU and reconnect\n",
                              __func__);
                    return ret;
                }
            }
        }

        if (!rf5800_at_ssb(priv))
        {
            rig_debug(RIG_DEBUG_ERR,
                      "%s: the radio answered SSB at %s - put it in SSB on the KDU and reconnect\n",
                      __func__, priv->prompt);
            return -RIG_EPROTO;
        }
    }

    /* PRESET ACTIVE CHAN 0, not PRESET SELECT 0, which leaves the channel
     * where it was (captured) */
    ret = rf5800_transaction(rig, "PRESET ACTIVE CHAN 0", RF5800_READ_MS, &r);

    if (ret != RIG_OK || !rf5800_at_ssb(priv))
    {
        rig_debug(RIG_DEBUG_ERR,
                  "%s: the radio did not select MANUAL channel 0 - select it on the KDU and reconnect\n",
                  __func__);
        return ret != RIG_OK ? ret : -RIG_EPROTO;
    }

    ret = rf5800_transaction(rig, "PRESET", RF5800_READ_MS, &r);

    if (ret != RIG_OK)
    {
        rig_debug(RIG_DEBUG_ERR,
                  "%s: the radio did not answer PRESET - check the connection and reconnect\n",
                  __func__);
        return ret;
    }

    if (rf5800_parse_preset(&r, &p) != RIG_OK || p.manual != 1)
    {
        rig_debug(RIG_DEBUG_ERR,
                  "%s: the radio is not on the MANUAL preset after PRESET ACTIVE CHAN 0 - select MANUAL channel 0 on the KDU and reconnect\n",
                  __func__);
        rf5800_log_row(__func__, &r, "PRESET CURRENT NAME ");
        return -RIG_EPROTO;
    }

    ret = rf5800_transaction(rig, "CHAN", RF5800_READ_MS, &r);

    if (ret != RIG_OK)
    {
        rig_debug(RIG_DEBUG_ERR,
                  "%s: the radio did not answer CHAN - check the connection and reconnect\n",
                  __func__);
        return ret;
    }

    if (rf5800_parse_chan(&r, &c) != RIG_OK || c.number != 0)
    {
        rig_debug(RIG_DEBUG_ERR,
                  "%s: the radio is not on channel 0 after PRESET ACTIVE CHAN 0 - select MANUAL channel 0 on the KDU and reconnect\n",
                  __func__);
        rf5800_log_row(__func__, &r, "CHAN CURRENT NUMBER ");
        return -RIG_EPROTO;
    }

    priv->channel = 0;
    priv->manual = 1;
    return RIG_OK;
}

static int rf5800_close(RIG *rig)
{
    /* nothing is sent: the radio stays in SSB on MANUAL channel 0 */
    rig_debug(RIG_DEBUG_VERBOSE, "%s called\n", __func__);
    return RIG_OK;
}

/* RX is VFO A, TX is VFO B; the radio itself has no VFOs. */
static int rf5800_set_freq(RIG *rig, vfo_t vfo, freq_t freq)
{
    char cmd[RF5800_CMD_MIN];
    struct rf5800_reply r;
    int ret;

    ret = vfo == RIG_VFO_B ? rf5800_cmd_txfreq(freq, cmd, sizeof(cmd))
          : rf5800_cmd_freq(freq, cmd, sizeof(cmd));

    if (ret != RIG_OK)
    {
        return ret;
    }

    return rf5800_write(rig, cmd, 1, RF5800_READ_MS, &r);
}

static int rf5800_get_freq(RIG *rig, vfo_t vfo, freq_t *freq)
{
    struct rf5800_chan c;
    int ret = rf5800_read_chan(rig, &c);

    if (ret == RIG_OK)
    {
        *freq = vfo == RIG_VFO_B ? c.tx_hz : c.rx_hz;
    }

    return ret;
}

static int rf5800_set_split_freq(RIG *rig, vfo_t vfo, freq_t tx_freq)
{
    char cmd[RF5800_CMD_MIN];
    struct rf5800_reply r;
    int ret = rf5800_cmd_txfreq(tx_freq, cmd, sizeof(cmd));

    if (ret != RIG_OK)
    {
        return ret;
    }

    return rf5800_write(rig, cmd, 1, RF5800_READ_MS, &r);
}

static int rf5800_get_split_freq(RIG *rig, vfo_t vfo, freq_t *tx_freq)
{
    struct rf5800_chan c;
    int ret = rf5800_read_chan(rig, &c);

    if (ret == RIG_OK)
    {
        *tx_freq = c.tx_hz;
    }

    return ret;
}

/* Split on sends nothing: the next set_split_freq makes it real.  Off sets
 * TX back to RX with one FREQ. */
static int rf5800_set_split_vfo(RIG *rig, vfo_t vfo, split_t split,
                                vfo_t tx_vfo)
{
    char cmd[RF5800_CMD_MIN];
    struct rf5800_reply r;
    struct rf5800_chan c;
    int ret;

    if (split != RIG_SPLIT_OFF)
    {
        return RIG_OK;
    }

    ret = rf5800_read_chan(rig, &c);

    if (ret != RIG_OK)
    {
        return ret;
    }

    ret = rf5800_cmd_freq(c.rx_hz, cmd, sizeof(cmd));

    if (ret != RIG_OK)
    {
        return ret;
    }

    return rf5800_write(rig, cmd, 1, RF5800_READ_MS, &r);
}

static int rf5800_get_split_vfo(RIG *rig, vfo_t vfo, split_t *split,
                                vfo_t *tx_vfo)
{
    struct rf5800_chan c;
    int ret = rf5800_read_chan(rig, &c);

    if (ret == RIG_OK)
    {
        *split = c.tx_hz != c.rx_hz ? RIG_SPLIT_ON : RIG_SPLIT_OFF;
        *tx_vfo = RIG_VFO_B;
    }

    return ret;
}

/* gate -> MODE -> checks -> gate -> BANDWIDTH: the second PRESET read is
 * what proves MANUAL 0 again before the second write. */
static int rf5800_set_mode(RIG *rig, vfo_t vfo, rmode_t mode, pbwidth_t width)
{
    char cmd[RF5800_CMD_MIN];
    char bw[RF5800_CMD_MIN];
    struct rf5800_reply r;
    int with_bw = width != RIG_PASSBAND_NORMAL && width != RIG_PASSBAND_NOCHANGE;
    int hz;
    int ret;

    /* every argument is checked before anything is sent */
    ret = rf5800_cmd_mode(mode, cmd, sizeof(cmd));

    if (ret == RIG_OK && with_bw)
    {
        ret = rf5800_snap_width(mode, width, &hz);

        if (ret == RIG_OK)
        {
            ret = rf5800_cmd_bandwidth(hz, bw, sizeof(bw));
        }
    }

    if (ret != RIG_OK)
    {
        return ret;
    }

    ret = rf5800_write(rig, cmd, 1, RF5800_READ_MS, &r);

    if (ret != RIG_OK || !with_bw)
    {
        return ret;
    }

    return rf5800_write(rig, bw, 1, RF5800_READ_MS, &r);
}

static int rf5800_get_mode(RIG *rig, vfo_t vfo, rmode_t *mode,
                           pbwidth_t *width)
{
    struct rf5800_chan c;
    int ret = rf5800_read_chan(rig, &c);

    if (ret == RIG_OK)
    {
        *mode = c.mode;
        *width = c.width;
    }

    return ret;
}

static int rf5800_set_level(RIG *rig, vfo_t vfo, setting_t level, value_t val)
{
    char cmd[RF5800_CMD_MIN];
    struct rf5800_reply r;
    int scoped = 0;
    int ret;

    switch (level)
    {
    case RIG_LEVEL_RFPOWER:
        ret = rf5800_cmd_power(val.f, cmd, sizeof(cmd));
        break;

    case RIG_LEVEL_SQL:
        ret = rf5800_cmd_sq_level(val.f, cmd, sizeof(cmd));
        break;

    case RIG_LEVEL_RF:
        ret = rf5800_cmd_rfgain(val.f, cmd, sizeof(cmd));
        break;

    /* AGC is a channel setting (its write pushes CURRENT CHAN 0, captured) */
    case RIG_LEVEL_AGC:
        ret = rf5800_cmd_agc(val.i, cmd, sizeof(cmd));
        scoped = 1;
        break;

    default:
        rig_debug(RIG_DEBUG_ERR,
                  "%s: level %s is not offered by the RF-5800H - use RFPOWER, SQL, RF or AGC\n",
                  __func__, rig_strlevel(level));
        return -RIG_EINVAL;
    }

    if (ret != RIG_OK)
    {
        return ret;
    }

    return rf5800_write(rig, cmd, scoped, RF5800_READ_MS, &r);
}

static int rf5800_get_level(RIG *rig, vfo_t vfo, setting_t level,
                            value_t *val)
{
    /* the thirds for the three-step words */
    static const float thirds[] = { 0.33f, 0.66f, 1.0f };
    struct rf5800_reply r;
    struct rf5800_radio rd;
    struct rf5800_chan c;
    int ret;

    switch (level)
    {
    case RIG_LEVEL_RFPOWER:
        ret = rf5800_transaction(rig, "POWER", RF5800_READ_MS, &r);
        return ret != RIG_OK ? ret : rf5800_parse_power(&r, &val->f);

    case RIG_LEVEL_SQL:
    case RIG_LEVEL_RF:
        ret = rf5800_read_radio(rig, &rd);

        if (ret != RIG_OK)
        {
            return ret;
        }

        if (level == RIG_LEVEL_SQL)
        {
            if (rd.sq_level < 0)
            {
                return rf5800_bad_row(__func__, "RADIO SQ_LEVEL");
            }

            val->f = thirds[rd.sq_level];
        }
        else
        {
            if (rd.rfgain < 0)
            {
                return rf5800_bad_row(__func__, "RADIO RFGAIN");
            }

            val->f = rd.rfgain / 100.0f;
        }

        return RIG_OK;

    case RIG_LEVEL_AGC:
        ret = rf5800_read_chan(rig, &c);

        if (ret != RIG_OK)
        {
            return ret;
        }

        if (c.agc < 0)
        {
            return rf5800_bad_row(__func__, "CHAN CURRENT AGC");
        }

        val->i = c.agc;
        return RIG_OK;

    default:
        rig_debug(RIG_DEBUG_ERR,
                  "%s: level %s is not offered by the RF-5800H - use RFPOWER, SQL, RF or AGC\n",
                  __func__, rig_strlevel(level));
        return -RIG_EINVAL;
    }
}

static int rf5800_set_func(RIG *rig, vfo_t vfo, setting_t func, int status)
{
    char cmd[RF5800_CMD_MIN];
    struct rf5800_reply r;
    int ret;

    if (func != RIG_FUNC_SQL)
    {
        rig_debug(RIG_DEBUG_ERR,
                  "%s: function %s is not offered by the RF-5800H - only SQL is\n",
                  __func__, rig_strfunc(func));
        return -RIG_EINVAL;
    }

    ret = rf5800_cmd_squelch(status, cmd, sizeof(cmd));

    if (ret != RIG_OK)
    {
        return ret;
    }

    /* a device setting, not channel-scoped */
    return rf5800_write(rig, cmd, 0, RF5800_READ_MS, &r);
}

static int rf5800_get_func(RIG *rig, vfo_t vfo, setting_t func, int *status)
{
    struct rf5800_radio rd;
    int ret;

    if (func != RIG_FUNC_SQL)
    {
        rig_debug(RIG_DEBUG_ERR,
                  "%s: function %s is not offered by the RF-5800H - only SQL is\n",
                  __func__, rig_strfunc(func));
        return -RIG_EINVAL;
    }

    ret = rf5800_read_radio(rig, &rd);

    if (ret != RIG_OK)
    {
        return ret;
    }

    if (rd.squelch < 0)
    {
        return rf5800_bad_row(__func__, "RADIO SQUELCH");
    }

    *status = rd.squelch;
    return RIG_OK;
}

/* RETUNE keys the transmitter while the antenna coupler tunes.  The prompt
 * arrives only when the tune ends.  Outcome precedence: ERROR, then FAILED,
 * then COMPLETE, else no result. */
static int rf5800_vfo_op(RIG *rig, vfo_t vfo, vfo_op_t op)
{
    struct rf5800_reply r;
    int ret;

    if (op != RIG_OP_TUNE)
    {
        rig_debug(RIG_DEBUG_ERR,
                  "%s: operation %s is not offered by the RF-5800H - only TUNE is\n",
                  __func__, rig_strvfop(op));
        return -RIG_EINVAL;
    }

    ret = rf5800_write(rig, "RETUNE", 0, RF5800_RETUNE_MS, &r);

    if (ret != RIG_OK)
    {
        return ret;
    }

    if (r.retune == 2)
    {
        rig_debug(RIG_DEBUG_ERR,
                  "%s: the tune FAILED (exception %d) - check that the channel can transmit and the antenna and coupler, then retry\n",
                  __func__, r.exception);
        return -RIG_ERJCTED;
    }

    if (r.retune == 1)
    {
        return RIG_OK;
    }

    rig_debug(RIG_DEBUG_ERR,
              "%s: RETUNE ended without a result - check the coupler on the KDU and retry\n",
              __func__);
    return -RIG_EPROTO;
}

/* the IDENTITY answer cached at open; never re-read */
static const char *rf5800_get_info(RIG *rig)
{
    const struct rf5800_priv *priv = STATE(rig)->priv;

    return priv->identity;
}

/* Exists so the frontend's split path finds a set_vfo; the radio has no
 * VFOs, so nothing is sent. */
static int rf5800_set_vfo(RIG *rig, vfo_t vfo)
{
    switch (vfo)
    {
    case RIG_VFO_A:
    case RIG_VFO_B:

    /* RIG_VFO_RX is RIG_VFO_CURR (include/hamlib/rig.h) */
    case RIG_VFO_CURR:
    case RIG_VFO_MAIN:
    case RIG_VFO_TX:
        return RIG_OK;

    default:
        rig_debug(RIG_DEBUG_ERR,
                  "%s: VFO %s does not exist on the RF-5800H - use A (RX) or B (TX)\n",
                  __func__, rig_strvfo(vfo));
        return -RIG_EINVAL;
    }
}

static int rf5800_get_vfo(RIG *rig, vfo_t *vfo)
{
    *vfo = RIG_VFO_A;
    return RIG_OK;
}

struct rig_caps rf5800_caps =
{
    RIG_MODEL(RIG_MODEL_RF5800H),
    .model_name =       "PRC-150/RF-5800H",
    .mfg_name =         "Harris",
    .version =          "20261114.1",
    .copyright =        "LGPL",
    .status =           RIG_STATUS_BETA,
    .rig_type =         RIG_TYPE_TRANSCEIVER,
    .targetable_vfo =   RIG_TARGETABLE_FREQ,
    .ptt_type =         RIG_PTT_NONE,
    .dcd_type =         RIG_DCD_NONE,
    .port_type =        RIG_PORT_SERIAL,
    /* rig_init opens at serial_rate_max; 9600 by serial_speed */
    .serial_rate_min =  9600,
    .serial_rate_max =  19200,
    .serial_data_bits = 8,
    .serial_stop_bits = 1,
    .serial_parity =    RIG_PARITY_NONE,
    .serial_handshake = RIG_HANDSHAKE_NONE,
    .write_delay =      0,
    .post_write_delay = 0,
    .timeout =          500,
    .retry =            0,
    /* disables read_string's retry and rig_init's halving of the timeout */
    .timeout_retry =    -1,

    .has_get_func =     RIG_FUNC_SQL,
    .has_set_func =     RIG_FUNC_SQL,
    .has_get_level =    RF5800_LEVELS,
    .has_set_level =    RF5800_LEVELS,
    .has_get_parm =     RIG_PARM_NONE,
    .has_set_parm =     RIG_PARM_NONE,
    .vfo_ops =          RIG_OP_TUNE,

    .rx_range_list1 = {
        {
            .startf = Hz(RF5800_MIN_HZ), .endf = Hz(RF5800_MAX_HZ), .modes = RF5800_MODES,
            .low_power = -1, .high_power = -1, .vfo = RF5800_VFOS
        },
        RIG_FRNG_END,
    },
    .tx_range_list1 = {
        {
            .startf = Hz(RF5800_MIN_HZ), .endf = Hz(RF5800_MAX_HZ), .modes = RF5800_MODES,
            .low_power = W(1), .high_power = W(20), .vfo = RF5800_VFOS
        },
        RIG_FRNG_END,
    },
    .tuning_steps = { {RF5800_MODES, 1}, RIG_TS_END, },

    /* the first entry per mode is Hamlib's "normal" */
    .filters = {
        {RIG_MODE_SSB, 3000},
        {RIG_MODE_SSB, 2000},
        {RIG_MODE_SSB, 2400},
        {RIG_MODE_SSB, 2700},
        {RIG_MODE_CW,  1000},
        {RIG_MODE_CW,  350},
        {RIG_MODE_CW,  500},
        {RIG_MODE_CW,  1500},
        {RIG_MODE_AM,  4000},
        {RIG_MODE_FM,  25000},
        RIG_FLT_END,
    },

    .rig_init =         rf5800_init,
    .rig_cleanup =      rf5800_cleanup,
    .rig_open =         rf5800_open,
    .rig_close =        rf5800_close,
    .set_freq =         rf5800_set_freq,
    .get_freq =         rf5800_get_freq,
    .set_split_freq =   rf5800_set_split_freq,
    .get_split_freq =   rf5800_get_split_freq,
    .set_split_vfo =    rf5800_set_split_vfo,
    .get_split_vfo =    rf5800_get_split_vfo,
    .set_mode =         rf5800_set_mode,
    .get_mode =         rf5800_get_mode,
    .set_level =        rf5800_set_level,
    .get_level =        rf5800_get_level,
    .set_func =         rf5800_set_func,
    .get_func =         rf5800_get_func,
    .vfo_op =           rf5800_vfo_op,
    .get_info =         rf5800_get_info,
    .set_vfo =          rf5800_set_vfo,
    .get_vfo =          rf5800_get_vfo,
    .hamlib_check_rig_caps = HAMLIB_CHECK_RIG_CAPS
};
