/*
 * rigctl_parse.c - (C) Stephane Fillod 2000-2008
 *
 * This program test/control a radio using Hamlib.
 * It takes commands in interactive mode as well as 
 * from command line options.
 *
 * $Id: rigctl_parse.c,v 1.5 2008-04-27 09:57:04 fillods Exp $  
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

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <ctype.h>
#include <errno.h>

#include <getopt.h>

#include <hamlib/rig.h>
#include "misc.h"
#include "iofunc.h"
#include "serial.h"
#include "sprintflst.h"

#include "rigctl_parse.h"

#ifdef HAVE_PTHREAD
#include <pthread.h>

static pthread_mutex_t rig_mutex = PTHREAD_MUTEX_INITIALIZER;
#endif

#define MAXNAMSIZ 32
#define MAXNBOPT 100	/* max number of different options */


#define ARG_IN1  0x01
#define ARG_OUT1 0x02
#define ARG_IN2  0x04
#define ARG_OUT2 0x08
#define ARG_IN3  0x10
#define ARG_OUT3 0x20
#define ARG_IN4  0x40
#define ARG_OUT4 0x80
#define ARG_IN_LINE 0x4000
#define ARG_NOVFO 0x8000

#define ARG_IN  (ARG_IN1|ARG_IN2|ARG_IN3|ARG_IN4)
#define ARG_OUT  (ARG_OUT1|ARG_OUT2|ARG_OUT3|ARG_OUT4)

struct test_table {
	unsigned char cmd;
	const char *name;
	int (*rig_routine)(RIG*, FILE*, int, const struct test_table*, vfo_t, 
			const char*, const char*, const char*);
	int flags;
	const char *arg1;
	const char *arg2;
	const char *arg3;
};


#define declare_proto_rig(f) static int (f)(RIG *rig, FILE *fout, int interactive, \
			const struct test_table *cmd, vfo_t vfo, const char *arg1, \
			const char *arg2, const char *arg3)

declare_proto_rig(set_freq);
declare_proto_rig(get_freq);
declare_proto_rig(set_rit);
declare_proto_rig(get_rit);
declare_proto_rig(set_xit);
declare_proto_rig(get_xit);
declare_proto_rig(set_mode);
declare_proto_rig(get_mode);
declare_proto_rig(set_vfo);
declare_proto_rig(get_vfo);
declare_proto_rig(set_ptt);
declare_proto_rig(get_ptt);
declare_proto_rig(get_ptt);
declare_proto_rig(set_rptr_shift);
declare_proto_rig(get_rptr_shift);
declare_proto_rig(set_rptr_offs);
declare_proto_rig(get_rptr_offs);
declare_proto_rig(set_ctcss_tone);
declare_proto_rig(get_ctcss_tone);
declare_proto_rig(set_dcs_code);
declare_proto_rig(get_dcs_code);
declare_proto_rig(set_split_freq);
declare_proto_rig(get_split_freq);
declare_proto_rig(set_split_mode);
declare_proto_rig(get_split_mode);
declare_proto_rig(set_split_vfo);
declare_proto_rig(get_split_vfo);
declare_proto_rig(set_ts);
declare_proto_rig(get_ts);
declare_proto_rig(power2mW);
declare_proto_rig(set_level);
declare_proto_rig(get_level);
declare_proto_rig(set_func);
declare_proto_rig(get_func);
declare_proto_rig(set_parm);
declare_proto_rig(get_parm);
declare_proto_rig(set_bank);
declare_proto_rig(set_mem);
declare_proto_rig(get_mem);
declare_proto_rig(vfo_op);
declare_proto_rig(scan);
declare_proto_rig(set_channel);
declare_proto_rig(get_channel);
declare_proto_rig(set_trn);
declare_proto_rig(get_trn);
declare_proto_rig(get_info);
declare_proto_rig(dump_caps);
declare_proto_rig(set_ant);
declare_proto_rig(get_ant);
declare_proto_rig(reset);
declare_proto_rig(send_morse);
declare_proto_rig(send_cmd);
declare_proto_rig(set_powerstat);
declare_proto_rig(get_powerstat);



/*
 * convention: upper case cmd is set, lowercase is get
 *
 * TODO: add missing rig_set_/rig_get_: sql, dcd, etc.
 * NB: 'q' 'Q' '?' are reserved by interactive mode interface
 * 		do NOT use -W since it's reserved by POSIX.
 *
 *	Available alphabetic letters: -.--------K-----*-----W-Y-
 */
