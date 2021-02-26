/*
 *  Hamlib PRM80 backend - main file
 *  Copyright (c) 2010,2021 by Stephane Fillod
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

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <stdio.h>
#include <stdlib.h>
#include <string.h>  /* String function definitions */
#include <unistd.h>  /* UNIX standard function definitions */
#include <math.h>

#include "hamlib/rig.h"
#include "serial.h"
#include "misc.h"
#include "cal.h"
#include "register.h"
#include "idx_builtin.h"

#include "prm80.h"


#define LF "\x0a"

#define BUFSZ 64

// Channel number min and max
#define CHAN_MIN 0
#define CHAN_MAX 99

#define RX_IF_OFFSET MHz(21.4)

// The rig's PLL only deals with freq in Hz divided by this value
#define FREQ_DIV 12500.

/* V4 commands
 * retrieved from https://github.com/f4fez/prm80
 * and https://github.com/f4fez/prm80/blob/master/doc/Computer_commands_V4.md
 * It used to be from https://sourceforge.net/projects/prm80/
 * and https://sourceforge.net/p/prm80/wiki/Computer%20commands%20V4/

MessageVersion:
IF TARGET EQ 8060
              DB   "PRM8060 V4.0"
ELSEIF TARGET EQ 8070
              DB   "PRM8070 V4.0"
ENDIF

MessageAide:  DB   "H",0Dh,0Ah
              DB   " Commandes disponibles :",0Dh,0Ah
              DB   " [0] = Reset.",0Dh,0Ah
              DB   " [1] a [5] = Show 80c552 port state P1 to P5.",0Dh,0Ah
              DB   " [C] = Print channels list.",0Dh,0Ah
              DB   " [D] = Set system byte.",0Dh,0Ah
              DB   " [E] = Show system state (Mode-Chan-Chanstate-Sql-Vol-Lock-RX freq-TX freq).",0Dh,0Ah
              DB   " [F] = Set squelch.",0Dh,0Ah
              DB   " [H] = Print this help page.",0Dh,0Ah
              DB   " [I] = Erase and init RAM and EEPROM.",0Dh,0Ah
              DB   " [K] = Set lock byte.",0Dh,0Ah
              DB   " [L] = Print latch state.",0Dh,0Ah
              DB   " [M] = Edit external RAM manualy.",0Dh,0Ah
              DB   " [N] = Set current channel.",0Dh,0Ah
              DB   " [O] = Set volume.",0Dh,0Ah
              DB   " [P] = Edit/Add channel.",0Dh,0Ah
              DB   " [Q] = Set channels number.",0Dh,0Ah
              DB   " [R] = Set synthetiser frequencies.",0Dh,0Ah
              DB   " [U] = Print 80c552 internal RAM.",0Dh,0Ah
              DB   " [S] = Copy EEPROM to external RAM.",0Dh,0Ah
              DB   " [T] = Set current channel state.",0Dh,0Ah
              DB   " [V] = Print firmware version.",0Dh,0Ah
              DB   " [X] = Copy external RAM to EEPROM.",0Dh,0Ah
              DB   " [Y] = Print first 2 kb from the EEPROM I2C 24c16.",0Dh,0Ah
              DB   " [Z] = Print external RAM ($0000 to $07FF).",0Dh,0Ah,0
*/
/*
[0] = Reset.
[C] = Print channels list.
[D] = Set system byte.

[E] = Show system state (Mode-Chan-Chanstate-Sql-Vol-Lock-RX freq-TX
                freq).
[F] = Set squelch.
[H] = Print this help page.
[K] = Set lock byte.
[N] = Set current channel.
[O] = Set volume.
[P] = Edit/Add channel.
[Q] = Set channels number.
[R] = Set synthetiser frequencies.
[T] = Set current channel state.
[V] = Print firmware version.
*/

