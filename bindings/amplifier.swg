/*
 *  Hamlib bindings - Amplifier interface
 *  Copyright (c) 2001,2002 by Stephane Fillod
 *  Copyright (c) 2020 by Michael Black W9MDB
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

%inline %{

typedef struct Amp {
	AMP *amp;
	struct amp_caps *caps;		/* shortcut to AMP->caps */
	struct amp_state *state;	/* shortcut to AMP->state */
	int error_status;
	int do_exception;
} Amp;

%}

/*
 * declare wrapper method with 0,1,2 arguments besides AMP*
 */
#define AMPMETHOD0(f) void f () \
				{ self->error_status = amp_##f(self->amp); }
#define AMPMETHOD1(f, t1) void f (t1 _##t1) \
				{ self->error_status = amp_##f(self->amp, _##t1); }
#define AMPMETHOD2(f, t1, t2) void f (t1 _##t1##_1, t2 _##t2##_2) \
				{ self->error_status = amp_##f(self->amp, _##t1##_1, _##t2##_2); }
#define AMPMETHOD3(f, t1, t2, t3) void f (t1 _##t1##_1, t2 _##t2##_2, t3 _##t3##_3) \
				{ self->error_status = amp_##f(self->amp, _##t1##_1, _##t2##_2, t3 _##t3##_3); }
#define AMPMETHOD4(f, t1, t2, t3, t4) void f (t1 _##t1##_1, t2 _##t2##_3, t3 _##t3##_3, ##t4##_4) \
				{ self->error_status = amp_##f(self->amp, _##t1##_1, _##t2##_3, t3 _##t3##_3, ##t4##_4); }

%extend Amp {
	Amp(amp_model_t amp_model) {
		Amp *r;

		r = (Amp*)malloc(sizeof(Amp));
		if (!r)
			return NULL;
		r->amp = amp_init(amp_model);
		if (!r->amp) {
			free(r);
			return NULL;
		}
		/* install shortcuts */
		r->caps = r->amp->caps;
		r->state = &r->amp->state;
		r->do_exception = 0;    /* default is disabled */
		r->error_status = RIG_OK;
		return r;
	}
	~Amp () {
		amp_cleanup(self->amp);
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

	AMPMETHOD0(open)
	AMPMETHOD0(close)

	AMPMETHOD1(reset, amp_reset_t)

	AMPMETHOD1(token_lookup, const_char_string)	/* conf */

  /* set functions */
  AMPMETHOD1(set_freq, freq_t)
  AMPMETHOD1(set_powerstat, powerstat_t)

	void set_conf(const char *name, const char *val) {
		token_t tok = amp_token_lookup(self->amp, name);
		if (tok == RIG_CONF_END)
			self->error_status = -RIG_EINVAL;
		else
			self->error_status = amp_set_conf(self->amp, tok, val);
	}

	AMPMETHOD2(set_conf, token_t, const_char_string)

/*
 * declare wrapper method with one output argument besides AMP* 
 */
#define AMPMETHOD1VGET(f, t1) t1 f \
        { t1 _##t1; self->error_status = amp_##f(self->amp, &_##t1); return _##t1; }


  /* get functions */

  const char *get_conf(token_t tok) {
          static char s[128] = "";
          self->error_status = amp_get_conf(self->amp, tok, s);
          return s;
  }

  const char *get_conf(const char *name) {
          token_t tok = amp_token_lookup(self->amp, name);
          static char s[128] = "";
          if (tok == RIG_CONF_END)
                  self->error_status = -RIG_EINVAL;
          else
                  self->error_status = amp_get_conf(self->amp, tok, s);
          return s;
  }

  const char * get_info(void) {
          const char *s;
          s = amp_get_info(self->amp);
          self->error_status = s ? RIG_OK : -RIG_EINVAL;
          return s;
  }

};
%{

  void Amp_get_freq(Amp *self, freq_t *freq)
  {
    self->error_status = amp_get_freq(self->amp, freq);
  }
  void Amp_get_powerstat(Amp *self, powerstat_t *status)
  {
    self->error_status = amp_get_powerstat(self->amp, status);
  }
%}
