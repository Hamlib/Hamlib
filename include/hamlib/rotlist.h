/*
 *  Hamlib Interface - list of known rotators
 *  Copyright (c) 2000,2001 by Stephane Fillod and Frank Singleton
 *
 *		$Id: rotlist.h,v 1.3 2002-01-16 17:03:57 fgretief Exp $
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

#ifndef _ROTLIST_H
#define _ROTLIST_H 1

#define ROT_MAKE_MODEL(a,b) ((a)*100+(b))
#define ROT_BACKEND_NUM(a) ((a)/100)

#define ROT_MODEL_NONE 0

#define ROT_DUMMY 0
#define ROT_BACKEND_DUMMY "dummy"
#define ROT_MODEL_DUMMY ROT_MAKE_MODEL(ROT_DUMMY, 1)

	/*
	 * RPC Network pseudo-backend
	 */
#define ROT_RPC 1
#define ROT_BACKEND_RPC "rpcrot"
#define ROT_MODEL_RPC ROT_MAKE_MODEL(ROT_RPC, 1)


	/*
	 * Easycomm
	 */
#define ROT_EASYCOMM 2
#define ROT_BACKEND_EASYCOMM "easycomm"
#define ROT_MODEL_EASYCOMM1 ROT_MAKE_MODEL(ROT_EASYCOMM, 1)
#define ROT_MODEL_EASYCOMM2 ROT_MAKE_MODEL(ROT_EASYCOMM, 2)

typedef int rot_model_t;

#define ROT_BACKEND_LIST {		\
		{ ROT_DUMMY, ROT_BACKEND_DUMMY }, \
		{ ROT_RPC, ROT_BACKEND_RPC }, \
		{ ROT_EASYCOMM, ROT_BACKEND_EASYCOMM }, \
		{ 0, NULL }, /* end */  \
}

/*
 * struct rot_backend_list {
 *		rot_model_t model;
 *		const char *backend;
 * } rot_backend_list[] = ROT_LIST;
 *
 */

#endif /* _ROTLIST_H */
