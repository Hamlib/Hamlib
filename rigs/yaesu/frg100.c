/*
 * frg100.c - (C) Stephane Fillod 2002-2005
 *
 * This shared library provides an API for communicating
 * via serial interface to an FRG-100 using the "CAT" interface
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
#include <string.h>  /* String function definitions */

#include "hamlib/rig.h"
#include "serial.h"
#include "misc.h"
#include "yaesu.h"
#include "frg100.h"

enum frg100_native_cmd_e
{
    FRG100_NATIVE_RECALL_MEM = 0,       /* 0x02, p1=ch */
    FRG100_NATIVE_VFO_TO_MEM,           /* 0x03, p1=ch, p2=0 */
    FRG100_NATIVE_MEM_HIDE,             /* 0x03, p1=ch, p2=1 */
    FRG100_NATIVE_VFO_A,                /* 0x05 */
    FRG100_NATIVE_FREQ_SET,             /* 0x0a, p1:4=freq */
    FRG100_NATIVE_MODE_SET_LSB,         /* 0x0c, p1=0x00 */
    FRG100_NATIVE_MODE_SET_USB,         /* 0x0c, p1=0x01 */
    FRG100_NATIVE_MODE_SET_CW_W,        /* 0x0c, p1=0x02 */
    FRG100_NATIVE_MODE_SET_CW_N,        /* 0x0c, p1=0x03 */
    FRG100_NATIVE_MODE_SET_AM,          /* 0x0c, p1=0x04 */
    FRG100_NATIVE_MODE_SET_RTTY_LSB_W,  /* 0x0c, p1=0x08 */
    FRG100_NATIVE_MODE_SET_RTTY_USB_W,  /* 0x0c, p1=0x09 */
    FRG100_NATIVE_MODE_SET_H3E,         /* 0x0c, p1=0x0d */
    FRG100_NATIVE_MODE_SET_RTTY_LSB_N,  /* 0x0c, p1=0x0e */
    FRG100_NATIVE_MODE_SET_RTTY_USB_N,  /* 0x0c, p1=0x0f */
    FRG100_NATIVE_PTT_OFF,              /* 0x0f, p1=0 */
    FRG100_NATIVE_PTT_ON,               /* 0x0f, p1=1 */
    FRG100_NATIVE_UPDATE_MEM_CHNL,      /* 0x10, p1=1 */
    FRG100_NATIVE_UPDATE_OP_DATA,       /* 0x10, p1=2 */
    FRG100_NATIVE_UPDATE_VFO_DATA,      /* 0x10, p1=3 */
    FRG100_NATIVE_TX_POWER_LOW,         /* 0x18 */
    FRG100_NATIVE_TX_POWER_MID,         /* 0x28 */
    FRG100_NATIVE_TX_POWER_HI,          /* 0x48 */
    FRG100_NATIVE_CPY_RX_TO_TX,         /* 0x85 */
    FRG100_NATIVE_TX_FREQ_SET,          /* 0x8a, p1:4=freq */
    FRG100_NATIVE_OP_FREQ_STEP_UP,      /* 0x8e, p1=0 */
    FRG100_NATIVE_OP_FREQ_STEP_DOWN,    /* 0x8e, p1=1 */
    FRG100_NATIVE_READ_METER,           /* 0xf7 */
    FRG100_NATIVE_READ_FLAGS,           /* 0xfa */
    FRG100_NATIVE_SIZE
};



#define FRG100_MODES (RIG_MODE_CW|RIG_MODE_SSB|RIG_MODE_AM|RIG_MODE_FM)

#define FRG100_VFOS (RIG_VFO_A)
#define FRG100_ANTS 0


/* TODO: get real measure numbers */
#define FRG100_STR_CAL { 2, { \
        {  0, -60 }, /* S0 -6dB */ \
        { 60,  60 }  /* +60 */ \
        } }

#define FRG100_MEM_CAP { \
                        .freq = 1,      \
                        .mode = 1,      \
                        .width = 1     \
                    }
