/*  hamlib - (C) Frank Singleton 2000 (vk3fcs@ix.netcom.com)
 *
 * rig.h - Copyright (C) 2000 Frank Singleton and Stephane Fillod
 * This program defines the Hamlib API and rig capabilities structures that
 * will be used for obtaining rig capabilities.
 *
 *
 * 	$Id: rig.h,v 1.15 2000-09-28 00:41:07 f4cfe Exp $	 *
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

/*
 * Error codes that can be returned by the Hamlib functions
 */
#define RIG_OK			0			/* completed sucessfully */
#define RIG_EINVAL		1			/* invalid parameter */
#define RIG_ECONF		2			/* invalid configuration (serial,..) */
#define RIG_ENOMEM		3			/* memory shortage */
#define RIG_ENIMPL		4			/* function not implemented */
#define RIG_ETIMEOUT	        5                       /* communication timed out */
#define RIG_EIO			6			/* IO error, including open failed */
#define RIG_EINTERNAL	        7			/* Internal Hamlib error, huh! */
#define RIG_EPROTO		8			/* Protocol error */
#define RIG_ERJCTED		9			/* Command rejected by the rig */
#define RIG_ETRUNC 		10			/* Command performed, but arg truncated */


/* Forward struct references */

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

enum rptr_shift_e {
	RIG_RPT_SHIFT_NONE = 0,
	RIG_RPT_SHIFT_MINUS,
	RIG_RPT_SHIFT_PLUS
};

typedef enum rptr_shift_e rptr_shift_t;

enum vfo_e {
	RIG_VFO_MAIN = 0,
	RIG_VFO_RX,
	RIG_VFO_TX,
	RIG_VFO_SUB,
	RIG_VFO_SAT_RX,
	RIG_VFO_SAT_TX,
	RIG_VFO_A,
	RIG_VFO_B,
	RIG_VFO_C

};

typedef enum vfo_e vfo_t;

enum ptt_e {
	RIG_PTT_OFF = 0,
	RIG_PTT_ON
};

typedef enum ptt_e ptt_t;

enum ptt_type_e {
	RIG_PTT_BUILTIN = 0,		/* PTT controlable through remote interface */
	RIG_PTT_SERIAL,				/* PTT accessed through CTS/RTS */
	RIG_PTT_PARALLEL,			/* PTT accessed through DATA0 */
	RIG_PTT_NONE				/* not available */
};


typedef enum ptt_type_e ptt_type_t;

/*
 * These are activated functions.
 */
#define RIG_FUNC_FAGC    	(1<<0)		/* Fast AGC */
#define RIG_FUNC_NB	    	(1<<1)		/* Noise Blanker */
#define RIG_FUNC_COMP    	(1<<2)		/* Compression */
#define RIG_FUNC_VOX    	(1<<3)		/* VOX */
#define RIG_FUNC_TONE    	(1<<4)		/* Tone */
#define RIG_FUNC_TSQL    	(1<<5)		/* may require a tone field */
#define RIG_FUNC_SBKIN    	(1<<6)		/* Semi Break-in (is it the rigth name?) */
#define RIG_FUNC_FBKIN    	(1<<7)		/* Full Break-in, for CW mode */
#define RIG_FUNC_ANF    	(1<<8)		/* Automatic Notch Filter (DSP) */
#define RIG_FUNC_NR     	(1<<9)		/* Noise Reduction (DSP) */



/*
 * frequency type in Hz, must be >32bits for SHF! 
 */
typedef long long freq_t;

#define Hz(f)	((freq_t)(f))
#define KHz(f)	((freq_t)((f)*1000))
#define MHz(f)	((freq_t)((f)*1000000))
#define GHz(f)	((freq_t)((f)*1000000000))

/*
 * power unit macros, converts to mW
 */
#define mW(p)	 ((int)(p))
#define Watts(p) ((int)((p)*1000))
#define KW(p)	 ((int)((p)*1000000))

typedef unsigned int rmode_t;	/* radio mode  */

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
#define FILPATHLEN 100
#define FRQRANGESIZ 30
#define MAXCHANDESC 30		/* describe channel eg: "WWV 5Mhz" */
#define TSLSTSIZ 20			/* max tuning step list size, zero ended */


/*
 * Put together a bunch of this struct in an array to define 
 * what your rig have access to 
 */ 
struct freq_range_list {
  freq_t start;
  freq_t end;
  unsigned long modes;	/* bitwise OR'ed RIG_MODE_* */
  int low_power;	/* in mW, -1 for no power (ie. rx list) */
  int high_power;	/* in mW, -1 for no power (ie. rx list) */
};

