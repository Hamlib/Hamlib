/* -*- Mode: c++ -*- */
/*
 *  Hamlib GNUradio backend - main file
 *  Copyright (c) 2001-2003 by Stephane Fillod
 *
 *	$Id: gnuradio.cc,v 1.3 2003-02-09 23:02:26 fillods Exp $
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
#include <VrFileSource.h>
#include <VrNullSink.h>
#include <VrAudioSink.h>
#include <VrFileSink.h>

#include <VrFixOffset.h>
#include <GrMC4020Source.h>

#include <VrConnect.h>
#include <VrMultiTask.h>
/* SSB */
#include <GrSSBMod.h>
#include <GrHilbert.h>
/* FM */
#include <VrComplexFIRfilter.h>
#include <VrQuadratureDemod.h>
#include <VrRealFIRfilter.h>

/* Chirp param's */
#define CARRIER_FREQ	          	        1.070e6	// AM 1070
#define AMPLITUDE				3000

#include <stdlib.h>
#include <stdio.h>   /* Standard input/output definitions */
#include <string.h>  /* String function definitions */
#include <unistd.h>  /* UNIX standard function definitions */
#include <fcntl.h>   /* File control definitions */
#include <errno.h>   /* Error number definitions */
#include <sys/ioctl.h>
#include <math.h>
#include <pthread.h>

#include <hamlib/rig.h>
#include <misc.h>
#include <token.h>

#include "gnuradio.h"
#include "gr_priv.h"	// struct gnuradio_priv_data


#define TOK_TUNER_MODEL TOKEN_BACKEND(1)

const struct confparams gnuradio_cfg_params[] = {
	{ TOK_TUNER_MODEL, "tuner_model", "Tuner model", "Hamlib rig tuner model number",
		"1" /* RIG_MODEL_DUMMY */, RIG_CONF_NUMERIC, { /* .n = */ { 0, INT_MAX, 1 } }
	},
	{ RIG_CONF_END, NULL, }
};

static void init_chan(RIG *rig, vfo_t vfo, channel_t *chan)
{
  chan->channel_num = 0;
  chan->vfo = vfo;
  strcpy(chan->channel_desc, strvfo(vfo));

  chan->freq = RIG_FREQ_NONE;
  chan->mode = RIG_MODE_NONE;
  chan->width = 0;
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
  const struct gnuradio_priv_caps *priv_caps = (const struct gnuradio_priv_caps*)rig->caps->priv;
  int i;

  priv = (struct gnuradio_priv_data*)malloc(sizeof(struct gnuradio_priv_data));
  if (!priv)
	  return -RIG_ENOMEM;
  rig->state.priv = (void*)priv;

  rig_debug(RIG_DEBUG_VERBOSE,"%s called\n", __FUNCTION__ );
  rig->state.rigport.type.rig = RIG_PORT_NONE;

  memset(priv->parms, 0, RIG_SETTING_MAX*sizeof(value_t));

  priv->m = NULL;
  priv->sink = NULL;
  priv->source = NULL;
  priv->need_fixer = 0;

  for (i=0; i<NUM_CHAN; i++) {
  	init_chan(rig, RIG_CTRL_BAND(RIG_CTRL_MAIN,i), &priv->chans[i]);
	priv->chans[i].levels[rig_setting2idx(RIG_LEVEL_AF)].f = 1.0;
	priv->chans[i].levels[rig_setting2idx(RIG_LEVEL_RF)].f = 1.0;
  }
  priv->curr_vfo = RIG_VFO_A;

  priv->tuner_model = priv_caps->tuner_model;
  priv->input_rate = priv_caps->input_rate;
  priv->IF_center_freq = priv_caps->IF_center_freq;

  pthread_mutex_init(&priv->mutex_process, NULL);

  return RIG_OK;
}


/*
 * Assumes rig!=NULL, rig->state.priv!=NULL
 */
int gnuradio_set_conf(RIG *rig, token_t token, const char *val)
{
	struct gnuradio_priv_data *priv;
	struct rig_state *rs;

	rs = &rig->state;
	priv = (struct gnuradio_priv_data*)rs->priv;

	switch(token) {
	case TOK_TUNER_MODEL:
		priv->tuner_model = atoi(val);
		break;
	default:
		/* if it's not for the gnuradio backend, maybe it's for the tuner */
		return rig_set_conf(priv->tuner, token, val);
	}
	return RIG_OK;
}

