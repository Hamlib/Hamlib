/*
 *  Hamlib simulator - Harris RF-5800H (AN/PRC-150 type) REMOTE port
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
 * Usage: simrf5800 <port> [scenario]
 *        simrf5800 --selftest
 *
 * <port> is a COM port or pty name (sim.h's openPort), or a number: a TCP
 * port to listen on at 127.0.0.1, one connection at a time (0 picks a free
 * port; the port is printed as "listening on 127.0.0.1:<n>").  Then
 *   rigctl -m 42002 -r 127.0.0.1:<n>
 *
 * Scenarios: ssb (default: SSB> on channel 25), ale (ALE>, scanning),
 * ale-call (ALE> with a call going out), busy (the radio's self-test is
 * running: the SSB word is refused 7021 and the prompt stays ALE>), hop
 * (HOP>), prgm (PRGM>), echo-off (REMOTE ECHO OFF), prc138 (a PRC-138 on
 * the line), bypassed (the coupler bypassed), rxonly (channel 0 receive
 * only).
 *
 * It replays answers captured from one RF-5800H; any other line is ignored
 * and named on the simulator's console as not simulated.
 *
 * --selftest drives the answers and the line assembly directly, with no
 * link, and exits non-zero on any failed check.
 */

/* the pty functions sim.h uses on POSIX */
#define _XOPEN_SOURCE 700

/* first: on Windows winsock2.h must come before anything that pulls in windows.h */
#include "rf5800_sock.h"

#ifdef _WIN32
#include <ws2tcpip.h>
#define SIM_BAD_SOCK INVALID_SOCKET
#else
#include <arpa/inet.h>
#include <netinet/in.h>
#include <sys/socket.h>
#define SIM_BAD_SOCK (-1)
#endif

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "misc.h"

#include "sim.h"

#define LINE_MAX_LEN 128
/* a line shown on the console: each byte up to four characters */
#define RENDER_MAX   (LINE_MAX_LEN * 4 + 32)
#define REPLY_MAX    2048

/* captured: FREQ 1.6 and 59.999999 accepted, 1.59999 and 60 refused 7001 */
#define MIN_HZ 1600000L
#define MAX_HZ 59999999L
/* ale-call: the SSB burst followed the ALE> prompt 134 and 158 ms later */
#define ALE_CALL_GAP_MS 150

#define ERR7001 "7001  'BAD VALUE'"
#define ERR7015 "7015  'COMMAND SYNTAX ERROR (TYPE ? FOR HELP)'"
#define ERR7019 "7019  'OPERATION IS NOT VALID IN THE CURRENT MODE'"
#define ERR7021 "7021  'SYSTEM BUSY - COMMAND IN PROGRESS'"
#define ERR7040 "7040  'CANNOT RETUNE WHEN COUPLER IS BYPASSED'"

#define NO_REPLAY "no captured answer is replayed for this line in this state"
#define NO_ARG    "no captured answer is replayed for this argument"
#define TOO_LONG  "the line is longer than the simulator holds, so no part of it is answered"
#define NOT_ASCII "the line holds a byte that is not printable ASCII, so no part of it is answered"

/* a channel as the radio reads it back */
struct chan
{
    int number;
    long rx_hz, tx_hz;
    char mode[4];
    char agc[5];
    int width;
    int rxonly;
};

static struct
{
    const char *scenario;
    char prompt[8];
    int echo;
    struct chan chans[2];   /* channel 0 and channel 25 */
    struct chan *cur;
    char power[5];
    int squelch;
    char sq_level[5];
    int rfgain;
} st;

/* the link: a TCP socket or the fd sim.h's openPort returned */
static int tcp;
static sock_t sock;
static int fd;

/* --selftest: what is sent ahead of an answer lands in sent[], a wait is
 * counted, not slept, and a line with no captured answer is recorded */
static int selftest;
static char sent[REPLY_MAX];
static size_t sent_at_pause;
static long paused_ms;
static int unsim_calls;
static char unsim_line[RENDER_MAX];

static int link_read(char *c)
{
    if (tcp)
    {
        return sock_read(sock, c, 1);
    }

    return read(fd, c, 1) == 1 ? 0 : -1;
}

static void link_write(const char *s)
{
    size_t n = strlen(s);

    if (tcp)
    {
        sock_write(sock, s, n);
    }
    else if (write(fd, s, n) != (int) n)
    {
        perror("write");
    }
}

/* sends part of an answer at once, ahead of the rest */
static void send_now(const char *s)
{
    if (selftest)
    {
        strncat(sent, s, sizeof(sent) - strlen(sent) - 1);
        return;
    }

    link_write(s);
}

static void pause_ms(long ms)
{
    if (selftest)
    {
        sent_at_pause = strlen(sent);
        paused_ms += ms;
        return;
    }

    hl_usleep(ms * 1000);
}

/* A line with no captured answer in this state: nothing is sent, nothing
 * changes, and the console says so. */
static void unsim(const char *line, const char *why)
{
    if (selftest)
    {
        unsim_calls++;
        snprintf(unsim_line, sizeof(unsim_line), "%s", line);
        return;
    }

    printf("not simulated: %s (%s, scenario %s): %s\n", line, st.prompt,
           st.scenario, why);
    fflush(stdout);
}

static int is(const char *s)
{
    return strcmp(st.scenario, s) == 0;
}

static int at(const char *prompt)
{
    return strcmp(st.prompt, prompt) == 0;
}

static int one_of(const char *s, const char *const *words)
{
    for (; *words; words++)
    {
        if (strcmp(s, *words) == 0)
        {
            return 1;
        }
    }

    return 0;
}

static void add(char *out, const char *fmt, const char *a)
{
    size_t n = strlen(out);

    snprintf(out + n, REPLY_MAX - n, fmt, a);
}

static void mhz(long hz, char *out, size_t n)
{
    snprintf(out, n, "%ld.%06ld", hz / 1000000L, hz % 1000000L);
}

/* captured: "* CURRENT CHAN 0: RX 7.231000  TX 7.231000 LSB" */
static void push_chan(char *out)
{
    char rx[16], tx[16], line[80];

    mhz(st.cur->rx_hz, rx, sizeof(rx));
    mhz(st.cur->tx_hz, tx, sizeof(tx));
    snprintf(line, sizeof(line), "* CURRENT CHAN %d: RX %s  TX %s %s\r\n",
             st.cur->number, rx, tx, st.cur->mode);
    add(out, "%s", line);
}

/* the two pushes that follow a change to the channel or its preset */
static void pushes(char *out)
{
    add(out, "%s", "* PRESET CHANGED TO MANUAL\r\n");
    push_chan(out);
}

/* the command comes back first, unless REMOTE ECHO is off */
static void echo(char *out, const char *cmd)
{
    if (st.echo)
    {
        add(out, "%s\r\n", cmd);
    }
}

/* every answer ends with a blank line, the prompt and a space */
static void prompt(char *out)
{
    add(out, "\r\n%s ", st.prompt);
}

/* captured: the echo, ERROR: nnnn  'TEXT' (two spaces), a blank line, the
 * prompt */
static void error(char *out, const char *cmd, const char *code_text)
{
    echo(out, cmd);
    add(out, "ERROR: %s\r\n", code_text);
    prompt(out);
}

/* the power reduction the radio reports at each POWER level (captured in
 * the POWER writes' answers, and in the SSB word's at LOW and HIGH) */
static const char *reduction(void)
{
    if (strcmp(st.power, "LOW") == 0)
    {
        return "13.0";
    }

    return strcmp(st.power, "MED") == 0 ? "6.0" : "0.0";
}

/* Captured by read-back: the widths each mode's channel takes.  A width
 * outside the mode's table falls to the mode's widest; a mode change keeps
 * a width the new table holds and turns any other to its widest. */
static const int bw_ssb[] = { 2000, 2400, 2700, 3000, 0 };
static const int bw_cw[] = { 350, 500, 1000, 1500, 0 };
static const int bw_ame[] = { 4000, 0 };
static const int bw_fm[] = { 25000, 0 };

static const int *widths(const char *mode)
{
    if (strcmp(mode, "CW") == 0) { return bw_cw; }

    if (strcmp(mode, "AME") == 0) { return bw_ame; }

    if (strcmp(mode, "FM") == 0) { return bw_fm; }

    return bw_ssb;
}

static int width_ok(const char *mode, int hz)
{
    const int *t;

    for (t = widths(mode); *t; t++)
    {
        if (*t == hz)
        {
            return 1;
        }
    }

    return 0;
}

static int widest(const char *mode)
{
    const int *t = widths(mode);

    while (t[1])
    {
        t++;
    }

    return *t;
}

/* the radio pads a width to four digits: BANDWIDTH 0350 */
static void width_text(int hz, char *out, size_t n)
{
    snprintf(out, n, "%04d", hz);
}

/* MHz as digits, or digits, a point and digits (the forms captured: 2.5,
 * 07.231, 60, 7231), to Hz rounded to 1 Hz (captured: 7.23100049 reads
 * 7.231000, 7.2310005 reads 7.231001); 0 when the text is not that */
static int parse_mhz(const char *s, long *hz)
{
    long whole = 0, micro = 0;
    int digits = 0, point = 0, places = 0, frac, up = 0;

    /* no leading point, and at most one leading zero, as 07.231 */
    if (*s == '.' || (s[0] == '0' && s[1] == '0'))
    {
        return 0;
    }

    for (; *s; s++)
    {
        if (*s == '.' && !point)
        {
            point = 1;
            continue;
        }

        if (*s < '0' || *s > '9')
        {
            return 0;
        }

        digits++;

        if (!point)
        {
            /* held at 1000 MHz, outside the range already, so that the Hz
             * below cannot overflow a 32-bit long */
            whole = whole * 10 + (*s - '0');

            if (whole > 1000)
            {
                whole = 1000;
            }
        }
        else if (places < 6)
        {
            micro = micro * 10 + (*s - '0');
            places++;
        }
        else if (places++ == 6)
        {
            up = *s >= '5';
        }
    }

    frac = places;

    for (; places < 6; places++)
    {
        micro *= 10;
    }

    *hz = whole * 1000000L + micro + up;
    /* a point with no digit after it is not a captured form */
    return digits > 0 && (!point || frac > 0);
}

/* captured: a write to the channel answers its echo, the two pushes and the
 * ack, also when nothing changes */
static void chan_write(char *out, const char *cmd, const char *ack)
{
    echo(out, cmd);
    pushes(out);
    add(out, "%s\r\n", ack);
    prompt(out);
}

/* captured: a RADIO setting answers the NVRAM push, then the two pushes and
 * the ack, also when the value is the one held */