static struct test_table test_list[] = {
	{ 'F', "set_freq", set_freq, ARG_IN, "Frequency" },
	{ 'f', "get_freq", get_freq, ARG_OUT, "Frequency" },
	{ 'M', "set_mode", set_mode, ARG_IN, "Mode", "Passband" },
	{ 'm', "get_mode", get_mode, ARG_OUT, "Mode", "Passband" },
	{ 'I', "set_split_freq", set_split_freq, ARG_IN, "Tx frequency" },
	{ 'i', "get_split_freq", get_split_freq, ARG_OUT, "Tx frequency" },
	{ 'X', "set_split_mode", set_split_mode, ARG_IN, "Mode", "Passband" },
	{ 'x', "get_split_mode", get_split_mode, ARG_OUT, "Mode", "Passband" },
	{ 'S', "set_split_vfo", set_split_vfo, ARG_IN, "Split mode", "TxVFO" },
	{ 's', "get_split_vfo", get_split_vfo, ARG_OUT, "Split mode", "TxVFO" },
	{ 'N', "set_ts", set_ts, ARG_IN, "Tuning step" },
	{ 'n', "get_ts", get_ts, ARG_OUT, "Tuning step" },
	{ 'L', "set_level", set_level, ARG_IN, "Level", "Value" },
	{ 'l', "get_level", get_level, ARG_IN1|ARG_OUT2, "Level", "Value" },
	{ 'U', "set_func", set_func, ARG_IN, "Func", "Func status" },
	{ 'u', "get_func", get_func, ARG_IN1|ARG_OUT2, "Func", "Func status" },
	{ 'P', "set_parm", set_parm, ARG_IN|ARG_NOVFO, "Parm", "Value" },
	{ 'p', "get_parm", get_parm, ARG_IN1|ARG_OUT2|ARG_NOVFO, "Parm", "Value" },
	{ 'G', "vfo_op", vfo_op, ARG_IN, "Mem/VFO op" },
	{ 'g', "scan", scan, ARG_IN, "Scan fct", "Channel" },
	{ 'A', "set_trn", set_trn, ARG_IN|ARG_NOVFO, "Transceive" },
	{ 'a', "get_trn", get_trn, ARG_OUT|ARG_NOVFO, "Transceive" },
	{ 'R', "set_rptr_shift", set_rptr_shift, ARG_IN, "Rptr shift" },
	{ 'r', "get_rptr_shift", get_rptr_shift, ARG_OUT, "Rptr shift" },
	{ 'O', "set_rptr_offs", set_rptr_offs, ARG_IN, "Rptr offset" },
	{ 'o', "get_rptr_offs", get_rptr_offs, ARG_OUT, "Rptr offset" },
	{ 'C', "set_ctcss_tone", set_ctcss_tone, ARG_IN, "CTCSS tone" },
	{ 'c', "get_ctcss_tone", get_ctcss_tone, ARG_OUT, "CTCSS tone" },
	{ 'D', "set_dcs_code", set_dcs_code, ARG_IN, "DCS code" },
	{ 'd', "get_dcs_code", get_dcs_code, ARG_OUT, "DCS code" },
	{ 'V', "set_vfo", set_vfo, ARG_IN|ARG_NOVFO, "VFO" },
	{ 'v', "get_vfo", get_vfo, ARG_OUT, "VFO" },
	{ 'T', "set_ptt", set_ptt, ARG_IN, "PTT" },
	{ 't', "get_ptt", get_ptt, ARG_OUT, "PTT" },
	{ 'E', "set_mem", set_mem, ARG_IN, "Memory#" },
	{ 'e', "get_mem", get_mem, ARG_OUT, "Memory#" },
	{ 'H', "set_channel", set_channel, ARG_IN|ARG_NOVFO, "Channel" },
	{ 'h', "get_channel", get_channel, ARG_IN|ARG_NOVFO, "Channel" },
	{ 'B', "set_bank", set_bank, ARG_IN, "Bank" },
	{ '_', "get_info", get_info, ARG_OUT|ARG_NOVFO, "Info" },
	{ 'J', "set_rit", set_rit, ARG_IN, "RIT" },
	{ 'j', "get_rit", get_rit, ARG_OUT, "RIT" },
	{ 'Z', "set_xit", set_xit, ARG_IN, "XIT" },
	{ 'z', "get_xit", get_xit, ARG_OUT, "XIT" },
	{ 'Y', "set_ant", set_ant, ARG_IN, "Antenna" },
	{ 'y', "get_ant", get_ant, ARG_OUT, "Antenna" },
	{ 0x87, "set_powerstat", set_powerstat, ARG_IN|ARG_NOVFO, "Status" },
	{ 0x88, "get_powerstat", get_powerstat, ARG_OUT|ARG_NOVFO, "Status" },
	{ '*', "reset", reset, ARG_IN, "Reset" },
	{ '2', "power2mW", power2mW, ARG_NOVFO },
	{ 'w', "send_cmd", send_cmd, ARG_IN1|ARG_IN_LINE|ARG_OUT2|ARG_NOVFO, "Cmd", "Reply" },
	{ '1', "dump_caps", dump_caps, ARG_NOVFO },
	{ 'b', "send_morse", send_morse, ARG_IN|ARG_IN_LINE, "Morse" },
	/* next one is 0x89 */
	{ 0x00, "", NULL },

};

static struct test_table *find_cmd_entry(int cmd)
{
	int i;
	for (i=0; i<MAXNBOPT && test_list[i].cmd != 0x00; i++)
		if (test_list[i].cmd == cmd)
			break;

	if (i >= MAXNBOPT || test_list[i].cmd == 0x00)
		return NULL;

	return &test_list[i];
}
/*
 * TODO: use Lex?
 */
static char parse_arg(const char *arg)
{
	int i;
	for (i=0; i<MAXNBOPT && test_list[i].cmd != 0; i++)
		if (!strncmp(arg, test_list[i].name, MAXNAMSIZ))
			return test_list[i].cmd;
	return 0;
}


/* 
 * This scanf works even in presence of signals (timer, SIGIO, ..)
 */
static int scanfc(FILE *fin, const char *format, void *p)
{
	int ret;

	do {
		ret = fscanf(fin, format, p);
		if (ret < 0) {
			if (errno == EINTR)
				continue;
			rig_debug(RIG_DEBUG_ERR, "fscanf: %s\n", strerror(errno));
		}
		return ret;
	} while(1);
}

#define MAXARGSZ 127

extern int interactive;
extern int prompt;