static const yaesu_cmd_set_t ncmd[] =
{
    { 0, { 0x00, 0x00, 0x00, 0x00, 0x02 } }, /* Recall Memory */
    { 0, { 0x00, 0x00, 0x00, 0x00, 0x03 } }, /* VFO to MEM */
    { 0, { 0x00, 0x00, 0x01, 0x00, 0x03 } }, /* Hide Memory Channel */
    { 1, { 0x00, 0x00, 0x00, 0x00, 0x05 } }, /* Select VFO (A) */
    { 0, { 0x00, 0x00, 0x00, 0x00, 0x0a } }, /* Set Op Freq */
    { 1, { 0x00, 0x00, 0x00, 0x00, 0x0c } }, /* OP Mode Set LSB */
    { 1, { 0x00, 0x00, 0x00, 0x01, 0x0c } }, /* OP Mode Set USB */
    { 1, { 0x00, 0x00, 0x00, 0x02, 0x0c } }, /* OP Mode Set CW-W */
    { 1, { 0x00, 0x00, 0x00, 0x03, 0x0c } }, /* OP Mode Set CW-N */
    { 1, { 0x00, 0x00, 0x00, 0x04, 0x0c } }, /* OP Mode Set AM */
    { 1, { 0x00, 0x00, 0x00, 0x08, 0x0c } }, /* OP Mode Set RTTY LSB-W */
    { 1, { 0x00, 0x00, 0x00, 0x09, 0x0c } }, /* OP Mode Set RTTY USB-W */
    { 1, { 0x00, 0x00, 0x00, 0x0d, 0x0c } }, /* OP Mode Set H3E */
    { 1, { 0x00, 0x00, 0x00, 0x0e, 0x0c } }, /* OP Mode Set RTTY LSB-N */
    { 1, { 0x00, 0x00, 0x00, 0x0f, 0x0c } }, /* OP Mode Set RTTY USB-N */
    { 1, { 0x00, 0x00, 0x00, 0x00, 0x0f } }, /* PTT (OFF) */
    { 1, { 0x00, 0x00, 0x00, 0x01, 0x0f } }, /* PTT (ON) */
    { 1, { 0x00, 0x00, 0x00, 0x01, 0x10 } }, /* Update Memory Ch Number */
    { 1, { 0x00, 0x00, 0x00, 0x02, 0x10 } }, /* Update Op Data */
    { 1, { 0x00, 0x00, 0x00, 0x03, 0x10 } }, /* Update VFO Data */
    { 1, { 0x00, 0x00, 0x00, 0x00, 0x18 } }, /* Set TX power low */
    { 1, { 0x00, 0x00, 0x00, 0x00, 0x28 } }, /* Set TX power mid */
    { 1, { 0x00, 0x00, 0x00, 0x00, 0x48 } }, /* Set TX power hi  */
    { 1, { 0x00, 0x00, 0x00, 0x00, 0x85 } }, /* Copy RX to TX */
    { 0, { 0x00, 0x00, 0x00, 0x00, 0x8a } }, /* Set TX Freq only */
    { 1, { 0x00, 0x00, 0x00, 0x00, 0x8e } }, /* Step Operating Frequency Up */
    { 1, { 0x00, 0x00, 0x00, 0x01, 0x8e } }, /* Step Operating Frequency Down */
    { 1, { 0x00, 0x00, 0x00, 0x00, 0xf7 } }, /* Read Meter */
    { 1, { 0x00, 0x00, 0x00, 0x00, 0xfa } }, /* Read Status Flags */
};

/* Private helper function prototypes */

static int frg100_open(RIG *rig);

static int frg100_get_freq(RIG *rig, vfo_t vfo, freq_t *freq);
static int frg100_set_freq(RIG *rig, vfo_t vfo, freq_t freq);
static int frg100_set_mode(RIG *rig, vfo_t vfo, rmode_t mode, pbwidth_t width);
static int frg100_set_vfo(RIG *rig, vfo_t vfo);
static int frg100_set_powerstat(RIG *rig, powerstat_t status);
static int frg100_get_level(RIG *rig, vfo_t vfo, setting_t level, value_t *val);

#if 0
static int rig2mode(RIG *rig, int md, rmode_t *mode, pbwidth_t *width);
#endif
static int mode2rig(RIG *rig, rmode_t mode, pbwidth_t width);

/*
 * frg100 rigs capabilities.
 * Also this struct is READONLY!
 *
 * TODO:
 *  - Memory recall
 *  - VFO->M & M->VFO
 *  - Lock, Up/Down
 *  - Status update
 *  - Clock set, Timer
 *  - Scan skip
 *  - Step Frequency Size
 *  - Dim
 */

