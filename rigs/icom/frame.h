/*
 *  Hamlib CI-V backend - low level communication header
 *  Copyright (c) 2000-2010 by Stephane Fillod
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

#ifndef _FRAME_H
#define _FRAME_H 1

// Has to be big enough for 0xfe sequence to wake up rig
#define MAXFRAMELEN 200

/*
 * helper functions
 */
int make_cmd_frame(unsigned char frame[], unsigned char re_id, unsigned char ctrl_id,
                   unsigned char cmd, int subcmd,
                   const unsigned char *data, int data_len);
int icom_frame_fix_preamble(int frame_len, unsigned char *frame);

int icom_transaction (RIG *rig, int cmd, int subcmd, const unsigned char *payload, int payload_len, unsigned char *data, int *data_len);
int read_icom_frame(hamlib_port_t *p, const unsigned char rxbuffer[], size_t rxbuffer_len);
int read_icom_frame_direct(hamlib_port_t *p, const unsigned char rxbuffer[], size_t rxbuffer_len);

int rig2icom_mode(RIG *rig, vfo_t vfo, rmode_t mode, pbwidth_t width, unsigned char *md, signed char *pd);
void icom2rig_mode(RIG *rig, unsigned char md, int pd, rmode_t *mode, pbwidth_t *width);

#endif /* _FRAME_H */
