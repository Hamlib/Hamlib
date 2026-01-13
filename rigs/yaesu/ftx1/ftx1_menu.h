/*
 * Hamlib Yaesu backend - FTX-1 Extended Menu Definitions
 * Copyright (c) 2025 by Terrell Deppe (KJ5HST)
 *
 * This file defines all EX command tokens for the FTX-1 CAT interface.
 * Based on Yaesu FTX-1 CAT Operation Reference Manual Rev 2508-C, Table 3.
 *
 * EX Command Format: EX P1P1 P2P2 P3P3 P4...;
 *   P1 = Group (01-11)
 *   P2 = Section (01-07)
 *   P3 = Item (01-37)
 *   P4 = Value (variable digits)
 */

#ifndef _FTX1_MENU_H
#define _FTX1_MENU_H

#include <hamlib/rig.h>

/*
 * Token encoding: 0xGGSSII where GG=group, SS=section, II=item
 * This allows direct encoding of the EX command address in the token.
 */
#define FTX1_TOKEN(g, s, i)  (((g) << 16) | ((s) << 8) | (i))
#define FTX1_TOKEN_GROUP(t)  (((t) >> 16) & 0xFF)
#define FTX1_TOKEN_SECTION(t) (((t) >> 8) & 0xFF)
#define FTX1_TOKEN_ITEM(t)   ((t) & 0xFF)

/*
 * ===========================================================================
 * EX01: RADIO SETTING
 * ===========================================================================
 */

/* EX0101: MODE SSB */
#define TOK_SSB_AF_TREBLE       FTX1_TOKEN(1, 1, 1)
#define TOK_SSB_AF_MID          FTX1_TOKEN(1, 1, 2)
#define TOK_SSB_AF_BASS         FTX1_TOKEN(1, 1, 3)
#define TOK_SSB_AGC_FAST        FTX1_TOKEN(1, 1, 4)
#define TOK_SSB_AGC_MID         FTX1_TOKEN(1, 1, 5)
#define TOK_SSB_AGC_SLOW        FTX1_TOKEN(1, 1, 6)
#define TOK_SSB_LCUT_FREQ       FTX1_TOKEN(1, 1, 7)
#define TOK_SSB_LCUT_SLOPE      FTX1_TOKEN(1, 1, 8)
#define TOK_SSB_HCUT_FREQ       FTX1_TOKEN(1, 1, 9)
#define TOK_SSB_HCUT_SLOPE      FTX1_TOKEN(1, 1, 10)
#define TOK_SSB_USB_OUT_LEVEL   FTX1_TOKEN(1, 1, 11)
#define TOK_SSB_TX_BPF_SEL      FTX1_TOKEN(1, 1, 12)
#define TOK_SSB_MOD_SOURCE      FTX1_TOKEN(1, 1, 13)
#define TOK_SSB_USB_MOD_GAIN    FTX1_TOKEN(1, 1, 14)
#define TOK_SSB_RPTT_SELECT     FTX1_TOKEN(1, 1, 15)
#define TOK_SSB_NAR_WIDTH       FTX1_TOKEN(1, 1, 16)
#define TOK_SSB_CW_AUTO_MODE    FTX1_TOKEN(1, 1, 17)

/* EX0102: MODE AM */
#define TOK_AM_AF_TREBLE        FTX1_TOKEN(1, 2, 1)
#define TOK_AM_AF_MID           FTX1_TOKEN(1, 2, 2)
#define TOK_AM_AF_BASS          FTX1_TOKEN(1, 2, 3)
#define TOK_AM_AGC_FAST         FTX1_TOKEN(1, 2, 4)
#define TOK_AM_AGC_MID          FTX1_TOKEN(1, 2, 5)
#define TOK_AM_AGC_SLOW         FTX1_TOKEN(1, 2, 6)
#define TOK_AM_LCUT_FREQ        FTX1_TOKEN(1, 2, 7)
#define TOK_AM_LCUT_SLOPE       FTX1_TOKEN(1, 2, 8)
#define TOK_AM_HCUT_FREQ        FTX1_TOKEN(1, 2, 9)
#define TOK_AM_HCUT_SLOPE       FTX1_TOKEN(1, 2, 10)
#define TOK_AM_USB_OUT_LEVEL    FTX1_TOKEN(1, 2, 11)
#define TOK_AM_TX_BPF_SEL       FTX1_TOKEN(1, 2, 12)
#define TOK_AM_MOD_SOURCE       FTX1_TOKEN(1, 2, 13)
#define TOK_AM_USB_MOD_GAIN     FTX1_TOKEN(1, 2, 14)
#define TOK_AM_RPTT_SELECT      FTX1_TOKEN(1, 2, 15)