int rigctl_parse(RIG *my_rig, FILE *fin, FILE *fout, char *argv[], int argc)
{
	int retcode;		/* generic return code from functions */
	unsigned char cmd;
	struct test_table *cmd_entry;

	char arg1[MAXARGSZ+1], *p1;
	char arg2[MAXARGSZ+1], *p2;
	char arg3[MAXARGSZ+1], *p3;
	static int last_was_ret = 1;
	vfo_t vfo = RIG_VFO_CURR;
	int vfo_mode = 0;

	if (interactive) {
		if (prompt)
			fprintf(fout, "\nRig command: ");

		do {
			if (scanfc(fin, "%c", &cmd) < 0)
				return -1;

			/* command by name */
			if (cmd == '\\') {
				unsigned char cmd_name[MAXNAMSIZ], *pcmd = cmd_name;
				int c_len = MAXNAMSIZ;

				if (scanfc(fin, "%c", pcmd) < 0)
					return -1;

				while(c_len-- && (isalnum(*pcmd) || *pcmd == '_' ))
					if (scanfc(fin, "%c", ++pcmd) < 0)
						return -1;

				*pcmd = '\0';
				cmd = parse_arg((char *) cmd_name);
				break;
			}

			if (cmd == 0x0a || cmd == 0x0d) {
				if (last_was_ret) {
					if (prompt) {
						fprintf(fout, "? for help, q to quit.\n");
						fprintf(fout, "\nRig command: ");
					}
					return 0;
				}
				last_was_ret = 1;
			}
		} while (cmd == 0x0a || cmd == 0x0d);

		last_was_ret = 0;

		/* comment line */
		if (cmd == '#' || cmd == ';') {
			while( cmd != '\n' && cmd != '\r')
				if (scanfc(fin, "%c", &cmd) < 0)
					return -1;
			return 0;
		}
		if (cmd == 'Q' || cmd == 'q')
				return 1;
		if (cmd == '?') {
			usage_rig(fout);
			return 0;
		}
	} else {
		/* parse rest of command line */
		if (optind >= argc)
			return 1;
		if (argv[optind][1] == '\0')
			cmd = argv[optind][0];
		else
			cmd = parse_arg(argv[optind]);
		optind++;
	}

	cmd_entry = find_cmd_entry(cmd);
	if (!cmd_entry) {
		fprintf(stderr, "Command '%c' not found!\n", cmd);
		return 0;
	}

	p1 = p2 = p3 = NULL;
	if (!(cmd_entry->flags & ARG_NOVFO) && vfo_mode) {
		if (interactive) {
			if (prompt)
				fprintf(fout, "VFO: ");
			if (scanfc(fin, "%s", arg1) < 0)
				return -1;
			vfo = rig_parse_vfo(arg1);
		} else {
			if (!argv[optind]) {
				fprintf(stderr, "Invalid arg for command '%s'\n", 
							cmd_entry->name);
				exit(2);
			}
			vfo = rig_parse_vfo(argv[optind++]);
		}
	}

	if ((cmd_entry->flags & ARG_IN_LINE) && 
			(cmd_entry->flags & ARG_IN1) && cmd_entry->arg1) {
		if (interactive) {
			char *nl;
			if (prompt)
				fprintf(fout, "%s: ", cmd_entry->arg1);
			fgets(arg1, MAXARGSZ, fin);
			if (arg1[0] == 0xa)
				fgets(arg1, MAXARGSZ, fin);
			nl = strchr(arg1, 0xa);
			if (nl) *nl = '\0';	/* chomp */
			p1 = arg1[0]==' '?arg1+1:arg1;
		} else {
			if (!argv[optind]) {
				fprintf(stderr, "Invalid arg for command '%s'\n", 
							cmd_entry->name);
				exit(2);
			}
			p1 = argv[optind++];
		}
	} else 
	  if ((cmd_entry->flags & ARG_IN1) && cmd_entry->arg1) {
		if (interactive) {
			if (prompt)
				fprintf(fout, "%s: ", cmd_entry->arg1);
			if (scanfc(fin, "%s", arg1) < 0)
				return -1;
			p1 = arg1;
		} else {
			if (!argv[optind]) {
				fprintf(stderr, "Invalid arg for command '%s'\n", 
							cmd_entry->name);
				exit(2);
			}
			p1 = argv[optind++];
		}
	}
	if ((cmd_entry->flags & ARG_IN2) && cmd_entry->arg2) {
		if (interactive) {
			if (prompt)
				fprintf(fout, "%s: ", cmd_entry->arg2);
			if (scanfc(fin, "%s", arg2) < 0)
				return -1;
			p2 = arg2;
		} else {
			if (!argv[optind]) {
				fprintf(stderr, "Invalid arg for command '%s'\n", 
							cmd_entry->name);
				exit(2);
			}
			p2 = argv[optind++];
		}
	}
	if ((cmd_entry->flags & ARG_IN3) && cmd_entry->arg3) {
		if (interactive) {
			if (prompt)
				fprintf(fout, "%s: ", cmd_entry->arg3);
			if (scanfc(fin, "%s", arg3) < 0)
				return -1;
			p3 = arg3;
		} else {
			if (!argv[optind]) {
				fprintf(stderr, "Invalid arg for command '%s'\n", 
							cmd_entry->name);
				exit(2);
			}
			p3 = argv[optind++];
		}
	}

	/*
	 * mutex locking needed because rigctld is multithreaded
	 * and hamlib is not MT-safe
	 */
#ifdef HAVE_PTHREAD
	pthread_mutex_lock(&rig_mutex);
#endif

	if (!prompt)
		rig_debug(RIG_DEBUG_TRACE, "rigctl: %c '%s' '%s' '%s'\n",
				cmd, p1, p2, p3);

	retcode = (*cmd_entry->rig_routine)(my_rig, fout, interactive, 
					cmd_entry, vfo, p1, p2, p3);

#ifdef HAVE_PTHREAD
	pthread_mutex_unlock(&rig_mutex);
#endif

	fflush(fout);
		
	if (retcode != RIG_OK) {
  		fprintf(fout, "%s: error = %s\n", cmd_entry->name, rigerror(retcode));
	}

	return 0;
}


void version()
{
	printf("rigctl, %s\n\n", hamlib_version);
	printf("%s\n", hamlib_copyright);
}

void usage_rig(FILE *fout)
{
	int i;

	fprintf(fout, "Commands (may not be available for this rig):\n");
	for (i=0; test_list[i].cmd != 0; i++) {
		fprintf(fout, "%c: %-16s(", isprint(test_list[i].cmd) ? 
				test_list[i].cmd:'?', test_list[i].name);

		if (test_list[i].arg1 && (test_list[i].flags&ARG_IN1))
				fprintf(fout, "%s", test_list[i].arg1);
		if (test_list[i].arg2 && (test_list[i].flags&ARG_IN2))
				fprintf(fout, ",%s", test_list[i].arg2);
		if (test_list[i].arg3 && (test_list[i].flags&ARG_IN3))
				fprintf(fout, ",%s", test_list[i].arg3);
		fprintf(fout, ")  \t");

		if (i%2)
			fprintf(fout, "\n");
	}
}


int print_conf_list(const struct confparams *cfp, rig_ptr_t data)
{
	RIG *rig = (RIG*) data;
	int i;
	char buf[128] = "";

	rig_get_conf(rig, cfp->token, buf);
	printf("%s: \"%s\"\n" "\t"
			"Default: %s, Value: %s\n", 
			cfp->name, cfp->tooltip, 
			cfp->dflt, buf );

	switch (cfp->type) {
	case RIG_CONF_NUMERIC:
		printf("\tRange: %.1f..%.1f, step %.1f\n", 
				cfp->u.n.min, cfp->u.n.max, cfp->u.n.step);
		break;
	case RIG_CONF_COMBO:
		if (!cfp->u.c.combostr)
			break;
		printf("\tCombo: %s", cfp->u.c.combostr[0]);
		for (i=1 ; i<RIG_COMBO_MAX && cfp->u.c.combostr[i]; i++)
			printf(", %s", cfp->u.c.combostr[i]);
		printf("\n");
		break;
	case RIG_CONF_STRING:
		printf("\tString.\n");
		break;
	case RIG_CONF_CHECKBUTTON:
		printf("\tCheck button.\n");
		break;
	default:
		printf("\tUnkown conf\n");
	}

	return 1;  /* !=0, we want them all ! */
}

static int print_model_list(const struct rig_caps *caps, void *data)
{
	printf("%d\t%-16s%-24s%-8s%s\n", caps->rig_model, caps->mfg_name,
			caps->model_name, caps->version, rig_strstatus(caps->status));
	return 1;  /* !=0, we want them all ! */
}

void list_models()
{
	int status;

	rig_load_all_backends();

	printf("Rig#\tMfg             Model                   Vers.   Status\n");
	status = rig_list_foreach(print_model_list, NULL);
	if (status != RIG_OK ) {
		printf("rig_list_foreach: error = %s \n", rigerror(status));
		exit(2);
	}
}


