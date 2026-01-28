/*
 * Hamlib Yaesu backend - FTX-1
 * Copyright (c) 2025 by Terrell Deppe (KJ5HST)
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA
 *
 * FIRMWARE NOTES (v1.08+):
 * - RIT/XIT (Clarifier): Uses CF command (spec page 8).
 *   P3=0: CLAR Setting (P4=RX on/off, P5=TX on/off)
 *   P3=1: CLAR Frequency (P4=+/-, P5-P8=0000-9999 Hz)
 * - ZI (Zero In) only works in CW mode (MD03 or MD07)
 * - BS (Band Select) is set-only - no read/query capability
 * - GP (GP OUT) requires menu: [OPERATION SETTING] → [GENERAL] → [TUN/LIN PORT SELECT] = "GPO"
 *   Format: GP P1 P2 P3 P4 where each controls GP OUT A/B/C/D (0=LOW, 1=HIGH)
 * - MR/MT/MZ (Memory) use 5-digit format, not 4-digit as documented
 *   Example: MR00001 (not MR0001) for channel 1
 * - MT (Memory Tag) is read/write: MT00001NAME sets 12-char name
 * - MZ (Memory Zone) is read/write: MZ00001DATA sets 10-digit zone data
 * - MC (Memory Channel) uses different format than documented:
 *   Read: MC0 (MAIN) or MC1 (SUB) returns MCNNNNNN (6-digit channel)
 *   Set: MCNNNNNN (6-digit channel, no VFO prefix)
 *   Returns '?' if channel doesn't exist (not programmed)
 * - CH (Memory Channel Up/Down) works with CH0/CH1 only:
 *   CH0 = next channel, CH1 = previous channel (cycles through all groups)
 *   Cycles: PMG (00xxxx) → QMB (05xxxx) → PMG (wraps around)
 *   CH; CH00; CH01; etc. return '?' - only CH0 and CH1 work
 * - VM mode codes differ from spec: 00=VFO, 11=Memory (not 01 as documented)
 *   Only VM000 set works; use SV command to toggle to memory mode
 */

#include <stdlib.h>
#include <hamlib/rig.h>
#include "serial.h"
#include "misc.h"
#include "sprintflst.h"
#include "yaesu.h"
#include "newcat.h"
#include "ftx1.h"
#include "ftx1_menu.h"

/* Private caps for newcat framework */
static const struct newcat_priv_caps ftx1_priv_caps = {
    .roofing_filter_count = 0,
};

/* Extern declarations for group-specific functions (add more as groups are implemented) */
extern int ftx1_set_vfo(RIG *rig, vfo_t vfo);
extern int ftx1_get_vfo(RIG *rig, vfo_t *vfo);
extern int ftx1_set_split_vfo(RIG *rig, vfo_t vfo, split_t split, vfo_t tx_vfo);
extern int ftx1_get_split_vfo(RIG *rig, vfo_t vfo, split_t *split, vfo_t *tx_vfo);
/* Additional externs for group 4 */
extern int ftx1_set_func(RIG *rig, vfo_t vfo, setting_t func, int status);
extern int ftx1_get_func(RIG *rig, vfo_t vfo, setting_t func, int *status);
extern int ftx1_set_level(RIG *rig, vfo_t vfo, setting_t level, value_t val);
extern int ftx1_get_level(RIG *rig, vfo_t vfo, setting_t level, value_t *val);
/* Additional externs for group 6 */
extern int ftx1_set_preamp_helper(RIG *rig, vfo_t vfo, value_t val);
extern int ftx1_get_preamp_helper(RIG *rig, vfo_t vfo, value_t *val);
extern int ftx1_set_att_helper(RIG *rig, vfo_t vfo, value_t val);
extern int ftx1_get_att_helper(RIG *rig, vfo_t vfo, value_t *val);

/* Wrappers from ftx1_func.c for rig caps */
extern int ftx1_set_ptt_func(RIG *rig, vfo_t vfo, ptt_t ptt);
extern int ftx1_get_ptt_func(RIG *rig, vfo_t vfo, ptt_t *ptt);
extern int ftx1_set_powerstat_func(RIG *rig, powerstat_t status);
extern int ftx1_get_powerstat_func(RIG *rig, powerstat_t *status);
extern int ftx1_set_ctcss_tone_func(RIG *rig, vfo_t vfo, tone_t tone);
extern int ftx1_get_ctcss_tone_func(RIG *rig, vfo_t vfo, tone_t *tone);
extern int ftx1_set_ctcss_sql_func(RIG *rig, vfo_t vfo, tone_t tone);
extern int ftx1_get_ctcss_sql_func(RIG *rig, vfo_t vfo, tone_t *tone);
extern int ftx1_set_dcs_code_func(RIG *rig, vfo_t vfo, tone_t code);
extern int ftx1_get_dcs_code_func(RIG *rig, vfo_t vfo, tone_t *code);
extern int ftx1_set_dcs_sql_func(RIG *rig, vfo_t vfo, tone_t code);
extern int ftx1_get_dcs_sql_func(RIG *rig, vfo_t vfo, tone_t *code);
extern int ftx1_send_morse_func(RIG *rig, vfo_t vfo, const char *msg);
extern int ftx1_stop_morse_func(RIG *rig, vfo_t vfo);
extern int ftx1_wait_morse_func(RIG *rig, vfo_t vfo);
extern int ftx1_set_trn_func(RIG *rig, int trn);
extern int ftx1_get_trn_func(RIG *rig, int *trn);
extern int ftx1_get_dcd(RIG *rig, vfo_t vfo, dcd_t *dcd);

/* Externs from ftx1_mem.c */
extern int ftx1_set_mem(RIG *rig, vfo_t vfo, int ch);
extern int ftx1_get_mem(RIG *rig, vfo_t vfo, int *ch);
extern int ftx1_set_channel(RIG *rig, vfo_t vfo, const channel_t *chan);
extern int ftx1_get_channel(RIG *rig, vfo_t vfo, channel_t *chan, int read_only);

/* Externs from ftx1_scan.c */
extern int ftx1_set_scan(RIG *rig, vfo_t vfo, scan_t scan, int ch);
extern int ftx1_get_scan(RIG *rig, vfo_t vfo, scan_t *scan, int *ch);
extern int ftx1_set_ts(RIG *rig, vfo_t vfo, shortfreq_t ts);
extern int ftx1_get_ts(RIG *rig, vfo_t vfo, shortfreq_t *ts);

/*
 * Externs from ftx1_clarifier.c
 * FTX-1 uses CF command for RX/TX clarifier (maps to Hamlib RIT/XIT)
 */
extern int ftx1_get_rx_clar(RIG *rig, vfo_t vfo, shortfreq_t *offset);
extern int ftx1_set_rx_clar(RIG *rig, vfo_t vfo, shortfreq_t offset);
extern int ftx1_get_tx_clar(RIG *rig, vfo_t vfo, shortfreq_t *offset);
extern int ftx1_set_tx_clar(RIG *rig, vfo_t vfo, shortfreq_t offset);

/* Externs from ftx1_freq.c */
extern int ftx1_set_freq(RIG *rig, vfo_t vfo, freq_t freq);
extern int ftx1_get_freq(RIG *rig, vfo_t vfo, freq_t *freq);
extern int ftx1_set_rptr_shift(RIG *rig, vfo_t vfo, rptr_shift_t shift);
extern int ftx1_get_rptr_shift(RIG *rig, vfo_t vfo, rptr_shift_t *shift);
extern int ftx1_set_rptr_offs(RIG *rig, vfo_t vfo, shortfreq_t offs);
extern int ftx1_get_rptr_offs(RIG *rig, vfo_t vfo, shortfreq_t *offs);

/* Externs from ftx1_vfo.c */
extern int ftx1_vfo_op(RIG *rig, vfo_t vfo, vfo_op_t op);
extern int ftx1_set_dual_receive(RIG *rig, int dual);
extern int ftx1_get_dual_receive(RIG *rig, int *dual);
extern int ftx1_set_split_freq(RIG *rig, vfo_t vfo, freq_t tx_freq);
extern int ftx1_get_split_freq(RIG *rig, vfo_t vfo, freq_t *tx_freq);
extern int ftx1_set_split_mode(RIG *rig, vfo_t vfo, rmode_t tx_mode, pbwidth_t tx_width);
extern int ftx1_get_split_mode(RIG *rig, vfo_t vfo, rmode_t *tx_mode, pbwidth_t *tx_width);

/*
 * Extended parameters (extparms) for Hamlib API access to EX menu items.
 *
 * These parameters are exposed through Hamlib's ext_parm API (p/P commands).
 * Token values are defined in ftx1_menu.h using FTX1_TOKEN(group, section, item).
 *
 * Usage:
 *   rigctl -m 1051 p KEYER_TYPE       # Get keyer type
 *   rigctl -m 1051 P KEYER_TYPE 3     # Set to ELEKEY-B
 *
 * Implementation notes:
 *   - RIG_CONF_NUMERIC types use val->f (float), not val->i
 *   - Get/set functions in ftx1_menu.c: ftx1_get_ext_parm(), ftx1_set_ext_parm()
 *   - rig_caps.extparms points to this static array
 *
 * To add a new parameter:
 *   1. Define token in ftx1_menu.h: #define TOK_XXX FTX1_TOKEN(g, s, i)
 *   2. Add entry to ftx1_menu_table[] in ftx1_menu.c with range/flags
 *   3. Add confparams entry here with matching token
 *
 * Known issues:
 *   - EX030601 (DIAL_SSB_CW_STEP) causes radio hang - do not expose
 *   - EX040108 (DISP_LED_DIMMER) causes radio hang - do not expose
 */