/*
 * Mode byte, which holds the state of system basic features:
    b0: Squelch mode is displayed on LCD if true. Channel mode if false.
    b1: Power level (High or Low mode)
    b2: Squelch open (Read only)
    b3: TX mode (Read only)
    b4: PLL locked (Read only)
    b5: Long key push (Internal)
    b6: Key bounce (Internal)
    b7: Force LCD refresh when set. Automaticaly cleared.

   Channel state byte:
    b0: Shift enable when true
    b1: Reverse mode when true
    b2: Positive shift when true. Negative if false
    b3: Scanning locked out channel if set
    b4-7: na.

   Lock byte, which disables user controls when connected to a computer
    b0: Keys disabled when true
    b1: TX disabled when true
    b2: Volume button disabled when true
    b3: RX disabled when true
    b4-b7: na.

 * *********************************************************************
 */

/*
 * TODO make read_colon_prompt_and_send() more generic to read
 *      a prompt terminated by "$" (without space afterwards)
 */
#define read_dollar_prompt_and_send read_colon_prompt_and_send

/*
 * Read a prompt terminated by ": ", then write an optional string s.
 */
static int read_colon_prompt_and_send(hamlib_port_t *rigport,
                                      char *data, int *data_len, const char *s)
{
    char buf[BUFSZ];
    char spacebuf[4];
    int buflen, retval;

    /* no data wanted? flush it anyway by reading it */
    if (data == NULL)
    {
        data = buf;
    }

    buflen = (data_len == NULL) ? sizeof(buf) : *data_len;

    retval = read_string(rigport, data, buflen, ":", 1);

    if (retval < 0)
    {
        return retval;
    }

    // Place an end of string
    data[(retval < buflen) ? retval : (buflen - 1)] = '\0';

    if (data_len != NULL)
    {
        *data_len = retval;
    }

    // Read one (dummy) space character after the colon
    retval = read_block(rigport, spacebuf, 1);

    if (retval < 0 && retval != -RIG_ETIMEOUT)
    {
        return retval;
    }

    // Here is the answer to the prompt
    retval = write_block(rigport, s, strlen(s));

    return retval;
}

/*
 * After each executed command, the rig generally sends "\r\n>"
 */
static int prm80_wait_for_prompt(hamlib_port_t *rigport)
{
    char buf[BUFSZ * 2];
    int retval;

    // Read up to the '>' prompt and discard content.
    retval = read_string(rigport, buf, sizeof(buf), ">", 1);

    if (retval < 0)
    {
        return retval;
    }

    return RIG_OK;
}

/*
 *
 * \param cmd is string of generally one letter (or digit)
 * \param arg1 is an optional string string sent
 * \param wait_prompt boolean when non-nul, will wait for "\r\n>" afterwards
 */
static int prm80_transaction(RIG *rig, const char *cmd,
                             const char *arg1, int wait_prompt)
{
    int retval;
    struct rig_state *rs = &rig->state;

    // Get rid of possible prompt sent by the rig
    rig_flush(&rs->rigport);

    // Start with the command
    retval = write_block(&rs->rigport, cmd, strlen(cmd));

    if (retval != RIG_OK)
    {
        return retval;
    }

    if (arg1 != NULL)
    {
        retval = read_colon_prompt_and_send(&rs->rigport, NULL, NULL, arg1);

        if (retval < 0)
        {
            return retval;
        }
    }

    if (wait_prompt)
    {
        prm80_wait_for_prompt(&rs->rigport);
    }

    return RIG_OK;
}

int prm80_init(RIG *rig)
{
    if (!rig)
    {
        return -RIG_EINVAL;
    }

    rig->state.priv = (void *)calloc(1, sizeof(struct prm80_priv_data));

    if (!rig->state.priv)
    {
        /* whoops! memory shortage! */
        return -RIG_ENOMEM;
    }

    return RIG_OK;
}

int prm80_cleanup(RIG *rig)
{
    if (rig == NULL)
    {
        return -RIG_EINVAL;
    }

    free(rig->state.priv);
    rig->state.priv = NULL;

    return RIG_OK;
}


/*
 * prm80_reset
 * Assumes rig!=NULL
 */
