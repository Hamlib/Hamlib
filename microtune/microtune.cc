/*
 *  Hamlib Microtune backend - main file
 *  Copyright (c) 2003 by Stephane Fillod
 *
 *
 *   This library is free software; you can redistribute it and/or modify
 *   it under the terms of the GNU General Public License as published by
 *   the Free Software Foundation; either version 2, or (at your option)
 *   any later version.
 *
 *   This program is distributed in the hope that it will be useful,
 *   but WITHOUT ANY WARRANTY; without even the implied warranty of
 *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *   GNU General Public License for more details.
 *
 *   You should have received a copy of the GNU General Public License
 *   along with this software; see the file COPYING.  If not, write to
 *   the Free Software Foundation, Inc., 59 Temple Place - Suite 330,
 *   Boston, MA 02111-1307, USA.
 *
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <stdlib.h>

#include <hamlib/rig.h>

#include "register.h"

#include "microtune.h"
#include "microtune_eval_board.h"

#include "microtune_4937.h"
#include "microtune_4702.h"

/*
 * TODO: allow these to be modifiable through rig_set_conf
	int	i2c_addr;
	int	reference_divider;
	bool	fast_tuning_p;	 if set, higher charge pump current:
				   faster tuning, worse phase noise
				   for distance < 10kHz to the carrier
 */

struct microtune_priv_data {
	microtune_eval_board *board;
	freq_t actual_freq;
};

/*
 * TODO:
 * 	- status iff PLL is locked
 *	- returns the output frequency of the tuner in Hz (->5.75e6) //3x7702.
 *							or 36.00e6
 */

int microtune_init(RIG *rig)
{
	struct microtune_priv_data *priv;

	priv = (struct microtune_priv_data*)malloc(sizeof(struct microtune_priv_data));
	if (!priv) {
		/* whoops! memory shortage! */
		return -RIG_ENOMEM;
	}

	priv->actual_freq = RIG_FREQ_NONE;

	rig->state.priv = (void*)priv;

	return RIG_OK;
}

int module_4937_open(RIG *rig)
{
	struct microtune_priv_data *priv = (struct microtune_priv_data *)rig->state.priv;

	priv->board = new microtune_4937(&rig->state.rigport);
	if (!priv->board) {
		return -RIG_ENOMEM;
	}

	if (!priv->board->board_present_p()) {
		rig_debug(RIG_DEBUG_WARN, "microtune: eval board is NOT present\n");
		delete priv->board;
		return -RIG_EPROTO;
	}

	return RIG_OK;
}

int module_4702_open(RIG *rig)
{
	struct microtune_priv_data *priv = (struct microtune_priv_data *)rig->state.priv;

	priv->board = new microtune_4702(&rig->state.rigport);
	if (!priv->board) {
		return -RIG_ENOMEM;
	}

	if (!priv->board->board_present_p()) {
		rig_debug(RIG_DEBUG_WARN, "microtune: eval board is NOT present\n");
		delete priv->board;
		return -RIG_EPROTO;
	}

	return RIG_OK;
}

int microtune_close(RIG *rig)
{
	struct microtune_priv_data *priv = (struct microtune_priv_data *)rig->state.priv;

	delete priv->board;

	return RIG_OK;
}

int microtune_cleanup(RIG *rig)
{
	struct microtune_priv_data *priv = (struct microtune_priv_data *)rig->state.priv;

	if (priv) {
		free(priv);
	}
	rig->state.priv = NULL;

	return RIG_OK;
}

/*
 * It takes about 100 ms for the PLL to settle.
 */
int microtune_set_freq(RIG *rig, vfo_t vfo, freq_t freq)
{
  double actual_freq;
  bool status;

  struct microtune_priv_data *priv = (struct microtune_priv_data *)rig->state.priv;

  status = priv->board->set_RF_freq((double)freq, &actual_freq);

  if (!status)
	  return -RIG_EIO;

  priv->actual_freq = (freq_t)actual_freq;
  return RIG_OK;
}


int microtune_get_freq(RIG *rig, vfo_t vfo, freq_t *freq)
{
  struct microtune_priv_data *priv = (struct microtune_priv_data *)rig->state.priv;

  *freq = priv->actual_freq;

  return RIG_OK;
}


/*
 * Assumes rig!=NULL, rig->state.priv!=NULL
 */
int microtune_set_ext_level(RIG *rig, vfo_t vfo, token_t token, value_t val)
{
  	struct microtune_priv_data *priv = (struct microtune_priv_data *)rig->state.priv;

	switch(token) {
	case TOK_AGCGAIN:
  		priv->board->set_AGC(val.f*1000);
		break;
	default:
		return -RIG_EINVAL;
	}
	return RIG_OK;
}




DECLARE_INITRIG_BACKEND(microtune)
{
	rig_debug(RIG_DEBUG_VERBOSE, "microtune: _init called\n");

	rig_register(&module_4937_caps);
	rig_register(&module_4702_caps);

	return RIG_OK;
}
