/*
 *  Harris PRC-138 support developed by Antonio Regazzoni - HB9RBS
 *  Tested with three different PRC138 versions with following motherboard
 *  Firmware (module 01A) : 8211A, 8214D, 8214F.
 *  04 april 2026
 *            
 *
 *  Copyright (c) 2004-2010 by Stephane Fillod
 *
 *
 *   This library is free software; you can redistribute it and/or
 *   modify it under the terms of the GNU Lesser General Public
 *   License as published by the Free Software Foundation; either
 *   version 2.1 of the License, or (at your option) any later version.
 *
 *   This library is distributed in the hope that it will be useful,
 *   but WITHOUT ANY WARRANTY; without even the implied warranty of
 *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 *   Lesser General Public License for more details.
 *
 *   You should have received a copy of the GNU Lesser General Public
 *   License along with this library; if not, write to the Free Software
 *   Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA
 *
 */

#include <ctype.h>
#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "hamlib/rig.h"
#include "register.h"
#include "iofunc.h"
#include "harris.h"
#include "prc138.h"

static const float bw_ssb[] = {1.5, 2.0, 2.4, 2.7, 3.0, 0};
static const float bw_cw[]  = {0.35, 0.68, 1.0, 1.5, 0};
static const float bw_am[]  = {3.0, 4.0, 5.0, 6.0, 0};

static int harris_transaction(RIG *rig, const char *cmd, char *resp, int resp_len)
{
    unsigned char buf[128];
    char local_buf[512];
    char *target_buf = resp ? resp : local_buf;
    int target_len = resp ? resp_len : sizeof(local_buf);
    int retry = 15;
    
    target_buf[0] = '\0';

    /* 1. Physical buffer flush */
    rig_flush(RIGPORT(rig));

    /* 2. Send command */
    if (write_block(RIGPORT(rig), (unsigned char *)cmd, strlen(cmd)) < 0) {
        return -RIG_EIO;
    }

    /* 3. Accumulation loop */
    while (retry > 0) {
        usleep(10000); //150000
        
        memset(buf, 0, sizeof(buf));
        (void) read_block(RIGPORT(rig), buf, sizeof(buf) - 1);
        
        if (buf[0] != '\0') {
            strncat(target_buf, (char *)buf, target_len - strlen(target_buf) - 1);
            
            /* If we find the prompt with space, we are done */
            if (strstr(target_buf, "SSB> ")) {
                return RIG_OK;
            }
        }
        retry--;
    }
    return -RIG_EIO;
}

int harris_set_freq(RIG *rig, vfo_t vfo, freq_t freq)
{
    char cmd[64];
    
    rig_debug(RIG_DEBUG_VERBOSE, "%s: setting frequency to %.0f Hz\n", __func__, freq);

    /* The %08.0f format is CRITICAL: 
       - 0 means 'pad with zeros'
       - 8 means 'total length of 8 characters'
       - .0f means 'real number without decimals' 
       Example: 7000000 becomes 07000000 */
    snprintf(cmd, sizeof(cmd), "FR %08.0f\r", freq);

    /* Use our trusted harris_transaction */
    if (harris_transaction(rig, cmd, NULL, 0) != RIG_OK) {
        rig_debug(RIG_DEBUG_ERR, "%s: error in FR transaction\n", __func__);
        return -RIG_EIO;
    }

    return RIG_OK;
}

int harris_get_freq(RIG *rig, vfo_t vfo, freq_t *freq)
{
    char buf[512];
    char *p;

    rig_debug(RIG_DEBUG_VERBOSE, "%s: frequency request\n", __func__);

    /* 1. Perform complete transaction */
    if (harris_transaction(rig, "FR\r", buf, sizeof(buf)) != RIG_OK) {
        rig_debug(RIG_DEBUG_ERR, "%s: error in FR transaction\n", __func__);
        return -RIG_EIO;
    }

    /* 2. Look for the Rx frequency line "RxFr " */
    p = strstr(buf, "RxFr ");
    if (p) {
        /* Skip "RxFr " (5 characters) and read the number */
        /* sscanf is more robust than atof for strings ending with special characters */
        if (sscanf(p + 5, "%lf", freq) != 1) {
            rig_debug(RIG_DEBUG_ERR, "%s: frequency format not recognized in [%s]\n", __func__, p);
            return -RIG_EPROTO;
        }
        
        rig_debug(RIG_DEBUG_VERBOSE, "%s: frequency read: %.0f Hz\n", __func__, *freq);
        return RIG_OK;
    }

    rig_debug(RIG_DEBUG_ERR, "%s: RxFr string not found in response\n", __func__);
    return -RIG_EPROTO;
}

