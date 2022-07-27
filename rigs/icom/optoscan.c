/*
 *  Hamlib CI-V backend - OptoScan extensions
 *  Copyright (c) 2000-2005 by Stephane Fillod
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

#include <stdio.h>
#include <stdlib.h>
#include <string.h>  /* String function definitions */

#include "hamlib/rig.h"
#include "serial.h"
#include "misc.h"
#include "cal.h"
#include "token.h"

#include "icom.h"
#include "icom_defs.h"
#include "frame.h"
#include "optoscan.h"


const struct confparams opto_ext_parms[] =
{
    { TOK_TAPECNTL, "tapecntl", "Toggle Tape Switch", "Toggles built in tape switch", 0, RIG_CONF_CHECKBUTTON, {} },
    { TOK_5KHZWIN,  "5khzwin", "Toggle 5kHz Search Window", "Toggles 5kHz search window", 0, RIG_CONF_CHECKBUTTON, {} },
    { TOK_SPEAKER,  "speaker", "Toggle speaker audio", "Toggles speaker audio", 0, RIG_CONF_CHECKBUTTON, {} },
    { TOK_AUDIO, "audio", "Audio present", "Audio present", NULL, RIG_CONF_CHECKBUTTON, {} },
    { TOK_DTMFPEND, "dtmfpend", "DTMF Digit Pending", "DTMF Digit Pending", NULL, RIG_CONF_CHECKBUTTON, {} },
    { TOK_DTMFOVRR, "dtmfovrr", "DTMF Buffer Overflow", "DTMF Buffer Overflow", NULL, RIG_CONF_CHECKBUTTON, {} },
    { TOK_CTCSSACT, "ctcssact", "CTCSS Tone Active", "CTCSS Tone Active", NULL, RIG_CONF_CHECKBUTTON, {} },
    { TOK_DCSACT,   "dcsact", "DCS Code Active", "DCS Code Active", NULL, RIG_CONF_CHECKBUTTON, {} },
    { RIG_CONF_END, NULL, }
};

static int optoscan_get_status_block(RIG *rig, struct optostat *status_block);
static int optoscan_send_freq(RIG *rig, vfo_t vfo, pltstate_t *state);
static int optoscan_RTS_toggle(RIG *rig);
static int optoscan_start_timer(RIG *rig, pltstate_t *state);
static int optoscan_wait_timer(RIG *rig, pltstate_t *state);

/*
 * optoscan_open
 * Assumes rig!=NULL, rig->state.priv!=NULL
 */
int optoscan_open(RIG *rig)
{
    struct icom_priv_data *priv;
    struct rig_state *rs;
    pltstate_t *pltstate;
    unsigned char ackbuf[16];
    int ack_len, retval;

    rs = &rig->state;
    priv = (struct icom_priv_data *)rs->priv;

    pltstate = calloc(1, sizeof(pltstate_t));

    if (!pltstate)
    {
        return -RIG_ENOMEM;
    }

    memset(pltstate, 0, sizeof(pltstate_t));
    priv->pltstate = pltstate;

    /* select REMOTE control */
    retval = icom_transaction(rig, C_CTL_MISC, S_OPTO_REMOTE,
                              NULL, 0, ackbuf, &ack_len);

    if (retval != RIG_OK)
    {
        free(pltstate);
        return retval;
    }

    if (ack_len != 1 || ackbuf[0] != ACK)
    {
        rig_debug(RIG_DEBUG_ERR, "optoscan_open: ack NG (%#.2x), "
                  "len=%d\n", ackbuf[0], ack_len);
        free(pltstate);
        return -RIG_ERJCTED;
    }

    return RIG_OK;
}

/*
 * optoscan_close
 * Assumes rig!=NULL, rig->state.priv!=NULL
 */
int optoscan_close(RIG *rig)
{
    struct icom_priv_data *priv;
    struct rig_state *rs;
    unsigned char ackbuf[16];
    int ack_len, retval;

    rs = &rig->state;
    priv = (struct icom_priv_data *)rs->priv;

    /* select LOCAL control */
    retval = icom_transaction(rig, C_CTL_MISC, S_OPTO_LOCAL,
                              NULL, 0, ackbuf, &ack_len);

    if (retval != RIG_OK)
    {
        return retval;
    }

    if (ack_len != 1 || ackbuf[0] != ACK)
    {
        rig_debug(RIG_DEBUG_ERR, "optoscan_close: ack NG (%#.2x), "
                  "len=%d\n", ackbuf[0], ack_len);
        return -RIG_ERJCTED;
    }

    free(priv->pltstate);

    return RIG_OK;
}

