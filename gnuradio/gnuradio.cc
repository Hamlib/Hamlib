/* -*- Mode: c++ -*- */
/*
 *  Hamlib GNUradio backend - main file
 *  Copyright (c) 2001-2004 by Stephane Fillod
 *
 *
 *   This library is free software; you can redistribute it and/or
 *   modify it under the terms of the GNU Lesser General Public
 *   License as published by the Free Software Foundation; either
 *   version 2.1 of the License, or (at your option) any later version.
 *
 *   This library is distributed in the hope that it will be useful,
 *   but WITHOUT ANY WARRANTY; without even the implied warranty of
 *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 *   Lesser General Public License for more details.
 *
 *   You should have received a copy of the GNU Lesser General Public
 *   License along with this library; if not, write to the Free Software
 *   Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA
 *
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

/*
 * Simple backend of a chirp source.
 */

#include <VrSigSource.h>
#include <GrAudioSource.h>
#include <VrFileSource.h>
#include <VrNullSink.h>
#include <GrAudioSink.h>
#include <VrFileSink.h>

#include <make_GrMC4020Source.h>
#include <VrFixOffset.h>
#include <GrConvertSF.h>

#include <VrConnect.h>
#include <VrMultiTask.h>


/* Demodulator chains */
#include <nfm.h>
#include <am.h>
#include <ssb.h>
#include <wfm.h>



/* Chirp param's */
#define CARRIER_FREQ	          	        1.070e6	// AM 1070
#define AMPLITUDE				3000

#define AUDIO_SINK	"/dev/dsp"
#define AUDIO_SRC	"/dev/dsp1"

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
#include <register.h>

#include "gnuradio.h"
#include "gr_priv.h"	// struct gnuradio_priv_data


/*
 * TODO: fft scope, with ext_level to display them
 */

#define TOK_TUNER_MODEL TOKEN_BACKEND(1)

const struct confparams gnuradio_cfg_params[] = {
	{ TOK_TUNER_MODEL, "tuner_model", "Tuner model", "Hamlib rig tuner model number",
		"1" /* RIG_MODEL_DUMMY */, RIG_CONF_NUMERIC, { /* .n = */ { 0, INT_MAX, 1 } }
	},
	/*
	 * TODO: IF_center_freq, input_rate, etc.
	 */
	{ RIG_CONF_END, NULL, }
};

