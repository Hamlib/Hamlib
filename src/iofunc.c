/*
 *  Hamlib Interface - generic file based io functions
 *  Copyright (c) 2000-2004 by Stephane Fillod and Frank Singleton
 *
 *	$Id: iofunc.c,v 1.14 2005-04-03 12:27:16 fillods Exp $
 *
 *   This library is free software; you can redistribute it and/or modify
 *   it under the terms of the GNU Library General Public License as
 *   published by the Free Software Foundation; either version 2 of
 *   the License, or (at your option) any later version.
 *
 *   This program is distributed in the hope that it will be useful,
 *   but WITHOUT ANY WARRANTY; without even the implied warranty of
 *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *   GNU Library General Public License for more details.
 *
 *   You should have received a copy of the GNU Library General Public
 *   License along with this library; if not, write to the Free Software
 *   Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 *
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <stdlib.h>
#include <stdio.h>   /* Standard input/output definitions */
#include <string.h>  /* String function definitions */
#include <unistd.h>  /* UNIX standard function definitions */
#include <fcntl.h>   /* File control definitions */
#include <errno.h>   /* Error number definitions */
#include <sys/time.h>
#include <sys/types.h>
#include <unistd.h>

#include "hamlib/rig.h"
#include "iofunc.h"
#include "misc.h"

#if defined(WIN32) && !defined(HAVE_TERMIOS_H)
#include "win32termios.h"
#define read win32_serial_read
#define write win32_serial_write
#define select win32_serial_select
#endif

/*
 * Write a block of count characters to file descriptor,
 * with a pause between each character if write_delay is > 0
 *
 * The write_delay is for Yaesu type rigs..require 5 character
 * sequence to be sent with 50-200msec between each char.
 *
 * Also, post_write_delay is for some Yaesu rigs (eg: FT747) that
 * get confused with sequential fast writes between cmd sequences.
 *
 *
 *
 * input:
 *
 * fd - file descriptor to write to
 * txbuffer - pointer to a command sequence array
 * count - count of byte to send from the txbuffer
 * write_delay - write delay in ms between 2 chars
 * post_write_delay - minimum delay between two writes
 * post_write_date - timeval of last write
 *
 * returns:
 *
 *  0 = OK
 *  <0 = NOT OK
 *
 * Actually, this function has nothing specific to serial comm,
 * it could work very well also with any file handle, like a socket.
 */

int HAMLIB_API write_block(hamlib_port_t *p, const char *txbuffer, size_t count)
{
  int i;

#ifdef WANT_NON_ACTIVE_POST_WRITE_DELAY
  if (p->post_write_date.tv_sec != 0) {
		  signed int date_delay;	/* in us */
		  struct timeval tv;

		  /* FIXME in Y2038 ... */
		  gettimeofday(&tv, NULL);
		  date_delay = p->post_write_delay*1000 - 
				  		((tv.tv_sec - p->post_write_date.tv_sec)*1000000 +
				  		 (tv.tv_usec - p->post_write_date.tv_usec));
		  if (date_delay > 0) {
				/*
				 * optional delay after last write 
				 */
				usleep(date_delay); 
		  }
		  p->post_write_date.tv_sec = 0;
  }
#endif

  if (p->write_delay > 0) {
  	for (i=0; i < count; i++) {
		if (write(p->fd, txbuffer+i, 1) < 0) {
			rig_debug(RIG_DEBUG_ERR,"write_block() failed - %s\n", 
							strerror(errno));
			return -RIG_EIO;
    	}
    	usleep(p->write_delay*1000);
  	}
  } else {
		  write(p->fd, txbuffer, count);
  }
  
  if (p->post_write_delay > 0) {
#ifdef WANT_NON_ACTIVE_POST_WRITE_DELAY
#define POST_WRITE_DELAY_TRSHLD 10

	if (p->post_write_delay > POST_WRITE_DELAY_TRSHLD) {
		struct timeval tv;
		gettimeofday(&tv, NULL);
		p->post_write_date.tv_sec = tv.tv_sec;
		p->post_write_date.tv_usec = tv.tv_usec;
	}
	else
#endif
    usleep(p->post_write_delay*1000); /* optional delay after last write */
				   /* otherwise some yaesu rigs get confused */
				   /* with sequential fast writes*/
  }
  rig_debug(RIG_DEBUG_TRACE,"TX %d bytes\n",count);
  dump_hex(txbuffer,count);
  
  return RIG_OK;
}


