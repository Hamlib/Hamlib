/*
 * Hamlib Yaesu backend - FTX-1 Audio and Levels Group
 * Copyright (c) 2025 by Terrell Deppe (KJ5HST)
 *
 * This file implements CAT commands for audio gain, RF gain, meters, and AGC.
 *
 * CAT Commands in this file:
 *   AG P1 P2P3P4;    - AF Gain (P1=VFO 0/1, P2-P4=000-255)
 *   RG P1 P2P3P4;    - RF Gain (P1=VFO 0/1, P2-P4=000-255)
 *   MG P1P2P3;       - Mic Gain (000-100)
 *   GT P1P2P3P4;     - AGC Time Constant (0000-6000ms or code)
 *   ML P1P2P3;       - Monitor Level (000-100)
 *   SM P1;           - S-Meter Read (P1=0 main, returns 0000-0255)
 *   RM P1;           - Meter Read (1=Main S, 2=Sub S, 3=COMP, 4=ALC, 5=PO, 6=SWR, 7=ID, 8=VDD)
 *   PC P1 P2P2P2;    - Power Control (P1=1 field head, P1=2 SPA-1)
 *                      Field head: 0.5-10W (can return decimal: PC10.5, PC11.5)
 *                      SPA-1: 5-100W (PC2005 to PC2100)
 *   VG P1P2P3;       - VOX Gain (000-100)
 *   VD P1P2P3P4;     - VOX Delay (0030-3000ms)
 *   SD P1P2P3P4;     - CW Break-in Delay (0030-3000ms)
 */

#include <stdlib.h>
#include <string.h>
#include <hamlib/rig.h>
#include "misc.h"
#include "yaesu.h"
#include "newcat.h"
#include "ftx1.h"

/* Level ranges */
#define FTX1_AF_GAIN_MIN 0
#define FTX1_AF_GAIN_MAX 255
#define FTX1_RF_GAIN_MIN 0
#define FTX1_RF_GAIN_MAX 255
#define FTX1_MIC_GAIN_MIN 0
#define FTX1_MIC_GAIN_MAX 100

/*
 * Power ranges by head type:
 *   FIELD_BATTERY: 0.5W - 6W (0.5W increments)
 *   FIELD_12V:     0.5W - 10W (0.5W increments)
 *   SPA1:          5W - 100W (1W increments) - "Optima" when attached to Field
 */
#define FTX1_POWER_MIN_FIELD 0.5f         /* Field head minimum (both modes) */
#define FTX1_POWER_MAX_FIELD_BATTERY 6.0f /* Field head on battery maximum */
#define FTX1_POWER_MAX_FIELD_12V 10.0f    /* Field head on 12V maximum */
#define FTX1_POWER_MIN_SPA1 5             /* Optima/SPA-1 minimum */
#define FTX1_POWER_MAX_SPA1 100           /* Optima/SPA-1 maximum */

/* Set AF Gain (AG P1 P2P3P4;) */
int ftx1_set_af_gain(RIG *rig, vfo_t vfo, float val)
{
    struct newcat_priv_data *priv = STATE(rig)->priv;
    int p1 = ftx1_vfo_to_p1(rig, vfo);
    int level = (int)(val * FTX1_AF_GAIN_MAX);

    rig_debug(RIG_DEBUG_VERBOSE, "%s: vfo=%s p1=%d val=%f\n", __func__,
              rig_strvfo(vfo), p1, val);

    if (level < FTX1_AF_GAIN_MIN) level = FTX1_AF_GAIN_MIN;
    if (level > FTX1_AF_GAIN_MAX) level = FTX1_AF_GAIN_MAX;

    SNPRINTF(priv->cmd_str, sizeof(priv->cmd_str), "AG%d%03d;", p1, level);
    return newcat_set_cmd(rig);
}

/* Get AF Gain */
int ftx1_get_af_gain(RIG *rig, vfo_t vfo, float *val)
{
    struct newcat_priv_data *priv = STATE(rig)->priv;
    int ret;
    int p1 = ftx1_vfo_to_p1(rig, vfo);
    int p1_resp, level;

    rig_debug(RIG_DEBUG_VERBOSE, "%s: vfo=%s p1=%d\n", __func__,
              rig_strvfo(vfo), p1);

    SNPRINTF(priv->cmd_str, sizeof(priv->cmd_str), "AG%d;", p1);

    ret = newcat_get_cmd(rig);
    if (ret != RIG_OK) return ret;

    if (sscanf(priv->ret_data + 2, "%1d%3d", &p1_resp, &level) != 2)
    {
        rig_debug(RIG_DEBUG_ERR, "%s: failed to parse '%s'\n", __func__,
                  priv->ret_data);
        return -RIG_EPROTO;
    }

    *val = (float)level / FTX1_AF_GAIN_MAX;
    return RIG_OK;
}

