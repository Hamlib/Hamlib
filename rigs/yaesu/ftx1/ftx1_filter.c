/*
 * Hamlib Yaesu backend - FTX-1 Filter Commands
 * Copyright (c) 2025 by Terrell Deppe (KJ5HST)
 *
 * This file implements CAT commands for notch, contour, and audio peak filters.
 *
 * CAT Commands in this file:
 *   BC P1 P2;           - Beat Cancel/ANF (P1=VFO 0/1, P2=0 off/1 on)
 *   BP P1 P2 P3P3P3;    - Manual Notch (P1=VFO, P2=0 on/off or 1 freq, P3=value)
 *                         P2=0: P3=000 OFF, 001 ON
 *                         P2=1: P3=001-320 (x10 Hz = 10-3200 Hz notch freq)
 *   CO P1 P2 P3P3P3P3;  - Contour/APF (P1=VFO, P2=type, P3=4-digit value)
 *                         P2=0: CONTOUR on/off (P3=0000 OFF, 0001 ON)
 *                         P2=1: CONTOUR freq (P3=0010-3200 Hz)
 *                         P2=2: APF on/off (P3=0000 OFF, 0001 ON)
 *                         P2=3: APF freq (P3=0000-0050, maps to -250 to +250 Hz)
 *   FN P1;              - Fine Tuning (P1: 0=OFF, 1=Fine ON, 2=Fast ON)
 */

#include <stdlib.h>
#include <string.h>
#include <hamlib/rig.h>
#include "misc.h"
#include "yaesu.h"
#include "newcat.h"
#include "ftx1.h"

/*
 * ftx1_set_anf_helper - Set Auto Notch Filter (Beat Cancel) on/off
 * CAT command: BC P1 P2;
 */
int ftx1_set_anf_helper(RIG *rig, vfo_t vfo, int status)
{
    struct newcat_priv_data *priv = STATE(rig)->priv;
    int p1 = ftx1_vfo_to_p1(rig, vfo);

    rig_debug(RIG_DEBUG_VERBOSE, "%s: vfo=%s p1=%d status=%d\n", __func__,
              rig_strvfo(vfo), p1, status);

    SNPRINTF(priv->cmd_str, sizeof(priv->cmd_str), "BC%d%d;", p1, status ? 1 : 0);

    return newcat_set_cmd(rig);
}

/*
 * ftx1_get_anf_helper - Get Auto Notch Filter (Beat Cancel) status
 * CAT command: BC P1;
 */
int ftx1_get_anf_helper(RIG *rig, vfo_t vfo, int *status)
{
    struct newcat_priv_data *priv = STATE(rig)->priv;
    int ret;
    int p1 = ftx1_vfo_to_p1(rig, vfo);
    int p1_resp, p2;

    rig_debug(RIG_DEBUG_VERBOSE, "%s: vfo=%s p1=%d\n", __func__, rig_strvfo(vfo), p1);

    SNPRINTF(priv->cmd_str, sizeof(priv->cmd_str), "BC%d;", p1);

    ret = newcat_get_cmd(rig);

    if (ret != RIG_OK)
    {
        return ret;
    }

    // Response: BC01 (VFO=0, status=1)
    if (sscanf(priv->ret_data + 2, "%1d%1d", &p1_resp, &p2) != 2)
    {
        rig_debug(RIG_DEBUG_ERR, "%s: failed to parse '%s'\n", __func__,
                  priv->ret_data);
        return -RIG_EPROTO;
    }

    *status = p2;

    return RIG_OK;
}

/*
 * ftx1_set_mn_helper - Set Manual Notch on/off
 * CAT command: BP P1 0 P3P3P3; (P2=0 for on/off, P3=000 OFF, 001 ON)
 */
int ftx1_set_mn_helper(RIG *rig, vfo_t vfo, int status)
{
    struct newcat_priv_data *priv = STATE(rig)->priv;
    int p1 = ftx1_vfo_to_p1(rig, vfo);

    rig_debug(RIG_DEBUG_VERBOSE, "%s: vfo=%s p1=%d status=%d\n", __func__,
              rig_strvfo(vfo), p1, status);

    SNPRINTF(priv->cmd_str, sizeof(priv->cmd_str), "BP%d0%03d;", p1, status ? 1 : 0);

    return newcat_set_cmd(rig);
}

/*
 * ftx1_get_mn_helper - Get Manual Notch status
 * CAT command: BP P1 0;
 */
