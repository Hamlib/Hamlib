/*
 *  Hamlib bindings - Rig interface
 *  Copyright (c) 2001-2008 by Stephane Fillod
 *
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
%{
#include <hamlib/rig.h>
%}

%include macros.swg
%immutable rig_caps::model_name;
%immutable rig_caps::mfg_name;
%immutable rig_caps::version;
%immutable rig_caps::copyright;

%include <hamlib/riglist.h>
%include <hamlib/rig.h>
%include <hamlib/rig_state.h>

%apply int *OUTPUT { int *};

%apply unsigned int *OUTPUT { ant_t * };
%apply double *OUTPUT { freq_t * };
%apply shortfreq_t *OUTPUT { pbwidth_t * };
%apply uint64_t *OUTPUT { rmode_t * };
%apply int *OUTPUT { split_t * };
%apply unsigned int *OUTPUT { vfo_t * };

%inline %{

#pragma SWIG nowarn=451

#ifdef SWIGPYTHON
	// This mirrors "struct rig_callbacks" from rig.h using Python types
	typedef struct {
		PyObject *freq_event;     /*!< Frequency change event */
		PyObject *freq_arg;       /*!< Frequency change argument */
		PyObject *mode_event;     /*!< Mode change event */
		PyObject *mode_arg;       /*!< Mode change argument */
		PyObject *vfo_event;      /*!< VFO change event */
		PyObject *vfo_arg;        /*!< VFO change argument */
		PyObject *ptt_event;      /*!< PTT change event */
		PyObject *ptt_arg;        /*!< PTT change argument */
		PyObject *dcd_event;      /*!< DCD change event */
		PyObject *dcd_arg;        /*!< DCD change argument */
		PyObject *pltune_event;   /*!< Pipeline tuning module freq/mode/width callback NOTE: the C struct doesn't have the _event suffix*/
		PyObject *pltune_arg;     /*!< Pipeline tuning argument */
		PyObject *spectrum_event; /*!< Spectrum line reception event */
		PyObject *spectrum_arg;   /*!< Spectrum line reception argument */
		/* etc.. */
	} python_callbacks_t;
#endif

typedef struct Rig {
	RIG *rig;
	struct rig_caps *caps;		/* shortcut to RIG->caps */
	struct rig_state *state;	/* shortcut to RIG->state */
	int error_status;
	int do_exception;
#ifdef SWIGPYTHON
	python_callbacks_t *python_callbacks;
#endif
} Rig;

typedef char * char_string;
typedef channel_t * channel_t_p;
typedef channel_t * const_channel_t_p;

#ifdef SWIGPYTHON
int rig_freq_cb_python(RIG *rig, vfo_t vfo, freq_t freq, rig_ptr_t arg)
{
    Rig *self = arg;
    PyObject *python_arguments;

    python_arguments = PyTuple_Pack(3,
                                    PyLong_FromLong(vfo),
                                    PyFloat_FromDouble(freq),
                                    self->python_callbacks->freq_arg
                                    );

    PyObject_CallObject(self->python_callbacks->freq_event, python_arguments);
    Py_XDECREF(python_arguments);

    return RIG_OK;
}

int rig_mode_cb_python(RIG *rig, vfo_t vfo, rmode_t rmode, pbwidth_t pbwidth, rig_ptr_t arg)
{
    Rig *self = arg;
    PyObject *python_arguments;

    python_arguments = PyTuple_Pack(4,
                                    PyLong_FromLong(vfo),
                                    PyLong_FromLong(rmode),
                                    PyLong_FromLong(pbwidth),
                                    self->python_callbacks->mode_arg
                                    );

    PyObject_CallObject(self->python_callbacks->mode_event, python_arguments);

    Py_XDECREF(python_arguments);

    return RIG_OK;
}

int rig_vfo_cb_python(RIG *rig, vfo_t vfo, rig_ptr_t arg)
{
    Rig *self = arg;
    PyObject *python_arguments;

    python_arguments = PyTuple_Pack(2,
                                    PyLong_FromLong(vfo),
                                    self->python_callbacks->vfo_arg
                                    );

    PyObject_CallObject(self->python_callbacks->vfo_event, python_arguments);

    Py_XDECREF(python_arguments);

    return RIG_OK;
}

