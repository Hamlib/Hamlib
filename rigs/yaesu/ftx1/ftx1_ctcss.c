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
 *   00=67.0   10=94.8   20=131.8  30=177.3  40=225.7
 *   01=69.3   11=97.4   21=136.5  31=179.9  41=229.1
 *   02=71.9   12=100.0  22=141.3  32=183.5  42=233.6
 *   03=74.4   13=103.5  23=146.2  33=186.2  43=241.8
 *   04=77.0   14=107.2  24=151.4  34=189.9  44=250.3
 *   05=79.7   15=110.9  25=156.7  35=192.8  45=254.1
 *   06=82.5   16=114.8  26=159.8  36=196.6  46=CS
 *   07=85.4   17=118.8  27=162.2  37=199.5  47-49=Reserved
 *   08=88.5   18=123.0  28=165.5  38=203.5
 *   09=91.5   19=127.3  29=167.9  39=206.5
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
    670,  693,  719,  744,  770,  797,  825,  854,  885,  915,   /* 01-10 */
    948,  974,  1000, 1035, 1072, 1109, 1148, 1188, 1230, 1273,  /* 11-20 */
    1318, 1365, 1413, 1462, 1514, 1567, 1598, 1622, 1655, 1679,  /* 21-30 */
    1773, 1799, 1835, 1862, 1899, 1928, 1966, 1995, 2035, 2065,  /* 31-40 */
    2257, 2291, 2336, 2418, 2503, 2541, 0, 0, 0, 0               /* 41-50 */
};

/* Convert CTCSS frequency (in 0.1 Hz) to tone number (0-based per spec) */
static int ftx1_freq_to_tone_num(unsigned int freq)
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
static unsigned int ftx1_tone_num_to_freq(int num)
{
    if (num < FTX1_CTCSS_MIN || num > FTX1_CTCSS_MAX)
    {
        return 0;
    }

    return ftx1_ctcss_tones[num];
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

    /* P1=0 for TX tone, P2P3P4 is 3-digit tone number */
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

    /* P1=0 for TX tone, need CN00; to query */
    SNPRINTF(priv->cmd_str, sizeof(priv->cmd_str), "CN00;");

    ret = newcat_get_cmd(rig);
    if (ret != RIG_OK) return ret;

    /* Response format: CN P1 P2P3P4 (e.g., CN00012 for TX tone 12) */
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

/* Set CTCSS Squelch Tone (CN P1 P2P3P4; with P1=1 for RX) */
int ftx1_set_ctcss_sql(RIG *rig, vfo_t vfo, tone_t tone)
{
    struct newcat_priv_data *priv = STATE(rig)->priv;
    int tone_num;

    (void)vfo;  /* Unused */

    tone_num = ftx1_freq_to_tone_num(tone);

    if (tone_num < 0)
    {
        rig_debug(RIG_DEBUG_ERR, "%s: tone %u not found in table\n", __func__,
                  tone);
        return -RIG_EINVAL;
    }

    rig_debug(RIG_DEBUG_VERBOSE, "%s: tone=%u tone_num=%d\n", __func__, tone,
              tone_num);

    /* P1=1 for RX tone, P2P3P4 is 3-digit tone number */
    SNPRINTF(priv->cmd_str, sizeof(priv->cmd_str), "CN10%03d;", tone_num);
    return newcat_set_cmd(rig);
}

/* Get CTCSS Squelch Tone */
int ftx1_get_ctcss_sql(RIG *rig, vfo_t vfo, tone_t *tone)
{
    struct newcat_priv_data *priv = STATE(rig)->priv;
    int ret, p1, tone_num;

    (void)vfo;  /* Unused */

    rig_debug(RIG_DEBUG_VERBOSE, "%s\n", __func__);

    /* P1=1 for RX tone, need CN10; to query */
    SNPRINTF(priv->cmd_str, sizeof(priv->cmd_str), "CN10;");

    ret = newcat_get_cmd(rig);
    if (ret != RIG_OK) return ret;

    /* Response format: CN P1 P2P3P4 (e.g., CN10012 for RX tone 12) */
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

    (void)vfo;  /* Unused - always Main for now */

    rig_debug(RIG_DEBUG_VERBOSE, "%s: code=%u\n", __func__, code);

    /* CN01XXX: Main VFO (0), DCS (1), code index */
    SNPRINTF(priv->cmd_str, sizeof(priv->cmd_str), "CN01%03u;", code);
    return newcat_set_cmd(rig);
}

/* Get DCS Code (CN P1 P2;) */
int ftx1_get_dcs_code(RIG *rig, vfo_t vfo, tone_t *code)
{
    struct newcat_priv_data *priv = STATE(rig)->priv;
    int ret, p1p2;
    unsigned int dcs;

    (void)vfo;  /* Unused */

    rig_debug(RIG_DEBUG_VERBOSE, "%s\n", __func__);

    /* Query Main DCS: CN01; */
    SNPRINTF(priv->cmd_str, sizeof(priv->cmd_str), "CN01;");

    ret = newcat_get_cmd(rig);
    if (ret != RIG_OK) return ret;

    /* Response format: CN01XXX (e.g., CN01023) */
    if (sscanf(priv->ret_data + 2, "%2d%3u", &p1p2, &dcs) != 2)
    {
        rig_debug(RIG_DEBUG_ERR, "%s: failed to parse '%s'\n", __func__,
                  priv->ret_data);
        return -RIG_EPROTO;
    }

    *code = dcs;

    rig_debug(RIG_DEBUG_VERBOSE, "%s: code=%u\n", __func__, *code);

    return RIG_OK;
}

/*
 * Set DCS Squelch Code (CN P1 P2 P3P4P5;)
 *
 * For RX squelch, we use Sub VFO DCS: CN11XXX
 * Note: This may need adjustment based on actual radio behavior.
 */
int ftx1_set_dcs_sql(RIG *rig, vfo_t vfo, tone_t code)
{
    struct newcat_priv_data *priv = STATE(rig)->priv;

    (void)vfo;  /* Unused */

    rig_debug(RIG_DEBUG_VERBOSE, "%s: code=%u\n", __func__, code);

    /* CN11XXX: Sub VFO (1), DCS (1), code index */
    SNPRINTF(priv->cmd_str, sizeof(priv->cmd_str), "CN11%03u;", code);
    return newcat_set_cmd(rig);
}

/* Get DCS Squelch Code (CN P1 P2;) */
int ftx1_get_dcs_sql(RIG *rig, vfo_t vfo, tone_t *code)
{
    struct newcat_priv_data *priv = STATE(rig)->priv;
    int ret, p1p2;
    unsigned int dcs;

    (void)vfo;  /* Unused */

    rig_debug(RIG_DEBUG_VERBOSE, "%s\n", __func__);

    /* Query Sub DCS: CN11; */
    SNPRINTF(priv->cmd_str, sizeof(priv->cmd_str), "CN11;");

    ret = newcat_get_cmd(rig);
    if (ret != RIG_OK) return ret;

    /* Response format: CN11XXX (e.g., CN11023) */
    if (sscanf(priv->ret_data + 2, "%2d%3u", &p1p2, &dcs) != 2)
    {
        rig_debug(RIG_DEBUG_ERR, "%s: failed to parse '%s'\n", __func__,
                  priv->ret_data);
        return -RIG_EPROTO;
    }

    *code = dcs;

    rig_debug(RIG_DEBUG_VERBOSE, "%s: code=%u\n", __func__, *code);

    return RIG_OK;
}
