/*
 * rigctl.c - (C) Stephane Fillod 2000-2010
 *            (C) Nate Bargmann 2003,2006,2008,2010,2011,2012,2013
 *            (C) The Hamlib Group 2002,2006,2007,2012
 *
 * This program test/control a radio using Hamlib.
 * It takes commands in interactive mode as well as
 * from command line options.
 *
 *
 *   This program is free software; you can redistribute it and/or modify
 *   it under the terms of the GNU General Public License as published by
 *   the Free Software Foundation; either version 2 of the License, or
 *   (at your option) any later version.
 *
 *   This program is distributed in the hope that it will be useful,
 *   but WITHOUT ANY WARRANTY; without even the implied warranty of
 *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *   GNU General Public License for more details.
 *
 *   You should have received a copy of the GNU General Public License along
 *   with this program; if not, write to the Free Software Foundation, Inc.,
 *   51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
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

#ifdef HAVE_LIBREADLINE
#  if defined(HAVE_READLINE_READLINE_H)
#    include <readline/readline.h>
#  elif defined(HAVE_READLINE_H)    /* !defined(HAVE_READLINE_READLINE_H) */
#    include <readline.h>
#  else                             /* !defined(HAVE_READLINE_H) */
extern char *readline ();
#  endif                            /* HAVE_READLINE_H */
#else
/* no readline */
#endif                              /* HAVE_LIBREADLINE */

#ifdef HAVE_READLINE_HISTORY
#  if defined(HAVE_READLINE_HISTORY_H)
#    include <readline/history.h>
#  elif defined(HAVE_HISTORY_H)
#    include <history.h>
#  else                             /* !defined(HAVE_HISTORY_H) */
extern void add_history ();
extern int write_history ();
extern int read_history ();
#  endif                            /* defined(HAVE_READLINE_HISTORY_H) */
                                    /* no history */
#endif                              /* HAVE_READLINE_HISTORY */


#include <hamlib/rig.h>
#include "misc.h"
#include "iofunc.h"
#include "serial.h"
#include "sprintflst.h"

#include "rigctl_parse.h"

#define MAXNAMSIZ 32
#define MAXNBOPT 100	/* max number of different options */


void usage(void);


/*
 * Reminder: when adding long options,
 * 		keep up to date SHORT_OPTIONS, usage()'s output and man page. thanks.
 * NB: do NOT use -W since it's reserved by POSIX.
 * TODO: add an option to read from a file
 */
#define SHORT_OPTIONS "m:r:p:d:P:D:s:c:t:lC:LuovhV"
static struct option long_options[] =
{
	{"model",           1, 0, 'm'},
	{"rig-file",        1, 0, 'r'},
	{"ptt-file",        1, 0, 'p'},
	{"dcd-file",        1, 0, 'd'},
	{"ptt-type",        1, 0, 'P'},
	{"dcd-type",        1, 0, 'D'},
	{"serial-speed",    1, 0, 's'},
	{"civaddr",         1, 0, 'c'},
	{"send-cmd-term",   1, 0, 't'},
	{"list",            0, 0, 'l'},
	{"set-conf",        1, 0, 'C'},
	{"show-conf",       0, 0, 'L'},
	{"dump-caps",       0, 0, 'u'},
	{"vfo",             0, 0, 'o'},
	{"verbose",         0, 0, 'v'},
	{"help",            0, 0, 'h'},
	{"version",         0, 0, 'V'},
	{0, 0, 0, 0}
};

#define MAXCONFLEN 128

/* variable for readline support */
#ifdef HAVE_LIBREADLINE
static const int have_rl = 1;
#else                               /* no readline */
static const int have_rl = 0;
#endif


int interactive = 1;    /* if no cmd on command line, switch to interactive */
int prompt = 1;         /* Print prompt in rigctl */
int vfo_mode = 0;       /* vfo_mode = 0 means target VFO is 'currVFO' */

char send_cmd_term = '\r';  /* send_cmd termination char */

