/*
 * Hamlib Yaesu backend - FTX-1 RIT/XIT (Clarifier) Operations
 * Copyright (c) 2025 by Terrell Deppe (KJ5HST)
 *
 * ===========================================================================
 * WARNING: RIT/XIT NOT SUPPORTED IN LATEST FIRMWARE
 * ===========================================================================
 * The RC/TC commands that worked in earlier FTX-1 firmware versions are no
 * longer functional in the latest firmware. The standard RT/XT toggle commands
 * also return '?'. As of firmware v1.08+, clarifier (RIT/XIT) must be
 * controlled directly from the radio front panel.
 *
 * This code is retained for reference but will return errors on current
 * firmware versions.
 * ===========================================================================
 *
 * CAT Commands in this file (NO LONGER WORKING):
 *   RC P1;           - Receiver Clarifier (RIT) offset - NOT SUPPORTED
 *   TC P1;           - Transmit Clarifier (XIT) offset - NOT SUPPORTED
 *   IF;              - Information query (clarifier state may be readable)
 *
 * Note: Both RT/XT and RC/TC commands return '?' on current FTX-1 firmware.
 */

#include <stdlib.h>
#include <string.h>
#include <hamlib/rig.h>
#include "misc.h"
#include "yaesu.h"
#include "newcat.h"
#include "ftx1.h"

/*
 * FTX-1 IF (Information) response structure
 *
 * The IF command returns a fixed-format response that contains the current
 * clarifier offset and enable states. This structure maps to that response
 * for easy parsing.
 */
typedef struct {
    char command[2];      /* "IF" */
    char memory_ch[3];    /* 001 -> 117 */
    char vfo_freq[9];     /* 9 digit value in Hz */
    char clarifier[5];    /* '+' or '-', followed by 0000 -> 9999 Hz */
    char rx_clarifier;    /* '0' = off, '1' = on (RIT) */
    char tx_clarifier;    /* '0' = off, '1' = on (XIT) */
    char mode;            /* Mode code */
    char vfo_memory;      /* VFO/Memory mode */
    char tone_mode;       /* CTCSS/DCS mode */
    char fixed[2];        /* Always '0', '0' */
    char repeater_offset; /* Repeater shift */
    char terminator;      /* ';' */
} ftx1_if_response_t;

/*
 * ftx1_get_rit - Get RIT (Receiver Clarifier) offset
 *
 * NOTE: This function may not work with current FTX-1 firmware.
 * Uses IF command to read clarifier state, returns offset if RX clarifier enabled.
 */
/*
 * FTX1_IF_RESPONSE_LEN - Expected minimum length of IF response
 * The IF response should be at least 27 characters: "IF" + 25 data chars + ";"
 */
#define FTX1_IF_RESPONSE_LEN 27

int ftx1_get_rit(RIG *rig, vfo_t vfo, shortfreq_t *rit)
{
    struct newcat_priv_data *priv = (struct newcat_priv_data *)STATE(rig)->priv;
    ftx1_if_response_t *rdata;
    int err;
    size_t resp_len;

    (void)vfo;  /* FTX-1 clarifier is not VFO-specific */

    rig_debug(RIG_DEBUG_VERBOSE, "%s called\n", __func__);

    if (!rig || !rit)
    {
        return -RIG_EINVAL;
    }

    SNPRINTF(priv->cmd_str, sizeof(priv->cmd_str), "IF;");

    if (RIG_OK != (err = newcat_get_cmd(rig)))
    {
        return err;
    }

    /* Validate response length before casting to structure */
    resp_len = strlen(priv->ret_data);
    if (resp_len < FTX1_IF_RESPONSE_LEN)
    {
        rig_debug(RIG_DEBUG_ERR, "%s: IF response too short (%zu < %d): '%s'\n",
                  __func__, resp_len, FTX1_IF_RESPONSE_LEN, priv->ret_data);
        return -RIG_EPROTO;
    }

    rdata = (ftx1_if_response_t *)priv->ret_data;

    /* Check if RX clarifier (RIT) is enabled */
    if (rdata->rx_clarifier == '1')
    {
        /* atoi() handles the sign prefix correctly - no need to negate again */
        *rit = atoi(rdata->clarifier);
    }
    else
    {
        *rit = 0;
    }

    rig_debug(RIG_DEBUG_VERBOSE, "%s: rit=%ld (rx_clar=%c)\n",
              __func__, (long)*rit, rdata->rx_clarifier);

    return RIG_OK;
}

/*
 * ftx1_set_rit - Set RIT (Receiver Clarifier) offset
 *
 * NOTE: This function may not work with current FTX-1 firmware.
 * Uses RC command: RC0; to clear, RC+NNNN; or RC-NNNN; to set offset.
 *
 * Note: Setting offset to 0 also disables the clarifier.
 */
