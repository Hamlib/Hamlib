/*
 *  Hamlib RPC backend - main file
 *  Copyright (c) 2001,2002 by Stephane Fillod
 *
 *	$Id: rpcrig_backend.c,v 1.7 2002-07-08 22:53:26 fillods Exp $
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

#include <rpc/rpc.h>
#include "rpcrig.h"

#include "rpcrig_backend.h"


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
	rig->state.rigport.type.rig = RIG_PORT_RPC;
	strcpy(rig->state.rigport.pathname, "localhost");

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

/*
 * assumes rig!=NULL, rs->priv != NULL
 */
static int rpcrig_open(RIG *rig)
{
	struct rpcrig_priv_data *priv;
	struct rig_state *rs;
	model_x *mdl_res;
	rigstate_res *rs_res;
	rig_model_t model;
	const struct rig_caps *caps;
	char *server;
	int i;

	rs = &rig->state;
	priv = (struct rpcrig_priv_data*)rs->priv;
	server = rs->rigport.pathname;

	priv->cl = clnt_create(server, RIGPROG, RIGVERS, "udp");
	if (priv->cl == NULL) {
		clnt_pcreateerror(server);
		return -RIG_ECONF;
	}
	mdl_res = getmodel_1(NULL, priv->cl);
	if (mdl_res == NULL) {
		clnt_perror(priv->cl, server);
		clnt_destroy(priv->cl);
		priv->cl = NULL;
		return -RIG_EPROTO;
	}
	model = *mdl_res;
	rig_debug(RIG_DEBUG_VERBOSE,"%s: model %d\n", __FUNCTION__, model);

	/*
	 * autoload if necessary
	 */
	rig_check_backend(model);

	caps = rig_get_caps(model);

	/*
	 * TODO: decide if it's the way to go.
	 * 		 This for example breaks reentrancy
	 */
	//memcpy(&rpcrig_caps, caps, sizeof(struct rig_caps));
	
	/*
	 * TODO: get these from RPC instead
	 */

	rs_res = getrigstate_1(NULL, priv->cl);
	if (rs_res == NULL) {
		clnt_perror(priv->cl, server);
		clnt_destroy(priv->cl);
		priv->cl = NULL;
		return -RIG_EPROTO;
	}
	rs->has_get_func = rs_res->rigstate_res_u.state.has_get_func;
	rs->has_set_func = rs_res->rigstate_res_u.state.has_set_func;
	rs->has_get_level = rs_res->rigstate_res_u.state.has_get_level;
	rs->has_set_level = rs_res->rigstate_res_u.state.has_set_level;
	rs->has_get_parm = rs_res->rigstate_res_u.state.has_get_parm;
	rs->has_set_parm = rs_res->rigstate_res_u.state.has_set_parm;

	rs->max_rit = rs_res->rigstate_res_u.state.max_rit;
	rs->max_xit = rs_res->rigstate_res_u.state.max_xit;
	rs->max_ifshift = rs_res->rigstate_res_u.state.max_ifshift;
	rs->announces = rs_res->rigstate_res_u.state.announces;

	memcpy(rs->preamp, rs_res->rigstate_res_u.state.preamp, 
					sizeof(int)*MAXDBLSTSIZ);
	memcpy(rs->attenuator, rs_res->rigstate_res_u.state.attenuator, 
					sizeof(int)*MAXDBLSTSIZ);

	memcpy(rs->tuning_steps, rs_res->rigstate_res_u.state.tuning_steps, 
					sizeof(struct tuning_step_list)*TSLSTSIZ);
	memcpy(rs->filters, rs_res->rigstate_res_u.state.filters, 
					sizeof(struct filter_list)*FLTLSTSIZ);
	memcpy(rs->chan_list, rs_res->rigstate_res_u.state.chan_list, 
					sizeof(chan_t)*CHANLSTSIZ);
	memcpy(rs->rx_range_list, rs_res->rigstate_res_u.state.rx_range_list, 
					sizeof(freq_range_t)*FRQRANGESIZ);
	memcpy(rs->tx_range_list, rs_res->rigstate_res_u.state.tx_range_list, 
					sizeof(freq_range_t)*FRQRANGESIZ);

	for (i=0; i<FRQRANGESIZ && !RIG_IS_FRNG_END(rs->rx_range_list[i]); i++) {
			rs->vfo_list |= rs->rx_range_list[i].vfo;
	}
	for (i=0; i<FRQRANGESIZ && !RIG_IS_FRNG_END(rs->tx_range_list[i]); i++) {
			rs->vfo_list |= rs->tx_range_list[i].vfo;
	}

	return RIG_OK;
}

