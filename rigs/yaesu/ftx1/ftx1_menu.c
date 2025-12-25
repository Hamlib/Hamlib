/*
 * Hamlib Yaesu backend - FTX-1 Extended Menu Implementation
 * Copyright (c) 2025 by Terrell Deppe (KJ5HST)
 *
 * This file implements all EX command access for the FTX-1 CAT interface.
 * Based on Yaesu FTX-1 CAT Operation Reference Manual Rev 2508-C, Table 3.
 */

#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <hamlib/rig.h>
#include "misc.h"
#include "yaesu.h"
#include "newcat.h"
#include "ftx1.h"
#include "ftx1_menu.h"

/*
 * Menu item table
 * Each entry defines: token, name, digits, min, max, flags
 *
 * NOTE: Entries are sorted by token for binary search lookup.
 */
static const struct ftx1_menu_item ftx1_menu_table[] = {
    /*
     * EX01: RADIO SETTING
     */
    /* EX0101: MODE SSB */
    { TOK_SSB_AF_TREBLE,     "SSB_AF_TREBLE",      3, -20, 10, FTX1_MENU_FLAG_SIGNED },
    { TOK_SSB_AF_MID,        "SSB_AF_MID",         3, -20, 10, FTX1_MENU_FLAG_SIGNED },
    { TOK_SSB_AF_BASS,       "SSB_AF_BASS",        3, -20, 10, FTX1_MENU_FLAG_SIGNED },
    { TOK_SSB_AGC_FAST,      "SSB_AGC_FAST",       4, 20, 4000, 0 },
    { TOK_SSB_AGC_MID,       "SSB_AGC_MID",        4, 20, 4000, 0 },
    { TOK_SSB_AGC_SLOW,      "SSB_AGC_SLOW",       4, 20, 4000, 0 },
    { TOK_SSB_LCUT_FREQ,     "SSB_LCUT_FREQ",      2, 0, 19, 0 },
    { TOK_SSB_LCUT_SLOPE,    "SSB_LCUT_SLOPE",     1, 0, 1, 0 },
    { TOK_SSB_HCUT_FREQ,     "SSB_HCUT_FREQ",      2, 0, 67, 0 },
    { TOK_SSB_HCUT_SLOPE,    "SSB_HCUT_SLOPE",     1, 0, 1, 0 },
    { TOK_SSB_USB_OUT_LEVEL, "SSB_USB_OUT_LEVEL",  3, 0, 100, 0 },
    { TOK_SSB_TX_BPF_SEL,    "SSB_TX_BPF_SEL",     1, 0, 4, 0 },
    { TOK_SSB_MOD_SOURCE,    "SSB_MOD_SOURCE",     1, 0, 3, 0 },
    { TOK_SSB_USB_MOD_GAIN,  "SSB_USB_MOD_GAIN",   3, 0, 100, 0 },
    { TOK_SSB_RPTT_SELECT,   "SSB_RPTT_SELECT",    1, 0, 2, 0 },
    { TOK_SSB_NAR_WIDTH,     "SSB_NAR_WIDTH",      2, 0, 22, 0 },
    { TOK_SSB_CW_AUTO_MODE,  "SSB_CW_AUTO_MODE",   1, 0, 2, 0 },

    /* EX0102: MODE AM */
    { TOK_AM_AF_TREBLE,      "AM_AF_TREBLE",       3, -20, 10, FTX1_MENU_FLAG_SIGNED },
    { TOK_AM_AF_MID,         "AM_AF_MID",          3, -20, 10, FTX1_MENU_FLAG_SIGNED },
    { TOK_AM_AF_BASS,        "AM_AF_BASS",         3, -20, 10, FTX1_MENU_FLAG_SIGNED },
    { TOK_AM_AGC_FAST,       "AM_AGC_FAST",        4, 20, 4000, 0 },
    { TOK_AM_AGC_MID,        "AM_AGC_MID",         4, 20, 4000, 0 },
    { TOK_AM_AGC_SLOW,       "AM_AGC_SLOW",        4, 20, 4000, 0 },
    { TOK_AM_LCUT_FREQ,      "AM_LCUT_FREQ",       2, 0, 19, 0 },
    { TOK_AM_LCUT_SLOPE,     "AM_LCUT_SLOPE",      1, 0, 1, 0 },
    { TOK_AM_HCUT_FREQ,      "AM_HCUT_FREQ",       2, 0, 67, 0 },
    { TOK_AM_HCUT_SLOPE,     "AM_HCUT_SLOPE",      1, 0, 1, 0 },
    { TOK_AM_USB_OUT_LEVEL,  "AM_USB_OUT_LEVEL",   3, 0, 100, 0 },
    { TOK_AM_TX_BPF_SEL,     "AM_TX_BPF_SEL",      1, 0, 4, 0 },
    { TOK_AM_MOD_SOURCE,     "AM_MOD_SOURCE",      1, 0, 3, 0 },
    { TOK_AM_USB_MOD_GAIN,   "AM_USB_MOD_GAIN",    3, 0, 100, 0 },
    { TOK_AM_RPTT_SELECT,    "AM_RPTT_SELECT",     1, 0, 2, 0 },

    /* EX0103: MODE FM */
    { TOK_FM_AF_TREBLE,      "FM_AF_TREBLE",       3, -20, 10, FTX1_MENU_FLAG_SIGNED },
    { TOK_FM_AF_MID,         "FM_AF_MID",          3, -20, 10, FTX1_MENU_FLAG_SIGNED },
    { TOK_FM_AF_BASS,        "FM_AF_BASS",         3, -20, 10, FTX1_MENU_FLAG_SIGNED },
    { TOK_FM_AGC_FAST,       "FM_AGC_FAST",        4, 20, 4000, 0 },
    { TOK_FM_AGC_MID,        "FM_AGC_MID",         4, 20, 4000, 0 },
    { TOK_FM_AGC_SLOW,       "FM_AGC_SLOW",        4, 20, 4000, 0 },
    { TOK_FM_LCUT_FREQ,      "FM_LCUT_FREQ",       2, 0, 19, 0 },
    { TOK_FM_LCUT_SLOPE,     "FM_LCUT_SLOPE",      1, 0, 1, 0 },
    { TOK_FM_HCUT_FREQ,      "FM_HCUT_FREQ",       2, 0, 67, 0 },
    { TOK_FM_HCUT_SLOPE,     "FM_HCUT_SLOPE",      1, 0, 1, 0 },
    { TOK_FM_USB_OUT_LEVEL,  "FM_USB_OUT_LEVEL",   3, 0, 100, 0 },
    { TOK_FM_MOD_SOURCE,     "FM_MOD_SOURCE",      1, 0, 3, 0 },
    { TOK_FM_USB_MOD_GAIN,   "FM_USB_MOD_GAIN",    3, 0, 100, 0 },
    { TOK_FM_RPTT_SELECT,    "FM_RPTT_SELECT",     1, 0, 2, 0 },
    { TOK_FM_RPT_SHIFT,      "FM_RPT_SHIFT",       1, 0, 3, 0 },
    { TOK_FM_RPT_SHIFT_28,   "FM_RPT_SHIFT_28",    4, 0, 1000, 0 },
    { TOK_FM_RPT_SHIFT_50,   "FM_RPT_SHIFT_50",    4, 0, 4000, 0 },
    { TOK_FM_RPT_SHIFT_144,  "FM_RPT_SHIFT_144",   4, 0, 100, 0 },
    { TOK_FM_RPT_SHIFT_430,  "FM_RPT_SHIFT_430",   4, 0, 100, 0 },
    { TOK_FM_SQL_TYPE,       "FM_SQL_TYPE",        1, 0, 5, 0 },
    { TOK_FM_TONE_FREQ,      "FM_TONE_FREQ",       2, 0, 49, 0 },
    { TOK_FM_DCS_CODE,       "FM_DCS_CODE",        2, 0, 103, 0 },
    { TOK_FM_DCS_RX_REV,     "FM_DCS_RX_REV",      1, 0, 2, 0 },
    { TOK_FM_DCS_TX_REV,     "FM_DCS_TX_REV",      1, 0, 1, 0 },
    { TOK_FM_PR_FREQ,        "FM_PR_FREQ",         4, 300, 3000, 0 },
    { TOK_FM_DTMF_DELAY,     "FM_DTMF_DELAY",      1, 0, 4, 0 },
    { TOK_FM_DTMF_SPEED,     "FM_DTMF_SPEED",      1, 0, 1, 0 },
    { TOK_FM_DTMF_MEM1,      "FM_DTMF_MEM1",       16, 0, 0, FTX1_MENU_FLAG_STRING },
    { TOK_FM_DTMF_MEM2,      "FM_DTMF_MEM2",       16, 0, 0, FTX1_MENU_FLAG_STRING },
    { TOK_FM_DTMF_MEM3,      "FM_DTMF_MEM3",       16, 0, 0, FTX1_MENU_FLAG_STRING },
    { TOK_FM_DTMF_MEM4,      "FM_DTMF_MEM4",       16, 0, 0, FTX1_MENU_FLAG_STRING },
    { TOK_FM_DTMF_MEM5,      "FM_DTMF_MEM5",       16, 0, 0, FTX1_MENU_FLAG_STRING },
    { TOK_FM_DTMF_MEM6,      "FM_DTMF_MEM6",       16, 0, 0, FTX1_MENU_FLAG_STRING },
    { TOK_FM_DTMF_MEM7,      "FM_DTMF_MEM7",       16, 0, 0, FTX1_MENU_FLAG_STRING },
    { TOK_FM_DTMF_MEM8,      "FM_DTMF_MEM8",       16, 0, 0, FTX1_MENU_FLAG_STRING },
    { TOK_FM_DTMF_MEM9,      "FM_DTMF_MEM9",       16, 0, 0, FTX1_MENU_FLAG_STRING },
    { TOK_FM_DTMF_MEM10,     "FM_DTMF_MEM10",      16, 0, 0, FTX1_MENU_FLAG_STRING },

    /* EX0104: MODE DATA */
    { TOK_DATA_AF_TREBLE,    "DATA_AF_TREBLE",     3, -20, 10, FTX1_MENU_FLAG_SIGNED },
    { TOK_DATA_AF_MID,       "DATA_AF_MID",        3, -20, 10, FTX1_MENU_FLAG_SIGNED },
    { TOK_DATA_AF_BASS,      "DATA_AF_BASS",       3, -20, 10, FTX1_MENU_FLAG_SIGNED },
    { TOK_DATA_AGC_FAST,     "DATA_AGC_FAST",      4, 20, 4000, 0 },
    { TOK_DATA_AGC_MID,      "DATA_AGC_MID",       4, 20, 4000, 0 },
    { TOK_DATA_AGC_SLOW,     "DATA_AGC_SLOW",      4, 20, 4000, 0 },
    { TOK_DATA_LCUT_FREQ,    "DATA_LCUT_FREQ",     2, 0, 19, 0 },
    { TOK_DATA_LCUT_SLOPE,   "DATA_LCUT_SLOPE",    1, 0, 1, 0 },
    { TOK_DATA_HCUT_FREQ,    "DATA_HCUT_FREQ",     2, 0, 67, 0 },
    { TOK_DATA_HCUT_SLOPE,   "DATA_HCUT_SLOPE",    1, 0, 1, 0 },
    { TOK_DATA_USB_OUT_LEVEL,"DATA_USB_OUT_LEVEL", 3, 0, 100, 0 },
    { TOK_DATA_TX_BPF_SEL,   "DATA_TX_BPF_SEL",    1, 0, 4, 0 },
    { TOK_DATA_MOD_SOURCE,   "DATA_MOD_SOURCE",    1, 0, 3, 0 },
    { TOK_DATA_USB_MOD_GAIN, "DATA_USB_MOD_GAIN",  3, 0, 100, 0 },
    { TOK_DATA_RPTT_SELECT,  "DATA_RPTT_SELECT",   1, 0, 2, 0 },
    { TOK_DATA_NAR_WIDTH,    "DATA_NAR_WIDTH",     2, 0, 20, 0 },
    { TOK_DATA_PSK_TONE,     "DATA_PSK_TONE",      1, 0, 2, 0 },
    { TOK_DATA_SHIFT_SSB,    "DATA_SHIFT_SSB",     4, 0, 3000, 0 },

    /* EX0105: MODE RTTY */
    { TOK_RTTY_AF_TREBLE,    "RTTY_AF_TREBLE",     3, -20, 10, FTX1_MENU_FLAG_SIGNED },
    { TOK_RTTY_AF_MID,       "RTTY_AF_MID",        3, -20, 10, FTX1_MENU_FLAG_SIGNED },
    { TOK_RTTY_AF_BASS,      "RTTY_AF_BASS",       3, -20, 10, FTX1_MENU_FLAG_SIGNED },
    { TOK_RTTY_AGC_FAST,     "RTTY_AGC_FAST",      4, 20, 4000, 0 },
    { TOK_RTTY_AGC_MID,      "RTTY_AGC_MID",       4, 20, 4000, 0 },
    { TOK_RTTY_AGC_SLOW,     "RTTY_AGC_SLOW",      4, 20, 4000, 0 },
    { TOK_RTTY_LCUT_FREQ,    "RTTY_LCUT_FREQ",     2, 0, 19, 0 },
    { TOK_RTTY_LCUT_SLOPE,   "RTTY_LCUT_SLOPE",    1, 0, 1, 0 },
    { TOK_RTTY_HCUT_FREQ,    "RTTY_HCUT_FREQ",     2, 0, 67, 0 },
    { TOK_RTTY_HCUT_SLOPE,   "RTTY_HCUT_SLOPE",    1, 0, 1, 0 },
    { TOK_RTTY_USB_OUT_LEVEL,"RTTY_USB_OUT_LEVEL", 3, 0, 100, 0 },
    { TOK_RTTY_RPTT_SELECT,  "RTTY_RPTT_SELECT",   1, 0, 2, 0 },
    { TOK_RTTY_NAR_WIDTH,    "RTTY_NAR_WIDTH",     2, 0, 20, 0 },
    { TOK_RTTY_MARK_FREQ,    "RTTY_MARK_FREQ",     1, 0, 1, 0 },
    { TOK_RTTY_SHIFT_FREQ,   "RTTY_SHIFT_FREQ",    1, 0, 3, 0 },
    { TOK_RTTY_POLARITY_TX,  "RTTY_POLARITY_TX",   1, 0, 1, 0 },

    /* EX0106: DIGITAL */
    { TOK_DIG_POPUP,         "DIG_POPUP",          2, 0, 60, 0 },
    { TOK_DIG_LOC_SERVICE,   "DIG_LOC_SERVICE",    1, 0, 1, 0 },
    { TOK_DIG_STANDBY_BEEP,  "DIG_STANDBY_BEEP",   1, 0, 1, 0 },

    /*
     * EX02: CW SETTING
     */
    /* EX0201: MODE CW */
    { TOK_CW_AF_TREBLE,      "CW_AF_TREBLE",       3, -20, 10, FTX1_MENU_FLAG_SIGNED },
    { TOK_CW_AF_MID,         "CW_AF_MID",          3, -20, 10, FTX1_MENU_FLAG_SIGNED },
    { TOK_CW_AF_BASS,        "CW_AF_BASS",         3, -20, 10, FTX1_MENU_FLAG_SIGNED },
    { TOK_CW_AGC_FAST,       "CW_AGC_FAST",        4, 20, 4000, 0 },
    { TOK_CW_AGC_MID,        "CW_AGC_MID",         4, 20, 4000, 0 },
    { TOK_CW_AGC_SLOW,       "CW_AGC_SLOW",        4, 20, 4000, 0 },
    { TOK_CW_LCUT_FREQ,      "CW_LCUT_FREQ",       2, 0, 19, 0 },
    { TOK_CW_LCUT_SLOPE,     "CW_LCUT_SLOPE",      1, 0, 1, 0 },
    { TOK_CW_HCUT_FREQ,      "CW_HCUT_FREQ",       2, 0, 67, 0 },
    { TOK_CW_HCUT_SLOPE,     "CW_HCUT_SLOPE",      1, 0, 1, 0 },
    { TOK_CW_USB_OUT_LEVEL,  "CW_USB_OUT_LEVEL",   3, 0, 100, 0 },
    { TOK_CW_RPTT_SELECT,    "CW_RPTT_SELECT",     1, 0, 2, 0 },
    { TOK_CW_NAR_WIDTH,      "CW_NAR_WIDTH",       2, 0, 20, 0 },
    { TOK_CW_PC_KEYING,      "CW_PC_KEYING",       1, 0, 2, 0 },
    { TOK_CW_BK_IN_TYPE,     "CW_BK_IN_TYPE",      1, 0, 1, 0 },
    { TOK_CW_FREQ_DISPLAY,   "CW_FREQ_DISPLAY",    1, 0, 1, 0 },
    { TOK_CW_QSK_DELAY,      "CW_QSK_DELAY",       1, 0, 3, 0 },
    { TOK_CW_INDICATOR,      "CW_INDICATOR",       1, 0, 1, 0 },

    /* EX0202: KEYER */
    { TOK_KEYER_TYPE,        "KEYER_TYPE",         1, 0, 5, 0 },
    { TOK_KEYER_DOT_DASH,    "KEYER_DOT_DASH",     1, 0, 1, 0 },
    { TOK_KEYER_WEIGHT,      "KEYER_WEIGHT",       2, 0, 20, 0 },
    { TOK_KEYER_NUM_STYLE,   "KEYER_NUM_STYLE",    1, 0, 6, 0 },
    { TOK_KEYER_CONTEST_NUM, "KEYER_CONTEST_NUM",  4, 1, 9999, 0 },
    { TOK_KEYER_CW_MEM1,     "KEYER_CW_MEM1",      1, 0, 1, 0 },
    { TOK_KEYER_CW_MEM2,     "KEYER_CW_MEM2",      1, 0, 1, 0 },
    { TOK_KEYER_CW_MEM3,     "KEYER_CW_MEM3",      1, 0, 1, 0 },
    { TOK_KEYER_CW_MEM4,     "KEYER_CW_MEM4",      1, 0, 1, 0 },
    { TOK_KEYER_CW_MEM5,     "KEYER_CW_MEM5",      1, 0, 1, 0 },
    { TOK_KEYER_REPEAT_INT,  "KEYER_REPEAT_INT",   2, 1, 60, 0 },

    /*
     * EX03: OPERATION SETTING
     */
    /* EX0301: GENERAL */
    { TOK_GEN_BEEP_LEVEL,    "GEN_BEEP_LEVEL",     3, 0, 100, 0 },
    { TOK_GEN_RF_SQL_VR,     "GEN_RF_SQL_VR",      1, 0, 2, 0 },
    { TOK_GEN_TUN_LIN_PORT,  "GEN_TUN_LIN_PORT",   1, 0, 3, 0 },
    { TOK_GEN_TUNER_SELECT,  "GEN_TUNER_SELECT",   1, 0, 3, 0 },
    { TOK_GEN_CAT1_RATE,     "GEN_CAT1_RATE",      1, 0, 4, 0 },
    { TOK_GEN_CAT1_TIMEOUT,  "GEN_CAT1_TIMEOUT",   1, 0, 3, 0 },
    { TOK_GEN_CAT1_STOP_BIT, "GEN_CAT1_STOP_BIT",  1, 0, 1, 0 },
    { TOK_GEN_CAT2_RATE,     "GEN_CAT2_RATE",      1, 0, 4, 0 },
    { TOK_GEN_CAT2_TIMEOUT,  "GEN_CAT2_TIMEOUT",   1, 0, 3, 0 },
    { TOK_GEN_CAT3_RATE,     "GEN_CAT3_RATE",      1, 0, 4, 0 },
    { TOK_GEN_CAT3_TIMEOUT,  "GEN_CAT3_TIMEOUT",   1, 0, 3, 0 },
    { TOK_GEN_TX_TIMEOUT,    "GEN_TX_TIMEOUT",     2, 0, 30, 0 },
    { TOK_GEN_REF_FREQ_ADJ,  "GEN_REF_FREQ_ADJ",   3, -25, 25, FTX1_MENU_FLAG_SIGNED },
    { TOK_GEN_CHARGE_CTRL,   "GEN_CHARGE_CTRL",    1, 0, 1, 0 },
    { TOK_GEN_SUB_BAND_MUTE, "GEN_SUB_BAND_MUTE",  1, 0, 1, 0 },
    { TOK_GEN_SPEAKER_SEL,   "GEN_SPEAKER_SEL",    1, 0, 2, 0 },
    { TOK_GEN_DITHER,        "GEN_DITHER",         1, 0, 1, 0 },

    /* EX0302: BAND-SCAN */
    { TOK_SCAN_QMB_CH,       "SCAN_QMB_CH",        1, 0, 1, 0 },
    { TOK_SCAN_BAND_STACK,   "SCAN_BAND_STACK",    1, 0, 1, 0 },
    { TOK_SCAN_BAND_EDGE,    "SCAN_BAND_EDGE",     1, 0, 1, 0 },
    { TOK_SCAN_RESUME,       "SCAN_RESUME",        1, 0, 4, 0 },

    /* EX0303: RX-DSP */
    { TOK_DSP_IF_NOTCH_W,    "DSP_IF_NOTCH_W",     1, 0, 1, 0 },
    { TOK_DSP_NB_REJECTION,  "DSP_NB_REJECTION",   1, 0, 2, 0 },
    { TOK_DSP_NB_WIDTH,      "DSP_NB_WIDTH",       1, 0, 2, 0 },
    { TOK_DSP_APF_WIDTH,     "DSP_APF_WIDTH",      1, 0, 2, 0 },
    { TOK_DSP_CONTOUR_LVL,   "DSP_CONTOUR_LVL",    3, -40, 20, FTX1_MENU_FLAG_SIGNED },
    { TOK_DSP_CONTOUR_W,     "DSP_CONTOUR_W",      2, 1, 11, 0 },

    /* EX0304: TX AUDIO */
    { TOK_TX_AMC_RELEASE,    "TX_AMC_RELEASE",     1, 0, 2, 0 },
    { TOK_TX_EQ1_FREQ,       "TX_EQ1_FREQ",        2, 0, 7, 0 },
    { TOK_TX_EQ1_LEVEL,      "TX_EQ1_LEVEL",       3, -20, 10, FTX1_MENU_FLAG_SIGNED },
    { TOK_TX_EQ1_BWTH,       "TX_EQ1_BWTH",        2, 0, 10, 0 },
    { TOK_TX_EQ2_FREQ,       "TX_EQ2_FREQ",        2, 0, 9, 0 },
    { TOK_TX_EQ2_LEVEL,      "TX_EQ2_LEVEL",       3, -20, 10, FTX1_MENU_FLAG_SIGNED },
    { TOK_TX_EQ2_BWTH,       "TX_EQ2_BWTH",        2, 0, 10, 0 },
    { TOK_TX_EQ3_FREQ,       "TX_EQ3_FREQ",        2, 0, 18, 0 },
    { TOK_TX_EQ3_LEVEL,      "TX_EQ3_LEVEL",       3, -20, 10, FTX1_MENU_FLAG_SIGNED },
    { TOK_TX_EQ3_BWTH,       "TX_EQ3_BWTH",        2, 0, 10, 0 },
    { TOK_TX_P_EQ1_FREQ,     "TX_P_EQ1_FREQ",      2, 0, 7, 0 },
    { TOK_TX_P_EQ1_LEVEL,    "TX_P_EQ1_LEVEL",     3, -20, 10, FTX1_MENU_FLAG_SIGNED },
    { TOK_TX_P_EQ1_BWTH,     "TX_P_EQ1_BWTH",      2, 0, 10, 0 },
    { TOK_TX_P_EQ2_FREQ,     "TX_P_EQ2_FREQ",      2, 0, 9, 0 },
    { TOK_TX_P_EQ2_LEVEL,    "TX_P_EQ2_LEVEL",     3, -20, 10, FTX1_MENU_FLAG_SIGNED },
    { TOK_TX_P_EQ2_BWTH,     "TX_P_EQ2_BWTH",      2, 0, 10, 0 },
    { TOK_TX_P_EQ3_FREQ,     "TX_P_EQ3_FREQ",      2, 0, 18, 0 },
    { TOK_TX_P_EQ3_LEVEL,    "TX_P_EQ3_LEVEL",     3, -20, 10, FTX1_MENU_FLAG_SIGNED },
    { TOK_TX_P_EQ3_BWTH,     "TX_P_EQ3_BWTH",      2, 0, 10, 0 },

    /* EX0305: TX GENERAL (Field Head) */
    { TOK_TXGEN_MAX_PWR_BAT, "TXGEN_MAX_PWR_BAT",  3, 5, 60, 0 },
    { TOK_TXGEN_QRP_MODE,    "TXGEN_QRP_MODE",     1, 0, 1, 0 },
    { TOK_TXGEN_HF_MAX_PWR,  "TXGEN_HF_MAX_PWR",   3, 5, 10, 0 },
    { TOK_TXGEN_50M_MAX_PWR, "TXGEN_50M_MAX_PWR",  3, 5, 10, 0 },
    { TOK_TXGEN_70M_MAX_PWR, "TXGEN_70M_MAX_PWR",  3, 5, 60, 0 },
    { TOK_TXGEN_144M_MAX_PWR,"TXGEN_144M_MAX_PWR", 3, 5, 100, 0 },
    { TOK_TXGEN_430M_MAX_PWR,"TXGEN_430M_MAX_PWR", 3, 5, 100, 0 },
    { TOK_TXGEN_AM_HF_MAX,   "TXGEN_AM_HF_MAX",    3, 5, 25, 0 },
    { TOK_TXGEN_AM_VU_MAX,   "TXGEN_AM_VU_MAX",    3, 5, 25, 0 },
    { TOK_TXGEN_VOX_SELECT,  "TXGEN_VOX_SELECT",   1, 0, 2, 0 },
    { TOK_TXGEN_EMERG_TX,    "TXGEN_EMERG_TX",     1, 0, 1, 0 },
    { TOK_TXGEN_TX_INHIBIT,  "TXGEN_TX_INHIBIT",   1, 0, 1, 0 },
    { TOK_TXGEN_METER_DET,   "TXGEN_METER_DET",    1, 0, 1, 0 },

    /* EX0306: KEY/DIAL */
    { TOK_DIAL_SSB_CW_STEP,  "DIAL_SSB_CW_STEP",   1, 0, 2, 0 },
    { TOK_DIAL_RTTY_PSK_STEP,"DIAL_RTTY_PSK_STEP", 1, 0, 2, 0 },
    { TOK_DIAL_FM_STEP,      "DIAL_FM_STEP",       1, 0, 6, 0 },
    { TOK_DIAL_CH_STEP,      "DIAL_CH_STEP",       1, 0, 3, 0 },
    { TOK_DIAL_AM_CH_STEP,   "DIAL_AM_CH_STEP",    1, 0, 5, 0 },
    { TOK_DIAL_FM_CH_STEP,   "DIAL_FM_CH_STEP",    1, 0, 5, 0 },
    { TOK_DIAL_MAIN_STEPS,   "DIAL_MAIN_STEPS",    1, 0, 2, 0 },
    { TOK_DIAL_MIC_P1,       "DIAL_MIC_P1",        2, 0, 20, 0 },
    { TOK_DIAL_MIC_P2,       "DIAL_MIC_P2",        2, 0, 20, 0 },
    { TOK_DIAL_MIC_P3,       "DIAL_MIC_P3",        2, 0, 20, 0 },
    { TOK_DIAL_MIC_P4,       "DIAL_MIC_P4",        2, 0, 20, 0 },
    { TOK_DIAL_MIC_UP,       "DIAL_MIC_UP",        2, 0, 20, 0 },
    { TOK_DIAL_MIC_DOWN,     "DIAL_MIC_DOWN",      2, 0, 20, 0 },
    { TOK_DIAL_MIC_SCAN,     "DIAL_MIC_SCAN",      1, 0, 1, 0 },

    /* EX0307: OPTION (SPA-1) */
    { TOK_OPT_TUNER_ANT1,    "OPT_TUNER_ANT1",     1, 0, 3, FTX1_MENU_FLAG_SPA1 },
    { TOK_OPT_TUNER_ANT2,    "OPT_TUNER_ANT2",     1, 0, 3, FTX1_MENU_FLAG_SPA1 },
    { TOK_OPT_ANT2_OP,       "OPT_ANT2_OP",        1, 0, 2, FTX1_MENU_FLAG_SPA1 },
    { TOK_OPT_HF_ANT_SEL,    "OPT_HF_ANT_SEL",     1, 0, 1, FTX1_MENU_FLAG_SPA1 },
    { TOK_OPT_HF_MAX_PWR,    "OPT_HF_MAX_PWR",     3, 5, 100, FTX1_MENU_FLAG_SPA1 },
    { TOK_OPT_50M_MAX_PWR,   "OPT_50M_MAX_PWR",    3, 5, 100, FTX1_MENU_FLAG_SPA1 },
    { TOK_OPT_70M_MAX_PWR,   "OPT_70M_MAX_PWR",    3, 5, 50, FTX1_MENU_FLAG_SPA1 },
    { TOK_OPT_144M_MAX_PWR,  "OPT_144M_MAX_PWR",   3, 5, 50, FTX1_MENU_FLAG_SPA1 },
    { TOK_OPT_430M_MAX_PWR,  "OPT_430M_MAX_PWR",   3, 5, 50, FTX1_MENU_FLAG_SPA1 },
    { TOK_OPT_AM_MAX_PWR,    "OPT_AM_MAX_PWR",     3, 5, 25, FTX1_MENU_FLAG_SPA1 },
    { TOK_OPT_AM_VU_MAX_PWR, "OPT_AM_VU_MAX_PWR",  3, 5, 13, FTX1_MENU_FLAG_SPA1 },
    /* GPS is on the head unit, NOT the SPA-1 amplifier */
    { TOK_OPT_GPS,           "OPT_GPS",            1, 0, 1, 0 },
    { TOK_OPT_GPS_PINNING,   "OPT_GPS_PINNING",    1, 0, 1, 0 },
    { TOK_OPT_GPS_BAUDRATE,  "OPT_GPS_BAUDRATE",   1, 0, 4, 0 },

    /*
     * EX04: DISPLAY SETTING
     */
    /* EX0401: DISPLAY */
    { TOK_DISP_MY_CALL,      "DISP_MY_CALL",       10, 0, 0, FTX1_MENU_FLAG_STRING },
    { TOK_DISP_MY_CALL_TIME, "DISP_MY_CALL_TIME",  1, 0, 5, 0 },
    { TOK_DISP_POPUP_TIME,   "DISP_POPUP_TIME",    1, 0, 2, 0 },
    { TOK_DISP_SCREEN_SAVER, "DISP_SCREEN_SAVER",  1, 0, 6, 0 },
    { TOK_DISP_SAVER_BAT,    "DISP_SAVER_BAT",     1, 0, 6, 0 },
    { TOK_DISP_SAVER_TYPE,   "DISP_SAVER_TYPE",    1, 0, 2, 0 },
    { TOK_DISP_AUTO_PWR_OFF, "DISP_AUTO_PWR_OFF",  1, 0, 24, 0 },
    { TOK_DISP_LED_DIMMER,   "DISP_LED_DIMMER",    2, 0, 20, 0 },

    /* EX0402: UNIT */
    { TOK_UNIT_POSITION,     "UNIT_POSITION",      1, 0, 1, 0 },
    { TOK_UNIT_DISTANCE,     "UNIT_DISTANCE",      1, 0, 1, 0 },
    { TOK_UNIT_SPEED,        "UNIT_SPEED",         1, 0, 2, 0 },
    { TOK_UNIT_ALTITUDE,     "UNIT_ALTITUDE",      1, 0, 1, 0 },
    { TOK_UNIT_TEMP,         "UNIT_TEMP",          1, 0, 1, 0 },
    { TOK_UNIT_RAIN,         "UNIT_RAIN",          1, 0, 1, 0 },
    { TOK_UNIT_WIND,         "UNIT_WIND",          1, 0, 1, 0 },

    /* EX0403: SCOPE */
    { TOK_SCOPE_RBW,         "SCOPE_RBW",          1, 0, 2, 0 },
    { TOK_SCOPE_CTR,         "SCOPE_CTR",          1, 0, 1, 0 },
    { TOK_SCOPE_2D_SENS,     "SCOPE_2D_SENS",      1, 0, 1, 0 },
    { TOK_SCOPE_3DSS_SENS,   "SCOPE_3DSS_SENS",    1, 0, 1, 0 },
    { TOK_SCOPE_AVERAGE,     "SCOPE_AVERAGE",      1, 0, 3, 0 },

    /* EX0404: VFO IND COLOR */
    { TOK_VMI_COLOR_VFO,     "VMI_COLOR_VFO",      1, 0, 3, 0 },
    { TOK_VMI_COLOR_MEM,     "VMI_COLOR_MEM",      1, 0, 3, 0 },
    { TOK_VMI_COLOR_CLAR,    "VMI_COLOR_CLAR",     1, 0, 1, 0 },

    /*
     * EX05: EXTENSION SETTING
     */
    /* EX0501: DATE&TIME */
    { TOK_DT_TIMEZONE,       "DT_TIMEZONE",        4, -120, 140, FTX1_MENU_FLAG_SIGNED },
    { TOK_DT_GPS_TIME_SET,   "DT_GPS_TIME_SET",    1, 0, 1, 0 },
    { TOK_DT_MY_POSITION,    "DT_MY_POSITION",     1, 0, 1, 0 },

    /*
     * EX06: APRS SETTING
     */
    /* EX0601: GENERAL */
    { TOK_APRS_MODEM_SEL,    "APRS_MODEM_SEL",     1, 0, 3, 0 },
    { TOK_APRS_MODEM_TYPE,   "APRS_MODEM_TYPE",    1, 0, 1, 0 },
    { TOK_APRS_AF_MUTE,      "APRS_AF_MUTE",       1, 0, 1, 0 },
    { TOK_APRS_TX_DELAY,     "APRS_TX_DELAY",      1, 0, 6, 0 },
    { TOK_APRS_CALLSIGN,     "APRS_CALLSIGN",      8, 0, 0, FTX1_MENU_FLAG_STRING },
    /* Items 06-08 don't exist */
    { TOK_APRS_DEST,         "APRS_DEST",          6, 0, 0, FTX1_MENU_FLAG_STRING },  /* Fixed: APYX01 */

    /* EX0602: MSG TEMPLATE */
    { TOK_APRS_MSG_TEXT1,    "APRS_MSG_TEXT1",     16, 0, 0, FTX1_MENU_FLAG_STRING },
    { TOK_APRS_MSG_TEXT2,    "APRS_MSG_TEXT2",     16, 0, 0, FTX1_MENU_FLAG_STRING },
    { TOK_APRS_MSG_TEXT3,    "APRS_MSG_TEXT3",     16, 0, 0, FTX1_MENU_FLAG_STRING },
    { TOK_APRS_MSG_TEXT4,    "APRS_MSG_TEXT4",     16, 0, 0, FTX1_MENU_FLAG_STRING },
    { TOK_APRS_MSG_TEXT5,    "APRS_MSG_TEXT5",     16, 0, 0, FTX1_MENU_FLAG_STRING },
    { TOK_APRS_MSG_TEXT6,    "APRS_MSG_TEXT6",     16, 0, 0, FTX1_MENU_FLAG_STRING },
    { TOK_APRS_MSG_TEXT7,    "APRS_MSG_TEXT7",     16, 0, 0, FTX1_MENU_FLAG_STRING },
    { TOK_APRS_MSG_TEXT8,    "APRS_MSG_TEXT8",     16, 0, 0, FTX1_MENU_FLAG_STRING },

    /* EX0603: MY SYMBOL */
    { TOK_APRS_MY_SYMBOL,    "APRS_MY_SYMBOL",     1, 0, 3, 0 },
    { TOK_APRS_ICON1,        "APRS_ICON1",         2, 0, 0, FTX1_MENU_FLAG_STRING },
    { TOK_APRS_ICON2,        "APRS_ICON2",         2, 0, 0, FTX1_MENU_FLAG_STRING },
    { TOK_APRS_ICON3,        "APRS_ICON3",         2, 0, 0, FTX1_MENU_FLAG_STRING },
    { TOK_APRS_ICON_USER,    "APRS_ICON_USER",     2, 0, 0, FTX1_MENU_FLAG_STRING },

    /* EX0604: DIGI PATH */
    { TOK_APRS_PATH_SEL,     "APRS_PATH_SEL",      1, 0, 2, 0 },

    /*
     * EX07: APRS BEACON
     */
    /* EX0701: BEACON SET */
    { TOK_BCN_TYPE,          "BCN_TYPE",           1, 0, 2, 0 },
    { TOK_BCN_INFO_AMBIG,    "BCN_INFO_AMBIG",     1, 0, 4, 0 },
    { TOK_BCN_SPEED_COURSE,  "BCN_SPEED_COURSE",   1, 0, 1, 0 },
    { TOK_BCN_ALTITUDE,      "BCN_ALTITUDE",       1, 0, 1, 0 },
    { TOK_BCN_POS_COMMENT,   "BCN_POS_COMMENT",    2, 0, 14, 0 },
    { TOK_BCN_EMERGENCY,     "BCN_EMERGENCY",      1, 0, 1, 0 },

    /* EX0702: AUTO BEACON */
    { TOK_BCN_INTERVAL,      "BCN_INTERVAL",       1, 0, 9, 0 },
    { TOK_BCN_PROPORTIONAL,  "BCN_PROPORTIONAL",   1, 0, 1, 0 },
    { TOK_BCN_DECAY,         "BCN_DECAY",          1, 0, 1, 0 },
    { TOK_BCN_AUTO_LOW_SPD,  "BCN_AUTO_LOW_SPD",   2, 1, 99, 0 },
    { TOK_BCN_DELAY,         "BCN_DELAY",          3, 5, 180, 0 },

    /* EX0703: SmartBeac */
    { TOK_SMART_LOW_SPD,     "SMART_LOW_SPD",      2, 2, 30, 0 },
    { TOK_SMART_HIGH_SPD,    "SMART_HIGH_SPD",     2, 3, 90, 0 },
    { TOK_SMART_SLOW_RATE,   "SMART_SLOW_RATE",    3, 1, 100, 0 },
    { TOK_SMART_FAST_RATE,   "SMART_FAST_RATE",    3, 10, 180, 0 },
    { TOK_SMART_TURN_ANGLE,  "SMART_TURN_ANGLE",   2, 5, 90, 0 },
    { TOK_SMART_TURN_SLOPE,  "SMART_TURN_SLOPE",   3, 1, 255, 0 },
    { TOK_SMART_TURN_TIME,   "SMART_TURN_TIME",    3, 5, 180, 0 },

    /* EX0704: BEACON TEXT */
    { TOK_BCN_TEXT_SEL,      "BCN_TEXT_SEL",       1, 0, 5, 0 },
    { TOK_BCN_TX_RATE,       "BCN_TX_RATE",        1, 0, 7, 0 },
    { TOK_BCN_FREQ,          "BCN_FREQ",           1, 0, 2, 0 },
    { TOK_BCN_STATUS1,       "BCN_STATUS1",        60, 0, 0, FTX1_MENU_FLAG_STRING },
    { TOK_BCN_STATUS2,       "BCN_STATUS2",        60, 0, 0, FTX1_MENU_FLAG_STRING },
    { TOK_BCN_STATUS3,       "BCN_STATUS3",        60, 0, 0, FTX1_MENU_FLAG_STRING },
    { TOK_BCN_STATUS4,       "BCN_STATUS4",        60, 0, 0, FTX1_MENU_FLAG_STRING },
    { TOK_BCN_STATUS5,       "BCN_STATUS5",        60, 0, 0, FTX1_MENU_FLAG_STRING },

    /*
     * EX08: APRS FILTER
     */
    /* EX0801: LIST SETTING */
    { TOK_FILT_LIST_SORT,    "FILT_LIST_SORT",     1, 0, 2, 0 },

    /* EX0802: STATION LIST */
    { TOK_FILT_MIC_E,        "FILT_MIC_E",         1, 0, 1, 0 },
    { TOK_FILT_POSITION,     "FILT_POSITION",      1, 0, 1, 0 },
    { TOK_FILT_WEATHER,      "FILT_WEATHER",       1, 0, 1, 0 },
    { TOK_FILT_OBJECT,       "FILT_OBJECT",        1, 0, 1, 0 },
    { TOK_FILT_ITEM,         "FILT_ITEM",          1, 0, 1, 0 },
    { TOK_FILT_STATUS,       "FILT_STATUS",        1, 0, 1, 0 },
    { TOK_FILT_OTHER,        "FILT_OTHER",         1, 0, 1, 0 },
    { TOK_FILT_ALTNET,       "FILT_ALTNET",        1, 0, 1, 0 },

    /* EX0803: POPUP */
    { TOK_FILT_POPUP_BCN,    "FILT_POPUP_BCN",     1, 0, 4, 0 },
    { TOK_FILT_POPUP_MSG,    "FILT_POPUP_MSG",     1, 0, 4, 0 },
    { TOK_FILT_POPUP_MYPACKET,"FILT_POPUP_MYPACKET",1, 0, 1, 0 },

    /* EX0804: RINGER */
    { TOK_RING_TX_BCN,       "RING_TX_BCN",        1, 0, 1, 0 },
    { TOK_RING_RX_BCN,       "RING_RX_BCN",        1, 0, 1, 0 },
    { TOK_RING_TX_MSG,       "RING_TX_MSG",        1, 0, 1, 0 },
    { TOK_RING_RX_MSG,       "RING_RX_MSG",        1, 0, 1, 0 },
    { TOK_RING_MY_PACKET,    "RING_MY_PACKET",     1, 0, 1, 0 },

    /* EX0806: MSG FILTER */
    { TOK_MSGFILT_GRP1,      "MSGFILT_GRP1",       9, 0, 0, FTX1_MENU_FLAG_STRING },
    { TOK_MSGFILT_GRP2,      "MSGFILT_GRP2",       9, 0, 0, FTX1_MENU_FLAG_STRING },
    { TOK_MSGFILT_GRP3,      "MSGFILT_GRP3",       9, 0, 0, FTX1_MENU_FLAG_STRING },
    { TOK_MSGFILT_GRP4,      "MSGFILT_GRP4",       9, 0, 0, FTX1_MENU_FLAG_STRING },
    { TOK_MSGFILT_GRP5,      "MSGFILT_GRP5",       9, 0, 0, FTX1_MENU_FLAG_STRING },
    { TOK_MSGFILT_GRP6,      "MSGFILT_GRP6",       9, 0, 0, FTX1_MENU_FLAG_STRING },
    { TOK_MSGFILT_BULLETIN1, "MSGFILT_BULLETIN1",  9, 0, 0, FTX1_MENU_FLAG_STRING },
    { TOK_MSGFILT_BULLETIN2, "MSGFILT_BULLETIN2",  9, 0, 0, FTX1_MENU_FLAG_STRING },
    { TOK_MSGFILT_BULLETIN3, "MSGFILT_BULLETIN3",  9, 0, 0, FTX1_MENU_FLAG_STRING },

    /*
     * EX09: PRESET
     */
    { TOK_PRE1_NAME,         "PRE1_NAME",          12, 0, 0, FTX1_MENU_FLAG_STRING },
    { TOK_PRE1_CAT1_RATE,    "PRE1_CAT1_RATE",     1, 0, 4, 0 },
    { TOK_PRE1_CAT1_TIMEOUT, "PRE1_CAT1_TIMEOUT",  1, 0, 3, 0 },
    { TOK_PRE1_STOP_BIT,     "PRE1_STOP_BIT",      1, 0, 1, 0 },
    { TOK_PRE1_AGC_FAST,     "PRE1_AGC_FAST",      4, 20, 4000, 0 },
    { TOK_PRE1_AGC_MID,      "PRE1_AGC_MID",       4, 20, 4000, 0 },
    { TOK_PRE1_AGC_SLOW,     "PRE1_AGC_SLOW",      4, 20, 4000, 0 },
    { TOK_PRE1_LCUT_FREQ,    "PRE1_LCUT_FREQ",     2, 0, 19, 0 },
    { TOK_PRE1_LCUT_SLOPE,   "PRE1_LCUT_SLOPE",    1, 0, 1, 0 },
    { TOK_PRE1_HCUT_FREQ,    "PRE1_HCUT_FREQ",     2, 0, 67, 0 },
    { TOK_PRE1_HCUT_SLOPE,   "PRE1_HCUT_SLOPE",    1, 0, 1, 0 },
    { TOK_PRE1_USB_OUT_LVL,  "PRE1_USB_OUT_LVL",   3, 0, 100, 0 },
    { TOK_PRE1_TX_BPF,       "PRE1_TX_BPF",        1, 0, 4, 0 },
    { TOK_PRE1_MOD_SOURCE,   "PRE1_MOD_SOURCE",    1, 0, 3, 0 },
    { TOK_PRE1_USB_MOD_GAIN, "PRE1_USB_MOD_GAIN",  3, 0, 100, 0 },
    { TOK_PRE1_RPTT_SELECT,  "PRE1_RPTT_SELECT",   1, 0, 3, 0 },

    /* PRESET 2 */
    { TOK_PRE2_NAME,         "PRE2_NAME",          12, 0, 0, FTX1_MENU_FLAG_STRING },
    { TOK_PRE2_CAT1_RATE,    "PRE2_CAT1_RATE",     1, 0, 4, 0 },
    { TOK_PRE2_CAT1_TIMEOUT, "PRE2_CAT1_TIMEOUT",  1, 0, 3, 0 },
    { TOK_PRE2_STOP_BIT,     "PRE2_STOP_BIT",      1, 0, 1, 0 },
    { TOK_PRE2_AGC_FAST,     "PRE2_AGC_FAST",      4, 20, 4000, 0 },
    { TOK_PRE2_AGC_MID,      "PRE2_AGC_MID",       4, 20, 4000, 0 },
    { TOK_PRE2_AGC_SLOW,     "PRE2_AGC_SLOW",      4, 20, 4000, 0 },
    { TOK_PRE2_LCUT_FREQ,    "PRE2_LCUT_FREQ",     2, 0, 19, 0 },
    { TOK_PRE2_LCUT_SLOPE,   "PRE2_LCUT_SLOPE",    1, 0, 1, 0 },
    { TOK_PRE2_HCUT_FREQ,    "PRE2_HCUT_FREQ",     2, 0, 67, 0 },
    { TOK_PRE2_HCUT_SLOPE,   "PRE2_HCUT_SLOPE",    1, 0, 1, 0 },
    { TOK_PRE2_USB_OUT_LVL,  "PRE2_USB_OUT_LVL",   3, 0, 100, 0 },
    { TOK_PRE2_TX_BPF,       "PRE2_TX_BPF",        1, 0, 4, 0 },
    { TOK_PRE2_MOD_SOURCE,   "PRE2_MOD_SOURCE",    1, 0, 3, 0 },
    { TOK_PRE2_USB_MOD_GAIN, "PRE2_USB_MOD_GAIN",  3, 0, 100, 0 },
    { TOK_PRE2_RPTT_SELECT,  "PRE2_RPTT_SELECT",   1, 0, 3, 0 },

    /* PRESET 3 */
    { TOK_PRE3_NAME,         "PRE3_NAME",          12, 0, 0, FTX1_MENU_FLAG_STRING },
    { TOK_PRE3_CAT1_RATE,    "PRE3_CAT1_RATE",     1, 0, 4, 0 },
    { TOK_PRE3_CAT1_TIMEOUT, "PRE3_CAT1_TIMEOUT",  1, 0, 3, 0 },
    { TOK_PRE3_STOP_BIT,     "PRE3_STOP_BIT",      1, 0, 1, 0 },
    { TOK_PRE3_AGC_FAST,     "PRE3_AGC_FAST",      4, 20, 4000, 0 },
    { TOK_PRE3_AGC_MID,      "PRE3_AGC_MID",       4, 20, 4000, 0 },
    { TOK_PRE3_AGC_SLOW,     "PRE3_AGC_SLOW",      4, 20, 4000, 0 },
    { TOK_PRE3_LCUT_FREQ,    "PRE3_LCUT_FREQ",     2, 0, 19, 0 },
    { TOK_PRE3_LCUT_SLOPE,   "PRE3_LCUT_SLOPE",    1, 0, 1, 0 },
    { TOK_PRE3_HCUT_FREQ,    "PRE3_HCUT_FREQ",     2, 0, 67, 0 },
    { TOK_PRE3_HCUT_SLOPE,   "PRE3_HCUT_SLOPE",    1, 0, 1, 0 },
    { TOK_PRE3_USB_OUT_LVL,  "PRE3_USB_OUT_LVL",   3, 0, 100, 0 },
    { TOK_PRE3_TX_BPF,       "PRE3_TX_BPF",        1, 0, 4, 0 },
    { TOK_PRE3_MOD_SOURCE,   "PRE3_MOD_SOURCE",    1, 0, 3, 0 },
    { TOK_PRE3_USB_MOD_GAIN, "PRE3_USB_MOD_GAIN",  3, 0, 100, 0 },
    { TOK_PRE3_RPTT_SELECT,  "PRE3_RPTT_SELECT",   1, 0, 3, 0 },

    /* PRESET 4 */
    { TOK_PRE4_NAME,         "PRE4_NAME",          12, 0, 0, FTX1_MENU_FLAG_STRING },
    { TOK_PRE4_CAT1_RATE,    "PRE4_CAT1_RATE",     1, 0, 4, 0 },
    { TOK_PRE4_CAT1_TIMEOUT, "PRE4_CAT1_TIMEOUT",  1, 0, 3, 0 },
    { TOK_PRE4_STOP_BIT,     "PRE4_STOP_BIT",      1, 0, 1, 0 },
    { TOK_PRE4_AGC_FAST,     "PRE4_AGC_FAST",      4, 20, 4000, 0 },
    { TOK_PRE4_AGC_MID,      "PRE4_AGC_MID",       4, 20, 4000, 0 },
    { TOK_PRE4_AGC_SLOW,     "PRE4_AGC_SLOW",      4, 20, 4000, 0 },
    { TOK_PRE4_LCUT_FREQ,    "PRE4_LCUT_FREQ",     2, 0, 19, 0 },
    { TOK_PRE4_LCUT_SLOPE,   "PRE4_LCUT_SLOPE",    1, 0, 1, 0 },
    { TOK_PRE4_HCUT_FREQ,    "PRE4_HCUT_FREQ",     2, 0, 67, 0 },
    { TOK_PRE4_HCUT_SLOPE,   "PRE4_HCUT_SLOPE",    1, 0, 1, 0 },
    { TOK_PRE4_USB_OUT_LVL,  "PRE4_USB_OUT_LVL",   3, 0, 100, 0 },
    { TOK_PRE4_TX_BPF,       "PRE4_TX_BPF",        1, 0, 4, 0 },
    { TOK_PRE4_MOD_SOURCE,   "PRE4_MOD_SOURCE",    1, 0, 3, 0 },
    { TOK_PRE4_USB_MOD_GAIN, "PRE4_USB_MOD_GAIN",  3, 0, 100, 0 },
    { TOK_PRE4_RPTT_SELECT,  "PRE4_RPTT_SELECT",   1, 0, 3, 0 },

    /* PRESET 5 */
    { TOK_PRE5_NAME,         "PRE5_NAME",          12, 0, 0, FTX1_MENU_FLAG_STRING },
    { TOK_PRE5_CAT1_RATE,    "PRE5_CAT1_RATE",     1, 0, 4, 0 },
    { TOK_PRE5_CAT1_TIMEOUT, "PRE5_CAT1_TIMEOUT",  1, 0, 3, 0 },
    { TOK_PRE5_STOP_BIT,     "PRE5_STOP_BIT",      1, 0, 1, 0 },
    { TOK_PRE5_AGC_FAST,     "PRE5_AGC_FAST",      4, 20, 4000, 0 },
    { TOK_PRE5_AGC_MID,      "PRE5_AGC_MID",       4, 20, 4000, 0 },
    { TOK_PRE5_AGC_SLOW,     "PRE5_AGC_SLOW",      4, 20, 4000, 0 },
    { TOK_PRE5_LCUT_FREQ,    "PRE5_LCUT_FREQ",     2, 0, 19, 0 },
    { TOK_PRE5_LCUT_SLOPE,   "PRE5_LCUT_SLOPE",    1, 0, 1, 0 },
    { TOK_PRE5_HCUT_FREQ,    "PRE5_HCUT_FREQ",     2, 0, 67, 0 },
    { TOK_PRE5_HCUT_SLOPE,   "PRE5_HCUT_SLOPE",    1, 0, 1, 0 },
    { TOK_PRE5_USB_OUT_LVL,  "PRE5_USB_OUT_LVL",   3, 0, 100, 0 },
    { TOK_PRE5_TX_BPF,       "PRE5_TX_BPF",        1, 0, 4, 0 },
    { TOK_PRE5_MOD_SOURCE,   "PRE5_MOD_SOURCE",    1, 0, 3, 0 },
    { TOK_PRE5_USB_MOD_GAIN, "PRE5_USB_MOD_GAIN",  3, 0, 100, 0 },
    { TOK_PRE5_RPTT_SELECT,  "PRE5_RPTT_SELECT",   1, 0, 3, 0 },

    /*
     * EX11: BLUETOOTH
     */
    { TOK_BT_ENABLE,         "BT_ENABLE",          1, 0, 1, 0 },
    { TOK_BT_AUDIO,          "BT_AUDIO",           1, 0, 1, 0 },
};