static void radio_write(char *out, const char *cmd)
{
    echo(out, cmd);
    add(out, "%s", "* RUNTIME_STATUS: 8077 \"NVRAM DATABASE CORRECTED\"\r\n");
    pushes(out);
    add(out, "%s\r\n", cmd);
    prompt(out);
}

/* CHAN: 11 rows, END-OF-LIST */
static void read_chan(char *out)
{
    char v[32];

    echo(out, "CHAN");
    snprintf(v, sizeof(v), "%d", st.cur->number);
    add(out, "CHAN CURRENT NUMBER %s\r\n", v);
    mhz(st.cur->rx_hz, v, sizeof(v));
    add(out, "CHAN CURRENT RXFREQ %s\r\n", v);
    mhz(st.cur->tx_hz, v, sizeof(v));
    add(out, "CHAN CURRENT TXFREQ %s\r\n", v);
    add(out, "CHAN CURRENT MODE %s\r\n", st.cur->mode);
    /* captured: FM reads AGC FAST whatever was written, and the AGC written
     * in FM shows once the channel leaves FM */
    add(out, "CHAN CURRENT AGC %s\r\n",
        strcmp(st.cur->mode, "FM") == 0 ? "FAST" : st.cur->agc);
    width_text(st.cur->width, v, sizeof(v));
    add(out, "CHAN CURRENT BANDWIDTH %s\r\n", v);
    add(out, "CHAN CURRENT RXONLY %s\r\n", st.cur->rxonly ? "YES" : "NO");
    add(out, "%s", "CHAN CURRENT HAIL DISABLED\r\n"
             "CHAN CURRENT HAILKEY 0\r\n"
             "CHAN CURRENT MAXPOWER NONE\r\n"
             "CHAN CURRENT SSBSCAN NO\r\n"
             "END-OF-LIST\r\n");
    prompt(out);
}

/* PRESET at SSB> and PRGM>: 13 rows, END-OF-LIST */
static void read_preset(char *out)
{
    char v[16];

    echo(out, "PRESET");
    add(out, "%s", "PRESET CURRENT NAME MANUAL\r\n"
             "PRESET CURRENT PRENUMBER 0\r\n"
             "PRESET CURRENT SSB\r\n"
             "PRESET CURRENT NET 1\r\n"
             "PRESET CURRENT MODEM OFF\r\n"
             "PRESET CURRENT MDMNUMBER 0\r\n");
    snprintf(v, sizeof(v), "%d", st.cur->number);
    add(out, "PRESET CURRENT CHAN %s\r\n", v);
    add(out, "%s", "PRESET CURRENT KEY (NONE)\r\n"
             "PRESET CURRENT PT CLR\r\n"
             "PRESET CURRENT CT 2400\r\n"
             "PRESET CURRENT ADDRESS (NONE)\r\n"
             "PRESET CURRENT CRYPTOTYPE NONE\r\n"
             "PRESET CURRENT ENABLE\r\n"
             "END-OF-LIST\r\n");
    prompt(out);
}

/* RADIO: 19 rows, END-OF-LIST; TXGAIN reads the POWER level */
static void read_radio(char *out)
{
    char v[16];

    echo(out, "RADIO");
    add(out, "%s", "RADIO NAME \r\n");
    add(out, "RADIO COUPLER %s\r\n", is("bypassed") ? "BYPASS" : "ENABLE");
    add(out, "%s", "RADIO HIGHQ_ANTENNA NOT_PRESENT\r\n");
    add(out, "RADIO TXGAIN %s\r\n", st.power);
    add(out, "%s", "RADIO SQ_TYPE SYLLABIC\r\n"
             "RADIO FMSQUELCH TONE\r\n");
    add(out, "RADIO SQ_LEVEL %s\r\n", st.sq_level);
    add(out, "%s", "RADIO RXNOISEBLANK ON\r\n"
             "RADIO FMDEVIATION 8.0\r\n"
             "RADIO COMPRESSION ON\r\n"
             "RADIO CWOFFSET 1000\r\n"
             "RADIO BFO 0\r\n"
             "RADIO SILENCE OFF\r\n"
             "RADIO SLFAD (NONE) ADDRESS 0\r\n"
             "RADIO 20W_COUPLER MEMORY\r\n");
    snprintf(v, sizeof(v), "%d", st.rfgain);
    add(out, "RADIO RFGAIN %s\r\n", v);
    add(out, "RADIO SQUELCH %s\r\n", st.squelch ? "ON" : "OFF");
    add(out, "%s", "RADIO TYPE RF-5800H\r\n"
             "RADIO BASEBAND OFF\r\n"
             "END-OF-LIST\r\n");
    prompt(out);
}

/* the SSB word's burst after its first lines, ending at SSB> */
static void burst(char *out)
{
    pushes(out);
    add(out, "%s", "* STATE OPMODE SSB\r\n");
    add(out, "* POWER REDUCTION %s DB\r\n", reduction());
    add(out, "%s", "* RADIO SILENCE OFF\r\nSSB\r\n");
    strcpy(st.prompt, "SSB>");
    prompt(out);
}

/* The SSB word, each shape as captured from its prompt. */
static void ssb_word(char *out)
{
    echo(out, "SSB");

    if (at("ALE>") && is("ale-call"))
    {
        /* during an outgoing call the word aborts the call and is answered
         * ALE>; the burst follows unasked, then the two pushes again */
        add(out, "%s", "* KEYSRC ALE KEYTYPE OFF\r\n"
                 "* ALECALL FAILED CALL ABORTED\r\n");
        prompt(out);
        send_now(out);
        out[0] = '\0';
        pause_ms(ALE_CALL_GAP_MS);
        burst(out);
        pushes(out);
    }
    else if (at("ALE>"))
    {
        /* from a scanning ALE: the coupler push first, and the two pushes
         * again after the prompt */
        add(out, "%s", "* COUPLER_STATUS UNBYPASSED\r\n");
        burst(out);
        pushes(out);
    }
    else if (at("HOP>"))
    {
        add(out, "%s", "* MDMSTAT ID:1 OFF\r\n* COUPLER_STATUS UNBYPASSED\r\n");
        burst(out);
    }
    else
    {
        /* at SSB>: the bare burst, nothing changes */
        burst(out);
    }
}

/* RETUNE: complete, refused with the coupler bypassed, or failed on a
 * receive-only channel; the two pushes ride the prompt line.  The id is the
 * radio's own counter, replayed as captured. */
static void retune(char *out)
{
    if (is("bypassed"))
    {
        error(out, "RETUNE", ERR7040);
        return;
    }

    echo(out, "RETUNE");
    add(out, "%s", "RETUNE\r\n");

    if (st.cur->rxonly)
    {
        add(out, "%s", "* EXCEPTION: 3100 \"CAN'T KEY - CHANNEL IS RX ONLY\"\r\n"
                 "* RETUNE ID: 17913 FAILED\r\n");
    }
    else
    {
        add(out, "%s", "* COUPLER_STATUS TUNING\r\n* COUPLER_STATUS TUNED\r\n"
                 "* KEYSRC PTT KEYTYPE CW\r\n* KEYSRC NONE KEYTYPE OFF\r\n"
                 "* RETUNE ID: 17769 COMPLETE\r\n");
    }

    prompt(out);
    pushes(out);
}

/* FREQ sets RX and TX, TXFREQ TX alone; the ack is the line as typed */
static void set_freq(char *out, const char *cmd, const char *arg, int tx_only)
{
    static const char *const words[] = { "UP", "+", "INC", NULL };
    long hz;

    if (parse_mhz(arg, &hz) && hz >= MIN_HZ && hz <= MAX_HZ)
    {
        st.cur->tx_hz = hz;

        if (!tx_only)
        {
            st.cur->rx_hz = hz;
        }

        chan_write(out, cmd, cmd);
    }
    else if (tx_only)
    {
        unsim(cmd, NO_ARG);
    }
    else if (parse_mhz(arg, &hz) || strcmp(arg, "+1") == 0)
    {
        error(out, cmd, ERR7001);
    }
    else if (one_of(arg, words))
    {
        error(out, cmd, ERR7015);
    }
    else
    {
        unsim(cmd, NO_ARG);
    }
}

/* MODE AM is acknowledged as MODE AME */
static void set_mode(char *out, const char *cmd, const char *arg)
{
    static const char *const modes[] = { "USB", "LSB", "CW", "AME", "FM", NULL };
    char ack[32];

    if (strcmp(arg, "AM") == 0)
    {
        arg = "AME";
    }

    if (one_of(arg, modes))
    {
        strcpy(st.cur->mode, arg);

        if (!width_ok(st.cur->mode, st.cur->width))
        {
            st.cur->width = widest(st.cur->mode);
        }

        snprintf(ack, sizeof(ack), "MODE %s", st.cur->mode);
        chan_write(out, cmd, ack);
    }
    else if (strcmp(arg, "KEY") == 0)
    {
        error(out, cmd, ERR7015);
    }
    else
    {
        unsim(cmd, NO_ARG);
    }
}

/* BANDWIDTH: each request captured in every mode, and the width it is
 * read as (5000 as 500); in LSB also 100 (read as 1000) and 2500 (read as
 * 25000).  The ack is the width read, padded; the channel takes it if the
 * mode's table holds it, else the mode's widest. */
static void set_bandwidth(char *out, const char *cmd, const char *arg)
{
    static const struct
    {
        const char *req;
        int read;
        int lsb_only;
    } reads[] =
    {
        { "350", 350, 0 }, { "500", 500, 0 }, { "1000", 1000, 0 }, { "1500", 1500, 0 },
        { "2000", 2000, 0 }, { "2400", 2400, 0 }, { "2700", 2700, 0 },
        { "3000", 3000, 0 }, { "4000", 4000, 0 }, { "5000", 500, 0 },
        { "6000", 6000, 0 }, { "25000", 25000, 0 }, { "100", 1000, 1 },
        { "2500", 25000, 1 },
    };
    char ack[32], v[16];
    size_t i;

    for (i = 0; i < sizeof(reads) / sizeof(reads[0]); i++)
    {
        if (strcmp(arg, reads[i].req) == 0
                && (!reads[i].lsb_only || strcmp(st.cur->mode, "LSB") == 0))
        {
            st.cur->width = width_ok(st.cur->mode, reads[i].read) ? reads[i].read :
                            widest(st.cur->mode);
            width_text(reads[i].read, v, sizeof(v));
            snprintf(ack, sizeof(ack), "BANDWIDTH %s", v);
            chan_write(out, cmd, ack);
            return;
        }
    }

    if (strcmp(arg, "BOGUS") == 0)
    {
        error(out, cmd, ERR7015);
        return;
    }

    unsim(cmd, NO_ARG);
}

