/*
 * hamlib - (C) Frank Singleton 2000 (vk3fcs@ix.netcom.com)
 *
 * ft847.c - (C) Frank Singleton 2000 (vk3fcs@ix.netcom.com)
 * This shared library provides an API for communicating
 * via serial interface to an FT-847 using the "CAT" interface.
 *
 *
 * $Id: ft847.c,v 1.26 2000-12-18 05:17:45 javabear Exp $  
 *
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
 * TODO - Remove static stuff, see ft747 for new style.
 *      - create yaesu.h for common command structure etc..
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
#include "ft847.h"
#include "misc.h"

/* prototypes */


/* Native ft847 cmd set prototypes. These are READ ONLY as each */
/* rig instance will copy from these and modify if required . */
/* Complete sequences (1) can be read and used directly as a cmd sequence . */
/* Incomplete sequences (0) must be completed with extra parameters */
/* eg: mem number, or freq etc.. */

static const ft847_cmd_set_t ncmd[] = { 
  { 1, { 0x00, 0x00, 0x00, 0x00, 0x00 } }, /* CAT = On */
  { 1, { 0x00, 0x00, 0x00, 0x00, 0x80 } }, /* CAT = Off */
  { 1, { 0x00, 0x00, 0x00, 0x00, 0x08 } }, /* ptt on */
  { 1, { 0x00, 0x00, 0x00, 0x01, 0x88 } }, /* ptt off */
  { 1, { 0x00, 0x00, 0x00, 0x00, 0x4e } }, /* sat mode on */
  { 1, { 0x00, 0x00, 0x00, 0x00, 0x8e } }, /* sat mode off */

  { 0, { 0x00, 0x00, 0x00, 0x00, 0x01 } }, /* set freq main */
  { 0, { 0x00, 0x00, 0x00, 0x00, 0x11 } }, /* set freq sat rx */
  { 0, { 0x00, 0x00, 0x00, 0x00, 0x21 } }, /* set freq sat tx */

  { 1, { 0x00, 0x00, 0x00, 0x00, 0x07 } }, /* mode set main LSB */
  { 1, { 0x01, 0x00, 0x00, 0x00, 0x07 } }, /* mode set main USB */
  { 1, { 0x02, 0x00, 0x00, 0x00, 0x07 } }, /* mode set main CW */
  { 1, { 0x03, 0x00, 0x00, 0x00, 0x07 } }, /* mode set main CWR */
  { 1, { 0x04, 0x00, 0x00, 0x00, 0x07 } }, /* mode set main AM */
  { 1, { 0x08, 0x00, 0x00, 0x00, 0x07 } }, /* mode set main FM */
  { 1, { 0x82, 0x00, 0x00, 0x00, 0x07 } }, /* mode set main CWN */
  { 1, { 0x83, 0x00, 0x00, 0x00, 0x07 } }, /* mode set main CWRN */
  { 1, { 0x84, 0x00, 0x00, 0x00, 0x07 } }, /* mode set main AMN */
  { 1, { 0x88, 0x00, 0x00, 0x00, 0x07 } }, /* mode set main FMN */

  { 1, { 0x00, 0x00, 0x00, 0x00, 0x17 } }, /* mode set sat rx LSB */
  { 1, { 0x01, 0x00, 0x00, 0x00, 0x17 } }, /* mode set sat rx USB */
  { 1, { 0x02, 0x00, 0x00, 0x00, 0x17 } }, /* mode set sat rx CW */
  { 1, { 0x03, 0x00, 0x00, 0x00, 0x17 } }, /* mode set sat rx CWR */
  { 1, { 0x04, 0x00, 0x00, 0x00, 0x17 } }, /* mode set sat rx AM */
  { 1, { 0x08, 0x00, 0x00, 0x00, 0x17 } }, /* mode set sat rx FM */
  { 1, { 0x82, 0x00, 0x00, 0x00, 0x17 } }, /* mode set sat rx CWN */
  { 1, { 0x83, 0x00, 0x00, 0x00, 0x17 } }, /* mode set sat rx CWRN */
  { 1, { 0x84, 0x00, 0x00, 0x00, 0x17 } }, /* mode set sat rx AMN */
  { 1, { 0x88, 0x00, 0x00, 0x00, 0x17 } }, /* mode set sat rx FMN */

  { 1, { 0x00, 0x00, 0x00, 0x00, 0x27 } }, /* mode set sat tx LSB */
  { 1, { 0x01, 0x00, 0x00, 0x00, 0x27 } }, /* mode set sat tx USB */
  { 1, { 0x02, 0x00, 0x00, 0x00, 0x27 } }, /* mode set sat tx CW */
  { 1, { 0x03, 0x00, 0x00, 0x00, 0x27 } }, /* mode set sat tx CWR */
  { 1, { 0x04, 0x00, 0x00, 0x00, 0x27 } }, /* mode set sat tx AM */
  { 1, { 0x08, 0x00, 0x00, 0x00, 0x27 } }, /* mode set sat tx FM */
  { 1, { 0x82, 0x00, 0x00, 0x00, 0x27 } }, /* mode set sat tx CWN */
  { 1, { 0x83, 0x00, 0x00, 0x00, 0x27 } }, /* mode set sat tx CWRN */
  { 1, { 0x84, 0x00, 0x00, 0x00, 0x27 } }, /* mode set sat tx AMN */
  { 1, { 0x88, 0x00, 0x00, 0x00, 0x27 } }, /* mode set sat tx FMN */

  { 1, { 0x0a, 0x00, 0x00, 0x00, 0x0a } }, /* set DCS on, main */
  { 1, { 0x2a, 0x00, 0x00, 0x00, 0x0a } }, /* set CTCSS/DCS enc/dec on, main */
  { 1, { 0x4a, 0x00, 0x00, 0x00, 0x0a } }, /* set CTCSS/DCS enc on, main */
  { 1, { 0x8a, 0x00, 0x00, 0x00, 0x0a } }, /* set CTCSS/DCS off, main */

  { 1, { 0x0a, 0x00, 0x00, 0x00, 0x1a } }, /* set DCS on, sat rx */
  { 1, { 0x2a, 0x00, 0x00, 0x00, 0x1a } }, /* set CTCSS/DCS enc/dec on, sat rx */
  { 1, { 0x4a, 0x00, 0x00, 0x00, 0x1a } }, /* set CTCSS/DCS enc on, sat rx */
  { 1, { 0x8a, 0x00, 0x00, 0x00, 0x1a } }, /* set CTCSS/DCS off, sat rx */

  { 1, { 0x0a, 0x00, 0x00, 0x00, 0x2a } }, /* set DCS on, sat tx */
  { 1, { 0x2a, 0x00, 0x00, 0x00, 0x2a } }, /* set CTCSS/DCS enc/dec on, sat tx */
  { 1, { 0x4a, 0x00, 0x00, 0x00, 0x2a } }, /* set CTCSS/DCS enc on, sat tx */
  { 1, { 0x8a, 0x00, 0x00, 0x00, 0x2a } }, /* set CTCSS/DCS off, sat tx */

  { 0, { 0x00, 0x00, 0x00, 0x00, 0x0b } }, /* set CTCSS  freq, main */
  { 0, { 0x00, 0x00, 0x00, 0x00, 0x1b } }, /* set CTCSS  freq, sat rx */
  { 0, { 0x00, 0x00, 0x00, 0x00, 0x2b } }, /* set CTCSS  freq, sat tx */

  { 0, { 0x00, 0x00, 0x00, 0x00, 0x0c } }, /* set DCS code, main */
  { 0, { 0x00, 0x00, 0x00, 0x00, 0x1c } }, /* set DCS code, sat rx */
  { 0, { 0x00, 0x00, 0x00, 0x00, 0x2c } }, /* set DCS code, sat tx */

  { 0, { 0x00, 0x00, 0x00, 0x00, 0x09 } }, /* set RPT shift MINUS */
  { 0, { 0x00, 0x00, 0x00, 0x00, 0x49 } }, /* set RPT shift PLUS */
  { 0, { 0x00, 0x00, 0x00, 0x00, 0x89 } }, /* set RPT shift SIMPLEX */

  { 0, { 0x00, 0x00, 0x00, 0x00, 0xf9 } }, /* set RPT offset freq */

  { 1, { 0x00, 0x00, 0x00, 0x00, 0xe7 } }, /* get RX status  */
  { 1, { 0x00, 0x00, 0x00, 0x00, 0xf7 } }, /* get TX status  */

  { 1, { 0x00, 0x00, 0x00, 0x00, 0x03 } }, /* get FREQ and MODE status, main  */
  { 1, { 0x00, 0x00, 0x00, 0x00, 0x13 } }, /* get FREQ and MODE status, sat rx  */
  { 1, { 0x00, 0x00, 0x00, 0x00, 0x23 } }, /* get FREQ and MODE status, sat tx  */

};


