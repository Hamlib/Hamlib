/*
 *  Hamlib simulator - Harris RF-350K (RT-1446/URC) J9 remote port
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
 * Usage: simrf350k <tcp-port> [scenario]
 *        simrf350k --selftest
 *
 * Listens at 127.0.0.1:<tcp-port>, one connection at a time (0 picks a free
 * port; the port is printed as "listening on 127.0.0.1:<n>").  Then
 *   rigctl -m 42003 -r 127.0.0.1:<n>
 * The radio's state lasts across connections.
 *
 * Scenarios: remote (default), local, lapsed, cw, ext-key, panel, tune,
 * reenter, late-gain, no-gain, deaf.
 *
 * It replays answers captured from one radio (REV 604M firmware, the ADAPT
 * status format); any other input is ignored and named on its console as
 * not simulated.
 *
 * Frequencies are in the radio's units of 10 Hz.  Each answer below is the
 * radio's own: "captured" says what a capture of the radio showed.  Where a
 * scenario's script decides WHEN an operator acts or WHICH write a fault
 * hits, the comment says "script".
 *
 * --selftest drives the line assembly, the command and the tick functions
 * against a supplied clock, with no socket, and exits non-zero on any
 * failed condition.
 */

/* first: on Windows winsock2.h must come before anything that pulls in windows.h */
#include "rf5800_sock.h"

#ifdef _WIN32
#include <ws2tcpip.h>
#define SIM_BAD_SOCK INVALID_SOCKET
#else
#include <arpa/inet.h>
#include <netinet/in.h>
#include <sys/select.h>
#include <sys/socket.h>
#define SIM_BAD_SOCK (-1)
#endif

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

/* milliseconds of a monotonic clock */
typedef long long sim_ms;

#define LINE_MAX_LEN 64
#define REPLY_MAX    512
/* captured: X002 pushed 10.40 to 10.55 s after the last line that feeds the
 * keep-alive, in nine captures; this is one of them, after a U */
#define KEEPALIVE_MS 10490
/* captured: a first key where a coupler tune is owed ends its tune 6 453 ms
 * after the K (at 3 996.00 kHz; seven captures, 4.7 to 6.8 s) */
#define TUNE_MS      6453
/* captured: the front-panel format's J01 lines, about ten a second */
#define PANEL_J01_MS 100
/* script, ext-key: the handset is pressed this long after the connection */
#define EXT_KEY_MS       3000
/* script, reenter: REMOTE is left this long after the connection, and
 * entered again REENTER_BACK_MS later */
#define REENTER_LEAVE_MS 8000
#define REENTER_BACK_MS  6150
/* captured: the frequencies the radio echoed and took at its two edges */
#define F_MIN 159999L
#define F_MAX 3000000L
/* captured: an unprogrammed channel holds 10 000.00 kHz USB; the
 * simulator's fifty channels all hold that */
#define CHAN_FREQ 1000000L
#define CHAN_MODE 1
/* the start frequency, 3 996.00 kHz, that of the captures */
#define HOME_FREQ 399600L
/* the late-gain script: the fourth dump after a G is the first to show it */
#define LATE_GAIN_DUMP 4

static struct sim_state
{
    const char *scenario;
    long freq;
    int mode;           /* 1 USB, 2 LSB, 3 AME, 4 CW */
    int agc;            /* 1 OFF, 2 SLOW, 3 MED, 4 FAST */
    int gain;           /* 00 max .. 63 min */
    int channel;        /* -1: none selected */
    int audio;          /* ( : 1 on, 0 off */
    int remote;
    int route;          /* " */
    int pttena;         /* ' : the PTT enable */
    int keyed;          /* by a K */
    int handset;        /* keyed by the handset */
    int warned;         /* !1 was sent */
    int lapsed;         /* the keep-alive lapsed and was not yet cleared */
    sim_ms fed;         /* the keep-alive's last feed */
    int unfed;          /* a line came since, whose feeding was never captured */
    int lapse_held;     /* that lapse was named not simulated; until a feed */
    int owed;           /* & : a coupler tune is owed */
    int tuning;         /* * : the tune runs */
    int tune_held;      /* its end was named not simulated */
    sim_ms tune_end;
    int tune_k, tune_u; /* the K and U lines received during it */
    long tuned;         /* the frequency the last tune completed on */
    char waiting[8];    /* M and A lines waiting for their F, in order */
    int wait_mode, wait_agc;
    sim_ms connected;
    sim_ms last_j01;    /* panel */
    int dumps;          /* local: the dumps sent */
    int pressed;        /* ext-key: the press happened */
    int reentered;      /* reenter: 0 in REMOTE, 1 left, 2 back, 3 abandoned */
    int queries;        /* deaf: the ? lines of this connection */
    int deaf_next;      /* deaf: the next line is not heard either */
    int gain_seen;      /* no-gain: the first G came */
    int gain_wait;      /* late-gain: gain_next is stored at a later dump */
    int gain_next;
    int gain_dumps;
} st;

/* A tune owed after a move to f, from the captured points only: 1 owed, 0
 * none, -1 nothing captured there.  Offsets are captured around two tuned
 * frequencies, up to which no tune was owed and from which one was; from
 * any other tuned frequency only moves of at least 0.8597 of it were
 * captured (each owed one). */
static const struct
{
    long tuned, up_none, up_owed, down_none, down_owed;
} owed_points[] =
{
    { 399600, 3956, 4036, 3596, 3956 },
    { 185000, 1869, 2035, 1665, 1832 },
};

static sock_t sock;
/* --selftest: the clock is this value, pushes land in pushed[] and each
 * call of unsim is counted */
static int selftest;
static sim_ms test_ms;
static char pushed[REPLY_MAX];
static int unsim_calls;

/* the line assembly: one received byte at a time */
struct framer
{
    char line[LINE_MAX_LEN]; /* the line; when bad, a bounded printable form */
    size_t n;
    int bad;                 /* it holds a byte no capture has in a line, or more */
    int done;                /* the line ended: the next byte starts another */
};

/* nothing is written while this is set: no client is connected */
static int no_client;

static int is(const char *s)
{
    return strcmp(st.scenario, s) == 0;
}

/* every timer reads this one monotonic 64-bit clock: on Windows a long is
 * 32 bits, and tv_sec * 1000 in it wraps negative */
static sim_ms now_ms(void)
{
    if (selftest)
    {
        return test_ms;
    }

#ifdef _WIN32
    return (sim_ms) GetTickCount64();
#else
    {
        struct timespec t;

        clock_gettime(CLOCK_MONOTONIC, &t);
        return (sim_ms) t.tv_sec * 1000 + t.tv_nsec / 1000000;
    }
#endif
}

static void add(char *out, const char *fmt, long v)
{
    size_t n = strlen(out);

    snprintf(out + n, REPLY_MAX - n, fmt, v);
}

static const char *state_name(void)
{
    static char s[128];

    snprintf(s, sizeof(s), "%s%s%s%s%s%s",
             is("panel") ? "front-panel format, " : "",
             st.remote ? "REMOTE" : "local",
             st.tuning ? ", coupler tuning" : st.keyed ? ", keyed" : "",
             st.handset ? ", keyed by the handset" : "",
             st.lapsed ? ", keep-alive lapsed" : "",
             st.owed ? ", tune owed" : "");
    return s;
}

/* The answer to an input no capture holds: nothing is sent, nothing
 * changes, and the console says so. */
static void unsim(const char *what, const char *why)
{
    if (selftest)
    {
        unsim_calls++;
        return;
    }

    printf("not simulated: %s (%s): %s\n", what, state_name(), why);
    fflush(stdout);
}

static void init_state(const char *scenario)
{
    memset(&st, 0, sizeof(st));
    st.scenario = scenario;
    st.freq = HOME_FREQ;
    st.mode = is("cw") ? 4 : 2;
    st.agc = 2;
    st.channel = -1;
    st.audio = 1;
    st.remote = 1;
    st.tuned = HOME_FREQ;
    st.lapsed = is("lapsed");
    st.fed = now_ms();

    /* captured: a dump at 7 200.00 kHz owing a tune, the last one
     * completed at 1 850.00 kHz */
    if (is("tune"))
    {
        st.freq = 720000;
        st.tuned = 185000;
        st.owed = 1;
    }
}

static void link_write(const char *s)
{
    if (no_client)
    {
        return;
    }

    if (selftest)
    {
        strncat(pushed, s, sizeof(pushed) - strlen(pushed) - 1);
        return;
    }

    sock_write(sock, s, strlen(s));
}

static void push(const char *s)
{
    if (!selftest && !no_client)
    {
        printf("Push:%s", s);
        fflush(stdout);
    }

    link_write(s);
}

static void fed(void)
{
    st.fed = now_ms();
    st.unfed = 0;
    st.lapse_held = 0;
}

/* captured: a feeder during a lapse clears it with X000 */
static void cleared(char *out)
{
    if (st.lapsed)
    {
        add(out, "X000\n", 0);
        st.lapsed = 0;
    }
}

static int owes(long f)
{
    const long d = f - st.tuned;
    const long a = d < 0 ? -d : d;
    size_t i;

    if (d == 0)
    {
        return 0;
    }

    for (i = 0; i < sizeof(owed_points) / sizeof(owed_points[0]); i++)
    {
        if (st.tuned == owed_points[i].tuned)
        {
            if (d > 0)
            {
                return a <= owed_points[i].up_none ? 0
                       : a >= owed_points[i].up_owed ? 1 : -1;
            }

            return a <= owed_points[i].down_none ? 0
                   : a >= owed_points[i].down_owed ? 1 : -1;
        }
    }

    return (long long) a * 10000 >= (long long) st.tuned * 8597 ? 1 : -1;
}

