/*
 * Hamlib Simple CAT backend
 * Copyright (c) 2026 by Hamlib Team
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public License
 * along with this library. If not, see <http://www.gnu.org/licenses/>.
 */

#include "hamlib/config.h"

#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "hamlib/rig.h"
#include "hamlib/rig_state.h"
#include "misc.h"
#include "register.h"
#include "serial.h"

#include "simplecat.h"

/*
 * Simple CAT Protocol implementation - Based on SIMPLE_CAT_PROTOCOL.md
 * specification
 */

#define SIMPLECAT_CMD_MAX 512
#define SIMPLECAT_RESP_MAX 256
#if defined(__MINGW32__) || defined(_WIN32)
#define SIMPLECAT_TIMEOUT_MS 5000
#define SIMPLECAT_CMD_TIMEOUT_MS 5000
#define SIMPLECAT_CDC_ACM_SETTLE_MS 1000
#else
#define SIMPLECAT_TIMEOUT_MS 2000
#define SIMPLECAT_CMD_TIMEOUT_MS 2000
#define SIMPLECAT_CDC_ACM_SETTLE_MS 250
#endif
#define SIMPLECAT_WRITE_RETRY 3
#define SIMPLECAT_FLUSH_DELAY_MS 50

/* Private data structure */
struct simplecat_priv_data
{
    freq_t freq;
    freq_t tx_freq;
    rmode_t mode;
    pbwidth_t width;
    split_t split;
    ptt_t ptt;
    shortfreq_t offset;   /* TX offset for digital modes */
    char digmode[8];      /* FT8 / FT4 / WSPR / other */
    char itone_data[512]; /* Loaded ITONE symbol data */
    int itone_count;      /* Number of symbols loaded */
    char itone_hash[33];  /* MD5 hash of loaded symbols */
};

/*
 * Send a command and read response
 * Returns RIG_OK on success, negative error code on failure
 */
