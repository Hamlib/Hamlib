/*
 *  Hamlib Alinco backend - main file
 *  Copyright (c) 2001-2010 by Stephane Fillod
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
#  include "config.h"
#endif

#include <stdio.h>
#include <stdlib.h>
#include <string.h>  /* String function definitions */
#include <unistd.h>  /* UNIX standard function definitions */
#include <math.h>

#include <hamlib/rig.h>
#include <serial.h>
#include <misc.h>
#include <cal.h>
#include <register.h>

#include "alinco.h"


/* Line Feed */
#define EOM "\x0d"
#define LF "\x0a"

#define BUFSZ 32

/*
 * modes in use by the "2G" command
 */
#define MD_LSB  '0'
#define MD_USB  '1'
#define MD_CWL  '2'
#define MD_CWU  '3'
#define MD_AM   '4'
#define MD_FM   '5'

#define AL "AL"
#define CMD_TXFREQ  "0A"    /* Transmit frequency */
#define CMD_RXFREQ  "0B"    /* Receive frequency */
#define CMD_VFO     "1A"
#define CMD_MEMMD   "1B"    /* Memory mode */
#define CMD_CHAN    "1D"    /* Channel Display */
#define CMD_UPDWN   "2A"    /* UP/DOWN */
#define CMD_MON     "2B"    /* Check Transmit Frequency */
#define CMD_PWR     "2C"    /* Transmit Output Power */
#define CMD_SCAN    "2D"    /* Scanning */
#define CMD_PRIO    "2E"    /* Priority */
#define CMD_SPLT    "2F"    /* Split */
#define CMD_MODE    "2G"    /* Mode */
#define CMD_RFGAIN  "2H"    /* RF Gain */
#define CMD_AGC     "2I"
#define CMD_FLTER   "2J"    /* Filter */
#define CMD_NB      "2K"
#define CMD_CTCSS   "2L"
#define CMD_TUNE    "2M"
#define CMD_SELECT  "2N"
#define CMD_MCALL   "2V"    /* Memory Channel Call Up */
#define CMD_SDATA   "2W"    /* Set Data */

/* Data Output Commands */
#define CMD_SMETER  "3A"    /* S-meter read */
#define CMD_PTT     "3B"    /* PTT status read */
#define CMD_SQL     "3C"    /* Squelch status */
#define CMD_RIT     "3D"    /* RIT status */
#define CMD_RMEM    "3E"    /* Current Memory-channel Number read */
#define CMD_RMV     "3G"    /* Memory/VFO -mode read */
#define CMD_RDATA   "3H"    /* Current Data read */
#define CMD_RSPLT   "3I"    /* Split read */
#define CMD_RPOWER  "3J"    /* Transmitter Output read */
#define CMD_RSELECT "3K"    /* SELECT Postion read */


/*
 * alinco_transaction
 * We assume that rig!=NULL, rig->state!= NULL, data!=NULL, data_len!=NULL
 * Otherwise, you'll get a nice seg fault. You've been warned!
 * TODO: error case handling
 */
int alinco_transaction(RIG *rig,
                       const char *cmd,
                       int cmd_len,
                       char *data,
                       int *data_len)
{
    int retval;
    struct rig_state *rs;
    char echobuf[BUFSZ + 1];

    rs = &rig->state;

    serial_flush(&rs->rigport);

    retval = write_block(&rs->rigport, cmd, cmd_len);

    if (retval != RIG_OK)
    {
        return retval;
    }

    /*
     * Transceiver sends an echo of cmd followed by a CR/LF
     * TODO: check whether cmd and echobuf match (optional)
     */
    retval = read_string(&rs->rigport, echobuf, BUFSZ, LF, strlen(LF));

    if (retval < 0)
    {
        return retval;
    }

    /* no data expected, check for OK returned */
    if (!data || !data_len)
    {
        retval = read_string(&rs->rigport, echobuf, BUFSZ, LF, strlen(LF));

        if (retval < 0)
        {
            return retval;
        }

        retval -= 2;
        echobuf[retval] = 0;

        if (strcmp(echobuf, "OK") == 0)
        {
            return RIG_OK;
        }
        else
        {
            return -RIG_ERJCTED;
        }
    }

    retval = read_string(&rs->rigport, data, BUFSZ, LF, strlen(LF));

    if (retval < 0)
    {
        return retval;
    }

    *data_len = retval;

    /* strip CR/LF from string
     */
    *data_len -= 2;
    data[*data_len] = 0;
    return RIG_OK;
}


