/*
 * hamlib - (C) Frank Singleton 2000,2001 (vk3fcs@ix.netcom.com)
 *
 * ft817.c - (C) Chris Karpinsky 2001 (aa1vl@arrl.net)
 * This shared library provides an API for communicating
 * via serial interface to an FT-817 using the "CAT" interface.
 * The starting point for this code was Frank's ft847 implementation.
 *
 *
 *    $Id: ft817.c,v 1.11 2005-04-03 19:27:59 fillods Exp $  
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
#include <string.h>  	/* String function definitions */
#include <unistd.h>  	/* UNIX standard function definitions */

#include "hamlib/rig.h"
#include "serial.h"
#include "yaesu.h"
#include "ft817.h"
#include "misc.h"

/* prototypes */

static int ft817_send_priv_cmd(RIG *rig, unsigned char ci);


/* Native ft817 cmd set prototypes. These are READ ONLY as each */
/* rig instance will copy from these and modify if required . */
/* Complete sequences (1) can be read and used directly as a cmd sequence . */
/* Incomplete sequences (0) must be completed with extra parameters */
/* eg: mem number, or freq etc.. */

static const yaesu_cmd_set_t ncmd[] = {
  { 1, { 0x00, 0x00, 0x00, 0x00, 0x00 } }, /* lock on */
  { 1, { 0x00, 0x00, 0x00, 0x00, 0x80 } }, /* lock off */
  { 1, { 0x00, 0x00, 0x00, 0x00, 0x08 } }, /* ptt on */
  { 1, { 0x00, 0x00, 0x00, 0x01, 0x88 } }, /* ptt off */
  { 0, { 0x00, 0x00, 0x00, 0x00, 0x01 } }, /* set freq */
  { 1, { 0x00, 0x00, 0x00, 0x00, 0x07 } }, /* mode set main LSB */
  { 1, { 0x01, 0x00, 0x00, 0x00, 0x07 } }, /* mode set main USB */
  { 1, { 0x02, 0x00, 0x00, 0x00, 0x07 } }, /* mode set main CW */
  { 1, { 0x03, 0x00, 0x00, 0x00, 0x07 } }, /* mode set main CWR */
  { 1, { 0x04, 0x00, 0x00, 0x00, 0x07 } }, /* mode set main AM */
  { 1, { 0x08, 0x00, 0x00, 0x00, 0x07 } }, /* mode set main FM */
  { 1, { 0x0a, 0x00, 0x00, 0x00, 0x07 } }, /* mode set main DIG */
  { 1, { 0x0c, 0x00, 0x00, 0x00, 0x07 } }, /* mode set main PKT */
  { 1, { 0x00, 0x00, 0x00, 0x00, 0x05 } }, /* clar on */
  { 1, { 0x00, 0x00, 0x00, 0x00, 0x85 } }, /* clar off */
  { 0, { 0x00, 0x00, 0x00, 0x00, 0xf5 } }, /* set clar freq */
  { 1, { 0x00, 0x00, 0x00, 0x00, 0x81 } }, /* toggle vfo a/b */
  { 1, { 0x00, 0x00, 0x00, 0x00, 0x02 } }, /* split on */
  { 1, { 0x00, 0x00, 0x00, 0x00, 0x82 } }, /* split off */
  { 1, { 0x00, 0x00, 0x00, 0x00, 0x09 } }, /* set RPT shift MINUS */
  { 1, { 0x00, 0x00, 0x00, 0x00, 0x49 } }, /* set RPT shift PLUS */
  { 1, { 0x00, 0x00, 0x00, 0x00, 0x89 } }, /* set RPT shift SIMPLEX */
  { 0, { 0x00, 0x00, 0x00, 0x00, 0xf9 } }, /* set RPT offset freq */
/* fix me */
  { 1, { 0x0a, 0x00, 0x00, 0x00, 0x0a } }, /* set DCS on */
  { 1, { 0x2a, 0x00, 0x00, 0x00, 0x0a } }, /* set CTCSS/DCS enc/dec on */
  { 1, { 0x4a, 0x00, 0x00, 0x00, 0x0a } }, /* set CTCSS/DCS enc on */
  { 1, { 0x8a, 0x00, 0x00, 0x00, 0x0a } }, /* set CTCSS/DCS off */
/* em xif */
  { 0, { 0x00, 0x00, 0x00, 0x00, 0x0b } }, /* set CTCSS tone */
  { 0, { 0x00, 0x00, 0x00, 0x00, 0x0c } }, /* set DCS code */
  { 1, { 0x00, 0x00, 0x00, 0x00, 0xe7 } }, /* get RX status  */
  { 1, { 0x00, 0x00, 0x00, 0x00, 0xf7 } }, /* get TX status  */
  { 1, { 0x00, 0x00, 0x00, 0x00, 0x03 } }, /* get FREQ and MODE status */
  { 1, { 0x00, 0x00, 0x00, 0x00, 0x00 } }, /* pwr wakeup sequence */
  { 1, { 0x00, 0x00, 0x00, 0x00, 0x0f } }, /* pwr on */
  { 1, { 0x00, 0x00, 0x00, 0x00, 0x8f } }  /* pwr off */
};


