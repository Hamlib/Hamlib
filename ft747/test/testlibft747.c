/*  hamlib - (C) Frank Singleton 2000 (vk3fcs@ix.netcom.com)
 *
 * testlibft747.c - (C) Frank Singleton 2000 (vk3fcs@ix.netcom.com)
 * This program tests the libft747.so API for communicating
 * via serial interface to an FT-747GX using the "CAT" interface
 * box (FIF-232C) or similar.
 *
 *
 * $Id: testlibft747.c,v 1.10 2000-09-04 19:58:55 javabear Exp $  
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
#include <sys/ioctl.h>

#include "testlibft747.h"
#include "ft747.h"
#include "rig.h"

static unsigned char datain[350]; /* data read from rig */

struct ft747_update_data {
  unsigned char displayed_status;
  unsigned char displayed_freq[5];
  unsigned char current_band_data;
  unsigned char vfo_a_status;
  unsigned char vfo_a_freq_block[5];
};



/*
 * Decode routines for status update map
 */

static void decode_status_flags(unsigned char flags) {

  if((flags & SF_DLOCK) != 0 ) {
    printf("Dial Lock = SET \n");
  } else {
    printf("Dial Lock = CLEAR \n");
  }

  if((flags & SF_SPLIT) != 0 ) {
    printf("Split = SET \n");
  } else {
    printf("Split = CLEAR \n");
  }

  if((flags & SF_CLAR) != 0 ) {
    printf("Clar = SET \n");
  } else {
    printf("Clar = CLEAR \n");
  }

  if((flags & SF_VFOAB) != 0 ) {
    printf("VFO = B \n");
  } else {
    printf("VFO = A \n");
  }

  if((flags & SF_VFOMR) != 0 ) {
    printf("VFO/MR = MR \n");
  } else {
    printf("VFO/MR = VFO \n");
  }

  if((flags & SF_RXTX) != 0 ) {
    printf("RX/TX = TX \n");
  } else {
    printf("RX/TX = RX \n");
  }

  if((flags & SF_PRI) != 0 ) {
    printf("Priority Monitoring = ON \n");
  } else {
    printf("Priority Monitoring = OFF \n");
  }


}


/*
 * Decode routines for status update map
 */

static void decode_mode_bit_map(unsigned char mbm) {
  unsigned char mask = 0x1f;
  unsigned char mode = mbm & mask; /* grab operating mode */

  printf("mbm = %x, mode = %x \n",mbm, mode);

  switch(mode) {
  case MODE_FM:
    printf("Current Mode = FM \n");
    break;
  case MODE_AM:
    printf("Current Mode = AM \n");
    break;
  case MODE_CW:
    printf("Current Mode = CW \n");
    printf("MODE_CW = %i \n", MODE_CW);
    break;
  case MODE_USB:
    printf("Current Mode = USB \n");
    break;
  case MODE_LSB:
    printf("Current Mode = LSB \n");
    break;
  default:
    printf("Current mode = XXXXX \n");
    break;
  }

  if((mbm & MODE_NAR) != 0 ) {
    printf("Narrow = SET \n");
  } else {
    printf("Narrow = CLEAR \n");
  }
  
}


/*
 * Decode routines for status update map
 */

static void decode_band_data(unsigned char data) {
  unsigned char mask = 0x0f;
  unsigned char bd = data & mask; /* grab band data */

  printf("Band data is %f - %f \n", band_data[bd], band_data[bd+1]);
  

}



/*
 * Simple test to see if we are talking to the RIG.
 */

static int test(fd) {
  struct ft747_update_data *header;

  cmd_set_split_yes(fd);
  sleep(1);
  cmd_set_split_no(fd);
  sleep(1);
  cmd_set_dlock_on(fd);
  sleep(1);
  cmd_set_dlock_off(fd);
  sleep(1);
  cmd_set_select_vfo_a(fd);
  sleep(1);
  cmd_set_up500k(fd);
  sleep(1);
  cmd_set_down500k(fd);
  sleep(1);
/*    cmd_memory_to_vfo(fd,1); */
  sleep(1);
/*    cmd_vfo_to_memory(fd,2); */
  sleep(1);
  cmd_set_mode(fd,3);		/* cw */
  cmd_set_ptt_on(fd);		/* stand back .. */
  sleep(1);
  cmd_set_ptt_off(fd);		/* phew.. */
  sleep(1);

  cmd_set_pacing(fd,0);		/* set pacing */

  cmd_get_update_store(fd,datain); /* read (sleep)  and store in datain */

  header = (struct ft747_update_data *) &datain;	/* overlay header onto datain */

  printf("Frequency = %x.%.2x%.2x%.2x \n", header->displayed_freq[1],
	 header->displayed_freq[2],
	 header->displayed_freq[3],
	 header->displayed_freq[4]
	 );

  decode_status_flags(datain[0]); /* decode flags */
  printf("Displayed Memory = %i \n", datain[23]);
  decode_mode_bit_map(datain[24]); /* decode mode bit map */
  decode_band_data(datain[6]); /* decode current band data */


  return 0;
}


/*
 * Main program starts here..
 */


int main(void) {

  struct rig_caps *rc;	/* points to rig caps */

  int fd;

  rc = rig_get_caps();		/* find capabilities */

  printf("rig_name = %s \n", rc->rig_name);
  strcpy(rc->serial_port_name,SERIAL_PORT); /* put wanted serial port in caps */
  
/*    fd = rig_open(SERIAL_PORT); */
/*    printf("port opened ok \n"); */

  fd = rig_open(rc);
  printf("port %s opened ok \n", rc->serial_port_name);
    
  test(fd);
  printf("testing communication result ok \n");

  rig_close(fd);
  printf("port closed ok \n");

  return 0;
}

