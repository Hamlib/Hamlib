/*
 *  Hamlib CI-V backend - low level communication header
 *  Copyright (c) 2000-2010 by Stephane Fillod
 *
 *
 *   This library is free software; you can redistribute it and/or modify
 *   it under the terms of the GNU Library General Public License as
 *   published by the Free Software Foundation; either version 2 of
 *   the License, or (at your option) any later version.
 *
 *   This program is distributed in the hope that it will be useful,
 *   but WITHOUT ANY WARRANTY; without even the implied warranty of
 *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *   GNU Library General Public License for more details.
 *
 *   You should have received a copy of the GNU Library General Public
 *   License along with this library; if not, write to the Free Software
 *   Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 *
 */

#ifndef _FRAME_H
#define _FRAME_H 1

#define MAXFRAMELEN 56

/*
 * helper functions 
 */
int make_cmd_frame(char frame[], char re_id, char ctrl_id, char cmd, int subcmd, const unsigned char *data, int data_len);

int icom_transaction (RIG *rig, int cmd, int subcmd, const unsigned char *payload, int payload_len, unsigned char *data, int *data_len);
int read_icom_frame(hamlib_port_t *p, unsigned char rxbuffer[]);

int rig2icom_mode(RIG *rig, rmode_t mode, pbwidth_t width, unsigned char *md, signed char *pd);
void icom2rig_mode(RIG *rig, unsigned char md, int pd, rmode_t *mode, pbwidth_t *width);

#endif /* _FRAME_H */
