/*
 * hamlib - (C) Frank Singleton 2000 (vk3fcs@ix.netcom.com)
 *
 * ft920.c - (C) Frank Singleton 2000 (vk3fcs@ix.netcom.com)
 *           (C) Nate Bargmann 2002 (n0nb@arrl.net)
 *           (C) Stephane Fillod 2002 (fillods@users.sourceforge.net)
 *
 * This shared library provides an API for communicating
 * via serial interface to an FT-920 using the "CAT" interface
 * Documentation can be found online at:
 * http://www.yaesu.com/amateur/pdf/manuals/ft_920.pdf
 * pages 86 to 90
 *
 *
 * $Id: ft920.c,v 1.3 2002-10-31 01:00:29 n0nb Exp $
 *
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 * 
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 * 
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.
 * 
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
#include "serial.h"
#include "misc.h"
#include "yaesu.h"
#include "ft920.h"


/* Private helper function prototypes */

static int ft920_get_update_data(RIG *rig, unsigned char ci, unsigned char rl);
static int ft920_send_priv_cmd(RIG *rig, unsigned char ci);

/* Native ft920 cmd set prototypes. These are READ ONLY as each */
/* rig instance will copy from these and modify if required . */
/* Complete sequences (1) can be read and used directly as a cmd sequence . */
/* Incomplete sequences (0) must be completed with extra parameters */
/* eg: mem number, or freq etc.. */

static const yaesu_cmd_set_t ncmd[] =	{
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
	{ 0, { 0x00, 0x00, 0x00, 0x00, 0x0a } }, /* set freq */

	{ 1, { 0x00, 0x00, 0x00, 0x00, 0x0c } }, /* vfo A mode set LSB */
	{ 1, { 0x00, 0x00, 0x00, 0x01, 0x0c } }, /* vfo A mode set USB */
	{ 1, { 0x00, 0x00, 0x00, 0x02, 0x0c } }, /* vfo A mode set CW-USB */
	{ 1, { 0x00, 0x00, 0x00, 0x03, 0x0c } }, /* vfo A mode set CW-LSB */
	{ 1, { 0x00, 0x00, 0x00, 0x04, 0x0c } }, /* vfo A mode set AM */
	{ 1, { 0x00, 0x00, 0x00, 0x05, 0x0c } }, /* vfo A mode set AM */
	{ 1, { 0x00, 0x00, 0x00, 0x06, 0x0c } }, /* vfo A mode set FM */
	{ 1, { 0x00, 0x00, 0x00, 0x07, 0x0c } }, /* vfo A mode set FMN */
	{ 1, { 0x00, 0x00, 0x00, 0x08, 0x0c } }, /* vfo A mode set DATA-LSB */
	{ 1, { 0x00, 0x00, 0x00, 0x09, 0x0c } }, /* vfo A mode set DATA-LSB */
	{ 1, { 0x00, 0x00, 0x00, 0x0a, 0x0c } }, /* vfo A mode set DATA-USB */
	{ 1, { 0x00, 0x00, 0x00, 0x0b, 0x0c } }, /* vfo A mode set DATA-FM */

/*  { 1, { 0x00, 0x00, 0x00, 0x80, 0x0c } }, */ /* vfo B mode set LSB */
/*  { 1, { 0x00, 0x00, 0x00, 0x81, 0x0c } }, */ /* vfo B mode set USB */
/*  { 1, { 0x00, 0x00, 0x00, 0x82, 0x0c } }, */ /* vfo B mode set CW-USB */
/*  { 1, { 0x00, 0x00, 0x00, 0x83, 0x0c } }, */ /* vfo B mode set CW-LSB */
/*  { 1, { 0x00, 0x00, 0x00, 0x84, 0x0c } }, */ /* vfo B mode set AM */
/*  { 1, { 0x00, 0x00, 0x00, 0x85, 0x0c } }, */ /* vfo B mode set AM */
/*  { 1, { 0x00, 0x00, 0x00, 0x86, 0x0c } }, */ /* vfo B mode set FM */
/*  { 1, { 0x00, 0x00, 0x00, 0x87, 0x0c } }, */ /* vfo B mode set FMN */
/*  { 1, { 0x00, 0x00, 0x00, 0x88, 0x0c } }, */ /* vfo B mode set DATA-LSB */
/*  { 1, { 0x00, 0x00, 0x00, 0x89, 0x0c } }, */ /* vfo B mode set DATA-LSB */
/*  { 1, { 0x00, 0x00, 0x00, 0x8a, 0x0c } }, */ /* vfo B mode set DATA-USB */
/*  { 1, { 0x00, 0x00, 0x00, 0x8b, 0x0c } }, */ /* vfo B mode set DATA-FM */

	{ 0, { 0x00, 0x00, 0x00, 0x00, 0x0e } }, /* update interval/pacing */
	{ 1, { 0x00, 0x00, 0x00, 0x03, 0x10 } }, /* status update VFO A & B update (28 bytes) hard coded for now */
	{ 1, { 0x00, 0x00, 0x00, 0x01, 0xFA } }, /* Read status flags */
/*  { 0, { 0x00, 0x00, 0x00, 0x00, 0x70 } }, */ /* keyer commands */
/*  { 1, { 0x00, 0x00, 0x00, 0x00, 0x81 } }, */ /* tuner off */
/*  { 1, { 0x00, 0x00, 0x00, 0x01, 0x81 } }, */ /* tuner on */
/*  { 1, { 0x00, 0x00, 0x00, 0x00, 0x82 } }, */ /* tuner start*/

};