int ftx1_get_mn_helper(RIG *rig, vfo_t vfo, int *status)
{
    struct newcat_priv_data *priv = STATE(rig)->priv;
    int ret;
    int p1 = ftx1_vfo_to_p1(rig, vfo);
    int p1_resp, p2, p3;

    rig_debug(RIG_DEBUG_VERBOSE, "%s: vfo=%s p1=%d\n", __func__, rig_strvfo(vfo), p1);

    SNPRINTF(priv->cmd_str, sizeof(priv->cmd_str), "BP%d0;", p1);

    ret = newcat_get_cmd(rig);

    if (ret != RIG_OK)
    {
        return ret;
    }

    // Response: BP00001 (VFO=0, P2=0, P3=001 ON)
    if (sscanf(priv->ret_data + 2, "%1d%1d%3d", &p1_resp, &p2, &p3) != 3)
    {
        rig_debug(RIG_DEBUG_ERR, "%s: failed to parse '%s'\n", __func__,
                  priv->ret_data);
        return -RIG_EPROTO;
    }

    *status = (p3 == 1) ? 1 : 0;

    return RIG_OK;
}

/*
 * ftx1_set_apf_helper - Set Audio Peak Filter on/off
 * CAT command: CO P1 2 P3P3P3P3; (P2=2 for APF on/off)
 */
int ftx1_set_apf_helper(RIG *rig, vfo_t vfo, int status)
{
    struct newcat_priv_data *priv = STATE(rig)->priv;
    int p1 = ftx1_vfo_to_p1(rig, vfo);

    rig_debug(RIG_DEBUG_VERBOSE, "%s: vfo=%s p1=%d status=%d\n", __func__,
              rig_strvfo(vfo), p1, status);

    SNPRINTF(priv->cmd_str, sizeof(priv->cmd_str), "CO%d2%04d;", p1, status ? 1 : 0);

    return newcat_set_cmd(rig);
}

/*
 * ftx1_get_apf_helper - Get Audio Peak Filter status
 * CAT command: CO P1 2;
 */
int ftx1_get_apf_helper(RIG *rig, vfo_t vfo, int *status)
{
    struct newcat_priv_data *priv = STATE(rig)->priv;
    int ret;
    int p1 = ftx1_vfo_to_p1(rig, vfo);
    int p1_resp, p2, p3;

    rig_debug(RIG_DEBUG_VERBOSE, "%s: vfo=%s p1=%d\n", __func__, rig_strvfo(vfo), p1);

    SNPRINTF(priv->cmd_str, sizeof(priv->cmd_str), "CO%d2;", p1);

    ret = newcat_get_cmd(rig);

    if (ret != RIG_OK)
    {
        return ret;
    }

    // Response: CO020001 (VFO=0, P2=2, P3=0001 ON)
    if (sscanf(priv->ret_data + 2, "%1d%1d%4d", &p1_resp, &p2, &p3) != 3)
    {
        rig_debug(RIG_DEBUG_ERR, "%s: failed to parse '%s'\n", __func__,
                  priv->ret_data);
        return -RIG_EPROTO;
    }

    *status = (p3 == 1) ? 1 : 0;

    return RIG_OK;
}

/*
 * ftx1_set_notchf_helper - Set Manual Notch frequency
 * CAT command: BP P1 1 P3P3P3; (P2=1 for freq, P3=001-320 x10Hz)
 */
int ftx1_set_notchf_helper(RIG *rig, vfo_t vfo, value_t val)
{
    struct newcat_priv_data *priv = STATE(rig)->priv;
    int p1 = ftx1_vfo_to_p1(rig, vfo);
    int p3;

    rig_debug(RIG_DEBUG_VERBOSE, "%s: vfo=%s p1=%d freq=%d\n", __func__,
              rig_strvfo(vfo), p1, val.i);

    // Convert Hz to command units (x10)
    p3 = val.i / 10;

    if (p3 < 1)
    {
        p3 = 1;
    }

    if (p3 > 320)
    {
        p3 = 320;
    }

    SNPRINTF(priv->cmd_str, sizeof(priv->cmd_str), "BP%d1%03d;", p1, p3);

    return newcat_set_cmd(rig);
}

/*
 * ftx1_get_notchf_helper - Get Manual Notch frequency
 * CAT command: BP P1 1;
 */