static const struct confparams ftx1_ext_parms[] = {
    /*
     * NOTE: Signed parameters (Audio EQ, Contour Level) are NOT exposed here
     * because the FTX-1 locks up when queried with the signed format.
     * String parameters are also excluded (use direct EX commands).
     *
     * Excluded tokens that cause radio hangs:
     *   - TOK_DIAL_SSB_CW_STEP (EX030601)
     *   - TOK_DISP_LED_DIMMER (EX040108)
     */

    /* AM Settings */
    { TOK_AM_AGC_FAST, "AM_AGC_FAST", "AM_AGC_FAST", "AM_AGC_FAST (20-4000)", "20", RIG_CONF_NUMERIC, { .n = { 20, 4000, 1 } } },
    { TOK_AM_AGC_MID, "AM_AGC_MID", "AM_AGC_MID", "AM_AGC_MID (20-4000)", "20", RIG_CONF_NUMERIC, { .n = { 20, 4000, 1 } } },
    { TOK_AM_AGC_SLOW, "AM_AGC_SLOW", "AM_AGC_SLOW", "AM_AGC_SLOW (20-4000)", "20", RIG_CONF_NUMERIC, { .n = { 20, 4000, 1 } } },
    { TOK_AM_LCUT_FREQ, "AM_LCUT_FREQ", "AM_LCUT_FREQ", "AM_LCUT_FREQ (0-19)", "0", RIG_CONF_NUMERIC, { .n = { 0, 19, 1 } } },
    { TOK_AM_LCUT_SLOPE, "AM_LCUT_SLOPE", "AM_LCUT_SLOPE", "AM_LCUT_SLOPE (0-1)", "0", RIG_CONF_NUMERIC, { .n = { 0, 1, 1 } } },
    { TOK_AM_HCUT_FREQ, "AM_HCUT_FREQ", "AM_HCUT_FREQ", "AM_HCUT_FREQ (0-67)", "0", RIG_CONF_NUMERIC, { .n = { 0, 67, 1 } } },
    { TOK_AM_HCUT_SLOPE, "AM_HCUT_SLOPE", "AM_HCUT_SLOPE", "AM_HCUT_SLOPE (0-1)", "0", RIG_CONF_NUMERIC, { .n = { 0, 1, 1 } } },
    { TOK_AM_USB_OUT_LEVEL, "AM_USB_OUT_LEVEL", "AM_USB_OUT_LEVEL", "AM_USB_OUT_LEVEL (0-100)", "0", RIG_CONF_NUMERIC, { .n = { 0, 100, 1 } } },
    { TOK_AM_TX_BPF_SEL, "AM_TX_BPF_SEL", "AM_TX_BPF_SEL", "AM_TX_BPF_SEL (0-4)", "0", RIG_CONF_NUMERIC, { .n = { 0, 4, 1 } } },
    { TOK_AM_MOD_SOURCE, "AM_MOD_SOURCE", "AM_MOD_SOURCE", "AM_MOD_SOURCE (0-3)", "0", RIG_CONF_NUMERIC, { .n = { 0, 3, 1 } } },
    { TOK_AM_USB_MOD_GAIN, "AM_USB_MOD_GAIN", "AM_USB_MOD_GAIN", "AM_USB_MOD_GAIN (0-100)", "0", RIG_CONF_NUMERIC, { .n = { 0, 100, 1 } } },
    { TOK_AM_RPTT_SELECT, "AM_RPTT_SELECT", "AM_RPTT_SELECT", "AM_RPTT_SELECT (0-2)", "0", RIG_CONF_NUMERIC, { .n = { 0, 2, 1 } } },

    /* APRS Settings */
    { TOK_APRS_MODEM_SEL, "APRS_MODEM_SEL", "APRS_MODEM_SEL", "APRS_MODEM_SEL (0-3)", "0", RIG_CONF_NUMERIC, { .n = { 0, 3, 1 } } },
    { TOK_APRS_MODEM_TYPE, "APRS_MODEM_TYPE", "APRS_MODEM_TYPE", "APRS_MODEM_TYPE (0-1)", "0", RIG_CONF_NUMERIC, { .n = { 0, 1, 1 } } },
    { TOK_APRS_AF_MUTE, "APRS_AF_MUTE", "APRS_AF_MUTE", "APRS_AF_MUTE (0-1)", "0", RIG_CONF_NUMERIC, { .n = { 0, 1, 1 } } },
    { TOK_APRS_TX_DELAY, "APRS_TX_DELAY", "APRS_TX_DELAY", "APRS_TX_DELAY (0-6)", "0", RIG_CONF_NUMERIC, { .n = { 0, 6, 1 } } },
    { TOK_APRS_MY_SYMBOL, "APRS_MY_SYMBOL", "APRS_MY_SYMBOL", "APRS_MY_SYMBOL (0-3)", "0", RIG_CONF_NUMERIC, { .n = { 0, 3, 1 } } },
    { TOK_APRS_PATH_SEL, "APRS_PATH_SEL", "APRS_PATH_SEL", "APRS_PATH_SEL (0-2)", "0", RIG_CONF_NUMERIC, { .n = { 0, 2, 1 } } },

    /* BCN Settings */
    { TOK_BCN_TYPE, "BCN_TYPE", "BCN_TYPE", "BCN_TYPE (0-2)", "0", RIG_CONF_NUMERIC, { .n = { 0, 2, 1 } } },
    { TOK_BCN_INFO_AMBIG, "BCN_INFO_AMBIG", "BCN_INFO_AMBIG", "BCN_INFO_AMBIG (0-4)", "0", RIG_CONF_NUMERIC, { .n = { 0, 4, 1 } } },
    { TOK_BCN_SPEED_COURSE, "BCN_SPEED_COURSE", "BCN_SPEED_COURSE", "BCN_SPEED_COURSE (0-1)", "0", RIG_CONF_NUMERIC, { .n = { 0, 1, 1 } } },
    { TOK_BCN_ALTITUDE, "BCN_ALTITUDE", "BCN_ALTITUDE", "BCN_ALTITUDE (0-1)", "0", RIG_CONF_NUMERIC, { .n = { 0, 1, 1 } } },
    { TOK_BCN_POS_COMMENT, "BCN_POS_COMMENT", "BCN_POS_COMMENT", "BCN_POS_COMMENT (0-14)", "0", RIG_CONF_NUMERIC, { .n = { 0, 14, 1 } } },
    { TOK_BCN_EMERGENCY, "BCN_EMERGENCY", "BCN_EMERGENCY", "BCN_EMERGENCY (0-1)", "0", RIG_CONF_NUMERIC, { .n = { 0, 1, 1 } } },
    { TOK_BCN_INTERVAL, "BCN_INTERVAL", "BCN_INTERVAL", "BCN_INTERVAL (0-9)", "0", RIG_CONF_NUMERIC, { .n = { 0, 9, 1 } } },
    { TOK_BCN_PROPORTIONAL, "BCN_PROPORTIONAL", "BCN_PROPORTIONAL", "BCN_PROPORTIONAL (0-1)", "0", RIG_CONF_NUMERIC, { .n = { 0, 1, 1 } } },
    { TOK_BCN_DECAY, "BCN_DECAY", "BCN_DECAY", "BCN_DECAY (0-1)", "0", RIG_CONF_NUMERIC, { .n = { 0, 1, 1 } } },
    { TOK_BCN_AUTO_LOW_SPD, "BCN_AUTO_LOW_SPD", "BCN_AUTO_LOW_SPD", "BCN_AUTO_LOW_SPD (1-99)", "1", RIG_CONF_NUMERIC, { .n = { 1, 99, 1 } } },
    { TOK_BCN_DELAY, "BCN_DELAY", "BCN_DELAY", "BCN_DELAY (5-180)", "5", RIG_CONF_NUMERIC, { .n = { 5, 180, 1 } } },
    { TOK_BCN_TEXT_SEL, "BCN_TEXT_SEL", "BCN_TEXT_SEL", "BCN_TEXT_SEL (0-5)", "0", RIG_CONF_NUMERIC, { .n = { 0, 5, 1 } } },
    { TOK_BCN_TX_RATE, "BCN_TX_RATE", "BCN_TX_RATE", "BCN_TX_RATE (0-7)", "0", RIG_CONF_NUMERIC, { .n = { 0, 7, 1 } } },
    { TOK_BCN_FREQ, "BCN_FREQ", "BCN_FREQ", "BCN_FREQ (0-2)", "0", RIG_CONF_NUMERIC, { .n = { 0, 2, 1 } } },

    /* BT Settings */
    { TOK_BT_ENABLE, "BT_ENABLE", "BT_ENABLE", "BT_ENABLE (0-1)", "0", RIG_CONF_NUMERIC, { .n = { 0, 1, 1 } } },
    { TOK_BT_AUDIO, "BT_AUDIO", "BT_AUDIO", "BT_AUDIO (0-1)", "0", RIG_CONF_NUMERIC, { .n = { 0, 1, 1 } } },

    /* CW Settings */
    { TOK_CW_AGC_FAST, "CW_AGC_FAST", "CW_AGC_FAST", "CW_AGC_FAST (20-4000)", "20", RIG_CONF_NUMERIC, { .n = { 20, 4000, 1 } } },
    { TOK_CW_AGC_MID, "CW_AGC_MID", "CW_AGC_MID", "CW_AGC_MID (20-4000)", "20", RIG_CONF_NUMERIC, { .n = { 20, 4000, 1 } } },
    { TOK_CW_AGC_SLOW, "CW_AGC_SLOW", "CW_AGC_SLOW", "CW_AGC_SLOW (20-4000)", "20", RIG_CONF_NUMERIC, { .n = { 20, 4000, 1 } } },
    { TOK_CW_LCUT_FREQ, "CW_LCUT_FREQ", "CW_LCUT_FREQ", "CW_LCUT_FREQ (0-19)", "0", RIG_CONF_NUMERIC, { .n = { 0, 19, 1 } } },
    { TOK_CW_LCUT_SLOPE, "CW_LCUT_SLOPE", "CW_LCUT_SLOPE", "CW_LCUT_SLOPE (0-1)", "0", RIG_CONF_NUMERIC, { .n = { 0, 1, 1 } } },
    { TOK_CW_HCUT_FREQ, "CW_HCUT_FREQ", "CW_HCUT_FREQ", "CW_HCUT_FREQ (0-67)", "0", RIG_CONF_NUMERIC, { .n = { 0, 67, 1 } } },
    { TOK_CW_HCUT_SLOPE, "CW_HCUT_SLOPE", "CW_HCUT_SLOPE", "CW_HCUT_SLOPE (0-1)", "0", RIG_CONF_NUMERIC, { .n = { 0, 1, 1 } } },
    { TOK_CW_USB_OUT_LEVEL, "CW_USB_OUT_LEVEL", "CW_USB_OUT_LEVEL", "CW_USB_OUT_LEVEL (0-100)", "0", RIG_CONF_NUMERIC, { .n = { 0, 100, 1 } } },
    { TOK_CW_RPTT_SELECT, "CW_RPTT_SELECT", "CW_RPTT_SELECT", "CW_RPTT_SELECT (0-2)", "0", RIG_CONF_NUMERIC, { .n = { 0, 2, 1 } } },
    { TOK_CW_NAR_WIDTH, "CW_NAR_WIDTH", "CW_NAR_WIDTH", "CW_NAR_WIDTH (0-20)", "0", RIG_CONF_NUMERIC, { .n = { 0, 20, 1 } } },
    { TOK_CW_PC_KEYING, "CW_PC_KEYING", "CW_PC_KEYING", "CW_PC_KEYING (0-2)", "0", RIG_CONF_NUMERIC, { .n = { 0, 2, 1 } } },
    { TOK_CW_BK_IN_TYPE, "CW_BK_IN_TYPE", "CW_BK_IN_TYPE", "CW_BK_IN_TYPE (0-1)", "0", RIG_CONF_NUMERIC, { .n = { 0, 1, 1 } } },
    { TOK_CW_FREQ_DISPLAY, "CW_FREQ_DISPLAY", "CW_FREQ_DISPLAY", "CW_FREQ_DISPLAY (0-1)", "0", RIG_CONF_NUMERIC, { .n = { 0, 1, 1 } } },
    { TOK_CW_QSK_DELAY, "CW_QSK_DELAY", "CW_QSK_DELAY", "CW_QSK_DELAY (0-3)", "0", RIG_CONF_NUMERIC, { .n = { 0, 3, 1 } } },
    { TOK_CW_INDICATOR, "CW_INDICATOR", "CW_INDICATOR", "CW_INDICATOR (0-1)", "0", RIG_CONF_NUMERIC, { .n = { 0, 1, 1 } } },

    /* DATA Settings */
    { TOK_DATA_AGC_FAST, "DATA_AGC_FAST", "DATA_AGC_FAST", "DATA_AGC_FAST (20-4000)", "20", RIG_CONF_NUMERIC, { .n = { 20, 4000, 1 } } },
    { TOK_DATA_AGC_MID, "DATA_AGC_MID", "DATA_AGC_MID", "DATA_AGC_MID (20-4000)", "20", RIG_CONF_NUMERIC, { .n = { 20, 4000, 1 } } },
    { TOK_DATA_AGC_SLOW, "DATA_AGC_SLOW", "DATA_AGC_SLOW", "DATA_AGC_SLOW (20-4000)", "20", RIG_CONF_NUMERIC, { .n = { 20, 4000, 1 } } },
    { TOK_DATA_LCUT_FREQ, "DATA_LCUT_FREQ", "DATA_LCUT_FREQ", "DATA_LCUT_FREQ (0-19)", "0", RIG_CONF_NUMERIC, { .n = { 0, 19, 1 } } },
    { TOK_DATA_LCUT_SLOPE, "DATA_LCUT_SLOPE", "DATA_LCUT_SLOPE", "DATA_LCUT_SLOPE (0-1)", "0", RIG_CONF_NUMERIC, { .n = { 0, 1, 1 } } },
    { TOK_DATA_HCUT_FREQ, "DATA_HCUT_FREQ", "DATA_HCUT_FREQ", "DATA_HCUT_FREQ (0-67)", "0", RIG_CONF_NUMERIC, { .n = { 0, 67, 1 } } },
    { TOK_DATA_HCUT_SLOPE, "DATA_HCUT_SLOPE", "DATA_HCUT_SLOPE", "DATA_HCUT_SLOPE (0-1)", "0", RIG_CONF_NUMERIC, { .n = { 0, 1, 1 } } },
    { TOK_DATA_USB_OUT_LEVEL, "DATA_USB_OUT_LEVEL", "DATA_USB_OUT_LEVEL", "DATA_USB_OUT_LEVEL (0-100)", "0", RIG_CONF_NUMERIC, { .n = { 0, 100, 1 } } },
    { TOK_DATA_TX_BPF_SEL, "DATA_TX_BPF_SEL", "DATA_TX_BPF_SEL", "DATA_TX_BPF_SEL (0-4)", "0", RIG_CONF_NUMERIC, { .n = { 0, 4, 1 } } },
    { TOK_DATA_MOD_SOURCE, "DATA_MOD_SOURCE", "DATA_MOD_SOURCE", "DATA_MOD_SOURCE (0-3)", "0", RIG_CONF_NUMERIC, { .n = { 0, 3, 1 } } },
    { TOK_DATA_USB_MOD_GAIN, "DATA_USB_MOD_GAIN", "DATA_USB_MOD_GAIN", "DATA_USB_MOD_GAIN (0-100)", "0", RIG_CONF_NUMERIC, { .n = { 0, 100, 1 } } },
    { TOK_DATA_RPTT_SELECT, "DATA_RPTT_SELECT", "DATA_RPTT_SELECT", "DATA_RPTT_SELECT (0-2)", "0", RIG_CONF_NUMERIC, { .n = { 0, 2, 1 } } },
    { TOK_DATA_NAR_WIDTH, "DATA_NAR_WIDTH", "DATA_NAR_WIDTH", "DATA_NAR_WIDTH (0-20)", "0", RIG_CONF_NUMERIC, { .n = { 0, 20, 1 } } },
    { TOK_DATA_PSK_TONE, "DATA_PSK_TONE", "DATA_PSK_TONE", "DATA_PSK_TONE (0-2)", "0", RIG_CONF_NUMERIC, { .n = { 0, 2, 1 } } },
    { TOK_DATA_SHIFT_SSB, "DATA_SHIFT_SSB", "DATA_SHIFT_SSB", "DATA_SHIFT_SSB (0-3000)", "0", RIG_CONF_NUMERIC, { .n = { 0, 3000, 1 } } },

    /* DIAL Settings */
    { TOK_DIAL_RTTY_PSK_STEP, "DIAL_RTTY_PSK_STEP", "DIAL_RTTY_PSK_STEP", "DIAL_RTTY_PSK_STEP (0-2)", "0", RIG_CONF_NUMERIC, { .n = { 0, 2, 1 } } },
    { TOK_DIAL_FM_STEP, "DIAL_FM_STEP", "DIAL_FM_STEP", "DIAL_FM_STEP (0-6)", "0", RIG_CONF_NUMERIC, { .n = { 0, 6, 1 } } },
    { TOK_DIAL_CH_STEP, "DIAL_CH_STEP", "DIAL_CH_STEP", "DIAL_CH_STEP (0-3)", "0", RIG_CONF_NUMERIC, { .n = { 0, 3, 1 } } },
    { TOK_DIAL_AM_CH_STEP, "DIAL_AM_CH_STEP", "DIAL_AM_CH_STEP", "DIAL_AM_CH_STEP (0-5)", "0", RIG_CONF_NUMERIC, { .n = { 0, 5, 1 } } },
    { TOK_DIAL_FM_CH_STEP, "DIAL_FM_CH_STEP", "DIAL_FM_CH_STEP", "DIAL_FM_CH_STEP (0-5)", "0", RIG_CONF_NUMERIC, { .n = { 0, 5, 1 } } },
    { TOK_DIAL_MAIN_STEPS, "DIAL_MAIN_STEPS", "DIAL_MAIN_STEPS", "DIAL_MAIN_STEPS (0-2)", "0", RIG_CONF_NUMERIC, { .n = { 0, 2, 1 } } },
    { TOK_DIAL_MIC_P1, "DIAL_MIC_P1", "DIAL_MIC_P1", "DIAL_MIC_P1 (0-20)", "0", RIG_CONF_NUMERIC, { .n = { 0, 20, 1 } } },
    { TOK_DIAL_MIC_P2, "DIAL_MIC_P2", "DIAL_MIC_P2", "DIAL_MIC_P2 (0-20)", "0", RIG_CONF_NUMERIC, { .n = { 0, 20, 1 } } },
    { TOK_DIAL_MIC_P3, "DIAL_MIC_P3", "DIAL_MIC_P3", "DIAL_MIC_P3 (0-20)", "0", RIG_CONF_NUMERIC, { .n = { 0, 20, 1 } } },
    { TOK_DIAL_MIC_P4, "DIAL_MIC_P4", "DIAL_MIC_P4", "DIAL_MIC_P4 (0-20)", "0", RIG_CONF_NUMERIC, { .n = { 0, 20, 1 } } },
    { TOK_DIAL_MIC_UP, "DIAL_MIC_UP", "DIAL_MIC_UP", "DIAL_MIC_UP (0-20)", "0", RIG_CONF_NUMERIC, { .n = { 0, 20, 1 } } },
    { TOK_DIAL_MIC_DOWN, "DIAL_MIC_DOWN", "DIAL_MIC_DOWN", "DIAL_MIC_DOWN (0-20)", "0", RIG_CONF_NUMERIC, { .n = { 0, 20, 1 } } },
    { TOK_DIAL_MIC_SCAN, "DIAL_MIC_SCAN", "DIAL_MIC_SCAN", "DIAL_MIC_SCAN (0-1)", "0", RIG_CONF_NUMERIC, { .n = { 0, 1, 1 } } },

    /* DIG Settings */
    { TOK_DIG_POPUP, "DIG_POPUP", "DIG_POPUP", "DIG_POPUP (0-60)", "0", RIG_CONF_NUMERIC, { .n = { 0, 60, 1 } } },
    { TOK_DIG_LOC_SERVICE, "DIG_LOC_SERVICE", "DIG_LOC_SERVICE", "DIG_LOC_SERVICE (0-1)", "0", RIG_CONF_NUMERIC, { .n = { 0, 1, 1 } } },
    { TOK_DIG_STANDBY_BEEP, "DIG_STANDBY_BEEP", "DIG_STANDBY_BEEP", "DIG_STANDBY_BEEP (0-1)", "0", RIG_CONF_NUMERIC, { .n = { 0, 1, 1 } } },

    /* DISP Settings */
    { TOK_DISP_MY_CALL_TIME, "DISP_MY_CALL_TIME", "DISP_MY_CALL_TIME", "DISP_MY_CALL_TIME (0-5)", "0", RIG_CONF_NUMERIC, { .n = { 0, 5, 1 } } },
    { TOK_DISP_POPUP_TIME, "DISP_POPUP_TIME", "DISP_POPUP_TIME", "DISP_POPUP_TIME (0-2)", "0", RIG_CONF_NUMERIC, { .n = { 0, 2, 1 } } },
    { TOK_DISP_SCREEN_SAVER, "DISP_SCREEN_SAVER", "DISP_SCREEN_SAVER", "DISP_SCREEN_SAVER (0-6)", "0", RIG_CONF_NUMERIC, { .n = { 0, 6, 1 } } },
    { TOK_DISP_SAVER_BAT, "DISP_SAVER_BAT", "DISP_SAVER_BAT", "DISP_SAVER_BAT (0-6)", "0", RIG_CONF_NUMERIC, { .n = { 0, 6, 1 } } },
    { TOK_DISP_SAVER_TYPE, "DISP_SAVER_TYPE", "DISP_SAVER_TYPE", "DISP_SAVER_TYPE (0-2)", "0", RIG_CONF_NUMERIC, { .n = { 0, 2, 1 } } },
    { TOK_DISP_AUTO_PWR_OFF, "DISP_AUTO_PWR_OFF", "DISP_AUTO_PWR_OFF", "DISP_AUTO_PWR_OFF (0-24)", "0", RIG_CONF_NUMERIC, { .n = { 0, 24, 1 } } },

    /* DSP Settings */
    { TOK_DSP_IF_NOTCH_W, "DSP_IF_NOTCH_W", "DSP_IF_NOTCH_W", "DSP_IF_NOTCH_W (0-1)", "0", RIG_CONF_NUMERIC, { .n = { 0, 1, 1 } } },
    { TOK_DSP_NB_REJECTION, "DSP_NB_REJECTION", "DSP_NB_REJECTION", "DSP_NB_REJECTION (0-2)", "0", RIG_CONF_NUMERIC, { .n = { 0, 2, 1 } } },
    { TOK_DSP_NB_WIDTH, "DSP_NB_WIDTH", "DSP_NB_WIDTH", "DSP_NB_WIDTH (0-2)", "0", RIG_CONF_NUMERIC, { .n = { 0, 2, 1 } } },
    { TOK_DSP_APF_WIDTH, "DSP_APF_WIDTH", "DSP_APF_WIDTH", "DSP_APF_WIDTH (0-2)", "0", RIG_CONF_NUMERIC, { .n = { 0, 2, 1 } } },
    { TOK_DSP_CONTOUR_W, "DSP_CONTOUR_W", "DSP_CONTOUR_W", "DSP_CONTOUR_W (1-11)", "1", RIG_CONF_NUMERIC, { .n = { 1, 11, 1 } } },

    /* DT Settings */
    { TOK_DT_GPS_TIME_SET, "DT_GPS_TIME_SET", "DT_GPS_TIME_SET", "DT_GPS_TIME_SET (0-1)", "0", RIG_CONF_NUMERIC, { .n = { 0, 1, 1 } } },
    { TOK_DT_MY_POSITION, "DT_MY_POSITION", "DT_MY_POSITION", "DT_MY_POSITION (0-1)", "0", RIG_CONF_NUMERIC, { .n = { 0, 1, 1 } } },

    /* FILT Settings */
    { TOK_FILT_LIST_SORT, "FILT_LIST_SORT", "FILT_LIST_SORT", "FILT_LIST_SORT (0-2)", "0", RIG_CONF_NUMERIC, { .n = { 0, 2, 1 } } },
    { TOK_FILT_MIC_E, "FILT_MIC_E", "FILT_MIC_E", "FILT_MIC_E (0-1)", "0", RIG_CONF_NUMERIC, { .n = { 0, 1, 1 } } },
    { TOK_FILT_POSITION, "FILT_POSITION", "FILT_POSITION", "FILT_POSITION (0-1)", "0", RIG_CONF_NUMERIC, { .n = { 0, 1, 1 } } },
    { TOK_FILT_WEATHER, "FILT_WEATHER", "FILT_WEATHER", "FILT_WEATHER (0-1)", "0", RIG_CONF_NUMERIC, { .n = { 0, 1, 1 } } },
    { TOK_FILT_OBJECT, "FILT_OBJECT", "FILT_OBJECT", "FILT_OBJECT (0-1)", "0", RIG_CONF_NUMERIC, { .n = { 0, 1, 1 } } },
    { TOK_FILT_ITEM, "FILT_ITEM", "FILT_ITEM", "FILT_ITEM (0-1)", "0", RIG_CONF_NUMERIC, { .n = { 0, 1, 1 } } },
    { TOK_FILT_STATUS, "FILT_STATUS", "FILT_STATUS", "FILT_STATUS (0-1)", "0", RIG_CONF_NUMERIC, { .n = { 0, 1, 1 } } },
    { TOK_FILT_OTHER, "FILT_OTHER", "FILT_OTHER", "FILT_OTHER (0-1)", "0", RIG_CONF_NUMERIC, { .n = { 0, 1, 1 } } },
    { TOK_FILT_ALTNET, "FILT_ALTNET", "FILT_ALTNET", "FILT_ALTNET (0-1)", "0", RIG_CONF_NUMERIC, { .n = { 0, 1, 1 } } },
    { TOK_FILT_POPUP_BCN, "FILT_POPUP_BCN", "FILT_POPUP_BCN", "FILT_POPUP_BCN (0-4)", "0", RIG_CONF_NUMERIC, { .n = { 0, 4, 1 } } },
    { TOK_FILT_POPUP_MSG, "FILT_POPUP_MSG", "FILT_POPUP_MSG", "FILT_POPUP_MSG (0-4)", "0", RIG_CONF_NUMERIC, { .n = { 0, 4, 1 } } },
    { TOK_FILT_POPUP_MYPACKET, "FILT_POPUP_MYPACKET", "FILT_POPUP_MYPACKET", "FILT_POPUP_MYPACKET (0-1)", "0", RIG_CONF_NUMERIC, { .n = { 0, 1, 1 } } },

    /* FM Settings */
    { TOK_FM_AGC_FAST, "FM_AGC_FAST", "FM_AGC_FAST", "FM_AGC_FAST (20-4000)", "20", RIG_CONF_NUMERIC, { .n = { 20, 4000, 1 } } },
    { TOK_FM_AGC_MID, "FM_AGC_MID", "FM_AGC_MID", "FM_AGC_MID (20-4000)", "20", RIG_CONF_NUMERIC, { .n = { 20, 4000, 1 } } },
    { TOK_FM_AGC_SLOW, "FM_AGC_SLOW", "FM_AGC_SLOW", "FM_AGC_SLOW (20-4000)", "20", RIG_CONF_NUMERIC, { .n = { 20, 4000, 1 } } },
    { TOK_FM_LCUT_FREQ, "FM_LCUT_FREQ", "FM_LCUT_FREQ", "FM_LCUT_FREQ (0-19)", "0", RIG_CONF_NUMERIC, { .n = { 0, 19, 1 } } },
    { TOK_FM_LCUT_SLOPE, "FM_LCUT_SLOPE", "FM_LCUT_SLOPE", "FM_LCUT_SLOPE (0-1)", "0", RIG_CONF_NUMERIC, { .n = { 0, 1, 1 } } },
    { TOK_FM_HCUT_FREQ, "FM_HCUT_FREQ", "FM_HCUT_FREQ", "FM_HCUT_FREQ (0-67)", "0", RIG_CONF_NUMERIC, { .n = { 0, 67, 1 } } },
    { TOK_FM_HCUT_SLOPE, "FM_HCUT_SLOPE", "FM_HCUT_SLOPE", "FM_HCUT_SLOPE (0-1)", "0", RIG_CONF_NUMERIC, { .n = { 0, 1, 1 } } },
    { TOK_FM_USB_OUT_LEVEL, "FM_USB_OUT_LEVEL", "FM_USB_OUT_LEVEL", "FM_USB_OUT_LEVEL (0-100)", "0", RIG_CONF_NUMERIC, { .n = { 0, 100, 1 } } },
    { TOK_FM_MOD_SOURCE, "FM_MOD_SOURCE", "FM_MOD_SOURCE", "FM_MOD_SOURCE (0-3)", "0", RIG_CONF_NUMERIC, { .n = { 0, 3, 1 } } },
    { TOK_FM_USB_MOD_GAIN, "FM_USB_MOD_GAIN", "FM_USB_MOD_GAIN", "FM_USB_MOD_GAIN (0-100)", "0", RIG_CONF_NUMERIC, { .n = { 0, 100, 1 } } },
    { TOK_FM_RPTT_SELECT, "FM_RPTT_SELECT", "FM_RPTT_SELECT", "FM_RPTT_SELECT (0-2)", "0", RIG_CONF_NUMERIC, { .n = { 0, 2, 1 } } },
    { TOK_FM_RPT_SHIFT, "FM_RPT_SHIFT", "FM_RPT_SHIFT", "FM_RPT_SHIFT (0-3)", "0", RIG_CONF_NUMERIC, { .n = { 0, 3, 1 } } },
    { TOK_FM_RPT_SHIFT_28, "FM_RPT_SHIFT_28", "FM_RPT_SHIFT_28", "FM_RPT_SHIFT_28 (0-1000)", "0", RIG_CONF_NUMERIC, { .n = { 0, 1000, 1 } } },
    { TOK_FM_RPT_SHIFT_50, "FM_RPT_SHIFT_50", "FM_RPT_SHIFT_50", "FM_RPT_SHIFT_50 (0-4000)", "0", RIG_CONF_NUMERIC, { .n = { 0, 4000, 1 } } },
    { TOK_FM_RPT_SHIFT_144, "FM_RPT_SHIFT_144", "FM_RPT_SHIFT_144", "FM_RPT_SHIFT_144 (0-100)", "0", RIG_CONF_NUMERIC, { .n = { 0, 100, 1 } } },
    { TOK_FM_RPT_SHIFT_430, "FM_RPT_SHIFT_430", "FM_RPT_SHIFT_430", "FM_RPT_SHIFT_430 (0-100)", "0", RIG_CONF_NUMERIC, { .n = { 0, 100, 1 } } },
    { TOK_FM_SQL_TYPE, "FM_SQL_TYPE", "FM_SQL_TYPE", "FM_SQL_TYPE (0-5)", "0", RIG_CONF_NUMERIC, { .n = { 0, 5, 1 } } },
    { TOK_FM_TONE_FREQ, "FM_TONE_FREQ", "FM_TONE_FREQ", "FM_TONE_FREQ (0-49)", "0", RIG_CONF_NUMERIC, { .n = { 0, 49, 1 } } },
    { TOK_FM_DCS_CODE, "FM_DCS_CODE", "FM_DCS_CODE", "FM_DCS_CODE (0-103)", "0", RIG_CONF_NUMERIC, { .n = { 0, 103, 1 } } },
    { TOK_FM_DCS_RX_REV, "FM_DCS_RX_REV", "FM_DCS_RX_REV", "FM_DCS_RX_REV (0-2)", "0", RIG_CONF_NUMERIC, { .n = { 0, 2, 1 } } },
    { TOK_FM_DCS_TX_REV, "FM_DCS_TX_REV", "FM_DCS_TX_REV", "FM_DCS_TX_REV (0-1)", "0", RIG_CONF_NUMERIC, { .n = { 0, 1, 1 } } },
    { TOK_FM_PR_FREQ, "FM_PR_FREQ", "FM_PR_FREQ", "FM_PR_FREQ (300-3000)", "300", RIG_CONF_NUMERIC, { .n = { 300, 3000, 1 } } },
    { TOK_FM_DTMF_DELAY, "FM_DTMF_DELAY", "FM_DTMF_DELAY", "FM_DTMF_DELAY (0-4)", "0", RIG_CONF_NUMERIC, { .n = { 0, 4, 1 } } },
    { TOK_FM_DTMF_SPEED, "FM_DTMF_SPEED", "FM_DTMF_SPEED", "FM_DTMF_SPEED (0-1)", "0", RIG_CONF_NUMERIC, { .n = { 0, 1, 1 } } },

    /* GEN Settings */
    { TOK_GEN_BEEP_LEVEL, "GEN_BEEP_LEVEL", "GEN_BEEP_LEVEL", "GEN_BEEP_LEVEL (0-100)", "0", RIG_CONF_NUMERIC, { .n = { 0, 100, 1 } } },
    { TOK_GEN_RF_SQL_VR, "GEN_RF_SQL_VR", "GEN_RF_SQL_VR", "GEN_RF_SQL_VR (0-2)", "0", RIG_CONF_NUMERIC, { .n = { 0, 2, 1 } } },
    { TOK_GEN_TUN_LIN_PORT, "GEN_TUN_LIN_PORT", "GEN_TUN_LIN_PORT", "GEN_TUN_LIN_PORT (0-3)", "0", RIG_CONF_NUMERIC, { .n = { 0, 3, 1 } } },
    { TOK_GEN_TUNER_SELECT, "GEN_TUNER_SELECT", "GEN_TUNER_SELECT", "GEN_TUNER_SELECT (0-3)", "0", RIG_CONF_NUMERIC, { .n = { 0, 3, 1 } } },
    { TOK_GEN_CAT1_RATE, "GEN_CAT1_RATE", "GEN_CAT1_RATE", "GEN_CAT1_RATE (0-4)", "0", RIG_CONF_NUMERIC, { .n = { 0, 4, 1 } } },
    { TOK_GEN_CAT1_TIMEOUT, "GEN_CAT1_TIMEOUT", "GEN_CAT1_TIMEOUT", "GEN_CAT1_TIMEOUT (0-3)", "0", RIG_CONF_NUMERIC, { .n = { 0, 3, 1 } } },
    { TOK_GEN_CAT1_STOP_BIT, "GEN_CAT1_STOP_BIT", "GEN_CAT1_STOP_BIT", "GEN_CAT1_STOP_BIT (0-1)", "0", RIG_CONF_NUMERIC, { .n = { 0, 1, 1 } } },
    { TOK_GEN_CAT2_RATE, "GEN_CAT2_RATE", "GEN_CAT2_RATE", "GEN_CAT2_RATE (0-4)", "0", RIG_CONF_NUMERIC, { .n = { 0, 4, 1 } } },
    { TOK_GEN_CAT2_TIMEOUT, "GEN_CAT2_TIMEOUT", "GEN_CAT2_TIMEOUT", "GEN_CAT2_TIMEOUT (0-3)", "0", RIG_CONF_NUMERIC, { .n = { 0, 3, 1 } } },
    { TOK_GEN_CAT3_RATE, "GEN_CAT3_RATE", "GEN_CAT3_RATE", "GEN_CAT3_RATE (0-4)", "0", RIG_CONF_NUMERIC, { .n = { 0, 4, 1 } } },
    { TOK_GEN_CAT3_TIMEOUT, "GEN_CAT3_TIMEOUT", "GEN_CAT3_TIMEOUT", "GEN_CAT3_TIMEOUT (0-3)", "0", RIG_CONF_NUMERIC, { .n = { 0, 3, 1 } } },
    { TOK_GEN_TX_TIMEOUT, "GEN_TX_TIMEOUT", "GEN_TX_TIMEOUT", "GEN_TX_TIMEOUT (0-30)", "0", RIG_CONF_NUMERIC, { .n = { 0, 30, 1 } } },
    { TOK_GEN_CHARGE_CTRL, "GEN_CHARGE_CTRL", "GEN_CHARGE_CTRL", "GEN_CHARGE_CTRL (0-1)", "0", RIG_CONF_NUMERIC, { .n = { 0, 1, 1 } } },
    { TOK_GEN_SUB_BAND_MUTE, "GEN_SUB_BAND_MUTE", "GEN_SUB_BAND_MUTE", "GEN_SUB_BAND_MUTE (0-1)", "0", RIG_CONF_NUMERIC, { .n = { 0, 1, 1 } } },
    { TOK_GEN_SPEAKER_SEL, "GEN_SPEAKER_SEL", "GEN_SPEAKER_SEL", "GEN_SPEAKER_SEL (0-2)", "0", RIG_CONF_NUMERIC, { .n = { 0, 2, 1 } } },
    { TOK_GEN_DITHER, "GEN_DITHER", "GEN_DITHER", "GEN_DITHER (0-1)", "0", RIG_CONF_NUMERIC, { .n = { 0, 1, 1 } } },

    /* KEYER Settings */
    { TOK_KEYER_TYPE, "KEYER_TYPE", "KEYER_TYPE", "KEYER_TYPE (0-5)", "0", RIG_CONF_NUMERIC, { .n = { 0, 5, 1 } } },
    { TOK_KEYER_DOT_DASH, "KEYER_DOT_DASH", "KEYER_DOT_DASH", "KEYER_DOT_DASH (0-1)", "0", RIG_CONF_NUMERIC, { .n = { 0, 1, 1 } } },
    { TOK_KEYER_WEIGHT, "KEYER_WEIGHT", "KEYER_WEIGHT", "KEYER_WEIGHT (0-20)", "0", RIG_CONF_NUMERIC, { .n = { 0, 20, 1 } } },
    { TOK_KEYER_NUM_STYLE, "KEYER_NUM_STYLE", "KEYER_NUM_STYLE", "KEYER_NUM_STYLE (0-6)", "0", RIG_CONF_NUMERIC, { .n = { 0, 6, 1 } } },
    { TOK_KEYER_CONTEST_NUM, "KEYER_CONTEST_NUM", "KEYER_CONTEST_NUM", "KEYER_CONTEST_NUM (1-9999)", "1", RIG_CONF_NUMERIC, { .n = { 1, 9999, 1 } } },
    { TOK_KEYER_CW_MEM1, "KEYER_CW_MEM1", "KEYER_CW_MEM1", "KEYER_CW_MEM1 (0-1)", "0", RIG_CONF_NUMERIC, { .n = { 0, 1, 1 } } },
    { TOK_KEYER_CW_MEM2, "KEYER_CW_MEM2", "KEYER_CW_MEM2", "KEYER_CW_MEM2 (0-1)", "0", RIG_CONF_NUMERIC, { .n = { 0, 1, 1 } } },
    { TOK_KEYER_CW_MEM3, "KEYER_CW_MEM3", "KEYER_CW_MEM3", "KEYER_CW_MEM3 (0-1)", "0", RIG_CONF_NUMERIC, { .n = { 0, 1, 1 } } },
    { TOK_KEYER_CW_MEM4, "KEYER_CW_MEM4", "KEYER_CW_MEM4", "KEYER_CW_MEM4 (0-1)", "0", RIG_CONF_NUMERIC, { .n = { 0, 1, 1 } } },
    { TOK_KEYER_CW_MEM5, "KEYER_CW_MEM5", "KEYER_CW_MEM5", "KEYER_CW_MEM5 (0-1)", "0", RIG_CONF_NUMERIC, { .n = { 0, 1, 1 } } },
    { TOK_KEYER_REPEAT_INT, "KEYER_REPEAT_INT", "KEYER_REPEAT_INT", "KEYER_REPEAT_INT (1-60)", "1", RIG_CONF_NUMERIC, { .n = { 1, 60, 1 } } },

    /* PRE1 Settings */
    { TOK_PRE1_CAT1_RATE, "PRE1_CAT1_RATE", "PRE1_CAT1_RATE", "PRE1_CAT1_RATE (0-4)", "0", RIG_CONF_NUMERIC, { .n = { 0, 4, 1 } } },
    { TOK_PRE1_CAT1_TIMEOUT, "PRE1_CAT1_TIMEOUT", "PRE1_CAT1_TIMEOUT", "PRE1_CAT1_TIMEOUT (0-3)", "0", RIG_CONF_NUMERIC, { .n = { 0, 3, 1 } } },
    { TOK_PRE1_STOP_BIT, "PRE1_STOP_BIT", "PRE1_STOP_BIT", "PRE1_STOP_BIT (0-1)", "0", RIG_CONF_NUMERIC, { .n = { 0, 1, 1 } } },
    { TOK_PRE1_AGC_FAST, "PRE1_AGC_FAST", "PRE1_AGC_FAST", "PRE1_AGC_FAST (20-4000)", "20", RIG_CONF_NUMERIC, { .n = { 20, 4000, 1 } } },
    { TOK_PRE1_AGC_MID, "PRE1_AGC_MID", "PRE1_AGC_MID", "PRE1_AGC_MID (20-4000)", "20", RIG_CONF_NUMERIC, { .n = { 20, 4000, 1 } } },
    { TOK_PRE1_AGC_SLOW, "PRE1_AGC_SLOW", "PRE1_AGC_SLOW", "PRE1_AGC_SLOW (20-4000)", "20", RIG_CONF_NUMERIC, { .n = { 20, 4000, 1 } } },
    { TOK_PRE1_LCUT_FREQ, "PRE1_LCUT_FREQ", "PRE1_LCUT_FREQ", "PRE1_LCUT_FREQ (0-19)", "0", RIG_CONF_NUMERIC, { .n = { 0, 19, 1 } } },
    { TOK_PRE1_LCUT_SLOPE, "PRE1_LCUT_SLOPE", "PRE1_LCUT_SLOPE", "PRE1_LCUT_SLOPE (0-1)", "0", RIG_CONF_NUMERIC, { .n = { 0, 1, 1 } } },
    { TOK_PRE1_HCUT_FREQ, "PRE1_HCUT_FREQ", "PRE1_HCUT_FREQ", "PRE1_HCUT_FREQ (0-67)", "0", RIG_CONF_NUMERIC, { .n = { 0, 67, 1 } } },
    { TOK_PRE1_HCUT_SLOPE, "PRE1_HCUT_SLOPE", "PRE1_HCUT_SLOPE", "PRE1_HCUT_SLOPE (0-1)", "0", RIG_CONF_NUMERIC, { .n = { 0, 1, 1 } } },
    { TOK_PRE1_USB_OUT_LVL, "PRE1_USB_OUT_LVL", "PRE1_USB_OUT_LVL", "PRE1_USB_OUT_LVL (0-100)", "0", RIG_CONF_NUMERIC, { .n = { 0, 100, 1 } } },
    { TOK_PRE1_TX_BPF, "PRE1_TX_BPF", "PRE1_TX_BPF", "PRE1_TX_BPF (0-4)", "0", RIG_CONF_NUMERIC, { .n = { 0, 4, 1 } } },
    { TOK_PRE1_MOD_SOURCE, "PRE1_MOD_SOURCE", "PRE1_MOD_SOURCE", "PRE1_MOD_SOURCE (0-3)", "0", RIG_CONF_NUMERIC, { .n = { 0, 3, 1 } } },
    { TOK_PRE1_USB_MOD_GAIN, "PRE1_USB_MOD_GAIN", "PRE1_USB_MOD_GAIN", "PRE1_USB_MOD_GAIN (0-100)", "0", RIG_CONF_NUMERIC, { .n = { 0, 100, 1 } } },
    { TOK_PRE1_RPTT_SELECT, "PRE1_RPTT_SELECT", "PRE1_RPTT_SELECT", "PRE1_RPTT_SELECT (0-3)", "0", RIG_CONF_NUMERIC, { .n = { 0, 3, 1 } } },

    /* PRE2 Settings */
    { TOK_PRE2_CAT1_RATE, "PRE2_CAT1_RATE", "PRE2_CAT1_RATE", "PRE2_CAT1_RATE (0-4)", "0", RIG_CONF_NUMERIC, { .n = { 0, 4, 1 } } },
    { TOK_PRE2_CAT1_TIMEOUT, "PRE2_CAT1_TIMEOUT", "PRE2_CAT1_TIMEOUT", "PRE2_CAT1_TIMEOUT (0-3)", "0", RIG_CONF_NUMERIC, { .n = { 0, 3, 1 } } },
    { TOK_PRE2_STOP_BIT, "PRE2_STOP_BIT", "PRE2_STOP_BIT", "PRE2_STOP_BIT (0-1)", "0", RIG_CONF_NUMERIC, { .n = { 0, 1, 1 } } },
    { TOK_PRE2_AGC_FAST, "PRE2_AGC_FAST", "PRE2_AGC_FAST", "PRE2_AGC_FAST (20-4000)", "20", RIG_CONF_NUMERIC, { .n = { 20, 4000, 1 } } },
    { TOK_PRE2_AGC_MID, "PRE2_AGC_MID", "PRE2_AGC_MID", "PRE2_AGC_MID (20-4000)", "20", RIG_CONF_NUMERIC, { .n = { 20, 4000, 1 } } },
    { TOK_PRE2_AGC_SLOW, "PRE2_AGC_SLOW", "PRE2_AGC_SLOW", "PRE2_AGC_SLOW (20-4000)", "20", RIG_CONF_NUMERIC, { .n = { 20, 4000, 1 } } },
    { TOK_PRE2_LCUT_FREQ, "PRE2_LCUT_FREQ", "PRE2_LCUT_FREQ", "PRE2_LCUT_FREQ (0-19)", "0", RIG_CONF_NUMERIC, { .n = { 0, 19, 1 } } },
    { TOK_PRE2_LCUT_SLOPE, "PRE2_LCUT_SLOPE", "PRE2_LCUT_SLOPE", "PRE2_LCUT_SLOPE (0-1)", "0", RIG_CONF_NUMERIC, { .n = { 0, 1, 1 } } },
    { TOK_PRE2_HCUT_FREQ, "PRE2_HCUT_FREQ", "PRE2_HCUT_FREQ", "PRE2_HCUT_FREQ (0-67)", "0", RIG_CONF_NUMERIC, { .n = { 0, 67, 1 } } },
    { TOK_PRE2_HCUT_SLOPE, "PRE2_HCUT_SLOPE", "PRE2_HCUT_SLOPE", "PRE2_HCUT_SLOPE (0-1)", "0", RIG_CONF_NUMERIC, { .n = { 0, 1, 1 } } },
    { TOK_PRE2_USB_OUT_LVL, "PRE2_USB_OUT_LVL", "PRE2_USB_OUT_LVL", "PRE2_USB_OUT_LVL (0-100)", "0", RIG_CONF_NUMERIC, { .n = { 0, 100, 1 } } },
    { TOK_PRE2_TX_BPF, "PRE2_TX_BPF", "PRE2_TX_BPF", "PRE2_TX_BPF (0-4)", "0", RIG_CONF_NUMERIC, { .n = { 0, 4, 1 } } },
    { TOK_PRE2_MOD_SOURCE, "PRE2_MOD_SOURCE", "PRE2_MOD_SOURCE", "PRE2_MOD_SOURCE (0-3)", "0", RIG_CONF_NUMERIC, { .n = { 0, 3, 1 } } },
    { TOK_PRE2_USB_MOD_GAIN, "PRE2_USB_MOD_GAIN", "PRE2_USB_MOD_GAIN", "PRE2_USB_MOD_GAIN (0-100)", "0", RIG_CONF_NUMERIC, { .n = { 0, 100, 1 } } },
    { TOK_PRE2_RPTT_SELECT, "PRE2_RPTT_SELECT", "PRE2_RPTT_SELECT", "PRE2_RPTT_SELECT (0-3)", "0", RIG_CONF_NUMERIC, { .n = { 0, 3, 1 } } },

    /* PRE3 Settings */
    { TOK_PRE3_CAT1_RATE, "PRE3_CAT1_RATE", "PRE3_CAT1_RATE", "PRE3_CAT1_RATE (0-4)", "0", RIG_CONF_NUMERIC, { .n = { 0, 4, 1 } } },
    { TOK_PRE3_CAT1_TIMEOUT, "PRE3_CAT1_TIMEOUT", "PRE3_CAT1_TIMEOUT", "PRE3_CAT1_TIMEOUT (0-3)", "0", RIG_CONF_NUMERIC, { .n = { 0, 3, 1 } } },
    { TOK_PRE3_STOP_BIT, "PRE3_STOP_BIT", "PRE3_STOP_BIT", "PRE3_STOP_BIT (0-1)", "0", RIG_CONF_NUMERIC, { .n = { 0, 1, 1 } } },
    { TOK_PRE3_AGC_FAST, "PRE3_AGC_FAST", "PRE3_AGC_FAST", "PRE3_AGC_FAST (20-4000)", "20", RIG_CONF_NUMERIC, { .n = { 20, 4000, 1 } } },
    { TOK_PRE3_AGC_MID, "PRE3_AGC_MID", "PRE3_AGC_MID", "PRE3_AGC_MID (20-4000)", "20", RIG_CONF_NUMERIC, { .n = { 20, 4000, 1 } } },
    { TOK_PRE3_AGC_SLOW, "PRE3_AGC_SLOW", "PRE3_AGC_SLOW", "PRE3_AGC_SLOW (20-4000)", "20", RIG_CONF_NUMERIC, { .n = { 20, 4000, 1 } } },
    { TOK_PRE3_LCUT_FREQ, "PRE3_LCUT_FREQ", "PRE3_LCUT_FREQ", "PRE3_LCUT_FREQ (0-19)", "0", RIG_CONF_NUMERIC, { .n = { 0, 19, 1 } } },
    { TOK_PRE3_LCUT_SLOPE, "PRE3_LCUT_SLOPE", "PRE3_LCUT_SLOPE", "PRE3_LCUT_SLOPE (0-1)", "0", RIG_CONF_NUMERIC, { .n = { 0, 1, 1 } } },
    { TOK_PRE3_HCUT_FREQ, "PRE3_HCUT_FREQ", "PRE3_HCUT_FREQ", "PRE3_HCUT_FREQ (0-67)", "0", RIG_CONF_NUMERIC, { .n = { 0, 67, 1 } } },
    { TOK_PRE3_HCUT_SLOPE, "PRE3_HCUT_SLOPE", "PRE3_HCUT_SLOPE", "PRE3_HCUT_SLOPE (0-1)", "0", RIG_CONF_NUMERIC, { .n = { 0, 1, 1 } } },
    { TOK_PRE3_USB_OUT_LVL, "PRE3_USB_OUT_LVL", "PRE3_USB_OUT_LVL", "PRE3_USB_OUT_LVL (0-100)", "0", RIG_CONF_NUMERIC, { .n = { 0, 100, 1 } } },
    { TOK_PRE3_TX_BPF, "PRE3_TX_BPF", "PRE3_TX_BPF", "PRE3_TX_BPF (0-4)", "0", RIG_CONF_NUMERIC, { .n = { 0, 4, 1 } } },
    { TOK_PRE3_MOD_SOURCE, "PRE3_MOD_SOURCE", "PRE3_MOD_SOURCE", "PRE3_MOD_SOURCE (0-3)", "0", RIG_CONF_NUMERIC, { .n = { 0, 3, 1 } } },
    { TOK_PRE3_USB_MOD_GAIN, "PRE3_USB_MOD_GAIN", "PRE3_USB_MOD_GAIN", "PRE3_USB_MOD_GAIN (0-100)", "0", RIG_CONF_NUMERIC, { .n = { 0, 100, 1 } } },
    { TOK_PRE3_RPTT_SELECT, "PRE3_RPTT_SELECT", "PRE3_RPTT_SELECT", "PRE3_RPTT_SELECT (0-3)", "0", RIG_CONF_NUMERIC, { .n = { 0, 3, 1 } } },

    /* PRE4 Settings */
    { TOK_PRE4_CAT1_RATE, "PRE4_CAT1_RATE", "PRE4_CAT1_RATE", "PRE4_CAT1_RATE (0-4)", "0", RIG_CONF_NUMERIC, { .n = { 0, 4, 1 } } },
    { TOK_PRE4_CAT1_TIMEOUT, "PRE4_CAT1_TIMEOUT", "PRE4_CAT1_TIMEOUT", "PRE4_CAT1_TIMEOUT (0-3)", "0", RIG_CONF_NUMERIC, { .n = { 0, 3, 1 } } },
    { TOK_PRE4_STOP_BIT, "PRE4_STOP_BIT", "PRE4_STOP_BIT", "PRE4_STOP_BIT (0-1)", "0", RIG_CONF_NUMERIC, { .n = { 0, 1, 1 } } },
    { TOK_PRE4_AGC_FAST, "PRE4_AGC_FAST", "PRE4_AGC_FAST", "PRE4_AGC_FAST (20-4000)", "20", RIG_CONF_NUMERIC, { .n = { 20, 4000, 1 } } },
    { TOK_PRE4_AGC_MID, "PRE4_AGC_MID", "PRE4_AGC_MID", "PRE4_AGC_MID (20-4000)", "20", RIG_CONF_NUMERIC, { .n = { 20, 4000, 1 } } },
    { TOK_PRE4_AGC_SLOW, "PRE4_AGC_SLOW", "PRE4_AGC_SLOW", "PRE4_AGC_SLOW (20-4000)", "20", RIG_CONF_NUMERIC, { .n = { 20, 4000, 1 } } },
    { TOK_PRE4_LCUT_FREQ, "PRE4_LCUT_FREQ", "PRE4_LCUT_FREQ", "PRE4_LCUT_FREQ (0-19)", "0", RIG_CONF_NUMERIC, { .n = { 0, 19, 1 } } },
    { TOK_PRE4_LCUT_SLOPE, "PRE4_LCUT_SLOPE", "PRE4_LCUT_SLOPE", "PRE4_LCUT_SLOPE (0-1)", "0", RIG_CONF_NUMERIC, { .n = { 0, 1, 1 } } },
    { TOK_PRE4_HCUT_FREQ, "PRE4_HCUT_FREQ", "PRE4_HCUT_FREQ", "PRE4_HCUT_FREQ (0-67)", "0", RIG_CONF_NUMERIC, { .n = { 0, 67, 1 } } },
    { TOK_PRE4_HCUT_SLOPE, "PRE4_HCUT_SLOPE", "PRE4_HCUT_SLOPE", "PRE4_HCUT_SLOPE (0-1)", "0", RIG_CONF_NUMERIC, { .n = { 0, 1, 1 } } },
    { TOK_PRE4_USB_OUT_LVL, "PRE4_USB_OUT_LVL", "PRE4_USB_OUT_LVL", "PRE4_USB_OUT_LVL (0-100)", "0", RIG_CONF_NUMERIC, { .n = { 0, 100, 1 } } },
    { TOK_PRE4_TX_BPF, "PRE4_TX_BPF", "PRE4_TX_BPF", "PRE4_TX_BPF (0-4)", "0", RIG_CONF_NUMERIC, { .n = { 0, 4, 1 } } },
    { TOK_PRE4_MOD_SOURCE, "PRE4_MOD_SOURCE", "PRE4_MOD_SOURCE", "PRE4_MOD_SOURCE (0-3)", "0", RIG_CONF_NUMERIC, { .n = { 0, 3, 1 } } },
    { TOK_PRE4_USB_MOD_GAIN, "PRE4_USB_MOD_GAIN", "PRE4_USB_MOD_GAIN", "PRE4_USB_MOD_GAIN (0-100)", "0", RIG_CONF_NUMERIC, { .n = { 0, 100, 1 } } },
    { TOK_PRE4_RPTT_SELECT, "PRE4_RPTT_SELECT", "PRE4_RPTT_SELECT", "PRE4_RPTT_SELECT (0-3)", "0", RIG_CONF_NUMERIC, { .n = { 0, 3, 1 } } },

    /* PRE5 Settings */
    { TOK_PRE5_CAT1_RATE, "PRE5_CAT1_RATE", "PRE5_CAT1_RATE", "PRE5_CAT1_RATE (0-4)", "0", RIG_CONF_NUMERIC, { .n = { 0, 4, 1 } } },
    { TOK_PRE5_CAT1_TIMEOUT, "PRE5_CAT1_TIMEOUT", "PRE5_CAT1_TIMEOUT", "PRE5_CAT1_TIMEOUT (0-3)", "0", RIG_CONF_NUMERIC, { .n = { 0, 3, 1 } } },
    { TOK_PRE5_STOP_BIT, "PRE5_STOP_BIT", "PRE5_STOP_BIT", "PRE5_STOP_BIT (0-1)", "0", RIG_CONF_NUMERIC, { .n = { 0, 1, 1 } } },
    { TOK_PRE5_AGC_FAST, "PRE5_AGC_FAST", "PRE5_AGC_FAST", "PRE5_AGC_FAST (20-4000)", "20", RIG_CONF_NUMERIC, { .n = { 20, 4000, 1 } } },
    { TOK_PRE5_AGC_MID, "PRE5_AGC_MID", "PRE5_AGC_MID", "PRE5_AGC_MID (20-4000)", "20", RIG_CONF_NUMERIC, { .n = { 20, 4000, 1 } } },
    { TOK_PRE5_AGC_SLOW, "PRE5_AGC_SLOW", "PRE5_AGC_SLOW", "PRE5_AGC_SLOW (20-4000)", "20", RIG_CONF_NUMERIC, { .n = { 20, 4000, 1 } } },
    { TOK_PRE5_LCUT_FREQ, "PRE5_LCUT_FREQ", "PRE5_LCUT_FREQ", "PRE5_LCUT_FREQ (0-19)", "0", RIG_CONF_NUMERIC, { .n = { 0, 19, 1 } } },
    { TOK_PRE5_LCUT_SLOPE, "PRE5_LCUT_SLOPE", "PRE5_LCUT_SLOPE", "PRE5_LCUT_SLOPE (0-1)", "0", RIG_CONF_NUMERIC, { .n = { 0, 1, 1 } } },
    { TOK_PRE5_HCUT_FREQ, "PRE5_HCUT_FREQ", "PRE5_HCUT_FREQ", "PRE5_HCUT_FREQ (0-67)", "0", RIG_CONF_NUMERIC, { .n = { 0, 67, 1 } } },
    { TOK_PRE5_HCUT_SLOPE, "PRE5_HCUT_SLOPE", "PRE5_HCUT_SLOPE", "PRE5_HCUT_SLOPE (0-1)", "0", RIG_CONF_NUMERIC, { .n = { 0, 1, 1 } } },
    { TOK_PRE5_USB_OUT_LVL, "PRE5_USB_OUT_LVL", "PRE5_USB_OUT_LVL", "PRE5_USB_OUT_LVL (0-100)", "0", RIG_CONF_NUMERIC, { .n = { 0, 100, 1 } } },
    { TOK_PRE5_TX_BPF, "PRE5_TX_BPF", "PRE5_TX_BPF", "PRE5_TX_BPF (0-4)", "0", RIG_CONF_NUMERIC, { .n = { 0, 4, 1 } } },
    { TOK_PRE5_MOD_SOURCE, "PRE5_MOD_SOURCE", "PRE5_MOD_SOURCE", "PRE5_MOD_SOURCE (0-3)", "0", RIG_CONF_NUMERIC, { .n = { 0, 3, 1 } } },
    { TOK_PRE5_USB_MOD_GAIN, "PRE5_USB_MOD_GAIN", "PRE5_USB_MOD_GAIN", "PRE5_USB_MOD_GAIN (0-100)", "0", RIG_CONF_NUMERIC, { .n = { 0, 100, 1 } } },
    { TOK_PRE5_RPTT_SELECT, "PRE5_RPTT_SELECT", "PRE5_RPTT_SELECT", "PRE5_RPTT_SELECT (0-3)", "0", RIG_CONF_NUMERIC, { .n = { 0, 3, 1 } } },

    /* RING Settings */
    { TOK_RING_TX_BCN, "RING_TX_BCN", "RING_TX_BCN", "RING_TX_BCN (0-1)", "0", RIG_CONF_NUMERIC, { .n = { 0, 1, 1 } } },
    { TOK_RING_RX_BCN, "RING_RX_BCN", "RING_RX_BCN", "RING_RX_BCN (0-1)", "0", RIG_CONF_NUMERIC, { .n = { 0, 1, 1 } } },
    { TOK_RING_TX_MSG, "RING_TX_MSG", "RING_TX_MSG", "RING_TX_MSG (0-1)", "0", RIG_CONF_NUMERIC, { .n = { 0, 1, 1 } } },
    { TOK_RING_RX_MSG, "RING_RX_MSG", "RING_RX_MSG", "RING_RX_MSG (0-1)", "0", RIG_CONF_NUMERIC, { .n = { 0, 1, 1 } } },
    { TOK_RING_MY_PACKET, "RING_MY_PACKET", "RING_MY_PACKET", "RING_MY_PACKET (0-1)", "0", RIG_CONF_NUMERIC, { .n = { 0, 1, 1 } } },

    /* RTTY Settings */
    { TOK_RTTY_AGC_FAST, "RTTY_AGC_FAST", "RTTY_AGC_FAST", "RTTY_AGC_FAST (20-4000)", "20", RIG_CONF_NUMERIC, { .n = { 20, 4000, 1 } } },
    { TOK_RTTY_AGC_MID, "RTTY_AGC_MID", "RTTY_AGC_MID", "RTTY_AGC_MID (20-4000)", "20", RIG_CONF_NUMERIC, { .n = { 20, 4000, 1 } } },
    { TOK_RTTY_AGC_SLOW, "RTTY_AGC_SLOW", "RTTY_AGC_SLOW", "RTTY_AGC_SLOW (20-4000)", "20", RIG_CONF_NUMERIC, { .n = { 20, 4000, 1 } } },
    { TOK_RTTY_LCUT_FREQ, "RTTY_LCUT_FREQ", "RTTY_LCUT_FREQ", "RTTY_LCUT_FREQ (0-19)", "0", RIG_CONF_NUMERIC, { .n = { 0, 19, 1 } } },
    { TOK_RTTY_LCUT_SLOPE, "RTTY_LCUT_SLOPE", "RTTY_LCUT_SLOPE", "RTTY_LCUT_SLOPE (0-1)", "0", RIG_CONF_NUMERIC, { .n = { 0, 1, 1 } } },
    { TOK_RTTY_HCUT_FREQ, "RTTY_HCUT_FREQ", "RTTY_HCUT_FREQ", "RTTY_HCUT_FREQ (0-67)", "0", RIG_CONF_NUMERIC, { .n = { 0, 67, 1 } } },
    { TOK_RTTY_HCUT_SLOPE, "RTTY_HCUT_SLOPE", "RTTY_HCUT_SLOPE", "RTTY_HCUT_SLOPE (0-1)", "0", RIG_CONF_NUMERIC, { .n = { 0, 1, 1 } } },
    { TOK_RTTY_USB_OUT_LEVEL, "RTTY_USB_OUT_LEVEL", "RTTY_USB_OUT_LEVEL", "RTTY_USB_OUT_LEVEL (0-100)", "0", RIG_CONF_NUMERIC, { .n = { 0, 100, 1 } } },
    { TOK_RTTY_RPTT_SELECT, "RTTY_RPTT_SELECT", "RTTY_RPTT_SELECT", "RTTY_RPTT_SELECT (0-2)", "0", RIG_CONF_NUMERIC, { .n = { 0, 2, 1 } } },
    { TOK_RTTY_NAR_WIDTH, "RTTY_NAR_WIDTH", "RTTY_NAR_WIDTH", "RTTY_NAR_WIDTH (0-20)", "0", RIG_CONF_NUMERIC, { .n = { 0, 20, 1 } } },
    { TOK_RTTY_MARK_FREQ, "RTTY_MARK_FREQ", "RTTY_MARK_FREQ", "RTTY_MARK_FREQ (0-1)", "0", RIG_CONF_NUMERIC, { .n = { 0, 1, 1 } } },
    { TOK_RTTY_SHIFT_FREQ, "RTTY_SHIFT_FREQ", "RTTY_SHIFT_FREQ", "RTTY_SHIFT_FREQ (0-3)", "0", RIG_CONF_NUMERIC, { .n = { 0, 3, 1 } } },
    { TOK_RTTY_POLARITY_TX, "RTTY_POLARITY_TX", "RTTY_POLARITY_TX", "RTTY_POLARITY_TX (0-1)", "0", RIG_CONF_NUMERIC, { .n = { 0, 1, 1 } } },

    /* SCAN Settings */
    { TOK_SCAN_QMB_CH, "SCAN_QMB_CH", "SCAN_QMB_CH", "SCAN_QMB_CH (0-1)", "0", RIG_CONF_NUMERIC, { .n = { 0, 1, 1 } } },
    { TOK_SCAN_BAND_STACK, "SCAN_BAND_STACK", "SCAN_BAND_STACK", "SCAN_BAND_STACK (0-1)", "0", RIG_CONF_NUMERIC, { .n = { 0, 1, 1 } } },
    { TOK_SCAN_BAND_EDGE, "SCAN_BAND_EDGE", "SCAN_BAND_EDGE", "SCAN_BAND_EDGE (0-1)", "0", RIG_CONF_NUMERIC, { .n = { 0, 1, 1 } } },
    { TOK_SCAN_RESUME, "SCAN_RESUME", "SCAN_RESUME", "SCAN_RESUME (0-4)", "0", RIG_CONF_NUMERIC, { .n = { 0, 4, 1 } } },

    /* SCOPE Settings */
    { TOK_SCOPE_RBW, "SCOPE_RBW", "SCOPE_RBW", "SCOPE_RBW (0-2)", "0", RIG_CONF_NUMERIC, { .n = { 0, 2, 1 } } },
    { TOK_SCOPE_CTR, "SCOPE_CTR", "SCOPE_CTR", "SCOPE_CTR (0-1)", "0", RIG_CONF_NUMERIC, { .n = { 0, 1, 1 } } },
    { TOK_SCOPE_2D_SENS, "SCOPE_2D_SENS", "SCOPE_2D_SENS", "SCOPE_2D_SENS (0-1)", "0", RIG_CONF_NUMERIC, { .n = { 0, 1, 1 } } },
    { TOK_SCOPE_3DSS_SENS, "SCOPE_3DSS_SENS", "SCOPE_3DSS_SENS", "SCOPE_3DSS_SENS (0-1)", "0", RIG_CONF_NUMERIC, { .n = { 0, 1, 1 } } },
    { TOK_SCOPE_AVERAGE, "SCOPE_AVERAGE", "SCOPE_AVERAGE", "SCOPE_AVERAGE (0-3)", "0", RIG_CONF_NUMERIC, { .n = { 0, 3, 1 } } },

    /* SMART Settings */
    { TOK_SMART_LOW_SPD, "SMART_LOW_SPD", "SMART_LOW_SPD", "SMART_LOW_SPD (2-30)", "2", RIG_CONF_NUMERIC, { .n = { 2, 30, 1 } } },
    { TOK_SMART_HIGH_SPD, "SMART_HIGH_SPD", "SMART_HIGH_SPD", "SMART_HIGH_SPD (3-90)", "3", RIG_CONF_NUMERIC, { .n = { 3, 90, 1 } } },
    { TOK_SMART_SLOW_RATE, "SMART_SLOW_RATE", "SMART_SLOW_RATE", "SMART_SLOW_RATE (1-100)", "1", RIG_CONF_NUMERIC, { .n = { 1, 100, 1 } } },
    { TOK_SMART_FAST_RATE, "SMART_FAST_RATE", "SMART_FAST_RATE", "SMART_FAST_RATE (10-180)", "10", RIG_CONF_NUMERIC, { .n = { 10, 180, 1 } } },
    { TOK_SMART_TURN_ANGLE, "SMART_TURN_ANGLE", "SMART_TURN_ANGLE", "SMART_TURN_ANGLE (5-90)", "5", RIG_CONF_NUMERIC, { .n = { 5, 90, 1 } } },
    { TOK_SMART_TURN_SLOPE, "SMART_TURN_SLOPE", "SMART_TURN_SLOPE", "SMART_TURN_SLOPE (1-255)", "1", RIG_CONF_NUMERIC, { .n = { 1, 255, 1 } } },
    { TOK_SMART_TURN_TIME, "SMART_TURN_TIME", "SMART_TURN_TIME", "SMART_TURN_TIME (5-180)", "5", RIG_CONF_NUMERIC, { .n = { 5, 180, 1 } } },

    /* SSB Settings */
    { TOK_SSB_AGC_FAST, "SSB_AGC_FAST", "SSB_AGC_FAST", "SSB_AGC_FAST (20-4000)", "20", RIG_CONF_NUMERIC, { .n = { 20, 4000, 1 } } },
    { TOK_SSB_AGC_MID, "SSB_AGC_MID", "SSB_AGC_MID", "SSB_AGC_MID (20-4000)", "20", RIG_CONF_NUMERIC, { .n = { 20, 4000, 1 } } },
    { TOK_SSB_AGC_SLOW, "SSB_AGC_SLOW", "SSB_AGC_SLOW", "SSB_AGC_SLOW (20-4000)", "20", RIG_CONF_NUMERIC, { .n = { 20, 4000, 1 } } },
    { TOK_SSB_LCUT_FREQ, "SSB_LCUT_FREQ", "SSB_LCUT_FREQ", "SSB_LCUT_FREQ (0-19)", "0", RIG_CONF_NUMERIC, { .n = { 0, 19, 1 } } },
    { TOK_SSB_LCUT_SLOPE, "SSB_LCUT_SLOPE", "SSB_LCUT_SLOPE", "SSB_LCUT_SLOPE (0-1)", "0", RIG_CONF_NUMERIC, { .n = { 0, 1, 1 } } },
    { TOK_SSB_HCUT_FREQ, "SSB_HCUT_FREQ", "SSB_HCUT_FREQ", "SSB_HCUT_FREQ (0-67)", "0", RIG_CONF_NUMERIC, { .n = { 0, 67, 1 } } },
    { TOK_SSB_HCUT_SLOPE, "SSB_HCUT_SLOPE", "SSB_HCUT_SLOPE", "SSB_HCUT_SLOPE (0-1)", "0", RIG_CONF_NUMERIC, { .n = { 0, 1, 1 } } },
    { TOK_SSB_USB_OUT_LEVEL, "SSB_USB_OUT_LEVEL", "SSB_USB_OUT_LEVEL", "SSB_USB_OUT_LEVEL (0-100)", "0", RIG_CONF_NUMERIC, { .n = { 0, 100, 1 } } },
    { TOK_SSB_TX_BPF_SEL, "SSB_TX_BPF_SEL", "SSB_TX_BPF_SEL", "SSB_TX_BPF_SEL (0-4)", "0", RIG_CONF_NUMERIC, { .n = { 0, 4, 1 } } },
    { TOK_SSB_MOD_SOURCE, "SSB_MOD_SOURCE", "SSB_MOD_SOURCE", "SSB_MOD_SOURCE (0-3)", "0", RIG_CONF_NUMERIC, { .n = { 0, 3, 1 } } },
    { TOK_SSB_USB_MOD_GAIN, "SSB_USB_MOD_GAIN", "SSB_USB_MOD_GAIN", "SSB_USB_MOD_GAIN (0-100)", "0", RIG_CONF_NUMERIC, { .n = { 0, 100, 1 } } },
    { TOK_SSB_RPTT_SELECT, "SSB_RPTT_SELECT", "SSB_RPTT_SELECT", "SSB_RPTT_SELECT (0-2)", "0", RIG_CONF_NUMERIC, { .n = { 0, 2, 1 } } },
    { TOK_SSB_NAR_WIDTH, "SSB_NAR_WIDTH", "SSB_NAR_WIDTH", "SSB_NAR_WIDTH (0-22)", "0", RIG_CONF_NUMERIC, { .n = { 0, 22, 1 } } },
    { TOK_SSB_CW_AUTO_MODE, "SSB_CW_AUTO_MODE", "SSB_CW_AUTO_MODE", "SSB_CW_AUTO_MODE (0-2)", "0", RIG_CONF_NUMERIC, { .n = { 0, 2, 1 } } },

    /* TX Settings */
    { TOK_TX_AMC_RELEASE, "TX_AMC_RELEASE", "TX_AMC_RELEASE", "TX_AMC_RELEASE (0-2)", "0", RIG_CONF_NUMERIC, { .n = { 0, 2, 1 } } },
    { TOK_TX_EQ1_FREQ, "TX_EQ1_FREQ", "TX_EQ1_FREQ", "TX_EQ1_FREQ (0-7)", "0", RIG_CONF_NUMERIC, { .n = { 0, 7, 1 } } },
    { TOK_TX_EQ1_BWTH, "TX_EQ1_BWTH", "TX_EQ1_BWTH", "TX_EQ1_BWTH (0-10)", "0", RIG_CONF_NUMERIC, { .n = { 0, 10, 1 } } },
    { TOK_TX_EQ2_FREQ, "TX_EQ2_FREQ", "TX_EQ2_FREQ", "TX_EQ2_FREQ (0-9)", "0", RIG_CONF_NUMERIC, { .n = { 0, 9, 1 } } },
    { TOK_TX_EQ2_BWTH, "TX_EQ2_BWTH", "TX_EQ2_BWTH", "TX_EQ2_BWTH (0-10)", "0", RIG_CONF_NUMERIC, { .n = { 0, 10, 1 } } },
    { TOK_TX_EQ3_FREQ, "TX_EQ3_FREQ", "TX_EQ3_FREQ", "TX_EQ3_FREQ (0-18)", "0", RIG_CONF_NUMERIC, { .n = { 0, 18, 1 } } },
    { TOK_TX_EQ3_BWTH, "TX_EQ3_BWTH", "TX_EQ3_BWTH", "TX_EQ3_BWTH (0-10)", "0", RIG_CONF_NUMERIC, { .n = { 0, 10, 1 } } },
    { TOK_TX_P_EQ1_FREQ, "TX_P_EQ1_FREQ", "TX_P_EQ1_FREQ", "TX_P_EQ1_FREQ (0-7)", "0", RIG_CONF_NUMERIC, { .n = { 0, 7, 1 } } },
    { TOK_TX_P_EQ1_BWTH, "TX_P_EQ1_BWTH", "TX_P_EQ1_BWTH", "TX_P_EQ1_BWTH (0-10)", "0", RIG_CONF_NUMERIC, { .n = { 0, 10, 1 } } },
    { TOK_TX_P_EQ2_FREQ, "TX_P_EQ2_FREQ", "TX_P_EQ2_FREQ", "TX_P_EQ2_FREQ (0-9)", "0", RIG_CONF_NUMERIC, { .n = { 0, 9, 1 } } },
    { TOK_TX_P_EQ2_BWTH, "TX_P_EQ2_BWTH", "TX_P_EQ2_BWTH", "TX_P_EQ2_BWTH (0-10)", "0", RIG_CONF_NUMERIC, { .n = { 0, 10, 1 } } },
    { TOK_TX_P_EQ3_FREQ, "TX_P_EQ3_FREQ", "TX_P_EQ3_FREQ", "TX_P_EQ3_FREQ (0-18)", "0", RIG_CONF_NUMERIC, { .n = { 0, 18, 1 } } },
    { TOK_TX_P_EQ3_BWTH, "TX_P_EQ3_BWTH", "TX_P_EQ3_BWTH", "TX_P_EQ3_BWTH (0-10)", "0", RIG_CONF_NUMERIC, { .n = { 0, 10, 1 } } },

    /* TXGEN Settings */
    { TOK_TXGEN_MAX_PWR_BAT, "TXGEN_MAX_PWR_BAT", "TXGEN_MAX_PWR_BAT", "TXGEN_MAX_PWR_BAT (5-60)", "5", RIG_CONF_NUMERIC, { .n = { 5, 60, 1 } } },
    { TOK_TXGEN_QRP_MODE, "TXGEN_QRP_MODE", "TXGEN_QRP_MODE", "TXGEN_QRP_MODE (0-1)", "0", RIG_CONF_NUMERIC, { .n = { 0, 1, 1 } } },
    { TOK_TXGEN_HF_MAX_PWR, "TXGEN_HF_MAX_PWR", "TXGEN_HF_MAX_PWR", "TXGEN_HF_MAX_PWR (5-10)", "5", RIG_CONF_NUMERIC, { .n = { 5, 10, 1 } } },
    { TOK_TXGEN_50M_MAX_PWR, "TXGEN_50M_MAX_PWR", "TXGEN_50M_MAX_PWR", "TXGEN_50M_MAX_PWR (5-10)", "5", RIG_CONF_NUMERIC, { .n = { 5, 10, 1 } } },
    { TOK_TXGEN_70M_MAX_PWR, "TXGEN_70M_MAX_PWR", "TXGEN_70M_MAX_PWR", "TXGEN_70M_MAX_PWR (5-60)", "5", RIG_CONF_NUMERIC, { .n = { 5, 60, 1 } } },

/* Total: 315 parameters */
    { TOK_TXGEN_144M_MAX_PWR, "TXGEN_144M_MAX_PWR", "TXGEN_144M_MAX_PWR", "TXGEN_144M_MAX_PWR (5-100)", "5", RIG_CONF_NUMERIC, { .n = { 5, 100, 1 } } },
    { TOK_TXGEN_430M_MAX_PWR, "TXGEN_430M_MAX_PWR", "TXGEN_430M_MAX_PWR", "TXGEN_430M_MAX_PWR (5-100)", "5", RIG_CONF_NUMERIC, { .n = { 5, 100, 1 } } },
    { TOK_TXGEN_AM_HF_MAX, "TXGEN_AM_HF_MAX", "TXGEN_AM_HF_MAX", "TXGEN_AM_HF_MAX (5-25)", "5", RIG_CONF_NUMERIC, { .n = { 5, 25, 1 } } },
    { TOK_TXGEN_AM_VU_MAX, "TXGEN_AM_VU_MAX", "TXGEN_AM_VU_MAX", "TXGEN_AM_VU_MAX (5-25)", "5", RIG_CONF_NUMERIC, { .n = { 5, 25, 1 } } },
    { TOK_TXGEN_VOX_SELECT, "TXGEN_VOX_SELECT", "TXGEN_VOX_SELECT", "TXGEN_VOX_SELECT (0-2)", "0", RIG_CONF_NUMERIC, { .n = { 0, 2, 1 } } },
    { TOK_TXGEN_EMERG_TX, "TXGEN_EMERG_TX", "TXGEN_EMERG_TX", "TXGEN_EMERG_TX (0-1)", "0", RIG_CONF_NUMERIC, { .n = { 0, 1, 1 } } },
    { TOK_TXGEN_TX_INHIBIT, "TXGEN_TX_INHIBIT", "TXGEN_TX_INHIBIT", "TXGEN_TX_INHIBIT (0-1)", "0", RIG_CONF_NUMERIC, { .n = { 0, 1, 1 } } },
    { TOK_TXGEN_METER_DET, "TXGEN_METER_DET", "TXGEN_METER_DET", "TXGEN_METER_DET (0-1)", "0", RIG_CONF_NUMERIC, { .n = { 0, 1, 1 } } },

    /* UNIT Settings */
    { TOK_UNIT_POSITION, "UNIT_POSITION", "UNIT_POSITION", "UNIT_POSITION (0-1)", "0", RIG_CONF_NUMERIC, { .n = { 0, 1, 1 } } },
    { TOK_UNIT_DISTANCE, "UNIT_DISTANCE", "UNIT_DISTANCE", "UNIT_DISTANCE (0-1)", "0", RIG_CONF_NUMERIC, { .n = { 0, 1, 1 } } },
    { TOK_UNIT_SPEED, "UNIT_SPEED", "UNIT_SPEED", "UNIT_SPEED (0-2)", "0", RIG_CONF_NUMERIC, { .n = { 0, 2, 1 } } },
    { TOK_UNIT_ALTITUDE, "UNIT_ALTITUDE", "UNIT_ALTITUDE", "UNIT_ALTITUDE (0-1)", "0", RIG_CONF_NUMERIC, { .n = { 0, 1, 1 } } },
    { TOK_UNIT_TEMP, "UNIT_TEMP", "UNIT_TEMP", "UNIT_TEMP (0-1)", "0", RIG_CONF_NUMERIC, { .n = { 0, 1, 1 } } },
    { TOK_UNIT_RAIN, "UNIT_RAIN", "UNIT_RAIN", "UNIT_RAIN (0-1)", "0", RIG_CONF_NUMERIC, { .n = { 0, 1, 1 } } },
    { TOK_UNIT_WIND, "UNIT_WIND", "UNIT_WIND", "UNIT_WIND (0-1)", "0", RIG_CONF_NUMERIC, { .n = { 0, 1, 1 } } },

    /* VMI Settings */
    { TOK_VMI_COLOR_VFO, "VMI_COLOR_VFO", "VMI_COLOR_VFO", "VMI_COLOR_VFO (0-3)", "0", RIG_CONF_NUMERIC, { .n = { 0, 3, 1 } } },
    { TOK_VMI_COLOR_MEM, "VMI_COLOR_MEM", "VMI_COLOR_MEM", "VMI_COLOR_MEM (0-3)", "0", RIG_CONF_NUMERIC, { .n = { 0, 3, 1 } } },
    { TOK_VMI_COLOR_CLAR, "VMI_COLOR_CLAR", "VMI_COLOR_CLAR", "VMI_COLOR_CLAR (0-1)", "0", RIG_CONF_NUMERIC, { .n = { 0, 1, 1 } } },

    /* SPA-1 Amplifier Settings (EX0307) - Require SPA-1 hardware */
    { TOK_OPT_TUNER_ANT1, "OPT_TUNER_ANT1", "OPT_TUNER_ANT1", "Tuner ANT1 (0-3, SPA-1 only)", "0", RIG_CONF_NUMERIC, { .n = { 0, 3, 1 } } },
    { TOK_OPT_TUNER_ANT2, "OPT_TUNER_ANT2", "OPT_TUNER_ANT2", "Tuner ANT2 (0-3, SPA-1 only)", "0", RIG_CONF_NUMERIC, { .n = { 0, 3, 1 } } },
    { TOK_OPT_ANT2_OP, "OPT_ANT2_OP", "OPT_ANT2_OP", "ANT2 operation (0-2, SPA-1 only)", "0", RIG_CONF_NUMERIC, { .n = { 0, 2, 1 } } },
    { TOK_OPT_HF_ANT_SEL, "OPT_HF_ANT_SEL", "OPT_HF_ANT_SEL", "HF antenna select (0-1, SPA-1 only)", "0", RIG_CONF_NUMERIC, { .n = { 0, 1, 1 } } },
    { TOK_OPT_HF_MAX_PWR, "OPT_HF_MAX_PWR", "OPT_HF_MAX_PWR", "HF max power 5-100W (SPA-1 only)", "100", RIG_CONF_NUMERIC, { .n = { 5, 100, 1 } } },
    { TOK_OPT_50M_MAX_PWR, "OPT_50M_MAX_PWR", "OPT_50M_MAX_PWR", "50MHz max power 5-100W (SPA-1 only)", "100", RIG_CONF_NUMERIC, { .n = { 5, 100, 1 } } },
    { TOK_OPT_70M_MAX_PWR, "OPT_70M_MAX_PWR", "OPT_70M_MAX_PWR", "70MHz max power 5-50W (SPA-1 only)", "50", RIG_CONF_NUMERIC, { .n = { 5, 50, 1 } } },
    { TOK_OPT_144M_MAX_PWR, "OPT_144M_MAX_PWR", "OPT_144M_MAX_PWR", "144MHz max power 5-50W (SPA-1 only)", "50", RIG_CONF_NUMERIC, { .n = { 5, 50, 1 } } },
    { TOK_OPT_430M_MAX_PWR, "OPT_430M_MAX_PWR", "OPT_430M_MAX_PWR", "430MHz max power 5-50W (SPA-1 only)", "50", RIG_CONF_NUMERIC, { .n = { 5, 50, 1 } } },
    { TOK_OPT_AM_MAX_PWR, "OPT_AM_MAX_PWR", "OPT_AM_MAX_PWR", "AM HF max power 5-25W (SPA-1 only)", "25", RIG_CONF_NUMERIC, { .n = { 5, 25, 1 } } },
    { TOK_OPT_AM_VU_MAX_PWR, "OPT_AM_VU_MAX_PWR", "OPT_AM_VU_MAX_PWR", "AM V/UHF max power 5-13W (SPA-1 only)", "13", RIG_CONF_NUMERIC, { .n = { 5, 13, 1 } } },
    /* GPS Settings - On head unit, not SPA-1 */
    { TOK_OPT_GPS, "OPT_GPS", "OPT_GPS", "GPS enable (0-1)", "0", RIG_CONF_NUMERIC, { .n = { 0, 1, 1 } } },
    { TOK_OPT_GPS_PINNING, "OPT_GPS_PINNING", "OPT_GPS_PINNING", "GPS pinning (0-1)", "0", RIG_CONF_NUMERIC, { .n = { 0, 1, 1 } } },
    { TOK_OPT_GPS_BAUDRATE, "OPT_GPS_BAUDRATE", "OPT_GPS_BAUDRATE", "GPS baudrate (0-4)", "0", RIG_CONF_NUMERIC, { .n = { 0, 4, 1 } } },

    /* Terminating entry */
    { RIG_CONF_END, NULL, }
};

