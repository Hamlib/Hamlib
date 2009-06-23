/*
 *  Hamlib Interface - numeric locale wrapping helpers
 *  Copyright (c) 2009 by Stephane Fillod
 *
 *	$Id: bandplan.h,v 1.1 2002-11-16 14:05:16 fillods Exp $
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

#define num_sprintf(a...) \
	({ int __ret; char *__savedlocale; \
	   __savedlocale = setlocale(LC_NUMERIC, NULL); \
	   setlocale(LC_NUMERIC, "C"); \
	   __ret = sprintf(a); \
	   setlocale(LC_NUMERIC, __savedlocale); \
	   __ret; \
	 })

#endif	/* _NUM_STDIO_H */
