/*
 *  Hamlib Interface - list of known rotators
 *  Copyright (c) 2000-2011 by Stephane Fillod
 *  Copyright (c) 2000-2002 by Frank Singleton
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

#ifndef _ROTLIST_H
#define _ROTLIST_H 1

/**
 * \addtogroup rotlist
 * @{
 */


/**
 * \brief Hamlib rotator model definitions.
 *
 * \file rotlist.h
 *
 * This file contains rotator model definitions for the Hamlib rotator
 * Application Programming Interface (API).  Each distinct rotator type has a
 * unique model number (ID) and is used by Hamlib to identify and distinguish
 * between the different hardware drivers.  The exact model numbers can be
 * acquired using the macros in this file.  To obtain a list of supported
 * rotator branches, one can use the statically defined ROT_BACKEND_LIST macro
 * (defined in configure.ac).  To obtain a full list of supported rotators
 * (including each model in every branch), the foreach_opened_rot() API
 * function can be used.
 *
 * The model number, or ID, is used to tell Hamlib which rotator the client
 * wishes to use which is passed to the rot_init() API call.
 */

/**
 * \brief The rotator model number is held in a signed integer.
 *
 * Model numbers are a simple decimal value that increments by a value of
 * 100 for each backend, e.g. the `DUMMY` backend has model numbers 1
 * to 100, the `EASYCOMM` backend has model numbers 201 to 300 and so on
 * (101 to 200 is currently unassigned).
 *
 * \note A limitation is that with ::rot_model_t being a signed integer that on
 * some systems such a value may be 16 bits.  This limits the number of backends
 * to 326 of 100 models each (32768 / 100 thus leaving only 68 models for
 * backend number 327 so round down to 326).  So far this doesn't seem like an
 * extreme limitation.
 *
 * \sa rot_model_t
 */
#define ROT_MAKE_MODEL(a,b) (100*(a)+(b))

/** Convenience macro to derive the backend family number from the model number. */
#define ROT_BACKEND_NUM(a) ((a)/100)

/**
 * \brief A macro that returns the model number for an unknown model.
 *
 * \def ROT_MODEL_NONE
 *
 * The none backend, as the name suggests, does nothing.  It is mainly for
 * internal use.
 */
#define ROT_MODEL_NONE 0


/** The `DUMMY` family.  Also contains network models. */
#define ROT_DUMMY 0
/** Used in register.c for the `be_name`. */
#define ROT_BACKEND_DUMMY "dummy"

/**
 * \brief A macro that returns the model number for `DUMMY`.
 *
 * \def ROT_MODEL_DUMMY
 *
 * The `DUMMY` model, as the name suggests, is a backend which performs
 * no hardware operations and always behaves as one would expect.  It can
 * be thought of as a hardware simulator and is very useful for testing
 * client applications.
 */
/**
 * \brief A macro that returns the model number for `NETROTCTL`.
 *
 * \def ROT_MODEL_NETROTCTL
 *
 * The `NETROTCTL` model allows use of the `rotctld` daemon through the normal
 * Hamlib API.
 */
/**
 * \brief A macro that returns the model number for `PSTROTATOR`.
 *
 * \def ROT_MODEL_PSTROTATOR
 *
 * The `PSTROTATOR` model allows Hamlib clients to access the rotators controlled
 * by the PstRotator software by YO3DMU: https://www.qsl.net/yo3dmu/index_Page346.htm
 */
/**
 * \brief A macro that returns the model number for `SATROTCTL`.
 *
 * \def ROT_MODEL_SATROTCTL
 *
 * The `SATROTCTL` model allows Hamlib clients to access the rotators controlled by
 * the S.A.T hardware by CSN Tecnologies: http://csntechnologies.net/
 */