static int rpcrig_close(RIG *rig)
{
	struct rpcrig_priv_data *priv;

	priv = (struct rpcrig_priv_data*)rig->state.priv;

	if (priv->cl)
		clnt_destroy(priv->cl);

	return RIG_OK;
}

static int rpcrig_set_vfo(RIG *rig, vfo_t vfo)
{
	struct rpcrig_priv_data *priv;
	int *result;
	vfo_x varg;

	priv = (struct rpcrig_priv_data*)rig->state.priv;

	varg = vfo;
	result = setvfo_1(&varg, priv->cl);
	if (result == NULL) {
		clnt_perror(priv->cl, "setvfo_1");
		return -RIG_EPROTO;
	}

	return *result;
}


static int rpcrig_get_vfo(RIG *rig, vfo_t *vfo)
{
	struct rpcrig_priv_data *priv;
	vfo_x v;
	vfo_res *vres;

	priv = (struct rpcrig_priv_data*)rig->state.priv;

	v = *vfo;
	vres = getvfo_1(&v, priv->cl);
	if (vres == NULL) {
		clnt_perror(priv->cl, "getvfo_1");
		return -RIG_EPROTO;
	}
	if (vres->rigstatus == RIG_OK)
		*vfo = vres->vfo_res_u.vfo;

	return vres->rigstatus;
}


