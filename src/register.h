/*
 *  Hamlib Interface - plugin registration
 *  Copyright (c) 2003 by Stephane Fillod
 *
 *	$Id: register.h,v 1.1 2003-04-16 22:30:38 fillods Exp $
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

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <hamlib/rig.h>
#include <hamlib/rotator.h>

#ifdef DECLARE_INITRIG_BACKEND
#undef DECLARE_INITRIG_BACKEND
#endif
#define DECLARE_INITRIG_BACKEND(backend) extern BACKEND_EXPORT(int) initrigs1_##backend(void *be_handle)

#ifdef DECLARE_PROBERIG_BACKEND
#undef DECLARE_PROBERIG_BACKEND
#endif
#define DECLARE_PROBERIG_BACKEND(backend) extern BACKEND_EXPORT(rig_model_t) probeallrigs1_##backend(port_t *port, rig_probe_func_t cfunc, rig_ptr_t data)

#ifdef DECLARE_INITROT_BACKEND
#undef DECLARE_INITROT_BACKEND
#endif
#define DECLARE_INITROT_BACKEND(backend) extern BACKEND_EXPORT(int) initrots1_##backend(void *be_handle)

#ifdef DECLARE_PROBEROT_BACKEND
#undef DECLARE_PROBEROT_BACKEND
#endif
#define DECLARE_PROBEROT_BACKEND(backend) extern BACKEND_EXPORT(rot_model_t) probeallrots1_##backend(port_t *port, rig_probe_func_t cfunc, rig_ptr_t data)