int harris_open(RIG *rig)
{
    rig_debug(RIG_DEBUG_VERBOSE, "%s: Opening and prompt synchronization\n", __func__);

    /* Send a simple carriage return to solicit the prompt */
    /* Pass NULL as response buffer because we only care about the result (RIG_OK) */
    if (harris_transaction(rig, "\r", NULL, 0) != RIG_OK) {
        rig_debug(RIG_DEBUG_ERR, "%s: Radio not responding to initial prompt\n", __func__);
        return -RIG_EIO;
    }

    rig_debug(RIG_DEBUG_VERBOSE, "%s: Connection successfully established\n", __func__);
    return RIG_OK;
}

static float find_closest_bandwidth(float target, const float *list) {
    float closest = list[0];
    float min_diff = 99.0; // High initial value

    for (int i = 0; list[i] != 0; i++) {
        float diff = target - list[i];
        if (diff < 0) diff = -diff; // Absolute value
        
        if (diff < min_diff) {
            min_diff = diff;
            closest = list[i];
        }
    }
    return closest;
}

int harris_set_pb_width(RIG *rig, vfo_t vfo, pbwidth_t width)
{
    char cmd[64];
    rmode_t current_mode;
    pbwidth_t current_width;
    const float *bw_list;

    rig_debug(RIG_DEBUG_VERBOSE, "%s: bandwidth change request to %ld Hz\n", __func__, width);

    /* 1. Retrieve current mode to determine which table to use */
    if (harris_get_mode(rig, vfo, &current_mode, &current_width) != RIG_OK) {
        return -RIG_EIO;
    }

    /* 2. Select the correct table */
    switch (current_mode) {
        case RIG_MODE_USB:
        case RIG_MODE_LSB: bw_list = bw_ssb; break;
        case RIG_MODE_CW:  bw_list = bw_cw;  break;
        case RIG_MODE_AM:  bw_list = bw_am;  break;
        default: bw_list = bw_ssb; // Fallback
    }

    /* 3. Approximation and sending */
    float target_khz = (float)width / 1000.0;
    float final_khz = find_closest_bandwidth(target_khz, bw_list);

    snprintf(cmd, sizeof(cmd), "BAND %.2f\r", final_khz);
    return harris_transaction(rig, cmd, NULL, 0);
}

int harris_set_mode(RIG *rig, vfo_t vfo, rmode_t mode, pbwidth_t width)
{
    char cmd[64];
    const char *mstr;

    switch (mode) {
        case RIG_MODE_USB: mstr = "USB"; break;
        case RIG_MODE_LSB: mstr = "LSB"; break;
        case RIG_MODE_CW:  mstr = "CW";  break;
        case RIG_MODE_AM:  mstr = "AME"; break;
        default: return -RIG_EINVAL;
    }

    /* 1. Change Mode */
    snprintf(cmd, sizeof(cmd), "MODE %s\r", mstr);
    if (harris_transaction(rig, cmd, NULL, 0) != RIG_OK) return -RIG_EIO;

    /* 2. Change bandwidth using the new function (if width > 0) */
    if (width > 0 && width != RIG_PASSBAND_NORMAL) {
        return harris_set_pb_width(rig, vfo, width);
    }

    return RIG_OK;
}

