/*
 *  Copyright (c) 2010-2011 by Mikhail Kshevetskiy (mikhail.kshevetskiy@gmail.com)
 *
 *  Code based on VX-1700 CAT manual:
 *  http://www.vertexstandard.com/downloadFile.cfm?FileID=3397&FileCatID=135&FileName=VX-1700_CAT_MANUAL_10_14_2008.pdf&FileContentType=application%2Fpdf
 *
 *  WARNING: this manual has two errors
 *    1) Status Update Command (10h), U=01 returns  0..199 for channels 1..200
 *    2) Frequency Data (bytes 1--4 of 9-Byte VFO Data Assignment, Status Update
 *       Command (10h), U=02 and U=03) uses bytes 1--3 for frequency, byte 4 is
 *       not used and always zero. Thus bytes 0x15,0xBE,0x68,0x00 means
 *       frequency = 10 * 0x15BE68 = 10 * 1425000 = 14.25 MHz
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
#include <stdio.h>
#include <string.h>

#include "hamlib/rig.h"
#include "bandplan.h"
#include "serial.h"
#include "misc.h"
#include "yaesu.h"
#include "vx1700.h"

// VX-1700 native commands
typedef enum vx1700_native_cmd_e
{
    VX1700_NATIVE_RECALL_MEM = 0,   /* 0x02, p1=ch */
    VX1700_NATIVE_VFO_TO_MEM,       /* 0x03, p1=ch, p2=0 */
    VX1700_NATIVE_MEM_HIDE,     /* 0x03, p1=ch, p2=1 */
    VX1700_NATIVE_VFO_A,        /* 0x05 */
    VX1700_NATIVE_FREQ_SET,     /* 0x0a, p1:4=freq */
    VX1700_NATIVE_MODE_SET_LSB,     /* 0x0c, p1=0x00 */
    VX1700_NATIVE_MODE_SET_USB,     /* 0x0c, p1=0x01 */
    VX1700_NATIVE_MODE_SET_CW_W,    /* 0x0c, p1=0x02 */
    VX1700_NATIVE_MODE_SET_CW_N,    /* 0x0c, p1=0x03 */
    VX1700_NATIVE_MODE_SET_AM,      /* 0x0c, p1=0x04 */
    VX1700_NATIVE_MODE_SET_RTTY_LSB_W,  /* 0x0c, p1=0x08 */
    VX1700_NATIVE_MODE_SET_RTTY_USB_W,  /* 0x0c, p1=0x09 */
    VX1700_NATIVE_MODE_SET_H3E,     /* 0x0c, p1=0x0d */
    VX1700_NATIVE_MODE_SET_RTTY_LSB_N,  /* 0x0c, p1=0x0e */
    VX1700_NATIVE_MODE_SET_RTTY_USB_N,  /* 0x0c, p1=0x0f */
    VX1700_NATIVE_PTT_OFF,      /* 0x0f, p1=0 */
    VX1700_NATIVE_PTT_ON,       /* 0x0f, p1=1 */
    VX1700_NATIVE_UPDATE_MEM_CHNL,  /* 0x10, p1=1 */
    VX1700_NATIVE_UPDATE_OP_DATA,   /* 0x10, p1=2 */
    VX1700_NATIVE_UPDATE_VFO_DATA,  /* 0x10, p1=3 */
    VX1700_NATIVE_TX_POWER_LOW,     /* 0x18 */
    VX1700_NATIVE_TX_POWER_MID,     /* 0x28 */
    VX1700_NATIVE_TX_POWER_HI,      /* 0x48 */
    VX1700_NATIVE_CPY_RX_TO_TX,     /* 0x85 */
    VX1700_NATIVE_TX_FREQ_SET,      /* 0x8a, p1:4=freq */
    VX1700_NATIVE_OP_FREQ_STEP_UP,  /* 0x8e, p1=0 */
    VX1700_NATIVE_OP_FREQ_STEP_DOWN,    /* 0x8e, p1=1 */
    VX1700_NATIVE_READ_METER,       /* 0xf7 */
    VX1700_NATIVE_READ_FLAGS,       /* 0xfa */
    VX1700_NATIVE_SIZE
} vx1700_native_cmd_t;

//  OpCode Declarations
#define VX1700_CMD_RECALLMEM        0x02
#define VX1700_CMD_VFO2MEM      0x03
#define VX1700_CMD_SEL_VFOA     0x05
#define VX1700_CMD_SET_VFOA     0x0a
#define VX1700_CMD_SEL_OP_MODE      0x0c
#define VX1700_CMD_PTT          0x0f
#define VX1700_CMD_UPDATE       0x10
#define VX1700_CMD_RX2TX        0x85
#define VX1700_CMD_STEP_VFO     0x8e
#define VX1700_CMD_RD_METER     0xf7
#define VX1700_CMD_RD_FLAGS     0xfa

// Return codes
#define VX1700_CMD_RETCODE_OK       0x00
#define VX1700_CMD_RETCODE_ERROR    0xF0

// Operating Mode Status
#define VX1700_MODE_LSB         0x00
#define VX1700_MODE_USB         0x01
#define VX1700_MODE_CW_W        0x02
#define VX1700_MODE_CW_N        0x03
#define VX1700_MODE_AM          0x04
#define VX1700_MODE_RTTY        0x05

// Operation Mode Selection
#define VX1700_OP_MODE_LSB      0x00
#define VX1700_OP_MODE_USB      0x01
#define VX1700_OP_MODE_CW_W     0x02
#define VX1700_OP_MODE_CW_N     0x03
#define VX1700_OP_MODE_AM       0x04
#define VX1700_OP_MODE_RTTY_LSB_W   0x08
#define VX1700_OP_MODE_RTTY_USB_W   0x09
#define VX1700_OP_MODE_H3E      0x0d
#define VX1700_OP_MODE_RTTY_LSB_N   0x0e
#define VX1700_OP_MODE_RTTY_USB_N   0x0f

// Status Flag 1 Masks
#define VX1700_SF_LOCKED        0x01    /* LOCK is activated */
#define VX1700_SF_MEM           0x20    /* Memory Mode       */
#define VX1700_SF_VFO           0x80    /* VFO Mode          */

