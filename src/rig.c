/* hamlib - Ham Radio Control Libraries
   Copyright (C) 2000 Stephane Fillod and Frank Singleton
   This file is part of the hamlib package.

   $Id: rig.c,v 1.5 2000-10-16 22:08:51 f4cfe Exp $

   Hamlib is free software; you can redistribute it and/or modify it
   under the terms of the GNU General Public License as published by
   the Free Software Foundation; either version 2 of the License, or
   (at your option) any later version.

   Hamlib is distributed in the hope that it will be useful, but WITHOUT
   ANY WARRANTY; without even the implied warranty of MERCHANTABILITY
   or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public
   License for more details.

   You should have received a copy of the GNU General Public License
   along with sane; see the file COPYING.  If not, write to the Free
   Software Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 */

#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <stdio.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>


#include <hamlib/rig.h>
#include <hamlib/riglist.h>
#include "serial.h"
#include "event.h"


#define DEFAULT_SERIAL_PORT "/dev/ttyS0"

/*
 * Data structure to track the opened rig (by rig_open)
 */
struct opened_rig_l {
		RIG *rig;
		struct opened_rig_l *next;
};
static struct opened_rig_l *opened_rig_list = { NULL };


/*
 * Careful, the order must be the same as their RIG_E* counterpart!
 * TODO: localise the messages..
 */
static const char *rigerror_table[] = {
		"Command completed sucessfully",
		"Invalid parameter",
		"Invalid configuration",
		"Memory shortage",
		"Feature not implemented",
		"Communication timed out",
		"IO error",
		"Internal Hamlib error",
		"Protocol error",
		"Command rejected by the rig",
		"Command performed, but arg truncated, result not guaranteed",
		"Feature not available",
		NULL,
};

/*
 * TODO: check table bounds, use gettext
 */
const char *rigerror(int errnum)
{
			return rigerror_table[abs(errnum)];
}

static int add_opened_rig(RIG *rig)
{
	struct opened_rig_l *p;
	p = (struct opened_rig_l *)malloc(sizeof(struct opened_rig_l));
	if (!p)
			return -RIG_ENOMEM;
	p->rig = rig;
	p->next = opened_rig_list;
	opened_rig_list = p;
	return RIG_OK;
}

static int remove_opened_rig(RIG *rig)
{	
	struct opened_rig_l *p,*q;
	q = NULL;

	for (p=opened_rig_list; p; p=p->next) {
			if (p->rig == rig) {
					if (q == NULL) {
							opened_rig_list = opened_rig_list->next;
					} else {
							q->next = p->next;
					}
					free(p);
					return RIG_OK;
			}
			q = p;
	}
	return -RIG_EINVAL;	/* Not found in list ! */
}

/*
 * Execs (*cfunc)(data) on each opened rig
 * Stops when cfunc returns 0
 */
int foreach_opened_rig(int (*cfunc)(RIG *, void *),void *data)
{	
	struct opened_rig_l *p;

	for (p=opened_rig_list; p; p=p->next) {
			if ((*cfunc)(p->rig,data) == 0)
					return RIG_OK;
	}
	return RIG_OK;
}

RIG *rig_init(rig_model_t rig_model)
{
		RIG *rig;
		const struct rig_caps *caps;

		rig_debug(RIG_DEBUG_VERBOSE,"rig:rig_init called \n");

		caps = rig_get_caps(rig_model);
		if (!caps)
				return NULL;

		/*
		 * okay, we've found it. Allocate some memory and set it to zeros,
		 * and especially the initialize the callbacks 
		 */ 
		rig = calloc(1, sizeof(RIG));
		if (rig == NULL) {
				/*
				 * FIXME: how can the caller know it's a memory shortage,
				 * 		  and not "rig not found" ?
				 */
				return NULL;
		}

		rig->caps = caps;

		/*
		 * populate the rig->state
		 * TODO: read the Preferences here! 
		 */

		rig->state.port_type = RIG_PORT_SERIAL; /* default is serial port */
		strncpy(rig->state.rig_path, DEFAULT_SERIAL_PORT, FILPATHLEN);
		rig->state.port_type = RIG_PORT_SERIAL; /* default is serial port */
		rig->state.serial_rate = rig->caps->serial_rate_max;	/* fastest ! */
		rig->state.serial_data_bits = rig->caps->serial_data_bits;
		rig->state.serial_stop_bits = rig->caps->serial_stop_bits;
		rig->state.serial_parity = rig->caps->serial_parity;
		rig->state.serial_handshake = rig->caps->serial_handshake;
		rig->state.write_delay = rig->caps->write_delay;
		rig->state.post_write_delay = rig->caps->post_write_delay;

		rig->state.timeout = rig->caps->timeout;
		rig->state.retry = rig->caps->retry;
		rig->state.transceive = rig->caps->transceive;
		rig->state.ptt_type = rig->caps->ptt_type;
		rig->state.vfo_comp = 0.0;	/* override it with preferences */

		/* 
		 * let the backend a chance to setup his private data
		 * FIXME: check rig_init() return code
		 */
		if (rig->caps->rig_init != NULL)
				rig->caps->rig_init(rig);	

		return rig;
}

