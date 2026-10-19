// ---------------------------------------------------------------------------
//   AnyTone AT-778UV / Retevis RT95 mic-bus backend
// ---------------------------------------------------------------------------
//
//  at778uv.c
//
//  Created by muurk - https://github.com/muurk
//  Copyright (C) 2026, muurk.
//  Written with Claude (Anthropic); see the Co-Authored-By commit trailer.
//
//   This library is free software; you can redistribute it and/or
//   modify it under the terms of the GNU Lesser General Public
//   License as published by the Free Software Foundation; either
//   version 2.1 of the License, or (at your option) any later version.
//
//   This library is distributed in the hope that it will be useful,
//   but WITHOUT ANY WARRANTY; without even the implied warranty of
//   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
//   Lesser General Public License for more details.
//
//   You should have received a copy of the GNU Lesser General Public
//   License along with this library; if not, write to the Free Software
//   Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA
//
// ---------------------------------------------------------------------------
//
// The radio is controlled over pin 8 of its RJ45 microphone socket: a single
// wire, half duplex, 9600 8N1, shared by the hand microphone's keypad and by
// the programming cable. Every byte the host sends is echoed back by the bus
// before any reply arrives, and both ends can talk at once, so replies are
// occasionally damaged and must be validated rather than merely counted.
//
// Three commands do all the work:
//
//   'D'  0x44   44 <sub> <d0> <d1> <d2> <d3> 06   -> 06
//               Ten sub-commands that write the live record or the live
//               settings and push the change to the RF processor. Rigidly
//               seven bytes: the trailing 06 is positional, not a delimiter.
//
//   'R'  0x52   52 <addr_hi> <addr_lo> 10         -> 57 <addr> 10 <16> <sum> 06
//               Codeplug block read. The length MUST be 0x10 - a longer read
//               returns a correctly checksummed frame whose tail is garbage
//               read back out of the shared buffer.
//
//   'A'  0x41   41 00 01 00 <key> 00 00 06        -> (no reply)
//               Inject a microphone keypress. Key 0x52 'R' posts no event but
//               makes the radio emit a status frame, which is how state is
//               polled; key 0x2F '/' toggles which VFO is selected.
//
// Reading from this bus is not a matter of counting bytes. The radio sends a
// status frame of its own accord every time the squelch opens, so unrelated
// frames land in the middle of replies; and because both ends share one wire,
// collisions damage the head of whatever is in flight - about 2.6% of replies
// on the bench, and a boot frame has been seen arriving with its first byte
// missing entirely. Every reply here is therefore found by resynchronising on
// a known lead byte, and every exchange that fails is retried - except a
// keypress, which must not be, because the radio's keys are toggles.
//
// Two hazards shape this backend.
//
// The dispatcher matches on the first byte, so stray ASCII is dangerous: any
// frame beginning 'E' reboots the radio, 'R' swallows the next three bytes as
// an address, and 'F' puts the radio into a factory test mode that only
// removing DC power will clear. Nothing here emits a byte the radio has not
// been asked for.
//
// More importantly, 'D' writes whichever record the radio currently has
// selected - and in memory mode that is the stored *memory channel*, not the
// VFO. It acks, the display does not change, and the channel is overwritten
// permanently. So every record write here is gated on reading back the mode
// first, and refuses with -RIG_ENTARGET rather than destroying a memory.
//
// ---------------------------------------------------------------------------

#include <stdlib.h>
#include <string.h>

#include "hamlib/config.h"
#include "hamlib/rig.h"
#include "hamlib/port.h"
#include "hamlib/rig_state.h"

#include "iofunc.h"
#include "misc.h"
#include "register.h"
#include "riglist.h"
#include "tones.h"
#include "idx_builtin.h"

#include "anytone.h"

// ---------------------------------------------------------------------------
//    WIRE PROTOCOL
// ---------------------------------------------------------------------------

#define AT778UV_CMD_IDENT       0x02
#define AT778UV_CMD_READ        0x52    /* 'R' */
#define AT778UV_CMD_SET         0x44    /* 'D' */
#define AT778UV_CMD_KEY         0x41    /* 'A' */
#define AT778UV_ACK             0x06

/* 'D' sub-commands. Every one of these was keyed on hardware. */
#define AT778UV_SET_FREQ        0
#define AT778UV_SET_OFFSET      1
#define AT778UV_SET_WIDTH       2
#define AT778UV_SET_RXTONE      3
#define AT778UV_SET_TXTONE      4
#define AT778UV_SET_TONESQL     5       /* not exposed: no Hamlib equivalent */
#define AT778UV_SET_SQUELCH     6
#define AT778UV_SET_VOLUME      7
#define AT778UV_SET_POWER       8
#define AT778UV_SET_VOX         9       /* not exposed: -P variants only, untested */

/* Tone modes shared by sub-commands 3 and 4. */
#define AT778UV_TONE_OFF        0
#define AT778UV_TONE_CTCSS      1
#define AT778UV_TONE_DCS        2

/* Microphone key codes. */
#define AT778UV_KEY_STATUS      0x52    /* 'R' - posts no event, draws a status frame */
#define AT778UV_KEY_VFO_TOGGLE  0x2F    /* '/' - toggles the selected VFO */

/* Codeplug addresses. Block reads are always 16 bytes. */
#define AT778UV_ADDR_VFOA       0x1900
#define AT778UV_ADDR_VFOB       0x1920
#define AT778UV_ADDR_SETTINGS   0x3200
#define AT778UV_ADDR_LIVE       0x3260
#define AT778UV_BLOCK_SIZE      0x10
#define AT778UV_RECORD_SIZE     32

/* Offsets within the 16-byte block at AT778UV_ADDR_SETTINGS. */
#define AT778UV_SET_OFF_SQUELCH 0x04
#define AT778UV_SET_OFF_VOLUME  0x06
#define AT778UV_SET_OFF_DUALW   0x0C    /* bit 0: dual watch */

/* Offsets within the 16-byte block at AT778UV_ADDR_LIVE. */
#define AT778UV_LIVE_OFF_CHANA  0x00    /* memory channel, ZERO based */
#define AT778UV_LIVE_OFF_MODEA  0x01    /* bit 0: 1 = VFO, 0 = memory */
#define AT778UV_LIVE_OFF_CHANB  0x07
#define AT778UV_LIVE_OFF_MODEB  0x08

/* Offsets within a 32-byte channel or VFO record. */
#define AT778UV_REC_FREQ        0
#define AT778UV_REC_OFFSET      4
#define AT778UV_REC_POWER       9       /* bits 3-2 txpower, bits 1-0 duplex */
#define AT778UV_REC_WIDTH       10      /* bits 3-2 channel width */
#define AT778UV_REC_TONEMODE    11      /* bits 3-2 RX enables, bits 1-0 TX */
#define AT778UV_REC_RXTONE      12
#define AT778UV_REC_TXTONE      13
#define AT778UV_REC_RXDCS       14      /* low byte; 15 bit0 is the ninth */
#define AT778UV_REC_RXDCS_HI    15      /* bit 1 invert, bit 0 code bit 8 */
#define AT778UV_REC_TXDCS       16      /* in the second block, at offset 0 */
#define AT778UV_REC_TXDCS_HI    17

/* Identify reply: 'I', 7 model bytes, a band byte, 6 version bytes, ACK. */
#define AT778UV_ID_LEN          16
#define AT778UV_ID_MODEL        1
#define AT778UV_ID_MODEL_LEN    7
#define AT778UV_ID_VERSION      9
#define AT778UV_ID_VERSION_LEN  6

/* Frame sizes. */
#define AT778UV_SET_FRAME_LEN   7
#define AT778UV_KEY_FRAME_LEN   8
#define AT778UV_READ_FRAME_LEN  4
#define AT778UV_READ_REPLY_LEN  22
#define AT778UV_STATUS_LEN      14
#define AT778UV_MAX_FRAME       64

/* Retry policy. The bus loses roughly one reply in forty to collisions, so a
   single attempt is not enough for a backend that will be polled. */
#define AT778UV_RETRIES         3
#define AT778UV_RETRY_DELAY_US  50000
#define AT778UV_MAX_DISCARD     64

/* Status frame layout. */
#define AT778UV_ST_COS_A        2
#define AT778UV_ST_COS_B        3
#define AT778UV_ST_SLOT         8       /* 0 = A, 1 = B */