int prm80_reset(RIG *rig, reset_t reset)
{
    int retval;

    /*
     * Reset CPU
     */
    retval = prm80_transaction(rig, "0", NULL, 1);

    if (retval != RIG_OK)
    {
        return retval;
    }

    return RIG_OK;
}


/*
 * Set RX and TX freq
 *
 * See https://github.com/f4fez/prm80/blob/master/doc/Computer_control.md
 *   "Adding a new channel" regarding freq format.
 */
int prm80_set_rx_tx_freq(RIG *rig, freq_t rx_freq, freq_t tx_freq)
{
    struct rig_state *rs = &rig->state;
    char rx_freq_buf[BUFSZ];
    char tx_freq_buf[BUFSZ];
    int rc;

    // for RX, compute the PLL word without the IF
    sprintf(rx_freq_buf, "%04X",
            (unsigned)((rx_freq - RX_IF_OFFSET) / FREQ_DIV));
    sprintf(tx_freq_buf, "%04X",
            (unsigned)(tx_freq / FREQ_DIV));

    // The protocol is like this :
    // "RX frequency : " XXXX
    // CRLF"TX frequency : " XXXX

    rc = prm80_transaction(rig, "R", rx_freq_buf, 0);

    if (rc != RIG_OK)
    {
        return rc;
    }

    // There's a second line to process after prm80_transaction()
    rc = read_colon_prompt_and_send(&rs->rigport, NULL, NULL, tx_freq_buf);

    if (rc != RIG_OK)
    {
        return rc;
    }

    // quid timeout in trx waiting for freq ?

    // NB: the [R] command does not update the checksum of the RAM!

    prm80_wait_for_prompt(&rs->rigport);

    return rc;
}

/*
 * Set (RX) freq
 */
int prm80_set_freq(RIG *rig, vfo_t vfo, freq_t freq)
{
    struct prm80_priv_data *priv = (struct prm80_priv_data *)rig->state.priv;
    freq_t tx_freq;
    int rc;

    if (priv->split == RIG_SPLIT_OFF)
    {
        tx_freq = freq;
    }
    else
    {
        tx_freq = (priv->tx_freq == 0.) ? freq : priv->tx_freq;
    }

    rc = prm80_set_rx_tx_freq(rig, freq, tx_freq);

    if (rc == RIG_OK)
    {
        priv->rx_freq = freq;
    }

    return rc;
}

/*
 * Set TX freq depending on emulated split state
 */
int prm80_set_split_freq(RIG *rig, vfo_t vfo, freq_t tx_freq)
{
    struct prm80_priv_data *priv = (struct prm80_priv_data *)rig->state.priv;
    freq_t rx_freq;
    int rc;

    rx_freq = (priv->rx_freq == 0.) ? tx_freq : priv->rx_freq;

    rc = prm80_set_rx_tx_freq(rig, rx_freq, tx_freq);

    if (rc == RIG_OK)
    {
        priv->tx_freq = tx_freq;
    }

    return rc;
}

/*
 * Get RX freq depending on emulated split state
 */
int prm80_get_freq(RIG *rig, vfo_t vfo, freq_t *freq)
{
    struct prm80_priv_data *priv = (struct prm80_priv_data *)rig->state.priv;
    int ret;
    channel_t chan;

    memset(&chan, 0, sizeof(chan));
    chan.vfo = RIG_VFO_CURR;

    ret = prm80_get_channel(rig, vfo, &chan, 0);

    if (ret != RIG_OK)
    {
        return ret;
    }

    *freq = chan.freq;
    priv->tx_freq = chan.tx_freq;

    return RIG_OK;
}

/*
 * Enable/disable Split
 *
 * Rem: don't care about vfo
 */
int prm80_set_split_vfo(RIG *rig, vfo_t vfo, split_t split, vfo_t tx_vfo)
{
    struct prm80_priv_data *priv = (struct prm80_priv_data *)rig->state.priv;

    priv->split = split;

    return RIG_OK;
}

/*
 * Get Split
 */
