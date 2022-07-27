/*
 *  Hamlib ELAD backend - main file
 *  Copyright (c) 2000-2011 by Stephane Fillod
 *  Copyright (C) 2009,2010 Alessandro Zummo <a.zummo@towertech.it>
 *  Copyright (C) 2009,2010,2011,2012,2013 by Nate Bargmann, n0nb@n0nb.us
 *  Copyright (C) 2018 by Giovanni Franza, info@hb9eik.ch
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
#include <math.h>
#include <ctype.h>

#include "hamlib/rig.h"
#include "network.h"
#include "serial.h"
#include "misc.h"
#include "register.h"
#include "cal.h"

#include "elad.h"

#ifndef max
#define max(a,b) (((a) (b)) ? (a) : (b))
#define min(a,b) (((a) < (b)) ? (a) : (b))
#endif

struct elad_id
{
    rig_model_t model;
    int id;
};

struct elad_id_string
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
static const struct elad_id elad_id_list[] =
{
    { RIG_MODEL_ELAD_FDM_DUO, 1 },
    { RIG_MODEL_NONE, UNKNOWN_ID }, /* end marker */
};

/* XXX numeric ids have been tested only with the TS-450 */
static const struct elad_id_string elad_id_string_list[] =
{
    { RIG_MODEL_ELAD_FDM_DUO,  "001" },
    { RIG_MODEL_NONE, NULL }, /* end marker */
};

rmode_t elad_mode_table[ELAD_MODE_TABLE_MAX] =
{
    [0] = RIG_MODE_NONE,
    [1] = RIG_MODE_LSB,
    [2] = RIG_MODE_USB,
    [3] = RIG_MODE_CW,
    [4] = RIG_MODE_FM,
    [5] = RIG_MODE_AM,
    [6] = RIG_MODE_RTTY,
    [7] = RIG_MODE_CWR,
    [8] = RIG_MODE_NONE,  /* TUNE mode */
    [9] = RIG_MODE_RTTYR
};

/*
 * 38 CTCSS sub-audible tones
 */
const tone_t elad38_ctcss_list[] =
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
const tone_t elad42_ctcss_list[] =
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
const struct confparams elad_cfg_params[] =
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
    { RIG_CONF_END, NULL, }
};


/**
 * elad_transaction
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
int elad_transaction(RIG *rig, const char *cmdstr, char *data, size_t datasize)
{
    struct elad_priv_data *priv = rig->state.priv;
    struct elad_priv_caps *caps = elad_caps(rig);
    struct rig_state *rs;
    int retval;
    char cmdtrm[2];  /* Default Command/Reply termination char */
    char *cmd;
    size_t len;
    int retry_read = 0;

    char buffer[ELAD_MAX_BUF_LEN]; /* use our own buffer since
                                       verification may need a longer
                                       buffer than the user supplied one */

    rig_debug(RIG_DEBUG_VERBOSE, "%s called\n", __func__);

    if ((!cmdstr && !datasize) || (datasize && !data))
    {
        return -RIG_EINVAL;
    }

    rs = &rig->state;

    rs->transaction_active = 1;

    /* Emulators don't need any post_write_delay */
    if (priv->is_emulation) { rs->rigport.post_write_delay = 0; }

    cmdtrm[0] = caps->cmdtrm;
    cmdtrm[1] = '\0';

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

    if (!datasize)
    {
        rig->state.transaction_active = 0;

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
    /* allow one extra byte for terminator we don't return */
    len = min(datasize ? datasize + 1 : strlen(priv->verify_cmd) + 13,
              ELAD_MAX_BUF_LEN);
    retval = read_string(&rs->rigport, (unsigned char *) buffer, len,
                         cmdtrm, strlen(cmdtrm), 0, 1);

    if (retval < 0)
    {
        if (retry_read++ < rs->rigport.retry)
        {
            goto transaction_write;
        }

        goto transaction_quit;
    }

    /* Check that command termination is correct */
    if (strchr(cmdtrm, buffer[strlen(buffer) - 1]) == NULL)
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
            }

            if (retry_read++ < rs->rigport.retry)
            {
                rig_debug(RIG_DEBUG_ERR, "%s: Retrying shortly\n", __func__);
                hl_usleep(rig->caps->timeout * 1000);
                goto transaction_read;
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
            rig_debug(RIG_DEBUG_ERR, "%s: Wrong reply %c%c for command %c%c\n",
                      __func__, buffer[0], buffer[1], cmdstr[0], cmdstr[1]);

            if (retry_read++ < rs->rigport.retry)
            {
                goto transaction_write;
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
        if (priv->verify_cmd[0] != buffer[0]
                || (priv->verify_cmd[1] && priv->verify_cmd[1] != buffer[1]))
        {
            /*
             * TODO: When RIG_TRN is enabled, we can pass the string to
             * the decoder for callback. That way we don't ignore any
             * commands.
             */
            rig_debug(RIG_DEBUG_ERR,
                      "%s: WRONG reply %c%c for command verification %c%c (datasize=%d)\n",
                      __func__, buffer[0], buffer[1]
                      , priv->verify_cmd[0], priv->verify_cmd[1], (int)datasize);

            if (retry_read++ < rs->rigport.retry)
            {
                goto transaction_write;
            }

            retval =  -RIG_EPROTO;
            goto transaction_quit;
        }
    }

    retval = RIG_OK;

transaction_quit:

    rs->transaction_active = 0;
    return retval;
}


/**
 * elad_safe_transaction
 * A wrapper for elad_transaction to check returned data against
 * expected length,
 *
 * Parameters:
 *  cmd     Same as elad_transaction() cmdstr
 *  buf     Same as kenwwod_transaction() data
 *  buf_size  Same as elad_transaction() datasize
 *  expected  Value of expected string length
 *
 * Returns:
 *   RIG_OK -   if no error occurred.
 *   RIG_EPROTO   if returned string and expected are not equal
 *   Error from elad_transaction() if any
 *
 */
int elad_safe_transaction(RIG *rig, const char *cmd, char *buf,
                          size_t buf_size, size_t expected)
{
    int err;
    int retry = 0;

    rig_debug(RIG_DEBUG_VERBOSE, "%s called\n", __func__);

    if (expected == 0)
    {
        buf_size = 0;
    }

    do
    {
        size_t length;
        err = elad_transaction(rig, cmd, buf, buf_size);

        if (err != RIG_OK)        /* return immediately on error as any
                                   retries handled at lower level */
        {
            return err;
        }

        length = strlen(buf);

        if (length != expected) /* worth retrying as some rigs
                                   occasionally send short results */
        {
            rig_debug(RIG_DEBUG_ERR, "%s: wrong answer; len for cmd %s: "
                      "expected = %d, got %d\n",
                      __func__, cmd, (int)expected, (int)length);
            err =  -RIG_EPROTO;
            hl_usleep(rig->caps->timeout * 1000);
        }
    }
    while (err != RIG_OK && ++retry < rig->state.rigport.retry);

    return err;
}

rmode_t elad2rmode(unsigned char mode, const rmode_t mode_table[])
{
    rig_debug(RIG_DEBUG_VERBOSE, "%s called\n", __func__);

    if (mode >= ELAD_MODE_TABLE_MAX)
    {
        return RIG_MODE_NONE;
    }

    return mode_table[mode];
}

char rmode2elad(rmode_t mode, const rmode_t mode_table[])
{
    rig_debug(RIG_DEBUG_VERBOSE, "%s called\n", __func__);

    if (mode != RIG_MODE_NONE)
    {
        int i;

        for (i = 0; i < ELAD_MODE_TABLE_MAX; i++)
        {
            if (mode_table[i] == mode)
            {
                return i;
            }
        }
    }

    return -1;
}

int elad_init(RIG *rig)
{
    struct elad_priv_data *priv;
    struct elad_priv_caps *caps = elad_caps(rig);

    rig_debug(RIG_DEBUG_VERBOSE, "%s called\n", __func__);

    rig->state.priv = calloc(1, sizeof(struct elad_priv_data));

    if (rig->state.priv == NULL)
    {
        return -RIG_ENOMEM;
    }

    priv = rig->state.priv;

    memset(priv, 0x00, sizeof(struct elad_priv_data));
    strcpy(priv->verify_cmd, RIG_MODEL_XG3 == rig->caps->rig_model ? ";" : "ID;");
    priv->split = RIG_SPLIT_OFF;
    priv->trn_state = -1;
    priv->curr_mode = 0;

    /* default mode_table */
    if (caps->mode_table == NULL)
    {
        caps->mode_table = elad_mode_table;
    }

    /* default if_len */
    if (caps->if_len == 0)
    {
        caps->if_len = 37;
    }

    rig_debug(RIG_DEBUG_TRACE, "%s: if_len = %d\n", __func__, caps->if_len);

    return RIG_OK;
}

int elad_cleanup(RIG *rig)
{
    rig_debug(RIG_DEBUG_VERBOSE, "%s called\n", __func__);

    free(rig->state.priv);
    rig->state.priv = NULL;

    return RIG_OK;
}

int elad_open(RIG *rig)
{
    struct elad_priv_data *priv = rig->state.priv;
    int err, i;
    char *idptr;
    char id[ELAD_MAX_BUF_LEN];

    rig_debug(RIG_DEBUG_VERBOSE, "%s called\n", __func__);

    if (RIG_MODEL_TS590S == rig->caps->rig_model)
    {
        char *dot_pos;
        /* we need the firmware version for these rigs to deal with f/w defects */
        static char fw_version[7];

        err = elad_transaction(rig, "FV", fw_version, sizeof(fw_version));

        if (RIG_OK != err)
        {
            rig_debug(RIG_DEBUG_ERR, "%s: cannot get f/w version\n", __func__);
            return err;
        }

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
            return -RIG_EPROTO;
        }

        rig_debug(RIG_DEBUG_TRACE, "%s: found f/w version %s\n", __func__,
                  priv->fw_rev);
    }

    /* get id in buffer, will be null terminated */
    err = elad_get_id(rig, id);

    if (RIG_MODEL_XG3 != rig->caps->rig_model && -RIG_ETIMEOUT == err)
    {
        /* Some Kenwood emulations have no ID command response :(
         * Try an FA command to see is anyone is listening */
        char buffer[ELAD_MAX_BUF_LEN];
        err = elad_transaction(rig, "FA", buffer, sizeof(buffer));

        if (RIG_OK != err)
        {
            rig_debug(RIG_DEBUG_ERR, "%s: no response from rig\n", __func__);
            return err;
        }

        /* here we know there is something that responds to FA but not
           to ID so use FA as the command verification command */
        strcpy(priv->verify_cmd, "FA;");
        strcpy(id, "ID019");      /* fake a TS-2000 */
    }
    else
    {
        if (err != RIG_OK)
        {
            rig_debug(RIG_DEBUG_ERR, "%s: cannot get identification\n", __func__);
            return err;
        }
    }

    /* id is something like 'IDXXX' or 'ID XXX' */
    if (strlen(id) < 5)
    {
        rig_debug(RIG_DEBUG_ERR, "%s: unknown id type (%s)\n", __func__, id);
        return -RIG_EPROTO;
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
    for (i = 0; elad_id_string_list[i].model != RIG_MODEL_NONE; i++)
    {
        if (strcmp(elad_id_string_list[i].id, idptr) != 0)
        {
            continue;
        }

        /* found matching id, verify driver */
        rig_debug(RIG_DEBUG_TRACE, "%s: found match %s\n",
                  __func__, elad_id_string_list[i].id);

        if (elad_id_string_list[i].model == rig->caps->rig_model)
        {
            /* get current AI state so it can be restored */
            elad_get_trn(rig, &priv->trn_state);  /* ignore errors */
            /* Currently we cannot cope with AI mode so turn it off in
               case last client left it on */
            elad_set_trn(rig, RIG_TRN_OFF); /* ignore status in case
                                            it's not supported */
            return RIG_OK;
        }

        /* driver mismatch */
        rig_debug(RIG_DEBUG_ERR,
                  "%s: wrong driver selected (%u instead of %u)\n",
                  __func__, rig->caps->rig_model,
                  elad_id_string_list[i].model);

        return -RIG_EINVAL;
    }

    rig_debug(RIG_DEBUG_ERR, "%s: your rig (%s) is unknown\n",
              __func__, id);

    return -RIG_EPROTO;
}