/* 
 * Receiver caps 
 */

#if 0
#define FT847_ALL_RX_MODES (RIG_MODE_AM| RIG_MODE_CW| RIG_MODE_USB| RIG_MODE_LSB| RIG_MODE_RTTY| RIG_MODE_FM| RIG_MODE_WFM| RIG_MODE_NFM| RIG_MODE_NAM| RIG_MODE_CWR)
#define FT847_SSB_CW_RX_MODES (RIG_MODE_CW| RIG_MODE_USB| RIG_MODE_LSB| RIG_MODE_NCW)
#define FT847_AM_FM_RX_MODES (RIG_MODE_AM| RIG_MODE_NAM |RIG_MODE_FM |RIG_MODE_NFM )
#else
#define FT847_ALL_RX_MODES (RIG_MODE_AM|RIG_MODE_CW|RIG_MODE_USB|RIG_MODE_LSB|RIG_MODE_RTTY|RIG_MODE_FM)
#define FT847_SSB_CW_RX_MODES (RIG_MODE_CW|RIG_MODE_USB|RIG_MODE_LSB)
#define FT847_AM_FM_RX_MODES (RIG_MODE_AM|RIG_MODE_FM)
#endif

/* tx doesn't have WFM.
 * 100W in 160-6m (25 watts AM carrier)
 * 50W in 2m/70cm (12.5 watts AM carrier)
 */ 
#if 0
#define FT847_OTHER_TX_MODES (RIG_MODE_AM| RIG_MODE_CW| RIG_MODE_USB| RIG_MODE_LSB| RIG_MODE_RTTY| RIG_MODE_FM| RIG_MODE_NFM| RIG_MODE_NAM| RIG_MODE_CWR)
#define FT847_AM_TX_MODES (RIG_MODE_AM| RIG_MODE_NAM)
#else
#define FT847_OTHER_TX_MODES (RIG_MODE_AM|RIG_MODE_CW|RIG_MODE_USB|RIG_MODE_LSB|RIG_MODE_RTTY|RIG_MODE_FM)
#define FT847_AM_TX_MODES (RIG_MODE_AM)
#endif

