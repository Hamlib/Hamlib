/*
 * hamlib - (C) Frank Singleton 2000 (vk3fcs@ix.netcom.com)
 *
 * yaesu.h - (C) Frank Singleton 2000 (vk3fcs@ix.netcom.com)
 * Common yaesu declarations for hamlib
 *
 * 	$Id: yaesu.h,v 1.7 2001-12-15 03:23:14 aa1vl Exp $	
 *
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

#ifndef _YAESU_H
#define _YAESU_H 1


/*
 * Length (in bytes) of yaesu command sequences
 */

#define YAESU_CMD_LENGTH                     5

/*
 * Basic Data structure for yaesu native cmd set
 */

struct yaesu_cmd_set {
  unsigned char ncomp;		        /* 1 = complete, 0 = incomplete, needs extra info */
  unsigned char nseq[YAESU_CMD_LENGTH];	/* native cmd sequence */
};

typedef struct yaesu_cmd_set yaesu_cmd_set_t; 

extern const struct rig_caps ft747_caps;
extern const struct rig_caps ft817_caps;
extern const struct rig_caps ft847_caps;

extern HAMLIB_EXPORT(int) init_yaesu(void *be_handle);

#endif /* _YAESU_H */
