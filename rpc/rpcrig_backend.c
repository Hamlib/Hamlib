/*
 *  Hamlib RPC backend - main file
 *  Copyright (c) 2001 by Stephane Fillod
 *
 *		$Id: rpcrig_backend.c,v 1.1 2001-10-07 21:42:13 f4cfe Exp $
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

#if defined(__CYGWIN__)
#  undef HAMLIB_DLL
#  include <hamlib/rig.h>
#  include <serial.h>
#  include <misc.h>
#  define HAMLIB_DLL
#  include <hamlib/rig_dll.h>
#else
#  include <hamlib/rig.h>
#  include <serial.h>
#  include <misc.h>
#endif

#include <rpc/rpc.h>
#include "rpcrig.h"

#include "rpcrig_backend.h"


/********************************************************************/
void freq_t2freq_s(freq_t freqt, freq_s *freqs)
{
	freqs->f1 = freqt & 0xffffffff;
	freqs->f2 = (freqt>>32) & 0xffffffff;
}
freq_t freq_s2freq_t(freq_s *freqs)
{
	return freqs->f1 | ((freq_t)freqs->f2 << 32);
}

struct rpcrig_priv_data {
		CLIENT *cl;
};

static int rpcrig_init(RIG *rig)
{
	if (!rig || !rig->caps)
		return -RIG_EINVAL;
	
	rig->state.priv = malloc(sizeof(struct rpcrig_priv_data));
	if (!rig->state.priv) {
		/* whoops! memory shortage! */
		return -RIG_ENOMEM;
	}
	rig->state.rigport.type.rig = RIG_PORT_NONE;

	return RIG_OK;
}

static int rpcrig_cleanup(RIG *rig)
{
	if (!rig)
		return -RIG_EINVAL;

	if (rig->state.priv)
		free(rig->state.priv);

	rig->state.priv = NULL;

	return RIG_OK;
}

static int rpcrig_open(RIG *rig)
{
	struct rpcrig_priv_data *priv;
	struct rig_state *rs;
	model_x *result;
	char *server = "localhost";
	rig_model_t model;
	const struct rig_caps *caps;
	int i;

	rs = &rig->state;
	priv = (struct rpcrig_priv_data*)rs->priv;

	priv->cl = clnt_create(server, RIGPROG, RIGVERS, "udp");
	if (priv->cl == NULL) {
		clnt_pcreateerror(server);
		return -RIG_ECONF;
	}
	result = getmodel_1(NULL, priv->cl);
	if (result == NULL) {
		clnt_perror(priv->cl, server);
		return -RIG_EPROTO;
	}
	model = *result;
	rig_debug(RIG_DEBUG_VERBOSE,__FUNCTION__ ": model %d\n", model);
	caps = rig_get_caps(model);

	/*
	 * TODO: get these from RPC instead
	 */
#if 0
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
#endif

	return RIG_OK;
}

static int rpcrig_close(RIG *rig)
{
	struct rpcrig_priv_data *priv;

	priv = (struct rpcrig_priv_data*)rig->state.priv;

	clnt_destroy(priv->cl);

	return RIG_OK;
}

static int rpcrig_set_freq(RIG *rig, vfo_t vfo, freq_t freq)
{
	struct rpcrig_priv_data *priv;
	struct rig_state *rs;
	int *result;
	freq_arg farg;
	char *server = "localhost";

	rs = &rig->state;
	priv = (struct rpcrig_priv_data*)rs->priv;

	freq_t2freq_s(freq, &farg.freq);
	result = setfreq_1(&farg, priv->cl);
	if (result == NULL) {
		clnt_perror(priv->cl, server);
		return -RIG_EPROTO;
	}

	return *result;
}


static int rpcrig_get_freq(RIG *rig, vfo_t vfo, freq_t *freq)
{
	struct rpcrig_priv_data *priv;
	struct rig_state *rs;
	freq_res *fres;
	vfo_x v;
	char *server = "localhost";

	rs = &rig->state;
	priv = (struct rpcrig_priv_data*)rs->priv;

	v = vfo;
	fres = getfreq_1(&v, priv->cl);
	if (fres == NULL) {
		clnt_perror(priv->cl, server);
		return -RIG_EPROTO;
	}
	if (fres->rigstatus == RIG_OK)
		*freq = freq_s2freq_t(&fres->freq_res_u.freq);

	return fres->rigstatus;
}


static int rpcrig_set_mode(RIG *rig, vfo_t vfo, rmode_t mode, pbwidth_t width)
{
  rig_debug(RIG_DEBUG_VERBOSE,__FUNCTION__ " called\n");

  return RIG_OK;
}


