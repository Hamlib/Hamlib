/*  hamlib - (C) Frank Singleton 2000 (vk3fcs@ix.netcom.com)
 *
 * rig.h - Copyright (C) 2000 Frank Singleton and Stephane Fillod
 * This program defines the Hamlib API and rig capabilities structures that
 * will be used for obtaining rig capabilities.
 *
 *
 * 	$Id: rig.h,v 1.7 2000-09-16 21:37:25 javabear Exp $	 *
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

#ifndef _RIG_H
#define _RIG_H 1

#include <riglist.h>	/* list in another file to not mess up w/ this one */

/* Forward enum and struct references */

struct rig;
struct rig_state;

typedef struct rig RIG;


/* 
 * Rig capabilities
 *
 * Basic rig type, can store some useful
 * info about different radios. Each lib must
 * be able to populate this structure, so we can make
 * useful enquiries about capablilities.
 */



enum rig_port_e {
	RIG_PORT_SERIAL = 0,
	RIG_PORT_NETWORK
};

enum serial_parity_e {
	RIG_PARITY_NONE = 0,
	RIG_PARITY_ODD,
	RIG_PARITY_EVEN
};

enum serial_handshake_e {
	RIG_HANDSHAKE_NONE = 0,
	RIG_HANDSHAKE_XONXOFF,
	RIG_HANDSHAKE_HARDWARE
};


enum rig_type_e {
	RIG_TYPE_TRANSCEIVER = 0,	/* aka base station */
	RIG_TYPE_HANDHELD,
	RIG_TYPE_MOBILE,
	RIG_TYPE_RECEIVER,
	RIG_TYPE_SCANNER,
	RIG_TYPE_COMPUTER,		/* eg. Pegasus */
	/* etc. */
	RIG_TYPE_OTHER
};

/*
 * Development status of the backend 
 */
enum rig_status_e {
	RIG_STATUS_ALPHA = 0,
	RIG_STATUS_UNTESTED,	/* written from specs, rig unavailable for test */
	RIG_STATUS_BETA,
	RIG_STATUS_STABLE,
	RIG_STATUS_NEW
};

enum rig_rptr_shift_e {
	RIG_RPT_SHIFT_NONE = 0,
	RIG_RPT_SHIFT_MINUS,
	RIG_RPT_SHIFT_PLUS
};

typedef enum rig_rptr_shift_e rig_rptr_shift_t;

enum rig_vfo_e {
	RIG_VFO_MAIN = 0,
	RIG_VFO_RX,
	RIG_VFO_TX,
	RIG_VFO_SUB,
	RIG_VFO_SAT_RX,
	RIG_VFO_SAT_TX

};

typedef enum rig_vfo_e rig_vfo_t;

/*
 * frequency type in Hz, must be >32bits for SHF! 
 */
typedef long long freq_t;

typedef unsigned int rig_mode_t;

/* Do not use an enum since this will be used w/ rig_mode_t bit fields */
#define RIG_MODE_AM    	(1<<0)
#define RIG_MODE_CW    	(1<<1)
#define RIG_MODE_USB	(1<<2)	 /* select somewhere else the filters ? */
#define RIG_MODE_LSB	(1<<3)
#define RIG_MODE_RTTY	(1<<4)
#define RIG_MODE_FM    	(1<<5)
#define RIG_MODE_NFM	(1<<6)	/* should we distinguish modes w/ filers? */
#define RIG_MODE_WFM	(1<<7)
#define RIG_MODE_NAM	(1<<8)	/* Narrow AM */
#define RIG_MODE_WAM	(1<<9)	/* Wide AM */
#define RIG_MODE_NCW	(1<<10)	
#define RIG_MODE_WCW	(1<<11)	
#define RIG_MODE_CWR	(1<<12)	/* Reverse CW */
#define RIG_MODE_NUSB	(1<<13)	
#define RIG_MODE_WUSB	(1<<14)	
#define RIG_MODE_NLSB	(1<<15)	
#define RIG_MODE_WLSB	(1<<16)	
#define RIG_MODE_NRTTY	(1<<17)	
#define RIG_MODE_WRTTY	(1<<18)	


#define RIGNAMSIZ 30
#define RIGVERSIZ 8
#define MAXRIGPATHLEN 100
#define FRQRANGESIZ 30


/* put together a buch of this struct in an array to define 
 * what your have access to 
 */ 