/* captured: leaving REMOTE at the panel (1; 0 when not simulated), and
 * entering it */
static int leave_remote(char *out)
{
    if (st.lapsed || st.keyed || st.handset || st.tuning)
    {
        unsim("leaving REMOTE", "never captured in this state");
        return 0;
    }

    st.remote = 0;
    st.route = 1;
    st.pttena = 1;
    add(out, "R0\n\"1\n'1\n,0\n)0\n", 0);
    return 1;
}

static void enter_remote(char *out)
{
    st.remote = 1;
    st.route = 0;
    st.pttena = 0;
    fed();
    add(out, "R1\n\"0\n'0\n", 0);
}

/* captured: the 22-line dump in REMOTE, 21 (no G) in local; a dump read
 * while lapsed says X002 and is followed by X000 */
static void dump(char *out)
{
    if (st.gain_wait && ++st.gain_dumps == LATE_GAIN_DUMP)
    {
        st.gain = st.gain_next;
        st.gain_wait = 0;
    }

    add(out, "#1\n$0\n&%ld\n", st.owed);
    add(out, "*%ld\n", st.tuning);
    add(out, "\"%ld\n", st.route);
    add(out, "'%ld\n", st.pttena);
    add(out, "(%ld\n,0\n", st.audio);
    add(out, "A%ld\n", st.agc);
    add(out, "M%ld\n", st.mode);
    add(out, "R%ld\nZ0\nE0\nO1\nT1\n", st.remote);
    add(out, st.keyed || st.handset ? "K\n" : "U\n", 0);
    add(out, "F%07ld\n", st.freq);
    add(out, "X00%ld\n^0\n", st.lapsed ? 2 : 0);

    if (st.remote)
    {
        add(out, "G%02ld\n", st.gain);
    }

    add(out, ")0\n.\n", 0);
    cleared(out);
    fed();
}

/* captured: the front-panel format's dump, byte for byte */
static const char panel_dump[] =
    "!0\n#1\n$0\n&1\n*0\n\"0\n'0\n(1\n,0\nA2\nD4\nH1\nI0\nM2\nR1\nS0\nV0\n"
    "Y0\nZ0\nE0\nO1\nT1\nU\nC;;\nF0389500\nB100\nX000\nQT0\nQC\x05\n)0\n.\n";

/* a line of exactly letter + n digits; its value, or -1 */
static long value(const char *line, char letter, size_t n)
{
    size_t i;

    if (line[0] != letter || strlen(line) != n + 1)
    {
        return -1;
    }

    for (i = 1; i <= n; i++)
    {
        if (line[i] < '0' || line[i] > '9')
        {
            return -1;
        }
    }

    return atol(line + 1);
}

/* the waiting M and A lines, confirmed when their F comes */
static void wait_line(const char *line)
{
    const long v = value(line, line[0], 1);

    if (v < 1 || v > 4)
    {
        unsim(line, "no such value was captured");
        return;
    }

    if (line[0] == 'M' ? st.wait_mode != 0 : st.wait_agc != 0)
    {
        unsim(line, "a second one waiting for its F was never captured");
        return;
    }

    *(line[0] == 'M' ? &st.wait_mode : &st.wait_agc) = (int) v;
    strcat(st.waiting, line);
    strcat(st.waiting, "\n");
    /* captured: nothing comes until the F */
    st.unfed = 1;
}

static void waiting_done(void)
{
    st.waiting[0] = '\0';
    st.wait_mode = st.wait_agc = 0;
}

/* captured, in REMOTE unkeyed: the waiting lines, the F, then X000 if it
 * cleared a lapse, the CW lines, C;; when it leaves a channel and the tune
 * owed when that changed.  Into CW with the PTT enable on a U follows
 * S1 H4 (captured with no channel and no change of the tune owed) */
static void f_remote(long f, const char *line, char *out)
{
    const int o = owes(f);
    const int mode = st.wait_mode ? st.wait_mode : st.mode;
    const int into = mode == 4 && st.mode != 4;
    const int outof = mode != 4 && st.mode == 4;

    if (o < 0)
    {
        unsim(line, "no capture holds whether this move owes a coupler tune");
        return;
    }

    if (into && st.pttena && (st.channel >= 0 || o != st.owed))
    {
        unsim(line,
              "the order of the U after S1 H4 and the C;; or & lines was never captured");
        return;
    }

    strcat(out, st.waiting);
    add(out, "F%07ld\n", f);
    cleared(out);

    if (into)
    {
        add(out, st.pttena ? "S1\nH4\nU\n" : "S1\nH4\n", 0);
    }
    else if (outof)
    {
        add(out, "S0\nH1\n", 0);
    }

    if (st.channel >= 0)
    {
        add(out, "C;;\n", 0);
        st.channel = -1;
    }

    if (o != st.owed)
    {
        add(out, "&%ld\n", o);
        st.owed = o;
    }

    st.freq = f;
    st.mode = mode;

    if (st.wait_agc)
    {
        st.agc = st.wait_agc;
    }

    waiting_done();
    fed();
}

/* captured, in local: an F is echoed twice; A+F and M+F are confirmed,
 * then pushed again, M+F with the CW lines, the AGC reset to slow and, out
 * of CW, V0 I0, and into CW a U after it */
static void f_local(long f, const char *line, char *out)
{
    const int mode = st.wait_mode;
    const int into = mode == 4 && st.mode != 4;
    const int outof = mode != 0 && mode != 4 && st.mode == 4;

    if (owes(f) != st.owed || st.channel >= 0)
    {
        unsim(line, "in local only an F that keeps the tune owed was captured");
        return;
    }

    if (mode && (st.wait_agc || mode == 3 || st.mode == 3
                 || (mode == 4 && st.mode == 4)))
    {
        unsim(line, "only M with F between USB, LSB and CW was captured in local");
        return;
    }

    if (st.wait_agc)
    {
        add(out, "A%ld\n", st.wait_agc);
        add(out, "F%07ld\n", f);
        add(out, "A%ld\n", st.wait_agc);
        st.agc = st.wait_agc;
    }
    else if (mode)
    {
        add(out, "M%ld\n", mode);
        add(out, "F%07ld\n", f);
        add(out, "M%ld\n", mode);
        add(out, into ? "S1\n" : outof ? "S0\n" : "", 0);
        add(out, "A2\n", 0);
        add(out, into ? "H4\n" : outof ? "H1\n" : "", 0);
        add(out, mode != 4 ? "V0\nI0\n" : "", 0);
        st.mode = mode;
        st.agc = 2;
    }
    else
    {
        add(out, "F%07ld\n", f);
    }

    add(out, "F%07ld\n", f);
    add(out, into ? "U\n" : "", 0);
    st.freq = f;
    waiting_done();
    fed();
}

static void f_line(const char *line, char *out)
{
    const long f = value(line, 'F', 7);

    if (f < F_MIN || f > F_MAX)
    {
        unsim(line, "no capture holds an F of this form or value");
    }
    else if (!st.remote)
    {
        f_local(f, line, out);
    }
    else if (st.keyed)
    {
        /* captured: keyed, an F is echoed and the frequency is kept */
        add(out, "F%07ld\n", f);
        fed();
    }
    else
    {
        f_remote(f, line, out);
    }
}

/* captured: a channel select loads the channel's frequency and mode, leaves
 * the AGC, and pushes & when the tune owed changes; a select that keeps a
 * tune owed pushes nothing more.  C50 to C99 get nothing.  Whether a C
 * feeds the keep-alive was never captured. */
static void c_line(long n, const char *line, char *out)
{
    const int o = owes(CHAN_FREQ);

    if (n >= 50)
    {
        st.unfed = 1;
        return;
    }

    if (o < 0 || (st.owed && !o) || st.mode == 4)
    {
        unsim(line, "no capture holds this select from this state");
        return;
    }

    add(out, "C%02ld\n", n);

    if (o != st.owed)
    {
        add(out, "&%ld\n", o);
        st.owed = o;
    }

    st.channel = (int) n;
    st.freq = CHAN_FREQ;
    st.mode = CHAN_MODE;
    st.unfed = 1;
}

/* captured: G is never answered and the same write's dump shows it; the
 * scripts of no-gain (the first G is not taken) and late-gain (every G is
 * late) */
static void g_line(long g, const char *line)
{
    if (is("no-gain") && !st.gain_seen)
    {
        st.gain_seen = 1;
        return;
    }

    if (is("late-gain"))
    {
        if (st.gain_wait)
        {
            unsim(line, "a second G before the first showed was never captured");
            return;
        }

        st.gain_wait = 1;
        st.gain_next = (int) g;
        st.gain_dumps = 0;
        return;
    }

    st.gain = (int) g;
}

/* captured: the start of a first-key tune */
static void k_line(const char *line, char *out)
{
    if (st.owed)
    {
        if (st.lapsed)
        {
            unsim(line, "a K owing a tune during a lapse was never captured");
            return;
        }

        st.owed = 0;
        st.tuning = 1;
        st.tune_end = now_ms() + TUNE_MS;
        st.tune_k = st.tune_u = 0;
        st.keyed = 1;
        add(out, "&0\nK\n*1\nK\n", 0);
        fed();
        return;
    }

    cleared(out);
    st.keyed = 1;
    add(out, "K\n", 0);
    fed();
}

