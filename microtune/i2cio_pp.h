/* -*-C-*-
*******************************************************************************
*
* File:         i2cio_pp.h
* Description:  i2cio class for parallel port
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

#ifndef _I2CIO_PP_H_
#define _I2CIO_PP_H_

#include "i2cio.h"
#include "parallel.h"

/*!
 * \brief concrete class that implements low level i/o for i2c bus using parallel port
 */
class i2cio_pp : public i2cio {
 public:

  i2cio_pp (hamlib_port_t *a_pp);

  virtual void set_scl (bool state);
  virtual void set_sda (bool state);
  virtual bool get_sda ();

  virtual void lock ();
  virtual void unlock ();

 private:
  hamlib_port_t	*d_pp;
};

#endif /* _I2CIO_PP_H_ */
