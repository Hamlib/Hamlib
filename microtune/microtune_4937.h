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

#ifndef _MICROTUNE_4937_H_
#define _MICROTUNE_4937_H_

#include <hamlib/rig.h>
#include "microtune_eval_board.h"

/*!
 * \brief abstract class for controlling microtune 4937 tuner module
 */
class microtune_4937 : public microtune_eval_board {
public:
  microtune_4937(hamlib_port_t *port) : microtune_eval_board (port), d_reference_divider(640), d_fast_tuning_p(false) {}
  ~microtune_4937() {}

  /*!
   * \brief select RF frequency to be tuned to output frequency.
   * \p freq is the requested frequency in Hz, \p actual_freq
   * is set to the actual frequency tuned.  It takes about 100 ms
   * for the PLL to settle.
   *
   * \returns true iff sucessful.
   */
  bool set_RF_freq (double freq, double *actual_freq);
  
  /*!
   * \returns true iff PLL is locked
   */
  bool pll_locked_p ();
  
  /*!
   * \returns the output frequency (IF center freq) of the tuner in Hz.
   */
  double get_output_freq ();

 private:
  int	d_reference_divider;
  bool	d_fast_tuning_p;	/* if set, higher charge pump current:
				   faster tuning, worse phase noise
				   for distance < 10kHz to the carrier */
};

#endif /* _MICROTUNE_4937_H_ */