#define ROT_MODEL_DUMMY ROT_MAKE_MODEL(ROT_DUMMY, 1)
#define ROT_MODEL_NETROTCTL ROT_MAKE_MODEL(ROT_DUMMY, 2)
#define ROT_MODEL_PSTROTATOR ROT_MAKE_MODEL(ROT_DUMMY, 3)
#define ROT_MODEL_SATROTCTL ROT_MAKE_MODEL(ROT_DUMMY, 4)


/** The `EASYCOMM` family. */
#define ROT_EASYCOMM 2
/** Used in register.c for the `be_name`. */
#define ROT_BACKEND_EASYCOMM "easycomm"

/**
 * \brief A macro that returns the model number of `EASYCOMM1`.
 *
 * \def ROT_MODEL_EASYCOMM1
 *
 * The `EASYCOMM1` model can be used with rotators that support the EASYCOMM
 * I Standard.
 */
/**
 * \brief A macro that returns the model number of the EASYCOMM 2 backend.
 *
 * \def ROT_MODEL_EASYCOMM2
 *
 * The EASYCOMM2 model can be used with rotators that support the EASYCOMM
 * II Standard.
 */
/**
 * \brief A macro that returns the model number of the EASYCOMM 3 backend.
 *
 * \def ROT_MODEL_EASYCOMM3
 *
 * The EASYCOMM3 model can be used with rotators that support the EASYCOMM
 * III Standard.
 */
#define ROT_MODEL_EASYCOMM1 ROT_MAKE_MODEL(ROT_EASYCOMM, 1)
#define ROT_MODEL_EASYCOMM2 ROT_MAKE_MODEL(ROT_EASYCOMM, 2)
#define ROT_MODEL_EASYCOMM3 ROT_MAKE_MODEL(ROT_EASYCOMM, 4)


/** The `FODTRACK` family. */
#define ROT_FODTRACK 3
/** Used in register.c for the `be_name`. */
#define ROT_BACKEND_FODTRACK "fodtrack"

/**
 * \brief A macro that returns the model number of `FODTRACK`.
 *
 * \def ROT_MODEL_FODTRACK
 *
 * The `FODTRACK model` can be used with rotators that support the FODTRACK
 * Standard.
 */
#define ROT_MODEL_FODTRACK ROT_MAKE_MODEL(ROT_FODTRACK, 1)


/** The `ROTOREZ` family. */
#define ROT_ROTOREZ 4
/** Used in register.c for the `be_name`. */
#define ROT_BACKEND_ROTOREZ "rotorez"

/**
 * \brief A macro that returns the model number of `ROTOREZ`.
 *
 * \def ROT_MODEL_ROTOREZ
 *
 * The `ROTOREZ` model can be used with Hy-Gain rotators that support the
 * extended DCU command set by the Idiom Press Rotor-EZ board.
 */
/**
 * \brief A macro that returns the model number of `ROTORCARD`.
 *
 * \def ROT_MODEL_ROTORCARD
 *
 * The `ROTORCARD` model can be used with Yaesu rotators that support the
 * extended DCU command set by the Idiom Press Rotor Card board.
 */
/**
 * \brief A macro that returns the model number of `DCU`.
 *
 * \def ROT_MODEL_DCU
 *
 * The `DCU` model can be used with rotators that support the DCU command set
 * by Hy-Gain (currently the DCU-1).
 */
/**
 * \brief A macro that returns the model number of `ERC`.
 *
 * \def ROT_MODEL_ERC
 *
 * The `ERC` model can be used with rotators that support the DCU command set
 * by DF9GR (currently the ERC).
 */
/**
 * \brief A macro that returns the model number of `RT21`.
 *
 * \def ROT_MODEL_RT21
 *
 * The `RT21` model can be used with rotators that support the DCU command set
 * by Green Heron (currently the RT-21).
 */
/**
 * \brief A macro that returns the model number of `YRC-1`.
 *
 * \def ROT_MODEL_YRC1
 *
 * The `YRC1` model can be used with rotators that support the DCU 2/3 command set
 */