/* Set RF Gain (RG P1 P2P3P4;) */
int ftx1_set_rf_gain(RIG *rig, vfo_t vfo, float val)
{
    struct newcat_priv_data *priv = STATE(rig)->priv;
    int p1 = ftx1_vfo_to_p1(rig, vfo);
    int level = (int)(val * FTX1_RF_GAIN_MAX);

    rig_debug(RIG_DEBUG_VERBOSE, "%s: vfo=%s p1=%d val=%f\n", __func__,
              rig_strvfo(vfo), p1, val);

    if (level < FTX1_RF_GAIN_MIN) level = FTX1_RF_GAIN_MIN;
    if (level > FTX1_RF_GAIN_MAX) level = FTX1_RF_GAIN_MAX;

    SNPRINTF(priv->cmd_str, sizeof(priv->cmd_str), "RG%d%03d;", p1, level);
    return newcat_set_cmd(rig);
}

/* Get RF Gain */
int ftx1_get_rf_gain(RIG *rig, vfo_t vfo, float *val)
{
    struct newcat_priv_data *priv = STATE(rig)->priv;
    int ret;
    int p1 = ftx1_vfo_to_p1(rig, vfo);
    int p1_resp, level;

    rig_debug(RIG_DEBUG_VERBOSE, "%s: vfo=%s p1=%d\n", __func__,
              rig_strvfo(vfo), p1);

    SNPRINTF(priv->cmd_str, sizeof(priv->cmd_str), "RG%d;", p1);

    ret = newcat_get_cmd(rig);
    if (ret != RIG_OK) return ret;

    if (sscanf(priv->ret_data + 2, "%1d%3d", &p1_resp, &level) != 2)
    {
        rig_debug(RIG_DEBUG_ERR, "%s: failed to parse '%s'\n", __func__,
                  priv->ret_data);
        return -RIG_EPROTO;
    }

    *val = (float)level / FTX1_RF_GAIN_MAX;
    return RIG_OK;
}

/* Set Mic Gain (MG P1P2P3;) */
int ftx1_set_mic_gain(RIG *rig, float val)
{
    struct newcat_priv_data *priv = STATE(rig)->priv;
    int level = (int)(val * FTX1_MIC_GAIN_MAX);

    rig_debug(RIG_DEBUG_VERBOSE, "%s: val=%f\n", __func__, val);

    if (level < FTX1_MIC_GAIN_MIN) level = FTX1_MIC_GAIN_MIN;
    if (level > FTX1_MIC_GAIN_MAX) level = FTX1_MIC_GAIN_MAX;

    SNPRINTF(priv->cmd_str, sizeof(priv->cmd_str), "MG%03d;", level);
    return newcat_set_cmd(rig);
}

/* Get Mic Gain */
int ftx1_get_mic_gain(RIG *rig, float *val)
{
    struct newcat_priv_data *priv = STATE(rig)->priv;
    int ret;
    int level;

    rig_debug(RIG_DEBUG_VERBOSE, "%s\n", __func__);

    SNPRINTF(priv->cmd_str, sizeof(priv->cmd_str), "MG;");

    ret = newcat_get_cmd(rig);
    if (ret != RIG_OK) return ret;

    if (sscanf(priv->ret_data + 2, "%3d", &level) != 1)
    {
        rig_debug(RIG_DEBUG_ERR, "%s: failed to parse '%s'\n", __func__,
                  priv->ret_data);
        return -RIG_EPROTO;
    }

    *val = (float)level / FTX1_MIC_GAIN_MAX;
    return RIG_OK;
}

/* Get S-Meter (SM P1;) */
int ftx1_get_smeter(RIG *rig, vfo_t vfo, int *val)
{
    struct newcat_priv_data *priv = STATE(rig)->priv;
    int ret;
    int p1 = ftx1_vfo_to_p1(rig, vfo);
    int p1_resp, level;

    rig_debug(RIG_DEBUG_VERBOSE, "%s: vfo=%s p1=%d\n", __func__,
              rig_strvfo(vfo), p1);

    SNPRINTF(priv->cmd_str, sizeof(priv->cmd_str), "SM%d;", p1);

    ret = newcat_get_cmd(rig);
    if (ret != RIG_OK) return ret;

    /* Response: SM P1 P2P3P4; (P2-P4 is 3 digits 000-255 per spec) */
    if (sscanf(priv->ret_data + 2, "%1d%3d", &p1_resp, &level) != 2)
    {
        rig_debug(RIG_DEBUG_ERR, "%s: failed to parse '%s'\n", __func__,
                  priv->ret_data);
        return -RIG_EPROTO;
    }

    *val = level;
    return RIG_OK;
}

