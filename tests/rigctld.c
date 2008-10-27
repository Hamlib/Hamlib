/*
 * rigctld.c - (C) Stephane Fillod 2000-2008
 *
 * This program test/control a radio using Hamlib.
 * It takes commands from network connection.
 *
 * $Id: rigctld.c,v 1.9 2008-10-27 22:23:36 fillods Exp $  
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

#include <sys/types.h>          /* See NOTES */

#ifdef HAVE_NETINET_IN_H
#include <netinet/in.h>
#endif
#ifdef HAVE_ARPA_INET_H
#include <arpa/inet.h>
#endif
#ifdef HAVE_SYS_SOCKET_H
#include <sys/socket.h>
#elif HAVE_WS2TCPIP_H
#include <ws2tcpip.h>
#endif

#ifdef HAVE_PTHREAD
#include <pthread.h>
#endif

#include <hamlib/rig.h>
#include "misc.h"
#include "iofunc.h"
#include "serial.h"
#include "sprintflst.h"

#include "rigctl_parse.h"

/*
 * Reminder: when adding long options, 
 * 		keep up to date SHORT_OPTIONS, usage()'s output and man page. thanks.
 * NB: do NOT use -W since it's reserved by POSIX.
 * TODO: add an option to read from a file
 */
#define SHORT_OPTIONS "m:r:p:d:P:D:s:c:lC:t:T:LeuovhV"
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
	{"port",  1, 0, 't'},
	{"listen-addr",  1, 0, 'T'},
	{"list",     0, 0, 'l'},
	{"set-conf", 1, 0, 'C'},
	{"show-conf",0, 0, 'L'},
	{"dump-caps",  0, 0, 'u'},
	{"vfo",  0, 0, 'o'},
	{"end-marker", 0, 0, 'e'},
	{"verbose",  0, 0, 'v'},
	{"help",     0, 0, 'h'},
	{"version",  0, 0, 'V'},
	{0, 0, 0, 0}
};

struct handle_data {
	RIG *rig;
	int sock;
	struct sockaddr_in cli_addr;
	socklen_t clilen;
};

void * handle_socket(void * arg);
void usage(void);

int interactive = 1;    /* no cmd because of daemon */
int prompt= 0 ;         /* Daemon mode for rigparse return string */
int opt_end= 0 ;        /* END marker for rigctld */

int portno = 4532;
uint32_t src_addr = INADDR_ANY;

#define MAXCONFLEN 128

int main (int argc, char *argv[])
{ 
	RIG *my_rig;		/* handle to rig (instance) */
	rig_model_t my_model = RIG_MODEL_DUMMY;

	int retcode;		/* generic return code from functions */

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

	int sock_listen;
	struct sockaddr_in serv_addr;
	int reuseaddr = 1;
	int a0,a1,a2,a3;

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
			case 't':
				if (!optarg) {
					usage();	/* wrong arg count */
					exit(1);
				}
				portno = atoi(optarg);
				break;
			case 'T':
				if (!optarg) {
					usage();	/* wrong arg count */
					exit(1);
				}
				if (4 != sscanf(optarg, "%d.%d.%d.%d", &a0,&a1,&a2,&a3)) {
					usage();	/* wrong arg count */
					exit(1);
				}
				src_addr = (a0<<24)|(a1<<16)|(a2<<8)|a3;
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
			case 'e':
				opt_end = 1;
				break;
			default:
				usage();	/* unknown option? */
				exit(1);
		}
	}

	rig_set_debug(verbose<2 ? RIG_DEBUG_WARN: verbose);

	rig_debug(RIG_DEBUG_VERBOSE, "rigctld, %s\n", hamlib_version);
	rig_debug(RIG_DEBUG_VERBOSE, "Report bugs to "
			"<hamlib-developer@lists.sourceforge.net>\n\n");

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

	/*
	 * Prepare listening socket
	 */
	sock_listen = socket(AF_INET, SOCK_STREAM, 0); 
	if (sock_listen < 0) {
		perror("ERROR opening socket");
		exit(2);
	}
	memset((char *) &serv_addr, 0, sizeof(serv_addr));

	serv_addr.sin_family = AF_INET;
	serv_addr.sin_port = htons(portno);
	serv_addr.sin_addr.s_addr = htonl(src_addr);


	if (setsockopt(sock_listen, SOL_SOCKET, SO_REUSEADDR,
				(char *)&reuseaddr,sizeof(reuseaddr)) < 0) {
		rig_debug(RIG_DEBUG_ERR, "setsockopt: %s\n", strerror(errno));
		exit (1);
	}
	if (bind(sock_listen, (struct sockaddr *) &serv_addr, sizeof(serv_addr)) < 0) {
		rig_debug(RIG_DEBUG_ERR, "binding: %s\n", strerror(errno));
		exit (1);
	}
	if (listen(sock_listen,4) < 0) {
		rig_debug(RIG_DEBUG_ERR, "listening: %s\n", strerror(errno));
		exit (1);
	}

	/*
	 * main loop accepting connections
	 */
	do {
#ifdef HAVE_PTHREAD
		pthread_t thread;
		pthread_attr_t attr;
#endif
		struct handle_data *arg;

		arg = malloc(sizeof(struct handle_data));
		if (!arg) {
			rig_debug(RIG_DEBUG_ERR, "malloc: %s\n", strerror(errno));
			exit (1);
		}

		arg->rig = my_rig;
		arg->clilen = sizeof(arg->cli_addr);
		arg->sock = accept(sock_listen, (struct sockaddr *) &arg->cli_addr,
				&arg->clilen);
		if (arg->sock < 0) {
			rig_debug(RIG_DEBUG_ERR, "accept: %s\n", strerror(errno));
			break;
		}

		rig_debug(RIG_DEBUG_VERBOSE, "Connection opened from %s:%d\n",
				inet_ntoa(arg->cli_addr.sin_addr),
				ntohs(arg->cli_addr.sin_port));

#ifdef HAVE_PTHREAD
		pthread_attr_init(&attr);
		pthread_attr_setdetachstate(&attr, PTHREAD_CREATE_DETACHED);

		retcode = pthread_create(&thread, &attr, handle_socket, arg);
		if (retcode != 0) {
			rig_debug(RIG_DEBUG_ERR, "pthread_create: %s\n", strerror(retcode));
			break;
		}
#else
		handle_socket(arg);
#endif
	}
	while (retcode == 0);

	rig_close(my_rig); /* close port */
	rig_cleanup(my_rig); /* if you care about memory */

	return 0;
}