/* In REMOTE, unkeyed or keyed by a K (no tune running). */
static void remote_line(const char *line, char *out)
{
    const char c = line[0];
    const long v1 = value(line, c, 1);
    const long v2 = value(line, c, 2);

    if (strcmp(line, "?") == 0)
    {
        dump(out);

        /* script, local: the operator leaves REMOTE after the second dump */
        if (is("local") && ++st.dumps == 2)
        {
            leave_remote(out);
        }
    }
    else if (strcmp(line, "U") == 0)
    {
        /* captured: U answers U and unkeys */
        cleared(out);
        st.keyed = 0;
        add(out, "U\n", 0);
        fed();
    }
    else if (strcmp(line, "K") == 0)
    {
        k_line(line, out);
    }
    else if (c == 'F')
    {
        f_line(line, out);
    }
    else if (st.lapsed && c != '(')
    {
        unsim(line, "what this line does during a lapse was never captured");
    }
    else if (c == '(' && (v1 == 0 || v1 == 1) && st.keyed)
    {
        /* captured: keyed, the mute's value held gets nothing */
        if (v1 != st.audio)
        {
            unsim(line, "a change of the mute while keyed was never captured");
            return;
        }

        fed();
    }
    else if (c == '(' && (v1 == 0 || v1 == 1))
    {
        /* captured: echoed, also after a lapse, which it clears */
        cleared(out);
        st.audio = (int) v1;
        add(out, "(%ld\n", v1);
        fed();
    }
    else if (st.keyed && c != 'G')
    {
        unsim(line, "this line while keyed was never captured");
    }
    else if (c == 'G' && v2 >= 0 && v2 <= 63)
    {
        /* captured: keyed, a G gets nothing and is not stored */
        if (!st.keyed)
        {
            g_line(v2, line);
        }
    }
    else if ((c == 'M' || c == 'A') && v1 >= 0)
    {
        wait_line(line);
    }
    else if (c == 'C' && v2 >= 0)
    {
        c_line(v2, line, out);
    }
    else if ((c == '\'' || c == '"') && (v1 == 0 || v1 == 1))
    {
        /* captured: echoed as sent, also when nothing changes */
        *(c == '"' ? &st.route : &st.pttena) = (int) v1;
        strcat(out, line);
        strcat(out, "\n");
    }
    else
    {
        unsim(line, "no capture holds an answer to this line");
    }
}

/* Out of REMOTE. */
static void local_line(const char *line, char *out)
{
    const char c = line[0];
    const long v1 = value(line, c, 1);
    const long v2 = value(line, c, 2);

    if (strcmp(line, "?") == 0)
    {
        dump(out);
    }
    else if (c == 'F')
    {
        f_line(line, out);
    }
    else if ((c == 'M' || c == 'A') && v1 >= 0)
    {
        wait_line(line);
    }
    else if (strcmp(line, "U") == 0
             || ((c == '\'' || c == '"' || c == '(') && (v1 == 0 || v1 == 1))
             || (c == 'C' && v2 >= 0)
             || (c == 'G' && v2 >= 0 && v2 <= 63))
    {
        /* captured: in local these get nothing and change nothing */
    }
    else
    {
        unsim(line, "no capture holds an answer to this line in local");
    }
}

/* captured, during a first-key tune: a dump is answered, a K or U gets
 * nothing until the tune ends */
static void tune_line(const char *line, char *out)
{
    if (strcmp(line, "?") == 0)
    {
        dump(out);
    }
    else if (strcmp(line, "K") == 0 && st.tune_u == 0)
    {
        st.tune_k++;
        fed();
    }
    else if (strcmp(line, "U") == 0 && st.tune_k == 0 && st.tune_u < 3)
    {
        st.tune_u++;
        fed();
    }
    else
    {
        unsim(line,
              "during a coupler tune only a dump, K lines or one or three U were captured");
    }
}

/* captured, under the handset's key: a dump, and U answered K; the first U
 * after a lapse answers !1 X000 K */
static void handset_line(const char *line, char *out)
{
    if (strcmp(line, "?") == 0 && !st.lapsed)
    {
        dump(out);
    }
    else if (strcmp(line, "U") == 0)
    {
        if (st.lapsed)
        {
            add(out, "!1\n", 0);
            st.warned = 1;
        }

        cleared(out);
        add(out, "K\n", 0);
        fed();
    }
    else
    {
        unsim(line, "no capture holds this line under the handset's key");
    }
}

/* The answer to one received line. */
static void answer(const char *line, char *out)
{
    out[0] = '\0';

    /* script, deaf: the third ? of a connection and the line after it are
     * not heard (captured: two losses in a row on the radio) */
    if (is("deaf"))
    {
        if (st.deaf_next)
        {
            st.deaf_next = 0;
            return;
        }

        if (strcmp(line, "?") == 0 && ++st.queries == 3)
        {
            st.deaf_next = 1;
            return;
        }
    }

    if (is("panel"))
    {
        if (strcmp(line, "?") == 0 && !st.lapsed)
        {
            strcat(out, panel_dump);
            fed();
        }
        else
        {
            unsim(line, "in the front-panel format only a dump is simulated");
        }
    }
    else if (st.waiting[0] != '\0' && line[0] != 'M' && line[0] != 'A'
             && line[0] != 'F')
    {
        unsim(line, "a line between an M or A and its F was never captured");
    }
    else if (st.tuning)
    {
        tune_line(line, out);
    }
    else if (st.handset)
    {
        handset_line(line, out);
    }
    else if (st.remote)
    {
        remote_line(line, out);
    }
    else
    {
        local_line(line, out);
    }
}

/* captured: the keep-alive's lapse; keyed by a K the radio unkeys, keyed by
 * the handset it stays keyed */
static void lapse(void)
{
    if (st.unfed || (st.handset && st.warned))
    {
        unsim("the keep-alive running out",
              "a lapse after this line, or a second under the handset, was never captured");
        st.lapse_held = 1;
        return;
    }

    /* a tune whose end was not simulated is still unresolved */
    if (st.tune_held)
    {
        unsim("the keep-alive running out",
              "a lapse during a tune whose end was not simulated was never captured");
        st.lapse_held = 1;
        return;
    }

    st.lapsed = 1;

    if (st.keyed)
    {
        st.keyed = 0;
        push("X002\nU\n");
    }
    else
    {
        push(st.handset ? "X002\nK\n" : "X002\n");
    }
}

/* captured: the end of a first-key tune.  The line before *0 is the key
 * state the radio ends in */
static void tune_ends(void)
{
    if (st.tune_u == 2)
    {
        unsim("the coupler tune's end", "an end after two U was never captured");
        st.tune_held = 1;
        return;
    }

    st.tuning = 0;
    st.tuned = st.freq;

    if (st.tune_u)
    {
        st.keyed = 0;
        push("U\n*0\n");
    }
    else
    {
        push("K\n*0\n");
    }
}

/* script, ext-key: the handset pressed; captured: with the PTT enable on
 * the radio pushes K and keys, with it off it pushes U */
static void press(void)
{
    st.pressed = 1;

    if (!st.remote || st.keyed || st.tuning || st.lapsed)
    {
        unsim("the handset pressed", "never captured in this state");
        return;
    }

    if (st.pttena)
    {
        st.handset = 1;
        push("K\n");
    }
    else
    {
        push("U\n");
    }
}

/* the radio's own timers: a tune's end, the keep-alive's lapse */
static void tune_due(sim_ms now)
{
    if (st.tuning && !st.tune_held && now >= st.tune_end)
    {
        tune_ends();
    }
}

static void lapse_due(sim_ms now)
{
    if (st.remote && !st.lapsed && !st.lapse_held
            && now - st.fed >= KEEPALIVE_MS)
    {
        lapse();
    }
}

/* The time-driven events, all on the one clock. */
static void tick(void)
{
    const sim_ms now = now_ms();
    char out[REPLY_MAX];

    if (is("ext-key") && !st.pressed && now - st.connected >= EXT_KEY_MS)
    {
        press();
    }

    if (is("panel") && now - st.last_j01 >= PANEL_J01_MS)
    {
        st.last_j01 = now;
        link_write("J01\n");
    }

    tune_due(now);
    out[0] = '\0';

    if (is("reenter") && st.reentered == 0
            && now - st.connected >= REENTER_LEAVE_MS)
    {
        st.reentered = leave_remote(out) ? 1 : 3;
    }
    else if (is("reenter") && st.reentered == 1
             && now - st.connected >= REENTER_LEAVE_MS + REENTER_BACK_MS)
    {
        st.reentered = 2;
        enter_remote(out);
    }

    if (out[0] != '\0')
    {
        push(out);
    }

    lapse_due(now);
}

/* One received byte.  1 when it ends a line, now in f->line: a LF ends a
 * line, and a bare U or K with no line open is a line of its own (captured:
 * the radio answers those without a LF).  A byte that is not printable
 * ASCII, or one more than the line can hold, marks the whole line bad: it
 * is never cut or cleaned into another line; f->line then holds a bounded
 * printable form of it, for the console. */
static int assemble(struct framer *f, char c)
{
    const unsigned char b = (unsigned char) c;
    size_t room;

    if (f->done)
    {
        f->n = 0;
        f->bad = 0;
        f->done = 0;
    }

    room = sizeof(f->line) - 1 - f->n;

    if (c == '\n' || (f->n == 0 && (c == 'U' || c == 'K')))
    {
        if (c != '\n')
        {
            f->line[f->n++] = c;
        }

        f->line[f->n] = '\0';
        f->done = 1;
        return 1;
    }

    if (b < 0x20 || b > 0x7e)
    {
        f->bad = 1;

        if (room >= 4)
        {
            snprintf(f->line + f->n, room + 1, "<%02X>", b);
            f->n += 4;
        }
    }
    else if (room >= 1)
    {
        f->line[f->n++] = c;
    }
    else
    {
        f->bad = 1;
    }

    return 0;
}

