/*
 *  Hamlib TS2000 backend - main header
 *  Copyright (c) 2000-2002 by Stephane Fillod
 *
 *		$Id: ts2k.h,v 1.2 2002-06-29 09:54:50 dedmons Exp $
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

/*
 * created from kenwood.h	--Dale kd7eni
 */

/*//+kd7eni*/

#ifndef _TS2K_H
#define _TS2K_H

#undef	_USEVFO

// imported from ts2000.h --Dale kd7eni
/*
 * 103 available DCS codes
 */
static const tone_t ts2k_dcs_list[] = {
	 23,  25,  26,  31,  32,  36,  43,  47,  51,  53,  54,
	 65,  71,  72,  73,  74, 114, 115, 116, 122, 125, 131,
	132, 134, 143, 145, 152, 155, 156, 162, 165, 172, 174,
	205, 212, 223, 225, 226, 243, 244, 245, 246, 251, 252,
	255, 261, 263, 265, 266, 271, 274, 306, 311, 315, 325,
	331, 332, 343, 346, 351, 356, 364, 365, 371, 411, 412,
	413, 423, 431, 432, 445, 446, 452, 454, 455, 462, 464,
	465, 466, 503, 506, 516, 523, 526, 532, 546, 565, 606,
	612, 624, 627, 631, 632, 654, 662, 664, 703, 712, 723,
	731, 732, 734, 743, 754,
	0
};

#define TS2K_PTT_ON_MAIN	8
#define TS2K_CTRL_ON_MAIN	4
#define TS2K_PTT_ON_SUB		2
#define TS2K_CTRL_ON_SUB	1

// Needed to prevent ts2k_transaction (kenwood_transaction) from
// expecting a reply from the ts2000 in cases where there is none.
#define NOREPLY	0
// some commands reply "?;" if rig currently in requested mode
// usually, there is NOREPLY when mode is changed.  This can be
// put in the acknowledge length to signify the error is no error.
#define BUGERR	-1

//-kd7eni

#define EOM_KEN ";"
#define EOM_TH "\r"

struct ts2k_priv_caps {
    /* read-only values */
    const char *cmdtrm;    /* Command termination chars (ken=';' or th='\r') */
    /* changable values */
        // nothing
};

#if 0  /* No private data for Kenwood backends. */
struct ts2k_priv_data {
    int dummy;  // placeholder for real entries.
};
#endif

extern int ts2k_init(RIG *rig);
extern int ts2k_cleanup(RIG *rig);


extern const int ts2k_ctcss_list[];

int ts2k_transaction(RIG *rig, const char *cmd, int cmd_len, char *data,
				size_t *data_len);
int ts2k_set_vfo(RIG *rig, vfo_t vfo);
int ts2k_get_vfo(RIG *rig, vfo_t *vfo);
int ts2k_set_freq(RIG *rig, vfo_t vfo, freq_t freq);
int ts2k_get_freq(RIG *rig, vfo_t vfo, freq_t *freq);
int ts2k_set_mode(RIG *rig, vfo_t vfo, rmode_t mode, pbwidth_t width);
int ts2k_get_mode(RIG *rig, vfo_t vfo, rmode_t *mode, pbwidth_t *width);
int ts2k_set_level(RIG *rig, vfo_t vfo, setting_t level, value_t val);
int ts2k_get_level(RIG *rig, vfo_t vfo, setting_t level, value_t *val);
int ts2k_set_func(RIG *rig, vfo_t vfo, setting_t func, int status);
int ts2k_get_func(RIG *rig, vfo_t vfo, setting_t func, int *status);
int ts2k_set_ctcss_tone(RIG *rig, vfo_t vfo, tone_t tone);
int ts2k_set_tone(RIG *rig, vfo_t vfo, tone_t tone);
int ts2k_get_ctcss_tone(RIG *rig, vfo_t vfo, tone_t *tone);
int ts2k_get_tone(RIG *rig, vfo_t vfo, tone_t *tone);
int ts2k_set_Tones(RIG *rig, vfo_t vfo, tone_t tone, const char ct);
int ts2k_get_Tones(RIG *rig, vfo_t vfo, tone_t *tone, const char *ct);
int ts2k_set_powerstat(RIG *rig, powerstat_t status);
int ts2k_get_powerstat(RIG *rig, powerstat_t *status);
int ts2k_reset(RIG *rig, reset_t reset);
int ts2k_send_morse(RIG *rig, vfo_t vfo, const char *msg);
int ts2k_get_ptt(RIG *rig, vfo_t vfo, ptt_t *ptt);
int ts2k_set_ptt(RIG *rig, vfo_t vfo, ptt_t ptt);
int ts2k_get_dcd(RIG *rig, vfo_t vfo, dcd_t *dcd);
int ts2k_vfo_op(RIG *rig, vfo_t vfo, vfo_op_t op);
int ts2k_set_mem(RIG *rig, vfo_t vfo, int ch);
int ts2k_get_mem(RIG *rig, vfo_t vfo, int *ch);
const char* ts2k_get_info(RIG *rig);