/*
 * This is the function run by the threads
 */
void * handle_socket(void *arg)
{
	struct handle_data *handle_data_arg = (struct handle_data *)arg;
	FILE *fsockin;
	FILE *fsockout;
	int retcode;

	fsockin = fdopen(handle_data_arg->sock, "rb");
	if (!fsockin) {
		rig_debug(RIG_DEBUG_ERR, "fdopen in: %s\n", strerror(errno));
		close(handle_data_arg->sock);
		free(arg);
#ifdef HAVE_PTHREAD
		pthread_exit(NULL);
#endif
		return NULL;
	}

	fsockout = fdopen(handle_data_arg->sock, "wb");
	if (!fsockout) {
		rig_debug(RIG_DEBUG_ERR, "fdopen out: %s\n", strerror(errno));
		fclose(fsockin);
		close(handle_data_arg->sock);
		free(arg);
#ifdef HAVE_PTHREAD
		pthread_exit(NULL);
#endif
		return NULL;
	}

	do {
		retcode = rigctl_parse(handle_data_arg->rig, fsockin, fsockout, NULL, 0);
		if (ferror(fsockin) || ferror(fsockout))
			retcode = 1;
	}
	while (retcode == 0);

	rig_debug(RIG_DEBUG_VERBOSE, "Connection closed from %s:%d\n",
				inet_ntoa(handle_data_arg->cli_addr.sin_addr),
				ntohs(handle_data_arg->cli_addr.sin_port));

	fclose(fsockin);
	fclose(fsockout);
	close(handle_data_arg->sock);
	free(arg);

#ifdef HAVE_PTHREAD
	pthread_exit(NULL);
#endif
	return NULL;
}

void usage(void)
{
	printf("Usage: rigctld [OPTION]...\n"
	   "Daemon serving COMMANDs to a connected radio transceiver or receiver.\n\n");


	printf(
	"  -m, --model=ID             select radio model number. See model list\n"
	"  -r, --rig-file=DEVICE      set device of the radio to operate on\n"
	"  -p, --ptt-file=DEVICE      set device of the PTT device to operate on\n"
	"  -d, --dcd-file=DEVICE      set device of the DCD device to operate on\n"
	"  -P, --ptt-type=TYPE        set type of the PTT device to operate on\n"
	"  -D, --dcd-type=TYPE        set type of the DCD device to operate on\n"
	"  -s, --serial-speed=BAUD    set serial speed of the serial port\n"
	"  -c, --civaddr=ID           set CI-V address, decimal (for Icom rigs only)\n"
	"  -t, --port=NUM             set TCP listening port, default %d\n"
	"  -T, --listen-addr=IPADDR   set listening IP address, default ANY\n"
	"  -C, --set-conf=PARM=VAL    set config parameters\n"
	"  -L, --show-conf            list all config parameters\n"
	"  -l, --list                 list all model numbers and exit\n"
	"  -u, --dump-caps            dump capabilities and exit\n"
	"  -o, --vfo                  do not default to VFO_CURR, require extra vfo arg\n"
	"  -e, --end-marker           use END marker in rigctld protocol\n"
	"  -v, --verbose              set verbose mode, cumulative\n"
	"  -h, --help                 display this help and exit\n"
	"  -V, --version              output version information and exit\n\n",
			portno );

	usage_rig(stdout);

	printf("\nReport bugs to <hamlib-developer@lists.sourceforge.net>.\n");

}


