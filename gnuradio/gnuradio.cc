/* -*- Mode: c++ -*- */
/*
 *  Hamlib GNUradio backend - main file
 *  Copyright (c) 2001-2003 by Stephane Fillod
 *
 *	$Id: gnuradio.cc,v 1.4 2003-04-06 18:50:21 fillods Exp $
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
#include <GrFreqXlatingFIRfilterSCF.h>
#include <GrFreqXlatingFIRfilterCCF.h>
#include <GrFIRfilterFSF.h>
#include <GrFIRfilterFFF.h>
#include <VrQuadratureDemod.h>	/* FM */
//#include <VrAmplitudeDemod.h>	/* AM */
/* SSB mod */
//#include <GrSSBMod.h>
//#include <GrHilbert.h>

#include <gr_firdes.h>
#include <gr_fir_builderF.h>


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


  /* ** Source ** */
  
  // --> short

  priv->source = new VrFileSource<short>(priv->input_rate, "/tmp/fm95_5_half.dat", true);
  // Chirp
  if (!priv->source)
  	priv->source = new VrSigSource<IOTYPE>(priv->input_rate, VR_SIN_WAVE, CARRIER_FREQ, AMPLITUDE);
  //new VrChirpSource<IOTYPE>(priv->input_rate, AMPLITUDE, 4);
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
	priv->source = new GrMC4020Source<short>(priv->input_rate, MCC_CH3_EN | MCC_ALL_1V);

	return gr_open(rig);
}


/*
 * sound card source override
 */
int graudio_open(RIG *rig)
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
 * tuner_freq is the display freq on tuner of the IF in digital domain
 * freq is the desired freq
 */