static void set_agc(char *out, const char *cmd, const char *arg)
{
    static const char *const agcs[] = { "OFF", "SLOW", "MED", "FAST", "DATA", "AUTO", NULL };

    if (one_of(arg, agcs))
    {
        strcpy(st.cur->agc, arg);
        chan_write(out, cmd, cmd);
    }
    else if (strcmp(arg, "BOGUS") == 0)
    {
        error(out, cmd, ERR7015);
    }
    else
    {
        unsim(cmd, NO_ARG);
    }
}

/* POWER: the reduction push only when the level changes */
static void set_power(char *out, const char *cmd, const char *arg)
{
    static const char *const levels[] = { "LOW", "MED", "HIGH", NULL };
    int changed;

    if (strcmp(arg, "BOGUS") == 0)
    {
        error(out, cmd, ERR7015);
        return;
    }

    if (!one_of(arg, levels))
    {
        unsim(cmd, NO_ARG);
        return;
    }

    changed = strcmp(st.power, arg) != 0;
    strcpy(st.power, arg);
    echo(out, cmd);
    pushes(out);

    if (changed)
    {
        add(out, "* POWER REDUCTION %s DB\r\n", reduction());
    }

    add(out, "POWER %s\r\n", st.power);
    prompt(out);
}

/* a whole number as the radio was sent it: no sign, no leading zero */
static int plain_number(const char *s)
{
    const char *p;

    if (s[0] == '\0' || (s[0] == '0' && s[1] != '\0'))
    {
        return 0;
    }

    for (p = s; *p; p++)
    {
        if (*p < '0' || *p > '9')
        {
            return 0;
        }
    }

    return 1;
}

/* RADIO SQUELCH, RADIO SQ_LEVEL, RADIO RFGAIN; 1 when cmd was one of them */
static int set_radio(char *out, const char *cmd)
{
    static const char *const onoff[] = { "ON", "OFF", NULL };
    static const char *const levels[] = { "LOW", "MED", "HIGH", NULL };
    const char *arg;

    if (strncmp(cmd, "RADIO SQUELCH ", 14) == 0)
    {
        arg = cmd + 14;

        if (one_of(arg, onoff))
        {
            st.squelch = strcmp(arg, "ON") == 0;
            radio_write(out, cmd);
            return 1;
        }
    }
    else if (strncmp(cmd, "RADIO SQ_LEVEL ", 15) == 0)
    {
        arg = cmd + 15;

        if (one_of(arg, levels))
        {
            strcpy(st.sq_level, arg);
            radio_write(out, cmd);
            return 1;
        }
    }
    else if (strncmp(cmd, "RADIO RFGAIN ", 13) == 0)
    {
        arg = cmd + 13;

        if (plain_number(arg) && strlen(arg) <= 3 && atoi(arg) <= 100)
        {
            st.rfgain = atoi(arg);
            radio_write(out, cmd);
            return 1;
        }

        if (plain_number(arg))
        {
            error(out, cmd, ERR7001);
            return 1;
        }
    }
    else
    {
        return 0;
    }

    if (strcmp(arg, "BOGUS") == 0)
    {
        error(out, cmd, ERR7015);
    }
    else
    {
        unsim(cmd, NO_ARG);
    }

    return 1;
}

/* The lines answered at SSB> beyond the reads and the SSB word. */
static void ssb_line(char *out, const char *cmd)
{
    static const char *const unknown[] = { "KEY", "PTT", "TX", "KEY ON", "UP", "DOWN", NULL };

    if (strcmp(cmd, "PRESET ACTIVE CHAN 0") == 0)
    {
        st.cur = &st.chans[0];
        chan_write(out, cmd, cmd);
    }
    else if (strncmp(cmd, "FREQ ", 5) == 0)
    {
        set_freq(out, cmd, cmd + 5, 0);
    }
    else if (strncmp(cmd, "TXFREQ ", 7) == 0)
    {
        set_freq(out, cmd, cmd + 7, 1);
    }
    else if (strncmp(cmd, "MODE ", 5) == 0)
    {
        set_mode(out, cmd, cmd + 5);
    }
    else if (strncmp(cmd, "BANDWIDTH ", 10) == 0)
    {
        set_bandwidth(out, cmd, cmd + 10);
    }
    else if (strncmp(cmd, "AGC ", 4) == 0)
    {
        set_agc(out, cmd, cmd + 4);
    }
    else if (strncmp(cmd, "POWER ", 6) == 0)
    {
        set_power(out, cmd, cmd + 6);
    }
    else if (set_radio(out, cmd))
    {
        return;
    }
    else if (strcmp(cmd, "RETUNE") == 0)
    {
        retune(out);
    }
    else if (one_of(cmd, unknown))
    {
        /* words the radio does not know */
        error(out, cmd, ERR7015);
    }
    else
    {
        unsim(cmd, NO_REPLAY);
    }
}

/* A PRC-138 answers in its own shape: CR, prompt. */
static void prc138(char *out, const char *cmd)
{
    if (cmd[0] == '\0')
    {
        add(out, "%s", "\r\n\r\n\rSSB> ");
    }
    else if (strcmp(cmd, "IDENTITY") == 0)
    {
        add(out, "%s", "IDENTITY\r\nPRC-138\r\n\r\n\rSSB> ");
    }
    else
    {
        unsim(cmd, NO_REPLAY);
    }
}

static void answer(const char *cmd, char *out)
{
    out[0] = '\0';

    if (is("prc138"))
    {
        prc138(out, cmd);
        return;
    }

    if (cmd[0] == '\0')
    {
        /* with the echo on, the first CR LF is the empty line's echo */
        if (st.echo)
        {
            add(out, "%s", "\r\n");
        }

        prompt(out);
        return;
    }

    if (strcmp(cmd, "IDENTITY") == 0)
    {
        echo(out, cmd);
        add(out, "%s", "IDENTITY RF-5800H\r\n");
        prompt(out);
        return;
    }

    if (is("busy") && strcmp(cmd, "SSB") == 0)
    {
        /* the radio's self-test is running */
        error(out, cmd, ERR7021);
        return;
    }

    if (is("busy") || is("echo-off"))
    {
        unsim(cmd, NO_REPLAY);
        return;
    }

    if (strcmp(cmd, "CHAN") == 0)
    {
        if (at("HOP>"))
        {
            error(out, cmd, ERR7019);
        }
        else
        {
            read_chan(out);
        }
    }
    else if (strcmp(cmd, "RADIO") == 0)
    {
        read_radio(out);
    }
    else if (strcmp(cmd, "POWER") == 0)
    {
        echo(out, cmd);
        add(out, "POWER %s\r\n", st.power);
        prompt(out);
    }
    else if (strcmp(cmd, "PRESET") == 0 && (at("SSB>") || at("PRGM>")))
    {
        read_preset(out);
    }
    else if (strcmp(cmd, "SSB") == 0 && !at("PRGM>"))
    {
        ssb_word(out);
    }
    else if (at("SSB>"))
    {
        ssb_line(out, cmd);
    }
    else
    {
        unsim(cmd, NO_REPLAY);
    }
}

static void init_state(const char *scenario)
{
    static const struct chan chan0 = { 0, 7231000, 7231000, "LSB", "MED", 3000, 0 };
    static const struct chan chan25 = { 25, 21096000, 21096000, "USB", "MED", 3000, 0 };

    memset(&st, 0, sizeof(st));
    st.scenario = scenario;
    st.echo = !is("echo-off");
    st.chans[0] = chan0;
    st.chans[0].rxonly = is("rxonly");
    st.chans[1] = chan25;
    strcpy(st.power, "LOW");
    strcpy(st.sq_level, "LOW");

    if (is("ale") || is("ale-call") || is("busy"))
    {
        strcpy(st.prompt, "ALE>");
    }
    else if (is("hop"))
    {
        strcpy(st.prompt, "HOP>");
    }
    else if (is("prgm"))
    {
        strcpy(st.prompt, "PRGM>");
    }
    else
    {
        strcpy(st.prompt, "SSB>");
    }

    /* channel 0 was current in the captures the SSB word's answers from
     * ALE> and HOP> are replayed from */
    st.cur = at("ALE>") || at("HOP>") ? &st.chans[0] : &st.chans[1];
}

/* The line assembly, for serve() and --selftest alike: a CR ends a line and
 * a LF is ignored (captured: CR LF is answered as one line).  A line is
 * kept whole or not at all: one longer than the simulator holds, or one
 * with a byte that is not printable ASCII, is never cut into another. */
struct framer
{
    char line[LINE_MAX_LEN];
    size_t n;           /* bytes held */
    size_t len;         /* bytes the line had, held or not */
    int bad;            /* a byte that is not printable ASCII */
    size_t done_len;    /* the same three for the line just ended */
    int done_bad;
    size_t done_n;
};

/* takes one byte; 1 when it ended a line, which is then in f->line */
static int frame(struct framer *f, char c)
{
    if (c == '\n')
    {
        return 0;
    }

    if (c != '\r')
    {
        if (f->n < sizeof(f->line) - 1)
        {
            f->line[f->n++] = c;
        }

        f->len++;
        f->bad |= c < 0x20 || c > 0x7e;
        return 0;
    }

    f->line[f->n] = '\0';
    f->done_n = f->n;
    f->done_len = f->len;
    f->done_bad = f->bad;
    f->n = 0;
    f->len = 0;
    f->bad = 0;
    return 1;
}

/* the line just ended, shown safely: other bytes as \xNN, and the length
 * when it was too long to hold */
static void render(const struct framer *f, char *out, size_t size)
{
    size_t i, k = 0;

    out[0] = '\0';

    for (i = 0; i < f->done_n && k + 5 < size; i++)
    {
        unsigned char b = (unsigned char) f->line[i];

        k += snprintf(out + k, size - k, b >= 0x20 && b <= 0x7e ? "%c" : "\\x%02X",
                      b);
    }

    if (f->done_len > f->done_n)
    {
        snprintf(out + k, size - k, "... (%lu bytes)", (unsigned long) f->done_len);
    }
}

/* Answers the line just ended: only a line held whole reaches answer() */
static void take(const struct framer *f, char *out)
{
    char shown[RENDER_MAX];

    out[0] = '\0';

    if (f->done_len < sizeof(f->line) && !f->done_bad)
    {
        answer(f->line, out);
        return;
    }

    render(f, shown, sizeof(shown));
    unsim(shown, f->done_bad ? NOT_ASCII : TOO_LONG);
}

/* Serves one connection. */
static void serve(void)
{
    struct framer f;
    char out[REPLY_MAX];
    char shown[RENDER_MAX];
    char c;

    memset(&f, 0, sizeof(f));

    while (link_read(&c) == 0)
    {
        if (!frame(&f, c))
        {
            continue;
        }

        render(&f, shown, sizeof(shown));
        printf("Cmd:%s\n", shown);
        take(&f, out);
        link_write(out);
        fflush(stdout);
    }
}

