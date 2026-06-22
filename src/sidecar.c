/*
 *  Hamlib Sidecar API - Implementation
 *  Copyright (c) 2026 by Jeff Francis N0GQ
 *
 *  This library is free software; you can redistribute it and/or
 *  modify it under the terms of the GNU Lesser General Public
 *  License as published by the Free Software Foundation; either
 *  version 2.1 of the License, or (at your option) any later version.
 */

/**
 * \file sidecar.c
 * \brief Hamlib sidecar API implementation
 */

#include "hamlib/config.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/socket.h>
#include <time.h>
#include <netinet/in.h>
#include <arpa/inet.h>

#include "hamlib/rig.h"
#include "hamlib/sidecar.h"
#include "misc.h"


/* -------------------------------------------------------------------------
 * Socket Management
 * ----------------------------------------------------------------------- */

/**
 * \brief Initialize a sidecar listener port
 */
int sidecar_init_port(int port)
{
    int fd, optval = 1;
    struct sockaddr_in addr;

    /* Create socket */
    fd = socket(AF_INET, SOCK_STREAM, 0);
    if (fd < 0)
    {
        rig_debug(RIG_DEBUG_ERR, "%s: socket() failed: %s\n",
                  __func__, strerror(errno));
        return -RIG_EIO;
    }

    /* Set SO_REUSEADDR to allow quick restart */
    if (setsockopt(fd, SOL_SOCKET, SO_REUSEADDR, &optval, sizeof(optval)) < 0)
    {
        rig_debug(RIG_DEBUG_WARN, "%s: setsockopt(SO_REUSEADDR) failed: %s\n",
                  __func__, strerror(errno));
    }

    /* Bind to localhost:port */
    memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
    addr.sin_port = htons(port);

    if (bind(fd, (struct sockaddr *)&addr, sizeof(addr)) < 0)
    {
        rig_debug(RIG_DEBUG_ERR, "%s: bind(localhost:%d) failed: %s\n",
                  __func__, port, strerror(errno));
        close(fd);
        return -RIG_EIO;
    }

    /* Listen (backlog=1, only one sidecar at a time) */
    if (listen(fd, 1) < 0)
    {
        rig_debug(RIG_DEBUG_ERR, "%s: listen() failed: %s\n",
                  __func__, strerror(errno));
        close(fd);
        return -RIG_EIO;
    }

    /* Set non-blocking */
    int flags = fcntl(fd, F_GETFL, 0);
    if (flags < 0 || fcntl(fd, F_SETFL, flags | O_NONBLOCK) < 0)
    {
        rig_debug(RIG_DEBUG_WARN, "%s: fcntl(O_NONBLOCK) failed: %s\n",
                  __func__, strerror(errno));
    }

    rig_debug(RIG_DEBUG_VERBOSE, "%s: sidecar listener on localhost:%d (fd %d)\n",
              __func__, port, fd);

    return fd;
}

/**
 * \brief Accept a sidecar client connection
 */
int sidecar_accept_client(int server_fd)
{
    struct sockaddr_in client_addr;
    socklen_t addr_len = sizeof(client_addr);
    int client_fd;

    if (server_fd < 0)
    {
        return -RIG_EINVAL;
    }

    client_fd = accept(server_fd, (struct sockaddr *)&client_addr, &addr_len);

    if (client_fd < 0)
    {
        if (errno == EWOULDBLOCK || errno == EAGAIN)
        {
            /* No client waiting, not an error */
            return -RIG_ENAVAIL;
        }
        else
        {
            rig_debug(RIG_DEBUG_ERR, "%s: accept() failed: %s\n",
                      __func__, strerror(errno));
            return -RIG_EIO;
        }
    }

    /* Set non-blocking on client socket */
    int flags = fcntl(client_fd, F_GETFL, 0);
    if (flags < 0 || fcntl(client_fd, F_SETFL, flags | O_NONBLOCK) < 0)
    {
        rig_debug(RIG_DEBUG_WARN, "%s: fcntl(O_NONBLOCK) on client failed: %s\n",
                  __func__, strerror(errno));
    }

    rig_debug(RIG_DEBUG_VERBOSE, "%s: accepted sidecar client (fd %d)\n",
              __func__, client_fd);

    return client_fd;
}