#define FT817_ALL_RX_MODES (RIG_MODE_AM|RIG_MODE_CW|RIG_MODE_USB|RIG_MODE_LSB|RIG_MODE_RTTY|RIG_MODE_FM)
#define FT817_SSB_CW_RX_MODES (RIG_MODE_CW|RIG_MODE_USB|RIG_MODE_LSB)
#define FT817_AM_FM_RX_MODES (RIG_MODE_AM|RIG_MODE_FM)

#define FT817_OTHER_TX_MODES (RIG_MODE_AM|RIG_MODE_CW|RIG_MODE_USB|RIG_MODE_LSB|RIG_MODE_RTTY|RIG_MODE_FM)
#define FT817_AM_TX_MODES (RIG_MODE_AM)
#define FT817_GET_RIG_LEVELS (RIG_LEVEL_STRENGTH|RIG_LEVEL_RFPOWER)
#define FT817_FUNC_ALL (RIG_FUNC_LOCK|RIG_FUNC_TONE|RIG_FUNC_TSQL)

const struct rig_caps ft817_caps = {
  .rig_model = 		RIG_MODEL_FT817,
  .model_name = 		"FT-817", 
  .mfg_name = 		"Yaesu", 
  .version = 		"0.1", 
  .copyright = 		"LGPL",
  .status = 		RIG_STATUS_ALPHA,
  .rig_type = 		RIG_TYPE_TRANSCEIVER,
  .ptt_type = 		RIG_PTT_RIG,
  .dcd_type = 		RIG_DCD_NONE,
  .port_type = 		RIG_PORT_SERIAL,
  .serial_rate_min = 	4800,
  .serial_rate_max = 	38400,
  .serial_data_bits = 	8,
  .serial_stop_bits = 	2,
  .serial_parity = 	RIG_PARITY_NONE,
  .serial_handshake = 	RIG_HANDSHAKE_NONE, 
  .write_delay = 		FT817_WRITE_DELAY,
  .post_write_delay = 	FT817_POST_WRITE_DELAY,
  .timeout = 		100,
  .retry = 		0, 
  .has_get_func = 		RIG_FUNC_NONE,
  .has_set_func = 		FT817_FUNC_ALL, 
  .has_get_level = 	FT817_GET_RIG_LEVELS,
  .has_set_level = 	RIG_LEVEL_NONE,
  .has_get_parm = 		RIG_PARM_NONE,
  .has_set_parm = 		RIG_PARM_NONE,	/* FIXME: parms */
  .level_gran = 		{}, 		/* granularity */
  .parm_gran = 		{},
  .ctcss_list = 		NULL,	/* FIXME: CTCSS/DCS list */
  .dcs_list = 		NULL,
  .preamp = 		{ RIG_DBLST_END, },	/* FIXME! */
  .attenuator = 		{ RIG_DBLST_END, },
  .max_rit = 		Hz(9999),
  .max_xit = 		Hz(0),
  .max_ifshift = 		Hz(0),
  .targetable_vfo = 	0,
  .transceive = 		RIG_TRN_OFF,
  .bank_qty = 		0,
  .chan_desc_sz = 		0,

  .chan_list =  { RIG_CHAN_END, },	/* FIXME: memory chan list: 78 */

  .rx_range_list1 =  { RIG_FRNG_END, },    /* FIXME: enter region 1 setting */
  .tx_range_list1 =  { RIG_FRNG_END, },
  .rx_range_list2 =  { 
    {kHz(100),MHz(56), FT817_ALL_RX_MODES,-1,-1},
    {MHz(76), MHz(108),RIG_MODE_WFM,      -1,-1},
    {MHz(108),MHz(154),FT817_ALL_RX_MODES,-1,-1},
    {MHz(420),MHz(470),FT817_ALL_RX_MODES,-1,-1},
    RIG_FRNG_END, 
  },

  .tx_range_list2 =  {
    {MHz(1.8),   MHz(2),      FT817_OTHER_TX_MODES, W(0.5),W(0.5)},
    {MHz(1.8),   MHz(2),      FT817_AM_TX_MODES,    W(0.5),W(1.5)},
    {MHz(3.5),   MHz(4),      FT817_OTHER_TX_MODES, W(0.5),W(5.0)},
    {MHz(3.5),   MHz(4),      FT817_AM_TX_MODES,    W(0.5),W(1.5)},
    {MHz(7),     MHz(7.3),    FT817_OTHER_TX_MODES, W(0.5),W(5.0)},
    {MHz(7),     MHz(7.3),    FT817_AM_TX_MODES,    W(0.5),W(1.5)},
    {MHz(10),    MHz(10.150), FT817_OTHER_TX_MODES, W(0.5),W(5.0)},
    {MHz(10),    MHz(10.150), FT817_AM_TX_MODES,    W(0.5),W(1.5)},
    {MHz(14),    MHz(14.350), FT817_OTHER_TX_MODES, W(0.5),W(5.0)},
    {MHz(14),    MHz(14.350), FT817_AM_TX_MODES,    W(0.5),W(1.5)},
    {MHz(18.068),MHz(18.168), FT817_OTHER_TX_MODES, W(0.5),W(5.0)},
    {MHz(18.068),MHz(18.168), FT817_AM_TX_MODES,    W(0.5),W(1.5)},
    {MHz(21),    MHz(21.450), FT817_OTHER_TX_MODES, W(0.5),W(5.0)},
    {MHz(21),    MHz(21.450), FT817_AM_TX_MODES,    W(0.5),W(1.5)},
    {MHz(24.890),MHz(24.990), FT817_OTHER_TX_MODES, W(0.5),W(5.0)},
    {MHz(24.890),MHz(24.990), FT817_AM_TX_MODES,    W(0.5),W(1.5)},
    {MHz(28),    MHz(29.7),   FT817_OTHER_TX_MODES, W(0.5),W(5.0)},
    {MHz(28),    MHz(29.7),   FT817_AM_TX_MODES,    W(0.5),W(1.5)},
    {MHz(50),    MHz(54),     FT817_OTHER_TX_MODES, W(0.5),W(5.0)},
    {MHz(50),    MHz(54),     FT817_AM_TX_MODES,    W(0.5),W(1.5)},
    {MHz(144),   MHz(148),    FT817_OTHER_TX_MODES, W(0.5),W(5.0)}, 
    {MHz(144),   MHz(148),    FT817_AM_TX_MODES,    W(0.5),W(1.5)}, 
    {MHz(430),   MHz(440),    FT817_OTHER_TX_MODES, W(0.5),W(5.0)},
    {MHz(430),   MHz(440),    FT817_AM_TX_MODES,    W(0.5),W(1.5)},
    RIG_FRNG_END, 
  },

  .tuning_steps =  {
    {FT817_SSB_CW_RX_MODES,10},
    {FT817_SSB_CW_RX_MODES,100},
    {FT817_AM_FM_RX_MODES,10},
    {FT817_AM_FM_RX_MODES,100},
    RIG_TS_END,
  },  

  .filters =  {
    RIG_FLT_END,
  },

  .priv = 			NULL,
  .rig_init = 		ft817_init, 
  .rig_cleanup = 		ft817_cleanup, 
  .rig_open = 		ft817_open, 
  .rig_close = 		ft817_close, 
  .set_freq = 		ft817_set_freq,
  .get_freq = 		ft817_get_freq,
  .set_mode = 		ft817_set_mode,
  .get_mode = 		ft817_get_mode,
  .set_vfo = 		ft817_set_vfo,
  .get_vfo = 		ft817_get_vfo,
  .set_ptt = 		ft817_set_ptt,
  .get_ptt = 		NULL,
  .get_dcd = 		NULL,
  .set_rptr_shift = 	NULL,
  .get_rptr_shift = 	NULL,
  .set_rptr_offs = 	NULL,
  .get_rptr_offs = 	NULL,
  .set_split_freq = 	NULL,
  .get_split_freq = 	NULL,
  .set_split_mode = 	NULL,
  .get_split_mode = 	NULL,
  .set_split_vfo = 	NULL,
  .get_split_vfo =	NULL,
  .set_rit = 		NULL,
  .get_rit = 		NULL,
  .set_xit = 		NULL,
  .get_xit = 		NULL,
  .set_ts = 		NULL,
  .get_ts = 		NULL,
  .set_dcs_code = 		NULL,
  .get_dcs_code = 		NULL,
  .set_ctcss_tone = 	NULL,
  .get_ctcss_tone = 	NULL,
  .set_dcs_sql = 		NULL,
  .get_dcs_sql = 		NULL,
  .set_ctcss_sql = 	NULL,
  .get_ctcss_sql = 	NULL,
  .set_powerstat = 	NULL,
  .get_powerstat = 	NULL,
  .reset = 		NULL,
  .set_ant = 		NULL,
  .get_ant = 		NULL,
  .set_level = 		NULL,
  .get_level = 		NULL,
  .set_func = 		NULL,
  .get_func = 		NULL,
  .set_parm = 		NULL,
  .get_parm = 		NULL,
}; 


