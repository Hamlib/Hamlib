/*
 *  Hamlib Dummy backend - main file
 *  Copyright (c) 2001-2003 by Stephane Fillod
 *
 *	$Id: dummy.c,v 1.32 2003-04-06 18:40:35 fillods Exp $
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
#include <stdio.h>   /* Standard input/output definitions */
#include <string.h>  /* String function definitions */
#include <unistd.h>  /* UNIX standard function definitions */
#include <fcntl.h>   /* File control definitions */
#include <errno.h>   /* Error number definitions */
#include <termios.h> /* POSIX terminal control definitions */
#include <sys/ioctl.h>
#include <math.h>

#include <hamlib/rig.h>
#include <serial.h>
#include <misc.h>
#include <tones.h>
#include <idx_builtin.h>

#include "dummy.h"

#define NB_CHAN 22		/* see caps->chan_list */

struct dummy_priv_data {
		/* current vfo already in rig_state ? */
		vfo_t curr_vfo;
		vfo_t last_vfo;	/* VFO A or VFO B, when in MEM mode */

		ptt_t ptt;
		powerstat_t powerstat;
		int bank;
		value_t parms[RIG_SETTING_MAX];

		channel_t *curr;	/* points to vfo_a, vfo_b or mem[] */

		channel_t vfo_a;
		channel_t vfo_b;
		channel_t mem[NB_CHAN];
};


/********************************************************************/

static void init_chan(RIG *rig, vfo_t vfo, channel_t *chan)
{
  chan->channel_num = 0;
  chan->vfo = vfo;
  strcpy(chan->channel_desc, strvfo(vfo));

  chan->freq = MHz(145);
  chan->mode = RIG_MODE_FM;
  chan->width = rig_passband_normal(rig, RIG_MODE_FM);
  chan->tx_freq = chan->freq;
  chan->tx_mode = chan->mode;
  chan->tx_width = chan->width;
  chan->split = RIG_SPLIT_OFF;

  chan->rptr_shift = RIG_RPT_SHIFT_NONE;
  chan->rptr_offs = 0;
  chan->ctcss_tone = 0;
  chan->dcs_code = 0;
  chan->ctcss_sql = 0;
  chan->dcs_sql = 0;
  chan->rit = 0;
  chan->xit = 0;
  chan->tuning_step = 0;
  chan->ant = 0;

  chan->funcs = (setting_t)0;
  memset(chan->levels, 0, RIG_SETTING_MAX*sizeof(value_t));
}

static int dummy_init(RIG *rig)
{
  struct dummy_priv_data *priv;
  int i;

  priv = (struct dummy_priv_data*)malloc(sizeof(struct dummy_priv_data));
  if (!priv)
		  return -RIG_ENOMEM;
  rig->state.priv = (void*)priv;

  rig_debug(RIG_DEBUG_VERBOSE,"%s called\n", __FUNCTION__);
  rig->state.rigport.type.rig = RIG_PORT_NONE;

  priv->ptt = RIG_PTT_OFF;
  priv->powerstat = RIG_POWER_ON;
  priv->bank = 0;
  memset(priv->parms, 0, RIG_SETTING_MAX*sizeof(value_t));

  memset(priv->mem, 0, sizeof(priv->mem));
  for (i=0; i<NB_CHAN; i++) {
	priv->mem[i].channel_num = i;
	priv->mem[i].vfo = RIG_VFO_MEM;
  }
  init_chan(rig, RIG_VFO_A, &priv->vfo_a);
  init_chan(rig, RIG_VFO_B, &priv->vfo_b);
  priv->curr = &priv->vfo_a;
  priv->curr_vfo = priv->last_vfo = RIG_VFO_A;

  return RIG_OK;
}

static int dummy_cleanup(RIG *rig)
{
  rig_debug(RIG_DEBUG_VERBOSE,"%s called\n", __FUNCTION__);

  if (rig->state.priv)
  	free(rig->state.priv);

  rig->state.priv = NULL;

  return RIG_OK;
}

static int dummy_open(RIG *rig)
{
  rig_debug(RIG_DEBUG_VERBOSE,"%s called\n", __FUNCTION__);

  return RIG_OK;
}

static int dummy_close(RIG *rig)
{
  rig_debug(RIG_DEBUG_VERBOSE,"%s called\n", __FUNCTION__);

  return RIG_OK;
}

static int dummy_set_freq(RIG *rig, vfo_t vfo, freq_t freq)
{
  struct dummy_priv_data *priv = (struct dummy_priv_data *)rig->state.priv;
  channel_t *curr = priv->curr;
  char fstr[20];

  sprintf_freq(fstr, freq);
  rig_debug(RIG_DEBUG_VERBOSE,"%s called: %s %s\n", __FUNCTION__, 
 			strvfo(vfo), fstr);
  curr->freq = freq;

  return RIG_OK;
}


static int dummy_get_freq(RIG *rig, vfo_t vfo, freq_t *freq)
{
  struct dummy_priv_data *priv = (struct dummy_priv_data *)rig->state.priv;
  channel_t *curr = priv->curr;

  rig_debug(RIG_DEBUG_VERBOSE,"%s called: %s\n", __FUNCTION__, strvfo(vfo));

  *freq = curr->freq;

  return RIG_OK;
}


