/*
 *  Hamlib PRM80 backend - main file
 *  Copyright (c) 2010 by Stephane Fillod
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

#define PROMPT ">"

#define BUFSZ 64

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
 * prm80_transaction
 * We assume that rig!=NULL, rig->state!= NULL, data!=NULL, data_len!=NULL
 * Otherwise, you'll get a nice seg fault. You've been warned!
 * TODO: error case handling
 */
static int prm80_transaction(RIG *rig, const char *cmd, int cmd_len, char *data,
                             int *data_len)
{
    int retval, i;
    struct rig_state *rs;

    rs = &rig->state;

    serial_flush(&rs->rigport);

    retval = write_block(&rs->rigport, cmd, cmd_len);

    if (retval != RIG_OK)
    {
        return retval;
    }

    /* no data wanted, but flush it anyway */
    if (!data || !data_len)
    {
        char retbuf[BUFSZ + 1];

        retval = read_string(&rs->rigport, retbuf, BUFSZ, LF, strlen(LF));

        if (retval < 0)
        {
            return retval;
        }

#if 0

        /*
         * Does transceiver sends back ">" ?
         */
        if (strstr(retbuf, PROMPT))
        {
            return RIG_OK;
        }
        else
        {
            return -RIG_ERJCTED;
        }

#else
        return RIG_OK;
#endif
    }

    retval = read_string(&rs->rigport, data, BUFSZ, LF, strlen(LF));

    if (retval == -RIG_ETIMEOUT)
    {
        retval = 0;
    }

    if (retval < 0)
    {
        return retval;
    }

    /* Clear possible MSB, because of 7S1 */
    for (i = 0; i < retval; i++)
    {
        data[i] &= 0x7f;
    }

    *data_len = retval;

    /* chomp CR/LF from string */
    if (*data_len >= 2 && data[*data_len - 1] == '\x0a')
    {
        *data_len -= 2;
    }

    data[*data_len] = '\0';

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
     * master reset ?
     */
    retval = prm80_transaction(rig, "0", 1, NULL, NULL);

    if (retval != RIG_OK)
    {
        return retval;
    }

    return RIG_OK;
}


/*
 * prm80_set_freq
 * Assumes rig!=NULL
 */
int prm80_set_freq(RIG *rig, vfo_t vfo, freq_t freq)
{
    char freqbuf[BUFSZ];
    int freq_len;

    /* wild guess */
    freq_len = sprintf(freqbuf, "R%04X%04X",
                       (unsigned)(freq / 12500.),
                       (unsigned)(freq / 12500.));

    return prm80_transaction(rig, freqbuf, freq_len, NULL, NULL);
}

/*
 * prm80_get_freq
 * Assumes rig!=NULL
 */
int prm80_get_freq(RIG *rig, vfo_t vfo, freq_t *freq)
{
    int ret;
    channel_t chan;

    memset(&chan, 0, sizeof(chan));
    chan.vfo = RIG_VFO_CURR;

    ret = prm80_get_channel(rig, &chan, 0);

    if (ret != RIG_OK)
    {
        return ret;
    }

    *freq = chan.freq;

    return RIG_OK;
}

/*
 * prm80_set_mem
 * Assumes rig!=NULL
 */