/*
 * assumes rig!=NULL,
 * Assumes rig!=NULL, rig->state.priv!=NULL
 *  and val points to a buffer big enough to hold the conf value.
 */
int gnuradio_get_conf(RIG *rig, token_t token, char *val)
{
	struct gnuradio_priv_data *priv;
	struct rig_state *rs;

	rs = &rig->state;
	priv = (struct gnuradio_priv_data*)rs->priv;

	switch(token) {
	case TOK_TUNER_MODEL:
		sprintf(val, "%d", priv->tuner_model);
		break;
	default:
		/* if it's not for the gnuradio backend, maybe it's for the tuner */
		return rig_get_conf(priv->tuner, token, val);
	}
	return RIG_OK;
}

/*
 * GNUradio process thread
 *
 * the thread is _created_ with priv->mutex_process held
 *
 * TODO: change process name to give a hint about this CPU hungry process
 * 	also shouldn't we block couple of disturbing signals?
 */
static void *gnuradio_process(void *arg)
{
	RIG *rig = (RIG *)arg;
	struct gnuradio_priv_data *priv;

	priv = (struct gnuradio_priv_data*)rig->state.priv;

	/* the mutex lock is not to gurantee reentrancy of rig_debug,
	 * this is just to know when backend want us start running
	 */
	pthread_mutex_lock(&priv->mutex_process);
	rig_debug(RIG_DEBUG_TRACE,"gnuradio process thread started\n");
	pthread_mutex_unlock(&priv->mutex_process);

	while (priv->do_process) {
		pthread_mutex_lock(&priv->mutex_process);
		priv->m->process();
		pthread_mutex_unlock(&priv->mutex_process);
	}
	return NULL;
}

int gr_open(RIG *rig)
{
  struct gnuradio_priv_data *priv = (struct gnuradio_priv_data*)rig->state.priv;
  const struct gnuradio_priv_caps *priv_caps = (const struct gnuradio_priv_caps*)rig->caps->priv;
  int ret;


  rig_debug(RIG_DEBUG_VERBOSE,"%s called\n", __FUNCTION__);

  /*
   * make sure gnuradio's tuner is not gnuradio!
   */
  if (RIG_BACKEND_NUM(priv_caps->tuner_model) == RIG_GNURADIO) {
	  return -RIG_ECONF;
  }

  priv->tuner = rig_init(priv->tuner_model);
  if (!priv->tuner) {
	  /* FIXME: wrong rig model? */
	  return -RIG_ENOMEM;
  }
  rig_open(priv->tuner);

  /* TODO:
   * 	copy priv->tuner->rx_range/tx_range to rig->state, 
   * 	and override available modes with gnuradio's
   */


  /* ** Source ** */
  
  // --> short

  // Chirp
  if (!priv->source)
  	priv->source = new VrSigSource<IOTYPE>(priv->input_rate, VR_SIN_WAVE, CARRIER_FREQ, AMPLITUDE);
  /* VrFileSource (double sampling_freq, const char *file, bool repeat = false) */
  //priv->source = new VrFileSource<short>(priv->input_rate, "microtune_source.sw", true);

  // short --> short
  priv->need_fixer = 1;
  priv->offset_fixer = new VrFixOffset<short,short>();
  NWO_CONNECT (priv->source, priv->offset_fixer);

  priv->sink = new VrAudioSink<short>();

  /* ** Sink ** */
  if (!priv->sink)
  	priv->sink = new VrNullSink<short>();
  //priv->sink = new VrFileSink<short>("microtune_audio.sw");

  priv->m = new VrMultiTask ();

  priv->m->start();

  /* or set it to MODE_NONE? */
  //gr_set_mode(rig, RIG_VFO_CURR, RIG_MODE_WFM, RIG_PASSBAND_NORMAL);

  priv->do_process = 1;

  pthread_mutex_lock(&priv->mutex_process);

  ret = pthread_create(&priv->process_thread, NULL, gnuradio_process, (void*)rig);

  pthread_mutex_unlock(&priv->mutex_process);

  if (ret != 0) {
	  /* TODO: undo the close*/
	  rig_debug(RIG_DEBUG_ERR, "%s: pthread_create failed: %s\n", __FUNCTION__, strerror(errno));
	  return -RIG_ENOMEM;	/* huh? */
  }

  return RIG_OK;
}


