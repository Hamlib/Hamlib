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
#include "serial.h"
#include "i2cio_pp.h"
#include "i2c.h"
#include <cmath>

static int AGC_DAC_I2C_ADDR =	0x2C;

microtune_eval_board::microtune_eval_board (hamlib_port_t *port)
{
  m_ppio = port;
  m_i2cio = new i2cio_pp (m_ppio);
  m_i2c = new i2c (m_i2cio);

  // disable upstream amplifier
  par_lock (m_ppio);
  unsigned char	t;
  par_read_data (m_ppio, &t);
  t &= ~(UT_DP_TX_ENABLE | UT_DP_TX_SDA | UT_DP_TX_SCL);
  t |= UT_DP_TX_AS;
  par_write_data (m_ppio, t);
  par_unlock (m_ppio);
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
  bool result = true;
  par_lock (m_ppio);

  unsigned char t;
  par_read_status (m_ppio, &t);
  if ((t & UT_SP_SHOULD_BE_ZERO) != 0
      || (t & UT_SP_SHOULD_BE_ONE) != UT_SP_SHOULD_BE_ONE)
    result = false;

  // could also see if SCL is looped back or not, but that seems like overkill

  par_unlock (m_ppio);
  return result;
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

/*
 * ----------------------------------------------------------------
 *			    AGC stuff
 *
 * We're using a MAX518 8-bit 5V dual dac for setting the AGC's
 * ----------------------------------------------------------------
 */
void 
microtune_eval_board::write_dac (int which, int value)
{
  unsigned char	cmd[2];
  cmd[0] = which & 1;
  cmd[1] = value;
  i2c_write (AGC_DAC_I2C_ADDR, cmd, sizeof (cmd));
}

void 
microtune_eval_board::write_both_dacs (int value0, int value1)
{
  unsigned char	cmd[4];
  cmd[0] = 0;
  cmd[1] = value0;
  cmd[2] = 1;
  cmd[3] = value1;
  i2c_write (AGC_DAC_I2C_ADDR, cmd, sizeof (cmd));
}

static int scale_volts (float volts)
{
  int	n;
  n = (int) rint (volts * (256 / 5.0));
  if (n < 0)
    n = 0;
  if (n > 255)
    n = 255;

  return n;
}

void
microtune_eval_board::set_RF_AGC_voltage (float volts)
{
  write_dac (0, scale_volts (volts));
}

void 
microtune_eval_board::set_IF_AGC_voltage (float volts)
{
  write_dac (1, scale_volts (volts));
}

static const float RF_MIN_V = 1.5;	// RF AGC control voltages
static const float RF_MAX_V = 4.0;
static const float IF_MIN_V = 2.0;	// IF AGC control voltages
static const float IF_MAX_V = 4.0;

static const float MIN_AGC  =    0;	// bottom of synthetic range
static const float MAX_AGC  = 1000;	// top of synthetic range

static const float CUTOVER_POINT = 667;


// linear is in the range MIN_AGC to MAX_AGC

static float
linear_to_RF_AGC_voltage (float linear)
{
  if (linear >= CUTOVER_POINT)
    return RF_MAX_V;

  float slope = (RF_MAX_V - RF_MIN_V) / CUTOVER_POINT;
  return RF_MIN_V + linear * slope;
}

static float
linear_to_IF_AGC_voltage (float linear)
{
  if (linear < CUTOVER_POINT)
    return IF_MIN_V;

  float slope = (IF_MAX_V - IF_MIN_V) / (MAX_AGC - CUTOVER_POINT);
  return IF_MIN_V + (linear - CUTOVER_POINT) * slope;
}

void
microtune_eval_board::set_AGC (float v)
{
  if (v < MIN_AGC)
    v = MIN_AGC;

  if (v > MAX_AGC)
    v = MAX_AGC;

  float rf_agc_voltage = linear_to_RF_AGC_voltage (v);
  float if_agc_voltage = linear_to_IF_AGC_voltage (v);

  set_RF_AGC_voltage (rf_agc_voltage);
  set_IF_AGC_voltage (if_agc_voltage);
}
