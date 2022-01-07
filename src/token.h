/*
 *  Hamlib Interface - token header
 *  Copyright (c) 2000-2009 by Stephane Fillod
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

/**
 * \addtogroup rig_internal
 * @{
 */

/**
 * \brief Token definitions
 * \file token.h
 *
 */

#ifndef _TOKEN_H

#define _TOKEN_H 1

#include <hamlib/rig.h>

/** \brief Create a backend token, \a t */
#define TOKEN_BACKEND(t) (t)
/** \brief Create a frontend token, \a t */
#define TOKEN_FRONTEND(t) ((t)|(1<<30))
/** \brief Test for token - frontend? */
#define IS_TOKEN_FRONTEND(t) ((t)&(1<<30))

/** \brief Null frontend token */
#define TOK_FRONTEND_NONE   TOKEN_FRONTEND(0)
/** \brief Null backend token */
#define TOK_BACKEND_NONE    TOKEN_BACKEND(0)

/*
 * tokens shared among rig and rotator,
 * Numbers go from TOKEN_FRONTEND(1) to TOKEN_FRONTEND(99)
 */

/** \brief Pathname is device for rig control, e.g. /dev/ttyS0 */
#define TOK_PATHNAME    TOKEN_FRONTEND(10)
/** \brief Delay before serial output (units?) */
#define TOK_WRITE_DELAY TOKEN_FRONTEND(12)
/** \brief Delay after serial output (units?) */
#define TOK_POST_WRITE_DELAY    TOKEN_FRONTEND(13)
/** \brief Timeout delay (units?) */
#define TOK_TIMEOUT     TOKEN_FRONTEND(14)
/** \brief Number of retries permitted */
#define TOK_RETRY       TOKEN_FRONTEND(15)
/** \brief Serial speed - "baud rate" */
#define TOK_SERIAL_SPEED    TOKEN_FRONTEND(20)
/** \brief No. data bits per serial character */
#define TOK_DATA_BITS   TOKEN_FRONTEND(21)
/** \brief  No. stop bits per serial character */
#define TOK_STOP_BITS   TOKEN_FRONTEND(22)
/** \brief  Serial parity (format?) */
#define TOK_PARITY      TOKEN_FRONTEND(23)
/** \brief Serial Handshake (format?)  */
#define TOK_HANDSHAKE   TOKEN_FRONTEND(24)
/** \brief Serial Req. To Send status */
#define TOK_RTS_STATE   TOKEN_FRONTEND(25)
/** \brief  Serial Data Terminal Ready status */
#define TOK_DTR_STATE   TOKEN_FRONTEND(26)
/** \brief  PTT type override */
#define TOK_PTT_TYPE    TOKEN_FRONTEND(30)
/** \brief  PTT pathname override */
#define TOK_PTT_PATHNAME    TOKEN_FRONTEND(31)
/** \brief  DCD type override */
#define TOK_DCD_TYPE    TOKEN_FRONTEND(32)
/** \brief  DCD pathname override */
#define TOK_DCD_PATHNAME    TOKEN_FRONTEND(33)
/** \brief  CM108 GPIO bit number for PTT */
#define TOK_PTT_BITNUM        TOKEN_FRONTEND(34)
/** \brief  PTT share with other applications */
#define TOK_PTT_SHARE        TOKEN_FRONTEND(35)
/** \brief  PTT share with other applications */
#define TOK_FLUSHX        TOKEN_FRONTEND(36)
/** \brief  Asynchronous data transfer support */
#define TOK_ASYNC        TOKEN_FRONTEND(37)

/*
 * rig specific tokens
 */
/* rx_range_list/tx_range_list, filters, announces, has(func,lvl,..) */

/** \brief rig: ?? */
#define TOK_VFO_COMP    TOKEN_FRONTEND(110)
/** \brief rig: Rig state poll interval in milliseconds */
#define TOK_POLL_INTERVAL   TOKEN_FRONTEND(111)
/** \brief rig: lo frequency of any transverters */
#define TOK_LO_FREQ         TOKEN_FRONTEND(112)
/** \brief rig: Range index 1-5 */
#define TOK_RANGE_SELECTED  TOKEN_FRONTEND(121)
/** \brief rig: Range Name */
#define TOK_RANGE_NAME  TOKEN_FRONTEND(122)
/** \brief rig: Cache timeout in milliseconds */
#define TOK_CACHE_TIMEOUT  TOKEN_FRONTEND(123)
/** \brief rig: Auto power on rig_open when supported */
#define TOK_AUTO_POWER_ON  TOKEN_FRONTEND(124)
/** \brief rig: Auto power off rig_close when supported */
#define TOK_AUTO_POWER_OFF  TOKEN_FRONTEND(125)
/** \brief rig: Auto disable screensaver */
#define TOK_AUTO_DISABLE_SCREENSAVER  TOKEN_FRONTEND(126)
/** \brief rig: Disable Yaesu band select logic */
#define TOK_DISABLE_YAESU_BANDSELECT  TOKEN_FRONTEND(127)
/** \brief rig: Supporess get_freq on VFOB for satellite RIT tuning */
#define TOK_TWIDDLE_TIMEOUT  TOKEN_FRONTEND(128)
/** \brief rig: Supporess get_freq on VFOB for satellite RIT tuning */
#define TOK_TWIDDLE_RIT  TOKEN_FRONTEND(129)
/*
 * rotator specific tokens
 * (strictly, should be documented as rotator_internal)
 */
/** \brief rot: Minimum Azimuth */
#define TOK_MIN_AZ  TOKEN_FRONTEND(110)
/** \brief rot: Maximum Azimuth */
#define TOK_MAX_AZ  TOKEN_FRONTEND(111)
/** \brief rot: Minimum Elevation */
#define TOK_MIN_EL  TOKEN_FRONTEND(112)
/** \brief rot: Maximum Elevation */
#define TOK_MAX_EL  TOKEN_FRONTEND(113)
/** \brief rot: South is zero degrees */
#define TOK_SOUTH_ZERO  TOKEN_FRONTEND(114)


#endif /* _TOKEN_H */

/** @} */ /* rig_internal definitions */
