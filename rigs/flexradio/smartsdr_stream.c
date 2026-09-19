/*
 *  Hamlib SmartSDR VITA-49 streaming
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

/* VITA-49 streaming codec and thread management for SmartSDR radios. */
/* Translates between VITA-49 UDP packets and Hamlib ring buffers. */

#ifdef HAVE_CONFIG_H
#  include "config.h"
#endif

#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <unistd.h>


#include <hamlib/rig.h>
#include "hamlib/port.h"
#include "hamlib/rig_state.h"
#include "misc.h"
#include "event.h"
#include "network.h"
#include "stream.h"
#include "stream_proto.h"
#include "stream_time.h"
#include "smartsdr_props.h"
#include "smartsdr_session.h"
#include "smartsdr_slice.h"
#include "smartsdr_stream.h"

/* VITA-49 TSI codes (timestamp-integer type) */
#define VITA49_TSI_NONE  0
#define VITA49_TSI_UTC   1
#define VITA49_TSI_GPS   2
#define VITA49_TSI_OTHER 3

/* VITA-49 TSF codes (timestamp-fractional type) */
#define VITA49_TSF_REAL_TIME_PS 2

/* RX: host-clock anchor cadence in packets when the radio provides no
 * absolute time (default DAX: TSI=OTHER, sample-count fractional) */
#define SMARTSDR_HOST_ANCHOR_EVERY_PKTS 64

/* TX: a timed burst further past due than this counts as late */
#define SMARTSDR_TX_LATE_TOLERANCE_MS 50

/* Radio UDP port for the source-IP discovery nudge and client udpport. */
#define SMARTSDR_DISCOVERY_PORT 4992

/* FlexRadio OUI, upper 32 bits of every VITA-49 class ID. */
#define SMARTSDR_FLEX_OUI 0x00001C2D


/* Honour the transmit targets due within the next frame.
 *
 * A timed target gates play-out until its instant: the thread sleeps in short
 * steps so a close while it waits is still noticed. A target already past the
 * tolerance is not delayed -- it is transmitted at once and recorded as late,
 * because holding audio back to punish it would only make it later. SOB and
 * EOB key and unkey PTT for backends that advertise CAP_BURST_PTT. */
static void smartsdr_tx_service_targets(struct smartsdr_stream_state *ss,
                                        struct rig_stream *stream)
{
    struct rig_stream_tx_target tgt;

    while (rig_stream_pop_tx_target(stream,
                                    ss->tx_consumed + SMARTSDR_TX_STEREO_PAIRS,
                                    &tgt))
    {
        if (tgt.flags & RIG_STREAM_TIME_FLAG_TX_TIMED)
        {
            int64_t now_s;
            uint64_t now_ps;
            int64_t wait_ms;

            stream_time_now(&now_s, &now_ps);
            wait_ms = stream_time_diff_ms(tgt.seconds, tgt.picoseconds,
                                          now_s, now_ps);

            if (wait_ms < -SMARTSDR_TX_LATE_TOLERANCE_MS)
            {
                struct rig_stream_write_status ev;

                memset(&ev, 0, sizeof(ev));
                ev.event = RIG_STREAM_WRITE_EVENT_LATE;
                ev.sample_index = rig_stream_get_samples_written(stream);
                ev.lateness = (int64_t)(-wait_ms)
                              * (int64_t)stream->backend_config.sample_rate / 1000;
                ev.time_valid = 1;
                ev.seconds = tgt.seconds;
                ev.picoseconds = tgt.picoseconds;
                ev.time_source = RIG_STREAM_TIME_SRC_HOST;
                ev.time_accuracy = RIG_STREAM_TIME_ACC_MS;
                stream_record_write_status(stream, &ev, 0);
            }

            while (wait_ms > 0 && ss->running)
            {
                hl_usleep(10 * 1000);
                stream_time_now(&now_s, &now_ps);
                wait_ms = stream_time_diff_ms(tgt.seconds, tgt.picoseconds,
                                              now_s, now_ps);
            }
        }

        if (ss->burst_ptt && (tgt.flags & RIG_STREAM_TIME_FLAG_SOB))
        {
            rig_set_ptt(ss->rig, RIG_VFO_CURR, RIG_PTT_ON);
        }

        if (ss->burst_ptt && (tgt.flags & RIG_STREAM_TIME_FLAG_EOB))
        {
            rig_set_ptt(ss->rig, RIG_VFO_CURR, RIG_PTT_OFF);
        }
    }
}


static void *smartsdr_tx_thread(void *arg);
static int smartsdr_udp_open(RIG *rig);
static void smartsdr_udp_close(RIG *rig);
static void *smartsdr_dispatch_thread(void *arg);


/* Drop this stream's reference to the shared socket. */
static void smartsdr_stream_release_udp(RIG *rig)
{
    struct smartsdr_priv_data *priv = (struct smartsdr_priv_data *)STATE(rig)->priv;

    pthread_mutex_lock(&priv->stream_lock);
    smartsdr_udp_close(rig);
    pthread_mutex_unlock(&priv->stream_lock);
}
static int parse_stream_id(const char *resp, uint32_t *stream_id);
static void parse_pan_create_waterfall(const char *resp,
                                       uint32_t *waterfall_id);
static int smartsdr_local_ip_for_radio(RIG *rig, char *ipstr, size_t ipstrlen);


/* ------------------------------------------------------------------ */
/* Local IPv4 used to reach the radio (for stream create ip= port=).   */
/* ------------------------------------------------------------------ */

static int smartsdr_local_ip_for_radio(RIG *rig, char *ipstr, size_t ipstrlen)
{
    hamlib_port_t *rp = RIGPORT(rig);
    const char *pathname = rp->pathname;
    const char *colon;
    char host[64];
    size_t hl;
    struct sockaddr_in ra;
    struct sockaddr_in la;
    socklen_t lalen = (socklen_t)sizeof(la);
    int fd;
    int port = SMARTSDR_DISCOVERY_PORT;

    if (pathname == NULL || ipstr == NULL
            || ipstrlen < (size_t)INET_ADDRSTRLEN)
    {
        return -1;
    }

    colon = strchr(pathname, ':');
    hl = colon ? (size_t)(colon - pathname) : strlen(pathname);

    if (hl >= sizeof(host))
    {
        hl = sizeof(host) - 1;
    }

    memcpy(host, pathname, hl);
    host[hl] = 0;

    if (colon != NULL && colon[1] != 0)
    {
        int p = atoi(colon + 1);

        if (p > 0 && p <= 65535)
        {
            port = p;
        }
    }

    fd = socket(AF_INET, SOCK_DGRAM, 0);

    if (fd < 0)
    {
        return -1;
    }

    memset(&ra, 0, sizeof(ra));
    ra.sin_family = AF_INET;
    ra.sin_port = htons((uint16_t)port);

    if (inet_pton(AF_INET, host, &ra.sin_addr) != 1)
    {
        socket_close(fd);
        return -1;
    }

    if (connect(fd, (struct sockaddr *)&ra, sizeof(ra)) < 0)
    {
        socket_close(fd);
        return -1;
    }

    if (getsockname(fd, (struct sockaddr *)&la, &lalen) != 0)
    {
        socket_close(fd);
        return -1;
    }

    socket_close(fd);

    if (inet_ntop(AF_INET, &la.sin_addr, ipstr, (socklen_t)ipstrlen) == NULL)
    {
        return -1;
    }

    return 0;
}


/* ------------------------------------------------------------------ */
/* DAXIQ pan bandwidth (MHz) vs requested sample rate (Hz)              */
/* ------------------------------------------------------------------ */

/* Only an explicit nat_traversal opt-in selects udp_register. The radio's
 * remote_on_enabled flag describes its SmartLink configuration, not this
 * connection, so it says nothing about whether a NAT sits in the path. */
static int smartsdr_priv_wants_wan_udp(const struct smartsdr_priv_data *priv)
{
    return priv->nat_traversal;
}


static double smartsdr_iq_bandwidth_mhz(int sample_rate)
{
    if (sample_rate <= 24000)
    {
        return 0.024;
    }

    if (sample_rate <= 48000)
    {
        return 0.048;
    }

    if (sample_rate <= 96000)
    {
        return 0.096;
    }

    return 0.192;
}


/* ------------------------------------------------------------------ */
/* Stream open helpers                                                 */
/* ------------------------------------------------------------------ */

/* Resolve the radio's UDP address from the rig pathname ("host[:port]") and
 * store it in priv->radio_udp_addr at the VITA data port. */
static int smartsdr_resolve_radio_udp_addr(RIG *rig)
{
    struct smartsdr_priv_data *priv = (struct smartsdr_priv_data *)STATE(rig)->priv;
    char ip_buf[64];
    const char *pathname = RIGPORT(rig)->pathname;
    const char *colon = strchr(pathname, ':');
    size_t ip_len = colon ? (size_t)(colon - pathname) : strlen(pathname);

    if (ip_len >= sizeof(ip_buf))
    {
        ip_len = sizeof(ip_buf) - 1;
    }

    memcpy(ip_buf, pathname, ip_len);
    ip_buf[ip_len] = 0;

    memset(&priv->radio_udp_addr, 0, sizeof(priv->radio_udp_addr));
    priv->radio_udp_addr.sin_family = AF_INET;
    priv->radio_udp_addr.sin_port =
        htons((uint16_t)(priv->vita_port > 0 ? priv->vita_port
                         : SMARTSDR_VITA_UDP_PORT));

    if (inet_pton(AF_INET, ip_buf, &priv->radio_udp_addr.sin_addr) != 1)
    {
        rig_debug(RIG_DEBUG_ERR, "%s: invalid radio IP: %s\n",
                  __func__, ip_buf);
        return -RIG_EINVAL;
    }

    return RIG_OK;
}