/* waits for a byte up to 100 ms; 1 = byte, 0 = none yet, -1 = closed */
static int link_poll(char *c)
{
    fd_set rd;
    struct timeval tv = { 0, 100000 };

    FD_ZERO(&rd);
    FD_SET(sock, &rd);

    if (select((int) sock + 1, &rd, NULL, NULL, &tv) <= 0)
    {
        return 0;
    }

    return sock_read(sock, c, 1) == 0 ? 1 : -1;
}

/* What a new connection resets; also called by --selftest.  Time ran while
 * no client was connected, so what the clock owes since (a tune's end, a
 * keep-alive lapse with its unkey) is played first, writing nothing: there
 * was nobody to read it.  Then the simulator counts the connection as a feed
 * of the keep-alive (a serial line has no connection to see). */
static void connection_opened(void)
{
    no_client = 1;
    tune_due(now_ms());
    lapse_due(now_ms());
    no_client = 0;
    st.connected = now_ms();
    st.last_j01 = st.connected;
    st.queries = 0;
    st.deaf_next = 0;
    fed();
}

/* One assembled line: named on the console, then answered; a bad line is
 * not simulated as a whole. */
static void line_in(const struct framer *f, char *out)
{
    if (!selftest)
    {
        printf("Cmd:%s\n", f->line);
    }

    if (f->bad)
    {
        out[0] = '\0';
        unsim(f->line,
              "a byte that is not printable ASCII, or a line longer than the simulator holds");
        return;
    }

    answer(f->line, out);
}

static void serve(void)
{
    struct framer fr;
    char out[REPLY_MAX];
    char c;
    int got;

    memset(&fr, 0, sizeof(fr));
    connection_opened();

    for (;;)
    {
        tick();
        got = link_poll(&c);

        if (got < 0)
        {
            break;
        }

        if (got == 0 || !assemble(&fr, c))
        {
            continue;
        }

        line_in(&fr, out);
        link_write(out);
        fflush(stdout);
    }
}

static int is_number(const char *s)
{
    if (*s == '\0')
    {
        return 0;
    }

    for (; *s; s++)
    {
        if (*s < '0' || *s > '9')
        {
            return 0;
        }
    }

    return 1;
}

static int serve_tcp(int port)
{
    struct sockaddr_in a;
    socklen_t alen = sizeof(a);
    sock_t listener;

    if (sock_startup() != 0)
    {
        fprintf(stderr, "socket startup failed\n");
        return 1;
    }

    listener = socket(AF_INET, SOCK_STREAM, 0);
    memset(&a, 0, sizeof(a));
    a.sin_family = AF_INET;
    a.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
    a.sin_port = htons((unsigned short) port);

    if (listener == SIM_BAD_SOCK
            || bind(listener, (struct sockaddr *) &a, sizeof(a)) != 0
            || listen(listener, 1) != 0
            || getsockname(listener, (struct sockaddr *) &a, &alen) != 0)
    {
        fprintf(stderr, "cannot listen on 127.0.0.1:%d\n", port);
        return 1;
    }

    printf("listening on 127.0.0.1:%d\n", ntohs(a.sin_port));
    fflush(stdout);

    for (;;)
    {
        sock = accept(listener, NULL, NULL);

        if (sock == SIM_BAD_SOCK)
        {
            break;
        }

        serve();
        sock_close(sock);
    }

    sock_close(listener);
    sock_cleanup();
    return 0;
}

/* --selftest: every numbered case (3-1 to 3-26), called directly against the
 * supplied clock (test_ms); pushes land in pushed[] and unsim counts */
static int st_fail;
static int st_checks;
static char reply[REPLY_MAX];
static struct framer test_framer;

/* a reply with its line ends shown as '|' */
static const char *shown(const char *s)
{
    static char b[2 * REPLY_MAX];
    size_t i;

    for (i = 0; s[i] != '\0' && i < sizeof(b) - 1; i++)
    {
        b[i] = s[i] == '\n' ? '|' : s[i];
    }

    b[i] = '\0';
    return b;
}

static void check(const char *name, int ok, const char *got)
{
    st_checks++;

    if (!ok)
    {
        st_fail++;
        fprintf(stderr, "FAIL %s: got '%s'\n", name, shown(got));
    }
}

static void expect(const char *name, const char *got, const char *want)
{
    st_checks++;

    if (strcmp(got, want) != 0)
    {
        st_fail++;
        fprintf(stderr, "FAIL %s: got '%s', ", name, shown(got));
        fprintf(stderr, "want '%s'\n", shown(want));
    }
}

/* a whole line of a reply */
static int has_line(const char *buf, const char *line)
{
    const size_t n = strlen(line);
    const char *p = buf;

    while ((p = strstr(p, line)) != NULL)
    {
        if ((p == buf || p[-1] == '\n') && p[n] == '\n')
        {
            return 1;
        }

        p++;
    }

    return 0;
}

static void expect_line(const char *name, const char *buf, const char *line)
{
    check(name, has_line(buf, line), buf);
}

static void start(const char *scenario)
{
    test_ms = 0;
    init_state(scenario);
    connection_opened();
    memset(&test_framer, 0, sizeof(test_framer));
    unsim_calls = 0;
}

/* one line at ms; the timers do not run */
static const char *say(sim_ms ms, const char *line)
{
    test_ms = ms;
    answer(line, reply);
    return reply;
}

/* the timers at ms: what they pushed */
static const char *at(sim_ms ms)
{
    test_ms = ms;
    pushed[0] = '\0';
    tick();
    return pushed;
}

/* n bytes at ms through the line assembly (NUL included): every answer,
 * in order */
static const char *bytes_n(sim_ms ms, const char *s, size_t n)
{
    static char all[REPLY_MAX];
    size_t i;

    all[0] = '\0';
    reply[0] = '\0';
    test_ms = ms;

    for (i = 0; i < n; i++)
    {
        if (assemble(&test_framer, s[i]))
        {
            line_in(&test_framer, reply);
            strncat(all, reply, sizeof(all) - strlen(all) - 1);
        }
    }

    return all;
}

static const char *bytes(sim_ms ms, const char *s)
{
    return bytes_n(ms, s, strlen(s));
}

/* fields a not-simulated event may change: the clock's bookkeeping of a
 * case that ends there, named per check */
enum
{
    KEEP_ALL = 0,
    BUT_FED = 1,        /* the time of the last feed */
    BUT_DUMPS = 2,      /* local's count of dumps */
    BUT_LAPSE_HELD = 4, /* a lapse named not simulated, until a feed */
    BUT_TUNE_HELD = 8,  /* a tune's end named not simulated */
    BUT_PRESSED = 16,   /* ext-key's script step */
    BUT_REENTERED = 32  /* reenter's script step */
};

/* the state equals before, apart from the fields named */
static int kept(const struct sim_state *before, int but)
{
    struct sim_state a;

    memcpy(&a, &st, sizeof(a));

    if (but & BUT_FED)
    {
        a.fed = before->fed;
    }

    if (but & BUT_DUMPS)
    {
        a.dumps = before->dumps;
    }

    if (but & BUT_LAPSE_HELD)
    {
        a.lapse_held = before->lapse_held;
    }

    if (but & BUT_TUNE_HELD)
    {
        a.tune_held = before->tune_held;
    }

    if (but & BUT_PRESSED)
    {
        a.pressed = before->pressed;
    }

    if (but & BUT_REENTERED)
    {
        a.reentered = before->reentered;
    }

    return memcmp(&a, before, sizeof(a)) == 0;
}

/* NOT SIMULATED: the call recorded, nothing sent, no state changed */
static void no_sim(const char *name, sim_ms ms, const char *line)
{
    struct sim_state before;
    const int n = unsim_calls;
    char got[2 * REPLY_MAX + 64];

    memcpy(&before, &st, sizeof(before));
    test_ms = ms;
    answer(line, reply);
    snprintf(got, sizeof(got), "sent '%s', %d unsim call(s), state %s",
             shown(reply), unsim_calls - n, kept(&before, KEEP_ALL) ? "kept" : "CHANGED");
    check(name, reply[0] == '\0' && unsim_calls == n + 1 && kept(&before, KEEP_ALL),
          got);
}

/* the same for n bytes through the line assembly */
static void no_sim_bytes(const char *name, sim_ms ms, const char *s, size_t n)
{
    struct sim_state before;
    const int calls = unsim_calls;
    char got[2 * REPLY_MAX + 64];

    memcpy(&before, &st, sizeof(before));
    bytes_n(ms, s, n);
    snprintf(got, sizeof(got), "sent '%s', %d unsim call(s), state %s",
             shown(reply), unsim_calls - calls,
             kept(&before, KEEP_ALL) ? "kept" : "CHANGED");
    check(name, reply[0] == '\0' && unsim_calls == calls + 1
          && kept(&before, KEEP_ALL), got);
}

/* NOT SIMULATED at a timer: the call recorded, nothing pushed, the state
 * kept but the fields named */
static void no_sim_at(const char *name, sim_ms ms, int but)
{
    struct sim_state before;
    const int n = unsim_calls;
    char got[2 * REPLY_MAX + 64];

    memcpy(&before, &st, sizeof(before));
    at(ms);
    snprintf(got, sizeof(got), "pushed '%s', %d unsim call(s), state %s",
             shown(pushed), unsim_calls - n, kept(&before, but) ? "kept" : "CHANGED");
    check(name, pushed[0] == '\0' && unsim_calls == n + 1
          && kept(&before, but), got);
}

