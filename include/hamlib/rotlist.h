/*
 *  Hamlib Interface - list of known rotators
 *  Copyright (c) 2000-2002 by Stephane Fillod and Frank Singleton
 *
 *	$Id: rotlist.h,v 1.6 2002-11-28 22:24:10 fillods Exp $
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

/*! \file rotlist.h
 *  \ingroup rot
 *  \brief Hamlib rotator model definitions.
 *
 *  This file contains rotator model definitions for the Hamlib rotator API.
 *  Each distinct rotator type has a unique model number (ID) and is used
 *  by hamlib to identify and distinguish between the different hardware drivers.
 *  The exact model numbers can be acquired using the macros in this
 *  file. To obtain a list of supported rotator branches, one can use the statically
 *  defined ROT_BACKEND_LIST macro. To obtain a full list of supported rotators (including
 *  each model in every branch), the foreach_opened_rot() API function can be used.
 *
 *  The model number, or ID, is used to tell hamlib, which rotator the client whishes to
 *  use. It is done with the rot_init() API call.
 */


#define ROT_MODEL_NONE 0

/*! \def ROT_MODEL_DUMMY
 *  \brief A macro that returns the model number for the dummy backend.
 *
 *  The dummy backend, as the name suggests, is a backend which performs
 *  no hardware operations and always behaves as one would expect. It can
 *  be thought of as a hardware simulator and is very usefull for testing
 *  client applications.
 */
#define ROT_DUMMY 0
#define ROT_BACKEND_DUMMY "dummy"
#define ROT_MODEL_DUMMY ROT_MAKE_MODEL(ROT_DUMMY, 1)

	/*
	 * RPC Network pseudo-backend
	 */
/*! \def ROT_MODEL_RPC
 *  \brief A macro that returns the model number of the RPC Network pseudo-backend.
 *
 *  The RPC backend can be used to connect and send commands to a rotator server,
 *  \c rpc.rotd, running on a remote machine. Using this client/server scheme,
 *  several clients can control and monitor the same rotator hardware.
 */
#define ROT_RPC 1
#define ROT_BACKEND_RPC "rpcrot"
#define ROT_MODEL_RPC ROT_MAKE_MODEL(ROT_RPC, 1)


	/*
	 * Easycomm
	 */
/*! \def ROT_MODEL_EASYCOMM1
 *  \brief A macro that returns the model number of the EasyComm 1 backend.
 *
 *  The EasyComm 1 backend can be used with rotators that support the
 *  EASYCOMM I Standard.
 */
/*! \def ROT_MODEL_EASYCOMM2
 *  \brief A macro that returns the model number of the EasyComm 2 backend.
 *
 *  The EasyComm 2 backend can be used with rotators that support the
 *  EASYCOMM II Standard.
 */
#define ROT_EASYCOMM 2
#define ROT_BACKEND_EASYCOMM "easycomm"
#define ROT_MODEL_EASYCOMM1 ROT_MAKE_MODEL(ROT_EASYCOMM, 1)
#define ROT_MODEL_EASYCOMM2 ROT_MAKE_MODEL(ROT_EASYCOMM, 2)

/*! \def ROT_MODEL_FODTRACK
 *  \brief A macro that returns the model number of the Fodtrack backend.
 *
 *  The Fodtrack backend can be used with rotators that support the
 *  FODTRACK Standard.
 */
#define ROT_FODTRACK 3
#define ROT_BACKEND_FODTRACK "fodtrack"
#define ROT_MODEL_FODTRACK ROT_MAKE_MODEL(ROT_FODTRACK, 1)


/*! \typedef typedef int rot_model_t
    \brief Convenience type definition for rotator model.
*/
typedef int rot_model_t;


/*! \def ROT_BACKEND_LIST
 *  \brief Static list of rotator models.
 *
 *  This is a NULL terminated list of available rotator backends. Each entry
 *  in the list consists of two fields: The branch number, which is an integer,
 *  and the branch name, which is a character string.
 */
#define ROT_BACKEND_LIST {		\
		{ ROT_DUMMY, ROT_BACKEND_DUMMY }, \
		{ ROT_RPC, ROT_BACKEND_RPC }, \
		{ ROT_EASYCOMM, ROT_BACKEND_EASYCOMM }, \
		{ ROT_FODTRACK, ROT_BACKEND_FODTRACK }, \
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
