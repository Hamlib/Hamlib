/*
 *  Hamlib tcl/tk bindings - rig
 *  Copyright (c) 2001,2002 by Stephane Fillod
 *
 *		$Id: tclrig.c,v 1.4 2002-08-26 21:26:06 fillods Exp $
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

#include <string.h>
#include <tcl.h>
#include <hamlib/rig.h>
#include <misc.h>

#include "tclrig.h"


#define ARG_IN1  0x01
#define ARG_OUT1 0x02
#define ARG_IN2  0x04
#define ARG_OUT2 0x08
#define ARG_IN3  0x10
#define ARG_OUT3 0x20
#define ARG_IN4  0x40
#define ARG_OUT4 0x80

#define ARG_IN  (ARG_IN1|ARG_IN2|ARG_IN3|ARG_IN4)
#define ARG_OUT  (ARG_OUT1|ARG_OUT2|ARG_OUT3|ARG_OUT4)

struct cmd_table {
	const char *name;
    int (*rig_routine)(RIG*, const struct cmd_table*, 
					Tcl_Interp *interp, int objc, Tcl_Obj *CONST objv[]);
	int flags;
	const char *arg1;
	const char *arg2;
	const char *arg3;
};

#define declare_proto_rig(f) static int (f)(RIG *rig, \
			const struct cmd_table *cmd, \
			Tcl_Interp *interp, int objc, Tcl_Obj *CONST objv[])

declare_proto_rig(tclrig_open);
declare_proto_rig(tclrig_close);
declare_proto_rig(tclrig_cleanup);
declare_proto_rig(set_conf);
declare_proto_rig(get_conf);
declare_proto_rig(set_freq);
declare_proto_rig(get_freq);
declare_proto_rig(set_mode);
declare_proto_rig(get_mode);
declare_proto_rig(set_vfo);
declare_proto_rig(get_vfo);
declare_proto_rig(get_info);
declare_proto_rig(get_strength);
declare_proto_rig(set_level);
declare_proto_rig(get_level);
declare_proto_rig(set_func);
declare_proto_rig(get_func);
declare_proto_rig(set_parm);
declare_proto_rig(get_parm);
declare_proto_rig(set_split_freq);
declare_proto_rig(get_split_freq);
declare_proto_rig(set_split_mode);
declare_proto_rig(get_split_mode);
declare_proto_rig(set_split);
declare_proto_rig(get_split);
declare_proto_rig(set_bank);
declare_proto_rig(set_mem);
declare_proto_rig(get_mem);
declare_proto_rig(vfo_op);
declare_proto_rig(scan);
declare_proto_rig(set_rptr_shift);
declare_proto_rig(get_rptr_shift);
declare_proto_rig(set_rptr_offs);
declare_proto_rig(get_rptr_offs);
declare_proto_rig(set_ctcss_tone);
declare_proto_rig(get_ctcss_tone);
declare_proto_rig(set_dcs_code);
declare_proto_rig(get_dcs_code);
declare_proto_rig(set_ctcss_sql);
declare_proto_rig(get_ctcss_sql);
declare_proto_rig(set_dcs_sql);
declare_proto_rig(get_dcs_sql);
declare_proto_rig(set_ts);
declare_proto_rig(get_ts);
declare_proto_rig(set_rit);
declare_proto_rig(get_rit);
declare_proto_rig(set_xit);
declare_proto_rig(get_xit);
#if 0
declare_proto_rig(set_ptt);
declare_proto_rig(get_ptt);
declare_proto_rig(get_dcd);
declare_proto_rig(power2mW);
declare_proto_rig(set_channel);
declare_proto_rig(get_channel);
declare_proto_rig(set_trn);
declare_proto_rig(get_trn);
declare_proto_rig(set_powerstat);
declare_proto_rig(get_powerstat);
declare_proto_rig(reset);
declare_proto_rig(set_ant);
declare_proto_rig(get_ant);
declare_proto_rig(send_dtmf);
declare_proto_rig(recv_dtmf);
declare_proto_rig(send_morse);
#endif

/*
 */