int harris_get_mode(RIG *rig, vfo_t vfo, rmode_t *mode, pbwidth_t *width)
{
    char buf[512];
    char *p;

    if (harris_transaction(rig, "MODE\r", buf, sizeof(buf)) != RIG_OK) return -RIG_EIO;

    /* Mode Parsing */
    p = strstr(buf, "MODE ");
    if (p) {
        p += 5;
        if (strncmp(p, "USB", 3) == 0)      *mode = RIG_MODE_USB;
        else if (strncmp(p, "LSB", 3) == 0) *mode = RIG_MODE_LSB;
        else if (strncmp(p, "CW", 2) == 0)  *mode = RIG_MODE_CW;
        else if (strncmp(p, "AME", 3) == 0 || strncmp(p, "AM", 2) == 0) *mode = RIG_MODE_AM;
    }

    /* Band Parsing (Filter) */
    p = strstr(buf, "BAND ");
    if (p) {
        *width = (pbwidth_t)(atof(p + 5) * 1000.0);
    }

    return RIG_OK;
}

int harris_get_level(RIG *rig, vfo_t vfo, setting_t level, value_t *val)
{
    char buf[2048];

    /* Special case for Preamp: SH command doesn't show it in dump */
    if (level == RIG_LEVEL_PREAMP) {
        if (harris_transaction(rig, "PRE\r", buf, sizeof(buf)) != RIG_OK) {
            return -RIG_EIO;
        }
        val->i = strstr(buf, "Preamp Enabled") ? 1 : 0;
        return RIG_OK;
    }

    /* For all other levels we use the SH dump */
    if (harris_transaction(rig, "SH\r", buf, sizeof(buf)) != RIG_OK) {
        return -RIG_EIO;
    }

    switch (level) {
        case RIG_LEVEL_SQL:
            if (strstr(buf, "SQ_LEVEL LOW")) val->f = 0.1f;
            else if (strstr(buf, "SQ_LEVEL MED")) val->f = 0.5f;
            else if (strstr(buf, "SQ_LEVEL HIGH")) val->f = 1.0f;
            else val->f = 0.0f;
            break;

        case RIG_LEVEL_RFPOWER:
            if (strstr(buf, "POWER low")) val->f = 0.1f;
            else if (strstr(buf, "POWER med")) val->f = 0.5f;
            else if (strstr(buf, "POWER hi")) val->f = 1.0f;
            else val->f = 0.0f;
            break;

        case RIG_LEVEL_AGC:
            if (strstr(buf, "AGC OFF")) val->i = RIG_AGC_OFF;
            else if (strstr(buf, "AGC SLOW")) val->i = RIG_AGC_SLOW;
            else if (strstr(buf, "AGC MED")) val->i = RIG_AGC_MEDIUM;
            else if (strstr(buf, "AGC FAST")) val->i = RIG_AGC_FAST;
            break;

        case RIG_LEVEL_RF:
            {
                char *p = strstr(buf, "RFG ");
                if (p) val->f = (float)atoi(p + 4) / 100.0f;
                else val->f = 1.0f;
            }
            break;

        default:
            return -RIG_EINVAL;
    }

    return RIG_OK;
}
 
