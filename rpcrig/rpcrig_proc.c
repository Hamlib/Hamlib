/*
 *  Hamlib RPC server - procedures
 *  Copyright (c) 2001 by Stephane Fillod
 *
 *		$Id: rpcrig_proc.c,v 1.4 2001-12-27 21:56:01 fillods Exp $
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
#include <sys/ioctl.h>

#include <rpc/rpc.h>
#include "rpcrig.h"
#include <hamlib/rig.h>

extern RIG *the_rpc_rig;

model_x *getmodel_1_svc(void *arg, struct svc_req *svc)
{
	static model_x res;

	rig_debug(RIG_DEBUG_VERBOSE,__FUNCTION__ " called\n");

	/* free previous result */
	//xdr_free(xdr_model, &res);

	res = the_rpc_rig->caps->rig_model;

	return &res;
}

rigstate_res *getrigstate_1_svc(void *arg, struct svc_req *svc)
{
	static rigstate_res res;
	struct rig_state *rs;

	rig_debug(RIG_DEBUG_VERBOSE,__FUNCTION__ " called\n");

	if (!the_rpc_rig->state.comm_state) {
		res.rigstatus = -RIG_ERJCTED;
		return &res;
	}

	rs = &the_rpc_rig->state;

	res.rigstate_res_u.state.itu_region = rs->itu_region;
	res.rigstate_res_u.state.has_set_func = rs->has_set_func;
	res.rigstate_res_u.state.has_get_func = rs->has_get_func;
	res.rigstate_res_u.state.has_set_level = rs->has_set_level;
	res.rigstate_res_u.state.has_get_level = rs->has_get_level;
	res.rigstate_res_u.state.has_set_parm = rs->has_set_parm;
	res.rigstate_res_u.state.has_get_parm = rs->has_get_parm;

	res.rigstate_res_u.state.max_rit = rs->max_rit;
	res.rigstate_res_u.state.max_xit = rs->max_xit;
	res.rigstate_res_u.state.max_ifshift = rs->max_ifshift;
	res.rigstate_res_u.state.announces = rs->announces;

	memcpy(res.rigstate_res_u.state.preamp, rs->preamp,
					sizeof(int)*MAXDBLSTSIZ);
	memcpy(res.rigstate_res_u.state.attenuator, rs->attenuator,
					sizeof(int)*MAXDBLSTSIZ);

	memcpy(res.rigstate_res_u.state.tuning_steps, rs->tuning_steps,
					sizeof(struct tuning_step_list)*TSLSTSIZ);
	memcpy(res.rigstate_res_u.state.filters, rs->filters, 
					sizeof(struct filter_list)*FLTLSTSIZ);
	memcpy(res.rigstate_res_u.state.chan_list, rs->chan_list, 
					sizeof(chan_t)*CHANLSTSIZ);
	memcpy(res.rigstate_res_u.state.rx_range_list, rs->rx_range_list, 
					sizeof(freq_range_t)*FRQRANGESIZ);
	memcpy(res.rigstate_res_u.state.tx_range_list, rs->tx_range_list, 
					sizeof(freq_range_t)*FRQRANGESIZ);

	res.rigstatus = RIG_OK;

	return &res;
}

int *setfreq_1_svc(freq_arg *farg, struct svc_req *svc)
{
	static int res;

	res = rig_set_freq(the_rpc_rig, farg->vfo, freq_x2t(&farg->freq));

	return &res;
}

freq_res *getfreq_1_svc(vfo_x *vfo, struct svc_req *svc)
{
	static freq_res res;
	freq_t f;

	res.rigstatus = rig_get_freq(the_rpc_rig, *vfo, &f);
	freq_t2x(f, &res.freq_res_u.freq);

	return &res;
}

int *setmode_1_svc(mode_arg *marg, struct svc_req *svc)
{
	static int res;
	rmode_t mode;
	pbwidth_t width;

	mode_s2t(&marg->mw, &mode, &width);
	res = rig_set_mode(the_rpc_rig, marg->vfo, mode, width);

	return &res;
}

mode_res *getmode_1_svc(vfo_x *vfo, struct svc_req *svc)
{
	static mode_res res;
	rmode_t mode;
	pbwidth_t width;

	res.rigstatus = rig_get_mode(the_rpc_rig, *vfo, &mode, &width);
	mode_t2s(mode, width, &res.mode_res_u.mw);

	return &res;
}

int *setvfo_1_svc(vfo_x *varg, struct svc_req *svc)
{
	static int res;

	res = rig_set_vfo(the_rpc_rig, *varg);

	return &res;
}

vfo_res *getvfo_1_svc(vfo_x *vfo, struct svc_req *svc)
{
	static vfo_res res;
	vfo_t v;

	v = *vfo;
	res.rigstatus = rig_get_vfo(the_rpc_rig, &v);
	res.vfo_res_u.vfo = v;

	return &res;
}

int *setsplitfreq_1_svc(freq_arg *farg, struct svc_req *svc)
{
	static int res;

	res = rig_set_split_freq(the_rpc_rig, farg->vfo, freq_x2t(&farg->freq));

	return &res;
}

