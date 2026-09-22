/*
 *  Hamlib SmartSDR control session
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

/* The TCP control session with a SmartSDR radio. Commands go out numbered and */
/* come back as an R-line, interleaved with unsolicited S-lines of status that */
/* arrive whenever the radio feels like it, so this file reassembles the byte  */
/* stream into lines, routes each line to the layer that wants it, and keeps   */
/* the session alive -- rebuilding it after a loss when asked to.              */

#ifdef HAVE_CONFIG_H
#  include "config.h"
#endif

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <errno.h>
#include <time.h>
#include <pthread.h>

/* Socket headers come from stream_proto.h, which picks the right set for the
 * host; do not include them directly. */
#include "stream_proto.h"

#include "hamlib/rig.h"
#include "hamlib/port.h"
#include "hamlib/rig_state.h"
#include "misc.h"
#include "cache.h"
#include "network.h"
#include "iofunc.h"
#include "smartsdr_priv.h"
#include "smartsdr_props.h"
#include "smartsdr_rig.h"
#include "smartsdr_session.h"
#include "smartsdr_slice.h"

static int smartsdr_parse_S(RIG *rig, char *s);

/* Reconnect backoff: doubles after each failed attempt, then holds at the cap. */
#define SMARTSDR_RECONNECT_MIN_MS  1000
#define SMARTSDR_RECONNECT_MAX_MS 30000

/* Slowest the keepalive may ping, and how finely the wait between pings is
 * broken up so that closing the rig does not have to sit through a whole
 * interval. The radio drops a client that has not spoken for about fifteen
 * seconds, which is what caps the cadence. */
#define SMARTSDR_KEEPALIVE_MAX_INTERVAL_MS 10000
#define SMARTSDR_KEEPALIVE_SLICE_MS          100

/* How long the control thread waits for the socket before looking at its run
 * flag again, so that closing the rig does not wait for the radio to speak. */
#define SMARTSDR_CONTROL_POLL_MS 50


static int smartsdr_tcp_append(struct smartsdr_priv_data *priv,
                               const char *data, size_t n)
{
    if (priv->tcp_asm_len + n + 1U > sizeof(priv->tcp_asm_buf))
    {
        rig_debug(RIG_DEBUG_ERR, "%s: TCP reassembly overflow\n", __func__);
        priv->tcp_asm_len = 0;
        priv->tcp_asm_buf[0] = 0;
        return -1;
    }

    memcpy(priv->tcp_asm_buf + priv->tcp_asm_len, data, n);
    priv->tcp_asm_len += n;
    priv->tcp_asm_buf[priv->tcp_asm_len] = 0;
    return 0;
}


static int smartsdr_tcp_pop_line(struct smartsdr_priv_data *priv,
                                 char *line_out, size_t line_out_sz)
{
    char *nl;
    size_t linelen;

    if (priv->tcp_asm_len == 0)
    {
        return 0;
    }

    nl = memchr(priv->tcp_asm_buf, '\n', priv->tcp_asm_len);

    if (nl == NULL)
    {
        return 0;
    }

    linelen = (size_t)(nl - priv->tcp_asm_buf);

    if (linelen + 1U >= line_out_sz)
    {
        rig_debug(RIG_DEBUG_ERR, "%s: line too long (%zu)\n", __func__,
                  linelen);
        priv->tcp_asm_len = 0;
        priv->tcp_asm_buf[0] = 0;
        return -1;
    }

    memcpy(line_out, priv->tcp_asm_buf, linelen);
    line_out[linelen] = 0;

    {
        size_t rest = priv->tcp_asm_len - linelen - 1U;

        if (rest > 0)
        {
            memmove(priv->tcp_asm_buf, nl + 1, rest);
        }

        priv->tcp_asm_len = rest;
        priv->tcp_asm_buf[priv->tcp_asm_len] = 0;
    }

    return 1;
}


/* ------------------------------------------------------------------ */
/* Control connection reader                                           */
/* ------------------------------------------------------------------ */

/* Take a waiter out of the list. Call with reply_lock held. */
static void smartsdr_reply_unlink(struct smartsdr_priv_data *priv,
                                  struct smartsdr_reply_waiter *waiter)
{
    struct smartsdr_reply_waiter **pp = &priv->reply_waiters;

    while (*pp != NULL)
    {
        if (*pp == waiter)
        {
            *pp = waiter->next;
            waiter->next = NULL;
            return;
        }

        pp = &(*pp)->next;
    }
}


/* Hand an R-line to the caller waiting for that sequence number. A reply
 * nobody is waiting for is what a caller that has already given up leaves
 * behind, so it is dropped rather than mistaken for someone else's. */
static void smartsdr_reply_deliver(struct smartsdr_priv_data *priv,
                                   const char *line)
{
    struct smartsdr_reply_waiter *waiter;
    const char *bar = strchr(line, '|');
    unsigned status = 0U;
    int seq = -1;

    if (sscanf(line + 1, "%d", &seq) != 1)
    {
        rig_debug(RIG_DEBUG_WARN, "%s: R-line carries no sequence: %s\n",
                  __func__, line);
        return;
    }

    if (bar != NULL)
    {
        sscanf(bar + 1, "%x", &status);
    }

    pthread_mutex_lock(&priv->reply_lock);

    for (waiter = priv->reply_waiters; waiter != NULL; waiter = waiter->next)
    {
        if (waiter->seq != seq || waiter->answered)
        {
            continue;
        }

        waiter->status = status;

        if (waiter->resp_buf != NULL && waiter->resp_buf_len > 0)
        {
            strncpy(waiter->resp_buf, line, (size_t)waiter->resp_buf_len - 1U);
            waiter->resp_buf[waiter->resp_buf_len - 1] = 0;
        }

        waiter->answered = 1;
        break;
    }

    pthread_cond_broadcast(&priv->reply_cond);
    pthread_mutex_unlock(&priv->reply_lock);

    if (waiter == NULL)
    {
        rig_debug(RIG_DEBUG_TRACE, "%s: nobody waiting for R%d\n", __func__,
                  seq);
    }
}


/* Route one line from the radio: status is absorbed here and now, a reply
 * goes to its caller, and the connect banner is only noted. */
