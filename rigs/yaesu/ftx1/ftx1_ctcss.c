/*
 * Hamlib Yaesu backend - FTX-1 CTCSS/DCS Encode/Decode Group
 * Copyright (c) 2025 by Terrell Deppe (KJ5HST)
 *
 * This file implements CAT commands for CTCSS and DCS tone control.
 *
 * CAT Commands in this file:
 *   CN P1 P2 P3P4P5; - Tone Number (P1=VFO 0/1, P2=Type 0=CTCSS/1=DCS, P3P4P5=code)
 *                      CN00XXX = Main CTCSS, CN01XXX = Main DCS
 *                      CN10XXX = Sub CTCSS,  CN11XXX = Sub DCS
 *                      Code range: 000-103 (table lookup for both CTCSS and DCS)
 *   CT P1 P2;        - CTCSS/DCS Mode (P1=VFO 0=Main, P2=0-3: off/ENC/TSQ/DCS)
 *                      NOTE: Must include VFO param; CT; alone returns ?;
 *
 * CTCSS Tone Table (00-49 per spec, 0-based indexing):
 *   00=67.0   10=94.8   20=131.8  30=171.3  40=203.5
 *   01=69.3   11=97.4   21=136.5  31=173.8  41=206.5
 *   02=71.9   12=100.0  22=141.3  32=177.3  42=210.7
 *   03=74.4   13=103.5  23=146.2  33=179.9  43=218.1
 *   04=77.0   14=107.2  24=151.4  34=183.5  44=225.7
 *   05=79.7   15=110.9  25=156.7  35=186.2  45=229.1
 *   06=82.5   16=114.8  26=159.8  36=189.9  46=233.6
 *   07=85.4   17=118.8  27=162.2  37=192.8  47=241.8
 *   08=88.5   18=123.0  28=165.5  38=196.6  48=250.3
 *   09=91.5   19=127.3  29=167.9  39=199.5  49=254.1
 *
 * DCS Code Table (00-103 per spec, 0-based indexing):
 * 000=023  015=074  030=165  045=261  060=356  075=462  090=627
 * 001=025  016=114  031=172  046=263  061=364  076=464  091=631
 * 002=026  017=115  032=174  047=265  062=365  077=465  092=632
 * 003=031  018=116  033=205  048=266  063=371  078=466  093=654
 * 004=032  019=122  034=212  049=271  064=411  079=503  094=662
 * 005=036  020=125  035=223  050=274  065=412  080=506  095=664
 * 006=043  021=131  036=225  051=306  066=413  081=516  096=703
 * 007=047  022=132  037=226  052=311  067=423  082=523  097=712
 * 008=051  023=134  038=243  053=315  068=431  083=526  098=723
 * 009=053  024=143  039=244  054=325  069=432  084=532  099=731
 * 010=054  025=145  040=245  055=331  070=445  085=546  100=732
 * 011=065  026=152  041=246  056=332  071=446  086=565  101=734
 * 012=071  027=155  042=251  057=343  072=452  087=606  102=743
 * 013=072  028=156  043=252  058=346  073=454  088=612  103=754
 * 014=073  029=162  044=255  059=351  074=455  089=624
 */

#include <stdlib.h>
#include <string.h>
#include <hamlib/rig.h>
#include "misc.h"
#include "yaesu.h"
#include "newcat.h"
#include "ftx1.h"

#define FTX1_CTCSS_MIN 0
#define FTX1_CTCSS_MAX 49

/* CTCSS tone frequencies in 0.1 Hz (multiply by 10 for actual) */
static const unsigned int ftx1_ctcss_tones[] = {
    670,  693,  719,  744,  770,  797,  825,  854,  885,  915,   /* 00-09 */
    948,  974,  1000, 1035, 1072, 1109, 1148, 1188, 1230, 1273,  /* 10-19 */
    1318, 1365, 1413, 1462, 1514, 1567, 1598, 1622, 1655, 1679,  /* 20-29 */
    1713, 1738, 1773, 1799, 1835, 1862, 1899, 1928, 1966, 1995,  /* 30-39 */
    2035, 2065, 2107, 2181, 2257, 2291, 2336, 2418, 2503, 2541   /* 40-49 */
};

