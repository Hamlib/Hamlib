/*
 *  Hamlib TenTenc backend - RX-340 description
 *  Copyright (c) 2003-2009 by Stephane Fillod
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
#include <string.h>

#include "hamlib/rig.h"
#include "bandplan.h"
#include "serial.h"
#include "misc.h"
#include "num_stdio.h"


#define RX340_MODES (RIG_MODE_FM|RIG_MODE_CW|RIG_MODE_SSB|RIG_MODE_DSB|\
            RIG_MODE_AM|RIG_MODE_AMS)

#define RX340_FUNCS (RIG_FUNC_NB)

#define RX340_LEVELS (RIG_LEVEL_STRENGTH| \
                RIG_LEVEL_RF|RIG_LEVEL_IF| \
                RIG_LEVEL_NOTCHF|RIG_LEVEL_SQL| \
                RIG_LEVEL_CWPITCH|RIG_LEVEL_AGC| \
                RIG_LEVEL_ATT|RIG_LEVEL_PREAMP)

#define RX340_ANTS (RIG_ANT_1)

#define RX340_PARMS (RIG_PARM_NONE)

#define RX340_VFO (RIG_VFO_A)

#define RX340_VFO_OPS (RIG_OP_TO_VFO|RIG_OP_FROM_VFO)

/* TODO: levels.. */
#define RX340_MEM_CAP {        \
        .freq = 1,      \
        .mode = 1,      \
        .width = 1,     \
}


#if 0
static int rx340_init(RIG *rig);
static int rx340_cleanup(RIG *rig);
#endif
static int rx340_open(RIG *rig);
static int rx340_close(RIG *rig);
static int rx340_set_freq(RIG *rig, vfo_t vfo, freq_t freq);
static int rx340_get_freq(RIG *rig, vfo_t vfo, freq_t *freq);
static int rx340_set_mode(RIG *rig, vfo_t vfo, rmode_t mode, pbwidth_t width);
static int rx340_get_mode(RIG *rig, vfo_t vfo, rmode_t *mode, pbwidth_t *width);
static int rx340_set_level(RIG *rig, vfo_t vfo, setting_t level, value_t val);
static int rx340_get_level(RIG *rig, vfo_t vfo, setting_t level, value_t *val);
static const char *rx340_get_info(RIG *rig);

/*
 * RX340 receiver capabilities.
 *
 * Protocol is documented at
 *      http://radio.tentec.com/downloads/receivers/RX340
 *
 * TODO: from/to memory, scan, get_level, ..
 * supposes non-multidrop
 */
const struct rig_caps rx340_caps =
{
    RIG_MODEL(RIG_MODEL_RX340),
    .model_name = "RX-340",
    .mfg_name =  "Ten-Tec",
    .version =  "20160409.0",
    .copyright =  "LGPL",
    .status =  RIG_STATUS_ALPHA,
    .rig_type =  RIG_TYPE_RECEIVER,
    .ptt_type =  RIG_PTT_NONE,
    .dcd_type =  RIG_DCD_NONE,
    .port_type =  RIG_PORT_SERIAL,
    .serial_rate_min =  75,
    .serial_rate_max =  38400,
    .serial_data_bits =  8,
    .serial_stop_bits =  1,
    .serial_parity =  RIG_PARITY_NONE,
    .serial_handshake =  RIG_HANDSHAKE_NONE,
    .write_delay =  0,
    .post_write_delay =  0,
    .timeout =  400,
    .retry =  3,

    .has_get_func =  RX340_FUNCS,
    .has_set_func =  RX340_FUNCS,
    .has_get_level =  RX340_LEVELS,
    .has_set_level =  RIG_LEVEL_SET(RX340_LEVELS),
    .has_get_parm =  RX340_PARMS,
    .has_set_parm =  RX340_PARMS,
    .level_gran =  {},                 /* FIXME: granularity */
    .parm_gran =  {},
    .ctcss_list =  NULL,
    .dcs_list =  NULL,
    .preamp =   { 10, RIG_DBLST_END },
    .attenuator =   { 15, RIG_DBLST_END },
    .max_rit =  Hz(0),
    .max_xit =  Hz(0),
    .max_ifshift =  kHz(2),
    .targetable_vfo =  RIG_TARGETABLE_NONE,
    .transceive =  RIG_TRN_OFF,
    .bank_qty =   0,
    .chan_desc_sz =  0,

    .chan_list =  {
        {   1,  100, RIG_MTYPE_MEM, RX340_MEM_CAP },
    },

    .rx_range_list1 =  {
        {kHz(0), MHz(30), RX340_MODES, -1, -1, RX340_VFO, RX340_ANTS},
        RIG_FRNG_END,
    },
    .tx_range_list1 =  {
        RIG_FRNG_END,
    },

    .rx_range_list2 =  {
        {kHz(0), MHz(30), RX340_MODES, -1, -1, RX340_VFO, RX340_ANTS},
        RIG_FRNG_END,
    },
    .tx_range_list2 =  {
        RIG_FRNG_END,
    },

    .tuning_steps =  {
        {RX340_MODES, 1},
        RIG_TS_END,
    },
    /* mode/filter list, remember: order matters! */
    .filters =  {
        {RX340_MODES, kHz(3.2)},
        {RX340_MODES, Hz(100)},
        {RX340_MODES, kHz(16)},
        {RX340_MODES, 0},
        RIG_FLT_END,
    },
    .priv = (void *)NULL,

    .rig_open =  rx340_open,
    .rig_close =  rx340_close,
    .set_freq =  rx340_set_freq,
    .get_freq =  rx340_get_freq,
    .set_mode =  rx340_set_mode,
    .get_mode =  rx340_get_mode,
    .set_level =  rx340_set_level,
    .get_level =  rx340_get_level,
    .get_info =  rx340_get_info,
    .hamlib_check_rig_caps = HAMLIB_CHECK_RIG_CAPS
};