/* 
 * Receiver caps 
 */


#define FT920_ALL_RX_MODES (RIG_MODE_AM|RIG_MODE_CW|RIG_MODE_USB|RIG_MODE_LSB)
#define FT920_SSB_CW_RX_MODES (RIG_MODE_CW|RIG_MODE_USB|RIG_MODE_LSB)
#define FT920_AM_RX_MODES (RIG_MODE_AM)
#define FT920_FM_RX_MODES (RIG_MODE_FM)


/* 
 * TX caps
 */ 

#define FT920_OTHER_TX_MODES (RIG_MODE_CW| RIG_MODE_USB| RIG_MODE_LSB )	/* 100 W class */
#define FT920_AM_TX_MODES (RIG_MODE_AM ) /* set 25W max */

#define FT920_FUNC_ALL (RIG_FUNC_FAGC|RIG_FUNC_NB|RIG_FUNC_COMP|RIG_FUNC_VOX|RIG_FUNC_TONE|RIG_FUNC_TSQL|RIG_FUNC_SBKIN|RIG_FUNC_FBKIN)	/* fix */




/*
 * future - private data
 *
 */

struct ft920_priv_data {
  unsigned char pacing;			 /* pacing value */
  unsigned int read_update_delay;        /* depends on pacing value */
  unsigned char current_vfo;	         /* active VFO from last cmd */
  unsigned char p_cmd[YAESU_CMD_LENGTH]; /* private copy of 1 constructed CAT cmd */
  yaesu_cmd_set_t pcs[FT_920_NATIVE_SIZE];		      /* private cmd set */
  unsigned char update_data[FT920_VFO_UPDATE_DATA_LENGTH]; /* returned data--max value, some are less */
};


/*
 * ft920 rigs capabilities.
 * Also this struct is READONLY!
 */