/*
 * optoscan_get_info
 * Assumes rig!=NULL, rig->state.priv!=NULL
 */
const char *optoscan_get_info(RIG *rig)
{
    unsigned char ackbuf[16];
    int ack_len, retval;
    static char info[64];

    /* select LOCAL control */
    retval = icom_transaction(rig, C_CTL_MISC, S_OPTO_RDID,
                              NULL, 0, ackbuf, &ack_len);

    if (retval != RIG_OK)
    {
        return NULL;
    }

    if (ack_len != 7)
    {
        rig_debug(RIG_DEBUG_ERR, "optoscan_get_info: ack NG (%#.2x), "
                  "len=%d\n", ackbuf[0], ack_len);
        return NULL;
    }

    SNPRINTF(info, sizeof(info), "OptoScan%c%c%c, software version %d.%d, "
             "interface version %d.%d\n",
             ackbuf[2], ackbuf[3], ackbuf[4],
             ackbuf[5] >> 4, ackbuf[5] & 0xf,
             ackbuf[6] >> 4, ackbuf[6] & 0xf);

    return info;
}

/*
 * optoscan_get_ctcss_tone
 * Assumes rig!=NULL, rig->state.priv!=NULL
 */
int optoscan_get_ctcss_tone(RIG *rig, vfo_t vfo, tone_t *tone)
{
    unsigned char tonebuf[MAXFRAMELEN];
    int tone_len, retval;

    retval = icom_transaction(rig, C_CTL_MISC, S_OPTO_RDCTCSS, NULL, 0,
                              tonebuf, &tone_len);

    if (retval != RIG_OK)
    {
        return retval;
    }

    if (tone_len != 4)
    {
        rig_debug(RIG_DEBUG_ERR, "optoscan_get_ctcss_tone: ack NG (%#.2x), "
                  "len=%d\n", tonebuf[0], tone_len);
        return -RIG_ERJCTED;
    }

    tone_len -= 2;

    *tone = from_bcd_be(tonebuf + 2, tone_len * 2);
    rig_debug(RIG_DEBUG_ERR, "optoscan_get_ctcss_tone: *tone=%u\n", *tone);

    return RIG_OK;
}


/*
 * optoscan_get_dcs_code
 * Assumes rig!=NULL, rig->state.priv!=NULL
 */
int optoscan_get_dcs_code(RIG *rig, vfo_t vfo, tone_t *code)
{
    unsigned char tonebuf[MAXFRAMELEN];
    int tone_len, retval;

    retval = icom_transaction(rig, C_CTL_MISC, S_OPTO_RDDCS, NULL, 0,
                              tonebuf, &tone_len);

    if (retval != RIG_OK)
    {
        return retval;
    }

    if (tone_len != 4)
    {
        rig_debug(RIG_DEBUG_ERR, "optoscan_get_dcs_code: ack NG (%#.2x), "
                  "len=%d\n", tonebuf[0], tone_len);
        return -RIG_ERJCTED;
    }

    tone_len -= 2;

    *code = from_bcd_be(tonebuf + 2, tone_len * 2);
    rig_debug(RIG_DEBUG_ERR, "optoscan_get_dcs_code: *code=%u\n", *code);

    return RIG_OK;
}

