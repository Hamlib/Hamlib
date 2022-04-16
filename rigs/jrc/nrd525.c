/*
 *  Hamlib JRC backend - NRD-525 description
 *  Copyright (c) 2001-2009 by Stephane Fillod
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
#include "iofunc.h"
#include "jrc.h"


static int nrd525_open(RIG *rig);
static int nrd525_close(RIG *rig);
static int nrd525_set_freq(RIG *rig, vfo_t vfo, freq_t freq);
static int nrd525_set_mode(RIG *rig, vfo_t vfo, rmode_t mode, pbwidth_t width);
static int nrd525_set_func(RIG *rig, vfo_t vfo, setting_t func, int status);
static int nrd525_set_level(RIG *rig, vfo_t vfo, setting_t level, value_t val);
static int nrd525_vfo_op(RIG *rig, vfo_t vfo, vfo_op_t op);
static int nrd525_set_mem(RIG *rig, vfo_t vfo, int ch);

#define NRD525_MODES (RIG_MODE_AM|RIG_MODE_CW|RIG_MODE_SSB|RIG_MODE_FM|RIG_MODE_RTTY|RIG_MODE_FAX)

#define NRD525_FUNC (RIG_FUNC_LOCK)
#define NRD525_LEVEL (RIG_LEVEL_ATT|RIG_LEVEL_AGC)

#define NRD525_VFO (RIG_VFO_VFO)

#define NRD525_MEM_CAP { \
    .freq = 1,      \
    .mode = 1,      \
    .width = 1,     \
    .levels = RIG_LEVEL_ATT|RIG_LEVEL_AGC, \
}

/*
 * NRD-525 rig capabilities.
 *
 */
const struct rig_caps nrd525_caps =
{
    RIG_MODEL(RIG_MODEL_NRD525),
    .model_name = "NRD-525",
    .mfg_name =  "JRC",
    .version =  BACKEND_VER ".0",
    .copyright =  "LGPL",
    .status =  RIG_STATUS_BETA,
    .rig_type =  RIG_TYPE_RECEIVER,
    .ptt_type =  RIG_PTT_NONE,
    .dcd_type =  RIG_DCD_NONE,
    .port_type =  RIG_PORT_SERIAL,
    .serial_rate_min =  300,
    .serial_rate_max =  4800,
    .serial_data_bits =  8,
    .serial_stop_bits =  1,
    .serial_parity =  RIG_PARITY_NONE,
    .serial_handshake =  RIG_HANDSHAKE_NONE,
    .write_delay =  0,
    .post_write_delay =  20,
    .timeout =  1000,
    .retry =  0,

    .has_get_func =  RIG_FUNC_NONE,
    .has_set_func =  NRD525_FUNC,
    .has_get_level =  RIG_LEVEL_NONE,
    .has_set_level =  NRD525_LEVEL,
    .has_get_parm =  RIG_PARM_NONE,
    .has_set_parm =  RIG_PARM_NONE,
    .level_gran = {},
    .parm_gran =  {},
    .ctcss_list =  NULL,
    .dcs_list =  NULL,
    .preamp =   { RIG_DBLST_END },
    .attenuator =   { 20, RIG_DBLST_END },
    .max_rit =  Hz(0),
    .max_xit =  Hz(0),
    .max_ifshift =  Hz(0),
    .targetable_vfo =  0,
    .transceive =  RIG_TRN_OFF,
    .vfo_ops =  RIG_OP_FROM_VFO,
    .scan_ops =  RIG_SCAN_NONE,
    .bank_qty =   0,
    .chan_desc_sz =  0,

    .chan_list =  {
        { 0, 199, RIG_MTYPE_MEM, NRD525_MEM_CAP },
        RIG_CHAN_END
    },

    .rx_range_list1 =  {
        {kHz(10), MHz(30), NRD525_MODES, -1, -1, NRD525_VFO},
        RIG_FRNG_END,
    },
    .tx_range_list1 =  { RIG_FRNG_END, },
    .rx_range_list2 =  {
        {kHz(10), MHz(30), NRD525_MODES, -1, -1, NRD525_VFO},
        RIG_FRNG_END,
    },
    .tx_range_list2 =  { RIG_FRNG_END, },

    .tuning_steps =  {
        {NRD525_MODES, 10},
        RIG_TS_END,
    },
    /* mode/filter list, .remember =  order matters! */
    .filters =  {
        {RIG_MODE_FM, kHz(12)},
        {RIG_MODE_FM, kHz(6)},
        {RIG_MODE_AM, kHz(6)},
        {RIG_MODE_AM, kHz(2)},
        {RIG_MODE_AM, kHz(12)},
        {RIG_MODE_SSB | RIG_MODE_RTTY | RIG_MODE_FAX, kHz(2)},
        {RIG_MODE_SSB | RIG_MODE_RTTY | RIG_MODE_FAX, kHz(1)},
        {RIG_MODE_SSB | RIG_MODE_RTTY | RIG_MODE_FAX, kHz(6)},
        {RIG_MODE_CW, kHz(1)},
        {RIG_MODE_CW, kHz(2)},
        RIG_FLT_END,
    },

    .rig_open =  nrd525_open,
    .rig_close =  nrd525_close,
    .set_freq =  nrd525_set_freq,
    .set_mode =  nrd525_set_mode,
    .set_func =  nrd525_set_func,
    .set_level =  nrd525_set_level,
    .set_mem =  nrd525_set_mem,
    .vfo_op =  nrd525_vfo_op,
    .hamlib_check_rig_caps = HAMLIB_CHECK_RIG_CAPS
};