static int simplecat_transaction(RIG *rig, const char *cmd, char *response,
                                 int resp_len)
{
    static unsigned char cmd_buf[SIMPLECAT_CMD_MAX];
    unsigned char resp_buf[SIMPLECAT_RESP_MAX];
    int cmd_len;
    int resp_len_actual;
    int retval;
    int i;

    rig_debug(RIG_DEBUG_VERBOSE, "%s: cmd='%s'\n", __func__, cmd);

    /* Validate response buffer and rig */
    if (!response || resp_len <= 0 || resp_len > SIMPLECAT_RESP_MAX)
    {
        rig_debug(RIG_DEBUG_ERR, "%s: invalid response buffer: %p, %d\n", __func__,
                  response, resp_len);
        return -RIG_EINVAL;
    }

    if (!rig)
    {
        rig_debug(RIG_DEBUG_ERR, "%s: NULL rig\n", __func__);
        return -RIG_EINVAL;
    }

    rig_debug(RIG_DEBUG_VERBOSE, "%s: using buffers cmd_buf=%p resp_buf=%p\n",
              __func__, cmd_buf, resp_buf);

    /* Format command with CR terminator (SimpleCAT protocol uses CR) */
    cmd_len = snprintf((char *)cmd_buf, SIMPLECAT_CMD_MAX, "%s\r", cmd);

    if (cmd_len <= 0 || cmd_len >= SIMPLECAT_CMD_MAX)
    {
        rig_debug(RIG_DEBUG_ERR, "%s: invalid command length\n", __func__);
        return -RIG_EINVAL;
    }

    rig_debug(RIG_DEBUG_VERBOSE, "%s: sending %d bytes: %s\n", __func__, cmd_len,
              cmd_buf);

    /* Flush receiver buffer before sending command to remove stale data */
    rig_flush(RIGPORT(rig));
    usleep(SIMPLECAT_FLUSH_DELAY_MS * 1000);

#if defined(__MINGW32__) || defined(_WIN32)
    /* Windows CDC ACM devices may need multiple write attempts */
    int write_attempts = 0;
    int write_success = 0;

    while (!write_success && write_attempts < SIMPLECAT_WRITE_RETRY)
    {
        retval = write_block(RIGPORT(rig), cmd_buf, cmd_len);

        if (retval == RIG_OK)
        {
            write_success = 1;
        }
        else
        {
            write_attempts++;
            rig_debug(RIG_DEBUG_WARN, "%s: write attempt %d failed: %d\n", __func__,
                      write_attempts, retval);

            if (write_attempts < SIMPLECAT_WRITE_RETRY)
            {
                usleep(500 * 1000);
                rig_flush(RIGPORT(rig));
            }
        }
    }

    if (!write_success)
    {
        rig_debug(RIG_DEBUG_ERR, "%s: failed to write after %d attempts\n",
                  __func__, SIMPLECAT_WRITE_RETRY);
        return -RIG_EIO;
    }

    /* Brief pause after write to allow firmware to process */
    usleep(50 * 1000);
#else
    /* Send command */
    retval = write_block(RIGPORT(rig), cmd_buf, cmd_len);

    if (retval != RIG_OK)
    {
        rig_debug(RIG_DEBUG_ERR, "%s: write_block failed: %d\n", __func__, retval);
        return retval;
    }

#endif

    /* Clear response buffer before read */
    memset(resp_buf, 0, SIMPLECAT_RESP_MAX);

    rig_debug(RIG_DEBUG_VERBOSE, "%s: about to read, max=%d\n", __func__,
              SIMPLECAT_RESP_MAX - 1);

    /* Read response byte-by-byte until we find LF terminator or timeout
     * This avoids the problematic read_string_generic() with its static minlen
     * variable */
    resp_len_actual = 0;

    for (i = 0; i < SIMPLECAT_RESP_MAX - 1; i++)
    {
        unsigned char c;
        int bytes_read;

        bytes_read = read_block(RIGPORT(rig), &c, 1);

        if (bytes_read < 0)
        {
            rig_debug(RIG_DEBUG_ERR,
                      "%s: read_block failed at byte %d: %d, errno=%s\n", __func__, i,
                      bytes_read, strerror(errno));
            return -RIG_EIO;
        }

        if (bytes_read == 0)
        {
            rig_debug(RIG_DEBUG_ERR, "%s: timeout at byte %d\n", __func__, i);
            return -RIG_ETIMEOUT;
        }

        resp_buf[i] = c;
        resp_len_actual++;

        /* Stop at LF terminator */
        if (c == '\n')
        {
            resp_buf[i] = '\0';
            break;
        }
    }

    rig_debug(RIG_DEBUG_VERBOSE, "%s: read %d bytes\n", __func__,
              resp_len_actual);

    if (resp_len_actual == 0)
    {
        rig_debug(RIG_DEBUG_ERR, "%s: no response received\n", __func__);
        return -RIG_EIO;
    }

    /* Remove trailing newline/carriage return characters */
    char *p_resp = (char *)resp_buf;
    size_t len = strlen(p_resp);

    while (len > 0 && (p_resp[len - 1] == '\n' || p_resp[len - 1] == '\r'))
    {
        p_resp[--len] = '\0';
    }

    rig_debug(RIG_DEBUG_VERBOSE, "%s: response='%s'\n", __func__, p_resp);

    /* Copy to output buffer */
    strncpy(response, p_resp, resp_len - 1);
    response[resp_len - 1] = '\0';

    return RIG_OK;
}

/*
 * Check if response indicates success
 */
static int simplecat_check_ok(const char *response)
{
    if (response && strncmp(response, "OK ", 3) == 0)
    {
        return RIG_OK;
    }

    if (response && strcmp(response, "OK") == 0)
    {
        return RIG_OK;
    }

    return -RIG_ERJCTED;
}

/*
 * Parse frequency from response
 */
static int simplecat_parse_freq(const char *response, freq_t *freq)
{
    char *endptr;
    const char *p = response;

    /* Skip command prefix if present */
    if (strncmp(p, "FREQ ", 5) == 0)
    {
        p += 5;
    }

    *freq = strtoll(p, &endptr, 10);

    if (endptr == p || *freq < 0)
    {
        return -RIG_EPROTO;
    }

    return RIG_OK;
}

/*
 * Initialize rig
 */
static int simplecat_init(RIG *rig)
{
    struct simplecat_priv_data *priv;

    rig_debug(RIG_DEBUG_VERBOSE, "%s: called\n", __func__);

    priv = (struct simplecat_priv_data *)calloc(
               1, sizeof(struct simplecat_priv_data));

    if (!priv)
    {
        return -RIG_ENOMEM;
    }

    /* Set defaults */
    priv->freq = MHz(14.074); /* Default to 14.074 MHz (FT8 frequency) */
    priv->tx_freq = priv->freq;
    priv->mode = RIG_MODE_USB;
    priv->width = kHz(2.4);
    priv->split = RIG_SPLIT_OFF;
    priv->ptt = RIG_PTT_OFF;
    priv->offset = 0;
    strcpy(priv->digmode, "FT8");
    priv->itone_count = 0;

    STATE(rig)->priv = priv;

    return RIG_OK;
}

/*
 * Cleanup rig
 */
