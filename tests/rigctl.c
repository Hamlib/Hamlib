/*
 * rigctl.c - (C) Stephane Fillod 2000-2003
 *
 * This program test/control a radio using Hamlib.
 * It takes commands in interactive mode as well as 
 * from command line options.
 *
 * $Id: rigctl.c,v 1.49 2004-02-08 20:25:36 fillods Exp $  
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
#include "sprintflst.h"

#ifdef _WIN32
#define LLFMT "L"
#else
#define LLFMT "ll"
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
#define ARG_NOVFO 0x8000

#define ARG_IN  (ARG_IN1|ARG_IN2|ARG_IN3|ARG_IN4)
#define ARG_OUT  (ARG_OUT1|ARG_OUT2|ARG_OUT3|ARG_OUT4)

struct test_table {
	unsigned char cmd;
	const char *name;
	int (*rig_routine)(RIG*, int, const struct test_table*, vfo_t, 
			const char*, const char*, const char*);
	int flags;
	const char *arg1;
	const char *arg2;
	const char *arg3;
};


/* 
 * external prototype
 */

int dumpcaps (RIG *);

/* 
 * Prototypes
 */
void usage();
void usage_rig();
void version();
void list_models();
void dump_chan(RIG*, channel_t*);
static int print_conf_list(const struct confparams *cfp, rig_ptr_t data);
int set_conf(RIG *my_rig, char *conf_parms);

#define declare_proto_rig(f) static int (f)(RIG *rig, int interactive, \
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



/*
 * convention: upper case cmd is set, lowercase is get
 *
 * TODO: add missing rig_set_/rig_get_: sql, dcd, etc.
 * NB: 'q' 'Q' '?' are reserved by interactive mode interface
 * 		do NOT use -W since it's reserved by POSIX.
 *
 *	Available alphabetic letters: -.--------K-----*-----W-Y-
 */
struct test_table test_list[] = {
		{ 'F', "set_freq", set_freq, ARG_IN, "Frequency" },
		{ 'f', "get_freq", get_freq, ARG_OUT, "Frequency" },
		{ 'J', "set_rit", set_rit, ARG_IN, "RIT" },
		{ 'j', "get_rit", get_rit, ARG_OUT, "RIT" },
		{ 'M', "set_mode", set_mode, ARG_IN, "Mode", "Passband" },
		{ 'm', "get_mode", get_mode, ARG_OUT, "Mode", "Passband" },
		{ 'V', "set_vfo", set_vfo, ARG_IN|ARG_NOVFO, "VFO" },
		{ 'v', "get_vfo", get_vfo, ARG_OUT, "VFO" },
		{ 'T', "set_ptt", set_ptt, ARG_IN, "PTT" },
		{ 't', "get_ptt", get_ptt, ARG_OUT, "PTT" },
		{ 'R', "set_rptr_shift", set_rptr_shift, ARG_IN, "Rptr shift" },
		{ 'r', "get_rptr_shift", get_rptr_shift, ARG_OUT, "Rptr shift" },
		{ 'O', "set_rptr_offs", set_rptr_offs, ARG_IN, "Rptr offset" },
		{ 'o', "get_rptr_offs", get_rptr_offs, ARG_OUT, "Rptr offset" },
		{ 'C', "set_ctcss_tone", set_ctcss_tone, ARG_IN, "CTCSS tone" },
		{ 'c', "get_ctcss_tone", get_ctcss_tone, ARG_OUT, "CTCSS tone" },
		{ 'D', "set_dcs_code", set_dcs_code, ARG_IN, "DCS code" },
		{ 'd', "get_dcs_code", get_dcs_code, ARG_OUT, "DCS code" },
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
		{ 'E', "set_mem", set_mem, ARG_IN, "Memory#" },
		{ 'e', "get_mem", get_mem, ARG_OUT, "Memory#" },
		{ 'G', "vfo_op", vfo_op, ARG_IN, "Mem/VFO op" },
		{ 'g', "scan", scan, ARG_IN, "Scan fct", "Channel" },
		{ 'H', "set_channel", set_channel, ARG_IN|ARG_NOVFO,  /* FIXME */ },
		{ 'h', "get_channel", get_channel, ARG_IN|ARG_NOVFO, "Channel" },
		{ 'A', "set_trn", set_trn, ARG_IN|ARG_NOVFO, "Transceive" },
		{ 'a', "get_trn", get_trn, ARG_OUT|ARG_NOVFO, "Transceive" },
		{ 'B', "set_bank", set_bank, ARG_IN, "Bank" },
		{ '_', "get_info", get_info, ARG_OUT|ARG_NOVFO, "Info" },
		{ '2', "power2mW", power2mW, ARG_NOVFO },
		{ '1', "dump_caps", dump_caps, ARG_NOVFO },
		{ 'Z', "set_xit", set_xit, ARG_IN, "XIT" },
		{ 'z', "get_xit", get_xit, ARG_OUT, "XIT" },
		{ 0x83, "set_ant", set_ant, ARG_IN, "Antenna" },
		{ 0x84, "get_ant", get_ant, ARG_OUT, "Antenna" },
		{ 0x85, "reset", reset, ARG_IN, "Reset" },
		{ 0x00, "", NULL },

};

