/*
 * Hamlib Yaesu backend - FTX-1 Clarifier Operations
 * Copyright (c) 2025 by Terrell Deppe (KJ5HST)
 *
 * CAT Commands in this file:
 *   CF P1 P2 P3 [P4 P5 P6 P7 P8];  - Clarifier control
 *
 * CF Command Parameters:
 *   P1: 0=MAIN-side, 1=SUB-side
 *   P2: 0 (Fixed)
 *   P3: 0=CLAR Setting, 1=CLAR Frequency
 *
 *   When P3=0 (CLAR Setting):
 *     P4: 0=RX CLAR OFF, 1=RX CLAR ON
 *     P5: 0=TX CLAR OFF, 1=TX CLAR ON
 *     P6-P8: 000 (Fixed)
 *
 *   When P3=1 (CLAR Frequency):
 *     P4: + or -
 *     P5-P8: 0000-9999 Hz
 */

#include <stdlib.h>
#include <string.h>
#include <hamlib/rig.h>
#include "misc.h"
#include "yaesu.h"
#include "newcat.h"
#include "ftx1.h"

/*
 * CF command response lengths
 * CF000 response: CF00RXXTX000; (12 chars) - P3=0 (setting)
 * CF001 response: CF01+NNNN; or CF01-NNNN; (10 chars) - P3=1 (frequency)
 */
#define FTX1_CF_SETTING_RESP_LEN  12  /* CF00RXXTX000; */
#define FTX1_CF_FREQ_RESP_LEN     10  /* CF01+NNNN; */

/*
 * Helper function to get VFO parameter for CF command
 * Returns '0' for MAIN VFO, '1' for SUB VFO
 */
static char ftx1_get_cf_vfo_param(RIG *rig, vfo_t vfo)
{
    vfo_t curr_vfo = vfo;

    if (curr_vfo == RIG_VFO_CURR || curr_vfo == RIG_VFO_NONE)
    {
        curr_vfo = STATE(rig)->current_vfo;
    }

    return (curr_vfo == RIG_VFO_SUB || curr_vfo == RIG_VFO_B) ? '1' : '0';
}

/*
 * ftx1_get_rx_clar - Get RX Clarifier offset
 *
 * Uses CF command with P3=0 to get clarifier enable state, then P3=1 to get frequency.
 * Returns the clarifier frequency if RX CLAR is enabled, otherwise 0.
 */
int ftx1_get_rx_clar(RIG *rig, vfo_t vfo, shortfreq_t *offset)
{
    struct newcat_priv_data *priv = (struct newcat_priv_data *)STATE(rig)->priv;
    int err;
    size_t resp_len;
    char vfo_param;
    char rx_clar_enabled;

    rig_debug(RIG_DEBUG_VERBOSE, "%s called\n", __func__);

    if (!rig || !offset)
    {
        return -RIG_EINVAL;
    }

    vfo_param = ftx1_get_cf_vfo_param(rig, vfo);

    /* Query clarifier enable state: CF P1 0 0; */
    SNPRINTF(priv->cmd_str, sizeof(priv->cmd_str), "CF%c00;", vfo_param);

    if (RIG_OK != (err = newcat_get_cmd(rig)))
    {
        return err;
    }

    /* Response format: CF00RXXTX000; where RX is P4, TX is P5 */
    resp_len = strlen(priv->ret_data);

    if (resp_len < FTX1_CF_SETTING_RESP_LEN)
    {
        rig_debug(RIG_DEBUG_ERR, "%s: CF setting response too short (%zu): '%s'\n",
                  __func__, resp_len, priv->ret_data);
        return -RIG_EPROTO;
    }

    /* P4 (RX CLAR enable) is at position 4 (0-indexed) */
    rx_clar_enabled = priv->ret_data[4];

    rig_debug(RIG_DEBUG_TRACE, "%s: CF setting response='%s', rx_clar_enabled=%c\n",
              __func__, priv->ret_data, rx_clar_enabled);

    if (rx_clar_enabled != '1')
    {
        *offset = 0;
        return RIG_OK;
    }

    /* RX CLAR is enabled, now get the frequency: CF P1 0 1; */
    SNPRINTF(priv->cmd_str, sizeof(priv->cmd_str), "CF%c01;", vfo_param);

    if (RIG_OK != (err = newcat_get_cmd(rig)))
    {
        return err;
    }

    /* Response format: CF01+NNNN; or CF01-NNNN; */
    resp_len = strlen(priv->ret_data);

    if (resp_len < FTX1_CF_FREQ_RESP_LEN)
    {
        rig_debug(RIG_DEBUG_ERR, "%s: CF freq response too short (%zu): '%s'\n",
                  __func__, resp_len, priv->ret_data);
        return -RIG_EPROTO;
    }

    /* Frequency starts at position 4: +NNNN or -NNNN */
    *offset = atoi(&priv->ret_data[4]);

    rig_debug(RIG_DEBUG_VERBOSE, "%s: offset=%ld\n", __func__, (long)*offset);

    return RIG_OK;
}