static int simplecat_cleanup(RIG *rig)
{
    rig_debug(RIG_DEBUG_VERBOSE, "%s: called\n", __func__);

    if (STATE(rig)->priv)
    {
        free(STATE(rig)->priv);
        STATE(rig)->priv = NULL;
    }

    return RIG_OK;
}

/*
 * Open rig connection
 */
static int simplecat_open(RIG *rig)
{
    struct simplecat_priv_data *priv = STATE(rig)->priv;
    char response[SIMPLECAT_RESP_MAX];
    int retval;

    rig_debug(RIG_DEBUG_VERBOSE, "%s: called\n", __func__);

#if defined(__MINGW32__) || defined(_WIN32)
    /* Windows CDC ACM devices need DTR toggled to become ready for I/O.
     * Without this, WriteFile fails with ERROR_IO_PENDING -> EIO. */
    rig_debug(RIG_DEBUG_VERBOSE, "%s: Windows CDC ACM initialization\n",
              __func__);
    ser_set_dtr(RIGPORT(rig), 0);
    usleep(100 * 1000);
    ser_set_dtr(RIGPORT(rig), 1);
    usleep(SIMPLECAT_CDC_ACM_SETTLE_MS * 1000);
    /* Flush any stale data that arrived during DTR toggle */
    rig_flush(RIGPORT(rig));
#else
    /* Brief settle delay for all platforms */
    usleep(SIMPLECAT_CDC_ACM_SETTLE_MS * 1000);
    rig_flush(RIGPORT(rig));
#endif

    /* Test communication by getting frequency */
    retval = simplecat_transaction(rig, "FREQ", response, sizeof(response));

    if (retval == RIG_OK)
    {
        retval = simplecat_parse_freq(response, &priv->freq);

        if (retval == RIG_OK)
        {
            priv->tx_freq = priv->freq;
        }
    }
    else
    {
        rig_debug(RIG_DEBUG_WARN, "%s: FREQ query failed: %d\n", __func__, retval);
    }

    /* Get current mode */
    retval = simplecat_transaction(rig, "MODE", response, sizeof(response));

    if (retval == RIG_OK)
    {
        if (strstr(response, "USB"))
        {
            priv->mode = RIG_MODE_USB;
        }
        else if (strstr(response, "LSB"))
        {
            priv->mode = RIG_MODE_LSB;
        }
        else if (strstr(response, "AM"))
        {
            priv->mode = RIG_MODE_AM;
        }
        else if (strstr(response, "FM"))
        {
            priv->mode = RIG_MODE_FM;
        }
    }
    else
    {
        rig_debug(RIG_DEBUG_WARN, "%s: MODE query failed: %d\n", __func__, retval);
    }

    return RIG_OK;
}

/*
 * Close rig connection
 */
static int simplecat_close(RIG *rig)
{
    rig_debug(RIG_DEBUG_VERBOSE, "%s: called\n", __func__);
    return RIG_OK;
}

/*
 * Set frequency
 */
static int simplecat_set_freq(RIG *rig, vfo_t vfo, freq_t freq)
{
    struct simplecat_priv_data *priv = STATE(rig)->priv;
    char cmd[64];
    char response[SIMPLECAT_RESP_MAX];
    int retval;

    rig_debug(RIG_DEBUG_VERBOSE, "%s: freq=%" PRIfreq " Hz\n", __func__, freq);

    snprintf(cmd, sizeof(cmd), "FREQ %" PRIfreq, freq);
    retval = simplecat_transaction(rig, cmd, response, sizeof(response));

    if (retval == RIG_OK)
    {
        retval = simplecat_check_ok(response);

        if (retval == RIG_OK)
        {
            priv->freq = freq;

            if (priv->split == RIG_SPLIT_OFF)
            {
                priv->tx_freq = freq;
            }
        }
    }

    return retval;
}

/*
 * Get frequency
 */
static int simplecat_get_freq(RIG *rig, vfo_t vfo, freq_t *freq)
{
    struct simplecat_priv_data *priv = STATE(rig)->priv;
    char response[SIMPLECAT_RESP_MAX];
    int retval;

    rig_debug(RIG_DEBUG_VERBOSE, "%s: called\n", __func__);

    retval = simplecat_transaction(rig, "FREQ", response, sizeof(response));

    if (retval == RIG_OK)
    {
        retval = simplecat_parse_freq(response, freq);

        if (retval == RIG_OK)
        {
            priv->freq = *freq;
        }
    }

    return retval;
}

/*
 * Set mode
 */