/* TODO: error checking of new */
static int mc4020_open(RIG *rig)
{
	struct gnuradio_priv_data *priv = (struct gnuradio_priv_data*)rig->state.priv;

	/* input sample rate from PCI-DAS4020/12: 20000000  */
	priv->source = new GrMC4020Source<short>(priv->input_rate, MCC_CH3_EN | MCC_ALL_1V);

	return gr_open(rig);
}


/*
 * sound card source override
 */
static int graudio_open(RIG *rig)
{
	struct gnuradio_priv_data *priv = (struct gnuradio_priv_data*)rig->state.priv;

	/*
	 * assumes sound card is full duplex!
	 */
	priv->source =  new VrAudioSource<short>(priv->input_rate);

	return gr_open(rig);
}


int gr_close(RIG *rig)
{
  struct gnuradio_priv_data *priv = (struct gnuradio_priv_data*)rig->state.priv;
  int ret;

  rig_debug(RIG_DEBUG_VERBOSE,"%s called\n", __FUNCTION__);

  priv->do_process = 0;
  pthread_mutex_lock(&priv->mutex_process);
  priv->m->stop();
  pthread_mutex_unlock(&priv->mutex_process);

  ret = pthread_join(priv->process_thread, NULL);
  if (ret != 0) {
	  rig_debug(RIG_DEBUG_ERR, "%s: pthread_join failed: %s\n", __FUNCTION__, strerror(errno));
  }
  rig_debug(RIG_DEBUG_TRACE,"%s: process thread stopped\n", __FUNCTION__);

  delete priv->m;
  delete priv->sink;
  delete priv->source;
  if (priv->need_fixer)
  	delete priv->offset_fixer;

  priv->m = NULL;
  priv->sink = NULL;
  priv->source = NULL;
  priv->need_fixer = 0;

  rig_close(priv->tuner);

  return RIG_OK;
}

int gr_cleanup(RIG *rig)
{
  struct gnuradio_priv_data *priv = (struct gnuradio_priv_data*)rig->state.priv;

  rig_debug(RIG_DEBUG_VERBOSE,"%s called\n", __FUNCTION__);

  rig_cleanup(priv->tuner);

  /* note: mutex must be unlocked! */
  pthread_mutex_destroy(&priv->mutex_process);

  if (rig->state.priv)
  	free(rig->state.priv);

  rig->state.priv = NULL;

  return RIG_OK;
}

static int vfo2chan_num(RIG *rig, vfo_t vfo)
{
	struct gnuradio_priv_data *priv = (struct gnuradio_priv_data *)rig->state.priv;

	if (vfo == RIG_VFO_CURR)
		vfo = priv->curr_vfo;
	switch (vfo) {
	case RIG_VFO_A:
		return 0;
	case RIG_VFO_B:
		return 1;
	}
  	rig_debug(RIG_DEBUG_WARN,"%s unknown vfo: %d\n", vfo);
	return 0;
}

/*
 * rig_set_freq is a good candidate for the GNUradio GUI setFrequency callback?
 */