void smartsdr_send_udp_nudge(RIG *rig)
{
    struct smartsdr_priv_data *priv = (struct smartsdr_priv_data *)STATE(rig)->priv;
    struct sockaddr_in radio_addr;
    uint8_t nudge = 0x00;

    memset(&radio_addr, 0, sizeof(radio_addr));
    radio_addr.sin_family = AF_INET;
    radio_addr.sin_port = htons(SMARTSDR_DISCOVERY_PORT);
    radio_addr.sin_addr = priv->radio_udp_addr.sin_addr;

    if (sendto(priv->udp_sock, (char *)&nudge, 1, 0,
               (struct sockaddr *)&radio_addr, sizeof(radio_addr)) < 0)
    {
        rig_debug(RIG_DEBUG_WARN, "%s: UDP nudge sendto failed: %s\n",
                  __func__, strerror(errno));
    }
}


/* Make the radio learn our source IP:port for the VITA stream. Sends the 1-byte
 * nudge, then registers the endpoint: nat_traversal uses client udp_register so
 * the radio learns a NAT-translated address; otherwise client udpport names the
 * local port directly. Errors here are non-fatal: some firmware rejects the
 * registration but still streams. */
static void smartsdr_register_udp_port(RIG *rig,
                                       struct smartsdr_priv_data *priv,
                                       int is_iq)
{
    char cmd[128];
    char resp[SMARTSDR_RESP_LEN];
    int retval;

    smartsdr_send_udp_nudge(rig);

    if (is_iq)
    {
        (void)smartsdr_transaction_resp(rig, "ping", resp, sizeof(resp));
    }

    if (smartsdr_priv_wants_wan_udp(priv) && priv->client_handle != 0U)
    {
        snprintf(cmd, sizeof(cmd), "client udp_register handle=0x%x",
                 (unsigned)priv->client_handle);
        retval = smartsdr_transaction_resp(rig, cmd, resp, sizeof(resp));

        if (retval != 0)
        {
            rig_debug(RIG_DEBUG_VERBOSE,
                      "%s: client udp_register returned %d (continuing)\n",
                      __func__, retval);
        }
    }
    else
    {
        snprintf(cmd, sizeof(cmd), "client udpport %d", priv->udp_port);
        retval = smartsdr_transaction_resp(rig, cmd, resp, sizeof(resp));

        if (retval != 0)
        {
            /* client udpport error is expected on some firmware — continue */
            rig_debug(RIG_DEBUG_VERBOSE,
                      "%s: client udpport returned error (expected on some FW)\n",
                      __func__);
        }
    }
}


/* Find or create the panadapter that the DAXIQ RX stream binds to, and record
 * its ID and ownership in ss. Returns RIG_OK or a negative error code. */
static int smartsdr_setup_panadapter(RIG *rig,
                                     struct smartsdr_priv_data *priv,
                                     struct smartsdr_stream_state *ss)
{
    struct rig_stream *stream = ss->stream;
    char cmd[128];
    char resp[SMARTSDR_RESP_LEN];
    int retval;
    uint32_t pan_id = 0;

    if (priv->slice_pan_id == 0U)
    {
        (void)smartsdr_sync_display_slice_status(rig);
    }

    if (priv->slice_pan_id != 0U)
    {
        pan_id = priv->slice_pan_id;
        ss->pan_created_by_us = 0;
    }
    else
    {
        double center_mhz;
        double bw_mhz;
        freq_t f_hz;

        f_hz = smartsdr_slice_freq(priv);

        if (f_hz <= 0.0) { f_hz = (freq_t)14000000.0; }

        center_mhz = (double)f_hz / 1e6;
        bw_mhz = smartsdr_iq_bandwidth_mhz(stream->backend_config.sample_rate);

        snprintf(cmd, sizeof(cmd),
                 "display pan c freq=%.6f ant=ANT1 x=100 y=100",
                 center_mhz);
        retval = smartsdr_transaction_resp(rig, cmd, resp, sizeof(resp));

        if (retval != 0)
        {
            snprintf(cmd, sizeof(cmd),
                     "display pan create x=100 y=100 center=%.6f "
                     "bandwidth=%.6f",
                     center_mhz, bw_mhz);
            retval = smartsdr_transaction_resp(rig, cmd, resp, sizeof(resp));
        }

        if (retval != 0)
        {
            (void)smartsdr_sync_display_slice_status(rig);

            if (priv->slice_pan_id != 0U)
            {
                pan_id = priv->slice_pan_id;
                ss->pan_created_by_us = 0;
            }
            else
            {
                rig_debug(RIG_DEBUG_ERR,
                          "%s: display pan create failed: %d\n",
                          __func__, retval);
                return -RIG_EIO;
            }
        }
        else
        {
            if (parse_stream_id(resp, &pan_id) < 0)
            {
                rig_debug(RIG_DEBUG_ERR,
                          "%s: failed to parse pan ID from: %s\n",
                          __func__, resp);
                return -RIG_EPROTO;
            }

            {
                uint32_t wf_from_r = 0U;

                parse_pan_create_waterfall(resp, &wf_from_r);

                if (wf_from_r != 0U)
                {
                    priv->slice_waterfall_id = wf_from_r;
                }
            }

            ss->pan_created_by_us = 1;
        }
    }

    ss->pan_id = pan_id;
    priv->pan_waterfall_bind_pan_id = 0U;

    if (ss->pan_created_by_us != 0)
    {
        priv->pan_waterfall_bind_pan_id = ss->pan_id;
    }

    if (ss->pan_id != 0U && priv->slice_waterfall_id == 0U
            && ss->pan_created_by_us == 0
            && priv->slice_pan_id == ss->pan_id)
    {
        snprintf(cmd, sizeof(cmd), "display pan set 0x%x fps=10",
                 ss->pan_id);
        smartsdr_command_or_warn(rig, cmd);
    }

    return RIG_OK;
}


/* ------------------------------------------------------------------ */
/* Backend stream callbacks                                            */
/* ------------------------------------------------------------------ */

/* Ask the radio for a DAX-IQ stream and the panadapter it is referenced to.
 * The caller owns ss and releases it, so failures here just report. */
/* Undo the panadapter this open created, if it created one. Every failure
 * after the panadapter exists has to do this, and doing it in three places by
 * hand is how one of them ends up different from the others. */
static void smartsdr_iq_drop_our_pan(RIG *rig, struct smartsdr_priv_data *priv,
                                     struct smartsdr_stream_state *ss,
                                     int is_rx)
{
    char cmd[128];
    char resp[SMARTSDR_RESP_LEN];

    if (is_rx && ss->pan_created_by_us != 0 && ss->pan_id != 0)
    {
        snprintf(cmd, sizeof(cmd), "display pan remove 0x%x", ss->pan_id);
        (void)smartsdr_transaction_resp(rig, cmd, resp, sizeof(resp));
    }

    priv->pan_waterfall_bind_pan_id = 0U;
}


/* Ask the radio for the DAX-IQ stream. Four spellings, oldest firmware last:
 * the modern "type=dax_iq" form with and without an explicit endpoint, then
 * the legacy "daxiq=" form the same two ways. Only the modern form understands
 * daxiq_rate, so which one answered decides whether the rate is sent. */
static int smartsdr_iq_create_stream(RIG *rig, struct smartsdr_priv_data *priv,
                                     struct smartsdr_stream_state *ss,
                                     int is_rx)
{
    struct rig_stream *stream = ss->stream;
    char cmd[128];
    char resp[SMARTSDR_RESP_LEN];
    char lip[INET_ADDRSTRLEN];
    int have_lip = (smartsdr_local_ip_for_radio(rig, lip, sizeof(lip)) == 0);
    int modern_dax_iq = 0;
    int retval = -RIG_EPROTO;

    if (have_lip)
    {
        snprintf(cmd, sizeof(cmd),
                 "stream create type=dax_iq daxiq_channel=%d ip=%s port=%d",
                 ss->dax_channel, lip, priv->udp_port);
        retval = smartsdr_transaction_resp(rig, cmd, resp, sizeof(resp));
    }

    if (retval != RIG_OK)
    {
        snprintf(cmd, sizeof(cmd),
                 "stream create type=dax_iq daxiq_channel=%d",
                 ss->dax_channel);
        retval = smartsdr_transaction_resp(rig, cmd, resp, sizeof(resp));
    }

    if (retval == RIG_OK)
    {
        modern_dax_iq = 1;
    }
    else if (have_lip)
    {
        snprintf(cmd, sizeof(cmd), "stream create daxiq=%d ip=%s port=%d",
                 ss->dax_channel, lip, priv->udp_port);
        retval = smartsdr_transaction_resp(rig, cmd, resp, sizeof(resp));
    }

    if (retval != RIG_OK)
    {
        snprintf(cmd, sizeof(cmd), "stream create daxiq=%d", ss->dax_channel);
        retval = smartsdr_transaction_resp(rig, cmd, resp, sizeof(resp));
    }

    if (retval != 0)
    {
        rig_debug(RIG_DEBUG_ERR, "%s: stream create dax_iq failed: %d\n",
                  __func__, retval);
        smartsdr_iq_drop_our_pan(rig, priv, ss, is_rx);
        return -RIG_EIO;
    }

    if (parse_stream_id(resp, &ss->vita_stream_id) < 0)
    {
        rig_debug(RIG_DEBUG_ERR,
                  "%s: failed to parse DAXIQ stream ID from: %s\n",
                  __func__, resp);
        smartsdr_iq_drop_our_pan(rig, priv, ss, is_rx);
        return -RIG_EPROTO;
    }

    if (modern_dax_iq)
    {
        snprintf(cmd, sizeof(cmd), "stream set 0x%x daxiq_rate=%d",
                 ss->vita_stream_id, stream->backend_config.sample_rate);
        smartsdr_command_or_warn(rig, cmd);
    }

    return RIG_OK;
}

