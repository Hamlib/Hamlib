/*
 * hamlib - (C) Frank Singleton 2000-2003
 *          (C) Stephane Fillod 2000-2008
 *
 * ft100.c - (C) Chris Karpinsky 2001 (aa1vl@arrl.net)
 * This shared library provides an API for communicating
 * via serial interface to an FT-100 using the "CAT" interface.
 * The starting point for this code was Frank's ft847 implementation.
 *
 *
 *    $Id: ft100.c,v 1.23 2008-09-16 18:11:26 fillods Exp $  
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

#include <stdio.h>
#include <stdlib.h>
#include <string.h>  	/* String function definitions */
#include <unistd.h>  	/* UNIX standard function definitions */
#include <math.h>

#include "hamlib/rig.h"
#include "serial.h"
#include "yaesu.h"
#include "ft100.h"
#include "misc.h"
#include "yaesu_tones.h"


/* prototypes */

static int ft100_send_priv_cmd(RIG *rig, unsigned char ci);


/* Native ft100 cmd set prototypes. These are READ ONLY as each */
/* rig instance will copy from these and modify if required . */
/* Complete sequences (1) can be read and used directly as a cmd sequence . */
/* Incomplete sequences (0) must be completed with extra parameters */
/* eg: mem number, or freq etc.. */


/* kc2ivl - what works on a ft100    as of 02/27/2002        */
/*          ptt on/off                                       */
/*          set mode AM,CW,USB,LSB,FM use RTTY for DIG mode  */
/*          set split on/off                                 */
/*          set repeater +, - splx                           */
/*          set frequency of current vfo                     */

static const yaesu_cmd_set_t ncmd[] = {
  { 0, { 0x00, 0x00, 0x00, 0x00, 0x00 } }, /* lock on */
  { 0, { 0x00, 0x00, 0x00, 0x00, 0x00 } }, /* lock off */
   
  { 1, { 0x00, 0x00, 0x00, 0x01, 0x0f } }, /* ptt on */
  { 1, { 0x00, 0x00, 0x00, 0x00, 0x0f } }, /* ptt off */
   
  { 0, { 0x00, 0x00, 0x00, 0x00, 0x0a } }, /* set freq */
   
  { 1, { 0x00, 0x00, 0x00, 0x00, 0x0c } }, /* mode set main LSB */
  { 1, { 0x00, 0x00, 0x00, 0x01, 0x0c } }, /* mode set main USB */
  { 1, { 0x00, 0x00, 0x00, 0x02, 0x0c } }, /* mode set main CW */
  { 1, { 0x00, 0x00, 0x00, 0x03, 0x0c } }, /* mode set main CWR */
  { 1, { 0x00, 0x00, 0x00, 0x04, 0x0c } }, /* mode set main AM */
  { 1, { 0x00, 0x00, 0x00, 0x06, 0x0c } }, /* mode set main FM */
  { 1, { 0x00, 0x00, 0x00, 0x05, 0x0c } }, /* mode set main DIG */
  { 1, { 0x00, 0x00, 0x00, 0x07, 0x0c } }, /* mode set main WFM */
   
  { 0, { 0x00, 0x00, 0x00, 0x00, 0x00 } }, /* clar on */
  { 0, { 0x00, 0x00, 0x00, 0x00, 0x00 } }, /* clar off */
  { 0, { 0x00, 0x00, 0x00, 0x00, 0x00 } }, /* set clar freq */

  { 0, { 0x00, 0x00, 0x00, 0x00, 0x05 } }, /* toggle vfo a/b */
  { 1, { 0x00, 0x00, 0x00, 0x00, 0x05 } }, /* select vfo a   */
  { 1, { 0x00, 0x00, 0x00, 0x01, 0x05 } }, /* select vfo b   */
   
  { 1, { 0x00, 0x00, 0x00, 0x01, 0x01 } }, /* split on */
  { 1, { 0x00, 0x00, 0x00, 0x00, 0x01 } }, /* split off */
   
  { 1, { 0x00, 0x00, 0x00, 0x01, 0x84 } }, /* set RPT shift MINUS */
  { 1, { 0x00, 0x00, 0x00, 0x02, 0x84 } }, /* set RPT shift PLUS */
  { 1, { 0x00, 0x00, 0x00, 0x00, 0x84 } }, /* set RPT shift SIMPLEX */
  { 0, { 0x00, 0x00, 0x00, 0x00, 0x00 } }, /* set RPT offset freq */

/* fix me */
  { 1, { 0x00, 0x00, 0x00, 0x03, 0x92 } }, /* set DCS on */
  { 1, { 0x00, 0x00, 0x00, 0x02, 0x92 } }, /* set CTCSS/DCS enc/dec on */
  { 1, { 0x00, 0x00, 0x00, 0x01, 0x92 } }, /* set CTCSS/DCS enc on */
  { 1, { 0x00, 0x00, 0x00, 0x00, 0x92 } }, /* set CTCSS/DCS off */
/* em xif */
   
  { 0, { 0x00, 0x00, 0x00, 0x00, 0x90 } }, /* set CTCSS tone */
  { 0, { 0x00, 0x00, 0x00, 0x00, 0x91 } }, /* set DCS code */
   
  { 0, { 0x00, 0x00, 0x00, 0x00, 0x00 } }, /* get RX status  */
  { 0, { 0x00, 0x00, 0x00, 0x00, 0x00 } }, /* get TX status  */
   
  { 0, { 0x00, 0x00, 0x00, 0x00, 0x10 } }, /* get FREQ and MODE status */
   
  { 0, { 0x00, 0x00, 0x00, 0x00, 0x00 } }, /* pwr wakeup sequence */
  { 0, { 0x00, 0x00, 0x00, 0x00, 0x00 } }, /* pwr on */
  { 0, { 0x00, 0x00, 0x00, 0x00, 0x00 } }, /* pwr off */
   
  { 1, { 0x00, 0x00, 0x00, 0x01, 0x10 } }, /* read status block */
  { 1, { 0x00, 0x00, 0x00, 0x00, 0xf7 } }, /* read meter block */
  { 1, { 0x00, 0x00, 0x00, 0x01, 0xfa } }  /* read flags block */ 
};