int rig_open(RIG *rig)
{
		int status;

		rig_debug(RIG_DEBUG_VERBOSE,"rig:rig_open called \n");

		if (!rig)
				return -RIG_EINVAL;

		switch(rig->state.port_type) {
		case RIG_PORT_SERIAL:
				status = serial_open(&rig->state);
				if (status != 0)
						return status;
				break;

		case RIG_PORT_NETWORK:	/* not implemented yet! */
		default:
				return -RIG_ENIMPL;
		}

		rig->state.stream = fdopen(rig->state.fd, "r+b");

		add_opened_rig(rig);

		/* 
		 * Maybe the backend has something to initialize
		 * FIXME: check rig_open() return code
		 */
		if (rig->caps->rig_open != NULL)
				rig->caps->rig_open(rig);	

		return RIG_OK;
}

/*
 * Examples of typical rig_* wrapper
 */

/*
 * rig_set_freq
 *
 */

int rig_set_freq(RIG *rig, freq_t freq)
{
		if (!rig || !rig->caps)
			return -RIG_EINVAL;

		if (rig->state.vfo_comp != 0.0)
				freq += (freq_t)(rig->state.vfo_comp * freq);

		if (rig->caps->set_freq == NULL)
			return -RIG_ENAVAIL;	/* not implemented */
		else
			return rig->caps->set_freq(rig, freq);
}

/*
 * rig_get_freq
 *
 */

int rig_get_freq(RIG *rig, freq_t *freq)
{
		int status;

		if (!rig || !rig->caps || !freq)
			return -RIG_EINVAL;

		if (rig->caps->get_freq == NULL)
			return -RIG_ENAVAIL;	/* not implemented */
		else {
			status = rig->caps->get_freq(rig, freq);
			if (rig->state.vfo_comp != 0.0)
				*freq += (freq_t)(rig->state.vfo_comp * (*freq));
			return status;
		}
}


/*
 * rig_set_mode
 *
 */

int rig_set_mode(RIG *rig, rmode_t mode)
{
		if (!rig || !rig->caps)
			return -RIG_EINVAL;

		if (rig->caps->set_mode == NULL)
			return -RIG_ENAVAIL;	/* not implemented */
		else
			return rig->caps->set_mode(rig, mode);
}

/*
 * rig_get_mode
 *
 */

int rig_get_mode(RIG *rig, rmode_t *mode)
{
		if (!rig || !rig->caps || !mode)
			return -RIG_EINVAL;

		if (rig->caps->get_mode == NULL)
			return -RIG_ENAVAIL;	/* not implemented */
		else
			return rig->caps->get_mode(rig, mode);
}

/*
 * rig_set_passband
 *
 */

int rig_set_passband(RIG *rig, pbwidth_t width)
{
		if (!rig || !rig->caps)
			return -RIG_EINVAL;

		if (rig->caps->set_passband == NULL)
			return -RIG_ENAVAIL;	/* not implemented */
		else
			return rig->caps->set_passband(rig, width);
}

/*
 * rig_get_passband
 *
 */

int rig_get_passband(RIG *rig, pbwidth_t *width)
{
		if (!rig || !rig->caps || !width)
			return -RIG_EINVAL;

		if (rig->caps->get_passband == NULL)
			return -RIG_ENAVAIL;	/* not implemented */
		else
			return rig->caps->get_passband(rig, width);
}