/* Get Meter (RM P1;) - various meters */
int ftx1_get_meter(RIG *rig, int meter_type, int *val)
{
    struct newcat_priv_data *priv = STATE(rig)->priv;
    int ret;
    int p1_resp, level;

    rig_debug(RIG_DEBUG_VERBOSE, "%s: meter_type=%d\n", __func__, meter_type);

    /* meter_type per CAT manual p.23: 1=Main S, 2=Sub S, 3=COMP, 4=ALC, 5=PO, 6=SWR, 7=ID, 8=VDD */
    if (meter_type < 1 || meter_type > 8)
    {
        return -RIG_EINVAL;
    }

    SNPRINTF(priv->cmd_str, sizeof(priv->cmd_str), "RM%d;", meter_type);

    ret = newcat_get_cmd(rig);
    if (ret != RIG_OK) return ret;

    /* Response: RM P1 P2P3P4; */
    if (sscanf(priv->ret_data + 2, "%1d%3d", &p1_resp, &level) != 2)
    {
        rig_debug(RIG_DEBUG_ERR, "%s: failed to parse '%s'\n", __func__,
                  priv->ret_data);
        return -RIG_EPROTO;
    }

    *val = level;
    return RIG_OK;
}

/*
 * ftx1_set_power - Set Power Control
 *
 * CAT command: PC P1 P2...;
 *   P1 = 1 for field head (0.5-6W on battery, 0.5-10W on 12V)
 *   P1 = 2 for SPA-1 (5-100W)
 *
 * PC1 (field head) format:
 *   - Fractional watts: PC10.5; PC11.1; PC11.5; etc. (decimal, NOT zero-padded)
 *   - Whole watts: PC1001; PC1002; ... PC1010; (3-digit zero-padded)
 *
 * PC2 (SPA-1) format:
 *   - Always whole watts: PC2005; PC2010; ... PC2100; (3-digit zero-padded)
 *   - Never uses decimal format
 *
 * Uses pre-detected head type from ftx1_open() for efficiency and safety.
 * Falls back to PC query if detection hasn't been done yet.
 */
int ftx1_set_power(RIG *rig, float val)
{
    struct newcat_priv_data *priv = STATE(rig)->priv;
    int ret;
    int head_type;
    float watts;
    float max_power;
    int watts_int;

    rig_debug(RIG_DEBUG_VERBOSE, "%s: val=%f\n", __func__, val);

    /*
     * Use pre-detected head type from ftx1_open() for better performance
     * and to ensure consistency with SPA-1 guardrails.
     */
    head_type = ftx1_get_head_type(rig);

    if (head_type == FTX1_HEAD_UNKNOWN)
    {
        /* Fallback: Query current PC to detect head type if not already known */
        rig_debug(RIG_DEBUG_VERBOSE, "%s: head type unknown, querying PC\n", __func__);
        SNPRINTF(priv->cmd_str, sizeof(priv->cmd_str), "PC;");
        ret = newcat_get_cmd(rig);
        if (ret == RIG_OK && strlen(priv->ret_data) >= 3)
        {
            if (strncmp(priv->ret_data, "PC2", 3) == 0)
            {
                head_type = FTX1_HEAD_SPA1;
            }
            else
            {
                head_type = FTX1_HEAD_FIELD_12V;  /* Default to 12V if unknown */
            }
        }
        if (head_type == FTX1_HEAD_UNKNOWN)
        {
            head_type = FTX1_HEAD_FIELD_12V;  /* Default to field head 12V */
        }
        /* Small delay after query */
        hl_usleep(50 * 1000);
    }

    rig_debug(RIG_DEBUG_VERBOSE, "%s: using head_type=%d\n", __func__, head_type);

    if (head_type == FTX1_HEAD_SPA1)
    {
        /* Optima/SPA-1: 5-100W, always 3-digit zero-padded whole watts */
        watts_int = (int)(val * FTX1_POWER_MAX_SPA1);
        if (watts_int < FTX1_POWER_MIN_SPA1) watts_int = FTX1_POWER_MIN_SPA1;
        if (watts_int > FTX1_POWER_MAX_SPA1) watts_int = FTX1_POWER_MAX_SPA1;
        SNPRINTF(priv->cmd_str, sizeof(priv->cmd_str), "PC2%03d;", watts_int);
    }
    else
    {
        /*
         * Field head power ranges:
         *   FIELD_BATTERY: 0.5-6W
         *   FIELD_12V:     0.5-10W
         */
        if (head_type == FTX1_HEAD_FIELD_BATTERY)
        {
            max_power = FTX1_POWER_MAX_FIELD_BATTERY;
        }
        else
        {
            max_power = FTX1_POWER_MAX_FIELD_12V;
        }

        watts = val * max_power;
        if (watts < FTX1_POWER_MIN_FIELD) watts = FTX1_POWER_MIN_FIELD;
        if (watts > max_power) watts = max_power;

        /* Check if watts is essentially a whole number (within 0.05 tolerance) */
        /* This avoids floating-point precision issues with direct equality */
        if ((watts - (int)watts) < 0.05f)
        {
            /* Whole watts: use 3-digit zero-padded format */
            SNPRINTF(priv->cmd_str, sizeof(priv->cmd_str), "PC1%03d;", (int)(watts + 0.5f));
        }
        else
        {
            /* Fractional watts: use decimal format */
            SNPRINTF(priv->cmd_str, sizeof(priv->cmd_str), "PC1%.1f;", watts);
        }
    }

    return newcat_set_cmd(rig);
}

