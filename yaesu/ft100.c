/*
 * hamlib - (C) Frank Singleton 2000-2002
 *
 * ft100.c - (C) Chris Karpinsky 2001 (aa1vl@arrl.net)
 * This shared library provides an API for communicating
 * via serial interface to an FT-100 using the "CAT" interface.
 * The starting point for this code was Frank's ft847 implementation.
 *
 *
 *    $Id: ft100.c,v 1.7 2003-02-14 15:23:00 avflinsch Exp $  
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
#include <stdio.h>   	/* Standard input/output definitions */
#include <string.h>  	/* String function definitions */
#include <unistd.h>  	/* UNIX standard function definitions */
#include <fcntl.h>   	/* File control definitions */
#include <errno.h>   	/* Error number definitions */
#include <termios.h> 	/* POSIX terminal control definitions */
#include <sys/ioctl.h>

#include <hamlib/rig.h>
#include <hamlib/riglist.h>
#include "serial.h"
#include "yaesu.h"
#include "ft100.h"
#include "misc.h"
#include "yaesu_tones.h"


// avf/kc2ivl fixed translation table feb-14-2003
const char *CFREQ_TBL[256] =
{   
 "00", "01", "02", "03", "04", "05", "06", "07", "08", "09","0A","0B","0C","0D","0E", "0F",	
 "10", "11", "12", "13", "14", "15", "16", "17", "18", "19","1A","1B","1C","1D","1E", "1F",
 "20", "21", "22", "23", "24", "25", "26", "27", "28", "29","2A","2B","2C","2D","2E", "2F",
 "30", "31", "32", "33", "34", "35", "36", "37", "38", "39","3A","3B","3C","3D","3E", "3F",
 "40", "41", "42", "43", "44", "45", "46", "47", "48", "49","4A","4B","4C","4D","4E", "4F",
 "50", "51", "52", "53", "54", "55", "56", "57", "58", "59","5A","5B","5C","5D","5E", "5F",
 "60", "61", "62", "63", "64", "65", "66", "67", "68", "69","6A","6B","6C","6D","6E", "6F",
 "70", "71", "72", "73", "74", "75", "76", "77", "78", "79","7A","7B","7C","7D","7E", "7F",
 "80", "81", "82", "83", "84", "85", "86", "87", "88", "89","8A","8B","8C","8D","8E", "8F",
 "90", "91", "92", "93", "94", "95", "96", "97", "98", "99","9A","9B","9C","9D","9E", "9F",
 "A0", "A1", "A2", "A3", "A4", "A5", "A6", "A7", "A8", "A9","AA","AB","AC","AD","AE", "AF",
 "B0", "B1", "B2", "B3", "B4", "B5", "B6", "B7", "B8", "B9","BA","BB","BC","BD","BE", "BF",
 "C0", "C1", "C2", "C3", "C4", "C5", "C6", "C7", "C8", "C9","CA","CB","CC","CD","CE", "CF",
 "D0", "D1", "D2", "D3", "D4", "D5", "D6", "D7", "D8", "D9","DA","DB","DC","DD","DE", "DF",
 "E0", "E1", "E2", "E3", "E4", "E5", "E6", "E7", "E8", "E9","EA","EB","EC","ED","EE", "EF",
 "F0", "F1", "F2", "F3", "F4", "F5", "F6", "F7", "F8", "F9","FA","FB","FC","FD","FE", "FF"
};




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
  { 0, { 0x00, 0x00, 0x00, 0x00, 0xfa } }  /* read flags block */ 
};


#define FT100_ALL_RX_MODES (RIG_MODE_AM|RIG_MODE_CW|RIG_MODE_USB|RIG_MODE_LSB|RIG_MODE_RTTY|RIG_MODE_FM)
#define FT100_SSB_CW_RX_MODES (RIG_MODE_CW|RIG_MODE_USB|RIG_MODE_LSB)
#define FT100_AM_FM_RX_MODES (RIG_MODE_AM|RIG_MODE_FM)