/*
 * rig_set_vfo
 *
 */

int rig_set_vfo(RIG *rig, vfo_t vfo)
{
		if (!rig || !rig->caps)
			return -RIG_EINVAL;

		if (rig->caps->set_vfo == NULL)
			return -RIG_ENAVAIL;	/* not implemented */
		else
			return rig->caps->set_vfo(rig, vfo);
}

/*
 * rig_get_vfo
 *
 */

int rig_get_vfo(RIG *rig, vfo_t *vfo)
{
		if (!rig || !rig->caps || !vfo)
			return -RIG_EINVAL;

		if (rig->caps->get_vfo == NULL)
			return -RIG_ENAVAIL;	/* not implemented */
		else
			return rig->caps->get_vfo(rig, vfo);
}

/*
 * rig_set_ptt
 * Set ptt on/off
 */

int rig_set_ptt(RIG *rig, ptt_t ptt)
{
		if (!rig || !rig->caps)
			return -RIG_EINVAL;

		switch (rig->state.ptt_type) {
		case RIG_PTT_RIG:
			if (rig->caps->set_ptt == NULL)
				return -RIG_ENIMPL;	/* not implemented */
			else
				return rig->caps->set_ptt(rig, ptt);
			break;

		case RIG_PTT_SERIAL:
		case RIG_PTT_PARALLEL:
		case RIG_PTT_NONE:
		default:
				return -RIG_ENIMPL;	/* not implemented */
		}
}

/*
 * rig_get_ptt
 * Are we on air?
 */

int rig_get_ptt(RIG *rig, ptt_t *ptt)
{
		if (!rig || !rig->caps || !ptt)
			return -RIG_EINVAL;

		switch (rig->state.ptt_type) {
		case RIG_PTT_RIG:
			if (rig->caps->get_ptt == NULL)
				return -RIG_ENIMPL;	/* not implemented */
			else
				return rig->caps->get_ptt(rig, ptt);
			break;

		case RIG_PTT_SERIAL:
		case RIG_PTT_PARALLEL:
		case RIG_PTT_NONE:
		default:
				return -RIG_ENIMPL;	/* not implemented */
		}
}


/*
 * rig_set_rpt_shift
 * Set the repeater shift
 */

int rig_set_rpt_shift(RIG *rig, rptr_shift_t rptr_shift)
{
		if (!rig || !rig->caps)
			return -RIG_EINVAL;

		if (rig->caps->set_rpt_shift == NULL)
			return -RIG_ENAVAIL;	/* not implemented */
		else
			return rig->caps->set_rpt_shift(rig, rptr_shift);
}

/*
 * rig_get_rpt_shift
 * Get the current repeater shift
 */

int rig_get_rpt_shift(RIG *rig, rptr_shift_t *rptr_shift)
{
		if (!rig || !rig->caps || !rptr_shift)
			return -RIG_EINVAL;

		if (rig->caps->get_rpt_shift == NULL)
			return -RIG_ENAVAIL;	/* not implemented */
		else
			return rig->caps->get_rpt_shift(rig, rptr_shift);
}

/*
 * rig_set_rpt_offs
 * Set the repeater offset
 */

int rig_set_rpt_offs(RIG *rig, unsigned long rptr_offs)
{
		if (!rig || !rig->caps)
			return -RIG_EINVAL;

		if (rig->caps->set_rpt_offs == NULL)
			return -RIG_ENAVAIL;	/* not implemented */
		else
			return rig->caps->set_rpt_offs(rig, rptr_offs);
}

/*
 * rig_get_rpt_offs
 * Get the current repeater offset
 */

int rig_get_rpt_offs(RIG *rig, unsigned long *rptr_offs)
{
		if (!rig || !rig->caps || !rptr_offs)
			return -RIG_EINVAL;

		if (rig->caps->get_rpt_offs == NULL)
			return -RIG_ENAVAIL;	/* not implemented */
		else
			return rig->caps->get_rpt_offs(rig, rptr_offs);
}


/*
 * rig_set_split_freq
 * Set the split frequencies
 */

int rig_set_split_freq(RIG *rig, freq_t rx_freq, freq_t tx_freq)
{
		if (!rig || !rig->caps)
			return -RIG_EINVAL;

		if (rig->caps->set_split_freq == NULL)
			return -RIG_ENAVAIL;	/* not implemented */
		else
			return rig->caps->set_split_freq(rig, rx_freq, tx_freq);
}

