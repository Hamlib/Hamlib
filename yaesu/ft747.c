/*
 * hamlib - (C) Frank Singleton 2000 (vk3fcs@ix.netcom.com)
 *
 * ft747.c - (C) Frank Singleton 2000 (vk3fcs@ix.netcom.com)
 * This shared library provides an API for communicating
 * via serial interface to an FT-747GX using the "CAT" interface
 * box (FIF-232C) or similar
 *
 *
 * $Id: ft747.c,v 1.9 2001-03-02 18:35:18 f4cfe Exp $  
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
 */


/*
 * TODO -   FS
 *
 * 1. Rentrant code, remove static stuff from all functions [started]
 * 2. rationalise code, more helper functions [started]
 * 3. Allow cached reads
 * 4. Fix crappy 25Hz resolution handling of FT747 aaarrgh !
 * 5. Put variant of ftxxx_send_cmd in yaesu.c
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
#include "ft747.h"


/* Private helper function prototypes */

static int ft747_get_update_data(RIG *rig);
static int ft747_send_priv_cmd(RIG *rig, unsigned char ci);

/* Native ft747 cmd set prototypes. These are READ ONLY as each */
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


#define FT747_ALL_RX_MODES (RIG_MODE_AM|RIG_MODE_CW|RIG_MODE_USB|RIG_MODE_LSB)
#define FT747_SSB_CW_RX_MODES (RIG_MODE_CW|RIG_MODE_USB|RIG_MODE_LSB)
#define FT747_AM_RX_MODES (RIG_MODE_AM)
#define FT747_FM_RX_MODES (RIG_MODE_FM)


/* 
 * TX caps
 */ 

#define FT747_OTHER_TX_MODES (RIG_MODE_CW| RIG_MODE_USB| RIG_MODE_LSB )	/* 100 W class */
#define FT747_AM_TX_MODES (RIG_MODE_AM ) /* set 25W max */

#define FT747_FUNC_ALL (RIG_FUNC_FAGC|RIG_FUNC_NB|RIG_FUNC_COMP|RIG_FUNC_VOX|RIG_FUNC_TONE|RIG_FUNC_TSQL|RIG_FUNC_SBKIN|RIG_FUNC_FBKIN)	/* fix */

/*
 * ft747 rigs capabilities.
 * Also this struct is READONLY!
 */

