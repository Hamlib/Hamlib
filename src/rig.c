/**
 * \file src/rig.c
 * \ingroup rig
 * \brief Ham Radio Control Libraries interface
 * \author Stephane Fillod
 * \author Frank Singleton
 * \date 2000-2002
 *
 * Hamlib interface is a frontend implementing wrapper functions.
 */

/*
 *  Hamlib Interface - main file
 *  Copyright (c) 2000-2002 by Stephane Fillod and Frank Singleton
 *
 *	$Id: rig.c,v 1.63 2002-09-24 21:42:56 fillods Exp $
 *
 *   This library is free software; you can redistribute it and/or modify
 *   it under the terms of the GNU Library General Public License as
 *   published by the Free Software Foundation; either version 2 of
 *   the License, or (at your option) any later version.
 *
 *   This program is distributed in the hope that it will be useful,
 *   but WITHOUT ANY WARRANTY; without even the implied warranty of
 *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *   GNU Library General Public License for more details.
 *
 *   You should have received a copy of the GNU Library General Public
 *   License along with this library; if not, write to the Free Software
 *   Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 *
 */

/*! \page rig Rig (radio) interface
 *
 * Although the word rig can stand for lot of thinfs, we are
 * understanding it as general remote controllable radio equipment,
 * with a so-called tunable VFO.
 */

/**
 * \example ../tests/testrig.c
 * \anchor example_test_rig
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
#include <serial.h>
#include "event.h"
#include "conf.h"
#include "token.h"

/**
 * \brief Hamlib release number
 */
const char hamlib_version[] = "Hamlib version " VERSION;

/**
 * \brief Hamlib copyright notice
 */
const char hamlib_copyright[] = 
  "Copyright (C) 2000, 2001, 2002 Stephane Fillod and Frank Singleton\n"
  "This is free software; see the source for copying conditions.  There is NO\n"
  "warranty; not even for MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.";


#ifndef DOC_HIDDEN

#define DEFAULT_SERIAL_PORT "/dev/ttyS0"

#define CHECK_RIG_ARG(r) (!(r) || !(r)->caps || !(r)->state.comm_state)

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
		"Communication bus error",
		"Communication bus collision",
		NULL,
};

#define ERROR_TBL_SZ (sizeof(rigerror_table)/sizeof(char *))

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
 * \brief execs cfunc() on each opened rig
 * \param cfunc	The function to be executed on each rig
 * \param data	Data pointer to be passed to cfunc() 
 *
 *  Calls cfunc() function for each opened rig. 
 *  The contents of the opened rig table
 *  is processed in random order according to a function
 *  pointed to by \a cfunc, whic is called with two arguments,
 *  the first pointing to the RIG handle, the second
 *  to a data pointer \a data.
 *  If \a data is not needed, then it can be set to NULL.
 *  The processing of the opened rig table is stopped
 *  when cfunc() returns 0.
 * \internal
 *
 * \return always RIG_OK.
 */

int foreach_opened_rig(int (*cfunc)(RIG *, rig_ptr_t), rig_ptr_t data)
{	
	struct opened_rig_l *p;

	for (p=opened_rig_list; p; p=p->next) {
			if ((*cfunc)(p->rig,data) == 0)
					return RIG_OK;
	}
	return RIG_OK;
}

#endif /* !DOC_HIDDEN */

/**
 * \brief get string describing the error code
 * \param errnum	The error code
 * \return the appropriate description string, ortherwise a NULL pointer 
 * if the error code is unknown.
 *
 * Returns a string describing the error code passed in the argument \a errnum.
 *
 * \todo support gettext/localization
 */
const char *rigerror(int errnum)
{
		errnum = abs(errnum);
		if (errnum > ERROR_TBL_SZ)
			return NULL;
		return rigerror_table[errnum];
}

/**
 * \brief allocate a new RIG handle
 * \param rig_model	The rig model for this new handle
 *
 * Allocates a new \link ::RIG \endlink handle and initializes the associated data 
 * for \a rig_model.
 *
 * \return a pointer to the #RIG handle otherwise NULL if memory allocation
 * failed or \a rig_model is unknown (e.g. backend autoload failed).
 *
 * \sa rig_cleanup(), rig_open()
 */

RIG *rig_init(rig_model_t rig_model)
{
		RIG *rig;
		const struct rig_caps *caps;
		struct rig_state *rs;
		int i, retcode;

		rig_debug(RIG_DEBUG_VERBOSE,"rig:rig_init called \n");

		rig_check_backend(rig_model);

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

		rs->comm_state = 0;
		rs->rigport.type.rig = caps->port_type; /* default from caps */
		strncpy(rs->rigport.pathname, DEFAULT_SERIAL_PORT, FILPATHLEN);
		rs->rigport.parm.serial.rate = caps->serial_rate_max;	/* fastest ! */
		rs->rigport.parm.serial.data_bits = caps->serial_data_bits;
		rs->rigport.parm.serial.stop_bits = caps->serial_stop_bits;
		rs->rigport.parm.serial.parity = caps->serial_parity;
		rs->rigport.parm.serial.handshake = caps->serial_handshake;
		rs->rigport.write_delay = caps->write_delay;
		rs->rigport.post_write_delay = caps->post_write_delay;

		rs->rigport.timeout = caps->timeout;
		rs->rigport.retry = caps->retry;
		rs->pttport.type.ptt = caps->ptt_type;
		rs->dcdport.type.dcd = caps->dcd_type;

		rs->vfo_comp = 0.0;	/* override it with preferences */
		rs->current_vfo = RIG_VFO_CURR;	/* we don't know yet! */
		rs->transceive = caps->transceive;
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
		for (i=0; i<FRQRANGESIZ && !RIG_IS_FRNG_END(rs->rx_range_list[i]); i++) {
					rs->vfo_list |= rs->rx_range_list[i].vfo;
		}
		for (i=0; i<FRQRANGESIZ && !RIG_IS_FRNG_END(rs->tx_range_list[i]); i++) {
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

		rs->rigport.fd = rs->pttport.fd = rs->dcdport.fd = -1;

		/* 
		 * let the backend a chance to setup his private data
		 * This must be done only once defaults are setup,
		 * so the backend init can override rig_state.
		 */
		if (caps->rig_init != NULL) {
				retcode = caps->rig_init(rig);
				if (retcode != RIG_OK) {
						rig_debug(RIG_DEBUG_VERBOSE,"rig:backend_init failed!\n");
						/* cleanup and exit */
						free(rig);
						return NULL;
				}
		}

		return rig;
}

/**
 * \brief open the communication to the rig
 * \param rig	The #RIG handle of the radio to be opened
 *
 * Opens communication to a radio which \a RIG handle has been passed
 * by argument.
 *
 * \return RIG_OK if the operation has been sucessful, otherwise 
 * a negative value if an error occured (in which case, cause is 
 * set appropriately).
 *
 * \retval RIG_EINVAL	\a rig is NULL or unconsistent.
 * \retval RIG_ENIMPL	port type communication is not implemented yet.
 *
 * \sa rig_init(), rig_close()
 */

int rig_open(RIG *rig)
{
		const struct rig_caps *caps;
		struct rig_state *rs;
		int status;
		vfo_t vfo;

		rig_debug(RIG_DEBUG_VERBOSE,"rig:rig_open called \n");

		if (!rig || !rig->caps)
				return -RIG_EINVAL;

		caps = rig->caps;
		rs = &rig->state;

		if (rs->comm_state)
				return -RIG_EINVAL;

		rs->rigport.fd = -1;

		switch(rs->rigport.type.rig) {
		case RIG_PORT_SERIAL:
				status = serial_open(&rs->rigport);
				if (status != 0)
						return status;
				break;

		case RIG_PORT_DEVICE:
				status = open(rs->rigport.pathname, O_RDWR, 0);
				if (status < 0)
						return -RIG_EIO;
				rs->rigport.fd = status;
				break;

		case RIG_PORT_NONE:
		case RIG_PORT_RPC:
				break;	/* ez :) */

		case RIG_PORT_NETWORK:	/* not implemented yet! */
				return -RIG_ENIMPL;
		default:
				return -RIG_EINVAL;
		}

		/*
		 * FIXME: what to do if PTT open fails or PTT unsupported?
		 * 			fail rig_open?  remember unallocating..
		 */
		switch(rs->pttport.type.ptt) {
		case RIG_PTT_NONE:
		case RIG_PTT_RIG:
				break;
		case RIG_PTT_SERIAL_RTS:
		case RIG_PTT_SERIAL_DTR:
				rs->pttport.fd = ser_open(&rs->pttport);
				if (rs->pttport.fd < 0)
					rig_debug(RIG_DEBUG_ERR, "Cannot open PTT device \"%s\"\n",
								rs->pttport.pathname);
				break;
		case RIG_PTT_PARALLEL:
				rs->pttport.fd = par_open(&rs->pttport);
				if (rs->pttport.fd < 0)
					rig_debug(RIG_DEBUG_ERR, "Cannot open PTT device \"%s\"\n",
								rs->pttport.pathname);
				break;
		default:
				rig_debug(RIG_DEBUG_ERR, "Unsupported PTT type %d\n",
								rs->pttport.type.ptt);
		}

		switch(rs->dcdport.type.dcd) {
		case RIG_DCD_NONE:
		case RIG_DCD_RIG:
				break;
		case RIG_DCD_SERIAL_DSR:
		case RIG_DCD_SERIAL_CTS:
				rs->dcdport.fd = ser_open(&rs->dcdport);
				if (rs->dcdport.fd < 0)
					rig_debug(RIG_DEBUG_ERR, "Cannot open DCD device \"%s\"\n",
								rs->dcdport.pathname);
				break;
		case RIG_DCD_PARALLEL:
				rs->dcdport.fd = par_open(&rs->dcdport);
				if (rs->dcdport.fd < 0)
					rig_debug(RIG_DEBUG_ERR, "Cannot open DCD device \"%s\"\n",
								rs->dcdport.pathname);
				break;
		default:
				rig_debug(RIG_DEBUG_ERR, "Unsupported DCD type %d\n",
								rs->dcdport.type.dcd);
		}

		add_opened_rig(rig);

		rs->comm_state = 1;

		/* 
		 * Maybe the backend has something to initialize
		 * In case of failure, just close down and report error code.
		 */
		if (caps->rig_open != NULL) {
				status = caps->rig_open(rig);	
				if (status != RIG_OK) {
						rig_close(rig);
						return status;
				}
		}

		/*
		 * trigger state->current_vfo first retrieval
		 */
        rig_get_vfo(rig, &vfo);

#if 0
        /*
         * Check the current tranceive state of the rig
         */
        if (rs->transceive == RIG_TRN_RIG) {
            int retval, trn;
            retval = rig_get_trn(rig, &trn);
            if (retval == RIG_OK && trn == RIG_TRN_RIG)
                add_trn_rig(rig);
        }
#endif
		return RIG_OK;
}

/**
 * \brief close the communication to the rig
 * \param rig	The #RIG handle of the radio to be closed
 *
 * Closes communication to a radio which \a RIG handle has been passed
 * by argument that was previously open with rig_open().
 *
 * \return RIG_OK if the operation has been sucessful, otherwise 
 * a negative value if an error occured (in which case, cause is 
 * set appropriately).
 *
 * \sa rig_cleanup(), rig_open()
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

		if (!rs->comm_state)
				return -RIG_EINVAL;

		if (rs->transceive) {
				/*
				 * TODO: check error codes et al.
				 */
				remove_trn_rig(rig);
		}

		/*
		 * Let the backend say 73s to the rig.
		 * and ignore the return code.
		 */
		if (caps->rig_close)
				caps->rig_close(rig);

		/* 
		 * FIXME: what happens if PTT and rig ports are the same?
		 * 			(eg. ptt_type = RIG_PTT_SERIAL)
		 */
		switch(rs->pttport.type.ptt) {
		case RIG_PTT_NONE:
		case RIG_PTT_RIG:
				break;
		case RIG_PTT_SERIAL_RTS:
		case RIG_PTT_SERIAL_DTR:
				ser_close(&rs->pttport);
				break;
		case RIG_PTT_PARALLEL:
				par_close(&rs->pttport);
				break;
		default:
				rig_debug(RIG_DEBUG_ERR, "Unsupported PTT type %d\n",
								rs->pttport.type.ptt);
		}

		switch(rs->dcdport.type.dcd) {
		case RIG_DCD_NONE:
		case RIG_DCD_RIG:
				break;
		case RIG_DCD_SERIAL_DSR:
		case RIG_DCD_SERIAL_CTS:
				ser_close(&rs->dcdport);
				break;
		case RIG_DCD_PARALLEL:
				par_close(&rs->dcdport);
				break;
		default:
				rig_debug(RIG_DEBUG_ERR, "Unsupported DCD type %d\n",
								rs->dcdport.type.dcd);
		}

		rs->dcdport.fd = rs->pttport.fd = -1;

		if (rs->rigport.fd != -1) {
				if (!rs->rigport.stream)
						fclose(rs->rigport.stream); /* this closes also fd */
				else
					close(rs->rigport.fd);
				rs->rigport.fd = -1;
				rs->rigport.stream = NULL;
		}

		remove_opened_rig(rig);

		rs->comm_state = 0;

		return RIG_OK;
}

/**
 * \brief release a rig handle and free associated memory
 * \param rig	The #RIG handle of the radio to be closed
 *
 * Releases a rig struct which port has eventualy been closed already 
 * with rig_close().
 *
 * \return RIG_OK if the operation has been sucessful, otherwise 
 * a negative value if an error occured (in which case, cause is 
 * set appropriately).
 *
 * \sa rig_init(), rig_close()
 */