static int dummy_set_mode(RIG *rig, vfo_t vfo, rmode_t mode, pbwidth_t width)
{
  struct dummy_priv_data *priv = (struct dummy_priv_data *)rig->state.priv;
  channel_t *curr = priv->curr;
  char buf[16];

  sprintf_freq(buf, width);
  rig_debug(RIG_DEBUG_VERBOSE,"%s called: %s %s %s\n", __FUNCTION__, 
  		strvfo(vfo), strrmode(mode), buf);

  curr->mode = mode;
  curr->width = width;

  return RIG_OK;
}


static int dummy_get_mode(RIG *rig, vfo_t vfo, rmode_t *mode, pbwidth_t *width)
{
  struct dummy_priv_data *priv = (struct dummy_priv_data *)rig->state.priv;
  channel_t *curr = priv->curr;

  rig_debug(RIG_DEBUG_VERBOSE,"%s called: %s\n", __FUNCTION__, strvfo(vfo));

  *mode = curr->mode;
  *width = curr->width;

  return RIG_OK;
}


static int dummy_set_vfo(RIG *rig, vfo_t vfo)
{
  struct dummy_priv_data *priv = (struct dummy_priv_data *)rig->state.priv;
  channel_t *curr = priv->curr;

  rig_debug(RIG_DEBUG_VERBOSE,"%s called: %s\n", __FUNCTION__, strvfo(vfo));

  priv->last_vfo = priv->curr_vfo;
  priv->curr_vfo = vfo;
  switch (vfo) {
  case RIG_VFO_VFO:	/* FIXME */

  case RIG_VFO_A: priv->curr = &priv->vfo_a; break;
  case RIG_VFO_B: priv->curr = &priv->vfo_b; break;
  case RIG_VFO_MEM: 
				  if (curr->channel_num >= 0 && curr->channel_num < NB_CHAN) {
						  priv->curr = &priv->mem[curr->channel_num];
						  break;
				  }
  default:
  	rig_debug(RIG_DEBUG_VERBOSE,"%s unknown vfo: %s\n", __FUNCTION__, strvfo(vfo));
  }

  return RIG_OK;
}


static int dummy_get_vfo(RIG *rig, vfo_t *vfo)
{
  struct dummy_priv_data *priv = (struct dummy_priv_data *)rig->state.priv;

  *vfo = priv->curr_vfo;
  rig_debug(RIG_DEBUG_VERBOSE,"%s called: %s\n", __FUNCTION__, strvfo(*vfo));

  return RIG_OK;
}


static int dummy_set_ptt(RIG *rig, vfo_t vfo, ptt_t ptt)
{
  struct dummy_priv_data *priv = (struct dummy_priv_data *)rig->state.priv;

  rig_debug(RIG_DEBUG_VERBOSE, "%s called\n", __FUNCTION__);
  priv->ptt = ptt;

  return RIG_OK;
}


static int dummy_get_ptt(RIG *rig, vfo_t vfo, ptt_t *ptt)
{
  struct dummy_priv_data *priv = (struct dummy_priv_data *)rig->state.priv;

  rig_debug(RIG_DEBUG_VERBOSE, "%s called\n", __FUNCTION__);
  *ptt = priv->ptt;

  return RIG_OK;
}


static int dummy_get_dcd(RIG *rig, vfo_t vfo, dcd_t *dcd)
{
  static int twiddle = 0;

  rig_debug(RIG_DEBUG_VERBOSE, "%s called\n", __FUNCTION__);
  *dcd = twiddle++ & 1 ? RIG_DCD_ON : RIG_DCD_OFF;

  return RIG_OK;
}


static int dummy_set_rptr_shift(RIG *rig, vfo_t vfo, rptr_shift_t rptr_shift)
{
  struct dummy_priv_data *priv = (struct dummy_priv_data *)rig->state.priv;
  channel_t *curr = priv->curr;

  rig_debug(RIG_DEBUG_VERBOSE, "%s called\n", __FUNCTION__);
  curr->rptr_shift = rptr_shift;

  return RIG_OK;
}


static int dummy_get_rptr_shift(RIG *rig, vfo_t vfo, rptr_shift_t *rptr_shift)
{
  struct dummy_priv_data *priv = (struct dummy_priv_data *)rig->state.priv;
  channel_t *curr = priv->curr;

  *rptr_shift = curr->rptr_shift;
  rig_debug(RIG_DEBUG_VERBOSE, "%s called\n", __FUNCTION__);

  return RIG_OK;
}


static int dummy_set_rptr_offs(RIG *rig, vfo_t vfo, shortfreq_t rptr_offs)
{
  struct dummy_priv_data *priv = (struct dummy_priv_data *)rig->state.priv;
  channel_t *curr = priv->curr;

  rig_debug(RIG_DEBUG_VERBOSE, "%s called\n", __FUNCTION__);
  curr->rptr_offs = rptr_offs;

  return RIG_OK;
}