int set_conf(RIG *my_rig, char *conf_parms)
{
	char *p, *q, *n;
	int ret;

	p = conf_parms;
	while (p && *p != '\0') {
			/* FIXME: left hand value of = cannot be null */
		q = strchr(p, '=');
		if (q) *q++ = '\0';
		n = strchr(q, ',');
		if (n) *n++ = '\0';

		ret = rig_set_conf(my_rig, rig_token_lookup(my_rig, p), q);
		if (ret != RIG_OK)
			return ret;
		p = n;
	}
	return RIG_OK;
}


/*
 * static int (f)(RIG *rig, FILE *fout, int interactive, const struct test_table *cmd, 
 * 		vfo_t vfo, const void *arg1, const void *arg2, const void *arg3)
 */

/* 'F' */
declare_proto_rig(set_freq)
{
	freq_t freq;

	sscanf(arg1, "%"SCNfreq, &freq);
	return rig_set_freq(rig, vfo, freq);
}

/* 'f' */
declare_proto_rig(get_freq)
{
	int status;
	freq_t freq;

	status = rig_get_freq(rig, vfo, &freq);
	if (status != RIG_OK)
		return status;

	if (interactive && prompt)
		fprintf(fout, "%s: ", cmd->arg1); /* i.e. "Frequency" */
    fprintf(fout, "%"PRIll"\n", (long long)freq);

	if (interactive && !prompt)             /* only for rigctld */
		fprintf(fout, "END\n");

	return status;
}

/* 'J' */
declare_proto_rig(set_rit)
{
	shortfreq_t rit;

	sscanf(arg1, "%ld", &rit);
	return rig_set_rit(rig, vfo, rit);
}

/* 'j' */
declare_proto_rig(get_rit)
{
	int status;
	shortfreq_t rit;

	status = rig_get_rit(rig, vfo, &rit);
	if (status != RIG_OK)
		return status;
	if (interactive && prompt)
		fprintf(fout, "%s: ", cmd->arg1);
	fprintf(fout, "%ld\n", rit);

	if (interactive && !prompt)             /* only for rigctld */
		fprintf(fout, "END\n");

	return status;
}

/* 'Z' */
declare_proto_rig(set_xit)
{
	shortfreq_t xit;

	sscanf(arg1, "%ld", &xit);
	return rig_set_xit(rig, vfo, xit);
}

/* 'z' */
declare_proto_rig(get_xit)
{
	int status;
	shortfreq_t xit;

	status = rig_get_xit(rig, vfo, &xit);
	if (status != RIG_OK)
		return status;
	if (interactive && prompt)
		fprintf(fout, "%s: ", cmd->arg1);
	fprintf(fout, "%ld\n", xit);

	if (interactive && !prompt)             /* only for rigctld */
		fprintf(fout, "END\n");

	return status;
}

/* 'M' */
declare_proto_rig(set_mode)
{
	rmode_t mode;
	pbwidth_t width;

	mode = rig_parse_mode(arg1);
	sscanf(arg2, "%ld", &width);
	return rig_set_mode(rig, vfo, mode, width);
}

/* 'm' */
declare_proto_rig(get_mode)
{
	int status;
	rmode_t mode;
	pbwidth_t width;

	status = rig_get_mode(rig, vfo, &mode, &width);
	if (status != RIG_OK)
		return status;
	if (interactive && prompt)
		fprintf(fout, "%s: ", cmd->arg1);
	fprintf(fout, "%s\n", rig_strrmode(mode));
	if (interactive && prompt)
		fprintf(fout, "%s: ", cmd->arg2);
	fprintf(fout, "%ld\n", width);

	if (interactive && !prompt)             /* only for rigctld */
		fprintf(fout, "END\n");

	return status;
}

/* 'V' */
declare_proto_rig(set_vfo)
{
	return rig_set_vfo(rig, rig_parse_vfo(arg1));
}

/* 'v' */
declare_proto_rig(get_vfo)
{
	int status;

	status = rig_get_vfo(rig, &vfo);
	if (status != RIG_OK)
		return status;
	if (interactive && prompt)
		fprintf(fout, "%s: ", cmd->arg1);
	fprintf(fout, "%s\n", rig_strvfo(vfo));

	if (interactive && !prompt)             /* only for rigctld */
		fprintf(fout, "END\n");

	return status;
}

/* 'T' */
declare_proto_rig(set_ptt)
{
        int   ptt;

	sscanf(arg1, "%d", &ptt);
	return rig_set_ptt(rig, vfo, (ptt_t) ptt);
}

/* 't' */
declare_proto_rig(get_ptt)
{
	int status;
	ptt_t ptt;

	status = rig_get_ptt(rig, vfo, &ptt);
	if (status != RIG_OK)
		return status;
	if (interactive && prompt)
		fprintf(fout, "%s: ", cmd->arg1);
	fprintf(fout, "%d\n", ptt);

	if (interactive && !prompt)             /* only for rigctld */
		fprintf(fout, "END\n");

	return status;
}

/* 'R' */
declare_proto_rig(set_rptr_shift)
{
	rptr_shift_t rptr_shift;

	rptr_shift = rig_parse_rptr_shift(arg1);
	return rig_set_rptr_shift(rig, vfo, rptr_shift);
}

/* 'r' */
declare_proto_rig(get_rptr_shift)
{
	int status;
	rptr_shift_t rptr_shift;

	status = rig_get_rptr_shift(rig, vfo, &rptr_shift);
	if (status != RIG_OK)
		return status;
	if (interactive && prompt)
		fprintf(fout, "%s: ", cmd->arg1);
	fprintf(fout, "%s\n", rig_strptrshift(rptr_shift));

	if (interactive && !prompt)             /* only for rigctld */
		fprintf(fout, "END\n");

	return status;
}

/* 'O' */
declare_proto_rig(set_rptr_offs)
{
	unsigned long rptr_offs;

	sscanf(arg1, "%ld", &rptr_offs);
	return rig_set_rptr_offs(rig, vfo, rptr_offs);
}

/* 'o' */
declare_proto_rig(get_rptr_offs)
{
	int status;
	shortfreq_t rptr_offs;

	status = rig_get_rptr_offs(rig, vfo, &rptr_offs);
	if (status != RIG_OK)
		return status;
	if (interactive && prompt)
		fprintf(fout, "%s: ", cmd->arg1);
	fprintf(fout, "%ld\n", rptr_offs);

	if (interactive && !prompt)             /* only for rigctld */
		fprintf(fout, "END\n");

	return status;
}

/* 'C' */
declare_proto_rig(set_ctcss_tone)
{
	tone_t tone;

	sscanf(arg1, "%d", &tone);
	return rig_set_ctcss_tone(rig, vfo, tone);
}