#define FT847_FUNC_ALL (RIG_FUNC_FAGC|RIG_FUNC_NB|RIG_FUNC_COMP|RIG_FUNC_VOX|RIG_FUNC_TONE|RIG_FUNC_TSQL|RIG_FUNC_SBKIN|RIG_FUNC_FBKIN)	/* fix */

/*
 * ft847 rigs capabilities.
 * Notice that some rigs share the same functions.
 * Also this struct is READONLY!
 */
const struct rig_caps ft847_caps = {
  RIG_MODEL_FT847, "FT-847", "Yaesu", "0.1", RIG_STATUS_ALPHA,
  RIG_TYPE_TRANSCEIVER,RIG_PTT_NONE, 4800, 57600, 8, 2, RIG_PARITY_NONE, 
  RIG_HANDSHAKE_NONE,FT847_WRITE_DELAY ,FT847_POST_WRITE_DELAY, 100, 0, FT847_FUNC_ALL, 0, 0, 78, RIG_TRN_OFF,
  { {100000,76000000,FT847_ALL_RX_MODES,-1,-1}, /* rx range begin */
    {108000000,174000000,FT847_ALL_RX_MODES,-1,-1},
    {420000000,512000000,FT847_ALL_RX_MODES,-1,-1},

    {0,0,0,0,0}, }, /* rx range end */

  { {1800000,1999999,FT847_OTHER_TX_MODES,5000,100000},	/* 5-100W class */
    {1800000,1999999,FT847_AM_TX_MODES,1000,25000},	/* 1-5W class */

    {3500000,3999999,FT847_OTHER_TX_MODES,5000,100000},
    {3500000,3999999,FT847_AM_TX_MODES,1000,25000},

    {7000000,7300000,FT847_OTHER_TX_MODES,5000,100000},
    {7000000,7300000,FT847_AM_TX_MODES,1000,25000},

    {10000000,10150000,FT847_OTHER_TX_MODES,5000,100000},
    {10000000,10150000,FT847_AM_TX_MODES,1000,25000},

    {14000000,14350000,FT847_OTHER_TX_MODES,5000,100000},
    {14000000,14350000,FT847_AM_TX_MODES,1000,25000},

    {18068000,18168000,FT847_OTHER_TX_MODES,5000,100000},
    {18068000,18168000,FT847_AM_TX_MODES,1000,25000},

    {21000000,21450000,FT847_OTHER_TX_MODES,5000,100000},
    {21000000,21450000,FT847_AM_TX_MODES,1000,25000},

    {24890000,24990000,FT847_OTHER_TX_MODES,5000,100000},
    {24890000,24990000,FT847_AM_TX_MODES,1000,25000},

    {28000000,29700000,FT847_OTHER_TX_MODES,5000,100000},
    {28000000,29700000,FT847_AM_TX_MODES,1000,25000},

    {50000000,54000000,FT847_OTHER_TX_MODES,5000,100000},
    {50000000,54000000,FT847_AM_TX_MODES,1000,25000},

    {144000000,148000000,FT847_OTHER_TX_MODES,1000,50000}, 
    {144000000,148000000,FT847_AM_TX_MODES,1000,12500}, 

    {430000000,44000000,FT847_OTHER_TX_MODES,1000,50000}, /* check range */
    {430000000,440000000,FT847_AM_TX_MODES,1000,12500},

    {0,0,0,0,0} },

  { {FT847_SSB_CW_RX_MODES,1}, /* normal */
    {FT847_SSB_CW_RX_MODES,10}, /* fast */
    {FT847_SSB_CW_RX_MODES,100}, /* faster */

    
    {FT847_AM_FM_RX_MODES,10}, /* normal */
    {FT847_AM_FM_RX_MODES,100}, /* fast  */
        
    {0,0},
  },  
  ft847_init, 
  ft847_cleanup, 
  ft847_open, 
  ft847_close, 
  NULL /* probe not supported yet */,

  ft847_set_freq,		/* set freq */
  ft847_get_freq,		/* get freq */
  ft847_set_mode,		/* set mode */
  ft847_get_mode,		/* get mode */
  ft847_set_vfo,		/* set vfo */
  ft847_get_vfo,		/* get vfo */
  ft847_set_ptt,		/* set ptt */
  ft847_get_ptt,		/* get ptt */

  NULL,
  NULL,
};


/*
 * Function definitions below
 */

/*
 * setup *priv 
 * serial port is already open (rig->state->fd)
 */

int ft847_init(RIG *rig) {
  struct ft847_priv_data *p;
  
  if (!rig)
    return -RIG_EINVAL;
  
  p = (struct ft847_priv_data*)malloc(sizeof(struct ft847_priv_data));
  if (!p) {
				/* whoops! memory shortage! */
    return -RIG_ENOMEM;
  }

  rig_debug(RIG_DEBUG_VERBOSE,"ft847:ft847_init called \n");

  /* 
   * Copy native cmd set to private cmd storage area 
   */

  memcpy(p->pcs,ncmd,sizeof(ncmd));

  p->current_vfo =  RIG_VFO_A;	/* default to VFO_A */
  rig->state.priv = (void*)p;
  
  return RIG_OK;
}