#define FT100_ALL_RX_MODES (RIG_MODE_AM|RIG_MODE_CW|RIG_MODE_CWR|RIG_MODE_USB|RIG_MODE_LSB|RIG_MODE_RTTY|RIG_MODE_FM)
#define FT100_SSB_CW_RX_MODES (RIG_MODE_CW|RIG_MODE_CWR|RIG_MODE_USB|RIG_MODE_LSB)
#define FT100_AM_FM_RX_MODES (RIG_MODE_AM|RIG_MODE_FM)

#define FT100_OTHER_TX_MODES (RIG_MODE_CW|RIG_MODE_CWR|RIG_MODE_USB|RIG_MODE_LSB|RIG_MODE_RTTY|RIG_MODE_FM)
#define FT100_AM_TX_MODES (RIG_MODE_AM)
#define FT100_GET_RIG_LEVELS (RIG_LEVEL_RAWSTR|RIG_LEVEL_RFPOWER|RIG_LEVEL_SWR|RIG_LEVEL_ALC)
#define FT100_FUNC_ALL (RIG_FUNC_LOCK|RIG_FUNC_TONE|RIG_FUNC_TSQL)

#define FT100_VFO_ALL (RIG_VFO_A|RIG_VFO_B)

/* S-meter calibration, ascending order of RAW values */
#define FT100_STR_CAL { 9, \
	        { \
			{  90,  60 }, /* +60 */ \
			{ 105,  40 }, /* +40 */ \
			{ 115,  20 }, /* +20 */ \
			{ 120,   0 }, /*  S9 */ \
			{ 130,  -6 }, /*  S8 */ \
			{ 140, -12 }, /*  S7 */ \
			{ 160, -18 }, /*  S6 */ \
			{ 180, -24 }, /*  S5 */ \
			{ 200, -54 }  /*  S0 */ \
	        } }

