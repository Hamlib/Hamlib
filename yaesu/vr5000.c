/*
 * vr5000.c - (C) Stephane Fillod 2005
 *
 * This shared library provides an API for communicating
 * via serial interface to an VR-5000 using the "CAT" interface
 *
 *	$Id: vr5000.c,v 1.1 2005-02-26 23:11:32 fillods Exp $
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



#define VR5000_MODES (RIG_MODE_CW|RIG_MODE_SSB|RIG_MODE_AM|RIG_MODE_FM|RIG_MODE_WFM)

#define VR5000_VFOS (RIG_VFO_A|RIG_VFO_B)
#define VR5000_ANTS 0


/* TODO: get real measure numbers */
#define VR5000_STR_CAL { 2, { \
		{  0, -60 }, /* S0 -6dB */ \
		{ 63,  60 }  /* +60 */ \
		} }

/* Private helper function prototypes */

static int vr5000_init(RIG *rig);
static int vr5000_cleanup(RIG *rig);
static int vr5000_open(RIG *rig);
static int vr5000_close(RIG *rig);

static int vr5000_set_freq(RIG *rig, vfo_t vfo, freq_t freq);
static int vr5000_set_mode(RIG *rig, vfo_t vfo, rmode_t mode, pbwidth_t width);
static int vr5000_set_vfo(RIG *rig, vfo_t vfo);
static int vr5000_get_level(RIG *rig, vfo_t vfo, setting_t level, value_t *val);
static int vr5000_get_dcd(RIG *rig, vfo_t vfo, dcd_t *dcd);
static int vr5000_set_ts(RIG *rig, vfo_t vfo, shortfreq_t ts);

static int mode2rig(RIG *rig, rmode_t mode, pbwidth_t width);

/*
 * vr5000 rigs capabilities.
 */

const struct rig_caps vr5000_caps = {
  .rig_model =          RIG_MODEL_VR5000,
  .model_name =         "VR-5000",
  .mfg_name =           "Yaesu",
  .version =            "0.1",
  .copyright =          "LGPL",
  .status =             RIG_STATUS_UNTESTED,
  .rig_type =           RIG_TYPE_RECEIVER,
  .ptt_type =           RIG_PTT_NONE,
  .dcd_type =           RIG_DCD_RIG,
  .port_type =          RIG_PORT_SERIAL,
  .serial_rate_min =    4800,
  .serial_rate_max =    57600,
  .serial_data_bits =   8,
  .serial_stop_bits =   2,
  .serial_parity =      RIG_PARITY_NONE,
  .serial_handshake =   RIG_HANDSHAKE_NONE,
  .write_delay =        0,
  .post_write_delay =   0,
  .timeout =            1000,
  .retry =              0,
  .has_get_func =       RIG_FUNC_NONE,
  .has_set_func =       RIG_FUNC_NONE,
  .has_get_level =      RIG_LEVEL_RAWSTR,
  .has_set_level =      RIG_LEVEL_NONE,
  .has_get_parm =       RIG_PARM_NONE,
  .has_set_parm =       RIG_PARM_NONE,
  .vfo_ops =		RIG_OP_NONE,
  .preamp =             { RIG_DBLST_END, },
  .attenuator =         { RIG_DBLST_END, },
  .max_rit =            Hz(0),
  .max_xit =            Hz(0),
  .max_ifshift =        Hz(0),
  .targetable_vfo =     RIG_TARGETABLE_FREQ|RIG_TARGETABLE_MODE,
  .transceive =         RIG_TRN_OFF,
  .bank_qty =           0,
  .chan_desc_sz =       0,
  .chan_list =          { },
  .rx_range_list1 =     { 
    {kHz(100), GHz(2.6)-20, VR5000_MODES, -1, -1, RIG_VFO_A, VR5000_ANTS },
    {kHz(100), GHz(2.6)-20, RIG_MODE_AM|RIG_MODE_FM, -1, -1, RIG_VFO_B, VR5000_ANTS },
    RIG_FRNG_END,
  }, /* Region 1 rx ranges */

  .tx_range_list1 =     {
	RIG_FRNG_END,
  },    /* region 1 TX ranges */

  .rx_range_list2 =     {
    {kHz(100), GHz(2.6)-20, VR5000_MODES, -1, -1, RIG_VFO_A, VR5000_ANTS },
    {kHz(100), GHz(2.6)-20, RIG_MODE_AM|RIG_MODE_FM, -1, -1, RIG_VFO_B, VR5000_ANTS },
    RIG_FRNG_END,
  }, /* Region 2 rx ranges */

  .tx_range_list2 =     {
	RIG_FRNG_END,
  },    /* region 2 TX ranges */

  .tuning_steps =       {
    {RIG_MODE_SSB|RIG_MODE_CW, Hz(20)},
    {RIG_MODE_SSB|RIG_MODE_CW, Hz(100)},
    {RIG_MODE_SSB|RIG_MODE_CW, Hz(500)},
    {RIG_MODE_AM|RIG_MODE_SSB|RIG_MODE_CW, kHz(1)},
    {RIG_MODE_FM|RIG_MODE_SSB|RIG_MODE_CW|RIG_MODE_AM, kHz(5)},
    {RIG_MODE_FM, kHz(6.25)},
    {RIG_MODE_AM, kHz(9)},
    {RIG_MODE_AM|RIG_MODE_WFM|RIG_MODE_FM, kHz(10)},
    {RIG_MODE_FM, kHz(12.5)},
    {RIG_MODE_AM|RIG_MODE_FM, kHz(20)},
    {RIG_MODE_AM|RIG_MODE_FM, kHz(25)},
    {RIG_MODE_AM|RIG_MODE_WFM|RIG_MODE_FM, kHz(50)},
    {RIG_MODE_AM|RIG_MODE_WFM|RIG_MODE_FM, kHz(100)},
    {RIG_MODE_AM|RIG_MODE_WFM|RIG_MODE_FM, kHz(500)},
    RIG_TS_END,
  },

    /* mode/filter list, remember: order matters! */
  .filters =            {
    {RIG_MODE_AM,   kHz(6)},
    {RIG_MODE_SSB|RIG_MODE_CW|RIG_MODE_AM,  kHz(2.4)},
    {RIG_MODE_AM|RIG_MODE_FM,   kHz(15)},
    {RIG_MODE_WFM,  kHz(230)},
    RIG_FLT_END,
  },

  .str_cal = VR5000_STR_CAL,

  .rig_init =		vr5000_init,
  .rig_cleanup =	vr5000_cleanup,
  .rig_open =		vr5000_open,
  .rig_close =		vr5000_close,

  .set_freq =           vr5000_set_freq,
  .set_mode =           vr5000_set_mode,
  .set_vfo =            vr5000_set_vfo,
  .set_ts =             vr5000_set_ts,
  .get_level =		vr5000_get_level,
  .get_dcd =		vr5000_get_dcd,
};


