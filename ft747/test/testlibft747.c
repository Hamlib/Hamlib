/*  hamlib - (C) Frank Singleton 2000 (vk3fcs@ix.netcom.com)
 *
 * testlibft747.c - (C) Frank Singleton 2000 (vk3fcs@ix.netcom.com)
 * This program tests the libft747.so API for communicating
 * via serial interface to an FT-747GX using the "CAT" interface
 * box (FIF-232C) or similar.
 *
 *
 * $Id: testlibft747.c,v 1.1 2000-07-18 20:55:01 frank Exp $  
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
  case 1:
    printf("Current Mode = FM \n");
    break;
  case 2:
    printf("Current Mode = AM \n");
    break;
  case 4:
    printf("Current Mode = CW \n");
    break;
  case 8:
    printf("Current Mode = USB \n");
    break;
  case 16:
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
 * Do a hex dump of the unsigned car array.
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
  int n,i;
  int bytes = 0;
  struct ft747_update_data *header;

  cmd_split_yes(fd);
  sleep(1);
  cmd_split_no(fd);
  sleep(1);
  cmd_dlock_on(fd);
  sleep(1);
/*    cmd_dlock_off(fd); */
  sleep(1);
  cmd_select_vfo_a(fd);
  sleep(1);
  cmd_up500k(fd);
  sleep(1);
  cmd_down500k(fd);
  sleep(1);
/*    cmd_memory_to_vfo(fd,1); */
  sleep(1);
/*    cmd_vfo_to_memory(fd,2); */
  sleep(1);
  cmd_mode_set(fd,3);		/* cw */
  cmd_ptt_on(fd);		/* stand back .. */
  sleep(1);
  cmd_ptt_off(fd);		/* phew.. */
  sleep(1);

  cmd_pacing_set(fd,0);		/* set pacing */
  cmd_update(fd);		/* request data from rig */
/*    sleep(1); */			/* be nice .. */
 /*   n = read(fd,datain,345);	 */	/* grab 345 bytes from rig */

/*    printf("n = %i, frequency = %.6s \n", n, datain[1]); */


  /*
   * Sleep regularly until the buffer contains all 345 bytes
   * This should handle most values used for pacing.
   */

  bytes = 0;
  while(bytes < 345) {
    ioctl(fd, FIONREAD, &bytes); /* get bytes in buffer */
    printf("bytes  = %i\n", bytes);
    sleep(1);
    
  }

  /* this should not block now */
  
  n = read(fd,datain,345);		/* grab 345 bytes from rig */

  for(i=0; i<n; i++) {
    printf("i = %i ,datain[i] = %x \n", i, datain[i]);
  }

  header = (struct ft747_update_data *) &datain;	/* overlay header onto datain */

  printf("Frequency = %x.%x%x%x \n", header->displayed_freq[1],
	 header->displayed_freq[2],
	 header->displayed_freq[3],
	 header->displayed_freq[4]
	 );

  decode_status_flags(datain[0]); /* decode flags */
  printf("Displayed Memory = %i \n", datain[23]);
  decode_mode_bit_map(datain[24]); /* decode mode bit map */
  decode_band_data(datain[6]); /* decode current band data */

  dump_hex(datain, 345, 16);	/* do a hex dump */

  return 0;
}


/*
 * Main program starts here..
 */


int main(void) {


  int fd;

  fd = open_port(SERIAL_PORT);
  printf("port opened ok \n");

  test(fd);
  printf("testing communication result ok \n");

  close(fd);
  printf("port closed ok \n");

  return 0;
}

