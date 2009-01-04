/*
 * rotctl.c - (C) Stephane Fillod 2000-2009
 *
 * This program test/control a rotator using Hamlib.
 * It takes commands in interactive mode as well as 
 * from command line options.
 *
 *	$Id: rotctl.c,v 1.13 2009-01-04 14:49:17 fillods Exp $  
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

#include <getopt.h>

#include <hamlib/rotator.h>
#include "misc.h"

#include "rotctl_parse.h"

/* 
 * Prototypes
 */
void usage();

/*
 * Reminder: when adding long options, 
 * keep up to date SHORT_OPTIONS, usage()'s output and man page. thanks.
 * NB: do NOT use -W since it's reserved by POSIX.
 * TODO: add an option to read from a file
 */
#define SHORT_OPTIONS "m:r:s:C:t:LvhVl"
static struct option long_options[] =
{
	{"model",    1, 0, 'm'},
	{"rot-file", 1, 0, 'r'},
	{"serial-speed", 1, 0, 's'},
	{"send-cmd-term", 1, 0, 't'},
	{"list",     0, 0, 'l'},
	{"set-conf", 1, 0, 'C'},
	{"show-conf",0, 0, 'L'},
	{"verbose",  0, 0, 'v'},
	{"help",     0, 0, 'h'},
	{"version",  0, 0, 'V'},
	{0, 0, 0, 0}
};

#define MAXCONFLEN 128

int interactive = 1;    /* if no cmd on command line, switch to interactive */
int prompt = 1;         /* Print prompt in rotctl */
int opt_end= 0 ;        /* only used by rotctld */

char send_cmd_term = '\r';     /* send_cmd termination char */

int main (int argc, char *argv[])
{ 
	ROT *my_rot;		/* handle to rot (instance) */
	rot_model_t my_model = ROT_MODEL_DUMMY;

	int retcode;		/* generic return code from functions */

	int verbose = 0;
	int show_conf = 0;
	const char *rot_file=NULL;
	int serial_rate = 0;
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
					rot_file = optarg;
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
			case 't':
					if (!optarg) {
						usage();        /* wrong arg count */
						exit(1);
					}
					if (strlen(optarg) > 1)
						send_cmd_term = strtol(optarg, NULL, 0);
					else
						send_cmd_term = optarg[0];
					break;
			case 'v':
					verbose++;
					break;
			case 'L':
					show_conf++;
					break;
			case 'l':
					list_models();
					exit(0);
			default:
					usage();	/* unknown option? */
					exit(1);
		}
	}
	if (verbose < 2)
		rig_set_debug(RIG_DEBUG_WARN);

	/*
	 * at least one command on command line, 
	 * disable interactive mode
	 */
	if (optind < argc)
		interactive = 0;

  	my_rot = rot_init(my_model);

	if (!my_rot) {
		fprintf(stderr, "Unknown rot num %d, or initialization error.\n", 
						my_model);
		fprintf(stderr, "Please check with --list option.\n");
		exit(2);
	}

	retcode = set_conf(my_rot, conf_parms);
	if (retcode != RIG_OK) {
		fprintf(stderr, "Config parameter error: %s\n", rigerror(retcode));
		exit(2);
	}

	if (rot_file)
		strncpy(my_rot->state.rotport.pathname, rot_file, FILPATHLEN);

	/* FIXME: bound checking and port type == serial */
	if (serial_rate != 0)
		my_rot->state.rotport.parm.serial.rate = serial_rate;

	/*
	 * print out conf parameters
	 */
	if (show_conf) {
		rot_token_foreach(my_rot, print_conf_list, (rig_ptr_t)my_rot);
	}

	retcode = rot_open(my_rot);
	if (retcode != RIG_OK) {
	  	fprintf(stderr,"rot_open: error = %s \n", rigerror(retcode));
		exit(2);
	}

	if (verbose > 0)
		printf("Opened rot model %d, '%s'\n", my_rot->caps->rot_model,
						my_rot->caps->model_name);

	rig_debug(RIG_DEBUG_VERBOSE, "Backend version: %s, Status: %s\n",
			my_rot->caps->version, rig_strstatus(my_rot->caps->status));

	do {
		retcode = rotctl_parse(my_rot, stdin, stdout, argv, argc);
	}
	while (retcode == 0);

	rot_close(my_rot); /* close port */
	rot_cleanup(my_rot); /* if you care about memory */

	return 0;
}

void usage()
{
	printf("Usage: rotctl [OPTION]... [COMMAND]...\n"
	   "Send COMMANDs to a connected antenna rotator.\n\n");


	printf(
	"  -m, --model=ID             select rotator model number. See model list\n"
	"  -r, --rot-file=DEVICE      set device of the rotator to operate on\n"
	"  -s, --serial-speed=BAUD    set serial speed of the serial port\n"
	"  -t, --send-cmd-term=CHAR   set send_cmd command termination char\n"
	"  -C, --set-conf=PARM=VAL    set config parameters\n"
	"  -L, --show-conf            list all config parameters\n"
	"  -l, --list                 list all model numbers and exit\n"
	"  -v, --verbose              set verbose mode, cumulative\n"
	"  -h, --help                 display this help and exit\n"
	"  -V, --version              output version information and exit\n\n"
		);

	usage_rot(stdout);

	printf("\nReport bugs to <hamlib-developer@lists.sourceforge.net>.\n");
}

