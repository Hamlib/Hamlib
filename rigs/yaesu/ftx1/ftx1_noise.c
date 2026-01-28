/*
 * Hamlib Yaesu backend - FTX-1 Noise Reduction Group
 * Copyright (c) 2025 by Terrell Deppe (KJ5HST)
 *
 * This file implements helper functions for noise blanker and digital noise reduction.
 *
 * CAT Commands in this file:
 *   NL P1 P2P3P4;    - Noise Blanker Level (P1=VFO 0/1, P2P3P4=000-010)
 *                      Set to 000 for OFF, 001-010 for level
 *   RL P1 P2P3;      - Noise Reduction Level (P1=VFO 0/1, P2P3=00-15)
 *                      Set to 00 for OFF, 01-15 for level
 *
 * Note: NB and NR on/off commands return ?; on FTX-1.
 *       Use NL/RL level commands instead (level 0 = OFF).
 */

#include <stdlib.h>
#include <string.h>
#include <hamlib/rig.h>
#include "misc.h"
#include "yaesu.h"
#include "newcat.h"
#include "ftx1.h"

/* Level ranges */
#define FTX1_NB_LEVEL_MIN 0
#define FTX1_NB_LEVEL_MAX 10   /* 0=OFF, 1-10=level */
#define FTX1_NR_LEVEL_MIN 0
#define FTX1_NR_LEVEL_MAX 15   /* 0=OFF, 1-15=level */

/*
 * ftx1_set_nb_helper - Set Noise Blanker on/off via NL level
 *
 * FTX-1 doesn't have NB on/off command, so we use NL level:
 *   NL P1 000; = OFF
 *   NL P1 005; = ON (mid-level)
 */
int ftx1_set_nb_helper(RIG *rig, vfo_t vfo, int status)
{
    struct newcat_priv_data *priv = STATE(rig)->priv;
    int p1 = ftx1_vfo_to_p1(rig, vfo);
    int level = status ? 5 : 0;  /* Default to mid-level when turning on */

    rig_debug(RIG_DEBUG_VERBOSE, "%s: vfo=%s p1=%d status=%d\n", __func__,
              rig_strvfo(vfo), p1, status);

    SNPRINTF(priv->cmd_str, sizeof(priv->cmd_str), "NL%d%03d;", p1, level);
    return newcat_set_cmd(rig);
}

/*
 * ftx1_get_nb_helper - Get Noise Blanker on/off status
 *
 * Query NL level - if > 0, NB is on
 */
int ftx1_get_nb_helper(RIG *rig, vfo_t vfo, int *status)
{
    struct newcat_priv_data *priv = STATE(rig)->priv;
    int ret;
    int p1 = ftx1_vfo_to_p1(rig, vfo);
    int p1_resp, level;

    rig_debug(RIG_DEBUG_VERBOSE, "%s: vfo=%s p1=%d\n", __func__,
              rig_strvfo(vfo), p1);

    SNPRINTF(priv->cmd_str, sizeof(priv->cmd_str), "NL%d;", p1);

    ret = newcat_get_cmd(rig);
    if (ret != RIG_OK) return ret;

    /* Response: NL P1 P2P3P4; (VFO + 3-digit level 000-010) */
    if (sscanf(priv->ret_data + 2, "%1d%3d", &p1_resp, &level) != 2)
    {
        rig_debug(RIG_DEBUG_ERR, "%s: failed to parse '%s'\n", __func__,
                  priv->ret_data);
        return -RIG_EPROTO;
    }

    *status = (level > 0) ? 1 : 0;
    return RIG_OK;
}

/*
 * ftx1_set_nr_helper - Set Noise Reduction on/off via RL level
 *
 * FTX-1 doesn't have NR on/off command, so we use RL level:
 *   RL P1 00; = OFF
 *   RL P1 08; = ON (mid-level)
 */
