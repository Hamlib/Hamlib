/*
 *  Hamlib GNUradio backend - AM class
 *  Copyright (c) 2003 by Stephane Fillod
 *
 *	$Id: am.h,v 1.1 2003-09-28 20:51:05 fillods Exp $
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

#ifndef _AM_H
#define _AM_H 1

#include "demod.h"

#include <GrMagnitude.h>	/* AM */


class AMDemodChainCF : public DemodChainCF {
  private:
	GrMagnitude<d_iType, d_oType> *a_demod;

  public:
	AMDemodChainCF (VrSource<d_iType> *src, VrSink<d_oType> *snk, rmode_t mode, pbwidth_t width, int input_rate, freq_t centerfreq = 0) : 
		DemodChainCF(src, snk, mode, width, input_rate, centerfreq) { 

		demod_in = demod_out = a_demod = new GrMagnitude<d_iType, d_oType>();
	}
	~AMDemodChainCF() { delete a_demod; }

	//void setWidth(pbwidth_t width) { }
	//void setFreq(freq_t centerfreq) { }
	void setRFgain(float gain) { /* a_demod->setGain(gain); */ }
};

#endif	/* _AM_H */