/*
 * Reminder: when adding long options, 
 * 		keep up to date SHORT_OPTIONS, usage()'s output and man page. thanks.
 * NB: do NOT use -W since it's reserved by POSIX.
 * TODO: add an option to read from a file
 */
#define SHORT_OPTIONS "m:r:p:d:P:D:s:c:lC:LuovhV"
static struct option long_options[] =
{
	{"model",    1, 0, 'm'},
	{"rig-file", 1, 0, 'r'},
	{"ptt-file", 1, 0, 'p'},
	{"dcd-file", 1, 0, 'd'},
	{"ptt-type", 1, 0, 'P'},
	{"dcd-type", 1, 0, 'D'},
	{"serial-speed", 1, 0, 's'},
	{"civaddr",  1, 0, 'c'},
	{"list",     0, 0, 'l'},
	{"set-conf", 1, 0, 'C'},
	{"show-conf",0, 0, 'L'},
	{"dump-caps",  0, 0, 'u'},
	{"vfo",  0, 0, 'o'},
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

static int scanfc(const char *format, void *p)
{
	int ret;

	do {
		ret = scanf(format, p);
		if (ret < 0) {
			if (errno != EINTR)
				rig_debug(RIG_DEBUG_ERR, "scanf: %s\n", strerror(errno));
			continue;
		}
		return ret;
	} while(1);
}

#define MAXCONFLEN 128

int main (int argc, char *argv[])
{ 
	RIG *my_rig;		/* handle to rig (nstance) */
	rig_model_t my_model = RIG_MODEL_DUMMY;

	int interactive=1;	/* if no cmd on command line, switch to interactive */
	int retcode;		/* generic return code from functions */
	unsigned char cmd;
	struct test_table *cmd_entry;

	int verbose = 0;
	int vfo_mode = 0;
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

	rig_set_debug(verbose<2 ? RIG_DEBUG_WARN: verbose);

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
		strncpy(my_rig->state.rigport.pathname, rig_file, FILPATHLEN);

	/* 
	 * ex: RIG_PTT_PARALLEL and /dev/parport0
	 */
	if (ptt_type != RIG_PTT_NONE)
		my_rig->state.pttport.type.ptt = ptt_type;
	if (dcd_type != RIG_DCD_NONE)
		my_rig->state.dcdport.type.dcd = dcd_type;
	if (ptt_file)
		strncpy(my_rig->state.pttport.pathname, ptt_file, FILPATHLEN);
	if (dcd_file)
		strncpy(my_rig->state.dcdport.pathname, dcd_file, FILPATHLEN);
	/* FIXME: bound checking and port type == serial */
	if (serial_rate != 0)
		my_rig->state.rigport.parm.serial.rate = serial_rate;
	if (civaddr)
        rig_set_conf(my_rig, rig_token_lookup(my_rig, "civaddr"), civaddr);

	/*
	 * print out conf parameters
	 */
	if (show_conf) {
			rig_token_foreach(my_rig, print_conf_list, (rig_ptr_t)my_rig);
	}

	/*
	 * print out conf parameters, and exists immediately
	 * We may be interested only in only caps, and rig_open may fail.
	 */
	if (dump_caps_opt) {
		dumpcaps(my_rig);
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
					my_rig->caps->version, strstatus(my_rig->caps->status));

#define MAXARGSZ 127
	while (1) {
			char arg1[MAXARGSZ+1], *p1;
			char arg2[MAXARGSZ+1], *p2;
			char arg3[MAXARGSZ+1], *p3;
			static int last_was_ret = 1;
			vfo_t vfo = RIG_VFO_CURR;

			if (interactive) {
				printf("\nRig command: ");

				do {
					scanfc("%c", &cmd);

					/* command by name */
					if (cmd == '\\') {
						unsigned char cmd_name[MAXNAMSIZ], *pcmd = cmd_name;
						int c_len = MAXNAMSIZ;

						scanfc("%c", pcmd);
						while(c_len-- && (isalnum(*pcmd) || *pcmd == '_' ))
							scanfc("%c", ++pcmd);

						*pcmd = '\0';
						cmd = parse_arg(cmd_name);
						break;
					}

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

				/* comment line */
				if (cmd == '#' || cmd == ';') {
					while( cmd != '\n' && cmd != '\r')
						scanfc("%c", &cmd);
					continue;
				}
				if (cmd == 'Q' || cmd == 'q')
						break;
				if (cmd == '?') {
						usage_rig();
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
			if (!(cmd_entry->flags & ARG_NOVFO) && vfo_mode) {
				if (interactive) {
					printf("VFO: ");
					scanfc("%s", arg1);
					vfo = parse_vfo(arg1);
				} else {
					if (!argv[optind]) {
						fprintf(stderr, "Invalid arg for command '%s'\n", 
									cmd_entry->name);
						exit(2);
					}
					vfo = parse_vfo(argv[optind++]);
				}
			}
			if ((cmd_entry->flags & ARG_IN1) && cmd_entry->arg1) {
				if (interactive) {
					printf("%s: ", cmd_entry->arg1);
					scanfc("%s", arg1);
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
					scanfc("%s", arg2);
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
					scanfc("%s", arg3);
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
			retcode = (*cmd_entry->rig_routine)(my_rig, interactive, 
							cmd_entry, vfo, p1, p2, p3);
				
			if (retcode != RIG_OK ) {
	  			printf("%s: error = %s\n", cmd_entry->name, rigerror(retcode));
			}
	}

	rig_close(my_rig); /* close port */
	rig_cleanup(my_rig); /* if you care about memory */

	return 0;
}



void version()
{
		printf("rigctl, %s\n\n", hamlib_version);
		printf("%s\n", hamlib_copyright);
}

void usage_rig()
{
		int i;

		printf("Commands (may not be available for this rig):\n");
		for (i=0; test_list[i].cmd != 0; i++) {
			printf("%c: %-16s(", isprint(test_list[i].cmd)?test_list[i].cmd:'?', test_list[i].name);
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
	"  -C, --set-conf=PARM=VAL    set config parameters\n"
	"  -L, --show-conf            list all config parameters\n"
	"  -l, --list                 list all model numbers and exit\n"
	"  -u, --dump-caps            dump capabilities and exit\n"
	"  -o, --vfo                  do not default to VFO_CURR, require extra vfo arg\n"
	"  -v, --verbose              set verbose mode, cumulative\n"
	"  -h, --help                 display this help and exit\n"
	"  -V, --version              output version information and exit\n\n"
			);

		usage_rig();

		printf("\nReport bugs to <hamlib-developer@lists.sourceforge.net>.\n");

}

static int print_conf_list(const struct confparams *cfp, rig_ptr_t data)
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
		printf("String.\n");
		break;
	case RIG_CONF_CHECKBUTTON:
		printf("Check button.\n");
		break;
	default:
		printf("Unkown conf\n");
	}

	return 1;  /* !=0, we want them all ! */
}

static int print_model_list(const struct rig_caps *caps, void *data)
{
	printf("%d\t%-16s%-24s%-8s%s\n", caps->rig_model, caps->mfg_name,
				caps->model_name, caps->version, strstatus(caps->status));
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
 * static int (f)(RIG *rig, int interactive, const void *arg1, const void *arg2, const void *arg3, const void *arg4)
 */

declare_proto_rig(set_freq)
{
		freq_t freq;

		sscanf(arg1, "%"FREQFMT, &freq);
		return rig_set_freq(rig, vfo, freq);
}

declare_proto_rig(get_freq)
{
		int status;
		freq_t freq;

		status = rig_get_freq(rig, vfo, &freq);
		if (status != RIG_OK)
				return status;
		if (interactive)
			printf("%s: ", cmd->arg1); /* i.e. "Frequency" */
		printf("%lld\n", (long long)freq);
		return status;
}

declare_proto_rig(set_rit)
{
		shortfreq_t rit;

		sscanf(arg1, "%ld", &rit);
		return rig_set_rit(rig, vfo, rit);
}

declare_proto_rig(get_rit)
{
		int status;
		shortfreq_t rit;

		status = rig_get_rit(rig, vfo, &rit);
		if (status != RIG_OK)
				return status;
		if (interactive)
			printf("%s: ", cmd->arg1);
		printf("%ld\n", rit);
		return status;
}

declare_proto_rig(set_xit)
{
		shortfreq_t xit;

		sscanf(arg1, "%ld", &xit);
		return rig_set_xit(rig, vfo, xit);
}

declare_proto_rig(get_xit)
{
		int status;
		shortfreq_t xit;

		status = rig_get_xit(rig, vfo, &xit);
		if (status != RIG_OK)
				return status;
		if (interactive)
			printf("%s: ", cmd->arg1);
		printf("%ld\n", xit);
		return status;
}


declare_proto_rig(set_mode)
{
		rmode_t mode;
		pbwidth_t width;

		mode = parse_mode(arg1);
		sscanf(arg2, "%ld", &width);
		return rig_set_mode(rig, vfo, mode, width);
}


declare_proto_rig(get_mode)
{
		int status;
		rmode_t mode;
		pbwidth_t width;

		status = rig_get_mode(rig, vfo, &mode, &width);
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


declare_proto_rig(set_vfo)
{
		return rig_set_vfo(rig, parse_vfo(arg1));
}


declare_proto_rig(get_vfo)
{
		int status;

		status = rig_get_vfo(rig, &vfo);
		if (status != RIG_OK)
				return status;
		if (interactive)
			printf("%s: ", cmd->arg1);
		printf("%s\n", strvfo(vfo));
		return status;
}


declare_proto_rig(set_ptt)
{
		ptt_t ptt;

		sscanf(arg1, "%d", (int*)&ptt);
		return rig_set_ptt(rig, vfo, ptt);
}


declare_proto_rig(get_ptt)
{
		int status;
		ptt_t ptt;

		status = rig_get_ptt(rig, vfo, &ptt);
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
		return rig_set_rptr_shift(rig, vfo, rptr_shift);
}


declare_proto_rig(get_rptr_shift)
{
		int status;
		rptr_shift_t rptr_shift;

		status = rig_get_rptr_shift(rig, vfo, &rptr_shift);
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
		return rig_set_rptr_offs(rig, vfo, rptr_offs);
}


declare_proto_rig(get_rptr_offs)
{
		int status;
		unsigned long rptr_offs;

		status = rig_get_rptr_offs(rig, vfo, &rptr_offs);
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
		return rig_set_ctcss_tone(rig, vfo, tone);
}


declare_proto_rig(get_ctcss_tone)
{
		int status;
		tone_t tone;

		status = rig_get_ctcss_tone(rig, vfo, &tone);
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
		return rig_set_dcs_code(rig, vfo, code);
}


declare_proto_rig(get_dcs_code)
{
		int status;
		tone_t code;

		status = rig_get_dcs_code(rig, vfo, &code);
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

		sscanf(arg1, "%"FREQFMT, &txfreq);
		return rig_set_split_freq(rig, vfo, txfreq);
}


declare_proto_rig(get_split_freq)
{
		int status;
		freq_t txfreq;

		status = rig_get_split_freq(rig, vfo, &txfreq);
		if (status != RIG_OK)
				return status;
		if (interactive)
			printf("%s: ", cmd->arg1);
		printf("%lld\n", (long long)txfreq);
		return status;
}

declare_proto_rig(set_split_mode)
{
		rmode_t mode;
		pbwidth_t width;

		mode = parse_mode(arg1);
		sscanf(arg2, "%d", (int*)&width);
		return rig_set_split_mode(rig, vfo, mode, width);
}


declare_proto_rig(get_split_mode)
{
		int status;
		rmode_t mode;
		pbwidth_t width;

		status = rig_get_split_mode(rig, vfo, &mode, &width);
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


declare_proto_rig(set_split_vfo)
{
		split_t split;

		sscanf(arg1, "%d", (int*)&split);
		return rig_set_split_vfo(rig, vfo, split, parse_vfo(arg2));
}


declare_proto_rig(get_split_vfo)
{
		int status;
		split_t split;
		vfo_t tx_vfo;

		status = rig_get_split_vfo(rig, vfo, &split, &tx_vfo);
		if (status != RIG_OK)
				return status;
		if (interactive)
			printf("%s: ", cmd->arg1);
		printf("%d\n", split);
		if (interactive)
			printf("%s: ", cmd->arg2);
		printf("%s\n", strvfo(tx_vfo));
		return status;
}


declare_proto_rig(set_ts)
{
		unsigned long ts;

		sscanf(arg1, "%ld", &ts);
		return rig_set_ts(rig, vfo, ts);
}


declare_proto_rig(get_ts)
{
		int status;
		unsigned long ts;

		status = rig_get_ts(rig, vfo, &ts);
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
		scanf("%"FREQFMT, &freq);
		printf("Mode: ");
		scanf("%d", &mode);
		status = rig_power2mW(rig, &mwp, power, freq, mode);
		printf("Power: %d mW\n", mwp);
		return status;
}

/*
 * RIG_CONF_ extparm's type:
 *   NUMERIC: val.f
 *   COMBO: val.i, starting from 0
 *   STRING: val.s
 *   CHECKBUTTON: val.i 0/1
 */
declare_proto_rig(set_level)
{
		setting_t level;
		value_t val;

		level = parse_level(arg1);
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


declare_proto_rig(get_level)
{
		int status;
		setting_t level;
		value_t val;

		level = parse_level(arg1);
		if (!rig_has_get_level(rig, level)) {
			const struct confparams *cfp;

			cfp = rig_ext_lookup(rig, arg1);
			if (!cfp)
				return -RIG_EINVAL;	/* no such parameter */

			status = rig_get_ext_level(rig, vfo, cfp->token, &val);
			if (status != RIG_OK)
				return status;

			if (interactive)
				printf("%s: ", cmd->arg2);

			switch (cfp->type) {
			case RIG_CONF_CHECKBUTTON:
			case RIG_CONF_COMBO:
				printf("%d\n", val.i);
				break;
			case RIG_CONF_NUMERIC:
				printf("%f\n", val.f);
				break;
			case RIG_CONF_STRING:
				printf("%s\n", val.s);
				break;
			default:
				return -RIG_ECONF;
			}
			return status;
		}

		status = rig_get_level(rig, vfo, level, &val);
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
		return rig_set_func(rig, vfo, func, func_stat);
}


declare_proto_rig(get_func)
{
		int status;
		setting_t func;
		int func_stat;

		func = parse_func(arg1);
		status = rig_get_func(rig, vfo, func, &func_stat);
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


declare_proto_rig(get_parm)
{
		int status;
		setting_t parm;
		value_t val;

		parm = parse_parm(arg1);
		if (!rig_has_get_parm(rig, parm)) {
			const struct confparams *cfp;

			cfp = rig_ext_lookup(rig, arg1);
			if (!cfp)
				return -RIG_EINVAL;	/* no such parameter */

			status = rig_get_ext_parm(rig, cfp->token, &val);
			if (status != RIG_OK)
				return status;

			if (interactive)
				printf("%s: ", cmd->arg2);

			switch (cfp->type) {
			case RIG_CONF_CHECKBUTTON:
			case RIG_CONF_COMBO:
				printf("%d\n", val.i);
				break;
			case RIG_CONF_NUMERIC:
				printf("%f\n", val.f);
				break;
			case RIG_CONF_STRING:
				printf("%s\n", val.s);
				break;
			default:
				return -RIG_ECONF;
			}
			return status;
		}

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
		return rig_set_bank(rig, vfo, bank);
}


declare_proto_rig(set_mem)
{
		int ch;

		sscanf(arg1, "%d", &ch);
		return rig_set_mem(rig, vfo, ch);
}


declare_proto_rig(get_mem)
{
		int status;
		int ch;

		status = rig_get_mem(rig, vfo, &ch);
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
		return rig_vfo_op(rig, vfo, op);
}

declare_proto_rig(scan)
{
		scan_t op;
		int ch;

		op = parse_scan(arg1);
		sscanf(arg2, "%d", &ch);
		return rig_scan(rig, vfo, op, ch);
}

declare_proto_rig(set_channel)
{
		fprintf(stderr,"rigctl set_channel not implemented yet!\n");
		return -RIG_ENIMPL;
}


declare_proto_rig(get_channel)
{
		int status;
		channel_t chan;

		if (isdigit(arg1[0])) {
			chan.vfo = RIG_VFO_MEM;
			if (sscanf(arg1, "%d", &chan.channel_num) != 1)
				return -RIG_EINVAL;
		} else {
			chan.vfo = parse_vfo(arg1);
			chan.channel_num = 0;
		}

		status = rig_get_channel(rig, &chan);
		if (status != RIG_OK)
				return status;
		dump_chan(rig, &chan);
		return status;
}

static int myfreq_event(RIG *rig, vfo_t vfo, freq_t freq, rig_ptr_t arg)
{
	printf("Event: freq changed to %lliHz on %s\n", (long long)freq, strvfo(vfo));
	return 0;
}

static int mymode_event(RIG *rig, vfo_t vfo, rmode_t mode, pbwidth_t width, rig_ptr_t arg)
{
	printf("Event: mode changed to %s, width %liHz on %s\n", strrmode(mode),
			width, strvfo(vfo));
	return 0;
}

static int myvfo_event(RIG *rig, vfo_t vfo, rig_ptr_t arg)
{
	printf("Event: vfo changed to %s\n", strvfo(vfo));
	return 0;
}

static int myptt_event(RIG *rig, vfo_t vfo, ptt_t ptt, rig_ptr_t arg)
{
	printf("Event: PTT changed to %i on %s\n", ptt, strvfo(vfo));
	return 0;
}

static int mydcd_event(RIG *rig, vfo_t vfo, dcd_t dcd, rig_ptr_t arg)
{
	printf("Event: DCD changed to %i on %s\n", dcd, strvfo(vfo));
	return 0;
}

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

declare_proto_rig(get_info)
{
		const char *s;

		s = rig_get_info(rig);
		if (interactive)
			printf("%s: ", cmd->arg1);
		printf("%s\n", s ? s : "None");
		return RIG_OK;
}


void dump_chan(RIG *rig, channel_t *chan)
{
	int idx, firstloop=1;
	char freqbuf[16];
	char widthbuf[16];
	char prntbuf[256];

	printf("Channel: %d, Name: '%s'\n", chan->channel_num, 
					chan->channel_desc);

	printf("VFO: %s, Antenna: %d, Split: %s\n", strvfo(chan->vfo),
					chan->ant, chan->split==RIG_SPLIT_ON?"ON":"OFF");

	sprintf_freq(freqbuf, chan->freq);
	sprintf_freq(widthbuf, chan->width);
	printf("Freq:   %s\tMode:   %s\tWidth:   %s\n", 
						freqbuf, strrmode(chan->mode), widthbuf);

	sprintf_freq(freqbuf, chan->tx_freq);
	sprintf_freq(widthbuf, chan->tx_width);
	printf("txFreq: %s\ttxMode: %s\ttxWidth: %s\n", 
						freqbuf, strrmode(chan->tx_mode), widthbuf);

	sprintf_freq(freqbuf,chan->rptr_offs);
	printf("Shift: %s, Offset: %s%s, ", strptrshift(chan->rptr_shift),
						chan->rptr_offs>0?"+":"", freqbuf);

	sprintf_freq(freqbuf,chan->tuning_step);
	printf("Step: %s, ", freqbuf);
	sprintf_freq(freqbuf,chan->rit);
	printf("RIT: %s%s, ", chan->rit>0?"+":"", freqbuf);
	sprintf_freq(freqbuf,chan->xit);
	printf("XIT: %s%s\n", chan->xit>0?"+":"", freqbuf);

	printf("CTCSS: %d.%dHz, ", chan->ctcss_tone/10, chan->ctcss_tone%10);
	printf("CTCSSsql: %d.%dHz, ", chan->ctcss_sql/10, chan->ctcss_sql%10);
	printf("DCS: %d.%d, ", chan->dcs_code/10, chan->dcs_code%10);
	printf("DCSsql: %d.%d\n", chan->dcs_sql/10, chan->dcs_sql%10);

	sprintf_func(prntbuf, chan->funcs);
	printf("Functions: %s\n", prntbuf);
	
	printf("Levels:");
	for (idx=0; idx<60; idx++) {
			setting_t level = rig_idx2setting(idx);
			const char *level_s;

			if (!rig_has_set_level(rig, level))
					continue;
			level_s = strlevel(level);
			if (!level_s)
					continue;	/* duh! */
			if (firstloop)
					firstloop = 0;
			else
					printf(",\t");
			if (RIG_LEVEL_IS_FLOAT(level))
					printf(" %s: %g%%", level_s, 100*chan->levels[idx].f);
			else
					printf(" %s: %d", level_s, chan->levels[idx].i);
	}
	printf("\n");
}

declare_proto_rig(dump_caps)
{
		dumpcaps(rig);

		return RIG_OK;
}

declare_proto_rig(set_ant)
{
	ant_t ant;

	sscanf(arg1, "%d", &ant);
	return rig_set_ant(rig, vfo, rig_idx2setting(ant));
}

declare_proto_rig(get_ant)
{
	int status;
	ant_t ant;

	status = rig_get_ant(rig, vfo, &ant);
	if (status != RIG_OK)
		return status;
	if (interactive)
		printf("%s: ", cmd->arg1);
	printf("%d\n", rig_setting2idx(ant));
	return status;
}

declare_proto_rig(reset)
{
	reset_t reset;

	sscanf(arg1, "%d", &reset);
	return rig_reset(rig, reset);
}