#define FT100_OTHER_TX_MODES (RIG_MODE_AM|RIG_MODE_CW|RIG_MODE_USB|RIG_MODE_LSB|RIG_MODE_RTTY|RIG_MODE_FM)
#define FT100_AM_TX_MODES (RIG_MODE_AM)
#define FT100_GET_RIG_LEVELS (RIG_LEVEL_STRENGTH|RIG_LEVEL_RFPOWER)
#define FT100_FUNC_ALL (RIG_FUNC_LOCK|RIG_FUNC_TONE|RIG_FUNC_TSQL)

const struct rig_caps ft100_caps = {
  .rig_model = 		RIG_MODEL_FT100,
  .model_name = 		"FT-100", 
  .mfg_name = 		"Yaesu", 
  .version = 		"0.1", 
  .copyright = 		"LGPL",
  .status = 		RIG_STATUS_ALPHA,
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

  .chan_list =  { RIG_CHAN_END, },	/* FIXME: memory chan .list =  78 */

  .rx_range_list1 =  { RIG_FRNG_END, },    /* FIXME: enter region 1 setting */
  .tx_range_list1 =  { RIG_FRNG_END, },
  .rx_range_list2 =  { 
    {kHz(100),MHz(56), FT100_ALL_RX_MODES,-1,-1},
    {MHz(76), MHz(108),RIG_MODE_WFM,      -1,-1},
    {MHz(108),MHz(154),FT100_ALL_RX_MODES,-1,-1},
    {MHz(420),MHz(470),FT100_ALL_RX_MODES,-1,-1},
    RIG_FRNG_END, 
  },

  .tx_range_list2 =  {
    {MHz(1.8),   MHz(2),      FT100_OTHER_TX_MODES, W(0.5),W(0.5)},
    {MHz(1.8),   MHz(2),      FT100_AM_TX_MODES,    W(0.5),W(1.5)},
    {MHz(3.5),   MHz(4),      FT100_OTHER_TX_MODES, W(0.5),W(5.0)},
    {MHz(3.5),   MHz(4),      FT100_AM_TX_MODES,    W(0.5),W(1.5)},
    {MHz(7),     MHz(7.3),    FT100_OTHER_TX_MODES, W(0.5),W(5.0)},
    {MHz(7),     MHz(7.3),    FT100_AM_TX_MODES,    W(0.5),W(1.5)},
    {MHz(10),    MHz(10.150), FT100_OTHER_TX_MODES, W(0.5),W(5.0)},
    {MHz(10),    MHz(10.150), FT100_AM_TX_MODES,    W(0.5),W(1.5)},
    {MHz(14),    MHz(14.350), FT100_OTHER_TX_MODES, W(0.5),W(5.0)},
    {MHz(14),    MHz(14.350), FT100_AM_TX_MODES,    W(0.5),W(1.5)},
    {MHz(18.068),MHz(18.168), FT100_OTHER_TX_MODES, W(0.5),W(5.0)},
    {MHz(18.068),MHz(18.168), FT100_AM_TX_MODES,    W(0.5),W(1.5)},
    {MHz(21),    MHz(21.450), FT100_OTHER_TX_MODES, W(0.5),W(5.0)},
    {MHz(21),    MHz(21.450), FT100_AM_TX_MODES,    W(0.5),W(1.5)},
    {MHz(24.890),MHz(24.990), FT100_OTHER_TX_MODES, W(0.5),W(5.0)},
    {MHz(24.890),MHz(24.990), FT100_AM_TX_MODES,    W(0.5),W(1.5)},
    {MHz(28),    MHz(29.7),   FT100_OTHER_TX_MODES, W(0.5),W(5.0)},
    {MHz(28),    MHz(29.7),   FT100_AM_TX_MODES,    W(0.5),W(1.5)},
    {MHz(50),    MHz(54),     FT100_OTHER_TX_MODES, W(0.5),W(5.0)},
    {MHz(50),    MHz(54),     FT100_AM_TX_MODES,    W(0.5),W(1.5)},
    {MHz(144),   MHz(148),    FT100_OTHER_TX_MODES, W(0.5),W(5.0)}, 
    {MHz(144),   MHz(148),    FT100_AM_TX_MODES,    W(0.5),W(1.5)}, 
    {MHz(430),   MHz(440),    FT100_OTHER_TX_MODES, W(0.5),W(5.0)},
    {MHz(430),   MHz(440),    FT100_AM_TX_MODES,    W(0.5),W(1.5)},
    RIG_FRNG_END, 
  },

  .tuning_steps =  {
    {FT100_SSB_CW_RX_MODES,10},
    {FT100_SSB_CW_RX_MODES,100},
    {FT100_AM_FM_RX_MODES,10},
    {FT100_AM_FM_RX_MODES,100},
    RIG_TS_END,
  },  

  .filters =  {
    RIG_FLT_END,
  },

  .priv = 			NULL,
  .rig_init = 		ft100_init, 
  .rig_cleanup = 		ft100_cleanup, 
  .rig_open = 		ft100_open, 
  .rig_close = 		ft100_close, 
  .set_freq = 		ft100_set_freq,
  .get_freq = 		ft100_get_freq,
  .set_mode = 		ft100_set_mode,
  .get_mode = 		NULL,
  .set_vfo = 		NULL,
  .get_vfo = 		NULL,
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
  .set_split = 		ft100_set_split,
  .get_split = 		NULL,
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
  .get_level = 		NULL,
  .set_func = 		NULL,
  .get_func = 		NULL,
  .set_parm = 		NULL,
  .get_parm = 		NULL,
}; 