/* Point the DAX-IQ channel at the panadapter feeding it. Firmware disagrees on
 * which object owns that binding, so the panafall is tried first, then the
 * waterfall, then the panadapter under each of its two spellings. Failing all
 * four leaves a stream nothing will ever fill, so the stream goes back too. */
static int smartsdr_iq_bind_to_pan(RIG *rig, struct smartsdr_priv_data *priv,
                                   struct smartsdr_stream_state *ss)
{
    struct rig_stream *stream = ss->stream;
    char cmd[128];
    char resp[SMARTSDR_RESP_LEN];
    uint32_t waterfall_id = priv->slice_waterfall_id;
    int retval = -RIG_EPROTO;

    if (waterfall_id != 0U)
    {
        snprintf(cmd, sizeof(cmd),
                 "display panafall set 0x%x daxiq_channel=%d",
                 waterfall_id, ss->dax_channel);
        retval = smartsdr_transaction_resp(rig, cmd, resp, sizeof(resp));
    }

    if (retval != RIG_OK && waterfall_id != 0U)
    {
        snprintf(cmd, sizeof(cmd),
                 "display waterfall set 0x%x daxiq_channel=%d",
                 waterfall_id, ss->dax_channel);
        retval = smartsdr_transaction_resp(rig, cmd, resp, sizeof(resp));
        rig_debug(RIG_DEBUG_VERBOSE,
                  "%s: panafall daxiq bind failed, tried waterfall: %d\n",
                  __func__, retval);
    }

    if (retval != RIG_OK)
    {
        snprintf(cmd, sizeof(cmd), "display pan set 0x%x daxiq_channel=%d",
                 ss->pan_id, ss->dax_channel);
        retval = smartsdr_transaction_resp(rig, cmd, resp, sizeof(resp));
    }

    if (retval != RIG_OK)
    {
        snprintf(cmd, sizeof(cmd), "display pan set 0x%x daxiq=%d",
                 ss->pan_id, ss->dax_channel);
        retval = smartsdr_transaction_resp(rig, cmd, resp, sizeof(resp));
    }

    if (retval != RIG_OK)
    {
        rig_debug(RIG_DEBUG_ERR,
                  "%s: DAXIQ bind failed (panafall/waterfall "
                  "and pan fallback): %d\n",
                  __func__, retval);
        snprintf(cmd, sizeof(cmd), "stream remove 0x%x", ss->vita_stream_id);
        (void)smartsdr_transaction_resp(rig, cmd, resp, sizeof(resp));
        smartsdr_iq_drop_our_pan(rig, priv, ss, 1);
        return -RIG_EIO;
    }

    priv->pan_waterfall_bind_pan_id = 0U;

    /* Newer firmware wants the channel claimed by client handle as well; older
     * firmware ignores it, so a failure here is not one. */
    if (priv->client_handle != 0U && ss->pan_id != 0U)
    {
        snprintf(cmd, sizeof(cmd),
                 "dax iq s %d pan=0x%x daxiq_rate=%d client_handle=0x%x",
                 ss->dax_channel, (unsigned)ss->pan_id,
                 stream->backend_config.sample_rate,
                 (unsigned)priv->client_handle);
        smartsdr_command_or_warn(rig, cmd);
    }

    return RIG_OK;
}


static int smartsdr_open_iq_stream(RIG *rig, struct smartsdr_priv_data *priv,
                                   struct smartsdr_stream_state *ss, int is_rx)
{
    char cmd[128];
    int retval;

    if (is_rx)
    {
        retval = smartsdr_setup_panadapter(rig, priv, ss);

        if (retval != RIG_OK)
        {
            return retval;
        }
    }

    /* Official API: slice set dax=<ch> assigns DAX audio/IQ routing for slice. */
    snprintf(cmd, sizeof(cmd), "slice set %d dax=%d", priv->slicenum,
             ss->dax_channel);
    smartsdr_command_or_warn(rig, cmd);

    retval = smartsdr_iq_create_stream(rig, priv, ss, is_rx);

    if (retval != RIG_OK)
    {
        return retval;
    }

    if (is_rx)
    {
        return smartsdr_iq_bind_to_pan(rig, priv, ss);
    }

    return RIG_OK;
}


/* Ask the radio for a DAX audio stream and point the transmitter at it.
 * The caller owns ss and releases it, so failures here just report. */
static int smartsdr_open_audio_stream(RIG *rig, struct smartsdr_priv_data *priv,
                                      struct smartsdr_stream_state *ss, int is_rx)
{
    char cmd[128];
    char resp[SMARTSDR_RESP_LEN];
    int retval;

    if (is_rx)
    {
        /* RX audio: create remote_audio_rx stream (returns stream ID in body) */
        snprintf(cmd, sizeof(cmd),
                 "stream create type=remote_audio_rx compression=none");
    }
    else
    {
        /* TX audio: create dax_tx stream */
        snprintf(cmd, sizeof(cmd), "stream create type=dax_tx");
    }

    retval = smartsdr_transaction_resp(rig, cmd, resp, sizeof(resp));

    if (retval != 0)
    {
        rig_debug(RIG_DEBUG_ERR, "%s: stream create failed: %d\n",
                  __func__, retval);
        return -RIG_EIO;
    }

    if (parse_stream_id(resp, &ss->vita_stream_id) < 0)
    {
        rig_debug(RIG_DEBUG_ERR,
                  "%s: failed to parse audio stream ID from: %s\n",
                  __func__, resp);
        return -RIG_EPROTO;
    }

    /* For TX streams, enable metering in RX mode */
    if (!is_rx)
    {
        smartsdr_command_or_warn(rig, "transmit set met_in_rx=1");

        /* Creating the dax_tx stream only opens the pipe; the radio still
         * modulates from the microphone until told that DAX is the
         * transmit source. Without this the samples are carried and
         * discarded, which looks like a working transmit -- bytes sent,
         * no underruns -- while the radio puts out nothing. */
        smartsdr_command_or_warn(rig, "transmit set dax=1");
        priv->tx_dax_enabled = 1;
    }

    return RIG_OK;
}

int smartsdr_stream_open(RIG *rig, struct rig_stream *stream)
{
    struct smartsdr_priv_data *priv = (struct smartsdr_priv_data *)STATE(rig)->priv;
    struct smartsdr_stream_state *ss;
    int retval;
    /* stream.h is the backend-facing internal stream API, so reading
     * rig_stream fields directly here is intended, not a layering violation. */
    int is_iq = stream_type_is_iq(stream->type);
    int is_rx = stream_type_is_rx(stream->type);

    rig_debug(RIG_DEBUG_VERBOSE, "%s: type=%d is_iq=%d is_rx=%d\n",
              __func__, stream->type, is_iq, is_rx);

    ss = calloc(1, sizeof(*ss));

    if (!ss)
    {
        return -RIG_ENOMEM;
    }

    ss->stream = stream;
    ss->rig = rig;
    ss->dax_channel = priv->slicenum + 1;
    ss->burst_ptt = (stream->caps_flags & RIG_STREAM_CAP_BURST_PTT) != 0;

    /* Resolve the radio IP before UDP registration so the nudge reaches the
     * correct address. */
    retval = smartsdr_resolve_radio_udp_addr(rig);

    if (retval != RIG_OK)
    {
        free(ss);
        return retval;
    }

    /* One socket serves every stream on this rig; this may only take a
     * reference on one another stream already opened. */
    pthread_mutex_lock(&priv->stream_lock);
    retval = smartsdr_udp_open(rig);
    pthread_mutex_unlock(&priv->stream_lock);

    if (retval != RIG_OK)
    {
        free(ss);
        return retval;
    }

    smartsdr_register_udp_port(rig, priv, is_iq);

    retval = is_iq ? smartsdr_open_iq_stream(rig, priv, ss, is_rx)
             : smartsdr_open_audio_stream(rig, priv, ss, is_rx);

    if (retval != RIG_OK)
    {
        smartsdr_stream_release_udp(rig);
        free(ss);
        return retval;
    }

    rig_debug(RIG_DEBUG_VERBOSE, "%s: stream_id=0x%x dax=%d\n",
              __func__, ss->vita_stream_id, ss->dax_channel);

    ss->running = 1;

    if (is_rx)
    {
        /* RX has no thread of its own: the rig's dispatcher delivers packets
         * once the stream is in the table. */
        int slot;
        int placed = 0;

        pthread_mutex_lock(&priv->stream_lock);

        for (slot = 0; slot < SMARTSDR_MAX_RX_STREAMS; slot++)
        {
            if (priv->rx_streams[slot] == NULL)
            {
                priv->rx_streams[slot] = ss;
                placed = 1;
                break;
            }
        }

        pthread_mutex_unlock(&priv->stream_lock);

        if (!placed)
        {
            rig_debug(RIG_DEBUG_ERR, "%s: no free RX stream slot\n", __func__);
            ss->running = 0;
            priv->pan_waterfall_bind_pan_id = 0U;
            smartsdr_stream_release_udp(rig);
            free(ss);
            return -RIG_EINTERNAL;
        }
    }
    else
    {
        int err = pthread_create(&ss->thread, NULL, smartsdr_tx_thread, ss);

        if (err)
        {
            rig_debug(RIG_DEBUG_ERR, "%s: thread create failed: %s\n",
                      __func__, strerror(err));
            ss->running = 0;
            priv->pan_waterfall_bind_pan_id = 0U;
            smartsdr_stream_release_udp(rig);
            free(ss);
            return -RIG_EINTERNAL;
        }
    }

    priv->pan_waterfall_bind_pan_id = 0U;
    stream->backend_priv = ss;
    return RIG_OK;
}