static int rpcrig_set_freq(RIG *rig, vfo_t vfo, freq_t freq)
{
	struct rpcrig_priv_data *priv;
	struct rig_state *rs;
	int *result;
	freq_arg farg;

	rs = &rig->state;
	priv = (struct rpcrig_priv_data*)rs->priv;

	farg.vfo = vfo;
	freq_t2x(freq, &farg.freq);
	result = setfreq_1(&farg, priv->cl);
	if (result == NULL) {
		clnt_perror(priv->cl, "setfreq_1");
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

	rs = &rig->state;
	priv = (struct rpcrig_priv_data*)rs->priv;

	v = vfo;
	fres = getfreq_1(&v, priv->cl);
	if (fres == NULL) {
		clnt_perror(priv->cl, "getfreq_1");
		return -RIG_EPROTO;
	}
	if (fres->rigstatus == RIG_OK)
		*freq = freq_x2t(&fres->freq_res_u.freq);

	return fres->rigstatus;
}


static int rpcrig_set_mode(RIG *rig, vfo_t vfo, rmode_t mode, pbwidth_t width)
{
	struct rpcrig_priv_data *priv;
	int *result;
	mode_arg marg;

	priv = (struct rpcrig_priv_data*)rig->state.priv;

	marg.vfo = vfo;
	mode_t2s(mode, width, &marg.mw);
	result = setmode_1(&marg, priv->cl);
	if (result == NULL) {
		clnt_perror(priv->cl, "setmode_1");
		return -RIG_EPROTO;
	}

	return *result;
}


static int rpcrig_get_mode(RIG *rig, vfo_t vfo, rmode_t *mode, pbwidth_t *width)
{
	struct rpcrig_priv_data *priv;
	mode_res *mres;
	vfo_x v;

	priv = (struct rpcrig_priv_data*)rig->state.priv;

	v = vfo;
	mres = getmode_1(&v, priv->cl);
	if (mres == NULL) {
		clnt_perror(priv->cl, "getmode_1");
		return -RIG_EPROTO;
	}
	if (mres->rigstatus == RIG_OK)
		mode_s2t(&mres->mode_res_u.mw, mode, width);

	return mres->rigstatus;
}

static int rpcrig_set_split_freq(RIG *rig, vfo_t vfo, freq_t tx_freq)
{
	struct rpcrig_priv_data *priv;
	struct rig_state *rs;
	int *result;
	freq_arg farg;

	rs = &rig->state;
	priv = (struct rpcrig_priv_data*)rs->priv;

	farg.vfo = vfo;
	freq_t2x(tx_freq, &farg.freq);
	result = setsplitfreq_1(&farg, priv->cl);
	if (result == NULL) {
		clnt_perror(priv->cl, "setsplitfreq_1");
		return -RIG_EPROTO;
	}

	return *result;
}

static int rpcrig_get_split_freq(RIG *rig, vfo_t vfo, freq_t *tx_freq)
{
	struct rpcrig_priv_data *priv;
	struct rig_state *rs;
	freq_res *fres;
	vfo_x v;

	rs = &rig->state;
	priv = (struct rpcrig_priv_data*)rs->priv;

	v = vfo;
	fres = getsplitfreq_1(&v, priv->cl);
	if (fres == NULL) {
		clnt_perror(priv->cl, "getsplitfreq_1");
		return -RIG_EPROTO;
	}
	if (fres->rigstatus == RIG_OK)
		*tx_freq = freq_x2t(&fres->freq_res_u.freq);

	return fres->rigstatus;
}

static int rpcrig_set_split_mode(RIG *rig, vfo_t vfo, rmode_t tx_mode, pbwidth_t tx_width)
{
	struct rpcrig_priv_data *priv;
	int *result;
	mode_arg marg;

	priv = (struct rpcrig_priv_data*)rig->state.priv;

	marg.vfo = vfo;
	mode_t2s(tx_mode, tx_width, &marg.mw);
	result = setsplitmode_1(&marg, priv->cl);
	if (result == NULL) {
		clnt_perror(priv->cl, "setsplitmode_1");
		return -RIG_EPROTO;
	}

	return *result;
}

static int rpcrig_get_split_mode(RIG *rig, vfo_t vfo, rmode_t *tx_mode, pbwidth_t *tx_width)
{
	struct rpcrig_priv_data *priv;
	mode_res *mres;
	vfo_x v;

	priv = (struct rpcrig_priv_data*)rig->state.priv;

	v = vfo;
	mres = getsplitmode_1(&v, priv->cl);
	if (mres == NULL) {
		clnt_perror(priv->cl, "getsplitmode_1");
		return -RIG_EPROTO;
	}
	if (mres->rigstatus == RIG_OK)
		mode_s2t(&mres->mode_res_u.mw, tx_mode, tx_width);

	return mres->rigstatus;
}

static int rpcrig_set_split(RIG *rig, vfo_t vfo, split_t split)
{
	struct rpcrig_priv_data *priv;
	int *result;
	split_arg sarg;

	priv = (struct rpcrig_priv_data*)rig->state.priv;

	sarg.vfo = vfo;
	sarg.split = split;
	result = setsplit_1(&sarg, priv->cl);
	if (result == NULL) {
		clnt_perror(priv->cl, "setsplit_1");
		return -RIG_EPROTO;
	}

	return *result;
}


static int rpcrig_get_split(RIG *rig, vfo_t vfo, split_t *split)
{
	struct rpcrig_priv_data *priv;
	split_res *sres;
	vfo_x v;

	priv = (struct rpcrig_priv_data*)rig->state.priv;

	v = vfo;
	sres = getsplit_1(&v, priv->cl);
	if (sres == NULL) {
		clnt_perror(priv->cl, "getsplit_1");
		return -RIG_EPROTO;
	}
	if (sres->rigstatus == RIG_OK)
		*split = sres->split_res_u.split;

	return sres->rigstatus;
}


static int rpcrig_set_ptt(RIG *rig, vfo_t vfo, ptt_t ptt)
{
	struct rpcrig_priv_data *priv;
	int *result;
	ptt_arg arg;

	priv = (struct rpcrig_priv_data*)rig->state.priv;

	arg.vfo = vfo;
	arg.ptt = ptt;
	result = setptt_1(&arg, priv->cl);
	if (result == NULL) {
		clnt_perror(priv->cl, "setptt_1");
		return -RIG_EPROTO;
	}

	return *result;
}


static int rpcrig_get_ptt(RIG *rig, vfo_t vfo, ptt_t *ptt)
{
	struct rpcrig_priv_data *priv;
	ptt_res *res;
	vfo_x v;

	priv = (struct rpcrig_priv_data*)rig->state.priv;

	v = vfo;
	res = getptt_1(&v, priv->cl);
	if (res == NULL) {
		clnt_perror(priv->cl, "getptt_1");
		return -RIG_EPROTO;
	}
	if (res->rigstatus == RIG_OK)
		*ptt = res->ptt_res_u.ptt;

	return res->rigstatus;
}


static int rpcrig_get_dcd(RIG *rig, vfo_t vfo, dcd_t *dcd)
{
	struct rpcrig_priv_data *priv;
	dcd_res *res;
	vfo_x v;

	priv = (struct rpcrig_priv_data*)rig->state.priv;

	v = vfo;
	res = getdcd_1(&v, priv->cl);
	if (res == NULL) {
		clnt_perror(priv->cl, "getdcd_1");
		return -RIG_EPROTO;
	}
	if (res->rigstatus == RIG_OK)
		*dcd = res->dcd_res_u.dcd;

	return res->rigstatus;
}


static int rpcrig_set_rptr_shift(RIG *rig, vfo_t vfo, rptr_shift_t rptr_shift)
{
  rig_debug(RIG_DEBUG_VERBOSE,"%s called\n", __FUNCTION__);

  return -RIG_ENIMPL;
}


static int rpcrig_get_rptr_shift(RIG *rig, vfo_t vfo, rptr_shift_t *rptr_shift)
{
  rig_debug(RIG_DEBUG_VERBOSE,"%s called\n", __FUNCTION__);

  return -RIG_ENIMPL;
}


static int rpcrig_set_rptr_offs(RIG *rig, vfo_t vfo, shortfreq_t rptr_offs)
{
  rig_debug(RIG_DEBUG_VERBOSE,"%s called\n", __FUNCTION__);

  return -RIG_ENIMPL;
}


static int rpcrig_get_rptr_offs(RIG *rig, vfo_t vfo, shortfreq_t *rptr_offs)
{
  rig_debug(RIG_DEBUG_VERBOSE,"%s called\n", __FUNCTION__);

  return -RIG_ENIMPL;
}


static int rpcrig_set_ctcss_tone(RIG *rig, vfo_t vfo, tone_t tone)
{
  rig_debug(RIG_DEBUG_VERBOSE,"%s called\n", __FUNCTION__);

  return -RIG_ENIMPL;
}


static int rpcrig_get_ctcss_tone(RIG *rig, vfo_t vfo, tone_t *tone)
{
  rig_debug(RIG_DEBUG_VERBOSE,"%s called\n", __FUNCTION__);

  return -RIG_ENIMPL;
}


static int rpcrig_set_dcs_code(RIG *rig, vfo_t vfo, tone_t code)
{
  rig_debug(RIG_DEBUG_VERBOSE,"%s called\n", __FUNCTION__);

  return -RIG_ENIMPL;
}


static int rpcrig_get_dcs_code(RIG *rig, vfo_t vfo, tone_t *code)
{
  rig_debug(RIG_DEBUG_VERBOSE,"%s called\n", __FUNCTION__);

  return -RIG_ENIMPL;
}


static int rpcrig_set_ctcss_sql(RIG *rig, vfo_t vfo, tone_t tone)
{
  rig_debug(RIG_DEBUG_VERBOSE,"%s called\n", __FUNCTION__);

  return -RIG_ENIMPL;
}


static int rpcrig_get_ctcss_sql(RIG *rig, vfo_t vfo, tone_t *tone)
{
  rig_debug(RIG_DEBUG_VERBOSE,"%s called\n", __FUNCTION__);

  return -RIG_ENIMPL;
}


static int rpcrig_set_dcs_sql(RIG *rig, vfo_t vfo, unsigned int code)
{
  rig_debug(RIG_DEBUG_VERBOSE,"%s called\n", __FUNCTION__);

  return -RIG_ENIMPL;
}


static int rpcrig_get_dcs_sql(RIG *rig, vfo_t vfo, unsigned int *code)
{
  rig_debug(RIG_DEBUG_VERBOSE,"%s called\n", __FUNCTION__);

  return -RIG_ENIMPL;
}


static int rpcrig_set_rit(RIG *rig, vfo_t vfo, shortfreq_t rit)
{
  rig_debug(RIG_DEBUG_VERBOSE,"%s called\n", __FUNCTION__);

  return -RIG_ENIMPL;
}


static int rpcrig_get_rit(RIG *rig, vfo_t vfo, shortfreq_t *rit)
{
  rig_debug(RIG_DEBUG_VERBOSE,"%s called\n", __FUNCTION__);

  return -RIG_ENIMPL;
}


static int rpcrig_set_xit(RIG *rig, vfo_t vfo, shortfreq_t xit)
{
  rig_debug(RIG_DEBUG_VERBOSE,"%s called\n", __FUNCTION__);

  return -RIG_ENIMPL;
}


static int rpcrig_get_xit(RIG *rig, vfo_t vfo, shortfreq_t *xit)
{
  rig_debug(RIG_DEBUG_VERBOSE,"%s called\n", __FUNCTION__);

  return -RIG_ENIMPL;
}


static int rpcrig_set_ts(RIG *rig, vfo_t vfo, shortfreq_t ts)
{
  rig_debug(RIG_DEBUG_VERBOSE,"%s called\n", __FUNCTION__);

  return -RIG_ENIMPL;
}


static int rpcrig_get_ts(RIG *rig, vfo_t vfo, shortfreq_t *ts)
{
  rig_debug(RIG_DEBUG_VERBOSE,"%s called\n", __FUNCTION__);

  return -RIG_ENIMPL;
}


static int rpcrig_set_func(RIG *rig, vfo_t vfo, setting_t func, int status)
{
	struct rpcrig_priv_data *priv;
	int *result;
	setting_arg arg;

	priv = (struct rpcrig_priv_data*)rig->state.priv;

	arg.vfo = vfo;
	setting_t2x(func, &arg.setting);
	arg.val.i = status;
	result = setfunc_1(&arg, priv->cl);
	if (result == NULL) {
		clnt_perror(priv->cl, "setfunc_1");
		return -RIG_EPROTO;
	}

	return *result;
}


static int rpcrig_get_func(RIG *rig, vfo_t vfo, setting_t func, int *status)
{
	struct rpcrig_priv_data *priv;
	val_res *res;
	setting_arg arg;

	priv = (struct rpcrig_priv_data*)rig->state.priv;

	arg.vfo = vfo;
	setting_t2x(func, &arg.setting);
	arg.val.i = *status;
	res = getfunc_1(&arg, priv->cl);
	if (res == NULL) {
		clnt_perror(priv->cl, "getfunc_1");
		return -RIG_EPROTO;
	}
	if (res->rigstatus == RIG_OK)
			*status = res->val_res_u.val.i;

	return res->rigstatus;
}


static int rpcrig_set_level(RIG *rig, vfo_t vfo, setting_t level, value_t val)
{
	struct rpcrig_priv_data *priv;
	int *result;
	setting_arg arg;

	priv = (struct rpcrig_priv_data*)rig->state.priv;

	arg.vfo = vfo;
	setting_t2x(level, &arg.setting);
	if (RIG_LEVEL_IS_FLOAT(level))
			arg.val.f = val.f;
	else
			arg.val.i = val.i;
	result = setlevel_1(&arg, priv->cl);
	if (result == NULL) {
		clnt_perror(priv->cl, "setlevel_1");
		return -RIG_EPROTO;
	}

	return *result;
}


static int rpcrig_get_level(RIG *rig, vfo_t vfo, setting_t level, value_t *val)
{
	struct rpcrig_priv_data *priv;
	val_res *res;
	setting_arg arg;

	priv = (struct rpcrig_priv_data*)rig->state.priv;

	arg.vfo = vfo;
	setting_t2x(level, &arg.setting);
	if (RIG_LEVEL_IS_FLOAT(level))
			arg.val.f = val->f;
	else
			arg.val.i = val->i;
	res = getlevel_1(&arg, priv->cl);
	if (res == NULL) {
		clnt_perror(priv->cl, "getlevel_1");
		return -RIG_EPROTO;
	}
	if (res->rigstatus == RIG_OK) {
			if (RIG_LEVEL_IS_FLOAT(level))
				val->f = res->val_res_u.val.f;
			else
				val->i = res->val_res_u.val.i;
	}

	return res->rigstatus;
}


static int rpcrig_set_parm(RIG *rig, setting_t parm, value_t val)
{
	struct rpcrig_priv_data *priv;
	int *result;
	setting_arg arg;

	priv = (struct rpcrig_priv_data*)rig->state.priv;

	setting_t2x(parm, &arg.setting);
	if (RIG_LEVEL_IS_FLOAT(parm))
			arg.val.f = val.f;
	else
			arg.val.i = val.i;
	result = setparm_1(&arg, priv->cl);
	if (result == NULL) {
		clnt_perror(priv->cl, "setparm_1");
		return -RIG_EPROTO;
	}

	return *result;
}


static int rpcrig_get_parm(RIG *rig, setting_t parm, value_t *val)
{
	struct rpcrig_priv_data *priv;
	val_res *res;
	setting_arg arg;

	priv = (struct rpcrig_priv_data*)rig->state.priv;

	setting_t2x(parm, &arg.setting);
	if (RIG_LEVEL_IS_FLOAT(parm))
			arg.val.f = val->f;
	else
			arg.val.i = val->i;
	res = getparm_1(&arg, priv->cl);
	if (res == NULL) {
		clnt_perror(priv->cl, "getparm_1");
		return -RIG_EPROTO;
	}
	if (res->rigstatus == RIG_OK) {
			if (RIG_LEVEL_IS_FLOAT(parm))
				val->f = res->val_res_u.val.f;
			else
				val->i = res->val_res_u.val.i;
	}

	return res->rigstatus;
}

static int rpcrig_vfo_op(RIG *rig, vfo_t vfo, vfo_op_t op)
{
	struct rpcrig_priv_data *priv;
	int *result;
	vfo_op_arg arg;

	priv = (struct rpcrig_priv_data*)rig->state.priv;

	arg.vfo = vfo;
	arg.op = op;
	result = vfoop_1(&arg, priv->cl);
	if (result == NULL) {
		clnt_perror(priv->cl, "vfoop_1");
		return -RIG_EPROTO;
	}

	return *result;
}

static int rpcrig_set_powerstat(RIG *rig, powerstat_t status)
{
  rig_debug(RIG_DEBUG_VERBOSE,"%s called\n", __FUNCTION__);

  return -RIG_ENIMPL;
}


static int rpcrig_get_powerstat(RIG *rig, powerstat_t *status)
{
  rig_debug(RIG_DEBUG_VERBOSE,"%s called\n", __FUNCTION__);

  return -RIG_ENIMPL;
}


static int rpcrig_set_ant(RIG *rig, vfo_t vfo, ant_t ant)
{
  rig_debug(RIG_DEBUG_VERBOSE,"%s called\n", __FUNCTION__);

  return -RIG_ENIMPL;
}


static int rpcrig_get_ant(RIG *rig, vfo_t vfo, ant_t *ant)
{
  rig_debug(RIG_DEBUG_VERBOSE,"%s called\n", __FUNCTION__);

  return -RIG_ENIMPL;
}


static int rpcrig_set_bank(RIG *rig, vfo_t vfo, int bank)
{
  rig_debug(RIG_DEBUG_VERBOSE,"%s called\n", __FUNCTION__);

  return -RIG_ENIMPL;
}


static int rpcrig_set_mem(RIG *rig, vfo_t vfo, int ch)
{
  rig_debug(RIG_DEBUG_VERBOSE,"%s called\n", __FUNCTION__);

  return -RIG_ENIMPL;
}


static int rpcrig_get_mem(RIG *rig, vfo_t vfo, int *ch)
{
  rig_debug(RIG_DEBUG_VERBOSE,"%s called\n", __FUNCTION__);

  return -RIG_ENIMPL;
}


static int rpcrig_set_channel(RIG *rig, const channel_t *chan)
{
  rig_debug(RIG_DEBUG_VERBOSE,"%s called\n", __FUNCTION__);

  return -RIG_ENIMPL;
}


static int rpcrig_get_channel(RIG *rig, channel_t *chan)
{
  rig_debug(RIG_DEBUG_VERBOSE,"%s called\n", __FUNCTION__);

  return -RIG_ENIMPL;
}


static int rpcrig_set_trn(RIG *rig, int trn)
{
  rig_debug(RIG_DEBUG_VERBOSE,"%s called\n", __FUNCTION__);

  return -RIG_ENIMPL;
}


static int rpcrig_get_trn(RIG *rig, int *trn)
{
  rig_debug(RIG_DEBUG_VERBOSE,"%s called\n", __FUNCTION__);

  return -RIG_ENIMPL;
}

static const char *rpcrig_get_info(RIG *rig)
{
  rig_debug(RIG_DEBUG_VERBOSE,"%s called\n", __FUNCTION__);

  return "";
}


static int rpcrig_send_dtmf(RIG *rig, vfo_t vfo, const char *digits)
{
  rig_debug(RIG_DEBUG_VERBOSE,"%s called\n", __FUNCTION__);

  return -RIG_ENIMPL;
}

static int rpcrig_recv_dtmf(RIG *rig, vfo_t vfo, char *digits, int *length)
{
  rig_debug(RIG_DEBUG_VERBOSE,"%s called\n", __FUNCTION__);

  return -RIG_ENIMPL;
}

static int rpcrig_send_morse(RIG *rig, vfo_t vfo, const char *msg)
{
  rig_debug(RIG_DEBUG_VERBOSE,"%s called\n", __FUNCTION__);

  return -RIG_ENIMPL;
}



/*
 * Dummy rig capabilities.
 */

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
  has_get_func:  RIG_FUNC_NONE,
  has_set_func:  RIG_FUNC_NONE,
  has_get_level: RIG_LEVEL_NONE,
  has_set_level: RIG_LEVEL_NONE,
  has_get_parm:	 RIG_PARM_NONE,
  has_set_parm:	 RIG_PARM_NONE,
  ctcss_list:	 NULL,
  dcs_list:  	 NULL,
  chan_list:	 { RIG_CHAN_END, },
  transceive:    RIG_TRN_OFF,
  attenuator:    { RIG_DBLST_END, },
  rx_range_list2: { RIG_FRNG_END, },
  tx_range_list2: { RIG_FRNG_END, },
  tuning_steps: { RIG_TS_END, },
  priv: NULL,

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

int initrigs_rpcrig(void *be_handle)
{
	rig_debug(RIG_DEBUG_VERBOSE, "rpcrig: _init called\n");

	rig_register(&rpcrig_caps);

	return RIG_OK;
}


