/*
 *  Hamlib GNUradio backend - gnuradio priv structure
 *  Copyright (c) 2001-2003 by Stephane Fillod
 *
 *	$Id: gr_priv.h,v 1.3 2003-02-09 22:54:15 fillods Exp $
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
/* FM */
#include <VrComplexFIRfilter.h>
#include <VrQuadratureDemod.h>
#include <VrRealFIRfilter.h>
/* SSB */
#include <GrSSBMod.h>
#include <GrHilbert.h>

#include <pthread.h>

#include <hamlib/rig.h>

#define IOTYPE short

union mod_data {
	struct {
		/* (W)FM demod */
		VrComplexFIRfilter<short> *chan_filter_1;
		VrQuadratureDemod<float> *fm_demod_1;
		VrRealFIRfilter<float,short> *audio_filter_1;

		/* these levels, if not in RIG_LEVEL's already,
		 * are good candidates for rig_set_ext_level
		 */
		int CFIRdecimate;
		int chanTaps;
		float chanGain;
		float FMdemodGain;
		int RFIRdecimate;
		int ifTaps;
		float ifGain;	/* LEVEL_RF? */
	} fm;

	struct {
		/* SSB demod */
		GrHilbert<short> *hilb;
		GrSSBMod<short> *shifter;
	} ssb;
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

	VrSource<IOTYPE> *source;	/*< IF source */
	VrFixOffset<IOTYPE,IOTYPE> *offset_fixer;	/*< some sources need it */
	VrSink<short> *sink;		/*< Audio sink */
	VrMultiTask *m;
	int need_fixer;		/*< always need Offset fixer ? */

	pthread_t process_thread;
	pthread_mutex_t mutex_process;
	volatile int do_process;	/*< flag to tell process thread to stop */

	union mod_data mods[NUM_CHAN];	/*< Modulation objects and stuff, one per channel */
};

#define GR_SOURCE(priv) ((priv)->need_fixer?(priv)->offset_fixer:(priv)->source)

#endif	/* _GR_PRIV_H */
