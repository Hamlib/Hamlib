/*
 *  Hamlib Interface - network communication header
 *  Copyright (c) 2000-2008 by Stephane Fillod
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

#ifndef _NETWORK_H
#define _NETWORK_H 1

#include <hamlib/rig.h>
#include "iofunc.h"

__BEGIN_DECLS

/* Hamlib internal use, see rig.c */
int network_open(hamlib_port_t *p, int default_port);
int network_close(hamlib_port_t *rp);
void network_flush(hamlib_port_t *rp);
int network_publish_rig_poll_data(RIG *rig);
int network_publish_rig_transceive_data(RIG *rig);
int network_publish_rig_spectrum_data(RIG *rig, struct rig_spectrum_line *line);
HAMLIB_EXPORT(int) network_multicast_publisher_start(RIG *rig, const char *multicast_addr, int multicast_port, enum multicast_item_e items);
HAMLIB_EXPORT(int) network_multicast_publisher_stop(RIG *rig);

__END_DECLS

#endif /* _NETWORK_H */