/* --selftest: answer() and the line assembly driven directly, no link */
static int st_fail;
static char got[REPLY_MAX];
static char want[REPLY_MAX];

/* a reply with its CR and LF shown */
static void show(const char *tag, const char *s)
{
    fprintf(stderr, "    %s '", tag);

    for (; *s; s++)
    {
        if (*s == '\r')
        {
            fputs("\\r", stderr);
        }
        else if (*s == '\n')
        {
            fputs("\\n", stderr);
        }
        else
        {
            fputc(*s, stderr);
        }
    }

    fputs("'\n", stderr);
}

static void expect(const char *what, const char *have, const char *wanted)
{
    if (strcmp(have, wanted) != 0)
    {
        st_fail++;
        fprintf(stderr, "FAIL %s\n", what);
        show("got ", have);
        show("want", wanted);
    }
}

static void expect_true(const char *what, int ok)
{
    if (!ok)
    {
        st_fail++;
        fprintf(stderr, "FAIL %s\n", what);
    }
}

/* one line through answer(), as serve() would pass it */
static const char *ask(const char *line)
{
    sent[0] = '\0';
    answer(line, got);
    return got;
}

/* a line with no captured answer: the call recorded, nothing sent, no state
 * changed */
static void none(const char *what, const char *line)
{
    static unsigned char before[sizeof(st)];
    int calls = unsim_calls;

    memcpy(before, &st, sizeof(st));
    ask(line);

    if (unsim_calls != calls + 1 || strcmp(unsim_line, line) != 0
            || got[0] != '\0' || sent[0] != '\0'
            || memcmp(before, &st, sizeof(st)) != 0)
    {
        st_fail++;
        fprintf(stderr, "FAIL %s: not simulated wanted (recorded %d, line '%s', "
                        "state %s)\n", what, unsim_calls - calls,
                unsim_calls != calls ? unsim_line : "",
                memcmp(before, &st, sizeof(st)) == 0 ? "kept" : "CHANGED");
        show("sent", sent);
        show("got ", got);
    }
}

/* bytes through the line assembly; every completed line is answered and
 * the answers joined; returns the number of lines */
static int feed_n(const char *bytes, size_t len, char *all)
{
    struct framer f;
    int lines = 0;
    size_t i;

    memset(&f, 0, sizeof(f));
    all[0] = '\0';

    for (i = 0; i < len; i++)
    {
        if (frame(&f, bytes[i]))
        {
            sent[0] = '\0';
            take(&f, got);
            strncat(all, got, REPLY_MAX - strlen(all) - 1);
            lines++;
        }
    }

    return lines;
}

static int feed(const char *bytes, char *all)
{
    return feed_n(bytes, strlen(bytes), all);
}

/* bytes through the line assembly that no capture holds as a whole: one
 * not-simulated call naming them as rendered, nothing sent, no state
 * changed */
static void none_bytes(const char *what, const char *bytes, size_t len,
                       const char *rendered)
{
    static unsigned char before[sizeof(st)];
    static char all[REPLY_MAX];
    int calls = unsim_calls;

    memcpy(before, &st, sizeof(st));
    sent[0] = '\0';
    feed_n(bytes, len, all);

    if (unsim_calls != calls + 1 || strcmp(unsim_line, rendered) != 0
            || all[0] != '\0' || sent[0] != '\0'
            || memcmp(before, &st, sizeof(st)) != 0)
    {
        st_fail++;
        fprintf(stderr, "FAIL %s: not simulated wanted (recorded %d, state %s)\n",
                what, unsim_calls - calls,
                memcmp(before, &st, sizeof(st)) == 0 ? "kept" : "CHANGED");
        show("line", unsim_calls != calls ? unsim_line : "");
        show("want", rendered);
        show("got ", all);
    }
}

#define E7001 "ERROR: 7001  'BAD VALUE'\r\n\r\n"
#define E7015 "ERROR: 7015  'COMMAND SYNTAX ERROR (TYPE ? FOR HELP)'\r\n\r\n"
#define E7019 "ERROR: 7019  'OPERATION IS NOT VALID IN THE CURRENT MODE'\r\n\r\n"
#define MANUAL "* PRESET CHANGED TO MANUAL\r\n"
#define CH0 "* CURRENT CHAN 0: RX 7.231000  TX 7.231000 LSB\r\n"
#define NVRAM "* RUNTIME_STATUS: 8077 \"NVRAM DATABASE CORRECTED\"\r\n"
#define BURST_TAIL(db) "* STATE OPMODE SSB\r\n* POWER REDUCTION " db " DB\r\n" \
    "* RADIO SILENCE OFF\r\nSSB\r\n\r\nSSB> "

static const char *want_chan(int num, const char *freq, const char *mode,
                             const char *agc, const char *bw, const char *rxonly,
                             const char *pr)
{
    snprintf(want, sizeof(want), "CHAN\r\nCHAN CURRENT NUMBER %d\r\n"
                                 "CHAN CURRENT RXFREQ %s\r\nCHAN CURRENT TXFREQ %s\r\n"
                                 "CHAN CURRENT MODE %s\r\nCHAN CURRENT AGC %s\r\n"
                                 "CHAN CURRENT BANDWIDTH %s\r\nCHAN CURRENT RXONLY %s\r\n"
                                 "CHAN CURRENT HAIL DISABLED\r\nCHAN CURRENT HAILKEY 0\r\n"
                                 "CHAN CURRENT MAXPOWER NONE\r\nCHAN CURRENT SSBSCAN NO\r\n"
                                 "END-OF-LIST\r\n\r\n%s ", num, freq, freq, mode, agc, bw, rxonly, pr);
    return want;
}

static const char *want_preset(int chan, const char *pr)
{
    snprintf(want, sizeof(want), "PRESET\r\nPRESET CURRENT NAME MANUAL\r\n"
                                 "PRESET CURRENT PRENUMBER 0\r\nPRESET CURRENT SSB\r\n"
                                 "PRESET CURRENT NET 1\r\nPRESET CURRENT MODEM OFF\r\n"
                                 "PRESET CURRENT MDMNUMBER 0\r\nPRESET CURRENT CHAN %d\r\n"
                                 "PRESET CURRENT KEY (NONE)\r\nPRESET CURRENT PT CLR\r\n"
                                 "PRESET CURRENT CT 2400\r\nPRESET CURRENT ADDRESS (NONE)\r\n"
                                 "PRESET CURRENT CRYPTOTYPE NONE\r\nPRESET CURRENT ENABLE\r\n"
                                 "END-OF-LIST\r\n\r\n%s ", chan, pr);
    return want;
}

static const char *want_radio(const char *coupler, const char *txgain,
                              const char *sq_level, const char *rfgain,
                              const char *squelch, const char *pr)
{
    snprintf(want, sizeof(want), "RADIO\r\nRADIO NAME \r\nRADIO COUPLER %s\r\n"
                                 "RADIO HIGHQ_ANTENNA NOT_PRESENT\r\nRADIO TXGAIN %s\r\n"
                                 "RADIO SQ_TYPE SYLLABIC\r\nRADIO FMSQUELCH TONE\r\n"
                                 "RADIO SQ_LEVEL %s\r\nRADIO RXNOISEBLANK ON\r\n"
                                 "RADIO FMDEVIATION 8.0\r\nRADIO COMPRESSION ON\r\n"
                                 "RADIO CWOFFSET 1000\r\nRADIO BFO 0\r\nRADIO SILENCE OFF\r\n"
                                 "RADIO SLFAD (NONE) ADDRESS 0\r\nRADIO 20W_COUPLER MEMORY\r\n"
                                 "RADIO RFGAIN %s\r\nRADIO SQUELCH %s\r\nRADIO TYPE RF-5800H\r\n"
                                 "RADIO BASEBAND OFF\r\nEND-OF-LIST\r\n\r\n%s ", coupler, txgain,
             sq_level, rfgain, squelch, pr);
    return want;
}

/* a channel write's answer on channel 0 at SSB> */
static const char *want_write(const char *cmd, const char *push,
                              const char *ack)
{
    snprintf(want, sizeof(want), "%s\r\n" MANUAL "%s\r\n%s\r\n\r\nSSB> ", cmd,
             push, ack);
    return want;
}

static const char *want_error(const char *cmd, const char *err, const char *pr)
{
    snprintf(want, sizeof(want), "%s\r\n%s%s ", cmd, err, pr);
    return want;
}

/* ssb, then the open's PRESET ACTIVE CHAN 0: channel 0 current */
static void on_chan0(const char *scenario)
{
    init_state(scenario);
    ask("PRESET ACTIVE CHAN 0");
}