/*
 * alinco_set_vfo
 * Assumes rig!=NULL
 */
int alinco_set_vfo(RIG *rig, vfo_t vfo)
{
    char cmdbuf[BUFSZ];
    int cmd_len;
    char vfo_num;

    switch (vfo)
    {
    case RIG_VFO_A: vfo_num = '1'; break;
    case RIG_VFO_B: vfo_num = '2'; break;
    case RIG_VFO_MEM:
        return alinco_transaction(rig,
                                  AL CMD_MEMMD "0" EOM,
                                  strlen(AL CMD_MEMMD "0" EOM),
                                  NULL,
                                  NULL);

    default:
        rig_debug(RIG_DEBUG_ERR,
                  "alinco_set_vfo: unsupported VFO %d\n",
                  vfo);

        return -RIG_EINVAL;
    }

    cmd_len = sprintf(cmdbuf, AL CMD_VFO "%c" EOM, vfo_num);

    return alinco_transaction(rig, cmdbuf, cmd_len, NULL, NULL);
}


/*
 * alinco_get_vfo
 * Assumes rig!=NULL, !vfo
 */
int alinco_get_vfo(RIG *rig, vfo_t *vfo)
{
    char vfobuf[BUFSZ];
    int vfo_len, retval;

    retval = alinco_transaction(rig,
                                AL CMD_RMV EOM,
                                strlen(AL CMD_RMV EOM),
                                vfobuf,
                                &vfo_len);

    if (retval != RIG_OK)
    {
        return retval;
    }

    if (vfo_len != 4)
    {
        rig_debug(RIG_DEBUG_ERR,
                  "alinco_get_vfo: wrong answer %s, "
                  "len=%d\n",
                  vfobuf,
                  vfo_len);

        return -RIG_ERJCTED;
    }

    vfobuf[vfo_len] = '\0';

    if (!strcmp(vfobuf, "VFOA"))
    {
        *vfo = RIG_VFO_A;
    }
    else if (!strcmp(vfobuf, "VFOB"))
    {
        *vfo = RIG_VFO_B;
    }
    else if (!strcmp(vfobuf, "MEMO"))
    {
        *vfo = RIG_VFO_MEM;
    }
    else
    {
        rig_debug(RIG_DEBUG_ERR,
                  "alinco_get_vfo: unsupported VFO %s\n",
                  vfobuf);

        return -RIG_EPROTO;
    }

    return RIG_OK;
}


/*
 * alinco_set_freq
 * Assumes rig!=NULL
 */
int alinco_set_freq(RIG *rig, vfo_t vfo, freq_t freq)
{
    char freqbuf[BUFSZ];
    int freq_len;

    /* max 10 digits */
    if (freq >= GHz(10))
    {
        return -RIG_EINVAL;
    }

    /* at least 6 digits */
    freq_len = sprintf(freqbuf, AL CMD_RXFREQ "%06"PRIll EOM, (int64_t)freq);

    return alinco_transaction(rig, freqbuf, freq_len, NULL, NULL);
}


/*
 * where databuf points to a 26 char long buffer
 */
static int current_data_read(RIG *rig, char *databuf)
{
    int data_len, retval;

    retval = alinco_transaction(rig,
                                AL CMD_RDATA EOM,
                                strlen(AL CMD_RDATA EOM),
                                databuf,
                                &data_len);

    if (retval != RIG_OK)
    {
        return retval;
    }

    if (data_len != 26)
    {
        rig_debug(RIG_DEBUG_ERR,
                  "alinco_current_data_read: wrong answer %s, len=%d\n",
                  databuf,
                  data_len);

        return -RIG_ERJCTED;
    }

    return RIG_OK;
}


/*
 * alinco_get_freq
 * Assumes rig!=NULL, freq!=NULL
 */