#define ROT_MODEL_ROTOREZ ROT_MAKE_MODEL(ROT_ROTOREZ, 1)
#define ROT_MODEL_ROTORCARD ROT_MAKE_MODEL(ROT_ROTOREZ, 2)
#define ROT_MODEL_DCU ROT_MAKE_MODEL(ROT_ROTOREZ, 3)
#define ROT_MODEL_ERC ROT_MAKE_MODEL(ROT_ROTOREZ, 4)
#define ROT_MODEL_RT21 ROT_MAKE_MODEL(ROT_ROTOREZ, 5)
#define ROT_MODEL_YRC1 ROT_MAKE_MODEL(ROT_ROTOREZ, 6)
#define ROT_MODEL_RT21 ROT_MAKE_MODEL(ROT_ROTOREZ, 5)


/** The `SARTEK` family. */
#define ROT_SARTEK 5
/** Used in register.c for the `be_name`. */
#define ROT_BACKEND_SARTEK "sartek"

/**
 * \brief A macro that returns the model number of `SARTEK1`.
 *
 * \def ROT_MODEL_SARTEK1
 *
 * The `SARTEK1` model can be used with rotators that support the SARtek
 * protocol.
 */
#define ROT_MODEL_SARTEK1 ROT_MAKE_MODEL(ROT_SARTEK, 1)


/** The `GS232A` family. */
#define ROT_GS232A 6
/** Used in register.c for the `be_name`. */
#define ROT_BACKEND_GS232A "gs232a"

/**
 * \brief A macro that returns the model number of `GS232A`.
 *
 * \def ROT_MODEL_GS232A
 *
 * The `GS232A` model can be used with rotators that support the GS-232A
 * protocol.
 */
/**
 * \brief A macro that returns the model number of `GS232_GENERIC`.
 *
 * \def ROT_MODEL_GS232_GENERIC
 *
 * The` GS232_GENERIC` model can be used with rotators that support the
 * generic (even if not coded correctly) GS-232 protocol.
 */
/**
 * \brief A macro that returns the model number of `GS232B`.
 *
 * \def ROT_MODEL_GS232B
 *
 * The `GS232B` model can be used with rotators that support the GS232B
 * protocol.
 */
/**
 * \brief A macro that returns the model number of `F1TETRACKER`.
 *
 * \def ROT_MODEL_F1TETRACKER
 *
 * The `F1TETRACKER` model can be used with rotators that support the F1TE
 * Tracker protocol.
 */
/**
 * \brief A macro that returns the model number of `GS23`.
 *
 * \def ROT_MODEL_GS23
 *
 * The `GS23` model can be used with rotators that support the GS-23 protocol.
 */
/**
 * \brief A macro that returns the model number of `GS232`.
 *
 * \def ROT_MODEL_GS232
 *
 * The `GS232` model can be used with rotators that support the GS-232
 * protocol.
 */
/**
 * \brief A macro that returns the model number of `LVB`.
 *
 * \def ROT_MODEL_LVB
 *
 * The `LVB` model can be used with rotators that support the G6LVB AMSAT LVB
 * Tracker GS-232 based protocol.
 */
/**
 * \brief A macro that returns the model number of `ST2`.
 *
 * \def ROT_MODEL_ST2
 *
 * The `ST2` model can be used with rotators that support the Fox Delta ST2
 * GS-232 based protocol.
 */
/**
 * \brief A macro that returns the model number of `GS232A_AZ` Azimuth.
 *
 * \def ROT_MODEL_GS232A_AZ
 *
 * The `GS232A_AZ` model can be used with azimuth rotators that support the
 * GS-232A protocol.
 */
/**
 * \brief A macro that returns the model number of `GS232A_EL` Elevation.
 *
 * \def ROT_MODEL_GS232A_EL
 *
 * The `GS232A_EL` model can be used with elevation rotators that support the
 * GS-232A protocol.
 */