/* 'c' */
declare_proto_rig(get_ctcss_tone)
{
	int status;
	tone_t tone;

	status = rig_get_ctcss_tone(rig, vfo, &tone);
	if (status != RIG_OK)
		return status;
	if (interactive && prompt)
		fprintf(fout, "%s: ", cmd->arg1);
	fprintf(fout, "%d\n", tone);

	if (interactive && !prompt)             /* only for rigctld */
		fprintf(fout, "END\n");

	return status;
}

/* 'D' */
declare_proto_rig(set_dcs_code)
{
	tone_t code;

	sscanf(arg1, "%d", &code);
	return rig_set_dcs_code(rig, vfo, code);
}

/* 'd' */
declare_proto_rig(get_dcs_code)
{
	int status;
	tone_t code;

	status = rig_get_dcs_code(rig, vfo, &code);
	if (status != RIG_OK)
		return status;
	if (interactive && prompt)
		fprintf(fout, "%s: ", cmd->arg1);
	fprintf(fout, "%d\n", code);

	if (interactive && !prompt)             /* only for rigctld */
		fprintf(fout, "END\n");

	return status;
}

/* 'I' */
declare_proto_rig(set_split_freq)
{
	freq_t txfreq;

	sscanf(arg1, "%"SCNfreq, &txfreq);
	return rig_set_split_freq(rig, vfo, txfreq);
}

/* 'i' */
declare_proto_rig(get_split_freq)
{
	int status;
	freq_t txfreq;

	status = rig_get_split_freq(rig, vfo, &txfreq);
	if (status != RIG_OK)
		return status;
	if (interactive && prompt)
		fprintf(fout, "%s: ", cmd->arg1);
	fprintf(fout, "%"PRIll"\n", (long long)txfreq);

	if (interactive && !prompt)             /* only for rigctld */
		fprintf(fout, "END\n");

	return status;
}

/* 'X' */
declare_proto_rig(set_split_mode)
{
	rmode_t mode;
	int     width;

	mode = rig_parse_mode(arg1);
	sscanf(arg2, "%d", &width);
	return rig_set_split_mode(rig, vfo, mode, (pbwidth_t) width);
}

/* 'x' */
declare_proto_rig(get_split_mode)
{
	int status;
	rmode_t mode;
	pbwidth_t width;

	status = rig_get_split_mode(rig, vfo, &mode, &width);
	if (status != RIG_OK)
		return status;
	if (interactive && prompt)
		fprintf(fout, "%s: ", cmd->arg1);
	fprintf(fout, "%s\n", rig_strrmode(mode));
	if (interactive && prompt)
		fprintf(fout, "%s: ", cmd->arg2);
	fprintf(fout, "%ld\n", width);

	if (interactive && !prompt)             /* only for rigctld */
		fprintf(fout, "END\n");

	return status;
}

/* 'S' */
declare_proto_rig(set_split_vfo)
{
	int split;

	sscanf(arg1, "%d", &split);
	return rig_set_split_vfo(rig, vfo, (split_t) split, rig_parse_vfo(arg2));
}

/* 's' */
declare_proto_rig(get_split_vfo)
{
	int status;
	split_t split;
	vfo_t tx_vfo;

	status = rig_get_split_vfo(rig, vfo, &split, &tx_vfo);
	if (status != RIG_OK)
		return status;
	if (interactive && prompt)
		fprintf(fout, "%s: ", cmd->arg1);
	fprintf(fout, "%d\n", split);
	if (interactive && prompt)
		fprintf(fout, "%s: ", cmd->arg2);
	fprintf(fout, "%s\n", rig_strvfo(tx_vfo));

	if (interactive && !prompt)             /* only for rigctld */
		fprintf(fout, "END\n");

	return status;
}

/* 'N' */
declare_proto_rig(set_ts)
{
	unsigned long ts;

	sscanf(arg1, "%ld", &ts);
	return rig_set_ts(rig, vfo, ts);
}

/* 'n' */
declare_proto_rig(get_ts)
{
	int status;
	shortfreq_t ts;

	status = rig_get_ts(rig, vfo, &ts);
	if (status != RIG_OK)
		return status;
	if (interactive && prompt)
		fprintf(fout, "%s: ", cmd->arg1);
	fprintf(fout, "%ld\n", ts);

	if (interactive && !prompt)             /* only for rigctld */
		fprintf(fout, "END\n");

	return status;
}

/* '2' */
/* FIXME: For rigctld */
declare_proto_rig(power2mW)
{
	int status;
	float power;
	freq_t freq;
	int mode;
	unsigned int mwp;

	fprintf(fout, "Power [0.0 .. 1.0]: ");
	scanf("%f", &power);
	fprintf(fout, "Frequency: ");
	scanf("%"SCNfreq, &freq);
	fprintf(fout, "Mode: ");
	scanf("%d", &mode);
	status = rig_power2mW(rig, &mwp, power, freq, (rmode_t) mode);
	fprintf(fout, "Power: %d mW\n", mwp);

    if (interactive && !prompt)             /* only for rigctld */
		fprintf(fout, "END\n");

	return status;
}

/*
 * RIG_CONF_ extparm's type:
 *   NUMERIC: val.f
 *   COMBO: val.i, starting from 0
 *   STRING: val.s
 *   CHECKBUTTON: val.i 0/1
 *
 * 'L'
 */
declare_proto_rig(set_level)
{
	setting_t level;
	value_t val;

	level = rig_parse_level(arg1);
	if (!rig_has_set_level(rig, level)) {
		const struct confparams *cfp;

		cfp = rig_ext_lookup(rig, arg1);
		if (!cfp)
			return -RIG_EINVAL;	/* no such parameter */

		switch (cfp->type) {
		case RIG_CONF_CHECKBUTTON:
		case RIG_CONF_COMBO:
			sscanf(arg2, "%d", &val.i);
			break;
		case RIG_CONF_NUMERIC:
			sscanf(arg2, "%f", &val.f);
			break;
		case RIG_CONF_STRING:
			val.cs = arg2;
			break;
		default:
			return -RIG_ECONF;
		}
		return rig_set_ext_level(rig, vfo, cfp->token, val);
	}

	if (RIG_LEVEL_IS_FLOAT(level))
		sscanf(arg2, "%f", &val.f);
	else
		sscanf(arg2, "%d", &val.i);

	return rig_set_level(rig, vfo, level, val);
}