const struct rig_caps ft920_caps = {
    .rig_model =       	RIG_MODEL_FT920,
    .model_name =      	"FT-920",
    .mfg_name =		"Yaesu",
    .version =		"0.0.2",
    .copyright =       	"GPL",
    .status =		RIG_STATUS_ALPHA,
    .rig_type =		RIG_TYPE_TRANSCEIVER,
    .ptt_type =		RIG_PTT_NONE,
    .dcd_type =		RIG_DCD_NONE,
    .port_type =       	RIG_PORT_SERIAL,
    .serial_rate_min =	4800,
    .serial_rate_max =	4800,
    .serial_data_bits =	8,
    .serial_stop_bits =	2,
    .serial_parity =	RIG_PARITY_NONE,
    .serial_handshake =	RIG_HANDSHAKE_NONE,
    .write_delay =     	FT920_WRITE_DELAY,
    .post_write_delay =	FT920_POST_WRITE_DELAY,
    .timeout =		2000,
    .retry =		0,
    .has_get_func =    	FT920_FUNC_ALL,
    .has_set_func =    	FT920_FUNC_ALL,
    .has_get_level =	RIG_LEVEL_NONE,
    .has_set_level =	RIG_LEVEL_NONE,
    .has_get_parm =    	RIG_PARM_NONE,
    .has_set_parm =    	RIG_PARM_NONE,
    .ctcss_list =      	NULL,
    .dcs_list =		NULL,
    .preamp =		{ RIG_DBLST_END, },
    .attenuator =      	{ RIG_DBLST_END, },
    .max_rit =		Hz(9999),
    .max_xit =		Hz(9999),
    .max_ifshift =     	Hz(0),
    .targetable_vfo =	0,
    .transceive =      	RIG_TRN_OFF,
    .bank_qty =		0,
    .chan_desc_sz =    	0,
    .chan_list =       	{ RIG_CHAN_END, },	/* FIXME: memory channel list: 122 (!) */

    .rx_range_list1 =	{ RIG_FRNG_END, },	/* FIXME: enter region 1 setting */

    .tx_range_list1 =	{ RIG_FRNG_END, },

    .rx_range_list2 =	{
        {kHz(100),	MHz(30),	FT920_ALL_RX_MODES, -1, -1 },   /* General coverage + ham */
	{MHz(48),	MHz(56),	FT920_ALL_RX_MODES, -1, -1 },   /* 6m! */
	RIG_FRNG_END,
    }, /* Region 2 rx ranges */

    .tx_range_list2 =	{
        {MHz(1.8),	MHz(1.99999),	FT920_OTHER_TX_MODES,	W(5),	W(100)},	/* 100W class */
	{MHz(1.8),	MHz(1.99999),	FT920_AM_TX_MODES,     	W(2),	W(25)},		/* 25W class */
    
	{MHz(3.5),     	MHz(3.99999),	FT920_OTHER_TX_MODES,	W(5),	W(100)},
	{MHz(3.5),     	MHz(3.99999),	FT920_AM_TX_MODES,     	W(2),	W(25)},
    
	{MHz(7),       	MHz(7.29999),	FT920_OTHER_TX_MODES,	W(5),	W(100)},
	{MHz(7),       	MHz(7.29999),	FT920_AM_TX_MODES,     	W(2),	W(25)},

	{MHz(10.1),    	MHz(10.14999),	FT920_OTHER_TX_MODES,	W(5),	W(100)},
	{MHz(10.1),    	MHz(10.14999),	FT920_AM_TX_MODES,     	W(2),	W(25)},
    
	{MHz(14),      	MHz(14.34999),	FT920_OTHER_TX_MODES,	W(5),	W(100)},
	{MHz(14),      	MHz(14.34999),	FT920_AM_TX_MODES,     	W(2),	W(25)},
    
	{MHz(18.068),	MHz(18.16799),	FT920_OTHER_TX_MODES,	W(5),	W(100)},
	{MHz(18.068),	MHz(18.16799),	FT920_AM_TX_MODES,     	W(2),	W(25)},
    
	{MHz(21),	MHz(21.44999),	FT920_OTHER_TX_MODES,	W(5),	W(100)},
	{MHz(21),	MHz(21.44999),	FT920_AM_TX_MODES,     	W(2),	W(25)},
    
	{MHz(24.89),	MHz(24.98999),	FT920_OTHER_TX_MODES,	W(5),	W(100)},
	{MHz(24.89),	MHz(24.98999),	FT920_AM_TX_MODES,     	W(2),	W(25)},
    
	{MHz(28),	MHz(29.69999),	FT920_OTHER_TX_MODES,	W(5),	W(100)},
	{MHz(28),	MHz(29.69999),	FT920_AM_TX_MODES,     	W(2),	W(25)},
    
	{MHz(50),	MHz(53.99999),	FT920_OTHER_TX_MODES,	W(5),	W(100)},
	{MHz(50),	MHz(53.99999),	FT920_AM_TX_MODES,     	W(2),	W(25)},

	RIG_FRNG_END,
    },  /* region 2 TX ranges */

    .tuning_steps =	{
        {FT920_SSB_CW_RX_MODES,	Hz(10)},	/* Normal */
	{FT920_SSB_CW_RX_MODES,	Hz(100)},	/* Fast */

	{FT920_AM_RX_MODES,	Hz(100)},	/* Normal */
	{FT920_AM_RX_MODES,	kHz(1)},	/* Fast */

	{FT920_FM_RX_MODES,	Hz(100)},	/* Normal */
	{FT920_FM_RX_MODES,	kHz(1)}, 	/* Fast */

        RIG_TS_END,

	/* The FT-920 has a Fine tuning step which increments in 1 Hz steps
	 * for SSB_CW_RX_MODES, and 10 Hz steps for AM_RX_MODES and
	 * FM_RX_MODES.  It doesn't appear that anything finer than 10 Hz
	 * is available through the CAT interface, however. -N0NB
         */
    },

    /* mode/filter list, .remember =  order matters! */

    .filters =	{
        {RIG_MODE_SSB,	kHz(2.4)},	/* standard SSB filter bandwidth */
	{RIG_MODE_CW,	kHz(2.4)},	/* normal CW filter */
	{RIG_MODE_CW,	kHz(0.5)},	/* CW filter with narrow selection (must be installed!) */
	{RIG_MODE_AM,	kHz(15)},	/* normal AM filter (stock radio has no AM filter!) */
	{RIG_MODE_AM,	kHz(2.4)},	/* AM filter with narrow selection (SSB filter switched in) */
	{RIG_MODE_FM,	kHz(12)},	/* FM with optional FM unit */
	{RIG_MODE_WFM,	kHz(12)},	/* WideFM, with optional FM unit. */

	RIG_FLT_END,
    },

    .priv =		NULL,			/* private data */

    .rig_init =		ft920_init,
    .rig_cleanup =	ft920_cleanup,
    .rig_open =		ft920_open,		/* port opened */
    .rig_close =	ft920_close,		/* port closed */

    .set_freq =		ft920_set_freq,		/* set freq */
    .get_freq =		ft920_get_freq,		/* get freq */
    .set_mode =		ft920_set_mode,		/* set mode */
    .get_mode =		ft920_get_mode,		/* get mode */
    .set_vfo =		ft920_set_vfo,		/* set vfo */
    .get_vfo =		ft920_get_vfo,		/* get vfo */

};