/**
 * \brief A macro that returns the model number of ` GS232B_AZ` Azimuth.
 *
 * \def ROT_MODEL_GS232B_AZ
 *
 * The `GS232B_AZ` model can be used with azimuth rotators that support the
 * GS-232B protocol.
 */
/**
 * \brief A macro that returns the model number of `GS232B_EL` Elevation.
 *
 * \def ROT_MODEL_GS232B_EL
 *
 * The `GS232B_EL` model can be used with elevation rotators that support the
 * GS-232B protocol.
 */
/**
 * \brief A macro that returns the model number of `GS23_AZ` azimuth.
 *
 * \def ROT_MODEL_GS23_AZ
 *
 * The `GS23_AZ` model can be used with azimuth rotators that support a
 * generic version of the GS-232A protocol.
 */
/**
 * \brief A macro that returns the model number of `AF6SA_WRC`.
 *
 * \def ROT_MODEL_AF6SA_WRC
 *
 * The `AF6SA_WRC` model can be used with the AF6SA controller.
 * http://af6sa.com/projects/wrc.html
 */
//! @cond Doxygen_Suppress
//! @endcond
#define ROT_MODEL_GS232A ROT_MAKE_MODEL(ROT_GS232A, 1)
#define ROT_MODEL_GS232_GENERIC ROT_MAKE_MODEL(ROT_GS232A, 2) /* GENERIC */
#define ROT_MODEL_GS232B ROT_MAKE_MODEL(ROT_GS232A, 3)
#define ROT_MODEL_F1TETRACKER ROT_MAKE_MODEL(ROT_GS232A, 4)
#define ROT_MODEL_GS23 ROT_MAKE_MODEL(ROT_GS232A, 5)
#define ROT_MODEL_GS232 ROT_MAKE_MODEL(ROT_GS232A, 6) /* Not A or B */
#define ROT_MODEL_LVB ROT_MAKE_MODEL(ROT_GS232A, 7)
#define ROT_MODEL_ST2 ROT_MAKE_MODEL(ROT_GS232A, 8)
#define ROT_MODEL_GS232A_AZ ROT_MAKE_MODEL(ROT_GS232A, 9)
#define ROT_MODEL_GS232A_EL ROT_MAKE_MODEL(ROT_GS232A, 10)
#define ROT_MODEL_GS232B_AZ ROT_MAKE_MODEL(ROT_GS232A, 11)
#define ROT_MODEL_GS232B_EL ROT_MAKE_MODEL(ROT_GS232A, 12)
#define ROT_MODEL_GS23_AZ ROT_MAKE_MODEL(ROT_GS232A, 13)
#define ROT_MODEL_AF6SA_WRC ROT_MAKE_MODEL(ROT_GS232A, 14)

//! @cond Doxygen_Suppress
/** The `ARRAYSOLUTIONS` family. */
#define ROT_ARRAYSOLUTIONS 327                  // Adjust value when implemented.
/** Used in register.c for the `be_name`. */
#define ROT_BACKEND ARRAYSOLUTIONS "arraysolutions"
// Add documentation when this model is implemented.
#define ROT_MODEL_ARRAYSOLUTIONS_SAL_12_20_30 ROT_MAKE_MODEL(ROT_ARRAYSOLUTIONS, 1)
//! @endcond

/** The `KIT` family. */
#define ROT_KIT 7
/** Used in register.c for the `be_name`. */
#define ROT_BACKEND_KIT "kit"

/**
 * \brief A macro that returns the model number of `PCROTOR`.
 *
 * \def ROT_MODEL_PCROTOR
 *
 * The `PCROTOR` model is a member of the kit backend group that can be used
 * with home brewed rotators.
 */
#define ROT_MODEL_PCROTOR ROT_MAKE_MODEL(ROT_KIT, 1)


/** The `HEATHKIT` family. */
#define ROT_HEATHKIT 8
/** Used in register.c for the `be_name`. */
#define ROT_BACKEND_HEATHKIT "heathkit"

