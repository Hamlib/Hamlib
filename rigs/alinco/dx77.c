/*
 *  Hamlib Alinco backend - DX77 description
 *  Copyright (c) 2001-2005 by Stephane Fillod
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

#include <hamlib/rig.h>
#include "idx_builtin.h"
#include "alinco.h"
#include <serial.h>
#include <misc.h>
#include <cal.h>

/*
 * modes in use by the "2G" command
 */
#define MD_LSB  '0'
#define MD_USB  '1'
#define MD_CWL  '2'
#define MD_CWU  '3'
#define MD_AM   '4'
#define MD_FM   '5'

/* Line Feed */
#define EOM "\x0d"
#define LF "\x0a"

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
#define CMD_RSELECT "3K"    /* SELECT Position read */


#define DX77_ALL_MODES (RIG_MODE_AM|RIG_MODE_CW|RIG_MODE_SSB|RIG_MODE_FM)
#define DX77_OTHER_TX_MODES (RIG_MODE_CW|RIG_MODE_SSB|RIG_MODE_FM)
#define DX77_AM_TX_MODES RIG_MODE_AM

#define DX77_FUNC (RIG_FUNC_FAGC|RIG_FUNC_NB|RIG_FUNC_TONE)

#define DX77_LEVEL_ALL (RIG_LEVEL_RAWSTR|RIG_LEVEL_RFPOWER|RIG_LEVEL_KEYSPD|RIG_LEVEL_BKINDL|RIG_LEVEL_CWPITCH)

#define DX77_PARM_ALL (RIG_PARM_BEEP|RIG_PARM_BACKLIGHT)

#define DX77_VFO (RIG_VFO_A|RIG_VFO_B)

/* 90 is S9 */
#define DX77_STR_CAL { 13, {                    \
            {   0, -60 },                       \
            {  28, -48 },                       \
            {  36, -42 },                       \
            {  42, -36 },                       \
            {  50, -30 },                       \
            {  58, -24 },                       \
            {  66, -18 },                       \
            {  74, -12 },                       \
            {  82, -6 },                        \
            {  90, 0 },                         \
            { 132, 20 },                        \
            { 174, 40 },                        \
            { 216, 60 },                        \
        } }


int dx77_set_vfo(RIG *rig, vfo_t vfo);
int dx77_get_vfo(RIG *rig, vfo_t *vfo);
int dx77_set_freq(RIG *rig, vfo_t vfo, freq_t freq);
int dx77_get_freq(RIG *rig, vfo_t vfo, freq_t *freq);
int dx77_set_mode(RIG *rig, vfo_t vfo, rmode_t mode, pbwidth_t width);
int dx77_get_mode(RIG *rig, vfo_t vfo, rmode_t *mode, pbwidth_t *width);
int dx77_set_split_vfo(RIG *rig, vfo_t vfo, split_t split, vfo_t tx_vfo);
int dx77_get_split_vfo(RIG *rig, vfo_t vfo, split_t *split, vfo_t *tx_vfo);
int dx77_set_split_freq(RIG *rig, vfo_t vfo, freq_t tx_freq);
int dx77_get_split_freq(RIG *rig, vfo_t vfo, freq_t *tx_freq);
int dx77_set_func(RIG *rig, vfo_t vfo, setting_t func, int status);
int dx77_get_func(RIG *rig, vfo_t vfo, setting_t func, int *status);
int dx77_set_level(RIG *rig, vfo_t vfo, setting_t level, value_t val);
int dx77_get_level(RIG *rig, vfo_t vfo, setting_t level, value_t *val);
int dx77_set_parm(RIG *rig, setting_t parm, value_t val);
int dx77_get_parm(RIG *rig, setting_t parm, value_t *val);
int dx77_set_ctcss_tone(RIG *rig, vfo_t vfo, tone_t tone);
int dx77_get_rit(RIG *rig, vfo_t vfo, shortfreq_t *rit);
int dx77_get_ptt(RIG *rig, vfo_t vfo, ptt_t *ptt);
int dx77_get_dcd(RIG *rig, vfo_t vfo, dcd_t *dcd);
int dx77_set_mem(RIG *rig, vfo_t vfo, int ch);
int dx77_get_mem(RIG *rig, vfo_t vfo, int *ch);