/* 'l' */
declare_proto_rig(get_level)
{
	int status;
	setting_t level;
	value_t val;

	level = rig_parse_level(arg1);
	if (!rig_has_get_level(rig, level)) {
		const struct confparams *cfp;

		cfp = rig_ext_lookup(rig, arg1);
		if (!cfp)
			return -RIG_EINVAL;	/* no such parameter */

		status = rig_get_ext_level(rig, vfo, cfp->token, &val);
		if (status != RIG_OK)
			return status;

		if (interactive && prompt)
			fprintf(fout, "%s: ", cmd->arg2);

		switch (cfp->type) {
		case RIG_CONF_CHECKBUTTON:
		case RIG_CONF_COMBO:
			fprintf(fout, "%d\n", val.i);
			break;
		case RIG_CONF_NUMERIC:
			fprintf(fout, "%f\n", val.f);
			break;
		case RIG_CONF_STRING:
			fprintf(fout, "%s\n", val.s);
			break;
		default:
			return -RIG_ECONF;
		}
		return status;
	}

	status = rig_get_level(rig, vfo, level, &val);
	if (status != RIG_OK)
		return status;
	if (interactive && prompt)
		fprintf(fout, "%s: ", cmd->arg2);
	if (RIG_LEVEL_IS_FLOAT(level))
		fprintf(fout, "%f\n", val.f);
	else
		fprintf(fout, "%d\n", val.i);

	if (interactive && !prompt)             /* only for rigctld */
		fprintf(fout, "END\n");

	return status;
}

/* 'U' */
declare_proto_rig(set_func)
{
	setting_t func;
	int func_stat;

	func = rig_parse_func(arg1);
	sscanf(arg2, "%d", (int*)&func_stat);
	return rig_set_func(rig, vfo, func, func_stat);
}

/* 'u' */
declare_proto_rig(get_func)
{
	int status;
	setting_t func;
	int func_stat;

	func = rig_parse_func(arg1);
	status = rig_get_func(rig, vfo, func, &func_stat);
	if (status != RIG_OK)
		return status;
	if (interactive && prompt)
		fprintf(fout, "%s: ", cmd->arg2);
	fprintf(fout, "%d\n", func_stat);

	if (interactive && !prompt)             /* only for rigctld */
		fprintf(fout, "END\n");

	return status;
}

/* 'P' */
declare_proto_rig(set_parm)
{
	setting_t parm;
	value_t val;

	parm = rig_parse_parm(arg1);

	if (!rig_has_set_parm(rig, parm)) {
		const struct confparams *cfp;

		cfp = rig_ext_lookup(rig, arg1);
		if (!cfp)
			return -RIG_EINVAL;	/* no such parameter */

		switch (cfp->type) {
		case RIG_CONF_CHECKBUTTON:
		case RIG_CONF_COMBO:
			sscanf(arg2, "%d", &val.i);
			break;
		case RIG_CONF_NUMERIC:
			sscanf(arg2, "%f", &val.f);
			break;
		case RIG_CONF_STRING:
			val.cs = arg2;
			break;
		default:
			return -RIG_ECONF;
		}
		return rig_set_ext_parm(rig, cfp->token, val);
	}

	if (RIG_PARM_IS_FLOAT(parm))
		sscanf(arg2, "%f", &val.f);
	else
		sscanf(arg2, "%d", &val.i);

	return rig_set_parm(rig, parm, val);
}

/* 'p' */
declare_proto_rig(get_parm)
{
	int status;
	setting_t parm;
	value_t val;

	parm = rig_parse_parm(arg1);
	if (!rig_has_get_parm(rig, parm)) {
		const struct confparams *cfp;

		cfp = rig_ext_lookup(rig, arg1);
		if (!cfp)
			return -RIG_EINVAL;	/* no such parameter */

		status = rig_get_ext_parm(rig, cfp->token, &val);
		if (status != RIG_OK)
			return status;

		if (interactive && prompt)
			fprintf(fout, "%s: ", cmd->arg2);

		switch (cfp->type) {
		case RIG_CONF_CHECKBUTTON:
		case RIG_CONF_COMBO:
			fprintf(fout, "%d\n", val.i);
			break;
		case RIG_CONF_NUMERIC:
			fprintf(fout, "%f\n", val.f);
			break;
		case RIG_CONF_STRING:
			fprintf(fout, "%s\n", val.s);
			break;
		default:
			return -RIG_ECONF;
		}
		return status;
	}

	status = rig_get_parm(rig, parm, &val);
	if (status != RIG_OK)
		return status;
	if (interactive && prompt)
		fprintf(fout, "%s: ", cmd->arg2);
	if (RIG_PARM_IS_FLOAT(parm))
		fprintf(fout, "%f\n", val.f);
	else
		fprintf(fout, "%d\n", val.i);

	if (interactive && !prompt)             /* only for rigctld */
		fprintf(fout, "END\n");

	return status;
}

/* 'B' */
declare_proto_rig(set_bank)
{
	int bank;

	sscanf(arg1, "%d", &bank);
	return rig_set_bank(rig, vfo, bank);
}

/* 'E' */
declare_proto_rig(set_mem)
{
	int ch;

	sscanf(arg1, "%d", &ch);
	return rig_set_mem(rig, vfo, ch);
}

/* 'e' */
declare_proto_rig(get_mem)
{
	int status;
	int ch;

	status = rig_get_mem(rig, vfo, &ch);
	if (status != RIG_OK)
		return status;
	if (interactive && prompt)
		fprintf(fout, "%s: ", cmd->arg1);
	fprintf(fout, "%d\n", ch);

    if (interactive && !prompt)             /* only for rigctld */
		fprintf(fout, "END\n");

	return status;
}

/* 'G' */
declare_proto_rig(vfo_op)
{
	vfo_op_t op;

	op = rig_parse_vfo_op(arg1);
	return rig_vfo_op(rig, vfo, op);
}

/* 'g' */
declare_proto_rig(scan)
{
	scan_t op;
	int ch;

	op = rig_parse_scan(arg1);
	sscanf(arg2, "%d", &ch);
	return rig_scan(rig, vfo, op, ch);
}

