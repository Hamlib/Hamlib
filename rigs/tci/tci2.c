/*
 *  Hamlib TCI 2.0 backend
 *  Copyright (c) 2026 by Jeff Francis N0GQ <gjfrancis@protonmail.com>
 *
 *  TCI (Transceiver Control Interface) protocol 2.0
 *  Specification: https://github.com/ExpertSDR3/TCI
 *
 *  Architecture notes:
 *    TCI is a push-based WebSocket protocol.  On connect the server streams
 *    all current state and ends with READY;.  Thereafter any parameter change
 *    (client- or server-initiated) is broadcast to every connected client.
 *
 *    Because responses are not strictly correlated with requests, this backend
 *    maintains a state cache (struct tci2_priv) that is updated by every
 *    incoming message via tci2_process_message().  tci2_recv_until() drives
 *    that loop while waiting for a specific reply token.
 *
 *    Multi-receiver hardware (e.g. SunSDR2 DX) exposes several independent
 *    TCI transceivers (TRX 0, 1, …).  The `trx' config parameter selects
 *    which one this backend instance controls.  Default is 0.
 *
 *    This backend is CAT-only: it does not relay TCI audio or IQ streams.
 *    Binary WebSocket frames carrying audio or IQ are silently discarded.
 *
 *  This library is free software; you can redistribute it and/or
 *  modify it under the terms of the GNU Lesser General Public
 *  License as published by the Free Software Foundation; either
 *  version 2.1 of the License, or (at your option) any later version.
 *
 *  This library is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 *  Lesser General Public License for more details.
 *
 *  You should have received a copy of the GNU Lesser General Public
 *  License along with this library; if not, write to the Free Software
 *  Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA
 */

#include "hamlib/config.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <ctype.h>
#include <time.h>
#include <unistd.h>

/* select() lives in different headers on POSIX vs Windows.
 * Match the pattern the rest of Hamlib uses (see src/network.c). */
#if defined (HAVE_SYS_SOCKET_H)
#  include <sys/socket.h>
#endif
#if defined (HAVE_SYS_SELECT_H)
#  include <sys/select.h>
#endif
#if defined (HAVE_SYS_TIME_H)
#  include <sys/time.h>
#endif
#if defined (HAVE_WINSOCK2_H)
#  include <winsock2.h>
#endif

#include "hamlib/rig.h"
#include "hamlib/port.h"
#include "hamlib/rig_state.h"
#include "iofunc.h"
#include "misc.h"
#include "tci2.h"

/* -------------------------------------------------------------------------
 * Constants
 * ---------------------------------------------------------------------- */

#define TCI2_DEFAULTPATH   "127.0.0.1:50001"
#define TCI2_BUFLEN        8192
#define TCI2_WS_BUFLEN     32768  /* Sized to absorb any binary IQ/audio
                                   * frame the server pushes -- they're
                                   * discarded but we still have to read
                                   * the bytes off the socket. */
#define TCI2_CMDLEN        512
#define TCI2_INIT_MAX_MSGS 256   /* max frames to drain during open */
#define TCI2_RECV_MAX      256   /* max frames to scan for a reply */
#define TCI2_MAX_RT_MODES  64    /* runtime mode table slots */

/* Fallback TX max power in mW if the caps table has no TX range list
 * (e.g. RX-only rig or a caps file that forgot to populate tx_range_list1).
 * Priv->tx_max_mw is normally initialized from caps->tx_range_list at
 * open time — see tci2_open. */
#define TCI2_DEFAULT_TX_MW 100000u  /* 100 W */

/* -------------------------------------------------------------------------
 * Static mode map: TCI mode string <-> Hamlib rmode_t
 *
 * Entries are tried in order for incoming strings (tci2_str_to_mode).
 * The runtime table (priv->rt_modes) is consulted first for outgoing
 * strings (tci2_mode_to_str) so the server's own preferred strings are used.
 * ---------------------------------------------------------------------- */

static const struct
{
    const char *tci;
    rmode_t     hamlib;
} tci2_mode_map[] =
{
    /* Primary names — sent to server if no runtime table entry */
    { "LSB",     RIG_MODE_LSB    },
    { "USB",     RIG_MODE_USB    },
    { "CW",      RIG_MODE_CW     },
    { "CWR",     RIG_MODE_CWR    },
    { "AM",      RIG_MODE_AM     },
    { "SAM",     RIG_MODE_SAM    },
    { "SAL",     RIG_MODE_SAL    },
    { "SAH",     RIG_MODE_SAH    },
    { "DSB",     RIG_MODE_DSB    },
    { "FM",      RIG_MODE_FM     },
    { "NFM",     RIG_MODE_FMN    },
    { "WFM",     RIG_MODE_WFM    },
    { "RTTY",    RIG_MODE_RTTY   },
    { "RTTY-R",  RIG_MODE_RTTYR  },
    { "DIGL",    RIG_MODE_PKTLSB },
    { "DIGU",    RIG_MODE_PKTUSB },
    { "C4FM",    RIG_MODE_C4FM   },
    /* Aliases seen on real hardware — receive-parse only */
    { "USB-D",   RIG_MODE_PKTUSB },
    { "USB-D1",  RIG_MODE_PKTUSB },
    { "USB-D2",  RIG_MODE_PKTUSB },
    { "USB-D3",  RIG_MODE_PKTUSB },
    { "LSB-D",   RIG_MODE_PKTLSB },
    { "LSB-D1",  RIG_MODE_PKTLSB },
    { "LSB-D2",  RIG_MODE_PKTLSB },
    { "LSB-D3",  RIG_MODE_PKTLSB },
    { "USER-U",  RIG_MODE_PKTUSB },
    { "USER-L",  RIG_MODE_PKTLSB },
    { "UCW",     RIG_MODE_CW     },
    { "PKT",     RIG_MODE_RTTY   },
    { NULL,      0               }
};

/* -------------------------------------------------------------------------
 * Private state
 * ---------------------------------------------------------------------- */

struct tci2_priv
{
    /* Radio state cache */
    freq_t  freq[2];          /* VFO A (0) and B (1), Hz */
    char    mode_str[32];     /* current mode as TCI string */
    int     filter_low;       /* RX_FILTER_BAND low offset, Hz */
    int     filter_high;      /* RX_FILTER_BAND high offset, Hz */
    ptt_t   ptt;
    split_t split;
    int     rit_enabled;
    int     xit_enabled;
    int     rit_offset;       /* Hz */
    int     xit_offset;       /* Hz */
    int     volume_db;        /* -60..0 */
    int     drive;            /* 0..100 */
    int     squelch_enabled;
    int     squelch_level;    /* -140..0 dB */
    float   signal_strength;  /* dBm (from RX_CHANNEL_SENSORS) */
    int     nb_enabled;
    int     nb_threshold;     /* 1..100 */
    int     nb_duration;      /* 1..300 ms */
    int     nr_enabled;
    int     anf_enabled;
    int     lock_enabled;
    int     agc_mode;         /* 0=off 1=fast 2=normal */
    int     cw_speed_wpm;     /* CW keyer speed (CW_MACROS_SPEED), WPM */
    char    txsource[16];     /* TX audio source: "default", "mic", "vac" */
    float   tx_power_w;       /* TX power output, W RMS (from TX_SENSORS) */
    float   tx_swr;           /* SWR (from TX_SENSORS) */
    int     mute_enabled;
    int     tune_enabled;
    int     digl_offset;      /* DIGL mode frequency offset, Hz (0..4000) */
    int     digu_offset;      /* DIGU mode frequency offset, Hz (0..4000) */

    /* Device info received during init */
    char    device[64];
    char    modulations_list[512]; /* raw MODULATIONS_LIST from server */
    int     trx_count;
    int     channel_count;
    int     receive_only;
    freq_t  vfo_limits_low;
    freq_t  vfo_limits_high;
    int     ready;

    /* Runtime mode table (server's own strings) */
    struct
    {
        rmode_t hamlib;
        char    tci[32];
    } rt_modes[TCI2_MAX_RT_MODES];
    int rt_mode_count;

    /* Configuration */
    int     trx_num;          /* which TRX to control (0-based) */
    vfo_t   current_vfo;

    /* Per-radio maximum TX power in mW, derived from caps->tx_range_list
     * at open time.  Used by tci2_power2mW / tci2_mW2power so the 0..1
     * normalized value is relative to *this* rig's max, not a hardcoded
     * global constant. */
    unsigned int tx_max_mw;
};

/* -------------------------------------------------------------------------
 * Forward declarations
 * ---------------------------------------------------------------------- */

static rmode_t tci2_str_to_mode(const char *s);

/* -------------------------------------------------------------------------
 * Backend config parameters
 * ---------------------------------------------------------------------- */

const struct confparams tci2_cfg_params[] =
{
    {
        TOK_TCI2_TRX, "trx", "Transceiver Number",
        "TCI transceiver (receiver) index (0-based). "
        "Selects which TRX to control on multi-receiver hardware "
        "(e.g. SunSDR2 DX has two).",
        "0", RIG_CONF_INT, { .n = { .min = 0, .max = 7, .step = 1 } }
    },
    {
        TOK_TCI2_TXSRC, "txsource", "TX Audio Source",
        "Audio source used when keying TX: default, mic, or vac. "
        "Overridden per-PTT-call by RIG_PTT_ON_MIC (mic) and RIG_PTT_ON_DATA (vac).",
        "default", RIG_CONF_STRING, {}
    },
    {
        TOK_TCI2_DIGL_OFFSET, "digl_offset", "DIGL Frequency Offset",
        "Frequency offset for DIGL (digital lower sideband) mode, Hz (0..4000). "
        "Sent to the server on connect.",
        "0", RIG_CONF_INT, { .n = { .min = 0, .max = 4000, .step = 1 } }
    },
    {
        TOK_TCI2_DIGU_OFFSET, "digu_offset", "DIGU Frequency Offset",
        "Frequency offset for DIGU (digital upper sideband) mode, Hz (0..4000). "
        "Sent to the server on connect.",
        "0", RIG_CONF_INT, { .n = { .min = 0, .max = 4000, .step = 1 } }
    },
    { RIG_CONF_END, NULL, NULL, NULL, NULL, RIG_CONF_STRING, {} }
};

int tci2_set_conf(RIG *rig, hamlib_token_t token, const char *val)
{
    struct tci2_priv *priv = (struct tci2_priv *)STATE(rig)->priv;

    switch (token)
    {
    case TOK_TCI2_TRX:
    {
        int n = atoi(val);

        if (n < 0 || n > 7)
        {
            return -RIG_EINVAL;
        }

        priv->trx_num = n;
        return RIG_OK;
    }

    case TOK_TCI2_TXSRC:
        if (strcasecmp(val, "mic")     == 0 ||
                strcasecmp(val, "vac")     == 0 ||
                strcasecmp(val, "default") == 0)
        {
            strncpy(priv->txsource, val, sizeof(priv->txsource) - 1);
            priv->txsource[sizeof(priv->txsource) - 1] = '\0';
            return RIG_OK;
        }

        return -RIG_EINVAL;

    case TOK_TCI2_DIGL_OFFSET:
    {
        int off = atoi(val);

        if (off < 0 || off > 4000) { return -RIG_EINVAL; }

        priv->digl_offset = off;
        return RIG_OK;
    }

    case TOK_TCI2_DIGU_OFFSET:
    {
        int off = atoi(val);

        if (off < 0 || off > 4000) { return -RIG_EINVAL; }

        priv->digu_offset = off;
        return RIG_OK;
    }

    default:
        return -RIG_EINVAL;
    }
}

int tci2_get_conf(RIG *rig, hamlib_token_t token, char *val)
{
    struct tci2_priv *priv = (struct tci2_priv *)STATE(rig)->priv;

    switch (token)
    {
    case TOK_TCI2_TRX:
        SNPRINTF(val, 8, "%d", priv->trx_num);
        return RIG_OK;

    case TOK_TCI2_TXSRC:
        strncpy(val, priv->txsource, 16);
        val[15] = '\0';
        return RIG_OK;

    case TOK_TCI2_DIGL_OFFSET:
        SNPRINTF(val, 8, "%d", priv->digl_offset);
        return RIG_OK;

    case TOK_TCI2_DIGU_OFFSET:
        SNPRINTF(val, 8, "%d", priv->digu_offset);
        return RIG_OK;

    default:
        return -RIG_EINVAL;
    }
}

