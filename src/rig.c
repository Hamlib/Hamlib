/* hamlib - Ham Radio Control Libraries
   Copyright (C) 2000 Stephane Fillod and Frank Singleton
   This file is part of the hamlib package.

   $Id: rig.c,v 1.6 2000-10-22 16:08:23 f4cfe Exp $

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

/**
 *      rigerror - return string describing error code
 *      @errnum:   The error code
 *
 * 		The rigerror() function returns a string describing the
 *      error code passed in the argument @errnum.
 *
 *		RETURN VALUE: The  rigerror() function returns the appropriate
 *		description string, or an unknown error message if the error 
 *		code is unknown.
 *
 *      TODO: check table bounds, support gettext
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

/**
 *      foreach_opened_rig - execs cfunc() on each opened rig
 *      @cfunc:	The function to be executed on each rig
 *      @data:	Data pointer to be passed to cfunc() 
 *
 *      The foreach_opened_rig() function calls cfunc() function
 *      for each opened rig. The contents of the opened rig table
 *      is processed in random order according to a function
 *      pointed to by @cfunc, whic is called with two arguments,
 *      the first pointing to the &RIG handle, the second
 *      to a data pointer @data.
 *      If @data is not needed, then it can be set to %NULL.
 *      The processing of the opened rig table is stopped
 *      when cfunc() returns 0.
 *
 *      RETURN VALUE: The foreach_opened_rig() function always returns
 *		%RIG_OK.
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

/**
 *      rig_init - allocate a new &RIG handle
 *      @rig_model:	The rig model for this new handle
 *      @data:	Data pointer to be passed to cfunc() 
 *
 *      The rig_init() function allocates a new &RIG handle and
 *      initialize the associated data for @rig_model.
 *
 *      RETURN VALUE: The rig_init() function returns a pointer
 *      to the &RIG handle or %NULL if memory allocation failed
 *      or @rig_model is unknown.
 *
 *      SEE ALSO: rig_cleanup(), rig_open()
 */

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

/**
 *      rig_open - open the communication to the rig
 *      @rig:	The &RIG handle of the radio to be opened
 *
 *      The rig_open() function opens communication to a radio
 *      which &RIG handle has been passed by argument.
 *
 *      RETURN VALUE: The rig_open() function returns %RIG_OK
 *      if the operation has been sucessful, or a negative value
 *      if an error occured (in which case, cause is set appropriately).
 *
 *      ERRORS: %RIG_EINVAL	@rig is %NULL or unconsistent.
 *      		%RIG_ENIMPL	port type communication is not implemented yet.
 *
 *      SEE ALSO: rig_close()
 */

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


/*
 * Examples of typical rig_* wrapper
 */

