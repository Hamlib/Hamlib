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

#include <hamlib/config.h>

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <unistd.h>
#include <ctype.h>
#include <errno.h>
#include <getopt.h>
#include <rig_tests.h>

// If true adds some debug statements to see flow of rigctl parsing
int debugflow = 0;

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
#include "riglist.h"
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
#define ARG_OUT5 0x100
#define ARG_IN_LINE 0x4000
#define ARG_NOVFO 0x8000

#define ARG_IN  (ARG_IN1|ARG_IN2|ARG_IN3|ARG_IN4)
#define ARG_OUT (ARG_OUT1|ARG_OUT2|ARG_OUT3|ARG_OUT4|ARG_OUT5)

static int chk_vfo_executed;
char rigctld_password[65];
int is_passwordOK;
int is_rigctld;
extern int lock_mode; // used by rigctld
extern powerstat_t rig_powerstat;




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
    const char *arg5;
    const char *arg6;
};


#define CHKSCN1ARG(a) if ((a) != 1) { rig_debug(RIG_DEBUG_ERR,"%s: chkarg err\n", __func__);RETURNFUNC2(-RIG_EINVAL);} else do {} while(0)

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
declare_proto_rig(get_modes);
declare_proto_rig(get_mode_bandwidths);
declare_proto_rig(set_vfo);
declare_proto_rig(get_vfo);
declare_proto_rig(get_rig_info);
declare_proto_rig(get_vfo_info);
declare_proto_rig(get_vfo_list);
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
declare_proto_rig(stop_voice_mem);
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
declare_proto_rig(password);
//declare_proto_rig(set_password);
declare_proto_rig(set_clock);
declare_proto_rig(get_clock);
declare_proto_rig(set_separator);
declare_proto_rig(get_separator);
declare_proto_rig(set_lock_mode);
declare_proto_rig(get_lock_mode);
declare_proto_rig(send_raw);
declare_proto_rig(client_version);
declare_proto_rig(hamlib_version);
declare_proto_rig(test);
declare_proto_rig(cm108_get_bit);
declare_proto_rig(cm108_set_bit);
declare_proto_rig(set_conf);
declare_proto_rig(get_conf);


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
    { 'F',  "set_freq",         ACTION(set_freq),       ARG_IN1 | ARG_OUT1, "Frequency" },
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
    { 'y',  "get_ant",          ACTION(get_ant),        ARG_IN1 | ARG_OUT2, "AntCurr", "Option", "AntTx", "AntRx" },
    { 0x87, "set_powerstat",    ACTION(set_powerstat),  ARG_IN  | ARG_NOVFO, "Power Status" },
    { 0x88, "get_powerstat",    ACTION(get_powerstat),  ARG_OUT | ARG_NOVFO, "Power Status" },
    { 0x89, "send_dtmf",        ACTION(send_dtmf),      ARG_IN | ARG_NOVFO, "Digits" },
    { 0x8a, "recv_dtmf",        ACTION(recv_dtmf),      ARG_OUT | ARG_NOVFO, "Digits" },
    { '*',  "reset",            ACTION(reset),          ARG_IN | ARG_NOVFO, "Reset" },
    { 'w',  "send_cmd",         ACTION(send_cmd),       ARG_IN1 | ARG_IN_LINE | ARG_OUT2 | ARG_NOVFO, "Cmd", "Reply" },
    { 'W',  "send_cmd_rx",      ACTION(send_cmd),       ARG_IN | ARG_OUT2 | ARG_NOVFO, "Cmd", "Reply"},
    { 'b',  "send_morse",       ACTION(send_morse),     ARG_IN | ARG_NOVFO  | ARG_IN_LINE, "Morse" },
    { 0xbb, "stop_morse",       ACTION(stop_morse),     ARG_NOVFO},
    { 0xbc, "wait_morse",       ACTION(wait_morse),     ARG_NOVFO},
    { 0x94, "send_voice_mem",   ACTION(send_voice_mem), ARG_NOVFO | ARG_IN, "Voice Mem#" },
    { 0xab, "stop_voice_mem",   ACTION(stop_voice_mem), ARG_NOVFO},
    { 0x8b, "get_dcd",          ACTION(get_dcd),        ARG_OUT | ARG_NOVFO, "DCD" },
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
    { 0xf3, "get_vfo_info",     ACTION(get_vfo_info),   ARG_IN1 | ARG_NOVFO | ARG_OUT5, "VFO", "Freq", "Mode", "Width", "Split", "SatMode" }, /* get several vfo parameters at once */
    { 0xf5, "get_rig_info",     ACTION(get_rig_info),   ARG_NOVFO | ARG_OUT, "RigInfo" }, /* get several vfo parameters at once */
    { 0xf4, "get_vfo_list",    ACTION(get_vfo_list),   ARG_OUT | ARG_NOVFO, "VFOs" },
    { 0xf6, "get_modes",       ACTION(get_modes),   ARG_OUT | ARG_NOVFO, "Modes" },
//    { 0xf9, "get_clock",        ACTION(get_clock),      ARG_IN | ARG_NOVFO, "local/utc" },
    { 0xf9, "get_clock",        ACTION(get_clock),      ARG_NOVFO },
    { 0xf8, "set_clock",        ACTION(set_clock),      ARG_IN | ARG_NOVFO, "local or utc or YYYY-MM-DDTHH:MM:SS.sss+ZZ or YYYY-MM-DDTHH:MM+ZZ" },
    { 0xf1, "halt",             ACTION(halt),           ARG_NOVFO },   /* rigctld only--halt the daemon */
    { 0x8c, "pause",            ACTION(pause),          ARG_IN | ARG_NOVFO, "Seconds" },
    { 0x98, "password",         ACTION(password),       ARG_IN | ARG_NOVFO, "Password" },
//    { 0x99, "set_password",     ACTION(set_password),   ARG_IN | ARG_NOVFO, "Password" },
    { 0xf7, "get_mode_bandwidths", ACTION(get_mode_bandwidths),   ARG_IN | ARG_NOVFO, "Mode" },
    { 0xa0, "set_separator",     ACTION(set_separator), ARG_IN | ARG_NOVFO, "Separator" },
    { 0xa1, "get_separator",     ACTION(get_separator), ARG_NOVFO, "Separator" },
    { 0xa2, "set_lock_mode",     ACTION(set_lock_mode), ARG_IN | ARG_NOVFO, "Locked" },
    { 0xa3, "get_lock_mode",     ACTION(get_lock_mode), ARG_NOVFO, "Locked" },
    { 0xa4, "send_raw",          ACTION(send_raw), ARG_NOVFO | ARG_IN1 | ARG_IN2 | ARG_OUT3, "Terminator", "Command", "Send raw answer" },
    { 0xa5, "client_version",    ACTION(client_version), ARG_NOVFO | ARG_IN1, "Version", "Client version" },
    { 0xa6, "get_vfo_list",    ACTION(get_vfo_list), ARG_NOVFO },
    { 0xa7, "test",    ACTION(test), ARG_NOVFO | ARG_IN, "routine" },
    { 0xa8, "hamlib_version",    ACTION(hamlib_version), ARG_NOVFO },
    { 0xa9, "get_gpio",    ACTION(cm108_get_bit), ARG_NOVFO | ARG_IN1 | ARG_OUT1, "GPIO#", "0/1" },
    { 0xaa, "set_gpio",    ACTION(cm108_set_bit), ARG_NOVFO | ARG_IN, "GPIO#", "0/1" },
    { 0xac, "set_conf",    ACTION(set_conf), ARG_NOVFO | ARG_IN, "Token", "Token Value" },
    { 0xad, "get_conf",    ACTION(get_conf), ARG_NOVFO | ARG_IN1 | ARG_OUT2, "Token", "Value"},
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


