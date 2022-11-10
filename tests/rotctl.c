/*
 * rotctl.c - (C) Stephane Fillod 2000-2010
 *            (C) Nate Bargmann 2003,2007,2010,2011,2012,2013
 *            (C) The Hamlib Group 2002,2006
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
#include <hamlib/config.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
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
#  define HST_SHRT_OPTS ""
#endif                              /* HAVE_READLINE_HISTORY */


#include <hamlib/rotator.h>
#include "misc.h"

#include "rotctl_parse.h"

/*
 * Prototypes
 */
void usage();

/*
 * Reminder: when adding long options,
 * keep up to date SHORT_OPTIONS, usage()'s output and man page. thanks.
 * NB: do NOT use -W since it's reserved by POSIX.
 * TODO: add an option to read from a file
 */
#define SHORT_OPTIONS "+m:r:R:s:C:o:O:t:LvhVluZ"
static struct option long_options[] =
{
    {"model",           1, 0, 'm'},
    {"rot-file",        1, 0, 'r'},
    {"rot-file2",       1, 0, 'R'},
    {"serial-speed",    1, 0, 's'},
    {"send-cmd-term",   1, 0, 't'},
    {"list",            0, 0, 'l'},
    {"set-conf",        1, 0, 'C'},
    {"set-azoffset",    1, 0, 'o'},
    {"set-eloffset",    1, 0, 'O'},
    {"show-conf",       0, 0, 'L'},
    {"dump-caps",       0, 0, 'u'},
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

/* variable for readline support */
#ifdef HAVE_LIBREADLINE
static const int have_rl = 1;
#endif

int main(int argc, char *argv[])
{
    ROT *my_rot;        /* handle to rot (instance) */
    rot_model_t my_model = ROT_MODEL_DUMMY;

    int retcode;        /* generic return code from functions */
    int exitcode;

    int verbose = 0;
    int show_conf = 0;
    int dump_caps_opt = 0;

#ifdef HAVE_READLINE_HISTORY
    int rd_hist = 0;
    int sv_hist = 0;
    const char *hist_dir = NULL;
    const char hist_file[] = "/.rotctl_history";
    char *hist_path = NULL;
#endif  /* HAVE_READLINE_HISTORY */

    const char *rot_file = NULL;
    const char *rot_file2 = NULL; // for 2nd controller for RT21
    int serial_rate = 0;
    char conf_parms[MAXCONFLEN] = "";
    int interactive = 1; /* if no cmd on command line, switch to interactive */
    int prompt = 1;      /* Print prompt in rotctl */
    char send_cmd_term = '\r';  /* send_cmd termination char */
    azimuth_t az_offset = 0;
    elevation_t el_offset = 0;

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

            rot_file = optarg;
            break;

        case 'R':
            if (!optarg)
            {
                usage();    /* wrong arg count */
                exit(1);
            }

            rot_file2 = optarg;
            break;

        case 's':
            if (!optarg)
            {
                usage();    /* wrong arg count */
                exit(1);
            }

            if (sscanf(optarg, "%d%1s", &serial_rate, dummy) != 1)
            {
                fprintf(stderr, "Invalid baud rate of %s\n", optarg);
                exit(1);
            }

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

        case 't':
            if (!optarg)
            {
                usage();        /* wrong arg count */
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

        case 'o':
            if (!optarg)
            {
                usage();    /* wrong arg count */
                exit(1);
            }

            az_offset = atof(optarg);
            break;

        case 'O':
            if (!optarg)
            {
                usage();    /* wrong arg count */
                exit(1);
            }

            el_offset = atof(optarg);
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
            rig_set_debug(0);
            list_models();
            exit(0);

        case 'u':
            dump_caps_opt++;
            break;

        case 'Z':
            rig_set_debug_time_stamp(1);
            break;

        default:
            usage();    /* unknown option? */
            exit(1);
        }
    }

    rig_set_debug(verbose);

    rig_debug(RIG_DEBUG_VERBOSE, "rotctl %s\n", hamlib_version2);
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

    my_rot = rot_init(my_model);

    if (!my_rot)
    {
        fprintf(stderr,
                "Unknown rot num %d, or initialization error.\n",
                my_model);

        fprintf(stderr, "Please check with --list option.\n");
        exit(2);
    }

    retcode = set_conf(my_rot, conf_parms);

