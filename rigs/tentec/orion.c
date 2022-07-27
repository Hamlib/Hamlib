/*
 *  Hamlib TenTenc backend - TT-565 description
 *  Copyright (c) 2004-2011 by Martin Ewing
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

/* Edits by Martin Ewing AA6E, 23 Mar 2005 --> ??
 * Added valid length settings before tentec_transaction calls.
 * Added vfo_curr initialization to VFO A
 * Fixed up VSWR & S-meter, set ATT, set AGC, add rough STR_CAL func.
 * Use local tt565_transaction due to quirky serial interface
 * Variable-length transaction read ok.
 * Calibrated S-meter response with signal generator.
 * Read re-tries implemented.
 * Added RIG_LEVEL_CWPITCH, RIG_LEVEL_KEYSPD, send_morse()
 * Added RIG_FUNC_TUNER, RIG_FUNC_LOCK and RIG_FUNC_VOX, fixed MEM_CAP.
 * Added VFO_OPS
 * Support LEVEL_VOX, VOXGAIN, ANTIVOX
 * Support LEVEL_NR as Orion NB setting (firmware bug), FUNC_NB -> NB=0,4
 * Add get_, set_ant (ignores rx only ant)
 * Use binary mode for VFO read / write, for speed.
 * November, 2007:
 * Add RIG_LEVEL_STRENGTH capability (should have been there all along)
 * Implement auto-detect of firmware for S-meter cal, etc.
 * Fixed bug in tt565_reset (to send "XX" instead of "X")
 * Filtered rig info string to ensure all graphics.
 * Big reliability improvement (for fldigi, v 2.062a) 2/15/2008
 * Jan., 2009:
 * Remove RIG_LEVEL_STRENGTH, so that frontend can handle it.
 * Dec., 2011: Implement VFO range checking. Adjust range_lists for max coverage.
 * Fix tt565_transaction for case of Morse (/) command.
 */

/* Known issues & to-do list:
 * Memory channels - emulate a more complete memory system?
 * Send_Morse() - needs to buffer more than 20 chars?
 * Figure out "granularities".
 * XCHG or other "fancy" VFO & MEM operations?
 */

/**
 * \addtogroup tentec_orion
 * @{ */

/**
 * \file orion.c
 * \brief Backend for Tentec Orion 565 / 566
 *
 * This backend supports the Ten-Tec Orion (565) and Orion II (566) transceivers.
 * \n This backend tested mostly with firmware versions 1.372 and 2.062a
 */
#include <hamlib/config.h>

#include <stdio.h>
#include <stdlib.h>
#include <ctype.h>
#include <string.h>  /* String function definitions */
#include <math.h>

#include <hamlib/rig.h>
#include "bandplan.h"
#include "serial.h"
#include "misc.h"
#include "idx_builtin.h"
#include "orion.h"
#include <cal.h>

#ifdef TT565_TIME
/**
 * \returns current time in secs/microsecs
 */
double tt565_timenow()  /* returns current time in secs+microsecs */
{
    struct timeval tv;
    gettimeofday(&tv, NULL);
    return (double)tv.tv_sec + ((double)tv.tv_usec) / 1.0e+6;
}
#endif

/**
 * \param rig Rig descriptor
 * \param cmd command to send
 * \param cmd_len length of command string
 * \param data string to receive return data from Orion (NULL if no return desired)
 * \param data_len length of data string
 * \returns RIG_OK or < 0 if error
 * \brief tt565_transaction, adapted from tentec_transaction (tentec.c)
 *
 * This is the basic I/O transaction to/from the Orion.
 * \n Read variable number of bytes, up to buffer size, if data & data_len != NULL.
 * \n We assume that rig!=NULL, rig->state!= NULL.
 * Otherwise, you'll get a nice seg fault. You've been warned!
 */

static int tt565_transaction(RIG *rig, const char *cmd, int cmd_len, char *data,
                             int *data_len)
{
    int data_len_init, itry;
    struct rig_state *rs;
    static int passcount = 0;
#ifdef TT565_TIME
    double ft1, ft2;
#endif
    passcount++;        // for debugging
    /* Capture buffer length for possible read re-try. */
    data_len_init = (data && data_len) ? *data_len : 0;

    /* Allow transaction re-tries according to capabilities. */
    for (itry = 0; itry < rig->caps->retry; itry++)
    {
        int retval;
        rs = &rig->state;
        rig_flush(&rs->rigport); /* discard pending i/p */
        retval = write_block(&rs->rigport, (unsigned char *) cmd, cmd_len);

        if (retval != RIG_OK)
        {
            return retval;
        }

        /* no data expected, TODO: flush input? */
        if (!data || !data_len)
        {
            /* If it's not a 'write' to rig or a Morse command, there must be data. */
            if ((*cmd != '*') && (*cmd != '/'))
            {
                rig_debug(RIG_DEBUG_ERR, "%s: cmd reject 1\n", __func__);
                return -RIG_ERJCTED;
            }

            return RIG_OK;  /* normal exit if write, but no read */
        }

#ifdef TT565_TIME
        ft1 = tt565_timenow();
#endif
        *data_len = data_len_init;  /* restore orig. buffer length */
        *data_len = read_string(&rs->rigport, (unsigned char *) data, *data_len,
                                EOM, strlen(EOM), 0, 1);

        if (!strncmp(data, "Z!", 2))     // command unrecognized??
        {
            rig_debug(RIG_DEBUG_ERR, "%s: cmd reject 2\n", __func__);
            return -RIG_ERJCTED;    // what is a better error return?
        }

        /* XX and ?V are oddball commands.  Thanks, Ten-Tec! */
        if (!strncmp(cmd, "XX", 2)) // Was it a firmware reset cmd?
        {
            return RIG_OK;      // Then we accept the response.
        }

        if (!strncmp(cmd, "?V", 2)) // Was it a read firmware version cmd?
        {
            return RIG_OK;      // ditto
        }

        if (cmd[0] != '?')          // was this a read cmd?
        {
            rig_debug(RIG_DEBUG_ERR, "%s: cmd reject 3\n", __func__);
            return -RIG_ERJCTED;    // No, but it should have been!
        }
        else                            // Yes, it was a 'read', phew!
        {
            if (!strncmp(data + 1, cmd + 1, cmd_len - 2)) //response matches cmd?
            {
                return RIG_OK;  // all is well, normal exit
            }
            else
            {
                /* The command read back does not match the command that
                        was written.  We report the problem if debugging,
                        and issue another read in hopes of eventual success.
                        */
                rig_debug(RIG_DEBUG_WARN,
                          "** retry after delay (io=%d, retry=%d) **\n",
                          passcount, itry);
                *data_len = data_len_init;  /* restore orig. buffer length */
                read_string(&rs->rigport, (unsigned char *) data, *data_len,
                            EOM, strlen(EOM), 0, 1);      // purge the input stream...
                continue;                   // now go retry the full command
            }
        }

#ifdef TT565_TIME
        ft2 = tt565_timenow();

        if (*data_len == -RIG_ETIMEOUT)
            rig_debug(RIG_DEBUG_ERR, "Timeout %d: Elapsed = %f secs.\n",
                      itry, ft2 - ft1);
        else
            rig_debug(RIG_DEBUG_ERR,
                      "Other Error #%d, itry=%d: Elapsed = %f secs.\n",
                      *data_len, itry, ft2 - ft1);

#endif
    }   /* end of itry loop */

    rig_debug(RIG_DEBUG_ERR, "** Ran out of retries io=%d **\n",
              passcount);
    return -RIG_ETIMEOUT;
}

/**
 * \param rig
 * \returns RIG_OK or < 0
 * \brief Basically, it just sets up *priv
 */
int tt565_init(RIG *rig)
{
    struct tt565_priv_data *priv;
    rig->state.priv = (struct tt565_priv_data *)calloc(1, sizeof(
                          struct tt565_priv_data));

    if (!rig->state.priv) { return -RIG_ENOMEM; } /* no memory available */

    priv = rig->state.priv;

    memset(priv, 0, sizeof(struct tt565_priv_data));
    priv->ch = 0; /* set arbitrary initial status */
    priv->vfo_curr = RIG_VFO_A;
    return RIG_OK;
}

/**
 * \param rig
 * \brief tt565_cleanup routine
 *
 * the serial port is closed by the frontend
 */
int tt565_cleanup(RIG *rig)
{
    if (rig->state.priv)
    {
        free(rig->state.priv);
    }

    rig->state.priv = NULL;

    return RIG_OK;
}

/**
 * \param rig
 * \brief tt565_open routine
 *
 * Open the rig - check f * This backend supports the Ten-Tec Orion (565) and Orion II (566) transceivers.
irmware version issues
 */
int tt565_open(RIG *rig)
{
    cal_table_t cal1 = TT565_STR_CAL_V1, cal2 = TT565_STR_CAL_V2;
    char *buf;

    /* Detect version 1 or version 2 firmware. V2 is default. */
    /* The only difference for us is the S-meter cal table */

    /* Get Orion's Version string (?V command response) */
    buf = (char *)tt565_get_info(rig);

    /* Is Orion firmware version 1.* or 2.*? */
    if (!strstr(buf, "1."))
    {
        /* Not v1 means probably v2 */
        memcpy(&rig->state.str_cal, &cal2, sizeof(cal_table_t));
    }
    else
    {
        memcpy(&rig->state.str_cal, &cal1, sizeof(cal_table_t));
    }

    return RIG_OK;
}