/**
 *      rig_set_freq - set the frequency of the current VFO
 *      @rig:	The rig handle
 *      @freq:	The frequency to set to
 *
 *      The rig_set_freq() function sets the frequency of the current VFO.
 *
 *      RETURN VALUE: The rig_set_freq() function returns %RIG_OK
 *      if the operation has been sucessful, or a negative value
 *      if an error occured (in which case, cause is set appropriately).
 *
 *      SEE ALSO: rig_get_freq()
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

/**
 *      rig_get_freq - get the frequency of the current VFO
 *      @rig:	The rig handle
 *      @freq:	The location where to store the current frequency
 *
 *      The rig_get_freq() function retrieves the frequency of the current VFO.
 *
 *      RETURN VALUE: The rig_get_freq() function returns %RIG_OK
 *      if the operation has been sucessful, or a negative value
 *      if an error occured (in which case, cause is set appropriately).
 *
 *      SEE ALSO: rig_set_freq()
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


/**
 *      rig_set_mode - set the mode of the current VFO
 *      @rig:	The rig handle
 *      @mode:	The mode to set to
 *
 *      The rig_set_mode() function sets the mode of the current VFO.
 *
 *      RETURN VALUE: The rig_set_mode() function returns %RIG_OK
 *      if the operation has been sucessful, or a negative value
 *      if an error occured (in which case, cause is set appropriately).
 *
 *      SEE ALSO: rig_get_mode()
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

/**
 *      rig_get_mode - get the mode of the current VFO
 *      @rig:	The rig handle
 *      @mode:	The location where to store the current mode
 *
 *      The rig_set_mode() function retrieves the mode of the current VFO.
 *
 *      RETURN VALUE: The rig_get_mode() function returns %RIG_OK
 *      if the operation has been sucessful, or a negative value
 *      if an error occured (in which case, cause is set appropriately).
 *
 *      SEE ALSO: rig_set_mode()
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

/**
 *      rig_set_vfo - set the current VFO
 *      @rig:	The rig handle
 *      @vfo:	The VFO to set to
 *
 *      The rig_set_vfo() function sets the current VFO.
 *
 *      RETURN VALUE: The rig_set_vfo() function returns %RIG_OK
 *      if the operation has been sucessful, or a negative value
 *      if an error occured (in which case, cause is set appropriately).
 *
 *      SEE ALSO: rig_get_vfo()
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

/**
 *      rig_get_vfo - get the current VFO
 *      @rig:	The rig handle
 *      @vfo:	The location where to store the current VFO
 *
 *      The rig_get_vfo() function retrieves the current VFO.
 *
 *      RETURN VALUE: The rig_get_vfo() function returns %RIG_OK
 *      if the operation has been sucessful, or a negative value
 *      if an error occured (in which case, cause is set appropriately).
 *
 *      SEE ALSO: rig_set_vfo()
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

/**
 *      rig_set_ptt - set PTT on/off
 *      @rig:	The rig handle
 *      @ptt:	The PTT status to set to
 *
 *      The rig_set_ptt() function sets "Push-To-Talk" on/off.
 *
 *      RETURN VALUE: The rig_set_ptt() function returns %RIG_OK
 *      if the operation has been sucessful, or a negative value
 *      if an error occured (in which case, cause is set appropriately).
 *
 *      SEE ALSO: rig_get_ptt()
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

/**
 *      rig_get_ptt - get the status of the PTT
 *      @rig:	The rig handle
 *      @ptt:	The location where to store the status of the PTT
 *
 *      The rig_get_ptt() function retrieves the status of PTT (are we
 *      on the air?).
 *
 *      RETURN VALUE: The rig_get_ptt() function returns %RIG_OK
 *      if the operation has been sucessful, or a negative value
 *      if an error occured (in which case, cause is set appropriately).
 *
 *      SEE ALSO: rig_set_ptt()
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


/**
 *      rig_set_rptr_shift - set the repeater shift
 *      @rig:	The rig handle
 *      @rptr_shift:	The repeater shift to set to
 *
 *      The rig_set_rptr_shift() function sets the current repeater shift.
 *
 *      RETURN VALUE: The rig_rptr_shift() function returns %RIG_OK
 *      if the operation has been sucessful, or a negative value
 *      if an error occured (in which case, cause is set appropriately).
 *
 *      SEE ALSO: rig_get_rptr_shift()
 */
int rig_set_rptr_shift(RIG *rig, rptr_shift_t rptr_shift)
{
		if (!rig || !rig->caps)
			return -RIG_EINVAL;

		if (rig->caps->set_rptr_shift == NULL)
			return -RIG_ENAVAIL;	/* not implemented */
		else
			return rig->caps->set_rptr_shift(rig, rptr_shift);
}

/**
 *      rig_get_rptr_shift - get the current repeater shift
 *      @rig:	The rig handle
 *      @rptr_shift:	The location where to store the current repeater shift
 *
 *      The rig_get_rptr_shift() function retrieves the current repeater shift.
 *
 *      RETURN VALUE: The rig_get_rptr_shift() function returns %RIG_OK
 *      if the operation has been sucessful, or a negative value
 *      if an error occured (in which case, cause is set appropriately).
 *
 *      SEE ALSO: rig_set_rptr_shift()
 */
int rig_get_rptr_shift(RIG *rig, rptr_shift_t *rptr_shift)
{
		if (!rig || !rig->caps || !rptr_shift)
			return -RIG_EINVAL;

		if (rig->caps->get_rptr_shift == NULL)
			return -RIG_ENAVAIL;	/* not implemented */
		else
			return rig->caps->get_rptr_shift(rig, rptr_shift);
}