int elad_close(RIG *rig)
{
    struct elad_priv_data *priv = rig->state.priv;

    rig_debug(RIG_DEBUG_VERBOSE, "%s called\n", __func__);

    if (!no_restore_ai && priv->trn_state >= 0)
    {
        /* restore AI state */
        elad_set_trn(rig, priv->trn_state); /* ignore status in case
                                                 it's not supported */
    }

    return RIG_OK;
}


/* ID
 *  Reads transceiver ID number
 *
 *  caller must give a buffer of ELAD_MAX_BUF_LEN size
 *
 */
int elad_get_id(RIG *rig, char *buf)
{
    rig_debug(RIG_DEBUG_VERBOSE, "%s called\n", __func__);

    return elad_transaction(rig, "ID", buf, ELAD_MAX_BUF_LEN);
}


/* IF
 *  Retrieves the transceiver status
 *
 */
static int elad_get_if(RIG *rig)
{
    struct elad_priv_data *priv = rig->state.priv;
    struct elad_priv_caps *caps = elad_caps(rig);

    rig_debug(RIG_DEBUG_VERBOSE, "%s called\n", __func__);

    return elad_safe_transaction(rig, "IF", priv->info,
                                 ELAD_MAX_BUF_LEN, caps->if_len);
}


/* FN FR FT
 *  Sets the RX/TX VFO or M.CH mode of the transceiver, does not set split
 *  VFO, but leaves it unchanged if in split VFO mode.
 *
 */
int elad_set_vfo(RIG *rig, vfo_t vfo)
{
    char cmdbuf[6];
    int retval;
    char vfo_function;
    struct elad_priv_data *priv = rig->state.priv;

    rig_debug(RIG_DEBUG_VERBOSE, "%s called\n", __func__);

    /* Emulations do not need to set VFO since VFOB is a copy of VFOA
     * except for frequency.  And we can change freq without changing VFOS
     * This prevents a 1.8 second delay in PowerSDR when switching VFOs
     * We'll do this once if curr_mode has not been set yet
     */
    if (priv->is_emulation && priv->curr_mode > 0) { return RIG_OK; }

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

    case RIG_VFO_CURR:
        return RIG_OK;

    default:
        rig_debug(RIG_DEBUG_ERR, "%s: unsupported VFO %s\n", __func__, rig_strvfo(vfo));
        return -RIG_EINVAL;
    }

    //if rig=ts2000 then check Satellite mode status
    if (rig->caps->rig_model == RIG_MODEL_TS2000 && !priv->is_emulation)
    {
        char retbuf[20];
        rig_debug(RIG_DEBUG_VERBOSE, "%s: Checking Satellite mode status\n", __func__);
        SNPRINTF(cmdbuf, sizeof(cmdbuf), "SA");

        retval = elad_transaction(rig, cmdbuf, retbuf, 20);

        if (retval != RIG_OK)
        {
            return retval;
        }

        rig_debug(RIG_DEBUG_VERBOSE, "Satellite mode status %s\n", retbuf);

        //Satellite mode ON
        if (retbuf[2] == '1')
        {
            //SAT mode doesn't allow FR command (cannot select VFO)
            //selecting VFO is useless in SAT MODE
            return RIG_OK;
        }
    }

    SNPRINTF(cmdbuf, sizeof(cmdbuf), "FR%c", vfo_function);

    if (rig->caps->rig_model == RIG_MODEL_TS50
            || rig->caps->rig_model == RIG_MODEL_TS940)
    {
        cmdbuf[1] = 'N';
    }

    /* set RX VFO */
    retval = elad_transaction(rig, cmdbuf, NULL, 0);

    if (retval != RIG_OK)
    {
        return retval;
    }

    /* if FN command then there's no FT or FR */
    /* If split mode on, the don't change TxVFO */
    if ('N' == cmdbuf[1] || priv->split != RIG_SPLIT_OFF)
    {
        return RIG_OK;
    }

    /* set TX VFO */
    cmdbuf[1] = 'T';
    return elad_transaction(rig, cmdbuf, NULL, 0);
}


/* CB
 *  Sets the operating VFO, does not set split
 *  VFO, but leaves it unchanged if in split VFO mode.
 *
 */
int elad_set_vfo_main_sub(RIG *rig, vfo_t vfo)
{
    char cmdbuf[6];
    char vfo_function;

    rig_debug(RIG_DEBUG_VERBOSE, "%s called\n", __func__);

    switch (vfo)
    {
    case RIG_VFO_A:
    case RIG_VFO_MAIN:
        vfo_function = '0';
        break;

    case RIG_VFO_B:
    case RIG_VFO_SUB:
        vfo_function = '1';
        break;

    case RIG_VFO_CURR:
        return RIG_OK;

    default:
        rig_debug(RIG_DEBUG_ERR, "%s: unsupported VFO %s\n", __func__, rig_strvfo(vfo));
        return -RIG_EINVAL;
    }

    SNPRINTF(cmdbuf, sizeof(cmdbuf), "CB%c", vfo_function);
    return elad_transaction(rig, cmdbuf, NULL, 0);
}


/* CB
 *  Gets the operating VFO
 *
 */
int elad_get_vfo_main_sub(RIG *rig, vfo_t *vfo)
{
    char buf[4];
    int rc;

    rig_debug(RIG_DEBUG_VERBOSE, "%s called\n", __func__);

    if (RIG_OK == (rc = elad_safe_transaction(rig, "CB", buf, sizeof(buf), 3)))
    {
        *vfo = buf[2] == '1' ? RIG_VFO_SUB : RIG_VFO_MAIN;
    }

    return rc;
}


/* FR FT TB
 *  Sets the split RX/TX VFO or M.CH mode of the transceiver.
 *
 */
int elad_set_split_vfo(RIG *rig, vfo_t vfo, split_t split, vfo_t txvfo)
{
    struct elad_priv_data *priv = rig->state.priv;
    char cmdbuf[6];
    int retval;
    unsigned char vfo_function;
    split_t tsplit;
    vfo_t tvfo;

    rig_debug(RIG_DEBUG_VERBOSE, "%s called\n", __func__);

    retval = elad_get_split_vfo_if(rig, vfo, &tsplit, &tvfo);

    if (split == tsplit)
    {
        rig_debug(RIG_DEBUG_TRACE, "%s: No change detected...ignoring request\n",
                  __func__);
    }

    rig_debug(RIG_DEBUG_TRACE, "%s: Change detected requested split %d!=%d\n",
              __func__, split, tsplit);

    if (split)
    {
        // Rx MAIN/Tx SUB is the only split method
        retval = elad_set_vfo_main_sub(rig, RIG_VFO_MAIN);

        if (retval != RIG_OK) { return retval; }
    }

    SNPRINTF(cmdbuf, sizeof(cmdbuf), "SP%c", RIG_SPLIT_ON == split ? '1' : '0');
    return elad_transaction(rig, cmdbuf, NULL, 0);

    /* Split off means Rx and Tx are the same */
    if (split == RIG_SPLIT_OFF)
    {
        txvfo = RIG_VFO_MAIN;

        if (txvfo == RIG_VFO_CURR)
        {
            retval = rig_get_vfo(rig, &txvfo);

            if (retval != RIG_OK)
            {
                return retval;
            }
        }
    }

    switch (txvfo)
    {
    case RIG_VFO_VFO:
    case RIG_VFO_MAIN:
    case RIG_VFO_A: vfo_function = '0'; txvfo = RIG_VFO_MAIN; break;

    case RIG_VFO_SUB:
    case RIG_VFO_B: vfo_function = '1'; txvfo = RIG_VFO_SUB; break;

    case RIG_VFO_MEM: vfo_function = '2'; break;

    default:
        rig_debug(RIG_DEBUG_ERR, "%s: unsupported VFO %s\n", __func__,
                  rig_strvfo(txvfo));
        return -RIG_EINVAL;
    }

    /* set TX VFO */
    SNPRINTF(cmdbuf, sizeof(cmdbuf), "FT%c", vfo_function);
    retval = elad_transaction(rig, cmdbuf, NULL, 0);

    if (retval != RIG_OK)
    {
        return retval;
    }

    retval = elad_set_split(rig, vfo, split, txvfo);
    /* Remember whether split is on, for elad_set_vfo */
    priv->split = split;

    return RIG_OK;
}


/* SP
 *  Sets the split mode of the transceivers that have the FN command.
 *
 */
int elad_set_split(RIG *rig, vfo_t vfo, split_t split, vfo_t txvfo)
{
    struct elad_priv_data *priv = rig->state.priv;
    char cmdbuf[6];
    int retval;

    rig_debug(RIG_DEBUG_VERBOSE, "%s called\n", __func__);

    // we want stand-alone split so we can control it
    SNPRINTF(cmdbuf, sizeof(cmdbuf), "SP%c", RIG_SPLIT_ON == split ? '2' : '0');

    retval = elad_transaction(rig, cmdbuf, NULL, 0);

    if (retval != RIG_OK)
    {
        return retval;
    }

    /* Remember whether split is on, for elad_set_vfo */
    priv->split = split;

    return RIG_OK;
}


/* IF TB
 *  Gets split VFO status from elad_get_if()
 *
 */