/*
 * rig_get_split_freq
 * Get the current split frequencies
 */

int rig_get_split_freq(RIG *rig, freq_t *rx_freq, freq_t *tx_freq)
{
		if (!rig || !rig->caps || !rx_freq || !tx_freq)
			return -RIG_EINVAL;

		if (rig->caps->get_split_freq == NULL)
			return -RIG_ENAVAIL;	/* not implemented */
		else
			return rig->caps->get_split_freq(rig, rx_freq, tx_freq);
}



/*
 * rig_set_split
 * Set the split mode
 */

int rig_set_split(RIG *rig, split_t split)
{
		if (!rig || !rig->caps)
			return -RIG_EINVAL;

		if (rig->caps->set_split == NULL)
			return -RIG_ENAVAIL;	/* not implemented */
		else
			return rig->caps->set_split(rig, split);
}

/*
 * rig_get_split
 * Get the current split mode
 */

int rig_get_split(RIG *rig, split_t *split)
{
		if (!rig || !rig->caps || !split)
			return -RIG_EINVAL;

		if (rig->caps->get_split == NULL)
			return -RIG_ENAVAIL;	/* not implemented */
		else
			return rig->caps->get_split(rig, split);
}




/*
 * rig_set_ts
 * Set the Tuning Step
 */

int rig_set_ts(RIG *rig, unsigned long ts)
{
		if (!rig || !rig->caps)
			return -RIG_EINVAL;

		if (rig->caps->set_ts == NULL)
			return -RIG_ENAVAIL;	/* not implemented */
		else
			return rig->caps->set_ts(rig, ts);
}

/*
 * rig_get_ts
 * Get the current Tuning Step
 */

int rig_get_ts(RIG *rig, unsigned long *ts)
{
		if (!rig || !rig->caps || !ts)
			return -RIG_EINVAL;

		if (rig->caps->get_ts == NULL)
			return -RIG_ENAVAIL;	/* not implemented */
		else
			return rig->caps->get_ts(rig, ts);
}

#if 0
/*
 * rig_set_power
 * NB: power must be on a [0.0 .. 1.0] scale
 * Approximation and rounding is done by the backend
 */

int rig_set_power(RIG *rig, float power)
{
		if (!rig || !rig->caps || power<0.0 || power>1.0)
			return -RIG_EINVAL;

		if (rig->caps->set_power == NULL)
			return -RIG_ENAVAIL;	/* not implemented */
		else
			return rig->caps->set_power(rig, power);
}

/*
 * rig_get_power
 * NB: returned power must be on a [0.0 .. 1.0] scale
 */

int rig_get_power(RIG *rig, float *power)
{
		if (!rig || !rig->caps || !power)
			return -RIG_EINVAL;

		if (rig->caps->get_power == NULL)
			return -RIG_ENAVAIL;	/* not implemented */
		else
			return rig->caps->get_power(rig, power);
}
#endif

/*
 * rig_power2mW
 * NB: power must be on a [0.0 .. 1.0] scale, returned value is in mW
 */
int rig_power2mW(RIG *rig, unsigned int *mwpower, float power, freq_t freq, mode_t mode)
{
		const freq_range_t *txrange;

		if (!rig || !rig->caps || !mwpower || power<0.0 || power>1.0)
			return -RIG_EINVAL;

		if (rig->caps->power2mW == NULL) {
			txrange = rig_get_range(rig->caps->tx_range_list, freq, mode);
			if (!txrange) {
				/*
				 * freq is not on the tx range!
				 */
				return -RIG_ECONF; /* could be RIG_EINVAL ? */
			}
			*mwpower = (unsigned long)(power*txrange->high_power);
			return RIG_OK;
		} else
			return rig->caps->power2mW(rig, mwpower, power, freq, mode);
}

/*
 * rig_mW2power
 * NB: returned power is on a [0.0 .. 1.0] scale
 */
