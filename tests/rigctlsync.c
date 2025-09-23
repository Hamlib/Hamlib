/*
 * rigctlsync.c - (C) Stephane Fillod 2000-2011
 *             (C) The Hamlib Group 2012
 *             (C) Nate Bargmann 2008,2010,2011,2012,2013
 *             (C) Michael Black W9MDB 2023 - derived from rigctlcom.c
 *
 *   This program will synchronize frequency from one rig to another
 *   Implemented for AirSpy SDR# to keep freq synced with a real rig
 *   It simply polls the real rig and when freq changes sends it to SDR# (or whatever rig is hooked up)
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

#ifdef WIN32
#define WIN32_LEAN_AND_MEAN
// cppcheck-suppress *
#include <windows.h>
#endif

// cppcheck-suppress *
#include <stdio.h>
// cppcheck-suppress *
#include <stdlib.h>
// cppcheck-suppress *
#include <string.h>
// cppcheck-suppress *
#include <signal.h>

// cppcheck-suppress *
#include <getopt.h>

// cppcheck-suppress *
#include <sys/types.h>

#ifdef HAVE_NETINET_IN_H
// cppcheck-suppress *
#  include <netinet/in.h>
#endif

#ifdef HAVE_ARPA_INET_H
// cppcheck-suppress *
#  include <arpa/inet.h>
#endif

#ifdef HAVE_SYS_SOCKET_H
// cppcheck-suppress *
#  include <sys/socket.h>
#elif HAVE_WS2TCPIP_H
#  include <ws2tcpip.h>
#  include <fcntl.h>
#  if defined(HAVE_WSPIAPI_H)
#    include <wspiapi.h>
#  endif
#endif

#include "hamlib/rig.h"
#include "rigctl_parse.h"
#include "riglist.h"
#include "sleep.h"

/*
 * Reminder: when adding long options,
 *      keep up to date SHORT_OPTIONS, usage()'s output and man page. thanks.
 * NB: do NOT use -W since it's reserved by POSIX.
 * TODO: add an option to read from a file
 */
#define SHORT_OPTIONS "Bm:M:r:R:p:d:P:D:s:S:c:C:lLuvhVZ"
static struct option long_options[] =
{
    {"mapa2b",          0, 0, 'B'},
    {"model",           1, 0, 'm'},
    {"rig-file",        1, 0, 'r'},
    {"rig-file2",       1, 0, 'R'},
    {"ptt-file",        1, 0, 'p'},
    {"dcd-file",        1, 0, 'd'},
    {"ptt-type",        1, 0, 'P'},
    {"dcd-type",        1, 0, 'D'},
    {"serial-speed",    1, 0, 's'},
    {"serial-speed2",   1, 0, 'S'},
    {"civaddr",         1, 0, 'c'},
    {"set-conf",        1, 0, 'C'},
    {"list",            0, 0, 'l'},
    {"show-conf",       0, 0, 'L'},
    {"dump-caps",       0, 0, 'u'},
    {"verbose",         0, 0, 'v'},
    {"help",            0, 0, 'h'},
    {"version",         0, 0, 'V'},
    {"debug-time-stamps", 0, 0, 'Z'},
    {0, 0, 0, 0}
};

static void usage(FILE *fout);
static RIG *my_rig;             /* handle to rig */
static RIG
*my_rig_sync;        /* rig the gets synchronized -- freq only for now */
static int verbose = RIG_DEBUG_NONE;
/* CW Skimmer can only set VFOA */
/* IC7300 for example can run VFOA on FM and VFOB on CW */
/* So -A/--mapa2b changes set_freq on VFOA to VFOB */
/* This allows working CW Skimmer in split mode and transmit on VFOB */
static int mapa2b;              /* maps set_freq on VFOA to VFOB instead */

#ifdef HAVE_SIG_ATOMIC_T
static sig_atomic_t volatile ctrl_c;
#else
static int volatile ctrl_c;
#endif

#define MAXCONFLEN 2048

