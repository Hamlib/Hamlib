/*
 *  Hamlib CI-V backend - low level communication header
 *  Copyright (c) 2000,2001 by Stephane Fillod
 *
 *		$Id: frame.h,v 1.9 2001-09-19 21:17:35 f4cfe Exp $
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


/*
 * helper functions 
 */
int make_cmd_frame(char frame[], char re_id, char cmd, int subcmd, const char *data, int data_len);

int read_icom_block(port_t *p, unsigned char *rxbuffer, size_t count);
int icom_transaction (RIG *rig, int cmd, int subcmd, const char *payload, int payload_len, char *data, int *data_len);
int read_icom_frame(port_t *p, unsigned char rxbuffer[]);

unsigned short rig2icom_mode(RIG *rig, rmode_t mode, pbwidth_t width);
void icom2rig_mode(RIG *rig, unsigned short icmode, rmode_t *mode, pbwidth_t *width);

#endif /* _FRAME_H */