/**
 * \brief A macro that returns the model number of `HD1780`.
 *
 * \def ROT_MODEL_HD1780
 *
 * The `HD1780` model can be used with rotators that support the Heathkit
 * HD-1780 protocol.
 */
#define ROT_MODEL_HD1780 ROT_MAKE_MODEL(ROT_HEATHKIT, 1)


/** The `SPID` family. */
#define ROT_SPID 9
/** Used in register.c for the `be_name`. */
#define ROT_BACKEND_SPID "spid"

/**
 * \brief A macro that returns the model number of `ROT2PROG`.
 *
 * \def ROT_MODEL_SPID_ROT2PROG
 *
 * The `SPID_ROT2PROG` model can be used with rotators that support the SPID
 * azimuth and elevation protocol.
 */
/**
 * \brief A macro that returns the model number of `ROT1PROG`.
 *
 * \def ROT_MODEL_SPID_ROT1PROG
 *
 * The `SPID_ROT1PROG` model can be used with rotators that support the SPID
 * azimuth protocol.
 */
/**
 * \brief A macro that returns the model number of `SPID_MD01_ROT2PROG`.
 *
 * \def ROT_MODEL_SPID_MD01_ROT2PROG
 *
 * The `SPID_MD01_ROT2PROG` model can be used with rotators that support the
 * extended SPID ROT2PROG azimuth and elevation protocol.
 */
#define ROT_MODEL_SPID_ROT2PROG ROT_MAKE_MODEL(ROT_SPID, 1)
#define ROT_MODEL_SPID_ROT1PROG ROT_MAKE_MODEL(ROT_SPID, 2)
#define ROT_MODEL_SPID_MD01_ROT2PROG ROT_MAKE_MODEL(ROT_SPID, 3)


/** The `M2` family. */
#define ROT_M2 10
/** Used in register.c for the `be_name`. */
#define ROT_BACKEND_M2 "m2"

/**
 * \brief A macro that returns the model number of `RC2800`.
 *
 * \def ROT_MODEL_RC2800
 *
 * The `RC2800` model can be used with rotators that support the M2 (M
 * Squared) RC2800 protocol.
 */
/**
 * \brief A macro that returns the model number of `RC2800_EARLY_AZ`.
 *
 * \def ROT_MODEL_RC2800_EARLY_AZ
 *
 * The `RC2800_EARLY_AZ` model can be used with rotators that support the M2
 * (M Squared) RC2800 early azimuth protocol.
 */
/**
 * \brief A macro that returns the model number of `RC2800_EARLY_AZEL`.
 *
 * \def ROT_MODEL_RC2800_EARLY_AZEL
 *
 * The `RC2800_EARLY_AZEL` model can be used with rotators that support the M2
 * (M Squared) RC2800 early azimuth and elevation protocol.
 */
#define ROT_MODEL_RC2800 ROT_MAKE_MODEL(ROT_M2, 1)
#define ROT_MODEL_RC2800_EARLY_AZ ROT_MAKE_MODEL(ROT_M2, 2)
#define ROT_MODEL_RC2800_EARLY_AZEL ROT_MAKE_MODEL(ROT_M2, 3)


/** The `ARS` family. */
#define ROT_ARS 11
/** Used in register.c for the `be_name`. */
#define ROT_BACKEND_ARS "ars"

/**
 * \brief A macro that returns the model number of `RCI_AZEL`.
 *
 * \def ROT_MODEL_RCI_AZEL
 *
 * The `RCI_AZEL` model can be used with rotators that support the ARS azimuth
 * and elevation protocol.
 */
/**
 * \brief A macro that returns the model number of `RCI_AZ`.
 *
 * \def ROT_MODEL_RCI_AZ
 *
 * The `RCI_AZ` model can be used with rotators that support the ARS azimuth
 * protocol.
 */