static void check_reads(void)
{
    static const struct
    {
        const char *scenario, *pr;
    } at[] =
    {
        { "ssb", "SSB>" }, { "ale", "ALE>" }, { "hop", "HOP>" }, { "prgm", "PRGM>" },
        { "ale-call", "ALE>" },
    };
    char name[96], w[REPLY_MAX];
    size_t i;

    for (i = 0; i < sizeof(at) / sizeof(at[0]); i++)
    {
        init_state(at[i].scenario);
        snprintf(name, sizeof(name), "5-1 empty line at %s (%s)", at[i].pr,
                 at[i].scenario);
        snprintf(w, sizeof(w), "\r\n\r\n%s ", at[i].pr);
        expect(name, ask(""), w);
        snprintf(name, sizeof(name), "5-2 IDENTITY at %s (%s)", at[i].pr,
                 at[i].scenario);
        snprintf(w, sizeof(w), "IDENTITY\r\nIDENTITY RF-5800H\r\n\r\n%s ", at[i].pr);
        expect(name, ask("IDENTITY"), w);
        snprintf(name, sizeof(name), "5-6 POWER read at %s (%s)", at[i].pr,
                 at[i].scenario);
        snprintf(w, sizeof(w), "POWER\r\nPOWER LOW\r\n\r\n%s ", at[i].pr);
        expect(name, ask("POWER"), w);
        snprintf(name, sizeof(name), "5-5 RADIO at %s (%s), the start values",
                 at[i].pr, at[i].scenario);
        expect(name, ask("RADIO"), want_radio("ENABLE", "LOW", "LOW", "0", "OFF",
                                              at[i].pr));
    }

    /* 5-3: CHAN at SSB>, ALE>, PRGM>; 7019 at HOP> */
    init_state("ssb");
    expect("5-3 CHAN at SSB>, channel 25 at the start", ask("CHAN"),
           want_chan(25, "21.096000", "USB", "MED", "3000", "NO", "SSB>"));
    init_state("prgm");
    expect("5-3 CHAN at PRGM>, channel 25 at the start", ask("CHAN"),
           want_chan(25, "21.096000", "USB", "MED", "3000", "NO", "PRGM>"));
    init_state("ale");
    expect("5-3 CHAN at ALE>, channel 0 current, AGC MED", ask("CHAN"),
           want_chan(0, "7.231000", "LSB", "MED", "3000", "NO", "ALE>"));
    init_state("hop");
    expect("5-3 CHAN at HOP>: 7019", ask("CHAN"), want_error("CHAN", E7019,
            "HOP>"));
    on_chan0("ssb");
    expect("5-3 channel 0 at the start: 7.231 LSB, AGC MED, 3000", ask("CHAN"),
           want_chan(0, "7.231000", "LSB", "MED", "3000", "NO", "SSB>"));
    ask("MODE FM");
    expect("5-3 FM reads AGC FAST", ask("CHAN"),
           want_chan(0, "7.231000", "FM", "FAST", "25000", "NO", "SSB>"));
    ask("AGC SLOW");
    expect("5-3 an AGC written in FM still reads FAST in FM", ask("CHAN"),
           want_chan(0, "7.231000", "FM", "FAST", "25000", "NO", "SSB>"));
    ask("MODE USB");
    expect("5-3 an AGC written in FM shows after FM", ask("CHAN"),
           want_chan(0, "7.231000", "USB", "SLOW", "3000", "NO", "SSB>"));
    on_chan0("rxonly");
    expect("5-3 rxonly: CHAN reads RXONLY YES", ask("CHAN"),
           want_chan(0, "7.231000", "LSB", "MED", "3000", "YES", "SSB>"));

    /* 5-4: PRESET at SSB> and PRGM>; not simulated at ALE>, HOP> */
    init_state("ssb");
    expect("5-4 PRESET at SSB>, channel 25", ask("PRESET"), want_preset(25,
            "SSB>"));
    ask("PRESET ACTIVE CHAN 0");
    expect("5-4 PRESET at SSB>, channel 0 after the select", ask("PRESET"),
           want_preset(0, "SSB>"));
    init_state("prgm");
    expect("5-4 PRESET at PRGM>", ask("PRESET"), want_preset(25, "PRGM>"));
    init_state("ale");
    none("5-4 PRESET at ALE>: not simulated", "PRESET");
    init_state("hop");
    none("5-4 PRESET at HOP>: not simulated", "PRESET");

    /* 5-5: TXGAIN reads the POWER word; COUPLER BYPASS in bypassed */
    on_chan0("ssb");
    ask("POWER MED");
    expect("5-5 RADIO TXGAIN reads MED after POWER MED", ask("RADIO"),
           want_radio("ENABLE", "MED", "LOW", "0", "OFF", "SSB>"));
    ask("POWER HIGH");
    expect("5-5 RADIO TXGAIN reads HIGH after POWER HIGH", ask("RADIO"),
           want_radio("ENABLE", "HIGH", "LOW", "0", "OFF", "SSB>"));
    init_state("bypassed");
    expect("5-5 bypassed: RADIO COUPLER BYPASS", ask("RADIO"),
           want_radio("BYPASS", "LOW", "LOW", "0", "OFF", "SSB>"));
}

static void check_ssb_word(void)
{
    static unsigned char before[sizeof(st)];

    init_state("ale");
    expect("5-7 SSB at ALE>: the coupler push first, the two pushes after the "
           "prompt", ask("SSB"), "SSB\r\n* COUPLER_STATUS UNBYPASSED\r\n" MANUAL CH0
           BURST_TAIL("13.0") MANUAL CH0);
    expect("5-7 after SSB the prompt is SSB>", ask(""), "\r\n\r\nSSB> ");

    init_state("hop");
    expect("5-8 SSB at HOP>: MDMSTAT, the coupler push, nothing after the prompt",
           ask("SSB"), "SSB\r\n* MDMSTAT ID:1 OFF\r\n* COUPLER_STATUS UNBYPASSED\r\n"
           MANUAL CH0 BURST_TAIL("13.0"));
    expect("5-8 after SSB the prompt is SSB>", ask(""), "\r\n\r\nSSB> ");

    on_chan0("ssb");
    memcpy(before, &st, sizeof(st));
    expect("5-9 SSB at SSB>: the bare burst", ask("SSB"),
           "SSB\r\n" MANUAL CH0 BURST_TAIL("13.0"));
    expect_true("5-9 SSB at SSB>: nothing changes",
                memcmp(before, &st, sizeof(st)) == 0);
    ask("POWER MED");
    expect("5-9 the burst's reduction figure at POWER MED", ask("SSB"),
           "SSB\r\n" MANUAL CH0 BURST_TAIL("6.0"));
    ask("POWER HIGH");
    expect("5-9 the burst's reduction figure at POWER HIGH", ask("SSB"),
           "SSB\r\n" MANUAL CH0 BURST_TAIL("0.0"));

    init_state("busy");
    expect("5-10 busy: SSB refused 7021 at ALE>", ask("SSB"),
           "SSB\r\nERROR: 7021  'SYSTEM BUSY - COMMAND IN PROGRESS'\r\n\r\nALE> ");
    expect("5-10 busy: the prompt stays ALE>", ask(""), "\r\n\r\nALE> ");

    init_state("ale-call");
    paused_ms = 0;
    sent_at_pause = 0;
    ask("SSB");
    expect("5-11 ale-call: the abort sent first", sent,
           "SSB\r\n* KEYSRC ALE KEYTYPE OFF\r\n* ALECALL FAILED CALL ABORTED\r\n"
           "\r\nALE> ");
    expect_true("5-11 ale-call: the abort sent before a 150 ms wait",
                paused_ms == 150 && sent_at_pause == strlen(sent));
    expect("5-11 ale-call: then the burst, the reduction figure by POWER", got,
           MANUAL CH0 BURST_TAIL("13.0") MANUAL CH0);
    sent[0] = '\0';
    paused_ms = 0;
    expect("5-9 ale-call after the call: SSB at SSB> is the bare burst",
           ask("SSB"), "SSB\r\n" MANUAL CH0 BURST_TAIL("13.0"));
    expect_true("5-9 ale-call after the call: nothing sent ahead, no wait",
                sent[0] == '\0' && paused_ms == 0);
}