int elad_get_split_vfo_if(RIG *rig, vfo_t rxvfo, split_t *split, vfo_t *txvfo)
{
    struct elad_priv_data *priv = rig->state.priv;
    int retval;
    int transmitting;

    rig_debug(RIG_DEBUG_VERBOSE, "%s called\n", __func__);

    if (!split || !txvfo)
    {
        return -RIG_EINVAL;
    }


    if (RIG_MODEL_TS990S == rig->caps->rig_model)
    {
        char buf[4];

        if (RIG_OK == (retval = elad_safe_transaction(rig, "SP", buf, sizeof(buf), 3)))
        {
            if ('1' == buf[2])
            {
                *split = RIG_SPLIT_ON;
                *txvfo = RIG_VFO_SUB;
            }
            else
            {
                *split = RIG_SPLIT_OFF;
                *txvfo = RIG_VFO_MAIN;
            }
        }

        return retval;
    }

    retval = elad_get_if(rig);

    if (retval != RIG_OK)
    {
        return retval;
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
        return -RIG_EPROTO;
    }

    /* Remember whether split is on, for elad_set_vfo */
    priv->split = *split;

    /* find where is the txvfo.. */
    /* Elecraft info[30] does not track split VFO when transmitting */
    transmitting = '1' == priv->info[28]
                   && RIG_MODEL_K2 != rig->caps->rig_model
                   && RIG_MODEL_K3 != rig->caps->rig_model;

    switch (priv->info[30])
    {
    case '0':
        *txvfo = (*split && !transmitting) ? RIG_VFO_B : RIG_VFO_A;
        break;

    case '1':
        *txvfo = (*split && !transmitting) ? RIG_VFO_A : RIG_VFO_B;
        break;

    case '2':
        *txvfo = RIG_VFO_MEM; /* SPLIT MEM operation doesn't involve VFO A or VFO B */
        break;

    default:
        rig_debug(RIG_DEBUG_ERR, "%s: unsupported VFO %c\n",
                  __func__, priv->info[30]);
        return -RIG_EPROTO;
    }

    return RIG_OK;
}


/*
 * elad_get_vfo_if using byte 31 of the IF information field
 *
 * Specifically this needs to return the RX VFO, the IF command tells
 * us the TX VFO in split TX mode when transmitting so we need to swap
 * results sometimes.
 */
int elad_get_vfo_if(RIG *rig, vfo_t *vfo)
{
    int retval;
    struct elad_priv_data *priv = rig->state.priv;
    int split_and_transmitting;

    rig_debug(RIG_DEBUG_VERBOSE, "%s called\n", __func__);

    retval = elad_get_if(rig);

    if (retval != RIG_OK)
    {
        return retval;
    }

    /* Elecraft info[30] does not track split VFO when transmitting */
    split_and_transmitting =
        '1' == priv->info[28] /* transmitting */
        && '1' == priv->info[32]               /* split */
        && RIG_MODEL_K2 != rig->caps->rig_model
        && RIG_MODEL_K3 != rig->caps->rig_model;

    switch (priv->info[30])
    {
    case '0':
        *vfo = split_and_transmitting ? RIG_VFO_B : RIG_VFO_A;
        break;

    case '1':
        *vfo = split_and_transmitting ? RIG_VFO_A : RIG_VFO_B;
        break;

    case '2':
        *vfo = RIG_VFO_MEM;
        break;

    default:
        rig_debug(RIG_DEBUG_ERR, "%s: unsupported VFO %c\n",
                  __func__, priv->info[30]);
        return -RIG_EPROTO;
    }

    return RIG_OK;
}


/*
 * elad_set_freq
 */
int elad_set_freq(RIG *rig, vfo_t vfo, freq_t freq)
{
    char freqbuf[16];
    unsigned char vfo_letter = '\0';
    vfo_t tvfo;
    int err;
    struct elad_priv_data *priv = rig->state.priv;


    rig_debug(RIG_DEBUG_VERBOSE, "%s called\n", __func__);

    tvfo = (vfo == RIG_VFO_CURR
            || vfo == RIG_VFO_VFO) ? rig->state.current_vfo : vfo;

    if (RIG_VFO_CURR == tvfo)
    {
        /* fetch from rig */
        err = rig_get_vfo(rig, &tvfo);

        if (RIG_OK != err) { return err; }
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

    default:
        rig_debug(RIG_DEBUG_ERR, "%s: unsupported VFO %s\n", __func__, rig_strvfo(vfo));
        return -RIG_EINVAL;
    }

    // cppcheck-suppress *
    SNPRINTF(freqbuf, sizeof(freqbuf), "F%c%011"PRIll, vfo_letter, (int64_t)freq);

    err = elad_transaction(rig, freqbuf, NULL, 0);

    if (RIG_OK == err && RIG_MODEL_TS590S == rig->caps->rig_model
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

        err = elad_get_if(rig);

        if (RIG_OK != err)
        {
            return err;
        }

        if ('1' == priv->info[32] && priv->info[30] != ('A' == vfo_letter ? '0' : '1'))
        {
            /* split mode and setting "back" VFO */

            /* set other VFO to whatever it is at currently */
            err = elad_safe_transaction(rig, 'A' == vfo_letter ? "FB" : "FA", freqbuf, 16,
                                        13);

            if (RIG_OK != err)
            {
                return err;
            }

            err = elad_transaction(rig, freqbuf, NULL, 0);
        }
    }

    return err;
}

int elad_get_freq_if(RIG *rig, vfo_t vfo, freq_t *freq)
{
    struct elad_priv_data *priv = rig->state.priv;
    char freqbuf[50];
    int retval;

    rig_debug(RIG_DEBUG_VERBOSE, "%s called\n", __func__);

    if (!freq)
    {
        return -RIG_EINVAL;
    }

    retval = elad_get_if(rig);

    if (retval != RIG_OK)
    {
        return retval;
    }

    memcpy(freqbuf, priv->info, 15);
    freqbuf[14] = '\0';
    sscanf(freqbuf + 2, "%"SCNfreq, freq);

    return RIG_OK;
}

/*
 * elad_get_freq
 */
int elad_get_freq(RIG *rig, vfo_t vfo, freq_t *freq)
{
    char freqbuf[50];
    char cmdbuf[4];
    int retval;
    unsigned char vfo_letter = '\0';
    vfo_t tvfo;

    rig_debug(RIG_DEBUG_VERBOSE, "%s called\n", __func__);

    if (!freq)
    {
        return -RIG_EINVAL;
    }

    tvfo = (vfo == RIG_VFO_CURR
            || vfo == RIG_VFO_VFO) ? rig->state.current_vfo : vfo;

    if (RIG_VFO_CURR == tvfo)
    {
        /* fetch from rig */
        retval = rig_get_vfo(rig, &tvfo);

        if (RIG_OK != retval) { return retval; }
    }

    /* memory frequency cannot be read with an Fx command, use IF */
    if (tvfo == RIG_VFO_MEM)
    {

        return elad_get_freq_if(rig, vfo, freq);
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

    default:
        rig_debug(RIG_DEBUG_ERR, "%s: unsupported VFO %s\n", __func__, rig_strvfo(vfo));
        return -RIG_EINVAL;
    }

    SNPRINTF(cmdbuf, sizeof(cmdbuf), "F%c", vfo_letter);

    retval = elad_safe_transaction(rig, cmdbuf, freqbuf, 50, 13);

    if (retval != RIG_OK)
    {
        return retval;
    }

    sscanf(freqbuf + 2, "%"SCNfreq, freq);

    return RIG_OK;
}

int elad_get_rit(RIG *rig, vfo_t vfo, shortfreq_t *rit)
{
    int retval;
    char buf[6];
    struct elad_priv_data *priv = rig->state.priv;

    rig_debug(RIG_DEBUG_VERBOSE, "%s called\n", __func__);

    retval = elad_get_if(rig);

    if (retval != RIG_OK)
    {
        return retval;
    }

    memcpy(buf, &priv->info[18], 5);

    buf[5] = '\0';
    *rit = atoi(buf);

    return RIG_OK;
}

/*
 * rit can only move up/down by 10 Hz, so we use a loop...
 */
int elad_set_rit(RIG *rig, vfo_t vfo, shortfreq_t rit)
{
    char buf[4];
    int retval, i;

    rig_debug(RIG_DEBUG_VERBOSE, "%s called\n", __func__);

    if (rit == 0)
    {
        return elad_transaction(rig, "RC", NULL, 0);
    }

    SNPRINTF(buf, sizeof(buf), "R%c", (rit > 0) ? 'U' : 'D');

    retval = elad_transaction(rig, "RC", NULL, 0);

    if (retval != RIG_OK)
    {
        return retval;
    }

    for (i = 0; i < labs(lrint(rit / 10)); i++)
    {
        retval = elad_transaction(rig, buf, NULL, 0);
    }

    return retval;
}

/*
 * rit and xit are the same
 */
int elad_get_xit(RIG *rig, vfo_t vfo, shortfreq_t *rit)
{
    rig_debug(RIG_DEBUG_VERBOSE, "%s called\n", __func__);

    if (!rit)
    {
        return -RIG_EINVAL;
    }

    return elad_get_rit(rig, vfo, rit);
}

int elad_set_xit(RIG *rig, vfo_t vfo, shortfreq_t rit)
{
    rig_debug(RIG_DEBUG_VERBOSE, "%s called\n", __func__);

    return elad_set_rit(rig, vfo, rit);
}

