/* hamlib - Ham Radio Control Libraries
   Copyright (C) 2000,2001 Stephane Fillod and Frank Singleton
   This file is part of the hamlib package.

   $Id: rig.c,v 1.29 2001-06-02 18:05:14 f4cfe Exp $

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

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <stdio.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>


#include <hamlib/rig.h>
#include <hamlib/riglist.h>
#include <serial.h>
#include "event.h"

/*
 * Hamlib version. Should we include copyright here too?
 */
const char hamlib_version[] = "Hamlib version " VERSION;


#define DEFAULT_SERIAL_PORT "/dev/ttyS0"

/*
 * 52 CTCSS sub-audible tones
 */
const tone_t full_ctcss_list[] = {
		600,  670,  693,  719,  744,  770,  797,  825,  854,  885,  915,
		948,  974, 1000, 1035, 1072, 1109, 1148, 1188, 1200,  1230, 1273,
		1318, 1365, 1413, 1462, 1514, 1567, 1598, 1622, 1655, 1679,
		1713, 1738, 1773, 1799, 1835, 1862, 1899, 1928, 1966, 1995,
		2035, 2065, 2107, 2181, 2257, 2291, 2336, 2418, 2503, 2541,
		0,
};

/*
 * 50 CTCSS sub-audible tones, from 67.0Hz to 254.1Hz
 * Don't even think about changing a bit of this array, several
 * backends depend on it. If you need to, create a copy for your 
 * own caps. --SF
 */
const tone_t common_ctcss_list[] = {
		670,  693,  719,  744,  770,  797,  825,  854,  885,  915,
		948,  974, 1000, 1035, 1072, 1109, 1148, 1188,  1230, 1273,
		1318, 1365, 1413, 1462, 1514, 1567, 1598, 1622, 1655, 1679,
		1713, 1738, 1773, 1799, 1835, 1862, 1899, 1928, 1966, 1995,
		2035, 2065, 2107, 2181, 2257, 2291, 2336, 2418, 2503, 2541,
		0,
};

/*
 * 106 DCS codes
 */
const tone_t full_dcs_list[] = {
		017, 023, 025, 026, 031, 032, 036, 043, 047, 050, 051, 053, 
		054, 065, 071, 072, 073, 074, 114, 115, 116, 122, 125, 131, 
		132, 134, 143, 145, 152, 155, 156, 162, 165, 172, 174, 205, 
		212, 223, 225, 226, 243, 244, 245, 246, 251, 252, 255, 261, 
		263, 265, 266, 271, 274, 306, 311, 315, 325, 331, 332, 343, 
		346, 351, 356, 364, 365, 371, 411, 412, 413, 423, 431, 432, 
		445, 446, 452, 454, 455, 462, 464, 465, 466, 503, 506, 516, 
		523, 526, 532, 546, 565, 606, 612, 624, 627, 631, 632, 654, 
		662, 664, 703, 712, 723, 731, 732, 734, 743, 754, 
		0,
};
	
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
		"Target VFO unaccessible",
		NULL,
};
#define ERROR_TBL_SZ (sizeof(rigerror_table)/sizeof(char *))

/**
 *      rigerror - return string describing error code
 *      @errnum:   The error code
 *
 * 		The rigerror() function returns a string describing the
 *      error code passed in the argument @errnum.
 *
 *		RETURN VALUE: The  rigerror() function returns the appropriate
 *		description string, or a NULL pointer if the error 
 *		code is unknown.
 *
 *      TODO: check table bounds, support gettext
 */
const char *rigerror(int errnum)
{
		errnum = abs(errnum);
		if (errnum > ERROR_TBL_SZ)
			return NULL;
		return rigerror_table[errnum];
}

/*
 * track which rig is opened (with rig_open)
 * needed at least for transceive mode
 */
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
		struct rig_state *rs;
		int i;

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

		rs = &rig->state;

		rs->port_type = caps->port_type; /* default from caps */
		strncpy(rs->rig_path, DEFAULT_SERIAL_PORT, FILPATHLEN);
		rs->serial_rate = caps->serial_rate_max;	/* fastest ! */
		rs->serial_data_bits = caps->serial_data_bits;
		rs->serial_stop_bits = caps->serial_stop_bits;
		rs->serial_parity = caps->serial_parity;
		rs->serial_handshake = caps->serial_handshake;
		rs->write_delay = caps->write_delay;
		rs->post_write_delay = caps->post_write_delay;

		rs->timeout = caps->timeout;
		rs->retry = caps->retry;
		rs->transceive = caps->transceive;
		rs->ptt_type = caps->ptt_type;
		rs->dcd_type = caps->dcd_type;
		rs->vfo_comp = 0.0;	/* override it with preferences */
		rs->current_vfo = RIG_VFO_CURR;	/* we don't know yet! */

		/* should it be a parameter to rig_init ? --SF */
		rs->itu_region = RIG_ITU_REGION2;

		switch(rs->itu_region) {
			case RIG_ITU_REGION1:
					memcpy(rs->tx_range_list, caps->tx_range_list1, 
									sizeof(struct freq_range_list)*FRQRANGESIZ);
					memcpy(rs->rx_range_list, caps->rx_range_list1, 
									sizeof(struct freq_range_list)*FRQRANGESIZ);
					break;
			case RIG_ITU_REGION2:
			case RIG_ITU_REGION3:
			default:
					memcpy(rs->tx_range_list, caps->tx_range_list2, 
									sizeof(struct freq_range_list)*FRQRANGESIZ);
					memcpy(rs->rx_range_list, caps->rx_range_list2, 
									sizeof(struct freq_range_list)*FRQRANGESIZ);
					break;
		}

		rs->vfo_list = 0;
		for (i=0; i<FRQRANGESIZ; i++) {
				if (rs->rx_range_list[i].start != 0 &&
								rs->rx_range_list[i].end != 0)
					rs->vfo_list |= rs->rx_range_list[i].vfo;
				if (rs->tx_range_list[i].start != 0 &&
								rs->tx_range_list[i].end != 0)
					rs->vfo_list |= rs->tx_range_list[i].vfo;
		}

		memcpy(rs->preamp, caps->preamp, sizeof(int)*MAXDBLSTSIZ);
		memcpy(rs->attenuator, caps->attenuator, sizeof(int)*MAXDBLSTSIZ);
		memcpy(rs->tuning_steps, caps->tuning_steps, 
						sizeof(struct tuning_step_list)*TSLSTSIZ);
		memcpy(rs->filters, caps->filters, 
						sizeof(struct filter_list)*FLTLSTSIZ);
		memcpy(rs->chan_list, caps->chan_list, sizeof(chan_t)*CHANLSTSIZ);

		rs->has_get_func = caps->has_get_func;
		rs->has_set_func = caps->has_set_func;
		rs->has_get_level = caps->has_get_level;
		rs->has_set_level = caps->has_set_level;
		rs->has_get_parm = caps->has_get_parm;
		rs->has_set_parm = caps->has_set_parm;

		rs->max_rit = caps->max_rit;
		rs->max_xit = caps->max_xit;
		rs->max_ifshift = caps->max_ifshift;
		rs->announces = caps->announces;

		rs->fd = -1;
		rs->ptt_fd = -1;

		/* 
		 * let the backend a chance to setup his private data
		 * FIXME: check rig_init() return code
		 */
		if (caps->rig_init != NULL)
				caps->rig_init(rig);	

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
		const struct rig_caps *caps;
		int status;
		vfo_t vfo;

		rig_debug(RIG_DEBUG_VERBOSE,"rig:rig_open called \n");

		if (!rig || !rig->caps)
				return -RIG_EINVAL;

		caps = rig->caps;
		rig->state.fd = -1;

		switch(rig->state.port_type) {
		case RIG_PORT_SERIAL:
				status = serial_open(&rig->state);
				if (status != 0)
						return status;
				break;

		case RIG_PORT_DEVICE:
				status = open(rig->state.rig_path, O_RDWR, 0);
				if (status < 0)
						return -RIG_EIO;
				rig->state.fd = status;
				break;

		case RIG_PORT_NONE:
				break;	/* ez :) */

		case RIG_PORT_NETWORK:	/* not implemented yet! */
				return -RIG_ENIMPL;
		default:
				return -RIG_EINVAL;
		}

		if (rig->state.fd >= 0)
			rig->state.stream = fdopen(rig->state.fd, "r+b");

		/*
		 * FIXME: what to do if PTT open fails or PTT unsupported?
		 * 			fail rig_open?  remember unallocating..
		 */
		switch(rig->state.ptt_type) {
		case RIG_PTT_NONE:
				break;
		case RIG_PTT_SERIAL_RTS:
		case RIG_PTT_SERIAL_DTR:
				rig->state.ptt_fd = ser_ptt_open(&rig->state);
				if (rig->state.ptt_fd < 0)
					rig_debug(RIG_DEBUG_ERR, "Cannot open PTT device \"%s\"\n",
								rig->state.ptt_path);
				break;
		case RIG_PTT_PARALLEL:
				rig->state.ptt_fd = par_ptt_open(&rig->state);
				if (rig->state.ptt_fd < 0)
					rig_debug(RIG_DEBUG_ERR, "Cannot open PTT device \"%s\"\n",
								rig->state.ptt_path);
				break;
		default:
				rig_debug(RIG_DEBUG_ERR, "Unsupported PTT type %d\n",
								rig->state.ptt_type);
		}

		add_opened_rig(rig);

		/* 
		 * Maybe the backend has something to initialize
		 * FIXME: check rig_open() return code
		 */
		if (caps->rig_open != NULL)
				caps->rig_open(rig);	

		/*
		 * trigger state->current_vfo first retrieval
		 */
		if (!caps->targetable_vfo && caps->get_vfo)
				caps->get_vfo(rig, &vfo);	

		return RIG_OK;
}

/*
 * Close port
 */