/*
 * ft847_cleanup routine
 * the serial port is closed by the frontend
 */

int ft847_cleanup(RIG *rig) {
  if (!rig)
    return -RIG_EINVAL;
  
  if (rig->state.priv)
    free(rig->state.priv);
  rig->state.priv = NULL;
  
  return RIG_OK;
}


/*
 * ft847_open  routine
 * 
 */

int ft847_open(RIG *rig) {
  struct rig_state *rig_s;
  struct ft847_priv_data *p;
  unsigned char *cmd;		/* points to sequence to send */
  unsigned char cmd_index;	/* index of sequence to send */

 
  if (!rig)
    return -RIG_EINVAL;

  p = (struct ft847_priv_data*)rig->state.priv;
  rig_s = &rig->state;
  
  /* Good time to set CAT ON */

  cmd_index = FT_847_NATIVE_CAT_ON;
  cmd = (unsigned char *) p->pcs[cmd_index].nseq; /* get native sequence */
  write_block(rig_s->fd, cmd, FT847_CMD_LENGTH, rig_s->write_delay, rig_s->post_write_delay);  

  return RIG_OK;
}

/*
 * ft847_close  routine
 * 
 */

int ft847_close(RIG *rig) {
  struct rig_state *rig_s;
  struct ft847_priv_data *p;
  unsigned char *cmd;		/* points to sequence to send */
  unsigned char cmd_index;	/* index of sequence to send */
 
  if (!rig)
    return -RIG_EINVAL;

  p = (struct ft847_priv_data*)rig->state.priv;
  rig_s = &rig->state;

  /* Good time to set CAT OFF */
  
  cmd_index = FT_847_NATIVE_CAT_OFF;
  cmd = (unsigned char *) p->pcs[cmd_index].nseq; /* get native sequence */
  write_block(rig_s->fd, cmd, FT847_CMD_LENGTH, rig_s->write_delay, rig_s->post_write_delay);
  
  return RIG_OK;
}


/*
 * Example of wrapping backend function inside frontend API
 * 	 
 */


int ft847_set_freq(RIG *rig, vfo_t vfo, freq_t freq) {
  struct rig_state *rig_s;
  struct ft847_priv_data *p;
  unsigned char *cmd;		/* points to sequence to send */
  unsigned char cmd_index;	/* index of sequence to send */

  if (!rig)
    return -RIG_EINVAL;

  p = (struct ft847_priv_data*)rig->state.priv;

  rig_s = &rig->state;

  rig_debug(RIG_DEBUG_VERBOSE,"ft847: requested freq = %Li Hz \n", freq);


  /* 
   * Copy native cmd freq_set to private cmd storage area 
   */

  /*
   * TODO Is VFO_B = SAT_RX ? -- FS
   */


  rig_debug(RIG_DEBUG_VERBOSE,"ft847: vfo =%i \n", vfo);
  
  switch(vfo) {
  case RIG_VFO_A:
    cmd_index = FT_847_NATIVE_CAT_SET_FREQ_MAIN;
    break;
  case RIG_VFO_B:
    cmd_index = FT_847_NATIVE_CAT_SET_FREQ_SAT_RX_VFO;
    break;
  case RIG_VFO_CURR:
    switch(p->current_vfo) {	/* what is my active VFO ? */
    case RIG_VFO_A:
      cmd_index = FT_847_NATIVE_CAT_SET_FREQ_MAIN;
      break;
    case RIG_VFO_B:
      cmd_index = FT_847_NATIVE_CAT_SET_FREQ_SAT_RX_VFO;
      break;
    default:
      rig_debug(RIG_DEBUG_VERBOSE,"ft847: Unknown default VFO \n");
      return -RIG_EINVAL;		/* sorry, wrong current VFO */
    }
    break;

  default:
    rig_debug(RIG_DEBUG_VERBOSE,"ft847: Unknown  VFO \n");
    return -RIG_EINVAL;		/* sorry, wrong VFO */
  }

  memcpy(&p->p_cmd,&ncmd[cmd_index].nseq,FT847_CMD_LENGTH);  

  to_bcd_be(p->p_cmd,freq/10,8);	/* store bcd format in in p_cmd */
				/* TODO -- fix 10Hz resolution -- FS */

  rig_debug(RIG_DEBUG_VERBOSE,"ft847: requested freq after conversion = %Li Hz \n", from_bcd_be(p->p_cmd,8)* 10 );

  cmd = p->p_cmd; /* get native sequence */
  write_block(rig_s->fd, cmd, FT847_CMD_LENGTH, rig_s->write_delay, rig_s->post_write_delay);

  return RIG_OK;
}

int ft847_get_freq(RIG *rig, vfo_t vfo, freq_t *freq) {
  return -RIG_ENIMPL;
}

int ft847_set_mode(RIG *rig, vfo_t vfo, rmode_t mode, pbwidth_t width) {
  return -RIG_ENIMPL;
}

int ft847_get_mode(RIG *rig, vfo_t vfo, rmode_t *mode, pbwidth_t *width) {
  return -RIG_ENIMPL;
}


/*
 * TODO -- FT847 has NO set_vfo cmd. VFO is specified
 * in other cmd's like set freq, set mode etc..
 *
 * For the time being, simply cache the requested
 * VFO in private active VFO
 * Must fix this later. MAybe store active VFO in
 * frontend instead ?
 *
 * Perhaps I can try a dummy cmd that includes the VFO..
 * eg: set_dcs etc..
 * Try later -- FS
 *
 *
 * Also, handle only VFO_A and VFO_B . Add SAT VFO's later.
 *
 */

