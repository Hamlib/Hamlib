/*
 *  Hamlib Interface - Port structure
 *  Copyright (c) 2000-2025 The Hamlib Group
 *  Copyright (c) 2025 George Baltz
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

#ifndef _HL_PORT_H
#define _HL_PORT_H 1


__BEGIN_DECLS

/**
 * \addtogroup port
 * @{
 */

/**
 * \brief Hamlib port data structures.
 *
 * \file port.h
 *
 * This file contains the data structures and declarations for the Hamlib
 * port Application Programming Interface (API).
 */

/**
 * \brief Port definition
 *
 * Of course, looks like OO painstakingly programmed in C, sigh.
 *
 * \warning
 * DO NOT CHANGE THIS STRUCTURE AT ALL UNTIL 5.0.
 * Right now it is static inside the rig structure.
 * 5.0 will change it to a pointer which can then be added to.
 * At that point only add to the end of the structure.
 */
typedef struct hamlib_port {
    union {
        rig_port_t rig;     /*!< Communication port of #rig_port_e type. */
        ptt_type_t ptt;     /*!< PTT port of #ptt_type_e type. */
        dcd_type_t dcd;     /*!< DCD port of #dcd_type_e type. */
    } type;                 /*!< Type of port in use.*/

    int fd;                 /*!< File descriptor */
    void *handle;           /*!< handle for USB */

    int write_delay;        /*!< Delay between each byte sent out, in mS */
    int post_write_delay;   /*!< Delay between each commands send out, in mS */

    struct {
        int tv_sec, tv_usec;
    } post_write_date;      /*!< hamlib internal use */

    int timeout;            /*!< Timeout, in mS */
    short retry;            /*!< Maximum number of retries, 0 to disable */
    short flushx;           /*!< If true flush is done with read instead of TCFLUSH - MicroHam */

    char pathname[HAMLIB_FILPATHLEN];      /*!< Port pathname */

    union {
        struct {
            int rate;       /*!< Serial baud rate */
            int data_bits;  /*!< Number of data bits */
            int stop_bits;  /*!< Number of stop bits */
            enum serial_parity_e parity;        /*!< Serial parity */
            enum serial_handshake_e handshake;  /*!< Serial handshake */
            enum serial_control_state_e rts_state;    /*!< RTS set state */
            enum serial_control_state_e dtr_state;    /*!< DTR set state */
        } serial;           /*!< serial attributes */

        struct {
            int pin;        /*!< Parallel port pin number */
        } parallel;         /*!< parallel attributes */

        struct {
            int ptt_bitnum; /*!< Bit number for CM108 GPIO PTT */
        } cm108;            /*!< CM108 attributes */

        struct {
            int vid;        /*!< Vendor ID */
            int pid;        /*!< Product ID */
            int conf;       /*!< Configuration */
            int iface;      /*!< interface */
            int alt;        /*!< alternate */
            char *vendor_name;  /*!< Vendor name (opt.) */
            char *product;      /*!< Product (opt.) */
        } usb;              /*!< USB attributes */

        struct {
            int on_value;   /*!< GPIO: 1 == normal, GPION: 0 == inverted */
            int value;      /*!< Toggle PTT ON or OFF */
        } gpio;             /*!< GPIO attributes */
    } parm;                 /*!< Port parameter union */
    int client_port;        /*!< client socket port for tcp connection */
    RIG *rig;               /*!< our parent RIG device */
    int asyncio;            /*!< enable asynchronous data handling if true -- async collides with python keyword so _async is used */
#if defined(_WIN32)
    hamlib_async_pipe_t *sync_data_pipe;         /*!< pipe data structure for synchronous data */
    hamlib_async_pipe_t *sync_data_error_pipe;   /*!< pipe data structure for synchronous data error codes */
#else
    int fd_sync_write;          /*!< file descriptor for writing synchronous data */
    int fd_sync_read;           /*!< file descriptor for reading synchronous data */
    int fd_sync_error_write;    /*!< file descriptor for writing synchronous data error codes */
    int fd_sync_error_read;     /*!< file descriptor for reading synchronous data error codes */
#endif
    short timeout_retry;    /*!< number of retries to make in case of read timeout errors, some serial interfaces may require this, 0 to disable */
// DO NOT ADD ANYTHING HERE UNTIL 5.0!!
} hamlib_port_t;