static int rpcrig_get_mode(RIG *rig, vfo_t vfo, rmode_t *mode, pbwidth_t *width)
{
  rig_debug(RIG_DEBUG_VERBOSE,__FUNCTION__ " called\n");

  return RIG_OK;
}


static int rpcrig_set_vfo(RIG *rig, vfo_t vfo)
{
  rig_debug(RIG_DEBUG_VERBOSE,__FUNCTION__ " called\n");

  return RIG_OK;
}


static int rpcrig_get_vfo(RIG *rig, vfo_t *vfo)
{
  rig_debug(RIG_DEBUG_VERBOSE,__FUNCTION__ " called\n");

  return RIG_OK;
}


static int rpcrig_set_ptt(RIG *rig, vfo_t vfo, ptt_t ptt)
{
  rig_debug(RIG_DEBUG_VERBOSE,__FUNCTION__ " called\n");

  return RIG_OK;
}


static int rpcrig_get_ptt(RIG *rig, vfo_t vfo, ptt_t *ptt)
{
  rig_debug(RIG_DEBUG_VERBOSE,__FUNCTION__ " called\n");

  return RIG_OK;
}


static int rpcrig_get_dcd(RIG *rig, vfo_t vfo, dcd_t *dcd)
{
  rig_debug(RIG_DEBUG_VERBOSE,__FUNCTION__ " called\n");

  return RIG_OK;
}


static int rpcrig_set_rptr_shift(RIG *rig, vfo_t vfo, rptr_shift_t rptr_shift)
{
  rig_debug(RIG_DEBUG_VERBOSE,__FUNCTION__ " called\n");

  return RIG_OK;
}


static int rpcrig_get_rptr_shift(RIG *rig, vfo_t vfo, rptr_shift_t *rptr_shift)
{
  rig_debug(RIG_DEBUG_VERBOSE,__FUNCTION__ " called\n");

  return RIG_OK;
}


static int rpcrig_set_rptr_offs(RIG *rig, vfo_t vfo, shortfreq_t rptr_offs)
{
  rig_debug(RIG_DEBUG_VERBOSE,__FUNCTION__ " called\n");

  return RIG_OK;
}


static int rpcrig_get_rptr_offs(RIG *rig, vfo_t vfo, shortfreq_t *rptr_offs)
{
  rig_debug(RIG_DEBUG_VERBOSE,__FUNCTION__ " called\n");

  return RIG_OK;
}


static int rpcrig_set_ctcss_tone(RIG *rig, vfo_t vfo, tone_t tone)
{
  rig_debug(RIG_DEBUG_VERBOSE,__FUNCTION__ " called\n");

  return RIG_OK;
}


static int rpcrig_get_ctcss_tone(RIG *rig, vfo_t vfo, tone_t *tone)
{
  rig_debug(RIG_DEBUG_VERBOSE,__FUNCTION__ " called\n");

  return RIG_OK;
}


static int rpcrig_set_dcs_code(RIG *rig, vfo_t vfo, tone_t code)
{
  rig_debug(RIG_DEBUG_VERBOSE,__FUNCTION__ " called\n");

  return RIG_OK;
}


static int rpcrig_get_dcs_code(RIG *rig, vfo_t vfo, tone_t *code)
{
  rig_debug(RIG_DEBUG_VERBOSE,__FUNCTION__ " called\n");

  return RIG_OK;
}


static int rpcrig_set_ctcss_sql(RIG *rig, vfo_t vfo, tone_t tone)
{
  rig_debug(RIG_DEBUG_VERBOSE,__FUNCTION__ " called\n");

  return RIG_OK;
}


static int rpcrig_get_ctcss_sql(RIG *rig, vfo_t vfo, tone_t *tone)
{
  rig_debug(RIG_DEBUG_VERBOSE,__FUNCTION__ " called\n");

  return RIG_OK;
}


static int rpcrig_set_dcs_sql(RIG *rig, vfo_t vfo, unsigned int code)
{
  rig_debug(RIG_DEBUG_VERBOSE,__FUNCTION__ " called\n");

  return RIG_OK;
}


static int rpcrig_get_dcs_sql(RIG *rig, vfo_t vfo, unsigned int *code)
{
  rig_debug(RIG_DEBUG_VERBOSE,__FUNCTION__ " called\n");

  return RIG_OK;
}


static int rpcrig_set_split_freq(RIG *rig, vfo_t vfo, freq_t tx_freq)
{
  rig_debug(RIG_DEBUG_VERBOSE,__FUNCTION__ " called\n");

  return RIG_OK;
}