/**
 * \param rig
 * \param vfo RIG_VFO_MAIN or RIG_VFO_SUB
 * \returns 'M' or 'S' for main or subreceiver or <0 error
 * \brief vfo must be RIG_VFO_MAIN or RIG_VFO_SUB
 *
 * Note that Orion's "VFO"s are supposed to be logically independent
 * of the main/sub receiver selection.  (In reality, they are not quite
 * independent.)
 */
static char which_receiver(const RIG *rig, vfo_t vfo)
{
    struct tt565_priv_data *priv = (struct tt565_priv_data *)rig->state.priv;

    if (vfo == RIG_VFO_CURR)
    {
        vfo = priv->vfo_curr;
    }

    switch (vfo)
    {
    case RIG_VFO_A:
    case RIG_VFO_B:
    case RIG_VFO_MAIN: return 'M';

    case RIG_VFO_SUB: return 'S';

    default:
        rig_debug(RIG_DEBUG_ERR, "%s: unsupported Receiver %s\n",
                  __func__, rig_strvfo(vfo));
        return -RIG_EINVAL;
    }
}
/**
 * \param rig
 * \param vfo RIG_VFO_A, RIG_VFO_B, or RIG_VFO_NONE
 * \returns 'A' or 'B' or 'N' for VFO A, B, or null VFO, or <0 error
 * \brief vfo must be RIG_VFO_A, RIG_VFO_B, or RIG_VFO_NONE.
 */
static char which_vfo(const RIG *rig, vfo_t vfo)
{
    struct tt565_priv_data *priv = (struct tt565_priv_data *)rig->state.priv;

    if (vfo == RIG_VFO_CURR)
    {
        vfo = priv->vfo_curr;
    }

    switch (vfo)
    {
    case RIG_VFO_A: return 'A';

    case RIG_VFO_B: return 'B';

    case RIG_VFO_NONE: return 'N';

    default:
        rig_debug(RIG_DEBUG_ERR, "%s: unsupported VFO %s\n",
                  __func__, rig_strvfo(vfo));
        return -RIG_EINVAL;
    }
}

/**
 * \param rig must != NULL
 * \param vfo RIG_VFO_A or RIG_VFO_B
 * \param freq
 * \brief Set a frequence into the specified VFO
 *
 * assumes rig->state.priv!=NULL
 * \n assumes priv->mode in AM,CW,LSB or USB.
 */

int tt565_set_freq(RIG *rig, vfo_t vfo, freq_t freq)
{
    int retval, i, in_range;
    freq_range_t this_range;
    char cmdbuf[TT565_BUFSIZE];
    /* Check for valid frequency request.
     * Find freq range that includes current request and
     * matches the VFO A/B setting. c.f. rig_get_range().
     * Recall VFOA = ham only, VFOB = gen coverage for Hamlib.
     * (We assume VFOA = Main RXTX and VFOB = Sub RX.)
     * If outside range, return RIG_ERJECTED for compatibility vs icom.c etc.
     */
    in_range = FALSE;

    for (i = 0; i < HAMLIB_FRQRANGESIZ; i++)
    {
        this_range = rig->state.rx_range_list[i];

        if (this_range.startf == 0 && this_range.endf == 0)
        {
            break;      /* have come to early end of range list */
        }

        /* We don't care about mode setting, but vfo must match. */
        if (freq >= this_range.startf && freq <= this_range.endf &&
                (this_range.vfo == rig->state.current_vfo))
        {
            in_range = TRUE;
            break;
        }
    }

    if (!in_range) { return -RIG_ERJCTED; } /* Sorry, invalid freq request */

#ifdef TT565_ASCII_FREQ
    /*  Use ASCII mode to set frequencies */
    // cppcheck-suppress *
    SNPRINTF(cmdbuf, sizeof(cmdbuf), "*%cF%"PRIll EOM,
             which_vfo(rig, vfo),
             (int64_t)freq);
#else
    /* Use binary mode */
    /* Set frequency using Orion's binary mode (short) sequence.
    The short sequence transfers faster and may require less Orion
    firmware effort, but some bugs are reported. */

    /* Construct command packet by brute force. */
    unsigned int myfreq;
    myfreq = freq;
    cmdbuf[0] = '*';
    cmdbuf[1] = which_vfo(rig, vfo);
    cmdbuf[2] = (myfreq & 0xff000000) >> 24;
    cmdbuf[3] = (myfreq & 0x00ff0000) >> 16;
    cmdbuf[4] = (myfreq & 0x0000ff00) >> 8;
    cmdbuf[5] =  myfreq & 0x000000ff;
    cmdbuf[6] = '\r';   /* i.e. EOM */
    cmdbuf[7] = 0;
#endif
    retval = tt565_transaction(rig, cmdbuf, strlen(cmdbuf), NULL, NULL);

    return retval;
}

/**
 * \param rig must != NULL
 * \param vfo RIG_VFO_A or RIG_VFO_B
 * \param freq must != NULL
 * \brief Get the frequency currently set in the specified VFO (A or B)
 *
 * Performs query on physical rig
 */
int tt565_get_freq(RIG *rig, vfo_t vfo, freq_t *freq)
{
    int resp_len, retval;
    char cmdbuf[TT565_BUFSIZE], respbuf[TT565_BUFSIZE];
    unsigned int binf;
#ifdef TT565_ASCII_FREQ
    /*  use ASCII mode */
    SNPRINTF(cmdbuf, sizeof(cmdbuf), "?%cF" EOM,
             which_vfo(rig, vfo));
#else
    /* Get freq with Orion binary mode short sequence. */
    SNPRINTF(cmdbuf, sizeof(cmdbuf), "?%c" EOM,
             which_vfo(rig, vfo));
#endif
    resp_len = sizeof(respbuf);
    retval = tt565_transaction(rig, cmdbuf, strlen(cmdbuf), respbuf, &resp_len);

    if (retval != RIG_OK)
    {
        return retval;
    }

#ifdef TT565_ASCII_FREQ
    respbuf[12] = '\0';
    sscanf(respbuf + 3, "%8u", &binf);
    *freq = (freq_t) binf;
#else

    /* Test for valid binary mode return. */
    if (respbuf[1] != which_vfo(rig, vfo) || resp_len <= 5)
    {
        rig_debug(RIG_DEBUG_ERR, "%s: unexpected answer '%s'\n",
                  __func__, respbuf);
        return -RIG_EPROTO;
    }

    /* Convert binary to integer, endedness independent */
    binf = (unsigned char)respbuf[2] << 24 | (unsigned char)respbuf[3] << 16
           | (unsigned char)respbuf[4] << 8  | (unsigned char)respbuf[5];
    *freq = (freq_t) binf;
#endif
    return RIG_OK;
}

/**
 * \param rig must != NULL
 * \param vfo RIG_VFO_MAIN or RIG_VFO_SUB
 * \returns RIG_OK or < 0
 * \brief set RIG_VFO_CURR and send info to physical rig.
 *
 * Places Orion into Main or Sub Rx active state
 */
int tt565_set_vfo(RIG *rig, vfo_t vfo)
{
    struct tt565_priv_data *priv = (struct tt565_priv_data *)rig->state.priv;

    if (vfo == RIG_VFO_CURR)
    {
        return RIG_OK;
    }

    if (vfo == RIG_VFO_MAIN || vfo == RIG_VFO_SUB)
    {
        char vfobuf[TT565_BUFSIZE];
        /* Select Sub or Main RX */
        SNPRINTF(vfobuf, sizeof(vfobuf), "*K%c" EOM,
                 vfo == RIG_VFO_SUB ? 'S' : 'M');

        return tt565_transaction(rig, vfobuf, strlen(vfobuf), NULL, NULL);
    }

    priv->vfo_curr = vfo;

    return RIG_OK;
}

/**
 * \param rig must != NULL
 * \param vfo Set = stored state of current VFO state
 * \returns RIG_OK
 */
int tt565_get_vfo(RIG *rig, vfo_t *vfo)
{
    struct tt565_priv_data *priv = (struct tt565_priv_data *)rig->state.priv;

    *vfo = priv->vfo_curr;

    return RIG_OK;
}

/**
 * \param rig must != NULL
 * \param vfo Rx vfo specifier token
 * \param split
 * \param tx_vfo Tx vfo specifier token
 * \returns RIG_OK or < 0
 * \brief Set split operating mode
 *
 * Sets Main Rx to "vfo"( A or B) , Main Tx to "tx_vfo" (A, B, or N).
 * \n Sub Rx is set to "None". That should be fixed!
 */
int tt565_set_split_vfo(RIG *rig, vfo_t vfo, split_t split, vfo_t tx_vfo)
{
    int retval;
    char cmdbuf[TT565_BUFSIZE];

    SNPRINTF(cmdbuf, sizeof(cmdbuf), "*KV%c%c%c" EOM,
             which_vfo(rig, vfo),
             'N',           /* FIXME */
             which_vfo(rig, RIG_SPLIT_ON == split ? tx_vfo : vfo));

    retval = tt565_transaction(rig, cmdbuf, strlen(cmdbuf), NULL, NULL);

    return retval;
}
/**
 * \param c
 * \returns RIG_VFO_x, x= A, B, or NONE
 * \brief Translate an Orion command character to internal token form.
 */
static vfo_t tt2vfo(char c)
{
    switch (c)
    {
    case 'A': return RIG_VFO_A;

    case 'B': return RIG_VFO_B;

    case 'N': return RIG_VFO_NONE;
    }

    return RIG_VFO_NONE;
}