int ft817_init(RIG *rig) {
  struct ft817_priv_data *p;
  
  if (!rig)  return -RIG_EINVAL;
  
  p = (struct ft817_priv_data*)malloc(sizeof(struct ft817_priv_data));
  if (!p)  return -RIG_ENOMEM;

  rig_debug(RIG_DEBUG_VERBOSE,"ft817:ft817_init called \n");

  /* 
   * Copy complete native cmd set to private cmd storage area 
   */

  memcpy(p->pcs,ncmd,sizeof(ncmd));

  p->current_vfo = RIG_VFO_A;	/* no clue which VFO is active, so guess VFO 1 */
  rig->state.priv = (void*)p;
  
  return RIG_OK;
}

int ft817_cleanup(RIG *rig) {
  if (!rig)
    return -RIG_EINVAL;
  
  if (rig->state.priv)
    free(rig->state.priv);
  rig->state.priv = NULL;

  rig_debug(RIG_DEBUG_VERBOSE,"ft817:ft817_cleanup called \n");
  
  return RIG_OK;
}

int ft817_open(RIG *rig) {

  rig_debug(RIG_DEBUG_VERBOSE,"ft817:ft817_open called \n");

  return RIG_OK;
}

int ft817_close(RIG *rig) {

  rig_debug(RIG_DEBUG_VERBOSE,"ft817:ft817_close called \n");

  return RIG_OK;
}

