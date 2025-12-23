/*
 * Hamlib Yaesu backend - FTX-1 RIT/XIT (Clarifier) Operations
 * Copyright (c) 2025 by Terrell Deppe (KJ5HST)
 *
 * ===========================================================================
 * SPECIAL ACKNOWLEDGMENT - JEREMY MILLER (KO4SSD)
 * ===========================================================================
 * The RIT/XIT implementation in this file is based on the brilliant work of
 * Jeremy Miller (KO4SSD) in Hamlib PR #1826. Jeremy discovered that the FTX-1
 * does NOT support the standard RT/XT (RIT/XIT toggle) commands - they return
 * '?' on this radio. Instead, he figured out that the RC (Receiver Clarifier)
 * and TC (Transmit Clarifier) commands work correctly for setting RIT/XIT
 * offsets, and that the IF (Information) command can be parsed to read the
 * current clarifier state.
 *
 * This was a critical discovery after much trial and error. The FTX-1 CAT
 * manual was not initially available, and even after release, the differences
 * from other Yaesu radios were not obvious. Jeremy's persistence in testing
 * with actual hardware and his willingness to share his findings with the
 * community made this implementation possible.
 *
 * Thank you, Jeremy, for your excellent work on FTX-1 support in Hamlib!
 * Your contribution is greatly appreciated by the amateur radio community.
 *
 * Jeremy's original implementation: https://github.com/Hamlib/Hamlib/pull/1826
 * ===========================================================================
 *
 * CAT Commands in this file (discovered/verified by Jeremy Miller):
 *   RC P1;           - Receiver Clarifier (RIT) offset
 *                      RC0; = clear, RC+NNNN; = positive, RC-NNNN; = negative
 *   TC P1;           - Transmit Clarifier (XIT) offset
 *                      TC0; = clear, TC+NNNN; = positive, TC-NNNN; = negative
 *   IF;              - Information query (used to read clarifier state)
 *                      Response includes clarifier offset and RX/TX enable flags
 *
 * Note: The RT (RIT toggle) and XT (XIT toggle) commands return '?' on FTX-1.
 * This is documented in our firmware notes but Jeremy was the first to find
 * the working alternative using RC/TC commands.
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
 * Adapted from Jeremy Miller's ftx1info struct in PR #1826
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
 * Implementation by Jeremy Miller (KO4SSD) - PR #1826
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
 * Implementation by Jeremy Miller (KO4SSD) - PR #1826
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
 * Implementation by Jeremy Miller (KO4SSD) - PR #1826
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
 * Implementation by Jeremy Miller (KO4SSD) - PR #1826
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