int smartsdr_stream_close(RIG *rig, struct rig_stream *stream)
{
    struct smartsdr_priv_data *priv = (struct smartsdr_priv_data *)STATE(rig)->priv;
    struct smartsdr_stream_state *ss =
        (struct smartsdr_stream_state *)stream->backend_priv;
    char cmd[128];
    char resp[SMARTSDR_RESP_LEN];

    if (!ss)
    {
        return RIG_OK;
    }

    rig_debug(RIG_DEBUG_VERBOSE, "%s: closing stream_id=0x%x\n",
              __func__, ss->vita_stream_id);

    ss->running = 0;

    if (stream_type_is_rx(stream->type))
    {
        /* Out of the table before the state is freed, so the dispatcher can
         * never hand a packet to a dead stream. */
        int slot;

        pthread_mutex_lock(&priv->stream_lock);

        for (slot = 0; slot < SMARTSDR_MAX_RX_STREAMS; slot++)
        {
            if (priv->rx_streams[slot] == ss)
            {
                priv->rx_streams[slot] = NULL;
                break;
            }
        }

        pthread_mutex_unlock(&priv->stream_lock);
    }
    else
    {
        pthread_join(ss->thread, NULL);
    }

    /* Remove stream on radio */
    snprintf(cmd, sizeof(cmd), "stream remove 0x%x", ss->vita_stream_id);

    (void)smartsdr_transaction_resp(rig, cmd, resp, sizeof(resp));

    /* If we created a panadapter, remove it */
    if (ss->pan_created_by_us && ss->pan_id != 0)
    {
        snprintf(cmd, sizeof(cmd), "display pan remove 0x%x", ss->pan_id);
        (void)smartsdr_transaction_resp(rig, cmd, resp, sizeof(resp));
    }

    /* Hand the microphone back: leaving the radio on DAX would silence the
     * operator's own transmit once this stream is gone. */
    if (!stream_type_is_rx(stream->type) && priv->tx_dax_enabled)
    {
        (void)smartsdr_transaction_resp(rig, "transmit set dax=0", NULL, 0);
        priv->tx_dax_enabled = 0;

        /* Put the configured source back, in case it was not dax. */
        smartsdr_apply_tx_audio_source(rig);
    }

    smartsdr_stream_release_udp(rig);
    free(ss);
    stream->backend_priv = NULL;

    return RIG_OK;
}


/* The radio's own view of now, which is what RIG_STREAM_CAP_HW_TIME promises
 * is available. How much better than the host clock it is depends on what the
 * radio has fitted: a GPSDO stamps packets with a UTC-traceable time, while a
 * TCXO only disciplines the rate between anchors the host still places. The
 * classification is deliberately the same one the per-packet anchors use, so a
 * caller cannot see this call and the anchor stream disagree about the source.
 * Before the first stamped packet -- and on a TX stream, which receives none --
 * this is the host clock, reported honestly as such. */
int smartsdr_stream_hardware_time(RIG *rig, struct rig_stream *stream,
                                  struct rig_stream_time_anchor *now)
{
    struct smartsdr_priv_data *priv =
        (struct smartsdr_priv_data *)STATE(rig)->priv;
    struct smartsdr_stream_state *ss =
        (struct smartsdr_stream_state *)stream->backend_priv;
    int radio_stamped = 0;
    int timeline_seen = 0;

    memset(now, 0, sizeof(*now));
    now->sample_index = rig_stream_get_samples_written(stream);

    if (ss != NULL)
    {
        smartsdr_state_lock(priv);

        if (ss->hw_time_valid)
        {
            now->seconds = ss->hw_seconds;
            now->picoseconds = ss->hw_picoseconds;
            radio_stamped = 1;
        }

        timeline_seen = ss->timeline_seen;
        smartsdr_state_unlock(priv);
    }

    if (radio_stamped)
    {
        now->source = RIG_STREAM_TIME_SRC_GPS;
        now->flags = RIG_STREAM_TIME_FLAG_LOCKED
                     | RIG_STREAM_TIME_FLAG_SAMPLE_REFERENCED;
        now->accuracy = RIG_STREAM_TIME_ACC_100NS;
        return RIG_OK;
    }

    stream_time_now(&now->seconds, &now->picoseconds);

    if (priv->tcxo_present && timeline_seen)
    {
        now->source = RIG_STREAM_TIME_SRC_RADIO;
        now->accuracy = RIG_STREAM_TIME_ACC_US;
    }
    else
    {
        now->source = RIG_STREAM_TIME_SRC_HOST;
        now->accuracy = RIG_STREAM_TIME_ACC_MS;
    }

    return RIG_OK;
}


/* ------------------------------------------------------------------ */
/* UDP socket setup                                                    */
/* ------------------------------------------------------------------ */

/* Open the rig's one VITA socket and start the dispatcher, or just take a
 * reference if a stream already did. Call under priv->stream_lock. */
static int smartsdr_udp_open(RIG *rig)
{
    struct smartsdr_priv_data *priv = (struct smartsdr_priv_data *)STATE(rig)->priv;
    struct sockaddr_in addr;
#ifdef _WIN32
    DWORD tv;
#else
    struct timeval tv;
#endif
    socklen_t addrlen;
    int reuse = 1;

    /* A teardown that has already decided to close cannot be joined while it
     * holds the lock, so it drops it. Claiming the socket in that window
     * would hand this stream a descriptor the teardown is about to close, so
     * wait for it to finish and then open a fresh one. */
    while (priv->udp_closing)
    {
        pthread_cond_wait(&priv->udp_idle, &priv->stream_lock);
    }

    if (priv->udp_users > 0)
    {
        priv->udp_users++;
        return RIG_OK;
    }

    priv->udp_sock = socket(AF_INET, SOCK_DGRAM, 0);

    if (priv->udp_sock < 0)
    {
        rig_debug(RIG_DEBUG_ERR, "%s: socket() failed: %s\n",
                  __func__, strerror(errno));
        return -RIG_EIO;
    }

#ifdef SO_REUSEADDR

    if (setsockopt(priv->udp_sock, SOL_SOCKET, SO_REUSEADDR,
                   (const char *)&reuse, sizeof(reuse)) < 0)
    {
        rig_debug(RIG_DEBUG_WARN, "%s: setsockopt SO_REUSEADDR failed: %s\n",
                  __func__, strerror(errno));
    }

#endif

    memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = INADDR_ANY;
    /* LAN: bind the radio's VITA port when free (same as Flex client libs)
     * so the radio sends IQ/audio to the same port GUI clients use. The radio
     * holds it itself, so this normally fails and the ephemeral bind below is
     * what runs; see the vita_port token for why it is not simply 4991. */
    addr.sin_port = htons((uint16_t)(priv->vita_port > 0 ? priv->vita_port
                                     : SMARTSDR_VITA_UDP_PORT));

    if (bind(priv->udp_sock, (struct sockaddr *)&addr, sizeof(addr)) < 0)
    {
        addr.sin_port = 0;

        if (bind(priv->udp_sock, (struct sockaddr *)&addr, sizeof(addr)) < 0)
        {
            rig_debug(RIG_DEBUG_ERR, "%s: bind() failed: %s\n",
                      __func__, strerror(errno));
            socket_close(priv->udp_sock);
            priv->udp_sock = -1;
            return -RIG_EIO;
        }
    }

    addrlen = sizeof(addr);

    if (getsockname(priv->udp_sock, (struct sockaddr *)&addr, &addrlen) < 0)
    {
        rig_debug(RIG_DEBUG_ERR, "%s: getsockname() failed: %s\n",
                  __func__, strerror(errno));
        socket_close(priv->udp_sock);
        priv->udp_sock = -1;
        return -RIG_EIO;
    }

    priv->udp_port = ntohs(addr.sin_port);

    /* A bounded read lets the dispatcher notice a stop request and the
     * session-lost flag between datagrams. Winsock wants a DWORD of
     * milliseconds here rather than a struct timeval, and reads the zero
     * tv_sec of one as "wait forever", which strands the dispatch thread in
     * recvfrom() and hangs the join that stops it. */
#ifdef _WIN32
    tv = 100;
#else
    tv.tv_sec = 0;
    tv.tv_usec = 100000;
#endif

    if (setsockopt(priv->udp_sock, SOL_SOCKET, SO_RCVTIMEO,
                   (const char *)&tv, sizeof(tv)) < 0)
    {
        rig_debug(RIG_DEBUG_WARN, "%s: setsockopt SO_RCVTIMEO failed: %s\n",
                  __func__, strerror(errno));
    }

    priv->dispatch_running = 1;

    if (pthread_create(&priv->dispatch_thread, NULL, smartsdr_dispatch_thread,
                       rig) != 0)
    {
        rig_debug(RIG_DEBUG_ERR, "%s: dispatch thread create failed\n",
                  __func__);
        priv->dispatch_running = 0;
        socket_close(priv->udp_sock);
        priv->udp_sock = -1;
        return -RIG_EINTERNAL;
    }

    priv->udp_users = 1;
    rig_debug(RIG_DEBUG_VERBOSE, "%s: VITA socket on port %d\n", __func__,
              priv->udp_port);
    return RIG_OK;
}