/* -------------------------------------------------------------------------
 * WebSocket helpers
 * ---------------------------------------------------------------------- */

static void ws_b64encode(const unsigned char *in, int inlen, char *out)
{
    static const char b64[] =
        "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
    int i = 0, j = 0;

    while (i < inlen - 2)
    {
        out[j++] = b64[(in[i] >> 2) & 0x3F];
        out[j++] = b64[((in[i] & 3) << 4) | ((in[i + 1] >> 4) & 0xF)];
        out[j++] = b64[((in[i + 1] & 0xF) << 2) | ((in[i + 2] >> 6) & 3)];
        out[j++] = b64[in[i + 2] & 0x3F];
        i += 3;
    }

    if (i < inlen)
    {
        out[j++] = b64[(in[i] >> 2) & 0x3F];

        if (i == inlen - 1)
        {
            out[j++] = b64[(in[i] & 3) << 4];
            out[j++] = '=';
        }
        else
        {
            out[j++] = b64[((in[i] & 3) << 4) | ((in[i + 1] >> 4) & 0xF)];
            out[j++] = b64[(in[i + 1] & 0xF) << 2];
        }

        out[j++] = '=';
    }

    out[j] = '\0';
}

/*
 * Send the HTTP/1.1 WebSocket upgrade request and consume server response
 * headers.  Leaves the socket positioned at the first WebSocket frame byte.
 */
static int ws_handshake(RIG *rig)
{
    char buf[1024];
    char req[768];
    unsigned char key_raw[16];
    char key_b64[25];
    int retval, got_101 = 0;
    hamlib_port_t *rp = RIGPORT(rig);

    srand((unsigned int)time(NULL) ^ (unsigned int)(uintptr_t)rig);

    for (int i = 0; i < 16; i++)
    {
        key_raw[i] = (unsigned char)(rand() & 0xFF);
    }

    ws_b64encode(key_raw, 16, key_b64);

    SNPRINTF(req, sizeof(req),
             "GET / HTTP/1.1\r\n"
             "Host: %s\r\n"
             "Upgrade: websocket\r\n"
             "Connection: Upgrade\r\n"
             "Sec-WebSocket-Key: %s\r\n"
             "Sec-WebSocket-Version: 13\r\n"
             "\r\n",
             rp->pathname, key_b64);

    retval = write_block(rp, (unsigned char *)req, strlen(req));

    if (retval < 0)
    {
        return retval;
    }

    /* Read HTTP response headers one line at a time until blank line */
    for (int i = 0; i < 64; i++)
    {
        retval = read_string(rp, (unsigned char *)buf, sizeof(buf) - 1,
                             "\n", 1, 0, 1);

        if (retval <= 0)
        {
            return -RIG_EIO;
        }

        buf[retval] = '\0';
        rig_debug(RIG_DEBUG_VERBOSE, "%s: hdr: %s", __func__, buf);

        if (strstr(buf, " 101 "))
        {
            got_101 = 1;
        }

        /* Strip trailing CR/LF; empty line = end of headers */
        char *p = buf + strlen(buf) - 1;

        while (p >= buf && (*p == '\r' || *p == '\n'))
        {
            *p-- = '\0';
        }

        if (buf[0] == '\0')
        {
            break;
        }
    }

    if (!got_101)
    {
        rig_debug(RIG_DEBUG_ERR,
                  "%s: server did not return 101 Switching Protocols\n",
                  __func__);
        return -RIG_EPROTO;
    }

    return RIG_OK;
}

/*
 * Send one masked WebSocket frame with FIN=1.  Generic — used for text
 * (opcode 0x1), Pong (0xA), and Close (0x8).  Control frames per
 * RFC 6455 §5.5 must have payload ≤125 bytes; text/binary can be larger
 * but this helper caps at TCI2_CMDLEN (no fragmentation on the send path
 * — every message we generate fits in one frame).
 */
static int ws_send_frame(RIG *rig, unsigned char opcode,
                         const unsigned char *payload, size_t plen)
{
    unsigned char frame[TCI2_CMDLEN + 16];
    unsigned char mask[4];
    size_t flen;

    if (plen + 16 > sizeof(frame))
    {
        return -RIG_ETRUNC;
    }

    mask[0] = (unsigned char)(rand() & 0xFF);
    mask[1] = (unsigned char)(rand() & 0xFF);
    mask[2] = (unsigned char)(rand() & 0xFF);
    mask[3] = (unsigned char)(rand() & 0xFF);

    frame[0] = (unsigned char)(0x80 | (opcode & 0x0F)); /* FIN=1 + opcode */

    if (plen <= 125)
    {
        frame[1] = (unsigned char)(0x80 | plen);
        memcpy(frame + 2, mask, 4);

        for (size_t i = 0; i < plen; i++)
        {
            frame[6 + i] = payload[i] ^ mask[i & 3];
        }

        flen = 6 + plen;
    }
    else
    {
        frame[1] = (unsigned char)(0x80 | 126);
        frame[2] = (unsigned char)((plen >> 8) & 0xFF);
        frame[3] = (unsigned char)(plen & 0xFF);
        memcpy(frame + 4, mask, 4);

        for (size_t i = 0; i < plen; i++)
        {
            frame[8 + i] = payload[i] ^ mask[i & 3];
        }

        flen = 8 + plen;
    }

    return write_block(RIGPORT(rig), frame, flen);
}

/* Convenience: send a text (opcode 0x1) frame. */
static int ws_send_text(RIG *rig, const char *text)
{
    return ws_send_frame(rig, 0x1, (const unsigned char *)text, strlen(text));
}

/* Send an RFC 6455 §5.5.1 Close frame with an optional status code.
 * A status of 0 means send an empty payload (allowed by the spec). */
static int ws_send_close(RIG *rig, unsigned short status)
{
    unsigned char payload[2];

    if (status == 0)
    {
        return ws_send_frame(rig, 0x8, NULL, 0);
    }

    payload[0] = (unsigned char)((status >> 8) & 0xFF);
    payload[1] = (unsigned char)(status & 0xFF);
    return ws_send_frame(rig, 0x8, payload, 2);
}

/* Send an RFC 6455 §5.5.3 Pong frame echoing the received Ping payload. */
static int ws_send_pong(RIG *rig, const unsigned char *payload, size_t plen)
{
    return ws_send_frame(rig, 0xA, payload, plen);
}

/*
 * Read one raw WebSocket frame from the socket into `buf` (up to buflen
 * bytes; larger payloads are drained and truncated).  Returns the *actual*
 * payload length read into buf (which may equal `plen_full` on success
 * or 0 on truncation), with the frame header state exposed via the out
 * params.  Applies mask unmasking to bytes stored in buf.
 *
 * This is the low-level primitive.  ws_recv_frame() below composes calls
 * to this into a full RFC 6455 message-assembly loop, dispatching
 * control frames inline (Ping→Pong, Close handshake).
 *
 * Out params:
 *   *opcode_out    — WebSocket opcode (0x0 continuation, 0x1 text,
 *                    0x2 binary, 0x8 close, 0x9 ping, 0xA pong)
 *   *fin_out       — 1 if this is the last frame of the message, else 0
 *   *plen_full_out — original payload length on the wire (may exceed
 *                    what actually fits in buf)
 */
static int ws_read_one_frame(RIG *rig,
                             unsigned char *buf, size_t buflen,
                             int *opcode_out, int *fin_out,
                             size_t *plen_full_out)
{
    unsigned char hdr[2];
    unsigned char ext[8];
    unsigned char mask[4];
    unsigned char discard[256];
    hamlib_port_t *rp = RIGPORT(rig);
    int retval;
    size_t plen;
    size_t stored;
    int masked, opcode, fin;

    retval = read_block(rp, hdr, 2);
    if (retval != 2)
    {
        return (retval < 0) ? retval : -RIG_EIO;
    }

    fin    = (hdr[0] & 0x80) != 0;
    opcode = hdr[0] & 0x0F;
    masked = (hdr[1] & 0x80) != 0;
    plen   = hdr[1] & 0x7F;

    if (plen == 126)
    {
        retval = read_block(rp, ext, 2);
        if (retval != 2) { return -RIG_EIO; }
        plen = ((size_t)ext[0] << 8) | ext[1];
    }
    else if (plen == 127)
    {
        retval = read_block(rp, ext, 8);
        if (retval != 8) { return -RIG_EIO; }
        plen = ((size_t)ext[4] << 24) | ((size_t)ext[5] << 16) |
               ((size_t)ext[6] << 8)  | ext[7];
    }

    if (masked)
    {
        retval = read_block(rp, mask, 4);
        if (retval != 4) { return -RIG_EIO; }
    }

    /* Read as much as fits into buf, drain the rest. */
    stored = (plen > buflen) ? buflen : plen;

    if (stored > 0)
    {
        retval = read_block(rp, buf, stored);
        if (retval != (int)stored) { return -RIG_EIO; }

        if (masked)
        {
            for (size_t i = 0; i < stored; i++)
            {
                buf[i] ^= mask[i & 3];
            }
        }
    }

    /* Drain any overflow that didn't fit. */
    {
        size_t remain = plen - stored;
        while (remain > 0)
        {
            size_t chunk = (remain > sizeof(discard)) ? sizeof(discard) : remain;
            retval = read_block(rp, discard, chunk);
            if (retval != (int)chunk) { return -RIG_EIO; }
            remain -= chunk;
        }
    }

    *opcode_out    = opcode;
    *fin_out       = fin;
    *plen_full_out = plen;
    return (int)stored;
}

/*
 * Receive one complete WebSocket message from the server.
 * Handles per RFC 6455:
 *   §5.4  fragmentation reassembly (FIN bit + continuation opcode 0x0)
 *   §5.5.2 Ping (0x9) → Pong (0xA) auto-response
 *   §5.5.1 Close (0x8) handshake — echo Close back and return -RIG_EIO
 *   binary frames (0x2) — drained/discarded (CAT-only backend)
 *
 * Returns payload length on success (buf is NUL-terminated text),
 * 0 for a binary message (audio/IQ — buf is empty),
 * or a negative Hamlib error code (on Close, transport error, etc.).
 */
static int ws_recv_frame(RIG *rig, char *buf, int buflen)
{
    unsigned char frame_buf[TCI2_WS_BUFLEN];
    size_t assembled = 0;      /* bytes accumulated in buf across frames */
    int msg_opcode = -1;       /* opcode of the message being assembled  */
    int is_binary = 0;
    int truncated = 0;
    int frames_seen = 0;

    /* Cap total frames per message to bound worst-case latency if a peer
     * sends a pathological continuation chain. */
    while (frames_seen++ < TCI2_RECV_MAX)
    {
        int opcode, fin;
        size_t plen_full;
        int stored = ws_read_one_frame(rig, frame_buf, sizeof(frame_buf),
                                       &opcode, &fin, &plen_full);
        if (stored < 0) { return stored; }

        /* --- Control frames (§5.5): opcodes 0x8, 0x9, 0xA.  May be
         * interleaved with data-frame fragments and MUST be handled
         * without disturbing the reassembly buffer. */
        if (opcode & 0x08)
        {
            if (opcode == 0x09)  /* Ping */
            {
                ws_send_pong(rig, frame_buf, (size_t)stored);
                continue;
            }
            if (opcode == 0x0A)  /* Pong — nothing to do */
            {
                continue;
            }
            if (opcode == 0x08)  /* Close */
            {
                /* Echo back a Close if we haven't already sent one.
                 * Best-effort — ignore write errors. */
                ws_send_close(rig, 0);
                return -RIG_EIO;
            }
            /* Unknown control opcode — ignore */
            continue;
        }

        /* --- Data frames: 0x0 (continuation), 0x1 (text), 0x2 (binary). */
        if (opcode == 0x0)
        {
            /* Continuation of an in-progress message */
            if (msg_opcode < 0)
            {
                rig_debug(RIG_DEBUG_WARN,
                          "%s: continuation frame with no message in progress\n",
                          __func__);
                return -RIG_EPROTO;
            }
        }
        else
        {
            /* Start of a new message */
            if (msg_opcode >= 0)
            {
                rig_debug(RIG_DEBUG_WARN,
                          "%s: new data frame before previous FIN\n", __func__);
                return -RIG_EPROTO;
            }
            msg_opcode = opcode;
            is_binary = (opcode == 0x02);
        }

        if (is_binary)
        {
            /* CAT-only — silently discard. */
            if (fin)
            {
                buf[0] = '\0';
                return 0;
            }
            continue;
        }

        /* Text — accumulate into caller buf. */
        if (stored < (int)plen_full)
        {
            /* This fragment was already truncated by ws_read_one_frame
             * because our internal frame_buf overflowed.  Skip it. */
            truncated = 1;
        }
        else if (!truncated)
        {
            size_t space = (size_t)buflen - 1 - assembled;
            size_t take = ((size_t)stored > space) ? space : (size_t)stored;
            memcpy(buf + assembled, frame_buf, take);
            assembled += take;
            if (take < (size_t)stored) { truncated = 1; }
        }

        if (!fin) { continue; }

        /* End of message */
        if (truncated)
        {
            rig_debug(RIG_DEBUG_WARN,
                      "%s: dropped oversized text message\n", __func__);
            buf[0] = '\0';
            return 0;
        }

        buf[assembled] = '\0';

        /* Normalize: uppercase the command keyword (before the first ':') so
         * sscanf format strings and prefix comparisons work regardless of
         * whether the server sends "VFO:" or "vfo:". */
        for (size_t i = 0; i < assembled &&
                           buf[i] != ':' && buf[i] != ';'; i++)
        {
            buf[i] = (char)toupper((unsigned char)buf[i]);
        }

        rig_debug(RIG_DEBUG_TRACE, "%s: recv: %s\n", __func__, buf);
        return (int)assembled;
    }

    rig_debug(RIG_DEBUG_ERR,
              "%s: message exceeded %d frames\n", __func__, TCI2_RECV_MAX);
    return -RIG_EPROTO;
}

