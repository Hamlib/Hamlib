/*
 *  Hamlib GNUradio backend - Demodulator chain class
 *  Copyright (c) 2003-2004 by Stephane Fillod
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

#ifndef _DEMOD_H
#define _DEMOD_H 1


#include <VrSigProc.h>
#include <GrFreqXlatingFIRfilterCCF.h>
#include <GrFIRfilterFFF.h>
#include <VrAmp.h>

//#include <GrAGC.h>
#include <HrAGC.h>

#include <gr_firdes.h>
#include <gr_fir_builderF.h>

#include "hamlib/rig.h"
#include "misc.h"


#define d_iType VrComplex
#define d_oType float

class DemodChainCF {
  public:
	DemodChainCF (VrSource<d_iType> *d_source, VrSink<d_oType> *d_sink, rmode_t mode, pbwidth_t width, int input_rate, freq_t centerfreq = 0) :
		d_source(d_source), d_sink(d_sink), mode(mode), width(width), input_rate(input_rate), centerfreq(centerfreq), rf_gain(1.0), if_width_of_transition_band(1000), CFIRdecimate(1), RFIRdecimate(1) {}
	virtual ~DemodChainCF () { delete agc; delete mixer; delete gainstage; delete audio_filter; }

	virtual const char *name() { return rig_strrmode(mode); }

	virtual void connect (void);
	void setWidth (pbwidth_t width);
	void setFreq (freq_t centerfreq);

	rmode_t mode;
	pbwidth_t width;

  protected:
	VrSource<d_iType> *d_source;
	VrSink<d_oType> *d_sink;

	VrSigProc *demod_in;
	VrSigProc *demod_out;

	HrAGC<d_oType,d_oType> *agc;
	GrFreqXlatingFIRfilterCCF *mixer;
	VrAmp<d_oType,d_oType> *gainstage;
	GrFIRfilterFFF *audio_filter;

	int input_rate;
	freq_t centerfreq;
	float rf_gain;
	double if_width_of_transition_band;
	int CFIRdecimate;
	int RFIRdecimate;

};


void DemodChainCF::connect()
{
  // change_filt();
     vector<float> channel_coeffs =
      gr_firdes::low_pass (rf_gain,			// gain
			   input_rate,			// sampling rate
			   width/2,			// low-pass cutoff freq
			   if_width_of_transition_band,	// width of transition band
			   gr_firdes::WIN_HAMMING);

     rig_debug(RIG_DEBUG_VERBOSE, "Number of channel_coeffs: %d\n", channel_coeffs.size ());

     vector<float> audio_coeffs =
      gr_firdes::band_pass (1,				// gain
			   input_rate,			// sampling rate
			   300,
			   width,
			   250,	// width of transition band
			   gr_firdes::WIN_HAMMING);

     rig_debug(RIG_DEBUG_VERBOSE, "Number of audio_coeffs: %d\n", audio_coeffs.size ());


  mixer = new GrFreqXlatingFIRfilterCCF (CFIRdecimate, channel_coeffs, centerfreq);
  agc = new HrAGC<d_oType,d_oType>(1e-2);
  gainstage = new VrAmp<float,float>(1);	/* AF */
  audio_filter = new GrFIRfilterFFF (RFIRdecimate, audio_coeffs);

  NWO_CONNECT (d_source, mixer);
  NWO_CONNECT (mixer, demod_in);
  NWO_CONNECT (demod_out, agc);
  NWO_CONNECT (agc, gainstage);
  NWO_CONNECT (gainstage, audio_filter);
  NWO_CONNECT (audio_filter, d_sink);

}

void DemodChainCF::setFreq (freq_t centerfreq)
{
	rig_debug(RIG_DEBUG_TRACE,"%s: %lld\n",__FUNCTION__, centerfreq);
	mixer->setCenterFreq(centerfreq);
}

void DemodChainCF::setWidth (pbwidth_t width)
{
  // change_filt();
     vector<float> channel_coeffs =
      gr_firdes::low_pass (1.0,				// gain
			   input_rate,			// sampling rate
			   width/2,			// low-pass cutoff freq
			   if_width_of_transition_band,	// width of transition band
			   gr_firdes::WIN_HAMMING);

     rig_debug(RIG_DEBUG_VERBOSE, "%s: Number of channel_coeffs: %d\n", __FUNCTION__, channel_coeffs.size ());

     vector<float> audio_coeffs =
      gr_firdes::band_pass (1,				// gain
			   input_rate,			// sampling rate
			   300,
			   width,
			   250,			// width of transition band
			   gr_firdes::WIN_HAMMING);

     rig_debug(RIG_DEBUG_VERBOSE, "Number of audio_coeffs: %d\n", audio_coeffs.size ());


     mixer->setTaps(channel_coeffs);
     audio_filter->setTaps(audio_coeffs);
}


#endif	/* _DEMOD_H */
