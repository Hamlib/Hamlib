/*
 * hamlib - (C) Frank Singleton 2000 (vk3fcs@ix.netcom.com)
 *
 * ft757gx.c - (C) Frank Singleton 2000 (vk3fcs@ix.netcom.com)
 * This shared library provides an API for communicating
 * via serial interface to an FT-757GX using the "CAT" interface
 * box (FIF-232C) or similar
 *
 *
 * $Id: ft757gx.c,v 1.1 2004-04-24 13:00:59 fillods Exp $  
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


/*
 * TODO -   FS
 *
 * 1. Rentrant code, remove static stuff from all functions [started]
 * 2. rationalise code, more helper functions [started]
 * 3. Allow cached reads
 * 4. Fix crappy 25Hz resolution handling of FT757GX aaarrgh !
 * 5. Put variant of ftxxx_send_cmd in yaesu.c
 *
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <stdlib.h>
#include <string.h>  /* String function definitions */
#include <unistd.h>  /* UNIX standard function definitions */

#include "hamlib/rig.h"
#include "serial.h"
#include "misc.h"
#include "yaesu.h"
#include "ft757gx.h"


/* Private helper function prototypes */

static int ft757gx_get_update_data(RIG *rig);
static int ft757gx_send_priv_cmd(RIG *rig, unsigned char ci);

/* Native ft757gx cmd set prototypes. These are READ ONLY as each */
/* rig instance will copy from these and modify if required . */
/* Complete sequences (1) can be read and used directly as a cmd sequence . */
/* Incomplete sequences (0) must be completed with extra parameters */
/* eg: mem number, or freq etc.. */

static const yaesu_cmd_set_t ncmd[] = { 
  { 1, { 0x00, 0x00, 0x00, 0x00, 0x01 } }, /* split = off */
  { 1, { 0x00, 0x00, 0x00, 0x01, 0x01 } }, /* split = on */
  { 0, { 0x00, 0x00, 0x00, 0x00, 0x02 } }, /* recall memory*/
  { 0, { 0x00, 0x00, 0x00, 0x00, 0x03 } }, /* vfo to  memory*/
  { 1, { 0x00, 0x00, 0x00, 0x00, 0x04 } }, /* dial lock = off */
  { 1, { 0x00, 0x00, 0x00, 0x01, 0x04 } }, /* dial lock = on */
  { 1, { 0x00, 0x00, 0x00, 0x00, 0x05 } }, /* select vfo A */
  { 1, { 0x00, 0x00, 0x00, 0x01, 0x05 } }, /* select vfo B */
  { 0, { 0x00, 0x00, 0x00, 0x00, 0x06 } }, /* memory to vfo*/
  { 1, { 0x00, 0x00, 0x00, 0x00, 0x07 } }, /* up 500 khz */
  { 1, { 0x00, 0x00, 0x00, 0x00, 0x08 } }, /* down 500 khz */
  { 1, { 0x00, 0x00, 0x00, 0x00, 0x09 } }, /* clarify off */
  { 1, { 0x00, 0x00, 0x00, 0x01, 0x09 } }, /* clarify on */
  { 0, { 0x00, 0x00, 0x00, 0x00, 0x0a } }, /* set freq */

  { 1, { 0x00, 0x00, 0x00, 0x00, 0x0c } }, /* mode set LSB */
  { 1, { 0x00, 0x00, 0x00, 0x01, 0x0c } }, /* mode set USB */
  { 1, { 0x00, 0x00, 0x00, 0x02, 0x0c } }, /* mode set CWW */
  { 1, { 0x00, 0x00, 0x00, 0x03, 0x0c } }, /* mode set CWN */
  { 1, { 0x00, 0x00, 0x00, 0x04, 0x0c } }, /* mode set AMW */
  { 1, { 0x00, 0x00, 0x00, 0x05, 0x0c } }, /* mode set AMN */
  { 1, { 0x00, 0x00, 0x00, 0x06, 0x0c } }, /* mode set FMW */
  { 1, { 0x00, 0x00, 0x00, 0x07, 0x0c } }, /* mode set FMN */

  { 0, { 0x00, 0x00, 0x00, 0x00, 0x0e } }, /* pacing set */
  { 1, { 0x00, 0x00, 0x00, 0x00, 0x0f } }, /* ptt off */
  { 1, { 0x00, 0x00, 0x00, 0x01, 0x0f } }, /* ptt on */
  { 1, { 0x00, 0x00, 0x00, 0x00, 0x10 } }, /* request update from rig */

};



/* 
 * Receiver caps 
 */