const struct rig_caps frg100_caps =
{
    RIG_MODEL(RIG_MODEL_FRG100),
    .model_name =         "FRG-100",
    .mfg_name =           "Yaesu",
    .version =            "20160409.0",
    .copyright =          "LGPL",
    .status =             RIG_STATUS_STABLE,
    .rig_type =           RIG_TYPE_RECEIVER,
    .ptt_type =           RIG_PTT_NONE,
    .dcd_type =           RIG_DCD_NONE,
    .port_type =          RIG_PORT_SERIAL,
    .serial_rate_min =    4800,
    .serial_rate_max =    4800,
    .serial_data_bits =   8,
    .serial_stop_bits =   2,
    .serial_parity =      RIG_PARITY_NONE,
    .serial_handshake =   RIG_HANDSHAKE_NONE,
    .write_delay =        20,
    .post_write_delay =   300,
    .timeout =            2000,
    .retry =              0,
    .has_get_func =       RIG_FUNC_LOCK,
    .has_set_func =       RIG_FUNC_LOCK,
    .has_get_level =      RIG_LEVEL_RAWSTR,
    .has_set_level =      RIG_LEVEL_BAND_SELECT,
    .has_get_parm =       RIG_PARM_NONE,
    .has_set_parm =       RIG_PARM_BACKLIGHT,
    .level_gran =
    {
#include "level_gran_yaesu.h"
    },
    .vfo_ops =        RIG_OP_FROM_VFO | RIG_OP_TO_VFO | RIG_OP_UP | RIG_OP_DOWN,
    .preamp =             { RIG_DBLST_END, },
    .attenuator =         { RIG_DBLST_END, },
    .max_rit =            Hz(0),
    .max_xit =            Hz(0),
    .max_ifshift =        Hz(0),
    .targetable_vfo =     RIG_TARGETABLE_NONE,
    .transceive =         RIG_TRN_OFF,
    .bank_qty =           0,
    .chan_desc_sz =       0,
    .chan_list =          {
        {      1,  0x32, RIG_MTYPE_MEM, FRG100_MEM_CAP },
        {   0x33,  0x34, RIG_MTYPE_EDGE },
    },
    .rx_range_list1 =     {
        {kHz(50), MHz(30), FRG100_MODES, 0, 0, FRG100_VFOS, FRG100_ANTS },
        RIG_FRNG_END,
    }, /* Region 1 rx ranges */

    .tx_range_list1 =     {
        {kHz(50), MHz(30), FRG100_MODES, 0, 0, FRG100_VFOS, FRG100_ANTS },
        RIG_FRNG_END,
    },    /* region 1 TX ranges */

    .rx_range_list2 =     {
        {kHz(50), MHz(30), FRG100_MODES, 0, 0, FRG100_VFOS, FRG100_ANTS },
        RIG_FRNG_END,
    }, /* Region 2 rx ranges */

    .tx_range_list2 =     {
        {kHz(50), MHz(30), FRG100_MODES, 0, 0, FRG100_VFOS, FRG100_ANTS },
        RIG_FRNG_END,
    },    /* region 2 TX ranges */

    .tuning_steps =       {
        {RIG_MODE_SSB | RIG_MODE_CW, Hz(10)},
        {RIG_MODE_FM | RIG_MODE_AM, Hz(100)},
        RIG_TS_END,
    },

    /* mode/filter list, remember: order matters! */
    .filters =            {
        {RIG_MODE_SSB | RIG_MODE_CW,  kHz(2.4)},
        {RIG_MODE_CW,   Hz(500)},
        {RIG_MODE_AM,   kHz(6)},
        {RIG_MODE_AM,   kHz(4)},
        {RIG_MODE_FM,   kHz(15)},
        RIG_FLT_END,
    },

    .str_cal = FRG100_STR_CAL,

    .rig_open =       frg100_open,

    .get_freq =           frg100_get_freq,
    .set_freq =           frg100_set_freq,
    .set_mode =           frg100_set_mode,
    .set_vfo =            frg100_set_vfo,
    .get_level =      frg100_get_level,

    .set_powerstat =  frg100_set_powerstat,
    .hamlib_check_rig_caps = HAMLIB_CHECK_RIG_CAPS
};

static inline int frg100_channel_is_ok(unsigned char channel)
{
    return ((channel >= FRG100_MIN_CHANNEL) &&
            (channel <= FRG100_MAX_CHANNEL)) ? 1 : 0;
}