int rig_mW2power(RIG *rig, float *power, unsigned int mwpower, freq_t freq, mode_t mode)
{
		const freq_range_t *txrange;

		if (!rig || !rig->caps || !power || mwpower==0)
			return -RIG_EINVAL;

		if (rig->caps->mW2power == NULL) {
			txrange = rig_get_range(rig->caps->tx_range_list, freq, mode);
			if (!txrange) {
				/*
				 * freq is not on the tx range!
				 */
				return -RIG_ECONF; /* could be RIG_EINVAL ? */
			}
			*power = txrange->high_power/mwpower;
			if (*power > 1.0)
					*power = 1.0;
			return (mwpower>txrange->high_power? RIG_OK : RIG_ETRUNC);
		} else
			return rig->caps->mW2power(rig, power, mwpower, freq, mode);
}

#if 0

/*
 * rig_set_volume
 * The volume is specified on a scale from 0.0 to 1.0.
 * 0.0 means the lowest volume, whereas 1.0 is the loudest the rig
 * can do.
 * The backend is responsible for the appropriate conversion (and rounding)
 */

int rig_set_volume(RIG *rig, float vol)
{
		if (!rig || !rig->caps || vol<0.0 || vol>1.0)
			return -RIG_EINVAL;

		if (rig->caps->set_volume == NULL)
			return -RIG_ENAVAIL;	/* not implemented */
		else
			return rig->caps->set_volume(rig, vol);
}

/*
 * rig_get_volume
 *
 */

int rig_get_volume(RIG *rig, float *vol)
{
		if (!rig || !rig->caps || !vol)
			return -RIG_EINVAL;

		if (rig->caps->get_volume == NULL)
			return -RIG_ENAVAIL;	/* not implemented */
		else
			return rig->caps->get_volume(rig, vol);
}

/*
 * rig_set_squelch
 *
 */

int rig_set_squelch(RIG *rig, float sql)
{
		if (!rig || !rig->caps || sql<0.0 || sql>1.0)
			return -RIG_EINVAL;

		if (rig->caps->set_squelch == NULL)
			return -RIG_ENAVAIL;	/* not implemented */
		else
			return rig->caps->set_squelch(rig, sql);
}

/*
 * rig_get_squelch
 * Read squelch condition
 */

int rig_get_squelch(RIG *rig, float *sql)
{
		if (!rig || !rig->caps || !sql)
			return -RIG_EINVAL;

		if (rig->caps->get_squelch == NULL)
			return -RIG_ENAVAIL;	/* not implemented */
		else
			return rig->caps->get_squelch(rig, sql);
}

/*
 * rig_get_squelch_status
 * Read squelch status (open/closed)
 */

int rig_get_squelch_status(RIG *rig, int *sql_status)
{
		if (!rig || !rig->caps || !sql_status)
			return -RIG_EINVAL;

		if (rig->caps->get_squelch_status == NULL)
			return -RIG_ENAVAIL;	/* not implemented */
		else
			return rig->caps->get_squelch_status(rig, sql_status);
}

/*
 * rig_set_ant
 * Set antenna number
 */

int rig_set_ant(RIG *rig, int ant)
{
		if (!rig || !rig->caps)
			return -RIG_EINVAL;

		if (rig->caps->set_ant == NULL)
			return -RIG_ENAVAIL;	/* not implemented */
		else
			return rig->caps->set_ant(rig, ant);
}

/*
 * rig_get_ant
 * Read squelch condition
 */

int rig_get_ant(RIG *rig, int *ant)
{
		if (!rig || !rig->caps || !ant)
			return -RIG_EINVAL;

		if (rig->caps->get_ant == NULL)
			return -RIG_ENAVAIL;	/* not implemented */
		else
			return rig->caps->get_ant(rig, ant);
}

/*
 * rig_set_att
 * Set attenuator (db)
 */

int rig_set_att(RIG *rig, int att)
{
		if (!rig || !rig->caps)
			return -RIG_EINVAL;

		if (rig->caps->set_att == NULL)
			return -RIG_ENAVAIL;	/* not implemented */
		else
			return rig->caps->set_att(rig, att);
}

/*
 * rig_get_att
 * Read attenuator (db)
 */

int rig_get_att(RIG *rig, int *att)
{
		if (!rig || !rig->caps || !att)
			return -RIG_EINVAL;

		if (rig->caps->get_att == NULL)
			return -RIG_ENAVAIL;	/* not implemented */
		else
			return rig->caps->get_att(rig, att);
}

