/*
 * rotctl_parse.c - (C) Stephane Fillod 2000-2010
 *
 * This program test/control a rotator using Hamlib.
 * It takes commands in interactive mode as well as
 * from command line options.
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
					const char*, const char*, const char*, const char*, const char*);
	int flags;
	const char *arg1;
	const char *arg2;
	const char *arg3;
	const char *arg4;
	const char *arg5;
	const char *arg6;
};

#define CHKSCN1ARG(a) if ((a) != 1) return -RIG_EINVAL; else do {} while(0)

#define declare_proto_rot(f) static int (f)(ROT *rot, FILE *fout, int interactive, \
			const struct test_table *cmd, const char *arg1, const char *arg2, \
			const char *arg3, const char *arg4, const char *arg5, const char *arg6)

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
declare_proto_rot(dump_caps);
/* Follows are functions from locator.c */
declare_proto_rot(loc2lonlat);
declare_proto_rot(lonlat2loc);
declare_proto_rot(d_m_s2dec);
declare_proto_rot(dec2d_m_s);
declare_proto_rot(d_mm2dec);
declare_proto_rot(dec2d_mm);
declare_proto_rot(coord2qrb);
declare_proto_rot(az_sp2az_lp);
declare_proto_rot(dist_sp2dist_lp);

/*
 * convention: upper case cmd is set, lowercase is get
 *
 * NB: 'q' 'Q' '?' are reserved by interactive mode interface
 */
struct test_table test_list[] = {
	{ 'P', "set_pos",   set_position,   ARG_IN, "Azimuth", "Elevation" },
	{ 'p', "get_pos",   get_position,   ARG_OUT, "Azimuth", "Elevation" },
	{ 'K', "park",      park,           ARG_NONE, },
	{ 'S', "stop",      stop,           ARG_NONE, },
	{ 'R', "reset",     reset,          ARG_IN, "Reset" },
	{ 'M', "move",      move,           ARG_IN, "Direction", "Speed" },
	{ 'C', "set_conf",  inter_set_conf, ARG_IN, "Token", "Value" },
	{ '_', "get_info",  get_info,       ARG_OUT, "Info" },
	{ 'w', "send_cmd",  send_cmd,       ARG_IN1|ARG_IN_LINE|ARG_OUT2, "Cmd", "Reply" },
	{ '1', "dump_caps", dump_caps, },
	{ 0x8f,"dump_state",dump_state,     ARG_OUT },
	{ 'L', "lonlat2loc",lonlat2loc,     ARG_IN1|ARG_IN2|ARG_IN3|ARG_OUT1, "Longitude", "Latitude", "Loc Len [2-12]", "Locator" },
	{ 'l', "loc2lonlat",loc2lonlat,     ARG_IN1|ARG_OUT1|ARG_OUT2, "Locator", "Longitude", "Latitude" },
	{ 'D', "dms2dec",   d_m_s2dec,      ARG_IN1|ARG_IN2|ARG_IN3|ARG_IN4|ARG_OUT1, "Degrees", "Minutes", "Seconds", "S/W", "Dec Degrees" },
	{ 'd', "dec2dms",   dec2d_m_s,      ARG_IN1|ARG_OUT1|ARG_OUT2|ARG_OUT3|ARG_OUT4, "Dec Degrees", "Degrees", "Minutes", "Seconds", "S/W" },
	{ 'E', "dmmm2dec",  d_mm2dec,       ARG_IN1|ARG_IN2|ARG_IN3|ARG_OUT1, "Degrees", "Dec Minutes", "S/W", "Dec Deg" },
	{ 'e', "dec2dmmm",  dec2d_mm,       ARG_IN1|ARG_OUT1|ARG_OUT2|ARG_OUT3, "Dec Deg", "Degrees", "Dec Minutes", "S/W" },
	{ 'B', "qrb",       coord2qrb,      ARG_IN1|ARG_IN2|ARG_IN3|ARG_IN4|ARG_OUT1|ARG_OUT2, "Lon 1", "Lat 1", "Lon 2", "Lat 2", "QRB Distance", "QRB Azimuth" },
	{ 'A', "a_sp2a_lp", az_sp2az_lp,    ARG_IN1|ARG_OUT1, "Short Path Deg", "Long Path Deg" },
	{ 'a', "d_sp2d_lp", dist_sp2dist_lp,ARG_IN1|ARG_OUT1, "Short Path km", "Long Path km" },
	{ 0x00, "", NULL },

};