#define FTX1_MENU_TABLE_SIZE (sizeof(ftx1_menu_table) / sizeof(ftx1_menu_table[0]))

/*
 * Find a menu item by token
 */
const struct ftx1_menu_item *ftx1_menu_find_token(token_t token)
{
    size_t i;

    for (i = 0; i < FTX1_MENU_TABLE_SIZE; i++)
    {
        if (ftx1_menu_table[i].token == token)
        {
            return &ftx1_menu_table[i];
        }
    }

    return NULL;
}

/*
 * Get menu item count
 */
int ftx1_get_ext_parm_count(void)
{
    return (int)FTX1_MENU_TABLE_SIZE;
}

/*
 * Set an EX menu parameter by token
 */
int ftx1_menu_set_token(RIG *rig, token_t token, value_t val)
{
    struct newcat_priv_data *priv;
    const struct ftx1_menu_item *item;
    int group, section, menu_item;
    char fmt[32];

    if (!rig)
    {
        return -RIG_EINVAL;
    }

    priv = STATE(rig)->priv;
    if (!priv)
    {
        return -RIG_EINTERNAL;
    }

    item = ftx1_menu_find_token(token);
    if (!item)
    {
        rig_debug(RIG_DEBUG_ERR, "%s: unknown token 0x%lx\n",
                  __func__, (unsigned long)token);
        return -RIG_EINVAL;
    }

    /* Check SPA-1 requirement */
    if ((item->flags & FTX1_MENU_FLAG_SPA1) && !ftx1_has_spa1(rig))
    {
        rig_debug(RIG_DEBUG_WARN, "%s: %s requires SPA-1 amplifier\n",
                  __func__, item->name);
        return -RIG_ENAVAIL;
    }

    /* Extract address from token */
    group = FTX1_TOKEN_GROUP(token);
    section = FTX1_TOKEN_SECTION(token);
    menu_item = FTX1_TOKEN_ITEM(token);

    /* For numeric types, val.f is used; convert to int for validation/command */
    int int_val = (item->flags & FTX1_MENU_FLAG_STRING) ? 0 : (int)val.f;

    rig_debug(RIG_DEBUG_VERBOSE, "%s: %s group=%d section=%d item=%d val=%d\n",
              __func__, item->name, group, section, menu_item, int_val);

    /* Validate value */
    if (!(item->flags & FTX1_MENU_FLAG_STRING))
    {
        if (int_val < item->min_val || int_val > item->max_val)
        {
            rig_debug(RIG_DEBUG_ERR, "%s: value %d out of range [%d, %d]\n",
                      __func__, int_val, item->min_val, item->max_val);
            return -RIG_EINVAL;
        }
    }

    /* Build command string */
    if (item->flags & FTX1_MENU_FLAG_STRING)
    {
        /* String parameter - value is a string pointer */
        SNPRINTF(priv->cmd_str, sizeof(priv->cmd_str), "EX%02d%02d%02d%s;",
                 group, section, menu_item, val.cs ? val.cs : "");
    }
    else if (item->flags & FTX1_MENU_FLAG_SIGNED)
    {
        /* Signed value - need special formatting */
        int v = int_val;
        char sign = (v >= 0) ? '+' : '-';
        if (v < 0) v = -v;

        SNPRINTF(fmt, sizeof(fmt), "EX%%02d%%02d%%02d%c%%0%dd;",
                 sign, item->digits);
        SNPRINTF(priv->cmd_str, sizeof(priv->cmd_str), fmt,
                 group, section, menu_item, v);
    }
    else
    {
        /* Unsigned value */
        SNPRINTF(fmt, sizeof(fmt), "EX%%02d%%02d%%02d%%0%dd;", item->digits);
        SNPRINTF(priv->cmd_str, sizeof(priv->cmd_str), fmt,
                 group, section, menu_item, int_val);
    }

    return newcat_set_cmd(rig);
}