/* EX0103: MODE FM */
#define TOK_FM_AF_TREBLE        FTX1_TOKEN(1, 3, 1)
#define TOK_FM_AF_MID           FTX1_TOKEN(1, 3, 2)
#define TOK_FM_AF_BASS          FTX1_TOKEN(1, 3, 3)
#define TOK_FM_AGC_FAST         FTX1_TOKEN(1, 3, 4)
#define TOK_FM_AGC_MID          FTX1_TOKEN(1, 3, 5)
#define TOK_FM_AGC_SLOW         FTX1_TOKEN(1, 3, 6)
#define TOK_FM_LCUT_FREQ        FTX1_TOKEN(1, 3, 7)
#define TOK_FM_LCUT_SLOPE       FTX1_TOKEN(1, 3, 8)
#define TOK_FM_HCUT_FREQ        FTX1_TOKEN(1, 3, 9)
#define TOK_FM_HCUT_SLOPE       FTX1_TOKEN(1, 3, 10)
#define TOK_FM_USB_OUT_LEVEL    FTX1_TOKEN(1, 3, 11)
#define TOK_FM_MOD_SOURCE       FTX1_TOKEN(1, 3, 12)
#define TOK_FM_USB_MOD_GAIN     FTX1_TOKEN(1, 3, 13)
#define TOK_FM_RPTT_SELECT      FTX1_TOKEN(1, 3, 14)
#define TOK_FM_RPT_SHIFT        FTX1_TOKEN(1, 3, 15)
#define TOK_FM_RPT_SHIFT_28     FTX1_TOKEN(1, 3, 16)
#define TOK_FM_RPT_SHIFT_50     FTX1_TOKEN(1, 3, 17)
#define TOK_FM_RPT_SHIFT_144    FTX1_TOKEN(1, 3, 18)
#define TOK_FM_RPT_SHIFT_430    FTX1_TOKEN(1, 3, 19)
#define TOK_FM_SQL_TYPE         FTX1_TOKEN(1, 3, 20)
#define TOK_FM_TONE_FREQ        FTX1_TOKEN(1, 3, 21)
#define TOK_FM_DCS_CODE         FTX1_TOKEN(1, 3, 22)
#define TOK_FM_DCS_RX_REV       FTX1_TOKEN(1, 3, 23)
#define TOK_FM_DCS_TX_REV       FTX1_TOKEN(1, 3, 24)
#define TOK_FM_PR_FREQ          FTX1_TOKEN(1, 3, 25)
#define TOK_FM_DTMF_DELAY       FTX1_TOKEN(1, 3, 26)
#define TOK_FM_DTMF_SPEED       FTX1_TOKEN(1, 3, 27)
#define TOK_FM_DTMF_MEM1        FTX1_TOKEN(1, 3, 28)
#define TOK_FM_DTMF_MEM2        FTX1_TOKEN(1, 3, 29)
#define TOK_FM_DTMF_MEM3        FTX1_TOKEN(1, 3, 30)
#define TOK_FM_DTMF_MEM4        FTX1_TOKEN(1, 3, 31)
#define TOK_FM_DTMF_MEM5        FTX1_TOKEN(1, 3, 32)
#define TOK_FM_DTMF_MEM6        FTX1_TOKEN(1, 3, 33)
#define TOK_FM_DTMF_MEM7        FTX1_TOKEN(1, 3, 34)
#define TOK_FM_DTMF_MEM8        FTX1_TOKEN(1, 3, 35)
#define TOK_FM_DTMF_MEM9        FTX1_TOKEN(1, 3, 36)
#define TOK_FM_DTMF_MEM10       FTX1_TOKEN(1, 3, 37)

/* EX0104: MODE DATA */
#define TOK_DATA_AF_TREBLE      FTX1_TOKEN(1, 4, 1)
#define TOK_DATA_AF_MID         FTX1_TOKEN(1, 4, 2)
#define TOK_DATA_AF_BASS        FTX1_TOKEN(1, 4, 3)
#define TOK_DATA_AGC_FAST       FTX1_TOKEN(1, 4, 4)
#define TOK_DATA_AGC_MID        FTX1_TOKEN(1, 4, 5)
#define TOK_DATA_AGC_SLOW       FTX1_TOKEN(1, 4, 6)
#define TOK_DATA_LCUT_FREQ      FTX1_TOKEN(1, 4, 7)
#define TOK_DATA_LCUT_SLOPE     FTX1_TOKEN(1, 4, 8)
#define TOK_DATA_HCUT_FREQ      FTX1_TOKEN(1, 4, 9)
#define TOK_DATA_HCUT_SLOPE     FTX1_TOKEN(1, 4, 10)
#define TOK_DATA_USB_OUT_LEVEL  FTX1_TOKEN(1, 4, 11)
#define TOK_DATA_TX_BPF_SEL     FTX1_TOKEN(1, 4, 12)
#define TOK_DATA_MOD_SOURCE     FTX1_TOKEN(1, 4, 13)
#define TOK_DATA_USB_MOD_GAIN   FTX1_TOKEN(1, 4, 14)
#define TOK_DATA_RPTT_SELECT    FTX1_TOKEN(1, 4, 15)
#define TOK_DATA_NAR_WIDTH      FTX1_TOKEN(1, 4, 16)
#define TOK_DATA_PSK_TONE       FTX1_TOKEN(1, 4, 17)
#define TOK_DATA_SHIFT_SSB      FTX1_TOKEN(1, 4, 18)

/* EX0105: MODE RTTY */
#define TOK_RTTY_AF_TREBLE      FTX1_TOKEN(1, 5, 1)
#define TOK_RTTY_AF_MID         FTX1_TOKEN(1, 5, 2)
#define TOK_RTTY_AF_BASS        FTX1_TOKEN(1, 5, 3)
#define TOK_RTTY_AGC_FAST       FTX1_TOKEN(1, 5, 4)
#define TOK_RTTY_AGC_MID        FTX1_TOKEN(1, 5, 5)
#define TOK_RTTY_AGC_SLOW       FTX1_TOKEN(1, 5, 6)
#define TOK_RTTY_LCUT_FREQ      FTX1_TOKEN(1, 5, 7)
#define TOK_RTTY_LCUT_SLOPE     FTX1_TOKEN(1, 5, 8)
#define TOK_RTTY_HCUT_FREQ      FTX1_TOKEN(1, 5, 9)
#define TOK_RTTY_HCUT_SLOPE     FTX1_TOKEN(1, 5, 10)
#define TOK_RTTY_USB_OUT_LEVEL  FTX1_TOKEN(1, 5, 11)
#define TOK_RTTY_RPTT_SELECT    FTX1_TOKEN(1, 5, 12)
#define TOK_RTTY_NAR_WIDTH      FTX1_TOKEN(1, 5, 13)
#define TOK_RTTY_MARK_FREQ      FTX1_TOKEN(1, 5, 14)
#define TOK_RTTY_SHIFT_FREQ     FTX1_TOKEN(1, 5, 15)
#define TOK_RTTY_POLARITY_TX    FTX1_TOKEN(1, 5, 16)

