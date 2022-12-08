/*
 * rotctl_parse.c - (C) Stephane Fillod 2000-2010
 *                  (C) Nate Bargmann 2003,2007,2010,2011,2012,2013
 *                  (C) The Hamlib Group 2002,2006,2011
 *
 * This program test/control a rotator using Hamlib.
 * It takes commands in interactive mode as well as
 * from command line options.
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

// TODO: Add "symmetric" set_conf + get_conf to rigctl+rotctl

#include <hamlib/config.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <ctype.h>
#include <errno.h>

#ifdef HAVE_LIBREADLINE
#  if defined(HAVE_READLINE_READLINE_H)
#    include <readline/readline.h>
#  elif defined(HAVE_READLINE_H)    /* !defined(HAVE_READLINE_READLINE_H) */
#    include <readline.h>
#  else                             /* !defined(HAVE_READLINE_H) */
extern char *readline();
#  endif                            /* HAVE_READLINE_H */
#else
/* no readline */
#endif                              /* HAVE_LIBREADLINE */

#ifdef HAVE_READLINE_HISTORY
#  if defined(HAVE_READLINE_HISTORY_H)
#    include <readline/history.h>
#  elif defined(HAVE_HISTORY_H)
#    include <history.h>
#  else                             /* !defined(HAVE_HISTORY_H) */
extern void add_history();
extern int write_history();
extern int read_history();
#  endif                            /* defined(HAVE_READLINE_HISTORY_H) */
/* no history */
#endif                              /* HAVE_READLINE_HISTORY */

#include <hamlib/rotator.h>
#include "serial.h"
#include "misc.h"


/* HAVE_SSLEEP is defined when Windows Sleep is found
 * HAVE_SLEEP is defined when POSIX sleep is found
 * _WIN32 is defined when compiling with MinGW
 *
 * When cross-compiling from POSIX to Windows using MinGW, HAVE_SLEEP
 * will often be defined by configure although it is not supported by
 * MinGW.  So substitute the sleep definition below in such a case and
 * when compiling on Windows using MinGW where HAVE_SLEEP will be
 * undefined.
 *
 * FIXME:  Needs better handling for all versions of MinGW.
 *
 */
#if (defined(HAVE_SSLEEP) || defined(_WIN32)) && (!defined(HAVE_SLEEP))
#  include "hl_sleep.h"
#endif

#include "rotctl_parse.h"
#include "sprintflst.h"

/* Hash table implementation See:  http://uthash.sourceforge.net/ */
#include "uthash.h"

#ifdef HAVE_PTHREAD
#  include <pthread.h>

static pthread_mutex_t rot_mutex = PTHREAD_MUTEX_INITIALIZER;
#endif

#define STR1(S) #S
#define STR(S) STR1(S)

#define MAXNAMSIZ 32
#define MAXNBOPT 100    /* max number of different options */
#define MAXARGSZ 127

#define ARG_IN1  0x01
#define ARG_OUT1 0x02
#define ARG_IN2  0x04
#define ARG_OUT2 0x08
#define ARG_IN3  0x10
#define ARG_OUT3 0x20
#define ARG_IN4  0x40
#define ARG_OUT4 0x80
#define ARG_IN_LINE 0x4000

#define ARG_NONE    0
#define ARG_IN  (ARG_IN1|ARG_IN2|ARG_IN3|ARG_IN4)
#define ARG_OUT  (ARG_OUT1|ARG_OUT2|ARG_OUT3|ARG_OUT4)

/* variables for readline support */
#ifdef HAVE_LIBREADLINE
static char *input_line = (char *)NULL;
static char *result = (char *)NULL;
static char *parsed_input[sizeof(char *) * 7];
static const int have_rl = 1;
#endif

#ifdef HAVE_READLINE_HISTORY
static char *rp_hist_buf = (char *)NULL;
#endif

struct test_table
{
    unsigned char cmd;
    const char *name;
    int (*rot_routine)(ROT *,
                       FILE *,
                       int,
                       int,
                       char,
                       int,
                       char,
                       const struct test_table *,
                       const char *,
                       const char *,
                       const char *,
                       const char *,
                       const char *,
                       const char *);
    int flags;
    const char *arg1;
    const char *arg2;
    const char *arg3;
    const char *arg4;
    const char *arg5;
    const char *arg6;
};

#define CHKSCN1ARG(a) if ((a) != 1) return -RIG_EINVAL; else do {} while(0)

#define ACTION(f) rotctl_##f
#define declare_proto_rot(f) static int (ACTION(f))(ROT *rot,           \
                                                    FILE *fout,         \
                                                    int interactive,    \
                                                    int prompt,         \
                                                    char send_cmd_term, \
                                                    int ext_resp,       \
                                                    char resp_sep,      \
                                                    const struct test_table *cmd, \
                                                    const char *arg1,   \
                                                    const char *arg2,   \
                                                    const char *arg3,   \
                                                    const char *arg4,   \
                                                    const char *arg5,   \
                                                    const char *arg6)

declare_proto_rot(set_position);
declare_proto_rot(get_position);
declare_proto_rot(stop);
declare_proto_rot(park);
declare_proto_rot(reset);
declare_proto_rot(move);
declare_proto_rot(set_level);
declare_proto_rot(get_level);
declare_proto_rot(set_func);
declare_proto_rot(get_func);
declare_proto_rot(set_parm);
declare_proto_rot(get_parm);
declare_proto_rot(get_info);
declare_proto_rot(get_status);
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
declare_proto_rot(pause);

/*
 * convention: upper case cmd is set, lowercase is get
 *
 * NB: 'q' 'Q' '?' are reserved by interactive mode interface
 */
struct test_table test_list[] =
{
    { 'P', "set_pos",       ACTION(set_position),       ARG_IN, "Azimuth", "Elevation" },
    { 'p', "get_pos",       ACTION(get_position),       ARG_OUT, "Azimuth", "Elevation" },
    { 'K', "park",          ACTION(park),               ARG_NONE, },
    { 'S', "stop",          ACTION(stop),               ARG_NONE, },
    { 'R', "reset",         ACTION(reset),              ARG_IN, "Reset" },
    { 'M', "move",          ACTION(move),               ARG_IN, "Direction", "Speed" },
    { 'V',  "set_level",    ACTION(set_level),          ARG_IN, "Level", "Level Value" },
    { 'v',  "get_level",    ACTION(get_level),          ARG_IN1 | ARG_OUT2, "Level", "Level Value" },
    { 'U',  "set_func",     ACTION(set_func),           ARG_IN, "Func", "Func Status" },
    { 'u',  "get_func",     ACTION(get_func),           ARG_IN1 | ARG_OUT2, "Func", "Func Status" },
    { 'X',  "set_parm",     ACTION(set_parm),           ARG_IN, "Parm", "Parm Value" },
    { 'x',  "get_parm",     ACTION(get_parm),           ARG_IN1 | ARG_OUT2, "Parm", "Parm Value" },
    { 'C', "set_conf",      ACTION(inter_set_conf),     ARG_IN, "Token", "Value" },
    { '_', "get_info",      ACTION(get_info),           ARG_OUT, "Info" },
    { 's', "get_status",    ACTION(get_status),         ARG_OUT, "Status flags" },
    { 'w', "send_cmd",      ACTION(send_cmd),           ARG_IN1 | ARG_IN_LINE | ARG_OUT2, "Cmd", "Reply" },
    { '1', "dump_caps",     ACTION(dump_caps), },
    { 0x8f, "dump_state",   ACTION(dump_state),         ARG_OUT },
    { 'L', "lonlat2loc",    ACTION(lonlat2loc),         ARG_IN1 | ARG_IN2 | ARG_IN3 | ARG_OUT1, "Longitude", "Latitude", "Loc Len [2-12]", "Locator" },
    { 'l', "loc2lonlat",    ACTION(loc2lonlat),         ARG_IN1 | ARG_OUT1 | ARG_OUT2, "Locator", "Longitude", "Latitude" },
    { 'D', "dms2dec",       ACTION(d_m_s2dec),          ARG_IN1 | ARG_IN2 | ARG_IN3 | ARG_IN4 | ARG_OUT1, "Degrees", "Minutes", "Seconds", "S/W", "Dec Degrees" },
    { 'd', "dec2dms",       ACTION(dec2d_m_s),          ARG_IN1 | ARG_OUT1 | ARG_OUT2 | ARG_OUT3 | ARG_OUT4, "Dec Degrees", "Degrees", "Minutes", "Seconds", "S/W" },
    { 'E', "dmmm2dec",      ACTION(d_mm2dec),           ARG_IN1 | ARG_IN2 | ARG_IN3 | ARG_OUT1, "Degrees", "Dec Minutes", "S/W", "Dec Deg" },
    { 'e', "dec2dmmm",      ACTION(dec2d_mm),           ARG_IN1 | ARG_OUT1 | ARG_OUT2 | ARG_OUT3, "Dec Deg", "Degrees", "Dec Minutes", "S/W" },
    { 'B', "qrb",           ACTION(coord2qrb),          ARG_IN1 | ARG_IN2 | ARG_IN3 | ARG_IN4 | ARG_OUT1 | ARG_OUT2, "Lon 1", "Lat 1", "Lon 2", "Lat 2", "QRB Distance", "QRB Azimuth" },
    { 'A', "a_sp2a_lp",     ACTION(az_sp2az_lp),        ARG_IN1 | ARG_OUT1, "Short Path Deg", "Long Path Deg" },
    { 'a', "d_sp2d_lp",     ACTION(dist_sp2dist_lp),    ARG_IN1 | ARG_OUT1, "Short Path km", "Long Path km" },
    { 0x8c, "pause",        ACTION(pause),              ARG_IN, "Seconds" },
    { 0x00, "", NULL },

};


