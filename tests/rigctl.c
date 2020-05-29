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
#include "hamlibdatetime.h"

#ifdef HAVE_CONFIG_H
#  include "config.h"
#endif

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <ctype.h>
#include <errno.h>
#include <getopt.h>

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


#include <hamlib/rig.h>
#include "misc.h"
#include "iofunc.h"
#include "serial.h"
#include "sprintflst.h"
#include "dummy/dummy.h"
#include "rigctl_parse.h"

#define MAXNAMSIZ 32
#define MAXNBOPT 100    /* max number of different options */


static void usage(void);

/*
 * Reminder: when adding long options,
 *      keep up to date SHORT_OPTIONS, usage()'s output and man page. thanks.
 * NB: do NOT use -W since it's reserved by POSIX.
 * TODO: add an option to read from a file
 */
#define SHORT_OPTIONS "+m:r:p:d:P:D:s:c:t:lC:LuonvhVZ"
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
    {"ignore rig open error", 0, 0, 'Y'},
    {"debug-time-stamps", 0, 0, 'Z'},
#ifdef HAVE_READLINE_HISTORY
    {"read-history",    0, 0, 'i'},
    {"save-history",    0, 0, 'I'},
#endif
    {"verbose",         0, 0, 'v'},
    {"help",            0, 0, 'h'},
    {"version",         0, 0, 'V'},
    {0, 0, 0, 0}

};

#define MAXCONFLEN 1024

