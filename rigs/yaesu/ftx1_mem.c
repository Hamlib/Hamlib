/*
 * Hamlib Yaesu backend - FTX-1 Memory Operations Group
 * Copyright (c) 2025 by Terrell Deppe (KJ5HST)
 *
 * This file implements CAT commands for memory channel operations.
 *
 * CAT Commands in this file:
 *   MC P1P2P3;       - Memory Channel Select (001-117)
 *   MR;              - Memory Read (reads current channel data)
 *   MW;              - Memory Write (writes VFO data to current channel)
 *   MT P1P2P3;       - Memory Channel Tag/Name (read name)
 *   MA;              - Memory to VFO-A
 *   MB;              - Memory to VFO-B
 *   AM;              - VFO-A to Memory (store current VFO-A to memory)
 *   BM;              - VFO-B to Memory (store current VFO-B to memory)
 *   MZ P1 P2 P3;     - Memory Zone (lower/upper band limits)
 *   QR P1;           - Quick Memory Recall (P1=0-9)
 *   QS;              - Quick Memory Store
 *
 * Memory Channel Ranges:
 *   001-099 = Regular memory channels
 *   100-117 = Special channels (P1-P9, PMS, etc.)
 */

#include <stdlib.h>
#include <string.h>
#include <hamlib/rig.h>
#include "misc.h"
#include "yaesu.h"
#include "newcat.h"

#define FTX1_MEM_MIN 1
#define FTX1_MEM_MAX 117
#define FTX1_MEM_REGULAR_MAX 99

/* Set Memory Channel (MC P1P2P3;) */
int ftx1_set_mem(RIG *rig, vfo_t vfo, int ch)
{
    struct newcat_priv_data *priv = STATE(rig)->priv;

    (void)vfo;

    if (ch < FTX1_MEM_MIN || ch > FTX1_MEM_MAX) {
        rig_debug(RIG_DEBUG_ERR, "%s: channel %d out of range %d-%d\n",
                  __func__, ch, FTX1_MEM_MIN, FTX1_MEM_MAX);
        return -RIG_EINVAL;
    }

    rig_debug(RIG_DEBUG_VERBOSE, "%s: ch=%d\n", __func__, ch);

    SNPRINTF(priv->cmd_str, sizeof(priv->cmd_str), "MC%03d;", ch);
    return newcat_set_cmd(rig);
}

/* Get Memory Channel */
int ftx1_get_mem(RIG *rig, vfo_t vfo, int *ch)
{
    int ret, channel;
    struct newcat_priv_data *priv = STATE(rig)->priv;

    (void)vfo;

    rig_debug(RIG_DEBUG_VERBOSE, "%s\n", __func__);

    SNPRINTF(priv->cmd_str, sizeof(priv->cmd_str), "MC;");

    ret = newcat_get_cmd(rig);
    if (ret != RIG_OK) return ret;

    if (sscanf(priv->ret_data + 2, "%03d", &channel) != 1) {
        rig_debug(RIG_DEBUG_ERR, "%s: failed to parse '%s'\n", __func__, priv->ret_data);
        return -RIG_EPROTO;
    }

    *ch = channel;

    rig_debug(RIG_DEBUG_VERBOSE, "%s: ch=%d\n", __func__, *ch);

    return RIG_OK;
}

/* Memory Write (MW;) - write VFO to current memory channel */
int ftx1_set_channel(RIG *rig, vfo_t vfo, const channel_t *chan)
{
    struct newcat_priv_data *priv = STATE(rig)->priv;
    int ret;

    (void)vfo;

    /* First select the channel */
    if (chan->channel_num < FTX1_MEM_MIN || chan->channel_num > FTX1_MEM_MAX) {
        return -RIG_EINVAL;
    }

    ret = ftx1_set_mem(rig, vfo, chan->channel_num);
    if (ret != RIG_OK) return ret;

    rig_debug(RIG_DEBUG_VERBOSE, "%s: writing to channel %d\n", __func__,
              chan->channel_num);

    /* Then write current VFO to it */
    SNPRINTF(priv->cmd_str, sizeof(priv->cmd_str), "MW;");
    return newcat_set_cmd(rig);
}