/*
 * ftx1_detect_spa1 - Detect SPA-1 amplifier via VE4 command
 *
 * The VE4; command returns the SPA-1 firmware version if connected.
 * If no SPA-1 is present, the command typically returns '?' or an error.
 *
 * Returns: 1 if SPA-1 detected, 0 otherwise
 */
static int ftx1_detect_spa1(RIG *rig)
{
    struct newcat_priv_data *priv = STATE(rig)->priv;
    int ret;

    rig_debug(RIG_DEBUG_VERBOSE, "%s: checking for SPA-1\n", __func__);

    SNPRINTF(priv->cmd_str, sizeof(priv->cmd_str), "VE4;");
    ret = newcat_get_cmd(rig);

    if (ret != RIG_OK)
    {
        rig_debug(RIG_DEBUG_VERBOSE, "%s: VE4 command failed (no SPA-1)\n", __func__);
        return 0;
    }

    /* VE4 returns version string like "VE4xxx;" if SPA-1 is present */
    if (strlen(priv->ret_data) >= 4 && priv->ret_data[0] == 'V' &&
        priv->ret_data[1] == 'E' && priv->ret_data[2] == '4')
    {
        rig_debug(RIG_DEBUG_VERBOSE, "%s: SPA-1 detected, firmware: %s\n",
                  __func__, priv->ret_data);
        return 1;
    }

    rig_debug(RIG_DEBUG_VERBOSE, "%s: no SPA-1 detected\n", __func__);
    return 0;
}