static int frg100_do_transaction(RIG *rig,
                                 const unsigned char cmd[YAESU_CMD_LENGTH],
                                 unsigned char *retbuf, size_t retbuf_len)
{
    struct rig_state    *rs;
    unsigned char       default_retbuf[1];
    int                 retval;

    if (retbuf == NULL)
    {
        retbuf = default_retbuf;
        retbuf_len = sizeof(default_retbuf);
    }

    rs = &rig->state;
    memset(retbuf, 0, retbuf_len);

    rig_flush(&rs->rigport);
    retval = write_block(&rs->rigport, cmd, YAESU_CMD_LENGTH);

    if (retval != RIG_OK) { return retval; }

    retval = read_block(&rs->rigport, retbuf, retbuf_len);

    if (retval != retbuf_len)
    {
        if ((retval == 1) && (retbuf[0] == FRG100_CMD_RETCODE_ERROR)) { return -RIG_ERJCTED; }

        return -RIG_EIO;
    }

    if (retval == 1)
    {
        if ((cmd[4] == FRG100_CMD_UPDATE) && (cmd[3] == 0x01))
        {
            /* read memory channel number */
            if (frg100_channel_is_ok(retbuf[0] + 1))
            {
                /* WARNING: Documentation bug, actually
                            we got 0--199 for channels 1--200 */
                return RIG_OK;
            }

            if (retbuf[0] == FRG100_CMD_RETCODE_ERROR) { return -RIG_ERJCTED; }

            return -RIG_EIO;
        }

        if (retbuf[0] == FRG100_CMD_RETCODE_OK) { return RIG_OK; }

        if (retbuf[0] == FRG100_CMD_RETCODE_ERROR) { return -RIG_ERJCTED; }

        return -RIG_EIO;
    }

    return RIG_OK;
}

static inline freq_t frg100_read_freq_from_buf(const unsigned char p[])
{
    /* WARNING: documentation bug, actually frequency stored in bytes 0..2 only,
                byte 3 is not used and contain zero */
    return ((((((unsigned)p[2]) << 8) + p[1]) << 8) + p[0]) * 10.0;
}

static inline int frg100_read_op_data_raw(RIG *rig, unsigned char reply[])
{
    if (rig == NULL) { return -RIG_EINVAL; }

    return frg100_do_transaction(rig, ncmd[FRG100_NATIVE_UPDATE_OP_DATA].nseq,
                                 reply, FRG100_OP_DATA_LENGTH);
}


static int frg100_read_op_data(RIG *rig, unsigned char *hwmode,
                               freq_t *rx_freq, freq_t *tx_freq)
{
    int                 ret;
    unsigned char       reply[FRG100_OP_DATA_LENGTH];

    if ((ret = frg100_read_op_data_raw(rig, reply)) != RIG_OK) { return ret; }

    if (hwmode  != NULL) { *hwmode  = reply[7]; }

    if (rx_freq != NULL) { *rx_freq = frg100_read_freq_from_buf(reply + 2); }

    if (tx_freq != NULL) { *tx_freq = frg100_read_freq_from_buf(reply + 11); }

    return RIG_OK;
}



/*
 * frg100_open  routine
 *
 */
int frg100_open(RIG *rig)
{
    unsigned char cmd[YAESU_CMD_LENGTH] = { 0x00, 0x00, 0x00, 0x00, 0x0e};

    rig_debug(RIG_DEBUG_TRACE, "%s: called\n", __func__);

    /* send 0 delay pacing */
    return write_block(&rig->state.rigport, cmd, YAESU_CMD_LENGTH);

}


int frg100_get_freq(RIG *rig, vfo_t vfo, freq_t *freq)
{
    (void) vfo;
    return frg100_read_op_data(rig, NULL, freq, NULL);
}

int frg100_set_freq(RIG *rig, vfo_t vfo, freq_t freq)
{
    unsigned char cmd[YAESU_CMD_LENGTH] = { 0x00, 0x00, 0x00, 0x00, 0x0a};

    /* store bcd format in cmd (LSB) */
    to_bcd(cmd, freq / 10, 8);

    /* Frequency set */
    return write_block(&rig->state.rigport, cmd, YAESU_CMD_LENGTH);
}


int frg100_set_mode(RIG *rig, vfo_t vfo, rmode_t mode, pbwidth_t width)
{
    unsigned char cmd[YAESU_CMD_LENGTH] = { 0x00, 0x00, 0x00, 0x00, 0x0c};


    /* fill in p1 */
    cmd[3] = mode2rig(rig, mode, width);

    return write_block(&rig->state.rigport, cmd, YAESU_CMD_LENGTH);
}


/*
 * Actually, this is tape relay, 0xfe=on
 */
int frg100_set_powerstat(RIG *rig, powerstat_t status)
{
    unsigned char cmd[YAESU_CMD_LENGTH] = { 0x00, 0x00, 0x00, 0x00, 0x20};

    rig_debug(RIG_DEBUG_TRACE, "%s: called\n", __func__);

    cmd[3] = status == RIG_POWER_OFF ? 0x00 : 0x01;

    /* Frequency set */
    return write_block(&rig->state.rigport, cmd, YAESU_CMD_LENGTH);
}