static int simplecat_set_mode(RIG *rig, vfo_t vfo, rmode_t mode,
                              pbwidth_t width)
{
    struct simplecat_priv_data *priv = STATE(rig)->priv;
    char cmd[64];
    char response[SIMPLECAT_RESP_MAX];
    const char *mode_str;
    int retval;

    rig_debug(RIG_DEBUG_VERBOSE, "%s: mode=%s width=%" PRIfreq "\n", __func__,
              rig_strrmode(mode), (double)width);

    switch (mode)
    {
    case RIG_MODE_USB:
        mode_str = "USB";
        break;

    case RIG_MODE_LSB:
        mode_str = "LSB";
        break;

    case RIG_MODE_AM:
        mode_str = "AM";
        break;

    case RIG_MODE_FM:
        mode_str = "FM";
        break;

    default:
        rig_debug(RIG_DEBUG_ERR, "%s: unsupported mode: %s\n", __func__,
                  rig_strrmode(mode));
        return -RIG_EINVAL;
    }

    snprintf(cmd, sizeof(cmd), "MODE %s", mode_str);
    retval = simplecat_transaction(rig, cmd, response, sizeof(response));

    if (retval == RIG_OK)
    {
        retval = simplecat_check_ok(response);

        if (retval == RIG_OK)
        {
            priv->mode = mode;

            if (width != RIG_PASSBAND_NOCHANGE)
            {
                priv->width = width;
            }
        }
    }

    return retval;
}

/*
 * Get mode
 */
static int simplecat_get_mode(RIG *rig, vfo_t vfo, rmode_t *mode,
                              pbwidth_t *width)
{
    struct simplecat_priv_data *priv = STATE(rig)->priv;
    char response[SIMPLECAT_RESP_MAX];
    int retval;

    rig_debug(RIG_DEBUG_VERBOSE, "%s: called\n", __func__);

    retval = simplecat_transaction(rig, "MODE", response, sizeof(response));

    if (retval == RIG_OK)
    {
        if (strstr(response, "USB"))
        {
            *mode = RIG_MODE_USB;
        }
        else if (strstr(response, "LSB"))
        {
            *mode = RIG_MODE_LSB;
        }
        else if (strstr(response, "AM"))
        {
            *mode = RIG_MODE_AM;
        }
        else if (strstr(response, "FM"))
        {
            *mode = RIG_MODE_FM;
        }
        else
        {
            *mode = priv->mode;
        }

        *width = priv->width;
    }

    return retval;
}

/*
 * Set PTT
 */
static int simplecat_set_ptt(RIG *rig, vfo_t vfo, ptt_t ptt)
{
    struct simplecat_priv_data *priv = STATE(rig)->priv;
    char response[SIMPLECAT_RESP_MAX];
    const char *cmd;
    int retval;

    rig_debug(RIG_DEBUG_VERBOSE, "%s: ptt=%d\n", __func__, ptt);

    cmd = (ptt == RIG_PTT_ON) ? "TX ON" : "TX OFF";
    retval = simplecat_transaction(rig, cmd, response, sizeof(response));

    if (retval == RIG_OK)
    {
        retval = simplecat_check_ok(response);

        if (retval == RIG_OK)
        {
            priv->ptt = ptt;
        }
    }

    return retval;
}

/*
 * Get PTT
 */
static int simplecat_get_ptt(RIG *rig, vfo_t vfo, ptt_t *ptt)
{
    struct simplecat_priv_data *priv = STATE(rig)->priv;
    char response[SIMPLECAT_RESP_MAX];
    int retval;

    rig_debug(RIG_DEBUG_VERBOSE, "%s: called\n", __func__);

    retval = simplecat_transaction(rig, "PTT", response, sizeof(response));

    if (retval == RIG_OK)
    {
        if (strstr(response, "ON"))
        {
            *ptt = RIG_PTT_ON;
        }
        else if (strstr(response, "OFF"))
        {
            *ptt = RIG_PTT_OFF;
        }
        else
        {
            *ptt = priv->ptt;
        }
    }

    return retval;
}

/*
 * Set split mode
 */
static int simplecat_set_split_vfo(RIG *rig, vfo_t vfo, split_t split,
                                   vfo_t tx_vfo)
{
    struct simplecat_priv_data *priv = STATE(rig)->priv;

    rig_debug(RIG_DEBUG_VERBOSE, "%s: split=%d\n", __func__, split);

    priv->split = split;

    if (split == RIG_SPLIT_OFF)
    {
        priv->tx_freq = priv->freq;
    }

    return RIG_OK;
}

/*
 * Get split mode
 */