#if 0
# ifdef WIN32
static BOOL WINAPI CtrlHandler(DWORD fdwCtrlType)
{
    rig_debug(RIG_DEBUG_VERBOSE, "CtrlHandler called\n");

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
# else
static void signal_handler(int sig)
{
    switch (sig)
    {
    case SIGINT:
        ctrl_c = 1;
        break;

    default:
        /* do nothing */
        break;
    }
}
# endif  /* ifdef WIN32 */
#endif   /* if 0 */

#if 0
static void handle_error(enum rig_debug_level_e lvl, const char *msg)
{
    int e;
# ifdef __MINGW32__
    LPVOID lpMsgBuf;

    lpMsgBuf = (LPVOID)"Unknown error";
    e = WSAGetLastError();

    if (FormatMessage(FORMAT_MESSAGE_ALLOCATE_BUFFER
                      | FORMAT_MESSAGE_FROM_SYSTEM
                      | FORMAT_MESSAGE_IGNORE_INSERTS,
                      NULL,
                      e,
                      MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT),
                      // Default language
                      (LPTSTR)&lpMsgBuf,
                      0,
                      NULL))
    {

        rig_debug(lvl, "%s: Network error %d: %s\n", msg, e, lpMsgBuf);
        LocalFree(lpMsgBuf);
    }
    else
    {
        rig_debug(lvl, "%s: Network error %d\n", msg, e);
    }

# else
    e = errno;
    rig_debug(lvl, "%s: Network error %d: %s\n", msg, e, strerror(e));
# endif  /* ifdef __MINGW32__ */
}
#endif  /* if 0 */


int main(int argc, char *argv[])
{
    rig_model_t my_model[] = { RIG_MODEL_DUMMY, RIG_MODEL_SDRSHARP };

    int retcode;                /* generic return code from functions */

    int show_conf = 0;
    int dump_caps_opt = 0;
    const char *rig_file = NULL, *rig_file2 = "127.0.0.1:4532";
    //const char **ptt_file = NULL, *dcd_file = NULL;
    //ptt_type_t ptt_type = RIG_PTT_NONE;
    //dcd_type_t dcd_type = RIG_DCD_NONE;
    int serial_rate = 0;
    int serial_rate2 = 0;  /* virtual com port default speed */
    char *civaddr = NULL;       /* NULL means no need to set conf */
    char conf_parms[MAXCONFLEN] = "";

    printf("rigctlsync Version 1.0\n");

    rig_set_debug(verbose);
    while (1)
    {
        int c;
        int option_index = 0;
        char dummy[2];

        c = getopt_long(argc,
                        argv,
                        SHORT_OPTIONS,
                        long_options,
                        &option_index);

        if (c == -1)
        {
            break;
        }

        switch (c)
        {
        case 'h':
            usage(stdout);
            exit(0);

        case 'V':
            rig_version();
            exit(0);

        case 'B':
            mapa2b = 1;
            break;

        case 'm':
        case 'M':
            if (c == 'm')
            {
                my_model[0] = atoi(optarg);
            }
            else
            {
                my_model[1] = atoi(optarg);
            }

            break;

        case 'r':
            rig_file = optarg;
            break;

        case 'R':
            rig_file2 = optarg;
            break;


#if 0

        case 'p':
            ptt_file = optarg;
            break;
#endif

#if 0

        case 'd':
            dcd_file = optarg;
            break;
#endif

#if 0

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
            else if (!strcmp(optarg, "NONE"))
            {
                ptt_type = RIG_PTT_NONE;
            }
            else
            {
                ptt_type = atoi(optarg);
            }

            break;
#endif

#if 0

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
            else if (!strcmp(optarg, "NONE"))
            {
                dcd_type = RIG_DCD_NONE;
            }
            else
            {
                dcd_type = atoi(optarg);
            }

            break;
#endif

        case 'c':
            civaddr = optarg;
            break;

        case 's':
            if (sscanf(optarg, "%d%1s", &serial_rate, dummy) != 1)
            {
                fprintf(stderr, "Invalid baud rate of %s\n", optarg);
                exit(1);
            }

            break;

        case 'S':
            serial_rate2 = atoi(optarg);
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

        case 'Z':
            rig_set_debug_time_stamp(1);
            break;

        default:
            usage(stderr);        
            exit(1);
        }
    }

    rig_debug(RIG_DEBUG_VERBOSE, "%s, %s\n", "rigctlsync", hamlib_version2);
    rig_debug(RIG_DEBUG_VERBOSE, "%s",
              "Report bugs to <hamlib-developer@lists.sourceforge.net>\n\n");

    if (argc < 3)
    {
        usage(stderr);
        exit(1);
    }

    my_rig = rig_init(my_model[0]);

    if (!my_rig)
    {
        fprintf(stderr,
                "Unknown rig num %d, or initialization error.\n",
                my_model[0]);

        fprintf(stderr, "Please check with --list option.\n");
        exit(2);
    }

    my_rig_sync = rig_init(my_model[1]);

    if (!my_rig_sync)
    {
        fprintf(stderr,
                "Unknown rig num %d, or initialization error.\n",
                my_model[1]);

        fprintf(stderr, "Please check with --list option.\n");
        exit(2);
    }

#if 0
    retcode = set_conf(my_rig, conf_parms);

    if (retcode != RIG_OK)
    {
        fprintf(stderr, "Config parameter error: %s\n", rigerror(retcode));
        exit(2);
    }

#endif

    if (my_model[0] > 5 && !rig_file)
    {
        fprintf(stderr, "-r rig com port not provided\n");
        exit(2);
    }

    if (rig_file)
    {
        strncpy(RIGPORT(my_rig)->pathname, rig_file, HAMLIB_FILPATHLEN - 1);
    }

    fprintf(stderr, "rig to send frequency to: %s\n", rig_file2);
    strncpy(RIGPORT(my_rig_sync)->pathname, rig_file2, HAMLIB_FILPATHLEN - 1);

#if 0

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
    }

    if (dcd_file)
    {
        strncpy(DCDPORT(my_rig)->pathname, dcd_file, HAMLIB_FILPATHLEN - 1);
    }