static int dummy_get_rptr_offs(RIG *rig, vfo_t vfo, shortfreq_t *rptr_offs)
{
  struct dummy_priv_data *priv = (struct dummy_priv_data *)rig->state.priv;
  channel_t *curr = priv->curr;

  *rptr_offs = curr->rptr_offs;
  rig_debug(RIG_DEBUG_VERBOSE, "%s called\n", __FUNCTION__);

  return RIG_OK;
}


static int dummy_set_ctcss_tone(RIG *rig, vfo_t vfo, tone_t tone)
{
  struct dummy_priv_data *priv = (struct dummy_priv_data *)rig->state.priv;
  channel_t *curr = priv->curr;

  rig_debug(RIG_DEBUG_VERBOSE, "%s called\n", __FUNCTION__);
  curr->ctcss_tone = tone;

  return RIG_OK;
}


static int dummy_get_ctcss_tone(RIG *rig, vfo_t vfo, tone_t *tone)
{
  struct dummy_priv_data *priv = (struct dummy_priv_data *)rig->state.priv;
  channel_t *curr = priv->curr;

  *tone = curr->ctcss_tone;
  rig_debug(RIG_DEBUG_VERBOSE, "%s called\n", __FUNCTION__);

  return RIG_OK;
}


static int dummy_set_dcs_code(RIG *rig, vfo_t vfo, tone_t code)
{
  struct dummy_priv_data *priv = (struct dummy_priv_data *)rig->state.priv;
  channel_t *curr = priv->curr;

  rig_debug(RIG_DEBUG_VERBOSE, "%s called\n", __FUNCTION__);
  curr->dcs_code = code;

  return RIG_OK;
}


static int dummy_get_dcs_code(RIG *rig, vfo_t vfo, tone_t *code)
{
  struct dummy_priv_data *priv = (struct dummy_priv_data *)rig->state.priv;
  channel_t *curr = priv->curr;

  *code = curr->dcs_code;
  rig_debug(RIG_DEBUG_VERBOSE, "%s called\n", __FUNCTION__);

  return RIG_OK;
}


static int dummy_set_ctcss_sql(RIG *rig, vfo_t vfo, tone_t tone)
{
  struct dummy_priv_data *priv = (struct dummy_priv_data *)rig->state.priv;
  channel_t *curr = priv->curr;

  rig_debug(RIG_DEBUG_VERBOSE, "%s called\n", __FUNCTION__);
  curr->ctcss_sql = tone;

  return RIG_OK;
}


static int dummy_get_ctcss_sql(RIG *rig, vfo_t vfo, tone_t *tone)
{
  struct dummy_priv_data *priv = (struct dummy_priv_data *)rig->state.priv;
  channel_t *curr = priv->curr;

  *tone = curr->ctcss_sql;
  rig_debug(RIG_DEBUG_VERBOSE, "%s called\n", __FUNCTION__);

  return RIG_OK;
}


static int dummy_set_dcs_sql(RIG *rig, vfo_t vfo, unsigned int code)
{
  struct dummy_priv_data *priv = (struct dummy_priv_data *)rig->state.priv;
  channel_t *curr = priv->curr;

  rig_debug(RIG_DEBUG_VERBOSE, "%s called\n", __FUNCTION__);
  curr->dcs_sql = code;

  return RIG_OK;
}


static int dummy_get_dcs_sql(RIG *rig, vfo_t vfo, unsigned int *code)
{
  struct dummy_priv_data *priv = (struct dummy_priv_data *)rig->state.priv;
  channel_t *curr = priv->curr;

  *code = curr->dcs_sql;
  rig_debug(RIG_DEBUG_VERBOSE, "%s called\n", __FUNCTION__);

  return RIG_OK;
}


static int dummy_set_split_freq(RIG *rig, vfo_t vfo, freq_t tx_freq)
{
  struct dummy_priv_data *priv = (struct dummy_priv_data *)rig->state.priv;
  channel_t *curr = priv->curr;
  char fstr[20];

  sprintf_freq(fstr, tx_freq);
  rig_debug(RIG_DEBUG_VERBOSE,"%s called: %s %s\n", __FUNCTION__, 
 			strvfo(vfo), fstr);
  curr->tx_freq = tx_freq;

  return RIG_OK;
}


static int dummy_get_split_freq(RIG *rig, vfo_t vfo, freq_t *tx_freq)
{
  struct dummy_priv_data *priv = (struct dummy_priv_data *)rig->state.priv;
  channel_t *curr = priv->curr;

  rig_debug(RIG_DEBUG_VERBOSE,"%s called: %s\n", __FUNCTION__,strvfo(vfo));

  *tx_freq = curr->tx_freq;

  return RIG_OK;
}

static int dummy_set_split_mode(RIG *rig, vfo_t vfo, rmode_t tx_mode, pbwidth_t tx_width)
{
  struct dummy_priv_data *priv = (struct dummy_priv_data *)rig->state.priv;
  channel_t *curr = priv->curr;
  char buf[16];

  sprintf_freq(buf, tx_width);
  rig_debug(RIG_DEBUG_VERBOSE,"%s called: %s %s %s\n", __FUNCTION__,
  		strvfo(vfo), strrmode(tx_mode), buf);

  curr->tx_mode = tx_mode;
  curr->tx_width = tx_width;

  return RIG_OK;
}