int alinco_get_freq(RIG *rig, vfo_t vfo, freq_t *freq)
{
    int retval;
    char freqbuf[BUFSZ];

    retval = current_data_read(rig, freqbuf);

    if (retval != RIG_OK)
    {
        return retval;
    }

    /* extract RX freq */
    freqbuf[16] = '\0';
    sscanf(freqbuf + 6, "%"SCNfreq, freq);

    return RIG_OK;
}


/*
 * alinco_set_mode
 * Assumes rig!=NULL
 */
int alinco_set_mode(RIG *rig, vfo_t vfo, rmode_t mode, pbwidth_t width)
{
    char mdbuf[BUFSZ];
    int mdbuf_len, wide_filter, retval;
    char amode;

    switch (mode)
    {
    /* FIXME: MD_CWL or MD_CWU? */
    case RIG_MODE_CW:       amode = MD_CWU; break;
    case RIG_MODE_USB:      amode = MD_USB; break;
    case RIG_MODE_LSB:      amode = MD_LSB; break;
    case RIG_MODE_FM:       amode = MD_FM; break;
    case RIG_MODE_AM:       amode = MD_AM; break;
    default:
        rig_debug(RIG_DEBUG_ERR,
                  "alinco_set_mode: unsupported mode %d\n",
                  mode);

        return -RIG_EINVAL;
    }

    mdbuf_len = sprintf(mdbuf, AL CMD_MODE "%c" EOM, amode);
    retval = alinco_transaction(rig, mdbuf, mdbuf_len, NULL, NULL);

    if (retval != RIG_OK)
    {
        return retval;
    }

    if (width == RIG_PASSBAND_NOCHANGE) { return retval; }

    /*
     * TODO: please DX77 owners, check this, I'm not sure
     *          which passband is default!
     */
    if (width != RIG_PASSBAND_NORMAL
        && width < rig_passband_normal(rig, mode))
    {
        wide_filter = 0;
    }
    else
    {
        wide_filter = 1;
    }

    mdbuf_len = sprintf(mdbuf, AL CMD_FLTER "%02d" EOM, wide_filter);
    retval = alinco_transaction(rig, mdbuf, mdbuf_len, NULL, NULL);

    return retval;
}


/*
 * alinco_get_mode
 * Assumes rig!=NULL, mode!=NULL
 */
int alinco_get_mode(RIG *rig, vfo_t vfo, rmode_t *mode, pbwidth_t *width)
{
    int retval;
    int settings;
    char modebuf[BUFSZ];

    retval = current_data_read(rig, modebuf);

    if (retval != RIG_OK)
    {
        return retval;
    }

    /* FIXME: CWL&CWU: what are they? CW & CWR? */
    switch (modebuf[3])
    {
    case MD_CWL:
    case MD_CWU:      *mode = RIG_MODE_CW; break;
    case MD_USB:      *mode = RIG_MODE_USB; break;
    case MD_LSB:      *mode = RIG_MODE_LSB; break;
    case MD_AM:       *mode = RIG_MODE_AM; break;
    case MD_FM:       *mode = RIG_MODE_FM; break;
    default:
        rig_debug(RIG_DEBUG_ERR,
                  "alinco_get_mode: unknown mode %c%c\n",
                  modebuf[2],
                  modebuf[3]);

        return -RIG_EINVAL;
    }

    modebuf[2] = '\0';
    settings = strtol(modebuf, (char **)NULL, 16);

    /*
     * TODO: please DX77 owners, check this, I'm not sure
     *          which passband is default!
     */
    if (settings & 0x02)
    {
        *width = rig_passband_narrow(rig, *mode);
    }
    else
    {
        *width = rig_passband_normal(rig, *mode);
    }

    return RIG_OK;
}


/*
 * alinco_set_split
 * Assumes rig!=NULL
 */
int alinco_set_split_vfo(RIG *rig, vfo_t vfo, split_t split, vfo_t tx_vfo)
{
    int cmd_len;
    char cmdbuf[BUFSZ];

    cmd_len = sprintf(cmdbuf,
                      AL CMD_SPLT "%d" EOM,
                      split == RIG_SPLIT_ON ? 1 : 0);

    return alinco_transaction(rig, cmdbuf, cmd_len, NULL, NULL);
}


