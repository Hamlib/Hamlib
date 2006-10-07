/*
 * hamlib - (C) Frank Singleton 2000 (vk3fcs@ix.netcom.com)
 *
 * ft757gx.c - (C) Stephane Fillod 2004
 * This shared library provides an API for communicating
 * via serial interface to an FT-757GX using the "CAT" interface
 * box (FIF-232C) or similar
 *
 *
 * $Id: ft757gx.c,v 1.4 2006-10-07 15:51:38 csete Exp $  
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
 * TODO
 *
 * 1. Allow cached reads
 * 2. set_mem/get_mem, vfo_op, get_channel, set_split/get_split,
 * 	set_func/get_func
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


static int ft757_init(RIG *rig);
static int ft757_cleanup(RIG *rig);
static int ft757_open(RIG *rig);

static int ft757_set_freq(RIG *rig, vfo_t vfo, freq_t freq);
static int ft757_get_freq(RIG *rig, vfo_t vfo, freq_t *freq);

static int ft757_set_mode(RIG *rig, vfo_t vfo, rmode_t mode, pbwidth_t width); /* select mode */
static int ft757_get_mode(RIG *rig, vfo_t vfo, rmode_t *mode, pbwidth_t *width); /* get mode */

static int ft757_set_vfo(RIG *rig, vfo_t vfo); /* select vfo */
static int ft757_get_vfo(RIG *rig, vfo_t *vfo); /* get vfo */

static int ft757_get_ptt(RIG *rig, vfo_t vfo, ptt_t *ptt);
static int ft757_get_level(RIG *rig, vfo_t vfo, setting_t level, value_t *val);



/* Private helper function prototypes */

static int ft757_get_update_data(RIG *rig);
static int mode2rig(RIG *rig, rmode_t mode, pbwidth_t width);
static int rig2mode(RIG *rig, int md, rmode_t *mode, pbwidth_t *width);


/*
 * Some useful offsets in the status update map (offset)
 *
 * Status Update Chart, FT757GXII
 */
#define STATUS_CURR_FREQ	6	/* Operating Frequency */
#define STATUS_CURR_MODE	10
#define STATUS_VFOA_FREQ	11
#define STATUS_VFOA_MODE	15
#define STATUS_VFOB_FREQ	16
#define STATUS_VFOB_MODE	20


#define MODE_LSB	0x0
#define MODE_USB	0x1
#define MODE_CWW	0x2
#define MODE_CWN	0x3
#define MODE_AM 	0x4
#define MODE_FM 	0x5

/* 
 * Receiver caps 
 */

#define FT757GX_ALL_RX_MODES (RIG_MODE_AM|RIG_MODE_CW|RIG_MODE_SSB|RIG_MODE_FM)


/* 
 * TX caps
 */ 

#define FT757GX_ALL_TX_MODES (RIG_MODE_AM|RIG_MODE_CW|RIG_MODE_SSB|RIG_MODE_FM)


/*
 * future - private data
 *
 */

struct ft757_priv_data {
  unsigned char pacing;		/* pacing value */
  unsigned int read_update_delay;	 /* depends on pacing value */
  unsigned char current_vfo;	/* active VFO from last cmd , can be either RIG_VFO_A or RIG_VFO_B only */
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
  .version =           "0.3",
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

  .rig_init =   ft757_init, 
  .rig_cleanup =    ft757_cleanup, 
  .rig_open =   ft757_open,				/* port opened */
  .rig_close =  NULL,				/* port closed */

  .set_freq =   ft757_set_freq,		/* set freq */
  .get_freq =   NULL,		/* get freq */
  .set_mode =   NULL,		/* set mode */
  .get_mode =   NULL,		/* get mode */
  .set_vfo =    ft757_set_vfo,		/* set vfo */

  .get_vfo =    NULL,		/* get vfo */
  .set_ptt =    NULL,		/* set ptt */
  .get_ptt =    NULL,		/* get ptt */
};

/* TODO: get better measure numbers */
#define FT757GXII_STR_CAL { 2, { \
		{  0, -60 }, /* S0 -6dB */ \
		{ 15,  60 }  /* +60 */ \
		} }

#define FT757_MEM_CAP {	\
	                .freq = 1,      \
	                .mode = 1,      \
	                .width = 1     \
	        }