int optoscan_recv_dtmf(RIG *rig, vfo_t vfo, char *digits, int *length)
{
    unsigned char dtmfbuf[MAXFRAMELEN], digit;
    int len, digitpos;
    const unsigned char xlate[] = {'0', '1', '2', '3', '4', '5', '6',
                                   '7', '8', '9', 'A', 'B', 'C', 'D',
                                   '*', '#'
                                  };
    digitpos = 0;

    do
    {
        int retval = icom_transaction(rig, C_CTL_MISC, S_OPTO_RDDTMF,
                                      NULL, 0, dtmfbuf, &len);

        if (retval != RIG_OK)
        {
            return retval;
        }

        if (len != 3)
        {
            rig_debug(RIG_DEBUG_ERR, "optoscan_recv_dtmf: ack NG (%#.2x), len=%d\n",
                      dtmfbuf[0], len);
            return -RIG_ERJCTED;
        }

        digit = dtmfbuf[2];

        if (digit < 16)
        {
            digits[digitpos] = xlate[digit];
            digitpos++;
        }
    }
    while ((digit != 0x99) && (digitpos < *length));

    *length = digitpos;
    digits[digitpos] = 0;

    if (*length > 0)
    {
        rig_debug(RIG_DEBUG_ERR, "%s: %d digits - %s\n", __func__, *length, digits);
    }
    else
    {
        rig_debug(RIG_DEBUG_ERR, "%s: no digits to read.\n", __func__);
    }

    return RIG_OK;
}

/*
 * Assumes rig!=NULL, rig->state.priv!=NULL
 */
int optoscan_set_ext_parm(RIG *rig, token_t token, value_t val)
{
    unsigned char epbuf[MAXFRAMELEN], ackbuf[MAXFRAMELEN];
    int ack_len;
    int retval, subcode;

    memset(epbuf, 0, MAXFRAMELEN);
    memset(ackbuf, 0, MAXFRAMELEN);

    switch (token)
    {
    case TOK_TAPECNTL:
        if (val.i == 0)
        {
            subcode = S_OPTO_TAPE_OFF;
        }
        else
        {
            subcode = S_OPTO_TAPE_ON;
        }

        break;

    case TOK_5KHZWIN:
        if (val.i == 0)
        {
            subcode = S_OPTO_5KSCOFF;
        }
        else
        {
            subcode = S_OPTO_5KSCON;
        }

        break;

    case TOK_SPEAKER:
        if (val.i == 0)
        {
            subcode = S_OPTO_SPKROFF;
        }
        else
        {
            subcode = S_OPTO_SPKRON;
        }

        break;

    default:
        return -RIG_EINVAL;
    }

    retval = icom_transaction(rig, C_CTL_MISC, subcode, epbuf, 0,
                              ackbuf, &ack_len);

    if (retval != RIG_OK)
    {
        return retval;
    }

    if (ack_len != 1 || ackbuf[0] != ACK)
    {
        rig_debug(RIG_DEBUG_ERR, "%s: ack NG (%#.2x), "
                  "len=%d\n", __func__, ackbuf[0], ack_len);
        return -RIG_ERJCTED;
    }

    return RIG_OK;
}

/*
 * Assumes rig!=NULL, rig->state.priv!=NULL
 *  and val points to a buffer big enough to hold the conf value.
 */
int optoscan_get_ext_parm(RIG *rig, token_t token, value_t *val)
{
    struct optostat status_block;
    int retval;

    retval = optoscan_get_status_block(rig, &status_block);

    if (retval != RIG_OK)
    {
        return retval;
    }

    switch (token)
    {
    case TOK_TAPECNTL:
        val->i = status_block.tape_enabled;
        break;

    case TOK_5KHZWIN:
        val->i = status_block.fivekhz_enabled;
        break;

    case TOK_SPEAKER:
        val->i = status_block.speaker_enabled;
        break;

    case TOK_AUDIO:
        val->i = status_block.audio_present;
        break;

    case TOK_DTMFPEND:
        val->i = status_block.DTMF_pending;
        break;

    case TOK_DTMFOVRR:
        val->i = status_block.DTMF_overrun;
        break;

    case TOK_CTCSSACT:
        val->i = status_block.CTCSS_active;
        break;

    case TOK_DCSACT:
        val->i = status_block.DCS_active;
        break;

    default:
        return -RIG_ENIMPL;
    }

    return RIG_OK;
}

/*
 * optoscan_set_level
 * Assumes rig!=NULL, rig->state.priv!=NULL
 */
