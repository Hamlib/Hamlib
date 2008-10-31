/*
 *  Hamlib RPC backend - main file
 *  Copyright (c) 2001-2008 by Stephane Fillod
 *
 *	$Id: rpcrig_backend.c,v 1.20 2008-10-31 22:14:10 fillods Exp $
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
#include <stdio.h>
#include <string.h>  /* String function definitions */
#include <unistd.h>  /* UNIX standard function definitions */
#include <math.h>
#include <limits.h>
#include <netdb.h>

#include "hamlib/rig.h"
#include "serial.h"
#include "misc.h"
#include "token.h"
#include "register.h"

#include <rpc/rpc.h>
#ifdef HAVE_RPC_RPCENT_H
#include <rpc/rpcent.h>
#endif
#include "rpcrig.h"

#include "rpcrig_backend.h"

#define RIGPROTO "udp"

/*
 * Borrowed from stringify.h
 * Indirect stringification.  Doing two levels allows the parameter to be a
 * macro itself.  For example, compile with -DFOO=bar, __stringify(FOO)
 * converts to "bar".
 */

#define r_stringify_1(x)	#x
#define r_stringify(x)		r_stringify_1(x)

struct rpcrig_priv_data {
		CLIENT *cl;
		unsigned long prognum;
};

#define SETBODY1(f, rpc_type, rig_arg)	\
	struct rpcrig_priv_data *priv;				\
	int *result;						\
	rpc_type##_x arg;						\
								\
	priv = (struct rpcrig_priv_data*)rig->state.priv;	\
								\
	arg = rig_arg;						\
	result = f##_1(&arg, priv->cl);			\
	if (result == NULL) {					\
		clnt_perror(priv->cl, "##f##_1");		\
		return -RIG_EPROTO;				\
	}							\
								\
	return *result;

#define GETBODY1(f, rpc_type, rig_arg)	\
	struct rpcrig_priv_data *priv;				\
	rpc_type##_x arg;					\
	rpc_type##_res *res;					\
								\
	priv = (struct rpcrig_priv_data*)rig->state.priv;	\
								\
	arg = *rig_arg;						\
	res = f##_1(&arg, priv->cl);				\
	if (res == NULL) {					\
		clnt_perror(priv->cl, "##f##_1");		\
		return -RIG_EPROTO;				\
	}							\
	if (res->rigstatus == RIG_OK)				\
		*rig_arg = res->rpc_type##_res_u.rpc_type;	\
								\
	return res->rigstatus;

#define SETBODYVFO1(f, rpc_type, rig_arg)	\
	struct rpcrig_priv_data *priv;				\
	int *result;						\
	rpc_type##_arg arg;					\
								\
	priv = (struct rpcrig_priv_data*)rig->state.priv;	\
								\
	arg.vfo = vfo;						\
	arg.rpc_type = rig_arg;					\
	result = f##_1(&arg, priv->cl);				\
	if (result == NULL) {					\
		clnt_perror(priv->cl, "##f##_1");		\
		return -RIG_EPROTO;				\
	}							\
								\
	return *result;

#define GETBODYVFO1(f, rpc_type, rig_arg)	\
	struct rpcrig_priv_data *priv;				\
	rpc_type##_res *res;					\
	vfo_x v;						\
								\
	priv = (struct rpcrig_priv_data*)rig->state.priv;	\
								\
	v = vfo;						\
	res = f##_1(&v, priv->cl);				\
	if (res == NULL) {					\
		clnt_perror(priv->cl, "##f##_1");		\
		return -RIG_EPROTO;				\
	}							\
	if (res->rigstatus == RIG_OK)				\
		*rig_arg = res->rpc_type##_res_u.rpc_type;	\
								\
	return res->rigstatus;


static unsigned long extract_prognum(const char val[])
{
	if (val[0] == '+') {
		return RIGPROG + atol(val+1);
	} else
		if (val[0] < '0' || val[0] > '9') {
			struct rpcent *ent;
			ent = getrpcbyname (val);
			if (ent)
				return ent->r_number;
			else
				return 0;
	} else
		return atol(val);
}


