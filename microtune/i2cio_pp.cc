/* -*-C++-*-
*******************************************************************************
*
* File:         i2cio_pp.cc
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

#include "i2cio_pp.h"
#include "microtune_eval_board_defs.h"

i2cio_pp::i2cio_pp (hamlib_port_t *pp)
{
  unsigned char	r;
  d_pp = pp;
  par_lock (d_pp);
  par_read_control (d_pp, &r);
  par_write_control (d_pp, r & ~UT_CP_MUST_BE_ZERO);	// output, no interrupts
  par_unlock (d_pp);
}

void
i2cio_pp::set_scl (bool state)
{
  unsigned char	r;
  par_read_control(d_pp, &r);

  if (!state){					// active low
    par_write_control (d_pp, r | UT_CP_TUNER_SCL);
  }
  else {
    par_write_control (d_pp, r & ~UT_CP_TUNER_SCL);
  }
  par_read_control (d_pp, &r);	// use for 1us delay
  par_read_control (d_pp, &r);	// use for 1us delay
}

void
i2cio_pp::set_sda (bool state)
{
  unsigned char	r;
  par_read_data (d_pp, &r);

  if (!state){					// active low
    par_write_data (d_pp, r | UT_DP_TUNER_SDA_OUT);
  }
  else {
    par_write_data (d_pp, r & ~UT_DP_TUNER_SDA_OUT);
  }
  par_read_data (d_pp, &r);	// use for 1us delay
  par_read_data (d_pp, &r);	// use for 1us delay
}

bool
i2cio_pp::get_sda ()
{
  unsigned char	r;
  par_read_status (d_pp, &r);
  return (r & UT_SP_TUNER_SDA_IN) == 0;	// eval board has an inverter on it
}

void
i2cio_pp::lock ()
{
  par_lock (d_pp);
}

void
i2cio_pp::unlock ()
{
  par_unlock (d_pp);
}