int optoscan_set_level(RIG *rig, vfo_t vfo, setting_t level, value_t val)
{
    unsigned char lvlbuf[MAXFRAMELEN], ackbuf[MAXFRAMELEN];
    int ack_len;
    int lvl_cn, lvl_sc;     /* Command Number, Subcommand */
    int icom_val;
    int retval;

    memset(lvlbuf, 0, MAXFRAMELEN);

    /*
     * So far, levels of float type are in [0.0..1.0] range
     */
    if (RIG_LEVEL_IS_FLOAT(level))
    {
        icom_val = val.f * 255;
    }
    else
    {
        icom_val = val.i;
    }

    switch (level)
    {
    case RIG_LEVEL_AF:
        lvl_cn = C_CTL_MISC;

        if (icom_val == 0)
        {
            lvl_sc = S_OPTO_SPKROFF;
        }
        else
        {
            lvl_sc = S_OPTO_SPKRON;
        }

        break;

    default:
        rig_debug(RIG_DEBUG_ERR, "Unsupported set_level %s", rig_strlevel(level));
        return -RIG_EINVAL;
    }

    retval = icom_transaction(rig, lvl_cn, lvl_sc, lvlbuf, 0,
                              ackbuf, &ack_len);

    if (retval != RIG_OK)
    {
        return retval;
    }

    if (ack_len != 1 || ackbuf[0] != ACK)
    {
        rig_debug(RIG_DEBUG_ERR, "optoscan_set_level: ack NG (%#.2x), "
                  "len=%d\n", ackbuf[0], ack_len);
        return -RIG_ERJCTED;
    }

    return RIG_OK;
}

/*
 * optoscan_get_level
 * Assumes rig!=NULL, rig->state.priv!=NULL, val!=NULL
 */
int optoscan_get_level(RIG *rig, vfo_t vfo, setting_t level, value_t *val)
{
    struct optostat status_block;
    int lvl_len = 0;
    int icom_val;
    int retval;

    if (level != RIG_LEVEL_AF)
    {
        int lvl_cn, lvl_sc;     /* Command Number, Subcommand */
        unsigned char lvlbuf[MAXFRAMELEN];
        int cmdhead;

        switch (level)
        {
        case RIG_LEVEL_RAWSTR:
            lvl_cn = C_RD_SQSM;
            lvl_sc = S_SML;
            break;

        default:
            rig_debug(RIG_DEBUG_ERR, "Unsupported get_level %s", rig_strlevel(level));
            return -RIG_EINVAL;
        }

        retval = icom_transaction(rig, lvl_cn, lvl_sc, NULL, 0,
                                  lvlbuf, &lvl_len);

        if (retval != RIG_OK)
        {
            return retval;
        }

        /*
         * strbuf should contain Cn,Sc,Data area
         */
        cmdhead = (lvl_sc == -1) ? 1 : 2;
        lvl_len -= cmdhead;

        if (lvlbuf[0] != ACK && lvlbuf[0] != lvl_cn)
        {
            rig_debug(RIG_DEBUG_ERR, "optoscan_get_level: ack NG (%#.2x), "
                      "len=%d\n", lvlbuf[0], lvl_len);
            return -RIG_ERJCTED;
        }

        /*
         * The result is a 3 digit BCD, but in *big endian* order: 0000..0255
         * (from_bcd is little endian)
         */
        icom_val = from_bcd_be(lvlbuf + cmdhead, lvl_len * 2);
    }
    else     /* level == RIG_LEVEL_AF */
    {
        retval = optoscan_get_status_block(rig, &status_block);

        if (retval != RIG_OK)
        {
            return retval;
        }

        icom_val = 0;

        if (status_block.speaker_enabled == 1)
        {
            icom_val = 255;
        }
    }

    switch (level)
    {
    case RIG_LEVEL_RAWSTR:
        val->i = icom_val;
        break;

    default:
        if (RIG_LEVEL_IS_FLOAT(level))
        {
            val->f = (float)icom_val / 255;
        }
        else
        {
            val->i = icom_val;
        }
    }

    rig_debug(RIG_DEBUG_TRACE, "optoscan_get_level: %d %d %d %f\n", lvl_len,
              icom_val, val->i, val->f);

    return RIG_OK;
}

