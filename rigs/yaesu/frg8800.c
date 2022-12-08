/*
 * frg8800.c - (C) Stephane Fillod 2002-2004
 *
 * This shared library provides an API for communicating
 * via serial interface to an FRG-8800 using the "CAT" interface
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

#include "hamlib/rig.h"
#include "serial.h"
#include "misc.h"
#include "yaesu.h"


/* Private helper function prototypes */


#define FRG8800_MODES (RIG_MODE_CW|RIG_MODE_SSB|RIG_MODE_AM|RIG_MODE_FM|RIG_MODE_WFM)

#define FRG8800_VFOS (RIG_VFO_A)
#define FRG8800_ANTS 0

static int frg8800_open(RIG *rig);
static int frg8800_close(RIG *rig);

static int frg8800_set_freq(RIG *rig, vfo_t vfo, freq_t freq);
static int frg8800_set_mode(RIG *rig, vfo_t vfo, rmode_t mode, pbwidth_t width);
static int frg8800_set_powerstat(RIG *rig, powerstat_t status);

/*
 * frg8800 rigs capabilities.
 * Also this struct is READONLY!
 *
 */

const struct rig_caps frg8800_caps =
{
    RIG_MODEL(RIG_MODEL_FRG8800),
    .model_name =         "FRG-8800",
    .mfg_name =           "Yaesu",
    .version =            "20160409.0",
    .copyright =          "LGPL",
    .status =             RIG_STATUS_ALPHA,
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
    .write_delay =        0,
    .post_write_delay =   300,
    .timeout =            2000,
    .retry =              0,
    .has_get_func =       RIG_FUNC_NONE,
    .has_set_func =       RIG_FUNC_NONE,
    .has_get_level =      RIG_LEVEL_BAND_SELECT,
    .has_set_level =      RIG_LEVEL_BAND_SELECT,
    .has_get_parm =       RIG_PARM_NONE,
    .has_set_parm =       RIG_PARM_NONE,
    .level_gran =
    {
#include "level_gran_yaesu.h"
    },
    .vfo_ops =        RIG_OP_NONE,
    .preamp =             { RIG_DBLST_END, },
    .attenuator =         { RIG_DBLST_END, },
    .max_rit =            Hz(0),
    .max_xit =            Hz(0),
    .max_ifshift =        Hz(0),
    .targetable_vfo =     RIG_TARGETABLE_FREQ,
    .transceive =         RIG_TRN_OFF,
    .bank_qty =           0,
    .chan_desc_sz =       0,
    .chan_list =          {
        RIG_CHAN_END,
    },
    .rx_range_list1 =     {
        {kHz(150), MHz(29.999), FRG8800_MODES, -1, -1, FRG8800_VFOS, FRG8800_ANTS },
        {MHz(118), MHz(173.999), FRG8800_MODES, -1, -1, FRG8800_VFOS, FRG8800_ANTS },
        RIG_FRNG_END,
    }, /* Region 1 rx ranges */

    .tx_range_list1 =     {
        RIG_FRNG_END,
    },    /* region 1 TX ranges */

    .rx_range_list2 =     {
        {kHz(150), MHz(29.999), FRG8800_MODES, -1, -1, FRG8800_VFOS, FRG8800_ANTS },
        {MHz(118), MHz(173.999), FRG8800_MODES, -1, -1, FRG8800_VFOS, FRG8800_ANTS },
        RIG_FRNG_END,
    }, /* Region 2 rx ranges */

    .tx_range_list2 =     {
        RIG_FRNG_END,
    },    /* region 2 TX ranges */

    .tuning_steps =       {
        {FRG8800_MODES, Hz(25)},
        RIG_TS_END,
    },

    /* mode/filter list, remember: order matters! */
    .filters =            {
        {RIG_MODE_AM,   kHz(6)},
        {RIG_MODE_SSB | RIG_MODE_CW | RIG_MODE_AM,  kHz(2.7)},
        {RIG_MODE_CW,   Hz(500)},
        {RIG_MODE_FM,   kHz(12.5)},
        {RIG_MODE_WFM,   kHz(230)}, /* optional unit */

        RIG_FLT_END,
    },

    .rig_open =       frg8800_open,
    .rig_close =      frg8800_close,

    .set_freq =           frg8800_set_freq,
    .set_mode =           frg8800_set_mode,

    .set_powerstat =  frg8800_set_powerstat,
    .hamlib_check_rig_caps = HAMLIB_CHECK_RIG_CAPS
};