/*
 * ftx1_get_power - Get Power Control
 *
 * Response formats:
 *   PC1 (field head):
 *     - Battery (0.5-6W): PC10.5; PC1001; ... PC1006;
 *     - 12V (0.5-10W): PC10.5; PC1001; ... PC1010;
 *     - Fractional: decimal format (PC10.5, PC11.5, etc.)
 *     - Whole watts: 3-digit zero-padded (PC1001, PC1006, PC1010)
 *   PC2 (SPA-1, 5-100W):
 *     - Always whole watts: PC2005; PC2010; ... PC2100; (3-digit zero-padded)
 *
 * Returns power as 0.0-1.0 normalized value based on detected head type.
 */
int ftx1_get_power(RIG *rig, float *val)
{
    struct newcat_priv_data *priv = STATE(rig)->priv;
    int ret;
    int p1;
    int head_type;
    float watts;
    float max_power;

    rig_debug(RIG_DEBUG_VERBOSE, "%s\n", __func__);

    SNPRINTF(priv->cmd_str, sizeof(priv->cmd_str), "PC;");

    ret = newcat_get_cmd(rig);
    if (ret != RIG_OK) return ret;

    /* Response: PC P1 P2... where P2 can be 3-digit or decimal */
    /* Try to parse P1 first (always 1 digit) */
    if (sscanf(priv->ret_data + 2, "%1d", &p1) != 1)
    {
        rig_debug(RIG_DEBUG_ERR, "%s: failed to parse P1 from '%s'\n", __func__,
                  priv->ret_data);
        return -RIG_EPROTO;
    }

    /* Now parse the power value - could be decimal (0.5, 1.5, 5.1) or integer (001, 005, 010) */
    if (sscanf(priv->ret_data + 3, "%f", &watts) != 1)
    {
        rig_debug(RIG_DEBUG_ERR, "%s: failed to parse watts from '%s'\n", __func__,
                  priv->ret_data);
        return -RIG_EPROTO;
    }

    rig_debug(RIG_DEBUG_VERBOSE, "%s: p1=%d watts=%.1f\n", __func__, p1, watts);

    /* Get detected head type for proper scaling */
    head_type = ftx1_get_head_type(rig);

    /* Convert watts to 0.0-1.0 range based on head type */
    if (p1 == 2)
    {
        /* Optima/SPA-1: 5-100W maps to 0.05-1.0 */
        *val = watts / (float)FTX1_POWER_MAX_SPA1;
    }
    else if (p1 == 1)
    {
        /*
         * Field head: scale based on detected power source
         *   FIELD_BATTERY: 0.5-6W
         *   FIELD_12V:     0.5-10W
         */
        if (head_type == FTX1_HEAD_FIELD_BATTERY)
        {
            max_power = FTX1_POWER_MAX_FIELD_BATTERY;
        }
        else
        {
            max_power = FTX1_POWER_MAX_FIELD_12V;
        }
        *val = watts / max_power;
    }
    else
    {
        rig_debug(RIG_DEBUG_ERR, "%s: unexpected P1=%d\n", __func__, p1);
        *val = watts / FTX1_POWER_MAX_FIELD_12V;  /* Default to field head 12V */
    }

    return RIG_OK;
}

/* Set VOX Gain (VG P1P2P3;) */
int ftx1_set_vox_gain(RIG *rig, float val)
{
    struct newcat_priv_data *priv = STATE(rig)->priv;
    int level = (int)(val * 100);

    rig_debug(RIG_DEBUG_VERBOSE, "%s: val=%f\n", __func__, val);

    if (level < 0) level = 0;
    if (level > 100) level = 100;

    SNPRINTF(priv->cmd_str, sizeof(priv->cmd_str), "VG%03d;", level);
    return newcat_set_cmd(rig);
}

/* Get VOX Gain */
int ftx1_get_vox_gain(RIG *rig, float *val)
{
    struct newcat_priv_data *priv = STATE(rig)->priv;
    int ret;
    int level;

    rig_debug(RIG_DEBUG_VERBOSE, "%s\n", __func__);

    SNPRINTF(priv->cmd_str, sizeof(priv->cmd_str), "VG;");

    ret = newcat_get_cmd(rig);
    if (ret != RIG_OK) return ret;

    if (sscanf(priv->ret_data + 2, "%3d", &level) != 1)
    {
        rig_debug(RIG_DEBUG_ERR, "%s: failed to parse '%s'\n", __func__,
                  priv->ret_data);
        return -RIG_EPROTO;
    }

    *val = (float)level / 100.0f;
    return RIG_OK;
}

