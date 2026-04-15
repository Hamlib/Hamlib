/*
 * Hamlib Yaesu backend - FTX-1 Memory Operations Group
 * Copyright (c) 2025 by Terrell Deppe (KJ5HST)
 *
 * This file implements CAT commands for memory channel operations.
 *
 * FIRMWARE FORMAT NOTES (verified 2025-12-09):
 * All memory commands use DIFFERENT FORMATS than documented in the spec!
 *
 * MC (Memory Channel Select):
 *   Query: MC0 (MAIN VFO) or MC1 (SUB VFO)
 *   Response: MCNNNNNN (6-digit channel, e.g., MC000001)
 *   Set: MCNNNNNN (6-digit channel)
 *   Returns '?' if channel doesn't exist (not programmed)
 *
 * MR (Memory Read): 5-digit format
 *   Query: MR00001 (not MR001 or MR0001)
 *   Response: MR00001FFFFFFFFF+OOOOOPPMMxxxx
 *
 * MT (Memory Tag): 5-digit format, FULL READ/WRITE
 *   Read: MT00001 returns MT00001[12-char name, space padded]
 *   Set: MT00001NAMEHERE (12 chars, space padded)
 *
 * MZ (Memory Zone): 5-digit format, FULL READ/WRITE
 *   Read: MZ00001 returns MZ00001[10-digit zone data]
 *   Set: MZ00001NNNNNNNNNN (10-digit zone data)
 *
 * VM (VFO/Memory Mode):
 *   Mode codes DIFFER FROM SPEC: 00=VFO, 11=Memory (not 01!)
 *   Only VM000 set works; use SV to toggle to memory mode
 *
 * CH (Memory Channel Up/Down):
 *   CH0 = next memory channel, CH1 = previous memory channel
 *   Cycles through ALL channels across groups: PMG (00xxxx) → QMB (05xxxx)
 *   CH; CH00; CH01; etc. return '?' - only CH0 and CH1 work
 *   MC response reflects group: MCGGnnnn where GG=group (00-05), nnnn=channel
 *
 * Memory Channel Ranges:
 *   000001-000099 = Regular memory channels (6-digit for MC)
 *   000100-000117 = Special channels
 *   00001-00099 = Regular memory channels (5-digit for MR/MT/MZ)
 */

#include <stdlib.h>
#include <string.h>
#include <hamlib/rig.h>
#include "misc.h"
#include "yaesu.h"
#include "newcat.h"
#include "ftx1.h"

#define FTX1_MEM_MIN 1
#define FTX1_MEM_MAX 117
#define FTX1_MEM_REGULAR_MAX 99

/* Forward declaration - needed for ftx1_set_vfo_mem_mode */
static int ftx1_get_vfo_mem_mode_internal(RIG *rig, vfo_t *vfo);

/*
 * Set Memory Channel (MC)
 * FIRMWARE FORMAT: MCNNNNNN (6-digit channel number)
 *   Set: MC000001 for channel 1
 *   Returns '?' if channel doesn't exist (not programmed)
 *
 * Note: VFO parameter is ignored - FTX-1 uses single memory channel format
 */
int ftx1_set_mem(RIG *rig, vfo_t vfo, int ch)
{
    struct newcat_priv_data *priv = STATE(rig)->priv;

    (void)vfo;  /* VFO not used in MC command */

    if (ch < FTX1_MEM_MIN || ch > FTX1_MEM_MAX) {
        rig_debug(RIG_DEBUG_ERR, "%s: channel %d out of range %d-%d\n",
                  __func__, ch, FTX1_MEM_MIN, FTX1_MEM_MAX);
        return -RIG_EINVAL;
    }

    rig_debug(RIG_DEBUG_VERBOSE, "%s: ch=%d\n", __func__, ch);

    /* Format: MCNNNNNN (6-digit channel) */
    SNPRINTF(priv->cmd_str, sizeof(priv->cmd_str), "MC%06d;", ch);
    return newcat_set_cmd(rig);
}

/*
 * Get Memory Channel (MC)
 * FIRMWARE FORMAT: MC0 or MC1 query returns MCNNNNNN (6-digit channel)
 *   Query: MC0 (MAIN VFO) or MC1 (SUB VFO)
 *   Response: MC000001 for channel 1
 *
 * Note: The VFO number in query selects which VFO to read,
 *       but response format is the same (MC + 6-digit channel)
 */
int ftx1_get_mem(RIG *rig, vfo_t vfo, int *ch)
{
    int ret, channel;
    struct newcat_priv_data *priv = STATE(rig)->priv;
    int vfo_num;

    /* Determine VFO number: 0=MAIN, 1=SUB (with currVFO resolution) */
    vfo_num = ftx1_vfo_to_p1(rig, vfo);

    rig_debug(RIG_DEBUG_VERBOSE, "%s: vfo=%d\n", __func__, vfo_num);

    /* Format: MCx where x=VFO (0 or 1) */
    SNPRINTF(priv->cmd_str, sizeof(priv->cmd_str), "MC%d;", vfo_num);

    ret = newcat_get_cmd(rig);
    if (ret != RIG_OK) return ret;

    /* Response: MCNNNNNN (MC + 6-digit channel) */
    /* Skip "MC" (2 chars) and parse 6-digit channel */
    if (strlen(priv->ret_data) < 8) {
        rig_debug(RIG_DEBUG_ERR, "%s: response too short '%s'\n", __func__, priv->ret_data);
        return -RIG_EPROTO;
    }

    if (sscanf(priv->ret_data + 2, "%6d", &channel) != 1) {
        rig_debug(RIG_DEBUG_ERR, "%s: failed to parse '%s'\n", __func__, priv->ret_data);
        return -RIG_EPROTO;
    }

    *ch = channel;

    rig_debug(RIG_DEBUG_VERBOSE, "%s: ch=%d\n", __func__, *ch);

    return RIG_OK;
}