static int update_freq(RIG *rig, unsigned chan_num, freq_t tuner_freq, freq_t freq)
{
  struct gnuradio_priv_data *priv = (struct gnuradio_priv_data *)rig->state.priv;
  channel_t *chan = &priv->chans[chan_num];
  struct mod_data *mod = &priv->mods[chan_num];
  double freq_offset;

  /*
   * In case the tuner is not a real tuner
   */
  if (priv->tuner_model == RIG_MODEL_DUMMY)
	  tuner_freq = priv->IF_center_freq;

  freq_offset = (double) (freq - tuner_freq);
  rig_debug(RIG_DEBUG_VERBOSE, "%s: %lld %lld freq_offset=%g\n",
		  __FUNCTION__, tuner_freq, freq, freq_offset);

  if (freq_offset == 0)
	  return RIG_OK;	/* nothing to do */

  /* TODO: change mode plumbing (don't forget locking!)
   * workaround?: set_mode(NONE), set_mode(previous)
   */
  pthread_mutex_lock(&priv->mutex_process);
  switch (chan->mode) {
  case RIG_MODE_WFM:
  case RIG_MODE_FM:
  case RIG_MODE_USB:
	  /* not so sure about if setCenter_Freq is the Right thing to do(tm). */
	mod->chan_filter->setCenterFreq(priv->IF_center_freq + freq_offset);
	break;
	/*
	 * set_freq for SSB mod
	 * mod->ssb.shifter->set_freq(2*M_PI*freq_offset/(double)priv->input_rate);
	 */
  case RIG_MODE_NONE:
	break;  
  default:
	  rig_debug(RIG_DEBUG_WARN, "%s: mode %s unimplemented!\n",
			  __FUNCTION__, strrmode(chan->mode));
	  break;
  }
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
 			__FUNCTION__, strvfo(vfo), fstr);

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
	  ret = rig_set_freq(priv->tuner, RIG_VFO_CURR, freq + GR_MAX_FREQUENCY(priv)/2);
	  if (ret != RIG_OK)
		  return ret;
	  /*
	   * query freq right back, because wanted freq may not be real freq,
	   * because of resolution of tuner
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
  struct mod_data *mod = &priv->mods[chan_num];
  char buf[16];
  freq_t tuner_freq;
  int ret = RIG_OK;
  double freq_offset;

  if (width == RIG_PASSBAND_NORMAL)
	  width = rig_passband_normal(rig, mode);
  sprintf_freq(buf, width);
  rig_debug(RIG_DEBUG_VERBOSE,"%s called: %s %s %s\n", 
  		__FUNCTION__, strvfo(vfo), strrmode(mode), buf);

  if (mode == chan->mode && width == chan->width)
	  return RIG_OK;

  /*
   * TODO: check if only change in width
   */


  ret = rig_get_freq(priv->tuner, RIG_VFO_CURR, &tuner_freq);
  if (ret != RIG_OK)
	  return ret;
  /*
   * In case the tuner is not a real tuner
   */
  if (priv->tuner_model == RIG_MODEL_DUMMY)
	  tuner_freq = 0;

  freq_offset = (double) (chan->freq - tuner_freq);
  rig_debug(RIG_DEBUG_VERBOSE, "%s: freq_offset=%g\n",
		  __FUNCTION__, freq_offset);

  pthread_mutex_lock(&priv->mutex_process);

  /* TODO: destroy GNUradio connections beforehand if different mode? */
  if (chan->mode != RIG_MODE_NONE && mode != chan->mode) {

  	switch (chan->mode) {
	  case RIG_MODE_FM:
	  case RIG_MODE_WFM:
		delete mod->demod.wfm.demod;
		delete mod->chan_filter;
		delete mod->audio_filter;
		break;
	  case RIG_MODE_USB:
		delete mod->audio_filter;
		delete mod->chan_filter;
		break;
	  case RIG_MODE_AM:
		delete mod->chan_filter;
		delete mod->audio_filter;
	  default:
		break;
	  }
  }

  /* TODO: if same mode, but different width, then adapt */
 
  priv->m->stop();

  switch (mode) {
  case RIG_MODE_FM:
	  {
  mod->CFIRdecimate = 20;
  mod->CFIRdecimate2 = 25;
  mod->RFIRdecimate = 5;

const float channelSpacing = 80e3;
const float demodBW = 3e3;
const float TAU = 75e-6;  // 75us in US, 50us in EUR  

const int quadRate = priv->input_rate / mod->CFIRdecimate / mod->CFIRdecimate2;
const int audioRate = quadRate / mod->RFIRdecimate;

   
    // 
    // design first stage selection filter
    //
    
    // width of transition band.
    //
    // We make this twice as wide as you would think.  This allows
    // some aliasing, but it's outside our final bandwidth, determined
    // by the next filter

    float transition_width = (priv->input_rate / mod->CFIRdecimate - width);

    vector<float> cs1_coeffs =
      gr_firdes::low_pass (1.0,				// gain
			   priv->input_rate,			// sampling rate
			   width / 2,		// low-pass cutoff freq
			   transition_width,
			   gr_firdes::WIN_HANN);
    
    cerr << "Number of cs1_coeffs: " << cs1_coeffs.size () << endl;
    
    //
    // design second stage channel selection filter
    //
    // This is the one that we tune
    //
    vector<float> cs2_coeffs =
      gr_firdes::low_pass (1.0,
			   priv->input_rate / mod->CFIRdecimate,
			   width / 2,
			   (channelSpacing - width) / 2,
			   gr_firdes::WIN_HANN);
    
    cerr << "Number of cs2_coeffs: " << cs2_coeffs.size () << endl;
    
    //
    // design audio filter
    //
    vector<float> audio_coeffs =
      gr_firdes::band_pass (1e3,			// gain
			    quadRate,			// sampling rate
			    300,
			    demodBW,
			    250,
			    gr_firdes::WIN_HAMMING);
    
    cerr << "Number of audio_coeffs: " << audio_coeffs.size () << endl;
    
    //
    // design demphasis filter
    //
    vector<double> fftaps = vector<double>(2);
    vector<double> fbtaps = vector<double>(2);
    
    fftaps[0] = 1 - exp(-1/(TAU*audioRate));
    fftaps[1] = 0;
    fbtaps[0] = 0;
    fbtaps[1] = exp(-1/TAU/priv->input_rate*50);;

    // 
    // now instantiate the modules
    //

    // short --> VrComplex
    mod->chan_filter = 
    		new GrFreqXlatingFIRfilterSCF (mod->CFIRdecimate,
						cs1_coeffs,
						priv->IF_center_freq - freq_offset);
    
    // VrComplex --> VrComplex
    mod->demod.fm.chan2_filter = 
    		new GrFreqXlatingFIRfilterCCF (mod->CFIRdecimate2, cs2_coeffs, 0);
   

    // VrComplex --> float
    mod->demod.fm.demod = new VrQuadratureDemod<float>(1);
    

    // float --> float
    mod->audioF_filter = new GrFIRfilterFFF (mod->RFIRdecimate, audio_coeffs);
    
    // float --> float
    mod->demod.fm.deemph =
      new GrIIRfilter<float,float,double> (1,fftaps,fbtaps); 
    
    // float --> short
    mod->demod.fm.cfs = new GrConvertFS ();
    
    //connect the modules together
    
	NWO_CONNECT (GR_SOURCE(priv), mod->chan_filter);
	NWO_CONNECT (mod->chan_filter, mod->demod.fm.chan2_filter);
	NWO_CONNECT (mod->demod.fm.chan2_filter, mod->demod.fm.demod);
	NWO_CONNECT (mod->demod.fm.demod, mod->audioF_filter);
	NWO_CONNECT (mod->audioF_filter, mod->demod.fm.deemph);
	NWO_CONNECT (mod->demod.fm.deemph, mod->demod.fm.cfs);
	NWO_CONNECT (mod->demod.fm.cfs, priv->sink);

	break;
	  }

  case RIG_MODE_WFM:
	  {
  mod->CFIRdecimate = 125;
  mod->RFIRdecimate = 5;

	const int quadRate = priv->input_rate / mod->CFIRdecimate;
	const int audioRate = quadRate / mod->RFIRdecimate;

	rig_debug(RIG_DEBUG_VERBOSE, "Input Sampling Rate: %d\n", priv->input_rate);
	rig_debug(RIG_DEBUG_VERBOSE, "Complex FIR decimation factor: %d\n", mod->CFIRdecimate);
	rig_debug(RIG_DEBUG_VERBOSE, "QuadDemod Sampling Rate: %d\n", quadRate);
	rig_debug(RIG_DEBUG_VERBOSE, "Real FIR decimation factor: %d\n", mod->RFIRdecimate);
	rig_debug(RIG_DEBUG_VERBOSE, "Audio Sampling Rate: %d\n", audioRate);

 
    // build channel filter
    //
    // note that the totally bogus transition width is because
    // we don't have enough mips right now to really do the right thing.
    // This results in a filter with 83 taps, which is just a few
    // more than the original 75 in microtune_fm_demo.
     vector<float> channel_coeffs =
      gr_firdes::low_pass (1.0,				// gain
			   priv->input_rate,		// sampling rate
			   width,			// low-pass cutoff: freq 250e3
			   8*100e3,			// width of transition band
			   gr_firdes::WIN_HAMMING);

	rig_debug(RIG_DEBUG_VERBOSE, "Number of channel_coeffs: %d\n", channel_coeffs.size ());
  
    // short --> VrComplex
    mod->chan_filter = 
	    new GrFreqXlatingFIRfilterSCF(mod->CFIRdecimate, channel_coeffs, 
			    priv->IF_center_freq - freq_offset);


    // float --> short
    double width_of_transition_band = audioRate / 32;
    vector<float> audio_coeffs =
      gr_firdes::low_pass (1.0,				// gain
			   quadRate,			// sampling rate
			   audioRate/2 - width_of_transition_band, // low-pass cutoff freq
			   width_of_transition_band,
			   gr_firdes::WIN_HAMMING);

	rig_debug(RIG_DEBUG_VERBOSE, "Number of audio_coeffs: %d\n", audio_coeffs.size ());
	rig_debug(RIG_DEBUG_VERBOSE, "Low-pass cutoff freq: %d\n", audioRate/2 - (int)width_of_transition_band);
  
    mod->audio_filter = 
      new GrFIRfilterFSF(mod->RFIRdecimate, audio_coeffs);

  	mod->demod.wfm.FMdemodGain = 2200;
	float volume = chan->levels[rig_setting2idx(RIG_LEVEL_AF)].f;

	//
	// setup Wide FM demodulator chain
	//
    // VrComplex --> float
    mod->demod.wfm.demod =
      new VrQuadratureDemod<float>(volume * mod->demod.wfm.FMdemodGain);

    //connect the modules together

	NWO_CONNECT (GR_SOURCE(priv), mod->chan_filter);
	NWO_CONNECT (mod->chan_filter, mod->demod.wfm.demod);
	NWO_CONNECT (mod->demod.wfm.demod, mod->audio_filter);
	NWO_CONNECT (mod->audio_filter, priv->sink);

	break;
	  }

  case RIG_MODE_USB:
	  {
	float rf_gain = chan->levels[rig_setting2idx(RIG_LEVEL_RF)].f;


#if 0
	 //SSB mod:
	mod->ssb.hilb = new GrHilbert<short>(31);	/* what's that 31? */
	mod->ssb.shifter = new GrSSBMod<short>(2*M_PI*freq_offset/(double)priv->input_rate,
			rf_gain);
#endif

	//connect the modules together
	NWO_CONNECT (GR_SOURCE(priv), mod->chan_filter);
	NWO_CONNECT (mod->chan_filter, mod->demod.fm.demod);
	NWO_CONNECT (mod->demod.fm.demod, mod->audio_filter);
	NWO_CONNECT (mod->audio_filter, priv->sink);

	break;
	  }

#if 0
  case RIG_MODE_AM:
	  {
	float volume = chan->levels[rig_setting2idx(RIG_LEVEL_AF)].f;

	//
	// setup Wide FM demodulator chain
	//

    // VrComplex --> float
    mod->am.demod =
      new VrAmplitudeDemod<float>(0.0, 0.05);

    //connect the modules together

	NWO_CONNECT (GR_SOURCE(priv), mod->chan_filter);
	NWO_CONNECT (mod->chan_filter, mod->am.demod);
	NWO_CONNECT (mod->am.demod, mod->audio_filter);
	NWO_CONNECT (mod->audio_filter, priv->sink);

	break;
	  }
#endif

  case RIG_MODE_NONE:
	  /* ez */
	break;  

  default:
	  ret = -RIG_EINVAL;
  }

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