/*
 * alinco_get_split
 * Assumes rig!=NULL, split!=NULL
 */
int alinco_get_split_vfo(RIG *rig, vfo_t vfo, split_t *split, vfo_t *tx_vfo)
{
    int splt_len, retval;
    char spltbuf[BUFSZ];

    retval = alinco_transaction(rig,
                                AL CMD_RSPLT EOM,
                                strlen(AL CMD_RSPLT EOM),
                                spltbuf,
                                &splt_len);

    if (retval != RIG_OK)
    {
        return retval;
    }

    if (splt_len != 2)
    {
        rig_debug(RIG_DEBUG_ERR,
                  "alinco_get_split: wrong answer %s, len=%d\n",
                  spltbuf,
                  splt_len);

        return -RIG_ERJCTED;
    }

    spltbuf[splt_len] = '\0';

    if (!strcmp(spltbuf, "OF"))
    {
        *split = RIG_SPLIT_OFF;
    }
    else if (!strcmp(spltbuf, "ON"))
    {
        *split = RIG_SPLIT_ON;
    }
    else
    {
        rig_debug(RIG_DEBUG_ERR,
                  "alinco_get_split: unsupported SPLIT %s\n",
                  spltbuf);

        return -RIG_EPROTO;
    }

    return RIG_OK;
}


/*
 * alinco_set_split_freq
 * Assumes rig!=NULL
 */
int alinco_set_split_freq(RIG *rig, vfo_t vfo, freq_t tx_freq)
{
    char freqbuf[BUFSZ];
    int freq_len;
    int retval;

    /* max 10 digits */
    if (tx_freq >= GHz(10))
    {
        return -RIG_EINVAL;
    }

    /* at least 6 digits */
    freq_len = sprintf(freqbuf, AL CMD_TXFREQ "%06"PRIll EOM, (int64_t)tx_freq);

    retval = alinco_transaction(rig, freqbuf, freq_len, NULL, NULL);

    return retval;
}


/*
 * alinco_get_split_freq
 * Assumes rig!=NULL, rx_freq!=NULL, tx_freq!=NULL
 */
int alinco_get_split_freq(RIG *rig, vfo_t vfo, freq_t *tx_freq)
{
    int retval;
    char freqbuf[BUFSZ];

    retval = current_data_read(rig, freqbuf);

    if (retval != RIG_OK)
    {
        return retval;
    }

    /* extract TX freq first, as RX kills freqbuf[16] */
    freqbuf[26] = '\0';
    sscanf(freqbuf + 16, "%"SCNfreq, tx_freq);

    return RIG_OK;
}


/*
 * alinco_get_rit
 * Assumes rig!=NULL, split!=NULL
 */
int alinco_get_rit(RIG *rig, vfo_t vfo, shortfreq_t *rit)
{
    int rit_len, retval;
    char ritbuf[BUFSZ];

    /* read in Hertz unit */
    retval = alinco_transaction(rig,
                                AL CMD_RIT "0" EOM,
                                strlen(AL CMD_RIT "0" EOM),
                                ritbuf,
                                &rit_len);

    if (retval != RIG_OK)
    {
        return retval;
    }

    if (rit_len != 8)   /* || (ritbuf[0] != '+' && ritbuf[0] != '-')) { */
    {
        rig_debug(RIG_DEBUG_ERR,
                  "alinco_get_rit: wrong answer %s, len=%d\n",
                  ritbuf,
                  rit_len);

        return -RIG_ERJCTED;
    }

    ritbuf[rit_len] = '\0';
    ritbuf[0] = ' ';
    ritbuf[1] = ' ';
    ritbuf[2] = ' ';

    *rit = atoi(ritbuf);

    return RIG_OK;
}


/*
 * alinco_set_func
 * Assumes rig!=NULL
 */