static int dummy_get_split_mode(RIG *rig, vfo_t vfo, rmode_t *tx_mode, pbwidth_t *tx_width)
{
  struct dummy_priv_data *priv = (struct dummy_priv_data *)rig->state.priv;
  channel_t *curr = priv->curr;

  rig_debug(RIG_DEBUG_VERBOSE,"%s called: %s\n", __FUNCTION__, strvfo(vfo));

  *tx_mode = curr->tx_mode;
  *tx_width = curr->tx_width;

  return RIG_OK;
}

static int dummy_set_split_vfo(RIG *rig, vfo_t vfo, split_t split, vfo_t tx_vfo)
{
  struct dummy_priv_data *priv = (struct dummy_priv_data *)rig->state.priv;
  channel_t *curr = priv->curr;

  rig_debug(RIG_DEBUG_VERBOSE, "%s called\n", __FUNCTION__);
  curr->split = split;

  return RIG_OK;
}


static int dummy_get_split_vfo(RIG *rig, vfo_t vfo, split_t *split, vfo_t *tx_vfo)
{
  struct dummy_priv_data *priv = (struct dummy_priv_data *)rig->state.priv;
  channel_t *curr = priv->curr;

  *split = curr->split;
  rig_debug(RIG_DEBUG_VERBOSE, "%s called\n", __FUNCTION__);

  return RIG_OK;
}

static int dummy_set_rit(RIG *rig, vfo_t vfo, shortfreq_t rit)
{
  struct dummy_priv_data *priv = (struct dummy_priv_data *)rig->state.priv;
  channel_t *curr = priv->curr;

  rig_debug(RIG_DEBUG_VERBOSE, "%s called\n", __FUNCTION__);
  curr->rit = rit;

  return RIG_OK;
}


static int dummy_get_rit(RIG *rig, vfo_t vfo, shortfreq_t *rit)
{
  struct dummy_priv_data *priv = (struct dummy_priv_data *)rig->state.priv;
  channel_t *curr = priv->curr;

  *rit = curr->rit;
  rig_debug(RIG_DEBUG_VERBOSE, "%s called\n", __FUNCTION__);

  return RIG_OK;
}


static int dummy_set_xit(RIG *rig, vfo_t vfo, shortfreq_t xit)
{
  struct dummy_priv_data *priv = (struct dummy_priv_data *)rig->state.priv;
  channel_t *curr = priv->curr;

  rig_debug(RIG_DEBUG_VERBOSE, "%s called\n", __FUNCTION__);
  curr->xit = xit;

  return RIG_OK;
}


static int dummy_get_xit(RIG *rig, vfo_t vfo, shortfreq_t *xit)
{
  struct dummy_priv_data *priv = (struct dummy_priv_data *)rig->state.priv;
  channel_t *curr = priv->curr;

  *xit = curr->xit;
  rig_debug(RIG_DEBUG_VERBOSE, "%s called\n", __FUNCTION__);

  return RIG_OK;
}


static int dummy_set_ts(RIG *rig, vfo_t vfo, shortfreq_t ts)
{
  struct dummy_priv_data *priv = (struct dummy_priv_data *)rig->state.priv;
  channel_t *curr = priv->curr;

  rig_debug(RIG_DEBUG_VERBOSE, "%s called\n", __FUNCTION__);
  curr->tuning_step = ts;

  return RIG_OK;
}


static int dummy_get_ts(RIG *rig, vfo_t vfo, shortfreq_t *ts)
{
  struct dummy_priv_data *priv = (struct dummy_priv_data *)rig->state.priv;
  channel_t *curr = priv->curr;

  *ts = curr->tuning_step;
  rig_debug(RIG_DEBUG_VERBOSE, "%s called\n", __FUNCTION__);

  return RIG_OK;
}


static int dummy_set_func(RIG *rig, vfo_t vfo, setting_t func, int status)
{
  struct dummy_priv_data *priv = (struct dummy_priv_data *)rig->state.priv;
  channel_t *curr = priv->curr;

  rig_debug(RIG_DEBUG_VERBOSE,"%s called: %s %d\n",__FUNCTION__, 
				  strfunc(func), status);
  if (func < RIG_SETTING_MAX) {
		if (status)
			curr->funcs |=  func;
  		else
			curr->funcs &= ~func;
  }

  return RIG_OK;
}


static int dummy_get_func(RIG *rig, vfo_t vfo, setting_t func, int *status)
{
  struct dummy_priv_data *priv = (struct dummy_priv_data *)rig->state.priv;
  channel_t *curr = priv->curr;

  if (func < RIG_SETTING_MAX)
		*status = curr->funcs & func ? 1 : 0;

  rig_debug(RIG_DEBUG_VERBOSE,"%s called: %s\n",__FUNCTION__, 
				  strfunc(func));

  return RIG_OK;
}