// modifies s to remove quotes
void strip_quotes(char *s)
{
    char *s2 = strdup(s);
    char *p;

    if (s[0] != '\"') { return; } // no quotes

    s2 = strdup(&s[1]);
    p = strrchr(s2, '"');

    if (p) { *p = 0; }

    strcpy(s, s2);
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
                rig_debug(RIG_DEBUG_TRACE, "%s fscanf of:", __func__);
                dump_hex((unsigned char *)p, strlen(p));
                rig_debug(RIG_DEBUG_TRACE, " failed with format '%s'\n", format);
                ret = 0x0a;
            }
        }

        if (ret < 1) { rig_debug(RIG_DEBUG_TRACE, "%s: ret=%d\n", __func__, ret); }

        if (errno == 22) { return  -22; }

        if (ferror(fin)) { rig_debug(RIG_DEBUG_ERR, "%s: errno=%d, %s\n", __func__, errno, strerror(errno)); }

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
                 int *ext_resp_ptr, char *resp_sep_ptr, int use_password)
{
    int retcode = -RIG_EINTERNAL;        /* generic return code from functions */
    unsigned char cmd;
    struct test_table *cmd_entry = NULL;
    struct rig_state *rs = STATE(my_rig);

    char command[MAXARGSZ + 1];
    char arg1[MAXARGSZ + 1], *p1 = NULL;
    char arg2[MAXARGSZ + 1], *p2 = NULL;
    char arg3[MAXARGSZ + 1], *p3 = NULL;
    vfo_t vfo = RIG_VFO_CURR;
    char client_version[32];

    rig_debug(RIG_DEBUG_TRACE, "%s: called, interactive=%d\n", __func__,
              interactive);

    /* cmd, internal, rigctld */
    if (!(interactive && prompt && have_rl))
    {

        if (interactive)
        {
            static int last_was_ret = 1;
            static int last_cmd;

            if (prompt)
            {
                fprintf_flush(fout, "\nRig command: ");
            }

            do
            {
                if ((retcode = scanfc(fin, "%c", &cmd)) < 1)
                {
                    if (last_cmd == 0)
                    {
                        rig_debug(RIG_DEBUG_WARN,
                                  "%s: nothing to scan#1? retcode=%d, last_cmd=[empty]\n",
                                  __func__,
                                  retcode);
                    }
                    else
                    {
                        rig_debug(RIG_DEBUG_WARN, "%s: nothing to scan#1? retcode=%d, last_cmd=%c\n",
                                  __func__,
                                  retcode, last_cmd);
                    }

                    return (RIGCTL_PARSE_ERROR);
                }

                if (cmd != 0xa && cmd != 0xd)
                {
                    rig_debug(RIG_DEBUG_TRACE, "%s: cmd=%c(%02x) handle=%d\n", __func__,
                              isprint(cmd) ? cmd : ' ', cmd, fileno(fin));
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
                        return (RIGCTL_PARSE_ERROR);
                    }
                }
                else if (cmd == '+' && prompt)
                {
                    return (RIG_OK);
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
                        return (RIGCTL_PARSE_ERROR);
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

                    return (RIG_OK);
                }

                /* command by name */
                if (cmd == '\\')
                {
                    char cmd_name[MAXNAMSIZ], *pcmd = cmd_name;

                    if (scanfc(fin, "%c", pcmd) < 1)
                    {
                        rig_debug(RIG_DEBUG_WARN, "%s: nothing to scan#4?\n", __func__);
                        return (RIGCTL_PARSE_ERROR);
                    }

                    retcode = fscanf(fin, "%s", ++pcmd);

                    if (retcode == 0 || retcode == EOF) { rig_debug(RIG_DEBUG_WARN, "%s: unable to scan %c\n", __func__, *(pcmd - 1)); }

                    while (*++pcmd);

                    *pcmd = '\0';
                    cmd = parse_arg((char *)cmd_name);
                    rig_debug(RIG_DEBUG_VERBOSE, "%s: cmd=%s handle=%d\n", __func__, cmd_name,
                              fileno(fin));
                    break;
                }

                //rig_debug(RIG_DEBUG_VERBOSE, "%s: cmd==0x%02x\n", __func__, cmd);

                if (cmd == 0x0a || cmd == 0x0d)
                {
                    if (last_was_ret)
                    {
                        if (prompt)
                        {
                            fprintf(fout, "? for help, q to quit.\n");
                            fprintf_flush(fout, "\nRig command: ");
                        }

                        return (RIG_OK);
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
                        return (RIGCTL_PARSE_ERROR);
                    }
                }

                return (RIG_OK);
            }

            rs->vfo_opt = *vfo_opt;

            if (cmd == 'Q' || cmd == 'q')
            {
                rig_debug(RIG_DEBUG_TRACE, "%s: quit returning NETRIGCTL_RET 0\n", __func__);

                if (interactive && !prompt) { fprintf(fout, "%s0\n", NETRIGCTL_RET); }

                fflush(fout);
                return (RIGCTL_PARSE_END);
            }

            if (cmd == '?')
            {
                usage_rig(fout);
                fflush(fout);
                return (RIG_OK);
            }
        }
        else
        {
            /* parse rest of command line */
            retcode = next_word(command, argc, argv, 1);

            if (EOF == retcode)
            {
                return (RIGCTL_PARSE_END);
            }
            else if (retcode < 0)
            {
                return (RIGCTL_PARSE_ERROR);
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

        if (strcmp(command,"skip-init")==0)
        {
            // no-op now since it's automatic in non-interactive mode
            return(RIG_OK);
        }

        cmd_entry = find_cmd_entry(cmd);

        if (!cmd_entry)
        {
            if (cmd == 0)
            {
                fprintf(stderr, "Command '%s' not found!\n", command);
            }
            else if (cmd != ' ')
            {
                fprintf(stderr, "Command '%c' not found!\n", cmd);
            }

            return (RIG_OK);
        }

        if (!(cmd_entry->flags & ARG_NOVFO) && *vfo_opt)
        {
            if (interactive)
            {
                arg1[0] = fgetc(fin);
                arg1[1] = 0;

                if (prompt && arg1[0] == 0x0a)
                {
                    fprintf_flush(fout, "VFO: ");
                }

                if (scanfc(fin, "%s", arg1) < 1)
                {
                    rig_debug(RIG_DEBUG_WARN, "%s: nothing to scan#7?\n", __func__);
                    return (RIGCTL_PARSE_ERROR);
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
                    return (RIGCTL_PARSE_ERROR);
                }

                vfo = rig_parse_vfo(arg1);
            }
        }

        if (debugflow) { rig_debug(RIG_DEBUG_TRACE, "%s: debug1\n", __func__); }

        if ((cmd_entry->flags & ARG_IN_LINE)
                && (cmd_entry->flags & ARG_IN1)
                && cmd_entry->arg1)
        {
            if (debugflow) { rig_debug(RIG_DEBUG_TRACE, "%s: debug2\n", __func__); }

            if (interactive)
            {
                char *nl;

                if (debugflow) { rig_debug(RIG_DEBUG_TRACE, "%s: debug2a\n", __func__); }


                if (fgets(arg1, MAXARGSZ, fin) == NULL)
                {
                    return (RIGCTL_PARSE_ERROR);
                }

                if (arg1[0] == 0xa)
                {
                    if (debugflow) { rig_debug(RIG_DEBUG_TRACE, "%s: debug2b\n", __func__); }

                    if (prompt)
                    {
                        fprintf_flush(fout, "%s: ", cmd_entry->arg1);
                    }

                    if (fgets(arg1, MAXARGSZ, fin) == NULL)
                    {
                        return (RIGCTL_PARSE_ERROR);
                    }
                }

                nl = strchr(arg1, 0xa);

                if (nl)
                {
                    *nl = '\0';    /* chomp */
                }

                /* skip a space arg if first arg...happens parsing rigctld commands */
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
                    return (RIGCTL_PARSE_END);
                }
                else if (retcode < 0)
                {
                    return (RIGCTL_PARSE_ERROR);
                }

                p1 = arg1;
            }
        }
        else if ((cmd_entry->flags & ARG_IN1) && cmd_entry->arg1)
        {
            if (debugflow) { rig_debug(RIG_DEBUG_TRACE, "%s: debug3\n", __func__); }

            if (interactive)
            {
                arg1[0] = fgetc(fin);
                arg1[1] = 0;

                if (debugflow) { rig_debug(RIG_DEBUG_TRACE, "%s: debug4 arg1=%c\n", __func__, arg1[0]); }

                if (prompt && arg1[0] == 0x0a)
                {
                    fprintf_flush(fout, "%s: ", cmd_entry->arg1);
                }

                if (scanfc(fin, "%s", arg1) < 1)
                {
                    rig_debug(RIG_DEBUG_WARN, "%s: nothing to scan#8?\n", __func__);
                    return (RIGCTL_PARSE_ERROR);
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
                    return (1);
                }
                else if (retcode < 0)
                {
                    return (RIGCTL_PARSE_ERROR);
                }

                p1 = arg1;
            }
        }

        if (debugflow) { rig_debug(RIG_DEBUG_TRACE, "%s: debug5\n", __func__); }

        if (p1
                && p1[0] != '?'
                && (cmd_entry->flags & ARG_IN2)
                && cmd_entry->arg2)
        {
            if (debugflow) { rig_debug(RIG_DEBUG_TRACE, "%s: debug6\n", __func__); }

            if (interactive)
            {
                if (debugflow) { rig_debug(RIG_DEBUG_TRACE, "%s: debug7\n", __func__); }

                arg2[0] = fgetc(fin);
                arg2[1] = 0;

                if (prompt && arg2[0]  == 0x0a)
                {
                    if (debugflow) { rig_debug(RIG_DEBUG_TRACE, "%s: debug8\n", __func__); }

                    fprintf_flush(fout, "%s: ", cmd_entry->arg2);
                }

                if (scanfc(fin, "%s", arg2) < 1)
                {
                    rig_debug(RIG_DEBUG_WARN, "%s: nothing to scan#9?\n", __func__);
                    return (RIGCTL_PARSE_ERROR);
                }

                p2 = arg2;
            }
            else
            {
                if (debugflow) { rig_debug(RIG_DEBUG_TRACE, "%s: debug9\n", __func__); }

                retcode = next_word(arg2, argc, argv, 0);

                if (EOF == retcode)
                {
                    fprintf(stderr, "Invalid arg for command '%s'\n",
                            cmd_entry->name);
                    return (RIGCTL_PARSE_END);
                }
                else if (retcode < 0)
                {
                    return (RIGCTL_PARSE_ERROR);
                }

                p2 = arg2;
            }
        }

        if (debugflow) { rig_debug(RIG_DEBUG_TRACE, "%s: debug10\n", __func__); }

        if (p1
                && p1[0] != '?'
                && (cmd_entry->flags & ARG_IN3)
                && cmd_entry->arg3)
        {
            if (debugflow) { rig_debug(RIG_DEBUG_TRACE, "%s: debug11\n", __func__); }

            if (interactive)
            {
                if (debugflow) { rig_debug(RIG_DEBUG_TRACE, "%s: debug12\n", __func__); }

                if (prompt)
                {
                    if (debugflow) { rig_debug(RIG_DEBUG_TRACE, "%s: debug13\n", __func__); }

                    fprintf_flush(fout, "%s: ", cmd_entry->arg3);
                }

                if (scanfc(fin, "%s", arg3) < 1)
                {
                    rig_debug(RIG_DEBUG_WARN, "%s: nothing to scan#10?\n", __func__);
                    return (RIGCTL_PARSE_ERROR);
                }

                p3 = arg3;
            }
            else
            {
                if (debugflow) { rig_debug(RIG_DEBUG_TRACE, "%s: debug14\n", __func__); }

                retcode = next_word(arg3, argc, argv, 0);

                if (EOF == retcode)
                {
                    fprintf(stderr,
                            "Invalid arg for command '%s'\n",
                            cmd_entry->name);
                    return (RIGCTL_PARSE_END);
                }
                else if (retcode < 0)
                {
                    return (RIGCTL_PARSE_ERROR);
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
            free(rp_hist_buf);
            return (RIGCTL_PARSE_END);
        }

        /* Q or q to quit */
        if (!(strncasecmp(input_line, "q", 1)))
        {
            free(rp_hist_buf);
            return (RIGCTL_PARSE_END);
        }

        /* '?' for help */
        if (!(strncmp(input_line, "?", 1)))
        {
            usage_rig(fout);
            fflush(fout);
            free(rp_hist_buf);
            return (RIG_OK);
        }

        /* '#' for comment */
        if (!(strncmp(input_line, "#", 1)))
        {
            free(rp_hist_buf);
            return (RIG_OK);
        }

        /* Blank line entered */
        if (!(strcmp(input_line, "")))
        {
            fprintf(fout, "? for help, q to quit.\n");
            fflush(fout);
            free(rp_hist_buf);
            return (RIG_OK);
        }

        rig_debug(RIG_DEBUG_TRACE, "%s: input_line: %s\n", __func__, input_line);

        /* Split input_line on any number of spaces to get the command token
         * Tabs are intercepted by readline for completion and a newline
         * causes readline to return the typed text.  If more than one
         * argument is given, it will be parsed out later.
         */
        result = strtok(input_line, " ");
readline_repeat:

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
            free(rp_hist_buf);
            return (RIGCTL_PARSE_END);
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
                    free(rp_hist_buf);
                    return (RIG_OK);
                }
            }

            cmd = parse_arg(cmd_name);
        }
        /* Single '\' entered, prompt again */
        else if ((*parsed_input[0] == '\\') && (strlen(parsed_input[0]) == 1))
        {
            free(rp_hist_buf);
            return (RIG_OK);
        }
        /* Multiple characters but no leading '\' */
        else
        {
            fprintf(stderr, "Precede multiple character command names with '\\'\n");
            free(rp_hist_buf);
            return (RIG_OK);
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

            free(rp_hist_buf);
            return (RIG_OK);
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
                    free(rp_hist_buf);
                    return (RIGCTL_PARSE_END);
                }

                /* Blank line entered */
                if (!(strcmp(input_line, "")))
                {
                    fprintf(fout, "? for help, q to quit.\n");
                    fflush(fout);
                    free(rp_hist_buf);
                    return (RIG_OK);
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
                    free(rp_hist_buf);
                    return (RIGCTL_PARSE_END);
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
                    free(rp_hist_buf);
                    return (RIG_OK);
                }

                if (input_line)
                {
                    parsed_input[x] = input_line;
                }
                else
                {
                    fprintf_flush(fout, "\n");
                    free(rp_hist_buf);
                    return (RIGCTL_PARSE_END);
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

                if (!input_line || !(strcmp(input_line, "")))
                {
                    fprintf(fout, "? for help, q to quit.\n");
                    fflush(fout);
                    free(rp_hist_buf);
                    return (RIG_OK);
                }

                result = strtok(input_line, " ");

                if (result)
                {
                    parsed_input[x] = result;
                }
                else
                {
                    fprintf_flush(fout, "\n");
                    free(rp_hist_buf);
                    return (RIGCTL_PARSE_END);
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

                if (!input_line || !(strcmp(input_line, "")))
                {
                    fprintf(fout, "? for help, q to quit.\n");
                    fflush(fout);
                    free(rp_hist_buf);
                    return (RIG_OK);
                }

                result = strtok(input_line, " ");

                if (result)
                {
                    parsed_input[x] = result;
                }
                else
                {
                    fprintf_flush(fout, "\n");
                    free(rp_hist_buf);
                    return (RIGCTL_PARSE_END);
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
                    free(rp_hist_buf);
                    return (RIG_OK);
                }

                result = strtok(input_line, " ");

                if (result)
                {
                    parsed_input[x] = result;
                }
                else
                {
                    fprintf_flush(fout, "\n");
                    free(rp_hist_buf);
                    return (RIGCTL_PARSE_END);
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
    if (p1) { strip_quotes(p1); }

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

        // exception for get_vfo_info cmd which fails with log4om otherwise
        if (*vfo_opt && cmd != 0xf3)
        {
            p1 == NULL ? a1[0] = '\0' : snprintf(a1, sizeof(a1), ":%s", p1);
        }
        else
        {
            p1 == NULL ? a1[0] = '\0' : snprintf(a1, sizeof(a1), " %s", p1);
        }

        p2 == NULL ? a2[0] = '\0' : snprintf(a2, sizeof(a2), " %s", p2);
        p3 == NULL ? a3[0] = '\0' : snprintf(a3, sizeof(a3), " %s", p3);

        if (cmd == 'b') { strtok(a1, "\r\n"); }

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

    if (rs->comm_state == 0)
    {
        rig_debug(RIG_DEBUG_WARN, "%s: %p rig not open...trying to reopen\n", __func__,
                  &rs->comm_state);
        rig_open(my_rig);
    }

    // chk_vfo is the one command we'll allow without a password
    // since it's in the initial handshake
    int preCmd =
        0;  // some command are allowed without password to satisfy rigctld initialization from rigctl -m 2

    if (cmd_entry->arg1 != NULL)
    {
        if (strcmp(cmd_entry->arg1, "ChkVFO") == 0) { preCmd = 1; }
        else if (strcmp(cmd_entry->arg1, "VFO") == 0) { preCmd = 1; }
        else if (strcmp(cmd_entry->arg1, "Password") == 0) { preCmd = 1; }
    }

    if (use_password && !is_passwordOK && (cmd_entry->arg1 != NULL) && !preCmd)
    {
        rig_debug(RIG_DEBUG_ERR, "%s: password has not been provided\n", __func__);
        fflush(fin);
        retcode = -RIG_ESECURITY;
    }

    else
    {
        // Allow only certain commands when the rig is powered off
        if (rs->powerstat == RIG_POWER_OFF && (rig_powerstat == RIG_POWER_OFF
                                               || rig_powerstat == RIG_POWER_STANDBY)
                && cmd_entry->cmd != '1' // dump_caps
                && cmd_entry->cmd != '3' // dump_conf
                && cmd_entry->cmd != 0x8f // dump_state
                && cmd_entry->cmd != 0xf0 // chk_vfo
                && cmd_entry->cmd != 0x87 // set_powerstat
                && cmd_entry->cmd != 0x88 // get_powerstat
                && cmd_entry->cmd != 0xa5 // client_version
                && cmd_entry->cmd != 0xf2 // set_vfo_opt
                && my_rig->caps->rig_model !=
                RIG_MODEL_POWERSDR) // some rigs can do stuff when powered off
        {
            rig_debug(RIG_DEBUG_WARN,
                      "%s: command %s not allowed when rig is powered off\n", __func__,
                      cmd_entry->name);
            retcode = -RIG_EPOWER;
        }
        else
        {
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
        }

        // we need to copy client_version to our thread in case there are multiple client versions
        // client_version is used to determine any backward compatibility requirements or problems
        strncpy(client_version, rs->client_version, sizeof(client_version));
    }


    if (retcode == -RIG_EIO)
    {
        rig_debug(RIG_DEBUG_ERR, "%s: RIG_EIO?\n", __func__);

        if (sync_cb) { sync_cb(0); }    /* unlock if necessary */

        return (retcode);
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
                    //"%s: error = %s\n",
                    //cmd_entry->name,
                    "error = %s\n",
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

    if (*resp_sep_ptr != '\n') { fprintf(fout, "\n"); }

    fflush(fout);

#ifdef HAVE_LIBREADLINE

    if (input_line != NULL && (result = strtok(NULL, " "))) { goto readline_repeat; }

#endif

    if (sync_cb) { sync_cb(0); }    /* unlock if necessary */

    return (retcode);
}


declare_proto_rig(hamlib_version)
{
    fprintf(fout, "rigctl(d), %s\n\n", hamlib_version2);
    fprintf(fout, "%s\n", hamlib_copyright);
    return RIG_OK;
}

declare_proto_rig(client_version)
{
    struct rig_state *rs = STATE(rig);

    if ((interactive && prompt) || (interactive && !prompt && ext_resp))
    {
        fprintf(fout, "%s: ", cmd->arg1);
    }

    fprintf(fout, "%s%c", arg1, resp_sep);
    strncpy(rs->client_version, arg1, sizeof(rs->client_version) - 1);
    rig_debug(RIG_DEBUG_VERBOSE, "%s: client_version=%s\n", __func__,
              rs->client_version);
    return RIG_OK;
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
    case RIG_CONF_INT:
        printf("\tRange: %.0f..%.0f, step %.0f\n",
               cfp->u.n.min,
               cfp->u.n.max,
               cfp->u.n.step);
        break;

    case RIG_CONF_NUMERIC:
        printf("\tRange: %g..%g, step %.1f\n",
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

// short list for rigctl/rigctld display
int print_conf_list2(const struct confparams *cfp, rig_ptr_t data)
{
    RIG *rig = (RIG *) data;
    char buf[128] = "";

    rig_get_conf(rig, cfp->token, buf);
    fprintf(stdout, "%s: \"%s\"\n" "\t" "Default: %s, Value: %s\n",
            cfp->name,
            cfp->tooltip,
            cfp->dflt,
            buf);

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

        if (strcmp(s->mfg_name, "Misc") == 0
                || strcmp(s->mfg_name, "Other") == 0
                || strcmp(s->mfg_name, "Dummy") == 0)
        {
            printf("Do not use %s as mfg_name\n", s->mfg_name);
            exit(0);
        }

        if (strcmp(s->model_name, "Misc") == 0
                || strcmp(s->model_name, "Other") == 0)
        {
            printf("Do not use %s as model_name\n", s->model_name);
            exit(0);
        }
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

    ENTERFUNC2;

    CHKSCN1ARG(sscanf(arg1, "%"SCNfreq, &freq));
    retval = rig_set_freq(rig, vfo, freq);

    if (retval == RIG_OK)
    {
        //fprintf(fout, "%s%c", rig_strvfo(vfo), resp_sep);
        //fprintf(fout, fmt, (int64_t)freq, resp_sep);

    }

    RETURNFUNC2(retval);
}


/* 'f' */
declare_proto_rig(get_freq)
{
    int status;
    freq_t freq;
    // cppcheck-suppress *
    char *fmt = "%"PRIll"%c";

    ENTERFUNC2;

    status = rig_get_freq(rig, vfo, &freq);

    if (status != RIG_OK)
    {
        RETURNFUNC2(status);
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

    RETURNFUNC2(status);
}


/* 'J' */
declare_proto_rig(set_rit)
{
    shortfreq_t rit;

    ENTERFUNC2;

    CHKSCN1ARG(sscanf(arg1, "%ld", &rit));

    RETURNFUNC2(rig_set_rit(rig, vfo, rit));
}


/* 'j' */
declare_proto_rig(get_rit)
{
    int status;
    shortfreq_t rit;

    ENTERFUNC2;

    status = rig_get_rit(rig, vfo, &rit);

    if (status != RIG_OK)
    {
        RETURNFUNC2(status);
    }

    if ((interactive && prompt) || (interactive && !prompt && ext_resp))
    {
        fprintf(fout, "%s: ", cmd->arg1);
    }

    fprintf(fout, "%ld%c", rit, resp_sep);

    RETURNFUNC2(status);
}


/* 'Z' */
declare_proto_rig(set_xit)
{
    shortfreq_t xit;

    ENTERFUNC2;

    CHKSCN1ARG(sscanf(arg1, "%ld", &xit));

    RETURNFUNC2(rig_set_xit(rig, vfo, xit));
}


/* 'z' */
declare_proto_rig(get_xit)
{
    int status;
    shortfreq_t xit;

    ENTERFUNC2;

    status = rig_get_xit(rig, vfo, &xit);

    if (status != RIG_OK)
    {
        RETURNFUNC2(status);
    }

    if ((interactive && prompt) || (interactive && !prompt && ext_resp))
    {
        fprintf(fout, "%s: ", cmd->arg1);
    }

    fprintf(fout, "%ld%c", xit, resp_sep);

    RETURNFUNC2(status);
}


/* 'M' */
declare_proto_rig(set_mode)
{
    rmode_t mode;
    pbwidth_t width;
    struct rig_state *rs = STATE(rig);

    ENTERFUNC2;

    if (rs->lock_mode || lock_mode) { RETURNFUNC2(RIG_OK); }

    if (!strcmp(arg1, "?"))
    {
        char s[SPRINTF_MAX_SIZE];
        rig_sprintf_mode(s, sizeof(s), rs->mode_list);
        fprintf(fout, "%s\n", s);
        RETURNFUNC2(RIG_OK);
    }

    mode = rig_parse_mode(arg1);
    CHKSCN1ARG(sscanf(arg2, "%ld", &width));

    if (rs->lock_mode) { RETURNFUNC2(RIG_OK); }

    RETURNFUNC2(rig_set_mode(rig, vfo, mode, width));
}


/* 'm' */
declare_proto_rig(get_mode)
{
    int status;
    rmode_t mode;
    pbwidth_t width;

    ENTERFUNC2;

    status = rig_get_mode(rig, vfo, &mode, &width);

    if (status != RIG_OK)
    {
        RETURNFUNC2(status);
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

    RETURNFUNC2(status);
}


/* 'V' */
declare_proto_rig(set_vfo)
{
    int retval;

    ENTERFUNC2;

    if (!strcmp(arg1, "?"))
    {
        char s[SPRINTF_MAX_SIZE];
        rig_sprintf_vfo(s, sizeof(s), STATE(rig)->vfo_list);
        fprintf(fout, "%s\n", s);
        RETURNFUNC2(RIG_OK);
    }

    vfo = rig_parse_vfo(arg1);

    if (vfo == RIG_VFO_NONE)
    {
        int c;

        while ((c = fgetc(fin)) != '\n' && c != '\r' && c > 0);

        return -RIG_EINVAL;
    }

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

    if (retval != RIG_OK)
    {
        rig_debug(RIG_DEBUG_ERR, "%s: set_vfo(%s) failed, requested %s\n", __func__,
                  rig_strvfo(vfo), arg1);
    }

    RETURNFUNC2(retval);
}


/* 'v' */
declare_proto_rig(get_vfo)
{
    int status;

    ENTERFUNC2;

    status = rig_get_vfo(rig, &vfo);

    if (status != RIG_OK)
    {
        RETURNFUNC2(status);
    }

    if ((interactive && prompt) || (interactive && !prompt && ext_resp))
    {
        fprintf(fout, "%s: ", cmd->arg1);
    }

    fprintf(fout, "%s%c", rig_strvfo(vfo), resp_sep);

    RETURNFUNC2(status);
}

declare_proto_rig(get_rig_info)
{
    char buf[1024]; // big enough to last numerous years hopefully
    int ret;
    ENTERFUNC2;
    ret = rig_get_rig_info(rig, buf, sizeof(buf));

    if (ret != RIG_OK) { RETURNFUNC2(ret); }

    fprintf(fout, "%s\n", buf);
    RETURNFUNC2(RIG_OK);
}

/* '\get_vfo_info' */
declare_proto_rig(get_vfo_info)
{
    int retval;

    ENTERFUNC2;
    ELAPSED1;

    if (!strcmp(arg1, "?"))
    {
        char s[SPRINTF_MAX_SIZE];
        rig_sprintf_vfo(s, sizeof(s), STATE(rig)->vfo_list);
        fprintf(fout, "%s\n", s);
        RETURNFUNC2(RIG_OK);
    }

    vfo = rig_parse_vfo(arg1);
    freq_t freq = 0;
    rmode_t mode = RIG_MODE_NONE;
    pbwidth_t width = 0;
    split_t split;
    int satmode = 0;
    retval = rig_get_vfo_info(rig, vfo, &freq, &mode, &width, &split, &satmode);

    if (retval != RIG_OK)
    {
        rig_debug(RIG_DEBUG_ERR, "%s: vfo=%s\n", __func__, rig_strvfo(vfo));
    }

    const char *modestr = rig_strrmode(mode);

    if (strlen(modestr) == 0) { modestr = "None"; }

    if ((interactive && prompt) || (interactive && !prompt && ext_resp))
    {
        fprintf(fout, "%s: %.0f%c", cmd->arg2, freq, resp_sep);
        fprintf(fout, "%s: %s%c", cmd->arg3, modestr, resp_sep);
        fprintf(fout, "%s: %d%c", cmd->arg4, (int)width, resp_sep);
        fprintf(fout, "%s: %d%c", cmd->arg5, (int)split, resp_sep);
        fprintf(fout, "%s: %d%c", cmd->arg6, (int)satmode, resp_sep);
    }
    else
    {
        fprintf(fout, "%.0f%c%s%c%d%c%d%c%d\n", freq, resp_sep, modestr, resp_sep,
                (int)width, resp_sep, (int)split, resp_sep,
                (int)satmode);
    }

    ELAPSED2;
    RETURNFUNC2(retval);
}

/* '\get_vfo_list' */
declare_proto_rig(get_vfo_list)
{
    static char prntbuf[256];

    ENTERFUNC2;

    rig_sprintf_vfo(prntbuf, sizeof(prntbuf), STATE(rig)->vfo_list);

    if ((interactive && prompt) || (interactive && !prompt && ext_resp))
    {
        fprintf(fout, "%s: ", cmd->arg1);
    }

    fprintf(fout, "%s%c\n", prntbuf[0] ? prntbuf : "None", ext_resp);

    RETURNFUNC2(RIG_OK);
}

/* '\test' */
declare_proto_rig(test)
{
    ENTERFUNC2;

    if (!strcmp(arg1, "?"))
    {
        fprintf(fout, "cw\n");
        RETURNFUNC2(RIG_OK);
    }

    if (strcmp(arg1, "cw") == 0) { rig_test_cw(rig); }

    RETURNFUNC2(RIG_OK);
}

/* '\get_modes' */
declare_proto_rig(get_modes)
{
    static char prntbuf[1024];
    int i;
    char freqbuf[32];

    ENTERFUNC2;

    rig_strrmodes(STATE(rig)->mode_list, prntbuf, sizeof(prntbuf));

    if ((interactive && prompt) || (interactive && !prompt && ext_resp))
    {
        fprintf(fout, "%s: ", cmd->arg1);
    }

    fprintf(fout, "%s%c", prntbuf[0] ? prntbuf : "None", ext_resp);

    fprintf(fout, "\nBandwidths:");

    for (i = 1; i < RIG_MODE_TESTS_MAX; i <<= 1)
    {
        pbwidth_t pbnorm = rig_passband_normal(rig, i);

        if (pbnorm == 0)
        {
            continue;
        }

        sprintf_freq(freqbuf, sizeof(freqbuf), pbnorm);
        fprintf(fout, "\n\t%s\tNormal: %s,\t", rig_strrmode(i), freqbuf);

        sprintf_freq(freqbuf, sizeof(freqbuf), rig_passband_narrow(rig, i));
        fprintf(fout, "Narrow: %s,\t", freqbuf);

        sprintf_freq(freqbuf, sizeof(freqbuf), rig_passband_wide(rig, i));
        fprintf(fout, "Wide: %s", freqbuf);
    }


    RETURNFUNC2(RIG_OK);
}

declare_proto_rig(get_mode_bandwidths)
{
    int i;
    char freqbuf[32];

    ENTERFUNC2;

    rmode_t mode = rig_parse_mode(arg1);

    for (i = 1; i < RIG_MODE_TESTS_MAX; i <<= 1)
    {
        if (i != mode) { continue; }

        if (mode == RIG_MODE_CWR) { mode = RIG_MODE_CW; }

        if (mode == RIG_MODE_RTTYR) { mode = RIG_MODE_RTTY; }

        pbwidth_t pbnorm = rig_passband_normal(rig, i);

        if (pbnorm == 0)
        {
            continue;
        }

//        sprintf_freq(freqbuf, sizeof(freqbuf), pbnorm);
        SNPRINTF(freqbuf, sizeof(freqbuf), "%ldHz", pbnorm);
        fprintf(fout, "Mode=%s\n", rig_strrmode(i));
        fprintf(fout, "Normal=%s\n", freqbuf);

        SNPRINTF(freqbuf, sizeof(freqbuf), "%ldHz", rig_passband_narrow(rig, i));
        fprintf(fout, "Narrow=%s\n", freqbuf);

        SNPRINTF(freqbuf, sizeof(freqbuf), "%ldHz", rig_passband_wide(rig, i));
        fprintf(fout, "Wide=%s", freqbuf);
    }


    RETURNFUNC2(RIG_OK);
}




/* 'T' */
declare_proto_rig(set_ptt)
{
    int   scr;
    ptt_t ptt;

    ENTERFUNC2;

    CHKSCN1ARG(sscanf(arg1, "%d", &scr));
    ptt = scr;
    rig_debug(RIG_DEBUG_VERBOSE, "%s: set_ptt ptt=%d\n", __func__, ptt);

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

        // No longer map this -- is confusing rigctld and MICDATA rigs
        // https://github.com/Hamlib/Hamlib/issues/998
#if 0

        // map to a legal value
        if (rig->caps->ptt_type != RIG_PTT_RIG_MICDATA)
        {
            rig_debug(RIG_DEBUG_ERR, "%s: pttport.type.ptt=%d\n", __func__,
                      PTTPORT(rig)->type.ptt);
            ptt = RIG_PTT_ON;
        }

#endif

        break;

    case RIG_PTT_ON:
    case RIG_PTT_OFF:
        // nothing to do
        break;

    default:
        // this case is not handled in hamlib, but we guard against
        // illegal parameters here. The hamlib behaviour is to switch
        // on PTT whenever ptt != RIG_PTT_OFF.
        RETURNFUNC2(-RIG_EINVAL);
    }

    rig_debug(RIG_DEBUG_ERR, "%s: ptt=%d\n", __func__, ptt);
    RETURNFUNC2(rig_set_ptt(rig, vfo, ptt));
}


/* 't' */
declare_proto_rig(get_ptt)
{
    int status;
    ptt_t ptt = 0;

    ENTERFUNC2;

    status = rig_get_ptt(rig, vfo, &ptt);

    if (status != RIG_OK)
    {
        RETURNFUNC2(status);
    }

    if ((interactive && prompt) || (interactive && !prompt && ext_resp))
    {
        fprintf(fout, "%s: ", cmd->arg1);
    }

    /* TODO MICDATA */
    fprintf(fout, "%d%c", ptt, resp_sep);

    RETURNFUNC2(status);
}


/* '0x8b' */
declare_proto_rig(get_dcd)
{
    int status;
    dcd_t dcd;

    ENTERFUNC2;

    status = rig_get_dcd(rig, vfo, &dcd);

    if (status != RIG_OK)
    {
        RETURNFUNC2(status);
    }

    if ((interactive && prompt) || (interactive && !prompt && ext_resp))
    {
        fprintf(fout, "%s: ", cmd->arg1);
    }

    fprintf(fout, "%d%c", dcd, resp_sep);

    RETURNFUNC2(status);
}


/* 'R' */
declare_proto_rig(set_rptr_shift)
{
    rptr_shift_t rptr_shift;

    ENTERFUNC2;

    rptr_shift = rig_parse_rptr_shift(arg1);

    RETURNFUNC2(rig_set_rptr_shift(rig, vfo, rptr_shift));
}


/* 'r' */
declare_proto_rig(get_rptr_shift)
{
    int status;
    rptr_shift_t rptr_shift;

    ENTERFUNC2;

    status = rig_get_rptr_shift(rig, vfo, &rptr_shift);

    if (status != RIG_OK)
    {
        RETURNFUNC2(status);
    }

    if ((interactive && prompt) || (interactive && !prompt && ext_resp))
    {
        fprintf(fout, "%s: ", cmd->arg1);
    }

    fprintf(fout, "%s%c", rig_strptrshift(rptr_shift), resp_sep);

    RETURNFUNC2(status);
}


/* 'O' */
declare_proto_rig(set_rptr_offs)
{
    unsigned long rptr_offs;

    ENTERFUNC2;

    CHKSCN1ARG(sscanf(arg1, "%lu", &rptr_offs));

    RETURNFUNC2(rig_set_rptr_offs(rig, vfo, rptr_offs));
}


/* 'o' */
declare_proto_rig(get_rptr_offs)
{
    int status;
    shortfreq_t rptr_offs;

    ENTERFUNC2;

    status = rig_get_rptr_offs(rig, vfo, &rptr_offs);

    if (status != RIG_OK)
    {
        RETURNFUNC2(status);
    }

    if ((interactive && prompt) || (interactive && !prompt && ext_resp))
    {
        fprintf(fout, "%s: ", cmd->arg1);
    }

    fprintf(fout, "%ld%c", rptr_offs, resp_sep);

    RETURNFUNC2(status);
}


/* 'C' */
declare_proto_rig(set_ctcss_tone)
{
    tone_t tone;

    ENTERFUNC2;

    CHKSCN1ARG(sscanf(arg1, "%u", &tone));

    RETURNFUNC2(rig_set_ctcss_tone(rig, vfo, tone));
}


/* 'c' */
declare_proto_rig(get_ctcss_tone)
{
    int status;
    tone_t tone;

    ENTERFUNC2;

    status = rig_get_ctcss_tone(rig, vfo, &tone);

    if (status != RIG_OK)
    {
        RETURNFUNC2(status);
    }

    if ((interactive && prompt) || (interactive && !prompt && ext_resp))
    {
        fprintf(fout, "%s: ", cmd->arg1);
    }

    fprintf(fout, "%d%c", tone, resp_sep);

    RETURNFUNC2(status);
}


/* 'D' */
declare_proto_rig(set_dcs_code)
{
    tone_t code;

    ENTERFUNC2;

    CHKSCN1ARG(sscanf(arg1, "%u", &code));

    RETURNFUNC2(rig_set_dcs_code(rig, vfo, code));
}


/* 'd' */
declare_proto_rig(get_dcs_code)
{
    int status;
    tone_t code;

    ENTERFUNC2;

    status = rig_get_dcs_code(rig, vfo, &code);

    if (status != RIG_OK)
    {
        RETURNFUNC2(status);
    }

    if ((interactive && prompt) || (interactive && !prompt && ext_resp))
    {
        fprintf(fout, "%s: ", cmd->arg1);
    }

    fprintf(fout, "%d%c", code, resp_sep);

    RETURNFUNC2(status);
}


/* '0x90' */
declare_proto_rig(set_ctcss_sql)
{
    tone_t tone;

    ENTERFUNC2;

    CHKSCN1ARG(sscanf(arg1, "%u", &tone));

    RETURNFUNC2(rig_set_ctcss_sql(rig, vfo, tone));
}


/* '0x91' */
declare_proto_rig(get_ctcss_sql)
{
    int status;
    tone_t tone;

    ENTERFUNC2;

    status = rig_get_ctcss_sql(rig, vfo, &tone);

    if (status != RIG_OK)
    {
        RETURNFUNC2(status);
    }

    if ((interactive && prompt) || (interactive && !prompt && ext_resp))
    {
        fprintf(fout, "%s: ", cmd->arg1);
    }

    fprintf(fout, "%d%c", tone, resp_sep);

    RETURNFUNC2(status);
}


/* '0x92' */
declare_proto_rig(set_dcs_sql)
{
    tone_t code;

    ENTERFUNC2;

    CHKSCN1ARG(sscanf(arg1, "%u", &code));

    RETURNFUNC2(rig_set_dcs_sql(rig, vfo, code));
}


/* '0x93' */
declare_proto_rig(get_dcs_sql)
{
    int status;
    tone_t code;

    ENTERFUNC2;

    status = rig_get_dcs_sql(rig, vfo, &code);

    if (status != RIG_OK)
    {
        RETURNFUNC2(status);
    }

    if ((interactive && prompt) || (interactive && !prompt && ext_resp))
    {
        fprintf(fout, "%s: ", cmd->arg1);
    }

    fprintf(fout, "%d%c", code, resp_sep);

    RETURNFUNC2(status);
}


/* 'I' */
declare_proto_rig(set_split_freq)
{
    freq_t txfreq;
    vfo_t txvfo = RIG_VFO_TX;

    ENTERFUNC2;

    CHKSCN1ARG(sscanf(arg1, "%"SCNfreq, &txfreq));

    RETURNFUNC2(rig_set_split_freq(rig, txvfo, txfreq));
}


/* 'i' */
declare_proto_rig(get_split_freq)
{
    int status;
    freq_t txfreq;
    vfo_t txvfo = RIG_VFO_TX;

    ENTERFUNC2;

    status = rig_get_split_freq(rig, txvfo, &txfreq);

    if (status != RIG_OK)
    {
        RETURNFUNC2(status);
    }

    if ((interactive && prompt) || (interactive && !prompt && ext_resp))
    {
        fprintf(fout, "%s: ", cmd->arg1);
    }

    fprintf(fout, "%"PRIll"%c", (int64_t)txfreq, resp_sep);

    RETURNFUNC2(status);
}


/* 'X' */
declare_proto_rig(set_split_mode)
{
    rmode_t mode;
    int  width;
    vfo_t txvfo = RIG_VFO_TX;

    ENTERFUNC2;

    if (!strcmp(arg1, "?"))
    {
        char s[SPRINTF_MAX_SIZE];
        rig_sprintf_mode(s, sizeof(s), STATE(rig)->mode_list);
        fprintf(fout, "%s\n", s);
        RETURNFUNC2(RIG_OK);
    }

    // mode could be RIG_MODE_NONE here
    // we treat it as non-fatal
    // rig_parse_mode will spit out error msg
    mode = rig_parse_mode(arg1);
    CHKSCN1ARG(sscanf(arg2, "%d", &width));
    RETURNFUNC2(rig_set_split_mode(rig, txvfo, mode, (pbwidth_t) width));
}


/* 'x' */
declare_proto_rig(get_split_mode)
{
    int status;
    rmode_t mode;
    pbwidth_t width;
    vfo_t txvfo = RIG_VFO_TX;

    ENTERFUNC2;

    status = rig_get_split_mode(rig, txvfo, &mode, &width);

    if (status != RIG_OK)
    {
        RETURNFUNC2(status);
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

    RETURNFUNC2(status);
}


/* 'K' */
declare_proto_rig(set_split_freq_mode)
{
    freq_t freq;
    rmode_t mode;
    int  width;
    vfo_t txvfo = RIG_VFO_TX;

    ENTERFUNC2;

    if (!strcmp(arg1, "?"))
    {
        char s[SPRINTF_MAX_SIZE];
        rig_sprintf_mode(s, sizeof(s), STATE(rig)->mode_list);
        fprintf(fout, "%s\n", s);
        RETURNFUNC2(RIG_OK);
    }

    CHKSCN1ARG(sscanf(arg1, "%"SCNfreq, &freq));
    mode = rig_parse_mode(arg2);
    CHKSCN1ARG(sscanf(arg3, "%d", &width));
    RETURNFUNC2(rig_set_split_freq_mode(rig, txvfo, freq, mode, (pbwidth_t) width));
}


/* 'k' */
declare_proto_rig(get_split_freq_mode)
{
    int status;
    freq_t freq;
    rmode_t mode;
    pbwidth_t width;
    vfo_t txvfo = RIG_VFO_TX;

    ENTERFUNC2;

    status = rig_get_split_freq_mode(rig, txvfo, &freq, &mode, &width);

    if (status != RIG_OK)
    {
        RETURNFUNC2(status);
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

    RETURNFUNC2(status);
}


/* 'S' */
declare_proto_rig(set_split_vfo)
{
    int split;
    vfo_t tx_vfo;

    ENTERFUNC2;

    CHKSCN1ARG(sscanf(arg1, "%d", &split));

    if (!strcmp(arg2, "?"))
    {
        char s[SPRINTF_MAX_SIZE];
        rig_sprintf_vfo(s, sizeof(s), STATE(rig)->vfo_list);
        fprintf(fout, "%s\n", s);
        RETURNFUNC2(RIG_OK);
    }

    tx_vfo = rig_parse_vfo(arg2);

    if (tx_vfo == RIG_VFO_NONE)
    {
        RETURNFUNC2(-RIG_EINVAL);
    }

    rig_debug(RIG_DEBUG_VERBOSE, "%s(%d): rx_vfo = %s, tx_vfo = %s\n", __func__,
              __LINE__, rig_strvfo(vfo), rig_strvfo(tx_vfo));

    RETURNFUNC2(rig_set_split_vfo(rig, vfo, (split_t) split, tx_vfo));
}


/* 's' */
declare_proto_rig(get_split_vfo)
{
    int status;
    split_t split;
    vfo_t tx_vfo;

    ENTERFUNC2;

    status = rig_get_split_vfo(rig, vfo, &split, &tx_vfo);

    if (status != RIG_OK)
    {
        RETURNFUNC2(status);
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

    RETURNFUNC2(status);
}


/* 'N' */
declare_proto_rig(set_ts)
{
    unsigned long ts;
    char s[SPRINTF_MAX_SIZE];

    ENTERFUNC2;

    if (!strcmp(arg1, "?"))
    {
        rig_sprintf_tuning_steps(s, sizeof(s), rig->caps->tuning_steps);
        fprintf(fout, "%s\n", s);
        RETURNFUNC2(RIG_OK);
    }

    CHKSCN1ARG(sscanf(arg1, "%lu", &ts));

    RETURNFUNC2(rig_set_ts(rig, vfo, ts));
}


/* 'n' */
declare_proto_rig(get_ts)
{
    int status;
    shortfreq_t ts;

    ENTERFUNC2;

    status = rig_get_ts(rig, vfo, &ts);

    if (status != RIG_OK)
    {
        RETURNFUNC2(status);
    }

    if ((interactive && prompt) || (interactive && !prompt && ext_resp))
    {
        fprintf(fout, "%s: ", cmd->arg1);
    }

    fprintf(fout, "%ld%c", ts, resp_sep);

    RETURNFUNC2(status);
}


/* '2' */
declare_proto_rig(power2mW)
{
    int status;
    float power;
    freq_t freq;
    rmode_t mode;
    unsigned int mwp;

    ENTERFUNC2;

    CHKSCN1ARG(sscanf(arg1, "%f", &power));
    CHKSCN1ARG(sscanf(arg2, "%"SCNfreq, &freq));
    mode = rig_parse_mode(arg3);

    status = rig_power2mW(rig, &mwp, power, freq, mode);

    if (status != RIG_OK)
    {
        RETURNFUNC2(status);
    }

    if ((interactive && prompt) || (interactive && !prompt && ext_resp))
    {
        fprintf(fout, "%s: ", cmd->arg4);
    }

    fprintf(fout, "%i%c", mwp, resp_sep);

    RETURNFUNC2(status);
}


/* '4' */
declare_proto_rig(mW2power)
{
    int status;
    float power;
    freq_t freq;
    rmode_t mode;
    unsigned int mwp;

    ENTERFUNC2;

    CHKSCN1ARG(sscanf(arg1, "%u", &mwp));
    CHKSCN1ARG(sscanf(arg2, "%"SCNfreq, &freq));
    mode = rig_parse_mode(arg3);

    status = rig_mW2power(rig, &power, mwp, freq, mode);

    if (status != RIG_OK)
    {
        RETURNFUNC2(status);
    }

    if ((interactive && prompt) || (interactive && !prompt && ext_resp))
    {
        fprintf(fout, "%s: ", cmd->arg4);
    }

    fprintf(fout, "%f%c", power, resp_sep);

    RETURNFUNC2(status);
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

    ENTERFUNC2;

    if (!strcmp(arg1, "?"))
    {
        char s[SPRINTF_MAX_SIZE];
        rig_sprintf_level(s, sizeof(s), STATE(rig)->has_set_level);
        fputs(s, fout);

        if (rig->caps->set_ext_level)
        {
            sprintf_level_ext(s, sizeof(s), rig->caps->extlevels);
            fputs(s, fout);
        }

        fputc('\n', fout);
        RETURNFUNC2(RIG_OK);
    }

    level = rig_parse_level(arg1);

    if ((!strcmp(arg2, "?") || arg2[0] == 0) && level == RIG_LEVEL_METER)
    {
        fprintf(fout, "COMP ALC SWR ID/IC VDD DB PO TEMP%c", resp_sep);
        RETURNFUNC2(RIG_OK);
    }


    // some Java apps send comma in international setups so substitute period
    char *p = strchr(arg2, ',');

    if (p) { *p = '.'; }

    if (!rig_has_set_level(rig, level))
    {
        const struct confparams *cfp;

        cfp = rig_ext_lookup(rig, arg1);

        if (!cfp)
        {
            RETURNFUNC2(-RIG_ENAVAIL);    /* no such parameter */
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

        case RIG_CONF_INT:
            CHKSCN1ARG(sscanf(arg2, "%f", &val.f));
            break;

        case RIG_CONF_NUMERIC:
            CHKSCN1ARG(sscanf(arg2, "%g", &val.f));
            break;

        case RIG_CONF_STRING:
            val.cs = arg2;
            break;

        default:
            RETURNFUNC2(-RIG_ECONF);
        }

        RETURNFUNC2(rig_set_ext_level(rig, vfo, cfp->token, val));
    }

    int dummy;

    if (level == RIG_LEVEL_METER && sscanf(arg2, "%d", &dummy) <= 0)
    {
        if (strcmp(arg2, "COMP") == 0) { arg2 = "2"; }
        else if (strcmp(arg2, "ALC") == 0) { arg2 = "4"; }
        else if (strcmp(arg2, "SWR") == 0) { arg2 = "1"; }
        else if (strcmp(arg2, "ID") == 0 || strcmp(arg2, "IC") == 0) { arg2 = "8"; }
        else if (strcmp(arg2, "VDD") == 0) { arg2 = "64"; }
        else if (strcmp(arg2, "DB") == 0) { arg2 = "16"; }
        else if (strcmp(arg2, "PO") == 0) { arg2 = "32"; }
        else if (strcmp(arg2, "TEMP") == 0) { arg2 = "128"; }
        else
        {
            rig_debug(RIG_DEBUG_ERR,
                      "%s: unknown meter=%s, only know COMP,ALC,SWR,ID/IC,VDD,DB,PO,TEMP\n", __func__,
                      arg2);
            RETURNFUNC2(-RIG_EINVAL);
        }
    }

    if (RIG_LEVEL_IS_FLOAT(level))
    {
        CHKSCN1ARG(sscanf(arg2, "%f", &val.f));
    }
    else
    {
        CHKSCN1ARG(sscanf(arg2, "%d", &val.i));
    }

    RETURNFUNC2(rig_set_level(rig, vfo, level, val));
}


/* 'l' */
declare_proto_rig(get_level)
{
    int status;
    setting_t level;
    value_t val;

    ENTERFUNC2;

    if (!strcmp(arg1, "?"))
    {
        char s[SPRINTF_MAX_SIZE];
        rig_sprintf_level(s, sizeof(s), STATE(rig)->has_get_level);
        fputs(s, fout);

        if (rig->caps->get_ext_level)
        {
            sprintf_level_ext(s, sizeof(s), rig->caps->extlevels);
            fprintf(fout, "%s%c", s, resp_sep);
        }

        fputc('\n', fout);
        RETURNFUNC2(RIG_OK);
    }

    level = rig_parse_level(arg1);

    if (!rig_has_get_level(rig, level))
    {
        const struct confparams *cfp;

        cfp = rig_ext_lookup(rig, arg1);

        if (!cfp)
        {
            rig_debug(RIG_DEBUG_ERR, "%s: level not found=%s\n", __func__, arg1);
            RETURNFUNC2(-RIG_EINVAL);    /* no such parameter */
        }

        status = rig_get_ext_level(rig, vfo, cfp->token, &val);

        if (status != RIG_OK)
        {
            RETURNFUNC2(status);
        }

        if (interactive && prompt)
        {
            fprintf(fout, "%s: ", cmd->arg2);
        }

        switch (cfp->type)
        {
        case RIG_CONF_BUTTON:
            /* there's no sense in retrieving value of stateless button */
            RETURNFUNC2(-RIG_EINVAL);

        case RIG_CONF_CHECKBUTTON:
        case RIG_CONF_COMBO:
            fprintf(fout, "%d%c", val.i, resp_sep);
            break;

        case RIG_CONF_NUMERIC:
            fprintf(fout, "%g%c", val.f, resp_sep);
            break;

        case RIG_CONF_INT:
            fprintf(fout, "%.0f%c", val.f, resp_sep);
            break;

        case RIG_CONF_STRING:
            fprintf(fout, "%s%c", val.s, resp_sep);
            break;

        default:
            RETURNFUNC2(-RIG_ECONF);
        }

        RETURNFUNC2(status);
    }

    status = rig_get_level(rig, vfo, level, &val);

    if (status != RIG_OK)
    {
        RETURNFUNC2(status);
    }

    if (interactive && prompt)
    {
        fprintf(fout, "%s: ", cmd->arg2);
    }

    if (level == RIG_LEVEL_METER && interactive && prompt)
    {
        // we will show text answers as they make morse sense for rigtl
        switch (val.i)
        {
        case RIG_METER_COMP: fprintf(fout, "%d=%s%c", val.i, "COMP", resp_sep); break;

        case RIG_METER_ALC: fprintf(fout, "%d=%s%c", val.i, "ALC", resp_sep); break;

        case RIG_METER_SWR: fprintf(fout, "%d=%s%c", val.i, "SWR", resp_sep); break;

        case RIG_METER_IC: fprintf(fout, "%d=%s%c", val.i, "IC", resp_sep); break;

        case RIG_METER_VDD: fprintf(fout, "%d=%s%c", val.i, "VDD", resp_sep); break;

        case RIG_METER_DB: fprintf(fout, "%d=%s%c", val.i, "DB", resp_sep); break;

        case RIG_METER_PO: fprintf(fout, "%d=%s%c", val.i, "PO", resp_sep); break;

        case RIG_METER_TEMP: fprintf(fout, "%d=%s%c", val.i, "TEMP", resp_sep); break;

        default:
            rig_debug(RIG_DEBUG_ERR,
                      "%s: unknown meter=%d, only know COMP,ALC,SWR,ID/IC,VDD,DB,PO,TEMP\n", __func__,
                      val.i);
            RETURNFUNC2(-RIG_EINVAL);
        }

        RETURNFUNC2(RIG_OK);
    }


    if (RIG_LEVEL_IS_FLOAT(level))
    {
        fprintf(fout, "%g%c", val.f, resp_sep);
    }
    else
    {
        fprintf(fout, "%d%c", val.i, resp_sep);
    }

    RETURNFUNC2(status);
}


/* 'U' */
declare_proto_rig(set_func)
{
    setting_t func;
    int func_stat;

    ENTERFUNC2;

    if (!strcmp(arg1, "?"))
    {
        char s[SPRINTF_MAX_SIZE];
        rig_sprintf_func(s, sizeof(s), STATE(rig)->has_set_func);
        fprintf(fout, "%s\n", s);
        RETURNFUNC2(RIG_OK);
    }

    func = rig_parse_func(arg1);

    if (!rig_has_set_func(rig, func))
    {
        const struct confparams *cfp;

        cfp = rig_ext_lookup(rig, arg1);

        if (!cfp)
        {
            RETURNFUNC2(-RIG_ENAVAIL);    /* no such parameter */
        }

        CHKSCN1ARG(sscanf(arg2, "%d", &func_stat));

        RETURNFUNC2(rig_set_ext_func(rig, vfo, cfp->token, func_stat));
    }

    CHKSCN1ARG(sscanf(arg2, "%d", &func_stat));
    RETURNFUNC2(rig_set_func(rig, vfo, func, func_stat));
}


/* 'u' */
declare_proto_rig(get_func)
{
    int status;
    setting_t func;
    int func_stat;

    ENTERFUNC2;

    if (!strcmp(arg1, "?"))
    {
        char s[SPRINTF_MAX_SIZE];
        rig_sprintf_func(s, sizeof(s), STATE(rig)->has_get_func);
        fprintf(fout, "%s\n", s);
        RETURNFUNC2(RIG_OK);
    }

    func = rig_parse_func(arg1);

    if (!rig_has_get_func(rig, func))
    {
        const struct confparams *cfp;

        cfp = rig_ext_lookup(rig, arg1);

        if (!cfp)
        {
            RETURNFUNC2(-RIG_EINVAL);    /* no such parameter */
        }

        status = rig_get_ext_func(rig, vfo, cfp->token, &func_stat);

        if (status != RIG_OK)
        {
            RETURNFUNC2(status);
        }

        if (interactive && prompt)
        {
            fprintf(fout, "%s: ", cmd->arg2);
        }

        fprintf(fout, "%d%c", func_stat, resp_sep);

        RETURNFUNC2(status);
    }

    status = rig_get_func(rig, vfo, func, &func_stat);

    if (status != RIG_OK)
    {
        RETURNFUNC2(status);
    }

    if (interactive && prompt)
    {
        fprintf(fout, "%s: ", cmd->arg1);
    }

    fprintf(fout, "%d%c", func_stat, resp_sep);

    RETURNFUNC2(status);
}


/* 'P' */
declare_proto_rig(set_parm)
{
    setting_t parm;
    value_t val;

    ENTERFUNC2;

    if (!strcmp(arg1, "?"))
    {
        char s[SPRINTF_MAX_SIZE];
        rig_sprintf_parm(s, sizeof(s), STATE(rig)->has_set_parm);
        fprintf(fout, "%s\n", s);
        RETURNFUNC2(RIG_OK);
    }

    if (strcmp(arg1, "BANDSELECT") == 0 && !strcmp(arg2, "?"))
    {
        char s[SPRINTF_MAX_SIZE];
        rig_sprintf_parm_gran(s, sizeof(s) - 1, RIG_PARM_BANDSELECT,
                              rig->caps->parm_gran);
        char *p = strchr(s, ')');

        if (p) { *p = 0; }

        p = strchr(s, '(');

        if (p)
        {
            char *comma;

            while ((comma = strchr(p, ','))) { *comma = ' '; }

            fprintf(fout, "%s\n", p + 1);
            RETURNFUNC2(RIG_OK);
        }
        else { RETURNFUNC2(-RIG_EINTERNAL); }
    }

    if (strcmp(arg1, "KEYERTYPE") == 0 && strcmp(arg2, "?") != 0)
    {
        if (strcmp(arg2, "STRAIGHT") == 0) {arg2 = "0";}
        else if (strcmp(arg2, "BUG") == 0) {arg2 = "1";}
        else if (strcmp(arg2, "PADDLE") == 0) {arg2 = "2";}
    }

    parm = rig_parse_parm(arg1);

    if (!rig_has_set_parm(rig, parm))
    {
        const struct confparams *cfp;

        cfp = rig_ext_lookup(rig, arg1);

        if (!cfp)
        {
            RETURNFUNC2(-RIG_EINVAL);    /* no such parameter */
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

        case RIG_CONF_INT:
            CHKSCN1ARG(sscanf(arg2, "%f", &val.f));
            break;

        case RIG_CONF_NUMERIC:
            CHKSCN1ARG(sscanf(arg2, "%g", &val.f));
            break;

        case RIG_CONF_STRING:
#if 0
            if (parm == RIG_PARM_KEYERTYPE)
            {
                val.i = atoi(arg2);
            }
            else
#endif
            {
                val.cs = arg2;
            }

            break;

        case RIG_CONF_BINARY:
            val.b.d = (unsigned char *)arg2;
            break;

        default:
            RETURNFUNC2(-RIG_ECONF);
        }

        RETURNFUNC2(rig_set_ext_parm(rig, cfp->token, val));
    }

    if (RIG_PARM_IS_FLOAT(parm))
    {
        CHKSCN1ARG(sscanf(arg2, "%f", &val.f));
    }
    else if (RIG_PARM_IS_STRING(parm))
    {
        if (parm == RIG_PARM_KEYERTYPE)
        {
            val.i = atoi(arg2);
        }
        else
        {
            val.cs = arg2;
        }
    }
    else
    {
        CHKSCN1ARG(sscanf(arg2, "%d", &val.i));
    }

    RETURNFUNC2(rig_set_parm(rig, parm, val));
}


/* 'p' */
declare_proto_rig(get_parm)
{
    int status;
    setting_t parm;
    value_t val;
    char buffer[RIG_BIN_MAX];

    ENTERFUNC2;

    if (!strcmp(arg1, "?"))
    {
        char s[SPRINTF_MAX_SIZE];
        rig_sprintf_parm(s, sizeof(s), STATE(rig)->has_get_parm);
        fprintf(fout, "%s\n", s);
        RETURNFUNC2(RIG_OK);
    }

    parm = rig_parse_parm(arg1);

    if (!rig_has_get_parm(rig, parm))
    {
        const struct confparams *cfp;

        cfp = rig_ext_lookup(rig, arg1);

        if (!cfp)
        {
            RETURNFUNC2(-RIG_EINVAL);    /* no such parameter */
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
            RETURNFUNC2(status);
        }

        if (interactive && prompt)
        {
            fprintf(fout, "%s: ", cmd->arg2);
        }

        switch (cfp->type)
        {
        case RIG_CONF_BUTTON:
            /* there's not sense in retrieving value of stateless button */
            RETURNFUNC2(-RIG_EINVAL);

        case RIG_CONF_CHECKBUTTON:
        case RIG_CONF_COMBO:
            fprintf(fout, "%d%c", val.i, resp_sep);
            break;

        case RIG_CONF_INT:
            fprintf(fout, "%.0f%c", val.f, resp_sep);
            break;

        case RIG_CONF_NUMERIC:
            fprintf(fout, "%g%c", val.f, resp_sep);
            break;

        case RIG_CONF_STRING:
            fprintf(fout, "%s%c", val.s, resp_sep);
            break;

        case RIG_CONF_BINARY:
            dump_hex((unsigned char *)buffer, val.b.l);
            fprintf(fout, "%c", resp_sep);
            break;

        default:
            RETURNFUNC2(-RIG_ECONF);
        }

        RETURNFUNC2(status);
    }

    status = rig_get_parm(rig, parm, &val);

    if (status != RIG_OK)
    {
        RETURNFUNC2(status);
    }

    if (interactive && prompt)
    {
        fprintf(fout, "%s: ", cmd->arg2);
    }

    if (parm == RIG_PARM_KEYERTYPE)
    {
        char *s = "STRAIGHT";

        if (val.i == 1) { s = "BUG"; }
        else if (val.i == 2) { s = "PADDLE"; }

        fprintf(fout, "%s%cv", s, resp_sep);
    }
    else if (RIG_PARM_IS_FLOAT(parm))
    {
        rig_debug(RIG_DEBUG_ERR, "%s: float\n", __func__);
        fprintf(fout, "%f%c", val.f, resp_sep);
    }
    else if (RIG_PARM_IS_STRING(parm))
    {
        rig_debug(RIG_DEBUG_ERR, "%s: string\n", __func__);
        fprintf(fout, "%s%c", val.s, resp_sep);
    }
    else
    {
        rig_debug(RIG_DEBUG_ERR, "%s: int\n", __func__);
        fprintf(fout, "%d%c", val.i, resp_sep);
    }

    RETURNFUNC2(status);
}


/* 'B' */
declare_proto_rig(set_bank)
{
    int bank;

    ENTERFUNC2;

    CHKSCN1ARG(sscanf(arg1, "%d", &bank));

    RETURNFUNC2(rig_set_bank(rig, vfo, bank));
}


/* 'E' */
declare_proto_rig(set_mem)
{
    int ch;

    ENTERFUNC2;

    CHKSCN1ARG(sscanf(arg1, "%d", &ch));

    RETURNFUNC2(rig_set_mem(rig, vfo, ch));
}


/* 'e' */
declare_proto_rig(get_mem)
{
    int status;
    int ch;

    ENTERFUNC2;

    status = rig_get_mem(rig, vfo, &ch);

    if (status != RIG_OK)
    {
        RETURNFUNC2(status);
    }

    if ((interactive && prompt) || (interactive && !prompt && ext_resp))
    {
        fprintf(fout, "%s: ", cmd->arg1);
    }

    fprintf(fout, "%d%c", ch, resp_sep);

    RETURNFUNC2(status);
}


/* 'G' */
declare_proto_rig(vfo_op)
{
    vfo_op_t op;

    ENTERFUNC2;

    if (!strcmp(arg1, "?"))
    {
        char s[SPRINTF_MAX_SIZE];
        rig_sprintf_vfop(s, sizeof(s), STATE(rig)->vfo_ops);
        fprintf(fout, "%s\n", s);
        RETURNFUNC2(RIG_OK);
    }

    op = rig_parse_vfo_op(arg1);

    if (RIG_OP_NONE == op)
    {
        rig_debug(RIG_DEBUG_ERR, "%s: rig_parse_vfo failed with '%s'\n", __func__,
                  arg1);
        RETURNFUNC2(-RIG_EINVAL);
    }

    RETURNFUNC2(rig_vfo_op(rig, vfo, op));
}


/* 'g' */
declare_proto_rig(scan)
{
    scan_t op;
    int ch;

    ENTERFUNC2;

    if (!strcmp(arg1, "?"))
    {
        char s[SPRINTF_MAX_SIZE];
        rig_sprintf_scan(s, sizeof(s), rig->caps->scan_ops);
        fprintf(fout, "%s\n", s);
        RETURNFUNC2(RIG_OK);
    }

    op = rig_parse_scan(arg1);
    CHKSCN1ARG(sscanf(arg2, "%d", &ch));
    RETURNFUNC2(rig_scan(rig, vfo, op, ch));
}


/* 'H' */
declare_proto_rig(set_channel)
{
    const channel_cap_t *mem_caps = NULL;
    const chan_t *chan_list;
    channel_t chan;
    int status;
    char s[16];

    ENTERFUNC2;

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
        RETURNFUNC2(-RIG_ECONF);
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

    status = rig_set_channel(rig, vfo, &chan);

    RETURNFUNC2(status);
}


/* 'h' */
declare_proto_rig(get_channel)
{
    int status;
    int read_only = 0;
    channel_t chan;

    ENTERFUNC2;

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

    status = rig_get_channel(rig, RIG_VFO_NONE, &chan, read_only);

    if (status != RIG_OK)
    {
        RETURNFUNC2(status);
    }

    status = dump_chan(fout, rig, &chan);

    if (chan.ext_levels)
    {
        free(chan.ext_levels);
    }

    RETURNFUNC2(status);
}


/* 'A' */
/**
 * \deprecated Function deprecated. Use the new async data functionality instead.
 */
declare_proto_rig(set_trn)
{
    ENTERFUNC2;
    RETURNFUNC2(-RIG_EDEPRECATED);
}


/* 'a' */
/**
 * \deprecated Function deprecated. Use the new async data functionality instead.
 */
declare_proto_rig(get_trn)
{
    ENTERFUNC2;
    RETURNFUNC2(-RIG_EDEPRECATED);
}


/* '_' */
declare_proto_rig(get_info)
{
    const char *s;

    ENTERFUNC2;

    s = rig_get_info(rig);

    if ((interactive && prompt) || (interactive && !prompt && ext_resp))
    {
        fprintf(fout, "%s: ", cmd->arg1);
    }

    fprintf(fout, "%s%c", s ? s : "None", resp_sep);

    RETURNFUNC2(RIG_OK);
}


int dump_chan(FILE *fout, RIG *rig, channel_t *chan)
{
    int idx, firstloop = 1;
    char freqbuf[16];
    char widthbuf[16];
    char prntbuf[256];

    ENTERFUNC2;

    fprintf(fout,
            "Channel: %d, Name: '%s'\n",
            chan->channel_num,
            chan->channel_desc);

    fprintf(fout,
            "VFO: %s, Antenna: %d, Split: %s\n",
            rig_strvfo(chan->vfo),
            chan->ant,
            chan->split == RIG_SPLIT_ON ? "ON" : "OFF");

    sprintf_freq(freqbuf, sizeof(freqbuf), chan->freq);
    sprintf_freq(widthbuf, sizeof(widthbuf), chan->width);
    fprintf(fout,
            "Freq:   %s\tMode:   %s\tWidth:   %s\n",
            freqbuf,
            rig_strrmode(chan->mode),
            widthbuf);

    sprintf_freq(freqbuf, sizeof(freqbuf), chan->tx_freq);
    sprintf_freq(widthbuf, sizeof(widthbuf), chan->tx_width);
    fprintf(fout,
            "txFreq: %s\ttxMode: %s\ttxWidth: %s\n",
            freqbuf,
            rig_strrmode(chan->tx_mode),
            widthbuf);

    sprintf_freq(freqbuf, sizeof(freqbuf), chan->rptr_offs);
    fprintf(fout,
            "Shift: %s, Offset: %s%s, ",
            rig_strptrshift(chan->rptr_shift),
            chan->rptr_offs > 0 ? "+" : "",
            freqbuf);

    sprintf_freq(freqbuf, sizeof(freqbuf), chan->tuning_step);
    fprintf(fout, "Step: %s, ", freqbuf);
    sprintf_freq(freqbuf, sizeof(freqbuf), chan->rit);
    fprintf(fout, "RIT: %s%s, ", chan->rit > 0 ? "+" : "", freqbuf);
    sprintf_freq(freqbuf, sizeof(freqbuf), chan->xit);
    fprintf(fout, "XIT: %s%s\n", chan->xit > 0 ? "+" : "", freqbuf);

    fprintf(fout, "CTCSS: %u.%uHz, ", chan->ctcss_tone / 10, chan->ctcss_tone % 10);
    fprintf(fout,
            "CTCSSsql: %u.%uHz, ",
            chan->ctcss_sql / 10,
            chan->ctcss_sql % 10);

    fprintf(fout, "DCS: %u.%u, ", chan->dcs_code / 10, chan->dcs_code % 10);
    fprintf(fout, "DCSsql: %u.%u\n", chan->dcs_sql / 10, chan->dcs_sql % 10);

    rig_sprintf_func(prntbuf, sizeof(prntbuf), chan->funcs);
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
            RETURNFUNC2(-RIG_EINVAL);
        }

        switch (cfp->type)
        {
        case RIG_CONF_STRING:
            strcpy(lstr, chan->ext_levels[idx].val.s);
            break;

        case RIG_CONF_COMBO:
            SNPRINTF(lstr, sizeof(lstr), "%d", chan->ext_levels[idx].val.i);
            break;

        case RIG_CONF_INT:
            SNPRINTF(lstr, sizeof(lstr), "%.0f", chan->ext_levels[idx].val.f);
            break;

        case RIG_CONF_NUMERIC:
            SNPRINTF(lstr, sizeof(lstr), "%g", chan->ext_levels[idx].val.f);
            break;

        case RIG_CONF_CHECKBUTTON:
            SNPRINTF(lstr, sizeof(lstr), "%s", chan->ext_levels[idx].val.i ? "ON" : "OFF");
            break;

        case RIG_CONF_BUTTON:
            continue;

        default:
            RETURNFUNC2(-RIG_EINTERNAL);
        }

        fprintf(fout, ",\t %s: %s", cfp->name, lstr);
    }

    fprintf(fout, "\n");

    RETURNFUNC2(RIG_OK);
}


/* '1' */
declare_proto_rig(dump_caps)
{
    ENTERFUNC2;

#if 1

    if (rig->caps->rig_model == RIG_MODEL_NETRIGCTL)
    {
        char cmd[32];
        SNPRINTF(cmd, sizeof(cmd), "\\dump_caps\n");
        dumpstate(rig, fout);
    }
    else
#endif
    {
        dumpcaps(rig, fout);
    }

    RETURNFUNC2(RIG_OK);
}


/* For rigctld internal use */
declare_proto_rig(dump_state)
{
    int i;
    struct rig_state *rs = STATE(rig);
    char buf[1024];

    ENTERFUNC2;

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

    for (i = 0; i < HAMLIB_FRQRANGESIZ
            && !RIG_IS_FRNG_END(rs->rx_range_list[i]); i++)
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

    for (i = 0; i < HAMLIB_FRQRANGESIZ
            && !RIG_IS_FRNG_END(rs->tx_range_list[i]); i++)
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

    for (i = 0; i < HAMLIB_TSLSTSIZ && !RIG_IS_TS_END(rs->tuning_steps[i]); i++)
    {
        fprintf(fout,
                "0x%"PRXll" %ld\n",
                rs->tuning_steps[i].modes,
                rs->tuning_steps[i].ts);
    }

    fprintf(fout, "0 0\n");

    for (i = 0; i < HAMLIB_FLTLSTSIZ && !RIG_IS_FLT_END(rs->filters[i]); i++)
    {
        fprintf(fout,
                "0x%"PRXll" %ld\n",
                rs->filters[i].modes,
                rs->filters[i].width);
    }

    fprintf(fout, "0 0\n");

#if 0
    chan_t chan_list[HAMLIB_CHANLSTSIZ]; /*!< Channel list, zero ended */
#endif

    fprintf(fout, "%ld\n", rs->max_rit);
    fprintf(fout, "%ld\n", rs->max_xit);
    fprintf(fout, "%ld\n", rs->max_ifshift);
    fprintf(fout, "%d\n", rs->announces);

    for (i = 0; i < HAMLIB_MAXDBLSTSIZ && rs->preamp[i]; i++)
    {
        fprintf(fout, "%d ", rs->preamp[i]);
    }

    fprintf(fout, "\n");

    for (i = 0; i < HAMLIB_MAXDBLSTSIZ && rs->attenuator[i]; i++)
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
    rig_debug(RIG_DEBUG_ERR, "%s: chk_vfo_executed=%d\n", __func__,
              chk_vfo_executed);

    if (chk_vfo_executed) // for 3.3 compatibility
    {
        fprintf(fout, "vfo_ops=0x%x\n", rig->caps->vfo_ops);
        fprintf(fout, "ptt_type=0x%x\n",
                PTTPORT(rig)->type.ptt);
        fprintf(fout, "targetable_vfo=0x%x\n", rig->caps->targetable_vfo);
        fprintf(fout, "has_set_vfo=%d\n", rig->caps->set_vfo != NULL);
        fprintf(fout, "has_get_vfo=%d\n", rig->caps->get_vfo != NULL);
        fprintf(fout, "has_set_freq=%d\n", rig->caps->set_freq != NULL);
        fprintf(fout, "has_get_freq=%d\n", rig->caps->get_freq != NULL);
        fprintf(fout, "has_set_conf=%d\n", rig->caps->set_conf != NULL);
        fprintf(fout, "has_get_conf=%d\n", rig->caps->get_conf != NULL);
#if 0
        fprintf(fout, "has_set_parm=%d\n", rig->caps->set_parm != NULL);
        fprintf(fout, "has_get_parm=%d\n", rig->caps->get_parm != NULL);
        fprintf(fout, "parm_gran=0x%x\n", rig->caps->parm_gran);
#endif
        // for the future
//        fprintf(fout, "has_set_trn=%d\n", rig->caps->set_trn != NULL);
//        fprintf(fout, "has_get_trn=%d\n", rig->caps->get_trn != NULL);
        fprintf(fout, "has_power2mW=%d\n", rig->caps->power2mW != NULL);
        fprintf(fout, "has_mW2power=%d\n", rig->caps->mW2power != NULL);
        fprintf(fout, "has_get_ant=%d\n", rig->caps->get_ant != NULL);
        fprintf(fout, "has_set_ant=%d\n", rig->caps->set_ant != NULL);
        fprintf(fout, "timeout=%d\n", rig->caps->timeout);
        fprintf(fout, "rig_model=%d\n", rig->caps->rig_model);
        fprintf(fout, "rigctld_version=%s\n", hamlib_version2);
        rig_sprintf_agc_levels(rig, buf, sizeof(buf));

        if (strlen(buf) > 0) { fprintf(fout, "agc_levels=%s\n", buf); }

        if (rig->caps->ctcss_list)
        {
            fprintf(fout, "ctcss_list=");

            for (i = 0; i < CTCSS_LIST_SIZE && rig->caps->ctcss_list[i] != 0; i++)
            {
                fprintf(fout,
                        " %u.%1u",
                        rig->caps->ctcss_list[i] / 10, rig->caps->ctcss_list[i] % 10);
            }

            fprintf(fout, "\n");
        }

        if (rig->caps->dcs_list)
        {
            fprintf(fout, "dcs_list=");

            for (i = 0; i < DCS_LIST_SIZE && rig->caps->dcs_list[i] != 0; i++)
            {
                fprintf(fout,
                        " %u",
                        rig->caps->dcs_list[i]);
            }

            fprintf(fout, "\n");
        }


        fprintf(fout, "level_gran=");

        for (i = 0; i < RIG_SETTING_MAX; ++i)
        {
            if (RIG_LEVEL_IS_FLOAT(i))
            {
                fprintf(fout, "%d=%g,%g,%g;", i, rs->level_gran[i].min.f,
                        rs->level_gran[i].max.f, rs->level_gran[i].step.f);
            }
            else
            {
                fprintf(fout, "%d=%d,%d,%d;", i, rs->level_gran[i].min.i,
                        rs->level_gran[i].max.i, rs->level_gran[i].step.i);
            }
        }

        fprintf(fout, "\nparm_gran=");

        for (i = 0; i < RIG_SETTING_MAX; ++i)
        {
            if (RIG_LEVEL_IS_FLOAT(i))
            {
                fprintf(fout, "%d=%g,%g,%g;", i, rs->parm_gran[i].min.f,
                        rs->parm_gran[i].max.f, rs->parm_gran[i].step.f);
            }
            else
            {
                fprintf(fout, "%d=%d,%d,%d;", i, rs->level_gran[i].min.i,
                        rs->level_gran[i].max.i, rs->level_gran[i].step.i);
            }
        }

        fprintf(fout, "\n");

        rs->rig_model = rig->caps->rig_model;
        fprintf(fout, "rig_model=%d\n", rs->rig_model);
        fprintf(fout, "hamlib_version=%s\n", hamlib_version2);
        fprintf(fout, "done\n");
    }

    RETURNFUNC2(RIG_OK);
}


/* '3' */
declare_proto_rig(dump_conf)
{
    ENTERFUNC2;

    dumpconf(rig, fout);

    RETURNFUNC2(RIG_OK);
}


/* 'Y' */
declare_proto_rig(set_ant)
{
    ant_t ant;
    value_t option; // some rigs have a another option for the antenna

    ENTERFUNC2;

    CHKSCN1ARG(sscanf(arg1, "%d", &ant));
    CHKSCN1ARG(sscanf(arg2, "%d", &option.i)); // assuming they are integer values

    RETURNFUNC2(rig_set_ant(rig, vfo, rig_idx2setting(ant - 1), option));
}


/* 'y' */
declare_proto_rig(get_ant)
{
    int status;
    ant_t ant, ant_curr, ant_tx, ant_rx;
    value_t option;
    char antbuf[64];

    ENTERFUNC2;

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
        RETURNFUNC2(status);
    }

    if ((interactive && prompt) || (interactive && !prompt && ext_resp))
    {
        fprintf(fout, "%s: ", cmd->arg1);
    }

    rig_sprintf_ant(antbuf, sizeof(antbuf), ant_curr);
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

    rig_sprintf_ant(antbuf, sizeof(antbuf), ant_tx);
    fprintf(fout, "%s%c", antbuf, resp_sep);
    //fprintf(fout, "%d%c", rig_setting2idx(ant_tx)+1, resp_sep);

    if ((interactive && prompt) || (interactive && !prompt && ext_resp))
    {
        fprintf(fout, "%s: ", cmd->arg4);
    }

    rig_sprintf_ant(antbuf, sizeof(antbuf), ant_rx);
    fprintf(fout, "%s%c", antbuf, resp_sep);
    //fprintf(fout, "%d%c", rig_setting2idx(ant_rx)+1, resp_sep);

    RETURNFUNC2(status);
}


/* '*' */
declare_proto_rig(reset)
{
    int reset;

    ENTERFUNC2;

    CHKSCN1ARG(sscanf(arg1, "%d", &reset));

    RETURNFUNC2(rig_reset(rig, (reset_t) reset));
}


/* 'b' */
declare_proto_rig(send_morse)
{
    ENTERFUNC2;

    RETURNFUNC2(rig_send_morse(rig, vfo, arg1));
}

/* 0xvv */
declare_proto_rig(stop_morse)
{
    ENTERFUNC2;

    RETURNFUNC2(rig_stop_morse(rig, vfo));
}

declare_proto_rig(wait_morse)
{
    ENTERFUNC2;

    RETURNFUNC2(rig_wait_morse(rig, vfo));
}

/* '8' */
declare_proto_rig(send_voice_mem)
{
    int ch;

    ENTERFUNC2;

    CHKSCN1ARG(sscanf(arg1, "%d", &ch));

    RETURNFUNC2(rig_send_voice_mem(rig, vfo, ch));
}

declare_proto_rig(stop_voice_mem)
{
    ENTERFUNC2;
    RETURNFUNC2(rig_stop_voice_mem(rig, vfo));
}

declare_proto_rig(send_dtmf)
{
    ENTERFUNC2;

    RETURNFUNC2(rig_send_dtmf(rig, vfo, arg1));
}


declare_proto_rig(recv_dtmf)
{
    int status;
    int len;
    char digits[MAXARGSZ];

    ENTERFUNC2;

    len = MAXARGSZ - 1;
    status = rig_recv_dtmf(rig, vfo, digits, &len);

    if (status != RIG_OK)
    {
        RETURNFUNC2(status);
    }

    if (interactive && prompt)
    {
        fprintf(fout, "%s: ", cmd->arg1);
    }

    fprintf(fout, "%s\n", digits);

    RETURNFUNC2(status);
}


/* '0x87' */
declare_proto_rig(set_powerstat)
{
    int stat;
    int retval;

    ENTERFUNC2;

    CHKSCN1ARG(sscanf(arg1, "%d", &stat));

    retval = rig_set_powerstat(rig, (powerstat_t) stat);

    if (retval == RIG_OK)
    {
        rig_powerstat = stat; // update our global so others can see powerstat
    }

    fflush(fin);
    RETURNFUNC2(retval);
}


/* '0x88' */
declare_proto_rig(get_powerstat)
{
    int status;
    powerstat_t stat;

    ENTERFUNC2;

    status = rig_get_powerstat(rig, &stat);

    if (status != RIG_OK)
    {
        RETURNFUNC2(status);
    }

    if ((interactive && prompt) || (interactive && !prompt && ext_resp))
    {
        fprintf(fout, "%s: ", cmd->arg1);
    }

    fprintf(fout, "%d%c", stat, resp_sep);
    rig_powerstat = stat; // update our global so others can see powerstat

    RETURNFUNC2(status);
}

static int hasbinary(char *s, int len)
{
    int i;

    for (i = 0; i < len; ++i)
    {
        if (!isascii(s[i])) { return (1); }
    }

    return 0;
}

/*
 * Special debugging purpose send command display reply until there's a
 * timeout.
 *
 * 'w' and 'W'
 */
extern int flrig_cat_string2(RIG *rig, const char *arg, char *value,
                             int value_size);
extern int flrig_cat_string(RIG *rig, const char *arg);

static void toLowerCase(char *s)
{
    if (s == NULL) { return; } // Check if the pointer is NULL

    while (*s)   // Iterate until we hit the null terminator of the string
    {
        *s = tolower((unsigned char) * s); // Convert each character to lowercase
        s++; // Move to the next character
    }
}

declare_proto_rig(send_cmd)
{
    int retval;
    hamlib_port_t *rp = RIGPORT(rig);
    int backend_num, cmd_len;
#define BUFSZ 512
    char bufcmd[BUFSZ * 5]; // allow for 5 chars for each binary
    unsigned char buf[BUFSZ];
    char eom_buf[4] = { 0xa, 0xd, 0, 0 };
    int binary = 0;
    int rxbytes = BUFSZ;
    int cmdcount = 1;
    int simulate = rig->caps->rig_model == RIG_MODEL_DUMMY ||
                   rig->caps->rig_model == RIG_MODEL_NONE ||
                   rp->rig == RIG_PORT_NONE;

    ENTERFUNC2;

    /*
     * binary protocols enter values as \0xZZ\0xYY..
     */
    backend_num = RIG_BACKEND_NUM(rig->caps->rig_model);

    rig_debug(RIG_DEBUG_TRACE, "%s: backend_num=%d\n", __func__, backend_num);

    if (rig->caps->rig_model == RIG_MODEL_FLRIG)
    {
        // call flrig raw send function cat_string or cat_priority_string
        flrig_cat_string2(rig, arg1, (char *)buf, sizeof(buf));
        fwrite(cmd->arg2, 1, strlen(cmd->arg2), fout); /* i.e. "Frequency" */
        fprintf(fout, ": %s\n", buf);
        RETURNFUNC2(RIG_OK);
    }

    // need to move the eom_buf to rig-specifc backends
    // we'll let KENWOOD backends use the ; char in the rigctl commands
    if (backend_num == RIG_KENWOOD || backend_num == RIG_YAESU)
    {
        rig_debug(RIG_DEBUG_TRACE, "%s: KENWOOD\n", __func__);
//        eom_buf[0] = 0;
        send_cmd_term = 0;
    }

    rig_debug(RIG_DEBUG_TRACE, "%s: arg1=%s\n", __func__, arg1);

    // note that hex sscanf expects at least 2 values to pass this check
    // is there any situation where only one x00 value would be written?
    unsigned int n, i1, i2;
    n = sscanf(arg1, "x%x x%x", &i1, &i2);

    if (send_cmd_term == -1
            || backend_num == RIG_ICOM
            || backend_num == RIG_KACHINA
            || backend_num == RIG_MICROTUNE
            || ((strstr(arg1, "\\0x") || n == 2)
                && (rig->caps->rig_model != RIG_MODEL_NETRIGCTL))
       )
    {

        const char *p = arg1, *pp = NULL;
        int i;
        rig_debug(RIG_DEBUG_TRACE, "%s: send_cmd_term==-1, arg1=%s\n", __func__, arg1);

        if (arg1[strlen(arg1) - 1] != ';' && strstr(arg1, "\\0x") == NULL
                && sscanf(arg1, "x%x x%x", &i1, &i2) != 2)
        {
            rig_debug(RIG_DEBUG_ERR,
                      "%s: expecting binary hex string here, either x00 xff or \\0x00 \\0xff or x00xff\n",
                      __func__);
            RETURNFUNC2(-RIG_EINVAL);
        }

        for (i = 0; i < BUFSZ - 1 && p != pp; i++)
        {
            rig_debug(RIG_DEBUG_ERR, "%s: p=%s\n", __func__, p);
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

        //SNPRINTF(bufcmd, sizeof(bufcmd), "%s %s\n", cmd->cmd, arg1);
        if (cmd->cmd == 'w')
        {
            SNPRINTF(bufcmd, sizeof(bufcmd), "%c %s\n", cmd->cmd, arg1);
        }
        else
        {
            int nbytes = 0;
            int n = sscanf(arg2, "%d", &nbytes);

            if (n == 1) {SNPRINTF(bufcmd, sizeof(bufcmd), "%c %s %d\n", cmd->cmd, arg1, nbytes);}
            else {SNPRINTF(bufcmd, sizeof(bufcmd), "%c %s %s\n", cmd->cmd, arg1, arg2);}
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

        SNPRINTF(tmpbuf, sizeof(tmpbuf), "0x%02x 0x%02x 0x%02x", eom_buf[0], eom_buf[1],
                 eom_buf[2]);

        rig_debug(RIG_DEBUG_TRACE, "%s: text protocol, send_cmd_term=%s\n", __func__,
                  tmpbuf);
    }

    rig_debug(RIG_DEBUG_TRACE, "%s: rigport=%d, bufcmd=%s, cmd_len=%d\n", __func__,
              rp->fd, hasbinary(bufcmd, cmd_len) ? "BINARY" : bufcmd, cmd_len);

    set_transaction_active(rig);

    if (simulate)
    {
        rig_debug(RIG_DEBUG_VERBOSE, "%s: simulating response for model %s\n",
                  __func__, rig->caps->model_name);
    }
    else
    {
        rig_flush(rp);

        // we don't want the 'w' command to wait too long
        int save_retry = rp->retry;
        rp->retry = 0;
        retval = write_block(rp, (unsigned char *) bufcmd, cmd_len);
        rp->retry = save_retry;

        if (retval != RIG_OK)
        {
            rig_flush_force(rp, 1);
            set_transaction_inactive(rig);
            RETURNFUNC2(retval);
        }
    }

    if ((interactive && prompt) || (interactive && !prompt && ext_resp))
    {
        rig_debug(RIG_DEBUG_TRACE, "%s: debug Cmd\n", __func__);
        fwrite(cmd->arg2, 1, strlen(cmd->arg2), fout); /* i.e. "Frequency" */
        fwrite(": ", 1, 2, fout); /* i.e. "Frequency" */
    }

    retval = 0;

    if (strstr(arg1, ";") || arg2[0] == ';')
    {
        char *p = strchr(arg1, ';');

        while (p)
        {
            cmdcount++;
            p = strchr(p + 1, ';');
        }

        // Kenwood, Yaeus, and similar
        eom_buf[0] = ';';
        eom_buf[1] = 0;
    }
    else if (strstr(arg1, "xfd") || strstr(arg2, "xfd"))
    {
        char *s = strdup(arg1);
        toLowerCase(s);
        // ICOM answer terminator
        eom_buf[0] = 0xfd;
        eom_buf[1] = 0;
        char *p = strstr(s, "xfd");

        while (p)
        {
            cmdcount++;
            p = strstr(p + 1, "xfd");
        }

        free(s);
    }

    do
    {
        int hexval;
        rxbytes = BUFSZ; // leave room for CR

        if (arg2) // see if we have bytes or a hexval for terminating the 'W' command
        {
            sscanf(arg2, "%d", &rxbytes);

            if (sscanf(arg2, "\\0x%2x", &hexval) == 1)
            {
                eom_buf[0] = hexval;
            }
        }

        if (rxbytes > 0 && rxbytes != BUFSZ)
        {
            ++rxbytes;  // need length + 1 for end of string
            eom_buf[0] = 0;
        }

        if (simulate)
        {
            if (retval == 0)
            {
                // Simulate a response by copying the command
                memcpy(buf, bufcmd, cmd_len);
                retval = cmd_len;
            }
            else
            {
                retval = 0;
            }
        }
        else
        {
            /* Assumes CR or LF is end of line char for all ASCII protocols. */
            retval = read_string(rp, buf, rxbytes, eom_buf,
                                 strlen(eom_buf), 0, 1);

            if (retval < 0)
            {
                rig_debug(RIG_DEBUG_ERR, "%s: read_string error %s\n", __func__,
                          rigerror(retval));
                break;
            }
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
                SNPRINTF(hex, sizeof(hex), "\\0x%02X", (unsigned char)buf[i]);

                if ((strlen(hexbuf) + strlen(hex) + 1) >= hexbufbytes)
                {
                    hexbufbytes *= 2;
                    rig_debug(RIG_DEBUG_TRACE, "%s: doubling hexbuf size to %d\n", __func__,
                              hexbufbytes);
                    hexbuf = realloc(hexbuf, hexbufbytes);
                }

                strncat(hexbuf, hex, hexbufbytes - 1);
            }

            rig_flush_force(rp, 1);
            set_transaction_inactive(rig);

            rig_debug(RIG_DEBUG_TRACE, "%s: binary=%s, retval=%d\n", __func__, hexbuf,
                      retval);
            fprintf(fout, "%s %d\n", hexbuf, retval);
            free(hexbuf);
            RETURNFUNC2(RIG_OK);
        }
        else
        {
            // we should be in printable ASCII here
            fprintf(fout, "%s%c", buf, cmdcount == 1 ? '\n' : '\0');
        }
    }
    while ((retval > 0 && rxbytes == BUFSZ && eom_buf[0] != ';'
            && (unsigned char)eom_buf[0] != 0xfd) || --cmdcount > 1);

    rig_flush_force(rp, 1);
    set_transaction_inactive(rig);

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

    RETURNFUNC2(retval);
}


/* '0xf0'--test if rigctld called with -o|--vfo option */
declare_proto_rig(chk_vfo)
{
    ENTERFUNC2;

    rig_debug(RIG_DEBUG_ERR, "%s: **********************************\n", __func__);

    if ((interactive && prompt) || (interactive && !prompt && ext_resp))
    {
        fprintf(fout, "%s: ", cmd->arg1);    /* i.e. "Frequency" */
    }

    fprintf(fout, "%d\n", STATE(rig)->vfo_opt);

    chk_vfo_executed = 1; // this allows us to control dump_state version

    RETURNFUNC2(RIG_OK);
}

/* '(' -- turn vfo option on */
declare_proto_rig(set_vfo_opt)
{
    int opt = 0;
    ENTERFUNC2;

    CHKSCN1ARG(sscanf(arg1, "%d", &opt));
    *vfo_opt = STATE(rig)->vfo_opt = opt;
    RETURNFUNC2(rig_set_vfo_opt(rig, opt));
}

/* '0xf1'--halt rigctld daemon */
declare_proto_rig(halt)
{
    /* a bit rough, TODO: clean daemon shutdown */
    exit(0);

    return (RIG_OK);
}


/* '0x8c'--pause processing */
declare_proto_rig(pause)
{
    unsigned seconds;

    CHKSCN1ARG(sscanf(arg1, "%u", &seconds));
    sleep(seconds);

    return (RIG_OK);
}

int rigctld_password_check(RIG *rig, const char *md5)
{
    int retval = -RIG_EINVAL;
    //fprintf(fout, "password %s\n", password);
    //rig_debug(RIG_DEBUG_TRACE, "%s: %s == %s\n", __func__, md5, rigctld_password);
    is_passwordOK = 0;

    char *mymd5 = rig_make_md5(rigctld_password);

    if (strcmp(md5, mymd5) == 0)
    {
        retval = RIG_OK;
        is_passwordOK = 1;
    }

    return (retval);
}

/* 0x98 */
declare_proto_rig(password)
{
    int retval = -RIG_EPROTO;
    const char *key = arg1;

    ENTERFUNC2;

    if (is_rigctld)
    {
        retval = rigctld_password_check(rig, key);
    }
    else
    {
        retval = rig_password(rig, key);
        //retval = RIG_OK;
    }

    if (retval == RIG_OK)
    {
        rig_debug(RIG_DEBUG_ERR, "%s: #1 password OK\n", __func__);
        //fprintf(fout, "Logged in\n");
    }
    else
    {
        //rig_debug(RIG_DEBUG_ERR, "%s: password error, '%s'!='%s'\n", __func__,
        //          key, rigctld_password);
        rig_debug(RIG_DEBUG_ERR, "%s: password error\n", __func__);
    }

    RETURNFUNC2(retval);
}

#if 0 // don't think we need this yet
/* 0x99 */
declare_proto_rig(set_password)
{
    const char *passwd = arg1;
    strncpy(rigctld_password, passwd, sizeof(passwd) - 1);
    rig_debug(RIG_DEBUG_ERR, "%s: set_password %s\n", __func__, rigctld_password);
    return (RIG_OK);
}
#endif

/* '0x8d' */
declare_proto_rig(set_twiddle)
{
    int seconds;

    ENTERFUNC2;

    CHKSCN1ARG(sscanf(arg1, "%d", &seconds));

    RETURNFUNC2(rig_set_twiddle(rig, seconds));
}

/* '0x97' */
declare_proto_rig(set_uplink)
{
    int val;

    ENTERFUNC2;

    CHKSCN1ARG(sscanf(arg1, "%d", &val));

    RETURNFUNC2(rig_set_uplink(rig, val));
}



/* '0x8e' */
declare_proto_rig(get_twiddle)
{
    int status;
    int seconds;

    ENTERFUNC2;

    status = rig_get_twiddle(rig, &seconds);

    if (status != RIG_OK)
    {
        RETURNFUNC2(status);
    }

    if ((interactive && prompt) || (interactive && !prompt && ext_resp))
    {
        fprintf(fout, "%s: ", cmd->arg1);
    }

    fprintf(fout, "%d\n", seconds);

    RETURNFUNC2(status);
}

/* '0x95' */
declare_proto_rig(set_cache)
{
    int ms;

    ENTERFUNC2;

    CHKSCN1ARG(sscanf(arg1, "%d", &ms));

    RETURNFUNC2(rig_set_cache_timeout_ms(rig, HAMLIB_CACHE_ALL, ms));
}


/* '0x96' */
declare_proto_rig(get_cache)
{
    int ms;

    ENTERFUNC2;

    ms = rig_get_cache_timeout_ms(rig, HAMLIB_CACHE_ALL);

    if ((interactive && prompt) || (interactive && !prompt && ext_resp))
    {
        fprintf(fout, "%s: ", cmd->arg1);
    }

    fprintf(fout, "%d\n", ms);

    RETURNFUNC2(RIG_OK);
}

/* '0xf8' */
declare_proto_rig(set_clock)
{
    int year, mon, day, hour = -1, min = -1, sec = 0;
    double msec = -1;
    int utc_offset = 0;
    int n;
    char timebuf[64];

    ENTERFUNC2;

    if (arg1 && strcasecmp(arg1, "local") == 0)
    {
        date_strget(timebuf, sizeof(timebuf), 1);
        rig_debug(RIG_DEBUG_VERBOSE, "%s: local time set = %s\n", __func__, timebuf);
        n = sscanf(timebuf, "%04d-%02d-%02dT%02d:%02d:%02d%lf%d", &year, &mon, &day,
                   &hour,
                   &min, &sec, &msec, &utc_offset);
    }
    else if (arg1 && strcasecmp(arg1, "utc") == 0)
    {
        date_strget(timebuf, sizeof(timebuf), 0);
        rig_debug(RIG_DEBUG_VERBOSE, "%s: utc time set = %s\n", __func__, timebuf);
        n = sscanf(timebuf, "%04d-%02d-%02dT%02d:%02d:%02d%lf%d", &year, &mon, &day,
                   &hour,
                   &min, &sec, &msec, &utc_offset);
    }
    else if (arg1 && (arg1[16] == '+' || arg1[16] == '-'))
    {
        // YYYY-MM-DDTHH:MM+ZZ
        n = sscanf(arg1, "%04d-%02d-%02dT%02d:%02d%d", &year, &mon, &day, &hour,
                   &min, &utc_offset);
    }
    else if (arg1 && (arg1[19] == '+' || arg1[19] == '-'))
    {
        // YYYY-MM-DDTHH:MM:SS+ZZ
        n = sscanf(arg1, "%04d-%02d-%02dT%02d:%02d:%02d%d", &year, &mon, &day, &hour,
                   &min, &sec, &utc_offset);
    }
    else if (arg1 && (arg1[23] == '+' || arg1[23] == '-'))
    {
        // YYYY-MM-DDTHH:MM:SS.SSS+ZZ
        n = sscanf(arg1, "%04d-%02d-%02dT%02d:%02d:%02d%lf%d", &year, &mon, &day, &hour,
                   &min, &sec, &msec, &utc_offset);
    }
    else
    {
        rig_debug(RIG_DEBUG_ERR, "%s: '%s' not valid time format\n", __func__, arg1);
        RETURNFUNC2(-RIG_EINVAL);
    }

    rig_debug(RIG_DEBUG_VERBOSE,
              "%s: n=%d, %04d-%02d-%02dT%02d:%02d:%02d.%0.3f%s%02d\n",
              __func__, n, year, mon, day, hour, min, sec, msec, utc_offset >= 0 ? "+" : "-",
              (unsigned)abs(utc_offset));

    if (abs(utc_offset) < 24) { utc_offset *= 100; } // allow for minutes offset too

    rig_debug(RIG_DEBUG_VERBOSE, "%s: utc_offset=%d\n", __func__, utc_offset);

    RETURNFUNC2(rig_set_clock(rig, year, mon, day, hour, min, sec, msec,
                              utc_offset));
}

/* '0xf9' */
declare_proto_rig(get_clock)
{
    //char option[64];
    int year, month, day, hour, min, sec, utc_offset, aoffset;
    int retval;
    double msec;

    ENTERFUNC2;

    //CHKSCN1ARG(sscanf(arg1, "%63s", option));

    retval = rig_get_clock(rig, &year, &month, &day, &hour, &min, &sec, &msec,
                           &utc_offset);
    aoffset = abs(utc_offset);
    fprintf(fout, "%04d-%02d-%02dT%02d:%02d:%06.3f%s%02d:%02d\n", year, month, day,
            hour, min, sec + msec / 1000, utc_offset >= 0 ? "+" : "-",
            aoffset / 100, aoffset % 100);

    return retval;
}

char rig_resp_sep = '\n';
/* '0xa0' */
declare_proto_rig(set_separator)
{
    CHKSCN1ARG(sscanf(arg1, "%c", &rig_resp_sep));
    rig_debug(RIG_DEBUG_ERR, "%s: arg1=%s, resp_sep=0x%x, %p\n", __func__, arg1,
              (unsigned int)rig_resp_sep, &rig_resp_sep);
    return RIG_OK;
}

/* '0xa1' */
declare_proto_rig(get_separator)
{
    char buf[32];

    if ((interactive && prompt) || (interactive && !prompt && ext_resp))
    {
        fprintf(fout, "%s: ", cmd->arg1);
    }

    sprintf(buf, "0x%x", rig_resp_sep);

    fprintf(fout, "%s\n", buf);
    return RIG_OK;
}

/* '0xa2' */
declare_proto_rig(set_lock_mode)
{
    int lock;
    int retval;
    rig_debug(RIG_DEBUG_TRACE, "%s:\n", __func__);
    CHKSCN1ARG(sscanf(arg1, "%d", &lock));

    if (is_rigctld)
    {
        rig_debug(RIG_DEBUG_ERR, "%s: rigctld lock\n", __func__);
        lock_mode = lock;
        retval = RIG_OK;
    }
    else
    {
        rig_debug(RIG_DEBUG_ERR, "%s: rig lock\n", __func__);
        retval =  rig_set_lock_mode(rig, lock);
    }

    return retval;
}

/* '0xa3' */
declare_proto_rig(get_lock_mode)
{
    int retval;
    int lock;

    rig_debug(RIG_DEBUG_TRACE, "%s:\n", __func__);

    if ((interactive && prompt) || (interactive && !prompt && ext_resp))
    {
        fprintf(fout, "%s: ", cmd->arg1);
    }

    if (is_rigctld)
    {
        rig_debug(RIG_DEBUG_ERR, "%s: rigctld lock\n", __func__);
        lock = lock_mode;
        retval = RIG_OK;
    }
    else
    {
        rig_debug(RIG_DEBUG_ERR, "%s: rig lock\n", __func__);
        retval = rig_get_lock_mode(rig, &lock);
    }

    if (retval != RIG_OK)
    {
        return retval;
    }

    fprintf(fout, "%d\n", lock);
    return RIG_OK;
}


static int parse_hex(const char *s, unsigned char *buf, int len)
{
    int i = 0;
    buf[0] = 0;
    char *s2 = strdup(s);
    char *p = strtok(s2, ";:");

    while (p)
    {
        unsigned int val;
        int n;

        if ((n = sscanf(p, "0x%2x", &val)) != 1)
        {
            n = sscanf(p, "x%2x", &val);
        }

        if (n == 0)
        {
            rig_debug(RIG_DEBUG_ERR, "%s: unable to parse 0x??; or x??; from '%s'\n",
                      __func__, s);
        }

        buf[i++] = val;
        p = strtok(NULL, ";:");
    }

    free(s2);
    return i;
}

// sends whatever is in s -- no additions or changes done
extern int netrigctl_send_raw(RIG *rig, char *s);
/* 0xa4 */
declare_proto_rig(send_raw)
{
    int result;
    int reply_len;
    unsigned char termbyte[1];
    unsigned char *term = NULL;
    unsigned char buf[200];
    unsigned char send[200];
    unsigned char *sendp = (unsigned char *)arg2;
    int arg2_len = strlen(arg2);
    int is_binary = 0;
    int buf_len = sizeof(buf);
    int val = 0;

    if (rig->caps->rig_model == RIG_MODEL_NETRIGCTL)
    {
        char netbuf[1024];
        int retval;
        snprintf(netbuf, sizeof(netbuf) - 1, "\\sendraw %s %s\n", arg2, arg3);
        rig_debug(RIG_DEBUG_ERR, "%s: calling netrigctl\n", __func__);
        retval = RIG_OK;
        return retval;
    }

    if (strcmp(arg1, ";") == 0) { termbyte[0] = ';'; term = termbyte; }
    else if (strcasecmp(arg1, "CR") == 0) { termbyte[0] = 0x0d; term = termbyte; }
    else if (strcasecmp(arg1, "LF") == 0) { termbyte[0] = 0x0a; term = termbyte; }
    else if (strcasecmp(arg1, "ICOM") == 0) { termbyte[0] = 0xfd; term = termbyte; }
    else if (sscanf(arg1, "0x%x", &val) == 1) { termbyte[0] = val; term = termbyte; }
    else if (sscanf(arg1, "%d", &val) == 1)
    {
        if (val < buf_len - 1)
        {
            // Reserve one byte more to allow padding with null
            buf_len = val + 1;
        }
        else
        {
            rig_debug(RIG_DEBUG_ERR,
                      "%s: response length %d is larger than maximum of %d bytes",
                      __func__, val, buf_len);
            return -RIG_EINVAL;
        }
    }
    else
    {
        rig_debug(RIG_DEBUG_ERR,
                  "%s: unknown arg1 val=%s, expected ';', 'CR', 'LF', 'ICOM', 0xFF (hex byte) or "
                  "# of bytes where 0 means no reply and -1 means unknown",
                  __func__, arg1);
        return -RIG_EINVAL;
    }

    if (strncmp(arg2, "0x", 2)  == 0 || arg2[0] == 'x')
    {
        arg2_len = parse_hex(arg2, send, sizeof(send));
        sendp = send;
        is_binary = 1;
    }

    rig_debug(RIG_DEBUG_TRACE, "%s:\n", __func__);

    result = rig_send_raw(rig, (unsigned char *)sendp, arg2_len, buf, buf_len,
                          term);

    if (result < 0)
    {
        return result;
    }

    reply_len = result;
    buf[buf_len + 1] = 0; // null terminate in case it's a string

    if ((interactive && prompt) || (interactive && !prompt && ext_resp))
    {
        fprintf(fout, "%s: ", cmd->arg3);
    }

    if (reply_len == 0)
    {
        fprintf(fout, "No answer\n");
    }
    else if (is_binary)
    {
        int i;

        for (i = 0; i < reply_len; ++i)
        {
            fprintf(fout, "0x%02x ", buf[i]);
        }

        fprintf(fout, "\n");
    }
    else
    {
        fprintf(fout, "%s\n", buf);
    }

    return RIG_OK;
}

declare_proto_rig(cm108_get_bit)
{
    rig_debug(RIG_DEBUG_TRACE, "%s:\n", __func__);

    int gpio = -1;
    int bit = -1;
    // try GPIO format first
    int n = sscanf(arg1, "GPIO%d", &gpio);

    if (n == 0 || n == EOF)
    {
        n = sscanf(arg1, "%d", &gpio);
    }

    if (n != 1) { return -RIG_EINVAL; }

    int retval = rig_cm108_get_bit(PTTPORT(rig), gpio, &bit);

    if (retval != RIG_OK)
    {
        rig_debug(RIG_DEBUG_ERR, "%s: %s\n", __func__, strerror(retval));
        return retval;
    }

    rig_debug(RIG_DEBUG_TRACE, "%s: gpio=%d\n", __func__, gpio);

    if ((interactive && prompt) || (interactive && !prompt && ext_resp))
    {
        fprintf(fout, "%s: ", cmd->arg2);
    }

    fprintf(fout, "%d (simulated)\n", 1);


    return RIG_OK;
}

declare_proto_rig(cm108_set_bit)
{
    rig_debug(RIG_DEBUG_TRACE, "%s:\n", __func__);
    int gpio, bit = -1;
    // try GPIO format first
    int n = sscanf(arg1, "GPIO%d", &gpio);

    if (n == 0 || n == EOF)
    {
        n = sscanf(arg1, "%d", &gpio);
    }

    if (n != 1) { return -RIG_EINVAL; }

    n = sscanf(arg2, "%d", &bit);

    if (n != 1) { return -RIG_EINVAL; }

    rig_debug(RIG_DEBUG_TRACE, "%s: set gpio=%d, bit=%d\n", __func__, gpio, bit);
    int retval = rig_cm108_set_bit(PTTPORT(rig), gpio, bit);

    if (retval != RIG_OK)
    {
        rig_debug(RIG_DEBUG_ERR, "%s: %s\n", __func__, strerror(retval));
    }

    return retval;
}

declare_proto_rig(get_conf)
{
    int ret;
    rig_debug(RIG_DEBUG_ERR, "%s: \n", __func__);

    if (arg1 == NULL || arg1[0] == '?')
    {
        dumpconf_list(rig, stdout);
        debugmsgsave[0] = 0;
        debugmsgsave2[0] = 0;
        return RIG_OK;
    }

    hamlib_token_t mytoken = rig_token_lookup(rig, arg1);

    if (mytoken == 0)
    {
        rig_debug(RIG_DEBUG_ERR, "%s: unknown token '%s' for this rig\n", __func__,
                  arg1);
        ret = -RIG_EINVAL;
    }
    else
    {
        char value[4096]; // no max value known -- should we limit it?
        ret = rig_get_conf(rig, mytoken, value);

        if (ret != RIG_OK)
        {
            return ret;
        }

        fprintf(fout, "%s=%s\n", arg1, value);
    }

    return (ret);
}

declare_proto_rig(set_conf)
{
    int ret;
    rig_debug(RIG_DEBUG_TRACE, "%s: called\n", __func__);


    if (arg1[0] == '?')
    {
        dumpconf_list(rig, fout);
        debugmsgsave[0] = 0;
        debugmsgsave2[0] = 0;
        return RIG_OK;
    }

    hamlib_token_t mytoken = rig_token_lookup(rig, arg1);

    if (mytoken == 0)
    {
        rig_debug(RIG_DEBUG_ERR, "%s: unknown token '%s' for this rig\n", __func__,
                  arg1);
        ret = -RIG_EINVAL;
    }
    else
    {
        ret = rig_set_conf(rig, rig_token_lookup(rig, arg1), arg2);
    }

    return (ret);
}