struct test_table *find_cmd_entry(int cmd)
{
    int i;

    for (i = 0; test_list[i].cmd != 0x00; i++)
        if (test_list[i].cmd == cmd)
        {
            break;
        }

    if (test_list[i].cmd == 0x00)
    {
        return NULL;
    }

    return &test_list[i];
}


/* Structure for hash table provided by uthash.h
 *
 * Structure and hash functions patterned after/copied from example.c
 * distributed with the uthash package. See:  http://uthash.sourceforge.net/
 */
struct mod_lst
{
    int id;                 /* caps->rot_model This is the hash key */
    char mfg_name[32];      /* caps->mfg_name */
    char model_name[32];    /* caps->model_name */
    char version[32];       /* caps->version */
    char status[32];        /* caps->status */
    char macro_name[64];    /* caps->macro_name */
    UT_hash_handle hh;      /* makes this structure hashable */
};


/* Hash declaration.  Must be initialized to NULL */
struct mod_lst *models = NULL;

/* Add model information to the hash */
void hash_add_model(int id,
                    const char *mfg_name,
                    const char *model_name,
                    const char *version,
                    const char *status,
                    const char *macro_name)
{
    struct mod_lst *s;

    s = (struct mod_lst *)calloc(1, sizeof(struct mod_lst));

    s->id = id;
    SNPRINTF(s->mfg_name, sizeof(s->mfg_name), "%s", mfg_name);
    SNPRINTF(s->model_name, sizeof(s->model_name), "%s", model_name);
    SNPRINTF(s->version, sizeof(s->version), "%s", version);
    SNPRINTF(s->status, sizeof(s->status), "%s", status);
    SNPRINTF(s->macro_name, sizeof(s->macro_name), "%s", macro_name);

    HASH_ADD_INT(models, id, s);    /* id: name of key field */
}


/* Hash sorting functions */
int hash_model_id_sort(struct mod_lst *a, struct mod_lst *b)
{
    return (a->id > b->id);
}


void hash_sort_by_model_id()
{
    HASH_SORT(models, hash_model_id_sort);
}


/* Delete hash */
void hash_delete_all()
{
    struct mod_lst *current_model, *tmp = NULL;

    HASH_ITER(hh, models, current_model, tmp)
    {
        /* delete it (models advances to next) */
        HASH_DEL(models, current_model);
        free(current_model);            /* free it */
    }
}


#ifdef HAVE_LIBREADLINE
/* Frees allocated memory and sets pointers to NULL before calling readline
 * and then parses the input into space separated tokens.
 */
static void rp_getline(const char *s)
{
    int i;

    /* free allocated memory and set pointers to NULL */
    if (input_line)
    {
        free(input_line);
        input_line = (char *)NULL;
    }

    if (result)
    {
        result = (char *)NULL;
    }

    /* cmd, arg1, arg2, arg3, arg4, arg5, arg6
     * arg5 and arg 6 are currently unused.
     */
    for (i = 0; i < 7; i++)
    {
        parsed_input[i] = NULL;
    }

    /* Action!  Returns typed line with newline stripped. */
    input_line = readline(s);
}
#endif


/*
 * TODO: use Lex?
 */
char parse_arg(const char *arg)
{
    int i;

    for (i = 0; test_list[i].cmd != 0; i++)
    {
        if (!strncmp(arg, test_list[i].name, MAXNAMSIZ))
        {
            return test_list[i].cmd;
        }
    }

    return 0;
}


/*
 * This scanf works even in presence of signals (timer, SIGIO, ..)
 */
static int scanfc(FILE *fin, const char *format, void *p)
{
    do
    {
        int ret;

        ret = fscanf(fin, format, p);

        if (ret < 0)
        {
            if (errno == EINTR)
            {
                continue;
            }

            rig_debug(RIG_DEBUG_ERR, "fscanf: %s\n", strerror(errno));
            rig_debug(RIG_DEBUG_ERR, "fscanf: parsing '%s' with '%s'\n", (char *)p, format);
        }

        return ret;
    }
    while (1);
}


/*
 * function to get the next word from the command line or from stdin
 * until stdin exhausted. stdin is read if the special token '-' is
 * found on the command line.
 *
 * returns EOF when words exhausted
 * returns <0 is error number
 * returns >=0 when successful
 */
static int next_word(char *buffer, int argc, char *argv[], int newline)
{
    int ret;
    char c;
    static int reading_stdin;

    if (!reading_stdin)
    {
        if (optind >= argc)
        {
            return EOF;
        }
        else if (newline
                 && '-' == argv[optind][0]
                 && 1 == strlen(argv[optind]))
        {

            ++optind;
            reading_stdin = 1;
        }
    }

    if (reading_stdin)
    {
        do
        {
            do
            {
                ret = scanf(" %c%" STR(MAXARGSZ) "[^ \t\n#]", &c, &buffer[1]);
            }
            while (EINTR == ret);

            if (ret > 0 && '#' == c)
            {
                do
                {
                    ret = scanf("%*[^\n]");
                }
                while (EINTR == ret);   /* consume comments */

                ret = 0;
            }
        }
        while (!ret);

        if (EOF == ret)
        {
            reading_stdin = 0;
        }
        else if (ret < 0)
        {
            rig_debug(RIG_DEBUG_ERR, "scanf: %s\n", strerror(errno));
            reading_stdin = 0;
        }
        else
        {
            buffer[0] = c;
            buffer[1 == ret ? 1 : MAXARGSZ] = '\0';

            if (newline)
            {
                putchar('\n');
            }

            fputs(buffer, stdout);
            putchar(' ');
        }
    }

    if (!reading_stdin)
    {
        if (optind < argc)
        {
            strncpy(buffer, argv[optind++], MAXARGSZ);
            buffer[MAXARGSZ] = '\0';
            ret = 1;
        }
        else
        {
            ret = EOF;
        }
    }

    return ret;
}


#define fprintf_flush(f, a...)                  \
    ({ int __ret;                               \
        __ret = fprintf((f), a);                \
        fflush((f));                            \
        __ret;                                  \
    })