static void smartsdr_control_dispatch(RIG *rig, char *line)
{
    struct smartsdr_priv_data *priv =
        (struct smartsdr_priv_data *)STATE(rig)->priv;

    if (line[0] == 'S')
    {
        smartsdr_parse_S(rig, line);
    }
    else if (line[0] == 'R')
    {
        smartsdr_reply_deliver(priv, line);
    }
    else if (line[0] == 'V' || line[0] == 'H' || line[0] == 'M')
    {
        rig_debug(RIG_DEBUG_TRACE, "%s: banner %s\n", __func__, line);
    }
    else if (line[0] != 0)
    {
        rig_debug(RIG_DEBUG_WARN, "%s: Unknown packet type=%s\n", __func__,
                  line);
    }
}


/* Wait up to timeout_ms for the control socket to have something, then take
 * whatever is there. Returns the byte count, 0 when nothing arrived, or
 * negative when the connection is gone. */
static int smartsdr_control_recv(RIG *rig, char *buf, size_t buf_len,
                                 int timeout_ms)
{
    hamlib_port_t *rp = RIGPORT(rig);
    int fd = rp->fd;
    struct timeval tv;
    fd_set rfds;
    int n;

    if (fd < 0)
    {
        return -1;
    }

    FD_ZERO(&rfds);
    FD_SET(fd, &rfds);
    tv.tv_sec = timeout_ms / 1000;
    tv.tv_usec = (long)(timeout_ms % 1000) * 1000;

    n = select(fd + 1, &rfds, NULL, NULL, &tv);

    if (n == 0)
    {
        return 0;
    }

    if (n < 0)
    {
        return (errno == EINTR) ? 0 : -1;
    }

    n = (int)recv(fd, buf, (int)buf_len, 0);

    /* The radio closing the connection reads as end of file. */
    if (n == 0)
    {
        return -1;
    }

    if (n < 0)
    {
        return (errno == EINTR || errno == EAGAIN || errno == EWOULDBLOCK)
               ? 0 : -1;
    }

    return n;
}


/* The reader for the control connection. Everything the radio sends arrives
 * here: status is absorbed as it comes rather than when a command next
 * happens to look, replies go to their callers, and the time of the last byte
 * is what says the radio is still there. */
static void *smartsdr_control_thread(void *arg)
{
    RIG *rig = (RIG *)arg;
    struct smartsdr_priv_data *priv =
        (struct smartsdr_priv_data *)STATE(rig)->priv;
    char chunk[12288];
    char line[16384];

    rig_debug(RIG_DEBUG_VERBOSE, "%s: control thread started\n", __func__);

    while (priv->control_running)
    {
        int n = smartsdr_control_recv(rig, chunk, sizeof(chunk),
                                      SMARTSDR_CONTROL_POLL_MS);

        if (n < 0)
        {
            /* The socket failed, which is unambiguous. Pause before looking
             * again so that a dead connection does not spin. */
            smartsdr_session_lost(rig, RIG_COMM_REASON_SOCKET_ERROR);
            hl_usleep(SMARTSDR_CONTROL_POLL_MS * 1000);
            continue;
        }

        if (n > 0)
        {
            /* Anything at all from the radio is proof of life. */
            priv->last_heard_ms = smartsdr_now_ms();

            if (smartsdr_tcp_append(priv, chunk, (size_t)n) == 0)
            {
                while (smartsdr_tcp_pop_line(priv, line, sizeof(line)) > 0)
                {
                    smartsdr_control_dispatch(rig, line);
                }
            }
        }

        /* The radio answers everything and is pinged twice within this
         * budget, so silence is the one symptom that does not also describe a
         * slow radio. */
        if (smartsdr_now_ms() - priv->last_heard_ms
                > priv->liveness_timeout_ms)
        {
            smartsdr_session_lost(rig, RIG_COMM_REASON_LINK_TIMEOUT);
        }
    }

    rig_debug(RIG_DEBUG_VERBOSE, "%s: control thread exiting\n", __func__);
    return NULL;
}


int smartsdr_control_start(RIG *rig)
{
    struct smartsdr_priv_data *priv =
        (struct smartsdr_priv_data *)STATE(rig)->priv;

    if (priv->control_started)
    {
        return RIG_OK;
    }

    /* Whatever was half-read belonged to a connection that is gone. */
    priv->tcp_asm_len = 0;
    priv->tcp_asm_buf[0] = 0;
    priv->last_heard_ms = smartsdr_now_ms();
    priv->control_running = 1;

    if (pthread_create(&priv->control_thread, NULL, smartsdr_control_thread,
                       rig) != 0)
    {
        priv->control_running = 0;
        rig_debug(RIG_DEBUG_ERR, "%s: control thread create failed\n",
                  __func__);
        return -RIG_EINTERNAL;
    }

    priv->control_started = 1;
    return RIG_OK;
}


void smartsdr_control_stop(RIG *rig)
{
    struct smartsdr_priv_data *priv =
        (struct smartsdr_priv_data *)STATE(rig)->priv;

    if (!priv->control_started)
    {
        return;
    }

    priv->control_running = 0;
    pthread_join(priv->control_thread, NULL);
    priv->control_started = 0;

    /* No reply can arrive now, so release anyone still waiting for one. */
    pthread_mutex_lock(&priv->reply_lock);
    pthread_cond_broadcast(&priv->reply_cond);
    pthread_mutex_unlock(&priv->reply_lock);
}


/* ------------------------------------------------------------------ */
/* Commanding the radio                                                */
/* ------------------------------------------------------------------ */

static void smartsdr_reply_deadline(struct timespec *deadline, int timeout_ms)
{
    struct timespec now;

    /* pthread_cond_timedwait measures against the wall clock. */
    clock_gettime(CLOCK_REALTIME, &now);
    deadline->tv_sec = now.tv_sec + timeout_ms / 1000;
    deadline->tv_nsec = now.tv_nsec + (long)(timeout_ms % 1000) * 1000000L;

    if (deadline->tv_nsec >= 1000000000L)
    {
        deadline->tv_sec++;
        deadline->tv_nsec -= 1000000000L;
    }
}


/* Wait for the control thread to deliver this caller's reply, and take the
 * waiter out of the list either way. Returns 1 when the reply arrived. */