static int dummy_set_level(RIG *rig, vfo_t vfo, setting_t level, value_t val)
{
  struct dummy_priv_data *priv = (struct dummy_priv_data *)rig->state.priv;
  channel_t *curr = priv->curr;
  int idx;
  char lstr[32];

  idx = rig_setting2idx(level);
  if (idx < RIG_SETTING_MAX)
  	curr->levels[idx] = val;

  if (RIG_LEVEL_IS_FLOAT(level))
		  sprintf(lstr, "%f", val.f);
  else
		  sprintf(lstr, "%d", val.i);
  rig_debug(RIG_DEBUG_VERBOSE,"%s called: %s %s\n",__FUNCTION__, 
				  strlevel(level), lstr);

  return RIG_OK;
}


static int dummy_get_level(RIG *rig, vfo_t vfo, setting_t level, value_t *val)
{
  struct dummy_priv_data *priv = (struct dummy_priv_data *)rig->state.priv;
  channel_t *curr = priv->curr;
  int idx;

  idx = rig_setting2idx(level);

  if (idx < RIG_SETTING_MAX)
  	*val = curr->levels[idx];
  rig_debug(RIG_DEBUG_VERBOSE,"%s called: %s\n",__FUNCTION__, 
				  strlevel(level));

  return RIG_OK;
}


static int dummy_set_powerstat(RIG *rig, powerstat_t status)
{
  struct dummy_priv_data *priv = (struct dummy_priv_data *)rig->state.priv;

  rig_debug(RIG_DEBUG_VERBOSE, "%s called\n", __FUNCTION__);
  priv->powerstat = status;

  return RIG_OK;
}


static int dummy_get_powerstat(RIG *rig, powerstat_t *status)
{
  struct dummy_priv_data *priv = (struct dummy_priv_data *)rig->state.priv;

  *status = priv->powerstat;
  rig_debug(RIG_DEBUG_VERBOSE, "%s called\n", __FUNCTION__);

  return RIG_OK;
}


static int dummy_set_parm(RIG *rig, setting_t parm, value_t val)
{
  struct dummy_priv_data *priv = (struct dummy_priv_data *)rig->state.priv;
  int idx;
  char pstr[32];

  idx = rig_setting2idx(parm);

  if (RIG_PARM_IS_FLOAT(parm))
		  sprintf(pstr, "%f", val.f);
  else
		  sprintf(pstr, "%d", val.i);
  rig_debug(RIG_DEBUG_VERBOSE,"%s called: %s %s\n", __FUNCTION__, 
				  strparm(parm), pstr);
  if (idx < RIG_SETTING_MAX)
  	priv->parms[idx] = val;

  return RIG_OK;
}


static int dummy_get_parm(RIG *rig, setting_t parm, value_t *val)
{
  struct dummy_priv_data *priv = (struct dummy_priv_data *)rig->state.priv;
  int idx;

  idx = rig_setting2idx(parm);

  if (idx < RIG_SETTING_MAX)
  	*val = priv->parms[idx];
  rig_debug(RIG_DEBUG_VERBOSE,"%s called %s\n",__FUNCTION__, 
				  strparm(parm));

  return RIG_OK;
}


static int dummy_set_ant(RIG *rig, vfo_t vfo, ant_t ant)
{
  struct dummy_priv_data *priv = (struct dummy_priv_data *)rig->state.priv;
  channel_t *curr = priv->curr;

  curr->ant = ant;
  rig_debug(RIG_DEBUG_VERBOSE, "%s called\n", __FUNCTION__);

  return RIG_OK;
}


static int dummy_get_ant(RIG *rig, vfo_t vfo, ant_t *ant)
{
  struct dummy_priv_data *priv = (struct dummy_priv_data *)rig->state.priv;
  channel_t *curr = priv->curr;

  rig_debug(RIG_DEBUG_VERBOSE, "%s called\n", __FUNCTION__);
  *ant = curr->ant;

  return RIG_OK;
}


static int dummy_set_bank(RIG *rig, vfo_t vfo, int bank)
{
  struct dummy_priv_data *priv = (struct dummy_priv_data *)rig->state.priv;

  priv->bank = bank;
  rig_debug(RIG_DEBUG_VERBOSE, "%s called\n", __FUNCTION__);

  return RIG_OK;
}


static int dummy_set_mem(RIG *rig, vfo_t vfo, int ch)
{
  struct dummy_priv_data *priv = (struct dummy_priv_data *)rig->state.priv;

  rig_debug(RIG_DEBUG_VERBOSE, "%s called\n", __FUNCTION__);

  if (priv->curr_vfo == RIG_VFO_MEM)
		  priv->curr = &priv->mem[ch];
  else
		  priv->curr->channel_num = ch;

  return RIG_OK;
}


static int dummy_get_mem(RIG *rig, vfo_t vfo, int *ch)
{
  struct dummy_priv_data *priv = (struct dummy_priv_data *)rig->state.priv;
  channel_t *curr = priv->curr;

  *ch = curr->channel_num;
  rig_debug(RIG_DEBUG_VERBOSE, "%s called\n", __FUNCTION__);

  return RIG_OK;
}

static int dummy_scan(RIG *rig, vfo_t vfo, scan_t scan, int ch)
{
  rig_debug(RIG_DEBUG_VERBOSE,"%s called: %s %d\n",__FUNCTION__, 
				  strscan(scan), ch);

  return RIG_OK;
}