/* -------------------------------------------------------------------------
 * TCI protocol layer
 * ---------------------------------------------------------------------- */

/* Forward declaration -- tci2_drain uses tci2_process_message via
 * the same recv path, but the recv loop below uses tci2_process_message
 * directly, so we forward-declare it for tci2_drain. */
static void tci2_process_message(RIG *rig, const char *msg);
static void tci2_build_mode_list(RIG *rig);

/*
 * Drain any frames sitting in the socket buffer right now.  TCI is
 * push-based: the server echoes every SET command and emits unsolicited
 * sensor updates (RX_SENSORS, TX_SENSORS) at the subscription rate.  In
 * a CAT-only backend with no continuous reader thread these accumulate
 * between user-driven get/set calls, so the next tci2_recv_until() can
 * match its prefix on a stale echo rather than the fresh reply.
 *
 * Called at the head of every send-then-recv query.  Updates the state
 * cache for any text frames drained.
 */
static int tci2_drain(RIG *rig)
{
    hamlib_port_t *rp = RIGPORT(rig);
    char buf[TCI2_WS_BUFLEN];
    int drained = 0;

    while (drained < TCI2_RECV_MAX)
    {
        fd_set rfds;
        struct timeval tv = { 0, 0 };  /* non-blocking poll */
        int pr;

        FD_ZERO(&rfds);
        FD_SET(rp->fd, &rfds);

        pr = select(rp->fd + 1, &rfds, NULL, NULL, &tv);
        if (pr <= 0) { break; }
        if (!FD_ISSET(rp->fd, &rfds)) { break; }

        int retval = ws_recv_frame(rig, buf, sizeof(buf));
        if (retval < 0) { return retval; }

        if (buf[0] != '\0')
        {
            tci2_process_message(rig, buf);
        }

        drained++;
    }

    if (drained > 0)
    {
        rig_debug(RIG_DEBUG_TRACE, "%s: drained %d stale frame(s)\n",
                  __func__, drained);
    }

    return RIG_OK;
}

/*
 * Reconnect to the TCI server after a connection failure.
 * Closes the socket, reopens TCP, performs WebSocket handshake, drains
 * until READY, rebuilds mode table, and re-subscribes to sensors.
 * Returns RIG_OK on success or a negative error code.
 */
static int tci2_reconnect(RIG *rig)
{
    struct tci2_priv *priv = (struct tci2_priv *)STATE(rig)->priv;
    hamlib_port_t *rp = RIGPORT(rig);
    char buf[TCI2_WS_BUFLEN];
    int retval;

    rig_debug(RIG_DEBUG_WARN, "%s: attempting reconnect\n", __func__);

    port_close(rp, RIG_PORT_NETWORK);

    hl_usleep(100 * 1000);

    retval = port_open(rp);

    if (retval != RIG_OK)
    {
        rig_debug(RIG_DEBUG_ERR, "%s: port_open failed: %s\n",
                  __func__, rigerror(retval));
        return retval;
    }

    retval = ws_handshake(rig);

    if (retval != RIG_OK)
    {
        rig_debug(RIG_DEBUG_ERR, "%s: WebSocket handshake failed: %s\n",
                  __func__, rigerror(retval));
        return retval;
    }

    priv->ready = 0;

    for (int i = 0; i < TCI2_INIT_MAX_MSGS && !priv->ready; i++)
    {
        retval = ws_recv_frame(rig, buf, sizeof(buf));

        if (retval < 0)
        {
            rig_debug(RIG_DEBUG_ERR,
                      "%s: recv error waiting for READY: %s\n",
                      __func__, rigerror(retval));
            return retval;
        }

        if (buf[0] != '\0')
        {
            tci2_process_message(rig, buf);
        }
    }

    if (!priv->ready)
    {
        rig_debug(RIG_DEBUG_ERR, "%s: READY not received after reconnect\n",
                  __func__);
        return -RIG_ETIMEOUT;
    }

    tci2_build_mode_list(rig);

    ws_send_text(rig, "RX_SENSORS_ENABLE:true,200;");
    ws_send_text(rig, "TX_SENSORS_ENABLE:true,200;");

    if (priv->digl_offset != 0)
    {
        char cmd[TCI2_CMDLEN];
        SNPRINTF(cmd, sizeof(cmd), "DIGL_OFFSET:%d;", priv->digl_offset);
        ws_send_text(rig, cmd);
    }

    if (priv->digu_offset != 0)
    {
        char cmd[TCI2_CMDLEN];
        SNPRINTF(cmd, sizeof(cmd), "DIGU_OFFSET:%d;", priv->digu_offset);
        ws_send_text(rig, cmd);
    }

    rig_debug(RIG_DEBUG_VERBOSE,
              "%s: reconnected — device=%s trx=%d\n",
              __func__, priv->device, priv->trx_num);

    return RIG_OK;
}

static int tci2_send(RIG *rig, const char *cmd)
{
    /* Drain any stale echoes / sensor pushes that have queued in the
     * kernel buffer since our last syscall.  Without this, a subsequent
     * tci2_recv_until() can match its prefix against a previous SET's
     * echo and return one cycle stale data. */
    tci2_drain(rig);

    rig_debug(RIG_DEBUG_VERBOSE, "%s: send: %s\n", __func__, cmd);
    return ws_send_text(rig, cmd);
}

/*
 * Update the private state cache from a received TCI message.
 * Only updates fields for the configured TRX (priv->trx_num).
 */