/*
 * dx77 rig capabilities.
 *
 * protocol is documented at
 *      http://www.alinco.com/pdf.files/DX77-77_SOFTWARE_MANUAL.pdf
 *
 * This backend was a pleasure to develop. Documentation is clear,
 * and the protocol logical. I'm wondering is the rig's good too. --SF
 *
 * TODO:
 *  - get_parm/set_parm and some LEVELs left (Set Data "2W" command).
 *  - tuner
 *  - up/down
 *  - scan
 */
const struct rig_caps dx77_caps =
{
    RIG_MODEL(RIG_MODEL_DX77),
    .model_name =       "DX-77",
    .mfg_name =         "Alinco",
    .version =          BACKEND_VER ".1",
    .copyright =        "LGPL",
    .status =           RIG_STATUS_STABLE,
    .rig_type =         RIG_TYPE_TRANSCEIVER,
    .ptt_type =         RIG_PTT_NONE,
    .dcd_type =         RIG_DCD_RIG,
    .port_type =        RIG_PORT_SERIAL,
    .serial_rate_min =  9600,
    .serial_rate_max =  9600,
    .serial_data_bits = 8,
    .serial_stop_bits = 2,
    .serial_parity =    RIG_PARITY_NONE,
    .serial_handshake = RIG_HANDSHAKE_NONE,
    .write_delay =      0,
    .post_write_delay = 0,
    .timeout =          200,
    .retry =            3,

    .has_get_func =     DX77_FUNC,
    .has_set_func =     DX77_FUNC | RIG_FUNC_MON | RIG_FUNC_COMP,
    .has_get_level =    DX77_LEVEL_ALL,
    .has_set_level =    RIG_LEVEL_SET(DX77_LEVEL_ALL),
    .has_get_parm =     RIG_PARM_NONE,
    .has_set_parm =     RIG_PARM_SET(DX77_PARM_ALL),
    .level_gran =
    {
        [LVL_RAWSTR] =  { .min = { .i = 0 }, .max = { .i = 255 } },
    },
    .parm_gran =        {},
    .ctcss_list =       common_ctcss_list,
    .dcs_list =         NULL,
    .preamp =           { 10, RIG_DBLST_END },
    .attenuator =       { 10, 20, RIG_DBLST_END },
    .max_rit =          kHz(1),
    .max_xit =          Hz(0),
    .max_ifshift =      Hz(0),
    .targetable_vfo =   0,
    .transceive =       RIG_TRN_OFF,
    .bank_qty =         0,
    .chan_desc_sz =     0,

    .chan_list =
    {
        { 0, 99, RIG_MTYPE_MEM },
        RIG_CHAN_END,
    },

    .rx_range_list1 = {
        {
            .startf = kHz(500), .endf = MHz(30), .modes = DX77_OTHER_TX_MODES,
            .low_power = -1, .high_power = -1, RIG_VFO_A, RIG_ANT_NONE
        },
        {
            .startf = kHz(500), .endf = MHz(30), .modes = DX77_AM_TX_MODES,
            .low_power = -1, .high_power = -1, RIG_VFO_A, RIG_ANT_NONE
        },
        RIG_FRNG_END,
    },
    .tx_range_list1 = {RIG_FRNG_END,},
    .tx_range_list2 = {RIG_FRNG_END,},

    .rx_range_list2 =
    {
        {kHz(500), MHz(30), DX77_ALL_MODES, -1, -1, DX77_VFO},
        RIG_FRNG_END,
    },
    .tx_range_list2 =
    {
        {kHz(1800), MHz(2) - 100, DX77_OTHER_TX_MODES, W(10), W(100), DX77_VFO},
        {kHz(1800), MHz(2) - 100, DX77_AM_TX_MODES, W(4), W(40), DX77_VFO},
        {kHz(3500), MHz(4) - 100, DX77_OTHER_TX_MODES, W(10), W(100), DX77_VFO},
        {kHz(3500), MHz(4) - 100, DX77_AM_TX_MODES, W(4), W(40), DX77_VFO},
        {MHz(7), kHz(7300), DX77_OTHER_TX_MODES, W(10), W(100), DX77_VFO},
        {MHz(7), kHz(7300), DX77_AM_TX_MODES, W(4), W(40), DX77_VFO},
        {kHz(10100), kHz(10150), DX77_OTHER_TX_MODES, W(10), W(100), DX77_VFO},
        {kHz(10100), kHz(10150), DX77_AM_TX_MODES, W(4), W(40), DX77_VFO},
        {MHz(14), kHz(14350), DX77_OTHER_TX_MODES, W(10), W(100), DX77_VFO},
        {MHz(14), kHz(14350), DX77_AM_TX_MODES, W(4), W(40), DX77_VFO},
        {kHz(18068), kHz(18168), DX77_OTHER_TX_MODES, W(10), W(100), DX77_VFO},
        {kHz(18068), kHz(18168), DX77_AM_TX_MODES, W(4), W(40), DX77_VFO},
        {MHz(21), kHz(21450), DX77_OTHER_TX_MODES, W(10), W(100), DX77_VFO},
        {MHz(21), kHz(21450), DX77_AM_TX_MODES, W(4), W(40), DX77_VFO},
        {kHz(24890), kHz(24990), DX77_OTHER_TX_MODES, W(10), W(100), DX77_VFO},
        {kHz(24890), kHz(24990), DX77_AM_TX_MODES, W(4), W(40), DX77_VFO},
        {MHz(28), kHz(29700), DX77_OTHER_TX_MODES, W(10), W(100), DX77_VFO},
        {MHz(28), kHz(29700), DX77_AM_TX_MODES, W(4), W(40), DX77_VFO},
        RIG_FRNG_END,
    },
    .tuning_steps =
    {
        {DX77_ALL_MODES, 10},  /* FIXME: add other ts */
        RIG_TS_END,
    },
    /* mode/filter list, remember: order matters! */
    .filters =
    {
        {RIG_MODE_SSB | RIG_MODE_CW, kHz(2.7)},
        {RIG_MODE_CW, kHz(0.5)},
        {RIG_MODE_AM | RIG_MODE_FM, kHz(8)},
        {RIG_MODE_AM, kHz(2.7)},
        RIG_FLT_END,
    },
    .str_cal =          DX77_STR_CAL,

    .set_freq =         dx77_set_freq,
    .get_freq =         dx77_get_freq,
    .set_mode =         dx77_set_mode,
    .get_mode =         dx77_get_mode,
    .set_vfo =          dx77_set_vfo,
    .get_vfo =          dx77_get_vfo,
    .set_split_vfo =    dx77_set_split_vfo,
    .get_split_vfo =    dx77_get_split_vfo,
    .set_split_freq =   dx77_set_split_freq,
    .get_split_freq =   dx77_get_split_freq,
    .set_ctcss_tone =   dx77_set_ctcss_tone,
    .get_rit =          dx77_get_rit,
    .get_ptt =          dx77_get_ptt,
    .get_dcd =          dx77_get_dcd,
    .set_func =         dx77_set_func,
    .get_func =         dx77_get_func,
    .set_parm =         dx77_set_parm,
    .set_level =        dx77_set_level,
    .get_level =        dx77_get_level,
    .set_mem =          dx77_set_mem,
    .get_mem =          dx77_get_mem,
    .hamlib_check_rig_caps = HAMLIB_CHECK_RIG_CAPS
};