int rig_close(RIG *rig)
{
		const struct rig_caps *caps;
		struct rig_state *rs;

		rig_debug(RIG_DEBUG_VERBOSE,"rig:rig_close called \n");

		if (!rig || !rig->caps)
				return -RIG_EINVAL;

		caps = rig->caps;
		rs = &rig->state;

		if (rs->transceive) {
				/*
				 * TODO: check error codes et al.
				 */
				remove_trn_rig(rig);
		}

		/*
		 * Let the backend say 73s to the rig
		 */
		if (caps->rig_close)
				caps->rig_close(rig);

		/* 
		 * FIXME: what happens if PTT and rig ports are the same?
		 * 			(eg. ptt_type = RIG_PTT_SERIAL)
		 */
		switch(rs->ptt_type) {
		case RIG_PTT_NONE:
				break;
		case RIG_PTT_SERIAL_RTS:
		case RIG_PTT_SERIAL_DTR:
				ser_ptt_close(rs);
				break;
		case RIG_PTT_PARALLEL:
				par_ptt_close(rs);
				break;
		default:
				rig_debug(RIG_DEBUG_ERR, "Unsupported PTT type %d\n",
								rs->ptt_type);
		}

		rs->ptt_fd = -1;

		if (rs->fd != -1) {
				if (!rs->stream)
						fclose(rs->stream); /* this closes also fd */
				else
					close(rs->fd);
				rs->fd = -1;
				rs->stream = NULL;
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

		if (!rig || !rig->caps)
				return -RIG_EINVAL;

		/*
		 * basically free up the priv struct 
		 */
		if (rig->caps->rig_cleanup)
				rig->caps->rig_cleanup(rig);

		free(rig);

		return RIG_OK;
}


/**
 *      rig_set_freq - set the frequency of the current VFO
 *      @rig:	The rig handle
 *      @vfo:	The target VFO
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

int rig_set_freq(RIG *rig, vfo_t vfo, freq_t freq)
{
		const struct rig_caps *caps;
		int retcode;
		vfo_t curr_vfo;

		if (!rig || !rig->caps)
			return -RIG_EINVAL;

		caps = rig->caps;

		if (rig->state.vfo_comp != 0.0)
				freq += (freq_t)((double)rig->state.vfo_comp * freq);

		if (caps->set_freq == NULL)
			return -RIG_ENAVAIL;

		if (caps->targetable_vfo || vfo == RIG_VFO_CURR ||
										vfo == rig->state.current_vfo)
			return caps->set_freq(rig, vfo, freq);

		if (!caps->set_vfo)
			return -RIG_ENTARGET;
		curr_vfo = rig->state.current_vfo;
		retcode = caps->set_vfo(rig, vfo);
		if (retcode != RIG_OK)
				return retcode;

		retcode = caps->set_freq(rig, vfo, freq);
		caps->set_vfo(rig, curr_vfo);
		return retcode;
}

/**
 *      rig_get_freq - get the frequency of the current VFO
 *      @rig:	The rig handle
 *      @vfo:	The target VFO
 *      @freq:	The location where to store the current frequency
 *
 *      The rig_get_freq() function retrieves the frequency of the current VFO.
 *	The value stored at @freq location equals %RIG_FREQ_NONE when the current
 *	frequency of the VFO is not defined (e.g. blank memory).
 *
 *      RETURN VALUE: The rig_get_freq() function returns %RIG_OK
 *      if the operation has been sucessful, or a negative value
 *      if an error occured (in which case, cause is set appropriately).
 *
 *      SEE ALSO: rig_set_freq()
 */

int rig_get_freq(RIG *rig, vfo_t vfo, freq_t *freq)
{
		const struct rig_caps *caps;
		int retcode;
		vfo_t curr_vfo;

		if (!rig || !rig->caps || !freq)
			return -RIG_EINVAL;

		caps = rig->caps;

		if (caps->get_freq == NULL)
			return -RIG_ENAVAIL;

		if (caps->targetable_vfo || vfo == RIG_VFO_CURR ||
										vfo == rig->state.current_vfo) {
			retcode = caps->get_freq(rig, vfo, freq);
		} else {
			if (!caps->set_vfo)
				return -RIG_ENAVAIL;
			curr_vfo = rig->state.current_vfo;
			retcode = caps->set_vfo(rig, vfo);
			if (retcode != RIG_OK)
					return retcode;
			retcode = caps->get_freq(rig, vfo, freq);
			caps->set_vfo(rig, curr_vfo);
		}
		/* VFO compensation */
		if (rig->state.vfo_comp != 0.0)
			*freq += (freq_t)(rig->state.vfo_comp * (*freq));
		return retcode;
}


/**
 *      rig_set_mode - set the mode of the current VFO
 *      @rig:	The rig handle
 *      @vfo:	The target VFO
 *      @mode:	The mode to set to
 *      @width:	The passband width to set to
 *
 *      The rig_set_mode() function sets the mode of the current VFO.
 *      As a begining, the backend is free to ignore the @width argument,
 *      assuming the default passband width.
 *
 *      RETURN VALUE: The rig_set_mode() function returns %RIG_OK
 *      if the operation has been sucessful, or a negative value
 *      if an error occured (in which case, cause is set appropriately).
 *
 *      SEE ALSO: rig_get_mode()
 */

int rig_set_mode(RIG *rig, vfo_t vfo, rmode_t mode, pbwidth_t width)
{
		const struct rig_caps *caps;
		int retcode;
		vfo_t curr_vfo;

		if (!rig || !rig->caps)
			return -RIG_EINVAL;

		caps = rig->caps;

		if (caps->set_mode == NULL)
			return -RIG_ENAVAIL;

		if (caps->targetable_vfo || vfo == RIG_VFO_CURR ||
										vfo == rig->state.current_vfo)
			return caps->set_mode(rig, vfo, mode, width);

		if (!caps->set_vfo)
			return -RIG_ENTARGET;
		curr_vfo = rig->state.current_vfo;
		retcode = caps->set_vfo(rig, vfo);
		if (retcode != RIG_OK)
				return retcode;

		retcode = caps->set_mode(rig, vfo, mode, width);
		caps->set_vfo(rig, curr_vfo);
		return retcode;
}

/**
 *      rig_get_mode - get the mode of the current VFO
 *      @rig:	The rig handle
 *      @vfo:	The target VFO
 *      @mode:	The location where to store the current mode
 *      @width:	The location where to store the current passband width
 *
 *      The rig_get_mode() function retrieves the mode of the current VFO.
 *      If the backend is unable to determine the width, it must
 *      return %RIG_PASSBAND_NORMAL as a default.
 *	The value stored at @mode location equals %RIG_MODE_NONE when the current
 *	mode of the VFO is not defined (e.g. blank memory).
 *
 *      RETURN VALUE: The rig_get_mode() function returns %RIG_OK
 *      if the operation has been sucessful, or a negative value
 *      if an error occured (in which case, cause is set appropriately).
 *
 *      SEE ALSO: rig_set_mode()
 */

int rig_get_mode(RIG *rig, vfo_t vfo, rmode_t *mode, pbwidth_t *width)
{
		const struct rig_caps *caps;
		int retcode;
		vfo_t curr_vfo;

		if (!rig || !rig->caps || !mode || !width)
			return -RIG_EINVAL;

		caps = rig->caps;

		if (caps->get_mode == NULL)
			return -RIG_ENAVAIL;

		if (caps->targetable_vfo || vfo == RIG_VFO_CURR ||
										vfo == rig->state.current_vfo)
			return caps->get_mode(rig, vfo, mode, width);

		if (!caps->set_vfo)
			return -RIG_ENTARGET;
		curr_vfo = rig->state.current_vfo;
		retcode = caps->set_vfo(rig, vfo);
		if (retcode != RIG_OK)
				return retcode;

		retcode = caps->get_mode(rig, vfo, mode, width);
		caps->set_vfo(rig, curr_vfo);
		return retcode;
}

/**
 *      rig_passband_normal - get the normal passband of a mode
 *      @rig:	The rig handle
 *      @mode:	The mode to get the passband
 *
 *      The rig_passband_normal() function returns the normal (default)
 *      passband for the given the @mode.
 *
 *      RETURN VALUE: The rig_passband_normal() function returns 
 *      the passband in Hz if the operation has been sucessful,
 *      or a 0 if an error occured (passband not found, whatever).
 *
 *      SEE ALSO: rig_passband_narrow(), rig_passband_wide()
 */

pbwidth_t rig_passband_normal(RIG *rig, rmode_t mode)
{
		const struct rig_state *rs;
		int i;

		if (!rig)
				return 0;	/* huhu! */

		rs = &rig->state;

		for (i=0; i<FLTLSTSIZ && rs->filters[i].modes; i++) {
				if (rs->filters[i].modes & mode) {
						return rs->filters[i].width;
				}
		}

		return 0;
}

/**
 *      rig_passband_narrow - get the narrow passband of a mode
 *      @rig:	The rig handle
 *      @mode:	The mode to get the passband
 *
 *      The rig_passband_narrow() function returns the narrow (closest)
 *      passband for the given the @mode.
 *
 *		EXAMPLE: rig_set_mode(my_rig, RIG_MODE_LSB, 
 *				 			rig_passband_narrow(my_rig, RIG_MODE_LSB) );
 *
 *      RETURN VALUE: The rig_passband_narrow() function returns 
 *      the passband in Hz if the operation has been sucessful,
 *      or a 0 if an error occured (passband not found, whatever).
 *
 *      SEE ALSO: rig_passband_normal(), rig_passband_wide()
 */

pbwidth_t rig_passband_narrow(RIG *rig, rmode_t mode)
{
		const struct rig_state *rs;
		pbwidth_t normal;
		int i;

		if (!rig)
				return 0;	/* huhu! */

		rs = &rig->state;

		for (i=0; i<FLTLSTSIZ-1 && rs->filters[i].modes; i++) {
				if (rs->filters[i].modes & mode) {
					normal = rs->filters[i].width;
					for (i++; i<FLTLSTSIZ && rs->filters[i].modes; i++) {
							if ((rs->filters[i].modes & mode) &&
											(rs->filters[i].width < normal)) {
									return rs->filters[i].width;
							}
					}
					return 0;
				}
		}

		return 0;
}

/**
 *      rig_passband_wide - get the wide passband of a mode
 *      @rig:	The rig handle
 *      @mode:	The mode to get the passband
 *
 *      The rig_passband_wide() function returns the wide (default)
 *      passband for the given the @mode.
 *
 *		EXAMPLE: rig_set_mode(my_rig, RIG_MODE_AM, 
 *				 			rig_passband_wide(my_rig, RIG_MODE_AM) );
 *
 *      RETURN VALUE: The rig_passband_wide() function returns 
 *      the passband in Hz if the operation has been sucessful,
 *      or a 0 if an error occured (passband not found, whatever).
 *
 *      SEE ALSO: rig_passband_narrow(), rig_passband_normal()
 */

pbwidth_t rig_passband_wide(RIG *rig, rmode_t mode)
{
		const struct rig_state *rs;
		pbwidth_t normal;
		int i;

		if (!rig)
				return 0;	/* huhu! */

		rs = &rig->state;

		for (i=0; i<FLTLSTSIZ-1 && rs->filters[i].modes; i++) {
				if (rs->filters[i].modes & mode) {
					normal = rs->filters[i].width;
					for (i++; i<FLTLSTSIZ && rs->filters[i].modes; i++) {
							if ((rs->filters[i].modes & mode) &&
											(rs->filters[i].width > normal)) {
									return rs->filters[i].width;
							}
					}
					return 0;
				}
		}

		return 0;
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
		const struct rig_caps *caps;
		int retcode;

		if (!rig || !rig->caps)
			return -RIG_EINVAL;

		caps = rig->caps;

		if (caps->set_vfo == NULL)
			return -RIG_ENAVAIL;

		retcode= caps->set_vfo(rig, vfo);
		if (retcode == RIG_OK)
			rig->state.current_vfo = vfo;
		return retcode;
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
		const struct rig_caps *caps;
		int retcode;

		if (!rig || !rig->caps || !vfo)
			return -RIG_EINVAL;

		caps = rig->caps;

		if (caps->get_vfo == NULL)
			return -RIG_ENAVAIL;

		retcode= caps->get_vfo(rig, vfo);
		if (retcode == RIG_OK)
			rig->state.current_vfo = *vfo;
		return retcode;
}

/**
 *      rig_set_ptt - set PTT on/off
 *      @rig:	The rig handle
 *      @vfo:	The target VFO
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
int rig_set_ptt(RIG *rig, vfo_t vfo, ptt_t ptt)
{
		const struct rig_caps *caps;
		int retcode;
		vfo_t curr_vfo;

		if (!rig || !rig->caps)
			return -RIG_EINVAL;

		caps = rig->caps;

		switch (rig->state.ptt_type) {
		case RIG_PTT_RIG:
			if (caps->set_ptt == NULL)
				return -RIG_ENIMPL;

			if (caps->targetable_vfo || vfo == RIG_VFO_CURR ||
											vfo == rig->state.current_vfo)
				return caps->set_ptt(rig, vfo, ptt);
	
			if (!caps->set_vfo)
				return -RIG_ENTARGET;
			curr_vfo = rig->state.current_vfo;
			retcode = caps->set_vfo(rig, vfo);
			if (retcode != RIG_OK)
					return retcode;
	
			retcode = caps->set_ptt(rig, vfo, ptt);
			caps->set_vfo(rig, curr_vfo);
			return retcode;

			break;

		case RIG_PTT_SERIAL_DTR:
		case RIG_PTT_SERIAL_RTS:
				ser_ptt_set(&rig->state, ptt);
				break;
		case RIG_PTT_PARALLEL:
				par_ptt_set(&rig->state, ptt);
				break;

		case RIG_PTT_NONE:
				return -RIG_ENAVAIL;	/* not available */
		default:
				return -RIG_EINVAL;
		}

		return RIG_OK;
}

/**
 *      rig_get_ptt - get the status of the PTT
 *      @rig:	The rig handle
 *      @vfo:	The target VFO
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
int rig_get_ptt(RIG *rig, vfo_t vfo, ptt_t *ptt)
{	
		const struct rig_caps *caps;
		int retcode;
		vfo_t curr_vfo;

		if (!rig || !rig->caps || !ptt)
			return -RIG_EINVAL;

		caps = rig->caps;

		switch (rig->state.ptt_type) {
		case RIG_PTT_RIG:
			if (caps->get_ptt == NULL)
				return -RIG_ENIMPL;

			if (caps->targetable_vfo || vfo == RIG_VFO_CURR ||
											vfo == rig->state.current_vfo)
				return caps->get_ptt(rig, vfo, ptt);
	
			if (!caps->set_vfo)
				return -RIG_ENTARGET;
			curr_vfo = rig->state.current_vfo;
			retcode = caps->set_vfo(rig, vfo);
			if (retcode != RIG_OK)
					return retcode;
	
			retcode = caps->get_ptt(rig, vfo, ptt);
			caps->set_vfo(rig, curr_vfo);
			return retcode;

			break;

		case RIG_PTT_SERIAL_RTS:
		case RIG_PTT_SERIAL_DTR:
				ser_ptt_get(&rig->state, ptt);
				break;
		case RIG_PTT_PARALLEL:
				par_ptt_get(&rig->state, ptt);
				break;

		case RIG_PTT_NONE:
				return -RIG_ENAVAIL;	/* not available */

		default:
				return -RIG_EINVAL;
		}

		return RIG_OK;
}

