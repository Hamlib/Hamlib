/*
 * rotctl.c - (C) Stephane Fillod 2000-2008
 *
 * This program test/control a rotator using Hamlib.
 * It takes commands in interactive mode as well as 
 * from command line options.
 *
 *	$Id: rotctl_parse.c,v 1.2 2008-09-21 19:27:54 fillods Exp $  
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

#include <hamlib/rotator.h>
#include "serial.h"
#include "misc.h"

#include "rotctl_parse.h"

#ifdef HAVE_PTHREAD
#include <pthread.h>

static pthread_mutex_t rot_mutex = PTHREAD_MUTEX_INITIALIZER;
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

#define ARG_NONE	0
#define ARG_IN  (ARG_IN1|ARG_IN2|ARG_IN3|ARG_IN4)
#define ARG_OUT  (ARG_OUT1|ARG_OUT2|ARG_OUT3|ARG_OUT4)

struct test_table {
	unsigned char cmd;
	const char *name;
    	int (*rot_routine)(ROT*, FILE*, int, const struct test_table*, const char*, 
					const char*, const char*);
	int flags;
	const char *arg1;
	const char *arg2;
	const char *arg3;
};

#define declare_proto_rot(f) static int (f)(ROT *rot, FILE *fout, int interactive, \
			const struct test_table *cmd, const char *arg1, \
			const char *arg2, const char *arg3)

declare_proto_rot(set_position);
declare_proto_rot(get_position);
declare_proto_rot(stop);
declare_proto_rot(park);
declare_proto_rot(reset);
declare_proto_rot(move);
declare_proto_rot(get_info);
declare_proto_rot(inter_set_conf);  /* interactive mode set_conf */
declare_proto_rot(send_cmd);
declare_proto_rot(dump_state);

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
		{ 'M', "move", move, ARG_IN, "Direction", "Speed" },
		{ 'C', "set_conf", inter_set_conf, ARG_IN, "Token", "Value" },
		{ '_', "get_info", get_info, ARG_OUT, "Info" },
		{ 'w', "send_cmd", send_cmd, ARG_IN1|ARG_IN_LINE|ARG_OUT2, "Cmd", "Reply" },
		{ 0x8f,"dump_state", dump_state, ARG_OUT },
		{ 0x00, "", NULL },

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
 * TODO: use Lex?
 */
char parse_arg(const char *arg)
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

