/*
 * hamlib - (C) Frank Singleton 2000 (javabear at users.sourceforge.net)
 *
 * ft920.c - (C) Frank Singleton 2000 (javabear at users.sourceforge.net)
 *           (C) Nate Bargmann 2002, 2003 (n0nb at arrl.net)
 *           (C) Stephane Fillod 2002 (fillods at users.sourceforge.net)
 *
 * This shared library provides an API for communicating
 * via serial interface to an FT-920 using the "CAT" interface
 * Documentation can be found online at:
 * http://www.yaesu.com/amateur/pdf/manuals/ft_920.pdf
 * pages 86 to 90
 *
 *
 * $Id: ft920.c,v 1.16 2003-04-07 22:42:07 fillods Exp $
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
#include "ft920.h"


/*
 * Functions considered to be Beta code (2002-11-15):
 * set_vfo
 * get_vfo
 * set_freq
 * get_freq
 * set_mode
 * get_mode
 *
 * Functions considered to be Alpha code (2003-01-16):
 * set_split
 * get_split
 * set_split_freq
 * get_split_freq
 * set_split_mode
 * get_split_mode
 * set_rit
 * get_rit
 * set_xit
 * get_xit
 *
 */

/* Private helper function prototypes */

static int ft920_get_update_data(RIG *rig, unsigned char ci, unsigned char rl);

static int ft920_send_static_cmd(RIG *rig, unsigned char ci);

static int ft920_send_dynamic_cmd(RIG *rig, unsigned char ci,
                                  unsigned char p1, unsigned char p2,
                                  unsigned char p3, unsigned char p4);

static int ft920_send_dial_freq(RIG *rig, unsigned char ci, freq_t freq);

static int ft920_send_rit_freq(RIG *rig, unsigned char ci, shortfreq_t rit);
/*
 * Native ft920 cmd set prototypes. These are READ ONLY as each
 * rig instance will copy from these and modify if required.
 * Complete sequences (1) can be read and used directly as a cmd sequence.
 * Incomplete sequences (0) must be completed with extra parameters
 * eg: mem number, or freq etc..
 *
 * TODO: Shorten this static array with parameter substitution -N0NB
 *
 */

static const yaesu_cmd_set_t ncmd[] = {
  { 1, { 0x00, 0x00, 0x00, 0x00, 0x01 } }, /* split = off */
  { 1, { 0x00, 0x00, 0x00, 0x01, 0x01 } }, /* split = on */
  { 0, { 0x00, 0x00, 0x00, 0x00, 0x02 } }, /* recall memory */
  { 0, { 0x00, 0x00, 0x00, 0x00, 0x03 } }, /* memory operations */
  { 1, { 0x00, 0x00, 0x00, 0x00, 0x05 } }, /* select vfo A */
  { 1, { 0x00, 0x00, 0x00, 0x01, 0x05 } }, /* select vfo B */
  { 0, { 0x00, 0x00, 0x00, 0x00, 0x06 } }, /* copy memory data to vfo A */
  { 0, { 0x00, 0x00, 0x00, 0x00, 0x09 } }, /* clarifier operations */
  { 0, { 0x00, 0x00, 0x00, 0x00, 0x0a } }, /* set vfo A freq */
  { 0, { 0x00, 0x00, 0x00, 0x00, 0x0c } }, /* mode set */
  { 0, { 0x00, 0x00, 0x00, 0x00, 0x0e } }, /* update interval/pacing */
  { 1, { 0x00, 0x00, 0x00, 0x01, 0x10 } }, /* Status Update Data--Memory Channel Number (1 byte) */
  { 1, { 0x00, 0x00, 0x00, 0x02, 0x10 } }, /* Status Update Data--Current operating data for VFO/Memory (28 bytes) */
  { 1, { 0x00, 0x00, 0x00, 0x03, 0x10 } }, /* Status Update DATA--VFO A and B Data (28 bytes) */
  { 0, { 0x00, 0x00, 0x00, 0x04, 0x10 } }, /* Status Update Data--Memory Channel Data (14 bytes) P4 = 0x00-0x89 Memory Channel Number */
  { 0, { 0x00, 0x00, 0x00, 0x00, 0x8a } }, /* set vfo B frequency */
  { 1, { 0x00, 0x00, 0x00, 0x00, 0x8c } }, /* VFO A wide filter */
  { 1, { 0x00, 0x00, 0x00, 0x02, 0x8c } }, /* VFO A narrow filter */
  { 1, { 0x00, 0x00, 0x00, 0x80, 0x8c } }, /* VFO B wide filter */
  { 1, { 0x00, 0x00, 0x00, 0x82, 0x8c } }, /* VFO B narrow filter */
  { 1, { 0x00, 0x00, 0x00, 0x01, 0xFA } }, /* Read status flags */
/*  { 0, { 0x00, 0x00, 0x00, 0x00, 0x70 } }, */ /* keyer commands */
/*  { 1, { 0x00, 0x00, 0x00, 0x00, 0x81 } }, */ /* tuner off */
/*  { 1, { 0x00, 0x00, 0x00, 0x01, 0x81 } }, */ /* tuner on */
/*  { 1, { 0x00, 0x00, 0x00, 0x00, 0x82 } }, */ /* tuner start*/

};


/*
 * future - private data
 *
 * FIXME: Does this need to be exposed to the application/frontend through
 * ft920_caps.priv? -N0NB
 */

struct ft920_priv_data {
  unsigned char pacing;                     /* pacing value */
  unsigned int read_update_delay;           /* depends on pacing value */
  vfo_t current_vfo;                        /* active VFO from last cmd */
  unsigned char p_cmd[YAESU_CMD_LENGTH];    /* private copy of 1 constructed CAT cmd */
  yaesu_cmd_set_t pcs[FT920_NATIVE_SIZE];   /* private cmd set */
  unsigned char update_data[FT920_VFO_DATA_LENGTH]; /* returned data--max value, some are less */
};

/*
 * ft920 rigs capabilities.
 * Also this struct is READONLY!
 *
 */