/* 'H' */
declare_proto_rig(set_channel)
{
	const channel_cap_t *mem_caps = NULL;
	const chan_t *chan_list;
	channel_t chan;

	return -RIG_ENIMPL;

	if (isdigit(arg1[0])) {
		chan.vfo = RIG_VFO_MEM;
		if (sscanf(arg1, "%d", &chan.channel_num) != 1)
			return -RIG_EINVAL;
		/*
		 * find mem_caps in caps, we'll need it later
		 */
		chan_list = rig_lookup_mem_caps(rig, chan.channel_num);
		if (chan_list)
			mem_caps = &chan_list->mem_caps;

	} else {
		chan.vfo = rig_parse_vfo(arg1);
		chan.channel_num = 0;

		/* TODO: mem_caps for VFO! */
	}

	if (!mem_caps)
		return -RIG_ECONF;

#ifdef FIXME
	if (mem_caps->bank_num)
		sscanf(arg1, "%d", &chan.bank_num);
	if (mem_caps->vfo)
		chan.vfo = rig_parse_vfo(arg1);
	if (mem_caps->ant)
		sscanf(arg1, "%d", &chan.ant);
	if (mem_caps->freq)
		sscanf(arg1, "%"SCNfreq, &chan.freq);
	if (mem_caps->mode)
		chan.mode = rig_parse_mode(arg1);
	if (mem_caps->width)
		sscanf(arg1, "%d", (int*)&chan.width);

	if (mem_caps->tx_freq)
		sscanf(arg1, "%"SCNfreq, &chan.tx_freq);
	if (mem_caps->tx_mode)
		chan.tx_mode = rig_parse_mode(arg1);
	if (mem_caps->tx_width)
		sscanf(arg1, "%d", (int*)&chan.tx_width);

	if (mem_caps->split)
		sscanf(arg1, "%d", &chan.split);
	if (mem_caps->tx_vfo)
		chan.tx_vfo = rig_parse_vfo(arg1);
	if (mem_caps->rptr_shift)
		chan.rptr_shift = rig_parse_rptr_shift(arg1);
	if (mem_caps->rptr_offs)
		sscanf(arg1, "%d", &chan.rptr_offs);
	if (mem_caps->tuning_step)
		sscanf(arg1, "%d", &chan.tuning_step);
	if (mem_caps->rit)
		sscanf(arg1, "%d", &chan.rit);
	if (mem_caps->xit)
		sscanf(arg1, "%d", &chan.xit);
	if (mem_caps->funcs)
		sscanf(arg1, "%x", &chan.funcs);
#if 0
		/* for all levels, ask */
	if (mem_caps->levels)
		sscanf(arg1, "%d", &chan.levels);
#endif
	if (mem_caps->ctcss_tone)
		sscanf(arg1, "%d", &chan.ctcss_tone);
	if (mem_caps->ctcss_sql)
		sscanf(arg1, "%d", &chan.ctcss_sql);
	if (mem_caps->dcs_code)
		sscanf(arg1, "%d", &chan.dcs_code);
	if (mem_caps->dcs_sql)
		sscanf(arg1, "%d", &chan.dcs_sql);
	if (mem_caps->scan_group)
		sscanf(arg1, "%d", &chan.scan_group);
	if (mem_caps->flags)
		sscanf(arg1, "%d", &chan.flags);
	if (mem_caps->channel_desc)
		strcpy(chan.channel_desc, arg1);
#if 0
	/* TODO: same as levels */
	if (mem_caps->ext_levels)
		sscanf(arg1, "%d", &chan.ext_levels);
#endif

#endif

	return RIG_OK;
}

/* 'h' */
declare_proto_rig(get_channel)
{
	int status;
	channel_t chan;

	if (isdigit(arg1[0])) {
		chan.vfo = RIG_VFO_MEM;
		if (sscanf(arg1, "%d", &chan.channel_num) != 1)
			return -RIG_EINVAL;
	} else {
		chan.vfo = rig_parse_vfo(arg1);
		chan.channel_num = 0;
	}

	status = rig_get_channel(rig, &chan);
	if (status != RIG_OK)
		return status;
	dump_chan(fout, rig, &chan);

	if (interactive && !prompt)             /* only for rigctld */
		fprintf(fout, "END\n");

	return status;
}

static int myfreq_event(RIG *rig, vfo_t vfo, freq_t freq, rig_ptr_t arg)
{
	printf("Event: freq changed to %"PRIll"Hz on %s\n", (long long)freq, rig_strvfo(vfo));
	return 0;
}

static int mymode_event(RIG *rig, vfo_t vfo, rmode_t mode, pbwidth_t width, rig_ptr_t arg)
{
	printf("Event: mode changed to %s, width %liHz on %s\n", rig_strrmode(mode),
			width, rig_strvfo(vfo));
	return 0;
}

static int myvfo_event(RIG *rig, vfo_t vfo, rig_ptr_t arg)
{
	printf("Event: vfo changed to %s\n", rig_strvfo(vfo));
	return 0;
}

static int myptt_event(RIG *rig, vfo_t vfo, ptt_t ptt, rig_ptr_t arg)
{
	printf("Event: PTT changed to %i on %s\n", ptt, rig_strvfo(vfo));
	return 0;
}

static int mydcd_event(RIG *rig, vfo_t vfo, dcd_t dcd, rig_ptr_t arg)
{
	printf("Event: DCD changed to %i on %s\n", dcd, rig_strvfo(vfo));
	return 0;
}

/* 'A' */
declare_proto_rig(set_trn)
{
	int trn;

	if (!strcmp(arg1, "OFF"))
		trn = RIG_TRN_OFF;
	else if (!strcmp(arg1, "RIG") || !strcmp(arg1, "ON"))
		trn = RIG_TRN_RIG;
	else if (!strcmp(arg1, "POLL"))
		trn = RIG_TRN_POLL;
	else
		return -RIG_EINVAL;

	if (trn != RIG_TRN_OFF) {
		rig_set_freq_callback(rig, myfreq_event, NULL);
		rig_set_mode_callback(rig, mymode_event, NULL);
		rig_set_vfo_callback (rig, myvfo_event, NULL);
		rig_set_ptt_callback (rig, myptt_event, NULL);
		rig_set_dcd_callback (rig, mydcd_event, NULL);
	}

	return rig_set_trn(rig, trn);
}

/* 'a' */
declare_proto_rig(get_trn)
{
	int status;
	int trn;

	status = rig_get_trn(rig, &trn);
	if (status != RIG_OK)
		return status;
	if (interactive && prompt)
		fprintf(fout, "%s: ", cmd->arg1);
	fprintf(fout, "%d\n", trn);

	if (interactive && !prompt)             /* only for rigctld */
		fprintf(fout, "END\n");

	return status;
}

/* '_' */
declare_proto_rig(get_info)
{
	const char *s;

	s = rig_get_info(rig);
	if (interactive && prompt)
		fprintf(fout, "%s: ", cmd->arg1);
	fprintf(fout, "%s\n", s ? s : "None");

	if (interactive && !prompt)             /* only for rigctld */
		fprintf(fout, "END\n");

	return RIG_OK;
}