const struct rig_caps ft757gx2_caps = {
  .rig_model =        RIG_MODEL_FT757GXII, 
  .model_name =       "FT-757GXII",
  .mfg_name =         "Yaesu", 
  .version =           "0.2",
  .copyright =         "LGPL",
  .status =            RIG_STATUS_ALPHA, 
  .rig_type =          RIG_TYPE_MOBILE, 
  .ptt_type =          RIG_PTT_SERIAL_DTR,	/* pin4? */
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
  .has_get_func =      RIG_FUNC_LOCK,
  .has_set_func =      RIG_FUNC_LOCK,
  .has_get_level =     RIG_LEVEL_RAWSTR,
  .has_set_level =     RIG_LEVEL_NONE,
  .has_get_parm =      RIG_PARM_NONE,
  .has_set_parm =      RIG_PARM_NONE,
  .vfo_ops =           RIG_OP_CPY | RIG_OP_FROM_VFO | RIG_OP_TO_VFO | 
	  			RIG_OP_UP | RIG_OP_DOWN,
  .ctcss_list =        NULL,
  .dcs_list =          NULL,
  .preamp =            { RIG_DBLST_END, },
  .attenuator =        { RIG_DBLST_END, },
  .max_rit =           Hz(0),
  .max_xit =           Hz(0),
  .max_ifshift =       Hz(0),
  .targetable_vfo =    0,
  .transceive =        RIG_TRN_OFF,
  .bank_qty =          0,
  .chan_desc_sz =      0,
  .chan_list =         {
		{   0,  9, RIG_MTYPE_MEM, FT757_MEM_CAP },
		RIG_CHAN_END
			},

  .rx_range_list1 =    { RIG_FRNG_END, },    /* FIXME: enter region 1 setting */

  .tx_range_list1 =    { RIG_FRNG_END, },

  .rx_range_list2 =    { { .start = kHz(150), .end = 29999990, 
			.modes = FT757GX_ALL_RX_MODES,.low_power = -1,.high_power = -1}, 
		      RIG_FRNG_END, }, /* rx range */

  /* FIXME: 10m is "less" and AM is 25W carrier */
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
    RIG_TS_END,
  },

      /* mode/filter list, .remember =  order matters! */
  .filters =  {
	{RIG_MODE_SSB|RIG_MODE_CW,  kHz(2.7)},
	{RIG_MODE_CW,  Hz(600)},	/* narrow */
	{RIG_MODE_AM,  kHz(6)},
	{RIG_MODE_FM,  kHz(15)},

    RIG_FLT_END,
  },

  .str_cal = FT757GXII_STR_CAL,

  .priv =   NULL, /* private data */

  .rig_init =   ft757_init, 
  .rig_cleanup =    ft757_cleanup, 
  .rig_open =   ft757_open,			/* port opened */
  .rig_close =  NULL,				/* port closed */

  .set_freq =   ft757_set_freq,		/* set freq */
  .get_freq =   ft757_get_freq,		/* get freq */
  .set_mode =   ft757_set_mode,		/* set mode */
  .get_mode =   ft757_get_mode,		/* get mode */
  .set_vfo =    ft757_set_vfo,		/* set vfo */
  .get_vfo =    ft757_get_vfo,	/* get vfo */
  .get_level =    ft757_get_level,
  .get_ptt =    ft757_get_ptt,	/* get ptt */
};



/*
 * _init 
 *
 */


int ft757_init(RIG *rig) {
  struct ft757_priv_data *p;
  
  if (!rig)
    return -RIG_EINVAL;
  
  p = (struct ft757_priv_data*)malloc(sizeof(struct ft757_priv_data));
  if (!p)			/* whoops! memory shortage! */    
    return -RIG_ENOMEM;
  
  rig_debug(RIG_DEBUG_VERBOSE,"%s called\n", __FUNCTION__);

  /* TODO: read pacing from preferences */

  p->pacing = FT757GX_PACING_DEFAULT_VALUE; /* set pacing to minimum for now */
  p->read_update_delay = FT757GX_DEFAULT_READ_TIMEOUT; /* set update timeout to safe value */
  p->current_vfo =  RIG_VFO_A;	/* default to VFO_A ? */
  rig->state.priv = (void*)p;
  
  return RIG_OK;
}


/*
 * ft757_cleanup routine
 * the serial port is closed by the frontend
 */

int ft757_cleanup(RIG *rig) {
  if (!rig)
    return -RIG_EINVAL;
  
  rig_debug(RIG_DEBUG_VERBOSE, "%s called\n", __FUNCTION__);

  if (rig->state.priv)
    free(rig->state.priv);
  rig->state.priv = NULL;
  
  return RIG_OK;
}

/*
 * ft757_open  routine
 * 
 */