int alinco_set_func(RIG *rig, vfo_t vfo, setting_t func, int status)
{
    int cmd_len;
    char cmdbuf[BUFSZ];

    /* Optimize:
     *   sort the switch cases with the most frequent first
     */
    switch (func)
    {
    case RIG_FUNC_TONE:
        cmd_len = sprintf(cmdbuf, AL CMD_CTCSS "%02d" EOM, status ? 51 : 0);

        return alinco_transaction(rig, cmdbuf, cmd_len, NULL, NULL);

    case RIG_FUNC_FAGC:
        cmd_len = sprintf(cmdbuf, AL CMD_AGC "%02d" EOM, status ? 1 : 2);

        return alinco_transaction(rig, cmdbuf, cmd_len, NULL, NULL);

    case RIG_FUNC_NB:
        cmd_len = sprintf(cmdbuf, AL CMD_NB "%d" EOM, status ? 1 : 0);

        return alinco_transaction(rig, cmdbuf, cmd_len, NULL, NULL);

    case RIG_FUNC_COMP:
        cmd_len = sprintf(cmdbuf, AL CMD_SDATA "C%d" EOM, status ? 1 : 0);
        return alinco_transaction(rig, cmdbuf, cmd_len, NULL, NULL);

    case RIG_FUNC_MON:
        cmd_len = sprintf(cmdbuf, AL CMD_MON "%d" EOM, status ? 1 : 0);

        return alinco_transaction(rig, cmdbuf, cmd_len, NULL, NULL);

    default:
        rig_debug(RIG_DEBUG_ERR, "Unsupported set_func %d\n", func);
        return -RIG_EINVAL;
    }

    return RIG_OK;
}


/*
 * alinco_get_func
 * Assumes rig!=NULL, status!=NULL
 */
int alinco_get_func(RIG *rig, vfo_t vfo, setting_t func, int *status)
{
    int retval;
    int settings;
    char funcbuf[BUFSZ];

    /* Optimize:
     *   sort the switch cases with the most frequent first
     */
    switch (func)
    {
    case RIG_FUNC_TONE:
        retval = current_data_read(rig, funcbuf);

        if (retval != RIG_OK)
        {
            return retval;
        }

        funcbuf[2] = '\0';
        settings = strtol(funcbuf, (char **)NULL, 16);
        *status = settings & 0x08 ? 1 : 0;
        break;

    case RIG_FUNC_FAGC:
        retval = current_data_read(rig, funcbuf);

        if (retval != RIG_OK)
        {
            return retval;
        }

        funcbuf[2] = '\0';
        settings = strtol(funcbuf, (char **)NULL, 16);
        *status = settings & 0x01 ? 1 : 0;
        break;

    case RIG_FUNC_NB:
        retval = current_data_read(rig, funcbuf);

        if (retval != RIG_OK)
        {
            return retval;
        }

        funcbuf[2] = '\0';
        settings = strtol(funcbuf, (char **)NULL, 16);
        *status = settings & 0x04 ? 1 : 0;
        break;

    default:
        rig_debug(RIG_DEBUG_ERR, "Unsupported get_func %d\n", func);
        return -RIG_EINVAL;
    }

    return RIG_OK;
}


/*
 * alinco_set_level
 * Assumes rig!=NULL
 * FIXME: cannot support PREAMP and ATT both at same time (make sense though)
 */