/**
 * \brief Close a sidecar port
 */
void sidecar_close_port(int fd)
{
    if (fd >= 0)
    {
        close(fd);
        rig_debug(RIG_DEBUG_VERBOSE, "%s: closed sidecar fd %d\n",
                  __func__, fd);
    }
}


/* -------------------------------------------------------------------------
 * Frame Building
 * ----------------------------------------------------------------------- */

/**
 * \brief Build a sidecar frame
 */
int sidecar_build_frame(
    uint8_t *buf, size_t buflen,
    uint32_t receiver, uint32_t sample_rate,
    uint32_t format, uint32_t length,
    uint32_t stream_type, uint32_t channels,
    const void *payload, size_t payload_len)
{
    uint32_t *hdr = (uint32_t *)buf;
    size_t total_len = SIDECAR_HEADER_LEN + payload_len;

    if (buf == NULL || buflen < total_len)
    {
        rig_debug(RIG_DEBUG_ERR, "%s: buffer too small (%zu < %zu)\n",
                  __func__, buflen, total_len);
        return -RIG_EINVAL;
    }

    /* Zero header */
    memset(hdr, 0, SIDECAR_HEADER_LEN);

    /* Fill header (little-endian) */
    hdr[0] = receiver;
    hdr[1] = sample_rate;
    hdr[2] = format;
    hdr[3] = 0;  /* codec (reserved) */
    hdr[4] = 0;  /* crc (reserved) */
    hdr[5] = length;
    hdr[6] = stream_type;
    hdr[7] = channels;
    /* hdr[8-15] already zeroed */

    /* Append payload if present */
    if (payload != NULL && payload_len > 0)
    {
        memcpy(buf + SIDECAR_HEADER_LEN, payload, payload_len);
    }

    return (int)total_len;
}


/* -------------------------------------------------------------------------
 * Helper: Send Frame
 * ----------------------------------------------------------------------- */

/**
 * \brief Send a pre-built frame to sidecar
 *
 * Internal helper. Handles EAGAIN/EWOULDBLOCK gracefully.
 */
static int sidecar_send_frame(int fd, const void *frame, size_t frame_len)
{
    /* Loop sending until the entire frame is on the wire or we hit a
     * fatal socket error. Partial sends are common with large IQ frames
     * (e.g. 128 KiB at 2.4 MS/s); previously the code dropped the
     * remainder, which meant the receiver saw the start of frame N
     * concatenated with frame N+1's header at the wrong offset and
     * triggered an endless resync loop. */
    if (fd < 0)
    {
        return RIG_OK;  /* No sidecar connected, silently succeed */
    }

    const uint8_t *p = (const uint8_t *)frame;
    size_t remaining = frame_len;
    while (remaining > 0)
    {
        ssize_t sent = send(fd, p, remaining, MSG_NOSIGNAL);
        if (sent > 0)
        {
            p += (size_t)sent;
            remaining -= (size_t)sent;
            continue;
        }

        if (sent < 0)
        {
            if (errno == EINTR)
            {
                continue;
            }
            if (errno == EAGAIN || errno == EWOULDBLOCK)
            {
                /* Socket buffer full. Block briefly and retry rather than
                 * dropping bytes mid-frame (which would desync the receiver
                 * permanently).  Localhost TCP, so a small sleep self-paces
                 * us against the sidecar's drain rate. */
                struct timespec ts = { .tv_sec = 0, .tv_nsec = 1000000 }; /* 1 ms */
                nanosleep(&ts, NULL);
                continue;
            }
            if (errno == EPIPE || errno == ECONNRESET)
            {
                rig_debug(RIG_DEBUG_WARN, "%s: sidecar disconnected\n", __func__);
                return -RIG_EIO;
            }
            rig_debug(RIG_DEBUG_ERR, "%s: send() failed: %s\n",
                      __func__, strerror(errno));
            return -RIG_EIO;
        }
    }

    return RIG_OK;
}


/* -------------------------------------------------------------------------
 * Audio/IQ Streaming
 * ----------------------------------------------------------------------- */

/**
 * \brief Send RX audio samples to sidecar
 */