/**
 *      rig_get_dcd - get the status of the DCD
 *      @rig:	The rig handle
 *      @vfo:	The target VFO
 *      @dcd:	The location where to store the status of the DCD
 *
 *      The rig_get_dcd() function retrieves the status of DCD (is squelch
 *	open?).
 *
 *      RETURN VALUE: The rig_get_dcd() function returns %RIG_OK
 *      if the operation has been sucessful, or a negative value
 *      if an error occured (in which case, cause is set appropriately).
 *
 */
int rig_get_dcd(RIG *rig, vfo_t vfo, dcd_t *dcd)
{	
		const struct rig_caps *caps;
		int retcode;
		vfo_t curr_vfo;

		if (!rig || !rig->caps || !dcd)
			return -RIG_EINVAL;

		caps = rig->caps;

		switch (rig->state.dcd_type) {
		case RIG_DCD_RIG:
			if (caps->get_dcd == NULL)
				return -RIG_ENIMPL;

			if (caps->targetable_vfo || vfo == RIG_VFO_CURR ||
											vfo == rig->state.current_vfo)
				return caps->get_dcd(rig, vfo, dcd);
	
			if (!caps->set_vfo)
				return -RIG_ENTARGET;
			curr_vfo = rig->state.current_vfo;
			retcode = caps->set_vfo(rig, vfo);
			if (retcode != RIG_OK)
					return retcode;
	
			retcode = caps->get_dcd(rig, vfo, dcd);
			caps->set_vfo(rig, curr_vfo);
			return retcode;

			break;

		case RIG_DCD_SERIAL_CTS:
		case RIG_DCD_SERIAL_DSR:
				ser_dcd_get(&rig->state, dcd);
				break;
		case RIG_DCD_PARALLEL:
				par_dcd_get(&rig->state, dcd);
				break;

		case RIG_DCD_NONE:
				return -RIG_ENAVAIL;	/* not available */

		default:
				return -RIG_EINVAL;
		}

		return RIG_OK;
}


/**
 *      rig_set_rptr_shift - set the repeater shift
 *      @rig:	The rig handle
 *      @vfo:	The target VFO
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
int rig_set_rptr_shift(RIG *rig, vfo_t vfo, rptr_shift_t rptr_shift)
{
		const struct rig_caps *caps;
		int retcode;
		vfo_t curr_vfo;

		if (!rig || !rig->caps)
			return -RIG_EINVAL;

		caps = rig->caps;

		if (caps->set_rptr_shift == NULL)
			return -RIG_ENAVAIL;

		if (caps->targetable_vfo || vfo == RIG_VFO_CURR ||
										vfo == rig->state.current_vfo)
			return caps->set_rptr_shift(rig, vfo, rptr_shift);

		if (!caps->set_vfo)
			return -RIG_ENTARGET;
		curr_vfo = rig->state.current_vfo;
		retcode = caps->set_vfo(rig, vfo);
		if (retcode != RIG_OK)
				return retcode;

		retcode = caps->set_rptr_shift(rig, vfo, rptr_shift);
		caps->set_vfo(rig, curr_vfo);
		return retcode;
}

/**
 *      rig_get_rptr_shift - get the current repeater shift
 *      @rig:	The rig handle
 *      @vfo:	The target VFO
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
int rig_get_rptr_shift(RIG *rig, vfo_t vfo, rptr_shift_t *rptr_shift)
{
		const struct rig_caps *caps;
		int retcode;
		vfo_t curr_vfo;

		if (!rig || !rig->caps || !rptr_shift)
			return -RIG_EINVAL;

		caps = rig->caps;

		if (caps->get_rptr_shift == NULL)
			return -RIG_ENAVAIL;

		if (caps->targetable_vfo || vfo == RIG_VFO_CURR ||
										vfo == rig->state.current_vfo)
			return caps->get_rptr_shift(rig, vfo, rptr_shift);

		if (!caps->set_vfo)
			return -RIG_ENTARGET;
		curr_vfo = rig->state.current_vfo;
		retcode = caps->set_vfo(rig, vfo);
		if (retcode != RIG_OK)
				return retcode;

		retcode = caps->get_rptr_shift(rig, vfo, rptr_shift);
		caps->set_vfo(rig, curr_vfo);
		return retcode;
}

/**
 *      rig_set_rptr_offs - set the repeater offset
 *      @rig:	The rig handle
 *      @vfo:	The target VFO
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

int rig_set_rptr_offs(RIG *rig, vfo_t vfo, shortfreq_t rptr_offs)
{
		const struct rig_caps *caps;
		int retcode;
		vfo_t curr_vfo;

		if (!rig || !rig->caps)
			return -RIG_EINVAL;

		caps = rig->caps;

		if (caps->set_rptr_offs == NULL)
			return -RIG_ENAVAIL;

		if (caps->targetable_vfo || vfo == RIG_VFO_CURR ||
										vfo == rig->state.current_vfo)
			return caps->set_rptr_offs(rig, vfo, rptr_offs);

		if (!caps->set_vfo)
			return -RIG_ENTARGET;
		curr_vfo = rig->state.current_vfo;
		retcode = caps->set_vfo(rig, vfo);
		if (retcode != RIG_OK)
				return retcode;

		retcode = caps->set_rptr_offs(rig, vfo, rptr_offs);
		caps->set_vfo(rig, curr_vfo);
		return retcode;
}

/**
 *      rig_get_rptr_offs - get the current repeater offset
 *      @rig:	The rig handle
 *      @vfo:	The target VFO
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

int rig_get_rptr_offs(RIG *rig, vfo_t vfo, shortfreq_t *rptr_offs)
{
		const struct rig_caps *caps;
		int retcode;
		vfo_t curr_vfo;

		if (!rig || !rig->caps || !rptr_offs)
			return -RIG_EINVAL;

		caps = rig->caps;

		if (caps->get_rptr_offs == NULL)
			return -RIG_ENAVAIL;

		if (caps->targetable_vfo || vfo == RIG_VFO_CURR ||
										vfo == rig->state.current_vfo)
			return caps->get_rptr_offs(rig, vfo, rptr_offs);

		if (!caps->set_vfo)
			return -RIG_ENTARGET;
		curr_vfo = rig->state.current_vfo;
		retcode = caps->set_vfo(rig, vfo);
		if (retcode != RIG_OK)
				return retcode;

		retcode = caps->get_rptr_offs(rig, vfo, rptr_offs);
		caps->set_vfo(rig, curr_vfo);
		return retcode;
}


/**
 *      rig_set_split_freq - set the split frequencies
 *      @rig:	The rig handle
 *      @vfo:	The target VFO
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

int rig_set_split_freq(RIG *rig, vfo_t vfo, freq_t rx_freq, freq_t tx_freq)
{
		const struct rig_caps *caps;
		int retcode;
		vfo_t curr_vfo;

		if (!rig || !rig->caps)
			return -RIG_EINVAL;

		caps = rig->caps;

		if (caps->set_split_freq == NULL)
			return -RIG_ENAVAIL;

		if (caps->targetable_vfo || vfo == RIG_VFO_CURR ||
										vfo == rig->state.current_vfo)
			return caps->set_split_freq(rig, vfo, rx_freq, tx_freq);

		if (!caps->set_vfo)
			return -RIG_ENTARGET;
		curr_vfo = rig->state.current_vfo;
		retcode = caps->set_vfo(rig, vfo);
		if (retcode != RIG_OK)
				return retcode;

		retcode = caps->set_split_freq(rig, vfo, rx_freq, tx_freq);
		caps->set_vfo(rig, curr_vfo);
		return retcode;
}

/**
 *      rig_get_split_freq - get the current split frequencies
 *      @rig:	The rig handle
 *      @vfo:	The target VFO
 *      @rx_freq:	The location where to store the current receive split frequency
 *      @tx_freq:	The location where to store the current transmit split frequency
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
int rig_get_split_freq(RIG *rig, vfo_t vfo, freq_t *rx_freq, freq_t *tx_freq)
{
		const struct rig_caps *caps;
		int retcode;
		vfo_t curr_vfo;

		if (!rig || !rig->caps || !rx_freq || !tx_freq)
			return -RIG_EINVAL;

		caps = rig->caps;

		if (caps->get_split_freq == NULL)
			return -RIG_ENAVAIL;

		if (caps->targetable_vfo || vfo == RIG_VFO_CURR ||
										vfo == rig->state.current_vfo)
			return caps->get_split_freq(rig, vfo, rx_freq, tx_freq);

		if (!caps->set_vfo)
			return -RIG_ENTARGET;
		curr_vfo = rig->state.current_vfo;
		retcode = caps->set_vfo(rig, vfo);
		if (retcode != RIG_OK)
				return retcode;

		retcode = caps->get_split_freq(rig, vfo, rx_freq, tx_freq);
		caps->set_vfo(rig, curr_vfo);
		return retcode;
}

/**
 *      rig_set_split_mode - set the split modes
 *      @rig:	The rig handle
 *      @vfo:	The target VFO
 *      @rx_mode:	The receive split mode to set to
 *      @rx_width:	The receive split width to set to
 *      @tx_mode:	The transmit split mode to set to
 *      @tx_width:	The transmit split width to set to
 *
 *      The rig_set_split_mode() function sets the split mode.
 *
 *      RETURN VALUE: The rig_set_split_mode() function returns %RIG_OK
 *      if the operation has been sucessful, or a negative value
 *      if an error occured (in which case, cause is set appropriately).
 *
 *      SEE ALSO: rig_get_split_mode()
 */

int rig_set_split_mode(RIG *rig, vfo_t vfo, rmode_t rx_mode, pbwidth_t rx_width, rmode_t tx_mode, pbwidth_t tx_width)
{
		const struct rig_caps *caps;
		int retcode;
		vfo_t curr_vfo;

		if (!rig || !rig->caps)
			return -RIG_EINVAL;

		caps = rig->caps;

		if (caps->set_split_mode == NULL)
			return -RIG_ENAVAIL;

		if (caps->targetable_vfo || vfo == RIG_VFO_CURR ||
										vfo == rig->state.current_vfo)
			return caps->set_split_mode(rig, vfo, rx_mode, rx_width, 
							tx_mode, tx_width);

		if (!caps->set_vfo)
			return -RIG_ENTARGET;
		curr_vfo = rig->state.current_vfo;
		retcode = caps->set_vfo(rig, vfo);
		if (retcode != RIG_OK)
				return retcode;

		retcode = caps->set_split_mode(rig, vfo, rx_mode, rx_width,
						tx_mode, tx_width);
		caps->set_vfo(rig, curr_vfo);
		return retcode;
}

/**
 *      rig_get_split_mode - get the current split modes
 *      @rig:	The rig handle
 *      @vfo:	The target VFO
 *      @rx_mode:	The location where to store the current receive split mode
 *      @rx_width:	The location where to store the current receive split width
 *      @tx_mode:	The location where to store the current transmit split mode
 *      @tx_width:	The location where to store the current transmit split width
 *
 *      The rig_get_split_mode() function retrieves the current split
 *      mode.
 *
 *      RETURN VALUE: The rig_get_split_mode() function returns %RIG_OK
 *      if the operation has been sucessful, or a negative value
 *      if an error occured (in which case, cause is set appropriately).
 *
 *      SEE ALSO: rig_set_split_mode()
 */
int rig_get_split_mode(RIG *rig, vfo_t vfo, rmode_t *rx_mode, pbwidth_t *rx_width, rmode_t *tx_mode, pbwidth_t *tx_width)
{
		const struct rig_caps *caps;
		int retcode;
		vfo_t curr_vfo;

		if (!rig || !rig->caps || !rx_mode || !rx_width || 
						!tx_mode || !tx_width)
			return -RIG_EINVAL;

		caps = rig->caps;

		if (caps->get_split_mode == NULL)
			return -RIG_ENAVAIL;

		if (caps->targetable_vfo || vfo == RIG_VFO_CURR ||
										vfo == rig->state.current_vfo)
			return caps->get_split_mode(rig, vfo, rx_mode, rx_width,
							tx_mode, tx_width);

		if (!caps->set_vfo)
			return -RIG_ENTARGET;
		curr_vfo = rig->state.current_vfo;
		retcode = caps->set_vfo(rig, vfo);
		if (retcode != RIG_OK)
				return retcode;

		retcode = caps->get_split_mode(rig, vfo, rx_mode, rx_width,
						tx_mode, tx_width);
		caps->set_vfo(rig, curr_vfo);
		return retcode;
}