static void tci2_process_message(RIG *rig, const char *msg)
{
    struct tci2_priv *priv = (struct tci2_priv *)STATE(rig)->priv;
    const int trx_sel = priv->trx_num;
    char val[64];
    int trx, ch, n;

    if (strncasecmp(msg, "READY", 5) == 0)
    {
        priv->ready = 1;
    }
    else if (strncasecmp(msg, "VFO_LIMITS:", 11) == 0)
    {
        double lo, hi;

        if (sscanf(msg + 11, "%lf,%lf", &lo, &hi) == 2)
        {
            priv->vfo_limits_low  = (freq_t)lo;
            priv->vfo_limits_high = (freq_t)hi;
        }
    }
    else if (strncasecmp(msg, "TRX_COUNT:", 10) == 0)
    {
        sscanf(msg + 10, "%d", &priv->trx_count);
    }
    else if (strncasecmp(msg, "CHANNEL_COUNT:", 14) == 0)
    {
        sscanf(msg + 14, "%d", &priv->channel_count);
    }
    else if (strncasecmp(msg, "DEVICE:", 7) == 0)
    {
        strncpy(priv->device, msg + 7, sizeof(priv->device) - 1);
        priv->device[sizeof(priv->device) - 1] = '\0';
        char *p = strchr(priv->device, ';');

        if (p) { *p = '\0'; }
    }
    else if (strncasecmp(msg, "RECEIVE_ONLY:", 13) == 0)
    {
        strncpy(val, msg + 13, sizeof(val) - 1);
        val[sizeof(val) - 1] = '\0';
        char *p = strchr(val, ';');

        if (p) { *p = '\0'; }

        priv->receive_only = (strcasecmp(val, "true") == 0);
    }
    else if (strncasecmp(msg, "MODULATIONS_LIST:", 17) == 0)
    {
        strncpy(priv->modulations_list, msg + 17,
                sizeof(priv->modulations_list) - 1);
        priv->modulations_list[sizeof(priv->modulations_list) - 1] = '\0';
        char *p = strchr(priv->modulations_list, ';');

        if (p) { *p = '\0'; }
    }
    else if (strncasecmp(msg, "VFO:", 4) == 0)
    {
        double freq;
        n = sscanf(msg + 4, "%d,%d,%lf", &trx, &ch, &freq);

        if (n == 3 && trx == trx_sel && ch >= 0 && ch <= 1)
        {
            priv->freq[ch] = (freq_t)freq;
        }
    }
    else if (strncasecmp(msg, "MODULATION:", 11) == 0)
    {
        char mode[32] = {0};
        n = sscanf(msg + 11, "%d,%31s", &trx, mode);

        if (n == 2 && trx == trx_sel)
        {
            char *p = strchr(mode, ';');

            if (p) { *p = '\0'; }

            strncpy(priv->mode_str, mode, sizeof(priv->mode_str) - 1);
            priv->mode_str[sizeof(priv->mode_str) - 1] = '\0';
        }
    }
    else if (strncasecmp(msg, "RX_FILTER_BAND:", 15) == 0)
    {
        int lo, hi;
        n = sscanf(msg + 15, "%d,%d,%d", &trx, &lo, &hi);

        if (n == 3 && trx == trx_sel)
        {
            priv->filter_low  = lo;
            priv->filter_high = hi;
        }
    }
    else if (strncasecmp(msg, "TRX:", 4) == 0)
    {
        char state[16] = {0};
        n = sscanf(msg + 4, "%d,%15s", &trx, state);

        if (n == 2 && trx == trx_sel)
        {
            char *p = strchr(state, ';');

            if (p) { *p = '\0'; }

            p = strchr(state, ','); /* strip optional signal source arg */

            if (p) { *p = '\0'; }

            priv->ptt = (strcasecmp(state, "true") == 0)
                        ? RIG_PTT_ON : RIG_PTT_OFF;
        }
    }
    else if (strncasecmp(msg, "SPLIT_ENABLE:", 13) == 0)
    {
        n = sscanf(msg + 13, "%d,%63s", &trx, val);

        if (n == 2 && trx == trx_sel)
        {
            char *p = strchr(val, ';');

            if (p) { *p = '\0'; }

            priv->split = (strcasecmp(val, "true") == 0)
                          ? RIG_SPLIT_ON : RIG_SPLIT_OFF;
        }
    }
    else if (strncasecmp(msg, "RIT_ENABLE:", 11) == 0)
    {
        n = sscanf(msg + 11, "%d,%63s", &trx, val);

        if (n == 2 && trx == trx_sel)
        {
            char *p = strchr(val, ';');

            if (p) { *p = '\0'; }

            priv->rit_enabled = (strcasecmp(val, "true") == 0);
        }
    }
    else if (strncasecmp(msg, "RIT_OFFSET:", 11) == 0)
    {
        int off;
        n = sscanf(msg + 11, "%d,%d", &trx, &off);

        if (n == 2 && trx == trx_sel)
        {
            priv->rit_offset = off;
        }
    }
    else if (strncasecmp(msg, "XIT_ENABLE:", 11) == 0)
    {
        n = sscanf(msg + 11, "%d,%63s", &trx, val);

        if (n == 2 && trx == trx_sel)
        {
            char *p = strchr(val, ';');

            if (p) { *p = '\0'; }

            priv->xit_enabled = (strcasecmp(val, "true") == 0);
        }
    }
    else if (strncasecmp(msg, "XIT_OFFSET:", 11) == 0)
    {
        int off;
        n = sscanf(msg + 11, "%d,%d", &trx, &off);

        if (n == 2 && trx == trx_sel)
        {
            priv->xit_offset = off;
        }
    }
    else if (strncasecmp(msg, "VOLUME:", 7) == 0)
    {
        /* VOLUME has no TRX argument — it is global */
        int vol;

        if (sscanf(msg + 7, "%d", &vol) == 1)
        {
            priv->volume_db = vol;
        }
    }
    else if (strncasecmp(msg, "DRIVE:", 6) == 0)
    {
        int drv;
        n = sscanf(msg + 6, "%d,%d", &trx, &drv);

        if (n == 2 && trx == trx_sel)
        {
            priv->drive = drv;
        }
    }
    else if (strncasecmp(msg, "SQL_ENABLE:", 11) == 0)
    {
        n = sscanf(msg + 11, "%d,%63s", &trx, val);

        if (n == 2 && trx == trx_sel)
        {
            char *p = strchr(val, ';');

            if (p) { *p = '\0'; }

            priv->squelch_enabled = (strcasecmp(val, "true") == 0);
        }
    }
    else if (strncasecmp(msg, "SQL_LEVEL:", 10) == 0)
    {
        int lev;
        n = sscanf(msg + 10, "%d,%d", &trx, &lev);

        if (n == 2 && trx == trx_sel)
        {
            priv->squelch_level = lev;
        }
    }
    else if (strncasecmp(msg, "RX_CHANNEL_SENSORS:", 19) == 0)
    {
        float lev;
        n = sscanf(msg + 19, "%d,%d,%f", &trx, &ch, &lev);

        if (n == 3 && trx == trx_sel && ch == 0)
        {
            priv->signal_strength = lev;
        }
    }
    else if (strncasecmp(msg, "RX_NB_ENABLE:", 13) == 0)
    {
        n = sscanf(msg + 13, "%d,%63s", &trx, val);

        if (n == 2 && trx == trx_sel)
        {
            char *p = strchr(val, ';');

            if (p) { *p = '\0'; }

            priv->nb_enabled = (strcasecmp(val, "true") == 0);
        }
    }
    else if (strncasecmp(msg, "RX_NB_PARAM:", 12) == 0)
    {
        int thresh, dur;
        n = sscanf(msg + 12, "%d,%d,%d", &trx, &thresh, &dur);

        if (n == 3 && trx == trx_sel)
        {
            priv->nb_threshold = thresh;
            priv->nb_duration  = dur;
        }
    }
    else if (strncasecmp(msg, "RX_NR_ENABLE:", 13) == 0)
    {
        n = sscanf(msg + 13, "%d,%63s", &trx, val);

        if (n == 2 && trx == trx_sel)
        {
            char *p = strchr(val, ';');

            if (p) { *p = '\0'; }

            priv->nr_enabled = (strcasecmp(val, "true") == 0);
        }
    }
    else if (strncasecmp(msg, "RX_ANF_ENABLE:", 14) == 0)
    {
        n = sscanf(msg + 14, "%d,%63s", &trx, val);

        if (n == 2 && trx == trx_sel)
        {
            char *p = strchr(val, ';');

            if (p) { *p = '\0'; }

            priv->anf_enabled = (strcasecmp(val, "true") == 0);
        }
    }
    else if (strncasecmp(msg, "LOCK:", 5) == 0)
    {
        /* Bidirectional control command for VFO lock */
        n = sscanf(msg + 5, "%d,%63s", &trx, val);

        if (n == 2 && trx == trx_sel)
        {
            char *p = strchr(val, ';');

            if (p) { *p = '\0'; }

            priv->lock_enabled = (strcasecmp(val, "true") == 0);
        }
    }
    else if (strncasecmp(msg, "VFO_LOCK:", 9) == 0)
    {
        /* Server-to-client notification that a per-channel lock changed.
         * Update the cache using VFO A (channel 0) as the canonical lock. */
        int vfo_ch;
        n = sscanf(msg + 9, "%d,%d,%63s", &trx, &vfo_ch, val);

        if (n == 3 && trx == trx_sel && vfo_ch == 0)
        {
            char *p = strchr(val, ';');

            if (p) { *p = '\0'; }

            priv->lock_enabled = (strcasecmp(val, "true") == 0);
        }
    }
    else if (strncasecmp(msg, "MUTE:", 5) == 0)
    {
        strncpy(val, msg + 5, sizeof(val) - 1);
        val[sizeof(val) - 1] = '\0';
        char *p = strchr(val, ';');

        if (p) { *p = '\0'; }

        priv->mute_enabled = (strcasecmp(val, "true") == 0);
    }
    else if (strncasecmp(msg, "TUNE:", 5) == 0)
    {
        n = sscanf(msg + 5, "%d,%63s", &trx, val);

        if (n == 2 && trx == trx_sel)
        {
            char *p = strchr(val, ';');

            if (p) { *p = '\0'; }

            priv->tune_enabled = (strcasecmp(val, "true") == 0);
        }
    }
    else if (strncasecmp(msg, "TX_FREQUENCY:", 13) == 0)
    {
        double freq;

        if (sscanf(msg + 13, "%lf", &freq) == 1)
        {
            priv->freq[1] = (freq_t)freq;
        }
    }
    else if (strncasecmp(msg, "TX_FOOTSWITCH:", 14) == 0)
    {
        n = sscanf(msg + 14, "%d,%63s", &trx, val);

        if (n == 2 && trx == trx_sel)
        {
            char *p = strchr(val, ';');

            if (p) { *p = '\0'; }

            priv->ptt = (strcasecmp(val, "true") == 0)
                        ? RIG_PTT_ON : RIG_PTT_OFF;
        }
    }
    else if (strncasecmp(msg, "TX_SENSORS:", 11) == 0)
    {
        float mic_dbm, pwr_rms, pwr_peak, swr;
        n = sscanf(msg + 11, "%d,%f,%f,%f,%f",
                   &trx, &mic_dbm, &pwr_rms, &pwr_peak, &swr);

        if (n == 5 && trx == trx_sel)
        {
            priv->tx_power_w = pwr_rms;
            priv->tx_swr     = swr;
        }
    }
    else if (strncasecmp(msg, "DIGL_OFFSET:", 12) == 0)
    {
        int off;

        if (sscanf(msg + 12, "%d", &off) == 1)
        {
            priv->digl_offset = off;
        }
    }
    else if (strncasecmp(msg, "DIGU_OFFSET:", 12) == 0)
    {
        int off;

        if (sscanf(msg + 12, "%d", &off) == 1)
        {
            priv->digu_offset = off;
        }
    }
    else if (strncasecmp(msg, "AGC_MODE:", 9) == 0)
    {
        n = sscanf(msg + 9, "%d,%63s", &trx, val);

        if (n == 2 && trx == trx_sel)
        {
            char *p = strchr(val, ';');

            if (p) { *p = '\0'; }

            if (strcasecmp(val, "off") == 0) { priv->agc_mode = 0; }
            else if (strcasecmp(val, "fast") == 0) { priv->agc_mode = 1; }
            else { priv->agc_mode = 2; } /* normal */
        }
    }
    else if (strncasecmp(msg, "CW_MACROS_SPEED:", 16) == 0)
    {
        int wpm;

        if (sscanf(msg + 16, "%d", &wpm) == 1)
        {
            priv->cw_speed_wpm = wpm;
        }
    }
}

/*
 * Read frames until one whose command token matches `prefix' is received
 * (or TCI2_RECV_MAX frames scanned).  All frames update the state cache.
 * Pass prefix=NULL to consume exactly one frame.  Binary frames (audio/IQ)
 * are silently discarded by ws_recv_frame.
 */
static int tci2_recv_until(RIG *rig, const char *prefix,
                            char *reply, int replylen)
{
    /* Sized for the largest binary frame; ws_recv_frame drains binary
     * payloads internally but still needs a usable buffer for text. */
    char buf[TCI2_WS_BUFLEN];
    int retval;

    for (int i = 0; i < TCI2_RECV_MAX; i++)
    {
        retval = ws_recv_frame(rig, buf, sizeof(buf));

        if (retval < 0)
        {
            return retval;
        }

        if (buf[0] == '\0')
        {
            /* Binary frame -- ignore */
            continue;
        }

        tci2_process_message(rig, buf);

        if (prefix == NULL ||
                strncasecmp(buf, prefix, strlen(prefix)) == 0)
        {
            if (reply && replylen > 0)
            {
                strncpy(reply, buf, replylen - 1);
                reply[replylen - 1] = '\0';
            }

            return RIG_OK;
        }
    }

    return -RIG_ETIMEOUT;
}

/*
 * High-level transaction: send a command and optionally wait for a reply
 * matching `prefix'.  Retries on I/O errors with automatic reconnection.
 *
 * For queries:  pass prefix, reply buffer, and replylen.
 * For fire-and-forget sets:  pass prefix=NULL, reply=NULL, replylen=0.
 *
 * Uses RIGPORT(rig)->retry (set from rig_caps.retry) as the maximum
 * number of reconnect attempts per transaction.
 */
static int tci2_transaction(RIG *rig, const char *cmd,
                            const char *prefix,
                            char *reply, int replylen)
{
    hamlib_port_t *rp = RIGPORT(rig);
    int retval;
    int attempts = rp->retry;

    if (attempts < 1)
    {
        attempts = 1;
    }

    for (int try = 0; try < attempts; try++)
    {
        if (try > 0)
        {
            rig_debug(RIG_DEBUG_WARN,
                      "%s: retry %d/%d for cmd '%s'\n",
                      __func__, try, attempts - 1, cmd);

            retval = tci2_reconnect(rig);

            if (retval != RIG_OK)
            {
                rig_debug(RIG_DEBUG_ERR,
                          "%s: reconnect failed: %s\n",
                          __func__, rigerror(retval));
                hl_usleep(500 * 1000);
                continue;
            }
        }

        retval = tci2_send(rig, cmd);

        if (retval < 0)
        {
            rig_debug(RIG_DEBUG_WARN,
                      "%s: send failed: %s\n",
                      __func__, rigerror(retval));
            continue;
        }

        if (prefix == NULL)
        {
            return RIG_OK;
        }

        retval = tci2_recv_until(rig, prefix, reply, replylen);

        if (retval == RIG_OK)
        {
            return RIG_OK;
        }

        if (retval == -RIG_ETIMEOUT)
        {
            rig_debug(RIG_DEBUG_WARN,
                      "%s: recv timeout waiting for '%s'\n",
                      __func__, prefix);
        }
        else
        {
            rig_debug(RIG_DEBUG_WARN,
                      "%s: recv error: %s\n",
                      __func__, rigerror(retval));
        }
    }

    return retval;
}

/* -------------------------------------------------------------------------
 * Mode / filter helpers
 * ---------------------------------------------------------------------- */

/*
 * Build the runtime mode table and STATE(rig)->mode_list from the
 * MODULATIONS_LIST the server sent during init.
 *
 * The runtime table maps each Hamlib mode to the exact TCI string the server
 * uses, so set_mode sends the server's own preferred string rather than
 * our hardcoded fallback.
 */