int main(int argc, char *argv[])
{
    RIG *my_rig;        /* handle to rig (instance) */
    rig_model_t my_model = RIG_MODEL_DUMMY;

    int retcode;        /* generic return code from functions */
    int exitcode;

    int verbose = 0;
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
    char *civaddr = NULL;   /* NULL means no need to set conf */
    char conf_parms[MAXCONFLEN] = "";
    int interactive;    /* if no cmd on command line, switch to interactive */
    int prompt = 1;         /* Print prompt in rigctl */
    int vfo_opt = 0;       /* vfo_opt = 0 means target VFO is 'currVFO' */
    char send_cmd_term = '\r';  /* send_cmd termination char */
    int ext_resp = 0;
    char resp_sep = '\n';
    int i;

    while (1)
    {
        int c;
        int option_index = 0;

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
        case 'h':
            usage();
            exit(0);

        case 'V':
            version();
            exit(0);

        case 'm':
            if (!optarg)
            {
                usage();    /* wrong arg count */
                exit(1);
            }

            my_model = atoi(optarg);
            break;

        case 'r':
            if (!optarg)
            {
                usage();    /* wrong arg count */
                exit(1);
            }

            rig_file = optarg;
            break;

        case 'p':
            if (!optarg)
            {
                usage();    /* wrong arg count */
                exit(1);
            }

            ptt_file = optarg;
            break;

        case 'd':
            if (!optarg)
            {
                usage();    /* wrong arg count */
                exit(1);
            }

            dcd_file = optarg;
            break;

        case 'P':
            if (!optarg)
            {
                usage();    /* wrong arg count */
                exit(1);
            }

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
            if (!optarg)
            {
                usage();    /* wrong arg count */
                exit(1);
            }

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
            if (!optarg)
            {
                usage();    /* wrong arg count */
                exit(1);
            }

            civaddr = optarg;
            break;

        case 't':
            if (!optarg)
            {
                usage();    /* wrong arg count */
                exit(1);
            }

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
            if (!optarg)
            {
                usage();    /* wrong arg count */
                exit(1);
            }

            serial_rate = atoi(optarg);
            break;

        case 'C':
            if (!optarg)
            {
                usage();    /* wrong arg count */
                exit(1);
            }

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

            strncat(conf_parms, optarg, MAXCONFLEN - strlen(conf_parms));
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
            break;

        case 'L':
            show_conf++;
            break;

        case 'l':
            rig_set_debug(verbose);
            list_models();
            exit(0);

        case 'u':
            dump_caps_opt++;
            break;

        case 'Y':
            ignore_rig_open_error = 1;

        case 'Z':
            rig_set_debug_time_stamp(1);
            break;

        default:
            usage();    /* unknown option? */
            exit(1);
        }
    }

    rig_set_debug(verbose);

    rig_debug(RIG_DEBUG_VERBOSE, "rigctl %s\nLast commit was %s\n", hamlib_version,
              HAMLIBDATETIME);
    rig_debug(RIG_DEBUG_VERBOSE, "%s",
              "Report bugs to <hamlib-developer@lists.sourceforge.net>\n\n");

    /*
     * at least one command on command line,
     * disable interactive mode
     */
    if (optind < argc)
    {
        interactive = 0;
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

    retcode = set_conf(my_rig, conf_parms);

    if (retcode != RIG_OK)
    {
        fprintf(stderr, "Config parameter error: %s\n", rigerror(retcode));
        exit(2);
    }

    if (rig_file)
    {
        strncpy(my_rig->state.rigport.pathname, rig_file, FILPATHLEN - 1);
    }

    /*
     * ex: RIG_PTT_PARALLEL and /dev/parport0
     */
    if (ptt_type != RIG_PTT_NONE)
    {
        my_rig->state.pttport.type.ptt = ptt_type;
    }

    if (dcd_type != RIG_DCD_NONE)
    {
        my_rig->state.dcdport.type.dcd = dcd_type;
    }

    if (ptt_file)
    {
        strncpy(my_rig->state.pttport.pathname, ptt_file, FILPATHLEN - 1);
    }

    if (dcd_file)
    {
        strncpy(my_rig->state.dcdport.pathname, dcd_file, FILPATHLEN - 1);
    }

    /* FIXME: bound checking and port type == serial */
    if (serial_rate != 0)
    {
        my_rig->state.rigport.parm.serial.rate = serial_rate;
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
        dumpcaps(my_rig, stdout);
        rig_cleanup(my_rig);    /* if you care about memory */
        exit(0);
    }

    i = 0;

    do   // we'll try 5 times and sleep 200ms between tries
    {
        retcode = rig_open(my_rig);

        if (retcode != RIG_OK)
        {
            hl_usleep(200000);
            rig_debug(RIG_DEBUG_TRACE, "%s: error opening rig, try#%d\n", __func__, i + 1);
        }
    }
    while (retcode != RIG_OK && ++i < 5);

    if (retcode != RIG_OK)
    {
        fprintf(stderr, "rig_open: error = %s \n", rigerror(retcode));

        if (!ignore_rig_open_error) { exit(2); }
    }

    if (verbose > 0)
    {
        printf("Opened rig model %u, '%s'\n",
               my_rig->caps->rig_model,
               my_rig->caps->model_name);
    }

    if (my_rig->caps->rig_model == RIG_MODEL_NETRIGCTL)
    {
        /* We automatically detect if we need to be in vfo mode or not */
        int rigctld_vfo_opt = netrigctl_get_vfo_mode(my_rig);
        vfo_opt = my_rig->state.vfo_opt = rigctld_vfo_opt;
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

            snprintf(hist_path, hist_path_size, "%s%s", hist_dir, hist_file);
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

    do
    {
        retcode = rigctl_parse(my_rig, stdin, stdout, argv, argc, NULL,
                               interactive, prompt, &vfo_opt, send_cmd_term,
                               &ext_resp, &resp_sep);

        if (retcode == 2)
        {
            exitcode = 2;
        }
    }
    while (retcode == 0 || retcode == 2 || retcode == -RIG_ENAVAIL);

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


void usage(void)
{
    printf("Usage: rigctl [OPTION]... [COMMAND]...\n"
           "Send COMMANDs to a connected radio transceiver or receiver.\n\n");


    printf(
        "  -m, --model=ID                select radio model number. See model list\n"
        "  -r, --rig-file=DEVICE         set device of the radio to operate on\n"
        "  -p, --ptt-file=DEVICE         set device of the PTT device to operate on\n"
        "  -d, --dcd-file=DEVICE         set device of the DCD device to operate on\n"
        "  -P, --ptt-type=TYPE           set type of the PTT device to operate on\n"
        "  -D, --dcd-type=TYPE           set type of the DCD device to operate on\n"
        "  -s, --serial-speed=BAUD       set serial speed of the serial port\n"
        "  -c, --civaddr=ID              set CI-V address, decimal (for Icom rigs only)\n"
        "  -t, --send-cmd-term=CHAR      set send_cmd command termination char\n"
        "  -C, --set-conf=PARM=VAL       set config parameters\n"
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
        "  -Y, --ignore_err              ignore rig_open errors\n"
        "  -Z, --debug-time-stamps       enable time stamps for debug messages\n"
        "  -h, --help                    display this help and exit\n"
        "  -V, --version                 output version information and exit\n\n"
    );

    usage_rig(stdout);

    printf("\nReport bugs to <hamlib-developer@lists.sourceforge.net>.\n");

}