const struct rig_caps ft747_caps = {
  RIG_MODEL_FT747, "FT-747GX", "Yaesu", "0.1", "GPL?",
  RIG_STATUS_ALPHA, RIG_TYPE_MOBILE, 
  RIG_PTT_RIG, RIG_DCD_NONE, RIG_PORT_SERIAL,
  4800, 4800, 8, 2, RIG_PARITY_NONE, RIG_HANDSHAKE_NONE, 
  FT747_WRITE_DELAY, FT747_POST_WRITE_DELAY, 2000, 0,
  FT747_FUNC_ALL, FT747_FUNC_ALL, RIG_LEVEL_NONE, RIG_LEVEL_NONE,
  RIG_PARM_NONE, RIG_PARM_NONE,	/* FIXME: parms */
  NULL, NULL,	/* FIXME: CTCSS/DCS list */
  { RIG_DBLST_END, },	/* FIXME! */
  { RIG_DBLST_END, },
  NULL,
  Hz(9999), Hz(0),	/* RIT, IF-SHIFT */
  0,			/* FIXME: VFO list */
  0, RIG_TRN_OFF,
  20, 0, 0,

  { RIG_CHAN_END, },	/* FIXME: memory channel list */

  { RIG_FRNG_END, },    /* FIXME: enter region 1 setting */
  { RIG_FRNG_END, },
  { {100000,29999900,FT747_ALL_RX_MODES,-1,-1}, RIG_FRNG_END, }, /* rx range */
  
  { {1500000,1999900,FT747_OTHER_TX_MODES,5000,100000},	/* 100W class */ 
    {1500000,1999900,FT747_AM_TX_MODES,2000,25000},	/* 25W class */
    
    {3500000,3999900,FT747_OTHER_TX_MODES,5000,100000},
    {3500000,3999900,FT747_AM_TX_MODES,2000,25000},
    
    {7000000,7499900,FT747_OTHER_TX_MODES,5000,100000},
    {7000000,7499900,FT747_AM_TX_MODES,2000,25000},
    
    {10000000,10499900,FT747_OTHER_TX_MODES,5000,100000},
    {10000000,10499900,FT747_AM_TX_MODES,2000,25000},
    
    {14000000,14499900,FT747_OTHER_TX_MODES,5000,100000},
    {14000000,14499900,FT747_AM_TX_MODES,2000,25000},
    
    {18000000,18499900,FT747_OTHER_TX_MODES,5000,100000},
    {18000000,18499900,FT747_AM_TX_MODES,2000,25000},
    
    {21000000,21499900,FT747_OTHER_TX_MODES,5000,100000},
    {21000000,21499900,FT747_AM_TX_MODES,2000,25000},
    
    {24500000,24999900,FT747_OTHER_TX_MODES,5000,100000},
    {24500000,24999900,FT747_AM_TX_MODES,2000,25000},
    
    {28000000,29999900,FT747_OTHER_TX_MODES,5000,100000},
    {28000000,29999900,FT747_AM_TX_MODES,2000,25000},
    
    RIG_FRNG_END, },
  
  { {FT747_SSB_CW_RX_MODES,25}, /* fast off */
    {FT747_SSB_CW_RX_MODES,2500}, /* fast on */
    
    {FT747_AM_RX_MODES,kHz(1)}, /* fast off */
    {FT747_AM_RX_MODES,kHz(10)}, /* fast on */
    
    {FT747_FM_RX_MODES,kHz(5)}, /* fast off */
    {FT747_FM_RX_MODES,12500}, /* fast on */
    
    RIG_TS_END,
  },  
      /* mode/filter list, remember: order matters! */
  {
		/* FIXME! */
		RIG_FLT_END,
  },
  NULL,	/* priv */

  ft747_init, 
  ft747_cleanup, 
  ft747_open,				/* port opened */
  ft747_close,				/* port closed */
  NULL,				/* probe not supported yet */

  ft747_set_freq,		/* set freq */
  ft747_get_freq,		/* get freq */
  ft747_set_mode,		/* set mode */
  ft747_get_mode,		/* get mode */
  ft747_set_vfo,		/* set vfo */
  ft747_get_vfo,		/* get vfo */
  ft747_set_ptt,		/* set ptt */
  ft747_get_ptt,		/* get ptt */

  NULL,				/* add later */
  NULL,				/* add later */

};


/*
 * _init 
 *
 */


int ft747_init(RIG *rig) {
  struct ft747_priv_data *p;
  
  if (!rig)
    return -RIG_EINVAL;
  
  p = (struct ft747_priv_data*)malloc(sizeof(struct ft747_priv_data));
  if (!p)			/* whoops! memory shortage! */    
    return -RIG_ENOMEM;
  
  rig_debug(RIG_DEBUG_VERBOSE,"ft747:ft747_init called \n");

  /* 
   * Copy native cmd set to private cmd storage area 
   */

  memcpy(p->pcs,ncmd,sizeof(ncmd));

  /* TODO: read pacing from preferences */

  p->pacing = FT747_PACING_DEFAULT_VALUE; /* set pacing to minimum for now */
  p->read_update_delay = FT747_DEFAULT_READ_TIMEOUT; /* set update timeout to safe value */
  p->current_vfo =  RIG_VFO_A;	/* default to VFO_A ? */
  rig->state.priv = (void*)p;
  
  return RIG_OK;
}


/*
 * ft747_cleanup routine
 * the serial port is closed by the frontend
 */