struct freq_range_list {
		freq_t start;
		freq_t end;
		unsigned long modes;	/* bitwise OR'ed RIG_MODE_* */
		int low_power;	/* in mW, -1 for no power (ie. rx list) */
		int high_power;	/* in mW, -1 for no power (ie. rx list) */
};

/* Basic rig type, can store some useful
* info about different radios. Each lib must
* be able to populate this structure, so we can make
* useful enquiries about capablilities.
*/

/* The main idea of this struct is that it will be defined by the backend
 * rig driver, and will remain readonly for the application
 */
struct rig_caps {
	rig_model_t rig_model; /* eg. RIG_MODEL_FT847 */
	unsigned char model_name[RIGNAMSIZ]; /* eg "ft847" */
	unsigned char mfg_name[RIGNAMSIZ]; /* eg "Yeasu" */
	char version[RIGVERSIZ]; /* driver version, eg "0.5" */
	enum rig_status_e status; /* among ALPHA, BETA, STABLE, NEW  */
	enum rig_type_e rig_type;
	int serial_rate_min; /* eg 4800 */
	int serial_rate_max; /* eg 9600 */
	int serial_data_bits; /* eg 8 */
	int serial_stop_bits; /* eg 2 */
	enum serial_parity_e serial_parity; /* */
	enum serial_handshake_e serial_handshake; /* */
	int write_delay;		/* delay in ms between each byte sent out */
	int timeout;	/* in ms */
	int retry;		/* maximum number of retries, 0 to disable */
	struct freq_range_list rx_range_list[FRQRANGESIZ];
	struct freq_range_list tx_range_list[FRQRANGESIZ];

	int (*rig_init)(RIG *rig);	/* setup *priv */
	int (*rig_cleanup)(RIG *rig);
	int (*rig_open)(RIG *rig);	/* called when port just opened */
	int (*rig_close)(RIG *rig);	/* called before port is to close */
	int (*rig_probe)(RIG *rig); /* Experimental: may work.. */
  
       /* cmd API below */
	int (*set_freq_main_vfo_hz)(RIG *rig, freq_t freq, rig_mode_t mode);

  /*
    int (*set_freq)(RIG *rig, freq_t freq);
    int (*set_mode)(RIG *rig, rig_mode_t mode);
    int (*set_vfo)(RIG *rig, rig_vfo_t vfo);
  */

/* etc... */

};

/*
 * example of 3.5MHz -> 3.8MHz, 100W transmit range
 *   tx_range_list = {{3500000,3800000,RIG_MODE_AM|RIG_MODE_CW,100000},0}
 *
 */


/* 
 * Rig state
 */
struct rig_state {
	enum rig_port_e port_type;	/* serial, network, etc. */
	int serial_rate;
	int serial_data_bits; /* eg 8 */
	int serial_stop_bits; /* eg 2 */
	enum serial_parity_e serial_parity; /* */
	enum serial_handshake_e serial_handshake; /* */
	int write_delay;        /* delay in ms between each byte sent out */
	int timeout;	/* in ms */
	int retry;		/* maximum number of retries, 0 to disable */
	char rig_path[MAXRIGPATHLEN]; /* serial port/network path(host:port) */
	int fd;	/* serial port/socket file handle */
	void *priv;  /* pointer to private data */
	/* stuff like CI_V_address for Icom goes in this *priv 51 area */

	/* etc... */
};

/*
 * Rig callbacks
 * ie., the rig notify the host computer someone changed 
 *	the freq/mode from the panel, depressed a button, etc.
 * 
 * Event based programming, really appropriate in a GUI.
 * So far, haven't heard of any rig capable of this, but there must be. --SF
 */
struct rig_callbacks {
	int (*freq_main_vfo_hz_event)(RIG *rig, freq_t freq, rig_mode_t mode); 
	/* etc.. */
};

/* 
 * struct rig is the master data structure, 
 * acting as a handle for the controlled rig.
 */
struct rig {
	const struct rig_caps *caps;
	struct rig_state state;
	struct rig_callbacks callbacks;
};



/* --------------- API function prototypes -----------------*/

RIG *rig_init(rig_model_t rig_model);
int rig_open(RIG *rig);

int cmd_set_freq(RIG *rig, freq_t freq, rig_mode_t mode, rig_vfo_t vfo );
/* etc. */

int rig_close(RIG *rig);
int rig_cleanup(RIG *rig);

RIG *rig_probe(const char *rig_path);

const struct rig_caps *rig_get_caps(rig_model_t rig_model);

#endif /* _RIG_H */

