/*  hamlib - (C) Frank Singleton 2000 (vk3fcs@ix.netcom.com)
 *
 * testlibft847.c - (C) Frank Singleton 2000 (vk3fcs@ix.netcom.com)
 * This program tests the libft847.so API for communicating
 * via serial interface to an FT-847 using the "CAT" interface.
 *
 *
 * $Id: testlibft847.c,v 1.13 2000-09-04 03:44:26 javabear Exp $  
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

#include "testlibft847.h"
#include "ft847.h"
#include "rig.h"


static unsigned char datain[5]; /* data read from rig */

/*
 * Decode routine for TX status update map
 */

static void decode_tx_status_flags(unsigned char txflag) {

  printf("TX Status = %i \n", txflag);
  printf("TXSF_PTT_STATUS = %i \n",TXSF_PTT_STATUS);

  if((txflag & TXSF_PTT_STATUS) != 0 ) {
    printf("PTT = OFF (RX) \n");
  } else {
    printf("PTT = ON (TX) \n");
  }

  printf("PO/ALC Meter Data = %i \n", txflag & TXSF_POALC_METER_MASK);

}

/*
 * Decode routine for RX status update map
 */

static void decode_rx_status_flags(unsigned char rxflag) {

  if((rxflag & RXSF_DISC_CENTER) != 0 ) {
    printf("Discriminator = Off Center  \n");
  } else {
    printf("Discriminator = Centered \n");
  }

  if((rxflag & RXSF_SQUELCH_STATUS) != 0 ) {
    printf("Squelch = Squelch On (no signal)  \n");
  } else {
    printf("Squelch = Squelch Off (signal present)  \n");
  }

  if((rxflag & RXSF_CTCSS_DCS_CODE) != 0 ) {
    printf("CTCSS/DCS Code = Un-Matched  \n");
  } else {
    printf("CTCSS/DCS Code = Matched  \n");
  }

  printf("S-Meter Meter Data = %i \n", rxflag & RXSF_SMETER_MASK);

}


/*
 * Decode routine for Mode Status
 */

static void decode_mode(unsigned char mode) {

  switch(mode) { 
  case 0:
    printf("Current Mode = LSB \n");
    break;
  case 1:
    printf("Current Mode = USB \n");
    break;
  case 2:
    printf("Current Mode = CW \n");
    break;
  case 3:
    printf("Current Mode = CWR \n");
    break;
  case 4:
    printf("Current Mode = AM \n");
    break;
  case 8:
    printf("Current Mode = FM \n");
    break;
  case 82:
    printf("Current Mode = CW(N) \n");
    break;
  case 83:
    printf("Current Mode = CW(N)-R \n");
    break;
  case 84:
    printf("Current Mode = AM(N) \n");
    break;
  case 88:
    printf("Current Mode = FM(N) \n");
    break;
  default:
    printf("Current mode = XXXXX \n");
    break;
  }
  
}



/*
 * Simple test to see if we are talking to the RIG.
 */

static int test(fd) {
  unsigned char data1;
  unsigned char mode;
  int i;
  long int frq;			/* freq */

  cmd_set_cat_off(fd);		/* cat off */
  sleep(1);
  cmd_set_cat_on(fd);		/* cat on */
  sleep(1);
  cmd_set_sat_on(fd);		/* sat mode on */
  sleep(5);
  cmd_set_sat_off(fd);		/* sat mode off */
  sleep(3);
  cmd_set_opmode_main_vfo(fd,MODE_AM);
  sleep(1);
  data1 = cmd_get_rx_status(fd);
  printf("data1 = %i \n", data1);
  decode_rx_status_flags(data1);


/*    decode_rx_status_flags(data1); */
  sleep(1);

  for (i=0; i<4; i++) {
    data1 = cmd_get_rx_status(fd);
    decode_rx_status_flags(data1);
    sleep(1);
    frq = cmd_get_freq_mode_status_main_vfo(fd, &mode);
    printf("freq = %ld Hz and mode = %x \n",frq, mode );

    sleep(1);   
  }

  cmd_set_freq_main_vfo_hz(fd,439700000,MODE_FM);
  sleep(5);
  cmd_set_freq_main_vfo_hz(fd,123456780,MODE_CW);
  sleep(5);
  cmd_set_freq_main_vfo_hz(fd,770000,MODE_AM);
  sleep(5);

  cmd_set_freq_sat_rx_vfo_hz(fd,137125000,MODE_FM);
  sleep(5);
  cmd_set_freq_sat_rx_vfo_hz(fd,137125000,MODE_FM);
  sleep(5);
  cmd_set_freq_sat_tx_vfo_hz(fd,437875000,MODE_FMN);
  sleep(5);

  cmd_set_cat_off(fd);		/* cat off */
    
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

  fd = rig_open(SERIAL_PORT);
  printf("port %s opened ok \n",SERIAL_PORT);
  
  test(fd);
  printf("testing communication result ok \n");
  
  rig_close(fd);
  printf("port %s closed ok \n",SERIAL_PORT);
  
  return 0;
}