int init_ft100(void *be_handle) {
  rig_debug(RIG_DEBUG_VERBOSE, "ft100: _init called\n");

  rig_register(&ft100_caps);
  
  return RIG_OK;
}


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

  p->current_vfo = RIG_VFO1;	/* no clue which VFO is active, so guess VFO 1 */
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
    rig_debug(RIG_DEBUG_VERBOSE,"ft100: Attempt to send incomplete sequence \n");
    return -RIG_EINVAL;
  }
  
  cmd = (unsigned char *) p->pcs[cmd_index].nseq; /* get native sequence */

  rig_debug(RIG_DEBUG_VERBOSE,"ft100: Attempt to send sequence ="); 
  for (i=0; i < YAESU_CMD_LENGTH; i++)
      rig_debug(RIG_DEBUG_VERBOSE," %3i",(int)cmd[i]);
  rig_debug(RIG_DEBUG_VERBOSE," \n");
   
  write_block(&rig_s->rigport, cmd, YAESU_CMD_LENGTH);
    
  return RIG_OK;
}


int ft100_set_freq(RIG *rig, vfo_t vfo, freq_t freq) {
  struct rig_state *rig_s;
  struct ft100_priv_data *p;
  unsigned char *cmd;		/* points to sequence to send */
  unsigned char cmd_index;	/* index of sequence to send */

  if (!rig)  return -RIG_EINVAL;

  p = (struct ft100_priv_data*)rig->state.priv;

  rig_s = &rig->state;

  rig_debug(RIG_DEBUG_VERBOSE,"ft100: requested freq = %lli Hz \n", freq);
  rig_debug(RIG_DEBUG_VERBOSE,"ft100: vfo =%i \n", vfo);

  if( ( vfo != RIG_VFO_CURR ) &&
      ( ft100_set_vfo( rig, vfo ) != RIG_OK ) )  return -RIG_ERJCTED;
 
  switch( vfo ) {
  case RIG_VFO_CURR:
  case RIG_VFO1:
  case RIG_VFO2:
    cmd_index = FT100_NATIVE_CAT_SET_FREQ;
    break;
  default:
    rig_debug(RIG_DEBUG_VERBOSE,"ft100: Unknown VFO \n");
    return -RIG_EINVAL;
  }

  memcpy(p->p_cmd,&ncmd[cmd_index].nseq,YAESU_CMD_LENGTH);

  to_bcd(p->p_cmd,freq,8);	/* store bcd format in in p_cmd */
				/* TODO -- fix 10Hz resolution -- FS */

  rig_debug(RIG_DEBUG_VERBOSE,"ft100: requested freq after conversion = %lli Hz \n", from_bcd_be(p->p_cmd,8)* 10 );

  cmd = p->p_cmd; /* get native sequence */
  write_block(&rig_s->rigport, cmd, YAESU_CMD_LENGTH);

  return RIG_OK;
}