/*
 * Memory Write — VFO-B + BM workaround (issue #2034)
 *
 * The FTX-1 MW command is firmware-broken: the radio returns '?;' for
 * every MW write on every channel, regardless of parameters (verified
 * against fw v1.12). We therefore implement rig_set_channel by
 * programming VFO-B to the target state and committing it to the
 * target memory slot via the BM command:
 *
 *   1. MC<ch>;   Point the "last-selected channel" cursor at the
 *                target slot. Ignore '?' if the slot is currently
 *                unprogrammed — the cursor still updates for BM.
 *   2. VM000;    Force VFO mode in case MC entered memory mode.
 *   3. FB / MD1 / CT1 / CN10 / OS1 / CF1   Program VFO-B.
 *                CT/CN/OS only work in FM, so tone and shift are only
 *                emitted for FM-family channel modes. Clarifier (CF)
 *                is programmed for all modes.
 *   4. BM;       Copy VFO-B to the last-selected memory channel.
 *
 * VFO-B is used rather than VFO-A so the operator's primary RX VFO
 * is not disturbed. VFO-B's frequency and mode are snapshotted at
 * entry and best-effort restored on exit (including error paths).
 * VFO-B's clarifier and CT state are cleared during cleanup — any
 * caller that relied on those aspects of VFO-B's state will need to
 * reprogram them after rig_set_channel.
 *
 * Firmware limitations verified empirically on fw v1.12: the BM
 * command does NOT propagate two kinds of state to the memory slot,
 * even though the CAT commands to set them on VFO-B succeed:
 *
 *   - DCS: CT13 (DCS mode) + CN11<code> program VFO-B correctly, but
 *     the stored memory channel reads back with CTCSS mode OFF. DCS
 *     channels cannot be programmed via CAT and must be set up from
 *     the front panel. We still emit the CT/CN commands so that if
 *     firmware is fixed later, the code path works.
 *
 *   - XIT (TX clarifier flag): the clarifier offset frequency and
 *     the RX clarifier on/off flag both propagate correctly, but the
 *     TX CLAR on/off flag is dropped by BM and always reads back as
 *     OFF in the stored channel. We set it anyway for forward
 *     compatibility.
 */
