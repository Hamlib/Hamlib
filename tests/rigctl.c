/*
 * rigctl.c - (C) Stephane Fillod 2000-2010
 *            (C) Nate Bargmann 2003,2006,2008,2010,2011,2012,2013
 *            (C) The Hamlib Group 2002,2006,2007,2012
 *
 * This program test/control a radio using Hamlib.
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
/* SPDX-License-Identifier: GPL-2.0-or-later */

#include "hamlib/config.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <getopt.h>
#include <signal.h>

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
#  include <sys/stat.h>
#  define HST_SHRT_OPTS "iI"
#  if defined(HAVE_READLINE_HISTORY_H)
#    include <readline/history.h>
#  elif defined(HAVE_HISTORY_H)
#    include <history.h>
#  else                             /* !defined(HAVE_HISTORY_H) */
extern void add_history();
extern int write_history();
extern int read_history();
#  endif                            /* defined(HAVE_READLINE_HISTORY_H) */
#else
/* no history */
#define HST_SHRT_OPTS ""
#endif                              /* HAVE_READLINE_HISTORY */


#include "hamlib/rig.h"
#include "misc.h"
#include "rigctl_parse.h"
#include "riglist.h"
#include "token.h"

#define MAXNAMSIZ 32
#define MAXNBOPT 100    /* max number of different options */

/*
 * Prototypes
 */
static void usage(FILE *fout);
static void short_usage(FILE *fout);

/*
 * Reminder: when adding long options,
 *      keep up to date SHORT_OPTIONS, usage()'s output and man page. thanks.
 * NB: do NOT use -W since it's reserved by POSIX.
 * TODO: add an option to read from a file
 */
#define SHORT_OPTIONS "+m:r:p:d:P:D:s:c:t:lC:LuonvhVYZ!#"
static struct option long_options[] =
{
    {"model",           1, 0, 'm'},
    {"rig-file",        1, 0, 'r'},
    {"ptt-file",        1, 0, 'p'},
    {"dcd-file",        1, 0, 'd'},
    {"ptt-type",        1, 0, 'P'},
    {"dcd-type",        1, 0, 'D'},
    {"serial-speed",    1, 0, 's'},
    {"civaddr",         1, 0, 'c'},
    {"send-cmd-term",   1, 0, 't'},
    {"list",            0, 0, 'l'},
    {"set-conf",        1, 0, 'C'},
    {"show-conf",       0, 0, 'L'},
    {"dump-caps",       0, 0, 'u'},
    {"vfo",             0, 0, 'o'},
    {"no-restore-ai",   0, 0, 'n'},
    {"ignore-err",      0, 0, 'Y'},
    {"debug-time-stamps", 0, 0, 'Z'},
#ifdef HAVE_READLINE_HISTORY
    {"read-history",    0, 0, 'i'},
    {"save-history",    0, 0, 'I'},
#endif
    {"verbose",         0, 0, 'v'},
    {"help",            0, 0, 'h'},
    {"version",         0, 0, 'V'},
    {"cookie",          0, 0, '!'},
    {"skip-init",       0, 0, '#'},
    {0, 0, 0, 0}

};

extern char rig_resp_sep;
extern powerstat_t rig_powerstat;

static RIG *my_rig;        /* handle to rig (instance) */

#ifdef HAVE_SIG_ATOMIC_T
static sig_atomic_t volatile ctrl_c = 0;
#else
static int volatile ctrl_c = 0;
#endif

#define MAXCONFLEN 2048

#ifdef WIN32
static BOOL WINAPI CtrlHandler(DWORD fdwCtrlType)
{
    rig_debug(RIG_DEBUG_VERBOSE, "%s: called\n", __func__);

    switch (fdwCtrlType)
    {
    case CTRL_C_EVENT:
    case CTRL_CLOSE_EVENT:
        ctrl_c = 1;
        return TRUE;

    default:
        return FALSE;
    }
}
#else
static void signal_handler(int sig)
{
    switch (sig)
    {
    case SIGINT:
    case SIGTERM:
        fprintf(stderr, "\nTerminating application, caught signal %d\n", sig);
        // Close stdin to stop reading input
        fclose(stdin);
        ctrl_c = 1;
        break;

    default:
        /* do nothing */
        break;
    }
}
#endif

