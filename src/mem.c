/**
 * \file src/mem.c
 * \ingroup rig
 * \brief Memory and channel interface
 * \author Stephane Fillod
 * \date 2000-2003
 *
 * Hamlib interface is a frontend implementing wrapper functions.
 */

/*
 *  Hamlib Interface - mem/channel calls
 *  Copyright (c) 2000-2003 by Stephane Fillod
 *
 *	$Id: mem.c,v 1.2 2003-04-06 18:40:35 fillods Exp $
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
int generic_restore_channel(RIG *rig, const channel_t *chan)
{
  int i;
  struct ext_list *p;

  if (CHECK_RIG_ARG(rig) || !chan)
	return -RIG_EINVAL;

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

