/*
 * hamlib - (C) Frank Singleton 2000 (javabear at users.sourceforge.net)
 *
 * ft920.c - (C) Frank Singleton 2000 (javabear at users.sourceforge.net)
 *           (C) Nate Bargmann 2002 (n0nb at arrl.net)
 *           (C) Stephane Fillod 2002 (fillods at users.sourceforge.net)
 *
 * This shared library provides an API for communicating
 * via serial interface to an FT-920 using the "CAT" interface
 * Documentation can be found online at:
 * http://www.yaesu.com/amateur/pdf/manuals/ft_920.pdf
 * pages 86 to 90
 *
 *
 * $Id: ft920.c,v 1.10 2002-11-23 14:09:20 n0nb Exp $
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


#include <stdlib.h>
#include <stdio.h>   /* Standard input/output definitions */
#include <string.h>  /* String function definitions */
#include <unistd.h>  /* UNIX standard function definitions */
#include <fcntl.h>   /* File control definitions */
#include <errno.h>   /* Error number definitions */
#include <termios.h> /* POSIX terminal control definitions */
#include <sys/ioctl.h>

#include <hamlib/rig.h>
#include <hamlib/riglist.h>
#include <bandplan.h>
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
 */

/* Private helper function prototypes */

static int ft920_get_update_data(RIG *rig, unsigned char ci, unsigned char rl);
static int ft920_send_priv_cmd(RIG *rig, unsigned char ci);

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
/*  { 0, { 0x00, 0x00, 0x00, 0x00, 0x09 } }, */ /* clarifier operations */
/*  { 1, { 0x00, 0x00, 0x00, 0x01, 0x09 } }, */ /* RX clarifier on */
/*  { 1, { 0x00, 0x00, 0x00, 0x80, 0x09 } }, */ /* TX clarifier on */
/*  { 1, { 0x00, 0x00, 0x00, 0x81, 0x09 } }, */ /* TX clarifier on */
  { 0, { 0x00, 0x00, 0x00, 0x00, 0x0a } }, /* set vfo A freq */

  { 1, { 0x00, 0x00, 0x00, 0x00, 0x0c } }, /* vfo A mode set LSB */
  { 1, { 0x00, 0x00, 0x00, 0x01, 0x0c } }, /* vfo A mode set USB */
  { 1, { 0x00, 0x00, 0x00, 0x02, 0x0c } }, /* vfo A mode set CW-USB */
  { 1, { 0x00, 0x00, 0x00, 0x03, 0x0c } }, /* vfo A mode set CW-LSB */
  { 1, { 0x00, 0x00, 0x00, 0x04, 0x0c } }, /* vfo A mode set AM */
  { 1, { 0x00, 0x00, 0x00, 0x06, 0x0c } }, /* vfo A mode set FM */
  { 1, { 0x00, 0x00, 0x00, 0x07, 0x0c } }, /* vfo A mode set FMN */
  { 1, { 0x00, 0x00, 0x00, 0x08, 0x0c } }, /* vfo A mode set DATA-LSB */
  { 1, { 0x00, 0x00, 0x00, 0x0a, 0x0c } }, /* vfo A mode set DATA-USB */
  { 1, { 0x00, 0x00, 0x00, 0x0b, 0x0c } }, /* vfo A mode set DATA-FM */

  { 1, { 0x00, 0x00, 0x00, 0x80, 0x0c } }, /* vfo B mode set LSB */
  { 1, { 0x00, 0x00, 0x00, 0x81, 0x0c } }, /* vfo B mode set USB */
  { 1, { 0x00, 0x00, 0x00, 0x82, 0x0c } }, /* vfo B mode set CW-USB */
  { 1, { 0x00, 0x00, 0x00, 0x83, 0x0c } }, /* vfo B mode set CW-LSB */
  { 1, { 0x00, 0x00, 0x00, 0x84, 0x0c } }, /* vfo B mode set AM */
  { 1, { 0x00, 0x00, 0x00, 0x86, 0x0c } }, /* vfo B mode set FM */
  { 1, { 0x00, 0x00, 0x00, 0x87, 0x0c } }, /* vfo B mode set FMN */
  { 1, { 0x00, 0x00, 0x00, 0x88, 0x0c } }, /* vfo B mode set DATA-LSB */
  { 1, { 0x00, 0x00, 0x00, 0x8a, 0x0c } }, /* vfo B mode set DATA-USB */
  { 1, { 0x00, 0x00, 0x00, 0x8b, 0x0c } }, /* vfo B mode set DATA-FM */

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
  .version =            "0.1.2",
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
  .set_split =          ft920_set_split,
  .get_split =          ft920_get_split,

};