/* EX0106: DIGITAL */
#define TOK_DIG_POPUP           FTX1_TOKEN(1, 6, 1)
#define TOK_DIG_LOC_SERVICE     FTX1_TOKEN(1, 6, 2)
#define TOK_DIG_STANDBY_BEEP    FTX1_TOKEN(1, 6, 3)

/*
 * ===========================================================================
 * EX02: CW SETTING
 * ===========================================================================
 */

/* EX0201: MODE CW */
#define TOK_CW_AF_TREBLE        FTX1_TOKEN(2, 1, 1)
#define TOK_CW_AF_MID           FTX1_TOKEN(2, 1, 2)
#define TOK_CW_AF_BASS          FTX1_TOKEN(2, 1, 3)
#define TOK_CW_AGC_FAST         FTX1_TOKEN(2, 1, 4)
#define TOK_CW_AGC_MID          FTX1_TOKEN(2, 1, 5)
#define TOK_CW_AGC_SLOW         FTX1_TOKEN(2, 1, 6)
#define TOK_CW_LCUT_FREQ        FTX1_TOKEN(2, 1, 7)
#define TOK_CW_LCUT_SLOPE       FTX1_TOKEN(2, 1, 8)
#define TOK_CW_HCUT_FREQ        FTX1_TOKEN(2, 1, 9)
#define TOK_CW_HCUT_SLOPE       FTX1_TOKEN(2, 1, 10)
#define TOK_CW_USB_OUT_LEVEL    FTX1_TOKEN(2, 1, 11)
#define TOK_CW_RPTT_SELECT      FTX1_TOKEN(2, 1, 12)
#define TOK_CW_NAR_WIDTH        FTX1_TOKEN(2, 1, 13)
#define TOK_CW_PC_KEYING        FTX1_TOKEN(2, 1, 14)
#define TOK_CW_BK_IN_TYPE       FTX1_TOKEN(2, 1, 15)
#define TOK_CW_FREQ_DISPLAY     FTX1_TOKEN(2, 1, 16)
#define TOK_CW_QSK_DELAY        FTX1_TOKEN(2, 1, 17)
#define TOK_CW_INDICATOR        FTX1_TOKEN(2, 1, 18)

/* EX0202: KEYER */
#define TOK_KEYER_TYPE          FTX1_TOKEN(2, 2, 1)
#define TOK_KEYER_DOT_DASH      FTX1_TOKEN(2, 2, 2)
#define TOK_KEYER_WEIGHT        FTX1_TOKEN(2, 2, 3)
#define TOK_KEYER_NUM_STYLE     FTX1_TOKEN(2, 2, 4)
#define TOK_KEYER_CONTEST_NUM   FTX1_TOKEN(2, 2, 5)
#define TOK_KEYER_CW_MEM1       FTX1_TOKEN(2, 2, 6)
#define TOK_KEYER_CW_MEM2       FTX1_TOKEN(2, 2, 7)
#define TOK_KEYER_CW_MEM3       FTX1_TOKEN(2, 2, 8)
#define TOK_KEYER_CW_MEM4       FTX1_TOKEN(2, 2, 9)
#define TOK_KEYER_CW_MEM5       FTX1_TOKEN(2, 2, 10)
#define TOK_KEYER_REPEAT_INT    FTX1_TOKEN(2, 2, 11)

/*
 * ===========================================================================
 * EX03: OPERATION SETTING
 * ===========================================================================
 */

/* EX0301: GENERAL */
#define TOK_GEN_BEEP_LEVEL      FTX1_TOKEN(3, 1, 1)
#define TOK_GEN_RF_SQL_VR       FTX1_TOKEN(3, 1, 2)
#define TOK_GEN_TUN_LIN_PORT    FTX1_TOKEN(3, 1, 3)
#define TOK_GEN_TUNER_SELECT    FTX1_TOKEN(3, 1, 4)
#define TOK_GEN_CAT1_RATE       FTX1_TOKEN(3, 1, 5)
#define TOK_GEN_CAT1_TIMEOUT    FTX1_TOKEN(3, 1, 6)
#define TOK_GEN_CAT1_STOP_BIT   FTX1_TOKEN(3, 1, 7)
#define TOK_GEN_CAT2_RATE       FTX1_TOKEN(3, 1, 8)
#define TOK_GEN_CAT2_TIMEOUT    FTX1_TOKEN(3, 1, 9)
#define TOK_GEN_CAT3_RATE       FTX1_TOKEN(3, 1, 10)
#define TOK_GEN_CAT3_TIMEOUT    FTX1_TOKEN(3, 1, 11)
#define TOK_GEN_TX_TIMEOUT      FTX1_TOKEN(3, 1, 12)
#define TOK_GEN_REF_FREQ_ADJ    FTX1_TOKEN(3, 1, 13)
#define TOK_GEN_CHARGE_CTRL     FTX1_TOKEN(3, 1, 14)
#define TOK_GEN_SUB_BAND_MUTE   FTX1_TOKEN(3, 1, 15)
#define TOK_GEN_SPEAKER_SEL     FTX1_TOKEN(3, 1, 16)
#define TOK_GEN_DITHER          FTX1_TOKEN(3, 1, 17)

