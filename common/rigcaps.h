/*  hamlib - Copyright (C) 2000 Frank Singleton
 *
 * rigcaps.h - Copyrith (C) 2000 Frank Singleton and Stephane Fillod
 * This program defines some rig capabilities structures that
 * will be used for obtaining rig capabilities,
 *
 *
 * 	$Id: rigcaps.h,v 1.1 2000-09-14 00:50:55 f4cfe Exp $	 *
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

/*
 * Basic rig type, can store some useful
 * info about different radios. Each lib must
 * be able to populate this structure, so we can make
 * useful enquiries about capablilities.
 */

#ifndef _RIGCAPS_H
#define _RIGCAPS_H 1

#include <riglist.h>	/* list in another file to not mess up w/ this one */

#include <rig.h>

typedef struct rig RIG;	/* forward reference to <rig.h> */
struct rig_state;


enum serial_parity_e {
	RIG_PARITY_NONE = 0,
	RIG_PARITY_ODD,
	RIG_PARITY_EVEN
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

/* development status of the backend */
enum rig_status_e {
	RIG_STATUS_ALPHA = 0,
	RIG_STATUS_BETA,
	RIG_STATUS_STABLE,
	RIG_STATUS_NEW
};

#ifdef __GNUC__
typedef unsigned long long int freq_t; /* in Hz, must be >32bits for SHF! */
#else
typedef struct {
	unsigned long int val[2];
} freq_t;
#endif

typedef unsigned int rig_mode_t;

/* Do not use an enum since this will be used w/ rig_mode_t bit fields */
#define RIG_MODE_AM		(1<<0)
#define RIG_MODE_CW		(1<<1)
#define RIG_MODE_USB	(1<<2)	 /* select somewhere else the filters ? */
#define RIG_MODE_LSB	(1<<3)
#define RIG_MODE_RTTY	(1<<4)
#define RIG_MODE_FM		(1<<5)
#define RIG_MODE_WFM	(1<<6)
#define RIG_MODE_NFM	(1<<7)	/* should we distinguish modes w/ filers? */
#define RIG_MODE_NAM	(1<<8)	/* Narrow AM */
#define RIG_MODE_CWR	(1<<9)	/* Reverse CW */


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
		unsigned long modes;	/* OR'ed RIG_MODE_* */
		int low_power;	/* in mW, -1 for no power (ie. rx list) */
		int high_power;	/* in mW, -1 for no power (ie. rx list) */
}

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
char model_name[RIGNAMSIZ]; /* eg "ft847" */
char mfg_name[RIGNAMSIZ]; /* eg "Yeasu" */
char version[RIGVERSIZ]; /* driver version, eg "0.5" */
enum rig_status_e status; /* among ALPHA, BETA, STABLE, NEW  */
enum rig_type_e rig_type;
unsigned short int serial_rate_min; /* eg 4800 */
unsigned short int serial_rate_max; /* eg 9600 */
unsigned char serial_data_bits; /* eg 8 */
unsigned char serial_stop_bits; /* eg 2 */
enum serial_parity_e serial_parity; /* */
struct freq_range_list rx_range_list[FRQRANGESIZ];
struct freq_range_list tx_range_list[FRQRANGESIZ];
int (*rig_init)(RIG *rig);	/* setup *priv */
int (*rig_cleanup)(RIG *rig);
int (*rig_probe)(RIG *rig); /* Experimental: may work.. */
int (*set_freq_main_vfo_hz)(struct rig_state *rig_s, freq_t freq, rig_mode_t mode);
/* etc... */

};

/*
 * example of 3.5MHz -> 3.8MHz, 100W transmit range
 *   tx_range_list = {{3500000,3800000,RIG_MODE_AM|RIG_MODE_CW,100000},0}
 *
 */


#endif /* _RIGCAPS_H */

