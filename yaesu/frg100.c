/*
 * frg100.c - (C) Stephane Fillod 2002-2005
 *
 * This shared library provides an API for communicating
 * via serial interface to an FRG-100 using the "CAT" interface
 *
 *	$Id: frg100.c,v 1.4 2005-02-26 22:30:55 fillods Exp $
 *
 *   This library is free software; you can redistribute it and/or modify
 *   it under the terms of the GNU Library General Public License as
 *   published by the Free Software Foundation; either version 2 of
 *   the License, or (at your option) any later version.
 *
 *   This program is distributed in the hope that it will be useful,
 *   but WITHOUT ANY WARRANTY; without even the implied warranty of
 *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *   GNU Library General Public License for more details.
 *
 *   You should have received a copy of the GNU Library General Public
 *   License along with this library; if not, write to the Free Software
 *   Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
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



#define FRG100_MODES (RIG_MODE_CW|RIG_MODE_SSB|RIG_MODE_AM|RIG_MODE_FM)

#define FRG100_VFOS (RIG_VFO_A)
#define FRG100_ANTS 0


/* TODO: get real measure numbers */
#define FRG100_STR_CAL { 2, { \
		{  0, -60 }, /* S0 -6dB */ \
		{ 60,  60 }  /* +60 */ \
		} }

#define FRG100_MEM_CAP { \
                        .freq = 1,      \
                        .mode = 1,      \
                        .width = 1     \
	                }

/* Private helper function prototypes */

static int frg100_open(RIG *rig);

static int frg100_set_freq(RIG *rig, vfo_t vfo, freq_t freq);
static int frg100_set_mode(RIG *rig, vfo_t vfo, rmode_t mode, pbwidth_t width);
static int frg100_set_vfo(RIG *rig, vfo_t vfo);
static int frg100_set_powerstat(RIG *rig, powerstat_t status);
static int frg100_get_level(RIG *rig, vfo_t vfo, setting_t level, value_t *val);

static int rig2mode(RIG *rig, int md, rmode_t *mode, pbwidth_t *width);
static int mode2rig(RIG *rig, rmode_t mode, pbwidth_t width);

/*
 * frg100 rigs capabilities.
 * Also this struct is READONLY!
 *
 * TODO:
 * 	- Memory recall
 * 	- VFO->M & M->VFO
 * 	- Lock, Up/Down
 * 	- Status update
 * 	- Clock set, Timer
 * 	- Scan skip
 * 	- Step Frequency Size
 * 	- Dim
 */