/*
 * Function definitions below
 */

/*
 * dx77_transaction
 * We assume that rig!=NULL, rig->state!= NULL, data!=NULL, data_len!=NULL
 * Otherwise, you'll get a nice seg fault. You've been warned!
 * TODO: error case handling
 */
int dx77_transaction(RIG *rig,
                     const char *cmd,
                     int cmd_len,
                     char *data,
                     int *data_len)
{

    int retval;
    struct rig_state *rs;
    char echobuf[BUFSZ + 1];

    if (cmd == NULL)
    {
        rig_debug(RIG_DEBUG_ERR,
                  "%s: null argument for cmd?\n", __func__);
        return -RIG_EINTERNAL;
    }

    rs = &rig->state;

    rig_flush(&rs->rigport);

    retval = write_block(&rs->rigport, (unsigned char *) cmd, cmd_len);

    if (retval != RIG_OK)
    {
        return retval;
    }

    /*
     * Transceiver sends an echo of cmd followed by a CR/LF
     * TODO: check whether cmd and echobuf match (optional)
     */
    retval = read_string(&rs->rigport, (unsigned char *) echobuf, BUFSZ,
                         LF, strlen(LF), 0, 1);

    if (retval < 0)
    {
        return retval;
    }

    if (!(data && data_len))
    {
        rig_debug(RIG_DEBUG_ERR, "%s: data and datalen not both NULL??\n", __func__);
        return -RIG_EINTERNAL;
    }

    /* no data expected, check for OK returned */
    if (data == NULL)
    {
        retval = read_string(&rs->rigport, (unsigned char *) echobuf, BUFSZ,
                             LF, strlen(LF), 0, 1);

        if (retval < 0)
        {
            return retval;
        }

        if (retval > 2) { retval -= 2; }

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

    retval = read_string(&rs->rigport, (unsigned char *) data, BUFSZ,
                         LF, strlen(LF), 0, 1);

    if (retval < 0)
    {
        return retval;
    }

    *data_len = retval;

    /* strip CR/LF from string
     */
    data[0] = 0;

    if (*data_len > 2)
    {
        *data_len -= 2;
        data[*data_len] = 0;
    }

    return RIG_OK;
}

/*
 * dx77_set_vfo
 * Assumes rig!=NULL
 */
int dx77_set_vfo(RIG *rig, vfo_t vfo)
{
    char cmdbuf[BUFSZ];
    char vfo_num;

    switch (vfo)
    {
    case RIG_VFO_A: vfo_num = '1'; break;

    case RIG_VFO_B: vfo_num = '2'; break;

    case RIG_VFO_MEM:
        return dx77_transaction(rig,
                                AL CMD_MEMMD "0" EOM,
                                strlen(AL CMD_MEMMD "0" EOM),
                                NULL,
                                NULL);

    default:
        rig_debug(RIG_DEBUG_ERR,
                  "dx77_set_vfo: unsupported VFO %s\n",
                  rig_strvfo(vfo));

        return -RIG_EINVAL;
    }

    SNPRINTF(cmdbuf, sizeof(cmdbuf), AL CMD_VFO "%c" EOM, vfo_num);

    return dx77_transaction(rig, cmdbuf, strlen(cmdbuf), NULL, NULL);
}


/*
 * dx77_get_vfo
 * Assumes rig!=NULL, !vfo
 */
int dx77_get_vfo(RIG *rig, vfo_t *vfo)
{
    char vfobuf[BUFSZ];
    int vfo_len, retval;

    retval = dx77_transaction(rig,
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
                  "dx77_get_vfo: wrong answer %s, "
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
                  "dx77_get_vfo: unsupported VFO %s\n",
                  vfobuf);

        return -RIG_EPROTO;
    }

    return RIG_OK;
}