static int smartsdr_reply_await(struct smartsdr_priv_data *priv,
                                struct smartsdr_reply_waiter *waiter,
                                int timeout_ms)
{
    struct timespec deadline;
    int answered;

    smartsdr_reply_deadline(&deadline, timeout_ms);

    pthread_mutex_lock(&priv->reply_lock);

    while (!waiter->answered && priv->control_running)
    {
        if (pthread_cond_timedwait(&priv->reply_cond, &priv->reply_lock,
                                   &deadline) != 0)
        {
            break;
        }
    }

    answered = waiter->answered;
    smartsdr_reply_unlink(priv, waiter);
    pthread_mutex_unlock(&priv->reply_lock);

    return answered;
}


/* Send a command whose result the caller has no way to act on, but whose
 * rejection must not pass in silence. A refused setup command leaves the
 * radio configured differently from what was asked with nothing else to show
 * for it, which is how a transmit routing command went unnoticed while every
 * key silently failed. Teardown uses a plain (void) instead: there is nothing
 * to salvage once a stream is going away. */
void smartsdr_command_or_warn(RIG *rig, char *cmd)
{
    int retval = smartsdr_transaction_resp(rig, cmd, NULL, 0);

    if (retval != RIG_OK)
    {
        rig_debug(RIG_DEBUG_WARN, "%s: radio refused \"%s\": %s\n",
                  __func__, cmd, rigerror(retval));
    }
}


int smartsdr_transaction_resp(RIG *rig, char *cmd_buf,
                              char *resp_buf, int resp_buf_len)
{
    struct smartsdr_priv_data *priv = (struct smartsdr_priv_data *)STATE(rig)->priv;
    struct smartsdr_reply_waiter waiter;
    char cmd[4096];
    int retval;

    if (resp_buf && resp_buf_len > 0)
    {
        resp_buf[0] = 0;
    }

    /* Status is absorbed by the control thread the moment it arrives, so a
     * call that sends nothing has nothing left to do. */
    if (cmd_buf == NULL)
    {
        return RIG_OK;
    }

    memset(&waiter, 0, sizeof(waiter));
    waiter.resp_buf = resp_buf;
    waiter.resp_buf_len = resp_buf_len;

    /* Writers take turns with each other; the reader is a thread of its own
     * and never waits for this. */
    pthread_mutex_lock(&priv->tcp_mutex);

    if (priv->seqnum > 999999) { priv->seqnum = 0; }

    waiter.seq = priv->seqnum++;

    /* Enrol before the command goes out: the radio can answer it before this
     * thread runs again. */
    pthread_mutex_lock(&priv->reply_lock);
    waiter.next = priv->reply_waiters;
    priv->reply_waiters = &waiter;
    pthread_mutex_unlock(&priv->reply_lock);

    snprintf(cmd, sizeof(cmd), "C%d|%s%c", waiter.seq, cmd_buf, 0x0a);
    retval = write_block(RIGPORT(rig), (unsigned char *)cmd, strlen(cmd));

    pthread_mutex_unlock(&priv->tcp_mutex);

    if (retval != RIG_OK)
    {
        pthread_mutex_lock(&priv->reply_lock);
        smartsdr_reply_unlink(priv, &waiter);
        pthread_mutex_unlock(&priv->reply_lock);

        rig_debug(RIG_DEBUG_ERR, "%s: SmartSDR write_block err=0x%x\n",
                  __func__, retval);
        /* A failed write is unambiguous: the link is gone. Reporting it
         * here means the caller's own command notices, rather than
         * waiting for the silence to be noticed. */
        smartsdr_session_lost(rig, RIG_COMM_REASON_SOCKET_ERROR);
        return retval;
    }

    /* Some commands earn no R-line at all, so the wait has to be allowed to
     * end without one. */
    if (!smartsdr_reply_await(priv, &waiter, RIGPORT(rig)->timeout))
    {
        return -RIG_ETIMEOUT;
    }

    if (waiter.status == 0U)
    {
        return RIG_OK;
    }

    /* A radio that has not been told about this client refuses to record the
     * program name and then carries on regardless, so the session is good. */
    if (strncmp(cmd_buf, "client program", 14) == 0
            && waiter.status == 0x10000002U)
    {
        rig_debug(RIG_DEBUG_VERBOSE,
                  "%s: client program not in radio whitelist (continuing)\n",
                  __func__);
        return RIG_OK;
    }

    rig_debug(RIG_DEBUG_ERR, "%s: SmartSDR err=0x%x\n", __func__,
              waiter.status);
    return -RIG_EPROTO;
}


static unsigned smartsdr_r_line_status(const char *r_line)
{
    const char *p;

    if (!r_line || r_line[0] != 'R')
    {
        return 0U;
    }

    p = strchr(r_line, '|');

    if (p == NULL || p[1] == 0)
    {
        return 0U;
    }

    p++;
    {
        unsigned v = 0U;

        if (sscanf(p, "%x", &v) != 1)
        {
            return 0U;
        }

        return v;
    }
}


int smartsdr_sync_display_slice_status(RIG *rig)
{
    struct smartsdr_priv_data *priv =
        (struct smartsdr_priv_data *)STATE(rig)->priv;
    char resp[SMARTSDR_RESP_LEN];
    char cmd[48];
    int r1;
    int r2;
    int r3;

    snprintf(cmd, sizeof(cmd), "sub slice %d", priv->slicenum);
    r1 = smartsdr_transaction_resp(rig, cmd, resp, sizeof(resp));
    r2 = smartsdr_transaction_resp(rig, "sub pan all", resp, sizeof(resp));
    r3 = smartsdr_transaction_resp(rig, "ping", resp, sizeof(resp));

    if (r1 != RIG_OK || r2 != RIG_OK || r3 != RIG_OK)
    {
        rig_debug(RIG_DEBUG_VERBOSE,
                  "%s: sub/ping returned r1=%d r2=%d r3=%d\n", __func__,
                  r1, r2, r3);
    }

    return RIG_OK;
}