static void chan_vfo(channel_t *chan, vfo_t vfo)
{
	chan->vfo = vfo;
	strcpy(chan->channel_desc, strvfo(vfo));
}

static int dummy_vfo_op(RIG *rig, vfo_t vfo, vfo_op_t op)
{
  struct dummy_priv_data *priv = (struct dummy_priv_data *)rig->state.priv;
  channel_t *curr = priv->curr;
  int ret;
  freq_t freq;
  shortfreq_t ts;

  rig_debug(RIG_DEBUG_VERBOSE,"%s called: %s\n",__FUNCTION__, 
				  strvfop(op));

  switch (op) {
	  case RIG_OP_FROM_VFO:	/* VFO->MEM */
			  if (priv->curr_vfo == RIG_VFO_MEM) {
				int ch = curr->channel_num;
			  	memcpy(curr, priv->last_vfo == RIG_VFO_A ?
							  &priv->vfo_a : &priv->vfo_b, sizeof(channel_t));
				curr->channel_num = ch;
				curr->channel_desc[0] = '\0';
				curr->vfo = RIG_VFO_MEM;
			  } else {
				channel_t *mem_chan = &priv->mem[curr->channel_num];
			  	memcpy(mem_chan, curr, sizeof(channel_t));
				mem_chan->channel_num = curr->channel_num;
				mem_chan->channel_desc[0] = '\0';
				mem_chan->vfo = RIG_VFO_MEM;
			  }
			  break;
	  case RIG_OP_TO_VFO:         /* MEM->VFO */
			  if (priv->curr_vfo == RIG_VFO_MEM) {
			    channel_t *vfo_chan = (priv->last_vfo == RIG_VFO_A) ? 
											&priv->vfo_a : &priv->vfo_b;
			  	memcpy(vfo_chan, curr, sizeof(channel_t));
				chan_vfo(vfo_chan, priv->last_vfo);
			  } else {
			  	memcpy(&priv->mem[curr->channel_num], 
								curr, sizeof(channel_t));
				chan_vfo(curr, priv->curr_vfo);
			  }
			  break;
	  case RIG_OP_CPY:	 /* VFO A = VFO B   or   VFO B = VFO A */
			  if (priv->curr_vfo == RIG_VFO_A) {
			  	memcpy(&priv->vfo_b, &priv->vfo_a, sizeof(channel_t));
				chan_vfo(&priv->vfo_b, RIG_VFO_B);
				break;
			  } else if (priv->curr_vfo == RIG_VFO_B) {
			  	memcpy(&priv->vfo_a, &priv->vfo_b, sizeof(channel_t));
				chan_vfo(&priv->vfo_a, RIG_VFO_A);
				break;
			  }
  			  rig_debug(RIG_DEBUG_VERBOSE,"%s beep!\n", __FUNCTION__ );
			  break;
	  case RIG_OP_XCHG: /* Exchange VFO A/B */ 
			  {
				channel_t chan;
			  	memcpy(&chan, &priv->vfo_b, sizeof(channel_t));
			  	memcpy(&priv->vfo_b, &priv->vfo_a, sizeof(channel_t));
			  	memcpy(&priv->vfo_a, &chan, sizeof(channel_t));
				chan_vfo(&priv->vfo_a, RIG_VFO_A);
				chan_vfo(&priv->vfo_b, RIG_VFO_B);
				break;
			  }
	  case RIG_OP_MCL:	/* Memory clear */
			  if (priv->curr_vfo == RIG_VFO_MEM) {
					int ch = curr->channel_num;
				  	memset(curr, 0, sizeof(channel_t));
					curr->channel_num = ch;
					curr->vfo = RIG_VFO_MEM;
			  } else {
					channel_t *mem_chan = &priv->mem[curr->channel_num];
				  	memset(mem_chan, 0, sizeof(channel_t));
					mem_chan->channel_num = curr->channel_num;
					mem_chan->vfo = RIG_VFO_MEM;
			  }
			  break;
	case RIG_OP_UP:
		ret = dummy_get_freq(rig, vfo, &freq);
		if (!ret) break;
		ret = dummy_get_ts(rig, vfo, &ts);
		if (!ret) break;
		ret = dummy_set_freq(rig, vfo, freq+ts);	/* up */
		break;
        case RIG_OP_DOWN:
		ret = dummy_get_freq(rig, vfo, &freq);
		if (!ret) break;
		ret = dummy_get_ts(rig, vfo, &ts);
		if (!ret) break;
		ret = dummy_set_freq(rig, vfo, freq-ts);	/* down */
		break;

	default:
		break;
  }

  return RIG_OK;
}

static int dummy_set_channel(RIG *rig, const channel_t *chan)
{
  struct dummy_priv_data *priv = (struct dummy_priv_data *)rig->state.priv;

  rig_debug(RIG_DEBUG_VERBOSE, "%s called\n", __FUNCTION__);

  /* TODO: check chan->channel_num is in a valid range */
  switch (chan->vfo) {
	  case RIG_VFO_MEM:
	  	memcpy(&priv->mem[chan->channel_num], chan, sizeof(channel_t));
		break;
	  case RIG_VFO_A:
	  	memcpy(&priv->vfo_a, chan, sizeof(channel_t));
		break;
	  case RIG_VFO_B:
	  	memcpy(&priv->vfo_b, chan, sizeof(channel_t));
		break;
	  case RIG_VFO_CURR:
	  	memcpy(priv->curr, chan, sizeof(channel_t));
		break;
	  default:
		return -RIG_EINVAL;
  }

  return RIG_OK;
}


