/*  hamlib - (C) Frank Singleton 2000 (vk3fcs@ix.netcom.com)
 *
 * rig.h - Copyright (C) 2000 Frank Singleton and Stephane Fillod
 * This program defines the Hamlib API
 *
 *
 * 	$Id: rig.h,v 1.4 2000-09-14 00:52:27 f4cfe Exp $	 *
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

#ifndef _RIG_H
#define _RIG_H 1

#include <rigcaps.h>

enum rig_parity_e;	/* forward reference to <rigcaps.h> */ 

struct rig_state {
	unsigned short int serial_rate;
	unsigned char serial_data_bits; /* eg 8 */
	unsigned char serial_stop_bits; /* eg 2 */
	enum rig_parity_e serial_parity; /* */
	char rig_path[MAXRIGPATHLEN]; /* requested serial port or network path(host:port) */
	int fd;	/* serial port/socket file handle */
	void *priv;  /* pointer to private data */
	/* stuff like CI_V_address for Icom goes in the *priv 51 area */

	/* etc... */
};

/*
 * event based programming,
 * really appropriate in a GUI.
 * So far, haven't heard of any rig capable of this. --SF
 */
struct rig_callbacks {
/* the rig notify the host computer someone changed 
 * the freq/mode from the panel 
 */ 
	int (*freq_main_vfo_hz_event)(struct rig_state *rig_s, freq_t freq, rig_mode_t mode); 
	/* etc.. */
};

/* Here is the master data structure, 
 * acting as a handle 
 */
struct rig {
	const struct rig_caps *caps;
	struct rig_state state;
	struct rig_callback callbacks;
};

typedef struct rig RIG;


/* --------------- visible function prototypes -----------------*/

RIG *rig_open(const char *rig_path, rig_model_t rig_model);
int cmd_set_freq_main_vfo_hz(RIG *rig, freq_t freq, rig_mode_t mode);
/* etc. */

int rig_close(RIG *rig);
RIG *rig_probe(const char *rig_path);

const struct rig_caps *rig_get_caps(rig_model_t rig_model);

#endif /* _RIG_H */