#endif

    /* FIXME: bound checking and port type == serial */
    if (serial_rate != 0)
    {
        RIGPORT(my_rig)->parm.serial.rate = serial_rate;
    }

    if (serial_rate2 != 0)
    {
        RIGPORT(my_rig_sync)->parm.serial.rate = serial_rate2;
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
        rig_token_foreach(my_rig, print_conf_list, (rig_ptr_t)my_rig);
    }

    /*
     * print out conf parameters, and exits immediately
     * We may be interested only in only caps, and rig_open may fail.
     */
    if (dump_caps_opt)
    {
        dumpcaps(my_rig, stdout);
        rig_cleanup(my_rig);    /* if you care about memory */
        exit(0);
    }

    /* open and close rig connection to check early for issues */
    retcode = rig_open(my_rig);

    if (retcode != RIG_OK)
    {
        fprintf(stderr, "rig_open: error = %s \n", rigerror(retcode));
        exit(2);
    }


    retcode = rig_open(my_rig_sync);

    if (retcode != RIG_OK)
    {
        fprintf(stderr, "rig_open sync: error = %s \n", rigerror(retcode));
        exit(2);
    }


    if (verbose > 0)
    {
        printf("Opened rig model %u, '%s'\n",
               my_rig->caps->rig_model,
               my_rig->caps->model_name);
    }

    rig_debug(RIG_DEBUG_VERBOSE, "Backend version: %s, Status: %s\n",
              my_rig->caps->version, rig_strstatus(my_rig->caps->status));

    /*
     * main loop
     */
    do
    {
        freq_t freq;
        retcode = rig_get_freq(my_rig, RIG_VFO_CURR, &freq);

        if (retcode != RIG_OK)
        {
            rig_debug(RIG_DEBUG_ERR, "%s: Error in rig_get_freq: %s\n", __func__,
                      rigerror(retcode));
        }

        retcode = rig_set_freq(my_rig_sync, RIG_VFO_CURR, freq);

        hl_usleep(400 * 1000); // fairly fast to keep up
    }
    while (retcode == 0 && !ctrl_c);

    rig_close(my_rig);          /* close port */
    rig_cleanup(my_rig);        /* if you care about memory */

    return 0;
}

static void usage(FILE *fout)
{
    const char *name = "rigctlsync";

    fprintf(fout, "Usage: %s -m rignumber -r comport -s baud -M rignumber -R comport [OPTIONS]...\n\n"
           "Will copy frequency from -m rig to -M rig\n"
           "e.g. will keep SDR# synchronized to a rig.\n\n",
           name);

    fprintf(fout, "Example: Sync freq from rigctld to SDR#\n");
    fprintf(fout, "\t%s -m 2 -M 9 -R 127.0.0.1:4532\n\n", name);
    fprintf(fout, "See the %s.1 manual page for complete details.\n\n", name);

    fprintf(fout,
        "  -m, --model=ID                select radio model number. See model list (-l)\n"
        "  -r, --rig-file=DEVICE         set device of the radio to operate on\n"
        "  -R, --rig-file2=DEVICE        set device of the virtual com port to operate on\n"
        "  -s, --serial-speed=BAUD       set serial speed of the serial port\n"
        "  -S, --serial-speed2=BAUD      set serial speed of the virtual com port [default=115200]\n"
        "  -c, --civaddr=ID              set CI-V address, decimal (for Icom rigs only)\n"
        "  -C, --set-conf=PARM=VAL[,...] set config parameters\n"
        "  -L, --show-conf               list all config parameters\n"
        "  -l, --list                    list all model numbers and exit\n"
        "  -u, --dump-caps               dump capabilities and exit\n"
        "  -v, --verbose                 set verbose mode, cumulative (-v to -vvvvv)\n"
        "  -Z, --debug-time-stamps       enable time stamps for debug messages\n"
        "  -h, --help                    display this help and exit\n"
        "  -V, --version                 output version information and exit\n\n"
    );

    fprintf(fout, "\nReport bugs to <hamlib-developer@lists.sourceforge.net>.\n");
}
