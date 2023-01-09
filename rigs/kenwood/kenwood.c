/*
 *  Hamlib Kenwood backend - main file
 *  Copyright (c) 2000-2011 by Stephane Fillod
 *  Copyright (C) 2009,2010 Alessandro Zummo <a.zummo@towertech.it>
 *  Copyright (C) 2009,2010,2011,2012,2013 by Nate Bargmann, n0nb@n0nb.us
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
#include <hamlib/config.h>

#include <stdlib.h>
#include <stdio.h>
#include <string.h>  /* String function definitions */
#include <unistd.h>  /* UNIX standard function definitions */
#include <ctype.h>

#include "hamlib/rig.h"
#include "network.h"
#include "serial.h"
#include "register.h"
#include "cal.h"
#include "cache.h"

#include "kenwood.h"
#include "ts990s.h"

#ifndef max
#define max(a,b) (((a) > (b)) ? (a) : (b))
#define min(a,b) (((a) < (b)) ? (a) : (b))
#endif

struct kenwood_id
{
    rig_model_t model;
    int id;
};

struct kenwood_id_string
{
    rig_model_t model;
    const char *id;
};

#define UNKNOWN_ID -1

/*
 * Identification number as returned by "ID;"
 * Please, if the model number of your rig is listed as UNKNOWN_ID,
 * send the value to <fillods@users.sourceforge.net> for inclusion. Thanks --SF
 *
 * TODO: sort this list with most frequent rigs first.
 */
static const struct kenwood_id kenwood_id_list[] =
{
    { RIG_MODEL_TS940, 1 },
    { RIG_MODEL_TS811, 2 },
    { RIG_MODEL_TS711, 3 },
    { RIG_MODEL_TS440, 4 },
    { RIG_MODEL_R5000, 5 },
    { RIG_MODEL_TS140S, 6 },
//    { RIG_MODEL_TS680S, 6 }, // The TS680S is supposed #6 too but it will return as TS140S since it matches it
    { RIG_MODEL_TS790, 7 },
    { RIG_MODEL_TS950S, 8 },
    { RIG_MODEL_TS850, 9 },
    { RIG_MODEL_TS450S, 10 },
    { RIG_MODEL_TS690S, 11 },
    { RIG_MODEL_TS950SDX, 12 },
    { RIG_MODEL_TS50, 13 },
    { RIG_MODEL_TS870S, 15 },
    { RIG_MODEL_TRC80, 16 },
    { RIG_MODEL_TS570D, 17 }, /* Elecraft K2|K3 also returns 17 */
    { RIG_MODEL_TS570S, 18 },
    { RIG_MODEL_TS2000, 19 },
    { RIG_MODEL_TS480, 20 },
    { RIG_MODEL_TS590S, 21 },
    { RIG_MODEL_TS990S, 22 },
    { RIG_MODEL_TS590SG, 23 },
    { RIG_MODEL_TS890S, 24 },
    { RIG_MODEL_NONE, UNKNOWN_ID }, /* end marker */
};

/* XXX numeric ids have been tested only with the TS-450 */
static const struct kenwood_id_string kenwood_id_string_list[] =
{
    { RIG_MODEL_TS940,  "001" },
    { RIG_MODEL_TS811,  "002" },
    { RIG_MODEL_TS711,  "003" },
    { RIG_MODEL_TS440,  "004" },
    { RIG_MODEL_R5000,  "005" },
    { RIG_MODEL_TS140S, "006" },
    { RIG_MODEL_TS790,  "007" },
    { RIG_MODEL_TS950S, "008" },
    { RIG_MODEL_TS850,  "009" },
    { RIG_MODEL_TS450S, "010" },
    { RIG_MODEL_TS690S, "011" },
    { RIG_MODEL_TS950SDX, "012" },
    { RIG_MODEL_TS50,   "013" },
    { RIG_MODEL_TS870S, "015" },
    { RIG_MODEL_TS570D, "017" },  /* Elecraft K2|K3|KX3 also returns 17 */
    { RIG_MODEL_TS570S, "018" },
    { RIG_MODEL_TS2000, "019" },
    { RIG_MODEL_TS480,  "020" },
    { RIG_MODEL_PT8000A, "020" }, // TS480 ID but behaves differently
    { RIG_MODEL_SDRUNO, "020" }, // TS480 ID but behaves differently
    { RIG_MODEL_TS590S, "021" },
    { RIG_MODEL_TS990S, "022" },
    { RIG_MODEL_TS590SG,  "023" },
    { RIG_MODEL_TS890S,  "024" },
    { RIG_MODEL_THD7A,  "TH-D7" },
    { RIG_MODEL_THD7AG, "TH-D7G" },
    { RIG_MODEL_TMD700, "TM-D700" },
    { RIG_MODEL_TMD710, "TM-D710" },
    { RIG_MODEL_THD72A, "TH-D72" },
    { RIG_MODEL_THD74, "TH-D74" },
    { RIG_MODEL_TMV7, "TM-V7" },
    { RIG_MODEL_TMV71,  "TM-V71" },
    { RIG_MODEL_THF6A,  "TH-F6" },
    { RIG_MODEL_THF7E,  "TH-F7" },
    { RIG_MODEL_THG71,  "TH-G71" },
    { RIG_MODEL_MALACHITE,  "020" },
    { RIG_MODEL_NONE, NULL }, /* end marker */
};

rmode_t kenwood_mode_table[KENWOOD_MODE_TABLE_MAX] =
{
    [0] = RIG_MODE_NONE,
    [1] = RIG_MODE_LSB,
    [2] = RIG_MODE_USB,
    [3] = RIG_MODE_CW,
    [4] = RIG_MODE_FM,
    [5] = RIG_MODE_AM,
    [6] = RIG_MODE_RTTY, // FSK Mode
    [7] = RIG_MODE_CWR,
    [8] = RIG_MODE_NONE,  /* TUNE mode or PKTUSB for SDRUNO */
    [9] = RIG_MODE_RTTYR,  // FSKR Mode
    [10] = RIG_MODE_PSK,
    [11] = RIG_MODE_PSKR,
    [12] = RIG_MODE_PKTLSB,
    [13] = RIG_MODE_PKTUSB,
    [14] = RIG_MODE_PKTFM,
    [15] = RIG_MODE_PKTAM
};

/*
 * 38 CTCSS sub-audible tones
 */
tone_t kenwood38_ctcss_list[] =
{
    670,  719,  744,  770,  797,  825,  854,  885,  915,  948,
    974, 1000, 1035, 1072, 1109, 1148, 1188, 1230, 1273, 1318,
    1365, 1413, 1462, 1514, 1567, 1622, 1679, 1738, 1799, 1862,
    1928, 2035, 2107, 2181, 2257, 2336, 2418, 2503,
    0,
};


/*
 * 42 CTCSS sub-audible tones
 */
tone_t kenwood42_ctcss_list[] =
{
    670,  693,  719,  744,  770,  797,  825,  854,  885,  915,  948,
    974, 1000, 1035, 1072, 1109, 1148, 1188, 1230, 1273, 1318,
    1365, 1413, 1462, 1514, 1567, 1622, 1679, 1738, 1799, 1862,
    1928, 2035, 2065, 2107, 2181, 2257, 2291, 2336, 2418, 2503, 2541,
    0,
};


/* Token definitions for .cfgparams in rig_caps
 *
 * See enum rig_conf_e and struct confparams in rig.h
 */
struct confparams kenwood_cfg_params[] =
{
    {
        TOK_FINE, "fine", "Fine", "Fine step mode",
        NULL, RIG_CONF_CHECKBUTTON, { }
    },
    {
        TOK_VOICE, "voice", "Voice", "Voice recall",
        NULL, RIG_CONF_BUTTON, { }
    },
    {
        TOK_XIT, "xit", "XIT", "XIT",
        NULL, RIG_CONF_CHECKBUTTON, { }
    },
    {
        TOK_RIT, "rit", "RIT", "RIT",
        NULL, RIG_CONF_CHECKBUTTON, { }
    },
    {
        TOK_NO_ID, "no_id", "No ID", "If true do not send ID; with set commands",
        NULL, RIG_CONF_CHECKBUTTON, { }
    },
    { RIG_CONF_END, NULL, }
};


/**
 * kenwood_transaction
 * Assumes rig!=NULL rig->state!=NULL rig->caps!=NULL
 *
 * Parameters:
 * cmdstr:    Command to be sent to the rig. cmdstr can also be NULL,
 *        indicating that only a reply is needed (nothing will be sent).
 * data:    Buffer for reply string.  Can be NULL, indicating that no reply
 *        is needed and will return with RIG_OK after command was sent.
 * datasize: Size of buffer. It is the caller's responsibily to provide
 *         a large enough buffer for all possible replies for a command.
 *
 * returns:
 *   RIG_OK -   if no error occurred.
 *   RIG_EIO -    if an I/O error occurred while sending/receiving data.
 *   RIG_ETIMEOUT - if timeout expires without any characters received.
 *   RIG_REJECTED - if a negative acknowledge was received or command not
 *          recognized by rig.
 */
int kenwood_transaction(RIG *rig, const char *cmdstr, char *data,
                        size_t datasize)
{
    char buffer[KENWOOD_MAX_BUF_LEN]; /* use our own buffer since
                                       verification may need a longer
                                       buffer than the user supplied one */
    char cmdtrm_str[2];   /* Default Command/Reply termination char */
    int retval = -RIG_EINTERNAL;
    char *cmd;
    int len;
    int retry_read = 0;
    struct kenwood_priv_data *priv = rig->state.priv;
    struct kenwood_priv_caps *caps = kenwood_caps(rig);
    struct rig_state *rs;

    rig_debug(RIG_DEBUG_VERBOSE, "%s called cmd=%s datasize=%d\n", __func__,
              cmdstr ? cmdstr : "(NULL)",
              (int)datasize);

    if ((!cmdstr && !datasize) || (datasize && !data))
    {
        RETURNFUNC2(-RIG_EINVAL);
    }

    rs = &rig->state;

    rs->transaction_active = 1;

    /* Emulators don't need any post_write_delay */
    if (priv->is_emulation) { rs->rigport.post_write_delay = 0; }

    // if this is an IF cmdstr and not the first time through check cache
    if (cmdstr && strcmp(cmdstr, "IF") == 0 && priv->cache_start.tv_sec != 0)
    {
        int cache_age_ms;

        cache_age_ms = elapsed_ms(&priv->cache_start, HAMLIB_ELAPSED_GET);

        if (cache_age_ms < 500) // 500ms cache time
        {
            rig_debug(RIG_DEBUG_TRACE, "%s(%d): cache hit, age=%dms\n", __func__, __LINE__,
                      cache_age_ms);

            if (data) { strncpy(data, priv->last_if_response, datasize); }

            RETURNFUNC2(RIG_OK);
        }

        // else we drop through and do the real IF command
    }

    if (cmdstr && (strlen(cmdstr) > 2 || strcmp(cmdstr, "RX") == 0
                   || strncmp(cmdstr, "TX", 2) == 0 || strncmp(cmdstr, "ZZTX", 4)) == 0)
    {
        // then we must be setting something so we'll invalidate the cache
        rig_debug(RIG_DEBUG_TRACE, "%s: cache invalidated\n", __func__);
        priv->cache_start.tv_sec = 0;
    }

    cmdtrm_str[0] = caps->cmdtrm;
    cmdtrm_str[1] = '\0';

transaction_write:

    if (cmdstr)
    {
        rig_debug(RIG_DEBUG_TRACE, "%s: cmdstr = %s\n", __func__, cmdstr);

        len = strlen(cmdstr);

        cmd = calloc(1, len + 2);

        if (cmd == NULL)
        {
            retval = -RIG_ENOMEM;
            goto transaction_quit;
        }

        memcpy(cmd, cmdstr, len);

        /* XXX the if is temporary, until all invocations are fixed */
        if (cmdstr[len - 1] != ';' && cmdstr[len - 1] != '\r')
        {
            cmd[len] = caps->cmdtrm;
            len++;
        }

        /* flush anything in the read buffer before command is sent */
        rig_flush(&rs->rigport);

        retval = write_block(&rs->rigport, (unsigned char *) cmd, len);

        free(cmd);

        if (retval != RIG_OK)
        {
            goto transaction_quit;
        }
    }

    // we're not going to do the verify on RX cmd
    // Seems some rigs (like TS-480) return "?" when RX is done while PTT=OFF
    // So we'll skip the checks just on this one command for now
    // The TS-480 PC Control says RX; should return RX0; but it doesn't
    // We may eventually want to verify PTT with rig_get_ptt instead
    // The TS-2000 doesn't like doing and ID right after RU or RD
    if (retval == RIG_OK)
    {
        int skip = strncmp(cmdstr, "RX", 2) == 0;
        skip |= strncmp(cmdstr, "RU", 2) == 0;
        skip |= strncmp(cmdstr, "RD", 2) == 0;
        skip |= strncmp(cmdstr, "KYW", 3) == 0;

        if (skip)
        {
            goto transaction_quit;
        }
    }

    // Malachite SDR cannot send ID after FA
    if (!datasize && priv->no_id) { RETURNFUNC2(RIG_OK); }

    if (!datasize)
    {
        rig->state.transaction_active = 0;

        // there are some commands that have problems with immediate follow-up
        // so we'll just ignore them

        /* no reply expected so we need to write a command that always
           gives a reply so we can read any error replies from the actual
           command being sent without blocking */
        if (RIG_OK != (retval = write_block(&rs->rigport,
                                            (unsigned char *) priv->verify_cmd, strlen(priv->verify_cmd))))
        {
            goto transaction_quit;
        }
    }

transaction_read:
    /* allow room for most any response */
    len = min(datasize ? datasize + 1 : strlen(priv->verify_cmd) + 48,
              KENWOOD_MAX_BUF_LEN);
    retval = read_string(&rs->rigport, (unsigned char *) buffer, len,
                         cmdtrm_str, strlen(cmdtrm_str), 0, 1);
    rig_debug(RIG_DEBUG_TRACE, "%s: read_string(expected=%d, len=%d)='%s'\n",
              __func__,
              len, (int)strlen(buffer), buffer);

    if (retval < 0)
    {
        rig_debug(RIG_DEBUG_WARN,
                  "%s: read_string retval < 0, retval = %d, retry_read=%d, rs->rigport.retry=%d\n",
                  __func__,
                  retval, retry_read, rs->rigport.retry);

        // only retry if we expect a response from the command
        if (retry_read++ < rs->rigport.retry)
        {
            goto transaction_write;
            // we use to not re-do the write
            // but now we use ID; to verify commands are working
            // so in order to retry commands need to re-write them
            // https://github.com/Hamlib/Hamlib/issues/983
#if 0

            if (datasize)
            {
                goto transaction_write;
            }
            else if (-RIG_ETIMEOUT == retval)
            {
                goto transaction_read;
            }

#endif
        }

        goto transaction_quit;
    }

    /* Check that command termination is correct */
    if (strchr(cmdtrm_str, buffer[strlen(buffer) - 1]) == NULL)
    {
        rig_debug(RIG_DEBUG_ERR, "%s: Command is not correctly terminated '%s'\n",
                  __func__, buffer);

        if (retry_read++ < rs->rigport.retry)
        {
            goto transaction_write;
        }

        retval = -RIG_EPROTO;
        goto transaction_quit;
    }

    if (strlen(buffer) == 2)
    {
        switch (buffer[0])
        {
        case 'N':

            /* Command recognised by rig but invalid data entered. */
            if (cmdstr)
            {
                rig_debug(RIG_DEBUG_VERBOSE, "%s: NegAck for '%s'\n", __func__, cmdstr);
            }

            retval = -RIG_ENAVAIL;
            goto transaction_quit;

        case 'O':

            /* Too many characters sent without a carriage return */
            if (cmdstr)
            {
                rig_debug(RIG_DEBUG_VERBOSE, "%s: Overflow for '%s'\n", __func__, cmdstr);
            }

            if (retry_read++ < rs->rigport.retry)
            {
                goto transaction_write;
            }

            retval = -RIG_EPROTO;
            goto transaction_quit;

        case 'E':

            /* Communication error */
            if (cmdstr)
            {
                rig_debug(RIG_DEBUG_VERBOSE, "%s: Communication error for '%s'\n", __func__,
                          cmdstr);
            }

            if (retry_read++ < rs->rigport.retry)
            {
                goto transaction_write;
            }

            retval = -RIG_EIO;
            goto transaction_quit;

        case '?':

            /* Command not understood by rig or rig busy */
            if (cmdstr)
            {
                rig_debug(RIG_DEBUG_ERR, "%s: Unknown command or rig busy '%s'\n", __func__,
                          cmdstr);

                // sometimes IF; command after TX; will return ? but still return IF response
                if (retry_read++ <= 1)
                {
                    hl_usleep(100 * 1000);
                    goto transaction_read;
                }
            }

            if (retry_read++ < rs->rigport.retry)
            {
                rig_debug(RIG_DEBUG_ERR, "%s: Retrying shortly %d of %d\n", __func__,
                          retry_read, rs->rigport.retry);
                hl_usleep(rig->caps->timeout * 1000);
                goto transaction_write;
            }

            retval = -RIG_ERJCTED;
            goto transaction_quit;
        }
    }

    /*
     * Check that we received the correct reply. The first two characters
     * should be the same as command. Because the Elecraft XG3 uses
     * single character commands we only check the first character in
     * that case.
     */
    if (datasize)
    {
        if (cmdstr && (buffer[0] != cmdstr[0] || (cmdstr[1] && buffer[1] != cmdstr[1])))
        {
            /*
             * TODO: When RIG_TRN is enabled, we can pass the string to
             * the decoder for callback. That way we don't ignore any
             * commands.
             */
            rig_debug(RIG_DEBUG_ERR, "%s: wrong reply %c%c for command %c%c\n",
                      __func__, buffer[0], buffer[1], cmdstr[0], cmdstr[1]);

            rig_debug(RIG_DEBUG_ERR, "%s: retry_read=%d, rs->rigport.retry=%d\n", __func__,
                      retry_read, rs->rigport.retry);

            if (retry_read++ < rs->rigport.retry)
            {
                if (strlen(buffer) == 0)
                {
                    goto transaction_write;    // didn't get an answer so send again
                }
                else
                {
                    // should be able to handle transceive mode here
                    goto transaction_read;    // might be an async or corrupt reply so we'll read until timeout
                }
            }

            retval =  -RIG_EPROTO;
            goto transaction_quit;
        }

        if (retval > 0)
        {
            /* move the result excluding the command terminator into the
               caller buffer */
            len = min(datasize, retval) - 1;
            strncpy(data, buffer, len);
            data[len] = '\0';
        }
    }
    else
    {
        rig_debug(RIG_DEBUG_TRACE, "%s: No data expected, checking %s in %s\n",
                  __func__,
                  priv->verify_cmd, buffer);

        // seems some rigs will send back an IF response to RX/TX when it changes the status
        // normally RX/TX returns nothing when it's a null effect
        // TS-950SDX is known to behave this way
        if (strncmp(cmdstr, "RX", 2) == 0 || strncmp(cmdstr, "TX", 2) == 0)
        {
            if (strncmp(priv->verify_cmd, "IF", 2) == 0)
            {
                rig_debug(RIG_DEBUG_TRACE, "%s: RX/TX got IF response so we're good\n",
                          __func__);
                goto transaction_quit;
            }
        }

        if (priv->verify_cmd[0] != buffer[0]
                || (priv->verify_cmd[1] && priv->verify_cmd[1] != buffer[1]))
        {
            /*
             * TODO: When RIG_TRN is enabled, we can pass the string to
             * the decoder for callback. That way we don't ignore any
             * commands.
             */
            // if we got FA or FB unexpectedly then perhaps RIG_TRN is enabled and we just need to handle it
            if (strncmp(buffer, "FA", 2) == 0)
            {
                freq_t freq;
                sscanf(buffer, "FA%lg", &freq);
                rig_set_cache_freq(rig, RIG_VFO_A, freq);
                goto transaction_read;
            }
            else if (strncmp(buffer, "FB", 2) == 0)
            {
                freq_t freq;
                sscanf(buffer, "FB%lg", &freq);
                rig_set_cache_freq(rig, RIG_VFO_B, freq);
                goto transaction_read;
            }

            rig_debug(RIG_DEBUG_ERR, "%s: wrong reply %c%c for command verification %c%c\n",
                      __func__, buffer[0], buffer[1]
                      , priv->verify_cmd[0], priv->verify_cmd[1]);

            if (retry_read++ < rs->rigport.retry)
            {
                goto transaction_write;
            }

            retval =  -RIG_EPROTO;
            goto transaction_quit;
        }
    }

    retval = RIG_OK;
    rig_debug(RIG_DEBUG_TRACE, "%s: returning RIG_OK, retval=%d\n", __func__,
              retval);

transaction_quit:

    // update the cache
    if (retval == RIG_OK && cmdstr && strcmp(cmdstr, "IF") == 0)
    {
        elapsed_ms(&priv->cache_start, HAMLIB_ELAPSED_SET);
        strncpy(priv->last_if_response, buffer, caps->if_len);
    }

    rs->transaction_active = 0;
    RETURNFUNC2(retval);
}