int elad_scan(RIG *rig, vfo_t vfo, scan_t scan, int ch)
{
    rig_debug(RIG_DEBUG_VERBOSE, "%s called\n", __func__);

    if (RIG_MODEL_TS990S == rig->caps->rig_model)
    {
        return elad_transaction(rig, scan == RIG_SCAN_STOP ? "SC00" : "SC01", NULL, 0);
    }
    else
    {
        return elad_transaction(rig, scan == RIG_SCAN_STOP ? "SC0" : "SC1", NULL, 0);
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
static int elad_set_filter(RIG *rig, pbwidth_t width)
{
    char *cmd;

    rig_debug(RIG_DEBUG_VERBOSE, "%s called\n", __func__);

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

    return elad_transaction(rig, cmd, NULL, 0);
}

/*
 * elad_set_mode
 */
int elad_set_mode(RIG *rig, vfo_t vfo, rmode_t mode, pbwidth_t width)
{
    struct elad_priv_data *priv = rig->state.priv;
    struct elad_priv_caps *caps = elad_caps(rig);
    char buf[6];
    char kmode;
    int err;
    char data_mode = '0';

    rig_debug(RIG_DEBUG_VERBOSE, "%s called\n", __func__);

    if (RIG_MODEL_TS590S == rig->caps->rig_model
            || RIG_MODEL_TS590SG == rig->caps->rig_model)
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

    if (priv->is_emulation || rig->caps->rig_model == RIG_MODEL_HPSDR)
    {
        /* emulations like PowerSDR and SmartSDR normally hijack the
           RTTY modes for SSB-DATA AFSK modes */
        if (RIG_MODE_PKTLSB == mode) { mode = RIG_MODE_RTTY; }

        if (RIG_MODE_PKTUSB == mode) { mode = RIG_MODE_RTTYR; }
    }

    kmode = rmode2elad(mode, caps->mode_table);

    if (kmode < 0)
    {
        rig_debug(RIG_DEBUG_WARN, "%s: unsupported mode '%s'\n",
                  __func__, rig_strrmode(mode));
        return -RIG_EINVAL;
    }

    if (RIG_MODEL_TS990S == rig->caps->rig_model)
    {
        /* The TS990s has targetable read mode but can only set the mode
           of the current VFO :( So we need to toggle the operating VFO
           to set the "back" VFO mode. This is done here rather than not
           setting caps.targetable_vfo to not include
           RIG_TARGETABLE_MODE since the toggle is not required for
           reading the mode. */
        char c;
        vfo_t curr_vfo;
        err = elad_get_vfo_main_sub(rig, &curr_vfo);

        if (err != RIG_OK) { return err; }

        if (kmode <= 9)
        {
            c = '0' + kmode;
        }
        else
        {
            c = 'A' + kmode - 10;
        }

        if (vfo != RIG_VFO_CURR && vfo != curr_vfo)
        {
            err = elad_set_vfo_main_sub(rig, vfo);

            if (err != RIG_OK) { return err; }
        }

        SNPRINTF(buf, sizeof(buf), "OM0%c", c);  /* target vfo is ignored */
        err = elad_transaction(rig, buf, NULL, 0);

        if (vfo != RIG_VFO_CURR && vfo != curr_vfo)
        {
            int err2 = elad_set_vfo_main_sub(rig, curr_vfo);

            if (RIG_OK == err && err2 != RIG_OK) { return err2; }
        }
    }
    else
    {
        SNPRINTF(buf, sizeof(buf), "MD%c", '0' + kmode);
        err = elad_transaction(rig, buf, NULL, 0);
    }

    if (err != RIG_OK) { return err; }

    if (RIG_MODEL_TS590S == rig->caps->rig_model
            || RIG_MODEL_TS590SG == rig->caps->rig_model)
    {
        if (!(RIG_MODE_CW == mode
                || RIG_MODE_CWR == mode
                || RIG_MODE_AM == mode
                || RIG_MODE_RTTY == mode
                || RIG_MODE_RTTYR == mode))
        {
            /* supports DATA sub modes - see above */
            SNPRINTF(buf, sizeof(buf), "DA%c", data_mode);
            err = elad_transaction(rig, buf, NULL, 0);

            if (err != RIG_OK) { return err; }
        }
    }

    if (RIG_PASSBAND_NOCHANGE == width) { return RIG_OK; }

    if (rig->caps->rig_model == RIG_MODEL_TS450S
            || rig->caps->rig_model == RIG_MODEL_TS690S
            || rig->caps->rig_model == RIG_MODEL_TS850
            || rig->caps->rig_model == RIG_MODEL_TS950SDX)
    {

        if (RIG_PASSBAND_NORMAL == width)
        {
            width = rig_passband_normal(rig, mode);
        }

        elad_set_filter(rig, width);
        /* non fatal */
    }

    return RIG_OK;
}

static int elad_get_filter(RIG *rig, pbwidth_t *width)
{
    int err, f, f1, f2;
    char buf[10];

    rig_debug(RIG_DEBUG_VERBOSE, "%s called\n", __func__);

    if (!width)
    {
        return -RIG_EINVAL;
    }

    err = elad_safe_transaction(rig, "FL", buf, sizeof(buf), 8);

    if (err != RIG_OK)
    {
        return err;
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

    return RIG_OK;
}

/*
 * elad_get_mode
 */
int elad_get_mode(RIG *rig, vfo_t vfo, rmode_t *mode, pbwidth_t *width)
{
    struct elad_priv_data *priv = rig->state.priv;
    struct elad_priv_caps *caps = elad_caps(rig);
    char cmd[4];
    char modebuf[10];
    int offs;
    int retval;
    int kmode;

    rig_debug(RIG_DEBUG_VERBOSE, "%s called\n", __func__);

    if (!mode || !width)
    {
        return -RIG_EINVAL;
    }

    /* for emulation do not read mode from VFOB as it is copy of VFOA */
    /* we avoid the VFO swapping most of the time this way */
    /* only need to get it if it has to be initialized */
    if (priv->curr_mode > 0 && priv->is_emulation && vfo == RIG_VFO_B)
    {
        return priv->curr_mode;
    }

    if (RIG_MODEL_TS990S == rig->caps->rig_model)
    {
        char c;

        if (RIG_VFO_CURR == vfo || RIG_VFO_VFO == vfo)
        {
            if (RIG_OK != (retval = elad_get_vfo_main_sub(rig, &vfo)))
            {
                return retval;
            }
        }

        switch (vfo)
        {
        case RIG_VFO_MAIN: c = '0'; break;

        case RIG_VFO_SUB: c = '1'; break;

        default:
            rig_debug(RIG_DEBUG_ERR, "%s: unsupported VFO %s\n", __func__, rig_strvfo(vfo));
            return -RIG_EINVAL;
        }

        SNPRINTF(cmd, sizeof(cmd), "OM%c", c);
        offs = 3;
    }
    else
    {
        SNPRINTF(cmd, sizeof(cmd), "MD");
        offs = 2;
    }

    retval = elad_safe_transaction(rig, cmd, modebuf, 6, offs + 1);

    if (retval != RIG_OK)
    {
        return retval;
    }

    if (modebuf[offs] <= '9')
    {
        kmode = modebuf[offs] - '0';
    }
    else
    {
        kmode = modebuf[offs] - 'A' + 10;
    }

    *mode = elad2rmode(kmode, caps->mode_table);

    if (priv->is_emulation || rig->caps->rig_model == RIG_MODEL_HPSDR)
    {
        /* emulations like PowerSDR and SmartSDR normally hijack the
           RTTY modes for SSB-DATA AFSK modes */
        if (RIG_MODE_RTTY == *mode) { *mode = RIG_MODE_PKTLSB; }

        if (RIG_MODE_RTTYR == *mode) { *mode = RIG_MODE_PKTUSB; }
    }

    if (RIG_MODEL_TS590S == rig->caps->rig_model
            || RIG_MODEL_TS590SG == rig->caps->rig_model)
    {
        /* supports DATA sub-modes */
        retval = elad_safe_transaction(rig, "DA", modebuf, 6, 3);

        if (retval != RIG_OK)
        {
            return retval;
        }

        if ('1' == modebuf[2])
        {
            switch (*mode)
            {
            case RIG_MODE_USB: *mode = RIG_MODE_PKTUSB; break;

            case RIG_MODE_LSB: *mode = RIG_MODE_PKTLSB; break;

            case RIG_MODE_FM: *mode = RIG_MODE_PKTFM; break;

            default: break;
            }
        }
    }

    /* XXX ? */
    *width = rig_passband_normal(rig, *mode);

    return RIG_OK;
}

/* This is used when the radio does not support MD; for mode reading */
int elad_get_mode_if(RIG *rig, vfo_t vfo, rmode_t *mode, pbwidth_t *width)
{
    int err;
    struct elad_priv_caps *caps = elad_caps(rig);
    struct elad_priv_data *priv = rig->state.priv;

    rig_debug(RIG_DEBUG_VERBOSE, "%s called\n", __func__);

    if (!mode || !width)
    {
        return -RIG_EINVAL;
    }

    err = elad_get_if(rig);

    if (err != RIG_OK)
    {
        return err;
    }

    *mode = elad2rmode(priv->info[29] - '0', caps->mode_table);

    *width = rig_passband_normal(rig, *mode);

    if (rig->caps->rig_model == RIG_MODEL_TS450S
            || rig->caps->rig_model == RIG_MODEL_TS690S
            || rig->caps->rig_model == RIG_MODEL_TS850
            || rig->caps->rig_model == RIG_MODEL_TS950SDX)
    {

        elad_get_filter(rig, width);
        /* non fatal */
    }

    return RIG_OK;
}

int elad_set_level(RIG *rig, vfo_t vfo, setting_t level, value_t val)
{
    char levelbuf[16];
    int i, elad_val;

    rig_debug(RIG_DEBUG_VERBOSE, "%s called\n", __func__);

    if (RIG_LEVEL_IS_FLOAT(level))
    {
        elad_val = val.f * 255;
    }
    else
    {
        elad_val = val.i;
    }

    switch (level)
    {
    case RIG_LEVEL_RFPOWER:

        /*
         * Best estimate: 1.0 corresponds to 100W
         * Anything better must be done in rig-specific files.
         */
        if (RIG_LEVEL_IS_FLOAT(level)) { elad_val = val.f * 100; }

        SNPRINTF(levelbuf, sizeof(levelbuf), "PC%03d", elad_val);
        break;

    case RIG_LEVEL_AF:
        SNPRINTF(levelbuf, sizeof(levelbuf), "AG%03d", elad_val);
        break;

    case RIG_LEVEL_RF:
        /* XXX check level range */
        SNPRINTF(levelbuf, sizeof(levelbuf), "RG%03d", elad_val);
        break;

    case RIG_LEVEL_SQL:
        SNPRINTF(levelbuf, sizeof(levelbuf), "SQ%03d", elad_val);
        break;

    case RIG_LEVEL_AGC:
        if (elad_val > 3)
        {
            elad_val = 3;    /* 0.. 255 */
        }

        SNPRINTF(levelbuf, sizeof(levelbuf), "GT%03d", 84 * elad_val);
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
                return -RIG_EINVAL;
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
                return -RIG_EINVAL;
            }
        }

        break;

    case RIG_LEVEL_SLOPE_HIGH:
        if (val.i > 20 || val.i < 0)
        {
            return -RIG_EINVAL;
        }

        SNPRINTF(levelbuf, sizeof(levelbuf), "SH%02d", (val.i));
        break;

    case RIG_LEVEL_SLOPE_LOW:
        if (val.i > 20 || val.i < 0)
        {
            return -RIG_EINVAL;
        }

        SNPRINTF(levelbuf, sizeof(levelbuf), "SL%02d", (val.i));
        break;

    case RIG_LEVEL_CWPITCH:
        if (val.i > 1000 || val.i < 400)
        {
            return -RIG_EINVAL;
        }

        SNPRINTF(levelbuf, sizeof(levelbuf), "PT%02d", (val.i / 50) - 8);
        break;

    case RIG_LEVEL_KEYSPD:
        if (val.i > 50 || val.i < 5)
        {
            return -RIG_EINVAL;
        }

        SNPRINTF(levelbuf, sizeof(levelbuf), "KS%03d", val.i);
        break;

    default:
        rig_debug(RIG_DEBUG_ERR, "Unsupported set_level %s", rig_strlevel(level));
        return -RIG_EINVAL;
    }

    return elad_transaction(rig, levelbuf, NULL, 0);
}

int get_elad_level(RIG *rig, const char *cmd, float *f)
{
    char lvlbuf[10];
    int retval;
    int lvl;
    int len = strlen(cmd);

    rig_debug(RIG_DEBUG_VERBOSE, "%s called\n", __func__);

    if (!f)
    {
        return -RIG_EINVAL;
    }

    retval = elad_safe_transaction(rig, cmd, lvlbuf, 10, len + 3);

    if (retval != RIG_OK)
    {
        return retval;
    }

    /* 000..255 */
    sscanf(lvlbuf + len, "%d", &lvl);
    *f = lvl / 255.0;
    return RIG_OK;
};


/*
 * elad_get_level
 */
int elad_get_level(RIG *rig, vfo_t vfo, setting_t level, value_t *val)
{
    char lvlbuf[ELAD_MAX_BUF_LEN];
    char *cmd;
    int retval;
    int lvl;
    int i, ret, agclevel, len;

    rig_debug(RIG_DEBUG_VERBOSE, "%s called\n", __func__);

    if (!val)
    {
        return -RIG_EINVAL;
    }

    switch (level)
    {
    case RIG_LEVEL_RAWSTR:
        if (RIG_MODEL_TS590S == rig->caps->rig_model
                || RIG_MODEL_TS590SG == rig->caps->rig_model)
        {
            cmd = "SM0";
            len = 3;
        }
        else
        {
            cmd = "SM";
            len = 2;
        }

        retval = elad_safe_transaction(rig, cmd, lvlbuf, 10, len + 4);

        if (retval != RIG_OK)
        {
            return retval;
        }

        /* XXX atoi ? */
        sscanf(lvlbuf + len, "%d", &val->i); /* rawstr */
        break;

    case RIG_LEVEL_STRENGTH:
        if (RIG_MODEL_TS590S == rig->caps->rig_model
                || RIG_MODEL_TS590SG == rig->caps->rig_model)
        {
            cmd = "SM0";
            len = 3;
        }
        else
        {
            cmd = "SM";
            len = 2;
        }

        retval = elad_safe_transaction(rig, cmd, lvlbuf, 10, len + 4);

        if (retval != RIG_OK)
        {
            return retval;
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
        retval = elad_safe_transaction(rig, "RA", lvlbuf, 50, 6);

        if (retval != RIG_OK)
        {
            return retval;
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
                    return -RIG_EPROTO;
                }
            }

            if (i != lvl)
            {
                return -RIG_EINTERNAL;
            }

            val->i = rig->state.attenuator[i - 1];
        }

        break;

    case RIG_LEVEL_PREAMP:
        retval = elad_safe_transaction(rig, "PA", lvlbuf, 50, 3);

        if (retval != RIG_OK)
        {
            return retval;
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
                    return -RIG_EPROTO;
                }
            }

            if (i != lvl)
            {
                return -RIG_EINTERNAL;
            }

            val->i = rig->state.preamp[i - 1];
        }
        else
        {
            rig_debug(RIG_DEBUG_ERR, "%s: "
                      "unexpected preamp char '%c'\n",
                      __func__, lvlbuf[2]);
            return -RIG_EPROTO;
        }

        break;

    case RIG_LEVEL_RFPOWER:
        /*
         * an answer "PC100" means 100 Watt
         * which is val=1.0 on most rigs, but
         * get_elad_level maps 0...255 onto 0.0 ... 1.0
         */
        ret = get_elad_level(rig, "PC", &val->f);
        val->f = val->f * (255.0 / 100.0);
        return ret;

    case RIG_LEVEL_AF:
        return get_elad_level(rig, "AG", &val->f);

    case RIG_LEVEL_RF:
        return get_elad_level(rig, "RG", &val->f);

    case RIG_LEVEL_SQL:
        return get_elad_level(rig, "SQ", &val->f);

    case RIG_LEVEL_MICGAIN:
        return get_elad_level(rig, "MG", &val->f);

    case RIG_LEVEL_AGC:
        ret = get_elad_level(rig, "GT", &val->f);
        agclevel = 255 * val->f;

        if (agclevel == 0) { val->i = 0; }
        else if (agclevel < 85) { val->i = 1; }
        else if (agclevel < 170) { val->i = 2; }
        else if (agclevel <= 255) { val->i = 3; }

        return ret;

    case RIG_LEVEL_SLOPE_LOW:
        retval = elad_transaction(rig, "SL", lvlbuf, sizeof(lvlbuf));

        if (retval != RIG_OK)
        {
            return retval;
        }

        val->i = atoi(&lvlbuf[2]);
        break;

    case RIG_LEVEL_SLOPE_HIGH:
        retval = elad_transaction(rig, "SH", lvlbuf, sizeof(lvlbuf));

        if (retval != RIG_OK)
        {
            return retval;
        }

        val->i = atoi(&lvlbuf[2]);
        break;

    case RIG_LEVEL_CWPITCH:
        retval = elad_safe_transaction(rig, "PT", lvlbuf, 50, 4);

        if (retval != RIG_OK)
        {
            return retval;
        }

        sscanf(lvlbuf + 2, "%d", &val->i);
        val->i = (val->i * 1000) + 1000; /* 00 - 08 */
        break;

    case RIG_LEVEL_KEYSPD:
        retval = elad_safe_transaction(rig, "KS", lvlbuf, 50, 5);

        if (retval != RIG_OK)
        {
            return retval;
        }

        sscanf(lvlbuf + 2, "%d", &val->i);
        break;

    case RIG_LEVEL_IF:
    case RIG_LEVEL_APF:
    case RIG_LEVEL_NR:
    case RIG_LEVEL_PBT_IN:
    case RIG_LEVEL_PBT_OUT:
    case RIG_LEVEL_NOTCHF:
    case RIG_LEVEL_COMP:
    case RIG_LEVEL_BKINDL:
    case RIG_LEVEL_BALANCE:
        return -RIG_ENIMPL;

    default:
        rig_debug(RIG_DEBUG_ERR, "Unsupported get_level %s", rig_strlevel(level));
        return -RIG_EINVAL;
    }

    return RIG_OK;
}