static int smartsdr_parse_third_field_hex_id(const char *r_line, uint32_t *id)
{
    const char *p;
    unsigned u;

    if (!r_line || r_line[0] != 'R' || !id)
    {
        return -1;
    }

    p = strchr(r_line, '|');

    if (p == NULL)
    {
        return -1;
    }

    p++;

    p = strchr(p, '|');

    if (p == NULL)
    {
        return -1;
    }

    p++;

    while (*p == ' ' || *p == '\t')
    {
        p++;
    }

    if (strncmp(p, "pan=", 4) == 0)
    {
        p += 4;
    }

    if (sscanf(p, "%x", &u) != 1)
    {
        return -1;
    }

    *id = (uint32_t)u;
    return 0;
}


/* Third field of R-line may be "0xPAN" or "0xPAN,0xWATERFALL" (Flex create). */
static int smartsdr_parse_r_line_pan_waterfall(const char *r_line,
        uint32_t *pan_id,
        uint32_t *waterfall_id)
{
    const char *p;
    char *endp = NULL;
    unsigned long pan_ul;
    unsigned long wf_ul = 0UL;

    if (!r_line || r_line[0] != 'R' || !pan_id || !waterfall_id)
    {
        return -1;
    }

    *waterfall_id = 0U;
    p = strchr(r_line, '|');

    if (p == NULL)
    {
        return -1;
    }

    p = strchr(p + 1, '|');

    if (p == NULL)
    {
        return -1;
    }

    p++;

    while (*p == ' ' || *p == '\t')
    {
        p++;
    }

    pan_ul = strtoul(p, &endp, 0);

    if (endp == p || pan_ul == 0UL)
    {
        return -1;
    }

    *pan_id = (uint32_t)pan_ul;

    if (*endp == ',')
    {
        wf_ul = strtoul(endp + 1, &endp, 0);

        if (wf_ul != 0UL)
        {
            *waterfall_id = (uint32_t)wf_ul;
        }
    }

    return 0;
}


/* The slice already has a panadapter: tune it and let the pan follow, rather
 * than building one. Reached twice -- once when the pan id is already known,
 * and again when the radio answers a create with "already exists" -- so it
 * lives here instead of being written out both times. Returns 1 when it
 * handled the request. */
static int smartsdr_slice_tune_existing_pan(RIG *rig,
                                            struct smartsdr_priv_data *priv,
                                            double freq_mhz)
{
    char cmd[128];

    if (priv->slice_pan_id == 0U)
    {
        return 0;
    }

    snprintf(cmd, sizeof(cmd), "slice tune %d %.6f autopan=1",
             priv->slicenum, freq_mhz);
    smartsdr_command_or_warn(rig, cmd);
    smartsdr_slice_set_freq(priv, freq_mhz * 1e6);
    rig_set_cache_freq(rig, RIG_VFO_A, smartsdr_slice_freq(priv));
    priv->pan_waterfall_bind_pan_id = 0U;

    /* A pan with no waterfall of its own sends nothing until it is told how
     * often to. */
    if (priv->slice_waterfall_id == 0U)
    {
        snprintf(cmd, sizeof(cmd), "display pan set 0x%x fps=10",
                 (unsigned)priv->slice_pan_id);
        smartsdr_command_or_warn(rig, cmd);
    }

    snprintf(cmd, sizeof(cmd), "sub slice %d", priv->slicenum);
    smartsdr_command_or_warn(rig, cmd);
    return 1;
}


int smartsdr_prepare_slice_panafall(RIG *rig, double freq_mhz)
{
    struct smartsdr_priv_data *priv =
        (struct smartsdr_priv_data *)STATE(rig)->priv;
    char cmd[192];
    char resp[SMARTSDR_RESP_LEN];
    uint32_t pan_id = 0;
    uint32_t wf_id = 0;
    int retval;

    ENTERFUNC;

    if (!priv || freq_mhz <= 0.0)
    {
        RETURNFUNC(-RIG_EINVAL);
    }

    snprintf(cmd, sizeof(cmd), "sub slice %d", priv->slicenum);
    smartsdr_command_or_warn(rig, cmd);

    if (smartsdr_slice_tune_existing_pan(rig, priv, freq_mhz))
    {
        RETURNFUNC(RIG_OK);
    }

    snprintf(cmd, sizeof(cmd),
             "display pan c freq=%.6f ant=ANT1 x=100 y=100", freq_mhz);
    retval = smartsdr_transaction_resp(rig, cmd, resp, sizeof(resp));

    if (retval != RIG_OK)
    {
        snprintf(cmd, sizeof(cmd),
                 "display pan create x=100 y=100 center=%.6f "
                 "bandwidth=0.048",
                 freq_mhz);
        retval = smartsdr_transaction_resp(rig, cmd, resp, sizeof(resp));
    }

    if (retval != RIG_OK)
    {
        snprintf(cmd, sizeof(cmd),
                 "display panafall c freq=%.6f ant=ANT1 x=100 y=100",
                 freq_mhz);
        retval = smartsdr_transaction_resp(rig, cmd, resp, sizeof(resp));
    }

    if (retval != RIG_OK)
    {
        snprintf(cmd, sizeof(cmd),
                 "display panafall create x=100 y=100");
        retval = smartsdr_transaction_resp(rig, cmd, resp, sizeof(resp));
    }

    if (retval != RIG_OK)
    {
        snprintf(cmd, sizeof(cmd),
                 "display pan create x=100 y=100 fps=10 center=%.6f "
                 "bandwidth=0.048 min_dbm=-130 max_dbm=-50",
                 freq_mhz);
        retval = smartsdr_transaction_resp(rig, cmd, resp, sizeof(resp));
    }

    if (retval != RIG_OK)
    {
        if (smartsdr_r_line_status(resp) == 0x50000009U)
        {
            (void)smartsdr_sync_display_slice_status(rig);

            if (smartsdr_slice_tune_existing_pan(rig, priv, freq_mhz))
            {
                RETURNFUNC(RIG_OK);
            }
        }

        rig_debug(RIG_DEBUG_WARN, "%s: panadapter create failed\n", __func__);
        RETURNFUNC(retval);
    }

    if (smartsdr_parse_r_line_pan_waterfall(resp, &pan_id, &wf_id) < 0)
    {
        if (smartsdr_parse_third_field_hex_id(resp, &pan_id) < 0)
        {
            rig_debug(RIG_DEBUG_ERR, "%s: bad R-line for pan id: %s\n",
                      __func__, resp);
            RETURNFUNC(-RIG_EPROTO);
        }
    }

    snprintf(cmd, sizeof(cmd),
             "display pan set 0x%x xpixels=1024 ypixels=700", pan_id);
    smartsdr_command_or_warn(rig, cmd);

    snprintf(cmd, sizeof(cmd),
             "display pan set 0x%x fps=10 min_dbm=-130 max_dbm=-50",
             pan_id);
    smartsdr_command_or_warn(rig, cmd);

    snprintf(cmd, sizeof(cmd), "slice create pan=0x%x freq=%.6f",
             pan_id, freq_mhz);
    retval = smartsdr_transaction_resp(rig, cmd, resp, sizeof(resp));

    if (retval != RIG_OK)
    {
        snprintf(cmd, sizeof(cmd), "slice set %d pan=0x%x",
                 priv->slicenum, pan_id);
        smartsdr_command_or_warn(rig, cmd);
    }

    priv->pan_waterfall_bind_pan_id = 0U;
    priv->slice_pan_id = pan_id;
    priv->slice_waterfall_id = wf_id;
    smartsdr_slice_set_freq(priv, freq_mhz * 1e6);
    rig_set_cache_freq(rig, RIG_VFO_A, smartsdr_slice_freq(priv));

    snprintf(cmd, sizeof(cmd), "sub slice %d", priv->slicenum);
    smartsdr_command_or_warn(rig, cmd);

    RETURNFUNC(RIG_OK);
}