#define FT757GX_ALL_RX_MODES (RIG_MODE_AM|RIG_MODE_CW|RIG_MODE_USB|RIG_MODE_LSB|RIG_MODE_FM)


/* 
 * TX caps
 */ 

#define FT757GX_ALL_TX_MODES (RIG_MODE_AM|RIG_MODE_CW|RIG_MODE_USB|RIG_MODE_LSB|RIG_MODE_FM)


/*
 * future - private data
 *
 */

struct ft757gx_priv_data {
  unsigned char pacing;		/* pacing value */
  unsigned int read_update_delay;	 /* depends on pacing value */
  unsigned char current_vfo;	/* active VFO from last cmd , can be either RIG_VFO_A or RIG_VFO_B only */
  unsigned char p_cmd[YAESU_CMD_LENGTH]; /* private copy of 1 constructed CAT cmd */
  yaesu_cmd_set_t pcs[FT_757GX_NATIVE_SIZE];		/* private cmd set */
  unsigned char update_data[FT757GX_STATUS_UPDATE_DATA_LENGTH]; /* returned data */
};


/*
 * ft757gx rigs capabilities.
 * Also this struct is READONLY!
 */

const struct rig_caps ft757gx_caps = {
  .rig_model =        RIG_MODEL_FT757, 
  .model_name =       "FT-757GX", 
  .mfg_name =         "Yaesu", 
  .version =           "0.1", 
  .copyright =         "LGPL",
  .status =            RIG_STATUS_ALPHA, 
  .rig_type =          RIG_TYPE_MOBILE, 
  .ptt_type =          RIG_PTT_NONE,
  .dcd_type =          RIG_DCD_NONE,
  .port_type =         RIG_PORT_SERIAL,
  .serial_rate_min =   4800,
  .serial_rate_max =   4800,
  .serial_data_bits =  8,
  .serial_stop_bits =  2,
  .serial_parity =     RIG_PARITY_NONE,
  .serial_handshake =  RIG_HANDSHAKE_NONE,
  .write_delay =       FT757GX_WRITE_DELAY,
  .post_write_delay =  FT757GX_POST_WRITE_DELAY,
  .timeout =           2000,
  .retry =             0,
  .has_get_func =      RIG_FUNC_NONE,
  .has_set_func =      RIG_FUNC_NONE,
  .has_get_level =     RIG_LEVEL_NONE,
  .has_set_level =     RIG_LEVEL_NONE,
  .has_get_parm =      RIG_PARM_NONE,
  .has_set_parm =      RIG_PARM_NONE,
  .ctcss_list =        NULL,
  .dcs_list =          NULL,
  .preamp =            { RIG_DBLST_END, },
  .attenuator =        { RIG_DBLST_END, },
  .max_rit =           Hz(9999),
  .max_xit =           Hz(0),
  .max_ifshift =       Hz(0),
  .targetable_vfo =    0,
  .transceive =        RIG_TRN_OFF,
  .bank_qty =          0,
  .chan_desc_sz =      0,
  .chan_list =         { RIG_CHAN_END, },	/* FIXME: memory channel list:20 */

  .rx_range_list1 =    { RIG_FRNG_END, },    /* FIXME: enter region 1 setting */

  .tx_range_list1 =    { RIG_FRNG_END, },

  .rx_range_list2 =    { { .start = kHz(500), .end = 29999990, 
			.modes = FT757GX_ALL_RX_MODES,.low_power = -1,.high_power = -1}, 
		      RIG_FRNG_END, }, /* rx range */

  .tx_range_list2 =    { {kHz(1500),1999900,FT757GX_ALL_TX_MODES,.low_power = 5000,.high_power = 100000},
    
    {.start = kHz(3500),3999900,FT757GX_ALL_TX_MODES,5000,100000},
    
    {.start = kHz(7000),7499900,FT757GX_ALL_TX_MODES,5000,100000},
    
    {.start = MHz(10),10499900,FT757GX_ALL_TX_MODES,5000,100000},
    
    {.start = MHz(14),14499900,FT757GX_ALL_TX_MODES,5000,100000},
    
    {.start = MHz(18),18499900,FT757GX_ALL_TX_MODES,5000,100000},
    
    {.start = MHz(21),21499900,FT757GX_ALL_TX_MODES,5000,100000},
    
    {.start = kHz(24500),24999900,FT757GX_ALL_TX_MODES,5000,100000},
    
    {.start = MHz(28),29999900,FT757GX_ALL_TX_MODES,5000,100000},
    
    RIG_FRNG_END, },

  
  .tuning_steps =  {
    {FT757GX_ALL_RX_MODES,10},
    {FT757GX_ALL_RX_MODES,100},
    RIG_TS_END,
  },

      /* mode/filter list, .remember =  order matters! */
  .filters =  {
    RIG_FLT_END,
  },


  .priv =   NULL, /* private data */

  .rig_init =   ft757gx_init, 
  .rig_cleanup =    ft757gx_cleanup, 
  .rig_open =   ft757gx_open,				/* port opened */
  .rig_close =  ft757gx_close,				/* port closed */

  .set_freq =   ft757gx_set_freq,		/* set freq */
  .get_freq =   NULL,		/* get freq */
  .set_mode =   NULL,		/* set mode */
  .get_mode =   NULL,		/* get mode */
  .set_vfo =    ft757gx_set_vfo,		/* set vfo */

  .get_vfo =    NULL,		/* get vfo */
  .set_ptt =    NULL,		/* set ptt */
  .get_ptt =    NULL,		/* get ptt */
};


