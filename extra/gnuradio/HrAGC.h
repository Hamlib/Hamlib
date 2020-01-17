/* -*- c++ -*- */
/*
 * Copyright 2002 Free Software Foundation, Inc.
 * 
 * This file is part of GNU Radio
 * 
 * GNU Radio is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2, or (at your option)
 * any later version.
 * 
 * GNU Radio is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 * 
 * You should have received a copy of the GNU General Public License
 * along with GNU Radio; see the file COPYING.  If not, write to
 * the Free Software Foundation, Inc., 59 Temple Place - Suite 330,
 * Boston, MA 02111-1307, USA.
 */


#ifndef _HRAGC_H_
#define _HRAGC_H_

#include <VrSigProc.h>

template<class iType,class oType> 
class HrAGC : public VrSigProc {
protected:
  oType k;
  oType reference;
  oType gain;
  virtual void initialize();
public: 
  virtual const char *name() { return "HrAGC"; }
  virtual int work(VrSampleRange output, void *ao[],
		   VrSampleRange inputs[], void *ai[]);
  HrAGC(double k):VrSigProc(1,sizeof(iType),sizeof(oType)),k(k) { }
};

template<class iType,class oType> int
HrAGC<iType,oType>::work(VrSampleRange output, void *ao[],
		VrSampleRange inputs[], void *ai[])
{
  iType **i = (iType **)ai;
  oType **o = (oType **)ao;
  int size = output.size;

  // for normalize
  iType max = 0;
#if 1
  { int sz;
	  for (sz = 0; sz < size; sz++)
		  if (fabs(*(i[0]+sz)) > max)
			  max = fabs(*(i[0]+sz));
  }
#endif

  while(size -- > 0) {
#if 0
    power = power + ((*i[0] * *i[0]) - power)/k;
    inv_gain = sqrt(power);
    *o[0]++ = (*i[0]++ / inv_gain);
#elif 0
    /* AGC with malus on saturation */

    oType output = (*i[0]++ * gain);
    if (output < reference) {
    	*o[0]++ = output;
    	gain += (reference - fabsf(output)) * k;
    } else {
    	*o[0]++ = reference;
    	gain *= reference/fabsf(output);
    }
#else
    /* AGC with normalization */

    gain = 0.8 / max;
    *o[0]++ = (*i[0]++ * gain);
#endif
  }
  return output.size;
}

template<class iType,class oType> void
HrAGC<iType,oType>::initialize()  
{
  reference = 1;
  gain = 1;
}
#endif