static void tci2_build_mode_list(RIG *rig)
{
    struct tci2_priv *priv = (struct tci2_priv *)STATE(rig)->priv;
    char buf[sizeof(priv->modulations_list)];
    rmode_t modes = 0;
    char *tok, *save;

    priv->rt_mode_count = 0;

    if (priv->modulations_list[0] == '\0')
    {
        return;
    }

    strncpy(buf, priv->modulations_list, sizeof(buf) - 1);
    buf[sizeof(buf) - 1] = '\0';

    tok = strtok_r(buf, ",", &save);

    while (tok != NULL)
    {
        while (*tok == ' ') { tok++; } /* skip leading whitespace */

        rmode_t m = RIG_MODE_NONE;

        for (int i = 0; tci2_mode_map[i].tci != NULL; i++)
        {
            if (strcasecmp(tok, tci2_mode_map[i].tci) == 0)
            {
                m = tci2_mode_map[i].hamlib;
                break;
            }
        }

        if (m != RIG_MODE_NONE)
        {
            modes |= m;

            /* Store first occurrence of each Hamlib mode — server's preferred
             * string for that mode is used in set_mode. */
            if (priv->rt_mode_count < TCI2_MAX_RT_MODES)
            {
                int found = 0;

                for (int i = 0; i < priv->rt_mode_count; i++)
                {
                    if (priv->rt_modes[i].hamlib == m)
                    {
                        found = 1;
                        break;
                    }
                }

                if (!found)
                {
                    priv->rt_modes[priv->rt_mode_count].hamlib = m;
                    strncpy(priv->rt_modes[priv->rt_mode_count].tci, tok,
                            sizeof(priv->rt_modes[0].tci) - 1);
                    priv->rt_modes[priv->rt_mode_count].tci[
                        sizeof(priv->rt_modes[0].tci) - 1] = '\0';
                    priv->rt_mode_count++;
                }
            }
        }
        else
        {
            rig_debug(RIG_DEBUG_VERBOSE,
                      "%s: no Hamlib mapping for TCI mode '%s'\n",
                      __func__, tok);
        }

        tok = strtok_r(NULL, ",", &save);
    }

    if (modes != 0)
    {
        STATE(rig)->mode_list = modes;
        rig_debug(RIG_DEBUG_VERBOSE,
                  "%s: mode_list updated from MODULATIONS_LIST (%d entries)\n",
                  __func__, priv->rt_mode_count);
    }
}

/*
 * Convert a TCI mode string to Hamlib rmode_t.
 * Uses the static map (which includes known aliases).
 */
static rmode_t tci2_str_to_mode(const char *s)
{
    for (int i = 0; tci2_mode_map[i].tci != NULL; i++)
    {
        if (strcasecmp(s, tci2_mode_map[i].tci) == 0)
        {
            return tci2_mode_map[i].hamlib;
        }
    }

    return RIG_MODE_NONE;
}

/*
 * Convert a Hamlib rmode_t to a TCI mode string for SENDING to the server.
 * Uses the runtime table (built from the server's own MODULATIONS_LIST) so we
 * only ever send strings the server understands.  Falls back to the static map
 * only when the runtime table is empty (server sent no MODULATIONS_LIST).
 */
static const char *tci2_mode_to_str(RIG *rig, rmode_t mode)
{
    struct tci2_priv *priv = (struct tci2_priv *)STATE(rig)->priv;

    for (int i = 0; i < priv->rt_mode_count; i++)
    {
        if (priv->rt_modes[i].hamlib == mode)
        {
            return priv->rt_modes[i].tci;
        }
    }

    /* Runtime table is populated but this mode isn't in it — server doesn't
     * support it.  Return NULL so set_mode can reject with -RIG_EINVAL. */
    if (priv->rt_mode_count > 0)
    {
        return NULL;
    }

    /* No MODULATIONS_LIST received; fall back to static defaults. */
    for (int i = 0; tci2_mode_map[i].tci != NULL; i++)
    {
        if (tci2_mode_map[i].hamlib == mode)
        {
            return tci2_mode_map[i].tci;
        }
    }

    return NULL;
}

/* -------------------------------------------------------------------------
 * Lifecycle
 * ---------------------------------------------------------------------- */

int tci2_init(RIG *rig)
{
    struct tci2_priv *priv;

    ENTERFUNC;

    priv = calloc(1, sizeof(*priv));

    if (!priv)
    {
        RETURNFUNC(-RIG_ENOMEM);
    }

    priv->current_vfo     = RIG_VFO_A;
    priv->volume_db       = -12;
    priv->drive           = 50;
    priv->squelch_level   = -100;
    priv->signal_strength = -120.0f;
    priv->tx_power_w      = 0.0f;
    priv->tx_swr          = 1.0f;
    priv->agc_mode        = 2; /* normal */
    priv->cw_speed_wpm    = 25;
    priv->nb_threshold    = 50;
    priv->nb_duration     = 25;
    priv->trx_num         = 0;
    priv->rt_mode_count   = 0;
    strncpy(priv->mode_str, "USB", sizeof(priv->mode_str) - 1);
    strncpy(priv->txsource, "default", sizeof(priv->txsource) - 1);

    /* Derive max TX power in mW from caps->tx_range_list1.  Takes the
     * largest high_power across all ranges (all bands on a TCI radio
     * usually have the same TX ceiling anyway).  Falls back to a
     * generous default if the caps table has no TX list. */
    priv->tx_max_mw = 0;
    for (int i = 0; i < HAMLIB_FRQRANGESIZ; i++)
    {
        const freq_range_t *r = &rig->caps->tx_range_list1[i];
        if (RIG_IS_FRNG_END(*r)) { break; }
        if (r->high_power > 0 &&
            (unsigned int)r->high_power > priv->tx_max_mw)
        {
            priv->tx_max_mw = (unsigned int)r->high_power;
        }
    }
    if (priv->tx_max_mw == 0) { priv->tx_max_mw = TCI2_DEFAULT_TX_MW; }

    STATE(rig)->priv = priv;

    strncpy(RIGPORT(rig)->pathname, TCI2_DEFAULTPATH,
            sizeof(RIGPORT(rig)->pathname) - 1);

    RETURNFUNC(RIG_OK);
}

int tci2_open(RIG *rig)
{
    struct tci2_priv *priv = (struct tci2_priv *)STATE(rig)->priv;
    char buf[TCI2_WS_BUFLEN];
    int retval;

    ENTERFUNC;

    retval = ws_handshake(rig);

    if (retval != RIG_OK)
    {
        rig_debug(RIG_DEBUG_ERR, "%s: WebSocket handshake failed\n", __func__);
        RETURNFUNC(retval);
    }

    rig_debug(RIG_DEBUG_VERBOSE,
              "%s: WebSocket connected, waiting for READY\n", __func__);

    /* Drain all initialization messages until READY; */
    priv->ready = 0;

    for (int i = 0; i < TCI2_INIT_MAX_MSGS && !priv->ready; i++)
    {
        retval = ws_recv_frame(rig, buf, sizeof(buf));

        if (retval < 0)
        {
            rig_debug(RIG_DEBUG_ERR, "%s: recv error during init: %s\n",
                      __func__, rigerror(retval));
            RETURNFUNC(retval);
        }

        if (buf[0] != '\0')
        {
            tci2_process_message(rig, buf);
        }
    }

    if (!priv->ready)
    {
        rig_debug(RIG_DEBUG_ERR,
                  "%s: READY not received after %d messages\n",
                  __func__, TCI2_INIT_MAX_MSGS);
        RETURNFUNC(-RIG_ETIMEOUT);
    }

    rig_debug(RIG_DEBUG_VERBOSE,
              "%s: TCI ready — device=%s trx=%d ch=%d rx_only=%d trx_sel=%d\n",
              __func__, priv->device, priv->trx_count,
              priv->channel_count, priv->receive_only, priv->trx_num);

    /* Build runtime mode table from server's MODULATIONS_LIST */
    tci2_build_mode_list(rig);

    /* Subscribe to per-channel signal level and TX sensors at 200 ms interval */
    if (tci2_transaction(rig, "RX_SENSORS_ENABLE:true,200;",
                         NULL, NULL, 0) != RIG_OK)
    {
        rig_debug(RIG_DEBUG_WARN,
                  "%s: RX_SENSORS_ENABLE failed\n", __func__);
    }

    if (tci2_transaction(rig, "TX_SENSORS_ENABLE:true,200;",
                         NULL, NULL, 0) != RIG_OK)
    {
        rig_debug(RIG_DEBUG_WARN,
                  "%s: TX_SENSORS_ENABLE failed\n", __func__);
    }

    /* Apply configured digital mode offsets if non-default */
    if (priv->digl_offset != 0)
    {
        char cmd[TCI2_CMDLEN];
        SNPRINTF(cmd, sizeof(cmd), "DIGL_OFFSET:%d;", priv->digl_offset);
        tci2_transaction(rig, cmd, NULL, NULL, 0);
    }

    if (priv->digu_offset != 0)
    {
        char cmd[TCI2_CMDLEN];
        SNPRINTF(cmd, sizeof(cmd), "DIGU_OFFSET:%d;", priv->digu_offset);
        tci2_transaction(rig, cmd, NULL, NULL, 0);
    }

    STATE(rig)->current_vfo = RIG_VFO_A;

    RETURNFUNC(RIG_OK);
}

int tci2_close(RIG *rig)
{
    ENTERFUNC;

    /* Best-effort unsubscribe; don't retry on failure since we're closing. */
    ws_send_text(rig, "RX_SENSORS_ENABLE:false;");
    ws_send_text(rig, "TX_SENSORS_ENABLE:false;");

    /* RFC 6455 §5.5.1: send a Close frame before dropping the TCP.
     * 1000 = Normal Closure.  Best-effort; errors are non-fatal. */
    ws_send_close(rig, 1000);

    RETURNFUNC(RIG_OK);
}

int tci2_cleanup(RIG *rig)
{
    ENTERFUNC;

    free(STATE(rig)->priv);
    STATE(rig)->priv = NULL;
    RETURNFUNC(RIG_OK);
}

/* -------------------------------------------------------------------------
 * Frequency
 * ---------------------------------------------------------------------- */

int tci2_set_freq(RIG *rig, vfo_t vfo, freq_t freq)
{
    struct tci2_priv *priv = (struct tci2_priv *)STATE(rig)->priv;
    char cmd[TCI2_CMDLEN];
    int ch, retval;

    ENTERFUNC;

    if (vfo == RIG_VFO_CURR)
    {
        vfo = priv->current_vfo;
    }

    ch = (vfo == RIG_VFO_B) ? 1 : 0;

    /* TCI distinguishes DDS (panorama center / IQ-stream LO) from VFO
     * (cursor inside the panorama).  Send DDS first to move the panorama
     * center, then VFO so the cursor lands at the requested freq. */
    SNPRINTF(cmd, sizeof(cmd), "DDS:%d,%.0f;", priv->trx_num, freq);
    retval = tci2_transaction(rig, cmd, NULL, NULL, 0);

    if (retval < 0) { RETURNFUNC(retval); }

    SNPRINTF(cmd, sizeof(cmd), "VFO:%d,%d,%.0f;", priv->trx_num, ch, freq);
    retval = tci2_transaction(rig, cmd, NULL, NULL, 0);

    if (retval == RIG_OK)
    {
        priv->freq[ch] = freq;
    }

    RETURNFUNC(retval);
}

int tci2_get_freq(RIG *rig, vfo_t vfo, freq_t *freq)
{
    struct tci2_priv *priv = (struct tci2_priv *)STATE(rig)->priv;
    char cmd[TCI2_CMDLEN];
    char prefix[32];
    char reply[TCI2_BUFLEN];
    int ch, retval;
    double f;

    ENTERFUNC;

    if (vfo == RIG_VFO_CURR)
    {
        vfo = priv->current_vfo;
    }

    ch = (vfo == RIG_VFO_B) ? 1 : 0;

    SNPRINTF(cmd, sizeof(cmd), "VFO:%d,%d;", priv->trx_num, ch);
    SNPRINTF(prefix, sizeof(prefix), "VFO:%d,%d,", priv->trx_num, ch);

    retval = tci2_transaction(rig, cmd, prefix, reply, sizeof(reply));

    if (retval != RIG_OK) { RETURNFUNC(retval); }

    if (sscanf(reply, "VFO:%*d,%*d,%lf", &f) != 1)
    {
        RETURNFUNC(-RIG_EPROTO);
    }

    *freq = (freq_t)f;
    RETURNFUNC(RIG_OK);
}

/* -------------------------------------------------------------------------
 * Mode
 * ---------------------------------------------------------------------- */