/*
 * _init 
 *
 */


int ft757gx_init(RIG *rig) {
  struct ft757gx_priv_data *p;
  
  if (!rig)
    return -RIG_EINVAL;
  
  p = (struct ft757gx_priv_data*)malloc(sizeof(struct ft757gx_priv_data));
  if (!p)			/* whoops! memory shortage! */    
    return -RIG_ENOMEM;
  
  rig_debug(RIG_DEBUG_VERBOSE,"ft757gx:ft757gx_init called \n");

  /* 
   * Copy native cmd set to private cmd storage area 
   */

  memcpy(p->pcs,ncmd,sizeof(ncmd));

  /* TODO: read pacing from preferences */

  p->pacing = FT757GX_PACING_DEFAULT_VALUE; /* set pacing to minimum for now */
  p->read_update_delay = FT757GX_DEFAULT_READ_TIMEOUT; /* set update timeout to safe value */
  p->current_vfo =  RIG_VFO_A;	/* default to VFO_A ? */
  rig->state.priv = (void*)p;
  
  return RIG_OK;
}


/*
 * ft757gx_cleanup routine
 * the serial port is closed by the frontend
 */

int ft757gx_cleanup(RIG *rig) {
  if (!rig)
    return -RIG_EINVAL;
  
  rig_debug(RIG_DEBUG_VERBOSE, "ft757gx: _cleanup called\n");

  if (rig->state.priv)
    free(rig->state.priv);
  rig->state.priv = NULL;
  
  return RIG_OK;
}

/*
 * ft757gx_open  routine
 * 
 */

int ft757gx_open(RIG *rig) {
  struct rig_state *rig_s;
 
  if (!rig)
    return -RIG_EINVAL;

  rig_s = &rig->state;

  rig_debug(RIG_DEBUG_VERBOSE,"ft757gx:rig_open: write_delay = %i msec \n", 
				  rig_s->rigport.write_delay);
  rig_debug(RIG_DEBUG_VERBOSE,"ft757gx:rig_open: post_write_delay = %i msec \n", 
				  rig_s->rigport.post_write_delay);

  
   /* TODO */

  return RIG_OK;
}


/*
 * ft757gx_close  routine
 * 
 */

int ft757gx_close(RIG *rig) {

  if (!rig)
    return -RIG_EINVAL;

  rig_debug(RIG_DEBUG_VERBOSE,"ft757gx:ft757gx_close called \n");

  return RIG_OK;
}


/*
 * Example of wrapping backend function inside frontend API
 * 	 
 */


int ft757gx_set_freq(RIG *rig, vfo_t vfo, freq_t freq) {
  struct rig_state *rig_s;
  struct ft757gx_priv_data *p;
  unsigned char *cmd;		/* points to sequence to send */

  if (!rig)
    return -RIG_EINVAL;


  p = (struct ft757gx_priv_data*)rig->state.priv;

  rig_s = &rig->state;

  rig_debug(RIG_DEBUG_VERBOSE,"ft757gx: requested freq = %lli Hz \n", freq);

  /* frontend sets VFO now , if targetable_vfo = 0 */

#if 0
  ft757gx_set_vfo(rig, vfo);	/* select VFO first , new API */
#endif

  /* 
   * Copy native cmd freq_set to private cmd storage area 
   */

  memcpy(&p->p_cmd,&ncmd[FT_757GX_NATIVE_FREQ_SET].nseq,YAESU_CMD_LENGTH);  

  to_bcd(p->p_cmd,freq/10,8);	/* store bcd format in in p_cmd */
				/* TODO -- fix 10Hz resolution -- FS */

  rig_debug(RIG_DEBUG_VERBOSE,"ft757gx: requested freq after conversion = %lli Hz \n", from_bcd(p->p_cmd,8)* 10 );

  cmd = p->p_cmd; /* get native sequence */
  write_block(&rig_s->rigport, cmd, YAESU_CMD_LENGTH);

  return RIG_OK;
}