/* Ranges the radio's own UI enforces but the wire does not. */
#define AT778UV_VOLUME_MAX      36
#define AT778UV_SQUELCH_MAX     9
#define AT778UV_POWER_MAX       2       /* 0 Low, 1 Medium, 2 High */
#define AT778UV_CHANNELS        200     /* memory channels, numbered 1..200 */

#define AT778UV_VFOS            (RIG_VFO_A | RIG_VFO_B)
#define AT778UV_MODES           (RIG_MODE_FM | RIG_MODE_FMN)
#define AT778UV_LEVELS          (RIG_LEVEL_AF | RIG_LEVEL_SQL | RIG_LEVEL_RFPOWER)
#define AT778UV_FUNCS           (RIG_FUNC_MUTE | RIG_FUNC_TONE | RIG_FUNC_TSQL)

// ---------------------------------------------------------------------------
//    CTCSS
//
// The radio's tone codes are NOT an index into Hamlib's standard list. They
// run from 0x00 = 62.5 Hz - a tone that is not in the standard list at all -
// to 0x32 = 254.1 Hz, so the wire value is this table's index directly.
// 0x33 selects a custom tone held in the record, which is not exposed here.
// ---------------------------------------------------------------------------

static tone_t at778uv_ctcss_list[] =
{
    625,  670,  693,  719,  744,  770,  797,  825,  854,  885,
    915,  948,  974,  1000, 1035, 1072, 1109, 1148, 1188, 1230,
    1273, 1318, 1365, 1413, 1462, 1514, 1567, 1598, 1622, 1655,
    1679, 1713, 1738, 1773, 1799, 1835, 1862, 1899, 1928, 1966,
    1995, 2035, 2065, 2107, 2181, 2257, 2291, 2336, 2418, 2503,
    2541, 0
};

// ---------------------------------------------------------------------------
//    PRIVATE DATA
// ---------------------------------------------------------------------------

struct at778uv_priv_data
{
    vfo_t curr_vfo;         /* last slot reported by a status frame */
    char  model[16];        /* as reported by the radio, e.g. "AT778UV" */
    char  version[16];
};

// ---------------------------------------------------------------------------
//    BCD
//
// Frequencies and offsets are four packed BCD bytes, most significant first,
// in units of 10 Hz - the codeplug's own encoding. 446.045 MHz is 44 60 45 00.
// ---------------------------------------------------------------------------

static void at778uv_to_bcd(freq_t freq, unsigned char *bcd)
{
    unsigned long v = (unsigned long)(freq / 10);
    int i;

    for (i = 3; i >= 0; i--)
    {
        bcd[i] = (unsigned char)((((v / 10) % 10) << 4) | (v % 10));
        v /= 100;
    }
}

//
// Returns a negative Hamlib error rather than a frequency if the bytes are
// not valid BCD. That is not paranoia: an unprogrammed memory channel reads
// as all 0xFF, which would otherwise decode to a confident but meaningless
// frequency, and the block checksum cannot catch it because 0xFF is
// legitimate stored data.
//
static int at778uv_from_bcd(const unsigned char *bcd, freq_t *freq)
{
    unsigned long v = 0;
    int i;

    for (i = 0; i < 4; i++)
    {
        int hi = (bcd[i] >> 4) & 0x0f;
        int lo = bcd[i] & 0x0f;

        if (hi > 9 || lo > 9)
        {
            return -RIG_EPROTO;
        }

        v = (v * 100) + (hi * 10) + lo;
    }

    *freq = (freq_t)v * 10;

    return RIG_OK;
}

//
// Map a Hamlib 0.0-1.0 level onto an integer range.
//
// The clamp is applied to the float BEFORE the cast, not after: converting an
// out-of-range double to int is undefined behaviour, so clamping the result
// would already be too late.
//
static int at778uv_scale(float val, int lo, int hi)
{
    float scaled;

    if (!(val > 0.0f))      /* also catches NaN */
    {
        return lo;
    }

    if (val > 1.0f)
    {
        return hi;
    }

    scaled = (val * hi) + 0.5f;

    if (scaled < (float)lo)
    {
        return lo;
    }

    return (int)scaled;
}

// ---------------------------------------------------------------------------
//    DCS
//
// A DCS code is stored as its three digits read as octal: 023 becomes
// 0*64 + 2*8 + 3 = 0x13. The value needs nine bits, so the low eight are one
// record byte and the ninth is bit 0 of the next.
// ---------------------------------------------------------------------------

static int at778uv_dcs_to_wire(tone_t code)
{
    return (int)(((code / 100) % 10) * 64 + ((code / 10) % 10) * 8 + (code % 10));
}

static tone_t at778uv_dcs_from_wire(int wire)
{
    return (tone_t)(((wire >> 6) & 0x07) * 100 + ((wire >> 3) & 0x07) * 10
                    + (wire & 0x07));
}

// ---------------------------------------------------------------------------
//    TRANSPORT
// ---------------------------------------------------------------------------

//
// Framing.
//
// Two facts make a naive read unsafe. The radio volunteers a status frame
// every time the squelch opens, so unrelated 14-byte frames arrive in the
// middle of a reply we asked for; and because both ends share one wire,
// collisions damage the HEAD of whatever is in flight - measured at about
// 2.6% of replies on the bench, and a boot frame has been seen arriving with
// its leading byte missing altogether.
//
// So nothing here assumes the next byte is the byte it wants. Replies are
// found by resynchronising on a known lead byte, status frames that turn up
// uninvited are consumed and logged rather than treated as corruption, and
// anything that parses as neither is discarded a byte at a time.
//
static int at778uv_await(RIG *rig, unsigned char lead, unsigned char *buf,
                         int len)
{
    hamlib_port_t *rp = RIGPORT(rig);
    unsigned char pushback = 0;
    int have_pushback = 0;
    int discarded = 0;
    int retval;

    while (discarded < AT778UV_MAX_DISCARD)
    {
        unsigned char c;

        if (have_pushback)
        {
            c = pushback;
            have_pushback = 0;
        }
        else
        {
            retval = read_block(rp, &c, 1);

            if (retval < 0)
            {
                return retval;
            }
        }

        if (c == 'S')
        {
            unsigned char st[AT778UV_STATUS_LEN];

            /* Confirm the second header byte before committing to reading a
               whole frame, so a stray 'S' in a data field cannot swallow the
               bytes that follow it. */
            st[0] = c;
            retval = read_block(rp, &st[1], 1);

            if (retval < 0)
            {
                return retval;
            }

            if (st[1] != 'T')
            {
                pushback = st[1];
                have_pushback = 1;
                discarded += 2;
                continue;
            }

            retval = read_block(rp, &st[2], AT778UV_STATUS_LEN - 2);

            if (retval < 0)
            {
                return retval;
            }

            if (st[AT778UV_STATUS_LEN - 1] != AT778UV_ACK)
            {
                rig_debug(RIG_DEBUG_WARN, "%s: damaged status frame, skipped\n",
                          __func__);
                discarded += AT778UV_STATUS_LEN;
                continue;
            }

            if (lead == 'S')
            {
                if (len < AT778UV_STATUS_LEN)
                {
                    return -RIG_EINTERNAL;
                }

                memcpy(buf, st, AT778UV_STATUS_LEN);
                return RIG_OK;
            }

            /* Somebody keyed up mid-transaction. Not our frame; carry on. */
            rig_debug(RIG_DEBUG_VERBOSE, "%s: unsolicited status frame\n", __func__);
            continue;
        }

        if (c == lead)
        {
            buf[0] = c;

            if (len > 1)
            {
                retval = read_block(rp, &buf[1], len - 1);

                if (retval < 0)
                {
                    return retval;
                }
            }

            return RIG_OK;
        }

        discarded++;
    }

    rig_debug(RIG_DEBUG_ERR, "%s: no 0x%02x frame after %d stray bytes\n",
              __func__, lead, discarded);

    return -RIG_EPROTO;
}