int ftx1_set_channel(RIG *rig, vfo_t vfo, const channel_t *chan)
{
    struct newcat_priv_data *priv = STATE(rig)->priv;
    char saved_fb[32]  = {0};
    char saved_md1[16] = {0};
    char mode_char;
    int err;
    int is_fm_family;

    (void)vfo;

    rig_debug(RIG_DEBUG_VERBOSE, "%s: ch=%d freq=%.0f mode=%s\n", __func__,
              chan->channel_num, chan->freq, rig_strrmode(chan->mode));

    if (chan->channel_num < FTX1_MEM_MIN || chan->channel_num > FTX1_MEM_MAX)
    {
        rig_debug(RIG_DEBUG_ERR, "%s: channel %d out of range %d-%d\n",
                  __func__, chan->channel_num, FTX1_MEM_MIN, FTX1_MEM_MAX);
        return -RIG_EINVAL;
    }

    switch (chan->mode)
    {
    case RIG_MODE_LSB:    mode_char = '1'; break;
    case RIG_MODE_USB:    mode_char = '2'; break;
    case RIG_MODE_CW:     mode_char = '3'; break;  /* CW-U */
    case RIG_MODE_FM:     mode_char = '4'; break;
    case RIG_MODE_AM:     mode_char = '5'; break;
    case RIG_MODE_RTTYR:  mode_char = '6'; break;  /* RTTY-L */
    case RIG_MODE_CWR:    mode_char = '7'; break;  /* CW-L */
    case RIG_MODE_PKTLSB: mode_char = '8'; break;  /* DATA-L */
    case RIG_MODE_RTTY:   mode_char = '9'; break;  /* RTTY-U */
    case RIG_MODE_PKTFM:  mode_char = 'A'; break;  /* DATA-FM */
    case RIG_MODE_FMN:    mode_char = 'B'; break;  /* FM-N */
    case RIG_MODE_PKTUSB: mode_char = 'C'; break;  /* DATA-U */
    case RIG_MODE_AMN:    mode_char = 'D'; break;  /* AM-N */
    case RIG_MODE_PSK:    mode_char = 'E'; break;  /* PSK */
    default:
        rig_debug(RIG_DEBUG_ERR, "%s: unsupported mode %s\n",
                  __func__, rig_strrmode(chan->mode));
        return -RIG_EINVAL;
    }

    is_fm_family = (chan->mode == RIG_MODE_FM    || chan->mode == RIG_MODE_FMN
                 || chan->mode == RIG_MODE_PKTFM || chan->mode == RIG_MODE_PKTFMN);

    /* Snapshot VFO-B state for best-effort restore on exit. Responses
     * (FB%09d; and MD1%c;) are already valid set commands and can be
     * replayed verbatim. If the query fails, the saved_* buffer stays
     * empty and restore is skipped. */
    SNPRINTF(priv->cmd_str, sizeof(priv->cmd_str), "FB;");
    if (newcat_get_cmd(rig) == RIG_OK)
    {
        strncpy(saved_fb, priv->ret_data, sizeof(saved_fb) - 1);
    }
    SNPRINTF(priv->cmd_str, sizeof(priv->cmd_str), "MD1;");
    if (newcat_get_cmd(rig) == RIG_OK)
    {
        strncpy(saved_md1, priv->ret_data, sizeof(saved_md1) - 1);
    }

    /* 1. Point the MC cursor at the target slot. Returns '?;' when the
     *    slot is unprogrammed, but the cursor still updates for BM. */
    SNPRINTF(priv->cmd_str, sizeof(priv->cmd_str), "MC%06d;",
             chan->channel_num);
    (void)newcat_set_cmd(rig);

    /* 2. Force VFO mode. Must be the 6-char form — extra digits are
     *    rejected by firmware. */
    SNPRINTF(priv->cmd_str, sizeof(priv->cmd_str), "VM000;");
    (void)newcat_set_cmd(rig);

    /* 3a. Program VFO-B frequency. */
    SNPRINTF(priv->cmd_str, sizeof(priv->cmd_str), "FB%09.0f;", chan->freq);
    err = newcat_set_cmd(rig);
    if (err != RIG_OK) goto restore;

    /* 3b. Program VFO-B mode. */
    SNPRINTF(priv->cmd_str, sizeof(priv->cmd_str), "MD1%c;", mode_char);
    err = newcat_set_cmd(rig);
    if (err != RIG_OK) goto restore;

    /* 3c. FM-family channels: program CTCSS/DCS and repeater shift.
     *     CT, CN (P2=1), and OS commands only function in FM mode, so
     *     non-FM channels skip this block entirely. */
    if (is_fm_family)
    {
        int os_mode;

        if (chan->dcs_code != 0)
        {
            /* DCS: select CT mode 3, then program code via CN P2=1.
             * (The DC command is firmware-broken — use CN instead.) */
            SNPRINTF(priv->cmd_str, sizeof(priv->cmd_str), "CT13;");
            (void)newcat_set_cmd(rig);

            SNPRINTF(priv->cmd_str, sizeof(priv->cmd_str), "CN11%03u;",
                     chan->dcs_code);
            (void)newcat_set_cmd(rig);
        }
        else if (chan->ctcss_tone != 0)
        {
            /* CTCSS: TSQ when both encode and squelch tones are set,
             * else ENC only. CN P2=0 programs the tone index. */
            int ct_mode = (chan->ctcss_sql != 0)
                            ? FTX1_CTCSS_MODE_TSQ
                            : FTX1_CTCSS_MODE_ENC;
            int tone_num = ftx1_freq_to_tone_num(chan->ctcss_tone);

            SNPRINTF(priv->cmd_str, sizeof(priv->cmd_str),
                     "CT1%d;", ct_mode);
            (void)newcat_set_cmd(rig);

            if (tone_num >= 0)
            {
                SNPRINTF(priv->cmd_str, sizeof(priv->cmd_str),
                         "CN10%03d;", tone_num);
                (void)newcat_set_cmd(rig);
            }
        }
        else
        {
            /* Explicit CT OFF in case VFO-B was previously configured
             * with a tone. */
            SNPRINTF(priv->cmd_str, sizeof(priv->cmd_str), "CT10;");
            (void)newcat_set_cmd(rig);
        }

        switch (chan->rptr_shift)
        {
        case RIG_RPT_SHIFT_PLUS:  os_mode = 1; break;
        case RIG_RPT_SHIFT_MINUS: os_mode = 2; break;
        default:                  os_mode = 0; break;  /* simplex */
        }
        SNPRINTF(priv->cmd_str, sizeof(priv->cmd_str), "OS1%d;", os_mode);
        (void)newcat_set_cmd(rig);
    }

    /* 3d. Clarifier (RIT/XIT). The FTX-1 uses one shared offset with
     *     independent RX/TX on-off toggles via the 11-char CF command:
     *     CF P1 P2 P3 P4 P5P5P5P5 ; where P3=0 sets RX/TX on/off
     *     (P4: 0=RX OFF, 1=RX ON, 2=TX OFF, 3=TX ON) and P3=1 sets the
     *     offset frequency (P4=+/-, P5P5P5P5 = offset Hz).
     *
     *     If both rit and xit are set, rit's offset wins (matches the
     *     pre-#2034 behavior of the broken MW path). Note that the TX
     *     CLAR flag is silently dropped by BM (firmware limitation);
     *     we still emit it for forward compatibility. */
    if (chan->rit != 0 || chan->xit != 0)
    {
        int off;
        char dir;

        if (chan->rit != 0)
        {
            dir = (chan->rit >= 0) ? '+' : '-';
            off = (int)labs(chan->rit);
        }
        else
        {
            dir = (chan->xit >= 0) ? '+' : '-';
            off = (int)labs(chan->xit);
        }
        if (off > 9999) off = 9999;

        SNPRINTF(priv->cmd_str, sizeof(priv->cmd_str),
                 "CF100%d0000;", (chan->rit != 0) ? 1 : 0);
        (void)newcat_set_cmd(rig);

        SNPRINTF(priv->cmd_str, sizeof(priv->cmd_str),
                 "CF100%d0000;", (chan->xit != 0) ? 3 : 2);
        (void)newcat_set_cmd(rig);

        SNPRINTF(priv->cmd_str, sizeof(priv->cmd_str),
                 "CF101%c%04d;", dir, off);
        (void)newcat_set_cmd(rig);
    }
    else
    {
        /* Explicitly clear VFO-B clarifier state so it doesn't leak
         * from a previous write or the operator's prior VFO-B. */
        SNPRINTF(priv->cmd_str, sizeof(priv->cmd_str), "CF10000000;");
        (void)newcat_set_cmd(rig);
        SNPRINTF(priv->cmd_str, sizeof(priv->cmd_str), "CF10020000;");
        (void)newcat_set_cmd(rig);
    }

    /* 4. Commit VFO-B to the last-selected memory channel. */
    SNPRINTF(priv->cmd_str, sizeof(priv->cmd_str), "BM;");
    err = newcat_set_cmd(rig);

restore:
    /* Best-effort restore of VFO-B. Clarifier and CT state that we may
     * have touched are cleared unconditionally — we don't snapshot
     * them because CF has no reliable read form. FB and MD1 are
     * replayed from the entry snapshot. Restore failures are not
     * surfaced: the caller cares about the write's status, not the
     * side-effect cleanup. */
    SNPRINTF(priv->cmd_str, sizeof(priv->cmd_str), "CF10000000;");
    (void)newcat_set_cmd(rig);
    SNPRINTF(priv->cmd_str, sizeof(priv->cmd_str), "CF10020000;");
    (void)newcat_set_cmd(rig);
    if (is_fm_family)
    {
        SNPRINTF(priv->cmd_str, sizeof(priv->cmd_str), "CT10;");
        (void)newcat_set_cmd(rig);
    }
    if (saved_fb[0] != '\0')
    {
        SNPRINTF(priv->cmd_str, sizeof(priv->cmd_str), "%s", saved_fb);
        (void)newcat_set_cmd(rig);
    }
    if (saved_md1[0] != '\0')
    {
        SNPRINTF(priv->cmd_str, sizeof(priv->cmd_str), "%s", saved_md1);
        (void)newcat_set_cmd(rig);
    }

    return err;
}