static struct cmd_table cmd_list[] = {
		{ "open", tclrig_open },
		{ "close", tclrig_close },
		{ "cleanup", tclrig_cleanup },
		{ "set_conf", set_conf, ARG_IN, "Token", "Value" },
		{ "get_conf", get_conf, ARG_IN1|ARG_OUT2, "Token", "Value" },
		{ "set_freq", set_freq, ARG_IN, "Frequency" },
		{ "get_freq", get_freq, ARG_OUT, "Frequency" },
		{ "set_mode", set_mode, ARG_IN, "Mode", "Passband" },
		{ "get_mode", get_mode, ARG_OUT, "Mode", "Passband" },
		{ "set_vfo", set_vfo, ARG_IN, "VFO" },
		{ "get_vfo", get_vfo, ARG_OUT, "VFO" },
		{ "get_strength", get_strength, ARG_OUT, "Strength" },
		{ "set_level", set_level, ARG_IN, "Level", "Value" },
		{ "get_level", get_level, ARG_IN1|ARG_OUT2, "Level", "Value" },
		{ "set_func", set_func, ARG_IN, "Func", "Func status" },
		{ "get_func", get_func, ARG_IN1|ARG_OUT2, "Func", "Func status" },
		{ "set_parm", set_parm, ARG_IN, "Parm", "Value" },
		{ "get_parm", get_parm, ARG_IN1|ARG_OUT2, "Parm", "Value" },
		{ "set_bank", set_bank, ARG_IN, "Bank" },
		{ "set_mem", set_mem, ARG_IN, "Memory#" },
		{ "get_mem", get_mem, ARG_OUT, "Memory#" },
		{ "vfo_op", vfo_op, ARG_IN, "Mem/VFO op" },
		{ "set_split_freq", set_split_freq, ARG_IN, "Tx frequency" },
		{ "get_split_freq", get_split_freq, ARG_OUT, "Tx frequency" },
		{ "set_split_mode", set_split_mode, ARG_IN, "Mode", "Passband" },
		{ "get_split_mode", get_split_mode, ARG_OUT, "Mode", "Passband" },
		{ "set_split", set_split, ARG_IN, "Split mode" },
		{ "get_split", get_split, ARG_OUT, "Split mode" },
		{ "scan", scan, ARG_IN, "Scan fct", "Channel" },
		{ "set_rptr_shift", set_rptr_shift, ARG_IN, "Rptr shift" },
		{ "get_rptr_shift", get_rptr_shift, ARG_OUT, "Rptr shift" },
		{ "set_rptr_offs", set_rptr_offs, ARG_IN, "Rptr offset" },
		{ "get_rptr_offs", get_rptr_offs, ARG_OUT, "Rptr offset" },
		{ "set_ctcss_tone", set_ctcss_tone, ARG_IN, "CTCSS tone" },
		{ "get_ctcss_tone", get_ctcss_tone, ARG_OUT, "CTCSS tone" },
		{ "set_dcs_code", set_dcs_code, ARG_IN, "DCS code" },
		{ "get_dcs_code", get_dcs_code, ARG_OUT, "DCS code" },
		{ "set_ctcss_sql", set_ctcss_sql, ARG_IN, "CTCSS tone" },
		{ "get_ctcss_sql", get_ctcss_sql, ARG_OUT, "CTCSS tone" },
		{ "set_dcs_sql", set_dcs_sql, ARG_IN, "DCS code" },
		{ "get_dcs_sql", get_dcs_sql, ARG_OUT, "DCS code" },
		{ "set_ts", set_ts, ARG_IN, "Tuning step" },
		{ "get_ts", get_ts, ARG_OUT, "Tuning step" },
		{ "set_rit", set_rit, ARG_IN, "RIT" },
		{ "get_rit", get_rit, ARG_OUT, "RIT" },
		{ "set_xit", set_xit, ARG_IN, "XIT" },
		{ "get_xit", get_xit, ARG_OUT, "XIT" },
#if 0
		{ "set_ptt", set_ptt, ARG_IN, "PTT" },
		{ "get_ptt", get_ptt, ARG_OUT, "PTT" },
		{ "set_channel", set_channel, ARG_IN,  /* huh! */ },
		{ "get_channel", get_channel, ARG_IN, "Channel" },
		{ "set_trn", set_trn, ARG_IN, "Transceive" },
		{ "get_trn", get_trn, ARG_OUT, "Transceive" },
		{ "power2mW", power2mW },
#endif
		{ "get_info", get_info, ARG_OUT, "Info" },
		{ NULL, NULL },

};

#define MAXNAMSIZ 32
#define MAXNBOPT 100	/* max number of different options */


/*
 * TODO: use Lex and some kind of hash table
 */
static struct cmd_table *parse_cmd(const char *name)
{
		int i;

		for (i=0; i<MAXNBOPT && cmd_list[i].name != NULL; i++)
				if (!strncmp(name, cmd_list[i].name, MAXNAMSIZ))
						return &cmd_list[i];
		return NULL;
}


/*
 * since there's no long long support in tcl API, 
 * freq_t is mapped to double object, which has enough
 * significant digits for frequencies up to tens of GHz.
 */