/**
 * \param rig must != NULL
 * \param vfo
 * \param split Returned with RIG_SPLIT_ON if Tx <> Rx vfo, .._OFF otherwise
 * \param tx_vfo Returned RIG_VFO_x, signifying selected Tx vfo
 * \returns RIG_OK or < 0
 * \brief Get the current split status and Tx vfo selection.
 */
int tt565_get_split_vfo(RIG *rig, vfo_t vfo, split_t *split, vfo_t *tx_vfo)
{
    int resp_len, retval;
    char cmdbuf[TT565_BUFSIZE], respbuf[TT565_BUFSIZE];
    char ttreceiver;

    SNPRINTF(cmdbuf, sizeof(cmdbuf), "?KV" EOM);
    resp_len = sizeof(respbuf);
    retval = tt565_transaction(rig, cmdbuf, strlen(cmdbuf), respbuf, &resp_len);

    if (retval != RIG_OK)
    {
        return retval;
    }

    if (respbuf[2] != 'V' || resp_len < 5)
    {
        rig_debug(RIG_DEBUG_ERR, "%s: unexpected answer '%s'\n",
                  __func__, respbuf);
        return -RIG_EPROTO;
    }

    ttreceiver = vfo == RIG_VFO_SUB ? respbuf[4] : respbuf[3];

    *tx_vfo = tt2vfo(respbuf[5]);

    *split = ttreceiver == respbuf[5] ? RIG_SPLIT_OFF : RIG_SPLIT_ON;

    return RIG_OK;
}

/**
 * \param rig must != NULL
 * \param vfo
 * \param mode
 * \param width passband in Hz or = RIG_PASSBAND_NORMAL (=0) which gives a nominal value
 * \brief Set operating mode to RIG_MODE_x with indicated passband width.
 *
 * Supported modes x= USB, LSB, CW, CWR, AM, FM, RTTY
 * \n This applies to currently selected receiver (Main Rx=Tx or Sub Rx)
 * \sa tt565_set_vfo
 *
 * \remarks Note widespread confusion between "VFO" and "Receiver".  The Orion
 * has VFOs A and B which may be mapped to Main and Sub Receivers independently.
 * But Hamlib may have different ideas!
 */
int tt565_set_mode(RIG *rig, vfo_t vfo, rmode_t mode, pbwidth_t width)
{
    struct rig_state *rs = &rig->state;
    char ttmode, ttreceiver;
    int retval;
    char mdbuf[TT565_BUFSIZE];

    switch (mode)
    {
    case RIG_MODE_USB:  ttmode = TT565_USB; break;

    case RIG_MODE_LSB:  ttmode = TT565_LSB; break;

    case RIG_MODE_CW:   ttmode = TT565_CW; break;

    case RIG_MODE_CWR:  ttmode = TT565_CWR; break;

    case RIG_MODE_AM:   ttmode = TT565_AM; break;

    case RIG_MODE_FM:   ttmode = TT565_FM; break;

    case RIG_MODE_RTTY: ttmode = TT565_RTTY; break;

    default:
        rig_debug(RIG_DEBUG_ERR, "%s: unsupported mode %s\n",
                  __func__, rig_strrmode(mode));
        return -RIG_EINVAL;
    }

    if (width == RIG_PASSBAND_NORMAL)
    {
        width = rig_passband_normal(rig, mode);
    }

    ttreceiver = which_receiver(rig, vfo);


    if (rig->caps->rig_model == RIG_MODEL_TT599)
    {
        // Additional R%CF0 puts bandwidth control back to bandwidth knob
        SNPRINTF(mdbuf, sizeof(mdbuf), "*R%cM%c" EOM "*R%cF%d" EOM "R%cF0" EOM,
                 ttreceiver,
                 ttmode,
                 ttreceiver,
                 (int)width,
                 ttreceiver
                );
    }
    else
    {
        SNPRINTF(mdbuf, sizeof(mdbuf), "*R%cM%c" EOM "*R%cF%d" EOM,
                 ttreceiver,
                 ttmode,
                 ttreceiver,
                 (int)width
                );
    }

    retval = write_block(&rs->rigport, (unsigned char *) mdbuf, strlen(mdbuf));

    return retval;
}

/**
 * \param rig must != NULL
 * \param vfo
 * \param mode Receives current mode setting, must be != NULL
 * \param width Receives current bandwidth setting, must be != NULL
 * \returns RIG_OK or < 0
 * \brief Get op. mode and bandwidth for selected vfo
 *
 * \remarks Confusion of VFO and Main/Sub TRx/Rx. See tt565_set_mode.
 */
int tt565_get_mode(RIG *rig, vfo_t vfo, rmode_t *mode, pbwidth_t *width)
{
    int resp_len, retval;
    char cmdbuf[TT565_BUFSIZE], respbuf[TT565_BUFSIZE];
    char ttmode, ttreceiver;

    ttreceiver = which_receiver(rig, vfo);

    /* Query mode */
    SNPRINTF(cmdbuf, sizeof(cmdbuf), "?R%cM" EOM, ttreceiver);
    resp_len = sizeof(respbuf);
    retval = tt565_transaction(rig, cmdbuf, strlen(cmdbuf), respbuf, &resp_len);

    if (retval != RIG_OK)
    {
        return retval;
    }

    if (respbuf[1] != 'R' || respbuf[3] != 'M' || resp_len <= 4)
    {
        rig_debug(RIG_DEBUG_ERR, "%s: unexpected answer '%s'\n",
                  __func__, respbuf);
        return -RIG_EPROTO;
    }

    ttmode = respbuf[4];

    switch (ttmode)
    {
    case TT565_USB: *mode = RIG_MODE_USB; break;

    case TT565_LSB: *mode = RIG_MODE_LSB; break;

    case TT565_CW:  *mode = RIG_MODE_CW;  break;

    case TT565_CWR: *mode = RIG_MODE_CWR;  break;

    case TT565_AM:  *mode = RIG_MODE_AM;  break;

    case TT565_FM:  *mode = RIG_MODE_FM;  break;

    case TT565_RTTY:    *mode = RIG_MODE_RTTY;  break;

    default:
        rig_debug(RIG_DEBUG_ERR, "%s: unsupported mode '%c'\n",
                  __func__, ttmode);
        return -RIG_EPROTO;
    }

    /* Orion may need some time to "recover" from ?RxM before ?RxF */
    hl_usleep(80000);      // try 80 ms
    /* Query passband width (filter) */
    SNPRINTF(cmdbuf, sizeof(cmdbuf), "?R%cF" EOM, ttreceiver);
    resp_len = sizeof(respbuf);
    retval = tt565_transaction(rig, cmdbuf, strlen(cmdbuf), respbuf, &resp_len);

    if (retval != RIG_OK)
    {
        return retval;
    }

    if (respbuf[1] != 'R' || respbuf[3] != 'F' || resp_len <= 4)
    {
        rig_debug(RIG_DEBUG_ERR, "%s: unexpected answer '%s'\n",
                  __func__, respbuf);
        return -RIG_EPROTO;
    }

    *width = atoi(respbuf + 4);

    return RIG_OK;
}

/**
 * \param rig must != NULL
 * \param vfo
 * \param ts Tuning Step, Hz
 * \returns RIG_OK or < 0
 * \brief Set Tuning Step for VFO A or B.
 */
int tt565_set_ts(RIG *rig, vfo_t vfo, shortfreq_t ts)
{
    int retval;
    char cmdbuf[TT565_BUFSIZE];

    SNPRINTF(cmdbuf, sizeof(cmdbuf), "*R%cI%d" EOM,
             which_receiver(rig, vfo),
             (int)ts);

    retval = tt565_transaction(rig, cmdbuf, strlen(cmdbuf), NULL, NULL);

    return retval;
}

/**
 * \param rig must != NULL
 * \param vfo
 * \param ts Receives Tuning Step, Hz
 * \returns RIG_OK or < 0
 * \brief Get Tuning Step for VFO A or B.
 */
int tt565_get_ts(RIG *rig, vfo_t vfo, shortfreq_t *ts)
{
    int resp_len, retval;
    char cmdbuf[TT565_BUFSIZE], respbuf[TT565_BUFSIZE];

    SNPRINTF(cmdbuf, sizeof(cmdbuf), "?R%cI" EOM,
             which_receiver(rig, vfo));

    resp_len = sizeof(respbuf);
    retval = tt565_transaction(rig, cmdbuf, strlen(cmdbuf), respbuf, &resp_len);

    if (retval != RIG_OK)
    {
        return retval;
    }

    if (respbuf[1] != 'R' || respbuf[3] != 'I' || resp_len <= 4)
    {
        rig_debug(RIG_DEBUG_ERR, "%s: unexpected answer '%s'\n",
                  __func__, respbuf);
        return -RIG_EPROTO;
    }

    *ts = atoi(respbuf + 4);

    return RIG_OK;
}

/**
 * \param rig must != NULL
 * \param vfo
 * \param rit Rx incremental tuning, Hz
 * \returns RIG_OK or < 0
 * \brief Set Rx incremental tuning
 * Note: command rit != 0 ==> rit "on"; rit == 0 ==> rit "off"
 */
int tt565_set_rit(RIG *rig, vfo_t vfo, shortfreq_t rit)
{
    int retval;
    char cmdbuf[TT565_BUFSIZE];

    SNPRINTF(cmdbuf, sizeof(cmdbuf), "*R%cR%d" EOM,
             which_receiver(rig, vfo),
             (int)rit);

    retval = tt565_transaction(rig, cmdbuf, strlen(cmdbuf), NULL, NULL);

    return retval;
}

