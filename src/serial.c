/*
 * hamlib - (C) Frank Singleton 2000 (vk3fcs@ix.netcom.com)
 *
 * serial.c - (C) Frank Singleton 2000 (vk3fcs@ix.netcom.com), 
 * 				  Stephane Fillod 2000
 * Provides useful routines for read/write serial data for communicating
 * via serial interface.
 *
 * $Id: serial.c,v 1.5 2001-01-28 22:18:09 f4cfe Exp $  
 *
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 * 
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 * 
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.
 * 
 */

#include <stdlib.h>
#include <stdio.h>   /* Standard input/output definitions */
#include <string.h>  /* String function definitions */
#include <unistd.h>  /* UNIX standard function definitions */
#include <fcntl.h>   /* File control definitions */
#include <errno.h>   /* Error number definitions */
#include <termios.h> /* POSIX terminal control definitions */
#include <sys/time.h>
#include <sys/types.h>
#include <unistd.h>
#include <sys/ioctl.h>

#include <hamlib/rig.h>
#include "serial.h"
#include "misc.h"


/*
 * Open serial port using rig.state data
 *
 *
 * input: ptr to rig.state structure, with serial port
 *        populated. (eg: "/dev/ttyS1").
 * 
 * returns:
 *
 * 0 if OK or negative value on error.
 *
 */

int serial_open(struct rig_state *rs) {

  int fd;				/* File descriptor for the port */
  struct termios options;
  speed_t speed;			/* serial comm speed */

  if (!rs)
		  return -RIG_EINVAL;

  /*
   * Open in Non-blocking mode. Watch for EAGAIN errors!
   */
  fd = open(rs->rig_path, O_RDWR | O_NOCTTY | O_NDELAY);

  if (fd == -1) {
    
    /* Could not open the port. */
    
    rig_debug(RIG_DEBUG_ERR, "serial_open: Unable to open %s - %s\n", 
						rs->rig_path, strerror(errno));
    return -RIG_EIO;
  }
 
  /*
   * Get the current options for the port...
   */
  
  tcgetattr(fd, &options);
  
  /*
   * Set the baud rates to requested values
   */

  switch(rs->serial_rate) {
  case 300:
	speed = B300;		/* yikes... */
    break;
  case 1200:
	speed = B1200;
    break;
  case 2400:
	speed = B2400;
    break;
  case 4800:
	speed = B4800;
    break;
  case 9600:
	speed = B9600;
    break;
  case 19200:
	speed = B19200;
    break;
  case 38400:
	speed = B38400;
    break;
  case 57600:
	speed = B57600;		/* cool.. */
    break;
  default:
    rig_debug(RIG_DEBUG_ERR, "open_serial: unsupported rate specified: %d\n", 
					rs->serial_rate);
	close(fd);
    return -RIG_ECONF;
  }
  cfsetispeed(&options, speed);
  cfsetospeed(&options, speed);

  /*
   * Enable the receiver and set local mode...
   */
  
  options.c_cflag |= (CLOCAL | CREAD);

/*
 * Set data to requested values.
 *
 */

  switch(rs->serial_data_bits) {
  case 7:
    options.c_cflag &= ~CSIZE;
    options.c_cflag |= CS7;
    break;
  case 8:
    options.c_cflag &= ~CSIZE;
    options.c_cflag |= CS8;
    break;
  default:
    rig_debug(RIG_DEBUG_ERR,"open_serial: unsupported serial_data_bits specified: %d\n",
					rs->serial_data_bits);
	close(fd);
    return -RIG_ECONF;
    break;
  }

/*
 * Set stop bits to requested values.
 *
 */  

  switch(rs->serial_stop_bits) {
  case 1:
    options.c_cflag &= ~CSTOPB;
    break;
  case 2:
    options.c_cflag |= CSTOPB;
    break;

  default:
    rig_debug(RIG_DEBUG_ERR, "open_serial: unsupported serial_stop_bits specified: %d\n",
					rs->serial_stop_bits);
	close(fd);
    return -RIG_ECONF;
    break;
  }

/*
 * Set parity to requested values.
 *
 */  

  switch(rs->serial_parity) {
  case RIG_PARITY_NONE:
    options.c_cflag &= ~PARENB;
    break;
  case RIG_PARITY_EVEN:
    options.c_cflag |= PARENB;
    options.c_cflag &= ~PARODD;
    break;
  case RIG_PARITY_ODD:
    options.c_cflag |= PARENB;
    options.c_cflag |= PARODD;
    break;
  default:
    rig_debug(RIG_DEBUG_ERR, "open_serial: unsupported serial_parity specified: %d\n",
					rs->serial_parity);
	close(fd);
    return -RIG_ECONF;
    break;
  }


/*
 * Set flow control to requested mode
 *
 */  

  switch(rs->serial_handshake) {
  case RIG_HANDSHAKE_NONE:
    options.c_cflag &= ~CRTSCTS;
    options.c_iflag &= ~IXON;
    break;
  case RIG_HANDSHAKE_XONXOFF:
    options.c_cflag &= ~CRTSCTS;
    options.c_iflag |= IXON;		/* Enable Xon/Xoff software handshaking */
    break;
  case RIG_HANDSHAKE_HARDWARE:
    options.c_cflag |= CRTSCTS;		/* Enable Hardware handshaking */
    options.c_iflag &= ~IXON;
    break;
  default:
    rig_debug(RIG_DEBUG_ERR, "open_serial: unsupported flow_control specified: %d\n",
					rs->serial_handshake);
	close(fd);
    return -RIG_ECONF;
    break;
  }

  /*
   * Choose raw input, no preprocessing please ..
   */

  options.c_lflag &= ~(ICANON | ECHO | ECHOE | ISIG);

  /*
   * Choose raw output, no preprocessing please ..
   */

  options.c_oflag &= ~OPOST;

  /*
   * Flush serial port
   */

  tcflush(fd, TCIFLUSH);

  /*
   * Finally, set the new options for the port...
   */
  
  if (tcsetattr(fd, TCSANOW, &options) == -1) {
		rig_debug(RIG_DEBUG_ERR, "open_serial: tcsetattr failed: %s\n", 
					strerror(errno));
		close(fd);
		return -RIG_ECONF;		/* arg, so close! */
  }

  rs->fd = fd;

  return RIG_OK;
}