int rotctl_parse(ROT *my_rot, FILE *fin, FILE *fout, char *argv[], int argc,
                 int interactive, int prompt, char send_cmd_term)
{
    int retcode;            /* generic return code from functions */
    unsigned char cmd;
    struct test_table *cmd_entry = NULL;
    int ext_resp = 0;
    char resp_sep = '\n';

    char command[MAXARGSZ + 1];
    char arg1[MAXARGSZ + 1], *p1 = NULL;
    char arg2[MAXARGSZ + 1], *p2 = NULL;
    char arg3[MAXARGSZ + 1], *p3 = NULL;
    char arg4[MAXARGSZ + 1], *p4 = NULL;
#ifdef __USEP5P6__
    char *p5 = NULL;
    char *p6 = NULL;
#endif

    /* cmd, internal, rotctld */
#ifdef HAVE_LIBREADLINE

    if (!(interactive && prompt && have_rl))
#endif
    {
        if (interactive)
        {
            static int last_was_ret = 1;

            if (prompt)
            {
                fprintf_flush(fout, "\nRotator command: ");
            }

            do
            {
                if (scanfc(fin, "%c", &cmd) < 1)
                {
                    return -1;
                }

                /* Extended response protocol requested with leading '+' on command
                 * string--rotctld only!
                 */
                if (cmd == '+' && !prompt)
                {
                    ext_resp = 1;

                    if (scanfc(fin, "%c", &cmd) < 1)
                    {
                        return -1;
                    }
                }
                else if (cmd == '+' && prompt)
                {
                    return 0;
                }

                if (cmd != '\\'
                        && cmd != '_'
                        && cmd != '#'
                        && ispunct(cmd)
                        && !prompt)
                {

                    ext_resp = 1;
                    resp_sep = cmd;

                    if (scanfc(fin, "%c", &cmd) < 1)
                    {
                        return -1;
                    }
                }
                else if (cmd != '\\'
                         && cmd != '?'
                         && cmd != '_'
                         && cmd != '#'
                         && ispunct(cmd)
                         && prompt)
                {
                    return 0;
                }

                /* command by name */
                if (cmd == '\\')
                {
                    unsigned char cmd_name[MAXNAMSIZ], *pcmd = cmd_name;
                    int c_len = MAXNAMSIZ;

                    if (scanfc(fin, "%c", pcmd) < 1)
                    {
                        return -1;
                    }

                    while (c_len-- && (isalnum(*pcmd) || *pcmd == '_'))
                    {
                        if (scanfc(fin, "%c", ++pcmd) < 1)
                        {
                            return -1;
                        }
                    }

                    *pcmd = '\0';
                    cmd = parse_arg((char *) cmd_name);
                    break;
                }

                if (cmd == 0x0a || cmd == 0x0d)
                {
                    if (last_was_ret)
                    {
                        if (prompt)
                        {
                            fprintf_flush(fout, "? for help, q to quit.\n");
                        }

                        return 0;
                    }

                    last_was_ret = 1;
                }
            }
            while (cmd == 0x0a || cmd == 0x0d);

            last_was_ret = 0;

            /* comment line */
            if (cmd == '#')
            {
                while (cmd != '\n' && cmd != '\r')
                {
                    if (scanfc(fin, "%c", &cmd) < 1)
                    {
                        return -1;
                    }
                }

                return 0;
            }

            if (cmd == 'Q' || cmd == 'q')
            {
                return 1;
            }

            if (cmd == '?')
            {
                usage_rot(fout);
                fflush(fout);
                return 0;
            }
        }
        else
        {
            /* parse rest of command line */
            retcode = next_word(command, argc, argv, 1);

            if (EOF == retcode)
            {
                return 1;
            }
            else if (retcode < 0)
            {
                return retcode;
            }
            else if ('\0' == command[1])
            {
                cmd = command[0];
            }
            else
            {
                cmd = parse_arg(command);
            }
        }

        cmd_entry = find_cmd_entry(cmd);

        if (!cmd_entry)
        {
            fprintf_flush(stderr, "Command '%c' not found!\n", cmd);
            return 0;
        }

        if ((cmd_entry->flags & ARG_IN_LINE)
                && (cmd_entry->flags & ARG_IN1)
                && cmd_entry->arg1)
        {

            if (interactive)
            {
                char *nl;

                if (prompt)
                {
                    fprintf_flush(fout, "%s: ", cmd_entry->arg1);
                }

                if (fgets(arg1, MAXARGSZ, fin) == NULL)
                {
                    return -1;
                }

                if (arg1[0] == 0xa)
                {
                    if (fgets(arg1, MAXARGSZ, fin) == NULL)
                    {
                        return -1;
                    }
                }

                nl = strchr(arg1, 0xa);

                if (nl)
                {
                    *nl = '\0';    /* chomp */
                }

                p1 = arg1[0] == ' ' ? arg1 + 1 : arg1;
            }
            else
            {
                retcode = next_word(arg1, argc, argv, 0);

                if (EOF == retcode)
                {
                    fprintf(stderr,
                            "Invalid arg for command '%s'\n",
                            cmd_entry->name);
                    return 1;
                }
                else if (retcode < 0)
                {
                    return retcode;
                }

                p1 = arg1;
            }
        }
        else  if ((cmd_entry->flags & ARG_IN1) && cmd_entry->arg1)
        {
            if (interactive)
            {
                if (prompt)
                {
                    fprintf_flush(fout, "%s: ", cmd_entry->arg1);
                }

                if (scanfc(fin, "%s", arg1) < 1)
                {
                    return -1;
                }

                p1 = arg1;
            }
            else
            {
                retcode = next_word(arg1, argc, argv, 0);

                if (EOF == retcode)
                {
                    fprintf(stderr,
                            "Invalid arg for command '%s'\n",
                            cmd_entry->name);
                    return 1;
                }
                else if (retcode < 0)
                {
                    return retcode;
                }

                p1 = arg1;
            }
        }

        if (p1
                && p1[0] != '?'
                && (cmd_entry->flags & ARG_IN2)
                && cmd_entry->arg2)
        {

            if (interactive)
            {
                if (prompt)
                {
                    fprintf_flush(fout, "%s: ", cmd_entry->arg2);
                }

                if (scanfc(fin, "%s", arg2) < 1)
                {
                    return -1;
                }

                p2 = arg2;
            }
            else
            {
                retcode = next_word(arg2, argc, argv, 0);

                if (EOF == retcode)
                {
                    fprintf(stderr,
                            "Invalid arg for command '%s'\n",
                            cmd_entry->name);
                    return 1;
                }
                else if (retcode < 0)
                {
                    return retcode;
                }

                p2 = arg2;
            }
        }

        if (p1
                && p1[0] != '?'
                && (cmd_entry->flags & ARG_IN3)
                && cmd_entry->arg3)
        {

            if (interactive)
            {
                if (prompt)
                {
                    fprintf_flush(fout, "%s: ", cmd_entry->arg3);
                }

                if (scanfc(fin, "%s", arg3) < 1)
                {
                    return -1;
                }

                p3 = arg3;
            }
            else
            {
                retcode = next_word(arg3, argc, argv, 0);

                if (EOF == retcode)
                {
                    fprintf(stderr,
                            "Invalid arg for command '%s'\n",
                            cmd_entry->name);
                    return 1;
                }
                else if (retcode < 0)
                {
                    return retcode;
                }

                p3 = arg3;
            }
        }

        if (p1
                && p1[0] != '?'
                && (cmd_entry->flags & ARG_IN4)
                && cmd_entry->arg4)
        {

            if (interactive)
            {
                if (prompt)
                {
                    fprintf_flush(fout, "%s: ", cmd_entry->arg4);
                }

                if (scanfc(fin, "%s", arg4) < 1)
                {
                    return -1;
                }

                p4 = arg4;
            }
            else
            {
                retcode = next_word(arg4, argc, argv, 0);

                if (EOF == retcode)
                {
                    fprintf(stderr,
                            "Invalid arg for command '%s'\n",
                            cmd_entry->name);
                    return 1;
                }
                else if (retcode < 0)
                {
                    return retcode;
                }

                p4 = arg4;
            }
        }
    }

#ifdef HAVE_LIBREADLINE

    if (interactive && prompt && have_rl)
    {
        int j, x;

#ifdef HAVE_READLINE_HISTORY
        /* Minimum space for 32+1+128+1+128+1+128+1+128+1+128+1+128+1 = 807
         * chars, so allocate 896 chars cleared to zero for safety.
         */
        rp_hist_buf = (char *)calloc(896, sizeof(char));
#endif

        rp_getline("\nRotator command: ");

        /* EOF (Ctl-D) received on empty input line, bail out gracefully. */
        if (!input_line)
        {
            fprintf_flush(fout, "\n");
            return 1;
        }

        /* Q or q to quit */
        if (!(strncasecmp(input_line, "q", 1)))
        {
            return 1;
        }

        /* '?' for help */
        if (!(strncmp(input_line, "?", 1)))
        {
            usage_rot(fout);
            fflush(fout);
            return 0;
        }

        /* '#' for comment */
        if (!(strncmp(input_line, "#", 1)))
        {
            return 0;
        }

        /* Blank line entered */
        if (!(strcmp(input_line, "")))
        {
            fprintf(fout, "? for help, q to quit.\n");
            fflush(fout);
            return 0;
        }

        rig_debug(RIG_DEBUG_BUG, "%s: input_line: %s\n", __func__, input_line);

        /* Split input_line on any number of spaces to get the command token
         * Tabs are intercepted by readline for completion and a newline
         * causes readline to return the typed text.  If more than one
         * argument is given, it will be parsed out later.
         */
        result = strtok(input_line, " ");

        /* parsed_input stores pointers into input_line where the token strings
         * start.
         */
        if (result)
        {
            parsed_input[0] = result;
        }
        else
        {
            /* Oops!  Invoke GDB!! */
            fprintf_flush(fout, "\n");
            return 1;
        }

        /* At this point parsed_input contains the typed text of the command
         * with surrounding space characters removed.  If Readline History is
         * available, copy the command string into a history buffer.
         */

        /* Single character command */
        if ((strlen(parsed_input[0]) == 1) && (*parsed_input[0] != '\\'))
        {
            cmd = *parsed_input[0];

#ifdef HAVE_READLINE_HISTORY

            /* Store what is typed, not validated, for history. */
            if (rp_hist_buf)
            {
                strncpy(rp_hist_buf, parsed_input[0], 1);
            }

#endif
        }
        /* Test the command token, parsed_input[0] */
        else if ((*parsed_input[0] == '\\') && (strlen(parsed_input[0]) > 1))
        {
            char cmd_name[MAXNAMSIZ];

            /* if there is no terminating '\0' character in the source string,
             * srncpy() doesn't add one even if the supplied length is less
             * than the destination array.  Truncate the source string here.
             */
            if (strlen(parsed_input[0] + 1) >= MAXNAMSIZ)
            {
                *(parsed_input[0] + MAXNAMSIZ) = '\0';
            }

#ifdef HAVE_READLINE_HISTORY

            if (rp_hist_buf)
            {
                strncpy(rp_hist_buf, parsed_input[0], MAXNAMSIZ);
            }

#endif
            /* The starting position of the source string is the first
             * character past the initial '\'.
             */
            SNPRINTF(cmd_name, sizeof(cmd_name), "%s", parsed_input[0] + 1);

            /* Sanity check as valid multiple character commands consist of
             * alphanumeric characters and the underscore ('_') character.
             */
            for (j = 0; cmd_name[j] != '\0'; j++)
            {
                if (!(isalnum((int)cmd_name[j]) || cmd_name[j] == '_'))
                {
                    fprintf(stderr,
                            "Valid multiple character command names contain alphanumeric characters plus '_'\n");
                    return 0;
                }
            }

            cmd = parse_arg(cmd_name);
        }
        /* Single '\' entered, prompt again */
        else if ((*parsed_input[0] == '\\') && (strlen(parsed_input[0]) == 1))
        {
            return 0;
        }
        /* Multiple characters but no leading '\' */
        else
        {
            fprintf(stderr, "Precede multiple character command names with '\\'\n");
            return 0;
        }

        cmd_entry = find_cmd_entry(cmd);

        if (!cmd_entry)
        {
            if (cmd == '\0')
            {
                fprintf(stderr, "Command '%s' not found!\n", parsed_input[0]);
            }
            else
            {
                fprintf(stderr, "Command '%c' not found!\n", cmd);
            }

            return 0;
        }

        /* \send_cmd */
        if ((cmd_entry->flags & ARG_IN_LINE)
                && (cmd_entry->flags & ARG_IN1)
                && cmd_entry->arg1)
        {

            /* Check for a non-existent delimiter so as to not break up
             * remaining line into separate tokens (spaces OK).
             */
            result = strtok(NULL, "\0");

            if (result)
            {
                x = 1;
                parsed_input[x] = result;
            }
            else
            {
                char pmptstr[(strlen(cmd_entry->arg1) + 3)];
                x = 0;

                strcpy(pmptstr, cmd_entry->arg1);
                strcat(pmptstr, ": ");

                rp_getline(pmptstr);

                if (input_line == NULL)
                {
                    fprintf_flush(fout, "\n");
                    return 1;
                }
                /* Blank line entered */
                else if (!(strcmp(input_line, "")))
                {
                    fprintf(fout, "? for help, q to quit.\n");
                    fflush(fout);
                    return 0;
                }
                else
                {
                    parsed_input[x] = input_line;
                }
            }

            /* The arg1 array size is MAXARGSZ + 1 so truncate it to fit if larger. */
            if (strlen(parsed_input[x]) > MAXARGSZ)
            {
                parsed_input[x][MAXARGSZ] = '\0';
            }

#ifdef HAVE_READLINE_HISTORY

            if (rp_hist_buf)
            {
                strncat(rp_hist_buf, " ", 2);
                strncat(rp_hist_buf, parsed_input[x], MAXARGSZ);
            }

#endif
            strcpy(arg1, parsed_input[x]);
            p1 = arg1;
        }

        /* Normal argument parsing. */
        else if ((cmd_entry->flags & ARG_IN1) && cmd_entry->arg1)
        {
            result = strtok(NULL, " ");

            if (result)
            {
                x = 1;
                parsed_input[x] = result;
            }
            else
            {
                char pmptstr[(strlen(cmd_entry->arg1) + 3)];
                x = 0;

                strcpy(pmptstr, cmd_entry->arg1);
                strcat(pmptstr, ": ");

                rp_getline(pmptstr);

                if (!(strcmp(input_line, "")))
                {
                    fprintf(fout, "? for help, q to quit.\n");
                    fflush(fout);
                    return 0;
                }

                result = strtok(input_line, " ");

                if (result)
                {
                    parsed_input[x] = result;
                }
                else
                {
                    fprintf_flush(fout, "\n");
                    return 1;
                }
            }

            if (strlen(parsed_input[x]) > MAXARGSZ)
            {
                parsed_input[x][MAXARGSZ] = '\0';
            }

#ifdef HAVE_READLINE_HISTORY

            if (rp_hist_buf)
            {
                strncat(rp_hist_buf, " ", 2);
                strncat(rp_hist_buf, parsed_input[x], MAXARGSZ);
            }

#endif
            strcpy(arg1, parsed_input[x]);
            p1 = arg1;
        }

        if (p1
                && p1[0] != '?'
                && (cmd_entry->flags & ARG_IN2)
                && cmd_entry->arg2)
        {

            result = strtok(NULL, " ");

            if (result)
            {
                x = 2;
                parsed_input[x] = result;
            }
            else
            {
                char pmptstr[(strlen(cmd_entry->arg2) + 3)];
                x = 0;

                strcpy(pmptstr, cmd_entry->arg2);
                strcat(pmptstr, ": ");

                rp_getline(pmptstr);

                if (!(strcmp(input_line, "")))
                {
                    fprintf(fout, "? for help, q to quit.\n");
                    fflush(fout);
                    return 0;
                }

                result = strtok(input_line, " ");

                if (result)
                {
                    parsed_input[x] = result;
                }
                else
                {
                    fprintf_flush(fout, "\n");
                    return 1;
                }
            }

            if (strlen(parsed_input[x]) > MAXARGSZ)
            {
                parsed_input[x][MAXARGSZ] = '\0';
            }

#ifdef HAVE_READLINE_HISTORY

            if (rp_hist_buf)
            {
                strncat(rp_hist_buf, " ", 2);
                strncat(rp_hist_buf, parsed_input[x], MAXARGSZ);
            }

#endif
            strcpy(arg2, parsed_input[x]);
            p2 = arg2;
        }

        if (p1
                && p1[0] != '?'
                && (cmd_entry->flags & ARG_IN3)
                && cmd_entry->arg3)
        {

            result = strtok(NULL, " ");

            if (result)
            {
                x = 3;
                parsed_input[x] = result;
            }
            else
            {
                char pmptstr[(strlen(cmd_entry->arg3) + 3)];
                x = 0;

                strcpy(pmptstr, cmd_entry->arg3);
                strcat(pmptstr, ": ");

                rp_getline(pmptstr);

                if (!(strcmp(input_line, "")))
                {
                    fprintf(fout, "? for help, q to quit.\n");
                    fflush(fout);
                    return 0;
                }

                result = strtok(input_line, " ");

                if (result)
                {
                    parsed_input[x] = result;
                }
                else
                {
                    fprintf_flush(fout, "\n");
                    return 1;
                }
            }

            if (strlen(parsed_input[x]) > MAXARGSZ)
            {
                parsed_input[x][MAXARGSZ] = '\0';
            }

#ifdef HAVE_READLINE_HISTORY

            if (rp_hist_buf)
            {
                strncat(rp_hist_buf, " ", 2);
                strncat(rp_hist_buf, parsed_input[x], MAXARGSZ);
            }

#endif
            strcpy(arg3, parsed_input[x]);
            p3 = arg3;
        }

        if (p1
                && p1[0] != '?'
                && (cmd_entry->flags & ARG_IN4)
                && cmd_entry->arg4)
        {

            result = strtok(NULL, " ");

            if (result)
            {
                x = 4;
                parsed_input[x] = result;
            }
            else
            {
                char pmptstr[(strlen(cmd_entry->arg4) + 3)];
                x = 0;

                strcpy(pmptstr, cmd_entry->arg4);
                strcat(pmptstr, ": ");

                rp_getline(pmptstr);

                if (!(strcmp(input_line, "")))
                {
                    fprintf(fout, "? for help, q to quit.\n");
                    fflush(fout);
                    return 0;
                }

                result = strtok(input_line, " ");

                if (result)
                {
                    parsed_input[x] = result;
                }
                else
                {
                    fprintf_flush(fout, "\n");
                    return 1;
                }
            }

            if (strlen(parsed_input[x]) > MAXARGSZ)
            {
                parsed_input[x][MAXARGSZ] = '\0';
            }

#ifdef HAVE_READLINE_HISTORY

            if (rp_hist_buf)
            {
                strncat(rp_hist_buf, " ", 2);
                strncat(rp_hist_buf, parsed_input[x], MAXARGSZ);
            }

#endif
            strcpy(arg4, parsed_input[x]);
            p4 = arg4;
        }

#ifdef HAVE_READLINE_HISTORY

        if (rp_hist_buf)
        {
            add_history(rp_hist_buf);
            free(rp_hist_buf);
            rp_hist_buf = (char *)NULL;
        }

#endif
    }

#endif // HAVE_LIBREADLINE

    /*
     * mutex locking needed because rotctld is multithreaded
     * and hamlib is not MT-safe
     */
#ifdef HAVE_PTHREAD
    pthread_mutex_lock(&rot_mutex);
#endif

    if (!prompt)
    {
        rig_debug(RIG_DEBUG_TRACE,
                  "rotctl(d): %c '%s' '%s' '%s' '%s'\n",
                  cmd,
                  p1 ? p1 : "",
                  p2 ? p2 : "",
                  p3 ? p3 : "",
                  p4 ? p4 : "");
    }

    /*
     * Extended Response protocol: output received command name and arguments
     * response.
     */
    if (interactive && ext_resp && !prompt)
    {
        char a1[MAXARGSZ + 2];
        char a2[MAXARGSZ + 2];
        char a3[MAXARGSZ + 2];
        char a4[MAXARGSZ + 2];

        p1 == NULL ? a1[0] = '\0' : snprintf(a1, sizeof(a1), " %s", p1);
        p2 == NULL ? a2[0] = '\0' : snprintf(a2, sizeof(a2), " %s", p2);
        p3 == NULL ? a3[0] = '\0' : snprintf(a3, sizeof(a3), " %s", p3);
        p4 == NULL ? a4[0] = '\0' : snprintf(a4, sizeof(a4), " %s", p4);

        fprintf(fout, "%s:%s%s%s%s%c", cmd_entry->name, a1, a2, a3, a4, resp_sep);
    }

    retcode = (*cmd_entry->rot_routine)(my_rot,
                                        fout,
                                        interactive,
                                        prompt,
                                        send_cmd_term,
                                        ext_resp,
                                        resp_sep,
                                        cmd_entry,
                                        p1,
                                        p2 ? p2 : "",
                                        p3 ? p3 : "",
                                        p4 ? p4 : "",
#ifdef __USEP5P6__
                                        p5 ? p5 : "",
                                        p6 ? p6 : "");
#else
                                        "",
                                        "");