int smartsdr_transaction(RIG *rig, char *buf)
{
    return smartsdr_transaction_resp(rig, buf, NULL, 0);
}


/* NAT traversal backstop: re-assert the client's UDP endpoint over TCP now and
 * then. This cannot refresh a NAT mapping by itself — only a UDP datagram from
 * the client can, which the stream threads send — so it runs rarely. */
static void *smartsdr_wan_register_loop(void *arg)
{
    RIG *rig = (RIG *)arg;
    struct smartsdr_priv_data *priv =
        (struct smartsdr_priv_data *)STATE(rig)->priv;
    unsigned elapsed_ms = 0;

    rig_debug(RIG_DEBUG_VERBOSE, "%s: NAT re-register thread started\n",
              __func__);

    while (priv->wan_register_running)
    {
        hl_usleep(100000);

        if (!priv->wan_register_running)
        {
            break;
        }

        if (!priv->nat_traversal || priv->client_handle == 0)
        {
            elapsed_ms = 0;
            continue;
        }

        elapsed_ms += 100;

        if (elapsed_ms >= SMARTSDR_NAT_REREGISTER_INTERVAL_MS)
        {
            char cmd[96];

            elapsed_ms = 0;
            snprintf(cmd, sizeof(cmd), "client udp_register handle=0x%x",
                     (unsigned)priv->client_handle);
            smartsdr_command_or_warn(rig, cmd);
        }
    }

    rig_debug(RIG_DEBUG_VERBOSE, "%s: NAT re-register thread exiting\n",
              __func__);
    return NULL;
}


/* Record that the radio is gone. Recorded once: later failures do not
 * overwrite the first reason, which is the one that explains what happened. */
void smartsdr_session_lost(RIG *rig, unsigned reason)
{
    struct smartsdr_priv_data *priv = (struct smartsdr_priv_data *)STATE(rig)->priv;

    if (priv->session_lost)
    {
        return;
    }

    priv->session_lost_reason = reason;
    priv->session_lost = 1;

    /* Reason before status: an application polls the status and then reads
     * the reason, so writing them the other way round lets it pair a fresh
     * status with the previous reason. */
    STATE(rig)->comm_reason = (rig_comm_reason_t)reason;
    STATE(rig)->comm_status = RIG_COMM_STATUS_DISCONNECTED;

    rig_debug(RIG_DEBUG_ERR, "%s: lost the radio: %s\n", __func__,
              rig_strcommreason((rig_comm_reason_t)reason));
}


/* Ping twice within the silence budget, so that a link which is merely quiet
 * still proves itself before the budget runs out, and never less often than
 * the radio tolerates. */
static int smartsdr_keepalive_interval_ms(const struct smartsdr_priv_data *priv)
{
    int half = priv->liveness_timeout_ms / 2;

    return half < SMARTSDR_KEEPALIVE_MAX_INTERVAL_MS
           ? half : SMARTSDR_KEEPALIVE_MAX_INTERVAL_MS;
}


/* The radio drops a client that has not spoken for about fifteen seconds, so
 * something has to keep talking to it. Whether the answer comes back is the
 * control thread's business: it hears every byte the radio sends and declares
 * the loss from silence. */
static void *smartsdr_keepalive(void *arg)
{
    RIG *rig = (RIG *)arg;
    struct smartsdr_priv_data *priv = (struct smartsdr_priv_data *)STATE(rig)->priv;

    rig_debug(RIG_DEBUG_VERBOSE, "%s: keepalive thread started\n", __func__);

    while (priv->keepalive_running)
    {
        /* Sleep in slices so closing the rig does not wait out the interval,
         * the same reason the reconnect loop does it. */
        {
            int interval = smartsdr_keepalive_interval_ms(priv);
            int slept;

            for (slept = 0; slept < interval && priv->keepalive_running;
                    slept += SMARTSDR_KEEPALIVE_SLICE_MS)
            {
                hl_usleep(SMARTSDR_KEEPALIVE_SLICE_MS * 1000);
            }
        }

        if (!priv->keepalive_running)
        {
            break;
        }

        /* Keepalive: a failed ping is what the session-loss detection is
         * for, so there is nothing useful to do with the result here. */
        (void)smartsdr_transaction(rig, "ping");
    }

    rig_debug(RIG_DEBUG_VERBOSE, "%s: keepalive thread exiting\n", __func__);
    return NULL;
}

/* Registration and subscriptions. Without these the radio grants no stream
 * privileges and sends no status, so a failure here is fatal to the session
 * rather than something to carry on from. Shared by open and reconnect. */
