/* ioctl API for radio devices.
 * (C) 1997 Michael McCormack
 *
 * Adapted for wrkit and newer winradio receivers.
 * (C) 1999-2000 <pab@users.sourceforge.net>
 */

#ifndef RADIO_H
#define RADIO_H

#include <linux/ioctl.h>

/* define ioctl() numbers for the radio */

#define RADIO_ID 0x8C /* See linux/Documentation/ioctl-number.txt */

#define RADIO_GET_POWER	  _IOR(RADIO_ID,0x00,long)
#define RADIO_SET_POWER	  _IOW(RADIO_ID,0x01,long)
#define RADIO_GET_MODE	  _IOR(RADIO_ID,0x02,long)
#define RADIO_SET_MODE	  _IOW(RADIO_ID,0x03,long)
#define RADIO_GET_MUTE	  _IOR(RADIO_ID,0x04,long)
#define RADIO_SET_MUTE	  _IOW(RADIO_ID,0x05,long)
#define RADIO_GET_ATTN	  _IOR(RADIO_ID,0x06,long)
#define RADIO_SET_ATTN	  _IOW(RADIO_ID,0x07,long)
#define RADIO_GET_VOL	  _IOR(RADIO_ID,0x08,long)
#define RADIO_SET_VOL	  _IOW(RADIO_ID,0x09,long)
#define RADIO_GET_FREQ	  _IOR(RADIO_ID,0x0a,long) /* Hz */
#define RADIO_SET_FREQ	  _IOW(RADIO_ID,0x0b,long)
#define RADIO_GET_BFO	  _IOR(RADIO_ID,0x0c,long) /* Hz */
#define RADIO_SET_BFO	  _IOW(RADIO_ID,0x0d,long)
/*
#define RADIO_GET_SSAM	  _IOR(RADIO_ID,0x0e,long)
#define RADIO_GET_SSFMN	  _IOR(RADIO_ID,0x0f,long)
#define RADIO_GET_SSFMW1  _IOR(RADIO_ID,0x10,long)
#define RADIO_GET_SSFMW2  _IOR(RADIO_ID,0x11,long)
*/
#define RADIO_GET_SS      _IOR(RADIO_ID,0x12,long) /* 0..120 */
#define RADIO_GET_IFS	  _IOR(RADIO_ID,0x13,long) /* Hz */
#define RADIO_SET_IFS	  _IOW(RADIO_ID,0x14,long)
#define RADIO_GET_DESCR	  _IOR(RADIO_ID,0x15,char[256])

#define RADIO_GET_AGC	  _IOR(RADIO_ID,0x16,long)
#define RADIO_SET_AGC	  _IOW(RADIO_ID,0x17,long)
#define RADIO_GET_IFG	  _IOR(RADIO_ID,0x18,long)
#define RADIO_SET_IFG	  _IOW(RADIO_ID,0x19,long)
/* Someone forgot 0x1A-0x1F ? */
#define RADIO_GET_MAXVOL  _IOR(RADIO_ID,0x20,long)
#define RADIO_GET_MAXIFG  _IOR(RADIO_ID,0x21,long)

/* radio modes */

typedef enum {
  RADIO_CW  = 0,
  RADIO_AM  = 1,
  RADIO_FMN = 2,
  RADIO_FMW = 3,
  RADIO_LSB = 4,
  RADIO_USB = 5,
  RADIO_FMM = 6,
  RADIO_FM6 = 7,
} radio_mode;

#endif /* RADIO_H */