/*
 * _init 
 *
 */


int ft920_init(RIG *rig) {
  struct ft920_priv_data *p;
  
  if (!rig)
    return -RIG_EINVAL;
  
  p = (struct ft920_priv_data*)malloc(sizeof(struct ft920_priv_data));
  if (!p)			/* whoops! memory shortage! */    
    return -RIG_ENOMEM;
  
  rig_debug(RIG_DEBUG_VERBOSE,"ft920: ft920_init called \n");

  /* 
   * Copy native cmd set to private cmd storage area 
   */

  memcpy(p->pcs,ncmd,sizeof(ncmd));

  /* TODO: read pacing from preferences */

  p->pacing = FT920_PACING_DEFAULT_VALUE; /* set pacing to minimum for now */
  p->read_update_delay = FT920_DEFAULT_READ_TIMEOUT; /* set update timeout to safe value */
  p->current_vfo =  RIG_VFO_A;	/* default to VFO_A ? */
  rig->state.priv = (void*)p;
  
  return RIG_OK;
}


/*
 * ft920_cleanup routine
 * the serial port is closed by the frontend
 */

int ft920_cleanup(RIG *rig) {
  if (!rig)
    return -RIG_EINVAL;
  
  rig_debug(RIG_DEBUG_VERBOSE, "ft920: _cleanup called\n");

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
 
  if (!rig)
    return -RIG_EINVAL;

  rig_s = &rig->state;

  rig_debug(RIG_DEBUG_VERBOSE,"ft920: rig_open: write_delay = %i msec \n",
				  rig_s->rigport.write_delay);
  rig_debug(RIG_DEBUG_VERBOSE,"ft920: rig_open: post_write_delay = %i msec \n",
				  rig_s->rigport.post_write_delay);

  
   /* TODO */

  return RIG_OK;
}


/*
 * ft920_close  routine
 * 
 */