/**
 * kenwood_safe_transaction
 * A wrapper for kenwood_transaction to check returned data against
 * expected length,
 *
 * Parameters:
 *  cmd     Same as kenwood_transaction() cmdstr
 *  buf     Same as kenwwod_transaction() data
 *  buf_size  Same as kenwood_transaction() datasize
 *  expected  Value of expected string length
 *
 * Returns:
 *   RIG_OK -   if no error occurred.
 *   RIG_EPROTO   if returned string and expected are not equal
 *   Error from kenwood_transaction() if any
 *
 */
int kenwood_safe_transaction(RIG *rig, const char *cmd, char *buf,
                             size_t buf_size, size_t expected)
{
    int err;
    int retry = 0;

    rig_debug(RIG_DEBUG_VERBOSE, "%s called, cmd=%s, expected=%d\n", __func__, cmd,
              (int)expected);

    if (!cmd)
    {
        RETURNFUNC2(-RIG_EINVAL);
    }

    memset(buf, 0, buf_size);

    if (expected == 0)
    {
        buf_size = 0;
    }

    do
    {
        size_t length;
        // some PowerSDR commands have variable len
        int checklen =  !RIG_IS_POWERSDR;
        err = kenwood_transaction(rig, cmd, buf, buf_size);

        if (err != RIG_OK)        /* return immediately on error as any
                                   retries handled at lower level */
        {
            RETURNFUNC2(err);
        }

        length = strlen(buf);

        if (checklen && length != expected) /* worth retrying as some rigs
                                   occasionally send short results */
        {
            struct kenwood_priv_data *priv = rig->state.priv;
            rig_debug(RIG_DEBUG_ERR,
                      "%s: wrong answer; len for cmd %s: expected = %d, got %d\n",
                      __func__, cmd, (int)expected, (int)length);
            err =  -RIG_EPROTO;
            elapsed_ms(&priv->cache_start, HAMLIB_ELAPSED_INVALIDATE);
            hl_usleep(50 * 1000); // let's do a short wait
        }
    }
    while (err != RIG_OK && ++retry < rig->state.rigport.retry);

    RETURNFUNC2(err);
}

rmode_t kenwood2rmode(unsigned char mode, const rmode_t mode_table[])
{
    rig_debug(RIG_DEBUG_VERBOSE, "%s called\n", __func__);

    if (mode >= KENWOOD_MODE_TABLE_MAX)
    {
        return (RIG_MODE_NONE);
    }

    return (mode_table[mode]);
}

char rmode2kenwood(rmode_t mode, const rmode_t mode_table[])
{
    rig_debug(RIG_DEBUG_VERBOSE, "%s called, mode=%s\n", __func__,
              rig_strrmode(mode));

    if (mode != RIG_MODE_NONE)
    {
        int i;

        for (i = 0; i < KENWOOD_MODE_TABLE_MAX; i++)
        {
            if (mode_table[i] == mode)
            {
                rig_debug(RIG_DEBUG_VERBOSE, "%s: returning %d\n", __func__, i);
                return (i);
            }
        }
    }

    return (-1);
}

int kenwood_init(RIG *rig)
{
    struct kenwood_priv_data *priv;
    struct kenwood_priv_caps *caps = kenwood_caps(rig);

    rig_debug(RIG_DEBUG_VERBOSE, "%s called, version %s/%s\n", __func__,
              BACKEND_VER, rig->caps->version);

    rig->state.priv = calloc(1, sizeof(struct kenwood_priv_data));

    if (rig->state.priv == NULL)
    {
        RETURNFUNC2(-RIG_ENOMEM);
    }

    priv = rig->state.priv;

    memset(priv, 0x00, sizeof(struct kenwood_priv_data));

    if (RIG_IS_XG3)
    {
        priv->verify_cmd[0] = caps->cmdtrm;
        priv->verify_cmd[1] = '\0';
    }
    else
    {
        priv->verify_cmd[0] = 'I';
        priv->verify_cmd[1] = 'D';
        priv->verify_cmd[2] = caps->cmdtrm;
        priv->verify_cmd[3] = '\0';
    }

    priv->split = RIG_SPLIT_OFF;
    priv->trn_state = -1;
    priv->curr_mode = 0;
    priv->micgain_min = -1;
    priv->micgain_max = -1;
    priv->has_ps = 1;  // until proven otherwise

    if (rig->caps->rig_model == RIG_MODEL_TS450S
            || rig->caps->rig_model == RIG_MODEL_TS50
            || rig->caps->rig_model == RIG_MODEL_TS140S
            || rig->caps->rig_model == RIG_MODEL_TS440)
    {
        priv->has_ps = 0;
    }

    /* default mode_table */
    if (caps->mode_table == NULL)
    {
        caps->mode_table = kenwood_mode_table;
    }

    /* default if_len */
    if (caps->if_len == 0)
    {
        caps->if_len = 37;
    }

    priv->ag_format = -1;  // force determination of AG format

    rig_debug(RIG_DEBUG_TRACE, "%s: if_len = %d\n", __func__, caps->if_len);

    // SDRUno uses mode 8 for DIG
    if (rig->caps->rig_model == RIG_MODEL_SDRUNO)
    {
        kenwood_mode_table[8] = RIG_MODE_PKTUSB;
    }

    RETURNFUNC2(RIG_OK);
}

int kenwood_cleanup(RIG *rig)
{
    ENTERFUNC;

    free(rig->state.priv);
    rig->state.priv = NULL;

    RETURNFUNC(RIG_OK);
}

int kenwood_open(RIG *rig)
{
    struct kenwood_priv_data *priv = rig->state.priv;
    struct kenwood_priv_caps *caps = kenwood_caps(rig);
    int err, i;
    char *idptr;
    char id[KENWOOD_MAX_BUF_LEN];
    int retry_save = rig->state.rigport.retry;

    ENTERFUNC;

    id[0] = 0;
    rig->state.rigport.retry = 0;
    err = kenwood_get_id(rig, id);

    if (err != RIG_OK)
    {
        // TS450S is flaky on the 1st ID call so we'll try again
        hl_usleep(200 * 1000);
        err = kenwood_get_id(rig, id);
    }

    if (err == RIG_OK && priv->has_ps)   // some rigs give ID while in standby
    {
        powerstat_t powerstat = 0;
        rig_debug(RIG_DEBUG_TRACE, "%s: got ID so try PS\n", __func__);
        err = rig_get_powerstat(rig, &powerstat);

        if (err == RIG_OK && powerstat == 0 && priv->poweron == 0
                && rig->state.auto_power_on)
        {
            priv->has_ps = 1;
            rig_debug(RIG_DEBUG_TRACE, "%s: got PS0 so powerup\n", __func__);
            rig_set_powerstat(rig, 1);
        }
        else if (err == -RIG_ETIMEOUT) // Some rigs like TS-450 dont' have PS cmd
        {
            priv->has_ps = 0;
        }

        priv->poweron = 1;

        err = RIG_OK;  // reset our err back to OK for later checks
    }

    if (err == -RIG_ETIMEOUT && rig->state.auto_power_on)
    {
        // Ensure rig is on
        rig_set_powerstat(rig, 1);
        /* Try get id again */
        err = kenwood_get_id(rig, id);
    }

    if (RIG_OK != err)
    {
        rig_debug(RIG_DEBUG_ERR,
                  "%s: no response to get_id from rig...continuing anyway\n", __func__);
    }

    if (RIG_IS_TS2000
            || RIG_IS_TS480
            || RIG_IS_TS590S
            || RIG_IS_TS590SG
            || RIG_IS_TS890S
            || RIG_IS_TS990S)
    {
        // rig has Set 2 RIT/XIT function
        rig_debug(RIG_DEBUG_TRACE, "%s: rig has_rit2\n", __func__);
        priv->has_rit2 = 1;
    }

    if (RIG_IS_TS590S)
    {
        /* we need the firmware version for these rigs to deal with f/w defects */
        static char fw_version[7];
        char *dot_pos;

        err = kenwood_transaction(rig, "FV", fw_version, sizeof(fw_version));

        if (RIG_OK != err)
        {
            rig_debug(RIG_DEBUG_ERR, "%s: cannot get f/w version, defaulting to 1.0\n",
                      __func__);
            rig->state.rigport.retry = retry_save;
            priv->fw_rev_uint = 100;
        }
        else
        {

            /* store the data  after the "FV" which should be  a f/w version
               string of the form n.n e.g. 1.07 */
            priv->fw_rev = &fw_version[2];
            dot_pos = strchr(fw_version, '.');

            if (dot_pos)
            {
                priv->fw_rev_uint = atoi(&fw_version[2]) * 100 + atoi(dot_pos + 1);
            }
            else
            {
                rig_debug(RIG_DEBUG_ERR, "%s: cannot get f/w version\n", __func__);
                rig->state.rigport.retry = retry_save;
                RETURNFUNC(-RIG_EPROTO);
            }
        }

        rig_debug(RIG_DEBUG_TRACE, "%s: found f/w version %.1f\n", __func__,
                  priv->fw_rev_uint / 100.0);
    }

    if (!RIG_IS_XG3 && -RIG_ETIMEOUT == err)
    {
        char buffer[KENWOOD_MAX_BUF_LEN];
        /* Some Kenwood emulations have no ID command response :(
         * Try an FA command to see if anyone is listening */
        err = kenwood_transaction(rig, "FA", buffer, sizeof(buffer));

        if (RIG_OK != err)
        {
            rig_debug(RIG_DEBUG_ERR, "%s: no response from rig\n", __func__);
            rig->state.rigport.retry = retry_save;
            RETURNFUNC(err);
        }

        /* here we know there is something that responds to FA but not
           to ID so use FA as the command verification command */
        priv->verify_cmd[0] = 'F';
        priv->verify_cmd[1] = 'A';
        priv->verify_cmd[2] = caps->cmdtrm;
        priv->verify_cmd[3] = '\0';
        strcpy(id, "ID019");      /* fake a TS-2000 */
    }
    else
    {
        if (err != RIG_OK)
        {
            rig_debug(RIG_DEBUG_ERR, "%s: cannot get identification\n", __func__);
            rig->state.rigport.retry = retry_save;
            RETURNFUNC(err);
        }
    }

    /* id is something like 'IDXXX' or 'ID XXX' */
    if (strlen(id) < 5)
    {
        rig_debug(RIG_DEBUG_ERR, "%s: unknown id type (%s)...continuing\n", __func__,
                  id);

        // Malachite SDR gives no response to ID and is supposed to be TS480 compatible
        if (RIG_IS_MALACHITE) { strcpy(id, "ID020"); }

    }

    if (!strcmp("IDID900", id)    /* DDUtil in TS-2000 mode */
            || !strcmp("ID900", id)   /* PowerSDR after ZZID; command */
            || !strcmp("ID904", id)   /* SmartSDR Flex-6700 */
            || !strcmp("ID905", id)   /* PowerSDR Flex-6500 */
            || !strcmp("ID906", id)   /* PowerSDR Flex-6700R */
            || !strcmp("ID907", id)   /* PowerSDR Flex-6300 */
            || !strcmp("ID908", id)   /* PowerSDR Flex-6400 */
            || !strcmp("ID909", id)   /* PowerSDR Flex-6600 */
       )
    {
        priv->is_emulation = 1;   /* Emulations don't have SAT mode */
        strcpy(id, "ID019");   /* fake it */
    }

    /* check for a white space and skip it */
    idptr = &id[2];

    if (*idptr == ' ')
    {
        idptr++;
    }

    /* compare id string */
    for (i = 0; kenwood_id_string_list[i].model != RIG_MODEL_NONE; i++)
    {
        //rig_debug(RIG_DEBUG_ERR, "%s: comparing '%s'=='%s'\n", __func__, kenwood_id_string_list[i].id, idptr);
        if (strcmp(kenwood_id_string_list[i].id, idptr) != 0)
        {
            continue;
        }

        /* found matching id, verify driver */
        rig_debug(RIG_DEBUG_TRACE, "%s: found match %s\n",
                  __func__, kenwood_id_string_list[i].id);

        // current vfo is rx_vfo
        rig_get_vfo(rig, &rig->state.rx_vfo);

        if (kenwood_id_string_list[i].model == rig->caps->rig_model)
        {
            int retval;
            split_t split;
            vfo_t tx_vfo;
            rig_debug(RIG_DEBUG_VERBOSE, "%s: found the right driver for %s(%d)\n",
                      __func__, rig->caps->model_name, rig->caps->rig_model);
            /* get current AI state so it can be restored */
            kenwood_get_trn(rig, &priv->trn_state);  /* ignore errors */

            /* Currently we cannot cope with AI mode so turn it off in
               case last client left it on */
            if (priv->trn_state != RIG_TRN_OFF)
            {
                kenwood_set_trn(rig, RIG_TRN_OFF); /* ignore status in case
                                                      it's not supported */
            }

            if (!RIG_IS_THD74 && !RIG_IS_THD7A)
            {
                // call get_split to fill in current split and tx_vfo status
                retval = kenwood_get_split_vfo_if(rig, RIG_VFO_A, &split, &tx_vfo);

                if (retval != RIG_OK)
                {
                    rig_debug(RIG_DEBUG_ERR, "%s: %s\n", __func__, rigerror(retval));
                }

                priv->tx_vfo = tx_vfo;
                rig_debug(RIG_DEBUG_VERBOSE, "%s: priv->tx_vfo=%s\n", __func__,
                          rig_strvfo(priv->tx_vfo));
            }

            rig->state.rigport.retry = retry_save;

            RETURNFUNC(RIG_OK);
        }

        /* driver mismatch */
        rig_debug(RIG_DEBUG_VERBOSE,
                  "%s: not the right driver apparently (found %u, asked for %d, checked %s)\n",
                  __func__, rig->caps->rig_model,
                  kenwood_id_string_list[i].model,
                  rig->caps->model_name);

        // we continue to search for other matching IDs/models
    }

    rig_debug(RIG_DEBUG_VERBOSE,
              "%s: your rig (%s) did not match but we will continue anyways\n",
              __func__, id);

    // we're making this non fatal
    // mismatched IDs can still be tested
    rig->state.rigport.retry = retry_save;

    RETURNFUNC(RIG_OK);
}


int kenwood_close(RIG *rig)
{
    struct kenwood_priv_data *priv = rig->state.priv;

    ENTERFUNC;

    if (!no_restore_ai && priv->trn_state >= 0)
    {
        /* restore AI state */
        kenwood_set_trn(rig, priv->trn_state); /* ignore status in case
                                                 it's not supported */
    }

    if (priv->poweron != 0 && rig->state.auto_power_off)
    {
        rig_debug(RIG_DEBUG_TRACE, "%s: got PS1 so powerdown\n", __func__);
        rig_set_powerstat(rig, 0);
    }

    RETURNFUNC(RIG_OK);
}


/* ID
 *  Reads transceiver ID number
 *
 *  caller must give a buffer of KENWOOD_MAX_BUF_LEN size
 *
 */
int kenwood_get_id(RIG *rig, char *buf)
{
    ENTERFUNC;

    RETURNFUNC(kenwood_transaction(rig, "ID", buf, KENWOOD_MAX_BUF_LEN));
}


/* IF
 *  Retrieves the transceiver status
 *
 */
int kenwood_get_if(RIG *rig)
{
    struct kenwood_priv_data *priv = rig->state.priv;
    struct kenwood_priv_caps *caps = kenwood_caps(rig);

    ENTERFUNC;

    RETURNFUNC(kenwood_safe_transaction(rig, "IF", priv->info,
                                        KENWOOD_MAX_BUF_LEN, caps->if_len));
}


/* FN FR FT
 *  Sets the RX/TX VFO or M.CH mode of the transceiver, does not set split
 *  VFO, but leaves it unchanged if in split VFO mode.
 *
 */
int kenwood_set_vfo(RIG *rig, vfo_t vfo)
{
    char cmdbuf[12];
    int retval;
    char vfo_function;
    struct kenwood_priv_data *priv = rig->state.priv;

    ENTERFUNC;
    rig_debug(RIG_DEBUG_VERBOSE,
              "%s called vfo=%s, is_emulation=%d, curr_mode=%s\n", __func__, rig_strvfo(vfo),
              priv->is_emulation,  rig_strrmode(priv->curr_mode));


    /* Emulations do not need to set VFO since VFOB is a copy of VFOA
     * except for frequency.  And we can change freq without changing VFOS
     * This prevents a 1.8 second delay in PowerSDR when switching VFOs
     * We'll do this once if curr_mode has not been set yet
     */
    if (vfo == RIG_VFO_B &&  priv->is_emulation && priv->curr_mode > 0)
    {
        HAMLIB_TRACE;
        RETURNFUNC2(RIG_OK);
    }

#if 0

    if (rig->state.current_vfo == vfo)
    {
        rig_debug(RIG_DEBUG_VERBOSE, "%s: vfo already is %s...skipping\n", __func__,
                  rig_strvfo(vfo));
        RETURNFUNC2(RIG_OK);
    }

#endif

    switch (vfo)
    {
    case RIG_VFO_A:
        vfo_function = '0';
        break;

    case RIG_VFO_B:
        vfo_function = '1';
        break;

    case RIG_VFO_MEM:
        vfo_function = '2';
        break;

    case RIG_VFO_TX:
        vfo_function = rig->state.tx_vfo == RIG_VFO_B ? '1' : '0';
        break;

#if 0 // VFO_RX really should NOT be VFO_CURR as VFO_CURR could be either VFO

    case RIG_VFO_RX:
        vfo_function = rig->state.rx_vfo == RIG_VFO_B ? '1' : '0';
        break;
#endif

    case RIG_VFO_CURR:
        HAMLIB_TRACE;
        rig->state.current_vfo = RIG_VFO_CURR;
        RETURNFUNC2(RIG_OK);

    default:
        rig_debug(RIG_DEBUG_ERR, "%s: unsupported VFO %s\n", __func__, rig_strvfo(vfo));
        RETURNFUNC2(-RIG_EINVAL);
    }

    //if rig=ts2000 then check Satellite mode status
    if (RIG_IS_TS2000 && !priv->is_emulation)
    {
        char retbuf[20];
        rig_debug(RIG_DEBUG_VERBOSE, "%s: checking satellite mode status\n", __func__);
        SNPRINTF(cmdbuf, sizeof(cmdbuf), "SA");

        retval = kenwood_transaction(rig, cmdbuf, retbuf, 20);

        if (retval != RIG_OK)
        {
            RETURNFUNC2(retval);
        }

        rig_debug(RIG_DEBUG_VERBOSE, "%s: satellite mode status %s\n", __func__,
                  retbuf);

        //Satellite mode ON
        if (retbuf[2] == '1')
        {
            //SAT mode doesn't allow FR command (cannot select VFO)
            //selecting VFO is useless in SAT MODE
            RETURNFUNC2(RIG_OK);
        }
    }

    HAMLIB_TRACE;
    SNPRINTF(cmdbuf, sizeof(cmdbuf), "FR%c", vfo_function);

    // as we change VFO we will change split to the other VFO
    // some rigs turn split off with FR command
    if (priv->split)
    {
        if (vfo_function == '0')
        {
            strcat(cmdbuf, ";FT1");
        }
        else
        {
            strcat(cmdbuf, ";FT0");
        }
    }

    if (RIG_IS_TS50 || RIG_IS_TS940)
    {
        cmdbuf[1] = 'N';
    }

    /* set RX VFO */
    retval = kenwood_transaction(rig, cmdbuf, NULL, 0);

    if (retval != RIG_OK)
    {
        RETURNFUNC2(retval);
    }

    HAMLIB_TRACE;
    rig->state.current_vfo = vfo;

    /* if FN command then there's no FT or FR */
    /* If split mode on, the don't change TxVFO */
    if ('N' == cmdbuf[1] || priv->split != RIG_SPLIT_OFF)
    {
        RETURNFUNC2(RIG_OK);
    }

    HAMLIB_TRACE;

    // some rigs need split turned on after VFOA is set
    if (priv->split == RIG_SPLIT_ON)
    {
        // so let's figure out who the rx_vfo is based on the tx_vfo
        HAMLIB_TRACE;
        vfo_t rx_vfo = RIG_VFO_A;

        switch (priv->tx_vfo)
        {
        case RIG_VFO_A:
            rx_vfo = RIG_VFO_B;
            break;

        case RIG_VFO_MAIN:
            rx_vfo = RIG_VFO_SUB;
            break;

        case RIG_VFO_MAIN_A:
            rx_vfo = RIG_VFO_MAIN_B;
            break;

        case RIG_VFO_B:
            rx_vfo = RIG_VFO_A;
            break;

        case RIG_VFO_SUB:
            rx_vfo = RIG_VFO_MAIN;
            break;

        case RIG_VFO_SUB_B:
            rx_vfo = RIG_VFO_MAIN_A;
            break;

        default:
            rig_debug(RIG_DEBUG_ERR, "%s: unhandled VFO=%s, deafaulting to VFOA\n",
                      __func__, rig_strvfo(priv->tx_vfo));

        }

        retval = rig_set_split_vfo(rig, rx_vfo, 1, priv->tx_vfo);
    }

#if 0
    /* set TX VFO */
    cmdbuf[1] = 'T';
    RETURNFUNC(kenwood_transaction(rig, cmdbuf, NULL, 0));
#else
    RETURNFUNC2(retval);
#endif
}


/* CB
 *  Sets the operating VFO, does not set split
 *  VFO, but leaves it unchanged if in split VFO mode.
 *
 */