int ft757_open(RIG *rig)
{
  unsigned char cmd[YAESU_CMD_LENGTH] = { 0x00, 0x00, 0x00, 0x00, 0x0e};
  struct ft757_priv_data *priv = (struct ft757_priv_data*)rig->state.priv;
  int retval;
 
  serial_flush(&rig->state.rigport);

   /* send 0 delay PACING cmd to rig  */
  write_block(&rig->state.rigport, (char *) cmd, YAESU_CMD_LENGTH);

  /* read back the 75 status bytes */
  retval = read_block(&rig->state.rigport,
                      (char *) priv->update_data,
                      FT757GX_STATUS_UPDATE_DATA_LENGTH);

  if (retval != FT757GX_STATUS_UPDATE_DATA_LENGTH) {

	rig_debug(RIG_DEBUG_ERR,"%s: update_data failed %d\n",
			__FUNCTION__, retval);
	memset(priv->update_data,0,FT757GX_STATUS_UPDATE_DATA_LENGTH);
  }

  return RIG_OK;
}


/*
 * This command only functions when operating on a vfo.
 * TODO: test Status Update Byte 1
 *
 */

int ft757_set_freq(RIG *rig, vfo_t vfo, freq_t freq)
{
  unsigned char cmd[YAESU_CMD_LENGTH] = { 0x00, 0x00, 0x00, 0x00, 0x0a};


  /* fill in first four bytes */
  to_bcd(cmd, freq/10, 8);

  return write_block(&rig->state.rigport, (char *) cmd, YAESU_CMD_LENGTH);
}

int ft757_set_mode(RIG *rig, vfo_t vfo, rmode_t mode, pbwidth_t width)
{
  unsigned char cmd[YAESU_CMD_LENGTH] = { 0x00, 0x00, 0x00, 0x00, 0x0c};


  /* fill in p1 */
  cmd[3] = mode2rig(rig, mode, width);

  return write_block(&rig->state.rigport, (char *) cmd, YAESU_CMD_LENGTH);
}



/*
 * Return Freq
 */

int ft757_get_freq(RIG *rig, vfo_t vfo, freq_t *freq)
{
  struct ft757_priv_data *priv = (struct ft757_priv_data*)rig->state.priv;
  int retval;

  retval = ft757_get_update_data(rig);	/* get whole shebang from rig */
  if (retval < 0)
	  return retval;

  /* grab freq and convert */

  switch(vfo) {
  case RIG_VFO_CURR:
    *freq = from_bcd_be(priv->update_data+STATUS_CURR_FREQ, 8);
    break;
  case RIG_VFO_A:
    *freq = from_bcd_be(priv->update_data+STATUS_VFOA_FREQ, 8);
    break;
  case RIG_VFO_B:
    *freq = from_bcd_be(priv->update_data+STATUS_VFOB_FREQ, 8);
  break;
  default:
    return -RIG_EINVAL;		/* sorry, wrong VFO */
  }

  return RIG_OK;
}



int ft757_get_mode(RIG *rig, vfo_t vfo, rmode_t *mode, pbwidth_t *width)
{
  struct ft757_priv_data *priv = (struct ft757_priv_data*)rig->state.priv;
  int retval;

  retval = ft757_get_update_data(rig);	/* get whole shebang from rig */
  if (retval < 0)
	  return retval;

  switch(vfo) {
  case RIG_VFO_CURR:
    retval = rig2mode(rig, priv->update_data[STATUS_CURR_MODE], mode, width);
    break;
  case RIG_VFO_A:
    retval = rig2mode(rig, priv->update_data[STATUS_VFOA_MODE], mode, width);
    break;
  case RIG_VFO_B:
    retval = rig2mode(rig, priv->update_data[STATUS_VFOB_MODE], mode, width);
  break;
  default:
    return -RIG_EINVAL;		/* sorry, wrong VFO */
  }

  return retval;
}


/*
 * set vfo and store requested vfo for later RIG_VFO_CURR
 * requests.
 *
 */
int ft757_set_vfo(RIG *rig, vfo_t vfo) {
  unsigned char cmd[YAESU_CMD_LENGTH] = { 0x00, 0x00, 0x00, 0x00, 0x05};
  struct ft757_priv_data *priv = (struct ft757_priv_data*)rig->state.priv;


  switch(vfo) {
  case RIG_VFO_CURR:
	  return RIG_OK;
  case RIG_VFO_A:
	cmd[3] = 0x00;
	break;
  case RIG_VFO_B:
	cmd[3] = 0x01;
	break;
  default:
    return -RIG_EINVAL;		/* sorry, wrong VFO */
  }

  priv->current_vfo = vfo;
  
  return write_block(&rig->state.rigport, (char *) cmd, YAESU_CMD_LENGTH);
}


