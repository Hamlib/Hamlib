/*
 *  Hamlib GNUradio backend - gnuradio priv structure
 *  Copyright (c) 2001-2003 by Stephane Fillod
 *
 *	$Id: gr_priv.h,v 1.5 2003-09-28 15:59:27 fillods Exp $
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

#ifndef _GR_PRIV_H
#define _GR_PRIV_H 1

#include <VrSigSource.h>
#include <VrSink.h>
#include <VrMultiTask.h>
#include <VrFixOffset.h>
#include <GrFreqXlatingFIRfilterSCF.h>
#include <GrFreqXlatingFIRfilterCCF.h>
#include <GrFIRfilterFSF.h>
#include <VrQuadratureDemod.h>	/* FM */
//#include <VrAmplitudeDemod.h>	/* AM */
#include <GrIIRfilter.h>
#include <GrConvertFS.h>
#include <VrAmp.h>

/* SSB mod */
//#include <GrSSBMod.h>
//#include <GrHilbert.h>

#include <pthread.h>

#include <hamlib/rig.h>

#define IOTYPE short

struct mod_data {
	freq_t centerfreq;
	GrFreqXlatingFIRfilterCCF *mixer;
	VrAmp<float,float> *gainstage;
	VrSigProc *demod;

	int CFIRdecimate;
	int CFIRdecimate2;
	int RFIRdecimate;
	GrFreqXlatingFIRfilterSCF *chan_filter;
	GrFIRfilterFSF *audio_filter;
	GrFIRfilterFFF *audioF_filter;
};

#define NUM_CHAN 2	/* VFO A and VFO B */

struct gnuradio_priv_data {
	/* current vfo already in rig_state ? */
	vfo_t curr_vfo;

	value_t parms[RIG_SETTING_MAX];

	channel_t chans[NUM_CHAN];

	/* tuner providing IF */
	rig_model_t tuner_model;
	RIG *tuner;
	shortfreq_t input_rate;
	shortfreq_t IF_center_freq;

	VrSource<VrComplex> *source;	/*< IF source */
	VrFixOffset<IOTYPE,IOTYPE> *offset_fixer;	/*< some sources need it */
	VrSink<float> *sink;		/*< Audio sink */
	VrMultiTask *m;
	int need_fixer;		/*< always need Offset fixer ? */

	pthread_t process_thread;
	pthread_mutex_t mutex_process;
	volatile int do_process;	/*< flag to tell process thread to stop */

	struct mod_data mods[NUM_CHAN];	/*< Modulation objects and stuff, one per channel */
};

//#define GR_SOURCE(priv) ((priv)->need_fixer?(priv)->offset_fixer:(priv)->source)
//#define GR_SOURCE(priv) ((priv)->offset_fixer)
#define GR_SOURCE(priv) ((priv)->source)

//#define GR_MAX_FREQUENCY(priv) ((priv)->input_rate/2)
#define GR_MAX_FREQUENCY(priv) ((priv)->input_rate)

#endif	/* _GR_PRIV_H */
