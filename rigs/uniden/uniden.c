/*
 *  Hamlib Uniden backend - main file
 *  Copyright (c) 2001-2008 by Stephane Fillod
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
#include <string.h>  /* String function definitions */
#include <unistd.h>  /* UNIX standard function definitions */

#include "hamlib/rig.h"
#include "serial.h"
#include "misc.h"
#include "register.h"
#include "idx_builtin.h"

#include "uniden.h"


static const struct { rig_model_t model; const char *id; }
uniden_id_string_list[] =
{
    { RIG_MODEL_BC780,  "BC780" },
    { RIG_MODEL_BC245,  "BC245XLT" },
    { RIG_MODEL_BC250,  "BC250D" },
    { RIG_MODEL_BC895,  "BC895" },
    { RIG_MODEL_BC235,  "BC235XLT" },
    { RIG_MODEL_BC785,  "BC785" },
    { RIG_MODEL_BC786,  "BC786D" },
    { RIG_MODEL_PRO2052, "PRO2052" }, /* ?? */
    { RIG_MODEL_BCT8,   "BCT8" },    /* ?? */
    { RIG_MODEL_BC898,  "BC898T" },  /* TBC */
    { RIG_MODEL_NONE, NULL },   /* end marker */
};


tone_t uniden_ctcss_list[] =
{
    670,  719,  744,  770,  797,  825,  854,  885,  915,
    948, 974, 1000, 1035, 1072, 1109, 1148, 1188, 1230,
    1273, 1318, 1365, 1413, 1462, 1514, 1567, 1622, 1679,
    1738, 1799, 1862, 1928, 2035, 2107, 2181, 2257, 2336,
    2418, 2503, 0
};

tone_t uniden_dcs_list[] =
{
    23,  25,  26,  31,  32,  36,  43,  47,  51,  53,
    54,  65,  71,  72,  73,  74, 114, 115, 116, 122,
    125, 131, 132, 134, 143, 145, 152, 155, 156, 162,
    165, 172, 174, 205, 212, 223, 225, 226, 243, 244,
    245, 246, 251, 252, 255, 261, 263, 265, 266, 271,
    274, 306, 311, 315, 325, 331, 332, 343, 346, 351,
    356, 364, 365, 371, 411, 412, 413, 423, 431, 432,
    445, 446, 452, 454, 455, 462, 464, 465, 466, 503,
    506, 516, 523, 526, 532, 546, 565, 606, 612, 624,
    627, 631, 632, 654, 662, 664, 703, 712, 723, 731,
    732, 734, 743, 754, 0
};

/*
 * Uniden backend: should work for:
 * BC235XLT, BC895XLT, BC245XLT, BC780XLT, BC250D,  BC785D and BCT8
 * and most probably for the RadioShack PRO-2052.
 *
 * Protocol information available at http://www.cantonmaine.com/pro2052.htm
 * and http://www.freqofnature.com/software/protocols.html
 *
 * It seems like these rigs have no VFO, I mean only mem channels.
 * Is that correct? --SF
 */

#define EOM "\r"

#define BUFSZ 64

/**
 * uniden_transaction
 * Assumes rig!=NULL rig->state!=NULL rig->caps!=NULL
 *
 * cmdstr - Command to be sent to the rig. Cmdstr can also be NULL, indicating
 *          that only a reply is needed (nothing will be send).
 * replystr - Reply prefix to be expected. Replystr can also be NULL, indicating
 *          that the prefix is either the cmdstr prefix or OK.
 * data - Buffer for reply string.  Can be NULL, indicating that no reply is
 *        is needed and will return with RIG_OK after command was sent.
 * datasize - in: Size of buffer. It is the caller's responsibily to provide
 *            a large enough buffer for all possible replies for a command.
 *            out: location where to store number of bytes read.
 *
 * returns:
 *   RIG_OK  -  if no error occurred.
 *   RIG_EIO  -  if an I/O error occurred while sending/receiving data.
 *   RIG_ETIMEOUT  -  if timeout expires without any characters received.
 *   RIG_REJECTED  -  if a negative acknowledge was received or command not
 *                    recognized by rig.
 */