int ts2k_get_trn(RIG *rig, int *trn);
int ts2k_set_trn(RIG *rig, int trn);

/*
 * functions I've written -- Dale KD7ENI
 */
int ts2k_scan(RIG *rig, vfo_t vfo, scan_t scan, int ch);
int ts2k_scan_on(RIG *rig, char ch);
int ts2k_scan_off(RIG *rig);
int ts2k_get_channel(RIG *rig, vfo_t vfo, channel_t *chan); 
int ts2k_set_channel(RIG *rig, vfo_t vfo, channel_t *chan); 
char *ts2k_get_ctrl(RIG *rig);
int ts2k_set_ctrl(RIG *rig, int ptt, int ctrl);
int ts2k_vfo_ctrl(RIG *rig, vfo_t vfo);
int ts2k_get_dcs_code(RIG *rig, vfo_t vfo, tone_t *tone); 
int ts2k_set_dcs_code(RIG *rig, vfo_t vfo, tone_t  tone); 
int ts2k_get_int(char *c, int i);
int ts2k_sat_off(RIG *rig, vfo_t vfo);
int ts2k_sat_on(RIG *rig, vfo_t vfo);
int ts2k_get_parm(RIG *rig, setting_t parm, value_t *val);
int ts2k_set_parm(RIG *rig, setting_t parm, value_t val);
int ts2k_get_rit(RIG *rig, vfo_t vfo, shortfreq_t *rit);
int ts2k_set_rit(RIG *rig, vfo_t vfo, shortfreq_t  rit);
int ts2k_get_rptr_offs(RIG *rig, vfo_t vfo, shortfreq_t *offs);
int ts2k_set_rptr_offs(RIG *rig, vfo_t vfo, shortfreq_t  offs);
int ts2k_get_rptr_shift(RIG *rig, vfo_t vfo, rptr_shift_t *shift);
int ts2k_set_rptr_shift(RIG *rig, vfo_t vfo, rptr_shift_t  shift);
int ts2k_get_split(RIG *rig, vfo_t vfo, split_t *split);
int ts2k_set_split(RIG *rig, vfo_t vfo, split_t  split);
int ts2k_get_split_freq(RIG *rig, vfo_t vfo, freq_t *tx_freq);
int ts2k_set_split_freq(RIG *rig, vfo_t vfo, freq_t  tx_freq);
int ts2k_get_split_mode(RIG *rig, vfo_t vfo, rmode_t *txmode, pbwidth_t *txwidth);
int ts2k_set_split_mode(RIG *rig, vfo_t vfo, rmode_t  txmode, pbwidth_t  txwidth); 
int ts2k_get_ts(RIG *rig, vfo_t vfo, shortfreq_t *ts);
int ts2k_set_ts(RIG *rig, vfo_t vfo, shortfreq_t ts);
int ts2k_get_xit(RIG *rig, vfo_t vfo, shortfreq_t *freq);
int ts2k_set_xit(RIG *rig, vfo_t vfo, shortfreq_t  freq);