const struct rig_caps ft920_caps = {
  .rig_model =          RIG_MODEL_FT920,
  .model_name =         "FT-920",
  .mfg_name =           "Yaesu",
  .version =            "0.2.1",
  .copyright =          "LGPL",
  .status =             RIG_STATUS_ALPHA,
  .rig_type =           RIG_TYPE_TRANSCEIVER,
  .ptt_type =           RIG_PTT_NONE,
  .dcd_type =           RIG_DCD_NONE,
  .port_type =          RIG_PORT_SERIAL,
  .serial_rate_min =    4800,
  .serial_rate_max =    4800,
  .serial_data_bits =   8,
  .serial_stop_bits =   2,
  .serial_parity =      RIG_PARITY_NONE,
  .serial_handshake =   RIG_HANDSHAKE_NONE,
  .write_delay =        FT920_WRITE_DELAY,
  .post_write_delay =   FT920_POST_WRITE_DELAY,
  .timeout =            2000,
  .retry =              0,
  .has_get_func =       RIG_FUNC_NONE,
  .has_set_func =       RIG_FUNC_NONE,
  .has_get_level =      RIG_LEVEL_NONE,
  .has_set_level =      RIG_LEVEL_NONE,
  .has_get_parm =       RIG_PARM_NONE,
  .has_set_parm =       RIG_PARM_NONE,
  .ctcss_list =         NULL,
  .dcs_list =           NULL,
  .preamp =             { RIG_DBLST_END, },
  .attenuator =         { RIG_DBLST_END, },
  .max_rit =            Hz(9999),
  .max_xit =            Hz(9999),
  .max_ifshift =        Hz(0),
  .targetable_vfo =     RIG_TARGETABLE_ALL,
  .transceive =         RIG_TRN_OFF,        /* Yaesus have to be polled, sigh */
  .bank_qty =           0,
  .chan_desc_sz =       0,
  .chan_list =          { RIG_CHAN_END, },  /* FIXME: memory channel list: 122 (!) */

  .rx_range_list1 =     {
    {kHz(100), MHz(30), FT920_ALL_RX_MODES, -1, -1, FT920_VFO_ALL, FT920_ANTS},   /* General coverage + ham */
    {MHz(48), MHz(56), FT920_ALL_RX_MODES, -1, -1, FT920_VFO_ALL, FT920_ANTS},   /* 6m! */
    RIG_FRNG_END,
  }, /* FIXME:  Are these the correct Region 1 values? */

  .tx_range_list1 =     {
	FRQ_RNG_HF(1, FT920_OTHER_TX_MODES, W(5), W(100), FT920_VFO_ALL, FT920_ANTS),
	FRQ_RNG_HF(1, FT920_AM_TX_MODES, W(2), W(25), FT920_VFO_ALL, FT920_ANTS),	/* AM class */

	FRQ_RNG_6m(1, FT920_OTHER_TX_MODES, W(5), W(100), FT920_VFO_ALL, FT920_ANTS),
	FRQ_RNG_6m(1, FT920_AM_TX_MODES, W(2), W(25), FT920_VFO_ALL, FT920_ANTS),	/* AM class */
    RIG_FRNG_END,
  },

  .rx_range_list2 =     {
    {kHz(100), MHz(30), FT920_ALL_RX_MODES, -1, -1, FT920_VFO_ALL, FT920_ANTS},
    {MHz(48), MHz(56), FT920_ALL_RX_MODES, -1, -1, FT920_VFO_ALL, FT920_ANTS},
    RIG_FRNG_END,
  },

  .tx_range_list2 =     {
	FRQ_RNG_HF(2, FT920_OTHER_TX_MODES, W(5), W(100), FT920_VFO_ALL, FT920_ANTS),
	FRQ_RNG_HF(2, FT920_AM_TX_MODES, W(2), W(25), FT920_VFO_ALL, FT920_ANTS),	/* AM class */

	FRQ_RNG_6m(2, FT920_OTHER_TX_MODES, W(5), W(100), FT920_VFO_ALL, FT920_ANTS),
	FRQ_RNG_6m(2, FT920_AM_TX_MODES, W(2), W(25), FT920_VFO_ALL, FT920_ANTS),	/* AM class */
    RIG_FRNG_END,
  },

  .tuning_steps =       {
    {FT920_SSB_CW_RX_MODES, Hz(10)},    /* Normal */
    {FT920_SSB_CW_RX_MODES, Hz(100)},   /* Fast */

    {FT920_AM_RX_MODES,     Hz(100)},   /* Normal */
    {FT920_AM_RX_MODES,     kHz(1)},    /* Fast */

    {FT920_FM_RX_MODES,     Hz(100)},   /* Normal */
    {FT920_FM_RX_MODES,     kHz(1)},    /* Fast */

    RIG_TS_END,

    /*
     * The FT-920 has a Fine tuning step which increments in 1 Hz steps
     * for SSB_CW_RX_MODES, and 10 Hz steps for AM_RX_MODES and
     * FM_RX_MODES.  It doesn't appear that anything finer than 10 Hz
     * is available through the CAT interface, however. -N0NB
     *
     */
  },

    /* mode/filter list, .remember =  order matters! */
  .filters =            {
    {RIG_MODE_SSB,  kHz(2.4)},  /* standard SSB filter bandwidth */
    {RIG_MODE_CW,   kHz(2.4)},  /* normal CW filter */
    {RIG_MODE_CW,   kHz(0.5)},  /* CW filter with narrow selection (must be installed!) */
    {RIG_MODE_AM,   kHz(15)},   /* normal AM filter (stock radio has no AM filter!) */
    {RIG_MODE_AM,   kHz(2.4)},  /* AM filter with narrow selection (SSB filter switched in) */
    {RIG_MODE_FM,   kHz(12)},   /* FM with optional FM unit */
    {RIG_MODE_WFM,  kHz(12)},   /* WideFM, with optional FM unit. */
    {RIG_MODE_RTTY, kHz(1.8)},  /* Alias of MODE_DATA_L */
    {RIG_MODE_RTTY, kHz(0.5)},  /* Alias of MODE_DATA_LN */

    RIG_FLT_END,
  },

  .priv =               NULL,           /* private data FIXME: */

  .rig_init =           ft920_init,
  .rig_cleanup =        ft920_cleanup,
  .rig_open =           ft920_open,     /* port opened */
  .rig_close =          ft920_close,    /* port closed */

  .set_freq =           ft920_set_freq, /* set freq */
  .get_freq =           ft920_get_freq, /* get freq */
  .set_mode =           ft920_set_mode, /* set mode */
  .get_mode =           ft920_get_mode, /* get mode */
  .set_vfo =            ft920_set_vfo,  /* set vfo */
  .get_vfo =            ft920_get_vfo,  /* get vfo */
  .set_split_vfo =      ft920_set_split_vfo,
  .get_split_vfo =      ft920_get_split_vfo,
  .set_split_freq =     ft920_set_split_freq,
  .get_split_freq =     ft920_get_split_freq,
  .set_split_mode =     ft920_set_split_mode,
  .get_split_mode =     ft920_get_split_mode,
  .set_rit =            ft920_set_rit,
  .get_rit =            ft920_get_rit,
  .set_xit =            ft920_set_xit,
  .get_xit =            ft920_get_xit,

};


/*
 * ************************************
 *
 * Hamlib API functions
 *
 * ************************************
 */

/*
 * _init 
 *
 */

static int ft920_init(RIG *rig) {
  struct ft920_priv_data *priv;
  
  rig_debug(RIG_DEBUG_VERBOSE, "%s called\n", __func__);

  if (!rig)
    return -RIG_EINVAL;
  
  priv = (struct ft920_priv_data *)malloc(sizeof(struct ft920_priv_data));
  if (!priv)                       /* whoops! memory shortage! */
    return -RIG_ENOMEM;
  
  /*
   * Copy native cmd set to private cmd storage area 
   */
  memcpy(priv->pcs, ncmd, sizeof(ncmd));

  /* TODO: read pacing from preferences */
  priv->pacing = FT920_PACING_DEFAULT_VALUE; /* set pacing to minimum for now */
  priv->read_update_delay = FT920_DEFAULT_READ_TIMEOUT; /* set update timeout to safe value */
  priv->current_vfo =  RIG_VFO_A;  /* default to VFO_A */
  rig->state.priv = (void *)priv;
  
  return RIG_OK;
}