int
DoRigLib(ClientData clientData, Tcl_Interp *interp,
       int objc, Tcl_Obj *CONST objv[])
{
	Tcl_Obj *resultPtr;
	int    retcode;
	char *subcmd;

	/* objc gives the number of "objects" = arguments to
	the command */
	if (objc != 2 && objc != 3) {
		Tcl_WrongNumArgs(interp, 1, objv, "cmd ?model?");
		return TCL_ERROR;
	}
	
	/* Process the arguments
	objv is an array holding the arguments */

  /* Use Tcl_GetIntFromObj to put an integer value
       into a variable from the argument list */

	subcmd = Tcl_GetString(objv[1]);
	resultPtr = Tcl_GetObjResult(interp);

	/* TODO: make use of flex here! */

	if (!strcasecmp(subcmd, "version")) {
		Tcl_SetStringObj(resultPtr, hamlib_version, -1);
	} else if (!strcasecmp(subcmd, "help")) {
		Tcl_SetStringObj(resultPtr, "no help yet!", -1);
	} else if (!strcasecmp(subcmd, "init")) {
		rig_model_t model;
		RIG *tcl_rig;
		char rigcmd_name[16];
		static int rigcmd_cpt=0;

		retcode = Tcl_GetIntFromObj(interp, objv[2], (int*)&model);
		if (retcode != TCL_OK)
				return retcode;
		tcl_rig = rig_init(model);
		if (!tcl_rig)
				return TCL_ERROR;
		sprintf(rigcmd_name, "rig%d", rigcmd_cpt++);
		Tcl_CreateObjCommand(interp, rigcmd_name, DoRig,
	       (ClientData)tcl_rig, (Tcl_CmdDeleteProc *)NULL);
		Tcl_SetStringObj(resultPtr, rigcmd_name, -1);
	} else {
		return TCL_ERROR;
	}
	return TCL_OK;
}

int
DoRig(ClientData clientData, Tcl_Interp *interp,
       int objc, Tcl_Obj *CONST objv[])
{
	int    retcode;
	char *subcmd;
	RIG *tcl_rig = (RIG*)clientData;
	struct cmd_table *cmd_entry;

	if (!tcl_rig)
			return TCL_ERROR;

	/* objc gives the number of "objects" = arguments to
	the command */
	if (objc < 2) {
		Tcl_WrongNumArgs(interp, 1, objv, "var1 var2 ?var3?");
		return TCL_ERROR;
	}
	
	/* Process the arguments
	objv is an array holding the arguments */

  /* Use Tcl_GetIntFromObj to put an integer value
       into a variable from the argument list */

	subcmd = Tcl_GetString(objv[1]);
	cmd_entry = parse_cmd(subcmd);
	if (!cmd_entry) {
			/* unknow subcmd */
		return TCL_ERROR;
	}

	retcode = (*cmd_entry->rig_routine)(tcl_rig, 
					cmd_entry, interp, objc, objv);
				
	if (retcode != RIG_OK ) {
			/* FIXME: set Tcl error, and retcode can be TCL_ERROR! */
		fprintf(stderr, "%s: %s\n", cmd_entry->name, rigerror(retcode));
		return TCL_ERROR;
	}

	return TCL_OK;
}

/*
 * static int (f)(RIG *rig, const struct cmd_table *cmd, 
 *			Tcl_Interp *interp, int objc, Tcl_Obj *CONST objv[])
 */

declare_proto_rig(tclrig_open)
{
		return rig_open(rig);
}

declare_proto_rig(tclrig_close)
{
		return rig_close(rig);
}

/*
 * FIXME: delete rig%d tcl command
 */
declare_proto_rig(tclrig_cleanup)
{
		return rig_cleanup(rig);
}

declare_proto_rig(set_conf)
{
		token_t tok;

		tok = rig_token_lookup(rig, Tcl_GetString(objv[2]));

		return rig_set_conf(rig, tok, Tcl_GetString(objv[3]));
}

declare_proto_rig(get_conf)
{
		int status;
		token_t tok;
		Tcl_Obj *resultPtr;
		char buf[255];

		resultPtr = Tcl_GetObjResult(interp);

		tok = rig_token_lookup(rig, Tcl_GetString(objv[2]));

		status = rig_get_conf(rig, tok, buf);
		if (status != RIG_OK)
				return status;

		Tcl_SetStringObj(resultPtr, buf, -1);

		return RIG_OK;
}

declare_proto_rig(set_freq)
{
		int status;
		double d;
		freq_t freq;

		status = Tcl_GetDoubleFromObj(interp, objv[2], &d);
		if (status != TCL_OK)
			return status;
		freq = (freq_t)d;

		return rig_set_freq(rig, RIG_VFO_CURR, freq);
}

declare_proto_rig(get_freq)
{
		int status;
		freq_t freq;
		Tcl_Obj *resultPtr;

		resultPtr = Tcl_GetObjResult(interp);

		status = rig_get_freq(rig, RIG_VFO_CURR, &freq);
		if (status != RIG_OK)
				return status;
		Tcl_SetDoubleObj(resultPtr, (double)freq);

		return RIG_OK;
}

declare_proto_rig(get_strength)
{
		int status;
		value_t val;
		Tcl_Obj *resultPtr;

		resultPtr = Tcl_GetObjResult(interp);

		status = rig_get_strength(rig, RIG_VFO_CURR, &val);
		if (status != RIG_OK)
				return status;
		Tcl_SetIntObj(resultPtr, val.i);

		return RIG_OK;
}