int elad_set_func(RIG *rig, vfo_t vfo, setting_t func, int status)
{
    char buf[6]; /* longest cmd is GTxxx */

    rig_debug(RIG_DEBUG_VERBOSE, "%s called\n", __func__);

    switch (func)
    {
    case RIG_FUNC_NB:
        SNPRINTF(buf, sizeof(buf), "NB%c", (status == 0) ? '0' : '1');
        return elad_transaction(rig, buf, NULL, 0);

    case RIG_FUNC_ABM:
        SNPRINTF(buf, sizeof(buf), "AM%c", (status == 0) ? '0' : '1');
        return elad_transaction(rig, buf, NULL, 0);

    case RIG_FUNC_COMP:
        SNPRINTF(buf, sizeof(buf), "PR%c", (status == 0) ? '0' : '1');
        return elad_transaction(rig, buf, NULL, 0);

    case RIG_FUNC_TONE:
        SNPRINTF(buf, sizeof(buf), "TO%c", (status == 0) ? '0' : '1');
        return elad_transaction(rig, buf, NULL, 0);

    case RIG_FUNC_TSQL:
        SNPRINTF(buf, sizeof(buf), "CT%c", (status == 0) ? '0' : '1');
        return elad_transaction(rig, buf, NULL, 0);

    case RIG_FUNC_VOX:
        SNPRINTF(buf, sizeof(buf), "VX%c", (status == 0) ? '0' : '1');
        return elad_transaction(rig, buf, NULL, 0);

    case RIG_FUNC_FAGC:
        SNPRINTF(buf, sizeof(buf), "GT00%c", (status == 0) ? '4' : '2');
        return elad_transaction(rig, buf, NULL, 0);

    case RIG_FUNC_NR:
        SNPRINTF(buf, sizeof(buf), "NR%c", (status == 0) ? '0' : '1');
        return elad_transaction(rig, buf, NULL, 0);

    case RIG_FUNC_BC:
        SNPRINTF(buf, sizeof(buf), "BC%c", (status == 0) ? '0' : '1');
        return elad_transaction(rig, buf, NULL, 0);

    case RIG_FUNC_ANF:
        SNPRINTF(buf, sizeof(buf), "NT%c", (status == 0) ? '0' : '1');
        return elad_transaction(rig, buf, NULL, 0);

    case RIG_FUNC_LOCK:
        SNPRINTF(buf, sizeof(buf), "LK%c", (status == 0) ? '0' : '1');
        return elad_transaction(rig, buf, NULL, 0);

    case RIG_FUNC_AIP:
        SNPRINTF(buf, sizeof(buf), "MX%c", (status == 0) ? '0' : '1');
        return elad_transaction(rig, buf, NULL, 0);

    case RIG_FUNC_RIT:
        SNPRINTF(buf, sizeof(buf), "RT%c", (status == 0) ? '0' : '1');
        return elad_transaction(rig, buf, NULL, 0);

    case RIG_FUNC_XIT:
        SNPRINTF(buf, sizeof(buf), "XT%c", (status == 0) ? '0' : '1');
        return elad_transaction(rig, buf, NULL, 0);

    default:
        rig_debug(RIG_DEBUG_ERR, "Unsupported set_func %s", rig_strfunc(func));
        return -RIG_EINVAL;
    }

    return -RIG_EINVAL;
}

/*
 * works for any 'format 1' command
 * answer is always 4 bytes: two byte command id, status and terminator
 */
int get_elad_func(RIG *rig, const char *cmd, int *status)
{
    int retval;
    char buf[10];

    rig_debug(RIG_DEBUG_VERBOSE, "%s called\n", __func__);

    if (!cmd || !status)
    {
        return -RIG_EINVAL;
    }

    retval = elad_safe_transaction(rig, cmd, buf, 10, 3);

    if (retval != RIG_OK)
    {
        return retval;
    }

    *status = buf[2] == '0' ? 0 : 1;

    return RIG_OK;
};

/*
 * elad_get_func
 */
