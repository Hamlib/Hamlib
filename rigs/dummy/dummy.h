/*
 *  Hamlib Dummy backend - main header
 *  Copyright (c) 2001-2009 by Stephane Fillod
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

#ifndef _DUMMY_H
#define _DUMMY_H 1

#include "hamlib/rig.h"
#include "token.h"

/* backend conf */
#define TOK_CFG_MAGICCONF    TOKEN_BACKEND(1)
#define TOK_CFG_STATIC_DATA  TOKEN_BACKEND(2)


/* ext_level's and ext_parm's tokens */
#define TOK_EL_MAGICLEVEL    TOKEN_BACKEND(1)
#define TOK_EL_MAGICFUNC     TOKEN_BACKEND(2)
#define TOK_EL_MAGICOP       TOKEN_BACKEND(3)
#define TOK_EP_MAGICPARM     TOKEN_BACKEND(4)
#define TOK_EL_MAGICCOMBO    TOKEN_BACKEND(5)
#define TOK_EL_MAGICEXTFUNC  TOKEN_BACKEND(6)


extern struct rig_caps dummy_caps;
extern struct rig_caps dummy_no_vfo_caps;
extern struct rig_caps netrigctl_caps;
extern const struct rig_caps flrig_caps;
extern const struct rig_caps trxmanager_caps;
extern const struct rig_caps tci1x_caps;

int netrigctl_get_vfo_mode(RIG *);

#endif /* _DUMMY_H */