    if (retcode != RIG_OK)
    {
        fprintf(stderr, "Config parameter error: %s\n", rigerror(retcode));
        exit(2);
    }

    if (rot_file)
    {
        strncpy(my_rot->state.rotport.pathname, rot_file, HAMLIB_FILPATHLEN - 1);
    }

    if (rot_file2)
    {
        strncpy(my_rot->state.rotport2.pathname, rot_file2, HAMLIB_FILPATHLEN - 1);
    }

    /* FIXME: bound checking and port type == serial */
    if (serial_rate != 0)
    {
        my_rot->state.rotport.parm.serial.rate = serial_rate;
    }

    /*
     * print out conf parameters
     */
    if (show_conf)
    {
        rot_token_foreach(my_rot, print_conf_list, (rig_ptr_t)my_rot);
    }

    retcode = rot_open(my_rot);

    if (retcode != RIG_OK)
    {
        fprintf(stderr, "rot_open: error = %s \n", rigerror(retcode));
        exit(2);
    }

    /*
     * Print out capabilities, and exits immediately as we may be interested
     * only in caps, and rig_open may fail.
     */
    if (dump_caps_opt)
    {
        dumpcaps_rot(my_rot, stdout);
        rot_cleanup(my_rot);    /* if you care about memory */
        exit(0);
    }

    my_rot->state.az_offset = az_offset;
    my_rot->state.el_offset = el_offset;

    if (verbose > 0)
    {
        printf("Opened rot model %d, '%s'\n",
               my_rot->caps->rot_model,
               my_rot->caps->model_name);
    }

    rig_debug(RIG_DEBUG_VERBOSE,
              "Backend version: %s, Status: %s\n",
              my_rot->caps->version,
              rig_strstatus(my_rot->caps->status));

    exitcode = 0;

#ifdef HAVE_LIBREADLINE

    if (interactive && prompt && have_rl)
    {
        rl_readline_name = "rotctl";
#ifdef HAVE_READLINE_HISTORY
        using_history();    /* Initialize Readline History */

        if (rd_hist || sv_hist)
        {
            int hist_path_size;
            struct stat hist_dir_stat;

            if (!(hist_dir = getenv("ROTCTL_HIST_DIR")))
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

            SNPRINTF(hist_path, hist_path_size, "%s%s", hist_dir, hist_file);

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
        retcode = rotctl_parse(my_rot, stdin, stdout, argv, argc,
                               interactive, prompt, send_cmd_term);

        if (retcode == 2)
        {
            exitcode = 2;
        }
    }
    while (retcode == 0 || retcode == 2);

#ifdef HAVE_LIBREADLINE

    if (interactive && prompt && have_rl)
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

#endif  /* HAVE_LIBREADLINE */
    rot_close(my_rot);   /* close port */
    rot_cleanup(my_rot); /* if you care about memory */

    return exitcode;
}


void usage()
{
    printf("Usage: rotctl [OPTION]... [COMMAND]...\n"
           "Send COMMANDs to a connected antenna rotator.\n\n");

    printf(
        "  -m, --model=ID                select rotator model number. See model list\n"
        "  -r, --rot-file=DEVICE         set device of the rotator to operate on\n"
        "  -R, --rot-file2=DEVICE        set device of the 2nd rotator controller to operate on\n"
        "  -s, --serial-speed=BAUD       set serial speed of the serial port\n"
        "  -t, --send-cmd-term=CHAR      set send_cmd command termination char\n"
        "  -C, --set-conf=PARM=VAL       set config parameters\n"
        "  -o, --set-azoffset==VAL       set offset for azimuth\n"
        "  -O, --set-eloffset==VAL       set offset for elevation\n"
        "  -L, --show-conf               list all config parameters\n"
        "  -l, --list                    list all model numbers and exit\n"
        "  -u, --dump-caps               dump capabilities and exit\n"
#ifdef HAVE_READLINE_HISTORY
        "  -i, --read-history            read prior interactive session history\n"
        "  -I, --save-history            save current interactive session history\n"
#endif
        "  -v, --verbose                 set verbose mode, cumulative\n"
        "  -Z, --debug-time-stamps       enable time stamps for debug messages\n"
        "  -h, --help                    display this help and exit\n"
        "  -V, --version                 output version information and exit\n\n"
    );

    usage_rot(stdout);

    printf("\nReport bugs to <hamlib-developer@lists.sourceforge.net>.\n");
}