int kenwood_set_vfo_main_sub(RIG *rig, vfo_t vfo)
{
    char cmdbuf[6];
    char vfo_function;

    ENTERFUNC;

    switch (vfo)
    {
    case RIG_VFO_MAIN:
        vfo_function = '0';
        break;

    case RIG_VFO_SUB:
        vfo_function = '1';
        break;

    case RIG_VFO_CURR:
        RETURNFUNC(RIG_OK);

    default:
        rig_debug(RIG_DEBUG_ERR, "%s: unsupported VFO %s\n", __func__, rig_strvfo(vfo));
        RETURNFUNC(-RIG_EINVAL);
    }

    SNPRINTF(cmdbuf, sizeof(cmdbuf), "CB%c", vfo_function);
    RETURNFUNC(kenwood_transaction(rig, cmdbuf, NULL, 0));
}


/* CB
 *  Gets the operating VFO
 *
 */
int kenwood_get_vfo_main_sub(RIG *rig, vfo_t *vfo)
{
    char buf[4];
    int rc;

    ENTERFUNC;

    if (!vfo)
    {
        RETURNFUNC(-RIG_EINVAL);
    }

    if (RIG_OK == (rc = kenwood_safe_transaction(rig, "CB", buf, sizeof(buf), 3)))
    {
        *vfo = buf[2] == '1' ? RIG_VFO_SUB : RIG_VFO_MAIN;
    }

    RETURNFUNC(rc);
}


/* FR FT TB
 *  Sets the split RX/TX VFO or M.CH mode of the transceiver.
 *
 */
int kenwood_set_split_vfo(RIG *rig, vfo_t vfo, split_t split, vfo_t txvfo)
{
    struct kenwood_priv_data *priv = rig->state.priv;
    char cmdbuf[12];
    int retval;
    unsigned char vfo_function;
    split_t tsplit = 0;

    rig_debug(RIG_DEBUG_VERBOSE, "%s called %s,%d,%s\n", __func__, rig_strvfo(vfo),
              split, rig_strvfo(txvfo));

    if (RIG_IS_TS990S)
    {
        if (split)
        {
            // Rx MAIN/Tx SUB is the only split method
            retval = kenwood_set_vfo_main_sub(rig, RIG_VFO_MAIN);

            if (retval != RIG_OK) { RETURNFUNC2(retval); }
        }

        SNPRINTF(cmdbuf, sizeof(cmdbuf), "TB%c", RIG_SPLIT_ON == split ? '1' : '0');
        RETURNFUNC2(kenwood_transaction(rig, cmdbuf, NULL, 0));
    }

    if (vfo == RIG_VFO_CURR) { vfo = rig->state.current_vfo; }

    if (vfo == RIG_VFO_TX || vfo == RIG_VFO_RX) { vfo = vfo_fixup(rig, vfo, split); }

    switch (vfo)
    {
    case RIG_VFO_A: vfo_function = '0'; break;

    case RIG_VFO_B: vfo_function = '1'; break;

    case RIG_VFO_MEM: vfo_function = '2'; break;

    default:
        rig_debug(RIG_DEBUG_ERR, "%s: unsupported VFO %s\n", __func__, rig_strvfo(vfo));
        RETURNFUNC2(-RIG_EINVAL);
    }

    vfo_t tx_vfo;
    rig_get_split_vfo(rig, vfo, &tsplit, &tx_vfo);
    rig_debug(RIG_DEBUG_VERBOSE, "%s(%d): tsplit=%d, split=%d\n", __func__,
              __LINE__, tsplit, split);

    if (tsplit == split)
    {
        rig_debug(RIG_DEBUG_TRACE, "%s: split already set\n", __func__);
        RETURNFUNC2(RIG_OK);
    }

    /* set RX VFO */
    SNPRINTF(cmdbuf, sizeof(cmdbuf), "FR%c", vfo_function);

    // FR can turn off split on some Kenwood rigs
    // So we'll turn it back on just in case
    HAMLIB_TRACE;

    if (split)
    {
        if (vfo_function == '0')
        {
            HAMLIB_TRACE;
            strcat(cmdbuf, ";FT1");
        }
        else
        {
            HAMLIB_TRACE;
            strcat(cmdbuf, ";FT0");
        }
    }
    else
    {
        HAMLIB_TRACE;
        strcat(cmdbuf, ";FT0");
    }

    retval = kenwood_transaction(rig, cmdbuf, NULL, 0);

    if (retval != RIG_OK)
    {
        RETURNFUNC2(retval);
    }

    /* Split off means Rx and Tx are the same */
    if (split == RIG_SPLIT_OFF)
    {
        txvfo = vfo;

        if (txvfo == RIG_VFO_CURR)
        {
            retval = rig_get_vfo(rig, &txvfo);

            if (retval != RIG_OK)
            {
                RETURNFUNC2(retval);
            }
        }
    }

    if (txvfo == RIG_VFO_CURR && vfo == RIG_VFO_A)
    {
        if (vfo == RIG_VFO_A) { txvfo = RIG_VFO_B; }
        else if (vfo == RIG_VFO_B) { txvfo = RIG_VFO_A; }
        else
        {
            rig_debug(RIG_DEBUG_ERR, "%s: unsupported split VFO=%s\n", __func__,
                      rig_strvfo(txvfo));
            RETURNFUNC2(-RIG_EINVAL);
        }
    }

    txvfo = vfo_fixup(rig, txvfo, RIG_SPLIT_ON);

    switch (txvfo)
    {
    case RIG_VFO_VFO:
    case RIG_VFO_MAIN:
    case RIG_VFO_A: vfo_function = '0'; break;

    case RIG_VFO_SUB:
    case RIG_VFO_B: vfo_function = '1'; break;

    case RIG_VFO_MEM: vfo_function = '2'; break;

    default:
        rig_debug(RIG_DEBUG_ERR, "%s: unsupported VFO %s\n", __func__,
                  rig_strvfo(txvfo));
        RETURNFUNC2(-RIG_EINVAL);
    }

    priv->tx_vfo = txvfo;

    /* do not attempt redundant split change commands on Elecraft as
       they impact output power when transmitting
       and all other rigs don't need to set it if it's already set correctly
    */
    tsplit = RIG_SPLIT_OFF; // default in case rig does not set split status
    retval = rig_get_split_vfo(rig, vfo, &tsplit, &tx_vfo);

    priv->split = rig->state.cache.split = split;
    rig->state.cache.split_vfo = txvfo;
    elapsed_ms(&rig->state.cache.time_split, HAMLIB_ELAPSED_SET);

    // and it should be OK to do a SPLIT_OFF at any time so we won's skip that
    if (retval == RIG_OK && split == RIG_SPLIT_ON && tsplit == RIG_SPLIT_ON)
    {
        rig_debug(RIG_DEBUG_VERBOSE, "%s: already set split=%d\n", __func__, tsplit);
        RETURNFUNC2(RIG_OK);
    }

    rig_debug(RIG_DEBUG_VERBOSE, "%s: split is=%d, split wants=%d\n", __func__,
              tsplit, split);

    /* set TX VFO */
    // if turning on split need to do some VFOB setup on Elecraft rigs to avoid SPLIT N/A and ER59 messages
    if (rig->caps->rig_model ==
            RIG_MODEL_K4 //  Elecraft needs VFOB to be same band as VFOA
            || rig->caps->rig_model == RIG_MODEL_K3
            || rig->caps->rig_model == RIG_MODEL_KX2
            || rig->caps->rig_model == RIG_MODEL_KX3)
    {
        rig_set_freq(rig, RIG_VFO_B, rig->state.cache.freqMainA);
    }

    if (retval != RIG_OK)
    {
        RETURNFUNC2(retval);
    }

    /* Remember whether split is on, for kenwood_set_vfo */
    priv->split = rig->state.cache.split = split;
    elapsed_ms(&rig->state.cache.time_split, HAMLIB_ELAPSED_SET);

    RETURNFUNC2(RIG_OK);
}


/* SP
 *  Sets the split mode of the transceivers that have the FN command.
 *
 */
int kenwood_set_split(RIG *rig, vfo_t vfo, split_t split, vfo_t txvfo)
{
    struct kenwood_priv_data *priv = rig->state.priv;
    char cmdbuf[6];
    int retval;

    ENTERFUNC;

    SNPRINTF(cmdbuf, sizeof(cmdbuf), "SP%c", RIG_SPLIT_ON == split ? '1' : '0');

    retval = kenwood_transaction(rig, cmdbuf, NULL, 0);

    if (retval != RIG_OK)
    {
        RETURNFUNC(retval);
    }

    /* Remember whether split is on, for kenwood_set_vfo */
    priv->split = split;
    priv->tx_vfo = txvfo;
    rig_debug(RIG_DEBUG_VERBOSE, "%s: priv->tx_vfo=%s\n", __func__,
              rig_strvfo(priv->tx_vfo));

    RETURNFUNC(RIG_OK);
}


/* IF TB
 *  Gets split VFO status from kenwood_get_if()
 *
 */
int kenwood_get_split_vfo_if(RIG *rig, vfo_t rxvfo, split_t *split,
                             vfo_t *txvfo)
{
    int transmitting;
    int retval;
    struct kenwood_priv_data *priv = rig->state.priv;

    ENTERFUNC;

    if (!split || !txvfo)
    {
        RETURNFUNC(-RIG_EINVAL);
    }

    if (RIG_IS_TS990S)
    {
        char buf[4];

        if (RIG_OK == (retval = kenwood_safe_transaction(rig, "TB", buf, sizeof(buf),
                                3)))
        {
            if ('1' == buf[2])
            {
                *split = RIG_SPLIT_ON;
                *txvfo = RIG_VFO_SUB;
                priv->tx_vfo = rig->state.tx_vfo = *txvfo;
            }
            else
            {
                *split = RIG_SPLIT_OFF;
                *txvfo = RIG_VFO_MAIN;
                priv->tx_vfo = rig->state.tx_vfo = *txvfo;
            }
        }

        RETURNFUNC(retval);
    }

    retval = kenwood_get_if(rig);

    if (retval != RIG_OK)
    {
        RETURNFUNC(retval);
    }

    switch (priv->info[32])
    {
    case '0':
        *split = RIG_SPLIT_OFF;
        break;

    case '1':
        *split = RIG_SPLIT_ON;
        break;

    default:
        rig_debug(RIG_DEBUG_ERR, "%s: unsupported split %c\n",
                  __func__, priv->info[32]);
        RETURNFUNC(-RIG_EPROTO);
    }

    /* Remember whether split is on, for kenwood_set_vfo */
    priv->split = *split;

    /* find where is the txvfo.. */
    /* Elecraft info[30] does not track split VFO when transmitting */
    transmitting = '1' == priv->info[28] && !RIG_IS_K2 && !RIG_IS_K3;

    switch (priv->info[30])
    {
    case '0':
        if (rig->state.rx_vfo == RIG_VFO_A)
        {
            HAMLIB_TRACE;
            *txvfo = rig->state.tx_vfo = priv->tx_vfo = (*split
                                         && !transmitting) ? RIG_VFO_B : RIG_VFO_A;
        }
        else if (rig->state.rx_vfo == RIG_VFO_B)
        {
            HAMLIB_TRACE;
            *txvfo = rig->state.tx_vfo = priv->tx_vfo = (*split
                                         && !transmitting) ? RIG_VFO_B : RIG_VFO_A;
        }
        else
        {
            rig_debug(RIG_DEBUG_WARN, "%s(%d): unknown rx_vfo=%s\n", __func__, __LINE__,
                      rig_strvfo(rig->state.rx_vfo));
            *txvfo = RIG_VFO_A; // pick a default
            rig->state.rx_vfo = priv->tx_vfo = RIG_VFO_A;
        }

        break;

    case '1':
        if (rig->state.rx_vfo == RIG_VFO_A)
        {
            HAMLIB_TRACE;
            *txvfo = priv->tx_vfo = (*split && !transmitting) ? RIG_VFO_A : RIG_VFO_B;
        }
        else if (rig->state.rx_vfo == RIG_VFO_B)
        {
            HAMLIB_TRACE;
            *txvfo = priv->tx_vfo = (*split && !transmitting) ? RIG_VFO_B : RIG_VFO_A;
        }
        else
        {
            rig_debug(RIG_DEBUG_WARN, "%s(%d): unknown rx_vfo=%s\n", __func__, __LINE__,
                      rig_strvfo(rig->state.rx_vfo));
            *txvfo = RIG_VFO_A; // pick a default
            rig->state.rx_vfo = RIG_VFO_A;
        }

        break;

    case '2':
        *txvfo = priv->tx_vfo =
                     RIG_VFO_MEM; /* SPLIT MEM operation doesn't involve VFO A or VFO B */
        break;

    default:
        rig_debug(RIG_DEBUG_ERR, "%s: unsupported VFO %c\n",
                  __func__, priv->info[30]);
        RETURNFUNC(-RIG_EPROTO);
    }

    priv->tx_vfo = *txvfo;
    rig_debug(RIG_DEBUG_VERBOSE, "%s: priv->tx_vfo=%s, split=%d\n", __func__,
              rig_strvfo(priv->tx_vfo), *split);
    RETURNFUNC(RIG_OK);
}


/*
 * kenwood_get_vfo_if using byte 31 of the IF information field
 *
 * Specifically this needs to return the RX VFO, the IF command tells
 * us the TX VFO in split TX mode when transmitting so we need to swap
 * results sometimes.
 */
int kenwood_get_vfo_if(RIG *rig, vfo_t *vfo)
{
    int retval;
    int split_and_transmitting;
    struct kenwood_priv_data *priv = rig->state.priv;

    ENTERFUNC;

    if (!vfo)
    {
        RETURNFUNC(-RIG_EINVAL);
    }

    retval = kenwood_get_if(rig);

    if (retval != RIG_OK)
    {
        RETURNFUNC(retval);
    }

    /* Elecraft info[30] does not track split VFO when transmitting */
    split_and_transmitting =
        '1' == priv->info[28] /* transmitting */
        && '1' == priv->info[32]               /* split */
        && !RIG_IS_K2
        && !RIG_IS_K3;

    switch (priv->info[30])
    {
    case '0':
        *vfo = rig->state.rx_vfo = rig->state.tx_vfo = priv->tx_vfo =
                                       split_and_transmitting ? RIG_VFO_B : RIG_VFO_A;

        if (priv->info[32] == '1') { priv->tx_vfo = rig->state.tx_vfo = RIG_VFO_B; }

        break;

    case '1':
        *vfo = split_and_transmitting ? RIG_VFO_A : RIG_VFO_B;
        priv->tx_vfo = RIG_VFO_B;
        break;

    case '2':
        *vfo = priv->tx_vfo = RIG_VFO_MEM;
        break;

    default:
        rig_debug(RIG_DEBUG_ERR, "%s: unsupported VFO %c\n",
                  __func__, priv->info[30]);
        RETURNFUNC(-RIG_EPROTO);
    }

    rig_debug(RIG_DEBUG_VERBOSE, "%s: priv->tx_vfo=%s\n", __func__,
              rig_strvfo(priv->tx_vfo));
    RETURNFUNC(RIG_OK);
}


/*
 * kenwood_set_freq
 */
int kenwood_set_freq(RIG *rig, vfo_t vfo, freq_t freq)
{
    char freqbuf[16];
    unsigned char vfo_letter = '\0';
    vfo_t tvfo;
    freq_t tfreq = 0;
    int err;
    struct kenwood_priv_data *priv = rig->state.priv;

    rig_debug(RIG_DEBUG_VERBOSE, "%s called vfo=%s freq=%.0f\n", __func__,
              rig_strvfo(vfo), freq);

    tvfo = (vfo == RIG_VFO_CURR
            || vfo == RIG_VFO_VFO) ? rig->state.current_vfo : vfo;

    rig_debug(RIG_DEBUG_TRACE, "%s: tvfo=%s\n", __func__, rig_strvfo(tvfo));

    if (tvfo == RIG_VFO_CURR || tvfo == RIG_VFO_NONE)
    {
        /* fetch from rig */
        err = rig_get_vfo(rig, &tvfo);

        if (RIG_OK != err) { RETURNFUNC2(err); }
    }

    rig_get_freq(rig, tvfo, &tfreq);

    if (tfreq == freq)
    {
        rig_debug(RIG_DEBUG_TRACE, "%s: no freq change needed\n", __func__);
        RETURNFUNC2(RIG_OK);
    }

    switch (tvfo)
    {
    case RIG_VFO_A:
    case RIG_VFO_MAIN:
        vfo_letter = 'A';
        break;

    case RIG_VFO_B:
    case RIG_VFO_SUB:
        vfo_letter = 'B';
        break;

    case RIG_VFO_C:
        vfo_letter = 'C';
        break;

    case RIG_VFO_TX:
        if (priv->tx_vfo == RIG_VFO_A) { vfo_letter = 'A'; }
        else if (priv->tx_vfo == RIG_VFO_B) { vfo_letter = 'B'; }
        else
        {
            rig_debug(RIG_DEBUG_ERR, "%s: unsupported tx_vfo, tx_vfo=%s\n", __func__,
                      rig_strvfo(priv->tx_vfo));
        }

        break;

    default:
        rig_debug(RIG_DEBUG_ERR, "%s: unsupported VFO %s\n", __func__, rig_strvfo(vfo));
        RETURNFUNC2(-RIG_EINVAL);
    }

    // cppcheck-suppress *
    SNPRINTF(freqbuf, sizeof(freqbuf), "F%c%011"PRIll, vfo_letter, (int64_t)freq);

    // we need to modify priv->verify_cmd if ID is not being used
    // if FB command than we change to FB and back again to avoid VFO blinking
    if (priv->verify_cmd[1] == 'A' && vfo_letter == 'B') { priv->verify_cmd[1] = 'A'; }

    err = kenwood_transaction(rig, freqbuf, NULL, 0);

    if (priv->verify_cmd[1] == 'B' && vfo_letter == 'B') { priv->verify_cmd[1] = 'A'; }

    if (RIG_OK == err && RIG_IS_TS590S
            && priv->fw_rev_uint <= 107 && ('A' == vfo_letter || 'B' == vfo_letter))
    {
        /* TS590s f/w rev 1.07 or earlier has a defect that means
           frequency set on TX VFO in split mode may not be set
           correctly.

           The symptom of the defect is either TX on the wrong
           frequency (i.e. TX on a frequency different from that
           showing on the TX VFO) or no output.

           We use an IF command to find out if we have just set
           the "back" VFO when the rig is in split mode. If we
           have; we then read the other VFO and set it to what we
           read - a null transaction that fixes the defect. */

        err = kenwood_get_if(rig);

        if (RIG_OK != err)
        {
            RETURNFUNC2(err);
        }

        if ('1' == priv->info[32] && priv->info[30] != ('A' == vfo_letter ? '0' : '1'))
        {
            /* split mode and setting "back" VFO */

            /* set other VFO to whatever it is at currently */
            err = kenwood_safe_transaction(rig, 'A' == vfo_letter ? "FB" : "FA", freqbuf,
                                           16, 13);

            if (RIG_OK != err)
            {
                RETURNFUNC2(err);
            }

            err = kenwood_transaction(rig, freqbuf, NULL, 0);
        }
    }

    RETURNFUNC2(err);
}

int kenwood_get_freq_if(RIG *rig, vfo_t vfo, freq_t *freq)
{
    struct kenwood_priv_data *priv = rig->state.priv;
    char freqbuf[50];
    int retval;

    ENTERFUNC;

    if (!freq)
    {
        RETURNFUNC(-RIG_EINVAL);
    }

    retval = kenwood_get_if(rig);

    if (retval != RIG_OK)
    {
        RETURNFUNC(retval);
    }

    memcpy(freqbuf, priv->info, 15);
    freqbuf[14] = '\0';
    sscanf(freqbuf + 2, "%"SCNfreq, freq);

    RETURNFUNC(RIG_OK);
}

/*
 * kenwood_get_freq
 */
int kenwood_get_freq(RIG *rig, vfo_t vfo, freq_t *freq)
{
    char freqbuf[50];
    char cmdbuf[4];
    int retval;
    unsigned char vfo_letter = '\0';
    vfo_t tvfo;
    struct kenwood_priv_data *priv = rig->state.priv;

    ENTERFUNC;

    if (!freq)
    {
        RETURNFUNC(-RIG_EINVAL);
    }

    tvfo = (vfo == RIG_VFO_CURR
            || vfo == RIG_VFO_VFO) ? rig->state.current_vfo : vfo;

    if (RIG_VFO_CURR == tvfo)
    {
        /* fetch from rig */
        retval = rig_get_vfo(rig, &tvfo);

        if (RIG_OK != retval) { RETURNFUNC(retval); }
    }

    /* memory frequency cannot be read with an Fx command, use IF */
    if (tvfo == RIG_VFO_MEM)
    {

        RETURNFUNC(kenwood_get_freq_if(rig, vfo, freq));
    }

    switch (tvfo)
    {
    case RIG_VFO_A:
    case RIG_VFO_MAIN:
        vfo_letter = 'A';
        break;

    case RIG_VFO_B:
    case RIG_VFO_SUB:
        vfo_letter = 'B';
        break;

    case RIG_VFO_C:
        vfo_letter = 'C';
        break;

    case RIG_VFO_TX:
        if (priv->split) { vfo_letter = 'B'; } // always assume B is the TX VFO
        else { vfo_letter = 'A'; }

        break;

    default:
        rig_debug(RIG_DEBUG_ERR, "%s: unsupported VFO %s\n", __func__, rig_strvfo(vfo));
        RETURNFUNC(-RIG_EINVAL);
    }

    if (rig->caps->rig_model == RIG_MODEL_MALACHITE && vfo == RIG_VFO_B)
    {
        // Malachite does not have VFOB so we'll just return VFOA
        *freq = 0;
        RETURNFUNC(RIG_OK);
    }

    SNPRINTF(cmdbuf, sizeof(cmdbuf), "F%c", vfo_letter);

    retval = kenwood_safe_transaction(rig, cmdbuf, freqbuf, 50, 13);

    if (retval != RIG_OK)
    {
        RETURNFUNC(retval);
    }

    sscanf(freqbuf + 2, "%"SCNfreq, freq);

    RETURNFUNC(RIG_OK);
}