/* EX0302: BAND-SCAN */
#define TOK_SCAN_QMB_CH         FTX1_TOKEN(3, 2, 1)
#define TOK_SCAN_BAND_STACK     FTX1_TOKEN(3, 2, 2)
#define TOK_SCAN_BAND_EDGE      FTX1_TOKEN(3, 2, 3)
#define TOK_SCAN_RESUME         FTX1_TOKEN(3, 2, 4)

/* EX0303: RX-DSP */
#define TOK_DSP_IF_NOTCH_W      FTX1_TOKEN(3, 3, 1)
#define TOK_DSP_NB_REJECTION    FTX1_TOKEN(3, 3, 2)
#define TOK_DSP_NB_WIDTH        FTX1_TOKEN(3, 3, 3)
#define TOK_DSP_APF_WIDTH       FTX1_TOKEN(3, 3, 4)
#define TOK_DSP_CONTOUR_LVL     FTX1_TOKEN(3, 3, 5)
#define TOK_DSP_CONTOUR_W       FTX1_TOKEN(3, 3, 6)

/* EX0304: TX AUDIO */
#define TOK_TX_AMC_RELEASE      FTX1_TOKEN(3, 4, 1)
#define TOK_TX_EQ1_FREQ         FTX1_TOKEN(3, 4, 2)
#define TOK_TX_EQ1_LEVEL        FTX1_TOKEN(3, 4, 3)
#define TOK_TX_EQ1_BWTH         FTX1_TOKEN(3, 4, 4)
#define TOK_TX_EQ2_FREQ         FTX1_TOKEN(3, 4, 5)
#define TOK_TX_EQ2_LEVEL        FTX1_TOKEN(3, 4, 6)
#define TOK_TX_EQ2_BWTH         FTX1_TOKEN(3, 4, 7)
#define TOK_TX_EQ3_FREQ         FTX1_TOKEN(3, 4, 8)
#define TOK_TX_EQ3_LEVEL        FTX1_TOKEN(3, 4, 9)
#define TOK_TX_EQ3_BWTH         FTX1_TOKEN(3, 4, 10)
#define TOK_TX_P_EQ1_FREQ       FTX1_TOKEN(3, 4, 11)
#define TOK_TX_P_EQ1_LEVEL      FTX1_TOKEN(3, 4, 12)
#define TOK_TX_P_EQ1_BWTH       FTX1_TOKEN(3, 4, 13)
#define TOK_TX_P_EQ2_FREQ       FTX1_TOKEN(3, 4, 14)
#define TOK_TX_P_EQ2_LEVEL      FTX1_TOKEN(3, 4, 15)
#define TOK_TX_P_EQ2_BWTH       FTX1_TOKEN(3, 4, 16)
#define TOK_TX_P_EQ3_FREQ       FTX1_TOKEN(3, 4, 17)
#define TOK_TX_P_EQ3_LEVEL      FTX1_TOKEN(3, 4, 18)
#define TOK_TX_P_EQ3_BWTH       FTX1_TOKEN(3, 4, 19)

/* EX0305: TX GENERAL (Field Head power limits) */
#define TOK_TXGEN_MAX_PWR_BAT   FTX1_TOKEN(3, 5, 1)
#define TOK_TXGEN_QRP_MODE      FTX1_TOKEN(3, 5, 2)
#define TOK_TXGEN_HF_MAX_PWR    FTX1_TOKEN(3, 5, 3)
#define TOK_TXGEN_50M_MAX_PWR   FTX1_TOKEN(3, 5, 4)
#define TOK_TXGEN_70M_MAX_PWR   FTX1_TOKEN(3, 5, 5)
#define TOK_TXGEN_144M_MAX_PWR  FTX1_TOKEN(3, 5, 6)
#define TOK_TXGEN_430M_MAX_PWR  FTX1_TOKEN(3, 5, 7)
#define TOK_TXGEN_AM_HF_MAX     FTX1_TOKEN(3, 5, 8)
#define TOK_TXGEN_AM_VU_MAX     FTX1_TOKEN(3, 5, 9)
#define TOK_TXGEN_VOX_SELECT    FTX1_TOKEN(3, 5, 10)
#define TOK_TXGEN_EMERG_TX      FTX1_TOKEN(3, 5, 11)
#define TOK_TXGEN_TX_INHIBIT    FTX1_TOKEN(3, 5, 12)
#define TOK_TXGEN_METER_DET     FTX1_TOKEN(3, 5, 13)

/* EX0306: KEY/DIAL */
#define TOK_DIAL_SSB_CW_STEP    FTX1_TOKEN(3, 6, 1)
#define TOK_DIAL_RTTY_PSK_STEP  FTX1_TOKEN(3, 6, 2)
#define TOK_DIAL_FM_STEP        FTX1_TOKEN(3, 6, 3)
#define TOK_DIAL_CH_STEP        FTX1_TOKEN(3, 6, 4)
#define TOK_DIAL_AM_CH_STEP     FTX1_TOKEN(3, 6, 5)
#define TOK_DIAL_FM_CH_STEP     FTX1_TOKEN(3, 6, 6)
#define TOK_DIAL_MAIN_STEPS     FTX1_TOKEN(3, 6, 7)
#define TOK_DIAL_MIC_P1         FTX1_TOKEN(3, 6, 8)
#define TOK_DIAL_MIC_P2         FTX1_TOKEN(3, 6, 9)
#define TOK_DIAL_MIC_P3         FTX1_TOKEN(3, 6, 10)
#define TOK_DIAL_MIC_P4         FTX1_TOKEN(3, 6, 11)
#define TOK_DIAL_MIC_UP         FTX1_TOKEN(3, 6, 12)
#define TOK_DIAL_MIC_DOWN       FTX1_TOKEN(3, 6, 13)
#define TOK_DIAL_MIC_SCAN       FTX1_TOKEN(3, 6, 14)