static int simplecat_get_split_vfo(RIG *rig, vfo_t vfo, split_t *split,
                                   vfo_t *tx_vfo)
{
    struct simplecat_priv_data *priv = STATE(rig)->priv;

    *split = priv->split;
    *tx_vfo = RIG_VFO_B;

    return RIG_OK;
}

/*
 * Set split frequency (TX frequency)
 */
static int simplecat_set_split_freq(RIG *rig, vfo_t vfo, freq_t freq)
{
    struct simplecat_priv_data *priv = STATE(rig)->priv;

    rig_debug(RIG_DEBUG_VERBOSE, "%s: tx_freq=%" PRIfreq " Hz\n", __func__, freq);

    priv->tx_freq = freq;
    priv->split = RIG_SPLIT_ON;

    return RIG_OK;
}

/*
 * Get split frequency (TX frequency)
 */
static int simplecat_get_split_freq(RIG *rig, vfo_t vfo, freq_t *freq)
{
    struct simplecat_priv_data *priv = STATE(rig)->priv;

    *freq = priv->tx_freq;

    return RIG_OK;
}

/*
 * Get info string
 */
static const char *simplecat_get_info(RIG *rig)
{
    static char info[128];
    struct simplecat_priv_data *priv = STATE(rig)->priv;

    snprintf(info, sizeof(info), "Simple CAT %s", priv->digmode);

    return info;
}

/*
 * Set TX offset for digital modes
 */
static int simplecat_set_offset(RIG *rig, vfo_t vfo, shortfreq_t offset)
{
    struct simplecat_priv_data *priv = STATE(rig)->priv;
    char cmd[64];
    char response[SIMPLECAT_RESP_MAX];
    int retval;

    rig_debug(RIG_DEBUG_VERBOSE, "%s: offset=%ld Hz\n", __func__, offset);

    snprintf(cmd, sizeof(cmd), "OFFSET %ld", offset);
    retval = simplecat_transaction(rig, cmd, response, sizeof(response));

    if (retval == RIG_OK)
    {
        retval = simplecat_check_ok(response);

        if (retval == RIG_OK)
        {
            priv->offset = offset;
        }
    }

    return retval;
}

/*
 * Get TX offset for digital modes
 */
static int simplecat_get_offset(RIG *rig, vfo_t vfo, shortfreq_t *offset)
{
    struct simplecat_priv_data *priv = STATE(rig)->priv;
    char response[SIMPLECAT_RESP_MAX];
    int retval;

    rig_debug(RIG_DEBUG_VERBOSE, "%s: called\n", __func__);

    retval = simplecat_transaction(rig, "OFFSET", response, sizeof(response));

    if (retval == RIG_OK)
    {
        char *p = response;

        if (strncmp(p, "OFFSET ", 7) == 0)
        {
            p += 7;
        }

        *offset = atol(p);
        priv->offset = *offset;
    }

    return retval;
}

/*
 * Set digital mode (FT8/FT4)
 */
static int simplecat_set_digmode(RIG *rig, vfo_t vfo, const char *digmode)
{
    struct simplecat_priv_data *priv = STATE(rig)->priv;
    char cmd[64];
    char response[SIMPLECAT_RESP_MAX];
    int retval;

    rig_debug(RIG_DEBUG_VERBOSE, "%s: digmode=%s\n", __func__, digmode);

    snprintf(cmd, sizeof(cmd), "DIGMODE %s", digmode);
    retval = simplecat_transaction(rig, cmd, response, sizeof(response));

    if (retval == RIG_OK)
    {
        retval = simplecat_check_ok(response);

        if (retval == RIG_OK)
        {
            strncpy(priv->digmode, digmode, sizeof(priv->digmode) - 1);
            priv->digmode[sizeof(priv->digmode) - 1] = '\0';
        }
    }

    return retval;
}

/*
 * Get digital mode (FT8/FT4)
 */
static int simplecat_get_digmode(RIG *rig, vfo_t vfo, char *digmode,
                                 int digmode_len)
{
    struct simplecat_priv_data *priv = STATE(rig)->priv;
    char response[SIMPLECAT_RESP_MAX];
    int retval;

    rig_debug(RIG_DEBUG_VERBOSE, "%s: called\n", __func__);

    retval = simplecat_transaction(rig, "DIGMODE", response, sizeof(response));

    if (retval == RIG_OK)
    {
        char *p = response;

        if (strncmp(p, "DIGMODE ", 8) == 0)
        {
            p += 8;
        }

        strncpy(digmode, p, digmode_len - 1);
        digmode[digmode_len - 1] = '\0';
        strncpy(priv->digmode, p, sizeof(priv->digmode) - 1);
        priv->digmode[sizeof(priv->digmode) - 1] = '\0';
    }

    return retval;
}