int main (int argc, char *argv[])
{
	RIG *my_rig;		/* handle to rig (nstance) */
	rig_model_t my_model = RIG_MODEL_DUMMY;

	int retcode;		/* generic return code from functions */
	int exitcode;

	int verbose = 0;
	int show_conf = 0;
	int dump_caps_opt = 0;
	const char *rig_file=NULL, *ptt_file=NULL, *dcd_file=NULL;
	ptt_type_t ptt_type = RIG_PTT_NONE;
	dcd_type_t dcd_type = RIG_DCD_NONE;
	int serial_rate = 0;
	char *civaddr = NULL;	/* NULL means no need to set conf */
	char conf_parms[MAXCONFLEN] = "";

	while(1) {
		int c;
		int option_index = 0;

		c = getopt_long (argc, argv, SHORT_OPTIONS,
			long_options, &option_index);
		if (c == -1)
			break;

		switch(c) {
			case 'h':
				usage();
				exit(0);
			case 'V':
				version();
				exit(0);
			case 'm':
				if (!optarg) {
					usage();	/* wrong arg count */
					exit(1);
				}
				my_model = atoi(optarg);
				break;
			case 'r':
				if (!optarg) {
					usage();	/* wrong arg count */
					exit(1);
				}
				rig_file = optarg;
				break;
			case 'p':
				if (!optarg) {
					usage();	/* wrong arg count */
					exit(1);
				}
				ptt_file = optarg;
				break;
			case 'd':
				if (!optarg) {
					usage();	/* wrong arg count */
					exit(1);
				}
				dcd_file = optarg;
				break;
			case 'P':
				if (!optarg) {
					usage();	/* wrong arg count */
					exit(1);
				}
				if (!strcmp(optarg, "RIG"))
					ptt_type = RIG_PTT_RIG;
				else if (!strcmp(optarg, "DTR"))
					ptt_type = RIG_PTT_SERIAL_DTR;
				else if (!strcmp(optarg, "RTS"))
					ptt_type = RIG_PTT_SERIAL_RTS;
				else if (!strcmp(optarg, "PARALLEL"))
					ptt_type = RIG_PTT_PARALLEL;
				else if (!strcmp(optarg, "CM108"))
					ptt_type = RIG_PTT_CM108;
				else if (!strcmp(optarg, "NONE"))
					ptt_type = RIG_PTT_NONE;
				else
					ptt_type = atoi(optarg);
				break;
			case 'D':
				if (!optarg) {
					usage();	/* wrong arg count */
					exit(1);
				}
				if (!strcmp(optarg, "RIG"))
					dcd_type = RIG_DCD_RIG;
				else if (!strcmp(optarg, "DSR"))
					dcd_type = RIG_DCD_SERIAL_DSR;
				else if (!strcmp(optarg, "CTS"))
					dcd_type = RIG_DCD_SERIAL_CTS;
				else if (!strcmp(optarg, "CD"))
					dcd_type = RIG_DCD_SERIAL_CAR;
				else if (!strcmp(optarg, "PARALLEL"))
					dcd_type = RIG_DCD_PARALLEL;
				else if (!strcmp(optarg, "CM108"))
					dcd_type = RIG_DCD_CM108;
				else if (!strcmp(optarg, "NONE"))
					dcd_type = RIG_DCD_NONE;
				else
					dcd_type = atoi(optarg);
				break;
			case 'c':
				if (!optarg) {
					usage();	/* wrong arg count */
					exit(1);
				}
				civaddr = optarg;
				break;
			case 't':
				if (!optarg) {
					usage();	/* wrong arg count */
					exit(1);
				}
				if (strlen(optarg) > 1)
					send_cmd_term = strtol(optarg, NULL, 0);
				else
					send_cmd_term = optarg[0];
				break;
			case 's':
				if (!optarg) {
					usage();	/* wrong arg count */
					exit(1);
				}
				serial_rate = atoi(optarg);
				break;
			case 'C':
				if (!optarg) {
					usage();	/* wrong arg count */
					exit(1);
				}
				if (*conf_parms != '\0')
					strcat(conf_parms, ",");
				strncat(conf_parms, optarg, MAXCONFLEN-strlen(conf_parms));
				break;
			case 'o':
				vfo_mode++;
				break;
			case 'v':
				verbose++;
				break;
			case 'L':
				show_conf++;
				break;
			case 'l':
				rig_set_debug(verbose);
				list_models();
				exit(0);
			case 'u':
				dump_caps_opt++;
				break;
			default:
				usage();	/* unknown option? */
				exit(1);
		}
	}

	rig_set_debug(verbose);

	rig_debug(RIG_DEBUG_VERBOSE, "rigctl, %s\n", hamlib_version);
	rig_debug(RIG_DEBUG_VERBOSE, "Report bugs to "
			"<hamlib-developer@lists.sourceforge.net>\n\n");

	/*
	 * at least one command on command line,
	 * disable interactive mode
	 */
	if (optind < argc)
		interactive = 0;

  	my_rig = rig_init(my_model);

	if (!my_rig) {
		fprintf(stderr, "Unknown rig num %d, or initialization error.\n",
						my_model);
		fprintf(stderr, "Please check with --list option.\n");
		exit(2);
	}

	retcode = set_conf(my_rig, conf_parms);
	if (retcode != RIG_OK) {
		fprintf(stderr, "Config parameter error: %s\n", rigerror(retcode));
		exit(2);
	}

	if (rig_file)
		strncpy(my_rig->state.rigport.pathname, rig_file, FILPATHLEN - 1);

	/*
	 * ex: RIG_PTT_PARALLEL and /dev/parport0
	 */
	if (ptt_type != RIG_PTT_NONE)
		my_rig->state.pttport.type.ptt = ptt_type;
	if (dcd_type != RIG_DCD_NONE)
		my_rig->state.dcdport.type.dcd = dcd_type;
	if (ptt_file)
		strncpy(my_rig->state.pttport.pathname, ptt_file, FILPATHLEN - 1);
	if (dcd_file)
		strncpy(my_rig->state.dcdport.pathname, dcd_file, FILPATHLEN - 1);
	/* FIXME: bound checking and port type == serial */
	if (serial_rate != 0)
		my_rig->state.rigport.parm.serial.rate = serial_rate;
	if (civaddr)
        	rig_set_conf(my_rig, rig_token_lookup(my_rig, "civaddr"), civaddr);

	/*
	 * print out conf parameters
	 */
	if (show_conf) {
		dumpconf(my_rig, stdout);
	}

	/*
	 * print out capabilities, and exists immediately
	 * We may be interested only in only caps, and rig_open may fail.
	 */
	if (dump_caps_opt) {
		dumpcaps(my_rig, stdout);
		rig_cleanup(my_rig); /* if you care about memory */
		exit(0);
	}

	retcode = rig_open(my_rig);
	if (retcode != RIG_OK) {
	  	fprintf(stderr,"rig_open: error = %s \n", rigerror(retcode));
		exit(2);
	}

	if (verbose > 0)
		printf("Opened rig model %d, '%s'\n", my_rig->caps->rig_model,
				my_rig->caps->model_name);
	rig_debug(RIG_DEBUG_VERBOSE, "Backend version: %s, Status: %s\n",
			my_rig->caps->version, rig_strstatus(my_rig->caps->status));

	exitcode = 0;

#ifdef HAVE_LIBREADLINE

	if (interactive && prompt && have_rl) {
		rl_readline_name = "rigctl";
#ifdef HAVE_READLINE_HISTORY
		using_history();
#endif
	}

#endif	/* HAVE_LIBREADLINE */

	do {
		retcode = rigctl_parse(my_rig, stdin, stdout, argv, argc);
		if (retcode == 2)
			exitcode = 2;
	}
	while (retcode == 0 || retcode == 2);

	rig_close(my_rig); /* close port */
	rig_cleanup(my_rig); /* if you care about memory */

	return exitcode;
}


