/*
 * rigswr.c - (C) Stephane Fillod and Thierry Leconte 2004
 *
 * This program output swr curve value
 * using Hamlib.
 *
 * $Id: rigswr.c,v 1.1 2004-06-14 20:13:35 f4dwv Exp $  
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


/* 
 * external prototype
 */


/* 
 * Prototypes
 */
void usage();
void version();
int set_conf(RIG *rig, char *conf_parms);

/*
 * Reminder: when adding long options, 
 * 		keep up to date SHORT_OPTIONS, usage()'s output and man page. thanks.
 * NB: do NOT use -W since it's reserved by POSIX.
 */
#define SHORT_OPTIONS "m:r:s:c:C:avhV"
static struct option long_options[] =
{
	{"model",    1, 0, 'm'},
	{"rig-file", 1, 0, 'r'},
	{"serial-speed", 1, 0, 's'},
	{"civaddr",  1, 0, 'c'},
	{"set-conf", 1, 0, 'C'},
	{"all",  0, 0, 'a'},
	{"verbose",  0, 0, 'v'},
	{"help",     0, 0, 'h'},
	{"version",  0, 0, 'V'},
	{0, 0, 0, 0}
};

#define MAXCONFLEN 128

int all;

int main (int argc, char *argv[])
{ 
	RIG *rig;		/* handle to rig (nstance) */
	rig_model_t my_model = RIG_MODEL_DUMMY;

	int retcode;		/* generic return code from functions */

	int verbose = 0;
	const char *rig_file=NULL;
	int serial_rate = 0;
	char *civaddr = NULL;	/* NULL means no need to set conf */
	char conf_parms[MAXCONFLEN] = "";
	freq_t freq,freqstop;
	freq_t step=kHz(100);

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
			case 'a':
					all++;
					break;
			case 'v':
					verbose++;
					break;
			default:
					usage();	/* unknown option? */
					exit(1);
		}
	}

	rig_set_debug(verbose<2 ? RIG_DEBUG_WARN: verbose);

	rig_debug(RIG_DEBUG_VERBOSE, "rigmem, %s\n", hamlib_version);
	rig_debug(RIG_DEBUG_VERBOSE, "Report bugs to "
					"<hamlib-developer@lists.sourceforge.net>\n\n");

	if (optind+1 >= argc) {
		usage();
		exit(1);
	}

  	rig = rig_init(my_model);

	if (!rig) {
		fprintf(stderr, "Unknown rig num %d, or initialization error.\n", 
						my_model);
		fprintf(stderr, "Please check with --list option.\n");
		exit(2);
	}

	retcode = set_conf(rig, conf_parms);
	if (retcode != RIG_OK) {
		fprintf(stderr, "Config parameter error: %s\n", rigerror(retcode));
		exit(2);
	}

	if (rig_file)
		strncpy(rig->state.rigport.pathname, rig_file, FILPATHLEN);

	/* FIXME: bound checking and port type == serial */
	if (serial_rate != 0)
		rig->state.rigport.parm.serial.rate = serial_rate;
	if (civaddr)
        	rig_set_conf(rig, rig_token_lookup(rig, "civaddr"), civaddr);

	retcode = rig_open(rig);
	if (retcode != RIG_OK) {
	 	fprintf(stderr,"rig_open: error = %s \n", rigerror(retcode));
		exit(2);
	}

	if (verbose > 0)
			printf("Opened rig model %d, '%s'\n", rig->caps->rig_model,
							rig->caps->model_name);

	if((rig->caps->has_get_level & RIG_LEVEL_SWR) ==0) {
	 	fprintf(stderr,"rig backend for  %s could not get SWR\nSorry\n", rig->caps->model_name);
		exit(2);
	}

	freq=atof(argv[optind++]);
	freqstop=atof(argv[optind++]);
	if (optind < argc) 
		step=atof(argv[optind]);
	
	rig_set_freq(rig,RIG_VFO_CURR,freq);
	rig_set_mode(rig,RIG_VFO_CURR,RIG_MODE_CW,Hz(500));

	while(freq<=freqstop) {
		value_t swr;

		rig_set_ptt(rig,RIG_VFO_CURR,RIG_PTT_ON);
		usleep(500000);
		rig_get_level(rig,RIG_VFO_CURR,RIG_LEVEL_SWR,&swr);
		rig_set_ptt(rig,RIG_VFO_CURR,RIG_PTT_OFF);

		printf("%10.0f %4.2f\n",freq,swr.f);
		freq+=step;
		rig_set_freq(rig,RIG_VFO_CURR,freq);
	}

	return 0;
}



void version()
{
		printf("rigmem, %s\n\n", hamlib_version);
		printf("%s\n", hamlib_copyright);
}

void usage()
{
		printf("Usage: rigmem [OPTION]... D...FILE\n"
		   "Output SWR vs Frequency.\n\n");


		printf(
	"  -m, --model=ID             select radio model number. See model list\n"
	"  -r, --rig-file=DEVICE      set device of the radio to operate on\n"
	"  -s, --serial-speed=BAUD    set serial speed of the serial port\n"
	"  -c, --civaddr=ID           set CI-V address, decimal (for Icom rigs only)\n"
	"  -C, --set-conf=PARM=VAL    set config parameters\n"
	"  -a, --all                  bypass mem_caps, apply to all fields of channel_t\n"
	"  -v, --verbose              set verbose mode, cumulative\n"
	"  -h, --help                 display this help and exit\n"
	"  -V, --version              output version information and exit\n\n"
			);

	printf("\nReport bugs to <hamlib-developer@lists.sourceforge.net>.\n");

}

int set_conf(RIG *rig, char *conf_parms)
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

		ret = rig_set_conf(rig, rig_token_lookup(rig, p), q);
		if (ret != RIG_OK)
				return ret;
		p = n;
	}
	return RIG_OK;
}
