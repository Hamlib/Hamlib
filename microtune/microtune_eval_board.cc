/* -*-C++-*-
*******************************************************************************
*
* File:         microtune_eval_board.cc
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

#include "microtune_eval_board.h"
#include "microtune_eval_board_defs.h"
#include "ppio.h"
#include "i2cio_pp.h"
#include "i2c.h"

microtune_eval_board::microtune_eval_board (int which_pp)
{
  m_ppio = new ppio (which_pp);
  m_i2cio = new i2cio_pp (m_ppio);
  m_i2c = new i2c (m_i2cio);

  // disable upstream amplifier
  int	t = m_ppio->read_data ();
  t &= ~(UT_DP_TX_ENABLE | UT_DP_TX_SDA | UT_DP_TX_SCL);
  t |= UT_DP_TX_AS;
  m_ppio->write_data (t);
}

microtune_eval_board::~microtune_eval_board ()
{
  delete m_i2c;
  delete m_i2cio;
  delete m_i2c;
}


//! is the eval board present?
bool 
microtune_eval_board::board_present_p ()
{
  int	t = m_ppio->read_status ();
  if ((t & UT_SP_SHOULD_BE_ZERO) != 0
      || (t & UT_SP_SHOULD_BE_ONE) != UT_SP_SHOULD_BE_ONE)
    return false;

  // could also see if SCL is looped back or not, but that seems like overkill

  return true;
}

// returns true iff successful
bool 
microtune_eval_board::i2c_write (int addr, const unsigned char *buf, int nbytes)
{
  return m_i2c->write (addr, buf, nbytes);
}

// returns number of bytes read or -1 if error
int
microtune_eval_board::i2c_read (int addr, unsigned char *buf, int max_bytes)
{
  return m_i2c->read (addr, buf, max_bytes);
}