#define FTX1_DCS_MIN 0
#define FTX1_DCS_MAX 103

static const unsigned int ftx1_dcs_codes[] = {
     /* 000-014 */
    23,  25,  26,  31,  32,  36,  43,  47,  51,  53,  54,  65,  71,  72,  73,
     /* 015-029 */
    74,  114, 115, 116, 122, 125, 131, 132, 134, 143, 145, 152, 155, 156, 162,
     /* 030-044 */
    165, 172, 174, 205, 212, 223, 225, 226, 243, 244, 245, 246, 251, 252, 255,
      /* 045-059 */
    261, 263, 265, 266, 271, 274, 306, 311, 315, 325, 331, 332, 343, 346, 351,
      /* 060-074 */
    356, 364, 365, 371, 411, 412, 413, 423, 431, 432, 445, 446, 452, 454, 455,
      /* 075-089 */
    462, 464, 465, 466, 503, 506, 516, 523, 526, 532, 546, 565, 606, 612, 624,
      /* 090-103 */
    627, 631, 632, 654, 662, 664, 703, 712, 723, 731, 732, 734, 743, 754
};

/* Convert CTCSS frequency (in 0.1 Hz) to tone number (0-based per spec) */
int ftx1_freq_to_tone_num(unsigned int freq)
{
    int i;

    for (i = 0; i <= FTX1_CTCSS_MAX; i++)
    {
        if (ftx1_ctcss_tones[i] == freq)
        {
            return i;  /* Tone numbers are 0-based (000-049) per spec */
        }
    }

    return -1;  /* Not found */
}

/* Convert tone number (0-based per spec) to frequency (in 0.1 Hz) */
unsigned int ftx1_tone_num_to_freq(int num)
{
    if (num < FTX1_CTCSS_MIN || num > FTX1_CTCSS_MAX)
    {
        return 0;
    }

    return ftx1_ctcss_tones[num];
}

int ftx1_code_to_dcs_num(unsigned int code)
{
    for (int i = 0; i <= FTX1_DCS_MAX; i++)
    {
        if (ftx1_dcs_codes[i] == code)
        {
            return i;  /* DCS numbers are 0-based (000-103) per spec */
        }
    }

    return -1;  /* Not found */
}

unsigned int ftx1_dcs_num_to_code(int num)
{
    if (num < FTX1_DCS_MIN || num > FTX1_DCS_MAX)
    {
        return 0;
    }

    return ftx1_dcs_codes[num];
}

/*
 * ftx1_set_ctcss_mode - Set CTCSS Mode (CT P1 P2;)
 *
 * Parameter 'mode' is the FTX-1 CT command value:
 *   0 = Off
 *   1 = CTCSS Encode only (TX tone)
 *   2 = Tone Squelch (TX+RX tone)
 *   3 = DCS mode
 *
 * Command format: CT P1 P2; where P1=VFO (0=Main), P2=mode (0-3)
 *
 * Note: This function expects FTX1_CTCSS_MODE_* values, not Hamlib
 * RIG_FUNC_* flags. The caller is responsible for mapping Hamlib
 * function flags to FTX-1 mode values if needed.
 */
int ftx1_set_ctcss_mode(RIG *rig, tone_t mode)
{
    struct newcat_priv_data *priv = STATE(rig)->priv;

    /* Validate and use FTX-1 CT command values directly */
    if (mode > FTX1_CTCSS_MODE_DCS)
    {
        rig_debug(RIG_DEBUG_ERR, "%s: invalid mode %u (must be 0-3)\n",
                  __func__, mode);
        return -RIG_EINVAL;
    }

    rig_debug(RIG_DEBUG_VERBOSE, "%s: mode=%u\n", __func__, mode);

    /* CT sets are transient overlays in Memory mode — accepted but not
     * persisted.  Exit memory mode so the change actually sticks. */
    ftx1_ensure_vfo_mode(rig);

    /* CT P1 P2; where P1=VFO (0=Main), P2=mode */
    SNPRINTF(priv->cmd_str, sizeof(priv->cmd_str), "CT0%d;", (int)mode);
    return newcat_set_cmd(rig);
}