/* a captured silence: nothing sent and no unsim call */
static void silent(const char *name, sim_ms ms, const char *line)
{
    const int n = unsim_calls;

    say(ms, line);
    check(name, reply[0] == '\0' && unsim_calls == n, reply);
}

/* the captured dumps the start states give, byte for byte */
static const char dump_home[] =
    "#1\n$0\n&0\n*0\n\"0\n'0\n(1\n,0\nA2\nM2\nR1\nZ0\nE0\nO1\nT1\nU\nF0399600\n"
    "X000\n^0\nG00\n)0\n.\n";
static const char dump_lapsed[] =
    "#1\n$0\n&0\n*0\n\"0\n'0\n(1\n,0\nA2\nM2\nR1\nZ0\nE0\nO1\nT1\nU\nF0399600\n"
    "X002\n^0\nG00\n)0\n.\nX000\n";
static const char dump_local[] =
    "#1\n$0\n&0\n*0\n\"1\n'1\n(1\n,0\nA2\nM2\nR0\nZ0\nE0\nO1\nT1\nU\nF0399600\n"
    "X000\n^0\n)0\n.\n";
static const char dump_tune[] =
    "#1\n$0\n&1\n*0\n\"0\n'0\n(1\n,0\nA2\nM2\nR1\nZ0\nE0\nO1\nT1\nU\nF0720000\n"
    "X000\n^0\nG00\n)0\n.\n";
static const char dump_front_panel[] =
    "!0\n#1\n$0\n&1\n*0\n\"0\n'0\n(1\n,0\nA2\nD4\nH1\nI0\nM2\nR1\nS0\nV0\n"
    "Y0\nZ0\nE0\nO1\nT1\nU\nC;;\nF0389500\nB100\nX000\nQT0\nQC\x05\n)0\n.\n";
static const char exit_push[] = "R0\n\"1\n'1\n,0\n)0\n";