// Status Flag 2 Masks
#define VX1700_SF_PTT_BY_CAT        0x01    /* PTT closed by CAT           */
#define VX1700_SF_MEM_SCAN_PAUSE    0x02    /* Scanning paused             */
#define VX1700_SF_MEM_SCAN      0x04    /* Scanning enabled            */
#define VX1700_SF_RTTY_FILTER_NARROW    0x08    /* Narrow RTTY filter selected */
#define VX1700_SF_CW_FILTER_NARROW  0x10    /* Narrow CW filter selected   */
#define VX1700_SF_RTTY_USB      0x20    /* USB selected for RTTY       */

// Status Flag 3 Masks
#define VX1700_SF_10W_TX        0x20    /* 10 Watt TX output selected */
#define VX1700_SF_TUNER_ON      0x20    /* Antenna Tuner working      */
#define VX1700_SF_TRANSMISSION_ON   0x80    /* Transmission in progress   */


/* HAMLIB API implementation */
static int          vx1700_init(RIG *rig);
static int          vx1700_open(RIG *rig);
static int          vx1700_cleanup(RIG *rig);
static const char *vx1700_get_info(RIG *rig);

static int vx1700_set_vfo(RIG *rig, vfo_t vfo);
static int vx1700_get_vfo(RIG *rig, vfo_t *vfo);
static int vx1700_set_freq(RIG *rig, vfo_t vfo, freq_t freq);
static int vx1700_get_freq(RIG *rig, vfo_t vfo, freq_t *freq);
static int vx1700_set_split_freq(RIG *rig, vfo_t vfo, freq_t tx_freq);
static int vx1700_get_split_freq(RIG *rig, vfo_t vfo, freq_t *tx_freq);
static int vx1700_set_mode(RIG *rig, vfo_t vfo, rmode_t mode, pbwidth_t width);
static int vx1700_get_mode(RIG *rig, vfo_t vfo, rmode_t *mode,
                           pbwidth_t *width);
static int vx1700_set_ptt(RIG *rig, vfo_t vfo, ptt_t ptt);
static int vx1700_get_ptt(RIG *rig, vfo_t vfo, ptt_t *ptt);
static int vx1700_set_func(RIG *rig, vfo_t vfo, setting_t func, int status);
static int vx1700_get_func(RIG *rig, vfo_t vfo, setting_t func, int *status);
static int vx1700_set_level(RIG *rig, vfo_t vfo, setting_t level, value_t val);
static int vx1700_get_level(RIG *rig, vfo_t vfo, setting_t level, value_t *val);
static int vx1700_set_mem(RIG *rig, vfo_t vfo, int ch);
static int vx1700_get_mem(RIG *rig, vfo_t vfo, int *ch);
static int vx1700_vfo_op(RIG *rig, vfo_t vfo, vfo_op_t op);

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


/*
 * Private data
 */
struct vx1700_priv_data
{
    unsigned char   ch; /* memory channel */
};


/*
 * vx1700 rigs capabilities.
 */
#define VX1700_MEM_CAP {    \
        .freq    = 1,   \
        .tx_freq = 1,   \
        .mode    = 1,   \
        .width   = 1,   \
}

const struct rig_caps vx1700_caps =
{
    RIG_MODEL(RIG_MODEL_VX1700),
    .model_name =  "VX-1700",
    .mfg_name =  "Vertex Standard",
    .version =  "20210221.0",
    .copyright =  "LGPL",
    .status =   RIG_STATUS_STABLE,
    .rig_type =  RIG_TYPE_TRANSCEIVER,
    .ptt_type =  RIG_PTT_RIG_MICDATA,
    .dcd_type =  RIG_DCD_NONE,  /* we have DCD pin in DATA Jack, but get_dcd() is unavailable (yet?) */
    .port_type =  RIG_PORT_SERIAL,
    .serial_rate_min =  4800,
    .serial_rate_max =  4800,
    .serial_data_bits =  8,
    .serial_stop_bits =  2,
    .serial_parity =  RIG_PARITY_NONE,
    .serial_handshake =  RIG_HANDSHAKE_NONE,
    .write_delay =  0,
    .post_write_delay =  0,
    .timeout =  600,
    .retry =  3,

    .has_get_func = (RIG_FUNC_LOCK | RIG_FUNC_TUNER),
    .has_set_func =  RIG_FUNC_NONE,
    .has_get_level =  RIG_LEVEL_BAND_SELECT,
    .has_set_level =  RIG_LEVEL_RFPOWER,
    .has_get_parm =  RIG_PARM_NONE,
    .has_set_parm =  RIG_PARM_NONE,
    .level_gran =  {
        // cppcheck-suppress *
        [LVL_RFPOWER] = { .min = { .i = 0 }, .max = { .i = 2 } },
    },
    .parm_gran =  {},
    .preamp =   { RIG_DBLST_END },
    .attenuator =   { RIG_DBLST_END },
    .max_rit =  Hz(0),
    .max_xit =  Hz(0),
    .max_ifshift =  Hz(0),
    .vfo_ops =  VX1700_VFO_OPS,
    .targetable_vfo =  0,
    .transceive =  RIG_TRN_OFF,
    .bank_qty =   0,
    .chan_desc_sz =  0,

    .chan_list =  { { VX1700_MIN_CHANNEL, VX1700_MAX_CHANNEL, RIG_MTYPE_MEM, VX1700_MEM_CAP }, RIG_CHAN_END },

    .rx_range_list1 =  {
        { kHz(30),   MHz(30),  VX1700_MODES, -1,    -1,     VX1700_VFO_ALL, VX1700_ANTS },
        RIG_FRNG_END,
    },
    .tx_range_list1 =  {
        { kHz(1600), MHz(4) - 1, VX1700_MODES, W(31), W(125), VX1700_VFO_ALL, VX1700_ANTS },
        { kHz(4000), MHz(30),  VX1700_MODES, W(25), W(100), VX1700_VFO_ALL, VX1700_ANTS },
        RIG_FRNG_END,
    },

    .rx_range_list2 =  {
        { kHz(30),   MHz(30),  VX1700_MODES, -1,    -1,     VX1700_VFO_ALL, VX1700_ANTS },
        RIG_FRNG_END,
    },
    .tx_range_list2 =  {
        { kHz(1600), MHz(4) - 1, VX1700_MODES, W(31), W(125), VX1700_VFO_ALL, VX1700_ANTS },
        { kHz(4000), MHz(30),  VX1700_MODES, W(25), W(100), VX1700_VFO_ALL, VX1700_ANTS },
        RIG_FRNG_END,
    },

    .tuning_steps =  {
        { VX1700_MODES, 100 },
        RIG_TS_END,
    },

    /* mode/filter list, remember: order matters! */
    .filters =  {
        { RIG_MODE_AM,                                  VX1700_FILTER_WIDTH_AM     },
        { RIG_MODE_SSB,                                 VX1700_FILTER_WIDTH_SSB    },
        { RIG_MODE_CW | RIG_MODE_RTTY | RIG_MODE_RTTYR, VX1700_FILTER_WIDTH_WIDE   },
        { RIG_MODE_CW | RIG_MODE_RTTY | RIG_MODE_RTTYR, VX1700_FILTER_WIDTH_NARROW },
        RIG_FLT_END,
    },

    .priv =  NULL,

    .rig_init       = vx1700_init,
    .rig_open       = vx1700_open,
    .rig_cleanup    = vx1700_cleanup,

    .set_vfo        = vx1700_set_vfo,
    .get_vfo        = vx1700_get_vfo,
    .set_freq       = vx1700_set_freq,
    .get_freq       = vx1700_get_freq,
    .set_split_freq = vx1700_set_split_freq,
    .get_split_freq = vx1700_get_split_freq,
    .set_mode       = vx1700_set_mode,
    .get_mode       = vx1700_get_mode,
    .get_ptt        = vx1700_get_ptt,
    .set_ptt        = vx1700_set_ptt,
    .get_info       = vx1700_get_info,
    .set_func       = vx1700_set_func,
    .get_func       = vx1700_get_func,
    .set_level      = vx1700_set_level,
    .get_level      = vx1700_get_level,
    .set_mem        = vx1700_set_mem,
    .get_mem        = vx1700_get_mem,
    .vfo_op         = vx1700_vfo_op,
    .hamlib_check_rig_caps = HAMLIB_CHECK_RIG_CAPS
};


