/*
 *  Hamlib Interface - list of known amplifiers
 *  Copyright (c) 2000-2011 by Stephane Fillod
 *  Copyright (c) 2000-2002 by Frank Singleton
 *  Copyright (C) 2019 by Michael Black W9MDB. Derived from rotlist.h
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

#ifndef _AMPLIST_H
#define _AMPLIST_H 1

//! @cond Doxygen_Suppress
#define AMP_MAKE_MODEL(a,b) ((a)*100+(b))
#define AMP_BACKEND_NUM(a) ((a)/100)
//! @endcond


/**
 * \addtogroup amplifier
 * @{
 */

/**
 *  \file amplist.h
 *  \brief Hamlib amplifier model definitions.
 *
 *  This file contains amplifier model definitions for the Hamlib amplifier
 *  Application Programming Interface (API).  Each distinct amplifier type has
 *  a unique model number (ID) and is used by Hamlib to identify and
 *  distinguish between the different hardware drivers.  The exact model
 *  numbers can be acquired using the macros in this file. To obtain a list of
 *  supported amplifier branches, one can use the statically defined
 *  AMP_BACKEND_LIST macro. To obtain a full list of supported amplifiers
 *  (including each model in every branch), the foreach_opened_amp() API
 *  function can be used.
 *
 *  The model number, or ID, is used to tell Hamlib which amplifier the client
 *  wishes to use. It is done with the amp_init() API call.
 */


//! @cond Doxygen_Suppress
#define AMP_MODEL_NONE 0
//! @endcond

/*
 * Hamlib
 */

/**
 *  \def AMP_MODEL_DUMMY
 *  \brief A macro that returns the model number for the dummy backend.
 *
 *  The dummy backend, as the name suggests, is a backend which performs no
 *  hardware operations and always behaves as one would expect. It can be
 *  thought of as a hardware simulator and is very useful for testing client
 *  applications.
 *
 *  \def AMP_MODEL_NETAMPCTL
 *  \brief A macro that returns the model number for the netampctl backend.
 *
 *  This backend allows use of the ampctld daemon through the normal
 *  Hamlib API.
 */
//! @cond Doxygen_Suppress
#define AMP_DUMMY 0
#define AMP_BACKEND_DUMMY "dummy"
//! @endcond
#define AMP_MODEL_DUMMY AMP_MAKE_MODEL(AMP_DUMMY, 1)
#define AMP_MODEL_NETAMPCTL AMP_MAKE_MODEL(AMP_DUMMY, 2)


/*
 * Elecraft
 */

/**
 *  \def AMP_MODEL_ELECRAFT_KPA1500
 *  \brief A macro that returns the model number of the kpa1500 backend.
 *
 *  The kpa1500 backend can be used with amplifiers that support the Elecraft
 *  KPA-1500 protocol.
 */
//! @cond Doxygen_Suppress
#define AMP_ELECRAFT 2
#define AMP_BACKEND_ELECRAFT "elecraft"
//! @endcond
#define AMP_MODEL_ELECRAFT_KPA1500 AMP_MAKE_MODEL(AMP_ELECRAFT, 1)
//#define AMP_MODEL_ELECRAFT_KPA500 AMP_MAKE_MODEL(AMP_ELECRAFT, 2)

/**
 *  \typedef typedef int amp_model_t
 *  \brief Convenience type definition for amplifier model.
 */
typedef int amp_model_t;


#endif /* _AMPLIST_H */

/** @} */
