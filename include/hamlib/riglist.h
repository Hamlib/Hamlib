/*  hamlib - Copyright (C) 2000 Frank Singleton
 *
 * riglist.h - Copyrith (C) 2000 Stephane Fillod
 * This program defines the list of supported rigs.
 *
 *
 * 	$Id: riglist.h,v 1.2 2000-10-10 22:05:23 f4cfe Exp $	 *
 *
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 * 
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 * 
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.
 * 
 */

#ifndef _RIGLIST_H
#define _RIGLIST_H 1


enum rig_model_e {
	RIG_MODEL_FT847 = 0,
	RIG_MODEL_FT1000,
	RIG_MODEL_FT1000D,
	RIG_MODEL_FT747,
	RIG_MODEL_FT840,
	RIG_MODEL_FT920,

	RIG_MODEL_TS570D,
	RIG_MODEL_TS870S,
	RIG_MODEL_TS950,

	RIG_MODEL_IC1271,
	RIG_MODEL_IC1275,
	RIG_MODEL_IC271,
	RIG_MODEL_IC275,
	RIG_MODEL_IC375,
	RIG_MODEL_IC471,
	RIG_MODEL_IC475,
	RIG_MODEL_IC575,
	RIG_MODEL_IC706,
	RIG_MODEL_IC706MKII,
	RIG_MODEL_IC706MKIIG,
	RIG_MODEL_IC707,
	RIG_MODEL_IC718,
	RIG_MODEL_IC725,
	RIG_MODEL_IC726,
	RIG_MODEL_IC728,
	RIG_MODEL_IC729,
	RIG_MODEL_IC731,
	RIG_MODEL_IC735,
	RIG_MODEL_IC736,
	RIG_MODEL_IC737,
	RIG_MODEL_IC738,
	RIG_MODEL_IC746,
	RIG_MODEL_IC751,
	RIG_MODEL_IC751A,
	RIG_MODEL_IC756,
	RIG_MODEL_IC756PRO,
	RIG_MODEL_IC761,
	RIG_MODEL_IC765,
	RIG_MODEL_IC775,
	RIG_MODEL_IC781,
	RIG_MODEL_IC820,
	RIG_MODEL_IC821,
	RIG_MODEL_IC821H,
	RIG_MODEL_IC970,
	RIG_MODEL_ICR71,
	RIG_MODEL_ICR10,
	RIG_MODEL_ICR72,
	RIG_MODEL_ICR75,
	RIG_MODEL_ICR7000,
	RIG_MODEL_ICR7100,
	RIG_MODEL_ICR8500,
	RIG_MODEL_ICR9000,
	RIG_MODEL_PCR1000

	/* etc. */
};

typedef enum rig_model_e rig_model_t;

/*
 * It would be nice to have an automatic way of referencing all the backends
 * supported by hamlib. Maybe this array should be placed in a separate file..
 */
#if 0
extern const struct rig_caps ft747_caps;
extern const struct rig_caps ft847_caps;
extern const struct rig_caps ic706_caps;
extern const struct rig_caps ic706mkiig_caps;
extern const struct rig_caps ft747_caps;

#endif
/* etc. */


#endif /* _RIGLIST_H */





