/*
 * ampctl_parse.c - (C) Stephane Fillod 2000-2010
 *                  (C) Nate Bargmann 2003,2007,2010,2011,2012,2013
 *                  (C) The Hamlib Group 2002,2006,2011
 * Derived from rotctl_parse.c by Michael Black 2019
 *
 * This program test/control an amplifier using Hamlib.
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

#include <hamlib/amplifier.h>
#include "serial.h"
#include "misc.h"
#include "sprintflst.h"

#include "ampctl_parse.h"

/* Hash table implementation See:  http://uthash.sourceforge.net/ */
#include "uthash.h"

#ifdef HAVE_PTHREAD
#  include <pthread.h>

static pthread_mutex_t amp_mutex = PTHREAD_MUTEX_INITIALIZER;
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

#ifdef HAVE_READLINE_HISTORY
static char *rp_hist_buf = (char *)NULL;
#endif

#else
static const int have_rl = 0;
#endif

struct test_table
{
    unsigned char cmd;
    const char *name;
    int (*amp_routine)(AMP *,
                       FILE *,
                       int,
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

#define ACTION(f) ampctl_##f
#define declare_proto_amp(f) static int (ACTION(f))(AMP *amp,           \
                                                    FILE *fout,         \
                                                    int interactive,    \
                                                    const struct test_table *cmd, \
                                                    const char *arg1,   \
                                                    const char *arg2,   \
                                                    const char *arg3,   \
                                                    const char *arg4,   \
                                                    const char *arg5,   \
                                                    const char *arg6)

declare_proto_amp(set_freq);
declare_proto_amp(get_freq);
declare_proto_amp(send_cmd);
declare_proto_amp(dump_state);
declare_proto_amp(dump_caps);
declare_proto_amp(get_info);
declare_proto_amp(reset);
declare_proto_amp(set_level);
declare_proto_amp(get_level);
declare_proto_amp(set_powerstat);
declare_proto_amp(get_powerstat);
//declare_proto_amp(dump_caps);

/*
 * convention: upper case cmd is set, lowercase is get
 *
 * NB: 'q' 'Q' '?' are reserved by interactive mode interface
 */
struct test_table test_list[] =
{
    { 'F', "set_freq",      ACTION(set_freq),       ARG_IN, "Frequency(Hz)" },
    { 'f', "get_freq",      ACTION(get_freq),       ARG_OUT, "Frequency(Hz)" },
    { 'l', "get_level",     ACTION(get_level),      ARG_IN1 | ARG_OUT2, "Level", "Level Value" },
    { 'L', "set_level",     ACTION(set_level),      ARG_IN, "Level", "Level Value" },
    { 'w', "send_cmd",      ACTION(send_cmd),       ARG_IN1 | ARG_IN_LINE | ARG_OUT2, "Cmd", "Reply" },
    { 0x8f, "dump_state",   ACTION(dump_state),     ARG_OUT },
    { '1', "dump_caps",     ACTION(dump_caps), },
    { '_', "get_info",      ACTION(get_info),       ARG_OUT, "Info" },
    { 'R', "reset",         ACTION(reset),          ARG_IN, "Reset" },
    { 0x87, "set_powerstat",    ACTION(set_powerstat),  ARG_IN, "Power Status" },
    { 0x88, "get_powerstat",    ACTION(get_powerstat),  ARG_OUT, "Power Status" },
    { 0x00, "", NULL },
};


struct test_table *find_cmd_entry(int cmd)
{
    int i;