const struct rig_caps ft100_caps = {
  .rig_model = 		RIG_MODEL_FT100,
  .model_name = 	"FT-100", 
  .mfg_name = 		"Yaesu", 
  .version = 		"0.3", 
  .copyright = 		"LGPL",
  .status = 		RIG_STATUS_BETA,
  .rig_type = 		RIG_TYPE_TRANSCEIVER,
  .ptt_type = 		RIG_PTT_RIG,
  .dcd_type = 		RIG_DCD_NONE,
  .port_type = 		RIG_PORT_SERIAL,
  .serial_rate_min = 	4800,                /* ft100/d 4800 only */
  .serial_rate_max = 	4800,                /* no others allowed */
  .serial_data_bits = 	8,
  .serial_stop_bits = 	2,
  .serial_parity = 	RIG_PARITY_NONE,
  .serial_handshake = 	RIG_HANDSHAKE_NONE, 
  .write_delay = 		FT100_WRITE_DELAY,
  .post_write_delay = 	FT100_POST_WRITE_DELAY,
  .timeout = 		100,
  .retry = 		0, 
  .has_get_func = 		RIG_FUNC_NONE,
  .has_set_func = 		FT100_FUNC_ALL, 
  .has_get_level = 	FT100_GET_RIG_LEVELS,
  .has_set_level = 	RIG_LEVEL_NONE,
  .has_get_parm = 		RIG_PARM_NONE,
  .has_set_parm = 		RIG_PARM_NONE,	/* FIXME: parms */
  .level_gran =		{},		/* granularity */
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

  .chan_list =  { RIG_CHAN_END, },	/* FIXME: memory chan .list =  78 */

  .rx_range_list1 =  { RIG_FRNG_END, },    /* FIXME: enter region 1 setting */
  .tx_range_list1 =  { RIG_FRNG_END, },
  .rx_range_list2 =  { 
    {kHz(100),MHz(56), FT100_ALL_RX_MODES,-1,-1,FT100_VFO_ALL},
    {MHz(76), MHz(108),RIG_MODE_WFM,      -1,-1,FT100_VFO_ALL},
    {MHz(108),MHz(154),FT100_ALL_RX_MODES,-1,-1,FT100_VFO_ALL},
    {MHz(420),MHz(470),FT100_ALL_RX_MODES,-1,-1,FT100_VFO_ALL},
    RIG_FRNG_END, 
  },

  .tx_range_list2 =  {
    {MHz(1.8),   MHz(2),      FT100_OTHER_TX_MODES, W(0.5),W(0.5),FT100_VFO_ALL},
    {MHz(1.8),   MHz(2),      FT100_AM_TX_MODES,    W(0.5),W(1.5),FT100_VFO_ALL},
    {MHz(3.5),   MHz(4),      FT100_OTHER_TX_MODES, W(0.5),W(5.0),FT100_VFO_ALL},
    {MHz(3.5),   MHz(4),      FT100_AM_TX_MODES,    W(0.5),W(1.5),FT100_VFO_ALL},
    {MHz(7),     MHz(7.3),    FT100_OTHER_TX_MODES, W(0.5),W(5.0),FT100_VFO_ALL},
    {MHz(7),     MHz(7.3),    FT100_AM_TX_MODES,    W(0.5),W(1.5),FT100_VFO_ALL},
    {MHz(10),    MHz(10.150), FT100_OTHER_TX_MODES, W(0.5),W(5.0),FT100_VFO_ALL},
    {MHz(10),    MHz(10.150), FT100_AM_TX_MODES,    W(0.5),W(1.5),FT100_VFO_ALL},
    {MHz(14),    MHz(14.350), FT100_OTHER_TX_MODES, W(0.5),W(5.0),FT100_VFO_ALL},
    {MHz(14),    MHz(14.350), FT100_AM_TX_MODES,    W(0.5),W(1.5),FT100_VFO_ALL},
    {MHz(18.068),MHz(18.168), FT100_OTHER_TX_MODES, W(0.5),W(5.0),FT100_VFO_ALL},
    {MHz(18.068),MHz(18.168), FT100_AM_TX_MODES,    W(0.5),W(1.5),FT100_VFO_ALL},
    {MHz(21),    MHz(21.450), FT100_OTHER_TX_MODES, W(0.5),W(5.0),FT100_VFO_ALL},
    {MHz(21),    MHz(21.450), FT100_AM_TX_MODES,    W(0.5),W(1.5),FT100_VFO_ALL},
    {MHz(24.890),MHz(24.990), FT100_OTHER_TX_MODES, W(0.5),W(5.0),FT100_VFO_ALL},
    {MHz(24.890),MHz(24.990), FT100_AM_TX_MODES,    W(0.5),W(1.5),FT100_VFO_ALL},
    {MHz(28),    MHz(29.7),   FT100_OTHER_TX_MODES, W(0.5),W(5.0),FT100_VFO_ALL},
    {MHz(28),    MHz(29.7),   FT100_AM_TX_MODES,    W(0.5),W(1.5),FT100_VFO_ALL},
    {MHz(50),    MHz(54),     FT100_OTHER_TX_MODES, W(0.5),W(5.0),FT100_VFO_ALL},
    {MHz(50),    MHz(54),     FT100_AM_TX_MODES,    W(0.5),W(1.5),FT100_VFO_ALL},
    {MHz(144),   MHz(148),    FT100_OTHER_TX_MODES, W(0.5),W(5.0),FT100_VFO_ALL}, 
    {MHz(144),   MHz(148),    FT100_AM_TX_MODES,    W(0.5),W(1.5),FT100_VFO_ALL}, 
    {MHz(430),   MHz(440),    FT100_OTHER_TX_MODES, W(0.5),W(5.0),FT100_VFO_ALL},
    {MHz(430),   MHz(440),    FT100_AM_TX_MODES,    W(0.5),W(1.5),FT100_VFO_ALL},
    RIG_FRNG_END, 
  },

  .tuning_steps =  {
	  /* FIXME */
    {FT100_SSB_CW_RX_MODES,10},
    {FT100_SSB_CW_RX_MODES,100},
    {FT100_AM_FM_RX_MODES,10},
    {FT100_AM_FM_RX_MODES,100},
    {RIG_MODE_WFM, kHz(5)},
    {RIG_MODE_WFM, kHz(50)},
    RIG_TS_END,
  },  

  .filters =  {
    {RIG_MODE_SSB|RIG_MODE_CW|RIG_MODE_CWR|RIG_MODE_RTTY, kHz(2.4)},
    {RIG_MODE_SSB|RIG_MODE_CW|RIG_MODE_CWR|RIG_MODE_RTTY, Hz(300)},
    {RIG_MODE_SSB|RIG_MODE_CW|RIG_MODE_CWR|RIG_MODE_RTTY, Hz(500)},
    {RIG_MODE_AM|RIG_MODE_FM, kHz(6)},
    {RIG_MODE_WFM, kHz(230)},
    RIG_FLT_END,
  },
  .str_cal = FT100_STR_CAL,

  .priv = 			NULL,
  .rig_init = 		ft100_init, 
  .rig_cleanup = 		ft100_cleanup, 
  .rig_open = 		ft100_open, 
  .rig_close = 		ft100_close, 
  .set_freq = 		ft100_set_freq,
  .get_freq = 		ft100_get_freq,
  .set_mode = 		ft100_set_mode,
  .get_mode = 		ft100_get_mode,
  .set_vfo = 		ft100_set_vfo,
  .get_vfo = 		ft100_get_vfo,
  .set_ptt = 	        ft100_set_ptt,
  .get_ptt = 		NULL,
  .get_dcd = 		NULL,
  .set_rptr_shift = 	ft100_set_rptr_shift,
  .get_rptr_shift = 	NULL,
  .set_rptr_offs = 	NULL,
  .get_rptr_offs = 	NULL,
  .set_split_freq = 	NULL,
  .get_split_freq = 	NULL,
  .set_split_mode =        NULL,
  .get_split_mode = 	NULL,
  .set_split_vfo = 	ft100_set_split_vfo,
  .get_split_vfo =	NULL,
  .set_rit = 		NULL,
  .get_rit = 		NULL,
  .set_xit = 		NULL,
  .get_xit = 		NULL,
  .set_ts = 		NULL,
  .get_ts = 		NULL,
  .set_dcs_code = 		ft100_set_dcs_code,
  .get_dcs_code = 		NULL,
  .set_ctcss_tone =        ft100_set_ctcss_tone,
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
  .get_level = 		ft100_get_level,
  .set_func = 		NULL,
  .get_func = 		NULL,
  .set_parm = 		NULL,
  .get_parm = 		NULL,
}; 