int rig_ptt_cb_python(RIG *rig, vfo_t vfo, ptt_t ptt, rig_ptr_t arg)
{
    Rig *self = arg;
    PyObject *python_arguments;

    python_arguments = PyTuple_Pack(3,
                                    PyLong_FromLong(vfo),
                                    PyLong_FromLong(ptt),
                                    self->python_callbacks->ptt_arg
                                    );

    PyObject_CallObject(self->python_callbacks->ptt_event, python_arguments);

    Py_XDECREF(python_arguments);

    return RIG_OK;
}

int rig_dcd_cb_python(RIG *rig, vfo_t vfo, dcd_t dcd, rig_ptr_t arg)
{
    Rig *self = arg;
    PyObject *python_arguments;

    python_arguments = PyTuple_Pack(3,
                                    PyLong_FromLong(vfo),
                                    PyLong_FromLong(dcd),
                                    self->python_callbacks->dcd_arg
                                    );

    PyObject_CallObject(self->python_callbacks->dcd_event, python_arguments);

    Py_XDECREF(python_arguments);

    return RIG_OK;
}

int rig_pltune_cb_python(RIG *rig, vfo_t vfo, freq_t *freq, rmode_t *rmode, pbwidth_t *pbwidth, rig_ptr_t arg)
{
    Rig *self = arg;
    PyObject *python_arguments;

    python_arguments = PyTuple_Pack(1,
                                    self->python_callbacks->pltune_arg
                                    );

    PyObject_CallObject(self->python_callbacks->pltune_event, python_arguments);

    Py_XDECREF(python_arguments);

    return RIG_OK;
}

int rig_spectrum_cb_python(RIG *rig, struct rig_spectrum_line *rig_spectrum_line, rig_ptr_t arg)
{
    Rig *self = arg;
    PyObject *python_arguments;

    python_arguments = PyTuple_Pack(1,
                                    self->python_callbacks->spectrum_arg
                                    );

    PyObject_CallObject(self->python_callbacks->spectrum_event, python_arguments);

    Py_XDECREF(python_arguments);

    return RIG_OK;
}
#endif
%}

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
		/* TODO: free ext_levels */
		free(self);
	}
}

%include "carrays.i"
%array_class(struct channel, channelArray);
%array_class(tone_t, toneArray);

/*
 * declare wrapper method with one argument besides RIG* and optional no target vfo
 */