int prm80_get_split_vfo(RIG *rig, vfo_t vfo, split_t *split, vfo_t *tx_vfo)
{
    struct prm80_priv_data *priv = (struct prm80_priv_data *)rig->state.priv;

    *split = priv->split;
    *tx_vfo = RIG_VFO_CURR;

    return RIG_OK;
}

/*
 * Get TX freq
 */
int prm80_get_split_freq(RIG *rig, vfo_t vfo, freq_t *tx_freq)
{
    struct prm80_priv_data *priv = (struct prm80_priv_data *)rig->state.priv;
    int ret;
    channel_t chan;

    memset(&chan, 0, sizeof(chan));
    chan.vfo = RIG_VFO_CURR;

    ret = prm80_get_channel(rig, vfo, &chan, 0);

    if (ret != RIG_OK)
    {
        return ret;
    }

    *tx_freq = chan.tx_freq;
    priv->rx_freq = chan.freq;

    return RIG_OK;
}

/*
 * Basic helper to ease some generic applications
 */
int prm80_get_mode(RIG *rig, vfo_t vfo, rmode_t *mode, pbwidth_t *width)
{
    // Can only do FM
    *mode = RIG_MODE_FM;
    *width = rig_passband_normal(rig, *mode);

    return RIG_OK;
}


/*
 * prm80_set_mem
 * Assumes rig!=NULL
 */
int prm80_set_mem(RIG *rig, vfo_t vfo, int ch)
{
    char chbuf[BUFSZ];

    /* [N] = Set current channel. */

    if (ch < CHAN_MIN || ch > CHAN_MAX)
    {
        return -RIG_EINVAL;
    }

    sprintf(chbuf, "%02u", (unsigned)ch);

    // Send command, no answer expected from rig except ">" prompt

    return prm80_transaction(rig, "N", chbuf, 1);
}

/*
 * prm80_get_mem
 * Assumes rig!=NULL
 */
int prm80_get_mem(RIG *rig, vfo_t vfo, int *ch)
{
    int ret;
    channel_t chan;

    memset(&chan, 0, sizeof(chan));
    chan.vfo = RIG_VFO_CURR;

    ret = prm80_get_channel(rig, vfo, &chan, 0);

    if (ret != RIG_OK)
    {
        return ret;
    }

    *ch = chan.channel_num;

    return RIG_OK;
}

/*
 * Convert first two hexadecimal digit to integer
 */
static int hhtoi(const char *p)
{
    char buf[4];

    buf[0] = p[0];
    buf[1] = p[1];
    buf[2] = '\0';

    return (int)strtol(buf, NULL, 16);
}

/*
 * prm80_get_channel
 * Assumes rig!=NULL
 */