/**
 *      rig_set_rptr_offs - set the repeater offset
 *      @rig:	The rig handle
 *      @rptr_offs:	The VFO to set to
 *
 *      The rig_set_rptr_offs() function sets the current repeater offset.
 *
 *      RETURN VALUE: The rig_set_rptr_offs() function returns %RIG_OK
 *      if the operation has been sucessful, or a negative value
 *      if an error occured (in which case, cause is set appropriately).
 *
 *      SEE ALSO: rig_get_rptr_offs()
 */

int rig_set_rptr_offs(RIG *rig, unsigned long rptr_offs)
{
		if (!rig || !rig->caps)
			return -RIG_EINVAL;

		if (rig->caps->set_rptr_offs == NULL)
			return -RIG_ENAVAIL;	/* not implemented */
		else
			return rig->caps->set_rptr_offs(rig, rptr_offs);
}

/**
 *      rig_get_rptr_offs - get the current repeater offset
 *      @rig:	The rig handle
 *      @rptr_offs:	The location where to store the current repeater offset
 *
 *      The rig_get_rptr_offs() function retrieves the current repeater offset.
 *
 *      RETURN VALUE: The rig_get_rptr_offs() function returns %RIG_OK
 *      if the operation has been sucessful, or a negative value
 *      if an error occured (in which case, cause is set appropriately).
 *
 *      SEE ALSO: rig_set_rptr_offs()
 */

int rig_get_rptr_offs(RIG *rig, unsigned long *rptr_offs)
{
		if (!rig || !rig->caps || !rptr_offs)
			return -RIG_EINVAL;

		if (rig->caps->get_rptr_offs == NULL)
			return -RIG_ENAVAIL;	/* not implemented */
		else
			return rig->caps->get_rptr_offs(rig, rptr_offs);
}