int tci2_set_mode(RIG *rig, vfo_t vfo, rmode_t mode, pbwidth_t width)
{
    struct tci2_priv *priv = (struct tci2_priv *)STATE(rig)->priv;
    char cmd[TCI2_CMDLEN];
    const char *modestr;
    int retval;

    ENTERFUNC;

    modestr = tci2_mode_to_str(rig, mode);

    if (!modestr)
    {
        rig_debug(RIG_DEBUG_ERR, "%s: unsupported mode %s\n",
                  __func__, rig_strrmode(mode));
        RETURNFUNC(-RIG_EINVAL);
    }

    SNPRINTF(cmd, sizeof(cmd), "MODULATION:%d,%s;", priv->trx_num, modestr);
    retval = tci2_transaction(rig, cmd, NULL, NULL, 0);

    if (retval < 0) { RETURNFUNC(retval); }

    /* Note: RX_FILTER_BAND SET is intentionally NOT sent here.
     * ExpertSDR3 auto-applies a mode-appropriate filter on MODULATION change.
     * Sending an explicit RX_FILTER_BAND after MODULATION produces incorrect
     * filter state in the server (observed empirically; cause unclear).
     * The filter width returned by get_mode reflects the server's auto-set
     * value via the READY-dump cache in priv->filter_low/filter_high. */
    (void)width;

    RETURNFUNC(RIG_OK);
}

int tci2_get_mode(RIG *rig, vfo_t vfo, rmode_t *mode, pbwidth_t *width)
{
    struct tci2_priv *priv = (struct tci2_priv *)STATE(rig)->priv;
    char cmd[TCI2_CMDLEN];
    char prefix[64];
    char reply[TCI2_BUFLEN];
    char modestr[32];
    int retval;

    ENTERFUNC;

    SNPRINTF(cmd, sizeof(cmd), "MODULATION:%d;", priv->trx_num);
    SNPRINTF(prefix, sizeof(prefix), "MODULATION:%d,", priv->trx_num);

    retval = tci2_transaction(rig, cmd, prefix, reply, sizeof(reply));

    if (retval != RIG_OK) { RETURNFUNC(retval); }

    if (sscanf(reply, "MODULATION:%*d,%31s", modestr) != 1)
    {
        RETURNFUNC(-RIG_EPROTO);
    }

    char *p = strchr(modestr, ';');

    if (p) { *p = '\0'; }

    *mode = tci2_str_to_mode(modestr);

    if (*mode == RIG_MODE_NONE)
    {
        rig_debug(RIG_DEBUG_WARN,
                  "%s: unknown TCI mode '%s'\n", __func__, modestr);
        *mode = RIG_MODE_USB;
    }

    SNPRINTF(priv->mode_str, sizeof(priv->mode_str), "%s", modestr);

    /* RX_FILTER_BAND is push-only from the server.  A bare "RX_FILTER_BAND:N;"
     * query is not supported — ExpertSDR3 interprets it as a SET command with
     * no arguments and returns arbitrary internal values.  Use the cached
     * filter boundaries that were captured from the READY state dump (and kept
     * up to date by tci2_process_message on every subsequent push). */
    if (priv->filter_low != 0 || priv->filter_high != 0)
    {
        *width = (pbwidth_t)abs(priv->filter_high - priv->filter_low);
    }
    else
    {
        *width = rig_passband_normal(rig, *mode);
    }

    RETURNFUNC(RIG_OK);
}

/* -------------------------------------------------------------------------
 * PTT
 * ---------------------------------------------------------------------- */

int tci2_set_ptt(RIG *rig, vfo_t vfo, ptt_t ptt)
{
    struct tci2_priv *priv = (struct tci2_priv *)STATE(rig)->priv;
    char cmd[TCI2_CMDLEN];
    int retval;

    ENTERFUNC;

    if (priv->receive_only && ptt != RIG_PTT_OFF)
    {
        rig_debug(RIG_DEBUG_WARN, "%s: device is receive-only\n", __func__);
        RETURNFUNC(-RIG_ENAVAIL);
    }

    if (ptt == RIG_PTT_OFF)
    {
        SNPRINTF(cmd, sizeof(cmd), "TRX:%d,false;", priv->trx_num);
    }
    else
    {
        const char *src;

        rig_debug(RIG_DEBUG_VERBOSE, "%s: ptt=%d, txsource='%s'\n",
                  __func__, ptt, priv->txsource);

        if (ptt == RIG_PTT_ON_MIC)
        {
            src = ",Mic";
        }
        else if (ptt == RIG_PTT_ON_DATA)
        {
            src = ",Vac";
        }
        else if (strcasecmp(priv->txsource, "mic") == 0)
        {
            src = ",Mic";
        }
        else if (strcasecmp(priv->txsource, "vac") == 0)
        {
            src = ",Vac";
        }
        else
        {
            src = "";
        }

        SNPRINTF(cmd, sizeof(cmd), "TRX:%d,true%s;", priv->trx_num, src);
    }

    retval = tci2_transaction(rig, cmd, NULL, NULL, 0);

    if (retval == RIG_OK)
    {
        priv->ptt = ptt;
    }

    RETURNFUNC(retval);
}

int tci2_get_ptt(RIG *rig, vfo_t vfo, ptt_t *ptt)
{
    struct tci2_priv *priv = (struct tci2_priv *)STATE(rig)->priv;
    char cmd[TCI2_CMDLEN];
    char prefix[32];
    char reply[TCI2_BUFLEN];
    char state[16];
    int retval;

    ENTERFUNC;

    SNPRINTF(cmd, sizeof(cmd), "TRX:%d;", priv->trx_num);
    SNPRINTF(prefix, sizeof(prefix), "TRX:%d,", priv->trx_num);

    retval = tci2_transaction(rig, cmd, prefix, reply, sizeof(reply));

    if (retval != RIG_OK) { RETURNFUNC(retval); }

    if (sscanf(reply, "TRX:%*d,%15s", state) != 1)
    {
        RETURNFUNC(-RIG_EPROTO);
    }

    char *p = strchr(state, ';');

    if (p) { *p = '\0'; }

    p = strchr(state, ',');

    if (p) { *p = '\0'; } /* strip optional signal source arg */

    priv->ptt = (strcasecmp(state, "true") == 0) ? RIG_PTT_ON : RIG_PTT_OFF;
    *ptt = priv->ptt;

    RETURNFUNC(RIG_OK);
}

/* -------------------------------------------------------------------------
 * VFO / Split
 * ---------------------------------------------------------------------- */

int tci2_set_vfo(RIG *rig, vfo_t vfo)
{
    struct tci2_priv *priv = (struct tci2_priv *)STATE(rig)->priv;

    ENTERFUNC;

    if (vfo == RIG_VFO_A || vfo == RIG_VFO_B)
    {
        priv->current_vfo = vfo;
    }

    STATE(rig)->current_vfo = priv->current_vfo;
    RETURNFUNC(RIG_OK);
}

int tci2_get_vfo(RIG *rig, vfo_t *vfo)
{
    struct tci2_priv *priv = (struct tci2_priv *)STATE(rig)->priv;

    ENTERFUNC;
    *vfo = priv->current_vfo;
    RETURNFUNC(RIG_OK);
}

int tci2_set_split_vfo(RIG *rig, vfo_t vfo, split_t split,
                               vfo_t tx_vfo)
{
    struct tci2_priv *priv = (struct tci2_priv *)STATE(rig)->priv;
    char cmd[TCI2_CMDLEN];
    int retval;

    ENTERFUNC;

    SNPRINTF(cmd, sizeof(cmd), "SPLIT_ENABLE:%d,%s;",
             priv->trx_num,
             (split == RIG_SPLIT_ON) ? "true" : "false");

    retval = tci2_transaction(rig, cmd, NULL, NULL, 0);

    if (retval == RIG_OK)
    {
        priv->split = split;
    }

    RETURNFUNC(retval);
}

int tci2_get_split_vfo(RIG *rig, vfo_t vfo, split_t *split,
                               vfo_t *tx_vfo)
{
    struct tci2_priv *priv = (struct tci2_priv *)STATE(rig)->priv;
    char cmd[TCI2_CMDLEN];
    char prefix[64];
    char reply[TCI2_BUFLEN];
    char state[16];
    int retval;

    ENTERFUNC;

    SNPRINTF(cmd, sizeof(cmd), "SPLIT_ENABLE:%d;", priv->trx_num);
    SNPRINTF(prefix, sizeof(prefix), "SPLIT_ENABLE:%d,", priv->trx_num);

    retval = tci2_transaction(rig, cmd, prefix, reply, sizeof(reply));

    if (retval != RIG_OK) { RETURNFUNC(retval); }

    if (sscanf(reply, "SPLIT_ENABLE:%*d,%15s", state) != 1)
    {
        RETURNFUNC(-RIG_EPROTO);
    }

    char *p = strchr(state, ';');

    if (p) { *p = '\0'; }

    priv->split = (strcasecmp(state, "true") == 0)
                  ? RIG_SPLIT_ON : RIG_SPLIT_OFF;
    *split  = priv->split;
    *tx_vfo = (priv->split == RIG_SPLIT_ON) ? RIG_VFO_B : RIG_VFO_A;

    RETURNFUNC(RIG_OK);
}

/* -------------------------------------------------------------------------
 * Split frequency
 * ---------------------------------------------------------------------- */

int tci2_set_split_freq(RIG *rig, vfo_t vfo, freq_t tx_freq)
{
    struct tci2_priv *priv = (struct tci2_priv *)STATE(rig)->priv;
    char cmd[TCI2_CMDLEN];
    int retval;

    ENTERFUNC;

    SNPRINTF(cmd, sizeof(cmd), "VFO:%d,1,%.0f;", priv->trx_num, tx_freq);
    retval = tci2_transaction(rig, cmd, NULL, NULL, 0);

    if (retval == RIG_OK)
    {
        priv->freq[1] = tx_freq;
    }

    RETURNFUNC(retval);
}

int tci2_get_split_freq(RIG *rig, vfo_t vfo, freq_t *tx_freq)
{
    struct tci2_priv *priv = (struct tci2_priv *)STATE(rig)->priv;
    char cmd[TCI2_CMDLEN];
    char prefix[32];
    char reply[TCI2_BUFLEN];
    double f;
    int retval;

    ENTERFUNC;

    SNPRINTF(cmd, sizeof(cmd), "VFO:%d,1;", priv->trx_num);
    SNPRINTF(prefix, sizeof(prefix), "VFO:%d,1,", priv->trx_num);

    retval = tci2_transaction(rig, cmd, prefix, reply, sizeof(reply));

    if (retval != RIG_OK) { RETURNFUNC(retval); }

    if (sscanf(reply, "VFO:%*d,%*d,%lf", &f) != 1)
    {
        RETURNFUNC(-RIG_EPROTO);
    }

    *tx_freq = (freq_t)f;
    RETURNFUNC(RIG_OK);
}

/* -------------------------------------------------------------------------
 * RIT / XIT
 * ---------------------------------------------------------------------- */

int tci2_set_rit(RIG *rig, vfo_t vfo, shortfreq_t rit)
{
    struct tci2_priv *priv = (struct tci2_priv *)STATE(rig)->priv;
    char cmd[TCI2_CMDLEN];
    int retval;

    ENTERFUNC;

    if (rit == 0)
    {
        SNPRINTF(cmd, sizeof(cmd), "RIT_ENABLE:%d,false;", priv->trx_num);
        retval = tci2_transaction(rig, cmd, NULL, NULL, 0);

        if (retval == RIG_OK)
        {
            priv->rit_enabled = 0;
            priv->rit_offset  = 0;
        }

        RETURNFUNC(retval);
    }

    SNPRINTF(cmd, sizeof(cmd), "RIT_OFFSET:%d,%ld;",
             priv->trx_num, (long)rit);
    retval = tci2_transaction(rig, cmd, NULL, NULL, 0);

    if (retval < 0) { RETURNFUNC(retval); }

    SNPRINTF(cmd, sizeof(cmd), "RIT_ENABLE:%d,true;", priv->trx_num);
    retval = tci2_transaction(rig, cmd, NULL, NULL, 0);

    if (retval == RIG_OK)
    {
        priv->rit_enabled = 1;
        priv->rit_offset  = (int)rit;
    }

    RETURNFUNC(retval);
}