int ft920_close(RIG *rig) {

  if (!rig)
    return -RIG_EINVAL;

  rig_debug(RIG_DEBUG_VERBOSE,"ft920: ft920_close called \n");

  return RIG_OK;
}


/*
 * Example of wrapping backend function inside frontend API
 * 	 
 */


int ft920_set_freq(RIG *rig, vfo_t vfo, freq_t freq) {
  struct rig_state *rig_s;
  struct ft920_priv_data *p;
  unsigned char *cmd;		/* points to sequence to send */

  if (!rig)
    return -RIG_EINVAL;


  p = (struct ft920_priv_data*)rig->state.priv;

  rig_s = &rig->state;

  rig_debug(RIG_DEBUG_VERBOSE,"ft920: requested freq = %lli Hz \n", freq);

  /* frontend sets VFO now , if targetable_vfo = 0 */

#if 0
  ft920_set_vfo(rig, vfo);	/* select VFO first , new API */
#endif

  /* 
   * Copy native cmd freq_set to private cmd storage area 
   */

  memcpy(&p->p_cmd,&ncmd[FT_920_NATIVE_FREQ_SET].nseq,YAESU_CMD_LENGTH);  

  to_bcd(p->p_cmd,freq/10,8);	/* store bcd format in in p_cmd */
				/* TODO -- fix 10Hz resolution -- FS */

  rig_debug(RIG_DEBUG_VERBOSE,"ft920: requested freq after conversion = %lli Hz \n", from_bcd(p->p_cmd,8)* 10 );

  cmd = p->p_cmd; /* get native sequence */
  write_block(&rig_s->rigport, cmd, YAESU_CMD_LENGTH);

  return RIG_OK;
}


/*
 * Return Freq for a given VFO
 */

int ft920_get_freq(RIG *rig, vfo_t vfo, freq_t *freq) {
  struct ft920_priv_data *priv;
  unsigned char *p;
  freq_t f;

  rig_debug(RIG_DEBUG_VERBOSE,"ft920: ft920_get_freq called\n");

  if (!rig)
    return -RIG_EINVAL;
  
  priv = (struct ft920_priv_data*)rig->state.priv;

  ft920_get_update_data(rig, FT_920_NATIVE_VFO_UPDATE, FT920_VFO_UPDATE_DATA_LENGTH);	/* get VFO record from rig*/

  if (vfo == RIG_VFO_CURR )
    vfo = priv->current_vfo;	/* from previous vfo cmd */

  switch(vfo) {
  case RIG_VFO_A:
    p = &priv->update_data[FT920_SUMO_VFO_A_FREQ];
    break;
  case RIG_VFO_B:
    p = &priv->update_data[FT920_SUMO_VFO_B_FREQ];
    break;
  default:
    return -RIG_EINVAL;		/* sorry, wrong VFO */
  }

  /* big endian integer */
  f = (((((p[0]<<8) + p[1])<<8) + p[2])<<8) + p[3];

  rig_debug(RIG_DEBUG_VERBOSE,"ft920: freq = %lli Hz for VFO = %u\n", f, vfo);

  *freq = f;			/* return diplayed frequency */

  return RIG_OK;
}


/*
 * set mode : eg AM, CW etc for a given VFO
 *
 */