/**
 *      rig_set_split - set the split mode
 *      @rig:	The rig handle
 *      @vfo:	The target VFO
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
int rig_set_split(RIG *rig, vfo_t vfo, split_t split)
{
		const struct rig_caps *caps;
		int retcode;
		vfo_t curr_vfo;

		if (!rig || !rig->caps)
			return -RIG_EINVAL;

		caps = rig->caps;

		if (caps->set_split == NULL)
			return -RIG_ENAVAIL;

		if (caps->targetable_vfo || vfo == RIG_VFO_CURR ||
										vfo == rig->state.current_vfo)
			return caps->set_split(rig, vfo, split);

		if (!caps->set_vfo)
			return -RIG_ENTARGET;
		curr_vfo = rig->state.current_vfo;
		retcode = caps->set_vfo(rig, vfo);
		if (retcode != RIG_OK)
				return retcode;

		retcode = caps->set_split(rig, vfo, split);
		caps->set_vfo(rig, curr_vfo);
		return retcode;
}

/**
 *      rig_get_split - get the current split mode
 *      @rig:	The rig handle
 *      @vfo:	The target VFO
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
int rig_get_split(RIG *rig, vfo_t vfo, split_t *split)
{
		const struct rig_caps *caps;
		int retcode;
		vfo_t curr_vfo;

		if (!rig || !rig->caps || !split)
			return -RIG_EINVAL;

		caps = rig->caps;

		if (caps->get_split == NULL)
			return -RIG_ENAVAIL;

		if (caps->targetable_vfo || vfo == RIG_VFO_CURR ||
										vfo == rig->state.current_vfo)
			return caps->get_split(rig, vfo, split);

		if (!caps->set_vfo)
			return -RIG_ENTARGET;
		curr_vfo = rig->state.current_vfo;
		retcode = caps->set_vfo(rig, vfo);
		if (retcode != RIG_OK)
				return retcode;

		retcode = caps->get_split(rig, vfo, split);
		caps->set_vfo(rig, curr_vfo);
		return retcode;
}

/**
 *      rig_set_rit - set the RIT
 *      @rig:	The rig handle
 *      @vfo:	The target VFO
 *      @rit:	The RIT offset to adjust to
 *
 *      The rig_set_rit() function sets the current RIT offset.
 *      A value of 0 for @rit disables RIT.
 *
 *      RETURN VALUE: The rig_set_rit() function returns %RIG_OK
 *      if the operation has been sucessful, or a negative value
 *      if an error occured (in which case, cause is set appropriately).
 *
 *      SEE ALSO: rig_get_rit()
 */

int rig_set_rit(RIG *rig, vfo_t vfo, shortfreq_t rit)
{
		const struct rig_caps *caps;
		int retcode;
		vfo_t curr_vfo;

		if (!rig || !rig->caps)
			return -RIG_EINVAL;

		caps = rig->caps;

		if (caps->set_rit == NULL)
			return -RIG_ENAVAIL;

		if (caps->targetable_vfo || vfo == RIG_VFO_CURR ||
										vfo == rig->state.current_vfo)
			return caps->set_rit(rig, vfo, rit);

		if (!caps->set_vfo)
			return -RIG_ENTARGET;
		curr_vfo = rig->state.current_vfo;
		retcode = caps->set_vfo(rig, vfo);
		if (retcode != RIG_OK)
				return retcode;

		retcode = caps->set_rit(rig, vfo, rit);
		caps->set_vfo(rig, curr_vfo);
		return retcode;
}

/**
 *      rig_get_rit - get the current RIT offset
 *      @rig:	The rig handle
 *      @vfo:	The target VFO
 *      @rit:	The location where to store the current RIT offset
 *
 *      The rig_get_rit() function retrieves the current RIT offset.
 *
 *      RETURN VALUE: The rig_get_rit() function returns %RIG_OK
 *      if the operation has been sucessful, or a negative value
 *      if an error occured (in which case, cause is set appropriately).
 *
 *      SEE ALSO: rig_set_rit()
 */

int rig_get_rit(RIG *rig, vfo_t vfo, shortfreq_t *rit)
{
		const struct rig_caps *caps;
		int retcode;
		vfo_t curr_vfo;

		if (!rig || !rig->caps || !rit)
			return -RIG_EINVAL;

		caps = rig->caps;

		if (caps->get_rit == NULL)
			return -RIG_ENAVAIL;

		if (caps->targetable_vfo || vfo == RIG_VFO_CURR ||
										vfo == rig->state.current_vfo)
			return caps->get_rit(rig, vfo, rit);

		if (!caps->set_vfo)
			return -RIG_ENTARGET;
		curr_vfo = rig->state.current_vfo;
		retcode = caps->set_vfo(rig, vfo);
		if (retcode != RIG_OK)
				return retcode;

		retcode = caps->get_rit(rig, vfo, rit);
		caps->set_vfo(rig, curr_vfo);
		return retcode;
}

/**
 *      rig_set_xit - set the XIT
 *      @rig:	The rig handle
 *      @vfo:	The target VFO
 *      @xit:	The XIT offset to adjust to
 *
 *      The rig_set_xit() function sets the current XIT offset.
 *      A value of 0 for @xit disables XIT.
 *
 *      RETURN VALUE: The rig_set_xit() function returns %RIG_OK
 *      if the operation has been sucessful, or a negative value
 *      if an error occured (in which case, cause is set appropriately).
 *
 *      SEE ALSO: rig_get_xit()
 */

int rig_set_xit(RIG *rig, vfo_t vfo, shortfreq_t xit)
{
		const struct rig_caps *caps;
		int retcode;
		vfo_t curr_vfo;

		if (!rig || !rig->caps)
			return -RIG_EINVAL;

		caps = rig->caps;

		if (caps->set_xit == NULL)
			return -RIG_ENAVAIL;

		if (caps->targetable_vfo || vfo == RIG_VFO_CURR ||
										vfo == rig->state.current_vfo)
			return caps->set_xit(rig, vfo, xit);

		if (!caps->set_vfo)
			return -RIG_ENTARGET;
		curr_vfo = rig->state.current_vfo;
		retcode = caps->set_vfo(rig, vfo);
		if (retcode != RIG_OK)
				return retcode;

		retcode = caps->set_xit(rig, vfo, xit);
		caps->set_vfo(rig, curr_vfo);
		return retcode;
}

/**
 *      rig_get_xit - get the current XIT offset
 *      @rig:	The rig handle
 *      @vfo:	The target VFO
 *      @xit:	The location where to store the current XIT offset
 *
 *      The rig_get_xit() function retrieves the current XIT offset.
 *
 *      RETURN VALUE: The rig_get_xit() function returns %RIG_OK
 *      if the operation has been sucessful, or a negative value
 *      if an error occured (in which case, cause is set appropriately).
 *
 *      SEE ALSO: rig_set_xit()
 */

int rig_get_xit(RIG *rig, vfo_t vfo, shortfreq_t *xit)
{
		const struct rig_caps *caps;
		int retcode;
		vfo_t curr_vfo;

		if (!rig || !rig->caps || !xit)
			return -RIG_EINVAL;

		caps = rig->caps;

		if (caps->get_xit == NULL)
			return -RIG_ENAVAIL;

		if (caps->targetable_vfo || vfo == RIG_VFO_CURR ||
										vfo == rig->state.current_vfo)
			return caps->get_xit(rig, vfo, xit);

		if (!caps->set_vfo)
			return -RIG_ENTARGET;
		curr_vfo = rig->state.current_vfo;
		retcode = caps->set_vfo(rig, vfo);
		if (retcode != RIG_OK)
				return retcode;

		retcode = caps->get_xit(rig, vfo, xit);
		caps->set_vfo(rig, curr_vfo);
		return retcode;
}



/**
 *      rig_set_ts - set the Tuning Step
 *      @rig:	The rig handle
 *      @vfo:	The target VFO
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

int rig_set_ts(RIG *rig, vfo_t vfo, shortfreq_t ts)
{
		const struct rig_caps *caps;
		int retcode;
		vfo_t curr_vfo;

		if (!rig || !rig->caps)
			return -RIG_EINVAL;

		caps = rig->caps;

		if (caps->set_ts == NULL)
			return -RIG_ENAVAIL;

		if (caps->targetable_vfo || vfo == RIG_VFO_CURR ||
										vfo == rig->state.current_vfo)
			return caps->set_ts(rig, vfo, ts);

		if (!caps->set_vfo)
			return -RIG_ENTARGET;
		curr_vfo = rig->state.current_vfo;
		retcode = caps->set_vfo(rig, vfo);
		if (retcode != RIG_OK)
				return retcode;

		retcode = caps->set_ts(rig, vfo, ts);
		caps->set_vfo(rig, curr_vfo);
		return retcode;
}

/**
 *      rig_get_ts - get the current Tuning Step
 *      @rig:	The rig handle
 *      @vfo:	The target VFO
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

int rig_get_ts(RIG *rig, vfo_t vfo, shortfreq_t *ts)
{
		const struct rig_caps *caps;
		int retcode;
		vfo_t curr_vfo;

		if (!rig || !rig->caps || !ts)
			return -RIG_EINVAL;

		caps = rig->caps;

		if (caps->get_ts == NULL)
			return -RIG_ENAVAIL;

		if (caps->targetable_vfo || vfo == RIG_VFO_CURR ||
										vfo == rig->state.current_vfo)
			return caps->get_ts(rig, vfo, ts);

		if (!caps->set_vfo)
			return -RIG_ENTARGET;
		curr_vfo = rig->state.current_vfo;
		retcode = caps->set_vfo(rig, vfo);
		if (retcode != RIG_OK)
				return retcode;

		retcode = caps->get_ts(rig, vfo, ts);
		caps->set_vfo(rig, curr_vfo);
		return retcode;
}

#if 0
/**
 *      rig_set_ann - set the announce level
 *      @rig:	The rig handle
 *      @ann:	The announce level
 *
 *      The rig_set_ann() function sets the current announce level.
 *
 *      RETURN VALUE: The rig_set_ann() function returns %RIG_OK
 *      if the operation has been sucessful, or a negative value
 *      if an error occured (in which case, cause is set appropriately).
 *
 *      SEE ALSO: rig_get_ann()
 */

int rig_set_ann(RIG *rig, ann_t ann)
{
		if (!rig || !rig->caps)
			return -RIG_EINVAL;

		if (rig->caps->set_ann == NULL)
			return -RIG_ENAVAIL;

		return rig->caps->set_ann(rig, ann);
}

/**
 *      rig_get_ann - get the current announce level
 *      @rig:	The rig handle
 *      @ann:	The location where to store the current announce level
 *
 *      The rig_get_ann() function retrieves the current announce level.
 *
 *      RETURN VALUE: The rig_get_ann() function returns %RIG_OK
 *      if the operation has been sucessful, or a negative value
 *      if an error occured (in which case, cause is set appropriately).
 *
 *      SEE ALSO: rig_set_ann()
 */

int rig_get_ann(RIG *rig, ann_t *ann)
{
		if (!rig || !rig->caps || !ann)
			return -RIG_EINVAL;

		if (rig->caps->get_ann == NULL)
			return -RIG_ENAVAIL;

		return rig->caps->get_ann(rig, ann);
}
#endif

/**
 *      rig_set_ant - set the antenna
 *      @rig:	The rig handle
 *      @vfo:	The target VFO
 *      @ant:	The anntena to select
 *
 *      The rig_set_ant() function sets the current antenna.
 *
 *      RETURN VALUE: The rig_set_ant() function returns %RIG_OK
 *      if the operation has been sucessful, or a negative value
 *      if an error occured (in which case, cause is set appropriately).
 *
 *      SEE ALSO: rig_get_ant()
 */

int rig_set_ant(RIG *rig, vfo_t vfo, ant_t ant)
{
		const struct rig_caps *caps;
		int retcode;
		vfo_t curr_vfo;

		if (!rig || !rig->caps)
			return -RIG_EINVAL;

		caps = rig->caps;

		if (caps->set_ant == NULL)
			return -RIG_ENAVAIL;

		if (caps->targetable_vfo || vfo == RIG_VFO_CURR ||
										vfo == rig->state.current_vfo)
			return caps->set_ant(rig, vfo, ant);

		if (!caps->set_vfo)
			return -RIG_ENTARGET;
		curr_vfo = rig->state.current_vfo;
		retcode = caps->set_vfo(rig, vfo);
		if (retcode != RIG_OK)
				return retcode;

		retcode = caps->set_ant(rig, vfo, ant);
		caps->set_vfo(rig, curr_vfo);
		return retcode;
}

