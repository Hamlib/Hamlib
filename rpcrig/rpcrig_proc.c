/*
 *  Hamlib RPC server - procedures
 *  Copyright (c) 2001-2002 by Stephane Fillod
 *
 *	$Id: rpcrig_proc.c,v 1.6 2002-08-23 20:01:09 fillods Exp $
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


/* without VFO arg */
#define DECLARESET1(f, rig_f, rpc_type)	\
int * f##_1_svc(rpc_type *arg, struct svc_req *svc)		\
{								\
	static int res;						\
								\
	res = rig_f(the_rpc_rig, *arg);				\
								\
	return &res;						\
}

#define DECLAREGET1(f, rig_f, rpc_type, rig_type)	\
rpc_type##_res *f##_1_svc(void *rpc_arg, struct svc_req *svc)	\
{								\
	static rpc_type##_res res;				\
	rig_type arg;						\
								\
	res.rigstatus = rig_f(the_rpc_rig, &arg);		\
	res.rpc_type##_res_u.rpc_type = arg;			\
								\
	return &res;						\
}

/* with VFO arg */
#define DECLARESETV1(f, rig_f, rpc_type)	\
int *f##_1_svc(rpc_type##_arg *arg, struct svc_req *svc)	\
{								\
	static int res;						\
								\
	res = rig_f(the_rpc_rig, arg->vfo, arg->rpc_type);	\
								\
	return &res;						\
}

#define DECLAREGETV1(f, rig_f, rpc_type, rig_type)	\
rpc_type##_res *f##_1_svc(vfo_x *vfo, struct svc_req *svc)	\
{								\
	static rpc_type##_res res;				\
	rig_type arg;						\
								\
	res.rigstatus = rig_f(the_rpc_rig, *vfo, &arg);		\
	res.rpc_type##_res_u.rpc_type = arg;			\
								\
	return &res;						\
}

model_x *getmodel_1_svc(void *arg, struct svc_req *svc)
{
	static model_x res;

	rig_debug(RIG_DEBUG_VERBOSE,"%s called\n", __FUNCTION__);

	/* free previous result */
	//xdr_free(xdr_model, &res);

	res = the_rpc_rig->caps->rig_model;

	return &res;
}

rigstate_res *getrigstate_1_svc(void *arg, struct svc_req *svc)
{
	static rigstate_res res;
	struct rig_state *rs;

	rig_debug(RIG_DEBUG_VERBOSE,"%s called\n", __FUNCTION__);

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

DECLARESET1(setvfo, rig_set_vfo, vfo_x)

vfo_res *getvfo_1_svc(vfo_x *vfo, struct svc_req *svc)
{
	static vfo_res res;
	vfo_t v;

	v = *vfo;	/* NB: arg vfo is also input argument to get_vfo */
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

DECLARESETV1(setsplit, rig_set_split, split)
DECLAREGETV1(getsplit, rig_get_split, split, split_t)

DECLARESETV1(setptt, rig_set_ptt, ptt)
DECLAREGETV1(getptt, rig_get_ptt, ptt, ptt_t)
DECLAREGETV1(getdcd, rig_get_dcd, dcd, dcd_t)

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
	if (RIG_PARM_IS_FLOAT(setting))
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
	if (RIG_PARM_IS_FLOAT(setting))
		val.f = arg->val.f;
	else
		val.i = arg->val.i;

	res.rigstatus = rig_get_parm(the_rpc_rig, setting, &val);
	if (RIG_PARM_IS_FLOAT(setting))
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
	val.i = arg->val.i;
	res.rigstatus = rig_get_func(the_rpc_rig, arg->vfo, setting, 
					&res.val_res_u.val.i);

	return &res;
}

int *scan_1_svc(scan_arg *arg, struct svc_req *svc)
{
	static int res;

	res = rig_scan(the_rpc_rig, arg->vfo, arg->scan, arg->ch);

	return &res;
}

DECLARESETV1(vfoop, rig_vfo_op, vfo_op)

DECLARESETV1(setrptrshift, rig_set_rptr_shift, rptrshift)
DECLAREGETV1(getrptrshift, rig_get_rptr_shift, rptrshift, rptr_shift_t)
DECLARESETV1(setrptroffs, rig_set_rptr_offs, shortfreq)
DECLAREGETV1(getrptroffs, rig_get_rptr_offs, shortfreq, shortfreq_t)

DECLARESETV1(setctcsstone, rig_set_ctcss_tone, tone)
DECLAREGETV1(getctcsstone, rig_get_ctcss_tone, tone, tone_t)
DECLARESETV1(setctcsssql, rig_set_ctcss_sql, tone)
DECLAREGETV1(getctcsssql, rig_get_ctcss_sql, tone, tone_t)
DECLARESETV1(setdcscode, rig_set_dcs_code, tone)
DECLAREGETV1(getdcscode, rig_get_dcs_code, tone, tone_t)
DECLARESETV1(setdcssql, rig_set_dcs_sql, tone)
DECLAREGETV1(getdcssql, rig_get_dcs_sql, tone, tone_t)

DECLARESETV1(setrit, rig_set_rit, shortfreq)
DECLAREGETV1(getrit, rig_get_rit, shortfreq, shortfreq_t)
DECLARESETV1(setxit, rig_set_xit, shortfreq)
DECLAREGETV1(getxit, rig_get_xit, shortfreq, shortfreq_t)
DECLARESETV1(setts, rig_set_ts, shortfreq)
DECLAREGETV1(getts, rig_get_ts, shortfreq, shortfreq_t)

DECLARESETV1(setant, rig_set_ant, ant)
DECLAREGETV1(getant, rig_get_ant, ant, ant_t)
DECLARESETV1(setmem, rig_set_mem, ch)
DECLAREGETV1(getmem, rig_get_mem, ch, int)
DECLARESETV1(setbank, rig_set_bank, ch)

DECLARESET1(reset, rig_reset, reset_x)
DECLARESET1(setpowerstat, rig_set_powerstat, powerstat_x)
DECLAREGET1(getpowerstat, rig_get_powerstat, powerstat, powerstat_t)

