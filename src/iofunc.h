/*
 *  Hamlib Interface - io function header
 *  Copyright (c) 2000-2005 by Stephane Fillod and Frank Singleton
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

#ifndef _IOFUNC_H
#define _IOFUNC_H 1
 
#include <sys/types.h>
#include <hamlib/rig.h>

extern HAMLIB_EXPORT(int) port_open(hamlib_port_t *p);
extern HAMLIB_EXPORT(int) port_close(hamlib_port_t *p, rig_port_t port_type);


extern HAMLIB_EXPORT(int) read_block(hamlib_port_t *p,
                                     unsigned char *rxbuffer,
                                     size_t count);

extern HAMLIB_EXPORT(int) read_block_direct(hamlib_port_t *p,
                                            unsigned char *rxbuffer,
                                            size_t count);

extern HAMLIB_EXPORT(int) write_block(hamlib_port_t *p,
                                      const unsigned char *txbuffer,
                                      size_t count);

extern HAMLIB_EXPORT(int) write_block_sync(hamlib_port_t *p,
                                           const unsigned char *txbuffer,
                                           size_t count);

extern HAMLIB_EXPORT(int) write_block_sync_error(hamlib_port_t *p,
                                                 const unsigned char *txbuffer,
                                                 size_t count);

extern HAMLIB_EXPORT(int) read_string(hamlib_port_t *p,
                                      unsigned char *rxbuffer,
                                      size_t rxmax,
                                      const char *stopset,
                                      int stopset_len,
                                      int flush_flag,
                                      int expected_len);

extern HAMLIB_EXPORT(int) read_string_direct(hamlib_port_t *p,
                                             unsigned char *rxbuffer,
                                             size_t rxmax,
                                             const char *stopset,
                                             int stopset_len,
                                             int flush_flag,
                                             int expected_len);

#endif /* _IOFUNC_H */
