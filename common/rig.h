/*  hamlib - (C) Frank Singleton 2000 (vk3fcs@ix.netcom.com)
 *
 * rig.h - (C) Frank Singleton 2000 (vk3fcs@ix.netcom.com)
 * This program defines some rig capabilities structures that
 * will be used for obtaining rig capabilities,
 *
 *
 * 	$Id: rig.h,v 1.3 2000-09-04 17:51:28 javabear Exp $	 *
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

/*
 * Basic rig type, can store some useful
 * info about different radios. Each lib must
 * be able to populate this structure, so we can make
 * useful enquiries about capablilities.
 */

#define RIG_PARITY_NONE      0
#define RIG_PARITY_ODD       1
#define RIG_PARITY_EVEN      2

struct rig_caps {
  char rig_name[30];		/* eg ft847 */
  unsigned short int serial_rate_min;		/* eg 4800 */
  unsigned short int serial_rate_max;		/* eg 9600 */
  unsigned char serial_data_bits;	/* eg 8 */
  unsigned char serial_stop_bits;	/* eg 2 */
  unsigned char serial_parity;	/*  */
  char serial_port_name[30];		/* requested serial port */
};


/*
 * visible function prototypes
 *
 */

/*  int open_port2(struct rig_caps *rc); */










