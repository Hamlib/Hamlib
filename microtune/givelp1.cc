/* -*-C++-*-
*******************************************************************************
*
* File:         givelp1.cc
* Description:  give a program direct access to LPT1 io ports
*
*******************************************************************************
*/

/*
 * Copyright 2001,2002 Free Software Foundation, Inc.
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

#include <sys/io.h>
#include <sys/types.h>
#include <unistd.h>
#include <stdlib.h>
#include <errno.h>
#include <stdio.h>

// which is 0, 1 or 2 to specify lp0, lp1 or lp2

const static int parallel_port_base[3] = {
  0x3bc,	// lp0 used to be on monochome display adapter
  0x378,	// lp1 most common
  0x278		// lp2 secondary
};


/*!
 * \brief enable direct parallel port io access from user land.
 * \p is in [0,2] and specifies which parallel port to access.
 *
 * \returns -1 on error, else base ioport address
 */
int 
enable_parallel_ioport_access (int which)
{
  errno = 0;
  if (which < 0 || which >= 3)
    return -1;

  if (ioperm (parallel_port_base[which], 3, 1) < 0)
    return -1;

  return parallel_port_base[which];
}

bool
reset_eids ()
{
  if (setgid (getgid ()) < 0){
    perror ("setguid");
    return false;
  }

  if (setuid (getuid ()) < 0){
    perror ("setuid");
    return false;
  }
  
  return true;
}


int main (int argc, char **argv)
{
  if (argc < 2){
    fprintf (stderr, "usage: givelp1 program-to-run [arg...]\n");
    exit (2);
  }
    
  if (enable_parallel_ioport_access (1) == -1){
    if (errno != 0){
      perror ("ioperm");
      fprintf (stderr, "givelp1 must be setuid root to use ioperm system call\n");
      fprintf (stderr, "Running as root, please execute:  \n");
      fprintf (stderr, "  # chown root givelp1\n");
      fprintf (stderr, "  # chmod u+s givelp1\n");
      exit (1);
    }
  }

  if (!reset_eids ()){
    fprintf (stderr, "Can't drop root permissions\n");
    exit (3);
  }

  // if successful, no return
  execvp (argv[1], argv+1);

  perror (argv[1]);
  exit (4);
}