int ft100_init(RIG *rig) {
  struct ft100_priv_data *p;
  
  if (!rig)  return -RIG_EINVAL;
  
  p = (struct ft100_priv_data*)malloc(sizeof(struct ft100_priv_data));
  if (!p)  return -RIG_ENOMEM;

  rig_debug(RIG_DEBUG_VERBOSE,"ft100:ft100_init called \n");

  /* 
   * Copy complete native cmd set to private cmd storage area 
   */

  memcpy(p->pcs,ncmd,sizeof(ncmd));

  p->current_vfo = RIG_VFO_A; /* no clue which VFO is active, so guess VFO 1 */

  rig->state.priv = (void*)p;
  
  return RIG_OK;
}

int ft100_cleanup(RIG *rig) {
  if (!rig)
    return -RIG_EINVAL;
  
  if (rig->state.priv)
    free(rig->state.priv);
  rig->state.priv = NULL;

  rig_debug(RIG_DEBUG_VERBOSE,"ft100:ft100_cleanup called \n");
  
  return RIG_OK;
}

int ft100_open(RIG *rig) {

  rig_debug(RIG_DEBUG_VERBOSE,"ft100:ft100_open called \n");

  return RIG_OK;
}

int ft100_close(RIG *rig) {

  rig_debug(RIG_DEBUG_VERBOSE,"ft100:ft100_close called \n");

  return RIG_OK;
}

/*
 * private helper function to send a private command
 * sequence . Must only be complete sequences.
 *
 */

static int ft100_send_priv_cmd(RIG *rig, unsigned char cmd_index) {
 
  struct rig_state *rig_s;
  struct ft100_priv_data *p;
  unsigned char *cmd;		/* points to sequence to send */
  int i;
   
  rig_debug(RIG_DEBUG_VERBOSE,"ft100: ft100_send_priv_cmd \n");
 
  if (!rig)  return -RIG_EINVAL;

  p = (struct ft100_priv_data*)rig->state.priv;
  rig_s = &rig->state;
  
  if (! p->pcs[cmd_index].ncomp) {
    rig_debug(RIG_DEBUG_ERR,"ft100: Attempt to send incomplete sequence \n");
    return -RIG_EINVAL;
  }
  
  cmd = (unsigned char *) p->pcs[cmd_index].nseq; /* get native sequence */

  rig_debug(RIG_DEBUG_VERBOSE,"ft100: Attempt to send sequence ="); 
  for (i=0; i < YAESU_CMD_LENGTH; i++)
      rig_debug(RIG_DEBUG_VERBOSE," %3i",(int)cmd[i]);
  rig_debug(RIG_DEBUG_VERBOSE," \n");
   
  return write_block(&rig_s->rigport, (char *) cmd, YAESU_CMD_LENGTH);
}