/*
 * dx77_set_freq
 * Assumes rig!=NULL
 */
int dx77_set_freq(RIG *rig, vfo_t vfo, freq_t freq)
{
    char freqbuf[BUFSZ];

    /* max 10 digits */
    if (freq >= GHz(10))
    {
        return -RIG_EINVAL;
    }

    /* at least 6 digits */
    // cppcheck-suppress *
    SNPRINTF(freqbuf, sizeof(freqbuf), AL CMD_RXFREQ "%06"PRIll EOM, (int64_t)freq);

    return dx77_transaction(rig, freqbuf, strlen(freqbuf), NULL, NULL);
}


/*
 * where databuf points to a 26 char long buffer
 */
static int current_data_read(RIG *rig, char *databuf)
{
    int data_len, retval;

    retval = dx77_transaction(rig,
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
                  "dx77_current_data_read: wrong answer %s, len=%d\n",
                  databuf,
                  data_len);

        return -RIG_ERJCTED;
    }

    return RIG_OK;
}


/*
 * dx77_get_freq
 * Assumes rig!=NULL, freq!=NULL
 */
int dx77_get_freq(RIG *rig, vfo_t vfo, freq_t *freq)
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
 * dx77_set_mode
 * Assumes rig!=NULL
 */
int dx77_set_mode(RIG *rig, vfo_t vfo, rmode_t mode, pbwidth_t width)
{
    char mdbuf[BUFSZ];
    int wide_filter, retval;
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
        rig_debug(RIG_DEBUG_ERR, "dx77_set_mode: unsupported mode %s\n",
                  rig_strrmode(mode));

        return -RIG_EINVAL;
    }

    SNPRINTF(mdbuf, sizeof(mdbuf), AL CMD_MODE "%c" EOM, amode);
    retval = dx77_transaction(rig, mdbuf, strlen(mdbuf), NULL, NULL);

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

    SNPRINTF(mdbuf, sizeof(mdbuf), AL CMD_FLTER "%02d" EOM, wide_filter);
    retval = dx77_transaction(rig, mdbuf, strlen(mdbuf), NULL, NULL);

    return retval;
}