//
// Send a frame and collect its reply, retrying the whole exchange if the
// reply does not arrive intact.
//
// `tries` is the caller's, not a constant, because retrying is not always
// safe. Reads and parameter writes are idempotent - setting the same
// frequency twice is indistinguishable from setting it once - so those retry
// freely. A KEYPRESS MUST NOT: the radio's keys are toggles, and sending one
// twice returns it to where it started, which is precisely the bug that made
// the original key-map survey unreliable.
//
static int at778uv_transaction(RIG *rig, const unsigned char *cmd, int cmd_len,
                               unsigned char lead, unsigned char *reply,
                               int reply_len, int tries)
{
    hamlib_port_t *rp = RIGPORT(rig);
    unsigned char echo[AT778UV_MAX_FRAME];
    int retval = -RIG_EPROTO;
    int attempt;

    if (cmd_len > AT778UV_MAX_FRAME)
    {
        return -RIG_EINTERNAL;
    }

    for (attempt = 0; attempt < tries; attempt++)
    {
        if (attempt > 0)
        {
            rig_debug(RIG_DEBUG_WARN, "%s: retry %d of %d\n", __func__, attempt,
                      tries - 1);
            hl_usleep(AT778UV_RETRY_DELAY_US);
        }

        rig_flush(rp);

        retval = write_block(rp, cmd, cmd_len);

        if (retval != RIG_OK)
        {
            continue;
        }

        /* Everything we send is echoed before any reply. Read the echo back,
           but do not insist it is perfect: a collision can damage the echo
           while the command still reached the radio intact, and the reply
           reader resynchronises regardless. Failing here would throw away
           exchanges that actually worked. */
        retval = read_block(rp, echo, cmd_len);

        if (retval < 0)
        {
            continue;
        }

        if (memcmp(echo, cmd, cmd_len) != 0)
        {
            rig_debug(RIG_DEBUG_WARN, "%s: echo damaged - bus collision\n",
                      __func__);
        }

        if (reply_len == 0)
        {
            return RIG_OK;
        }

        retval = at778uv_await(rig, lead, reply, reply_len);

        if (retval == RIG_OK)
        {
            return retval;
        }
    }

    return retval;
}

//
// Read one 16-byte codeplug block. The length byte must be 0x10: the radio
// answers a longer request with a well-formed, correctly checksummed frame
// whose tail is stale buffer content, so a caller that trusted it would get
// plausible rubbish.
//
static int at778uv_read_block(RIG *rig, unsigned short addr,
                              unsigned char *data)
{
    unsigned char cmd[AT778UV_READ_FRAME_LEN];
    unsigned char reply[AT778UV_READ_REPLY_LEN];
    unsigned char sum = 0;
    int retval;
    int i;

    cmd[0] = AT778UV_CMD_READ;
    cmd[1] = (unsigned char)(addr >> 8);
    cmd[2] = (unsigned char)(addr & 0xff);
    cmd[3] = AT778UV_BLOCK_SIZE;

    retval = at778uv_transaction(rig, cmd, sizeof(cmd), 'W', reply, sizeof(reply),
                                 AT778UV_RETRIES);

    if (retval != RIG_OK)
    {
        return retval;
    }

    if (reply[0] != 'W' || reply[1] != cmd[1] || reply[2] != cmd[2]
            || reply[3] != AT778UV_BLOCK_SIZE
            || reply[AT778UV_READ_REPLY_LEN - 1] != AT778UV_ACK)
    {
        rig_debug(RIG_DEBUG_ERR, "%s: malformed reply for 0x%04x\n", __func__, addr);
        return -RIG_EPROTO;
    }

    /* Sum runs from the address through the data, excluding the leading 'W'. */
    for (i = 1; i < AT778UV_READ_REPLY_LEN - 2; i++)
    {
        sum = (unsigned char)(sum + reply[i]);
    }

    if (sum != reply[AT778UV_READ_REPLY_LEN - 2])
    {
        rig_debug(RIG_DEBUG_ERR, "%s: checksum %02x != %02x for 0x%04x\n",
                  __func__, sum, reply[AT778UV_READ_REPLY_LEN - 2], addr);
        return -RIG_EPROTO;
    }

    memcpy(data, &reply[4], AT778UV_BLOCK_SIZE);

    return RIG_OK;
}

//
// Issue one 'D' sub-command. The frame is always seven bytes; data bytes a
// sub-handler does not consult are ignored, but they must be present or the
// radio holds the line waiting for them and swallows whatever is sent next.
//
static int at778uv_set_param(RIG *rig, unsigned char sub, unsigned char d0,
                             unsigned char d1, unsigned char d2, unsigned char d3)
{
    unsigned char cmd[AT778UV_SET_FRAME_LEN];
    unsigned char ack = 0;
    int retval;

    cmd[0] = AT778UV_CMD_SET;
    cmd[1] = sub;
    cmd[2] = d0;
    cmd[3] = d1;
    cmd[4] = d2;
    cmd[5] = d3;
    cmd[6] = AT778UV_ACK;

    retval = at778uv_transaction(rig, cmd, sizeof(cmd), AT778UV_ACK, &ack, 1,
                                 AT778UV_RETRIES);

    if (retval != RIG_OK)
    {
        return retval;
    }

    if (ack != AT778UV_ACK)
    {
        rig_debug(RIG_DEBUG_ERR, "%s: sub %d not acked (0x%02x)\n", __func__, sub,
                  ack);
        return -RIG_ERJCTED;
    }

    return RIG_OK;
}

//
// Poll the status frame. Key 'R' is the only key in the radio's table that
// posts no UI event, so this is a read with no side effect. Collisions damage
// the head of the reply often enough to be worth one retry.
//
static int at778uv_status(RIG *rig, unsigned char *st)
{
    static const unsigned char cmd[AT778UV_KEY_FRAME_LEN] =
    {
        AT778UV_CMD_KEY, 0x00, 0x01, 0x00, AT778UV_KEY_STATUS, 0x00, 0x00, AT778UV_ACK
    };

    /* Safe to repeat: 'R' is the one key in the radio's table that posts no
       UI event, so a retried status poll changes nothing. */
    return at778uv_transaction(rig, cmd, sizeof(cmd), 'S', st,
                               AT778UV_STATUS_LEN, AT778UV_RETRIES);
}

//
// Inject a microphone keypress. Press only: the radio counts a release frame
// as a second press, which silently cancels anything that toggles.
//
static int at778uv_key(RIG *rig, unsigned char key)
{
    unsigned char cmd[AT778UV_KEY_FRAME_LEN];

    cmd[0] = AT778UV_CMD_KEY;
    cmd[1] = 0x00;
    cmd[2] = 0x01;
    cmd[3] = 0x00;
    cmd[4] = key;
    cmd[5] = 0x00;
    cmd[6] = 0x00;
    cmd[7] = AT778UV_ACK;

    /* Key frames draw no reply at all, and must never be retried: the radio's
       keys are toggles, so a repeat undoes the press. One attempt only. */
    return at778uv_transaction(rig, cmd, sizeof(cmd), 0, NULL, 0, 1);
}

// ---------------------------------------------------------------------------
//    STATE
// ---------------------------------------------------------------------------

static int at778uv_selected_vfo(RIG *rig, vfo_t *vfo)
{
    unsigned char st[AT778UV_STATUS_LEN];
    int retval = at778uv_status(rig, st);

    if (retval != RIG_OK)
    {
        return retval;
    }

    *vfo = st[AT778UV_ST_SLOT] ? RIG_VFO_B : RIG_VFO_A;

    return RIG_OK;
}

//
// Resolve RIG_VFO_CURR and friends to a concrete slot.
//
static int at778uv_resolve_vfo(RIG *rig, vfo_t vfo, vfo_t *out)
{
    struct at778uv_priv_data *priv = STATE(rig)->priv;

    if (vfo == RIG_VFO_A || vfo == RIG_VFO_B)
    {
        *out = vfo;
        return RIG_OK;
    }

    if (vfo == RIG_VFO_MAIN)
    {
        *out = RIG_VFO_A;
        return RIG_OK;
    }

    if (vfo == RIG_VFO_SUB)
    {
        *out = RIG_VFO_B;
        return RIG_OK;
    }

    if (priv->curr_vfo == RIG_VFO_A || priv->curr_vfo == RIG_VFO_B)
    {
        *out = priv->curr_vfo;
        return RIG_OK;
    }

    return at778uv_selected_vfo(rig, out);
}

// ---------------------------------------------------------------------------
//    VFO
// ---------------------------------------------------------------------------

static int at778uv_get_vfo(RIG *rig, vfo_t *vfo)
{
    struct at778uv_priv_data *priv = STATE(rig)->priv;
    int retval;

    ENTERFUNC;

    retval = at778uv_selected_vfo(rig, vfo);

    if (retval == RIG_OK)
    {
        priv->curr_vfo = *vfo;
    }

    RETURNFUNC(retval);
}