/*
 * Get an EX menu parameter by token
 */
int ftx1_menu_get_token(RIG *rig, token_t token, value_t *val)
{
    struct newcat_priv_data *priv;
    const struct ftx1_menu_item *item;
    int group, section, menu_item;
    int ret;

    if (!rig || !val)
    {
        return -RIG_EINVAL;
    }

    priv = STATE(rig)->priv;
    if (!priv)
    {
        return -RIG_EINTERNAL;
    }

    item = ftx1_menu_find_token(token);
    if (!item)
    {
        rig_debug(RIG_DEBUG_ERR, "%s: unknown token 0x%lx\n",
                  __func__, (unsigned long)token);
        return -RIG_EINVAL;
    }

    /* Check SPA-1 requirement */
    if ((item->flags & FTX1_MENU_FLAG_SPA1) && !ftx1_has_spa1(rig))
    {
        rig_debug(RIG_DEBUG_WARN, "%s: %s requires SPA-1 amplifier\n",
                  __func__, item->name);
        return -RIG_ENAVAIL;
    }

    /* Extract address from token */
    group = FTX1_TOKEN_GROUP(token);
    section = FTX1_TOKEN_SECTION(token);
    menu_item = FTX1_TOKEN_ITEM(token);

    rig_debug(RIG_DEBUG_VERBOSE, "%s: %s group=%d section=%d item=%d\n",
              __func__, item->name, group, section, menu_item);

    /* Build query command */
    SNPRINTF(priv->cmd_str, sizeof(priv->cmd_str), "EX%02d%02d%02d;",
             group, section, menu_item);

    ret = newcat_get_cmd(rig);
    if (ret != RIG_OK)
    {
        return ret;
    }

    /*
     * Parse response: EX P1P1 P2P2 P3P3 value;
     * Value starts at position 8 (after "EX" + 6 address digits)
     */
    if (strlen(priv->ret_data) > 8)
    {
        char *valstr = priv->ret_data + 8;
        char *semi = strchr(valstr, ';');

        if (semi)
        {
            *semi = '\0';
        }

        if (item->flags & FTX1_MENU_FLAG_STRING)
        {
            /* Return as string - caller must provide buffer */
            val->cs = valstr;
        }
        else
        {
            /* Parse numeric value - use val->f for RIG_CONF_NUMERIC compatibility */
            val->f = (float)atoi(valstr);
        }

        rig_debug(RIG_DEBUG_VERBOSE, "%s: %s value=%.0f\n",
                  __func__, item->name,
                  (item->flags & FTX1_MENU_FLAG_STRING) ? 0.0f : val->f);
    }
    else
    {
        rig_debug(RIG_DEBUG_ERR, "%s: response too short '%s'\n",
                  __func__, priv->ret_data);
        return -RIG_EPROTO;
    }

    return RIG_OK;
}

