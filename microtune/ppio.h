/* -*-C++-*-
*******************************************************************************
*
* File:         ppio.h
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

#ifndef _PPIO_H_
#define _PPIO_H_

/*! \brief provide low level access to parallel port bits */

class ppio {
 public:
  //! \p which specifies the parallel port to use [0,2]
  ppio (int a_which = 1);
  virtual ~ppio () {};

  virtual void write_data (unsigned char v);
  virtual unsigned char read_data ();
  virtual void write_control (unsigned char v);
  virtual unsigned char read_control ();
  virtual unsigned char read_status ();

 private:
  int	base;
};


#endif /* _PPIO_H_ */