static void selftest_rows(void)
{
    /* 3-1 */
    start("remote");
    expect("3-1 ? in REMOTE: the 22 lines", say(0, "?"), dump_home);
    start("lapsed");
    expect("3-1 ? lapsed: X002 in it and X000 after the .", say(0, "?"),
           dump_lapsed);
    expect("3-1 the lapsed dump's X000 clears the lapse", say(100, "U"), "U\n");

    /* 3-2, 3-22 (local's script: the operator leaves after the second dump) */
    start("local");
    expect("3-22 local: the first dump in REMOTE", say(0, "?"), dump_home);
    check("3-22 local: the exit push after the second dump",
          strcmp(say(10, "?"), "") != 0 && strlen(reply) > strlen(exit_push)
          && strcmp(reply + strlen(reply) - strlen(exit_push), exit_push) == 0,
          reply);
    expect("3-2 ? in local: the 21 lines", say(20, "?"), dump_local);

    /* 3-3 */
    start("panel");
    expect("3-3 panel: the front-panel dump", say(0, "?"), dump_front_panel);
    expect("3-3 panel: no J01 before 100 ms", at(99), "");
    expect("3-3 panel: J01 at 100 ms", at(100), "J01\n");
    expect("3-3 panel: no J01 at 150 ms", at(150), "");
    expect("3-3 panel: J01 at 200 ms", at(200), "J01\n");
    no_sim("3-3 panel: any other line", 210, "U");

    /* 3-4 */
    start("remote");
    expect("3-4 U in REMOTE: U", bytes(0, "U"), "U\n");
    start("lapsed");
    expect("3-4 U lapsed: X000 then U", say(0, "U"), "X000\nU\n");
    start("remote");
    say(0, "K");
    expect("3-4 U unkeys a K key: U", say(100, "U"), "U\n");
    expect_line("3-4 after the unkey the dump reads U", say(200, "?"), "U");
    start("ext-key");
    say(0, "'1");
    at(3000);
    expect("3-4 U while the handset keys: K", say(3100, "U"), "K\n");

    /* 3-5: a captured silence, not a call of unsim */
    start("local");
    say(0, "?");
    say(10, "?");
    silent("3-5 U in local: nothing", 20, "U");

    /* 3-6 */
    start("remote");
    expect("3-6 K, no tune owed: K", bytes(0, "K"), "K\n");
    expect("3-6 K again while keyed: K", say(100, "K"), "K\n");
    expect_line("3-6 keyed: the dump reads K", say(200, "?"), "K");
    start("lapsed");
    expect("3-6 K lapsed, no tune owed: X000 then K", say(0, "K"), "X000\nK\n");

    /* 3-7 */
    start("tune");
    expect("3-7 tune: the start dump owes a tune", say(0, "?"), dump_tune);
    expect("3-7 K with a tune owed: &0 K *1 K", say(10, "K"), "&0\nK\n*1\nK\n");

    /* 3-8 */
    say(1000, "?");
    expect_line("3-8 ? during the tune: the dump reads &0", reply, "&0");
    expect_line("3-8 ? during the tune: the dump reads *1", reply, "*1");
    expect_line("3-8 ? during the tune: the dump reads K", reply, "K");
    expect("3-8 nothing written: no end at 6 462 ms", at(6462), "");
    expect("3-8 nothing written: K then *0 at 6 463 ms", at(6463), "K\n*0\n");
    say(6500, "?");
    expect_line("3-8 after the tune still keyed: K", reply, "K");
    expect_line("3-8 after the tune: *0", reply, "*0");
    expect_line("3-8 after the tune: &0", reply, "&0");
    expect("3-8 the tuned frequency is stored: an F to it owes nothing",
           say(6600, "F0720000"), "F0720000\n");

    start("tune");
    say(0, "K");
    silent("3-8 K during the tune: nothing now", 4010, "K");
    expect("3-8 one K written: K then *0 at the end", at(6453), "K\n*0\n");

    start("tune");
    say(0, "K");
    silent("3-8 U during the tune: nothing now", 2400, "U");
    silent("3-8 the second U: nothing now", 3400, "U");
    silent("3-8 the third U: nothing now", 4400, "U");
    expect("3-8 three U: U then *0 at the end", at(6453), "U\n*0\n");
    expect_line("3-8 after three U: unkeyed", say(6500, "?"), "U");

    start("tune");
    say(0, "K");
    say(2004, "U");
    expect("3-8 one U: no end at 6 452 ms", at(6452), "");
    expect("3-8 one U: U then *0 at the end", at(6453), "U\n*0\n");
    expect_line("3-8 after one U: unkeyed", say(6500, "?"), "U");

    /* captured: a status read after the one U is answered whole, as before */
    start("tune");
    say(0, "K");
    say(2009, "U");
    say(3010, "?");
    check("3-8 ? after the one U during the tune: *1 and K",
          has_line(reply, "*1") && has_line(reply, "K"), reply);

    start("tune");
    say(0, "K");
    no_sim("3-8 an F during the tune", 1000, "F0720000");
    no_sim("3-8 a G during the tune", 1000, "G10");
    no_sim("3-8 a ( during the tune", 1000, "(1");
    say(1000, "U");
    no_sim("3-8 a K after a U during the tune", 1100, "K");
    say(2000, "U");
    say(3000, "U");
    no_sim("3-8 a fourth U during the tune", 3500, "U");

    start("tune");
    say(0, "K");
    say(1000, "K");
    no_sim("3-8 a U after a K during the tune", 1100, "U");

    start("tune");
    say(0, "K");
    say(1000, "U");
    say(2000, "U");
    no_sim_at("3-8 the end after two U", 6453, BUT_TUNE_HELD);
    expect("3-8 after two U: no lapse at 12 489 ms", at(12489), "");
    no_sim_at("3-8 the lapse after two U: not simulated", 12490, BUT_LAPSE_HELD);
    {
        const int n = unsim_calls;

        check("3-8 ... named once, nothing sent after it",
              at(13000)[0] == '\0' && unsim_calls == n, pushed);
    }
    expect_line("3-8 the end after two U changes nothing: still tuning",
                say(13100, "?"), "*1");

    /* 3-9 */
    start("local");
    say(0, "?");
    say(10, "?");
    no_sim("3-9 K in local", 20, "K");
    start("ext-key");
    say(0, "'1");
    at(3000);
    no_sim("3-9 K under the handset's key", 3100, "K");
    start("remote");
    say(0, "F0185000");
    at(10490);
    no_sim("3-9 K lapsed with a tune owed", 10500, "K");

    /* 3-10 */
    start("remote");
    expect("3-10 F: the echo", say(0, "F0399601"), "F0399601\n");
    expect_line("3-10 F: the dump shows it", say(10, "?"), "F0399601");
    start("remote");
    silent("3-10 A waits", 0, "A2");
    silent("3-10 M waits", 0, "M2");
    expect("3-10 A, M and F: confirmed in the order sent, then the F",
           say(0, "F0399600"), "A2\nM2\nF0399600\n");
    say(0, "M2");
    say(0, "A2");
    expect("3-10 M, A and F: confirmed in the order sent, then the F",
           say(0, "F0399600"), "M2\nA2\nF0399600\n");
    start("remote");
    say(0, "M4");
    expect("3-10 into CW: S1 H4", say(0, "F0399600"), "M4\nF0399600\nS1\nH4\n");
    say(0, "M2");
    expect("3-10 out of CW: S0 H1", say(0, "F0399600"),
           "M2\nF0399600\nS0\nH1\n");
    start("remote");
    say(0, "'1");
    say(0, "M4");
    expect("3-10 into CW with the PTT enable on: S1 H4, then U",
           say(0, "F0399600"), "M4\nF0399600\nS1\nH4\nU\n");
    start("remote");
    expect("3-17 C05 from the tuned frequency: C05 then &1", say(0, "C05"),
           "C05\n&1\n");
    say(0, "M4");
    expect("3-10 into CW leaving a channel back to the tuned frequency",
           say(0, "F0399600"), "M4\nF0399600\nS1\nH4\nC;;\n&0\n");
    start("lapsed");
    expect("3-10 F lapsed: the echo, then X000", say(0, "F0399600"),
           "F0399600\nX000\n");
    expect("3-10 the F cleared the lapse", say(10, "U"), "U\n");
    start("remote");
    expect("3-10 an F that owes a tune: &1", say(0, "F0403636"),
           "F0403636\n&1\n");
    expect("3-10 back to the tuned frequency: &0", say(0, "F0399600"),
           "F0399600\n&0\n");
    say(0, "F0403996");
    expect("3-10 from owed to owed: the echo only", say(0, "F0407592"),
           "F0407592\n");
    start("remote");
    expect("3-10 F0159999 (the low edge)", say(0, "F0159999"), "F0159999\n&1\n");
    expect("3-10 F3000000 (the high edge)", say(0, "F3000000"), "F3000000\n");
    no_sim("3-10 F below F0159999", 0, "F0159998");
    no_sim("3-10 F above F3000000", 0, "F3000001");
    no_sim("3-10 F with six digits", 0, "F039960");
    no_sim("3-10 F with eight digits", 0, "F03996000");

    /* 3-10, the tune owed: the captured bounds around 3 996.00 kHz */
    start("remote");
    expect("3-10 home +3956: no tune owed", say(0, "F0403556"), "F0403556\n");
    expect("3-10 home -3596: no tune owed", say(0, "F0396004"), "F0396004\n");
    expect("3-10 home -3956: owed", say(0, "F0395644"), "F0395644\n&1\n");
    expect("3-10 home +4036 (owed to owed)", say(0, "F0403636"), "F0403636\n");
    no_sim("3-10 home +3957: between the bounds", 0, "F0403557");
    no_sim("3-10 home +4035: between the bounds", 0, "F0403635");
    no_sim("3-10 home -3597: between the bounds", 0, "F0396003");
    no_sim("3-10 home -3955: between the bounds", 0, "F0395645");
    expect("3-10 home exactly: &0", say(0, "F0399600"), "F0399600\n&0\n");

    /* ... and around 1 850.00 kHz (the tune scenario's last tuned) */
    start("tune");
    expect("3-10 1850 +1869: none owed", say(0, "F0186869"), "F0186869\n&0\n");
    expect("3-10 1850 +2035: owed", say(0, "F0187035"), "F0187035\n&1\n");
    expect("3-10 1850 exactly: &0", say(0, "F0185000"), "F0185000\n&0\n");
    expect("3-10 1850 -1665: none owed", say(0, "F0183335"), "F0183335\n");
    expect("3-10 1850 -1832: owed", say(0, "F0183168"), "F0183168\n&1\n");
    no_sim("3-10 1850 +1870: between the bounds", 0, "F0186870");
    no_sim("3-10 1850 +2034: between the bounds", 0, "F0187034");
    no_sim("3-10 1850 -1666: between the bounds", 0, "F0183334");
    no_sim("3-10 1850 -1831: between the bounds", 0, "F0183169");

    /* ... and from any other tuned frequency: 0, or at least 0.8597 of it */
    start("tune");
    say(0, "K");
    at(6453);
    say(6500, "U");
    expect("3-10 another tuned frequency: 0.8597 of it owes",
           say(6600, "F1338984"), "F1338984\n&1\n");
    expect("3-10 another tuned frequency: exactly it owes nothing",
           say(6700, "F0720000"), "F0720000\n&0\n");
    no_sim("3-10 another tuned frequency: less than 0.8597 of it", 6800,
           "F1338983");

    /* 3-11 */
    start("remote");
    silent("3-11 M alone: nothing until the F", 0, "M1");
    expect("3-11 the F confirms the M", say(0, "F0399600"), "M1\nF0399600\n");
    silent("3-11 A alone: nothing until the F", 0, "A3");
    expect("3-11 the F confirms the A", say(0, "F0399600"), "A3\nF0399600\n");
    expect_line("3-11 the A took", say(0, "?"), "A3");
    say(0, "M1");
    no_sim("3-11 a ? while an M waits", 0, "?");
    no_sim("3-11 a second M while one waits", 0, "M2");

    /* 3-12, 3-13, 3-14, 3-15: in local */
    start("local");
    say(0, "?");
    say(10, "?");
    expect("3-12 F in local: the line twice", say(20, "F0399601"),
           "F0399601\nF0399601\n");
    expect_line("3-12 F in local: it takes", say(30, "?"), "F0399601");
    no_sim("3-12 F in local that owes a tune", 40, "F0403636");
    say(50, "A3");
    expect("3-13 A+F in local: An, F, then An, F again", say(50, "F0399600"),
           "A3\nF0399600\nA3\nF0399600\n");
    expect_line("3-13 A+F in local: it takes", say(60, "?"), "A3");
    say(70, "M1");
    expect("3-14 M1+F in local from LSB", say(70, "F0399600"),
           "M1\nF0399600\nM1\nA2\nV0\nI0\nF0399600\n");
    say(80, "?");
    expect_line("3-14 the AGC becomes slow", reply, "A2");
    expect_line("3-14 the mode took", reply, "M1");
    say(90, "M4");
    expect("3-14 M4+F in local: into CW, then a bare U", say(90, "F0399600"),
           "M4\nF0399600\nM4\nS1\nA2\nH4\nF0399600\nU\n");
    say(100, "M2");
    expect("3-14 M2+F in local: out of CW", say(100, "F0399600"),
           "M2\nF0399600\nM2\nS0\nA2\nH1\nV0\nI0\nF0399600\n");
    say(110, "M3");
    no_sim("3-14 M3 in local", 110, "F0399600");
    start("local");
    say(0, "?");
    say(10, "?");
    say(20, "M1");
    say(20, "A3");
    no_sim("3-14 M with A in local", 20, "F0399600");
    start("local");
    say(0, "?");
    say(10, "?");
    say(20, "M4");
    say(20, "F0399600");
    say(30, "M4");
    no_sim("3-14 CW to CW in local", 30, "F0399600");
    start("local");
    say(0, "?");
    say(10, "?");
    silent("3-15 ' in local: nothing", 20, "'0");
    silent("3-15 \" in local: nothing", 20, "\"0");
    silent("3-15 ( in local: nothing", 20, "(0");
    silent("3-15 C in local: nothing", 20, "C05");
    silent("3-15 G in local: nothing", 20, "G10");
    expect("3-15 in local: no change", say(30, "?"), dump_local);

    /* 3-16 */
    start("remote");
    silent("3-16 G: nothing", 0, "G10");
    expect_line("3-16 the same write's dump shows it", say(0, "?"), "G10");

    /* 3-17 */
    start("remote");
    say(0, "A3");
    say(0, "F0399600");
    say(0, "C05");
    say(10, "?");
    expect_line("3-17 the channel's frequency loaded", reply, "F1000000");
    expect_line("3-17 the channel's mode loaded", reply, "M1");
    expect_line("3-17 the AGC left as it was", reply, "A3");
    expect("3-17 a select while owed that stays owed: Cnn alone", say(20, "C49"),
           "C49\n");
    silent("3-17 C50: nothing", 30, "C50");
    silent("3-17 C99: nothing", 30, "C99");
    expect_line("3-17 C50 changes nothing", say(40, "?"), "F1000000");
    start("cw");
    no_sim("3-17 a select from CW", 0, "C05");
    start("remote");
    say(0, "C05");
    say(10, "K");
    at(6463);
    say(6500, "U");
    say(6600, "F3000000");
    no_sim("3-17 a select from owed to none owed", 6700, "C05");

    /* 3-18 */
    start("remote");
    expect("3-18 ( in REMOTE: the same line", say(0, "(0"), "(0\n");
    expect_line("3-18 the dump shows it", say(10, "?"), "(0");
    start("lapsed");
    expect("3-18 ( lapsed: X000 then the line", say(0, "(1"), "X000\n(1\n");

    /* 3-19 */
    start("remote");
    expect("3-19 '1: the same line", say(0, "'1"), "'1\n");
    expect("3-19 '1 again: the same line", say(0, "'1"), "'1\n");
    expect("3-19 \"0 unchanged: the same line", say(0, "\"0"), "\"0\n");
    expect("3-19 \"1: the same line", say(0, "\"1"), "\"1\n");
    say(10, "?");
    expect_line("3-19 the dump shows '1", reply, "'1");
    expect_line("3-19 the dump shows \"1", reply, "\"1");

    /* 3-20 */
    start("remote");
    say(0, "K");
    silent("3-20 keyed: G gets nothing", 10, "G10");
    silent("3-20 keyed: ( with the value held gets nothing", 20, "(1");
    expect("3-20 keyed: F is echoed", say(30, "F0399601"), "F0399601\n");
    say(40, "?");
    expect_line("3-20 keyed: the G was not stored", reply, "G00");
    expect_line("3-20 keyed: the frequency was not changed", reply, "F0399600");
    no_sim("3-20 keyed: ( with the other value", 50, "(0");
    no_sim("3-20 keyed: M", 50, "M1");
    no_sim("3-20 keyed: A", 50, "A3");
    no_sim("3-20 keyed: C", 50, "C05");
    no_sim("3-20 keyed: ' (never captured while keyed)", 50, "'1");

    /* 3-21 (ext-key's script: the handset pressed 3 s after the connection) */
    start("ext-key");
    say(0, "'1");
    expect("3-21 no press before 3 000 ms", at(2999), "");
    expect("3-21 the press with '1 pushes K", at(3000), "K\n");
    expect_line("3-21 the dump reads K", say(3100, "?"), "K");
    expect("3-21 U under the handset's key: K", say(3200, "U"), "K\n");
    expect("3-21 a lapse under it: X002, K", at(3200 + 10490), "X002\nK\n");
    expect("3-21 the next U: !1, X000, K", say(13700, "U"), "!1\nX000\nK\n");
    expect("3-21 a later U: K", say(13800, "U"), "K\n");
    no_sim("3-21 another write under the handset's key", 13900, "F0399600");
    start("ext-key");
    expect("3-21 the press with '0 pushes U", at(3000), "U\n");
    expect("3-21 with '0 not keyed: U", say(3100, "U"), "U\n");

    /* 3-22 (reenter's script) */
    start("reenter");
    expect("3-22 reenter: nothing before 8 000 ms", at(7999), "");
    expect("3-22 reenter: the exit push at 8 000 ms", at(8000), exit_push);
    expect("3-22 reenter: still local at 14 149 ms", at(14149), "");
    expect("3-22 reenter: the entry push 6 150 ms later", at(14150),
           "R1\n\"0\n'0\n");
    expect("keep-alive: starts again at a REMOTE entry (none at 24 639 ms)",
           at(24639), "");
    expect("keep-alive: starts again at a REMOTE entry (X002 at 24 640 ms)",
           at(24640), "X002\n");
    start("local");
    say(0, "?");
    say(5, "K");
    {
        const int n = unsim_calls;
        struct sim_state before;

        memcpy(&before, &st, sizeof(before));
        say(10, "?");
        check("3-22 leaving REMOTE while keyed: not simulated (the dump alone; the state kept but the dump's feed and count)",
              unsim_calls == n + 1 && strstr(reply, "R0\n") == NULL
              && kept(&before, BUT_FED | BUT_DUMPS), reply);
    }
    expect_line("3-22 still in REMOTE", say(20, "?"), "R1");

    /* 3-23 (deaf's script: the third ? of a connection and the line after) */
    start("deaf");
    say(0, "?");
    say(10, "?");
    silent("3-23 deaf: the third ? is not heard", 20, "?");
    silent("3-23 deaf: nor the line after it", 2020, "?");
    expect_line("3-23 deaf: the next ? is answered", say(4020, "?"), ".");
    connection_opened();
    say(5000, "?");
    say(5010, "?");
    silent("3-23 deaf, a second connection: its third ? is not heard", 5020, "?");

    /* 3-24 (no-gain's script: the first G is not taken) */
    start("no-gain");
    say(0, "G10");
    expect_line("3-24 no-gain: the first G not taken", say(10, "?"), "G00");
    say(20, "G10");
    expect_line("3-24 no-gain: a later G taken", say(30, "?"), "G10");

    /* 3-25 (late-gain's script: every G is a late one) */
    start("late-gain");
    say(0, "G10");
    expect_line("3-25 late-gain: the same write's dump: old", say(0, "?"), "G00");
    expect_line("3-25 late-gain: the next: old", say(250, "?"), "G00");
    expect_line("3-25 late-gain: the next: old", say(500, "?"), "G00");
    expect_line("3-25 late-gain: the fourth: new", say(750, "?"), "G10");
    say(1000, "G20");
    say(1000, "?");
    no_sim("3-25 late-gain: a second G before the fourth dump", 1100, "G30");

    /* 3-26 */
    start("remote");
    no_sim("3-26 a line the radio was never asked (P00)", 0, "P00");
    no_sim("3-26 an empty line", 0, "");
    no_sim("3-26 '2", 0, "'2");
    no_sim("3-26 (2", 0, "(2");
    no_sim("3-26 G64", 0, "G64");
    no_sim("3-26 G with one digit", 0, "G1");
    no_sim("3-26 M5", 0, "M5");
    no_sim("3-26 A0", 0, "A0");
    no_sim("3-26 C with one digit", 0, "C5");
    no_sim("3-26 ? with a CR", 0, "?\r");
}