/* Drop a reference; the last one out stops the dispatcher and closes the
 * socket. Call under priv->stream_lock. */
static void smartsdr_udp_close(RIG *rig)
{
    struct smartsdr_priv_data *priv = (struct smartsdr_priv_data *)STATE(rig)->priv;

    if (priv->udp_users == 0 || --priv->udp_users > 0)
    {
        return;
    }

    /* Announce the teardown before dropping the lock, so an opener waits for
     * it rather than taking a reference to a socket that is about to go. */
    priv->udp_closing = 1;
    priv->dispatch_running = 0;
    /* The dispatcher takes stream_lock to look streams up, so it cannot be
     * joined while holding it. */
    pthread_mutex_unlock(&priv->stream_lock);
    pthread_join(priv->dispatch_thread, NULL);
    pthread_mutex_lock(&priv->stream_lock);

    socket_close(priv->udp_sock);
    priv->udp_sock = -1;

    priv->udp_closing = 0;
    pthread_cond_broadcast(&priv->udp_idle);
}


/* ------------------------------------------------------------------ */
/* Response parsing                                                    */
/* ------------------------------------------------------------------ */

/* Parse hex stream ID from SmartSDR R-line response.
 * Format: "R<seq>|00000000|<hex_id>" or "R<seq>|0|<hex_id>\n" */
static int parse_stream_id(const char *resp, uint32_t *stream_id)
{
    const char *p;
    char *endp = NULL;
    unsigned long id;

    if (!resp || !stream_id)
    {
        return -1;
    }

    /* Find the third field after second '|' */
    p = strchr(resp, '|');

    if (!p)
    {
        return -1;
    }

    p = strchr(p + 1, '|');

    if (!p)
    {
        return -1;
    }

    p++;
    errno = 0;
    id = strtoul(p, &endp, 16);

    if (endp == p || errno != 0 || id > 0xFFFFFFFFUL)
    {
        return -1;
    }

    *stream_id = (uint32_t)id;
    return 0;
}


/* Pan create R-line: "R|0|0xPAN" or "R|0|0xPAN,0xWATERFALL" (Flex API). */
static void parse_pan_create_waterfall(const char *resp, uint32_t *waterfall_id)
{
    const char *p;
    char *endp = NULL;
    unsigned long wf_ul;

    if (!resp || !waterfall_id)
    {
        return;
    }

    *waterfall_id = 0U;
    p = strchr(resp, '|');

    if (p == NULL)
    {
        return;
    }

    p = strchr(p + 1, '|');

    if (p == NULL)
    {
        return;
    }

    p++;

    while (*p == ' ' || *p == '\t')
    {
        p++;
    }

    (void)strtoul(p, &endp, 0);

    if (endp != NULL && *endp == ',')
    {
        wf_ul = strtoul(endp + 1, &endp, 0);

        if (wf_ul != 0UL)
        {
            *waterfall_id = (uint32_t)wf_ul;
        }
    }
}


/* Meter packets carry a flat run of (uint16 id, int16 raw) pairs. Raw values
 * are fixed-point and the divisor depends on the meter's unit, measured
 * against known-good readings on hardware: an idle PA at 27.6 degC, a 14.03 V
 * supply, and an SWR of exactly 1.00 with no carrier. */
static void smartsdr_absorb_meter_packet(struct smartsdr_priv_data *priv,
        const uint8_t *payload,
        int payload_bytes)
{
    static const double divisor[SMARTSDR_MTR_COUNT] =
    {
        [SMARTSDR_MTR_LEVEL]  = 128.0,   /* dBm  */
        [SMARTSDR_MTR_SWR]    = 128.0,   /* SWR  */
        [SMARTSDR_MTR_ALC]    = 128.0,   /* dBFS */
        [SMARTSDR_MTR_FWDPWR] = 128.0,   /* dBm  */
        [SMARTSDR_MTR_PATEMP] =  64.0,   /* degC */
        [SMARTSDR_MTR_VOLTS]  = 256.0,   /* Volts */
        [SMARTSDR_MTR_AMPS]   = 256.0,   /* Amps */
    };
    int64_t now = smartsdr_now_ms();
    int off;

    smartsdr_state_lock(priv);

    for (off = 0; off + 4 <= payload_bytes; off += 4)
    {
        uint16_t id = (uint16_t)((payload[off] << 8) | payload[off + 1]);
        int16_t raw = (int16_t)((payload[off + 2] << 8) | payload[off + 3]);
        int m;

        for (m = 0; m < SMARTSDR_MTR_COUNT; m++)
        {
            if (priv->meter_wire_id[m] == (int16_t)id)
            {
                priv->meter_value[m] = (double)raw / divisor[m];
                priv->meter_stamp_ms[m] = now;
                break;
            }
        }
    }

    smartsdr_state_unlock(priv);
}



/* The radio keeps the input selection and the DAX flag apart, so they can be
 * set to contradict each other -- input MIC with dax=1 modulates from nothing.
 * Writing both from one setting keeps them consistent. */
void smartsdr_apply_tx_audio_source(RIG *rig)
{
    struct smartsdr_priv_data *priv = (struct smartsdr_priv_data *)STATE(rig)->priv;
    const char *src = priv->tx_audio_source;
    char cmd[64];
    int want_dax;

    if (src[0] == '\0')
    {
        return;
    }

    want_dax = (strcmp(src, "DAX") == 0);

    if (!want_dax)
    {
        /* The setting is spelled as the radio names its inputs, so it goes
         * out as it came in. */
        snprintf(cmd, sizeof(cmd), "mic input %s", src);
        smartsdr_command_or_warn(rig, cmd);
    }

    snprintf(cmd, sizeof(cmd), "transmit set dax=%d", want_dax ? 1 : 0);
    smartsdr_command_or_warn(rig, cmd);
    priv->tx_dax_enabled = (uint8_t)want_dax;
}


/* Metering needs the shared socket even when no stream is open, and the radio
 * pushes values continuously once subscribed rather than answering a query.
 * Both are therefore set up on the first meter read and held for the session,
 * so a rig that never reads a meter carries neither. */
int smartsdr_meters_start(RIG *rig)
{
    struct smartsdr_priv_data *priv = (struct smartsdr_priv_data *)STATE(rig)->priv;
    int retval;

    if (priv->meters_subscribed)
    {
        return RIG_OK;
    }

    retval = smartsdr_resolve_radio_udp_addr(rig);

    if (retval != RIG_OK)
    {
        return retval;
    }

    pthread_mutex_lock(&priv->stream_lock);
    retval = smartsdr_udp_open(rig);
    pthread_mutex_unlock(&priv->stream_lock);

    if (retval != RIG_OK)
    {
        return retval;
    }

    smartsdr_register_udp_port(rig, priv, 0);
    smartsdr_command_or_warn(rig, "sub meter all");
    priv->meters_subscribed = 1;

    return RIG_OK;
}


void smartsdr_meters_stop(RIG *rig)
{
    struct smartsdr_priv_data *priv = (struct smartsdr_priv_data *)STATE(rig)->priv;

    if (!priv->meters_subscribed)
    {
        return;
    }

    priv->meters_subscribed = 0;
    smartsdr_stream_release_udp(rig);
}


/* Read the last value the radio reported for a meter. */
int smartsdr_meter_read(RIG *rig, int slot, double *val)
{
    struct smartsdr_priv_data *priv = (struct smartsdr_priv_data *)STATE(rig)->priv;
    int retval = smartsdr_meters_start(rig);
    int waited = 0;
    int budget;
    int fps;
    int reported;

    if (retval != RIG_OK)
    {
        return retval;
    }

    smartsdr_state_lock(priv);
    fps = priv->meter_fps[slot];
    smartsdr_state_unlock(priv);

    /* The first read can still land before the first packet carrying this
     * meter. How long that takes depends on the meter: the radio advertises a
     * frame rate for the ones it sends on a timer, and fps=0 for the slow
     * ones -- PA temperature, supply voltage and current -- which it sends
     * only as they change. Waiting the same short time for both would report
     * a working meter as unavailable. */
    if (fps > 0)
    {
        /* The radio batches meters into packets by subset rather than sending
         * every meter every frame, so a meter can skip several of its own
         * frames; allow for that rather than for one nominal period. */
        budget = 10 * 1000 / fps;

        if (budget < SMARTSDR_METER_WAIT_MS)
        {
            budget = SMARTSDR_METER_WAIT_MS;
        }
    }
    else
    {
        budget = SMARTSDR_METER_SLOW_WAIT_MS;
    }

    for (;;)
    {
        smartsdr_state_lock(priv);
        reported = (priv->meter_stamp_ms[slot] != 0);

        if (reported)
        {
            *val = priv->meter_value[slot];
        }

        smartsdr_state_unlock(priv);

        if (reported)
        {
            return RIG_OK;
        }

        if (waited >= budget)
        {
            return -RIG_ENAVAIL;
        }

        hl_usleep(20 * 1000);
        waited += 20;
    }
}


/* Bind to the panadapter this rig's slice already uses, or make one, and take
 * the shared socket so its FFT arrives. Creating a panadapter is visible on
 * the radio, so this only happens when asked for. */
