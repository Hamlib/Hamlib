/*
 * rotctl.c - (C) Stephane Fillod 2000,2001
 *
 * This program test/control a rotator using Hamlib.
 * It takes commands in interactive mode as well as 
 * from command line options.
 *
 * $Id: rotctl.c,v 1.2 2002-01-14 07:00:55 fillods Exp $  
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

#include <getopt.h>

#include <hamlib/rotator.h>
#include "misc.h"



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

#define ARG_NONE	0
#define ARG_IN  (ARG_IN1|ARG_IN2|ARG_IN3|ARG_IN4)
#define ARG_OUT  (ARG_OUT1|ARG_OUT2|ARG_OUT3|ARG_OUT4)

struct test_table {
	unsigned char cmd;
	const char *name;
    int (*rot_routine)(ROT*, int, const struct test_table*, const char*, 
					const char*, const char*);
	int flags;
	const char *arg1;
	const char *arg2;
	const char *arg3;
};

/* 
 * Prototypes
 */
void usage();
void usage_rot();
void version();
void list_models();
static int print_conf_list(const struct confparams *cfp, rig_ptr_t data);
int set_conf(ROT *my_rot, char *conf_parms);

rmode_t parse_mode(const char *s);
vfo_t parse_vfo(const char *s);
setting_t parse_func(const char *s);
setting_t parse_level(const char *s);
setting_t parse_parm(const char *s);
vfo_op_t parse_vfo_op(const char *s);
scan_t parse_scan(const char *s);
rptr_shift_t parse_rptr_shift(const char *s);

#define declare_proto_rot(f) static int (f)(ROT *rot, int interactive, \
			const struct test_table *cmd, const char *arg1, \
			const char *arg2, const char *arg3)

declare_proto_rot(set_position);
declare_proto_rot(get_position);
declare_proto_rot(stop);
declare_proto_rot(park);
declare_proto_rot(reset);
declare_proto_rot(get_info);


/*
 * convention: upper case cmd is set, lowercase is get
 *
 * NB: 'q' 'Q' '?' are reserved by interactive mode interface
 */
struct test_table test_list[] = {
		{ 'P', "set_pos", set_position, ARG_IN, "Azimuth", "Elevation" },
		{ 'p', "get_pos", get_position, ARG_OUT, "Azimuth", "Elevation" },
		{ 'K', "park", park, ARG_NONE, },
		{ 'S', "stop", stop, ARG_NONE, },
		{ 'R', "reset", reset, ARG_IN, "Reset" },
		{ '_', "get_info", get_info, ARG_OUT, "Info" },
		{ 0x00, "", NULL },

};

/*
 * Reminder: when adding long options, 
 * 		keep up to date SHORT_OPTIONS, usage()'s output and man page. thanks.
 * NB: do NOT use -W since it's reserved by POSIX.
 * TODO: add an option to read from a file
 */
#define SHORT_OPTIONS "m:r:s:C:LvhVl"
static struct option long_options[] =
{
	{"model",    1, 0, 'm'},
	{"rot-file", 1, 0, 'r'},
	{"serial-speed", 1, 0, 's'},
	{"list",     0, 0, 'l'},
	{"set-conf", 1, 0, 'C'},
	{"show-conf",0, 0, 'L'},
	{"verbose",  0, 0, 'v'},
	{"help",     0, 0, 'h'},
	{"version",  0, 0, 'V'},
	{0, 0, 0, 0}
};

struct test_table *find_cmd_entry(int cmd)
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
 * TODO: use Lex
 */
char parse_arg(const char *arg)
{
		int i;
		for (i=0; i<MAXNBOPT && test_list[i].cmd != 0; i++)
				if (!strncmp(arg, test_list[i].name, MAXNAMSIZ))
						return test_list[i].cmd;
		return 0;
}

#define MAXCONFLEN 128

