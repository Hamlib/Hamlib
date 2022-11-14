/** \addtogroup rig
 * @{
 */

/**
 * \file src/mem.c
 * \brief Memory and channel interface
 * \author Stephane Fillod
 * \date 2000-2011
 *
 * Hamlib interface is a frontend implementing wrapper functions.
 *
 */

/*
 *  Hamlib Interface - mem/channel calls
 *  Copyright (c) 2000-2011 by Stephane Fillod
 *
 *   This library is free software; you can redistribute it and/or
 *   modify it under the terms of the GNU Lesser General Public
 *   License as published by the Free Software Foundation; either
 *   version 2.1 of the License, or (at your option) any later version.
 *
 *   This library is distributed in the hope that it will be useful,
 *   but WITHOUT ANY WARRANTY; without even the implied warranty of
 *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 *   Lesser General Public License for more details.
 *
 *   You should have received a copy of the GNU Lesser General Public
 *   License along with this library; if not, write to the Free Software
 *   Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA
 *
 */

#include <hamlib/config.h>

#include <stdlib.h>
#include <string.h>
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
 * \param rig   The rig handle
 * \param vfo   The target VFO
 * \param ch    The memory channel number
 *
 *  Sets the current memory channel number.
 *  It is not mandatory for the radio to be in memory mode. Actually
 *  it depends on rigs. YMMV.
 *
 * \return RIG_OK if the operation has been successful, otherwise
 * a negative value if an error occurred (in which case, cause is
 * set appropriately).
 *
 * \sa rig_get_mem()
 */
int HAMLIB_API rig_set_mem(RIG *rig, vfo_t vfo, int ch)
{
    const struct rig_caps *caps;
    int retcode;
    vfo_t curr_vfo;

    rig_debug(RIG_DEBUG_VERBOSE, "%s called\n", __func__);

    if (CHECK_RIG_ARG(rig))
    {
        return -RIG_EINVAL;
    }

    caps = rig->caps;

    if (caps->set_mem == NULL)
    {
        return -RIG_ENAVAIL;
    }

    if ((caps->targetable_vfo & RIG_TARGETABLE_MEM)
            || vfo == RIG_VFO_CURR
            || vfo == rig->state.current_vfo)
    {
        return caps->set_mem(rig, vfo, ch);
    }

    if (!caps->set_vfo)
    {
        return -RIG_ENTARGET;
    }

    curr_vfo = rig->state.current_vfo;
    retcode = caps->set_vfo(rig, vfo);

    if (retcode != RIG_OK)
    {
        return retcode;
    }

    retcode = caps->set_mem(rig, vfo, ch);
    caps->set_vfo(rig, curr_vfo);

    return retcode;
}


/**
 * \brief get the current memory channel number
 * \param rig   The rig handle
 * \param vfo   The target VFO
 * \param ch    The location where to store the current memory channel number
 *
 *  Retrieves the current memory channel number.
 *  It is not mandatory for the radio to be in memory mode. Actually
 *  it depends on rigs. YMMV.
 *
 * \return RIG_OK if the operation has been successful, otherwise
 * a negative value if an error occurred (in which case, cause is
 * set appropriately).
 *
 * \sa rig_set_mem()
 */
int HAMLIB_API rig_get_mem(RIG *rig, vfo_t vfo, int *ch)
{
    const struct rig_caps *caps;
    int retcode;
    vfo_t curr_vfo;

    rig_debug(RIG_DEBUG_VERBOSE, "%s called\n", __func__);

    if (CHECK_RIG_ARG(rig) || !ch)
    {
        return -RIG_EINVAL;
    }

    caps = rig->caps;

    if (caps->get_mem == NULL)
    {
        return -RIG_ENAVAIL;
    }

    if ((caps->targetable_vfo & RIG_TARGETABLE_MEM)
            || vfo == RIG_VFO_CURR
            || vfo == rig->state.current_vfo)
    {
        return caps->get_mem(rig, vfo, ch);
    }

    if (!caps->set_vfo)
    {
        return -RIG_ENTARGET;
    }

    curr_vfo = rig->state.current_vfo;
    retcode = caps->set_vfo(rig, vfo);

    if (retcode != RIG_OK)
    {
        return retcode;
    }

    retcode = caps->get_mem(rig, vfo, ch);
    caps->set_vfo(rig, curr_vfo);

    return retcode;
}


/**
 * \brief set the current memory bank
 * \param rig   The rig handle
 * \param vfo   The target VFO
 * \param bank  The memory bank
 *
 *  Sets the current memory bank number.
 *  It is not mandatory for the radio to be in memory mode. Actually
 *  it depends on rigs. YMMV.
 *
 * \return RIG_OK if the operation has been successful, otherwise
 * a negative value if an error occurred (in which case, cause is
 * set appropriately).
 *
 * \sa rig_set_mem()
 */
int HAMLIB_API rig_set_bank(RIG *rig, vfo_t vfo, int bank)
{
    const struct rig_caps *caps;
    int retcode;
    vfo_t curr_vfo;

    rig_debug(RIG_DEBUG_VERBOSE, "%s called\n", __func__);

    if (CHECK_RIG_ARG(rig))
    {
        return -RIG_EINVAL;
    }

    caps = rig->caps;

    if (caps->set_bank == NULL)
    {
        return -RIG_ENAVAIL;
    }

    if ((caps->targetable_vfo & RIG_TARGETABLE_BANK)
            || vfo == RIG_VFO_CURR
            || vfo == rig->state.current_vfo)
    {
        return caps->set_bank(rig, vfo, bank);
    }

    if (!caps->set_vfo)
    {
        return -RIG_ENTARGET;
    }

    curr_vfo = rig->state.current_vfo;
    retcode = caps->set_vfo(rig, vfo);

    if (retcode != RIG_OK)
    {
        return retcode;
    }

    retcode = caps->set_bank(rig, vfo, bank);
    caps->set_vfo(rig, curr_vfo);

    return retcode;
}

