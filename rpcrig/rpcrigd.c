/*
 * rpcrigd - (C) Stephane Fillod 2001
 *
 * This program let programs control a radio through
 * the mean of RPC services using Hamlib.
 *
 * $Id: rpcrigd.c,v 1.1 2001-10-16 19:29:03 f4cfe Exp $  
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
 * TODO: add an option to read from a file
 */
#define SHORT_OPTIONS "m:r:p:P:d:D:c:vhVl"
static struct option long_options[] =
{
	{"model",    1, 0, 'm'},
	{"rig-file", 1, 0, 'r'},
	{"ptt-file", 1, 0, 'p'},
	{"dcd-file", 1, 0, 'd'},
	{"ptt-type", 1, 0, 'P'},
	{"dcd-type", 1, 0, 'D'},
	{"civaddr",  1, 0, 'c'},
	{"list",     0, 0, 'l'},
	{"verbose",  0, 0, 'v'},
	{"help",     0, 0, 'h'},
	{"version",  0, 0, 'V'},
	{0, 0, 0, 0}
};



RIG *the_rpc_rig;

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
					ptt_type = atoi(optarg);
					break;
			case 'D':
					if (!optarg) {
							usage();	/* wrong arg count */
							exit(1);
					}
					dcd_type = atoi(optarg);
					break;
			case 'c':
					if (!optarg) {
							usage();	/* wrong arg count */
							exit(1);
					}
					civaddr = optarg;
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
			fprintf(stderr, "Please check with --list option.\n");
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
	if (civaddr)
        rig_set_conf(the_rpc_rig, rig_token_lookup(the_rpc_rig, "civaddr"), civaddr);


	if ((retcode = rig_open(the_rpc_rig)) != RIG_OK) {
	  		fprintf(stderr,"rig_open: error = %s \n", rigerror(retcode));
			exit(2);
	}

	atexit(rigd_exit);

	if (verbose > 0)
			printf("Opened rig model %d, '%s'\n", the_rpc_rig->caps->rig_model,
							the_rpc_rig->caps->model_name);


	/* Setup the RPC service and fire it up */


	pmap_unset (RIGPROG, RIGVERS);

	transp = svcudp_create(RPC_ANYSOCK);
	if (transp == NULL) {
		fprintf (stderr, "%s", "cannot create udp service.");
		exit(1);
	}
	if (!svc_register(transp, RIGPROG, RIGVERS, rigprog_1, IPPROTO_UDP)) {
		fprintf (stderr, "%s", "unable to register (RIGPROG, RIGVERS, udp).");
		exit(1);
	}

	transp = svctcp_create(RPC_ANYSOCK, 0, 0);
	if (transp == NULL) {
		fprintf (stderr, "%s", "cannot create tcp service.");
		exit(1);
	}
	if (!svc_register(transp, RIGPROG, RIGVERS, rigprog_1, IPPROTO_TCP)) {
		fprintf (stderr, "%s", "unable to register (RIGPROG, RIGVERS, tcp).");
		exit(1);
	}

	svc_run ();
	fprintf (stderr, "%s", "svc_run returned");

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
		printf("Usage: rpc.rigd [OPTION]... [COMMAND]...\n"
		   "Offer RPC services to remotely control a connected radio transceiver or receiver.\n\n");

		printf(
	"  -m, --model=ID             select radio model number. See model list\n"
	"  -r, --rig-file=DEVICE      set device of the radio to operate on\n"
	"  -p, --ptt-file=DEVICE      set device of the PTT device to operate on\n"
	"  -d, --dcd-file=DEVICE      set device of the DCD device to operate on\n"
	"  -P, --ptt-type=TYPE        set type of the PTT device to operate on\n"
	"  -D, --dcd-type=TYPE        set type of the DCD device to operate on\n"
	"  -c, --civaddr=ID           set CI-V address (for Icom rigs only)\n"
	"  -l, --list                 list all model numbers and exit\n"
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