int sidecar_send_rx_audio(
    int fd, uint32_t receiver, uint32_t sample_rate,
    uint32_t format, uint32_t channels,
    const void *samples, size_t sample_count)
{
    /* Stack buffer big enough for the common case (TCI 384 kHz IQ frames are
     * ~32 KiB, KiwiSDR audio frames are 1 KiB, smart-sidecar audio chunks
     * are sub-KiB).  Larger payloads (e.g. RTL-SDR 1 MiB IQ buffers from
     * librtlsdr's async loop) fall through to a heap allocation. */
    uint8_t  stack_frame[SIDECAR_HEADER_LEN + 65536];
    uint8_t *frame = stack_frame;
    uint8_t *heap_frame = NULL;
    size_t   sample_bytes;
    int      frame_len;
    int      ret;

    if (samples == NULL || sample_count == 0)
    {
        return -RIG_EINVAL;
    }

    /* Calculate payload size */
    switch (format)
    {
        case SIDECAR_FMT_INT16:   sample_bytes = 2; break;
        case SIDECAR_FMT_INT24:   sample_bytes = 3; break;
        case SIDECAR_FMT_INT32:   sample_bytes = 4; break;
        case SIDECAR_FMT_FLOAT32: sample_bytes = 4; break;
        default:
            rig_debug(RIG_DEBUG_ERR, "%s: invalid format %u\n", __func__, format);
            return -RIG_EINVAL;
    }

    size_t payload_len = sample_count * channels * sample_bytes;
    size_t total_len = SIDECAR_HEADER_LEN + payload_len;

    /* Sanity ceiling: refuse > 16 MiB single frame (way bigger than anything
     * an SDR backend should hand us in a single call).  Backends streaming
     * very large buffers should chunk on their side regardless, since the
     * sidecar protocol is built around continuous low-latency frames. */
    if (payload_len > (16u * 1024u * 1024u))
    {
        rig_debug(RIG_DEBUG_ERR, "%s: payload too large (%zu bytes)\n",
                  __func__, payload_len);
        return -RIG_EINVAL;
    }

    if (total_len > sizeof(stack_frame))
    {
        heap_frame = (uint8_t *)malloc(total_len);
        if (heap_frame == NULL)
        {
            rig_debug(RIG_DEBUG_ERR, "%s: malloc(%zu) failed\n",
                      __func__, total_len);
            return -RIG_ENOMEM;
        }
        frame = heap_frame;
    }

    frame_len = sidecar_build_frame(frame, total_len,
                                    receiver, sample_rate, format, sample_count,
                                    SIDECAR_STREAM_RX_AUDIO, channels,
                                    samples, payload_len);
    if (frame_len < 0)
    {
        free(heap_frame);
        return frame_len;
    }

    ret = sidecar_send_frame(fd, frame, frame_len);
    free(heap_frame);
    return ret;
}

/**
 * \brief Send RX IQ samples to sidecar
 */