/*
 * ftx1_read_channel_tone_state — read a channel's CTCSS tone / DCS code.
 *
 * The MR response carries only the tone MODE (P8), not the actual tone
 * number or DCS code. To retrieve those we have to select the memory
 * channel via MC, query CN (which reflects the stored channel state in
 * memory mode), then restore the caller's prior VM/MC position.
 *
 * Intrusiveness note: this briefly switches the radio to memory mode
 * on the target channel. The visible side effect is a momentary front-
 * panel display change. State is restored at the end — if the caller
 * was previously in VFO mode we return via VM000, if they were already
 * in memory mode on another channel we MC back to that channel.
 *
 * mr_p8 is the raw stored P8 byte from MR, encoded per the firmware's
 * MW/MR swap (different from CT encoding):
 *   1 = TSQ (tone squelch — TX+RX CTCSS tone)
 *   2 = ENC (TX CTCSS tone only)
 *   3 = DCS
 *
 * Returns void: best-effort. On any CAT failure the tone fields are
 * simply left at whatever the caller passed in, and the restore step
 * still runs.
 */
static void ftx1_read_channel_tone_state(RIG *rig, int ch, int mr_p8,
                                         channel_t *chan)
{
    struct newcat_priv_data *priv = STATE(rig)->priv;
    char saved_vm[16] = {0};
    char saved_mc[16] = {0};
    int p1p2, val;

    /* Snapshot current VM and MC so we can restore them. */
    SNPRINTF(priv->cmd_str, sizeof(priv->cmd_str), "VM0;");
    if (newcat_get_cmd(rig) == RIG_OK)
    {
        strncpy(saved_vm, priv->ret_data, sizeof(saved_vm) - 1);
    }
    SNPRINTF(priv->cmd_str, sizeof(priv->cmd_str), "MC0;");
    if (newcat_get_cmd(rig) == RIG_OK)
    {
        strncpy(saved_mc, priv->ret_data, sizeof(saved_mc) - 1);
    }

    /* Select the target memory channel. */
    SNPRINTF(priv->cmd_str, sizeof(priv->cmd_str), "MC%06d;", ch);
    if (newcat_set_cmd(rig) != RIG_OK)
    {
        goto restore;
    }

    if (mr_p8 == 3)
    {
        /* DCS: query main-side DCS code via CN01. */
        SNPRINTF(priv->cmd_str, sizeof(priv->cmd_str), "CN01;");
        if (newcat_get_cmd(rig) == RIG_OK
            && sscanf(priv->ret_data + 2, "%2d%3d", &p1p2, &val) == 2)
        {
            chan->dcs_code = (tone_t)val;
        }
    }
    else
    {
        /* CTCSS: query main-side tone index via CN00. */
        SNPRINTF(priv->cmd_str, sizeof(priv->cmd_str), "CN00;");
        if (newcat_get_cmd(rig) == RIG_OK
            && sscanf(priv->ret_data + 2, "%2d%3d", &p1p2, &val) == 2)
        {
            tone_t tone = (tone_t)ftx1_tone_num_to_freq(val);
            chan->ctcss_tone = tone;
            /* MW/MR P8=1 is TSQ (encode and decode tones both active);
             * P8=2 is ENC only. */
            if (mr_p8 == 1)
            {
                chan->ctcss_sql = tone;
            }
        }
    }

restore:
    /* Return to the caller's prior position. Memory mode on some other
     * channel is restored via MC; otherwise fall back to VFO mode. */
    if (strstr(saved_vm, "VM011;") != NULL && saved_mc[0] != '\0')
    {
        SNPRINTF(priv->cmd_str, sizeof(priv->cmd_str), "%s", saved_mc);
        (void)newcat_set_cmd(rig);
    }
    else
    {
        SNPRINTF(priv->cmd_str, sizeof(priv->cmd_str), "VM000;");
        (void)newcat_set_cmd(rig);
    }
}

