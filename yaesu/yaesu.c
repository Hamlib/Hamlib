/*
 * hamlib - (C) Frank Singleton 2000 (vk3fcs@ix.netcom.com)
 *
 * yaesu.c - (C) Stephane Fillod 2001,2002
 *
 * This shared library provides an API for communicating
 * via serial interface to a Yaesu rig
 *
 *
 *	$Id: yaesu.c,v 1.9 2002-11-21 23:09:28 fillods Exp $  
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


#include <stdlib.h>
#include <stdio.h>   /* Standard input/output definitions */
#include <string.h>  /* String function definitions */
#include <unistd.h>  /* UNIX standard function definitions */
#include <fcntl.h>   /* File control definitions */
#include <errno.h>   /* Error number definitions */
#include <termios.h> /* POSIX terminal control definitions */
#include <sys/ioctl.h>

#include <hamlib/rig.h>
#include "serial.h"
#include "misc.h"

#include "yaesu.h"


/*
 * initrigs_yaesu is called by rig_backend_load
 */

int initrigs_yaesu(void *be_handle)
{
  rig_debug(RIG_DEBUG_VERBOSE, "yaesu: _init called\n");

  rig_register(&ft747_caps); 
  rig_register(&ft817_caps);
  rig_register(&ft847_caps); 
  rig_register(&ft100_caps); 
  rig_register(&ft920_caps);
  rig_register(&ft1000mp_caps);

  return RIG_OK;
}