int sidecar_send_rx_iq(
    int fd, uint32_t receiver, uint32_t sample_rate,
    uint32_t format,
    const void *samples, size_t sample_count)
{
    /* IQ uses stream_type SIDECAR_STREAM_IQ (0). Channels=2 (I, Q).
     *
     * Earlier revisions of this function reused sidecar_send_rx_audio()
     * which stamped stream_type=RX_AUDIO; smart sidecars saw IQ frames
     * arrive as RX_AUDIO and tried to play them as 2-channel audio,
     * producing noise. The TCI backend forwards its own frames raw with
     * the IQ stream_type already set, so this only ever bit IQ-emitting
     * backends that use this convenience wrapper (RTL-SDR, KiwiSDR /IQ).
     *
     * Build the frame inline so we can stamp the correct stream_type. */
    uint8_t  stack_frame[SIDECAR_HEADER_LEN + 65536];
    uint8_t *frame = stack_frame;
    uint8_t *heap_frame = NULL;
    size_t   sample_bytes;
    int      frame_len;
    int      ret;

    if (samples == NULL || sample_count == 0)
    {
        return -RIG_EINVAL;
    }

    switch (format)
    {
        case SIDECAR_FMT_INT16:   sample_bytes = 2; break;
        case SIDECAR_FMT_INT24:   sample_bytes = 3; break;
        case SIDECAR_FMT_INT32:   sample_bytes = 4; break;
        case SIDECAR_FMT_FLOAT32: sample_bytes = 4; break;
        default:
            rig_debug(RIG_DEBUG_ERR, "%s: invalid format %u\n", __func__, format);
            return -RIG_EINVAL;
    }

    /* IQ is 2 channels (I, Q) */
    size_t payload_len = sample_count * 2 * sample_bytes;
    size_t total_len = SIDECAR_HEADER_LEN + payload_len;

    if (payload_len > (16u * 1024u * 1024u))
    {
        rig_debug(RIG_DEBUG_ERR, "%s: payload too large (%zu bytes)\n",
                  __func__, payload_len);
        return -RIG_EINVAL;
    }

    if (total_len > sizeof(stack_frame))
    {
        heap_frame = (uint8_t *)malloc(total_len);
        if (heap_frame == NULL)
        {
            rig_debug(RIG_DEBUG_ERR, "%s: malloc(%zu) failed\n",
                      __func__, total_len);
            return -RIG_ENOMEM;
        }
        frame = heap_frame;
    }

    frame_len = sidecar_build_frame(frame, total_len,
                                    receiver, sample_rate, format, sample_count,
                                    SIDECAR_STREAM_IQ, 2,
                                    samples, payload_len);
    if (frame_len < 0)
    {
        free(heap_frame);
        return frame_len;
    }

    ret = sidecar_send_frame(fd, frame, frame_len);
    free(heap_frame);
    return ret;
}


/* -------------------------------------------------------------------------
 * Control Frame Emission
 * ----------------------------------------------------------------------- */

/**
 * \brief Emit mode change control frame
 */
int sidecar_emit_mode(
    int fd, uint32_t receiver, rmode_t mode, pbwidth_t width)
{
    uint8_t frame[SIDECAR_HEADER_LEN];
    int frame_len;

    frame_len = sidecar_build_frame(frame, sizeof(frame),
                                    receiver, 0, 0, (uint32_t)mode,
                                    SIDECAR_STREAM_MODE, (uint32_t)width,
                                    NULL, 0);
    if (frame_len < 0)
    {
        return frame_len;
    }

    return sidecar_send_frame(fd, frame, frame_len);
}

/**
 * \brief Emit frequency change control frame
 */
int sidecar_emit_freq(
    int fd, uint32_t receiver, freq_t freq)
{
    uint8_t frame[SIDECAR_HEADER_LEN];
    int frame_len;
    uint64_t freq64 = (uint64_t)freq;
    uint32_t freq_lo = (uint32_t)(freq64 & 0xFFFFFFFF);
    uint32_t freq_hi = (uint32_t)(freq64 >> 32);

    frame_len = sidecar_build_frame(frame, sizeof(frame),
                                    receiver, 0, 0, freq_lo,
                                    SIDECAR_STREAM_FREQ, freq_hi,
                                    NULL, 0);
    if (frame_len < 0)
    {
        return frame_len;
    }

    return sidecar_send_frame(fd, frame, frame_len);
}

/**
 * \brief Emit split enable/disable control frame
 */
int sidecar_emit_split(
    int fd, uint32_t receiver, split_t split, vfo_t tx_vfo)
{
    uint8_t frame[SIDECAR_HEADER_LEN];
    int frame_len;
    uint32_t enabled = (split == RIG_SPLIT_ON) ? 1 : 0;
    uint32_t tx_ch = (tx_vfo == RIG_VFO_B) ? 1 : 0;

    frame_len = sidecar_build_frame(frame, sizeof(frame),
                                    receiver, 0, 0, enabled,
                                    SIDECAR_STREAM_SPLIT, tx_ch,
                                    NULL, 0);
    if (frame_len < 0)
    {
        return frame_len;
    }

    return sidecar_send_frame(fd, frame, frame_len);
}

/**
 * \brief Emit filter edges control frame
 */