int
uniden_transaction(RIG *rig, const char *cmdstr, int cmd_len,
                   const char *replystr,
                   char *data, size_t *datasize)
{
    struct rig_state *rs;
    int retval;
    int retry_read = 0;
    char replybuf[BUFSZ];
    size_t reply_len = BUFSZ;

    rs = &rig->state;
    rs->transaction_active = 1;

transaction_write:

    rig_flush(&rs->rigport);

    if (cmdstr)
    {
        retval = write_block(&rs->rigport, (unsigned char *) cmdstr, strlen(cmdstr));

        if (retval != RIG_OK)
        {
            goto transaction_quit;
        }
    }

    /* Always read the reply to known if it went OK */
    if (!data)
    {
        data = replybuf;
    }

    if (!datasize)
    {
        datasize = &reply_len;
    }

    memset(data, 0, *datasize);
    retval = read_string(&rs->rigport, (unsigned char *) data, *datasize, EOM,
                         strlen(EOM), 0, 1);

    if (retval < 0)
    {
        if (retry_read++ < rig->state.rigport.retry)
        {
            goto transaction_write;
        }

        goto transaction_quit;
    }
    else
    {
        *datasize = retval;
    }

    /* Check that command termination is correct */
    if (strchr(EOM, data[strlen(data) - 1]) == NULL)
    {
        rig_debug(RIG_DEBUG_ERR, "%s: Command is not correctly terminated '%s'\n",
                  __func__, data);

        if (retry_read++ < rig->state.rigport.retry)
        {
            goto transaction_write;
        }

        retval = -RIG_EPROTO;
        goto transaction_quit;
    }

    if (strcmp(data, "OK"EOM) == 0)
    {
        /* everything is fine */
        retval = RIG_OK;
        goto transaction_quit;
    }

    /*  Any syntax returning NG indicates a VALID Command but not entered
     *  in the right mode or using the correct parameters. ERR indicates
     *  an INVALID Command.
     */
    if (strcmp(data, "NG"EOM) == 0 || strcmp(data, "ORER"EOM) == 0)
    {
        /* Invalid command */
        rig_debug(RIG_DEBUG_VERBOSE, "%s: NG/Overflow for '%s'\n", __func__, cmdstr);
        retval = -RIG_EPROTO;
        goto transaction_quit;
    }

    if (strcmp(data, "ERR"EOM) == 0)
    {
        /*  Command format error */
        rig_debug(RIG_DEBUG_VERBOSE, "%s: Error for '%s'\n", __func__, cmdstr);
        retval = -RIG_EINVAL;
        goto transaction_quit;
    }

#define CONFIG_STRIP_CMDTRM 1
#ifdef CONFIG_STRIP_CMDTRM

    if (strlen(data) > 0)
    {
        data[strlen(data) - 1] = '\0';    /* not very elegant, but should work. */
    }
    else
    {
        data[0] = '\0';
    }

#endif

    /* Special case for SQuelch */
    if (replystr && !memcmp(cmdstr, "SQ", 2) && (data[0] == '-'
            || data[0] == '+'))
    {
        retval = RIG_OK;
        goto transaction_quit;
    }

    /* Command prefix if no replystr supplied */
    if (!replystr)
    {
        replystr = cmdstr;
    }

    /*
     * Check that received the correct reply. The first two characters
     * should be the same as command.
     */
    if (replystr && replystr[0] && (data[0] != replystr[0] ||
                                    (replystr[1] && data[1] != replystr[1])))
    {
        /*
         * TODO: When RIG_TRN is enabled, we can pass the string
         *       to the decoder for callback. That way we don't ignore
         *       any commands.
         */
        rig_debug(RIG_DEBUG_ERR, "%s: Unexpected reply '%s'\n", __func__, data);

        if (retry_read++ < rig->state.rigport.retry)
        {
            goto transaction_write;
        }

        retval =  -RIG_EPROTO;
        goto transaction_quit;
    }

    retval = RIG_OK;
transaction_quit:
    rs->transaction_active = 0;
    return retval;
}

/*
 * uniden_set_freq
 * Assumes rig!=NULL
 */
