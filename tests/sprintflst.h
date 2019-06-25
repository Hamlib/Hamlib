/*
 *  Hamlib Interface - sprintf toolbox header
 *  Copyright (c) 2003-2008 by Stephane Fillod
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

#ifndef _SPRINTFLST_H
#define _SPRINTFLST_H 1

#include <hamlib/rig.h>

#define SPRINTF_MAX_SIZE 256

__BEGIN_DECLS

extern int sprintf_mode(char *str, rmode_t);
extern int sprintf_vfo(char *str, vfo_t);
extern int sprintf_ant(char *str, ant_t);
extern int sprintf_func(char *str, setting_t);
extern int sprintf_level(char *str, setting_t);
extern int sprintf_level_amp(char *str, setting_t);
extern int sprintf_level_ext(char *str, const struct confparams *);
extern int sprintf_level_gran(char *str, setting_t, const gran_t gran[]);
extern int sprintf_parm(char *str, setting_t);
extern int sprintf_parm_gran(char *str, setting_t, const gran_t gran[]);
extern int sprintf_vfop(char *str, vfo_op_t);
extern int sprintf_scan(char *str, scan_t);
extern char *get_rig_conf_type(enum rig_conf_e type);

__END_DECLS

#endif /* _SPRINTFLST_H */