/*
 * Return Freq for a given VFO
 */

int ft757gx_get_freq(RIG *rig, vfo_t vfo, freq_t *freq) {
  struct ft757gx_priv_data *p;
  freq_t f;

  rig_debug(RIG_DEBUG_VERBOSE,"ft757gx:ft757gx_get_freq called \n");

  if (!rig)
    return -RIG_EINVAL;
  
  p = (struct ft757gx_priv_data*)rig->state.priv;

  ft757gx_get_update_data(rig);	/* get whole shebang from rig */

  if (vfo == RIG_VFO_CURR )
    vfo = p->current_vfo;	/* from previous vfo cmd */

  switch(vfo) {
  case RIG_VFO_A:
    f = from_bcd_be(&(p->update_data[FT757GX_SUMO_VFO_A_FREQ]),8); /* grab freq and convert */
    break;
  case RIG_VFO_B:
    f = from_bcd_be(&(p->update_data[FT757GX_SUMO_VFO_B_FREQ]),8); /* grab freq and convert */
  break;
  default:
    return -RIG_EINVAL;		/* sorry, wrong VFO */
  }

  rig_debug(RIG_DEBUG_VERBOSE,"ft757gx:  freq = %lli Hz  for VFO = %u \n", f, vfo);

  (*freq) = f;			/* return diplayed frequency */

  return RIG_OK;
}





/*
 * set vfo and store requested vfo for later RIG_VFO_CURR
 * requests.
 *
 */
int ft757gx_set_vfo(RIG *rig, vfo_t vfo) {
  struct rig_state *rig_s;
  struct ft757gx_priv_data *p;
  unsigned char cmd_index;	/* index of sequence to send */


  if (!rig)
    return -RIG_EINVAL;
  
  p = (struct ft757gx_priv_data*)rig->state.priv;
  rig_s = &rig->state;
  
  
  /* 
   * TODO : check for errors -- FS
   */


  switch(vfo) {
  case RIG_VFO_A:
    cmd_index = FT_757GX_NATIVE_VFO_A;
    p->current_vfo = vfo;		/* update active VFO */
    break;
  case RIG_VFO_B:
    cmd_index = FT_757GX_NATIVE_VFO_B;
    p->current_vfo = vfo;		/* update active VFO */
    break;
  case RIG_VFO_CURR:
    switch(p->current_vfo) {	/* what is my active VFO ? */
    case RIG_VFO_A:
      cmd_index = FT_757GX_NATIVE_VFO_A;
      break;
    case RIG_VFO_B:
      cmd_index = FT_757GX_NATIVE_VFO_B;
      break;
    default:
      rig_debug(RIG_DEBUG_VERBOSE,"ft757gx: Unknown default VFO \n");
      return -RIG_EINVAL;		/* sorry, wrong current VFO */
    }

    break;

  default:
    return -RIG_EINVAL;		/* sorry, wrong VFO */
  }
  
  /*
   * phew! now send cmd to rig
   */ 

  ft757gx_send_priv_cmd(rig,cmd_index);

  return RIG_OK;

}


int ft757gx_get_vfo(RIG *rig, vfo_t *vfo) {
  struct ft757gx_priv_data *p;
  unsigned char status;		/* ft757gx status flag */

  if (!rig)
    return -RIG_EINVAL;
  
  p = (struct ft757gx_priv_data*)rig->state.priv;

  ft757gx_get_update_data(rig);	/* get whole shebang from rig */
  
  status = p->update_data[FT757GX_SUMO_DISPLAYED_STATUS];
  status &= SF_VFOAB; /* check VFO bit*/
  
  rig_debug(RIG_DEBUG_VERBOSE,"ft757gx: vfo status = %x \n", status);

  /* 
   * translate vfo status from ft757gx to generic.
   */

  if (status) {
    rig_debug(RIG_DEBUG_VERBOSE,"ft757gx: VFO = B \n");
    (*vfo) = RIG_VFO_B;
    return RIG_OK; 
  } else {
    rig_debug(RIG_DEBUG_VERBOSE,"ft757gx: VFO = A \n");
    (*vfo) = RIG_VFO_A;
    return RIG_OK;
  }

}