static inline freq_t vx1700_read_freq_from_buf(const unsigned char p[])
{
    /* WARNING: documentation bug, actually frequency stored in bytes 0..2 only,
                byte 3 is not used and contain zero */
    return ((((((unsigned)p[0]) << 8) + p[1]) << 8) + p[2]) * 10.0;
}

static inline int vx1700_channel_is_ok(unsigned char channel)
{
    return ((channel >= VX1700_MIN_CHANNEL) &&
            (channel <= VX1700_MAX_CHANNEL)) ? 1 : 0;
}


/*
 * Function definitions below
 */
static int vx1700_do_transaction(RIG *rig,
                                 const unsigned char cmd[YAESU_CMD_LENGTH],
                                 unsigned char *retbuf, size_t retbuf_len)
{
    struct rig_state    *rs;
    unsigned char   default_retbuf[1];
    int         retval;

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
        if ((retval == 1) && (retbuf[0] == VX1700_CMD_RETCODE_ERROR)) { return -RIG_ERJCTED; }

        return -RIG_EIO;
    }

    if (retval == 1)
    {
        if ((cmd[4] == VX1700_CMD_UPDATE) && (cmd[3] == 0x01))
        {
            /* read memory channel number */
            if (vx1700_channel_is_ok(retbuf[0] + 1))
            {
                /* WARNING: Documentation bug, actually
                            we got 0--199 for channels 1--200 */
                return RIG_OK;
            }

            if (retbuf[0] == VX1700_CMD_RETCODE_ERROR) { return -RIG_ERJCTED; }

            return -RIG_EIO;
        }

        if (retbuf[0] == VX1700_CMD_RETCODE_OK) { return RIG_OK; }

        if (retbuf[0] == VX1700_CMD_RETCODE_ERROR) { return -RIG_ERJCTED; }

        return -RIG_EIO;
    }

    return RIG_OK;
}

/*
 * Private helper function to send a complete command sequence.
 *
 * TODO: place variant of this in yaesu.c
 *
 * Arguments:   *rig    Valid RIG instance
 *              ci      Command index of the ncmd table
 *
 * Returns:     RIG_OK if all called functions are successful,
 *              otherwise returns error from called functiion
 */
static int vx1700_do_static_cmd(RIG *rig, unsigned char ci)
{
    if (rig == NULL) { return -RIG_EINVAL; }

    if (! ncmd[ci].ncomp)
    {
        rig_debug(RIG_DEBUG_TRACE,
                  "%s: Attempt to send incomplete sequence\n", __func__);
        return -RIG_EINVAL;
    }

    return vx1700_do_transaction(rig, ncmd[ci].nseq, NULL, 0);
}

/*
 * Private helper function to build and then send a complete command
 * sequence.
 *
 * TODO: place variant of this in yaesu.c
 *
 * Arguments:   *rig    Valid RIG instance
 *              ci      Command index of the cmd struct
 *              p1-p4   Command parameters
 *
 * Returns:     RIG_OK if all called functions are successful,
 *              otherwise returns error from called functiion
 */
static int vx1700_do_dynamic_cmd(RIG *rig, unsigned char ci,
                                 unsigned char p1, unsigned char p2,
                                 unsigned char p3, unsigned char p4)
{

    unsigned char   cmd[YAESU_CMD_LENGTH];

    if (rig == NULL) { return -RIG_EINVAL; }

    if (ncmd[ci].ncomp)
    {
        rig_debug(RIG_DEBUG_TRACE,
                  "%s: Attempt to modify complete sequence\n", __func__);
        return -RIG_EINVAL;
    }

    memcpy(&cmd, &ncmd[ci].nseq, YAESU_CMD_LENGTH);
    cmd[3] = p1;
    cmd[2] = p2;
    cmd[1] = p3;
    cmd[0] = p4;
    return vx1700_do_transaction(rig, cmd, NULL, 0);
}