int prm80_get_channel(RIG *rig, vfo_t vfo, channel_t *chan, int read_only)
{
    struct prm80_priv_data *priv = (struct prm80_priv_data *)rig->state.priv;
    struct rig_state *rs = &rig->state;
    char statebuf[BUFSZ];
    char *p;
    int ret, chanstate, mode_byte, lock_byte;

    if (chan->vfo == RIG_VFO_MEM)
    {
        ret = prm80_set_mem(rig, RIG_VFO_CURR, chan->channel_num);

        if (ret != RIG_OK)
        {
            return ret;
        }
    }

    // Get rid of possible prompt sent by the rig
    rig_flush(&rs->rigport);

    /* [E] = Show system state */
    ret = write_block(&rs->rigport, "E", 1);

    if (ret < 0)
    {
        RETURNFUNC(ret);
    }

    // The response length is fixed
    ret = read_block(&rs->rigport, statebuf, 20);

    if (ret < 0)
    {
        return ret;
    }

    if (ret >= 0)
    {
        statebuf[ret] = '\0';
    }

    if (ret < 20)
    {
        rig_debug(RIG_DEBUG_ERR, "%s: len=%d < 20, statebuf='%s'\n", __func__,
                  ret, statebuf);
        RETURNFUNC(-RIG_EPROTO);
    }

    p = strchr(statebuf, '>');

    if (p)
    {
        int left_to_read = (p - statebuf) + 1;
        memmove(statebuf, p + 1, 20 - left_to_read);
        ret = read_block(&rs->rigport, statebuf + 20 - left_to_read, left_to_read);

        if (ret >= 0)
        {
            statebuf[20] = '\0';
        }

        rig_debug(RIG_DEBUG_WARN, "%s: len=%d, statebuf='%s'\n", __func__, ret,
                  statebuf);
    }

    /* (Mode-Chan-Chanstate-Sql-Vol-Lock-RX freq-TX freq). */
    /* Examples: 1240080AFF0033F02D40 or 14000C00FD0079708020 */

    /* Current mode:
       ; b0: Squelch       b1: power
       ; b2: Squelch open  b3: TX
       ; b4: PLL locked    b5: Long press memorize
       ; b6: Debouncing in effect b7: LCD refresh
      */
    mode_byte = hhtoi(statebuf);

    chan->mode = RIG_MODE_FM;
    chan->width = rig_passband_normal(rig, chan->mode);
    chan->channel_num = hhtoi(statebuf + 2);
    chan->tx_mode = chan->mode;
    chan->tx_width = chan->width;

    /* Chan state:
       ; b0: shift enabled   b1: reverse
       ; b2: shift +         b3: lock out
     */
    chanstate = hhtoi(statebuf + 4) & 0x0f;
    /* is it rptr_shift or split mode ? */
    chan->rptr_shift = (chanstate & 0x01) == 0 ? RIG_RPT_SHIFT_NONE :
                       (chanstate & 0x02) ? RIG_RPT_SHIFT_MINUS :
                       (chanstate & 0x04) ? RIG_RPT_SHIFT_PLUS : RIG_RPT_SHIFT_NONE;
    chan->flags = (chanstate & 0x08) ? RIG_CHFLAG_SKIP : 0;

    // cppcheck-suppress *
    chan->levels[LVL_SQL].f = ((float)(hhtoi(statebuf + 6) >> 4)) / 15.;
    chan->levels[LVL_AF].f  = ((float)(hhtoi(statebuf + 8) >> 4)) / 15.;
    chan->levels[LVL_RFPOWER].f  = (mode_byte & 0x02) ? 1.0 : 0.0;

    chan->funcs |= (chanstate & 0x02) ? RIG_FUNC_REV : 0;

    lock_byte = hhtoi(statebuf + 10) & 0x0f;
    chan->funcs = (lock_byte != 0) ? RIG_FUNC_LOCK : 0;

    chan->freq = ((hhtoi(statebuf + 12) << 8) + hhtoi(statebuf + 14)) * FREQ_DIV +
                 RX_IF_OFFSET;
    chan->tx_freq = ((hhtoi(statebuf + 16) << 8) + hhtoi(statebuf + 18)) * FREQ_DIV;

    if (chan->rptr_shift != RIG_RPT_SHIFT_NONE)
    {
        chan->rptr_offs = chan->tx_freq - chan->freq;
        chan->split = RIG_SPLIT_OFF;
    }
    else
    {
        chan->rptr_offs = 0;
        chan->split = priv->split; // RIG_SPLIT_ON; ?
    }

    if (!read_only)
    {
        // Set rig to channel values
        rig_debug(RIG_DEBUG_WARN,
                  "%s: please contact hamlib mailing list to implement this\n", __func__);
        rig_debug(RIG_DEBUG_WARN,
                  "%s: need to know if rig updates when channel read or not\n", __func__);
        //return -RIG_ENIMPL;
    }

    prm80_wait_for_prompt(&rs->rigport);

    return RIG_OK;
}

/*
 * prm80_set_channel handles RIG_VFO_MEM and RIG_VFO_CURR
 */