/* EX0307: OPTION (SPA-1 power limits) */
#define TOK_OPT_TUNER_ANT1      FTX1_TOKEN(3, 7, 1)
#define TOK_OPT_TUNER_ANT2      FTX1_TOKEN(3, 7, 2)
#define TOK_OPT_ANT2_OP         FTX1_TOKEN(3, 7, 3)
#define TOK_OPT_HF_ANT_SEL      FTX1_TOKEN(3, 7, 4)
#define TOK_OPT_HF_MAX_PWR      FTX1_TOKEN(3, 7, 5)
#define TOK_OPT_50M_MAX_PWR     FTX1_TOKEN(3, 7, 6)
#define TOK_OPT_70M_MAX_PWR     FTX1_TOKEN(3, 7, 7)
#define TOK_OPT_144M_MAX_PWR    FTX1_TOKEN(3, 7, 8)
#define TOK_OPT_430M_MAX_PWR    FTX1_TOKEN(3, 7, 9)
#define TOK_OPT_AM_MAX_PWR      FTX1_TOKEN(3, 7, 10)
#define TOK_OPT_AM_VU_MAX_PWR   FTX1_TOKEN(3, 7, 11)
#define TOK_OPT_GPS             FTX1_TOKEN(3, 7, 12)
#define TOK_OPT_GPS_PINNING     FTX1_TOKEN(3, 7, 13)
#define TOK_OPT_GPS_BAUDRATE    FTX1_TOKEN(3, 7, 14)

/*
 * ===========================================================================
 * EX04: DISPLAY SETTING
 * ===========================================================================
 */

/* EX0401: DISPLAY */
#define TOK_DISP_MY_CALL        FTX1_TOKEN(4, 1, 1)
#define TOK_DISP_MY_CALL_TIME   FTX1_TOKEN(4, 1, 2)
#define TOK_DISP_POPUP_TIME     FTX1_TOKEN(4, 1, 3)
#define TOK_DISP_SCREEN_SAVER   FTX1_TOKEN(4, 1, 4)
#define TOK_DISP_SAVER_BAT      FTX1_TOKEN(4, 1, 5)
#define TOK_DISP_SAVER_TYPE     FTX1_TOKEN(4, 1, 6)
#define TOK_DISP_AUTO_PWR_OFF   FTX1_TOKEN(4, 1, 7)
#define TOK_DISP_LED_DIMMER     FTX1_TOKEN(4, 1, 8)

/* EX0402: UNIT */
#define TOK_UNIT_POSITION       FTX1_TOKEN(4, 2, 1)
#define TOK_UNIT_DISTANCE       FTX1_TOKEN(4, 2, 2)
#define TOK_UNIT_SPEED          FTX1_TOKEN(4, 2, 3)
#define TOK_UNIT_ALTITUDE       FTX1_TOKEN(4, 2, 4)
#define TOK_UNIT_TEMP           FTX1_TOKEN(4, 2, 5)
#define TOK_UNIT_RAIN           FTX1_TOKEN(4, 2, 6)
#define TOK_UNIT_WIND           FTX1_TOKEN(4, 2, 7)

/* EX0403: SCOPE */
#define TOK_SCOPE_RBW           FTX1_TOKEN(4, 3, 1)
#define TOK_SCOPE_CTR           FTX1_TOKEN(4, 3, 2)
#define TOK_SCOPE_2D_SENS       FTX1_TOKEN(4, 3, 3)
#define TOK_SCOPE_3DSS_SENS     FTX1_TOKEN(4, 3, 4)
#define TOK_SCOPE_AVERAGE       FTX1_TOKEN(4, 3, 5)

/* EX0404: VFO IND COLOR */
#define TOK_VMI_COLOR_VFO       FTX1_TOKEN(4, 4, 1)
#define TOK_VMI_COLOR_MEM       FTX1_TOKEN(4, 4, 2)
#define TOK_VMI_COLOR_CLAR      FTX1_TOKEN(4, 4, 3)

/*
 * ===========================================================================
 * EX05: EXTENSION SETTING
 * ===========================================================================
 */

/* EX0501: DATE&TIME */
#define TOK_DT_TIMEZONE         FTX1_TOKEN(5, 1, 1)
#define TOK_DT_GPS_TIME_SET     FTX1_TOKEN(5, 1, 7)
#define TOK_DT_MY_POSITION      FTX1_TOKEN(5, 1, 8)

/*
 * ===========================================================================
 * EX06: APRS SETTING
 * ===========================================================================
 */

/* EX0601: GENERAL */
#define TOK_APRS_MODEM_SEL      FTX1_TOKEN(6, 1, 1)
#define TOK_APRS_MODEM_TYPE     FTX1_TOKEN(6, 1, 2)
#define TOK_APRS_AF_MUTE        FTX1_TOKEN(6, 1, 3)
#define TOK_APRS_TX_DELAY       FTX1_TOKEN(6, 1, 4)
#define TOK_APRS_CALLSIGN       FTX1_TOKEN(6, 1, 5)
/* Note: Items 06-08 don't exist per CAT manual */
#define TOK_APRS_DEST           FTX1_TOKEN(6, 1, 9)   /* APRS DESTINATION (fixed: APYX01) */