/*
 * Lists the tuning steps available for each mode
 */
struct tuning_step_list {
  unsigned long modes;	/* bitwise OR'ed RIG_MODE_* */
  unsigned long ts;		/* tuning step in Hz */
};



/* 
 * Convenience struct, describes a freq/vfo/mode combo 
 * Also useful for memory handling -- FS
 *
 */

struct channel {
  int channel_num;
  freq_t freq;
  rmode_t mode;
  vfo_t vfo;
  int power;	/* in mW */
  signed int preamp;	/* in dB, if < 0, this is attenuator */
  unsigned long tuning_step;	/* */
  unsigned char channel_desc[MAXCHANDESC];
};

typedef struct channel channel_t;

/* Basic rig type, can store some useful
* info about different radios. Each lib must
* be able to populate this structure, so we can make
* useful enquiries about capablilities.
*/

/* 
 * The main idea of this struct is that it will be defined by the backend
 * rig driver, and will remain readonly for the application.
 * Fields that need to be modifiable by the application are
 * copied into the struct rig_state, which is a kind of private
 * of the RIG instance.
 * This way, you can have several rigs running within the same application,
 * sharing the struct rig_caps of the backend, while keeping their own
 * customized data.
 */
struct rig_caps {
  rig_model_t rig_model; /* eg. RIG_MODEL_FT847 */
  unsigned char model_name[RIGNAMSIZ]; /* eg "ft847" */
  unsigned char mfg_name[RIGNAMSIZ]; /* eg "Yeasu" */
  char version[RIGVERSIZ]; /* driver version, eg "0.5" */
  enum rig_status_e status; /* among ALPHA, BETA, STABLE, NEW  */
  enum rig_type_e rig_type;
  enum ptt_type_e ptt_type;	/* how we will key the rig */
  int serial_rate_min; /* eg 4800 */
  int serial_rate_max; /* eg 9600 */
  int serial_data_bits; /* eg 8 */
  int serial_stop_bits; /* eg 2 */
  enum serial_parity_e serial_parity; /* */
  enum serial_handshake_e serial_handshake; /* */
  int write_delay;		/* delay in ms between each byte sent out */
  int timeout;	/* in ms */
  int retry;		/* maximum number of retries, 0 to disable */
  unsigned long has_func;		/* bitwise OR'ed RIG_FUNC_FAGC, NG, etc. */
  int chan_qty;		/* number of channels */
  struct freq_range_list rx_range_list[FRQRANGESIZ];
  struct freq_range_list tx_range_list[FRQRANGESIZ];
  struct tuning_step_list tuning_steps[TSLSTSIZ];

  /*
   * Rig Admin API
   *
   */
 
  int (*rig_init)(RIG *rig);	/* setup *priv */
  int (*rig_cleanup)(RIG *rig);
  int (*rig_open)(RIG *rig);	/* called when port just opened */
  int (*rig_close)(RIG *rig);	/* called before port is to close */
  int (*rig_probe)(RIG *rig); /* Experimental: may work.. */
  
  /*
   *  General API commands, from most primitive to least.. :()
   *  List Set/Get functions pairs
   */
  
  int (*set_freq)(RIG *rig, freq_t freq); /* select freq */
  int (*get_freq)(RIG *rig, freq_t *freq); /* get freq */

  int (*set_mode)(RIG *rig, rmode_t mode); /* select mode */
  int (*get_mode)(RIG *rig, rmode_t *mode); /* get mode */

  int (*set_vfo)(RIG *rig, vfo_t vfo); /* select vfo (A,B, etc.) */
  int (*get_vfo)(RIG *rig, vfo_t *vfo); /* get vfo */

  int (*set_ptt)(RIG *rig, ptt_t ptt); /* ptt on/off */
  int (*get_ptt)(RIG *rig, ptt_t *ptt); /* get ptt status */

  int (*set_rpt_shift)(RIG *rig, rptr_shift_t rptr_shift );	/* set repeater shift */
  int (*get_rpt_shift)(RIG *rig, rptr_shift_t *rptr_shift);	/* get repeater shift */

  int (*set_tone)(RIG *rig, unsigned int tone);	/* set tone */
  int (*get_tone)(RIG *rig, unsigned int *tone);	/* get tone */
  int (*set_tonesq)(RIG *rig, unsigned int tone);	/* set tone squelch */
  int (*get_tonesq)(RIG *rig, unsigned int *tone);	/* get tone squelch */