int prm80_set_channel(RIG *rig, vfo_t vfo, const channel_t *chan)
{
    struct prm80_priv_data *priv = (struct prm80_priv_data *)rig->state.priv;
    struct rig_state *rs = &rig->state;
    char buf[BUFSZ];
    int ret, chanstate;
    freq_t tx_freq;

    if (chan->vfo == RIG_VFO_MEM)
    {
        // setting channel without calling set_mem()

        if (chan->channel_num < CHAN_MIN || chan->channel_num > CHAN_MAX)
        {
            return -RIG_EINVAL;
        }

        /* [P] = Edit/Add channel */
        /* Example
           Channel to set : 00
           PLL value to load : $8020
           Channel state : $00

           TODO: handle the possible query from the rig:
            "This channel number doesn't exist. Add new channel (Y/N) ? "
           TODO implement correctly read_dollar_prompt_and_send (dollar prompt)
           */

        sprintf(buf, "%02u", (unsigned)chan->channel_num);

        ret = prm80_transaction(rig, "P", buf, 0);

        if (ret != RIG_OK)
        {
            return ret;
        }

        // Set the RX frequency as PLL word
        sprintf(buf, "%04X", (unsigned)((chan->freq - RX_IF_OFFSET) / FREQ_DIV));
        ret = read_dollar_prompt_and_send(&rs->rigport, NULL, NULL, buf);

        if (ret != RIG_OK)
        {
            return ret;
        }

        // the channel status byte.
        switch (chan->rptr_shift)
        {
        case RIG_RPT_SHIFT_NONE : chanstate = 0x00; break;

        case RIG_RPT_SHIFT_MINUS : chanstate = 0x03; break;

        case RIG_RPT_SHIFT_PLUS : chanstate = 0x05; break;

        default: chanstate = 0x00; break;
        }

        chanstate |= (chan->flags & RIG_CHFLAG_SKIP) ? 0x08 : 0;

        sprintf(buf, "%02X", chanstate);
        ret = read_dollar_prompt_and_send(&rs->rigport, NULL, NULL, buf);

        if (ret != RIG_OK)
        {
            return ret;
        }

        prm80_wait_for_prompt(&rs->rigport);
    }
    else
    {
        // assume here chan->vfo == RIG_VFO_CURR
        // that is the "RAM" VFO not backed by memory

        tx_freq = (chan->split == RIG_SPLIT_ON) ? chan->tx_freq : chan->freq;

        ret = prm80_set_rx_tx_freq(rig, chan->freq, tx_freq);

        if (ret != RIG_OK)
        {
            return ret;
        }

        priv->split = chan->split;
        priv->rx_freq = chan->freq;
        priv->tx_freq = tx_freq;

        ret = prm80_set_level(rig, vfo, RIG_LEVEL_SQL, chan->levels[LVL_SQL]);

        if (ret != RIG_OK)
        {
            return ret;
        }

        ret = prm80_set_level(rig, vfo, RIG_LEVEL_AF, chan->levels[LVL_AF]);

        if (ret != RIG_OK)
        {
            return ret;
        }

#if 0
        // Not implemented yet..
        ret = prm80_set_level(rig, vfo, RIG_LEVEL_RFPOWER, chan->levels[LVL_RFPOWER]);

        if (ret != RIG_OK)
        {
            return ret;
        }

#endif

        ret = prm80_set_func(rig, vfo, RIG_FUNC_LOCK, chan->funcs & RIG_FUNC_LOCK);

        if (ret != RIG_OK)
        {
            return ret;
        }
    }

    return RIG_OK;
}


// TODO FUNC_REV ?
int prm80_set_func(RIG *rig, vfo_t vfo, setting_t func, int status)
{
    int ret;

    if (func & RIG_FUNC_LOCK)
    {
        /* Lock keys/TX/Vol */
        ret = prm80_transaction(rig, "K", (status != 0) ? "03" : "00", 1);
    }
    else
    {
        ret = -RIG_EINVAL;
    }

    return ret;
}