/*
 * Read "num" bytes from "fd" and put results into
 * an array of unsigned char pointed to by "rxbuffer"
 * 
 * Blocks on read until timeout hits.
 *
 * It then reads "num" bytes into rxbuffer.
 *
 * Actually, this function has nothing specific to serial comm,
 * it could work very well also with any file handle, like a socket.
 */

int HAMLIB_API read_block(hamlib_port_t *p, char *rxbuffer, size_t count)
{  
  fd_set rfds;
  struct timeval tv, tv_timeout;
  int rd_count, total_count = 0;
  int retval;

  FD_ZERO(&rfds);
  FD_SET(p->fd, &rfds);

  /*
   * Wait up to timeout ms.
   */
  tv_timeout.tv_sec = p->timeout/1000;
  tv_timeout.tv_usec = (p->timeout%1000)*1000;

  while (count > 0) {
		tv = tv_timeout;	/* select may have updated it */

		retval = select(p->fd+1, &rfds, NULL, NULL, &tv);
		if (retval == 0) {
			dump_hex(rxbuffer, total_count);
			rig_debug(RIG_DEBUG_WARN, "read_block: timedout after %d chars\n",
							total_count);
				return -RIG_ETIMEOUT;
		}
		if (retval < 0) {
			dump_hex(rxbuffer, total_count);
			rig_debug(RIG_DEBUG_ERR,"read_block: select error after %d chars: "
							"%s\n", total_count, strerror(errno));
				return -RIG_EIO;
		}

		/*
		 * grab bytes from the rig
		 * The file descriptor must have been set up non blocking.
		 */
  		rd_count = read(p->fd, rxbuffer+total_count, count);
		if (rd_count < 0) {
				rig_debug(RIG_DEBUG_ERR, "read_block: read failed - %s\n",
									strerror(errno));
				return -RIG_EIO;
		}
		total_count += rd_count;
		count -= rd_count;
  }

  rig_debug(RIG_DEBUG_TRACE,"RX %d bytes\n",total_count);
  dump_hex(rxbuffer, total_count);
  return total_count;			/* return bytes count read */
}


/*
 * Read a string from "fd" and put result into
 * an array of unsigned char pointed to by "rxbuffer"
 *
 * Blocks on read until timeout hits.
 *
 * It then reads characters until one of the characters in
 * "stopset" is found, or until "rxmax-1" characters was copied
 * into rxbuffer.  String termination character is added at the end.
 *
 * Actually, this function has nothing specific to serial comm,
 * it could work very well also with any file handle, like a socket.
 *
 * Assumes rxbuffer!=NULL
 */
int HAMLIB_API read_string(hamlib_port_t *p, char *rxbuffer, size_t rxmax, const char *stopset,
				int stopset_len)
{
  fd_set rfds;
  struct timeval tv, tv_timeout;
  int rd_count, total_count = 0;
  int retval;

  FD_ZERO(&rfds);
  FD_SET(p->fd, &rfds);

  /*
   * Wait up to timeout ms.
   */
  tv_timeout.tv_sec = p->timeout/1000;
  tv_timeout.tv_usec = (p->timeout%1000)*1000;

  while (total_count < rxmax-1) {
		tv = tv_timeout;	/* select may have updated it */

		retval = select(p->fd+1, &rfds, NULL, NULL, &tv);
        if (retval == 0)    /* Timed out */
            break;

		if (retval < 0) {
			dump_hex(rxbuffer, total_count);
            rig_debug(RIG_DEBUG_ERR, "%s: select error after %d chars: %s\n", 
				__FUNCTION__, total_count, strerror(errno));
            return -RIG_EIO;
		}
		/*
         * read 1 character from the rig, (check if in stop set)
		 * The file descriptor must have been set up non blocking.
		 */
        rd_count = read(p->fd, &rxbuffer[total_count], 1);
		if (rd_count < 0) {
			dump_hex(rxbuffer, total_count);
            rig_debug(RIG_DEBUG_ERR, "%s: read failed - %s\n",__FUNCTION__,
                            strerror(errno));
            return -RIG_EIO;
		}
        ++total_count;
		if (stopset && memchr(stopset, rxbuffer[total_count-1], stopset_len))
			break;
  }
  /*
   * Doesn't hurt anyway. But be aware, some binary protocols may have
   * null chars within th received buffer.
   */
  rxbuffer[total_count] = '\000';

  if (total_count == 0) {
    rig_debug(RIG_DEBUG_WARN, "%s: timedout without reading a character\n",
		    __FUNCTION__);
    return -RIG_ETIMEOUT;
  }

  rig_debug(RIG_DEBUG_TRACE,"RX %d characters\n",total_count);
  dump_hex(rxbuffer, total_count);
  return total_count;			/* return bytes count read */
}