static void check_writes(void)
{
    static const char *const agcs[] = { "OFF", "SLOW", "MED", "FAST", "DATA", "AUTO" };
    char name[96], cmd[32];
    size_t i;

    init_state("ssb");
    expect("5-12 PRESET ACTIVE CHAN 0 from channel 25", ask("PRESET ACTIVE CHAN 0"),
           "PRESET ACTIVE CHAN 0\r\n" MANUAL CH0 "PRESET ACTIVE CHAN 0\r\n\r\nSSB> ");
    expect_true("5-12 channel 0 is current", st.cur == &st.chans[0]);
    expect("5-12 PRESET ACTIVE CHAN 0 when channel 0 already is",
           ask("PRESET ACTIVE CHAN 0"),
           "PRESET ACTIVE CHAN 0\r\n" MANUAL CH0 "PRESET ACTIVE CHAN 0\r\n\r\nSSB> ");

    /* 5-13 */
    on_chan0("ssb");
    expect("5-13 FREQ sets RX and TX, acked as typed", ask("FREQ 14.25"),
           want_write("FREQ 14.25", "* CURRENT CHAN 0: RX 14.250000  TX 14.250000 LSB",
                      "FREQ 14.25"));
    expect("5-13 TXFREQ sets TX alone", ask("TXFREQ 14.3"),
           want_write("TXFREQ 14.3",
                      "* CURRENT CHAN 0: RX 14.250000  TX 14.300000 LSB", "TXFREQ 14.3"));
    expect("5-13 FREQ 07.231 acked as typed", ask("FREQ 07.231"),
           want_write("FREQ 07.231", "* CURRENT CHAN 0: RX 7.231000  TX 7.231000 LSB",
                      "FREQ 07.231"));
    expect("5-13 FREQ 7.2310005 rounds to 7.231001", ask("FREQ 7.2310005"),
           want_write("FREQ 7.2310005", "* CURRENT CHAN 0: RX 7.231001  TX 7.231001 LSB",
                      "FREQ 7.2310005"));
    expect("5-13 FREQ 7.23100049 rounds to 7.231000", ask("FREQ 7.23100049"),
           want_write("FREQ 7.23100049",
                      "* CURRENT CHAN 0: RX 7.231000  TX 7.231000 LSB", "FREQ 7.23100049"));
    expect("5-13 FREQ 1.6 accepted", ask("FREQ 1.6"),
           want_write("FREQ 1.6", "* CURRENT CHAN 0: RX 1.600000  TX 1.600000 LSB",
                      "FREQ 1.6"));
    expect("5-13 FREQ 59.999999 accepted", ask("FREQ 59.999999"),
           want_write("FREQ 59.999999",
                      "* CURRENT CHAN 0: RX 59.999999  TX 59.999999 LSB", "FREQ 59.999999"));
    expect("5-13 FREQ 1.59999: 7001", ask("FREQ 1.59999"),
           want_error("FREQ 1.59999", E7001, "SSB>"));
    expect("5-13 FREQ 60: 7001", ask("FREQ 60"), want_error("FREQ 60", E7001,
            "SSB>"));
    expect("5-13 FREQ 7231: 7001", ask("FREQ 7231"), want_error("FREQ 7231", E7001,
            "SSB>"));
    expect("5-13 FREQ 4297: 7001 (no wrap of a 32-bit Hz)", ask("FREQ 4297"),
           want_error("FREQ 4297", E7001, "SSB>"));
    expect("5-13 FREQ UP: 7015", ask("FREQ UP"), want_error("FREQ UP", E7015,
            "SSB>"));
    expect("5-13 FREQ +: 7015", ask("FREQ +"), want_error("FREQ +", E7015, "SSB>"));
    expect("5-13 FREQ INC: 7015", ask("FREQ INC"), want_error("FREQ INC", E7015,
            "SSB>"));
    expect("5-13 FREQ +1: 7001", ask("FREQ +1"), want_error("FREQ +1", E7001,
            "SSB>"));
    /* at most one leading zero, as the captured 07.231 */
    none("5-13 FREQ 007.231 (two leading zeros): not simulated", "FREQ 007.231");
    none("5-13 FREQ 0007.231 (three leading zeros): not simulated",
         "FREQ 0007.231");
    none("5-13 FREQ 00.5 (two leading zeros): not simulated", "FREQ 00.5");
    none("5-13 TXFREQ 007.231 (two leading zeros): not simulated",
         "TXFREQ 007.231");
    expect("5-13 FREQ 07.231 (one leading zero) still answered", ask("FREQ 07.231"),
           want_write("FREQ 07.231", "* CURRENT CHAN 0: RX 7.231000  TX 7.231000 LSB",
                      "FREQ 07.231"));
    none("5-13 FREQ with another argument: not simulated", "FREQ 1e1");
    none("5-13 FREQ 7,231: not simulated", "FREQ 7,231");
    none("5-13 a bare FREQ: not simulated", "FREQ");
    none("5-13 TXFREQ UP: not simulated", "TXFREQ UP");
    none("5-13 TXFREQ 60: not simulated", "TXFREQ 60");
    none("5-13 TXFREQ +1: not simulated", "TXFREQ +1");
    none("5-13 TXFREQ INC: not simulated", "TXFREQ INC");

    /* 5-14 */
    on_chan0("ssb");
    expect("5-14 MODE USB acked", ask("MODE USB"),
           want_write("MODE USB", "* CURRENT CHAN 0: RX 7.231000  TX 7.231000 USB",
                      "MODE USB"));
    expect("5-14 MODE AM acked MODE AME", ask("MODE AM"),
           want_write("MODE AM", "* CURRENT CHAN 0: RX 7.231000  TX 7.231000 AME",
                      "MODE AME"));
    expect("5-14 a foreign width becomes the mode's widest (USB 3000 to AME 4000)",
           ask("CHAN"), want_chan(0, "7.231000", "AME", "MED", "4000", "NO", "SSB>"));
    ask("MODE CW");
    expect("5-14 AME 4000 to CW: 1500", ask("CHAN"),
           want_chan(0, "7.231000", "CW", "MED", "1500", "NO", "SSB>"));
    ask("MODE USB");
    ask("BANDWIDTH 2400");
    ask("MODE LSB");
    expect("5-14 a width the new table holds is kept (USB 2400 to LSB)",
           ask("CHAN"),
           want_chan(0, "7.231000", "LSB", "MED", "2400", "NO", "SSB>"));
    expect("5-14 MODE KEY: 7015", ask("MODE KEY"), want_error("MODE KEY", E7015,
            "SSB>"));
    none("5-14 MODE XYZ: not simulated", "MODE XYZ");
    none("5-14 MODE usb: not simulated", "MODE usb");

    /* 5-15 */
    on_chan0("ssb");
    ask("MODE USB");
    expect("5-15 BANDWIDTH 2400 in USB", ask("BANDWIDTH 2400"),
           want_write("BANDWIDTH 2400", "* CURRENT CHAN 0: RX 7.231000  TX 7.231000 USB",
                      "BANDWIDTH 2400"));
    ask("BANDWIDTH 4000");
    expect("5-15 an out-of-table width with 2400 held reads the widest",
           ask("CHAN"),
           want_chan(0, "7.231000", "USB", "MED", "3000", "NO", "SSB>"));
    expect("5-15 BANDWIDTH 350 acked padded", ask("BANDWIDTH 350"),
           want_write("BANDWIDTH 350", "* CURRENT CHAN 0: RX 7.231000  TX 7.231000 USB",
                      "BANDWIDTH 0350"));
    ask("MODE CW");
    expect("5-15 BANDWIDTH 5000 acked 0500", ask("BANDWIDTH 5000"),
           want_write("BANDWIDTH 5000", "* CURRENT CHAN 0: RX 7.231000  TX 7.231000 CW",
                      "BANDWIDTH 0500"));
    expect("5-15 CW takes 5000 as 500", ask("CHAN"),
           want_chan(0, "7.231000", "CW", "MED", "0500", "NO", "SSB>"));
    ask("BANDWIDTH 6000");
    expect("5-15 CW 6000 from a held 500 reads 1500", ask("CHAN"),
           want_chan(0, "7.231000", "CW", "MED", "1500", "NO", "SSB>"));
    ask("MODE LSB");
    ask("BANDWIDTH 3000");
    expect("5-15 LSB BANDWIDTH 100 acked 1000", ask("BANDWIDTH 100"),
           "BANDWIDTH 100\r\n" MANUAL CH0 "BANDWIDTH 1000\r\n\r\nSSB> ");
    expect("5-15 LSB after 100 reads 3000", ask("CHAN"),
           want_chan(0, "7.231000", "LSB", "MED", "3000", "NO", "SSB>"));
    expect("5-15 LSB BANDWIDTH 2500 acked 25000", ask("BANDWIDTH 2500"),
           "BANDWIDTH 2500\r\n" MANUAL CH0 "BANDWIDTH 25000\r\n\r\nSSB> ");
    expect("5-15 LSB after 2500 reads 3000", ask("CHAN"),
           want_chan(0, "7.231000", "LSB", "MED", "3000", "NO", "SSB>"));
    expect("5-15 BANDWIDTH BOGUS: 7015", ask("BANDWIDTH BOGUS"),
           want_error("BANDWIDTH BOGUS", E7015, "SSB>"));
    none("5-15 BANDWIDTH 1234: not simulated", "BANDWIDTH 1234");
    none("5-15 BANDWIDTH 0350: not simulated", "BANDWIDTH 0350");
    ask("MODE USB");
    none("5-15 BANDWIDTH 100 in USB: not simulated", "BANDWIDTH 100");
    none("5-15 BANDWIDTH 2500 in USB: not simulated", "BANDWIDTH 2500");
    ask("MODE CW");
    none("5-15 BANDWIDTH 100 in CW: not simulated", "BANDWIDTH 100");
    none("5-15 BANDWIDTH 2500 in CW: not simulated", "BANDWIDTH 2500");

    /* 5-16 */
    on_chan0("ssb");

    for (i = 0; i < sizeof(agcs) / sizeof(agcs[0]); i++)
    {
        snprintf(cmd, sizeof(cmd), "AGC %s", agcs[i]);
        snprintf(name, sizeof(name), "5-16 %s acked", cmd);
        ask(cmd);
        snprintf(want, sizeof(want), "%s\r\n" MANUAL CH0 "%s\r\n\r\nSSB> ", cmd, cmd);
        expect(name, got, want);
        snprintf(name, sizeof(name), "5-16 %s read back", cmd);
        expect(name, ask("CHAN"), want_chan(0, "7.231000", "LSB", agcs[i], "3000",
                                            "NO", "SSB>"));
    }

    expect("5-16 AGC BOGUS: 7015", ask("AGC BOGUS"), want_error("AGC BOGUS", E7015,
            "SSB>"));
    none("5-16 AGC SL: not simulated", "AGC SL");
    none("5-16 AGC slow: not simulated", "AGC slow");

    /* 5-17 */
    on_chan0("ssb");
    expect("5-17 POWER MED from LOW: the reduction line", ask("POWER MED"),
           "POWER MED\r\n" MANUAL CH0
           "* POWER REDUCTION 6.0 DB\r\nPOWER MED\r\n\r\nSSB> ");
    expect("5-17 POWER read after the write", ask("POWER"),
           "POWER\r\nPOWER MED\r\n\r\nSSB> ");
    expect("5-17 POWER HIGH from MED: the reduction line", ask("POWER HIGH"),
           "POWER HIGH\r\n" MANUAL CH0
           "* POWER REDUCTION 0.0 DB\r\nPOWER HIGH\r\n\r\nSSB> ");
    expect("5-17 POWER LOW from HIGH: the reduction line", ask("POWER LOW"),
           "POWER LOW\r\n" MANUAL CH0
           "* POWER REDUCTION 13.0 DB\r\nPOWER LOW\r\n\r\nSSB> ");
    expect("5-17 same-value POWER: no reduction line", ask("POWER LOW"),
           "POWER LOW\r\n" MANUAL CH0 "POWER LOW\r\n\r\nSSB> ");
    expect("5-17 POWER BOGUS: 7015", ask("POWER BOGUS"), want_error("POWER BOGUS",
            E7015, "SSB>"));
    none("5-17 POWER HI: not simulated", "POWER HI");

    /* 5-18 */
    on_chan0("ssb");
    expect("5-18 RADIO SQUELCH ON", ask("RADIO SQUELCH ON"),
           "RADIO SQUELCH ON\r\n" NVRAM MANUAL CH0 "RADIO SQUELCH ON\r\n\r\nSSB> ");
    expect("5-18 RADIO SQUELCH OFF", ask("RADIO SQUELCH OFF"),
           "RADIO SQUELCH OFF\r\n" NVRAM MANUAL CH0 "RADIO SQUELCH OFF\r\n\r\nSSB> ");
    expect("5-18 a same-value RADIO SQUELCH OFF: the same shape",
           ask("RADIO SQUELCH OFF"),
           "RADIO SQUELCH OFF\r\n" NVRAM MANUAL CH0 "RADIO SQUELCH OFF\r\n\r\nSSB> ");
    expect("5-18 a same-value RADIO SQ_LEVEL LOW: the same shape",
           ask("RADIO SQ_LEVEL LOW"),
           "RADIO SQ_LEVEL LOW\r\n" NVRAM MANUAL CH0 "RADIO SQ_LEVEL LOW\r\n\r\nSSB> ");
    expect("5-18 RADIO SQ_LEVEL HIGH", ask("RADIO SQ_LEVEL HIGH"),
           "RADIO SQ_LEVEL HIGH\r\n" NVRAM MANUAL CH0 "RADIO SQ_LEVEL HIGH\r\n\r\nSSB> ");
    expect("5-18 RADIO RFGAIN 50", ask("RADIO RFGAIN 50"),
           "RADIO RFGAIN 50\r\n" NVRAM MANUAL CH0 "RADIO RFGAIN 50\r\n\r\nSSB> ");
    ask("RADIO SQUELCH ON");
    expect("5-18 the RADIO read shows the writes", ask("RADIO"),
           want_radio("ENABLE", "LOW", "HIGH", "50", "ON", "SSB>"));
    expect("5-18 RADIO RFGAIN 100", ask("RADIO RFGAIN 100"),
           "RADIO RFGAIN 100\r\n" NVRAM MANUAL CH0 "RADIO RFGAIN 100\r\n\r\nSSB> ");
    expect("5-18 RADIO RFGAIN 101: 7001", ask("RADIO RFGAIN 101"),
           want_error("RADIO RFGAIN 101", E7001, "SSB>"));
    expect("5-18 RADIO RFGAIN 4294967346: 7001 (no wrap)",
           ask("RADIO RFGAIN 4294967346"),
           want_error("RADIO RFGAIN 4294967346", E7001, "SSB>"));
    expect("5-18 RADIO RFGAIN BOGUS: 7015", ask("RADIO RFGAIN BOGUS"),
           want_error("RADIO RFGAIN BOGUS", E7015, "SSB>"));
    expect("5-18 RADIO SQUELCH BOGUS: 7015", ask("RADIO SQUELCH BOGUS"),
           want_error("RADIO SQUELCH BOGUS", E7015, "SSB>"));
    expect("5-18 RADIO SQ_LEVEL BOGUS: 7015", ask("RADIO SQ_LEVEL BOGUS"),
           want_error("RADIO SQ_LEVEL BOGUS", E7015, "SSB>"));
    expect("5-18 the refused writes changed nothing", ask("RADIO"),
           want_radio("ENABLE", "LOW", "HIGH", "100", "ON", "SSB>"));
    none("5-18 RADIO RFGAIN -5: not simulated", "RADIO RFGAIN -5");
    none("5-18 RADIO RFGAIN 050: not simulated", "RADIO RFGAIN 050");
    none("5-18 RADIO SQUELCH on: not simulated", "RADIO SQUELCH on");
    none("5-18 RADIO COUPLER BYPASS: not simulated", "RADIO COUPLER BYPASS");
}