#define MODE_SET_AM 0x00
#define MODE_SET_LSB    0x01
#define MODE_SET_USB    0x02
#define MODE_SET_CW 0x03
#define MODE_SET_FM 0x0c
#define MODE_SET_FMW    0x04


/*
 * frg8800_open  routine
 *
 */
int frg8800_open(RIG *rig)
{
    unsigned char cmd[YAESU_CMD_LENGTH] = { 0x00, 0x00, 0x00, 0x00, 0x00};

    rig_debug(RIG_DEBUG_TRACE, "%s: called\n", __func__);

    /* send Ext Cntl ON: Activate CAT */
    return write_block(&rig->state.rigport, cmd, YAESU_CMD_LENGTH);

}

int frg8800_close(RIG *rig)
{
    unsigned char cmd[YAESU_CMD_LENGTH] = { 0x00, 0x00, 0x00, 0x80, 0x00};

    rig_debug(RIG_DEBUG_TRACE, "%s: called\n", __func__);

    /* send Ext Cntl OFF: Deactivate CAT */
    return write_block(&rig->state.rigport, cmd, YAESU_CMD_LENGTH);

}


int frg8800_set_freq(RIG *rig, vfo_t vfo, freq_t freq)
{
    unsigned char cmd[YAESU_CMD_LENGTH] = { 0x00, 0x00, 0x00, 0x00, 0x01};

    rig_debug(RIG_DEBUG_TRACE, "%s: called\n", __func__);

    /* store bcd format in cmd (LSB) */
    to_bcd(cmd, freq / 10, 8);

    /* Byte1: 100Hz's and 25Hz step code */
    cmd[0] = (cmd[0] & 0xf0) | (1 << ((((long long)freq) % 100) / 25));

    /* Frequency set */
    return write_block(&rig->state.rigport, cmd, YAESU_CMD_LENGTH);
}


int frg8800_set_mode(RIG *rig, vfo_t vfo, rmode_t mode, pbwidth_t width)
{
    unsigned char cmd[YAESU_CMD_LENGTH] = { 0x00, 0x00, 0x00, 0x00, 0x80};
    unsigned char md;

    rig_debug(RIG_DEBUG_TRACE, "%s: frg8800_set_mode called %s\n", __func__,
              rig_strrmode(mode));

    /*
     * translate mode from generic to frg8800 specific
     */
    switch (mode)
    {
    case RIG_MODE_AM: md = MODE_SET_AM; break;

    case RIG_MODE_CW: md = MODE_SET_CW; break;

    case RIG_MODE_USB:    md = MODE_SET_USB; break;

    case RIG_MODE_LSB:    md = MODE_SET_LSB; break;

    case RIG_MODE_FM: md = MODE_SET_FM; break;

    case RIG_MODE_WFM:    md = MODE_SET_FMW; break;

    default:
        return -RIG_EINVAL;         /* sorry, wrong MODE */
    }

    if (width != RIG_PASSBAND_NOCHANGE
            && width != RIG_PASSBAND_NORMAL
            && width < rig_passband_normal(rig, mode))
    {
        md |= 0x08;
    }

    cmd[3] = md;

    /* Mode set */
    return write_block(&rig->state.rigport, cmd, YAESU_CMD_LENGTH);
}



int frg8800_set_powerstat(RIG *rig, powerstat_t status)
{
    unsigned char cmd[YAESU_CMD_LENGTH] = { 0x00, 0x00, 0x00, 0x00, 0x80};

    rig_debug(RIG_DEBUG_TRACE, "%s: called\n", __func__);

    cmd[3] = status == RIG_POWER_OFF ? 0xff : 0xfe;

    /* Frequency set */
    return write_block(&rig->state.rigport, cmd, YAESU_CMD_LENGTH);
}