/**
 * \deprecated
 * This structure will be removed in 5.0 and should not be used in new code.
 *
 * \warning
 * DO NOT CHANGE THIS STRUCTURE AT ALL!
 * Will be removed in 5.0.
 */
typedef struct hamlib_port_deprecated {
    union {
        rig_port_t rig;     /*!< Communication port type */
        ptt_type_t ptt;     /*!< PTT port type */
        dcd_type_t dcd;     /*!< DCD port type */
    } type;                 /*!< Type of port in use.*/

    int fd;                 /*!< File descriptor */
    void *handle;           /*!< handle for USB */

    int write_delay;        /*!< Delay between each byte sent out, in mS */
    int post_write_delay;   /*!< Delay between each commands send out, in mS */

    struct {
        int tv_sec, tv_usec;
    } post_write_date;      /*!< hamlib internal use */

    int timeout;            /*!< Timeout, in mS */
    short retry;            /*!< Maximum number of retries, 0 to disable */
    short flushx;           /*!< If true flush is done with read instead of TCFLUSH - MicroHam */

    char pathname[HAMLIB_FILPATHLEN];      /*!< Port pathname */

    union {
        struct {
            int rate;       /*!< Serial baud rate */
            int data_bits;  /*!< Number of data bits */
            int stop_bits;  /*!< Number of stop bits */
            enum serial_parity_e parity;        /*!< Serial parity */
            enum serial_handshake_e handshake;  /*!< Serial handshake */
            enum serial_control_state_e rts_state;    /*!< RTS set state */
            enum serial_control_state_e dtr_state;    /*!< DTR set state */
        } serial;           /*!< serial attributes */

        struct {
            int pin;        /*!< Parallel port pin number */
        } parallel;         /*!< parallel attributes */

        struct {
            int ptt_bitnum; /*!< Bit number for CM108 GPIO PTT */
        } cm108;            /*!< CM108 attributes */

        struct {
            int vid;        /*!< Vendor ID */
            int pid;        /*!< Product ID */
            int conf;       /*!< Configuration */
            int iface;      /*!< interface */
            int alt;        /*!< alternate */
            char *vendor_name;  /*!< Vendor name (opt.) */
            char *product;      /*!< Product (opt.) */
        } usb;              /*!< USB attributes */

        struct {
            int on_value;   /*!< GPIO: 1 == normal, GPION: 0 == inverted */
            int value;      /*!< Toggle PTT ON or OFF */
        } gpio;             /*!< GPIO attributes */
    } parm;                 /*!< Port parameter union */
    int client_port;      /*!< client socket port for tcp connection */
    RIG *rig;             /*!< our parent RIG device */
} hamlib_port_t_deprecated;

#if !defined(__APPLE__) || !defined(__cplusplus)
//! @deprecated Obsolete port type
typedef hamlib_port_t_deprecated port_t_deprecated;

//! Short type name of the hamlib_port structure.
typedef hamlib_port_t port_t;
#endif

///@{
/// Macro for application access to #hamlib_port_t data for this port type.
#define HAMLIB_RIGPORT(r) ((hamlib_port_t *)rig_data_pointer((r), RIG_PTRX_RIGPORT))
#define HAMLIB_PTTPORT(r) ((hamlib_port_t *)rig_data_pointer((r), RIG_PTRX_PTTPORT))
#define HAMLIB_DCDPORT(r) ((hamlib_port_t *)rig_data_pointer((r), RIG_PTRX_DCDPORT))
#define HAMLIB_AMPPORT(a) ((hamlib_port_t *)amp_data_pointer((a), RIG_PTRX_AMPPORT))
#define HAMLIB_ROTPORT(r) ((hamlib_port_t *)rot_data_pointer((r), RIG_PTRX_ROTPORT))
#define HAMLIB_ROTPORT2(r) ((hamlib_port_t *)rot_data_pointer((r), RIG_PTRX_ROTPORT2))
///@}

__END_DECLS

#endif /* _HL_PORT_H */

/** @} */