/*
 * Memory Read (MR) - read memory channel data
 * FIRMWARE FORMAT: MR P1P2P2P2P2 (5-digit: bank + 4-digit channel)
 *   Query: MR00001 for channel 1
 *   Response format (30+ chars after MR):
 *     Position  Field          Description
 *     0-4       P1             5-digit channel number
 *     5-13      P2             9-digit frequency (Hz)
 *     14        P3 sign        Clarifier direction (+/-)
 *     15-18     P3 offset      4-digit clarifier offset
 *     19        P4             RX CLAR on/off (0/1)
 *     20        P5             TX CLAR on/off (0/1)
 *     21        P6             Mode code (1-E)
 *     22        P7             VFO/Memory (0/1)
 *     23        P8             CTCSS mode (0-3, MW/MR swap encoding)
 *     24-25     P9             Reserved "00"
 *     26        P10            Shift (0=simplex, 1=+, 2=-)
 *   Returns '?' if channel doesn't exist (not programmed)
 *
 * P8 does NOT carry the actual tone number or DCS code — those have
 * to be fetched separately via CN, which requires briefly selecting
 * the channel via MC. See ftx1_read_channel_tone_state().
 */
int ftx1_get_channel(RIG *rig, vfo_t vfo, channel_t *chan, int read_only)
{
    int ret;
    struct newcat_priv_data *priv = STATE(rig)->priv;
    int ch;
    const char *p;
    char *endptr;
    char freq_str[10];
    char offset_str[5];
    int clar_offset;
    char clar_sign;
    int rx_clar, tx_clar;
    char mode_char;
    int ctcss_mode;
    int shift_mode;

    (void)vfo;
    (void)read_only;

    ch = chan->channel_num;

    rig_debug(RIG_DEBUG_VERBOSE, "%s: ch=%d\n", __func__, ch);

    if (ch < FTX1_MEM_MIN || ch > FTX1_MEM_MAX) {
        rig_debug(RIG_DEBUG_ERR, "%s: channel %d out of range %d-%d\n",
                  __func__, ch, FTX1_MEM_MIN, FTX1_MEM_MAX);
        return -RIG_EINVAL;
    }

    /* Format: MR P1P2P2P2P2 (bank 0 + 4-digit channel) */
    SNPRINTF(priv->cmd_str, sizeof(priv->cmd_str), "MR%05d;", ch);

    ret = newcat_get_cmd(rig);
    if (ret != RIG_OK) return ret;

    /* Response: MR + 5 (channel) + 9 (freq) + 1 (sign) + 4 (offset) +
     *           1 (rx_clar) + 1 (tx_clar) + 1 (mode) + 1 (vfo/mem) +
     *           1 (ctcss) + 2 (reserved) + 1 (shift) = 29 chars minimum */
    if (strlen(priv->ret_data) < 29) {
        rig_debug(RIG_DEBUG_ERR, "%s: response too short '%s' (len=%zu)\n",
                  __func__, priv->ret_data, strlen(priv->ret_data));
        return -RIG_EPROTO;
    }

    /* Point past "MR" prefix */
    p = priv->ret_data + 2;

    /* Skip 5-digit channel number, point to frequency */
    p += 5;

    /* Parse frequency (9 digits) using strtod for validation */
    strncpy(freq_str, p, 9);
    freq_str[9] = '\0';
    chan->freq = strtod(freq_str, &endptr);
    if (endptr == freq_str) {
        rig_debug(RIG_DEBUG_ERR, "%s: failed to parse frequency from '%s'\n",
                  __func__, freq_str);
        return -RIG_EPROTO;
    }
    p += 9;

    /* Parse clarifier: sign (1 char) + offset (4 digits) */
    clar_sign = *p++;
    strncpy(offset_str, p, 4);
    offset_str[4] = '\0';
    clar_offset = atoi(offset_str);
    if (clar_sign == '-') clar_offset = -clar_offset;
    p += 4;

    /* Parse RX/TX clarifier on/off */
    rx_clar = (*p++ == '1') ? 1 : 0;
    tx_clar = (*p++ == '1') ? 1 : 0;

    /* Set RIT/XIT based on clarifier state */
    if (rx_clar) {
        chan->rit = clar_offset;
    } else {
        chan->rit = 0;
    }
    if (tx_clar) {
        chan->xit = clar_offset;
    } else {
        chan->xit = 0;
    }

    /* Parse mode (1 char) */
    mode_char = *p++;
    switch (mode_char) {
    case '1': chan->mode = RIG_MODE_LSB; break;
    case '2': chan->mode = RIG_MODE_USB; break;
    case '3': chan->mode = RIG_MODE_CW; break;
    case '4': chan->mode = RIG_MODE_FM; break;
    case '5': chan->mode = RIG_MODE_AM; break;
    case '6': chan->mode = RIG_MODE_RTTYR; break;
    case '7': chan->mode = RIG_MODE_CWR; break;
    case '8': chan->mode = RIG_MODE_PKTLSB; break;
    case '9': chan->mode = RIG_MODE_RTTY; break;
    case 'A': chan->mode = RIG_MODE_PKTFM; break;
    case 'B': chan->mode = RIG_MODE_FMN; break;
    case 'C': chan->mode = RIG_MODE_PKTUSB; break;
    case 'D': chan->mode = RIG_MODE_AMN; break;
    case 'E': chan->mode = RIG_MODE_PSK; break;
    default:  chan->mode = RIG_MODE_USB; break;
    }

    /* Skip VFO/Memory indicator (1 char) */
    p++;

    /* Parse CTCSS mode (1 char). This is the MW/MR stored encoding,
     * which is swapped relative to CT:
     *   0 = OFF, 1 = TSQ, 2 = ENC, 3 = DCS
     * The actual tone number or DCS code is fetched below via the
     * ftx1_read_channel_tone_state helper — MR does not carry it. */
    ctcss_mode = *p++ - '0';

    /* Skip reserved "00" (2 chars) */
    p += 2;

    /* Parse shift mode (1 char) */
    shift_mode = *p - '0';
    switch (shift_mode) {
    case 1:  chan->rptr_shift = RIG_RPT_SHIFT_PLUS; break;
    case 2:  chan->rptr_shift = RIG_RPT_SHIFT_MINUS; break;
    default: chan->rptr_shift = RIG_RPT_SHIFT_NONE; break;
    }

    /* Set width to default (not stored in memory response) */
    chan->width = RIG_PASSBAND_NORMAL;

    /* If the channel has a tone, fetch the actual number or DCS code
     * via CN (intrusive — briefly selects the channel and restores). */
    if (ctcss_mode > 0 && ctcss_mode <= 3)
    {
        ftx1_read_channel_tone_state(rig, ch, ctcss_mode, chan);
    }

    rig_debug(RIG_DEBUG_VERBOSE,
              "%s: ch=%d freq=%.0f mode=%s rit=%ld xit=%ld shift=%d\n",
              __func__, ch, chan->freq, rig_strrmode(chan->mode),
              chan->rit, chan->xit, chan->rptr_shift);

    return RIG_OK;
}