#ifndef DOC_HIDDEN


/*
 * call on every ext_levels of a rig
 */
static int generic_retr_extl(RIG *rig,
                             const struct confparams *cfp,
                             rig_ptr_t ptr)
{
    channel_t *chan = (channel_t *)ptr;
    struct ext_list *p;

    if (chan->ext_levels == NULL)
    {
        p = chan->ext_levels = calloc(1, 2 * sizeof(struct ext_list));
    }
    else
    {
        unsigned el_size = 0;

        for (p = chan->ext_levels; !RIG_IS_EXT_END(*p); p++)
        {
            el_size += sizeof(struct ext_list);
        }

        chan->ext_levels = realloc(chan->ext_levels,
                                   el_size + sizeof(struct ext_list));
    }

    if (!chan->ext_levels)
    {
        rig_debug(RIG_DEBUG_ERR,
                  "%s: %d memory allocation error!\n",
                  __func__,
                  __LINE__);
        return -RIG_ENOMEM;
    }

    p->token = cfp->token;
    rig_get_ext_level(rig, RIG_VFO_CURR, p->token, &p->val);
    p++;
    p->token = 0;   /* RIG_EXT_END */

    return 1;   /* process them all */
}


static const channel_cap_t mem_cap_all =
{
    .bank_num = 1,
    .vfo = 1,
    .ant = 1,
    .freq = 1,
    .mode = 1,
    .width = 1,
    .tx_freq = 1,
    .tx_mode = 1,
    .tx_width = 1,
    .split = 1,
    .tx_vfo = 1,
    .rptr_shift = 1,
    .rptr_offs = 1,
    .tuning_step = 1,
    .rit = 1,
    .xit = 1,
    .funcs = (setting_t) - 1,
    .levels = (setting_t) - 1,
    .ctcss_tone = 1,
    .ctcss_sql = 1,
    .dcs_code = 1,
    .dcs_sql = 1,
    .scan_group = 1,
    .flags = 1,
    .channel_desc = 1,
    .ext_levels = 1,
};


static int rig_mem_caps_empty(const channel_cap_t *mem_cap)
{
    return !(
               mem_cap->bank_num ||
               mem_cap->vfo ||
               mem_cap->ant ||
               mem_cap->freq ||
               mem_cap->mode ||
               mem_cap->width ||
               mem_cap->tx_freq ||
               mem_cap->tx_mode ||
               mem_cap->tx_width ||
               mem_cap->split ||
               mem_cap->tx_vfo ||
               mem_cap->rptr_shift ||
               mem_cap->rptr_offs ||
               mem_cap->tuning_step ||
               mem_cap->rit ||
               mem_cap->xit ||
               mem_cap->funcs ||
               mem_cap->levels ||
               mem_cap->ctcss_tone ||
               mem_cap->ctcss_sql ||
               mem_cap->dcs_code ||
               mem_cap->dcs_sql ||
               mem_cap->scan_group ||
               mem_cap->flags ||
               mem_cap->channel_desc ||
               mem_cap->ext_levels
           );
}


/*
 * stores current VFO state into chan by emulating rig_get_channel
 */
