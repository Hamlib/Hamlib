/*
 *  Hamlib bindings - Rig interface
 *  Copyright (c) 2001,2002 by Stephane Fillod
 *
 *	$Id: rig.swg,v 1.4 2002-10-08 21:54:58 fillods Exp $
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

%inline %{

typedef struct Rig {
	RIG *rig;
	struct rig_caps *caps;		/* shortcut to RIG->caps */
	struct rig_state *state;	/* shortcut to RIG->state */
	int error_status;
	int do_exception;
} Rig;

%}

#define MAX_STATIC 256

%extend channel {
	channel(vfo_t vfo = RIG_VFO_CURR, int ch = 0) {
		channel_t *chan;

		chan = (channel_t*)malloc(sizeof(channel_t));
		if (!chan)
			return NULL;
		memset(chan, 0, sizeof(channel_t));
		chan->vfo = vfo;
		chan->channel_num = ch;
		return chan;
	}
	~channel (void) {
		/* TODO: free up ext_levels */
		free(self);
	}
}


/*
 * decalre wrapper method with one argument besides RIG* and optional no target vfo
 */
#define METHOD1(f, t1) void f (t1 _##t1  _VFO_DECL) \
				{ self->error_status = rig_##f(self->rig  _VFO_ARG, _##t1); }
#define METHOD2(f, t1, t2) void f (t1 _##t1##_1, t2 _##t2##_2  _VFO_DECL) \
				{ self->error_status = rig_##f(self->rig  _VFO_ARG, _##t1##_1, _##t2##_2); }

/*
 * decalre wrapper method with one output argument besides RIG* (no target vfo)
 */
#define METHOD1GET(f, t1) t1 f (void) \
				{ t1 _##t1; self->error_status = rig_##f(self->rig, &_##t1); return _##t1; }

/*
 * decalre wrapper method with one output argument besides RIG* and vfo
 */
#define METHOD1VGET(f, t1) t1 f (vfo_t vfo = RIG_VFO_CURR) \
				{ t1 _##t1; self->error_status = rig_##f(self->rig, vfo, &_##t1); return _##t1; }


#define METHODSIMPLESET(f, t1, fld, chk) void set_##f (setting_t stg, t1 fld  _VFO_DECL) \
		{ value_t val; 	if (chk (stg)) {	\
			self->error_status = -RIG_EINVAL;	/* invalid type */	\
			return;	\
		}	\
		val.fld = fld;	\
		self->error_status = rig_set_##f(self->rig  _VFO_ARG , stg, val);	\
	}

/*
 * RIG_CONF_ extparm's type:
 *   NUMERIC: val.f
 *   COMBO: val.i, starting from 0
 *   STRING: val.s
 *   CHECKBUTTON: val.i 0/1
 */

#define METHODSUPERSET(f, t1, fld, chk) void set_##f (const char *name, t1 fld  _VFO_DECL) \
		{ setting_t stg; value_t val; 	\
		stg = parse_##f(name);	\
		if (!rig_has_set_##f(self->rig, stg)) {	\
			const struct confparams *cfp;	\
			cfp = rig_ext_lookup(self->rig, name);	\
			if (!cfp) {	\
				self->error_status = -RIG_EINVAL;	/* no such parameter */ \
				return;	\
			} \
			switch (cfp->type) {	\
			case RIG_CONF_NUMERIC:	\
				val.fld = fld;	\
				break;	\
			case RIG_CONF_CHECKBUTTON:	\
			case RIG_CONF_COMBO:		\
				val.i = (int)fld;	\
				break;	\
			case RIG_CONF_STRING:		\
				self->error_status = -RIG_EINVAL;	/* invalid type */	\
				return;	\
			default:	\
				self->error_status = -RIG_ECONF;	\
				return;	\
			}	\
			self->error_status = rig_set_ext_##f(self->rig  _VFO_ARG, cfp->token, val);	\
			return;	\
		}	\
		if (chk (stg)) {	\
			self->error_status = -RIG_EINVAL;	/* invalid type */	\
			return;	\
		}	\
		val.fld = fld;	\
		self->error_status = rig_set_##f(self->rig  _VFO_ARG , stg, val);	\
	}

#define METHODSTRSET(f) void set_##f (const char *name, const char *s  _VFO_DECL) \
		{ value_t val; /* only ext_level/parm's can have string values */  \
		const struct confparams *cfp; \
		cfp = rig_ext_lookup(self->rig, name); \
		if (!cfp) {	\
			self->error_status = -RIG_EINVAL;	/* no such parameter */ \
			return; \
		} \
		if (cfp->type != RIG_CONF_STRING) { \
			self->error_status = -RIG_EINVAL;	/* invalid type */ \
			return; \
		} \
		val.cs = s; \
		self->error_status = rig_set_ext_##f(self->rig _VFO_ARG, cfp->token, val); \
	}

#define METHODSIMPLEGET(f, t1, fld, chk) t1 get_##f##_##fld (setting_t stg  _VFO_DECL) \
		{ value_t val; 	if (chk (stg)) {	\
			self->error_status = -RIG_EINVAL;	/* invalid type */	\
			return val.fld;		/* undefined value */	\
		}	\
		self->error_status = rig_get_##f(self->rig  _VFO_ARG , stg, &val);	\
		return val.fld;	\
	}



#define METHODSUPERGET(fct, t1, fld, chk) t1 get_##fct##_##fld(const char *name  _VFO_DECL) \
		{ setting_t stg; value_t val; 	\
		stg = parse_##fct(name);	\
		if (!rig_has_get_##fct(self->rig, stg)) {	\
			const struct confparams *cfp;	\
			cfp = rig_ext_lookup(self->rig, name);	\
			if (!cfp) {	\
				self->error_status = -RIG_EINVAL;	/* no such parameter */ \
				return val.fld;		/* undefined */ \
			} \
			if (cfp->type == RIG_CONF_STRING) { \
				self->error_status = -RIG_EINVAL;	\
				return val.fld;	/* undefined */ \
			}	\
			self->error_status = rig_get_ext_##fct(self->rig  _VFO_ARG, cfp->token, &val);	\
			switch (cfp->type) {	\
			case RIG_CONF_NUMERIC:	\
				return (t1)val.f;	\
			case RIG_CONF_CHECKBUTTON:	\
			case RIG_CONF_COMBO:		\
				return (t1)val.i;	\
			default:	\
				self->error_status = -RIG_ECONF;	\
				return val.fld;	/* undefined */ \
			}	\
		}	\
		if (chk (stg)) {	\
			self->error_status = -RIG_EINVAL;	/* invalid type */	\
			return val.fld;	/* undefined */ \
		}	\
		self->error_status = rig_get_##fct(self->rig  _VFO_ARG , stg, &val);	\
		return val.fld;	\
	}

#define METHODSTRGET(f) const char * get_##f (const char *name  _VFO_DECL) \
		{ static char s[MAX_STATIC]=""; value_t val; /* only ext_level/parm's can have string values */ \
		const struct confparams *cfp; \
		cfp = rig_ext_lookup(self->rig, name); \
		if (!cfp) {	\
			self->error_status = -RIG_EINVAL;	/* no such parameter */ \
			return ""; \
		} \
		if (cfp->type != RIG_CONF_STRING) { \
			self->error_status = -RIG_EINVAL;	/* invalid type */ \
			return ""; \
		} \
		val.s = s; \
		self->error_status = rig_get_ext_##f(self->rig _VFO_ARG, cfp->token, &val); \
		return val.s; \
	}


/*
 * Rig class alike
 */
%extend Rig {
	Rig(rig_model_t rig_model) {
		Rig *r;

		r = (Rig*)malloc(sizeof(Rig));
		if (!r)
			return NULL;
		r->rig = rig_init(rig_model);
		if (!r->rig) {
			free(r);
			return NULL;
		}
		/* install shortcuts */
		r->caps = r->rig->caps;
		r->state = &r->rig->state;
		r->do_exception = 0;	/* default is disabled */
		r->error_status = RIG_OK;
		return r;
	}
	~Rig () {
		rig_cleanup(self->rig);
		free(self);
	}

/*
 * return code checking
 */
%exception {
	arg1->error_status = RIG_OK;
	$action
	if (arg1->error_status != RIG_OK && arg1->do_exception)
		SWIG_exception(SWIG_UnknownError, rigerror(arg1->error_status));
}

	void open () {
		self->error_status = rig_open(self->rig);
	}
	void close () {
		self->error_status = rig_close(self->rig);
	}

	/*
	 * group all vfo, and non vfo apart
	 */
#define _VFO_ARG	,vfo
#define _VFO_DECL	,vfo_t vfo = RIG_VFO_CURR

	METHOD1(set_freq, freq_t)
	METHOD2(set_mode, rmode_t, pbwidth_t)
	METHOD1(set_ptt, ptt_t)
	METHOD1(set_rptr_shift, rptr_shift_t)
	METHOD1(set_rptr_offs, shortfreq_t)
	METHOD1(set_ctcss_tone, tone_t)
	METHOD1(set_dcs_code, tone_t)
	METHOD1(set_ctcss_sql, tone_t)
	METHOD1(set_dcs_sql, tone_t)
	METHOD1(set_split_freq, freq_t)
	METHOD2(set_split_mode, rmode_t, pbwidth_t)
	METHOD1(set_split, split_t)
	METHOD1(set_rit, shortfreq_t)
	METHOD1(set_xit, shortfreq_t)
	METHOD1(set_ts, shortfreq_t)
	METHOD1(set_ant, ant_t)
	METHOD2(set_func, setting_t, int)
	METHOD1(set_bank, int)
	METHOD1(set_mem, int)
	METHOD1(send_dtmf, const_char_string)
	METHOD1(send_morse, const_char_string)
	METHOD1(vfo_op, vfo_op_t)
	METHOD2(scan, scan_t, int)
	METHODSIMPLESET(level, int, i, RIG_LEVEL_IS_FLOAT)
	METHODSIMPLESET(level, float, f, !RIG_LEVEL_IS_FLOAT)
	METHODSUPERSET(level, int, i, RIG_LEVEL_IS_FLOAT)
	METHODSUPERSET(level, float, f, !RIG_LEVEL_IS_FLOAT)
	METHODSTRSET(level)
	METHOD2(set_ext_level, token_t, value_t)

	METHODSIMPLEGET(level, int, i, RIG_LEVEL_IS_FLOAT)
	METHODSIMPLEGET(level, float, f, !RIG_LEVEL_IS_FLOAT)
	METHODSUPERGET(level, int, i, RIG_LEVEL_IS_FLOAT)
	METHODSUPERGET(level, float, f, !RIG_LEVEL_IS_FLOAT)
	METHODSTRGET(level)

	/*
	 * these ones do not take a vfo arg
	 */
#undef _VFO_ARG
#undef _VFO_DECL
#define _VFO_ARG
#define _VFO_DECL
	METHOD1(set_vfo, vfo_t)		/* particular case */
	METHOD1(set_powerstat, powerstat_t)
	METHOD1(set_trn, int)
	METHOD1(has_set_level, setting_t)
	METHOD1(has_set_parm, setting_t)
	METHOD1(has_set_func, setting_t)
	METHOD1(reset, reset_t)
	METHOD1(has_scan, scan_t)
	METHOD1(has_vfo_op, vfo_op_t)
	METHOD1(passband_normal, rmode_t)
	METHOD1(passband_narrow, rmode_t)
	METHOD1(passband_wide, rmode_t)

	METHOD1(ext_token_lookup, const_char_string)	/* level & parm */
	METHOD1(token_lookup, const_char_string)	/* conf */

	METHOD2(set_conf, token_t, const_char_string)
	METHOD2(set_ext_parm, token_t, value_t)

	METHODSIMPLESET(parm, int, i, RIG_PARM_IS_FLOAT)
	METHODSIMPLESET(parm, float, f, !RIG_PARM_IS_FLOAT)
	METHODSUPERSET(parm, int, i, RIG_PARM_IS_FLOAT)
	METHODSUPERSET(parm, float, f, !RIG_PARM_IS_FLOAT)
	METHODSTRSET(parm)

	METHODSIMPLEGET(parm, int, i, RIG_PARM_IS_FLOAT)
	METHODSIMPLEGET(parm, float, f, !RIG_PARM_IS_FLOAT)
	METHODSUPERGET(parm, int, i, RIG_PARM_IS_FLOAT)
	METHODSUPERGET(parm, float, f, !RIG_PARM_IS_FLOAT)
	METHODSTRGET(parm)


	/* TODO: get rid of value_t, use dynamic casting */
	void set_conf(const char *name, const char *val) {
		token_t tok = rig_token_lookup(self->rig, name);
		if (tok == RIG_CONF_END)
			self->error_status = -RIG_EINVAL;
		else
			self->error_status = rig_set_conf(self->rig, tok, val);
	}



	/* TODO: get_ext_parm_list, level, conf, .. */

	/* get functions */

	METHOD1VGET(get_freq, freq_t)
	extern void get_mode(rmode_t *OUTPUT, pbwidth_t *OUTPUT, vfo_t vfo = RIG_VFO_CURR);
	extern void get_split_mode(rmode_t *OUTPUT, pbwidth_t *OUTPUT, vfo_t vfo = RIG_VFO_CURR);
	METHOD1GET(get_vfo, vfo_t)
	METHOD1VGET(get_ptt, ptt_t)
	METHOD1VGET(get_rptr_shift, rptr_shift_t)
	METHOD1VGET(get_rptr_offs, shortfreq_t)
	METHOD1VGET(get_ctcss_tone, tone_t)
	METHOD1VGET(get_dcs_code, tone_t)
	METHOD1VGET(get_ctcss_sql, tone_t)
	METHOD1VGET(get_dcs_sql, tone_t)
	METHOD1VGET(get_split_freq, freq_t)
	METHOD1VGET(get_split, split_t)
	METHOD1VGET(get_rit, shortfreq_t)
	METHOD1VGET(get_xit, shortfreq_t)
	METHOD1VGET(get_ts, shortfreq_t)
	METHOD1VGET(get_ant, ant_t)
	METHOD1VGET(get_mem, int)
	METHOD1GET(get_powerstat, powerstat_t)
	METHOD1GET(get_trn, int)


	//METHOD2GET(get_ext_parm, token_t, value_t)


	void set_channel(const struct channel *chan) {
		self->error_status = rig_set_channel(self->rig, chan);
	}

	void get_channel(struct channel *chan) {
		self->error_status = rig_get_channel(self->rig, chan);
		/* TODO: handle ext_level's */
	}

	/* get_channel() returns current VFO data
	 * get_channel(10) returns content of memory #10
	 * get_channel(0, RIG_VFO_A) returns VFO A data
	 */
	struct channel *get_channel(int channel_num = INT_MAX, vfo_t vfo = RIG_VFO_MEM) {
		channel_t *chan;
		chan = new_channel(channel_num, channel_num != INT_MAX ? vfo : RIG_VFO_CURR);
		self->error_status = rig_get_channel(self->rig, chan);
		/* TODO: copy ext_level's */
		return chan;
	}

	const char *get_conf(token_t tok) {
		static char s[MAX_STATIC] = "";
		self->error_status = rig_get_conf(self->rig, tok, s);
		return s;
	}

	const char *get_conf(const char *name) {
		token_t tok = rig_token_lookup(self->rig, name);
		static char s[MAX_STATIC] = "";
		if (tok == RIG_CONF_END)
			self->error_status = -RIG_EINVAL;
		else
			self->error_status = rig_get_conf(self->rig, tok, s);
		return s;
	}

	const char *recv_dtmf(vfo_t vfo = RIG_VFO_CURR) {
		static char s[64];
		int len = sizeof(s)-1;
		self->error_status = rig_recv_dtmf(self->rig, vfo, s, &len);
		s[len] = '\0';
		return s;
	}
	const char * get_info(void) {
		const char *s;
		s = rig_get_info(self->rig);
		self->error_status = s ? RIG_OK : -RIG_EINVAL;
		return s;
	}

	int get_func(setting_t func, vfo_t vfo = RIG_VFO_CURR) { 
		int status;
		self->error_status = rig_get_func(self->rig, vfo, func, &status);
		return status;
	}



#ifndef SWIGJAVA
	void get_level(setting_t level, vfo_t vfo = RIG_VFO_CURR)
		{ value_t val; self->error_status = rig_get_level(self->rig, vfo, level, &val); 
			//if (RIG_LEVEL_IS_FLOAT(level))
			/* TODO: dynamic casting */
		}
	/* TODO also: get_parm */
#endif
};

%{

/* 
 * these ones returns 2 values, here is a perl example:
 * 	($mode, $width) = $rig->get_mode();
 */
void Rig_get_mode(Rig *self, rmode_t *mode, pbwidth_t *width, vfo_t vfo)
{
	self->error_status = rig_get_mode(self->rig, vfo, mode, width);
}

void Rig_get_split_mode(Rig *self, rmode_t *mode, pbwidth_t *width, vfo_t vfo)
{
	self->error_status = rig_get_mode(self->rig, vfo, mode, width);
}

%}

