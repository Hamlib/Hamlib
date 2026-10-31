/*
 * Internal rig frontend interfaces.
 *
 * Never installed and not public API.  As in the other internal src/
 * headers (iofunc.h, misc.h), HAMLIB_EXPORT makes the symbols
 * reachable from out-of-library executables such as the test suite;
 * HL_PRIVATE arms a deprecation warning for any other consumer.
 *
 * SPDX-License-Identifier: LGPL-2.1-or-later
 */

#ifndef HAMLIB_RIG_INTERNAL_H
#define HAMLIB_RIG_INTERNAL_H

#include "hamlib/rig.h"
#include "hamlib/rig_state.h"

typedef int (*hamlib_thread_start_t)(pthread_t *thread,
                                     void *(*routine)(void *), void *arg);

HL_PRIVATE extern HAMLIB_EXPORT(int)
rig_async_data_handler_start(RIG *rig, hamlib_thread_start_t thread_start);
HL_PRIVATE extern HAMLIB_EXPORT(int)
rig_morse_data_handler_start(RIG *rig, hamlib_thread_start_t thread_start);

#endif /* HAMLIB_RIG_INTERNAL_H */