int ft100_set_freq(RIG *rig, vfo_t vfo, freq_t freq) {
  struct rig_state *rig_s;
  struct ft100_priv_data *p;
  unsigned char *cmd;		/* points to sequence to send */
  unsigned char cmd_index;	/* index of sequence to send */

  if (!rig)  return -RIG_EINVAL;

  p = (struct ft100_priv_data*)rig->state.priv;

  rig_s = &rig->state;

  rig_debug(RIG_DEBUG_VERBOSE,"ft100: requested freq = %"PRIfreq" Hz \n", freq);
  rig_debug(RIG_DEBUG_VERBOSE,"ft100: vfo =%i \n", vfo);

  if( ( vfo != RIG_VFO_CURR ) &&
      ( ft100_set_vfo( rig, vfo ) != RIG_OK ) )  return -RIG_ERJCTED;
 
  switch( vfo ) {
  case RIG_VFO_CURR:
  case RIG_VFO_A:
  case RIG_VFO_B:
    cmd_index = FT100_NATIVE_CAT_SET_FREQ;
    break;
  default:
    rig_debug(RIG_DEBUG_VERBOSE,"ft100: Unknown VFO \n");
    return -RIG_EINVAL;
  }

  memcpy(p->p_cmd,&ncmd[cmd_index].nseq,YAESU_CMD_LENGTH);

  /* fixed 10Hz bug by OH2MMY */
  freq = (int)freq/10;
  to_bcd(p->p_cmd,freq,8);	/* store bcd format in in p_cmd */
				/* TODO -- fix 10Hz resolution -- FS */

  rig_debug(RIG_DEBUG_VERBOSE,"ft100: requested freq after conversion = %"PRIfreq" Hz \n", from_bcd_be(p->p_cmd,8)* 10 );

  cmd = p->p_cmd; /* get native sequence */
  write_block(&rig_s->rigport, (char *) cmd, YAESU_CMD_LENGTH);

  return RIG_OK;
}

int ft100_get_freq(RIG *rig, vfo_t vfo, freq_t *freq) {
  
   FT100_STATUS_INFO ft100_status;
  
   freq_t d1, d2;
   char freq_str[10];
    
   int ret;
    
   rig_debug(RIG_DEBUG_VERBOSE,"ft100: get_freq \n");
   
   if( !rig )  return -RIG_EINVAL;
   if( !freq )  return -RIG_EINVAL;
  
   serial_flush( &rig->state.rigport );

   ret = ft100_send_priv_cmd(rig,FT100_NATIVE_CAT_READ_STATUS);
   if (ret != RIG_OK)
	   return ret;

   ret = read_block( &rig->state.rigport, (char*)&ft100_status, sizeof(FT100_STATUS_INFO));  
   rig_debug(RIG_DEBUG_VERBOSE,"ft100: read status=%i \n",ret);
   if (ret < 0)
	   return ret;

   rig_debug(RIG_DEBUG_VERBOSE,"ft100: Freq= %3i %3i %3i %3i \n",(int)ft100_status.freq[0], (int)ft100_status.freq[1], (int)ft100_status.freq[2],(int)ft100_status.freq[3]);
  
   /* now convert it .... */
   
   sprintf(freq_str, "%02X%02X%02X%02X",
	  ft100_status.freq[0],
	  ft100_status.freq[1],
	  ft100_status.freq[2],
	  ft100_status.freq[3]);
   
   d1=strtol(freq_str,NULL,16);
   d2=(d1*1.25); 		/* fixed 10Hz bug by OH2MMY */
   
   rig_debug(RIG_DEBUG_VERBOSE,"ft100: d1=%"PRIfreq" d2=%"PRIfreq"\n",d1,d2);
   
   rig_debug(RIG_DEBUG_VERBOSE,"ft100: get_freq= %8"PRIll" \n",(long long)d2);
   
   *freq = d2;
   
   return RIG_OK;
}

int ft100_set_mode(RIG *rig, vfo_t vfo, rmode_t mode, pbwidth_t width) {
  unsigned char cmd_index;	/* index of sequence to send */
  int ret;

  rig_debug(RIG_DEBUG_VERBOSE,"ft100: generic mode = %x, width %d\n", mode, width);

  switch(mode) {
  case RIG_MODE_AM:
    cmd_index = FT100_NATIVE_CAT_SET_MODE_AM;
    break;
  case RIG_MODE_CW:
    cmd_index = FT100_NATIVE_CAT_SET_MODE_CW;
    break;
  case RIG_MODE_CWR:
    cmd_index = FT100_NATIVE_CAT_SET_MODE_CWR;
    break;
  case RIG_MODE_USB:
    cmd_index = FT100_NATIVE_CAT_SET_MODE_USB;
    break;
  case RIG_MODE_LSB:
    cmd_index = FT100_NATIVE_CAT_SET_MODE_LSB;
    break;
  case RIG_MODE_FM:
    cmd_index = FT100_NATIVE_CAT_SET_MODE_FM;
    break;
  case RIG_MODE_RTTY:
    cmd_index = FT100_NATIVE_CAT_SET_MODE_DIG;
    break;
  case RIG_MODE_WFM:
    cmd_index = FT100_NATIVE_CAT_SET_MODE_WFM;
    break;
  default:
    return -RIG_EINVAL;
  }

  ret = ft100_send_priv_cmd(rig,cmd_index);
  if (ret != RIG_OK)
	  return ret;

#if 0
  /* ignore width for now. Should be Opcode 0x8C */
  switch(width) {
  case RIG_PASSBAND_NORMAL:
    return ft100_send_priv_cmd(rig,cmd_index);
  default:
    return -RIG_EINVAL;    
  }
#endif

  return RIG_OK;
}