int ftx1_set_nr_helper(RIG *rig, vfo_t vfo, int status)
{
    struct newcat_priv_data *priv = STATE(rig)->priv;
    int p1 = ftx1_vfo_to_p1(rig, vfo);
    int level = status ? 8 : 0;  /* Default to mid-level when turning on */

    rig_debug(RIG_DEBUG_VERBOSE, "%s: vfo=%s p1=%d status=%d\n", __func__,
              rig_strvfo(vfo), p1, status);

    SNPRINTF(priv->cmd_str, sizeof(priv->cmd_str), "RL%d%02d;", p1, level);
    return newcat_set_cmd(rig);
}

/*
 * ftx1_get_nr_helper - Get Noise Reduction on/off status
 *
 * Query RL level - if > 0, NR is on
 */
int ftx1_get_nr_helper(RIG *rig, vfo_t vfo, int *status)
{
    struct newcat_priv_data *priv = STATE(rig)->priv;
    int ret;
    int p1 = ftx1_vfo_to_p1(rig, vfo);
    int p1_resp, level;

    rig_debug(RIG_DEBUG_VERBOSE, "%s: vfo=%s p1=%d\n", __func__,
              rig_strvfo(vfo), p1);

    SNPRINTF(priv->cmd_str, sizeof(priv->cmd_str), "RL%d;", p1);

    ret = newcat_get_cmd(rig);
    if (ret != RIG_OK) return ret;

    /* Response: RL P1 P2P3; (VFO + 2-digit level 00-15) */
    if (sscanf(priv->ret_data + 2, "%1d%2d", &p1_resp, &level) != 2)
    {
        rig_debug(RIG_DEBUG_ERR, "%s: failed to parse '%s'\n", __func__,
                  priv->ret_data);
        return -RIG_EPROTO;
    }

    *status = (level > 0) ? 1 : 0;
    return RIG_OK;
}

/*
 * ftx1_set_nb_level_helper - Set Noise Blanker Level
 * CAT command: NL P1 P2P3P4; (P1=VFO, P2P3P4=000-010)
 */
int ftx1_set_nb_level_helper(RIG *rig, vfo_t vfo, value_t val)
{
    struct newcat_priv_data *priv = STATE(rig)->priv;
    int p1 = ftx1_vfo_to_p1(rig, vfo);
    int level = (int)(val.f * FTX1_NB_LEVEL_MAX);

    rig_debug(RIG_DEBUG_VERBOSE, "%s: vfo=%s p1=%d level=%d\n", __func__,
              rig_strvfo(vfo), p1, level);

    if (level < FTX1_NB_LEVEL_MIN) level = FTX1_NB_LEVEL_MIN;
    if (level > FTX1_NB_LEVEL_MAX) level = FTX1_NB_LEVEL_MAX;

    SNPRINTF(priv->cmd_str, sizeof(priv->cmd_str), "NL%d%03d;", p1, level);
    return newcat_set_cmd(rig);
}

/*
 * ftx1_get_nb_level_helper - Get Noise Blanker Level
 * CAT command: NL P1; Response: NL P1 P2P3P4;
 */
int ftx1_get_nb_level_helper(RIG *rig, vfo_t vfo, value_t *val)
{
    struct newcat_priv_data *priv = STATE(rig)->priv;
    int ret;
    int p1 = ftx1_vfo_to_p1(rig, vfo);
    int p1_resp, level;

    rig_debug(RIG_DEBUG_VERBOSE, "%s: vfo=%s p1=%d\n", __func__,
              rig_strvfo(vfo), p1);

    SNPRINTF(priv->cmd_str, sizeof(priv->cmd_str), "NL%d;", p1);

    ret = newcat_get_cmd(rig);
    if (ret != RIG_OK) return ret;

    /* Response: NL P1 P2P3P4; (3-digit level 000-010) */
    if (sscanf(priv->ret_data + 2, "%1d%3d", &p1_resp, &level) != 2)
    {
        rig_debug(RIG_DEBUG_ERR, "%s: failed to parse '%s'\n", __func__,
                  priv->ret_data);
        return -RIG_EPROTO;
    }

    val->f = (float)level / FTX1_NB_LEVEL_MAX;
    return RIG_OK;
}

/*
 * ftx1_set_nr_level_helper - Set Noise Reduction Level
 * CAT command: RL P1 P2P3; (P1=VFO, P2P3=00-15)
 */