int kenwood_get_rit(RIG *rig, vfo_t vfo, shortfreq_t *rit)
{
    int retval;
    char buf[7];
    struct kenwood_priv_data *priv = rig->state.priv;

    ENTERFUNC;

    if (!rit)
    {
        RETURNFUNC(-RIG_EINVAL);
    }

    retval = kenwood_get_if(rig);

    if (retval != RIG_OK)
    {
        RETURNFUNC(retval);
    }

    // TODO: Fix for different rigs
    memcpy(buf, &priv->info[17], 6);

    buf[6] = '\0';
    *rit = atoi(buf);

    RETURNFUNC(RIG_OK);
}

/*
 * rit can only move up/down by 10 Hz, so we use a loop...
 */
int kenwood_set_rit(RIG *rig, vfo_t vfo, shortfreq_t rit)
{
    char buf[32];
    int retval, i;
    int diff;
    int rit_enabled;
    int xit_enabled;
    shortfreq_t curr_rit;
    struct kenwood_priv_data *priv = rig->state.priv;

    rig_debug(RIG_DEBUG_VERBOSE, "%s called: vfo=%s, rit=%ld\n",
              __func__,
              rig_strvfo(vfo), rit);

    // RC clear command cannot be executed if RIT/XIT is not enabled
    retval = kenwood_get_func(rig, vfo, RIG_FUNC_RIT, &rit_enabled);

    if (retval != RIG_OK)
    {
        RETURNFUNC2(retval);
    }

    if (!rit_enabled)
    {
        retval = kenwood_get_func(rig, vfo, RIG_FUNC_XIT, &xit_enabled);

        if (retval != RIG_OK)
        {
            RETURNFUNC2(retval);
        }
    }

    if (!rit_enabled && !xit_enabled)
    {
        retval = kenwood_set_func(rig, vfo, RIG_FUNC_RIT, 1);

        if (retval != RIG_OK)
        {
            RETURNFUNC2(retval);
        }
    }

    // by getting current rit we can determine how to handle change
    // we just use curr_rit - rit to determine how far we need to move
    // No need to zero out rit
    retval = kenwood_get_rit(rig, RIG_VFO_CURR, &curr_rit);

    if (retval != RIG_OK)
    {
        RETURNFUNC2(retval);
    }

#if 0 // no longer needed if diff can be done
    retval = kenwood_transaction(rig, "RC", NULL, 0);

    if (retval != RIG_OK)
    {
        RETURNFUNC2(retval);
    }

#endif

    if (rit == 0 && curr_rit == 0)
    {
        RETURNFUNC2(RIG_OK);
    }

    if (priv->has_rit2)
    {
        diff = rit - curr_rit;
        rig_debug(RIG_DEBUG_TRACE, "%s: rit=%ld, curr_rit=%ld, diff=%d\n", __func__,
                  rit, curr_rit, diff);
        SNPRINTF(buf, sizeof(buf), "R%c%05d", (diff > 0) ? 'U' : 'D', abs((int) diff));
        retval = kenwood_transaction(rig, buf, NULL, 0);
    }
    else
    {
        SNPRINTF(buf, sizeof(buf), "R%c", (rit > 0) ? 'U' : 'D');
        diff = labs(((curr_rit - rit) + (curr_rit - rit) >= 0 ? 5 : -5) /
                    10); // round to nearest 10Hz
        rig_debug(RIG_DEBUG_TRACE, "%s: rit=%ld, curr_rit=%ld, diff=%d\n", __func__,
                  rit, curr_rit, diff);
        rig_debug(RIG_DEBUG_TRACE, "%s: rit change loop=%d\n", __func__, diff);

        for (i = 0; i < diff; i++)
        {
            retval = kenwood_transaction(rig, buf, NULL, 0);
        }
    }

    RETURNFUNC2(retval);
}

/*
 * rit and xit are the same
 */
int kenwood_get_xit(RIG *rig, vfo_t vfo, shortfreq_t *rit)
{
    ENTERFUNC;

    RETURNFUNC(kenwood_get_rit(rig, vfo, rit));
}

int kenwood_set_xit(RIG *rig, vfo_t vfo, shortfreq_t rit)
{
    ENTERFUNC;

    RETURNFUNC(kenwood_set_rit(rig, vfo, rit));
}

int kenwood_scan(RIG *rig, vfo_t vfo, scan_t scan, int ch)
{
    rig_debug(RIG_DEBUG_VERBOSE, "%s called\n", __func__);

    if (RIG_IS_TS990S)
    {
        RETURNFUNC(kenwood_transaction(rig, scan == RIG_SCAN_STOP ? "SC00" : "SC01",
                                       NULL, 0));
    }
    else
    {
        RETURNFUNC(kenwood_transaction(rig, scan == RIG_SCAN_STOP ? "SC0" : "SC1", NULL,
                                       0));
    }
}

/*
 *  000 No select
 *  002 FM Wide
 *  003 FM Narrow
 *  005 AM
 *  007 SSB
 *  009 CW
 *  010 CW NARROW
 */

/* XXX revise */
static int kenwood_set_filter(RIG *rig, pbwidth_t width)
{
    char *cmd;

    ENTERFUNC;

    if (width <= Hz(250))
    {
        cmd = "FL010009";
    }
    else if (width <= Hz(500))
    {
        cmd = "FL009009";
    }
    else if (width <= kHz(2.7))
    {
        cmd = "FL007007";
    }
    else if (width <= kHz(6))
    {
        cmd = "FL005005";
    }
    else
    {
        cmd = "FL002002";
    }

    RETURNFUNC(kenwood_transaction(rig, cmd, NULL, 0));
}

static int kenwood_set_filter_width(RIG *rig, rmode_t mode, pbwidth_t width)
{
    struct kenwood_priv_caps *caps = kenwood_caps(rig);
    struct kenwood_filter_width *selected_filter_width = NULL;
    char cmd[20];
    int i;

    rig_debug(RIG_DEBUG_VERBOSE, "%s called, width=%ld\n", __func__, width);

    if (caps->filter_width == NULL)
    {
        RETURNFUNC2(-RIG_ENAVAIL);
    }

    for (i = 0; caps->filter_width[i].value >= 0; i++)
    {
        if (caps->filter_width[i].modes & mode)
        {
            selected_filter_width = &caps->filter_width[i];

            if (caps->filter_width[i].width_hz >= width)
            {
                break;
            }
        }
    }

    if (selected_filter_width == NULL)
    {
        RETURNFUNC2(-RIG_EINVAL);
    }

    SNPRINTF(cmd, sizeof(cmd), "FW%04d", selected_filter_width->value);

    RETURNFUNC2(kenwood_transaction(rig, cmd, NULL, 0));
}

/*
 * kenwood_set_mode
 */
int kenwood_set_mode(RIG *rig, vfo_t vfo, rmode_t mode, pbwidth_t width)
{
    char c;
    char kmode;
    char buf[6];
    char data_mode = '0';
    char *data_cmd = "DA";
    int err;
    int datamode = 0;
    int needdata;
    struct kenwood_priv_data *priv = rig->state.priv;
    struct kenwood_priv_caps *caps = kenwood_caps(rig);

    rig_debug(RIG_DEBUG_VERBOSE,
              "%s called, vfo=%s, mode=%s, width=%d, curr_vfo=%s\n", __func__,
              rig_strvfo(vfo), rig_strrmode(mode), (int)width,
              rig_strvfo(rig->state.current_vfo));

    // we wont' set opposite VFO if the mode is the same as requested
    // setting VFOB mode requires split modifications which cause VFO flashing
    // this should generally work unless the user changes mode on VFOB
    // in which case VFOB won't get mode changed until restart
    if (priv->split && (priv->tx_vfo  & (RIG_VFO_B | RIG_VFO_SUB | RIG_VFO_SUB_A)))
    {
        if (priv->modeB == mode)
        {
            rig_debug(RIG_DEBUG_TRACE, "%s: VFOB mode already %s so ignoring request\n",
                      __func__, rig_strrmode(mode));
            return (RIG_OK);
        }
    }

    if (RIG_IS_TS590S || RIG_IS_TS590SG || RIG_IS_TS950S || RIG_IS_TS950SDX)
    {
        /* supports DATA sub modes */
        switch (mode)
        {
        case RIG_MODE_PKTUSB:
            data_mode = '1';
            mode = RIG_MODE_USB;
            break;

        case RIG_MODE_PKTLSB:
            data_mode = '1';
            mode = RIG_MODE_LSB;
            break;

        case RIG_MODE_PKTFM:
            data_mode = '1';
            mode = RIG_MODE_FM;
            break;

        default: break;
        }
    }

    if (priv->is_emulation || RIG_IS_HPSDR)
    {
        /* emulations like PowerSDR and SmartSDR normally hijack the
           RTTY modes for SSB-DATA AFSK modes */
        rig_debug(RIG_DEBUG_VERBOSE,
                  "%s: emulate=%d, HPSDR=%d, changing PKT mode to RTTY\n", __func__,
                  priv->is_emulation, RIG_IS_HPSDR);

        if (RIG_MODE_PKTLSB == mode) { mode = RIG_MODE_RTTY; }

        if (RIG_MODE_PKTUSB == mode) { mode = RIG_MODE_RTTYR; }
    }

    kmode = rmode2kenwood(mode, caps->mode_table);

    if (kmode < 0)
    {
        rig_debug(RIG_DEBUG_WARN, "%s: unsupported mode '%s'\n",
                  __func__, rig_strrmode(mode));
        RETURNFUNC2(-RIG_EINVAL);
    }

    if (kmode <= 9)
    {
        c = '0' + kmode;
    }
    else
    {
        c = 'A' + kmode - 10;
    }

    rig_debug(RIG_DEBUG_VERBOSE, "%s: kmode=%d, cmode=%c, datamode=%c\n", __func__,
              kmode, c, data_mode);

    if (RIG_IS_TS990S)
    {
        /* The TS990s has targetable read mode but can only set the mode
           of the current VFO :( So we need to toggle the operating VFO
           to set the "back" VFO mode. This is done here rather than not
           setting caps.targetable_vfo to not include
           RIG_TARGETABLE_MODE since the toggle is not required for
           reading the mode. */
        vfo_t curr_vfo;
        err = kenwood_get_vfo_main_sub(rig, &curr_vfo);

        if (err != RIG_OK) { RETURNFUNC2(err); }

        if (vfo != RIG_VFO_CURR && vfo != curr_vfo)
        {
            err = kenwood_set_vfo_main_sub(rig, vfo);

            if (err != RIG_OK) { RETURNFUNC2(err); }
        }

        SNPRINTF(buf, sizeof(buf), "OM0%c", c);  /* target vfo is ignored */
        err = kenwood_transaction(rig, buf, NULL, 0);

        if (err == RIG_OK && vfo != RIG_VFO_CURR && vfo != curr_vfo)
        {
            int err2 = kenwood_set_vfo_main_sub(rig, curr_vfo);

            if (err2 != RIG_OK) { RETURNFUNC2(err2); }
        }
    }
    else
    {
        pbwidth_t twidth;
        err = rig_get_mode(rig, vfo, &priv->curr_mode, &twidth);

    }

    if (err != RIG_OK) { RETURNFUNC2(err); }

    if (data_mode == '1' && (RIG_IS_TS590S || RIG_IS_TS590SG || RIG_IS_TS950S
                             || RIG_IS_TS950SDX))
    {
        if (RIG_IS_TS950S || RIG_IS_TS950SDX)
        {
            data_cmd = "DT";
        }

        datamode = 1;
    }

    rig_debug(RIG_DEBUG_VERBOSE,
              "%s: vfo=%s, curr_mode=%s, new_mode=%s, datamode=%d\n",
              __func__, rig_strvfo(vfo), rig_strrmode(priv->curr_mode), rig_strrmode(mode),
              datamode);

    // only change mode if needed
    if (priv->curr_mode != mode)
    {
        SNPRINTF(buf, sizeof(buf), "MD%c", c);
        err = kenwood_transaction(rig, buf, NULL, 0);
    }

    // determine if we need to set datamode on A or B
    needdata = 0;

    if (vfo == RIG_VFO_CURR)
    {
        HAMLIB_TRACE;
        vfo = rig->state.current_vfo;
    }

    if ((vfo & (RIG_VFO_A | RIG_VFO_MAIN)) && ((priv->datamodeA ==  0 && datamode)
            || (priv->datamodeA == 1 && !datamode)))
    {
        needdata = 1;
    }

    if ((vfo & (RIG_VFO_B | RIG_VFO_SUB)) && ((priv->datamodeB ==  0 && datamode)
            || (priv->datamodeB == 1 && !datamode)))
    {
        needdata = 1;
    }

    if (needdata)
    {
        /* supports DATA sub modes - see above */
        SNPRINTF(buf, sizeof(buf), "%s%c", data_cmd, data_mode);
        err = kenwood_transaction(rig, buf, NULL, 0);

        if (err != RIG_OK) { RETURNFUNC2(err); }
    }
    else if (datamode)
    {
        rig_debug(RIG_DEBUG_VERBOSE, "%s(%d): datamode set on %s not needed\n",
                  __func__, __LINE__, rig_strvfo(vfo));
    }

    if (RIG_PASSBAND_NOCHANGE == width) { RETURNFUNC2(RIG_OK); }

    if (RIG_IS_TS450S || RIG_IS_TS690S || RIG_IS_TS850 || RIG_IS_TS950S
            || RIG_IS_TS950SDX)
    {

        if (RIG_PASSBAND_NORMAL == width)
        {
            width = rig_passband_normal(rig, mode);
        }

        kenwood_set_filter(rig, width);
        /* non fatal */
    }
    else if (RIG_IS_TS480)
    {
        err = kenwood_set_filter_width(rig, mode, width);

        if (err != RIG_OK)
        {
            // Ignore errors as non-fatal
            rig_debug(RIG_DEBUG_ERR, "%s: error setting filter width, error: %d\n",
                      __func__, err);
        }
    }

    RETURNFUNC2(RIG_OK);
}

static int kenwood_get_filter(RIG *rig, pbwidth_t *width)
{
    int err, f, f1, f2;
    char buf[10];

    ENTERFUNC;

    if (!width)
    {
        RETURNFUNC(-RIG_EINVAL);
    }

    err = kenwood_safe_transaction(rig, "FL", buf, sizeof(buf), 8);

    if (err != RIG_OK)
    {
        RETURNFUNC(err);
    }

    f2 = atoi(&buf[5]);

    buf[5] = '\0';
    f1 = atoi(&buf[2]);

    if (f2 > f1)
    {
        f = f2;
    }
    else
    {
        f = f1;
    }

    switch (f)
    {
    case 2:
        *width = kHz(12);
        break;

    case 3:
    case 5:
        *width = kHz(6);
        break;

    case 7:
        *width = kHz(2.7);
        break;

    case 9:
        *width = Hz(500);
        break;

    case 10:
        *width = Hz(250);
        break;
    }

    RETURNFUNC(RIG_OK);
}

static int kenwood_get_filter_width(RIG *rig, rmode_t mode, pbwidth_t *width)
{
    struct kenwood_priv_caps *caps = kenwood_caps(rig);
    char ackbuf[20];
    int i;
    int retval;
    int filter_value;

    ENTERFUNC;

    if (caps->filter_width == NULL)
    {
        RETURNFUNC(-RIG_ENAVAIL);
    }

    retval = kenwood_safe_transaction(rig, "FW", ackbuf, sizeof(ackbuf), 6);

    if (retval != RIG_OK)
    {
        RETURNFUNC(retval);
    }

    sscanf(ackbuf, "FW%d", &filter_value);

    for (i = 0; caps->filter_width[i].value >= 0; i++)
    {
        if (caps->filter_width[i].modes & mode)
        {
            if (caps->filter_width[i].value == filter_value)
            {
                *width = caps->filter_width[i].width_hz;
                RETURNFUNC(RIG_OK);
            }
        }
    }

    if (filter_value >= 50) // then it's probably a custom filter width
    {
        *width = filter_value;
        return (RIG_OK);
    }

    RETURNFUNC(-RIG_EINVAL);
}

/*
 * kenwood_get_mode
 */
int kenwood_get_mode(RIG *rig, vfo_t vfo, rmode_t *mode, pbwidth_t *width)
{
    char cmd[4];
    char modebuf[10];
    int offs;
    int retval;
    int kmode;

    struct kenwood_priv_data *priv = rig->state.priv;
    struct kenwood_priv_caps *caps = kenwood_caps(rig);

    rig_debug(RIG_DEBUG_VERBOSE, "%s called, curr_vfo=%s\n", __func__,
              rig_strvfo(rig->state.current_vfo));

    if (!mode || !width)
    {
        RETURNFUNC2(-RIG_EINVAL);
    }

    /* for emulation do not read mode from VFOB as it is copy of VFOA */
    /* we avoid the VFO swapping most of the time this way */
    /* only need to get it if it has to be initialized */
    if (priv->curr_mode > 0 && priv->is_emulation && vfo == RIG_VFO_B)
    {
        rig->state.current_vfo = RIG_VFO_A;
        RETURNFUNC2(RIG_OK);
    }

    if (RIG_IS_TS990S)
    {
        char c;

        if (RIG_VFO_CURR == vfo || RIG_VFO_VFO == vfo)
        {
            if (RIG_OK != (retval = kenwood_get_vfo_main_sub(rig, &vfo)))
            {
                RETURNFUNC2(retval);
            }
        }

        switch (vfo)
        {
        case RIG_VFO_MAIN: c = '0'; break;

        case RIG_VFO_SUB: c = '1'; break;

        default:
            rig_debug(RIG_DEBUG_ERR, "%s: unsupported VFO %s\n", __func__, rig_strvfo(vfo));
            RETURNFUNC2(-RIG_EINVAL);
        }

        SNPRINTF(cmd, sizeof(cmd), "OM%c", c);
        offs = 3;
    }
    else
    {
        if (vfo == RIG_VFO_B
                && rig->caps->rig_model == RIG_MODEL_K4) // K4 new MD$ command for VFOB
        {
            SNPRINTF(cmd, sizeof(cmd), "MD$");
            offs = 3;
        }
        else
        {
            SNPRINTF(cmd, sizeof(cmd), "MD");
            offs = 2;
        }
    }

    retval = kenwood_safe_transaction(rig, cmd, modebuf, 6, offs + 1);

    if (retval != RIG_OK)
    {
        RETURNFUNC2(retval);
    }

    if (modebuf[offs] <= '9')
    {
        kmode = modebuf[offs] - '0';
    }
    else
    {
        kmode = modebuf[offs] - 'A' + 10;
    }

    *mode = kenwood2rmode(kmode, caps->mode_table);

    if (priv->is_emulation || RIG_IS_HPSDR)
    {
        /* emulations like PowerSDR and SmartSDR normally hijack the
           RTTY modes for SSB-DATA AFSK modes */
        rig_debug(RIG_DEBUG_VERBOSE,
                  "%s: emulate=%d, HPSDR=%d, changing RTTY mode to PKT\n", __func__,
                  priv->is_emulation, RIG_IS_HPSDR);

        if (RIG_MODE_RTTY == *mode) { *mode = RIG_MODE_PKTLSB; }

        if (RIG_MODE_RTTYR == *mode) { *mode = RIG_MODE_PKTUSB; }
    }

    if (RIG_IS_TS590S || RIG_IS_TS590SG || RIG_IS_TS950S || RIG_IS_TS950SDX)
    {
        /* supports DATA sub-modes */
        retval = kenwood_safe_transaction(rig, "DA", modebuf, 6, 3);

        if (retval != RIG_OK)
        {
            RETURNFUNC2(retval);
        }

        if ('1' == modebuf[2])
        {
            if (vfo == RIG_VFO_A) { priv->datamodeA = 1; }
            else { priv->datamodeB = 1; }

            switch (*mode)
            {
            case RIG_MODE_USB: *mode = RIG_MODE_PKTUSB; break;

            case RIG_MODE_LSB: *mode = RIG_MODE_PKTLSB; break;

            case RIG_MODE_FM: *mode = RIG_MODE_PKTFM; break;

            case RIG_MODE_AM: *mode = RIG_MODE_PKTAM; break;

            default: break;
            }
        }
        else
        {
            if (vfo == RIG_VFO_A) { priv->datamodeA = 0; }
            else { priv->datamodeB = 0; }
        }
    }

    if (RIG_IS_TS480)
    {
        retval = kenwood_get_filter_width(rig, *mode, width);

        if (retval != RIG_OK)
        {
            // Ignore errors as non-fatal
            rig_debug(RIG_DEBUG_ERR, "%s: error getting filter width, error: %d\n",
                      __func__, retval);
            *width = rig_passband_normal(rig, *mode);
        }
    }
    else
    {
        *width = rig_passband_normal(rig, *mode);
    }

    if (vfo == RIG_VFO_A) { priv->modeA = *mode; }
    else { priv->modeB = *mode; }

    RETURNFUNC2(RIG_OK);
}