#endif

#ifdef HAVE_PTHREAD
    pthread_mutex_unlock(&rot_mutex);
#endif

    if (retcode == RIG_EIO) { return retcode; }

    if (retcode != RIG_OK)
    {
        /* only for rotctld */
        if (interactive && !prompt)
        {
            rot_debug(RIG_DEBUG_TRACE, "%s: NETROTCTL_RET %d\n", __func__, retcode);
            fprintf(fout, NETROTCTL_RET "%d\n", retcode);
            // ext_resp = 0; // not used ?
            resp_sep = '\n';
        }
        else
        {
            if (cmd_entry->name != NULL)
            {
                fprintf(fout, "%s: error = %s\n", cmd_entry->name, rigerror(retcode));
            }
        }
    }
    else
    {
        /* only for rotctld */
        if (interactive && !prompt)
        {
            /* netrotctl RIG_OK */
            if (!(cmd_entry->flags & ARG_OUT) && !ext_resp)
            {
                rot_debug(RIG_DEBUG_TRACE, "%s: NETROTCTL_RET 0\n", __func__);
                fprintf(fout, NETROTCTL_RET "0\n");
            }

            /* Extended Response protocol */
            else if (ext_resp && cmd != 0xf0)
            {
                rot_debug(RIG_DEBUG_TRACE, "%s: NETROTCTL_RET 0\n", __func__);
                fprintf(fout, NETROTCTL_RET "0\n");
                resp_sep = '\n';
            }
        }
    }

    fflush(fout);

    return retcode != RIG_OK ? 2 : 0;
}



