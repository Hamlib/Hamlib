/*
 * hamlib - (C) Frank Singleton 2000 (vk3fcs@ix.netcom.com)
 *
 * ft747.c - (C) Frank Singleton 2000 (vk3fcs@ix.netcom.com)
 * This shared library provides an API for communicating
 * via serial interface to an FT-747GX using the "CAT" interface
 * box (FIF-232C) or similar
 *
 *
 * $Id: ft747.c,v 1.11 2000-09-16 23:16:26 javabear Exp $  
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

#include "rig.h"
#include "riglist.h"
#include "serial.h"
#include "ft747.h"

/* prototypes */
int ft747_set_freq_main_vfo_hz(RIG *rig, freq_t freq, rig_mode_t mode);


/* 
 * Receiver caps 
 */

#define FT747_ALL_RX_MODES (RIG_MODE_AM| RIG_MODE_CW| RIG_MODE_USB| RIG_MODE_LSB| RIG_MODE_NAM| RIG_MODE_NCW)

/* 
 * TX caps
 */ 

#define FT747_OTHER_TX_MODES (RIG_MODE_CW| RIG_MODE_USB| RIG_MODE_LSB )	/* 100 W class */
#define FT747_AM_TX_MODES (RIG_MODE_AM ) /* set 25W max */

/*
 * ft747 rigs capabilities.
 * Also this struct is READONLY!
 */

const struct rig_caps ft747_caps = {
  RIG_MODEL_FT747, "FT-747GX", "Yaesu", "0.1", RIG_STATUS_ALPHA,
  RIG_TYPE_MOBILE, 4800, 4800, 8, 2, RIG_PARITY_NONE, 
  RIG_HANDSHAKE_NONE, 50, 2000, 0,
  { {100000,29999900,FT747_ALL_RX_MODES,-1,-1}, {0,0,0,0,0}, }, /* rx range */

  { {1500000,1999900,FT747_OTHER_TX_MODES,5000,100000},	/* 100W class */ 
    {1500000,1999900,FT747_AM_TX_MODES,2000,25000},	/* 25W class */

    {3500000,3999900,FT747_OTHER_TX_MODES,5000,100000},
    {3500000,3999900,FT747_AM_TX_MODES,2000,25000},

    {7000000,7499900,FT747_OTHER_TX_MODES,5000,100000},
    {7000000,7499900,FT747_AM_TX_MODES,2000,25000},

    {10000000,10499900,FT747_OTHER_TX_MODES,5000,100000},
    {10000000,10499900,FT747_AM_TX_MODES,2000,25000},

    {14000000,14499900,FT747_OTHER_TX_MODES,5000,100000},
    {14000000,14499900,FT747_AM_TX_MODES,2000,25000},

    {18000000,18499900,FT747_OTHER_TX_MODES,5000,100000},
    {18000000,18499900,FT747_AM_TX_MODES,2000,25000},

    {21000000,21499900,FT747_OTHER_TX_MODES,5000,100000},
    {21000000,21499900,FT747_AM_TX_MODES,2000,25000},

    {24500000,24999900,FT747_OTHER_TX_MODES,5000,100000},
    {24500000,24999900,FT747_AM_TX_MODES,2000,25000},

    {28000000,29999900,FT747_OTHER_TX_MODES,5000,100000},
    {28000000,29999900,FT747_AM_TX_MODES,2000,25000},

    {0,0,0,0,0} },
  
  ft747_init, 
  ft747_cleanup, 
  NULL,				/* port opened */
  NULL,				/* port closed */
  NULL,				/* probe not supported yet */

  ft747_set_freq_main_vfo_hz	/* example implementation */

};

/*  
 * Init some private data
 */

static const struct ft747_priv_data ft747_priv = { 747 }; /* dummy atm */


/*
 * Function definitions below
 */

/*
 * setup *priv 
 * serial port is already open (rig->state->fd)
 */

int ft747_init(RIG *rig) {
  struct ft747_priv_data *p;
  
  if (!rig)
    return -1;
  
  p = (struct ft747_priv_data*)malloc(sizeof(struct ft747_priv_data));
  if (!p) {
				/* whoops! memory shortage! */
    return -2;
  }
  
  /* init the priv_data from static struct 
   *          + override with rig-specific preferences
   */
  
  *p = ft747_priv;
  
  rig->state.priv = (void*)p;
  
  return 0;
}


/*
 * ft747_cleanup routine
 * the serial port is closed by the frontend
 */
int ft747_cleanup(RIG *rig) {
  if (!rig)
    return -1;
  
  if (rig->state.priv)
    free(rig->state.priv);
  rig->state.priv = NULL;
  
  return 0;
}

		/* 
		 * should check return code and that write wrote cmd_len chars! 
		 */
/*  		write_block2(rig_s->fd, buf, frm_len, rig_s->write_delay); */


/*
 * Example of wrapping backend function inside frontend API
 * 	 
 */