/* OS456 Pipeline tuning algorithm:
 *  Step 2:  Send the next frequency and mode to the receiver using the
 *       TRANSFER NEXT FREQUENCY/MODE command.
 *
 *      Step 3:  Change the state of the RTS interface signal to cause the
 *               next frequency and mode to become the current frequency and
 *               mode, and the receiver to begin settling.
 *
 *  Step 4:  While the receiver is still settling on the current
 *       frequency and mode, send the next frequency and mode to the
 *       receiver using the TRANSFER NEXT FREQUENCY/MODE command.
 *
 *  Step 5:  Wait for the receiver to finish settling.  The total
 *       settling time, including sending the next frequency and
 *       mode, is 20 milliseconds (0.02 seconds).
 *
 *  Step 6:  Check the squelch status by reading the DCD interface
 *       signal.  If the squelch is open, scanning is stopped.
 *       Otherwise, scanning continues.  Optionally, the status of
 *       the CTCSS/DCS/DTMF decoder can be checked, and the
 *       appropriate action taken.
 *
 *  Step 7:  Continuously repeat steps 3 through 6 above.
 */
int optoscan_scan(RIG *rig, vfo_t vfo, scan_t scan, int ch)
{
    pltstate_t *state;
    pltune_cb_t cb;
    int rc, pin_state;
    struct rig_state *rs;

    if (scan != RIG_SCAN_PLT)
    {
        return -RIG_ENAVAIL;
    }

    rs = &rig->state;
    cb = rig->callbacks.pltune;
    state = ((struct icom_priv_data *)rs->priv)->pltstate;

    if (state == NULL)
    {
        return -RIG_EINTERNAL;
    }

    if (state->freq == 0)   /* pltstate_t is not initialized - perform setup */
    {
        /* time for CIV command to be sent. this is subtracted from */
        /* rcvr settle time */
        state->usleep_time = (1000000 / (rig->state.rigport.parm.serial.rate))
                             * 13 * 9;

        rc = cb(rig, vfo, &(state->next_freq), &(state->next_mode),
                &(state->next_width), rig->callbacks.pltune_arg);

        if (rc == RIG_SCAN_STOP)
        {
            return RIG_OK;    /* callback halted loop */
        }

        /* Step 1 is implicit, since hamlib does this when it opens the device */
        optoscan_send_freq(rig, vfo, state); /*Step 2*/
    }

    rc = 0;

    while (rc != RIG_SCAN_STOP)
    {
        optoscan_RTS_toggle(rig); /*Step 3*/

        state->freq = state->next_freq;
        state->mode = state->next_mode;

        optoscan_start_timer(rig, state);

        rc = cb(rig, vfo, &(state->next_freq), &(state->next_mode),
                &(state->next_width), rig->callbacks.pltune_arg);

        if (rc != RIG_SCAN_STOP)
        {
            optoscan_send_freq(rig, vfo, state); /*Step 4*/
        }

        optoscan_wait_timer(rig, state); /*Step 5*/

        ser_get_car(&rs->rigport, &pin_state);

        if (pin_state)   /*Step 6*/
        {
            return RIG_OK; /* we've broken squelch - return(). caller can */
            /* get current freq & mode out of state str    */
        }
    }

    /* exiting pipeline loop - force state init on next call */
    state->freq = 0;

    return RIG_OK;
}

/*
 * Assumes rig!=NULL, status_block !=NULL
 */