int ft920_set_mode(RIG *rig, vfo_t vfo, rmode_t mode, pbwidth_t width ) {
  unsigned char cmd_index;	/* index of sequence to send */

  if (!rig)
    return -RIG_EINVAL;

  /* frontend sets VFO now , if targetable_vfo = 0 */

#if 0
  ft920_set_vfo(rig, vfo);	/* select VFO first , new API */ 
#endif

  /* 
   * translate mode from generic to ft920 specific
   */

  rig_debug(RIG_DEBUG_VERBOSE,"ft920: generic mode = %x \n", mode);

  switch(mode) {
  case RIG_MODE_AM:
    cmd_index = FT_920_NATIVE_MODE_SET_AMW;
    break;
  case RIG_MODE_CW:
    cmd_index = FT_920_NATIVE_MODE_SET_CW_USB;
    break;
  case RIG_MODE_USB:
    cmd_index = FT_920_NATIVE_MODE_SET_USB;
    break;
  case RIG_MODE_LSB:
    cmd_index = FT_920_NATIVE_MODE_SET_LSB;
    break;
  case RIG_MODE_FM:
    cmd_index = FT_920_NATIVE_MODE_SET_FMW;
    break;
  default:
    return -RIG_EINVAL;		/* sorry, wrong MODE */
  }


  /*
   * Now set width
   */

  switch(width) {
  case RIG_PASSBAND_NORMAL:	/* easy  case , no change to native sequence */
    break;

#ifdef RIG_PASSBAND_OLDTIME
  case RIG_PASSBAND_WIDE:
    return -RIG_EINVAL;		/* sorry, WIDE WIDTH is not supported */

  case RIG_PASSBAND_NARROW:	/* must set narrow */
    switch(mode) {
    case RIG_MODE_AM:
      cmd_index = FT_920_NATIVE_MODE_SET_AMN;
      break;
    case RIG_MODE_FM:
      cmd_index = FT_920_NATIVE_MODE_SET_FMN;
      break;
    case RIG_MODE_CW:
      cmd_index = FT_920_NATIVE_MODE_SET_CWN;
      break;
    default:
      return -RIG_EINVAL;		/* sorry, wrong MODE/WIDTH combo  */    
    }
    break;
#else
  /* TODO! */
#endif

  default:
    return -RIG_EINVAL;		/* sorry, wrong WIDTH requested  */    
  }

  /*
   * phew! now send cmd to rig
   */ 

  ft920_send_priv_cmd(rig,cmd_index);

  rig_debug(RIG_DEBUG_VERBOSE,"ft920: cmd_index = %i \n", cmd_index);

  return RIG_OK;		/* good */
  
}