/*
 * dx77_get_mode
 * Assumes rig!=NULL, mode!=NULL
 */
int dx77_get_mode(RIG *rig, vfo_t vfo, rmode_t *mode, pbwidth_t *width)
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
                  "dx77_get_mode: unknown mode %c%c\n",
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
 * dx77_set_split
 * Assumes rig!=NULL
 */
int dx77_set_split_vfo(RIG *rig, vfo_t vfo, split_t split, vfo_t tx_vfo)
{
    char cmdbuf[BUFSZ];

    SNPRINTF(cmdbuf, sizeof(cmdbuf),
             AL CMD_SPLT "%d" EOM,
             split == RIG_SPLIT_ON ? 1 : 0);

    return dx77_transaction(rig, cmdbuf, strlen(cmdbuf), NULL, NULL);
}


/*
 * dx77_get_split
 * Assumes rig!=NULL, split!=NULL
 */
int dx77_get_split_vfo(RIG *rig, vfo_t vfo, split_t *split, vfo_t *tx_vfo)
{
    int splt_len, retval;
    char spltbuf[BUFSZ];

    retval = dx77_transaction(rig,
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
                  "dx77_get_split: wrong answer %s, len=%d\n",
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
                  "dx77_get_split: unsupported SPLIT %s\n",
                  spltbuf);

        return -RIG_EPROTO;
    }

    return RIG_OK;
}


/*
 * dx77_set_split_freq
 * Assumes rig!=NULL
 */
int dx77_set_split_freq(RIG *rig, vfo_t vfo, freq_t tx_freq)
{
    char freqbuf[BUFSZ];
    int retval;

    /* max 10 digits */
    if (tx_freq >= GHz(10))
    {
        return -RIG_EINVAL;
    }

    /* at least 6 digits */
    SNPRINTF(freqbuf, sizeof(freqbuf), AL CMD_TXFREQ "%06"PRIll EOM,
             (int64_t)tx_freq);

    retval = dx77_transaction(rig, freqbuf, strlen(freqbuf), NULL, NULL);

    return retval;
}


/*
 * dx77_get_split_freq
 * Assumes rig!=NULL, rx_freq!=NULL, tx_freq!=NULL
 */
int dx77_get_split_freq(RIG *rig, vfo_t vfo, freq_t *tx_freq)
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
 * dx77_get_rit
 * Assumes rig!=NULL, split!=NULL
 */