/* Memory Read (MR;) - read current memory channel to VFO */
int ftx1_get_channel(RIG *rig, vfo_t vfo, channel_t *chan, int read_only)
{
    int ret;
    struct newcat_priv_data *priv = STATE(rig)->priv;

    (void)vfo;
    (void)read_only;

    rig_debug(RIG_DEBUG_VERBOSE, "%s\n", __func__);

    /* First get current channel number */
    ret = ftx1_get_mem(rig, vfo, &chan->channel_num);
    if (ret != RIG_OK) return ret;

    /* Then read memory data */
    SNPRINTF(priv->cmd_str, sizeof(priv->cmd_str), "MR;");

    ret = newcat_get_cmd(rig);
    if (ret != RIG_OK) return ret;

    /* Parse MR response - format depends on radio firmware */
    /* MR returns frequency, mode, and other data */
    /* Basic parsing - extract frequency if present */
    if (strlen(priv->ret_data) > 10) {
        freq_t freq;
        if (sscanf(priv->ret_data + 2, "%9lf", &freq) == 1) {
            chan->freq = freq;
        }
    }

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

/* Get Memory Tag/Name (MT P1P2P3;) */
int ftx1_get_mem_name(RIG *rig, int ch, char *name, size_t name_len)
{
    int ret;
    struct newcat_priv_data *priv = STATE(rig)->priv;

    rig_debug(RIG_DEBUG_VERBOSE, "%s: ch=%d\n", __func__, ch);

    if (ch < FTX1_MEM_MIN || ch > FTX1_MEM_MAX) {
        return -RIG_EINVAL;
    }

    SNPRINTF(priv->cmd_str, sizeof(priv->cmd_str), "MT%03d;", ch);

    ret = newcat_get_cmd(rig);
    if (ret != RIG_OK) return ret;

    /* Response: MT P1P2P3 name; - extract name after channel number */
    if (strlen(priv->ret_data) > 5) {
        strncpy(name, priv->ret_data + 5, name_len - 1);
        name[name_len - 1] = '\0';
        /* Remove trailing semicolon if present */
        size_t len = strlen(name);
        if (len > 0 && name[len - 1] == ';') {
            name[len - 1] = '\0';
        }
    } else {
        name[0] = '\0';
    }

    return RIG_OK;
}

/* Quick Memory Store (QS;) */
int ftx1_quick_mem_store(RIG *rig)
{
    struct newcat_priv_data *priv = STATE(rig)->priv;

    rig_debug(RIG_DEBUG_VERBOSE, "%s\n", __func__);

    SNPRINTF(priv->cmd_str, sizeof(priv->cmd_str), "QS;");
    return newcat_set_cmd(rig);
}

/* Quick Memory Recall (QR P1;) */
int ftx1_quick_mem_recall(RIG *rig, int slot)
{
    struct newcat_priv_data *priv = STATE(rig)->priv;

    rig_debug(RIG_DEBUG_VERBOSE, "%s: slot=%d\n", __func__, slot);

    if (slot < 0 || slot > 9) {
        return -RIG_EINVAL;
    }

    SNPRINTF(priv->cmd_str, sizeof(priv->cmd_str), "QR%1d;", slot);
    return newcat_set_cmd(rig);
}

/* VFO/Memory mode selection helper */
int ftx1_set_vfo_mem_mode(RIG *rig, vfo_t vfo)
{
    struct newcat_priv_data *priv = STATE(rig)->priv;
    int p1;

    /* VM P1; - P1: 0=VFO mode, 1=Memory mode, 2=Memory Tune mode */
    if (vfo == RIG_VFO_MEM) {
        p1 = 1;
    } else {
        p1 = 0;
    }

    rig_debug(RIG_DEBUG_VERBOSE, "%s: vfo=%s p1=%d\n", __func__,
              rig_strvfo(vfo), p1);

    SNPRINTF(priv->cmd_str, sizeof(priv->cmd_str), "VM%1d;", p1);
    return newcat_set_cmd(rig);
}

/* Get VFO/Memory mode */
int ftx1_get_vfo_mem_mode(RIG *rig, vfo_t *vfo)
{
    int ret, p1;
    struct newcat_priv_data *priv = STATE(rig)->priv;

    rig_debug(RIG_DEBUG_VERBOSE, "%s\n", __func__);

    SNPRINTF(priv->cmd_str, sizeof(priv->cmd_str), "VM;");

    ret = newcat_get_cmd(rig);
    if (ret != RIG_OK) return ret;

    if (sscanf(priv->ret_data + 2, "%1d", &p1) != 1) {
        rig_debug(RIG_DEBUG_ERR, "%s: failed to parse '%s'\n", __func__, priv->ret_data);
        return -RIG_EPROTO;
    }

    *vfo = (p1 == 1) ? RIG_VFO_MEM : RIG_VFO_CURR;

    rig_debug(RIG_DEBUG_VERBOSE, "%s: p1=%d vfo=%s\n", __func__, p1,
              rig_strvfo(*vfo));

    return RIG_OK;
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
 * ftx1_set_mem_zone - Set Memory Zone limits (MZ)
 * CAT command: MZ P1P1P1P1P1 P2 P3P3P3P3P3P3P3P3P3;
 *   P1 = Channel number (5 digits, 00001-00999)
 *   P2 = Limit type (0=lower, 1=upper)
 *   P3 = Frequency (9 digits in Hz)
 */
int ftx1_set_mem_zone(RIG *rig, int channel, int limit_type, freq_t freq)
{
    struct newcat_priv_data *priv = STATE(rig)->priv;

    rig_debug(RIG_DEBUG_VERBOSE, "%s: ch=%d limit=%d freq=%.0f\n", __func__,
              channel, limit_type, freq);

    if (channel < 1 || channel > 999) {
        return -RIG_EINVAL;
    }

    if (limit_type < 0 || limit_type > 1) {
        return -RIG_EINVAL;
    }

    SNPRINTF(priv->cmd_str, sizeof(priv->cmd_str), "MZ%05d%d%09.0f;",
             channel, limit_type, freq);

    return newcat_set_cmd(rig);
}

/*
 * ftx1_get_mem_zone - Get Memory Zone limits (MZ)
 * CAT command: MZ P1P1P1P1P1; Response: MZ P1P1P1P1P1 P2 P3P3P3P3P3P3P3P3P3;
 */
int ftx1_get_mem_zone(RIG *rig, int channel, int *limit_type, freq_t *freq)
{
    struct newcat_priv_data *priv = STATE(rig)->priv;
    int ret;
    int ch_resp, lim;
    double f;

    rig_debug(RIG_DEBUG_VERBOSE, "%s: ch=%d\n", __func__, channel);

    if (channel < 1 || channel > 999) {
        return -RIG_EINVAL;
    }

    SNPRINTF(priv->cmd_str, sizeof(priv->cmd_str), "MZ%05d;", channel);

    ret = newcat_get_cmd(rig);
    if (ret != RIG_OK) return ret;

    /* Response: MZ P1P1P1P1P1 P2 P3P3P3P3P3P3P3P3P3; */
    if (sscanf(priv->ret_data + 2, "%5d%1d%9lf", &ch_resp, &lim, &f) != 3)
    {
        rig_debug(RIG_DEBUG_ERR, "%s: failed to parse '%s'\n", __func__,
                  priv->ret_data);
        return -RIG_EPROTO;
    }

    *limit_type = lim;
    *freq = f;

    rig_debug(RIG_DEBUG_VERBOSE, "%s: limit=%d freq=%.0f\n", __func__,
              *limit_type, *freq);

    return RIG_OK;
}
