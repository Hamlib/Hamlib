/*
 *  Hamlib Interface - plugin registration
 *  Copyright (c) 2003-2005 by Stephane Fillod
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
#ifndef _REGISTER_H
#define _REGISTER_H 1


#include <hamlib/rig.h>
#include <hamlib/rotator.h>

#ifdef __cplusplus
#define EXTERN_C extern "C"
#else
#define EXTERN_C extern
#endif

#define CONCAT4(w__, x__, y__, z__) w__ ## x__ ## y__ ## z__
#define MAKE_VERSIONED_FN(prefix__, version__, name_args__) CONCAT4(prefix__, version__, _, name_args__)
/* void MAKE_VERSIONED_FN(foo, 42, bar(int i))  ->  void foo42_bar(int i) */

#ifndef ABI_VERSION
#error ABI_VERSION undefined! Did you include config.h?
#endif

#ifdef DECLARE_INITRIG_BACKEND
#undef DECLARE_INITRIG_BACKEND
#endif
#define DECLARE_INITRIG_BACKEND(backend) EXTERN_C BACKEND_EXPORT(int) MAKE_VERSIONED_FN(initrigs, ABI_VERSION, backend(void *be_handle))

#ifdef DECLARE_PROBERIG_BACKEND
#undef DECLARE_PROBERIG_BACKEND
#endif
#define DECLARE_PROBERIG_BACKEND(backend) EXTERN_C BACKEND_EXPORT(rig_model_t) MAKE_VERSIONED_FN(probeallrigs, ABI_VERSION, backend(hamlib_port_t *port, rig_probe_func_t cfunc, rig_ptr_t data))

#ifdef DECLARE_INITROT_BACKEND
#undef DECLARE_INITROT_BACKEND
#endif
#define DECLARE_INITROT_BACKEND(backend) EXTERN_C BACKEND_EXPORT(int) MAKE_VERSIONED_FN(initrots, ABI_VERSION, backend(void *be_handle))

#ifdef DECLARE_PROBEROT_BACKEND
#undef DECLARE_PROBEROT_BACKEND
#endif
#define DECLARE_PROBEROT_BACKEND(backend) EXTERN_C BACKEND_EXPORT(rot_model_t) MAKE_VERSIONED_FN(probeallrots, ABI_VERSION, backend(hamlib_port_t *port, rig_probe_func_t cfunc, rig_ptr_t data))

#endif	/* _REGISTER_H */