/* for each mode, BANDWIDTH n, the ack and the width CHAN then reads, as
 * captured from the radio */
static const struct
{
    const char *mode, *req, *ack, *read, *agc;
} bw_pairs[] =
{
    { "USB", "350", "0350", "3000", "MED" },
    { "USB", "500", "0500", "3000", "MED" },
    { "USB", "1000", "1000", "3000", "MED" },
    { "USB", "1500", "1500", "3000", "MED" },
    { "USB", "2000", "2000", "2000", "MED" },
    { "USB", "2400", "2400", "2400", "MED" },
    { "USB", "2700", "2700", "2700", "MED" },
    { "USB", "3000", "3000", "3000", "MED" },
    { "USB", "4000", "4000", "3000", "MED" },
    { "USB", "5000", "0500", "3000", "MED" },
    { "USB", "6000", "6000", "3000", "MED" },
    { "USB", "25000", "25000", "3000", "MED" },
    { "LSB", "350", "0350", "3000", "MED" },
    { "LSB", "500", "0500", "3000", "MED" },
    { "LSB", "1000", "1000", "3000", "MED" },
    { "LSB", "1500", "1500", "3000", "MED" },
    { "LSB", "2000", "2000", "2000", "MED" },
    { "LSB", "2400", "2400", "2400", "MED" },
    { "LSB", "2700", "2700", "2700", "MED" },
    { "LSB", "3000", "3000", "3000", "MED" },
    { "LSB", "4000", "4000", "3000", "MED" },
    { "LSB", "5000", "0500", "3000", "MED" },
    { "LSB", "6000", "6000", "3000", "MED" },
    { "LSB", "25000", "25000", "3000", "MED" },
    { "AME", "350", "0350", "4000", "MED" },
    { "AME", "500", "0500", "4000", "MED" },
    { "AME", "1000", "1000", "4000", "MED" },
    { "AME", "1500", "1500", "4000", "MED" },
    { "AME", "2000", "2000", "4000", "MED" },
    { "AME", "2400", "2400", "4000", "MED" },
    { "AME", "2700", "2700", "4000", "MED" },
    { "AME", "3000", "3000", "4000", "MED" },
    { "AME", "4000", "4000", "4000", "MED" },
    { "AME", "5000", "0500", "4000", "MED" },
    { "AME", "6000", "6000", "4000", "MED" },
    { "AME", "25000", "25000", "4000", "MED" },
    { "CW", "350", "0350", "0350", "MED" },
    { "CW", "500", "0500", "0500", "MED" },
    { "CW", "1000", "1000", "1000", "MED" },
    { "CW", "1500", "1500", "1500", "MED" },
    { "CW", "2000", "2000", "1500", "MED" },
    { "CW", "2400", "2400", "1500", "MED" },
    { "CW", "2700", "2700", "1500", "MED" },
    { "CW", "3000", "3000", "1500", "MED" },
    { "CW", "4000", "4000", "1500", "MED" },
    { "CW", "5000", "0500", "0500", "MED" },
    { "CW", "6000", "6000", "1500", "MED" },
    { "CW", "25000", "25000", "1500", "MED" },
    { "FM", "350", "0350", "25000", "FAST" },
    { "FM", "500", "0500", "25000", "FAST" },
    { "FM", "1000", "1000", "25000", "FAST" },
    { "FM", "1500", "1500", "25000", "FAST" },
    { "FM", "2000", "2000", "25000", "FAST" },
    { "FM", "2400", "2400", "25000", "FAST" },
    { "FM", "2700", "2700", "25000", "FAST" },
    { "FM", "3000", "3000", "25000", "FAST" },
    { "FM", "4000", "4000", "25000", "FAST" },
    { "FM", "5000", "0500", "25000", "FAST" },
    { "FM", "6000", "6000", "25000", "FAST" },
    { "FM", "25000", "25000", "25000", "FAST" },
    { "LSB", "100", "1000", "3000", "MED" },
    { "LSB", "2500", "25000", "3000", "MED" },
};

/* the pairs no capture holds: not simulated */
static const char *const bw_unheld[][2] =
{
    { "USB", "100" },
    { "USB", "2500" },
    { "CW", "100" },
    { "CW", "2500" },
    { "AME", "100" },
    { "AME", "2500" },
    { "FM", "100" },
    { "FM", "2500" },
};

/* MODE w from a mode, AGC and width read just before it, and the mode, AGC
 * and width CHAN read just after, as captured from the radio */
static const struct
{
    const char *from, *agc, *width, *word, *to, *to_agc, *to_width;
} mode_pairs[] =
{
    { "LSB", "MED", "3000", "USB", "USB", "MED", "3000" },
    { "USB", "MED", "3000", "LSB", "LSB", "MED", "3000" },
    { "LSB", "MED", "3000", "AME", "AME", "MED", "4000" },
    { "AME", "MED", "4000", "CW", "CW", "MED", "1500" },
    { "CW", "MED", "1500", "FM", "FM", "FAST", "25000" },
    { "LSB", "MED", "3000", "USB", "USB", "MED", "3000" },
    { "USB", "MED", "3000", "LSB", "LSB", "MED", "3000" },
    { "LSB", "MED", "3000", "AME", "AME", "MED", "4000" },
    { "AME", "MED", "4000", "CW", "CW", "MED", "1500" },
    { "CW", "MED", "1500", "FM", "FM", "FAST", "25000" },
    { "FM", "MED", "25000", "AM", "AME", "MED", "4000" },
    { "USB", "MED", "2400", "LSB", "LSB", "MED", "2400" },
    { "LSB", "MED", "3000", "FM", "FM", "FAST", "25000" },
    { "FM", "SLOW", "25000", "USB", "USB", "SLOW", "3000" },
};

/* 5-14 and 5-15, pair by pair as captured: each BANDWIDTH request in each
 * mode, its ack and the width the channel then reads; each MODE change
 * from the width held before it; and the pairs no capture holds, not
 * simulated.  The expected values come from the captures, not from the
 * simulator's tables. */
static void check_matrix(void)
{
    char name[128], cmd[48], push[96], ack[48];
    size_t i;

    for (i = 0; i < sizeof(bw_pairs) / sizeof(bw_pairs[0]); i++)
    {
        on_chan0("ssb");
        strcpy(st.cur->mode, bw_pairs[i].mode);
        snprintf(cmd, sizeof(cmd), "BANDWIDTH %s", bw_pairs[i].req);
        snprintf(push, sizeof(push), "* CURRENT CHAN 0: RX 7.231000  TX 7.231000 %s",
                 bw_pairs[i].mode);
        snprintf(ack, sizeof(ack), "BANDWIDTH %s", bw_pairs[i].ack);
        snprintf(name, sizeof(name), "5-15 matrix %s %s: acked %s",
                 bw_pairs[i].mode, cmd, bw_pairs[i].ack);
        expect(name, ask(cmd), want_write(cmd, push, ack));
        snprintf(name, sizeof(name), "5-15 matrix %s %s: reads %s",
                 bw_pairs[i].mode, cmd, bw_pairs[i].read);
        expect(name, ask("CHAN"), want_chan(0, "7.231000", bw_pairs[i].mode,
                                            bw_pairs[i].agc, bw_pairs[i].read, "NO", "SSB>"));
    }

    for (i = 0; i < sizeof(bw_unheld) / sizeof(bw_unheld[0]); i++)
    {
        on_chan0("ssb");
        strcpy(st.cur->mode, bw_unheld[i][0]);
        snprintf(cmd, sizeof(cmd), "BANDWIDTH %s", bw_unheld[i][1]);
        snprintf(name, sizeof(name),
                 "5-15 matrix %s %s: no capture holds it, not simulated",
                 bw_unheld[i][0], cmd);
        none(name, cmd);
    }

    for (i = 0; i < sizeof(mode_pairs) / sizeof(mode_pairs[0]); i++)
    {
        on_chan0("ssb");
        strcpy(st.cur->mode, mode_pairs[i].from);
        strcpy(st.cur->agc, mode_pairs[i].agc);
        st.cur->width = atoi(mode_pairs[i].width);
        snprintf(cmd, sizeof(cmd), "MODE %s", mode_pairs[i].word);
        snprintf(push, sizeof(push), "* CURRENT CHAN 0: RX 7.231000  TX 7.231000 %s",
                 mode_pairs[i].to);
        snprintf(ack, sizeof(ack), "MODE %s", mode_pairs[i].to);
        snprintf(name, sizeof(name),
                 "5-14 matrix %s %s from %s %s: acked",
                 mode_pairs[i].from, cmd, mode_pairs[i].from, mode_pairs[i].width);
        expect(name, ask(cmd), want_write(cmd, push, ack));
        snprintf(name, sizeof(name),
                 "5-14 matrix %s %s from %s: reads %s %s",
                 mode_pairs[i].from, cmd, mode_pairs[i].width, mode_pairs[i].to,
                 mode_pairs[i].to_width);
        expect(name, ask("CHAN"), want_chan(0, "7.231000", mode_pairs[i].to,
                                            mode_pairs[i].to_agc, mode_pairs[i].to_width, "NO", "SSB>"));
    }
}