int sidecar_emit_filter(
    int fd, uint32_t receiver, int low_hz, int high_hz)
{
    uint8_t frame[SIDECAR_HEADER_LEN];
    int frame_len;

    frame_len = sidecar_build_frame(frame, sizeof(frame),
                                    receiver, 0, 0, (uint32_t)low_hz,
                                    SIDECAR_STREAM_FILTER, (uint32_t)high_hz,
                                    NULL, 0);
    if (frame_len < 0)
    {
        return frame_len;
    }

    return sidecar_send_frame(fd, frame, frame_len);
}

/**
 * \brief Emit AGC level control frame
 */
int sidecar_emit_agc_level(
    int fd, uint32_t receiver, int agc_level)
{
    uint8_t frame[SIDECAR_HEADER_LEN];
    int frame_len;

    frame_len = sidecar_build_frame(frame, sizeof(frame),
                                    receiver, 0, 0, (uint32_t)agc_level,
                                    SIDECAR_STREAM_AGC_LEVEL, 0,
                                    NULL, 0);
    if (frame_len < 0)
    {
        return frame_len;
    }

    return sidecar_send_frame(fd, frame, frame_len);
}

/**
 * \brief Emit noise reduction level control frame
 */
int sidecar_emit_nr_level(
    int fd, uint32_t receiver, float nr_level)
{
    uint8_t frame[SIDECAR_HEADER_LEN];
    int frame_len;
    uint32_t level_bits;

    /* Reinterpret float as uint32 for transmission */
    memcpy(&level_bits, &nr_level, sizeof(uint32_t));

    frame_len = sidecar_build_frame(frame, sizeof(frame),
                                    receiver, 0, 0, level_bits,
                                    SIDECAR_STREAM_NR_LEVEL, 0,
                                    NULL, 0);
    if (frame_len < 0)
    {
        return frame_len;
    }

    return sidecar_send_frame(fd, frame, frame_len);
}

/**
 * \brief Emit noise blanker level control frame
 */
int sidecar_emit_nb_level(
    int fd, uint32_t receiver, float nb_level)
{
    uint8_t frame[SIDECAR_HEADER_LEN];
    int frame_len;
    uint32_t level_bits;

    /* Reinterpret float as uint32 for transmission */
    memcpy(&level_bits, &nb_level, sizeof(uint32_t));

    frame_len = sidecar_build_frame(frame, sizeof(frame),
                                    receiver, 0, 0, level_bits,
                                    SIDECAR_STREAM_NB_LEVEL, 0,
                                    NULL, 0);
    if (frame_len < 0)
    {
        return frame_len;
    }

    return sidecar_send_frame(fd, frame, frame_len);
}

/**
 * \brief Emit notch filter control frame
 */
int sidecar_emit_notch(
    int fd, uint32_t receiver, int notch_hz, int enable)
{
    uint8_t frame[SIDECAR_HEADER_LEN];
    int frame_len;

    frame_len = sidecar_build_frame(frame, sizeof(frame),
                                    receiver, 0, 0, (uint32_t)notch_hz,
                                    SIDECAR_STREAM_NOTCH, (uint32_t)enable,
                                    NULL, 0);
    if (frame_len < 0)
    {
        return frame_len;
    }

    return sidecar_send_frame(fd, frame, frame_len);
}

/**
 * \brief Emit RF gain control frame
 */
int sidecar_emit_rf_gain(
    int fd, uint32_t receiver, float rf_gain)
{
    uint8_t frame[SIDECAR_HEADER_LEN];
    int frame_len;
    uint32_t gain_bits;

    /* Reinterpret float as uint32 for transmission */
    memcpy(&gain_bits, &rf_gain, sizeof(uint32_t));

    frame_len = sidecar_build_frame(frame, sizeof(frame),
                                    receiver, 0, 0, gain_bits,
                                    SIDECAR_STREAM_RF_GAIN, 0,
                                    NULL, 0);
    if (frame_len < 0)
    {
        return frame_len;
    }

    return sidecar_send_frame(fd, frame, frame_len);
}

/**
 * \brief Emit squelch level control frame
 */