static int vx1700_do_freq_cmd(RIG *rig, unsigned char ci, freq_t freq)
{
    unsigned char   cmd[YAESU_CMD_LENGTH];

    if (rig == NULL) { return -RIG_EINVAL; }

    if ((ci != VX1700_NATIVE_FREQ_SET) && (ci != VX1700_NATIVE_TX_FREQ_SET))
    {
        rig_debug(RIG_DEBUG_TRACE,
                  "%s: Attempt to use non frequency sequence\n", __func__);
        return -RIG_EINVAL;
    }

    memcpy(&cmd, &ncmd[ci].nseq, YAESU_CMD_LENGTH);
    to_bcd(cmd, freq / 10, VX1700_BCD_DIAL);
    return vx1700_do_transaction(rig, cmd, NULL, 0);
}

static inline int vx1700_read_mem_channel_number(RIG *rig,
        unsigned char *channel)
{
    int         ret;
    unsigned char   reply[VX1700_MEM_CHNL_LENGTH];

    ret = vx1700_do_transaction(rig, ncmd[VX1700_NATIVE_UPDATE_MEM_CHNL].nseq,
                                reply, VX1700_MEM_CHNL_LENGTH);

    if (ret == -RIG_ERJCTED)
    {
        /* we are on VFO mode, so memory channel is not available at the moment */
        *channel = VX1700_MIN_CHANNEL - 1;
        return RIG_OK;
    }

    if (ret != RIG_OK) { return ret; }

    /* WARNING: Documentation bug, actually we got 0--199 for channels 1--200 */
    *channel = VX1700_MIN_CHANNEL + reply[0];
    return RIG_OK;
}

static inline int vx1700_read_status_flags(RIG *rig, unsigned char reply[])
{
    if (rig == NULL) { return -RIG_EINVAL; }

    return vx1700_do_transaction(rig, ncmd[VX1700_NATIVE_READ_FLAGS].nseq,
                                 reply, VX1700_STATUS_FLAGS_LENGTH);
}

static inline int vx1700_read_meter(RIG *rig, unsigned char reply[])
{
    if (rig == NULL) { return -RIG_EINVAL; }

    return vx1700_do_transaction(rig, ncmd[VX1700_NATIVE_READ_METER].nseq,
                                 reply, VX1700_READ_METER_LENGTH);
}

static inline int vx1700_read_vfo_data_raw(RIG *rig, unsigned char reply[])
{
    if (rig == NULL) { return -RIG_EINVAL; }

    return vx1700_do_transaction(rig, ncmd[VX1700_NATIVE_UPDATE_VFO_DATA].nseq,
                                 reply, VX1700_VFO_DATA_LENGTH);
}

static inline int vx1700_read_op_data_raw(RIG *rig, unsigned char reply[])
{
    if (rig == NULL) { return -RIG_EINVAL; }

    return vx1700_do_transaction(rig, ncmd[VX1700_NATIVE_UPDATE_OP_DATA].nseq,
                                 reply, VX1700_OP_DATA_LENGTH);
}

#if 0 /* unused; re-enabled as needed. */
static int vx1700_read_vfo_data(RIG *rig, unsigned char *hwmode,
                                freq_t *rx_freq, freq_t *tx_freq)
{
    int         ret;
    unsigned char   reply[VX1700_VFO_DATA_LENGTH];

    if ((ret = vx1700_read_vfo_data_raw(rig, reply)) != RIG_OK) { return ret; }

    if (hwmode  != NULL) { *hwmode  = reply[6]; }

    if (rx_freq != NULL) { *rx_freq = vx1700_read_freq_from_buf(reply + 1); }

    if (tx_freq != NULL) { *tx_freq = vx1700_read_freq_from_buf(reply + 10); }

    return RIG_OK;
}
#endif /* unused */

static int vx1700_read_op_data(RIG *rig, unsigned char *hwmode,
                               freq_t *rx_freq, freq_t *tx_freq)
{
    int         ret;
    unsigned char   reply[VX1700_OP_DATA_LENGTH];

    if ((ret = vx1700_read_op_data_raw(rig, reply)) != RIG_OK) { return ret; }

    if (hwmode  != NULL) { *hwmode  = reply[7]; }

    if (rx_freq != NULL) { *rx_freq = vx1700_read_freq_from_buf(reply + 2); }

    if (tx_freq != NULL) { *tx_freq = vx1700_read_freq_from_buf(reply + 11); }

    return RIG_OK;
}

static const char *vx1700_get_hwmode_str(unsigned char hwmode)
{
    switch (hwmode)
    {
    case VX1700_MODE_AM:
        return "AM (A3E)";

    case VX1700_MODE_LSB:
        return "LSB (J3E)";

    case VX1700_MODE_USB:
        return "USB (J3E)";

    case VX1700_MODE_CW_W:
        return "CW (A1A-W)";

    case VX1700_MODE_CW_N:
        return "CW (A1A-N)";

    case VX1700_MODE_RTTY:
        return "RTTY[R] (J2B)";

    default:
        return "unknown";
    }
}

static void vx1700_parse_vfo_data(const char *func,
                                  const unsigned char buf[VX1700_VFO_DATA_LENGTH])
{
    rig_debug(RIG_DEBUG_TRACE, "%s: vfo-data: rx.band_data=0x%02d\n", func, buf[0]);
    rig_debug(RIG_DEBUG_TRACE, "%s: vfo-data: rx.freq=%f\n",          func,
              vx1700_read_freq_from_buf(buf + 1));
    rig_debug(RIG_DEBUG_TRACE, "%s: vfo-data: rx.mode=0x%02d, %s\n",  func, buf[6],
              vx1700_get_hwmode_str(buf[6]));
    rig_debug(RIG_DEBUG_TRACE, "%s: vfo-data: tx.band_data=0x%02d\n", func, buf[9]);
    rig_debug(RIG_DEBUG_TRACE, "%s: vfo-data: tx.freq=%f\n",          func,
              vx1700_read_freq_from_buf(buf + 10));
    rig_debug(RIG_DEBUG_TRACE, "%s: vfo-data: tx.mode=0x%02d, %s\n",  func, buf[15],
              vx1700_get_hwmode_str(buf[15]));
}