int gr_set_freq(RIG *rig, vfo_t vfo, freq_t freq)
{
  struct gnuradio_priv_data *priv = (struct gnuradio_priv_data *)rig->state.priv;
  int chan_num = vfo2chan_num(rig, vfo);
  channel_t *chan = &priv->chans[chan_num];
  union mod_data *mod = &priv->mods[chan_num];
  freq_t tuner_freq;
  double freq_offset;
  int ret;
  char fstr[20];

  sprintf_freq(fstr, freq);
  rig_debug(RIG_DEBUG_VERBOSE,"%s called: %s %s\n", 
 			__FUNCTION__, strvfo(vfo), fstr);

  ret = rig_get_freq(priv->tuner, RIG_VFO_CURR, &tuner_freq);
  if (ret != RIG_OK)
	  return ret;

  /* check if we're out of current IF window
   * TODO: with several VFO's, center the IF inbetween if out of window
   */
  if (freq < tuner_freq || 
		  freq /* + mode_width */ > tuner_freq+priv->input_rate/2) {

	  /* do not set tuner to freq, but center it */
	  ret = rig_set_freq(priv->tuner, RIG_VFO_CURR, freq + priv->input_rate/4);
	  if (ret != RIG_OK)
		  return ret;
	  /* query freq right back, because wanted freq may not be real freq,
	   * because of resolution of tuner */
  	  ret = rig_get_freq(priv->tuner, RIG_VFO_CURR, &tuner_freq);
	  if (ret != RIG_OK)
		return ret;
  }
  /*
   * In case the tuner is not a real tuner
   */
  if (priv->tuner_model == RIG_MODEL_DUMMY)
	  tuner_freq = 0;

  freq_offset = (double) (tuner_freq - freq);
  rig_debug(RIG_DEBUG_VERBOSE, "%s: %lld %lld freq_offset=%g\n",
		  __FUNCTION__, tuner_freq, freq, freq_offset);

  /* TODO: change mode plumbing (don't forget locking!)
   * workaround?: set_mode(NONE), set_mode(previous)
   */
  pthread_mutex_lock(&priv->mutex_process);
  switch (chan->mode) {
  case RIG_MODE_WFM:
  case RIG_MODE_FM:
	  /* not so sure about if setCenter_Freq is the Right thing to do(tm). */
	mod->fm.chan_filter_1->setCenter_Freq(priv->IF_center_freq - freq_offset);
	break;

  case RIG_MODE_USB:
	  {
	mod->ssb.shifter->set_freq(2*M_PI*freq_offset/(double)priv->input_rate);
	break;
	  }
  case RIG_MODE_NONE:
	break;  
  default:
	  rig_debug(RIG_DEBUG_WARN, "%s: mode % unimplemented!\n",
			  __FUNCTION__, strrmode(chan->mode));
	  break;
  }
  pthread_mutex_unlock(&priv->mutex_process);

  chan->freq = freq;

  return RIG_OK;
}


int gr_get_freq(RIG *rig, vfo_t vfo, freq_t *freq)
{
  struct gnuradio_priv_data *priv = (struct gnuradio_priv_data *)rig->state.priv;
  int chan_num = vfo2chan_num(rig, vfo);
  channel_t *chan = &priv->chans[chan_num];

  rig_debug(RIG_DEBUG_VERBOSE,"%s called: %s\n", __FUNCTION__, strvfo(vfo));

  *freq = chan->freq;

  return RIG_OK;
}


/*
 * WIP
 */