/* This is used when the radio does not support MD; for mode reading */
int kenwood_get_mode_if(RIG *rig, vfo_t vfo, rmode_t *mode, pbwidth_t *width)
{
    int err;
    struct kenwood_priv_caps *caps = kenwood_caps(rig);
    struct kenwood_priv_data *priv = rig->state.priv;

    ENTERFUNC;

    if (!mode || !width)
    {
        RETURNFUNC(-RIG_EINVAL);
    }

    err = kenwood_get_if(rig);

    if (err != RIG_OK)
    {
        RETURNFUNC(err);
    }

    *mode = kenwood2rmode(priv->info[29] - '0', caps->mode_table);

    *width = rig_passband_normal(rig, *mode);

    if (RIG_IS_TS450S || RIG_IS_TS690S || RIG_IS_TS850 || RIG_IS_TS950S
            || RIG_IS_TS950SDX)
    {

        kenwood_get_filter(rig, width);
        /* non fatal */
    }

    RETURNFUNC(RIG_OK);
}

/* kenwood_get_micgain_minmax
 * Kenwood rigs have different micgain levels
 * This routine relies on the idea that setting the micgain
 * to 0 and 255 will result in the minimum and maximum values being set
 * If a rig doesn't behave this way then customize inside that rig's backend
*/
static int kenwood_get_micgain_minmax(RIG *rig, int *micgain_now,
                                      int *micgain_min,
                                      int *micgain_max,
                                      int restore)
{
    int expected_length = 18;
    int retval;
    char levelbuf[expected_length + 1];
    // read micgain_now, set 0, read micgain_min, set 255, read_micgain_max; set 0
    // we set back to 0 for safety and if restore is true we restore micgain_min
    // otherwise we expect calling routine to be setting new micgain level
    // we batch these commands together for speed
    char *cmd = "MG;MG000;MG;MG255;MG;MG000;";
    int n;
    struct rig_state *rs = &rig->state;

    ENTERFUNC;

    retval = write_block(&rs->rigport, (unsigned char *) cmd, strlen(cmd));

    if (retval != RIG_OK) { RETURNFUNC(retval); }

    retval = read_string(&rs->rigport, (unsigned char *) levelbuf, sizeof(levelbuf),
                         NULL, 0, 0, 1);

    rig_debug(RIG_DEBUG_TRACE, "%s: retval=%d\n", __func__, retval);

    if (retval != 18)
    {
        rig_debug(RIG_DEBUG_ERR, "%s: expected 19, got %d in '%s'\n", __func__, retval,
                  levelbuf);
        RETURNFUNC(-RIG_EPROTO);
    }

    n = sscanf(levelbuf, "MG%d;MG%d;MG%d", micgain_now, micgain_min, micgain_max);

    if (n != 3)
    {
        rig_debug(RIG_DEBUG_ERR, "%s: count not parse 3 values from '%s'\n", __func__,
                  levelbuf);
        RETURNFUNC(-RIG_EPROTO);
    }

    if (restore)
    {
        SNPRINTF(levelbuf, sizeof(levelbuf), "MG%03d;", *micgain_now);
        retval = kenwood_transaction(rig, levelbuf, NULL, 0);
        RETURNFUNC(retval);
    }

    rig_debug(RIG_DEBUG_TRACE, "%s: returning now=%d, min=%d, max=%d\n", __func__,
              *micgain_now, *micgain_min, *micgain_max);
    RETURNFUNC(RIG_OK);
}

/* kenwood_get_power_minmax
 * Kenwood rigs have different power levels by mode and by rig
 * This routine relies on the idea that setting the power
 * to 0 and 255 will result in the minimum and maximum values being set
 * If a rig doesn't behave this way then customize inside that rig's backend
*/
static int kenwood_get_power_minmax(RIG *rig, int *power_now, int *power_min,
                                    int *power_max,
                                    int restore)
{
    int max_length = 18;
    int expected_length;
    int retval;
    char levelbuf[max_length + 1];
    // read power_now, set 0, read power_min, set 255, read_power_max; set 0
    // we set back to 0 for safety and if restore is true we restore power_min
    // otherwise we expect calling routine to be setting new power level
    // we batch these commands together for speed
    char *cmd;
    int n;
    struct rig_state *rs = &rig->state;

    ENTERFUNC;

    switch (rig->caps->rig_model)
    {
    // TS480 can't handle the long command string
    // We can treat it like the TS890S
    case RIG_MODEL_TS480:

    // TS890S can't take power levels outside 5-100 and 5-25
    // So all we'll do is read power_now
    case RIG_MODEL_TS890S:
        rig->state.power_min = *power_min = 5;
        rig->state.power_max = *power_max = 100;

        if (rig->state.current_mode == RIG_MODE_AM) { *power_max = 50; }

        if (rig->state.current_freq >= 70)
        {
            rig->state.power_max = 50;

            if (rig->state.current_mode == RIG_MODE_AM) { *power_max = 13; }
        }


        cmd = "PC;";
        break;

    default:
        cmd = "PC;PC000;PC;PC255;PC;PC000;";
    }

    // Don't do this if PTT is on...don't want to max out power!!
    if (rig->state.cache.ptt == RIG_PTT_ON)
    {
        rig_debug(RIG_DEBUG_TRACE, "%s: ptt on so not checking min/max power levels\n",
                  __func__);
        // return the last values we got
        *power_now = rig->state.power_now;
        *power_min = rig->state.power_min;
        *power_max = rig->state.power_max;
        RETURNFUNC(RIG_OK);
    }

    retval = write_block(&rs->rigport, (unsigned char *) cmd, strlen(cmd));

    if (retval != RIG_OK) { RETURNFUNC(retval); }

    if (RIG_IS_TS890S || RIG_IS_TS480)
    {
        expected_length = 6;
    }
    else
    {
        expected_length = 18;
    }

    retval = read_string(&rs->rigport, (unsigned char *) levelbuf,
                         expected_length + 1,
                         NULL, 0, 0, 1);

    rig_debug(RIG_DEBUG_TRACE, "%s: retval=%d\n", __func__, retval);

    if (retval != expected_length)
    {
        rig_debug(RIG_DEBUG_ERR, "%s: expected %d, got %d in '%s'\n", __func__,
                  expected_length,
                  retval,
                  levelbuf);
        RETURNFUNC(-RIG_EPROTO);
    }

    if (RIG_IS_TS890S || RIG_IS_TS480)
    {
        n = sscanf(levelbuf, "PC%d;", power_now);

        if (n != 1)
        {
            rig_debug(RIG_DEBUG_ERR, "%s: count not parse 1 value from '%s'\n", __func__,
                      levelbuf);
            RETURNFUNC(-RIG_EPROTO);
        }
    }
    else
    {
        n = sscanf(levelbuf, "PC%d;PC%d;PC%d", power_now, power_min, power_max);

        if (n != 3)
        {
            rig_debug(RIG_DEBUG_ERR, "%s: count not parse 3 values from '%s'\n", __func__,
                      levelbuf);
            RETURNFUNC(-RIG_EPROTO);
        }

        if (restore) // only need to restore if 3-value cmd is done
        {
            SNPRINTF(levelbuf, sizeof(levelbuf), "PC%03d;", *power_now);
            retval = kenwood_transaction(rig, levelbuf, NULL, 0);
            RETURNFUNC(retval);
        }
    }

    rig_debug(RIG_DEBUG_TRACE, "%s: returning now=%d, min=%d, max=%d\n", __func__,
              *power_now, *power_min, *power_max);

    rig->state.power_now = *power_now;
    rig->state.power_min = *power_min;
    rig->state.power_max = *power_max;
    RETURNFUNC(RIG_OK);
}

static int kenwood_find_slope_filter_for_frequency(RIG *rig, vfo_t vfo,
        struct kenwood_slope_filter *filter, int frequency_hz, int *value)
{
    int retval;
    int i;
    struct kenwood_slope_filter *last_filter = NULL;
    freq_t freq;
    int cache_ms_freq;
    rmode_t mode;
    int cache_ms_mode;
    pbwidth_t width;
    int cache_ms_width;
    int data_mode_filter_active;

    if (filter == NULL)
    {
        return -RIG_ENAVAIL;
    }

    retval = rig_get_cache(rig, vfo, &freq, &cache_ms_freq, &mode, &cache_ms_mode,
                           &width, &cache_ms_width);

    if (retval != RIG_OK)
    {
        return -RIG_EINVAL;
    }

    retval = rig_get_ext_func(rig, vfo, TOK_FUNC_FILTER_WIDTH_DATA,
                              &data_mode_filter_active);

    if (retval != RIG_OK)
    {
        // Ignore errors, e.g. if the command is not supported
        data_mode_filter_active = 0;
    }

    for (i = 0; filter[i].value >= 0; i++)
    {
        if (filter[i].modes & mode
                && filter[i].data_mode_filter == data_mode_filter_active)
        {
            if (filter[i].frequency_hz >= frequency_hz)
            {
                *value = filter[i].value;
                return RIG_OK;
            }

            last_filter = &filter[i];
        }
    }

    if (last_filter != NULL)
    {
        *value = last_filter->value;
        return RIG_OK;
    }

    return -RIG_EINVAL;
}

static int kenwood_find_slope_filter_for_value(RIG *rig, vfo_t vfo,
        struct kenwood_slope_filter *filter, int value, int *frequency_hz)
{
    int retval;
    int i;
    freq_t freq;
    int cache_ms_freq;
    rmode_t mode;
    int cache_ms_mode;
    pbwidth_t width;
    int cache_ms_width;
    int data_mode_filter_active;

    if (filter == NULL)
    {
        return -RIG_ENAVAIL;
    }

    retval = rig_get_cache(rig, vfo, &freq, &cache_ms_freq, &mode, &cache_ms_mode,
                           &width, &cache_ms_width);

    if (retval != RIG_OK)
    {
        return -RIG_EINVAL;
    }

    retval = rig_get_ext_func(rig, vfo, TOK_FUNC_FILTER_WIDTH_DATA,
                              &data_mode_filter_active);

    if (retval != RIG_OK)
    {
        // Ignore errors, e.g. if the command is not supported
        data_mode_filter_active = 0;
    }

    for (i = 0; filter[i].value >= 0; i++)
    {
        if (filter[i].modes & mode
                && filter[i].data_mode_filter == data_mode_filter_active)
        {
            if (filter[i].value == value)
            {
                *frequency_hz = filter[i].frequency_hz;
                return RIG_OK;
            }
        }
    }

    return -RIG_EINVAL;
}

int kenwood_set_level(RIG *rig, vfo_t vfo, setting_t level, value_t val)
{
    char levelbuf[16];
    int i, kenwood_val;
    struct kenwood_priv_data *priv = rig->state.priv;
    struct kenwood_priv_caps *caps = kenwood_caps(rig);

    ENTERFUNC;

    if (RIG_LEVEL_IS_FLOAT(level))
    {
        if (val.f > 1.0) { RETURNFUNC(-RIG_EINVAL); }

        kenwood_val = val.f * 255;
    }
    else
    {
        kenwood_val = val.i;
    }

    switch (level)
    {
        int retval;

    case RIG_LEVEL_RFPOWER:
    {
        int power_now, power_min, power_max;
        // Power min/max can vary so we query to find them out every time
        retval = kenwood_get_power_minmax(rig, &power_now, &power_min, &power_max, 0);

        if (retval != RIG_OK) { RETURNFUNC(retval); }

        // https://github.com/Hamlib/Hamlib/issues/465
        kenwood_val = val.f * power_max;

        if (kenwood_val < power_min) { kenwood_val = power_min; }

        if (kenwood_val > power_max) { kenwood_val = power_max; }

        SNPRINTF(levelbuf, sizeof(levelbuf), "PC%03d", kenwood_val);
        break;
    }


    case RIG_LEVEL_AF:
    {
        int vfo_set = vfo == RIG_VFO_A || vfo == RIG_VFO_MAIN ? 0 : 1;

        // some rigs only recognize 0 for vfo_set
        // https://github.com/Hamlib/Hamlib/issues/304
        // This is now fixed for all rigs
        // https://github.com/Hamlib/Hamlib/issues/380
        // ag_format is determined in kenwood_get_level
        switch (priv->ag_format)
        {
        case 1:
            SNPRINTF(levelbuf, sizeof(levelbuf), "AG%03d", kenwood_val);
            break;

        case 2:
            SNPRINTF(levelbuf, sizeof(levelbuf), "AG0%03d", kenwood_val);
            break;

        case 3:
            SNPRINTF(levelbuf, sizeof(levelbuf), "AG%d%03d", vfo_set, kenwood_val);
            break;

        default:
            rig_debug(RIG_DEBUG_ERR, "%s: Unknown ag_format=%d\n", __func__,
                      priv->ag_format);
        }
    }
    break;

    case RIG_LEVEL_MICGAIN:
    {
        int micgain_now;

        if (priv->micgain_min == -1) // then we need to know our min/max
        {
            retval = kenwood_get_micgain_minmax(rig, &micgain_now, &priv->micgain_min,
                                                &priv->micgain_max, 0);

            if (retval != RIG_OK) { RETURNFUNC(retval); }
        }

        if (val.f > 1.0 || val.f < 0) { RETURNFUNC(-RIG_EINVAL); }

        // is micgain_min ever > 0 ??
        kenwood_val = val.f * (priv->micgain_max - priv->micgain_min) +
                      priv->micgain_min;
        SNPRINTF(levelbuf, sizeof(levelbuf), "MG%03d", kenwood_val);
        break;
    }

    case RIG_LEVEL_RF:

        /* XXX check level range */
        // KX2 and KX3 have range -190 to 250
        if (val.f > 1.0 || val.f < 0) { RETURNFUNC(-RIG_EINVAL); }

        kenwood_val = val.f * 255.0;
        SNPRINTF(levelbuf, sizeof(levelbuf), "RG%03d", kenwood_val);
        break;

    case RIG_LEVEL_SQL:
        /* Default to RX#0 */
        SNPRINTF(levelbuf, sizeof(levelbuf), "SQ0%03d", kenwood_val);
        break;

    case RIG_LEVEL_AGC:
        SNPRINTF(levelbuf, sizeof(levelbuf), "GT%03d", 84 * kenwood_val);
        break;

    case RIG_LEVEL_ATT:

        /* set the attenuator if a correct value is entered */
        if (val.i == 0)
        {
            SNPRINTF(levelbuf, sizeof(levelbuf), "RA00");
        }
        else
        {
            int foundit = 0;

            for (i = 0; i < HAMLIB_MAXDBLSTSIZ && rig->state.attenuator[i]; i++)
            {
                if (val.i == rig->state.attenuator[i])
                {
                    SNPRINTF(levelbuf, sizeof(levelbuf), "RA%02d", i + 1);
                    foundit = 1;
                    break;
                }
            }

            if (!foundit)
            {
                RETURNFUNC(-RIG_EINVAL);
            }
        }

        break;

    case RIG_LEVEL_PREAMP:

        /* set the preamp if a correct value is entered */
        if (val.i == 0)
        {
            SNPRINTF(levelbuf, sizeof(levelbuf), "PA0");
        }
        else
        {
            int foundit = 0;

            for (i = 0; i < HAMLIB_MAXDBLSTSIZ && rig->state.preamp[i]; i++)
            {
                if (val.i == rig->state.preamp[i])
                {
                    SNPRINTF(levelbuf, sizeof(levelbuf), "PA%01d", i + 1);
                    foundit = 1;
                    break;
                }
            }

            if (!foundit)
            {
                RETURNFUNC(-RIG_EINVAL);
            }
        }

        break;

    case RIG_LEVEL_SLOPE_HIGH:
        retval = kenwood_find_slope_filter_for_frequency(rig, vfo,
                 caps->slope_filter_high, val.i, &kenwood_val);

        if (retval != RIG_OK)
        {
            // Fall back to using raw values
            if (val.i > 20 || val.i < 0)
            {
                RETURNFUNC(-RIG_EINVAL);
            }

            kenwood_val = val.i;
        }

        SNPRINTF(levelbuf, sizeof(levelbuf), "SH%02d", kenwood_val);
        break;

    case RIG_LEVEL_SLOPE_LOW:
        retval = kenwood_find_slope_filter_for_frequency(rig, vfo,
                 caps->slope_filter_low, val.i, &kenwood_val);

        if (retval != RIG_OK)
        {
            // Fall back to using raw values
            if (val.i > 20 || val.i < 0)
            {
                RETURNFUNC(-RIG_EINVAL);
            }

            kenwood_val = val.i;
        }

        SNPRINTF(levelbuf, sizeof(levelbuf), "SL%02d", kenwood_val);
        break;

    case RIG_LEVEL_CWPITCH:
        if (val.i > 1000 || val.i < 400)
        {
            RETURNFUNC(-RIG_EINVAL);
        }

        SNPRINTF(levelbuf, sizeof(levelbuf), "PT%02d", (val.i / 50) - 8);
        break;

    case RIG_LEVEL_KEYSPD:
        if (val.i > 60 || val.i < 5)
        {
            RETURNFUNC(-RIG_EINVAL);
        }

        SNPRINTF(levelbuf, sizeof(levelbuf), "KS%03d", val.i);
        break;

    case RIG_LEVEL_COMP:
        if (RIG_IS_TS990S)
        {
            kenwood_val = val.f * 255.0f;
        }
        else
        {
            kenwood_val = val.f * 100.0f;
        }

        SNPRINTF(levelbuf, sizeof(levelbuf), "PL%03d%03d", kenwood_val, kenwood_val);
        break;

    case RIG_LEVEL_VOXDELAY:
        if (val.i > 30 || val.i < 0)
        {
            RETURNFUNC(-RIG_EINVAL);
        }

        // Raw value is in milliseconds
        SNPRINTF(levelbuf, sizeof(levelbuf), "VD%04d", val.i * 100);
        break;

    case RIG_LEVEL_VOXGAIN:
        kenwood_val = val.f * 9.0f;
        SNPRINTF(levelbuf, sizeof(levelbuf), "VG%03d", kenwood_val);
        break;

    case RIG_LEVEL_BKIN_DLYMS:
        if (val.i > 1000 || val.i < 0)
        {
            RETURNFUNC(-RIG_EINVAL);
        }

        SNPRINTF(levelbuf, sizeof(levelbuf), "SD%04d", val.i);
        break;

    default:
        rig_debug(RIG_DEBUG_ERR, "%s: unsupported set_level %s", __func__,
                  rig_strlevel(level));
        RETURNFUNC(-RIG_EINVAL);
    }

    RETURNFUNC(kenwood_transaction(rig, levelbuf, NULL, 0));
}

int get_kenwood_level(RIG *rig, const char *cmd, float *fval, int *ival)
{
    char lvlbuf[10];
    int retval;
    int lvl;
    int len = strlen(cmd);

    ENTERFUNC;

    if (!fval && !ival)
    {
        RETURNFUNC(-RIG_EINVAL);
    }

    retval = kenwood_safe_transaction(rig, cmd, lvlbuf, 10, len + 3);

    if (retval != RIG_OK)
    {
        RETURNFUNC(retval);
    }

    /* 000..255 */
    sscanf(lvlbuf + len, "%d", &lvl);

    if (ival) { *ival = lvl; } // raw value

    if (fval) { *fval = lvl / 255.0; } // our default scaling of 0-255

    RETURNFUNC(RIG_OK);
}


/*
 * kenwood_get_level
 */