static int generic_save_channel(RIG *rig, channel_t *chan)
{
    int i;
    int chan_num;
    vfo_t vfo;
    setting_t setting;
    const channel_cap_t *mem_cap = NULL;
    value_t vdummy = {0};

    chan_num = chan->channel_num;
    vfo = chan->vfo;
    memset(chan, 0, sizeof(channel_t));
    chan->channel_num = chan_num;
    chan->vfo = vfo;

    if (vfo == RIG_VFO_MEM)
    {
        const chan_t *chan_cap;
        chan_cap = rig_lookup_mem_caps(rig, chan_num);

        if (chan_cap)
        {
            mem_cap = &chan_cap->mem_caps;
        }
    }

    /* If vfo!=RIG_VFO_MEM or incomplete backend, try all properties */
    if (mem_cap == NULL || rig_mem_caps_empty(mem_cap))
    {
        mem_cap = &mem_cap_all;
    }

    if (mem_cap->freq)
    {
        int retval = rig_get_freq(rig, RIG_VFO_CURR, &chan->freq);

        /* empty channel ? */
        if (retval == -RIG_ENAVAIL || chan->freq == RIG_FREQ_NONE)
        {
            return -RIG_ENAVAIL;
        }
    }

    if (mem_cap->vfo)
    {
        rig_get_vfo(rig, &chan->vfo);
    }

    if (mem_cap->mode || mem_cap->width)
    {
        rig_get_mode(rig, RIG_VFO_CURR, &chan->mode, &chan->width);
    }

    chan->split = RIG_SPLIT_OFF;

    if (mem_cap->split)
    {
        rig_get_split_vfo(rig, RIG_VFO_CURR, &chan->split, &chan->tx_vfo);
    }

    if (chan->split != RIG_SPLIT_OFF)
    {
        if (mem_cap->tx_freq)
        {
            rig_get_split_freq(rig, RIG_VFO_CURR, &chan->tx_freq);
        }

        if (mem_cap->tx_mode || mem_cap->tx_width)
        {
            rig_get_split_mode(rig, RIG_VFO_CURR, &chan->tx_mode, &chan->tx_width);
        }
    }
    else
    {
        chan->tx_freq = chan->freq;
        chan->tx_mode = chan->mode;
        chan->tx_width = chan->width;
    }

    if (mem_cap->rptr_shift)
    {
        rig_get_rptr_shift(rig, RIG_VFO_CURR, &chan->rptr_shift);
    }

    if (mem_cap->rptr_offs)
    {
        rig_get_rptr_offs(rig, RIG_VFO_CURR, &chan->rptr_offs);
    }

    if (mem_cap->ant)
    {
        ant_t ant_tx, ant_rx;
        rig_get_ant(rig, RIG_VFO_CURR, RIG_ANT_CURR, &vdummy, &chan->ant, &ant_tx,
                    &ant_rx);
    }

    if (mem_cap->tuning_step)
    {
        rig_get_ts(rig, RIG_VFO_CURR, &chan->tuning_step);
    }

    if (mem_cap->rit)
    {
        rig_get_rit(rig, RIG_VFO_CURR, &chan->rit);
    }

    if (mem_cap->xit)
    {
        rig_get_xit(rig, RIG_VFO_CURR, &chan->xit);
    }

    for (i = 0; i < RIG_SETTING_MAX; i++)
    {
        setting = rig_idx2setting(i);

        if ((setting & mem_cap->levels) && RIG_LEVEL_SET(setting))
        {
            rig_get_level(rig, RIG_VFO_CURR, setting, &chan->levels[i]);
        }
    }

    for (i = 0; i < RIG_SETTING_MAX; i++)
    {
        int fstatus;
        setting = rig_idx2setting(i);

        if ((setting & mem_cap->funcs)
                && (rig_get_func(rig, RIG_VFO_CURR, setting, &fstatus) == RIG_OK))
        {
            chan->funcs |= fstatus ? setting : 0;
        }
    }

    if (mem_cap->ctcss_tone)
    {
        rig_get_ctcss_tone(rig, RIG_VFO_CURR, &chan->ctcss_tone);
    }

    if (mem_cap->ctcss_sql)
    {
        rig_get_ctcss_sql(rig, RIG_VFO_CURR, &chan->ctcss_sql);
    }

    if (mem_cap->dcs_code)
    {
        rig_get_dcs_code(rig, RIG_VFO_CURR, &chan->dcs_code);
    }

    if (mem_cap->dcs_sql)
    {
        rig_get_dcs_sql(rig, RIG_VFO_CURR, &chan->dcs_sql);
    }

    /*
     * TODO: (missing calls)
     * - channel_desc
     * - bank_num
     * - scan_group
     * - flags
     */

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
    setting_t setting;
    const channel_cap_t *mem_cap = NULL;
    value_t vdummy = {0};

    if (chan->vfo == RIG_VFO_MEM)
    {
        const chan_t *chan_cap;
        chan_cap = rig_lookup_mem_caps(rig, chan->channel_num);

        if (chan_cap)
        {
            mem_cap = &chan_cap->mem_caps;
        }
    }

    /* If vfo!=RIG_VFO_MEM or incomplete backend, try all properties */
    if (mem_cap == NULL || rig_mem_caps_empty(mem_cap))
    {
        mem_cap = &mem_cap_all;
    }

    rig_set_vfo(rig, chan->vfo);

    if (mem_cap->freq)
    {
        rig_set_freq(rig, RIG_VFO_CURR, chan->freq);
    }

    if (mem_cap->mode || mem_cap->width)
    {
        rig_set_mode(rig, RIG_VFO_CURR, chan->mode, chan->width);
    }

    rig_set_split_vfo(rig, RIG_VFO_CURR, chan->split, chan->tx_vfo);

    if (chan->split != RIG_SPLIT_OFF)
    {
        if (mem_cap->tx_freq)
        {
            rig_set_split_freq(rig, RIG_VFO_CURR, chan->tx_freq);
        }

        if (mem_cap->tx_mode || mem_cap->tx_width)
        {
            rig_set_split_mode(rig, RIG_VFO_CURR, chan->tx_mode, chan->tx_width);
        }
    }

    if (mem_cap->rptr_shift)
    {
        rig_set_rptr_shift(rig, RIG_VFO_CURR, chan->rptr_shift);
    }

    if (mem_cap->rptr_offs)
    {
        rig_set_rptr_offs(rig, RIG_VFO_CURR, chan->rptr_offs);
    }

    for (i = 0; i < RIG_SETTING_MAX; i++)
    {
        setting = rig_idx2setting(i);

        if (setting & mem_cap->levels)
        {
            rig_set_level(rig, RIG_VFO_CURR, setting, chan->levels[i]);
        }
    }

    if (mem_cap->ant)
    {
        rig_set_ant(rig, RIG_VFO_CURR, chan->ant, vdummy);
    }

    if (mem_cap->tuning_step)
    {
        rig_set_ts(rig, RIG_VFO_CURR, chan->tuning_step);
    }

    if (mem_cap->rit)
    {
        rig_set_rit(rig, RIG_VFO_CURR, chan->rit);
    }

    if (mem_cap->xit)
    {
        rig_set_xit(rig, RIG_VFO_CURR, chan->xit);
    }

    for (i = 0; i < RIG_SETTING_MAX; i++)
    {
        setting = rig_idx2setting(i);

        if (setting & mem_cap->funcs)
            rig_set_func(rig, RIG_VFO_CURR, setting,
                         chan->funcs & rig_idx2setting(i));
    }

    if (mem_cap->ctcss_tone)
    {
        rig_set_ctcss_tone(rig, RIG_VFO_CURR, chan->ctcss_tone);
    }

    if (mem_cap->ctcss_sql)
    {
        rig_set_ctcss_sql(rig, RIG_VFO_CURR, chan->ctcss_sql);
    }

    if (mem_cap->dcs_code)
    {
        rig_set_dcs_code(rig, RIG_VFO_CURR, chan->dcs_code);
    }

    if (mem_cap->dcs_sql)
    {
        rig_set_dcs_sql(rig, RIG_VFO_CURR, chan->dcs_sql);
    }

    /*
     * TODO: (missing calls)
     * - channel_desc
     * - bank_num
     * - scan_group
     * - flags
     */

    for (p = chan->ext_levels; p && !RIG_IS_EXT_END(*p); p++)
    {
        rig_set_ext_level(rig, RIG_VFO_CURR, p->token, p->val);
    }

    return RIG_OK;
}
#endif  /* !DOC_HIDDEN */


