/*
 * rigctl_parse.c - (C) Stephane Fillod 2000-2011
 *                  (C) Nate Bargmann 2003,2006,2008,2010,2011,2012,2013
 *                  (C) Terry Embry 2008-2009
 *                  (C) The Hamlib Group 2002,2006,2007,2008,2009,2010,2011
 *
 * This program tests/controls a radio using Hamlib.
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

#ifdef HAVE_CONFIG_H
#  include "config.h"
#endif

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


#include <hamlib/rig.h>
#include "misc.h"
#include "iofunc.h"
#include "serial.h"
#include "sprintflst.h"

#include "rigctl_parse.h"

/* Hash table implementation See:  http://uthash.sourceforge.net/ */
#include "uthash.h"

#define STR1(S) #S
#define STR(S) STR1(S)

#define MAXNAMSIZ 32
#define MAXNBOPT 100    /* max number of different options */
#define MAXARGSZ 511

#define ARG_IN1  0x01
#define ARG_OUT1 0x02
#define ARG_IN2  0x04
#define ARG_OUT2 0x08
#define ARG_IN3  0x10
#define ARG_OUT3 0x20
#define ARG_IN4  0x40
#define ARG_OUT4 0x80
#define ARG_IN_LINE 0x4000
#define ARG_NOVFO 0x8000

#define ARG_IN  (ARG_IN1|ARG_IN2|ARG_IN3|ARG_IN4)
#define ARG_OUT (ARG_OUT1|ARG_OUT2|ARG_OUT3|ARG_OUT4)

static int chk_vfo_executed;

/* variables for readline support */
#ifdef HAVE_LIBREADLINE
static char *input_line = (char *)NULL;
static char *result = (char *)NULL;
static char *parsed_input[sizeof(char *) * 5];
static const int have_rl = 1;

#ifdef HAVE_READLINE_HISTORY
static char *rp_hist_buf = (char *)NULL;
#endif

#else                               /* no readline */
static const int have_rl = 0;
#endif


struct test_table
{
    unsigned char cmd;
    const char *name;
    int (*rig_routine)(RIG *,
                       FILE *,
                       FILE *,
                       int,
                       int,
                       int *,
                       char,
                       int,
                       char,
                       const struct test_table *,
                       vfo_t,
                       const char *,
                       const char *,
                       const char *);
    int flags;
    const char *arg1;
    const char *arg2;
    const char *arg3;
    const char *arg4;
};


#define CHKSCN1ARG(a) if ((a) != 1) { rig_debug(RIG_DEBUG_ERR,"%s: chkarg err\n", __func__);return -RIG_EINVAL;} else do {} while(0)

#define ACTION(f) rigctl_##f
#define declare_proto_rig(f) static int (ACTION(f))(RIG *rig,           \
                                                    FILE *fout,         \
                                                    FILE *fin,          \
                                                    int interactive,    \
                                                    int prompt,         \
                                                    int *vfo_opt,       \
                                                    char send_cmd_term, \
                                                    int ext_resp,       \
                                                    char resp_sep,      \
                                                    const struct test_table *cmd, \
                                                    vfo_t vfo,          \
                                                    const char *arg1,   \
                                                    const char *arg2,   \
                                                    const char *arg3)

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
declare_proto_rig(get_dcd);
declare_proto_rig(set_rptr_shift);
declare_proto_rig(get_rptr_shift);
declare_proto_rig(set_rptr_offs);
declare_proto_rig(get_rptr_offs);
declare_proto_rig(set_ctcss_tone);
declare_proto_rig(get_ctcss_tone);
declare_proto_rig(set_dcs_code);
declare_proto_rig(get_dcs_code);
declare_proto_rig(set_ctcss_sql);
declare_proto_rig(get_ctcss_sql);
declare_proto_rig(set_dcs_sql);
declare_proto_rig(get_dcs_sql);
declare_proto_rig(set_split_freq);
declare_proto_rig(get_split_freq);
declare_proto_rig(set_split_mode);
declare_proto_rig(get_split_mode);
declare_proto_rig(set_split_freq_mode);
declare_proto_rig(get_split_freq_mode);
declare_proto_rig(set_split_vfo);
declare_proto_rig(get_split_vfo);
declare_proto_rig(set_ts);
declare_proto_rig(get_ts);
declare_proto_rig(power2mW);
declare_proto_rig(mW2power);
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
declare_proto_rig(dump_conf);
declare_proto_rig(dump_state);
declare_proto_rig(set_ant);
declare_proto_rig(get_ant);
declare_proto_rig(reset);
declare_proto_rig(send_morse);
declare_proto_rig(stop_morse);
declare_proto_rig(wait_morse);
declare_proto_rig(send_voice_mem);
declare_proto_rig(send_cmd);
declare_proto_rig(set_powerstat);
declare_proto_rig(get_powerstat);
declare_proto_rig(send_dtmf);
declare_proto_rig(recv_dtmf);
declare_proto_rig(chk_vfo);
declare_proto_rig(set_vfo_opt);
declare_proto_rig(set_twiddle);
declare_proto_rig(get_twiddle);
declare_proto_rig(set_uplink);
declare_proto_rig(set_cache);
declare_proto_rig(get_cache);
declare_proto_rig(halt);
declare_proto_rig(pause);


/*
 * convention: upper case cmd is set, lowercase is get
 *
 * TODO: add missing rig_set_/rig_get_: sql, dcd, etc.
 * NB: 'q' 'Q' '?' are reserved by interactive mode interface
 *      do NOT use -W since it's reserved by POSIX.
 *
 *  Available alphabetic letters: -.--------------*-----W-Y-
 */
static struct test_table test_list[] =
{
#if 0 // implement set_freq VFO later if it can be detected
//    { 'F',  "set_freq",         ACTION(set_freq),       ARG_IN1 | ARG_OUT1, "Frequency" },
    { 'f',  "get_freq",         ACTION(get_freq),       ARG_OUT, "Frequency", "VFO" },
#else
    { 'F',  "set_freq",         ACTION(set_freq),       ARG_IN1, "Frequency" },
#endif
    { 'f',  "get_freq",         ACTION(get_freq),       ARG_OUT, "Frequency" },
    { 'M',  "set_mode",         ACTION(set_mode),       ARG_IN, "Mode", "Passband" },
    { 'm',  "get_mode",         ACTION(get_mode),       ARG_OUT, "Mode", "Passband" },
    { 'I',  "set_split_freq",   ACTION(set_split_freq), ARG_IN, "TX Frequency" },
    { 'i',  "get_split_freq",   ACTION(get_split_freq), ARG_OUT, "TX Frequency" },
    { 'X',  "set_split_mode",   ACTION(set_split_mode), ARG_IN, "TX Mode", "TX Passband" },
    { 'x',  "get_split_mode",   ACTION(get_split_mode), ARG_OUT, "TX Mode", "TX Passband" },
    { 'K',  "set_split_freq_mode",  ACTION(set_split_freq_mode), ARG_IN,    "TX Frequency", "TX Mode", "TX Passband" },
    { 'k',  "get_split_freq_mode",  ACTION(get_split_freq_mode), ARG_OUT,   "TX Frequency", "TX Mode", "TX Passband" },
    { 'S',  "set_split_vfo",    ACTION(set_split_vfo),  ARG_IN, "Split", "TX VFO" },
    { 's',  "get_split_vfo",    ACTION(get_split_vfo),  ARG_OUT, "Split", "TX VFO" },
    { 'N',  "set_ts",           ACTION(set_ts),         ARG_IN, "Tuning Step" },
    { 'n',  "get_ts",           ACTION(get_ts),         ARG_OUT, "Tuning Step" },
    { 'L',  "set_level",        ACTION(set_level),      ARG_IN, "Level", "Level Value" },
    { 'l',  "get_level",        ACTION(get_level),      ARG_IN1 | ARG_OUT2, "Level", "Level Value" },
    { 'U',  "set_func",         ACTION(set_func),       ARG_IN, "Func", "Func Status" },
    { 'u',  "get_func",         ACTION(get_func),       ARG_IN1 | ARG_OUT2, "Func", "Func Status" },
    { 'P',  "set_parm",         ACTION(set_parm),       ARG_IN  | ARG_NOVFO, "Parm", "Parm Value" },
    { 'p',  "get_parm",         ACTION(get_parm),       ARG_IN1 | ARG_OUT2 | ARG_NOVFO, "Parm", "Parm Value" },
    { 'G',  "vfo_op",           ACTION(vfo_op),         ARG_IN, "Mem/VFO Op" },
    { 'g',  "scan",             ACTION(scan),           ARG_IN, "Scan Fct", "Scan Channel" },
    { 'A',  "set_trn",          ACTION(set_trn),        ARG_IN  | ARG_NOVFO, "Transceive" },
    { 'a',  "get_trn",          ACTION(get_trn),        ARG_OUT | ARG_NOVFO, "Transceive" },
    { 'R',  "set_rptr_shift",   ACTION(set_rptr_shift), ARG_IN, "Rptr Shift" },
    { 'r',  "get_rptr_shift",   ACTION(get_rptr_shift), ARG_OUT, "Rptr Shift" },
    { 'O',  "set_rptr_offs",    ACTION(set_rptr_offs),  ARG_IN, "Rptr Offset" },
    { 'o',  "get_rptr_offs",    ACTION(get_rptr_offs),  ARG_OUT, "Rptr Offset" },
    { 'C',  "set_ctcss_tone",   ACTION(set_ctcss_tone), ARG_IN, "CTCSS Tone" },
    { 'c',  "get_ctcss_tone",   ACTION(get_ctcss_tone), ARG_OUT, "CTCSS Tone" },
    { 'D',  "set_dcs_code",     ACTION(set_dcs_code),   ARG_IN, "DCS Code" },
    { 'd',  "get_dcs_code",     ACTION(get_dcs_code),   ARG_OUT, "DCS Code" },
    { 0x90, "set_ctcss_sql",    ACTION(set_ctcss_sql),  ARG_IN, "CTCSS Sql" },
    { 0x91, "get_ctcss_sql",    ACTION(get_ctcss_sql),  ARG_OUT, "CTCSS Sql" },
    { 0x92, "set_dcs_sql",      ACTION(set_dcs_sql),    ARG_IN, "DCS Sql" },
    { 0x93, "get_dcs_sql",      ACTION(get_dcs_sql),    ARG_OUT, "DCS Sql" },
    //
    //{ 'V',  "set_vfo",          ACTION(set_vfo),        ARG_IN  | ARG_NOVFO | ARG_OUT, "VFO" },
    { 'V',  "set_vfo",          ACTION(set_vfo),        ARG_IN  | ARG_NOVFO, "VFO" },
    { 'v',  "get_vfo",          ACTION(get_vfo),        ARG_NOVFO | ARG_OUT, "VFO" },
    { 'T',  "set_ptt",          ACTION(set_ptt),        ARG_IN, "PTT" },
    { 't',  "get_ptt",          ACTION(get_ptt),        ARG_OUT, "PTT" },
    { 'E',  "set_mem",          ACTION(set_mem),        ARG_IN, "Memory#" },
    { 'e',  "get_mem",          ACTION(get_mem),        ARG_OUT, "Memory#" },
    { 'H',  "set_channel",      ACTION(set_channel),    ARG_IN  | ARG_NOVFO, "Channel"},
    { 'h',  "get_channel",      ACTION(get_channel),    ARG_IN  | ARG_NOVFO, "Channel", "Read Only" },
    { 'B',  "set_bank",         ACTION(set_bank),       ARG_IN, "Bank" },
    { '_',  "get_info",         ACTION(get_info),       ARG_OUT | ARG_NOVFO, "Info" },
    { 'J',  "set_rit",          ACTION(set_rit),        ARG_IN, "RIT" },
    { 'j',  "get_rit",          ACTION(get_rit),        ARG_OUT, "RIT" },
    { 'Z',  "set_xit",          ACTION(set_xit),        ARG_IN, "XIT" },
    { 'z',  "get_xit",          ACTION(get_xit),        ARG_OUT, "XIT" },
    { 'Y',  "set_ant",          ACTION(set_ant),        ARG_IN, "Antenna", "Option" },
    { 'y',  "get_ant",          ACTION(get_ant),        ARG_IN1 | ARG_OUT2 | ARG_NOVFO, "AntCurr", "Option", "AntTx", "AntRx" },
    { 0x87, "set_powerstat",    ACTION(set_powerstat),  ARG_IN  | ARG_NOVFO, "Power Status" },
    { 0x88, "get_powerstat",    ACTION(get_powerstat),  ARG_OUT | ARG_NOVFO, "Power Status" },
    { 0x89, "send_dtmf",        ACTION(send_dtmf),      ARG_IN, "Digits" },
    { 0x8a, "recv_dtmf",        ACTION(recv_dtmf),      ARG_OUT, "Digits" },
    { '*',  "reset",            ACTION(reset),          ARG_IN, "Reset" },
    { 'w',  "send_cmd",         ACTION(send_cmd),       ARG_IN1 | ARG_IN_LINE | ARG_OUT2 | ARG_NOVFO, "Cmd", "Reply" },
    { 'W',  "send_cmd_rx",      ACTION(send_cmd),       ARG_IN | ARG_OUT2 | ARG_NOVFO, "Cmd", "Reply"},
    { 'b',  "send_morse",       ACTION(send_morse),     ARG_IN  | ARG_IN_LINE, "Morse" },
    { 0xbb, "stop_morse",       ACTION(stop_morse),     },
    { 0xbc, "wait_morse",       ACTION(wait_morse),     },
    { 0x94, "send_voice_mem",   ACTION(send_voice_mem), ARG_IN, "Voice Mem#" },
    { 0x8b, "get_dcd",          ACTION(get_dcd),        ARG_OUT, "DCD" },
    { 0x8d, "set_twiddle",      ACTION(set_twiddle),    ARG_IN  | ARG_NOVFO, "Timeout (secs)" },
    { 0x8e, "get_twiddle",      ACTION(get_twiddle),    ARG_OUT | ARG_NOVFO, "Timeout (secs)" },
    { 0x97, "uplink",           ACTION(set_uplink),     ARG_IN | ARG_NOVFO, "1=Sub, 2=Main" },
    { 0x95, "set_cache",        ACTION(set_cache),      ARG_IN | ARG_NOVFO, "Timeout (msecs)" },
    { 0x96, "get_cache",        ACTION(get_cache),      ARG_OUT | ARG_NOVFO, "Timeout (msecs)" },
    { '2',  "power2mW",         ACTION(power2mW),       ARG_IN1 | ARG_IN2 | ARG_IN3 | ARG_OUT1 | ARG_NOVFO, "Power [0.0..1.0]", "Frequency", "Mode", "Power mW" },
    { '4',  "mW2power",         ACTION(mW2power),       ARG_IN1 | ARG_IN2 | ARG_IN3 | ARG_OUT1 | ARG_NOVFO, "Pwr mW", "Freq", "Mode", "Power [0.0..1.0]" },
    { '1',  "dump_caps",        ACTION(dump_caps),      ARG_NOVFO },
    { '3',  "dump_conf",        ACTION(dump_conf),      ARG_NOVFO },
    { 0x8f, "dump_state",       ACTION(dump_state),     ARG_OUT | ARG_NOVFO },
    { 0xf0, "chk_vfo",          ACTION(chk_vfo),        ARG_NOVFO, "ChkVFO" },   /* rigctld only--check for VFO mode */
    { 0xf2, "set_vfo_opt",      ACTION(set_vfo_opt),    ARG_NOVFO | ARG_IN, "Status" }, /* turn vfo option on/off */
    { 0xf1, "halt",             ACTION(halt),           ARG_NOVFO },   /* rigctld only--halt the daemon */
    { 0x8c, "pause",            ACTION(pause),          ARG_IN, "Seconds" },
    { 0x00, "", NULL },
};


