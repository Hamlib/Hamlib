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

#include <hamlib/config.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>  /* String function definitions */
#include <ctype.h>

#include "hamlib/rig.h"
#include "serial.h"
#include "cal.h"
#include "register.h"
#include "idx_builtin.h"

#include "prm80.h"


#define LF "\x0a"

#define BUFSZ 64

// Channel number min and max
#define CHAN_MIN 0
#define CHAN_MAX 99

// "E" system state is cached for this time (ms)
#define PRM80_CACHE_TIMEOUT 200

// Length (in bytes) of the response to the "E" command
#define CMD_E_RSP_LEN 22

#define RX_IF_OFFSET MHz(21.4)

// The rig's PLL only deals with freq in Hz divided by this value
#define FREQ_DIV 12500.

/* V5 based on V4 commands
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
              DB   " [E] = Show system state (Mode-Chan-Chanstate-Sql-Vol-Lock-RX freq-TX freq,RSSI).",0Dh,0Ah
              DB   " [F] = Set squelch.",0Dh,0Ah
              DB   " [H] = Print this help page.",0Dh,0Ah
              DB   " [I] = Erase and init RAM and EEPROM.",0Dh,0Ah
              DB   " [K] = Set lock byte.",0Dh,0Ah
              DB   " [L] = Print latch state.",0Dh,0Ah
              DB   " [M] = Edit external RAM manually.",0Dh,0Ah
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
                freq-RSSI).
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
    b7: Force LCD refresh when set. Automatically cleared.

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

static void prm80_force_cache_timeout(RIG *rig)
{
    struct prm80_priv_data *priv = (struct prm80_priv_data *)rig->state.priv;

    rig_force_cache_timeout(&priv->status_tv);
}

/*
 * Read a prompt terminated by delimiter, then write an optional string s.
 */
static int read_prompt_and_send(hamlib_port_t *rigport,
                                char *data, int *data_len, const char *s, const char *delimiter,
                                int space_after_delim)
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

    retval = read_string(rigport, (unsigned char *) data, buflen, delimiter, 1, 0,
                         1);

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
    if (space_after_delim)
    {
        retval = read_block(rigport, (unsigned char *) spacebuf, 1);

        if (retval < 0 && retval != -RIG_ETIMEOUT)
        {
            return retval;
        }
    }

    // Here is the answer to the prompt
    retval = write_block(rigport, (unsigned char *) s, strlen(s));

    return retval;
}

/*
 * Read a prompt terminated by ": ", then write an optional string s.
 */
static int read_colon_prompt_and_send(hamlib_port_t *rigport,
                                      char *data, int *data_len, const char *s)
{
    return read_prompt_and_send(rigport, data, data_len, s, ":", 1);
}

/*
 * Read a prompt terminated by "$" (without space afterwards),
 * then write an optional string s.
 */
static int read_dollar_prompt_and_send(hamlib_port_t *rigport,
                                       char *data, int *data_len, const char *s)
{
    return read_prompt_and_send(rigport, data, data_len, s, "$", 0);
}

/*
 * After each executed command, the rig generally sends "\r\n>"
 */
static int prm80_wait_for_prompt(hamlib_port_t *rigport)
{
    char buf[BUFSZ * 2];
    int retval;

    // Read up to the '>' prompt and discard content.
    retval = read_string(rigport, (unsigned char *) buf, sizeof(buf), ">", 1, 0, 1);

    if (retval < 0)
    {
        return retval;
    }

    return RIG_OK;
}

/*
 *
 * \param cmd is string of generally one letter (or digit)
 * \param arg1 is an optional string to send afterwards
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
    retval = write_block(&rs->rigport, (unsigned char *) cmd, strlen(cmd));

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

    prm80_force_cache_timeout(rig);

    return RIG_OK;
}

/*
 * Convert freq in Hz to the RX PLL value representation with PRM08 firmware
 */
static unsigned rx_freq_to_pll_value(freq_t rx_freq)
{
    // UHF vs VHF
    if (rx_freq > MHz(300))
    {
        return (unsigned)((rx_freq - RX_IF_OFFSET) / FREQ_DIV);
    }
    else
    {
        return (unsigned)((rx_freq + RX_IF_OFFSET) / FREQ_DIV);
    }
}