/*
 * VD code to milliseconds lookup table (FTX-1 CAT manual / Java Audio.java:204-209)
 *
 * Codes 00-05 are non-linear: 30, 50, 100, 150, 200, 250 ms
 * Codes 06-33 are linear: (code - 6) * 100 + 300 ms
 */
static const int ftx1_vd_code_to_ms[] = {
    30, 50, 100, 150, 200, 250,                          /* 00-05 */
    300, 400, 500, 600, 700, 800, 900, 1000,              /* 06-13 */
    1100, 1200, 1300, 1400, 1500, 1600, 1700, 1800,       /* 14-21 */
    1900, 2000, 2100, 2200, 2300, 2400, 2500, 2600,       /* 22-29 */
    2700, 2800, 2900, 3000                                 /* 30-33 */
};
#define FTX1_VD_CODE_COUNT (sizeof(ftx1_vd_code_to_ms) / sizeof(ftx1_vd_code_to_ms[0]))

/*
 * ftx1_ms_to_vd_code - Find the closest VD code for a given millisecond value
 */
static int ftx1_ms_to_vd_code(int ms)
{
    int i;
    int best = 0;
    int best_diff = abs(ms - ftx1_vd_code_to_ms[0]);

    for (i = 1; i < (int)FTX1_VD_CODE_COUNT; i++)
    {
        int diff = abs(ms - ftx1_vd_code_to_ms[i]);

        if (diff < best_diff)
        {
            best = i;
            best_diff = diff;
        }
    }

    return best;
}

/*
 * ftx1_set_vox_delay - Set VOX Delay
 * CAT command: VD P1P2; (2 digits, 00-33)
 *
 * Hamlib VOXDELAY is in tenths of seconds. Convert to ms, find closest VD code.
 */
int ftx1_set_vox_delay(RIG *rig, int tenths)
{
    struct newcat_priv_data *priv = STATE(rig)->priv;
    int ms = tenths * 100;
    int code;

    rig_debug(RIG_DEBUG_VERBOSE, "%s: tenths=%d ms=%d\n", __func__, tenths, ms);

    code = ftx1_ms_to_vd_code(ms);

    SNPRINTF(priv->cmd_str, sizeof(priv->cmd_str), "VD%02d;", code);
    return newcat_set_cmd(rig);
}

/* Get VOX Delay — returns tenths of seconds */
int ftx1_get_vox_delay(RIG *rig, int *tenths)
{
    struct newcat_priv_data *priv = STATE(rig)->priv;
    int ret;
    int code;

    rig_debug(RIG_DEBUG_VERBOSE, "%s\n", __func__);

    SNPRINTF(priv->cmd_str, sizeof(priv->cmd_str), "VD;");

    ret = newcat_get_cmd(rig);
    if (ret != RIG_OK) return ret;

    if (sscanf(priv->ret_data + 2, "%2d", &code) != 1)
    {
        rig_debug(RIG_DEBUG_ERR, "%s: failed to parse '%s'\n", __func__,
                  priv->ret_data);
        return -RIG_EPROTO;
    }

    if (code < 0 || code >= (int)FTX1_VD_CODE_COUNT)
    {
        rig_debug(RIG_DEBUG_ERR, "%s: VD code %d out of range\n", __func__, code);
        return -RIG_EPROTO;
    }

    /* Convert ms to tenths of seconds (integer division rounds down) */
    *tenths = ftx1_vd_code_to_ms[code] / 100;
    return RIG_OK;
}

/*
 * ftx1_set_monitor_level - Set Monitor Level (ML P1 P2P3P4;)
 * CAT command: ML P1 P2P3P4; (P1=VFO 0/1, P2-P4=000-100)
 */
int ftx1_set_monitor_level(RIG *rig, vfo_t vfo, float val)
{
    struct newcat_priv_data *priv = STATE(rig)->priv;
    int p1 = ftx1_vfo_to_p1(rig, vfo);
    int level = (int)(val * 100);

    rig_debug(RIG_DEBUG_VERBOSE, "%s: vfo=%s p1=%d val=%f\n", __func__,
              rig_strvfo(vfo), p1, val);

    if (level < 0) level = 0;
    if (level > 100) level = 100;

    SNPRINTF(priv->cmd_str, sizeof(priv->cmd_str), "ML%d%03d;", p1, level);
    return newcat_set_cmd(rig);
}

/*
 * ftx1_get_monitor_level - Get Monitor Level
 * Response: ML P1 P2P3P4; (P1=VFO, P2-P4=level)
 */