static int set_rf_gain(RIG *rig, channel_t *chan, struct mod_data *mod, float gain)
{
  struct gnuradio_priv_data *priv = (struct gnuradio_priv_data *)rig->state.priv;

  pthread_mutex_lock(&priv->mutex_process);
  switch (chan->mode) {
  case RIG_MODE_USB:
	  {
	//mod->ssb.shifter->set_gain(gain);
	break;
	  }
  case RIG_MODE_NONE:
	break;  
  case RIG_MODE_WFM:
  case RIG_MODE_FM:
	  /* chan_filter/VrComplexFIRfilter is missing a setGain method! */
  default:
	  rig_debug(RIG_DEBUG_WARN, "%s: mode %s unimplemented!\n",
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
  struct mod_data *mod = &priv->mods[chan_num];
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
	rig_debug(RIG_DEBUG_WARN, "%s: level %s unimplemented!\n",
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
				  strvfop(op));

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

int initrigs_gnuradio(void *be_handle)
{
	rig_debug(RIG_DEBUG_VERBOSE, "gnuradio: _init called\n");

	rig_register(&gr_caps);
	rig_register(&mc4020_caps);
	rig_register(&graudio_caps);

	return RIG_OK;
}

#if 0
    VrGUI *guimain = 0;
    VrGUILayout *horiz = 0;
    VrGUILayout *vert = 0;


    if (use_gui_p){
      guimain = new VrGUI(argc, argv);
      horiz = guimain->top->horizontal();
      vert = horiz->vertical();
    }

    VrSink<VrComplex> *fft_sink1 = 0;
    VrSink<float> *fft_sink2 = 0;
    VrSink<short> *fft_sink3 = 0;

    if (use_gui_p){
      // sink1 is channel filter output
      fft_sink1 = new GrFFTSink<VrComplex>(vert, 50, 130, 512);

      // sink2 is fm demod output
      fft_sink2 = new GrFFTSink<float>(vert, 40, 140, 512);

      // sink3 is audio output
      fft_sink3 = new GrFFTSink<short>(horiz, 40, 140, 512);
    }

    if (use_gui_p)
      NWO_CONNECT (chan_filter, fft_sink1);

    if (use_gui_p)
      NWO_CONNECT (demod, fft_sink2);

    if (use_gui_p)
      NWO_CONNECT (audio_filter, fft_sink3);

    VrMultiTask *m = new VrMultiTask ();
    if (use_gui_p){
      m->add (fft_sink1);
      m->add (fft_sink3);
      m->add (fft_sink2);
    }

    m->add (final_sink);

        if (use_gui_p)
      guimain->start ();

    
    while (1){
      if (use_gui_p)
	guimain->processEvents(10 /*ms*/);

#endif