static freq_t pll_value_to_rx_freq(unsigned pll_value)
{
    freq_t rx_freq;

    rx_freq = (freq_t)pll_value * FREQ_DIV;

    // UHF vs VHF
    if (rx_freq > MHz(300))
    {
        rx_freq += RX_IF_OFFSET;
    }
    else
    {
        rx_freq -= RX_IF_OFFSET;
    }

    return rx_freq;
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
    SNPRINTF(rx_freq_buf, sizeof(rx_freq_buf), "%04X",
             rx_freq_to_pll_value(rx_freq));
    SNPRINTF(tx_freq_buf, sizeof(tx_freq_buf), "%04X",
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

    prm80_force_cache_timeout(rig);

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

    prm80_force_cache_timeout(rig);

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

    SNPRINTF(chbuf, sizeof(chbuf), "%02u", (unsigned)ch);

    prm80_force_cache_timeout(rig);

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
static unsigned hhtoi(const char *p)
{
    char buf[4];

    // it has to be hex digits
    if (!isxdigit(p[0]) || !isxdigit(p[1]))
    {
        rig_debug(RIG_DEBUG_ERR, "%s: unexpected content '%s'\n", __func__, p);
        return 0;
    }

    buf[0] = p[0];
    buf[1] = p[1];
    buf[2] = '\0';

    return (unsigned)strtol(buf, NULL, 16);
}

/**
 * Get system state [E] from rig into \a statebuf
 */
static int prm80_do_read_system_state(hamlib_port_t *rigport, char *statebuf)
{
    char *p;
    int ret;

    // Get rid of possible prompt sent by the rig
    rig_flush(rigport);

    /* [E] = Show system state */
    ret = write_block(rigport, (unsigned char *) "E", 1);

    if (ret < 0)
    {
        return (ret);
    }

    // The response length is fixed
    ret = read_block(rigport, (unsigned char *) statebuf, CMD_E_RSP_LEN);

    if (ret < 0)
    {
        return ret;
    }

    if (ret >= 0)
    {
        statebuf[ret] = '\0';
    }

    if (ret < CMD_E_RSP_LEN)
    {
        rig_debug(RIG_DEBUG_ERR, "%s: len=%d < %d, statebuf='%s'\n", __func__,
                  ret, CMD_E_RSP_LEN, statebuf);
        return (-RIG_EPROTO);
    }

    p = strchr(statebuf, '>');

    if (p)
    {
        int left_to_read = (p - statebuf) + 1;
        memmove(statebuf, p + 1, CMD_E_RSP_LEN - left_to_read);
        ret = read_block(rigport,
                         (unsigned char *) statebuf + CMD_E_RSP_LEN - left_to_read,
                         left_to_read);

        if (ret < 0)
        {
            return ret;
        }
        else
        {
            statebuf[CMD_E_RSP_LEN] = '\0';
        }

        rig_debug(RIG_DEBUG_WARN, "%s: len=%d, statebuf='%s'\n", __func__, ret,
                  statebuf);
    }

    prm80_wait_for_prompt(rigport);

    return RIG_OK;
}

/*
 * Layer to handle the cache to Get system state [E]
 */
static int prm80_read_system_state(RIG *rig, char *statebuf)
{
    struct prm80_priv_data *priv = (struct prm80_priv_data *)rig->state.priv;
    int ret = RIG_OK;

    if (rig_check_cache_timeout(&priv->status_tv, PRM80_CACHE_TIMEOUT))
    {
        ret = prm80_do_read_system_state(&rig->state.rigport, statebuf);

        if (ret == RIG_OK)
        {
            strcpy(priv->cached_statebuf, statebuf);

            /* update cache date */
            gettimeofday(&priv->status_tv, NULL);
        }
    }
    else
    {
        strcpy(statebuf, priv->cached_statebuf);
    }

    return ret;
}

/*
 * prm80_get_channel
 * Assumes rig!=NULL
 */
int prm80_get_channel(RIG *rig, vfo_t vfo, channel_t *chan, int read_only)
{
    struct prm80_priv_data *priv = (struct prm80_priv_data *)rig->state.priv;
    char statebuf[BUFSZ];
    int ret, chanstate, mode_byte, lock_byte;

    if (chan->vfo == RIG_VFO_MEM)
    {
        ret = prm80_set_mem(rig, RIG_VFO_CURR, chan->channel_num);

        if (ret != RIG_OK)
        {
            return ret;
        }
    }

    ret = prm80_read_system_state(rig, statebuf);

    if (ret != RIG_OK)
    {
        return ret;
    }

    /* (Mode-Chan-Chanstate-Sql-Vol-Lock-RX freq-TX freq-RSSI). */
    /* Examples: 1240080AFF0033F02D40__ or 14000C00FD0079708020__ */

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

    // squelch is in low nibble
    chan->levels[LVL_SQL].f = ((float)(hhtoi(statebuf + 6) & 0x0F)) / 15.;
    // volume is hex "00" .. "10"
    chan->levels[LVL_AF].f  = ((float)hhtoi(statebuf + 8)) / 16.;
    chan->levels[LVL_RFPOWER].f  = (mode_byte & 0x02) ? 1.0 : 0.0;

    // new in FW V5
    chan->levels[LVL_RAWSTR].i = hhtoi(statebuf + 20);

    chan->funcs  = 0;
    chan->funcs |= (chanstate & 0x02) ? RIG_FUNC_REV : 0;

    lock_byte = hhtoi(statebuf + 10) & 0x0f;
    chan->funcs |= (lock_byte & 0x05) ? RIG_FUNC_LOCK : 0;
    chan->funcs |= (lock_byte & 0x08) ? RIG_FUNC_MUTE : 0;

    chan->freq = pll_value_to_rx_freq((hhtoi(statebuf + 12) << 8) + hhtoi(
                                          statebuf + 14));
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

          Possibly:
            "This channel number doesn't exist. Add new channel (Y/N) ? "
           */

        SNPRINTF(buf, sizeof(buf), "%02u", (unsigned)chan->channel_num);

        ret = prm80_transaction(rig, "P", buf, 0);

        if (ret != RIG_OK)
        {
            return ret;
        }

        // Set the RX frequency as PLL word.
        SNPRINTF(buf, sizeof(buf), "%04X", rx_freq_to_pll_value(chan->freq));

        // "PLL value to load : $"
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

        SNPRINTF(buf, sizeof(buf), "%02X", chanstate);

        // "Channel state : $"
        ret = read_dollar_prompt_and_send(&rs->rigport, NULL, NULL, buf);

        if (ret != RIG_OK)
        {
            return ret;
        }

        // Determine if prompt came back (CRLF'>') or have to
        // handle the possible query from the rig:
        // "This channel number doesn't exist. Add new channel (Y/N) ? "
        ret = read_block(&rs->rigport, (unsigned char *) buf, 3);

        if (ret < 0)
        {
            RETURNFUNC(ret);
        }

        if (ret == 3 && buf[2] == 'T')
        {
            // Read the question
            ret = read_string(&rs->rigport, (unsigned char *) buf, sizeof(buf), "?", 1, 0,
                              1);

            if (ret < 0)
            {
                RETURNFUNC(ret);
            }

            // Read extra space
            ret = read_block(&rs->rigport, (unsigned char *) buf, 1);

            if (ret < 0)
            {
                RETURNFUNC(ret);
            }

            // Send confirmation
            ret = write_block(&rs->rigport, (unsigned char *) "Y", 1);

            if (ret < 0)
            {
                RETURNFUNC(ret);
            }
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

        ret = prm80_set_func(rig, vfo, RIG_FUNC_LOCK,
                             !!(chan->funcs & RIG_FUNC_LOCK));

        if (ret != RIG_OK)
        {
            return ret;
        }
    }

    prm80_force_cache_timeout(rig);

    return RIG_OK;
}


// TODO FUNC_REV through Channel state byte ?
// TODO "Read-Modify-Write" (or shadowing in priv area) of the lock bits
int prm80_set_func(RIG *rig, vfo_t vfo, setting_t func, int status)
{
    int ret;

    if (func & RIG_FUNC_LOCK)
    {
        /* Lock keys(b0)/Vol(b2) */
        ret = prm80_transaction(rig, "K", (status != 0) ? "05" : "00", 1);
    }
    else if (func & RIG_FUNC_MUTE)
    {
        /* Lock RX(b3) */
        ret = prm80_transaction(rig, "K", (status != 0) ? "08" : "00", 1);
    }
    else
    {
        ret = -RIG_EINVAL;
    }

    prm80_force_cache_timeout(rig);

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

    *status = !!(chan.funcs & func);

    return RIG_OK;
}

/*
 * prm80_set_level
 * Assumes rig!=NULL
 */
int prm80_set_level(RIG *rig, vfo_t vfo, setting_t level, value_t val)
{
    char buf[BUFSZ];
    int ret, mode_byte;

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
        // Unlike system state, volume decimal
        SNPRINTF(buf, sizeof(buf), "%02u", (unsigned)(val.f * 16));

        return prm80_transaction(rig, "O", buf, 1);

    case RIG_LEVEL_SQL:
        SNPRINTF(buf, sizeof(buf), "%02u", (unsigned)(val.f * 15));

        return prm80_transaction(rig, "F", buf, 1);

    case RIG_LEVEL_RFPOWER:
        /* Current mode:
           ; b0: Squelch       b1: power
           ; b2: Squelch open  b3: TX
           ; b4: PLL locked    b5: Long press memorize
           ; b6: Debouncing in effect b7: LCD refresh
           */
        // Perform a "Read-Modify-Write" of the mode_byte
        ret = prm80_read_system_state(rig, buf);

        if (ret != RIG_OK)
        {
            return ret;
        }

        mode_byte  = hhtoi(buf);
        mode_byte &= ~0x02;
        mode_byte |= (val.f == 0.) ? 0 : 0x02;
        SNPRINTF(buf, sizeof(buf), "%02X", (unsigned)mode_byte);

        return prm80_transaction(rig, "D", buf, 1);

    default:
        rig_debug(RIG_DEBUG_ERR, "%s: unsupported set_level %s\n", __func__,
                  rig_strlevel(level));
        return -RIG_EINVAL;
    }

    prm80_force_cache_timeout(rig);

    return RIG_OK;
}

#ifdef V4_ONLY
/*
 * get_level RIG_LEVEL_RAWSTR
 */
static int prm80_get_rawstr_RAM(RIG *rig, value_t *val)
{
    char buf[BUFSZ];
    struct rig_state *rs = &rig->state;
    int ret, i;

    /* [U] = Print 80c552 internal RAM. */

    // Send cmd, Wait for colon prompt, but then send nothing
    ret = prm80_transaction(rig, "U", "", 0);

    if (ret < 0)
    {
        return ret;
    }

    // Read CRLF
    ret = read_string(&rs->rigport, buf, BUFSZ, "\n", 1, 0, 1);

    if (ret < 0)
    {
        return ret;
    }

    // (16 lines of 16 bytes each)

    // According to prm.a51, the rssi_hold variable is in RAM at RAM+35.
    // The RAM base is at 030h.

#define RSSI_HOLD_ADDR (0x30 + 35)  // = 0x53

    for (i = 0; i < (RSSI_HOLD_ADDR / 16) + 1; i++)
    {
        ret = read_string(&rs->rigport, buf, BUFSZ, "\n", 1, 0, 1);

        if (ret < 0)
        {
            return ret;
        }
    }

    // A line looks like this
    // "$50 : 00 01 02 53 04 05 06 07 08 09 0a 0b 0c 0d 0e 0f\r\n"

    val->i = hhtoi(buf + 6 + 3 * (RSSI_HOLD_ADDR % 16));

    // discard the remaining content of RAM print
    for (i = 0; i < (16 - RSSI_HOLD_ADDR / 16) - 1; i++)
    {
        read_string(&rs->rigport, buf, BUFSZ, "\n", 1, 0, 1);
    }

    prm80_wait_for_prompt(&rs->rigport);

    return RIG_OK;
}
#endif

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
    case RIG_LEVEL_RAWSTR:
        val->i = chan.levels[LVL_RAWSTR].i;

        break;

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

int prm80_get_ptt(RIG *rig, vfo_t vfo, ptt_t *ptt)
{
    char statebuf[BUFSZ];
    int ret, mode_byte;

    ret = prm80_read_system_state(rig, statebuf);

    if (ret != RIG_OK)
    {
        return ret;
    }

    mode_byte = hhtoi(statebuf);

    // TX mode on?
    *ptt = (mode_byte & 0x08) ? RIG_PTT_ON : RIG_PTT_OFF;

    return RIG_OK;
}

int prm80_get_dcd(RIG *rig, vfo_t vfo, dcd_t *dcd)
{
    char statebuf[BUFSZ];
    int ret, mode_byte;

    ret = prm80_read_system_state(rig, statebuf);

    if (ret != RIG_OK)
    {
        return ret;
    }

    mode_byte = hhtoi(statebuf);

    // Squelch open?
    *dcd = (mode_byte & 0x04) ? RIG_DCD_ON : RIG_DCD_OFF;

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
    ret = write_block(&rs->rigport, (unsigned char *) "V", 1);

    if (ret < 0)
    {
        return NULL;
    }

    ret = read_string(&rs->rigport, (unsigned char *) s_buf, BUFSZ, ">", 1, 0, 1);

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