/**
 * \brief set channel data
 * \param rig   The rig handle
 * \param chan  The location of data to set for this channel
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
 * \return RIG_OK if the operation has been successful, otherwise
 * a negative value if an error occurred (in which case, cause is
 * set appropriately).
 *
 * \sa rig_get_channel()
 */
int HAMLIB_API rig_set_channel(RIG *rig, vfo_t vfo, const channel_t *chan)
{
    struct rig_caps *rc;
    int curr_chan_num = -1, get_mem_status = RIG_OK;
    vfo_t curr_vfo;
    vfo_t vfotmp; /* requested vfo */
    int retcode;
    int can_emulate_by_vfo_mem, can_emulate_by_vfo_op;

    rig_debug(RIG_DEBUG_VERBOSE, "%s called\n", __func__);

    if (CHECK_RIG_ARG(rig) || !chan)
    {
        return -RIG_EINVAL;
    }

    /*
     * TODO: check validity of chan->channel_num
     */

    rc = rig->caps;

    if (rc->set_channel)
    {
        return rc->set_channel(rig, vfo, chan);
    }

    /*
     * if not available, emulate it
     * Optional: get_vfo, set_vfo,
     */

    vfotmp = chan->vfo;

    if (vfotmp == RIG_VFO_CURR)
    {
        return generic_restore_channel(rig, chan);
    }

    /* any emulation requires set_mem() */
    if (vfotmp == RIG_VFO_MEM && !rc->set_mem)
    {
        return -RIG_ENAVAIL;
    }

    can_emulate_by_vfo_mem = rc->set_vfo
                             && ((rig->state.vfo_list & RIG_VFO_MEM) == RIG_VFO_MEM);

    can_emulate_by_vfo_op = rc->vfo_op
                            && rig_has_vfo_op(rig, RIG_OP_FROM_VFO);

    if (!can_emulate_by_vfo_mem && !can_emulate_by_vfo_op)
    {
        return -RIG_ENTARGET;
    }

    curr_vfo = rig->state.current_vfo;

    if (vfotmp == RIG_VFO_MEM)
    {
        get_mem_status = rig_get_mem(rig, RIG_VFO_CURR, &curr_chan_num);
    }

    if (can_emulate_by_vfo_mem && curr_vfo != vfotmp)
    {
        retcode = rig_set_vfo(rig, vfotmp);

        if (retcode != RIG_OK)
        {
            return retcode;
        }
    }

    if (vfotmp == RIG_VFO_MEM)
    {
        rig_set_mem(rig, RIG_VFO_CURR, chan->channel_num);
    }

    retcode = generic_restore_channel(rig, chan);

    if (!can_emulate_by_vfo_mem && can_emulate_by_vfo_op)
    {
        retcode = rig_vfo_op(rig, RIG_VFO_CURR, RIG_OP_FROM_VFO);

        if (retcode != RIG_OK)
        {
            return retcode;
        }
    }

    /* restore current memory number */
    if (vfotmp == RIG_VFO_MEM && get_mem_status == RIG_OK)
    {
        rig_set_mem(rig, RIG_VFO_CURR, curr_chan_num);
    }

    if (can_emulate_by_vfo_mem)
    {
        rig_set_vfo(rig, curr_vfo);
    }

    return retcode;
}


/**
 * \brief get channel data
 * \param rig   The rig handle
 * \param chan  The location where to store the channel data
 * \param read_only  if true chan info will be filled but rig will not change, if false rig will update to chan info
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
  chan->read_only = 1;
  err = rig_get_channel(rig, &chan);
  if (err != RIG_OK)
    error("get_channel failed: %s", rigerror(err));

\endcode
 *
 *  The rig_get_channel is supposed to have no impact on the current VFO
 *  and memory number selected. Depending on backend and rig capabilities,
 *  the chan struct may not be filled in completely.
 *
 *  Note: chan->ext_levels is a pointer to a newly allocated memory.
 *  This is the responsibility of the caller to manage and eventually
 *  free it.
 *
 * \return RIG_OK if the operation has been successful, otherwise
 * a negative value if an error occurred (in which case, cause is
 * set appropriately).
 *
 * \sa rig_set_channel()
 */