int ft747_cleanup(RIG *rig) {
  if (!rig)
    return -RIG_EINVAL;
  
  rig_debug(RIG_DEBUG_VERBOSE, "ft747: _cleanup called\n");

  if (rig->state.priv)
    free(rig->state.priv);
  rig->state.priv = NULL;
  
  return RIG_OK;
}

/*
 * ft747_open  routine
 * 
 */

int ft747_open(RIG *rig) {
  struct rig_state *rig_s;
 
  if (!rig)
    return -RIG_EINVAL;

  rig_s = &rig->state;

  rig_debug(RIG_DEBUG_VERBOSE,"ft747:rig_open: write_delay = %i msec \n", rig_s->write_delay);
  rig_debug(RIG_DEBUG_VERBOSE,"ft747:rig_open: post_write_delay = %i msec \n", rig_s->post_write_delay);

  
   /* TODO */

  return RIG_OK;
}


/*
 * ft747_close  routine
 * 
 */

int ft747_close(RIG *rig) {

  if (!rig)
    return -RIG_EINVAL;

  rig_debug(RIG_DEBUG_VERBOSE,"ft747:ft747_close called \n");

  return RIG_OK;
}


/*
 * Example of wrapping backend function inside frontend API
 * 	 
 */


int ft747_set_freq(RIG *rig, vfo_t vfo, freq_t freq) {
  struct rig_state *rig_s;
  struct ft747_priv_data *p;
  unsigned char *cmd;		/* points to sequence to send */

  if (!rig)
    return -RIG_EINVAL;


  p = (struct ft747_priv_data*)rig->state.priv;

  rig_s = &rig->state;

  rig_debug(RIG_DEBUG_VERBOSE,"ft747: requested freq = %Li Hz \n", freq);

  /* frontend sets VFO now , if targetable_vfo = 0 */

#if 0
  ft747_set_vfo(rig, vfo);	/* select VFO first , new API */
#endif

  /* 
   * Copy native cmd freq_set to private cmd storage area 
   */

  memcpy(&p->p_cmd,&ncmd[FT_747_NATIVE_FREQ_SET].nseq,YAESU_CMD_LENGTH);  

  to_bcd(p->p_cmd,freq/10,8);	/* store bcd format in in p_cmd */
				/* TODO -- fix 10Hz resolution -- FS */

  rig_debug(RIG_DEBUG_VERBOSE,"ft747: requested freq after conversion = %Li Hz \n", from_bcd(p->p_cmd,8)* 10 );

  cmd = p->p_cmd; /* get native sequence */
  write_block(rig_s->fd, cmd, YAESU_CMD_LENGTH, rig_s->write_delay, rig_s->post_write_delay);

  return RIG_OK;
}


/*
 * Return Freq for a given VFO
 */

int ft747_get_freq(RIG *rig, vfo_t vfo, freq_t *freq) {
  struct ft747_priv_data *p;
  freq_t f;

  rig_debug(RIG_DEBUG_VERBOSE,"ft747:ft747_get_freq called \n");

  if (!rig)
    return -RIG_EINVAL;
  
  p = (struct ft747_priv_data*)rig->state.priv;

  ft747_get_update_data(rig);	/* get whole shebang from rig */

  if (vfo == RIG_VFO_CURR )
    vfo = p->current_vfo;	/* from previous vfo cmd */

  switch(vfo) {
  case RIG_VFO_A:
    f = from_bcd_be(&(p->update_data[FT747_SUMO_VFO_A_FREQ]),8); /* grab freq and convert */
    break;
  case RIG_VFO_B:
    f = from_bcd_be(&(p->update_data[FT747_SUMO_VFO_B_FREQ]),8); /* grab freq and convert */
  break;
  default:
    return -RIG_EINVAL;		/* sorry, wrong VFO */
  }

  rig_debug(RIG_DEBUG_VERBOSE,"ft747:  freq = %Li Hz  for VFO = %u \n", f, vfo);

  (*freq) = f;			/* return diplayed frequency */

  return RIG_OK;
}