static int dummy_get_channel(RIG *rig, channel_t *chan)
{
  struct dummy_priv_data *priv = (struct dummy_priv_data *)rig->state.priv;

  rig_debug(RIG_DEBUG_VERBOSE, "%s called\n", __FUNCTION__);

  /* TODO: check chan->channel_num is in a valid range */
  switch (chan->vfo) {
	  case RIG_VFO_MEM:
	  	memcpy(chan, &priv->mem[chan->channel_num], sizeof(channel_t));
		break;
	  case RIG_VFO_A:
	  	memcpy(chan, &priv->vfo_a, sizeof(channel_t));
		break;
	  case RIG_VFO_B:
	  	memcpy(chan, &priv->vfo_b, sizeof(channel_t));
		break;
	  case RIG_VFO_CURR:
	  	memcpy(chan, priv->curr, sizeof(channel_t));
		break;
	  default:
		return -RIG_EINVAL;
  }

  return RIG_OK;
}


static int dummy_set_trn(RIG *rig, int trn)
{
  rig_debug(RIG_DEBUG_VERBOSE, "%s called\n", __FUNCTION__);

  return RIG_OK;
}


static int dummy_get_trn(RIG *rig, int *trn)
{
  *trn = RIG_TRN_OFF;
  rig_debug(RIG_DEBUG_VERBOSE, "%s called\n", __FUNCTION__);

  return RIG_OK;
}

static const char *dummy_get_info(RIG *rig)
{
  rig_debug(RIG_DEBUG_VERBOSE, "%s called\n", __FUNCTION__);

  return "Nothing much (dummy)";
}


static int dummy_send_dtmf(RIG *rig, vfo_t vfo, const char *digits)
{
  rig_debug(RIG_DEBUG_VERBOSE,"%s called: %s\n", __FUNCTION__, digits);

  return RIG_OK;
}

static int dummy_recv_dtmf(RIG *rig, vfo_t vfo, char *digits, int *length)
{
  rig_debug(RIG_DEBUG_VERBOSE, "%s called\n", __FUNCTION__);
  strcpy(digits, "0123456789ABCDEF");
  *length = 16;

  return RIG_OK;
}

static int dummy_send_morse(RIG *rig, vfo_t vfo, const char *msg)
{
  rig_debug(RIG_DEBUG_VERBOSE,"%s called: %s\n", __FUNCTION__, msg);

  return RIG_OK;
}



/*
 * Dummy rig capabilities.
 */

#define DUMMY_FUNC  0x7fffffffffffffLL	/* has it all */
#define DUMMY_LEVEL 0x7ffffffffffffffLL
#define DUMMY_PARM  0x7ffffffffffffffLL
#define DUMMY_VFO_OP  0x7ffffffL
#define DUMMY_SCAN	0x7ffffffL

#define DUMMY_MODES (RIG_MODE_AM | RIG_MODE_CW | RIG_MODE_RTTY | \
                     RIG_MODE_SSB | RIG_MODE_FM | RIG_MODE_WFM)

#define DUMMY_MEM_CAP {    \
	.bank_num = 1,	\
	.vfo = 1,	\
	.ant = 1,	\
	.freq = 1,	\
	.mode = 1,	\
	.width = 1,	\
	.tx_freq = 1,	\
	.tx_mode = 1,	\
	.tx_width = 1,	\
	.split = 1,	\
	.rptr_shift = 1,	\
	.rptr_offs = 1,	\
	.tuning_step = 1,	\
	.rit = 1,	\
	.xit = 1,	\
	.funcs = 1,	\
	.levels = 1,	\
	.ctcss_tone = 1,	\
	.ctcss_sql = 1,	\
	.dcs_code = 1,	\
	.dcs_sql = 1,	\
	.scan_group = 1,	\
	.flags = 1,	\
	.channel_desc = 1,	\
	.ext_levels = 1,	\
	}