int ftx1_get_notchf_helper(RIG *rig, vfo_t vfo, value_t *val)
{
    struct newcat_priv_data *priv = STATE(rig)->priv;
    int ret;
    int p1 = ftx1_vfo_to_p1(rig, vfo);
    int p1_resp, p2, p3;

    rig_debug(RIG_DEBUG_VERBOSE, "%s: vfo=%s p1=%d\n", __func__, rig_strvfo(vfo), p1);

    SNPRINTF(priv->cmd_str, sizeof(priv->cmd_str), "BP%d1;", p1);

    ret = newcat_get_cmd(rig);

    if (ret != RIG_OK)
    {
        return ret;
    }

    // Response: BP01320 (VFO=0, P2=1, P3=320)
    if (sscanf(priv->ret_data + 2, "%1d%1d%3d", &p1_resp, &p2, &p3) != 3)
    {
        rig_debug(RIG_DEBUG_ERR, "%s: failed to parse '%s'\n", __func__,
                  priv->ret_data);
        return -RIG_EPROTO;
    }

    val->i = p3 * 10;  // Convert to Hz

    return RIG_OK;
}

/*
 * ftx1_set_apf_level_helper - Set APF frequency offset
 * CAT command: CO P1 3 P3P3P3P3; (P2=3 for APF freq, P3=0000-0050)
 */
int ftx1_set_apf_level_helper(RIG *rig, vfo_t vfo, value_t val)
{
    struct newcat_priv_data *priv = STATE(rig)->priv;
    int p1 = ftx1_vfo_to_p1(rig, vfo);
    int p3;

    rig_debug(RIG_DEBUG_VERBOSE, "%s: vfo=%s p1=%d level=%f\n", __func__,
              rig_strvfo(vfo), p1, val.f);

    // val.f is 0.0-1.0, map to 0-50
    p3 = (int)(val.f * 50);

    if (p3 < 0)
    {
        p3 = 0;
    }

    if (p3 > 50)
    {
        p3 = 50;
    }

    SNPRINTF(priv->cmd_str, sizeof(priv->cmd_str), "CO%d3%04d;", p1, p3);

    return newcat_set_cmd(rig);
}

/*
 * ftx1_get_apf_level_helper - Get APF frequency offset
 * CAT command: CO P1 3;
 */
int ftx1_get_apf_level_helper(RIG *rig, vfo_t vfo, value_t *val)
{
    struct newcat_priv_data *priv = STATE(rig)->priv;
    int ret;
    int p1 = ftx1_vfo_to_p1(rig, vfo);
    int p1_resp, p2, p3;

    rig_debug(RIG_DEBUG_VERBOSE, "%s: vfo=%s p1=%d\n", __func__, rig_strvfo(vfo), p1);

    SNPRINTF(priv->cmd_str, sizeof(priv->cmd_str), "CO%d3;", p1);

    ret = newcat_get_cmd(rig);

    if (ret != RIG_OK)
    {
        return ret;
    }

    // Response: CO030025 (VFO=0, P2=3, P3=0025)
    if (sscanf(priv->ret_data + 2, "%1d%1d%4d", &p1_resp, &p2, &p3) != 3)
    {
        rig_debug(RIG_DEBUG_ERR, "%s: failed to parse '%s'\n", __func__,
                  priv->ret_data);
        return -RIG_EPROTO;
    }

    val->f = (float)p3 / 50.0f;  // Convert 0-50 to 0.0-1.0

    return RIG_OK;
}

/*
 * ftx1_set_fine_tuning - Set Fine Tuning mode
 * CAT command: FN P1; (P1: 0=OFF, 1=Fine ON, 2=Fast ON)
 *
 * Spec (page 16): FN is "FINE TUNING" control, not filter number
 */
int ftx1_set_fine_tuning(RIG *rig, int mode)
{
    struct newcat_priv_data *priv = STATE(rig)->priv;

    rig_debug(RIG_DEBUG_VERBOSE, "%s: mode=%d\n", __func__, mode);

    if (mode < 0 || mode > 2)
    {
        rig_debug(RIG_DEBUG_ERR, "%s: invalid fine tuning mode %d\n",
                  __func__, mode);
        return -RIG_EINVAL;
    }

    SNPRINTF(priv->cmd_str, sizeof(priv->cmd_str), "FN%d;", mode);

    return newcat_set_cmd(rig);
}

/*
 * ftx1_get_fine_tuning - Get Fine Tuning mode
 * CAT command: FN; Response: FN P1;
 *
 * Returns: 0=OFF, 1=Fine ON, 2=Fast ON
 */