static int rpcrig_get_split_freq(RIG *rig, vfo_t vfo, freq_t *tx_freq)
{
  rig_debug(RIG_DEBUG_VERBOSE,__FUNCTION__ " called\n");

  return RIG_OK;
}

static int rpcrig_set_split_mode(RIG *rig, vfo_t vfo, rmode_t tx_mode, pbwidth_t tx_width)
{
  rig_debug(RIG_DEBUG_VERBOSE,__FUNCTION__ " called\n");

  return RIG_OK;
}

static int rpcrig_get_split_mode(RIG *rig, vfo_t vfo, rmode_t *tx_mode, pbwidth_t *tx_width)
{
  rig_debug(RIG_DEBUG_VERBOSE,__FUNCTION__ " called\n");

  return RIG_OK;
}

static int rpcrig_set_split(RIG *rig, vfo_t vfo, split_t split)
{
  rig_debug(RIG_DEBUG_VERBOSE,__FUNCTION__ " called\n");

  return RIG_OK;
}


static int rpcrig_get_split(RIG *rig, vfo_t vfo, split_t *split)
{
  rig_debug(RIG_DEBUG_VERBOSE,__FUNCTION__ " called\n");

  return RIG_OK;
}

static int rpcrig_set_rit(RIG *rig, vfo_t vfo, shortfreq_t rit)
{
  rig_debug(RIG_DEBUG_VERBOSE,__FUNCTION__ " called\n");

  return RIG_OK;
}


static int rpcrig_get_rit(RIG *rig, vfo_t vfo, shortfreq_t *rit)
{
  rig_debug(RIG_DEBUG_VERBOSE,__FUNCTION__ " called\n");

  return RIG_OK;
}


static int rpcrig_set_xit(RIG *rig, vfo_t vfo, shortfreq_t xit)
{
  rig_debug(RIG_DEBUG_VERBOSE,__FUNCTION__ " called\n");

  return RIG_OK;
}


static int rpcrig_get_xit(RIG *rig, vfo_t vfo, shortfreq_t *xit)
{
  rig_debug(RIG_DEBUG_VERBOSE,__FUNCTION__ " called\n");

  return RIG_OK;
}


static int rpcrig_set_ts(RIG *rig, vfo_t vfo, shortfreq_t ts)
{
  rig_debug(RIG_DEBUG_VERBOSE,__FUNCTION__ " called\n");

  return RIG_OK;
}


static int rpcrig_get_ts(RIG *rig, vfo_t vfo, shortfreq_t *ts)
{
  rig_debug(RIG_DEBUG_VERBOSE,__FUNCTION__ " called\n");

  return RIG_OK;
}


static int rpcrig_power2mW(RIG *rig, unsigned int *mwpower, float power, freq_t freq, rmode_t mode)
{
  rig_debug(RIG_DEBUG_VERBOSE,__FUNCTION__ " called\n");

  return RIG_OK;
}


static int rpcrig_mW2power(RIG *rig, float *power, unsigned int mwpower, freq_t freq, rmode_t mode)
{
  rig_debug(RIG_DEBUG_VERBOSE,__FUNCTION__ " called\n");

  return RIG_OK;
}


static int rpcrig_set_level(RIG *rig, vfo_t vfo, setting_t level, value_t val)
{
  rig_debug(RIG_DEBUG_VERBOSE,__FUNCTION__ " called\n");

  return RIG_OK;
}


static int rpcrig_get_level(RIG *rig, vfo_t vfo, setting_t level, value_t *val)
{
  rig_debug(RIG_DEBUG_VERBOSE,__FUNCTION__ " called\n");

  return RIG_OK;
}


static int rpcrig_set_powerstat(RIG *rig, powerstat_t status)
{
  rig_debug(RIG_DEBUG_VERBOSE,__FUNCTION__ " called\n");

  return RIG_OK;
}


static int rpcrig_get_powerstat(RIG *rig, powerstat_t *status)
{
  rig_debug(RIG_DEBUG_VERBOSE,__FUNCTION__ " called\n");

  return RIG_OK;
}


static int rpcrig_set_parm(RIG *rig, setting_t parm, value_t val)
{
  rig_debug(RIG_DEBUG_VERBOSE,__FUNCTION__ " called\n");

  return RIG_OK;
}


static int rpcrig_get_parm(RIG *rig, setting_t parm, value_t *val)
{
  rig_debug(RIG_DEBUG_VERBOSE,__FUNCTION__ " called\n");

  return RIG_OK;
}


