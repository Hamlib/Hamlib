/*
 *  Hamlib GNUradio backend - SSB class
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

#ifndef _SSB_H
#define _SSB_H 1

#include "demod.h"

#include <GrSSBMod.h>		/* SSB */


class USBDemodChainCF : public DemodChainCF {
  private:
	GrSSBMod<d_oType> *s_demod;
	float low_cutoff;

  public:
	USBDemodChainCF (VrSource<d_iType> *src, VrSink<d_oType> *snk, rmode_t mode, pbwidth_t width, int input_rate, freq_t centerfreq = 0) :
		DemodChainCF(src, snk, mode, width, input_rate, centerfreq) {

		low_cutoff = 300;
		// centerfreq, relative to IF_center_freq
		centerfreq += (freq_t)(low_cutoff + width/2);

		s_demod = new GrSSBMod<d_oType>(2*M_PI*(low_cutoff+width/2)/(double)input_rate,1.0);

		demod_in = demod_out = s_demod;
	}
	~USBDemodChainCF() { delete s_demod; }

	//void setWidth(pbwidth_t width) { }
	void setFreq(freq_t centerfreq) { s_demod->set_freq(centerfreq); }
	void setRFgain(float gain) { s_demod->set_gain(gain); }
};

class LSBDemodChainCF : public DemodChainCF {
  private:
	GrSSBMod<d_oType> *s_demod;
	float low_cutoff;

  public:
	LSBDemodChainCF (VrSource<d_iType> *src, VrSink<d_oType> *snk, rmode_t mode, pbwidth_t width, int input_rate, freq_t centerfreq = 0) :
		DemodChainCF(src, snk, mode, width, input_rate, centerfreq) {

		float low_cutoff = 300;
		// centerfreq, relative to IF_center_freq
		centerfreq += (freq_t)(-low_cutoff - width/2);

		s_demod = new GrSSBMod<d_oType>(-2*M_PI*(low_cutoff+width/2)/(double)input_rate,1.0);

		demod_in = demod_out = s_demod;
	}
	~LSBDemodChainCF() { delete s_demod; }

	//void setWidth(pbwidth_t width) { }
	void setFreq(freq_t centerfreq) { s_demod->set_freq(centerfreq); }
	void setRFgain(float gain) { s_demod->set_gain(gain); }
};


#endif	/* _SSB_H */