/*
 * ftx1_set_rx_clar - Set RX Clarifier offset
 *
 * Uses CF command:
 *   - CF P1 0 0 P4 P5 000; to set clarifier enable state
 *   - CF P1 0 1 +/-NNNN; to set frequency
 *
 * Setting offset to 0 disables RX CLAR. Non-zero enables and sets frequency.
 */
int ftx1_set_rx_clar(RIG *rig, vfo_t vfo, shortfreq_t offset)
{
    struct newcat_priv_data *priv = (struct newcat_priv_data *)STATE(rig)->priv;
    int err;
    char vfo_param;
    size_t resp_len;
    char tx_clar_enabled;

    rig_debug(RIG_DEBUG_VERBOSE, "%s called with offset=%ld\n", __func__, (long)offset);

    /* Validate range: FTX-1 supports ±9999 Hz */
    if (offset < -9999 || offset > 9999)
    {
        rig_debug(RIG_DEBUG_ERR, "%s: offset %ld out of range (±9999 Hz)\n",
                  __func__, (long)offset);
        return -RIG_EINVAL;
    }

    vfo_param = ftx1_get_cf_vfo_param(rig, vfo);

    /* First, get current TX CLAR state so we preserve it */
    SNPRINTF(priv->cmd_str, sizeof(priv->cmd_str), "CF%c00;", vfo_param);

    if (RIG_OK != (err = newcat_get_cmd(rig)))
    {
        return err;
    }

    resp_len = strlen(priv->ret_data);

    if (resp_len < FTX1_CF_SETTING_RESP_LEN)
    {
        rig_debug(RIG_DEBUG_ERR, "%s: CF response too short (%zu): '%s'\n",
                  __func__, resp_len, priv->ret_data);
        return -RIG_EPROTO;
    }

    /* P5 (TX CLAR enable) is at position 5 */
    tx_clar_enabled = priv->ret_data[5];

    if (offset == 0)
    {
        /* Disable RX CLAR, preserve TX CLAR state */
        SNPRINTF(priv->cmd_str, sizeof(priv->cmd_str), "CF%c000%c000;",
                 vfo_param, tx_clar_enabled);
    }
    else
    {
        /* Enable RX CLAR, preserve TX CLAR state, then set frequency */
        SNPRINTF(priv->cmd_str, sizeof(priv->cmd_str), "CF%c001%c000;",
                 vfo_param, tx_clar_enabled);

        rig_debug(RIG_DEBUG_TRACE, "%s: enable cmd=%s\n", __func__, priv->cmd_str);

        if (RIG_OK != (err = newcat_set_cmd(rig)))
        {
            return err;
        }

        /* Now set the frequency */
        if (offset > 0)
        {
            SNPRINTF(priv->cmd_str, sizeof(priv->cmd_str), "CF%c01+%04ld;",
                     vfo_param, (long)offset);
        }
        else
        {
            SNPRINTF(priv->cmd_str, sizeof(priv->cmd_str), "CF%c01-%04ld;",
                     vfo_param, (long)(-offset));
        }
    }

    rig_debug(RIG_DEBUG_TRACE, "%s: cmd=%s\n", __func__, priv->cmd_str);

    if (RIG_OK != (err = newcat_set_cmd(rig)))
    {
        return err;
    }

    return RIG_OK;
}

/*
 * ftx1_get_tx_clar - Get TX Clarifier offset
 *
 * Uses CF command with P3=0 to get clarifier enable state, then P3=1 to get frequency.
 * Returns the clarifier frequency if TX CLAR is enabled, otherwise 0.
 */