static int rpcrig_set_ant(RIG *rig, vfo_t vfo, ant_t ant)
{
  rig_debug(RIG_DEBUG_VERBOSE,__FUNCTION__ " called\n");

  return RIG_OK;
}


static int rpcrig_get_ant(RIG *rig, vfo_t vfo, ant_t *ant)
{
  rig_debug(RIG_DEBUG_VERBOSE,__FUNCTION__ " called\n");

  return RIG_OK;
}


static int rpcrig_set_func(RIG *rig, vfo_t vfo, setting_t func, int status)
{
  rig_debug(RIG_DEBUG_VERBOSE,__FUNCTION__ " called\n");

  return RIG_OK;
}


static int rpcrig_get_func(RIG *rig, vfo_t vfo, setting_t func, int *status)
{
  rig_debug(RIG_DEBUG_VERBOSE,__FUNCTION__ " called\n");

  return RIG_OK;
}


static int rpcrig_set_bank(RIG *rig, vfo_t vfo, int bank)
{
  rig_debug(RIG_DEBUG_VERBOSE,__FUNCTION__ " called\n");

  return RIG_OK;
}


static int rpcrig_set_mem(RIG *rig, vfo_t vfo, int ch)
{
  rig_debug(RIG_DEBUG_VERBOSE,__FUNCTION__ " called\n");

  return RIG_OK;
}


static int rpcrig_get_mem(RIG *rig, vfo_t vfo, int *ch)
{
  rig_debug(RIG_DEBUG_VERBOSE,__FUNCTION__ " called\n");

  return RIG_OK;
}


static int rpcrig_vfo_op(RIG *rig, vfo_t vfo, vfo_op_t op)
{
  rig_debug(RIG_DEBUG_VERBOSE,__FUNCTION__ " called\n");

  return RIG_OK;
}

static int rpcrig_set_channel(RIG *rig, const channel_t *chan)
{
  rig_debug(RIG_DEBUG_VERBOSE,__FUNCTION__ " called\n");

  return RIG_OK;
}


static int rpcrig_get_channel(RIG *rig, channel_t *chan)
{
  rig_debug(RIG_DEBUG_VERBOSE,__FUNCTION__ " called\n");

  return RIG_OK;
}


static int rpcrig_set_trn(RIG *rig, vfo_t vfo, int trn)
{
  rig_debug(RIG_DEBUG_VERBOSE,__FUNCTION__ " called\n");

  return RIG_OK;
}


static int rpcrig_get_trn(RIG *rig, vfo_t vfo, int *trn)
{
  rig_debug(RIG_DEBUG_VERBOSE,__FUNCTION__ " called\n");

  return RIG_OK;
}

static const char *rpcrig_get_info(RIG *rig)
{
  rig_debug(RIG_DEBUG_VERBOSE,__FUNCTION__ " called\n");

  return "";
}


static int rpcrig_send_dtmf(RIG *rig, vfo_t vfo, const char *digits)
{
  rig_debug(RIG_DEBUG_VERBOSE,__FUNCTION__ " called\n");

  return RIG_OK;
}

static int rpcrig_recv_dtmf(RIG *rig, vfo_t vfo, char *digits, int *length)
{
  rig_debug(RIG_DEBUG_VERBOSE,__FUNCTION__ " called\n");

  return RIG_OK;
}

static int rpcrig_send_morse(RIG *rig, vfo_t vfo, const char *msg)
{
  rig_debug(RIG_DEBUG_VERBOSE,__FUNCTION__ " called\n");

  return RIG_OK;
}



/*
 * Dummy rig capabilities.
 */

#define RPCRIG_FUNC  0
#define RPCRIG_SET_LEVEL (RIG_LEVEL_ATT | RIG_LEVEL_AF)
#define RPCRIG_LEVEL (RPCRIG_SET_LEVEL | RIG_LEVEL_STRENGTH)

#define RPCRIG_MODES (RIG_MODE_AM | RIG_MODE_CW | \
                     RIG_MODE_USB | RIG_MODE_LSB | RIG_MODE_FM | RIG_MODE_WFM)