#define ROT_MODEL_RCI_AZEL ROT_MAKE_MODEL(ROT_ARS, 1)
#define ROT_MODEL_RCI_AZ ROT_MAKE_MODEL(ROT_ARS, 2)


/** The `AMSAT` family. */
#define ROT_AMSAT 12
/** Used in register.c for the `be_name`. */
#define ROT_BACKEND_AMSAT "amsat"

/**
 * \brief A macro that returns the model number of `IF100`.
 *
 * \def ROT_MODEL_IF100
 *
 * The `IF100` model can be used with rotators that support the AMSAT IF-100
 * interface.
 */
#define ROT_MODEL_IF100 ROT_MAKE_MODEL(ROT_AMSAT, 1)


/** The `TS7400` family. */
#define ROT_TS7400 13
/** Used in register.c for the `be_name`. */
#define ROT_BACKEND_TS7400 "ts7400"

/**
 * \brief A macro that returns the model number of `TS7400`.
 *
 * \def ROT_MODEL_TS7400
 *
 * The `TS7400` model supports an embedded ARM board using the TS-7400 Linux
 * board.  More information is at https://www.embeddedarm.com
 */
#define ROT_MODEL_TS7400 ROT_MAKE_MODEL(ROT_TS7400, 1)


/** The `CELESTRON` family. */
#define ROT_CELESTRON 14
/** Used in register.c for the `be_name`. */
#define ROT_BACKEND_CELESTRON "celestron"

/**
 * \brief A macro that returns the model number of `NEXSTAR`.
 *
 * \def ROT_MODEL_NEXSTAR
 *
 * The `NEXSTAR` model can be used with rotators that support the Celestron
 * NexStar protocol and alike.
 */
#define ROT_MODEL_NEXSTAR ROT_MAKE_MODEL(ROT_CELESTRON, 1)


/** The `ETHER6` family. */
#define ROT_ETHER6 15
/** Used in register.c for the `be_name`. */
#define ROT_BACKEND_ETHER6 "ether6"

/**
 * \brief A macro that returns the model number of `ETHER6`.
 *
 * \def ROT_MODEL_ETHER6
 *
 * The `ETHER6` model can be used with rotators that support the Ether6
 * protocol.
 */
#define ROT_MODEL_ETHER6 ROT_MAKE_MODEL(ROT_ETHER6, 1)


/** The `CNCTRK` family. */
#define ROT_CNCTRK 16
/** Used in register.c for the `be_name`. */
#define ROT_BACKEND_CNCTRK "cnctrk"

/**
 * \brief A macro that returns the model number of `CNCTRK`.
 *
 * \def ROT_MODEL_CNCTRK
 *
 * The `CNCTRK` model can be used with rotators that support the LinuxCNC
 * running Axis GUI interface.
 */
#define ROT_MODEL_CNCTRK ROT_MAKE_MODEL(ROT_CNCTRK, 1)


/** The `PROSISTEL` family. */
#define ROT_PROSISTEL 17
/** Used in register.c for the `be_name`. */
#define ROT_BACKEND_PROSISTEL "prosistel"

/**
 * \brief A macro that returns the model number of `PROSISTEL_D_AZ`.
 *
 * \def ROT_MODEL_PROSISTEL_D_AZ
 *
 * The `PROSISTEL_D_AZ` model can be used with rotators that support the Prosistel
 * azimuth protocol.
 */
/**
 * \brief A macro that returns the model number of `PROSISTEL_D_EL`.
 *
 * \def ROT_MODEL_PROSISTEL_D_EL
 *
 * The `PROSISTEL_D_EL` model can be used with rotators that support the Prosistel
 * elevation protocol.
 */
/**
 * \brief A macro that returns the model number of `PROSISTEL_COMBI_TRACK_AZEL`.
 *
 * \def ROT_MODEL_PROSISTEL_COMBI_TRACK_AZEL
 *
 * The `PROSISTEL_AZEL_COMBI_TRACK_AZEL` model can be used with rotators that
 * support the Prosistel combination azimuth and elevation protocol.
 */