int HAMLIB_API rig_get_channel(RIG *rig, vfo_t vfox, channel_t *chan,
                               int read_only)
{
    struct rig_caps *rc;
    int curr_chan_num = -1, get_mem_status = RIG_OK;
    vfo_t curr_vfo;
    vfo_t vfotmp = RIG_VFO_NONE; /* requested vfo */
    int retcode = RIG_OK;
    int can_emulate_by_vfo_mem, can_emulate_by_vfo_op;

    rig_debug(RIG_DEBUG_VERBOSE, "%s called\n", __func__);

    if (CHECK_RIG_ARG(rig) || !chan)
    {
        return -RIG_EINVAL;
    }

    /*
     * TODO: check validity of chan->channel_num
     */

    rc = rig->caps;

    if (rc->get_channel)
    {
        return rc->get_channel(rig, vfotmp, chan, read_only);
    }

    /*
     * if not available, emulate it
     * Optional: get_vfo, set_vfo
     * TODO: check return codes
     */
    vfotmp = chan->vfo;

    if (vfotmp == RIG_VFO_CURR)
    {
        return generic_save_channel(rig, chan);
    }

    /* any emulation requires set_mem() */
    if (vfotmp == RIG_VFO_MEM && !rc->set_mem)
    {
        return -RIG_ENAVAIL;
    }

    can_emulate_by_vfo_mem = rc->set_vfo
                             && ((rig->state.vfo_list & RIG_VFO_MEM) == RIG_VFO_MEM);

    can_emulate_by_vfo_op = rc->vfo_op
                            && rig_has_vfo_op(rig, RIG_OP_TO_VFO);

    if (!can_emulate_by_vfo_mem && !can_emulate_by_vfo_op)
    {
        return -RIG_ENTARGET;
    }

    curr_vfo = rig->state.current_vfo;

    if (vfotmp == RIG_VFO_MEM)
    {
        get_mem_status = rig_get_mem(rig, RIG_VFO_CURR, &curr_chan_num);
    }

    if (!read_only)
    {
        if (can_emulate_by_vfo_mem && curr_vfo != vfotmp)
        {
            retcode = rig_set_vfo(rig, vfotmp);

            if (retcode != RIG_OK)
            {
                return retcode;
            }
        }

        if (vfotmp == RIG_VFO_MEM)
        {
            rig_set_mem(rig, RIG_VFO_CURR, chan->channel_num);
        }

        if (!can_emulate_by_vfo_mem && can_emulate_by_vfo_op)
        {
            retcode = rig_vfo_op(rig, RIG_VFO_CURR, RIG_OP_TO_VFO);

            if (retcode != RIG_OK)
            {
                return retcode;
            }
        }

        retcode = generic_save_channel(rig, chan);

        /* restore current memory number */
        if (vfotmp == RIG_VFO_MEM && get_mem_status == RIG_OK)
        {
            rig_set_mem(rig, RIG_VFO_CURR, curr_chan_num);
        }

        if (can_emulate_by_vfo_mem)
        {
            rig_set_vfo(rig, curr_vfo);
        }

    }

    return retcode;
}


#ifndef DOC_HIDDEN
int get_chan_all_cb_generic(RIG *rig, vfo_t vfo, chan_cb_t chan_cb,
                            rig_ptr_t arg)
{
    int i, j;
    chan_t *chan_list = rig->state.chan_list;
    channel_t *chan;

    for (i = 0; !RIG_IS_CHAN_END(chan_list[i]) && i < HAMLIB_CHANLSTSIZ; i++)
    {
        int retval;

        /*
         * setting chan to NULL means the application
         * has to provide a struct where to store data
         * future data for channel channel_num
         */
        chan = NULL;
        retval = chan_cb(rig, vfo, &chan, chan_list[i].startc, chan_list, arg);

        if (retval != RIG_OK)
        {
            return retval;
        }

        if (chan == NULL)
        {
            return -RIG_ENOMEM;
        }

        for (j = chan_list[i].startc; j <= chan_list[i].endc; j++)
        {
            int chan_next;

            chan->vfo = RIG_VFO_MEM;
            chan->channel_num = j;

            /*
             * TODO: if doesn't have rc->get_channel, special generic
             */
            retval = rig_get_channel(rig, vfo, chan, 1);

            if (retval == -RIG_ENAVAIL)
            {
                /*
                 * empty channel
                 *
                 * Should it continue or call chan_cb with special arg?
                 */
                continue;
            }

            if (retval != RIG_OK)
            {
                return retval;
            }

            chan_next = j < chan_list[i].endc ? j + 1 : j;

            chan_cb(rig, vfo, &chan, chan_next, chan_list, arg);
        }
    }

    return RIG_OK;
}


int set_chan_all_cb_generic(RIG *rig, vfo_t vfo, chan_cb_t chan_cb,
                            rig_ptr_t arg)
{
    int i, j, retval;
    chan_t *chan_list = rig->state.chan_list;
    channel_t *chan;

    for (i = 0; !RIG_IS_CHAN_END(chan_list[i]) && i < HAMLIB_CHANLSTSIZ; i++)
    {

        for (j = chan_list[i].startc; j <= chan_list[i].endc; j++)
        {

            chan_cb(rig, vfo, &chan, j, chan_list, arg);
            chan->vfo = RIG_VFO_MEM;

            retval = rig_set_channel(rig, vfo, chan);

            if (retval != RIG_OK)
            {
                return retval;
            }
        }
    }

    return RIG_OK;
}


struct map_all_s
{
    channel_t *chans;
    const struct confparams *cfgps;
    value_t *vals;
};


/*
 * chan_cb_t to be used for non cb get/set_all
 */
static int map_chan(RIG *rig,
                    vfo_t vfo,
                    channel_t **chan,
                    int channel_num,
                    const chan_t *chan_list,
                    rig_ptr_t arg)
{
    struct map_all_s *map_arg = (struct map_all_s *)arg;

    /* TODO: check channel_num within start-end of chan_list */

    *chan = &map_arg->chans[channel_num];

    return RIG_OK;
}

#endif  /* DOC_HIDDEN */


/**
 * \brief set all channel data, by callback
 * \param rig       The rig handle
 * \param chan_cb   Pointer to a callback function to provide channel data
 * \param arg       Arbitrary argument passed back to \a chan_cb
 *
 *  Write the data associated with a all the memory channels.
 *  This is the preferred method to support clonable rigs.
 *
 * \return RIG_OK if the operation has been successful, otherwise
 * a negative value if an error occurred (in which case, cause is
 * set appropriately).
 *
 * \sa rig_set_chan_all(), rig_get_chan_all_cb()
 */