int ft847_set_vfo(RIG *rig, vfo_t vfo) {
  struct rig_state *rig_s;
  struct ft847_priv_data *p;

  if (!rig)
    return -RIG_EINVAL;
  
  p = (struct ft847_priv_data*)rig->state.priv;
  rig_s = &rig->state;
  
  
  /* 
   * TODO : check for errors -- FS
   */

  switch(vfo) {
  case RIG_VFO_A:
    p->current_vfo = vfo;		/* update active VFO */
    break;
  case RIG_VFO_B:
    p->current_vfo = vfo;		/* update active VFO */
    break;
  default:
    return -RIG_EINVAL;		/* sorry, wrong VFO */
  }

  return RIG_OK;

}

int ft847_get_vfo(RIG *rig, vfo_t *vfo) {
  return -RIG_ENIMPL;

}


/*
 * _set_ptt
 *
 */


int ft847_set_ptt(RIG *rig, vfo_t vfo, ptt_t ptt) {
  struct rig_state *rig_s;
  struct ft847_priv_data *p;
  unsigned char *cmd;		/* points to sequence to send */
  unsigned char cmd_index;	/* index of sequence to send */

  if (!rig)
    return -RIG_EINVAL;
  
  p = (struct ft847_priv_data*)rig->state.priv;
  rig_s = &rig->state;
    
  /* 
   * TODO : check for errors -- FS
   */

  switch(ptt) {
  case RIG_PTT_ON:
    cmd_index = FT_847_NATIVE_CAT_PTT_ON;
    break;
  case RIG_PTT_OFF:
    cmd_index = FT_847_NATIVE_CAT_PTT_OFF;
    break;
  default:
    return -RIG_EINVAL;		/* sorry, wrong ptt range */
  }

  /*
   * phew! now send cmd to rig
   */ 

  cmd = (unsigned char *) p->pcs[cmd_index].nseq; /* get native sequence */
  write_block(rig_s->fd, cmd, FT847_CMD_LENGTH, rig_s->write_delay, rig_s->post_write_delay);
  
  return RIG_OK;		/* good */

}

int ft847_get_ptt(RIG *rig, vfo_t vfo, ptt_t *ptt) {
  return -RIG_ENIMPL;

}


int ft847_set_freq_main_vfo_hz(RIG *rig, freq_t freq, rmode_t mode) {
  struct ft847_priv_data *p;
  struct rig_state *rig_s;
  unsigned char buf[16];
  int frm_len = 5;		/* fix later */

  /*  examlpe opcode */
  static unsigned char data[] = { 0x00, 0x00, 0x00, 0x01, 0x04 }; /* dial lock = on */

  
  if (!rig)
    return -1;
  
  p = (struct ft847_priv_data*)rig->state.priv;
  rig_s = &rig->state;
  
  /* 
   * should check return code and that write wrote cmd_len chars! 
   */

  write_block(rig_s->fd, data, frm_len, rig_s->write_delay, rig_s->post_write_delay);
  
  /*
   * wait for ACK ... etc.. 
   */
  read_block(rig_s->fd, buf, frm_len, rig_s->timeout);
  
  /*
   * TODO: check address, Command number and ack!
   */
  
  /* then set the mode ... etc. */
  
  return 0;
}






#if 0

#define TRUE    1;
#define FALSE   0;


static unsigned char datain[5]; /* data read from rig */

static long int  calc_freq_from_packed4(unsigned char *in);
static void calc_packed4_from_freq(long int freq, unsigned char *out);


static struct rig_caps rigft847 = {
  "ft847", 4800, 57600, 8, 2, RIG_PARITY_NONE, ""
};


/*
 * Allowable DCS Codes 
 *
 */

static int dcs_allowed[] = { 
  23,25,26,31,32,36,43,47,51,53,54,65,71,
  72,73,74,114,115,116,122,125,131,132,134,143,145,
  152,155,156,162,165,172,174,205,212,223,225,226,243,
  244,245,246,251,252,255,266,263,265,266,271,274,306,
  311,315,325,331,332,343,347,351,356,364,365,371,411,
  412,413,423,431,432,445,446,452,454,455,462,464,465,
  466,503,506,516,523,526,532,546,565,606,612,624,627,
  631,632,654,662,664,703,712,723,731,732,734,743,754

};



/*
 * Function definitions below
 */


/*
 * Open serial connection to rig.
 * returns fd.
 */


/*  int rig_open(char *serial_port) { */
/*    return open_port(serial_port); */
/*  } */

/*
 * Open serial connection to rig using rig_caps
 * returns fd.
 */


int rig_open(struct rig_caps *rc) {
  return open_port2(rc);
}



/*
 * Closes serial connection to rig.
 * 
 */

int rig_close(int fd) {
  return close_port(fd);
}


/*
 * Gets rig capabilities.
 * 
 */

struct rig_caps *rig_get_caps() {
 
  struct rig_caps *rc;
  rc = &rigft847;

  printf("rig = %s \n", rc->rig_name);

  printf("rig serial_rate_min = = %u \n", rc->serial_rate_min);
  printf("rig serial_rate_max = = %u \n", rc->serial_rate_max);