struct rig_caps rpcrig_caps = {
  rig_model:     RIG_MODEL_RPC,
  model_name:    "RPC rig",
  mfg_name:      "Hamlib",
  version:       "0.1",
  copyright:	 "LGPL",
  status:        RIG_STATUS_NEW,
  rig_type:      RIG_TYPE_OTHER,
  targetable_vfo:	 0,
  ptt_type:      RIG_PTT_NONE,
  dcd_type:      RIG_DCD_NONE,
  port_type:     RIG_PORT_NONE,
  has_get_func:  RPCRIG_FUNC,
  has_set_func:  RPCRIG_FUNC,
  has_get_level: RPCRIG_LEVEL,
  has_set_level: RPCRIG_SET_LEVEL,
  has_get_parm:	 RIG_PARM_NONE,	/* FIXME */
  has_set_parm:	 RIG_PARM_NONE,	/* FIXME */
  ctcss_list:	 NULL,	/* FIXME */
  dcs_list:  	 NULL,  /* FIXME */
  chan_list:	 { RIG_CHAN_END, },	/* FIXME */
  transceive:    RIG_TRN_OFF,
  attenuator:    { 10, 20, 30, RIG_DBLST_END, },
  rx_range_list2: { {start:kHz(150),end:MHz(1500),modes:RPCRIG_MODES,
		    low_power:-1,high_power:-1,RIG_VFO_A|RIG_VFO_B},
		    RIG_FRNG_END, },
  tx_range_list2: { RIG_FRNG_END, },
  tuning_steps: { {RPCRIG_MODES,1}, RIG_TS_END, },
  priv: NULL,	/* priv */

  rig_init:    rpcrig_init,
  rig_cleanup: rpcrig_cleanup,
  rig_open:    rpcrig_open,
  rig_close:   rpcrig_close,

  set_freq:    rpcrig_set_freq,
  get_freq:    rpcrig_get_freq,
  set_mode:    rpcrig_set_mode,
  get_mode:    rpcrig_get_mode,
  set_vfo:     rpcrig_set_vfo,
  get_vfo:     rpcrig_get_vfo,
  
  set_powerstat: rpcrig_set_powerstat,
  get_powerstat: rpcrig_get_powerstat,
  set_level:    rpcrig_set_level,
  get_level:    rpcrig_get_level,
  set_func:     rpcrig_set_func,
  get_func:     rpcrig_get_func,
  set_parm:     rpcrig_set_parm,
  get_parm:     rpcrig_get_parm,

  get_info:     rpcrig_get_info,


  set_ptt:	rpcrig_set_ptt,
  get_ptt:	rpcrig_get_ptt,
  get_dcd:	rpcrig_get_dcd,
  set_rptr_shift:	rpcrig_set_rptr_shift,
  get_rptr_shift:	rpcrig_get_rptr_shift,
  set_rptr_offs:	rpcrig_set_rptr_offs,
  get_rptr_offs:	rpcrig_get_rptr_offs,
  set_ctcss_tone:	rpcrig_set_ctcss_tone,
  get_ctcss_tone:	rpcrig_get_ctcss_tone,
  set_dcs_code:	rpcrig_set_dcs_code,
  get_dcs_code:	rpcrig_get_dcs_code,
  set_ctcss_sql:	rpcrig_set_ctcss_sql,
  get_ctcss_sql:	rpcrig_get_ctcss_sql,
  set_dcs_sql:	rpcrig_set_dcs_sql,
  get_dcs_sql:	rpcrig_get_dcs_sql,
  set_split_freq:	rpcrig_set_split_freq,
  get_split_freq:	rpcrig_get_split_freq,
  set_split_mode:	rpcrig_set_split_mode,
  get_split_mode:	rpcrig_get_split_mode,
  set_split:	rpcrig_set_split,
  get_split:	rpcrig_get_split,
  set_rit:	rpcrig_set_rit,
  get_rit:	rpcrig_get_rit,
  set_xit:	rpcrig_set_xit,
  get_xit:	rpcrig_get_xit,
  set_ts:	rpcrig_set_ts,
  get_ts:	rpcrig_get_ts,
  power2mW:	rpcrig_power2mW,
  mW2power:	rpcrig_mW2power,
  set_ant:	rpcrig_set_ant,
  get_ant:	rpcrig_get_ant,
  set_bank:	rpcrig_set_bank,
  set_mem:	rpcrig_set_mem,
  get_mem:	rpcrig_get_mem,
  vfo_op:	rpcrig_vfo_op,
  send_dtmf: rpcrig_send_dtmf,
  recv_dtmf: rpcrig_recv_dtmf,
  send_morse: rpcrig_send_morse,
  set_channel:	rpcrig_set_channel,
  get_channel:	rpcrig_get_channel,
  set_trn:	rpcrig_set_trn,
  get_trn:	rpcrig_get_trn,
};

int init_rpcrig(void *be_handle)
{
	rig_debug(RIG_DEBUG_VERBOSE, "rpcrig: _init called\n");

	rig_register(&rpcrig_caps);

	return RIG_OK;
}