int ftx1_set_nr_level_helper(RIG *rig, vfo_t vfo, value_t val)
{
    struct newcat_priv_data *priv = STATE(rig)->priv;
    int p1 = ftx1_vfo_to_p1(rig, vfo);
    int level = (int)(val.f * FTX1_NR_LEVEL_MAX);

    rig_debug(RIG_DEBUG_VERBOSE, "%s: vfo=%s p1=%d level=%d\n", __func__,
              rig_strvfo(vfo), p1, level);

    if (level < FTX1_NR_LEVEL_MIN) level = FTX1_NR_LEVEL_MIN;
    if (level > FTX1_NR_LEVEL_MAX) level = FTX1_NR_LEVEL_MAX;

    SNPRINTF(priv->cmd_str, sizeof(priv->cmd_str), "RL%d%02d;", p1, level);
    return newcat_set_cmd(rig);
}

/*
 * ftx1_get_nr_level_helper - Get Noise Reduction Level
 * CAT command: RL P1; Response: RL P1 P2P3;
 */
int ftx1_get_nr_level_helper(RIG *rig, vfo_t vfo, value_t *val)
{
    struct newcat_priv_data *priv = STATE(rig)->priv;
    int ret;
    int p1 = ftx1_vfo_to_p1(rig, vfo);
    int p1_resp, level;

    rig_debug(RIG_DEBUG_VERBOSE, "%s: vfo=%s p1=%d\n", __func__,
              rig_strvfo(vfo), p1);

    SNPRINTF(priv->cmd_str, sizeof(priv->cmd_str), "RL%d;", p1);

    ret = newcat_get_cmd(rig);
    if (ret != RIG_OK) return ret;

    /* Response: RL P1 P2P3; (2-digit level 00-15) */
    if (sscanf(priv->ret_data + 2, "%1d%2d", &p1_resp, &level) != 2)
    {
        rig_debug(RIG_DEBUG_ERR, "%s: failed to parse '%s'\n", __func__,
                  priv->ret_data);
        return -RIG_EPROTO;
    }

    val->f = (float)level / FTX1_NR_LEVEL_MAX;
    return RIG_OK;
}

/*
 * ftx1_set_na_helper - Set Narrow filter (NA command)
 * CAT command: NA P1 P2; (P1=VFO 0/1, P2=0/1 off/on)
 */
int ftx1_set_na_helper(RIG *rig, vfo_t vfo, int status)
{
    struct newcat_priv_data *priv = STATE(rig)->priv;
    int p1 = ftx1_vfo_to_p1(rig, vfo);
    int p2 = status ? 1 : 0;

    rig_debug(RIG_DEBUG_VERBOSE, "%s: vfo=%s p1=%d status=%d\n", __func__,
              rig_strvfo(vfo), p1, status);

    SNPRINTF(priv->cmd_str, sizeof(priv->cmd_str), "NA%d%d;", p1, p2);
    return newcat_set_cmd(rig);
}

/*
 * ftx1_get_na_helper - Get Narrow filter status
 * CAT command: NA P1; Response: NA P1 P2;
 */
int ftx1_get_na_helper(RIG *rig, vfo_t vfo, int *status)
{
    struct newcat_priv_data *priv = STATE(rig)->priv;
    int ret;
    int p1 = ftx1_vfo_to_p1(rig, vfo);
    int p1_resp, p2;

    rig_debug(RIG_DEBUG_VERBOSE, "%s: vfo=%s p1=%d\n", __func__,
              rig_strvfo(vfo), p1);

    SNPRINTF(priv->cmd_str, sizeof(priv->cmd_str), "NA%d;", p1);

    ret = newcat_get_cmd(rig);
    if (ret != RIG_OK) return ret;

    /* Response: NA P1 P2; (P1=VFO, P2=on/off) */
    if (sscanf(priv->ret_data + 2, "%1d%1d", &p1_resp, &p2) != 2)
    {
        rig_debug(RIG_DEBUG_ERR, "%s: failed to parse '%s'\n", __func__,
                  priv->ret_data);
        return -RIG_EPROTO;
    }

    *status = p2;
    return RIG_OK;
}