/*
 * Load ITONE symbol data
 */
static int simplecat_set_itone(RIG *rig, vfo_t vfo, const char *itone_data)
{
    struct simplecat_priv_data *priv = STATE(rig)->priv;
    char cmd[SIMPLECAT_CMD_MAX];
    char response[SIMPLECAT_RESP_MAX];
    int retval;
    int len;
    int data_len;

    rig_debug(RIG_DEBUG_VERBOSE, "%s: itone_data length=%zu\n", __func__,
              strlen(itone_data));

    data_len = strlen(itone_data);

    if (data_len <= 0 || data_len > 400)
    {
        rig_debug(RIG_DEBUG_ERR, "%s: invalid ITONE data length: %d\n", __func__,
                  data_len);
        return -RIG_EINVAL;
    }

    /* Build ITONE command */
    len = snprintf(cmd, sizeof(cmd), "ITONE %s", itone_data);

    if (len <= 0 || len >= sizeof(cmd))
    {
        rig_debug(RIG_DEBUG_ERR, "%s: ITONE command too long: %d bytes\n", __func__,
                  len);
        return -RIG_EINVAL;
    }

    /* Use longer timeout for ITONE command due to large data */
    retval = simplecat_transaction(rig, cmd, response, sizeof(response));

    if (retval == RIG_OK)
    {
        retval = simplecat_check_ok(response);

        if (retval == RIG_OK)
        {
            /* Parse the count from response "OK ITONE <n>" */
            char *p = strstr(response, "ITONE");

            if (p)
            {
                p += 5;

                while (*p == ' ')
                {
                    p++;
                }

                priv->itone_count = atoi(p);
                rig_debug(RIG_DEBUG_VERBOSE, "%s: loaded %d symbols\n", __func__,
                          priv->itone_count);
            }

            /* Store the data */
            strncpy(priv->itone_data, itone_data, sizeof(priv->itone_data) - 1);
            priv->itone_data[sizeof(priv->itone_data) - 1] = '\0';
        }
        else
        {
            rig_debug(RIG_DEBUG_ERR, "%s: ITONE command rejected: %s\n", __func__,
                      response);
        }
    }

    return retval;
}

/*
 * Get ITONE hash
 */
static int simplecat_get_itonehash(RIG *rig, vfo_t vfo, char *hash,
                                   int hash_len)
{
    struct simplecat_priv_data *priv = STATE(rig)->priv;
    char response[SIMPLECAT_RESP_MAX];
    int retval;

    rig_debug(RIG_DEBUG_VERBOSE, "%s: called\n", __func__);

    retval = simplecat_transaction(rig, "ITONEHASH", response, sizeof(response));

    if (retval == RIG_OK)
    {
        char *p = response;

        if (strncmp(p, "ITONEHASH ", 10) == 0)
        {
            p += 10;
        }

        strncpy(hash, p, hash_len - 1);
        hash[hash_len - 1] = '\0';
        strncpy(priv->itone_hash, p, sizeof(priv->itone_hash) - 1);
        priv->itone_hash[sizeof(priv->itone_hash) - 1] = '\0';
    }

    return retval;
}

/*
 * Abort transmission
 */
static int simplecat_abort(RIG *rig, vfo_t vfo)
{
    char response[SIMPLECAT_RESP_MAX];
    int retval;

    rig_debug(RIG_DEBUG_VERBOSE, "%s: called\n", __func__);

    retval = simplecat_transaction(rig, "ABORT", response, sizeof(response));

    if (retval == RIG_OK)
    {
        retval = simplecat_check_ok(response);
    }

    return retval;
}

/*
 * Get firmware version
 */
static const char *simplecat_get_version(RIG *rig)
{
    static char version[64];
    char response[SIMPLECAT_RESP_MAX];
    int retval;

    rig_debug(RIG_DEBUG_VERBOSE, "%s: called\n", __func__);

    retval = simplecat_transaction(rig, "VERSION", response, sizeof(response));

    if (retval == RIG_OK)
    {
        char *p = response;

        if (strncmp(p, "VERSION ", 8) == 0)
        {
            p += 8;
        }

        strncpy(version, p, sizeof(version) - 1);
        version[sizeof(version) - 1] = '\0';
    }
    else
    {
        strncpy(version, "Unknown", sizeof(version) - 1);
    }

    return version;
}

#ifndef TOKEN_BACKEND
#define TOKEN_BACKEND(t) (t)
#endif