/**
 * \brief A macro that returns the model number of `PROSISTEL_D_EL_CBOXAZ`.
 *
 * \def ROT_MODEL_PROSISTEL_D_EL_CBOXAZ
 *
 * The `PROSISTEL_D_EL_CBOXAZ` model can be used with the elevation rotator
 * with Control Box D using azimuth logic.
 */
#define ROT_MODEL_PROSISTEL_D_AZ ROT_MAKE_MODEL(ROT_PROSISTEL, 1)
#define ROT_MODEL_PROSISTEL_D_EL ROT_MAKE_MODEL(ROT_PROSISTEL, 2)
#define ROT_MODEL_PROSISTEL_COMBI_TRACK_AZEL ROT_MAKE_MODEL(ROT_PROSISTEL, 3)
#define ROT_MODEL_PROSISTEL_D_EL_CBOXAZ ROT_MAKE_MODEL(ROT_PROSISTEL, 4)


/** The `MEADE` family. */
#define ROT_MEADE 18
/** Used in register.c for the `be_name`. */
#define ROT_BACKEND_MEADE "meade"

/**
 * \brief A macro that returns the model number of `MEADE`.
 *
 * \def ROT_MODEL_MEADE
 *
 * The `MEADE` model can be used with Meade telescope rotators like the
 * DS-2000.
 */
#define ROT_MODEL_MEADE ROT_MAKE_MODEL(ROT_MEADE, 1)


/** The `IOPTRON` family. */
#define ROT_IOPTRON 19
/** Used in register.c for the `be_name`. */
#define ROT_BACKEND_IOPTRON "ioptron"

/**
 * \brief A macro that returns the model number of `IOPTRON`.
 *
 * \def ROT_MODEL_IOPTRON
 *
 * The `IOPTRON` model can be used with IOPTRON telescope mounts.
 */
#define ROT_MODEL_IOPTRON ROT_MAKE_MODEL(ROT_IOPTRON, 1)


/** The `INDI` family. */
#define ROT_INDI 20
/** Used in register.c for the `be_name`. */
#define ROT_BACKEND_INDI "indi"

/**
 * \brief A macro that returns the model number of `INDI`.
 *
 * \def ROT_MODEL_INDI
 *
 * The `INDI` model can be used with rotators that support the INDI interface.
 */
#define ROT_MODEL_INDI ROT_MAKE_MODEL(ROT_INDI, 1)


/** The `SATEL` family. */
#define ROT_SATEL 21
/** Used in register.c for the `be_name`. */
#define ROT_BACKEND_SATEL "satel"

/**
 * \brief A macro that returns the model number of `SATEL`.
 *
 * \def ROT_MODEL_SATEL
 *
 * The `SATEL` model can be used with rotators that support the VE5FP
 * interface.
 */
#define ROT_MODEL_SATEL ROT_MAKE_MODEL(ROT_SATEL, 1)


/** The `RADANT` family. */
#define ROT_RADANT 22
/** Used in register.c for the `be_name`. */
#define ROT_BACKEND_RADANT "radant"

/**
 * \brief A macro that returns the model number of `RADANT`.
 *
 * \def ROT_MODEL_RADANT
 *
 * The `RADANT` model can be used with rotators that support the MS232
 * interface.
 */
#define ROT_MODEL_RADANT ROT_MAKE_MODEL(ROT_RADANT, 1)


/** The `ANDROIDSENSOR` family. */
#define ROT_ANDROIDSENSOR 23
/** Used in register.c for the `be_name`. */
#define ROT_BACKEND_ANDROIDSENSOR "androidsensor"