static int rpcrig_init(RIG *rig)
{
	struct rpcrig_priv_data *priv;
	if (!rig || !rig->caps)
		return -RIG_EINVAL;
	
	rig->state.priv = malloc(sizeof(struct rpcrig_priv_data));
	if (!rig->state.priv) {
		/* whoops! memory shortage! */
		return -RIG_ENOMEM;
	}
	priv = (struct rpcrig_priv_data*)rig->state.priv;

	rig->state.rigport.type.rig = RIG_PORT_RPC;
	rig->state.pttport.type.ptt = RIG_PTT_RIG;
	rig->state.dcdport.type.dcd = RIG_DCD_RIG;
	strcpy(rig->state.rigport.pathname, "localhost");
	priv->prognum = RIGPROG;

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
	char *server, *s;
	int i;

	rs = &rig->state;
	priv = (struct rpcrig_priv_data*)rs->priv;
	server = strdup(rs->rigport.pathname);
	s = strchr(server, ':');
	if (s) {
		unsigned long prognum;
		*s = '\0';
		prognum = extract_prognum(s+1);
		if (prognum == 0) {
			free(server);
			return -RIG_ECONF;
		}
		priv->prognum = prognum;
	}

	priv->cl = clnt_create(server, priv->prognum, RIGVERS, RIGPROTO);
	if (priv->cl == NULL) {
		clnt_pcreateerror(server);
		free(server);
		return -RIG_ECONF;
	}
	mdl_res = getmodel_1(NULL, priv->cl);
	if (mdl_res == NULL) {
		clnt_perror(priv->cl, server);
		clnt_destroy(priv->cl);
		free(server);
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
		free(server);
		priv->cl = NULL;
		return -RIG_EPROTO;
	}
	free(server);
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
	SETBODY1(setvfo, vfo, vfo);
}