static void handle_error(enum rig_debug_level_e lvl, const char *msg)
{
    int e;
#ifdef __MINGW32__
    LPVOID lpMsgBuf;

    lpMsgBuf = (LPVOID)"Unknown error";
    e = WSAGetLastError();

    if (FormatMessage(FORMAT_MESSAGE_ALLOCATE_BUFFER
                      | FORMAT_MESSAGE_FROM_SYSTEM
                      | FORMAT_MESSAGE_IGNORE_INSERTS,
                      NULL, e,
                      MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT),
                      // Default language
                      (LPTSTR)&lpMsgBuf, 0, NULL))
    {

        rig_debug(lvl, "%s: Network error %d: %s\n", msg, e, (char *)lpMsgBuf);
        LocalFree(lpMsgBuf);
    }
    else
    {
        rig_debug(lvl, "%s: Network error %d\n", msg, e);
    }

#else
    e = errno;
    rig_debug(lvl, "%s: Network error %d: %s\n", msg, e, strerror(e));
#endif
}

int main(int argc, char *argv[])
{
    rig_model_t my_model = RIG_MODEL_DUMMY;

    int retcode;        /* generic return code from functions */
    int exitcode;

    int verbose = RIG_DEBUG_NONE;
    int show_conf = 0;
    int dump_caps_opt = 0;
    int ignore_rig_open_error = 0;

#ifdef HAVE_READLINE_HISTORY
    int rd_hist = 0;
    int sv_hist = 0;
    const char *hist_dir = NULL;
    const char hist_file[] = "/.rigctl_history";
    char *hist_path = NULL;
#endif  /* HAVE_READLINE_HISTORY */

    const char *rig_file = NULL, *ptt_file = NULL, *dcd_file = NULL;
    ptt_type_t ptt_type = RIG_PTT_NONE;
    dcd_type_t dcd_type = RIG_DCD_NONE;
    int serial_rate = 0;
    const char *civaddr = NULL;   /* NULL means no need to set conf */
    char conf_parms[MAXCONFLEN] = "";
    int interactive;    /* if no cmd on command line, switch to interactive */
    int prompt = 1;         /* Print prompt in rigctl */
    int vfo_opt = 0;       /* vfo_opt = 0 means target VFO is 'currVFO' */
    char send_cmd_term = '\r';  /* send_cmd termination char */
    int ext_resp = 0;
    int i;
    char rigstartup[1024];
    char vbuf[1024];
    rig_powerstat = RIG_POWER_ON; // defaults to power on
    struct timespec powerstat_check_time;
#if HAVE_SIGACTION
    struct sigaction act;
#endif

    int err = setvbuf(stderr, vbuf, _IOFBF, sizeof(vbuf));

    if (err) { rig_debug(RIG_DEBUG_ERR, "%s: setvbuf err=%s\n", __func__, strerror(err)); }

    rig_set_debug(verbose);
    while (1)
    {
        int c;
        int option_index = 0;
        char dummy[2];

        c = getopt_long(argc,
                        argv,
                        SHORT_OPTIONS HST_SHRT_OPTS,
                        long_options,
                        &option_index);

        if (c == -1)
        {
            break;
        }

        switch (c)
        {
        case '#':
            skip_init = 1;
            break;

        case '!':
            cookie_use = 1;
            break;

        case 'h':
            usage(stdout);
            exit(0);

        case 'V':
            printf("rigctl %s\n", hamlib_version2);
            exit(0);

        case 'm':
            my_model = atoi(optarg);
            break;

        case 'r':
            rig_file = optarg;
            break;

        case 'p':
            ptt_file = optarg;
            break;

        case 'd':
            dcd_file = optarg;
            break;

        case 'P':
            if (!strcmp(optarg, "RIG"))
            {
                ptt_type = RIG_PTT_RIG;
            }
            else if (!strcmp(optarg, "DTR"))
            {
                ptt_type = RIG_PTT_SERIAL_DTR;
            }
            else if (!strcmp(optarg, "RTS"))
            {
                ptt_type = RIG_PTT_SERIAL_RTS;
            }
            else if (!strcmp(optarg, "PARALLEL"))
            {
                ptt_type = RIG_PTT_PARALLEL;
            }
            else if (!strcmp(optarg, "CM108"))
            {
                ptt_type = RIG_PTT_CM108;
            }
            else if (!strcmp(optarg, "GPIO"))
            {
                ptt_type = RIG_PTT_GPIO;
            }
            else if (!strcmp(optarg, "GPION"))
            {
                ptt_type = RIG_PTT_GPION;
            }
            else if (!strcmp(optarg, "NONE"))
            {
                ptt_type = RIG_PTT_NONE;
            }
            else
            {
                puts("Unrecognised PTT type, using NONE");
                ptt_type = RIG_PTT_NONE;
            }

            break;

        case 'D':
            if (!strcmp(optarg, "RIG"))
            {
                dcd_type = RIG_DCD_RIG;
            }
            else if (!strcmp(optarg, "DSR"))
            {
                dcd_type = RIG_DCD_SERIAL_DSR;
            }
            else if (!strcmp(optarg, "CTS"))
            {
                dcd_type = RIG_DCD_SERIAL_CTS;
            }
            else if (!strcmp(optarg, "CD"))
            {
                dcd_type = RIG_DCD_SERIAL_CAR;
            }
            else if (!strcmp(optarg, "PARALLEL"))
            {
                dcd_type = RIG_DCD_PARALLEL;
            }
            else if (!strcmp(optarg, "CM108"))
            {
                dcd_type = RIG_DCD_CM108;
            }
            else if (!strcmp(optarg, "GPIO"))
            {
                dcd_type = RIG_DCD_GPIO;
            }
            else if (!strcmp(optarg, "GPION"))
            {
                dcd_type = RIG_DCD_GPION;
            }
            else if (!strcmp(optarg, "NONE"))
            {
                dcd_type = RIG_DCD_NONE;
            }
            else
            {
                puts("Unrecognised DCD type, using NONE");
                dcd_type = RIG_DCD_NONE;
            }

            break;

        case 'c':
            civaddr = optarg;
            break;

        case 't':
            if (strlen(optarg) > 1)
            {
                send_cmd_term = strtol(optarg, NULL, 0);
            }
            else
            {
                send_cmd_term = optarg[0];
            }

            break;

        case 's':
            if (sscanf(optarg, "%d%1s", &serial_rate, dummy) != 1)
            {
                fprintf(stderr, "Invalid baud rate of %s\n", optarg);
                exit(1);
            }

            break;

        case 'C':
            if (*conf_parms != '\0')
            {
                strcat(conf_parms, ",");
            }

            if (strlen(conf_parms) + strlen(optarg) > MAXCONFLEN - 24)
            {
                printf("Length of conf_parms exceeds internal maximum of %d\n",
                       MAXCONFLEN - 24);
                return 1;
            }

            strncat(conf_parms, optarg, MAXCONFLEN - strlen(conf_parms) - 1);
            break;

        case 'o':
            vfo_opt = 1;
            break;

        case 'n':
            rig_no_restore_ai();
            break;
#ifdef HAVE_READLINE_HISTORY

        case 'i':
            rd_hist++;
            break;

        case 'I':
            sv_hist++;
            break;
#endif  /* HAVE_READLINE_HISTORY */

        case 'v':
            verbose++;
            rig_set_debug(verbose);
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

        case 'Y':
            ignore_rig_open_error = 1;
            break;

        case 'Z':
            rig_set_debug_time_stamp(1);
            break;

        default:
            /* unknown getopt option */
            short_usage(stderr);
            exit(1);
        }
    }

    SNPRINTF(rigstartup, sizeof(rigstartup), "%s(%d) Startup:", __FILE__, __LINE__);

    for (i = 0; i < argc; ++i) { strcat(rigstartup, " "); strcat(rigstartup, argv[i]); }

    rig_debug(RIG_DEBUG_VERBOSE, "%s\n", rigstartup);


    rig_debug(RIG_DEBUG_VERBOSE, "rigctl %s\n", hamlib_version2);
    rig_debug(RIG_DEBUG_VERBOSE, "%s",
              "Report bugs to <hamlib-developer@lists.sourceforge.net>\n\n");

    /*
     * at least one command on command line,
     * disable interactive mode
     */
    if (optind < argc)
    {
        interactive = 0;
        // skip_init here caused segfault on rigctl -m 2041 -r /dev/pts/4 M CW 250 b 73
        //skip_init = 1;
    }
    else
    {
        interactive = 1;
    }

    my_rig = rig_init(my_model);

    if (!my_rig)
    {
        fprintf(stderr,
                "Unknown rig num %u, or initialization error.\n",
                my_model);
        fprintf(stderr, "Please check with --list option.\n");
        exit(2);
    }

    my_rig->caps->ptt_type = ptt_type;
    const char *token = strtok(conf_parms, ",");
    struct rig_state *rs = STATE(my_rig);

    while (token)
    {
        char mytoken[100], myvalue[100];
        hamlib_token_t lookup;
        sscanf(token, "%99[^=]=%99s", mytoken, myvalue);
        //printf("mytoken=%s,myvalue=%s\n",mytoken, myvalue);
        lookup = rig_token_lookup(my_rig, mytoken);

        if (lookup == 0)
        {
            rig_debug(RIG_DEBUG_ERR, "%s: no such token as '%s'\n", __func__, mytoken);
            token = strtok(NULL, ",");
            continue;
        }

        retcode = rig_set_conf(my_rig, lookup, myvalue);

        if (retcode != RIG_OK)
        {
            fprintf(stderr, "Config parameter error: %s\n", rigerror(retcode));
            exit(2);
        }

        ptt_type = my_rig->caps->ptt_type; // in case we set the ptt_type with set_conf

        token = strtok(NULL, ",");
    }

    if (rig_file)
    {
        rig_set_conf(my_rig, TOK_PATHNAME, rig_file);
    }

    /*
     * ex: RIG_PTT_PARALLEL and /dev/parport0
     */
    if (ptt_type != RIG_PTT_NONE)
    {
        PTTPORT(my_rig)->type.ptt = ptt_type;
    }

    if (dcd_type != RIG_DCD_NONE)
    {
        DCDPORT(my_rig)->type.dcd = dcd_type;
    }

    if (ptt_file)
    {
        strncpy(PTTPORT(my_rig)->pathname, ptt_file, HAMLIB_FILPATHLEN - 1);

        // default to RTS when ptt_type is not specified
        if (ptt_type == RIG_PTT_NONE)
        {
            rig_debug(RIG_DEBUG_VERBOSE, "%s: defaulting to RTS PTT\n", __func__);
            my_rig->caps->ptt_type = RIG_PTT_SERIAL_RTS;
        }
    }

    if (dcd_file)
    {
        strncpy(DCDPORT(my_rig)->pathname, dcd_file, HAMLIB_FILPATHLEN - 1);
    }

    /* FIXME: bound checking and port type == serial */
    if (serial_rate != 0)
    {
        RIGPORT(my_rig)->parm.serial.rate = serial_rate;
        rs->rigport_deprecated.parm.serial.rate = serial_rate;
    }

    if (civaddr)
    {
        rig_set_conf(my_rig, rig_token_lookup(my_rig, "civaddr"), civaddr);
    }

    /*
     * print out conf parameters
     */
    if (show_conf)
    {
        dumpconf(my_rig, stdout);
    }

    /*
     * print out capabilities, and exit immediately
     * We may be interested only in only caps, and rig_open may fail.
     */
    if (dump_caps_opt)
    {

        // if rigctld then we need to open to get the rig caps
        if (my_model == RIG_MODEL_NETRIGCTL)
        {
            int ret;
            rig_set_debug(verbose);
            my_rig = rig_init(my_model);

            if ((ret = rig_open(my_rig)) != RIG_OK)
            {
                fprintf(stderr, "Unable to open rigctld: %s\n", rigerror(ret));
                exit(1);
            }

            rig_close(my_rig);
            dumpstate(my_rig, stdout);
            rig_close(my_rig);
            exit(0);
        }

        dumpcaps(my_rig, stdout);
        rig_cleanup(my_rig);    /* if you care about memory */
        exit(0);
    }

    retcode = rig_open(my_rig);

    if (retcode != RIG_OK)
    {
//        fprintf(stderr, "rig_open: error = %s %s \n", rig_file,
//                rigerror(retcode));

        if (!ignore_rig_open_error) { exit(2); }
    }

    if (verbose > 0)
    {
        printf("Opened rig model %u, '%s'\n",
               my_rig->caps->rig_model,
               my_rig->caps->model_name);
    }

    if (!skip_init && my_rig->caps->get_powerstat)
    {
        rig_get_powerstat(my_rig, &rig_powerstat);
        rs->powerstat = rig_powerstat;
    }

    if (my_rig->caps->rig_model == RIG_MODEL_NETRIGCTL)
    {
        /* We automatically detect if we need to be in vfo mode or not */
        int rigctld_vfo_opt = netrigctl_get_vfo_mode(my_rig);
        vfo_opt = rs->vfo_opt = rigctld_vfo_opt;
        rig_debug(RIG_DEBUG_TRACE, "%s vfo_opt=%d\n", __func__, vfo_opt);
    }

    rig_debug(RIG_DEBUG_VERBOSE,
              "Backend version: %s, Status: %s\n",
              my_rig->caps->version,
              rig_strstatus(my_rig->caps->status));

    exitcode = 0;

#ifdef HAVE_LIBREADLINE

    if (interactive && prompt)
    {
        rl_readline_name = "rigctl";

#ifdef HAVE_READLINE_HISTORY
        using_history();    /* Initialize Readline History */

        if (rd_hist || sv_hist)
        {
            int hist_path_size;
            struct stat hist_dir_stat;

            if (!(hist_dir = getenv("RIGCTL_HIST_DIR")))
            {
                hist_dir = getenv("HOME");
            }

            if (((stat(hist_dir, &hist_dir_stat) == -1) && (errno == ENOENT))
                    || !(S_ISDIR(hist_dir_stat.st_mode)))
            {
                fprintf(stderr, "Warning: %s is not a directory!\n", hist_dir);
            }

            hist_path_size = sizeof(char) * (strlen(hist_dir) + strlen(hist_file) + 1);
            hist_path = (char *)calloc(hist_path_size, sizeof(char));

            if (hist_path)
            {
                SNPRINTF(hist_path, hist_path_size, "%s%s", hist_dir, hist_file);
            }
	    else
            {
                fprintf(stderr, "Allocation failed - no readline history\n");
            }
        }

        if (rd_hist && hist_path)
        {
            if (read_history(hist_path) == ENOENT)
            {
                fprintf(stderr,
                        "Warning: Could not read history from %s\n",
                        hist_path);
            }
        }

#endif  /* HAVE_READLINE_HISTORY */
    }

#endif  /* HAVE_LIBREADLINE */
    int rig_opened = 1;  // our rig is already open

#if HAVE_SIGACTION

#ifdef SIGPIPE
    /* Ignore SIGPIPE as we will handle it at the write()/send() calls
       that will consequently fail with EPIPE. All child threads will
       inherit this disposition which is what we want. */
    memset(&act, 0, sizeof act);
    act.sa_handler = SIG_IGN;
    act.sa_flags = SA_RESTART;

    if (sigaction(SIGPIPE, &act, NULL))
    {
        handle_error(RIG_DEBUG_ERR, "sigaction SIGPIPE");
    }

#endif

#ifdef SIGINT
    memset(&act, 0, sizeof act);
    act.sa_handler = signal_handler;

    if (sigaction(SIGINT, &act, NULL))
    {
        handle_error(RIG_DEBUG_ERR, "sigaction SIGINT");
    }

#endif
#ifdef SIGTERM
    memset(&act, 0, sizeof act);
    act.sa_handler = signal_handler;

    if (sigaction(SIGTERM, &act, NULL))
    {
        handle_error(RIG_DEBUG_ERR, "sigaction SIGTERM");
    }

#endif
#elif defined (WIN32)

    if (!SetConsoleCtrlHandler(CtrlHandler, TRUE))
    {
        handle_error(RIG_DEBUG_ERR, "SetConsoleCtrlHandler");
    }

#elif HAVE_SIGNAL
#ifdef SIGPIPE

    if (SIG_ERR == signal(SIGPIPE, SIG_IGN))
    {
        handle_error(RIG_DEBUG_ERR, "signal SIGPIPE");
    }

#endif
#ifdef SIGINT

    if (SIG_ERR == signal(SIGINT, signal_handler))
    {
        handle_error(RIG_DEBUG_ERR, "signal SIGINT");
    }

#endif
#ifdef SIGTERM

    if (SIG_ERR == signal(SIGTERM, signal_handler))
    {
        handle_error(RIG_DEBUG_ERR, "signal SIGTERM");
    }

#endif
#endif

    elapsed_ms(&powerstat_check_time, HAMLIB_ELAPSED_SET);

    do
    {
        if (!rig_opened)
        {
            // rig may have closed on us to try once to reopen
            retcode = rig_open(my_rig);
            rig_debug(RIG_DEBUG_WARN, "%s: rig_open again retcode=%d\n", __func__, retcode);
        }

        retcode = rigctl_parse(my_rig, stdin, stdout, argv, argc, NULL,
                               interactive, prompt, &vfo_opt, send_cmd_term,
                               &ext_resp, &rig_resp_sep, 0);

        // If we get a timeout, the rig might be powered off
        // Update our power status in case power gets turned off
        // Check power status if rig is powered off, but not more often than once per second
        if (my_rig->caps->get_powerstat && (retcode == -RIG_ETIMEOUT ||
                                            (retcode == -RIG_EPOWER
                                             && elapsed_ms(&powerstat_check_time, HAMLIB_ELAPSED_GET) >= 1000)))
        {
            powerstat_t powerstat;
            rig_get_powerstat(my_rig, &powerstat);
            rig_powerstat = powerstat;

            if (powerstat == RIG_POWER_OFF || powerstat == RIG_POWER_STANDBY)
            {
                retcode = -RIG_EPOWER;
            }

            elapsed_ms(&powerstat_check_time, HAMLIB_ELAPSED_SET);
        }

        // if we get a hard error we try to reopen the rig again
        // this should cover short dropouts that can occur
        if (retcode < 0 && !RIG_IS_SOFT_ERRCODE(retcode))
        {
            int retry = 3;
            rig_debug(RIG_DEBUG_ERR, "%s: i/o error\n", __func__);

            do
            {
                retcode = rig_close(my_rig);
                hl_usleep(1000 * 1000);
                rig_debug(RIG_DEBUG_ERR, "%s: rig_close retcode=%d\n", __func__, retcode);
                retcode = rig_open(my_rig);
                rig_opened = retcode == RIG_OK ? 1 : 0;
                rig_debug(RIG_DEBUG_ERR, "%s: rig_open retcode=%d\n", __func__, retcode);
            }
            while (retry-- > 0 && retcode != RIG_OK);
        }
    }
    while (!ctrl_c && (retcode == RIG_OK || RIG_IS_SOFT_ERRCODE(retcode)));

    if (interactive && prompt)
    {
#ifdef HAVE_READLINE_HISTORY

        if (sv_hist && hist_path)
        {
            if (write_history(hist_path) == ENOENT)
            {
                fprintf(stderr,
                        "\nWarning: Could not write history to %s\n",
                        hist_path);
            }
        }

        if ((rd_hist || sv_hist) && hist_path)
        {
            free(hist_path);
            hist_path = (char *)NULL;
        }

#endif  /* HAVE_READLINE_HISTORY */
    }

    rig_close(my_rig);   /* close port */
    rig_cleanup(my_rig); /* if you care about memory */

    return exitcode;
}