int tci2_get_rit(RIG *rig, vfo_t vfo, shortfreq_t *rit)
{
    struct tci2_priv *priv = (struct tci2_priv *)STATE(rig)->priv;
    char cmd[TCI2_CMDLEN];
    char prefix[64];
    char reply[TCI2_BUFLEN];
    char state[16];
    int retval;

    ENTERFUNC;

    SNPRINTF(cmd, sizeof(cmd), "RIT_ENABLE:%d;", priv->trx_num);
    SNPRINTF(prefix, sizeof(prefix), "RIT_ENABLE:%d,", priv->trx_num);

    retval = tci2_transaction(rig, cmd, prefix, reply, sizeof(reply));

    if (retval != RIG_OK) { RETURNFUNC(retval); }

    if (sscanf(reply, "RIT_ENABLE:%*d,%15s", state) != 1)
    {
        RETURNFUNC(-RIG_EPROTO);
    }

    char *p = strchr(state, ';');

    if (p) { *p = '\0'; }

    if (strcasecmp(state, "false") == 0)
    {
        priv->rit_enabled = 0;
        *rit = 0;
        RETURNFUNC(RIG_OK);
    }

    priv->rit_enabled = 1;

    SNPRINTF(cmd, sizeof(cmd), "RIT_OFFSET:%d;", priv->trx_num);
    SNPRINTF(prefix, sizeof(prefix), "RIT_OFFSET:%d,", priv->trx_num);

    retval = tci2_transaction(rig, cmd, prefix, reply, sizeof(reply));

    if (retval != RIG_OK) { RETURNFUNC(retval); }

    if (sscanf(reply, "RIT_OFFSET:%*d,%d", &priv->rit_offset) != 1)
    {
        RETURNFUNC(-RIG_EPROTO);
    }

    *rit = (shortfreq_t)priv->rit_offset;
    RETURNFUNC(RIG_OK);
}

int tci2_set_xit(RIG *rig, vfo_t vfo, shortfreq_t xit)
{
    struct tci2_priv *priv = (struct tci2_priv *)STATE(rig)->priv;
    char cmd[TCI2_CMDLEN];
    int retval;

    ENTERFUNC;

    if (xit == 0)
    {
        SNPRINTF(cmd, sizeof(cmd), "XIT_ENABLE:%d,false;", priv->trx_num);
        retval = tci2_transaction(rig, cmd, NULL, NULL, 0);

        if (retval == RIG_OK)
        {
            priv->xit_enabled = 0;
            priv->xit_offset  = 0;
        }

        RETURNFUNC(retval);
    }

    SNPRINTF(cmd, sizeof(cmd), "XIT_OFFSET:%d,%ld;",
             priv->trx_num, (long)xit);
    retval = tci2_transaction(rig, cmd, NULL, NULL, 0);

    if (retval < 0) { RETURNFUNC(retval); }

    SNPRINTF(cmd, sizeof(cmd), "XIT_ENABLE:%d,true;", priv->trx_num);
    retval = tci2_transaction(rig, cmd, NULL, NULL, 0);

    if (retval == RIG_OK)
    {
        priv->xit_enabled = 1;
        priv->xit_offset  = (int)xit;
    }

    RETURNFUNC(retval);
}

int tci2_get_xit(RIG *rig, vfo_t vfo, shortfreq_t *xit)
{
    struct tci2_priv *priv = (struct tci2_priv *)STATE(rig)->priv;
    char cmd[TCI2_CMDLEN];
    char prefix[64];
    char reply[TCI2_BUFLEN];
    char state[16];
    int retval;

    ENTERFUNC;

    SNPRINTF(cmd, sizeof(cmd), "XIT_ENABLE:%d;", priv->trx_num);
    SNPRINTF(prefix, sizeof(prefix), "XIT_ENABLE:%d,", priv->trx_num);

    retval = tci2_transaction(rig, cmd, prefix, reply, sizeof(reply));

    if (retval != RIG_OK) { RETURNFUNC(retval); }

    if (sscanf(reply, "XIT_ENABLE:%*d,%15s", state) != 1)
    {
        RETURNFUNC(-RIG_EPROTO);
    }

    char *p = strchr(state, ';');

    if (p) { *p = '\0'; }

    if (strcasecmp(state, "false") == 0)
    {
        priv->xit_enabled = 0;
        *xit = 0;
        RETURNFUNC(RIG_OK);
    }

    priv->xit_enabled = 1;

    SNPRINTF(cmd, sizeof(cmd), "XIT_OFFSET:%d;", priv->trx_num);
    SNPRINTF(prefix, sizeof(prefix), "XIT_OFFSET:%d,", priv->trx_num);

    retval = tci2_transaction(rig, cmd, prefix, reply, sizeof(reply));

    if (retval != RIG_OK) { RETURNFUNC(retval); }

    if (sscanf(reply, "XIT_OFFSET:%*d,%d", &priv->xit_offset) != 1)
    {
        RETURNFUNC(-RIG_EPROTO);
    }

    *xit = (shortfreq_t)priv->xit_offset;
    RETURNFUNC(RIG_OK);
}

/* -------------------------------------------------------------------------
 * Levels
 * ---------------------------------------------------------------------- */

int tci2_set_level(RIG *rig, vfo_t vfo, setting_t level, value_t val)
{
    struct tci2_priv *priv = (struct tci2_priv *)STATE(rig)->priv;
    char cmd[TCI2_CMDLEN];
    int retval;

    ENTERFUNC;

    switch (level)
    {
    case RIG_LEVEL_AF:
        /* val.f 0.0-1.0 → -60..0 dB */
        priv->volume_db = (int)(val.f * 60.0f) - 60;
        SNPRINTF(cmd, sizeof(cmd), "VOLUME:%d;", priv->volume_db);
        break;

    case RIG_LEVEL_RFPOWER:
        /* val.f 0.0-1.0 → 0..100 */
        priv->drive = (int)(val.f * 100.0f + 0.5f);

        if (priv->drive > 100) { priv->drive = 100; }

        SNPRINTF(cmd, sizeof(cmd), "DRIVE:%d,%d;", priv->trx_num, priv->drive);
        break;

    case RIG_LEVEL_SQL:
        /* val.f 0.0-1.0 → -140..0 dB; also toggle SQL_ENABLE */
        priv->squelch_level = (int)(val.f * 140.0f) - 140;
        {
            char en[TCI2_CMDLEN];
            SNPRINTF(en, sizeof(en), "SQL_ENABLE:%d,%s;",
                     priv->trx_num,
                     val.f > 0.0f ? "true" : "false");
            tci2_transaction(rig, en, NULL, NULL, 0);
        }
        SNPRINTF(cmd, sizeof(cmd), "SQL_LEVEL:%d,%d;",
                 priv->trx_num, priv->squelch_level);
        break;

    case RIG_LEVEL_NB:
        /* val.f 0.0-1.0 → threshold 1..100; preserve current duration */
        priv->nb_threshold = (int)(val.f * 99.0f + 1.5f);

        if (priv->nb_threshold < 1) { priv->nb_threshold = 1; }

        if (priv->nb_threshold > 100) { priv->nb_threshold = 100; }

        SNPRINTF(cmd, sizeof(cmd), "RX_NB_PARAM:%d,%d,%d;",
                 priv->trx_num, priv->nb_threshold, priv->nb_duration);
        break;

    case RIG_LEVEL_AGC:
        switch (val.i)
        {
        case RIG_AGC_OFF:
            SNPRINTF(cmd, sizeof(cmd), "AGC_MODE:%d,off;", priv->trx_num);
            priv->agc_mode = 0;
            break;

        case RIG_AGC_FAST:
        case RIG_AGC_SUPERFAST:
            SNPRINTF(cmd, sizeof(cmd), "AGC_MODE:%d,fast;", priv->trx_num);
            priv->agc_mode = 1;
            break;

        default:
            SNPRINTF(cmd, sizeof(cmd), "AGC_MODE:%d,normal;", priv->trx_num);
            priv->agc_mode = 2;
            break;
        }

        break;

    case RIG_LEVEL_KEYSPD:
        /* CW_KEYER_SPEED controls the hardware iambic keyer (client → server
         * only; no readback).  Update the cache so get_level KEYSPD returns
         * the value we just set for the duration of this session. */
        priv->cw_speed_wpm = val.i;

        if (priv->cw_speed_wpm < 1)  { priv->cw_speed_wpm = 1;  }

        if (priv->cw_speed_wpm > 60) { priv->cw_speed_wpm = 60; }

        SNPRINTF(cmd, sizeof(cmd), "CW_KEYER_SPEED:%d;", priv->cw_speed_wpm);
        break;

    default:
        RETURNFUNC(-RIG_EINVAL);
    }

    retval = tci2_transaction(rig, cmd, NULL, NULL, 0);
    RETURNFUNC(retval);
}

int tci2_get_level(RIG *rig, vfo_t vfo, setting_t level, value_t *val)
{
    struct tci2_priv *priv = (struct tci2_priv *)STATE(rig)->priv;
    char cmd[TCI2_CMDLEN];
    char prefix[64];
    char reply[TCI2_BUFLEN];
    int retval;

    ENTERFUNC;

    switch (level)
    {
    case RIG_LEVEL_AF:
        /* VOLUME has no TRX argument */
        retval = tci2_transaction(rig, "VOLUME;", "VOLUME:",
                                  reply, sizeof(reply));

        if (retval != RIG_OK) { RETURNFUNC(retval); }

        if (sscanf(reply, "VOLUME:%d", &priv->volume_db) != 1)
        {
            RETURNFUNC(-RIG_EPROTO);
        }

        val->f = (float)(priv->volume_db + 60) / 60.0f;
        break;

    case RIG_LEVEL_RFPOWER:
        SNPRINTF(cmd, sizeof(cmd), "DRIVE:%d;", priv->trx_num);
        SNPRINTF(prefix, sizeof(prefix), "DRIVE:%d,", priv->trx_num);

        retval = tci2_transaction(rig, cmd, prefix, reply, sizeof(reply));

        if (retval != RIG_OK) { RETURNFUNC(retval); }

        if (sscanf(reply, "DRIVE:%*d,%d", &priv->drive) != 1)
        {
            RETURNFUNC(-RIG_EPROTO);
        }

        val->f = (float)priv->drive / 100.0f;
        break;

    case RIG_LEVEL_SQL:
        SNPRINTF(cmd, sizeof(cmd), "SQL_LEVEL:%d;", priv->trx_num);
        SNPRINTF(prefix, sizeof(prefix), "SQL_LEVEL:%d,", priv->trx_num);

        retval = tci2_transaction(rig, cmd, prefix, reply, sizeof(reply));

        if (retval != RIG_OK) { RETURNFUNC(retval); }

        if (sscanf(reply, "SQL_LEVEL:%*d,%d", &priv->squelch_level) != 1)
        {
            RETURNFUNC(-RIG_EPROTO);
        }

        val->f = (float)(priv->squelch_level + 140) / 140.0f;
        break;

    case RIG_LEVEL_NB:
        SNPRINTF(cmd, sizeof(cmd), "RX_NB_PARAM:%d;", priv->trx_num);
        SNPRINTF(prefix, sizeof(prefix), "RX_NB_PARAM:%d,", priv->trx_num);

        retval = tci2_transaction(rig, cmd, prefix, reply, sizeof(reply));

        if (retval != RIG_OK) { RETURNFUNC(retval); }

        {
            int thresh, dur;

            if (sscanf(reply, "RX_NB_PARAM:%*d,%d,%d", &thresh, &dur) != 2)
            {
                RETURNFUNC(-RIG_EPROTO);
            }

            priv->nb_threshold = thresh;
            priv->nb_duration  = dur;
            val->f = (float)(thresh - 1) / 99.0f;
        }

        break;

    case RIG_LEVEL_STRENGTH:
        /* Signal strength is pushed asynchronously; return from cache.
         * S9 = -73 dBm; value is dB relative to S9. */
        val->i = (int)(priv->signal_strength + 73.0f);
        break;

    case RIG_LEVEL_AGC:
        SNPRINTF(cmd, sizeof(cmd), "AGC_MODE:%d;", priv->trx_num);
        SNPRINTF(prefix, sizeof(prefix), "AGC_MODE:%d,", priv->trx_num);

        retval = tci2_transaction(rig, cmd, prefix, reply, sizeof(reply));

        if (retval != RIG_OK) { RETURNFUNC(retval); }

        {
            char agcstr[16];

            if (sscanf(reply, "AGC_MODE:%*d,%15s", agcstr) != 1)
            {
                RETURNFUNC(-RIG_EPROTO);
            }

            char *p = strchr(agcstr, ';');

            if (p) { *p = '\0'; }

            if (strcasecmp(agcstr, "off") == 0)
            {
                val->i = RIG_AGC_OFF;
                priv->agc_mode = 0;
            }
            else if (strcasecmp(agcstr, "fast") == 0)
            {
                val->i = RIG_AGC_FAST;
                priv->agc_mode = 1;
            }
            else
            {
                val->i = RIG_AGC_SLOW;
                priv->agc_mode = 2;
            }
        }

        break;

    case RIG_LEVEL_KEYSPD:
        /* CW_MACROS_SPEED is push-only: the server sends it during the READY
         * state dump but does not respond to bare queries.  Return the value
         * captured during tci2_open. */
        val->i = priv->cw_speed_wpm;
        break;

    case RIG_LEVEL_SWR:
        /* SWR is pushed asynchronously by TX_SENSORS; return from cache.
         * 1.0 = perfect match; only meaningful while transmitting. */
        val->f = priv->tx_swr;
        break;

    case RIG_LEVEL_RFPOWER_METER:
        /* TX power is pushed asynchronously; return normalised 0..1.
         * tx_power_w is in watts; tx_max_mw is per-rig in milliwatts. */
        val->f = priv->tx_power_w / ((float)priv->tx_max_mw / 1000.0f);

        if (val->f > 1.0f) { val->f = 1.0f; }

        break;

    default:
        RETURNFUNC(-RIG_EINVAL);
    }

    RETURNFUNC(RIG_OK);
}