//
// The radio offers a toggle, not a set, so read first and only send the key
// if the slot is wrong - then confirm. Sending it blindly would flip a radio
// that was already correct, and Hamlib's own get_dcd path calls set_vfo twice
// per poll when a VFO is named explicitly.
//
static int at778uv_set_vfo(RIG *rig, vfo_t vfo)
{
    struct at778uv_priv_data *priv = STATE(rig)->priv;
    vfo_t want;
    vfo_t have;
    int retval;

    ENTERFUNC;

    retval = at778uv_resolve_vfo(rig, vfo, &want);

    if (retval != RIG_OK)
    {
        RETURNFUNC(retval);
    }

    retval = at778uv_selected_vfo(rig, &have);

    if (retval != RIG_OK)
    {
        RETURNFUNC(retval);
    }

    if (have == want)
    {
        priv->curr_vfo = have;
        RETURNFUNC(RIG_OK);
    }

    retval = at778uv_key(rig, AT778UV_KEY_VFO_TOGGLE);

    if (retval != RIG_OK)
    {
        RETURNFUNC(retval);
    }

    retval = at778uv_selected_vfo(rig, &have);

    if (retval != RIG_OK)
    {
        RETURNFUNC(retval);
    }

    if (have != want)
    {
        rig_debug(RIG_DEBUG_ERR, "%s: toggle did not select the requested VFO\n",
                  __func__);
        RETURNFUNC(-RIG_EPROTO);
    }

    priv->curr_vfo = have;

    RETURNFUNC(RIG_OK);
}

/* Defined below, with the other record helpers. */
static int at778uv_record_addr(RIG *rig, vfo_t vfo, unsigned short *addr,
                               int *is_vfo_out);

// ---------------------------------------------------------------------------
//    THE MEMORY-MODE GUARD
//
// 'D' writes whichever record the radio has selected. In memory mode that is
// the stored memory channel: the write acks, the display does not move, and
// the channel is permanently overwritten. A retune loop in that state would
// walk an operator's memories destroying them one at a time with no visible
// symptom, so every record write goes through here first.
//
// The mode is not cached. The front panel can change it at any moment, and
// the two reads cost about as much as the write they protect.
// ---------------------------------------------------------------------------

//
// `addr`, when not NULL, receives the address of the record the write will
// land on. The mode check below already reads the live block to find it, so
// a caller that afterwards wants to read the record back can use this
// instead of resolving it a second time - two of the seven bus transactions
// a set_freq used to cost were exactly that duplication.
//
static int at778uv_prepare_record_write(RIG *rig, vfo_t vfo, vfo_t *target,
                                        unsigned short *addr)
{
    unsigned short resolved = 0;
    int is_vfo = 0;
    int retval;

    retval = at778uv_resolve_vfo(rig, vfo, target);

    if (retval != RIG_OK)
    {
        return retval;
    }

    /* One read serves both questions. The live block carries the mode and
       the channel number for both slots, so the address the write will land
       on comes out of the same 83 ms transaction as the permission check. */
    retval = at778uv_record_addr(rig, *target, &resolved, &is_vfo);

    if (retval != RIG_OK)
    {
        return retval;
    }

    if (!is_vfo)
    {
        rig_debug(RIG_DEBUG_ERR,
                  "%s: %s is in memory mode; refusing to write, as the radio "
                  "would overwrite the stored channel\n",
                  __func__, rig_strvfo(*target));

        /* Deliberately NOT -RIG_ENTARGET. rig_set_freq turns that one into
           RIG_OK on purpose (src/rig.c, "we will just return RIG_OK and the
           frequency set will be ignored"), which would hand the caller a
           success for a write we refused - the very silent failure this
           guard exists to prevent. RIG_ERJCTED propagates. */
        return -RIG_ERJCTED;
    }

    /* Only now is it worth moving the radio. */
    retval = at778uv_set_vfo(rig, *target);

    if (retval != RIG_OK)
    {
        return retval;
    }

    if (addr != NULL)
    {
        *addr = resolved;
    }

    return RIG_OK;
}

//
// Read the record the slot is actually operating from.
//
// This is NOT simply the VFO record. In memory mode the radio runs from the
// stored memory channel, and the VFO record sits untouched - so reading it
// would report the frequency, mode, power and tones of a VFO the operator is
// not listening to, with no indication anything was wrong. Measured: with the
// radio on memory channel 58, reading the VFO record returned the VFO's
// 446.030 while the radio was tuned elsewhere entirely.
//
// So resolve the address first. Display channel N lives at (N-1) * 32, and
// the channel byte at 0x3260 is already zero based, so it is the multiplier
// directly.
//
// The cost is one extra block read, ~83 ms, on every getter. That is the
// price of not lying about what the radio is doing, and Hamlib's own cache
// absorbs it for a client polling at any sane rate.
//
static int at778uv_record_addr(RIG *rig, vfo_t vfo, unsigned short *addr,
                               int *is_vfo_out)
{
    unsigned char live[AT778UV_BLOCK_SIZE];
    int retval = at778uv_read_block(rig, AT778UV_ADDR_LIVE, live);
    int is_vfo;
    int chan;

    if (retval != RIG_OK)
    {
        return retval;
    }

    if (vfo == RIG_VFO_B)
    {
        is_vfo = live[AT778UV_LIVE_OFF_MODEB] & 1;
        chan   = live[AT778UV_LIVE_OFF_CHANB];
    }
    else
    {
        is_vfo = live[AT778UV_LIVE_OFF_MODEA] & 1;
        chan   = live[AT778UV_LIVE_OFF_CHANA];
    }

    if (is_vfo_out != NULL)
    {
        *is_vfo_out = is_vfo;
    }

    if (is_vfo)
    {
        *addr = (vfo == RIG_VFO_B) ? AT778UV_ADDR_VFOB : AT778UV_ADDR_VFOA;
    }
    else
    {
        /* Refuse to turn a bad channel number into an address. The records
           run 0..199; 200 would land on the VFO A record and anything above
           that on the occupied/scan bitmaps, so an out-of-range byte would
           quietly produce a plausible-looking frequency from the wrong
           part of the codeplug. */
        if (chan >= AT778UV_CHANNELS)
        {
            rig_debug(RIG_DEBUG_ERR, "%s: channel %d out of range\n", __func__,
                      chan);
            return -RIG_EPROTO;
        }

        *addr = (unsigned short)(chan * AT778UV_RECORD_SIZE);
    }

    return RIG_OK;
}

static int at778uv_read_record(RIG *rig, vfo_t vfo, unsigned char *rec)
{
    unsigned short addr;
    int retval = at778uv_record_addr(rig, vfo, &addr, NULL);

    if (retval != RIG_OK)
    {
        return retval;
    }

    return at778uv_read_block(rig, addr, rec);
}

// ---------------------------------------------------------------------------
//    FREQUENCY
// ---------------------------------------------------------------------------

static int at778uv_set_freq(RIG *rig, vfo_t vfo, freq_t freq)
{
    unsigned char bcd[4];
    unsigned char rec[AT778UV_BLOCK_SIZE];
    unsigned short addr;
    vfo_t target;
    int retval;

    ENTERFUNC;

    retval = at778uv_prepare_record_write(rig, vfo, &target, &addr);

    if (retval != RIG_OK)
    {
        RETURNFUNC(retval);
    }

    at778uv_to_bcd(freq, bcd);

    /* Skip a write that would change nothing. These writes reach EEPROM -
       the radio persists them exactly as it persists a turn of the dial - so
       a client polling set_freq at its idle frequency would otherwise burn a
       write cycle every time it called. The read costs 83 ms against the
       134 ms write it avoids. */
    retval = at778uv_read_block(rig, addr, rec);

    if (retval != RIG_OK)
    {
        RETURNFUNC(retval);
    }

    if (memcmp(&rec[AT778UV_REC_FREQ], bcd, 4) == 0)
    {
        rig_debug(RIG_DEBUG_VERBOSE, "%s: already on %"PRIfreq" Hz, no write\n",
                  __func__, freq);
        RETURNFUNC(RIG_OK);
    }

    retval = at778uv_set_param(rig, AT778UV_SET_FREQ, bcd[0], bcd[1], bcd[2],
                               bcd[3]);

    if (retval != RIG_OK)
    {
        RETURNFUNC(retval);
    }

    /* Read back. A write the radio quietly declined acks exactly like one it
       accepted, so the only honest confirmation is the stored record. */
    retval = at778uv_read_block(rig, addr, rec);

    if (retval != RIG_OK)
    {
        RETURNFUNC(retval);
    }

    if (memcmp(&rec[AT778UV_REC_FREQ], bcd, 4) != 0)
    {
        rig_debug(RIG_DEBUG_ERR, "%s: readback mismatch, frequency not applied\n",
                  __func__);
        RETURNFUNC(-RIG_EPROTO);
    }

    RETURNFUNC(RIG_OK);
}