/* end kd7eni functions */

extern const struct rig_caps thd7a_caps;
extern const struct rig_caps thf7a_caps;
extern const struct rig_caps thf7e_caps;
extern const struct rig_caps ts450s_caps;
extern const struct rig_caps ts50s_caps;
extern const struct rig_caps ts570d_caps;
extern const struct rig_caps ts570s_caps;
extern const struct rig_caps ts790_caps;
extern const struct rig_caps ts850_caps;
extern const struct rig_caps ts870s_caps;
extern const struct rig_caps ts950sdx_caps;
extern const struct rig_caps ts2000_caps;

extern BACKEND_EXPORT(int) initrigs_ts2k(void *be_handle);
extern BACKEND_EXPORT(rig_model_t) proberigs_ts2k(port_t *port);


#endif /* _TS2000_H */


/************** Temporary local copy of rig.h *************************/



#ifndef _RIG_H_TEMP
#define _RIG_H_TEMP 1

#define RIG_RPT_SHIFT_1750 (RIG_RPT_SHIFT_PLUS + 1)

/*
 * I've cleaned up the VFO definition to make it easier to change
 * when the MoonMelter is finally released.  Essentially, I've
 * done nothing.	--Dale	:)
 */

/*
 * Upper segment: "rig Major"
 * Lower segment: "VFO minor"
 *
 *	MSB		    LSB
 *	N	n+1 n	      0
 *	+-+-+-+-+-+-+-+-+-+-+-+
 *	|	   |	      |
 *	    Rig		VFO
 *	   Major       minor
 */
//typedef unsigned int vfo_t;

#define BIT(a)	( ((vfo_t) 1) << (a))
//#define BIT(a)	(1L << (a))

#define RIG_MINOR	3
/* M=Major, m=minor */
#define RIG_SET_VFO(M,m)	((vfo_t) ( ((M) << (RIG_MINOR+1)) | (m) ))
/* Note: prior definition exibited exponential growth in bit count */

#define RIG_VFO_RESERVED	RIG_SET_VFO(0, BIT(0))
#define RIG_VFO_RESERVED2	RIG_SET_VFO(0, BIT(1))

/* VFO Minor */
#define RIG_VFO1	RIG_SET_VFO(0, BIT(2))
#define RIG_VFO2	RIG_SET_VFO(0, BIT(3))
#define RIG_VFO3	RIG_SET_VFO(0, BIT(4))
/*					   |
 * RIG_MINOR = n :== MAX >-----------------'
 */

/* Rig Major */
#define RIG_CTRL_MAIN	RIG_SET_VFO(BIT(0), 0)
#define RIG_CTRL_SUB	RIG_SET_VFO(BIT(1), 0)
#define RIG_CTRL_MEM	RIG_SET_VFO(BIT(2), 0)

/* Standard VFO's for common use */
#define RIG_VFO_A	(RIG_CTRL_MAIN | RIG_VFO1)
#define RIG_VFO_B	(RIG_CTRL_MAIN | RIG_VFO2)
#define RIG_VFO_C	(RIG_CTRL_SUB  | RIG_VFO1)
#define RIG_VFO_MEM	RIG_CTRL_MEM
/* VFOC should be VFO3 because ambiguities may arise someday */

/* VFO stuff that may be handy. */
#define RIG_VFO_MASK	(RIG_VFO1 | RIG_VFO2 | RIG_VFO3)
#define RIG_CTRL_MASK	(RIG_CTRL_MAIN | RIG_CTRL_SUB | RIG_CTRL_MEM)
#define RIG_VFO_VALID	(RIG_CTRL_MASK | RIG_VFO_MASK)
#define RIG_VFO_TEST(v)	(((v) & RIG_VFO_VALID) != 0)