void usage(void)
{
	printf("Usage: rigctl [OPTION]... [COMMAND]...\n"
	   "Send COMMANDs to a connected radio transceiver or receiver.\n\n");


	printf(
	"  -m, --model=ID             select radio model number. See model list\n"
	"  -r, --rig-file=DEVICE      set device of the radio to operate on\n"
	"  -p, --ptt-file=DEVICE      set device of the PTT device to operate on\n"
	"  -d, --dcd-file=DEVICE      set device of the DCD device to operate on\n"
	"  -P, --ptt-type=TYPE        set type of the PTT device to operate on\n"
	"  -D, --dcd-type=TYPE        set type of the DCD device to operate on\n"
	"  -s, --serial-speed=BAUD    set serial speed of the serial port\n"
	"  -c, --civaddr=ID           set CI-V address, decimal (for Icom rigs only)\n"
	"  -t, --send-cmd-term=CHAR   set send_cmd command termination char\n"
	"  -C, --set-conf=PARM=VAL    set config parameters\n"
	"  -L, --show-conf            list all config parameters\n"
	"  -l, --list                 list all model numbers and exit\n"
	"  -u, --dump-caps            dump capabilities and exit\n"
	"  -o, --vfo                  do not default to VFO_CURR, require extra vfo arg\n"
	"  -v, --verbose              set verbose mode, cumulative\n"
	"  -h, --help                 display this help and exit\n"
	"  -V, --version              output version information and exit\n\n"
			);

	usage_rig(stdout);

	printf("\nReport bugs to <hamlib-developer@lists.sourceforge.net>.\n");

}