int smartsdr_register_session(RIG *rig)
{
    static const char *const setup[] =
    {
        "client gui",          /* required before creating streams */
        "client program Hamlib",
        /* Set station + DAX/MTU before subscribing to streams. */
        "client station Hamlib",
        "client set send_reduced_bw_dax=1",
        "client set enforce_network_mtu=1 network_mtu=1500",
        "keepalive enable",
        "sub slice all",
        "sub pan all",
        "sub audio all",
        "sub daxiq all",
        "sub tx all",       /* transmit object: RF power, mic, CW, VOX */
        "sub radio all",    /* radio object: which oscillator disciplines it */
    };
    size_t i;

    for (i = 0; i < sizeof(setup) / sizeof(setup[0]); i++)
    {
        int rc = smartsdr_transaction(rig, (char *)setup[i]);

        if (rc != RIG_OK)
        {
            rig_debug(RIG_DEBUG_ERR, "%s: \"%s\" failed: %s\n",
                      __func__, setup[i], rigerror(rc));
            return rc;
        }
    }

    return RIG_OK;
}




static void smartsdr_absorb_display_pan_status(RIG *rig, const char *line)
{
    struct smartsdr_priv_data *priv =
        (struct smartsdr_priv_data *)STATE(rig)->priv;
    const char *sub;
    const char *wf;
    char *endp;
    unsigned long pan_ul;
    unsigned long wf_ul;

    sub = strstr(line, "|display pan ");

    if (sub == NULL)
    {
        return;
    }

    sub += (int)strlen("|display pan ");

    if (strncmp(sub, "removed", 7) == 0)
    {
        return;
    }

    pan_ul = strtoul(sub, &endp, 0);

    if (endp == sub || pan_ul == 0UL)
    {
        return;
    }

    /* DAX-IQ is referenced to the panadapter centre rather than the slice
     * frequency; they differ whenever the slice is not centred in its pan. */
    if (priv->slice_pan_id != 0U && (uint32_t)pan_ul == priv->slice_pan_id)
    {
        const char *c = strstr(sub, " center=");

        if (c != NULL)
        {
            priv->pan_center_hz = strtod(c + 8, NULL) * 1e6;
        }
    }

    /* A spectrum line reports the panadapter's own span and level range, and
     * the operator can change either while it is being read. */
    if (priv->spectrum_pan_id != 0U && (uint32_t)pan_ul == priv->spectrum_pan_id)
    {
        const char *center = strstr(sub, " center=");
        const char *bandwidth = strstr(sub, " bandwidth=");
        const char *min_dbm = strstr(sub, " min_dbm=");
        const char *max_dbm = strstr(sub, " max_dbm=");
        const char *y_pixels = strstr(sub, " y_pixels=");

        if (center != NULL)
        {
            priv->spectrum_center_hz = strtod(center + 8, NULL) * 1e6;
        }

        if (bandwidth != NULL)
        {
            priv->spectrum_span_hz = strtod(bandwidth + 11, NULL) * 1e6;
        }

        if (min_dbm != NULL)
        {
            priv->spectrum_min_dbm = strtod(min_dbm + 9, NULL);
        }

        if (max_dbm != NULL)
        {
            priv->spectrum_max_dbm = strtod(max_dbm + 9, NULL);
        }

        if (y_pixels != NULL)
        {
            priv->spectrum_height = (int)strtol(y_pixels + 10, NULL, 10);
        }
    }

    {
        int pan_matches = (priv->slice_pan_id != 0U
                           && (uint32_t)pan_ul == priv->slice_pan_id);
        int bind_matches = (priv->pan_waterfall_bind_pan_id != 0U
                            && (uint32_t)pan_ul
                            == priv->pan_waterfall_bind_pan_id);
        int infer_pan = (priv->slice_pan_id == 0U
                         && priv->pan_waterfall_bind_pan_id == 0U);

        if (!pan_matches && !bind_matches && !infer_pan)
        {
            return;
        }

        if (infer_pan)
        {
            priv->slice_pan_id = (uint32_t)pan_ul;
        }
    }

    wf = strstr(line, "waterfall=");

    if (wf == NULL)
    {
        return;
    }

    wf_ul = strtoul(wf + 10, &endp, 0);

    if (endp == wf + 10 || wf_ul == 0UL)
    {
        return;
    }

    priv->slice_waterfall_id = (uint32_t)wf_ul;
    rig_debug(RIG_DEBUG_VERBOSE, "%s: pan=0x%lx waterfall=0x%lx\n",
              __func__, pan_ul, wf_ul);
}


int smartsdr_parse_status_line(RIG *rig, char *line)
{
    return smartsdr_parse_S(rig, line);
}


/* Meter status lines name the radio's meters and give each an index, which is
 * the ID carried in the meter data packets. Fields are separated by '#' and
 * each is prefixed by the index it belongs to, and one line may describe
 * several meters:
 *
 *   S<handle>|meter 16.src=SLC#16.nam=LEVEL#16.unit=dBm#16.fps=10#17.src=...
 *
 * Indices differ between radios, firmware and even sessions, so the meters of
 * interest are matched by name and their indices recorded. */
static void smartsdr_absorb_meter_status(struct smartsdr_priv_data *priv,
        const char *s)
{
    static const struct { const char *nam; int slot; } wanted[] =
    {
        { "LEVEL",     SMARTSDR_MTR_LEVEL  },
        { "SWR",       SMARTSDR_MTR_SWR    },
        { "ALC",       SMARTSDR_MTR_ALC    },
        { "FWDPWR",    SMARTSDR_MTR_FWDPWR },
        { "PATEMP",    SMARTSDR_MTR_PATEMP },
        { "+13.8B",    SMARTSDR_MTR_VOLTS  },
        { "PACURRENT", SMARTSDR_MTR_AMPS   },
    };
    const char *line = strstr(s, "|meter ");
    const char *nam;

    if (line == NULL)
    {
        return;
    }

    for (nam = strstr(line, ".nam="); nam != NULL;
            nam = strstr(nam + 5, ".nam="))
    {
        const char *digits = nam;
        const char *value = nam + 5;
        char fps_key[24];
        const char *fps;
        long idx;
        size_t i;

        /* The index is the run of digits immediately before the '.'. */
        while (digits > line && isdigit((unsigned char)digits[-1]))
        {
            digits--;
        }

        if (digits == nam)
        {
            continue;
        }

        idx = strtol(digits, NULL, 10);

        if (idx < 0 || idx > INT16_MAX)
        {
            continue;
        }

        for (i = 0; i < sizeof(wanted) / sizeof(wanted[0]); i++)
        {
            size_t len = strlen(wanted[i].nam);

            /* The value runs to the next '#', so compare only that far. */
            if (strncmp(value, wanted[i].nam, len) != 0
                    || (value[len] != '#' && value[len] != '\0'
                        && value[len] != '\n' && value[len] != ' '))
            {
                continue;
            }

            /* The radio sends a meter on a timer at this rate, or only as it
             * changes when the rate is zero. */
            snprintf(fps_key, sizeof(fps_key), "%ld.fps=", idx);
            fps = strstr(line, fps_key);
            priv->meter_fps[wanted[i].slot] =
                fps ? (int16_t)strtol(fps + strlen(fps_key), NULL, 10) : 0;

            /* The wire id is what marks the meter resolved, so it is written
             * last: whoever sees it has the rate that belongs with it. */
            priv->meter_wire_id[wanted[i].slot] = (int16_t)idx;
            break;
        }
    }
}