int gr_set_mode(RIG *rig, vfo_t vfo, rmode_t mode, pbwidth_t width)
{
  struct gnuradio_priv_data *priv = (struct gnuradio_priv_data *)rig->state.priv;
  int chan_num = vfo2chan_num(rig, vfo);
  channel_t *chan = &priv->chans[chan_num];
  union mod_data *mod = &priv->mods[chan_num];
  char buf[16];
  freq_t tuner_freq;
  int ret = RIG_OK;
  double freq_offset;

  sprintf_freq(buf, width);
  rig_debug(RIG_DEBUG_VERBOSE,"%s called: %s %s %s\n", 
  		__FUNCTION__, strvfo(vfo), strrmode(mode), buf);

  if (mode == chan->mode && width == chan->width)
	  return RIG_OK;

  ret = rig_get_freq(priv->tuner, RIG_VFO_CURR, &tuner_freq);
  if (ret != RIG_OK)
	  return ret;
  /*
   * In case the tuner is not a real tuner
   */
  if (priv->tuner_model == RIG_MODEL_DUMMY)
	  tuner_freq = 0;

  freq_offset = (double) (tuner_freq - chan->freq);
  rig_debug(RIG_DEBUG_VERBOSE, "%s: freq_offset=%g\n",
		  __FUNCTION__, freq_offset);

  pthread_mutex_lock(&priv->mutex_process);

  /* TODO: destroy GNUradio connections beforehand if different mode? */
  if (chan->mode != RIG_MODE_NONE && mode != chan->mode) {

  	switch (chan->mode) {
	  case RIG_MODE_FM:
	  case RIG_MODE_WFM:
		delete mod->fm.audio_filter_1;
		delete mod->fm.fm_demod_1;
		delete mod->fm.chan_filter_1;
		break;
	  case RIG_MODE_USB:
		delete mod->ssb.hilb;
		delete mod->ssb.shifter;
		break;
	  default:
		break;
	  }
  }

  /* TODO: if same mode, but different width, then adapt */

  switch (mode) {
  case RIG_MODE_FM:	/* is there a difference ? */
  case RIG_MODE_WFM:
	  {
  mod->fm.chanTaps = 75;
  mod->fm.CFIRdecimate = 125;
  mod->fm.chanGain = 2.0;
  mod->fm.FMdemodGain = 2200;
  mod->fm.RFIRdecimate = 5;
  mod->fm.ifTaps = 50;
  mod->fm.ifGain = 1.0;
	const int quadRate = priv->input_rate / mod->fm.CFIRdecimate;
	const int audioRate = quadRate / mod->fm.RFIRdecimate;
	float volume = chan->levels[rig_setting2idx(RIG_LEVEL_AF)].f;

	//
	// setup Wide FM demodulator chain
	//
	
	// short --> VrComplex
	mod->fm.chan_filter_1 = new VrComplexFIRfilter<short>(mod->fm.CFIRdecimate, mod->fm.chanTaps,
				  priv->IF_center_freq - freq_offset,
				  mod->fm.chanGain);

	// VrComplex --> float
	mod->fm.fm_demod_1 = new VrQuadratureDemod<float>(volume * mod->fm.FMdemodGain);

	// float --> short
	mod->fm.audio_filter_1 = new VrRealFIRfilter<float,short>(mod->fm.RFIRdecimate, audioRate/2,
				     mod->fm.ifTaps, mod->fm.ifGain);

	//connect the modules together

	NWO_CONNECT (GR_SOURCE(priv), mod->fm.chan_filter_1);
	NWO_CONNECT (mod->fm.chan_filter_1, mod->fm.fm_demod_1);
	NWO_CONNECT (mod->fm.fm_demod_1, mod->fm.audio_filter_1);
	NWO_CONNECT (mod->fm.audio_filter_1, priv->sink);

  	priv->m->stop();
  	priv->m->add (priv->sink);
  	priv->m->start();

	break;
	  }

  case RIG_MODE_USB:
	  {
	float rf_gain = chan->levels[rig_setting2idx(RIG_LEVEL_RF)].f;

	/* SSB is a WIP
	 * I guess we're missing a low pass filter?
	 */

	mod->ssb.hilb = new GrHilbert<short>(31);	/* what's that 31? */
	mod->ssb.shifter = new GrSSBMod<short>(2*M_PI*freq_offset/(double)priv->input_rate,
			rf_gain);

	//connect the modules together
	NWO_CONNECT (GR_SOURCE(priv), mod->ssb.hilb);
	NWO_CONNECT (mod->ssb.hilb, mod->ssb.shifter);
	NWO_CONNECT (mod->ssb.shifter, priv->sink);

  	priv->m->stop();
  	priv->m->add (priv->sink);
  	priv->m->start();

	break;
	  }

  case RIG_MODE_NONE:
	  /* ez */
	break;  

  default:
	  ret = -RIG_EINVAL;
  }

  pthread_mutex_unlock(&priv->mutex_process);

  if (ret == RIG_OK) {
	chan->mode = mode;
	chan->width = width;
  }

  return ret;
}


int gr_get_mode(RIG *rig, vfo_t vfo, rmode_t *mode, pbwidth_t *width)
{
  struct gnuradio_priv_data *priv = (struct gnuradio_priv_data *)rig->state.priv;
  int chan_num = vfo2chan_num(rig, vfo);
  channel_t *chan = &priv->chans[chan_num];

  rig_debug(RIG_DEBUG_VERBOSE,"%s called: %s\n", __FUNCTION__, strvfo(vfo));

  *mode = chan->mode;
  *width = chan->width;

  return RIG_OK;
}