/*
 * Function definitions below
 */

static int nrd525_open(RIG *rig)
{
    return write_block(&rig->state.rigport, (unsigned char *) "H1", 2);
}

static int nrd525_close(RIG *rig)
{
    return write_block(&rig->state.rigport, (unsigned char *) "H0", 2);
}

static int nrd525_set_freq(RIG *rig, vfo_t vfo, freq_t freq)
{
    char freqbuf[12];

    SNPRINTF(freqbuf, sizeof(freqbuf), "F%08u", (unsigned)(freq / 10));

    return write_block(&rig->state.rigport, (unsigned char *) freqbuf,
                       strlen(freqbuf));
}

static int nrd525_set_mode(RIG *rig, vfo_t vfo, rmode_t mode, pbwidth_t width)
{
    int retval;
    char *modestr;

    switch (mode)
    {
    case RIG_MODE_RTTY: modestr = "D0"; break;

    case RIG_MODE_CW: modestr = "D1"; break;

    case RIG_MODE_USB: modestr = "D2"; break;

    case RIG_MODE_LSB: modestr = "D3"; break;

    case RIG_MODE_AM: modestr = "D4"; break;

    case RIG_MODE_FM: modestr = "D5"; break;

    case RIG_MODE_FAX: modestr = "D6"; break;

    default:
        return -RIG_EINVAL;
    }

    retval = write_block(&rig->state.rigport, (unsigned char *) modestr,
                         strlen(modestr));

    if (retval != RIG_OK)
    {
        return retval;
    }

    if (RIG_PASSBAND_NOCHANGE == width) { return retval; }

    // TODO: width

    return retval;
}

static int nrd525_set_func(RIG *rig, vfo_t vfo, setting_t func, int status)
{
    return -RIG_ENIMPL;
}

static int nrd525_set_level(RIG *rig, vfo_t vfo, setting_t level, value_t val)
{
    switch (level)
    {
    case RIG_LEVEL_ATT:
        return write_block(&rig->state.rigport,
                           (unsigned char *)(val.i != 0 ? "A1" : "A0"), 2);

    case RIG_LEVEL_AGC:
        return write_block(&rig->state.rigport,
                           (unsigned char *)(val.i == RIG_AGC_SLOW ? "G0" :
                                             (val.i == RIG_AGC_FAST ? "G1" : "G2")), 2);

    default:
        return -RIG_EINVAL;
    }
}

static int nrd525_set_mem(RIG *rig, vfo_t vfo, int ch)
{
    char membuf[12];

    SNPRINTF(membuf, sizeof(membuf), "C%03d", ch);

    return write_block(&rig->state.rigport, (unsigned char *) membuf,
                       strlen(membuf));
}

static int nrd525_vfo_op(RIG *rig, vfo_t vfo, vfo_op_t op)
{
    switch (op)
    {
    case RIG_OP_FROM_VFO:
        return write_block(&rig->state.rigport, (unsigned char *) "E1", 2);

    default:
        return -RIG_EINVAL;
    }
}