static int at778uv_get_freq(RIG *rig, vfo_t vfo, freq_t *freq)
{
    unsigned char rec[AT778UV_BLOCK_SIZE];
    vfo_t target;
    int retval;

    ENTERFUNC;

    retval = at778uv_resolve_vfo(rig, vfo, &target);

    if (retval != RIG_OK)
    {
        RETURNFUNC(retval);
    }

    retval = at778uv_read_record(rig, target, rec);

    if (retval != RIG_OK)
    {
        RETURNFUNC(retval);
    }

    RETURNFUNC(at778uv_from_bcd(&rec[AT778UV_REC_FREQ], freq));
}

// ---------------------------------------------------------------------------
//    CARRIER DETECT
//
// Byte 2 is the carrier on the primary slot and byte 3 the carrier on the
// secondary one. Under dual watch either can open, so both are reported.
// ---------------------------------------------------------------------------

static int at778uv_get_dcd(RIG *rig, vfo_t vfo, dcd_t *dcd)
{
    unsigned char st[AT778UV_STATUS_LEN];
    int retval;

    ENTERFUNC;

    /* Carrier detect is reported for both slots at once, so the requested
       VFO does not narrow it - see the OR below. */
    (void)vfo;

    retval = at778uv_status(rig, st);

    if (retval != RIG_OK)
    {
        RETURNFUNC(retval);
    }

    *dcd = (st[AT778UV_ST_COS_A] || st[AT778UV_ST_COS_B]) ? RIG_DCD_ON
           : RIG_DCD_OFF;

    RETURNFUNC(RIG_OK);
}

// ---------------------------------------------------------------------------
//    MODE
// ---------------------------------------------------------------------------

static int at778uv_set_mode(RIG *rig, vfo_t vfo, rmode_t mode, pbwidth_t width)
{
    vfo_t target;
    unsigned char code;
    int retval;

    ENTERFUNC;

    if (mode == RIG_MODE_FMN)
    {
        code = 0;                       /* 12.5 kHz */
    }
    else if (mode == RIG_MODE_FM)
    {
        /* RIG_PASSBAND_NOCHANGE is -1 and means leave the width alone, so it
           must not be lumped in with "not specified" - that would widen a
           channel the caller asked us not to touch. NORMAL (0) does mean
           "pick the default", which here is the wide 25 kHz setting. */
        if (width == RIG_PASSBAND_NOCHANGE)
        {
            RETURNFUNC(RIG_OK);
        }

        code = (width > 0 && width <= 20000) ? 1 : 2;    /* 20 kHz or 25 kHz */
    }
    else
    {
        RETURNFUNC(-RIG_EINVAL);
    }

    retval = at778uv_prepare_record_write(rig, vfo, &target, NULL);

    if (retval != RIG_OK)
    {
        RETURNFUNC(retval);
    }

    RETURNFUNC(at778uv_set_param(rig, AT778UV_SET_WIDTH, code, 0, 0, 0));
}

static int at778uv_get_mode(RIG *rig, vfo_t vfo, rmode_t *mode,
                            pbwidth_t *width)
{
    unsigned char rec[AT778UV_BLOCK_SIZE];
    vfo_t target;
    int retval;
    int code;

    ENTERFUNC;

    retval = at778uv_resolve_vfo(rig, vfo, &target);

    if (retval != RIG_OK)
    {
        RETURNFUNC(retval);
    }

    retval = at778uv_read_record(rig, target, rec);

    if (retval != RIG_OK)
    {
        RETURNFUNC(retval);
    }

    code = (rec[AT778UV_REC_WIDTH] >> 2) & 0x03;

    switch (code)
    {
    case 0:
        *mode = RIG_MODE_FMN;
        *width = kHz(12.5);
        break;

    case 1:
        *mode = RIG_MODE_FM;
        *width = kHz(20);
        break;

    default:
        *mode = RIG_MODE_FM;
        *width = kHz(25);
        break;
    }

    RETURNFUNC(RIG_OK);
}

// ---------------------------------------------------------------------------
//    LEVELS
//
// The radio does not range-check any of these: a squelch of 0xff is stored
// verbatim into a field its own menu treats as four bits. Clamping is ours.
// ---------------------------------------------------------------------------

static int at778uv_set_level(RIG *rig, vfo_t vfo, setting_t level, value_t val)
{
    vfo_t target;
    int retval;
    int v;

    ENTERFUNC;

    switch (level)
    {
    case RIG_LEVEL_AF:
        /* Zero is the mute path, not a volume; use RIG_FUNC_MUTE for that. */
        v = at778uv_scale(val.f, 1, AT778UV_VOLUME_MAX);

        RETURNFUNC(at778uv_set_param(rig, AT778UV_SET_VOLUME,
                                     (unsigned char)v, 0, 0, 0));

    case RIG_LEVEL_SQL:
        v = at778uv_scale(val.f, 0, AT778UV_SQUELCH_MAX);

        RETURNFUNC(at778uv_set_param(rig, AT778UV_SET_SQUELCH,
                                     (unsigned char)v, 0, 0, 0));

    case RIG_LEVEL_RFPOWER:
        v = at778uv_scale(val.f, 0, AT778UV_POWER_MAX);

        retval = at778uv_prepare_record_write(rig, vfo, &target, NULL);

        if (retval != RIG_OK)
        {
            RETURNFUNC(retval);
        }

        RETURNFUNC(at778uv_set_param(rig, AT778UV_SET_POWER,
                                     (unsigned char)v, 0, 0, 0));

    default:
        RETURNFUNC(-RIG_EINVAL);
    }
}

static int at778uv_get_level(RIG *rig, vfo_t vfo, setting_t level, value_t *val)
{
    unsigned char buf[AT778UV_BLOCK_SIZE];
    vfo_t target;
    int retval;

    ENTERFUNC;

    switch (level)
    {
    case RIG_LEVEL_AF:
    case RIG_LEVEL_SQL:
        retval = at778uv_read_block(rig, AT778UV_ADDR_SETTINGS, buf);

        if (retval != RIG_OK)
        {
            RETURNFUNC(retval);
        }

        if (level == RIG_LEVEL_AF)
        {
            val->f = (float)buf[AT778UV_SET_OFF_VOLUME] / AT778UV_VOLUME_MAX;
        }
        else
        {
            val->f = (float)(buf[AT778UV_SET_OFF_SQUELCH] & 0x0f)
                     / AT778UV_SQUELCH_MAX;
        }

        RETURNFUNC(RIG_OK);

    case RIG_LEVEL_RFPOWER:
        retval = at778uv_resolve_vfo(rig, vfo, &target);

        if (retval != RIG_OK)
        {
            RETURNFUNC(retval);
        }

        retval = at778uv_read_record(rig, target, buf);

        if (retval != RIG_OK)
        {
            RETURNFUNC(retval);
        }

        val->f = (float)((buf[AT778UV_REC_POWER] >> 2) & 0x03) / 2.0f;

        RETURNFUNC(RIG_OK);

    default:
        RETURNFUNC(-RIG_EINVAL);
    }
}

// ---------------------------------------------------------------------------
//    FUNCTIONS
//
// Mute and volume share one sub-command: writing 0 sets a mute bit and
// deliberately leaves the stored level alone, so unmuting only has to send
// the level back. The mute bit itself lives in a SRAM byte the status frame
// masks off, so get_func(MUTE) is not offered - there is no honest read.
// ---------------------------------------------------------------------------