/* EX0602: MSG TEMPLATE */
#define TOK_APRS_MSG_TEXT1      FTX1_TOKEN(6, 2, 1)
#define TOK_APRS_MSG_TEXT2      FTX1_TOKEN(6, 2, 2)
#define TOK_APRS_MSG_TEXT3      FTX1_TOKEN(6, 2, 3)
#define TOK_APRS_MSG_TEXT4      FTX1_TOKEN(6, 2, 4)
#define TOK_APRS_MSG_TEXT5      FTX1_TOKEN(6, 2, 5)
#define TOK_APRS_MSG_TEXT6      FTX1_TOKEN(6, 2, 6)
#define TOK_APRS_MSG_TEXT7      FTX1_TOKEN(6, 2, 7)
#define TOK_APRS_MSG_TEXT8      FTX1_TOKEN(6, 2, 8)

/* EX0603: MY SYMBOL */
#define TOK_APRS_MY_SYMBOL      FTX1_TOKEN(6, 3, 1)
#define TOK_APRS_ICON1          FTX1_TOKEN(6, 3, 2)
#define TOK_APRS_ICON2          FTX1_TOKEN(6, 3, 3)
#define TOK_APRS_ICON3          FTX1_TOKEN(6, 3, 4)
#define TOK_APRS_ICON_USER      FTX1_TOKEN(6, 3, 5)

/* EX0604: DIGI PATH */
#define TOK_APRS_PATH_SEL       FTX1_TOKEN(6, 4, 1)

/*
 * ===========================================================================
 * EX07: APRS BEACON
 * ===========================================================================
 */

/* EX0701: BEACON SET */
#define TOK_BCN_TYPE            FTX1_TOKEN(7, 1, 1)
#define TOK_BCN_INFO_AMBIG      FTX1_TOKEN(7, 1, 2)
#define TOK_BCN_SPEED_COURSE    FTX1_TOKEN(7, 1, 3)
#define TOK_BCN_ALTITUDE        FTX1_TOKEN(7, 1, 4)
#define TOK_BCN_POS_COMMENT     FTX1_TOKEN(7, 1, 5)
#define TOK_BCN_EMERGENCY       FTX1_TOKEN(7, 1, 6)

/* EX0702: AUTO BEACON */
#define TOK_BCN_INTERVAL        FTX1_TOKEN(7, 2, 1)
#define TOK_BCN_PROPORTIONAL    FTX1_TOKEN(7, 2, 2)
#define TOK_BCN_DECAY           FTX1_TOKEN(7, 2, 3)
#define TOK_BCN_AUTO_LOW_SPD    FTX1_TOKEN(7, 2, 4)
#define TOK_BCN_DELAY           FTX1_TOKEN(7, 2, 5)

/* EX0703: SmartBeac */
#define TOK_SMART_LOW_SPD       FTX1_TOKEN(7, 3, 1)
#define TOK_SMART_HIGH_SPD      FTX1_TOKEN(7, 3, 2)
#define TOK_SMART_SLOW_RATE     FTX1_TOKEN(7, 3, 3)
#define TOK_SMART_FAST_RATE     FTX1_TOKEN(7, 3, 4)
#define TOK_SMART_TURN_ANGLE    FTX1_TOKEN(7, 3, 5)
#define TOK_SMART_TURN_SLOPE    FTX1_TOKEN(7, 3, 6)
#define TOK_SMART_TURN_TIME     FTX1_TOKEN(7, 3, 7)

/* EX0704: BEACON TEXT */
#define TOK_BCN_TEXT_SEL        FTX1_TOKEN(7, 4, 1)
#define TOK_BCN_TX_RATE         FTX1_TOKEN(7, 4, 2)
#define TOK_BCN_FREQ            FTX1_TOKEN(7, 4, 3)
#define TOK_BCN_STATUS1         FTX1_TOKEN(7, 4, 4)
#define TOK_BCN_STATUS2         FTX1_TOKEN(7, 4, 5)
#define TOK_BCN_STATUS3         FTX1_TOKEN(7, 4, 6)
#define TOK_BCN_STATUS4         FTX1_TOKEN(7, 4, 7)
#define TOK_BCN_STATUS5         FTX1_TOKEN(7, 4, 8)

/*
 * ===========================================================================
 * EX08: APRS FILTER
 * ===========================================================================
 */

/* EX0801: LIST SETTING */
#define TOK_FILT_LIST_SORT      FTX1_TOKEN(8, 1, 1)

/* EX0802: STATION LIST */
#define TOK_FILT_MIC_E          FTX1_TOKEN(8, 2, 1)
#define TOK_FILT_POSITION       FTX1_TOKEN(8, 2, 2)
#define TOK_FILT_WEATHER        FTX1_TOKEN(8, 2, 3)
#define TOK_FILT_OBJECT         FTX1_TOKEN(8, 2, 4)
#define TOK_FILT_ITEM           FTX1_TOKEN(8, 2, 5)
#define TOK_FILT_STATUS         FTX1_TOKEN(8, 2, 6)
#define TOK_FILT_OTHER          FTX1_TOKEN(8, 2, 7)
#define TOK_FILT_ALTNET         FTX1_TOKEN(8, 2, 8)

/* EX0803: POPUP */
#define TOK_FILT_POPUP_BCN      FTX1_TOKEN(8, 3, 1)
#define TOK_FILT_POPUP_MSG      FTX1_TOKEN(8, 3, 2)
#define TOK_FILT_POPUP_MYPACKET FTX1_TOKEN(8, 3, 3)