/*
 * rig_set_preamp
 * Set preamp (db)
 */

int rig_set_preamp(RIG *rig, int preamp)
{
		if (!rig || !rig->caps)
			return -RIG_EINVAL;

		if (rig->caps->set_preamp == NULL)
			return -RIG_ENAVAIL;	/* not implemented */
		else
			return rig->caps->set_preamp(rig, preamp);
}

/*
 * rig_get_preamp
 * Read preamp (db)
 */

int rig_get_preamp(RIG *rig, int *preamp)
{
		if (!rig || !rig->caps || !preamp)
			return -RIG_EINVAL;

		if (rig->caps->get_preamp == NULL)
			return -RIG_ENAVAIL;	/* not implemented */
		else
			return rig->caps->get_preamp(rig, preamp);
}
#endif

/*
 * rig_set_ctcss
 * Set CTCSS.
 * NB: tone is NOT in HZ, but in tenth of Hz!
 * Exemple: If you want to set subaudible tone of 88.5, then pass 885
 * 			to this function
 * Also, if you want to disable Tone squelch, set tone to 0.
 */

int rig_set_ctcss(RIG *rig, unsigned int tone)
{
		if (!rig || !rig->caps)
			return -RIG_EINVAL;

		if (rig->caps->set_ctcss == NULL)
			return -RIG_ENAVAIL;	/* not implemented */
		else
			return rig->caps->set_ctcss(rig, tone);
}

/*
 * rig_get_ctcss
 * Read CTCSS
 * NB: tone is NOT in HZ, but in tenth of Hz!
 */

int rig_get_ctcss(RIG *rig, unsigned int *tone)
{
		if (!rig || !rig->caps || !tone)
			return -RIG_EINVAL;

		if (rig->caps->get_ctcss == NULL)
			return -RIG_ENAVAIL;	/* not implemented */
		else
			return rig->caps->get_ctcss(rig, tone);
}

/*
 * rig_set_dcs
 * Set subaudible tone to access a repeater.
 * NB: tone is NOT in HZ, but in tenth of Hz!
 * Exemple: If you want to set subaudible tone of 88.5, then pass 885
 * 			to this function
 * Also, if you want to disable Tone generation, set tone to 0.
 */

int rig_set_dcs(RIG *rig, unsigned int tone)
{
		if (!rig || !rig->caps)
			return -RIG_EINVAL;

		if (rig->caps->set_dcs == NULL)
			return -RIG_ENAVAIL;	/* not implemented */
		else
			return rig->caps->set_dcs(rig, tone);
}

/*
 * rig_get_dcs
 * Read current subaudible tone to access a repeater.
 * NB: tone is NOT in HZ, but in tenth of Hz!
 */

int rig_get_dcs(RIG *rig, unsigned int *tone)
{
		if (!rig || !rig->caps || !tone)
			return -RIG_EINVAL;

		if (rig->caps->get_dcs == NULL)
			return -RIG_ENAVAIL;	/* not implemented */
		else
			return rig->caps->get_dcs(rig, tone);
}


#if 0
/*
 * rig_get_strength
 * read S-meter level
 * The signal strength in db is returned in pointed strength argument
 */

int rig_get_strength(RIG *rig, int *strength)
{
		if (!rig || !rig->caps || !strength)
			return -RIG_EINVAL;

		if (rig->caps->get_strength == NULL)
			return -RIG_ENAVAIL;	/* not implemented */
		else
			return rig->caps->get_strength(rig, strength);
}
#endif

/*
 * rig_set_poweron
 */

int rig_set_poweron(RIG *rig)
{
		if (!rig || !rig->caps)
			return -RIG_EINVAL;

		if (rig->caps->set_poweron == NULL)
			return -RIG_ENAVAIL;	/* not implemented */
		else
			return rig->caps->set_poweron(rig);
}

/*
 * rig_set_poweroff
 */

int rig_set_poweroff(RIG *rig)
{
		if (!rig || !rig->caps)
			return -RIG_EINVAL;

		if (rig->caps->set_poweroff == NULL)
			return -RIG_ENAVAIL;	/* not implemented */
		else
			return rig->caps->set_poweroff(rig);
}



/*
 * more rig_* to come -- FS
 *
 */



/*
 * Close port
 */
