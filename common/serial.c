/*
 * hamlib - (C) Frank Singleton 2000 (vk3fcs@ix.netcom.com)
 *
 * serial.c - (C) Frank Singleton 2000 (vk3fcs@ix.netcom.com)
 * Provides useful routines for read/write serial data for communicating
 * via serial interface .
 *
 * $Id: serial.c,v 1.12 2000-09-16 23:56:35 javabear Exp $  
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

#include <rig.h>
#include "serial.h"


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
		  return -1;

  /*
   * Open in Non-blocking mode. Watch for EAGAIN errors!
   */
  fd = open(rs->rig_path, O_RDWR | O_NOCTTY | O_NDELAY);

  if (fd == -1) {
    
    /* Could not open the port. */
    
    fprintf(stderr, "serial_open: Unable to open %s - %s\n", 
						rs->rig_path, strerror(errno));
    return -2;
  }
 
  /*
   * Get the current options for the port...
   */
  
  tcgetattr(fd, &options);
  
  /*
   * Set the baud rates to requested values
   */

  switch(rs->serial_rate) {
  case 1200:
	speed = B1200;
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
	speed = B57600;
    break;
  default:
    fprintf(stderr, "open_serial: unsupported rate specified: %d\n", 
					rs->serial_rate);
	close(fd);
    return -1;
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
    printf("open_serial: unsupported serial_data_bits specified: %d\n",
					rs->serial_data_bits);
	close(fd);
    return -1;
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
    fprintf(stderr, "open_serial: unsupported serial_stop_bits specified: %d\n",
					rs->serial_stop_bits);
	close(fd);
    return -1;
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
    fprintf(stderr, "open_serial: unsupported serial_parity specified: %d\n",
					rs->serial_parity);
	close(fd);
    return -1;
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
    fprintf(stderr, "open_serial: unsupported flow_control specified: %d\n",
					rs->serial_handshake);
	close(fd);
    return -1;
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
		fprintf(stderr, "open_serial: tcsetattr failed: %s\n", 
					strerror(errno));
		close(fd);
		return -1;		/* arg, so close! */
  }

  rs->fd = fd;

  return 0;
}







/*
 * 'close_port()' - Close serial port 
 *
 * fd - file descriptor for open port
 *
 *
 * Returns success (0) or -1 on error.
 */

int close_port(int fd) {

  if (close(fd) <0 ) {
    printf("close_port: Unable to close port using fd %i - ",fd);
    return -1;			/* oops */
  }
  return 0;			/* ok */
}



/*
 * Read "num" bytes from "fd" and put results into
 * an array of unsigned char pointed to by "rxbuffer"
 * 
 * Sleeps every second until number of bytes to read
 * is the amount requested.
 *
 * It the reads "num" bytes into rxbuffer.
 *
 */


int read_sleep(int fd, unsigned char *rxbuffer, int num ) {  
  int bytes = 0;
  int n = 0;

  printf("serial:read_sleep called with num = %i \n",num);

  while(1) {
    ioctl(fd, FIONREAD, &bytes); /* get bytes in buffer */
    printf("bytes  = %i\n", bytes);
    if (bytes == num)
      break;
    sleep(1);			/* wait 1 second and try again */
  }

  /* this should not block now */
  
  n = read(fd,rxbuffer,num);	/* grab num bytes from rig */

  dump_hex(rxbuffer,num,16);
  return n;			/* return bytes read */
}

/*
 * Write a count character block to file descriptor,
 * with a pause between each character if write_delay is > 0
 *
 * The delay is for Yaesu type rigs..require 5 character
 * sequence to be sent with 50-200msec between each char.
 *
 * input:
 *
 * fd - file descriptor to write to
 * txbuffer - pointer to a command sequence array
 * count - count of byte to send from the txbuffer
 * write_delay - write delay in ms between 2 chars
 *
 * returns:
 *
 *  0 = OK
 * -1 = NOT OK
 *
 */

int write_block2(int fd, const unsigned char *txbuffer, size_t count, int write_delay)
{
  int i;

  if (write_delay > 0) {
  	for (i=0; i < count; i++) {
		if (write(fd, txbuffer+i, 1) < 0) {
			fprintf(stderr,"write_block() failed - %s\n", strerror(errno));
			return -1;
    	}
    	usleep(write_delay*1000);
  	}
  } else {
		  write(fd, txbuffer, count);
  }
  dump_hex(txbuffer,count,16);
  
  return 0;
}

/*
 * Read "num" bytes from "fd" and put results into
 * an array of unsigned char pointed to by "rxbuffer"
 * 
 * Blocks on read until timeout hits.
 *
 * It then reads "num" bytes into rxbuffer.
 *
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
			fprintf(stderr,"rig timeout after %d chars or select error - %s!\n",
							total_count, strerror(errno));
				return -1;
		}

		/*
		 * grab bytes from the rig
		 * The file descriptor must have been set up non blocking.
		 */
  		rd_count = read(fd, rxbuffer+total_count, count);
		if (rd_count < 0) {
				fprintf(stderr, "read_block: read failed - %s\n",
									strerror(errno));
				return -1;
		}
		total_count += rd_count;
		count -= rd_count;
  }

  dump_hex(rxbuffer, total_count, 16);
  return total_count;			/* return bytes count read */
}



/*
 * Do a hex dump of the unsigned char array.
 */

void dump_hex(const unsigned char *ptr, int size, int width) {
  int i =0;

  printf("\n");
  printf("serial: Memory Dump: size = %i, width = %i .\n",size,width);

  for(i=0; i<size; i++) {
    if (i % width == 0) {
      printf("\n");
      printf("0x%.4x  ",i);
    }
    printf(" 0x%.2x", *(ptr+i));

  }
  printf("\n");

  return;
} 



/* todo: put in other file -- FS */


/*
 * Convert char to packed decimal
 * eg: 33 (0x21) => 0x33
 *
 */

char calc_packed_from_char(unsigned char dec ) {
 
  char d1,d2,pkd;

  d1 = dec/10;
  d2 = dec - (d1 * 10);
  pkd = (d1*16)+d2;
 
  return pkd;
}


/*
 * Convert packed decimal to decimal
 * eg: 0x33 (51) => 33 decimal
 *
 */

char calc_char_from_packed(unsigned char pkd ) {
 
  char d1,d2,dec;

  d1 = pkd/16;
  d2 = pkd - (d1 * 16);
  dec = (d1*10)+d2;

  return dec;
}