int dx77_get_rit(RIG *rig, vfo_t vfo, shortfreq_t *rit)
{
    int rit_len, retval;
    char ritbuf[BUFSZ];

    /* read in Hertz unit */
    retval = dx77_transaction(rig,
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
                  "dx77_get_rit: wrong answer %s, len=%d\n",
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
 * dx77_set_func
 * Assumes rig!=NULL
 */
int dx77_set_func(RIG *rig, vfo_t vfo, setting_t func, int status)
{
    char cmdbuf[BUFSZ];

    /* Optimize:
     *   sort the switch cases with the most frequent first
     */
    switch (func)
    {
    case RIG_FUNC_TONE:
        SNPRINTF(cmdbuf, sizeof(cmdbuf), AL CMD_CTCSS "%02d" EOM, status ? 51 : 0);

        return dx77_transaction(rig, cmdbuf, strlen(cmdbuf), NULL, NULL);

    case RIG_FUNC_FAGC:
        SNPRINTF(cmdbuf, sizeof(cmdbuf), AL CMD_AGC "%02d" EOM, status ? 1 : 2);

        return dx77_transaction(rig, cmdbuf, strlen(cmdbuf), NULL, NULL);

    case RIG_FUNC_NB:
        SNPRINTF(cmdbuf, sizeof(cmdbuf), AL CMD_NB "%d" EOM, status ? 1 : 0);

        return dx77_transaction(rig, cmdbuf, strlen(cmdbuf), NULL, NULL);

    case RIG_FUNC_COMP:
        SNPRINTF(cmdbuf, sizeof(cmdbuf), AL CMD_SDATA "C%d" EOM, status ? 1 : 0);
        return dx77_transaction(rig, cmdbuf, strlen(cmdbuf), NULL, NULL);

    case RIG_FUNC_MON:
        SNPRINTF(cmdbuf, sizeof(cmdbuf), AL CMD_MON "%d" EOM, status ? 1 : 0);

        return dx77_transaction(rig, cmdbuf, strlen(cmdbuf), NULL, NULL);

    default:
        rig_debug(RIG_DEBUG_ERR, "Unsupported set_func %d\n", (int)func);
        return -RIG_EINVAL;
    }

    return RIG_OK;
}


/*
 * dx77_get_func
 * Assumes rig!=NULL, status!=NULL
 */
int dx77_get_func(RIG *rig, vfo_t vfo, setting_t func, int *status)
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
        *status = (settings & 0x08) ? 1 : 0;
        break;

    case RIG_FUNC_FAGC:
        retval = current_data_read(rig, funcbuf);

        if (retval != RIG_OK)
        {
            return retval;
        }

        funcbuf[2] = '\0';
        settings = strtol(funcbuf, (char **)NULL, 16);
        *status = (settings & 0x01) ? 1 : 0;
        break;

    case RIG_FUNC_NB:
        retval = current_data_read(rig, funcbuf);

        if (retval != RIG_OK)
        {
            return retval;
        }

        funcbuf[2] = '\0';
        settings = strtol(funcbuf, (char **)NULL, 16);
        *status = (settings & 0x04) ? 1 : 0;
        break;

    default:
        rig_debug(RIG_DEBUG_ERR, "Unsupported get_func %d\n", (int)func);
        return -RIG_EINVAL;
    }

    return RIG_OK;
}


/*
 * dx77_set_level
 * Assumes rig!=NULL
 * FIXME: cannot support PREAMP and ATT both at same time (make sense though)
 */
int dx77_set_level(RIG *rig, vfo_t vfo, setting_t level, value_t val)
{
    int lvl;
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

        SNPRINTF(cmdbuf, sizeof(cmdbuf), AL CMD_RFGAIN "%02d" EOM, lvl);

        return dx77_transaction(rig, cmdbuf, strlen(cmdbuf), NULL, NULL);

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

        SNPRINTF(cmdbuf, sizeof(cmdbuf), AL CMD_RFGAIN "%02d" EOM, lvl);

        return dx77_transaction(rig, cmdbuf, strlen(cmdbuf), NULL, NULL);

    case RIG_LEVEL_RFPOWER:
        SNPRINTF(cmdbuf, sizeof(cmdbuf), AL CMD_PWR "%1d" EOM, val.f < 0.5 ? 1 : 0);

        return dx77_transaction(rig, cmdbuf, strlen(cmdbuf), NULL, NULL);

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

        SNPRINTF(cmdbuf, sizeof(cmdbuf), AL CMD_SDATA "P%02d" EOM, lvl);

        return dx77_transaction(rig, cmdbuf, strlen(cmdbuf), NULL, NULL);

    case RIG_LEVEL_CWPITCH:
        lvl = 4;

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

        SNPRINTF(cmdbuf, sizeof(cmdbuf), AL CMD_SDATA "M%02d" EOM, lvl);

        return dx77_transaction(rig, cmdbuf, strlen(cmdbuf), NULL, NULL);

    default:
        rig_debug(RIG_DEBUG_ERR, "Unsupported set_level %s\n", rig_strlevel(level));

        return -RIG_EINVAL;
    }

    return RIG_OK;
}


/*
 * dx77_get_level
 * Assumes rig!=NULL, val!=NULL
 */
int dx77_get_level(RIG *rig, vfo_t vfo, setting_t level, value_t *val)
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
        retval = dx77_transaction(rig,
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
                      "dx77_get_level: wrong answer len=%d\n",
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
        retval = dx77_transaction(rig,
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
                      "dx77_get_level: wrong answer len=%d\n",
                      lvl_len);

            return -RIG_ERJCTED;
        }

        /* H or L */
        val->f = lvlbuf[0] == 'H' ? 1.0 : 0.0;
        break;

    default:
        rig_debug(RIG_DEBUG_ERR, "Unsupported get_level %s\n", rig_strlevel(level));

        return -RIG_EINVAL;
    }

    return RIG_OK;
}