int prm80_set_mem(RIG *rig, vfo_t vfo, int ch)
{
    int cmd_len;
    char cmdbuf[BUFSZ];

    /* [N] = Set current channel. */

    if (ch < 0 || ch > 99)
    {
        return -RIG_EINVAL;
    }

    cmd_len = sprintf(cmdbuf, "N%02d", ch);

    return prm80_transaction(rig, cmdbuf, cmd_len, NULL, NULL);
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

    ret = prm80_get_channel(rig, &chan, 0);

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
int prm80_get_channel(RIG *rig, channel_t *chan, int read_only)
{
    char statebuf[BUFSZ];
    int statebuf_len = BUFSZ;
    int ret, chanstate;

    if (chan->vfo == RIG_VFO_MEM)
    {
        ret = prm80_set_mem(rig, RIG_VFO_CURR, chan->channel_num);

        if (ret != RIG_OK)
        {
            return ret;
        }
    }

    /* [E] = Show system state (Mode-Chan-Chanstate-Sql-Vol-Lock-RX freq-TX freq). */
    ret = prm80_transaction(rig, "E", 1, statebuf, &statebuf_len);

    if (ret != RIG_OK)
    {
        return ret;
    }

    if (statebuf_len < 20)
    {
        return -RIG_EPROTO;
    }

    /* Example: 1240080AFF0033F02D40 */
    if (hhtoi(statebuf) != 0x12)
        rig_debug(RIG_DEBUG_WARN, "%s: Unknown mode 0x%c%c\n",
                  __func__, statebuf[0], statebuf[1]);

    chan->mode = RIG_MODE_FM;
    chan->width = rig_passband_normal(rig, chan->mode);
    chan->channel_num = hhtoi(statebuf + 2);

    chanstate = hhtoi(statebuf + 4) & 0x0f;
    /* is it rptr_shift or split mode ? */
    chan->rptr_shift = (chanstate & 0x01) == 0 ? RIG_RPT_SHIFT_NONE :
                       (chanstate & 0x02) ? RIG_RPT_SHIFT_MINUS :
                       (chanstate & 0x04) ? RIG_RPT_SHIFT_PLUS : RIG_RPT_SHIFT_NONE;
    chan->flags = (chanstate & 0x08) ? RIG_CHFLAG_SKIP : 0;

    chan->levels[LVL_SQL].f = ((float)(hhtoi(statebuf + 6) >> 4)) / 15.;
    chan->levels[LVL_AF].f  = ((float)(hhtoi(statebuf + 8) >> 4)) / 15.;
    /* same as chanstate bit 1 */
    chan->flags = hhtoi(statebuf + 10) == 0 ? 0 : RIG_CHFLAG_SKIP;
    chan->freq = ((hhtoi(statebuf + 12) << 8) + hhtoi(statebuf + 14)) * 12500;
    chan->tx_freq = ((hhtoi(statebuf + 16) << 8) + hhtoi(statebuf + 18)) * 12500;
    chan->rptr_offs = chan->tx_freq - chan->freq;

    if (!read_only)
    {
        // Set rig to channel values
        rig_debug(RIG_DEBUG_ERR,
                  "%s: please contact hamlib mailing list to implement this\n", __func__);
        rig_debug(RIG_DEBUG_ERR,
                  "%s: need to know if rig updates when channel read or not\n", __func__);
        return -RIG_ENIMPL;
    }

    return RIG_OK;
}

/*
 * prm80_set_channel
 * Assumes rig!=NULL
 */
int prm80_set_channel(RIG *rig, const channel_t *chan)
{
    char statebuf[BUFSZ];
    int statebuf_len = BUFSZ;
    int ret;

    if (chan->vfo == RIG_VFO_MEM)
    {
        ret = prm80_set_mem(rig, RIG_VFO_CURR, chan->channel_num);

        if (ret != RIG_OK)
        {
            return ret;
        }
    }

    /* [T] = Set current channel state. (Mode-Chan-Chanstate-Sql-Vol-Lock-RX freq-TX freq) ? */
    /* Example: 1240080AFF0033F02D40 ? */
    statebuf_len = sprintf(statebuf, "T%02X%02X%02X%02X%02X%02X%04X%04X",
                           0x12,
                           chan->channel_num,
                           (chan->flags & RIG_CHFLAG_SKIP) ? 0x08 : 0, /* TODO: tx shift */
                           (unsigned)(chan->levels[LVL_SQL].f * 15),
                           (unsigned)(chan->levels[LVL_AF].f * 15),
                           (chan->flags & RIG_CHFLAG_SKIP) ? 0x01 : 0x00, /* Lock */
                           (unsigned)(chan->freq / 12500.),
                           (unsigned)(chan->tx_freq / 12500.)
                          );

    ret = prm80_transaction(rig, statebuf, statebuf_len, NULL, NULL);

    if (ret != RIG_OK)
    {
        return ret;
    }

    return RIG_OK;
}


/*
 * prm80_set_level
 * Assumes rig!=NULL
 */
int prm80_set_level(RIG *rig, vfo_t vfo, setting_t level, value_t val)
{
    int cmd_len;
    char cmdbuf[BUFSZ];

    switch (level)
    {
    case RIG_LEVEL_AF:
        cmd_len = sprintf(cmdbuf, "O%02u", (unsigned)(val.f * 15));

        return prm80_transaction(rig, cmdbuf, cmd_len, NULL, NULL);

    case RIG_LEVEL_SQL:
        cmd_len = sprintf(cmdbuf, "F%02u", (unsigned)(val.f * 15));

        return prm80_transaction(rig, cmdbuf, cmd_len, NULL, NULL);

    case RIG_LEVEL_RFPOWER:
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

    ret = prm80_get_channel(rig, &chan, 1);

    if (ret != RIG_OK)
    {
        return ret;
    }

    switch (level)
    {
    case RIG_LEVEL_AF:
        // cppcheck-suppress *
        val->f = chan.levels[LVL_AF].f;

        break;

    case RIG_LEVEL_SQL:
        // cppcheck-suppress *
        val->f = chan.levels[LVL_SQL].f;

        break;

    default:
        rig_debug(RIG_DEBUG_ERR, "%s: unsupported set_level %s\n", __func__,
                  rig_strlevel(level));
        return -RIG_EINVAL;
    }

    return RIG_OK;
}

/*
 * prm80_get_info
 * Assumes rig!=NULL
 */
const char *prm80_get_info(RIG *rig)
{
    static char buf[BUFSZ];
    int ret, buf_len = BUFSZ;

    /* [V] = Print firmware version. */
    ret = prm80_transaction(rig, "V", 1, buf, &buf_len);

    if (ret < 0)
    {
        return NULL;
    }

    return buf;
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