int smartsdr_spectrum_start(RIG *rig)
{
    struct smartsdr_priv_data *priv = (struct smartsdr_priv_data *)STATE(rig)->priv;
    char cmd[160];
    int retval;

    if (priv->spectrum_pan_id != 0U)
    {
        return RIG_OK;
    }

    retval = smartsdr_resolve_radio_udp_addr(rig);

    if (retval != RIG_OK)
    {
        return retval;
    }

    pthread_mutex_lock(&priv->stream_lock);
    retval = smartsdr_udp_open(rig);
    pthread_mutex_unlock(&priv->stream_lock);

    if (retval != RIG_OK)
    {
        return retval;
    }

    smartsdr_register_udp_port(rig, priv, 0);

    /* Read the panadapter the slice is already using. A panadapter created
     * without a slice behind it is not fed by any receiver: it emits one
     * frame and then a flat line, which looks like a working spectrum of a
     * dead band. */
    if (priv->slice_pan_id == 0U)
    {
        rig_debug(RIG_DEBUG_ERR,
                  "%s: slice %d has no panadapter to read\n", __func__,
                  priv->slicenum);
        smartsdr_stream_release_udp(rig);
        return -RIG_ENAVAIL;
    }

    priv->spectrum_pan_id = priv->slice_pan_id;
    priv->spectrum_pan_created = 0;

    /* Ask for a full-resolution sweep. The radio scales bin values to the
     * panadapter's height, so a display-sized one carries only as many
     * levels as it has pixels. A client that owns the panadapter may refuse
     * this, which costs resolution but nothing else. */
    snprintf(cmd, sizeof(cmd), "display pan set 0x%x xpixels=%d ypixels=%d",
             priv->spectrum_pan_id, SMARTSDR_SPECTRUM_BINS,
             SMARTSDR_SPECTRUM_YPIXELS);
    smartsdr_command_or_warn(rig, cmd);

    /* Until the radio reports this panadapter's own settings. */
    if (priv->spectrum_span_hz <= 0.0)
    {
        smartsdr_state_lock(priv);
        priv->spectrum_center_hz = smartsdr_slice_freq(priv);
        smartsdr_state_unlock(priv);
        priv->spectrum_span_hz = SMARTSDR_SPECTRUM_SPAN_HZ;
        priv->spectrum_min_dbm = -135.0;
        priv->spectrum_max_dbm = -40.0;
    }

    rig_debug(RIG_DEBUG_VERBOSE, "%s: panadapter 0x%x%s\n", __func__,
              priv->spectrum_pan_id,
              priv->spectrum_pan_created ? " (created)" : " (existing)");
    return RIG_OK;
}


void smartsdr_spectrum_stop(RIG *rig)
{
    struct smartsdr_priv_data *priv = (struct smartsdr_priv_data *)STATE(rig)->priv;
    char cmd[64];
    char resp[SMARTSDR_RESP_LEN];

    if (priv->spectrum_pan_id == 0U)
    {
        return;
    }

    /* Only a panadapter this backend made is removed; one the operator was
     * already using stays. */
    if (priv->spectrum_pan_created)
    {
        snprintf(cmd, sizeof(cmd), "display pan remove 0x%x",
                 priv->spectrum_pan_id);
        (void)smartsdr_transaction_resp(rig, cmd, resp, sizeof(resp));
    }

    priv->spectrum_pan_id = 0U;
    priv->spectrum_pan_created = 0;
    smartsdr_stream_release_udp(rig);
}


/* Panadapter FFT. The payload is a 12-byte header followed by num_bins
 * big-endian u16 bins, padded to a 32-bit boundary:
 *
 *   u16 start_bin  u16 num_bins  u16 bin_size  u16 total_bins  u32 frame
 *
 * A frame wider than one datagram arrives as several packets sharing a frame
 * index, each carrying the slice of bins starting at start_bin, so bins are
 * reassembled before the line goes out. Bin values span the panadapter's
 * min_dbm..max_dbm range with larger meaning stronger, which is the same
 * direction Hamlib uses, so they only need narrowing to 8 bits. */
static void smartsdr_absorb_pan_fft(RIG *rig, uint32_t stream_id,
                                    const uint8_t *payload, int payload_bytes)
{
    struct smartsdr_priv_data *priv = (struct smartsdr_priv_data *)STATE(rig)->priv;
    struct rig_spectrum_line line;
    unsigned start, nbins, total;
    uint32_t frame;
    unsigned i;

    if (payload_bytes < 12 || stream_id != priv->spectrum_pan_id)
    {
        return;
    }

    start = (unsigned)((payload[0] << 8) | payload[1]);
    nbins = (unsigned)((payload[2] << 8) | payload[3]);
    total = (unsigned)((payload[6] << 8) | payload[7]);
    frame = ((uint32_t)payload[8] << 24) | ((uint32_t)payload[9] << 16)
            | ((uint32_t)payload[10] << 8) | (uint32_t)payload[11];

    if (nbins == 0 || total == 0 || total > SMARTSDR_MAX_SPECTRUM_BINS
            || start + nbins > total
            || payload_bytes < 12 + (int)nbins * 2)
    {
        return;
    }

    /* A new frame index abandons whatever was partly assembled: the rest of
     * the previous frame is not coming. */
    if (frame != priv->spectrum_frame || total != (unsigned)priv->spectrum_total)
    {
        priv->spectrum_frame = frame;
        priv->spectrum_total = (int)total;
        priv->spectrum_have = 0;
    }

    {
        /* Bin values are heights within the panadapter, so they run 0 at the
         * top -- the strongest signal, at max_dbm -- down to y_pixels at
         * min_dbm. Hamlib reports larger as stronger, so they inverted, and
         * scaled when the panadapter is taller than 8 bits can hold. */
        unsigned height = priv->spectrum_height > 0
                          ? (unsigned)priv->spectrum_height
                          : SMARTSDR_SPECTRUM_YPIXELS;

        for (i = 0; i < nbins; i++)
        {
            unsigned v = (unsigned)((payload[12 + i * 2] << 8)
                                    | payload[13 + i * 2]);

            if (v > height)
            {
                v = height;
            }

            priv->spectrum_bins[start + i] =
                (unsigned char)(((height - v) * 255u) / height);
        }
    }

    priv->spectrum_have += (int)nbins;

    if (priv->spectrum_have < priv->spectrum_total)
    {
        return;
    }

    priv->spectrum_have = 0;

    memset(&line, 0, sizeof(line));
    line.id = 0;
    line.data_level_min = 0;
    line.data_level_max = 255;
    line.signal_strength_min = priv->spectrum_min_dbm;
    line.signal_strength_max = priv->spectrum_max_dbm;
    line.spectrum_mode = RIG_SPECTRUM_MODE_CENTER;
    line.center_freq = (freq_t)priv->spectrum_center_hz;
    line.span_freq = (freq_t)priv->spectrum_span_hz;
    line.low_edge_freq = (freq_t)(priv->spectrum_center_hz
                                  - priv->spectrum_span_hz / 2.0);
    line.high_edge_freq = (freq_t)(priv->spectrum_center_hz
                                   + priv->spectrum_span_hz / 2.0);
    line.spectrum_data_length = (size_t)priv->spectrum_total;
    line.spectrum_data = priv->spectrum_bins;

    rig_fire_spectrum_event(rig, &line);
}


/* ------------------------------------------------------------------ */
/* Dispatcher: one thread reads the rig's VITA socket and hands each    */
/* packet to the stream that owns its stream ID                        */
/* ------------------------------------------------------------------ */

/* Feed one VITA packet to the stream it belongs to. */
/* A sequence gap, told to the consumer in the terms its stream needs: audio is
 * zero-filled so the timeline stays continuous and the hole is heard as brief
 * silence, while an I/Q reader is told outright, because invented samples would
 * corrupt the phase a demodulator tracks. */
static void smartsdr_stream_fill_gap(struct rig_stream *stream,
                                     const struct vita49_header *hdr,
                                     unsigned missing_packets,
                                     int payload_bytes,
                                     float *sample_buf,
                                     size_t sample_buf_bytes)
{
    /* Packets are a fixed size within a stream, so the packet in hand sizes
     * the hole the missing ones left -- measured in ring-buffer bytes, which
     * is four times the wire size for the mono-s16 path that widens to stereo
     * float32. */
    size_t out_per_packet = (hdr->pcc == VITA49_PCC_AUDIO_S16)
                            ? (size_t)payload_bytes * 4
                            : (size_t)payload_bytes;
    size_t gap_bytes = (size_t)missing_packets * out_per_packet;
    size_t remaining;

    if (stream_type_is_iq(stream->type))
    {
        uint64_t lost = gap_bytes / (2 * sizeof(float));

        rig_stream_mark_gap(stream, lost);
        rig_debug(RIG_DEBUG_VERBOSE, "%s: IQ gap of %u packets (%llu samples)\n",
                  __func__, missing_packets, (unsigned long long)lost);
        return;
    }

    remaining = gap_bytes;
    memset(sample_buf, 0, sample_buf_bytes);

    while (remaining > 0)
    {
        size_t chunk = remaining > sample_buf_bytes ? sample_buf_bytes : remaining;
        /* A short write means the ring is full, which the frontend already
         * counts as an overrun; the producer has nowhere to put the remainder
         * either way. The same applies to the payload writes. */
        (void)stream_backend_write(stream, (const void *)sample_buf, chunk);
        remaining -= chunk;
    }

    stream->gap_count++;
    rig_debug(RIG_DEBUG_VERBOSE, "%s: gap of %u packets, zero-filled %lu bytes\n",
              __func__, missing_packets, (unsigned long)gap_bytes);
}

/* One payload, converted to what the ring carries and handed over. The radio
 * announces the layout in the packet class code. */
