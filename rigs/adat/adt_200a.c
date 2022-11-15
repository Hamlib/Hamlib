// ---------------------------------------------------------------------------
//    ADT-200A HAMLIB BACKEND
// ---------------------------------------------------------------------------
//
//  adt_200a.c
//
//  Created by Frank Goenninger DG1SBG.
//  Copyright © 2011, 2012 Frank Goenninger.
//
//   This library is free software; you can redistribute it and/or
//   modify it under the terms of the GNU Lesser General Public
//   License as published by the Free Software Foundation; either
//   version 2.1 of the License, or (at your option) any later version.
//
//   This library is distributed in the hope that it will be useful,
//   but WITHOUT ANY WARRANTY; without even the implied warranty of
//   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
//   Lesser General Public License for more details.
//
//   You should have received a copy of the GNU Lesser General Public
//   License along with this library; if not, write to the Free Software
//   Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA


#include <hamlib/config.h>

// ---------------------------------------------------------------------------
//    ADT-200A INCLUDES
// ---------------------------------------------------------------------------

#include "adt_200a.h"

// ---------------------------------------------------------------------------
//    GLOBAL DEFINITIONS
// ---------------------------------------------------------------------------

// GLOBAL VARS

// static const struct confparams adt_200a_cfg_params[] =
// {
//  { TOKEN_PRODUCT_NAME, "usb_product_name", "USB Product Name", "USB Product Name (DSP Bo
//    Model + ' Serial '+ ID Code, e.g. 'TRX3C Serial C945D5B' )",
//    ADT_200A_PRODUCT_NAME, RIG_CONF_STRING, { .n = { 0,0,0 } }
//  },
//
//  { RIG_CONF_END, NULL, }
//};


// ---------------------------------------------------------------------------
//    ADT-200A HAMLIB CAPS / DESCRIPTION
// ---------------------------------------------------------------------------