int kenwood_get_level(RIG *rig, vfo_t vfo, setting_t level, value_t *val)
{
    char lvlbuf[KENWOOD_MAX_BUF_LEN];
    char *cmd;
    int retval;
    int lvl;
    int i, ret, agclevel, len, value;
    struct kenwood_priv_data *priv = rig->state.priv;
    struct kenwood_priv_caps *caps = kenwood_caps(rig);

    ENTERFUNC;

    if (!val)
    {
        RETURNFUNC(-RIG_EINVAL);
    }

    switch (level)
    {
        int power_now, power_min, power_max;
        int min_pitch, step_pitch;  /* Hz */

    case RIG_LEVEL_RAWSTR:
        if (RIG_IS_TS590S || RIG_IS_TS590SG)
        {
            cmd = "SM0";
            len = 3;
        }
        else
        {
            cmd = "SM";
            len = 2;
        }

        retval = kenwood_safe_transaction(rig, cmd, lvlbuf, 10, len + 4);

        if (retval != RIG_OK)
        {
            RETURNFUNC(retval);
        }

        /* XXX atoi ? */
        sscanf(lvlbuf + len, "%d", &val->i); /* rawstr */
        break;

    case RIG_LEVEL_STRENGTH:
        if (RIG_IS_TS590S || RIG_IS_TS590SG || RIG_IS_TS480)
        {
            cmd = "SM0";
            len = 3;
        }
        else
        {
            cmd = "SM";
            len = 2;
        }

        retval = kenwood_safe_transaction(rig, cmd, lvlbuf, 10, len + 4);

        if (retval != RIG_OK)
        {
            RETURNFUNC(retval);
        }

        sscanf(lvlbuf + len, "%d", &val->i); /* rawstr */

        if (rig->caps->str_cal.size)
        {
            val->i = (int) rig_raw2val(val->i, &rig->caps->str_cal);
        }
        else
        {
            val->i = (val->i * 4) - 54;
        }

        break;

    case RIG_LEVEL_ATT:
        retval = kenwood_safe_transaction(rig, "RA", lvlbuf, 50, 6);

        if (retval != RIG_OK)
        {
            RETURNFUNC(retval);
        }

        sscanf(lvlbuf + 2, "%d", &lvl);

        if (lvl == 0)
        {
            val->i = 0;
        }
        else
        {
            for (i = 0; i < lvl && i < HAMLIB_MAXDBLSTSIZ; i++)
            {
                if (rig->state.attenuator[i] == 0)
                {
                    rig_debug(RIG_DEBUG_ERR, "%s: "
                              "unexpected att level %d\n",
                              __func__, lvl);
                    RETURNFUNC(-RIG_EPROTO);
                }
            }

            if (i != lvl)
            {
                RETURNFUNC(-RIG_EINTERNAL);
            }

            val->i = rig->state.attenuator[i - 1];
        }

        break;

    case RIG_LEVEL_PREAMP:
        retval = kenwood_safe_transaction(rig, "PA", lvlbuf, 50, 3);

        if (retval != RIG_OK)
        {
            RETURNFUNC(retval);
        }

        if (lvlbuf[2] == '0')
        {
            val->i = 0;
        }
        else if (isdigit((int)lvlbuf[2]))
        {
            lvl = lvlbuf[2] - '0';

            for (i = 0; i < lvl && i < HAMLIB_MAXDBLSTSIZ; i++)
            {
                if (rig->state.preamp[i] == 0)
                {
                    rig_debug(RIG_DEBUG_ERR, "%s: "
                              "unexpected preamp level %d\n",
                              __func__, lvl);
                    RETURNFUNC(-RIG_EPROTO);
                }
            }

            if (i != lvl)
            {
                RETURNFUNC(-RIG_EINTERNAL);
            }

            val->i = rig->state.preamp[i - 1];
        }
        else
        {
            rig_debug(RIG_DEBUG_ERR, "%s: "
                      "unexpected preamp char '%c'\n",
                      __func__, lvlbuf[2]);
            RETURNFUNC(-RIG_EPROTO);
        }

        break;

    case RIG_LEVEL_RFPOWER:
        // Power min/max can vary so we query to find them out every time
        retval = kenwood_get_power_minmax(rig, &power_now, &power_min, &power_max, 1);

        if (retval != RIG_OK) { RETURNFUNC(retval); }

        power_min = 0; // our return scale is 0-max to match the input scale
        val->f = (power_now - power_min) / (float)(power_max - power_min);
        RETURNFUNC(RIG_OK);

    case RIG_LEVEL_AF:

        // first time through we'll determine the AG format
        // Can be "AG" "AG0" or "AG0/1"
        // This could be done by rig but easy enough to make it automagic
        if (priv->ag_format < 0)
        {
            int retry_save = rig->state.rigport.retry;
            rig->state.rigport.retry = 0;  // speed up this check so no retries
            rig_debug(RIG_DEBUG_TRACE, "%s: AF format check determination...\n", __func__);
            // Determine AG format
            // =-1 == Undetermine
            // 0 == Unknown
            // 1 == AG
            // 2 == AG0 (fixed VFO)
            // 3 == AG0/1 (with VFO arg)
            char buffer[KENWOOD_MAX_BUF_LEN];
            ret = kenwood_transaction(rig, "AG", buffer, sizeof(buffer));

            if (ret == RIG_OK)
            {
                priv->ag_format = 1;
            }
            else
            {
                ret = kenwood_transaction(rig, "AG1", buffer, sizeof(buffer));

                if (ret == RIG_OK)
                {
                    priv->ag_format = 3;
                }
                else
                {
                    ret = kenwood_transaction(rig, "AG0", buffer, sizeof(buffer));

                    if (ret == RIG_OK)
                    {
                        priv->ag_format = 2;
                    }
                    else
                    {
                        priv->ag_format = 0; // rats....can't figure it out
                    }
                }
            }

            rig->state.rigport.retry = retry_save;
        }

        rig_debug(RIG_DEBUG_TRACE, "%s: ag_format=%d\n", __func__, priv->ag_format);

        if (priv->ag_format == 0)
        {
            priv->ag_format = -1;  // we'll keep trying next time
            rig_debug(RIG_DEBUG_WARN, "%s: Unable to set AG format?\n", __func__);
            RETURNFUNC(RIG_OK);  // this is non-fatal for no))w
        }

        switch (priv->ag_format)
        {
        case 0:
            priv->ag_format = -1; // reset to try again
            RETURNFUNC(RIG_OK);
            break;

        case 1:
            RETURNFUNC(get_kenwood_level(rig, "AG", &val->f, NULL));
            break;

        case 2:
            RETURNFUNC(get_kenwood_level(rig, "AG0", &val->f, NULL));
            break;

        case 3:
            RETURNFUNC(get_kenwood_level(rig, vfo == RIG_VFO_MAIN ? "AG0" : "AG1", &val->f,
                                         NULL));
            break;

        default:
            rig_debug(RIG_DEBUG_WARN, "%s: Invalid ag_format=%d?\n", __func__,
                      priv->ag_format);
            RETURNFUNC(-RIG_EPROTO);
        }

    case RIG_LEVEL_RF:
        RETURNFUNC(get_kenwood_level(rig, "RG", &val->f, NULL));



    case RIG_LEVEL_MICGAIN:
    {
        int micgain_now;

        if (priv->micgain_min == -1) // then we need to know our min/max
        {
            retval = kenwood_get_micgain_minmax(rig, &micgain_now, &priv->micgain_min,
                                                &priv->micgain_max, 1);

            if (retval != RIG_OK) { RETURNFUNC(retval); }
        }

        rig_debug(RIG_DEBUG_TRACE, "%s: micgain_min=%d, micgain_max=%d\n", __func__,
                  priv->micgain_min, priv->micgain_max);

        ret = get_kenwood_level(rig, "MG", NULL, &val->i);

        if (ret != RIG_OK)
        {
            rig_debug(RIG_DEBUG_ERR, "%s: Error getting MICGAIN\n", __func__);
            RETURNFUNC(ret);
        }

        val->f = (val->i - priv->micgain_min) / (float)(priv->micgain_max -
                 priv->micgain_min);
        RETURNFUNC(RIG_OK);
    }

    case RIG_LEVEL_AGC:
        ret = get_kenwood_level(rig, "GT", NULL, &val->i);
        agclevel = val->i;

        if (agclevel == 0) { val->i = 0; }
        else if (agclevel < 85) { val->i = 1; }
        else if (agclevel < 170) { val->i = 2; }
        else if (agclevel <= 255) { val->i = 3; }

        RETURNFUNC(ret);

    case RIG_LEVEL_SLOPE_LOW:
        retval = kenwood_transaction(rig, "SL", lvlbuf, sizeof(lvlbuf));

        if (retval != RIG_OK)
        {
            RETURNFUNC(retval);
        }

        value = atoi(&lvlbuf[2]);

        retval = kenwood_find_slope_filter_for_value(rig, vfo, caps->slope_filter_low,
                 value, &val->i);

        if (retval != RIG_OK)
        {
            if (retval == -RIG_ENAVAIL)
            {
                // Fall back to using raw values
                val->i = value;
            }
            else
            {
                RETURNFUNC(retval);
            }
        }

        break;

    case RIG_LEVEL_SLOPE_HIGH:
        retval = kenwood_transaction(rig, "SH", lvlbuf, sizeof(lvlbuf));

        if (retval != RIG_OK)
        {
            RETURNFUNC(retval);
        }

        value = atoi(&lvlbuf[2]);

        retval = kenwood_find_slope_filter_for_value(rig, vfo, caps->slope_filter_high,
                 value, &val->i);

        if (retval != RIG_OK)
        {
            if (retval == -RIG_ENAVAIL)
            {
                // Fall back to using raw values
                val->i = value;
            }
            else
            {
                RETURNFUNC(retval);
            }
        }

        break;

    case RIG_LEVEL_CWPITCH:
        if (RIG_IS_TS890S)
        {
            len = 5;
            min_pitch = 300;
            step_pitch = 5;
        }
        else
        {
            len = 4;
            min_pitch = 400;
            step_pitch = 50;
        }

        retval = kenwood_safe_transaction(rig, "PT", lvlbuf, 50, len);

        if (retval != RIG_OK)
        {
            RETURNFUNC(retval);
        }

        sscanf(lvlbuf + 2, "%d", &val->i); /* 00 - 12 or 000 - 160 */
        val->i = (val->i * step_pitch) + min_pitch; /* 400 - 1000 or 300 - 1100
                             */
        break;

    case RIG_LEVEL_KEYSPD:
        retval = kenwood_safe_transaction(rig, "KS", lvlbuf, 50, 5);

        if (retval != RIG_OK)
        {
            RETURNFUNC(retval);
        }

        sscanf(lvlbuf + 2, "%d", &val->i);
        break;

    case RIG_LEVEL_COMP:
    {
        int raw_value;
        retval = kenwood_safe_transaction(rig, "PL", lvlbuf, 50, 8);

        if (retval != RIG_OK)
        {
            RETURNFUNC(retval);
        }

        sscanf(lvlbuf + 2, "%3d", &raw_value);

        if (RIG_IS_TS990S)
        {
            val->f = (float) raw_value / 255.0f;
        }
        else
        {
            val->f = (float) raw_value / 100.0f;
        }

        break;
    }

    case RIG_LEVEL_VOXDELAY:
    {
        int raw_value;
        retval = kenwood_safe_transaction(rig, "VD", lvlbuf, 50, 6);

        if (retval != RIG_OK)
        {
            RETURNFUNC(retval);
        }

        sscanf(lvlbuf + 2, "%d", &raw_value);

        // Value is in milliseconds
        val->i = raw_value / 100;
        break;
    }

    case RIG_LEVEL_VOXGAIN:
    {
        int raw_value;
        retval = kenwood_safe_transaction(rig, "VG", lvlbuf, 50, 5);

        if (retval != RIG_OK)
        {
            RETURNFUNC(retval);
        }

        sscanf(lvlbuf + 2, "%d", &raw_value);

        val->f = (float) raw_value / 9.0f;
        break;
    }

    case RIG_LEVEL_BKIN_DLYMS:
    {
        int raw_value;
        retval = kenwood_safe_transaction(rig, "SD", lvlbuf, 50, 6);

        if (retval != RIG_OK)
        {
            RETURNFUNC(retval);
        }

        sscanf(lvlbuf + 2, "%d", &raw_value);

        val->i = raw_value;
        break;
    }

    case RIG_LEVEL_IF:
    case RIG_LEVEL_APF:
    case RIG_LEVEL_NR:
    case RIG_LEVEL_PBT_IN:
    case RIG_LEVEL_PBT_OUT:
    case RIG_LEVEL_NOTCHF:
    case RIG_LEVEL_BKINDL:
    case RIG_LEVEL_BALANCE:
        RETURNFUNC(-RIG_ENIMPL);

    default:
        rig_debug(RIG_DEBUG_ERR, "%s: unsupported get_level %s", __func__,
                  rig_strlevel(level));
        RETURNFUNC(-RIG_EINVAL);
    }

    RETURNFUNC(RIG_OK);
}

int kenwood_set_func(RIG *rig, vfo_t vfo, setting_t func, int status)
{
    char buf[10]; /* longest cmd is GTxxx */

    ENTERFUNC;

    switch (func)
    {
    case RIG_FUNC_NB:
    case RIG_FUNC_NB2:

        /* newer Kenwoods have a second noise blanker */
        if (RIG_IS_TS890S)
        {
            switch (func)
            {
            case RIG_FUNC_NB:
                SNPRINTF(buf, sizeof(buf), "NB1%c", (status == 0) ? '0' : '1');
                break;

            case RIG_FUNC_NB2:
                SNPRINTF(buf, sizeof(buf), "NB2%c", (status == 0) ? '0' : '1');
                break;

            default:
                rig_debug(RIG_DEBUG_ERR, "%s: expected 0,1, or 2 and got %d\n", __func__,
                          status);
                RETURNFUNC(-RIG_EINVAL);
            }
        }
        else
        {
            SNPRINTF(buf, sizeof(buf), "NB%c", (status == 0) ? '0' : '1');
        }

        RETURNFUNC(kenwood_transaction(rig, buf, NULL, 0));

    case RIG_FUNC_ABM:
        SNPRINTF(buf, sizeof(buf), "AM%c", (status == 0) ? '0' : '1');
        RETURNFUNC(kenwood_transaction(rig, buf, NULL, 0));

    case RIG_FUNC_COMP:
        if (RIG_IS_TS890S)
        {
            SNPRINTF(buf, sizeof(buf), "PR0%c", (status == 0) ? '0' : '1');
        }
        else
        {
            SNPRINTF(buf, sizeof(buf), "PR%c", (status == 0) ? '0' : '1');
        }

        RETURNFUNC(kenwood_transaction(rig, buf, NULL, 0));

    case RIG_FUNC_TONE:
        SNPRINTF(buf, sizeof(buf), "TO%c", (status == 0) ? '0' : '1');
        RETURNFUNC(kenwood_transaction(rig, buf, NULL, 0));

    case RIG_FUNC_TSQL:
        SNPRINTF(buf, sizeof(buf), "CT%c", (status == 0) ? '0' : '1');
        RETURNFUNC(kenwood_transaction(rig, buf, NULL, 0));

    case RIG_FUNC_VOX:
        SNPRINTF(buf, sizeof(buf), "VX%c", (status == 0) ? '0' : '1');
        RETURNFUNC(kenwood_transaction(rig, buf, NULL, 0));

    case RIG_FUNC_FAGC:
        SNPRINTF(buf, sizeof(buf), "GT00%c", (status == 0) ? '4' : '2');
        RETURNFUNC(kenwood_transaction(rig, buf, NULL, 0));

    case RIG_FUNC_NR:
        if (RIG_IS_TS890S)
        {
            char c = '1';

            if (status == 2) { c = '2'; }

            SNPRINTF(buf, sizeof(buf), "NR%c", (status == 0) ? '0' : c);
        }
        else
        {
            SNPRINTF(buf, sizeof(buf), "NR%c", (status == 0) ? '0' : '1');
        }

        RETURNFUNC(kenwood_transaction(rig, buf, NULL, 0));

    case RIG_FUNC_BC:
        SNPRINTF(buf, sizeof(buf), "BC%c", (status == 0) ? '0' : '1');
        RETURNFUNC(kenwood_transaction(rig, buf, NULL, 0));

    case RIG_FUNC_BC2:
        SNPRINTF(buf, sizeof(buf), "BC%c", (status == 0) ? '0' : '2');
        RETURNFUNC(kenwood_transaction(rig, buf, NULL, 0));

    case RIG_FUNC_ANF:
        SNPRINTF(buf, sizeof(buf), "NT%c", (status == 0) ? '0' : '1');
        RETURNFUNC(kenwood_transaction(rig, buf, NULL, 0));

    case RIG_FUNC_LOCK:
        SNPRINTF(buf, sizeof(buf), "LK%c", (status == 0) ? '0' : '1');
        RETURNFUNC(kenwood_transaction(rig, buf, NULL, 0));

    case RIG_FUNC_AIP:
        SNPRINTF(buf, sizeof(buf), "MX%c", (status == 0) ? '0' : '1');
        RETURNFUNC(kenwood_transaction(rig, buf, NULL, 0));

    case RIG_FUNC_RIT:
        SNPRINTF(buf, sizeof(buf), "RT%c", (status == 0) ? '0' : '1');
        RETURNFUNC(kenwood_transaction(rig, buf, NULL, 0));

    case RIG_FUNC_XIT:
        SNPRINTF(buf, sizeof(buf), "XT%c", (status == 0) ? '0' : '1');
        RETURNFUNC(kenwood_transaction(rig, buf, NULL, 0));

    case RIG_FUNC_TUNER:
        SNPRINTF(buf, sizeof(buf), "AC1%c0", (status == 0) ? '0' : '1');
        RETURNFUNC(kenwood_transaction(rig, buf, NULL, 0));

    case RIG_FUNC_FBKIN:
        SNPRINTF(buf, sizeof(buf), "SD%04d", (status == 1) ? 0 : 50);
        RETURNFUNC(kenwood_transaction(rig, buf, NULL, 0));

    default:
        rig_debug(RIG_DEBUG_ERR, "Unsupported set_func %s", rig_strfunc(func));
        RETURNFUNC(-RIG_EINVAL);
    }

    RETURNFUNC(-RIG_EINVAL);
}

/*
 * works for any 'format 1' command or newer command like the TS890 has
 * as long as the return is a number 0-9
 * answer is always 4 bytes or 5 bytes: two or three byte command id, status and terminator
 */
int get_kenwood_func(RIG *rig, const char *cmd, int *status)
{
    int retval;
    char buf[10];
    int offset = 2;

    ENTERFUNC;

    if (!cmd || !status)
    {
        RETURNFUNC(-RIG_EINVAL);
    }

    if (strlen(cmd) == 3) { offset = 3; } // some commands are 3 letters

    retval = kenwood_safe_transaction(rig, cmd, buf, sizeof(buf), offset + 1);

    if (retval != RIG_OK)
    {
        RETURNFUNC(retval);
    }

    *status = buf[offset] - '0'; // just return whatever the rig returns

    RETURNFUNC(RIG_OK);
}

/*
 * kenwood_get_func
 */
int kenwood_get_func(RIG *rig, vfo_t vfo, setting_t func, int *status)
{
    char *cmd;
    char respbuf[20];
    int retval;
    int raw_value;

    ENTERFUNC;

    if (!status)
    {
        RETURNFUNC(-RIG_EINVAL);
    }

    switch (func)
    {
    case RIG_FUNC_FAGC:
        retval = kenwood_safe_transaction(rig, "GT", respbuf, 20, 5);

        if (retval != RIG_OK)
        {
            RETURNFUNC(retval);
        }

        *status = respbuf[4] != '4' ? 1 : 0;
        RETURNFUNC(RIG_OK);

    case RIG_FUNC_NB:
        cmd = "NB";

        if (RIG_IS_TS890S)
        {
            cmd = "NB1";
        }

        RETURNFUNC(get_kenwood_func(rig, cmd, status));

    case RIG_FUNC_NB2:
        RETURNFUNC(get_kenwood_func(rig, "NB2", status));

    case RIG_FUNC_ABM:
        RETURNFUNC(get_kenwood_func(rig, "AM", status));

    case RIG_FUNC_COMP:
        RETURNFUNC(get_kenwood_func(rig, "PR", status));

    case RIG_FUNC_TONE:
        RETURNFUNC(get_kenwood_func(rig, "TO", status));

    case RIG_FUNC_TSQL:
        RETURNFUNC(get_kenwood_func(rig, "CT", status));

    case RIG_FUNC_VOX:
        RETURNFUNC(get_kenwood_func(rig, "VX", status));

    case RIG_FUNC_NR:
        RETURNFUNC(get_kenwood_func(rig, "NR", status));

    /* FIXME on TS2000 */
    // Check for BC #1
    case RIG_FUNC_BC: // Most will return BC1 or BC0, if BC2 then BC1 is off
        retval = get_kenwood_func(rig, "BC", &raw_value);

        if (retval == RIG_OK)
        {
            *status = raw_value == 1 ? 1 : 0;
        }

        RETURNFUNC(retval);

    case RIG_FUNC_BC2: // TS-890 check Beat Cancel 2 we return boolean true/false
        retval = get_kenwood_func(rig, "BC", &raw_value);

        if (retval == RIG_OK)
        {
            *status = raw_value == 2 ? 1 : 0;
        }

        RETURNFUNC(retval);

    case RIG_FUNC_ANF:
        RETURNFUNC(get_kenwood_func(rig, "NT", status));

    case RIG_FUNC_LOCK:
        RETURNFUNC(get_kenwood_func(rig, "LK", status));

    case RIG_FUNC_AIP:
        RETURNFUNC(get_kenwood_func(rig, "MX", status));

    case RIG_FUNC_RIT:
        RETURNFUNC(get_kenwood_func(rig, "RT", status));

    case RIG_FUNC_XIT:
        RETURNFUNC(get_kenwood_func(rig, "XT", status));

    case RIG_FUNC_TUNER:
        retval = kenwood_safe_transaction(rig, "AC", respbuf, 20, 5);

        if (retval != RIG_OK)
        {
            RETURNFUNC(retval);
        }

        *status = respbuf[3] != '0' ? 1 : 0;
        RETURNFUNC(RIG_OK);

    case RIG_FUNC_FBKIN:
    {
        int raw_value;

        retval = kenwood_safe_transaction(rig, "SD", respbuf, 20, 6);

        if (retval != RIG_OK)
        {
            RETURNFUNC(retval);
        }

        sscanf(respbuf + 2, "%d", &raw_value);
        *status = (raw_value == 0) ? 1 : 0;
        RETURNFUNC(RIG_OK);
    }

    default:
        rig_debug(RIG_DEBUG_ERR, "Unsupported get_func %s", rig_strfunc(func));
        RETURNFUNC(-RIG_EINVAL);
    }

    RETURNFUNC(-RIG_EINVAL);
}

/*
 * kenwood_set_ctcss_tone
 * Assumes rig->caps->ctcss_list != NULL
 *
 * Warning! This is untested stuff! May work at least on TS-870S
 *  Please owners report to me <fillods@users.sourceforge.net>, thanks. --SF
 */