int ftx1_get_monitor_level(RIG *rig, vfo_t vfo, float *val)
{
    struct newcat_priv_data *priv = STATE(rig)->priv;
    int ret;
    int p1 = ftx1_vfo_to_p1(rig, vfo);
    int p1_resp, level;

    rig_debug(RIG_DEBUG_VERBOSE, "%s: vfo=%s p1=%d\n", __func__,
              rig_strvfo(vfo), p1);

    SNPRINTF(priv->cmd_str, sizeof(priv->cmd_str), "ML%d;", p1);

    ret = newcat_get_cmd(rig);
    if (ret != RIG_OK) return ret;

    /* Response: ML0100 (P1=0, level=100) */
    if (sscanf(priv->ret_data + 2, "%1d%3d", &p1_resp, &level) != 2)
    {
        rig_debug(RIG_DEBUG_ERR, "%s: failed to parse '%s'\n", __func__,
                  priv->ret_data);
        return -RIG_EPROTO;
    }

    *val = (float)level / 100.0f;
    return RIG_OK;
}

/*
 * ftx1_set_agc - Set AGC Time Constant
 * CAT command: GT P1 P2; (P1=VFO 0/1, P2=mode 0-4)
 *
 * FTX-1 AGC values (P2):
 *   0 = OFF
 *   1 = FAST
 *   2 = MID
 *   3 = SLOW
 *   4 = AUTO
 */
int ftx1_set_agc(RIG *rig, vfo_t vfo, int val)
{
    struct newcat_priv_data *priv = STATE(rig)->priv;
    int p1 = ftx1_vfo_to_p1(rig, vfo);

    rig_debug(RIG_DEBUG_VERBOSE, "%s: vfo=%s p1=%d val=%d\n", __func__,
              rig_strvfo(vfo), p1, val);

    /* val is AGC setting code (0-4 for FTX-1) */
    SNPRINTF(priv->cmd_str, sizeof(priv->cmd_str), "GT%d%d;", p1, val);
    return newcat_set_cmd(rig);
}

/* Get AGC */
int ftx1_get_agc(RIG *rig, vfo_t vfo, int *val)
{
    struct newcat_priv_data *priv = STATE(rig)->priv;
    int ret;
    int p1 = ftx1_vfo_to_p1(rig, vfo);
    int p1_resp, agc;

    rig_debug(RIG_DEBUG_VERBOSE, "%s: vfo=%s p1=%d\n", __func__,
              rig_strvfo(vfo), p1);

    SNPRINTF(priv->cmd_str, sizeof(priv->cmd_str), "GT%d;", p1);

    ret = newcat_get_cmd(rig);
    if (ret != RIG_OK) return ret;

    /* Response: GT0X where X is AGC mode (0-6, 4-6 are AUTO variants) */
    if (sscanf(priv->ret_data + 2, "%1d%1d", &p1_resp, &agc) != 2)
    {
        rig_debug(RIG_DEBUG_ERR, "%s: failed to parse '%s'\n", __func__,
                  priv->ret_data);
        return -RIG_EPROTO;
    }

    *val = agc;
    return RIG_OK;
}

/*
 * ftx1_set_amc_output - Set AMC Output Level (AO command)
 * CAT command: AO P1P2P3; (000-100)
 */
int ftx1_set_amc_output(RIG *rig, float val)
{
    struct newcat_priv_data *priv = STATE(rig)->priv;
    int level = (int)(val * 100);

    rig_debug(RIG_DEBUG_VERBOSE, "%s: val=%f\n", __func__, val);

    if (level < 0) level = 0;
    if (level > 100) level = 100;

    SNPRINTF(priv->cmd_str, sizeof(priv->cmd_str), "AO%03d;", level);
    return newcat_set_cmd(rig);
}

/* Get AMC Output Level */
int ftx1_get_amc_output(RIG *rig, float *val)
{
    struct newcat_priv_data *priv = STATE(rig)->priv;
    int ret;
    int level;

    rig_debug(RIG_DEBUG_VERBOSE, "%s\n", __func__);

    SNPRINTF(priv->cmd_str, sizeof(priv->cmd_str), "AO;");

    ret = newcat_get_cmd(rig);
    if (ret != RIG_OK) return ret;

    /* Response: AO P1P2P3; (3 digits 000-100) */
    if (sscanf(priv->ret_data + 2, "%3d", &level) != 1)
    {
        rig_debug(RIG_DEBUG_ERR, "%s: failed to parse '%s'\n", __func__,
                  priv->ret_data);
        return -RIG_EPROTO;
    }

    *val = (float)level / 100.0f;
    return RIG_OK;
}

/*
 * ftx1_set_width - Set Filter Width (SH command)
 * CAT command: SH P1 P2 P3P4; (P1=VFO, P2=0, P3P4=00-23)
 *
 * Width codes 00-23, Hz mapping is mode-dependent (see CAT manual Table 5):
 *   LSB/USB: 00=Default, 01=300..23=4000 Hz
 *   CW/DATA/RTTY/PSK: 00=Default, 01=50..21=4000 Hz
 *   AM-N: 02=6000 Hz (fixed), FM/DATA-FM: 03=16000 Hz (fixed)
 */