static void selftest_keepalive(void)
{
    static const char *const feeders[] = { "?", "U", "K", "F0399600", "(1" };
    static const char *const others[] = { "G00", "'0", "\"0" };
    char name[96];
    size_t i;

    for (i = 0; i < sizeof(feeders) / sizeof(feeders[0]); i++)
    {
        start("remote");
        say(1000, feeders[i]);
        snprintf(name, sizeof(name), "keep-alive: %s feeds it (none at 11 489 ms)",
                 feeders[i]);
        expect(name, at(11489), "");
        snprintf(name, sizeof(name), "keep-alive: %s feeds it (X002 at 11 490 ms)",
                 feeders[i]);
        expect(name, at(11490), i == 2 ? "X002\nU\n" : "X002\n");
    }

    for (i = 0; i < sizeof(others) / sizeof(others[0]); i++)
    {
        start("remote");
        say(1000, others[i]);
        snprintf(name, sizeof(name),
                 "keep-alive: %s does not feed it (none at 10 489 ms)",
                 others[i]);
        expect(name, at(10489), "");
        snprintf(name, sizeof(name),
                 "keep-alive: %s does not feed it (X002 at 10 490 ms)",
                 others[i]);
        expect(name, at(10490), "X002\n");
    }

    start("remote");
    at(10490);
    expect("keep-alive: X002 is pushed once", at(30000), "");
    start("remote");
    say(0, "K");
    expect("keep-alive: keyed by K, X002 then U", at(10490), "X002\nU\n");
    expect_line("keep-alive: keyed by K, the radio unkeys", say(10500, "?"), "U");
    start("local");
    say(0, "?");
    say(10, "?");
    expect("keep-alive: it does not run in local", at(30000), "");
    start("remote");
    at(9000);
    connection_opened();
    expect("keep-alive: a new connection feeds it (none at 19 489 ms)", at(19489),
           "");
    expect("keep-alive: a new connection feeds it (X002 at 19 490 ms)", at(19490),
           "X002\n");
    start("remote");
    say(1000, "C50");
    no_sim_at("keep-alive: running out after a C is not simulated", 10490,
              BUT_LAPSE_HELD);
    start("remote");
    say(1000, "M1");
    no_sim_at("keep-alive: running out after a lone M is not simulated", 10490,
              BUT_LAPSE_HELD);
}

static void selftest_framing(void)
{
    start("remote");
    expect("framing: no line before its LF", bytes(0, "F0399601"), "");
    expect("framing: the LF ends it", bytes(0, "\n"), "F0399601\n");
    expect("framing: a bare U with no line open is a line", bytes(0, "U"), "U\n");
    expect("framing: a bare K with no line open is a line", bytes(0, "K"), "K\n");
    expect("framing: K then U, two lines", bytes(0, "KU"), "K\nU\n");
    start("remote");
    expect("framing: M and F in one write", bytes(0, "M1\nF0399600\n"),
           "M1\nF0399600\n");
    {
        const int n = unsim_calls;

        check("framing: a U inside an open line is part of it",
              bytes(0, "?U")[0] == '\0' && unsim_calls == n, "");
        bytes(0, "\n");
        check("framing: ... and the line ?U is not simulated",
              unsim_calls == n + 1 && reply[0] == '\0', reply);
    }
}

/* a line no capture holds whole is not simulated whole: never cut,
 * cleaned or shortened into one that is */
static void selftest_whole_lines(void)
{
    static const char nul_inside[] = "F0399601\0HELLO\n";
    static const char nul_first[] = "\0?\n";
    static const char nul_before_u[] = "\0U";
    static const char high_bit[] = "?\xbf\n";
    static const char cr_end[] = "?\r\n";
    char longline[LINE_MAX_LEN + 16];

    start("remote");
    no_sim_bytes("framing: a NUL inside a line: the whole line not simulated", 0,
                 nul_inside, sizeof(nul_inside) - 1);
    expect_line("framing: ... and the frequency did not move", say(10, "?"),
                "F0399600");
    no_sim_bytes("framing: a NUL first in a line: the whole line not simulated", 20,
                 nul_first, sizeof(nul_first) - 1);
    no_sim_bytes("framing: a byte above 0x7e in a line: not simulated", 30,
                 high_bit, sizeof(high_bit) - 1);
    no_sim_bytes("framing: a CR before the LF: not simulated", 40, cr_end,
                 sizeof(cr_end) - 1);
    {
        const int n = unsim_calls;

        check("framing: a U after a NUL is inside the open line, not a line",
              bytes_n(50, nul_before_u, sizeof(nul_before_u) - 1)[0] == '\0'
              && unsim_calls == n, reply);
        no_sim_bytes("framing: ... and that line is not simulated at its LF", 50,
                     "\n", 1);
    }
    memset(longline, '0', sizeof(longline));
    longline[0] = 'F';
    longline[sizeof(longline) - 1] = '\n';
    no_sim_bytes("framing: a line longer than the simulator holds: not simulated",
                 60, longline, sizeof(longline));
    expect("framing: the next line is assembled afresh", bytes(70, "U"), "U\n");
    no_sim_bytes("framing: a line longer than the simulator holds, before a ?", 80,
                 longline, sizeof(longline));
    expect("framing: ... and the ? after it is answered with the whole dump",
           bytes(90, "?\n"), dump_home);
}

/* every call of unsim with its check of the state: those the cases above
 * do not check */