int ftx1_get_fine_tuning(RIG *rig, int *mode)
{
    struct newcat_priv_data *priv = STATE(rig)->priv;
    int ret;
    int p1;

    rig_debug(RIG_DEBUG_VERBOSE, "%s\n", __func__);

    SNPRINTF(priv->cmd_str, sizeof(priv->cmd_str), "FN;");

    ret = newcat_get_cmd(rig);

    if (ret != RIG_OK)
    {
        return ret;
    }

    /* Response: FN0, FN1, or FN2 (single digit) */
    if (sscanf(priv->ret_data + 2, "%1d", &p1) != 1)
    {
        rig_debug(RIG_DEBUG_ERR, "%s: failed to parse '%s'\n", __func__,
                  priv->ret_data);
        return -RIG_EPROTO;
    }

    *mode = p1;

    return RIG_OK;
}

/*
 * ftx1_set_contour - Set Contour on/off
 * CAT command: CO P1 0 P3P3P3P3; (P2=0 for on/off)
 */
int ftx1_set_contour(RIG *rig, vfo_t vfo, int status)
{
    struct newcat_priv_data *priv = STATE(rig)->priv;
    int p1 = ftx1_vfo_to_p1(rig, vfo);

    rig_debug(RIG_DEBUG_VERBOSE, "%s: vfo=%s p1=%d status=%d\n", __func__,
              rig_strvfo(vfo), p1, status);

    SNPRINTF(priv->cmd_str, sizeof(priv->cmd_str), "CO%d0%04d;", p1, status ? 1 : 0);

    return newcat_set_cmd(rig);
}

/*
 * ftx1_get_contour - Get Contour status
 * CAT command: CO P1 0;
 */
int ftx1_get_contour(RIG *rig, vfo_t vfo, int *status)
{
    struct newcat_priv_data *priv = STATE(rig)->priv;
    int ret;
    int p1 = ftx1_vfo_to_p1(rig, vfo);
    int p1_resp, p2, p3;

    rig_debug(RIG_DEBUG_VERBOSE, "%s: vfo=%s p1=%d\n", __func__, rig_strvfo(vfo), p1);

    SNPRINTF(priv->cmd_str, sizeof(priv->cmd_str), "CO%d0;", p1);

    ret = newcat_get_cmd(rig);

    if (ret != RIG_OK)
    {
        return ret;
    }

    /* Response: CO000001 (VFO=0, P2=0, P3=0001 ON) */
    if (sscanf(priv->ret_data + 2, "%1d%1d%4d", &p1_resp, &p2, &p3) != 3)
    {
        rig_debug(RIG_DEBUG_ERR, "%s: failed to parse '%s'\n", __func__,
                  priv->ret_data);
        return -RIG_EPROTO;
    }

    *status = (p3 == 1) ? 1 : 0;

    return RIG_OK;
}

/*
 * ftx1_set_contour_freq - Set Contour frequency
 * CAT command: CO P1 1 P3P3P3P3; (P2=1 for freq, P3=0010-3200 Hz)
 */
int ftx1_set_contour_freq(RIG *rig, vfo_t vfo, int freq)
{
    struct newcat_priv_data *priv = STATE(rig)->priv;
    int p1 = ftx1_vfo_to_p1(rig, vfo);

    rig_debug(RIG_DEBUG_VERBOSE, "%s: vfo=%s p1=%d freq=%d\n", __func__,
              rig_strvfo(vfo), p1, freq);

    if (freq < 10) freq = 10;
    if (freq > 3200) freq = 3200;

    SNPRINTF(priv->cmd_str, sizeof(priv->cmd_str), "CO%d1%04d;", p1, freq);

    return newcat_set_cmd(rig);
}

/*
 * ftx1_get_contour_freq - Get Contour frequency
 * CAT command: CO P1 1;
 */
int ftx1_get_contour_freq(RIG *rig, vfo_t vfo, int *freq)
{
    struct newcat_priv_data *priv = STATE(rig)->priv;
    int ret;
    int p1 = ftx1_vfo_to_p1(rig, vfo);
    int p1_resp, p2, p3;

    rig_debug(RIG_DEBUG_VERBOSE, "%s: vfo=%s p1=%d\n", __func__, rig_strvfo(vfo), p1);

    SNPRINTF(priv->cmd_str, sizeof(priv->cmd_str), "CO%d1;", p1);

    ret = newcat_get_cmd(rig);

    if (ret != RIG_OK)
    {
        return ret;
    }

    /* Response: CO011000 (VFO=0, P2=1, P3=1000 Hz) */
    if (sscanf(priv->ret_data + 2, "%1d%1d%4d", &p1_resp, &p2, &p3) != 3)
    {
        rig_debug(RIG_DEBUG_ERR, "%s: failed to parse '%s'\n", __func__,
                  priv->ret_data);
        return -RIG_EPROTO;
    }

    *freq = p3;

    return RIG_OK;
}
