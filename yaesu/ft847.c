/*
 * hamlib - (C) Frank Singleton 2000,2001 (vk3fcs@ix.netcom.com)
 *
 * ft847.c - (C) Frank Singleton 2000 (vk3fcs@ix.netcom.com)
 * This shared library provides an API for communicating
 * via serial interface to an FT-847 using the "CAT" interface.
 *
 *
 * $Id: ft847.c,v 1.19 2001-12-20 22:59:09 fillods Exp $  
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
 * TODO - Remove static stuff, see ft747 for new style [started]
 *      - create yaesu.h for common command structure etc..[started]
 *      - add mode set before freq set to avoid prior mode offset (eg: CW)
 *
 */

/*
 * Notes on limitations in RIG control capabilities. These are
 * related to the Yaesu's FT847 design, not my program :-)
 *
 * 1. Rig opcodes allow only 10Hz resolution.
 * 2. Cannot select VFO B
 * 3. Using CAT and Tuner controls simultaneously  can
 *    cause problems.
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
#include "yaesu.h"
#include "ft847.h"
#include "misc.h"

/* prototypes */

static int ft847_send_priv_cmd(RIG *rig, unsigned char ci);


/* Native ft847 cmd set prototypes. These are READ ONLY as each */
/* rig instance will copy from these and modify if required . */
/* Complete sequences (1) can be read and used directly as a cmd sequence . */
/* Incomplete sequences (0) must be completed with extra parameters */
/* eg: mem number, or freq etc.. */