/**
 * \brief A macro that returns the model number of `ANDROIDSENSOR`.
 *
 * \def ROT_MODEL_ANDROIDSENSOR
 *
 * The androidsensor rotator is not a real rotator, it uses the accelerometer
 * sensor and magnetic field sensor of the cell phone or tablet to perform
 * attitude determination for your antenna and the phone tied to it.  Now you
 * can wave your antenna to find radio signals.
 */
#define ROT_MODEL_ANDROIDSENSOR ROT_MAKE_MODEL(ROT_ANDROIDSENSOR, 1)


/** The `GRBLTRK` family. */
#define ROT_GRBLTRK 24
/** Used in register.c for the `be_name`. */
#define ROT_BACKEND_GRBLTRK "grbltrk"

/**
 * \brief A macro that returns the model number of `ROT_MODEL_GRBLTRK_SER`.
 *
 * \def ROT_MODEL_GRBLTRK_SER
 *
 * The `GRBLTRK_SER` model can be used with rotators that support the GRBL
 * serial protocol.
 */
/**
 * \brief A macro that returns the model number of `ROT_MODEL_GRBLTRK_NET`.
 *
 * \def ROT_MODEL_GRBLTRK_NET
 *
 * The `GRBLTRK_NET` model can be used with rotators that support the GRBL
 * network protocol.
 */
#define ROT_MODEL_GRBLTRK_SER ROT_MAKE_MODEL(ROT_GRBLTRK, 1)
#define ROT_MODEL_GRBLTRK_NET ROT_MAKE_MODEL(ROT_GRBLTRK, 2)


/** The `FLIR` family. */
#define ROT_FLIR 25
/** Used in register.c for the `be_name`. */
#define ROT_BACKEND_FLIR "flir"

/**
 * \brief A macro that returns the model number of `FLIR`.
 *
 * \def ROT_MODEL_FLIR
 *
 * The `FLIR` model can be used with FLIR and DirectedPercepition 
 * rotators using the PTU protocol (e.g. PTU-D48). Currently only 
 * the serial interface is supported and no ethernet.
 */
#define ROT_MODEL_FLIR ROT_MAKE_MODEL(ROT_FLIR, 1)


/** The `APEX` family. */
#define ROT_APEX 26
/** Used in register.c for the `be_name`. */
#define ROT_BACKEND_APEX "apex"

/**
 * \brief A macro that returns the model number of `APEX`.
 *
 * \def ROT_MODEL_APEX_SHARED_LOOP
 *
 * The `APEX` model can be used with APEX * rotators. 
 */
#define ROT_MODEL_APEX_SHARED_LOOP ROT_MAKE_MODEL(ROT_APEX, 1)


/** The `SAEBRTRACK` family. */
#define ROT_SAEBRTRACK 27
/** Used in register.c for the `be_name`. */
#define ROT_BACKEND_SAEBRTRACK "SAEBRTrack"

/**
 * \brief A macro that returns the model number of `SAEBRTRACK`.
 *
 * \def ROT_MODEL_SAEBRTRACK
 *
 * The `SAEBRTRACK` model can be used with SAEBRTRACK * rotators. 
 */
#define ROT_MODEL_SAEBRTRACK ROT_MAKE_MODEL(ROT_SAEBRTRACK, 1)


/** The `SKYWATCHER` family. */
#define ROT_SKYWATCHER 28
/** Used in register.c for the `be_name`. */
#define ROT_BACKEND_SKYWATCHER "SkyWatcher"

/**
 * \brief A macro that returns the model number of `SKYWATCHER`.
 *
 * \def ROT_MODEL_SKYWATCHER
 *
 * The `SKYWATCHER` model can be used with SKYWATCHER * rotators.
 */
#define ROT_MODEL_SKYWATCHER ROT_MAKE_MODEL(ROT_SKYWATCHER, 1)


/**
 * \brief Convenience type definition for a rotator model.
 *
 * \typedef typedef int rot_model_t
*/
typedef int rot_model_t;


#endif /* _ROTLIST_H */

/** @} */
