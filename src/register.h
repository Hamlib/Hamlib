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
#include <hamlib/amplifier.h>
#include <hamlib/config.h>

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

#define PREFIX_INITRIG initrigs
#define PREFIX_PROBERIG probeallrigs

#define DECLARE_INITRIG_BACKEND(backend)                                \
    EXTERN_C BACKEND_EXPORT(int)                                        \
    MAKE_VERSIONED_FN(PREFIX_INITRIG, ABI_VERSION, backend(void *be_handle))

#define DECLARE_PROBERIG_BACKEND(backend)                               \
    EXTERN_C BACKEND_EXPORT(rig_model_t)                                \
    MAKE_VERSIONED_FN(PREFIX_PROBERIG,                                  \
                      ABI_VERSION,                                      \
                      backend(hamlib_port_t *port,                      \
                              rig_probe_func_t cfunc,                   \
                              rig_ptr_t data))

#define PREFIX_INITROTS initrots
#define PREFIX_PROBEROTS probeallrots

#define DECLARE_INITROT_BACKEND(backend)                                \
    EXTERN_C BACKEND_EXPORT(int)                                        \
    MAKE_VERSIONED_FN(PREFIX_INITROTS, ABI_VERSION, backend(void *be_handle))

#define DECLARE_PROBEROT_BACKEND(backend)               \
    EXTERN_C BACKEND_EXPORT(rot_model_t)                \
    MAKE_VERSIONED_FN(PREFIX_PROBEROTS,                 \
                      ABI_VERSION,                      \
                      backend(hamlib_port_t *port,      \
                              rig_probe_func_t cfunc,   \
                              rig_ptr_t data))

#define PREFIX_INITAMPS initamps
#define PREFIX_PROBEAMPS probeallamps

#define DECLARE_INITAMP_BACKEND(backend)                                \
    EXTERN_C BACKEND_EXPORT(int)                                        \
    MAKE_VERSIONED_FN(PREFIX_INITAMPS, ABI_VERSION, backend(void *be_handle))

#define DECLARE_PROBEAMP_BACKEND(backend)               \
    EXTERN_C BACKEND_EXPORT(amp_model_t)                \
    MAKE_VERSIONED_FN(PREFIX_PROBEAMPS,                 \
                      ABI_VERSION,                      \
                      backend(hamlib_port_t *port,      \
                              rig_probe_func_t cfunc,   \
                              rig_ptr_t data))

#endif  /* _REGISTER_H */