/*
 * VR-5000 backend needs priv data to handle composite cmds
 */
struct vr5000_priv_data {
	vfo_t curr_vfo;
	unsigned char curr_ts;
	unsigned char curr_mode;
};

int vr5000_init(RIG *rig)
{
  struct vr5000_priv_data *priv;

  priv = (struct vr5000_priv_data*)malloc(sizeof(struct vr5000_priv_data));
  if (!priv)  return -RIG_ENOMEM;

  rig->state.priv = (void*)priv;

  return RIG_OK;
}

int vr5000_cleanup(RIG *rig)
{
  if (!rig)
	return -RIG_EINVAL;

  if (rig->state.priv)
	free(rig->state.priv);
  rig->state.priv = NULL;

  return RIG_OK;
}

/*
 * vr5000_open  routine: CAT ON
 * 
 */
int vr5000_open(RIG *rig)
{
  struct vr5000_priv_data *priv = rig->state.priv;
  unsigned char cmd[YAESU_CMD_LENGTH] = { 0x00, 0x00, 0x00, 0x00, 0x00};
 
  priv->curr_vfo = RIG_VFO_A; /* no clue which VFO is active, so guess VFO 1 */
  priv->curr_ts = 0x03; /* no clue, set step to 1kHz */
  priv->curr_mode = 0x05; /* no clue, set mode to AM */


  /* send 0 delay pacing */
  return write_block(&rig->state.rigport, cmd, YAESU_CMD_LENGTH);

}

int vr5000_close(RIG *rig)
{
  unsigned char cmd[YAESU_CMD_LENGTH] = { 0x00, 0x00, 0x00, 0x00, 0x80};
 
  /* send 0 delay pacing */
  return write_block(&rig->state.rigport, cmd, YAESU_CMD_LENGTH);
}



int vr5000_set_freq(RIG *rig, vfo_t vfo, freq_t freq)
{
  struct vr5000_priv_data *priv = rig->state.priv;
  unsigned char cmd[YAESU_CMD_LENGTH] = { 0x00, 0x00, 0x00, 0x00, 0x01};
  unsigned int frq;

  if (vfo == RIG_VFO_CURR)
	  vfo = priv->curr_vfo;

  if (vfo == RIG_VFO_B || vfo == RIG_VFO_SUB) {
	  cmd[4] |= 0x30;
  }

  frq = (unsigned int)(freq/10);

  cmd[0] = (frq >> 24) & 0xff;
  cmd[1] = (frq >> 16) & 0xff;
  cmd[2] = (frq >> 8) & 0xff;
  cmd[3] = frq & 0xff;

  /* Frequency set */
  return write_block(&rig->state.rigport, cmd, YAESU_CMD_LENGTH);
}