const struct rig_caps adt_200a_caps =
{
    RIG_MODEL(RIG_MODEL_ADT_200A),
    .model_name         =  "ADT-200A",
    .mfg_name           =  "ADAT www.adat.ch",
    .version            =  BACKEND_VER ".0",
    .copyright          =  "Frank Goenninger, DG1SBG. License: Creative Commons",
    .status             =  RIG_STATUS_STABLE,
    .rig_type           =  RIG_TYPE_TRANSCEIVER,
    .ptt_type           =  RIG_PTT_RIG,
    .dcd_type           =  RIG_DCD_NONE,
    .port_type          =  RIG_PORT_SERIAL,
    .serial_rate_min    =  115200,
    .serial_rate_max    =  115200,
    .serial_data_bits   =  8,
    .serial_stop_bits   =  1,
    .serial_parity      =  RIG_PARITY_NONE,
    .serial_handshake   =  RIG_HANDSHAKE_NONE,
    .write_delay        =  0,
    .post_write_delay   =  20,
    .timeout            =  250,
    .retry              =  3,

    .has_get_func       =  ADT_200A_FUNCS,
    .has_set_func       =  ADT_200A_FUNCS,
    .has_get_level      =  ADT_200A_GET_LEVEL,
    .has_set_level      =  RIG_LEVEL_SET(ADT_200A_SET_LEVEL),
    .has_get_parm       =  RIG_PARM_NONE,
    .has_set_parm       =  RIG_PARM_NONE,
    .level_gran         =  {},
    .parm_gran          =  {},
    .ctcss_list         =  NULL,
    .dcs_list           =  NULL,
    .preamp             =  { 5, 10, RIG_DBLST_END, },
    .attenuator         =  { 5, 10, 15, 20, 25, RIG_DBLST_END, },
    .max_rit            =  ADT_200A_RIT,
    .max_xit            =  ADT_200A_XIT,
    .max_ifshift        =  Hz(500),
    .targetable_vfo     =  RIG_TARGETABLE_NONE,
    .transceive         =  0,
    .bank_qty           =  1,

    .chan_desc_sz     =  ADAT_MEM_DESC_SIZE,
    .chan_list        =
    {
        {   0,  99, RIG_MTYPE_MEM, ADAT_MEM_CAPS },
        RIG_CHAN_END,
    },

    .rx_range_list1 =
    {
        { kHz(10), MHz(30), ADT_200A_MODES, -1, -1, ADT_200A_VFO },
        RIG_FRNG_END,
    },

    .tx_range_list1 =
    {
        { kHz(10), MHz(30), ADT_200A_MODES, mW(100), W(50), ADT_200A_VFO },
        RIG_FRNG_END,
    },

    .rx_range_list2 =
    {
        { kHz(10), MHz(30), ADT_200A_MODES, -1, -1, ADT_200A_VFO },
        RIG_FRNG_END,
    },

    .tx_range_list2 =
    {
        { kHz(10), MHz(30), ADT_200A_MODES, mW(100), W(50), ADT_200A_VFO },
        RIG_FRNG_END,
    },

    .tuning_steps =
    {
        { ADT_200A_MODES, RIG_TS_ANY }, // TODO: get actual list here
        RIG_TS_END,
    },

    .filters =
    {
        { RIG_MODE_CW | RIG_MODE_CWR, Hz(50) },
        { RIG_MODE_CW | RIG_MODE_CWR, Hz(75) },
        { RIG_MODE_CW | RIG_MODE_CWR, Hz(100) },
        { RIG_MODE_CW | RIG_MODE_CWR, Hz(150) },
        { RIG_MODE_CW | RIG_MODE_CWR, Hz(200) },
        { RIG_MODE_CW | RIG_MODE_CWR, Hz(300) },
        { RIG_MODE_CW | RIG_MODE_CWR, Hz(750) },
        { RIG_MODE_CW | RIG_MODE_CWR, Hz(1000) },
        { RIG_MODE_CW | RIG_MODE_CWR, Hz(1200) },
        { RIG_MODE_LSB | RIG_MODE_USB, Hz(300) },
        { RIG_MODE_LSB | RIG_MODE_USB, Hz(500) },
        { RIG_MODE_LSB | RIG_MODE_USB, Hz(750) },
        { RIG_MODE_LSB | RIG_MODE_USB, Hz(1000) },
        { RIG_MODE_LSB | RIG_MODE_USB, Hz(1200) },
        { RIG_MODE_LSB | RIG_MODE_USB, Hz(1500) },
        { RIG_MODE_LSB | RIG_MODE_USB, Hz(1800) },
        { RIG_MODE_LSB | RIG_MODE_USB, Hz(2000) },
        { RIG_MODE_LSB | RIG_MODE_USB, Hz(2200) },
        { RIG_MODE_LSB | RIG_MODE_USB, Hz(2400) },
        { RIG_MODE_LSB | RIG_MODE_USB, Hz(2700) },
        { RIG_MODE_LSB | RIG_MODE_USB, Hz(3500) },
        { RIG_MODE_AM | RIG_MODE_SAL | RIG_MODE_SAH, Hz(3000) },
        { RIG_MODE_AM | RIG_MODE_SAL | RIG_MODE_SAH, Hz(3500) },
        { RIG_MODE_AM | RIG_MODE_SAL | RIG_MODE_SAH, Hz(4000) },
        { RIG_MODE_AM | RIG_MODE_SAL | RIG_MODE_SAH, Hz(4500) },
        { RIG_MODE_AM | RIG_MODE_SAL | RIG_MODE_SAH, Hz(5000) },
        { RIG_MODE_AM | RIG_MODE_SAL | RIG_MODE_SAH, Hz(6000) },
        { RIG_MODE_AM | RIG_MODE_SAL | RIG_MODE_SAH, Hz(7000) },
        { RIG_MODE_AM | RIG_MODE_SAL | RIG_MODE_SAH, Hz(8000) },
        { RIG_MODE_FM, Hz(6000) },
        { RIG_MODE_FM, Hz(7000) },
        { RIG_MODE_FM, Hz(8000) },
        { RIG_MODE_FM, Hz(9000) },
        { RIG_MODE_FM, Hz(10000) },
        { RIG_MODE_FM, Hz(11000) },
        { RIG_MODE_FM, Hz(12000) },
        RIG_FLT_END,
    },
    .str_cal = ADT_200A_STR_CAL,

    // .cfgparams          =  adt_200a_cfg_params,

    .rig_init           =  adat_init,
    .rig_cleanup        =  adat_cleanup,
    .rig_open           =  adat_open,
    .reset              =  adat_reset,
    .rig_close          =  adat_close,

    .set_conf           =  adat_set_conf,
    .get_conf           =  adat_get_conf,

    .set_freq           =  adat_set_freq,
    .get_freq           =  adat_get_freq,

    .get_level          =  adat_get_level,
    .set_level          =  adat_set_level,

    .set_mode           =  adat_set_mode,
    .get_mode           =  adat_get_mode,

    .get_vfo            =  adat_get_vfo,
    .set_vfo            =  adat_set_vfo,

    .get_ptt            =  adat_get_ptt,
    .set_ptt            =  adat_set_ptt,

    .decode_event       =  adat_handle_event,

    .get_info           =  adat_get_info,

    .power2mW           =  adat_power2mW,
    .mW2power           =  adat_mW2power,

    .get_powerstat      =  adat_get_powerstat,
    .hamlib_check_rig_caps = HAMLIB_CHECK_RIG_CAPS
};

// ---------------------------------------------------------------------------
//    END OF FILE
// ---------------------------------------------------------------------------
