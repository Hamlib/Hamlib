/*
 * rpcrigd - (C) Stephane Fillod 2001-2002
 *
 * This program let programs control a radio through
 * the mean of RPC services using Hamlib.
 *
 *	$Id: rpcrigd.c,v 1.5 2002-09-13 06:58:55 fillods Exp $  
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

#include "rpcrig.h"
#include <stdio.h>
#include <stdlib.h>
#include <rpc/pmap_clnt.h>
#include <string.h>
#include <memory.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <unistd.h>
#include <netdb.h>


/* TODO: autoconf should check for getopt support, include libs otherwise */
#include <getopt.h>

#include <hamlib/rig.h>



/* 
 * Prototypes
 */
void usage();
void version();
void rigd_exit();

void rigprog_1(struct svc_req *rqstp, register SVCXPRT *transp);


/*
 * Reminder: when adding long options, 
 * 		keep up to date SHORT_OPTIONS, usage()'s output and man page. thanks.
 * NB: do NOT use -W since it's reserved by POSIX.
 */
#define SHORT_OPTIONS "m:r:p:P:d:D:c:s:C:t:vhV"
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
	{"set-conf", 1, 0, 'C'},
	{"prog", 1, 0, 't'},
	{"verbose",  0, 0, 'v'},
	{"help",     0, 0, 'h'},
	{"version",  0, 0, 'V'},
	{0, 0, 0, 0}
};


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


static unsigned long extract_prognum(const char val[])
{
	if (val[0] == '+') {
		return RIGPROG + atol(val+1);
	} else
		if (val[0] < '0' || val[0] > '9') {
			struct rpcent *ent;
			ent = getrpcbyname (val);
			if (ent)
				return ent->r_number;
			else
				return 0;
	} else
		return atol(val);
}


RIG *the_rpc_rig;


#define MAXCONFLEN 128

int
main (int argc, char *argv[])
{
	register SVCXPRT *transp;

	rig_model_t my_model = RIG_MODEL_DUMMY;
	int retcode;		/* generic return code from functions */

	int verbose = 0;
	const char *rig_file=NULL, *ptt_file=NULL, *dcd_file=NULL;
	ptt_type_t ptt_type = RIG_PTT_NONE;
	dcd_type_t dcd_type = RIG_DCD_NONE;
	char *civaddr = NULL;	/* NULL means no need to set conf */
	int serial_rate = 0;
	char conf_parms[MAXCONFLEN] = "";
	unsigned long prognum = RIGPROG;

	/* Arguments parsing */

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
			case 't':
					if (!optarg) {
							usage();	/* wrong arg count */
							exit(1);
					}
					prognum = extract_prognum(optarg);
					break;
			case 'v':
					verbose++;
					break;
			default:
					usage();	/* unknown option? */
					exit(1);
		}
	}
	if (verbose < 2)
		rig_set_debug(RIG_DEBUG_WARN);


	/* Opening the local rig */

  	the_rpc_rig = rig_init(my_model);

	if (!the_rpc_rig) {
			fprintf(stderr, "Unknown rig num %d, or initialization error.\n", 
							my_model);
			fprintf(stderr, "Please check with rigctl --list option.\n");
			exit(2);
	}

	retcode = set_conf(the_rpc_rig, conf_parms);
	if (retcode != RIG_OK) {
			fprintf(stderr, "Config parameter error: %s\n", rigerror(retcode));
			exit(2);
	}

	if (rig_file)
		strncpy(the_rpc_rig->state.rigport.pathname, rig_file, FILPATHLEN);

	/* 
	 * ex: RIG_PTT_PARALLEL and /dev/parport0
	 */
	if (ptt_type != RIG_PTT_NONE)
		the_rpc_rig->state.pttport.type.ptt = ptt_type;
	if (dcd_type != RIG_DCD_NONE)
		the_rpc_rig->state.dcdport.type.dcd = dcd_type;
	if (ptt_file)
		strncpy(the_rpc_rig->state.pttport.pathname, ptt_file, FILPATHLEN);
	if (dcd_file)
		strncpy(the_rpc_rig->state.dcdport.pathname, dcd_file, FILPATHLEN);
	/* FIXME: bound checking and port type == serial */
	if (serial_rate != 0)
		the_rpc_rig->state.rigport.parm.serial.rate = serial_rate;
	if (civaddr)
        rig_set_conf(the_rpc_rig, rig_token_lookup(the_rpc_rig, "civaddr"), civaddr);


	retcode = rig_open(the_rpc_rig);
	if (retcode != RIG_OK) {
	  		fprintf(stderr,"rig_open: error = %s \n", rigerror(retcode));
			exit(2);
	}

	atexit(rigd_exit);

	if (verbose > 0)
			printf("Opened rig model %d, '%s'\n", the_rpc_rig->caps->rig_model,
							the_rpc_rig->caps->model_name);


	/* Setup the RPC service and fire it up */


	pmap_unset (prognum, RIGVERS);

	transp = svcudp_create(RPC_ANYSOCK);
	if (transp == NULL) {
		fprintf (stderr, "cannot create udp service.");
		exit(1);
	}
	if (!svc_register(transp, prognum, RIGVERS, rigprog_1, IPPROTO_UDP)) {
		fprintf (stderr, "unable to register (%lu, %u, udp).", prognum, RIGVERS);
		exit(1);
	}

	transp = svctcp_create(RPC_ANYSOCK, 0, 0);
	if (transp == NULL) {
		fprintf (stderr, "cannot create tcp service.");
		exit(1);
	}
	if (!svc_register(transp, prognum, RIGVERS, rigprog_1, IPPROTO_TCP)) {
		fprintf (stderr, "unable to register (%lu, %u, tcp).", prognum, RIGVERS);
		exit(1);
	}

	svc_run ();
	fprintf (stderr, "svc_run returned");

	/* the rig gets automatically closed in rigd_exit() */

	exit (1);
	/* NOTREACHED */
}


void rigd_exit()
{
	rig_close(the_rpc_rig);
	rig_cleanup(the_rpc_rig);
}



void usage()
{
		printf("Usage: rpc.rigd [OPTION]...\n"
		   "Offer RPC services to remotely control a connected radio transceiver or receiver.\n\n");

		printf(
	"  -m, --model=ID             select radio model number. See model list\n"
	"  -r, --rig-file=DEVICE      set device of the radio to operate on\n"
	"  -p, --ptt-file=DEVICE      set device of the PTT device to operate on\n"
	"  -d, --dcd-file=DEVICE      set device of the DCD device to operate on\n"
	"  -P, --ptt-type=TYPE        set type of the PTT device to operate on\n"
	"  -D, --dcd-type=TYPE        set type of the DCD device to operate on\n"
	"  -s, --serial-speed=BAUD    set serial speed of the serial port\n"
	"  -c, --civaddr=ID           set CI-V address (for Icom rigs only)\n"
	"  -C, --set-conf=PARM=VAL    set config parameters\n"
	"  -t, --prog=NUMBER          set RPC program number\n"
	"  -v, --verbose              set verbose mode, cumulative\n"
	"  -h, --help                 display this help and exit\n"
	"  -V, --version              output version information and exit\n\n"
			);

		printf("\nReport bugs to <hamlib-developer@lists.sourceforge.net>.\n");

}

void version()
{
	printf("rpcrigd, %s\n\n", hamlib_version);
	printf("%s\n", hamlib_copyright);
}