static int at778uv_set_func(RIG *rig, vfo_t vfo, setting_t func, int status)
{
    unsigned char buf[AT778UV_BLOCK_SIZE];
    vfo_t target;
    int level;
    int retval;

    ENTERFUNC;

    switch (func)
    {
    case RIG_FUNC_MUTE:
        if (status)
        {
            RETURNFUNC(at778uv_set_param(rig, AT778UV_SET_VOLUME, 0, 0, 0, 0));
        }

        retval = at778uv_read_block(rig, AT778UV_ADDR_SETTINGS, buf);

        if (retval != RIG_OK)
        {
            RETURNFUNC(retval);
        }

        /* A stored zero would mute again - the operator can wind the volume
           knob all the way down - so unmute to the quietest audible step
           instead of silently doing nothing. */
        level = buf[AT778UV_SET_OFF_VOLUME];

        if (level < 1)
        {
            level = 1;
        }

        RETURNFUNC(at778uv_set_param(rig, AT778UV_SET_VOLUME,
                                     (unsigned char)level, 0, 0, 0));

    case RIG_FUNC_TONE:
        retval = at778uv_prepare_record_write(rig, vfo, &target, NULL);

        if (retval != RIG_OK)
        {
            RETURNFUNC(retval);
        }

        if (!status)
        {
            RETURNFUNC(at778uv_set_param(rig, AT778UV_SET_TXTONE,
                                         AT778UV_TONE_OFF, 0, 0, 0));
        }

        retval = at778uv_read_record(rig, target, buf);

        if (retval != RIG_OK)
        {
            RETURNFUNC(retval);
        }

        RETURNFUNC(at778uv_set_param(rig, AT778UV_SET_TXTONE, AT778UV_TONE_CTCSS,
                                     buf[AT778UV_REC_TXTONE], 0, 0));

    case RIG_FUNC_TSQL:
        retval = at778uv_prepare_record_write(rig, vfo, &target, NULL);

        if (retval != RIG_OK)
        {
            RETURNFUNC(retval);
        }

        if (!status)
        {
            RETURNFUNC(at778uv_set_param(rig, AT778UV_SET_RXTONE,
                                         AT778UV_TONE_OFF, 0, 0, 0));
        }

        retval = at778uv_read_record(rig, target, buf);

        if (retval != RIG_OK)
        {
            RETURNFUNC(retval);
        }

        RETURNFUNC(at778uv_set_param(rig, AT778UV_SET_RXTONE, AT778UV_TONE_CTCSS,
                                     buf[AT778UV_REC_RXTONE], 0, 0));

    default:
        RETURNFUNC(-RIG_EINVAL);
    }
}

static int at778uv_get_func(RIG *rig, vfo_t vfo, setting_t func, int *status)
{
    unsigned char rec[AT778UV_BLOCK_SIZE];
    vfo_t target;
    int retval;

    ENTERFUNC;

    if (func != RIG_FUNC_TONE && func != RIG_FUNC_TSQL)
    {
        RETURNFUNC(-RIG_EINVAL);
    }

    retval = at778uv_resolve_vfo(rig, vfo, &target);

    if (retval != RIG_OK)
    {
        RETURNFUNC(retval);
    }

    retval = at778uv_read_record(rig, target, rec);

    if (retval != RIG_OK)
    {
        RETURNFUNC(retval);
    }

    if (func == RIG_FUNC_TONE)
    {
        *status = (rec[AT778UV_REC_TONEMODE] & 0x03) == AT778UV_TONE_CTCSS;
    }
    else
    {
        *status = ((rec[AT778UV_REC_TONEMODE] >> 2) & 0x03) == AT778UV_TONE_CTCSS;
    }

    RETURNFUNC(RIG_OK);
}

// ---------------------------------------------------------------------------
//    TONES
// ---------------------------------------------------------------------------

static int at778uv_tone_index(tone_t tone)
{
    int i;

    for (i = 0; at778uv_ctcss_list[i] != 0; i++)
    {
        if (at778uv_ctcss_list[i] == tone)
        {
            return i;
        }
    }

    return -1;
}

static int at778uv_set_tone(RIG *rig, vfo_t vfo, tone_t tone, int sub)
{
    vfo_t target;
    int idx;
    int retval;

    if (tone == 0)
    {
        retval = at778uv_prepare_record_write(rig, vfo, &target, NULL);

        if (retval != RIG_OK)
        {
            return retval;
        }

        return at778uv_set_param(rig, (unsigned char)sub, AT778UV_TONE_OFF, 0, 0, 0);
    }

    idx = at778uv_tone_index(tone);

    if (idx < 0)
    {
        return -RIG_EINVAL;
    }

    retval = at778uv_prepare_record_write(rig, vfo, &target, NULL);

    if (retval != RIG_OK)
    {
        return retval;
    }

    return at778uv_set_param(rig, (unsigned char)sub, AT778UV_TONE_CTCSS,
                             (unsigned char)idx, 0, 0);
}

static int at778uv_set_ctcss_tone(RIG *rig, vfo_t vfo, tone_t tone)
{
    ENTERFUNC;
    RETURNFUNC(at778uv_set_tone(rig, vfo, tone, AT778UV_SET_TXTONE));
}

static int at778uv_set_ctcss_sql(RIG *rig, vfo_t vfo, tone_t tone)
{
    ENTERFUNC;
    RETURNFUNC(at778uv_set_tone(rig, vfo, tone, AT778UV_SET_RXTONE));
}

static int at778uv_get_tone(RIG *rig, vfo_t vfo, tone_t *tone, int rx)
{
    unsigned char rec[AT778UV_BLOCK_SIZE];
    vfo_t target;
    int retval;
    int mode;
    int idx;

    retval = at778uv_resolve_vfo(rig, vfo, &target);

    if (retval != RIG_OK)
    {
        return retval;
    }

    retval = at778uv_read_record(rig, target, rec);

    if (retval != RIG_OK)
    {
        return retval;
    }

    mode = rx ? ((rec[AT778UV_REC_TONEMODE] >> 2) & 0x03)
           : (rec[AT778UV_REC_TONEMODE] & 0x03);

    if (mode != AT778UV_TONE_CTCSS)
    {
        *tone = 0;
        return RIG_OK;
    }

    idx = rx ? rec[AT778UV_REC_RXTONE] : rec[AT778UV_REC_TXTONE];

    if (idx < 0 || idx >= (int)(sizeof(at778uv_ctcss_list) / sizeof(tone_t)) - 1)
    {
        *tone = 0;
        return RIG_OK;
    }

    *tone = at778uv_ctcss_list[idx];

    return RIG_OK;
}

static int at778uv_get_ctcss_tone(RIG *rig, vfo_t vfo, tone_t *tone)
{
    ENTERFUNC;
    RETURNFUNC(at778uv_get_tone(rig, vfo, tone, 0));
}

static int at778uv_get_ctcss_sql(RIG *rig, vfo_t vfo, tone_t *tone)
{
    ENTERFUNC;
    RETURNFUNC(at778uv_get_tone(rig, vfo, tone, 1));
}

//
// DCS. The nine-bit code is split across two data bytes of the frame: the
// invert flag first, then the code's low byte, then its ninth bit.
//
static int at778uv_set_dcs(RIG *rig, vfo_t vfo, tone_t code, int sub)
{
    vfo_t target;
    int wire;
    int retval;

    retval = at778uv_prepare_record_write(rig, vfo, &target, NULL);

    if (retval != RIG_OK)
    {
        return retval;
    }

    if (code == 0)
    {
        return at778uv_set_param(rig, (unsigned char)sub, AT778UV_TONE_OFF, 0, 0, 0);
    }

    wire = at778uv_dcs_to_wire(code);

    return at778uv_set_param(rig, (unsigned char)sub, AT778UV_TONE_DCS, 0,
                             (unsigned char)(wire & 0xff),
                             (unsigned char)((wire >> 8) & 0x01));
}

static int at778uv_set_dcs_code(RIG *rig, vfo_t vfo, tone_t code)
{
    ENTERFUNC;
    RETURNFUNC(at778uv_set_dcs(rig, vfo, code, AT778UV_SET_TXTONE));
}

static int at778uv_set_dcs_sql(RIG *rig, vfo_t vfo, tone_t code)
{
    ENTERFUNC;
    RETURNFUNC(at778uv_set_dcs(rig, vfo, code, AT778UV_SET_RXTONE));
}