int ft757gx_set_ptt(RIG *rig,vfo_t vfo, ptt_t ptt) {
  unsigned char cmd_index;	/* index of sequence to send */

  if (!rig)
    return -RIG_EINVAL;

  /* frontend sets VFO now , if targetable_vfo = 0 */
  
#if 0
  ft757gx_set_vfo(rig,vfo);	/* select VFO first */
#endif

  switch(ptt) {
  case RIG_PTT_OFF:
    cmd_index = FT_757GX_NATIVE_PTT_OFF;
    break;
  case RIG_PTT_ON:
    cmd_index = FT_757GX_NATIVE_PTT_ON;
    break;
  default:
    return -RIG_EINVAL;		/* sorry, wrong VFO */
  }

  /*
   * phew! now send cmd to rig
   */ 

  ft757gx_send_priv_cmd(rig,cmd_index);

  return RIG_OK;		/* good */
}

int ft757gx_get_ptt(RIG *rig, vfo_t vfo, ptt_t *ptt) {
  struct ft757gx_priv_data *p;
  unsigned char status;		/* ft757gx mode */

  if (!rig)
    return -RIG_EINVAL;
  
  p = (struct ft757gx_priv_data*)rig->state.priv;

  ft757gx_get_update_data(rig);	/* get whole shebang from rig */
  
  status = p->update_data[FT757GX_SUMO_DISPLAYED_STATUS];
  status = status & SF_RXTX; /* check RXTX bit*/
  
  rig_debug(RIG_DEBUG_VERBOSE,"ft757gx: ptt status = %x \n", status);

  /* 
   * translate mode from ft757gx to generic.
   */

  if (status) {
    rig_debug(RIG_DEBUG_VERBOSE,"ft757gx: PTT = ON \n");
    (*ptt) = RIG_PTT_ON;
    return RIG_OK; 
  } else {
    rig_debug(RIG_DEBUG_VERBOSE,"ft757gx: PTT = OFF \n");
    (*ptt) = RIG_PTT_OFF;
    return RIG_OK;
  }

}


/*
 * private helper function. Retrieves update data from rig.
 * using pacing value and buffer indicated in *priv struct.
 *
 * need to use this when doing ft757gx_get_* stuff
 */

static int ft757gx_get_update_data(RIG *rig) {
  struct rig_state *rig_s;
  struct ft757gx_priv_data *p;
  unsigned char *cmd;		/* points to sequence to send */
  int n;			/* for read_  */

  if (!rig)
    return -RIG_EINVAL;
  
  p = (struct ft757gx_priv_data*)rig->state.priv;
  rig_s = &rig->state;

  /* 
   * Copy native cmd PACING  to private cmd storage area 
   */

  memcpy(&p->p_cmd,&ncmd[FT_757GX_NATIVE_PACING].nseq,YAESU_CMD_LENGTH);  
  p->p_cmd[3] = p->pacing;		/* get pacing value, and store in private cmd */
  rig_debug(RIG_DEBUG_VERBOSE,"ft757gx: read pacing = %i \n",p->pacing);

   /* send PACING cmd to rig  */

  cmd = p->p_cmd;
  write_block(&rig_s->rigport, cmd, YAESU_CMD_LENGTH);

  /* send UPDATE comand to fetch data*/

  ft757gx_send_priv_cmd(rig,FT_757GX_NATIVE_UPDATE);

  /*    n = read_sleep(rig_s->fd,p->update_data, FT757GX_STATUS_UPDATE_DATA_LENGTH, FT757GX_DEFAULT_READ_TIMEOUT);  */
  n = read_block(&rig_s->rigport, p->update_data, 
				  FT757GX_STATUS_UPDATE_DATA_LENGTH); 

  return 0;

}


/*
 * private helper function to send a private command
 * sequence . Must only be complete sequences.
 * TODO: place variant of this in yaesu.c
 */

static int ft757gx_send_priv_cmd(RIG *rig, unsigned char ci) {

  struct rig_state *rig_s;
  struct ft757gx_priv_data *p;
  unsigned char *cmd;		/* points to sequence to send */
  unsigned char cmd_index;	/* index of sequence to send */
 
  if (!rig)
    return -RIG_EINVAL;


  p = (struct ft757gx_priv_data*)rig->state.priv;
  rig_s = &rig->state;
  
  cmd_index = ci;		/* get command */

  if (! p->pcs[cmd_index].ncomp) {
    rig_debug(RIG_DEBUG_VERBOSE,"ft757gx: Attempt to send incomplete sequence \n");
    return -RIG_EINVAL;
  }

  cmd = (unsigned char *) p->pcs[cmd_index].nseq; /* get native sequence */
  write_block(&rig_s->rigport, cmd, YAESU_CMD_LENGTH);
  
  return RIG_OK;

}

