/**
 * \file src/mem.c
 * \ingroup rig
 * \brief Memory and channel interface
 * \author Stephane Fillod
 * \date 2000-2006
 *
 * Hamlib interface is a frontend implementing wrapper functions.
 */

/*
 *  Hamlib Interface - mem/channel calls
 *  Copyright (c) 2000-2006 by Stephane Fillod
 *
 *	$Id: mem.c,v 1.9 2006-10-07 13:26:06 csete Exp $
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

#ifndef DOC_HIDDEN

#define CHECK_RIG_ARG(r) (!(r) || !(r)->caps || !(r)->state.comm_state)

#endif /* !DOC_HIDDEN */


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

int HAMLIB_API rig_set_mem(RIG *rig, vfo_t vfo, int ch)
{
	const struct rig_caps *caps;
	int retcode;
	vfo_t curr_vfo;

	if (CHECK_RIG_ARG(rig))
		return -RIG_EINVAL;

	caps = rig->caps;

	if (caps->set_mem == NULL)
		return -RIG_ENAVAIL;

	if ((caps->targetable_vfo&RIG_TARGETABLE_PURE) ||
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
int HAMLIB_API rig_get_mem(RIG *rig, vfo_t vfo, int *ch)
{
	const struct rig_caps *caps;
	int retcode;
	vfo_t curr_vfo;

	if (CHECK_RIG_ARG(rig) || !ch)
		return -RIG_EINVAL;

	caps = rig->caps;

	if (caps->get_mem == NULL)
		return -RIG_ENAVAIL;

	if ((caps->targetable_vfo&RIG_TARGETABLE_PURE) ||
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

int HAMLIB_API rig_set_bank(RIG *rig, vfo_t vfo, int bank)
{
	const struct rig_caps *caps;
	int retcode;
	vfo_t curr_vfo;

	if (CHECK_RIG_ARG(rig))
		return -RIG_EINVAL;

	caps = rig->caps;

	if (caps->set_bank == NULL)
		return -RIG_ENAVAIL;
	
	if ((caps->targetable_vfo&RIG_TARGETABLE_PURE) ||
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
static int generic_save_channel(RIG *rig, channel_t *chan)
{
  int i, retval;
  int chan_num;
  vfo_t vfo;

  chan_num = chan->channel_num;
  vfo = chan->vfo;
  memset(chan, 0, sizeof(channel_t));
  chan->channel_num = chan_num;
  chan->vfo = vfo;

  retval = rig_get_freq(rig, RIG_VFO_CURR, &chan->freq);
  /* empty channel ? */
  if (retval == -RIG_ENAVAIL || chan->freq == RIG_FREQ_NONE)
	  return -RIG_ENAVAIL;

  rig_get_vfo(rig, &chan->vfo);
  rig_get_mode(rig, RIG_VFO_CURR, &chan->mode, &chan->width);

  chan->split = RIG_SPLIT_OFF;
  rig_get_split_vfo(rig, RIG_VFO_CURR, &chan->split, &chan->tx_vfo);
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
static int generic_restore_channel(RIG *rig, const channel_t *chan)
{
  int i;
  struct ext_list *p;

  rig_set_vfo(rig, chan->vfo);
  rig_set_freq(rig, RIG_VFO_CURR, chan->freq);
  rig_set_mode(rig, RIG_VFO_CURR, chan->mode, chan->width);
  rig_set_split_vfo(rig, RIG_VFO_CURR, chan->split, chan->tx_vfo);
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

int HAMLIB_API rig_set_channel(RIG *rig, const channel_t *chan)
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
int HAMLIB_API rig_get_channel(RIG *rig, channel_t *chan)
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


#ifndef DOC_HIDDEN
int get_chan_all_cb_generic (RIG *rig, chan_cb_t chan_cb, rig_ptr_t arg)
{
	int i,j,retval;
	chan_t *chan_list = rig->state.chan_list;
	channel_t *chan;

 	for (i=0; !RIG_IS_CHAN_END(chan_list[i]) && i < CHANLSTSIZ; i++) {
		
		/*
		 * setting chan to NULL means the application
		 * has to provide a struct where to store data
		 * future data for channel channel_num
		 */
		chan = NULL;
		retval = chan_cb(rig, &chan, chan_list[i].start, chan_list, arg);
		if (retval != RIG_OK)
			return retval;
		if (chan == NULL)
			return -RIG_ENOMEM;

		for (j = chan_list[i].start; j <= chan_list[i].end; j++) {
			int chan_next;

			chan->vfo = RIG_VFO_MEM;
			chan->channel_num = j;

			/*
			 * TODO: if doesn't have rc->get_channel, special generic
			 */
			retval = rig_get_channel(rig, chan);

			if (retval == -RIG_ENAVAIL) {
				/*
				 * empty channel
				 *
				 * Should it continue or call chan_cb with special arg?
				 */
				continue;
			}

			if (retval != RIG_OK)
				return retval;

			chan_next = j < chan_list[i].end ? j+1 : j;

			chan_cb(rig, &chan, chan_next, chan_list, arg);
		}
	}

	return RIG_OK;
}

int set_chan_all_cb_generic (RIG *rig, chan_cb_t chan_cb, rig_ptr_t arg)
{
	int i,j,retval;
	chan_t *chan_list = rig->state.chan_list;
	channel_t *chan;

 	for (i=0; !RIG_IS_CHAN_END(chan_list[i]) && i < CHANLSTSIZ; i++) {

		for (j = chan_list[i].start; j <= chan_list[i].end; j++) {

			chan_cb(rig, &chan, j, chan_list, arg);
			chan->vfo = RIG_VFO_MEM;

			retval = rig_set_channel(rig, chan);

			if (retval != RIG_OK)
				return retval;
		}
	}

	return RIG_OK;
}

struct map_all_s {
	channel_t *chans;
	const struct confparams *cfgps;
	value_t *vals;
};

/*
 * chan_cb_t to be used for non cb get/set_all
 */
static int map_chan (RIG *rig, channel_t **chan, int channel_num, const chan_t *chan_list, rig_ptr_t arg)
{
	struct map_all_s *map_arg = (struct map_all_s*)arg;

	/* TODO: check channel_num within start-end of chan_list */

	*chan = &map_arg->chans[channel_num];

	return RIG_OK;
}

#endif	/* DOC_HIDDEN */

/**
 * \brief set all channel data, by callback
 * \param rig	The rig handle
 * \param chan_cb	Pointer to a callback function to provide channel data
 * \param arg	Arbitrary argument passed back to \a chan_cb
 *
 *  Write the data associated with a all the memory channels.
 *
 * \return RIG_OK if the operation has been sucessful, otherwise 
 * a negative value if an error occured (in which case, cause is 
 * set appropriately).
 *
 * \sa rig_set_chan_all(), rig_get_chan_all_cb()
 */
int HAMLIB_API rig_set_chan_all_cb (RIG *rig, chan_cb_t chan_cb, rig_ptr_t arg)
{
	struct rig_caps *rc;
	int retval;

	if (CHECK_RIG_ARG(rig) || !chan_cb)
		return -RIG_EINVAL;

	rc = rig->caps;

	if (rc->set_chan_all_cb)
		return rc->set_chan_all_cb(rig, chan_cb, arg);


	/* if not available, emulate it */
	retval = set_chan_all_cb_generic (rig, chan_cb, arg);

	return retval;
}

/**
 * \brief get all channel data, by callback
 * \param rig	The rig handle
 * \param chan_cb	Pointer to a callback function to retrieve channel data
 * \param arg	Arbitrary argument passed back to \a chan_cb
 *
 *  Retrieves the data associated with a all the memory channels.
 *
 *  \a chan_cb is called first with no data in \chan (chan equals NULL). 
 *  This means the application has to provide a struct where to store 
 *  future data for channel channel_num. If channel_num == chan->channel_num,
 *  the application does not need to provide a new allocated structure.
 *
 * \return RIG_OK if the operation has been sucessful, otherwise 
 * a negative value if an error occured (in which case, cause is 
 * set appropriately).
 *
 * \sa rig_get_chan_all(), rig_set_chan_all_cb()
 */
int HAMLIB_API rig_get_chan_all_cb (RIG *rig, chan_cb_t chan_cb, rig_ptr_t arg)
{
	struct rig_caps *rc;
	int retval;

	if (CHECK_RIG_ARG(rig) || !chan_cb)
		return -RIG_EINVAL;

	rc = rig->caps;

	if (rc->get_chan_all_cb)
		return rc->get_chan_all_cb(rig, chan_cb, arg);


	/* if not available, emulate it */
	retval = get_chan_all_cb_generic (rig, chan_cb, arg);

	return retval;
}


/**
 * \brief set all channel data
 * \param rig	The rig handle
 * \param chan	The location of data to set for all channels
 *
 * Write the data associated with all the memory channels.
 *
 * \return RIG_OK if the operation has been sucessful, otherwise 
 * a negative value if an error occured (in which case, cause is 
 * set appropriately).
 *
 * \sa rig_set_chan_all_cb(), rig_get_chan_all()
 */
int HAMLIB_API rig_set_chan_all (RIG *rig, const channel_t chans[])
{
	struct rig_caps *rc;
	struct map_all_s map_arg;
	int retval;

	if (CHECK_RIG_ARG(rig) || !chans)
		return -RIG_EINVAL;

	rc = rig->caps;
	map_arg.chans = (channel_t *) chans;

	if (rc->set_chan_all_cb)
		return rc->set_chan_all_cb(rig, map_chan, (rig_ptr_t)&map_arg);


	/* if not available, emulate it */
	retval = set_chan_all_cb_generic (rig, map_chan, (rig_ptr_t)&map_arg);

	return retval;
}


/**
 * \brief get all channel data
 * \param rig	The rig handle
 * \param chan	The location where to store all the channel data
 *
 * Retrieves the data associated with all the memory channels.
 *
 * \return RIG_OK if the operation has been sucessful, otherwise 
 * a negative value if an error occured (in which case, cause is 
 * set appropriately).
 *
 * \sa rig_get_chan_all_cb(), rig_set_chan_all()
 */
int HAMLIB_API rig_get_chan_all (RIG *rig, channel_t chans[])
{
	struct rig_caps *rc;
	struct map_all_s map_arg;
	int retval;

	if (CHECK_RIG_ARG(rig) || !chans)
		return -RIG_EINVAL;

	rc = rig->caps;
	map_arg.chans = chans;

	if (rc->get_chan_all_cb)
		return rc->get_chan_all_cb(rig, map_chan, (rig_ptr_t)&map_arg);

	/*
	 * if not available, emulate it
	 *
	 * TODO: save_current_state, restore_current_state
	 */
	retval = get_chan_all_cb_generic (rig, map_chan, (rig_ptr_t)&map_arg);

	return retval;
}

int HAMLIB_API rig_copy_channel(RIG *rig, channel_t *chan1, const channel_t *chan2)
{
	memcpy(chan1, chan2, sizeof(channel_t));
	/* TODO: ext_levels */
	return RIG_OK;
}

#ifndef DOC_HIDDEN
static int map_parm (RIG *rig, const struct confparams *cfgps, value_t *value,
		rig_ptr_t arg)
{
	return -RIG_ENIMPL;
}

int get_parm_all_cb_generic (RIG *rig, confval_cb_t parm_cb, rig_ptr_t cfgps,
			rig_ptr_t vals)
{
	return -RIG_ENIMPL;
}

int set_parm_all_cb_generic (RIG *rig, confval_cb_t parm_cb, rig_ptr_t cfgps,
			rig_ptr_t vals)
{
	return -RIG_ENIMPL;
}

#endif	/* DOC_HIDDEN */

/**
 * \brief set all channel and non-channel data by call-back
 * \param rig	The rig handle
 * \param chan_cb	The callback for channel data
 * \param parm_cb	The callback for non-channel(aka parm) data
 * \param arg	Cookie passed to \a chan_cb and \a parm_cb
 *
 * Writes the data associated with all the memory channels,
 * and rigs memory parameters, by callback.
 *
 * \return RIG_OK if the operation has been sucessful, otherwise 
 * a negative value if an error occured (in which case, cause is 
 * set appropriately).
 *
 * \sa rig_get_mem_all_cb(), rig_set_mem_all()
 * \todo finish coding and testing of mem_all functions
 */
int HAMLIB_API rig_set_mem_all_cb (RIG *rig, chan_cb_t chan_cb, confval_cb_t parm_cb, rig_ptr_t arg)
{
	struct rig_caps *rc;
	int retval;

	if (CHECK_RIG_ARG(rig) || !chan_cb)
		return -RIG_EINVAL;

	rc = rig->caps;

	if (rc->set_mem_all_cb)
		return rc->set_mem_all_cb(rig, chan_cb, parm_cb, arg);


	/* if not available, emulate it */
	retval = rig_set_chan_all_cb (rig, chan_cb, arg);
	if (retval != RIG_OK)
		return retval;

#if 0
	retval = rig_set_parm_all_cb (rig, parm_cb, arg);
	if (retval != RIG_OK)
		return retval;
#else
	return -RIG_ENIMPL;
#endif

	return retval;
}

/**
 * \brief get all channel and non-channel data by call-back
 * \param rig	The rig handle
 * \param chan_cb	The callback for channel data
 * \param parm_cb	The callback for non-channel(aka parm) data
 * \param arg	Cookie passed to \a chan_cb and \a parm_cb
 *
 * Retrieves the data associated with all the memory channels,
 * and rigs memory parameters, by callback.
 *
 * \return RIG_OK if the operation has been sucessful, otherwise 
 * a negative value if an error occured (in which case, cause is 
 * set appropriately).
 *
 * \sa rig_get_mem_all_cb(), rig_set_mem_all()
 *
 * \todo get all parm's
 * \todo finish coding and testing of mem_all functions
 */
int HAMLIB_API rig_get_mem_all_cb (RIG *rig, chan_cb_t chan_cb, confval_cb_t parm_cb, rig_ptr_t arg)
{
	struct rig_caps *rc;
	int retval;

	if (CHECK_RIG_ARG(rig) || !chan_cb)
		return -RIG_EINVAL;

	rc = rig->caps;

	if (rc->get_mem_all_cb)
		return rc->get_mem_all_cb(rig, chan_cb, parm_cb, arg);


	/* if not available, emulate it */
	retval = rig_get_chan_all_cb (rig, chan_cb, arg);
	if (retval != RIG_OK)
		return retval;

#if 0
	retval = rig_get_parm_cb (rig, parm_cb, arg);
	if (retval != RIG_OK)
		return retval;
#else
	return -RIG_ENIMPL;
#endif

	return retval;
}

/**
 * \brief set all channel and non-channel data
 * \param rig	The rig handle
 * \param chan_cb	The callback for channel data
 * \param parm_cb	The callback for non-channel(aka parm) data
 * \param arg	Cookie passed to \a chan_cb and \a parm_cb
 *
 * Writes the data associated with all the memory channels,
 * and rigs memory parameters.
 *
 * \return RIG_OK if the operation has been sucessful, otherwise 
 * a negative value if an error occured (in which case, cause is 
 * set appropriately).
 *
 * \sa rig_get_mem_all(), rig_set_mem_all_cb()
 *
 * \todo set all parm's
 * \todo finish coding and testing of mem_all functions
 */
int HAMLIB_API rig_set_mem_all (RIG *rig, const channel_t chans[], const struct confparams cfgps[], const value_t vals[])
{
	struct rig_caps *rc;
	int retval;
	struct map_all_s mem_all_arg;

	if (CHECK_RIG_ARG(rig) || !chans || !cfgps || !vals)
		return -RIG_EINVAL;

	rc = rig->caps;
	mem_all_arg.chans = (channel_t *) chans;
	mem_all_arg.cfgps = cfgps;
	mem_all_arg.vals = (value_t *) vals;

	if (rc->set_mem_all_cb)
		return rc->set_mem_all_cb(rig, map_chan, map_parm,
				(rig_ptr_t)&mem_all_arg);

	/* if not available, emulate it */
	retval = rig_set_chan_all (rig, chans);
	if (retval != RIG_OK)
		return retval;

#if 0
	retval = rig_set_parm_all (rig, parms);
	if (retval != RIG_OK)
		return retval;
#else
	return -RIG_ENIMPL;
#endif

	return retval;
}

/**
 * \brief get all channel and non-channel data
 * \param rig	The rig handle
 * \param chans	Array of channels where to store the data
 * \param cfgps	Array of config parameters to retrieve
 * \param vals	Array of values where to store the data
 *
 * Retrieves the data associated with all the memory channels,
 * and rigs memory parameters.
 *
 * \return RIG_OK if the operation has been sucessful, otherwise 
 * a negative value if an error occured (in which case, cause is 
 * set appropriately).
 *
 * \sa rig_get_mem_all(), rig_set_mem_all_cb()
 * \todo finish coding and testing of mem_all functions
 */
int HAMLIB_API rig_get_mem_all (RIG *rig, channel_t chans[], const struct confparams cfgps[], value_t vals[])
{
	struct rig_caps *rc;
	int retval;
	struct map_all_s mem_all_arg;

	if (CHECK_RIG_ARG(rig) || !chans || !cfgps || !vals)
		return -RIG_EINVAL;

	rc = rig->caps;
	mem_all_arg.chans = chans;
	mem_all_arg.cfgps = cfgps;
	mem_all_arg.vals = vals;

	if (rc->get_mem_all_cb)
		return rc->get_mem_all_cb(rig, map_chan, map_parm,
				(rig_ptr_t)&mem_all_arg);

	/*
	 * if not available, emulate it
	 *
	 * TODO: save_current_state, restore_current_state
	 */
	retval = rig_get_chan_all (rig, chans);
	if (retval != RIG_OK)
		return retval;

	retval = get_parm_all_cb_generic (rig, map_parm, (rig_ptr_t)cfgps,
			(rig_ptr_t)vals);

	return retval;
}