declare_proto_rig(set_mode)
{
		int status;
		rmode_t mode;
		pbwidth_t width;

		mode = parse_mode(Tcl_GetString(objv[2]));
		status = Tcl_GetLongFromObj(interp, objv[3], &width);
		if (status != TCL_OK)
			return status;
		return rig_set_mode(rig, RIG_VFO_CURR, mode, width);
}


declare_proto_rig(get_mode)
{
		int status;
		rmode_t mode;
		pbwidth_t width;
		Tcl_Obj *resultPtr;

		resultPtr = Tcl_GetObjResult(interp);

		status = rig_get_mode(rig, RIG_VFO_CURR, &mode, &width);
		if (status != RIG_OK)
				return status;

		/* FIXME: return also width */
		Tcl_SetStringObj(resultPtr, strrmode(mode), -1);

		return RIG_OK;
}


declare_proto_rig(set_vfo)
{
		vfo_t vfo;

		vfo = parse_vfo(Tcl_GetString(objv[2]));
		return rig_set_vfo(rig, vfo);
}


declare_proto_rig(get_vfo)
{
		int status;
		vfo_t vfo;
		Tcl_Obj *resultPtr;

		resultPtr = Tcl_GetObjResult(interp);

		vfo = parse_vfo(Tcl_GetString(objv[2]));
		status = rig_get_vfo(rig, &vfo);
		if (status != RIG_OK)
				return status;

		Tcl_SetStringObj(resultPtr, strvfo(vfo), -1);
		return RIG_OK;
}

declare_proto_rig(set_split)
{
		split_t split;
		int arg;

		Tcl_GetIntFromObj(interp, objv[2], &arg);
		split = arg ? RIG_SPLIT_ON : RIG_SPLIT_OFF;
		return rig_set_split(rig, RIG_VFO_CURR, split);
}


declare_proto_rig(get_split)
{
		int status;
		split_t split;
		Tcl_Obj *resultPtr;

		resultPtr = Tcl_GetObjResult(interp);

		status = rig_get_split(rig, RIG_VFO_CURR, &split);
		if (status != RIG_OK)
				return status;

		Tcl_SetIntObj(resultPtr, split);
		return RIG_OK;
}

declare_proto_rig(scan)
{
		scan_t scan;
		int ch;

		scan = parse_scan(Tcl_GetString(objv[2]));
		Tcl_GetIntFromObj(interp, objv[3], &ch);
		return rig_scan(rig, RIG_VFO_CURR, scan, ch);
}


declare_proto_rig(vfo_op)
{
		vfo_op_t op;

		op = parse_vfo_op(Tcl_GetString(objv[2]));
		return rig_vfo_op(rig, RIG_VFO_CURR, op);
}


declare_proto_rig(set_bank)
{
		int bank;

		Tcl_GetIntFromObj(interp, objv[2], &bank);
		return rig_set_bank(rig, RIG_VFO_CURR, bank);
}


declare_proto_rig(set_mem)
{
		int ch;

		Tcl_GetIntFromObj(interp, objv[2], &ch);
		return rig_set_mem(rig, RIG_VFO_CURR, ch);
}


declare_proto_rig(get_mem)
{
		int status;
		int ch;
		Tcl_Obj *resultPtr;

		resultPtr = Tcl_GetObjResult(interp);

		status = rig_get_mem(rig, RIG_VFO_CURR, &ch);
		if (status != RIG_OK)
				return status;

		Tcl_SetIntObj(resultPtr, ch);
		return RIG_OK;
}



declare_proto_rig(set_split_freq)
{
		int status;
		double d;
		freq_t freq;

		status = Tcl_GetDoubleFromObj(interp, objv[2], &d);
		if (status != TCL_OK)
			return status;
		freq = (freq_t)d;

		return rig_set_split_freq(rig, RIG_VFO_CURR, freq);
}

declare_proto_rig(get_split_freq)
{
		int status;
		freq_t freq;
		Tcl_Obj *resultPtr;

		resultPtr = Tcl_GetObjResult(interp);

		status = rig_get_split_freq(rig, RIG_VFO_CURR, &freq);
		if (status != RIG_OK)
				return status;
		Tcl_SetDoubleObj(resultPtr, (double)freq);

		return RIG_OK;
}

declare_proto_rig(set_split_mode)
{
		int status;
		rmode_t mode;
		pbwidth_t width;

		mode = parse_mode(Tcl_GetString(objv[2]));
		status = Tcl_GetLongFromObj(interp, objv[3], &width);
		if (status != TCL_OK)
			return status;
		return rig_set_split_mode(rig, RIG_VFO_CURR, mode, width);
}


