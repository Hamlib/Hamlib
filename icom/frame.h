/*
 * hamlib - (C) Frank Singleton 2000 (vk3fcs@ix.netcom.com)
 *
 * frame.h - Copyright (C) 2000,2001 Stephane Fillod
 * This shared library provides an API for communicating
 * via serial interface to an ICOM using the "CI-V" interface.
 *
 *
 *    $Id: frame.h,v 1.6 2001-06-04 21:09:13 f4cfe Exp $  
 *
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 * 
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 * 
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.
 * 
 */

#ifndef _FRAME_H
#define _FRAME_H 1

#include <hamlib/rig.h>

/*
 * helper functions 
 */
int make_cmd_frame(char frame[], char re_id, char cmd, int subcmd, const char *data, int data_len);

int icom_transaction (RIG *rig, int cmd, int subcmd, const char *payload, int payload_len, char *data, int *data_len);
int read_icom_frame(port_t *p, unsigned char rxbuffer[]);

unsigned short rig2icom_mode(RIG *rig, rmode_t mode, pbwidth_t width);
void icom2rig_mode(RIG *rig, unsigned short icmode, rmode_t *mode, pbwidth_t *width);

#endif /* _FRAME_H */