#define TOK_OFFSET TOKEN_BACKEND(1)
#define TOK_DIGMODE TOKEN_BACKEND(2)
#define TOK_ITONE TOKEN_BACKEND(3)
#define TOK_ITONEHASH TOKEN_BACKEND(4)
#define TOK_ABORT TOKEN_BACKEND(5)
#define TOK_VERSION TOKEN_BACKEND(6)

static const struct confparams simplecat_ext_parms[] =
{
    {
        TOK_OFFSET,
        "offset",
        "TX Offset",
        "TX Offset frequency in Hz",
        NULL,
        RIG_CONF_NUMERIC,
        {.n = {-100000, 100000, 1}}
    },
    {
        TOK_DIGMODE,
        "digmode",
        "Digital Mode",
        "Digital Mode (FT8/FT4)",
        "FT8",
        RIG_CONF_STRING,
        {}
    },
    {
        TOK_ITONE,
        "itone",
        "ITONE Data",
        "ITONE Symbol Data",
        NULL,
        RIG_CONF_STRING,
        {}
    },
    {
        TOK_ITONEHASH,
        "itonehash",
        "ITONE Hash",
        "ITONE Data MD5 Hash",
        NULL,
        RIG_CONF_STRING,
        {}
    },
    {
        TOK_ABORT,
        "abort",
        "Abort TX",
        "Abort transmission",
        NULL,
        RIG_CONF_BUTTON,
        {}
    },
    {
        TOK_VERSION,
        "version",
        "Firmware Version",
        "Get firmware version string",
        NULL,
        RIG_CONF_STRING,
        {}
    },
    {
        RIG_CONF_END,
        NULL,
    }
};

static int simplecat_set_ext_parm(RIG *rig, hamlib_token_t token, value_t val)
{
    switch (token)
    {
    case TOK_OFFSET:
        return simplecat_set_offset(rig, RIG_VFO_CURR, (shortfreq_t)val.f);

    default:
        return -RIG_EINVAL;
    }
}

static int simplecat_get_ext_parm(RIG *rig, hamlib_token_t token,
                                  value_t *val)
{
    int retval;
    shortfreq_t offset;

    switch (token)
    {
    case TOK_OFFSET:
        retval = simplecat_get_offset(rig, RIG_VFO_CURR, &offset);

        if (retval == RIG_OK)
        {
            val->f = (float)offset;
        }

        return retval;

    case TOK_VERSION:
        /* Version is handled via get_conf */
        return -RIG_ENIMPL;

    default:
        return -RIG_EINVAL;
    }
}

static int simplecat_set_conf(RIG *rig, hamlib_token_t token, const char *val)
{
    switch (token)
    {
    case TOK_DIGMODE:
        return simplecat_set_digmode(rig, RIG_VFO_CURR, val);

    case TOK_ITONE:
        return simplecat_set_itone(rig, RIG_VFO_CURR, val);

    case TOK_ABORT:
        return simplecat_abort(rig, RIG_VFO_CURR);

    default:
        return -RIG_EINVAL;
    }
}

static int simplecat_get_conf(RIG *rig, hamlib_token_t token, char *val)
{
    const char *version_str;

    switch (token)
    {
    case TOK_DIGMODE:
        return simplecat_get_digmode(rig, RIG_VFO_CURR, val, 64);

    case TOK_ITONEHASH:
        return simplecat_get_itonehash(rig, RIG_VFO_CURR, val, 64);

    case TOK_VERSION:
        version_str = simplecat_get_version(rig);
        strncpy(val, version_str, 64);
        val[63] = '\0';
        return RIG_OK;

    default:
        return -RIG_EINVAL;
    }
}

/*
 * Rig capabilities structure
 */
struct rig_caps simplecat_caps =
{
    RIG_MODEL(RIG_MODEL_SIMPLECAT),
    .model_name = "Simple CAT",
    .mfg_name = "Bunzee Labs",
    .version = "20260322.3",
    .copyright = "LGPL",
    .status = RIG_STATUS_STABLE,
    .rig_type = RIG_TYPE_TRANSCEIVER,
    .ptt_type = RIG_PTT_RIG,
    .dcd_type = RIG_DCD_NONE,
    .port_type = RIG_PORT_SERIAL,
    .serial_rate_min = 115200,
    .serial_rate_max = 115200,
    .serial_data_bits = 8,
    .serial_stop_bits = 1,
    .serial_parity = RIG_PARITY_NONE,
    .serial_handshake = RIG_HANDSHAKE_NONE,
    .write_delay = 0,
    .post_write_delay = 0,
    .timeout = SIMPLECAT_TIMEOUT_MS,
    .retry = 0,