declare_proto_rig(get_split_mode)
{
		int status;
		rmode_t mode;
		pbwidth_t width;
		Tcl_Obj *resultPtr;

		resultPtr = Tcl_GetObjResult(interp);

		status = rig_get_split_mode(rig, RIG_VFO_CURR, &mode, &width);
		if (status != RIG_OK)
				return status;

		/* FIXME: return also width */
		Tcl_SetStringObj(resultPtr, strrmode(mode), -1);

		return RIG_OK;
}

declare_proto_rig(set_rptr_shift)
{
		rptr_shift_t rptr_shift;

		rptr_shift = parse_rptr_shift(Tcl_GetString(objv[2]));
		return rig_set_rptr_shift(rig, RIG_VFO_CURR, rptr_shift);
}


declare_proto_rig(get_rptr_shift)
{
		int status;
		rptr_shift_t rptr_shift;
		Tcl_Obj *resultPtr;

		resultPtr = Tcl_GetObjResult(interp);

		status = rig_get_rptr_shift(rig, RIG_VFO_CURR, &rptr_shift);
		if (status != RIG_OK)
				return status;

		/* FIXME: return also width */
		Tcl_SetStringObj(resultPtr, strptrshift(rptr_shift), -1);

		return RIG_OK;
}

declare_proto_rig(set_rptr_offs)
{
		shortfreq_t offs;

		Tcl_GetLongFromObj(interp, objv[2], &offs);
		return rig_set_rptr_offs(rig, RIG_VFO_CURR, offs);
}


declare_proto_rig(get_rptr_offs)
{
		int status;
		shortfreq_t offs;
		Tcl_Obj *resultPtr;

		resultPtr = Tcl_GetObjResult(interp);

		status = rig_get_rptr_offs(rig, RIG_VFO_CURR, &offs);
		if (status != RIG_OK)
				return status;

		Tcl_SetLongObj(resultPtr, offs);
		return RIG_OK;
}

declare_proto_rig(set_rit)
{
		shortfreq_t rit;

		Tcl_GetLongFromObj(interp, objv[2], &rit);
		return rig_set_rit(rig, RIG_VFO_CURR, rit);
}


declare_proto_rig(get_rit)
{
		int status;
		shortfreq_t rit;
		Tcl_Obj *resultPtr;

		resultPtr = Tcl_GetObjResult(interp);

		status = rig_get_rit(rig, RIG_VFO_CURR, &rit);
		if (status != RIG_OK)
				return status;

		Tcl_SetLongObj(resultPtr, rit);
		return RIG_OK;
}

declare_proto_rig(set_xit)
{
		shortfreq_t xit;

		Tcl_GetLongFromObj(interp, objv[2], &xit);
		return rig_set_xit(rig, RIG_VFO_CURR, xit);
}


declare_proto_rig(get_xit)
{
		int status;
		shortfreq_t xit;
		Tcl_Obj *resultPtr;

		resultPtr = Tcl_GetObjResult(interp);

		status = rig_get_xit(rig, RIG_VFO_CURR, &xit);
		if (status != RIG_OK)
				return status;

		Tcl_SetLongObj(resultPtr, xit);
		return RIG_OK;
}


declare_proto_rig(set_ts)
{
		shortfreq_t ts;

		Tcl_GetLongFromObj(interp, objv[2], &ts);
		return rig_set_ts(rig, RIG_VFO_CURR, ts);
}


declare_proto_rig(get_ts)
{
		int status;
		shortfreq_t ts;
		Tcl_Obj *resultPtr;

		resultPtr = Tcl_GetObjResult(interp);

		status = rig_get_ts(rig, RIG_VFO_CURR, &ts);
		if (status != RIG_OK)
				return status;

		Tcl_SetLongObj(resultPtr, ts);
		return RIG_OK;
}

declare_proto_rig(set_dcs_code)
{
		tone_t code;

		Tcl_GetIntFromObj(interp, objv[2], &code);
		return rig_set_dcs_code(rig, RIG_VFO_CURR, code);
}


declare_proto_rig(get_dcs_code)
{
		int status;
		tone_t code;
		Tcl_Obj *resultPtr;

		resultPtr = Tcl_GetObjResult(interp);

		status = rig_get_dcs_code(rig, RIG_VFO_CURR, &code);
		if (status != RIG_OK)
				return status;

		Tcl_SetIntObj(resultPtr, code);
		return RIG_OK;
}

declare_proto_rig(set_dcs_sql)
{
		tone_t code;

		Tcl_GetIntFromObj(interp, objv[2], &code);
		return rig_set_dcs_sql(rig, RIG_VFO_CURR, code);
}


declare_proto_rig(get_dcs_sql)
{
		int status;
		tone_t code;
		Tcl_Obj *resultPtr;

		resultPtr = Tcl_GetObjResult(interp);

		status = rig_get_dcs_sql(rig, RIG_VFO_CURR, &code);
		if (status != RIG_OK)
				return status;

		Tcl_SetIntObj(resultPtr, code);
		return RIG_OK;
}