static void vx1700_parse_op_data(const char *func,
                                 const unsigned char buf[VX1700_OP_DATA_LENGTH])
{
    rig_debug(RIG_DEBUG_TRACE, "%s: op-data: Semi Duplex Memory Channel: %s\n",
              func, (buf[0] & 0x20) ? "yes" : "no");
    rig_debug(RIG_DEBUG_TRACE, "%s: op-data: Alpha Numeric Channel: %s\n",
              func, (buf[0] & 0x40) ? "yes" : "no");
    rig_debug(RIG_DEBUG_TRACE, "%s: op-data: Erased Memory Channel: %s\n",
              func, (buf[0] & 0x80) ? "yes" : "no");
    rig_debug(RIG_DEBUG_TRACE, "%s: op-data: rx.band_data=0x%02d\n",
              func, buf[1]);
    rig_debug(RIG_DEBUG_TRACE, "%s: op-data: rx.freq=%f\n",
              func, vx1700_read_freq_from_buf(buf + 2));
    rig_debug(RIG_DEBUG_TRACE, "%s: op-data: rx.mode=0x%02d, %s\n",
              func, buf[7], vx1700_get_hwmode_str(buf[7]));
    rig_debug(RIG_DEBUG_TRACE, "%s: op-data: tx.band_data=0x%02d\n",
              func, buf[10]);
    rig_debug(RIG_DEBUG_TRACE, "%s: op-data: tx.freq=%f\n",
              func, vx1700_read_freq_from_buf(buf + 11));
    rig_debug(RIG_DEBUG_TRACE, "%s: op-data: tx.mode=0x%02d, %s\n",
              func, buf[16], vx1700_get_hwmode_str(buf[16]));
}

static void vx1700_parse_status_flags(const char *func,
                                      const unsigned char buf[VX1700_STATUS_FLAGS_LENGTH])
{
    rig_debug(RIG_DEBUG_TRACE, "%s: flags: Lock: %s\n",               func,
              (buf[0] & VX1700_SF_LOCKED) ? "yes" : "no");
    rig_debug(RIG_DEBUG_TRACE, "%s: flags: Memory Mode: %s\n",        func,
              (buf[0] & VX1700_SF_MEM) ? "yes" : "no");
    rig_debug(RIG_DEBUG_TRACE, "%s: flags: VFO Mode: %s\n",           func,
              (buf[0] & VX1700_SF_VFO) ? "yes" : "no");
    rig_debug(RIG_DEBUG_TRACE, "%s: flags: PTT closed by CAT: %s\n",  func,
              (buf[1] & VX1700_SF_PTT_BY_CAT) ? "yes" : "no");
    rig_debug(RIG_DEBUG_TRACE, "%s: flags: Scanning paused: %s\n",    func,
              (buf[1] & VX1700_SF_MEM_SCAN_PAUSE) ? "yes" : "no");
    rig_debug(RIG_DEBUG_TRACE, "%s: flags: Scanning enabled: %s\n",   func,
              (buf[1] & VX1700_SF_MEM_SCAN) ? "yes" : "no");
    rig_debug(RIG_DEBUG_TRACE, "%s: flags: Narrow RTTY filter: %s\n", func,
              (buf[1] & VX1700_SF_RTTY_FILTER_NARROW) ? "yes" : "no");
    rig_debug(RIG_DEBUG_TRACE, "%s: flags: Narrow CW filter: %s\n",   func,
              (buf[1] & VX1700_SF_CW_FILTER_NARROW) ? "yes" : "no");
    rig_debug(RIG_DEBUG_TRACE, "%s: flags: USB for RTTY: %s\n",       func,
              (buf[1] & VX1700_SF_RTTY_USB) ? "yes" : "no");
    rig_debug(RIG_DEBUG_TRACE, "%s: flags: 10 Watt TX output: %s\n",  func,
              (buf[2] & VX1700_SF_10W_TX) ? "yes" : "no");
    rig_debug(RIG_DEBUG_TRACE, "%s: flags: Antenna Tuner: %s\n",      func,
              (buf[2] & VX1700_SF_TUNER_ON) ? "on" : "off");
    rig_debug(RIG_DEBUG_TRACE, "%s: flags: Transmission: %s\n",       func,
              (buf[2] & VX1700_SF_TRANSMISSION_ON) ? "yes" : "no");
    rig_debug(RIG_DEBUG_TRACE,
              "%s: flags: end bytes (0x06, 0x04): 0x%02x, 0x%02x\n", func, buf[3], buf[4]);
}

static void vx1700_parse_meter(const char *func,
                               const unsigned char buf[VX1700_READ_METER_LENGTH])
{
    rig_debug(RIG_DEBUG_TRACE, "%s: meter: data: 0x%02x, 0x%02x, 0x%02x, 0x%02x\n",
              __func__, buf[0], buf[1], buf[2], buf[3]);
    rig_debug(RIG_DEBUG_TRACE, "%s: meter: end byte (0xF7): 0x%02x\n", __func__,
              buf[4]);
}

static void dump_radio_state(RIG *rig)
{
    unsigned char   channel = 0;
    unsigned char   reply[VX1700_OP_DATA_LENGTH];

    if (rig == NULL) { return; }

    if (vx1700_read_mem_channel_number(rig, &channel) != RIG_OK) { return; }

    if (vx1700_channel_is_ok(channel))
    {
        rig_debug(RIG_DEBUG_TRACE, "%s: Current Memory Channel %d\n", __func__,
                  (int)channel);
    }
    else
    {
        rig_debug(RIG_DEBUG_TRACE,
                  "%s: Memory Channel number is not available at the moment\n", __func__);
    }

    if (vx1700_read_op_data_raw(rig, reply) != RIG_OK) { return; }

    vx1700_parse_op_data(__func__, reply);

    if (vx1700_read_vfo_data_raw(rig, reply) != RIG_OK) { return; }

    vx1700_parse_vfo_data(__func__, reply);

    if (vx1700_read_status_flags(rig, reply) != RIG_OK) { return; }

    vx1700_parse_status_flags(__func__, reply);

    if (vx1700_read_meter(rig, reply) != RIG_OK) { return; }

    vx1700_parse_meter(__func__, reply);
}

