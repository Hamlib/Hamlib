/* ioctl API for radio devices.
 * (C) 1997 Michael McCormack
 *
 * Adapted for wrkit and newer winradio receivers.
 * (C) 1999-2000 Pascal Brisset
 */

#ifndef RADIO_H
#define RADIO_H

#include <linux/ioctl.h>

/* define ioctl() numbers for the radio */

#define RADIO_GET_POWER	  _IOR('w',0x00,long)
#define RADIO_SET_POWER	  _IOW('w',0x01,long)
#define RADIO_GET_MODE	  _IOR('w',0x02,long)
#define RADIO_SET_MODE	  _IOW('w',0x03,long)
#define RADIO_GET_MUTE	  _IOR('w',0x04,long)
#define RADIO_SET_MUTE	  _IOW('w',0x05,long)
#define RADIO_GET_ATTN	  _IOR('w',0x06,long)
#define RADIO_SET_ATTN	  _IOW('w',0x07,long)
#define RADIO_GET_VOL	  _IOR('w',0x08,long)
#define RADIO_SET_VOL	  _IOW('w',0x09,long)
#define RADIO_GET_FREQ	  _IOR('w',0x0a,long) /* Hz */
#define RADIO_SET_FREQ	  _IOW('w',0x0b,long)
#define RADIO_GET_BFO	  _IOR('w',0x0c,long) /* Hz */
#define RADIO_SET_BFO	  _IOW('w',0x0d,long)
/*
#define RADIO_GET_SSAM	  _IOR('w',0x0e,long)
#define RADIO_GET_SSFMN	  _IOR('w',0x0f,long)
#define RADIO_GET_SSFMW1  _IOR('w',0x10,long)
#define RADIO_GET_SSFMW2  _IOR('w',0x11,long)
*/
#define RADIO_GET_SS      _IOR('w',0x12,long) /* 0..120 */
#define RADIO_GET_IFS	  _IOR('w',0x13,long) /* Hz */
#define RADIO_SET_IFS	  _IOW('w',0x14,long)
#define RADIO_GET_DESCR	  _IOR('w',0x15,char[256])

#define RADIO_GET_AGC	  _IOR('w',0x16,long)
#define RADIO_SET_AGC	  _IOW('w',0x17,long)
#define RADIO_GET_IFG	  _IOR('w',0x18,long)
#define RADIO_SET_IFG	  _IOW('w',0x19,long)

#define RADIO_GET_MAXVOL  _IOR('w',0x20,long)

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