/*
 * Read "num" bytes from "fd" and put results into
 * an array of unsigned char pointed to by "rxbuffer"
 * 
 * Sleeps read_delay milliseconds until number of bytes to read
 * is the amount requested.
 *
 * It then reads "num" bytes into rxbuffer.
 *
 * input:
 * fd - file descriptor
 * rxbuffer - ptr to rx buffer
 * num - number of bytes to read
 * read_delay - number of msec between read attempts
 *
 * returns:
 *
 * n - numbers of bytes read
 *
 */


int read_sleep(int fd, unsigned char *rxbuffer, int num , int read_delay) {  
  int bytes = 0;
  int n = 0;

  rig_debug(RIG_DEBUG_ERR,"serial:read_sleep called with num = %i \n",num);

  while(1) {
    ioctl(fd, FIONREAD, &bytes); /* get bytes in buffer */
    rig_debug(RIG_DEBUG_TRACE,"bytes  = %i\n", bytes);
    if (bytes == num)
      break;
    usleep(read_delay*1000);	/* sleep read_delay msecs */
  }

  /* this should not block now */
  
  n = read(fd,rxbuffer,num);	/* grab num bytes from rig */

  dump_hex(rxbuffer,num);
  return n;			/* return bytes read */
}

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

int write_block(int fd, const unsigned char *txbuffer, size_t count, int write_delay, int post_write_delay /* , struct timeval *post_write_date */ )
{
  int i;

#ifdef WANT_NON_ACTIVE_POST_WRITE_DELAY
  if (post_write_date->tv_sec != 0) {
		  signed int date_delay;	/* in us */
		  struct timeval tv;

		  /* FIXME in Y2038 ... */
		  gettimeofday(tv, NULL);
		  date_delay = post_write_delay*1000 - 
				  		((tv.tv_sec - post_write_date->tv_sec)*1000000 +
				  		 (tv.tv_usec - post_write_date->tv_usec));
		  if (date_delay > 0) {
				/*
				 * optional delay after last write 
				 */
				usleep(date_delay); 
		  }
		  post_write_date->tv_sec = 0;
  }
#endif

  if (write_delay > 0) {
  	for (i=0; i < count; i++) {
		if (write(fd, txbuffer+i, 1) < 0) {
			rig_debug(RIG_DEBUG_ERR,"write_block() failed - %s\n", 
							strerror(errno));
			return -RIG_EIO;
    	}
    	usleep(write_delay*1000);
  	}
  } else {
		  write(fd, txbuffer, count);
  }
  
  if (post_write_delay > 0) {
#ifdef WANT_NON_ACTIVE_POST_WRITE_DELAY
#define POST_WRITE_DELAY_TRSHLD 10

	if (post_write_delay > POST_WRITE_DELAY_TRSHLD)
		gettimeofday(post_write_date, NULL);
	else
#endif
    usleep(post_write_delay*1000); /* optional delay after last write */
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

int read_block(int fd, unsigned char *rxbuffer, size_t count, int timeout )
{  
  fd_set rfds;
  struct timeval tv, tv_timeout;
  int rd_count, total_count = 0;
  int retval;

  FD_ZERO(&rfds);
  FD_SET(fd, &rfds);

  /*
   * Wait up to timeout ms.
   */
  tv_timeout.tv_sec = timeout/1000;
  tv_timeout.tv_usec = (timeout%1000)*1000;

  while (count > 0) {
		tv = tv_timeout;	/* select may have updated it */

		retval = select(fd+1, &rfds, NULL, NULL, &tv);
		if (!retval) {
			rig_debug(RIG_DEBUG_ERR,"rig timeout after %d chars or select error - %s!\n",
							total_count, strerror(errno));
				return -RIG_ETIMEOUT;
		}

		/*
		 * grab bytes from the rig
		 * The file descriptor must have been set up non blocking.
		 */
  		rd_count = read(fd, rxbuffer+total_count, count);
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

int fread_block(FILE *stream, unsigned char *rxbuffer, size_t count, int timeout )
{  
  fd_set rfds;
  struct timeval tv, tv_timeout;
  int rd_count, total_count = 0;
  int retval;
  int fd;

  fd = fileno(stream);

  FD_ZERO(&rfds);
  FD_SET(fd, &rfds);

  /*
   * Wait up to timeout ms.
   */
  tv_timeout.tv_sec = timeout/1000;
  tv_timeout.tv_usec = (timeout%1000)*1000;


		/*
		 * grab bytes from the rig
		 * The file descriptor must have been set up non blocking.
		 */
  		rd_count = fread(rxbuffer, 1, count, stream);
		if (rd_count < 0) {
				rig_debug(RIG_DEBUG_ERR, "read_block: read failed - %s\n",
									strerror(errno));
				return -RIG_EIO;
		}
		total_count += rd_count;
		count -= rd_count;

  while (count > 0) {
		tv = tv_timeout;	/* select may have updated it */

		retval = select(fd+1, &rfds, NULL, NULL, &tv);
		if (!retval) {
			rig_debug(RIG_DEBUG_ERR,"rig timeout after %d chars or select error - %s!\n",
							total_count, strerror(errno));
				return -RIG_ETIMEOUT;
		}

		/*
		 * grab bytes from the rig
		 * The file descriptor must have been set up non blocking.
		 */
  		rd_count = fread(rxbuffer+total_count, 1, count, stream);
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
 * TODO: split ptt_set in ser_ptt_set and par_ptt_set
 * +ser_ptt_open/ser_ptt_close & par_ptt_open/par_ptt_close
 */
int ptt_set(int fd, ptt_type_t ptt_type, int pttx)
{
		unsigned char y;

		switch(ptt_type) {
		case RIG_PTT_SERIAL_RTS:
			y = TIOCM_RTS;
			return ioctl(fd, pttx ? TIOCMBIS : TIOCMBIC, &y);

		case RIG_PTT_SERIAL_DTR:
			return -RIG_ENIMPL;
			y = TIOCM_DTR;
			return ioctl(fd, pttx ? TIOCMBIS : TIOCMBIC, &y);

#ifdef LINUX	/* Have PPdev */
		case RIG_PTT_PARALLEL:
				{
					unsigned char reg;

					reg = !!pttx;
					return ioctl(fd, PPWDATA, &reg);
				}
#endif
		default:
				rig_debug(RIG_DEBUG_ERR,"Unsupported PTT type %d\n",ptt_type);
				return -RIG_EINVAL;
		}
		return 0;
}