/*
 * ftx1_get_ctcss_mode - Get CTCSS Mode (CT P1;)
 *
 * Command format: CT P1; where P1=VFO (0=Main)
 * Response format: CT P1 P2; where P2=mode (0-3)
 *
 * Returns the FTX-1 CT command value in *mode:
 *   0 = Off (FTX1_CTCSS_MODE_OFF)
 *   1 = CTCSS Encode only (FTX1_CTCSS_MODE_ENC)
 *   2 = Tone Squelch (FTX1_CTCSS_MODE_TSQ)
 *   3 = DCS mode (FTX1_CTCSS_MODE_DCS)
 */
int ftx1_get_ctcss_mode(RIG *rig, tone_t *mode)
{
    struct newcat_priv_data *priv = STATE(rig)->priv;
    int ret, vfo, p2;

    rig_debug(RIG_DEBUG_VERBOSE, "%s\n", __func__);

    /* CT P1; where P1=VFO (0=Main) */
    SNPRINTF(priv->cmd_str, sizeof(priv->cmd_str), "CT0;");

    ret = newcat_get_cmd(rig);
    if (ret != RIG_OK) return ret;

    /* Response: CT P1 P2; (e.g., CT00; means VFO 0, mode 0) */
    if (sscanf(priv->ret_data + 2, "%1d%1d", &vfo, &p2) != 2)
    {
        rig_debug(RIG_DEBUG_ERR, "%s: failed to parse '%s'\n", __func__,
                  priv->ret_data);
        return -RIG_EPROTO;
    }

    /* Return FTX-1 mode value directly (0-3) */
    *mode = (tone_t)p2;

    rig_debug(RIG_DEBUG_VERBOSE, "%s: vfo=%d mode=%u\n", __func__, vfo, *mode);

    return RIG_OK;
}

/* Set CTCSS Tone (CN P1 P2P3;) */
int ftx1_set_ctcss_tone(RIG *rig, vfo_t vfo, tone_t tone)
{
    struct newcat_priv_data *priv = STATE(rig)->priv;
    int tone_num;

    (void)vfo;  /* Unused */

    /* tone is in 0.1 Hz, find matching tone number */
    tone_num = ftx1_freq_to_tone_num(tone);

    if (tone_num < 0)
    {
        rig_debug(RIG_DEBUG_ERR, "%s: tone %u not found in table\n", __func__,
                  tone);
        return -RIG_EINVAL;
    }

    rig_debug(RIG_DEBUG_VERBOSE, "%s: tone=%u tone_num=%d\n", __func__, tone,
              tone_num);

    /* CN on Main is a transient overlay in Memory mode — exit first. */
    ftx1_ensure_vfo_mode(rig);

    /* P1=0 for Main VFO, P2=0 for CTCSS TONE, P3 is 3-digit tone number */
    SNPRINTF(priv->cmd_str, sizeof(priv->cmd_str), "CN00%03d;", tone_num);
    return newcat_set_cmd(rig);
}

/* Get CTCSS Tone */
int ftx1_get_ctcss_tone(RIG *rig, vfo_t vfo, tone_t *tone)
{
    struct newcat_priv_data *priv = STATE(rig)->priv;
    int ret, p1, tone_num;

    (void)vfo;  /* Unused */

    rig_debug(RIG_DEBUG_VERBOSE, "%s\n", __func__);

    /* P1=0 for Main VFO, P2=0 for CTCSS TONE, need CN00; to query */
    SNPRINTF(priv->cmd_str, sizeof(priv->cmd_str), "CN00;");

    ret = newcat_get_cmd(rig);
    if (ret != RIG_OK) return ret;

    /* Response format: CN P1 P2P3 (e.g., CN00012 for tone 12) */
    if (sscanf(priv->ret_data + 2, "%2d%3d", &p1, &tone_num) != 2)
    {
        rig_debug(RIG_DEBUG_ERR, "%s: failed to parse '%s'\n", __func__,
                  priv->ret_data);
        return -RIG_EPROTO;
    }

    *tone = ftx1_tone_num_to_freq(tone_num);

    rig_debug(RIG_DEBUG_VERBOSE, "%s: tone_num=%d tone=%u\n", __func__,
              tone_num, *tone);

    return RIG_OK;
}