/*
 * ft920_cleanup routine
 * the serial port is closed by the frontend
 *
 */

static int ft920_cleanup(RIG *rig) {

  rig_debug(RIG_DEBUG_VERBOSE, "%s called\n", __func__);

  if (!rig)
    return -RIG_EINVAL;
  
  if (rig->state.priv)
    free(rig->state.priv);
  rig->state.priv = NULL;
  
  return RIG_OK;
}


/*
 * ft920_open  routine
 * 
 */

static int ft920_open(RIG *rig) {
  struct rig_state *rig_s;
 
  rig_debug(RIG_DEBUG_VERBOSE, "%s called\n", __func__);

  if (!rig)
    return -RIG_EINVAL;

  rig_s = &rig->state;

  rig_debug(RIG_DEBUG_TRACE, "%s: write_delay = %i msec\n",
            __func__, rig_s->rigport.write_delay);
  rig_debug(RIG_DEBUG_TRACE, "%s: post_write_delay = %i msec\n",
            __func__, rig_s->rigport.post_write_delay);

   /* TODO */

  return RIG_OK;
}


/*
 * ft920_close  routine
 * 
 */

static int ft920_close(RIG *rig) {

  rig_debug(RIG_DEBUG_VERBOSE, "%s called\n", __func__);

  if (!rig)
    return -RIG_EINVAL;

  return RIG_OK;
}


/*
 * set freq for a given VFO
 *
 * If vfo is set to RIG_VFO_CUR then vfo from priv_data is used.
 *
 */

static int ft920_set_freq(RIG *rig, vfo_t vfo, freq_t freq) {
  struct ft920_priv_data *priv;
  int err, cmd_index;

  rig_debug(RIG_DEBUG_VERBOSE, "%s called\n", __func__);

  if (!rig)
    return -RIG_EINVAL;

  priv = (struct ft920_priv_data *)rig->state.priv;

  rig_debug(RIG_DEBUG_TRACE, "%s: passed vfo = 0x%02x\n", __func__, vfo);
  rig_debug(RIG_DEBUG_TRACE, "%s: passed freq = %lli Hz\n", __func__, freq);

  if (vfo == RIG_VFO_CURR) {
    vfo = priv->current_vfo;    /* from previous vfo cmd */
    rig_debug(RIG_DEBUG_TRACE, "%s: priv->current_vfo = 0x%02x\n",
              __func__, vfo);
  }

  switch(vfo) {
  case RIG_VFO_A:               /* force main display to VFO */
  case RIG_VFO_VFO:
    err = ft920_set_vfo(rig, RIG_VFO_A);
    if (err != RIG_OK)
      return err;
  case RIG_VFO_MEM:             /* MEM TUNE or user doesn't care */
  case RIG_VFO_MAIN:
    cmd_index = FT920_NATIVE_VFO_A_FREQ_SET;
    break;
  case RIG_VFO_B:
  case RIG_VFO_SUB:
    cmd_index = FT920_NATIVE_VFO_B_FREQ_SET;
    break;
  default:
    return -RIG_EINVAL;         /* sorry, unsupported VFO */
  }
  rig_debug(RIG_DEBUG_TRACE, "%s: set cmd_index = 0x%02x\n",
            __func__, cmd_index);

  err = ft920_send_dial_freq(rig, cmd_index, freq);
  if (err != RIG_OK)
    return err;

  return RIG_OK;
}


/*
 * Return Freq for a given VFO
 *
 */

static int ft920_get_freq(RIG *rig, vfo_t vfo, freq_t *freq) {
  struct ft920_priv_data *priv;
  unsigned char *p;
  unsigned char offset;
  freq_t f;
  int err, cmd_index;

  rig_debug(RIG_DEBUG_VERBOSE, "%s called\n", __func__);
  rig_debug(RIG_DEBUG_TRACE, "%s: passed vfo = 0x%02x\n", __func__, vfo);

  if (!rig)
    return -RIG_EINVAL;
  
  priv = (struct ft920_priv_data *)rig->state.priv;

  if (vfo == RIG_VFO_CURR) {
    vfo = priv->current_vfo;    /* from previous vfo cmd */
    rig_debug(RIG_DEBUG_TRACE,
              "%s: priv->current_vfo = 0x%02x\n", __func__, vfo);
  }

  switch(vfo) {
  case RIG_VFO_A:
  case RIG_VFO_VFO:
    cmd_index = FT920_NATIVE_VFO_DATA;
    offset = FT920_SUMO_VFO_A_FREQ;
    break;
  case RIG_VFO_B:
  case RIG_VFO_SUB:
    cmd_index = FT920_NATIVE_OP_DATA;
    offset = FT920_SUMO_VFO_B_FREQ;
    break;
  case RIG_VFO_MEM:
  case RIG_VFO_MAIN:
    cmd_index = FT920_NATIVE_OP_DATA;
    offset = FT920_SUMO_DISPLAYED_FREQ;
    break;
  default:
    return -RIG_EINVAL;         /* sorry, wrong VFO */
  }
  err = ft920_get_update_data(rig, cmd_index, FT920_VFO_DATA_LENGTH);
  if (err != RIG_OK)
    return err;
  p = &priv->update_data[offset];

  /* big endian integer */
  f = (((((p[0]<<8) + p[1])<<8) + p[2])<<8) + p[3];

  rig_debug(RIG_DEBUG_TRACE,
            "%s: freq = %lli Hz for vfo 0x%02x\n", __func__, f, vfo);

  *freq = f;                    /* return displayed frequency */

  return RIG_OK;
}


/*
 * set mode and passband: eg AM, CW etc for a given VFO
 *
 * If vfo is set to RIG_VFO_CUR then vfo from priv_data is used.
 *
 */

