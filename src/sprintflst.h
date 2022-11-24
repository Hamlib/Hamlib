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

#define SPRINTF_MAX_SIZE 1024

__BEGIN_DECLS

extern HAMLIB_EXPORT( int ) rig_sprintf_mode(char *str, int len, rmode_t mode);
extern HAMLIB_EXPORT( int ) rig_sprintf_vfo(char *str, int len, vfo_t vfo);
extern HAMLIB_EXPORT( int ) rig_sprintf_ant(char *str, int len, ant_t ant);
extern HAMLIB_EXPORT( int ) rig_sprintf_func(char *str, int len, setting_t func);
extern HAMLIB_EXPORT( int ) rig_sprintf_agc_levels(RIG *rig, char *str, int len);
extern HAMLIB_EXPORT( int ) rot_sprintf_func(char *str, int len, setting_t func);
extern HAMLIB_EXPORT( int ) rig_sprintf_level(char *str, int len, setting_t level);
extern HAMLIB_EXPORT( int ) rot_sprintf_level(char *str, int len, setting_t level);
extern HAMLIB_EXPORT( int ) amp_sprintf_level(char *str, int len, setting_t level);
extern HAMLIB_EXPORT( int ) sprintf_level_ext(char *str, int len, const struct confparams *extlevels);
extern HAMLIB_EXPORT( int ) rig_sprintf_level_gran(char *str, int len, setting_t level, const gran_t *gran);
extern HAMLIB_EXPORT( int ) rot_sprintf_level_gran(char *str, int len, setting_t level, const gran_t *gran);
extern HAMLIB_EXPORT( int ) rig_sprintf_parm(char *str, int len, setting_t parm);
extern HAMLIB_EXPORT( int ) rot_sprintf_parm(char *str, int len, setting_t parm);
extern HAMLIB_EXPORT( int ) rig_sprintf_parm_gran(char *str, int len, setting_t parm, const gran_t *gran);
extern HAMLIB_EXPORT( int ) rot_sprintf_parm_gran(char *str, int len, setting_t parm, const gran_t *gran);
extern HAMLIB_EXPORT( int ) rig_sprintf_vfop(char *str, int len, vfo_op_t op);
extern HAMLIB_EXPORT( int ) rig_sprintf_scan(char *str, int len, scan_t rscan);
extern HAMLIB_EXPORT( int ) rot_sprintf_status(char *str, int len, rot_status_t status);
extern HAMLIB_EXPORT( int ) rig_sprintf_spectrum_modes(char *str, int nlen, const enum rig_spectrum_mode_e *modes);
extern HAMLIB_EXPORT( int ) rig_sprintf_spectrum_spans(char *str, int nlen, const freq_t *spans);
extern HAMLIB_EXPORT( int )  rig_sprintf_spectrum_avg_modes(char *str, int nlen, const struct rig_spectrum_avg_mode *avg_modes);
extern HAMLIB_EXPORT( char *) get_rig_conf_type(enum rig_conf_e type);
extern HAMLIB_EXPORT( int ) print_ext_param(const struct confparams *cfp, rig_ptr_t ptr);

__END_DECLS

#endif /* _SPRINTFLST_H */
