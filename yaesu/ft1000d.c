/*
 * hamlib - (C) Stephane Fillod 2002-2004 (fillods at users.sourceforge.net)
 *
 * ft1000d.c - (C) Berndt Josef Wulf (wulf at ping.net.au)
 *
 * This shared library provides an API for communicating
 * via serial interface to an FT-1000D using the "CAT" interface
 *
 *
 * $Id: ft1000d.c,v 1.1 2004-08-17 20:07:20 fillods Exp $
 *
 *
 *  This library is free software; you can redistribute it and/or
 *  modify it under the terms of the GNU Lesser General Public
 *  License as published by the Free Software Foundation; either
 *  version 2 of the License, or (at your option) any later version.
 *
 *  This library is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 *  Lesser General Public License for more details.
 *
 *  You should have received a copy of the GNU Lesser General Public
 *  License along with this library; if not, write to the Free Software
 *  Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA 02111-1307  USA
 *
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <stdlib.h>
#include <string.h>  /* String function definitions */
#include <unistd.h>  /* UNIX standard function definitions */

#include "hamlib/rig.h"
#include "bandplan.h"
#include "serial.h"
#include "misc.h"
#include "yaesu.h"
#include "ft990.h"

/* Receiver caps */

#define FT1000D_ALL_RX_MODES (RIG_MODE_LSB|RIG_MODE_USB|RIG_MODE_CW|RIG_MODE_AM|RIG_MODE_FM|RIG_MODE_RTTY|RIG_MODE_RTTYR)
#define FT1000D_SSB_CW_RX_MODES (RIG_MODE_CW|RIG_MODE_USB|RIG_MODE_LSB)
#define FT1000D_RTTY_RX_MODES (RIG_MODE_RTTY|RIG_MODE_RTTYR)
#define FT1000D_AM_RX_MODES (RIG_MODE_AM)
#define FT1000D_FM_RX_MODES (RIG_MODE_FM)


/* TX caps */

#define FT1000D_OTHER_TX_MODES (RIG_MODE_CW|RIG_MODE_USB|RIG_MODE_LSB|RIG_MODE_RTTY|RIG_MODE_RTTYR|RIG_MODE_FM) /* 100 W class */
#define FT1000D_AM_TX_MODES (RIG_MODE_AM )    /* set 25W max */


/* Other features */

#define FT1000D_VFO_ALL (RIG_VFO_A|RIG_VFO_B)
#define FT1000D_ANTS 0

/* Timing values in mS */

#define FT1000D_WRITE_DELAY               50


/* Delay sequential fast writes */

#define FT1000D_POST_WRITE_DELAY          5


#define FT1000D_MEM_CAP {          \
                .freq = 1,       \
                .mode = 1,       \
                .width = 1,      \
                .rit = 1,        \
                .xit = 1,        \
                .rptr_shift = 1, \
                .flags = 1,      \
}

/*
 * FT1000D rigs capabilities.
 *
 * Right now, this backend is a clone of the FT990 backend.
 *
 * If someone with an FT1000D can test and confirm hamlib working using the
 * existing FT990 support (-m 116) we perhaps could add support for this rig.
 * According to the documentation at hand, the protocol for the FT1000D is, 
 * with the exception of dual VFO operation, identical to the FT990.
 *
 * Essentially the differences are:
 * - additional commands concerning dual band operation
 * - additional 8 channels in update packet giving a total of 1636 bytes
 * - flags byte 1 bit 1 - dual receive operation instead of VFO B in use
 * - flags byte 1 bit 2 - Antenna now tuning instead of fast tuning rate
 * - flags byte 1 bit 4 - VFO B in use (RX or TX) instead of antenna tuning
 * - flags byte 3 bit 7 - Sub VFO tuning know locked instead of sidetone active
 */