/**
 *      rig_set_split_freq - set the split frequencies
 *      @rig:	The rig handle
 *      @rx_freq:	The receive split frequency to set to
 *      @tx_freq:	The transmit split frequency to set to
 *
 *      The rig_set_split_freq() function sets the split frequencies.
 *
 *      RETURN VALUE: The rig_set_split_freq() function returns %RIG_OK
 *      if the operation has been sucessful, or a negative value
 *      if an error occured (in which case, cause is set appropriately).
 *
 *      SEE ALSO: rig_get_split_freq()
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

/**
 *      rig_get_split_freq - get the current split frequencies
 *      @rig:	The rig handle
 *      @rx_freq:	The location where to store the current receive split frequency
 *      @tx_freq:	The location where to store the current receive split frequency
 *
 *      The rig_get_split_freq() function retrieves the current split
 *      frequencies.
 *
 *      RETURN VALUE: The rig_get_split_freq() function returns %RIG_OK
 *      if the operation has been sucessful, or a negative value
 *      if an error occured (in which case, cause is set appropriately).
 *
 *      SEE ALSO: rig_set_split_freq()
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



/**
 *      rig_set_split - set the split mode
 *      @rig:	The rig handle
 *      @split:	The split mode to set to
 *
 *      The rig_set_split() function sets the current split mode.
 *
 *      RETURN VALUE: The rig_set_split() function returns %RIG_OK
 *      if the operation has been sucessful, or a negative value
 *      if an error occured (in which case, cause is set appropriately).
 *
 *      SEE ALSO: rig_get_split()
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

/**
 *      rig_get_split - get the current split mode
 *      @rig:	The rig handle
 *      @split:	The location where to store the current split mode
 *
 *      The rig_get_split() function retrieves the current split mode.
 *
 *      RETURN VALUE: The rig_get_split() function returns %RIG_OK
 *      if the operation has been sucessful, or a negative value
 *      if an error occured (in which case, cause is set appropriately).
 *
 *      SEE ALSO: rig_set_split()
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


/**
 *      rig_set_ts - set the Tuning Step
 *      @rig:	The rig handle
 *      @ts:	The tuning step to set to
 *
 *      The rig_set_rs() function sets the Tuning Step.
 *
 *      RETURN VALUE: The rig_set_ts() function returns %RIG_OK
 *      if the operation has been sucessful, or a negative value
 *      if an error occured (in which case, cause is set appropriately).
 *
 *      SEE ALSO: rig_get_ts()
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

/**
 *      rig_get_ts - get the current Tuning Step
 *      @rig:	The rig handle
 *      @ts:	The location where to store the current tuning step
 *
 *      The rig_get_ts() function retrieves the current tuning step.
 *
 *      RETURN VALUE: The rig_get_ts() function returns %RIG_OK
 *      if the operation has been sucessful, or a negative value
 *      if an error occured (in which case, cause is set appropriately).
 *
 *      SEE ALSO: rig_set_ts()
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

/**
 *      rig_power2mW - conversion utility from relative range to absolute in mW
 *      @rig:	The rig handle
 *      @mwpower:	The location where to store the converted power in mW
 *      @power:	The relative power
 *      @freq:	The frequency where the conversion should take place
 *      @mode:	The mode where the conversion should take place
 *
 *      The rig_power2mW() function converts a power value expressed in
 *      a range on a [0.0 .. 1.0] relative scale to the real transmit power
 *      in milli Watts the radio would emit.
 *      The @freq and @mode where the conversion should take place must be
 *      also provided since the relative power is peculiar to a specific
 *      freq and mode range of the radio.
 *
 *      RETURN VALUE: The rig_power2mW() function returns %RIG_OK
 *      if the operation has been sucessful, or a negative value
 *      if an error occured (in which case, cause is set appropriately).
 *
 *      SEE ALSO: rig_mW2power()
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

/**
 *      rig_mW2power - conversion utility from absolute in mW to relative range
 *      @rig:	The rig handle
 *      @power:	The location where to store the converted relative power
 *      @mwpower:	The power in mW
 *      @freq:	The frequency where the conversion should take place
 *      @mode:	The mode where the conversion should take place
 *
 *      The rig_mW2power() function converts a power value expressed in
 *      the real transmit power in milli Watts the radio would emit to
 *      a range on a [0.0 .. 1.0] relative scale.
 *      The @freq and @mode where the conversion should take place must be
 *      also provided since the relative power is peculiar to a specific
 *      freq and mode range of the radio.
 *
 *      RETURN VALUE: The rig_mW2power() function returns %RIG_OK
 *      if the operation has been sucessful, or a negative value
 *      if an error occured (in which case, cause is set appropriately).
 *
 *      SEE ALSO: rig_power2mW()
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

/**
 *      rig_set_ctcss - set CTCSS
 *      @rig:	The rig handle
 *      @tone:	The tone to set to
 *
 *      The rig_set_ctcss() function sets the current squelch tone.
 *      NB, @tone is NOT in Hz, but in tenth of Hz! This way,
 *      if you want to set subaudible tone of 88.5 Hz for example,
 *      then pass 885 to this function. Also, to disable Tone squelch,
 *      set @tone to 0.
 *
 *      RETURN VALUE: The rig_set_ctcss() function returns %RIG_OK
 *      if the operation has been sucessful, or a negative value
 *      if an error occured (in which case, cause is set appropriately).
 *
 *      SEE ALSO: rig_get_ctcss(), rig_set_dcs(), rig_get_dcs()
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
/**
 *      rig_get_ctcss - get the current CTCSS
 *      @rig:	The rig handle
 *      @tone:	The location where to store the current tone
 *
 *      The rig_get_ctcss() function retrieves the current 
 *      squelch tone.
 *      NB, @tone is NOT in Hz, but in tenth of Hz! This way,
 *      if the function rig_get_ctcss() returns a subaudible tone of 885
 *      for example, then the real tone is 88.5 Hz. 
 *      Also, a value of 0 for @tone means the Tone squelch is disabled.
 *
 *      RETURN VALUE: The rig_get_ctcss() function returns %RIG_OK
 *      if the operation has been sucessful, or a negative value
 *      if an error occured (in which case, cause is set appropriately).
 *
 *      SEE ALSO: rig_set_ctcss(), rig_set_dcs(), rig_get_dcs()
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

/**
 *      rig_set_dcs - set the current DCS
 *      @rig:	The rig handle
 *      @tone:	The tone to set to
 *
 *      The rig_set_dcs() function sets the current subaudible tone to 
 *      access a repeater or tone squelched frequency.
 *      NB, @tone is NOT in Hz, but in tenth of Hz! This way,
 *      if you want to set subaudible tone of 88.5 Hz for example,
 *      then pass 885 to this function. Also, to disable Tone squelch,
 *      set @tone to 0.
 *
 *      RETURN VALUE: The rig_set_dcs() function returns %RIG_OK
 *      if the operation has been sucessful, or a negative value
 *      if an error occured (in which case, cause is set appropriately).
 *
 *      SEE ALSO: rig_get_dcs(), rig_set_ctcss(), rig_get_ctcss()
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

/**
 *      rig_get_dcs - get the current DCS
 *      @rig:	The rig handle
 *      @tone:	The location where to store the current tone
 *
 *      The rig_get_dcs() function retrieves the current DCS. 
 *      NB, @tone is NOT in Hz, but in tenth of Hz! This way,
 *      if the function rig_get_dcs() returns a subaudible tone of 885
 *      for example, then the real tone is 88.5 Hz. 
 *      Also, a value of 0 for @tone means the Tone encoding is disabled.
 *
 *      RETURN VALUE: The rig_get_dcs() function returns %RIG_OK
 *      if the operation has been sucessful, or a negative value
 *      if an error occured (in which case, cause is set appropriately).
 *
 *      SEE ALSO: rig_get_dcs(), rig_set_ctcss(), rig_get_ctcss()
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

/**
 *      rig_set_poweron - turn on the radio
 *      @rig:	The rig handle
 *
 *      The rig_set_poweron() function turns on the radio.
 *
 *      RETURN VALUE: The rig_set_poweron() function returns %RIG_OK
 *      if the operation has been sucessful, or a negative value
 *      if an error occured (in which case, cause is set appropriately).
 *
 *      SEE ALSO: rig_set_poweroff()
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

/**
 *      rig_set_poweroff - turn off the radio
 *      @rig:	The rig handle
 *
 *      The rig_set_poweroff() function turns off the radio.
 *
 *      RETURN VALUE: The rig_set_poweroff() function returns %RIG_OK
 *      if the operation has been sucessful, or a negative value
 *      if an error occured (in which case, cause is set appropriately).
 *
 *      SEE ALSO: rig_set_poweron()
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



#if 0

/* CAUTION: this is really Experimental, It never worked!!
 * try to guess a rig, can be very buggy! (but fun if it works!)
 * FIXME: finish me and handle nicely errors
 * TODO: rewrite rig_probe, it's the wrong approach.
 * 		 Each rig backend should provide a rigbe_probe function
 * 		 so with help of rig_list_foreach we can probe them all. --SF
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

/**
 *      rig_set_level - set a radio level setting
 *      @rig:	The rig handle
 *      @level:	The level setting
 *      @val:	The value to set the level setting to
 *
 *      The rig_set_level() function sets the level of a setting.
 *      The level value @val can be a float or an integer. See &value_t
 *      for more information.
 *
 *      RETURN VALUE: The rig_set_level() function returns %RIG_OK
 *      if the operation has been sucessful, or a negative value
 *      if an error occured (in which case, cause is set appropriately).
 *
 *      SEE ALSO: rig_has_set_level(), rig_get_level()
 */
