/*
 * hamlib - (C) Frank Singleton 2000 (vk3fcs@ix.netcom.com)
 *
 * event.h - (C) 2000 Stephane Fillod 2000
 *
 *
 *    $Id: event.h,v 1.1 2000-10-08 21:19:26 f4cfe Exp $  
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


#ifndef _EVENT_H
#define _EVENT_H 1

#include <hamlib/rig.h>


int add_trn_rig(RIG *rig);
int remove_trn_rig(RIG *rig);

#endif /* _EVENT_H */