/**
 * \param rig must != NULL
 * \param vfo
 * \param rit Receives Rx incremental tuning, Hz
 * \returns RIG_OK or < 0
 * \brief Get Rx incremental tuning
 */
int tt565_get_rit(RIG *rig, vfo_t vfo, shortfreq_t *rit)
{
    int resp_len, retval;
    char cmdbuf[TT565_BUFSIZE], respbuf[TT565_BUFSIZE];

    SNPRINTF(cmdbuf, sizeof(cmdbuf), "?R%cR" EOM,
             which_receiver(rig, vfo));

    resp_len = sizeof(respbuf);
    retval = tt565_transaction(rig, cmdbuf, strlen(cmdbuf), respbuf, &resp_len);

    if (retval != RIG_OK)
    {
        return retval;
    }

    if (respbuf[1] != 'R' || respbuf[3] != 'R' || resp_len <= 4)
    {
        rig_debug(RIG_DEBUG_ERR, "%s: unexpected answer '%s'\n",
                  __func__, respbuf);
        return -RIG_EPROTO;
    }

    *rit = atoi(respbuf + 4);

    return RIG_OK;
}

/**
 * \param rig must != NULL
 * \param vfo
 * \param xit Tx incremental tuning, Hz
 * \returns RIG_OK or < 0
 * \brief Set Tx incremental tuning (Main TRx only)
 * Note: command xit != 0 ==> xit "on"; xit == 0 ==> xit "off"
 */
int tt565_set_xit(RIG *rig, vfo_t vfo, shortfreq_t xit)
{
    int retval;
    char cmdbuf[TT565_BUFSIZE];

    /* Sub receiver does not contain an XIT setting */

    SNPRINTF(cmdbuf, sizeof(cmdbuf), "*R%cX%d" EOM,
             'M',
             (int)xit);

    retval = tt565_transaction(rig, cmdbuf, strlen(cmdbuf), NULL, NULL);

    return retval;
}

/**
 * \param rig must != NULL
 * \param vfo
 * \param xit Receives Tx incremental tuning, Hz
 * \returns RIG_OK or < 0
 * \brief Get Tx incremental tuning (Main TRx only)
 */
int tt565_get_xit(RIG *rig, vfo_t vfo, shortfreq_t *xit)
{
    int resp_len, retval;
    char cmdbuf[TT565_BUFSIZE], respbuf[TT565_BUFSIZE];

    SNPRINTF(cmdbuf, sizeof(cmdbuf), "?R%cX" EOM,
             'M');

    resp_len = sizeof(respbuf);
    retval = tt565_transaction(rig, cmdbuf, strlen(cmdbuf), respbuf, &resp_len);

    if (retval != RIG_OK)
    {
        return retval;
    }

    if (respbuf[1] != 'R' || respbuf[3] != 'X' || resp_len <= 4)
    {
        rig_debug(RIG_DEBUG_ERR, "%s: unexpected answer '%s'\n",
                  __func__, respbuf);
        return -RIG_EPROTO;
    }

    *xit = atoi(respbuf + 4);

    return RIG_OK;
}

/**
 * \param rig must != NULL
 * \param vfo
 * \param ptt RIG_PTT_ON or RIG_PTT_OFF
 * \returns RIG_OK or < 0
 * \brief Set push to talk (Tx on/off)
 */
int tt565_set_ptt(RIG *rig, vfo_t vfo, ptt_t ptt)
{
    struct rig_state *rs = &rig->state;

    return write_block(&rs->rigport,
                       (unsigned char *)(ptt == RIG_PTT_ON ? "*TK" EOM : "*TU" EOM), 4);
}

/**
 * \param rig must != NULL
 * \param vfo
 * \param ptt Receives RIG_PTT_ON or RIG_PTT_OFF
 * \returns RIG_OK or < 0
 * \brief Get push to talk (Tx on/off)
 */
int tt565_get_ptt(RIG *rig, vfo_t vfo, ptt_t *ptt)
{
    int resp_len, retval;
    char respbuf[TT565_BUFSIZE];

    resp_len = sizeof(respbuf);
    retval = tt565_transaction(rig, "?S" EOM, 3, respbuf, &resp_len);

    if (retval != RIG_OK)
    {
        return retval;
    }

    if (respbuf[1] != 'S' || resp_len < 5)
    {
        rig_debug(RIG_DEBUG_ERR, "%s: unexpected answer '%s'\n",
                  __func__, respbuf);
        return -RIG_EPROTO;
    }

    *ptt = respbuf[2] == 'T' ? RIG_PTT_ON : RIG_PTT_OFF ;

    return RIG_OK;
}

/**
 * \param rig must != NULL
 * \param reset (not used)
 * \returns RIG_OK or < 0
 * \brief Restart Orion firmware
 *
 * Sends an "X" command and listens for reply = "ORION START".  This only
 * seems to test for healthy connection to the firmware.  There is no effect
 * on Orion's state, AFAIK.
 */
int tt565_reset(RIG *rig, reset_t reset)
{
    int retval, reset_len;
    char reset_buf[TT565_BUFSIZE];

    if (reset == RIG_RESET_NONE) { return RIG_OK; } /* No operation requested. */

    reset_len = sizeof(reset_buf);
    retval = tt565_transaction(rig, "XX" EOM, 3, reset_buf, &reset_len);

    if (retval != RIG_OK) { return retval; }

    if (!strstr(reset_buf, "ORION START"))
    {
        rig_debug(RIG_DEBUG_ERR, "%s: unexpected answer '%s'\n",
                  __func__, reset_buf);
        return -RIG_EPROTO;
    }

    return RIG_OK;
}

/**
 * \param rig must != NULL
 * \returns firmware identification string or NULL
 * \brief Get firmware identification, e.g., "Version 1.372"
 *
 * Re-entrancy issue (what else is new?)
 */
const char *tt565_get_info(RIG *rig)
{
    static char buf[TT565_BUFSIZE]; /* FIXME: reentrancy */
    int firmware_len, retval, i;

    firmware_len = sizeof(buf);
    retval = tt565_transaction(rig, "?V" EOM, 3, buf, &firmware_len);

    if (retval != RIG_OK || firmware_len < 8)
    {
        rig_debug(RIG_DEBUG_ERR, "%s: ack NG, len=%d\n",
                  __func__, firmware_len);
        buf[0] = '\0';
        return buf;
    }

    buf[firmware_len] = '\0';

    /* filter out any non-graphic characters */
    for (i = 0; i < strlen(buf); i++)
        if (!isgraph((int)buf[i])) { buf[i] = ' '; }   // bad chars -> spaces

    return buf;
}

/**
 * \param rig must != NULL
 * \param vfo
 * \param level A level id token, e.g. RIG_LEVEL_AF
 * \param val Value for the level, on a scale or via a token
 * \returns RIG_OK or < 0
 * \brief Sets any of Orion's "Level" adjustments
 *
 * Unfortunately, "val" type is not well defined.  Sometimes it is a float (AF gain),
 * an integer (RF Atten.), or an enum (RIG_AGC_FAST)...
 *
 * Supported Levels and Units
 * \n  -RIG_LEVEL_RFPOWER, float 0.0 - 1.0
 * \n  -RIG_LEVEL_AGC, int RIG_AGC_x, x= OFF, FAST, MEDIUM, SLOW, USER
 * \n  -RIG_LEVEL_AF, float 0.0 - 1.0
 * \n  -RIG_LEVEL_IF, passband tuning, int Hz
 * \n  -RIG_LEVEL_RF, IF gain (!), float 0.0 - 1.0
 * \n  -RIG_LEVEL_ATT, Atten. setting, int dB (we pick 0, 6, 12, or 18 dB)
 * \n  -RIG_LEVEL_PREAMP, Preamp on/off, 0-1 (main Rx only)
 * \n  -RIG_LEVEL_SQL, squelch, float 0.0 - 1.0
 * \n  -RIG_LEVEL_MICGAIN, float 0.0 - 1.0
 * \n  -RIG_LEVEL_COMP, speech compression, float 0.0 - 1.0
 * \n  -RIG_LEVEL_CWPITCH, int Hz
 * \n  -RIG_LEVEL_KEYSPD, int wpm
 * \n  -RIG_LEVEL_NR, noise reduction/blank, float 0.0 - 1.0
 * \n  -RIG_LEVEL_VOXDELAY, vox delay, float x 1/10 second
 * \n  -RIG_LEVEL_VOXGAIN, float 0.0 - 1.0
 * \n  -RIG_LEVEL_ANTIVOX, float 0.0 - 1.0
 *
 * \n FIXME: cannot support PREAMP and ATT both at same time (make sens though)
 */