int alinco_set_level(RIG *rig, vfo_t vfo, setting_t level, value_t val)
{
    int cmd_len, lvl;
    char cmdbuf[BUFSZ];

    /* Optimize:
     *   sort the switch cases with the most frequent first
     */
    switch (level)
    {
    case RIG_LEVEL_PREAMP:
        switch (val.i)
        {
        case 0: lvl = 0; break;
        case 10: lvl = 1; break;
        default: rig_debug(RIG_DEBUG_ERR,
                           "Unsupported Preamp %d\n",
                           val.i);

            return -RIG_EINVAL;
        }

        cmd_len = sprintf(cmdbuf, AL CMD_RFGAIN "%02d" EOM, lvl);

        return alinco_transaction(rig, cmdbuf, cmd_len, NULL, NULL);

    case RIG_LEVEL_ATT:
        switch (val.i)
        {
        case 0: lvl = 0; break;
        case 10: lvl = 11; break;
        case 20: lvl = 10; break;
        default: rig_debug(RIG_DEBUG_ERR,
                           "Unsupported Att %d\n",
                           val.i);

            return -RIG_EINVAL;
        }

        cmd_len = sprintf(cmdbuf, AL CMD_RFGAIN "%02d" EOM, lvl);

        return alinco_transaction(rig, cmdbuf, cmd_len, NULL, NULL);

    case RIG_LEVEL_RFPOWER:
        cmd_len = sprintf(cmdbuf, AL CMD_PWR "%1d" EOM, val.f < 0.5 ? 1 : 0);

        return alinco_transaction(rig, cmdbuf, cmd_len, NULL, NULL);

    case RIG_LEVEL_KEYSPD:
        if (val.i < 6)
        {
            lvl = 31;
        }
        else if (val.i >= 6 && val.i < 20)
        {
            lvl = val.i + 25;
        }
        else if (val.i >= 20 && val.i <= 50)
        {
            lvl = val.i - 20;
        }
        else
        {
            lvl = 30;
        }

        cmd_len = sprintf(cmdbuf, AL CMD_SDATA "P%02d" EOM, lvl);

        return alinco_transaction(rig, cmdbuf, cmd_len, NULL, NULL);

    case RIG_LEVEL_CWPITCH:
        if (val.i < 426)
        {
            lvl = 5;
        }
        else if (val.i >= 426 && val.i <= 475)
        {
            lvl = 6;
        }
        else if (val.i >= 476 && val.i <= 525)
        {
            lvl = 7;
        }
        else if (val.i >= 526 && val.i <= 575)
        {
            lvl = 8;
        }
        else if (val.i >= 576 && val.i <= 625)
        {
            lvl = 9;
        }
        else if (val.i >= 626 && val.i <= 675)
        {
            lvl = 10;
        }
        else if (val.i >= 676 && val.i <= 725)
        {
            lvl = 11;
        }
        else if (val.i >= 726 && val.i <= 775)
        {
            lvl = 12;
        }
        else if (val.i >= 776 && val.i <= 825)
        {
            lvl = 0;
        }
        else if (val.i >= 826 && val.i <= 875)
        {
            lvl = 1;
        }
        else if (val.i >= 876 && val.i <= 925)
        {
            lvl = 2;
        }
        else if (val.i >= 926 && val.i <= 975)
        {
            lvl = 3;
        }
        else if (val.i >= 976 && val.i <= 1025)
        {
            lvl = 4;
        }
        else
        {
            lvl = 4;
        }

        cmd_len = sprintf(cmdbuf, AL CMD_SDATA "M%02d" EOM, lvl);

        return alinco_transaction(rig, cmdbuf, cmd_len, NULL, NULL);

    default:
        rig_debug(RIG_DEBUG_ERR, "Unsupported set_level %d\n", level);

        return -RIG_EINVAL;
    }

    return RIG_OK;
}


/*
 * alinco_get_level
 * Assumes rig!=NULL, val!=NULL
 */