int rig_set_level(RIG *rig, setting_t level, value_t val)
{
		if (!rig || !rig->caps)
			return -RIG_EINVAL;

		if (rig->caps->set_level == NULL)
			return -RIG_ENAVAIL;	/* not implemented */
		else
			return rig->caps->set_level(rig, level, val);
}

/**
 *      rig_get_level - get the level of a level
 *      @rig:	The rig handle
 *      @level:	The level setting
 *      @val:	The location where to store the value of @level
 *
 *      The rig_get_level() function retrieves the value of a @level.
 *      The level value @val can be a float or an integer. See &value_t
 *      for more information.
 *
 *      RETURN VALUE: The rig_get_level() function returns %RIG_OK
 *      if the operation has been sucessful, or a negative value
 *      if an error occured (in which case, cause is set appropriately).
 *
 *      SEE ALSO: rig_has_level(), rig_set_level()
 */
int rig_get_level(RIG *rig, setting_t level, value_t *val)
{
		if (!rig || !rig->caps || !val)
			return -RIG_EINVAL;

		if (rig->caps->get_level == NULL)
			return -RIG_ENAVAIL;	/* not implemented */
		else
			return rig->caps->get_level(rig, set, val);
}

/**
 *      rig_has_level - check retrieval ability of level settings
 *      @rig:	The rig handle
 *      @level:	The level settings
 *
 *      The rig_has_level() "macro" checks if a rig can *get* a level setting.
 *      Since the @level is a OR'ed bitwise argument, more than
 *      one level can be checked at the same time.
 *
 *      RETURN VALUE: The rig_has_level() "macro" returns a bit wise
 *      mask of supported level settings that can be retrieve,
 *      0 if none supported.
 *
 * 		EXAMPLE: if (rig_has_level(my_rig, RIG_LVL_STRENGTH)) disp_Smeter();
 *
 *      SEE ALSO: rig_has_set_level(), rig_get_level()
 */