int tt565_set_level(RIG *rig, vfo_t vfo, setting_t level, value_t val)
{
    int retval, ii;
    char cmdbuf[TT565_BUFSIZE], cc;

    switch (level)
    {
    case RIG_LEVEL_RFPOWER:
        SNPRINTF(cmdbuf, sizeof(cmdbuf), "*TP%d" EOM,
                 (int)(val.f * 100));
        break;

    case RIG_LEVEL_AGC:
        switch (val.i)
        {
        case RIG_AGC_OFF:    cc = 'O'; break;

        case RIG_AGC_FAST:   cc = 'F'; break;

        case RIG_AGC_MEDIUM: cc = 'M'; break;

        case RIG_AGC_SLOW:   cc = 'S'; break;

        case RIG_AGC_USER:   cc = 'P'; break;

        default: cc = 'M';
        }

        SNPRINTF(cmdbuf, sizeof(cmdbuf), "*R%cA%c" EOM,
                 which_receiver(rig, vfo),
                 cc);
        break;

    case RIG_LEVEL_AF:
        /* AF Gain, float 0.0 - 1.0 */
        SNPRINTF(cmdbuf, sizeof(cmdbuf), "*U%c%d" EOM,
                 which_receiver(rig, vfo),
                 (int)(val.f * 255));
        break;

    case RIG_LEVEL_IF:
        /* This is passband tuning int Hz */
        SNPRINTF(cmdbuf, sizeof(cmdbuf), "*R%cP%d" EOM,
                 which_receiver(rig, vfo),
                 val.i);
        break;

    case RIG_LEVEL_RF:
        /* This is IF Gain, float 0.0 - 1.0 */
        SNPRINTF(cmdbuf, sizeof(cmdbuf), "*R%cG%d" EOM,
                 which_receiver(rig, vfo),
                 (int)(val.f * 100));
        break;

    case RIG_LEVEL_ATT:
        /* RF Attenuator, int dB */
        ii = -1;    /* Request 0-5 dB -> 0, 6-11 dB -> 6, etc. */

        while (rig->caps->attenuator[++ii] != RIG_DBLST_END)
        {
            if (rig->caps->attenuator[ii] > val.i) { break; }
        }

        SNPRINTF(cmdbuf, sizeof(cmdbuf), "*R%cT%d" EOM,
                 which_receiver(rig, vfo),
                 ii);
        break;

    case RIG_LEVEL_PREAMP:

        /* Sub receiver does not contain a Preamp */
        if (which_receiver(rig, vfo) == 'S')
        {
            return -RIG_EINVAL;
        }

        /* RF Preamp (main Rx), int 0 or 1 */
        SNPRINTF(cmdbuf, sizeof(cmdbuf), "*RME%d" EOM,
                 val.i == 0 ? 0 : 1);
        break;

    case RIG_LEVEL_SQL:
        /* Squelch level, float 0.0 - 1.0 */
        SNPRINTF(cmdbuf, sizeof(cmdbuf), "*R%cS%d" EOM,
                 which_receiver(rig, vfo),
                 (int)((val.f * 127) - 127));
        break;

    case RIG_LEVEL_MICGAIN:
        /* Mic gain, float 0.0 - 1.0 */
        SNPRINTF(cmdbuf, sizeof(cmdbuf), "*TM%d" EOM,
                 (int)(val.f * 100));
        break;

    case RIG_LEVEL_COMP:
        /* Speech Processor, float 0.0 - 1.0 */
        SNPRINTF(cmdbuf, sizeof(cmdbuf), "*TS%d" EOM,
                 (int)(val.f * 9));
        break;

    case RIG_LEVEL_CWPITCH:

        /* "CWPITCH" is the "Tone" button on the Orion.
           Manual menu adjustment works down to 100 Hz, but not via
           computer. int Hz.
        */
        if (val.i > TT565_TONE_MAX) { val.i = TT565_TONE_MAX; }
        else if (val.i < TT565_TONE_MIN) { val.i = TT565_TONE_MIN; }

        SNPRINTF(cmdbuf, sizeof(cmdbuf), "*CT%d" EOM,
                 val.i);
        break;

    case RIG_LEVEL_KEYSPD:

        /* Keyer speed setting does not imply Keyer = "on".  That is a
           command which should be a hamlib function, but is not.
           Keyer speed determines the rate of computer sent CW also.
        */
        if (val.i > TT565_CW_MAX) { val.i = TT565_CW_MAX; }
        else if (val.i < TT565_CW_MIN) { val.i = TT565_CW_MIN; }

        SNPRINTF(cmdbuf, sizeof(cmdbuf), "*CS%d" EOM,
                 val.i);
        break;

    case RIG_LEVEL_NR:
        if (rig->caps->rig_model == RIG_MODEL_TT599)
        {
            ii = (int)(val.f * 10);

            if (ii > 9) { ii = 9; } // cannot set NR level 10 apparently

            SNPRINTF(cmdbuf, sizeof(cmdbuf), "*RMNN%c" EOM, ii);
        }
        else
        {
            /* Noise Reduction (blanking) Float 0.0 - 1.0
                For some reason NB setting is supported in 1.372, but
               NR, NOTCH, and AN are not.
               FOR NOW -- RIG_LEVEL_NR controls the Orion NB setting
            */
            SNPRINTF(cmdbuf, sizeof(cmdbuf), "*R%cNB%d" EOM,
                     which_receiver(rig, vfo),
                     (int)(val.f * 9));
        }

        break;

    case RIG_LEVEL_VOXDELAY:
        /* VOX delay, float tenths of seconds */
        SNPRINTF(cmdbuf, sizeof(cmdbuf), "*TH%4.2f" EOM, 0.1 * val.f);
        break;

    case RIG_LEVEL_VOXGAIN:
        /* Float, 0.0 - 1.0 */
        SNPRINTF(cmdbuf, sizeof(cmdbuf), "*TG%d" EOM, (int)(100.0 * val.f));
        break;

    case RIG_LEVEL_ANTIVOX:
        /* Float, 0.0 - 1.0 */
        SNPRINTF(cmdbuf, sizeof(cmdbuf), "*TA%d" EOM, (int)(100.0 * val.f));
        break;

    default:
        rig_debug(RIG_DEBUG_ERR, "%s: unsupported level %s\n",
                  __func__, rig_strlevel(level));
        return -RIG_EINVAL;
    }

    retval = tt565_transaction(rig, cmdbuf, strlen(cmdbuf), NULL, NULL);
    return retval;
}

/**
 * \param rig must be != NULL
 * \param vfo
 * \param level identifier for level of interest
 * \param val Receives level's value, must != NULL
 * \brief Get the current value of an Orion "level"
 *
 * \sa tt565_get_level
 * Supported rx levels:
 * \n  -RIG_LEVEL_SWR
 * \n  -RIG_LEVEL_RAWSTR, int  raw rx signal strength (rig units)
 * \n  -RIG_LEVEL_RFPOWER
 * \n  -RIG_LEVEL_AGC
 * \n  -RIG_LEVEL_AF
 * \n  -RIG_LEVEL_IF
 * \n  -RIG_LEVEL_RF
 * \n  -RIG_LEVEL_ATT
 * \n  -RIG_LEVEL_PREAMP
 * \n  -RIG_LEVEL_SQL
 * \n  -RIG_LEVEL_MICGAIN
 * \n  -RIG_LEVEL_COMP
 * \n  -RIG_LEVEL_CWPITCH
 * \n  -RIG_LEVEL_KEYSPED
 * \n  -RIG_LEVEL_NR
 * \n  -RIG_LEVEL_VOX
 * \n  -RIG_LEVEL_VOXGAIN
 * \n  -RIG_LEVEL_ANTIVOX
 *
 * (RIG_LEVEL_STRENGTH, int calibrated signal strength (dB, S9 = 0) is
 * handled in settings.c)

 */
