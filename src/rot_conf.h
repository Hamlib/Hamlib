/*
 *  Hamlib Interface - configuration header
 *  Copyright (c) 2000,2001,2002 by Stephane Fillod and Frank Singleton
 *
 *		$Id: rot_conf.h,v 1.2 2002-01-27 14:55:30 fillods Exp $
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

#ifndef _ROT_CONF_H
#define _ROT_CONF_H 1

#include <hamlib/rotator.h>

int frontrot_set_conf(ROT *rot, token_t token, const char *val);
int frontrot_get_conf(ROT *rot, token_t token, char *val);


#endif /* _ROT_CONF_H */
