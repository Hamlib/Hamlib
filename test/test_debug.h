/*
 *  Hamlib streaming subsystem test helpers
 *  Copyright (c) 2026 by Mikael Nousiainen OH3BHX
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
/* SPDX-License-Identifier: LGPL-2.1-or-later */

/* Default debug level for the unit-test suites.
 *
 * The library's built-in default is RIG_DEBUG_TRACE, which floods the
 * test logs with every function entry. The suites run at RIG_DEBUG_WARN
 * instead, so a captured test log holds the acutest per-case results,
 * warnings/errors, and the suites' own deliberate diagnostics.
 *
 * To debug a failure at full detail, set RIG_TEST_DEBUG in the
 * environment (any value): the suite then runs at RIG_DEBUG_TRACE, e.g.
 *
 *     RIG_TEST_DEBUG=1 make check
 *     RIG_TEST_DEBUG=1 ./test/test_netrigctl_stream
 *
 * Suites that spawn a rigctld child also raise the child's verbosity to
 * TRACE when the variable is set (see test_netrigctl_stream.c).
 *
 * Include this header once per test program; the constructor runs before
 * acutest's main(). Both compilers used for the tests (gcc, clang,
 * mingw-w64 gcc) support the constructor attribute.
 */

#ifndef HAMLIB_TEST_DEBUG_H
#define HAMLIB_TEST_DEBUG_H

#include <hamlib/rig.h>
#include <stdlib.h>

static __attribute__((constructor)) void test_debug_init(void)
{
    rig_set_debug(getenv("RIG_TEST_DEBUG") != NULL
                  ? RIG_DEBUG_TRACE : RIG_DEBUG_WARN);
}

#endif /* HAMLIB_TEST_DEBUG_H */