static int ft920_set_mode(RIG *rig, vfo_t vfo, rmode_t mode,
                          pbwidth_t width ) {
  struct ft920_priv_data *priv;
  unsigned char cmd_index;      /* index of sequence to send */
  unsigned char mode_parm;      /* mode parameter */
  int err;

  rig_debug(RIG_DEBUG_VERBOSE, "%s called\n", __func__);

  if (!rig)
    return -RIG_EINVAL;

  rig_debug(RIG_DEBUG_TRACE, "%s: passed vfo = 0x%02x\n", __func__, vfo);
  rig_debug(RIG_DEBUG_TRACE, "%s: passed mode = %i\n", __func__, mode);
  rig_debug(RIG_DEBUG_TRACE, "%s: passed width = %li Hz\n",
            __func__, width);

  priv = (struct ft920_priv_data *)rig->state.priv;

  if (vfo == RIG_VFO_CURR) {
    vfo = priv->current_vfo;    /* from previous vfo cmd */
    rig_debug(RIG_DEBUG_TRACE,
              "%s: priv->current_vfo  = 0x%02x\n", __func__, vfo);
  }

  /* translate mode from generic to ft920 specific */
  switch(vfo) {
  case RIG_VFO_A:               /* force to VFO */
  case RIG_VFO_VFO:
    err = ft920_set_vfo(rig, RIG_VFO_A);
    if (err != RIG_OK)
      return err;
  case RIG_VFO_MEM:             /* MEM TUNE or user doesn't care */
  case RIG_VFO_MAIN:
    switch(mode) {
    case RIG_MODE_AM:
      mode_parm = MODE_SET_A_AM_W;
      break;
    case RIG_MODE_CW:
      mode_parm = MODE_SET_A_CW_U;
      break;
    case RIG_MODE_USB:
      mode_parm = MODE_SET_A_USB;
      break;
    case RIG_MODE_LSB:
      mode_parm = MODE_SET_A_LSB;
      break;
    case RIG_MODE_FM:
      mode_parm = MODE_SET_A_FM_W;
      break;
    case RIG_MODE_RTTY:
      mode_parm = MODE_SET_A_DATA_L;
      break;
    default:
      return -RIG_EINVAL;       /* sorry, wrong MODE */
    }
    break;

  /* Now VFO B */
  case RIG_VFO_B:
  case RIG_VFO_SUB:
    switch(mode) {
    case RIG_MODE_AM:
      mode_parm = MODE_SET_B_AM_W;
      break;
    case RIG_MODE_CW:
      mode_parm = MODE_SET_B_CW_U;
      break;
    case RIG_MODE_USB:
      mode_parm = MODE_SET_B_USB;
      break;
    case RIG_MODE_LSB:
      mode_parm = MODE_SET_B_LSB;
      break;
    case RIG_MODE_FM:
      mode_parm = MODE_SET_B_FM_W;
      break;
    case RIG_MODE_RTTY:
      mode_parm = MODE_SET_B_DATA_L;
      break;
    default:
      return -RIG_EINVAL;
    }
    break;
  default:
    return -RIG_EINVAL;         /* sorry, wrong VFO */
  }

  /*
   * Now set width (shamelessly stolen from ft847.c and then butchered :)
   * The FT-920 doesn't appear to support narrow width in USB or LSB modes
   *
   * Yeah, it's ugly... -N0NB
   *
   */
  if (width == RIG_PASSBAND_NORMAL ||
      width == rig_passband_normal(rig, mode)) {
    switch(vfo) {
    case RIG_VFO_A:
    case RIG_VFO_VFO:
    case RIG_VFO_MEM:
    case RIG_VFO_MAIN:
      cmd_index = FT920_NATIVE_VFO_A_PASSBAND_WIDE;
      break;
    case RIG_VFO_B:
    case RIG_VFO_SUB:
      cmd_index = FT920_NATIVE_VFO_B_PASSBAND_WIDE;
      break;
    }
  } else {
    if (width == rig_passband_narrow(rig, mode)) {
      switch(mode) {
      case RIG_MODE_CW:
      case RIG_MODE_AM:
      case RIG_MODE_FM:
      case RIG_MODE_RTTY:
        switch(vfo) {
        case RIG_VFO_A:
        case RIG_VFO_VFO:
        case RIG_VFO_MEM:
        case RIG_VFO_MAIN:
          cmd_index = FT920_NATIVE_VFO_A_PASSBAND_NAR;
          break;
        case RIG_VFO_B:
        case RIG_VFO_SUB:
          cmd_index = FT920_NATIVE_VFO_B_PASSBAND_NAR;
          break;
        }
        break;
      default:
        return -RIG_EINVAL; /* Invalid mode, how can caller know? */
      }
    } else {
      if (width != RIG_PASSBAND_NORMAL &&
          width != rig_passband_normal(rig, mode)) {
        return -RIG_EINVAL; /* Invalid width, how can caller know? */
      }
    }
  }

  rig_debug(RIG_DEBUG_TRACE, "%s: set mode_parm = 0x%02x\n", __func__, mode_parm);
  rig_debug(RIG_DEBUG_TRACE, "%s: set cmd_index = %i\n",
            __func__, cmd_index);

  err = ft920_send_dynamic_cmd(rig, FT920_NATIVE_MODE_SET,
                               mode_parm, 0, 0, 0);
  if (err != RIG_OK)
    return err;

  err = ft920_send_static_cmd(rig, cmd_index);
  if (err != RIG_OK)
    return err;

  return RIG_OK;                /* good */
}


/*
 * get mode : eg AM, CW etc for a given VFO
 *
 */

static int ft920_get_mode(RIG *rig, vfo_t vfo, rmode_t *mode, pbwidth_t *width) {
  struct ft920_priv_data *priv;
  unsigned char mymode, offset;         /* ft920 mode, flag offset */
  int err, cmd_index, norm;

  rig_debug(RIG_DEBUG_VERBOSE, "%s called\n", __func__);

  if (!rig)
    return -RIG_EINVAL;

  rig_debug(RIG_DEBUG_TRACE, "%s: passed vfo = 0x%02x\n", __func__, vfo);

  priv = (struct ft920_priv_data *)rig->state.priv;

  if (vfo == RIG_VFO_CURR) {
    vfo = priv->current_vfo;    /* from previous vfo cmd */
    rig_debug(RIG_DEBUG_TRACE,
              "%s: priv->current_vfo = 0x%02x\n", __func__, vfo);
  }

  switch(vfo) {
  case RIG_VFO_A:
  case RIG_VFO_VFO:
    cmd_index = FT920_NATIVE_VFO_DATA;
    offset = FT920_SUMO_DISPLAYED_MODE;
    break;
  case RIG_VFO_B:
  case RIG_VFO_SUB:
    cmd_index = FT920_NATIVE_VFO_DATA;
    offset = FT920_SUMO_VFO_B_MODE;
    break;
  case RIG_VFO_MEM:
  case RIG_VFO_MAIN:
    cmd_index = FT920_NATIVE_OP_DATA;
    offset = FT920_SUMO_DISPLAYED_MODE;
    break;
  default:
    return -RIG_EINVAL;
  }

  err = ft920_get_update_data(rig, cmd_index, FT920_VFO_DATA_LENGTH);
  if (err != RIG_OK)
    return err;

  mymode = priv->update_data[offset];
  mymode &= MODE_MASK;

  rig_debug(RIG_DEBUG_TRACE, "%s: mymode = 0x%02x\n", __func__, mymode);

  /*
   * translate mode from ft920 to generic.
   *
   * FIXME: FT-920 has 3 DATA modes, LSB, USB, and FM
   * do we need more bit fields in rmode_t? -N0NB
   *
   */
  switch(mymode) {
  case MODE_USBN:               /* not sure this even exists */
    *mode = RIG_MODE_USB;
    norm = FALSE;
    break;
  case MODE_USB:
    *mode = RIG_MODE_USB;
    norm = TRUE;
    break;
  case MODE_LSBN:               /* not sure this even exists */
    *mode = RIG_MODE_LSB;
    norm = FALSE;
    break;
  case MODE_LSB:
    *mode = RIG_MODE_LSB;
    norm = TRUE;
    break;
  case MODE_CW_UN:
  case MODE_CW_LN:
    *mode = RIG_MODE_CW;
    norm = FALSE;
    break;
  case MODE_CW_U:
  case MODE_CW_L:
    *mode = RIG_MODE_CW;
    norm = TRUE;
    break;
  case MODE_AMN:
    *mode = RIG_MODE_AM;
    norm = FALSE;
    break;
  case MODE_AM:
    *mode = RIG_MODE_AM;
    norm = TRUE;
    break;
  case MODE_FMN:
    *mode = RIG_MODE_FM;
    norm = FALSE;
    break;
  case MODE_FM:
    *mode = RIG_MODE_FM;
    norm = TRUE;
    break;
  case MODE_DATA_LN:
    *mode = RIG_MODE_RTTY;
    norm = FALSE;
    break;
  case MODE_DATA_L:
    *mode = RIG_MODE_RTTY;
    norm = TRUE;
    break;
  default:
    return -RIG_EINVAL;         /* Oops! file bug report */
  }

  if (norm) {
    *width = rig_passband_normal(rig, *mode);
  } else {
    *width = rig_passband_narrow(rig, *mode);
  }

  rig_debug(RIG_DEBUG_TRACE, "%s: set mode = %i\n", __func__, *mode);
  rig_debug(RIG_DEBUG_TRACE, "%s: set width = %li Hz\n", __func__, *width);

  return RIG_OK;
}