void version()
{
    printf("rotctl(d), %s\n\n", hamlib_version2);
    printf("%s\n", hamlib_copyright);
}


void usage_rot(FILE *fout)
{
    int i;

    fprintf(fout, "Commands (some may not be available for this rotator):\n");

    for (i = 0; test_list[i].cmd != 0; i++)
    {
        int nbspaces;
        fprintf(fout,
                "%c: %-12s(",
                isprint(test_list[i].cmd) ? test_list[i].cmd : '?',
                test_list[i].name);

        nbspaces = 16;

        if (test_list[i].arg1 && (test_list[i].flags & ARG_IN1))
        {
            nbspaces -= fprintf(fout, "%s", test_list[i].arg1);
        }

        if (test_list[i].arg2 && (test_list[i].flags & ARG_IN2))
        {
            nbspaces -= fprintf(fout, ", %s", test_list[i].arg2);
        }

        if (test_list[i].arg3 && (test_list[i].flags & ARG_IN3))
        {
            nbspaces -= fprintf(fout, ", %s", test_list[i].arg3);
        }

        if (test_list[i].arg4 && (test_list[i].flags & ARG_IN4))
        {
            nbspaces -= fprintf(fout, ", %s", test_list[i].arg4);
        }

        fprintf(fout, ")\n");
    }

    fprintf(fout,
            "\n\nIn interactive mode prefix long command names with '\\', e.g. '\\dump_state'\n\n"
            "The special command '-' is used to read further commands from standard input\n"
            "Commands and arguments read from standard input must be white space separated,\n"
            "comments are allowed, comments start with the # character and continue to the\n"
            "end of the line.\n");
}