/*
 * set mode : eg AM, CW etc for a given VFO
 *
 */

int ft747_set_mode(RIG *rig, vfo_t vfo, rmode_t mode, pbwidth_t width ) {
  unsigned char cmd_index;	/* index of sequence to send */

  if (!rig)
    return -RIG_EINVAL;

  /* frontend sets VFO now , if targetable_vfo = 0 */

#if 0
  ft747_set_vfo(rig, vfo);	/* select VFO first , new API */ 
#endif

  /* 
   * translate mode from generic to ft747 specific
   */

  rig_debug(RIG_DEBUG_VERBOSE,"ft747: generic mode = %x \n", mode);

  switch(mode) {
  case RIG_MODE_AM:
    cmd_index = FT_747_NATIVE_MODE_SET_AMW;
    break;
  case RIG_MODE_CW:
    cmd_index = FT_747_NATIVE_MODE_SET_CWW;
    break;
  case RIG_MODE_USB:
    cmd_index = FT_747_NATIVE_MODE_SET_USB;
    break;
  case RIG_MODE_LSB:
    cmd_index = FT_747_NATIVE_MODE_SET_LSB;
    break;
  case RIG_MODE_FM:
    cmd_index = FT_747_NATIVE_MODE_SET_FMW;
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

  case RIG_PASSBAND_WIDE:
    return -RIG_EINVAL;		/* sorry, WIDE WIDTH is not supported */

  case RIG_PASSBAND_NARROW:	/* must set narrow */
    switch(mode) {
    case RIG_MODE_AM:
      cmd_index = FT_747_NATIVE_MODE_SET_AMN;
      break;
    case RIG_MODE_FM:
      cmd_index = FT_747_NATIVE_MODE_SET_FMN;
      break;
    case RIG_MODE_CW:
      cmd_index = FT_747_NATIVE_MODE_SET_CWN;
      break;
    default:
      return -RIG_EINVAL;		/* sorry, wrong MODE/WIDTH combo  */    
    }
    break;

  default:
    return -RIG_EINVAL;		/* sorry, wrong WIDTH requested  */    
  }
  
  /*
   * phew! now send cmd to rig
   */ 

  ft747_send_priv_cmd(rig,cmd_index);

  rig_debug(RIG_DEBUG_VERBOSE,"ft747: cmd_index = %i \n", cmd_index);

  return RIG_OK;		/* good */
  
}


