/*
 *  Hamlib CI-V backend - Perseus description
 *  Copyright (c) 2016 by Stephane Fillod
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

#include <hamlib/rig.h>
#include "serial.h"
#include "misc.h"
#include "idx_builtin.h"

#include "icom.h"
#include "icom_defs.h"
#include "frame.h"


/* TODO: $09 DRM, $0a USER */
#define PERSEUS_MODES (RIG_MODE_AM|RIG_MODE_SAM|RIG_MODE_SSB| \
        RIG_MODE_CW|RIG_MODE_CWR|RIG_MODE_RTTY|RIG_MODE_RTTYR| \
        RIG_MODE_FM)

#define PERSEUS_FUNCS (RIG_FUNC_NONE)

/* TODO (not standard) :
 *   RIG_LEVEL_AGC|RIG_LEVEL_NB|RIG_LEVEL_ANR|RIG_LEVEL_ANR|RIG_LEVEL_AF|RIG_LEVEL_ANF
 */

#define PERSEUS_LEVELS (RIG_LEVEL_ATT|RIG_LEVEL_SQL|RIG_LEVEL_RAWSTR)

#define PERSEUS_PARMS (RIG_PARM_NONE)

/* S-Meter calibration, according to the Reference Manual */
#define PERSEUS_STR_CAL { 2, \
    { \
        {    0, -67 }, /* -140 dBm */ \
        {  255, 103 }, /* +30 dBm */ \
    } }


static int perseus_r2i_mode(RIG *rig, vfo_t vfo, rmode_t mode, pbwidth_t width,
                            unsigned char *md, signed char *pd);
static void perseus_i2r_mode(RIG *rig, unsigned char md, int pd,
                             rmode_t *mode, pbwidth_t *width);

static struct icom_priv_caps perseus_priv_caps =
{
    0xE1,   /* default address */
    0,      /* 731 mode */
    0,      /* no XCHG */
    .r2i_mode = perseus_r2i_mode,
    .i2r_mode = perseus_i2r_mode,
};


/*
 * PERSEUS rigs capabilities.
 *
 * PERSEUS Receiver CAT Interface Reference Manual (Revision EN03) :
 *  http://microtelecom.it/perseus/PERSEUS_CI-V_Interface-EN03.pdf
 */
const struct rig_caps perseus_caps =
{
    RIG_MODEL(RIG_MODEL_PERSEUS),
    .model_name = "Perseus",
    .mfg_name =  "Microtelecom",
    .version =  BACKEND_VER ".0",
    .copyright =  "LGPL",
    .status =  RIG_STATUS_BETA,
    .rig_type =  RIG_TYPE_PCRECEIVER,
    .ptt_type =  RIG_PTT_NONE,
    .dcd_type =  RIG_DCD_NONE,
    .port_type =  RIG_PORT_SERIAL,
    .serial_rate_min =  300,
    .serial_rate_max =  19200,
    .serial_data_bits =  8,
    .serial_stop_bits =  1,
    .serial_parity =  RIG_PARITY_NONE,
    .serial_handshake =  RIG_HANDSHAKE_NONE,
    .write_delay =  0,
    .post_write_delay =  0,
    .timeout =  1000,
    .retry =  3,

    .has_get_func =  PERSEUS_FUNCS,
    .has_set_func =  PERSEUS_FUNCS,
    .has_get_level =  PERSEUS_LEVELS,
    .has_set_level =  RIG_LEVEL_SET(PERSEUS_LEVELS),
    .has_get_parm =  PERSEUS_PARMS,
    .has_set_parm =  PERSEUS_PARMS,
    .level_gran =  {},
    .parm_gran =  {},
    .ctcss_list =  NULL,
    .dcs_list =  NULL,
    .preamp =   { RIG_DBLST_END, },
    .attenuator =   { 10, 20, 30, RIG_DBLST_END, },
    .max_rit =  Hz(0),
    .max_xit =  Hz(0),
    .max_ifshift =  Hz(0),
    .targetable_vfo =  0,
    .vfo_ops =  RIG_OP_NONE,
    .transceive =  RIG_TRN_OFF,
    .bank_qty =   0,
    .chan_desc_sz =  0,

    .chan_list =  {
        RIG_CHAN_END,
    },

    .rx_range_list1 =  {
        {kHz(10), MHz(30), PERSEUS_MODES, -1, -1, RIG_VFO_A},
        RIG_FRNG_END,
    },
    .tx_range_list1 =  { RIG_FRNG_END, },

    .rx_range_list2 =  {
        {kHz(10), MHz(30), PERSEUS_MODES, -1, -1, RIG_VFO_A},
        RIG_FRNG_END,
    },
    .tx_range_list2 =  { RIG_FRNG_END, },   /* no TX ranges, this is a receiver */

    .tuning_steps =  {
        {PERSEUS_MODES, 100},  /* resolution */
        RIG_TS_END,
    },
    /* mode/filter list, remember: order matters! */
    .filters =  {
        {RIG_MODE_SSB | RIG_MODE_CW | RIG_MODE_CWR | RIG_MODE_RTTY | RIG_MODE_RTTYR, kHz(2.4)},
        {RIG_MODE_AM | RIG_MODE_SAM, kHz(8)},
        {RIG_MODE_FM, kHz(15)},
        RIG_FLT_END,
    },

    .str_cal = PERSEUS_STR_CAL,

    .cfgparams =  icom_cfg_params,
    .set_conf =  icom_set_conf,
    .get_conf =  icom_get_conf,

    .priv = (void *)& perseus_priv_caps,
    .rig_init =   icom_init,
    .rig_cleanup =   icom_cleanup,
    .rig_open =  icom_rig_open,
    .rig_close =  icom_rig_open,

    .set_freq =  icom_set_freq,
    .get_freq =  icom_get_freq,

    .set_mode =  icom_set_mode,
    .get_mode =  icom_get_mode,

    .set_level =  icom_set_level,
    .get_level =  icom_get_level,

    .hamlib_check_rig_caps = HAMLIB_CHECK_RIG_CAPS
};

/*
 * Function definitions below
 */


/*
 * This function does the special bandwidth coding for the Perseus
 *
 * NB: the filter width will be ignored.
 */
static int perseus_r2i_mode(RIG *rig, vfo_t vfo, rmode_t mode, pbwidth_t width,
                            unsigned char *md, signed char *pd)
{
    int err;

    err = rig2icom_mode(rig, vfo, mode, width, md, pd);

    if (err == 0 && mode == RIG_MODE_SAM)
    {
        *md = 0x06;
    }

    return err;
}

static void perseus_i2r_mode(RIG *rig, unsigned char md, int pd,
                             rmode_t *mode, pbwidth_t *width)
{
    icom2rig_mode(rig, md, pd, mode, width);

    if (md == 0x06)
    {
        *mode = RIG_MODE_SAM;
    }
}