/*
 * Confparams table for Hamlib ext_parm interface
 * This creates entries for all menu items
 */
static struct confparams ftx1_ext_parms[FTX1_MENU_TABLE_SIZE + 1];
static int ftx1_ext_parms_initialized = 0;

/*
 * Initialize the confparams table from our menu table
 */
static void ftx1_init_ext_parms(void)
{
    size_t i;

    if (ftx1_ext_parms_initialized)
    {
        return;
    }

    for (i = 0; i < FTX1_MENU_TABLE_SIZE; i++)
    {
        const struct ftx1_menu_item *item = &ftx1_menu_table[i];

        ftx1_ext_parms[i].token = item->token;
        ftx1_ext_parms[i].name = item->name;
        ftx1_ext_parms[i].label = item->name;
        ftx1_ext_parms[i].tooltip = "";
        ftx1_ext_parms[i].dflt = "";

        if (item->flags & FTX1_MENU_FLAG_STRING)
        {
            ftx1_ext_parms[i].type = RIG_CONF_STRING;
        }
        else
        {
            ftx1_ext_parms[i].type = RIG_CONF_NUMERIC;
            ftx1_ext_parms[i].u.n.min = item->min_val;
            ftx1_ext_parms[i].u.n.max = item->max_val;
            ftx1_ext_parms[i].u.n.step = 1;
        }
    }

    /* Terminating entry */
    ftx1_ext_parms[FTX1_MENU_TABLE_SIZE].token = 0;
    ftx1_ext_parms[FTX1_MENU_TABLE_SIZE].name = NULL;

    ftx1_ext_parms_initialized = 1;
}

