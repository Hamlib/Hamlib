/*
 * frg9600.c - (C) Stephane Fillod 2002-2004
 *
 * This shared library provides an API for communicating
 * via serial interface to an FRG-9600 using the "CAT" interface
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


#define FRG9600_MODES (RIG_MODE_SSB|RIG_MODE_AM|RIG_MODE_FM|RIG_MODE_WFM)

#define FRG9600_VFOS (RIG_VFO_A)

static int frg9600_set_freq(RIG *rig, vfo_t vfo, freq_t freq);
static int frg9600_set_mode(RIG *rig, vfo_t vfo, rmode_t mode, pbwidth_t width);

/*
 * frg9600 rigs capabilities.
 * Also this struct is READONLY!
 *
 */

const struct rig_caps frg9600_caps =
{
    RIG_MODEL(RIG_MODEL_FRG9600),
    .model_name =         "FRG-9600",
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
        {MHz(60), MHz(905), RIG_MODE_AM | RIG_MODE_FM | RIG_MODE_WFM, -1, -1, FRG9600_VFOS },
        {MHz(60), MHz(460), RIG_MODE_SSB, -1, -1, FRG9600_VFOS },
        RIG_FRNG_END,
    }, /* Region 1 rx ranges */

    .tx_range_list1 =     {
        RIG_FRNG_END,
    },    /* region 1 TX ranges */

    .rx_range_list2 =     {
        {MHz(60), MHz(905), RIG_MODE_AM | RIG_MODE_FM | RIG_MODE_WFM, -1, -1, FRG9600_VFOS },
        {MHz(60), MHz(460), RIG_MODE_SSB, -1, -1, FRG9600_VFOS },
        RIG_FRNG_END,
    }, /* Region 2 rx ranges */

    .tx_range_list2 =     {
        RIG_FRNG_END,
    },    /* region 2 TX ranges */

    .tuning_steps =       {
        {RIG_MODE_SSB | RIG_MODE_AM, Hz(100)},
        {RIG_MODE_FM, kHz(5)},
        {RIG_MODE_WFM, kHz(100)},
        RIG_TS_END,
    },

    /* mode/filter list, at -3dB ! */
    .filters =            {
        {RIG_MODE_AM,   kHz(6)},
        {RIG_MODE_SSB | RIG_MODE_AM,  kHz(2.4)},
        {RIG_MODE_FM,   kHz(15)},
        {RIG_MODE_WFM,   kHz(180)},
        RIG_FLT_END,
    },

    .set_freq =           frg9600_set_freq,
    .set_mode =           frg9600_set_mode,

    .hamlib_check_rig_caps = HAMLIB_CHECK_RIG_CAPS
};


#define MODE_SET_LSB    0x10
#define MODE_SET_USB    0x11
#define MODE_SET_AMN    0x14
#define MODE_SET_AMW    0x15
#define MODE_SET_FMN    0x16
#define MODE_SET_WFM    0x17


int frg9600_set_freq(RIG *rig, vfo_t vfo, freq_t freq)
{
    unsigned char cmd[YAESU_CMD_LENGTH] = { 0x0a, 0x00, 0x00, 0x00, 0x00};

    /* store bcd format in cmd (MSB) */
    to_bcd_be(cmd + 1, freq / 10, 8);

    /* Frequency set */
    return write_block(&rig->state.rigport, cmd, YAESU_CMD_LENGTH);
}


int frg9600_set_mode(RIG *rig, vfo_t vfo, rmode_t mode, pbwidth_t width)
{
    unsigned char cmd[YAESU_CMD_LENGTH] = { 0x00, 0x00, 0x00, 0x00, 0x00};
    unsigned char md;

    /*
     * translate mode from generic to frg9600 specific
     */
    switch (mode)
    {
    case RIG_MODE_USB:    md = MODE_SET_USB; break;

    case RIG_MODE_LSB:    md = MODE_SET_LSB; break;

    case RIG_MODE_FM: md = MODE_SET_FMN; break;

    case RIG_MODE_WFM:    md = MODE_SET_WFM; break;

    case RIG_MODE_AM:
        if (width != RIG_PASSBAND_NOCHANGE
                && width != RIG_PASSBAND_NORMAL
                && width < rig_passband_normal(rig, mode))
        {
            md = MODE_SET_AMN;
        }
        else
        {
            md = MODE_SET_AMW;
        }

        break;

    default:
        return -RIG_EINVAL;         /* sorry, wrong MODE */
    }

    cmd[0] = md;

    /* Mode set */
    return write_block(&rig->state.rigport, cmd, YAESU_CMD_LENGTH);
}