/* Memory to VFO-A (MA;) */
int ftx1_mem_to_vfo(RIG *rig)
{
    struct newcat_priv_data *priv = STATE(rig)->priv;

    rig_debug(RIG_DEBUG_VERBOSE, "%s\n", __func__);

    SNPRINTF(priv->cmd_str, sizeof(priv->cmd_str), "MA;");
    return newcat_set_cmd(rig);
}

/*
 * Get Memory Tag/Name (MT)
 * FIRMWARE FORMAT: MT P1P2P2P2P2 (5-digit: bank + 4-digit channel)
 *   Query: MT00001 for channel 1
 *   Response: MT00001[12-char name, space padded]
 *   Returns '?' if channel doesn't exist (not programmed)
 */
int ftx1_get_mem_name(RIG *rig, int ch, char *name, size_t name_len)
{
    int ret;
    struct newcat_priv_data *priv = STATE(rig)->priv;

    rig_debug(RIG_DEBUG_VERBOSE, "%s: ch=%d\n", __func__, ch);

    if (ch < FTX1_MEM_MIN || ch > FTX1_MEM_MAX) {
        return -RIG_EINVAL;
    }

    /* Format: MT P1P2P2P2P2 (bank 0 + 4-digit channel) */
    SNPRINTF(priv->cmd_str, sizeof(priv->cmd_str), "MT%05d;", ch);

    ret = newcat_get_cmd(rig);
    if (ret != RIG_OK) return ret;

    /* Response: MT00001[12-char name] */
    /* Skip "MT" + 5-digit channel (7 chars), then 12-char name */
    if (strlen(priv->ret_data) < 7) {
        rig_debug(RIG_DEBUG_ERR, "%s: response too short '%s'\n",
                  __func__, priv->ret_data);
        return -RIG_EPROTO;
    }

    /* Extract name (12 chars starting at position 7) */
    size_t copy_len = (name_len - 1 < 12) ? name_len - 1 : 12;
    strncpy(name, priv->ret_data + 7, copy_len);
    name[copy_len] = '\0';

    /* Trim trailing spaces */
    size_t len = strlen(name);
    while (len > 0 && name[len - 1] == ' ') {
        name[--len] = '\0';
    }

    return RIG_OK;
}

/*
 * Set Memory Tag/Name (MT)
 * FIRMWARE FORMAT: MT P1P2P2P2P2 NAME (5-digit channel + 12-char name)
 *   Set: MT00001NAMEHERE (12 chars, space padded)
 *   Returns '?' if channel doesn't exist (not programmed)
 */
int ftx1_set_mem_name(RIG *rig, int ch, const char *name)
{
    struct newcat_priv_data *priv = STATE(rig)->priv;
    char padded_name[13];

    rig_debug(RIG_DEBUG_VERBOSE, "%s: ch=%d name='%s'\n", __func__, ch, name);

    if (ch < FTX1_MEM_MIN || ch > FTX1_MEM_MAX) {
        return -RIG_EINVAL;
    }

    /* Pad name to 12 chars with spaces */
    memset(padded_name, ' ', 12);
    padded_name[12] = '\0';
    size_t name_len = strlen(name);
    if (name_len > 12) name_len = 12;
    memcpy(padded_name, name, name_len);

    /* Format: MT P1P2P2P2P2 NAME (bank 0 + 4-digit channel + 12-char name) */
    SNPRINTF(priv->cmd_str, sizeof(priv->cmd_str), "MT%05d%s;", ch, padded_name);

    return newcat_set_cmd(rig);
}

/*
 * QMB Store (QI;)
 * NOTE: QI command is ACCEPTED but NON-FUNCTIONAL in firmware v1.08+
 * The command is parsed (returns empty, not '?') but has no effect.
 * Kept for compatibility in case future firmware fixes this.
 */
int ftx1_quick_mem_store(RIG *rig)
{
    struct newcat_priv_data *priv = STATE(rig)->priv;

    rig_debug(RIG_DEBUG_VERBOSE, "%s (NOTE: QI is non-functional in firmware)\n", __func__);

    SNPRINTF(priv->cmd_str, sizeof(priv->cmd_str), "QI;");
    return newcat_set_cmd(rig);
}

/*
 * Quick Memory Recall (QR;)
 * NOTE: QR command is ACCEPTED but NON-FUNCTIONAL in firmware v1.08+
 * The command is parsed (returns empty, not '?') but has no effect.
 * CAT manual shows QR with no parameters, not QR P1 as originally coded.
 * Kept for compatibility in case future firmware fixes this.
 */
int ftx1_quick_mem_recall(RIG *rig, int slot)
{
    struct newcat_priv_data *priv = STATE(rig)->priv;

    rig_debug(RIG_DEBUG_VERBOSE, "%s: slot=%d (NOTE: QR is non-functional in firmware)\n", __func__, slot);

    /* CAT manual shows QR; with no parameters, but we accept slot for API compatibility */
    (void)slot;  /* unused - firmware ignores parameters anyway */

    SNPRINTF(priv->cmd_str, sizeof(priv->cmd_str), "QR;");
    return newcat_set_cmd(rig);
}