static void selftest_unsim_sites(void)
{
    /* lapse(): a second lapse under the handset's key, after the !1 */
    start("ext-key");
    say(0, "'1");
    at(3000);
    say(3200, "U");
    at(13690);
    say(13700, "U");
    no_sim_at("keep-alive: a second lapse under the handset's key", 24190,
              BUT_LAPSE_HELD);

    /* press(): the handset pressed over a K key */
    start("ext-key");
    say(0, "'1");
    say(0, "K");
    no_sim_at("3-21 the handset pressed over a K key", 3000, BUT_PRESSED);
    expect_line("3-21 ... still keyed by the K", say(3100, "?"), "K");

    /* leave_remote() from reenter's script, keyed */
    start("reenter");
    say(0, "K");
    no_sim_at("3-22 reenter: leaving REMOTE while keyed", 8000, BUT_REENTERED);
    expect_line("3-22 ... still in REMOTE", say(8100, "?"), "R1");
    expect("3-22 ... and no entry follows", at(14150), "");

    /* answer(): the front-panel format lapsed */
    start("panel");
    at(10490);
    no_sim("3-3 panel: a dump while lapsed", 10500, "?");

    /* remote_line(): a non-feeder during a lapse */
    start("lapsed");
    no_sim("3-16 G during a lapse", 0, "G10");
    no_sim("3-19 ' during a lapse", 0, "'1");
    no_sim("3-17 C during a lapse", 0, "C05");
    no_sim("3-11 M during a lapse", 0, "M1");

    /* f_remote(): into CW with the PTT enable on while leaving a channel */
    start("remote");
    say(0, "'1");
    say(0, "C05");
    say(0, "M4");
    no_sim("3-10 into CW with '1, leaving a channel", 0, "F0399600");

    /* f_local(): an F in local with a channel selected */
    start("local");
    say(0, "?");
    say(0, "C05");
    say(10, "?");
    no_sim("3-12 F in local with a channel selected", 20, "F1000001");

    /* c_line(): a select between two captured points */
    start("remote");
    say(0, "F0600000");
    say(10, "K");
    at(6463);
    say(6500, "U");
    no_sim("3-17 a select owing no capture's answer (0.67 of the tuned)", 6600,
           "C05");

    /* local_line(): an unknown line in local */
    start("local");
    say(0, "?");
    say(10, "?");
    no_sim("3-26 an unknown line in local", 20, "P00");
}

/* states the matrix starts a line in; the time the line is sent */
enum
{
    IN_UNKEYED, IN_KEYED, IN_HANDSET, IN_LAPSED, IN_TUNING, IN_PANEL
};

static sim_ms in_state(int s)
{
    switch (s)
    {
    case IN_KEYED:
        start("remote");
        say(0, "K");
        return 1000;

    case IN_HANDSET:
        start("ext-key");
        say(0, "'1");
        at(3000);
        return 4000;

    case IN_LAPSED:
        start("lapsed");
        return 1000;

    case IN_TUNING:
        start("tune");
        say(0, "K");
        return 5000;

    case IN_PANEL:
        start("panel");
        return 1000;

    default:
        start("remote");
        return 1000;
    }
}

/* the lapse after a line in a state: none at lapse_ms - 1, want at lapse_ms */
static void lapse_after(const char *name, int s, const char *line,
                        sim_ms lapse_ms, const char *want)
{
    const sim_ms t = in_state(s);
    char n1[128];

    say(t, line);

    if (s == IN_TUNING)
    {
        at(6453);
    }

    snprintf(n1, sizeof(n1), "%s (no lapse before)", name);
    check(n1, strstr(at(lapse_ms - 1), "X002") == NULL, pushed);
    check(name, strstr(at(lapse_ms), want) != NULL, pushed);
}

/* every feeder and non-feeder of the keep-alive in every state that takes
 * it; a line not simulated in a state has its check in the numbered cases */
static void selftest_feed_matrix(void)
{
    lapse_after("feeds, unkeyed: ?", IN_UNKEYED, "?", 11490, "X002\n");
    lapse_after("feeds, unkeyed: U", IN_UNKEYED, "U", 11490, "X002\n");
    lapse_after("feeds, unkeyed: K", IN_UNKEYED, "K", 11490, "X002\nU\n");
    lapse_after("feeds, unkeyed: F", IN_UNKEYED, "F0399600", 11490, "X002\n");
    lapse_after("feeds, unkeyed: (", IN_UNKEYED, "(1", 11490, "X002\n");
    lapse_after("does not feed, unkeyed: G", IN_UNKEYED, "G00", 10490, "X002\n");
    lapse_after("does not feed, unkeyed: '", IN_UNKEYED, "'0", 10490, "X002\n");
    lapse_after("does not feed, unkeyed: \"", IN_UNKEYED, "\"0", 10490, "X002\n");
    lapse_after("feeds, keyed: ?", IN_KEYED, "?", 11490, "X002\nU\n");
    lapse_after("feeds, keyed: U", IN_KEYED, "U", 11490, "X002\n");
    lapse_after("feeds, keyed: K", IN_KEYED, "K", 11490, "X002\nU\n");
    lapse_after("feeds, keyed: F", IN_KEYED, "F0399601", 11490, "X002\nU\n");
    lapse_after("feeds, keyed: ( with the value held", IN_KEYED, "(1", 11490,
                "X002\nU\n");
    lapse_after("does not feed, keyed: G", IN_KEYED, "G00", 10490, "X002\nU\n");
    lapse_after("feeds, under the handset: ?", IN_HANDSET, "?", 14490, "X002\nK\n");
    lapse_after("feeds, under the handset: U", IN_HANDSET, "U", 14490, "X002\nK\n");
    lapse_after("feeds, lapsed: ?", IN_LAPSED, "?", 11490, "X002\n");
    lapse_after("feeds, lapsed: U", IN_LAPSED, "U", 11490, "X002\n");
    lapse_after("feeds, lapsed: K", IN_LAPSED, "K", 11490, "X002\nU\n");
    lapse_after("feeds, lapsed: F", IN_LAPSED, "F0399600", 11490, "X002\n");
    lapse_after("feeds, lapsed: (", IN_LAPSED, "(1", 11490, "X002\n");
    lapse_after("feeds, tuning: ?", IN_TUNING, "?", 15490, "X002\nU\n");
    lapse_after("feeds, tuning: K", IN_TUNING, "K", 15490, "X002\nU\n");
    lapse_after("feeds, tuning: U", IN_TUNING, "U", 15490, "X002\n");
    lapse_after("feeds, front-panel format: ?", IN_PANEL, "?", 11490, "X002\n");
}

/* time runs while no client is connected: a new connection first plays
 * what the clock owes, writing nothing, then counts as a feed */
static void selftest_reconnect(void)
{
    start("remote");
    say(0, "K");
    pushed[0] = '\0';
    test_ms = 12000;
    connection_opened();
    expect("reconnect: nothing is written for what the clock owed", pushed, "");
    expect("reconnect after 12 000 ms, keyed by K: the lapse unkeyed it (the lapsed dump)",
           say(12000, "?"), dump_lapsed);

    start("remote");
    say(0, "K");
    test_ms = 5000;
    connection_opened();
    expect_line("reconnect after 5 000 ms: still keyed", say(5000, "?"), "K");
    expect("reconnect after 5 000 ms: no lapse 10 489 ms after it", at(15489), "");
    expect("reconnect after 5 000 ms: the lapse 10 490 ms after it", at(15490),
           "X002\nU\n");

    start("tune");
    say(0, "K");
    pushed[0] = '\0';
    test_ms = 8000;
    connection_opened();
    expect("reconnect during a tune: nothing is written for its end", pushed, "");
    say(8000, "?");
    expect_line("reconnect 8 000 ms into a tune: it is over (*0)", reply, "*0");
    expect_line("reconnect 8 000 ms into a tune: keyed as it ended (K)", reply,
                "K");

    start("tune");
    say(0, "K");
    say(2000, "U");
    test_ms = 8000;
    connection_opened();
    say(8000, "?");
    expect_line("reconnect after a tune with one U: it ended unkeyed (*0)", reply,
                "*0");
    expect_line("reconnect after a tune with one U: it ended unkeyed (U)", reply,
                "U");
}

static int run_selftest(void)
{
    selftest = 1;
    selftest_rows();
    selftest_keepalive();
    selftest_framing();
    selftest_whole_lines();
    selftest_unsim_sites();
    selftest_feed_matrix();
    selftest_reconnect();

    if (st_fail)
    {
        fprintf(stderr, "simrf350k selftest: %d of %d checks failed\n", st_fail,
                st_checks);
        return 1;
    }

    printf("simrf350k selftest: all %d checks passed\n", st_checks);
    return 0;
}

int main(int argc, char *argv[])
{
    const char *scenario = argc > 2 ? argv[2] : "remote";
    static const char *const known[] =
    {
        "remote", "local", "lapsed", "cw", "ext-key", "panel", "tune",
        "reenter", "late-gain", "no-gain", "deaf",
    };
    size_t i;

    if (argc == 2 && strcmp(argv[1], "--selftest") == 0)
    {
        return run_selftest();
    }

    for (i = 0; i < sizeof(known) / sizeof(known[0]); i++)
    {
        if (strcmp(scenario, known[i]) == 0)
        {
            break;
        }
    }

    if (argc < 2 || !is_number(argv[1]) || i == sizeof(known) / sizeof(known[0]))
    {
        fprintf(stderr,
                "usage: %s <tcp-port> [remote|local|lapsed|cw|ext-key|panel|tune|reenter|late-gain|no-gain|deaf]\n"
                "       %s --selftest\n"
                "It replays answers captured from one radio (REV 604M firmware, the ADAPT\n"
                "status format); any other input is ignored and named on its console as\n"
                "not simulated.\n", argv[0], argv[0]);
        return 2;
    }

    init_state(scenario);
    return serve_tcp(atoi(argv[1]));
}