  return  &rigft847;
}


/*
 * Implement OPCODES
 */

void cmd_set_cat_on(int fd) {
  static unsigned char data[] = { 0x00, 0x00, 0x00, 0x00, 0x00 }; /* cat = on */
  write_block(fd,data);
}

void cmd_set_cat_off(int fd) {
  static unsigned char data[] = { 0x00, 0x00, 0x00, 0x00, 0x80 }; /* cat = off */
  write_block(fd,data);
}

void cmd_set_ptt_on(int fd) {

#ifdef TX_ENABLED
  static unsigned char data[] = { 0x00, 0x00, 0x00, 0x00, 0x0f }; /* ptt = on */
  write_block(fd,data);
  printf("cmd_ptt_on complete \n");
#elsif
  printf("cmd_ptt_on disabled \n");
#endif

}


void cmd_set_ptt_off(int fd) {
  static unsigned char data[] = { 0x00, 0x00, 0x00, 0x00, 0x88 }; /* ptt = off */
  write_block(fd,data);
}

void cmd_set_sat_on(int fd) {
  static unsigned char data[] = { 0x00, 0x00, 0x00, 0x00, 0x4e }; /* sat = on */
  write_block(fd,data);
}

void cmd_set_sat_off(int fd) {
  static unsigned char data[] = { 0x00, 0x00, 0x00, 0x00, 0x8e }; /* sat = off */
  write_block(fd,data);
}

void cmd_set_freq_main_vfo(int fd, unsigned char d1,  unsigned char d2, 
			   unsigned char d3, unsigned char d4) {

  static unsigned char data[] = { 0x00, 0x00, 0x00, 0x00, 0x01 }; /* set freq, main vfo*/

  data[0] = d1;
  data[1] = d2;
  data[2] = d3;
  data[3] = d4;

  write_block(fd,data);

}

void cmd_set_freq_sat_rx_vfo(int fd, unsigned char d1,  unsigned char d2, 
			     unsigned char d3, unsigned char  d4) {

  static unsigned char data[] = { 0x00, 0x00, 0x00, 0x00, 0x11 }; /* set freq, sat rx vfo*/

  data[0] = d1;
  data[1] = d2;
  data[2] = d3;
  data[3] = d4;
  write_block(fd,data);
}

void cmd_set_freq_sat_tx_vfo(int fd, unsigned char d1,  unsigned char d2, 
			     unsigned char d3, unsigned char  d4) {

  static unsigned char data[] = { 0x00, 0x00, 0x00, 0x00, 0x21 }; /* set freq, sat tx vfo*/

  data[0] = d1;
  data[1] = d2;
  data[2] = d3;
  data[3] = d4;
  write_block(fd,data);
}

void cmd_set_opmode_main_vfo(int fd, unsigned char d1) {
  static unsigned char data[] = { 0x00, 0x00, 0x00, 0x00, 0x07 }; /* set opmode,  main vfo*/

  data[0] = d1;
  write_block(fd,data);
}

void cmd_set_opmode_sat_rx_vfo(int fd, unsigned char d1) {
  static unsigned char data[] = { 0x00, 0x00, 0x00, 0x00, 0x17 }; /* set opmode, sat rx vfo */

  data[0] = d1;
  write_block(fd,data);
}

void cmd_set_opmode_sat_tx_vfo(int fd, unsigned char d1) {
  static unsigned char data[] = { 0x00, 0x00, 0x00, 0x00, 0x27 }; /* set opmode, sat tx vfo */

  data[0] = d1;
  write_block(fd,data);
}

void cmd_set_ctcss_dcs_main_vfo(int fd, unsigned char d1) {
  static unsigned char data[] = { 0x00, 0x00, 0x00, 0x00, 0x0a }; /* set ctcss/dcs,  main vfo*/

  data[0] = d1;
  write_block(fd,data);
}

void cmd_set_ctcss_dcs_sat_rx_vfo(int fd, unsigned char d1) {
  static unsigned char data[] = { 0x00, 0x00, 0x00, 0x00, 0x1a }; /* set ctcss/dcs, sat rx vfo*/

  data[0] = d1;
  write_block(fd,data);
}

void cmd_set_ctcss_dcs_sat_tx_vfo(int fd, unsigned char d1) {
  static unsigned char data[] = { 0x00, 0x00, 0x00, 0x00, 0x2a }; /* set ctcss/dcs, sat tx vfo*/

  data[0] = d1;
  write_block(fd,data);
}

void cmd_set_ctcss_freq_main_vfo(int fd, unsigned char d1) {
  static unsigned char data[] = { 0x00, 0x00, 0x00, 0x00, 0x0b }; /* set ctcss freq,  main vfo*/

  data[0] = d1;
  write_block(fd,data);
}

void cmd_set_ctcss_freq_sat_rx_vfo(int fd, unsigned char d1) {
  static unsigned char data[] = { 0x00, 0x00, 0x00, 0x00, 0x1b }; /* set ctcss freq, sat rx vfo*/

  data[0] = d1;
  write_block(fd,data);
}

void cmd_set_ctcss_freq_sat_tx_vfo(int fd, unsigned char d1) {
  static unsigned char data[] = { 0x00, 0x00, 0x00, 0x00, 0x2b }; /* set ctcss freq, sat rx vfo*/

  data[0] = d1;
  write_block(fd,data);
}

