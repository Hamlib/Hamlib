/*
 *  Hamlib bindings - Rig interface
 *  Copyright (c) 2001,2002 by Stephane Fillod
 *
 *	$Id: rig.swg,v 1.2 2002-09-16 06:50:13 fillods Exp $
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


/*
 * decalre wrapper method with one argument besides RIG*  (no target vfo)
 */
#define METHOD1(f, t1) void f (t1 _##t1) \
				{ self->error_status = rig_##f(self->rig, _##t1); }
#define METHOD2(f, t1, t2) void f (t1 _##t1##_1, t2 _##t2##_2) \
				{ self->error_status = rig_##f(self->rig, _##t1##_1, _##t2##_2); }
/*
 * decalre wrapper method with one argument besides RIG* and vfo
 */
#define METHOD1V(f, t1) void f (t1 _##t1, vfo_t vfo = RIG_VFO_CURR) \
				{ self->error_status = rig_##f(self->rig, vfo, _##t1); }
/*
 * decalre wrapper method with two arguments besides RIG* and vfo
 */
#define METHOD2V(f, t1, t2) void f (t1 _##t1##_1, t2 _##t2##_2, vfo_t vfo = RIG_VFO_CURR) \
				{ self->error_status = rig_##f(self->rig, vfo, _##t1##_1, _##t2##_2); }

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
	/* set methods */
	METHOD1V(set_freq, freq_t)
	METHOD2V(set_mode, rmode_t, pbwidth_t)
	METHOD1(set_vfo, vfo_t)
	METHOD1V(set_ptt, ptt_t)
	METHOD1V(set_rptr_shift, rptr_shift_t)
	METHOD1V(set_rptr_offs, shortfreq_t)
	METHOD1V(set_ctcss_tone, tone_t)
	METHOD1V(set_dcs_code, tone_t)
	METHOD1V(set_ctcss_sql, tone_t)
	METHOD1V(set_dcs_sql, tone_t)
	METHOD1V(set_split_freq, freq_t)
	METHOD2V(set_split_mode, rmode_t, pbwidth_t)
	METHOD1V(set_split, split_t)
	METHOD1V(set_rit, shortfreq_t)
	METHOD1V(set_xit, shortfreq_t)
	METHOD1V(set_ts, shortfreq_t)
	METHOD1V(set_ant, ant_t)
	METHOD2V(set_func, setting_t, int)
	METHOD1V(set_bank, int)
	METHOD1V(set_mem, int)
	METHOD1(set_powerstat, powerstat_t)
	METHOD1(set_channel, const_channel_t_p)
	METHOD1(set_trn, int)
	METHOD1(has_set_level, setting_t)
	METHOD1(has_set_parm, setting_t)
	METHOD1(has_set_func, setting_t)
	METHOD1(reset, reset_t)
	METHOD1V(send_dtmf, const_char_string)
	METHOD1V(send_morse, const_char_string)
	METHOD1V(vfo_op, vfo_op_t)
	METHOD2V(scan, scan_t, int)
	METHOD1(has_scan, scan_t)
	METHOD1(has_vfo_op, vfo_op_t)
	METHOD1(passband_normal, rmode_t)
	METHOD1(passband_narrow, rmode_t)
	METHOD1(passband_wide, rmode_t)

	METHOD1(ext_token_lookup, const_char_string)	/* level & parm */
	METHOD1(token_lookup, const_char_string)	/* conf */

	void set_conf_s(const char *name, const char *val) {
		token_t tok = rig_token_lookup(self->rig, name);
		/* FIXME: token not found */
		self->error_status = rig_set_conf(self->rig, tok, val);
	}
	void set_ext_parm_s(const char *name, value_t val) {
		token_t tok = rig_ext_token_lookup(self->rig, name);
		/* FIXME: token not found */
		self->error_status = rig_set_ext_parm(self->rig, tok, val);
	}
	void set_ext_level_s(const char *name, value_t val, vfo_t vfo = RIG_VFO_CURR) {
		token_t tok = rig_ext_token_lookup(self->rig, name);
		/* FIXME: token not found */
		self->error_status = rig_set_ext_level(self->rig, vfo, tok, val);
	}

	METHOD2(set_conf, token_t, const_char_string)
	METHOD2(set_ext_parm, token_t, value_t)
	METHOD2V(set_ext_level, token_t, value_t)

	/* TODO: get rid of value_t, use dynamic casting */
	METHOD2V(set_level, setting_t, value_t)
	METHOD2(set_parm, setting_t, value_t)

	/* TODO: get_ext_parm_list, level, conf, .. */

	/* TODO: all the get functions */

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
	//METHOD2GET(get_parm, setting_t, value_t)
	//METHOD1GET(get_channel, channel_t)	/* special input/output */
	METHOD1GET(get_trn, int)
	// METHOD1V(recv_dtmf, char_string, int *length)
	//METHOD2GET(get_conf, token_t, const_char_string)
	//METHOD2GET(get_ext_parm, token_t, value_t)
	//METHOD2VGET(get_ext_level, token_t, value_t)
	//METHOD1GET(get_info, const_char_string)

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
#else
	/* TODO */
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