/*
 * ftx1_probe_field_head_power - Probe Field Head power source (battery vs 12V)
 *
 * The radio enforces hardware power limits based on actual power source.
 * On battery, the radio will not accept power settings above 6W.
 * This probe attempts to set 8W and checks if the radio accepts it.
 *
 * Returns: FTX1_HEAD_FIELD_BATTERY or FTX1_HEAD_FIELD_12V
 */
static int ftx1_probe_field_head_power(RIG *rig)
{
    struct newcat_priv_data *priv = STATE(rig)->priv;
    int ret;
    char original_power[16];
    int power_accepted;

    rig_debug(RIG_DEBUG_VERBOSE, "%s: probing Field Head power source\n", __func__);

    /* Save current power setting */
    SNPRINTF(priv->cmd_str, sizeof(priv->cmd_str), "PC;");
    ret = newcat_get_cmd(rig);

    if (ret != RIG_OK || strlen(priv->ret_data) < 4)
    {
        rig_debug(RIG_DEBUG_WARN, "%s: could not read current power\n", __func__);
        return FTX1_HEAD_FIELD_BATTERY;  /* Default to battery (safer) */
    }

    strncpy(original_power, priv->ret_data, sizeof(original_power) - 1);
    original_power[sizeof(original_power) - 1] = '\0';
    rig_debug(RIG_DEBUG_VERBOSE, "%s: original power: %s\n", __func__, original_power);

    /*
     * Try to set 8W (above battery max of 6W)
     * Use write_block directly to bypass command validation.
     * The radio will clamp to 6W on battery, and we detect this by reading back.
     * If we used newcat_set_cmd, validation would fail (PC1008 != PC1006).
     */
    {
        hamlib_port_t *rp = RIGPORT(rig);
        ret = write_block(rp, (unsigned char *) "PC1008;", 7);

        if (ret != RIG_OK)
        {
            rig_debug(RIG_DEBUG_WARN, "%s: could not send test power command\n",
                      __func__);
            return FTX1_HEAD_FIELD_BATTERY;
        }

        /* Small delay for radio to process the command */
        hl_usleep(50 * 1000);
    }

    /* Read back to see if it was accepted */
    SNPRINTF(priv->cmd_str, sizeof(priv->cmd_str), "PC;");
    ret = newcat_get_cmd(rig);

    if (ret != RIG_OK || strlen(priv->ret_data) < 5)
    {
        rig_debug(RIG_DEBUG_WARN, "%s: could not read back power\n", __func__);
        power_accepted = 0;
    }
    else
    {
        /* Check if power is 8W or above (PC1008 or higher) */
        /* Use strtof() for proper error handling (atof returns 0 on error) */
        char *endptr;
        float power_value = strtof(priv->ret_data + 3, &endptr);

        /* Validate that we parsed at least one character */
        if (endptr == priv->ret_data + 3)
        {
            rig_debug(RIG_DEBUG_WARN, "%s: failed to parse power value from '%s'\n",
                      __func__, priv->ret_data);
            power_accepted = 0;
        }
        else
        {
            power_accepted = (power_value >= 8.0f);
            rig_debug(RIG_DEBUG_VERBOSE, "%s: power readback: %.1f, accepted=%d\n",
                      __func__, power_value, power_accepted);
        }
    }

    /*
     * Restore original power setting using write_block directly.
     * We bypass validation here since we know the original value was valid.
     */
    {
        hamlib_port_t *rp = RIGPORT(rig);
        /* Remove trailing semicolon if present for strlen, then add it back */
        size_t len = strlen(original_power);

        rig_debug(RIG_DEBUG_VERBOSE, "%s: restoring power to '%s'\n", __func__,
                  original_power);
        ret = write_block(rp, (unsigned char *) original_power, len);

        if (ret != RIG_OK)
        {
            /* First attempt failed, wait and retry */
            rig_debug(RIG_DEBUG_WARN, "%s: first restore attempt failed, retrying\n",
                      __func__);
            hl_usleep(100 * 1000);  /* 100ms delay */
            ret = write_block(rp, (unsigned char *) original_power, len);

            if (ret != RIG_OK)
            {
                rig_debug(RIG_DEBUG_ERR,
                          "%s: CRITICAL - failed to restore original power setting '%s'\n",
                          __func__, original_power);
                /* Radio may be left at test power - log prominently but continue */
            }
        }

        /* Small delay to let radio process the restore command */
        hl_usleep(50 * 1000);
    }

    if (power_accepted)
    {
        rig_debug(RIG_DEBUG_VERBOSE, "%s: Field Head on 12V detected (accepted 8W)\n", __func__);
        return FTX1_HEAD_FIELD_12V;
    }
    else
    {
        rig_debug(RIG_DEBUG_VERBOSE, "%s: Field Head on battery detected (rejected 8W)\n", __func__);
        return FTX1_HEAD_FIELD_BATTERY;
    }
}