/* EX0804: RINGER */
#define TOK_RING_TX_BCN         FTX1_TOKEN(8, 4, 1)
#define TOK_RING_RX_BCN         FTX1_TOKEN(8, 4, 2)
#define TOK_RING_TX_MSG         FTX1_TOKEN(8, 4, 3)
#define TOK_RING_RX_MSG         FTX1_TOKEN(8, 4, 4)
#define TOK_RING_MY_PACKET      FTX1_TOKEN(8, 4, 7)

/* EX0806: MSG FILTER */
#define TOK_MSGFILT_GRP1        FTX1_TOKEN(8, 6, 1)
#define TOK_MSGFILT_GRP2        FTX1_TOKEN(8, 6, 2)
#define TOK_MSGFILT_GRP3        FTX1_TOKEN(8, 6, 3)
#define TOK_MSGFILT_GRP4        FTX1_TOKEN(8, 6, 4)
#define TOK_MSGFILT_GRP5        FTX1_TOKEN(8, 6, 5)
#define TOK_MSGFILT_GRP6        FTX1_TOKEN(8, 6, 6)
#define TOK_MSGFILT_BULLETIN1   FTX1_TOKEN(8, 6, 7)
#define TOK_MSGFILT_BULLETIN2   FTX1_TOKEN(8, 6, 8)
#define TOK_MSGFILT_BULLETIN3   FTX1_TOKEN(8, 6, 9)

/*
 * ===========================================================================
 * EX09: PRESET (5 presets with 16 items each)
 * ===========================================================================
 */

/* EX0901: PRESET1 */
#define TOK_PRE1_NAME           FTX1_TOKEN(9, 1, 1)
#define TOK_PRE1_CAT1_RATE      FTX1_TOKEN(9, 1, 2)
#define TOK_PRE1_CAT1_TIMEOUT   FTX1_TOKEN(9, 1, 3)
#define TOK_PRE1_STOP_BIT       FTX1_TOKEN(9, 1, 4)
#define TOK_PRE1_AGC_FAST       FTX1_TOKEN(9, 1, 5)
#define TOK_PRE1_AGC_MID        FTX1_TOKEN(9, 1, 6)
#define TOK_PRE1_AGC_SLOW       FTX1_TOKEN(9, 1, 7)
#define TOK_PRE1_LCUT_FREQ      FTX1_TOKEN(9, 1, 8)
#define TOK_PRE1_LCUT_SLOPE     FTX1_TOKEN(9, 1, 9)
#define TOK_PRE1_HCUT_FREQ      FTX1_TOKEN(9, 1, 10)
#define TOK_PRE1_HCUT_SLOPE     FTX1_TOKEN(9, 1, 11)
#define TOK_PRE1_USB_OUT_LVL    FTX1_TOKEN(9, 1, 12)
#define TOK_PRE1_TX_BPF         FTX1_TOKEN(9, 1, 13)
#define TOK_PRE1_MOD_SOURCE     FTX1_TOKEN(9, 1, 14)
#define TOK_PRE1_USB_MOD_GAIN   FTX1_TOKEN(9, 1, 15)
#define TOK_PRE1_RPTT_SELECT    FTX1_TOKEN(9, 1, 16)

/* EX0902-0905: PRESET2-5 follow same pattern */
/* PRESET 2 (EX0902) */
#define TOK_PRE2_NAME           FTX1_TOKEN(9, 2, 1)
#define TOK_PRE2_CAT1_RATE      FTX1_TOKEN(9, 2, 2)
#define TOK_PRE2_CAT1_TIMEOUT   FTX1_TOKEN(9, 2, 3)
#define TOK_PRE2_STOP_BIT       FTX1_TOKEN(9, 2, 4)
#define TOK_PRE2_AGC_FAST       FTX1_TOKEN(9, 2, 5)
#define TOK_PRE2_AGC_MID        FTX1_TOKEN(9, 2, 6)
#define TOK_PRE2_AGC_SLOW       FTX1_TOKEN(9, 2, 7)
#define TOK_PRE2_LCUT_FREQ      FTX1_TOKEN(9, 2, 8)
#define TOK_PRE2_LCUT_SLOPE     FTX1_TOKEN(9, 2, 9)
#define TOK_PRE2_HCUT_FREQ      FTX1_TOKEN(9, 2, 10)
#define TOK_PRE2_HCUT_SLOPE     FTX1_TOKEN(9, 2, 11)
#define TOK_PRE2_USB_OUT_LVL    FTX1_TOKEN(9, 2, 12)
#define TOK_PRE2_TX_BPF         FTX1_TOKEN(9, 2, 13)
#define TOK_PRE2_MOD_SOURCE     FTX1_TOKEN(9, 2, 14)
#define TOK_PRE2_USB_MOD_GAIN   FTX1_TOKEN(9, 2, 15)
#define TOK_PRE2_RPTT_SELECT    FTX1_TOKEN(9, 2, 16)