/*
 * set vfo and store requested vfo for later RIG_VFO_CURR
 * requests.
 *
 */

static int ft920_set_vfo(RIG *rig, vfo_t vfo) {
  struct ft920_priv_data *priv;
  unsigned char cmd_index;      /* index of sequence to send */
  int err;

  rig_debug(RIG_DEBUG_VERBOSE, "%s called\n", __func__);

  if (!rig)
    return -RIG_EINVAL;

  rig_debug(RIG_DEBUG_TRACE, "%s: passed vfo = 0x%02x\n", __func__, vfo);
  
  priv = (struct ft920_priv_data *)rig->state.priv;

  if (vfo == RIG_VFO_CURR) {
    vfo = priv->current_vfo;    /* from previous vfo cmd */
    rig_debug(RIG_DEBUG_TRACE,
              "%s: priv->current_vfo = 0x%02x\n", __func__, vfo);
  }

  switch(vfo) {
  case RIG_VFO_A:
  case RIG_VFO_VFO:
    cmd_index = FT920_NATIVE_VFO_A;
    priv->current_vfo = vfo;    /* update active VFO */
    break;
  case RIG_VFO_B:
    cmd_index = FT920_NATIVE_VFO_B;
    priv->current_vfo = vfo;
    break;
  default:
    return -RIG_EINVAL;         /* sorry, wrong VFO */
  }
  rig_debug(RIG_DEBUG_TRACE, "%s: set cmd_index = %i\n", __func__, cmd_index);
  
  err = ft920_send_static_cmd(rig, cmd_index);
  if (err != RIG_OK)
    return err;

  return RIG_OK;
}


/*
 * get current RX vfo/mem and store requested vfo for
 * later RIG_VFO_CURR requests plus pass the tested vfo/mem
 * back to the frontend.
 *
 */

static int ft920_get_vfo(RIG *rig, vfo_t *vfo) {
  struct ft920_priv_data *priv;
  unsigned char status_0;           /* ft920 status flag 0 */
  unsigned char status_1;           /* ft920 status flag 1 */
  int err;

  rig_debug(RIG_DEBUG_VERBOSE, "%s called\n", __func__);

  if (!rig)
    return -RIG_EINVAL;
  
  priv = (struct ft920_priv_data *)rig->state.priv;

  /* Get flags for VFO status */
  err = ft920_get_update_data(rig, FT920_NATIVE_STATUS_FLAGS,
                              FT920_STATUS_FLAGS_LENGTH);
  if (err != RIG_OK)
    return err;
  
  status_0 = priv->update_data[FT920_SUMO_DISPLAYED_STATUS_0];
  status_0 &= SF_VFOB;             /* get VFO B (sub display) active bits */

  status_1 = priv->update_data[FT920_SUMO_DISPLAYED_STATUS_1];
  status_1 &= SF_VFO_MASK;          /* get VFO/MEM (main display) active bits */
  
  rig_debug(RIG_DEBUG_TRACE,
            "%s: vfo status_0 = 0x%02x\n", __func__, status_0);
  rig_debug(RIG_DEBUG_TRACE,
            "%s: vfo status_1 = 0x%02x\n", __func__, status_1);

  /* 
   * translate vfo status from ft920 to generic.
   *
   * Figuring out whether VFO B is the active RX vfo is tough as
   * Status Flag 0 bits 0 & 1 contain this information.  Testing
   * Status Flag 1 only gives us the state of the main display.
   *
   */
  switch (status_0) {
  case SF_VFOB:
    *vfo = RIG_VFO_B;
    priv->current_vfo = RIG_VFO_B;
    break;
  case SF_SPLITB:               /* Split operation, RX on VFO B */
    *vfo = RIG_VFO_B;
    priv->current_vfo = RIG_VFO_B;
    break;
  }
    /*
     * Okay now test for the active MEM/VFO status of the main display
     *
     */
  switch (status_1) {
  case SF_QMB:
  case SF_MT:
  case SF_MR:
    *vfo = RIG_VFO_MEM;
    priv->current_vfo = RIG_VFO_MEM;
    break;
  case SF_VFO:
    switch (status_0){
    case SF_SPLITA:             /* Split operation, RX on VFO A */
      *vfo = RIG_VFO_A;
      priv->current_vfo = RIG_VFO_A;
      break;
    case SF_VFOA:
      *vfo = RIG_VFO_A;
      priv->current_vfo = RIG_VFO_A;
      break;
    }
    break;
  default:                      /* Oops! */
    return -RIG_EINVAL;         /* sorry, wrong current VFO */
  }
  rig_debug(RIG_DEBUG_TRACE, "%s: set vfo = 0x%02x\n", __func__, *vfo);

  return RIG_OK;

}


/*
 * set the '920 into split TX/RX mode
 *
 * VFO cannot be set as the set split on command only changes the
 * TX to the sub display.  Setting split off returns the TX to the
 * main display.
 *
 */