/*
 * Set VFO/Memory mode (VM)
 * FIRMWARE FORMAT: VMxPP where x=VFO (0=MAIN, 1=SUB), PP=mode
 *   Mode codes DIFFER FROM SPEC: 00=VFO, 11=Memory (not 01!)
 *   IMPORTANT: Only VM000 (VFO mode) set works!
 *   To switch to memory mode, use SV command to toggle
 */
int ftx1_set_vfo_mem_mode(RIG *rig, vfo_t vfo)
{
    struct newcat_priv_data *priv = STATE(rig)->priv;

    rig_debug(RIG_DEBUG_VERBOSE, "%s: vfo=%s\n", __func__, rig_strvfo(vfo));

    if (vfo == RIG_VFO_MEM) {
        /*
         * Memory mode requested - use SV toggle
         * First check current mode, then toggle if needed
         */
        int ret;
        vfo_t current_vfo;

        ret = ftx1_get_vfo_mem_mode_internal(rig, &current_vfo);
        if (ret != RIG_OK) return ret;

        if (current_vfo != RIG_VFO_MEM) {
            /* Toggle to memory mode using SV */
            SNPRINTF(priv->cmd_str, sizeof(priv->cmd_str), "SV;");
            return newcat_set_cmd(rig);
        }
        return RIG_OK;  /* Already in memory mode */
    } else {
        /* VFO mode requested - VM000 works */
        SNPRINTF(priv->cmd_str, sizeof(priv->cmd_str), "VM000;");
        return newcat_set_cmd(rig);
    }
}

/*
 * Get VFO/Memory mode (VM) - internal implementation
 * FIRMWARE FORMAT: VMx (x=VFO) returns VMxPP
 *   Mode codes DIFFER FROM SPEC: 00=VFO, 11=Memory (not 01!)
 */
static int ftx1_get_vfo_mem_mode_internal(RIG *rig, vfo_t *vfo)
{
    int ret, mode;
    struct newcat_priv_data *priv = STATE(rig)->priv;

    rig_debug(RIG_DEBUG_VERBOSE, "%s\n", __func__);

    /* Query MAIN VFO mode: VM0 returns VM0PP */
    SNPRINTF(priv->cmd_str, sizeof(priv->cmd_str), "VM0;");

    ret = newcat_get_cmd(rig);
    if (ret != RIG_OK) return ret;

    /* Response: VM0PP where PP is 2-digit mode code */
    if (strlen(priv->ret_data) < 5) {
        rig_debug(RIG_DEBUG_ERR, "%s: response too short '%s'\n",
                  __func__, priv->ret_data);
        return -RIG_EPROTO;
    }

    /* Parse 2-digit mode code at position 3 */
    if (sscanf(priv->ret_data + 3, "%2d", &mode) != 1) {
        rig_debug(RIG_DEBUG_ERR, "%s: failed to parse '%s'\n",
                  __func__, priv->ret_data);
        return -RIG_EPROTO;
    }

    /* Mode codes: 00=VFO, 11=Memory (firmware differs from spec) */
    *vfo = (mode == 11) ? RIG_VFO_MEM : RIG_VFO_CURR;

    rig_debug(RIG_DEBUG_VERBOSE, "%s: mode=%d vfo=%s\n", __func__, mode,
              rig_strvfo(*vfo));

    return RIG_OK;
}

/* Public wrapper for ftx1_get_vfo_mem_mode */
int ftx1_get_vfo_mem_mode(RIG *rig, vfo_t *vfo)
{
    return ftx1_get_vfo_mem_mode_internal(rig, vfo);
}

/*
 * ftx1_vfo_a_to_mem - Store VFO-A to Memory (AM;)
 * CAT command: AM; (set-only per spec)
 */
int ftx1_vfo_a_to_mem(RIG *rig)
{
    struct newcat_priv_data *priv = STATE(rig)->priv;

    rig_debug(RIG_DEBUG_VERBOSE, "%s\n", __func__);

    SNPRINTF(priv->cmd_str, sizeof(priv->cmd_str), "AM;");
    return newcat_set_cmd(rig);
}

/*
 * ftx1_vfo_b_to_mem - Store VFO-B to Memory (BM;)
 * CAT command: BM; (set-only per spec)
 */
int ftx1_vfo_b_to_mem(RIG *rig)
{
    struct newcat_priv_data *priv = STATE(rig)->priv;

    rig_debug(RIG_DEBUG_VERBOSE, "%s\n", __func__);

    SNPRINTF(priv->cmd_str, sizeof(priv->cmd_str), "BM;");
    return newcat_set_cmd(rig);
}

/*
 * ftx1_mem_to_vfo_b - Memory to VFO-B (MB;)
 * CAT command: MB; (set-only)
 */
int ftx1_mem_to_vfo_b(RIG *rig)
{
    struct newcat_priv_data *priv = STATE(rig)->priv;

    rig_debug(RIG_DEBUG_VERBOSE, "%s\n", __func__);

    SNPRINTF(priv->cmd_str, sizeof(priv->cmd_str), "MB;");
    return newcat_set_cmd(rig);
}

/*
 * Set Memory Zone data (MZ)
 * FIRMWARE FORMAT: MZ P1P2P2P2P2 DATA (5-digit channel + 10-digit zone data)
 *   Set: MZ00001NNNNNNNNNN (10-digit zone data)
 *   Returns '?' if channel doesn't exist (not programmed)
 *
 * Note: Actual zone data format is firmware-specific (10 digits).
 * This function takes raw zone data string for maximum flexibility.
 */
