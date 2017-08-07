/*
 *  Hamlib Interface - numeric locale wrapping helpers
 *  Copyright (c) 2009 by Stephane Fillod
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

#ifndef _NUM_STDIO_H
#define _NUM_STDIO_H 1

#include <locale.h>

/*
 * This header file is internal to Hamlib and its backends,
 * thus not part of the API.
 */

/*
 * Wrapper for sscanf to workaround some locales where the decimal
 * separator (float, ...) is not the dot.
 */
#define num_sscanf(a...) \
    ({ int __ret; char *__savedlocale; \
       __savedlocale = setlocale(LC_NUMERIC, NULL); \
       setlocale(LC_NUMERIC, "C"); \
       __ret = sscanf(a); \
       setlocale(LC_NUMERIC, __savedlocale); \
       __ret; \
     })

#define num_sprintf(s, a...) \
    ({ int __ret; char *__savedlocale; \
       __savedlocale = setlocale(LC_NUMERIC, NULL); \
       setlocale(LC_NUMERIC, "C"); \
       __ret = sprintf(s, a); \
       setlocale(LC_NUMERIC, __savedlocale); \
       __ret; \
     })

#define num_snprintf(s, n, a...) \
    ({ int __ret; char *__savedlocale; \
       __savedlocale = setlocale(LC_NUMERIC, NULL); \
       setlocale(LC_NUMERIC, "C"); \
       __ret = snprintf(s, n, a); \
       setlocale(LC_NUMERIC, __savedlocale); \
       __ret; \
     })

#endif  /* _NUM_STDIO_H */