static int vx1700_init(RIG *rig)
{
    struct vx1700_priv_data *priv;

    rig_debug(RIG_DEBUG_TRACE, "%s\n", __func__);

    rig->state.priv = calloc(1, sizeof(struct vx1700_priv_data));

    if (rig->state.priv == NULL) { return -RIG_ENOMEM; }

    priv = rig->state.priv;

    priv->ch = 1;
    return RIG_OK;
}

static int vx1700_open(RIG *rig)
{
    struct vx1700_priv_data *priv = (struct vx1700_priv_data *)rig->state.priv;
    struct rig_state        *state = &rig->state;
    int             ret;

    rig_debug(RIG_DEBUG_TRACE, "%s\n", __func__);

    if ((ret = vx1700_get_vfo(rig, &state->current_vfo)) != RIG_OK) { return ret; }

    if ((ret = vx1700_get_mode(rig, RIG_VFO_CURR, &state->current_mode,
                               &state->current_width)) != RIG_OK) { return ret; }

    if ((ret = vx1700_read_op_data(rig, NULL, &state->current_freq,
                                   NULL)) != RIG_OK) { return ret; }

    if ((ret = vx1700_read_mem_channel_number(rig, &priv->ch)) != RIG_OK) { return ret; }

    return RIG_OK;
}

static int vx1700_cleanup(RIG *rig)
{
    rig_debug(RIG_DEBUG_TRACE, "%s\n", __func__);

    if (rig->state.priv != NULL) { free(rig->state.priv); }

    rig->state.priv = NULL;
    return RIG_OK;
}

static const char *vx1700_get_info(RIG *rig)
{
    rig_debug(RIG_DEBUG_TRACE, "%s\n", __func__);
    dump_radio_state(rig);
    return "NO_INFO";
}

static int vx1700_set_vfo(RIG *rig, vfo_t vfo)
{
    struct vx1700_priv_data *priv = (struct vx1700_priv_data *)rig->state.priv;

    rig_debug(RIG_DEBUG_TRACE, "%s, vfo=%s\n", __func__, rig_strvfo(vfo));

    switch (vfo)
    {
    case RIG_VFO_CURR:
        return RIG_OK;

    case RIG_VFO_VFO:
    case RIG_VFO_A:
        return vx1700_do_static_cmd(rig, VX1700_NATIVE_VFO_A);

    case RIG_VFO_MEM:
        return vx1700_do_dynamic_cmd(rig, VX1700_NATIVE_RECALL_MEM, priv->ch, 0, 0, 0);

    default:
        return -RIG_EINVAL;
    }
}

static int vx1700_get_vfo(RIG *rig, vfo_t *vfo)
{
    int         ret;
    unsigned char   reply[VX1700_STATUS_FLAGS_LENGTH];

    rig_debug(RIG_DEBUG_TRACE, "%s\n", __func__);

    if ((ret = vx1700_read_status_flags(rig, reply)) != RIG_OK) { return ret; }

    *vfo = (reply[0] & VX1700_SF_MEM) ? RIG_VFO_MEM : RIG_VFO_A;
    return RIG_OK;
}

static int vx1700_set_freq(RIG *rig, vfo_t vfo, freq_t freq)
{
    (void) vfo;
    rig_debug(RIG_DEBUG_TRACE, "%s: freq=%f\n", __func__, freq);
    return vx1700_do_freq_cmd(rig, VX1700_NATIVE_FREQ_SET, freq);
}

static int vx1700_get_freq(RIG *rig, vfo_t vfo, freq_t *freq)
{
    (void) vfo;
    return vx1700_read_op_data(rig, NULL, freq, NULL);
}

static int vx1700_set_split_freq(RIG *rig, vfo_t vfo, freq_t tx_freq)
{
    (void) vfo;
    rig_debug(RIG_DEBUG_TRACE, "%s: freq=%f\n", __func__, tx_freq);
    int err = rig_set_split_vfo(rig, RIG_VFO_A, RIG_SPLIT_ON, RIG_VFO_B);

    if (err != RIG_OK) { RETURNFUNC(err); }

    return vx1700_do_freq_cmd(rig, VX1700_NATIVE_TX_FREQ_SET, tx_freq);
}

static int vx1700_get_split_freq(RIG *rig, vfo_t vfo, freq_t *tx_freq)
{
    (void) vfo;
    rig_debug(RIG_DEBUG_TRACE, "%s\n", __func__);
    return vx1700_read_op_data(rig, NULL, NULL, tx_freq);
}

static int vx1700_set_mode(RIG *rig, vfo_t vfo, rmode_t mode, pbwidth_t width)
{
    (void) rig;
    (void) vfo;
    rig_debug(RIG_DEBUG_TRACE, "%s: mode=0x%04x, width=%d\n", __func__, (int) mode,
              (int) width);

    if (width == RIG_PASSBAND_NORMAL)
    {
        width = rig_passband_normal(rig, mode);
    }

    switch (mode)
    {
    case RIG_MODE_AM:
        return vx1700_do_static_cmd(rig, VX1700_NATIVE_MODE_SET_AM);

    case RIG_MODE_LSB:
        return vx1700_do_static_cmd(rig, VX1700_NATIVE_MODE_SET_LSB);

    case RIG_MODE_USB:
        return vx1700_do_static_cmd(rig, VX1700_NATIVE_MODE_SET_USB);

    case RIG_MODE_CW:
        if (width > (VX1700_FILTER_WIDTH_NARROW + VX1700_FILTER_WIDTH_WIDE) / 2)
        {
            return vx1700_do_static_cmd(rig, VX1700_NATIVE_MODE_SET_CW_W);
        }
        else
        {
            return vx1700_do_static_cmd(rig, VX1700_NATIVE_MODE_SET_CW_N);
        }

    case RIG_MODE_RTTY:
        if (width > (VX1700_FILTER_WIDTH_NARROW + VX1700_FILTER_WIDTH_WIDE) / 2)
        {
            return vx1700_do_static_cmd(rig, VX1700_NATIVE_MODE_SET_RTTY_LSB_W);
        }
        else
        {
            return vx1700_do_static_cmd(rig, VX1700_NATIVE_MODE_SET_RTTY_LSB_N);
        }

    case RIG_MODE_RTTYR:
        if (width > (VX1700_FILTER_WIDTH_NARROW + VX1700_FILTER_WIDTH_WIDE) / 2)
        {
            return vx1700_do_static_cmd(rig, VX1700_NATIVE_MODE_SET_RTTY_USB_W);
        }
        else
        {
            return vx1700_do_static_cmd(rig, VX1700_NATIVE_MODE_SET_RTTY_USB_N);
        }

    default:
        return -RIG_EINVAL;
    }
}