/*
 * private helper function to send a private command
 * sequence . Must only be complete sequences.
 *
 */

static int ft817_send_priv_cmd(RIG *rig, unsigned char cmd_index) {

  struct rig_state *rig_s;
  struct ft817_priv_data *p;
  unsigned char *cmd;		/* points to sequence to send */
 
  if (!rig)  return -RIG_EINVAL;

  p = (struct ft817_priv_data*)rig->state.priv;
  rig_s = &rig->state;
  
  if (! p->pcs[cmd_index].ncomp) {
    rig_debug(RIG_DEBUG_VERBOSE,"ft817: Attempt to send incomplete sequence \n");
    return -RIG_EINVAL;
  }

  cmd = (unsigned char *) p->pcs[cmd_index].nseq; /* get native sequence */
  write_block(&rig_s->rigport, cmd, YAESU_CMD_LENGTH);
  
  return RIG_OK;
}


int ft817_set_freq(RIG *rig, vfo_t vfo, freq_t freq) {
  struct rig_state *rig_s;
  struct ft817_priv_data *p;
  unsigned char *cmd;		/* points to sequence to send */
  unsigned char cmd_index;	/* index of sequence to send */

  if (!rig)  return -RIG_EINVAL;

  p = (struct ft817_priv_data*)rig->state.priv;

  rig_s = &rig->state;

  rig_debug(RIG_DEBUG_VERBOSE,"ft817: requested freq = %"PRIfreq" Hz \n", freq);
  rig_debug(RIG_DEBUG_VERBOSE,"ft817: vfo =%i \n", vfo);

  if( ( vfo != RIG_VFO_CURR ) &&
      ( ft817_set_vfo( rig, vfo ) != RIG_OK ) )  return -RIG_ERJCTED;
 
  switch( vfo ) {
  case RIG_VFO_CURR:
  case RIG_VFO_A:
  case RIG_VFO_B:
    cmd_index = FT817_NATIVE_CAT_SET_FREQ;
    break;
  default:
    rig_debug(RIG_DEBUG_VERBOSE,"ft817: Unknown VFO \n");
    return -RIG_EINVAL;
  }

  memcpy(p->p_cmd,&ncmd[cmd_index].nseq,YAESU_CMD_LENGTH);

  to_bcd_be(p->p_cmd,freq/10,8);	/* store bcd format in in p_cmd */
				/* TODO -- fix 10Hz resolution -- FS */

  rig_debug(RIG_DEBUG_VERBOSE,"ft817: requested freq after conversion = %"PRIll" Hz \n", from_bcd_be(p->p_cmd,8)* 10 );

  cmd = p->p_cmd; /* get native sequence */
  write_block(&rig_s->rigport, cmd, YAESU_CMD_LENGTH);

  return RIG_OK;
}

