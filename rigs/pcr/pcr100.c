/*
 *  Hamlib PCR backend - PCR-100 description
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

#include <hamlib/rig.h>

#include "pcr.h"
#include "idx_builtin.h"

#define PCR100_MODES ( RIG_MODE_AM | RIG_MODE_FM | RIG_MODE_WFM )

#define PCR100_FUNC ( RIG_FUNC_TSQL )

#define PCR100_LEVEL ( \
            RIG_LEVEL_ATT | RIG_LEVEL_AF | RIG_LEVEL_SQL | RIG_LEVEL_IF | \
            RIG_LEVEL_AGC | RIG_LEVEL_STRENGTH | RIG_LEVEL_RAWSTR )

static const struct confparams pcr_ext_levels[] =
{
    {
        TOK_EL_ANL, "ANL", "Auto Noise Limiter", "Auto Noise Limiter",
        NULL, RIG_CONF_CHECKBUTTON
    },
    { RIG_CONF_END, NULL, }
};

static const struct pcr_priv_caps pcr100_priv =
{
    .reply_size = 6,
    .reply_offset   = 0,
    .always_sync    = 0,
};

/*
 * IC PCR100 rigs capabilities.
 */
const struct rig_caps pcr100_caps =
{
    RIG_MODEL(RIG_MODEL_PCR100),
    .model_name     = "IC-PCR100",
    .mfg_name       = "Icom",
    .version        = BACKEND_VER ".0",
    .copyright      = "LGPL",
    .status         = RIG_STATUS_STABLE,

    .rig_type       = RIG_TYPE_PCRECEIVER,
    .ptt_type       = RIG_PTT_NONE,
    .dcd_type       = RIG_DCD_RIG,
    .port_type      = RIG_PORT_SERIAL,

    .serial_rate_min    = 9600, /* slower speeds gave troubles */
    .serial_rate_max    = 38400,
    .serial_data_bits   = 8,
    .serial_stop_bits   = 1,
    .serial_parity      = RIG_PARITY_NONE,
    .serial_handshake   = RIG_HANDSHAKE_HARDWARE,

    .write_delay        = 12,
    .post_write_delay   = 2,
    .timeout        = 400,
    .retry          = 3,

    .has_get_func       = RIG_FUNC_NONE,
    .has_set_func       = PCR100_FUNC,
    .has_get_level      = PCR100_LEVEL,
    .has_set_level      = RIG_LEVEL_SET(PCR100_LEVEL),
    .has_get_parm       = RIG_PARM_NONE,
    .has_set_parm       = RIG_PARM_NONE,

    .level_gran = {
        // cppcheck-suppress *
        [LVL_RAWSTR] = { .min = { .i = 0 }, .max = { .i = 255 } },
        /* XXX check this */
        [LVL_IF] = { .min = { .i = -1270 }, .max = { .i = 1270 }, .step = { .i = 10 } },
    },
    .parm_gran  = { },

    .ctcss_list = pcr_ctcss_list,
    .dcs_list   = NULL,
    .preamp     = { RIG_DBLST_END },
    .attenuator = { 20, RIG_DBLST_END },

    .max_rit    = Hz(0),
    .max_xit    = Hz(0),
    .max_ifshift    = Hz(0),

    .targetable_vfo = 0,
    .transceive = RIG_TRN_RIG,
    .bank_qty   = 0,
    .chan_desc_sz   = 0,
    .chan_list  = { RIG_CHAN_END },

    .rx_range_list1 = {
        { kHz(10), GHz(1.3), PCR100_MODES, -1, -1, RIG_VFO_A },
        RIG_FRNG_END,
    },
    .tx_range_list1 = { RIG_FRNG_END },
    .rx_range_list2 = {
        { kHz(10), MHz(824) - 10, PCR100_MODES, -1, -1, RIG_VFO_A },
        { MHz(849) + 10, MHz(869) - 10, PCR100_MODES, -1, -1, RIG_VFO_A },
        { MHz(894) + 10, GHz(1.3), PCR100_MODES, -1, -1, RIG_VFO_A },
        RIG_FRNG_END,
    },
    .tx_range_list2 = { RIG_FRNG_END }, /* no TX ranges, this is a receiver */

    .tuning_steps = {
        { PCR100_MODES, Hz(1) },
        RIG_TS_END,
    },
    /* mode/filter list, remember: order matters!
     * first one is the default mode, the one given back
     * by rig_mode_normal(), the others must be sorted.
     */
    .filters = {
        { RIG_MODE_AM | RIG_MODE_FM, kHz(15) },

        { RIG_MODE_AM, kHz(2.8) },
        { RIG_MODE_AM | RIG_MODE_FM, kHz(6) },
        { RIG_MODE_AM | RIG_MODE_FM, kHz(50) },

        { RIG_MODE_WFM, kHz(230) },
        { RIG_MODE_WFM, kHz(50) },

        RIG_FLT_END,
    },

    .priv = (void *)& pcr100_priv,

    /* XXX verify */
    .str_cal = {
        7, {
            { 0, -60 },
            { 160, 0 },
            { 176, 20 },
            { 192, 30 },
            { 208, 40 },
            { 224, 50 },
            { 240, 60 },
        }
    },

    .extlevels = pcr_ext_levels,

    .rig_init   = pcr_init,
    .rig_cleanup    = pcr_cleanup,
    .rig_open   = pcr_open,
    .rig_close  = pcr_close,

    .set_freq   = pcr_set_freq,
    .get_freq   = pcr_get_freq,
    .set_mode   = pcr_set_mode,
    .get_mode   = pcr_get_mode,

    .get_info   = pcr_get_info,

    .set_level  = pcr_set_level,
    .get_level  = pcr_get_level,

    .set_func   = pcr_set_func,
    .get_func   = pcr_get_func,

    .set_ext_level  = pcr_set_ext_level,

    .set_ctcss_sql  = pcr_set_ctcss_sql,
    .get_ctcss_sql  = pcr_get_ctcss_sql,

    .set_trn    = pcr_set_trn,
    .decode_event   = pcr_decode_event,

    .set_powerstat  = pcr_set_powerstat,
    .get_powerstat  = pcr_get_powerstat,
    .hamlib_check_rig_caps = HAMLIB_CHECK_RIG_CAPS
};