static int ft920_set_split_vfo(RIG *rig, vfo_t vfo, split_t split, vfo_t tx_vfo) {
  unsigned char cmd_index;
  int err;

  rig_debug(RIG_DEBUG_VERBOSE, "%s called\n", __func__);

  if (!rig)
    return -RIG_EINVAL;

  rig_debug(RIG_DEBUG_TRACE, "%s: passed vfo = 0x%02x\n", __func__, vfo);
  rig_debug(RIG_DEBUG_TRACE, "%s: passed split = 0x%02x\n", __func__, split);
  
  switch(split) {
  case RIG_SPLIT_OFF:
    cmd_index = FT920_NATIVE_SPLIT_OFF;
    break;
  case RIG_SPLIT_ON:
    cmd_index = FT920_NATIVE_SPLIT_ON;
    break;
  default:
    return -RIG_EINVAL;
  }

  err = ft920_send_static_cmd(rig, cmd_index);
  if (err != RIG_OK)
    return err;

  return RIG_OK;
}


/*
 * Get whether the '920 is in split mode
 *
 * vfo value is not used
 *
 */

static int ft920_get_split_vfo(RIG *rig, vfo_t vfo, split_t *split, vfo_t *tx_vfo) {
  struct ft920_priv_data *priv;
  unsigned char status_0;
  int err;

  rig_debug(RIG_DEBUG_VERBOSE, "%s called\n", __func__);

  if (!rig)
    return -RIG_EINVAL;

  rig_debug(RIG_DEBUG_TRACE, "%s: passed vfo = 0x%02x\n", __func__, vfo);

  priv = (struct ft920_priv_data *)rig->state.priv;

  /* Get flags for VFO split status */
  err = ft920_get_update_data(rig, FT920_NATIVE_STATUS_FLAGS,
                              FT920_STATUS_FLAGS_LENGTH);
  if (err != RIG_OK)
    return err;
  
  status_0 = priv->update_data[FT920_SUMO_DISPLAYED_STATUS_0];
  status_0 &= SF_VFOB;             /* get VFO B (sub display) active bits */

  rig_debug(RIG_DEBUG_TRACE,
            "%s: split status_0 = 0x%02x\n", __func__, status_0);

  switch (status_0) {
  case SF_SPLITA:
  case SF_SPLITB:
    *split = RIG_SPLIT_ON;
    break;
  case SF_VFOA:
  case SF_VFOB:
    *split = RIG_SPLIT_OFF;
    break;
  default:
    return RIG_EINVAL;
  }

    return RIG_OK;
}


/*
 * set the '920 split TX freq
 *
 * Right now this is just a pass-through function and depends on the
 * user app to "know" which VFO to set.  Does this need to determine
 * the split direction and set accordingly?
 */

static int ft920_set_split_freq(RIG *rig, vfo_t vfo, freq_t tx_freq) {
  int err;

  rig_debug(RIG_DEBUG_VERBOSE, "%s called\n", __func__);

  if (!rig)
    return -RIG_EINVAL;

  err = ft920_set_freq(rig, vfo, tx_freq);
  if (err != RIG_OK)
    return err;

  return RIG_OK;
}


/*
 * get the '920 split TX freq
 *
 * Right now this is just a pass-through function and depends on the
 * user app to "know" which VFO to set.  Does this need to determine
 * the split direction and set accordingly?
 */

static int ft920_get_split_freq(RIG *rig, vfo_t vfo, freq_t *tx_freq) {
  int err;

  rig_debug(RIG_DEBUG_VERBOSE, "%s called\n", __func__);

  if (!rig)
    return -RIG_EINVAL;

  err = ft920_get_freq(rig, vfo, tx_freq);
  if (err != RIG_OK)
    return err;

  return RIG_OK;
}


/*
 * set the '920 split TX mode
 *
 * Right now this is just a pass-through function and depends on the
 * user app to "know" which VFO to set.  Does this need to determine
 * the split direction and set accordingly?
 */

static int ft920_set_split_mode(RIG *rig, vfo_t vfo, rmode_t tx_mode,
                                pbwidth_t tx_width) {
  int err;

  rig_debug(RIG_DEBUG_VERBOSE, "%s called\n", __func__);

  if (!rig)
    return -RIG_EINVAL;

  err = ft920_set_mode(rig, vfo, tx_mode, tx_width);
  if (err != RIG_OK)
    return err;

  return RIG_OK;
}


/*
 * get the '920 split TX mode
 *
 * Right now this is just a pass-through function and depends on the
 * user app to "know" which VFO to set.  Does this need to determine
 * the split direction and set accordingly?
 */

static int ft920_get_split_mode(RIG *rig, vfo_t vfo, rmode_t *tx_mode,
                                pbwidth_t *tx_width) {
  int err;

  rig_debug(RIG_DEBUG_VERBOSE, "%s called\n", __func__);

  if (!rig)
    return -RIG_EINVAL;

  err = ft920_get_mode(rig, vfo, tx_mode, tx_width);
  if (err != RIG_OK)
    return err;

  return RIG_OK;
}


/*
 * set the RIT offset
 *
 * vfo is ignored as RIT cannot be changed on sub VFO
 *
 * FIXME:   Should rig be forced into VFO mode if RIG_VFO_A or
 *          RIG_VFO_VFO is received?
 *
 * VFO and MEM rit values are independent.  The sub display carries
 * an RIT value only if A<>B button is pressed or set_vfo is called with
 * RIG_VFO_B and the main display has an RIT value.
 */

static int ft920_set_rit(RIG *rig, vfo_t vfo, shortfreq_t rit) {
  unsigned char offset;
  int err;

  rig_debug(RIG_DEBUG_VERBOSE, "%s called\n", __func__);

  if (!rig)
    return -RIG_EINVAL;

  if (rit < -9999 || rit > 9999)
    return -RIG_EINVAL;

  rig_debug(RIG_DEBUG_TRACE, "%s: passed vfo = 0x%02x\n", __func__, vfo);
  rig_debug(RIG_DEBUG_TRACE, "%s: passed rit = %li\n", __func__, rit);

  if (rit == 0) {
    offset = CLAR_RX_OFF;
  } else {
    offset = CLAR_RX_ON;
  }
  rig_debug(RIG_DEBUG_TRACE, "%s: set offset = 0x%02x\n", __func__, offset);

  err = ft920_send_dynamic_cmd(rig, FT920_NATIVE_CLARIFIER_OPS,
                               offset, 0, 0, 0);
  if (err != RIG_OK)
    return err;

  err = ft920_send_rit_freq(rig, FT920_NATIVE_CLARIFIER_OPS, rit);
  if (err != RIG_OK)
    return err;

  return RIG_OK;
}


/*
 * Get the RIT offset
 * Value of vfo is ignored as it's not needed
 * Rig returns offset as hex from 0x0000 to 0x270f for 0 to +9.999 kHz
 * and 0xffff to 0xd8f1 for -1 to -9.999 kHz
 *
 * VFO and MEM rit values are independent.  The sub display carries
 * an RIT value only if A<>B button is pressed or set_vfo is called with
 * RIG_VFO_B and the main display has an RIT value.
 */