static int rpcrig_get_vfo(RIG *rig, vfo_t *vfo)
{
	GETBODY1(getvfo, vfo, vfo);
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

static int rpcrig_set_split_vfo(RIG *rig, vfo_t vfo, split_t split, vfo_t tx_vfo)
{
	struct rpcrig_priv_data *priv;
	int *result;
	split_arg arg;

	priv = (struct rpcrig_priv_data*)rig->state.priv;

	arg.vfo = vfo;
	arg.split = split;
	arg.tx_vfo = tx_vfo;
	result = setsplitvfo_1(&arg, priv->cl);
	if (result == NULL) {
		clnt_perror(priv->cl, "setsplitvfo_1");
		return -RIG_EPROTO;
	}

	return *result;
}


static int rpcrig_get_split_vfo(RIG *rig, vfo_t vfo, split_t *split, vfo_t *tx_vfo)
{
	struct rpcrig_priv_data *priv;
	split_res *res;
	vfo_x v;

	priv = (struct rpcrig_priv_data*)rig->state.priv;

	v = vfo;
	res = getsplitvfo_1(&v, priv->cl);
	if (res == NULL) {
		clnt_perror(priv->cl, "getsplitvfo_1");
		return -RIG_EPROTO;
	}
	if (res->rigstatus == RIG_OK) {
		*split = res->split_res_u.split.split;
		*tx_vfo = res->split_res_u.split.tx_vfo;
	}

	return res->rigstatus;
}


static int rpcrig_set_ptt(RIG *rig, vfo_t vfo, ptt_t ptt)
{
	SETBODYVFO1(setptt, ptt, ptt);
}


static int rpcrig_get_ptt(RIG *rig, vfo_t vfo, ptt_t *ptt)
{
	GETBODYVFO1(getptt, ptt, ptt);
}


static int rpcrig_get_dcd(RIG *rig, vfo_t vfo, dcd_t *dcd)
{
	GETBODYVFO1(getdcd, dcd, dcd);
}


static int rpcrig_set_rptr_shift(RIG *rig, vfo_t vfo, rptr_shift_t rptr_shift)
{
	SETBODYVFO1(setrptrshift, rptrshift, rptr_shift);
}


static int rpcrig_get_rptr_shift(RIG *rig, vfo_t vfo, rptr_shift_t *rptr_shift)
{
	GETBODYVFO1(getrptrshift, rptrshift, rptr_shift);
}


static int rpcrig_set_rptr_offs(RIG *rig, vfo_t vfo, shortfreq_t rptr_offs)
{
	SETBODYVFO1(setrptroffs, shortfreq, rptr_offs);
}


static int rpcrig_get_rptr_offs(RIG *rig, vfo_t vfo, shortfreq_t *rptr_offs)
{
	GETBODYVFO1(getrptroffs, shortfreq, rptr_offs);
}


static int rpcrig_set_ctcss_tone(RIG *rig, vfo_t vfo, tone_t tone)
{
	SETBODYVFO1(setctcsstone, tone, tone);
}


static int rpcrig_get_ctcss_tone(RIG *rig, vfo_t vfo, tone_t *tone)
{
	GETBODYVFO1(getctcsstone, tone, tone);
}


static int rpcrig_set_dcs_code(RIG *rig, vfo_t vfo, tone_t code)
{
	SETBODYVFO1(setdcscode, tone, code);
}


static int rpcrig_get_dcs_code(RIG *rig, vfo_t vfo, tone_t *code)
{
	GETBODYVFO1(getdcscode, tone, code);
}


static int rpcrig_set_ctcss_sql(RIG *rig, vfo_t vfo, tone_t tone)
{
	SETBODYVFO1(setctcsssql, tone, tone);
}


static int rpcrig_get_ctcss_sql(RIG *rig, vfo_t vfo, tone_t *tone)
{
	GETBODYVFO1(getctcsssql, tone, tone);
}


static int rpcrig_set_dcs_sql(RIG *rig, vfo_t vfo, unsigned int code)
{
	SETBODYVFO1(setdcssql, tone, code);
}


static int rpcrig_get_dcs_sql(RIG *rig, vfo_t vfo, unsigned int *code)
{
	GETBODYVFO1(getdcssql, tone, code);
}


static int rpcrig_set_rit(RIG *rig, vfo_t vfo, shortfreq_t rit)
{
	SETBODYVFO1(setrit, shortfreq, rit);
}


static int rpcrig_get_rit(RIG *rig, vfo_t vfo, shortfreq_t *rit)
{
	GETBODYVFO1(getrit, shortfreq, rit);
}


static int rpcrig_set_xit(RIG *rig, vfo_t vfo, shortfreq_t xit)
{
	SETBODYVFO1(setxit, shortfreq, xit);
}


static int rpcrig_get_xit(RIG *rig, vfo_t vfo, shortfreq_t *xit)
{
	GETBODYVFO1(getxit, shortfreq, xit);
}


static int rpcrig_set_ts(RIG *rig, vfo_t vfo, shortfreq_t ts)
{
	SETBODYVFO1(setts, shortfreq, ts);
}


static int rpcrig_get_ts(RIG *rig, vfo_t vfo, shortfreq_t *ts)
{
	GETBODYVFO1(getts, shortfreq, ts);
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
	if (RIG_PARM_IS_FLOAT(parm))
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
			if (RIG_PARM_IS_FLOAT(parm))
				val->f = res->val_res_u.val.f;
			else
				val->i = res->val_res_u.val.i;
	}

	return res->rigstatus;
}

static int rpcrig_scan(RIG *rig, vfo_t vfo, scan_t rscan, int ch)
{
	struct rpcrig_priv_data *priv;
	int *result;
	scan_arg arg;

	priv = (struct rpcrig_priv_data*)rig->state.priv;

	arg.vfo = vfo;
	arg.scan = rscan;
	arg.ch = ch;
	result = scan_1(&arg, priv->cl);
	if (result == NULL) {
		clnt_perror(priv->cl, "scan_1");
		return -RIG_EPROTO;
	}

	return *result;
}

static int rpcrig_vfo_op(RIG *rig, vfo_t vfo, vfo_op_t op)
{
	SETBODYVFO1(vfoop, vfo_op, op);
}