setting_t rig_has_level(RIG *rig, setting_t level)
{
		if (!rig || !rig->caps)
				return -1;

		return (rig->caps->has_level & level);
}


/**
 *      rig_has_set_level - check settable ability of level settings
 *      @rig:	The rig handle
 *      @level:	The level settings
 *
 *      The rig_has_set_level() "macro" checks if a rig can *set* a level
 *      setting. Since the @level is a OR'ed bitwise argument, more than
 *      one level can be check at the same time.
 *
 *      RETURN VALUE: The rig_has_set_level() "macro" returns a bit wise
 *      mask of supported level settings that can be set,
 *      0 if none supported.
 *
 * 		EXAMPLE: if (rig_has_set_level(my_rig, RIG_LVL_RFPOWER)) crank_tx();
 *
 *      SEE ALSO: rig_has_level(), rig_set_level()
 */
setting_t rig_has_set_level(RIG *rig, setting_t level)
{
		if (!rig || !rig->caps)
				return -1;

		return (rig->caps->has_set_level & level);
}


/**
 *      rig_has_func - check ability of radio functions
 *      @rig:	The rig handle
 *      @func:	The functions
 *
 *      The rig_has_func() "macro" checks if a rig supports a set of functions.
 *      Since the @func is a OR'ed bitwise argument, more than
 *      one function can be checked at the same time.
 *
 *      RETURN VALUE: The rig_has_func() "macro" returns a bit wise
 *      mask of supported functions, 0 if none supported.
 *
 * 		EXAMPLE: if (rig_has_func(my_rig, RIG_FUNC_FAGC)) disp_fagc_button();
 *
 *      SEE ALSO: rig_set_func(), rig_get_func()
 */
setting_t rig_has_func(RIG *rig, setting_t func)
{
		if (!rig || !rig->caps)
				return -1;

		return (rig->caps->has_func & func);
}

/**
 *      rig_set_func - activate/desactivate functions of radio
 *      @rig:	The rig handle
 *      @func:	The functions to activate
 *      @status:	The status (on or off) to set to
 *
 *      The rig_set_func() function activate/desactivate functions of the radio.
 *
 *      RETURN VALUE: The rig_set_func() function returns %RIG_OK
 *      if the operation has been sucessful, or a negative value
 *      if an error occured (in which case, cause is set appropriately).
 *
 *      SEE ALSO: rig_get_func()
 */

int rig_set_func(RIG *rig, setting_t func, int status)
{
		if (!rig || !rig->caps)
			return -RIG_EINVAL;

		if (rig->caps->set_func == NULL)
			return -RIG_ENAVAIL;	/* not implemented */
		else
			return rig->caps->set_func(rig, func, status);
}