static int optoscan_get_status_block(RIG *rig, struct optostat *status_block)
{
    int retval, ack_len, expected_len;
    unsigned char ackbuf[MAXFRAMELEN];

    memset(status_block, 0, sizeof(struct optostat));

    retval = icom_transaction(rig, C_CTL_MISC, S_OPTO_RDSTAT, NULL, 0,
                              ackbuf, &ack_len);

    if (retval != RIG_OK)
    {
        return retval;
    }

    switch (rig->caps->rig_model)
    {
    case RIG_MODEL_OS456:
        expected_len = 4;
        break;

    case RIG_MODEL_OS535:
        expected_len = 5;
        break;

    default:
        rig_debug(RIG_DEBUG_ERR, "%s: unknown rig model", __func__);
        return -RIG_ERJCTED;
        break;
    }

    if (ack_len != expected_len)
    {
        rig_debug(RIG_DEBUG_ERR, "%s: ack NG (%#.2x), len=%d\n", __func__,
                  ackbuf[0], ack_len);
        return -RIG_ERJCTED;
    }

    if (ackbuf[2] & 1) { status_block->remote_control = 1; }

    if (ackbuf[2] & 2) { status_block->DTMF_pending = 1; }

    if (ackbuf[2] & 4) { status_block->DTMF_overrun = 1; }

    if (ackbuf[2] & 16) { status_block->squelch_open = 1; }

    if (ackbuf[2] & 32) { status_block->CTCSS_active = 1; }

    if (ackbuf[2] & 64) { status_block->DCS_active = 1; }

    if (ackbuf[3] & 1) { status_block->tape_enabled = 1; }

    if (ackbuf[3] & 2) { status_block->speaker_enabled = 1; }

    if (ackbuf[3] & 4) { status_block->fivekhz_enabled = 1; }

    if (ackbuf[3] & 16) { status_block->audio_present = 1; }

    rig_debug(RIG_DEBUG_VERBOSE, "remote_control     = %d\n",
              status_block->remote_control);
    rig_debug(RIG_DEBUG_VERBOSE, "DTMF_pending       = %d\n",
              status_block->DTMF_pending);
    rig_debug(RIG_DEBUG_VERBOSE, "DTMF_overrun       = %d\n",
              status_block->DTMF_overrun);
    rig_debug(RIG_DEBUG_VERBOSE, "squelch_open       = %d\n",
              status_block->squelch_open);
    rig_debug(RIG_DEBUG_VERBOSE, "CTCSS_active       = %d\n",
              status_block->CTCSS_active);
    rig_debug(RIG_DEBUG_VERBOSE, "DCS_active         = %d\n",
              status_block->DCS_active);
    rig_debug(RIG_DEBUG_VERBOSE, "tape_enabled       = %d\n",
              status_block->tape_enabled);
    rig_debug(RIG_DEBUG_VERBOSE, "speaker_enabled    = %d\n",
              status_block->speaker_enabled);
    rig_debug(RIG_DEBUG_VERBOSE, "fivekhz_enabled    = %d\n",
              status_block->fivekhz_enabled);
    rig_debug(RIG_DEBUG_VERBOSE, "audio_present      = %d\n",
              status_block->audio_present);

    return RIG_OK;
}


static int optoscan_send_freq(RIG *rig, vfo_t vfo, pltstate_t *state)
{
    unsigned char buff[OPTO_BUFF_SIZE];
    char md, pd;
    freq_t freq;
    rmode_t mode;

    freq = state->next_freq;
    mode = state->next_mode;

    memset(buff, 0, OPTO_BUFF_SIZE);

    to_bcd(buff, freq, 5 * 2); /* to_bcd requires nibble len */

    rig2icom_mode(rig, vfo, mode, 0, (unsigned char *) &md, (signed char *) &pd);
    buff[5] = md;

    /* read echo'd chars only...there will be no ACK from this command
     *
     * Note:
     *  It may have waited for pltstate->usleep_time before reading the echo'd
     *  chars, but the read will be blocking anyway. --SF
     * */
    return icom_transaction(rig, C_CTL_MISC, S_OPTO_NXT, buff, 6, NULL, NULL);
}

static int optoscan_RTS_toggle(RIG *rig)
{
    struct rig_state *rs;
    int state = 0;

    rs = &rig->state;
    ser_get_rts(&rs->rigport, &state);
    ser_set_rts(&rs->rigport, !state);

    return RIG_OK;
}

static int optoscan_start_timer(RIG *rig, pltstate_t *state)
{
    gettimeofday(&(state->timer_start), NULL);

    return RIG_OK;
}

static int optoscan_wait_timer(RIG *rig, pltstate_t *state)
{
    struct icom_priv_caps *priv_caps;
    int usec_diff;
    int settle_usec;

    priv_caps = (struct icom_priv_caps *)rig->caps->priv;
    settle_usec = priv_caps->settle_time * 1000; /*convert settle time (ms) to */
    /* settle time (usec)         */

    gettimeofday(&(state->timer_current), NULL);

    usec_diff = (int)labs((state->timer_current.tv_usec) -
                          (state->timer_start.tv_usec));

    if (usec_diff < settle_usec)
    {
        hl_usleep(settle_usec - usec_diff);   /* sleep balance of settle_time */
    }

    return RIG_OK;
}