static void smartsdr_stream_write_payload(struct rig_stream *stream,
        const struct vita49_header *hdr,
        const uint8_t *payload, int payload_bytes,
        float *sample_buf, size_t sample_buf_floats)
{
    int num_samples;

    switch (hdr->pcc)
    {
    case VITA49_PCC_AUDIO_S16:
    {
        /* Int16 mono, widened to float32 stereo. */
        int num_mono = payload_bytes / 2;

        if (num_mono > (int)(sample_buf_floats / 2))
        {
            num_mono = (int)(sample_buf_floats / 2);
        }

        vita49_convert_s16_to_f32_stereo(payload, sample_buf, num_mono);
        (void)stream_backend_write(stream, (const void *)sample_buf,
                                   num_mono * 2 * sizeof(float));
        return;
    }

    /* DAX I/Q. Measured on a FLEX-8400M: these carry float32 in *little*
     * endian, already host order here, unlike every other payload the radio
     * sends. Byte-swapping them turns valid samples into denormals. */
    case VITA49_PCC_IQ_24K:
    case VITA49_PCC_IQ_48K:
    case VITA49_PCC_IQ_96K:
    case VITA49_PCC_IQ_192K:
        num_samples = payload_bytes / (int)sizeof(float);

        if (num_samples > (int)sample_buf_floats)
        {
            num_samples = (int)sample_buf_floats;
        }

        vita49_convert_daxiq_float32(payload, sample_buf, num_samples);
        (void)stream_backend_write(stream, (const void *)sample_buf,
                                   num_samples * sizeof(float));
        return;

    /* Float32 interleaved, big-endian: stereo audio, and simflex's I/Q, which
     * it sends under the audio PCC. Anything unrecognised is treated the same
     * way, which is what the wire has always carried in practice. */
    case VITA49_PCC_AUDIO_F32:
    default:
        num_samples = payload_bytes / (int)sizeof(float);

        if (num_samples > (int)sample_buf_floats)
        {
            num_samples = (int)sample_buf_floats;
        }

        vita49_swap_float32(payload, sample_buf, num_samples);
        (void)stream_backend_write(stream, (const void *)sample_buf,
                                   num_samples * sizeof(float));
        return;
    }
}


/* Capture-time anchor, chosen from what the radio says it is worth rather than
 * assumed. A GPSDO makes the stamps traceable to UTC; a TCXO leaves the epoch
 * arbitrary but the timeline stable, which is what RIG_STREAM_TIME_SRC_RADIO
 * describes; anything else falls back to the host clock. A detected gap
 * re-anchors immediately with DISCONTINUITY. */
static void smartsdr_stream_push_anchor(struct smartsdr_stream_state *ss,
                                        struct rig_stream *stream,
                                        struct smartsdr_priv_data *priv,
                                        const struct vita49_header *phdr,
                                        int gap_hit)
{
    struct rig_stream_time_anchor anchor;
    int push = 0;

    memset(&anchor, 0, sizeof(anchor));
    anchor.sample_index = rig_stream_get_samples_written(stream);

    if (priv->gpsdo_present
            && (phdr->tsi == VITA49_TSI_UTC || phdr->tsi == VITA49_TSI_GPS)
            && phdr->tsf == VITA49_TSF_REAL_TIME_PS)
    {
        anchor.seconds = (int64_t)phdr->timestamp_int;
        anchor.picoseconds = phdr->timestamp_frac;
        stream_time_normalize(&anchor.seconds, &anchor.picoseconds);
        anchor.source = RIG_STREAM_TIME_SRC_GPS;
        anchor.flags = RIG_STREAM_TIME_FLAG_LOCKED
                       | RIG_STREAM_TIME_FLAG_SAMPLE_REFERENCED;
        anchor.accuracy = RIG_STREAM_TIME_ACC_100NS;
        push = 1;   /* per-packet hardware time: stamp every packet */

        /* Keep it for a later stream_hardware_time, which has no
         * packet of its own to read. */
        smartsdr_state_lock(priv);
        ss->hw_seconds = anchor.seconds;
        ss->hw_picoseconds = anchor.picoseconds;
        ss->hw_time_valid = 1;
        smartsdr_state_unlock(priv);
    }
    else if (priv->tcxo_present && ss->timeline_seen
             && (gap_hit
                 || ss->anchor_pkt_counter
                 % SMARTSDR_HOST_ANCHOR_EVERY_PKTS == 0))
    {
        /* The epoch is not traceable, so the host clock still sets
         * where the timeline sits in absolute terms; what the TCXO
         * buys is that the rate between anchors is disciplined
         * rather than drifting with the host. */
        stream_time_now(&anchor.seconds, &anchor.picoseconds);
        anchor.source = RIG_STREAM_TIME_SRC_RADIO;
        anchor.flags = RIG_STREAM_TIME_FLAG_SAMPLE_REFERENCED;
        anchor.accuracy = RIG_STREAM_TIME_ACC_US;
        push = 1;
    }
    else if (gap_hit
             || ss->anchor_pkt_counter % SMARTSDR_HOST_ANCHOR_EVERY_PKTS
             == 0)
    {
        stream_time_now(&anchor.seconds, &anchor.picoseconds);
        anchor.source = RIG_STREAM_TIME_SRC_HOST;
        anchor.accuracy = RIG_STREAM_TIME_ACC_MS;
        push = 1;
    }

    if (push)
    {
        if (gap_hit)
        {
            anchor.flags |= RIG_STREAM_TIME_FLAG_DISCONTINUITY;
        }

        rig_stream_push_time_anchor(stream, &anchor);
    }

    ss->anchor_pkt_counter++;
}


static void smartsdr_dispatch_to_stream(struct smartsdr_stream_state *ss,
                                        const struct vita49_header *phdr,
                                        const uint8_t *pkt_buf, int nbytes,
                                        float *sample_buf,
                                        size_t sample_buf_floats)
{
    struct rig_stream *stream = ss->stream;
    struct smartsdr_priv_data *priv =
        (struct smartsdr_priv_data *)STATE(ss->rig)->priv;
    struct vita49_header hdr = *phdr;
    size_t sample_buf_bytes = sample_buf_floats * sizeof(float);

    /* DAX-IQ samples are referenced to the panadapter centre, which the
     * operator can move while streaming, so republish it as it changes. */
    if (stream_type_is_iq(stream->type) && priv->pan_center_hz > 0.0
            && stream->center_freq != (freq_t)priv->pan_center_hz)
    {
        stream->center_freq = (freq_t)priv->pan_center_hz;
    }

    if (stream->paused)
    {
        return;
    }

    {
        /* Flex sends most client-bound VITA as EXT_DATA (0x3); DAX IQ may use IF_DATA
         * (0x1) on some paths — accept both packet types. */
        if (hdr.packet_type != VITA49_PKT_TYPE_EXT_DATA
                && hdr.packet_type != VITA49_PKT_TYPE_IF_DATA)
        {
            return;
        }

        /* Gap detection rides on the VITA packet counter, which the radio
         * increments by one per packet on both audio and I/Q — it is the
         * transport's own sequence number. The fractional timestamp cannot
         * serve here: on I/Q it is real time in picoseconds, and on audio it
         * is a constant, so neither counts samples.
         *
         * The counter is 4 bits, so a loss of an exact multiple of 16 packets
         * reads as no loss at all. That is a 59 ms burst at 48 kHz I/Q; a
         * dropout that large shows up in the byte rate regardless. */
        int trail_bytes = hdr.trailer_flag ? 4 : 0;
        int payload_bytes = nbytes - (int)VITA49_HEADER_BYTES - trail_bytes;
        int gap_hit = 0;
        unsigned missing_packets = 0;

        if (payload_bytes <= 0)
        {
            return;
        }

        if (ss->seq_initialized
                && hdr.packet_count != ((ss->last_packet_count + 1) & 0x0F))
        {
            gap_hit = 1;
            missing_packets = smartsdr_gap_from_counter(ss->last_packet_count,
                              hdr.packet_count);
        }

        ss->last_packet_count = hdr.packet_count;
        ss->seq_initialized = 1;

        if (gap_hit)
        {
            smartsdr_stream_fill_gap(stream, &hdr, missing_packets,
                                     payload_bytes, sample_buf,
                                     sample_buf_bytes);
        }

        /* Has the radio's fractional stamp actually advanced? That is what
         * separates a running clock from a field the radio leaves alone --
         * DAX audio reports the same value in every packet, while DAX-IQ
         * carries a timeline that moves. */
        if (ss->timeline_have_prev
                && hdr.timestamp_frac != ss->last_timestamp_frac)
        {
            ss->timeline_seen = 1;
        }

        ss->last_timestamp_frac = hdr.timestamp_frac;
        ss->timeline_have_prev = 1;

        smartsdr_stream_push_anchor(ss, stream, priv, &hdr, gap_hit);

        smartsdr_stream_write_payload(stream, &hdr,
                                      pkt_buf + VITA49_HEADER_BYTES,
                                      payload_bytes, sample_buf,
                                      sample_buf_floats);
    }

}


/* Nothing more will arrive once the radio is gone, so fail every open buffer
 * and let readers see -RIG_EIO rather than wait out a timeout that will never
 * be satisfied. */
static void smartsdr_fail_open_streams(struct smartsdr_priv_data *priv)
{
    int i;

    rig_debug(RIG_DEBUG_ERR, "%s: session lost, failing open streams\n",
              __func__);
    pthread_mutex_lock(&priv->stream_lock);

    for (i = 0; i < SMARTSDR_MAX_RX_STREAMS; i++)
    {
        if (priv->rx_streams[i])
        {
            priv->rx_streams[i]->stream->ringbuf.failed = 1;
        }
    }

    pthread_mutex_unlock(&priv->stream_lock);
}