  /*
   * It'd be nice to have a power2mW and mW2power functions
   * that could tell at what power (watts) the rig is running.
   * Unfortunately, on most rigs, the formula is not the same
   * on all bands/modes. Have to work this out.. --SF
   */
  int (*set_power)(RIG *rig, float power);	/* set TX power [0.0 .. 1.0] */
  int (*get_power)(RIG *rig, float *power);

  int (*set_volume)(RIG *rig, float vol);	/* select vol from 0.0 and 1.0 */
  int (*get_volume)(RIG *rig, float *vol);	/* get volume */

  int (*set_squelch)(RIG *rig, float sql);	/* set squelch */
  int (*get_squelch)(RIG *rig, float *sql);	/* set squelch */
  int (*get_strength)(RIG *rig, int *strength);	/* get signal strength */

  int (*set_poweron)(RIG *rig);
  int (*set_poweroff)(RIG *rig);

  int (*set_func)(RIG *rig, unsigned long func); /* activate the function(s) */
  int (*get_func)(RIG *rig, unsigned long *func); /* get the setting from rig */
/*
 * Convenience Functions 
 */

  int (*set_channel)(RIG *rig, const struct channel *ch);
  int (*get_channel)(RIG *rig, struct channel *ch);

  /* more to come... */
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
  	enum ptt_type_e ptt_type;	/* how we will key the rig */
  	char ptt_path[FILPATHLEN];	/* path to the keying device (serial,//) */
	double vfo_comp;	/* VFO compensation in PPM, 0.0 to disable */
	char rig_path[FILPATHLEN]; /* serial port/network path(host:port) */
	int fd;	/* serial port/socket file handle */
	/*
	 * Pointer to private data
	 * tuff like CI_V_address for Icom goes in this *priv 51 area
	 */
	void *priv;  /* pointer to private data */

	/* etc... */
};

/*
 * Rig callbacks
 * ie., the rig notify the host computer someone changed 
 *	the freq/mode from the panel, depressed a button, etc.
 *	In order to achieve this, the hamlib would have to run
 *	an internal thread listening the rig with a select()...
 * 
 * Event based programming, really appropriate in a GUI.
 * So far, haven't heard of any rig capable of this, but there must be. --SF
 */
struct rig_callbacks {
	int (*freq_main_vfo_hz_event)(RIG *rig, freq_t freq, rmode_t mode); 
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

  /*
   *  General API commands, from most primitive to least.. :()
   *  List Set/Get functions pairs
   */

int rig_set_freq(RIG *rig, freq_t freq); /* select freq */
int rig_get_freq(RIG *rig, freq_t *freq); /* get freq */

int rig_set_mode(RIG *rig, rmode_t mode); /* select mode */
int rig_get_mode(RIG *rig, rmode_t *mode); /* get mode */

int rig_set_vfo(RIG *rig, vfo_t vfo); /* select vfo */
int rig_get_vfo(RIG *rig, vfo_t *vfo); /* get vfo */

int rig_set_ptt(RIG *rig, ptt_t ptt); /* ptt on/off */
int rig_get_ptt(RIG *rig, ptt_t *ptt); /* get ptt status */

int rig_set_rpt_shift(RIG *rig, rptr_shift_t rptr_shift ); /* set repeater shift */
int rig_get_rpt_shift(RIG *rig, rptr_shift_t *rptr_shift); /* get repeater shift */

int rig_set_power(RIG *rig, float power);
int rig_get_power(RIG *rig, float *power);
int rig_set_volume(RIG *rig, float vol);
int rig_get_volume(RIG *rig, float *vol);
int rig_set_squelch(RIG *rig, float sql);
int rig_get_squelch(RIG *rig, float *sql);
int rig_set_tonesq(RIG *rig, unsigned int tone);
int rig_get_tonesq(RIG *rig, unsigned int *tone);
int rig_set_tone(RIG *rig, unsigned int tone);
int rig_get_tone(RIG *rig, unsigned int *tone);
int rig_get_strength(RIG *rig, int *strength);
int rig_set_poweron(RIG *rig);
int rig_set_poweroff(RIG *rig);

/* more to come -- FS */

int rig_close(RIG *rig);
int rig_cleanup(RIG *rig);

RIG *rig_probe(const char *rig_path);

int rig_has_func(RIG *rig, unsigned long func);	/* is part of capabilities? */
int rig_set_func(RIG *rig, unsigned long func);	/* activate the function(s) */
int rig_get_func(RIG *rig, unsigned long *func); /* get the setting from rig */

const struct rig_caps *rig_get_caps(rig_model_t rig_model);

const char *rigerror(int errnum);

#endif /* _RIG_H */