//
// Reading a DCS code back. The RX pair sits in the first half of the record;
// the TX pair is at offsets 16 and 17, which is the second 16-byte block.
//
static int at778uv_get_dcs(RIG *rig, vfo_t vfo, tone_t *code, int rx)
{
    unsigned char rec[AT778UV_BLOCK_SIZE];
    vfo_t target;
    unsigned short addr;
    int retval;
    int mode;
    int wire;

    retval = at778uv_resolve_vfo(rig, vfo, &target);

    if (retval != RIG_OK)
    {
        return retval;
    }

    retval = at778uv_read_record(rig, target, rec);

    if (retval != RIG_OK)
    {
        return retval;
    }

    mode = rx ? ((rec[AT778UV_REC_TONEMODE] >> 2) & 0x03)
           : (rec[AT778UV_REC_TONEMODE] & 0x03);

    if (mode != AT778UV_TONE_DCS)
    {
        *code = 0;
        return RIG_OK;
    }

    if (rx)
    {
        wire = rec[AT778UV_REC_RXDCS]
               | ((rec[AT778UV_REC_RXDCS_HI] & 0x01) << 8);
    }
    else
    {
        retval = at778uv_record_addr(rig, target, &addr, NULL);

        if (retval != RIG_OK)
        {
            return retval;
        }

        retval = at778uv_read_block(rig, addr + AT778UV_BLOCK_SIZE, rec);

        if (retval != RIG_OK)
        {
            return retval;
        }

        wire = rec[AT778UV_REC_TXDCS - AT778UV_BLOCK_SIZE]
               | ((rec[AT778UV_REC_TXDCS_HI - AT778UV_BLOCK_SIZE] & 0x01) << 8);
    }

    *code = at778uv_dcs_from_wire(wire);

    return RIG_OK;
}

static int at778uv_get_dcs_code(RIG *rig, vfo_t vfo, tone_t *code)
{
    ENTERFUNC;
    RETURNFUNC(at778uv_get_dcs(rig, vfo, code, 0));
}

static int at778uv_get_dcs_sql(RIG *rig, vfo_t vfo, tone_t *code)
{
    ENTERFUNC;
    RETURNFUNC(at778uv_get_dcs(rig, vfo, code, 1));
}

// ---------------------------------------------------------------------------
//    REPEATER OFFSET
//
// Writing an offset also forces the record's duplex field to a negative
// shift, and nothing in the command set writes that field with any other
// value. So an offset can be set but never cleared from here, and
// set_rptr_shift is deliberately not offered.
// ---------------------------------------------------------------------------

static int at778uv_set_rptr_offs(RIG *rig, vfo_t vfo, shortfreq_t offs)
{
    unsigned char bcd[4];
    vfo_t target;
    int retval;

    ENTERFUNC;

    /* The encoder takes an unsigned magnitude, and the radio has no way to
       express a positive shift anyway - see the note above. */
    if (offs < 0)
    {
        RETURNFUNC(-RIG_EINVAL);
    }

    retval = at778uv_prepare_record_write(rig, vfo, &target, NULL);

    if (retval != RIG_OK)
    {
        RETURNFUNC(retval);
    }

    at778uv_to_bcd((freq_t)offs, bcd);

    RETURNFUNC(at778uv_set_param(rig, AT778UV_SET_OFFSET, bcd[0], bcd[1], bcd[2],
                                 bcd[3]));
}

static int at778uv_get_rptr_offs(RIG *rig, vfo_t vfo, shortfreq_t *offs)
{
    unsigned char rec[AT778UV_BLOCK_SIZE];
    freq_t offsfreq;
    vfo_t target;
    int retval;

    ENTERFUNC;

    retval = at778uv_resolve_vfo(rig, vfo, &target);

    if (retval != RIG_OK)
    {
        RETURNFUNC(retval);
    }

    retval = at778uv_read_record(rig, target, rec);

    if (retval != RIG_OK)
    {
        RETURNFUNC(retval);
    }

    retval = at778uv_from_bcd(&rec[AT778UV_REC_OFFSET], &offsfreq);

    if (retval != RIG_OK)
    {
        RETURNFUNC(retval);
    }

    *offs = (shortfreq_t)offsfreq;

    RETURNFUNC(RIG_OK);
}

//
// Which memory channel the slot is parked on.
//
// Worth having even though the channel cannot be *set* from this bus: it is
// the only way a client can see that the operator has moved the radio into
// memory mode and started spinning through channels. Without it the first
// sign of memory mode is a set_freq being refused.
//
// The stored value is zero based - 0x2c is the channel the display calls 45 -
// so it is reported the way the radio's own screen shows it.
//
static int at778uv_get_mem(RIG *rig, vfo_t vfo, int *ch)
{
    unsigned char live[AT778UV_BLOCK_SIZE];
    vfo_t target;
    int retval;

    ENTERFUNC;

    retval = at778uv_resolve_vfo(rig, vfo, &target);

    if (retval != RIG_OK)
    {
        RETURNFUNC(retval);
    }

    retval = at778uv_read_block(rig, AT778UV_ADDR_LIVE, live);

    if (retval != RIG_OK)
    {
        RETURNFUNC(retval);
    }

    *ch = 1 + (int)live[(target == RIG_VFO_B) ? AT778UV_LIVE_OFF_CHANB
                        : AT778UV_LIVE_OFF_CHANA];

    RETURNFUNC(RIG_OK);
}

// ---------------------------------------------------------------------------
//    LIFECYCLE
// ---------------------------------------------------------------------------

static int at778uv_init(RIG *rig)
{
    struct at778uv_priv_data *priv;

    ENTERFUNC;

    priv = calloc(1, sizeof(struct at778uv_priv_data));

    if (priv == NULL)
    {
        RETURNFUNC(-RIG_ENOMEM);
    }

    priv->curr_vfo = RIG_VFO_NONE;
    STATE(rig)->priv = priv;

    RETURNFUNC(RIG_OK);
}

static int at778uv_cleanup(RIG *rig)
{
    ENTERFUNC;

    free(STATE(rig)->priv);
    STATE(rig)->priv = NULL;

    RETURNFUNC(RIG_OK);
}

//
// Identify the radio, then note two things the operator may want to know:
// whether VOX is fitted, which depends on the model string ending in 'P',
// and whether dual watch is on, which changes where a carrier can appear.
//
static int at778uv_open(RIG *rig)
{
    struct at778uv_priv_data *priv = STATE(rig)->priv;
    static const unsigned char cmd[1] = { AT778UV_CMD_IDENT };
    unsigned char reply[AT778UV_ID_LEN];
    unsigned char settings[AT778UV_BLOCK_SIZE];
    size_t len;
    int has_vox;
    int retval;

    ENTERFUNC;

    retval = at778uv_transaction(rig, cmd, sizeof(cmd), 'I', reply, sizeof(reply),
                                 AT778UV_RETRIES);

    if (retval != RIG_OK)
    {
        RETURNFUNC(retval);
    }

    if (reply[0] != 'I' || reply[AT778UV_ID_LEN - 1] != AT778UV_ACK)
    {
        rig_debug(RIG_DEBUG_ERR, "%s: no identify response\n", __func__);
        RETURNFUNC(-RIG_EPROTO);
    }

    memcpy(priv->model, &reply[AT778UV_ID_MODEL], AT778UV_ID_MODEL_LEN);
    priv->model[AT778UV_ID_MODEL_LEN] = '\0';
    memcpy(priv->version, &reply[AT778UV_ID_VERSION], AT778UV_ID_VERSION_LEN);
    priv->version[AT778UV_ID_VERSION_LEN] = '\0';

    /* The reported model is recorded but deliberately NOT checked against the
       selected one. This design is sold under at least five badges and each
       reports its own name - an RT95 answers "RT95" where an AT-778UV answers
       "AT778UV" - so rejecting a mismatch would refuse exactly the radios this
       backend exists to drive. The string is logged instead, which is what a
       user should quote in a bug report.

       VOX is fitted only to the -P variants, identified by a trailing 'P',
       the same test CHIRP's driver uses. Guard the length: a radio that
       reported an empty model would otherwise index model[-1]. */
    len = strlen(priv->model);
    has_vox = (len > 0 && priv->model[len - 1] == 'P');

    rig_debug(RIG_DEBUG_VERBOSE, "%s: model %s firmware %s, VOX %s\n", __func__,
              priv->model, priv->version, has_vox ? "fitted" : "not fitted");

    /* Informational only, so a failure here is deliberately not propagated:
       dual watch tells the operator where a carrier may appear, but not being
       able to read it is no reason to refuse to open the rig. */
    retval = at778uv_read_block(rig, AT778UV_ADDR_SETTINGS, settings);

    if (retval == RIG_OK)
    {
        if (settings[AT778UV_SET_OFF_DUALW] & 1)
        {
            rig_debug(RIG_DEBUG_VERBOSE,
                      "%s: dual watch is on - a carrier may open on either slot\n",
                      __func__);
        }
    }

    retval = at778uv_selected_vfo(rig, &priv->curr_vfo);

    if (retval != RIG_OK)
    {
        RETURNFUNC(retval);
    }

    RETURNFUNC(RIG_OK);
}