/* Only a datagram leaving this socket keeps alive the NAT mapping the radio
 * sends to, so the nudge is repeated from the receive loop rather than from
 * the TCP thread, which uses a different one. */
static void smartsdr_nat_nudge_if_due(RIG *rig, int64_t *last_nudge_s)
{
    int64_t now_s;
    uint64_t now_ps;

    stream_time_now(&now_s, &now_ps);

    if (*last_nudge_s == 0)
    {
        *last_nudge_s = now_s;
        return;
    }

    if (now_s - *last_nudge_s >= SMARTSDR_NAT_NUDGE_INTERVAL_MS / 1000)
    {
        *last_nudge_s = now_s;
        smartsdr_send_udp_nudge(rig);
    }
}


/* One thread per rig owns the VITA socket. Streams are looked up by ID under
 * stream_lock so a close cannot free one mid-packet. */
static void *smartsdr_dispatch_thread(void *arg)
{
    RIG *rig = (RIG *)arg;
    struct smartsdr_priv_data *priv = (struct smartsdr_priv_data *)STATE(rig)->priv;
    uint8_t pkt_buf[8192];
    float sample_buf[4096];
    int64_t last_nudge_s = 0;
    int failed_all = 0;

    rig_debug(RIG_DEBUG_VERBOSE, "%s: dispatch thread started\n", __func__);

    while (priv->dispatch_running)
    {
        struct sockaddr_in from;
        socklen_t fromlen = sizeof(from);
        struct vita49_header hdr;
        struct smartsdr_stream_state *ss = NULL;
        int nbytes;
        int i;

        if (priv->session_lost && !failed_all)
        {
            smartsdr_fail_open_streams(priv);
            failed_all = 1;
        }

        if (priv->nat_traversal)
        {
            smartsdr_nat_nudge_if_due(rig, &last_nudge_s);
        }

        nbytes = recvfrom(priv->udp_sock, (char *)pkt_buf, sizeof(pkt_buf),
                          0, (struct sockaddr *)&from, &fromlen);

        if (nbytes < VITA49_HEADER_BYTES)
        {
            continue;  /* Timeout or runt packet */
        }

        /* Accept VITA packets only from the radio's address, so another host
         * on the LAN cannot inject audio/IQ into a ring buffer. */
        if (from.sin_addr.s_addr != priv->radio_udp_addr.sin_addr.s_addr)
        {
            continue;
        }

        if (vita49_parse_header(pkt_buf, nbytes, &hdr) < 0)
        {
            continue;
        }

        /* Meters and the panadapter belong to the radio rather than to any
         * stream, so they are handled before the stream lookup. */
        if (hdr.pcc == VITA49_PCC_METER)
        {
            smartsdr_absorb_meter_packet(priv, pkt_buf + VITA49_HEADER_BYTES,
                                         nbytes - (int)VITA49_HEADER_BYTES);
            continue;
        }

        if (hdr.pcc == VITA49_PCC_PAN_FFT)
        {
            smartsdr_absorb_pan_fft(rig, hdr.stream_id,
                                    pkt_buf + VITA49_HEADER_BYTES,
                                    nbytes - (int)VITA49_HEADER_BYTES);
            continue;
        }

        pthread_mutex_lock(&priv->stream_lock);

        for (i = 0; i < SMARTSDR_MAX_RX_STREAMS; i++)
        {
            if (priv->rx_streams[i]
                    && priv->rx_streams[i]->vita_stream_id == hdr.stream_id)
            {
                ss = priv->rx_streams[i];
                break;
            }
        }

        if (ss)
        {
            smartsdr_dispatch_to_stream(ss, &hdr, pkt_buf, nbytes, sample_buf,
                                        sizeof(sample_buf) / sizeof(float));
        }

        pthread_mutex_unlock(&priv->stream_lock);
    }

    rig_debug(RIG_DEBUG_VERBOSE, "%s: dispatch thread exiting\n", __func__);
    return NULL;
    return NULL;
}


/* ------------------------------------------------------------------ */
/* TX thread: reads ring buffer, builds VITA-49, sends to radio        */
/* ------------------------------------------------------------------ */

static void *smartsdr_tx_thread(void *arg)
{
    struct smartsdr_stream_state *ss = (struct smartsdr_stream_state *)arg;
    struct rig_stream *stream = ss->stream;
    struct smartsdr_priv_data *priv =
        (struct smartsdr_priv_data *)STATE(ss->rig)->priv;
    float sample_buf[SMARTSDR_TX_SAMPLES];
    uint8_t pkt_buf[SMARTSDR_TX_PACKET_BYTES + 64];
    int is_iq = stream_type_is_iq(stream->type);

    /* Class ID for TX packets */
    uint64_t class_id;

    if (is_iq)
    {
        /* DAXIQ: use same class ID pattern */
        class_id = ((uint64_t)SMARTSDR_FLEX_OUI << 16) | VITA49_PCC_AUDIO_F32;
    }
    else
    {
        /* DAX audio: PCC 0x03E3 (float32 stereo) */
        class_id = ((uint64_t)SMARTSDR_FLEX_OUI << 16) | VITA49_PCC_AUDIO_F32;
    }

    if (stream->backend_config.sample_rate <= 0)
    {
        rig_debug(RIG_DEBUG_ERR,
                  "%s: invalid sample_rate %d; TX thread not starting\n",
                  __func__, stream->backend_config.sample_rate);
        ss->running = 0;
        return NULL;
    }

    /* TX frame timing: 128 stereo pairs at 24 kHz = ~5.33ms */
    int frame_bytes = SMARTSDR_TX_SAMPLES * sizeof(float);
    int frame_us = (SMARTSDR_TX_STEREO_PAIRS * 1000000) /
                   stream->backend_config.sample_rate;

    rig_debug(RIG_DEBUG_VERBOSE,
              "%s: TX thread started, stream_id=0x%x frame_us=%d\n",
              __func__, ss->vita_stream_id, frame_us);

    int pair_bytes = stream->frame_bytes > 0
                     ? stream->frame_bytes : (int)(2 * sizeof(float));

    /* Bytes read but not yet forming a whole stereo pair, carried across
     * frames so the consumed-pair count stays exact (no truncation drift). */
    size_t tx_partial_bytes = 0;

    while (ss->running)
    {
        /* A dead radio cannot accept samples either; fail the buffer so the
         * writer is told instead of filling it forever. */
        if (priv->session_lost)
        {
            rig_debug(RIG_DEBUG_ERR, "%s: session lost, failing the stream\n",
                      __func__);
            stream->ringbuf.failed = 1;
            break;
        }

        if (stream->paused)
        {
            hl_usleep(10 * 1000);
            continue;
        }

        smartsdr_tx_service_targets(ss, stream);

        /* Wait for a whole frame rather than running a clock of our own.
         * The application writes at real time, so consuming one frame at a
         * time makes its clock the transmit clock and no drift is possible.
         * Pacing with a fixed sleep instead cannot work: the sleep can only
         * overshoot, and at 24 kHz a frame is 5333 us against a nominal
         * 5333 us of audio, so any overshoot at all starves the radio and
         * overruns the ring.
         *
         * The bounded wait still emits silence when nothing arrives, which
         * keeps the radio's DAX play-out alive while PTT is held. */
        size_t got = 0;
        /* Waiting one frame period is too tight -- the application produces a
         * frame every frame period, so an equal wait races it and pads with
         * silence that was merely late. This is generous enough that only a
         * real stall reaches the silence path, which is what distinguishes
         * "not yet" from "not coming". */
        const int wait_ms = SMARTSDR_TX_FRAME_WAIT_MS;

        while (got < (size_t)frame_bytes && ss->running)
        {
            size_t n = stream_ringbuf_read(&stream->ringbuf,
                                           (uint8_t *)sample_buf + got,
                                           frame_bytes - got, wait_ms);

            if (n == 0)
            {
                break;      /* nothing came: send what there is, plus silence */
            }

            got += n;
        }

        /* Count whole stereo pairs exactly, carrying any leftover bytes
         * into the next frame so the pair counter never drifts. */
        tx_partial_bytes += got;
        ss->tx_consumed += tx_partial_bytes / (size_t)pair_bytes;
        tx_partial_bytes %= (size_t)pair_bytes;

        if (got < (size_t)frame_bytes)
        {
            /* Fill remainder with silence */
            if (got > 0)
            {
                memset((uint8_t *)sample_buf + got, 0, frame_bytes - got);
            }
            else
            {
                memset(sample_buf, 0, frame_bytes);
            }
        }

        /* Build VITA-49 TX packet */
        int pkt_len = vita49_build_tx_packet(pkt_buf, sizeof(pkt_buf),
                                             ss->vita_stream_id, class_id,
                                             ss->vita_packet_count++,
                                             sample_buf,
                                             SMARTSDR_TX_SAMPLES);

        if (pkt_len <= 0)
        {
            continue;
        }

        if (sendto(priv->udp_sock, (char *)pkt_buf, pkt_len, 0,
                   (struct sockaddr *)&priv->radio_udp_addr,
                   sizeof(priv->radio_udp_addr)) < 0)
        {
            rig_debug(RIG_DEBUG_WARN, "%s: TX sendto failed: %s\n",
                      __func__, strerror(errno));
        }

    }

    rig_debug(RIG_DEBUG_VERBOSE, "%s: TX thread exiting\n", __func__);
    return NULL;
}
