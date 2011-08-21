/*
 *  Hamlib GNUradio backend - Wide FM class
 *  Copyright (c) 2003 by Stephane Fillod
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

#ifndef _WFM_H
#define _WFM_H 1

#include "demod.h"

#include <VrQuadratureDemod.h>	/* WFM */
#include <GrFIRfilterFFF.h>


class WFMDemodChainCF : public DemodChainCF {
  private:
	VrQuadratureDemod<d_oType> *q_demod;
	int RFIRdecimate;
	GrFIRfilterFFF *audio_filter;

  public:
	WFMDemodChainCF (VrSource<d_iType> *src, VrSink<d_oType> *snk, rmode_t mode, pbwidth_t width, int input_rate, freq_t centerfreq = 0) :
		DemodChainCF(src, snk, mode, width, input_rate, centerfreq) {

		CFIRdecimate = 125;
		RFIRdecimate = 5;

		const int quadRate = input_rate / CFIRdecimate;
		const int audioRate = quadRate / RFIRdecimate;

		rig_debug(RIG_DEBUG_VERBOSE, "Input Sampling Rate: %d\n", input_rate);
		rig_debug(RIG_DEBUG_VERBOSE, "Complex FIR decimation factor: %d\n", CFIRdecimate);
		rig_debug(RIG_DEBUG_VERBOSE, "QuadDemod Sampling Rate: %d\n", quadRate);
		rig_debug(RIG_DEBUG_VERBOSE, "Real FIR decimation factor: %d\n", RFIRdecimate);
		rig_debug(RIG_DEBUG_VERBOSE, "Audio Sampling Rate: %d\n", audioRate);


		// build channel filter
		//
		// note that the totally bogus transition width is because
		// we don't have enough mips right now to really do the right thing.
		// This results in a filter with 83 taps, which is just a few
		// more than the original 75 in microtune_fm_demo.

		if_width_of_transition_band = 8*100e3;

		// float --> float
		double width_of_transition_band = audioRate / 32;
		vector<float> audio_coeffs =
		  gr_firdes::low_pass (1.0,			// gain
				   quadRate,			// sampling rate
				   audioRate/2 - width_of_transition_band, // low-pass cutoff freq
				   width_of_transition_band,
				   gr_firdes::WIN_HAMMING);

		rig_debug(RIG_DEBUG_VERBOSE, "Number of audio_coeffs: %d\n", audio_coeffs.size ());
		rig_debug(RIG_DEBUG_VERBOSE, "Low-pass cutoff freq: %d\n", audioRate/2 - (int)width_of_transition_band);

		audio_filter = new GrFIRfilterFFF(RFIRdecimate, audio_coeffs);

		q_demod = new VrQuadratureDemod<d_oType>(1.0);

		demod_in = q_demod;
		demod_out = audio_filter;
		NWO_CONNECT(demod_in, audio_filter);
	}

	~WFMDemodChainCF() { delete q_demod; delete audio_filter; }


	//void setWidth(pbwidth_t width) { }
	//void setFreq(freq_t centerfreq) { }
	void setRFgain(float gain) { q_demod->setGain(gain); }
};

#endif	/* _WFM_H */