int ftx1_get_tx_clar(RIG *rig, vfo_t vfo, shortfreq_t *offset)
{
    struct newcat_priv_data *priv = (struct newcat_priv_data *)STATE(rig)->priv;
    int err;
    size_t resp_len;
    char vfo_param;
    char tx_clar_enabled;

    rig_debug(RIG_DEBUG_VERBOSE, "%s called\n", __func__);

    if (!rig || !offset)
    {
        return -RIG_EINVAL;
    }

    vfo_param = ftx1_get_cf_vfo_param(rig, vfo);

    /* Query clarifier enable state: CF P1 0 0; */
    SNPRINTF(priv->cmd_str, sizeof(priv->cmd_str), "CF%c00;", vfo_param);

    if (RIG_OK != (err = newcat_get_cmd(rig)))
    {
        return err;
    }

    /* Response format: CF00RXXTX000; where RX is P4, TX is P5 */
    resp_len = strlen(priv->ret_data);

    if (resp_len < FTX1_CF_SETTING_RESP_LEN)
    {
        rig_debug(RIG_DEBUG_ERR, "%s: CF setting response too short (%zu): '%s'\n",
                  __func__, resp_len, priv->ret_data);
        return -RIG_EPROTO;
    }

    /* P5 (TX CLAR enable) is at position 5 (0-indexed) */
    tx_clar_enabled = priv->ret_data[5];

    rig_debug(RIG_DEBUG_TRACE, "%s: CF setting response='%s', tx_clar_enabled=%c\n",
              __func__, priv->ret_data, tx_clar_enabled);

    if (tx_clar_enabled != '1')
    {
        *offset = 0;
        return RIG_OK;
    }

    /* TX CLAR is enabled, now get the frequency: CF P1 0 1; */
    SNPRINTF(priv->cmd_str, sizeof(priv->cmd_str), "CF%c01;", vfo_param);

    if (RIG_OK != (err = newcat_get_cmd(rig)))
    {
        return err;
    }

    /* Response format: CF01+NNNN; or CF01-NNNN; */
    resp_len = strlen(priv->ret_data);

    if (resp_len < FTX1_CF_FREQ_RESP_LEN)
    {
        rig_debug(RIG_DEBUG_ERR, "%s: CF freq response too short (%zu): '%s'\n",
                  __func__, resp_len, priv->ret_data);
        return -RIG_EPROTO;
    }

    /* Frequency starts at position 4: +NNNN or -NNNN */
    *offset = atoi(&priv->ret_data[4]);

    rig_debug(RIG_DEBUG_VERBOSE, "%s: offset=%ld\n", __func__, (long)*offset);

    return RIG_OK;
}

/*
 * ftx1_set_tx_clar - Set TX Clarifier offset
 *
 * Uses CF command:
 *   - CF P1 0 0 P4 P5 000; to set clarifier enable state
 *   - CF P1 0 1 +/-NNNN; to set frequency
 *
 * Setting offset to 0 disables TX CLAR. Non-zero enables and sets frequency.
 */
int ftx1_set_tx_clar(RIG *rig, vfo_t vfo, shortfreq_t offset)
{
    struct newcat_priv_data *priv = (struct newcat_priv_data *)STATE(rig)->priv;
    int err;
    char vfo_param;
    size_t resp_len;
    char rx_clar_enabled;

    rig_debug(RIG_DEBUG_VERBOSE, "%s called with offset=%ld\n", __func__, (long)offset);

    /* Validate range: FTX-1 supports ±9999 Hz */
    if (offset < -9999 || offset > 9999)
    {
        rig_debug(RIG_DEBUG_ERR, "%s: offset %ld out of range (±9999 Hz)\n",
                  __func__, (long)offset);
        return -RIG_EINVAL;
    }

    vfo_param = ftx1_get_cf_vfo_param(rig, vfo);

    /* First, get current RX CLAR state so we preserve it */
    SNPRINTF(priv->cmd_str, sizeof(priv->cmd_str), "CF%c00;", vfo_param);

    if (RIG_OK != (err = newcat_get_cmd(rig)))
    {
        return err;
    }

    resp_len = strlen(priv->ret_data);

    if (resp_len < FTX1_CF_SETTING_RESP_LEN)
    {
        rig_debug(RIG_DEBUG_ERR, "%s: CF response too short (%zu): '%s'\n",
                  __func__, resp_len, priv->ret_data);
        return -RIG_EPROTO;
    }

    /* P4 (RX CLAR enable) is at position 4 */
    rx_clar_enabled = priv->ret_data[4];

    if (offset == 0)
    {
        /* Disable TX CLAR, preserve RX CLAR state */
        SNPRINTF(priv->cmd_str, sizeof(priv->cmd_str), "CF%c00%c0000;",
                 vfo_param, rx_clar_enabled);
    }
    else
    {
        /* Enable TX CLAR, preserve RX CLAR state, then set frequency */
        SNPRINTF(priv->cmd_str, sizeof(priv->cmd_str), "CF%c00%c1000;",
                 vfo_param, rx_clar_enabled);

        rig_debug(RIG_DEBUG_TRACE, "%s: enable cmd=%s\n", __func__, priv->cmd_str);

        if (RIG_OK != (err = newcat_set_cmd(rig)))
        {
            return err;
        }

        /* Now set the frequency */
        if (offset > 0)
        {
            SNPRINTF(priv->cmd_str, sizeof(priv->cmd_str), "CF%c01+%04ld;",
                     vfo_param, (long)offset);
        }
        else
        {
            SNPRINTF(priv->cmd_str, sizeof(priv->cmd_str), "CF%c01-%04ld;",
                     vfo_param, (long)(-offset));
        }
    }

    rig_debug(RIG_DEBUG_TRACE, "%s: cmd=%s\n", __func__, priv->cmd_str);

    if (RIG_OK != (err = newcat_set_cmd(rig)))
    {
        return err;
    }

    return RIG_OK;
}