int prm80_get_func(RIG *rig, vfo_t vfo, setting_t func, int *status)
{
    int ret;
    channel_t chan;

    memset(&chan, 0, sizeof(chan));
    chan.vfo = RIG_VFO_CURR;

    ret = prm80_get_channel(rig, vfo, &chan, 0);

    if (ret != RIG_OK)
    {
        return ret;
    }

    *status = (chan.funcs & func);

    return RIG_OK;
}

/*
 * prm80_set_level
 * Assumes rig!=NULL
 */
int prm80_set_level(RIG *rig, vfo_t vfo, setting_t level, value_t val)
{
    char buf[BUFSZ];

    // do some clamping, all levels are float values.
    if (val.f < 0.0)
    {
        val.f = 0.0;
    }
    else if (val.f > 1.0)
    {
        val.f = 1.0;
    }

    switch (level)
    {
    case RIG_LEVEL_AF:
        sprintf(buf, "%02u", (unsigned)(val.f * 15));

        return prm80_transaction(rig, "O", buf, 1);

    case RIG_LEVEL_SQL:
        sprintf(buf, "%02u", (unsigned)(val.f * 15));

        return prm80_transaction(rig, "F", buf, 1);

    case RIG_LEVEL_RFPOWER:
        // TODO : modify the Mode byte b1 ?
#if 0
        /* Current mode:
           ; b0: Squelch       b1: power
           ; b2: Squelch open  b3: TX
           ; b4: PLL locked    b5: Long press memorize
           ; b6: Debouncing in effect b7: LCD refresh
           */
        // TODO perform a "Read-Modify-Write" of the mode_byte
        mode_byte  = 0x10;
        mode_byte |= (chan->levels[LVL_RFPOWER].f == 0.) ? 0 : 0x02;
#endif
        return -RIG_ENIMPL;

    default:
        rig_debug(RIG_DEBUG_ERR, "%s: unsupported set_level %s\n", __func__,
                  rig_strlevel(level));
        return -RIG_EINVAL;
    }

    return RIG_OK;
}

/*
 * prm80_get_level
 * Assumes rig!=NULL
 */
int prm80_get_level(RIG *rig, vfo_t vfo, setting_t level, value_t *val)
{
    int ret;
    channel_t chan;

    memset(&chan, 0, sizeof(chan));
    chan.vfo = RIG_VFO_CURR;

    ret = prm80_get_channel(rig, vfo, &chan, 0);

    if (ret != RIG_OK)
    {
        return ret;
    }

    switch (level)
    {
    case RIG_LEVEL_AF:
        val->f = chan.levels[LVL_AF].f;

        break;

    case RIG_LEVEL_SQL:
        val->f = chan.levels[LVL_SQL].f;

        break;

    case RIG_LEVEL_RFPOWER:
        val->f = chan.levels[LVL_RFPOWER].f;

        break;

    default:
        rig_debug(RIG_DEBUG_ERR, "%s: unsupported set_level %s\n", __func__,
                  rig_strlevel(level));
        return -RIG_EINVAL;
    }

    return RIG_OK;
}

// TODO vfo_op : MCL FROM_VFO ..

/*
 * prm80_get_info
 * Assumes rig!=NULL
 */
const char *prm80_get_info(RIG *rig)
{
    static char s_buf[BUFSZ];
    struct rig_state *rs = &rig->state;
    char *p;
    int ret;

    // Get rid of possible prompt sent by the rig
    rig_flush(&rs->rigport);

    /* [V] = Print firmware version. */
    ret = write_block(&rs->rigport, "V", 1);

    if (ret < 0)
    {
        return NULL;
    }

    ret = read_string(&rs->rigport, s_buf, BUFSZ, ">", 1);

    if (ret < 0)
    {
        return NULL;
    }

    p = strchr(s_buf, '\r');

    if (p)
    {
        // chomp
        *p = '\0';
    }

    return s_buf;
}


/*
 * initrigs_prm80 is called by rig_backend_load
 */
DECLARE_INITRIG_BACKEND(prm80)
{
    rig_debug(RIG_DEBUG_VERBOSE, "%s: _init called\n", __func__);

    rig_register(&prm8060_caps);

    return RIG_OK;
}