int tt565_get_level(RIG *rig, vfo_t vfo, setting_t level, value_t *val)
{
    int retval, lvl_len;
    char cmdbuf[TT565_BUFSIZE], lvlbuf[TT565_BUFSIZE];

    /* Optimize: sort the switch cases with the most frequent first */
    switch (level)
    {
    case RIG_LEVEL_SWR:
        lvl_len = sizeof(lvlbuf);
        retval = tt565_transaction(rig, "?S" EOM, 3, lvlbuf, &lvl_len);

        if (retval != RIG_OK)
        {
            return retval;
        }

        if (rig->caps->rig_model == RIG_MODEL_TT599)
        {
            double fwd, ref;

            /* in Xmit, response is @STF99R10<cr> 99 watts forward,1.0 watt reflected
                uu = fwd watts
                vv = rev watts x 10

               in Rcv,  response is @SRM16<CR> Indicates 16 dbm
                uuu = 000-100 (apx) Main S meter
            */

            if (lvlbuf[1] != 'S' || lvl_len < 5)
            {
                rig_debug(RIG_DEBUG_ERR, "%s: unexpected answer '%s'\n",
                          __func__, lvlbuf);
                return -RIG_EPROTO;
            }

            if (lvlbuf[2] == 'T')
            {
                ref = atof(strchr(lvlbuf + 2, 'R') + 1) / 10.0;   /* reflected power */
                fwd = atof(strchr(lvlbuf + 2, 'F') + 1);          /* forward power */

                if (fwd == 0.0)
                {
                    val->f = 0.0;                                 /* no forward power */
                }
                else if (fwd == ref)                              /* too high SWR */
                {
                    val->f = 9.99;
                }
                else
                {
                    val->f = (1 + sqrt(ref / fwd)) / (1 - sqrt(ref / fwd)); /* calculate SWR */
                }

                if (val->f < 1.0) { val->f = 9.99; }              /* high VSWR */
            }
            else { val->f = 0.0; }  /* SWR in Receive = 0.0 */

        }
        else
        {

            /* in Xmit, response is @STFuuuRvvvSwww (or ...Swwww)
                uuu = 000-100 (apx) fwd watts
                            vvv = 000-100       rev watts
                            www = 256-999  256 * VSWR
               in Rcv,  response is @SRMuuuSvvv
                uuu = 000-100 (apx) Main S meter
                            vvv = 000-100 (apx) Sub  S meter
            */

            if (lvlbuf[1] != 'S' || lvl_len < 5)
            {
                rig_debug(RIG_DEBUG_ERR, "%s: unexpected answer '%s'\n",
                          __func__, lvlbuf);
                return -RIG_EPROTO;
            }

            if (lvlbuf[2] == 'T')
            {
                val->f = atof(strchr(lvlbuf + 5, 'S') + 1) / 256.0;

                if (val->f < 1.0) { val->f = 9.99; }    /* high VSWR */
            }
            else { val->f = 0.0; }  /* SWR in Receive = 0.0 */
        }

        break;

    case RIG_LEVEL_RAWSTR:      /* provide uncalibrated raw strength, int */
        /* NB: RIG_LEVEL_STRENGTH is handled in the frontend */
        lvl_len = sizeof(lvlbuf);
        retval = tt565_transaction(rig, "?S" EOM, 3, lvlbuf, &lvl_len);

        if (retval != RIG_OK)
        {
            return retval;
        }

        if (lvlbuf[1] != 'S' || lvl_len < 5)
        {
            rig_debug(RIG_DEBUG_ERR, "%s: unexpected answer '%s'\n",
                      __func__, lvlbuf);
            return -RIG_EPROTO;
        }

        if (lvlbuf[2] == 'R')
        {
            char *raw_field;

            /* response is @SRMnnnSnnn, incl main & sub rx. */
            /* TT's spec indicates variable length data 1-3 digits */
            if (vfo == RIG_VFO_SUB)     /* look at sub rx info */
            {
                raw_field = strchr(lvlbuf + 3, 'S') + 1; /* length may vary */
            }
            else                /* look at main rx info */
            {
                char *raw_field2;
                raw_field = lvlbuf + 4;
                raw_field2 = strchr(raw_field, 'S'); /* position may vary */

                if (raw_field2) { *raw_field2 = '\0'; } /* valid string */
            }

            val->i = atoi(raw_field);   /* get raw value */
        }
        else { val->i = 0; }    /* S-meter in xmit => 0 */

        break;

    case RIG_LEVEL_RFPOWER:
        lvl_len = sizeof(lvlbuf);
        retval = tt565_transaction(rig, "?TP" EOM, 4, lvlbuf, &lvl_len);

        if (retval != RIG_OK)
        {
            return retval;
        }

        if (lvlbuf[1] != 'T' || lvlbuf[2] != 'P' || lvl_len < 4)
        {
            rig_debug(RIG_DEBUG_ERR, "%s: unexpected answer '%s'\n",
                      __func__, lvlbuf);
            return -RIG_EPROTO;
        }

        val->f = atof(lvlbuf + 3) / 100.0;
        break;

    case RIG_LEVEL_AGC:
        SNPRINTF(cmdbuf, sizeof(cmdbuf), "?R%cA" EOM,
                 which_receiver(rig, vfo));

        lvl_len = sizeof(lvlbuf);
        retval = tt565_transaction(rig, cmdbuf, strlen(cmdbuf), lvlbuf, &lvl_len);

        if (retval != RIG_OK)
        {
            return retval;
        }

        if (lvlbuf[1] != 'R' || lvlbuf[3] != 'A' || lvl_len < 5)
        {
            rig_debug(RIG_DEBUG_ERR, "%s: unexpected answer '%s'\n",
                      __func__, lvlbuf);
            return -RIG_EPROTO;
        }

        switch (lvlbuf[4])
        {
        case 'O': val->i = RIG_AGC_OFF; break;

        case 'F': val->i = RIG_AGC_FAST; break;

        case 'M': val->i = RIG_AGC_MEDIUM; break;

        case 'S': val->i = RIG_AGC_SLOW; break;

        case 'P': val->i = RIG_AGC_USER; break;

        default:
            return -RIG_EPROTO;
        }

        break;

    case RIG_LEVEL_AF:
        SNPRINTF(cmdbuf, sizeof(cmdbuf), "?U%c" EOM,
                 which_receiver(rig, vfo));

        lvl_len = sizeof(lvlbuf);
        retval = tt565_transaction(rig, cmdbuf, strlen(cmdbuf), lvlbuf, &lvl_len);

        if (retval != RIG_OK)
        {
            return retval;
        }

        if (lvlbuf[1] != 'U' || lvl_len < 4)
        {
            rig_debug(RIG_DEBUG_ERR, "%s: unexpected answer '%s'\n",
                      __func__, lvlbuf);
            return -RIG_EPROTO;
        }

        val->f = atof(lvlbuf + 3) / 255.0;
        break;

    case RIG_LEVEL_IF:
        SNPRINTF(cmdbuf, sizeof(cmdbuf), "?R%cP" EOM,  /* passband tuning */
                 which_receiver(rig, vfo));

        lvl_len = sizeof(lvlbuf);
        retval = tt565_transaction(rig, cmdbuf, strlen(cmdbuf), lvlbuf, &lvl_len);

        if (retval != RIG_OK)
        {
            return retval;
        }

        if (lvlbuf[1] != 'R' || lvlbuf[3] != 'P' || lvl_len < 5)
        {
            rig_debug(RIG_DEBUG_ERR, "%s: unexpected answer '%s'\n",
                      __func__, lvlbuf);
            return -RIG_EPROTO;
        }

        val->i = atoi(lvlbuf + 4);
        break;

    case RIG_LEVEL_RF:
        SNPRINTF(cmdbuf, sizeof(cmdbuf), "?R%cG" EOM,
                 which_receiver(rig, vfo));

        lvl_len = sizeof(lvlbuf);
        retval = tt565_transaction(rig, cmdbuf, strlen(cmdbuf), lvlbuf, &lvl_len);

        if (retval != RIG_OK)
        {
            return retval;
        }

        if (lvlbuf[1] != 'R' || lvlbuf[3] != 'G' || lvl_len < 5)
        {
            rig_debug(RIG_DEBUG_ERR, "%s: unexpected answer '%s'\n",
                      __func__, lvlbuf);
            return -RIG_EPROTO;
        }

        val->f = atof(lvlbuf + 4) / 100.0;
        break;

    case RIG_LEVEL_ATT:
        SNPRINTF(cmdbuf, sizeof(cmdbuf), "?R%cT" EOM,
                 which_receiver(rig, vfo));

        lvl_len = sizeof(lvlbuf);
        retval = tt565_transaction(rig, cmdbuf, strlen(cmdbuf), lvlbuf, &lvl_len);

        if (retval != RIG_OK)
        {
            return retval;
        }

        if (lvlbuf[1] != 'R' || lvlbuf[3] != 'T' || lvl_len < 5)
        {
            rig_debug(RIG_DEBUG_ERR, "%s: unexpected answer '%s'\n",
                      __func__, lvlbuf);
            return -RIG_EPROTO;
        }

        if (lvlbuf[4] == '0')
        {
            val->i = 0;
        }
        else
        {
            val->i = rig->caps->attenuator[lvlbuf[4] - '1'];
        }

        break;

    case RIG_LEVEL_PREAMP:

        /* Sub receiver does not contain a Preamp */
        if (which_receiver(rig, vfo) == 'S')
        {
            val->i = 0;
            break;
        }

        lvl_len = sizeof(lvlbuf);
        retval = tt565_transaction(rig, "?RME" EOM, 5, lvlbuf, &lvl_len);

        if (retval != RIG_OK)
        {
            return retval;
        }

        if (lvlbuf[1] != 'R' || lvlbuf[3] != 'E' || lvl_len < 5)
        {
            rig_debug(RIG_DEBUG_ERR, "%s: unexpected answer '%s'\n",
                      __func__, lvlbuf);
            return -RIG_EPROTO;
        }

        val->i = lvlbuf[4] == '0' ? 0 : rig->caps->preamp[0];
        break;

    case RIG_LEVEL_SQL:
        SNPRINTF(cmdbuf, sizeof(cmdbuf), "?R%cS" EOM,
                 which_receiver(rig, vfo));

        lvl_len = sizeof(lvlbuf);
        retval = tt565_transaction(rig, cmdbuf, strlen(cmdbuf), lvlbuf, &lvl_len);

        if (retval != RIG_OK)
        {
            return retval;
        }

        if (lvlbuf[1] != 'R' || lvlbuf[3] != 'S' || lvl_len < 5)
        {
            rig_debug(RIG_DEBUG_ERR, "%s: unexpected answer '%s'\n",
                      __func__, lvlbuf);
            return -RIG_EPROTO;
        }

        val->f = (atof(lvlbuf + 4) + 127.0) / 127.0;
        break;

    case RIG_LEVEL_MICGAIN:
        lvl_len = sizeof(lvlbuf);
        retval = tt565_transaction(rig, "?TM" EOM, 4, lvlbuf, &lvl_len);

        if (retval != RIG_OK)
        {
            return retval;
        }

        if (lvlbuf[1] != 'T' || lvlbuf[2] != 'M' || lvl_len < 4)
        {
            rig_debug(RIG_DEBUG_ERR, "%s: unexpected answer '%s'\n",
                      __func__, lvlbuf);
            return -RIG_EPROTO;
        }

        val->f = atof(lvlbuf + 3) / 100.0;
        break;

    case RIG_LEVEL_COMP:
        lvl_len = sizeof(lvlbuf);
        retval = tt565_transaction(rig, "?TS" EOM, 4, lvlbuf, &lvl_len);

        if (retval != RIG_OK)
        {
            return retval;
        }

        if (lvlbuf[1] != 'T' || lvlbuf[2] != 'S' || lvl_len < 4)
        {
            rig_debug(RIG_DEBUG_ERR, "%s: unexpected answer '%s'\n",
                      __func__, lvlbuf);
            return -RIG_EPROTO;
        }

        val->f = atof(lvlbuf + 3) / 9.0;
        break;

    case RIG_LEVEL_CWPITCH:
        lvl_len = sizeof(lvlbuf);
        retval = tt565_transaction(rig, "?CT" EOM, 4, lvlbuf, &lvl_len);

        if (retval != RIG_OK)
        {
            return retval;
        }

        if (lvlbuf[1] != 'C' || lvlbuf[2] != 'T' || lvl_len < 4)
        {
            rig_debug(RIG_DEBUG_ERR, "%s: unexpected answer '%s'\n",
                      __func__, lvlbuf);
            return -RIG_EPROTO;
        }

        val->i = atoi(lvlbuf + 3);
        break;

    case RIG_LEVEL_KEYSPD:
        lvl_len = sizeof(lvlbuf);
        retval = tt565_transaction(rig, "?CS" EOM, 4, lvlbuf, &lvl_len);

        if (retval != RIG_OK)
        {
            return retval;
        }

        if (lvlbuf[1] != 'C' || lvlbuf[2] != 'S' || lvl_len < 4)
        {
            rig_debug(RIG_DEBUG_ERR, "%s: unexpected answer '%s'\n",
                      __func__, lvlbuf);
            return -RIG_EPROTO;
        }

        val->i = atoi(lvlbuf + 3);
        break;

    case RIG_LEVEL_NR:

        /* RIG_LEVEL_NR controls Orion NB setting - TEMP */
        if (rig->caps->rig_model == RIG_MODEL_TT599)
        {
            SNPRINTF(cmdbuf, sizeof(cmdbuf), "?RMNN" EOM)
        }
        else
        {
            SNPRINTF(cmdbuf, sizeof(cmdbuf), "?R%cNB" EOM,
                     which_receiver(rig, vfo));
        }

        lvl_len = sizeof(lvlbuf);
        retval = tt565_transaction(rig, cmdbuf, strlen(cmdbuf), lvlbuf, &lvl_len);

        if (retval != RIG_OK)
        {
            return retval;
        }

        if (lvlbuf[1] != 'R' || lvl_len < 6)
        {
            rig_debug(RIG_DEBUG_ERR, "%s: unexpected answer '%s'\n",
                      __func__, lvlbuf);
            return -RIG_EPROTO;
        }

        sscanf(lvlbuf + 5, "%f", &val->f);
        val->f /= 10.0;
        break;

    case RIG_LEVEL_VOXDELAY: /* =VOXDELAY, tenths of secs. */
        lvl_len = sizeof(lvlbuf);
        retval = tt565_transaction(rig, "?TH" EOM, 4, lvlbuf, &lvl_len);

        if (retval != RIG_OK)
        {
            return retval;
        }

        if (lvlbuf[1] != 'T' || lvlbuf[2] != 'H' || lvl_len < 4)
        {
            rig_debug(RIG_DEBUG_ERR, "%s: unexpected answer '%s'\n",
                      __func__, lvlbuf);
            return -RIG_EPROTO;
        }

        val->f = 10.0 * atof(lvlbuf + 3);
        break;

    case RIG_LEVEL_VOXGAIN: /* Float, 0.0 - 1.0 */
        lvl_len = sizeof(lvlbuf);
        retval = tt565_transaction(rig, "?TG" EOM, 4, lvlbuf, &lvl_len);

        if (retval != RIG_OK)
        {
            return retval;
        }

        if (lvlbuf[1] != 'T' || lvlbuf[2] != 'G' || lvl_len < 4)
        {
            rig_debug(RIG_DEBUG_ERR, "%s: unexpected answer '%s'\n",
                      __func__, lvlbuf);
            return -RIG_EPROTO;
        }

        val->f = 0.01 * atof(lvlbuf + 3);
        break;

    case RIG_LEVEL_ANTIVOX: /* Float, 0.0 - 1.0 */
        lvl_len = sizeof(lvlbuf);
        retval = tt565_transaction(rig, "?TA" EOM, 4, lvlbuf, &lvl_len);

        if (retval != RIG_OK)
        {
            return retval;
        }

        if (lvlbuf[1] != 'T' || lvlbuf[2] != 'A' || lvl_len < 4)
        {
            rig_debug(RIG_DEBUG_ERR, "%s: unexpected answer '%s'\n",
                      __func__, lvlbuf);
            return -RIG_EPROTO;
        }

        val->f = 0.01 * atof(lvlbuf + 3);
        break;

    default:
        rig_debug(RIG_DEBUG_ERR, "%s: unsupported level %s\n",
                  __func__, rig_strlevel(level));
        return -RIG_EINVAL;
    }

    return RIG_OK;
}
/**
 * \param rig !=NULL
 * \param vfo
 * \param ch Channel number
 * \returns RIG_OK
 * \brief This only sets the current memory channel locally.  No Orion I/O.
 *
 * Use RIG_OP_TO_VFO and RIG_OP_FROM_VFO to get/store a freq in the channel.
 * \sa tt565_vfo_op
 */