/* Set CTCSS Squelch Tone: the rig doesn't support separate tones */
int ftx1_set_ctcss_sql(RIG *rig, vfo_t vfo, tone_t tone)
{
    return ftx1_set_ctcss_tone(rig, vfo, tone);
}

/* Get CTCSS Squelch Tone */
int ftx1_get_ctcss_sql(RIG *rig, vfo_t vfo, tone_t *tone)
{
    return ftx1_get_ctcss_tone(rig, vfo, tone);
}

/*
 * Set DCS Code (CN P1 P2 P3P4P5;)
 *
 * CN command format:
 *   P1 = VFO: 0=Main, 1=Sub
 *   P2 = Type: 0=CTCSS, 1=DCS
 *   P3P4P5 = Code index (000-103, table lookup)
 *
 * Example: CN01023 = Main VFO, DCS, code index 023
 */
int ftx1_set_dcs_code(RIG *rig, vfo_t vfo, tone_t code)
{
    struct newcat_priv_data *priv = STATE(rig)->priv;
    int code_num;

    (void)vfo;  /* Unused - always Main for now */

    code_num = ftx1_code_to_dcs_num(code);
    if (code_num < 0)
    {
        rig_debug(RIG_DEBUG_ERR, "%s: code %u not found in table\n", __func__,
                  code);
        return -RIG_EINVAL;
    }

    rig_debug(RIG_DEBUG_VERBOSE, "%s: code=%u code_num=%d\n", __func__, code,
              code_num);

    /* CN on Main is a transient overlay in Memory mode — exit first. */
    ftx1_ensure_vfo_mode(rig);

    /* CN01XXX: Main VFO (0), DCS (1), code index */
    SNPRINTF(priv->cmd_str, sizeof(priv->cmd_str), "CN01%03d;", code_num);
    return newcat_set_cmd(rig);
}

/* Get DCS Code (CN P1 P2;) */
int ftx1_get_dcs_code(RIG *rig, vfo_t vfo, tone_t *code)
{
    struct newcat_priv_data *priv = STATE(rig)->priv;
    int ret, p1p2;
    unsigned int dcs_num;

    (void)vfo;  /* Unused */

    rig_debug(RIG_DEBUG_VERBOSE, "%s\n", __func__);

    /* Query Main DCS: CN01; */
    SNPRINTF(priv->cmd_str, sizeof(priv->cmd_str), "CN01;");

    ret = newcat_get_cmd(rig);
    if (ret != RIG_OK) return ret;

    /* Response format: CN01XXX (e.g., CN01023) */
    if (sscanf(priv->ret_data + 2, "%2d%3d", &p1p2, &dcs_num) != 2)
    {
        rig_debug(RIG_DEBUG_ERR, "%s: failed to parse '%s'\n", __func__,
                  priv->ret_data);
        return -RIG_EPROTO;
    }

    *code = ftx1_dcs_num_to_code(dcs_num);

    rig_debug(RIG_DEBUG_VERBOSE, "%s: code_num=%d code=%u\n", __func__,
              dcs_num, *code);

    return RIG_OK;
}

/*
 * Set DCS Squelch Code: the rig doesn't support separate codes
 */
int ftx1_set_dcs_sql(RIG *rig, vfo_t vfo, tone_t code)
{
    return ftx1_set_dcs_code(rig, vfo, code);
}

/* Get DCS Squelch Code */
int ftx1_get_dcs_sql(RIG *rig, vfo_t vfo, tone_t *code)
{
    return ftx1_get_dcs_code(rig, vfo, code);
}