int rotctl_parse(ROT *my_rot, FILE *fin, FILE *fout, char *argv[], int argc)
{ 
	int retcode;            /* generic return code from functions */
	unsigned char cmd;
	struct test_table *cmd_entry;

	char arg1[MAXARGSZ+1], *p1;
	char arg2[MAXARGSZ+1], *p2;
	char arg3[MAXARGSZ+1], *p3;
	static int last_was_ret = 1;

	if (interactive) {
		if (prompt)
			fprintf(fout, "\nRotator command: ");

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
						fprintf(fout, "\nRotator command: ");
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
			usage_rot(fout);
			fflush(fout);
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
	} else 	if ((cmd_entry->flags & ARG_IN1) && cmd_entry->arg1) {
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
	if (p1 && p1[0]!='?' && (cmd_entry->flags & ARG_IN2) && cmd_entry->arg2) {
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
	if (p1 && p1[0]!='?' && (cmd_entry->flags & ARG_IN3) && cmd_entry->arg3) {
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
	pthread_mutex_lock(&rot_mutex);
#endif

	if (!prompt)
		rig_debug(RIG_DEBUG_TRACE, "rotctl: %c '%s' '%s' '%s'\n",
				cmd, p1, p2, p3);

	retcode = (*cmd_entry->rot_routine)(my_rot, fout, interactive, 
					cmd_entry, p1, p2, p3);

#ifdef HAVE_PTHREAD
	pthread_mutex_unlock(&rot_mutex);
#endif

	if (retcode != RIG_OK) {
		if ((cmd_entry->flags & ARG_OUT) && interactive && !prompt)
			fprintf(fout, "ERROR %d\n", retcode);             /* only for rotctld */
		else
			fprintf(fout, "%s: error = %s\n", cmd_entry->name, rigerror(retcode));
	} else {
		if ((cmd_entry->flags & ARG_OUT) && interactive && !prompt)             /* only for rotctld */
			fprintf(fout, "END\n");
	}

	fflush(fout);

	return 0;
}



void version()
{
	printf("rotctl, %s\n\n", hamlib_version);
	printf("%s\n", hamlib_copyright);
}

void usage_rot(FILE *fout)
{
	int i;

	fprintf(fout, "Commands (may not be available for this rotator):\n");
	for (i=0; test_list[i].cmd != 0; i++) {
		fprintf(fout, "%c: %-16s(", test_list[i].cmd, test_list[i].name);
		if (test_list[i].arg1)
			fprintf(fout, "%s", test_list[i].arg1);
		if (test_list[i].arg2)
			fprintf(fout, ",%s", test_list[i].arg2);
		if (test_list[i].arg3)
			fprintf(fout, ",%s", test_list[i].arg3);
		fprintf(fout, ")  \t");

		if (i%2)
			fprintf(fout, "\n");
	}
}

int print_conf_list(const struct confparams *cfp, rig_ptr_t data)
{
	ROT *rot = (ROT*) data;
	int i;
	char buf[128] = "";

	rot_get_conf(rot, cfp->token, buf);
	printf("%s: \"%s\"\n" "\tDefault: %s, Value: %s\n", 
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
          break;
	}

	return 1;  /* !=0, we want them all ! */
}

static int print_model_list(const struct rot_caps *caps, void *data)
{
	printf("%d\t%-14s%-16s%-8s%s\n", caps->rot_model, caps->mfg_name,
			caps->model_name, caps->version, rig_strstatus(caps->status));
	return 1;  /* !=0, we want them all ! */
}

void list_models()
{
	int status;

	rot_load_all_backends();

	printf("Rot#\tMfg           Model           Vers.\n");
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
	if (interactive && prompt)
		fprintf(fout, "%s: ", cmd->arg1);
	fprintf(fout, "%f\n", az);
	if (interactive && prompt)
		fprintf(fout, "%s: ", cmd->arg2);
	fprintf(fout, "%f\n", el);

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
	if (interactive && prompt)
		fprintf(fout, "%s: ", cmd->arg1);
	fprintf(fout, "%s\n", s ? s : "None");

	return RIG_OK;
}

declare_proto_rot(move)
{
        int direction;
        int speed;

        sscanf(arg1, "%d", &direction);
	sscanf(arg2, "%d", &speed);
	return rot_move(rot, direction, speed);
}

declare_proto_rot(inter_set_conf)
{
	token_t token;
	char val[21] = "";      /* 20 chars enough? */

	sscanf(arg1, "%ld", &token);
	sscanf(arg2, "%s", val);
	return rot_set_conf(rot, token, val);
}

/* For rotctld internal use */
declare_proto_rot(dump_state)
{
	struct rot_state *rs = &rot->state;

	/*
	 * - Protocol version
	 */
#define ROTCTLD_PROT_VER 0
	fprintf(fout, "%d\n", ROTCTLD_PROT_VER);
	fprintf(fout, "%d\n", rot->caps->rot_model);

	fprintf(fout, "%lf\n", rs->min_az);
	fprintf(fout, "%lf\n", rs->max_az);
	fprintf(fout, "%lf\n", rs->min_el);
	fprintf(fout, "%lf\n", rs->max_el);

	return RIG_OK;
}

/*
 * special debugging purpose send command
 * display reply until there's a timeout
 *
 * 'w'
 */
declare_proto_rot(send_cmd)
{
	int retval;
	struct rot_state *rs;
	int backend_num, cmd_len;
#define BUFSZ 128
	char bufcmd[BUFSZ];
	char buf[BUFSZ];

	/*
	 * binary protocols enter values as \0xZZ\0xYY..
	 *
	 * Rem: no binary protocol for rotator as of now
	 */
	backend_num = ROT_BACKEND_NUM(rot->caps->rot_model);
	if (backend_num == -1) {
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

	rs = &rot->state;

	serial_flush(&rs->rotport);

	retval = write_block(&rs->rotport, bufcmd, cmd_len);
	if (retval != RIG_OK)
		return retval;

	if (interactive && prompt)
		fprintf(fout, "%s: ", cmd->arg2);

#define EOM "\0xa"
	do {
		retval = read_string(&rs->rotport, buf, BUFSZ, EOM, strlen(EOM));
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

	return retval;
}

