/*
 *  Hamlib Interface - gpio support header
 *  Copyright (c) 2016 by Jeroen Vreeken
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

#ifndef _GPIO_H
#define _GPIO_H

#include <hamlib/rig.h>


__BEGIN_DECLS

/* Hamlib internal use, see rig.c */
int gpio_open(hamlib_port_t *p, int output, int on_value);
int gpio_close(hamlib_port_t *p);
int gpio_ptt_set(hamlib_port_t *p, ptt_t pttx);
int gpio_ptt_get(hamlib_port_t *p, ptt_t *pttx);
int gpio_dcd_get(hamlib_port_t *p, dcd_t *dcdx);

__END_DECLS

#endif /* _GPIO_H */