    .has_get_func = RIG_FUNC_NONE,
    .has_set_func = RIG_FUNC_NONE,
    .has_get_level = RIG_LEVEL_NONE,
    .has_set_level = RIG_LEVEL_NONE,
    .has_get_parm = RIG_PARM_NONE,
    .has_set_parm = RIG_PARM_NONE,
    .level_gran = {},
    .parm_gran = {},
    .extparms = simplecat_ext_parms,
    .set_ext_parm = simplecat_set_ext_parm,
    .get_ext_parm = simplecat_get_ext_parm,
    .set_conf = simplecat_set_conf,
    .get_conf = simplecat_get_conf,
    .ctcss_list = NULL,
    .dcs_list = NULL,
    .preamp =
    {
        RIG_DBLST_END,
    },
    .attenuator =
    {
        RIG_DBLST_END,
    },
    .max_rit = Hz(0),
    .max_xit = Hz(0),
    .max_ifshift = Hz(0),
    .targetable_vfo = 0,
    .transceive = RIG_TRN_OFF,
    .bank_qty = 0,
    .chan_desc_sz = 0,
    .chan_list =
    {
        RIG_CHAN_END,
    },

    .rx_range_list1 =
    {
        {
            .startf = kHz(100),
            .endf = MHz(60),
            .modes = RIG_MODE_USB | RIG_MODE_LSB | RIG_MODE_AM | RIG_MODE_FM,
            .low_power = -1,
            .high_power = -1,
            .vfo = RIG_VFO_A,
            .ant = RIG_ANT_1,
            .label = "Simple CAT RX Range 1"
        },
        RIG_FRNG_END,
    },
    .rx_range_list2 =
    {
        {
            .startf = kHz(100),
            .endf = MHz(60),
            .modes = RIG_MODE_USB | RIG_MODE_LSB | RIG_MODE_AM | RIG_MODE_FM,
            .low_power = -1,
            .high_power = -1,
            .vfo = RIG_VFO_A,
            .ant = RIG_ANT_1,
            .label = "Simple CAT RX Range 2"
        },
        RIG_FRNG_END,
    },
    .tx_range_list1 =
    {
        {
            .startf = kHz(100),
            .endf = MHz(60),
            .modes = RIG_MODE_USB | RIG_MODE_LSB | RIG_MODE_AM | RIG_MODE_FM,
            .low_power = W(5),
            .high_power = W(100),
            .vfo = RIG_VFO_A,
            .ant = RIG_ANT_1,
            .label = "Simple CAT TX Range 1"
        },
        RIG_FRNG_END,
    },
    .tx_range_list2 =
    {
        {
            .startf = kHz(100),
            .endf = MHz(60),
            .modes = RIG_MODE_USB | RIG_MODE_LSB | RIG_MODE_AM | RIG_MODE_FM,
            .low_power = W(5),
            .high_power = W(100),
            .vfo = RIG_VFO_A,
            .ant = RIG_ANT_1,
            .label = "Simple CAT TX Range 2"
        },
        RIG_FRNG_END,
    },

    .tuning_steps =
    {
        {RIG_MODE_USB, Hz(10)},
        {RIG_MODE_LSB, Hz(10)},
        {RIG_MODE_AM, Hz(100)},
        {RIG_MODE_FM, Hz(100)},
        RIG_TS_END,
    },
    .filters =
    {
        {RIG_MODE_USB, kHz(2.4)},
        {RIG_MODE_LSB, kHz(2.4)},
        {RIG_MODE_AM, kHz(6)},
        {RIG_MODE_FM, kHz(15)},
        RIG_FLT_END,
    },

    .rig_init = simplecat_init,
    .rig_cleanup = simplecat_cleanup,
    .rig_open = simplecat_open,
    .rig_close = simplecat_close,

    .set_freq = simplecat_set_freq,
    .get_freq = simplecat_get_freq,
    .set_mode = simplecat_set_mode,
    .get_mode = simplecat_get_mode,
    .set_ptt = simplecat_set_ptt,
    .get_ptt = simplecat_get_ptt,
    .set_split_vfo = simplecat_set_split_vfo,
    .get_split_vfo = simplecat_get_split_vfo,
    .set_split_freq = simplecat_set_split_freq,
    .get_split_freq = simplecat_get_split_freq,
    .get_info = simplecat_get_info,
    .hamlib_check_rig_caps = HAMLIB_CHECK_RIG_CAPS
};

/*
 * Initialize backend
 */
DECLARE_INITRIG_BACKEND(simplecat)
{
    rig_debug(RIG_DEBUG_VERBOSE, "%s: _init called\n", __func__);

    rig_register(&simplecat_caps);

    return RIG_OK;
}