int elad_get_func(RIG *rig, vfo_t vfo, setting_t func, int *status)
{
    char fctbuf[20];
    int retval;

    rig_debug(RIG_DEBUG_VERBOSE, "%s called\n", __func__);

    if (!status)
    {
        return -RIG_EINVAL;
    }

    switch (func)
    {
    case RIG_FUNC_FAGC:
        retval = elad_safe_transaction(rig, "GT", fctbuf, 20, 5);

        if (retval != RIG_OK)
        {
            return retval;
        }

        *status = fctbuf[4] != '4' ? 1 : 0;
        return RIG_OK;

    case RIG_FUNC_NB:
        return get_elad_func(rig, "NB", status);

    case RIG_FUNC_ABM:
        return get_elad_func(rig, "AM", status);

    case RIG_FUNC_COMP:
        return get_elad_func(rig, "PR", status);

    case RIG_FUNC_TONE:
        return get_elad_func(rig, "TO", status);

    case RIG_FUNC_TSQL:
        return get_elad_func(rig, "CT", status);

    case RIG_FUNC_VOX:
        return get_elad_func(rig, "VX", status);

    case RIG_FUNC_NR:
        return get_elad_func(rig, "NR", status);

    /* FIXME on TS2000 */
    case RIG_FUNC_BC:
        return get_elad_func(rig, "BC", status);

    case RIG_FUNC_ANF:
        return get_elad_func(rig, "NT", status);

    case RIG_FUNC_LOCK:
        return get_elad_func(rig, "LK", status);

    case RIG_FUNC_AIP:
        return get_elad_func(rig, "MX", status);

    default:
        rig_debug(RIG_DEBUG_ERR, "Unsupported get_func %s", rig_strfunc(func));
        return -RIG_EINVAL;
    }

    return -RIG_EINVAL;
}

/*
 * elad_set_ctcss_tone
 * Assumes rig->caps->ctcss_list != NULL
 *
 * Warning! This is untested stuff! May work at least on TS-870S
 *  Please owners report to me <fillods@users.sourceforge.net>, thanks. --SF
 */
int elad_set_ctcss_tone(RIG *rig, vfo_t vfo, tone_t tone)
{
    const struct rig_caps *caps;
    char tonebuf[16];
    int i;

    rig_debug(RIG_DEBUG_VERBOSE, "%s called\n", __func__);

    caps = rig->caps;

    /* TODO: replace 200 by something like RIGTONEMAX */
    for (i = 0; caps->ctcss_list[i] != 0; i++)
    {
        if (caps->ctcss_list[i] == tone)
        {
            break;
        }
    }

    if (caps->ctcss_list[i] != tone)
    {
        return -RIG_EINVAL;
    }

    /* TODO: replace menu no 57 by a define */
    SNPRINTF(tonebuf, sizeof(tonebuf), "EX%03d%04d", 57, i + 1);

    return elad_transaction(rig, tonebuf, NULL, 0);
}

int elad_set_ctcss_tone_tn(RIG *rig, vfo_t vfo, tone_t tone)
{
    const struct rig_caps *caps = rig->caps;
    char buf[16];
    int i;

    rig_debug(RIG_DEBUG_VERBOSE, "%s called\n", __func__);

    /* XXX 40 is a fixed constant */
    for (i = 0; caps->ctcss_list[i] != 0; i++)
    {
        if (tone == caps->ctcss_list[i])
        {
            break;
        }
    }

    if (tone != caps->ctcss_list[i])
    {
        return -RIG_EINVAL;
    }

    if (RIG_MODEL_TS990S == rig->caps->rig_model)
    {
        char c;

        if (RIG_VFO_CURR == vfo || RIG_VFO_VFO == vfo)
        {
            int err;

            if (RIG_OK != (err = elad_get_vfo_main_sub(rig, &vfo)))
            {
                return err;
            }
        }

        switch (vfo)
        {
        case RIG_VFO_MAIN: c = '0'; break;

        case RIG_VFO_SUB: c = '1'; break;

        default:
            rig_debug(RIG_DEBUG_ERR, "%s: unsupported VFO %s\n", __func__, rig_strvfo(vfo));
            return -RIG_EINVAL;
        }

        SNPRINTF(buf, sizeof(buf), "TN%c%02d", c, i + 1);
    }
    else
    {
        SNPRINTF(buf, sizeof(buf), "TN%02d", i + 1);
    }

    return elad_transaction(rig, buf, NULL, 0);
}

/*
 * elad_get_ctcss_tone
 * Assumes rig->state.priv != NULL
 */
int elad_get_ctcss_tone(RIG *rig, vfo_t vfo, tone_t *tone)
{
    struct elad_priv_data *priv = rig->state.priv;
    const struct rig_caps *caps;
    char tonebuf[3];
    int i, retval;
    unsigned int tone_idx;

    rig_debug(RIG_DEBUG_VERBOSE, "%s called\n", __func__);

    caps = rig->caps;

    if (RIG_MODEL_TS990S == caps->rig_model)
    {
        char cmd[4];
        char buf[6];
        char c;

        if (RIG_VFO_CURR == vfo || RIG_VFO_VFO == vfo)
        {
            if (RIG_OK != (retval = elad_get_vfo_main_sub(rig, &vfo)))
            {
                return retval;
            }
        }

        switch (vfo)
        {
        case RIG_VFO_MAIN: c = '0'; break;

        case RIG_VFO_SUB: c = '1'; break;

        default:
            rig_debug(RIG_DEBUG_ERR, "%s: unsupported VFO %s\n", __func__, rig_strvfo(vfo));
            return -RIG_EINVAL;
        }

        SNPRINTF(cmd, sizeof(cmd), "TN%c", c);
        retval = elad_safe_transaction(rig, cmd, buf, sizeof(buf), 5);
        memcpy(tonebuf, &buf[3], 2);
    }
    else
    {
        retval = elad_get_if(rig);
        memcpy(tonebuf, &priv->info[34], 2);
    }

    if (retval != RIG_OK)
    {
        return retval;
    }

    tonebuf[2] = '\0';
    tone_idx = atoi(tonebuf);

    if (tone_idx == 0)
    {
        rig_debug(RIG_DEBUG_ERR, "%s: CTCSS tone is zero (%s)\n",
                  __func__, tonebuf);
        return -RIG_EPROTO;
    }

    /* check this tone exists. That's better than nothing. */
    for (i = 0; i < tone_idx; i++)
    {
        if (caps->ctcss_list[i] == 0)
        {
            rig_debug(RIG_DEBUG_ERR, "%s: CTCSS NG (%04u)\n",
                      __func__, tone_idx);
            return -RIG_EPROTO;
        }
    }

    *tone = caps->ctcss_list[tone_idx - 1];

    return RIG_OK;
}

int elad_set_ctcss_sql(RIG *rig, vfo_t vfo, tone_t tone)
{
    const struct rig_caps *caps = rig->caps;
    char buf[16];
    int i;

    rig_debug(RIG_DEBUG_VERBOSE, "%s called\n", __func__);

    for (i = 0; caps->ctcss_list[i] != 0; i++)
    {
        if (tone == caps->ctcss_list[i])
        {
            break;
        }
    }

    if (tone != caps->ctcss_list[i])
    {
        return -RIG_EINVAL;
    }

    if (RIG_MODEL_TS990S == rig->caps->rig_model)
    {
        char c;

        if (RIG_VFO_CURR == vfo || RIG_VFO_VFO == vfo)
        {
            int err;

            if (RIG_OK != (err = elad_get_vfo_main_sub(rig, &vfo)))
            {
                return err;
            }
        }

        switch (vfo)
        {
        case RIG_VFO_MAIN: c = '0'; break;

        case RIG_VFO_SUB: c = '1'; break;

        default:
            rig_debug(RIG_DEBUG_ERR, "%s: unsupported VFO %s\n", __func__, rig_strvfo(vfo));
            return -RIG_EINVAL;
        }

        SNPRINTF(buf, sizeof(buf), "CN%c%02d", c, i + 1);
    }
    else
    {
        SNPRINTF(buf, sizeof(buf), "CN%02d", i + 1);
    }

    return elad_transaction(rig, buf, NULL, 0);
}

int elad_get_ctcss_sql(RIG *rig, vfo_t vfo, tone_t *tone)
{
    const struct rig_caps *caps;
    char cmd[4];
    char tonebuf[6];
    int offs;
    int i, retval;
    unsigned int tone_idx;

    rig_debug(RIG_DEBUG_VERBOSE, "%s called\n", __func__);

    caps = rig->caps;

    if (RIG_MODEL_TS990S == rig->caps->rig_model)
    {
        char c;

        if (RIG_VFO_CURR == vfo || RIG_VFO_VFO == vfo)
        {
            if (RIG_OK != (retval = elad_get_vfo_main_sub(rig, &vfo)))
            {
                return retval;
            }
        }

        switch (vfo)
        {
        case RIG_VFO_MAIN: c = '0'; break;

        case RIG_VFO_SUB: c = '1'; break;

        default:
            rig_debug(RIG_DEBUG_ERR, "%s: unsupported VFO %s\n", __func__, rig_strvfo(vfo));
            return -RIG_EINVAL;
        }

        SNPRINTF(cmd, sizeof(cmd), "CN%c", c);
        offs = 3;
    }
    else
    {
        SNPRINTF(cmd, sizeof(cmd), "CT");
        offs = 2;
    }

    retval = elad_safe_transaction(rig, cmd, tonebuf, 6, offs + 2);

    if (retval != RIG_OK)
    {
        return retval;
    }

    tone_idx = atoi(tonebuf + offs);

    if (tone_idx == 0)
    {
        rig_debug(RIG_DEBUG_ERR, "%s: CTCSS is zero (%s)\n",
                  __func__, tonebuf);
        return -RIG_EPROTO;
    }

    /* check this tone exists. That's better than nothing. */
    for (i = 0; i < tone_idx; i++)
    {
        if (caps->ctcss_list[i] == 0)
        {
            rig_debug(RIG_DEBUG_ERR, "%s: CTCSS NG (%04u)\n",
                      __func__, tone_idx);
            return -RIG_EPROTO;
        }
    }

    *tone = caps->ctcss_list[tone_idx - 1];

    return RIG_OK;
}


/*
 * set the aerial/antenna to use
 */
int elad_set_ant(RIG *rig, vfo_t vfo, ant_t ant, value_t option)
{
    char cmd[8];
    char a;

    rig_debug(RIG_DEBUG_VERBOSE, "%s called\n", __func__);

    switch (ant)
    {
    case RIG_ANT_1: a = '1'; break;

    case RIG_ANT_2: a = '2'; break;

    case RIG_ANT_3: a = '3'; break;

    case RIG_ANT_4: a = '4'; break;

    default:
        return -RIG_EINVAL;
    }

    if (RIG_MODEL_TS990S == rig->caps->rig_model)
    {
        char c;

        if (RIG_VFO_CURR == vfo || RIG_VFO_VFO == vfo)
        {
            int err;

            if (RIG_OK != (err = elad_get_vfo_main_sub(rig, &vfo)))
            {
                return err;
            }
        }

        switch (vfo)
        {
        case RIG_VFO_MAIN: c = '0'; break;

        case RIG_VFO_SUB: c = '1'; break;

        default:
            rig_debug(RIG_DEBUG_ERR, "%s: unsupported VFO %s\n", __func__, rig_strvfo(vfo));
            return -RIG_EINVAL;
        }

        SNPRINTF(cmd, sizeof(cmd), "AN0%c%c99", c, a);
    }
    else
    {
        SNPRINTF(cmd, sizeof(cmd), "AN%c", a);
    }

    return elad_transaction(rig, cmd, NULL, 0);
}