int uniden_set_freq(RIG *rig, vfo_t vfo, freq_t freq)
{
    char freqbuf[BUFSZ];

    /* freq in hundreds of Hz */
    freq /= 100;

    /* exactly 8 digits */
    SNPRINTF(freqbuf, sizeof(freqbuf), "RF%08u" EOM, (unsigned)freq);

    return uniden_transaction(rig, freqbuf, strlen(freqbuf), NULL, NULL, NULL);
}

/*
 * uniden_get_freq
 * Assumes rig!=NULL
 */
int uniden_get_freq(RIG *rig, vfo_t vfo, freq_t *freq)
{
    char freqbuf[BUFSZ];
    size_t freq_len = BUFSZ;
    int ret;

    ret = uniden_transaction(rig, "RF" EOM, 3, NULL, freqbuf, &freq_len);

    if (ret != RIG_OK)
    {
        return ret;
    }

    if (freq_len < 10)
    {
        return -RIG_EPROTO;
    }

    sscanf(freqbuf + 2, "%"SCNfreq, freq);
    /* returned freq in hundreds of Hz */
    *freq *= 100;

    return RIG_OK;
}

/*
 * uniden_get_freq
 * Assumes rig!=NULL
 */
int uniden_get_freq_2(RIG *rig, vfo_t vfo, freq_t *freq)
{
    char freqbuf[BUFSZ];
    size_t freq_len = BUFSZ;
    int ret;

    ret = uniden_transaction(rig, "SG" EOM, 3, "S", freqbuf, &freq_len);

    if (ret != RIG_OK)
    {
        return ret;
    }

    if (freq_len < 10)
    {
        return -RIG_EPROTO;
    }

    sscanf(freqbuf + 6, "%"SCNfreq, freq);
    /* returned freq in hundreds of Hz */
    *freq *= 100;

    return RIG_OK;
}

int uniden_set_mode(RIG *rig, vfo_t vfo, rmode_t mode, pbwidth_t width)
{
    const char *modebuf;

    switch (mode)
    {
    case RIG_MODE_AM:
        modebuf = "RM AM"EOM;
        break;

    case RIG_MODE_FM:
        if (width > 0 && width < rig_passband_normal(rig, mode))
        {
            modebuf = "RM NFM"EOM;
        }
        else
        {
            modebuf = "RM FM"EOM;
        }

        break;

    case RIG_MODE_WFM:
        modebuf = "RM WFM"EOM;
        break;

    default:
        return -RIG_EINVAL;
    }

    return uniden_transaction(rig, modebuf, strlen(modebuf), NULL, NULL, NULL);
}

int uniden_get_mode(RIG *rig, vfo_t vfo, rmode_t *mode, pbwidth_t *width)
{
    char modebuf[BUFSZ];
    size_t mode_len = BUFSZ;
    int ret;

    ret = uniden_transaction(rig, "RM" EOM, 3, NULL, modebuf, &mode_len);

    if (ret != RIG_OK)
    {
        return ret;
    }

    if (mode_len < 4)
    {
        return -RIG_EPROTO;
    }

    *width = 0;

    if (!strcmp(modebuf + 3, "AM"))
    {
        *mode = RIG_MODE_AM;
    }
    else if (!strcmp(modebuf + 3, "WFM"))
    {
        *mode = RIG_MODE_WFM;
    }
    else if (!strcmp(modebuf + 3, "FM"))
    {
        *mode = RIG_MODE_FM;
    }
    else if (!strcmp(modebuf + 3, "NFM"))
    {
        *mode = RIG_MODE_FM;
        *width = rig_passband_narrow(rig, RIG_MODE_FM);
    }

    if (*width == 0)
    {
        *width = rig_passband_normal(rig, *mode);
    }

    return RIG_OK;
}

int uniden_set_level(RIG *rig, vfo_t vfo, setting_t level, value_t val)
{
    char levelbuf[16];
    int retval;

    switch (level)
    {
    case RIG_LEVEL_ATT:
        if (rig->state.attenuator[0] == 0)
        {
            return -RIG_EINVAL;
        }

        SNPRINTF(levelbuf, sizeof(levelbuf), "AT%c"EOM,
                 val.i != 0 ? 'N' : 'F');
        break;

    default:
        rig_debug(RIG_DEBUG_ERR, "%s: unsupported set_level %s", __func__,
                  rig_strlevel(level));
        return -RIG_EINVAL;
    }

    retval = uniden_transaction(rig, levelbuf, strlen(levelbuf), NULL, NULL, NULL);

    if (retval != RIG_OK)
    {
        return retval;
    }

    return RIG_OK;
}