declare_proto_rig(set_ctcss_tone)
{
		tone_t code;

		Tcl_GetIntFromObj(interp, objv[2], &code);
		return rig_set_ctcss_tone(rig, RIG_VFO_CURR, code);
}


declare_proto_rig(get_ctcss_tone)
{
		int status;
		tone_t code;
		Tcl_Obj *resultPtr;

		resultPtr = Tcl_GetObjResult(interp);

		status = rig_get_ctcss_tone(rig, RIG_VFO_CURR, &code);
		if (status != RIG_OK)
				return status;

		Tcl_SetIntObj(resultPtr, code);
		return RIG_OK;
}

declare_proto_rig(set_ctcss_sql)
{
		tone_t code;

		Tcl_GetIntFromObj(interp, objv[2], &code);
		return rig_set_ctcss_sql(rig, RIG_VFO_CURR, code);
}


declare_proto_rig(get_ctcss_sql)
{
		int status;
		tone_t code;
		Tcl_Obj *resultPtr;

		resultPtr = Tcl_GetObjResult(interp);

		status = rig_get_ctcss_sql(rig, RIG_VFO_CURR, &code);
		if (status != RIG_OK)
				return status;

		Tcl_SetIntObj(resultPtr, code);
		return RIG_OK;
}


declare_proto_rig(get_info)
{
		const char *s;
		Tcl_Obj *resultPtr;

		resultPtr = Tcl_GetObjResult(interp);

		s = rig_get_info(rig);
		Tcl_SetStringObj(resultPtr, s ? s : "None", -1);

		return RIG_OK;
}


declare_proto_rig(set_func)
{
		int status;
		setting_t func;
		int stat;

		func = parse_func(Tcl_GetString(objv[2]));
		status = Tcl_GetIntFromObj(interp, objv[3], &stat);
		if (status != TCL_OK)
			return status;
		return rig_set_func(rig, RIG_VFO_CURR, func, stat);
}

declare_proto_rig(get_func)
{
		int status;
		setting_t func;
		int stat;
		Tcl_Obj *resultPtr;

		resultPtr = Tcl_GetObjResult(interp);

		func = parse_func(Tcl_GetString(objv[2]));
		status = rig_get_func(rig, RIG_VFO_CURR, func, &stat);
		if (status != RIG_OK)
				return status;

		Tcl_SetIntObj(resultPtr, stat);
		return RIG_OK;
}

declare_proto_rig(set_level)
{
		int status;
		setting_t level;
		value_t val;
		double d;

		level = parse_level(Tcl_GetString(objv[2]));
		if (RIG_LEVEL_IS_FLOAT(level)) {
			status = Tcl_GetDoubleFromObj(interp, objv[3], &d);
			val.f = d;
		} else {
			status = Tcl_GetIntFromObj(interp, objv[3], &val.i);
		}
		if (status != TCL_OK)
			return status;
		return rig_set_level(rig, RIG_VFO_CURR, level, val);
}

declare_proto_rig(get_level)
{
		int status;
		setting_t level;
		value_t val;
		double d;
		Tcl_Obj *resultPtr;

		resultPtr = Tcl_GetObjResult(interp);

		level = parse_level(Tcl_GetString(objv[2]));
		status = rig_get_level(rig, RIG_VFO_CURR, level, &val);
		if (status != RIG_OK)
				return status;

		if (RIG_LEVEL_IS_FLOAT(level)) {
			d = val.f;
			Tcl_SetDoubleObj(resultPtr, d);
		} else {
			Tcl_SetIntObj(resultPtr, val.i);
		}
		return RIG_OK;
}

declare_proto_rig(set_parm)
{
		int status;
		setting_t parm;
		value_t val;
		double d;

		parm = parse_parm(Tcl_GetString(objv[2]));
		if (RIG_PARM_IS_FLOAT(parm)) {
			status = Tcl_GetDoubleFromObj(interp, objv[3], &d);
			val.f = d;
		} else {
			status = Tcl_GetIntFromObj(interp, objv[3], &val.i);
		}
		if (status != TCL_OK)
			return status;
		return rig_set_parm(rig, parm, val);
}

declare_proto_rig(get_parm)
{
		int status;
		setting_t parm;
		value_t val;
		double d;
		Tcl_Obj *resultPtr;

		resultPtr = Tcl_GetObjResult(interp);

		parm = parse_parm(Tcl_GetString(objv[2]));
		status = rig_get_parm(rig, parm, &val);
		if (status != RIG_OK)
				return status;

		if (RIG_PARM_IS_FLOAT(parm)) {
			d = val.f;
			Tcl_SetDoubleObj(resultPtr, d);
		} else {
			Tcl_SetIntObj(resultPtr, val.i);
		}
		return RIG_OK;
}





#if 0

declare_proto_rig(set_ptt)
{
		ptt_t ptt;

		sscanf(arg1, "%d", (int*)&ptt);
		return rig_set_ptt(rig, RIG_VFO_CURR, ptt);
}


