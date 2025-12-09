/*
 * Hamlib Yaesu backend - FTX-1 CTCSS/DCS Encode/Decode Group
 * Copyright (c) 2025 by Terrell Deppe (KJ5HST)
 *
 * This file implements CAT commands for CTCSS and DCS tone control.
 *
 * CAT Commands in this file:
 *   CN P1 P2P3;      - CTCSS Tone Number (P1=0 TX/1 RX, P2P3=01-50)
 *   CT P1;           - CTCSS Mode (0=off, 1=ENC, 2=TSQ, 3=DCS)
 *   TS P1;           - Tone Status Query (composite)
 *   DC P1 P2P3P4;    - DCS Code (P1=0 TX/1 RX, P2-P4=code number)
 *
 * CTCSS Tone Table (01-50):
 *   01=67.0   11=94.8   21=131.8  31=177.3  41=225.7
 *   02=69.3   12=97.4   22=136.5  32=179.9  42=229.1
 *   03=71.9   13=100.0  23=141.3  33=183.5  43=233.6
 *   04=74.4   14=103.5  24=146.2  34=186.2  44=241.8
 *   05=77.0   15=107.2  25=151.4  35=189.9  45=250.3
 *   06=79.7   16=110.9  26=156.7  36=192.8  46=254.1
 *   07=82.5   17=114.8  27=159.8  37=196.6  47=CS
 *   08=85.4   18=118.8  28=162.2  38=199.5  48-50=Reserved
 *   09=88.5   19=123.0  29=165.5  39=203.5
 *   10=91.5   20=127.3  30=167.9  40=206.5
 */

#include <stdlib.h>
#include <string.h>
#include <hamlib/rig.h>
#include "misc.h"
#include "yaesu.h"
#include "newcat.h"

#define FTX1_CTCSS_MIN 1
#define FTX1_CTCSS_MAX 50

/* CTCSS tone frequencies in 0.1 Hz (multiply by 10 for actual) */
static const unsigned int ftx1_ctcss_tones[] = {
    670,  693,  719,  744,  770,  797,  825,  854,  885,  915,   /* 01-10 */
    948,  974,  1000, 1035, 1072, 1109, 1148, 1188, 1230, 1273,  /* 11-20 */
    1318, 1365, 1413, 1462, 1514, 1567, 1598, 1622, 1655, 1679,  /* 21-30 */
    1773, 1799, 1835, 1862, 1899, 1928, 1966, 1995, 2035, 2065,  /* 31-40 */
    2257, 2291, 2336, 2418, 2503, 2541, 0, 0, 0, 0               /* 41-50 */
};

/* Convert CTCSS frequency (in 0.1 Hz) to tone number */
static int ftx1_freq_to_tone_num(unsigned int freq)
{
    int i;

    for (i = 0; i < FTX1_CTCSS_MAX; i++)
    {
        if (ftx1_ctcss_tones[i] == freq)
        {
            return i + 1;  /* Tone numbers are 1-based */
        }
    }

    return -1;  /* Not found */
}

/* Convert tone number to frequency (in 0.1 Hz) */
static unsigned int ftx1_tone_num_to_freq(int num)
{
    if (num < FTX1_CTCSS_MIN || num > FTX1_CTCSS_MAX)
    {
        return 0;
    }

    return ftx1_ctcss_tones[num - 1];
}

/* Set CTCSS Mode (CT P1;) */
int ftx1_set_ctcss_mode(RIG *rig, tone_t mode)
{
    struct newcat_priv_data *priv = STATE(rig)->priv;
    int p1;

    /* Map Hamlib tone mode to FTX-1 CT command */
    switch (mode)
    {
    case 0:                   /* Off */
        p1 = 0;
        break;

    case RIG_FUNC_TONE:       /* CTCSS Encode only */
        p1 = 1;
        break;

    case RIG_FUNC_TSQL:       /* Tone Squelch (encode + decode) */
        p1 = 2;
        break;

    default:
        p1 = 3;               /* DCS */
        break;
    }

    rig_debug(RIG_DEBUG_VERBOSE, "%s: mode=%u p1=%d\n", __func__, mode, p1);

    SNPRINTF(priv->cmd_str, sizeof(priv->cmd_str), "CT%d;", p1);
    return newcat_set_cmd(rig);
}