/**
 *      rig_get_ant - get the current antenna
 *      @rig:	The rig handle
 *      @vfo:	The target VFO
 *      @ant:	The location where to store the current antenna
 *
 *      The rig_get_ant() function retrieves the current antenna.
 *
 *      RETURN VALUE: The rig_get_ant() function returns %RIG_OK
 *      if the operation has been sucessful, or a negative value
 *      if an error occured (in which case, cause is set appropriately).
 *
 *      SEE ALSO: rig_set_ant()
 */

int rig_get_ant(RIG *rig, vfo_t vfo, ant_t *ant)
{
		const struct rig_caps *caps;
		int retcode;
		vfo_t curr_vfo;

		if (!rig || !rig->caps || !ant)
			return -RIG_EINVAL;

		caps = rig->caps;

		if (caps->get_ant == NULL)
			return -RIG_ENAVAIL;

		if (caps->targetable_vfo || vfo == RIG_VFO_CURR ||
										vfo == rig->state.current_vfo)
			return caps->get_ant(rig, vfo, ant);

		if (!caps->set_vfo)
			return -RIG_ENTARGET;
		curr_vfo = rig->state.current_vfo;
		retcode = caps->set_vfo(rig, vfo);
		if (retcode != RIG_OK)
				return retcode;

		retcode = caps->get_ant(rig, vfo, ant);
		caps->set_vfo(rig, curr_vfo);
		return retcode;
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
int rig_power2mW(RIG *rig, unsigned int *mwpower, float power, freq_t freq, rmode_t mode)
{
		const freq_range_t *txrange;

		if (!rig || !rig->caps || !mwpower || power<0.0 || power>1.0)
			return -RIG_EINVAL;

		if (rig->caps->power2mW != NULL)
			return rig->caps->power2mW(rig, mwpower, power, freq, mode);

		txrange = rig_get_range(rig->state.tx_range_list, freq, mode);
		if (!txrange) {
			/*
			 * freq is not on the tx range!
			 */
			return -RIG_ECONF; /* could be RIG_EINVAL ? */
		}
		*mwpower = (unsigned int)(power * txrange->high_power);
		return RIG_OK;
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
int rig_mW2power(RIG *rig, float *power, unsigned int mwpower, freq_t freq, rmode_t mode)
{
		const freq_range_t *txrange;

		if (!rig || !rig->caps || !power || mwpower==0)
			return -RIG_EINVAL;

		if (rig->caps->mW2power != NULL)
			return rig->caps->mW2power(rig, power, mwpower, freq, mode);

		txrange = rig_get_range(rig->state.tx_range_list, freq, mode);
		if (!txrange) {
			/*
			 * freq is not on the tx range!
			 */
			return -RIG_ECONF; /* could be RIG_EINVAL ? */
		}
		if (mwpower == 0) {
				*power = 0.0;
				return RIG_OK;
		}
		*power = (float)txrange->high_power/mwpower;
		if (*power > 1.0)
				*power = 1.0;
		return (mwpower>txrange->high_power? RIG_OK : RIG_ETRUNC);
}

/**
 *      rig_get_resolution - get the best frequency resolution of the rig
 *      @rig:	The rig handle
 *      @mode:	The mode where the conversion should take place
 *
 *      The rig_get_resolution() function returns the best frequency
 *      resolution of the rig, for a given @mode.
 *
 *      RETURN VALUE: The rig_get_resolution() function returns the
 *      frequency resolution in Hertz if the operation has been sucessful,
 *      or a negative value if an error occured.
 *
 */
shortfreq_t rig_get_resolution(RIG *rig, rmode_t mode)
{
		const struct rig_state *rs;
		int i;

		if (!rig || !rig->caps || !mode)
			return -RIG_EINVAL;

		rs = &rig->state;

		for (i=0; i<TSLSTSIZ && rs->tuning_steps[i].ts; i++) {
				if (rs->tuning_steps[i].modes & mode)
						return rs->tuning_steps[i].ts;
		}

		return -RIG_EINVAL;
}

/**
 *      rig_set_ctcss - set CTCSS
 *      @rig:	The rig handle
 *      @vfo:	The target VFO
 *      @tone:	The tone to set to
 *
 *      The rig_set_ctcss() function sets the current Continuous Tone
 *      Controlled Squelch System (CTCSS) sub-audible tone.
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

int rig_set_ctcss(RIG *rig, vfo_t vfo, unsigned int tone)
{
		const struct rig_caps *caps;
		int retcode;
		vfo_t curr_vfo;

		if (!rig || !rig->caps)
			return -RIG_EINVAL;

		caps = rig->caps;

		if (caps->set_ctcss == NULL)
			return -RIG_ENAVAIL;

		if (caps->targetable_vfo || vfo == RIG_VFO_CURR ||
										vfo == rig->state.current_vfo)
			return caps->set_ctcss(rig, vfo, tone);

		if (!caps->set_vfo)
			return -RIG_ENTARGET;
		curr_vfo = rig->state.current_vfo;
		retcode = caps->set_vfo(rig, vfo);
		if (retcode != RIG_OK)
				return retcode;

		retcode = caps->set_ctcss(rig, vfo, tone);
		caps->set_vfo(rig, curr_vfo);
		return retcode;
}

/**
 *      rig_get_ctcss - get the current CTCSS
 *      @rig:	The rig handle
 *      @vfo:	The target VFO
 *      @tone:	The location where to store the current tone
 *
 *      The rig_get_ctcss() function retrieves the current Continuous Tone
 *      Controlled Squelch System (CTCSS) sub-audible tone.
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
int rig_get_ctcss(RIG *rig, vfo_t vfo, unsigned int *tone)
{
		const struct rig_caps *caps;
		int retcode;
		vfo_t curr_vfo;

		if (!rig || !rig->caps || !tone)
			return -RIG_EINVAL;

		caps = rig->caps;

		if (caps->get_ctcss == NULL)
			return -RIG_ENAVAIL;

		if (caps->targetable_vfo || vfo == RIG_VFO_CURR ||
										vfo == rig->state.current_vfo)
			return caps->get_ctcss(rig, vfo, tone);

		if (!caps->set_vfo)
			return -RIG_ENTARGET;
		curr_vfo = rig->state.current_vfo;
		retcode = caps->set_vfo(rig, vfo);
		if (retcode != RIG_OK)
				return retcode;

		retcode = caps->get_ctcss(rig, vfo, tone);
		caps->set_vfo(rig, curr_vfo);
		return retcode;
}

/**
 *      rig_set_dcs - set the current DCS
 *      @rig:	The rig handle
 *      @vfo:	The target VFO
 *      @code:	The tone to set to
 *
 *      The rig_set_dcs() function sets the current Digitally-Coded Squelch
 *      code.
 *
 *      RETURN VALUE: The rig_set_dcs() function returns %RIG_OK
 *      if the operation has been sucessful, or a negative value
 *      if an error occured (in which case, cause is set appropriately).
 *
 *      SEE ALSO: rig_get_dcs(), rig_set_ctcss(), rig_get_ctcss()
 */

int rig_set_dcs(RIG *rig, vfo_t vfo, unsigned int code)
{
		const struct rig_caps *caps;
		int retcode;
		vfo_t curr_vfo;

		if (!rig || !rig->caps)
			return -RIG_EINVAL;

		caps = rig->caps;

		if (caps->set_dcs == NULL)
			return -RIG_ENAVAIL;

		if (caps->targetable_vfo || vfo == RIG_VFO_CURR ||
										vfo == rig->state.current_vfo)
			return caps->set_dcs(rig, vfo, code);

		if (!caps->set_vfo)
			return -RIG_ENTARGET;
		curr_vfo = rig->state.current_vfo;
		retcode = caps->set_vfo(rig, vfo);
		if (retcode != RIG_OK)
				return retcode;

		retcode = caps->set_dcs(rig, vfo, code);
		caps->set_vfo(rig, curr_vfo);
		return retcode;
}

/**
 *      rig_get_dcs - get the current DCS
 *      @rig:	The rig handle
 *      @vfo:	The target VFO
 *      @code:	The location where to store the current tone
 *
 *      The rig_get_dcs() function retrieves the current 
 *      Digitally-Coded Squelch. 
 *
 *      RETURN VALUE: The rig_get_dcs() function returns %RIG_OK
 *      if the operation has been sucessful, or a negative value
 *      if an error occured (in which case, cause is set appropriately).
 *
 *      SEE ALSO: rig_get_dcs(), rig_set_ctcss(), rig_get_ctcss()
 */
int rig_get_dcs(RIG *rig, vfo_t vfo, unsigned int *code)
{
		const struct rig_caps *caps;
		int retcode;
		vfo_t curr_vfo;

		if (!rig || !rig->caps || !code)
			return -RIG_EINVAL;

		caps = rig->caps;

		if (caps->get_dcs == NULL)
			return -RIG_ENAVAIL;

		if (caps->targetable_vfo || vfo == RIG_VFO_CURR ||
										vfo == rig->state.current_vfo)
			return caps->get_dcs(rig, vfo, code);

		if (!caps->set_vfo)
			return -RIG_ENTARGET;
		curr_vfo = rig->state.current_vfo;
		retcode = caps->set_vfo(rig, vfo);
		if (retcode != RIG_OK)
				return retcode;

		retcode = caps->get_dcs(rig, vfo, code);
		caps->set_vfo(rig, curr_vfo);
		return retcode;
}

/**
 *      rig_set_ctcss_sql - set CTCSS squelch
 *      @rig:	The rig handle
 *      @vfo:	The target VFO
 *      @tone:	The PL tone to set the squelch to
 *
 *      The rig_set_ctcss_sql() function sets the current Continuous Tone
 *      Controlled Squelch System (CTCSS) sub-audible squelch tone.
 *      NB, @tone is NOT in Hz, but in tenth of Hz! This way,
 *      if you want to set subaudible tone of 88.5 Hz for example,
 *      then pass 885 to this function. Also, to disable Tone squelch,
 *      set @tone to 0.
 *
 *      RETURN VALUE: The rig_set_ctcss_sql() function returns %RIG_OK
 *      if the operation has been sucessful, or a negative value
 *      if an error occured (in which case, cause is set appropriately).
 *
 *      SEE ALSO: rig_get_ctcss_sql(), rig_set_ctcss()
 */

int rig_set_ctcss_sql(RIG *rig, vfo_t vfo, unsigned int tone)
{
		const struct rig_caps *caps;
		int retcode;
		vfo_t curr_vfo;

		if (!rig || !rig->caps)
			return -RIG_EINVAL;

		caps = rig->caps;

		if (caps->set_ctcss_sql == NULL)
			return -RIG_ENAVAIL;

		if (caps->targetable_vfo || vfo == RIG_VFO_CURR ||
										vfo == rig->state.current_vfo)
			return caps->set_ctcss_sql(rig, vfo, tone);

		if (!caps->set_vfo)
			return -RIG_ENTARGET;
		curr_vfo = rig->state.current_vfo;
		retcode = caps->set_vfo(rig, vfo);
		if (retcode != RIG_OK)
				return retcode;

		retcode = caps->set_ctcss_sql(rig, vfo, tone);
		caps->set_vfo(rig, curr_vfo);
		return retcode;
}

/**
 *      rig_get_ctcss_sql - get the current CTCSS squelch
 *      @rig:	The rig handle
 *      @vfo:	The target VFO
 *      @tone:	The location where to store the current tone
 *
 *      The rig_get_ctcss_sql() function retrieves the current Continuous Tone
 *      Controlled Squelch System (CTCSS) sub-audible squelch tone.
 *      NB, @tone is NOT in Hz, but in tenth of Hz! This way,
 *      if the function rig_get_ctcss() returns a subaudible tone of 885
 *      for example, then the real tone is 88.5 Hz. 
 *      Also, a value of 0 for @tone means the Tone squelch is disabled.
 *
 *      RETURN VALUE: The rig_get_ctcss_sql() function returns %RIG_OK
 *      if the operation has been sucessful, or a negative value
 *      if an error occured (in which case, cause is set appropriately).
 *
 *      SEE ALSO: rig_set_ctcss_sql(), rig_get_ctcss()
 */
int rig_get_ctcss_sql(RIG *rig, vfo_t vfo, unsigned int *tone)
{
		const struct rig_caps *caps;
		int retcode;
		vfo_t curr_vfo;

		if (!rig || !rig->caps || !tone)
			return -RIG_EINVAL;

		caps = rig->caps;

		if (caps->get_ctcss_sql == NULL)
			return -RIG_ENAVAIL;

		if (caps->targetable_vfo || vfo == RIG_VFO_CURR ||
										vfo == rig->state.current_vfo)
			return caps->get_ctcss_sql(rig, vfo, tone);

		if (!caps->set_vfo)
			return -RIG_ENTARGET;
		curr_vfo = rig->state.current_vfo;
		retcode = caps->set_vfo(rig, vfo);
		if (retcode != RIG_OK)
				return retcode;

		retcode = caps->get_ctcss_sql(rig, vfo, tone);
		caps->set_vfo(rig, curr_vfo);
		return retcode;
}

/**
 *      rig_set_dcs_sql - set the current DCS
 *      @rig:	The rig handle
 *      @vfo:	The target VFO
 *      @code:	The tone to set to
 *
 *      The rig_set_dcs_sql() function sets the current Digitally-Coded Squelch
 *      code.
 *
 *      RETURN VALUE: The rig_set_dcs_sql() function returns %RIG_OK
 *      if the operation has been sucessful, or a negative value
 *      if an error occured (in which case, cause is set appropriately).
 *
 *      SEE ALSO: rig_get_dcs_sql(), rig_set_dcs()
 */

int rig_set_dcs_sql(RIG *rig, vfo_t vfo, unsigned int code)
{
		const struct rig_caps *caps;
		int retcode;
		vfo_t curr_vfo;

		if (!rig || !rig->caps)
			return -RIG_EINVAL;

		caps = rig->caps;

		if (caps->set_dcs_sql == NULL)
			return -RIG_ENAVAIL;

		if (caps->targetable_vfo || vfo == RIG_VFO_CURR ||
										vfo == rig->state.current_vfo)
			return caps->set_dcs_sql(rig, vfo, code);

		if (!caps->set_vfo)
			return -RIG_ENTARGET;
		curr_vfo = rig->state.current_vfo;
		retcode = caps->set_vfo(rig, vfo);
		if (retcode != RIG_OK)
				return retcode;

		retcode = caps->set_dcs_sql(rig, vfo, code);
		caps->set_vfo(rig, curr_vfo);
		return retcode;
}

/**
 *      rig_get_dcs_sql - get the current DCS
 *      @rig:	The rig handle
 *      @vfo:	The target VFO
 *      @code:	The location where to store the current tone
 *
 *      The rig_get_dcs_sql() function retrieves the current 
 *      Digitally-Coded Squelch. 
 *
 *      RETURN VALUE: The rig_get_dcs_sql() function returns %RIG_OK
 *      if the operation has been sucessful, or a negative value
 *      if an error occured (in which case, cause is set appropriately).
 *
 *      SEE ALSO: rig_get_dcs_sql(), rig_get_dcs()
 */
int rig_get_dcs_sql(RIG *rig, vfo_t vfo, unsigned int *code)
{
		const struct rig_caps *caps;
		int retcode;
		vfo_t curr_vfo;

		if (!rig || !rig->caps || !code)
			return -RIG_EINVAL;

		caps = rig->caps;

		if (caps->get_dcs_sql == NULL)
			return -RIG_ENAVAIL;

		if (caps->targetable_vfo || vfo == RIG_VFO_CURR ||
										vfo == rig->state.current_vfo)
			return caps->get_dcs_sql(rig, vfo, code);

		if (!caps->set_vfo)
			return -RIG_ENTARGET;
		curr_vfo = rig->state.current_vfo;
		retcode = caps->set_vfo(rig, vfo);
		if (retcode != RIG_OK)
				return retcode;

		retcode = caps->get_dcs_sql(rig, vfo, code);
		caps->set_vfo(rig, curr_vfo);
		return retcode;
}


/**
 *      rig_set_powerstat - turn on/off the radio
 *      @rig:	The rig handle
 *      @status:	The status to set to
 *
 *      The rig_set_powerstat() function turns on/off the radio.
 *      See %RIG_POWER_ON, %RIG_POWER_OFF and %RIG_POWER_STANDBY defines
 *      for the @status.
 *
 *      RETURN VALUE: The rig_set_powerstat() function returns %RIG_OK
 *      if the operation has been sucessful, or a negative value
 *      if an error occured (in which case, cause is set appropriately).
 *
 *      SEE ALSO: rig_get_powerstat()
 */

int rig_set_powerstat(RIG *rig, powerstat_t status)
{
		if (!rig || !rig->caps)
			return -RIG_EINVAL;

		if (rig->caps->set_powerstat == NULL)
			return -RIG_ENAVAIL;

		return rig->caps->set_powerstat(rig, status);
}

/**
 *      rig_get_powerstat - get the on/off status of the radio
 *      @rig:	The rig handle
 *      @status:	The locatation where to store the current status
 *
 *      The rig_get_powerstat() function retrieve the status of the radio.
 *      See %RIG_POWER_ON, %RIG_POWER_OFF and %RIG_POWER_STANDBY defines
 *      for the @status.
 *
 *      RETURN VALUE: The rig_get_powerstat() function returns %RIG_OK
 *      if the operation has been sucessful, or a negative value
 *      if an error occured (in which case, cause is set appropriately).
 *
 *      SEE ALSO: rig_set_powerstat()
 */

int rig_get_powerstat(RIG *rig, powerstat_t *status)
{
		if (!rig || !rig->caps || !status)
			return -RIG_EINVAL;

		if (rig->caps->get_powerstat == NULL)
			return -RIG_ENAVAIL;

		return rig->caps->get_powerstat(rig, status);
}

/**
 *      rig_reset - reset the radio
 *      @rig:	The rig handle
 *      @reset:	The reset operation to perform
 *
 *      The rig_reset() function reset the radio.
 *      See %RIG_RESET_NONE, %RIG_RESET_SOFT and %RIG_RESET_MCALL defines
 *      for the @reset.
 *
 *      RETURN VALUE: The rig_reset() function returns %RIG_OK
 *      if the operation has been sucessful, or a negative value
 *      if an error occured (in which case, cause is set appropriately).
 *
 */

int rig_reset(RIG *rig, reset_t reset)
{
		if (!rig || !rig->caps)
			return -RIG_EINVAL;

		if (rig->caps->reset == NULL)
			return -RIG_ENAVAIL;

		return rig->caps->reset(rig, reset);
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
 *      @vfo:	The target VFO
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
int rig_set_level(RIG *rig, vfo_t vfo, setting_t level, value_t val)
{
		const struct rig_caps *caps;
		int retcode;
		vfo_t curr_vfo;

		if (!rig || !rig->caps)
			return -RIG_EINVAL;

		caps = rig->caps;

		if (caps->set_level == NULL || !rig_has_set_level(rig,level))
			return -RIG_ENAVAIL;

		if (caps->targetable_vfo || vfo == RIG_VFO_CURR ||
										vfo == rig->state.current_vfo)
			return caps->set_level(rig, vfo, level, val);

		if (!caps->set_vfo)
			return -RIG_ENTARGET;
		curr_vfo = rig->state.current_vfo;
		retcode = caps->set_vfo(rig, vfo);
		if (retcode != RIG_OK)
				return retcode;

		retcode = caps->set_level(rig, vfo, level, val);
		caps->set_vfo(rig, curr_vfo);
		return retcode;
}

/**
 *      rig_get_level - get the value of a level
 *      @rig:	The rig handle
 *      @vfo:	The target VFO
 *      @level:	The level setting
 *      @val:	The location where to store the value of @level
 *
 *      The rig_get_level() function retrieves the value of a @level.
 *      The level value @val can be a float or an integer. See &value_t
 *      for more information.
 *
 * 		%RIG_LEVEL_STRENGTH: @val is an integer, representing the S Meter
 * 		level in dB, according to the ideal S Meter scale. The ideal
 * 		S Meter scale is as follow: S0=-54, S1=-48, S2=-42, S3=-36,
 * 		S4=-30, S5=-24, S6=-18, S7=-12, S8=-6, S9=0, +10=10, +20=20,
 * 		+30=30, +40=40, +50=50 and +60=60. This is the responsability
 * 		of the backend to return values calibrated for this scale.
 *
 *      RETURN VALUE: The rig_get_level() function returns %RIG_OK
 *      if the operation has been sucessful, or a negative value
 *      if an error occured (in which case, cause is set appropriately).
 *
 *      SEE ALSO: rig_has_level(), rig_set_level()
 */
int rig_get_level(RIG *rig, vfo_t vfo, setting_t level, value_t *val)
{
		const struct rig_caps *caps;
		int retcode;
		vfo_t curr_vfo;

		if (!rig || !rig->caps || !val)
			return -RIG_EINVAL;

		caps = rig->caps;

		if (caps->get_level == NULL || !rig_has_get_level(rig,level))
			return -RIG_ENAVAIL;

		if (caps->targetable_vfo || vfo == RIG_VFO_CURR ||
										vfo == rig->state.current_vfo)
			return caps->get_level(rig, vfo, level, val);

		if (!caps->set_vfo)
			return -RIG_ENTARGET;
		curr_vfo = rig->state.current_vfo;
		retcode = caps->set_vfo(rig, vfo);
		if (retcode != RIG_OK)
				return retcode;

		retcode = caps->get_level(rig, vfo, level, val);
		caps->set_vfo(rig, curr_vfo);
		return retcode;
}

/**
 *      rig_set_parm - set a radio parameter
 *      @rig:	The rig handle
 *      @parm:	The parameter
 *      @val:	The value to set the parameter sto
 *
 *      The rig_set_parm() function sets a parameter. 
 *      The parameter value @val can be a float or an integer. See &value_t
 *      for more information.
 *
 *      RETURN VALUE: The rig_set_parm() function returns %RIG_OK
 *      if the operation has been sucessful, or a negative value
 *      if an error occured (in which case, cause is set appropriately).
 *
 *      SEE ALSO: rig_has_set_parm(), rig_get_parm()
 */
int rig_set_parm(RIG *rig, setting_t parm, value_t val)
{
		if (!rig || !rig->caps)
			return -RIG_EINVAL;

		if (rig->caps->set_parm == NULL || !rig_has_set_parm(rig,parm))
			return -RIG_ENAVAIL;

		return rig->caps->set_parm(rig, parm, val);
}

/**
 *      rig_get_parm - get the value of a parameter
 *      @rig:	The rig handle
 *      @parm:	The parameter
 *      @val:	The location where to store the value of @parm
 *
 *      The rig_get_parm() function retrieves the value of a @parm.
 *      The parameter value @val can be a float or an integer. See &value_t
 *      for more information.
 *
 *      RETURN VALUE: The rig_get_parm() function returns %RIG_OK
 *      if the operation has been sucessful, or a negative value
 *      if an error occured (in which case, cause is set appropriately).
 *
 *      SEE ALSO: rig_has_parm(), rig_set_parm()
 */
int rig_get_parm(RIG *rig, setting_t parm, value_t *val)
{
		if (!rig || !rig->caps || !val)
			return -RIG_EINVAL;

		if (rig->caps->get_parm == NULL || !rig_has_get_parm(rig,parm))
			return -RIG_ENAVAIL;

		return rig->caps->get_parm(rig, parm, val);
}

/**
 *      rig_has_get_level - check retrieval ability of level settings
 *      @rig:	The rig handle
 *      @level:	The level settings
 *
 *      The rig_has_level() "macro" checks if a rig can *get* a level setting.
 *      Since the @level is a OR'ed bitwise argument, more than
 *      one level can be checked at the same time.
 *
 *      RETURN VALUE: The rig_has_get_level() "macro" returns a bit wise
 *      mask of supported level settings that can be retrieve,
 *      0 if none supported.
 *
 * 		EXAMPLE: if (rig_has_get_level(my_rig, RIG_LVL_STRENGTH)) disp_Smeter();
 *
 *      SEE ALSO: rig_has_set_level(), rig_get_level()
 */
setting_t rig_has_get_level(RIG *rig, setting_t level)
{
		if (!rig || !rig->caps)
				return 0;

		return (rig->state.has_get_level & level);
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
				return 0;

		return (rig->state.has_set_level & level);
}

/**
 *      rig_has_get_parm - check retrieval ability of parameter settings
 *      @rig:	The rig handle
 *      @parm:	The parameter settings
 *
 *      The rig_has_parm() "macro" checks if a rig can *get* a parm setting.
 *      Since the @parm is a OR'ed bitwise argument, more than
 *      one parameter can be checked at the same time.
 *
 *      RETURN VALUE: The rig_has_get_parm() "macro" returns a bit wise
 *      mask of supported parameter settings that can be retrieve,
 *      0 if none supported.
 *
 * 		EXAMPLE: if (rig_has_get_parm(my_rig, RIG_PARM_ANN)) good4you();
 *
 *      SEE ALSO: rig_has_set_parm(), rig_get_parm()
 */
setting_t rig_has_get_parm(RIG *rig, setting_t parm)
{
		if (!rig || !rig->caps)
				return 0;

		return (rig->state.has_get_parm & parm);
}


/**
 *      rig_has_set_parm - check settable ability of parameter settings
 *      @rig:	The rig handle
 *      @parm:	The parameter settings
 *
 *      The rig_has_set_parm() "macro" checks if a rig can *set* a parameter
 *      setting. Since the @parm is a OR'ed bitwise argument, more than
 *      one parameter can be check at the same time.
 *
 *      RETURN VALUE: The rig_has_set_parm() "macro" returns a bit wise
 *      mask of supported parameter settings that can be set,
 *      0 if none supported.
 *
 * 		EXAMPLE: if (rig_has_set_parm(my_rig, RIG_PARM_ANN)) announce_all();
 *
 *      SEE ALSO: rig_has_parm(), rig_set_parm()
 */
setting_t rig_has_set_parm(RIG *rig, setting_t parm)
{
		if (!rig || !rig->caps)
				return 0;

		return (rig->state.has_set_parm & parm);
}

/**
 *      rig_has_get_func - check ability of radio functions
 *      @rig:	The rig handle
 *      @func:	The functions
 *
 *      The rig_has_func() "macro" checks if a rig supports a set of functions.
 *      Since the @func is a OR'ed bitwise argument, more than
 *      one function can be checked at the same time.
 *
 *      RETURN VALUE: The rig_has_get_func() "macro" returns a bit wise
 *      mask of supported functions, 0 if none supported.
 *
 * 		EXAMPLE: if (rig_has_get_func(my_rig,RIG_FUNC_FAGC)) disp_fagc_button();
 *
 *      SEE ALSO: rig_set_func(), rig_get_func()
 */
setting_t rig_has_get_func(RIG *rig, setting_t func)
{
		if (!rig || !rig->caps)
				return 0;

		return (rig->state.has_get_func & func);
}

/**
 *      rig_has_set_func - check ability of radio functions
 *      @rig:	The rig handle
 *      @func:	The functions
 *
 *      The rig_has_func() "macro" checks if a rig supports a set of functions.
 *      Since the @func is a OR'ed bitwise argument, more than
 *      one function can be checked at the same time.
 *
 *      RETURN VALUE: The rig_has_set_func() "macro" returns a bit wise
 *      mask of supported functions, 0 if none supported.
 *
 * 		EXAMPLE: if (rig_has_set_func(my_rig,RIG_FUNC_FAGC)) disp_fagc_button();
 *
 *      SEE ALSO: rig_set_func(), rig_get_func()
 */
setting_t rig_has_set_func(RIG *rig, setting_t func)
{
		if (!rig || !rig->caps)
				return 0;

		return (rig->state.has_set_func & func);
}

/**
 *      rig_set_func - activate/desactivate functions of radio
 *      @rig:	The rig handle
 *      @vfo:	The target VFO
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

int rig_set_func(RIG *rig, vfo_t vfo, setting_t func, int status)
{
		const struct rig_caps *caps;
		int retcode;
		vfo_t curr_vfo;

		if (!rig || !rig->caps)
			return -RIG_EINVAL;

		caps = rig->caps;

		if (caps->set_func == NULL || !rig_has_set_func(rig,func))
			return -RIG_ENAVAIL;

		if (caps->targetable_vfo || vfo == RIG_VFO_CURR ||
										vfo == rig->state.current_vfo)
			return caps->set_func(rig, vfo, func, status);

		if (!caps->set_vfo)
			return -RIG_ENTARGET;
		curr_vfo = rig->state.current_vfo;
		retcode = caps->set_vfo(rig, vfo);
		if (retcode != RIG_OK)
				return retcode;

		retcode = caps->set_func(rig, vfo, func, status);
		caps->set_vfo(rig, curr_vfo);
		return retcode;
}

/**
 *      rig_get_func - get the status of functions of the radio
 *      @rig:	The rig handle
 *      @vfo:	The target VFO
 *      @func:	The functions to get the status
 *      @status:	The location where to store the function status
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
int rig_get_func(RIG *rig, vfo_t vfo, setting_t func, int *status)
{
		const struct rig_caps *caps;
		int retcode;
		vfo_t curr_vfo;

		if (!rig || !rig->caps || !func)
			return -RIG_EINVAL;

		caps = rig->caps;

		if (caps->get_func == NULL || !rig_has_get_func(rig,func))
			return -RIG_ENAVAIL;

		if (caps->targetable_vfo || vfo == RIG_VFO_CURR ||
										vfo == rig->state.current_vfo)
			return caps->get_func(rig, vfo, func, status);

		if (!caps->set_vfo)
			return -RIG_ENTARGET;
		curr_vfo = rig->state.current_vfo;
		retcode = caps->set_vfo(rig, vfo);
		if (retcode != RIG_OK)
				return retcode;

		retcode = caps->get_func(rig, vfo, func, status);
		caps->set_vfo(rig, curr_vfo);
		return retcode;
}

/**
 *      rig_set_mem - set the current memory channel number
 *      @rig:	The rig handle
 *      @vfo:	The target VFO
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

int rig_set_mem(RIG *rig, vfo_t vfo, int ch)
{
		const struct rig_caps *caps;
		int retcode;
		vfo_t curr_vfo;

		if (!rig || !rig->caps)
			return -RIG_EINVAL;

		caps = rig->caps;

		if (caps->set_mem == NULL)
			return -RIG_ENAVAIL;

		if (caps->targetable_vfo || vfo == RIG_VFO_CURR ||
										vfo == rig->state.current_vfo)
			return caps->set_mem(rig, vfo, ch);

		if (!caps->set_vfo)
			return -RIG_ENTARGET;
		curr_vfo = rig->state.current_vfo;
		retcode = caps->set_vfo(rig, vfo);
		if (retcode != RIG_OK)
				return retcode;

		retcode = caps->set_mem(rig, vfo, ch);
		caps->set_vfo(rig, curr_vfo);
		return retcode;
}

/**
 *      rig_get_mem - get the current memory channel number
 *      @rig:	The rig handle
 *      @vfo:	The target VFO
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
int rig_get_mem(RIG *rig, vfo_t vfo, int *ch)
{
		const struct rig_caps *caps;
		int retcode;
		vfo_t curr_vfo;

		if (!rig || !rig->caps || !ch)
			return -RIG_EINVAL;

		caps = rig->caps;

		if (caps->get_mem == NULL)
			return -RIG_ENAVAIL;

		if (caps->targetable_vfo || vfo == RIG_VFO_CURR ||
										vfo == rig->state.current_vfo)
			return caps->get_mem(rig, vfo, ch);

		if (!caps->set_vfo)
			return -RIG_ENTARGET;
		curr_vfo = rig->state.current_vfo;
		retcode = caps->set_vfo(rig, vfo);
		if (retcode != RIG_OK)
				return retcode;

		retcode = caps->get_mem(rig, vfo, ch);
		caps->set_vfo(rig, curr_vfo);
		return retcode;
}

/**
 *      rig_mv_ctl - perform Memory/VFO operations
 *      @rig:	The rig handle
 *      @vfo:	The target VFO
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

int rig_mv_ctl(RIG *rig, vfo_t vfo, mv_op_t op)
{
		const struct rig_caps *caps;
		int retcode;
		vfo_t curr_vfo;

		if (!rig || !rig->caps)
			return -RIG_EINVAL;

		caps = rig->caps;

		if (caps->mv_ctl == NULL)
			return -RIG_ENAVAIL;

		if (caps->targetable_vfo || vfo == RIG_VFO_CURR ||
										vfo == rig->state.current_vfo)
			return caps->mv_ctl(rig, vfo, op);

		if (!caps->set_vfo)
			return -RIG_ENTARGET;
		curr_vfo = rig->state.current_vfo;
		retcode = caps->set_vfo(rig, vfo);
		if (retcode != RIG_OK)
				return retcode;

		retcode = caps->mv_ctl(rig, vfo, op);
		caps->set_vfo(rig, curr_vfo);
		return retcode;
}

/**
 *      rig_send_dtmf - send DTMF digits
 *      @rig:	The rig handle
 *      @vfo:	The target VFO
 *      @digits:	Digits to be send
 *
 *      The rig_send_dtmf() function sends DTMF digits.
 *      See DTMF change speed, etc. (TODO).
 *
 *      RETURN VALUE: The rig_send_dtmf() function returns %RIG_OK
 *      if the operation has been sucessful, or a negative value
 *      if an error occured (in which case, cause is set appropriately).
 *
 */

int rig_send_dtmf(RIG *rig, vfo_t vfo, const char *digits)
{
		const struct rig_caps *caps;
		int retcode;
		vfo_t curr_vfo;

		if (!rig || !rig->caps || !digits)
			return -RIG_EINVAL;

		caps = rig->caps;

		if (caps->send_dtmf == NULL)
			return -RIG_ENAVAIL;

		if (caps->targetable_vfo || vfo == RIG_VFO_CURR ||
										vfo == rig->state.current_vfo)
			return caps->send_dtmf(rig, vfo, digits);

		if (!caps->set_vfo)
			return -RIG_ENTARGET;
		curr_vfo = rig->state.current_vfo;
		retcode = caps->set_vfo(rig, vfo);
		if (retcode != RIG_OK)
				return retcode;

		retcode = caps->send_dtmf(rig, vfo, digits);
		caps->set_vfo(rig, curr_vfo);
		return retcode;
}

/**
 *      rig_recv_dtmf - receive DTMF digits
 *      @rig:	The rig handle
 *      @vfo:	The target VFO
 *      @digits:	Location where the digits are to be stored 
 *      @length:	in: max length of buffer, out: number really read.
 *
 *      The rig_recv_dtmf() function receive DTMF digits (not blocking).
 *      See DTMF change speed, etc. (TODO).
 *
 *      RETURN VALUE: The rig_recv_dtmf() function returns %RIG_OK
 *      if the operation has been sucessful, or a negative value
 *      if an error occured (in which case, cause is set appropriately).
 *
 */

int rig_recv_dtmf(RIG *rig, vfo_t vfo, char *digits, int *length)
{
		const struct rig_caps *caps;
		int retcode;
		vfo_t curr_vfo;

		if (!rig || !rig->caps || !digits || !length)
			return -RIG_EINVAL;

		caps = rig->caps;

		if (caps->recv_dtmf == NULL)
			return -RIG_ENAVAIL;

		if (caps->targetable_vfo || vfo == RIG_VFO_CURR ||
										vfo == rig->state.current_vfo)
			return caps->recv_dtmf(rig, vfo, digits, length);

		if (!caps->set_vfo)
			return -RIG_ENTARGET;
		curr_vfo = rig->state.current_vfo;
		retcode = caps->set_vfo(rig, vfo);
		if (retcode != RIG_OK)
				return retcode;

		retcode = caps->recv_dtmf(rig, vfo, digits, length);
		caps->set_vfo(rig, curr_vfo);
		return retcode;
}

/**
 *      rig_send_morse - send morse code
 *      @rig:	The rig handle
 *      @vfo:	The target VFO
 *      @msg:	Message to be sent
 *
 *      The rig_send_morse() function sends morse message.
 *      See keyer change speed, etc. (TODO).
 *
 *      RETURN VALUE: The rig_send_morse() function returns %RIG_OK
 *      if the operation has been sucessful, or a negative value
 *      if an error occured (in which case, cause is set appropriately).
 *
 */

int rig_send_morse(RIG *rig, vfo_t vfo, const char *msg)
{
		const struct rig_caps *caps;
		int retcode;
		vfo_t curr_vfo;

		if (!rig || !rig->caps || !msg)
			return -RIG_EINVAL;

		caps = rig->caps;

		if (caps->send_morse == NULL)
			return -RIG_ENAVAIL;

		if (caps->targetable_vfo || vfo == RIG_VFO_CURR ||
										vfo == rig->state.current_vfo)
			return caps->send_morse(rig, vfo, msg);

		if (!caps->set_vfo)
			return -RIG_ENTARGET;
		curr_vfo = rig->state.current_vfo;
		retcode = caps->set_vfo(rig, vfo);
		if (retcode != RIG_OK)
				return retcode;

		retcode = caps->send_morse(rig, vfo, msg);
		caps->set_vfo(rig, curr_vfo);
		return retcode;
}


/**
 *      rig_set_bank - set the current memory bank
 *      @rig:	The rig handle
 *      @vfo:	The target VFO
 *      @bank:	The memory bank
 *
 *      The rig_set_bank() function sets the current memory bank.
 *      It is not mandatory for the radio to be in memory mode. Actually
 *      it depends on rigs. YMMV.
 *
 *      RETURN VALUE: The rig_set_bank() function returns %RIG_OK
 *      if the operation has been sucessful, or a negative value
 *      if an error occured (in which case, cause is set appropriately).
 *
 *      SEE ALSO: rig_set_mem()
 */

int rig_set_bank(RIG *rig, vfo_t vfo, int bank)
{
		const struct rig_caps *caps;
		int retcode;
		vfo_t curr_vfo;

		if (!rig || !rig->caps)
			return -RIG_EINVAL;

		caps = rig->caps;

		if (caps->set_bank == NULL)
			return -RIG_ENAVAIL;
		
		if (caps->targetable_vfo || vfo == RIG_VFO_CURR ||
										vfo == rig->state.current_vfo)
			return caps->set_bank(rig, vfo, bank);

		if (!caps->set_vfo)
			return -RIG_ENTARGET;
		curr_vfo = rig->state.current_vfo;
		retcode = caps->set_vfo(rig, vfo);
		if (retcode != RIG_OK)
				return retcode;

		retcode = caps->set_bank(rig, vfo, bank);
		caps->set_vfo(rig, curr_vfo);
		return retcode;
}


int rig_save_channel(RIG *rig, channel_t *chan)
{
  int i;
  int chan_num;

  chan_num = chan->channel_num;
  memset((void*)chan, 0, sizeof(channel_t));
  chan->channel_num = chan_num;

  rig_get_vfo(rig, &chan->vfo);
  rig_get_freq(rig, RIG_VFO_CURR, &chan->freq);
  rig_get_mode(rig, RIG_VFO_CURR, &chan->mode, &chan->width);
  rig_get_split(rig, RIG_VFO_CURR, &chan->split);
  if (chan->split != RIG_SPLIT_OFF) {
  	rig_get_split_freq(rig, RIG_VFO_CURR, &chan->freq, &chan->tx_freq);
  	rig_get_split_mode(rig, RIG_VFO_CURR, &chan->mode, &chan->width, 
					&chan->tx_mode, &chan->tx_width);
  }
  rig_get_rptr_shift(rig, RIG_VFO_CURR, &chan->rptr_shift);
  rig_get_rptr_offs(rig, RIG_VFO_CURR, &chan->rptr_offs);

  for (i=0; i<RIG_SETTING_MAX; i++)
  	rig_get_level(rig, RIG_VFO_CURR, rig_idx2setting(i), &chan->levels[i]);

  rig_get_ant(rig, RIG_VFO_CURR, &chan->ant);
  rig_get_ts(rig, RIG_VFO_CURR, &chan->tuning_step);
  rig_get_rit(rig, RIG_VFO_CURR, &chan->rit);
  rig_get_xit(rig, RIG_VFO_CURR, &chan->xit);

  chan->funcs = 0;
  for (i=0; i<RIG_SETTING_MAX; i++) {
  	int fstatus;
  	rig_get_func(rig, RIG_VFO_CURR, rig_idx2setting(i), &fstatus);
	chan->funcs |= fstatus? rig_idx2setting(i) : 0;
  }

  rig_get_ctcss(rig, RIG_VFO_CURR, &chan->ctcss);
  rig_get_ctcss_sql(rig, RIG_VFO_CURR, &chan->ctcss_sql);
  rig_get_dcs(rig, RIG_VFO_CURR, &chan->dcs);
  rig_get_dcs_sql(rig, RIG_VFO_CURR, &chan->dcs_sql);
/* rig_get_mem_name(rig, RIG_VFO_CURR, chan->channel_desc); */

	return RIG_OK;
}

/*
 * restore_channel data of current VFO/mem (does not save context!)
 * Assumes rig!=NULL, rig->state.priv!=NULL, chan!=NULL
 * TODO: still a WIP --SF
 *
 * missing: rptr_shift, rptr_offs, split (freq&mode),xit
 * level missing: AF, RF, SQL, IF, APF, NR, PBT_IN, PBT_OUT, CWPITCH, MICGAIN, etc.
 * TODO: error checking
 *
 * set_channel and get_channel should save and restore the context of RIG_VFO_CURR 
 *  before and afterhand, plus select the right channel.
 *  xet_channel can operate on VFO ?
 * set_channel:
 *	save_channel(&curr_chan)
 *	rig_mv_ctl(rig, RIG_VFO_CURR, RIG_MVOP_MEM_MODE);
 *	rig_set_mem(rig, RIG_VFO_CURR, chan->channel_num);
 *	restore_channel(chan)
 *	restore_channel(&curr_chan)
 *
 * get_channel:
 *	save_channel(&curr_chan)
 *	rig_mv_ctl(rig, RIG_VFO_CURR, RIG_MVOP_MEM_MODE);
 *	rig_set_mem(rig, RIG_VFO_CURR, chan->channel_num);
 *	save_channel(chan)
 *	restore_channel(&curr_chan)
 */
int rig_restore_channel(RIG *rig, const channel_t *chan)
{
  int i;

  rig_set_vfo(rig, chan->vfo);	/* huh!? */
  rig_set_freq(rig, RIG_VFO_CURR, chan->freq);
  rig_set_mode(rig, RIG_VFO_CURR, chan->mode, chan->width);
  rig_set_split(rig, RIG_VFO_CURR, chan->split);
  if (chan->split != RIG_SPLIT_OFF) {
  	rig_set_split_freq(rig, RIG_VFO_CURR, chan->freq, chan->tx_freq);
  	rig_set_split_mode(rig, RIG_VFO_CURR, chan->mode, chan->width, 
					chan->tx_mode, chan->tx_width);
  }
  rig_set_rptr_shift(rig, RIG_VFO_CURR, chan->rptr_shift);
  rig_set_rptr_offs(rig, RIG_VFO_CURR, chan->rptr_offs);
#if 0
   /* power in mW */
  rig_mW2power(rig, &hfpwr, chan->power, chan->freq, chan->mode);
  rig_set_level(rig, RIG_VFO_CURR, RIG_LEVEL_RFPOWER, hfpwr);
  rig_set_level(rig, RIG_VFO_CURR, RIG_LEVEL_ATT, chan->att);
  rig_set_level(rig, RIG_VFO_CURR, RIG_LEVEL_PREAMP, chan->preamp);
#else
  for (i=0; i<RIG_SETTING_MAX; i++)
  	rig_set_level(rig, RIG_VFO_CURR, rig_idx2setting(i), chan->levels[i]);
#endif

  rig_set_ant(rig, RIG_VFO_CURR, chan->ant);
  rig_set_ts(rig, RIG_VFO_CURR, chan->tuning_step);
  rig_set_rit(rig, RIG_VFO_CURR, chan->rit);
  rig_set_xit(rig, RIG_VFO_CURR, chan->xit);

  for (i=0; i<RIG_SETTING_MAX; i++)
  	rig_set_func(rig, RIG_VFO_CURR, rig_idx2setting(i), 
					chan->funcs & rig_idx2setting(i));

  rig_set_ctcss(rig, RIG_VFO_CURR, chan->ctcss);
  rig_set_ctcss_sql(rig, RIG_VFO_CURR, chan->ctcss_sql);
  rig_set_dcs(rig, RIG_VFO_CURR, chan->dcs);
  rig_set_dcs_sql(rig, RIG_VFO_CURR, chan->dcs_sql);
/* rig_set_mem_name(rig, RIG_VFO_CURR, chan->channel_desc); */

	return RIG_OK;
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
		channel_t curr_chan;
		int curr_chan_num;

		if (!rig || !rig->caps || !chan)
			return -RIG_EINVAL;

		/*
		 * if not available, emulate it
		 */
		if (rig->caps->set_channel == NULL) {
 			rig_save_channel(rig, &curr_chan);
			rig_mv_ctl(rig, RIG_VFO_CURR, RIG_MVOP_MEM_MODE);
			rig_get_mem(rig, RIG_VFO_CURR, &curr_chan_num);
			rig_set_mem(rig, RIG_VFO_CURR, chan->channel_num);
			rig_set_mem(rig, RIG_VFO_CURR, curr_chan_num);
			rig_restore_channel(rig, chan);
			rig_mv_ctl(rig, RIG_VFO_CURR, RIG_MVOP_VFO_MODE);
			rig_restore_channel(rig, &curr_chan);
 			return RIG_OK;
		}

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
		channel_t curr_chan;
		int curr_chan_num;

		if (!rig || !rig->caps || !chan)
			return -RIG_EINVAL;

		/*
		 * if not available, emulate it
		 */
		if (rig->caps->get_channel == NULL) {
#if 0
			rig_save_channel(rig, &curr_chan);
#endif
			rig_mv_ctl(rig, RIG_VFO_CURR, RIG_MVOP_MEM_MODE);
#if 0
			rig_get_mem(rig, RIG_VFO_CURR, &curr_chan_num);
#endif
			rig_set_mem(rig, RIG_VFO_CURR, chan->channel_num);
			rig_save_channel(rig, chan);
#if 0
			rig_set_mem(rig, RIG_VFO_CURR, curr_chan_num);
			rig_restore_channel(rig, &curr_chan);
#endif

 			return RIG_OK;
		}

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
		if (freq >= range_list[i].start && freq <= range_list[i].end &&
				(range_list[i].modes & mode)) {
				return &range_list[i];
		}
	}
	return NULL;
}


/**
 *      rig_set_trn - control the transceive mode 
 *      @rig:	The rig handle
 *      @vfo:	The target VFO
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

int rig_set_trn(RIG *rig, vfo_t vfo, int trn)
{
		const struct rig_caps *caps;
		int status;

		if (!rig || !rig->caps)
			return -RIG_EINVAL;

		caps = rig->caps;

		if (rig->state.transceive == RIG_TRN_OFF)
			return -RIG_ENAVAIL;

		if (trn == RIG_TRN_RIG) {
			if (rig->state.transceive) {
				/*
				 * TODO: check error codes et al.
				 */
				status = add_trn_rig(rig);
				if (caps->set_trn)
						return caps->set_trn(rig, vfo, RIG_TRN_RIG);
				else
						return status;
			} else {
				return -RIG_ECONF;
			}
		} else {
				status = remove_trn_rig(rig);
				if (caps->set_trn)
						return caps->set_trn(rig, vfo, RIG_TRN_OFF);
				else
						return status;
		}

		return RIG_OK;
}

