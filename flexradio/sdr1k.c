/*
 *  Hamlib Rotator backend - SDR-1000
 *  Copyright (c) 2003 by Stephane Fillod
 *
 *	$Id: sdr1k.c,v 1.2 2003-09-28 15:36:57 fillods Exp $
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
#ifdef HAVE_SYS_IOCTL_H
#include <sys/ioctl.h>
#endif

#include "hamlib/rig.h"
#include "serial.h"
#include "misc.h"
#include "bandplan.h"
#include "register.h"

#include "sdr1k.h"

static int sdr1k_set_freq(RIG *rig, vfo_t vfo, freq_t freq);
static int sdr1k_get_freq(RIG *rig, vfo_t vfo, freq_t *freq);
static int sdr1k_reset(RIG *rig, reset_t reset);
static int sdr1k_init(RIG *rig);
static int sdr1k_open(RIG *rig);
static int sdr1k_close(RIG *rig);
static int sdr1k_cleanup(RIG *rig);
static int sdr1k_set_ptt (RIG *rig, vfo_t vfo, ptt_t ptt);

typedef enum { L_EXT = 0, L_BAND = 1, L_DDS0 = 2, L_DDS1 = 3 } latch_t;

static void write_latch (RIG *rig, latch_t which, int value, int mask);
static void write_reg (RIG *rig, int addr, int data);
static void set_bit (RIG *rig, latch_t reg, int bit, int state);


#define DEFAULT_XTAL MHz(200)

struct sdr1k_priv_data {
  int	shadow[4];	/* shadow latches */
  freq_t	xtal;		/* base XTAL */
};


#define SDR1K_FUNC  RIG_FUNC_MUTE
#define SDR1K_LEVEL RIG_LEVEL_PREAMP
#define SDR1K_PARM  RIG_PARM_NONE

#define SDR1K_MODES (RIG_MODE_NONE)

#define SDR1K_VFO RIG_VFO_A

#define SDR1K_ANTS 0


/* ************************************************************************* */
/*
 * http://www.flex-radio.com
 * SDR-1000 rig capabilities.
 *
 *
 * TODO: set_gain?  RIG_FUNC_MUTE, set_external_pin?
 *
 *    def set_gain (self, high):
 *      self.set_bit(0, 7, high)
 *
 *    def set_mute (self, mute = 1):
 *      self.set_bit(1, 7, mute)
 *
 *    def set_unmute (self):
 *      self.set_bit(1, 7, 0)
 *
 *    def set_external_pin (self, pin, on = 1):
 *      assert (pin < 8 and pin > 0), "Out of range 1..7"
 *      self.set_bit(0, pin-1, on)
 *
 *    set_conf(XTAL)
 */

const struct rig_caps sdr1k_rig_caps = {
  .rig_model =      RIG_MODEL_SDR1000,
  .model_name =     "SDR-1000",
  .mfg_name =       "Flex-radio",
  .version =        "0.1.1",
  .copyright = 	    "LGPL",
  .status =         RIG_STATUS_NEW,
  .rig_type =       RIG_TYPE_TUNER,
  .targetable_vfo = 	 0,
  .ptt_type =       RIG_PTT_RIG,
  .dcd_type =       RIG_DCD_NONE,
  .port_type =      RIG_PORT_PARALLEL,

  .has_get_func =   SDR1K_FUNC,
  .has_set_func =   SDR1K_FUNC,
  .has_get_level =  SDR1K_LEVEL,
  .has_set_level =  RIG_LEVEL_SET(SDR1K_LEVEL),
  .has_get_parm = 	 SDR1K_PARM,
  .has_set_parm = 	 RIG_PARM_SET(SDR1K_PARM),
  .chan_list = 	 {
			RIG_CHAN_END,
		 },
  .scan_ops = 	 RIG_SCAN_NONE,
  .vfo_ops = 	 RIG_OP_NONE,
  .transceive =     RIG_TRN_OFF,
  .attenuator =     { RIG_DBLST_END, },
  .preamp = 	 { 20, RIG_DBLST_END, },

  .rx_range_list1 =  { {.start=Hz(1),.end=MHz(65),.modes=SDR1K_MODES,
		    .low_power=-1,.high_power=-1,SDR1K_VFO},
		    RIG_FRNG_END, },
  .tx_range_list1 =  {
	  	/* restricted to ham band */
		FRQ_RNG_HF(1,SDR1K_MODES, W(1),W(1),SDR1K_VFO,SDR1K_ANTS),
		FRQ_RNG_6m(1,SDR1K_MODES, W(1),W(1),SDR1K_VFO,SDR1K_ANTS),
		RIG_FRNG_END, },

  .rx_range_list2 =  { {.start=Hz(1),.end=MHz(65),.modes=SDR1K_MODES,
		    .low_power=-1,.high_power=-1,SDR1K_VFO},
		    RIG_FRNG_END, },
  .tx_range_list2 =  {
	  	/* restricted to ham band */
		FRQ_RNG_HF(2,SDR1K_MODES, W(1),W(1),SDR1K_VFO,SDR1K_ANTS),
		FRQ_RNG_6m(2,SDR1K_MODES, W(1),W(1),SDR1K_VFO,SDR1K_ANTS),
		RIG_FRNG_END, },

  .tuning_steps =  { {SDR1K_MODES,1},
			RIG_TS_END,
  },
  .priv =  NULL,	/* priv */

  .rig_init =     sdr1k_init,
  .rig_open =     sdr1k_open,
  .rig_close =    sdr1k_close,
  .rig_cleanup =  sdr1k_cleanup,

  .set_freq =     sdr1k_set_freq,
  .get_freq =     sdr1k_get_freq,
  .set_ptt  =     sdr1k_set_ptt,
  .reset    =     sdr1k_reset,
//  .set_level=     sdr1k_set_level,
//  .set_func =     sdr1k_set_func,

};