int ft100_get_freq(RIG *rig, vfo_t vfo, freq_t *freq) {
  
   FT100_STATUS_INFO ft100_status;
   FT100_METER_INFO  ft100_meters;
   FT100_FLAG_INFO   ft100_flags;
  
   freq_t d1, d2;
   char freq_str[6], sfreq[10];  
   int i;
    
   int n = 0;
    
   rig_debug(RIG_DEBUG_VERBOSE,"ft100: get_freq \n");
   
   if( !rig )  return -RIG_EINVAL;
   if( !freq )  return -RIG_EINVAL;
  
   serial_flush( &rig->state.rigport );
   n=ft100_get_info(rig, &ft100_status, &ft100_meters, &ft100_flags);
   rig_debug(RIG_DEBUG_VERBOSE,"ft100: Freq= %3i %3i %3i %3i \n",(int)ft100_status.freq[0], (int)ft100_status.freq[1], (int)ft100_status.freq[2],(int)ft100_status.freq[3]);
  
   /* now convert it .... */
   /* yes i know its butt fugly, but it works */
   
   
   for (i=0; i<5; i++ )
      freq_str[i]=0x00;

   for (i=0; i<4; i++)
        strcat(freq_str, CFREQ_TBL[(int)ft100_status.freq[i]]);
   
   d1=strtol(freq_str,NULL,16);
   d2=(d1*1.25)/10;
   
   rig_debug(RIG_DEBUG_VERBOSE,"ft100: d1=%lld d2=%lld\n",d1,d2);
   
   sprintf(sfreq,"%8lli",d2);
   
   rig_debug(RIG_DEBUG_VERBOSE,"ft100: get_freq= %s \n",sfreq);
   
   memcpy(freq, &d2, sizeof(freq_t));
   
   return RIG_OK;
}





int ft100_set_mode(RIG *rig, vfo_t vfo, rmode_t mode, pbwidth_t width) {
  unsigned char cmd_index;	/* index of sequence to send */

  rig_debug(RIG_DEBUG_VERBOSE,"ft100: generic mode = %x \n", mode);

  switch(mode) {
  case RIG_MODE_AM:
    cmd_index = FT100_NATIVE_CAT_SET_MODE_AM;
    break;
  case RIG_MODE_CW:
    cmd_index = FT100_NATIVE_CAT_SET_MODE_CW;
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
  default:
    return -RIG_EINVAL;
  }

  switch(width) {
  case RIG_PASSBAND_NORMAL:
    return ft100_send_priv_cmd(rig,cmd_index);
  default:
    return -RIG_EINVAL;    
  }

  return ft100_send_priv_cmd(rig,cmd_index);
}

