/*
 *  Hamlib bindings - Rotator interface
 *  Copyright (c) 2001,2002 by Stephane Fillod
 *
 *	$Id: rotator.swg,v 1.1 2002-09-08 22:31:01 fillods Exp $
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

typedef struct Rot {
	ROT *rot;
	struct rot_caps *caps;		/* shortcut to ROT->caps */
	struct rot_state *state;	/* shortcut to ROT->state */
} Rot;

%}

/*
 * decalre wrapper method with 0,1,2 arguments besides ROT*
 */
#define ROTMETHOD0(f) int f () \
				{ return rot_##f(self->rot); }
#define ROTMETHOD1(f, t1) int f (t1 _##t1) \
				{ return rot_##f(self->rot, _##t1); }
#define ROTMETHOD2(f, t1, t2) int f (t1 _##t1##_1, t2 _##t2##_2) \
				{ return rot_##f(self->rot, _##t1##_1, _##t2##_2); }

%extend Rot {
	Rot(rot_model_t rot_model) {
		Rot *r;

		r = (Rot*)malloc(sizeof(Rot));
		if (!r)
			return NULL;
		r->rot = rot_init(rot_model);
		if (!r->rot) {
			free(r);
			return NULL;
		}
		/* install shortcuts */
		r->caps = r->rot->caps;
		r->state = &r->rot->state;
		return r;
	}
	~Rot () {
		rot_cleanup(self->rot);
		free(self);
	}
	ROTMETHOD0(open)
	ROTMETHOD0(close)

	ROTMETHOD2(set_position, azimuth_t, elevation_t)
	ROTMETHOD0(stop)
	ROTMETHOD0(park)
	ROTMETHOD1(reset, rot_reset_t)
	ROTMETHOD2(move, int, int)

	ROTMETHOD1(token_lookup, const_char_string)	/* conf */

	int set_conf_s(const char *name, const char *val) {
		token_t tok = rot_token_lookup(self->rot, name);
		/* FIXME: token not found */
		return rot_set_conf(self->rot, tok, val);
	}

	ROTMETHOD2(set_conf, token_t, const_char_string)

	/* TODO: get_ext_parm_list, level, conf, .. */

	/* TODO: all the get functions */
};