/*
 * Get the confparams table
 */
const struct confparams *ftx1_get_ext_parms(void)
{
    ftx1_init_ext_parms();
    return ftx1_ext_parms;
}

/*
 * Convenience functions for commonly used menu items
 */

/* Get/Set CW keyer type (EX020201) */
int ftx1_set_keyer_type(RIG *rig, int type)
{
    value_t val;
    val.i = type;
    return ftx1_menu_set_token(rig, TOK_KEYER_TYPE, val);
}

int ftx1_get_keyer_type(RIG *rig, int *type)
{
    value_t val;
    int ret = ftx1_menu_get_token(rig, TOK_KEYER_TYPE, &val);
    if (ret == RIG_OK)
    {
        *type = val.i;
    }
    return ret;
}

/* Get/Set CW keyer weight (EX020203) */
int ftx1_set_keyer_weight(RIG *rig, int weight)
{
    value_t val;
    val.i = weight;
    return ftx1_menu_set_token(rig, TOK_KEYER_WEIGHT, val);
}

int ftx1_get_keyer_weight(RIG *rig, int *weight)
{
    value_t val;
    int ret = ftx1_menu_get_token(rig, TOK_KEYER_WEIGHT, &val);
    if (ret == RIG_OK)
    {
        *weight = val.i;
    }
    return ret;
}