/* Apply one status line to the model. The control thread is the only caller,
 * and it holds the state lock for exactly this: one line in, then out again,
 * so a reader never waits for more than a line's worth of parsing. The
 * frontend cache is mirrored afterwards, outside the lock, because that call
 * takes locks of its own. */
static int smartsdr_parse_S(RIG *rig, char *s)
{
    struct smartsdr_priv_data *priv = (struct smartsdr_priv_data *)STATE(rig)->priv;
    char slice_tag[24];
    const char *slicep;
    const char *p;
    freq_t cache_freq = 0;
    rmode_t cache_mode = RIG_MODE_NONE;
    pbwidth_t cache_width = 0;
    int mirror_freq = 0;
    int mirror_mode = 0;

    smartsdr_state_lock(priv);

    smartsdr_absorb_display_pan_status(rig, s);
    smartsdr_absorb_meter_status(priv, s);

    /* The radio object says which oscillator it has, which is what decides
     * whether its timestamps are traceable to UTC or merely stable. */
    if (strstr(s, "|radio ") != NULL)
    {
        const char *v;

        v = strstr(s, "gpsdo_present=");

    if (v != NULL)
        {
            priv->gpsdo_present = (uint8_t)(strtol(v + 14, NULL, 10) != 0);
        }

        v = strstr(s, "tcxo_present=");

    if (v != NULL)
        {
            priv->tcxo_present = (uint8_t)(strtol(v + 13, NULL, 10) != 0);
        }
    }

    /* "transmit band <n>" lines are per-band presets, each carrying its own
     * rfpower; they describe a different object from the live settings. */
    {
        const char *transmit = strstr(s, "|transmit ");
        const char *transmit_band = strstr(s, "|transmit band ");

        if (transmit != NULL && transmit_band == NULL)
        {
            smartsdr_props_absorb(priv, SMARTSDR_OBJ_TRANSMIT, s);
        }
    }

    /* The interlock object carries the radio-wide transmit state. */
    if (strstr(s, "|interlock ") != NULL)
    {
        smartsdr_props_absorb(priv, SMARTSDR_OBJ_INTERLOCK, s);
    }

    slicep = strstr(s, "|slice ");

    if (slicep != NULL)
    {
        /* Slices belong to the radio, so track every slice well enough to
         * answer split queries, not just the one this rig is bound to. */
        smartsdr_slice_info_absorb(priv, (int)strtol(slicep + 7, NULL, 10), s);
    }

    snprintf(slice_tag, sizeof(slice_tag), "|slice %d ", priv->slicenum);

    if (strstr(s, slice_tag) == NULL)
    {
        smartsdr_state_unlock(priv);
        return RIG_OK;
    }

    smartsdr_props_absorb(priv, SMARTSDR_OBJ_SLICE, s);

    /* Use " pan=" so audio_pan=... is not mistaken for pan=. */
    p = strstr(s, " pan=");

    if (p != NULL)
    {
        uint32_t np = (uint32_t)strtoul(p + 5, NULL, 0);

        if (np != priv->slice_pan_id)
        {
            priv->slice_waterfall_id = 0U;
        }

        priv->slice_pan_id = np;
    }

    p = strstr(s, "client_handle=");

    if (p != NULL)
    {
        priv->client_handle = (uint32_t)strtoul(p + 14, NULL, 0);
    }

    if (priv->props[SMARTSDR_P_RF_FREQUENCY].seen)
    {
        mirror_freq = 1;
        cache_freq = smartsdr_slice_freq(priv);
    }

    if (priv->props[SMARTSDR_P_MODE].seen)
    {
        mirror_mode = 1;
        cache_mode = smartsdr_slice_mode(priv);
        cache_width = smartsdr_slice_width(priv);
    }

    smartsdr_state_unlock(priv);

    /* Mirror into the frontend cache so cached reads agree with the backend. */
    if (mirror_freq)
    {
        rig_set_cache_freq(rig, RIG_VFO_A, cache_freq);
    }

    if (mirror_mode)
    {
        rig_set_cache_mode(rig, RIG_VFO_A, cache_mode, cache_width);
    }

    return RIG_OK;
}


/* Rebuild the control session after the radio came back. Streams are not
 * restored: their stream IDs died with the session and the radio has no memory
 * of them, so an application must reopen them. That is why they were failed
 * rather than silently stalled. */