int rig_close(RIG *rig)
{
		rig_debug(RIG_DEBUG_VERBOSE,"rig:rig_close called \n");

		if (rig == NULL || rig->caps)
				return -RIG_EINVAL;

		if (rig->state.transceive) {
				/*
				 * TODO: check error codes et al.
				 */
				remove_trn_rig(rig);
		}

		/*
		 * Let the backend say 73s to the rig
		 */
		if (rig->caps->rig_close)
				rig->caps->rig_close(rig);

		if (rig->state.fd != -1) {
				if (!rig->state.stream)
						fclose(rig->state.stream); /* this closes also fd */
				else
					close(rig->state.fd);
				rig->state.fd = -1;
				rig->state.stream = NULL;
		}

		remove_opened_rig(rig);

		return RIG_OK;
}

/*
 * Release a rig struct which port has already been closed
 */
int rig_cleanup(RIG *rig)
{
		rig_debug(RIG_DEBUG_VERBOSE,"rig:rig_cleanup called \n");

		if (rig == NULL || rig->caps)
				return -RIG_EINVAL;

		/*
		 * basically free up the priv struct 
		 */
		if (rig->caps->rig_cleanup)
				rig->caps->rig_cleanup(rig);

		free(rig);

		return RIG_OK;
}



#if 0

/* CAUTION: this is really Experimental, It never worked!!
 * try to guess a rig, can be very buggy! (but fun if it works!)
 * FIXME: finish me and handle nicely errors
 */
RIG *rig_probe(const char *port_path)
{
		RIG *rig;
		int i;

		for (i = 0; rig_base[i]; i++) {
			if (rig_base[i]->rig_probe != NULL) {
				rig = rig_init(rig_base[i]->rig_model);
				strncpy(rig->state.rig_path, port_path, FILPATHLEN);
				rig_open(rig);
				if (rig && rig_base[i]->rig_probe(rig) == 0) {
					return rig;
				} else {
					rig_close(rig);
					rig_cleanup(rig);
				}
			}
		}
		return NULL;
}
#endif

/*
 * rig_set_level
 */
int rig_set_level(RIG *rig, setting_t set, value_t val)
{
		if (!rig || !rig->caps)
			return -RIG_EINVAL;

		if (rig->caps->set_level == NULL)
			return -RIG_ENAVAIL;	/* not implemented */
		else
			return rig->caps->set_level(rig, set, val);
}

/*
 * rig_get_level
 */

int rig_get_level(RIG *rig, setting_t set, value_t *val)
{
		if (!rig || !rig->caps || !val)
			return -RIG_EINVAL;

		if (rig->caps->get_level == NULL)
			return -RIG_ENAVAIL;	/* not implemented */
		else
			return rig->caps->get_level(rig, set, val);
}

/*
 * "macro" to check if a rig can *get* a level setting,
 * example:  if (rig_has_set_level(my_rig, RIG_LVL_STRENGTH)) disp_Smeter();
 */
int rig_has_level(RIG *rig, setting_t level)
{
		if (!rig || !rig->caps)
				return -1;

		return (rig->caps->has_level & level);
}


/*
 * "macro" to check if a rig can *set* a level setting,
 * example:  if (rig_has_set_level(my_rig, RIG_LVL_RFPOWER)) crank_tx();
 */
int rig_has_set_level(RIG *rig, setting_t level)
{
		if (!rig || !rig->caps)
				return -1;

		return (rig->caps->has_set_level & level);
}



/*
 * "macro" to check if a rig has a function,
 * example:  if (rig_has_func(my_rig, RIG_FUNC_FAGC)) disp_fagc_button();
 */
int rig_has_func(RIG *rig, setting_t func)
{
		if (!rig || !rig->caps)
				return -1;

		return (rig->caps->has_func & func);
}

/*
 * rig_set_func
 */

int rig_set_func(RIG *rig, setting_t func)
{
		if (!rig || !rig->caps)
			return -RIG_EINVAL;

		if (rig->caps->set_func == NULL)
			return -RIG_ENAVAIL;	/* not implemented */
		else
			return rig->caps->set_func(rig, func);
}

/*
 * rig_get_func
 * Query the setting of function bits set to 1
 * and return their status.
 */