/*
 * uniden_get_level
 * Assumes rig!=NULL, val!=NULL
 */
int uniden_get_level(RIG *rig, vfo_t vfo, setting_t level, value_t *val)
{
    char lvlbuf[BUFSZ];
    int retval;
    size_t lvl_len = BUFSZ;

    switch (level)
    {
    case RIG_LEVEL_RAWSTR:
        retval = uniden_transaction(rig, "SG"EOM, 3, "S", lvlbuf, &lvl_len);

        if (retval != RIG_OK)
        {
            return retval;
        }

        if (lvl_len < 4)
        {
            rig_debug(RIG_DEBUG_ERR, "%s: wrong answer len=%d\n",
                      __func__, (int)lvl_len);
            return -RIG_ERJCTED;
        }

        /* S182 F08594375 */
        sscanf(lvlbuf + 1, "%d", &val->i);  /* rawstr */
        break;

    case RIG_LEVEL_ATT:
        retval = uniden_transaction(rig, "AT"EOM, 3, NULL, lvlbuf, &lvl_len);

        if (retval != RIG_OK)
        {
            return retval;
        }

        if (lvl_len < 3)
        {
            rig_debug(RIG_DEBUG_ERR, "%s: unexpected answer len=%d\n",
                      __func__, (int)lvl_len);
            return -RIG_ERJCTED;
        }

        val->i = lvlbuf[2] == 'N' ? rig->state.attenuator[0] : 0;
        break;

    default:
        rig_debug(RIG_DEBUG_ERR, "%s: unsupported get_level %s", __func__,
                  rig_strlevel(level));
        return -RIG_EINVAL;
    }

    return RIG_OK;
}

/*
 * uniden_get_dcd
 * Assumes rig!=NULL
 */
int uniden_get_dcd(RIG *rig, vfo_t vfo, dcd_t *dcd)
{
    char dcdbuf[BUFSZ];
    size_t dcd_len = BUFSZ;
    int ret;

    ret = uniden_transaction(rig, "SQ" EOM, 3, NULL, dcdbuf, &dcd_len);

    if (ret != RIG_OK)
    {
        return ret;
    }

    if (dcd_len < 1 || (dcdbuf[0] != '+' && dcdbuf[0] != '-'))
    {
        return -RIG_EPROTO;
    }

    *dcd = (dcdbuf[0] == '-') ? RIG_DCD_OFF : RIG_DCD_ON;

    return RIG_OK;
}

/*
 * uniden_set_mem
 * Assumes rig!=NULL
 */
int uniden_set_mem(RIG *rig, vfo_t vfo, int ch)
{
    char cmdbuf[BUFSZ];

    SNPRINTF(cmdbuf, sizeof(cmdbuf), "MA%03d" EOM, ch);

    return uniden_transaction(rig, cmdbuf, strlen(cmdbuf), NULL, NULL, NULL);
}

/*
 * uniden_get_mem
 * Assumes rig!=NULL
 */
int uniden_get_mem(RIG *rig, vfo_t vfo, int *ch)
{
    char membuf[BUFSZ];
    size_t mem_len = BUFSZ;
    int ret;

    ret = uniden_transaction(rig, "MA" EOM, 3, "C", membuf, &mem_len);

    if (ret != RIG_OK)
    {
        return ret;
    }

    if (mem_len < 4)
    {
        return -RIG_EPROTO;
    }

    sscanf(membuf + 1, "%d", ch);

    return RIG_OK;
}

/*
 * uniden_get_channel
 * Assumes rig!=NULL
 */
