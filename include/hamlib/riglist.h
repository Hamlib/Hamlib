/*  hamlib - Copyright (C) 2000 Frank Singleton
 *
 * riglist.h - Copyrith (C) 2000,2001 Stephane Fillod
 * This include defines the list of known rigs.
 *
 *
 * 	$Id: riglist.h,v 1.12 2001-06-02 17:48:46 f4cfe Exp $	 *
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
	RIG_MODEL_DUMMY = 0,

	/*
	 * Yaesu 
	 */
	RIG_MODEL_FT847,
	RIG_MODEL_FT1000,
	RIG_MODEL_FT1000D,
	RIG_MODEL_FT1000MP,
	RIG_MODEL_FT747,
	RIG_MODEL_FT757,
	RIG_MODEL_FT757GXII,
	RIG_MODEL_FT575,
	RIG_MODEL_FT767,
	RIG_MODEL_FT736R,
	RIG_MODEL_FT840,
	RIG_MODEL_FT820,
	RIG_MODEL_FT900,
	RIG_MODEL_FT920,
	RIG_MODEL_FT890,
	RIG_MODEL_FT990,
	RIG_MODEL_FRG100,	/* same as FT890/990 ? */
	RIG_MODEL_FRG9600,
	RIG_MODEL_FRG8800,

	/*
	 * Kenwood
	 */
	RIG_MODEL_TS50,
	RIG_MODEL_TS440,
	RIG_MODEL_TS450S,
	RIG_MODEL_TS570D,
	RIG_MODEL_TS690S,
	RIG_MODEL_TS711,
	RIG_MODEL_TS790,
	RIG_MODEL_TS811,
	RIG_MODEL_TS850,
	RIG_MODEL_TS870S,
	RIG_MODEL_TS940,
	RIG_MODEL_TS950,
	RIG_MODEL_TS950SDX,
	RIG_MODEL_R5000,

	/*
	 * Icom
	 */
	RIG_MODEL_ICALL,	/* do-it-all, for debug purpose */
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
	RIG_MODEL_ICR10,
	RIG_MODEL_ICR71,
	RIG_MODEL_ICR72,
	RIG_MODEL_ICR75,
	RIG_MODEL_ICR7000,
	RIG_MODEL_ICR7100,
	RIG_MODEL_ICR8500,
	RIG_MODEL_ICR9000,
	RIG_MODEL_PCR1000,
	RIG_MODEL_PCR100,

	/*
	 * Optoelectronics (CI-V)
	 */
	RIG_MODEL_MINISCOUT,
	RIG_MODEL_XPLORER,

	/*
	 * TenTec (CI-V)
	 */
	RIG_MODEL_OMNIVI,
	RIG_MODEL_OMNIVIP,	/* OMNI-IV+ */

	/*
	 * AOR
	 */
	RIG_MODEL_AR8200,
	RIG_MODEL_AR8000,
	RIG_MODEL_AR7030,
	RIG_MODEL_AR5000,
	RIG_MODEL_AR3030,
	RIG_MODEL_AR3000A,
	RIG_MODEL_AR3000,
	RIG_MODEL_AR2700,
	RIG_MODEL_AR2500,
	RIG_MODEL_AR16,

	/*
	 * JRC
	 */
	RIG_MODEL_JST145,
	RIG_MODEL_JST245,
	RIG_MODEL_CMH530,
	RIG_MODEL_NRD345,
	RIG_MODEL_NRD525,
	RIG_MODEL_NRD535,
	RIG_MODEL_NRD545,

	/*
	 * Radio Shack
	 */
	RIG_MODEL_RS64,		/* PRO-64 */
	RIG_MODEL_RS2005,	/* w/ OptoElectronics OS456 Board */
	RIG_MODEL_RS2006,	/* w/ OptoElectronics OS456 Board */
	RIG_MODEL_RS2035,	/* w/ OptoElectronics OS435 Board */
	RIG_MODEL_RS2042,	/* w/ OptoElectronics OS435 Board */
	RIG_MODEL_RS2041,	/* PRO-2041 */
	RIG_MODEL_RS2052,	/* PRO-2052 */

	/*
	 * Uniden
	 */
	RIG_MODEL_BC780,	/* Uniden BC780 - Trunk Tracker "Desktop Radio" */
	RIG_MODEL_BC245,
	RIG_MODEL_BC895,

	/*
	 * Drake
	 */
	RIG_MODEL_DKR8,
	RIG_MODEL_DKR8A,

	/*
	 * Lowe
	 */
	RIG_MODEL_HF150,
	RIG_MODEL_HF225,
	RIG_MODEL_HF250,

	/*
	 * Racal
	 */
	RIG_MODEL_RA3790,
	RIG_MODEL_RA3720,

	/*
	 * Watkins-Johnson
	 */
	RIG_MODEL_HF1000,
	RIG_MODEL_HF1000A,
	RIG_MODEL_WJ8711,

	/*
	 * Rohde & Schwarz
	 */
	RIG_MODEL_ESM500,
	RIG_MODEL_EK890,
	RIG_MODEL_EK891,
	RIG_MODEL_EK895,
	RIG_MODEL_EK070,

	/*
	 * Skanti
	 */
	RIG_MODEL_TRP7000,
	RIG_MODEL_TRP8000,
	RIG_MODEL_TRP9000,

	/*
	 * WiNRADiO/LinRADiO, by Rosetta?
	 */
	RIG_MODEL_WR1000,	/* WR-1000i */
	RIG_MODEL_WR1500,	/* WR-1500i and WR-1500e */
	RIG_MODEL_WR1550,	/* WR-1550e */
	RIG_MODEL_WR3100,
	RIG_MODEL_WR3150,	/* WR-3150i */
	RIG_MODEL_WR3500,
	RIG_MODEL_WR3700,

	/*
	 * Ten Tec
	 */
	RIG_MODEL_TT550,	/* Pegasus */
	RIG_MODEL_TT538,	/* Jupiter */
	RIG_MODEL_RX320,
	RIG_MODEL_RX340,

	RIG_MODEL_KWZ30,	/* KNEISNER +DOERING */
	RIG_MODEL_E1800,	/* DASA-Telefunken */
	RIG_MODEL_EKD500,	/* RFT */
	RIG_MODEL_DX77,		/* Alinco */


	/* etc. */
};

typedef enum rig_model_e rig_model_t;


#endif /* _RIGLIST_H */