/*
 * dx77_set_parm
 */
int dx77_set_parm(RIG *rig, setting_t parm, value_t val)
{
    char cmdbuf[BUFSZ];

    /* Optimize:
     *   sort the switch cases with the most frequent first
     */
    switch (parm)
    {
    case RIG_PARM_BEEP:
        rig_debug(RIG_DEBUG_ERR, "val is %d\n", val.i);
        SNPRINTF(cmdbuf, sizeof(cmdbuf), AL CMD_SDATA "A%d" EOM, val.i ? 1 : 0);
        return dx77_transaction(rig, cmdbuf, strlen(cmdbuf), NULL, NULL);

    case RIG_PARM_BACKLIGHT:
        rig_debug(RIG_DEBUG_ERR, "val is %0f\n", val.f);
        SNPRINTF(cmdbuf, sizeof(cmdbuf), AL CMD_SDATA "O%d" EOM, (int)(val.f * 5));
        return dx77_transaction(rig, cmdbuf, strlen(cmdbuf), NULL, NULL);

    default:
        rig_debug(RIG_DEBUG_ERR, "Unsupported set_parm %d\n", (int)parm);
        return -RIG_EINVAL;
    }

    return RIG_OK;
}


/*
 * dx77_set_ctcss_tone
 * Assumes rig!=NULL, rig->caps->ctcss_list != NULL
 */
int dx77_set_ctcss_tone(RIG *rig, vfo_t vfo, tone_t tone)
{
    const struct rig_caps *caps;
    unsigned char tonebuf[BUFSZ];
    int i;

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
        return -RIG_EINVAL;
    }

    SNPRINTF((char *) tonebuf, sizeof(tonebuf), AL CMD_CTCSS "%02d" EOM, i + 1);

    return dx77_transaction(rig, (char *) tonebuf, strlen((char *)tonebuf), NULL,
                            NULL);
}


/*
 * dx77_get_ptt
 * Assumes rig!=NULL, ptt!=NULL
 */
int dx77_get_ptt(RIG *rig, vfo_t vfo, ptt_t *ptt)
{
    char pttbuf[BUFSZ];
    int ptt_len, retval;

    retval = dx77_transaction(rig,
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
                  "dx77_get_ptt: wrong answer %s, len=%d\n",
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
                  "dx77_get_ptt: unknown PTT %s\n",
                  pttbuf);

        return -RIG_EPROTO;
    }

    return RIG_OK;
}


/*
 * dx77_get_dcd
 * Assumes rig!=NULL, dcd!=NULL
 */
int dx77_get_dcd(RIG *rig, vfo_t vfo, dcd_t *dcd)
{
    char dcdbuf[BUFSZ];
    int dcd_len, retval;

    retval = dx77_transaction(rig,
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
                  "dx77_get_dcd: wrong answer %s, len=%d\n",
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
                  "dx77_get_dcd: unknown SQL %s\n",
                  dcdbuf);

        return -RIG_EPROTO;
    }

    return RIG_OK;
}


/*
 * dx77_set_mem
 * Assumes rig!=NULL
 * FIXME: check we're in memory mode first
 */
int dx77_set_mem(RIG *rig, vfo_t vfo, int ch)
{
    char cmdbuf[BUFSZ];

    if (ch < 0 || ch > 99)
    {
        return -RIG_EINVAL;
    }

    SNPRINTF(cmdbuf, sizeof(cmdbuf), AL CMD_MCALL "%02d" EOM, ch);

    return dx77_transaction(rig, cmdbuf, strlen(cmdbuf), NULL, NULL);
}


/*
 * dx77_get_mem
 * Assumes rig!=NULL, !vfo
 */
int dx77_get_mem(RIG *rig, vfo_t vfo, int *ch)
{
    char membuf[BUFSZ];
    int mem_len, retval;

    retval = dx77_transaction(rig,
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
                  "dx77_get_mem: wrong answer %s, len=%d\n",
                  membuf,
                  mem_len);

        return -RIG_ERJCTED;
    }

    membuf[mem_len] = '\0';

    *ch = atoi(membuf);

    if (*ch < 0 || *ch > 99)
    {
        rig_debug(RIG_DEBUG_ERR, "dx77_get_mem: unknown mem %s\n",
                  membuf);
        return -RIG_EPROTO;
    }

    return RIG_OK;
}