static int vx1700_get_mode(RIG *rig, vfo_t vfo, rmode_t *mode, pbwidth_t *width)
{
    int         ret;
    unsigned char   hwmode;
    unsigned char   reply[VX1700_STATUS_FLAGS_LENGTH];

    (void) rig;
    (void) vfo;
    rig_debug(RIG_DEBUG_TRACE, "%s\n", __func__);

    if ((ret = vx1700_read_op_data(rig, &hwmode, NULL, NULL)) != RIG_OK) { return ret; }

    switch (hwmode)
    {
    case VX1700_MODE_AM:
        *mode  = RIG_MODE_AM;
        *width = VX1700_FILTER_WIDTH_AM;
        return RIG_OK;

    case VX1700_MODE_LSB:
        *mode  = RIG_MODE_LSB;
        *width = VX1700_FILTER_WIDTH_SSB;
        return RIG_OK;

    case VX1700_MODE_USB:
        *mode  = RIG_MODE_USB;
        *width = VX1700_FILTER_WIDTH_SSB;
        return RIG_OK;

    case VX1700_MODE_CW_W:
        *mode  = RIG_MODE_CW;
        *width = VX1700_FILTER_WIDTH_WIDE;
        return RIG_OK;

    case VX1700_MODE_CW_N:
        *mode  = RIG_MODE_CW;
        *width = VX1700_FILTER_WIDTH_NARROW;
        return RIG_OK;

    case VX1700_MODE_RTTY:
        if ((ret = vx1700_read_status_flags(rig, reply)) != RIG_OK) { return ret; }

        *mode  = (reply[1] & VX1700_SF_RTTY_USB) ?
                 RIG_MODE_RTTYR : RIG_MODE_RTTY;
        *width = (reply[1] & VX1700_SF_RTTY_FILTER_NARROW) ?
                 VX1700_FILTER_WIDTH_NARROW : VX1700_FILTER_WIDTH_WIDE;
        return RIG_OK;

    default:
        return -RIG_EPROTO;
    }
}

static int vx1700_set_ptt_gps_jack(ptt_t ptt)
{
    (void) ptt;

    /*
     * FIXME
     *
     * We are using GPIO to manage PTT pin in GPS/Data jack.
     * This highly binded to our specific device, so it makes
     * no sense to put our code here.
     * On regular PC this should be managed in another way,
     * probably via DTR/RTS.
     */
    return -RIG_EINVAL;
}

static int vx1700_set_ptt(RIG *rig, vfo_t vfo, ptt_t ptt)
{
    rmode_t     mode;
    pbwidth_t       width;
    int         ret;
    unsigned char   reply[VX1700_STATUS_FLAGS_LENGTH];

    rig_debug(RIG_DEBUG_TRACE, "%s, ptt=%d\n", __func__, ptt);

    /*
     * We have 3 PTT source on Vertex Standard VX-1700:
     *   1) ptt on radio garniture (not checked, FIXME)
     *   2) PTT commands inside CAT operation protocol
     *      - select radio garniture as audio input/output source
     *      - does not work in RTTY/RTTYR modes
     *   3) PTT pin in GPS/Data jack
     *      - select GPS/Data jack as input/output source
     *      - does not work in CW/AM modes
     */
    if ((ret = vx1700_get_mode(rig, vfo, &mode, &width)) != RIG_OK) { return ret; }

    switch (mode)
    {
    case RIG_MODE_AM:
    case RIG_MODE_CW:
        switch (ptt)
        {
        case RIG_PTT_ON:
        case RIG_PTT_ON_MIC:
            return vx1700_do_static_cmd(rig, VX1700_NATIVE_PTT_ON);

        case RIG_PTT_OFF:
            return vx1700_do_static_cmd(rig, VX1700_NATIVE_PTT_OFF);

        default:
            return -RIG_EINVAL;
        }

    case RIG_MODE_LSB:
    case RIG_MODE_USB:
        switch (ptt)
        {
        case RIG_PTT_ON:
        case RIG_PTT_ON_MIC:
            return vx1700_do_static_cmd(rig, VX1700_NATIVE_PTT_ON);

        case RIG_PTT_ON_DATA:
            return vx1700_set_ptt_gps_jack(RIG_PTT_ON);

        case RIG_PTT_OFF:
            if ((ret = vx1700_read_status_flags(rig, reply)) != RIG_OK) { return ret; }

            if (reply[1] & VX1700_SF_PTT_BY_CAT)
            {
                /* PTT was turned on by CAT command, turn it off accordingly */
                return vx1700_do_static_cmd(rig, VX1700_NATIVE_PTT_OFF);
            }

            /* PTT was turned on via special pin on GPS/DATA jack */
            return vx1700_set_ptt_gps_jack(RIG_PTT_OFF);

        default:
            return -RIG_EINVAL;
        }

    case RIG_MODE_RTTY:
    case RIG_MODE_RTTYR:
        switch (ptt)
        {
        case RIG_PTT_ON:
        case RIG_PTT_ON_DATA:
            return vx1700_set_ptt_gps_jack(RIG_PTT_ON);

        case RIG_PTT_OFF:
            return vx1700_set_ptt_gps_jack(RIG_PTT_OFF);

        default:
            return -RIG_EINVAL;
        }

    default:
        return -RIG_EINVAL;
    }
}