int kenwood_set_ctcss_tone(RIG *rig, vfo_t vfo, tone_t tone)
{
    const struct rig_caps *caps;
    char tonebuf[16];
    int i;

    ENTERFUNC;

    caps = rig->caps;

    for (i = 0; caps->ctcss_list[i] != 0; i++)
    {
        if (caps->ctcss_list[i] == tone)
        {
            break;
        }
    }

    if (caps->ctcss_list[i] != tone)
    {
        RETURNFUNC(-RIG_EINVAL);
    }

    /* TODO: replace menu no 57 by a define */
    SNPRINTF(tonebuf, sizeof(tonebuf), "EX%03d%04d", 57, i + 1);

    RETURNFUNC(kenwood_transaction(rig, tonebuf, NULL, 0));
}

int kenwood_set_ctcss_tone_tn(RIG *rig, vfo_t vfo, tone_t tone)
{
    const struct rig_caps *caps = rig->caps;
    char buf[16];
    int i;

    ENTERFUNC;

    for (i = 0; caps->ctcss_list[i] != 0; i++)
    {
        if (tone == caps->ctcss_list[i])
        {
            break;
        }
    }

    if (tone != caps->ctcss_list[i])
    {
        RETURNFUNC(-RIG_EINVAL);
    }

    if (RIG_IS_TS990S)
    {
        char c;

        if (RIG_VFO_CURR == vfo || RIG_VFO_VFO == vfo)
        {
            int err;

            if (RIG_OK != (err = kenwood_get_vfo_main_sub(rig, &vfo)))
            {
                RETURNFUNC(err);
            }
        }

        switch (vfo)
        {
        case RIG_VFO_MAIN: c = '0'; break;

        case RIG_VFO_SUB: c = '1'; break;

        default:
            rig_debug(RIG_DEBUG_ERR, "%s: unsupported VFO %s\n", __func__, rig_strvfo(vfo));
            RETURNFUNC(-RIG_EINVAL);
        }

        SNPRINTF(buf, sizeof(buf), "TN%c%02d", c, i + 1);
    }
    else
    {
        SNPRINTF(buf, sizeof(buf), "TN%02d", i + 1);
    }

    RETURNFUNC(kenwood_transaction(rig, buf, NULL, 0));
}

/*
 * kenwood_get_ctcss_tone
 * Assumes rig->state.priv != NULL
 */
int kenwood_get_ctcss_tone(RIG *rig, vfo_t vfo, tone_t *tone)
{
    struct kenwood_priv_data *priv = rig->state.priv;
    const struct rig_caps *caps;
    char tonebuf[3];
    int i, retval;
    unsigned int tone_idx;

    ENTERFUNC;

    if (!tone)
    {
        RETURNFUNC(-RIG_EINVAL);
    }

    caps = rig->caps;

    if (RIG_IS_TS990S)
    {
        char cmd[4];
        char buf[6];
        char c;

        if (RIG_VFO_CURR == vfo || RIG_VFO_VFO == vfo)
        {
            if (RIG_OK != (retval = kenwood_get_vfo_main_sub(rig, &vfo)))
            {
                RETURNFUNC(retval);
            }
        }

        switch (vfo)
        {
        case RIG_VFO_MAIN: c = '0'; break;

        case RIG_VFO_SUB: c = '1'; break;

        default:
            rig_debug(RIG_DEBUG_ERR, "%s: unsupported VFO %s\n", __func__, rig_strvfo(vfo));
            RETURNFUNC(-RIG_EINVAL);
        }

        SNPRINTF(cmd, sizeof(cmd), "TN%c", c);
        retval = kenwood_safe_transaction(rig, cmd, buf, sizeof(buf), 5);
        memcpy(tonebuf, &buf[3], 2);
    }
    else
    {
        retval = kenwood_get_if(rig);
        memcpy(tonebuf, &priv->info[34], 2);
    }

    if (retval != RIG_OK)
    {
        RETURNFUNC(retval);
    }

    tonebuf[2] = '\0';
    tone_idx = atoi(tonebuf);

    if (tone_idx == 0)
    {
        rig_debug(RIG_DEBUG_ERR, "%s: CTCSS tone is zero (%s)\n",
                  __func__, tonebuf);
        RETURNFUNC(-RIG_EPROTO);
    }

    /* check this tone exists. That's better than nothing. */
    for (i = 0; i < tone_idx; i++)
    {
        if (caps->ctcss_list[i] == 0)
        {
            rig_debug(RIG_DEBUG_ERR, "%s: CTCSS NG (%04u)\n",
                      __func__, tone_idx);
            RETURNFUNC(-RIG_EPROTO);
        }
    }

    *tone = caps->ctcss_list[tone_idx - 1];

    RETURNFUNC(RIG_OK);
}

int kenwood_set_ctcss_sql(RIG *rig, vfo_t vfo, tone_t tone)
{
    const struct rig_caps *caps = rig->caps;
    char buf[16];
    int i;

    ENTERFUNC;

    for (i = 0; caps->ctcss_list[i] != 0; i++)
    {
        if (tone == caps->ctcss_list[i])
        {
            break;
        }
    }

    if (tone != caps->ctcss_list[i])
    {
        RETURNFUNC(-RIG_EINVAL);
    }

    if (RIG_IS_TS990S)
    {
        char c;

        if (RIG_VFO_CURR == vfo || RIG_VFO_VFO == vfo)
        {
            int err;

            if (RIG_OK != (err = kenwood_get_vfo_main_sub(rig, &vfo)))
            {
                RETURNFUNC(err);
            }
        }

        switch (vfo)
        {
        case RIG_VFO_MAIN: c = '0'; break;

        case RIG_VFO_SUB: c = '1'; break;

        default:
            rig_debug(RIG_DEBUG_ERR, "%s: unsupported VFO %s\n", __func__, rig_strvfo(vfo));
            RETURNFUNC(-RIG_EINVAL);
        }

        SNPRINTF(buf, sizeof(buf), "CN%c%02d", c, i + 1);
    }
    else
    {
        SNPRINTF(buf, sizeof(buf), "CN%02d", i + 1);
    }

    RETURNFUNC(kenwood_transaction(rig, buf, NULL, 0));
}

int kenwood_get_ctcss_sql(RIG *rig, vfo_t vfo, tone_t *tone)
{
    const struct rig_caps *caps;
    char cmd[4];
    char tonebuf[6];
    int offs;
    int i, retval;
    unsigned int tone_idx;

    ENTERFUNC;

    if (!tone)
    {
        RETURNFUNC(-RIG_EINVAL);
    }

    caps = rig->caps;

    if (RIG_IS_TS990S)
    {
        char c;

        if (RIG_VFO_CURR == vfo || RIG_VFO_VFO == vfo)
        {
            if (RIG_OK != (retval = kenwood_get_vfo_main_sub(rig, &vfo)))
            {
                RETURNFUNC(retval);
            }
        }

        switch (vfo)
        {
        case RIG_VFO_MAIN: c = '0'; break;

        case RIG_VFO_SUB: c = '1'; break;

        default:
            rig_debug(RIG_DEBUG_ERR, "%s: unsupported VFO %s\n", __func__, rig_strvfo(vfo));
            RETURNFUNC(-RIG_EINVAL);
        }

        SNPRINTF(cmd, sizeof(cmd), "CN%c", c);
        offs = 3;
    }
    else
    {
        SNPRINTF(cmd, sizeof(cmd), "CT");
        offs = 2;
    }

    retval = kenwood_safe_transaction(rig, cmd, tonebuf, 6, offs + 2);

    if (retval != RIG_OK)
    {
        RETURNFUNC(retval);
    }

    tone_idx = atoi(tonebuf + offs);

    if (tone_idx == 0)
    {
        rig_debug(RIG_DEBUG_ERR, "%s: CTCSS is zero (%s)\n",
                  __func__, tonebuf);
        RETURNFUNC(-RIG_EPROTO);
    }

    /* check this tone exists. That's better than nothing. */
    for (i = 0; i < tone_idx; i++)
    {
        if (caps->ctcss_list[i] == 0)
        {
            rig_debug(RIG_DEBUG_ERR, "%s: CTCSS NG (%04u)\n",
                      __func__, tone_idx);
            RETURNFUNC(-RIG_EPROTO);
        }
    }

    *tone = caps->ctcss_list[tone_idx - 1];

    RETURNFUNC(RIG_OK);
}


/*
 * set the aerial/antenna to use
 */
int kenwood_set_ant(RIG *rig, vfo_t vfo, ant_t ant, value_t option)
{
    char cmd[8];
    char a;

    ENTERFUNC;

    switch (ant)
    {
    case RIG_ANT_1: a = '1'; break;

    case RIG_ANT_2: a = '2'; break;

    case RIG_ANT_3: a = '3'; break;

    case RIG_ANT_4: a = '4'; break;

    default:
        RETURNFUNC(-RIG_EINVAL);
    }

    if (RIG_IS_TS990S)
    {
        char c;

        if (RIG_VFO_CURR == vfo || RIG_VFO_VFO == vfo)
        {
            int err;

            if (RIG_OK != (err = kenwood_get_vfo_main_sub(rig, &vfo)))
            {
                RETURNFUNC(err);
            }
        }

        switch (vfo)
        {
        case RIG_VFO_MAIN: c = '0'; break;

        case RIG_VFO_SUB: c = '1'; break;

        default:
            rig_debug(RIG_DEBUG_ERR, "%s: unsupported VFO %s\n", __func__, rig_strvfo(vfo));
            RETURNFUNC(-RIG_EINVAL);
        }

        SNPRINTF(cmd, sizeof(cmd), "AN0%c%c99", c, a);
    }
    else
    {
        SNPRINTF(cmd, sizeof(cmd), "AN%c", a);
    }

    RETURNFUNC(kenwood_transaction(rig, cmd, NULL, 0));
}

int kenwood_set_ant_no_ack(RIG *rig, vfo_t vfo, ant_t ant, value_t option)
{
    const char *cmd;

    ENTERFUNC;

    switch (ant)
    {
    case RIG_ANT_1:
        cmd = "AN1";
        break;

    case RIG_ANT_2:
        cmd = "AN2";
        break;

    case RIG_ANT_3:
        cmd = "AN3";
        break;

    case RIG_ANT_4:
        cmd = "AN4";
        break;

    default:
        RETURNFUNC(-RIG_EINVAL);
    }

    RETURNFUNC(kenwood_transaction(rig, cmd, NULL, 0));
}

/*
 * get the aerial/antenna in use
 */
int kenwood_get_ant(RIG *rig, vfo_t vfo, ant_t dummy, value_t *option,
                    ant_t *ant_curr, ant_t *ant_tx, ant_t *ant_rx)
{
    char ackbuf[8];
    int offs;
    int retval;

    ENTERFUNC;

    if (!ant_curr)
    {
        RETURNFUNC(-RIG_EINVAL);
    }

    if (RIG_IS_TS990S)
    {
        retval = kenwood_safe_transaction(rig, "AN0", ackbuf, sizeof(ackbuf), 7);
        offs = 4;
    }
    else
    {
        retval = kenwood_safe_transaction(rig, "AN", ackbuf, sizeof(ackbuf), 3);
        offs = 2;
    }

    if (retval != RIG_OK)
    {
        RETURNFUNC(retval);
    }

    if (ackbuf[offs] < '1' || ackbuf[offs] > '9')
    {
        RETURNFUNC(-RIG_EPROTO);
    }

    *ant_curr = RIG_ANT_N(ackbuf[offs] - '1');

    /* XXX check that the returned antenna is valid for the current rig */

    RETURNFUNC(RIG_OK);
}

/*
 * kenwood_get_ptt
 */
int kenwood_get_ptt(RIG *rig, vfo_t vfo, ptt_t *ptt)
{
    struct kenwood_priv_data *priv = rig->state.priv;
    int retval;

    ENTERFUNC;

    if (!ptt)
    {
        RETURNFUNC(-RIG_EINVAL);
    }

    retval = kenwood_get_if(rig);

    if (retval != RIG_OK)
    {
        RETURNFUNC(retval);
    }

    *ptt = priv->info[28] == '0' ? RIG_PTT_OFF : RIG_PTT_ON;

    RETURNFUNC(RIG_OK);
}

int kenwood_set_ptt(RIG *rig, vfo_t vfo, ptt_t ptt)
{
    const char *ptt_cmd;

    ENTERFUNC;
    rig_debug(RIG_DEBUG_VERBOSE, "%s: ptt=%d\n", __func__, ptt);

    switch (ptt)
    {
    case RIG_PTT_ON:      ptt_cmd = "TX"; break;

    case RIG_PTT_ON_MIC:  ptt_cmd = "TX0"; break;

    case RIG_PTT_ON_DATA: ptt_cmd = "TX1"; break;

    case RIG_PTT_OFF: ptt_cmd = "RX"; break;

    default: RETURNFUNC(-RIG_EINVAL);
    }

    int retval = kenwood_transaction(rig, ptt_cmd, NULL, 0);

    if (ptt == RIG_PTT_OFF) { hl_usleep(100 * 1000); } // a little time for PTT to turn off

    RETURNFUNC(retval);
}

int kenwood_set_ptt_safe(RIG *rig, vfo_t vfo, ptt_t ptt)
{
    int err;
    ptt_t current_ptt;

    ENTERFUNC;

    err = kenwood_get_ptt(rig, vfo, &current_ptt);

    if (err != RIG_OK)
    {
        RETURNFUNC(err);
    }

    if (current_ptt == ptt)
    {
        RETURNFUNC(RIG_OK);
    }

    RETURNFUNC(kenwood_transaction(rig,
                                   (ptt == RIG_PTT_ON) ? "TX" : "RX", NULL, 0));
}


/*
 * kenwood_get_dcd
 */
int kenwood_get_dcd(RIG *rig, vfo_t vfo, dcd_t *dcd)
{
    char busybuf[10];
    int retval;
    int expected;
    int offs;

    ENTERFUNC;

    if (!dcd)
    {
        RETURNFUNC(-RIG_EINVAL);
    }

    if (RIG_IS_TS480 || RIG_IS_TS590S || RIG_IS_TS590SG || RIG_IS_TS990S
            || RIG_IS_TS2000)
    {
        expected = 4;
    }
    else
    {
        expected = 3;
    }

    retval = kenwood_safe_transaction(rig, "BY", busybuf, 10, expected);

    if (retval != RIG_OK)
    {
        RETURNFUNC(retval);
    }

    if ((RIG_IS_TS990S && RIG_VFO_SUB == vfo) || (RIG_IS_TS2000
            && RIG_VFO_SUB == vfo))
    {
        offs = 3;
    }
    else
    {
        offs = 2;
    }

    *dcd = (busybuf[offs] == '1') ? RIG_DCD_ON : RIG_DCD_OFF;

    RETURNFUNC(RIG_OK);
}

/*
 * kenwood_set_trn
 */
int kenwood_set_trn(RIG *rig, int trn)
{
    char buf[5];
    ENTERFUNC;

    switch (rig->caps->rig_model)
    {
    case RIG_MODEL_POWERSDR: // powersdr doesn't have AI command
        RETURNFUNC(-RIG_ENAVAIL);

    case RIG_MODEL_TS990S:
        RETURNFUNC(kenwood_transaction(rig, (trn == RIG_TRN_RIG) ? "AI2" : "AI0", NULL,
                                       0));

    case RIG_MODEL_THD7A:
    case RIG_MODEL_THD74:
        RETURNFUNC(kenwood_transaction(rig, (trn == RIG_TRN_RIG) ? "AI 1" : "AI 0", buf,
                                       sizeof buf));

    default:
        RETURNFUNC(kenwood_transaction(rig, (trn == RIG_TRN_RIG) ? "AI1" : "AI0", NULL,
                                       0));
    }
}

/*
 * kenwood_get_trn
 */
int kenwood_get_trn(RIG *rig, int *trn)
{
    char trnbuf[6];
    int retval;

    ENTERFUNC;

    if (!trn)
    {
        RETURNFUNC(-RIG_EINVAL);
    }

    /* these rigs only have AI[0|1] set commands and no AI query */
    if (RIG_IS_TS450S || RIG_IS_TS690S || RIG_IS_TS790 || RIG_IS_TS850
            || RIG_IS_TS950S || RIG_IS_TS950SDX || RIG_IS_POWERSDR)
    {
        RETURNFUNC(-RIG_ENAVAIL);
    }

    if (RIG_IS_THD74 || RIG_IS_THD7A)
    {
        retval = kenwood_safe_transaction(rig, "AI", trnbuf, 6, 4);
    }
    else
    {
        retval = kenwood_safe_transaction(rig, "AI", trnbuf, 6, 3);
    }

    if (retval != RIG_OK)
    {
        RETURNFUNC(retval);
    }

    if (RIG_IS_THD74 || RIG_IS_THD7A)
    {
        *trn = trnbuf[3] != '0' ? RIG_TRN_RIG : RIG_TRN_OFF;
    }
    else
    {
        *trn = trnbuf[2] != '0' ? RIG_TRN_RIG : RIG_TRN_OFF;
    }

    RETURNFUNC(RIG_OK);
}

/*
 * kenwood_set_powerstat
 */
int kenwood_set_powerstat(RIG *rig, powerstat_t status)
{
    int retval = kenwood_transaction(rig,
                                     (status == RIG_POWER_ON) ? ";;;;PS1;" : "PS0",
                                     NULL, 0);
    int i = 0;
    int retry_save = rig->state.rigport.retry;

    rig_debug(RIG_DEBUG_VERBOSE, "%s called status=%d\n", __func__, status);

    rig->state.rigport.retry = 0;

    if (status == RIG_POWER_ON) // wait for wakeup only
    {
        for (i = 0; i < 8; ++i) // up to ~10 seconds including the timeouts
        {
            freq_t freq;
            sleep(1);
            retval = rig_get_freq(rig, RIG_VFO_A, &freq);

            if (retval == RIG_OK)
            {
                rig->state.rigport.retry = retry_save;
                RETURNFUNC2(retval);
            }

            rig_debug(RIG_DEBUG_TRACE, "%s: Wait #%d for power up\n", __func__, i + 1);
        }
    }

    rig->state.rigport.retry = retry_save;

    if (i == 9)
    {
        rig_debug(RIG_DEBUG_TRACE, "%s: timeout waiting for powerup, try %d\n",
                  __func__,
                  i + 1);
        retval = -RIG_ETIMEOUT;
    }

    RETURNFUNC2(retval);
}

/*
 * kenwood_get_powerstat
 */
int kenwood_get_powerstat(RIG *rig, powerstat_t *status)
{
    char pwrbuf[6];
    int retval;
    struct kenwood_priv_data *priv = rig->state.priv;

    ENTERFUNC;

    if (!priv->has_ps)
    {
        *status = RIG_POWER_ON;
        RETURNFUNC(RIG_OK); // fake the OK return for these rigs
    }

    if (!status)
    {
        RETURNFUNC(-RIG_EINVAL);
    }

    retval = kenwood_safe_transaction(rig, "PS", pwrbuf, 6, 3);

    if (retval != RIG_OK)
    {
        RETURNFUNC(retval);
    }

    *status = pwrbuf[2] == '0' ? RIG_POWER_OFF : RIG_POWER_ON;

    RETURNFUNC(RIG_OK);
}

/*
 * kenwood_reset
 */
int kenwood_reset(RIG *rig, reset_t reset)
{
    char rstbuf[6];
    char rst;

    ENTERFUNC;

    if (RIG_IS_TS990S)
    {
        switch (reset)
        {
        case RIG_RESET_SOFT: rst = '4'; break;

        case RIG_RESET_VFO: rst = '3'; break;

        case RIG_RESET_MCALL: rst = '2'; break;

        case RIG_RESET_MASTER: rst = '5'; break;

        default:
            rig_debug(RIG_DEBUG_ERR, "%s: unsupported reset %d\n",
                      __func__, reset);
            RETURNFUNC(-RIG_EINVAL);
        }
    }
    else
    {
        switch (reset)
        {
        case RIG_RESET_VFO: rst = '1'; break;

        case RIG_RESET_MASTER: rst = '2'; break;

        default:
            rig_debug(RIG_DEBUG_ERR, "%s: unsupported reset %d\n",
                      __func__, reset);
            RETURNFUNC(-RIG_EINVAL);
        }
    }

    SNPRINTF(rstbuf, sizeof(rstbuf), "SR%c", rst);

    /* this command has no answer */
    RETURNFUNC(kenwood_transaction(rig, rstbuf, NULL, 0));
}

/*
 * kenwood_send_morse
 */