/*
 * Function definitions below
 */

#define BUFSZ 128

#define EOM "\015"  /* CR */

#define RX340_AM  '1'
#define RX340_FM  '2'
#define RX340_CW  '3'
#define RX340_CW1 '4'
#define RX340_ISB '5'
#define RX340_LSB '6'
#define RX340_USB '7'
#define RX340_SAM '8'


/*
 * rx340_transaction
 * read exactly data_len bytes
 * We assume that rig!=NULL, rig->state!= NULL, data!=NULL, data_len!=NULL
 * Otherwise, you'll get a nice seg fault. You've been warned!
 */
static int rx340_transaction(RIG *rig, const char *cmd, int cmd_len, char *data,
                             int *data_len)
{
    int retval;
    struct rig_state *rs;

    rs = &rig->state;

    rig_flush(&rs->rigport);

    retval = write_block(&rs->rigport, (unsigned char *) cmd, cmd_len);

    if (retval != RIG_OK)
    {
        return retval;
    }

    /* no data expected, TODO: flush input? */
    if (!data || !data_len)
    {
        return RIG_OK;
    }

    retval = read_string(&rs->rigport, (unsigned char *) data, BUFSZ, EOM, 1, 0, 1);

    if (retval < 0)
    {
        return retval;
    }

    *data_len = retval;

    return RIG_OK;
}


#if 0
/*
 * rx340_init:
 * Basically, it just sets up *priv
 */
int rx340_init(RIG *rig)
{
    struct rx340_priv_data *priv;

    priv = (struct rx340_priv_data *)calloc(1, sizeof(struct rx340_priv_data));

    if (!priv)
    {
        /* whoops! memory shortage! */
        return -RIG_ENOMEM;
    }

    memset(priv, 0, sizeof(struct rx340_priv_data));

    /*
     * set arbitrary initial status
     */

    rig->state.priv = (rig_ptr_t)priv;

    return RIG_OK;
}

/*
 * Tentec generic rx340_cleanup routine
 * the serial port is closed by the frontend
 */
int rx340_cleanup(RIG *rig)
{
    if (rig->state.priv)
    {
        free(rig->state.priv);
    }

    rig->state.priv = NULL;

    return RIG_OK;
}
#endif

int rx340_open(RIG *rig)
{
    struct rig_state *rs = &rig->state;

#define REMOTE_CMD "*R1"EOM
    return write_block(&rs->rigport, (unsigned char *) REMOTE_CMD,
                       strlen(REMOTE_CMD));
}

int rx340_close(RIG *rig)
{
    struct rig_state *rs = &rig->state;

#define LOCAL_CMD "*R0"EOM
    return write_block(&rs->rigport, (unsigned char *) LOCAL_CMD,
                       strlen(LOCAL_CMD));
}

/*
 * rx340_set_freq
 */
int rx340_set_freq(RIG *rig, vfo_t vfo, freq_t freq)
{
    struct rig_state *rs = &rig->state;
    int retval;
    char freqbuf[16];

    SNPRINTF(freqbuf, sizeof(freqbuf), "F%.6f" EOM, freq / 1e6);

    retval = write_block(&rs->rigport, (unsigned char *) freqbuf, strlen(freqbuf));

    return retval;
}