/* Get CTCSS Mode */
int ftx1_get_ctcss_mode(RIG *rig, tone_t *mode)
{
    struct newcat_priv_data *priv = STATE(rig)->priv;
    int ret, p1;

    rig_debug(RIG_DEBUG_VERBOSE, "%s\n", __func__);

    SNPRINTF(priv->cmd_str, sizeof(priv->cmd_str), "CT;");

    ret = newcat_get_cmd(rig);
    if (ret != RIG_OK) return ret;

    if (sscanf(priv->ret_data + 2, "%1d", &p1) != 1)
    {
        rig_debug(RIG_DEBUG_ERR, "%s: failed to parse '%s'\n", __func__,
                  priv->ret_data);
        return -RIG_EPROTO;
    }

    /* Map FTX-1 mode to Hamlib */
    switch (p1)
    {
    case 0:
        *mode = 0;
        break;

    case 1:
        *mode = RIG_FUNC_TONE;
        break;

    case 2:
        *mode = RIG_FUNC_TSQL;
        break;

    default:
        *mode = 0;  /* DCS not directly mapped */
        break;
    }

    rig_debug(RIG_DEBUG_VERBOSE, "%s: p1=%d mode=%u\n", __func__, p1, *mode);

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

/* Set DCS Code (DC P1 P2P3P4;) */
int ftx1_set_dcs_code(RIG *rig, vfo_t vfo, tone_t code)
{
    struct newcat_priv_data *priv = STATE(rig)->priv;

    (void)vfo;  /* Unused */

    rig_debug(RIG_DEBUG_VERBOSE, "%s: code=%u\n", __func__, code);

    /* P1=0 for TX DCS code */
    SNPRINTF(priv->cmd_str, sizeof(priv->cmd_str), "DC0%03u;", code);
    return newcat_set_cmd(rig);
}

/* Get DCS Code */
int ftx1_get_dcs_code(RIG *rig, vfo_t vfo, tone_t *code)
{
    struct newcat_priv_data *priv = STATE(rig)->priv;
    int ret, p1;
    unsigned int dcs;

    (void)vfo;  /* Unused */

    rig_debug(RIG_DEBUG_VERBOSE, "%s\n", __func__);

    SNPRINTF(priv->cmd_str, sizeof(priv->cmd_str), "DC0;");

    ret = newcat_get_cmd(rig);
    if (ret != RIG_OK) return ret;

    if (sscanf(priv->ret_data + 2, "%1d%3u", &p1, &dcs) != 2)
    {
        rig_debug(RIG_DEBUG_ERR, "%s: failed to parse '%s'\n", __func__,
                  priv->ret_data);
        return -RIG_EPROTO;
    }

    *code = dcs;

    rig_debug(RIG_DEBUG_VERBOSE, "%s: code=%u\n", __func__, *code);

    return RIG_OK;
}

/* Set DCS Squelch Code */
int ftx1_set_dcs_sql(RIG *rig, vfo_t vfo, tone_t code)
{
    struct newcat_priv_data *priv = STATE(rig)->priv;

    (void)vfo;  /* Unused */

    rig_debug(RIG_DEBUG_VERBOSE, "%s: code=%u\n", __func__, code);

    /* P1=1 for RX DCS code */
    SNPRINTF(priv->cmd_str, sizeof(priv->cmd_str), "DC1%03u;", code);
    return newcat_set_cmd(rig);
}

/* Get DCS Squelch Code */
int ftx1_get_dcs_sql(RIG *rig, vfo_t vfo, tone_t *code)
{
    struct newcat_priv_data *priv = STATE(rig)->priv;
    int ret, p1;
    unsigned int dcs;

    (void)vfo;  /* Unused */

    rig_debug(RIG_DEBUG_VERBOSE, "%s\n", __func__);

    SNPRINTF(priv->cmd_str, sizeof(priv->cmd_str), "DC1;");

    ret = newcat_get_cmd(rig);
    if (ret != RIG_OK) return ret;

    if (sscanf(priv->ret_data + 2, "%1d%3u", &p1, &dcs) != 2)
    {
        rig_debug(RIG_DEBUG_ERR, "%s: failed to parse '%s'\n", __func__,
                  priv->ret_data);
        return -RIG_EPROTO;
    }

    *code = dcs;

    rig_debug(RIG_DEBUG_VERBOSE, "%s: code=%u\n", __func__, *code);

    return RIG_OK;
}
