/*  hamlib - (C) Frank Singleton 2000 (vk3fcs@ix.netcom.com)
 *
 * testlibft847.c - (C) Frank Singleton 2000 (vk3fcs@ix.netcom.com)
 * This program tests the libft847.so API for communicating
 * via serial interface to an FT-847 using the "CAT" interface.
 *
 *
 * $Id: testlibft847.c,v 1.3 2000-07-26 00:09:29 javabear Exp $  
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


static unsigned char datain[5]; /* data read from rig */


/*
 * Decode routine for rx status update map
 */

static void decode_rx_status_flags(unsigned char rxflag) {

  if((rxflag & RXSF_PTT_STATUS) != 0 ) {
    printf("PTT = ON (TX) \n");
  } else {
    printf("PTT = OFF (RX) \n");
  }

  printf("PO/ALC Meter Data = %i \n", rxflag & RXSF_POALC_METER_MASK);

}

/*
 * Decode routine for tx status update map
 */

static void decode_tx_status_flags(unsigned char txflag) {

  if((txflag & TXSF_DISC_CENTER) != 0 ) {
    printf("Discriminator = Off Center  \n");
  } else {
    printf("Discriminator = Centered \n");
  }

  if((txflag & TXSF_SQUELCH_STATUS) != 0 ) {
    printf("Squelch = Squelch On (no signal)  \n");
  } else {
    printf("Squelch = Squelch Off (signal present)  \n");
  }

  if((txflag & TXSF_CTCSS_DCS_CODE) != 0 ) {
    printf("CTCSS/DCS Code = Un-Matched  \n");
  } else {
    printf("CTCSS/DCS Code = Matched  \n");
  }

  printf("S-Meter Meter Data = %i \n", txflag & TXSF_SMETER_MASK);

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
 * Do a hex dump of the unsigned char array.
 */

static void dump_hex(unsigned char *ptr, int size, int length) {
  int i;
  
  printf("Memory Dump \n\n");
  for(i=0; i<size; i++) {
    printf(" 0x%2.2x", *(ptr+i));
    if (i % length == 15) {
      printf("\n");
    }
  }
  printf("\n\n");
  
} 

/*
 * Simple test to see if we are talking to the RIG.
 */

static int test(fd) {
  unsigned char data1;

  cmd_cat_off(fd);		/* cat off */
  sleep(1);
  cmd_cat_on(fd);		/* cat on */
  sleep(1);
  cmd_sat_on(fd);		/* sat mode on */
  sleep(1);
  cmd_sat_off(fd);		/* sat mode off */
  sleep(1);
  cmd_cat_off(fd);		/* cat off */
    
  return 0;
}


/*
 * Main program starts here..
 */


int main(void) {


  int fd;
  
  fd = rig_open(SERIAL_PORT);
  printf("port %s opened ok \n",SERIAL_PORT);
  
  test(fd);
  printf("testing communication result ok \n");
  
  rig_close(fd);
  printf("port %s closed ok \n",SERIAL_PORT);
  
  return 0;
}