/*
 * ftx1_detect_head_type - Detect head type using PC command format and power probe
 *
 * Stage 1: PC command format identifies Field Head vs SPA-1
 *   PC1xxx = Field Head (battery or 12V)
 *   PC2xxx = Optima/SPA-1 (5-100W)
 *
 * Stage 2: For Field Head, power probe distinguishes battery vs 12V
 *   - Attempt to set 8W
 *   - If radio accepts 8W → 12V power (0.5-10W)
 *   - If radio rejects 8W → Battery power (0.5-6W)
 *
 * Returns: FTX1_HEAD_FIELD_BATTERY, FTX1_HEAD_FIELD_12V, FTX1_HEAD_SPA1, or FTX1_HEAD_UNKNOWN
 */
static int ftx1_detect_head_type(RIG *rig)
{
    struct newcat_priv_data *priv = STATE(rig)->priv;
    int ret;

    rig_debug(RIG_DEBUG_VERBOSE, "%s: detecting head type via PC command\n", __func__);

    /* Stage 1: Read PC command to determine Field vs SPA-1 */
    SNPRINTF(priv->cmd_str, sizeof(priv->cmd_str), "PC;");
    ret = newcat_get_cmd(rig);

    if (ret != RIG_OK)
    {
        rig_debug(RIG_DEBUG_ERR, "%s: PC query failed\n", __func__);
        return FTX1_HEAD_UNKNOWN;
    }

    rig_debug(RIG_DEBUG_VERBOSE, "%s: PC response: %s\n", __func__, priv->ret_data);

    /* Response format: PC1xxx (Field) or PC2xxx (SPA-1) */
    if (strlen(priv->ret_data) < 4)
    {
        rig_debug(RIG_DEBUG_ERR, "%s: PC response too short: %s\n",
                  __func__, priv->ret_data);
        return FTX1_HEAD_UNKNOWN;
    }

    /* Check head type indicator (position 2) */
    char head_code = priv->ret_data[2];

    if (head_code == '1')
    {
        /* Field Head - probe to determine battery vs 12V */
        rig_debug(RIG_DEBUG_VERBOSE, "%s: Field Head detected, probing power source\n", __func__);
        return ftx1_probe_field_head_power(rig);
    }
    else if (head_code == '2')
    {
        rig_debug(RIG_DEBUG_VERBOSE, "%s: Optima/SPA-1 (5-100W) detected\n", __func__);
        return FTX1_HEAD_SPA1;
    }

    rig_debug(RIG_DEBUG_WARN, "%s: unknown head type code: %c\n", __func__, head_code);
    return FTX1_HEAD_UNKNOWN;
}

