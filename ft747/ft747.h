/*
 * hamlib - (C) Frank Singleton 2000 (vk3fcs@ix.netcom.com)
 *
 * ft747.h - (C) Frank Singleton 2000 (vk3fcs@ix.netcom.com)
 * This shared library provides an API for communicating
 * via serial interface to an FT-747GX using the "CAT" interface
 * box (FIF-232C) or similar
 *
 *
 *    $Id: ft747.h,v 1.3 2000-07-28 03:05:18 javabear Exp $  
 */


/*
 * Allow TX commands to be disabled
 *
 */

#undef TX_ENABLED


/* MODES - when setting modes via cmd_mode_set() */

const int MODE_SET_LSB  =  0x00;
const int MODE_SET_USB  =  0x01;
const int MODE_SET_CWW  =  0x02;
const int MODE_SET_CWN  =  0x03;
const int MODE_SET_AMW  =  0x04;
const int MODE_SET_AMN  =  0x05;
const int MODE_SET_FMW  =  0x06;
const int MODE_SET_FMN  =  0x07;


/*
 * Status Flags
 */

const int SF_DLOCK  = (1<<0);
const int SF_SPLIT  = (1<<1);
const int SF_CLAR   = (1<<2);
const int SF_VFOAB  = (1<<3);
const int SF_VFOMR  = (1<<4);
const int SF_RXTX   = (1<<5);
const int SF_RESV   = (1<<6);
const int SF_PRI    = (1<<7);


/*
 * Mode Bitmap. Bits 5 and 6 unused
 * When reading modes
 */

const int MODE_FM   = (1<<0);
const int MODE_AM   = (1<<1);
const int MODE_CW   = (1<<2);
const int MODE_USB  = (1<<3);
const int MODE_LSB  = (1<<4);
const int MODE_NAR  = (1<<7);

/*
 * Map band data value to band.
 *
 * Band "n" is from band_data[n] to band_data[n+1]
 */

const float band_data[11] = { 0.0, 0.1, 2.5, 4.0, 7.5, 10.5, 14.5, 18.5, 21.5, 25.0, 30.0 };


/*
 * Visible functions in shared lib.
 *
 */
int rig_open(char *serial_port); /* return fd or -1 on error */
int rig_close(int fd);		 /* close port using fd */

/*
 * set commands
 */


void cmd_set_split_yes(int fd);
void cmd_set_split_no(int fd);
void cmd_set_recall_memory(int fd, int mem);
void cmd_set_vfo_to_memory(int fd, int mem);
void cmd_set_dlock_off(int fd);
void cmd_set_dlock_on(int fd);
void cmd_set_select_vfo_a(int fd);
void cmd_set_select_vfo_b(int fd);
void cmd_set_memory_to_vfo(int fd, int mem);
void cmd_set_up500k(int fd);
void cmd_set_down500k(int fd);
void cmd_set_clarify_off(int fd);
void cmd_set_clarify_on(int fd);
void cmd_set_freq(int fd, unsigned int freq);
void cmd_set_mode(int fd, int mode);
void cmd_set_pacing(int fd, int delay);
void cmd_set_ptt_off(int fd);
void cmd_set_ptt_on(int fd);	/* careful.. */

/*
 * get commands
 */

void cmd_get_update_store(int fd, unsigned char *buffer); /* data external */