int tt565_set_mem(RIG *rig, vfo_t vfo, int ch)
{
    struct tt565_priv_data *priv = (struct tt565_priv_data *)rig->state.priv;

    priv->ch = ch;  /* See RIG_OP_TO/FROM_VFO */
    return RIG_OK;
}

/**
 * \param rig != NULL
 * \param vfo
 * \param ch to receive the current channel number
 * \returns RIG_OK
 * \brief Get the current memory channel number (only)
 */
int tt565_get_mem(RIG *rig, vfo_t vfo, int *ch)
{
    struct tt565_priv_data *priv = (struct tt565_priv_data *)rig->state.priv;

    *ch = priv->ch;
    return RIG_OK;
}

/**
 * \param rig != NULL
 * \param vfo
 * \param op Operation to perform, a RIG_OP token
 * \returns RIG_OK or < 0
 * \brief perform a RIG_OP operation
 *
 * Supported operations:
 * \n RIG_OP_TO_VFO memory channel to VFO (includes bw, mode, etc)
 * \n RIG_OP_FROM_VFO stores VFO (& other data) to memory channel
 * \n RIG_OP_TUNE initiates a tuner cycle (if tuner present) MAY BE BROKEN
 * \n RIG_OP_UP increment VFO freq by tuning step
 * \n RIG_OP_DOWN decrement VFO freq by tuning step
 */
int tt565_vfo_op(RIG *rig, vfo_t vfo, vfo_op_t op)
{
    struct tt565_priv_data *priv = (struct tt565_priv_data *)rig->state.priv;
    char cmdbuf[TT565_BUFSIZE];
    int retval;

    switch (op)
    {
    case RIG_OP_TO_VFO:
    case RIG_OP_FROM_VFO:
        SNPRINTF(cmdbuf, sizeof(cmdbuf), "*K%c%c%d" EOM,
                 op == RIG_OP_TO_VFO ? 'R' : 'W',
                 which_vfo(rig, vfo),
                 priv->ch);
        break;

    case RIG_OP_TUNE:
        strcpy(cmdbuf, "*TTT" EOM);
        break;

    case RIG_OP_UP:
    case RIG_OP_DOWN:
        SNPRINTF(cmdbuf, sizeof(cmdbuf), "*%cS%c1" EOM,
                 which_vfo(rig, vfo),
                 op == RIG_OP_UP ? '+' : '-');
        break;

    default:
        rig_debug(RIG_DEBUG_ERR, "%s: Unsupported op %d\n",
                  __func__, op);
        return -RIG_EINVAL;
    }

    retval = tt565_transaction(rig, cmdbuf, strlen(cmdbuf), NULL, NULL);
    return retval;
}

/**
 * \param rig
 * \param vfo
 * \param msg A message string (<= 20 char)
 * \returns RIG_OK
 * \brief Send a string as morse characters
 *
 * Orion keyer must be on for morse, but we do not have a "keyer on" function in
 * hamlib (yet). Keyer will be forced on.
 *
 * Orion can queue up to about 20 characters.
 * We could batch a longer message into 20 char chunks, but there is no
 * simple way to tell if message has completed.  We could calculate a
 * duration based on keyer speed and the text that was sent, but
 * what we really need is a handshake for "message complete".
 * Without it, you can't easily use the Orion as a code practice machine.
 * For now, we let the user do the batching.
 * Note that rig panel is locked up for duration of message.
 */
int tt565_send_morse(RIG *rig, vfo_t vfo, const char *msg)
{
    int msg_len, retval, ic;
    char morsecmd[8];
    static int keyer_set = FALSE;   /*Shouldn't be here!*/

    /*  Force keyer on. */
    if (!keyer_set)
    {
        retval = tt565_transaction(rig, "*CK1" EOM, 5, NULL, NULL);

        if (retval != RIG_OK)
        {
            return retval;
        }

        keyer_set = TRUE;
        hl_usleep(100000); /* 100 msec - guess */
    }

    msg_len = strlen(msg);

    if (msg_len > 20) { msg_len = 20; } /* sanity limit 20 chars */

    for (ic = 0; ic < msg_len; ic++)
    {
        SNPRINTF(morsecmd, sizeof(morsecmd), "/%c" EOM, msg[ic]);
        retval = tt565_transaction(rig, morsecmd, strlen(morsecmd), NULL, NULL);

        if (retval != RIG_OK)
        {
            return retval;
        }
    }

    return RIG_OK;
}
/**
 * \param rig != NULL
 * \param vfo
 * \param func Identifier for function to be performed
 * \param status data for function
 * \returns RIG_OK or < 0
 * \brief Set an Orion "function"
 *
 * Note that vfo must == RIG_VFO_CURR
 *
 * Supported functions & data
 * \n RIG_FUNC_TUNER, off/on, 0/1
 * \n RIG_FUNC_VOX, off/on, 0/1
 * \n RIG_FUNC_LOCK, unlock/lock, 0/1
 * \n RIG_FUNC_NB, off/on, 0/1 (sets Orion NB=0 or =4), compare RIG_LEVEL_NR
 *
 */