/*
 * ftx1_open - FTX-1 specific rig open with head type detection
 *
 * Calls newcat_open, then auto-detects the head configuration:
 *   Stage 1: PC command format (PC1xxx=Field, PC2xxx=SPA-1)
 *   Stage 2: Power probe for Field Head (battery vs 12V)
 *   - Also queries VE4 to confirm SPA-1 presence
 *   - Stores results for use by tuner and power control functions
 */
static int ftx1_open(RIG *rig)
{
    struct newcat_priv_data *priv;
    int ret;

    rig_debug(RIG_DEBUG_VERBOSE, "%s called\n", __func__);

    /* Call the standard newcat open first */
    ret = newcat_open(rig);
    if (ret != RIG_OK)
    {
        return ret;
    }

    priv = STATE(rig)->priv;
    if (!priv)
    {
        rig_debug(RIG_DEBUG_ERR, "%s: newcat priv data not initialized\n", __func__);
        return -RIG_EINTERNAL;
    }

    /* Auto-detect head type and SPA-1 presence - store in per-rig instance */
    priv->ftx1_head_type = ftx1_detect_head_type(rig);
    priv->ftx1_spa1_detected = ftx1_detect_spa1(rig);
    priv->ftx1_detection_done = 1;

    rig_debug(RIG_DEBUG_VERBOSE,
              "%s: detection complete - head_type=%d spa1_detected=%d\n",
              __func__, priv->ftx1_head_type, priv->ftx1_spa1_detected);

    /* Cross-check: if head_type is SPA-1, spa1_detected should also be true */
    if (priv->ftx1_head_type == FTX1_HEAD_SPA1 && !priv->ftx1_spa1_detected)
    {
        rig_debug(RIG_DEBUG_WARN,
                  "%s: PC reports SPA-1 but VE4 detection failed\n", __func__);
    }

    return RIG_OK;
}

