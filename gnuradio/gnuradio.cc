/* -*- Mode: c++ -*- */
/*
 *  Hamlib GNUradio backend - main file
 *  Copyright (c) 2001,2002 by Stephane Fillod
 *
 *	$Id: gnuradio.cc,v 1.2 2002-08-26 21:26:06 fillods Exp $
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

/*
 * Simple backend of a chirp source.
 */

#include <VrSigSource.h>
#include <VrAudioSource.h>
#include <VrNullSink.h>
#include <VrAudioSink.h>
#include <VrSum.h>
#include <VrConnect.h>
#include <VrMultiTask.h>

/* move these into struct gnuradio_priv ? */
#define SAMPLING_FREQUENCY                          5e6 
#define	MAX_FREQUENCY		(SAMPLING_FREQUENCY / 2)
#define CARRIER_FREQ	          	        1.070e6	// AM 1070
#define AMPLITUDE                                   1.0
#define WAVEFORM                            VR_SIN_WAVE


#include <stdlib.h>
#include <stdio.h>   /* Standard input/output definitions */
#include <string.h>  /* String function definitions */
#include <unistd.h>  /* UNIX standard function definitions */
#include <fcntl.h>   /* File control definitions */
#include <errno.h>   /* Error number definitions */
#include <termios.h> /* POSIX terminal control definitions */
#include <sys/ioctl.h>
#include <math.h>

#include <hamlib/rig.h>
#include <misc.h>

#include "gnuradio.h"
#include "gr_priv.h"	// struct gnuradio_priv_data


static void init_chan(RIG *rig, vfo_t vfo, channel_t *chan)
{
  chan->channel_num = 0;
  chan->vfo = vfo;
  strcpy(chan->channel_desc, strvfo(vfo));

  chan->freq = MHz(145);
  chan->mode = RIG_MODE_FM;
  chan->width = rig_passband_normal(rig, RIG_MODE_FM);
  chan->tx_freq = chan->freq;
  chan->tx_mode = chan->mode;
  chan->tx_width = chan->width;
  chan->split = RIG_SPLIT_OFF;

  chan->rptr_shift = RIG_RPT_SHIFT_NONE;
  chan->rptr_offs = 0;
  chan->ctcss_tone = 0;
  chan->dcs_code = 0;
  chan->ctcss_sql = 0;
  chan->dcs_sql = 0;
  chan->rit = 0;
  chan->xit = 0;
  chan->tuning_step = 0;
  chan->ant = 0;

  chan->funcs = (setting_t)0;
  memset(chan->levels, 0, RIG_SETTING_MAX*sizeof(value_t));
}

int gr_init(RIG *rig)
{
  struct gnuradio_priv_data *priv;

  priv = (struct gnuradio_priv_data*)malloc(sizeof(struct gnuradio_priv_data));
  if (!priv)
	  return -RIG_ENOMEM;
  rig->state.priv = (void*)priv;

  rig_debug(RIG_DEBUG_VERBOSE,"%s called\n", __FUNCTION__ );
  rig->state.rigport.type.rig = RIG_PORT_NONE;

  memset(priv->parms, 0, RIG_SETTING_MAX*sizeof(value_t));

  init_chan(rig, RIG_VFO_A, &priv->vfo_a);
  priv->curr = &priv->vfo_a;
  priv->curr_vfo = priv->last_vfo = RIG_VFO_A;

  // or whatever
  priv->source = new VrSigSource<IOTYPE>(SAMPLING_FREQUENCY, WAVEFORM, CARRIER_FREQ, 2*AMPLITUDE);
  //priv->source =  new VrAudioSource<short>(SAMPLING_FREQUENCY);

  priv->sink = new VrNullSink<short>();
 //priv->sink = new VrAudioSink<short>();

  priv->m = new VrMultiTask ();
  priv->m->add (priv->sink);

  // now wire it all together from the sink, back to the sources
  
  NWO_CONNECT (priv->source, priv->sink);


  return RIG_OK;
}

int gr_open(RIG *rig)
{
  struct gnuradio_priv_data *priv = (struct gnuradio_priv_data*)rig->state.priv;

  rig_debug(RIG_DEBUG_VERBOSE,"%s called\n", __FUNCTION__);

  priv->m->start();

  return RIG_OK;
}

int gr_close(RIG *rig)
{
  rig_debug(RIG_DEBUG_VERBOSE,"%s called\n", __FUNCTION__);

  return RIG_OK;
}

int gr_cleanup(RIG *rig)
{
  struct gnuradio_priv_data *priv = (struct gnuradio_priv_data*)rig->state.priv;

  rig_debug(RIG_DEBUG_VERBOSE,"%s called\n", __FUNCTION__);

  delete priv->m;
  delete priv->sink;
  delete priv->source;

  if (rig->state.priv)
  	free(rig->state.priv);

  rig->state.priv = NULL;

  return RIG_OK;
}


int gr_set_freq(RIG *rig, vfo_t vfo, freq_t freq)
{
  struct gnuradio_priv_data *priv = (struct gnuradio_priv_data *)rig->state.priv;
  channel_t *curr = priv->curr;
  char fstr[20];

  sprintf_freq(fstr, freq);
  rig_debug(RIG_DEBUG_VERBOSE,"%s called: %s %s\n", 
 			__FUNCTION__, strvfo(vfo), fstr);
  curr->freq = freq;

  priv->source->setFrequency(freq);

  return RIG_OK;
}


int gr_get_freq(RIG *rig, vfo_t vfo, freq_t *freq)
{
  struct gnuradio_priv_data *priv = (struct gnuradio_priv_data *)rig->state.priv;
  channel_t *curr = priv->curr;

  rig_debug(RIG_DEBUG_VERBOSE,"%s called: %s\n", __FUNCTION__, strvfo(vfo));

  *freq = curr->freq;

  return RIG_OK;
}


/*
 * Does nothing yet
 */
int gr_set_mode(RIG *rig, vfo_t vfo, rmode_t mode, pbwidth_t width)
{
  struct gnuradio_priv_data *priv = (struct gnuradio_priv_data *)rig->state.priv;
  channel_t *curr = priv->curr;
  char buf[16];

  sprintf_freq(buf, width);
  rig_debug(RIG_DEBUG_VERBOSE,"%s called: %s %s %s\n", 
  		__FUNCTION__, strvfo(vfo), strrmode(mode), buf);

  curr->mode = mode;
  curr->width = width;

  return RIG_OK;
}


int gr_get_mode(RIG *rig, vfo_t vfo, rmode_t *mode, pbwidth_t *width)
{
  struct gnuradio_priv_data *priv = (struct gnuradio_priv_data *)rig->state.priv;
  channel_t *curr = priv->curr;

  rig_debug(RIG_DEBUG_VERBOSE,"%s called: %s\n", __FUNCTION__, strvfo(vfo));

  *mode = curr->mode;
  *width = curr->width;

  return RIG_OK;
}


int gr_get_vfo(RIG *rig, vfo_t *vfo)
{
  struct gnuradio_priv_data *priv = (struct gnuradio_priv_data *)rig->state.priv;

  *vfo = priv->curr_vfo;
  rig_debug(RIG_DEBUG_VERBOSE,"%s called: %s\n", __FUNCTION__, strvfo(*vfo));

  return RIG_OK;
}


int initrigs_gnuradio(void *be_handle)
{
	rig_debug(RIG_DEBUG_VERBOSE, "gnuradio: _init called\n");

	rig_register(&gr_caps);

	return RIG_OK;
}