declare_proto_rig(get_ptt)
{
		int status;
		ptt_t ptt;

		status = rig_get_ptt(rig, RIG_VFO_CURR, &ptt);
		if (status != RIG_OK)
				return status;
		if (interactive)
			printf("%s: ", cmd->arg1);
		printf("%d\n", ptt);
		return status;
}


declare_proto_rig(set_rptr_shift)
{
		rptr_shift_t rptr_shift;

		rptr_shift = parse_rptr_shift(arg1);
		return rig_set_rptr_shift(rig, RIG_VFO_CURR, rptr_shift);
}


declare_proto_rig(get_rptr_shift)
{
		int status;
		rptr_shift_t rptr_shift;

		status = rig_get_rptr_shift(rig, RIG_VFO_CURR, &rptr_shift);
		if (status != RIG_OK)
				return status;
		if (interactive)
			printf("%s: ", cmd->arg1);
		printf("%s\n", strptrshift(rptr_shift));
		return status;
}


declare_proto_rig(set_rptr_offs)
{
		unsigned long rptr_offs;

		sscanf(arg1, "%ld", &rptr_offs);
		return rig_set_rptr_offs(rig, RIG_VFO_CURR, rptr_offs);
}


declare_proto_rig(get_rptr_offs)
{
		int status;
		unsigned long rptr_offs;

		status = rig_get_rptr_offs(rig, RIG_VFO_CURR, &rptr_offs);
		if (status != RIG_OK)
				return status;
		if (interactive)
			printf("%s: ", cmd->arg1);
		printf("%ld\n", rptr_offs);
		return status;
}


declare_proto_rig(set_ctcss_tone)
{
		tone_t tone;

		sscanf(arg1, "%d", &tone);
		return rig_set_ctcss_tone(rig, RIG_VFO_CURR, tone);
}


declare_proto_rig(get_ctcss_tone)
{
		int status;
		tone_t tone;

		status = rig_get_ctcss_tone(rig, RIG_VFO_CURR, &tone);
		if (status != RIG_OK)
				return status;
		if (interactive)
			printf("%s: ", cmd->arg1);
		printf("%d\n", tone);
		return status;
}


declare_proto_rig(set_dcs_code)
{
		tone_t code;

		sscanf(arg1, "%d", &code);
		return rig_set_dcs_code(rig, RIG_VFO_CURR, code);
}


declare_proto_rig(get_dcs_code)
{
		int status;
		tone_t code;

		status = rig_get_dcs_code(rig, RIG_VFO_CURR, &code);
		if (status != RIG_OK)
				return status;
		if (interactive)
			printf("%s: ", cmd->arg1);
		printf("%d\n", code);
		return status;
}


declare_proto_rig(set_split_freq)
{
		freq_t txfreq;

		sscanf(arg1, "%lld", &txfreq);
		return rig_set_split_freq(rig, RIG_VFO_CURR, txfreq);
}


declare_proto_rig(get_split_freq)
{
		int status;
		freq_t txfreq;

		status = rig_get_split_freq(rig, RIG_VFO_CURR, &txfreq);
		if (status != RIG_OK)
				return status;
		if (interactive)
			printf("%s: ", cmd->arg1);
		printf("%lld\n", txfreq);
		return status;
}

declare_proto_rig(set_split_mode)
{
		rmode_t mode;
		pbwidth_t width;

		mode = parse_mode(arg1);
		sscanf(arg2, "%d", (int*)&width);
		return rig_set_split_mode(rig, RIG_VFO_CURR, mode, width);
}


declare_proto_rig(get_split_mode)
{
		int status;
		rmode_t mode;
		pbwidth_t width;

		status = rig_get_split_mode(rig, RIG_VFO_CURR, &mode, &width);
		if (status != RIG_OK)
				return status;
		if (interactive)
			printf("%s: ", cmd->arg1);
		printf("%s\n", strrmode(mode));
		if (interactive)
			printf("%s: ", cmd->arg2);
		printf("%ld\n", width);
		return status;
}


declare_proto_rig(set_split)
{
		split_t split;

		sscanf(arg1, "%d", (int*)&split);
		return rig_set_split(rig, RIG_VFO_CURR, split);
}


declare_proto_rig(get_split)
{
		int status;
		split_t split;

		status = rig_get_split(rig, RIG_VFO_CURR, &split);
		if (status != RIG_OK)
				return status;
		if (interactive)
			printf("%s: ", cmd->arg1);
		printf("%d\n", split);
		return status;
}


declare_proto_rig(set_ts)
{
		unsigned long ts;

		sscanf(arg1, "%ld", &ts);
		return rig_set_ts(rig, RIG_VFO_CURR, ts);
}


declare_proto_rig(get_ts)
{
		int status;
		unsigned long ts;

		status = rig_get_ts(rig, RIG_VFO_CURR, &ts);
		if (status != RIG_OK)
				return status;
		if (interactive)
			printf("%s: ", cmd->arg1);
		printf("%ld\n", ts);
		return status;
}