static int ft920_get_rit(RIG *rig, vfo_t vfo, shortfreq_t *rit) {
  struct ft920_priv_data *priv;
  unsigned char *p;
  unsigned char offset;
  shortfreq_t f;
  int err, cmd_index;

  rig_debug(RIG_DEBUG_VERBOSE, "%s called\n", __func__);

  if (!rig)
    return -RIG_EINVAL;
  
  rig_debug(RIG_DEBUG_TRACE, "%s: passed vfo = 0x%02x\n", __func__, vfo);

  priv = (struct ft920_priv_data *)rig->state.priv;

  if (vfo == RIG_VFO_CURR) {
    vfo = priv->current_vfo;    /* from previous vfo cmd */
    rig_debug(RIG_DEBUG_TRACE,
              "%s: priv->current_vfo = 0x%02x\n", __func__, vfo);
  }

  switch(vfo) {
  case RIG_VFO_MEM:
  case RIG_VFO_MAIN:
    cmd_index = FT920_NATIVE_OP_DATA;
    offset = FT920_SUMO_DISPLAYED_CLAR;
    break;
  case RIG_VFO_A:
  case RIG_VFO_VFO:
    cmd_index = FT920_NATIVE_VFO_DATA;
    offset = FT920_SUMO_VFO_A_CLAR;
    break;
  case RIG_VFO_B:
  case RIG_VFO_SUB:
    cmd_index = FT920_NATIVE_VFO_DATA;
    offset = FT920_SUMO_VFO_B_CLAR;
    break;
  default:
    return RIG_EINVAL;
  }
  rig_debug(RIG_DEBUG_TRACE, "%s: set cmd_index = %i\n", __func__, cmd_index);
  rig_debug(RIG_DEBUG_TRACE, "%s: set offset = 0x%02x\n", __func__, offset);

  err = ft920_get_update_data(rig, cmd_index, FT920_VFO_DATA_LENGTH);
  if (err != RIG_OK)
    return err;

  p = &priv->update_data[offset];

  /* big endian integer */
  f = (p[0]<<8) + p[1];
  if (f > 0xd8f0)               /* 0xd8f1 to 0xffff is negative offset */
    f = ~(0xffff - f);

  rig_debug(RIG_DEBUG_TRACE, "%s: read freq = %li Hz\n", __func__, f);

  *rit = f;                     /* store clarifier frequency */

  return RIG_OK;
}


/*
 * set the XIT offset
 *
 * vfo is ignored as XIT cannot be changed on sub VFO
 *
 * FIXME:   Should rig be forced into VFO mode if RIG_VFO_A or
 *          RIG_VFO_VFO is received?
 */

static int ft920_set_xit(RIG *rig, vfo_t vfo, shortfreq_t xit) {
  unsigned char offset;
  int err;

  rig_debug(RIG_DEBUG_VERBOSE, "%s called\n", __func__);

  if (!rig)
    return -RIG_EINVAL;

  if (xit < -9999 || xit > 9999)
    return -RIG_EINVAL;

  rig_debug(RIG_DEBUG_TRACE, "%s: passed vfo = 0x%02x\n", __func__, vfo);
  rig_debug(RIG_DEBUG_TRACE, "%s: passed xit = %li\n", __func__, xit);

  if (xit == 0) {
    offset = CLAR_TX_OFF;
  } else {
    offset = CLAR_TX_ON;
  }
  rig_debug(RIG_DEBUG_TRACE, "%s: set offset = 0x%02x\n", __func__, offset);

  err = ft920_send_dynamic_cmd(rig, FT920_NATIVE_CLARIFIER_OPS,
                               offset, 0, 0, 0);
  if (err != RIG_OK)
    return err;

  err = ft920_send_rit_freq(rig, FT920_NATIVE_CLARIFIER_OPS, xit);
  if (err != RIG_OK)
    return err;

  return RIG_OK;
}


/*
 * Get the XIT offset
 * Value of vfo is ignored as it's not needed
 * Rig returns offset as hex from 0x0000 to 0x270f for 0 to +9.999 kHz
 * and 0xffff to 0xd8f1 for -1 to -9.999 kHz
 */

static int ft920_get_xit(RIG *rig, vfo_t vfo, shortfreq_t *xit) {
  int err;

  rig_debug(RIG_DEBUG_VERBOSE, "%s called\n", __func__);

  if (!rig)
    return -RIG_EINVAL;

  err = ft920_get_rit(rig, vfo, xit);   /* abuse get_rit and store in *xit */
  if (err != RIG_OK)
    return err;

  return RIG_OK;
}


/*
 * ************************************
 *
 * Private functions to ft920 backend
 *
 * ************************************
 */


/*
 * Private helper function. Retrieves update data from rig.
 * using pacing value and buffer indicated in *priv struct.
 * Extended to be command agnostic as 920 has several ways to
 * get data and several ways to return it.
 *
 * Need to use this when doing ft920_get_* stuff
 *
 * Arguments:   *rig    Valid RIG instance
 *              ci      command index
 *              rl      expected length of returned data in octets
 *
 * Returns:     RIG_OK if all called functions are successful,
 *              otherwise returns error from called functiion
 */

static int ft920_get_update_data(RIG *rig, unsigned char ci, unsigned char rl) {
  struct rig_state *rig_s;
  struct ft920_priv_data *priv;
  int n;                        /* for read_  */
  int err;

  rig_debug(RIG_DEBUG_VERBOSE, "%s called\n", __func__);

  if (!rig)
    return -RIG_EINVAL;
  
  priv = (struct ft920_priv_data *)rig->state.priv;
  rig_s = &rig->state;

  /* Copy native cmd PACING to private cmd storage area */
  memcpy(&priv->p_cmd, &ncmd[FT920_NATIVE_PACING].nseq, YAESU_CMD_LENGTH);

  /* get pacing value, and store in private cmd */
  priv->p_cmd[P1] = priv->pacing;

  rig_debug(RIG_DEBUG_TRACE,
            "%s: read pacing = %i\n", __func__, priv->pacing);

  err = write_block(&rig_s->rigport, (unsigned char *) priv->p_cmd,
                    YAESU_CMD_LENGTH);
  if (err != RIG_OK)
    return err;

  err = ft920_send_static_cmd(rig, ci);
  if (err != RIG_OK)
    return err;

  n = read_block(&rig_s->rigport, priv->update_data, rl);
  if (n < 0)
    return n;                   /* die returning read_block error */

  rig_debug(RIG_DEBUG_TRACE, "%s: read %i bytes\n", __func__, n);

  return RIG_OK;
}


/*
 * Private helper function to send a complete command sequence.
 *
 * TODO: place variant of this in yaesu.c
 *
 * Arguments:   *rig    Valid RIG instance
 *              ci      Command index of the pcs struct
 *
 * Returns:     RIG_OK if all called functions are successful,
 *              otherwise returns error from called functiion
 */

static int ft920_send_static_cmd(RIG *rig, unsigned char ci) {
  struct rig_state *rig_s;
  struct ft920_priv_data *priv;
  int err;
 
  rig_debug(RIG_DEBUG_VERBOSE, "%s called\n", __func__);

  if (!rig)
    return -RIG_EINVAL;

  priv = (struct ft920_priv_data *)rig->state.priv;
  rig_s = &rig->state;
  
  if (!priv->pcs[ci].ncomp) {
    rig_debug(RIG_DEBUG_TRACE,
              "%s: Attempt to send incomplete sequence\n", __func__);
    return -RIG_EINVAL;
  }

  err = write_block(&rig_s->rigport, (unsigned char *) priv->pcs[ci].nseq,
                    YAESU_CMD_LENGTH);
  if (err != RIG_OK)
    return err;

  return RIG_OK;
}