void dump_chan(FILE *fout, RIG *rig, channel_t *chan)
{
	int idx, firstloop=1;
	char freqbuf[16];
	char widthbuf[16];
	char prntbuf[256];

	fprintf(fout, "Channel: %d, Name: '%s'\n", chan->channel_num, 
			chan->channel_desc);

	fprintf(fout, "VFO: %s, Antenna: %d, Split: %s\n", rig_strvfo(chan->vfo),
			chan->ant, chan->split==RIG_SPLIT_ON?"ON":"OFF");

	sprintf_freq(freqbuf, chan->freq);
	sprintf_freq(widthbuf, chan->width);
	fprintf(fout, "Freq:   %s\tMode:   %s\tWidth:   %s\n", 
			freqbuf, rig_strrmode(chan->mode), widthbuf);

	sprintf_freq(freqbuf, chan->tx_freq);
	sprintf_freq(widthbuf, chan->tx_width);
	fprintf(fout, "txFreq: %s\ttxMode: %s\ttxWidth: %s\n", 
			freqbuf, rig_strrmode(chan->tx_mode), widthbuf);

	sprintf_freq(freqbuf,chan->rptr_offs);
	fprintf(fout, "Shift: %s, Offset: %s%s, ", rig_strptrshift(chan->rptr_shift),
			chan->rptr_offs>0?"+":"", freqbuf);

	sprintf_freq(freqbuf,chan->tuning_step);
	fprintf(fout, "Step: %s, ", freqbuf);
	sprintf_freq(freqbuf,chan->rit);
	fprintf(fout, "RIT: %s%s, ", chan->rit>0?"+":"", freqbuf);
	sprintf_freq(freqbuf,chan->xit);
	fprintf(fout, "XIT: %s%s\n", chan->xit>0?"+":"", freqbuf);

	fprintf(fout, "CTCSS: %d.%dHz, ", chan->ctcss_tone/10, chan->ctcss_tone%10);
	fprintf(fout, "CTCSSsql: %d.%dHz, ", chan->ctcss_sql/10, chan->ctcss_sql%10);
	fprintf(fout, "DCS: %d.%d, ", chan->dcs_code/10, chan->dcs_code%10);
	fprintf(fout, "DCSsql: %d.%d\n", chan->dcs_sql/10, chan->dcs_sql%10);

	sprintf_func(prntbuf, chan->funcs);
	fprintf(fout, "Functions: %s\n", prntbuf);
	
	fprintf(fout, "Levels:");
	for (idx=0; idx<RIG_SETTING_MAX; idx++) {
		setting_t level = rig_idx2setting(idx);
		const char *level_s;

		if (!rig_has_set_level(rig, level))
				continue;
		level_s = rig_strlevel(level);
		if (!level_s)
				continue;	/* duh! */
		if (firstloop)
			firstloop = 0;
		else
			fprintf(fout, ",\t");
		if (RIG_LEVEL_IS_FLOAT(level))
			fprintf(fout, " %s: %g%%", level_s, 100*chan->levels[idx].f);
		else
			fprintf(fout, " %s: %d", level_s, chan->levels[idx].i);
	}
	fprintf(fout, "\n");
}

/* '1' */
declare_proto_rig(dump_caps)
{
	dumpcaps(rig, fout);

	if (interactive && !prompt)             /* only for rigctld */
		fprintf(fout, "END\n");

	return RIG_OK;
}

/* 'Y' */
declare_proto_rig(set_ant)
{
	ant_t ant;

	sscanf(arg1, "%d", &ant);
	return rig_set_ant(rig, vfo, rig_idx2setting(ant));
}

/* 'y' */
declare_proto_rig(get_ant)
{
	int status;
	ant_t ant;

	status = rig_get_ant(rig, vfo, &ant);
	if (status != RIG_OK)
		return status;
	if (interactive && prompt)
		fprintf(fout, "%s: ", cmd->arg1);
	fprintf(fout, "%d\n", rig_setting2idx(ant));

	if (interactive && !prompt)             /* only for rigctld */
		fprintf(fout, "END\n");

	return status;
}

/* '*' */
declare_proto_rig(reset)
{
	int reset;

	sscanf(arg1, "%d", &reset);
	return rig_reset(rig, (reset_t) reset);
}

/* 'b' */
declare_proto_rig(send_morse)
{
	return rig_send_morse(rig, vfo, arg1);
}

/* '0x87' */
declare_proto_rig(set_powerstat)
{
	int stat;

	sscanf(arg1, "%d", &stat);
	return rig_set_powerstat(rig, (powerstat_t) stat);
}

/* '0x88' */
declare_proto_rig(get_powerstat)
{
	int status;
	powerstat_t stat;

	status = rig_get_powerstat(rig, &stat);
	if (status != RIG_OK)
		return status;
	if (interactive && prompt)
		fprintf(fout, "%s: ", cmd->arg1);
	fprintf(fout, "%d\n", stat);

	if (interactive && !prompt)             /* only for rigctld */
		fprintf(fout, "END\n");

	return status;
}

/*
 * special debugging purpose send command
 * display reply until there's a timeout
 *
 * 'w'
 */
declare_proto_rig(send_cmd)
{
	int retval;
	struct rig_state *rs;
	int backend_num, cmd_len;
#define BUFSZ 128
	char bufcmd[BUFSZ];
	char buf[BUFSZ];

	/*
	 * binary protocols enter values as \0xZZ\0xYY..
	 */
	backend_num = RIG_BACKEND_NUM(rig->caps->rig_model);
	if (backend_num == RIG_YAESU || backend_num == RIG_ICOM ||
			backend_num == RIG_KACHINA ||
			backend_num == RIG_MICROTUNE) {
		const char *p = arg1, *pp = NULL;
		int i;
		for (i=0; i < BUFSZ-1 && p != pp; i++) {
			pp = p+1;
			bufcmd[i] = strtol(p+1, (char **) &p, 0);
		}
		cmd_len = i-1;
		/* must save length to allow 0x00 to be sent as part of a command
		*/
	} else {
		strncpy(bufcmd,arg1,BUFSZ);
		bufcmd[BUFSZ-1] = '\0';
		cmd_len = strlen(bufcmd);
		/*
		 * assumes CR is end of line char
		 * for all ascii protocols
		 */
		strcat(bufcmd, "\r");
	}

	rs = &rig->state;

	serial_flush(&rs->rigport);

	retval = write_block(&rs->rigport, bufcmd, cmd_len);
	if (retval != RIG_OK)
		return retval;

	if (interactive && prompt)
		fprintf(fout, "%s: ", cmd->arg2);

#define EOM "\0xa"
	do {
		retval = read_string(&rs->rigport, buf, BUFSZ, EOM, strlen(EOM));
		if (retval < 0)
			break;
	
		if (retval < BUFSZ)
			buf[retval] = '\0';
		else
			buf[BUFSZ-1] = '\0';
	
		fprintf(fout, "%s\n", buf);

	} while (retval > 0);

	if (retval > 0 || retval == -RIG_ETIMEOUT)
		retval = RIG_OK;

	if (interactive && !prompt)             /* only for rigctld */
		fprintf(fout, "END\n");

	return retval;
}