/* The following are for compatibility with existing code! */
#define RIG_VFO_NONE	(~RIG_VFO_VALID)
#define RIG_VFO_CURR	RIG_SET_VFO(0,0)
#define RIG_VFO_ALL	RIG_VFO_MASK
#define RIG_VFO_MAIN	RIG_CTRL_MAIN
#define RIG_VFO_SUB	RIG_CTRL_SUB
#define RIG_VFO_VFO	(RIG_VFO_VALID & ~RIG_VFO_MEM)
/*
 * Ahhh.  Now I can live happy and die free! --Dale
 */

#define RIG_SCAN_VFO	(1L<<4)		/* most basic of scans! */

#define RIG_SCAN_ALL	(RIG_SCAN_STOP | RIG_SCAN_MEM | RIG_SCAN_SLCT \
			| RIG_SCAN_PRIO | RIG_SCAN_PROG | RIG_SCAN_DELTA \
			| RIG_SCAN_VFO)
#define RIG_SCAN_EXCLUDE(e) (RIG_SCAN_ALL & ~(e))

/* There's no reason for every back-end to write huge lists.  The guys
 * with about 50% features still have some work.  Someone that knows
 * many rigs should make RIG_LEVEL_COMMON, RIG_FUNC_COMMON	--Dale
 */
#define RIG_LEVEL_ALL	(RIG_LEVEL_PREAMP | RIG_LEVEL_ATT | RIG_LEVEL_VOX \
			| RIG_LEVEL_AF | RIG_LEVEL_RF | RIG_LEVEL_SQL \
	| RIG_LEVEL_IF | RIG_LEVEL_APF | RIG_LEVEL_NR | RIG_LEVEL_PBT_IN \
	| RIG_LEVEL_PBT_OUT | RIG_LEVEL_CWPITCH | RIG_LEVEL_RFPOWER \
	| RIG_LEVEL_MICGAIN | RIG_LEVEL_KEYSPD | RIG_LEVEL_NOTCHF \
	| RIG_LEVEL_COMP | RIG_LEVEL_AGC | RIG_LEVEL_BKINDL \
	| RIG_LEVEL_BALANCE | RIG_LEVEL_METER | RIG_LEVEL_VOXGAIN \
	| RIG_LEVEL_VOXDELAY | RIG_LEVEL_ANTIVOX | RIG_LEVEL_SQLSTAT \
	| RIG_LEVEL_SWR | RIG_LEVEL_ALC | RIG_LEVEL_STRENGTH )

/* simplification macro */
#define RIG_LEVEL_EXCLUDE(e) (RIG_LEVEL_ALL & ~(e))

/* more simplification macros */
#define RIG_PARM_ALL	(RIG_PARM_ANN | RIG_PARM_APO | RIG_PARM_BACKLIGHT \
			| RIG_PARM_BEEP | RIG_PARM_TIME | RIG_PARM_BAT )
#define RIG_PARM_EXCLUDE(e)	(RIG_PARM_ALL & ~(e))

/* For the Ham who has it all  --Dale */
#define RIG_FUNC_ALL	(RIG_FUNC_FAGC | RIG_FUNC_NB | RIG_FUNC_COMP \
			| RIG_FUNC_VOX | RIG_FUNC_TONE | RIG_FUNC_TSQL \
	| RIG_FUNC_SBKIN | RIG_FUNC_FBKIN | RIG_FUNC_ANF | RIG_FUNC_NR \
	| RIG_FUNC_AIP | RIG_FUNC_APF | RIG_FUNC_MON | RIG_FUNC_MN \
	| RIG_FUNC_RNF | RIG_FUNC_ARO | RIG_FUNC_LOCK | RIG_FUNC_MUTE \
	| RIG_FUNC_VSC | RIG_FUNC_REV | RIG_FUNC_SQL | RIG_FUNC_ABM \
	| RIG_FUNC_BC | RIG_FUNC_MBC | RIG_FUNC_LMP | RIG_FUNC_AFC \
	| RIG_FUNC_SATMODE | RIG_FUNC_SCOPE | RIG_FUNC_RESUME )

/* Those of us who don't have everything */
//#define RIG_FUNC_EXCLUDE(f)	(RIG_FUNC_ALL & ~(f))

#endif /* _RIG_H */