int ftx1_set_mem_zone(RIG *rig, int channel, const char *zone_data)
{
    struct newcat_priv_data *priv = STATE(rig)->priv;
    char padded_data[11];

    rig_debug(RIG_DEBUG_VERBOSE, "%s: ch=%d data='%s'\n", __func__,
              channel, zone_data);

    if (channel < FTX1_MEM_MIN || channel > FTX1_MEM_MAX) {
        return -RIG_EINVAL;
    }

    /* Pad/truncate zone data to 10 chars */
    memset(padded_data, '0', 10);
    padded_data[10] = '\0';
    size_t data_len = strlen(zone_data);
    if (data_len > 10) data_len = 10;
    memcpy(padded_data, zone_data, data_len);

    /* Format: MZ P1P2P2P2P2 DATA (bank 0 + 4-digit channel + 10-digit data) */
    SNPRINTF(priv->cmd_str, sizeof(priv->cmd_str), "MZ%05d%s;",
             channel, padded_data);

    return newcat_set_cmd(rig);
}

/*
 * Get Memory Zone data (MZ)
 * FIRMWARE FORMAT: MZ P1P2P2P2P2 (5-digit: bank + 4-digit channel)
 *   Query: MZ00001 for channel 1
 *   Response: MZ00001[10-digit zone data]
 *   Returns '?' if channel doesn't exist (not programmed)
 */
int ftx1_get_mem_zone(RIG *rig, int channel, char *zone_data, size_t data_len)
{
    struct newcat_priv_data *priv = STATE(rig)->priv;
    int ret;

    rig_debug(RIG_DEBUG_VERBOSE, "%s: ch=%d\n", __func__, channel);

    if (channel < FTX1_MEM_MIN || channel > FTX1_MEM_MAX) {
        return -RIG_EINVAL;
    }

    /* Format: MZ P1P2P2P2P2 (bank 0 + 4-digit channel) */
    SNPRINTF(priv->cmd_str, sizeof(priv->cmd_str), "MZ%05d;", channel);

    ret = newcat_get_cmd(rig);
    if (ret != RIG_OK) return ret;

    /* Response: MZ00001[10-digit zone data] */
    /* Skip "MZ" + 5-digit channel (7 chars), then 10-digit data */
    if (strlen(priv->ret_data) < 17) {
        rig_debug(RIG_DEBUG_ERR, "%s: response too short '%s'\n",
                  __func__, priv->ret_data);
        return -RIG_EPROTO;
    }

    /* Extract zone data (10 chars starting at position 7) */
    size_t copy_len = (data_len - 1 < 10) ? data_len - 1 : 10;
    strncpy(zone_data, priv->ret_data + 7, copy_len);
    zone_data[copy_len] = '\0';

    rig_debug(RIG_DEBUG_VERBOSE, "%s: zone_data='%s'\n", __func__, zone_data);

    return RIG_OK;
}

/*
 * ftx1_mem_ch_up - Select next memory channel (CH0)
 * CAT command: CH0; (set-only)
 * Cycles through ALL memory channels across groups:
 *   PMG ch1 → ch2 → ... → QMB ch1 → ch2 → ... → PMG ch1 (wraps)
 * Display shows "M-ALL X-NN" where X=group, NN=channel
 */
int ftx1_mem_ch_up(RIG *rig)
{
    struct newcat_priv_data *priv = STATE(rig)->priv;

    rig_debug(RIG_DEBUG_VERBOSE, "%s\n", __func__);

    SNPRINTF(priv->cmd_str, sizeof(priv->cmd_str), "CH0;");
    return newcat_set_cmd(rig);
}

/*
 * ftx1_mem_ch_down - Select previous memory channel (CH1)
 * CAT command: CH1; (set-only)
 * Cycles through ALL memory channels in reverse order
 */
int ftx1_mem_ch_down(RIG *rig)
{
    struct newcat_priv_data *priv = STATE(rig)->priv;

    rig_debug(RIG_DEBUG_VERBOSE, "%s\n", __func__);

    SNPRINTF(priv->cmd_str, sizeof(priv->cmd_str), "CH1;");
    return newcat_set_cmd(rig);
}

/*
 * ftx1_get_mem_group - Get current memory group from MC response
 * Returns group number (0-5 observed) extracted from MCGGNNNN format
 * Group 00 = PMG (Primary Memory Group)
 * Group 05 = QMB (Quick Memory Bank)
 */
int ftx1_get_mem_group(RIG *rig, int *group)
{
    int ret, ch;
    struct newcat_priv_data *priv = STATE(rig)->priv;

    rig_debug(RIG_DEBUG_VERBOSE, "%s\n", __func__);

    /* Use MC0 to get current memory channel */
    SNPRINTF(priv->cmd_str, sizeof(priv->cmd_str), "MC0;");

    ret = newcat_get_cmd(rig);
    if (ret != RIG_OK) return ret;

    /* Response: MCGGNNNN where GG=group (00-05), NNNN=channel */
    if (strlen(priv->ret_data) < 8) {
        rig_debug(RIG_DEBUG_ERR, "%s: response too short '%s'\n",
                  __func__, priv->ret_data);
        return -RIG_EPROTO;
    }

    /* Parse group (2 digits at position 2) and channel (4 digits at position 4) */
    if (sscanf(priv->ret_data + 2, "%2d%4d", group, &ch) != 2) {
        rig_debug(RIG_DEBUG_ERR, "%s: failed to parse '%s'\n",
                  __func__, priv->ret_data);
        return -RIG_EPROTO;
    }

    rig_debug(RIG_DEBUG_VERBOSE, "%s: group=%d channel=%d\n", __func__, *group, ch);

    return RIG_OK;
}