/*
 * _init 
 *
 */

int ft920_init(RIG *rig) {
  struct ft920_priv_data *priv;
  
  rig_debug(RIG_DEBUG_VERBOSE, "ft920: ft920_init called\n");

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

int ft920_cleanup(RIG *rig) {

  rig_debug(RIG_DEBUG_VERBOSE, "ft920: ft920_cleanup called\n");

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

int ft920_open(RIG *rig) {
  struct rig_state *rig_s;
 
  rig_debug(RIG_DEBUG_VERBOSE, "ft920: ft920_open called\n");

  if (!rig)
    return -RIG_EINVAL;

  rig_s = &rig->state;

  rig_debug(RIG_DEBUG_TRACE, "ft920: rig_open: write_delay = %i msec\n",
            rig_s->rigport.write_delay);
  rig_debug(RIG_DEBUG_TRACE, "ft920: rig_open: post_write_delay = %i msec\n",
            rig_s->rigport.post_write_delay);

  
   /* TODO */

  return RIG_OK;
}


/*
 * ft920_close  routine
 * 
 */

int ft920_close(RIG *rig) {

  rig_debug(RIG_DEBUG_VERBOSE, "ft920: ft920_close called\n");

  if (!rig)
    return -RIG_EINVAL;

  return RIG_OK;
}


/*
 * set freq : for a given VFO
 *
 * If vfo is not given or is set to RIG_VFO_CUR then vfo
 * from priv_data is used.
 *
 */

int ft920_set_freq(RIG *rig, vfo_t vfo, freq_t freq) {
  struct rig_state *rig_s;
  struct ft920_priv_data *priv;
  unsigned char *cmd;           /* points to sequence to send */

  rig_debug(RIG_DEBUG_VERBOSE, "ft920: ft920_set_freq called\n");

  if (!rig)
    return -RIG_EINVAL;

  priv = (struct ft920_priv_data *)rig->state.priv;

  rig_s = &rig->state;

  rig_debug(RIG_DEBUG_TRACE, "ft920: set_freq: requested freq = %lli Hz\n", freq);

  if (vfo == RIG_VFO_CURR) {
    vfo = priv->current_vfo;    /* from previous vfo cmd */
    rig_debug(RIG_DEBUG_TRACE,
              "ft920: set_freq: priv->current_vfo = [0x%x]\n", vfo);
  }

  switch(vfo) {
  case RIG_VFO_A:
  case RIG_VFO_MEM:
    /*
     * Copy native cmd freq_set to private cmd storage area 
     */
    memcpy(&priv->p_cmd, &ncmd[FT920_NATIVE_VFO_A_FREQ_SET].nseq,
           YAESU_CMD_LENGTH);
  
    to_bcd(priv->p_cmd, freq/10, 8);    /* store bcd format in in p_cmd */

    rig_debug(RIG_DEBUG_TRACE,
              "ft920: set_freq: requested freq after conversion = %lli Hz\n",
              from_bcd(priv->p_cmd, 8)* 10);
  
    cmd = priv->p_cmd;          /* get native sequence */
    write_block(&rig_s->rigport, cmd, YAESU_CMD_LENGTH);
    break;
  case RIG_VFO_B:
    memcpy(&priv->p_cmd, &ncmd[FT920_NATIVE_VFO_B_FREQ_SET].nseq,
           YAESU_CMD_LENGTH);
  
    to_bcd(priv->p_cmd, freq/10, 8);    /* store bcd format in in p_cmd */

    rig_debug(RIG_DEBUG_TRACE,
              "ft920: set_freq: requested freq after conversion = %lli Hz\n",
              from_bcd(priv->p_cmd, 8)* 10);
  
    cmd = priv->p_cmd;          /* get native sequence */
    write_block(&rig_s->rigport, cmd, YAESU_CMD_LENGTH);
    break;
  default:
    return -RIG_EINVAL;         /* sorry, wrong VFO */
  }

  return RIG_OK;
}


/*
 * Return Freq for a given VFO
 *
 */

int ft920_get_freq(RIG *rig, vfo_t vfo, freq_t *freq) {
  struct ft920_priv_data *priv;
  unsigned char *p;
  freq_t f;

  rig_debug(RIG_DEBUG_VERBOSE, "ft920: get_freq called\n");
  rig_debug(RIG_DEBUG_TRACE, "ft920: get_freq: passed vfo = [0x%x]\n", vfo);

  if (!rig)
    return -RIG_EINVAL;
  
  priv = (struct ft920_priv_data *)rig->state.priv;

  if (vfo == RIG_VFO_CURR) {
    vfo = priv->current_vfo;    /* from previous vfo cmd */
    rig_debug(RIG_DEBUG_TRACE,
              "ft920: get_freq: priv->current_vfo = [0x%x]\n", vfo);
  }

  switch(vfo) {
  case RIG_VFO_A:
    ft920_get_update_data(rig, FT920_NATIVE_VFO_DATA,
                          FT920_VFO_DATA_LENGTH);
    p = &priv->update_data[FT920_SUMO_VFO_A_FREQ];
    rig_debug(RIG_DEBUG_TRACE, "ft920: get_freq: VFO A [0x%x]\n", vfo);
    break;
  case RIG_VFO_B:
    ft920_get_update_data(rig, FT920_NATIVE_OP_DATA,
                          FT920_VFO_DATA_LENGTH);
    p = &priv->update_data[FT920_SUMO_VFO_B_FREQ];
    rig_debug(RIG_DEBUG_TRACE, "ft920: get_freq: VFO B [0x%x]\n", vfo);
    break;
  case RIG_VFO_MEM:
    ft920_get_update_data(rig, FT920_NATIVE_OP_DATA,
                          FT920_VFO_DATA_LENGTH);
    p = &priv->update_data[FT920_SUMO_DISPLAYED_FREQ];
    rig_debug(RIG_DEBUG_TRACE,
              "ft920: get_freq: QMB/MEM TUNE/MEM RECALL [0x%x]\n", vfo);
    break;
  default:
    return -RIG_EINVAL;         /* sorry, wrong VFO */
  }

  /* big endian integer */
  f = (((((p[0]<<8) + p[1])<<8) + p[2])<<8) + p[3];

  rig_debug(RIG_DEBUG_TRACE,
            "ft920: get_freq: freq = %lli Hz for VFO [0x%x]\n", f, vfo);

  *freq = f;                    /* return diplayed frequency */

  return RIG_OK;
}


/*
 * set mode : eg AM, CW etc for a given VFO
 *
 * If vfo is not given or is set to RIG_VFO_CUR then vfo
 * from priv_data is used.
 *
 */

int ft920_set_mode(RIG *rig, vfo_t vfo, rmode_t mode, pbwidth_t width ) {
  struct ft920_priv_data *priv;
  unsigned char cmd_index;      /* index of sequence to send */

  rig_debug(RIG_DEBUG_VERBOSE,"ft920: ft920_set_mode called\n");

  if (!rig)
    return -RIG_EINVAL;

  priv = (struct ft920_priv_data *)rig->state.priv;

  /* frontend sets VFO now , if targetable_vfo = 0 */
  /* is this code needed now? */
#if 0
  ft920_set_vfo(rig, vfo);      /* select VFO first , new API */
#endif

  rig_debug(RIG_DEBUG_TRACE, "ft920: set_mode: generic mode = [0x%x]\n", mode);

  if (vfo == RIG_VFO_CURR) {
    vfo = priv->current_vfo;    /* from previous vfo cmd */
    rig_debug(RIG_DEBUG_TRACE,
              "ft920: set_mode: priv->current_vfo  = [0x%x]\n", vfo);
  }

  /* 
   * translate mode from generic to ft920 specific
   */
  switch(vfo){
  case RIG_VFO_A:
  case RIG_VFO_MEM:
    switch(mode) {
    case RIG_MODE_AM:
      cmd_index = FT920_NATIVE_VFO_A_MODE_SET_AM;
      break;
    case RIG_MODE_CW:
      cmd_index = FT920_NATIVE_VFO_A_MODE_SET_CW_USB;
      break;
    case RIG_MODE_USB:
      cmd_index = FT920_NATIVE_VFO_A_MODE_SET_USB;
      break;
    case RIG_MODE_LSB:
      cmd_index = FT920_NATIVE_VFO_A_MODE_SET_LSB;
      break;
    case RIG_MODE_FM:
      cmd_index = FT920_NATIVE_VFO_A_MODE_SET_FMW;
      break;
    case RIG_MODE_RTTY:
      cmd_index = FT920_NATIVE_VFO_A_MODE_SET_DATA_LSB;
      break;
    default:
      return -RIG_EINVAL;       /* sorry, wrong MODE */
    }
  
    ft920_send_priv_cmd(rig, cmd_index);
    rig_debug(RIG_DEBUG_TRACE,
              "ft920: set_mode: cmd_index = [%i]\n", cmd_index);
  
    /*
     * Now set width (shamelessly stolen from ft847.c and then butchered :)
     *
     * My FT-920 lacks a narrow SSB filter, is this a problem? -N0NB
     *
     * Yeah, it's ugly...
     *
     */
    if (width == RIG_PASSBAND_NORMAL ||
        width == rig_passband_normal(rig, mode)) {
      cmd_index = FT920_NATIVE_VFO_A_PASSBAND_WIDE;
    } else {
      if (width == rig_passband_narrow(rig, mode)) {
        switch(mode) {
        case RIG_MODE_LSB:
        case RIG_MODE_CW:
        case RIG_MODE_AM:
        case RIG_MODE_FM:
        case RIG_MODE_RTTY:
          cmd_index = FT920_NATIVE_VFO_A_PASSBAND_NAR;
          break;
        default:
          return -RIG_EINVAL;   /* sorry, wrong MODE/WIDTH combo  */
        }
      } else {
        if (width != RIG_PASSBAND_NORMAL &&
            width != rig_passband_normal(rig, mode)) {
          return -RIG_EINVAL;   /* sorry, wrong MODE/WIDTH combo  */
        }
      }
    }

    ft920_send_priv_cmd(rig, cmd_index);
    rig_debug(RIG_DEBUG_TRACE,
              "ft920: set_mode: cmd_index = [%i]\n", cmd_index);

    break;

  /* Now VFO B */
  case RIG_VFO_B:
    switch(mode) {
    case RIG_MODE_AM:
      cmd_index = FT920_NATIVE_VFO_B_MODE_SET_AM;
      break;
    case RIG_MODE_CW:
      cmd_index = FT920_NATIVE_VFO_B_MODE_SET_CW_USB;
      break;
    case RIG_MODE_USB:
      cmd_index = FT920_NATIVE_VFO_B_MODE_SET_USB;
      break;
    case RIG_MODE_LSB:
      cmd_index = FT920_NATIVE_VFO_B_MODE_SET_LSB;
      break;
    case RIG_MODE_FM:
      cmd_index = FT920_NATIVE_VFO_B_MODE_SET_FMW;
      break;
    case RIG_MODE_RTTY:
      cmd_index = FT920_NATIVE_VFO_B_MODE_SET_DATA_LSB;
      break;
    default:
      return -RIG_EINVAL;
    }
  
    ft920_send_priv_cmd(rig, cmd_index);
    rig_debug(RIG_DEBUG_TRACE,
              "ft920: set_mode: cmd_index = [%i]\n", cmd_index);
  
    if (width == RIG_PASSBAND_NORMAL ||
        width == rig_passband_normal(rig, mode)) {
      cmd_index = FT920_NATIVE_VFO_B_PASSBAND_WIDE;
    } else {
      if (width == rig_passband_narrow(rig, mode)) {
        switch(mode) {
        case RIG_MODE_LSB:
        case RIG_MODE_CW:
        case RIG_MODE_AM:
        case RIG_MODE_FM:
        case RIG_MODE_RTTY:
          cmd_index = FT920_NATIVE_VFO_B_PASSBAND_NAR;
          break;
        default:
          return -RIG_EINVAL;
        }
      } else {
        if (width != RIG_PASSBAND_NORMAL &&
            width != rig_passband_normal(rig, mode)) {
          return -RIG_EINVAL;
        }
      }
    }

    ft920_send_priv_cmd(rig,cmd_index);
    rig_debug(RIG_DEBUG_TRACE,
              "ft920: set_mode: cmd_index = [%i]\n", cmd_index);

    break;
  default:
    return -RIG_EINVAL;         /* sorry, wrong VFO */
  }

  return RIG_OK;                /* good */
}


/*
 * get mode : eg AM, CW etc for a given VFO
 *
 */

int ft920_get_mode(RIG *rig, vfo_t vfo, rmode_t *mode, pbwidth_t *width) {
  struct ft920_priv_data *priv;
  unsigned char mymode;         /* ft920 mode */

  rig_debug(RIG_DEBUG_VERBOSE, "ft920: get_mode called\n");

  if (!rig)
    return -RIG_EINVAL;
  
  priv = (struct ft920_priv_data *)rig->state.priv;

  if (vfo == RIG_VFO_CURR) {
    vfo = priv->current_vfo;    /* from previous vfo cmd */
    rig_debug(RIG_DEBUG_TRACE,
              "ft920: get_mode: priv->current_vfo = [0x%x]\n", vfo);
  }

  switch(vfo) {
  case RIG_VFO_A:
    ft920_get_update_data(rig, FT920_NATIVE_VFO_DATA,
                          FT920_VFO_DATA_LENGTH);
    mymode = priv->update_data[FT920_SUMO_DISPLAYED_MODE];
    mymode &= MODE_MASK;          /* mask out bits 4, 5 and 6 */
    break;
  case RIG_VFO_B:
    ft920_get_update_data(rig, FT920_NATIVE_VFO_DATA,
                          FT920_VFO_DATA_LENGTH);
    mymode = priv->update_data[FT920_SUMO_VFO_B_MODE];
    mymode &= MODE_MASK;
    break;
  case RIG_VFO_MEM:
    ft920_get_update_data(rig, FT920_NATIVE_OP_DATA,
                          FT920_VFO_DATA_LENGTH);
    mymode = priv->update_data[FT920_SUMO_DISPLAYED_MODE];
    mymode &= MODE_MASK;
    break;
  default:
    return -RIG_EINVAL;
  }
  rig_debug(RIG_DEBUG_TRACE, "ft920: get_mode: mymode = [0x%x]\n", mymode);

  *width = RIG_PASSBAND_NORMAL;

  /* 
   * translate mode from ft920 to generic.
   *
   * FIXME: FT-920 has 3 DATA modes, LSB, USB, and FM
   * do we need more bit fields in rmode_t? -N0NB
   *
   */
  switch(mymode) {
  case MODE_USBN:
    *width = rig_passband_narrow(rig, RIG_MODE_USB);
    *mode = RIG_MODE_USB;
    break;
  case MODE_USB:
    *mode = RIG_MODE_USB;
    *width = rig_passband_normal(rig, RIG_MODE_USB);
    break;
  case MODE_LSBN:
    *width = rig_passband_narrow(rig, RIG_MODE_LSB);
    *mode = RIG_MODE_LSB;
    break;
  case MODE_LSB:
    *mode = RIG_MODE_LSB;
    *width = rig_passband_normal(rig, RIG_MODE_LSB);
    break;
  case MODE_CW_UN:
  case MODE_CW_LN:
    *width = rig_passband_narrow(rig, RIG_MODE_CW);
    *mode = RIG_MODE_CW;
    break;
  case MODE_CW_U:
  case MODE_CW_L:
    *mode = RIG_MODE_CW;
    *width = rig_passband_normal(rig, RIG_MODE_CW);
    break;
  case MODE_AMN:
    *width = rig_passband_narrow(rig, RIG_MODE_AM);
    *mode = RIG_MODE_AM;
    break;
  case MODE_AM:
    *mode = RIG_MODE_AM;
    *width = rig_passband_normal(rig, RIG_MODE_AM);
    break;
  case MODE_FMN:
    *width = rig_passband_narrow(rig, RIG_MODE_FM);
    *mode = RIG_MODE_FM;
    break;
  case MODE_FM:
    *mode = RIG_MODE_FM;
    *width = rig_passband_normal(rig, RIG_MODE_FM);
    break;
  case MODE_DATA_LN:
    *width = rig_passband_narrow(rig, RIG_MODE_RTTY);
    *mode = RIG_MODE_RTTY;
    break;
  case MODE_DATA_L:
    *mode = RIG_MODE_RTTY;
    *width = rig_passband_normal(rig, RIG_MODE_RTTY);
    break;

  default:
    return -RIG_EINVAL;         /* sorry, wrong mode */
    break; 
  }
  
  return RIG_OK;
}


/*
 * set vfo and store requested vfo for later RIG_VFO_CURR
 * requests.
 *
 */

int ft920_set_vfo(RIG *rig, vfo_t vfo) {
  struct ft920_priv_data *priv;
  unsigned char cmd_index;      /* index of sequence to send */

  rig_debug(RIG_DEBUG_VERBOSE, "ft920: ft920_set_vfo called\n");

  if (!rig)
    return -RIG_EINVAL;
  
  priv = (struct ft920_priv_data *)rig->state.priv;

  rig_debug(RIG_DEBUG_TRACE, "ft920: set_vfo: vfo = [0x%x]\n", vfo);

  switch(vfo) {
  case RIG_VFO_A:
    cmd_index = FT920_NATIVE_VFO_A;
    priv->current_vfo = vfo;    /* update active VFO */
    break;
  case RIG_VFO_B:
    cmd_index = FT920_NATIVE_VFO_B;
    priv->current_vfo = vfo;
    break;
  case RIG_VFO_CURR:
    switch(priv->current_vfo) { /* what is my active VFO ? */
    case RIG_VFO_A:
      cmd_index = FT920_NATIVE_VFO_A;
      break;
    case RIG_VFO_B:
      cmd_index = FT920_NATIVE_VFO_B;
      break;
    }
    break;
  default:
    return -RIG_EINVAL;         /* sorry, wrong VFO */
    rig_debug(RIG_DEBUG_TRACE, "ft920: set_vfo: Unknown default VFO\n");
  }
  
  ft920_send_priv_cmd(rig, cmd_index);

  return RIG_OK;
}


/*
 * get current RX vfo/mem and store requested vfo for
 * later RIG_VFO_CURR requests plus pass the tested vfo/mem
 * back to the frontend.
 *
 */

int ft920_get_vfo(RIG *rig, vfo_t *vfo) {
  struct ft920_priv_data *priv;
  unsigned char status_0;           /* ft920 status flag 0 */
  unsigned char status_1;           /* ft920 status flag 1 */

  rig_debug(RIG_DEBUG_VERBOSE, "ft920: ft920_get_vfo called\n");

  if (!rig)
    return -RIG_EINVAL;
  
  priv = (struct ft920_priv_data *)rig->state.priv;

  /* Get flags for VFO status */
  ft920_get_update_data(rig, FT920_NATIVE_STATUS_FLAGS,
                        FT920_STATUS_FLAGS_LENGTH);
  
  status_0 = priv->update_data[FT920_SUMO_DISPLAYED_STATUS_0];
  status_0 &= SF_VFOB;             /* get VFO B (sub display) active bits */

  status_1 = priv->update_data[FT920_SUMO_DISPLAYED_STATUS_1];
  status_1 &= SF_VFO_MASK;          /* get VFO/MEM (main display) active bits */
  
  rig_debug(RIG_DEBUG_TRACE,
            "ft920: get_vfo: vfo status_0 = [0x%x]\n", status_0);
  rig_debug(RIG_DEBUG_TRACE,
            "ft920: get_vfo: vfo status_1 = [0x%x]\n", status_1);

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
    return RIG_OK;
  case SF_SPLITB:               /* Split operation, RX on VFO B */
    *vfo = RIG_VFO_B;
    priv->current_vfo = RIG_VFO_B;
    return RIG_OK;
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
    return RIG_OK;
  case SF_VFO:
    switch (status_0){
    case SF_SPLITA:             /* Split operation, RX on VFO A */
      *vfo = RIG_VFO_A;
      priv->current_vfo = RIG_VFO_A;
      return RIG_OK;
    case SF_VFOA:
      *vfo = RIG_VFO_A;
      priv->current_vfo = RIG_VFO_A;
      return RIG_OK;
    }
  default:                      /* Oops! */
    return -RIG_EINVAL;         /* sorry, wrong current VFO */
  }
}


/*
 * set the '920 into split TX/RX mode
 *
 * VFO cannot be set as the set split on command only changes the
 * TX to the sub display.  Setting split off returns the TX to the
 * main display.
 *
 */

int ft920_set_split(RIG *rig, vfo_t vfo, split_t split) {
  unsigned char cmd_index;

  rig_debug(RIG_DEBUG_VERBOSE, "ft920: ft920_set_split called\n");

  if (!rig)
    return -RIG_EINVAL;
  
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

  ft920_send_priv_cmd(rig, cmd_index);

  return RIG_OK;
}


/*
 * Get whether the '920 is in split mode
 *
 */

int ft920_get_split(RIG *rig, vfo_t vfo, split_t *split) {
  struct ft920_priv_data *priv;
  unsigned char status_0;

  rig_debug(RIG_DEBUG_VERBOSE, "ft920: ft920_get_split called\n");

  if (!rig)
    return -RIG_EINVAL;
  
  priv = (struct ft920_priv_data *)rig->state.priv;

  /* Get flags for VFO split status */
  ft920_get_update_data(rig, FT920_NATIVE_STATUS_FLAGS,
                        FT920_STATUS_FLAGS_LENGTH);
  
  status_0 = priv->update_data[FT920_SUMO_DISPLAYED_STATUS_0];
  status_0 &= SF_VFOB;             /* get VFO B (sub display) active bits */

  rig_debug(RIG_DEBUG_TRACE,
            "ft920: get_split: split status_0 = [0x%x]\n", status_0);

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
 * private helper function. Retrieves update data from rig.
 * using pacing value and buffer indicated in *priv struct.
 * Extended to be command agnostic as 920 has several ways to
 * get data and several ways to return it.
 *
 * need to use this when doing ft920_get_* stuff
 *
 * Variables:  ci = command index, rl = read length of returned data
 *
 */

static int ft920_get_update_data(RIG *rig, unsigned char ci, unsigned char rl) {
  struct rig_state *rig_s;
  struct ft920_priv_data *priv;
  unsigned char *cmd;           /* points to sequence to send */
  int n;                        /* for read_  */

  rig_debug(RIG_DEBUG_VERBOSE, "ft920: get_update_data called\n");

  if (!rig)
    return -RIG_EINVAL;
  
  priv = (struct ft920_priv_data *)rig->state.priv;
  rig_s = &rig->state;

  /* 
   * Copy native cmd PACING  to private cmd storage area 
   */
  memcpy(&priv->p_cmd, &ncmd[FT920_NATIVE_PACING].nseq, YAESU_CMD_LENGTH);
  priv->p_cmd[3] = priv->pacing;      /* get pacing value, and store in private cmd */
  rig_debug(RIG_DEBUG_TRACE,
            "ft920: get_update_data: read pacing = [%i]\n", priv->pacing);

  /* send PACING cmd to rig  */
  cmd = priv->p_cmd;
  write_block(&rig_s->rigport, cmd, YAESU_CMD_LENGTH);

  /* send UPDATE comand to fetch data*/
  ft920_send_priv_cmd(rig, ci);

  n = read_block(&rig_s->rigport, priv->update_data, rl);
  rig_debug(RIG_DEBUG_TRACE,
            "ft920: get_update_data: read %i bytes\n", n);

  return 0;
}


/*
 * init_ft920 is called by rig_backend_load
 *
 * Is this redundant to initrigs_yaesu? -N0NB
 *
 */

int init_ft920(void *be_handle) {
  rig_debug(RIG_DEBUG_VERBOSE, "ft920: _init called\n");
  rig_register(&ft920_caps); 
  return RIG_OK;
}


/*
 * private helper function to send a private command
 * sequence . Must only be complete sequences.
 * TODO: place variant of this in yaesu.c
 *
 */

static int ft920_send_priv_cmd(RIG *rig, unsigned char ci) {
  struct rig_state *rig_s;
  struct ft920_priv_data *priv;
  unsigned char *cmd;           /* points to sequence to send */
  unsigned char cmd_index;      /* index of sequence to send */
 
  rig_debug(RIG_DEBUG_VERBOSE, "ft920: send_priv_cmd called\n");

  if (!rig)
    return -RIG_EINVAL;


  priv = (struct ft920_priv_data *)rig->state.priv;
  rig_s = &rig->state;
  
  cmd_index = ci;               /* get command */

  if (! priv->pcs[cmd_index].ncomp) {
    rig_debug(RIG_DEBUG_TRACE,
              "ft920: send_priv_cmd: Attempt to send incomplete sequence\n");
    return -RIG_EINVAL;
  }

  cmd = (unsigned char *) priv->pcs[cmd_index].nseq; /* get native sequence */
  write_block(&rig_s->rigport, cmd, YAESU_CMD_LENGTH);

  return RIG_OK;
}