    for (i = 0; test_list[i].cmd != 0; i++)
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
    unsigned int id;        /* caps->amp_model This is the hash key */
    char mfg_name[32];      /* caps->mfg_name */
    char model_name[32];    /* caps->model_name */
    char version[32];       /* caps->version */
    char status[32];        /* caps->status */
    char macro_name[32];    /* caps->macro_name */
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
    struct mod_lst *current_model, *tmp;

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
        int ret = fscanf(fin, format, p);

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


extern int interactive;
extern int prompt;
extern char send_cmd_term;
int ext_resp = 0;
unsigned char resp_sep = '\n';      /* Default response separator */


int ampctl_parse(AMP *my_amp, FILE *fin, FILE *fout, char *argv[], int argc)
{
    int retcode;            /* generic return code from functions */
    unsigned char cmd;
    struct test_table *cmd_entry;

    char command[MAXARGSZ + 1];
    char arg1[MAXARGSZ + 1], *p1 = NULL;
    char arg2[MAXARGSZ + 1], *p2 = NULL;
    char arg3[MAXARGSZ + 1], *p3 = NULL;
    char arg4[MAXARGSZ + 1], *p4 = NULL;
#ifdef __USEP5P6__ // to avoid cppcheck warning
    char *p5 = NULL;
    char *p6 = NULL;
#endif

    /* cmd, internal, ampctld */
    if (!(interactive && prompt && have_rl))
    {
        if (interactive)
        {
            static int last_was_ret = 1;

            if (prompt)
            {
                fprintf_flush(fout, "\nAmplifier command: ");
            }

            do
            {
                if (scanfc(fin, "%c", &cmd) < 1)
                {
                    return -1;
                }

                /* Extended response protocol requested with leading '+' on command
                 * string--ampctld only!
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
                usage_amp(fout);
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

        rp_getline("\nAmplifier command: ");

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
            usage_amp(fout);
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

                /* Blank line entered */
                if (input_line && !(strcmp(input_line, "")))
                {
                    fprintf(fout, "? for help, q to quit.\n");
                    fflush(fout);
                    return 0;
                }

                if (input_line)
                {
                    parsed_input[x] = input_line;
                }
                else
                {
                    fprintf_flush(fout, "\n");
                    return 1;
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
     * mutex locking needed because ampctld is multithreaded
     * and hamlib is not MT-safe
     */
#ifdef HAVE_PTHREAD
    pthread_mutex_lock(&amp_mutex);
#endif

    if (!prompt)
    {
        rig_debug(RIG_DEBUG_TRACE,
                  "ampctl(d): %c '%s' '%s' '%s' '%s'\n",
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

    retcode = (*cmd_entry->amp_routine)(my_amp,
                                        fout,
                                        interactive,
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
    pthread_mutex_unlock(&amp_mutex);
#endif

    if (retcode == RIG_EIO) { return retcode; }

    if (retcode != RIG_OK)
    {
        /* only for ampctld */
        if (interactive && !prompt)
        {
            fprintf(fout, NETAMPCTL_RET "%d\n", retcode);
            ext_resp = 0;
            resp_sep = '\n';
        }
        else
        {
            fprintf(fout, "%s: error = %s\n", cmd_entry->name, rigerror(retcode));
        }
    }
    else
    {
        /* only for ampctld */
        if (interactive && !prompt)
        {
            /* netampctl RIG_OK */
            if (!(cmd_entry->flags & ARG_OUT) && !ext_resp)
            {
                fprintf(fout, NETAMPCTL_RET "0\n");
            }

            /* Extended Response protocol */
            else if (ext_resp && cmd != 0xf0)
            {
                fprintf(fout, NETAMPCTL_RET "0\n");
                ext_resp = 0;
                resp_sep = '\n';
            }
        }
    }

    fflush(fout);

    return retcode != RIG_OK ? 2 : 0;
}



void version()
{
    printf("ampctl(d), %s\n\n", hamlib_version2);
    printf("%s\n", hamlib_copyright);
}


void usage_amp(FILE *fout)
{
    int i;

    fprintf(fout, "Commands (some may not be available for this amplifier):\n");

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
        rig_debug(RIG_DEBUG_VERBOSE, "%s: nbspace left=%d\n", __func__, nbspaces);

        fprintf(fout, ")\n");
    }

    fprintf(fout,
            "\n\nIn interactive mode prefix long command names with '\\', e.g. '\\dump_state'\n\n"
            "The special command '-' is used to read further commands from standard input\n"
            "Commands and arguments read from standard input must be white space separated,\n"
            "comments are allowed, comments start with the # character and continue to the\n"
            "end of the line.\n");
}


#if 0
int print_conf_list(const struct confparams *cfp, rig_ptr_t data)
{
    AMP *amp = (AMP *) data;
    int i;
    char buf[128] = "";

    amp_get_conf(amp, cfp->token, buf);
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
#endif

static int hash_model_list(const struct amp_caps *caps, void *data)
{

    hash_add_model(caps->amp_model,
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
        printf("%6u  %-23s%-24s%-16s%-14s%s\n",
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

    amp_load_all_backends();

    printf(" Amp #  Mfg                    Model                   Version         Status        Macro\n");
    status = amp_list_foreach(hash_model_list, NULL);

    if (status != RIG_OK)
    {
        printf("amp_list_foreach: error = %s \n", rigerror(status));
        exit(2);
    }

    hash_sort_by_model_id();
    print_model_list();
    hash_delete_all();
}

int set_conf(AMP *my_amp, char *conf_parms)
{
    char *p, *n;

    p = conf_parms;

    while (p && *p != '\0')
    {
        char *q;
        int ret;
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

        ret = amp_set_conf(my_amp, amp_token_lookup(my_amp, p), q);

        if (ret != RIG_OK)
        {
            return ret;
        }

        p = n;
    }

    return RIG_OK;
}

/*
 * static int (f)(AMP *amp, int interactive, const void *arg1, const void *arg2, const void *arg3, const void *arg4)
 */


/* 'f' */
declare_proto_amp(get_freq)
{
    int status;
    freq_t freq;
    // cppcheck-suppress *
    char *fmt = "%"PRIll"%c";

    status = amp_get_freq(amp, &freq);

    if (status != RIG_OK)
    {
        return status;
    }

    if ((interactive && prompt) || (interactive && !prompt && ext_resp))
    {
        fprintf(fout, "%s: ", cmd->arg1);    /* i.e. "Frequency" */
    }

    fprintf(fout, fmt, (int64_t)freq, resp_sep);

    return status;
}

/* 'F' */
declare_proto_amp(set_freq)
{
    freq_t freq;

    CHKSCN1ARG(sscanf(arg1, "%"SCNfreq, &freq));
    return amp_set_freq(amp, freq);
}


/*
 * RIG_CONF_ extparm's type:
 *   NUMERIC: val.f
 *   COMBO: val.i, starting from 0
 *   STRING: val.s
 *   CHECKBUTTON: val.i 0/1
 *
 * 'L'
 */
declare_proto_amp(set_level)
{
    setting_t level;
    value_t val;

    if (!strcmp(arg1, "?"))
    {
        char s[SPRINTF_MAX_SIZE];
        rig_sprintf_level(s, sizeof(s), amp->state.has_set_level);
        fputs(s, fout);

        if (amp->caps->set_ext_level)
        {
            sprintf_level_ext(s, sizeof(s), amp->caps->extlevels);
            fputs(s, fout);
        }

        fputc('\n', fout);
        return (RIG_OK);
    }

    level = rig_parse_level(arg1);

    // some Java apps send comma in international setups so substitute period
    char *p = strchr(arg2, ',');

    if (p) { *p = '.'; }

    if (!amp_has_set_level(amp, level))
    {
        const struct confparams *cfp;

        cfp = amp_ext_lookup(amp, arg1);

        if (!cfp)
        {
            return (-RIG_ENAVAIL);   /* no such parameter */
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
            return (-RIG_ECONF);
        }

        return (amp_set_ext_level(amp, cfp->token, val));
    }

    if (RIG_LEVEL_IS_FLOAT(level))
    {
        CHKSCN1ARG(sscanf(arg2, "%f", &val.f));
    }
    else
    {
        CHKSCN1ARG(sscanf(arg2, "%d", &val.i));
    }

    return (amp_set_level(amp, level, val));
}

/* 'l' */
declare_proto_amp(get_level)
{
    int status;
    setting_t level;
    value_t val;

    if (!strcmp(arg1, "?"))
    {
        char s[SPRINTF_MAX_SIZE];
        amp_sprintf_level(s, sizeof(s), amp->state.has_get_level);

        fputs(s, fout);

        if (amp->caps->get_ext_level)
        {
            sprintf_level_ext(s, sizeof(s), amp->caps->extlevels);
            fputs(s, fout);
        }

        fputc('\n', fout);
        return RIG_OK;
    }

    level = amp_parse_level(arg1);

    if (!amp_has_get_level(amp, level))
    {
        const struct confparams *cfp;

        cfp = amp_ext_lookup(amp, arg1);

        if (!cfp)
        {
            return -RIG_EINVAL;    /* no such parameter */
        }

        status = amp_get_ext_level(amp, cfp->token, &val);

        if (status != RIG_OK)
        {
            return status;
        }

        if (interactive && prompt)
        {
            fprintf(fout, "%s: ", cmd->arg2);
        }

        printf("cfp->type=%d\n", cfp->type);

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

    status = amp_get_level(amp, level, &val);

    if (status != RIG_OK)
    {
        return status;
    }

    if (interactive && prompt)
    {
        fprintf(fout, "%s: ", cmd->arg2);
    }

    if (AMP_LEVEL_IS_FLOAT(level))
    {
        fprintf(fout, "%f\n", val.f);
    }
    else if (AMP_LEVEL_IS_STRING(level))
    {
        fprintf(fout, "%s\n", val.s);
    }
    else
    {
        fprintf(fout, "%d\n", val.i);
    }

    return status;
}

/* 'R' */
declare_proto_amp(reset)
{
    amp_reset_t reset;

    CHKSCN1ARG(sscanf(arg1, "%d", (int *)&reset));
    return amp_reset(amp, reset);
}


/* '_' */
declare_proto_amp(get_info)
{
    const char *s;

    s = amp_get_info(amp);

    if ((interactive && prompt) || (interactive && !prompt && ext_resp))
    {
        fprintf(fout, "%s: ", cmd->arg1);
    }

    fprintf(fout, "%s%c", s ? s : "None", resp_sep);

    return RIG_OK;
}

/* '1' */
declare_proto_amp(dump_caps)
{
    dumpcaps_amp(amp, fout);

    return RIG_OK;
}


/* For ampctld internal use
 * '0x8f'
 */
declare_proto_amp(dump_state)
{
    /*
     * - Protocol version
     */
#define AMPCTLD_PROT_VER 0

    if ((interactive && prompt) || (interactive && !prompt && ext_resp))
    {
        fprintf(fout, "ampctld Protocol Ver: ");
    }

    fprintf(fout, "%d%c", AMPCTLD_PROT_VER, resp_sep);

    if ((interactive && prompt) || (interactive && !prompt && ext_resp))
    {
        fprintf(fout, "Amplifier Model: ");

        switch (amp->caps->amp_model)
        {
        case AMP_MODEL_DUMMY:
            fprintf(fout, "dummy\n");
            break;

        case AMP_MODEL_ELECRAFT_KPA1500:
            fprintf(fout, "Elecraft KPA1500\n");
            break;

        default:
            fprintf(fout, "unknown=%02x\n", amp->caps->amp_model);
            break;
        }
    }

    return RIG_OK;
}

/* '0x87' */
declare_proto_amp(set_powerstat)
{
    int stat;

    CHKSCN1ARG(sscanf(arg1, "%d", &stat));
    return amp_set_powerstat(amp, (powerstat_t) stat);
}


/* '0x88' */
declare_proto_amp(get_powerstat)
{
    int status;
    powerstat_t stat;

    status = amp_get_powerstat(amp, &stat);

    if (status != RIG_OK)
    {
        return status;
    }

    if ((interactive && prompt) || (interactive && !prompt && ext_resp))
    {
        fprintf(fout, "%s: ", cmd->arg1);
    }

    fprintf(fout, "%d\n", stat);

    return status;
}

/*
 * Special debugging purpose send command display reply until there's a
 * timeout.
 *
 * 'w'
 */
declare_proto_amp(send_cmd)
{
    int retval;
    struct amp_state *rs;
    int backend_num, cmd_len;
#define BUFSZ 128
    unsigned char bufcmd[BUFSZ];
    unsigned char buf[BUFSZ];
    char eom_buf[4] = { 0xa, 0xd, 0, 0 };

    /*
     * binary protocols enter values as \0xZZ\0xYY..
     *
     * Rem: no binary protocol for amplifier as of now
     */
    backend_num = AMP_BACKEND_NUM(amp->caps->amp_model);

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

    rs = &amp->state;

    rig_flush(&rs->ampport);

    retval = write_block(&rs->ampport, bufcmd, cmd_len);

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
        retval = read_string(&rs->ampport, buf, BUFSZ, eom_buf, strlen(eom_buf), 0, 1);

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
/*
declare_proto_amp(lonlat2loc)
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
*/


/* 'l' */
/*
declare_proto_amp(loc2lonlat)
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
*/


/* 'D' */
/*
declare_proto_amp(d_m_s2dec)
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
*/


/* 'd' */
/*
declare_proto_amp(dec2d_m_s)
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
*/


/* 'E' */
/*
declare_proto_amp(d_mm2dec)
{
    int deg, sw;
    double dec_deg, min;

    CHKSCN1ARG(sscanf(arg1, "%d", &deg));
    CHKSCN1ARG(sscanf(arg2, "%lf", &min));
    CHKSCN1ARG(sscanf(arg3, "%d", &sw));

    dec_deg = dmmm2dec(deg, min, sw);

    if ((interactive && prompt) || (interactive && !prompt && ext_resp))
    {
        fprintf(fout, "%s: ", cmd->arg4);
    }

    fprintf(fout, "%lf%c", dec_deg, resp_sep);

    return RIG_OK;
}
*/


/* 'e' */
/*
declare_proto_amp(dec2d_mm)
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
*/


/* 'B' */
/*
declare_proto_amp(coord2qrb)
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
*/


/* 'A' */
/*
declare_proto_amp(az_sp2az_lp)
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
*/


/* 'a' */
/*
declare_proto_amp(dist_sp2dist_lp)
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
*/


/* '0x8c'--pause processing */
/*
declare_proto_amp(pause)
{
    unsigned seconds;
    CHKSCN1ARG(sscanf(arg1, "%u", &seconds));
    sleep(seconds);
    return RIG_OK;
}
*/