int HAMLIB_API rig_set_chan_all_cb(RIG *rig, vfo_t vfo, chan_cb_t chan_cb,
                                   rig_ptr_t arg)
{
    struct rig_caps *rc;
    int retval;

    rig_debug(RIG_DEBUG_VERBOSE, "%s called\n", __func__);

    if (CHECK_RIG_ARG(rig) || !chan_cb)
    {
        return -RIG_EINVAL;
    }

    rc = rig->caps;

    if (rc->set_chan_all_cb)
    {
        return rc->set_chan_all_cb(rig, vfo, chan_cb, arg);
    }

    /* if not available, emulate it */
    retval = set_chan_all_cb_generic(rig, vfo, chan_cb, arg);

    return retval;
}


/**
 * \brief get all channel data, by callback
 * \param rig       The rig handle
 * \param chan_cb   Pointer to a callback function to retrieve channel data
 * \param arg       Arbitrary argument passed back to \a chan_cb
 *
 *  Retrieves the data associated with a all the memory channels.
 *  This is the preferred method to support clonable rigs.
 *
 *  \a chan_cb is called first with no data in chan (chan equals NULL).
 *  This means the application has to provide a struct where to store
 *  future data for channel channel_num. If channel_num == chan->channel_num,
 *  the application does not need to provide a new allocated structure.
 *
 * \return RIG_OK if the operation has been successful, otherwise
 * a negative value if an error occurred (in which case, cause is
 * set appropriately).
 *
 * \sa rig_get_chan_all(), rig_set_chan_all_cb()
 */
int HAMLIB_API rig_get_chan_all_cb(RIG *rig, vfo_t vfo, chan_cb_t chan_cb,
                                   rig_ptr_t arg)
{
    struct rig_caps *rc;
    int retval;

    rig_debug(RIG_DEBUG_VERBOSE, "%s called\n", __func__);

    if (CHECK_RIG_ARG(rig) || !chan_cb)
    {
        return -RIG_EINVAL;
    }

    rc = rig->caps;

    if (rc->get_chan_all_cb)
    {
        return rc->get_chan_all_cb(rig, vfo, chan_cb, arg);
    }


    /* if not available, emulate it */
    retval = get_chan_all_cb_generic(rig, vfo, chan_cb, arg);

    return retval;
}


/**
 * \brief set all channel data
 * \param rig   The rig handle
 * \param chans The location of data to set for all channels
 *
 * Write the data associated with all the memory channels.
 *
 * \return RIG_OK if the operation has been successful, otherwise
 * a negative value if an error occurred (in which case, cause is
 * set appropriately).
 *
 * \sa rig_set_chan_all_cb(), rig_get_chan_all()
 */
int HAMLIB_API rig_set_chan_all(RIG *rig, vfo_t vfo, const channel_t chans[])
{
    struct rig_caps *rc;
    struct map_all_s map_arg;
    int retval;

    rig_debug(RIG_DEBUG_VERBOSE, "%s called\n", __func__);

    if (CHECK_RIG_ARG(rig) || !chans)
    {
        return -RIG_EINVAL;
    }

    rc = rig->caps;
    memset(&map_arg, 0, sizeof(map_arg));
    map_arg.chans = (channel_t *) chans;

    if (rc->set_chan_all_cb)
    {
        return rc->set_chan_all_cb(rig, vfo, map_chan, (rig_ptr_t)&map_arg);
    }


    /* if not available, emulate it */
    retval = set_chan_all_cb_generic(rig, vfo, map_chan, (rig_ptr_t)&map_arg);

    return retval;
}


/**
 * \brief get all channel data
 * \param rig   The rig handle
 * \param chans The location where to store all the channel data
 *
 * Retrieves the data associated with all the memory channels.
 *
 * \return RIG_OK if the operation has been successful, otherwise
 * a negative value if an error occurred (in which case, cause is
 * set appropriately).
 *
 * \sa rig_get_chan_all_cb(), rig_set_chan_all()
 */
int HAMLIB_API rig_get_chan_all(RIG *rig, vfo_t vfo, channel_t chans[])
{
    struct rig_caps *rc;
    struct map_all_s map_arg;
    int retval;

    rig_debug(RIG_DEBUG_VERBOSE, "%s called\n", __func__);

    if (CHECK_RIG_ARG(rig) || !chans)
    {
        return -RIG_EINVAL;
    }

    rc = rig->caps;
    memset(&map_arg, 0, sizeof(map_arg));
    map_arg.chans = chans;

    if (rc->get_chan_all_cb)
    {
        return rc->get_chan_all_cb(rig, vfo, map_chan, (rig_ptr_t)&map_arg);
    }

    /*
     * if not available, emulate it
     *
     * TODO: save_current_state, restore_current_state
     */
    retval = get_chan_all_cb_generic(rig, vfo, map_chan, (rig_ptr_t)&map_arg);

    return retval;
}


/**
 * \brief copy channel structure to another channel structure
 * \param rig   The rig handle
 * \param dest The destination location
 * \param src The source location
 *
 * Copies the data associated with one channel structure to another
 *
 * \return RIG_OK if the operation has been successful, otherwise
 * a negative value if an error occurred (in which case, cause is
 * set appropriately).
 *
 * \sa rig_get_chan_all_cb(), rig_set_chan_all()
 */
int HAMLIB_API rig_copy_channel(RIG *rig,
                                channel_t *dest,
                                const channel_t *src)
{
    struct ext_list *saved_ext_levels;
    int i;

    rig_debug(RIG_DEBUG_VERBOSE, "%s called\n", __func__);

    /* TODO: ext_levels[] of different sizes */
    for (i = 0; !RIG_IS_EXT_END(src->ext_levels[i])
            && !RIG_IS_EXT_END(dest->ext_levels[i]); i++)
    {
        dest->ext_levels[i] = src->ext_levels[i];
    }

    saved_ext_levels = dest->ext_levels;
    memcpy(dest, src, sizeof(channel_t));
    dest->ext_levels = saved_ext_levels;

    return RIG_OK;
}