static int smartsdr_reestablish(RIG *rig)
{
    struct smartsdr_priv_data *priv = (struct smartsdr_priv_data *)STATE(rig)->priv;
    hamlib_port_t *rp = RIGPORT(rig);
    int retval;

    /* The reader must not be looking at a descriptor that is about to be
     * closed and reused, so it is stopped for the swap and started again on
     * the new socket rather than made to tolerate one changing under it. */
    smartsdr_control_stop(rig);

    /* Replacing the socket has to exclude anyone mid-transaction, so hold the
     * same lock the transaction layer uses. It cannot be held across the
     * registration below, which takes it for every command. */
    pthread_mutex_lock(&priv->tcp_mutex);
    network_close(rp);
    retval = port_open(rp);
    pthread_mutex_unlock(&priv->tcp_mutex);

    if (retval != RIG_OK)
    {
        return retval;
    }

    /* Starting the reader also discards what was half-read from the dead
     * socket and gives the new session a fresh idea of when the radio was
     * last heard from. */
    retval = smartsdr_control_start(rig);

    if (retval != RIG_OK)
    {
        network_close(rp);
        return retval;
    }

    retval = smartsdr_register_session(rig);

    if (retval != RIG_OK)
    {
        smartsdr_control_stop(rig);
        network_close(rp);
        return retval;
    }

    /* The radio drops a client that stops pinging, so the rebuilt session
     * needs a keepalive. The one from the old session runs until the rig is
     * closed and pings the new socket just as well, so one is started here
     * only when there is none. */
    if (!priv->keepalive_running)
    {
        priv->keepalive_running = 1;

        if (pthread_create(&priv->keepalive_thread, NULL, smartsdr_keepalive,
                           rig) != 0)
        {
            priv->keepalive_running = 0;
            rig_debug(RIG_DEBUG_ERR, "%s: keepalive thread create failed\n",
                      __func__);
            /* The control thread this call started goes back with it, as on
             * the registration failure above. The reconnect loop retries and
             * would stop it then, but a function that reports failure should
             * not leave a thread running behind it. */
            smartsdr_control_stop(rig);
            return -RIG_EINTERNAL;
        }
    }

    smartsdr_query_info(rig);
    return RIG_OK;
}


static void *smartsdr_reconnect_loop(void *arg)
{
    RIG *rig = (RIG *)arg;
    struct smartsdr_priv_data *priv = (struct smartsdr_priv_data *)STATE(rig)->priv;
    int delay_ms = SMARTSDR_RECONNECT_MIN_MS;

    rig_debug(RIG_DEBUG_VERBOSE, "%s: reconnect thread started\n", __func__);

    while (!priv->stop_reconnect)
    {
        int64_t wake;

        if (!priv->session_lost)
        {
            hl_usleep(200 * 1000);
            continue;
        }

        rig_debug(RIG_DEBUG_WARN, "%s: reconnecting in %d ms\n", __func__,
                  delay_ms);

        /* Sleep in slices so closing the rig does not wait out the backoff. */
        wake = smartsdr_now_ms() + delay_ms;

        while (!priv->stop_reconnect && smartsdr_now_ms() < wake)
        {
            hl_usleep(100 * 1000);
        }

        if (priv->stop_reconnect)
        {
            break;
        }

        if (smartsdr_reestablish(rig) == RIG_OK)
        {
            rig_debug(RIG_DEBUG_VERBOSE, "%s: session re-established\n",
                      __func__);
            delay_ms = SMARTSDR_RECONNECT_MIN_MS;
            priv->session_lost_reason = RIG_COMM_REASON_NONE;
            priv->session_lost = 0;
            STATE(rig)->comm_reason = RIG_COMM_REASON_NONE;
            STATE(rig)->comm_status = RIG_COMM_STATUS_OK;
        }
        else
        {
            delay_ms *= 2;

            if (delay_ms > SMARTSDR_RECONNECT_MAX_MS)
            {
                delay_ms = SMARTSDR_RECONNECT_MAX_MS;
            }
        }
    }

    rig_debug(RIG_DEBUG_VERBOSE, "%s: reconnect thread exiting\n", __func__);
    return NULL;
}


/* ------------------------------------------------------------------ */
/* Background threads                                                  */
/* ------------------------------------------------------------------ */

int smartsdr_session_start_threads(RIG *rig)
{
    struct smartsdr_priv_data *priv = (struct smartsdr_priv_data *)STATE(rig)->priv;

    if (priv->auto_reconnect)
    {
        priv->stop_reconnect = 0;
        priv->reconnect_running = 1;

        if (pthread_create(&priv->reconnect_thread, NULL,
                           smartsdr_reconnect_loop, rig) != 0)
        {
            priv->reconnect_running = 0;
            rig_debug(RIG_DEBUG_ERR, "%s: reconnect thread create failed\n",
                      __func__);
        }
    }

    /* WAN / SmartLink-style registration (no-op on LAN until status says remote). */
    priv->wan_register_running = 1;

    if (pthread_create(&priv->wan_register_thread, NULL,
                       smartsdr_wan_register_loop, rig) != 0)
    {
        priv->wan_register_running = 0;
        rig_debug(RIG_DEBUG_ERR, "%s: WAN register thread create failed\n",
                  __func__);
    }

    /* Start keepalive thread */
    priv->keepalive_running = 1;
    int err = pthread_create(&priv->keepalive_thread, NULL,
                             smartsdr_keepalive, rig);

    if (err)
    {
        rig_debug(RIG_DEBUG_ERR, "%s: keepalive thread create failed: %s\n",
                  __func__, strerror(err));
        priv->keepalive_running = 0;

        /* Unwind the threads that did start. A failure here fails rig_open,
         * and Hamlib answers that by closing the port and returning -- it does
         * not call the backend's rig_close -- so anything left running outlives
         * the session it belongs to and holds a rig the application is then
         * free to clean up underneath it. */
        if (priv->wan_register_running)
        {
            priv->wan_register_running = 0;
            pthread_join(priv->wan_register_thread, NULL);
        }

        if (priv->reconnect_running)
        {
            priv->stop_reconnect = 1;
            priv->reconnect_running = 0;
            pthread_join(priv->reconnect_thread, NULL);
        }

        return -RIG_EINTERNAL;
    }

    return RIG_OK;
}


void smartsdr_session_stop_reconnect(RIG *rig)
{
    struct smartsdr_priv_data *priv = (struct smartsdr_priv_data *)STATE(rig)->priv;

    if (priv->reconnect_running)
    {
        priv->stop_reconnect = 1;
        pthread_join(priv->reconnect_thread, NULL);
        priv->reconnect_running = 0;
    }
}


void smartsdr_session_stop_threads(RIG *rig)
{
    struct smartsdr_priv_data *priv = (struct smartsdr_priv_data *)STATE(rig)->priv;

    if (priv->keepalive_running)
    {
        priv->keepalive_running = 0;
        pthread_join(priv->keepalive_thread, NULL);
    }

    if (priv->wan_register_running)
    {
        priv->wan_register_running = 0;
        pthread_join(priv->wan_register_thread, NULL);
    }
}
