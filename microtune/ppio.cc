/* -*-C++-*-
*******************************************************************************
*
* File:         ppio.cc
* Description:  
*
*******************************************************************************
*/

/*
 * Copyright 2001 Free Software Foundation, Inc.
 * 
 * This file is part of GNU Radio
 * 
 * GNU Radio is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2, or (at your option)
 * any later version.
 * 
 * GNU Radio is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 * 
 * You should have received a copy of the GNU General Public License
 * along with GNU Radio; see the file COPYING.  If not, write to
 * the Free Software Foundation, Inc., 59 Temple Place - Suite 330,
 * Boston, MA 02111-1307, USA.
 */

#include "ppio.h"
#include <stdlib.h>
#include <sys/io.h>


const static int parallel_port_base[3] = {
  0x3bc,	// lp0 used to be on monochome display adapter
  0x378,	// lp1 most common
  0x278		// lp2 secondary
};

// These control port bits are active low.
// We toggle them so that this weirdness doesn't get get propagated
// through our interface.

static int CP_ACTIVE_LOW_BITS	= 0x0B;

// These status port bits are active low.
// We toggle them so that this weirdness doesn't get get propagated
// through our interface.

static int SP_ACTIVE_LOW_BITS	= 0x80;

ppio::ppio (int which)
{
  if (which < 0 || which > 2)
    abort ();

  base = parallel_port_base[which];

  // try to read ports.  If we don't have the appropriate
  // ioperm bits set, we'll segfault here.  At least it's deterministic...

  inb (base + 0);
  inb (base + 1);
  inb (base + 2);
}

void 
ppio::write_data (unsigned char v)
{
  outb (v, base + 0);
}

unsigned char
ppio::read_data ()
{
  return inb (base + 0);
}

void 
ppio::write_control (unsigned char v)
{
  outb (v ^ CP_ACTIVE_LOW_BITS, base + 2);
}

unsigned char
ppio::read_control ()
{
  return inb (base + 2) ^ CP_ACTIVE_LOW_BITS;
}

unsigned char
ppio::read_status ()
{
  return inb (base + 1) ^ SP_ACTIVE_LOW_BITS;
}
