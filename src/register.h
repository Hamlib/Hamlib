/*
 *  Hamlib Interface - plugin registration
 *  Copyright (c) 2003 by Stephane Fillod
 *
 *	$Id: register.h,v 1.3 2003-11-16 22:15:37 fillods Exp $
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
#ifndef _REGISTER_H
#define _REGISTER_H 1


#include <hamlib/rig.h>
#include <hamlib/rotator.h>

#ifdef __cplusplus
#define EXTERN_C extern "C"
#else
#define EXTERN_C extern
#endif

#ifdef DECLARE_INITRIG_BACKEND
#undef DECLARE_INITRIG_BACKEND
#endif
#define DECLARE_INITRIG_BACKEND(backend) EXTERN_C BACKEND_EXPORT(int) initrigs1_##backend(void *be_handle)

#ifdef DECLARE_PROBERIG_BACKEND
#undef DECLARE_PROBERIG_BACKEND
#endif
#define DECLARE_PROBERIG_BACKEND(backend) EXTERN_C BACKEND_EXPORT(rig_model_t) probeallrigs1_##backend(port_t *port, rig_probe_func_t cfunc, rig_ptr_t data)

#ifdef DECLARE_INITROT_BACKEND
#undef DECLARE_INITROT_BACKEND
#endif
#define DECLARE_INITROT_BACKEND(backend) EXTERN_C BACKEND_EXPORT(int) initrots1_##backend(void *be_handle)

#ifdef DECLARE_PROBEROT_BACKEND
#undef DECLARE_PROBEROT_BACKEND
#endif
#define DECLARE_PROBEROT_BACKEND(backend) EXTERN_C BACKEND_EXPORT(rot_model_t) probeallrots1_##backend(port_t *port, rig_probe_func_t cfunc, rig_ptr_t data)

#endif	/* _REGISTER_H */