int print_conf_list(const struct confparams *cfp, rig_ptr_t data)
{
    ROT *rot = (ROT *) data;
    int i;
    char buf[128] = "";

    rot_get_conf2(rot, cfp->token, buf, sizeof(buf));
    printf("%s: \"%s\"\n" "\tDefault: %s, Value: %s\n",
           cfp->name,
           cfp->tooltip,
           cfp->dflt,
           buf);

    switch (cfp->type)
    {
    case RIG_CONF_NUMERIC:
        printf("\tRange: %.1f..%.1f, step %.1f\n",
               cfp->u.n.min,
               cfp->u.n.max,
               cfp->u.n.step);
        break;

    case RIG_CONF_CHECKBUTTON:
        printf("\tCheckbox: 0,1\n");
        break;

    case RIG_CONF_COMBO:
        if (!cfp->u.c.combostr[0])
        {
            break;
        }

        printf("\tCombo: %s", cfp->u.c.combostr[0]);

        for (i = 1 ; i < RIG_COMBO_MAX && cfp->u.c.combostr[i]; i++)
        {
            printf(", %s", cfp->u.c.combostr[i]);
        }

        printf("\n");
        break;

    default:
        break;
    }

    return 1;  /* != 0, we want them all ! */
}

static int hash_model_list(const struct rot_caps *caps, void *data)
{

    hash_add_model(caps->rot_model,
                   caps->mfg_name,
                   caps->model_name,
                   caps->version,
                   rig_strstatus(caps->status),
                   caps->macro_name);

    return 1;  /* !=0, we want them all ! */
}

void print_model_list()
{
    struct mod_lst *s;

    for (s = models; s != NULL; s = (struct mod_lst *)(s->hh.next))
    {
        printf("%6d  %-23s%-24s%-16s%-14s%s\n",
               s->id,
               s->mfg_name,
               s->model_name,
               s->version,
               s->status,
               s->macro_name);
    }
}


void list_models()
{
    int status;

    rot_load_all_backends();

    printf(" Rot #  Mfg                    Model                   Version         Status        Macro\n");
    status = rot_list_foreach(hash_model_list, NULL);

    if (status != RIG_OK)
    {
        printf("rot_list_foreach: error = %s \n", rigerror(status));
        exit(2);
    }

    hash_sort_by_model_id();
    print_model_list();
    hash_delete_all();
}