/* PRESET 3 (EX0903) */
#define TOK_PRE3_NAME           FTX1_TOKEN(9, 3, 1)
#define TOK_PRE3_CAT1_RATE      FTX1_TOKEN(9, 3, 2)
#define TOK_PRE3_CAT1_TIMEOUT   FTX1_TOKEN(9, 3, 3)
#define TOK_PRE3_STOP_BIT       FTX1_TOKEN(9, 3, 4)
#define TOK_PRE3_AGC_FAST       FTX1_TOKEN(9, 3, 5)
#define TOK_PRE3_AGC_MID        FTX1_TOKEN(9, 3, 6)
#define TOK_PRE3_AGC_SLOW       FTX1_TOKEN(9, 3, 7)
#define TOK_PRE3_LCUT_FREQ      FTX1_TOKEN(9, 3, 8)
#define TOK_PRE3_LCUT_SLOPE     FTX1_TOKEN(9, 3, 9)
#define TOK_PRE3_HCUT_FREQ      FTX1_TOKEN(9, 3, 10)
#define TOK_PRE3_HCUT_SLOPE     FTX1_TOKEN(9, 3, 11)
#define TOK_PRE3_USB_OUT_LVL    FTX1_TOKEN(9, 3, 12)
#define TOK_PRE3_TX_BPF         FTX1_TOKEN(9, 3, 13)
#define TOK_PRE3_MOD_SOURCE     FTX1_TOKEN(9, 3, 14)
#define TOK_PRE3_USB_MOD_GAIN   FTX1_TOKEN(9, 3, 15)
#define TOK_PRE3_RPTT_SELECT    FTX1_TOKEN(9, 3, 16)

/* PRESET 4 (EX0904) */
#define TOK_PRE4_NAME           FTX1_TOKEN(9, 4, 1)
#define TOK_PRE4_CAT1_RATE      FTX1_TOKEN(9, 4, 2)
#define TOK_PRE4_CAT1_TIMEOUT   FTX1_TOKEN(9, 4, 3)
#define TOK_PRE4_STOP_BIT       FTX1_TOKEN(9, 4, 4)
#define TOK_PRE4_AGC_FAST       FTX1_TOKEN(9, 4, 5)
#define TOK_PRE4_AGC_MID        FTX1_TOKEN(9, 4, 6)
#define TOK_PRE4_AGC_SLOW       FTX1_TOKEN(9, 4, 7)
#define TOK_PRE4_LCUT_FREQ      FTX1_TOKEN(9, 4, 8)
#define TOK_PRE4_LCUT_SLOPE     FTX1_TOKEN(9, 4, 9)
#define TOK_PRE4_HCUT_FREQ      FTX1_TOKEN(9, 4, 10)
#define TOK_PRE4_HCUT_SLOPE     FTX1_TOKEN(9, 4, 11)
#define TOK_PRE4_USB_OUT_LVL    FTX1_TOKEN(9, 4, 12)
#define TOK_PRE4_TX_BPF         FTX1_TOKEN(9, 4, 13)
#define TOK_PRE4_MOD_SOURCE     FTX1_TOKEN(9, 4, 14)
#define TOK_PRE4_USB_MOD_GAIN   FTX1_TOKEN(9, 4, 15)
#define TOK_PRE4_RPTT_SELECT    FTX1_TOKEN(9, 4, 16)

/* PRESET 5 (EX0905) */
#define TOK_PRE5_NAME           FTX1_TOKEN(9, 5, 1)
#define TOK_PRE5_CAT1_RATE      FTX1_TOKEN(9, 5, 2)
#define TOK_PRE5_CAT1_TIMEOUT   FTX1_TOKEN(9, 5, 3)
#define TOK_PRE5_STOP_BIT       FTX1_TOKEN(9, 5, 4)
#define TOK_PRE5_AGC_FAST       FTX1_TOKEN(9, 5, 5)
#define TOK_PRE5_AGC_MID        FTX1_TOKEN(9, 5, 6)
#define TOK_PRE5_AGC_SLOW       FTX1_TOKEN(9, 5, 7)
#define TOK_PRE5_LCUT_FREQ      FTX1_TOKEN(9, 5, 8)
#define TOK_PRE5_LCUT_SLOPE     FTX1_TOKEN(9, 5, 9)
#define TOK_PRE5_HCUT_FREQ      FTX1_TOKEN(9, 5, 10)
#define TOK_PRE5_HCUT_SLOPE     FTX1_TOKEN(9, 5, 11)
#define TOK_PRE5_USB_OUT_LVL    FTX1_TOKEN(9, 5, 12)
#define TOK_PRE5_TX_BPF         FTX1_TOKEN(9, 5, 13)
#define TOK_PRE5_MOD_SOURCE     FTX1_TOKEN(9, 5, 14)
#define TOK_PRE5_USB_MOD_GAIN   FTX1_TOKEN(9, 5, 15)
#define TOK_PRE5_RPTT_SELECT    FTX1_TOKEN(9, 5, 16)

/*
 * ===========================================================================
 * EX11: BLUETOOTH
 * ===========================================================================
 */

/* EX1101: Bluetooth */
#define TOK_BT_ENABLE           FTX1_TOKEN(11, 1, 1)
#define TOK_BT_AUDIO            FTX1_TOKEN(11, 1, 4)

/*
 * Menu item definition structure
 */
struct ftx1_menu_item {
    token_t token;          /* Hamlib token (encodes group/section/item) */
    const char *name;       /* Human-readable name */
    int digits;             /* Number of digits in P4 value */
    int min_val;            /* Minimum value */
    int max_val;            /* Maximum value */
    int flags;              /* Special flags (e.g., requires SPA-1) */
};

/* Menu item flags */
#define FTX1_MENU_FLAG_SPA1     0x01    /* Requires SPA-1 */
#define FTX1_MENU_FLAG_STRING   0x02    /* Value is a string, not numeric */
#define FTX1_MENU_FLAG_SIGNED   0x04    /* Value can be negative */

/*
 * Function prototypes
 */
extern int ftx1_menu_set_token(RIG *rig, token_t token, value_t val);
extern int ftx1_menu_get_token(RIG *rig, token_t token, value_t *val);
extern const struct ftx1_menu_item *ftx1_menu_find_token(token_t token);
extern const struct confparams *ftx1_get_ext_parms(void);
extern int ftx1_get_ext_parm_count(void);

#endif /* _FTX1_MENU_H */