int tt565_set_func(RIG *rig, vfo_t vfo, setting_t func, int status)
{
    char fcmdbuf[TT565_BUFSIZE];
    int retval;

    if (vfo != RIG_VFO_CURR)
    {
        return -RIG_EINVAL;
    }

    switch (func)
    {
    case RIG_FUNC_TUNER:
        SNPRINTF(fcmdbuf, sizeof(fcmdbuf), "*TT%c" EOM, !status ? 0 : 1);
        break;

    case RIG_FUNC_VOX:
        SNPRINTF(fcmdbuf, sizeof(fcmdbuf), "*TV%c" EOM, !status ? 0 : 1);
        break;

    case RIG_FUNC_LOCK:
        SNPRINTF(fcmdbuf, sizeof(fcmdbuf), "*%c%c" EOM,
                 which_vfo(rig, vfo),
                 !status ? 'U' : 'L');
        break;

    case RIG_FUNC_NB:
        /* NB "on" sets Orion NB=4; "off" -> NB=0.  See also
        RIG_LEVEL_NR which maps to NB setting due to firmware
        limitation.
        */
        SNPRINTF(fcmdbuf, sizeof(fcmdbuf), "*R%cNB%c" EOM,
                 which_receiver(rig, vfo),
                 !status ? '0' : '4');
        break;

    default:
        rig_debug(RIG_DEBUG_ERR, "%s: unsupported set_func %s", __func__,
                  rig_strfunc(func));
        return -RIG_EINVAL;
    }

    retval = tt565_transaction(rig, fcmdbuf, strlen(fcmdbuf), NULL, NULL);

    if (retval != RIG_OK)
    {
        return retval;
    }

    return RIG_OK;
}
/**
 * \param rig != NULL
 * \param vfo must == RIG_VFO_CURR
 * \param func
 * \param status receives result of function query
 * \returns RIG_OK or < 0
 * \brief get state of an Orion "function"
 *
 * \sa tt565_set_func
 */
int tt565_get_func(RIG *rig, vfo_t vfo, setting_t func, int *status)
{
    char fcmdbuf[TT565_BUFSIZE], frespbuf[TT565_BUFSIZE];
    int retval, fresplen;

    if (vfo != RIG_VFO_CURR)
    {
        return -RIG_EINVAL;
    }

    switch (func)
    {
    case RIG_FUNC_TUNER:
        SNPRINTF(fcmdbuf, sizeof(fcmdbuf), "?TT" EOM);
        break;

    case RIG_FUNC_VOX:
        SNPRINTF(fcmdbuf, sizeof(fcmdbuf), "?TV" EOM);
        break;

    case RIG_FUNC_LOCK:
        SNPRINTF(fcmdbuf, sizeof(fcmdbuf), "?%cU" EOM,
                 which_vfo(rig, vfo));
        /* needs special treatment */
        fresplen = sizeof(frespbuf);
        retval = tt565_transaction(rig, fcmdbuf, strlen(fcmdbuf),
                                   frespbuf, &fresplen);

        if (retval != RIG_OK)
        {
            return retval;
        }

        /* response is @AL @AU or @BL @BU */
        *status = frespbuf[ 2 ] == 'L' ? 1 : 0;
        return RIG_OK;

    case RIG_FUNC_NB:
        /* Note NB should be a LEVEL for Orion. It is also
           available through LEVEL_NR
        */
        SNPRINTF(fcmdbuf, sizeof(fcmdbuf), "?R%cNB" EOM,
                 which_receiver(rig, vfo));
        /* needs special treatment */
        fresplen = sizeof(frespbuf);
        retval = tt565_transaction(rig, fcmdbuf, strlen(fcmdbuf),
                                   frespbuf, &fresplen);

        if (retval != RIG_OK)
        {
            return retval;
        }

        /* response is @RxNBn, n=0--9. Return 0 iff receive NB=0 */
        *status = frespbuf[ 5 ] == '0' ? 0 : 1;
        return RIG_OK;

    default:
        rig_debug(RIG_DEBUG_ERR, "%s: unsupported get_func %s", __func__,
                  rig_strfunc(func));
        return -RIG_EINVAL;
    }

    fresplen = sizeof(frespbuf);
    retval = tt565_transaction(rig, fcmdbuf, strlen(fcmdbuf), frespbuf, &fresplen);

    if (retval != RIG_OK)
    {
        return retval;
    }

    *status = frespbuf[ 3 ] == '1' ? 1 : 0;
    return RIG_OK;
}
/**
 * \param rig != NULL
 * \param vfo
 * \param ant antenna identifier RIG_ANT_1 or RIG_ANT_2
 * \returns RIG_OK or < 0
 * \brief Antenna selection for Orion
 *
 * We support Ant_1 and Ant_2 for M and S receivers.
 * \n Note that Rx-only antenna (Ant_3?) is not supported at this time.
 * \n Orion command assigns MSBN (main rtx, sub rx, both, or none) to each ant,
 * but hamlib wants to assign an ant to rx/tx!
 * The efficient way would be to keep current config in rig priv area, but we will
 * ask the rig what its state is each time...
 */
int tt565_set_ant(RIG *rig, vfo_t vfo, ant_t ant, value_t option)
{
    char respbuf[TT565_BUFSIZE];
    int resp_len, retval;
    ant_t main_ant, sub_ant;

    /* First, find out what antenna config is now. */
    resp_len = sizeof(respbuf);
    retval = tt565_transaction(rig, "?KA" EOM, 4, respbuf, &resp_len);

    if (retval != RIG_OK)
    {
        return retval;
    }

    if (resp_len != 7 || respbuf[1] != 'K' || respbuf[2] != 'A')
    {
        rig_debug(RIG_DEBUG_ERR, "%s; tt565_set_ant: ?KA NG %s\n",
                  __func__, respbuf);
        return -RIG_EPROTO;
    }

    /* respbuf="@KAxxx"
     * x='M'|'S'|'B'|'N'=main/sub/both/none for ants 1,2,3.
     * but hardware will not permit all combinations!
     * respbuf [3,4] can be MS, SM, BN, NB
     * decode to rx-centric view
     */
    if (respbuf[3] == 'M' || respbuf[3] == 'B') { main_ant = RIG_ANT_1; }
    else { main_ant = RIG_ANT_2; }

    if (respbuf[3] == 'S' || respbuf[3] == 'B') { sub_ant = RIG_ANT_1; }
    else { sub_ant = RIG_ANT_2; }

    switch (which_receiver(rig, vfo))
    {
    case 'M':
        main_ant = ant;
        break;

    case 'S':
        sub_ant = ant;
        break;

    default:
    {
        /* no change? */
    }
    }

    /* re-encode ant. settings into command */
    if (main_ant == RIG_ANT_1)
    {
        if (sub_ant == RIG_ANT_1)
        {
            respbuf[3] = 'B';
            respbuf[4] = 'N';
        }
        else
        {
            respbuf[3] = 'M';
            respbuf[4] = 'S';
        }
    }
    else if (sub_ant == RIG_ANT_2)
    {
        respbuf[3] = 'N';
        respbuf[4] = 'B';
    }
    else
    {
        respbuf[3] = 'S';
        respbuf[4] = 'M';
    }

    respbuf[0] = '*';   /* respbuf becomes a store command */
    respbuf[5] = 'N';   /* Force no rx on Ant 3 */
    respbuf[6] = EOM[0];
    respbuf[7] = 0;
    retval = tt565_transaction(rig, respbuf, 7, NULL, NULL);

    if (retval != RIG_OK)
    {
        return retval;
    }

    return RIG_OK;
}
/**
 * \param rig != NULL
 * \param vfo
 * \param ant receives antenna identifier
 * \returns RIG_OK or < 0
 * \brief Find what antenna is "attached" to our vfo
 *
 * \sa tt565_set_ant
 */
int tt565_get_ant(RIG *rig, vfo_t vfo, ant_t dummy, value_t *option,
                  ant_t *ant_curr, ant_t *ant_tx, ant_t *ant_rx)
{
    char respbuf[TT565_BUFSIZE];
    int resp_len, retval;

    resp_len = sizeof(respbuf);
    retval = tt565_transaction(rig, "?KA" EOM, 4, respbuf, &resp_len);

    if (retval != RIG_OK)
    {
        return retval;
    }

    if (respbuf[1] != 'K' || respbuf[2] != 'A' || resp_len != 7)
    {
        rig_debug(RIG_DEBUG_ERR, "%s; tt565_get_ant: NG %s\n",
                  __func__, respbuf);
        return -RIG_EPROTO;
    }

    /* Look for first occurrence of M or S in ant 1, 2, 3 characters */
    if (respbuf[3] == which_receiver(rig, vfo) || respbuf[3] == 'B')
    {
        *ant_curr = RIG_ANT_1;
        return RIG_OK;
    }

    if (respbuf[4] == which_receiver(rig, vfo) || respbuf[4] == 'B')
    {
        *ant_curr = RIG_ANT_2;
        return RIG_OK;
    }

    *ant_curr = RIG_ANT_NONE;    /* ignore possible RIG_ANT_3 = rx only ant */
    return RIG_OK;
}
/* End of orion.c */
/** @} */