void cmd_set_dcs_code_main_vfo(int fd, unsigned char d1, unsigned char d2) {
  static unsigned char data[] = { 0x00, 0x00, 0x00, 0x00, 0x0c }; /* set dcs code,  main vfo*/

  data[0] = d1;
  data[1] = d2;
  write_block(fd,data);
}

void cmd_set_dcs_code_sat_rx_vfo(int fd, unsigned char d1, unsigned char d2) {
  static unsigned char data[] = { 0x00, 0x00, 0x00, 0x00, 0x1c }; /* set dcs code, sat rx vfo*/

  data[0] = d1;
  data[1] = d2;
  write_block(fd,data);
}

void cmd_set_dcs_code_sat_tx_vfo(int fd, unsigned char d1, unsigned char d2) {
  static unsigned char data[] = { 0x00, 0x00, 0x00, 0x00, 0x2c }; /* set dcs code, sat tx vfo*/

  data[0] = d1;
  data[1] = d2;
  write_block(fd,data);
}

void cmd_set_repeater_shift_minus(int fd) {
  static unsigned char data[] = { 0x09, 0x00, 0x00, 0x00, 0x09 }; /* set repeater shift minus */

  write_block(fd,data);
}

void cmd_set_repeater_shift_plus(int fd) {
  static unsigned char data[] = { 0x49, 0x00, 0x00, 0x00, 0x09 }; /* set repeater shift  */

  write_block(fd,data);
}

void cmd_set_repeater_shift_simplex(int fd) {
  static unsigned char data[] = { 0x89, 0x00, 0x00, 0x00, 0x09 }; /* set repeater simplex  */

  write_block(fd,data);
}

void cmd_set_repeater_offset(int fd, unsigned char d1,  unsigned char d2, 
			     unsigned char d3, unsigned char  d4) {

  static unsigned char data[] = { 0x00, 0x00, 0x00, 0x00, 0xf9 }; /* set repeater offset  */

  data[0] = d1;
  data[1] = d2;
  data[2] = d3;
  data[3] = d4;
  write_block(fd,data);
}


/*
 * Get data rx from the RIG...only 1 byte
 *
 */


unsigned char cmd_get_rx_status(int fd) {
  int n;			/* counters */

  static unsigned char data[] = { 0x00, 0x00, 0x00, 0x00, 0xe7 }; /* get receiver status */

  write_block(fd,data);
  n = read_sleep(fd,datain,1);	/* wait and read for 1 byte to be read */

  printf("datain[0] = %x \n",datain[0]);

  return datain[0];

}

/*
 * Get data tx from the RIG...only 1 byte
 *
 */

unsigned char cmd_get_tx_status(int fd) {
  int n;			/* counters */

  static unsigned char data[] = { 0x00, 0x00, 0x00, 0x00, 0xf7 }; /* get tx status */

  write_block(fd,data);
  n = read_sleep(fd,datain,1);	/* wait and read for 1 byte to be read */

  printf("datain[0] = %x \n",datain[0]);

  return datain[0];

}




/*
 * Get freq and mode data from the RIG main VFO...only 5 bytes
 *
 */

long int cmd_get_freq_mode_status_main_vfo(int fd, unsigned char *mode) {
  long int f;
  
  static unsigned char data[] = { 0x00, 0x00, 0x00, 0x00, 0x03 }; /* get freq and mode status */
								  /* main vfo*/
  write_block(fd,data);
  read_sleep(fd,datain,5);	/* wait and read for 5 byte to be read */
  f = calc_freq_from_packed4(datain); /* 1st 4 bytes */
  *mode = datain[4];		      /* last byte */
  
  return f;			/* return freq in Hz */

}

/*
 * Get freq and mode data from the RIG  sat RX vfo ...only 5 bytes
 *
 */

long int cmd_get_freq_mode_status_sat_rx_vfo(int fd, unsigned char *mode ) {
  long int f;

  static unsigned char data[] = { 0x00, 0x00, 0x00, 0x00, 0x13 }; /* get freq and mode status */
								  /* sat rx vfo*/
  write_block(fd,data);
  read_sleep(fd,datain,5);	/* wait and read for 5 byte to be read */
  f = calc_freq_from_packed4(datain); /* 1st 4 bytes */
  *mode = datain[4];		      /* last byte */
  
  return f;			/* return freq in Hz */

}


/*
 * Get freq and mode data from the RIG sat TX VFO...only 5 bytes
 *
 */

long int cmd_get_freq_mode_status_sat_tx_vfo(int fd, unsigned char *mode) {
  long int f;

  static unsigned char data[] = { 0x00, 0x00, 0x00, 0x00, 0x23 }; /* get freq and mode status */
								  /* sat tx vfo*/
  write_block(fd,data);
  read_sleep(fd,datain,5);	/* wait and read for 5 byte to be read */
  f = calc_freq_from_packed4(datain); /* 1st 4 bytes */
  *mode = datain[4];		      /* last byte */

  return f;			/* return freq in Hz */

}



/*
 * Set main vfo frequency in Hz and mode.
 *
 */

void cmd_set_freq_main_vfo_hz(int fd,long int freq, unsigned char mode) {
  unsigned char d1,d2,d3,d4;
  unsigned char farray[4];	/* holds packed freq */

  calc_packed4_from_freq(freq,farray); /* and store in farray */

  d1 = farray[0];
  d2 = farray[1];
  d3 = farray[2];
  d4 = farray[3];

  cmd_set_opmode_main_vfo(fd,mode); /* set mode first, otherwise previous CW mode */
				    /* causes offset for next freq set command */

  cmd_set_freq_main_vfo(fd,d1,d2,d3,d4); /* set freq */

}