int vr5000_set_mode(RIG *rig, vfo_t vfo, rmode_t mode, pbwidth_t width)
{
  struct vr5000_priv_data *priv = rig->state.priv;
  unsigned char cmd[YAESU_CMD_LENGTH] = { 0x00, 0x00, 0x00, 0x00, 0x07};
  int retval;

  if (vfo == RIG_VFO_CURR)
	  vfo = priv->curr_vfo;

  if (vfo == RIG_VFO_B || vfo == RIG_VFO_SUB) {
	  cmd[4] |= 0x30;
  }

  retval = mode2rig(rig, mode, width);
  if (retval < 0)
	  return retval;

  /* fill in m1 */
  cmd[0] = priv->curr_mode = retval;

  /* fill in m2 with tuning step */
  cmd[1] = priv->curr_ts;

  return write_block(&rig->state.rigport, cmd, YAESU_CMD_LENGTH);
}

int vr5000_set_ts(RIG *rig, vfo_t vfo, shortfreq_t ts)
{
  struct vr5000_priv_data *priv = rig->state.priv;
  unsigned char cmd[YAESU_CMD_LENGTH] = { 0x00, 0x00, 0x00, 0x00, 0x07};
  static const unsigned char steps[] = { 
	  0x21, 0x42, 0x02, 0x03, 0x43, 0x53, 0x63,
	  0x04, 0x14, 0x24, 0x35, 0x44, 0x05, 0x45 };
  int i;

  if (vfo == RIG_VFO_CURR)
	  vfo = priv->curr_vfo;

  if (vfo == RIG_VFO_B || vfo == RIG_VFO_SUB) {
	  cmd[4] |= 0x30;
  }

  for (i=0; i<TSLSTSIZ; i++) {
	if (rig->caps->tuning_steps[i].ts == ts)
		break;
  }
  if (i >= TSLSTSIZ) {
	return -RIG_EINVAL;     /* not found, unsupported */
  }

  /* fill in m1 */
  cmd[0] = priv->curr_mode;

  /* fill in m2 with tuning step */
  cmd[1] = priv->curr_ts = steps[i];

  return write_block(&rig->state.rigport, cmd, YAESU_CMD_LENGTH);
}


int vr5000_set_vfo(RIG *rig, vfo_t vfo)
{
  struct vr5000_priv_data *priv = rig->state.priv;

  priv->curr_vfo = vfo;

  return RIG_OK;
}


int vr5000_get_level(RIG *rig, vfo_t vfo, setting_t level, value_t *val)
{
  unsigned char cmd[YAESU_CMD_LENGTH] = { 0x00, 0x00, 0x00, 0x00, 0xe7};
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
  val->i = cmd[0] & 0x3f;

  return RIG_OK;
}

int vr5000_get_dcd(RIG *rig, vfo_t vfo, dcd_t *dcd)
{
  unsigned char cmd[YAESU_CMD_LENGTH] = { 0x00, 0x00, 0x00, 0x00, 0xe7};
  int retval;
 
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

  *dcd = (cmd[0] & 0x80) ? RIG_DCD_ON : RIG_DCD_OFF;

  return RIG_OK;
}


#define MODE_LSB	0x00
#define MODE_USB	0x01
#define MODE_CW 	0x02
#define MODE_AM		0x04
#define MODE_AMW	0x44
#define MODE_AMN	0x84
#define MODE_WFM	0x48
#define MODE_FMN	0x88


int mode2rig(RIG *rig, rmode_t mode, pbwidth_t width)
{
  int md;

  /* 
   * translate mode from generic to vr5000 specific
   */
  switch(mode) {
  case RIG_MODE_USB:	md = MODE_USB; break;
  case RIG_MODE_LSB:	md = MODE_LSB; break;
  case RIG_MODE_CW:	md = MODE_CW; break;
  case RIG_MODE_WFM:	md = MODE_WFM; break;
  case RIG_MODE_FM:	md = MODE_FMN; break;
  case RIG_MODE_AM:
  	if (width != RIG_PASSBAND_NORMAL &&
		  width < rig_passband_normal(rig, mode))
		md = MODE_AMN;
	else if (width != RIG_PASSBAND_NORMAL &&
		  width > rig_passband_normal(rig, mode))
		md = MODE_AMW;
	else
		md = MODE_AM;
	break;
  default:
    return -RIG_EINVAL;         /* sorry, wrong MODE */
  }
  return md;
}

