/*
 *  Hamlib GNUradio backend - gnuradio priv structure
 *  Copyright (c) 2001,2002 by Stephane Fillod
 *
 *	$Id: gr_priv.h,v 1.1 2002-07-06 09:27:38 fillods Exp $
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

/*
 * Simple backend of a chirp source.
 */

#include <VrSigSource.h>
#include <VrSink.h>
#include <VrMultiTask.h>

#include <hamlib/rig.h>

#define IOTYPE float

struct gnuradio_priv_data {
	/* current vfo already in rig_state ? */
	vfo_t curr_vfo;
	vfo_t last_vfo;	/* VFO A or VFO B, when in MEM mode */

	value_t parms[RIG_SETTING_MAX];

	channel_t *curr;	/* points to vfo_a, vfo_b or mem[] */
	channel_t vfo_a;

	VrSigSource<IOTYPE> *source;
	VrSink<short> *sink;
	VrMultiTask *m;
};


#endif