int ft817_get_freq(RIG *rig, vfo_t vfo, freq_t *freq) {

  int n = 0;
  unsigned char data[ YAESU_CMD_LENGTH ];

  if( !rig )  return -RIG_EINVAL;
  if( !freq )  return -RIG_EINVAL;

  serial_flush( &rig->state.rigport );

  ft817_send_priv_cmd( rig, FT817_NATIVE_CAT_GET_FREQ_MODE_STATUS );

  n = read_block( &rig->state.rigport, data, YAESU_CMD_LENGTH );

  if( n == YAESU_CMD_LENGTH ) {
//     printf( "[%.2x %.2x %.2x %.2x]", data[0], data[1], data[2], data[3] );
     *freq = MHz( 100*((data[0] & 0xf0) >> 4) ) +
             MHz(  10*( data[0] & 0x0f)       ) +
	     MHz(     ((data[1] & 0xf0) >> 4) ) +
	     kHz( 100*( data[1] & 0x0f)       ) +
	     kHz(  10*((data[2] & 0xf0) >> 4) ) +
	     kHz(     ( data[2] & 0x0f)       ) +
	          100*((data[3] & 0xf0) >> 4)   +
		   10*( data[3] & 0x0f);

     return RIG_OK;
  }

  return -RIG_EIO;
}


int ft817_set_mode(RIG *rig, vfo_t vfo, rmode_t mode, pbwidth_t width) {
  unsigned char cmd_index;	/* index of sequence to send */

  rig_debug(RIG_DEBUG_VERBOSE,"ft817: generic mode = %x \n", mode);

  switch(mode) {
  case RIG_MODE_AM:
    cmd_index = FT817_NATIVE_CAT_SET_MODE_AM;
    break;
  case RIG_MODE_CW:
    cmd_index = FT817_NATIVE_CAT_SET_MODE_CW;
    break;
  case RIG_MODE_USB:
    cmd_index = FT817_NATIVE_CAT_SET_MODE_USB;
    break;
  case RIG_MODE_LSB:
    cmd_index = FT817_NATIVE_CAT_SET_MODE_LSB;
    break;
  case RIG_MODE_FM:
    cmd_index = FT817_NATIVE_CAT_SET_MODE_FM;
    break;
  case RIG_MODE_RTTY:
    cmd_index = FT817_NATIVE_CAT_SET_MODE_DIG;
    break;
  default:
    return -RIG_EINVAL;
  }

  switch(width) {
  case RIG_PASSBAND_NORMAL:
    return ft817_send_priv_cmd(rig,cmd_index);
  default:
    return -RIG_EINVAL;    
  }

  return ft817_send_priv_cmd(rig,cmd_index);
}