int elad_set_ant_no_ack(RIG *rig, vfo_t vfo, ant_t ant)
{
    const char *cmd;

    rig_debug(RIG_DEBUG_VERBOSE, "%s called\n", __func__);

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
        return -RIG_EINVAL;
    }

    return elad_transaction(rig, cmd, NULL, 0);
}

/*
 * get the aerial/antenna in use
 */
int elad_get_ant(RIG *rig, vfo_t vfo, ant_t dummy, value_t *option,
                 ant_t *ant_curr, ant_t *ant_tx, ant_t *ant_rx)
{
    char ackbuf[8];
    int offs;
    int retval;

    rig_debug(RIG_DEBUG_VERBOSE, "%s called\n", __func__);

    if (RIG_MODEL_TS990S == rig->caps->rig_model)
    {
        retval = elad_safe_transaction(rig, "AN0", ackbuf, sizeof(ackbuf), 7);
        offs = 4;
    }
    else
    {
        retval = elad_safe_transaction(rig, "AN", ackbuf, sizeof(ackbuf), 3);
        offs = 2;
    }

    if (retval != RIG_OK)
    {
        return retval;
    }

    if (ackbuf[offs] < '1' || ackbuf[offs] > '9')
    {
        return -RIG_EPROTO;
    }

    *ant_curr = RIG_ANT_N(ackbuf[offs] - '1');

    /* XXX check that the returned antenna is valid for the current rig */

    return RIG_OK;
}

/*
 * elad_get_ptt
 */
int elad_get_ptt(RIG *rig, vfo_t vfo, ptt_t *ptt)
{
    struct elad_priv_data *priv = rig->state.priv;
    int retval;

    rig_debug(RIG_DEBUG_VERBOSE, "%s called\n", __func__);

    retval = elad_get_if(rig);

    if (retval != RIG_OK)
    {
        return retval;
    }

    *ptt = priv->info[28] == '0' ? RIG_PTT_OFF : RIG_PTT_ON;

    return RIG_OK;
}

int elad_set_ptt(RIG *rig, vfo_t vfo, ptt_t ptt)
{
    const char *ptt_cmd;
    char busybuf[10];

    rig_debug(RIG_DEBUG_VERBOSE, "%s called\n", __func__);

    switch (ptt)
    {
    case RIG_PTT_ON:      ptt_cmd = "TX"; break;

    case RIG_PTT_ON_MIC:  ptt_cmd = "TX0"; break;

    case RIG_PTT_ON_DATA: ptt_cmd = "TX1"; break;

    case RIG_PTT_OFF: ptt_cmd = "RX"; break;

    default: return -RIG_EINVAL;
    }

    return elad_transaction(rig, ptt_cmd, busybuf, 4);
}

int elad_set_ptt_safe(RIG *rig, vfo_t vfo, ptt_t ptt)
{
    char busybuf[10];
    int err;
    ptt_t current_ptt;

    rig_debug(RIG_DEBUG_VERBOSE, "%s called\n", __func__);

    err = elad_get_ptt(rig, vfo, &current_ptt);

    if (err != RIG_OK)
    {
        return err;
    }

    if (current_ptt == ptt)
    {
        return RIG_OK;
    }

    return elad_transaction(rig,
                            (ptt == RIG_PTT_ON) ? "TX" : "RX", busybuf, 4);
}


/*
 * elad_get_dcd
 */
int elad_get_dcd(RIG *rig, vfo_t vfo, dcd_t *dcd)
{
    char busybuf[10];
    int retval;
    int offs = 2;


    rig_debug(RIG_DEBUG_VERBOSE, "%s called\n", __func__);

    retval = elad_safe_transaction(rig, "BY", busybuf, 10, 3);

    if (retval != RIG_OK)
    {
        return retval;
    }

    if (RIG_MODEL_TS990S == rig->caps->rig_model && RIG_VFO_SUB == vfo)
    {
        offs = 3;
    }

    *dcd = (busybuf[offs] == '1') ? RIG_DCD_ON : RIG_DCD_OFF;

    return RIG_OK;
}

/*
 * elad_set_trn
 */
int elad_set_trn(RIG *rig, int trn)
{
    rig_debug(RIG_DEBUG_VERBOSE, "%s called\n", __func__);

    if (RIG_MODEL_TS990S == rig->caps->rig_model)
    {
        return elad_transaction(rig, (trn == RIG_TRN_RIG) ? "AI2" : "AI0", NULL, 0);
    }
    else
    {
        return elad_transaction(rig, (trn == RIG_TRN_RIG) ? "AI1" : "AI0", NULL, 0);
    }
}

/*
 * elad_get_trn
 */
int elad_get_trn(RIG *rig, int *trn)
{
    char trnbuf[6];
    int retval;

    rig_debug(RIG_DEBUG_VERBOSE, "%s called\n", __func__);

    if (!trn)
    {
        return -RIG_EINVAL;
    }

    /* these rigs only have AI[0|1] set commands and no AI query */
    if (rig->caps->rig_model == RIG_MODEL_TS450S
            || rig->caps->rig_model == RIG_MODEL_TS690S
            || rig->caps->rig_model == RIG_MODEL_TS790
            || rig->caps->rig_model == RIG_MODEL_TS850
            || rig->caps->rig_model == RIG_MODEL_TS950SDX)
    {
        return -RIG_ENAVAIL;
    }

    retval = elad_safe_transaction(rig, "AI", trnbuf, 6, 3);

    if (retval != RIG_OK)
    {
        return retval;
    }

    *trn = trnbuf[2] != '0' ? RIG_TRN_RIG : RIG_TRN_OFF;

    return RIG_OK;
}

/*
 * elad_set_powerstat
 */
int elad_set_powerstat(RIG *rig, powerstat_t status)
{
    rig_debug(RIG_DEBUG_VERBOSE, "%s called\n", __func__);

    return elad_transaction(rig, (status == RIG_POWER_ON) ? "PS1" : "PS0", NULL, 0);
}

/*
 * elad_get_powerstat
 */
int elad_get_powerstat(RIG *rig, powerstat_t *status)
{
    char pwrbuf[6];
    int retval;

    rig_debug(RIG_DEBUG_VERBOSE, "%s called\n", __func__);

    if (!status)
    {
        return -RIG_EINVAL;
    }

    retval = elad_safe_transaction(rig, "PS", pwrbuf, 6, 3);

    if (retval != RIG_OK)
    {
        return retval;
    }

    *status = pwrbuf[2] == '0' ? RIG_POWER_OFF : RIG_POWER_ON;

    return RIG_OK;
}

/*
 * elad_reset
 */
int elad_reset(RIG *rig, reset_t reset)
{
    char rstbuf[6];
    char rst;

    rig_debug(RIG_DEBUG_VERBOSE, "%s called\n", __func__);

    if (RIG_MODEL_TS990S == rig->caps->rig_model)
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
            return -RIG_EINVAL;
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
            return -RIG_EINVAL;
        }
    }

    SNPRINTF(rstbuf, sizeof(rstbuf), "SR%c", rst);

    /* this command has no answer */
    return elad_transaction(rig, rstbuf, NULL, 0);
}

/*
 * elad_send_morse
 */
int elad_send_morse(RIG *rig, vfo_t vfo, const char *msg)
{
    char morsebuf[40], m2[30];
    int msg_len, retval, i;
    const char *p;

    rig_debug(RIG_DEBUG_VERBOSE, "%s called\n", __func__);

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
            retval = elad_transaction(rig, "KY;", m2, 4);

            if (retval != RIG_OK)
            {
                return retval;
            }

            /*
             * If answer is "KY0;", there is space in buffer and we can proceed.
             * If answer is "KY1;", we have to wait a while
             * If answer is something else, return with error to prevent infinite loops
             */
            if (!strncmp(m2, "KY0", 3)) { break; }

            if (!strncmp(m2, "KY1", 3)) { hl_usleep(500000); }
            else { return -RIG_EINVAL; }
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

        retval = elad_transaction(rig, morsebuf, NULL, 0);

        if (retval != RIG_OK)
        {
            return retval;
        }

        msg_len -= buff_len;
        p += buff_len;
    }

    return RIG_OK;
}

/*
 * elad_vfo_op
 */
int elad_vfo_op(RIG *rig, vfo_t vfo, vfo_op_t op)
{
    rig_debug(RIG_DEBUG_VERBOSE, "%s called\n", __func__);

    switch (op)
    {
    case RIG_OP_UP:
        return elad_transaction(rig, "UP", NULL, 0);

    case RIG_OP_DOWN:
        return elad_transaction(rig, "DN", NULL, 0);

    case RIG_OP_BAND_UP:
        return elad_transaction(rig, "BU", NULL, 0);

    case RIG_OP_BAND_DOWN:
        return elad_transaction(rig, "BD", NULL, 0);

    default:
        rig_debug(RIG_DEBUG_ERR, "%s: unsupported op %#x\n",
                  __func__, op);
        return -RIG_EINVAL;
    }
}

/*
 * elad_set_mem
 */