/**
 *      rig_get_func - get the status of functions of the radio
 *      @rig:	The rig handle
 *      @func:	The location where to store the function status
 *
 *      The rig_get_func() function retrieves the status of functions
 *      of the radio. Only the function bits set to 1 will be queried.
 *      On return, @func will hold the status of funtions (bit set to 1 =
 *      activated, bit set to 0 = desactivated).
 *
 *      RETURN VALUE: The rig_get_func() function returns %RIG_OK
 *      if the operation has been sucessful, or a negative value
 *      if an error occured (in which case, cause is set appropriately).
 *
 *      SEE ALSO: rig_set_func()
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

/**
 *      rig_set_mem - set the current memory channel number
 *      @rig:	The rig handle
 *      @ch:	The memory channel number
 *
 *      The rig_set_mem() function sets the current memory channel number.
 *      It is not mandatory for the radio to be in memory mode. Actually
 *      it depends on rigs. YMMV.
 *
 *      RETURN VALUE: The rig_set_mem() function returns %RIG_OK
 *      if the operation has been sucessful, or a negative value
 *      if an error occured (in which case, cause is set appropriately).
 *
 *      SEE ALSO: rig_get_mem()
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

/**
 *      rig_get_mem - get the current memory channel number
 *      @rig:	The rig handle
 *      @ch:	The location where to store the current memory channel number
 *
 *      The rig_get_mem() function retrieves the current memory channel number.
 *      It is not mandatory for the radio to be in memory mode. Actually
 *      it depends on rigs. YMMV.
 *
 *      RETURN VALUE: The rig_get_mem() function returns %RIG_OK
 *      if the operation has been sucessful, or a negative value
 *      if an error occured (in which case, cause is set appropriately).
 *
 *      SEE ALSO: rig_set_mem()
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

/**
 *      rig_mv_ctl - perform Memory/VFO operations
 *      @rig:	The rig handle
 *      @op:	The Memory/VFO operation to perform
 *
 *      The rig_mv_ctl() function performs Memory/VFO operation.
 *      See &mv_op_t for more information.
 *
 *      RETURN VALUE: The rig_mv_ctl() function returns %RIG_OK
 *      if the operation has been sucessful, or a negative value
 *      if an error occured (in which case, cause is set appropriately).
 *
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

/**
 *      rig_set_channel - set channel data
 *      @rig:	The rig handle
 *      @chan:	The location of data to set for this channel
 *
 *      The rig_set_channel() function sets the data associated 
 *      with a channel. See &channel_t for more information.
 *
 *      RETURN VALUE: The rig_set_channel() function returns %RIG_OK
 *      if the operation has been sucessful, or a negative value
 *      if an error occured (in which case, cause is set appropriately).
 *
 *      SEE ALSO: rig_get_channel()
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

/**
 *      rig_get_channel - get channel data
 *      @rig:	The rig handle
 *      @chan:	The location where to store the channel data
 *
 *      The rig_get_channel() function retrieves the data associated 
 *      with the channel @chan->channel_num. 
 *      See &channel_t for more information.
 *
 *      RETURN VALUE: The rig_get_channel() function returns %RIG_OK
 *      if the operation has been sucessful, or a negative value
 *      if an error occured (in which case, cause is set appropriately).
 *
 *      SEE ALSO: rig_set_channel()
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

/**
 *      rig_get_range - find the freq_range of freq/mode
 *      @range_list:	The range list to search from
 *      @freq:	The frequency that will be part of this range
 *      @mode:	The mode that will be part of this range
 *
 *      The rig_get_range() function returns the location of the &freq_range_t
 *      including @freq and @mode.
 *      Works for rx and tx range list as well.
 *
 *      RETURN VALUE: The rig_get_range() function returns the location
 *      of the &freq_range_t if found, %NULL if not found or if @range_list
 *      is invalid.
 *
 */
const freq_range_t *
rig_get_range(const freq_range_t range_list[], freq_t freq, rmode_t mode)
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


/**
 *      rig_set_trn - control the transceive mode 
 *      @rig:	The rig handle
 *      @trn:	The transceive status to set to
 *
 *      The rig_set_trn() function enable/disable the transceive
 *      handling of a rig and kick off async mode.
 *
 *      RETURN VALUE: The rig_set_trn() function returns %RIG_OK
 *      if the operation has been sucessful, or a negative value
 *      if an error occured (in which case, cause is set appropriately).
 *
 *      SEE ALSO: rig_get_trn()
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

/**
 *      rig_get_trn - get the current transceive mode
 *      @rig:	The rig handle
 *      @trn:	The location where to store the current transceive mode
 *
 *      The rig_get_trn() function retrieves the current status
 *      of the transceive mode, ie. if radio sends new status automatically 
 *      when some changes happened on the radio.
 *
 *      RETURN VALUE: The rig_get_trn() function returns %RIG_OK
 *      if the operation has been sucessful, or a negative value
 *      if an error occured (in which case, cause is set appropriately).
 *
 *      SEE ALSO: rig_set_trn()
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


/*
 * more rig_* to come -- FS
 *
 */



