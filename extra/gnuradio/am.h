/*
 *  Hamlib GNUradio backend - AM class
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
};

#endif	/* _AM_H */