/* -------------------------------------------------------------------------
 * Functions (NB, NR, ANF, VFO_LOCK)
 * ---------------------------------------------------------------------- */

int tci2_set_func(RIG *rig, vfo_t vfo, setting_t func, int status)
{
    struct tci2_priv *priv = (struct tci2_priv *)STATE(rig)->priv;
    char cmd[TCI2_CMDLEN];
    const char *bstr = status ? "true" : "false";
    int retval;

    ENTERFUNC;

    switch (func)
    {
    case RIG_FUNC_NB:
        SNPRINTF(cmd, sizeof(cmd), "RX_NB_ENABLE:%d,%s;", priv->trx_num, bstr);
        priv->nb_enabled = status;
        break;

    case RIG_FUNC_NR:
        SNPRINTF(cmd, sizeof(cmd), "RX_NR_ENABLE:%d,%s;", priv->trx_num, bstr);
        priv->nr_enabled = status;
        break;

    case RIG_FUNC_ANF:
        SNPRINTF(cmd, sizeof(cmd), "RX_ANF_ENABLE:%d,%s;",
                 priv->trx_num, bstr);
        priv->anf_enabled = status;
        break;

    case RIG_FUNC_LOCK:
        /* VFO_LOCK is a server-to-client notification only in TCI 2.0;
         * there is no client SET command for VFO lock. */
        RETURNFUNC(-RIG_ENAVAIL);

    case RIG_FUNC_MUTE:
        SNPRINTF(cmd, sizeof(cmd), "MUTE:%s;", bstr);
        priv->mute_enabled = status;
        break;

    case RIG_FUNC_TUNER:
        SNPRINTF(cmd, sizeof(cmd), "TUNE:%d,%s;", priv->trx_num, bstr);
        priv->tune_enabled = status;
        break;

    default:
        RETURNFUNC(-RIG_EINVAL);
    }

    retval = tci2_transaction(rig, cmd, NULL, NULL, 0);
    RETURNFUNC(retval);
}

int tci2_get_func(RIG *rig, vfo_t vfo, setting_t func, int *status)
{
    struct tci2_priv *priv = (struct tci2_priv *)STATE(rig)->priv;
    char cmd[TCI2_CMDLEN];
    char prefix[64];
    char reply[TCI2_BUFLEN];
    char state[16];
    int retval;

    ENTERFUNC;

    switch (func)
    {
    case RIG_FUNC_NB:
        SNPRINTF(cmd, sizeof(cmd), "RX_NB_ENABLE:%d;", priv->trx_num);
        SNPRINTF(prefix, sizeof(prefix), "RX_NB_ENABLE:%d,", priv->trx_num);
        break;

    case RIG_FUNC_NR:
        SNPRINTF(cmd, sizeof(cmd), "RX_NR_ENABLE:%d;", priv->trx_num);
        SNPRINTF(prefix, sizeof(prefix), "RX_NR_ENABLE:%d,", priv->trx_num);
        break;

    case RIG_FUNC_ANF:
        SNPRINTF(cmd, sizeof(cmd), "RX_ANF_ENABLE:%d;", priv->trx_num);
        SNPRINTF(prefix, sizeof(prefix), "RX_ANF_ENABLE:%d,", priv->trx_num);
        break;

    case RIG_FUNC_LOCK:
        /* Server notifies lock state as VFO_LOCK:<trx>,<ch>,<bool>; not as a
         * LOCK: response.  Return the cached value from the READY state dump
         * (updated by tci2_process_message for both LOCK and VFO_LOCK). */
        *status = priv->lock_enabled;
        RETURNFUNC(RIG_OK);

    case RIG_FUNC_MUTE:
        SNPRINTF(cmd, sizeof(cmd), "MUTE;");
        SNPRINTF(prefix, sizeof(prefix), "MUTE:");
        break;

    case RIG_FUNC_TUNER:
        SNPRINTF(cmd, sizeof(cmd), "TUNE:%d;", priv->trx_num);
        SNPRINTF(prefix, sizeof(prefix), "TUNE:%d,", priv->trx_num);
        break;

    default:
        RETURNFUNC(-RIG_EINVAL);
    }

    retval = tci2_transaction(rig, cmd, prefix, reply, sizeof(reply));

    if (retval != RIG_OK) { RETURNFUNC(retval); }

    /* Boolean: last comma-separated field (LOCK/TUNE/NB/NR/ANF),
     * or directly after ':' for commands without a TRX arg (MUTE). */
    char *p = strrchr(reply, ',');

    if (!p)
    {
        p = strchr(reply, ':');
    }

    if (!p) { RETURNFUNC(-RIG_EPROTO); }

    strncpy(state, p + 1, sizeof(state) - 1);
    state[sizeof(state) - 1] = '\0';
    p = strchr(state, ';');

    if (p) { *p = '\0'; }

    *status = (strcasecmp(state, "true") == 0) ? 1 : 0;

    switch (func)
    {
    case RIG_FUNC_NB:     priv->nb_enabled   = *status; break;

    case RIG_FUNC_NR:     priv->nr_enabled   = *status; break;

    case RIG_FUNC_ANF:    priv->anf_enabled  = *status; break;

    case RIG_FUNC_LOCK:   priv->lock_enabled = *status; break;

    case RIG_FUNC_MUTE:   priv->mute_enabled = *status; break;

    case RIG_FUNC_TUNER:  priv->tune_enabled = *status; break;

    default: break;
    }

    RETURNFUNC(RIG_OK);
}

/* -------------------------------------------------------------------------
 * CW keying
 * ---------------------------------------------------------------------- */

int tci2_send_morse(RIG *rig, vfo_t vfo, const char *msg)
{
    struct tci2_priv *priv = (struct tci2_priv *)STATE(rig)->priv;
    char cmd[TCI2_CMDLEN];
    char escaped[TCI2_CMDLEN];
    int j = 0;

    ENTERFUNC;

    /* Escape TCI reserved chars: ':' → '^', ',' → '~', ';' → '*' */
    for (int i = 0; msg[i] && j < (int)sizeof(escaped) - 2; i++)
    {
        switch (msg[i])
        {
        case ':': escaped[j++] = '^'; break;

        case ',': escaped[j++] = '~'; break;

        case ';': escaped[j++] = '*'; break;

        default:  escaped[j++] = msg[i]; break;
        }
    }

    escaped[j] = '\0';

    SNPRINTF(cmd, sizeof(cmd), "CW_MACROS:%d,%s;", priv->trx_num, escaped);
    RETURNFUNC(tci2_transaction(rig, cmd, NULL, NULL, 0));
}

int tci2_stop_morse(RIG *rig, vfo_t vfo)
{
    ENTERFUNC;
    RETURNFUNC(tci2_transaction(rig, "CW_MACROS_STOP;", NULL, NULL, 0));
}

/*
 * Block until the server deasserts TX after a CW macro transmission, or
 * until the timeout expires.
 *
 * The TCI server pushes TRX:trx,true; when TX starts and TRX:trx,false; when
 * it ends.  Both update priv->ptt via tci2_process_message().
 *
 * Phase 1 (up to 2 s): wait for TX to be asserted.  If it never engages the
 * CW was either trivially short or silently rejected — return OK either way.
 * Phase 2 (up to 300 s): wait for TX to be released.
 */
int tci2_wait_morse(RIG *rig, vfo_t vfo)
{
    struct tci2_priv *priv = (struct tci2_priv *)STATE(rig)->priv;
    char buf[TCI2_WS_BUFLEN];
    int retval;
    time_t start = time(NULL);
    int tx_seen = 0;

    ENTERFUNC;

    /* Phase 1: wait up to 2 s for TX to engage */
    while (!tx_seen && time(NULL) - start < 2)
    {
        retval = ws_recv_frame(rig, buf, sizeof(buf));
        if (retval < 0) { RETURNFUNC(retval); }
        if (buf[0] != '\0') { tci2_process_message(rig, buf); }

        if (priv->ptt != RIG_PTT_OFF) { tx_seen = 1; }
    }

    if (!tx_seen)
    {
        /* TX never asserted — macro finished before we started polling, or
         * was rejected.  Either way there is nothing left to wait for. */
        RETURNFUNC(RIG_OK);
    }

    /* Phase 2: wait up to 300 s for TX to end */
    while (time(NULL) - start < 300)
    {
        if (priv->ptt == RIG_PTT_OFF) { RETURNFUNC(RIG_OK); }

        retval = ws_recv_frame(rig, buf, sizeof(buf));
        if (retval < 0) { RETURNFUNC(retval); }
        if (buf[0] != '\0') { tci2_process_message(rig, buf); }
    }

    RETURNFUNC(-RIG_ETIMEOUT);
}

/* -------------------------------------------------------------------------
 * Power conversion.  Uses per-rig priv->tx_max_mw (derived from caps at
 * init time) so 1.0 == this rig's rated TX ceiling.  The SunSDR2 PRO is
 * 15 W; a hypothetical 100 W TCI rig would set high_power = W(100) in
 * its caps and everything scales automatically.
 * ---------------------------------------------------------------------- */

int tci2_power2mW(RIG *rig, unsigned int *mwpower,
                          float power, freq_t freq, rmode_t mode)
{
    struct tci2_priv *priv = (struct tci2_priv *)STATE(rig)->priv;
    ENTERFUNC;
    *mwpower = (unsigned int)(power * (float)priv->tx_max_mw);
    RETURNFUNC(RIG_OK);
}

int tci2_mW2power(RIG *rig, float *power,
                          unsigned int mwpower, freq_t freq, rmode_t mode)
{
    struct tci2_priv *priv = (struct tci2_priv *)STATE(rig)->priv;
    ENTERFUNC;
    *power = (float)mwpower / (float)priv->tx_max_mw;

    if (*power > 1.0f) { *power = 1.0f; }

    RETURNFUNC(RIG_OK);
}

/* -------------------------------------------------------------------------
 * Info
 * ---------------------------------------------------------------------- */

const char *tci2_get_info(RIG *rig)
{
    struct tci2_priv *priv = (struct tci2_priv *)STATE(rig)->priv;
    static char info[128];
    SNPRINTF(info, sizeof(info), "TCI 2.0 — %s (trx %d)",
             priv->device, priv->trx_num);
    return info;
}

/* -------------------------------------------------------------------------
 * Capabilities
 *
 * Per-radio rig_caps definitions live in sibling files (e.g. sunsdr2-pro.c),
 * which wire their .rig_init/.set_freq/etc. callbacks to the tci2_* functions
 * exported above via tci2.h.
 * ---------------------------------------------------------------------- */