int ftx1_set_width(RIG *rig, vfo_t vfo, int width_code)
{
    struct newcat_priv_data *priv = STATE(rig)->priv;
    int p1 = ftx1_vfo_to_p1(rig, vfo);

    rig_debug(RIG_DEBUG_VERBOSE, "%s: vfo=%s width_code=%d\n", __func__,
              rig_strvfo(vfo), width_code);

    if (width_code < 0) width_code = 0;
    if (width_code > 23) width_code = 23;

    SNPRINTF(priv->cmd_str, sizeof(priv->cmd_str), "SH%d0%02d;", p1, width_code);
    return newcat_set_cmd(rig);
}

/* Get Filter Width */
int ftx1_get_width(RIG *rig, vfo_t vfo, int *width_code)
{
    struct newcat_priv_data *priv = STATE(rig)->priv;
    int ret;
    int p1 = ftx1_vfo_to_p1(rig, vfo);
    int p1_resp, p2, width;

    rig_debug(RIG_DEBUG_VERBOSE, "%s: vfo=%s\n", __func__, rig_strvfo(vfo));

    SNPRINTF(priv->cmd_str, sizeof(priv->cmd_str), "SH%d;", p1);

    ret = newcat_get_cmd(rig);
    if (ret != RIG_OK) return ret;

    /* Response: SH P1 P2 P3P4; (P1=VFO, P2=0, P3P4=width code) */
    if (sscanf(priv->ret_data + 2, "%1d%1d%2d", &p1_resp, &p2, &width) != 3)
    {
        rig_debug(RIG_DEBUG_ERR, "%s: failed to parse '%s'\n", __func__,
                  priv->ret_data);
        return -RIG_EPROTO;
    }

    *width_code = width;
    return RIG_OK;
}

/*
 * FTX-1 MS (Meter Switch) code mapping (CAT manual p.20):
 *   0 = PO    1 = COMP   2 = ALC   3 = VDD   4 = ID   5 = SWR
 *
 * Note: differs from other Yaesu radios (FT-891/991 use 0=COMP,1=ALC,2=PO,
 * 3=SWR,4=IC,5=VDD). The FTX-1 has its own unique ordering.
 */
static int ftx1_meter_to_cat(int hamlib_meter)
{
    switch (hamlib_meter)
    {
    case RIG_METER_PO:   return 0;
    case RIG_METER_COMP: return 1;
    case RIG_METER_ALC:  return 2;
    case RIG_METER_VDD:  return 3;
    case RIG_METER_IC:   return 4;
    case RIG_METER_SWR:  return 5;
    default:             return 0;  /* default to PO */
    }
}

static int ftx1_cat_to_meter(int cat_code)
{
    switch (cat_code)
    {
    case 0: return RIG_METER_PO;
    case 1: return RIG_METER_COMP;
    case 2: return RIG_METER_ALC;
    case 3: return RIG_METER_VDD;
    case 4: return RIG_METER_IC;
    case 5: return RIG_METER_SWR;
    default: return RIG_METER_NONE;
    }
}

/*
 * ftx1_set_meter_switch - Set Meter Switch (MS command)
 * CAT command: MS P1 P2; (P1=MAIN meter, P2=SUB meter)
 *
 * Takes a RIG_METER_* value, converts to FTX-1 CAT code, and sets the
 * meter for the specified VFO while preserving the other VFO's setting.
 */
int ftx1_set_meter_switch(RIG *rig, vfo_t vfo, int meter_type)
{
    struct newcat_priv_data *priv = STATE(rig)->priv;
    int ret;
    int main_meter, sub_meter;
    int cat_code;

    rig_debug(RIG_DEBUG_VERBOSE, "%s: vfo=%s meter_type=%d\n", __func__,
              rig_strvfo(vfo), meter_type);

    cat_code = ftx1_meter_to_cat(meter_type);

    /* Read current settings to preserve the other VFO's meter */
    SNPRINTF(priv->cmd_str, sizeof(priv->cmd_str), "MS;");
    ret = newcat_get_cmd(rig);
    if (ret != RIG_OK)
    {
        /* If read fails, set both to the same value */
        main_meter = cat_code;
        sub_meter = cat_code;
    }
    else
    {
        if (sscanf(priv->ret_data + 2, "%1d%1d", &main_meter, &sub_meter) != 2)
        {
            main_meter = cat_code;
            sub_meter = cat_code;
        }
    }

    /* Update the appropriate meter based on VFO (with currVFO resolution) */
    if (ftx1_vfo_to_p1(rig, vfo) == 1)
    {
        sub_meter = cat_code;
    }
    else
    {
        main_meter = cat_code;
    }

    SNPRINTF(priv->cmd_str, sizeof(priv->cmd_str), "MS%d%d;", main_meter,
             sub_meter);
    return newcat_set_cmd(rig);
}