static int vx1700_get_ptt(RIG *rig, vfo_t vfo, ptt_t *ptt)
{
    int         ret;
    unsigned char   reply[VX1700_STATUS_FLAGS_LENGTH];

    rig_debug(RIG_DEBUG_TRACE, "%s\n", __func__);

    if ((ret = vx1700_read_status_flags(rig, reply)) != RIG_OK) { return ret; }

    *ptt = (reply[2] & VX1700_SF_TRANSMISSION_ON) ? RIG_PTT_ON : RIG_PTT_OFF;
    return RIG_OK;
}

static int vx1700_set_func(RIG *rig, vfo_t vfo, setting_t func, int status)
{
    (void) rig;
    (void) vfo;
    rig_debug(RIG_DEBUG_TRACE, "%s: func=%s, status=%d\n", __func__,
              rig_strfunc(func), status);
    return -RIG_EINVAL;
}

static int vx1700_get_func(RIG *rig, vfo_t vfo, setting_t func, int *status)
{
    int         ret;
    unsigned char   reply[VX1700_STATUS_FLAGS_LENGTH];

    (void) rig;
    (void) vfo;
    rig_debug(RIG_DEBUG_TRACE, "%s: func=%s\n", __func__, rig_strfunc(func));

    switch (func)
    {
    case RIG_FUNC_LOCK:
        if ((ret = vx1700_read_status_flags(rig, reply)) != RIG_OK) { return ret; }

        *status = (reply[0] & VX1700_SF_LOCKED) ? 1 : 0;
        return RIG_OK;

    case RIG_FUNC_TUNER:
        if ((ret = vx1700_read_status_flags(rig, reply)) != RIG_OK) { return ret; }

        *status = (reply[2] & VX1700_SF_TUNER_ON) ? 1 : 0;
        return RIG_OK;

    default:
        return -RIG_EINVAL;
    }
}

static int vx1700_set_level(RIG *rig, vfo_t vfo, setting_t level, value_t val)
{
    (void) rig;
    (void) vfo;
    rig_debug(RIG_DEBUG_TRACE, "%s: level=%s, val=???\n", __func__,
              rig_strlevel(level));

    switch (level)
    {
    case RIG_LEVEL_RFPOWER:
        if ((val.f < 0.0) || (val.f > 1.0)) { return -RIG_EINVAL; }

        if (val.f < (1.0 / 3.0))
        {
            return vx1700_do_static_cmd(rig, VX1700_NATIVE_TX_POWER_LOW);
        }

        if (val.f < (2.0 / 3.0))
        {
            return vx1700_do_static_cmd(rig, VX1700_NATIVE_TX_POWER_MID);
        }

        return vx1700_do_static_cmd(rig, VX1700_NATIVE_TX_POWER_HI);

    default:
        return -RIG_EINVAL;
    }
}

static int vx1700_get_level(RIG *rig, vfo_t vfo, setting_t level, value_t *val)
{
    (void) rig;
    (void) vfo;
    rig_debug(RIG_DEBUG_TRACE, "%s: level=%s\n", __func__, rig_strlevel(level));
    return -RIG_EINVAL;
}

static int vx1700_set_mem(RIG *rig, vfo_t vfo, int ch)
{
    struct vx1700_priv_data *priv = (struct vx1700_priv_data *)rig->state.priv;
    struct rig_state *state = &rig->state;

    if (! vx1700_channel_is_ok(ch)) { return -RIG_EINVAL; }

    if (vfo == RIG_VFO_CURR) { vfo = state->current_vfo; }

    if (vfo == RIG_VFO_MEM)
    {
        int ret;
        ret = vx1700_do_dynamic_cmd(rig, VX1700_NATIVE_RECALL_MEM, ch, 0, 0, 0);

        if (ret == RIG_OK) { priv->ch = ch; }

        return ret;
    }

    priv->ch = ch;
    return RIG_OK;
}

static int vx1700_get_mem(RIG *rig, vfo_t vfo, int *ch)
{
    struct vx1700_priv_data *priv = (struct vx1700_priv_data *)rig->state.priv;
    struct rig_state        *state = &rig->state;
    unsigned char       channel = 0;

    if (vfo == RIG_VFO_CURR) { vfo = state->current_vfo; }

    if (vfo == RIG_VFO_MEM)
    {
        int ret;
        ret = vx1700_read_mem_channel_number(rig, &channel);

        if (ret != RIG_OK) { return ret; }

        if (vx1700_channel_is_ok(channel))
        {
            *ch = priv->ch = channel;
            return RIG_OK;
        }

        return -RIG_ERJCTED;
    }

    if (! vx1700_channel_is_ok(priv->ch)) { return -RIG_ERJCTED; }

    *ch = priv->ch;
    return RIG_OK;
}

static int vx1700_vfo_op(RIG *rig, vfo_t vfo, vfo_op_t op)
{
    struct vx1700_priv_data *priv = (struct vx1700_priv_data *)rig->state.priv;

    (void) rig;
    (void) vfo;
    rig_debug(RIG_DEBUG_TRACE, "%s: op=0x%04x\n", __func__, (int) op);

    switch (op)
    {
    case RIG_OP_TO_VFO:
        return vx1700_do_dynamic_cmd(rig, VX1700_NATIVE_RECALL_MEM, priv->ch, 0, 0, 0);

    case RIG_OP_FROM_VFO:
        return vx1700_do_dynamic_cmd(rig, VX1700_NATIVE_VFO_TO_MEM, priv->ch, 0, 0, 0);

    case RIG_OP_UP:
        return vx1700_do_static_cmd(rig, VX1700_NATIVE_OP_FREQ_STEP_UP);

    case RIG_OP_DOWN:
        return vx1700_do_static_cmd(rig, VX1700_NATIVE_OP_FREQ_STEP_DOWN);

    default:
        return -RIG_EINVAL;
    }
}