int harris_set_level(RIG *rig, vfo_t vfo, setting_t level, value_t val)
{
    char cmd[64];
    const char *p_str;

    switch (level) {
        case RIG_LEVEL_SQL:
            if (val.f <= 0.33f) p_str = "LOW";
            else if (val.f <= 0.66f) p_str = "MED";
            else p_str = "HIGH";
            snprintf(cmd, sizeof(cmd), "SQ_L %s\r", p_str);
            break;

        case RIG_LEVEL_RFPOWER:
            if (val.f <= 0.33f) p_str = "LOW";
            else if (val.f <= 0.66f) p_str = "MED";
            else p_str = "HIGH";
            snprintf(cmd, sizeof(cmd), "POW %s\r", p_str);
            break;

        case RIG_LEVEL_AGC:
            if (val.i == RIG_AGC_OFF) p_str = "OF";
            else if (val.i == RIG_AGC_SLOW) p_str = "SLOW";
            else if (val.i == RIG_AGC_MEDIUM) p_str = "MED";
            else if (val.i == RIG_AGC_FAST) p_str = "FAST";
            else p_str = "SLOW"; 
            snprintf(cmd, sizeof(cmd), "AGC %s\r", p_str); // Use extended AGC
            break;

        case RIG_LEVEL_RF:
            snprintf(cmd, sizeof(cmd), "RFG %d\r", (int)(val.f * 100));
            break;

        case RIG_LEVEL_PREAMP:
            // Crucial: Only "ENA" and "BYP" actually work
            snprintf(cmd, sizeof(cmd), "PRE %s\r", (val.i > 0) ? "ENA" : "BYP");
            break;

        default:
            return -RIG_EINVAL;
    }

    return harris_transaction(rig, cmd, NULL, 0);
} 

 
int harris_set_func(RIG *rig, vfo_t vfo, setting_t func, int status)
{
    char cmd[64];

    /* Only Squelch is supported in this function */
    if (func != RIG_FUNC_SQL) {
        rig_debug(RIG_DEBUG_TRACE, "%s: function %lu not supported\n", __func__, func);
        return -RIG_EINVAL;
    }

    /* Harris Command: SQ ON to activate, SQ OF to deactivate */
    /* Note: Using 'OF' with single F as per previous test logic */
    snprintf(cmd, sizeof(cmd), "SQ %s\r", status ? "ON" : "OF");

    rig_debug(RIG_DEBUG_VERBOSE, "%s: setting Squelch status %d [%s]\n", 
              __func__, status, cmd);

    /* Use centralized transaction to wait for SSB> prompt */
    return harris_transaction(rig, cmd, NULL, 0);
}

int harris_get_ptt(RIG *rig, vfo_t vfo, ptt_t *ptt)
{
    char buf[512];

    rig_debug(RIG_DEBUG_VERBOSE, "%s: PTT status query\n", __func__);

    /* Use centralized transaction by sending only 'K' */
    if (harris_transaction(rig, "K\r", buf, sizeof(buf)) != RIG_OK) {
        return -RIG_EIO;
    }

    /* Response analysis in buffer */
    /* Radio typically responds with "KEY ON", "KEY MIC" or "KEY OFF" */
    if (strstr(buf, "KEY ON") || strstr(buf, "KEY MIC")) {
        *ptt = RIG_PTT_ON;
    } else if (strstr(buf, "KEY OFF")) {
        *ptt = RIG_PTT_OFF;
    } else {
        /* Safety fallback based on partial strings */
        if (strstr(buf, "ON")) {
            *ptt = RIG_PTT_ON;
        } else {
            *ptt = RIG_PTT_OFF;
        }
    }

    rig_debug(RIG_DEBUG_TRACE, "%s: PTT detected: %s\n", 
              __func__, (*ptt == RIG_PTT_ON) ? "ON" : "OFF");

    return RIG_OK;
}

int harris_set_ptt(RIG *rig, vfo_t vfo, ptt_t ptt)
{
    char cmd[16];

    /* Build command: K ON to activate, K OF to deactivate */
    /* Note: maintaining 'OF' with single F as per original implementation */
    snprintf(cmd, sizeof(cmd), "K %s\r", (ptt == RIG_PTT_ON) ? "ON" : "OF");

    rig_debug(RIG_DEBUG_VERBOSE, "%s: setting PTT [%s]\n", __func__, cmd);

    /* Send via transaction (ignore text response, just look for prompt) */
    return harris_transaction(rig, cmd, NULL, 0);
}

int harris_init(RIG *rig) {
    struct harris_priv_data *priv;

    priv = (struct harris_priv_data *)calloc(1, sizeof(struct harris_priv_data));
    if (!priv) return -RIG_ENOMEM;

    rig->caps->priv = (void *)priv;
    return RIG_OK;
}

int harris_cleanup(RIG *rig) {
    if (rig->caps->priv) {
        free((void *)rig->caps->priv);
        rig->caps->priv = NULL;
    }
    return RIG_OK;
}

int harris_close(RIG *rig) {
    return RIG_OK;
}

/* This macro expands the initrigs5_harris function. */
DECLARE_INITRIG_BACKEND(harris)
{
    rig_debug(RIG_DEBUG_VERBOSE, "%s: _init called\n", __func__);
    rig_register((struct rig_caps *)&prc138_caps);
    return RIG_OK;
}