const struct rig_caps frg100_caps = {
  .rig_model =          RIG_MODEL_FRG100,
  .model_name =         "FRG-100",
  .mfg_name =           "Yaesu",
  .version =            "0.3",
  .copyright =          "LGPL",
  .status =             RIG_STATUS_BETA,
  .rig_type =           RIG_TYPE_RECEIVER,
  .ptt_type =           RIG_PTT_NONE,
  .dcd_type =           RIG_DCD_NONE,
  .port_type =          RIG_PORT_SERIAL,
  .serial_rate_min =    4800,
  .serial_rate_max =    4800,
  .serial_data_bits =   8,
  .serial_stop_bits =   2,
  .serial_parity =      RIG_PARITY_NONE,
  .serial_handshake =   RIG_HANDSHAKE_NONE,
  .write_delay =        20,
  .post_write_delay =   300,
  .timeout =            2000,
  .retry =              0,
  .has_get_func =       RIG_FUNC_LOCK,
  .has_set_func =       RIG_FUNC_LOCK,
  .has_get_level =      RIG_LEVEL_RAWSTR,
  .has_set_level =      RIG_LEVEL_NONE,
  .has_get_parm =       RIG_PARM_NONE,
  .has_set_parm =       RIG_PARM_BACKLIGHT,
  .vfo_ops =		RIG_OP_FROM_VFO|RIG_OP_TO_VFO|RIG_OP_UP|RIG_OP_DOWN,
  .preamp =             { RIG_DBLST_END, },
  .attenuator =         { RIG_DBLST_END, },
  .max_rit =            Hz(0),
  .max_xit =            Hz(0),
  .max_ifshift =        Hz(0),
  .targetable_vfo =     RIG_TARGETABLE_NONE,
  .transceive =         RIG_TRN_OFF,
  .bank_qty =           0,
  .chan_desc_sz =       0,
  .chan_list =          { 
	  {      1,  0x32, RIG_MTYPE_MEM, FRG100_MEM_CAP },
	  {   0x33,  0x34, RIG_MTYPE_EDGE },
			},
  .rx_range_list1 =     { 
    {kHz(50), MHz(30), FRG100_MODES, -1, -1, FRG100_VFOS, FRG100_ANTS },
    RIG_FRNG_END,
  }, /* Region 1 rx ranges */

  .tx_range_list1 =     {
	RIG_FRNG_END,
  },    /* region 1 TX ranges */

  .rx_range_list2 =     {
    {kHz(50), MHz(30), FRG100_MODES, -1, -1, FRG100_VFOS, FRG100_ANTS },
    RIG_FRNG_END,
  }, /* Region 2 rx ranges */

  .tx_range_list2 =     {
	RIG_FRNG_END,
  },    /* region 2 TX ranges */

  .tuning_steps =       {
    {RIG_MODE_SSB|RIG_MODE_CW, Hz(10)},
    {RIG_MODE_FM|RIG_MODE_AM, Hz(100)},
    RIG_TS_END,
  },

    /* mode/filter list, remember: order matters! */
  .filters =            {
    {RIG_MODE_SSB|RIG_MODE_CW,  kHz(2.4)},
    {RIG_MODE_CW,   Hz(500)},
    {RIG_MODE_AM,   kHz(6)},
    {RIG_MODE_AM,   kHz(4)},
    {RIG_MODE_FM,   kHz(15)},
    RIG_FLT_END,
  },

  .str_cal = FRG100_STR_CAL,

  .rig_open =		frg100_open,

  .set_freq =           frg100_set_freq,
  .set_mode =           frg100_set_mode,
  .set_vfo =            frg100_set_vfo,
  .get_level =		frg100_get_level,

  .set_powerstat =	frg100_set_powerstat,
};


/*
 * frg100_open  routine
 * 
 */
int frg100_open(RIG *rig)
{
  unsigned char cmd[YAESU_CMD_LENGTH] = { 0x00, 0x00, 0x00, 0x00, 0x0e};
 
  rig_debug(RIG_DEBUG_TRACE, "frg100: frg100_open called\n");

  /* send 0 delay pacing */
  return write_block(&rig->state.rigport, cmd, YAESU_CMD_LENGTH);

}


int frg100_set_freq(RIG *rig, vfo_t vfo, freq_t freq)
{
  unsigned char cmd[YAESU_CMD_LENGTH] = { 0x00, 0x00, 0x00, 0x00, 0x0a};

   /* store bcd format in cmd (LSB) */
  to_bcd(cmd, freq/10, 8);

  /* Frequency set */
  return write_block(&rig->state.rigport, cmd, YAESU_CMD_LENGTH);
}


int frg100_set_mode(RIG *rig, vfo_t vfo, rmode_t mode, pbwidth_t width)
{
  unsigned char cmd[YAESU_CMD_LENGTH] = { 0x00, 0x00, 0x00, 0x00, 0x0c};


  /* fill in p1 */
  cmd[3] = mode2rig(rig, mode, width);

  return write_block(&rig->state.rigport, cmd, YAESU_CMD_LENGTH);
}


/*
 * Actually, this is tape relay, 0xfe=on
 */