int alinco_get_level(RIG *rig, vfo_t vfo, setting_t level, value_t *val)
{
    int retval, lvl_len;
    char lvlbuf[BUFSZ];

    /* Optimize:
     *   sort the switch cases with the most frequent first
     */
    switch (level)
    {
    case RIG_LEVEL_RAWSTR:
        /* read A/D converted value */
        retval = alinco_transaction(rig,
                                    AL CMD_SMETER "1" EOM,
                                    strlen(AL CMD_SMETER "1" EOM),
                                    lvlbuf,
                                    &lvl_len);

        if (retval != RIG_OK)
        {
            return retval;
        }

        if (lvl_len != 6)
        {
            rig_debug(RIG_DEBUG_ERR,
                      "alinco_get_level: wrong answer len=%d\n",
                      lvl_len);

            return -RIG_ERJCTED;
        }

        lvlbuf[6] = '\0';
        val->i = atoi(lvlbuf + 3);
        break;

    case RIG_LEVEL_PREAMP:
        retval = current_data_read(rig, lvlbuf);

        if (retval != RIG_OK)
        {
            return retval;
        }

        switch (lvlbuf[5])
        {
        case '2':
        case '3':
        case '0': val->i = 0; break;
        case '1': val->i = 10; break;
        default: rig_debug(RIG_DEBUG_ERR,
                           "Unknown RF Gain %c%c\n",
                           lvlbuf[4],
                           lvlbuf[5]);
        }

        break;

    case RIG_LEVEL_ATT:
        retval = current_data_read(rig, lvlbuf);

        if (retval != RIG_OK)
        {
            return retval;
        }

        switch (lvlbuf[5])
        {
        case '1':
        case '0': val->i = 0; break;
        case '2': val->i = 20; break;
        case '3': val->i = 10; break;
        default: rig_debug(RIG_DEBUG_ERR,
                           "Unknown RF Gain %c%c\n",
                           lvlbuf[4],
                           lvlbuf[5]);
        }

        break;

    case RIG_LEVEL_RFPOWER:
        retval = alinco_transaction(rig,
                                    AL CMD_RPOWER EOM,
                                    strlen(AL CMD_RPOWER EOM),
                                    lvlbuf,
                                    &lvl_len);

        if (retval != RIG_OK)
        {
            return retval;
        }

        if (lvl_len != 1)
        {
            rig_debug(RIG_DEBUG_ERR,
                      "alinco_get_level: wrong answer len=%d\n",
                      lvl_len);

            return -RIG_ERJCTED;
        }

        /* H or L */
        val->f = lvlbuf[0] == 'H' ? 1.0 : 0.0;
        break;

    default:
        rig_debug(RIG_DEBUG_ERR, "Unsupported get_level %d\n", level);

        return -RIG_EINVAL;
    }

    return RIG_OK;
}


/*
 * alinco_set_parm
 */
int alinco_set_parm(RIG *rig, setting_t parm, value_t val)
{
    int cmd_len;
    char cmdbuf[BUFSZ];

    /* Optimize:
     *   sort the switch cases with the most frequent first
     */
    switch (parm)
    {
    case RIG_PARM_BEEP:
        rig_debug(RIG_DEBUG_ERR, "val is %d\n", val.i);
        cmd_len = sprintf(cmdbuf, AL CMD_SDATA "A%d" EOM, val.i ? 1 : 0);
        return alinco_transaction(rig, cmdbuf, cmd_len, NULL, NULL);

    case RIG_PARM_BACKLIGHT:
        rig_debug(RIG_DEBUG_ERR, "val is %0f\n", val.f);
        cmd_len = sprintf(cmdbuf, AL CMD_SDATA "O%d" EOM, (int)(val.f * 5));
        return alinco_transaction(rig, cmdbuf, cmd_len, NULL, NULL);

    default:
        rig_debug(RIG_DEBUG_ERR, "Unsupported set_parm %d\n", parm);
        return -RIG_EINVAL;
    }

    return RIG_OK;
}


/*
 * alinco_set_ctcss_tone
 * Assumes rig!=NULL, rig->caps->ctcss_list != NULL
 */
int alinco_set_ctcss_tone(RIG *rig, vfo_t vfo, tone_t tone)
{
    const struct rig_caps *caps;
    unsigned char tonebuf[BUFSZ];
    int tone_len;
    int i;

    caps = rig->caps;

    /* TODO: replace 200 by something like RIGTONEMAX */
    for (i = 0; caps->ctcss_list[i] != 0 && i < 200; i++)
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

    tone_len = sprintf((char *) tonebuf, AL CMD_CTCSS "%02d" EOM, i + 1);

    return alinco_transaction(rig, (char *) tonebuf, tone_len, NULL, NULL);
}


/*
 * alinco_get_ptt
 * Assumes rig!=NULL, ptt!=NULL
 */
