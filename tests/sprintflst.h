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
#include <hamlib/rotator.h>

#define SPRINTF_MAX_SIZE 512

__BEGIN_DECLS

extern int rig_sprintf_mode(char *str, rmode_t mode);
extern int rig_sprintf_vfo(char *str, vfo_t vfo);
extern int rig_sprintf_ant(char *str, ant_t ant);
extern int rig_sprintf_func(char *str, setting_t func);
extern int rot_sprintf_func(char *str, setting_t func);
extern int rig_sprintf_level(char *str, setting_t level);
extern int rot_sprintf_level(char *str, setting_t level);
extern int amp_sprintf_level(char *str, setting_t level);
extern int sprintf_level_ext(char *str, const struct confparams *extlevels);
extern int rig_sprintf_level_gran(char *str, setting_t level, const gran_t *gran);
extern int rot_sprintf_level_gran(char *str, setting_t level, const gran_t *gran);
extern int rig_sprintf_parm(char *str, setting_t parm);
extern int rot_sprintf_parm(char *str, setting_t parm);
extern int rig_sprintf_parm_gran(char *str, setting_t parm, const gran_t *gran);
extern int rot_sprintf_parm_gran(char *str, setting_t parm, const gran_t *gran);
extern int rig_sprintf_vfop(char *str, vfo_op_t op);
extern int rig_sprintf_scan(char *str, scan_t rscan);
extern int rot_sprintf_status(char *str, rot_status_t status);
extern char *get_rig_conf_type(enum rig_conf_e type);
int print_ext_param(const struct confparams *cfp, rig_ptr_t ptr);

__END_DECLS

#endif /* _SPRINTFLST_H */