int uniden_get_channel(RIG *rig, vfo_t vfo, channel_t *chan, int read_only)
{
    char cmdbuf[BUFSZ], membuf[BUFSZ];
    size_t mem_len = BUFSZ;
    int ret;
    int tone;

    if (chan->vfo == RIG_VFO_MEM)
    {
        SNPRINTF(cmdbuf, sizeof(cmdbuf), "PM%03d" EOM, chan->channel_num);
    }
    else
    {
        SNPRINTF(cmdbuf, sizeof(cmdbuf), "MA" EOM);
    }

    ret = uniden_transaction(rig, cmdbuf, strlen(cmdbuf), "C", membuf, &mem_len);

    if (ret != RIG_OK)
    {
        return ret;
    }

    /*
     * 0123456789012345678901234567890123456789
     * C089 F08511625 TN DN LF AF RF N000
     */
    if (mem_len < 30 ||
            membuf[5] != 'F' ||
            membuf[15] != 'T' ||
            membuf[18] != 'D' ||
            membuf[21] != 'L' ||
            membuf[24] != 'A' ||
            membuf[27] != 'R' ||
            membuf[30] != 'N')
    {
        return -RIG_EPROTO;
    }

    sscanf(membuf + 1, "%d", &chan->channel_num);
    sscanf(membuf + 6, "%"SCNfreq, &chan->freq);
    /* returned freq in hundreds of Hz */
    chan->freq *= 100;

    /* TODO: Trunk, Delay, Recording */

    chan->flags = (membuf[22] == 'N') ? RIG_CHFLAG_SKIP : 0;
    // cppcheck-suppress *
    chan->levels[LVL_ATT].i = (membuf[25] == 'N') ? rig->state.attenuator[0] : 0;
    sscanf(membuf + 41, "%d", &tone);

    if (tone >= 1 && tone <= 38)
    {
        chan->ctcss_sql = rig->caps->ctcss_list[tone - 1];    /* 1..38 */
    }
    else if (tone > 38)
    {
        chan->dcs_sql = rig->caps->dcs_list[tone - 39];    /* 39..142 */
    }

    if (chan->vfo == RIG_VFO_MEM && rig->caps->chan_desc_sz != 0)
    {
        /* only BC780 BC250 BC785 */
        SNPRINTF(cmdbuf, sizeof(cmdbuf), "TA C %03d" EOM, chan->channel_num);

        ret = uniden_transaction(rig, cmdbuf, strlen(cmdbuf), NULL, membuf, &mem_len);

        if (ret != RIG_OK)
        {
            return ret;
        }

        if (mem_len < 10 || memcmp(membuf, cmdbuf, 8))
        {
            return -RIG_EPROTO;
        }

        /*  TA C 001 My Alpha Tag */
        strncpy(chan->channel_desc, membuf + 9, rig->caps->chan_desc_sz);
    }

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
 * uniden_set_channel
 *
 * Only freq can be set?
 */
int uniden_set_channel(RIG *rig, vfo_t vfo, const channel_t *chan)
{
    char cmdbuf[BUFSZ], membuf[BUFSZ];
    size_t mem_len = BUFSZ;
    int ret;
#if 0 // deprecated
    int trunked = 0;
#endif

    if (chan->vfo != RIG_VFO_MEM)
    {
        return -RIG_EINVAL;
    }

    /* PM089T08511625 */
    SNPRINTF(cmdbuf, sizeof(cmdbuf), "PM%03d%c%08u" EOM, chan->channel_num,
#if 0
             trunked ? 'T' : ' ',
#else
             ' ',
#endif
             (unsigned)(chan->freq / 100));

    ret = uniden_transaction(rig, cmdbuf, strlen(cmdbuf), NULL, membuf, &mem_len);

    if (ret != RIG_OK)
    {
        return ret;
    }

    if (rig->caps->chan_desc_sz != 0)
    {
        /* only BC780 BC250 BC785 */
        SNPRINTF(cmdbuf, sizeof(cmdbuf), "TA C %03d %s" EOM,
                 chan->channel_num, chan->channel_desc);

        ret = uniden_transaction(rig, cmdbuf, strlen(cmdbuf), NULL, NULL, NULL);

        if (ret != RIG_OK)
        {
            return ret;
        }
    }

    return RIG_OK;
}


/*
 * uniden_get_info
 * Assumes rig!=NULL
 */
const char *uniden_get_info(RIG *rig)
{
    static char infobuf[BUFSZ];
    size_t info_len = BUFSZ / 2, vrinfo_len = BUFSZ / 2;
    int ret;

    ret = uniden_transaction(rig, "SI" EOM, 3, NULL, infobuf, &info_len);

    if (ret != RIG_OK)
    {
        return NULL;
    }

    /* SI BC250D,0000000000,104  */

    if (info_len < 4)
    {
        return NULL;
    }

    if (info_len >= BUFSZ)
    {
        info_len = BUFSZ - 1;
    }

    infobuf[info_len] = '\0';

    /* VR not on every rig */
    /* VR1.00 */
    ret = uniden_transaction(rig, "VR" EOM, 3, NULL, infobuf + info_len,
                             &vrinfo_len);

    if (ret == RIG_OK)
    {
        /* overwrite "VR" */
        /* FIXME: need to filter \r or it screws w/ stdout */
        infobuf[info_len] = '\n';
        infobuf[info_len + 1] = ' ';
    }
    else
    {
        infobuf[info_len] = '\0';
    }

    /* skip "SI " */
    return infobuf + 3;
}


#define IDBUFSZ 32

/*
 * proberigs_uniden
 *
 * Notes:
 * There's only one rig possible per port.
 *
 * rig_model_t probeallrigs_uniden(port_t *port, rig_probe_func_t cfunc, rig_ptr_t data)
 */
DECLARE_PROBERIG_BACKEND(uniden)
{
    char idbuf[IDBUFSZ];
    int id_len = -1, i;
    int retval = -1;
    int rates[] = { 9600, 19200, 0 };   /* possible baud rates */
    int rates_idx;

    if (!port)
    {
        return RIG_MODEL_NONE;
    }

    if (port->type.rig != RIG_PORT_SERIAL)
    {
        return RIG_MODEL_NONE;
    }

    port->write_delay = port->post_write_delay = 0;
    port->parm.serial.stop_bits = 1;
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

        retval = write_block(port, (unsigned char *) "SI"EOM, 3);
        id_len = read_string(port, (unsigned char *) idbuf, IDBUFSZ, EOM, 1, 0, 1);
        close(port->fd);

        if (retval != RIG_OK || id_len < 0)
        {
            continue;
        }
    }

    if (retval != RIG_OK || id_len < 0 || memcmp(idbuf, "SI ", 3))
    {
        return RIG_MODEL_NONE;
    }

    /*
     * reply should be something like 'SI xxx,xx,xx\x0d'
     */
    if (id_len < 4)
    {
        idbuf[id_len] = '\0';
        rig_debug(RIG_DEBUG_VERBOSE, "probe_uniden: protocol error,"
                  "  received %d: '%s'\n",
                  id_len, idbuf);
        return RIG_MODEL_NONE;
    }


    /* search ID string */
    for (i = 0; uniden_id_string_list[i].model != RIG_MODEL_NONE; i++)
    {
        if (!memcmp(uniden_id_string_list[i].id, idbuf + 3,
                    strlen(uniden_id_string_list[i].id)))
        {
            rig_debug(RIG_DEBUG_VERBOSE, "probe_uniden: "
                      "found '%s'\n", idbuf + 3);

            if (cfunc)
            {
                (*cfunc)(port, uniden_id_string_list[i].model, data);
            }

            return uniden_id_string_list[i].model;
        }
    }

    /*
     * not found in known table....
     * update uniden_id_list[]!
     */
    rig_debug(RIG_DEBUG_WARN, "probe_uniden: found unknown device "
              "with ID '%s', please report to Hamlib "
              "developers.\n", idbuf + 3);

    return RIG_MODEL_NONE;
}

/*
 * initrigs_uniden is called by rig_backend_load
 */
DECLARE_INITRIG_BACKEND(uniden)
{
    rig_debug(RIG_DEBUG_VERBOSE, "%s: _init called\n", __func__);

    rig_register(&bc895_caps);
    rig_register(&bc898_caps);
    rig_register(&bc245_caps);
    rig_register(&bc780_caps);
    rig_register(&bc250_caps);

    rig_register(&pro2052_caps);

    rig_register(&bcd396t_caps);
    rig_register(&bcd996t_caps);

    return RIG_OK;
}