int ft747_set_freq_main_vfo_hz(RIG *rig, freq_t freq, rig_mode_t mode) {
  struct ft747_priv_data *p;
  struct rig_state *rig_s;
  unsigned char buf[16];
  int frm_len = 5;		/* fix later */

  /*  examlpe opcode */
  static unsigned char data[] = { 0x00, 0x00, 0x00, 0x01, 0x04 }; /* dial lock = on */

  
  if (!rig)
    return -1;
  
  p = (struct ft747_priv_data*)rig->state.priv;
  rig_s = &rig->state;
  
  
  /* 
   * should check return code and that write wrote cmd_len chars! 
   */

  write_block2(rig_s->fd, data, frm_len, rig_s->write_delay);
  
  /*
   * wait for ACK ... etc.. 
   */
  read_block(rig_s->fd, buf, frm_len, rig_s->timeout);
  
  /*
   * TODO: check address, Command number and ack!
   */
  
  /* then set the mode ... etc. */
  
  return 0;
}






/*
 * Implement OPCODES
 */


#if 0

void ft747_cmd_set_split_yes(int fd) {
  static unsigned char data[] = { 0x00, 0x00, 0x00, 0x01, 0x01 }; /* split = on */
  write_block2(fd,data);
}

void ft747_cmd_set_split_no(int fd) {
  static unsigned char data[] = { 0x00, 0x00, 0x00, 0x00, 0x01 }; /* split = off */
  write_block2(fd,data);
}

void ft747_cmd_set_recall_memory(int fd, int mem) {
  static unsigned char data[] = { 0x00, 0x00, 0x00, 0x00, 0x02 }; /* recall memory*/
  
  data[3] = mem;
  write_block2(fd,data);
}

void ft747_cmd_set_vfo_to_memory(int fd, int mem) {
  static unsigned char data[] = { 0x00, 0x00, 0x00, 0x00, 0x03 }; /* vfo to  memory*/
  
  data[3] = mem;
  write_block2(fd,data);
}

void ft747_cmd_set_dlock_off(int fd) {
  static unsigned char data[] = { 0x00, 0x00, 0x00, 0x00, 0x04 }; /* dial lock = off */
  write_block2(fd,data);
}

void ft747_cmd_set_dlock_on(int fd) {
  static unsigned char data[] = { 0x00, 0x00, 0x00, 0x01, 0x04 }; /* dial lock = on */
  write_block2(fd,data);

}


void ft747_cmd_set_select_vfo_a(int fd) {
  static unsigned char data[] = { 0x00, 0x00, 0x00, 0x00, 0x05 }; /* select vfo A */
  write_block2(fd,data);
}

void ft747_cmd_set_select_vfo_b(int fd) {
  static unsigned char data[] = { 0x00, 0x00, 0x00, 0x01, 0x05 }; /* select vfo B */
  write_block2(fd,data);
}

void ft747_cmd_set_memory_to_vfo(int fd, int mem) {
  static unsigned char data[] = { 0x00, 0x00, 0x00, 0x00, 0x06 }; /* memory to vfo*/
  
  data[3] = mem;
  write_block2(fd,data);
}

void ft747_cmd_set_up500k(int fd) {
  static unsigned char data[] = { 0x00, 0x00, 0x00, 0x00, 0x07 }; /* up 500 khz */
  write_block2(fd,data);
}

void ft747_cmd_set_down500k(int fd) {
  static unsigned char data[] = { 0x00, 0x00, 0x00, 0x00, 0x08 }; /* down 500 khz */
  write_block2(fd,data);
}

void ft747_cmd_set_clarify_off(int fd) {
  static unsigned char data[] = { 0x00, 0x00, 0x00, 0x00, 0x09 }; /* clarify off */
  write_block2(fd,data);
  printf("ft747_cmd_clarify_off complete \n");
}

void ft747_cmd_set_clarify_on(int fd) {
  static unsigned char data[] = { 0x00, 0x00, 0x00, 0x01, 0x09 }; /* clarify on */
  write_block2(fd,data);
}


void ft747_cmd_set_freq(int fd, unsigned int freq) {
  printf("ft747_cmd_freq_set not implemented yet \n");
}


void ft747_cmd_set_mode(int fd, int mode) {
  static unsigned char data[] = { 0x00, 0x00, 0x00, 0x00, 0x0c }; /* mode set */

  data[3] = mode;
  write_block2(fd,data);

}

void ft747_cmd_set_pacing(int fd, int delay) {
  static unsigned char data[] = { 0x00, 0x00, 0x00, 0x00, 0x0e }; /* pacing set */

  data[3] = delay;
  write_block2(fd,data);

}

void ft747_cmd_set_ptt_off(int fd) {
  static unsigned char data[] = { 0x00, 0x00, 0x00, 0x00, 0x0f }; /* ptt off */
  write_block2(fd,data);

}

void ft747_cmd_set_ptt_on(int fd) {

#ifdef TX_ENABLED
  static unsigned char data[] = { 0x00, 0x00, 0x00, 0x01, 0x0f }; /* ptt on */
  write_block2(fd,data);
  printf("ft747_cmd_ptt_on complete \n");
#elsif
  printf("ft747_cmd_ptt_on disabled \n");
#endif

}

/*
 * Read data from rig and store in buffer provided
 * by the user.
 */

void ft747_cmd_get_update_store(int fd, unsigned char *buffer) {
  int n;			/* counter */

  static unsigned char data[] = { 0x00, 0x00, 0x00, 0x00, 0x10 }; /* request update from rig */

  write_block2(fd,data); 
  n = read_sleep(fd,buffer,345);	/* wait and read for 345 bytes to be read */

  return;
}


#endif