struct test_table *find_cmd_entry(int cmd)
{
	int i;
	for (i = 0; i < MAXNBOPT && test_list[i].cmd != 0x00; i++)
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
		for (i = 0; i < MAXNBOPT && test_list[i].cmd != 0; i++)
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

#define fprintf_flush(f, a...) \
            ({ int __ret; \
               __ret = fprintf((f), a); \
                          fflush((f)); \
               __ret; \
            })


#define MAXARGSZ 127

extern int interactive;
extern int prompt;
extern int opt_end;
extern char send_cmd_term;
int ext_resp = 0;
unsigned char resp_sep = '\n';      /* Default response separator */

int rotctl_parse(ROT *my_rot, FILE *fin, FILE *fout, char *argv[], int argc)
{
	int retcode;            /* generic return code from functions */
	unsigned char cmd;
	struct test_table *cmd_entry;

	char arg1[MAXARGSZ + 1], *p1;
	char arg2[MAXARGSZ + 1], *p2;
	char arg3[MAXARGSZ + 1], *p3;
	char arg4[MAXARGSZ + 1], *p4;
	char *p5, *p6;
	static int last_was_ret = 1;

	if (interactive) {
		if (prompt)
			fprintf_flush(fout, "\nRotator command: ");

		do {
			if (scanfc(fin, "%c", &cmd) < 1)
				return -1;

			/* Extended response protocol requested with leading '+' on command
			 * string--rotctld only!
			 */
			if (cmd == '+' && !prompt) {
				ext_resp = 1;
				if (scanfc(fin, "%c", &cmd) < 1)
					return -1;
			} else if (cmd == '+' && prompt) {
				return 0;
			}

			if (cmd != '\\' && cmd != '_' && cmd != '#' && ispunct(cmd) && !prompt) {
				ext_resp = 1;
				resp_sep = cmd;
				if (scanfc(fin, "%c", &cmd) < 1)
					return -1;
			} else if (cmd != '\\' && cmd != '?' && cmd != '_' && cmd != '#' && ispunct(cmd) && prompt) {
				return 0;
			}

			/* command by name */
			if (cmd == '\\') {
				unsigned char cmd_name[MAXNAMSIZ], *pcmd = cmd_name;
				int c_len = MAXNAMSIZ;

				if (scanfc(fin, "%c", pcmd) < 1)
					return -1;

				while(c_len-- && (isalnum(*pcmd) || *pcmd == '_' ))
					if (scanfc(fin, "%c", ++pcmd) < 1)
						return -1;

				*pcmd = '\0';
				cmd = parse_arg((char *) cmd_name);
				break;
			}

			if (cmd == 0x0a || cmd == 0x0d) {
				if (last_was_ret) {
					if (prompt) {
						fprintf_flush(fout, "? for help, q to quit.\n");
					}
					return 0;
				}
				last_was_ret = 1;
			}
		} while (cmd == 0x0a || cmd == 0x0d);

		last_was_ret = 0;

		/* comment line */
		if (cmd == '#') {
			while( cmd != '\n' && cmd != '\r')
				if (scanfc(fin, "%c", &cmd) < 1)
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
		fprintf_flush(stderr, "Command '%c' not found!\n", cmd);
		return 0;
	}

	p1 = p2 = p3 = p4 = p5 = p6 = NULL;

	if ((cmd_entry->flags & ARG_IN_LINE) &&
			(cmd_entry->flags & ARG_IN1) && cmd_entry->arg1) {
		if (interactive) {
			char *nl;
			if (prompt)
				fprintf_flush(fout, "%s: ", cmd_entry->arg1);
			if (fgets(arg1, MAXARGSZ, fin) == NULL)
                return -1;
			if (arg1[0] == 0xa)
				if (fgets(arg1, MAXARGSZ, fin) == NULL)
                    return -1;
			nl = strchr(arg1, 0xa);
			if (nl) *nl = '\0';	/* chomp */
			p1 = arg1[0]==' '?arg1+1:arg1;
		} else {
			if (!argv[optind]) {
				fprintf(stderr, "Invalid arg for command '%s'\n",
							cmd_entry->name);
				exit(1);
			}
			p1 = argv[optind++];
		}
	} else 	if ((cmd_entry->flags & ARG_IN1) && cmd_entry->arg1) {
		if (interactive) {
			if (prompt)
				fprintf_flush(fout, "%s: ", cmd_entry->arg1);
			if (scanfc(fin, "%s", arg1) < 1)
				return -1;
			p1 = arg1;
		} else {
			if (!argv[optind]) {
				fprintf(stderr, "Invalid arg for command '%s'\n",
							cmd_entry->name);
				exit(1);
			}
			p1 = argv[optind++];
		}
	}
	if (p1 && p1[0]!='?' && (cmd_entry->flags & ARG_IN2) && cmd_entry->arg2) {
		if (interactive) {
			if (prompt)
				fprintf_flush(fout, "%s: ", cmd_entry->arg2);
			if (scanfc(fin, "%s", arg2) < 1)
				return -1;
			p2 = arg2;
		} else {
			if (!argv[optind]) {
				fprintf(stderr, "Invalid arg for command '%s'\n",
							cmd_entry->name);
				exit(1);
			}
			p2 = argv[optind++];
		}
	}
	if (p1 && p1[0]!='?' && (cmd_entry->flags & ARG_IN3) && cmd_entry->arg3) {
		if (interactive) {
			if (prompt)
				fprintf_flush(fout, "%s: ", cmd_entry->arg3);
			if (scanfc(fin, "%s", arg3) < 1)
				return -1;
			p3 = arg3;
		} else {
			if (!argv[optind]) {
				fprintf(stderr, "Invalid arg for command '%s'\n",
							cmd_entry->name);
				exit(1);
			}
			p3 = argv[optind++];
		}
	}

	if (p1 && p1[0]!='?' && (cmd_entry->flags & ARG_IN4) && cmd_entry->arg4) {
		if (interactive) {
			if (prompt)
				fprintf_flush(fout, "%s: ", cmd_entry->arg4);
			if (scanfc(fin, "%s", arg4) < 1)
				return -1;
			p4 = arg4;
		} else {
			if (!argv[optind]) {
				fprintf(stderr, "Invalid arg for command '%s'\n",
							cmd_entry->name);
				exit(1);
			}
			p4 = argv[optind++];
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
		rig_debug(RIG_DEBUG_TRACE, "rotctl(d): %c '%s' '%s' '%s' '%s'\n",
				cmd, p1?p1:"", p2?p2:"", p3?p3:"", p4?p4:"");

    /*
     * Extended Response protocol: output received command name and arguments
     * response.
     */
    if (interactive && ext_resp && !prompt) {
        char a1[MAXARGSZ + 1];
        char a2[MAXARGSZ + 1];
        char a3[MAXARGSZ + 1];
        char a4[MAXARGSZ + 1];

        p1 == NULL ? a1[0] = '\0' : snprintf(a1, sizeof(a1), " %s", p1);
        p2 == NULL ? a2[0] = '\0' : snprintf(a2, sizeof(a2), " %s", p2);
        p3 == NULL ? a3[0] = '\0' : snprintf(a3, sizeof(a3), " %s", p3);
        p4 == NULL ? a4[0] = '\0' : snprintf(a4, sizeof(a4), " %s", p4);

        fprintf(fout, "%s:%s%s%s%s%c", cmd_entry->name, a1, a2, a3, a4, resp_sep);
    }

	retcode = (*cmd_entry->rot_routine)(my_rot, fout, interactive,
					cmd_entry, p1, p2 ? p2 : "", p3 ? p3 : "",
                    p4 ? p4 : "", p5 ? p5 : "", p6 ? p6 : "");

#ifdef HAVE_PTHREAD
	pthread_mutex_unlock(&rot_mutex);
#endif

	if (retcode != RIG_OK) {
		/* only for rotctld */
		if (interactive && !prompt) {
			fprintf(fout, NETROTCTL_RET "%d\n", retcode);
			ext_resp = 0;
			resp_sep = '\n';
		}
		else
			fprintf(fout, "%s: error = %s\n", cmd_entry->name, rigerror(retcode));
	} else {
		/* only for rotctld */
		if (interactive && !prompt) {
			/* netrotctl RIG_OK */
			if (!(cmd_entry->flags & ARG_OUT) && !opt_end && !ext_resp)
				fprintf(fout, NETROTCTL_RET "0\n");

			/* Extended Response protocol */
			else if (ext_resp && cmd != 0xf0) {
				fprintf(fout, NETROTCTL_RET "0\n");
				ext_resp = 0;
				resp_sep = '\n';
			}

			/* Nate's protocol (obsolete) */
			else if ((cmd_entry->flags & ARG_OUT) && opt_end)
				fprintf(fout, "END\n");
		}
	}

	fflush(fout);

	return retcode != RIG_OK ? 2 : 0;
}



void version()
{
	printf("rotctl, %s\n\n", hamlib_version);
	printf("%s\n", hamlib_copyright);
}


void usage_rot(FILE *fout)
{
	int i, nbspaces;

	fprintf(fout, "Commands (some may not be available for this rig):\n");
	for (i = 0; test_list[i].cmd != 0; i++) {
		fprintf(fout, "%c: %-12s(", isprint(test_list[i].cmd) ?
				test_list[i].cmd : '?', test_list[i].name);

		nbspaces = 16;
		if (test_list[i].arg1 && (test_list[i].flags&ARG_IN1))
				nbspaces -= fprintf(fout, "%s", test_list[i].arg1);
		if (test_list[i].arg2 && (test_list[i].flags&ARG_IN2))
				nbspaces -= fprintf(fout, ", %s", test_list[i].arg2);
		if (test_list[i].arg3 && (test_list[i].flags&ARG_IN3))
				nbspaces -= fprintf(fout, ", %s", test_list[i].arg3);
		if (test_list[i].arg4 && (test_list[i].flags&ARG_IN4))
				nbspaces -= fprintf(fout, ", %s", test_list[i].arg4);

		fprintf(fout, ")\n");
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
		for (i = 1 ; i < RIG_COMBO_MAX && cfp->u.c.combostr[i]; i++)
			printf(", %s", cfp->u.c.combostr[i]);
		printf("\n");
		break;
    default:
		break;
	}

	return 1;  /* != 0, we want them all ! */
}

static int print_model_list(const struct rot_caps *caps, void *data)
{
	printf("%-8d%-16s%-25s%-10s%s\n", caps->rot_model, caps->mfg_name,
			caps->model_name, caps->version, rig_strstatus(caps->status));
	return 1;  /* !=0, we want them all ! */
}

void list_models()
{
	int status;

	rot_load_all_backends();

	printf("Rot#    Mfg             Model                    Vers.\n");
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

/* 'P' */
declare_proto_rot(set_position)
{
	azimuth_t az;
	elevation_t el;

	CHKSCN1ARG(sscanf(arg1, "%f", &az));
	CHKSCN1ARG(sscanf(arg2, "%f", &el));
	return rot_set_position(rot, az, el);
}

/* 'p' */
declare_proto_rot(get_position)
{
	int status;
	azimuth_t az;
	elevation_t el;

	status = rot_get_position(rot, &az, &el);
	if (status != RIG_OK)
		return status;
	if ((interactive && prompt) || (interactive && !prompt && ext_resp))
		fprintf(fout, "%s: ", cmd->arg1);
	fprintf(fout, "%f%c", az, resp_sep);
	if ((interactive && prompt) || (interactive && !prompt && ext_resp))
		fprintf(fout, "%s: ", cmd->arg2);
	fprintf(fout, "%f%c", el, resp_sep);

	return status;
}

/* 'S' */
declare_proto_rot(stop)
{
	return rot_stop(rot);
}

/* 'K' */
declare_proto_rot(park)
{
	return rot_park(rot);
}

/* 'R' */
declare_proto_rot(reset)
{
	rot_reset_t reset;

	CHKSCN1ARG(sscanf(arg1, "%d", &reset));
	return rot_reset(rot, reset);
}

/* '_' */
declare_proto_rot(get_info)
{
	const char *s;

	s = rot_get_info(rot);
	if ((interactive && prompt) || (interactive && !prompt && ext_resp))
		fprintf(fout, "%s: ", cmd->arg1);
	fprintf(fout, "%s%c", s ? s : "None", resp_sep);

	return RIG_OK;
}

/* 'M' */
declare_proto_rot(move)
{
	int direction;
	int speed;

    if (!strcmp(arg1, "LEFT") || !strcmp(arg1, "CCW"))
        direction = ROT_MOVE_LEFT;
    else
    if (!strcmp(arg1, "RIGHT") || !strcmp(arg1, "CW"))
        direction = ROT_MOVE_RIGHT;
    else
    if (!strcmp(arg1, "UP"))
        direction = ROT_MOVE_UP;
    else
    if (!strcmp(arg1, "DOWN"))
        direction = ROT_MOVE_DOWN;
    else
	    CHKSCN1ARG(sscanf(arg1, "%d", &direction));

	CHKSCN1ARG(sscanf(arg2, "%d", &speed));
	return rot_move(rot, direction, speed);
}

/* 'C' */
declare_proto_rot(inter_set_conf)
{
	token_t token;

	CHKSCN1ARG(sscanf(arg1, "%ld", &token));
    if (!arg2 || arg2[0] == '\0')
        return -RIG_EINVAL;
	return rot_set_conf(rot, token, arg2);
}


/* '1' */
declare_proto_rot(dump_caps)
{
	dumpcaps_rot(rot, fout);

	return RIG_OK;
}


/* For rotctld internal use
 * '0x8f'
 */
declare_proto_rot(dump_state)
{
	struct rot_state *rs = &rot->state;

	/*
	 * - Protocol version
	 */
#define ROTCTLD_PROT_VER 0
	if ((interactive && prompt) || (interactive && !prompt && ext_resp))
		fprintf(fout, "rotctld Protocol Ver: ");
	fprintf(fout, "%d%c", ROTCTLD_PROT_VER, resp_sep);

	if ((interactive && prompt) || (interactive && !prompt && ext_resp))
		fprintf(fout, "Rotor Model: ");
	fprintf(fout, "%d%c", rot->caps->rot_model, resp_sep);

	if ((interactive && prompt) || (interactive && !prompt && ext_resp))
		fprintf(fout, "Minimum Azimuth: ");
	fprintf(fout, "%lf%c", rs->min_az, resp_sep);

	if ((interactive && prompt) || (interactive && !prompt && ext_resp))
		fprintf(fout, "Maximum Azimuth: ");
	fprintf(fout, "%lf%c", rs->max_az, resp_sep);

	if ((interactive && prompt) || (interactive && !prompt && ext_resp))
		fprintf(fout, "Minimum Elevation: ");
	fprintf(fout, "%lf%c", rs->min_el, resp_sep);

	if ((interactive && prompt) || (interactive && !prompt && ext_resp))
		fprintf(fout, "Maximum Elevation: ");
	fprintf(fout, "%lf%c", rs->max_el, resp_sep);

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
	char eom_buf[4] = { 0xa, 0xd, 0, 0 };

	/*
	 * binary protocols enter values as \0xZZ\0xYY..
	 *
	 * Rem: no binary protocol for rotator as of now
	 */
	backend_num = ROT_BACKEND_NUM(rot->caps->rot_model);
	if (send_cmd_term == -1 || backend_num == -1) {
		const char *p = arg1, *pp = NULL;
		int i;
		for (i = 0; i < BUFSZ - 1 && p != pp; i++) {
			pp = p + 1;
			bufcmd[i] = strtol(p + 1, (char **) &p, 0);
		}
		/* must save length to allow 0x00 to be sent as part of a command
		*/
		cmd_len = i - 1;

		/* no End Of Message chars */
		eom_buf[0] = '\0';
	} else {
		/* text protocol */

		strncpy(bufcmd,arg1, BUFSZ);
		bufcmd[BUFSZ - 2] = '\0';

		cmd_len = strlen(bufcmd);

		/* Automatic termination char */
		if (send_cmd_term != 0)
			bufcmd[cmd_len++] = send_cmd_term;
		eom_buf[2] = send_cmd_term;
	}

	rs = &rot->state;

	serial_flush(&rs->rotport);

	retval = write_block(&rs->rotport, bufcmd, cmd_len);
	if (retval != RIG_OK)
		return retval;

	if (interactive && prompt)
		fprintf(fout, "%s: ", cmd->arg2);

	do {
		/*
		 * assumes CR or LF is end of line char
		 * for all ascii protocols
		 */
		retval = read_string(&rs->rotport, buf, BUFSZ, eom_buf, strlen(eom_buf));
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

/* 'L' */
declare_proto_rot(lonlat2loc)
{
	unsigned char loc[MAXARGSZ + 1];
	double lat, lon;
	int err, pair;

	CHKSCN1ARG(sscanf(arg1, "%lf", &lon));
	CHKSCN1ARG(sscanf(arg2, "%lf", &lat));
	CHKSCN1ARG(sscanf(arg3, "%d", &pair));

	pair /= 2;

	err = longlat2locator(lon, lat, (char *)&loc, pair);

	if (err != RIG_OK)
		return err;

	if ((interactive && prompt) || (interactive && !prompt && ext_resp))
		fprintf(fout, "%s: ", cmd->arg4);
	fprintf(fout, "%s%c", loc, resp_sep);

	return err;
}

/* 'l' */
declare_proto_rot(loc2lonlat)
{
	unsigned char loc[MAXARGSZ + 1];
	double lat, lon;
	int status;

	CHKSCN1ARG(sscanf(arg1, "%s", (char *)&loc));

	status = locator2longlat(&lon, &lat, (const char *)loc);

	if (status != RIG_OK)
		return status;

	if ((interactive && prompt) || (interactive && !prompt && ext_resp))
		fprintf(fout, "%s: ", cmd->arg2);
	fprintf(fout, "%f%c", lon, resp_sep);
	if ((interactive && prompt) || (interactive && !prompt && ext_resp))
		fprintf(fout, "%s: ", cmd->arg3);
	fprintf(fout, "%f%c", lat, resp_sep);

	return status;
}

/* 'D' */
declare_proto_rot(d_m_s2dec)
{
	int deg, min, sw;
	double sec, dec_deg;

	CHKSCN1ARG(sscanf(arg1, "%d", &deg));
	CHKSCN1ARG(sscanf(arg2, "%d", &min));
	CHKSCN1ARG(sscanf(arg3, "%lf", &sec));
	CHKSCN1ARG(sscanf(arg4, "%d", &sw));

	dec_deg = dms2dec(deg, min, sec, sw);

	if ((interactive && prompt) || (interactive && !prompt && ext_resp))
		fprintf(fout, "%s: ", cmd->arg5);
	fprintf(fout, "%lf%c", dec_deg, resp_sep);

	return RIG_OK;
}

/* 'd' */
declare_proto_rot(dec2d_m_s)
{
	int deg, min, sw, err;
	double sec, dec_deg;

	CHKSCN1ARG(sscanf(arg1, "%lf", &dec_deg));

	err = dec2dms(dec_deg, &deg, &min, &sec, &sw);

	if (err != RIG_OK)
		return err;

	if ((interactive && prompt) || (interactive && !prompt && ext_resp))
		fprintf(fout, "%s: ", cmd->arg2);
	fprintf(fout, "%d%c", deg, resp_sep);
	if ((interactive && prompt) || (interactive && !prompt && ext_resp))
		fprintf(fout, "%s: ", cmd->arg3);
	fprintf(fout, "%d%c", min, resp_sep);
	if ((interactive && prompt) || (interactive && !prompt && ext_resp))
		fprintf(fout, "%s: ", cmd->arg4);
	fprintf(fout, "%lf%c", sec, resp_sep);
	if ((interactive && prompt) || (interactive && !prompt && ext_resp))
		fprintf(fout, "%s: ", cmd->arg5);
	fprintf(fout, "%d%c", sw, resp_sep);

	return err;
}

/* 'E' */
declare_proto_rot(d_mm2dec)
{
	int deg, sw;
	double dec_deg, min;

	CHKSCN1ARG(sscanf(arg1, "%d", &deg));
	CHKSCN1ARG(sscanf(arg2, "%lf", &min));
	CHKSCN1ARG(sscanf(arg3, "%d", &sw));

	dec_deg = dmmm2dec(deg, min, sw);

	if ((interactive && prompt) || (interactive && !prompt && ext_resp))
		fprintf(fout, "%s: ", cmd->arg4);
	fprintf(fout, "%lf%c", dec_deg, resp_sep);

	return RIG_OK;
}

/* 'e' */
declare_proto_rot(dec2d_mm)
{
	int deg, sw, err;
	double min, dec_deg;

	CHKSCN1ARG(sscanf(arg1, "%lf", &dec_deg));

	err = dec2dmmm(dec_deg, &deg, &min, &sw);

	if (err != RIG_OK)
		return err;

	if ((interactive && prompt) || (interactive && !prompt && ext_resp))
		fprintf(fout, "%s: ", cmd->arg2);
	fprintf(fout, "%d%c", deg, resp_sep);
	if ((interactive && prompt) || (interactive && !prompt && ext_resp))
		fprintf(fout, "%s: ", cmd->arg3);
	fprintf(fout, "%lf%c", min, resp_sep);
	if ((interactive && prompt) || (interactive && !prompt && ext_resp))
		fprintf(fout, "%s: ", cmd->arg4);
	fprintf(fout, "%d%c", sw, resp_sep);

	return err;
}

/* 'B' */
declare_proto_rot(coord2qrb)
{
	double lon1, lat1, lon2, lat2, dist, az;
	int err;

	CHKSCN1ARG(sscanf(arg1, "%lf", &lon1));
	CHKSCN1ARG(sscanf(arg2, "%lf", &lat1));
	CHKSCN1ARG(sscanf(arg3, "%lf", &lon2));
	CHKSCN1ARG(sscanf(arg4, "%lf", &lat2));

	err = qrb(lon1, lat1, lon2, lat2, &dist, &az);

	if (err != RIG_OK)
		return err;

	if ((interactive && prompt) || (interactive && !prompt && ext_resp))
		fprintf(fout, "%s: ", cmd->arg5);
	fprintf(fout, "%lf%c", dist, resp_sep);
	if ((interactive && prompt) || (interactive && !prompt && ext_resp))
		fprintf(fout, "%s: ", cmd->arg6);
	fprintf(fout, "%lf%c", az, resp_sep);

	return err;
}

/* 'A' */
declare_proto_rot(az_sp2az_lp)
{
	double az_sp, az_lp;

	CHKSCN1ARG(sscanf(arg1, "%lf", &az_sp));

	az_lp = azimuth_long_path(az_sp);

	if (az_lp < 0)
		return -RIG_EINVAL;

	if ((interactive && prompt) || (interactive && !prompt && ext_resp))
		fprintf(fout, "%s: ", cmd->arg2);
	fprintf(fout, "%lf%c", az_lp, resp_sep);

	return RIG_OK;
}

/* 'a' */
declare_proto_rot(dist_sp2dist_lp)
{
	double dist_sp, dist_lp;

	CHKSCN1ARG(sscanf(arg1, "%lf", &dist_sp));

	dist_lp = distance_long_path(dist_sp);

	if ((interactive && prompt) || (interactive && !prompt && ext_resp))
		fprintf(fout, "%s: ", cmd->arg2);
	fprintf(fout, "%lf%c", dist_lp, resp_sep);

	return RIG_OK;
}