freq_res *getsplitfreq_1_svc(vfo_x *vfo, struct svc_req *svc)
{
	static freq_res res;
	freq_t f;

	res.rigstatus = rig_get_split_freq(the_rpc_rig, *vfo, &f);
	freq_t2x(f, &res.freq_res_u.freq);

	return &res;
}

int *setsplitmode_1_svc(mode_arg *marg, struct svc_req *svc)
{
	static int res;
	rmode_t mode;
	pbwidth_t width;

	mode_s2t(&marg->mw, &mode, &width);
	res = rig_set_split_mode(the_rpc_rig, marg->vfo, mode, width);

	return &res;
}

mode_res *getsplitmode_1_svc(vfo_x *vfo, struct svc_req *svc)
{
	static mode_res res;
	rmode_t mode;
	pbwidth_t width;

	res.rigstatus = rig_get_split_mode(the_rpc_rig, *vfo, &mode, &width);
	mode_t2s(mode, width, &res.mode_res_u.mw);

	return &res;
}

int *setsplit_1_svc(split_arg *sarg, struct svc_req *svc)
{
	static int res;

	res = rig_set_split(the_rpc_rig, sarg->vfo, sarg->split);

	return &res;
}

split_res *getsplit_1_svc(vfo_x *vfo, struct svc_req *svc)
{
	static split_res res;
	split_t s;

	res.rigstatus = rig_get_split(the_rpc_rig, *vfo, &s);
	res.split_res_u.split = s;

	return &res;
}

int *setptt_1_svc(ptt_arg *arg, struct svc_req *svc)
{
	static int res;

	res = rig_set_ptt(the_rpc_rig, arg->vfo, arg->ptt);

	return &res;
}

ptt_res *getptt_1_svc(vfo_x *vfo, struct svc_req *svc)
{
	static ptt_res res;
	ptt_t p;

	res.rigstatus = rig_get_ptt(the_rpc_rig, *vfo, &p);
	res.ptt_res_u.ptt = p;

	return &res;
}

dcd_res *getdcd_1_svc(vfo_x *vfo, struct svc_req *svc)
{
	static dcd_res res;
	dcd_t d;

	res.rigstatus = rig_get_dcd(the_rpc_rig, *vfo, &d);
	res.dcd_res_u.dcd = d;

	return &res;
}

int *setlevel_1_svc(setting_arg *arg, struct svc_req *svc)
{
	static int res;
	setting_t setting;
	value_t val;

	setting = setting_x2t(&arg->setting);
	if (RIG_LEVEL_IS_FLOAT(setting))
		val.f = arg->val.f;
	else
		val.i = arg->val.i;

	res = rig_set_level(the_rpc_rig, arg->vfo, setting, val);

	return &res;
}

val_res *getlevel_1_svc(setting_arg *arg, struct svc_req *svc)
{
	static val_res res;
	setting_t setting;
	value_t val;

	setting = setting_x2t(&arg->setting);
	if (RIG_LEVEL_IS_FLOAT(setting))
		val.f = arg->val.f;
	else
		val.i = arg->val.i;

	res.rigstatus = rig_get_level(the_rpc_rig, arg->vfo, setting, &val);
	if (RIG_LEVEL_IS_FLOAT(setting))
		res.val_res_u.val.f = val.f;
	else
		res.val_res_u.val.i = val.i;

	return &res;
}

int *setparm_1_svc(setting_arg *arg, struct svc_req *svc)
{
	static int res;
	setting_t setting;
	value_t val;

	setting = setting_x2t(&arg->setting);
	if (RIG_LEVEL_IS_FLOAT(setting))
		val.f = arg->val.f;
	else
		val.i = arg->val.i;

	res = rig_set_parm(the_rpc_rig, setting, val);

	return &res;
}

val_res *getparm_1_svc(setting_arg *arg, struct svc_req *svc)
{
	static val_res res;
	setting_t setting;
	value_t val;

	setting = setting_x2t(&arg->setting);
	if (RIG_LEVEL_IS_FLOAT(setting))
		val.f = arg->val.f;
	else
		val.i = arg->val.i;

	res.rigstatus = rig_get_parm(the_rpc_rig, setting, &val);
	if (RIG_LEVEL_IS_FLOAT(setting))
		res.val_res_u.val.f = val.f;
	else
		res.val_res_u.val.i = val.i;

	return &res;
}

int *setfunc_1_svc(setting_arg *arg, struct svc_req *svc)
{
	static int res;
	setting_t setting;

	setting = setting_x2t(&arg->setting);

	res = rig_set_func(the_rpc_rig, arg->vfo, setting, arg->val.i);

	return &res;
}

val_res *getfunc_1_svc(setting_arg *arg, struct svc_req *svc)
{
	static val_res res;
	setting_t setting;
	value_t val;

	setting = setting_x2t(&arg->setting);
	if (RIG_LEVEL_IS_FLOAT(setting))
		val.f = arg->val.f;
	else
		val.i = arg->val.i;

	res.rigstatus = rig_get_func(the_rpc_rig, arg->vfo, setting, 
					&res.val_res_u.val.i);

	return &res;
}

int *vfoop_1_svc(vfo_op_arg *arg, struct svc_req *svc)
{
	static int res;

	res = rig_vfo_op(the_rpc_rig, arg->vfo, arg->op);

	return &res;
}