static int at778uv_close(RIG *rig)
{
    ENTERFUNC;
    RETURNFUNC(RIG_OK);
}

static const char *at778uv_get_info(RIG *rig)
{
    struct at778uv_priv_data *priv = STATE(rig)->priv;
    static char info[32];

    SNPRINTF(info, sizeof(info), "%s %s", priv->model, priv->version);

    return info;
}

// ---------------------------------------------------------------------------
//    CAPABILITIES
//
// PTT is not available over this bus - the microphone socket carries it on a
// discrete pin instead - so no PTT callbacks are declared and the operator
// keys the radio through that pin or a serial control line. The radio's
// received signal strength exists internally but is never sent to the
// microphone bus, so there is no S-meter to report either.
// ---------------------------------------------------------------------------

#define AT778UV_COMMON_CAPS                                                   \
    .version            =  BACKEND_VER ".0",                                  \
    .copyright          =  "LGPL",                                            \
    .rig_type           =  RIG_TYPE_TRANSCEIVER,                              \
    .ptt_type           =  RIG_PTT_NONE,                                      \
    .dcd_type           =  RIG_DCD_RIG,                                       \
    .port_type          =  RIG_PORT_SERIAL,                                   \
    .serial_rate_min    =  9600,                                              \
    .serial_rate_max    =  9600,                                              \
    .serial_data_bits   =  8,                                                 \
    .serial_stop_bits   =  1,                                                 \
    .serial_parity      =  RIG_PARITY_NONE,                                   \
    .serial_handshake   =  RIG_HANDSHAKE_NONE,                                \
    .write_delay        =  0,                                                 \
    .post_write_delay   =  0,                                                 \
    .timeout            =  1000,                                              \
    .retry              =  3,                                                 \
    .has_get_level      =  AT778UV_LEVELS,                                    \
    .has_set_level      =  AT778UV_LEVELS,                                    \
    .has_get_func       =  (RIG_FUNC_TONE | RIG_FUNC_TSQL),                   \
    .has_set_func       =  AT778UV_FUNCS,                                     \
    .level_gran         =                                                     \
    {                                                                         \
        [LVL_AF]      = { .min = { .f = 0 }, .max = { .f = 1.0 },              \
                          .step = { .f = 1.0f / AT778UV_VOLUME_MAX } },        \
        [LVL_SQL]     = { .min = { .f = 0 }, .max = { .f = 1.0 },              \
                          .step = { .f = 1.0f / AT778UV_SQUELCH_MAX } },       \
        [LVL_RFPOWER] = { .min = { .f = 0 }, .max = { .f = 1.0 },              \
                          .step = { .f = 0.5f } },                             \
    },                                                                        \
    .ctcss_list         =  at778uv_ctcss_list,                                \
    .dcs_list           =  common_dcs_list,                                   \
    .targetable_vfo     =  RIG_TARGETABLE_NONE,                               \
    .transceive         =  RIG_TRN_OFF,                                       \
    .filters            =                                                     \
    {                                                                         \
        { RIG_MODE_FM,  kHz(25) },                                            \
        { RIG_MODE_FM,  kHz(20) },                                            \
        { RIG_MODE_FMN, kHz(12.5) },                                          \
        RIG_FLT_END,                                                          \
    },                                                                        \
    .rx_range_list1 =                                                         \
    {                                                                         \
        { MHz(136), MHz(174), AT778UV_MODES, -1, -1, AT778UV_VFOS },          \
        { MHz(400), MHz(490), AT778UV_MODES, -1, -1, AT778UV_VFOS },          \
        RIG_FRNG_END,                                                         \
    },                                                                        \
    .tx_range_list1 =                                                         \
    {                                                                         \
        { MHz(144), MHz(148), AT778UV_MODES, W(5), W(25), AT778UV_VFOS },     \
        { MHz(430), MHz(440), AT778UV_MODES, W(5), W(20), AT778UV_VFOS },     \
        RIG_FRNG_END,                                                         \
    },                                                                        \
    .rx_range_list2 =                                                         \
    {                                                                         \
        { MHz(136), MHz(174), AT778UV_MODES, -1, -1, AT778UV_VFOS },          \
        { MHz(400), MHz(490), AT778UV_MODES, -1, -1, AT778UV_VFOS },          \
        RIG_FRNG_END,                                                         \
    },                                                                        \
    .tx_range_list2 =                                                         \
    {                                                                         \
        { MHz(144), MHz(148), AT778UV_MODES, W(5), W(25), AT778UV_VFOS },     \
        { MHz(430), MHz(440), AT778UV_MODES, W(5), W(20), AT778UV_VFOS },     \
        RIG_FRNG_END,                                                         \
    },                                                                        \
    .tuning_steps =                                                           \
    {                                                                         \
        { AT778UV_MODES, Hz(2500) },                                          \
        { AT778UV_MODES, Hz(5000) },                                          \
        { AT778UV_MODES, Hz(6250) },                                          \
        { AT778UV_MODES, Hz(10000) },                                         \
        { AT778UV_MODES, Hz(12500) },                                         \
        { AT778UV_MODES, Hz(25000) },                                         \
        RIG_TS_END,                                                           \
    },                                                                        \
    .rig_init           =  at778uv_init,                                      \
    .rig_cleanup        =  at778uv_cleanup,                                   \
    .rig_open           =  at778uv_open,                                      \
    .rig_close          =  at778uv_close,                                     \
    .get_info           =  at778uv_get_info,                                  \
    .set_freq           =  at778uv_set_freq,                                  \
    .get_freq           =  at778uv_get_freq,                                  \
    .set_vfo            =  at778uv_set_vfo,                                   \
    .get_vfo            =  at778uv_get_vfo,                                   \
    .get_dcd            =  at778uv_get_dcd,                                   \
    .set_mode           =  at778uv_set_mode,                                  \
    .get_mode           =  at778uv_get_mode,                                  \
    .set_level          =  at778uv_set_level,                                 \
    .get_level          =  at778uv_get_level,                                 \
    .set_func           =  at778uv_set_func,                                  \
    .get_func           =  at778uv_get_func,                                  \
    .set_ctcss_tone     =  at778uv_set_ctcss_tone,                            \
    .get_ctcss_tone     =  at778uv_get_ctcss_tone,                            \
    .set_ctcss_sql      =  at778uv_set_ctcss_sql,                             \
    .get_ctcss_sql      =  at778uv_get_ctcss_sql,                             \
    .set_dcs_code       =  at778uv_set_dcs_code,                              \
    .get_dcs_code       =  at778uv_get_dcs_code,                              \
    .set_dcs_sql        =  at778uv_set_dcs_sql,                               \
    .get_dcs_sql        =  at778uv_get_dcs_sql,                               \
    .set_rptr_offs      =  at778uv_set_rptr_offs,                             \
    .get_rptr_offs      =  at778uv_get_rptr_offs,                             \
    .get_mem            =  at778uv_get_mem,                                   \
    .chan_list          =                                                     \
    {                                                                         \
        /* The channel numbering get_mem reports. No attributes are claimed:  \
           get_channel and set_channel are not implemented, so nothing can    \
           actually retrieve a channel's frequency, tones or name, and        \
           advertising them would be an over-claim. */                        \
        { 1, AT778UV_CHANNELS, RIG_MTYPE_MEM, { 0 } },                        \
        RIG_CHAN_END,                                                         \
    },                                                                        \
    .hamlib_check_rig_caps = HAMLIB_CHECK_RIG_CAPS

struct rig_caps at778uv_caps =
{
    RIG_MODEL(RIG_MODEL_AT778UV),
    .model_name         =  "AT-778UV",
    .mfg_name           =  "AnyTone",
    .status             =  RIG_STATUS_BETA,
    AT778UV_COMMON_CAPS
};

//
// Same design, same firmware dispatcher, different badge. Reported by the
// identify command as RT95. Not tested on hardware by the author.
//
struct rig_caps rt95_caps =
{
    RIG_MODEL(RIG_MODEL_RT95),
    .model_name         =  "RT95",
    .mfg_name           =  "Retevis",
    .status             =  RIG_STATUS_UNTESTED,
    AT778UV_COMMON_CAPS
};

// ---------------------------------------------------------------------------
//    END OF FILE
// ---------------------------------------------------------------------------