int ft747_get_mode(RIG *rig, vfo_t vfo, rmode_t *mode, pbwidth_t *width) {
  struct ft747_priv_data *p;
  unsigned char mymode;		/* ft747 mode */

  if (!rig)
    return -RIG_EINVAL;
  
  p = (struct ft747_priv_data*)rig->state.priv;

  ft747_get_update_data(rig);	/* get whole shebang from rig */
  
  mymode = p->update_data[FT747_SUMO_DISPLAYED_MODE];
  mymode &= MODE_MASK; /* mask out bits 5 and 6 */
  
  rig_debug(RIG_DEBUG_VERBOSE,"ft747: mymode = %x \n", mymode);

  /* 
   * translate mode from ft747 to generic.
   */

  switch(mymode) {
  case MODE_FM:
    (*mode) = RIG_MODE_FM;
    rig_debug(RIG_DEBUG_VERBOSE,"ft747: mode = FM \n");
    break;
  case MODE_AM:
    (*mode) = RIG_MODE_AM;
    rig_debug(RIG_DEBUG_VERBOSE,"ft747: mode = AM \n");
    break;
  case MODE_CW:
    (*mode) = RIG_MODE_CW;
    rig_debug(RIG_DEBUG_VERBOSE,"ft747: mode = CW \n");
    break;
  case MODE_USB:
    (*mode) = RIG_MODE_USB;
    rig_debug(RIG_DEBUG_VERBOSE,"ft747: mode = USB \n");
    break;
  case MODE_LSB:
    (*mode) = RIG_MODE_LSB;
    rig_debug(RIG_DEBUG_VERBOSE,"ft747: mode = LSB \n");
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
int ft747_set_vfo(RIG *rig, vfo_t vfo) {
  struct rig_state *rig_s;
  struct ft747_priv_data *p;
  unsigned char cmd_index;	/* index of sequence to send */


  if (!rig)
    return -RIG_EINVAL;
  
  p = (struct ft747_priv_data*)rig->state.priv;
  rig_s = &rig->state;
  
  
  /* 
   * TODO : check for errors -- FS
   */


  switch(vfo) {
  case RIG_VFO_A:
    cmd_index = FT_747_NATIVE_VFO_A;
    p->current_vfo = vfo;		/* update active VFO */
    break;
  case RIG_VFO_B:
    cmd_index = FT_747_NATIVE_VFO_B;
    p->current_vfo = vfo;		/* update active VFO */
    break;
  case RIG_VFO_CURR:
    switch(p->current_vfo) {	/* what is my active VFO ? */
    case RIG_VFO_A:
      cmd_index = FT_747_NATIVE_VFO_A;
      break;
    case RIG_VFO_B:
      cmd_index = FT_747_NATIVE_VFO_B;
      break;
    default:
      rig_debug(RIG_DEBUG_VERBOSE,"ft747: Unknown default VFO \n");
      return -RIG_EINVAL;		/* sorry, wrong current VFO */
    }

    break;

  default:
    return -RIG_EINVAL;		/* sorry, wrong VFO */
  }
  
  /*
   * phew! now send cmd to rig
   */ 

  ft747_send_priv_cmd(rig,cmd_index);

  return RIG_OK;

}


int ft747_get_vfo(RIG *rig, vfo_t *vfo) {
  struct ft747_priv_data *p;
  unsigned char status;		/* ft747 status flag */

  if (!rig)
    return -RIG_EINVAL;
  
  p = (struct ft747_priv_data*)rig->state.priv;

  ft747_get_update_data(rig);	/* get whole shebang from rig */
  
  status = p->update_data[FT747_SUMO_DISPLAYED_STATUS];
  status &= SF_VFOAB; /* check VFO bit*/
  
  rig_debug(RIG_DEBUG_VERBOSE,"ft747: vfo status = %x \n", status);

  /* 
   * translate vfo status from ft747 to generic.
   */

  if (status) {
    rig_debug(RIG_DEBUG_VERBOSE,"ft747: VFO = B \n");
    (*vfo) = RIG_VFO_B;
    return RIG_OK; 
  } else {
    rig_debug(RIG_DEBUG_VERBOSE,"ft747: VFO = A \n");
    (*vfo) = RIG_VFO_A;
    return RIG_OK;
  }

}


int ft747_set_ptt(RIG *rig,vfo_t vfo, ptt_t ptt) {
  unsigned char cmd_index;	/* index of sequence to send */

  if (!rig)
    return -RIG_EINVAL;

  /* frontend sets VFO now , if targetable_vfo = 0 */
  
#if 0
  ft747_set_vfo(rig,vfo);	/* select VFO first */
#endif

  switch(ptt) {
  case RIG_PTT_OFF:
    cmd_index = FT_747_NATIVE_PTT_OFF;
    break;
  case RIG_PTT_ON:
    cmd_index = FT_747_NATIVE_PTT_ON;
    break;
  default:
    return -RIG_EINVAL;		/* sorry, wrong VFO */
  }

  /*
   * phew! now send cmd to rig
   */ 

  ft747_send_priv_cmd(rig,cmd_index);

  return RIG_OK;		/* good */
}

int ft747_get_ptt(RIG *rig, vfo_t vfo, ptt_t *ptt) {
  struct ft747_priv_data *p;
  unsigned char status;		/* ft747 mode */

  if (!rig)
    return -RIG_EINVAL;
  
  p = (struct ft747_priv_data*)rig->state.priv;

  ft747_get_update_data(rig);	/* get whole shebang from rig */
  
  status = p->update_data[FT747_SUMO_DISPLAYED_STATUS];
  status = status & SF_RXTX; /* check RXTX bit*/
  
  rig_debug(RIG_DEBUG_VERBOSE,"ft747: ptt status = %x \n", status);

  /* 
   * translate mode from ft747 to generic.
   */

  if (status) {
    rig_debug(RIG_DEBUG_VERBOSE,"ft747: PTT = ON \n");
    (*ptt) = RIG_PTT_ON;
    return RIG_OK; 
  } else {
    rig_debug(RIG_DEBUG_VERBOSE,"ft747: PTT = OFF \n");
    (*ptt) = RIG_PTT_OFF;
    return RIG_OK;
  }

}


/*
 * private helper function. Retrieves update data from rig.
 * using pacing value and buffer indicated in *priv struct.
 *
 * need to use this when doing ft747_get_* stuff
 */

static int ft747_get_update_data(RIG *rig) {
  struct rig_state *rig_s;
  struct ft747_priv_data *p;
  unsigned char *cmd;		/* points to sequence to send */
  int n;			/* for read_  */

  if (!rig)
    return -RIG_EINVAL;
  
  p = (struct ft747_priv_data*)rig->state.priv;
  rig_s = &rig->state;

  /* 
   * Copy native cmd PACING  to private cmd storage area 
   */

  memcpy(&p->p_cmd,&ncmd[FT_747_NATIVE_PACING].nseq,YAESU_CMD_LENGTH);  
  p->p_cmd[3] = p->pacing;		/* get pacing value, and store in private cmd */
  rig_debug(RIG_DEBUG_VERBOSE,"ft747: read pacing = %i \n",p->pacing);

   /* send PACING cmd to rig  */

  cmd = p->p_cmd;
  write_block(rig_s->fd, cmd, YAESU_CMD_LENGTH, rig_s->write_delay, rig_s->post_write_delay);

  /* send UPDATE comand to fetch data*/

  ft747_send_priv_cmd(rig,FT_747_NATIVE_UPDATE);

  /*    n = read_sleep(rig_s->fd,p->update_data, FT747_STATUS_UPDATE_DATA_LENGTH, FT747_DEFAULT_READ_TIMEOUT);  */
  n = read_block(rig_s->fd,p->update_data, FT747_STATUS_UPDATE_DATA_LENGTH, FT747_DEFAULT_READ_TIMEOUT); 

  return 0;

}



/*
 * init_ft747 is called by rig_backend_load
 */

int init_ft747(void *be_handle) {
  rig_debug(RIG_DEBUG_VERBOSE, "ft747: _init called\n");
  rig_register(&ft747_caps); 
  return RIG_OK;
}



/*
 * private helper function to send a private command
 * sequence . Must only be complete sequences.
 * TODO: place variant of this in yaesu.c
 */

static int ft747_send_priv_cmd(RIG *rig, unsigned char ci) {

  struct rig_state *rig_s;
  struct ft747_priv_data *p;
  unsigned char *cmd;		/* points to sequence to send */
  unsigned char cmd_index;	/* index of sequence to send */
 
  if (!rig)
    return -RIG_EINVAL;


  p = (struct ft747_priv_data*)rig->state.priv;
  rig_s = &rig->state;
  
  cmd_index = ci;		/* get command */

  if (! p->pcs[cmd_index].ncomp) {
    rig_debug(RIG_DEBUG_VERBOSE,"ft747: Attempt to send incomplete sequence \n");
    return -RIG_EINVAL;
  }

  cmd = (unsigned char *) p->pcs[cmd_index].nseq; /* get native sequence */
  write_block(rig_s->fd, cmd, YAESU_CMD_LENGTH, rig_s->write_delay, rig_s->post_write_delay);
  
  return RIG_OK;

}