int ftx1_set_rit(RIG *rig, vfo_t vfo, shortfreq_t rit)
{
    struct newcat_priv_data *priv = (struct newcat_priv_data *)STATE(rig)->priv;
    int err;

    (void)vfo;  /* FTX-1 clarifier is not VFO-specific */

    rig_debug(RIG_DEBUG_VERBOSE, "%s called with rit=%ld\n", __func__, (long)rit);

    /* Validate range: FTX-1 supports ±9999 Hz */
    if (rit < -9999 || rit > 9999)
    {
        rig_debug(RIG_DEBUG_ERR, "%s: RIT offset %ld out of range (±9999 Hz)\n",
                  __func__, (long)rit);
        return -RIG_EINVAL;
    }

    if (rit == 0)
    {
        /* Clear RIT - RC0; */
        SNPRINTF(priv->cmd_str, sizeof(priv->cmd_str), "RC0;");
    }
    else if (rit > 0)
    {
        /* Positive offset - RC+NNNN; */
        SNPRINTF(priv->cmd_str, sizeof(priv->cmd_str), "RC+%04ld;", (long)rit);
    }
    else
    {
        /* Negative offset - RC-NNNN; */
        SNPRINTF(priv->cmd_str, sizeof(priv->cmd_str), "RC-%04ld;", (long)(-rit));
    }

    rig_debug(RIG_DEBUG_TRACE, "%s: cmd=%s\n", __func__, priv->cmd_str);

    if (RIG_OK != (err = newcat_set_cmd(rig)))
    {
        return err;
    }

    return RIG_OK;
}

/*
 * ftx1_get_xit - Get XIT (Transmit Clarifier) offset
 *
 * NOTE: This function may not work with current FTX-1 firmware.
 * Uses IF command to read clarifier state, returns offset if TX clarifier enabled.
 */
int ftx1_get_xit(RIG *rig, vfo_t vfo, shortfreq_t *xit)
{
    struct newcat_priv_data *priv = (struct newcat_priv_data *)STATE(rig)->priv;
    ftx1_if_response_t *rdata;
    int err;
    size_t resp_len;

    (void)vfo;  /* FTX-1 clarifier is not VFO-specific */

    rig_debug(RIG_DEBUG_VERBOSE, "%s called\n", __func__);

    if (!rig || !xit)
    {
        return -RIG_EINVAL;
    }

    SNPRINTF(priv->cmd_str, sizeof(priv->cmd_str), "IF;");

    if (RIG_OK != (err = newcat_get_cmd(rig)))
    {
        return err;
    }

    /* Validate response length before casting to structure */
    resp_len = strlen(priv->ret_data);
    if (resp_len < FTX1_IF_RESPONSE_LEN)
    {
        rig_debug(RIG_DEBUG_ERR, "%s: IF response too short (%zu < %d): '%s'\n",
                  __func__, resp_len, FTX1_IF_RESPONSE_LEN, priv->ret_data);
        return -RIG_EPROTO;
    }

    rdata = (ftx1_if_response_t *)priv->ret_data;

    /* Check if TX clarifier (XIT) is enabled */
    if (rdata->tx_clarifier == '1')
    {
        /* atoi() handles the sign prefix correctly - no need to negate again */
        *xit = atoi(rdata->clarifier);
    }
    else
    {
        *xit = 0;
    }

    rig_debug(RIG_DEBUG_VERBOSE, "%s: xit=%ld (tx_clar=%c)\n",
              __func__, (long)*xit, rdata->tx_clarifier);

    return RIG_OK;
}

/*
 * ftx1_set_xit - Set XIT (Transmit Clarifier) offset
 *
 * NOTE: This function may not work with current FTX-1 firmware.
 * Uses TC command: TC0; to clear, TC+NNNN; or TC-NNNN; to set offset.
 *
 * Note: Setting offset to 0 also disables the clarifier.
 */
int ftx1_set_xit(RIG *rig, vfo_t vfo, shortfreq_t xit)
{
    struct newcat_priv_data *priv = (struct newcat_priv_data *)STATE(rig)->priv;
    int err;

    (void)vfo;  /* FTX-1 clarifier is not VFO-specific */

    rig_debug(RIG_DEBUG_VERBOSE, "%s called with xit=%ld\n", __func__, (long)xit);

    /* Validate range: FTX-1 supports ±9999 Hz */
    if (xit < -9999 || xit > 9999)
    {
        rig_debug(RIG_DEBUG_ERR, "%s: XIT offset %ld out of range (±9999 Hz)\n",
                  __func__, (long)xit);
        return -RIG_EINVAL;
    }

    if (xit == 0)
    {
        /* Clear XIT - TC0; */
        SNPRINTF(priv->cmd_str, sizeof(priv->cmd_str), "TC0;");
    }
    else if (xit > 0)
    {
        /* Positive offset - TC+NNNN; */
        SNPRINTF(priv->cmd_str, sizeof(priv->cmd_str), "TC+%04ld;", (long)xit);
    }
    else
    {
        /* Negative offset - TC-NNNN; */
        SNPRINTF(priv->cmd_str, sizeof(priv->cmd_str), "TC-%04ld;", (long)(-xit));
    }

    rig_debug(RIG_DEBUG_TRACE, "%s: cmd=%s\n", __func__, priv->cmd_str);

    if (RIG_OK != (err = newcat_set_cmd(rig)))
    {
        return err;
    }

    return RIG_OK;
}