/*
 * ftx1_close - FTX-1 specific rig close
 *
 * Resets detection state so reopening the rig will re-detect head configuration.
 * This handles scenarios where the head is swapped while the rig is powered off.
 */
static int ftx1_close(RIG *rig)
{
    struct newcat_priv_data *priv;

    rig_debug(RIG_DEBUG_VERBOSE, "%s called\n", __func__);

    priv = STATE(rig)->priv;
    if (priv)
    {
        /* Reset detection state for next open */
        priv->ftx1_head_type = FTX1_HEAD_UNKNOWN;
        priv->ftx1_spa1_detected = 0;
        priv->ftx1_detection_done = 0;
    }

    return newcat_close(rig);
}

/*
 * ftx1_has_spa1 - Check if Optima/SPA-1 amplifier is present
 *
 * Returns 1 if Optima/SPA-1 detected, 0 otherwise.
 * Used by tuner and power control functions for guardrails.
 */
int ftx1_has_spa1(RIG *rig)
{
    struct newcat_priv_data *priv;

    if (!rig)
    {
        return 0;
    }

    priv = STATE(rig)->priv;
    if (!priv)
    {
        return 0;
    }

    return priv->ftx1_spa1_detected ||
           priv->ftx1_head_type == FTX1_HEAD_SPA1;
}

/*
 * ftx1_get_head_type - Get detected head type
 *
 * Returns FTX1_HEAD_FIELD_BATTERY, FTX1_HEAD_FIELD_12V, FTX1_HEAD_SPA1,
 * or FTX1_HEAD_UNKNOWN
 */
int ftx1_get_head_type(RIG *rig)
{
    struct newcat_priv_data *priv;

    if (!rig)
    {
        return FTX1_HEAD_UNKNOWN;
    }

    priv = STATE(rig)->priv;
    if (!priv)
    {
        return FTX1_HEAD_UNKNOWN;
    }

    return priv->ftx1_head_type;
}

/*
 * ftx1_set_ext_parm - Set FTX-1 extended menu parameter via Hamlib API
 *
 * Called by Hamlib when user executes: rigctl P <param_name> <value>
 * Example: rigctl -m 1051 P KEYER_TYPE 3
 *
 * The token identifies which parameter (from ftx1_ext_parms[] array).
 * val.f contains the value to set (float for RIG_CONF_NUMERIC types).
 *
 * Delegates to ftx1_menu_set_token() which formats and sends EX command.
 */
static int ftx1_set_ext_parm(RIG *rig, hamlib_token_t token, value_t val)
{
    rig_debug(RIG_DEBUG_VERBOSE, "%s: token=0x%lx val=%.0f\n", __func__,
              (unsigned long)token, val.f);

    return ftx1_menu_set_token(rig, token, val);
}

/*
 * ftx1_get_ext_parm - Get FTX-1 extended menu parameter via Hamlib API
 *
 * Called by Hamlib when user executes: rigctl p <param_name>
 * Example: rigctl -m 1051 p KEYER_TYPE
 *
 * The token identifies which parameter (from ftx1_ext_parms[] array).
 * Returns value in val->f (float for RIG_CONF_NUMERIC types).
 *
 * Delegates to ftx1_menu_get_token() which sends EX query and parses response.
 */
static int ftx1_get_ext_parm(RIG *rig, hamlib_token_t token, value_t *val)
{
    rig_debug(RIG_DEBUG_VERBOSE, "%s: token=0x%lx\n", __func__,
              (unsigned long)token);

    return ftx1_menu_get_token(rig, token, val);
}