int gr_set_vfo(RIG *rig, vfo_t vfo)
{
  struct gnuradio_priv_data *priv = (struct gnuradio_priv_data *)rig->state.priv;

  if (vfo == RIG_VFO_VFO || vfo == RIG_VFO_RX)
	  return RIG_OK;

  if (vfo != RIG_VFO_A && vfo != RIG_VFO_B)
	  return -RIG_EINVAL;

  priv->curr_vfo = vfo;
  rig_debug(RIG_DEBUG_VERBOSE,"%s called: %s\n", __FUNCTION__, strvfo(vfo));

  return RIG_OK;
}


int gr_get_vfo(RIG *rig, vfo_t *vfo)
{
  struct gnuradio_priv_data *priv = (struct gnuradio_priv_data *)rig->state.priv;

  *vfo = priv->curr_vfo;
  rig_debug(RIG_DEBUG_VERBOSE,"%s called: %s\n", __FUNCTION__, strvfo(*vfo));

  return RIG_OK;
}

static int set_rf_gain(RIG *rig, channel_t *chan, union mod_data *mod, float gain)
{
  struct gnuradio_priv_data *priv = (struct gnuradio_priv_data *)rig->state.priv;

  pthread_mutex_lock(&priv->mutex_process);
  switch (chan->mode) {
  case RIG_MODE_USB:
	  {
	mod->ssb.shifter->set_gain(gain);
	break;
	  }
  case RIG_MODE_NONE:
	break;  
  case RIG_MODE_WFM:
  case RIG_MODE_FM:
	  /* chan_filter_1/VrComplexFIRfilter is missing a setGain method! */
  default:
	  rig_debug(RIG_DEBUG_WARN, "%s: mode % unimplemented!\n",
			  __FUNCTION__, strrmode(chan->mode));
	  break;
  }
  pthread_mutex_unlock(&priv->mutex_process);

  return RIG_OK;
}

/*
 * TODO:
 * 	restart machinery after level change (e.g. volume, etc.)
 */
int gnuradio_set_level(RIG *rig, vfo_t vfo, setting_t level, value_t val)
{
  struct gnuradio_priv_data *priv = (struct gnuradio_priv_data *)rig->state.priv;
  int chan_num = vfo2chan_num(rig, vfo);
  channel_t *chan = &priv->chans[chan_num];
  union mod_data *mod = &priv->mods[chan_num];
  char lstr[32];
  int idx;
  int ret = RIG_OK;

  idx = rig_setting2idx(level);
  if (idx < RIG_SETTING_MAX)
  	chan->levels[idx] = val;

  if (RIG_LEVEL_IS_FLOAT(level))
	  sprintf(lstr, "%f", val.f);
  else
	  sprintf(lstr, "%d", val.i);
  rig_debug(RIG_DEBUG_VERBOSE,"%s called: %s %s\n",__FUNCTION__, 
				  strlevel(level), lstr);
  /* TODO: check val is in range */

  switch (level) {
  case RIG_LEVEL_RF:
	ret = set_rf_gain(rig, chan, mod, val.f);
	break;
  default:
	rig_debug(RIG_DEBUG_WARN, "%s: level % unimplemented!\n",
			  __FUNCTION__, strlevel(level));
	break;
  }

  return ret;
}


int gnuradio_get_level(RIG *rig, vfo_t vfo, setting_t level, value_t *val)
{
  struct gnuradio_priv_data *priv = (struct gnuradio_priv_data *)rig->state.priv;
  int chan_num = vfo2chan_num(rig, vfo);
  channel_t *chan = &priv->chans[chan_num];
  int idx;

  idx = rig_setting2idx(level);

  if (idx < RIG_SETTING_MAX)
  	*val = chan->levels[idx];
  rig_debug(RIG_DEBUG_VERBOSE,"%s called: %s\n",__FUNCTION__, 
				  strlevel(level));

  return RIG_OK;
}

int initrigs_gnuradio(void *be_handle)
{
	rig_debug(RIG_DEBUG_VERBOSE, "gnuradio: _init called\n");

	rig_register(&gr_caps);
	rig_register(&mc4020_caps);
	rig_register(&graudio_caps);

	return RIG_OK;
}