/*  ft100_get_mode fixed by OH2MMY 
 *  Still answers wrong if on AM. Bug in rig's firmware?
 *  The rig answers something weird then, not what the manual says
 *  and the answer is different every time. Other modes do work.
 */

int ft100_get_mode(RIG *rig, vfo_t vfo, rmode_t *mode, pbwidth_t *width) {

  int n = 0;
  unsigned char data[ sizeof(FT100_STATUS_INFO) ];

  if( !rig )  return -RIG_EINVAL;
  if( !mode )  return -RIG_EINVAL;
  if( !width )  return -RIG_EINVAL;

  serial_flush( &rig->state.rigport );

  n = ft100_send_priv_cmd( rig, FT100_NATIVE_CAT_READ_STATUS );
  if (n != RIG_OK)
	  return n;
  n = read_block( &rig->state.rigport, (char *) data, sizeof(FT100_STATUS_INFO) );
  if (n < 0)
	  return n;
  
    switch( data[5] & 0x0f ) {
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
      *mode = RIG_MODE_CWR;
      break;
    case 0x04:
      *mode = RIG_MODE_AM;
      break; 
    case 0x05:
      *mode = RIG_MODE_RTTY;
      break;
    case 0x06:
      *mode = RIG_MODE_FM;
      break;
    case 0x07:
      *mode = RIG_MODE_WFM;
      break; 
    default:
      *mode = RIG_MODE_NONE;
    };

    switch( data[5] >> 4 ) {
    case 0x00:
      *width = Hz(6000);
      break;
    case 0x01:
      *width = Hz(2400);
      break;
    case 0x02:
      *width = Hz(500);
      break;
    case 0x03:
      *width = Hz(300);
      break;
    default:
      *width = RIG_PASSBAND_NORMAL;
    };

    return RIG_OK;
}



/*  Function ft100_set_vfo fixed by OH2MMY 
 *  Split doesn't work because there's no native command for that.
 *  Maybe will fix it later.
 */