int kenwood_send_morse(RIG *rig, vfo_t vfo, const char *msg)
{
    char morsebuf[40], m2[30];
    int msg_len, retval, i;
    const char *p;

    ENTERFUNC;

    if (!msg)
    {
        RETURNFUNC(-RIG_EINVAL);
    }

    p = msg;
    msg_len = strlen(msg);

    while (msg_len > 0)
    {
        int buff_len;

        /*
         * Check with "KY" if char buffer is available.
         * if not, sleep.
         */
        for (;;)
        {
            retval = kenwood_transaction(rig, "KY;", m2, 4);

            if (retval != RIG_OK)
            {
                RETURNFUNC(retval);
            }

            /*
             * If answer is "KY0;", there is space in buffer and we can proceed.
             * If answer is "KY1;", we have to wait a while
             * If answer is "KY2;", there is space in buffer and we aren't sending so we can proceed.
             * If answer is something else, return with error to prevent infinite loops
             */
            if (!strncmp(m2, "KY0", 3)) { break; }

            if (!strncmp(m2, "KY2", 3)) { break; }

            if (!strncmp(m2, "KY1", 3)) { hl_usleep(500000); }
            else { RETURNFUNC(-RIG_EINVAL); }
        }

        buff_len = msg_len > 24 ? 24 : msg_len;

        strncpy(m2, p, 24);
        m2[24] = '\0';

        /*
         * Make the total message segments 28 characters
         * in length because some Kenwoods demand it.
         * 0x20 fills in the message end.
         * Some rigs don't need the fill
         */
        switch (rig->caps->rig_model)
        {
        case RIG_MODEL_K3: // probably a lot more rigs need to go here
        case RIG_MODEL_K3S:
        case RIG_MODEL_KX2:
        case RIG_MODEL_KX3:
            SNPRINTF(morsebuf, sizeof(morsebuf), "KY %s", m2);
            break;

        default:
            /* the command must consist of 28 bytes 0x20 padded */
            SNPRINTF(morsebuf, sizeof(morsebuf), "KY %-24s", m2);

            for (i = strlen(morsebuf) - 1; i > 0 && morsebuf[i] == ' '; --i)
            {
                morsebuf[i] = 0x20;
            }
        }

        retval = kenwood_transaction(rig, morsebuf, NULL, 0);

        if (retval != RIG_OK)
        {
            RETURNFUNC(retval);
        }

        msg_len -= buff_len;
        p += buff_len;
    }

    RETURNFUNC(RIG_OK);
}

/*
 * kenwood_vfo_op
 */
int kenwood_vfo_op(RIG *rig, vfo_t vfo, vfo_op_t op)
{
    ENTERFUNC;

    switch (op)
    {
    case RIG_OP_UP:
        RETURNFUNC(kenwood_transaction(rig, "UP", NULL, 0));

    case RIG_OP_DOWN:
        RETURNFUNC(kenwood_transaction(rig, "DN", NULL, 0));

    case RIG_OP_BAND_UP:
        RETURNFUNC(kenwood_transaction(rig, "BU", NULL, 0));

    case RIG_OP_BAND_DOWN:
        RETURNFUNC(kenwood_transaction(rig, "BD", NULL, 0));

    case RIG_OP_TUNE:
        RETURNFUNC(kenwood_transaction(rig, "AC111", NULL, 0));

    case RIG_OP_CPY:
        RETURNFUNC(kenwood_transaction(rig, "VV", NULL, 0));

    default:
        rig_debug(RIG_DEBUG_ERR, "%s: unsupported op %#x\n",
                  __func__, op);
        RETURNFUNC(-RIG_EINVAL);
    }
}

/*
 * kenwood_set_mem
 */
int kenwood_set_mem(RIG *rig, vfo_t vfo, int ch)
{
    char buf[7];

    ENTERFUNC;

    if (RIG_IS_TS990S)
    {
        char c;

        if (RIG_VFO_CURR == vfo || RIG_VFO_VFO == vfo)
        {
            int err;

            if (RIG_OK != (err = kenwood_get_vfo_main_sub(rig, &vfo)))
            {
                RETURNFUNC(err);
            }
        }

        switch (vfo)
        {
        case RIG_VFO_MAIN: c = '0'; break;

        case RIG_VFO_SUB: c = '1'; break;

        default:
            rig_debug(RIG_DEBUG_ERR, "%s: unsupported VFO %s\n", __func__, rig_strvfo(vfo));
            RETURNFUNC(-RIG_EINVAL);
        }

        SNPRINTF(buf, sizeof(buf), "MN%c%03d", c, ch);
    }
    else
    {
        /*
         * "MCbmm;"
         * where b is the bank number, mm the memory number.
         * b can be a space
         */
        SNPRINTF(buf, sizeof(buf), "MC %02d", ch);
    }

    RETURNFUNC(kenwood_transaction(rig, buf, NULL, 0));
}

/*
 * kenwood_get_mem
 */
int kenwood_get_mem(RIG *rig, vfo_t vfo, int *ch)
{
    char cmd[4];
    char membuf[10];
    int offs;
    int retval;

    ENTERFUNC;

    if (!ch)
    {
        RETURNFUNC(-RIG_EINVAL);
    }

    if (RIG_IS_TS990S)
    {
        char c;

        if (RIG_VFO_CURR == vfo || RIG_VFO_VFO == vfo)
        {
            if (RIG_OK != (retval = kenwood_get_vfo_main_sub(rig, &vfo)))
            {
                RETURNFUNC(retval);
            }
        }

        switch (vfo)
        {
        case RIG_VFO_MAIN: c = '0'; break;

        case RIG_VFO_SUB: c = '1'; break;

        default:
            rig_debug(RIG_DEBUG_ERR, "%s: unsupported VFO %s\n", __func__, rig_strvfo(vfo));
            RETURNFUNC(-RIG_EINVAL);
        }

        SNPRINTF(cmd, sizeof(cmd), "MN%c", c);
        offs = 3;
    }
    else
    {
        /*
         * "MCbmm;"
         * where b is the bank number, mm the memory number.
         * b can be a space
         */
        SNPRINTF(cmd, sizeof(cmd), "MC");
        offs = 2;
    }

    retval = kenwood_safe_transaction(rig, cmd, membuf, sizeof(membuf), 3 + offs);

    if (retval != RIG_OK)
    {
        RETURNFUNC(retval);
    }

    *ch = atoi(membuf + offs);

    RETURNFUNC(RIG_OK);
}

int kenwood_get_mem_if(RIG *rig, vfo_t vfo, int *ch)
{
    int err;
    char buf[4];
    struct kenwood_priv_data *priv = rig->state.priv;

    ENTERFUNC;

    if (!ch)
    {
        RETURNFUNC(-RIG_EINVAL);
    }

    err = kenwood_get_if(rig);

    if (err != RIG_OK)
    {
        RETURNFUNC(err);
    }

    memcpy(buf, &priv->info[26], 2);
    buf[2] = '\0';

    *ch = atoi(buf);

    RETURNFUNC(RIG_OK);
}

int kenwood_get_channel(RIG *rig, vfo_t vfo, channel_t *chan, int read_only)
{
    int err;
    char buf[26];
    char cmd[8];
    char bank = ' ';
    struct kenwood_priv_caps *caps = kenwood_caps(rig);

    ENTERFUNC;

    if (!chan)
    {
        RETURNFUNC(-RIG_EINVAL);
    }

    /* put channel num in the command string */

    if (RIG_IS_TS940)
    {
        bank = '0' + chan->bank_num;
    }

    SNPRINTF(cmd, sizeof(cmd), "MR0%c%02d", bank, chan->channel_num);

    err = kenwood_safe_transaction(rig, cmd, buf, 26, 23);

    if (err != RIG_OK)
    {
        RETURNFUNC(err);
    }

    memset(chan, 0x00, sizeof(channel_t));

    chan->vfo = RIG_VFO_VFO;

    /* MR0 1700005890000510   ;
     * MRsbccfffffffffffMLTtt ;
     */

    /* parse from right to left */

    /* XXX based on the available documentation, there is no command
     * to read out the filters of a given memory channel. The rig, however,
     * stores this information.
     */

    if (buf[19] == '0' || buf[19] == ' ')
    {
        chan->ctcss_tone = 0;
    }
    else
    {
        buf[22] = '\0';

        if (rig->caps->ctcss_list)
        {
            chan->ctcss_tone = rig->caps->ctcss_list[atoi(&buf[20])];
        }
    }

    /* memory lockout */
    if (buf[18] == '1')
    {
        chan->flags |= RIG_CHFLAG_SKIP;
    }

    chan->mode = kenwood2rmode(buf[17] - '0', caps->mode_table);

    buf[17] = '\0';
    chan->freq = atoi(&buf[6]);

    if (chan->freq == RIG_FREQ_NONE)
    {
        RETURNFUNC(-RIG_ENAVAIL);
    }

    buf[6] = '\0';
    chan->channel_num = atoi(&buf[4]);

    if (buf[3] >= '0' && buf[3] <= '9')
    {
        chan->bank_num = buf[3] - '0';
    }

    /* split freq */
    cmd[2] = '1';
    err = kenwood_safe_transaction(rig, cmd, buf, 26, 23);

    if (err != RIG_OK)
    {
        RETURNFUNC(err);
    }

    chan->tx_mode = kenwood2rmode(buf[17] - '0', caps->mode_table);

    buf[17] = '\0';
    chan->tx_freq = atoi(&buf[6]);

    if (chan->freq == chan->tx_freq)
    {
        chan->tx_freq = RIG_FREQ_NONE;
        chan->tx_mode = RIG_MODE_NONE;
        chan->split = RIG_SPLIT_OFF;
    }
    else
    {
        chan->split = RIG_SPLIT_ON;
    }

    if (!read_only)
    {
        // Set rig to channel values
        rig_debug(RIG_DEBUG_ERR,
                  "%s: please contact hamlib mailing list to implement this\n", __func__);
        rig_debug(RIG_DEBUG_ERR,
                  "%s: need to know if rig updates when channel read or not\n", __func__);
        RETURNFUNC(-RIG_ENIMPL);
    }

    RETURNFUNC(RIG_OK);
}

int kenwood_set_channel(RIG *rig, vfo_t vfo, const channel_t *chan)
{
    char buf[128];
    char mode, tx_mode = 0;
    char bank = ' ';
    int err;
    int tone = 0;
    struct kenwood_priv_caps *caps = kenwood_caps(rig);

    ENTERFUNC;

    if (!chan)
    {
        RETURNFUNC(-RIG_EINVAL);
    }

    mode = rmode2kenwood(chan->mode, caps->mode_table);

    if (mode < 0)
    {
        rig_debug(RIG_DEBUG_ERR, "%s: unsupported mode '%s'\n",
                  __func__, rig_strrmode(chan->mode));
        RETURNFUNC(-RIG_EINVAL);
    }

    if (chan->split == RIG_SPLIT_ON)
    {
        tx_mode = rmode2kenwood(chan->tx_mode, caps->mode_table);

        if (tx_mode < 0)
        {
            rig_debug(RIG_DEBUG_ERR, "%s: unsupported mode '%s'\n",
                      __func__, rig_strrmode(chan->tx_mode));
            RETURNFUNC(-RIG_EINVAL);
        }

    }

    /* find tone */
    if (chan->ctcss_tone)
    {

        for (tone = 0; rig->caps->ctcss_list[tone] != 0; tone++)
        {
            if (chan->ctcss_tone == rig->caps->ctcss_list[tone])
            {
                break;
            }
        }

        if (chan->ctcss_tone != rig->caps->ctcss_list[tone])
        {
            tone = 0;
        }
    }

    if (RIG_IS_TS940)
    {
        bank = '0' + chan->bank_num;
    }

    SNPRINTF(buf, sizeof(buf),
             "MW0%c%02d%011"PRIll"%c%c%c%02d ", /* note the space at
                                                     the end */
             bank,
             chan->channel_num,
             (int64_t)chan->freq,
             '0' + mode,
             (chan->flags & RIG_CHFLAG_SKIP) ? '1' : '0',
             chan->ctcss_tone ? '1' : '0',
             chan->ctcss_tone ? (tone + 1) : 0);

    err = kenwood_transaction(rig, buf, NULL, 0);

    if (err != RIG_OK)
    {
        RETURNFUNC(err);
    }

    SNPRINTF(buf, sizeof(buf), "MW1%c%02d%011"PRIll"%c%c%c%02d ",
             bank,
             chan->channel_num,
             (int64_t)(chan->split == RIG_SPLIT_ON ? chan->tx_freq : 0),
             (chan->split == RIG_SPLIT_ON) ? ('0' + tx_mode) : '0',
             (chan->flags & RIG_CHFLAG_SKIP) ? '1' : '0',
             chan->ctcss_tone ? '1' : '0',
             chan->ctcss_tone ? (tone + 1) : 0);

    RETURNFUNC(kenwood_transaction(rig, buf, NULL, 0));
}

int kenwood_set_ext_parm(RIG *rig, token_t token, value_t val)
{
    struct kenwood_priv_data *priv = rig->state.priv;
    char buf[4];

    ENTERFUNC;

    switch (token)
    {
    case TOK_VOICE:
        RETURNFUNC(kenwood_transaction(rig, "VR", NULL, 0));

    case TOK_FINE:
        SNPRINTF(buf, sizeof(buf), "FS%c", (val.i == 0) ? '0' : '1');
        RETURNFUNC(kenwood_transaction(rig, buf, NULL, 0));

    case TOK_XIT:
        SNPRINTF(buf, sizeof(buf), "XT%c", (val.i == 0) ? '0' : '1');
        RETURNFUNC(kenwood_transaction(rig, buf, NULL, 0));

    case TOK_RIT:
        SNPRINTF(buf, sizeof(buf), "RT%c", (val.i == 0) ? '0' : '1');
        RETURNFUNC(kenwood_transaction(rig, buf, NULL, 0));

    case TOK_NO_ID:
        priv->no_id = val.i;
        RETURNFUNC(RIG_OK);
    }

    RETURNFUNC(-RIG_EINVAL);
}

int kenwood_get_ext_parm(RIG *rig, token_t token, value_t *val)
{
    int err;
    struct kenwood_priv_data *priv = rig->state.priv;

    ENTERFUNC;

    if (!val)
    {
        RETURNFUNC(-RIG_EINVAL);
    }

    switch (token)
    {
    case TOK_FINE:
        RETURNFUNC(get_kenwood_func(rig, "FS", &val->i));

    case TOK_XIT:
        err = kenwood_get_if(rig);

        if (err != RIG_OK)
        {
            RETURNFUNC(err);
        }

        val->i = (priv->info[24] == '1') ? 1 : 0;
        RETURNFUNC(RIG_OK);

    case TOK_RIT:
        err = kenwood_get_if(rig);

        if (err != RIG_OK)
        {
            RETURNFUNC(err);
        }

        val->i = (priv->info[23] == '1') ? 1 : 0;
        RETURNFUNC(RIG_OK);
    }

    RETURNFUNC(-RIG_ENIMPL);
}

/*
 * kenwood_get_info
 * supposed to work only for TS2000...
 */
const char *kenwood_get_info(RIG *rig)
{
    char firmbuf[10];
    int retval;

    ENTERFUNC;

    if (!rig)
    {
        return ("*rig == NULL");
    }

    retval = kenwood_safe_transaction(rig, "TY", firmbuf, 10, 5);

    if (retval != RIG_OK)
    {
        return (NULL);
    }

    switch (firmbuf[4])
    {
    case '0': return ("Firmware: Overseas type");

    case '1': return ("Firmware: Japanese 100W type");

    case '2': return ("Firmware: Japanese 20W type");

    default: return ("Firmware: unknown");
    }
}

#define IDBUFSZ 16

/*
 * proberigs_kenwood
 *
 * Notes:
 * There's only one rig possible per port.
 *
 * rig_model_t probeallrigs_kenwood(port_t *port, rig_probe_func_t cfunc, rig_ptr_t data)
 */
DECLARE_PROBERIG_BACKEND(kenwood)
{
    char idbuf[IDBUFSZ];
    int id_len = -1, i, k_id;
    int retval = -1;
    int rates[] = { 115200, 57600, 38400, 19200, 9600, 4800, 1200, 0 }; /* possible baud rates */
    int rates_idx;
    int write_delay = port->write_delay;
    short retry = port->retry;

    if (!port)
    {
        return (RIG_MODEL_NONE);
    }

    if (port->type.rig != RIG_PORT_SERIAL)
    {
        return (RIG_MODEL_NONE);
    }

    port->write_delay = port->post_write_delay = 0;
    port->parm.serial.stop_bits = 2;
    port->retry = 0;

    /*
     * try for all different baud rates
     */
    for (rates_idx = 0; rates[rates_idx]; rates_idx++)
    {
        port->parm.serial.rate = rates[rates_idx];
        port->timeout = 2 * 1000 / rates[rates_idx] + 50;

        retval = serial_open(port);

        if (retval != RIG_OK)
        {
            port->write_delay = write_delay;
            port->retry = retry;
            return (RIG_MODEL_NONE);
        }

        retval = write_block(port, (unsigned char *) "ID;", 3);
        id_len = read_string(port, (unsigned char *) idbuf, IDBUFSZ, ";\r", 2, 0, 1);
        close(port->fd);

        if (retval != RIG_OK || id_len < 0)
        {
            continue;
        }
    }

    if (retval != RIG_OK || id_len < 0 || !strcmp(idbuf, "ID;"))
    {
        port->write_delay = write_delay;
        port->retry = retry;
        return (RIG_MODEL_NONE);
    }

    /*
     * reply should be something like 'IDxxx;'
     */
    if (id_len != 5 && id_len != 6)
    {
        idbuf[7] = '\0';
        rig_debug(RIG_DEBUG_VERBOSE, "probe_kenwood: protocol error, "
                  " expected %d, received %d: %s\n",
                  6, id_len, idbuf);
        port->write_delay = write_delay;
        port->retry = retry;
        return (RIG_MODEL_NONE);
    }


    /* first, try ID string */
    for (i = 0; kenwood_id_string_list[i].model != RIG_MODEL_NONE; i++)
    {
        if (!strncmp(kenwood_id_string_list[i].id, idbuf + 2, 16))
        {
            rig_debug(RIG_DEBUG_VERBOSE, "probe_kenwood: "
                      "found %s\n", idbuf + 2);

            if (cfunc)
            {
                (*cfunc)(port, kenwood_id_string_list[i].model, data);
            }

            port->write_delay = write_delay;
            port->retry = retry;
            return (kenwood_id_string_list[i].model);
        }
    }

    /* then, try ID numbers */

    k_id = atoi(idbuf + 2);

    /*
     * Elecraft K2 returns same ID as TS570
     */
    if (k_id == 17)
    {
        retval = serial_open(port);

        if (retval != RIG_OK)
        {
            return (RIG_MODEL_NONE);
        }

        retval = write_block(port, (unsigned char *) "K2;", 3);
        id_len = read_string(port, (unsigned char *) idbuf, IDBUFSZ, ";\r", 2, 0, 1);
        close(port->fd);

        if (retval != RIG_OK)
        {
            return (RIG_MODEL_NONE);
        }

        /*
         * reply should be something like 'K2n;'
         */
        if (id_len == 4 || !strcmp(idbuf, "K2"))
        {
            rig_debug(RIG_DEBUG_VERBOSE, "%s: found K2\n", __func__);

            if (cfunc)
            {
                (*cfunc)(port, RIG_MODEL_K2, data);
            }

            return (RIG_MODEL_K2);
        }
    }

    for (i = 0; kenwood_id_list[i].model != RIG_MODEL_NONE; i++)
    {
        if (kenwood_id_list[i].id == k_id)
        {
            rig_debug(RIG_DEBUG_VERBOSE, "probe_kenwood: "
                      "found %03d\n", k_id);

            if (cfunc)
            {
                (*cfunc)(port, kenwood_id_list[i].model, data);
            }

            return (kenwood_id_list[i].model);
        }
    }

    /*
     * not found in known table....
     * update kenwood_id_list[]!
     */
    rig_debug(RIG_DEBUG_WARN, "probe_kenwood: found unknown device "
              "with ID %03d, please report to Hamlib "
              "developers.\n", k_id);

    rig_debug(RIG_DEBUG_TRACE, "%s: post_write_delay=%d\n", __func__,
              port->post_write_delay);
    return (RIG_MODEL_NONE);
}


/*
 * initrigs_kenwood is called by rig_backend_load
 */
DECLARE_INITRIG_BACKEND(kenwood)
{
    rig_register(&ts950s_caps);
    rig_register(&ts950sdx_caps);
    rig_register(&ts50s_caps);
    rig_register(&ts140_caps);
    rig_register(&ts450s_caps);
    rig_register(&ts570d_caps);
    rig_register(&ts570s_caps);
    rig_register(&ts680s_caps);
    rig_register(&ts690s_caps);
    rig_register(&ts790_caps);
    rig_register(&ts850_caps);
    rig_register(&ts870s_caps);
    rig_register(&ts930_caps);
    rig_register(&ts2000_caps);
    rig_register(&trc80_caps);
    rig_register(&k2_caps);
    rig_register(&k3_caps);
    rig_register(&k3s_caps);
    rig_register(&kx2_caps);
    rig_register(&kx3_caps);
    rig_register(&k4_caps);
    rig_register(&xg3_caps);

    rig_register(&ts440_caps);
    rig_register(&ts940_caps);
    rig_register(&ts711_caps);
    rig_register(&ts811_caps);
    rig_register(&r5000_caps);

    rig_register(&tmd700_caps);
    rig_register(&thd7a_caps);
    rig_register(&thd72a_caps);
    rig_register(&thd74_caps);
    rig_register(&thf7e_caps);
    rig_register(&thg71_caps);
    rig_register(&tmv7_caps);
    rig_register(&tmv71_caps);
    rig_register(&tmd710_caps);

    rig_register(&ts590_caps);
    rig_register(&ts990s_caps);
    rig_register(&ts590sg_caps);
    rig_register(&ts480_caps);
    rig_register(&thf6a_caps);

    rig_register(&transfox_caps);

    rig_register(&f6k_caps);
    rig_register(&powersdr_caps);
    rig_register(&pihpsdr_caps);
    rig_register(&ts890s_caps);
    rig_register(&pt8000a_caps);
    rig_register(&malachite_caps);
    rig_register(&tx500_caps);
    rig_register(&sdruno_caps);
    rig_register(&qrplabs_caps);

    return (RIG_OK);
}