declare_proto_rig(power2mW)
{
		int status;
		float power;
		freq_t freq;
		rmode_t mode;
		unsigned int mwp;

		printf("Power [0.0 .. 1.0]: ");
		scanf("%f", &power);
		printf("Frequency: ");
		scanf("%lld", &freq);
		printf("Mode: ");
		scanf("%d", &mode);
		status = rig_power2mW(rig, &mwp, power, freq, mode);
		printf("Power: %d mW\n", mwp);
		return status;
}


declare_proto_rig(set_level)
{
		setting_t level;
		value_t val;

		level = parse_level(arg1);
		if (RIG_LEVEL_IS_FLOAT(level))
			sscanf(arg2, "%f", &val.f);
		else
			sscanf(arg2, "%d", &val.i);

		return rig_set_level(rig, RIG_VFO_CURR, level, val);
}


declare_proto_rig(get_level)
{
		int status;
		setting_t level;
		value_t val;

		level = parse_level(arg1);
		status = rig_get_level(rig, RIG_VFO_CURR, level, &val);
		if (status != RIG_OK)
				return status;
		if (interactive)
			printf("%s: ", cmd->arg2);
		if (RIG_LEVEL_IS_FLOAT(level))
			printf("%f\n", val.f);
		else
			printf("%d\n", val.i);

		return status;
}


declare_proto_rig(set_func)
{
		setting_t func;
		int func_stat;

		func = parse_func(arg1);
		sscanf(arg2, "%d", (int*)&func_stat);
		return rig_set_func(rig, RIG_VFO_CURR, func, func_stat);
}


declare_proto_rig(get_func)
{
		int status;
		setting_t func;
		int func_stat;

		func = parse_func(arg1);
		status = rig_get_func(rig, RIG_VFO_CURR, func, &func_stat);
		if (status != RIG_OK)
				return status;
		if (interactive)
			printf("%s: ", cmd->arg2);
		printf("%d\n", func_stat);
		return status;
}

declare_proto_rig(set_parm)
{
		setting_t parm;
		value_t val;

		parm = parse_parm(arg1);
		if (RIG_PARM_IS_FLOAT(parm))
			sscanf(arg2, "%f", &val.f);
		else
			sscanf(arg2, "%d", &val.i);

		return rig_set_parm(rig, parm, val);
}


declare_proto_rig(get_parm)
{
		int status;
		setting_t parm;
		value_t val;

		parm = parse_parm(arg1);
		status = rig_get_parm(rig, parm, &val);
		if (status != RIG_OK)
				return status;
		if (interactive)
			printf("%s: ", cmd->arg2);
		if (RIG_PARM_IS_FLOAT(parm))
			printf("%f\n", val.f);
		else
			printf("%d\n", val.i);

		return status;
}


declare_proto_rig(set_bank)
{
		int bank;

		sscanf(arg1, "%d", &bank);
		return rig_set_bank(rig, RIG_VFO_CURR, bank);
}


declare_proto_rig(set_mem)
{
		int ch;

		sscanf(arg1, "%d", &ch);
		return rig_set_mem(rig, RIG_VFO_CURR, ch);
}


declare_proto_rig(get_mem)
{
		int status;
		int ch;

		status = rig_get_mem(rig, RIG_VFO_CURR, &ch);
		if (status != RIG_OK)
				return status;
		if (interactive)
			printf("%s: ", cmd->arg1);
		printf("%d\n", ch);
		return status;
}

declare_proto_rig(vfo_op)
{
		vfo_op_t op;

		op = parse_vfo_op(arg1);
		return rig_vfo_op(rig, RIG_VFO_CURR, op);
}

declare_proto_rig(scan)
{
		scan_t op;
		int ch;

		op = parse_scan(arg1);
		sscanf(arg2, "%d", &ch);
		return rig_scan(rig, RIG_VFO_CURR, op, ch);
}

declare_proto_rig(set_channel)
{
		fprintf(stderr,"rigctl_set_channel not implemented yet!\n");
		return -RIG_ENIMPL;
}


declare_proto_rig(get_channel)
{
		int status;
		channel_t chan;

		sscanf(arg1, "%d", &chan.channel_num);
		status = rig_get_channel(rig, &chan);
		if (status != RIG_OK)
				return status;
		/* TODO: dump data here */
		return status;
}


declare_proto_rig(set_trn)
{
		int trn;

		sscanf(arg1, "%d", &trn);
		return rig_set_trn(rig, trn);
}


declare_proto_rig(get_trn)
{
		int status;
		int trn;

		status = rig_get_trn(rig, &trn);
		if (status != RIG_OK)
				return status;
		if (interactive)
			printf("%s: ", cmd->arg1);
		printf("%d\n", trn);
		return status;
}
#endif