int rig_cleanup(RIG *rig)
{
		rig_debug(RIG_DEBUG_VERBOSE,"rig:rig_cleanup called \n");

		if (!rig || !rig->caps)
				return -RIG_EINVAL;

		/*
		 * check if they forgot to close the rig
		 */
		if (rig->state.comm_state)
				rig_close(rig);

		/*
		 * basically free up the priv struct 
		 */
		if (rig->caps->rig_cleanup)
				rig->caps->rig_cleanup(rig);

		free(rig);

		return RIG_OK;
}


/**
 * \brief set the frequency of the target VFO
 * \param rig	The rig handle
 * \param vfo	The target VFO
 * \param freq	The frequency to set to
 *
 * Sets the frequency of the target VFO.
 *
 * \return RIG_OK if the operation has been sucessful, otherwise 
 * a negative value if an error occured (in which case, cause is 
 * set appropriately).
 *
 * \sa rig_get_freq()
 */

int rig_set_freq(RIG *rig, vfo_t vfo, freq_t freq)
{
		const struct rig_caps *caps;
		int retcode;
		vfo_t curr_vfo;

		if (CHECK_RIG_ARG(rig))
			return -RIG_EINVAL;

		caps = rig->caps;

		if (rig->state.vfo_comp != 0.0)
				freq += (freq_t)((double)rig->state.vfo_comp * freq);

		if (caps->set_freq == NULL)
			return -RIG_ENAVAIL;

		if ((caps->targetable_vfo&RIG_TARGETABLE_FREQ) || 
						vfo == RIG_VFO_CURR || vfo == rig->state.current_vfo)
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
 * \brief get the frequency of the target VFO
 * \param rig	The rig handle
 * \param vfo	The target VFO
 * \param freq	The location where to store the current frequency
 *
 *  Retrieves the frequency of the target VFO.
 *	The value stored at \a freq location equals RIG_FREQ_NONE when the current
 *	frequency of the VFO is not defined (e.g. blank memory).
 *
 * \return RIG_OK if the operation has been sucessful, otherwise 
 * a negative value if an error occured (in which case, cause is 
 * set appropriately).
 *
 * \sa rig_set_freq()
 */

int rig_get_freq(RIG *rig, vfo_t vfo, freq_t *freq)
{
		const struct rig_caps *caps;
		int retcode;
		vfo_t curr_vfo;

		if (CHECK_RIG_ARG(rig) || !freq)
			return -RIG_EINVAL;

		caps = rig->caps;

		if (caps->get_freq == NULL)
			return -RIG_ENAVAIL;

		if ((caps->targetable_vfo&RIG_TARGETABLE_FREQ) || 
				vfo == RIG_VFO_CURR || vfo == rig->state.current_vfo) {
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
 * \brief set the mode of the target VFO
 * \param rig	The rig handle
 * \param vfo	The target VFO
 * \param mode	The mode to set to
 * \param width	The passband width to set to
 *
 * Sets the mode and associated passband of the target VFO.
 * The passband \a width must be supported by the backend of the rig.
 *
 * \return RIG_OK if the operation has been sucessful, otherwise 
 * a negative value if an error occured (in which case, cause is 
 * set appropriately).
 *
 * \sa rig_get_mode()
 */

int rig_set_mode(RIG *rig, vfo_t vfo, rmode_t mode, pbwidth_t width)
{
		const struct rig_caps *caps;
		int retcode;
		vfo_t curr_vfo;

		if (CHECK_RIG_ARG(rig))
			return -RIG_EINVAL;

		caps = rig->caps;

		if (caps->set_mode == NULL)
			return -RIG_ENAVAIL;

		if ((caps->targetable_vfo&RIG_TARGETABLE_ALL) ||
				vfo == RIG_VFO_CURR || vfo == rig->state.current_vfo)
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
 * \brief get the mode of the target VFO
 * \param rig	The rig handle
 * \param vfo	The target VFO
 * \param mode	The location where to store the current mode
 * \param width	The location where to store the current passband width
 *
 *  Retrieves the mode and passband of the target VFO.
 *  If the backend is unable to determine the width, the \a width
 *  will be set to RIG_PASSBAND_NORMAL as a default.
 *	The value stored at \a mode location equals RIG_MODE_NONE when the current
 *	mode of the VFO is not defined (e.g. blank memory).
 *
 * \return RIG_OK if the operation has been sucessful, otherwise 
 * a negative value if an error occured (in which case, cause is 
 * set appropriately).
 *
 * \sa rig_set_mode()
 */

int rig_get_mode(RIG *rig, vfo_t vfo, rmode_t *mode, pbwidth_t *width)
{
		const struct rig_caps *caps;
		int retcode;
		vfo_t curr_vfo;

		if (CHECK_RIG_ARG(rig) || !mode || !width)
			return -RIG_EINVAL;

		caps = rig->caps;

		if (caps->get_mode == NULL)
			return -RIG_ENAVAIL;

		if ((caps->targetable_vfo&RIG_TARGETABLE_ALL) || 
						vfo == RIG_VFO_CURR || vfo == rig->state.current_vfo)
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
 * \brief get the normal passband of a mode
 * \param rig	The rig handle
 * \param mode	The mode to get the passband
 *
 *  Returns the normal (default) passband for the given \a mode.
 *
 * \return the passband in Hz if the operation has been sucessful,
 * or a 0 if an error occured (passband not found, whatever).
 *
 * \sa rig_passband_narrow(), rig_passband_wide()
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
 * \brief get the narrow passband of a mode
 * \param rig	The rig handle
 * \param mode	The mode to get the passband
 *
 *  Returns the narrow (closest) passband for the given \a mode.
 *	EXAMPLE: rig_set_mode(my_rig, RIG_MODE_LSB, 
 *				 			rig_passband_narrow(my_rig, RIG_MODE_LSB) );
 *
 * \return the passband in Hz if the operation has been sucessful,
 * or a 0 if an error occured (passband not found, whatever).
 *
 * \sa rig_passband_normal(), rig_passband_wide()
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
 * \brief get the wide passband of a mode
 * \param rig	The rig handle
 * \param mode	The mode to get the passband
 *
 *  Returns the wide (default) passband for the given \a mode.
 *	EXAMPLE: rig_set_mode(my_rig, RIG_MODE_AM, 
 *				 			rig_passband_wide(my_rig, RIG_MODE_AM) );
 *
 * \return the passband in Hz if the operation has been sucessful,
 * or a 0 if an error occured (passband not found, whatever).
 *
 * \sa rig_passband_narrow(), rig_passband_normal()
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
 * \brief set the current VFO
 * \param rig	The rig handle
 * \param vfo	The VFO to set to
 *
 *  Sets the current VFO. The VFO can be RIG_VFO_A, RIG_VFO_B, RIG_VFO_C 
 *  for VFOA, VFOB, VFOC respectively or RIG_VFO_MEM for Memory mode.
 *  Supported VFOs depends on rig capabilities.
 *
 * \return RIG_OK if the operation has been sucessful, otherwise 
 * a negative value if an error occured (in which case, cause is 
 * set appropriately).
 *
 * \sa rig_get_vfo()
 */

int rig_set_vfo(RIG *rig, vfo_t vfo)
{
		const struct rig_caps *caps;
		int retcode;

		if (CHECK_RIG_ARG(rig))
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
 * \brief get the current VFO
 * \param rig	The rig handle
 * \param vfo	The location where to store the current VFO
 *
 *  Retrieves the current VFO. The VFO can be RIG_VFO_A, RIG_VFO_B, RIG_VFO_C 
 *  for VFOA, VFOB, VFOC respectively or RIG_VFO_MEM for Memory mode.
 *  Supported VFOs depends on rig capabilities.
 *
 * \return RIG_OK if the operation has been sucessful, otherwise 
 * a negative value if an error occured (in which case, cause is 
 * set appropriately).
 *
 * \sa rig_set_vfo()
 */

int rig_get_vfo(RIG *rig, vfo_t *vfo)
{
		const struct rig_caps *caps;
		int retcode;

		if (CHECK_RIG_ARG(rig) || !vfo)
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
 * \brief set PTT on/off
 * \param rig	The rig handle
 * \param vfo	The target VFO
 * \param ptt	The PTT status to set to
 *
 *  Sets "Push-To-Talk" on/off.
 *
 * \return RIG_OK if the operation has been sucessful, otherwise 
 * a negative value if an error occured (in which case, cause is 
 * set appropriately).
 *
 * \sa rig_get_ptt()
 */
int rig_set_ptt(RIG *rig, vfo_t vfo, ptt_t ptt)
{
		const struct rig_caps *caps;
		int retcode;
		vfo_t curr_vfo;

		if (CHECK_RIG_ARG(rig))
			return -RIG_EINVAL;

		caps = rig->caps;

		switch (rig->state.pttport.type.ptt) {
		case RIG_PTT_RIG:
			if (caps->set_ptt == NULL)
				return -RIG_ENIMPL;

			if ((caps->targetable_vfo&RIG_TARGETABLE_ALL) || 
						vfo == RIG_VFO_CURR || vfo == rig->state.current_vfo)
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
				ser_ptt_set(&rig->state.pttport, ptt);
				break;
		case RIG_PTT_PARALLEL:
				par_ptt_set(&rig->state.pttport, ptt);
				break;

		case RIG_PTT_NONE:
				return -RIG_ENAVAIL;	/* not available */
		default:
				return -RIG_EINVAL;
		}

		return RIG_OK;
}

/**
 * \brief get the status of the PTT
 * \param rig	The rig handle
 * \param vfo	The target VFO
 * \param ptt	The location where to store the status of the PTT
 *
 *  Retrieves the status of PTT (are we on the air?).
 *
 * \return RIG_OK if the operation has been sucessful, otherwise 
 * a negative value if an error occured (in which case, cause is 
 * set appropriately).
 *
 * \sa rig_set_ptt()
 */
int rig_get_ptt(RIG *rig, vfo_t vfo, ptt_t *ptt)
{	
		const struct rig_caps *caps;
		int retcode;
		vfo_t curr_vfo;

		if (CHECK_RIG_ARG(rig) || !ptt)
			return -RIG_EINVAL;

		caps = rig->caps;

		switch (rig->state.pttport.type.ptt) {
		case RIG_PTT_RIG:
			if (caps->get_ptt == NULL)
				return -RIG_ENIMPL;

			if ((caps->targetable_vfo&RIG_TARGETABLE_ALL) || 
						vfo == RIG_VFO_CURR || vfo == rig->state.current_vfo)
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
				ser_ptt_get(&rig->state.pttport, ptt);
				break;
		case RIG_PTT_PARALLEL:
				par_ptt_get(&rig->state.pttport, ptt);
				break;

		case RIG_PTT_NONE:
				return -RIG_ENAVAIL;	/* not available */

		default:
				return -RIG_EINVAL;
		}

		return RIG_OK;
}

/**
 * \brief get the status of the DCD
 * \param rig	The rig handle
 * \param vfo	The target VFO
 * \param dcd	The location where to store the status of the DCD
 *
 *  Retrieves the status of DCD (is squelch open?).
 *
 * \return RIG_OK if the operation has been sucessful, otherwise 
 * a negative value if an error occured (in which case, cause is 
 * set appropriately).
 *
 */
int rig_get_dcd(RIG *rig, vfo_t vfo, dcd_t *dcd)
{	
		const struct rig_caps *caps;
		int retcode;
		vfo_t curr_vfo;

		if (CHECK_RIG_ARG(rig) || !dcd)
			return -RIG_EINVAL;

		caps = rig->caps;

		switch (rig->state.dcdport.type.dcd) {
		case RIG_DCD_RIG:
			if (caps->get_dcd == NULL)
				return -RIG_ENIMPL;

			if ((caps->targetable_vfo&RIG_TARGETABLE_ALL) || 
						vfo == RIG_VFO_CURR || vfo == rig->state.current_vfo)
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
				ser_dcd_get(&rig->state.dcdport, dcd);
				break;
		case RIG_DCD_PARALLEL:
				par_dcd_get(&rig->state.dcdport, dcd);
				break;

		case RIG_DCD_NONE:
				return -RIG_ENAVAIL;	/* not available */

		default:
				return -RIG_EINVAL;
		}

		return RIG_OK;
}


/**
 * \brief set the repeater shift
 * \param rig	The rig handle
 * \param vfo	The target VFO
 * \param rptr_shift	The repeater shift to set to
 *
 *  Sets the current repeater shift.
 *
 * \return RIG_OK if the operation has been sucessful, otherwise 
 * a negative value if an error occured (in which case, cause is 
 * set appropriately).
 *
 * \sa rig_get_rptr_shift()
 */
int rig_set_rptr_shift(RIG *rig, vfo_t vfo, rptr_shift_t rptr_shift)
{
		const struct rig_caps *caps;
		int retcode;
		vfo_t curr_vfo;

		if (CHECK_RIG_ARG(rig))
			return -RIG_EINVAL;

		caps = rig->caps;

		if (caps->set_rptr_shift == NULL)
			return -RIG_ENAVAIL;

		if ((caps->targetable_vfo&RIG_TARGETABLE_ALL) || 
						vfo == RIG_VFO_CURR || vfo == rig->state.current_vfo)
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
 * \brief get the current repeater shift
 * \param rig	The rig handle
 * \param vfo	The target VFO
 * \param rptr_shift	The location where to store the current repeater shift
 *
 *  Retrieves the current repeater shift.
 *
 * \return RIG_OK if the operation has been sucessful, otherwise 
 * a negative value if an error occured (in which case, cause is 
 * set appropriately).
 *
 * \sa rig_set_rptr_shift()
 */
int rig_get_rptr_shift(RIG *rig, vfo_t vfo, rptr_shift_t *rptr_shift)
{
		const struct rig_caps *caps;
		int retcode;
		vfo_t curr_vfo;

		if (CHECK_RIG_ARG(rig) || !rptr_shift)
			return -RIG_EINVAL;

		caps = rig->caps;

		if (caps->get_rptr_shift == NULL)
			return -RIG_ENAVAIL;

		if ((caps->targetable_vfo&RIG_TARGETABLE_ALL) || 
						vfo == RIG_VFO_CURR || vfo == rig->state.current_vfo)
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
 * \brief set the repeater offset
 * \param rig	The rig handle
 * \param vfo	The target VFO
 * \param rptr_offs	The VFO to set to
 *
 *  Sets the current repeater offset.
 *
 * \return RIG_OK if the operation has been sucessful, otherwise 
 * a negative value if an error occured (in which case, cause is 
 * set appropriately).
 *
 * \sa rig_get_rptr_offs()
 */

int rig_set_rptr_offs(RIG *rig, vfo_t vfo, shortfreq_t rptr_offs)
{
		const struct rig_caps *caps;
		int retcode;
		vfo_t curr_vfo;

		if (CHECK_RIG_ARG(rig))
			return -RIG_EINVAL;

		caps = rig->caps;

		if (caps->set_rptr_offs == NULL)
			return -RIG_ENAVAIL;

		if ((caps->targetable_vfo&RIG_TARGETABLE_ALL) || 
						vfo == RIG_VFO_CURR || vfo == rig->state.current_vfo)
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
 * \brief get the current repeater offset
 * \param rig	The rig handle
 * \param vfo	The target VFO
 * \param rptr_offs	The location where to store the current repeater offset
 *
 *  Retrieves the current repeater offset.
 *
 * \return RIG_OK if the operation has been sucessful, otherwise 
 * a negative value if an error occured (in which case, cause is 
 * set appropriately).
 *
 * \sa rig_set_rptr_offs()
 */

int rig_get_rptr_offs(RIG *rig, vfo_t vfo, shortfreq_t *rptr_offs)
{
		const struct rig_caps *caps;
		int retcode;
		vfo_t curr_vfo;

		if (CHECK_RIG_ARG(rig) || !rptr_offs)
			return -RIG_EINVAL;

		caps = rig->caps;

		if (caps->get_rptr_offs == NULL)
			return -RIG_ENAVAIL;

		if ((caps->targetable_vfo&RIG_TARGETABLE_ALL) || 
						vfo == RIG_VFO_CURR || vfo == rig->state.current_vfo)
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
 * \brief set the split frequencies
 * \param rig	The rig handle
 * \param vfo	The target VFO
 * \param tx_freq	The transmit split frequency to set to
 *
 *  Sets the split frequencies.
 *
 * \return RIG_OK if the operation has been sucessful, otherwise 
 * a negative value if an error occured (in which case, cause is 
 * set appropriately).
 *
 * \sa rig_get_split_freq()
 */

int rig_set_split_freq(RIG *rig, vfo_t vfo, freq_t tx_freq)
{
		const struct rig_caps *caps;
		int retcode;
		vfo_t curr_vfo;

		if (CHECK_RIG_ARG(rig))
			return -RIG_EINVAL;

		caps = rig->caps;

		if (caps->set_split_freq == NULL)
			return -RIG_ENAVAIL;

		if ((caps->targetable_vfo&RIG_TARGETABLE_ALL) || 
						vfo == RIG_VFO_CURR || vfo == rig->state.current_vfo)
			return caps->set_split_freq(rig, vfo, tx_freq);

		if (!caps->set_vfo)
			return -RIG_ENTARGET;
		curr_vfo = rig->state.current_vfo;
		retcode = caps->set_vfo(rig, vfo);
		if (retcode != RIG_OK)
				return retcode;

		retcode = caps->set_split_freq(rig, vfo, tx_freq);
		caps->set_vfo(rig, curr_vfo);
		return retcode;
}

/**
 * \brief get the current split frequencies
 * \param rig	The rig handle
 * \param vfo	The target VFO
 * \param tx_freq	The location where to store the current transmit split frequency
 *
 *  Retrieves the current split frequencies.
 *
 * \return RIG_OK if the operation has been sucessful, otherwise 
 * a negative value if an error occured (in which case, cause is 
 * set appropriately).
 *
 * \sa rig_set_split_freq()
 */
int rig_get_split_freq(RIG *rig, vfo_t vfo, freq_t *tx_freq)
{
		const struct rig_caps *caps;
		int retcode;
		vfo_t curr_vfo;

		if (CHECK_RIG_ARG(rig) || !tx_freq)
			return -RIG_EINVAL;

		caps = rig->caps;

		if (caps->get_split_freq == NULL)
			return -RIG_ENAVAIL;

		if ((caps->targetable_vfo&RIG_TARGETABLE_ALL) || 
						vfo == RIG_VFO_CURR || vfo == rig->state.current_vfo)
			return caps->get_split_freq(rig, vfo, tx_freq);

		if (!caps->set_vfo)
			return -RIG_ENTARGET;
		curr_vfo = rig->state.current_vfo;
		retcode = caps->set_vfo(rig, vfo);
		if (retcode != RIG_OK)
				return retcode;

		retcode = caps->get_split_freq(rig, vfo, tx_freq);
		caps->set_vfo(rig, curr_vfo);
		return retcode;
}

/**
 * \brief set the split modes
 * \param rig	The rig handle
 * \param vfo	The target VFO
 * \param tx_mode	The transmit split mode to set to
 * \param tx_width	The transmit split width to set to
 *
 *  Sets the split mode.
 *
 * \return RIG_OK if the operation has been sucessful, otherwise 
 * a negative value if an error occured (in which case, cause is 
 * set appropriately).
 *
 * \sa rig_get_split_mode()
 */

int rig_set_split_mode(RIG *rig, vfo_t vfo, rmode_t tx_mode, pbwidth_t tx_width)
{
		const struct rig_caps *caps;
		int retcode;
		vfo_t curr_vfo;

		if (CHECK_RIG_ARG(rig))
			return -RIG_EINVAL;

		caps = rig->caps;

		if (caps->set_split_mode == NULL)
			return -RIG_ENAVAIL;

		if ((caps->targetable_vfo&RIG_TARGETABLE_ALL) || 
						vfo == RIG_VFO_CURR || vfo == rig->state.current_vfo)
			return caps->set_split_mode(rig, vfo, tx_mode, tx_width);

		if (!caps->set_vfo)
			return -RIG_ENTARGET;
		curr_vfo = rig->state.current_vfo;
		retcode = caps->set_vfo(rig, vfo);
		if (retcode != RIG_OK)
				return retcode;

		retcode = caps->set_split_mode(rig, vfo, tx_mode, tx_width);
		caps->set_vfo(rig, curr_vfo);
		return retcode;
}

/**
 * \brief get the current split modes
 * \param rig	The rig handle
 * \param vfo	The target VFO
 * \param tx_mode	The location where to store the current transmit split mode
 * \param tx_width	The location where to store the current transmit split width
 *
 *  Retrieves the current split mode.
 *
 * \return RIG_OK if the operation has been sucessful, otherwise 
 * a negative value if an error occured (in which case, cause is 
 * set appropriately).
 *
 * \sa rig_set_split_mode()
 */
int rig_get_split_mode(RIG *rig, vfo_t vfo, rmode_t *tx_mode, pbwidth_t *tx_width)
{
		const struct rig_caps *caps;
		int retcode;
		vfo_t curr_vfo;

		if (CHECK_RIG_ARG(rig) || !tx_mode || !tx_width)
			return -RIG_EINVAL;

		caps = rig->caps;

		if (caps->get_split_mode == NULL)
			return -RIG_ENAVAIL;

		if ((caps->targetable_vfo&RIG_TARGETABLE_ALL) || 
						vfo == RIG_VFO_CURR || vfo == rig->state.current_vfo)
			return caps->get_split_mode(rig, vfo, tx_mode, tx_width);

		if (!caps->set_vfo)
			return -RIG_ENTARGET;
		curr_vfo = rig->state.current_vfo;
		retcode = caps->set_vfo(rig, vfo);
		if (retcode != RIG_OK)
				return retcode;

		retcode = caps->get_split_mode(rig, vfo, tx_mode, tx_width);
		caps->set_vfo(rig, curr_vfo);
		return retcode;
}


/**
 * \brief set the split mode
 * \param rig	The rig handle
 * \param vfo	The target VFO
 * \param split	The split mode to set to
 *
 *  Sets the current split mode.
 *
 * \return RIG_OK if the operation has been sucessful, otherwise 
 * a negative value if an error occured (in which case, cause is 
 * set appropriately).
 *
 * \sa rig_get_split()
 */
int rig_set_split(RIG *rig, vfo_t vfo, split_t split)
{
		const struct rig_caps *caps;
		int retcode;
		vfo_t curr_vfo;

		if (CHECK_RIG_ARG(rig))
			return -RIG_EINVAL;

		caps = rig->caps;

		if (caps->set_split == NULL)
			return -RIG_ENAVAIL;

		if ((caps->targetable_vfo&RIG_TARGETABLE_ALL) || 
						vfo == RIG_VFO_CURR || vfo == rig->state.current_vfo)
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
 * \brief get the current split mode
 * \param rig	The rig handle
 * \param vfo	The target VFO
 * \param split	The location where to store the current split mode
 *
 *  Retrieves the current split mode.
 *
 * \return RIG_OK if the operation has been sucessful, otherwise 
 * a negative value if an error occured (in which case, cause is 
 * set appropriately).
 *
 * \sa rig_set_split()
 */
int rig_get_split(RIG *rig, vfo_t vfo, split_t *split)
{
		const struct rig_caps *caps;
		int retcode;
		vfo_t curr_vfo;

		if (CHECK_RIG_ARG(rig) || !split)
			return -RIG_EINVAL;

		caps = rig->caps;

		if (caps->get_split == NULL)
			return -RIG_ENAVAIL;

		if ((caps->targetable_vfo&RIG_TARGETABLE_ALL) || 
						vfo == RIG_VFO_CURR || vfo == rig->state.current_vfo)
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
 * \brief set the RIT
 * \param rig	The rig handle
 * \param vfo	The target VFO
 * \param rit	The RIT offset to adjust to
 *
 *  Sets the current RIT offset. A value of 0 for \a rit disables RIT.
 *
 * \return RIG_OK if the operation has been sucessful, otherwise 
 * a negative value if an error occured (in which case, cause is 
 * set appropriately).
 *
 * \sa rig_get_rit()
 */

int rig_set_rit(RIG *rig, vfo_t vfo, shortfreq_t rit)
{
		const struct rig_caps *caps;
		int retcode;
		vfo_t curr_vfo;

		if (CHECK_RIG_ARG(rig))
			return -RIG_EINVAL;

		caps = rig->caps;

		if (caps->set_rit == NULL)
			return -RIG_ENAVAIL;

		if ((caps->targetable_vfo&RIG_TARGETABLE_ALL) || 
						vfo == RIG_VFO_CURR || vfo == rig->state.current_vfo)
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
 * \brief get the current RIT offset
 * \param rig	The rig handle
 * \param vfo	The target VFO
 * \param rit	The location where to store the current RIT offset
 *
 *  Retrieves the current RIT offset.
 *
 * \return RIG_OK if the operation has been sucessful, otherwise 
 * a negative value if an error occured (in which case, cause is 
 * set appropriately).
 *
 * \sa rig_set_rit()
 */

int rig_get_rit(RIG *rig, vfo_t vfo, shortfreq_t *rit)
{
		const struct rig_caps *caps;
		int retcode;
		vfo_t curr_vfo;

		if (CHECK_RIG_ARG(rig) || !rit)
			return -RIG_EINVAL;

		caps = rig->caps;

		if (caps->get_rit == NULL)
			return -RIG_ENAVAIL;

		if ((caps->targetable_vfo&RIG_TARGETABLE_ALL) || 
						vfo == RIG_VFO_CURR || vfo == rig->state.current_vfo)
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
 * \brief set the XIT
 * \param rig	The rig handle
 * \param vfo	The target VFO
 * \param xit	The XIT offset to adjust to
 *
 *  Sets the current XIT offset. A value of 0 for \a xit disables XIT.
 *
 * \return RIG_OK if the operation has been sucessful, otherwise 
 * a negative value if an error occured (in which case, cause is 
 * set appropriately).
 *
 * \sa rig_get_xit()
 */

int rig_set_xit(RIG *rig, vfo_t vfo, shortfreq_t xit)
{
		const struct rig_caps *caps;
		int retcode;
		vfo_t curr_vfo;

		if (CHECK_RIG_ARG(rig))
			return -RIG_EINVAL;

		caps = rig->caps;

		if (caps->set_xit == NULL)
			return -RIG_ENAVAIL;

		if ((caps->targetable_vfo&RIG_TARGETABLE_ALL) || 
						vfo == RIG_VFO_CURR || vfo == rig->state.current_vfo)
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
 * \brief get the current XIT offset
 * \param rig	The rig handle
 * \param vfo	The target VFO
 * \param xit	The location where to store the current XIT offset
 *
 *  Retrieves the current XIT offset.
 *
 * \return RIG_OK if the operation has been sucessful, otherwise 
 * a negative value if an error occured (in which case, cause is 
 * set appropriately).
 *
 * \sa rig_set_xit()
 */

int rig_get_xit(RIG *rig, vfo_t vfo, shortfreq_t *xit)
{
		const struct rig_caps *caps;
		int retcode;
		vfo_t curr_vfo;

		if (CHECK_RIG_ARG(rig) || !xit)
			return -RIG_EINVAL;

		caps = rig->caps;

		if (caps->get_xit == NULL)
			return -RIG_ENAVAIL;

		if ((caps->targetable_vfo&RIG_TARGETABLE_ALL) || 
						vfo == RIG_VFO_CURR || vfo == rig->state.current_vfo)
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
 * \brief set the Tuning Step
 * \param rig	The rig handle
 * \param vfo	The target VFO
 * \param ts	The tuning step to set to
 *
 *  Sets the Tuning Step.
 *
 * \return RIG_OK if the operation has been sucessful, otherwise 
 * a negative value if an error occured (in which case, cause is 
 * set appropriately).
 *
 * \sa rig_get_ts()
 */

int rig_set_ts(RIG *rig, vfo_t vfo, shortfreq_t ts)
{
		const struct rig_caps *caps;
		int retcode;
		vfo_t curr_vfo;

		if (CHECK_RIG_ARG(rig))
			return -RIG_EINVAL;

		caps = rig->caps;

		if (caps->set_ts == NULL)
			return -RIG_ENAVAIL;

		if ((caps->targetable_vfo&RIG_TARGETABLE_ALL) || 
						vfo == RIG_VFO_CURR || vfo == rig->state.current_vfo)
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
 * \brief get the current Tuning Step
 * \param rig	The rig handle
 * \param vfo	The target VFO
 * \param ts	The location where to store the current tuning step
 *
 *  Retrieves the current tuning step.
 *
 * \return RIG_OK if the operation has been sucessful, otherwise 
 * a negative value if an error occured (in which case, cause is 
 * set appropriately).
 *
 * \sa rig_set_ts()
 */

int rig_get_ts(RIG *rig, vfo_t vfo, shortfreq_t *ts)
{
		const struct rig_caps *caps;
		int retcode;
		vfo_t curr_vfo;

		if (CHECK_RIG_ARG(rig) || !ts)
			return -RIG_EINVAL;

		caps = rig->caps;

		if (caps->get_ts == NULL)
			return -RIG_ENAVAIL;

		if ((caps->targetable_vfo&RIG_TARGETABLE_ALL) || 
						vfo == RIG_VFO_CURR || vfo == rig->state.current_vfo)
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

/**
 * \brief set the antenna
 * \param rig	The rig handle
 * \param vfo	The target VFO
 * \param ant	The anntena to select
 *
 *  Sets the current antenna.
 *
 * \return RIG_OK if the operation has been sucessful, otherwise 
 * a negative value if an error occured (in which case, cause is 
 * set appropriately).
 *
 * \sa rig_get_ant()
 */

int rig_set_ant(RIG *rig, vfo_t vfo, ant_t ant)
{
		const struct rig_caps *caps;
		int retcode;
		vfo_t curr_vfo;

		if (CHECK_RIG_ARG(rig))
			return -RIG_EINVAL;

		caps = rig->caps;

		if (caps->set_ant == NULL)
			return -RIG_ENAVAIL;

		if ((caps->targetable_vfo&RIG_TARGETABLE_ALL) || 
						vfo == RIG_VFO_CURR || vfo == rig->state.current_vfo)
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
 * \brief get the current antenna
 * \param rig	The rig handle
 * \param vfo	The target VFO
 * \param ant	The location where to store the current antenna
 *
 *  Retrieves the current antenna.
 *
 * \return RIG_OK if the operation has been sucessful, otherwise 
 * a negative value if an error occured (in which case, cause is 
 * set appropriately).
 *
 * \sa rig_set_ant()
 */

int rig_get_ant(RIG *rig, vfo_t vfo, ant_t *ant)
{
		const struct rig_caps *caps;
		int retcode;
		vfo_t curr_vfo;

		if (CHECK_RIG_ARG(rig) || !ant)
			return -RIG_EINVAL;

		caps = rig->caps;

		if (caps->get_ant == NULL)
			return -RIG_ENAVAIL;

		if ((caps->targetable_vfo&RIG_TARGETABLE_ALL) || 
						vfo == RIG_VFO_CURR || vfo == rig->state.current_vfo)
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
 * \brief conversion utility from relative range to absolute in mW
 * \param rig	The rig handle
 * \param mwpower	The location where to store the converted power in mW
 * \param power	The relative power
 * \param freq	The frequency where the conversion should take place
 * \param mode	The mode where the conversion should take place
 *
 *  Converts a power value expressed in a range on a [0.0 .. 1.0] relative 
 *  scale to the real transmit power in milli Watts the radio would emit.
 *  The \a freq and \a mode where the conversion should take place must be
 *  also provided since the relative power is peculiar to a specific
 *  freq and mode range of the radio.
 *
 * \return RIG_OK if the operation has been sucessful, otherwise 
 * a negative value if an error occured (in which case, cause is 
 * set appropriately).
 *
 * \sa rig_mW2power()
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
 * \brief conversion utility from absolute in mW to relative range
 * \param rig	The rig handle
 * \param power	The location where to store the converted relative power
 * \param mwpower	The power in mW
 * \param freq	The frequency where the conversion should take place
 * \param mode	The mode where the conversion should take place
 *
 * Converts a power value expressed in the real transmit power in milli Watts
 * the radio would emit to a range on a [0.0 .. 1.0] relative scale.
 * The \a freq and \a mode where the conversion should take place must be
 * also provided since the relative power is peculiar to a specific
 * freq and mode range of the radio.
 *
 * \return RIG_OK if the operation has been sucessful, otherwise 
 * a negative value if an error occured (in which case, cause is 
 * set appropriately).
 *
 * \sa rig_power2mW()
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
 * \brief get the best frequency resolution of the rig
 * \param rig	The rig handle
 * \param mode	The mode where the conversion should take place
 *
 *  Returns the best frequency resolution of the rig, for a given \a mode.
 *
 * \return the frequency resolution in Hertz if the operation h
 * has been sucessful, otherwise a negative value if an error occured.
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
 * \brief set CTCSS sub-tone frequency
 * \param rig	The rig handle
 * \param vfo	The target VFO
 * \param tone	The tone to set to
 *
 *  Sets the current Continuous Tone Controlled Squelch System (CTCSS) 
 *  sub-audible tone frequency.
 *  \note  the \a tone integer is NOT in Hz, but in tenth of Hz! This way,
 *  if you want to set a subaudible tone of 88.5 Hz for example,
 *  then pass 885 to this function.
 *  Also, to disable Tone encoding, set \a tone to 0 (FIXME: not clear in API).
 *
 * \return RIG_OK if the operation has been sucessful, otherwise 
 * a negative value if an error occured (in which case, cause is 
 * set appropriately).
 *
 * \sa rig_get_ctcss_tone(), rig_set_ctcss_sql()
 */

int rig_set_ctcss_tone(RIG *rig, vfo_t vfo, tone_t tone)
{
		const struct rig_caps *caps;
		int retcode;
		vfo_t curr_vfo;

		if (CHECK_RIG_ARG(rig))
			return -RIG_EINVAL;

		caps = rig->caps;

		if (caps->set_ctcss_tone == NULL)
			return -RIG_ENAVAIL;

		if ((caps->targetable_vfo&RIG_TARGETABLE_ALL) || 
						vfo == RIG_VFO_CURR || vfo == rig->state.current_vfo)
			return caps->set_ctcss_tone(rig, vfo, tone);

		if (!caps->set_vfo)
			return -RIG_ENTARGET;
		curr_vfo = rig->state.current_vfo;
		retcode = caps->set_vfo(rig, vfo);
		if (retcode != RIG_OK)
				return retcode;

		retcode = caps->set_ctcss_tone(rig, vfo, tone);
		caps->set_vfo(rig, curr_vfo);
		return retcode;
}

/**
 * \brief get the current CTCSS sub-tone frequency
 * \param rig	The rig handle
 * \param vfo	The target VFO
 * \param tone	The location where to store the current tone
 *
 *  Retrieves the current Continuous Tone Controlled Squelch System (CTCSS) 
 *  sub-audible tone frequency.
 *  \note the \a *tone integer is NOT in Hz, but in tenth of Hz! This way,
 *  if the function rig_get_ctcss_tone() returns a subaudible tone of 885
 *  for example, then the real tone is 88.5 Hz. 
 *  Also, a value of 0 for \a *tone means the Tone encoding is disabled.
 *
 * \return RIG_OK if the operation has been sucessful, otherwise 
 * a negative value if an error occured (in which case, cause is 
 * set appropriately).
 *
 * \sa rig_set_ctcss_tone(), rig_get_ctcss_sql()
 */
int rig_get_ctcss_tone(RIG *rig, vfo_t vfo, tone_t *tone)
{
		const struct rig_caps *caps;
		int retcode;
		vfo_t curr_vfo;

		if (CHECK_RIG_ARG(rig) || !tone)
			return -RIG_EINVAL;

		caps = rig->caps;

		if (caps->get_ctcss_tone == NULL)
			return -RIG_ENAVAIL;

		if ((caps->targetable_vfo&RIG_TARGETABLE_ALL) || 
						vfo == RIG_VFO_CURR || vfo == rig->state.current_vfo)
			return caps->get_ctcss_tone(rig, vfo, tone);

		if (!caps->set_vfo)
			return -RIG_ENTARGET;
		curr_vfo = rig->state.current_vfo;
		retcode = caps->set_vfo(rig, vfo);
		if (retcode != RIG_OK)
				return retcode;

		retcode = caps->get_ctcss_tone(rig, vfo, tone);
		caps->set_vfo(rig, curr_vfo);
		return retcode;
}

/**
 * \brief set the current encoding DCS code
 * \param rig	The rig handle
 * \param vfo	The target VFO
 * \param code	The tone to set to
 *
 * Sets the current encoding Digitally-Coded Squelch code.
 *
 * \return RIG_OK if the operation has been sucessful, otherwise 
 * a negative value if an error occured (in which case, cause is 
 * set appropriately).
 *
 * \sa rig_get_dcs_code(), rig_set_dcs_sql()
 */

int rig_set_dcs_code(RIG *rig, vfo_t vfo, tone_t code)
{
		const struct rig_caps *caps;
		int retcode;
		vfo_t curr_vfo;

		if (CHECK_RIG_ARG(rig))
			return -RIG_EINVAL;

		caps = rig->caps;

		if (caps->set_dcs_code == NULL)
			return -RIG_ENAVAIL;

		if ((caps->targetable_vfo&RIG_TARGETABLE_ALL) || 
						vfo == RIG_VFO_CURR || vfo == rig->state.current_vfo)
			return caps->set_dcs_code(rig, vfo, code);

		if (!caps->set_vfo)
			return -RIG_ENTARGET;
		curr_vfo = rig->state.current_vfo;
		retcode = caps->set_vfo(rig, vfo);
		if (retcode != RIG_OK)
				return retcode;

		retcode = caps->set_dcs_code(rig, vfo, code);
		caps->set_vfo(rig, curr_vfo);
		return retcode;
}

/**
 * \brief get the current encoding DCS code
 * \param rig	The rig handle
 * \param vfo	The target VFO
 * \param code	The location where to store the current tone
 *
 * Retrieves the current encoding Digitally-Coded Squelch code. 
 *
 * \return RIG_OK if the operation has been sucessful, otherwise 
 * a negative value if an error occured (in which case, cause is 
 * set appropriately).
 *
 * \sa rig_set_dcs_code(), rig_get_dcs_sql()
 */
int rig_get_dcs_code(RIG *rig, vfo_t vfo, tone_t *code)
{
		const struct rig_caps *caps;
		int retcode;
		vfo_t curr_vfo;

		if (CHECK_RIG_ARG(rig) || !code)
			return -RIG_EINVAL;

		caps = rig->caps;

		if (caps->get_dcs_code == NULL)
			return -RIG_ENAVAIL;

		if ((caps->targetable_vfo&RIG_TARGETABLE_ALL) || 
						vfo == RIG_VFO_CURR || vfo == rig->state.current_vfo)
			return caps->get_dcs_code(rig, vfo, code);

		if (!caps->set_vfo)
			return -RIG_ENTARGET;
		curr_vfo = rig->state.current_vfo;
		retcode = caps->set_vfo(rig, vfo);
		if (retcode != RIG_OK)
				return retcode;

		retcode = caps->get_dcs_code(rig, vfo, code);
		caps->set_vfo(rig, curr_vfo);
		return retcode;
}

/**
 * \brief set CTCSS squelch
 * \param rig	The rig handle
 * \param vfo	The target VFO
 * \param tone	The PL tone to set the squelch to
 *
 *  Sets the current Continuous Tone Controlled Squelch System (CTCSS)
 *  sub-audible *squelch* tone.
 *  \note \a tone is NOT in Hz, but in tenth of Hz! This way,
 *  if you want to set subaudible squelch tone of 88.5 Hz for example,
 *  then pass 885 to this function.
 *  Also, to disable Tone squelch, set \a tone to 0 (FIXME: not clear in API).
 *
 * \return RIG_OK if the operation has been sucessful, otherwise 
 * a negative value if an error occured (in which case, cause is 
 * set appropriately).
 *
 * \sa rig_get_ctcss_sql(), rig_set_ctcss_tone()
 */

int rig_set_ctcss_sql(RIG *rig, vfo_t vfo, tone_t tone)
{
		const struct rig_caps *caps;
		int retcode;
		vfo_t curr_vfo;

		if (CHECK_RIG_ARG(rig))
			return -RIG_EINVAL;

		caps = rig->caps;

		if (caps->set_ctcss_sql == NULL)
			return -RIG_ENAVAIL;

		if ((caps->targetable_vfo&RIG_TARGETABLE_ALL) || 
						vfo == RIG_VFO_CURR || vfo == rig->state.current_vfo)
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
 * \brief get the current CTCSS squelch
 * \param rig	The rig handle
 * \param vfo	The target VFO
 * \param tone	The location where to store the current tone
 *
 *  Retrieves the current Continuous Tone Controlled Squelch System (CTCSS) 
 *  sub-audible *squelch* tone.
 *  \note \a *tone is NOT in Hz, but in tenth of Hz! This way,
 *  if the function rig_get_ctcss_sql() returns a subaudible tone of 885
 *  for example, then the real tone is 88.5 Hz. 
 *  Also, a value of 0 for \a tone means the Tone squelch is disabled.
 *
 * \return RIG_OK if the operation has been sucessful, otherwise 
 * a negative value if an error occured (in which case, cause is 
 * set appropriately).
 *
 * \sa rig_set_ctcss_sql(), rig_get_ctcss_tone()
 */
int rig_get_ctcss_sql(RIG *rig, vfo_t vfo, tone_t *tone)
{
		const struct rig_caps *caps;
		int retcode;
		vfo_t curr_vfo;

		if (CHECK_RIG_ARG(rig) || !tone)
			return -RIG_EINVAL;

		caps = rig->caps;

		if (caps->get_ctcss_sql == NULL)
			return -RIG_ENAVAIL;

		if ((caps->targetable_vfo&RIG_TARGETABLE_ALL) ||
				vfo == RIG_VFO_CURR || vfo == rig->state.current_vfo)
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
 * \brief set the current DCS code
 * \param rig	The rig handle
 * \param vfo	The target VFO
 * \param code	The tone to set to
 *
 * Sets the current Digitally-Coded *Squelch* code.
 *
 * \return returns RIG_OK if the operation has been sucessful, ortherwise
 * a negative value if an error occured (in which case, cause is set
 * appropriately).
 *
 * \sa rig_get_dcs_sql(), rig_set_dcs_code()
 */

int rig_set_dcs_sql(RIG *rig, vfo_t vfo, tone_t code)
{
		const struct rig_caps *caps;
		int retcode;
		vfo_t curr_vfo;

		if (CHECK_RIG_ARG(rig))
			return -RIG_EINVAL;

		caps = rig->caps;

		if (caps->set_dcs_sql == NULL)
			return -RIG_ENAVAIL;

		if ((caps->targetable_vfo&RIG_TARGETABLE_ALL) ||
				vfo == RIG_VFO_CURR || vfo == rig->state.current_vfo)
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
 * \brief get the current DCS code
 * \param rig	The rig handle
 * \param vfo	The target VFO
 * \param code	The location where to store the current tone
 *
 * Retrieves the current Digitally-Coded *Squelch* code. 
 *
 * \return RIG_OK if the operation has been sucessful, ortherwise 
 * a negative value if an error occured (in which case, cause is 
 * set appropriately).
 *
 * \sa rig_set_dcs_sql(), rig_get_dcs_code()
 */
int rig_get_dcs_sql(RIG *rig, vfo_t vfo, tone_t *code)
{
		const struct rig_caps *caps;
		int retcode;
		vfo_t curr_vfo;

		if (CHECK_RIG_ARG(rig) || !code)
			return -RIG_EINVAL;

		caps = rig->caps;

		if (caps->get_dcs_sql == NULL)
			return -RIG_ENAVAIL;

		if ((caps->targetable_vfo&RIG_TARGETABLE_ALL) ||
				vfo == RIG_VFO_CURR || vfo == rig->state.current_vfo)
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
 * \brief turn on/off the radio
 * \param rig	The rig handle
 * \param status	The status to set to
 *
 * turns on/off the radio.
 * See #RIG_POWER_ON, #RIG_POWER_OFF and #RIG_POWER_STANDBY defines
 * for the \a status.
 *
 * \return RIG_OK if the operation has been sucessful, ortherwise 
 * a negative value if an error occured (in which case, cause is 
 * set appropriately).
 *
 * \sa rig_get_powerstat()
 */

int rig_set_powerstat(RIG *rig, powerstat_t status)
{
		if (CHECK_RIG_ARG(rig))
			return -RIG_EINVAL;

		if (rig->caps->set_powerstat == NULL)
			return -RIG_ENAVAIL;

		return rig->caps->set_powerstat(rig, status);
}

/**
 * \brief get the on/off status of the radio
 * \param rig	The rig handle
 * \param status	The locatation where to store the current status
 *
 *  Retrieve the status of the radio. See RIG_POWER_ON, RIG_POWER_OFF and 
 *  RIG_POWER_STANDBY defines for the \a status.
 *
 * \return RIG_OK if the operation has been sucessful, otherwise 
 * a negative value if an error occured (in which case, cause is 
 * set appropriately).
 *
 * \sa rig_set_powerstat()
 */

int rig_get_powerstat(RIG *rig, powerstat_t *status)
{
		if (CHECK_RIG_ARG(rig) || !status)
			return -RIG_EINVAL;

		if (rig->caps->get_powerstat == NULL)
			return -RIG_ENAVAIL;

		return rig->caps->get_powerstat(rig, status);
}

/**
 * \brief reset the radio
 * \param rig	The rig handle
 * \param reset	The reset operation to perform
 *
 *  Resets the radio.
 *  See RIG_RESET_NONE, RIG_RESET_SOFT and RIG_RESET_MCALL defines
 *  for the \a reset.
 *
 * \return RIG_OK if the operation has been sucessful, otherwise 
 * a negative value if an error occured (in which case, cause is 
 * set appropriately).
 *
 */

int rig_reset(RIG *rig, reset_t reset)
{
		if (CHECK_RIG_ARG(rig))
			return -RIG_EINVAL;

		if (rig->caps->reset == NULL)
			return -RIG_ENAVAIL;

		return rig->caps->reset(rig, reset);
}


/**
 * \brief try to guess a rig
 * \param port		A pointer describing a port linking the host to the rig
 *
 *  Try to guess what is the model of rig pointed to by port. 
 *  It can be very buggy, and mess up the radio at the other end.
 *  (but fun if it works!)
 *
 * \warning this is really Experimental, It has been tested only
 * with IC-706MkIIG. any feedback welcome! --SF
 *
 * \return the rig model id according to the rig_model_t type if found, 
 * otherwise RIG_MODEL_NONE if unable to determine rig model.
 */
rig_model_t rig_probe(port_t *port)
{
		if (!port)
				return RIG_MODEL_NONE;

		return rig_probe_all(port);
}

/**
 * \brief set a radio level setting
 * \param rig	The rig handle
 * \param vfo	The target VFO
 * \param level	The level setting
 * \param val	The value to set the level setting to
 *
 * Sets the level of a setting.
 * The level value \a val can be a float or an integer. See #value_t
 * for more information.
 *
 * \return RIG_OK if the operation has been sucessful, otherwise 
 * a negative value if an error occured (in which case, cause is 
 * set appropriately).
 *
 * \sa rig_has_set_level(), rig_get_level()
 */
int rig_set_level(RIG *rig, vfo_t vfo, setting_t level, value_t val)
{
		const struct rig_caps *caps;
		int retcode;
		vfo_t curr_vfo;

		if (CHECK_RIG_ARG(rig))
			return -RIG_EINVAL;

		caps = rig->caps;

		if (caps->set_level == NULL || !rig_has_set_level(rig,level))
			return -RIG_ENAVAIL;

		if ((caps->targetable_vfo&RIG_TARGETABLE_ALL) ||
				vfo == RIG_VFO_CURR || vfo == rig->state.current_vfo)
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
 * \brief get the value of a level
 * \param rig	The rig handle
 * \param vfo	The target VFO
 * \param level	The level setting
 * \param val	The location where to store the value of \a level
 *
 *  Retrieves the value of a \a level.
 *  The level value \a val can be a float or an integer. See #value_t
 *  for more information.
 *
 * 		RIG_LEVEL_STRENGTH: \a val is an integer, representing the S Meter
 * 		level in dB, according to the ideal S Meter scale. The ideal
 * 		S Meter scale is as follow: S0=-54, S1=-48, S2=-42, S3=-36,
 * 		S4=-30, S5=-24, S6=-18, S7=-12, S8=-6, S9=0, +10=10, +20=20,
 * 		+30=30, +40=40, +50=50 and +60=60. This is the responsability
 * 		of the backend to return values calibrated for this scale.
 *
 * \return RIG_OK if the operation has been sucessful, otherwise 
 * a negative value if an error occured (in which case, cause is 
 * set appropriately).
 *
 * \sa rig_has_get_level(), rig_set_level()
 */
int rig_get_level(RIG *rig, vfo_t vfo, setting_t level, value_t *val)
{
		const struct rig_caps *caps;
		int retcode;
		vfo_t curr_vfo;

		if (CHECK_RIG_ARG(rig) || !val)
			return -RIG_EINVAL;

		caps = rig->caps;

		if (caps->get_level == NULL || !rig_has_get_level(rig,level))
			return -RIG_ENAVAIL;

		if ((caps->targetable_vfo&RIG_TARGETABLE_ALL) ||
				vfo == RIG_VFO_CURR || vfo == rig->state.current_vfo)
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
 * \brief set a radio parameter
 * \param rig	The rig handle
 * \param parm	The parameter
 * \param val	The value to set the parameter to
 *
 *  Sets a parameter. 
 *  The parameter value \a val can be a float or an integer. See #value_t
 *  for more information.
 *
 * \return RIG_OK if the operation has been sucessful, otherwise 
 * a negative value if an error occured (in which case, cause is 
 * set appropriately).
 *
 * \sa rig_has_set_parm(), rig_get_parm()
 */
int rig_set_parm(RIG *rig, setting_t parm, value_t val)
{
		if (CHECK_RIG_ARG(rig))
			return -RIG_EINVAL;

		if (rig->caps->set_parm == NULL || !rig_has_set_parm(rig,parm))
			return -RIG_ENAVAIL;

		return rig->caps->set_parm(rig, parm, val);
}

/**
 * \brief get the value of a parameter
 * \param rig	The rig handle
 * \param parm	The parameter
 * \param val	The location where to store the value of \a parm
 *
 *  Retrieves the value of a \a parm.
 *  The parameter value \a val can be a float or an integer. See #value_t
 *  for more information.
 *
 * \return RIG_OK if the operation has been sucessful, otherwise 
 * a negative value if an error occured (in which case, cause is 
 * set appropriately).
 *
 * \sa rig_has_get_parm(), rig_set_parm()
 */
int rig_get_parm(RIG *rig, setting_t parm, value_t *val)
{
		if (CHECK_RIG_ARG(rig) || !val)
			return -RIG_EINVAL;

		if (rig->caps->get_parm == NULL || !rig_has_get_parm(rig,parm))
			return -RIG_ENAVAIL;

		return rig->caps->get_parm(rig, parm, val);
}

/**
 * \brief check retrieval ability of level settings
 * \param rig	The rig handle
 * \param level	The level settings
 *
 *  Checks if a rig is capable of *getting* a level setting.
 *  Since the \a level is an OR'ed bitwise argument, more than
 *  one level can be checked at the same time.
 *
 * 	EXAMPLE: if (rig_has_get_level(my_rig, RIG_LVL_STRENGTH)) disp_Smeter();
 *
 * \return a bit map of supported level settings that can be retrieved,
 * otherwise 0 if none supported.
 *
 * \sa rig_has_set_level(), rig_get_level()
 */
setting_t rig_has_get_level(RIG *rig, setting_t level)
{
		if (!rig || !rig->caps)
				return 0;

		return (rig->state.has_get_level & level);
}


/**
 * \brief check settable ability of level settings
 * \param rig	The rig handle
 * \param level	The level settings
 *
 *  Checks if a rig can *set* a level setting.
 *  Since the \a level is an OR'ed bitwise argument, more than
 *  one level can be check at the same time.
 *
 * 	EXAMPLE: if (rig_has_set_level(my_rig, RIG_LVL_RFPOWER)) crank_tx();
 *
 * \return a bit map of supported level settings that can be set,
 * otherwise 0 if none supported.
 *
 * \sa rig_has_get_level(), rig_set_level()
 */
setting_t rig_has_set_level(RIG *rig, setting_t level)
{
		if (!rig || !rig->caps)
				return 0;

		return (rig->state.has_set_level & level);
}

/**
 * \brief check retrieval ability of parameter settings
 * \param rig	The rig handle
 * \param parm	The parameter settings
 *
 *  Checks if a rig is capable of *getting* a parm setting.
 *  Since the \a parm is an OR'ed bitwise argument, more than
 *  one parameter can be checked at the same time.
 *
 * 	EXAMPLE: if (rig_has_get_parm(my_rig, RIG_PARM_ANN)) good4you();
 *
 * \return a bit map of supported parameter settings that can be retrieved,
 * otherwise 0 if none supported.
 *
 * \sa rig_has_set_parm(), rig_get_parm()
 */
setting_t rig_has_get_parm(RIG *rig, setting_t parm)
{
		if (!rig || !rig->caps)
				return 0;

		return (rig->state.has_get_parm & parm);
}


/**
 * \brief check settable ability of parameter settings
 * \param rig	The rig handle
 * \param parm	The parameter settings
 *
 *  Checks if a rig can *set* a parameter setting.
 *  Since the \a parm is an OR'ed bitwise argument, more than
 *  one parameter can be check at the same time.
 *
 * 	EXAMPLE: if (rig_has_set_parm(my_rig, RIG_PARM_ANN)) announce_all();
 *
 * \return a bit map of supported parameter settings that can be set,
 * otherwise 0 if none supported.
 *
 * \sa rig_has_get_parm(), rig_set_parm()
 */
setting_t rig_has_set_parm(RIG *rig, setting_t parm)
{
		if (!rig || !rig->caps)
				return 0;

		return (rig->state.has_set_parm & parm);
}

/**
 * \brief check ability of radio functions
 * \param rig	The rig handle
 * \param func	The functions
 *
 *  Checks if a rig supports a set of functions.
 *  Since the \a func is an OR'ed bitwise argument, more than
 *  one function can be checked at the same time.
 *
 * 	EXAMPLE: if (rig_has_get_func(my_rig,RIG_FUNC_FAGC)) disp_fagc_button();
 *
 *  \return a bit map of supported functions, 
 *  otherwise 0 if none supported.
 *
 * \sa rig_has_set_func(), rig_get_func()
 */
setting_t rig_has_get_func(RIG *rig, setting_t func)
{
		if (!rig || !rig->caps)
				return 0;

		return (rig->state.has_get_func & func);
}

/**
 * \brief check ability of radio functions
 * \param rig	The rig handle
 * \param func	The functions
 *
 *  Checks if a rig supports a set of functions.
 *  Since the \a func is an OR'ed bitwise argument, more than
 *  one function can be checked at the same time.
 *
 * 	EXAMPLE: if (rig_has_set_func(my_rig,RIG_FUNC_FAGC)) disp_fagc_button();
 *
 * \return a bit map of supported functions, 
 * otherwise 0 if none supported.
 *
 * \sa rig_set_func(), rig_has_get_func()
 */
setting_t rig_has_set_func(RIG *rig, setting_t func)
{
		if (!rig || !rig->caps)
				return 0;

		return (rig->state.has_set_func & func);
}

/**
 * \brief activate/desactivate functions of radio
 * \param rig	The rig handle
 * \param vfo	The target VFO
 * \param func	The functions to activate
 * \param status	The status (on or off) to set to
 *
 * Activate/desactivate functions of the radio.
 *
 * \return RIG_OK if the operation has been sucessful, otherwise 
 * a negative value if an error occured (in which case, cause is 
 * set appropriately).
 *
 * \sa rig_get_func()
 */

int rig_set_func(RIG *rig, vfo_t vfo, setting_t func, int status)
{
		const struct rig_caps *caps;
		int retcode;
		vfo_t curr_vfo;

		if (CHECK_RIG_ARG(rig))
			return -RIG_EINVAL;

		caps = rig->caps;

		if (caps->set_func == NULL || !rig_has_set_func(rig,func))
			return -RIG_ENAVAIL;

		if ((caps->targetable_vfo&RIG_TARGETABLE_ALL) ||
				vfo == RIG_VFO_CURR || vfo == rig->state.current_vfo)
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
 * \brief get the status of functions of the radio
 * \param rig	The rig handle
 * \param vfo	The target VFO
 * \param func	The functions to get the status
 * \param status	The location where to store the function status
 *
 *  Retrieves the status of functions of the radio. Only the function bits 
 *  set to 1 will be queried.
 *  On return, \a func will hold the status of functions (bit set to 1 =
 *  activated, bit set to 0 = desactivated).
 *
 * \return RIG_OK if the operation has been sucessful, otherwise 
 * a negative value if an error occured (in which case, cause is 
 * set appropriately).
 *
 * \sa rig_set_func()
 */
int rig_get_func(RIG *rig, vfo_t vfo, setting_t func, int *status)
{
		const struct rig_caps *caps;
		int retcode;
		vfo_t curr_vfo;

		if (CHECK_RIG_ARG(rig) || !func)
			return -RIG_EINVAL;

		caps = rig->caps;

		if (caps->get_func == NULL || !rig_has_get_func(rig,func))
			return -RIG_ENAVAIL;

		if ((caps->targetable_vfo&RIG_TARGETABLE_ALL) ||
				vfo == RIG_VFO_CURR || vfo == rig->state.current_vfo)
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
 * \brief set a radio level extra parameter
 * \param rig	The rig handle
 * \param vfo	The target VFO
 * \param token	The parameter
 * \param val	The value to set the parameter to
 *
 *  Sets an level extra parameter. 
 *
 * \return RIG_OK if the operation has been sucessful, otherwise 
 * a negative value if an error occured (in which case, cause is 
 * set appropriately).
 *
 * \sa rig_get_ext_level()
 */
int rig_set_ext_level(RIG *rig, vfo_t vfo, token_t token, value_t val)
{
	const struct rig_caps *caps;
	int retcode;
	vfo_t curr_vfo;

	if (CHECK_RIG_ARG(rig))
		return -RIG_EINVAL;

	caps = rig->caps;

	if (caps->set_ext_level == NULL)
		return -RIG_ENAVAIL;

	if ((caps->targetable_vfo&RIG_TARGETABLE_ALL) ||
		vfo == RIG_VFO_CURR || vfo == rig->state.current_vfo)
		return caps->set_ext_level(rig, vfo, token, val);

	if (!caps->set_vfo)
		return -RIG_ENTARGET;
	curr_vfo = rig->state.current_vfo;
	retcode = caps->set_vfo(rig, vfo);
	if (retcode != RIG_OK)
		return retcode;

	retcode = caps->set_ext_level(rig, vfo, token, val);
	caps->set_vfo(rig, curr_vfo);
	return retcode;
}

/**
 * \brief get the value of a level extra parameter
 * \param rig	The rig handle
 * \param vfo	The target VFO
 * \param token	The parameter
 * \param val	The location where to store the value of \a token
 *
 *  Retrieves the value of a level extra paramter associated with \a token.
 *
 * \return RIG_OK if the operation has been sucessful, otherwise 
 * a negative value if an error occured (in which case, cause is 
 * set appropriately).
 *
 * \sa rig_set_ext_level()
 */
int rig_get_ext_level(RIG *rig, vfo_t vfo, token_t token, value_t *val)
{
	const struct rig_caps *caps;
	int retcode;
	vfo_t curr_vfo;

	if (CHECK_RIG_ARG(rig) || !val)
		return -RIG_EINVAL;

	caps = rig->caps;

	if (caps->get_ext_level == NULL)
		return -RIG_ENAVAIL;

	if ((caps->targetable_vfo&RIG_TARGETABLE_ALL) ||
		vfo == RIG_VFO_CURR || vfo == rig->state.current_vfo)
		return caps->get_ext_level(rig, vfo, token, val);

	if (!caps->set_vfo)
		return -RIG_ENTARGET;
	curr_vfo = rig->state.current_vfo;
	retcode = caps->set_vfo(rig, vfo);
	if (retcode != RIG_OK)
		return retcode;

	retcode = caps->get_ext_level(rig, vfo, token, val);
	caps->set_vfo(rig, curr_vfo);
	return retcode;
}

/**
 * \brief set a radio parm extra parameter
 * \param rig	The rig handle
 * \param token	The parameter
 * \param val	The value to set the parameter to
 *
 *  Sets an parm extra parameter. 
 *
 * \return RIG_OK if the operation has been sucessful, otherwise 
 * a negative value if an error occured (in which case, cause is 
 * set appropriately).
 *
 * \sa rig_get_ext_parm()
 */
int rig_set_ext_parm(RIG *rig, token_t token, value_t val)
{
	if (CHECK_RIG_ARG(rig))
		return -RIG_EINVAL;

	if (rig->caps->set_ext_parm == NULL)
		return -RIG_ENAVAIL;

	return rig->caps->set_ext_parm(rig, token, val);
}

/**
 * \brief get the value of a parm extra parameter
 * \param rig	The rig handle
 * \param token	The parameter
 * \param val	The location where to store the value of \a token
 *
 *  Retrieves the value of a parm extra paramter associated with \a token.
 *
 * \return RIG_OK if the operation has been sucessful, otherwise 
 * a negative value if an error occured (in which case, cause is 
 * set appropriately).
 *
 * \sa rig_set_ext_parm()
 */
int rig_get_ext_parm(RIG *rig, token_t token, value_t *val)
{
	if (CHECK_RIG_ARG(rig) || !val)
		return -RIG_EINVAL;

	if (rig->caps->get_ext_parm == NULL)
		return -RIG_ENAVAIL;

	return rig->caps->get_ext_parm(rig, token, val);
}



/**
 * \brief set a radio configuration parameter
 * \param rig	The rig handle
 * \param token	The parameter
 * \param val	The value to set the parameter to
 *
 *  Sets a configuration parameter. 
 *
 * \return RIG_OK if the operation has been sucessful, otherwise 
 * a negative value if an error occured (in which case, cause is 
 * set appropriately).
 *
 * \sa rig_get_conf()
 */
int rig_set_conf(RIG *rig, token_t token, const char *val)
{
		if (!rig || !rig->caps)
			return -RIG_EINVAL;

		if (IS_TOKEN_FRONTEND(token))
				return frontend_set_conf(rig, token, val);

		if (rig->caps->set_conf == NULL)
			return -RIG_ENAVAIL;

		return rig->caps->set_conf(rig, token, val);
}

/**
 * \brief get the value of a configuration parameter
 * \param rig	The rig handle
 * \param token	The parameter
 * \param val	The location where to store the value of config \a token
 *
 *  Retrieves the value of a configuration paramter associated with \a token.
 *  The location pointed to by val must be large enough to hold the value of the config.
 *
 * \return RIG_OK if the operation has been sucessful, otherwise 
 * a negative value if an error occured (in which case, cause is 
 * set appropriately).
 *
 * \sa rig_set_conf()
 */
int rig_get_conf(RIG *rig, token_t token, char *val)
{
		if (!rig || !rig->caps || !val)
			return -RIG_EINVAL;

		if (IS_TOKEN_FRONTEND(token))
				return frontend_get_conf(rig, token, val);

		if (rig->caps->get_conf == NULL)
			return -RIG_ENAVAIL;

		return rig->caps->get_conf(rig, token, val);
}


/**
 * \brief set the current memory channel number
 * \param rig	The rig handle
 * \param vfo	The target VFO
 * \param ch	The memory channel number
 *
 *  Sets the current memory channel number.
 *  It is not mandatory for the radio to be in memory mode. Actually
 *  it depends on rigs. YMMV.
 *
 * \return RIG_OK if the operation has been sucessful, otherwise 
 * a negative value if an error occured (in which case, cause is 
 * set appropriately).
 *
 * \sa rig_get_mem()
 */

int rig_set_mem(RIG *rig, vfo_t vfo, int ch)
{
		const struct rig_caps *caps;
		int retcode;
		vfo_t curr_vfo;

		if (CHECK_RIG_ARG(rig))
			return -RIG_EINVAL;

		caps = rig->caps;

		if (caps->set_mem == NULL)
			return -RIG_ENAVAIL;

		if ((caps->targetable_vfo&RIG_TARGETABLE_ALL) ||
				vfo == RIG_VFO_CURR || vfo == rig->state.current_vfo)
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
 * \brief get the current memory channel number
 * \param rig	The rig handle
 * \param vfo	The target VFO
 * \param ch	The location where to store the current memory channel number
 *
 *  Retrieves the current memory channel number.
 *  It is not mandatory for the radio to be in memory mode. Actually
 *  it depends on rigs. YMMV.
 *
 * \return RIG_OK if the operation has been sucessful, otherwise 
 * a negative value if an error occured (in which case, cause is 
 * set appropriately).
 *
 * \sa rig_set_mem()
 */
int rig_get_mem(RIG *rig, vfo_t vfo, int *ch)
{
		const struct rig_caps *caps;
		int retcode;
		vfo_t curr_vfo;

		if (CHECK_RIG_ARG(rig) || !ch)
			return -RIG_EINVAL;

		caps = rig->caps;

		if (caps->get_mem == NULL)
			return -RIG_ENAVAIL;

		if ((caps->targetable_vfo&RIG_TARGETABLE_ALL) ||
				vfo == RIG_VFO_CURR || vfo == rig->state.current_vfo)
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
 * \brief check retrieval ability of VFO operations
 * \param rig	The rig handle
 * \param op	The VFO op
 *
 *  Checks if a rig is capable of executing a VFO operation. 
 *  Since the \a op is an OR'ed bitmap argument, more than
 *  one op can be checked at the same time.
 *
 * 	EXAMPLE: if (rig_has_vfo_op(my_rig, RIG_OP_CPY)) disp_VFOcpy_btn();
 *
 * \return a bit map mask of supported op settings that can be retrieved,
 * otherwise 0 if none supported.
 *
 * \sa rig_vfo_op()
 */
vfo_op_t rig_has_vfo_op(RIG *rig, vfo_op_t op)
{
		if (!rig || !rig->caps)
				return 0;

		return (rig->caps->vfo_ops & op);
}

/**
 * \brief perform Memory/VFO operations
 * \param rig	The rig handle
 * \param vfo	The target VFO
 * \param op	The Memory/VFO operation to perform
 *
 *  Performs Memory/VFO operation.
 *  See #vfo_op_t for more information.
 *
 * \return RIG_OK if the operation has been sucessful, otherwise 
 * a negative value if an error occured (in which case, cause is 
 * set appropriately).
 *
 * \sa rig_has_vfo_op()
 */

int rig_vfo_op(RIG *rig, vfo_t vfo, vfo_op_t op)
{
		const struct rig_caps *caps;
		int retcode;
		vfo_t curr_vfo;

		if (CHECK_RIG_ARG(rig))
			return -RIG_EINVAL;

		caps = rig->caps;

		if (caps->vfo_op == NULL || !rig_has_vfo_op(rig,op))
			return -RIG_ENAVAIL;

		if ((caps->targetable_vfo&RIG_TARGETABLE_ALL) ||
				vfo == RIG_VFO_CURR || vfo == rig->state.current_vfo)
			return caps->vfo_op(rig, vfo, op);

		if (!caps->set_vfo)
			return -RIG_ENTARGET;
		curr_vfo = rig->state.current_vfo;
		retcode = caps->set_vfo(rig, vfo);
		if (retcode != RIG_OK)
				return retcode;

		retcode = caps->vfo_op(rig, vfo, op);
		caps->set_vfo(rig, curr_vfo);
		return retcode;
}

/**
 * \brief check availability of scaning functions
 * \param rig	The rig handle
 * \param scan	The scan op
 *
 *  Checks if a rig is capable of performing a scan operation. 
 *  Since the \a scan parameter is an OR'ed bitmap argument, more than
 *  one op can be checked at the same time.
 *
 * 	EXAMPLE: if (rig_has_scan(my_rig, RIG_SCAN_PRIO)) disp_SCANprio_btn();
 *
 * \return a bit map of supported scan settings that can be retrieved,
 * otherwise 0 if none supported.
 *
 * \sa rig_scan()
 */
scan_t rig_has_scan(RIG *rig, scan_t scan)
{
		if (!rig || !rig->caps)
				return 0;

		return (rig->caps->scan_ops & scan);
}

/**
 * \brief perform Memory/VFO operations
 * \param rig	The rig handle
 * \param vfo	The target VFO
 * \param scan	The scanning operation to perform
 * \param ch	Optional channel argument used for the scan.
 *
 *  Performs scanning operation.
 *  See #scan_t for more information.
 *
 * \return RIG_OK if the operation has been sucessful, otherwise 
 * a negative value if an error occured (in which case, cause is 
 * set appropriately).
 *
 * \sa rig_has_scan()
 */

int rig_scan(RIG *rig, vfo_t vfo, scan_t scan, int ch)
{
		const struct rig_caps *caps;
		int retcode;
		vfo_t curr_vfo;

		if (CHECK_RIG_ARG(rig))
			return -RIG_EINVAL;

		caps = rig->caps;

		if (caps->scan == NULL || 
						(scan!=RIG_SCAN_STOP && !rig_has_scan(rig, scan)))
			return -RIG_ENAVAIL;

		if ((caps->targetable_vfo&RIG_TARGETABLE_ALL) ||
				vfo == RIG_VFO_CURR || vfo == rig->state.current_vfo)
			return caps->scan(rig, vfo, scan, ch);

		if (!caps->set_vfo)
			return -RIG_ENTARGET;
		curr_vfo = rig->state.current_vfo;
		retcode = caps->set_vfo(rig, vfo);
		if (retcode != RIG_OK)
				return retcode;

		retcode = caps->scan(rig, vfo, scan, ch);
		caps->set_vfo(rig, curr_vfo);
		return retcode;
}

/**
 * \brief send DTMF digits
 * \param rig	The rig handle
 * \param vfo	The target VFO
 * \param digits	Digits to be send
 *
 *  Sends DTMF digits.
 *  See DTMF change speed, etc. (TODO).
 *
 * \return RIG_OK if the operation has been sucessful, otherwise 
 * a negative value if an error occured (in which case, cause is 
 * set appropriately).
 *
 */

int rig_send_dtmf(RIG *rig, vfo_t vfo, const char *digits)
{
		const struct rig_caps *caps;
		int retcode;
		vfo_t curr_vfo;

		if (CHECK_RIG_ARG(rig) || !digits)
			return -RIG_EINVAL;

		caps = rig->caps;

		if (caps->send_dtmf == NULL)
			return -RIG_ENAVAIL;

		if ((caps->targetable_vfo&RIG_TARGETABLE_ALL) ||
				vfo == RIG_VFO_CURR || vfo == rig->state.current_vfo)
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
 * \brief receive DTMF digits
 * \param rig	The rig handle
 * \param vfo	The target VFO
 * \param digits	Location where the digits are to be stored 
 * \param length	in: max length of buffer, out: number really read.
 *
 *  Receives DTMF digits (not blocking).
 *  See DTMF change speed, etc. (TODO).
 *
 * \return RIG_OK if the operation has been sucessful, otherwise 
 * a negative value if an error occured (in which case, cause is 
 * set appropriately).
 *
 */

int rig_recv_dtmf(RIG *rig, vfo_t vfo, char *digits, int *length)
{
		const struct rig_caps *caps;
		int retcode;
		vfo_t curr_vfo;

		if (CHECK_RIG_ARG(rig) || !digits || !length)
			return -RIG_EINVAL;

		caps = rig->caps;

		if (caps->recv_dtmf == NULL)
			return -RIG_ENAVAIL;

		if ((caps->targetable_vfo&RIG_TARGETABLE_ALL) ||
				vfo == RIG_VFO_CURR || vfo == rig->state.current_vfo)
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
 * \brief send morse code
 * \param rig	The rig handle
 * \param vfo	The target VFO
 * \param msg	Message to be sent
 *
 *  Sends morse message.
 *  See keyer change speed, etc. (TODO).
 *
 * \return RIG_OK if the operation has been sucessful, otherwise 
 * a negative value if an error occured (in which case, cause is 
 * set appropriately).
 *
 */

int rig_send_morse(RIG *rig, vfo_t vfo, const char *msg)
{
		const struct rig_caps *caps;
		int retcode;
		vfo_t curr_vfo;

		if (CHECK_RIG_ARG(rig) || !msg)
			return -RIG_EINVAL;

		caps = rig->caps;

		if (caps->send_morse == NULL)
			return -RIG_ENAVAIL;

		if ((caps->targetable_vfo&RIG_TARGETABLE_ALL) ||
				vfo == RIG_VFO_CURR || vfo == rig->state.current_vfo)
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
 * \brief set the current memory bank
 * \param rig	The rig handle
 * \param vfo	The target VFO
 * \param bank	The memory bank
 *
 *  Sets the current memory bank number.
 *  It is not mandatory for the radio to be in memory mode. Actually
 *  it depends on rigs. YMMV.
 *
 * \return RIG_OK if the operation has been sucessful, otherwise 
 * a negative value if an error occured (in which case, cause is 
 * set appropriately).
 *
 * \sa rig_set_mem()
 */

int rig_set_bank(RIG *rig, vfo_t vfo, int bank)
{
		const struct rig_caps *caps;
		int retcode;
		vfo_t curr_vfo;

		if (CHECK_RIG_ARG(rig))
			return -RIG_EINVAL;

		caps = rig->caps;

		if (caps->set_bank == NULL)
			return -RIG_ENAVAIL;
		
		if ((caps->targetable_vfo&RIG_TARGETABLE_ALL) ||
				vfo == RIG_VFO_CURR || vfo == rig->state.current_vfo)
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

#ifndef DOC_HIDDEN
/*
 * call on every ext_levels of a rig
 */
static int generic_retr_extl(RIG *rig, const struct confparams *cfp, rig_ptr_t ptr)
{
	channel_t *chan = (channel_t *)ptr;
	struct ext_list *p;
	unsigned el_size = 0;

	if (chan->ext_levels == NULL)
		p = chan->ext_levels = malloc(2*sizeof(struct ext_list));
	else {
		for (p = chan->ext_levels; !RIG_IS_EXT_END(*p); p++)
			el_size += sizeof(struct ext_list);
		chan->ext_levels = realloc(chan->ext_levels,
				el_size+sizeof(struct ext_list));
	}

	p->token = cfp->token;
	rig_get_ext_level(rig, RIG_VFO_CURR, p->token, &p->val);
	p++;
	p->token = 0;	/* RIG_EXT_END */

	return 1;	/* process them all */
}

/*
 * stores current VFO state into chan by emulating rig_get_channel
 */
int generic_save_channel(RIG *rig, channel_t *chan)
{
  int i;
  int chan_num;
  vfo_t vfo;

  if (CHECK_RIG_ARG(rig) || !chan)
	return -RIG_EINVAL;

  chan_num = chan->channel_num;
  vfo = chan->vfo;
  memset(chan, 0, sizeof(channel_t));
  chan->channel_num = chan_num;
  chan->vfo = vfo;

  rig_get_vfo(rig, &chan->vfo);
  rig_get_freq(rig, RIG_VFO_CURR, &chan->freq);
  rig_get_mode(rig, RIG_VFO_CURR, &chan->mode, &chan->width);

  chan->split = RIG_SPLIT_OFF;
  rig_get_split(rig, RIG_VFO_CURR, &chan->split);
  if (chan->split != RIG_SPLIT_OFF) {
  	rig_get_split_freq(rig, RIG_VFO_CURR, &chan->tx_freq);
  	rig_get_split_mode(rig, RIG_VFO_CURR, &chan->tx_mode, &chan->tx_width);
  } else {
  	chan->tx_freq = chan->freq;
  	chan->tx_mode = chan->mode;
	chan->tx_width = chan->width;
  }
  rig_get_rptr_shift(rig, RIG_VFO_CURR, &chan->rptr_shift);
  rig_get_rptr_offs(rig, RIG_VFO_CURR, &chan->rptr_offs);

  rig_get_ant(rig, RIG_VFO_CURR, &chan->ant);
  rig_get_ts(rig, RIG_VFO_CURR, &chan->tuning_step);
  rig_get_rit(rig, RIG_VFO_CURR, &chan->rit);
  rig_get_xit(rig, RIG_VFO_CURR, &chan->xit);

  for (i=0; i<RIG_SETTING_MAX; i++)
	if (RIG_LEVEL_SET(rig_idx2setting(i)))
  		rig_get_level(rig, RIG_VFO_CURR, rig_idx2setting(i), &chan->levels[i]);

  chan->funcs = 0;
  for (i=0; i<RIG_SETTING_MAX; i++) {
  	int fstatus;
  	if (rig_get_func(rig, RIG_VFO_CURR, rig_idx2setting(i), &fstatus) == RIG_OK)
		chan->funcs |= fstatus ? rig_idx2setting(i) : 0;
  }

  rig_get_ctcss_tone(rig, RIG_VFO_CURR, &chan->ctcss_tone);
  rig_get_ctcss_sql(rig, RIG_VFO_CURR, &chan->ctcss_sql);
  rig_get_dcs_code(rig, RIG_VFO_CURR, &chan->dcs_code);
  rig_get_dcs_sql(rig, RIG_VFO_CURR, &chan->dcs_sql);
/* rig_get_mem_name(rig, RIG_VFO_CURR, chan->channel_desc); */

  rig_ext_level_foreach(rig, generic_retr_extl, (rig_ptr_t)chan);

  return RIG_OK;
}


/*
 * Restores chan into current VFO state by emulating rig_set_channel
 */
int generic_restore_channel(RIG *rig, const channel_t *chan)
{
  int i;
  struct ext_list *p;

  if (CHECK_RIG_ARG(rig) || !chan)
	return -RIG_EINVAL;

  rig_set_vfo(rig, chan->vfo);
  rig_set_freq(rig, RIG_VFO_CURR, chan->freq);
  rig_set_mode(rig, RIG_VFO_CURR, chan->mode, chan->width);
  rig_set_split(rig, RIG_VFO_CURR, chan->split);
  if (chan->split != RIG_SPLIT_OFF) {
  	rig_set_split_freq(rig, RIG_VFO_CURR, chan->tx_freq);
  	rig_set_split_mode(rig, RIG_VFO_CURR, chan->tx_mode, chan->tx_width);
  }
  rig_set_rptr_shift(rig, RIG_VFO_CURR, chan->rptr_shift);
  rig_set_rptr_offs(rig, RIG_VFO_CURR, chan->rptr_offs);
  for (i=0; i<RIG_SETTING_MAX; i++)
  	rig_set_level(rig, RIG_VFO_CURR, rig_idx2setting(i), chan->levels[i]);

  rig_set_ant(rig, RIG_VFO_CURR, chan->ant);
  rig_set_ts(rig, RIG_VFO_CURR, chan->tuning_step);
  rig_set_rit(rig, RIG_VFO_CURR, chan->rit);
  rig_set_xit(rig, RIG_VFO_CURR, chan->xit);

  for (i=0; i<RIG_SETTING_MAX; i++)
  	rig_set_func(rig, RIG_VFO_CURR, rig_idx2setting(i), 
					chan->funcs & rig_idx2setting(i));

  rig_set_ctcss_tone(rig, RIG_VFO_CURR, chan->ctcss_tone);
  rig_set_ctcss_sql(rig, RIG_VFO_CURR, chan->ctcss_sql);
  rig_set_dcs_code(rig, RIG_VFO_CURR, chan->dcs_code);
  rig_set_dcs_sql(rig, RIG_VFO_CURR, chan->dcs_sql);
/* rig_set_mem_name(rig, RIG_VFO_CURR, chan->channel_desc); */

  for (p = chan->ext_levels; !RIG_IS_EXT_END(*p); p++)
	  rig_set_ext_level(rig, RIG_VFO_CURR, p->token, p->val);

  return RIG_OK;
}
#endif	/* !DOC_HIDDEN */

/**
 * \brief set channel data
 * \param rig	The rig handle
 * \param chan	The location of data to set for this channel
 *
 *  Sets the data associated with a channel. This channel can either
 *  be the state of a VFO specified by \a chan->vfo, or a memory channel
 *  specified with \a chan->vfo = RIG_VFO_MEM and \a chan->channel_num.
 *  See #channel_t for more information.
 *
 *  The rig_set_channel is supposed to have no impact on the current VFO
 *  and memory number selected. Depending on backend and rig capabilities,
 *  the chan struct may not be set completely.
 *
 * \return RIG_OK if the operation has been sucessful, otherwise 
 * a negative value if an error occured (in which case, cause is 
 * set appropriately).
 *
 * \sa rig_get_channel()
 */

int rig_set_channel(RIG *rig, const channel_t *chan)
{
	struct rig_caps *rc;
	int curr_chan_num, get_mem_status = RIG_OK;
	vfo_t curr_vfo;
	vfo_t vfo; /* requested vfo */
	int retcode;
#ifdef PARANOID_CHANNEL_HANDLING
	channel_t curr_chan;
#endif

	if (CHECK_RIG_ARG(rig) || !chan)
		return -RIG_EINVAL;

	/*
	 * TODO: check chan->channel_num is valid
	 */

	rc = rig->caps;

	if (rc->set_channel)
		return rc->set_channel(rig, chan);

	/*
	 * if not available, emulate it
	 * Optional: get_vfo, set_vfo,
	 * TODO: check return codes
	 */

	vfo = chan->vfo;
	if (vfo == RIG_VFO_MEM && !rc->set_mem)
		return -RIG_ENAVAIL;

	if (vfo == RIG_VFO_CURR)
		return generic_restore_channel(rig, chan);

	if (!rc->set_vfo)
		return -RIG_ENTARGET;

	curr_vfo = rig->state.current_vfo;
	/* may be needed if the restore_channel has some side effects */
#ifdef PARANOID_CHANNEL_HANDLING
	generic_save_channel(rig, &curr_chan);
#endif

	if (vfo == RIG_VFO_MEM)
		get_mem_status = rig_get_mem(rig, RIG_VFO_CURR, &curr_chan_num);

	retcode = rig_set_vfo(rig, vfo);
	if (retcode != RIG_OK)
		return retcode;

	if (vfo == RIG_VFO_MEM)
		rig_set_mem(rig, RIG_VFO_CURR, chan->channel_num);

	retcode = generic_restore_channel(rig, chan);

	/* restore current memory number */
	if (vfo == RIG_VFO_MEM && get_mem_status == RIG_OK)
		rig_set_mem(rig, RIG_VFO_CURR, curr_chan_num);

	rig_set_vfo(rig, curr_vfo);

#ifdef PARANOID_CHANNEL_HANDLING
	generic_restore_channel(rig, &curr_chan);
#endif
	return retcode;
}

/**
 * \brief get channel data
 * \param rig	The rig handle
 * \param chan	The location where to store the channel data
 *
 *  Retrieves the data associated with a channel. This channel can either
 *  be the state of a VFO specified by \a chan->vfo, or a memory channel
 *  specified with \a chan->vfo = RIG_VFO_MEM and \a chan->channel_num.
 *  See #channel_t for more information.
 *
 *  Example:
\code
  channel_t chan;
  int err;

  chan->vfo = RIG_VFO_MEM;
  chan->channel_num = 10;
  err = rig_get_channel(rig, &chan);
  if (err != RIG_OK)
  	error("get_channel failed: %s", rigerror(err));

\endcode
 *
 *  The rig_get_channel is supposed to have no impact on the current VFO
 *  and memory number selected. Depending on backend and rig capabilities,
 *  the chan struct may not be filled in completely.
 *
 *  Note: chan->ext_levels is a pointer to a newly mallocated memory. 
 *  This is the responsability of the caller to manage and eventually
 *  free it.
 *
 * \return RIG_OK if the operation has been sucessful, otherwise 
 * a negative value if an error occured (in which case, cause is 
 * set appropriately).
 *
 * \sa rig_set_channel()
 */
int rig_get_channel(RIG *rig, channel_t *chan)
{
	struct rig_caps *rc;
	int curr_chan_num, get_mem_status = RIG_OK;
	vfo_t curr_vfo;
	vfo_t vfo;	/* requested vfo */
	int retcode;
#ifdef PARANOID_CHANNEL_HANDLING
	channel_t curr_chan;
#endif

	if (CHECK_RIG_ARG(rig) || !chan)
		return -RIG_EINVAL;

	/*
	 * TODO: check chan->channel_num is valid
	 */

	rc = rig->caps;

	if (rc->get_channel)
		return rc->get_channel(rig, chan);

	/*
	 * if not available, emulate it
	 * Optional: get_vfo, set_vfo
	 * TODO: check return codes
	 */
	vfo = chan->vfo;
	if (vfo == RIG_VFO_MEM && !rc->set_mem)
		return -RIG_ENAVAIL;

	if (vfo == RIG_VFO_CURR)
		return generic_save_channel(rig, chan);

	if (!rc->set_vfo)
		return -RIG_ENTARGET;

	curr_vfo = rig->state.current_vfo;
	/* may be needed if the restore_channel has some side effects */
#ifdef PARANOID_CHANNEL_HANDLING
	generic_save_channel(rig, &curr_chan);
#endif

	if (vfo == RIG_VFO_MEM)
		get_mem_status = rig_get_mem(rig, RIG_VFO_CURR, &curr_chan_num);

	retcode = rig_set_vfo(rig, vfo);
	if (retcode != RIG_OK)
		return retcode;

	if (vfo == RIG_VFO_MEM)
		rig_set_mem(rig, RIG_VFO_CURR, chan->channel_num);

	retcode = generic_save_channel(rig, chan);

	/* restore current memory number */
	if (vfo == RIG_VFO_MEM && get_mem_status == RIG_OK)
		rig_set_mem(rig, RIG_VFO_CURR, curr_chan_num);

	rig_set_vfo(rig, curr_vfo);

#ifdef PARANOID_CHANNEL_HANDLING
	generic_restore_channel(rig, &curr_chan);
#endif
	return retcode;
}

/**
 * \brief find the freq_range of freq/mode
 * \param range_list	The range list to search from
 * \param freq	The frequency that will be part of this range
 * \param mode	The mode that will be part of this range
 *
 *  Returns a pointer to the #freq_range_t including \a freq and \a mode.
 *  Works for rx and tx range list as well.
 *
 * \return the location of the #freq_range_t if found,
 * otherwise NULL if not found or if \a range_list is invalid.
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
 * \brief set the callback for freq events
 * \param rig	The rig handle
 * \param cb	The callback to install
 * \param arg	A Pointer to some private data to pass later on to the callback
 *
 *  Install a callback for freq events, to be called when in transceive mode.
 *
 * \return RIG_OK if the operation has been sucessful, otherwise 
 * a negative value if an error occured (in which case, cause is 
 * set appropriately).
 *
 * \sa rig_set_trn()
 */

int rig_set_freq_callback(RIG *rig, freq_cb_t cb, rig_ptr_t arg)
{
		if (CHECK_RIG_ARG(rig))
			return -RIG_EINVAL;

		rig->callbacks.freq_event = cb;
		rig->callbacks.freq_arg = arg;

		return RIG_OK;
}

/**
 * \brief set the callback for mode events
 * \param rig	The rig handle
 * \param cb	The callback to install
 * \param arg	A Pointer to some private data to pass later on to the callback
 *
 *  Install a callback for mode events, to be called when in transceive mode.
 *
 * \return RIG_OK if the operation has been sucessful, otherwise 
 * a negative value if an error occured (in which case, cause is 
 * set appropriately).
 *
 * \sa rig_set_trn()
 */

int rig_set_mode_callback(RIG *rig, mode_cb_t cb, rig_ptr_t arg)
{
		if (CHECK_RIG_ARG(rig))
			return -RIG_EINVAL;

		rig->callbacks.mode_event = cb;
		rig->callbacks.mode_arg = arg;

		return RIG_OK;
}

/**
 * \brief set the callback for vfo events
 * \param rig	The rig handle
 * \param cb	The callback to install
 * \param arg	A Pointer to some private data to pass later on to the callback
 *
 *  Install a callback for vfo events, to be called when in transceive mode.
 *
 * \return RIG_OK if the operation has been sucessful, otherwise 
 * a negative value if an error occured (in which case, cause is 
 * set appropriately).
 *
 * \sa rig_set_trn()
 */

int rig_set_vfo_callback(RIG *rig, vfo_cb_t cb, rig_ptr_t arg)
{
		if (CHECK_RIG_ARG(rig))
			return -RIG_EINVAL;

		rig->callbacks.vfo_event = cb;
		rig->callbacks.vfo_arg = arg;

		return RIG_OK;
}

/**
 * \brief set the callback for ptt events
 * \param rig	The rig handle
 * \param cb	The callback to install
 * \param arg	A Pointer to some private data to pass later on to the callback
 *
 *  Install a callback for ptt events, to be called when in transceive mode.
 *
 * \return RIG_OK if the operation has been sucessful, otherwise 
 * a negative value if an error occured (in which case, cause is 
 * set appropriately).
 *
 * \sa rig_set_trn()
 */

int rig_set_ptt_callback(RIG *rig, ptt_cb_t cb, rig_ptr_t arg)
{
		if (CHECK_RIG_ARG(rig))
			return -RIG_EINVAL;

		rig->callbacks.ptt_event = cb;
		rig->callbacks.ptt_arg = arg;

		return RIG_OK;
}

/**
 * \brief set the callback for dcd events
 * \param rig	The rig handle
 * \param cb	The callback to install
 * \param arg	A Pointer to some private data to pass later on to the callback
 *
 *  Install a callback for dcd events, to be called when in transceive mode.
 *
 * \return RIG_OK if the operation has been sucessful, otherwise 
 * a negative value if an error occured (in which case, cause is 
 * set appropriately).
 *
 * \sa rig_set_trn()
 */

int rig_set_dcd_callback(RIG *rig, dcd_cb_t cb, rig_ptr_t arg)
{
		if (CHECK_RIG_ARG(rig))
			return -RIG_EINVAL;

		rig->callbacks.dcd_event = cb;
		rig->callbacks.dcd_arg = arg;

		return RIG_OK;
}

/**
 * \brief control the transceive mode 
 * \param rig	The rig handle
 * \param trn	The transceive status to set to
 *
 *  Enable/disable the transceive handling of a rig and kick off async mode.
 *
 * \return RIG_OK if the operation has been sucessful, otherwise 
 * a negative value if an error occured (in which case, cause is 
 * set appropriately).
 *
 * \sa rig_get_trn()
 */

int rig_set_trn(RIG *rig, int trn)
{
		const struct rig_caps *caps;
		int status;

		if (CHECK_RIG_ARG(rig))
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
				if (status == RIG_OK && caps->set_trn)
						return caps->set_trn(rig, RIG_TRN_RIG);
				else
						return status;
			} else {
				return -RIG_ECONF;
			}
		} else {
				status = remove_trn_rig(rig);
				if (caps->set_trn)
						return caps->set_trn(rig, RIG_TRN_OFF);
				else
						return status;
		}

		return RIG_OK;
}

/**
 * \brief get the current transceive mode
 * \param rig	The rig handle
 * \param trn	The location where to store the current transceive mode
 *
 *  Retrieves the current status of the transceive mode, i.e. if radio 
 *  sends new status automatically when some changes happened on the radio.
 *
 * \return RIG_OK if the operation has been sucessful, otherwise 
 * a negative value if an error occured (in which case, cause is 
 * set appropriately).
 *
 * \sa rig_set_trn()
 */
int rig_get_trn(RIG *rig, int *trn)
{
		if (CHECK_RIG_ARG(rig) || !trn)
			return -RIG_EINVAL;

		if (rig->caps->get_trn == NULL)
			return -RIG_ENAVAIL;

		return rig->caps->get_trn(rig, trn);
}

/**
 * \brief get general information from the radio
 * \param rig	The rig handle
 *
 * Retrieves some general information from the radio.
 * This can include firmware revision, exact model name, or just nothing. 
 *
 * \return a pointer to freshly allocated memory containing the ASCIIZ string 
 * if the operation has been sucessful, otherwise NULL if an error occured
 * or get_info not part of capabilities.
 */
const char* rig_get_info(RIG *rig)
{
		if (CHECK_RIG_ARG(rig))
			return NULL;

		if (rig->caps->get_info == NULL)
			return NULL;

		return rig->caps->get_info(rig);
}


/**
 * \brief basically convert setting_t expressed 2^n to n
 * \param s	The setting to convert to
 *
 *  Converts a setting_t value expressed by 2^n to the value of n.
 *
 * \return the index such that 2^n is the setting, otherwise 0
 * if the setting was not found.
 */
int rig_setting2idx(setting_t s)
{
		int i;

		for (i = 0; i<RIG_SETTING_MAX; i++)
				if (s & rig_idx2setting(i))
						return i;

		return 0;
}