int ft920_get_mode(RIG *rig, vfo_t vfo, rmode_t *mode, pbwidth_t *width) {
  struct ft920_priv_data *p;
  unsigned char mymode;		/* ft920 mode */

  if (!rig)
    return -RIG_EINVAL;
  
  p = (struct ft920_priv_data*)rig->state.priv;

  /* FIXME: wrong command for mode--is part of VFO record -N0NB */
  ft920_get_update_data(rig, FT_920_NATIVE_UPDATE, FT920_STATUS_UPDATE_DATA_LENGTH);
  
  mymode = p->update_data[FT920_SUMO_DISPLAYED_MODE];
  mymode &= MODE_MASK; /* mask out bits 5 and 6 */
  
  rig_debug(RIG_DEBUG_VERBOSE,"ft920: mymode = %x \n", mymode);

  /* 
   * translate mode from ft920 to generic.
   */

  switch(mymode) {
  case MODE_FM:
    (*mode) = RIG_MODE_FM;
    rig_debug(RIG_DEBUG_VERBOSE,"ft920: mode = FM \n");
    break;
  case MODE_AM:
    (*mode) = RIG_MODE_AM;
    rig_debug(RIG_DEBUG_VERBOSE,"ft920: mode = AM \n");
    break;
  case MODE_CW:
    (*mode) = RIG_MODE_CW;
    rig_debug(RIG_DEBUG_VERBOSE,"ft920: mode = CW \n");
    break;
  case MODE_USB:
    (*mode) = RIG_MODE_USB;
    rig_debug(RIG_DEBUG_VERBOSE,"ft920: mode = USB \n");
    break;
  case MODE_LSB:
    (*mode) = RIG_MODE_LSB;
    rig_debug(RIG_DEBUG_VERBOSE,"ft920: mode = LSB \n");
    break;

  default:
    return -RIG_EINVAL;		/* sorry, wrong mode */
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
  struct rig_state *rig_s;
  struct ft920_priv_data *p;
  unsigned char cmd_index;	/* index of sequence to send */


  if (!rig)
    return -RIG_EINVAL;
  
  p = (struct ft920_priv_data*)rig->state.priv;
  rig_s = &rig->state;
  
  
  /* 
   * TODO : check for errors -- FS
   */


  switch(vfo) {
  case RIG_VFO_A:
    cmd_index = FT_920_NATIVE_VFO_A;
    p->current_vfo = vfo;		/* update active VFO */
    break;
  case RIG_VFO_B:
    cmd_index = FT_920_NATIVE_VFO_B;
    p->current_vfo = vfo;		/* update active VFO */
    break;
  case RIG_VFO_CURR:
    switch(p->current_vfo) {	/* what is my active VFO ? */
    case RIG_VFO_A:
      cmd_index = FT_920_NATIVE_VFO_A;
      break;
    case RIG_VFO_B:
      cmd_index = FT_920_NATIVE_VFO_B;
      break;
    default:
      rig_debug(RIG_DEBUG_VERBOSE,"ft920: Unknown default VFO \n");
      return -RIG_EINVAL;		/* sorry, wrong current VFO */
    }

    break;

  default:
    return -RIG_EINVAL;		/* sorry, wrong VFO */
  }
  
  /*
   * phew! now send cmd to rig
   */ 

  ft920_send_priv_cmd(rig,cmd_index);

  return RIG_OK;

}


int ft920_get_vfo(RIG *rig, vfo_t *vfo) {
  struct ft920_priv_data *p;
  unsigned char status;		/* ft920 status flag */

  if (!rig)
    return -RIG_EINVAL;
  
  p = (struct ft920_priv_data*)rig->state.priv;

  /* Get flags for VFO status */
  ft920_get_update_data(rig, FT_920_NATIVE_UPDATE, FT920_STATUS_UPDATE_DATA_LENGTH);
  
  status = p->update_data[FT920_SUMO_DISPLAYED_STATUS];
  status &= SF_VFOAB; /* check VFO bit*/
  
  rig_debug(RIG_DEBUG_VERBOSE,"ft920: vfo status = %x \n", status);

  /* 
   * translate vfo status from ft920 to generic.
   */

  if (status) {
    rig_debug(RIG_DEBUG_VERBOSE,"ft920: VFO = B \n");
    (*vfo) = RIG_VFO_B;
    return RIG_OK; 
  } else {
    rig_debug(RIG_DEBUG_VERBOSE,"ft920: VFO = A \n");
    (*vfo) = RIG_VFO_A;
    return RIG_OK;
  }

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
 */

static int ft920_get_update_data(RIG *rig, unsigned char ci, unsigned char rl) {
  struct rig_state *rig_s;
  struct ft920_priv_data *p;
  unsigned char *cmd;		/* points to sequence to send */
  int n;			/* for read_  */

  if (!rig)
    return -RIG_EINVAL;
  
  p = (struct ft920_priv_data*)rig->state.priv;
  rig_s = &rig->state;

  /* 
   * Copy native cmd PACING  to private cmd storage area 
   */

  memcpy(&p->p_cmd, &ncmd[FT_920_NATIVE_PACING].nseq, YAESU_CMD_LENGTH);
  p->p_cmd[3] = p->pacing;		/* get pacing value, and store in private cmd */
  rig_debug(RIG_DEBUG_VERBOSE,"ft920: read pacing = %i \n",p->pacing);

   /* send PACING cmd to rig  */

  cmd = p->p_cmd;
  write_block(&rig_s->rigport, cmd, YAESU_CMD_LENGTH);

  /* send UPDATE comand to fetch data*/

  ft920_send_priv_cmd(rig, ci);

  n = read_block(&rig_s->rigport, p->update_data, rl);

  return 0;

}



/*
 * init_ft920 is called by rig_backend_load
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
 */

static int ft920_send_priv_cmd(RIG *rig, unsigned char ci) {

  struct rig_state *rig_s;
  struct ft920_priv_data *p;
  unsigned char *cmd;		/* points to sequence to send */
  unsigned char cmd_index;	/* index of sequence to send */
 
  if (!rig)
    return -RIG_EINVAL;


  p = (struct ft920_priv_data*)rig->state.priv;
  rig_s = &rig->state;
  
  cmd_index = ci;		/* get command */

  if (! p->pcs[cmd_index].ncomp) {
    rig_debug(RIG_DEBUG_VERBOSE,"ft920: Attempt to send incomplete sequence \n");
    return -RIG_EINVAL;
  }

  cmd = (unsigned char *) p->pcs[cmd_index].nseq; /* get native sequence */
  write_block(&rig_s->rigport, cmd, YAESU_CMD_LENGTH);
  
  return RIG_OK;

}