/*
 * Private helper function to build and then send a complete command
 * sequence.
 *
 * TODO: place variant of this in yaesu.c
 *
 * Arguments:   *rig    Valid RIG instance
 *              ci      Command index of the pcs struct
 *              p1-p4   Command parameters
 *
 * Returns:     RIG_OK if all called functions are successful,
 *              otherwise returns error from called functiion
 */

static int ft920_send_dynamic_cmd(RIG *rig, unsigned char ci,
                                  unsigned char p1, unsigned char p2,
                                  unsigned char p3, unsigned char p4) {
  struct rig_state *rig_s;
  struct ft920_priv_data *priv;
  int err;
 
  rig_debug(RIG_DEBUG_VERBOSE, "%s called\n", __func__);

  if (!rig)
    return -RIG_EINVAL;

  rig_debug(RIG_DEBUG_TRACE, "%s: passed ci = %i\n", __func__, ci);
  rig_debug(RIG_DEBUG_TRACE,
            "%s: passed p1 = 0x%02x, p2 = 0x%02x, p3 = 0x%02x, p4 = 0x%02x,\n",
            __func__, p1, p2, p3, p4);

  priv = (struct ft920_priv_data *)rig->state.priv;
  if (priv->pcs[ci].ncomp) {
    rig_debug(RIG_DEBUG_TRACE,
              "%s: Attempt to modify complete sequence\n", __func__);
    return -RIG_EINVAL;
  }

  rig_s = &rig->state;
  memcpy(&priv->p_cmd, &ncmd[ci].nseq, YAESU_CMD_LENGTH);

  priv->p_cmd[P1] = p1;         /* ick */
  priv->p_cmd[P2] = p2;
  priv->p_cmd[P3] = p3;
  priv->p_cmd[P4] = p4;

  err = write_block(&rig_s->rigport, (unsigned char *) &priv->p_cmd,
                    YAESU_CMD_LENGTH);
  if (err != RIG_OK)
    return err;

  return RIG_OK;
}


/*
 * Private helper function to build and send a complete command to
 * change the Main or Sub display frequency.
 *
 * TODO: place variant of this in yaesu.c
 *
 * Arguments:   *rig    Valid RIG instance
 *              ci      Command index of the pcs struct
 *              freq    freq_t frequency value
 *
 * Returns:     RIG_OK if all called functions are successful,
 *              otherwise returns error from called functiion
 */

static int ft920_send_dial_freq(RIG *rig, unsigned char ci, freq_t freq) {
  struct rig_state *rig_s;
  struct ft920_priv_data *priv;
  int err;
 
  rig_debug(RIG_DEBUG_VERBOSE, "%s called\n", __func__);

  if (!rig)
    return -RIG_EINVAL;

  rig_debug(RIG_DEBUG_TRACE, "%s: passed ci = %i\n", __func__, ci);
  rig_debug(RIG_DEBUG_TRACE, "%s: passed freq = %lli Hz\n", __func__, freq);

  priv = (struct ft920_priv_data *)rig->state.priv;
  if (priv->pcs[ci].ncomp) {
    rig_debug(RIG_DEBUG_TRACE,
              "%s: Attempt to modify complete sequence\n", __func__);
    return -RIG_EINVAL;
  }

  rig_s = &rig->state;

  /* Copy native cmd freq_set to private cmd storage area */
  memcpy(&priv->p_cmd, &ncmd[ci].nseq, YAESU_CMD_LENGTH);

  /* store bcd format in in p_cmd */
  to_bcd(priv->p_cmd, freq/10, FT920_BCD_DIAL);

  rig_debug(RIG_DEBUG_TRACE,
            "%s: requested freq after conversion = %lli Hz\n",
            __func__, from_bcd(priv->p_cmd, FT920_BCD_DIAL)* 10);

  err = write_block(&rig_s->rigport, (unsigned char *) &priv->p_cmd,
                    YAESU_CMD_LENGTH);
  if (err != RIG_OK)
    return err;

  return RIG_OK;
}


/*
 * Private helper function to build and send a complete command to
 * change the Main or Sub display frequency.
 *
 * TODO: place variant of this in yaesu.c
 *
 * Arguments:   *rig    Valid RIG instance
 *              ci      Command index of the pcs struct
 *              rit     shortfreq_t frequency value
 *              p1      P1 value -- CLAR_SET_FREQ
 *              p2      P2 value -- CLAR_OFFSET_PLUS || CLAR_OFFSET_MINUS
 *
 * Returns:     RIG_OK if all called functions are successful,
 *              otherwise returns error from called functiion
 *
 * Assumes:     rit doesn't exceed tuning limits of rig
 */

static int ft920_send_rit_freq(RIG *rig, unsigned char ci, shortfreq_t rit) {
  struct rig_state *rig_s;
  struct ft920_priv_data *priv;
  unsigned char p1;
  unsigned char p2;
  int err;
 
  rig_debug(RIG_DEBUG_VERBOSE, "%s called\n", __func__);

  if (!rig)
    return -RIG_EINVAL;

  rig_debug(RIG_DEBUG_TRACE, "%s: passed ci = %i\n", __func__, ci);
  rig_debug(RIG_DEBUG_TRACE, "%s: passed rit = %li Hz\n", __func__, rit);

  priv = (struct ft920_priv_data *)rig->state.priv;
  if (priv->pcs[ci].ncomp) {
    rig_debug(RIG_DEBUG_TRACE,
              "%s: Attempt to modify complete sequence\n", __func__);
    return -RIG_EINVAL;
  }

  rig_s = &rig->state;

  p1 = CLAR_SET_FREQ;

  if (rit < 0) {
    rit = labs(rit);            /* get absolute value of rit */
    p2 = CLAR_OFFSET_MINUS;
  } else {
    p2 = CLAR_OFFSET_PLUS;
  }

  /* Copy native cmd clarifier ops to private cmd storage area */
  memcpy(&priv->p_cmd, &ncmd[ci].nseq, YAESU_CMD_LENGTH);

  /* store bcd format in in p_cmd */
  to_bcd(priv->p_cmd, rit/10, FT920_BCD_RIT);

  rig_debug(RIG_DEBUG_TRACE,
            "%s: requested rit after conversion = %li Hz\n",
            __func__, from_bcd(priv->p_cmd, FT920_BCD_RIT)* 10);

  priv->p_cmd[P1] = p1;         /* ick */
  priv->p_cmd[P2] = p2;

  err = write_block(&rig_s->rigport, (unsigned char *) &priv->p_cmd,
                    YAESU_CMD_LENGTH);
  if (err != RIG_OK)
    return err;

  return RIG_OK;
}


