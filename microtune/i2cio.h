/* -*-C++-*-
*******************************************************************************
*
* File:         i2cio.h
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

#ifndef _I2CIO_H_
#define _I2CIO_H_

/*!
 * \brief abstract class that implements low level i/o for i2c bus.
 */
class i2cio {
 public:

  i2cio () :
    udelay_scl_lo(0), udelay_scl_hi(0),
    udelay_sda_lo(0), udelay_sda_hi(0) {};

  virtual ~i2cio ();
  
  virtual void set_scl (bool state) = 0;
  virtual void set_sda (bool state) = 0;
  virtual bool get_sda () = 0;

  void set_udelay_scl_lo (int usecs) { udelay_scl_lo = usecs; }
  void set_udelay_scl_hi (int usecs) { udelay_scl_hi = usecs; }
  void set_udelay_sda_lo (int usecs) { udelay_sda_lo = usecs; }
  void set_udelay_sda_hi (int usecs) { udelay_sda_hi = usecs; }

  int get_udelay_scl_lo () { return udelay_scl_lo; }
  int get_udelay_scl_hi () { return udelay_scl_hi; }
  int get_udelay_sda_lo () { return udelay_sda_lo; }
  int get_udelay_sda_hi () { return udelay_sda_hi; }

  virtual void lock () = 0;
  virtual void unlock () = 0;
  
 private:
  int	udelay_scl_lo;
  int	udelay_scl_hi;
  int	udelay_sda_lo;
  int	udelay_sda_hi;
};


#endif /* _I2CIO_H_ */