int frg100_set_vfo(RIG *rig, vfo_t vfo)
{
    unsigned char cmd[YAESU_CMD_LENGTH] = { 0x00, 0x00, 0x00, 0x00, 0x00};


    switch (vfo)
    {
    case RIG_VFO_CURR:
        return RIG_OK;

    case RIG_VFO_VFO:
    case RIG_VFO_A:
        cmd[4] = 0x05;
        break;

    case RIG_VFO_MEM:
        /* TODO: cmd[3] = priv->current_mem_chan; */
        cmd[4] = 0x02;
        break;

    default:
        return -RIG_EINVAL;     /* sorry, wrong VFO */
    }

    return write_block(&rig->state.rigport, cmd, YAESU_CMD_LENGTH);
}


int frg100_get_level(RIG *rig, vfo_t vfo, setting_t level, value_t *val)
{
    unsigned char cmd[YAESU_CMD_LENGTH] = { 0x00, 0x00, 0x00, 0x00, 0xf7};
    int retval;

    if (level != RIG_LEVEL_RAWSTR)
    {
        return -RIG_EINVAL;
    }

    rig_flush(&rig->state.rigport);

    /* send READ STATUS(Meter only) cmd to rig  */
    retval = write_block(&rig->state.rigport, cmd, YAESU_CMD_LENGTH);

    if (retval < 0)
    {
        return retval;
    }

    /* read back the 1 byte */
    retval = read_block(&rig->state.rigport, cmd, 5);

    if (retval < 1)
    {
        rig_debug(RIG_DEBUG_ERR, "%s: read meter failed %d\n",
                  __func__, retval);

        return retval < 0 ? retval : -RIG_EIO;
    }

    val->i = cmd[0];

    return RIG_OK;
}


#define MODE_LSB    0x00
#define MODE_USB    0x01
#define MODE_CWW    0x02
#define MODE_CWN    0x03
#define MODE_AMW    0x04
#define MODE_AMN    0x05
#define MODE_FMW    0x06
#define MODE_FMN    0x07


int mode2rig(RIG *rig, rmode_t mode, pbwidth_t width)
{
    int md;

    /*
     * translate mode from generic to frg100 specific
     */
    switch (mode)
    {
    case RIG_MODE_USB:    md = MODE_USB; break;

    case RIG_MODE_LSB:    md = MODE_LSB; break;

    case RIG_MODE_AM:
        if (width != RIG_PASSBAND_NOCHANGE
                && (width != RIG_PASSBAND_NORMAL ||
                    width < rig_passband_normal(rig, mode)))
        {
            md = MODE_AMN;
        }
        else
        {
            md = MODE_AMW;
        }

        break;

    case RIG_MODE_FM:
        if (width != RIG_PASSBAND_NOCHANGE
                && (width != RIG_PASSBAND_NORMAL ||
                    width < rig_passband_normal(rig, mode)))
        {
            md = MODE_FMN;
        }
        else
        {
            md = MODE_FMW;
        }

        break;

    case RIG_MODE_CW:
        if (width != RIG_PASSBAND_NOCHANGE
                && (width != RIG_PASSBAND_NORMAL ||
                    width < rig_passband_normal(rig, mode)))
        {
            md = MODE_CWN;
        }
        else
        {
            md = MODE_CWW;
        }

        break;

    default:
        return -RIG_EINVAL;         /* sorry, wrong MODE */
    }

    return md;
}

/* function not used */
#if 0
int rig2mode(RIG *rig, int md, rmode_t *mode, pbwidth_t *width)
{
    /*
     * translate mode from frg100 specific to generic
     */
    switch (md)
    {
    case MODE_USB:    *mode = RIG_MODE_USB; break;

    case MODE_LSB:    *mode = RIG_MODE_LSB; break;

    case MODE_AMW:
    case MODE_AMN:    *mode = RIG_MODE_AM; break;

    case MODE_FMW:
    case MODE_FMN:    *mode = RIG_MODE_FM; break;

    case MODE_CWW:
    case MODE_CWN:    *mode = RIG_MODE_CW; break;

    default:
        return -RIG_EINVAL;         /* sorry, wrong MODE */
    }

    if (md == MODE_CWN || md == MODE_AMN || md == MODE_FMN)
    {
        *width = rig_passband_narrow(rig, *mode);
    }
    else
    {
        *width = rig_passband_normal(rig, *mode);
    }

    return RIG_OK;
}
#endif