int ft817_get_mode(RIG *rig, vfo_t vfo, rmode_t *mode, pbwidth_t *width) {

  int n = 0;
  unsigned char data[ YAESU_CMD_LENGTH ];

  if( !rig )  return -RIG_EINVAL;
  if( !mode )  return -RIG_EINVAL;
  if( !width )  return -RIG_EINVAL;

  serial_flush( &rig->state.rigport );
  ft817_send_priv_cmd( rig, FT817_NATIVE_CAT_GET_FREQ_MODE_STATUS );
  n = read_block( &rig->state.rigport, data, YAESU_CMD_LENGTH );
  if( n == YAESU_CMD_LENGTH ) {

    switch( data[4] ) {
    case 0x00:
      *mode = RIG_MODE_LSB;
      break;
    case 0x01:
      *mode = RIG_MODE_USB;
      break;
    case 0x02:
      *mode = RIG_MODE_CW;
      break;
    case 0x03:
      *mode = RIG_MODE_CW;  /* better suggestion? */
      break;
    case 0x04:
      *mode = RIG_MODE_AM;
      break;
    case 0x08:
      *mode = RIG_MODE_FM;
      break;
    case 0x0a:
      *mode = RIG_MODE_RTTY;
      break;
    default:
      *mode = RIG_MODE_NONE;
    };

    *width = RIG_PASSBAND_NORMAL;  /* TODO: be a bit more creative? */
    return RIG_OK;
  }

  return -RIG_EIO;
}

int ft817_set_vfo(RIG *rig, vfo_t vfo) {

  struct ft817_priv_data *p = (struct ft817_priv_data*)rig->state.priv;

  if (!rig)  return -RIG_EINVAL;

  switch(vfo) {
  case RIG_VFO_A:
  case RIG_VFO_B:
    if( p->current_vfo != vfo ) {
      if( ft817_send_priv_cmd( rig, FT817_NATIVE_CAT_SET_VFOAB ) == RIG_OK ) {
        p->current_vfo = vfo;
      } else {
        return -RIG_ERJCTED;
      }
    }
    break;
  default:
    return -RIG_EINVAL;
  }

  return RIG_OK;
}


int ft817_get_vfo(RIG *rig, vfo_t *vfo) {

  if( !rig )  return -RIG_EINVAL;
  if( !vfo )  return -RIG_EINVAL;
  
  /* No cmd to get vfo, return last-known value */
  *vfo = ((struct ft817_priv_data*)rig->state.priv)->current_vfo;

  return RIG_OK;
}


/* TODO:  consider the value of vfo */
int ft817_set_ptt(RIG *rig, vfo_t vfo, ptt_t ptt) {

  unsigned char cmd_index;

  rig_debug(RIG_DEBUG_VERBOSE,"ft817:ft817_set_ptt called \n");

  switch(ptt) {
  case RIG_PTT_ON:
    cmd_index = FT817_NATIVE_CAT_PTT_ON;
    break;
  case RIG_PTT_OFF:
    cmd_index = FT817_NATIVE_CAT_PTT_OFF;
    break;
  default:
    return -RIG_EINVAL;
  }

  ft817_send_priv_cmd(rig,cmd_index);

  return RIG_OK;
}


#if 0
/* TODO: all of this */
int ft817_get_ptt(RIG *rig, vfo_t vfo, ptt_t *ptt) {
  return -RIG_ENIMPL;
}


/* TODO: all of this */
int ft817_set_level(RIG *rig, vfo_t vfo, setting_t level, value_t val) {
   return -RIG_ENIMPL;
}


/* TODO: all of this */
int ft817_get_level(RIG *rig, vfo_t vfo, setting_t level, value_t *val) {

   if( !rig )  return -RIG_EINVAL;
   if( !val )  return -RIG_EINVAL;
   
   switch( level ) {
   
   case RIG_LEVEL_STRENGTH:
      break;
   case RIG_LEVEL_RFPOWER:
      break;
   default:
      return -RIG_EINVAL;
   }

   return -RIG_ENIMPL;
}


int ft817_set_func(RIG *rig, vfo_t vfo, setting_t func, int status) {
   return -RIG_ENIMPL;
}


int ft817_get_func(RIG *rig, vfo_t vfo, setting_t func, int *status) {
   return -RIG_ENIMPL;
}


int ft817_set_parm(RIG *rig, setting_t parm, value_t val) {
   return -RIG_ENIMPL;
}


int ft817_get_parm(RIG *rig, setting_t parm, value_t *val) {
   return -RIG_ENIMPL;
}
#endif