int ft100_set_vfo(RIG *rig, vfo_t vfo) {

  struct ft100_priv_data *p = (struct ft100_priv_data*)rig->state.priv;

  if (!rig)  return -RIG_EINVAL;

  switch(vfo) {
  case RIG_VFO_A:
    if( p->current_vfo != vfo ) { 
      if( ft100_send_priv_cmd( rig, FT100_NATIVE_CAT_SET_VFOA ) == RIG_OK ) {
        p->current_vfo = vfo;
      } else {
        return -RIG_ERJCTED;
      }
    } 
    break;
  case RIG_VFO_B:
    if( p->current_vfo != vfo ) { 
      if( ft100_send_priv_cmd( rig, FT100_NATIVE_CAT_SET_VFOB ) == RIG_OK ) {
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



/*  Function ft100_get_vfo written again by OH2MMY
 *  Tells the right answer if in VFO mode.
 *  TODO: handle memory modes.
 */

int ft100_get_vfo(RIG *rig, vfo_t *vfo) {

  struct ft100_priv_data *priv;
  unsigned char ft100_flags[ sizeof(FT100_FLAG_INFO) ];
  int n;

  if( !rig )  return -RIG_EINVAL;
  if( !vfo )  return -RIG_EINVAL;
  
  priv = (struct ft100_priv_data *)rig->state.priv;

  serial_flush( &rig->state.rigport );

  n = ft100_send_priv_cmd( rig, FT100_NATIVE_CAT_READ_FLAGS );
  if (n < 0)
	  return n;
  n = read_block( &rig->state.rigport, (char *) ft100_flags, sizeof(FT100_FLAG_INFO) );
  rig_debug(RIG_DEBUG_VERBOSE,"ft100: read flags=%i \n",n);
  if (n < 0)
	  return n;

  if ((ft100_flags[1] & 4) == 4) {
    *vfo = RIG_VFO_B;
    priv->current_vfo = RIG_VFO_B;
  } else {
    *vfo = RIG_VFO_A;
    priv->current_vfo = RIG_VFO_A;
  }

  return RIG_OK;
}


/* TODO:  consider the value of vfo */
int ft100_set_ptt(RIG *rig, vfo_t vfo, ptt_t ptt) {

  unsigned char cmd_index;

  rig_debug(RIG_DEBUG_VERBOSE,"ft100:ft100_set_ptt called \n");

  switch(ptt) {
  case RIG_PTT_ON:
    cmd_index = FT100_NATIVE_CAT_PTT_ON;
    break;
  case RIG_PTT_OFF:
    cmd_index = FT100_NATIVE_CAT_PTT_OFF;
    break;
  default:
    return -RIG_EINVAL;
  }

  ft100_send_priv_cmd(rig,cmd_index);

  return RIG_OK;
}


#if 0
/* TODO: all of this */
int ft100_get_ptt(RIG *rig, vfo_t vfo, ptt_t *ptt) {
  return -RIG_ENIMPL;
}


/* TODO: all of this */
int ft100_set_level(RIG *rig, vfo_t vfo, setting_t level, value_t val) {
   return -RIG_ENIMPL;
}
#endif

/*
 * blind implementation of get_level.
 * Please test on real hardware and send report on hamlib mailing list
 */
int ft100_get_level(RIG *rig, vfo_t vfo, setting_t level, value_t *val) {
   
   int ret;
   float f;
   FT100_METER_INFO ft100_meter;

   if( !rig )  return -RIG_EINVAL;
   if( !val )  return -RIG_EINVAL;

   rig_debug(RIG_DEBUG_VERBOSE,"%s: %s\n", __FUNCTION__, rig_strlevel(level));
   
   ret = ft100_send_priv_cmd(rig,FT100_NATIVE_CAT_READ_METERS);
   if (ret != RIG_OK)
	   return ret;
   ret = read_block( &rig->state.rigport, (char*)&ft100_meter, sizeof(FT100_METER_INFO));  
   rig_debug(RIG_DEBUG_VERBOSE,"%s: read meters=%d\n",__FUNCTION__, ret);
   if (ret < 0)
	   return ret;
  
   switch( level ) {
   
   case RIG_LEVEL_RAWSTR:
      val->i = ft100_meter.s_meter;
      break;
   case RIG_LEVEL_RFPOWER:
      val->f = (float)ft100_meter.tx_fwd_power/0xff;
      break;
   case RIG_LEVEL_SWR:
      if (ft100_meter.tx_fwd_power == 0)
	val->f = 0;
      else {
      	f = sqrt((float)ft100_meter.tx_rev_power/(float)ft100_meter.tx_fwd_power);
      	val->f = (1+f)/(1-f);
      }
      break;
   case RIG_LEVEL_ALC:
      /* need conversion ? */
      val->f = (float)ft100_meter.alc_level/0xff;
      break;
   default:
      return -RIG_EINVAL;
   }

   return RIG_OK;
}

#if 0
/* TODO: all of this */

int ft100_set_func(RIG *rig, vfo_t vfo, setting_t func, int status) {
   return -RIG_ENIMPL;
}


int ft100_get_func(RIG *rig, vfo_t vfo, setting_t func, int *status) {
   return -RIG_ENIMPL;
}


int ft100_set_parm(RIG *rig, setting_t parm, value_t val) {
   return -RIG_ENIMPL;
}


int ft100_get_parm(RIG *rig, setting_t parm, value_t *val) {
   return -RIG_ENIMPL;
}
#endif


/* kc2ivl */
int ft100_set_split_vfo(RIG *rig, vfo_t vfo, split_t split, vfo_t tx_vfo) {

  unsigned char cmd_index;

  rig_debug(RIG_DEBUG_VERBOSE,"ft100:ft100_set_split called \n");

  switch(split) {
  case RIG_SPLIT_ON:
    cmd_index = FT100_NATIVE_CAT_SPLIT_ON;
    break;
  case RIG_SPLIT_OFF:
    cmd_index = FT100_NATIVE_CAT_SPLIT_OFF;
    break;
  default:
    return -RIG_EINVAL;
  }
  
  ft100_send_priv_cmd(rig,cmd_index);
  return RIG_OK; 
}

int ft100_set_rptr_shift(RIG *rig, vfo_t vfo, rptr_shift_t shift) {

  unsigned char cmd_index;

  rig_debug(RIG_DEBUG_VERBOSE,"ft100:ft100_set_rptr_shift called \n");

  rig_debug(RIG_DEBUG_VERBOSE,"ft100: + - 0 %3i %3i %3i %3i %c\n", RIG_RPT_SHIFT_PLUS, RIG_RPT_SHIFT_MINUS, RIG_RPT_SHIFT_NONE, shift,  (char)shift);  
  switch(shift) {
  case RIG_RPT_SHIFT_PLUS:
    cmd_index = FT100_NATIVE_CAT_SET_RPT_SHIFT_PLUS;
    break;
  case RIG_RPT_SHIFT_MINUS:
    cmd_index = FT100_NATIVE_CAT_SET_RPT_SHIFT_MINUS;
    break;
  case RIG_RPT_SHIFT_NONE:
    cmd_index = FT100_NATIVE_CAT_SET_RPT_SHIFT_SIMPLEX;   
    break;
  default:
    return -RIG_EINVAL;
  }
  
  ft100_send_priv_cmd(rig,cmd_index);
  return RIG_OK; 
}



int ft100_set_dcs_code(RIG *rig, vfo_t vfo, tone_t code) {
  struct rig_state *rig_s;
  struct ft100_priv_data *p;
  unsigned char *cmd;		/* points to sequence to send */
  unsigned char cmd_index;	/* index of sequence to send */

  if (!rig)  return -RIG_EINVAL;

  p = (struct ft100_priv_data*)rig->state.priv;

  rig_s = &rig->state;

  rig_debug(RIG_DEBUG_VERBOSE,"ft100: ft100_set_dcs_code  =%3i %s\n", code,code_tbl[code]);

  if( ( vfo != RIG_VFO_CURR ) &&
      ( ft100_set_vfo( rig, vfo ) != RIG_OK ) )  return -RIG_ERJCTED;
   
  if (code > 103) return -RIG_EINVAL; /* there are 104 dcs codes available */
  
  switch( vfo ) {
  case RIG_VFO_CURR:
  case RIG_VFO_A:
  case RIG_VFO_B:
    cmd_index = FT100_NATIVE_CAT_SET_DCS_CODE;
    break;
  default:
    rig_debug(RIG_DEBUG_VERBOSE,"ft100: Unknown VFO \n");
    return -RIG_EINVAL;
  }

  cmd = p->p_cmd; /* get native sequence */
  memcpy(cmd,&ncmd[cmd_index].nseq,YAESU_CMD_LENGTH);
  
  cmd[3]=(char)code;
  
  write_block(&rig_s->rigport, (char *) cmd, YAESU_CMD_LENGTH);

  return RIG_OK;
}


int ft100_set_ctcss_tone(RIG *rig, vfo_t vfo, tone_t tone) {
  struct rig_state *rig_s;
  struct ft100_priv_data *p;
  unsigned char *cmd;		/* points to sequence to send */
  unsigned char cmd_index;	/* index of sequence to send */

  if (!rig)  return -RIG_EINVAL;

  if (tone > 38) return -RIG_EINVAL; /* there are 39 ctcss tones available */ 
   
  p = (struct ft100_priv_data*)rig->state.priv;

  rig_s = &rig->state;

  rig_debug(RIG_DEBUG_VERBOSE,"ft100: ft100_set_ctcss_tone  =%3i %s\n",tone,tone_tbl[tone]);

  if( ( vfo != RIG_VFO_CURR ) &&
      ( ft100_set_vfo( rig, vfo ) != RIG_OK ) )  return -RIG_ERJCTED;
 
  switch( vfo ) {
  case RIG_VFO_CURR:
  case RIG_VFO_A:
  case RIG_VFO_B:
    cmd_index = FT100_NATIVE_CAT_SET_CTCSS_FREQ;
    break;
  default:
    rig_debug(RIG_DEBUG_VERBOSE,"ft100: Unknown VFO \n");
    return -RIG_EINVAL;
  }

  cmd = p->p_cmd; /* get native sequence */
  memcpy(cmd,&ncmd[cmd_index].nseq,YAESU_CMD_LENGTH);
  
  cmd[3]=(char)tone;
  
  write_block(&rig_s->rigport, (char *) cmd, YAESU_CMD_LENGTH);

  return RIG_OK;
}


#if 0
/*
 * okie dokie here....
 * get everything, let the calling function figure out what it needs
 *  
 * Flags read added by OH2MMY
 *
 */
int ft100_get_info(RIG *rig, FT100_STATUS_INFO *ft100_status, FT100_METER_INFO *ft100_meter, FT100_FLAG_INFO *ft100_flags)
{
   unsigned char cmd_index;
   int n;
   
   rig_debug(RIG_DEBUG_VERBOSE,"ft100: getting all info \n");
   
   cmd_index=FT100_NATIVE_CAT_READ_STATUS;
   ft100_send_priv_cmd(rig,cmd_index);
   n = read_block( &rig->state.rigport, (char*)ft100_status, sizeof(FT100_STATUS_INFO));  
   rig_debug(RIG_DEBUG_VERBOSE,"ft100: read status=%i \n",n);
   
   cmd_index=FT100_NATIVE_CAT_READ_METERS;
   ft100_send_priv_cmd(rig,cmd_index);
   n = read_block( &rig->state.rigport, (char*)ft100_meter, sizeof(FT100_METER_INFO));  
   rig_debug(RIG_DEBUG_VERBOSE,"ft100: read meters=%i \n",n);
   
   cmd_index=FT100_NATIVE_CAT_READ_FLAGS;
   ft100_send_priv_cmd(rig,cmd_index);
   n = read_block( &rig->state.rigport, (char*)ft100_flags, sizeof(FT100_FLAG_INFO));
   rig_debug(RIG_DEBUG_VERBOSE,"ft100: read flags=%i \n",n);
   
   return RIG_OK;
}
#endif