/*
 * rx340_get_freq
 * Assumes rig!=NULL, freq!=NULL
 */
int rx340_get_freq(RIG *rig, vfo_t vfo, freq_t *freq)
{
    char buf[BUFSZ];
    int buf_len = 0;
    int retval;
    double f;

#define REPORT_FREQ "TF"EOM
    retval = rx340_transaction(rig, REPORT_FREQ, strlen(REPORT_FREQ), buf,
                               &buf_len);

    if (retval < 0)
    {
        return retval;
    }

    if (buf_len < 2 || buf[0] != 'F' || num_sscanf(buf + 1, "%lf", &f) != 1)
    {
        return -RIG_EPROTO;
    }

    *freq = f * 1e6;

    return RIG_OK;
}

/*
 * rx340_set_mode
 * Assumes rig!=NULL
 */
int rx340_set_mode(RIG *rig, vfo_t vfo, rmode_t mode, pbwidth_t width)
{
    struct rig_state *rs = &rig->state;
    char dmode;
    int retval;
    char mdbuf[32];

    switch (mode)
    {
    case RIG_MODE_USB:      dmode = RX340_USB; break;

    case RIG_MODE_LSB:      dmode = RX340_LSB; break;

    case RIG_MODE_CW:       dmode = RX340_CW; break;

    case RIG_MODE_FM:       dmode = RX340_FM; break;

    case RIG_MODE_AM:       dmode = RX340_AM; break;

    case RIG_MODE_AMS:      dmode = RX340_SAM; break;

    case RIG_MODE_DSB:      dmode = RX340_ISB; break;

    default:
        rig_debug(RIG_DEBUG_ERR,
                  "%s: unsupported mode %s\n",
                  __func__, rig_strrmode(mode));
        return -RIG_EINVAL;
    }

    if (width != RIG_PASSBAND_NOCHANGE)
    {
        if (width == RIG_PASSBAND_NORMAL)
        {
            width = rig_passband_normal(rig, mode);
        }

        /*
         * Set DETECTION MODE and IF FILTER
         */
        SNPRINTF(mdbuf, sizeof(mdbuf), "D%cI%.02f" EOM,
                 dmode, (float)width / 1e3);
    }
    else
    {
        /*
         * Set DETECTION MODE
         */
        SNPRINTF(mdbuf, sizeof(mdbuf),  "D%c" EOM, dmode);
    }

    retval = write_block(&rs->rigport, (unsigned char *) mdbuf, strlen(mdbuf));

    return retval;
}

/*
 * rx340_get_mode
 * Assumes rig!=NULL, mode!=NULL
 */
int rx340_get_mode(RIG *rig, vfo_t vfo, rmode_t *mode, pbwidth_t *width)
{
    char buf[BUFSZ];
    int buf_len = 0;
    int retval;
    double f;

#define REPORT_MODEFILTER "TDI"EOM
    retval = rx340_transaction(rig, REPORT_MODEFILTER,
                               strlen(REPORT_MODEFILTER), buf, &buf_len);

    if (retval < 0)
    {
        return retval;
    }

    if (buf_len < 4 || buf[0] != 'D' || buf[2] != 'I')
    {
        return -RIG_EPROTO;
    }

    switch (buf[1])
    {
    case RX340_USB: *mode = RIG_MODE_USB; break;

    case RX340_LSB: *mode = RIG_MODE_LSB; break;

    case RX340_CW1:
    case RX340_CW:  *mode = RIG_MODE_CW; break;

    case RX340_FM:  *mode = RIG_MODE_FM; break;

    case RX340_AM:  *mode = RIG_MODE_AM; break;

    case RX340_SAM: *mode = RIG_MODE_AMS; break;

    case RX340_ISB: *mode = RIG_MODE_DSB; break;

    default:
        rig_debug(RIG_DEBUG_ERR,
                  "%s: unknown mode '%c'\n",
                  __func__, buf[1]);
        return -RIG_EPROTO;
    }

    if (num_sscanf(buf + 3, "%lf", &f) != 1)
    {
        return -RIG_EPROTO;
    }

    *width = f * 1e3;

    return RIG_OK;
}


/*
 * rx340_set_level
 * Assumes rig!=NULL
 * cannot support PREAMP and ATT both at same time (make sense though)
 */