const struct rig_caps ft1000d_caps = {
  .rig_model =          RIG_MODEL_FT1000D,
  .model_name =         "FT-1000D",
  .mfg_name =           "Yaesu",
  .version =            "0.0.5",
  .copyright =          "LGPL",
  .status =             RIG_STATUS_ALPHA,
  .rig_type =           RIG_TYPE_TRANSCEIVER,
  .ptt_type =           RIG_PTT_RIG,
  .dcd_type =           RIG_DCD_NONE,
  .port_type =          RIG_PORT_SERIAL,
  .serial_rate_min =    4800,
  .serial_rate_max =    4800,
  .serial_data_bits =   8,
  .serial_stop_bits =   2,
  .serial_parity =      RIG_PARITY_NONE,
  .serial_handshake =   RIG_HANDSHAKE_NONE,
  .write_delay =        FT1000D_WRITE_DELAY,
  .post_write_delay =   FT1000D_POST_WRITE_DELAY,
  .timeout =            2000,
  .retry =              0,
  .has_get_func =       RIG_FUNC_LOCK | RIG_FUNC_TUNER | RIG_FUNC_MON,
  .has_set_func =       RIG_FUNC_LOCK | RIG_FUNC_TUNER,
  .has_get_level =      RIG_LEVEL_STRENGTH | RIG_LEVEL_SWR | RIG_LEVEL_ALC | \
                        RIG_LEVEL_ALC | RIG_LEVEL_RFPOWER,
  .has_set_level =      RIG_LEVEL_NONE,
  .has_get_parm =       RIG_PARM_NONE,
  .has_set_parm =       RIG_PARM_BACKLIGHT,
  .ctcss_list =         NULL,
  .dcs_list =           NULL,
  .preamp =             { RIG_DBLST_END, },
  .attenuator =         { RIG_DBLST_END, },
  .max_rit =            Hz(9999),
  .max_xit =            Hz(9999),
  .max_ifshift =        Hz(1200),
  .vfo_ops =            RIG_OP_CPY | RIG_OP_FROM_VFO | RIG_OP_TO_VFO |
                        RIG_OP_UP | RIG_OP_DOWN | RIG_OP_TUNE | RIG_OP_TOGGLE,
  .targetable_vfo =     RIG_TARGETABLE_ALL,
  .transceive =         RIG_TRN_OFF,        /* Yaesus have to be polled, sigh */
  .bank_qty =           0,
  .chan_desc_sz =       0,
  .chan_list =          {
                          {1, 99, RIG_MTYPE_MEM, FT1000D_MEM_CAP},
                           RIG_CHAN_END,
                        },
  .rx_range_list1 =     {
    {kHz(100), MHz(30), FT1000D_ALL_RX_MODES, -1, -1, FT1000D_VFO_ALL, FT1000D_ANTS},   /* General coverage + ham */
    RIG_FRNG_END,
  },

  .tx_range_list1 =     {
        FRQ_RNG_HF(1, FT1000D_OTHER_TX_MODES, W(5), W(100), FT1000D_VFO_ALL, FT1000D_ANTS),
        FRQ_RNG_HF(1, FT1000D_AM_TX_MODES, W(2), W(25), FT1000D_VFO_ALL, FT1000D_ANTS), /* AM class */
    RIG_FRNG_END,
  },

  .rx_range_list2 =     {
    {kHz(100), MHz(30), FT1000D_ALL_RX_MODES, -1, -1, FT1000D_VFO_ALL, FT1000D_ANTS},
    RIG_FRNG_END,
  },

  .tx_range_list2 =     {
        FRQ_RNG_HF(2, FT1000D_OTHER_TX_MODES, W(5), W(100), FT1000D_VFO_ALL, FT1000D_ANTS),
        FRQ_RNG_HF(2, FT1000D_AM_TX_MODES, W(2), W(25), FT1000D_VFO_ALL, FT1000D_ANTS), /* AM class */

    RIG_FRNG_END,
  },

  .tuning_steps =       {
    {FT1000D_SSB_CW_RX_MODES, Hz(10)},    /* Normal */
    {FT1000D_SSB_CW_RX_MODES, Hz(100)},   /* Fast */

    {FT1000D_AM_RX_MODES,     Hz(100)},   /* Normal */
    {FT1000D_AM_RX_MODES,     kHz(1)},    /* Fast */

    {FT1000D_FM_RX_MODES,     Hz(100)},   /* Normal */
    {FT1000D_FM_RX_MODES,     kHz(1)},    /* Fast */

    {FT1000D_RTTY_RX_MODES,   Hz(10)},    /* Normal */
    {FT1000D_RTTY_RX_MODES,   Hz(100)},   /* Fast */

    RIG_TS_END,

  },

    /* mode/filter list, .remember =  order matters! */
  .filters =            {
    {RIG_MODE_SSB,  RIG_FLT_ANY}, /* Enable all filters for SSB */
    {RIG_MODE_CW,   RIG_FLT_ANY}, /* Enable all filters for CW */
    {RIG_MODE_AM,   kHz(6)},      /* normal AM filter */
    {RIG_MODE_AM,   kHz(2.4)},    /* AM filter with narrow selection (SSB filter switched in) */
    {RIG_MODE_FM,   kHz(8)},      /* FM standard filter */
    {RIG_MODE_RTTY, RIG_FLT_ANY}, /* Enable all filters for RTTY */
    {RIG_MODE_RTTYR,RIG_FLT_ANY}, /* Enable all filters for Reverse RTTY */
    {RIG_MODE_PKTLSB,RIG_FLT_ANY}, /* Enable all filters for Packet Radio LSB */
    {RIG_MODE_PKTFM,kHz(8)},      /* FM standard filter for Packet Radio FM */
    RIG_FLT_END,
  },

  .priv =               NULL,           /* private data FIXME: */

  .rig_init =           ft990_init,
  .rig_cleanup =        ft990_cleanup,
  .rig_open =           ft990_open,     /* port opened */
  .rig_close =          ft990_close,    /* port closed */

  .set_freq =           ft990_set_freq,
  .get_freq =           ft990_get_freq,
  .set_mode =           ft990_set_mode,
  .get_mode =           ft990_get_mode,
  .set_vfo =            ft990_set_vfo,
  .get_vfo =            ft990_get_vfo,
  .set_ptt =            ft990_set_ptt,
  .get_ptt =            ft990_get_ptt,
  .set_rptr_shift =     ft990_set_rptr_shift,
  .get_rptr_shift =     ft990_get_rptr_shift,
  .set_rptr_offs =      ft990_set_rptr_offs,
  .set_split_vfo =      ft990_set_split_vfo,
  .get_split_vfo =      ft990_get_split_vfo,
  .set_rit =            ft990_set_rit,
  .get_rit =            ft990_get_rit,
  .set_xit =            ft990_set_xit,
  .get_xit =            ft990_get_xit,
  .set_func =           ft990_set_func,
  .get_func =           ft990_get_func,
  .set_parm =           ft990_set_parm,
  .get_level =          ft990_get_level,
  .set_mem =            ft990_set_mem,
  .get_mem =            ft990_get_mem,
  .vfo_op =             ft990_vfo_op,
  .set_channel =        ft990_set_channel,
  .get_channel =        ft990_get_channel,
};