static void init_chan(RIG *rig, vfo_t vfo, channel_t *chan)
{
  chan->channel_num = 0;
  chan->vfo = vfo;
  strcpy(chan->channel_desc, rig_strvfo(vfo));

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

  for (i=0; i<NUM_CHAN; i++) {
  	init_chan(rig, RIG_VFO_N(i), &priv->chans[i]);
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

  rig->state.has_set_func  |= priv->tuner->state.has_set_func;
  rig->state.has_get_func  |= priv->tuner->state.has_get_func;
  rig->state.has_set_level |= priv->tuner->state.has_set_level;
  rig->state.has_get_level |= priv->tuner->state.has_get_level;
  rig->state.has_set_parm  |= priv->tuner->state.has_set_parm;
  rig->state.has_get_parm  |= priv->tuner->state.has_get_parm;


  /* ** Source ** */

  // --> short

#if 0
  if (!priv->source) {
  	priv->source = new VrFileSource<short>(priv->input_rate, "/tmp/fm95_5_half.dat", true);
  }
  // Chirp
  if (!priv->source)
  	priv->source = new VrSigSource<IOTYPE>(priv->input_rate, VR_SIN_WAVE, CARRIER_FREQ, AMPLITUDE);
#endif
  //new VrChirpSource<IOTYPE>(priv->input_rate, AMPLITUDE, 4);
  /* VrFileSource (double sampling_freq, const char *file, bool repeat = false) */
  //priv->source = new VrFileSource<short>(priv->input_rate, "microtune_source.sw", true);

  priv->sink = new GrAudioSink<float>(1,AUDIO_SINK);

  /* ** Sink ** */
  if (!priv->sink)
  	priv->sink = new VrNullSink<float>();
  //priv->sink = new VrFileSink<short>("microtune_audio.sw");

  priv->m = new VrMultiTask ();

  priv->m->start();

  if (priv->tuner_model == RIG_MODEL_DUMMY) {
	  gr_set_freq(rig, RIG_VFO_CURR, priv->IF_center_freq);
  }

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
int mc4020_open(RIG *rig)
{
	struct gnuradio_priv_data *priv = (struct gnuradio_priv_data*)rig->state.priv;

	/* input sample rate from PCI-DAS4020/12: 20000000  */

	priv->mc4020_source = make_GrMC4020SourceS(priv->input_rate, MCC_CH3_EN | MCC_ALL_1V);

  	// short --> short
  	priv->offset_fixer = new VrFixOffset<short,short>();

  	// short --> float
	priv->convert_SF = new GrConvertSF();

	NWO_CONNECT (priv->mc4020_source, priv->offset_fixer);
	NWO_CONNECT (priv->offset_fixer, priv->convert_SF);
	NWO_CONNECT (priv->convert_SF, priv->source);

	return gr_open(rig);
}


/*
 * graudio, mono
 * sound card source override
 */
int graudio_open(RIG *rig)
{
	struct gnuradio_priv_data *priv = (struct gnuradio_priv_data*)rig->state.priv;

	/*
	 * assumes sound card is full duplex!
	 * mono source
	 */
	priv->source =  new GrAudioSource<VrComplex>(priv->input_rate, 1,1,AUDIO_SRC);

	return gr_open(rig);
}


/*
 * graudio I&Q, stereo
 * sound card source override
 */
int graudioiq_open(RIG *rig)
{
	struct gnuradio_priv_data *priv = (struct gnuradio_priv_data*)rig->state.priv;

	/*
	 * assumes sound card is full duplex!
	 * I&Q source
	 */
	priv->source =  new GrAudioSource<VrComplex>(priv->input_rate, 2,1, AUDIO_SRC);

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

  priv->m = NULL;
  priv->sink = NULL;
  priv->source = NULL;

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
 * tuner_freq is the display freq on tuner of the IF in digital domain
 * freq is the desired freq
 */
static int update_freq(RIG *rig, unsigned chan_num, freq_t tuner_freq, freq_t freq)
{
  struct gnuradio_priv_data *priv = (struct gnuradio_priv_data *)rig->state.priv;
  channel_t *chan = &priv->chans[chan_num];
  DemodChainCF *mod = priv->mods[chan_num];
  double freq_offset;

  if (chan->mode == RIG_MODE_NONE || !mod) {
	rig_debug(RIG_DEBUG_TRACE,"No (de)modulator for chan %d\n",chan_num);
	return RIG_OK;
  }

  /*
   * In case the tuner is not a real tuner
   */
  if (priv->tuner_model == RIG_MODEL_DUMMY)
	  tuner_freq = priv->IF_center_freq;

  freq_offset = (double) (freq - tuner_freq);
  rig_debug(RIG_DEBUG_VERBOSE, "%s: tuner:%lld gr:%lld freq_offset=%g\n",
		  __FUNCTION__, tuner_freq, freq, freq_offset);

  if (freq_offset == 0)
	  return RIG_OK;	/* nothing to do */

  pthread_mutex_lock(&priv->mutex_process);

  /*
   * not so sure about if setCenter_Freq is the Right thing to do(tm).
   */
  mod->setFreq((freq_t)(priv->IF_center_freq + freq_offset));

  pthread_mutex_unlock(&priv->mutex_process);

  return RIG_OK;
}

/*
 * rig_set_freq is a good candidate for the GNUradio GUI setFrequency callback?
 */
int gr_set_freq(RIG *rig, vfo_t vfo, freq_t freq)
{
  struct gnuradio_priv_data *priv = (struct gnuradio_priv_data *)rig->state.priv;
  int chan_num = vfo2chan_num(rig, vfo);
  channel_t *chan = &priv->chans[chan_num];
  freq_t tuner_freq;
  int ret, i;
  char fstr[20];

  sprintf_freq(fstr, freq);
  rig_debug(RIG_DEBUG_TRACE,"%s called: %s %s\n",
 			__FUNCTION__, rig_strvfo(vfo), fstr);

  ret = rig_get_freq(priv->tuner, RIG_VFO_CURR, &tuner_freq);
  if (ret != RIG_OK)
	  return ret;

  /* check if we're out of current IF window
   * TODO: with several VFO's, center the IF inbetween if out of window
   */
  if (freq < tuner_freq ||
		  freq /* + mode_width */ > tuner_freq+GR_MAX_FREQUENCY(priv)) {
	  /*
	   * do not set tuner to freq, but center it
	   */
#if 0
	  if (GR_MAX_FREQUENCY(priv)/2 < mode_offset)
		  tuner_freq = freq - mode_offset - resolution;
	  else
#endif
		  tuner_freq = freq - GR_MAX_FREQUENCY(priv)/2;
	  ret = rig_set_freq(priv->tuner, RIG_VFO_CURR, tuner_freq);
	  if (ret != RIG_OK)
		  return ret;
	  /*
	   * query freq right back, because wanted freq may be rounded
	   * because of the resolution of the tuner
	   */
  	  ret = rig_get_freq(priv->tuner, RIG_VFO_CURR, &tuner_freq);
	  if (ret != RIG_OK)
		return ret;

	  /*
	   * tuner freq changed, so adjust frequency offset of other channels
	   */
	  for (i = 0; i<NUM_CHAN; i++) {
		  if (i != chan_num)
  			update_freq(rig, i, tuner_freq, priv->chans[i].freq);
	  }
  }

  ret = update_freq(rig, chan_num, tuner_freq, freq);
  if (ret == RIG_OK)
  	chan->freq = freq;

  return ret;
}


int gr_get_freq(RIG *rig, vfo_t vfo, freq_t *freq)
{
  struct gnuradio_priv_data *priv = (struct gnuradio_priv_data *)rig->state.priv;
  int chan_num = vfo2chan_num(rig, vfo);
  channel_t *chan = &priv->chans[chan_num];

  rig_debug(RIG_DEBUG_VERBOSE,"%s called: %s\n", __FUNCTION__, rig_strvfo(vfo));

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
  DemodChainCF *mod = priv->mods[chan_num];
  char buf[16];
  freq_t tuner_freq;
  int ret = RIG_OK;
  double freq_offset;

  if (width == RIG_PASSBAND_NORMAL)
	  width = rig_passband_normal(rig, mode);
  sprintf_freq(buf, width);
  rig_debug(RIG_DEBUG_VERBOSE,"%s called: %s %s %s\n",
  		__FUNCTION__, rig_strvfo(vfo), rig_strrmode(mode), buf);

  if (mode == chan->mode
      && (RIG_PASSBAND_NOCHANGE == width || width == chan->width))
	  return RIG_OK;


  ret = rig_get_freq(priv->tuner, RIG_VFO_CURR, &tuner_freq);
  if (ret != RIG_OK)
	  return ret;
  /*
   * In case the tuner is not a real tuner
   */
  if (priv->tuner_model == RIG_MODEL_DUMMY)
	  tuner_freq = 0;

  freq_offset = (double) (chan->freq - tuner_freq + priv->IF_center_freq);
  rig_debug(RIG_DEBUG_VERBOSE, "%s: freq_offset=%g tuner_freq=%lld, IFcenter=%ld\n",
		  __FUNCTION__, freq_offset, tuner_freq, priv->IF_center_freq);




  pthread_mutex_lock(&priv->mutex_process);

  /* Same mode, but different width */
  if (width != RIG_PASSBAND_NOCHANGE
      && mode != RIG_MODE_NONE && mode == chan->mode) {
	mod->setWidth(width);
	chan->width = width;
	pthread_mutex_unlock(&priv->mutex_process);
	return RIG_OK;
  }

  priv->m->stop();

  /* TODO: destroy GNUradio connections beforehand if different mode? */
  if (chan->mode != RIG_MODE_NONE && mode != chan->mode) {

	delete mod;
  	priv->mods[chan_num] = NULL;

	delete priv->m;
	priv->m = new VrMultiTask ();
  }

  if (mode == RIG_MODE_NONE) {
	  /* ez */
    chan->mode = mode;
    if (width != RIG_PASSBAND_NOCHANGE) chan->width = width;
  	pthread_mutex_unlock(&priv->mutex_process);
	return RIG_OK;
  }

  float rf_gain = chan->levels[rig_setting2idx(RIG_LEVEL_RF)].f;

  if (RIG_PASSBAND_NOCHANGE == width) width = chan->width;

  switch(mode) {
  case RIG_MODE_LSB:
	mod = new LSBDemodChainCF(priv->source, priv->sink, mode, width, priv->input_rate, (freq_t)freq_offset);
	break;
  case RIG_MODE_USB:
	mod = new USBDemodChainCF(priv->source, priv->sink, mode, width, priv->input_rate, (freq_t)freq_offset);
	break;
  case RIG_MODE_AM:
	mod = new AMDemodChainCF(priv->source, priv->sink, mode, width, priv->input_rate, (freq_t)freq_offset);
	break;
  case RIG_MODE_FM:
	mod = new FMDemodChainCF(priv->source, priv->sink, mode, width, priv->input_rate, (freq_t)freq_offset);
	break;
  case RIG_MODE_WFM:
	mod = new WFMDemodChainCF(priv->source, priv->sink, mode, width, priv->input_rate, (freq_t)freq_offset);
	break;
  default:
	  ret = -RIG_EINVAL;
  }

  priv->mods[chan_num] = mod;

  /* wire up the chain */
  mod->connect();

  priv->m->add (priv->sink);
  priv->m->start();

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

  rig_debug(RIG_DEBUG_VERBOSE,"%s called: %s\n", __FUNCTION__, rig_strvfo(vfo));

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
  rig_debug(RIG_DEBUG_VERBOSE,"%s called: %s\n", __FUNCTION__, rig_strvfo(vfo));

  return RIG_OK;
}


int gr_get_vfo(RIG *rig, vfo_t *vfo)
{
  struct gnuradio_priv_data *priv = (struct gnuradio_priv_data *)rig->state.priv;

  *vfo = priv->curr_vfo;
  rig_debug(RIG_DEBUG_VERBOSE,"%s called: %s\n", __FUNCTION__, rig_strvfo(*vfo));

  return RIG_OK;
}

/*
 */
int gnuradio_set_level(RIG *rig, vfo_t vfo, setting_t level, value_t val)
{
  struct gnuradio_priv_data *priv = (struct gnuradio_priv_data *)rig->state.priv;
  int chan_num = vfo2chan_num(rig, vfo);
  channel_t *chan = &priv->chans[chan_num];
  DemodChainCF *mod = priv->mods[chan_num];
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
				  rig_strlevel(level), lstr);
  /* TODO: check val is in range */

  pthread_mutex_lock(&priv->mutex_process);

  switch (level) {
  case RIG_LEVEL_RF:
	  /* line-in level of sound card, etc. */
	break;
  default:
	rig_debug(RIG_DEBUG_TRACE, "%s: level %s, try tuner\n",
			  __FUNCTION__, rig_strlevel(level));
	ret = rig_set_level(priv->tuner, vfo, level, val);
	break;
  }

  pthread_mutex_unlock(&priv->mutex_process);

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
				  rig_strlevel(level));

  return RIG_OK;
}


/*
 * TODO: change BFO...
 */
int gnuradio_set_rit(RIG *rig, vfo_t vfo, shortfreq_t rit)
{
  struct gnuradio_priv_data *priv = (struct gnuradio_priv_data *)rig->state.priv;
  int chan_num = vfo2chan_num(rig, vfo);
  channel_t *chan = &priv->chans[chan_num];

  rig_debug(RIG_DEBUG_VERBOSE, "%s called\n", __FUNCTION__);

  chan->rit = rit;

  return RIG_OK;
}


int gnuradio_get_rit(RIG *rig, vfo_t vfo, shortfreq_t *rit)
{
  struct gnuradio_priv_data *priv = (struct gnuradio_priv_data *)rig->state.priv;
  int chan_num = vfo2chan_num(rig, vfo);
  channel_t *chan = &priv->chans[chan_num];

  *rit = chan->rit;
  rig_debug(RIG_DEBUG_VERBOSE, "%s called\n", __FUNCTION__);

  return RIG_OK;
}


/*
 * nothing much to be done but remembering
 */
int gnuradio_set_ts(RIG *rig, vfo_t vfo, shortfreq_t ts)
{
  struct gnuradio_priv_data *priv = (struct gnuradio_priv_data *)rig->state.priv;
  int chan_num = vfo2chan_num(rig, vfo);
  channel_t *chan = &priv->chans[chan_num];

  rig_debug(RIG_DEBUG_VERBOSE, "%s called\n", __FUNCTION__);
  chan->tuning_step = ts;

  return RIG_OK;
}


int gnuradio_get_ts(RIG *rig, vfo_t vfo, shortfreq_t *ts)
{
  struct gnuradio_priv_data *priv = (struct gnuradio_priv_data *)rig->state.priv;
  int chan_num = vfo2chan_num(rig, vfo);
  channel_t *chan = &priv->chans[chan_num];

  rig_debug(RIG_DEBUG_VERBOSE, "%s called\n", __FUNCTION__);
  *ts = chan->tuning_step;

  return RIG_OK;
}


int gnuradio_vfo_op(RIG *rig, vfo_t vfo, vfo_op_t op)
{
  freq_t freq;
  shortfreq_t ts;
  int ret = RIG_OK;

  rig_debug(RIG_DEBUG_VERBOSE,"%s called: %s\n",__FUNCTION__,
				  rig_strvfop(op));

  switch (op) {
  case RIG_OP_UP:
	  ret = gr_get_freq(rig, vfo, &freq);
	  if (ret != RIG_OK) break;
	  ret = gnuradio_get_ts(rig, vfo, &ts);
	  if (ret != RIG_OK) break;
	  ret = gr_set_freq(rig, vfo, freq+ts);	/* up */
	  break;
  case RIG_OP_DOWN:
	  ret = gr_get_freq(rig, vfo, &freq);
	  if (ret != RIG_OK) break;
	  ret = gnuradio_get_ts(rig, vfo, &ts);
	  if (ret != RIG_OK) break;
	  ret = gr_set_freq(rig, vfo, freq-ts);	/* down */
	  break;
  default:
	  break;
  }

  return ret;
}

DECLARE_INITRIG_BACKEND(gnuradio)
{
	rig_debug(RIG_DEBUG_VERBOSE, "gnuradio: _init called\n");

	rig_register(&gr_caps);
	rig_register(&mc4020_caps);
	rig_register(&graudio_caps);
	rig_register(&graudioiq_caps);

	return RIG_OK;
}