#define METHOD1(f, t1) void f (t1 _##t1  _VFO_DECL) \
				{ self->error_status = rig_##f(self->rig  _VFO_ARG, _##t1); }
#define METHOD2(f, t1, t2) void f (t1 _##t1##_1, t2 _##t2##_2  _VFO_DECL) \
				{ self->error_status = rig_##f(self->rig  _VFO_ARG, _##t1##_1, _##t2##_2); }
#define METHOD2_INIT(f, t1, t2, i2) void f (t1 _##t1##_1, t2 _##t2##_2 = i2  _VFO_DECL) \
				{ self->error_status = rig_##f(self->rig  _VFO_ARG, _##t1##_1, _##t2##_2); }
#define METHOD3(f, t1) void f ( vfo_t vfo, t1 _##t1) \
				{ self->error_status = rig_##f(self->rig  _VFO_ARG, _##t1); }
#define METHOD3_INIT(f, t1, t2, t3, i3) void f (t1 _##t1##_1, t2 _##t2##_2, t3 _##t3##_3 = i3  _VFO_DECL) \
				{ self->error_status = rig_##f(self->rig  _VFO_ARG, _##t1##_1, _##t2##_2, _##t3##_3); }
#define METHOD4(f, t1) void f ( vfo_t vfo, t1 _##t1) \
				{ self->error_status = rig_##f(self->rig  _VFO_ARG, _##t1); }
#define METHOD4_INIT(f, t1, t2, t3, t4, i4) void f (t1 _##t1##_1, t2 _##t2##_2, t3 _##t3##_3, ##t4##_4 = i4  _VFO_DECL) \
				{ self->error_status = rig_##f(self->rig  _VFO_ARG, _##t1##_1, _##t2##_2, _##t3##_3 _##t4##_4); }
/*
 * declare wrapper method with one output argument besides RIG* (no target vfo)
 */
#define METHOD1GET(f, t1) t1 f (void) \
				{ t1 _##t1; self->error_status = rig_##f(self->rig, &_##t1); return _##t1; }
#define METHOD1SET(f, t1) t1 f (void) \
				{ t1 _##t1; self->error_status = rig_##f(self->rig, _##t1); return _##t1; }

/*
 * declare wrapper method with one output argument besides RIG* and vfo
 */
#define METHOD1VGET(f, t1) t1 f (vfo_t vfo = RIG_VFO_CURR) \
				{ t1 _##t1; self->error_status = rig_##f(self->rig, vfo, &_##t1); return _##t1; }

#ifndef SWIGLUA
#define METHODSIMPLESET(f, t1, fld, chk) void set_##f (setting_t stg, t1 fld  _VFO_DECL) \
		{ value_t val = {0}; 	if (chk) {	\
			self->error_status = -RIG_EINVAL;	/* invalid type */	\
			return;	\
		}	\
		val.fld = fld;	\
		self->error_status = rig_set_##f(self->rig  _VFO_ARG , stg, val);	\
	}
#else
#define METHODSIMPLESET(f, t1, fld, chk) void set_##f (setting_t stg, t1 fld  _VFO_DECL) \
		{ value_t val = {0}; 	\
		if (chk) {	\
			int ival = (int)fld;	\
			val.i = fld;	\
		}	\
		else {	\
		    val.fld = fld;	\
		}	\
		self->error_status = rig_set_##f(self->rig  _VFO_ARG , stg, val);	\
	}
#endif

/*
 * RIG_CONF_ extparm's type:
 *   NUMERIC: val.f
 *   COMBO: val.i, starting from 0
 *   STRING: val.s
 *   CHECKBUTTON: val.i 0/1
 */

#ifndef SWIGLUA
#define METHODSUPERSET(f, t1, fld, chk) void set_##f (const char *name, t1 fld  _VFO_DECL) \
		{ setting_t stg; value_t val = {0}; 	\
		stg = rig_parse_##f(name);	\
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
		if (chk) {	\
			self->error_status = -RIG_EINVAL;	/* invalid type */	\
			return;	\
		}	\
		val.fld = fld;	\
		self->error_status = rig_set_##f(self->rig  _VFO_ARG , stg, val);	\
	}
#else
#define METHODSUPERSET(f, t1, fld, chk) void set_##f (const char *name, t1 fld  _VFO_DECL) \
		{ setting_t stg; value_t val = {0}; 	\
		stg = rig_parse_##f(name);	\
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
		if (chk) {	\
		    int ival = (int)fld;	\
		    val.i = ival;	\
		}	\
		else	{ \
		    val.fld = fld;	\
		}	\
		self->error_status = rig_set_##f(self->rig  _VFO_ARG , stg, val);	\
	}
#endif

#define METHODSTRSET(f) void set_##f (const char *name, const char *s  _VFO_DECL) \
		{ value_t val = {0}; /* only ext_level/parm's can have string values */  \
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
		{ value_t val = {0}; 	if (chk) {	\
			self->error_status = -RIG_EINVAL;	/* invalid type */	\
			return val.fld;		/* undefined value */	\
		}	\
		self->error_status = rig_get_##f(self->rig  _VFO_ARG , stg, &val);	\
		return val.fld;	\
	}



#define METHODSUPERGET(fct, t1, fld, chk) t1 get_##fct##_##fld(const char *name  _VFO_DECL) \
		{ setting_t stg; value_t val = {0}; 	\
		stg = rig_parse_##fct(name);	\
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
		if (chk) {	\
			self->error_status = -RIG_EINVAL;	/* invalid type */	\
			return val.fld;	/* undefined */ \
		}	\
		self->error_status = rig_get_##fct(self->rig  _VFO_ARG , stg, &val);	\
		return val.fld;	\
	}

#define METHODSTRGET(f) void get_##f (const char *name, char *returnstr  _VFO_DECL) \
		{ value_t val = {0}; /* only ext_level/parm's can have string values */ \
		const struct confparams *cfp; \
		returnstr[0] = '\0'; \
		cfp = rig_ext_lookup(self->rig, name); \
		if (!cfp || cfp->type != RIG_CONF_STRING) {	\
			self->error_status = -RIG_EINVAL;	/* no such parameter/invalid type */ \
			return; \
		} \
		val.s = returnstr; \
		self->error_status = rig_get_ext_##f(self->rig _VFO_ARG, cfp->token, &val); \
	}


/*
 * string as return value
 * NB: The %apply has to be outside the %extend,
 * but the %cstring_bounded_output must be inside.
 */
%ignore MAX_RETURNSTR;
#define MAX_RETURNSTR 256

#ifndef SWIGJAVA
%apply char *OUTPUT { char *returnstr };
#endif

/*
 * Rig class alike
 */
%extend Rig {

#ifndef SWIGLUA
#ifndef SWIG_CSTRING_UNIMPL
#ifndef SWIGJAVA
	%cstring_bounded_output(char *returnstr, MAX_RETURNSTR);
#endif
#endif
#endif

	Rig(int rig_model) {
		Rig *r;

		r = (Rig*)malloc(sizeof(Rig));
		if (!r)
			return NULL;
		r->rig = rig_init(rig_model);
		if (!r->rig) {
			free(r);
			return NULL;
		}
#ifdef SWIGPYTHON
		r->python_callbacks = calloc(1, sizeof(python_callbacks_t));
		if (!r->python_callbacks) {
			free(r);
			return NULL;
		}
#endif
		/* install shortcuts */
		r->caps = r->rig->caps;
		r->state = rig_data_pointer(r->rig, RIG_PTRX_STATE);
		r->do_exception = 0;	/* default is disabled */
		r->error_status = RIG_OK;

		return r;
	}
	~Rig () {
		rig_cleanup(self->rig);
#ifdef SWIGPYTHON
		free(self->python_callbacks);
#endif
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

	METHOD3(set_freq, freq_t)
	METHOD2_INIT(set_mode, rmode_t, pbwidth_t, RIG_PASSBAND_NORMAL)
	METHOD3(set_ptt, ptt_t)
	METHOD3(set_rptr_shift, rptr_shift_t)
	METHOD3(set_rptr_offs, shortfreq_t)
	METHOD3(set_ctcss_tone, tone_t)
	METHOD3(set_dcs_code, tone_t)
	METHOD3(set_ctcss_sql, tone_t)
	METHOD3(set_dcs_sql, tone_t)
	METHOD3(set_split_freq, freq_t)
	METHOD2_INIT(set_split_mode, rmode_t, pbwidth_t, RIG_PASSBAND_NORMAL)
	METHOD3_INIT(set_split_freq_mode, freq_t, rmode_t, pbwidth_t, RIG_PASSBAND_NORMAL)
	METHOD2(set_split_vfo, split_t, vfo_t)
	METHOD3(set_rit, shortfreq_t)
	METHOD3(set_xit, shortfreq_t)
	METHOD3(set_ts, shortfreq_t)
	METHOD2(set_ant, ant_t, value_t)
	METHOD2(set_func, setting_t, int)
	METHOD2(set_ext_func, hamlib_token_t, int)
	METHOD3(set_bank, int)
	METHOD3(set_mem, int)
	METHOD3(send_dtmf, const_char_string)
	METHOD3(send_morse, const_char_string)
	METHOD3(vfo_op, vfo_op_t)
	METHOD2(scan, scan_t, int)
	METHODSIMPLESET(level, int, i, RIG_LEVEL_IS_FLOAT(stg))
	METHODSIMPLESET(level, float, f, !RIG_LEVEL_IS_FLOAT(stg))
	METHODSUPERSET(level, int, i, RIG_LEVEL_IS_FLOAT(stg))
	METHODSUPERSET(level, float, f, !RIG_LEVEL_IS_FLOAT(stg))
	METHODSTRSET(level)
	METHOD2(set_ext_level, hamlib_token_t, value_t)

	METHODSIMPLEGET(level, int, i, RIG_LEVEL_IS_FLOAT(stg))
	METHODSIMPLEGET(level, float, f, !RIG_LEVEL_IS_FLOAT(stg))
	METHODSUPERGET(level, int, i, RIG_LEVEL_IS_FLOAT(stg))
	METHODSUPERGET(level, float, f, !RIG_LEVEL_IS_FLOAT(stg))
	METHODSTRGET(level)

	/*
	 * these ones take no vfo arg
	 */
#undef _VFO_ARG
#undef _VFO_DECL
#define _VFO_ARG
#define _VFO_DECL
	METHOD1(set_vfo, vfo_t)		/* particular case */
	METHOD1(set_powerstat, powerstat_t)
//	METHOD1(set_trn, int)
	METHOD1(has_get_level, setting_t)
	METHOD1(has_get_parm, setting_t)
	METHOD1(has_set_parm, setting_t)
	METHOD1(has_get_func, setting_t)
	METHOD1(has_set_func, setting_t)
	METHOD1(reset, reset_t)
	METHOD1(has_scan, scan_t)
	METHOD1(has_vfo_op, vfo_op_t)
	METHOD1(passband_normal, rmode_t)
	METHOD1(passband_narrow, rmode_t)
	METHOD1(passband_wide, rmode_t)
	METHOD1(set_vfo_opt, int)

	METHOD1(ext_token_lookup, const_char_string)	/* level & parm */
	METHOD1(token_lookup, const_char_string)	/* conf */

	METHOD2(set_conf, hamlib_token_t, const_char_string)
	METHOD2(set_ext_parm, hamlib_token_t, value_t)

	METHODSIMPLESET(parm, int, i, RIG_PARM_IS_FLOAT(stg))
	METHODSIMPLESET(parm, float, f, !RIG_PARM_IS_FLOAT(stg))
	METHODSUPERSET(parm, int, i, RIG_PARM_IS_FLOAT(stg))
	METHODSUPERSET(parm, float, f, !RIG_PARM_IS_FLOAT(stg))
	METHODSTRSET(parm)

	METHODSIMPLEGET(parm, int, i, RIG_PARM_IS_FLOAT(stg))
	METHODSIMPLEGET(parm, float, f, !RIG_PARM_IS_FLOAT(stg))
	METHODSUPERGET(parm, int, i, RIG_PARM_IS_FLOAT(stg))
	METHODSUPERGET(parm, float, f, !RIG_PARM_IS_FLOAT(stg))
	METHODSTRGET(parm)


	void set_conf(const char *name, const char *val) {
		hamlib_token_t tok = rig_token_lookup(self->rig, name);
		if (tok == RIG_CONF_END)
			self->error_status = -RIG_EINVAL;
		else
			self->error_status = rig_set_conf(self->rig, tok, val);
	}



	/* TODO: get_ext_parm_list, level, conf, .. */

	/* get functions */

	METHOD1VGET(get_freq, freq_t)
	extern void get_mode(rmode_t * OUTPUT, pbwidth_t * OUTPUT, vfo_t vfo = RIG_VFO_CURR);
	extern void get_split_mode(rmode_t * OUTPUT, pbwidth_t * OUTPUT, vfo_t vfo = RIG_VFO_CURR);
	METHOD1GET(get_vfo, vfo_t)
	METHOD1VGET(get_ptt, ptt_t)
	METHOD1VGET(get_rptr_shift, rptr_shift_t)
	METHOD1VGET(get_rptr_offs, shortfreq_t)
	METHOD1VGET(get_ctcss_tone, tone_t)
	METHOD1VGET(get_dcs_code, tone_t)
	METHOD1VGET(get_ctcss_sql, tone_t)
	METHOD1VGET(get_dcs_sql, tone_t)
	METHOD1VGET(get_split_freq, freq_t)
	void get_split_vfo(split_t *split, vfo_t *tx_vfo, vfo_t vfo = RIG_VFO_CURR)
	{ self->error_status = rig_get_split_vfo(self->rig, vfo, split, tx_vfo); }

	METHOD1VGET(get_rit, shortfreq_t)
	METHOD1VGET(get_xit, shortfreq_t)
	METHOD1VGET(get_ts, shortfreq_t)
	extern void get_ant(ant_t *OUTPUT, ant_t *OUTPUT, ant_t *OUTPUT, signed int *OUTPUT, ant_t ant, vfo_t vfo = RIG_VFO_CURR);
	void get_vfo_info (int *satmode, split_t *split, pbwidth_t *width, rmode_t *mode, freq_t *freq, vfo_t vfo = RIG_VFO_CURR)
	{ self->error_status = rig_get_vfo_info(self->rig, vfo, freq, mode, width, split, satmode); }
	METHOD1VGET(get_mem, int)
	METHOD1GET(get_powerstat, powerstat_t)
//	METHOD1GET(get_trn, int)
	METHOD1VGET(get_dcd, dcd_t)

	// Handling of event callbacks
#ifdef SWIGPYTHON
	RIG_SET_CALLBACK(freq)
	RIG_SET_CALLBACK(mode)
	RIG_SET_CALLBACK(vfo)
	RIG_SET_CALLBACK(ptt)
	RIG_SET_CALLBACK(dcd)
	RIG_SET_CALLBACK(pltune)
	RIG_SET_CALLBACK(spectrum)
#endif

	int mem_count(void) {
		return rig_mem_count(self->rig);
	}

	const chan_t *lookup_mem_caps(int channel_num = RIG_MEM_CAPS_ALL)
	{
		return rig_lookup_mem_caps(self->rig, channel_num);
	}

	void set_channel(const struct channel *chan) {
		self->error_status = rig_set_channel(self->rig, RIG_VFO_NONE, chan);
	}

	void get_channel(struct channel *chan, int read_only) {
		self->error_status = rig_get_channel(self->rig, RIG_VFO_NONE, chan, read_only);
		/* TODO: handle ext_level's */
	}

	void get_chan_all(struct channel *chans) {
		self->error_status = rig_get_chan_all(self->rig, RIG_VFO_NONE, chans);
		/* TODO: handle ext_level's */
	}

	/*
	 * Rem: does swig has to be told that returned object need to be handled by gc?
	 */
	struct channel *get_chan_all(void);

	/* get_channel() returns current VFO data
	 * get_channel(10) returns content of memory #10
	 * get_channel(0, RIG_VFO_A) returns VFO A data
	 * Rem: does swig has to be told that returned object need to be handled by gc?
	 */
	struct channel *get_channel(int read_only, int channel_num = INT_MAX, vfo_t vfo = RIG_VFO_MEM) {
		struct channel *chan;
		chan = new_channel(channel_num != INT_MAX ? vfo : RIG_VFO_CURR, channel_num);
		if (!chan)
		{
			self->error_status = -RIG_ENOMEM;
			return NULL;
		}
		self->error_status = rig_get_channel(self->rig, RIG_VFO_NONE, chan, read_only);
		/* TODO: copy ext_level's */
		return chan;
	}

	void get_conf(hamlib_token_t tok, char *returnstr) {
		returnstr[0] = '\0';
		self->error_status = rig_get_conf2(self->rig, tok, returnstr, MAX_RETURNSTR);
	}

	void get_conf(const char *name, char *returnstr) {
		returnstr[0] = '\0';
		hamlib_token_t tok = rig_token_lookup(self->rig, name);
		if (tok == RIG_CONF_END)
			self->error_status = -RIG_EINVAL;
		else
			self->error_status = rig_get_conf2(self->rig, tok, returnstr, MAX_RETURNSTR);
	}

	void recv_dtmf(char *returnstr, vfo_t vfo = RIG_VFO_CURR) {
		int len = MAX_RETURNSTR;
		self->error_status = rig_recv_dtmf(self->rig, vfo, returnstr, &len);
		returnstr[len] = '\0';
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

	int get_ext_func(hamlib_token_t func, vfo_t vfo = RIG_VFO_CURR) {
		int status;
		self->error_status = rig_get_ext_func(self->rig, vfo, func, &status);
		return status;
	}

#ifdef SWIGPYTHON
	PyObject *send_raw(PyObject *send_obj, PyObject *term_obj=NULL)
	{
		char *send, *term;
		PyObject *bytes_obj;
		Py_ssize_t send_len;
		int reply_len = MAX_RETURNSTR;
		char reply_buffer[MAX_RETURNSTR];
		int count;

		if (PyUnicode_Check(send_obj)) {
			bytes_obj = PyUnicode_AsUTF8String(send_obj);
			PyBytes_AsStringAndSize(bytes_obj, &send, &send_len);
			Py_XDECREF(bytes_obj);
		} else if (PyBytes_Check(send_obj)) {
			PyBytes_AsStringAndSize(send_obj, &send, &send_len);
		} else {
			SWIG_Python_RaiseOrModifyTypeError("Expected string or bytes for send argument");
			return NULL;
		}

		// Using NULL for length in PyUnicode_AsUTF8AndSize() and PyBytes_AsStringAndSize()
		// because we can't accept '\0' because there is no length for term in rig_send_raw()
		if (PyUnicode_Check(term_obj)) {
			bytes_obj = PyUnicode_AsUTF8String(term_obj);
			PyBytes_AsStringAndSize(bytes_obj, &term, NULL);
			Py_XDECREF(bytes_obj);
		} else if (PyBytes_Check(term_obj)) {
			PyBytes_AsStringAndSize(term_obj, &term, NULL);
		} else if (term_obj == Py_None) {
			term = NULL;
		} else {
			SWIG_Python_RaiseOrModifyTypeError("Expected string or bytes or NULL for term argument");
			return NULL;
		}

		count = rig_send_raw(self->rig, (unsigned char *)send, send_len, (unsigned char *)reply_buffer, reply_len, (unsigned char *)term);
		self->error_status = count < 0 ? count : RIG_OK;

		if (PyUnicode_Check(send_obj)) {
			return PyUnicode_FromStringAndSize(reply_buffer, count);
		} else {
			return PyBytes_FromStringAndSize(reply_buffer, count);
		}
	}
#endif

#ifdef SWIGPERL
	void send_raw(char *send, char *term, char *returnstr)
	{
		returnstr[0] = '\0';
		int count;

		count = rig_send_raw(self->rig, send, strlen(send), returnstr, MAX_RETURNSTR, term);
		self->error_status = count < 0 ? count : RIG_OK;
	}
#endif

//#ifndef SWIGJAVA
	/* TODO */
	void get_level(setting_t level, vfo_t vfo = RIG_VFO_CURR)
		{ value_t val = {0}; self->error_status = rig_get_level(self->rig, vfo, level, &val);
			//if (RIG_LEVEL_IS_FLOAT(level))
			/* TODO: dynamic casting */
		}

	void get_ext_level(hamlib_token_t token, vfo_t vfo = RIG_VFO_CURR)
		{ value_t val = {0}; self->error_status = rig_get_level(self->rig, vfo, token, &val);
			//if (RIG_LEVEL_IS_FLOAT(level))
			/* TODO: dynamic casting */
		}

	void get_parm(setting_t parm)
		{ value_t val = {0}; self->error_status = rig_get_parm(self->rig, parm, &val);
			//if (RIG_LEVEL_IS_FLOAT(parm))
			/* TODO: dynamic casting */
		}

	void get_ext_parm(hamlib_token_t parm)
		{ value_t val = {0}; self->error_status = rig_get_parm(self->rig, parm, &val);
			//if (RIG_LEVEL_IS_FLOAT(parm))
			/* TODO: dynamic casting */
		}

//#endif

};

%{

/*
 * these ones return 2 values, here is a perl example:
 * 	($mode, $width) = $rig->get_mode();
 */
void Rig_get_mode(Rig *self, rmode_t *mode, pbwidth_t *width, vfo_t vfo)
{
	self->error_status = rig_get_mode(self->rig, vfo, mode, width);
}

void Rig_get_split_mode(Rig *self, rmode_t *mode, pbwidth_t *width, vfo_t vfo)
{
	self->error_status = rig_get_split_mode(self->rig, vfo, mode, width);
}

/*
 * these ones return 3 values
 */
void Rig_get_split_freq_mode(Rig *self, vfo_t vfo, freq_t *tx_freq, rmode_t *tx_mode, pbwidth_t *tx_width)
{
	self->error_status = rig_get_split_freq_mode(self->rig, vfo, tx_freq, tx_mode, tx_width);
}


/*
 * these ones return 4 values
 */
void Rig_get_ant(Rig *self, ant_t *ant_rx, ant_t *ant_tx, ant_t *ant_curr, signed int *option, ant_t ant, vfo_t vfo)
{
	value_t value;
	self->error_status = rig_get_ant(self->rig, vfo, ant, &value, ant_curr, ant_tx, ant_rx);
	*option = value.i;
}

struct channel *Rig_get_chan_all(Rig *self)
{
	struct channel *chans;
	int nb_chans = rig_mem_count(self->rig);

	/* TODO: memleak: typemap to release that memory */
	chans = calloc(sizeof (struct channel), nb_chans);
	if (!chans)
	{
		self->error_status = -RIG_ENOMEM;
		return NULL;
	}
	self->error_status = rig_get_chan_all(self->rig, RIG_VFO_NONE, chans);
	/* TODO: copy ext_level's */
	return chans;
}

%}