int frg100_set_powerstat(RIG *rig, powerstat_t status)
{
  unsigned char cmd[YAESU_CMD_LENGTH] = { 0x00, 0x00, 0x00, 0x00, 0x20};

  rig_debug(RIG_DEBUG_TRACE,"frg100: frg100_set_powerstat called\n");

  cmd[3] = status == RIG_POWER_OFF ? 0x00 : 0x01;

  /* Frequency set */
  return write_block(&rig->state.rigport, cmd, YAESU_CMD_LENGTH);
}


int frg100_set_vfo(RIG *rig, vfo_t vfo)
{
  unsigned char cmd[YAESU_CMD_LENGTH] = { 0x00, 0x00, 0x00, 0x00, 0x00};


  switch(vfo) {
  case RIG_VFO_CURR:
	  return RIG_OK;
  case RIG_VFO_VFO:
  case RIG_VFO_A:
	cmd[4] = 0x05;
	break;
  case RIG_VFO_MEM:
	/* TODO: cmd[3] = priv->current_mem_chan; */
	cmd[4] = 0x02;
	break;
  default:
    return -RIG_EINVAL;		/* sorry, wrong VFO */
  }

  return write_block(&rig->state.rigport, cmd, YAESU_CMD_LENGTH);
}


int frg100_get_level(RIG *rig, vfo_t vfo, setting_t level, value_t *val)
{
  unsigned char cmd[YAESU_CMD_LENGTH] = { 0x00, 0x00, 0x00, 0x00, 0xf7};
  int retval;
 
  if (level != RIG_LEVEL_RAWSTR)
	  return -RIG_EINVAL;

  serial_flush(&rig->state.rigport);

  /* send READ STATUS(Meter only) cmd to rig  */
  retval = write_block(&rig->state.rigport, cmd, YAESU_CMD_LENGTH);
  if (retval < 0)
	  return retval;

  /* read back the 1 byte */
  retval = read_block(&rig->state.rigport, cmd, 5);

  if (retval < 1) {
	rig_debug(RIG_DEBUG_ERR,"%s: read meter failed %d\n", 
			__FUNCTION__,retval);

	return retval < 0 ? retval : -RIG_EIO;
  }
  val->i = cmd[0];

  return RIG_OK;
}


#define MODE_LSB	0x00
#define MODE_USB	0x01
#define MODE_CWW	0x02
#define MODE_CWN	0x03
#define MODE_AMW	0x04
#define MODE_AMN	0x05
#define MODE_FMW	0x06
#define MODE_FMN	0x07


int mode2rig(RIG *rig, rmode_t mode, pbwidth_t width)
{
  int md;

  /* 
   * translate mode from generic to frg100 specific
   */
  switch(mode) {
  case RIG_MODE_USB:	md = MODE_USB; break;
  case RIG_MODE_LSB:	md = MODE_LSB; break;
  case RIG_MODE_AM:
  	if (width != RIG_PASSBAND_NORMAL ||
		  width < rig_passband_normal(rig, mode))
		md = MODE_AMN;
	else
		md = MODE_AMW;
	break;
  case RIG_MODE_FM:
  	if (width != RIG_PASSBAND_NORMAL ||
		  width < rig_passband_normal(rig, mode))
		md = MODE_FMN;
	else
		md = MODE_FMW;
	break;
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
   * translate mode from frg100 specific to generic 
   */
  switch(md) {
  case MODE_USB:	*mode = RIG_MODE_USB; break;
  case MODE_LSB:	*mode = RIG_MODE_LSB; break;
  case MODE_AMW:
  case MODE_AMN:	*mode = RIG_MODE_AM; break;
  case MODE_FMW:
  case MODE_FMN:	*mode = RIG_MODE_FM; break;
  case MODE_CWW:
  case MODE_CWN:	*mode = RIG_MODE_CW; break;
  default:
    return -RIG_EINVAL;         /* sorry, wrong MODE */
  }
  if (md == MODE_CWN || md == MODE_AMN || md == MODE_FMN)
	*width = rig_passband_narrow(rig, *mode);
  else
	*width = rig_passband_normal(rig, *mode);

  return RIG_OK;
}