/**
 *      rig_get_trn - get the current transceive mode
 *      @rig:	The rig handle
 *      @vfo:	The target VFO
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
int rig_get_trn(RIG *rig, vfo_t vfo, int *trn)
{
		if (!rig || !rig->caps || !trn)
			return -RIG_EINVAL;

		if (rig->caps->get_trn == NULL)
			return -RIG_ENAVAIL;

		return rig->caps->get_trn(rig, vfo, trn);
}

/**
 *      rig_get_info - get general information from the radio
 *      @rig:	The rig handle
 *
 *      The rig_get_info() function retrieves some general information
 *      from the radio. This can include firmware revision, exact
 *      model name, or just nothing. 
 *
 *      RETURN VALUE: The rig_get_info() function returns a pointer
 *      to freshly allocated memory containing the ASCIIZ string 
 *      if the operation has been sucessful, or NULL 
 *      if an error occured.
 */
const char* rig_get_info(RIG *rig)
{
		if (!rig || !rig->caps)
			return NULL;

		if (rig->caps->get_info == NULL)
			return NULL;

		return rig->caps->get_info(rig);
}


/**
 *      rig_setting2idx - basically convert setting_t expressed 2^n to n
 *      @s:	The setting to convert
 *
 *      The rig_setting2idx() function converts a setting_t value expressed
 *      by 2^n to the value of n.
 *
 *      RETURN VALUE: The index such that 2^n is the setting, otherwise 0
 *      if the setting was not found.
 */
int rig_setting2idx(setting_t s)
{
		int i;

		for (i = 0; i<RIG_SETTING_MAX; i++)
				if (s & rig_idx2setting(i))
						return i;

		return 0;
}