int elad_set_mem(RIG *rig, vfo_t vfo, int ch)
{
    char buf[7];

    rig_debug(RIG_DEBUG_VERBOSE, "%s called\n", __func__);

    if (RIG_MODEL_TS990S == rig->caps->rig_model)
    {
        char c;

        if (RIG_VFO_CURR == vfo || RIG_VFO_VFO == vfo)
        {
            int err;

            if (RIG_OK != (err = elad_get_vfo_main_sub(rig, &vfo)))
            {
                return err;
            }
        }

        switch (vfo)
        {
        case RIG_VFO_MAIN: c = '0'; break;

        case RIG_VFO_SUB: c = '1'; break;

        default:
            rig_debug(RIG_DEBUG_ERR, "%s: unsupported VFO %s\n", __func__, rig_strvfo(vfo));
            return -RIG_EINVAL;
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

    return elad_transaction(rig, buf, NULL, 0);
}

/*
 * elad_get_mem
 */
int elad_get_mem(RIG *rig, vfo_t vfo, int *ch)
{
    char cmd[4];
    char membuf[10];
    int offs;
    int retval;

    rig_debug(RIG_DEBUG_VERBOSE, "%s called\n", __func__);

    if (RIG_MODEL_TS990S == rig->caps->rig_model)
    {
        char c;

        if (RIG_VFO_CURR == vfo || RIG_VFO_VFO == vfo)
        {
            if (RIG_OK != (retval = elad_get_vfo_main_sub(rig, &vfo)))
            {
                return retval;
            }
        }

        switch (vfo)
        {
        case RIG_VFO_MAIN: c = '0'; break;

        case RIG_VFO_SUB: c = '1'; break;

        default:
            rig_debug(RIG_DEBUG_ERR, "%s: unsupported VFO %s\n", __func__, rig_strvfo(vfo));
            return -RIG_EINVAL;
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

    retval = elad_safe_transaction(rig, cmd, membuf, sizeof(membuf), 3 + offs);

    if (retval != RIG_OK)
    {
        return retval;
    }

    *ch = atoi(membuf + offs);

    return RIG_OK;
}

int elad_get_mem_if(RIG *rig, vfo_t vfo, int *ch)
{
    int err;
    char buf[4];
    struct elad_priv_data *priv = rig->state.priv;

    rig_debug(RIG_DEBUG_VERBOSE, "%s called\n", __func__);

    err = elad_get_if(rig);

    if (err != RIG_OK)
    {
        return err;
    }

    memcpy(buf, &priv->info[26], 2);
    buf[2] = '\0';

    *ch = atoi(buf);

    return RIG_OK;
}

int elad_get_channel(RIG *rig, channel_t *chan)
{
    int err;
    char buf[26];
    char cmd[8];
    char bank = ' ';
    struct elad_priv_caps *caps = elad_caps(rig);

    rig_debug(RIG_DEBUG_VERBOSE, "%s called\n", __func__);

    /* put channel num in the command string */

    if (rig->caps->rig_model == RIG_MODEL_TS940)
    {
        bank = '0' + chan->bank_num;
    }

    SNPRINTF(cmd, sizeof(cmd), "MR0%c%02d", bank, chan->channel_num);

    err = elad_safe_transaction(rig, cmd, buf, 26, 23);

    if (err != RIG_OK)
    {
        return err;
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

    chan->mode = elad2rmode(buf[17] - '0', caps->mode_table);

    buf[17] = '\0';
    chan->freq = atoi(&buf[6]);

    if (chan->freq == RIG_FREQ_NONE)
    {
        return -RIG_ENAVAIL;
    }

    buf[6] = '\0';
    chan->channel_num = atoi(&buf[4]);

    if (buf[3] >= '0' && buf[3] <= '9')
    {
        chan->bank_num = buf[3] - '0';
    }

    /* split freq */
    cmd[2] = '1';
    err = elad_safe_transaction(rig, cmd, buf, 26, 23);

    if (err != RIG_OK)
    {
        return err;
    }

    chan->tx_mode = elad2rmode(buf[17] - '0', caps->mode_table);

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

    return RIG_OK;
}

int elad_set_channel(RIG *rig, const channel_t *chan)
{
    char buf[128];
    char mode, tx_mode = 0;
    int err;
    int tone = 0;
    char bank = ' ';
    struct elad_priv_caps *caps = elad_caps(rig);

    rig_debug(RIG_DEBUG_VERBOSE, "%s called\n", __func__);

    if (!chan)
    {
        return -RIG_EINVAL;
    }

    mode = rmode2elad(chan->mode, caps->mode_table);

    if (mode < 0)
    {
        rig_debug(RIG_DEBUG_ERR, "%s: unsupported mode '%s'\n",
                  __func__, rig_strrmode(chan->mode));
        return -RIG_EINVAL;
    }

    if (chan->split == RIG_SPLIT_ON)
    {
        tx_mode = rmode2elad(chan->tx_mode, caps->mode_table);

        if (tx_mode < 0)
        {
            rig_debug(RIG_DEBUG_ERR, "%s: unsupported mode '%s'\n",
                      __func__, rig_strrmode(chan->tx_mode));
            return -RIG_EINVAL;
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

    if (rig->caps->rig_model == RIG_MODEL_TS940)
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

    err = elad_transaction(rig, buf, NULL, 0);

    if (err != RIG_OK)
    {
        return err;
    }

    SNPRINTF(buf, sizeof(buf), "MW1%c%02d%011"PRIll"%c%c%c%02d ",
             bank,
             chan->channel_num,
             (int64_t)(chan->split == RIG_SPLIT_ON ? chan->tx_freq : 0),
             (chan->split == RIG_SPLIT_ON) ? ('0' + tx_mode) : '0',
             (chan->flags & RIG_CHFLAG_SKIP) ? '1' : '0',
             chan->ctcss_tone ? '1' : '0',
             chan->ctcss_tone ? (tone + 1) : 0);

    return elad_transaction(rig, buf, NULL, 0);
}

int elad_set_ext_parm(RIG *rig, token_t token, value_t val)
{
    char buf[4];

    rig_debug(RIG_DEBUG_VERBOSE, "%s called\n", __func__);

    switch (token)
    {
    case TOK_VOICE:
        return elad_transaction(rig, "VR", NULL, 0);

    case TOK_FINE:
        SNPRINTF(buf, sizeof(buf), "FS%c", (val.i == 0) ? '0' : '1');
        return elad_transaction(rig, buf, NULL, 0);

    case TOK_XIT:
        SNPRINTF(buf, sizeof(buf), "XT%c", (val.i == 0) ? '0' : '1');
        return elad_transaction(rig, buf, NULL, 0);

    case TOK_RIT:
        SNPRINTF(buf, sizeof(buf), "RT%c", (val.i == 0) ? '0' : '1');
        return elad_transaction(rig, buf, NULL, 0);
    }

    return -RIG_EINVAL;
}

int elad_get_ext_parm(RIG *rig, token_t token, value_t *val)
{
    int err;
    struct elad_priv_data *priv = rig->state.priv;

    rig_debug(RIG_DEBUG_VERBOSE, "%s called\n", __func__);

    switch (token)
    {
    case TOK_FINE:
        return get_elad_func(rig, "FS", &val->i);

    case TOK_XIT:
        err = elad_get_if(rig);

        if (err != RIG_OK)
        {
            return err;
        }

        val->i = (priv->info[24] == '1') ? 1 : 0;
        return RIG_OK;

    case TOK_RIT:
        err = elad_get_if(rig);

        if (err != RIG_OK)
        {
            return err;
        }

        val->i = (priv->info[23] == '1') ? 1 : 0;
        return RIG_OK;
    }

    return -RIG_ENIMPL;
}

/*
 * elad_get_info
 * supposed to work only for TS2000...
 */
const char *elad_get_info(RIG *rig)
{
    char firmbuf[10];
    int retval;

    rig_debug(RIG_DEBUG_VERBOSE, "%s called\n", __func__);

    retval = elad_safe_transaction(rig, "TY", firmbuf, 10, 5);

    if (retval != RIG_OK)
    {
        return NULL;
    }

    switch (firmbuf[4])
    {
    case '0': return "Firmware: Overseas type";

    case '1': return "Firmware: Japanese 100W type";

    case '2': return "Firmware: Japanese 20W type";

    default: return "Firmware: unknown";
    }
}

#define IDBUFSZ 16

/*
 * proberigs_elad
 *
 * Notes:
 * There's only one rig possible per port.
 *
 * rig_model_t probeallrigs_elad(port_t *port, rig_probe_func_t cfunc, rig_ptr_t data)
 */
DECLARE_PROBERIG_BACKEND(elad)
{
    char idbuf[IDBUFSZ];
    int id_len = -1, i, k_id;
    int retval = -1;
    int rates[] = { 115200, 57600, 38400, 19200, 9600, 4800, 1200, 0 }; /* possible baud rates */
    int rates_idx;

    rig_debug(RIG_DEBUG_VERBOSE, "%s called\n", __func__);

    if (!port)
    {
        return RIG_MODEL_NONE;
    }

    if (port->type.rig != RIG_PORT_SERIAL)
    {
        return RIG_MODEL_NONE;
    }

    port->write_delay = port->post_write_delay = 0;
    port->parm.serial.stop_bits = 2;
    port->retry = 1;

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
            return RIG_MODEL_NONE;
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
        return RIG_MODEL_NONE;
    }

    /*
     * reply should be something like 'IDxxx;'
     */
    if (id_len != 5 && id_len != 6)
    {
        idbuf[7] = '\0';
        rig_debug(RIG_DEBUG_VERBOSE, "probe_elad: protocol error, "
                  " expected %d, received %d: %s\n",
                  6, id_len, idbuf);
        return RIG_MODEL_NONE;
    }


    /* first, try ID string */
    for (i = 0; elad_id_string_list[i].model != RIG_MODEL_NONE; i++)
    {
        if (!strncmp(elad_id_string_list[i].id, idbuf + 2, 16))
        {
            rig_debug(RIG_DEBUG_VERBOSE, "probe_elad: "
                      "found %s\n", idbuf + 2);

            if (cfunc)
            {
                (*cfunc)(port, elad_id_string_list[i].model, data);
            }

            return elad_id_string_list[i].model;
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
            return RIG_MODEL_NONE;
        }

        retval = write_block(port, (unsigned char *) "K2;", 3);
        id_len = read_string(port, (unsigned char *) idbuf, IDBUFSZ, ";\r", 2, 0, 1);
        close(port->fd);

        if (retval != RIG_OK)
        {
            return RIG_MODEL_NONE;
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

            return RIG_MODEL_K2;
        }
    }

    for (i = 0; elad_id_list[i].model != RIG_MODEL_NONE; i++)
    {
        if (elad_id_list[i].id == k_id)
        {
            rig_debug(RIG_DEBUG_VERBOSE, "probe_elad: "
                      "found %03d\n", k_id);

            if (cfunc)
            {
                (*cfunc)(port, elad_id_list[i].model, data);
            }

            return elad_id_list[i].model;
        }
    }

    /*
     * not found in known table....
     * update elad_id_list[]!
     */
    rig_debug(RIG_DEBUG_WARN, "probe_elad: found unknown device "
              "with ID %03d, please report to Hamlib "
              "developers.\n", k_id);

    rig_debug(RIG_DEBUG_TRACE, "%s: post_write_delay=%d\n", __func__,
              port->post_write_delay);
    return RIG_MODEL_NONE;
}


/*
 * initrigs_elad is called by rig_backend_load
 */
DECLARE_INITRIG_BACKEND(elad)
{
    rig_debug(RIG_DEBUG_VERBOSE, "%s called\n", __func__);

//  rig_debug(RIG_DEBUG_VERBOSE, "elad: _init called\n");

    rig_register(&fdm_duo_caps);

    return RIG_OK;
}