int sidecar_emit_squelch(
    int fd, uint32_t receiver, float squelch)
{
    uint8_t frame[SIDECAR_HEADER_LEN];
    int frame_len;
    uint32_t squelch_bits;

    /* Reinterpret float as uint32 for transmission */
    memcpy(&squelch_bits, &squelch, sizeof(uint32_t));

    frame_len = sidecar_build_frame(frame, sizeof(frame),
                                    receiver, 0, 0, squelch_bits,
                                    SIDECAR_STREAM_SQUELCH, 0,
                                    NULL, 0);
    if (frame_len < 0)
    {
        return frame_len;
    }

    return sidecar_send_frame(fd, frame, frame_len);
}

/**
 * \brief Emit preamp gain control frame
 */
int sidecar_emit_preamp(
    int fd, uint32_t receiver, int preamp_db)
{
    uint8_t frame[SIDECAR_HEADER_LEN];
    int frame_len;

    frame_len = sidecar_build_frame(frame, sizeof(frame),
                                    receiver, 0, 0, (uint32_t)preamp_db,
                                    SIDECAR_STREAM_PREAMP, 0,
                                    NULL, 0);
    if (frame_len < 0)
    {
        return frame_len;
    }

    return sidecar_send_frame(fd, frame, frame_len);
}

/**
 * \brief Emit attenuator control frame
 */
int sidecar_emit_att(
    int fd, uint32_t receiver, int att_db)
{
    uint8_t frame[SIDECAR_HEADER_LEN];
    int frame_len;

    frame_len = sidecar_build_frame(frame, sizeof(frame),
                                    receiver, 0, 0, (uint32_t)att_db,
                                    SIDECAR_STREAM_ATT, 0,
                                    NULL, 0);
    if (frame_len < 0)
    {
        return frame_len;
    }

    return sidecar_send_frame(fd, frame, frame_len);
}

/**
 * \brief Emit CW pitch control frame
 */
int sidecar_emit_cw_pitch(
    int fd, uint32_t receiver, int pitch_hz)
{
    uint8_t frame[SIDECAR_HEADER_LEN];
    int frame_len;

    frame_len = sidecar_build_frame(frame, sizeof(frame),
                                    receiver, 0, 0, (uint32_t)pitch_hz,
                                    SIDECAR_STREAM_CW_PITCH, 0,
                                    NULL, 0);
    if (frame_len < 0)
    {
        return frame_len;
    }

    return sidecar_send_frame(fd, frame, frame_len);
}

/**
 * \brief Emit audio peak filter control frame
 */
int sidecar_emit_apf(
    int fd, uint32_t receiver, float apf_level)
{
    uint8_t frame[SIDECAR_HEADER_LEN];
    int frame_len;
    uint32_t level_bits;

    /* Reinterpret float as uint32 for transmission */
    memcpy(&level_bits, &apf_level, sizeof(uint32_t));

    frame_len = sidecar_build_frame(frame, sizeof(frame),
                                    receiver, 0, 0, level_bits,
                                    SIDECAR_STREAM_APF, 0,
                                    NULL, 0);
    if (frame_len < 0)
    {
        return frame_len;
    }

    return sidecar_send_frame(fd, frame, frame_len);
}

/**
 * \brief Emit PTT state control frame
 */
int sidecar_emit_ptt_state(
    int fd, uint32_t receiver, int ptt_on)
{
    uint8_t frame[SIDECAR_HEADER_LEN];
    int frame_len;

    frame_len = sidecar_build_frame(frame, sizeof(frame),
                                    receiver, 0, 0, (uint32_t)ptt_on,
                                    SIDECAR_STREAM_PTT_STATE, 0,
                                    NULL, 0);
    if (frame_len < 0)
    {
        return frame_len;
    }

    return sidecar_send_frame(fd, frame, frame_len);
}

/**
 * \brief Emit FM peak deviation control frame
 */
int sidecar_emit_fm_deviation(
    int fd, uint32_t receiver, int deviation_hz)
{
    uint8_t frame[SIDECAR_HEADER_LEN];
    int frame_len;

    frame_len = sidecar_build_frame(frame, sizeof(frame),
                                    receiver, 0, 0, (uint32_t)deviation_hz,
                                    SIDECAR_STREAM_FM_DEVIATION, 0,
                                    NULL, 0);
    if (frame_len < 0)
    {
        return frame_len;
    }

    return sidecar_send_frame(fd, frame, frame_len);
}