/*
 * ftx1_get_meter_switch - Get Meter Switch
 * Response: MS P1 P2; (P1=MAIN meter, P2=SUB meter)
 *
 * Returns the RIG_METER_* value for the specified VFO.
 */
int ftx1_get_meter_switch(RIG *rig, vfo_t vfo, int *meter_type)
{
    struct newcat_priv_data *priv = STATE(rig)->priv;
    int ret;
    int p1, p2;
    int cat_code;

    rig_debug(RIG_DEBUG_VERBOSE, "%s: vfo=%s\n", __func__, rig_strvfo(vfo));

    SNPRINTF(priv->cmd_str, sizeof(priv->cmd_str), "MS;");

    ret = newcat_get_cmd(rig);
    if (ret != RIG_OK) return ret;

    if (sscanf(priv->ret_data + 2, "%1d%1d", &p1, &p2) != 2)
    {
        rig_debug(RIG_DEBUG_ERR, "%s: failed to parse '%s'\n", __func__,
                  priv->ret_data);
        return -RIG_EPROTO;
    }

    /* Select the CAT code for the requested VFO, then translate */
    cat_code = (ftx1_vfo_to_p1(rig, vfo) == 1) ? p2 : p1;
    *meter_type = ftx1_cat_to_meter(cat_code);

    return RIG_OK;
}

/*
 * =============================================================================
 * PB Command: Play Back (DVS Voice Message)
 * =============================================================================
 * CAT format: PB P1 P2;
 *   P1 = 0 (always 0 for FTX-1)
 *   P2 = Voice message channel:
 *        0 = Stop playback/recording
 *        1 = Voice message channel 1
 *        2 = Voice message channel 2
 *        3 = Voice message channel 3
 *        4 = Voice message channel 4
 *        5 = Voice message channel 5
 *
 * Read response: PB0n where n is current channel (0 if not playing)
 * Set command: PB0n;
 *
 * WARNING: Playing TX voice messages causes transmission!
 */

/* Voice message channel values */
#define FTX1_PB_STOP      0   /* Stop playback */
#define FTX1_PB_CHANNEL_1 1   /* Channel 1 */
#define FTX1_PB_CHANNEL_2 2   /* Channel 2 */
#define FTX1_PB_CHANNEL_3 3   /* Channel 3 */
#define FTX1_PB_CHANNEL_4 4   /* Channel 4 */
#define FTX1_PB_CHANNEL_5 5   /* Channel 5 */

/*
 * ftx1_play_voice_msg - Play or stop voice message
 *
 * channel: 0=stop, 1-5=play channel
 * WARNING: This may cause transmission when playing TX messages!
 */
int ftx1_play_voice_msg(RIG *rig, int channel)
{
    struct newcat_priv_data *priv = STATE(rig)->priv;

    rig_debug(RIG_DEBUG_VERBOSE, "%s: channel=%d\n", __func__, channel);

    if (channel < 0 || channel > 5)
    {
        rig_debug(RIG_DEBUG_ERR, "%s: invalid channel %d (must be 0-5)\n",
                  __func__, channel);
        return -RIG_EINVAL;
    }

    SNPRINTF(priv->cmd_str, sizeof(priv->cmd_str), "PB0%d;", channel);

    return newcat_set_cmd(rig);
}

/*
 * ftx1_get_voice_msg_status - Get current voice message playback status
 *
 * Returns: channel number (0 if not playing, 1-5 if playing)
 */
int ftx1_get_voice_msg_status(RIG *rig, int *channel)
{
    struct newcat_priv_data *priv = STATE(rig)->priv;
    int ret;
    int p1, p2;

    rig_debug(RIG_DEBUG_VERBOSE, "%s\n", __func__);

    SNPRINTF(priv->cmd_str, sizeof(priv->cmd_str), "PB0;");

    ret = newcat_get_cmd(rig);
    if (ret != RIG_OK)
    {
        return ret;
    }

    /* Response: PB0n where n is channel (0-5) */
    if (sscanf(priv->ret_data + 2, "%1d%1d", &p1, &p2) != 2)
    {
        rig_debug(RIG_DEBUG_ERR, "%s: failed to parse '%s'\n",
                  __func__, priv->ret_data);
        return -RIG_EPROTO;
    }

    *channel = p2;

    rig_debug(RIG_DEBUG_VERBOSE, "%s: channel=%d\n", __func__, *channel);

    return RIG_OK;
}

/*
 * ftx1_stop_voice_msg - Stop voice message playback
 */
int ftx1_stop_voice_msg(RIG *rig)
{
    return ftx1_play_voice_msg(rig, FTX1_PB_STOP);
}