/* Get/Set paddle reverse (EX020202) */
int ftx1_set_paddle_reverse(RIG *rig, int reverse)
{
    value_t val;
    val.i = reverse ? 1 : 0;
    return ftx1_menu_set_token(rig, TOK_KEYER_DOT_DASH, val);
}

int ftx1_get_paddle_reverse(RIG *rig, int *reverse)
{
    value_t val;
    int ret = ftx1_menu_get_token(rig, TOK_KEYER_DOT_DASH, &val);
    if (ret == RIG_OK)
    {
        *reverse = (val.i == 1) ? 1 : 0;
    }
    return ret;
}

/* Get/Set contest number (EX020205) */
int ftx1_set_contest_number(RIG *rig, int number)
{
    value_t val;
    val.i = number;
    return ftx1_menu_set_token(rig, TOK_KEYER_CONTEST_NUM, val);
}

int ftx1_get_contest_number(RIG *rig, int *number)
{
    value_t val;
    int ret = ftx1_menu_get_token(rig, TOK_KEYER_CONTEST_NUM, &val);
    if (ret == RIG_OK)
    {
        *number = val.i;
    }
    return ret;
}

/* Get/Set SSB dial step (EX030601) */
int ftx1_set_ssb_dial_step(RIG *rig, int step)
{
    value_t val;
    val.i = step;
    return ftx1_menu_set_token(rig, TOK_DIAL_SSB_CW_STEP, val);
}