int ft100_get_mode(RIG *rig, vfo_t vfo, rmode_t *mode, pbwidth_t *width) {

  int n = 0;
  unsigned char data[ YAESU_CMD_LENGTH ];

  if( !rig )  return -RIG_EINVAL;
  if( !mode )  return -RIG_EINVAL;
  if( !width )  return -RIG_EINVAL;

  serial_flush( &rig->state.rigport );
  ft100_send_priv_cmd( rig, FT100_NATIVE_CAT_GET_FREQ_MODE_STATUS );
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


int ft100_set_vfo(RIG *rig, vfo_t vfo) {

  struct ft100_priv_data *p = (struct ft100_priv_data*)rig->state.priv;

  if (!rig)  return -RIG_EINVAL;

  switch(vfo) {
  case RIG_VFO1:
  case RIG_VFO2:
    if( p->current_vfo != vfo ) {
      if( ft100_send_priv_cmd( rig, FT100_NATIVE_CAT_SET_VFOAB ) == RIG_OK ) {
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



int ft100_get_vfo(RIG *rig, vfo_t *vfo) {

  if( !rig )  return -RIG_EINVAL;
  if( !vfo )  return -RIG_EINVAL;
  
  /* No cmd to get vfo, return last-known value */
  *vfo = ((struct ft100_priv_data*)rig->state.priv)->current_vfo;

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


/* TODO: all of this */
int ft100_get_ptt(RIG *rig, vfo_t vfo, ptt_t *ptt) {
  return -RIG_ENIMPL;
}


/* TODO: all of this */
int ft100_set_level(RIG *rig, vfo_t vfo, setting_t level, value_t val) {
   return -RIG_ENIMPL;
}


/* TODO: all of this */
int ft100_get_level(RIG *rig, vfo_t vfo, setting_t level, value_t *val) {

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


/* kc2ivl */
int ft100_set_split(RIG *rig, vfo_t vfo, split_t split) {

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
  case RIG_VFO1:
  case RIG_VFO2:
    cmd_index = FT100_NATIVE_CAT_SET_DCS_CODE;
    break;
  default:
    rig_debug(RIG_DEBUG_VERBOSE,"ft100: Unknown VFO \n");
    return -RIG_EINVAL;
  }

  cmd = p->p_cmd; /* get native sequence */
  memcpy(cmd,&ncmd[cmd_index].nseq,YAESU_CMD_LENGTH);
  
  cmd[3]=(char)code;
  
  write_block(&rig_s->rigport, cmd, YAESU_CMD_LENGTH);

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
  case RIG_VFO1:
  case RIG_VFO2:
    cmd_index = FT100_NATIVE_CAT_SET_CTCSS_FREQ;
    break;
  default:
    rig_debug(RIG_DEBUG_VERBOSE,"ft100: Unknown VFO \n");
    return -RIG_EINVAL;
  }

  cmd = p->p_cmd; /* get native sequence */
  memcpy(cmd,&ncmd[cmd_index].nseq,YAESU_CMD_LENGTH);
  
  cmd[3]=(char)tone;
  
  write_block(&rig_s->rigport, cmd, YAESU_CMD_LENGTH);

  return RIG_OK;
}


/*
 * okie dokie here....
 * get everything, let the calling function figure out what it needs
 * 
 */

int ft100_get_info(RIG *rig, FT100_STATUS_INFO *ft100_status, FT100_METER_INFO *ft100_meter, FT100_FLAG_INFO *ft100_flags)
{
   unsigned char cmd_index;
   int n;
   
   rig_debug(RIG_DEBUG_VERBOSE,"ft100: getting all info \n");
   
   cmd_index=FT100_NATIVE_CAT_READ_STATUS;
   ft100_send_priv_cmd(rig,cmd_index);
   n = read_block( &rig->state.rigport, ft100_status, sizeof(FT100_STATUS_INFO));  
   rig_debug(RIG_DEBUG_VERBOSE,"ft100: read status=%i \n",n);
   
   cmd_index=FT100_NATIVE_CAT_READ_METERS;
   ft100_send_priv_cmd(rig,cmd_index);
   n = read_block( &rig->state.rigport, ft100_meter, sizeof(FT100_METER_INFO));  
   rig_debug(RIG_DEBUG_VERBOSE,"ft100: read meters=%i \n",n);
   
   
   return RIG_OK;
}


