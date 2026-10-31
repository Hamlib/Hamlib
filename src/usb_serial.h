/*
 *  Hamlib Interface - USB-serial adapters driven from user space
 *  Copyright (c) 2026 by Rafael Diniz
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

#ifndef _USB_SERIAL_H
#define _USB_SERIAL_H 1

#include "hamlib/rig.h"

__BEGIN_DECLS

/*
 * A serial port whose pathname starts with "usb:" or "usbfd:" is a
 * USB-serial bridge chip that Hamlib drives itself through Linux usbfs, with
 * no kernel driver: an Android app gets no /dev/ttyUSB*, and some phone
 * kernels have no driver for the chip.  Chips: Silicon Labs CP210x, FTDI
 * (FT232R, FT2232H, FT4232H, FT232H, FT-X), WCH CH340/CH341, Prolific
 * PL2303, and any CDC-ACM device (by its interface class; "usb:" alone does
 * not pick one, name it by vendor:product).
 *
 *   usb:                       the first supported adapter
 *   usb:VVVV:PPPP              the first with this vendor:product (hex)
 *   usb:VVVV:PPPP:SERIAL       ...and this serial number string
 *   usb:/dev/bus/usb/BBB/DDD   this usbfs node
 *   usbfd:N                    an open usbfs descriptor, as Android's
 *                              UsbDeviceConnection.getFileDescriptor() gives
 *
 * Any of them may end in "@N": interface N of a chip with several ports
 * (the CP2105's second UART, an FT-710's Standard COM port); otherwise the
 * first interface with a bulk endpoint pair.
 *
 * usb_serial_open() returns a pseudo-terminal descriptor that the rest of
 * Hamlib reads, writes and polls as usual; bytes are pumped to and from the
 * chip's bulk endpoints.  Line settings and modem-control lines go to the
 * chip.  On other systems usb_serial_open() fails with -RIG_ENIMPL.
 */
extern int usb_serial_is_path(const char *pathname);
extern int usb_serial_is_fd(int fd);
extern int usb_serial_open(hamlib_port_t *rp);
extern int usb_serial_setup(hamlib_port_t *rp);
extern int usb_serial_set_rts(int fd, int state);
extern int usb_serial_set_dtr(int fd, int state);
extern int usb_serial_get_rts(int fd, int *state);
extern int usb_serial_get_dtr(int fd, int *state);
extern int usb_serial_get_status(int fd, int *cts, int *dsr, int *car);
extern void usb_serial_close(int fd);

__END_DECLS

#endif /* _USB_SERIAL_H */