int set_conf(ROT *my_rot, char *conf_parms)
{
    char *p;

    rot_debug(RIG_DEBUG_TRACE, "%s: called\n", __func__);
    p = conf_parms;

    while (p && *p != '\0')
    {
        int ret;
        char *q, *n = NULL;
        /* FIXME: left hand value of = cannot be null */
        q = strchr(p, '=');

        if (!q)
        {
            return RIG_EINVAL;
        }

        *q++ = '\0';
        n = strchr(q, ',');

        if (n)
        {
            *n++ = '\0';
        }

        rig_debug(RIG_DEBUG_TRACE, "%s: token=%s, val=%s\n", __func__, p, q);
        ret = rot_set_conf(my_rot, rot_token_lookup(my_rot, p), q);

        if (ret != RIG_OK)
        {
            return ret;
        }

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
    char *comma_pos;

    /* Fixing args with an invalid decimal separator. */
    comma_pos = strchr(arg1, ',');

    if (comma_pos)
    {
        *comma_pos = '.';
    }

    comma_pos = strchr(arg2, ',');

    if (comma_pos)
    {
        *comma_pos = '.';
    }

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
    {
        return status;
    }

    if ((interactive && prompt) || (interactive && !prompt && ext_resp))
    {
        fprintf(fout, "%s: ", cmd->arg1);
    }

    fprintf(fout, "%.2f%c", az, resp_sep);

    if ((interactive && prompt) || (interactive && !prompt && ext_resp))
    {
        fprintf(fout, "%s: ", cmd->arg2);
    }

    fprintf(fout, "%.2f%c", el, resp_sep);

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
    {
        fprintf(fout, "%s: ", cmd->arg1);
    }

    fprintf(fout, "%s%c", s ? s : "None", resp_sep);

    return RIG_OK;
}


/* 's' */
declare_proto_rot(get_status)
{
    int result;
    rot_status_t status;
    char s[SPRINTF_MAX_SIZE];

    result = rot_get_status(rot, &status);

    if (result != RIG_OK)
    {
        return result;
    }

    if ((interactive && prompt) || (interactive && !prompt && ext_resp))
    {
        fprintf(fout, "%s: ", cmd->arg1);
    }

    rot_sprintf_status(s, sizeof(s), status);
    fprintf(fout, "%s%c", s, resp_sep);

    return RIG_OK;
}


/* 'M' */
declare_proto_rot(move)
{
    int direction;
    int speed;

    if (!strcmp(arg1, "LEFT") || !strcmp(arg1, "CCW"))
    {
        direction = ROT_MOVE_LEFT;
    }
    else if (!strcmp(arg1, "RIGHT") || !strcmp(arg1, "CW"))
    {
        direction = ROT_MOVE_RIGHT;
    }
    else if (!strcmp(arg1, "UP"))
    {
        direction = ROT_MOVE_UP;
    }
    else if (!strcmp(arg1, "DOWN"))
    {
        direction = ROT_MOVE_DOWN;
    }
    else
    {
        CHKSCN1ARG(sscanf(arg1, "%d", &direction));
    }

    CHKSCN1ARG(sscanf(arg2, "%d", &speed));
    return rot_move(rot, direction, speed);
}


/*
 * 'V'
 */
declare_proto_rot(set_level)
{
    setting_t level;
    value_t val;

    if (!strcmp(arg1, "?"))
    {
        char s[SPRINTF_MAX_SIZE];
        rot_sprintf_level(s, sizeof(s), rot->state.has_set_level);
        fputs(s, fout);

        if (rot->caps->set_ext_level)
        {
            sprintf_level_ext(s, sizeof(s), rot->caps->extlevels);
            fputs(s, fout);
        }

        fputc('\n', fout);
        return RIG_OK;
    }

    level = rot_parse_level(arg1);

    if (!rot_has_set_level(rot, level))
    {
        const struct confparams *cfp;

        cfp = rot_ext_lookup(rot, arg1);

        if (!cfp)
        {
            return -RIG_ENAVAIL;    /* no such parameter */
        }

        switch (cfp->type)
        {
        case RIG_CONF_BUTTON:
            /* arg is ignored */
            val.i = 0; // avoid passing uninitialized data
            break;

        case RIG_CONF_CHECKBUTTON:
        case RIG_CONF_COMBO:
            CHKSCN1ARG(sscanf(arg2, "%d", &val.i));
            break;

        case RIG_CONF_NUMERIC:
            CHKSCN1ARG(sscanf(arg2, "%f", &val.f));
            break;

        case RIG_CONF_STRING:
            val.cs = arg2;
            break;

        default:
            return -RIG_ECONF;
        }

        return rot_set_ext_level(rot, cfp->token, val);
    }

    if (ROT_LEVEL_IS_FLOAT(level))
    {
        CHKSCN1ARG(sscanf(arg2, "%f", &val.f));
    }
    else
    {
        CHKSCN1ARG(sscanf(arg2, "%d", &val.i));
    }

    return rot_set_level(rot, level, val);
}


/* 'v' */
declare_proto_rot(get_level)
{
    int status;
    setting_t level;
    value_t val;

    if (!strcmp(arg1, "?"))
    {
        char s[SPRINTF_MAX_SIZE];
        rot_sprintf_level(s, sizeof(s), rot->state.has_get_level);
        fputs(s, fout);

        if (rot->caps->get_ext_level)
        {
            sprintf_level_ext(s, sizeof(s), rot->caps->extlevels);
            fputs(s, fout);
        }

        fputc('\n', fout);
        return RIG_OK;
    }

    level = rot_parse_level(arg1);

    if (!rot_has_get_level(rot, level))
    {
        const struct confparams *cfp;

        cfp = rot_ext_lookup(rot, arg1);

        if (!cfp)
        {
            return -RIG_EINVAL;    /* no such parameter */
        }

        status = rot_get_ext_level(rot, cfp->token, &val);

        if (status != RIG_OK)
        {
            return status;
        }

        if (interactive && prompt)
        {
            fprintf(fout, "%s: ", cmd->arg2);
        }

        switch (cfp->type)
        {
        case RIG_CONF_BUTTON:
            /* there's no sense in retrieving value of stateless button */
            return -RIG_EINVAL;

        case RIG_CONF_CHECKBUTTON:
        case RIG_CONF_COMBO:
            fprintf(fout, "%d\n", val.i);
            break;

        case RIG_CONF_NUMERIC:
            fprintf(fout, "%f\n", val.f);
            break;

        case RIG_CONF_STRING:
            fprintf(fout, "%s\n", val.s);
            break;

        default:
            return -RIG_ECONF;
        }

        return status;
    }

    status = rot_get_level(rot, level, &val);

    if (status != RIG_OK)
    {
        return status;
    }

    if (interactive && prompt)
    {
        fprintf(fout, "%s: ", cmd->arg2);
    }

    if (ROT_LEVEL_IS_FLOAT(level))
    {
        fprintf(fout, "%f\n", val.f);
    }
    else
    {
        fprintf(fout, "%d\n", val.i);
    }

    return status;
}


/* 'U' */
declare_proto_rot(set_func)
{
    setting_t func;
    int func_stat;

    if (!strcmp(arg1, "?"))
    {
        char s[SPRINTF_MAX_SIZE];
        rot_sprintf_func(s, sizeof(s), rot->state.has_set_func);
        fprintf(fout, "%s\n", s);
        return RIG_OK;
    }

    func = rot_parse_func(arg1);

    if (!rot_has_set_func(rot, func))
    {
        const struct confparams *cfp;

        cfp = rot_ext_lookup(rot, arg1);

        if (!cfp)
        {
            return -RIG_ENAVAIL;    /* no such parameter */
        }

        CHKSCN1ARG(sscanf(arg2, "%d", &func_stat));

        return rot_set_ext_func(rot, cfp->token, func_stat);
    }

    CHKSCN1ARG(sscanf(arg2, "%d", &func_stat));
    return rot_set_func(rot, func, func_stat);
}


/* 'u' */
declare_proto_rot(get_func)
{
    int status;
    setting_t func;
    int func_stat;

    if (!strcmp(arg1, "?"))
    {
        char s[SPRINTF_MAX_SIZE];
        rot_sprintf_func(s, sizeof(s), rot->state.has_get_func);
        fprintf(fout, "%s\n", s);
        return RIG_OK;
    }

    func = rot_parse_func(arg1);

    if (!rot_has_get_func(rot, func))
    {
        const struct confparams *cfp;

        cfp = rot_ext_lookup(rot, arg1);

        if (!cfp)
        {
            return -RIG_EINVAL;    /* no such parameter */
        }

        status = rot_get_ext_func(rot, cfp->token, &func_stat);

        if (status != RIG_OK)
        {
            return status;
        }

        if (interactive && prompt)
        {
            fprintf(fout, "%s: ", cmd->arg2);
        }

        fprintf(fout, "%d\n", func_stat);

        return status;
    }

    status = rot_get_func(rot, func, &func_stat);

    if (status != RIG_OK)
    {
        return status;
    }

    if (interactive && prompt)
    {
        fprintf(fout, "%s: ", cmd->arg2);
    }

    fprintf(fout, "%d\n", func_stat);

    return status;
}


/* 'R' */
declare_proto_rot(set_parm)
{
    setting_t parm;
    value_t val;

    if (!strcmp(arg1, "?"))
    {
        char s[SPRINTF_MAX_SIZE];
        rot_sprintf_parm(s, sizeof(s), rot->state.has_set_parm);
        fprintf(fout, "%s\n", s);
        return RIG_OK;
    }

    parm = rot_parse_parm(arg1);

    if (!rot_has_set_parm(rot, parm))
    {
        const struct confparams *cfp;

        cfp = rot_ext_lookup(rot, arg1);

        if (!cfp)
        {
            return -RIG_EINVAL;    /* no such parameter */
        }

        switch (cfp->type)
        {
        case RIG_CONF_BUTTON:
            /* arg is ignored */
            val.i = 0; // avoid passing uninitialized data
            break;

        case RIG_CONF_CHECKBUTTON:
        case RIG_CONF_COMBO:
            CHKSCN1ARG(sscanf(arg2, "%d", &val.i));
            break;

        case RIG_CONF_NUMERIC:
            CHKSCN1ARG(sscanf(arg2, "%f", &val.f));
            break;

        case RIG_CONF_STRING:
            val.cs = arg2;
            break;

        case RIG_CONF_BINARY:
            val.b.d = (unsigned char *)arg2;
            break;

        default:
            return -RIG_ECONF;
        }

        return rot_set_ext_parm(rot, cfp->token, val);
    }

    if (ROT_PARM_IS_FLOAT(parm))
    {
        CHKSCN1ARG(sscanf(arg2, "%f", &val.f));
    }
    else
    {
        CHKSCN1ARG(sscanf(arg2, "%d", &val.i));
    }

    return rot_set_parm(rot, parm, val);
}


/* 'r' */
declare_proto_rot(get_parm)
{
    int status;
    setting_t parm;
    value_t val;
    char buffer[RIG_BIN_MAX];

    if (!strcmp(arg1, "?"))
    {
        char s[SPRINTF_MAX_SIZE];
        rot_sprintf_parm(s, sizeof(s), rot->state.has_get_parm);
        fprintf(fout, "%s\n", s);
        return RIG_OK;
    }

    parm = rot_parse_parm(arg1);

    if (!rot_has_get_parm(rot, parm))
    {
        const struct confparams *cfp;

        cfp = rot_ext_lookup(rot, arg1);

        if (!cfp)
        {
            return -RIG_EINVAL;    /* no such parameter */
        }

        switch (cfp->type)
        {
        case RIG_CONF_STRING:
            memset(buffer, '0', sizeof(buffer));
            buffer[sizeof(buffer) - 1] = 0;
            val.s = buffer;
            break;

        case RIG_CONF_BINARY:
            memset(buffer, 0, sizeof(buffer));
            val.b.d = (unsigned char *)buffer;
            val.b.l = RIG_BIN_MAX;
            break;

        default:
            break;
        }

        status = rot_get_ext_parm(rot, cfp->token, &val);

        if (status != RIG_OK)
        {
            return status;
        }

        if (interactive && prompt)
        {
            fprintf(fout, "%s: ", cmd->arg2);
        }

        switch (cfp->type)
        {
        case RIG_CONF_BUTTON:
            /* there's not sense in retrieving value of stateless button */
            return -RIG_EINVAL;

        case RIG_CONF_CHECKBUTTON:
        case RIG_CONF_COMBO:
            fprintf(fout, "%d\n", val.i);
            break;

        case RIG_CONF_NUMERIC:
            fprintf(fout, "%f\n", val.f);
            break;

        case RIG_CONF_STRING:
            fprintf(fout, "%s\n", val.s);
            break;

        case RIG_CONF_BINARY:
            dump_hex((unsigned char *)buffer, val.b.l);
            break;

        default:
            return -RIG_ECONF;
        }

        return status;
    }

    status = rot_get_parm(rot, parm, &val);

    if (status != RIG_OK)
    {
        return status;
    }

    if (interactive && prompt)
    {
        fprintf(fout, "%s: ", cmd->arg2);
    }

    if (ROT_PARM_IS_FLOAT(parm))
    {
        fprintf(fout, "%f\n", val.f);
    }
    else
    {
        fprintf(fout, "%d\n", val.i);
    }

    return status;
}


/* 'C' */
declare_proto_rot(inter_set_conf)
{
    char buf[256];

    rot_debug(RIG_DEBUG_TRACE, "%s: called\n", __func__);

    if (!arg2 || arg2[0] == '\0')
    {
        rot_debug(RIG_DEBUG_ERR, "%s: arg1=='%s', arg2=='%s'\n", __func__, arg1, arg2);

        return -RIG_EINVAL;
    }

    SNPRINTF(buf, sizeof(buf), "%s=%s", arg1, arg2);
    return set_conf(rot, buf);
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
    char *tag;

    /*
     * - Protocol version
     */
#define ROTCTLD_PROT_VER 1

    if ((interactive && prompt) || (interactive && !prompt && ext_resp))
    {
        fprintf(fout, "rotctld Protocol Ver: ");
    }

    fprintf(fout, "%d%c", ROTCTLD_PROT_VER, resp_sep);

    if ((interactive && prompt) || (interactive && !prompt && ext_resp))
    {
        fprintf(fout, "Rotor Model: ");
    }

    fprintf(fout, "%d%c", rot->caps->rot_model, resp_sep);

    tag = "min_az=";

    if ((interactive && prompt) || (interactive && !prompt && ext_resp))
    {
        tag = "Minimum Azimuth: ";
    }

    fprintf(fout, "%s%lf%c", tag, rs->min_az + rot->state.az_offset, resp_sep);

    tag = "max_az=";

    if ((interactive && prompt) || (interactive && !prompt && ext_resp))
    {
        tag = "Maximum Azimuth: ";
    }

    fprintf(fout, "%s%lf%c", tag, rs->max_az + rot->state.az_offset, resp_sep);

    tag = "min_el=";

    if ((interactive && prompt) || (interactive && !prompt && ext_resp))
    {
        tag = "Minimum Elevation: ";
    }

    fprintf(fout, "%s%lf%c", tag, rs->min_el + rot->state.el_offset, resp_sep);

    tag = "max_el=";

    if ((interactive && prompt) || (interactive && !prompt && ext_resp))
    {
        tag = "Maximum Elevation: ";
    }

    fprintf(fout, "%s%lf%c", tag, rs->max_el + rot->state.el_offset, resp_sep);

    tag = "south_zero=";

    if ((interactive && prompt) || (interactive && !prompt && ext_resp))
    {
        tag = "South Zero: ";
    }

    fprintf(fout, "%s%d%c", tag, rs->south_zero, resp_sep);

    char *rtype = "Unknown";

    switch (rot->caps->rot_type)
    {
    case ROT_TYPE_OTHER: rtype = "Other"; break;

    case ROT_TYPE_AZIMUTH   : rtype = "Az"; break;

    case ROT_TYPE_ELEVATION   : rtype = "El"; break;

    case ROT_TYPE_AZEL   : rtype = "AzEl"; break;
    }

    fprintf(fout, "rot_type=%s%c", rtype, resp_sep);

    fprintf(fout, "done%c", resp_sep);

    return RIG_OK;
}


/*
 * Special debugging purpose send command display reply until there's a
 * timeout.
 *
 * 'w'
 */
declare_proto_rot(send_cmd)
{
    int retval;
    struct rot_state *rs;
    int backend_num, cmd_len;
#define BUFSZ 128
    unsigned char bufcmd[BUFSZ];
    unsigned char buf[BUFSZ];
    char eom_buf[4] = { 0xa, 0xd, 0, 0 };

    /*
     * binary protocols enter values as \0xZZ\0xYY..
     *
     * Rem: no binary protocol for rotator as of now
     */
    backend_num = ROT_BACKEND_NUM(rot->caps->rot_model);

    if (send_cmd_term == -1 || backend_num == -1)
    {
        const char *p = arg1, *pp = NULL;
        int i;

        for (i = 0; i < BUFSZ - 1 && p != pp; i++)
        {
            pp = p + 1;
            bufcmd[i] = strtol(p + 1, (char **) &p, 0);
        }

        /* must save length to allow 0x00 to be sent as part of a command */
        cmd_len = i - 1;

        /* no End Of Message chars */
        eom_buf[0] = '\0';
    }
    else
    {
        /* text protocol */
        strncpy((char *) bufcmd, arg1, BUFSZ);
        bufcmd[BUFSZ - 2] = '\0';

        cmd_len = strlen((char *) bufcmd);

        /* Automatic termination char */
        if (send_cmd_term != 0)
        {
            bufcmd[cmd_len++] = send_cmd_term;
        }

        eom_buf[2] = send_cmd_term;
    }

    rs = &rot->state;

    rig_flush(&rs->rotport);

    retval = write_block(&rs->rotport, bufcmd, cmd_len);

    if (retval != RIG_OK)
    {
        return retval;
    }

    if (interactive && prompt)
    {
        fprintf(fout, "%s: ", cmd->arg2);
    }

    do
    {
        /*
         * assumes CR or LF is end of line char
         * for all ascii protocols
         */
        retval = read_string(&rs->rotport, buf, BUFSZ, eom_buf, strlen(eom_buf), 0, 1);

        if (retval < 0)
        {
            break;
        }

        if (retval < BUFSZ)
        {
            buf[retval] = '\0';
        }
        else
        {
            buf[BUFSZ - 1] = '\0';
        }

        fprintf(fout, "%s\n", buf);

    }
    while (retval > 0);

    if (retval > 0 || retval == -RIG_ETIMEOUT)
    {
        retval = RIG_OK;
    }

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
    {
        return err;
    }

    if ((interactive && prompt) || (interactive && !prompt && ext_resp))
    {
        fprintf(fout, "%s: ", cmd->arg4);
    }

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
    {
        return status;
    }

    if ((interactive && prompt) || (interactive && !prompt && ext_resp))
    {
        fprintf(fout, "%s: ", cmd->arg2);
    }

    fprintf(fout, "%f%c", lon, resp_sep);

    if ((interactive && prompt) || (interactive && !prompt && ext_resp))
    {
        fprintf(fout, "%s: ", cmd->arg3);
    }

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
    {
        fprintf(fout, "%s: ", cmd->arg5);
    }

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
    {
        return err;
    }

    if ((interactive && prompt) || (interactive && !prompt && ext_resp))
    {
        fprintf(fout, "%s: ", cmd->arg2);
    }

    fprintf(fout, "%d%c", deg, resp_sep);

    if ((interactive && prompt) || (interactive && !prompt && ext_resp))
    {
        fprintf(fout, "%s: ", cmd->arg3);
    }

    fprintf(fout, "%d%c", min, resp_sep);

    if ((interactive && prompt) || (interactive && !prompt && ext_resp))
    {
        fprintf(fout, "%s: ", cmd->arg4);
    }

    fprintf(fout, "%lf%c", sec, resp_sep);

    if ((interactive && prompt) || (interactive && !prompt && ext_resp))
    {
        fprintf(fout, "%s: ", cmd->arg5);
    }

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

    dec_deg = dmmm2dec(deg, min, sw,
                       0.0); // we'll add real seconds when somebody asks for it

    if ((interactive && prompt) || (interactive && !prompt && ext_resp))
    {
        fprintf(fout, "%s: ", cmd->arg4);
    }

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
    {
        return err;
    }

    if ((interactive && prompt) || (interactive && !prompt && ext_resp))
    {
        fprintf(fout, "%s: ", cmd->arg2);
    }

    fprintf(fout, "%d%c", deg, resp_sep);

    if ((interactive && prompt) || (interactive && !prompt && ext_resp))
    {
        fprintf(fout, "%s: ", cmd->arg3);
    }

    fprintf(fout, "%lf%c", min, resp_sep);

    if ((interactive && prompt) || (interactive && !prompt && ext_resp))
    {
        fprintf(fout, "%s: ", cmd->arg4);
    }

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
    {
        return err;
    }

    if ((interactive && prompt) || (interactive && !prompt && ext_resp))
    {
        fprintf(fout, "%s: ", cmd->arg5);
    }

    fprintf(fout, "%lf%c", dist, resp_sep);

    if ((interactive && prompt) || (interactive && !prompt && ext_resp))
    {
        fprintf(fout, "%s: ", cmd->arg6);
    }

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
    {
        return -RIG_EINVAL;
    }

    if ((interactive && prompt) || (interactive && !prompt && ext_resp))
    {
        fprintf(fout, "%s: ", cmd->arg2);
    }

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
    {
        fprintf(fout, "%s: ", cmd->arg2);
    }

    fprintf(fout, "%lf%c", dist_lp, resp_sep);

    return RIG_OK;
}


/* '0x8c'--pause processing */
declare_proto_rot(pause)
{
    unsigned seconds;
    CHKSCN1ARG(sscanf(arg1, "%u", &seconds));
    sleep(seconds);
    return RIG_OK;
}
