/*  hamlib - Copyright (C) 2000 Frank Singleton
 *
 * riglist.h - Copyrith (C) 2000 Stephane Fillod
 * This program defines the list of supported rigs.
 *
 *
 * 	$Id: riglist.h,v 1.1 2000-09-14 00:50:55 f4cfe Exp $	 *
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
	RIG_MODEL_IC706,
	RIG_MODEL_IC706MKII,
	RIG_MODEL_IC706MKIIG
	/* etc. */
};

typedef enum rig_model_e rig_model_t;


#endif /* _RIGLIST_H */