int rig_get_func(RIG *rig, setting_t *func)
{
		if (!rig || !rig->caps || !func)
			return -RIG_EINVAL;

		if (rig->caps->get_func == NULL)
			return -RIG_ENAVAIL;	/* not implemented */
		else
			return rig->caps->get_func(rig, func);
}

/*
 * rig_set_mem
 * Set memory channel number.
 * Should not be obliged to be in memory mode. Depends on rig. YMMV.
 */

int rig_set_mem(RIG *rig, int ch)
{
		if (!rig || !rig->caps)
			return -RIG_EINVAL;

		if (rig->caps->set_mem == NULL)
			return -RIG_ENAVAIL;	/* not implemented */
		else
			return rig->caps->set_mem(rig, ch);
}

/*
 * rig_get_mem
 * Get memory channel number
 * Should not be obliged to be in memory mode. Depends on rig. YMMV.
 */

int rig_get_mem(RIG *rig, int *ch)
{
		if (!rig || !rig->caps || !ch)
			return -RIG_EINVAL;

		if (rig->caps->get_mem == NULL)
			return -RIG_ENAVAIL;	/* not implemented */
		else
			return rig->caps->get_mem(rig, ch);
}

/*
 * rig_mv_ctl
 * Memory/VFO operations, cf mv_op_t enum
 */

int rig_mv_ctl(RIG *rig, mv_op_t op)
{
		if (!rig || !rig->caps)
			return -RIG_EINVAL;

		if (rig->caps->mv_ctl == NULL)
			return -RIG_ENAVAIL;	/* not implemented */
		else
			return rig->caps->mv_ctl(rig, op);
}

/*
 * rig_set_channel
 */

int rig_set_channel(RIG *rig, const channel_t *chan)
{
		if (!rig || !rig->caps || !chan)
			return -RIG_EINVAL;

		if (rig->caps->set_channel == NULL)
			return -RIG_ENAVAIL;	/* not implemented */
		else
			return rig->caps->set_channel(rig, chan);
}

/*
 * rig_get_channel
 * The channel to get data about is stored in chan->channel_num
 */

int rig_get_channel(RIG *rig, channel_t *chan)
{
		if (!rig || !rig->caps || !chan)
			return -RIG_EINVAL;

		if (rig->caps->get_channel == NULL)
			return -RIG_ENAVAIL;	/* not implemented */
		else
			return rig->caps->get_channel(rig, chan);
}

/*
 * rig_get_range returns a pointer to the freq_range_t
 * 	concerning the freq/mode args.
 * Works for rx and tx range list as well
 */
const freq_range_t *
rig_get_range(const freq_range_t range_list[], freq_t freq, unsigned long mode)
{
	int i;

	for (i=0; i<FRQRANGESIZ; i++) {
		if (range_list[i].start == 0 && range_list[i].end == 0) {
				return NULL;
		}
		if ((freq >= range_list[i].start && freq <= range_list[i].end) &&
				(range_list[i].modes & mode)) {
				return (&range_list[i]);
		}
	}
	return NULL;
}


/*
 * rig_set_trn
 * Enable/disable the transceive handling of a rig
 *  and kick off async mode
 */

int rig_set_trn(RIG *rig, int trn)
{
		int status;

		if (!rig || !rig->caps)
			return -RIG_EINVAL;

		if (rig->caps->transceive == RIG_TRN_OFF)
			return -RIG_ENAVAIL;

		if (trn == RIG_TRN_ON) {
			if (rig->state.transceive) {
				/*
				 * TODO: check error codes et al.
				 */
				status = add_trn_rig(rig);
				if (rig->caps->set_trn)
						return rig->caps->set_trn(rig, RIG_TRN_ON);
				else
						return status;
			} else {
				return -RIG_ECONF;
			}
		} else {
				status = remove_trn_rig(rig);
				if (rig->caps->set_trn)
						return rig->caps->set_trn(rig, RIG_TRN_OFF);
				else
						return status;
		}

		return RIG_OK;
}

/*
 * rig_get_trn
 * Check if radio sends status automatically when change sent
 */

int rig_get_trn(RIG *rig, int *trn)
{
		if (!rig || !rig->caps || !trn)
			return -RIG_EINVAL;

		if (rig->caps->get_trn == NULL)
			return -RIG_ENAVAIL;	/* not implemented */
		else
			return rig->caps->get_trn(rig, trn);
}