const struct rig_caps dummy_caps = {
  .rig_model =      RIG_MODEL_DUMMY,
  .model_name =     "Dummy",
  .mfg_name =       "Hamlib",
  .version =        "0.2",
  .copyright = 	 "LGPL",
  .status =         RIG_STATUS_BETA,
  .rig_type =       RIG_TYPE_OTHER,
  .targetable_vfo = 	 0,
  .ptt_type =       RIG_PTT_RIG,
  .dcd_type =       RIG_DCD_RIG,
  .port_type =      RIG_PORT_NONE,
  .has_get_func =   DUMMY_FUNC,
  .has_set_func =   DUMMY_FUNC,
  .has_get_level =  DUMMY_LEVEL,
  .has_set_level =  RIG_LEVEL_SET(DUMMY_LEVEL),
  .has_get_parm = 	 DUMMY_PARM,
  .has_set_parm = 	 RIG_PARM_SET(DUMMY_PARM),
  .level_gran =		{ [LVL_CWPITCH] = 10 },
  .ctcss_list = 	 common_ctcss_list,
  .dcs_list =   	 full_dcs_list,
  .chan_list = 	 {
			{   0,  18, RIG_MTYPE_MEM, DUMMY_MEM_CAP },
			{  19,  19, RIG_MTYPE_CALL },
			{  20,  21, RIG_MTYPE_EDGE },
			RIG_CHAN_END,
		 },
  .scan_ops = 	 DUMMY_SCAN,
  .vfo_ops = 	 DUMMY_VFO_OP,
  .transceive =     RIG_TRN_OFF,
  .attenuator =     { 10, 20, 30, RIG_DBLST_END, },
  .preamp = 		 { 10, RIG_DBLST_END, },
  .rx_range_list2 =  { {.start=kHz(150),.end=MHz(1500),.modes=DUMMY_MODES,
		    .low_power=-1,.high_power=-1,RIG_VFO_A|RIG_VFO_B},
		    RIG_FRNG_END, },
  .tx_range_list2 =  { RIG_FRNG_END, },
  .tuning_steps =  { {DUMMY_MODES,1}, {DUMMY_MODES,RIG_TS_ANY}, RIG_TS_END, },
  .filters =  {
	{RIG_MODE_SSB|RIG_MODE_CW|RIG_MODE_RTTY, kHz(2.4)},
	{RIG_MODE_CW, Hz(500)},
	{RIG_MODE_AM, kHz(8)},
	{RIG_MODE_AM, kHz(2.4)},
	{RIG_MODE_FM, kHz(15)},
	{RIG_MODE_FM, kHz(8)},
	{RIG_MODE_WFM, kHz(230)},
	RIG_FLT_END,
  },

  .priv =  NULL,	/* priv */

  .rig_init =     dummy_init,
  .rig_cleanup =  dummy_cleanup,
  .rig_open =     dummy_open,
  .rig_close =    dummy_close,

  .set_freq =     dummy_set_freq,
  .get_freq =     dummy_get_freq,
  .set_mode =     dummy_set_mode,
  .get_mode =     dummy_get_mode,
  .set_vfo =      dummy_set_vfo,
  .get_vfo =      dummy_get_vfo,
  
  .set_powerstat =  dummy_set_powerstat,
  .get_powerstat =  dummy_get_powerstat,
  .set_level =     dummy_set_level,
  .get_level =     dummy_get_level,
  .set_func =      dummy_set_func,
  .get_func =      dummy_get_func,
  .set_parm =      dummy_set_parm,
  .get_parm =      dummy_get_parm,

  .get_info =      dummy_get_info,


  .set_ptt = 	dummy_set_ptt,
  .get_ptt = 	dummy_get_ptt,
  .get_dcd = 	dummy_get_dcd,
  .set_rptr_shift = 	dummy_set_rptr_shift,
  .get_rptr_shift = 	dummy_get_rptr_shift,
  .set_rptr_offs = 	dummy_set_rptr_offs,
  .get_rptr_offs = 	dummy_get_rptr_offs,
  .set_ctcss_tone = 	dummy_set_ctcss_tone,
  .get_ctcss_tone = 	dummy_get_ctcss_tone,
  .set_dcs_code = 	dummy_set_dcs_code,
  .get_dcs_code = 	dummy_get_dcs_code,
  .set_ctcss_sql = 	dummy_set_ctcss_sql,
  .get_ctcss_sql = 	dummy_get_ctcss_sql,
  .set_dcs_sql = 	dummy_set_dcs_sql,
  .get_dcs_sql = 	dummy_get_dcs_sql,
  .set_split_freq = 	dummy_set_split_freq,
  .get_split_freq = 	dummy_get_split_freq,
  .set_split_mode = 	dummy_set_split_mode,
  .get_split_mode = 	dummy_get_split_mode,
  .set_split_vfo = 	dummy_set_split_vfo,
  .get_split_vfo = 	dummy_get_split_vfo,
  .set_rit = 	dummy_set_rit,
  .get_rit = 	dummy_get_rit,
  .set_xit = 	dummy_set_xit,
  .get_xit = 	dummy_get_xit,
  .set_ts = 	dummy_set_ts,
  .get_ts = 	dummy_get_ts,
  .set_ant = 	dummy_set_ant,
  .get_ant = 	dummy_get_ant,
  .set_bank = 	dummy_set_bank,
  .set_mem = 	dummy_set_mem,
  .get_mem = 	dummy_get_mem,
  .vfo_op = 	dummy_vfo_op,
  .scan = 		dummy_scan,
  .send_dtmf =  dummy_send_dtmf,
  .recv_dtmf =  dummy_recv_dtmf,
  .send_morse =  dummy_send_morse,
  .set_channel = 	dummy_set_channel,
  .get_channel = 	dummy_get_channel,
  .set_trn = 	dummy_set_trn,
  .get_trn = 	dummy_get_trn,
};

int initrigs_dummy(void *be_handle)
{
	rig_debug(RIG_DEBUG_VERBOSE, "dummy: _init called\n");

	rig_register(&dummy_caps);

	return RIG_OK;
}
