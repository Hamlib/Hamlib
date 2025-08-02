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
/* SPDX-License-Identifier: LGPL-2.1-or-later */

#ifndef _AMPLIST_H
#define _AMPLIST_H 1

/**
 * \addtogroup amplist
 * @{
 */


/**
 * \brief Hamlib amplifier model definitions.
 *
 * \file amplist.h
 *
 * This file contains amplifier model definitions for the Hamlib amplifier
 * Application Programming Interface (API).  Each distinct amplifier type has
 * a unique model number (ID) and is used by Hamlib to identify and
 * distinguish between the different hardware drivers.  The exact model
 * numbers can be acquired using the macros in this file. To obtain a list of
 * supported amplifier branches, one can use the statically defined
 * AMP_BACKEND_LIST macro (defined in configure.ac). To obtain a full list of
 * supported amplifiers (including each model in every branch), the
 * foreach_opened_amp() API function can be used.
 *
 * The model number, or ID, is used to tell Hamlib which amplifier the client
 * wishes to use which is passed to the amp_init() API call.
 */

/**
 * \brief The amp model number is held in a signed integer.
 *
 * Model numbers are a simple decimal value that increments by a value of
 * 100 for each backend, e.g. the `DUMMY` backend has model numbers 1
 * to 100, the `ELECRAFT` backend has model numbers 201 to 300 and so on
 * (101 to 200 is currently unassigned).
 *
 * \note A limitation is that with ::amp_model_t being a signed integer that on
 * some systems such a value may be 16 bits.  This limits the number of backends
 * to 326 of 100 models each (32768 / 100 thus leaving only 68 models for
 * backend number 327 so round down to 326).  So far this doesn't seem like an
 * extreme limitation.
 *
 * \sa amp_model_t
 */
#define AMP_MAKE_MODEL(a,b) ((a)*100+(b))

/** Convenience macro to derive the backend family number from the model number. */
#define AMP_BACKEND_NUM(a) ((a)/100)


/**
 * \brief A macro that returns the model number for an unknown model.
 *
 * \def AMP_MODEL_NONE
 *
 * The none backend, as the name suggests, does nothing.  It is mainly for
 * internal use.
 */
#define AMP_MODEL_NONE 0


/** The `DUMMY` family.  Also contains network models. */
#define AMP_DUMMY 0
/** Used in amp_reg.c for the `be_name`. */
#define AMP_BACKEND_DUMMY "dummy"
/**
 * \brief A macro that returns the model number for `DUMMY`.
 *
 * The `DUMMY` model, as the name suggests, is a model which performs no
 * hardware operations and always behaves as one would expect.  It can be
 * thought of as a hardware simulator and is very useful for testing client
 * applications.
 */
#define AMP_MODEL_DUMMY AMP_MAKE_MODEL(AMP_DUMMY, 1)
/**
 * \brief A macro that returns the model number for `NETAMPCTL`.
 *
 * The `NETAMPCTL` model allows use of the `ampctld` daemon through the normal
 * Hamlib C API.
 */
#define AMP_MODEL_NETAMPCTL AMP_MAKE_MODEL(AMP_DUMMY, 2)


/** The `ELECRAFT` family. */
#define AMP_ELECRAFT 2
/** Used in amp_reg.c for the `be_name`. */
#define AMP_BACKEND_ELECRAFT "elecraft"
/**
 * \brief A macro that returns the model number of `KPA1500`.
 *
 * The `KPA1500` model can be used with amplifiers that support the Elecraft
 * KPA-1500 protocol.
 */
#define AMP_MODEL_ELECRAFT_KPA1500 AMP_MAKE_MODEL(AMP_ELECRAFT, 1)
//#define AMP_MODEL_ELECRAFT_KPA500 AMP_MAKE_MODEL(AMP_ELECRAFT, 2)


/** The `GEMINI` family. */
#define AMP_GEMINI 3
/** Used in amp_reg.c for the `be_name`. */
#define AMP_BACKEND_GEMINI "gemini"
/**
 * \brief A macro that returns the model number of `DX1200`.
 *
 * The Gemini DX1200 covers 160 through 4 meters.
 */
#define AMP_MODEL_GEMINI_DX1200 AMP_MAKE_MODEL(AMP_GEMINI, 1)


/** The `EXPERT` family. */
#define AMP_EXPERT 4
/** Used in amp_reg.c for the `be_name`. */
#define AMP_BACKEND_EXPERT "expert"
/**
 * \brief A macro that returns the model number of `FA`.
 *
 * The Expert FA series of amplifiers is supported by this backend.
 */
#define AMP_MODEL_EXPERT_FA AMP_MAKE_MODEL(AMP_EXPERT, 1)


/** Convenience type definition for an amplifier model. */
typedef int amp_model_t;


#endif /* _AMPLIST_H */

/** @} */