static void usage(FILE *fout)
{
    fprintf(fout, "Usage: rigctl [OPTION]... [COMMAND]...\n"
           "Send COMMANDs to a connected radio transceiver or receiver.\n\n");

    fprintf(fout,
        "  -m, --model=ID                select radio model number. See model list (-l)\n"
        "  -r, --rig-file=DEVICE         set device of the radio to operate on\n"
        "  -p, --ptt-file=DEVICE         set device of the PTT device to operate on\n"
        "  -d, --dcd-file=DEVICE         set device of the DCD device to operate on\n"
        "  -P, --ptt-type=TYPE           set type of the PTT device to operate on\n"
        "  -D, --dcd-type=TYPE           set type of the DCD device to operate on\n"
        "  -s, --serial-speed=BAUD       set serial speed of the serial port\n"
        "  -c, --civaddr=ID              set CI-V address, decimal (for Icom rigs only)\n"
        "  -t, --send-cmd-term=CHAR      set send_cmd command termination char\n"
        "  -C, --set-conf=PARM=VAL[,...] set config parameters\n"
        "  -L, --show-conf               list all config parameters\n"
        "  -l, --list                    list all model numbers and exit\n"
        "  -u, --dump-caps               dump capabilities and exit\n"
        "  -o, --vfo                     do not default to VFO_CURR, require extra vfo arg\n"
        "  -n, --no-restore-ai           do not restore auto information mode on rig\n"
#ifdef HAVE_READLINE_HISTORY
        "  -i, --read-history            read prior interactive session history\n"
        "  -I, --save-history            save current interactive session history\n"
#endif
        "  -v, --verbose                 set verbose mode, cumulative (-v to -vvvvv)\n"
        "  -Y, --ignore-err              ignore rig_open errors\n"
        "  -Z, --debug-time-stamps       enable time stamps for debug messages\n"
        "  -h, --help                    display this help and exit\n"
        "  -V, --version                 output version information and exit\n"
        "  -!, --cookie                  use cookie control\n"
        "  -#, --skip-init               skip rig initialization\n"
        "  -                             read commands from standard input\n"
        "\n"
    );

    usage_rig(fout);
}


static void short_usage(FILE *fout)
{
    fprintf(fout, "Usage: rigctl [OPTION]... [-m ID] [-r DEVICE] [-s BAUD] [COMMAND...|-]\n");
    fprintf(fout, "Send COMMANDs to a connected radio transceiver or receiver.\n\n");
    fprintf(fout, "Type: rigctl --help for extended usage.\n");
}
