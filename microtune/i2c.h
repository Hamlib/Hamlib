/* -*-C++-*-
*******************************************************************************
*
* File:         i2c.h
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

#ifndef _I2C_H_
#define _I2C_H_

#include "i2cio.h"

/*!
 * \brief class for controlling i2c bus
 */
class i2c {
 public:
  
  /*! i2c does not control lifetime of a_io */
  i2c (i2cio *io);
  ~i2c () {};
  
  //! \returns true iff successful
  bool write (int addr, const unsigned char *buf, int nbytes);

  //! \returns number of bytes read or -1 if error
  int read (int addr, unsigned char *buf, int max_bytes);


private:
  void start ();
  void stop ();
  void write_bit (bool bit);
  bool write_byte (char byte);
  
  void set_sda (bool bit) { d_io->set_sda (bit); }
  void set_scl (bool bit) { d_io->set_scl (bit); }
  bool get_sda () { return d_io->get_sda (); }

  i2cio	*d_io;
};

#endif /* _I2C_H_ */