static void check_retune(void)
{
    static const char complete[] = "RETUNE\r\nRETUNE\r\n* COUPLER_STATUS TUNING\r\n"
                                   "* COUPLER_STATUS TUNED\r\n* KEYSRC PTT KEYTYPE CW\r\n"
                                   "* KEYSRC NONE KEYTYPE OFF\r\n* RETUNE ID: 17769 COMPLETE\r\n\r\nSSB> "
                                   MANUAL CH0;

    on_chan0("ssb");
    expect("5-19 RETUNE complete", ask("RETUNE"), complete);
    expect("5-19 RETUNE again: the same id, no step", ask("RETUNE"), complete);
    on_chan0("rxonly");
    expect("5-19 rxonly: RETUNE FAILED", ask("RETUNE"),
           "RETUNE\r\nRETUNE\r\n* EXCEPTION: 3100 \"CAN'T KEY - CHANNEL IS RX ONLY\"\r\n"
           "* RETUNE ID: 17913 FAILED\r\n\r\nSSB> " MANUAL CH0);
    /* rxonly is channel 0's: channel 25 in that scenario is an ordinary one */
    init_state("rxonly");
    expect("5-3 rxonly: channel 25 reads RXONLY NO", ask("CHAN"),
           want_chan(25, "21.096000", "USB", "MED", "3000", "NO", "SSB>"));
    expect("5-19 rxonly: RETUNE on channel 25 completes", ask("RETUNE"),
           "RETUNE\r\nRETUNE\r\n* COUPLER_STATUS TUNING\r\n"
           "* COUPLER_STATUS TUNED\r\n* KEYSRC PTT KEYTYPE CW\r\n"
           "* KEYSRC NONE KEYTYPE OFF\r\n* RETUNE ID: 17769 COMPLETE\r\n\r\nSSB> "
           MANUAL "* CURRENT CHAN 25: RX 21.096000  TX 21.096000 USB\r\n");
    on_chan0("bypassed");
    expect("5-19 bypassed: RETUNE refused 7040 with the captured text",
           ask("RETUNE"), "RETUNE\r\nERROR: 7040  "
           "'CANNOT RETUNE WHEN COUPLER IS BYPASSED'\r\n\r\nSSB> ");
}

/* 5-21: a line is answered only as the whole line it is; one the
 * simulator cannot hold, or one with a byte that is not printable ASCII,
 * is not simulated as a whole and never shortened into another command */
static void check_framing(void)
{
    static const char nul[] = "FREQ 14.25\0BOGUS\r";
    static const char *const words[] =
    {
        "FREQ 7.", "FREQ .5", "FREQ  14.25", "FREQ 14.25 ", "FREQ +7.231",
        "FREQ -7.231", "MODE USB ", "IDENTITY ", "identity", "AGC  MED",
        "POWER LOW ", "RADIO  SQUELCH ON", "RADIO RFGAIN 50 ", "RADIO RFGAIN +50",
        "BANDWIDTH 03000", "BANDWIDTH +2400", "PRESET ACTIVE CHAN 00",
    };
    char line[200], bytes[256], rendered[256], name[96], all[REPLY_MAX];
    size_t i;

    on_chan0("ssb");
    /* a 127-byte line is held whole */
    snprintf(line, sizeof(line), "FREQ 7.231%0117d", 0);
    snprintf(bytes, sizeof(bytes), "%s\r", line);
    feed(bytes, all);
    expect("5-21 a 127-byte line is held whole and answered", all,
           want_write(line, "* CURRENT CHAN 0: RX 7.231000  TX 7.231000 LSB", line));
    /* one byte more is not simulated, though its first 127 are a command */
    snprintf(line, sizeof(line), "FREQ 7.231%0118d", 0);
    snprintf(bytes, sizeof(bytes), "%s\r", line);
    snprintf(rendered, sizeof(rendered), "%.127s... (128 bytes)", line);
    none_bytes("5-21 a 128-byte line: not simulated as a whole", bytes,
               strlen(bytes), rendered);
    snprintf(line, sizeof(line), "FREQ 14.25%0117dBOGUS", 0);
    snprintf(bytes, sizeof(bytes), "%s\r", line);
    snprintf(rendered, sizeof(rendered), "%.127s... (132 bytes)", line);
    none_bytes("5-21 an overlong line is never cut into a command", bytes,
               strlen(bytes), rendered);
    none_bytes("5-21 a NUL in the line: not simulated as a whole", nul,
               sizeof(nul) - 1, "FREQ 14.25\\x00BOGUS");
    none_bytes("5-21 a tab is not taken for a space", "FREQ\t14.25\r", 11,
               "FREQ\\x0914.25");
    none_bytes("5-21 a byte above 0x7E: not simulated as a whole",
               "FREQ 14.25\xb0\r", 12, "FREQ 14.25\\xB0");
    none_bytes("5-21 a DEL in the line: not simulated as a whole",
               "IDENTITY\x7f\r", 10, "IDENTITY\\x7F");
    expect("5-21 after those lines the state is the start's", ask("CHAN"),
           want_chan(0, "7.231000", "LSB", "MED", "3000", "NO", "SSB>"));

    /* every other changed form of a captured line: not simulated */
    for (i = 0; i < sizeof(words) / sizeof(words[0]); i++)
    {
        snprintf(name, sizeof(name), "5-21 '%s' is not a captured form: not simulated",
                 words[i]);
        none(name, words[i]);
    }
}

static void check_other(void)
{
    static const char *const unknown[] = { "KEY", "PTT", "TX", "KEY ON", "UP", "DOWN" };
    char name[96], all[REPLY_MAX];
    size_t i;

    on_chan0("ssb");

    for (i = 0; i < sizeof(unknown) / sizeof(unknown[0]); i++)
    {
        snprintf(name, sizeof(name), "5-20 %s: 7015", unknown[i]);
        expect(name, ask(unknown[i]), want_error(unknown[i], E7015, "SSB>"));
    }

    /* 5-21: CR ends a line, a LF is ignored */
    init_state("ssb");
    expect_true("5-21 no answer before the CR", feed("IDENTITY", all) == 0
                && all[0] == '\0');
    expect_true("5-21 a LF after the CR is ignored: two lines",
                feed("IDENTITY\r\n\r", all) == 2);
    expect("5-21 a LF after the CR is ignored: the answers", all,
           "IDENTITY\r\nIDENTITY RF-5800H\r\n\r\nSSB> \r\n\r\nSSB> ");
    check_framing();

    /* 5-22 */
    init_state("prc138");
    expect("5-22 prc138: the empty line", ask(""), "\r\n\r\n\rSSB> ");
    expect("5-22 prc138: IDENTITY", ask("IDENTITY"),
           "IDENTITY\r\nPRC-138\r\n\r\n\rSSB> ");
    none("5-22 prc138: CHAN not simulated", "CHAN");
    none("5-22 prc138: PRESET ACTIVE CHAN 0 not simulated", "PRESET ACTIVE CHAN 0");

    /* 5-23 */
    init_state("echo-off");
    expect("5-1 echo off: the empty line", ask(""), "\r\nSSB> ");
    expect("5-2 echo off: IDENTITY without its echo", ask("IDENTITY"),
           "IDENTITY RF-5800H\r\n\r\nSSB> ");
    none("5-23 echo-off: CHAN not simulated", "CHAN");
    none("5-23 echo-off: FREQ 60 not simulated", "FREQ 60");
    none("5-23 echo-off: PRESET ACTIVE CHAN 0 not simulated",
         "PRESET ACTIVE CHAN 0");
    init_state("busy");
    expect("5-23 busy: the empty line at ALE>", ask(""), "\r\n\r\nALE> ");
    expect("5-23 busy: IDENTITY at ALE>", ask("IDENTITY"),
           "IDENTITY\r\nIDENTITY RF-5800H\r\n\r\nALE> ");
    none("5-23 busy: CHAN not simulated", "CHAN");
    none("5-23 busy: RADIO not simulated", "RADIO");
    none("5-23 busy: POWER not simulated", "POWER");

    /* 5-24 */
    init_state("ale");
    none("5-24 FREQ at ALE>: not simulated", "FREQ 7.231");
    none("5-24 PRESET ACTIVE CHAN 0 at ALE>: not simulated",
         "PRESET ACTIVE CHAN 0");
    none("5-24 POWER LOW at ALE>: not simulated", "POWER LOW");
    none("5-24 RETUNE at ALE>: not simulated", "RETUNE");
    none("5-24 ALE STOP at ALE>: not simulated", "ALE STOP");
    init_state("ale-call");
    none("5-24 MODE LSB at ALE> (ale-call): not simulated", "MODE LSB");
    init_state("hop");
    none("5-24 POWER LOW at HOP>: not simulated", "POWER LOW");
    none("5-24 RADIO SQUELCH OFF at HOP>: not simulated", "RADIO SQUELCH OFF");
    init_state("prgm");
    none("5-24 SSB at PRGM>: not simulated", "SSB");
    none("5-24 FREQ at PRGM>: not simulated", "FREQ 7.231");
    none("5-24 PRESET ACTIVE CHAN 0 at PRGM>: not simulated",
         "PRESET ACTIVE CHAN 0");

    /* 5-25 */
    on_chan0("ssb");
    none("5-25 STATE at SSB>: not simulated", "STATE");
    none("5-25 RXFREQ at SSB>: not simulated", "RXFREQ 7.231");
    none("5-25 RFGAIN 50 at SSB>: not simulated", "RFGAIN 50");
    none("5-25 PRESET ACTIVE CHAN 25 at SSB>: not simulated",
         "PRESET ACTIVE CHAN 25");
    none("5-25 an unknown word at SSB>: not simulated", "XYZZY");
    none("5-25 ALE at SSB>: not simulated", "ALE");
}

static int run_selftest(void)
{
    selftest = 1;
    check_reads();
    check_ssb_word();
    check_writes();
    check_matrix();
    check_retune();
    check_other();

    if (st_fail)
    {
        fprintf(stderr, "simrf5800 selftest: %d failed\n", st_fail);
        return 1;
    }

    printf("simrf5800 selftest: all passed\n");
    return 0;
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
    tcp = 1;

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

int main(int argc, char *argv[])
{
    const char *scenario = argc > 2 ? argv[2] : "ssb";
    static const char *const known[] =
    {
        "ssb", "ale", "ale-call", "busy", "hop", "prgm", "echo-off", "prc138",
        "bypassed", "rxonly", NULL
    };

    if (argc == 2 && strcmp(argv[1], "--selftest") == 0)
    {
        return run_selftest();
    }

    if (argc < 2 || !one_of(scenario, known))
    {
        fprintf(stderr, "usage: %s <port|tcp-port> [ssb|ale|ale-call|busy|hop|"
                        "prgm|echo-off|prc138|bypassed|rxonly]\n"
                        "       %s --selftest\n"
                        "It replays answers captured from one RF-5800H; any other line "
                        "is ignored and named on the simulator's console as not "
                        "simulated.\n", argv[0], argv[0]);
        return 2;
    }

    init_state(scenario);

    if (is_number(argv[1]))
    {
        return serve_tcp(atoi(argv[1]));
    }

    fd = openPort(argv[1]);

    if (fd < 0)
    {
        return 1;
    }

    serve();
    return 0;
}
