/*
 *  Hamlib Kachina backend - main header
 *  Copyright (c) 2001 by Stephane Fillod
 *
 *		$Id: kachina.h,v 1.1 2001-08-12 22:52:44 f4cfe Exp $
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

#ifndef _KACHINA_H
#define _KACHINA_H 1

#if defined(__CYGWIN__)
#  undef HAMLIB_DLL
#  include <hamlib/rig.h>
#  include <cal.h>
#  define HAMLIB_DLL
#  include <hamlib/rig_dll.h>
#else
#  include <hamlib/rig.h>
#  include <cal.h>
#endif

struct kachina_priv_caps {
	cal_table_t str_cal;
};

int kachina_set_freq(RIG *rig, vfo_t vfo, freq_t freq);
int kachina_set_mode(RIG *rig, vfo_t vfo, rmode_t mode, pbwidth_t width);

extern const struct rig_caps k505dsp_caps;

extern HAMLIB_EXPORT(int) init_kachina(void *be_handle);


#endif /* _KACHINA_H */