#ifndef DOC_HIDDEN


static int map_parm(RIG *rig, const struct confparams *cfgps, value_t *value,
                    rig_ptr_t arg)
{
    return -RIG_ENIMPL;
}


int get_parm_all_cb_generic(RIG *rig, vfo_t vfo, confval_cb_t parm_cb,
                            rig_ptr_t cfgps,
                            rig_ptr_t vals)
{
    return -RIG_ENIMPL;
}


int set_parm_all_cb_generic(RIG *rig, vfo_t vfo, confval_cb_t parm_cb,
                            rig_ptr_t cfgps,
                            rig_ptr_t vals)
{
    return -RIG_ENIMPL;
}

#endif  /* DOC_HIDDEN */


/**
 * \brief set all channel and non-channel data by call-back
 * \param rig       The rig handle
 * \param chan_cb   The callback for channel data
 * \param parm_cb   The callback for non-channel(aka parm) data
 * \param arg       Cookie passed to \a chan_cb and \a parm_cb
 *
 * Writes the data associated with all the memory channels,
 * and rigs memory parameters, by callback.
 * This is the preferred method to support clonable rigs.
 *
 * \return RIG_OK if the operation has been successful, otherwise
 * a negative value if an error occurred (in which case, cause is
 * set appropriately).
 *
 * \sa rig_get_mem_all_cb(), rig_set_mem_all()
 * \todo finish coding and testing of mem_all functions
 */
int HAMLIB_API rig_set_mem_all_cb(RIG *rig,
                                  vfo_t vfo,
                                  chan_cb_t chan_cb,
                                  confval_cb_t parm_cb,
                                  rig_ptr_t arg)
{
    struct rig_caps *rc;
    int retval;

    rig_debug(RIG_DEBUG_VERBOSE, "%s called\n", __func__);

    if (CHECK_RIG_ARG(rig) || !chan_cb)
    {
        return -RIG_EINVAL;
    }

    rc = rig->caps;

    if (rc->set_mem_all_cb)
    {
        return rc->set_mem_all_cb(rig, vfo, chan_cb, parm_cb, arg);
    }


    /* if not available, emulate it */
    retval = rig_set_chan_all_cb(rig, vfo, chan_cb, arg);

    if (retval != RIG_OK)
    {
        return retval;
    }

#if 0
    retval = rig_set_parm_all_cb(rig, parm_cb, arg);

    if (retval != RIG_OK)
    {
        return retval;
    }

#else
    return -RIG_ENIMPL;
#endif

    return retval;
}


/**
 * \brief get all channel and non-channel data by call-back
 * \param rig       The rig handle
 * \param chan_cb   The callback for channel data
 * \param parm_cb   The callback for non-channel(aka parm) data
 * \param arg       Cookie passed to \a chan_cb and \a parm_cb
 *
 * Retrieves the data associated with all the memory channels,
 * and rigs memory parameters, by callback.
 * This is the preferred method to support clonable rigs.
 *
 * \return RIG_OK if the operation has been successful, otherwise
 * a negative value if an error occurred (in which case, cause is
 * set appropriately).
 *
 * \sa rig_get_mem_all_cb(), rig_set_mem_all()
 *
 * \todo get all parm's
 * \todo finish coding and testing of mem_all functions
 */
int HAMLIB_API rig_get_mem_all_cb(RIG *rig,
                                  vfo_t vfo,
                                  chan_cb_t chan_cb,
                                  confval_cb_t parm_cb,
                                  rig_ptr_t arg)
{
    struct rig_caps *rc;
    int retval;

    rig_debug(RIG_DEBUG_VERBOSE, "%s called\n", __func__);

    if (CHECK_RIG_ARG(rig) || !chan_cb)
    {
        return -RIG_EINVAL;
    }

    rc = rig->caps;

    if (rc->get_mem_all_cb)
    {
        return rc->get_mem_all_cb(rig, vfo, chan_cb, parm_cb, arg);
    }

    /* if not available, emulate it */
    retval = rig_get_chan_all_cb(rig, vfo, chan_cb, arg);

    if (retval != RIG_OK)
    {
        return retval;
    }

#if 0
    retval = rig_get_parm_cb(rig, parm_cb, arg);

    if (retval != RIG_OK)
    {
        return retval;
    }

#else
    return -RIG_ENIMPL;
#endif

    return retval;
}


/**
 * \brief set all channel and non-channel data
 * \param rig   The rig handle
 * \param chans Channel data
 * \param cfgps ??
 * \param vals  ??
 *
 * Writes the data associated with all the memory channels,
 * and rigs memory parameters.
 *
 * \return RIG_OK if the operation has been successful, otherwise
 * a negative value if an error occurred (in which case, cause is
 * set appropriately).
 *
 * \sa rig_get_mem_all(), rig_set_mem_all_cb()
 *
 * \todo set all parm's
 * \todo finish coding and testing of mem_all functions
 */