static const yaesu_cmd_set_t ncmd[] = { 
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


#define FT847_ALL_RX_MODES (RIG_MODE_AM|RIG_MODE_CW|RIG_MODE_SSB|RIG_MODE_FM)
#define FT847_SSB_CW_RX_MODES (RIG_MODE_CW|RIG_MODE_SSB)
#define FT847_AM_FM_RX_MODES (RIG_MODE_AM|RIG_MODE_FM)

/* tx doesn't have WFM.
 * 100W in 160-6m (25 watts AM carrier)
 * 50W in 2m/70cm (12.5 watts AM carrier)
 */ 

#define FT847_OTHER_TX_MODES (RIG_MODE_AM|RIG_MODE_CW|RIG_MODE_SSB|RIG_MODE_FM)
#define FT847_AM_TX_MODES (RIG_MODE_AM)

#define FT847_FUNC_ALL (RIG_FUNC_FAGC|RIG_FUNC_NB|RIG_FUNC_COMP|RIG_FUNC_VOX|RIG_FUNC_TONE|RIG_FUNC_TSQL|RIG_FUNC_SBKIN|RIG_FUNC_FBKIN)	/* fix */

/*
 * ft847 rigs capabilities.
 * Notice that some rigs share the same functions.
 * Also this struct is READONLY!
 */


const struct rig_caps ft847_caps = {
rig_model: RIG_MODEL_FT847,
model_name:"FT-847", 
mfg_name: "Yaesu", 
version: "0.1", 
copyright: "LGPL",
status: RIG_STATUS_ALPHA,
rig_type: RIG_TYPE_TRANSCEIVER,
ptt_type: RIG_PTT_RIG,
dcd_type: RIG_DCD_NONE,
port_type: RIG_PORT_SERIAL,
serial_rate_min: 4800,
serial_rate_max: 57600,
serial_data_bits: 8,
serial_stop_bits: 2,
serial_parity: RIG_PARITY_NONE,
serial_handshake: RIG_HANDSHAKE_NONE, 
write_delay: FT847_WRITE_DELAY,
post_write_delay: FT847_POST_WRITE_DELAY,
timeout: 100,
retry: 0, 

has_get_func: RIG_FUNC_NONE,
has_set_func: FT847_FUNC_ALL, 
has_get_level: RIG_LEVEL_NONE,	/* FIXME! */
has_set_level: RIG_LEVEL_NONE,
has_get_parm: RIG_PARM_NONE,
has_set_parm: RIG_PARM_NONE,	/* FIXME: parms */
level_gran: {}, 		/* granularity */
parm_gran: {},
ctcss_list: NULL,	/* FIXME: CTCSS/DCS list */
dcs_list: NULL,
preamp:  { RIG_DBLST_END, },	/* FIXME! */
attenuator:  { RIG_DBLST_END, },
max_rit: Hz(9999),
max_xit: Hz(0),
max_ifshift: Hz(0),
targetable_vfo: RIG_TARGETABLE_FREQ,
transceive: RIG_TRN_OFF,
bank_qty:  0,
chan_desc_sz: 0,

chan_list: { RIG_CHAN_END, },	/* FIXME: memory chan list: 78 */

rx_range_list1: { RIG_FRNG_END, },    /* FIXME: enter region 1 setting */
tx_range_list1: { RIG_FRNG_END, },
rx_range_list2: 
  { {kHz(100),MHz(30),FT847_ALL_RX_MODES,-1,-1}, /* rx range begin */
    {MHz(36),MHz(76),FT847_ALL_RX_MODES,-1,-1},
    {MHz(108),MHz(174),FT847_ALL_RX_MODES,-1,-1},
    {MHz(420),MHz(512),FT847_ALL_RX_MODES,-1,-1},

    RIG_FRNG_END, }, /* rx range end */

tx_range_list2:
  { {MHz(1.8),1999999,FT847_OTHER_TX_MODES,W(5),W(100)},	/* 5-100W class */
    {MHz(1.8),1999999,FT847_AM_TX_MODES,W(1),W(25)},	/* 1-25W class */

    {MHz(3.5),3999999,FT847_OTHER_TX_MODES,W(5),W(100)},
    {MHz(3.5),3999999,FT847_AM_TX_MODES,W(1),W(25)},

    {MHz(7),MHz(7.3),FT847_OTHER_TX_MODES,W(5),W(100)},
    {MHz(7),MHz(7.3),FT847_AM_TX_MODES,W(1),W(25)},

    {MHz(10),MHz(10.150),FT847_OTHER_TX_MODES,W(5),W(100)},
    {MHz(10),MHz(10.150),FT847_AM_TX_MODES,W(1),W(25)},

    {MHz(14),MHz(14.350),FT847_OTHER_TX_MODES,W(5),W(100)},
    {MHz(14),MHz(14.350),FT847_AM_TX_MODES,W(1),W(25)},

    {MHz(18.068),MHz(18.168),FT847_OTHER_TX_MODES,W(5),W(100)},
    {MHz(18.068),MHz(18.168),FT847_AM_TX_MODES,W(1),W(25)},

    {MHz(21),MHz(21.450),FT847_OTHER_TX_MODES,W(5),W(100)},
    {MHz(21),MHz(21.450),FT847_AM_TX_MODES,W(1),W(25)},

    {MHz(24.890),MHz(24.990),FT847_OTHER_TX_MODES,W(5),W(100)},
    {MHz(24.890),MHz(24.990),FT847_AM_TX_MODES,W(1),W(25)},

    {MHz(28),MHz(29.7),FT847_OTHER_TX_MODES,W(5),W(100)},
    {MHz(28),MHz(29.7),FT847_AM_TX_MODES,W(1),W(25)},

    {MHz(50),MHz(54),FT847_OTHER_TX_MODES,W(5),W(100)},
    {MHz(50),MHz(54),FT847_AM_TX_MODES,W(1),W(25)},

    {MHz(144),MHz(148),FT847_OTHER_TX_MODES,W(1),W(50)}, 
    {MHz(144),MHz(148),FT847_AM_TX_MODES,W(1),W(12.5)}, 

    {MHz(430),MHz(440),FT847_OTHER_TX_MODES,W(1),W(50)}, /* check range */
    {MHz(430),MHz(440),FT847_AM_TX_MODES,W(1),W(12.5)},

    RIG_FRNG_END, },

tuning_steps: { {FT847_SSB_CW_RX_MODES,1}, /* normal */
    {FT847_SSB_CW_RX_MODES,10}, /* fast */
    {FT847_SSB_CW_RX_MODES,100}, /* faster */

    
    {FT847_AM_FM_RX_MODES,10}, /* normal */
    {FT847_AM_FM_RX_MODES,100}, /* fast  */
        
    RIG_TS_END,
  },  
      /* mode/filter list, remember: order matters! */
filters: {
		{RIG_MODE_SSB|RIG_MODE_CW, kHz(2.2)},
		{RIG_MODE_AM, kHz(9)},
		{RIG_MODE_AM, kHz(2.2)},
		{RIG_MODE_FM, kHz(15)},
		{RIG_MODE_FM, kHz(9)},
		RIG_FLT_END,
  },

priv:  NULL,
rig_init:  ft847_init, 
rig_cleanup: ft847_cleanup, 
rig_open:  ft847_open, 
rig_close: ft847_close, 

set_freq:  ft847_set_freq,		/* set freq */
get_freq: ft847_get_freq,		/* get freq */
set_mode: ft847_set_mode,		/* set mode */
get_mode: ft847_get_mode,		/* get mode */
set_vfo: ft847_set_vfo,		/* set vfo */
get_vfo: ft847_get_vfo,		/* get vfo */
set_ptt: ft847_set_ptt,		/* set ptt */
get_ptt: ft847_get_ptt,		/* get ptt */

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
   * Copy complete native cmd set to private cmd storage area 
   */

  memcpy(p->pcs,ncmd,sizeof(ncmd));

  p->current_vfo =  RIG_VFO_MAIN;	/* default to VFO_MAIN */
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

  rig_debug(RIG_DEBUG_VERBOSE,"ft847:ft847_cleanup called \n");
  
  return RIG_OK;
}


/*
 * ft847_open  routine
 * 
 */

int ft847_open(RIG *rig) {

  /* Good time to set CAT ON */

  rig_debug(RIG_DEBUG_VERBOSE,"ft847:ft847_open called \n");

  ft847_send_priv_cmd(rig,FT_847_NATIVE_CAT_ON);

  return RIG_OK;
}

/*
 * ft847_close  routine
 * 
 */

int ft847_close(RIG *rig) {

  /* Good time to set CAT OFF */

  rig_debug(RIG_DEBUG_VERBOSE,"ft847:ft847_close called \n");

  ft847_send_priv_cmd(rig,FT_847_NATIVE_CAT_OFF);

  return RIG_OK;
}

/*
 * private helper function to send a private command
 * sequence . Must only be complete sequences.
 *
 */

static int ft847_send_priv_cmd(RIG *rig, unsigned char ci) {

  struct rig_state *rig_s;
  struct ft847_priv_data *p;
  unsigned char *cmd;		/* points to sequence to send */
  unsigned char cmd_index;	/* index of sequence to send */
 
  if (!rig)
    return -RIG_EINVAL;


  p = (struct ft847_priv_data*)rig->state.priv;
  rig_s = &rig->state;
  
  cmd_index = ci;		/* get command */

  if (! p->pcs[cmd_index].ncomp) {
    rig_debug(RIG_DEBUG_VERBOSE,"ft847: Attempt to send incomplete sequence \n");
    return -RIG_EINVAL;
  }

  cmd = (unsigned char *) p->pcs[cmd_index].nseq; /* get native sequence */
  write_block(&rig_s->rigport, cmd, YAESU_CMD_LENGTH);
  
  return RIG_OK;

}



/*
 * Set frequency to freq Hz. Note 10 Hz resolution -- YUK -- FS
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

  rig_debug(RIG_DEBUG_VERBOSE,"ft847: requested freq = %lli Hz \n", freq);


  /* 
   * Copy native cmd freq_set to private cmd storage area 
   */


  rig_debug(RIG_DEBUG_VERBOSE,"ft847: vfo =%i \n", vfo);
  
  switch(vfo) {
  case RIG_VFO_MAIN:
    cmd_index = FT_847_NATIVE_CAT_SET_FREQ_MAIN;
    break;
  case RIG_VFO_CURR:
    switch(p->current_vfo) {	/* what is my active VFO ? */
    case RIG_VFO_MAIN:
      cmd_index = FT_847_NATIVE_CAT_SET_FREQ_MAIN;
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

  memcpy(p->p_cmd,&ncmd[cmd_index].nseq,YAESU_CMD_LENGTH);  

  to_bcd_be(p->p_cmd,freq/10,8);	/* store bcd format in in p_cmd */
				/* TODO -- fix 10Hz resolution -- FS */

  rig_debug(RIG_DEBUG_VERBOSE,"ft847: requested freq after conversion = %lli Hz \n", from_bcd_be(p->p_cmd,8)* 10 );

  cmd = p->p_cmd; /* get native sequence */
  write_block(&rig_s->rigport, cmd, YAESU_CMD_LENGTH);

  return RIG_OK;
}

#define MD_LSB  0x00
#define MD_USB  0x01
#define MD_CW   0x02
#define MD_CWR  0x03
#define MD_AM   0x04
#define MD_FM   0x08
#define MD_CWN  0x82
#define MD_CWNR 0x83
#define MD_AMN  0x84
#define MD_FMN  0x88

static int get_freq_and_mode(RIG *rig, vfo_t vfo, freq_t *freq, rmode_t *mode,
				pbwidth_t *width) {
  struct rig_state *rs = &rig->state;
  struct ft847_priv_data *p;
  unsigned char *cmd;		/* points to sequence to send */
  unsigned char cmd_index;	/* index of sequence to send */
  unsigned char data[8];
  int n;

  p = (struct ft847_priv_data*)rs->priv;

  rig_debug(RIG_DEBUG_VERBOSE,"ft847: vfo =%i \n", vfo);
  
  if (vfo == RIG_VFO_MAIN)
		  vfo = p->current_vfo;

  /*
   * TODO:
   *	FT_847_NATIVE_CAT_GET_FREQ_MODE_STATUS_SAT_RX
   *	FT_847_NATIVE_CAT_GET_FREQ_MODE_STATUS_SAT_TX
   */
  switch(vfo) {
  case RIG_VFO_MAIN:
    cmd_index = FT_847_NATIVE_CAT_GET_FREQ_MODE_STATUS_MAIN;
    break;

  default:
    rig_debug(RIG_DEBUG_VERBOSE,"ft847: Unknown  VFO \n");
    return -RIG_EINVAL;		/* sorry, wrong VFO */
  }

  memcpy(p->p_cmd,&ncmd[cmd_index].nseq,YAESU_CMD_LENGTH);  

  cmd = p->p_cmd;
  write_block(&rs->rigport, cmd, YAESU_CMD_LENGTH);

  n = read_block(&rs->rigport, data, YAESU_CMD_LENGTH);
  if (n != YAESU_CMD_LENGTH) {
  		rig_debug(RIG_DEBUG_ERR,"ft847: read_block returned %d\n", n);
		return n < 0 ? n : -RIG_EPROTO;
  }

	/* Remember, this is 10Hz resolution */
  *freq = 10*from_bcd_be(data, 8);

  *width = RIG_PASSBAND_NORMAL;
  switch (data[4]) {
  case MD_LSB: *mode = RIG_MODE_LSB; break;
  case MD_USB: *mode = RIG_MODE_USB; break;

  case MD_CWN: 
			   *width = rig_passband_narrow(rig, RIG_MODE_CW);
  case MD_CW:  
			   *mode = RIG_MODE_CW;
			   break;

#ifdef RIG_MODE_CWR
  case MD_CWNR:
			   *width = rig_passband_narrow(rig, RIG_MODE_CW);
  case MD_CWR: 
				*mode = RIG_MODE_CWR;
				break;
#endif

  case MD_AMN:
			   *width = rig_passband_narrow(rig, RIG_MODE_AM);
  case MD_AM:
			   *mode = RIG_MODE_AM;
			   break;

  case MD_FMN:
			   *width = rig_passband_narrow(rig, RIG_MODE_FM);
  case MD_FM:  
			   *mode = RIG_MODE_FM; 
			   break;
  default:
    *mode = RIG_MODE_NONE;
    rig_debug(RIG_DEBUG_VERBOSE,"ft847: Unknown mode %02x\n", data[4]);
  }
  if (*width == RIG_PASSBAND_NORMAL)
		  *width = rig_passband_normal(rig, *mode);

  return RIG_OK;
}

/*
 * Note taken from http://my.en.com/~rayd/ft-847/FAQ/page4.htm#pollingcodes
 * The FT-847, as originally delivered, could not poll the radio for frequency
 * and mode information. This was added beginning with the 8G05 production
 * runs. The Operating Manual does not show the codes for polling the radio.
 * Note that you cannot query the sub-VFO, nor can you swap VFOs via software.
 */
int ft847_get_freq(RIG *rig, vfo_t vfo, freq_t *freq)
{
  rmode_t mode;
  pbwidth_t width;

  return get_freq_and_mode(rig, vfo, freq, &mode, &width);
}




/*
 * TODO -- add other VFO's
 *
 */

int ft847_set_mode(RIG *rig, vfo_t vfo, rmode_t mode, pbwidth_t width) {
  unsigned char cmd_index;	/* index of sequence to send */
  
  /* 
   * translate mode from generic to ft847 specific
   */

  rig_debug(RIG_DEBUG_VERBOSE,"ft847: generic mode = %x \n", mode);

  switch(mode) {
  case RIG_MODE_AM:
    cmd_index = FT_847_NATIVE_CAT_SET_MODE_MAIN_AM;
    break;
  case RIG_MODE_CW:
    cmd_index = FT_847_NATIVE_CAT_SET_MODE_MAIN_CW;
    break;
  case RIG_MODE_USB:
    cmd_index = FT_847_NATIVE_CAT_SET_MODE_MAIN_USB;
    break;
  case RIG_MODE_LSB:
    cmd_index = FT_847_NATIVE_CAT_SET_MODE_MAIN_LSB;
    break;
  case RIG_MODE_FM:
    cmd_index = FT_847_NATIVE_CAT_SET_MODE_MAIN_FM;
    break;
  default:
    return -RIG_EINVAL;		/* sorry, wrong MODE */
  }


  /*
   * Now set width
   */

  if (width == rig_passband_narrow(rig, mode)) {
    switch(mode) {
    case RIG_MODE_AM:
      cmd_index = FT_847_NATIVE_CAT_SET_MODE_MAIN_AMN;
      break;
    case RIG_MODE_FM:
      cmd_index = FT_847_NATIVE_CAT_SET_MODE_MAIN_FMN;
      break;
    case RIG_MODE_CW:
      cmd_index = FT_847_NATIVE_CAT_SET_MODE_MAIN_CWN;
      break;
    default:
      return -RIG_EINVAL;		/* sorry, wrong MODE/WIDTH combo  */    
    }
  } else {
  		if (width != RIG_PASSBAND_NORMAL && 
						width != rig_passband_normal(rig, mode)) {
      		return -RIG_EINVAL;		/* sorry, wrong MODE/WIDTH combo  */    
		}
	}
  /*
   * Now send the command
   */

  ft847_send_priv_cmd(rig,cmd_index);

  return RIG_OK;
}

int ft847_get_mode(RIG *rig, vfo_t vfo, rmode_t *mode, pbwidth_t *width) {
  freq_t freq;

  return get_freq_and_mode(rig, vfo, &freq, mode, width);
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
 * Also, RIG can handle only VFO_MAIN . Add SAT VFO's later.
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
  case RIG_VFO_MAIN:
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
  unsigned char cmd_index;	/* index of sequence to send */

  rig_debug(RIG_DEBUG_VERBOSE,"ft847:ft847_set_ptt called \n");

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

  ft847_send_priv_cmd(rig,cmd_index);

  return RIG_OK;		/* good */

}

int ft847_get_ptt(RIG *rig, vfo_t vfo, ptt_t *ptt) {
  return -RIG_ENIMPL;

}



/*
 * init_ft847 is called by rig_backend_load
 */

int init_ft847(void *be_handle) {
  rig_debug(RIG_DEBUG_VERBOSE, "ft847: _init called\n");

  rig_register(&ft847_caps);
  
  return RIG_OK;
}