/* Rig caps structure */
struct rig_caps ftx1_caps = {
    RIG_MODEL(RIG_MODEL_FTX1),
    .model_name = "FTX-1",
    .mfg_name = "Yaesu",
    .version = "20251224.0",  /* Date-based version - added ext_parm support */
    .copyright = "LGPL",
    .status = RIG_STATUS_BETA,  /* Update to stable once complete */
    .rig_type = RIG_TYPE_TRANSCEIVER,
    .ptt_type = RIG_PTT_RIG,
    .dcd_type = RIG_DCD_RIG,  /* FTX-1 RI command P8 provides squelch status */
    .port_type = RIG_PORT_SERIAL,
    .serial_rate_min = 4800,
    .serial_rate_max = 115200,
    .serial_data_bits = 8,
    .serial_stop_bits = 1,  /* 8N1 per FTX-1 default */
    .serial_parity = RIG_PARITY_NONE,
    .serial_handshake = RIG_HANDSHAKE_NONE,
    .write_delay = 0,
    .post_write_delay = 50,  /* Delay after write for FTX-1 response time */
    .timeout = 2000,
    .retry = 3,
    /* Note: ARO, FAGC, DIVERSITY return '?' from FTX-1 firmware - not supported */
    /* DUAL_WATCH uses FR command for dual/single receive mode */
    /* Contour (CO command) is FTX-1 specific - handle via ext_level */
    /* Note: NA command (narrow filter) available via ftx1_set_na_helper/ftx1_get_na_helper but no RIG_FUNC_NAR exists in Hamlib */
    .has_get_func = RIG_FUNC_COMP | RIG_FUNC_VOX | RIG_FUNC_TONE | RIG_FUNC_TSQL | RIG_FUNC_SBKIN | RIG_FUNC_FBKIN | RIG_FUNC_NB | RIG_FUNC_NR | RIG_FUNC_MN | RIG_FUNC_LOCK | RIG_FUNC_MON | RIG_FUNC_TUNER | RIG_FUNC_RIT | RIG_FUNC_XIT | RIG_FUNC_APF | RIG_FUNC_ANF | RIG_FUNC_DUAL_WATCH,
    .has_set_func = RIG_FUNC_COMP | RIG_FUNC_VOX | RIG_FUNC_TONE | RIG_FUNC_TSQL | RIG_FUNC_SBKIN | RIG_FUNC_FBKIN | RIG_FUNC_NB | RIG_FUNC_NR | RIG_FUNC_MN | RIG_FUNC_LOCK | RIG_FUNC_MON | RIG_FUNC_TUNER | RIG_FUNC_RIT | RIG_FUNC_XIT | RIG_FUNC_APF | RIG_FUNC_ANF | RIG_FUNC_DUAL_WATCH,
    /* Note: PBT_IN/PBT_OUT removed - FTX-1 has width (SH P2=0) and IF shift (IS), not true passband tuning */
    /* Note: BALANCE removed - FTX-1 has no audio balance control via CAT */
    .has_get_level = RIG_LEVEL_AF | RIG_LEVEL_RF | RIG_LEVEL_SQL | RIG_LEVEL_IF | RIG_LEVEL_APF | RIG_LEVEL_NB | RIG_LEVEL_NR | RIG_LEVEL_RFPOWER | RIG_LEVEL_MICGAIN | RIG_LEVEL_KEYSPD | RIG_LEVEL_NOTCHF | RIG_LEVEL_COMP | RIG_LEVEL_AGC | RIG_LEVEL_BKINDL | RIG_LEVEL_METER | RIG_LEVEL_VOXGAIN | RIG_LEVEL_VOXDELAY | RIG_LEVEL_ANTIVOX | RIG_LEVEL_RAWSTR | RIG_LEVEL_SWR | RIG_LEVEL_ALC | RIG_LEVEL_STRENGTH | RIG_LEVEL_ATT | RIG_LEVEL_PREAMP | RIG_LEVEL_MONITOR_GAIN | RIG_LEVEL_CWPITCH | RIG_LEVEL_BAND_SELECT,
    .has_set_level = RIG_LEVEL_SET(RIG_LEVEL_AF | RIG_LEVEL_RF | RIG_LEVEL_SQL | RIG_LEVEL_IF | RIG_LEVEL_APF | RIG_LEVEL_NB | RIG_LEVEL_NR | RIG_LEVEL_RFPOWER | RIG_LEVEL_MICGAIN | RIG_LEVEL_KEYSPD | RIG_LEVEL_NOTCHF | RIG_LEVEL_COMP | RIG_LEVEL_AGC | RIG_LEVEL_BKINDL | RIG_LEVEL_METER | RIG_LEVEL_VOXGAIN | RIG_LEVEL_VOXDELAY | RIG_LEVEL_ANTIVOX | RIG_LEVEL_RAWSTR | RIG_LEVEL_SWR | RIG_LEVEL_ALC | RIG_LEVEL_STRENGTH | RIG_LEVEL_ATT | RIG_LEVEL_PREAMP | RIG_LEVEL_MONITOR_GAIN | RIG_LEVEL_CWPITCH | RIG_LEVEL_BAND_SELECT),
    .has_get_parm = RIG_PARM_NONE,
    .has_set_parm = RIG_PARM_NONE,
    .level_gran = {
        /* FTX-1 specific level granularity */
        /* Suppress entries we override, then include common Yaesu defaults */
#define NO_LVL_MICGAIN
#define NO_LVL_VOXGAIN
#define NO_LVL_RFPOWER
#define NO_LVL_MONITOR_GAIN
#define NO_LVL_SQL
#define NO_LVL_CWPITCH
#define NO_LVL_NOTCHF
#define NO_LVL_BKINDL
#define NO_LVL_VOXDELAY
#include "level_gran_yaesu.h"
        /* FTX-1 overrides for levels with 0-100 range instead of 0-255 */
        [LVL_MICGAIN] = { .min = { .f = 0 }, .max = { .f = 1.0 }, .step = { .f = 1.0f / 100.0f } },
        [LVL_VOXGAIN] = { .min = { .f = 0 }, .max = { .f = 1.0 }, .step = { .f = 1.0f / 100.0f } },
        /*
         * RFPOWER level_gran: FTX-1 power ranges vary by head configuration:
         *   Field Battery: 0.5-6W   (min=0.083 normalized)
         *   Field 12V:     0.5-10W  (min=0.05 normalized)
         *   Optima/SPA-1:  5-100W   (min=0.05 normalized)
         *
         * Static level_gran cannot represent all three configs. Using SPA-1/12V
         * minimum (0.05) as default. Actual power limits are enforced at runtime
         * in ftx1_set_power() and ftx1_get_power() based on detected head type.
         * Field Battery users may see 0.05 minimum in apps but hardware will
         * clamp to 0.5W (0.083 normalized).
         */
        [LVL_RFPOWER] = { .min = { .f = 0.05 }, .max = { .f = 1.0 }, .step = { .f = 1.0f / 100.0f } },
        [LVL_MONITOR_GAIN] = { .min = { .f = 0 }, .max = { .f = 1.0 }, .step = { .f = 1.0f / 100.0f } },
        [LVL_SQL] = { .min = { .f = 0 }, .max = { .f = 1.0 }, .step = { .f = 1.0f / 255.0f } },
        /* CW pitch: FTX-1 KP command sets pitch 300-1050 Hz in 10Hz steps */
        [LVL_CWPITCH] = { .min = { .i = 300 }, .max = { .i = 1050 }, .step = { .i = 10 } },
        /* Key speed: FTX-1 uses 4-60 WPM */
        [LVL_KEYSPD] = { .min = { .i = 4 }, .max = { .i = 60 }, .step = { .i = 1 } },
        /* Break-in delay: FTX-1 SD command uses 00-33 (non-linear, see CAT manual) */
        [LVL_BKINDL] = { .min = { .i = 0 }, .max = { .i = 33 }, .step = { .i = 1 } },
        /* VOX delay: FTX-1 VD command uses 00-33 (non-linear, see CAT manual) */
        [LVL_VOXDELAY] = { .min = { .i = 0 }, .max = { .i = 33 }, .step = { .i = 1 } },
        /* Notch frequency: FTX-1 uses 1-3200 Hz */
        [LVL_NOTCHF] = { .min = { .i = 1 }, .max = { .i = 3200 }, .step = { .i = 10 } },
    },
    .ctcss_list = common_ctcss_list,
    .dcs_list = common_dcs_list,
    .extparms = ftx1_ext_parms,
    .str_cal = FTX1_STR_CAL,
    .preamp = {10, 20, RIG_DBLST_END},  /* AMP1=10dB, AMP2=20dB (0=IPO) */
    .attenuator = {12, RIG_DBLST_END},
    .max_rit = Hz(9999),
    .max_xit = Hz(9999),
    .max_ifshift = Hz(1200),
    .agc_level_count = 5,
    .agc_levels = { RIG_AGC_OFF, RIG_AGC_FAST, RIG_AGC_MEDIUM, RIG_AGC_SLOW, RIG_AGC_AUTO },
    .vfo_ops = RIG_OP_CPY | RIG_OP_XCHG | RIG_OP_FROM_VFO | RIG_OP_TO_VFO | RIG_OP_MCL | RIG_OP_TUNE | RIG_OP_UP | RIG_OP_DOWN | RIG_OP_BAND_UP | RIG_OP_BAND_DOWN,
    .targetable_vfo = RIG_TARGETABLE_ALL,
    .transceive = RIG_TRN_OFF,
    .bank_qty = 0,
    .chan_desc_sz = 12,  /* Tag size from manual */
    .chan_list = {
        {1, 99, RIG_MTYPE_MEM, NEWCAT_MEM_CAP},
        {100, 117, RIG_MTYPE_EDGE, NEWCAT_MEM_CAP},  /* PMS */
        {500, 503, RIG_MTYPE_MEM, NEWCAT_MEM_CAP},  /* 60m */
        RIG_CHAN_END,
    },
    /* FTX-1 uses MAIN/SUB VFOs, not A/B. This ensures vfo_fixup() preserves
     * MAIN as MAIN instead of converting to VFOA (which shares cache slot). */
    .rx_range_list1 = {
        {kHz(30), MHz(56), RIG_MODE_ALL, -1, -1, RIG_VFO_MAIN | RIG_VFO_SUB, RIG_ANT_1},
        {MHz(76), MHz(108), RIG_MODE_WFM, -1, -1, RIG_VFO_MAIN | RIG_VFO_SUB, RIG_ANT_1},
        {MHz(118), MHz(164), RIG_MODE_AM | RIG_MODE_FM, -1, -1, RIG_VFO_MAIN | RIG_VFO_SUB, RIG_ANT_1},
        {MHz(430), MHz(470), RIG_MODE_ALL, -1, -1, RIG_VFO_MAIN | RIG_VFO_SUB, RIG_ANT_1},
        RIG_FRNG_END,
    },
    .tx_range_list1 = {
        {kHz(1800), MHz(2) - 1, RIG_MODE_ALL, 5000, 100000, RIG_VFO_MAIN | RIG_VFO_SUB, RIG_ANT_1},
        {MHz(3.5), MHz(4) - 1, RIG_MODE_ALL, 5000, 100000, RIG_VFO_MAIN | RIG_VFO_SUB, RIG_ANT_1},
        {MHz(5.1675), MHz(5.4065), RIG_MODE_ALL, 5000, 100000, RIG_VFO_MAIN | RIG_VFO_SUB, RIG_ANT_1},  /* 60m */
        {MHz(7), MHz(7.3), RIG_MODE_ALL, 5000, 100000, RIG_VFO_MAIN | RIG_VFO_SUB, RIG_ANT_1},
        {MHz(10.1), MHz(10.15), RIG_MODE_ALL, 5000, 100000, RIG_VFO_MAIN | RIG_VFO_SUB, RIG_ANT_1},
        {MHz(14), MHz(14.35), RIG_MODE_ALL, 5000, 100000, RIG_VFO_MAIN | RIG_VFO_SUB, RIG_ANT_1},
        {MHz(18.068), MHz(18.168), RIG_MODE_ALL, 5000, 100000, RIG_VFO_MAIN | RIG_VFO_SUB, RIG_ANT_1},
        {MHz(21), MHz(21.45), RIG_MODE_ALL, 5000, 100000, RIG_VFO_MAIN | RIG_VFO_SUB, RIG_ANT_1},
        {MHz(24.89), MHz(24.99), RIG_MODE_ALL, 5000, 100000, RIG_VFO_MAIN | RIG_VFO_SUB, RIG_ANT_1},
        {MHz(28), MHz(29.7), RIG_MODE_ALL, 5000, 100000, RIG_VFO_MAIN | RIG_VFO_SUB, RIG_ANT_1},
        {MHz(50), MHz(54), RIG_MODE_ALL, 5000, 100000, RIG_VFO_MAIN | RIG_VFO_SUB, RIG_ANT_1},
        {MHz(144), MHz(148), RIG_MODE_ALL, 5000, 50000, RIG_VFO_MAIN | RIG_VFO_SUB, RIG_ANT_1},
        {MHz(430), MHz(450), RIG_MODE_ALL, 5000, 20000, RIG_VFO_MAIN | RIG_VFO_SUB, RIG_ANT_1},
        RIG_FRNG_END,
    },
    .rx_range_list2 = {
        {kHz(30), MHz(56), RIG_MODE_ALL, -1, -1, RIG_VFO_MAIN | RIG_VFO_SUB, RIG_ANT_1},
        {MHz(76), MHz(108), RIG_MODE_WFM, -1, -1, RIG_VFO_MAIN | RIG_VFO_SUB, RIG_ANT_1},
        {MHz(118), MHz(164), RIG_MODE_AM | RIG_MODE_FM, -1, -1, RIG_VFO_MAIN | RIG_VFO_SUB, RIG_ANT_1},
        {MHz(430), MHz(470), RIG_MODE_ALL, -1, -1, RIG_VFO_MAIN | RIG_VFO_SUB, RIG_ANT_1},
        RIG_FRNG_END,
    },
    .tx_range_list2 = {
        {kHz(1800), MHz(2) - 1, RIG_MODE_ALL, 5000, 100000, RIG_VFO_MAIN | RIG_VFO_SUB, RIG_ANT_1},
        {MHz(3.5), MHz(4) - 1, RIG_MODE_ALL, 5000, 100000, RIG_VFO_MAIN | RIG_VFO_SUB, RIG_ANT_1},
        {MHz(5.1675), MHz(5.4065), RIG_MODE_ALL, 5000, 100000, RIG_VFO_MAIN | RIG_VFO_SUB, RIG_ANT_1},  /* 60m */
        {MHz(7), MHz(7.3), RIG_MODE_ALL, 5000, 100000, RIG_VFO_MAIN | RIG_VFO_SUB, RIG_ANT_1},
        {MHz(10.1), MHz(10.15), RIG_MODE_ALL, 5000, 100000, RIG_VFO_MAIN | RIG_VFO_SUB, RIG_ANT_1},
        {MHz(14), MHz(14.35), RIG_MODE_ALL, 5000, 100000, RIG_VFO_MAIN | RIG_VFO_SUB, RIG_ANT_1},
        {MHz(18.068), MHz(18.168), RIG_MODE_ALL, 5000, 100000, RIG_VFO_MAIN | RIG_VFO_SUB, RIG_ANT_1},
        {MHz(21), MHz(21.45), RIG_MODE_ALL, 5000, 100000, RIG_VFO_MAIN | RIG_VFO_SUB, RIG_ANT_1},
        {MHz(24.89), MHz(24.99), RIG_MODE_ALL, 5000, 100000, RIG_VFO_MAIN | RIG_VFO_SUB, RIG_ANT_1},
        {MHz(28), MHz(29.7), RIG_MODE_ALL, 5000, 100000, RIG_VFO_MAIN | RIG_VFO_SUB, RIG_ANT_1},
        {MHz(50), MHz(54), RIG_MODE_ALL, 5000, 100000, RIG_VFO_MAIN | RIG_VFO_SUB, RIG_ANT_1},
        {MHz(144), MHz(148), RIG_MODE_ALL, 5000, 50000, RIG_VFO_MAIN | RIG_VFO_SUB, RIG_ANT_1},
        {MHz(430), MHz(450), RIG_MODE_ALL, 5000, 20000, RIG_VFO_MAIN | RIG_VFO_SUB, RIG_ANT_1},
        RIG_FRNG_END,
    },
    .tuning_steps = {
        {RIG_MODE_SSB | RIG_MODE_CW | RIG_MODE_RTTY | RIG_MODE_PKTLSB | RIG_MODE_PKTUSB, 10},
        {RIG_MODE_AM | RIG_MODE_FM | RIG_MODE_PKTFM, 100},
        {RIG_MODE_WFM, kHz(25)},
        RIG_TS_END,
    },
    .filters = {
        /* FTX-1 filter widths based on SH command codes (Hz):
         *   00=200, 01=250, 02=300, 03=350, 04=400, 05=450, 06=500
         *   07=600, 08=700, 09=800, 10=900, 11=1000, 12=1200, 13=1400
         *   14=1600, 15=1800, 16=2000, 17=2200, 18=2400, 19=2600
         *   20=2800, 21=3000, 22=3200, 23=3400 */
        /* CW modes: 200Hz to 3000Hz */
        {RIG_MODE_CW | RIG_MODE_CWR, Hz(500)},     /* Normal CW */
        {RIG_MODE_CW | RIG_MODE_CWR, Hz(200)},     /* Narrow CW */
        {RIG_MODE_CW | RIG_MODE_CWR, Hz(2400)},    /* Wide CW */
        {RIG_MODE_CW | RIG_MODE_CWR, Hz(1800)},
        {RIG_MODE_CW | RIG_MODE_CWR, Hz(1200)},
        {RIG_MODE_CW | RIG_MODE_CWR, Hz(800)},
        {RIG_MODE_CW | RIG_MODE_CWR, Hz(400)},
        {RIG_MODE_CW | RIG_MODE_CWR, Hz(300)},
        /* SSB modes: 200Hz to 3000Hz */
        {RIG_MODE_SSB, Hz(2400)},    /* Normal SSB */
        {RIG_MODE_SSB, Hz(1800)},    /* Narrow SSB */
        {RIG_MODE_SSB, Hz(3000)},    /* Wide SSB */
        {RIG_MODE_SSB, Hz(2800)},
        {RIG_MODE_SSB, Hz(2600)},
        {RIG_MODE_SSB, Hz(2200)},
        {RIG_MODE_SSB, Hz(2000)},
        {RIG_MODE_SSB, Hz(1600)},
        {RIG_MODE_SSB, Hz(1400)},
        {RIG_MODE_SSB, Hz(1200)},
        /* RTTY/DATA modes: 200Hz to 2400Hz */
        {RIG_MODE_RTTY | RIG_MODE_RTTYR | RIG_MODE_PKTLSB | RIG_MODE_PKTUSB, Hz(500)},
        {RIG_MODE_RTTY | RIG_MODE_RTTYR | RIG_MODE_PKTLSB | RIG_MODE_PKTUSB, Hz(250)},
        {RIG_MODE_RTTY | RIG_MODE_RTTYR | RIG_MODE_PKTLSB | RIG_MODE_PKTUSB, Hz(2400)},
        {RIG_MODE_RTTY | RIG_MODE_RTTYR | RIG_MODE_PKTLSB | RIG_MODE_PKTUSB, Hz(1800)},
        {RIG_MODE_RTTY | RIG_MODE_RTTYR | RIG_MODE_PKTLSB | RIG_MODE_PKTUSB, Hz(1200)},
        /* AM modes */
        {RIG_MODE_AM, Hz(6000)},     /* Normal AM */
        {RIG_MODE_AM, Hz(3000)},     /* Narrow AM */
        {RIG_MODE_AM, Hz(9000)},     /* Wide AM */
        /* FM modes */
        {RIG_MODE_FM | RIG_MODE_PKTFM, Hz(12000)},  /* Normal FM */
        {RIG_MODE_FM | RIG_MODE_PKTFM, Hz(9000)},   /* Narrow FM */
        /* Any mode, any filter */
        {RIG_MODE_SSB | RIG_MODE_CW | RIG_MODE_CWR | RIG_MODE_RTTY | RIG_MODE_RTTYR | RIG_MODE_PKTLSB | RIG_MODE_PKTUSB, RIG_FLT_ANY},
        RIG_FLT_END,
    },
    .priv = (void *)&ftx1_priv_caps,
    .rig_init = newcat_init,
    .rig_cleanup = newcat_cleanup,
    .rig_open = ftx1_open,
    .rig_close = ftx1_close,  /* FTX-1 specific close - resets detection state */
    .set_freq = ftx1_set_freq,
    .get_freq = ftx1_get_freq,
    .set_mode = newcat_set_mode,
    .get_mode = newcat_get_mode,
    .set_vfo = ftx1_set_vfo,
    .get_vfo = ftx1_get_vfo,
    .set_ptt = ftx1_set_ptt_func,
    .get_ptt = ftx1_get_ptt_func,
    .get_dcd = ftx1_get_dcd,  /* RI command P8 provides squelch/DCD status */
    .set_powerstat = ftx1_set_powerstat_func,
    .get_powerstat = ftx1_get_powerstat_func,
    .set_func = ftx1_set_func,
    .get_func = ftx1_get_func,
    .set_level = ftx1_set_level,
    .get_level = ftx1_get_level,
    .set_ctcss_tone = ftx1_set_ctcss_tone_func,
    .get_ctcss_tone = ftx1_get_ctcss_tone_func,
    .set_ctcss_sql = ftx1_set_ctcss_sql_func,
    .get_ctcss_sql = ftx1_get_ctcss_sql_func,
    .set_dcs_code = ftx1_set_dcs_code_func,
    .get_dcs_code = ftx1_get_dcs_code_func,
    .set_dcs_sql = ftx1_set_dcs_sql_func,
    .get_dcs_sql = ftx1_get_dcs_sql_func,
    .send_morse = ftx1_send_morse_func,
    .stop_morse = ftx1_stop_morse_func,
    .wait_morse = ftx1_wait_morse_func,
    .set_trn = ftx1_set_trn_func,
    .get_trn = ftx1_get_trn_func,
    .set_mem = ftx1_set_mem,
    .get_mem = ftx1_get_mem,
    .vfo_op = ftx1_vfo_op,
    .scan = ftx1_set_scan,
    .set_channel = ftx1_set_channel,
    .get_channel = ftx1_get_channel,
    .set_ts = ftx1_set_ts,
    .get_ts = ftx1_get_ts,
    .set_ext_level = newcat_set_ext_level,
    .get_ext_level = newcat_get_ext_level,
    .set_ext_parm = ftx1_set_ext_parm,
    .get_ext_parm = ftx1_get_ext_parm,
    .set_conf = newcat_set_conf,
    .get_conf2 = newcat_get_conf2,
    /*
     * RIT/XIT: Uses FTX-1 CF command for RX/TX clarifier
     */
    .set_rit = ftx1_set_rx_clar,
    .get_rit = ftx1_get_rx_clar,
    .set_xit = ftx1_set_tx_clar,
    .get_xit = ftx1_get_tx_clar,
    .set_rptr_shift = ftx1_set_rptr_shift,
    .get_rptr_shift = ftx1_get_rptr_shift,
    .set_rptr_offs = ftx1_set_rptr_offs,
    .get_rptr_offs = ftx1_get_rptr_offs,
    .set_split_vfo = ftx1_set_split_vfo,
    .get_split_vfo = ftx1_get_split_vfo,
    .set_split_freq = ftx1_set_split_freq,
    .get_split_freq = ftx1_get_split_freq,
    .set_split_mode = ftx1_set_split_mode,
    .get_split_mode = ftx1_get_split_mode,
    .get_info = newcat_get_info,
    .power2mW = newcat_power2mW,
    .mW2power = newcat_mW2power,
    .morse_qsize = 50,
    .hamlib_check_rig_caps = HAMLIB_CHECK_RIG_CAPS
};