int HAMLIB_API rig_set_mem_all(RIG *rig,
                               vfo_t vfo,
                               const channel_t chans[],
                               const struct confparams cfgps[],
                               const value_t vals[])
{
    struct rig_caps *rc;
    int retval;
    struct map_all_s mem_all_arg;

    rig_debug(RIG_DEBUG_VERBOSE, "%s called\n", __func__);

    if (CHECK_RIG_ARG(rig) || !chans || !cfgps || !vals)
    {
        return -RIG_EINVAL;
    }

    rc = rig->caps;
    mem_all_arg.chans = (channel_t *) chans;
    mem_all_arg.cfgps = cfgps;
    mem_all_arg.vals = (value_t *) vals;

    if (rc->set_mem_all_cb)
        return rc->set_mem_all_cb(rig, vfo, map_chan, map_parm,
                                  (rig_ptr_t)&mem_all_arg);

    /* if not available, emulate it */
    retval = rig_set_chan_all(rig, vfo, chans);

    if (retval != RIG_OK)
    {
        return retval;
    }

#if 0
    retval = rig_set_parm_all(rig, parms);

    if (retval != RIG_OK)
    {
        return retval;
    }

#else
    return -RIG_ENIMPL;
#endif

    return retval;
}


/**
 * \brief get all channel and non-channel data
 * \param rig   The rig handle
 * \param chans Array of channels where to store the data
 * \param cfgps Array of config parameters to retrieve
 * \param vals  Array of values where to store the data
 *
 * Retrieves the data associated with all the memory channels,
 * and rigs memory parameters.
 * This is the preferred method to support clonable rigs.
 *
 * \return RIG_OK if the operation has been successful, otherwise
 * a negative value if an error occurred (in which case, cause is
 * set appropriately).
 *
 * \sa rig_get_mem_all(), rig_set_mem_all_cb()
 * \todo finish coding and testing of mem_all functions
 */
int HAMLIB_API rig_get_mem_all(RIG *rig,
                               vfo_t vfo,
                               channel_t chans[],
                               const struct confparams cfgps[],
                               value_t vals[])
{
    struct rig_caps *rc;
    int retval;
    struct map_all_s mem_all_arg;

    rig_debug(RIG_DEBUG_VERBOSE, "%s called\n", __func__);

    if (CHECK_RIG_ARG(rig) || !chans || !cfgps || !vals)
    {
        return -RIG_EINVAL;
    }

    rc = rig->caps;
    mem_all_arg.chans = chans;
    mem_all_arg.cfgps = cfgps;
    mem_all_arg.vals = vals;

    if (rc->get_mem_all_cb)
        return rc->get_mem_all_cb(rig, vfo, map_chan, map_parm,
                                  (rig_ptr_t)&mem_all_arg);

    /*
     * if not available, emulate it
     *
     * TODO: save_current_state, restore_current_state
     */
    retval = rig_get_chan_all(rig, vfo, chans);

    if (retval != RIG_OK)
    {
        return retval;
    }

    retval = get_parm_all_cb_generic(rig, vfo, map_parm,
                                     (rig_ptr_t)cfgps,
                                     (rig_ptr_t)vals);

    return retval;
}


/**
 * \brief lookup the memory type and capabilities
 * \param rig   The rig handle
 * \param ch    The memory channel number
 *
 *  Lookup the memory type and capabilities associated with a channel number.
 *  If \a ch equals RIG_MEM_CAPS_ALL, then a union of all the mem_caps sets
 *  is returned (pointer to static memory).
 *
 * \return a pointer to a chan_t structure if the operation has been successful,
 * otherwise a NULL pointer, most probably because of incorrect channel number
 * or buggy backend.
 */
const chan_t *HAMLIB_API rig_lookup_mem_caps(RIG *rig, int ch)
{
    chan_t *chan_list;
    static chan_t chan_list_all;
    int i;

    rig_debug(RIG_DEBUG_VERBOSE, "%s called\n", __func__);

    if (CHECK_RIG_ARG(rig))
    {
        return NULL;
    }

    if (ch == RIG_MEM_CAPS_ALL)
    {
        memset(&chan_list_all, 0, sizeof(chan_list_all));
        chan_list = rig->state.chan_list;
        chan_list_all.startc = chan_list[0].startc;
        chan_list_all.type = RIG_MTYPE_NONE;    /* meaningless */

        for (i = 0; i < HAMLIB_CHANLSTSIZ && !RIG_IS_CHAN_END(chan_list[i]); i++)
        {
            int j;
            unsigned char *p1, *p2;
            p1 = (unsigned char *)&chan_list_all.mem_caps;
            p2 = (unsigned char *)&chan_list[i].mem_caps;

            /* It's kind of hackish, we just want to do update set with:
             *  chan_list_all.mem_caps |= chan_list[i].mem_caps
             */
            for (j = 0; j < sizeof(channel_cap_t); j++)
            {
                // cppcheck-suppress *
                p1[j] |= p2[j];
            }

            /* til the end, most probably meaningless */
            chan_list_all.endc = chan_list[i].endc;
        }

        return &chan_list_all;
    }

    chan_list = rig->state.chan_list;

    for (i = 0; i < HAMLIB_CHANLSTSIZ && !RIG_IS_CHAN_END(chan_list[i]); i++)
    {
        if (ch >= chan_list[i].startc && ch <= chan_list[i].endc)
        {
            return &chan_list[i];
        }
    }

    return NULL;
}


// Not referenced anywhere
/**
 * \brief get memory channel count
 * \param rig   The rig handle
 *
 *  Get the total memory channel count, computed from the rig caps
 *
 * \return the memory count
 */
int HAMLIB_API rig_mem_count(RIG *rig)
{
    const chan_t *chan_list;
    int i, count;

    rig_debug(RIG_DEBUG_VERBOSE, "%s called\n", __func__);

    if (CHECK_RIG_ARG(rig))
    {
        return -RIG_EINVAL;
    }

    chan_list = rig->state.chan_list;
    count = 0;

    for (i = 0; i < HAMLIB_CHANLSTSIZ && !RIG_IS_CHAN_END(chan_list[i]); i++)
    {
        count += chan_list[i].endc - chan_list[i].startc + 1;
    }

    return count;
}

/*! @} */