/* ************************************************************************* */

DECLARE_INITRIG_BACKEND(flexradio)
{
	rig_debug(RIG_DEBUG_VERBOSE, "%s called\n", __FUNCTION__);

	rig_register(&sdr1k_rig_caps);

	return RIG_OK;
}

/* ************************************************************************* */

int sdr1k_init(RIG *rig)
{
	struct sdr1k_priv_data *priv;

	priv = (struct sdr1k_priv_data*)malloc(sizeof(struct sdr1k_priv_data));
	if (!priv) {
		/* whoops! memory shortage! */
		return -RIG_ENOMEM;
	}

	rig->state.current_freq = RIG_FREQ_NONE;
	priv->xtal = DEFAULT_XTAL;

	rig->state.priv = (void*)priv;

	return RIG_OK;
}

static void pdelay(RIG *rig)
{
	usleep(1);
}

int sdr1k_open(RIG *rig)
{
	struct sdr1k_priv_data *priv = (struct sdr1k_priv_data *)rig->state.priv;

	priv->shadow[0] = 0;
	priv->shadow[1] = 0;
	priv->shadow[2] = 0;
	priv->shadow[3] = 0;

	sdr1k_reset(rig, 1);

	write_latch (rig, L_DDS1, 0x00, 0xC0); /* Reset low, WRS/ low */
	write_reg (rig, 0x20, 0x40);

	return RIG_OK;
}

int sdr1k_close(RIG *rig)
{
	/* place holder.. */

	return RIG_OK;
}

int sdr1k_cleanup(RIG *rig)
{
	struct sdr1k_priv_data *priv = (struct sdr1k_priv_data *)rig->state.priv;

	if (priv) {
		free(priv);
	}
	rig->state.priv = NULL;

	return RIG_OK;
}

int sdr1k_set_freq(RIG *rig, vfo_t vfo, freq_t freq)
{
	struct sdr1k_priv_data *priv = (struct sdr1k_priv_data *)rig->state.priv;
	int i, band;
	double ftw;

	/* set_band */
        if (freq <= MHz(2.25))
		band = 0;
	else if ( freq <= MHz(5.5))
		band = 1;
	else if (freq <= MHz(11))
		band = 3;    /* due to wiring mistake on board */
	else if (freq <= MHz(22))
		band = 2;    /* due to wiring mistake on board */
	else if (freq <= MHz(37.5))
		band = 4;
	else
		band = 5;

	rig_debug(RIG_DEBUG_VERBOSE, "%s %lld band %d\n", __FUNCTION__, freq, band);

	write_latch (rig, L_BAND, 1 << band, 0x3f);

	ftw = (double)freq / priv->xtal ;

	for (i = 0; i<6; i++) {
		int word;

		word = (int)(ftw * 256);
		ftw = ftw*256 - word;
		rig_debug(RIG_DEBUG_TRACE, "DDS %d [%02x]\n", i, word);
		write_reg (rig, 4+i, word);
	}

	return RIG_OK;
}

int sdr1k_get_freq(RIG *rig, vfo_t vfo, freq_t *freq)
{
	*freq = rig->state.current_freq;
	return RIG_OK;
}



int sdr1k_reset (RIG *rig, reset_t reset)
{
  port_t *pport = &rig->state.rigport;

  par_lock (pport);
  par_write_control (pport, 0x0F);
  pdelay(rig);
  par_unlock (pport);
  write_latch (rig, L_EXT,  0x00, 0xff);
  write_latch (rig, L_BAND, 0x00, 0xff);
  write_latch (rig, L_DDS0, 0x80, 0xff);	/* hold DDS in reset */
  write_latch (rig, L_DDS1, 0x00, 0xff);

  return RIG_OK;
}

int sdr1k_set_ptt (RIG *rig, vfo_t vfo, ptt_t ptt)
{
	set_bit(rig, 1, 6, ptt == RIG_PTT_ON);

	return RIG_OK;
}
  


void
write_latch (RIG *rig, latch_t which, int value, int mask)
{
  struct sdr1k_priv_data *priv = (struct sdr1k_priv_data *)rig->state.priv;
  port_t *pport = &rig->state.rigport;

  if (!(0 <= which && which <= 3))
    return;
  
  par_lock (pport);
  priv->shadow[which] = (priv->shadow[which] & ~mask) | (value & mask);
  par_write_data (pport, priv->shadow[which]);
  pdelay(rig);
  par_write_control (pport, 0x0F ^ (1 << which));
  pdelay(rig);
  par_write_control (pport, 0x0F);
  pdelay(rig);
  par_unlock (pport);
}


void
write_reg (RIG *rig, int addr, int data)
{
	write_latch (rig, L_DDS1, addr & 0x3f, 0x3f);
	write_latch (rig, L_DDS0, data, 0xff);
	write_latch (rig, L_DDS1, 0x40, 0x40);
	write_latch (rig, L_DDS1, 0x00, 0x40);
}

void
set_bit (RIG *rig, latch_t reg, int bit, int state)
{
	int val;
	
	val = state ? 1<<bit : 0;

	write_latch (rig, reg, val, 1<<bit);
}