int rx340_set_level(RIG *rig, vfo_t vfo, setting_t level, value_t val)
{
    struct rig_state *rs = &rig->state;
    int retval = RIG_OK;
    char cmdbuf[32];

    switch (level)
    {
    case RIG_LEVEL_ATT:
        SNPRINTF(cmdbuf, sizeof(cmdbuf), "K%c" EOM, val.i ? '3' : '1');
        break;

    case RIG_LEVEL_PREAMP:
        SNPRINTF(cmdbuf, sizeof(cmdbuf), "K%c" EOM, val.i ? '2' : '1');
        break;

    case RIG_LEVEL_AGC:
        /* default to MEDIUM */
        SNPRINTF(cmdbuf, sizeof(cmdbuf), "M%c" EOM,
                 val.i == RIG_AGC_SLOW ? '3' : (
                     val.i == RIG_AGC_FAST ? '1' : '2'));
        break;

    case RIG_LEVEL_RF:
        SNPRINTF(cmdbuf, sizeof(cmdbuf), "A%d" EOM, 120 - (int)(val.f * 120));
        break;

    case RIG_LEVEL_SQL:
        SNPRINTF(cmdbuf, sizeof(cmdbuf), "Q%d" EOM, 150 - (int)(val.f * 150));
        break;

    case RIG_LEVEL_NOTCHF:
        SNPRINTF(cmdbuf, sizeof(cmdbuf), "N%f" EOM, ((float)val.i) / 1e3);
        break;

    case RIG_LEVEL_IF:
        SNPRINTF(cmdbuf, sizeof(cmdbuf), "P%f" EOM, ((float)val.i) / 1e3);
        break;

    case RIG_LEVEL_CWPITCH:
        /* only in CW mode */
        SNPRINTF(cmdbuf, sizeof(cmdbuf), "B%f" EOM, ((float)val.i) / 1e3);
        break;

    default:
        rig_debug(RIG_DEBUG_ERR, "%s: Unsupported set_level %s\n",
                  __func__, rig_strlevel(level));
        return -RIG_EINVAL;
    }

    retval = write_block(&rs->rigport, (unsigned char *) cmdbuf, strlen(cmdbuf));
    return retval;
}


/*
 * rx340_get_level
 * Assumes rig!=NULL, val!=NULL
 */
int rx340_get_level(RIG *rig, vfo_t vfo, setting_t level, value_t *val)
{
    int retval, lvl_len;
    char lvlbuf[BUFSZ];

    switch (level)
    {
    case RIG_LEVEL_STRENGTH:
#define REPORT_STRENGTH "X"EOM
        retval = rx340_transaction(rig, REPORT_STRENGTH,
                                   strlen(REPORT_STRENGTH), lvlbuf, &lvl_len);

        if (retval != RIG_OK)
        {
            return retval;
        }

        if (lvl_len < 2 || lvlbuf[0] != 'X')
        {
            rig_debug(RIG_DEBUG_ERR, "%s: wrong answer"
                      "len=%d\n", __func__, lvl_len);
            return -RIG_EPROTO;
        }

        /* range 0-150 covering the dynamic range
         * of receiver -140..+10dBm
         */
        val->i = atoi(lvlbuf + 1) - 140 + 73;
        break;

    case RIG_LEVEL_AGC:
    case RIG_LEVEL_ATT:
    case RIG_LEVEL_PREAMP:
    case RIG_LEVEL_RF:
    case RIG_LEVEL_IF:
    case RIG_LEVEL_SQL:
    case RIG_LEVEL_CWPITCH:
    case RIG_LEVEL_NOTCHF:
        return -RIG_ENIMPL;

    default:
        rig_debug(RIG_DEBUG_ERR, "%s: Unsupported get_level %s\n",
                  __func__, rig_strlevel(level));
        return -RIG_EINVAL;
    }

    return RIG_OK;
}


/*
 * rx340_get_info
 * Assumes rig!=NULL
 */
const char *rx340_get_info(RIG *rig)
{
    static char buf[BUFSZ]; /* FIXME: reentrancy */
    int firmware_len = 0, retval;

#define REPORT_FIRM "V"EOM
    retval = rx340_transaction(rig, REPORT_FIRM, strlen(REPORT_FIRM), buf,
                               &firmware_len);

    if ((retval != RIG_OK) || (firmware_len > 10))
    {
        rig_debug(RIG_DEBUG_ERR, "%s: ack NG, len=%d\n",
                  __func__, firmware_len);
        return NULL;
    }

    return buf;
}