int main (int argc, char *argv[])
{ 
	ROT *my_rot;		/* handle to rot (instance) */
	rot_model_t my_model = ROT_MODEL_DUMMY;

	int interactive=1;	/* if no cmd on command line, switch to interactive */
	int retcode;		/* generic return code from functions */
	char cmd;
	struct test_table *cmd_entry;

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

#define MAXARGSZ 127
	while (1) {
			char arg1[MAXARGSZ+1], *p1;
			char arg2[MAXARGSZ+1], *p2;
			char arg3[MAXARGSZ+1], *p3;
			static int last_was_ret = 1;

			if (interactive) {
				printf("\nRig command: ");

				do {
					scanf("%c", &cmd);
					if (cmd == 0x0a || cmd == 0x0d) {
						if (last_was_ret) {
							printf("? for help, q to quit.\n");
							printf("\nRig command: ");
							continue;
						}
						last_was_ret = 1;
					}
				} while (cmd == 0x0a || cmd == 0x0d);

				last_was_ret = 0;

				if (cmd == 'Q' || cmd == 'q')
						break;
				if (cmd == '?') {
						usage_rot();
					continue;
				}
			} else {
					/* parse rest of command line */
					if (optind >= argc)
							break;
					if (argv[optind][1] == '\0')
							cmd = argv[optind][0];
					else
							cmd = parse_arg(argv[optind]);
					optind++;
			}

			cmd_entry = find_cmd_entry(cmd);
			if (!cmd_entry) {
				fprintf(stderr, "Command '%c' not found!\n", cmd);
				continue;
			}

			p1 = p2 = p3 = NULL;
			if ((cmd_entry->flags & ARG_IN1) && cmd_entry->arg1) {
				if (interactive) {
					printf("%s: ", cmd_entry->arg1);
					scanf("%s", arg1);
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
					printf("%s: ", cmd_entry->arg2);
					scanf("%s", arg2);
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
					printf("%s: ", cmd_entry->arg3);
					scanf("%s", arg3);
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
			retcode = (*cmd_entry->rot_routine)(my_rot, interactive, 
							cmd_entry, p1, p2, p3);
				
			if (retcode != RIG_OK ) {
	  			printf("%s: error = %s\n", cmd_entry->name, rigerror(retcode));
			}
	}

	rot_close(my_rot); /* close port */
	rot_cleanup(my_rot); /* if you care about memory */

	return 0;
}



void version()
{
		printf("rotctl, %s\n\n", hamlib_version);
		printf("%s\n", hamlib_copyright);
}

void usage_rot()
{
		int i;

		printf("Commands (may not be available for this rotator):\n");
		for (i=0; test_list[i].cmd != 0; i++) {
			printf("%c: %-16s(", test_list[i].cmd, test_list[i].name);
			if (test_list[i].arg1)
					printf("%s", test_list[i].arg1);
			if (test_list[i].arg2)
					printf(",%s", test_list[i].arg2);
			if (test_list[i].arg3)
					printf(",%s", test_list[i].arg3);
			printf(")  \t");

			if (i%2)
					printf("\n");
		}

}
void usage()
{
		printf("Usage: rotctl [OPTION]... [COMMAND]...\n"
		   "Send COMMANDs to a connected antenna rotator.\n\n");


		printf(
	"  -m, --model=ID             select rotator model number. See model list\n"
	"  -r, --rot-file=DEVICE      set device of the rotator to operate on\n"
	"  -s, --serial-speed=BAUD    set serial speed of the serial port\n"
	"  -C, --set-conf=PARM=VAL    set config parameters\n"
	"  -L, --show-conf            list all config parameters\n"
	"  -l, --list                 list all model numbers and exit\n"
	"  -v, --verbose              set verbose mode, cumulative\n"
	"  -h, --help                 display this help and exit\n"
	"  -V, --version              output version information and exit\n\n"
			);

		usage_rot();

		printf("\nReport bugs to <hamlib-developer@lists.sourceforge.net>.\n");

}

static int print_conf_list(const struct confparams *cfp, rig_ptr_t data)
{
	ROT *rot = (ROT*) data;
	int i;
	char buf[128] = "";

	rot_get_conf(rot, cfp->token, buf);
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
	default:
	}

	return 1;  /* !=0, we want them all ! */
}

static int print_model_list(const struct rot_caps *caps, void *data)
{
	printf("%d\t%-14s%-16s%s\n", caps->rot_model, caps->mfg_name,
										caps->model_name, caps->version);
	return 1;  /* !=0, we want them all ! */
}

void list_models()
{
	int status;

	rot_load_all_backends();

	printf("Rig#\tMfg           Model           Vers.\n");
	status = rot_list_foreach(print_model_list, NULL);
	if (status != RIG_OK ) {
		printf("rot_list_foreach: error = %s \n", rigerror(status));
		exit(2);
	}
}


int set_conf(ROT *my_rot, char *conf_parms)
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

		ret = rot_set_conf(my_rot, rot_token_lookup(my_rot, p), q);
		if (ret != RIG_OK)
				return ret;
		p = n;
	}
	return RIG_OK;
}


/*
 * static int (f)(ROT *rot, int interactive, const void *arg1, const void *arg2, const void *arg3, const void *arg4)
 */

declare_proto_rot(set_position)
{
		azimuth_t az;
		elevation_t el;

		sscanf(arg1, "%f", &az);
		sscanf(arg2, "%f", &el);
		return rot_set_position(rot, az, el);
}

declare_proto_rot(get_position)
{
		int status;
		azimuth_t az;
		elevation_t el;

		status = rot_get_position(rot, &az, &el);
		if (status != RIG_OK)
				return status;
		if (interactive)
			printf("%s: ", cmd->arg1);
		printf("%f\n", az);
		if (interactive)
			printf("%s: ", cmd->arg2);
		printf("%f", el);
		return status;
}

declare_proto_rot(stop)
{
		return rot_stop(rot);
}

declare_proto_rot(park)
{
		return rot_park(rot);
}


declare_proto_rot(reset)
{
		rot_reset_t reset;

		sscanf(arg1, "%d", &reset);
		return rot_reset(rot, reset);
}

declare_proto_rot(get_info)
{
		const char *s;

		s = rot_get_info(rot);
		if (interactive)
			printf("%s: ", cmd->arg1);
		printf("%s\n", s ? s : "None");
		return RIG_OK;
}


