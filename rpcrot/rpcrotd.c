/*
 * rpcrotd - (C) Stephane Fillod 2001-2002
 *
 * This program let programs control a rotator through
 * the mean of RPC services using Hamlib.
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

#include "rpcrot.h"
#include <stdio.h>
#include <stdlib.h>
#include <rpc/pmap_clnt.h>
#include <string.h>
#include <memory.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <unistd.h>
#include <netdb.h>
#ifdef HAVE_RPC_RPCENT_H
#include <rpc/rpcent.h>
#endif

#include <getopt.h>

#include <hamlib/rotator.h>


/*
 * Prototypes
 */
void usage();
void version();
void rotd_exit();

void rotprog_1(struct svc_req *rqstp, register SVCXPRT *transp);

/*
 * Reminder: when adding long options,
 *		keep up to date SHORT_OPTIONS, usage()'s output and man page. thanks.
 * NB: do NOT use -W since it's reserved by POSIX.
 * TODO: add an option to read from a file
 */
#define SHORT_OPTIONS "m:r:s:C:t:vhV"
static struct option long_options[] =
{
	{"model",    1, 0, 'm'},
	{"rot-file", 1, 0, 'r'},
	{"serial-speed", 1, 0, 's'},
	{"set-conf", 1, 0, 'C'},
	{"prog", 1, 0, 't'},
	{"verbose",  0, 0, 'v'},
	{"help",     0, 0, 'h'},
	{"version",  0, 0, 'V'},
	{0, 0, 0, 0}
};


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


static unsigned long extract_prognum(const char val[])
{
	if (val[0] == '+') {
		return ROTPROG + atol(val+1);
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

ROT *the_rpc_rot;


#define MAXCONFLEN 128

int
main (int argc, char *argv[])
{
	register SVCXPRT *transp;

	rot_model_t my_model = ROT_MODEL_DUMMY;
	int retcode;		/* generic return code from functions */

	int verbose = 0;
	const char *rot_file=NULL;
	int serial_rate = 0;
	char conf_parms[MAXCONFLEN] = "";
	unsigned long prognum = ROTPROG;

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


	/* Opening the local rotator */

	the_rpc_rot = rot_init(my_model);

	if (!the_rpc_rot) {
			fprintf(stderr, "Unknown rotator num %d, or initialization error.\n",
							my_model);
			fprintf(stderr, "Please check with rotctl --list option.\n");
			exit(2);
	}

	retcode = set_conf(the_rpc_rot, conf_parms);
	if (retcode != RIG_OK) {
			fprintf(stderr, "Config parameter error: %s\n", rigerror(retcode));
			exit(2);
	}

	if (rot_file)
		strncpy(the_rpc_rot->state.rotport.pathname, rot_file, FILPATHLEN - 1);
	/* FIXME: bound checking and port type == serial */
	if (serial_rate != 0)
		the_rpc_rot->state.rotport.parm.serial.rate = serial_rate;

	retcode = rot_open(the_rpc_rot);
	if (retcode != RIG_OK) {
			fprintf(stderr,"rot_open: error = %s \n", rigerror(retcode));
			exit(2);
	}

	atexit(rotd_exit);

	if (verbose > 0)
			printf("Opened rotator model %d, '%s'\n", the_rpc_rot->caps->rot_model,
							the_rpc_rot->caps->model_name);


	/* Setup the RPC service and fire it up */


	pmap_unset (prognum, ROTVERS);

	transp = svcudp_create(RPC_ANYSOCK);  /* Looks like we can spesify a socket number here?? */
	if (transp == NULL) {
		fprintf (stderr, "cannot create udp service.");
		exit(1);
	}
	if (!svc_register(transp, prognum, ROTVERS, rotprog_1, IPPROTO_UDP)) {
		fprintf (stderr, "unable to register (%lu, %u, udp).", prognum, ROTVERS);
		exit(1);
	}

	transp = svctcp_create(RPC_ANYSOCK, 0, 0);
	if (transp == NULL) {
		fprintf (stderr, "%s", "cannot create tcp service.");
		exit(1);
	}
	if (!svc_register(transp, prognum, ROTVERS, rotprog_1, IPPROTO_TCP)) {
		fprintf (stderr, "unable to register (%lu, %u, tcp).", prognum, ROTVERS);
		exit(1);
	}

	svc_run ();
	fprintf (stderr, "svc_run returned");

	/* the rotator gets automatically closed in rotd_exit() */

	exit (1);
	/* NOTREACHED */
}

void rotd_exit()
{
	rot_close(the_rpc_rot);
	rot_cleanup(the_rpc_rot);
}

void usage()
{
	printf("Usage: rpc.rotd [OPTION]... [COMMAND]...\n"
		   "Offer RPC services to remotely control a connected rotator.\n\n");

	printf(
	"  -m, --model=ID             select rotator model number. See model list\n"
	"  -r, --rot-file=DEVICE      set device of the rotator to operate on\n"
	"  -s, --serial-speed=BAUD    set serial speed of the serial port\n"
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
	printf("rpcrotd, %s\n\n", hamlib_version);
	printf("%s\n", hamlib_copyright);
}

/* end of file */

