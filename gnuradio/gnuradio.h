/*
 *  Hamlib GNUradio backend - main header
 *  Copyright (c) 2001,2002 by Stephane Fillod
 *
 *	$Id: gnuradio.h,v 1.1 2002-07-06 09:27:38 fillods Exp $
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

#ifndef _GNURADIO_H
#define _GNURADIO_H 1

#include <hamlib/rig.h>

__BEGIN_DECLS

int gr_init(RIG *rig);
int gr_cleanup(RIG *rig);
int gr_open(RIG *rig);
int gr_close(RIG *rig);
int gr_set_freq(RIG *rig, vfo_t vfo, freq_t freq);
int gr_get_freq(RIG *rig, vfo_t vfo, freq_t *freq);
int gr_get_vfo(RIG *rig, vfo_t *vfo);
int gr_set_mode(RIG *rig, vfo_t vfo, rmode_t mode, pbwidth_t width);
int gr_get_mode(RIG *rig, vfo_t vfo, rmode_t *mode, pbwidth_t *width);

extern const struct rig_caps gr_caps;

extern BACKEND_EXPORT(int) initrigs_gnuradio(void *be_handle);

__END_DECLS

#endif /* _GNURADIO_H */