int ftx1_get_ssb_dial_step(RIG *rig, int *step)
{
    value_t val;
    int ret = ftx1_menu_get_token(rig, TOK_DIAL_SSB_CW_STEP, &val);
    if (ret == RIG_OK)
    {
        *step = val.i;
    }
    return ret;
}

/* Get/Set NB rejection level (EX030302) */
int ftx1_set_nb_rejection(RIG *rig, int level)
{
    value_t val;
    val.i = level;
    return ftx1_menu_set_token(rig, TOK_DSP_NB_REJECTION, val);
}

int ftx1_get_nb_rejection(RIG *rig, int *level)
{
    value_t val;
    int ret = ftx1_menu_get_token(rig, TOK_DSP_NB_REJECTION, &val);
    if (ret == RIG_OK)
    {
        *level = val.i;
    }
    return ret;
}

/* Get/Set APF width (EX030304) */
int ftx1_set_apf_width(RIG *rig, int width)
{
    value_t val;
    val.i = width;
    return ftx1_menu_set_token(rig, TOK_DSP_APF_WIDTH, val);
}

int ftx1_get_apf_width(RIG *rig, int *width)
{
    value_t val;
    int ret = ftx1_menu_get_token(rig, TOK_DSP_APF_WIDTH, &val);
    if (ret == RIG_OK)
    {
        *width = val.i;
    }
    return ret;
}
