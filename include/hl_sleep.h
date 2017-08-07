/*
 *  Hamlib Interface - Replacement sleep() declaration for Windows
 *  Copyright (C) 2013 The Hamlib Group
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

/* Define missing sleep() prototype */
#ifdef  __cplusplus
extern "C" {
#endif

#ifdef HAVE_WINBASE_H
#include <windows.h>
#include <winbase.h>
#endif

/* TODO: what about SleepEx? */
static inline unsigned int sleep(unsigned int nb_sec)
{
    Sleep(nb_sec * 1000);
    return 0;
}

#ifdef  __cplusplus
}
#endif