static int rpcrig_set_ant(RIG *rig, vfo_t vfo, ant_t ant)
{
	SETBODYVFO1(setant, ant, ant);
}


static int rpcrig_get_ant(RIG *rig, vfo_t vfo, ant_t *ant)
{
	GETBODYVFO1(getant, ant, ant);
}


static int rpcrig_set_bank(RIG *rig, vfo_t vfo, int bank)
{
	SETBODYVFO1(setbank, ch, bank);
}


static int rpcrig_set_mem(RIG *rig, vfo_t vfo, int ch)
{
	SETBODYVFO1(setmem, ch, ch);
}


static int rpcrig_get_mem(RIG *rig, vfo_t vfo, int *ch)
{
	GETBODYVFO1(getmem, ch, ch);
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


static int rpcrig_reset(RIG *rig, reset_t reset)
{
	SETBODY1(reset, reset, reset);
}

static int rpcrig_set_powerstat(RIG *rig, powerstat_t status)
{
	SETBODY1(setpowerstat, powerstat, status);
}


static int rpcrig_get_powerstat(RIG *rig, powerstat_t *status)
{
	GETBODY1(getpowerstat, powerstat, status);
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



#define TOK_PROGNUM TOKEN_BACKEND(1)

static const struct confparams rpcrig_cfg_params[] = {
	{ TOK_PROGNUM, "prognum", "Program number", "RPC program number",
			r_stringify(RIGPROG), RIG_CONF_NUMERIC, { .n = { 100000, ULONG_MAX, 1 } }
	},
	{ RIG_CONF_END, NULL, }
};

/*
 * Assumes rig!=NULL, rig->state.priv!=NULL
 */
int rpcrig_set_conf(RIG *rig, token_t token, const char *val)
{
	struct rpcrig_priv_data *priv;
	struct rig_state *rs;

	rs = &rig->state;
	priv = (struct rpcrig_priv_data*)rs->priv;

	switch(token) {
	case TOK_PROGNUM:
		{
			unsigned long prognum;
			prognum = extract_prognum(val);
			if (prognum == 0)
				return -RIG_EINVAL;
			priv->prognum = prognum;
			break;
		}
	default:
		return -RIG_EINVAL;
	}
	return RIG_OK;
}

/*
 * assumes rig!=NULL,
 * Assumes rig!=NULL, rig->state.priv!=NULL
 *  and val points to a buffer big enough to hold the conf value.
 */
int rpcrig_get_conf(RIG *rig, token_t token, char *val)
{
	struct rpcrig_priv_data *priv;
	struct rig_state *rs;

	rs = &rig->state;
	priv = (struct rpcrig_priv_data*)rs->priv;

	switch(token) {
	case TOK_PROGNUM:
		sprintf(val, "%ld", priv->prognum);
		break;
	default:
		return -RIG_EINVAL;
	}
	return RIG_OK;
}


/*
 * Dummy rig capabilities.
 */

struct rig_caps rpcrig_caps = {
  .rig_model =      RIG_MODEL_RPC,
  .model_name =     "RPC rig",
  .mfg_name =       "Hamlib",
  .version =        "0.3",
  .copyright = 	 "LGPL",
  .status =         RIG_STATUS_BETA,
  .rig_type =       RIG_TYPE_OTHER,
  .targetable_vfo = 	 0,
  .ptt_type =       RIG_PTT_NONE,
  .dcd_type =       RIG_DCD_NONE,
  .port_type =      RIG_PORT_NONE,
  .has_get_func =   RIG_FUNC_NONE,
  .has_set_func =   RIG_FUNC_NONE,
  .has_get_level =  RIG_LEVEL_NONE,
  .has_set_level =  RIG_LEVEL_NONE,
  .has_get_parm = 	 RIG_PARM_NONE,
  .has_set_parm = 	 RIG_PARM_NONE,
  .ctcss_list = 	 NULL,
  .dcs_list =   	 NULL,
  .chan_list = 	 { RIG_CHAN_END, },
  .transceive =     RIG_TRN_OFF,
  .attenuator =     { RIG_DBLST_END, },
  .rx_range_list2 =  { RIG_FRNG_END, },
  .tx_range_list2 =  { RIG_FRNG_END, },
  .tuning_steps =  { RIG_TS_END, },
  .priv =  NULL,

  .rig_init =     rpcrig_init,
  .rig_cleanup =  rpcrig_cleanup,
  .rig_open =     rpcrig_open,
  .rig_close =    rpcrig_close,

  .cfgparams =	rpcrig_cfg_params,
  .set_conf =	rpcrig_set_conf,
  .get_conf =	rpcrig_get_conf,

  .set_freq =     rpcrig_set_freq,
  .get_freq =     rpcrig_get_freq,
  .set_mode =     rpcrig_set_mode,
  .get_mode =     rpcrig_get_mode,
  .set_vfo =      rpcrig_set_vfo,
  .get_vfo =      rpcrig_get_vfo,
  
  .set_powerstat =  rpcrig_set_powerstat,
  .get_powerstat =  rpcrig_get_powerstat,
  .set_level =     rpcrig_set_level,
  .get_level =     rpcrig_get_level,
  .set_func =      rpcrig_set_func,
  .get_func =      rpcrig_get_func,
  .set_parm =      rpcrig_set_parm,
  .get_parm =      rpcrig_get_parm,

  .get_info =      rpcrig_get_info,


  .set_ptt = 	rpcrig_set_ptt,
  .get_ptt = 	rpcrig_get_ptt,
  .get_dcd = 	rpcrig_get_dcd,
  .set_rptr_shift = 	rpcrig_set_rptr_shift,
  .get_rptr_shift = 	rpcrig_get_rptr_shift,
  .set_rptr_offs = 	rpcrig_set_rptr_offs,
  .get_rptr_offs = 	rpcrig_get_rptr_offs,
  .set_ctcss_tone = 	rpcrig_set_ctcss_tone,
  .get_ctcss_tone = 	rpcrig_get_ctcss_tone,
  .set_dcs_code = 	rpcrig_set_dcs_code,
  .get_dcs_code = 	rpcrig_get_dcs_code,
  .set_ctcss_sql = 	rpcrig_set_ctcss_sql,
  .get_ctcss_sql = 	rpcrig_get_ctcss_sql,
  .set_dcs_sql = 	rpcrig_set_dcs_sql,
  .get_dcs_sql = 	rpcrig_get_dcs_sql,
  .set_split_freq = 	rpcrig_set_split_freq,
  .get_split_freq = 	rpcrig_get_split_freq,
  .set_split_mode = 	rpcrig_set_split_mode,
  .get_split_mode = 	rpcrig_get_split_mode,
  .set_split_vfo = 	rpcrig_set_split_vfo,
  .get_split_vfo = 	rpcrig_get_split_vfo,
  .set_rit = 	rpcrig_set_rit,
  .get_rit = 	rpcrig_get_rit,
  .set_xit = 	rpcrig_set_xit,
  .get_xit = 	rpcrig_get_xit,
  .set_ts = 	rpcrig_set_ts,
  .get_ts = 	rpcrig_get_ts,
  .set_ant = 	rpcrig_set_ant,
  .get_ant = 	rpcrig_get_ant,
  .set_bank = 	rpcrig_set_bank,
  .set_mem = 	rpcrig_set_mem,
  .get_mem = 	rpcrig_get_mem,
  .vfo_op = 	rpcrig_vfo_op,
  .send_dtmf =  rpcrig_send_dtmf,
  .recv_dtmf =  rpcrig_recv_dtmf,
  .send_morse =  rpcrig_send_morse,
  .set_channel = 	rpcrig_set_channel,
  .get_channel = 	rpcrig_get_channel,
  .set_trn = 	rpcrig_set_trn,
  .get_trn = 	rpcrig_get_trn,
  .scan = 	rpcrig_scan,
  .reset = 	rpcrig_reset,
};

DECLARE_INITRIG_BACKEND(rpcrig)
{
	rig_debug(RIG_DEBUG_VERBOSE, "rpcrig: _init called\n");

	rig_register(&rpcrig_caps);

	return RIG_OK;
}

