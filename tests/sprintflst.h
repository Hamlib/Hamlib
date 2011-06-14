/*
 *  Hamlib Interface - sprintf toolbox header
 *  Copyright (c) 2003-2008 by Stephane Fillod
 *
 *	$Id: sprintflst.h,v 1.2 2008-05-23 14:26:09 fillods Exp $
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

#ifndef _SPRINTFLST_H
#define _SPRINTFLST_H 1

#include <hamlib/rig.h>

#define SPRINTF_MAX_SIZE 256

__BEGIN_DECLS

extern int sprintf_mode(char *str, rmode_t);
extern int sprintf_vfo(char *str, vfo_t);
extern int sprintf_func(char *str, setting_t);
extern int sprintf_level(char *str, setting_t);
extern int sprintf_level_ext(char *str, const struct confparams *);
extern int sprintf_level_gran(char *str, setting_t, const gran_t gran[]);
extern int sprintf_parm(char *str, setting_t);
extern int sprintf_parm_gran(char *str, setting_t, const gran_t gran[]);
extern int sprintf_vfop(char *str, vfo_op_t);
extern int sprintf_scan(char *str, scan_t);

__END_DECLS

#endif /* _SPRINTFLST_H */
