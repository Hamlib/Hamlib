/*
 *  Hamlib Microtune backend - main header
 *  Copyright (c) 2001-2003 by Stephane Fillod
 *
 *
 *   This library is free software; you can redistribute it and/or modify
 *   it under the terms of the GNU General Public License as published by
 *   the Free Software Foundation; either version 2, or (at your option)
 *   any later version.
 *
 *   This program is distributed in the hope that it will be useful,
 *   but WITHOUT ANY WARRANTY; without even the implied warranty of
 *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *   GNU General Public License for more details.
 *
 *   You should have received a copy of the GNU General Public License
 *   along with this software; see the file COPYING.  If not, write to
 *   the Free Software Foundation, Inc., 59 Temple Place - Suite 330,
 *   Boston, MA 02111-1307, USA.
 *
 */

#ifndef _MICRTOUNE_H
#define _MICRTOUNE_H 1

#include <hamlib/rig.h>
#include <token.h>

__BEGIN_DECLS

#define TOK_AGCGAIN TOKEN_BACKEND(1)


int microtune_init(RIG *rig);
int microtune_cleanup(RIG *rig);
int module_4937_open(RIG *rig);
int module_4702_open(RIG *rig);
int microtune_close(RIG *rig);
int microtune_set_freq(RIG *rig, vfo_t vfo, freq_t freq);
int microtune_get_freq(RIG *rig, vfo_t vfo, freq_t *freq);
int microtune_set_ext_level(RIG *rig, vfo_t vfo, token_t token, value_t val);


extern const struct rig_caps module_4937_caps;
extern const struct rig_caps module_4702_caps;


__END_DECLS

#endif /* _MICRTOUNE_H */