/*
 * Set sat rx vfo frequency in Hz and mode.
 *
 */

void cmd_set_freq_sat_rx_vfo_hz(int fd,long int freq, unsigned char mode) {
  unsigned char d1,d2,d3,d4;
  unsigned char farray[4];	/* holds packed freq */

  calc_packed4_from_freq(freq,farray); /* and store in farray */

  d1 = farray[0];
  d2 = farray[1];
  d3 = farray[2];
  d4 = farray[3];

  cmd_set_opmode_sat_rx_vfo(fd,mode); /* set mode first, otherwise previous CW mode */
				    /* causes offset for next freq set command */

  cmd_set_freq_sat_rx_vfo(fd,d1,d2,d3,d4); /* set freq */

}


/*
 * Set sat tx vfo frequency in Hz and mode.
 *
 */

void cmd_set_freq_sat_tx_vfo_hz(int fd,long int freq, unsigned char mode) {
  unsigned char d1,d2,d3,d4;
  unsigned char farray[4];	/* holds packed freq */

  calc_packed4_from_freq(freq,farray); /* and store in farray */

  d1 = farray[0];
  d2 = farray[1];
  d3 = farray[2];
  d4 = farray[3];

  cmd_set_opmode_sat_tx_vfo(fd,mode); /* set mode first, otherwise previous CW mode */
				    /* causes offset for next freq set command */

  cmd_set_freq_sat_tx_vfo(fd,d1,d2,d3,d4); /* set freq */

}

/*
 * Set Repeater offset in Hz.
 *
 */

void cmd_set_repeater_offset_hz(int fd,long int freq) {
  unsigned char d1,d2,d3,d4;
  unsigned char farray[4];	/* holds packed freq */

  /* can only specify multiples of 10Hz on FT847, but */
  /* calc_packed4_from_freq() does that for us , so dont */
  /* do it here.. */

  calc_packed4_from_freq(freq,farray); /* and store in farray */

  d1 = farray[0];
  d2 = farray[1];
  d3 = farray[2];
  d4 = farray[3];

  cmd_set_repeater_offset(fd,d1,d2,d3,d4); /* set freq */

}


/*
 * Private helper functions....
 *
 */


/*
 * Calculate freq from packed decimal (4 bytes, 8 digits) 
 * and return frequency in Hz. No string routines.
 *
 */

static long int  calc_freq_from_packed4(unsigned char *in) {
  long int f;			/* frequnecy in Hz */
  unsigned char d1,d2,d3,d4;

  printf("frequency/mode = %.2x%.2x%.2x%.2x %.2x \n",  in[0], in[1], in[2], in[3], in[4]); 

  d1 = calc_char_from_packed(in[0]);
  d2 = calc_char_from_packed(in[1]);
  d3 = calc_char_from_packed(in[2]);
  d4 = calc_char_from_packed(in[3]);

  f  = d1*1000000;
  f += d2*10000;
  f += d3*100;
  f += d4;

  f = f *10;			/* yaesu uses freq as multiple of 10 Hz, so must */
				/* scale to return Hz*/

  printf("ft847:frequency = %ld  Hz\n",  f); 
     
  return f;			/* Hz */
}



/*
 * Calculate  packed decimal from freq hz (4 bytes, 8 digits) 
 * and return packed decimal frequency in out[] array.
 *
 *  Note this routine also scales freq as multiple of
 *  10Hz as required for most freq parameters.
 *
 */

static void calc_packed4_from_freq(long int freq, unsigned char *out) {
  unsigned char d1,d2,d3,d4;
  long int f1,f2,f3,f4;

  freq = freq / 10;		/* yaesu ft847  only accepts 10Hz resolution */

  f1 = freq / 1000000;		                             /* get 100 Mhz/10 Mhz part */
  f2 = (freq - (f1 * 1000000)) / 10000;	                     /* get 1Mhz/100Khz part */
  f3 = (freq - (f1 * 1000000) - (f2 * 10000)) / 100;         /* get 10khz/1khz part */
  f4 = (freq - (f1 * 1000000) - (f2 * 10000) - (f3 * 100));  /* get 10khz/1khz part */

  d1 = calc_packed_from_char(f1);
  d2 = calc_packed_from_char(f2);
  d3 = calc_packed_from_char(f3);
  d4 = calc_packed_from_char(f4);

  out[0] = d1;			/* get 100 Mhz-10 Mhz part */	
  out[1] = d2;			/* get 1Mhz-100Khz part */
  out[2] = d3;			/* get 10khz-1khz part */
  out[3] = d4;			/* get 10khz-1khz part */
 
}





/*
 * To see if a value is present in an array of ints.
 *
 */

static int is_in_list(int *list, int list_length, int value) {
  int i;
  
  for(i=0; i<list_length; i++){
    if (*(list+i) == value)
      return TRUE;		/* found */
  }
  return FALSE;			/* not found */
}


#endif /* 0 */


/*
 * init_ft847 is called by rig_backend_load
 */
int init_ft847(void *be_handle)
{
		rig_debug(RIG_DEBUG_VERBOSE, "ft847: _init called\n");

		rig_register(&ft847_caps);

		return RIG_OK;
}