int ft757_get_vfo(RIG *rig, vfo_t *vfo) {
  struct ft757_priv_data *priv = (struct ft757_priv_data*)rig->state.priv;
  int retval;

  retval = ft757_get_update_data(rig);	/* get whole shebang from rig */
  if (retval < 0)
	  return retval;
  
  if (priv->update_data[0] & 0x10)
	return RIG_VFO_MEM;
  else
  	if (priv->update_data[0] & 0x08)
		return RIG_VFO_B;
  	else
		return RIG_VFO_A;

  return RIG_OK;
}

int ft757_get_ptt(RIG *rig, vfo_t vfo, ptt_t *ptt)
{
  struct ft757_priv_data *priv = (struct ft757_priv_data*)rig->state.priv;
  int retval;

  retval = ft757_get_update_data(rig);	/* get whole shebang from rig */
  if (retval < 0)
	  return retval;
  
  *ptt = priv->update_data[0] & 0x20 ?  RIG_PTT_ON : RIG_PTT_OFF;
  return RIG_OK;
}

int ft757_get_level(RIG *rig, vfo_t vfo, setting_t level, value_t *val)
{
  unsigned char cmd[YAESU_CMD_LENGTH] = { 0x00, 0x00, 0x00, 0x01, 0x10};
  int retval;
 
  if (level != RIG_LEVEL_RAWSTR)
	return -RIG_EINVAL;

  serial_flush(&rig->state.rigport);

  /* send READ STATUS(Meter only) cmd to rig  */
  retval = write_block(&rig->state.rigport, (char *) cmd, YAESU_CMD_LENGTH);
  if (retval < 0)
	  return retval;

  /* read back the 1 byte */
  retval = read_block(&rig->state.rigport, (char *) cmd, 1);

  if (retval != 1) {
	rig_debug(RIG_DEBUG_ERR,"%s: read meter failed %d\n", 
			__FUNCTION__,retval);

	return retval < 0 ? retval : -RIG_EIO;
  }
  val->i = cmd[0];

  return RIG_OK;
}


/*
 * private helper function. Retrieves update data from rig.
 * using pacing value and buffer indicated in *priv struct.
 *
 * need to use this when doing ft757_get_* stuff
 */

int ft757_get_update_data(RIG *rig)
{
  unsigned char cmd[YAESU_CMD_LENGTH] = { 0x00, 0x00, 0x00, 0x00, 0x10};
  struct ft757_priv_data *priv = (struct ft757_priv_data*)rig->state.priv;
  int retval;
 
  serial_flush(&rig->state.rigport);

  /* send READ STATUS cmd to rig  */
  retval = write_block(&rig->state.rigport, (char *) cmd, YAESU_CMD_LENGTH);
  if (retval < 0)
	  return retval;

  /* read back the 75 status bytes */
  retval = read_block(&rig->state.rigport,
                      (char *) priv->update_data,
                      FT757GX_STATUS_UPDATE_DATA_LENGTH);

  if (retval != FT757GX_STATUS_UPDATE_DATA_LENGTH) {

	rig_debug(RIG_DEBUG_ERR,"%s: read update_data failed %d\n", 
			__FUNCTION__,retval);

	return retval < 0 ? retval : -RIG_EIO;
  }

  return RIG_OK;
}


int mode2rig(RIG *rig, rmode_t mode, pbwidth_t width)
{
  int md;

  /* 
   * translate mode from generic to ft757 specific
   */
  switch(mode) {
  case RIG_MODE_AM:	md = MODE_AM; break;
  case RIG_MODE_USB:	md = MODE_USB; break;
  case RIG_MODE_LSB:	md = MODE_LSB; break;
  case RIG_MODE_FM:	md = MODE_FM; break;
  case RIG_MODE_CW:	
  	if (width != RIG_PASSBAND_NORMAL ||
		  width < rig_passband_normal(rig, mode))
		md = MODE_CWN;
	else
		md = MODE_CWW;
	break;
  default:
    return -RIG_EINVAL;         /* sorry, wrong MODE */
  }
  return md;
}

int rig2mode(RIG *rig, int md, rmode_t *mode, pbwidth_t *width)
{
  /* 
   * translate mode from ft757 specific to generic 
   */
  switch(md) {
  case MODE_AM:		*mode = RIG_MODE_AM; break;
  case MODE_USB:	*mode = RIG_MODE_USB; break;
  case MODE_LSB:	*mode = RIG_MODE_LSB; break;
  case MODE_FM:		*mode = RIG_MODE_FM; break;
  case MODE_CWW:
  case MODE_CWN:	*mode = RIG_MODE_CW; break;
  default:
    return -RIG_EINVAL;         /* sorry, wrong MODE */
  }
  if (md == MODE_CWN)
	*width = rig_passband_narrow(rig, *mode);
  else
	*width = rig_passband_normal(rig, *mode);

  return RIG_OK;
}