int alinco_get_ptt(RIG *rig, vfo_t vfo, ptt_t *ptt)
{
    char pttbuf[BUFSZ];
    int ptt_len, retval;

    retval = alinco_transaction(rig,
                                AL CMD_PTT EOM,
                                strlen(AL CMD_PTT EOM),
                                pttbuf,
                                &ptt_len);

    if (retval != RIG_OK)
    {
        return retval;
    }

    if (ptt_len != 3 && ptt_len != 4)
    {
        rig_debug(RIG_DEBUG_ERR,
                  "alinco_get_ptt: wrong answer %s, len=%d\n",
                  pttbuf,
                  ptt_len);

        return -RIG_ERJCTED;
    }

    pttbuf[ptt_len] = '\0';

    if (!strcmp(pttbuf, "SEND"))
    {
        *ptt = RIG_PTT_OFF;
    }
    else if (!strcmp(pttbuf, "REV"))
    {
        *ptt = RIG_PTT_ON;
    }
    else
    {
        rig_debug(RIG_DEBUG_ERR,
                  "alinco_get_ptt: unknown PTT %s\n",
                  pttbuf);

        return -RIG_EPROTO;
    }

    return RIG_OK;
}


/*
 * alinco_get_dcd
 * Assumes rig!=NULL, dcd!=NULL
 */
int alinco_get_dcd(RIG *rig, vfo_t vfo, dcd_t *dcd)
{
    char dcdbuf[BUFSZ];
    int dcd_len, retval;

    retval = alinco_transaction(rig,
                                AL CMD_SQL EOM,
                                strlen(AL CMD_SQL EOM),
                                dcdbuf,
                                &dcd_len);

    if (retval != RIG_OK)
    {
        return retval;
    }

    if (dcd_len != 4 && dcd_len != 5)
    {
        rig_debug(RIG_DEBUG_ERR,
                  "alinco_get_dcd: wrong answer %s, len=%d\n",
                  dcdbuf,
                  dcd_len);

        return -RIG_ERJCTED;
    }

    dcdbuf[dcd_len] = '\0';

    if (!strcmp(dcdbuf, "OPEN"))
    {
        *dcd = RIG_DCD_ON;
    }
    else if (!strcmp(dcdbuf, "CLOSE"))
    {
        *dcd = RIG_DCD_OFF;
    }
    else
    {
        rig_debug(RIG_DEBUG_ERR,
                  "alinco_get_dcd: unknown SQL %s\n",
                  dcdbuf);

        return -RIG_EPROTO;
    }

    return RIG_OK;
}


/*
 * alinco_set_mem
 * Assumes rig!=NULL
 * FIXME: check we're in memory mode first
 */
int alinco_set_mem(RIG *rig, vfo_t vfo, int ch)
{
    char cmdbuf[BUFSZ];
    int cmd_len;

    if (ch < 0 || ch > 99)
    {
        return -RIG_EINVAL;
    }

    cmd_len = sprintf(cmdbuf, AL CMD_MCALL "%02d" EOM, ch);

    return alinco_transaction(rig, cmdbuf, cmd_len, NULL, NULL);
}


/*
 * alinco_get_mem
 * Assumes rig!=NULL, !vfo
 */
int alinco_get_mem(RIG *rig, vfo_t vfo, int *ch)
{
    char membuf[BUFSZ];
    int mem_len, retval;

    retval = alinco_transaction(rig,
                                AL CMD_RMEM EOM,
                                strlen(AL CMD_RMEM EOM),
                                membuf,
                                &mem_len);

    if (retval != RIG_OK)
    {
        return retval;
    }

    if (mem_len != 2)
    {
        rig_debug(RIG_DEBUG_ERR,
                  "alinco_get_mem: wrong answer %s, len=%d\n",
                  membuf,
                  mem_len);

        return -RIG_ERJCTED;
    }

    membuf[mem_len] = '\0';

    *ch = atoi(membuf);

    if (*ch < 0 || *ch > 99)
    {
        rig_debug(RIG_DEBUG_ERR, "alinco_get_mem: unknown mem %s\n",
                  membuf);
        return -RIG_EPROTO;
    }

    return RIG_OK;
}


/*
 * initrigs_alinco is called by rig_backend_load
 */
DECLARE_INITRIG_BACKEND(alinco)
{
    rig_debug(RIG_DEBUG_VERBOSE, "alinco: _init called\n");

    rig_register(&dx77_caps);

    return RIG_OK;
}
