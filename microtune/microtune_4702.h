/* -*- c++ -*- */
/*
 * Copyright 2001,2003 Free Software Foundation, Inc.
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

#ifndef _MICROTUNE_4702_H_
#define _MICROTUNE_4702_H_

#include <hamlib/rig.h>
#include "microtune_eval_board.h"

/*!
 * \brief abstract class for controlling microtune 4702 tuner module
 */
class microtune_4702 : public microtune_eval_board {
public:

  microtune_4702(hamlib_port_t *port) : microtune_eval_board (port), d_reference_divider(320), prescaler(false) {}
  ~microtune_4702() {}

  /*!
   * \brief select RF frequency to be tuned to output frequency.
   * \p freq is the requested frequency in Hz, \p actual_freq
   * is set to the actual frequency tuned.  It takes about 100 ms
   * for the PLL to settle.
   *
   * \returns true iff sucessful.
   */
  bool set_RF_freq (double freq, double *actual_freq);
  bool read_info (unsigned char* buf);
 
  double get_output_freq ();
  bool pll_locked_p ();

 private:
  int	d_reference_divider;
  bool	prescaler;	/* if set, higher charge pump current:
				   faster tuning, worse phase noise
				   for distance < 10kHz to the carrier */
};

#endif /* _MICROTUNE_4702_H_ */