static struct test_table *find_cmd_entry(int cmd)
{
    int i;

    for (i = 0; test_list[i].cmd != 0x00; i++)
    {
        if (test_list[i].cmd == cmd)
        {
            break;
        }
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
    int id;                 /* caps->rig_model This is the hash key */
    char mfg_name[32];      /* caps->mfg_name */
    char model_name[32];    /* caps->model_name */
    char version[32];       /* caps->version */
    char status[32];        /* caps->status */
    char macro_name[32];        /* caps->status */
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

    s = (struct mod_lst *)malloc(sizeof(struct mod_lst));

    s->id = id;
    snprintf(s->mfg_name, sizeof(s->mfg_name), "%s", mfg_name);
    snprintf(s->model_name, sizeof(s->model_name), "%s", model_name);
    snprintf(s->version, sizeof(s->version), "%s", version);
    snprintf(s->status, sizeof(s->status), "%s", status);
    snprintf(s->macro_name, sizeof(s->macro_name), "%s", macro_name);

    HASH_ADD_INT(models, id, s);    /* id: name of key field */
}


/* Hash sorting functions */
int hash_model_id_sort(struct mod_lst *a, struct mod_lst *b)
{
    return (a->id > b->id);
}


void hash_sort_by_model_id()
{
    if (models != NULL)
    {
        HASH_SORT(models, hash_model_id_sort);
    }
    else
    {
        rig_debug(RIG_DEBUG_ERR, "%s: models empty?\n", __func__);
    }
}


/* Delete hash */
void hash_delete_all()
{
    struct mod_lst *current_model, *tmp;

    HASH_ITER(hh, models, current_model, tmp)
    {
        HASH_DEL(models, current_model);    /* delete it (models advances to next) */
        free(current_model);                /* free it */
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

    for (i = 0; i < 5; i++)
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
static char parse_arg(const char *arg)
{
    int i;

    for (i = 0; test_list[i].cmd != 0x00; i++)
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
        *(char *)p = 0;

        ret = fscanf(fin, format, p);

        if (ret < 0)
        {
            if (errno == EINTR)
            {
                continue;
            }

            if (!feof(fin))
            {
                rig_debug(RIG_DEBUG_ERR,
                          "fscanf: parsing '%s' with '%s'\n",
                          (char *)p,
                          format);
            }
        }

        if (ret < 1) { rig_debug(RIG_DEBUG_TRACE, "%s: ret=%d\n", __func__, ret); }

        if (ferror(fin)) { rig_debug(RIG_DEBUG_TRACE, "%s: errno=%d, %s\n", __func__, errno, strerror(errno)); }

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
        else if (newline && '-' == argv[optind][0] && 1 == strlen(argv[optind]))
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


#define fprintf_flush(f, a...)        \
    ({ fprintf((f), a);               \
       fflush((f));                   \
                                      \
    })


int rigctl_parse(RIG *my_rig, FILE *fin, FILE *fout, char *argv[], int argc,
                 sync_cb_t sync_cb,
                 int interactive, int prompt, int *vfo_opt, char send_cmd_term,
                 int *ext_resp_ptr, char *resp_sep_ptr)
{
    int retcode;        /* generic return code from functions */
    unsigned char cmd;
    struct test_table *cmd_entry = NULL;

    char command[MAXARGSZ + 1];
    char arg1[MAXARGSZ + 1], *p1 = NULL;
    char arg2[MAXARGSZ + 1], *p2 = NULL;
    char arg3[MAXARGSZ + 1], *p3 = NULL;
    vfo_t vfo = RIG_VFO_CURR;

    rig_debug(RIG_DEBUG_TRACE, "%s: called, interactive=%d\n", __func__,
              interactive);

    /* cmd, internal, rigctld */
    if (!(interactive && prompt && have_rl))
    {

        if (interactive)
        {
            static int last_was_ret = 1;

            if (prompt)
            {
                fprintf_flush(fout, "\nRig command: ");
            }

            do
            {
                if ((retcode = scanfc(fin, "%c", &cmd)) < 1)
                {
                    rig_debug(RIG_DEBUG_WARN, "%s: nothing to scan#1? retcode=%d\n", __func__,
                              retcode);
                    return -1;
                }

                if (cmd != 0xa)
                {
                    rig_debug(RIG_DEBUG_TRACE, "%s: cmd=%c(%02x)\n", __func__,
                              isprint(cmd) ? cmd : ' ', cmd);
                }

                /* Extended response protocol requested with leading '+' on command
                 * string--rigctld only!
                 */
                if (cmd == '+' && !prompt)
                {
                    *ext_resp_ptr = 1;

                    if (scanfc(fin, "%c", &cmd) < 1)
                    {
                        rig_debug(RIG_DEBUG_WARN, "%s: nothing to scan#2?\n", __func__);
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
                        && cmd != '('
                        && cmd != ')'
                        && ispunct(cmd)
                        && !prompt)
                {

                    *ext_resp_ptr = 1;
                    *resp_sep_ptr = cmd;

                    if (scanfc(fin, "%c", &cmd) < 1)
                    {
                        rig_debug(RIG_DEBUG_WARN, "%s: nothing to scan#3?\n", __func__);
                        return -1;
                    }
                }
                else if (cmd != '\\'
                         && cmd != '?'
                         && cmd != '_'
                         && cmd != '#'
                         && cmd != '('
                         && cmd != ')'
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
                        rig_debug(RIG_DEBUG_WARN, "%s: nothing to scan#4?\n", __func__);
                        return -1;
                    }

                    while (c_len-- && (isalnum(*pcmd) || *pcmd == '_'))
                    {
                        if (scanfc(fin, "%c", ++pcmd) < 1)
                        {
                            rig_debug(RIG_DEBUG_WARN, "%s: nothing to scan#5?\n", __func__);
                            return -1;
                        }
                    }

                    *pcmd = '\0';
                    cmd = parse_arg((char *)cmd_name);
                    rig_debug(RIG_DEBUG_VERBOSE, "%s: cmd=%s\n", __func__, cmd_name);
                    break;
                }

                rig_debug(RIG_DEBUG_VERBOSE, "%s: cmd==0x%02x\n", __func__, cmd);

                if (cmd == 0x0a || cmd == 0x0d)
                {
                    if (last_was_ret)
                    {
                        if (prompt)
                        {
                            fprintf(fout, "? for help, q to quit.\n");
                            fprintf_flush(fout, "\nRig command: ");
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
                        rig_debug(RIG_DEBUG_WARN, "%s: nothing to scan#6?\n", __func__);
                        return -1;
                    }
                }

                return 0;
            }

            my_rig->state.vfo_opt = *vfo_opt;
            rig_debug(RIG_DEBUG_TRACE, "%s: vfo_opt=%d\n", __func__, *vfo_opt);

            if (cmd == 'Q' || cmd == 'q')
            {
                rig_debug(RIG_DEBUG_TRACE, "%s: quit returning NETRIGCTL_RET 0\n", __func__);

                if (interactive && !prompt) { fprintf(fout, "%s0\n", NETRIGCTL_RET); }

                fflush(fout);
                return 1;
            }

            if (cmd == '?')
            {
                usage_rig(fout);
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
            if (cmd != ' ')
            {
                fprintf(stderr, "Command '%c' not found!\n", cmd);
            }

            return 0;
        }

        if (!(cmd_entry->flags & ARG_NOVFO) && *vfo_opt)
        {
            if (interactive)
            {
                if (prompt)
                {
                    fprintf_flush(fout, "VFO: ");
                }

                if (scanfc(fin, "%s", arg1) < 1)
                {
                    rig_debug(RIG_DEBUG_WARN, "%s: nothing to scan#7?\n", __func__);
                    return -1;
                }

                vfo = rig_parse_vfo(arg1);
            }
            else
            {
                retcode = next_word(arg1, argc, argv, 0);

                if (EOF == retcode)
                {
                    fprintf(stderr, "Invalid arg for command '%s'\n",
                            cmd_entry->name);
                }
                else if (retcode < 0)
                {
                    return retcode;
                }

                vfo = rig_parse_vfo(arg1);
            }
        }

        rig_debug(RIG_DEBUG_TRACE, "%s: debug1\n", __func__);

        if ((cmd_entry->flags & ARG_IN_LINE)
                && (cmd_entry->flags & ARG_IN1)
                && cmd_entry->arg1)
        {
            rig_debug(RIG_DEBUG_TRACE, "%s: debug2\n", __func__);

            if (interactive)
            {
                char *nl;
                rig_debug(RIG_DEBUG_TRACE, "%s: debug2a\n", __func__);


                if (fgets(arg1, MAXARGSZ, fin) == NULL)
                {
                    return -1;
                }

                if (arg1[0] == 0xa)
                {
                    rig_debug(RIG_DEBUG_TRACE, "%s: debug2b\n", __func__);

                    if (prompt)
                    {
                        fprintf_flush(fout, "%s: ", cmd_entry->arg1);
                    }

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

                if (cmd == 'b')
                {
                    p1 = arg1; /* CW must accept a space argument */
                }
                else   /* skip a space arg if first arg...but why? */
                {
                    p1 = arg1[0] == ' ' ? arg1 + 1 : arg1;
                }
            }
            else
            {
                retcode = next_word(arg1, argc, argv, 0);

                if (EOF == retcode)
                {
                    fprintf(stderr, "Invalid arg for command '%s'\n",
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
        else if ((cmd_entry->flags & ARG_IN1) && cmd_entry->arg1)
        {
            rig_debug(RIG_DEBUG_TRACE, "%s: debug3\n", __func__);

            if (interactive)
            {
                int c = fgetc(fin);
                rig_debug(RIG_DEBUG_TRACE, "%s: debug4 c=%02x\n", __func__, c);

                if (prompt && c == 0x0a)
                {
                    fprintf_flush(fout, "%s: ", cmd_entry->arg1);
                }
                else
                {
                    ungetc(c, fin);
                }

                if (scanfc(fin, "%s", arg1) < 1)
                {
                    rig_debug(RIG_DEBUG_WARN, "%s: nothing to scan#8?\n", __func__);
                    return -1;
                }

                p1 = arg1;
            }
            else
            {
                retcode = next_word(arg1, argc, argv, 0);

                if (EOF == retcode)
                {
                    fprintf(stderr, "Invalid arg for command '%s'\n",
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

        rig_debug(RIG_DEBUG_TRACE, "%s: debug5\n", __func__);

        if (p1
                && p1[0] != '?'
                && (cmd_entry->flags & ARG_IN2)
                && cmd_entry->arg2)
        {
            rig_debug(RIG_DEBUG_TRACE, "%s: debug6\n", __func__);

            if (interactive)
            {
                rig_debug(RIG_DEBUG_TRACE, "%s: debug7\n", __func__);

#if 0 // was printing Reply: twice

                if (prompt)
                {
                    rig_debug(RIG_DEBUG_TRACE, "%s: debug8\n", __func__);
                    fprintf_flush(fout, "%s: ", cmd_entry->arg2);
                }

#endif

                if (scanfc(fin, "%s", arg2) < 1)
                {
                    rig_debug(RIG_DEBUG_WARN, "%s: nothing to scan#9?\n", __func__);
                    return -1;
                }

                p2 = arg2;
            }
            else
            {
                rig_debug(RIG_DEBUG_TRACE, "%s: debug9\n", __func__);
                retcode = next_word(arg2, argc, argv, 0);

                if (EOF == retcode)
                {
                    fprintf(stderr, "Invalid arg for command '%s'\n",
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

        rig_debug(RIG_DEBUG_TRACE, "%s: debug10\n", __func__);

        if (p1
                && p1[0] != '?'
                && (cmd_entry->flags & ARG_IN3)
                && cmd_entry->arg3)
        {
            rig_debug(RIG_DEBUG_TRACE, "%s: debug11\n", __func__);

            if (interactive)
            {
                rig_debug(RIG_DEBUG_TRACE, "%s: debug12\n", __func__);

                if (prompt)
                {
                    rig_debug(RIG_DEBUG_TRACE, "%s: debug13\n", __func__);
                    fprintf_flush(fout, "%s: ", cmd_entry->arg3);
                }

                if (scanfc(fin, "%s", arg3) < 1)
                {
                    rig_debug(RIG_DEBUG_WARN, "%s: nothing to scan#10?\n", __func__);
                    return -1;
                }

                p3 = arg3;
            }
            else
            {
                rig_debug(RIG_DEBUG_TRACE, "%s: debug14\n", __func__);
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
    }

#ifdef HAVE_LIBREADLINE

    if (interactive && prompt && have_rl)
    {
        int j, x;

#ifdef HAVE_READLINE_HISTORY
        /* Minimum space for 32+1+32+1+128+1+128+1+128+1 = 453 chars, so
         * allocate 512 chars cleared to zero for safety.
         */
        rp_hist_buf = (char *)calloc(512, sizeof(char));
#endif

        rp_getline("\nRig command: ");

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
            usage_rig(fout);
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

        rig_debug(RIG_DEBUG_TRACE, "%s: input_line: %s\n", __func__, input_line);

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
            snprintf(cmd_name, sizeof(cmd_name), "%s", parsed_input[0] + 1);

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

        /* If vfo_opt is enabled (-o|--vfo) check if already given
         * or prompt for it.
         */
        if (!(cmd_entry->flags & ARG_NOVFO) && *vfo_opt)
        {
            /* Check if VFO was given with command. */
            result = strtok(NULL, " ");

            if (result)
            {
                x = 1;
                parsed_input[x] = result;
            }
            /* Need to prompt if a VFO string was not given. */
            else
            {
                x = 0;
                rp_getline("VFO: ");

                if (!input_line)
                {
                    fprintf_flush(fout, "\n");
                    return 1;
                }

                /* Blank line entered */
                if (!(strcmp(input_line, "")))
                {
                    fprintf(fout, "? for help, q to quit.\n");
                    fflush(fout);
                    return 0;
                }

                /* Get the first token of input, the rest, if any, will be
                 * used later.
                 */
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

            /* VFO name tokens are presently quite short.  Truncate excessively
             * long strings.
             */
            if (strlen(parsed_input[x]) >= MAXNAMSIZ)
            {
                *(parsed_input[x] + (MAXNAMSIZ - 1)) = '\0';
            }

#ifdef HAVE_READLINE_HISTORY

            if (rp_hist_buf)
            {
                strncat(rp_hist_buf, " ", 2);
                strncat(rp_hist_buf, parsed_input[x], MAXNAMSIZ);
            }

#endif

            /* Sanity check, VFO names are alpha only. */
            for (j = 0; j < MAXNAMSIZ && parsed_input[x][j] != '\0'; j++)
            {
                if (!(isalpha((int)parsed_input[x][j])))
                {
                    parsed_input[x][j] = '\0';

                    break;
                }
            }

            vfo = rig_parse_vfo(parsed_input[x]);

            if (vfo == RIG_VFO_NONE)
            {
                fprintf(stderr,
                        "Warning:  VFO '%s' unrecognized, using 'currVFO' instead.\n",
                        parsed_input[x]);
                vfo = RIG_VFO_CURR;
            }
        }

        /* \send_cmd, \send_morse */
        if ((cmd_entry->flags & ARG_IN_LINE)
                && (cmd_entry->flags & ARG_IN1)
                && cmd_entry->arg1)
        {
            /* Check for a non-existent delimiter so as to not break up
             * remaining line into separate tokens (spaces OK).
             */
            result = strtok(NULL, "\0");

            if (*vfo_opt && result)
            {
                x = 2;
                parsed_input[x] = result;
            }
            else if (result)
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

            if (*vfo_opt && result)
            {
                x = 2;
                parsed_input[x] = result;
            }
            else if (result)
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

            if (*vfo_opt && result)
            {
                x = 3;
                parsed_input[x] = result;
            }
            else if (result)
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

            if (*vfo_opt && result)
            {
                x = 4;
                parsed_input[x] = result;
            }
            else if (result)
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

    if (sync_cb) { sync_cb(1); }    /* lock if necessary */

    if (!prompt)
    {
        rig_debug(RIG_DEBUG_TRACE,
                  "rigctl(d): %c '%s' '%s' '%s' '%s'\n",
                  cmd,
                  rig_strvfo(vfo),
                  p1 ? p1 : "",
                  p2 ? p2 : "",
                  p3 ? p3 : "");
    }

    /*
     * Extended Response protocol: output received command name and arguments
     * response.  Don't send command header on '\chk_vfo' command.
     */
    if (interactive && *ext_resp_ptr && !prompt && cmd != 0xf0)
    {
        char a1[MAXARGSZ + 2];
        char a2[MAXARGSZ + 2];
        char a3[MAXARGSZ + 2];
        char vfo_str[MAXARGSZ + 2];

        *vfo_opt == 0 ? vfo_str[0] = '\0' : snprintf(vfo_str,
                                     sizeof(vfo_str),
                                     " %s",
                                     rig_strvfo(vfo));

        p1 == NULL ? a1[0] = '\0' : snprintf(a1, sizeof(a1), " %s", p1);
        p2 == NULL ? a2[0] = '\0' : snprintf(a2, sizeof(a2), " %s", p2);
        p3 == NULL ? a3[0] = '\0' : snprintf(a3, sizeof(a3), " %s", p3);

        fprintf(fout,
                "%s:%s%s%s%s%c",
                cmd_entry->name,
                vfo_str,
                a1,
                a2,
                a3,
                *resp_sep_ptr);
    }

    rig_debug(RIG_DEBUG_TRACE, "%s: vfo_opt=%d\n", __func__, *vfo_opt);
    retcode = (*cmd_entry->rig_routine)(my_rig,
                                        fout,
                                        fin,
                                        interactive,
                                        prompt,
                                        vfo_opt,
                                        send_cmd_term,
                                        *ext_resp_ptr,
                                        *resp_sep_ptr,
                                        cmd_entry,
                                        vfo,
                                        p1,
                                        p2 ? p2 : "",
                                        p3 ? p3 : "");

    rig_debug(RIG_DEBUG_TRACE, "%s: vfo_opt=%d\n", __func__, *vfo_opt);

    if (retcode == RIG_EIO)
    {
        rig_debug(RIG_DEBUG_ERR, "%s: RIG_EIO?\n", __func__);

        if (sync_cb) { sync_cb(0); }    /* unlock if necessary */

        return retcode;
    }

    if (retcode != RIG_OK)
    {
        /* only for rigctld */
        if (interactive && !prompt)
        {
            rig_debug(RIG_DEBUG_TRACE, "%s: return#1 "NETRIGCTL_RET "%d\n", __func__,
                      retcode);
            fprintf(fout, NETRIGCTL_RET "%d\n", retcode);
            *ext_resp_ptr = 0;
            *resp_sep_ptr = '\n';
        }
        else
        {
            fprintf(fout,
                    "%s: error = %s\n",
                    cmd_entry->name,
                    rigerror(retcode));
        }
    }
    else
    {
        /* only for rigctld */
        if (interactive && !prompt)
        {
            /* netrigctl RIG_OK */
            if (!(cmd_entry->flags & ARG_OUT)
                    && !*ext_resp_ptr && cmd != 0xf0)
            {
                rig_debug(RIG_DEBUG_TRACE, "%s: return#2 "NETRIGCTL_RET "0\n", __func__);
                fprintf(fout, NETRIGCTL_RET "0\n");
            }

            /* Extended Response protocol */
            else if (*ext_resp_ptr && cmd != 0xf0)
            {
                rig_debug(RIG_DEBUG_TRACE, "%s: return#3 "NETRIGCTL_RET "0\n", __func__);
                fprintf(fout, NETRIGCTL_RET "0\n");
                *ext_resp_ptr = 0;
                *resp_sep_ptr = '\n';
            }
        }
    }

    fflush(fout);

    rig_debug(RIG_DEBUG_TRACE, "%s: retcode=%d\n", __func__, retcode);

    if (sync_cb) { sync_cb(0); }    /* unlock if necessary */

    if (retcode == -RIG_ENAVAIL)
    {
        return retcode;
    }

    return retcode != RIG_OK ? 2 : 0;
}


void version()
{
    printf("rigctl(d), %s\n\n", hamlib_version);
    printf("%s\n", hamlib_copyright);
}


void usage_rig(FILE *fout)
{
    int i;

    fprintf(fout, "Commands (some may not be available for this rig):\n");

    for (i = 0; test_list[i].cmd != 0; i++)
    {
        int nbspaces = 18;
        fprintf(fout,
                "%c: %-16s(",
                isprint(test_list[i].cmd) ? test_list[i].cmd : '?',
                test_list[i].name);


        if (test_list[i].arg1 && (test_list[i].flags & ARG_IN1))
        {
            nbspaces -= fprintf(fout, "%s", test_list[i].arg1);
        }

        if (test_list[i].arg2 && (test_list[i].flags & ARG_IN2))
        {
            nbspaces -= fprintf(fout, ",%s", test_list[i].arg2);
        }

        if (test_list[i].arg3 && (test_list[i].flags & ARG_IN3))
        {
            nbspaces -= fprintf(fout, ",%s", test_list[i].arg3);
        }

        if (i % 2)
        {
            fprintf(fout, ")\n");
        }
        else
        {
            fprintf(fout, ")%*s", nbspaces, " ");
        }
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
    RIG *rig = (RIG *) data;
    int i;
    char buf[128] = "";

    rig_get_conf(rig, cfp->token, buf);
    printf("%s: \"%s\"\n" "\t" "Default: %s, Value: %s\n",
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

    case RIG_CONF_STRING:
        printf("\tString.\n");
        break;

    case RIG_CONF_CHECKBUTTON:
        printf("\tCheck button.\n");
        break;

    case RIG_CONF_BUTTON:
        printf("\tButton.\n");
        break;

    default:
        printf("\tUnknown conf\n");
    }

    return 1;  /* !=0, we want them all ! */
}


static int hash_model_list(const struct rig_caps *caps, void *data)
{
    hash_add_model(caps->rig_model,
                   caps->mfg_name,
                   caps->model_name,
                   caps->version,
                   caps->macro_name,
                   rig_strstatus(caps->status));

    return 1;  /* !=0, we want them all ! */
}


void print_model_list()
{
    struct mod_lst *s;

    for (s = models; s != NULL; s = (struct mod_lst *)(s->hh.next))
    {
        printf("%6d  %-23s%-24s%-16s%-12s%s\n",
               s->id,
               s->mfg_name,
               s->model_name,
               s->version,
               s->macro_name,
               s->status);
    }
}


void list_models()
{
    int status;

    rig_load_all_backends();

    printf(" Rig #  Mfg                    Model                   Version         Status      Macro\n");
    status = rig_list_foreach(hash_model_list, NULL);

    if (status != RIG_OK)
    {
        printf("rig_list_foreach: error = %s \n", rigerror(status));
        exit(2);
    }

    hash_sort_by_model_id();
    print_model_list();
    hash_delete_all();
}


int set_conf(RIG *my_rig, char *conf_parms)
{
    char *p, *n;

    p = conf_parms;

    while (p && *p != '\0')
    {
        int ret;

        /* FIXME: left hand value of = cannot be null */
        char *q = strchr(p, '=');

        if (!q)
        {
            return -RIG_EINVAL;
        }

        *q++ = '\0';
        n = strchr(q, ',');

        if (n)
        {
            *n++ = '\0';
        }

        ret = rig_set_conf(my_rig, rig_token_lookup(my_rig, p), q);

        if (ret != RIG_OK)
        {
            return ret;
        }

        p = n;
    }

    return RIG_OK;
}


/*
 * static int (f)(RIG *rig, FILE *fout, int interactive, const struct test_table *cmd,
 *      vfo_t vfo, const void *arg1, const void *arg2, const void *arg3)
 */

/* 'F' */
declare_proto_rig(set_freq)
{
    freq_t freq;
    int retval;
#if 0 // implement set_freq VFO later if it can be detected
    char *fmt = "%"PRIll"%c";
#endif

    CHKSCN1ARG(sscanf(arg1, "%"SCNfreq, &freq));
    retval = rig_set_freq(rig, vfo, freq);

    if (retval == RIG_OK)
    {
        //fprintf(fout, "%s%c", rig_strvfo(vfo), resp_sep);
        //fprintf(fout, fmt, (int64_t)freq, resp_sep);

    }

    return retval;
}


/* 'f' */
declare_proto_rig(get_freq)
{
    int status;
    freq_t freq;
    // cppcheck-suppress *
    char *fmt = "%"PRIll"%c";

    status = rig_get_freq(rig, vfo, &freq);

    if (status != RIG_OK)
    {
        return status;
    }

    if ((interactive && prompt) || (interactive && !prompt && ext_resp))
    {
        fprintf(fout, "%s: ", cmd->arg1);    /* i.e. "Frequency" */
    }

    fprintf(fout, fmt, (int64_t)freq, resp_sep);

#if 0 // this extra VFO being returned was confusing Log4OM

    if ((interactive && prompt) || (interactive && !prompt && ext_resp))
    {
        fprintf(fout, "%s: ", cmd->arg2);    /* i.e. "Frequency" */
    }

    fprintf(fout, "%s%c", rig_strvfo(vfo), resp_sep);
#endif

    return status;
}


/* 'J' */
declare_proto_rig(set_rit)
{
    shortfreq_t rit;

    CHKSCN1ARG(sscanf(arg1, "%ld", &rit));
    return rig_set_rit(rig, vfo, rit);
}


/* 'j' */
declare_proto_rig(get_rit)
{
    int status;
    shortfreq_t rit;

    status = rig_get_rit(rig, vfo, &rit);

    if (status != RIG_OK)
    {
        return status;
    }

    if ((interactive && prompt) || (interactive && !prompt && ext_resp))
    {
        fprintf(fout, "%s: ", cmd->arg1);
    }

    fprintf(fout, "%ld%c", rit, resp_sep);

    return status;
}


/* 'Z' */
declare_proto_rig(set_xit)
{
    shortfreq_t xit;

    CHKSCN1ARG(sscanf(arg1, "%ld", &xit));
    return rig_set_xit(rig, vfo, xit);
}


/* 'z' */
declare_proto_rig(get_xit)
{
    int status;
    shortfreq_t xit;

    status = rig_get_xit(rig, vfo, &xit);

    if (status != RIG_OK)
    {
        return status;
    }

    if ((interactive && prompt) || (interactive && !prompt && ext_resp))
    {
        fprintf(fout, "%s: ", cmd->arg1);
    }

    fprintf(fout, "%ld%c", xit, resp_sep);

    return status;
}


/* 'M' */
declare_proto_rig(set_mode)
{
    rmode_t mode;
    pbwidth_t width;

    if (!strcmp(arg1, "?"))
    {
        char s[SPRINTF_MAX_SIZE];
        sprintf_mode(s, rig->state.mode_list);
        fprintf(fout, "%s\n", s);
        return RIG_OK;
    }

    mode = rig_parse_mode(arg1);
    CHKSCN1ARG(sscanf(arg2, "%ld", &width));
    return rig_set_mode(rig, vfo, mode, width);
}


/* 'm' */
declare_proto_rig(get_mode)
{
    int status;
    rmode_t mode;
    pbwidth_t width;

    status = rig_get_mode(rig, vfo, &mode, &width);

    if (status != RIG_OK)
    {
        return status;
    }

    if ((interactive && prompt) || (interactive && !prompt && ext_resp))
    {
        fprintf(fout, "%s: ", cmd->arg1);
    }

    fprintf(fout, "%s%c", rig_strrmode(mode), resp_sep);

    if ((interactive && prompt) || (interactive && !prompt && ext_resp))
    {
        fprintf(fout, "%s: ", cmd->arg2);
    }

    fprintf(fout, "%ld%c", width, resp_sep);

    return status;
}


/* 'V' */
declare_proto_rig(set_vfo)
{
    int retval;

    if (!strcmp(arg1, "?"))
    {
        char s[SPRINTF_MAX_SIZE];
        sprintf_vfo(s, rig->state.vfo_list);
        fprintf(fout, "%s\n", s);
        return RIG_OK;
    }

    vfo = rig_parse_vfo(arg1);
    retval = rig_set_vfo(rig, vfo);

#if 0 // see if we can make this dynamic

    if (retval == RIG_OK)
    {
        if ((interactive && prompt) || (interactive && !prompt && ext_resp))
        {
            // fprintf(fout, "%s: ", cmd->arg1);
        }

        fprintf(fout, "%s%c", rig_strvfo(vfo), resp_sep);
    }

#endif

    return retval;
}


/* 'v' */
declare_proto_rig(get_vfo)
{
    int status;

    status = rig_get_vfo(rig, &vfo);

    if (status != RIG_OK)
    {
        return status;
    }

    if ((interactive && prompt) || (interactive && !prompt && ext_resp))
    {
        fprintf(fout, "%s: ", cmd->arg1);
    }

    fprintf(fout, "%s%c", rig_strvfo(vfo), resp_sep);

    return status;
}


/* 'T' */
declare_proto_rig(set_ptt)
{
    int   scr;
    ptt_t ptt;

    CHKSCN1ARG(sscanf(arg1, "%d", &scr));
    ptt = scr;

    /*
     * We allow RIG_PTT_ON_MIC and RIG_PTT_ON_DATA arriving from netrigctl.
     * However, if the rig does not have two separate CAT commands, or if
     * the rig is actually switched by a hardware signal (DTR etc.), then
     * we map this to RIG_PTT_ON.
     * Currently, this is not really necessary here because it is taken
     * case of in rig_set_ptt, but you never know ....
     */
    switch (ptt)
    {
    case RIG_PTT_ON_MIC:
    case RIG_PTT_ON_DATA:

        // map to a legal value
        if (rig->state.pttport.type.ptt != RIG_PTT_RIG_MICDATA)
        {
            ptt = RIG_PTT_ON;
        }

        break;

    case RIG_PTT_ON:
    case RIG_PTT_OFF:
        // nothing to do
        break;

    default:
        // this case is not handled in hamlib, but we guard against
        // illegal parameters here. The hamlib behaviour is to switch
        // on PTT whenever ptt != RIG_PTT_OFF.
        return -RIG_EINVAL;
    }

    return rig_set_ptt(rig, vfo, ptt);
}


/* 't' */
declare_proto_rig(get_ptt)
{
    int status;
    ptt_t ptt = 0;

    status = rig_get_ptt(rig, vfo, &ptt);

    if (status != RIG_OK)
    {
        return status;
    }

    if ((interactive && prompt) || (interactive && !prompt && ext_resp))
    {
        fprintf(fout, "%s: ", cmd->arg1);
    }

    /* TODO MICDATA */
    fprintf(fout, "%d%c", ptt, resp_sep);

    return status;
}


/* '0x8b' */
declare_proto_rig(get_dcd)
{
    int status;
    dcd_t dcd;

    status = rig_get_dcd(rig, vfo, &dcd);

    if (status != RIG_OK)
    {
        return status;
    }

    if ((interactive && prompt) || (interactive && !prompt && ext_resp))
    {
        fprintf(fout, "%s: ", cmd->arg1);
    }

    fprintf(fout, "%d%c", dcd, resp_sep);

    return status;
}


/* 'R' */
declare_proto_rig(set_rptr_shift)
{
    rptr_shift_t rptr_shift;

    rptr_shift = rig_parse_rptr_shift(arg1);
    return rig_set_rptr_shift(rig, vfo, rptr_shift);
}


/* 'r' */
declare_proto_rig(get_rptr_shift)
{
    int status;
    rptr_shift_t rptr_shift;

    status = rig_get_rptr_shift(rig, vfo, &rptr_shift);

    if (status != RIG_OK)
    {
        return status;
    }

    if ((interactive && prompt) || (interactive && !prompt && ext_resp))
    {
        fprintf(fout, "%s: ", cmd->arg1);
    }

    fprintf(fout, "%s%c", rig_strptrshift(rptr_shift), resp_sep);

    return status;
}


/* 'O' */
declare_proto_rig(set_rptr_offs)
{
    unsigned long rptr_offs;

    CHKSCN1ARG(sscanf(arg1, "%lu", &rptr_offs));
    return rig_set_rptr_offs(rig, vfo, rptr_offs);
}


/* 'o' */
declare_proto_rig(get_rptr_offs)
{
    int status;
    shortfreq_t rptr_offs;

    status = rig_get_rptr_offs(rig, vfo, &rptr_offs);

    if (status != RIG_OK)
    {
        return status;
    }

    if ((interactive && prompt) || (interactive && !prompt && ext_resp))
    {
        fprintf(fout, "%s: ", cmd->arg1);
    }

    fprintf(fout, "%ld%c", rptr_offs, resp_sep);

    return status;
}


/* 'C' */
declare_proto_rig(set_ctcss_tone)
{
    tone_t tone;

    CHKSCN1ARG(sscanf(arg1, "%u", &tone));
    return rig_set_ctcss_tone(rig, vfo, tone);
}


/* 'c' */
declare_proto_rig(get_ctcss_tone)
{
    int status;
    tone_t tone;

    status = rig_get_ctcss_tone(rig, vfo, &tone);

    if (status != RIG_OK)
    {
        return status;
    }

    if ((interactive && prompt) || (interactive && !prompt && ext_resp))
    {
        fprintf(fout, "%s: ", cmd->arg1);
    }

    fprintf(fout, "%d%c", tone, resp_sep);

    return status;
}


/* 'D' */
declare_proto_rig(set_dcs_code)
{
    tone_t code;

    CHKSCN1ARG(sscanf(arg1, "%u", &code));
    return rig_set_dcs_code(rig, vfo, code);
}


/* 'd' */
declare_proto_rig(get_dcs_code)
{
    int status;
    tone_t code;

    status = rig_get_dcs_code(rig, vfo, &code);

    if (status != RIG_OK)
    {
        return status;
    }

    if ((interactive && prompt) || (interactive && !prompt && ext_resp))
    {
        fprintf(fout, "%s: ", cmd->arg1);
    }

    fprintf(fout, "%d%c", code, resp_sep);

    return status;
}


/* '0x90' */
declare_proto_rig(set_ctcss_sql)
{
    tone_t tone;

    CHKSCN1ARG(sscanf(arg1, "%u", &tone));
    return rig_set_ctcss_sql(rig, vfo, tone);
}


/* '0x91' */
declare_proto_rig(get_ctcss_sql)
{
    int status;
    tone_t tone;

    status = rig_get_ctcss_sql(rig, vfo, &tone);

    if (status != RIG_OK)
    {
        return status;
    }

    if ((interactive && prompt) || (interactive && !prompt && ext_resp))
    {
        fprintf(fout, "%s: ", cmd->arg1);
    }

    fprintf(fout, "%d%c", tone, resp_sep);

    return status;
}


/* '0x92' */
declare_proto_rig(set_dcs_sql)
{
    tone_t code;

    CHKSCN1ARG(sscanf(arg1, "%u", &code));
    return rig_set_dcs_sql(rig, vfo, code);
}


/* '0x93' */
declare_proto_rig(get_dcs_sql)
{
    int status;
    tone_t code;

    status = rig_get_dcs_sql(rig, vfo, &code);

    if (status != RIG_OK)
    {
        return status;
    }

    if ((interactive && prompt) || (interactive && !prompt && ext_resp))
    {
        fprintf(fout, "%s: ", cmd->arg1);
    }

    fprintf(fout, "%d%c", code, resp_sep);

    return status;
}


/* 'I' */
declare_proto_rig(set_split_freq)
{
    freq_t txfreq;
    vfo_t txvfo = RIG_VFO_TX;

    CHKSCN1ARG(sscanf(arg1, "%"SCNfreq, &txfreq));
    return rig_set_split_freq(rig, txvfo, txfreq);
}


/* 'i' */
declare_proto_rig(get_split_freq)
{
    int status;
    freq_t txfreq;
    vfo_t txvfo = RIG_VFO_TX;

    status = rig_get_split_freq(rig, txvfo, &txfreq);

    if (status != RIG_OK)
    {
        return status;
    }

    if ((interactive && prompt) || (interactive && !prompt && ext_resp))
    {
        fprintf(fout, "%s: ", cmd->arg1);
    }

    fprintf(fout, "%"PRIll"%c", (int64_t)txfreq, resp_sep);

    return status;
}


/* 'X' */
declare_proto_rig(set_split_mode)
{
    rmode_t mode;
    int  width;
    vfo_t txvfo = RIG_VFO_TX;

    if (!strcmp(arg1, "?"))
    {
        char s[SPRINTF_MAX_SIZE];
        sprintf_mode(s, rig->state.mode_list);
        fprintf(fout, "%s\n", s);
        return RIG_OK;
    }

    mode = rig_parse_mode(arg1);
    CHKSCN1ARG(sscanf(arg2, "%d", &width));
    return rig_set_split_mode(rig, txvfo, mode, (pbwidth_t) width);
}


/* 'x' */
declare_proto_rig(get_split_mode)
{
    int status;
    rmode_t mode;
    pbwidth_t width;
    vfo_t txvfo = RIG_VFO_TX;

    status = rig_get_split_mode(rig, txvfo, &mode, &width);

    if (status != RIG_OK)
    {
        return status;
    }

    if ((interactive && prompt) || (interactive && !prompt && ext_resp))
    {
        fprintf(fout, "%s: ", cmd->arg1);
    }

    fprintf(fout, "%s%c", rig_strrmode(mode), resp_sep);

    if ((interactive && prompt) || (interactive && !prompt && ext_resp))
    {
        fprintf(fout, "%s: ", cmd->arg2);
    }

    fprintf(fout, "%ld%c", width, resp_sep);

    return status;
}


/* 'K' */
declare_proto_rig(set_split_freq_mode)
{
    freq_t freq;
    rmode_t mode;
    int  width;
    vfo_t txvfo = RIG_VFO_TX;

    if (!strcmp(arg1, "?"))
    {
        char s[SPRINTF_MAX_SIZE];
        sprintf_mode(s, rig->state.mode_list);
        fprintf(fout, "%s\n", s);
        return RIG_OK;
    }

    CHKSCN1ARG(sscanf(arg1, "%"SCNfreq, &freq));
    mode = rig_parse_mode(arg2);
    CHKSCN1ARG(sscanf(arg3, "%d", &width));
    return rig_set_split_freq_mode(rig, txvfo, freq, mode, (pbwidth_t) width);
}


/* 'k' */
declare_proto_rig(get_split_freq_mode)
{
    int status;
    freq_t freq;
    rmode_t mode;
    pbwidth_t width;
    vfo_t txvfo = RIG_VFO_TX;

    status = rig_get_split_freq_mode(rig, txvfo, &freq, &mode, &width);

    if (status != RIG_OK)
    {
        return status;
    }

    if ((interactive && prompt) || (interactive && !prompt && ext_resp))
    {
        fprintf(fout, "%s: ", cmd->arg1);
    }

    fprintf(fout, "%"PRIll"%c", (int64_t)freq, resp_sep);

    if ((interactive && prompt) || (interactive && !prompt && ext_resp))
    {
        fprintf(fout, "%s: ", cmd->arg2);
    }

    fprintf(fout, "%s%c", rig_strrmode(mode), resp_sep);

    if ((interactive && prompt) || (interactive && !prompt && ext_resp))
    {
        fprintf(fout, "%s: ", cmd->arg3);
    }

    fprintf(fout, "%ld%c", width, resp_sep);

    return status;
}


/* 'S' */
declare_proto_rig(set_split_vfo)
{
    int split;
    vfo_t tx_vfo;

    CHKSCN1ARG(sscanf(arg1, "%d", &split));

    if (!strcmp(arg2, "?"))
    {
        char s[SPRINTF_MAX_SIZE];
        sprintf_vfo(s, rig->state.vfo_list);
        fprintf(fout, "%s\n", s);
        return RIG_OK;
    }

    tx_vfo = rig_parse_vfo(arg2);

    if (tx_vfo == RIG_VFO_NONE)
    {
        return -RIG_EINVAL;
    }

    return rig_set_split_vfo(rig, vfo, (split_t) split, tx_vfo);
}


/* 's' */
declare_proto_rig(get_split_vfo)
{
    int status;
    split_t split;
    vfo_t tx_vfo;

    status = rig_get_split_vfo(rig, vfo, &split, &tx_vfo);

    if (status != RIG_OK)
    {
        return status;
    }

    if ((interactive && prompt) || (interactive && !prompt && ext_resp))
    {
        fprintf(fout, "%s: ", cmd->arg1);
    }

    fprintf(fout, "%d%c", split, resp_sep);

    if ((interactive && prompt) || (interactive && !prompt && ext_resp))
    {
        fprintf(fout, "%s: ", cmd->arg2);
    }

    fprintf(fout, "%s%c", rig_strvfo(tx_vfo), resp_sep);

    return status;
}


/* 'N' */
declare_proto_rig(set_ts)
{
    unsigned long ts;

    CHKSCN1ARG(sscanf(arg1, "%lu", &ts));
    return rig_set_ts(rig, vfo, ts);
}


/* 'n' */
declare_proto_rig(get_ts)
{
    int status;
    shortfreq_t ts;

    status = rig_get_ts(rig, vfo, &ts);

    if (status != RIG_OK)
    {
        return status;
    }

    if ((interactive && prompt) || (interactive && !prompt && ext_resp))
    {
        fprintf(fout, "%s: ", cmd->arg1);
    }

    fprintf(fout, "%ld%c", ts, resp_sep);

    return status;
}


/* '2' */
declare_proto_rig(power2mW)
{
    int status;
    float power;
    freq_t freq;
    rmode_t mode;
    unsigned int mwp;

    CHKSCN1ARG(sscanf(arg1, "%f", &power));
    CHKSCN1ARG(sscanf(arg2, "%"SCNfreq, &freq));
    mode = rig_parse_mode(arg3);

    status = rig_power2mW(rig, &mwp, power, freq, mode);

    if (status != RIG_OK)
    {
        return status;
    }

    if ((interactive && prompt) || (interactive && !prompt && ext_resp))
    {
        fprintf(fout, "%s: ", cmd->arg4);
    }

    fprintf(fout, "%i%c", mwp, resp_sep);

    return status;
}


/* '4' */
declare_proto_rig(mW2power)
{
    int status;
    float power;
    freq_t freq;
    rmode_t mode;
    unsigned int mwp;

    CHKSCN1ARG(sscanf(arg1, "%u", &mwp));
    CHKSCN1ARG(sscanf(arg2, "%"SCNfreq, &freq));
    mode = rig_parse_mode(arg3);

    status = rig_mW2power(rig, &power, mwp, freq, mode);

    if (status != RIG_OK)
    {
        return status;
    }

    if ((interactive && prompt) || (interactive && !prompt && ext_resp))
    {
        fprintf(fout, "%s: ", cmd->arg4);
    }

    fprintf(fout, "%f%c", power, resp_sep);

    return status;
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
declare_proto_rig(set_level)
{
    setting_t level;
    value_t val;

    if (!strcmp(arg1, "?"))
    {
        char s[SPRINTF_MAX_SIZE];
        sprintf_level(s, rig->state.has_set_level);
        fputs(s, fout);

        if (rig->caps->set_ext_level)
        {
            sprintf_level_ext(s, rig->caps->extlevels);
            fputs(s, fout);
        }

        fputc('\n', fout);
        return RIG_OK;
    }

    level = rig_parse_level(arg1);

    if (!rig_has_set_level(rig, level))
    {
        const struct confparams *cfp;

        cfp = rig_ext_lookup(rig, arg1);

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

        return rig_set_ext_level(rig, vfo, cfp->token, val);
    }

    if (RIG_LEVEL_IS_FLOAT(level))
    {
        CHKSCN1ARG(sscanf(arg2, "%f", &val.f));
    }
    else
    {
        CHKSCN1ARG(sscanf(arg2, "%d", &val.i));
    }

    return rig_set_level(rig, vfo, level, val);
}


/* 'l' */
declare_proto_rig(get_level)
{
    int status;
    setting_t level;
    value_t val;

    if (!strcmp(arg1, "?"))
    {
        char s[SPRINTF_MAX_SIZE];
        sprintf_level(s, rig->state.has_get_level);
        fputs(s, fout);

        if (rig->caps->get_ext_level)
        {
            sprintf_level_ext(s, rig->caps->extlevels);
            fputs(s, fout);
        }

        fputc('\n', fout);
        return RIG_OK;
    }

    level = rig_parse_level(arg1);

    if (!rig_has_get_level(rig, level))
    {
        const struct confparams *cfp;

        cfp = rig_ext_lookup(rig, arg1);

        if (!cfp)
        {
            return -RIG_EINVAL;    /* no such parameter */
        }

        status = rig_get_ext_level(rig, vfo, cfp->token, &val);

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

    status = rig_get_level(rig, vfo, level, &val);

    if (status != RIG_OK)
    {
        return status;
    }

    if (interactive && prompt)
    {
        fprintf(fout, "%s: ", cmd->arg2);
    }

    if (RIG_LEVEL_IS_FLOAT(level))
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
declare_proto_rig(set_func)
{
    setting_t func;
    int func_stat;

    if (!strcmp(arg1, "?"))
    {
        char s[SPRINTF_MAX_SIZE];
        sprintf_func(s, rig->state.has_set_func);
        fprintf(fout, "%s\n", s);
        return RIG_OK;
    }

    func = rig_parse_func(arg1);

    if (!rig_has_set_func(rig, func))
    {
        const struct confparams *cfp;

        cfp = rig_ext_lookup(rig, arg1);

        if (!cfp)
        {
            return -RIG_ENAVAIL;    /* no such parameter */
        }

        CHKSCN1ARG(sscanf(arg2, "%d", &func_stat));

        return rig_set_ext_func(rig, vfo, cfp->token, func_stat);
    }

    CHKSCN1ARG(sscanf(arg2, "%d", &func_stat));
    return rig_set_func(rig, vfo, func, func_stat);
}


/* 'u' */
declare_proto_rig(get_func)
{
    int status;
    setting_t func;
    int func_stat;

    if (!strcmp(arg1, "?"))
    {
        char s[SPRINTF_MAX_SIZE];
        sprintf_func(s, rig->state.has_get_func);
        fprintf(fout, "%s\n", s);
        return RIG_OK;
    }

    func = rig_parse_func(arg1);

    if (!rig_has_get_func(rig, func))
    {
        const struct confparams *cfp;

        cfp = rig_ext_lookup(rig, arg1);

        if (!cfp)
        {
            return -RIG_EINVAL;    /* no such parameter */
        }

        status = rig_get_ext_func(rig, vfo, cfp->token, &func_stat);

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

    status = rig_get_func(rig, vfo, func, &func_stat);

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


/* 'P' */
declare_proto_rig(set_parm)
{
    setting_t parm;
    value_t val;

    if (!strcmp(arg1, "?"))
    {
        char s[SPRINTF_MAX_SIZE];
        sprintf_parm(s, rig->state.has_set_parm);
        fprintf(fout, "%s\n", s);
        return RIG_OK;
    }

    parm = rig_parse_parm(arg1);

    if (!rig_has_set_parm(rig, parm))
    {
        const struct confparams *cfp;

        cfp = rig_ext_lookup(rig, arg1);

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

        return rig_set_ext_parm(rig, cfp->token, val);
    }

    if (RIG_PARM_IS_FLOAT(parm))
    {
        CHKSCN1ARG(sscanf(arg2, "%f", &val.f));
    }
    else
    {
        CHKSCN1ARG(sscanf(arg2, "%d", &val.i));
    }

    return rig_set_parm(rig, parm, val);
}


/* 'p' */
declare_proto_rig(get_parm)
{
    int status;
    setting_t parm;
    value_t val;
    char buffer[RIG_BIN_MAX];

    if (!strcmp(arg1, "?"))
    {
        char s[SPRINTF_MAX_SIZE];
        sprintf_parm(s, rig->state.has_get_parm);
        fprintf(fout, "%s\n", s);
        return RIG_OK;
    }

    parm = rig_parse_parm(arg1);

    if (!rig_has_get_parm(rig, parm))
    {
        const struct confparams *cfp;

        cfp = rig_ext_lookup(rig, arg1);

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

        status = rig_get_ext_parm(rig, cfp->token, &val);

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

    status = rig_get_parm(rig, parm, &val);

    if (status != RIG_OK)
    {
        return status;
    }

    if (interactive && prompt)
    {
        fprintf(fout, "%s: ", cmd->arg2);
    }

    if (RIG_PARM_IS_FLOAT(parm))
    {
        fprintf(fout, "%f\n", val.f);
    }
    else
    {
        fprintf(fout, "%d\n", val.i);
    }

    return status;
}


/* 'B' */
declare_proto_rig(set_bank)
{
    int bank;

    CHKSCN1ARG(sscanf(arg1, "%d", &bank));
    return rig_set_bank(rig, vfo, bank);
}


/* 'E' */
declare_proto_rig(set_mem)
{
    int ch;

    CHKSCN1ARG(sscanf(arg1, "%d", &ch));
    return rig_set_mem(rig, vfo, ch);
}


/* 'e' */
declare_proto_rig(get_mem)
{
    int status;
    int ch;

    status = rig_get_mem(rig, vfo, &ch);

    if (status != RIG_OK)
    {
        return status;
    }

    if ((interactive && prompt) || (interactive && !prompt && ext_resp))
    {
        fprintf(fout, "%s: ", cmd->arg1);
    }

    fprintf(fout, "%d%c", ch, resp_sep);

    return status;
}


/* 'G' */
declare_proto_rig(vfo_op)
{
    vfo_op_t op;

    if (!strcmp(arg1, "?"))
    {
        char s[SPRINTF_MAX_SIZE];
        sprintf_vfop(s, rig->caps->vfo_ops);
        fprintf(fout, "%s\n", s);
        return RIG_OK;
    }

    op = rig_parse_vfo_op(arg1);

    if (RIG_OP_NONE == op)
    {
        return -RIG_EINVAL;
    }

    return rig_vfo_op(rig, vfo, op);
}


/* 'g' */
declare_proto_rig(scan)
{
    scan_t op;
    int ch;

    if (!strcmp(arg1, "?"))
    {
        char s[SPRINTF_MAX_SIZE];
        sprintf_scan(s, rig->caps->scan_ops);
        fprintf(fout, "%s\n", s);
        return RIG_OK;
    }

    op = rig_parse_scan(arg1);
    CHKSCN1ARG(sscanf(arg2, "%d", &ch));
    return rig_scan(rig, vfo, op, ch);
}


/* 'H' */
declare_proto_rig(set_channel)
{
    const channel_cap_t *mem_caps = NULL;
    const chan_t *chan_list;
    channel_t chan;
    int status;
    char s[16];

    memset(&chan, 0, sizeof(channel_t));

    if (isdigit((int)arg1[0]))
    {
        chan.vfo = RIG_VFO_MEM;
        CHKSCN1ARG(sscanf(arg1, "%d", &chan.channel_num));
        /*
         * find mem_caps in caps, we'll need it later
         */
        chan_list = rig_lookup_mem_caps(rig, chan.channel_num);

        if (chan_list)
        {
            mem_caps = &chan_list->mem_caps;
        }

    }
    else
    {
        chan.vfo = rig_parse_vfo(arg1);
        chan.channel_num = 0;

        /* TODO: mem_caps for VFO! */
        /*       either from mem, or reverse computed from caps */
    }

    if (!mem_caps)
    {
        return -RIG_ECONF;
    }

    if (mem_caps->bank_num)
    {
        if ((interactive && prompt) || (interactive && !prompt && ext_resp))
        {
            fprintf_flush(fout, "Bank Num: ");
        }

        CHKSCN1ARG(scanfc(fin, "%d", &chan.bank_num));
    }

#if 0

    if (mem_caps->vfo)
    {
        if ((interactive && prompt) || (interactive && !prompt && ext_resp))
        {
            fprintf_flush(fout, "vfo (VFOA,MEM,etc...): ");
        }

        CHKSCN1ARG(scanfc(fin, "%s", s));
        chan.vfo = rig_parse_vfo(s);
    }

#endif

    if (mem_caps->ant)
    {
        if ((interactive && prompt) || (interactive && !prompt && ext_resp))
        {
            fprintf_flush(fout, "ant: ");
        }

        CHKSCN1ARG(scanfc(fin, "%d", &chan.ant));
    }

    if (mem_caps->freq)
    {
        if ((interactive && prompt) || (interactive && !prompt && ext_resp))
        {
            fprintf_flush(fout, "Frequency: ");
        }

        CHKSCN1ARG(scanfc(fin, "%"SCNfreq, &chan.freq));
    }

    if (mem_caps->mode)
    {
        if ((interactive && prompt) || (interactive && !prompt && ext_resp))
        {
            fprintf_flush(fout, "mode (FM,LSB,etc...): ");
        }

        CHKSCN1ARG(scanfc(fin, "%s", s));
        chan.mode = rig_parse_mode(s);
    }

    if (mem_caps->width)
    {
        if ((interactive && prompt) || (interactive && !prompt && ext_resp))
        {
            fprintf_flush(fout, "width: ");
        }

        CHKSCN1ARG(scanfc(fin, "%ld", &chan.width));
    }

    if (mem_caps->tx_freq)
    {
        if ((interactive && prompt) || (interactive && !prompt && ext_resp))
        {
            fprintf_flush(fout, "tx freq: ");
        }

        CHKSCN1ARG(scanfc(fin, "%"SCNfreq, &chan.tx_freq));
    }

    if (mem_caps->tx_mode)
    {
        if ((interactive && prompt) || (interactive && !prompt && ext_resp))
        {
            fprintf_flush(fout, "tx mode (FM,LSB,etc...): ");
        }

        CHKSCN1ARG(scanfc(fin, "%s", s));
        chan.tx_mode = rig_parse_mode(s);
    }

    if (mem_caps->tx_width)
    {
        if ((interactive && prompt) || (interactive && !prompt && ext_resp))
        {
            fprintf_flush(fout, "tx width: ");
        }

        CHKSCN1ARG(scanfc(fin, "%ld", &chan.tx_width));
    }

    if (mem_caps->split)
    {
        if ((interactive && prompt) || (interactive && !prompt && ext_resp))
        {
            fprintf_flush(fout, "split (0,1): ");
        }

        CHKSCN1ARG(scanfc(fin, "%d", &status));
        chan.split = status;
    }

    if (mem_caps->tx_vfo)
    {
        if ((interactive && prompt) || (interactive && !prompt && ext_resp))
        {
            fprintf_flush(fout, "tx vfo (VFOA,MEM,etc...): ");
        }

        CHKSCN1ARG(scanfc(fin, "%s", s));
        chan.tx_vfo = rig_parse_vfo(s);
    }

    if (mem_caps->rptr_shift)
    {
        if ((interactive && prompt) || (interactive && !prompt && ext_resp))
        {
            fprintf_flush(fout, "rptr shift (+-0): ");
        }

        CHKSCN1ARG(scanfc(fin, "%s", s));
        chan.rptr_shift = rig_parse_rptr_shift(s);
    }

    if (mem_caps->rptr_offs)
    {
        if ((interactive && prompt) || (interactive && !prompt && ext_resp))
        {
            fprintf_flush(fout, "rptr offset: ");
        }

        CHKSCN1ARG(scanfc(fin, "%ld", &chan.rptr_offs));
    }

    if (mem_caps->tuning_step)
    {
        if ((interactive && prompt) || (interactive && !prompt && ext_resp))
        {
            fprintf_flush(fout, "tuning step: ");
        }

        CHKSCN1ARG(scanfc(fin, "%ld", &chan.tuning_step));
    }

    if (mem_caps->rit)
    {
        if ((interactive && prompt) || (interactive && !prompt && ext_resp))
        {
            fprintf_flush(fout, "rit (Hz,0=off): ");
        }

        CHKSCN1ARG(scanfc(fin, "%ld", &chan.rit));
    }

    if (mem_caps->xit)
    {
        if ((interactive && prompt) || (interactive && !prompt && ext_resp))
        {
            fprintf_flush(fout, "xit (Hz,0=off): ");
        }

        CHKSCN1ARG(scanfc(fin, "%ld", &chan.xit));
    }

    if (mem_caps->funcs)
    {
        if ((interactive && prompt) || (interactive && !prompt && ext_resp))
        {
            fprintf_flush(fout, "funcs: ");
        }

        CHKSCN1ARG(scanfc(fin, "%lx", &chan.funcs));
    }

#if 0

    /* for all levels (except READONLY), ask */
    if (mem_caps->levels)
    {
        sscanf(arg1, "%d", &chan.levels);
    }

#endif

    if (mem_caps->ctcss_tone)
    {
        if ((interactive && prompt) || (interactive && !prompt && ext_resp))
        {
            fprintf_flush(fout, "ctcss tone freq in tenth of Hz (0=off): ");
        }

        CHKSCN1ARG(scanfc(fin, "%d", &chan.ctcss_tone));
    }

    if (mem_caps->ctcss_sql)
    {
        if ((interactive && prompt) || (interactive && !prompt && ext_resp))
        {
            fprintf_flush(fout, "ctcss sql freq in tenth of Hz (0=off): ");
        }

        CHKSCN1ARG(scanfc(fin, "%d", &chan.ctcss_sql));
    }

    if (mem_caps->dcs_code)
    {
        if ((interactive && prompt) || (interactive && !prompt && ext_resp))
        {
            fprintf_flush(fout, "dcs code: ");
        }

        CHKSCN1ARG(scanfc(fin, "%d", &chan.dcs_code));
    }

    if (mem_caps->dcs_sql)
    {
        if ((interactive && prompt) || (interactive && !prompt && ext_resp))
        {
            fprintf_flush(fout, "dcs sql: ");
        }

        CHKSCN1ARG(scanfc(fin, "%d", &chan.dcs_sql));
    }

    if (mem_caps->scan_group)
    {
        if ((interactive && prompt) || (interactive && !prompt && ext_resp))
        {
            fprintf_flush(fout, "scan group: ");
        }

        CHKSCN1ARG(scanfc(fin, "%d", &chan.scan_group));
    }

    if (mem_caps->flags)
    {
        if ((interactive && prompt) || (interactive && !prompt && ext_resp))
        {
            fprintf_flush(fout, "flags: ");
        }

        CHKSCN1ARG(scanfc(fin, "%d", &chan.flags));
    }

    if (mem_caps->channel_desc)
    {
        if ((interactive && prompt) || (interactive && !prompt && ext_resp))
        {
            fprintf_flush(fout, "channel desc: ");
        }

        CHKSCN1ARG(scanfc(fin, "%s", s));
        strcpy(chan.channel_desc, s);
    }

#if 0

    /* TODO: same as levels, allocate/free the array */
    if (mem_caps->ext_levels)
    {
        sscanf(arg1, "%d", &chan.ext_levels[i].val.i);
    }

#endif

    status = rig_set_channel(rig, &chan);

    return status;
}


/* 'h' */
declare_proto_rig(get_channel)
{
    int status;
    int read_only = 0;
    channel_t chan;

    memset(&chan, 0, sizeof(channel_t));

    if (isdigit((int)arg1[0]))
    {
        chan.vfo = RIG_VFO_MEM;
        CHKSCN1ARG(sscanf(arg1, "%d", &chan.channel_num));
    }
    else
    {
        chan.vfo = rig_parse_vfo(arg1);
        chan.channel_num = 0;
    }

    CHKSCN1ARG(sscanf(arg2, "%d", &read_only));

    status = rig_get_channel(rig, &chan, read_only);

    if (status != RIG_OK)
    {
        return status;
    }

    status = dump_chan(fout, rig, &chan);

    if (chan.ext_levels)
    {
        free(chan.ext_levels);
    }

    return status;
}


static int myfreq_event(RIG *rig, vfo_t vfo, freq_t freq, rig_ptr_t arg)
{
    printf("Event: freq changed to %"PRIll"Hz on %s\n",
           (int64_t)freq,
           rig_strvfo(vfo));

    return 0;
}


static int mymode_event(RIG *rig,
                        vfo_t vfo,
                        rmode_t mode,
                        pbwidth_t width,
                        rig_ptr_t arg)
{
    printf("Event: mode changed to %s, width %liHz on %s\n",
           rig_strrmode(mode),
           width, rig_strvfo(vfo));

    return 0;
}


static int myvfo_event(RIG *rig, vfo_t vfo, rig_ptr_t arg)
{
    printf("Event: vfo changed to %s\n", rig_strvfo(vfo));
    return 0;
}


static int myptt_event(RIG *rig, vfo_t vfo, ptt_t ptt, rig_ptr_t arg)
{
    printf("Event: PTT changed to %i on %s\n", ptt, rig_strvfo(vfo));

    return 0;
}


static int mydcd_event(RIG *rig, vfo_t vfo, dcd_t dcd, rig_ptr_t arg)
{
    printf("Event: DCD changed to %i on %s\n", dcd, rig_strvfo(vfo));

    return 0;
}


/* 'A' */
declare_proto_rig(set_trn)
{
    int trn;

    if (!strcmp(arg1, "?"))
    {
        fprintf(fout, "OFF RIG POLL\n");
        return RIG_OK;
    }

    if (!strcmp(arg1, "OFF"))
    {
        trn = RIG_TRN_OFF;
    }
    else if (!strcmp(arg1, "RIG") || !strcmp(arg1, "ON"))
    {
        trn = RIG_TRN_RIG;
    }
    else if (!strcmp(arg1, "POLL"))
    {
        trn = RIG_TRN_POLL;
    }
    else
    {
        return -RIG_EINVAL;
    }

    if (trn != RIG_TRN_OFF)
    {
        rig_set_freq_callback(rig, myfreq_event, NULL);
        rig_set_mode_callback(rig, mymode_event, NULL);
        rig_set_vfo_callback(rig, myvfo_event, NULL);
        rig_set_ptt_callback(rig, myptt_event, NULL);
        rig_set_dcd_callback(rig, mydcd_event, NULL);
    }

    return rig_set_trn(rig, trn);
}


/* 'a' */
declare_proto_rig(get_trn)
{
    int status;
    int trn;
    static const char *trn_txt[] =
    {
        "OFF",
        "RIG",
        "POLL"
    };

    status = rig_get_trn(rig, &trn);

    if (status != RIG_OK)
    {
        return status;
    }

    if ((interactive && prompt) || (interactive && !prompt && ext_resp))
    {
        fprintf(fout, "%s: ", cmd->arg1);
    }

    if (trn >= 0 && trn <= 2)
    {
        fprintf(fout, "%s%c", trn_txt[trn], resp_sep);
    }

    return status;
}


/* '_' */
declare_proto_rig(get_info)
{
    const char *s;

    s = rig_get_info(rig);

    if ((interactive && prompt) || (interactive && !prompt && ext_resp))
    {
        fprintf(fout, "%s: ", cmd->arg1);
    }

    fprintf(fout, "%s%c", s ? s : "None", resp_sep);

    return RIG_OK;
}


int dump_chan(FILE *fout, RIG *rig, channel_t *chan)
{
    int idx, firstloop = 1;
    char freqbuf[16];
    char widthbuf[16];
    char prntbuf[256];

    fprintf(fout,
            "Channel: %d, Name: '%s'\n",
            chan->channel_num,
            chan->channel_desc);

    fprintf(fout,
            "VFO: %s, Antenna: %d, Split: %s\n",
            rig_strvfo(chan->vfo),
            chan->ant,
            chan->split == RIG_SPLIT_ON ? "ON" : "OFF");

    sprintf_freq(freqbuf, chan->freq);
    sprintf_freq(widthbuf, chan->width);
    fprintf(fout,
            "Freq:   %s\tMode:   %s\tWidth:   %s\n",
            freqbuf,
            rig_strrmode(chan->mode),
            widthbuf);

    sprintf_freq(freqbuf, chan->tx_freq);
    sprintf_freq(widthbuf, chan->tx_width);
    fprintf(fout,
            "txFreq: %s\ttxMode: %s\ttxWidth: %s\n",
            freqbuf,
            rig_strrmode(chan->tx_mode),
            widthbuf);

    sprintf_freq(freqbuf, chan->rptr_offs);
    fprintf(fout,
            "Shift: %s, Offset: %s%s, ",
            rig_strptrshift(chan->rptr_shift),
            chan->rptr_offs > 0 ? "+" : "",
            freqbuf);

    sprintf_freq(freqbuf, chan->tuning_step);
    fprintf(fout, "Step: %s, ", freqbuf);
    sprintf_freq(freqbuf, chan->rit);
    fprintf(fout, "RIT: %s%s, ", chan->rit > 0 ? "+" : "", freqbuf);
    sprintf_freq(freqbuf, chan->xit);
    fprintf(fout, "XIT: %s%s\n", chan->xit > 0 ? "+" : "", freqbuf);

    fprintf(fout, "CTCSS: %u.%uHz, ", chan->ctcss_tone / 10, chan->ctcss_tone % 10);
    fprintf(fout,
            "CTCSSsql: %u.%uHz, ",
            chan->ctcss_sql / 10,
            chan->ctcss_sql % 10);

    fprintf(fout, "DCS: %u.%u, ", chan->dcs_code / 10, chan->dcs_code % 10);
    fprintf(fout, "DCSsql: %u.%u\n", chan->dcs_sql / 10, chan->dcs_sql % 10);

    sprintf_func(prntbuf, chan->funcs);
    fprintf(fout, "Functions: %s\n", prntbuf);

    fprintf(fout, "Levels:");

    for (idx = 0; idx < RIG_SETTING_MAX; idx++)
    {
        setting_t level = rig_idx2setting(idx);
        const char *level_s;

        if (!RIG_LEVEL_SET(level)
                || (!rig_has_set_level(rig, level)
                    && !rig_has_get_level(rig, level)))
        {

            continue;
        }

        level_s = rig_strlevel(level);

        if (!level_s || level_s[0] == '\0')
        {
            continue;    /* duh! */
        }

        if (firstloop)
        {
            firstloop = 0;
        }
        else
        {
            fprintf(fout, ",\t");
        }

        if (RIG_LEVEL_IS_FLOAT(level))
        {
            fprintf(fout, " %s: %g%%", level_s, 100 * chan->levels[idx].f);
        }
        else
        {
            fprintf(fout, " %s: %d", level_s, chan->levels[idx].i);
        }
    }

    /* ext_levels */
    for (idx = 0; chan->ext_levels
            && !RIG_IS_EXT_END(chan->ext_levels[idx]); idx++)
    {
        const struct confparams *cfp;
        char lstr[32];

        cfp = rig_ext_lookup_tok(rig, chan->ext_levels[idx].token);

        if (!cfp)
        {
            return -RIG_EINVAL;
        }

        switch (cfp->type)
        {
        case RIG_CONF_STRING:
            strcpy(lstr, chan->ext_levels[idx].val.s);
            break;

        case RIG_CONF_COMBO:
            sprintf(lstr, "%d", chan->ext_levels[idx].val.i);
            break;

        case RIG_CONF_NUMERIC:
            sprintf(lstr, "%f", chan->ext_levels[idx].val.f);
            break;

        case RIG_CONF_CHECKBUTTON:
            sprintf(lstr, "%s", chan->ext_levels[idx].val.i ? "ON" : "OFF");
            break;

        case RIG_CONF_BUTTON:
            continue;

        default:
            return -RIG_EINTERNAL;
        }

        fprintf(fout, ",\t %s: %s", cfp->name, lstr);
    }

    fprintf(fout, "\n");

    return RIG_OK;
}


/* '1' */
declare_proto_rig(dump_caps)
{
    dumpcaps(rig, fout);

    return RIG_OK;
}


/* For rigctld internal use */
declare_proto_rig(dump_state)
{
    int i;
    struct rig_state *rs = &rig->state;

    /*
     * - Protocol version
     */
#define RIGCTLD_PROT_VER 1
    fprintf(fout, "%d\n", RIGCTLD_PROT_VER);
    fprintf(fout, "%d\n", rig->caps->rig_model);
#if 0 // deprecated -- not one rig uses this
    fprintf(fout, "%d\n", rs->itu_region);
#else  // need to print something to maintain backward compatibility
    fprintf(fout, "%d\n", 0);
#endif

    for (i = 0; i < FRQRANGESIZ && !RIG_IS_FRNG_END(rs->rx_range_list[i]); i++)
    {
        fprintf(fout,
                "%"FREQFMT" %"FREQFMT" 0x%"PRXll" %d %d 0x%x 0x%x\n",
                rs->rx_range_list[i].startf,
                rs->rx_range_list[i].endf,
                rs->rx_range_list[i].modes,
                rs->rx_range_list[i].low_power,
                rs->rx_range_list[i].high_power,
                rs->rx_range_list[i].vfo,
                rs->rx_range_list[i].ant);
    }

    fprintf(fout, "0 0 0 0 0 0 0\n");

    for (i = 0; i < FRQRANGESIZ && !RIG_IS_FRNG_END(rs->tx_range_list[i]); i++)
    {
        fprintf(fout,
                "%"FREQFMT" %"FREQFMT" 0x%"PRXll" %d %d 0x%x 0x%x\n",
                rs->tx_range_list[i].startf,
                rs->tx_range_list[i].endf,
                rs->tx_range_list[i].modes,
                rs->tx_range_list[i].low_power,
                rs->tx_range_list[i].high_power,
                rs->tx_range_list[i].vfo,
                rs->tx_range_list[i].ant);
    }

    fprintf(fout, "0 0 0 0 0 0 0\n");

    for (i = 0; i < TSLSTSIZ && !RIG_IS_TS_END(rs->tuning_steps[i]); i++)
    {
        fprintf(fout,
                "0x%"PRXll" %ld\n",
                rs->tuning_steps[i].modes,
                rs->tuning_steps[i].ts);
    }

    fprintf(fout, "0 0\n");

    for (i = 0; i < FLTLSTSIZ && !RIG_IS_FLT_END(rs->filters[i]); i++)
    {
        fprintf(fout,
                "0x%"PRXll" %ld\n",
                rs->filters[i].modes,
                rs->filters[i].width);
    }

    fprintf(fout, "0 0\n");

#if 0
    chan_t chan_list[CHANLSTSIZ]; /*!< Channel list, zero ended */
#endif

    fprintf(fout, "%ld\n", rs->max_rit);
    fprintf(fout, "%ld\n", rs->max_xit);
    fprintf(fout, "%ld\n", rs->max_ifshift);
    fprintf(fout, "%d\n", rs->announces);

    for (i = 0; i < MAXDBLSTSIZ && rs->preamp[i]; i++)
    {
        fprintf(fout, "%d ", rs->preamp[i]);
    }

    fprintf(fout, "\n");

    for (i = 0; i < MAXDBLSTSIZ && rs->attenuator[i]; i++)
    {
        fprintf(fout, "%d ", rs->attenuator[i]);
    }

    fprintf(fout, "\n");

    fprintf(fout, "0x%"PRXll"\n", rs->has_get_func);
    fprintf(fout, "0x%"PRXll"\n", rs->has_set_func);
    fprintf(fout, "0x%"PRXll"\n", rs->has_get_level);
    fprintf(fout, "0x%"PRXll"\n", rs->has_set_level);
    fprintf(fout, "0x%"PRXll"\n", rs->has_get_parm);
    fprintf(fout, "0x%"PRXll"\n", rs->has_set_parm);

    // protocol 1 fields are "setting=value"
    // protocol 1 allows fields can be listed/processed in any order
    // protocol 1 fields can be multi-line -- just write the thing to allow for it
    // backward compatible as new values will just generate warnings
    if (chk_vfo_executed) // for 3.3 compatiblility
    {
        fprintf(fout, "vfo_ops=0x%x\n", rig->caps->vfo_ops);
        fprintf(fout, "ptt_type=0x%x\n", rig->state.pttport.type.ptt);
        fprintf(fout, "targetable_vfo=0x%x\n", rig->caps->targetable_vfo);
        fprintf(fout, "done\n");
    }

#if 0 // why isn't this implemented?  Does anybody care?
    gran_t level_gran[RIG_SETTING_MAX];   /*!< level granularity */
    gran_t parm_gran[RIG_SETTING_MAX];  /*!< parm granularity */
#endif

    return RIG_OK;
}


/* '3' */
declare_proto_rig(dump_conf)
{
    dumpconf(rig, fout);

    return RIG_OK;
}


/* 'Y' */
declare_proto_rig(set_ant)
{
    ant_t ant;
    value_t option; // some rigs have a another option for the antenna

    CHKSCN1ARG(sscanf(arg1, "%d", &ant));
    CHKSCN1ARG(sscanf(arg2, "%d", &option.i)); // assuming they are integer values

    return rig_set_ant(rig, vfo, rig_idx2setting(ant - 1), option);
}


/* 'y' */
declare_proto_rig(get_ant)
{
    int status;
    ant_t ant, ant_curr, ant_tx, ant_rx;
    value_t option;
    char antbuf[32];

    CHKSCN1ARG(sscanf(arg1, "%d", &ant));

    if (ant == 0) // then we want the current antenna info
    {
        status = rig_get_ant(rig, vfo, RIG_ANT_CURR, &option, &ant_curr, &ant_tx,
                             &ant_rx);
    }
    else
    {
        status = rig_get_ant(rig, vfo, rig_idx2setting(ant - 1), &option, &ant_curr,
                             &ant_tx, &ant_rx);
    }

    if (status != RIG_OK)
    {
        return status;
    }

    if ((interactive && prompt) || (interactive && !prompt && ext_resp))
    {
        fprintf(fout, "%s: ", cmd->arg1);
    }

    sprintf_ant(antbuf, ant_curr);
    fprintf(fout, "%s%c", antbuf, resp_sep);
    //fprintf(fout, "%d%c", rig_setting2idx(ant_curr)+1, resp_sep);

    if ((interactive && prompt) || (interactive && !prompt && ext_resp))
    {
        fprintf(fout, "%s: ", cmd->arg2);
    }

    fprintf(fout, "%d%c", option.i, resp_sep);

    if ((interactive && prompt) || (interactive && !prompt && ext_resp))
    {
        fprintf(fout, "%s: ", cmd->arg3);
    }

    sprintf_ant(antbuf, ant_tx);
    fprintf(fout, "%s%c", antbuf, resp_sep);
    //fprintf(fout, "%d%c", rig_setting2idx(ant_tx)+1, resp_sep);

    if ((interactive && prompt) || (interactive && !prompt && ext_resp))
    {
        fprintf(fout, "%s: ", cmd->arg4);
    }

    sprintf_ant(antbuf, ant_rx);
    fprintf(fout, "%s%c", antbuf, resp_sep);
    //fprintf(fout, "%d%c", rig_setting2idx(ant_rx)+1, resp_sep);

    return status;
}


/* '*' */
declare_proto_rig(reset)
{
    int reset;

    CHKSCN1ARG(sscanf(arg1, "%d", &reset));
    return rig_reset(rig, (reset_t) reset);
}


/* 'b' */
declare_proto_rig(send_morse)
{
    return rig_send_morse(rig, vfo, arg1);
}

/* 0xvv */
declare_proto_rig(stop_morse)
{
    return rig_stop_morse(rig, vfo);
}

declare_proto_rig(wait_morse)
{
    return rig_wait_morse(rig, vfo);
}

/* '8' */
declare_proto_rig(send_voice_mem)
{
    int ch;

    CHKSCN1ARG(sscanf(arg1, "%d", &ch));
    return rig_send_voice_mem(rig, vfo, ch);
}

declare_proto_rig(send_dtmf)
{
    return rig_send_dtmf(rig, vfo, arg1);
}


declare_proto_rig(recv_dtmf)
{
    int status;
    int len;
    char digits[MAXARGSZ];

    len = MAXARGSZ - 1;
    status = rig_recv_dtmf(rig, vfo, digits, &len);

    if (status != RIG_OK)
    {
        return status;
    }

    if (interactive && prompt)
    {
        fprintf(fout, "%s: ", cmd->arg1);
    }

    fprintf(fout, "%s\n", digits);

    return status;
}


/* '0x87' */
declare_proto_rig(set_powerstat)
{
    int stat;

    CHKSCN1ARG(sscanf(arg1, "%d", &stat));
    return rig_set_powerstat(rig, (powerstat_t) stat);
}


/* '0x88' */
declare_proto_rig(get_powerstat)
{
    int status;
    powerstat_t stat;

    status = rig_get_powerstat(rig, &stat);

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

static int hasbinary(char *s, int len)
{
    int i;

    for (i = 0; i < len; ++i)
    {
        if (!isascii(s[i])) { return 1; }
    }

    return 0;
}

/*
 * Special debugging purpose send command display reply until there's a
 * timeout.
 *
 * 'w' and 'W'
 */
declare_proto_rig(send_cmd)
{
    int retval;
    struct rig_state *rs;
    int backend_num, cmd_len;
#define BUFSZ 512
    char bufcmd[BUFSZ * 5]; // allow for 5 chars for binary
    unsigned char buf[BUFSZ];
    char eom_buf[4] = { 0xa, 0xd, 0, 0 };
    int binary = 0;
    int rxbytes = BUFSZ;

    rig_debug(RIG_DEBUG_TRACE, "%s: called\n", __func__);

    /*
     * binary protocols enter values as \0xZZ\0xYY..
     */
    backend_num = RIG_BACKEND_NUM(rig->caps->rig_model);

    rig_debug(RIG_DEBUG_TRACE, "%s: backend_num=%d\n", __func__, backend_num);

    // need to move the eom_buf to rig-specifc backends
    // we'll let KENWOOD backends use the ; char in the rigctl commands
    if (backend_num == RIG_KENWOOD || backend_num == RIG_YAESU)
    {
        rig_debug(RIG_DEBUG_TRACE, "%s: KENWOOD\n", __func__);
        eom_buf[0] = 0;
        send_cmd_term = 0;
    }

    rig_debug(RIG_DEBUG_TRACE, "%s: arg1=%s\n", __func__, arg1);

    if (send_cmd_term == -1
            || backend_num == RIG_ICOM
            || backend_num == RIG_KACHINA
            || backend_num == RIG_MICROTUNE
            || (strstr(arg1, "\\0x") && (rig->caps->rig_model != RIG_MODEL_NETRIGCTL))
       )
    {

        const char *p = arg1, *pp = NULL;
        int i;
        rig_debug(RIG_DEBUG_TRACE, "%s: send_cmd_term==-1, arg1=%s\n", __func__, arg1);

        if (arg1[strlen(arg1) - 1] != ';' && strstr(arg1, "\\0x") == NULL)
        {
            rig_debug(RIG_DEBUG_ERR, "%s: expecting binary hex string here\n", __func__);
            return -RIG_EINVAL;
        }

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
    else if (rig->caps->rig_model == RIG_MODEL_NETRIGCTL)
    {
        rig_debug(RIG_DEBUG_TRACE, "%s: we're netrigctl#2\n", __func__);

        //snprintf(bufcmd, sizeof(bufcmd), "%s %s\n", cmd->cmd, arg1);
        if (cmd->cmd == 'w')
        {
            snprintf(bufcmd, sizeof(bufcmd), "%c %s\n", cmd->cmd, arg1);
        }
        else
        {
            int nbytes = 0;
            sscanf(arg2, "%d", &nbytes);
            snprintf(bufcmd, sizeof(bufcmd), "%c %s %d\n", cmd->cmd, arg1, nbytes);
        }

        cmd_len = strlen(bufcmd);
        rig_debug(RIG_DEBUG_VERBOSE, "%s: cmd=%s, len=%d\n", __func__, bufcmd, cmd_len);
    }
    else
    {
        char tmpbuf[64];
        /* text protocol */
        strncpy(bufcmd, arg1, BUFSZ - 1);
        strtok(bufcmd, "\0xa\0xd");
        bufcmd[BUFSZ - 2] = '\0';
        cmd_len = strlen(bufcmd);
        rig_debug(RIG_DEBUG_TRACE, "%s: bufcmd=%s\n", __func__, bufcmd);

        /* Automatic termination char */
        if (send_cmd_term != 0)
        {
            bufcmd[cmd_len++] = send_cmd_term;
        }

        eom_buf[2] = send_cmd_term;

        snprintf(tmpbuf, sizeof(tmpbuf), "0x%02x 0x%02x 0x%02x", eom_buf[0], eom_buf[1],
                 eom_buf[2]);

        rig_debug(RIG_DEBUG_TRACE, "%s: text protocol, send_cmd_term=%s\n", __func__,
                  tmpbuf);
    }

    rs = &rig->state;

    rig_flush(&rs->rigport);

    rig_debug(RIG_DEBUG_TRACE, "%s: rigport=%d, bufcmd=%s, cmd_len=%d\n", __func__,
              rs->rigport.fd, hasbinary(bufcmd, cmd_len) ? "BINARY" : bufcmd, cmd_len);
    retval = write_block(&rs->rigport, (char *)bufcmd, cmd_len);

    if (retval != RIG_OK)
    {
        return retval;
    }

    if ((interactive && prompt) || (interactive && !prompt && ext_resp))
    {
        rig_debug(RIG_DEBUG_TRACE, "%s: debug Cmd\n", __func__);
        fwrite(cmd->arg2, 1, strlen(cmd->arg2), fout); /* i.e. "Frequency" */
        fwrite(": ", 1, 2, fout); /* i.e. "Frequency" */
    }

    do
    {
        if (arg2) { sscanf(arg2, "%d", &rxbytes); }

        if (rxbytes > 0)
        {
            ++rxbytes;  // need length + 1 for end of string
            eom_buf[0] = 0;
        }

        /* Assumes CR or LF is end of line char for all ASCII protocols. */
        retval = read_string(&rs->rigport, (char *)buf, rxbytes, eom_buf,
                             strlen(eom_buf));

        if (retval < 0)
        {
            rig_debug(RIG_DEBUG_ERR, "%s: read_string error %s\n", __func__,
                      rigerror(retval));

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

        //if (rig->caps->rig_model != RIG_MODEL_NETRIGCTL)
        {
            // see if we have binary being returned
            binary = hasbinary((char *)buf, retval);
        }

        if (binary)   // convert our buf to a hex representation
        {
            int i;
            char hex[8];
            int hexbufbytes = retval * 6;
            char *hexbuf = calloc(hexbufbytes, 1);
            rig_debug(RIG_DEBUG_VERBOSE, "%s: sending binary, hexbuf size=%d\n", __func__,
                      hexbufbytes);
            hexbuf[0] = 0;

            for (i = 0; i < retval; ++i)
            {
                snprintf(hex, sizeof(hex), "\\0x%02X", (unsigned char)buf[i]);

                if ((strlen(hexbuf) + strlen(hex) + 1) >= hexbufbytes)
                {
                    hexbufbytes *= 2;
                    rig_debug(RIG_DEBUG_TRACE, "%s: doubling hexbuf size to %d\n", __func__,
                              hexbufbytes);
                    hexbuf = realloc(hexbuf, hexbufbytes);
                }

                strncat(hexbuf, hex, hexbufbytes - 1);
            }

            rig_debug(RIG_DEBUG_TRACE, "%s: binary=%s, retval=%d\n", __func__, hexbuf,
                      retval);
            fprintf(fout, "%s %d\n", hexbuf, retval);
            free(hexbuf);
            return RIG_OK;
        }
        else
        {
            // we should be in printable ASCII here
            fprintf(fout, "%s\n", buf);
        }
    }
    while (retval > 0 && rxbytes == BUFSZ);

// we use fwrite in case of any nulls in binary return
    if (binary) { fwrite(buf, 1, retval, fout); }

    if (binary)
    {
        fwrite("\n", 1, 1, fout);
    }


    if (retval > 0 || retval == -RIG_ETIMEOUT)
    {
        retval = RIG_OK;
    }

    return retval;
}


/* '0xf0'--test if rigctld called with -o|--vfo option */
declare_proto_rig(chk_vfo)
{
    if ((interactive && prompt) || (interactive && !prompt && ext_resp))
    {
        fprintf(fout, "%s: ", cmd->arg1);    /* i.e. "Frequency" */
    }

    fprintf(fout, "%d\n", rig->state.vfo_opt);

    chk_vfo_executed = 1; // this allows us to control dump_state version

    return RIG_OK;
}

/* '(' -- turn vfo option on */
declare_proto_rig(set_vfo_opt)
{
    int opt = 0;
    rig_debug(RIG_DEBUG_VERBOSE, "%s: called\n", __func__);
    CHKSCN1ARG(sscanf(arg1, "%d", &opt));
    *vfo_opt = rig->state.vfo_opt = opt;
    return rig_set_vfo_opt(rig, opt);
}

/* '0xf1'--halt rigctld daemon */
declare_proto_rig(halt)
{
    /* a bit rough, TODO: clean daemon shutdown */
    exit(0);

    return RIG_OK;
}


/* '0x8c'--pause processing */
declare_proto_rig(pause)
{
    unsigned seconds;
    CHKSCN1ARG(sscanf(arg1, "%u", &seconds));
    sleep(seconds);
    return RIG_OK;
}

/* '0x8d' */
declare_proto_rig(set_twiddle)
{
    int seconds;

    CHKSCN1ARG(sscanf(arg1, "%d", &seconds));
    return rig_set_twiddle(rig, seconds);
}

/* '0x97' */
declare_proto_rig(set_uplink)
{
    int val;

    CHKSCN1ARG(sscanf(arg1, "%d", &val));
    return rig_set_uplink(rig, val);
}



/* '0x8e' */
declare_proto_rig(get_twiddle)
{
    int status;
    int seconds;

    status = rig_get_twiddle(rig, &seconds);

    if (status != RIG_OK)
    {
        return status;
    }

    if ((interactive && prompt) || (interactive && !prompt && ext_resp))
    {
        fprintf(fout, "%s: ", cmd->arg1);
    }

    fprintf(fout, "%d\n", seconds);

    return status;
}

/* '0x95' */
declare_proto_rig(set_cache)
{
    int ms;

    CHKSCN1ARG(sscanf(arg1, "%d", &ms));
    return rig_set_cache_timeout_ms(rig, HAMLIB_CACHE_ALL, ms);
}


/* '0x96' */
declare_proto_rig(get_cache)
{
    int ms;

    ms = rig_get_cache_timeout_ms(rig, HAMLIB_CACHE_ALL);

    if ((interactive && prompt) || (interactive && !prompt && ext_resp))
    {
        fprintf(fout, "%s: ", cmd->arg1);
    }

    fprintf(fout, "%d\n", ms);

    return RIG_OK;
}
