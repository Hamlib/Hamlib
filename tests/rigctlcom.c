/*
 * rigctlcom.c - (C) Stephane Fillod 2000-2011
 *             (C) Nate Bargmann 2008,2010,2011,2012,2013
 *             (C) Michael Black W9MDB 2018 - derived from rigctld.c
 *             (C) The Hamlib Group 2012
 *
 *   This program is a TS-2000 emulator
 *   It takes TS-2000 commands on the -R comport and sends them to the rig
 *   on the -r comport.  The -R port speed can be set with -S and always runs 8N1
 *   This allows programs that can do a TS-2000-over-serial-port to talk
 *   to any rig that hamlib supports.
 *   Also supports rigctld or flrig for multiple connections (i.e. rig sharing).
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
// cppcheck-suppress *
#include <ctype.h>
// cppcheck-suppress *
#include <errno.h>
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

#include <hamlib/rig.h>
#include "misc.h"
#include "iofunc.h"
#include "serial.h"
#include "sprintflst.h"
#include "rigctl_parse.h"

/*
 * Reminder: when adding long options,
 *      keep up to date SHORT_OPTIONS, usage()'s output and man page. thanks.
 * NB: do NOT use -W since it's reserved by POSIX.
 * TODO: add an option to read from a file
 */
#define SHORT_OPTIONS "B:m:r:R:p:d:P:D:s:S:c:C:lLuvhVZ"
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

void usage();
static int handle_ts2000(void *arg);

static RIG *my_rig;             /* handle to rig */
static hamlib_port_t my_com;    /* handle to virtual COM port */
static int verbose;
/* CW Skimmer can only set VFOA */
/* IC7300 for example can run VFOA on FM and VFOB on CW */
/* So -A/--mapa2b changes set_freq on VFOA to VFOB */
/* This allows working CW Skimmer in split mode and transmit on VFOB */
static int mapa2b;              /* maps set_freq on VFOA to VFOB instead */
static int kwidth;

#ifdef HAVE_SIG_ATOMIC_T
static sig_atomic_t volatile ctrl_c;
#else
static int volatile ctrl_c;
#endif

#define MAXCONFLEN 1024

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
    rig_model_t my_model = RIG_MODEL_DUMMY;

    int retcode;                /* generic return code from functions */

    int show_conf = 0;
    int dump_caps_opt = 0;
    const char *rig_file = NULL, *rig_file2 = NULL, *ptt_file = NULL,
                *dcd_file = NULL;
    ptt_type_t ptt_type = RIG_PTT_NONE;
    dcd_type_t dcd_type = RIG_DCD_NONE;
    int serial_rate = 0;
    int serial_rate2 = 115200;  /* virtual com port default speed */
    char *civaddr = NULL;       /* NULL means no need to set conf */
    char conf_parms[MAXCONFLEN] = "";
    int status;

    printf("rigctlcom Version 1.4\n");

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
            usage();
            exit(0);

        case 'V':
            version();
            exit(0);

        case 'B':
            mapa2b = 1;
            break;

        case 'm':
            if (!optarg)
            {
                usage();        /* wrong arg count */
                exit(1);
            }

            my_model = atoi(optarg);
            break;

        case 'r':
            if (!optarg)
            {
                usage();        /* wrong arg count */
                exit(1);
            }

            rig_file = optarg;
            break;

        case 'R':
            if (!optarg)
            {
                usage();        /* wrong arg count */
                exit(1);
            }

            rig_file2 = optarg;
            break;


        case 'p':
            if (!optarg)
            {
                usage();        /* wrong arg count */
                exit(1);
            }

            ptt_file = optarg;
            break;

        case 'd':
            if (!optarg)
            {
                usage();        /* wrong arg count */
                exit(1);
            }

            dcd_file = optarg;
            break;

        case 'P':
            if (!optarg)
            {
                usage();        /* wrong arg count */
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
            else if (!strcmp(optarg, "NONE"))
            {
                ptt_type = RIG_PTT_NONE;
            }
            else
            {
                ptt_type = atoi(optarg);
            }

            break;

        case 'D':
            if (!optarg)
            {
                usage();        /* wrong arg count */
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
            else if (!strcmp(optarg, "NONE"))
            {
                dcd_type = RIG_DCD_NONE;
            }
            else
            {
                dcd_type = atoi(optarg);
            }

            break;

        case 'c':
            if (!optarg)
            {
                usage();        /* wrong arg count */
                exit(1);
            }

            civaddr = optarg;
            break;

        case 's':
            if (!optarg)
            {
                usage();        /* wrong arg count */
                exit(1);
            }

            if (sscanf(optarg, "%d%1s", &serial_rate, dummy) != 1)
            {
                fprintf(stderr, "Invalid baud rate of %s\n", optarg);
                exit(1);
            }

            break;

        case 'S':
            if (!optarg)
            {
                usage();        /* wrong arg count */
                exit(1);
            }

            serial_rate2 = atoi(optarg);
            break;


        case 'C':
            if (!optarg)
            {
                usage();        /* wrong arg count */
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

        case 'Z':
            rig_set_debug_time_stamp(1);
            break;

        default:
            usage();            /* unknown option? */
            exit(1);
        }
    }

    rig_set_debug(verbose);

    rig_debug(RIG_DEBUG_VERBOSE, "%s, %s\n", "rigctlcom", hamlib_version2);
    rig_debug(RIG_DEBUG_VERBOSE, "%s",
              "Report bugs to <hamlib-developer@lists.sourceforge.net>\n\n");

    if (argc == 1)
    {
        usage();
        exit(2);
    }

    my_rig = rig_init(my_model);

    if (!my_rig)
    {
        fprintf(stderr,
                "Unknown rig num %d, or initialization error.\n",
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

    if (my_model > 5 && !rig_file)
    {
        fprintf(stderr, "-r rig com port not provided\n");
        exit(2);
    }

    if (rig_file)
    {
        strncpy(my_rig->state.rigport.pathname, rig_file, HAMLIB_FILPATHLEN - 1);
    }

    if (!rig_file2)
    {
        fprintf(stderr, "-R com port not provided\n");
        exit(2);
    }

    strncpy(my_com.pathname, rig_file2, HAMLIB_FILPATHLEN - 1);

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
        strncpy(my_rig->state.pttport.pathname, ptt_file, HAMLIB_FILPATHLEN - 1);
    }

    if (dcd_file)
    {
        strncpy(my_rig->state.dcdport.pathname, dcd_file, HAMLIB_FILPATHLEN - 1);
    }

    /* FIXME: bound checking and port type == serial */
    if (serial_rate != 0)
    {
        my_rig->state.rigport.parm.serial.rate = serial_rate;
    }

    if (serial_rate2 != 0)
    {
        my_com.parm.serial.rate = serial_rate2;
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

    if (verbose > 0)
    {
        printf("Opened rig model %d, '%s'\n",
               my_rig->caps->rig_model,
               my_rig->caps->model_name);
    }

    rig_debug(RIG_DEBUG_VERBOSE, "Backend version: %s, Status: %s\n",
              my_rig->caps->version, rig_strstatus(my_rig->caps->status));

    /*
     * main loop
     */
    my_com.type.rig = RIG_PORT_SERIAL;
    my_com.parm.serial.data_bits = 8;
    my_com.parm.serial.stop_bits = 1;
    my_com.timeout = 5000;
    my_com.parm.serial.parity = RIG_PARITY_NONE;
    my_com.parm.serial.handshake = RIG_HANDSHAKE_NONE;

    status = port_open(&my_com);

    if (status != RIG_OK)
    {
        rig_debug(RIG_DEBUG_ERR, "Unable to open %s\n", my_com.pathname);
        exit(2);
    }

    if (verbose > 0)
    {
        fprintf(stderr, " %s opened for application program\n", my_com.pathname);
    }

    do
    {
        char ts2000[1024];
        char *stop_set = ";\n\r";

        memset(ts2000, 0, sizeof(ts2000));

        status = read_string(&my_com,
                             (unsigned char *) ts2000,
                             sizeof(ts2000),
                             stop_set,
                             strlen(stop_set),
                             0,
                             1);

        rig_debug(RIG_DEBUG_TRACE, "%s: status=%d\n", __func__, status);

        if (strlen(ts2000) > 0)
        {
            int retval = handle_ts2000(ts2000);

            if (retval != RIG_OK)
            {
                rig_debug(RIG_DEBUG_ERR, "%s: %s\n", __func__, rigerror(retval));
            }
        }

    }
    while (retcode == 0 && !ctrl_c);

    rig_close(my_rig);          /* close port */
    rig_cleanup(my_rig);        /* if you care about memory */

    return 0;
}


static rmode_t ts2000_get_mode()
{
    rmode_t mode;
    pbwidth_t width;
    rig_get_mode(my_rig, vfo_fixup(my_rig, RIG_VFO_A, my_rig->state.cache.split),
                 &mode, &width);

    // Perhaps we should emulate a rig that has PKT modes instead??
    int kwidth_ssb[] = { 10, 50, 100, 200, 300, 400, 500, 600, 700, 800, 900, 1000 };
    int kwidth_am[] = { 10, 100, 200, 500 };

    // still need to cover packet filter 00=wide, 01=nar
    switch (mode)
    {
    case RIG_MODE_LSB:   mode = 1; kwidth = kwidth_ssb[width]; break;

    case RIG_MODE_USB:   mode = 2; kwidth = kwidth_ssb[width]; break;

    case RIG_MODE_CW:    mode = 3; kwidth = kwidth_am[width];
        break; // is this correct?

    case RIG_MODE_FM:    mode = 4; kwidth = kwidth_ssb[width]; break;

    case RIG_MODE_AM:    mode = 5; kwidth = kwidth_am[width]; break;

    case RIG_MODE_RTTY:  mode = 6; kwidth = kwidth_ssb[width]; break;

    case RIG_MODE_CWR:   mode = 7; kwidth = kwidth_am[width];
        break; // is this correct?

    case RIG_MODE_NONE:  mode = 8; kwidth = kwidth_am[width];
        break; // is this correct?

    case RIG_MODE_RTTYR: mode = 9; kwidth = kwidth_ssb[width]; break;

    case RIG_MODE_PKTUSB: mode = 2; kwidth = kwidth_ssb[width];
        break; // need to change to a TS_2000 mode

    case RIG_MODE_PKTLSB: mode = 1; kwidth = kwidth_ssb[width];
        break; // need to change to a TS_2000 mode

    default: mode = 0; break;
    }

    return mode;
}


static int write_block2(void *func,
                        hamlib_port_t *p,
                        const char *txbuffer,
                        size_t count)
{
    int retval = write_block(p, (unsigned char *) txbuffer, count);
    hl_usleep(5000);

    if (retval != RIG_OK)
    {
        rig_debug(RIG_DEBUG_ERR, "%s: %s\n", __func__, rigerror(retval));
    }

    return retval;
}


/*
 * This handles the TS-2000 emulation
 */
static int handle_ts2000(void *arg)
{
    rig_debug(RIG_DEBUG_VERBOSE, "%s: cmd=%s\n", __func__, (char *)arg);

    // Handle all the queries
    if (strcmp(arg, "ID;") == 0)
    {
        char *reply = "ID019;";
        return write_block2((void *)__func__, &my_com, reply, strlen(reply));
    }

    if (strcmp(arg, "AI;") == 0)
    {
        char *reply = "AI0;";
        return write_block2((void *)__func__, &my_com, reply, strlen(reply));
    }
    else if (strcmp(arg, "IF;") == 0)
    {
        freq_t freq;            // P1
        int freq_step = 10;     // P2 just use default value for now
        int rit_xit_freq = 0;   // P3 dummy value for now
        int rit = 0;            // P4 dummy value for now
        int xit = 0;            // P5 dummy value for now
        int bank1 = 0;          // P6 dummy value for now
        int bank2 = 0;          // P7 dummy value for now
        ptt_t ptt;              // P8
        rmode_t mode;           // P9
        vfo_t vfo;              // P10
        int scan = 0;           // P11 dummy value for now
        split_t split = 0;      // P1 2
        int p13 = 0;            // P13 Tone dummy value for now
        int p14 = 0;            // P14 Tone Freq dummy value for now
        int p15 = 0;            // P15 Shift status dummy value for now
        int retval = rig_get_freq(my_rig, vfo_fixup(my_rig, RIG_VFO_A,
                                  my_rig->state.cache.split), &freq);
        char response[64];
        char *fmt =
            // cppcheck-suppress *
            "IF%011"PRIll"%04d+%05d%1d%1d%1d%02d%1d%1"PRIll"%1d%1d%1d%1d%02d%1d;";

        if (retval != RIG_OK)
        {
            return retval;
        }

        mode = ts2000_get_mode();
        retval = rig_get_ptt(my_rig, vfo_fixup(my_rig, RIG_VFO_A,
                                               my_rig->state.cache.split), &ptt);

        if (retval != RIG_OK)
        {
            return retval;
        }

        // we need to know split status -- don't care about the vfo
        retval = rig_get_split_vfo(my_rig, RIG_VFO_CURR, &split, &vfo);

        if (retval != RIG_OK)
        {
            return retval;
        }

        retval = rig_get_vfo(my_rig, &vfo);

        if (retval != RIG_OK)
        {
            return retval;
        }

        switch (vfo)
        {
        case RIG_VFO_A:
        case RIG_VFO_MAIN:
        case RIG_VFO_MAIN_A:
        case RIG_VFO_SUB_A:
            vfo = 0;
            break;

        case RIG_VFO_B:
        case RIG_VFO_SUB:
        case RIG_VFO_MAIN_B:
        case RIG_VFO_SUB_B:
            vfo = 1;
            break;

        default:
            rig_debug(RIG_DEBUG_ERR, "%s: unexpected vfo=%d\n", __func__, vfo);
        }

        SNPRINTF(response,
                 sizeof(response),
                 fmt,
                 (uint64_t)freq,
                 freq_step,
                 rit_xit_freq,
                 rit, xit,
                 bank1,
                 bank2,
                 ptt,
                 mode,
                 vfo,
                 scan,
                 split,
                 p13,
                 p14,
                 p15);

        return write_block2((void *)__func__, &my_com, response, strlen(response));
    }
    else if (strcmp(arg, "MD;") == 0)
    {
        rmode_t mode = ts2000_get_mode();
        char response[32];

        SNPRINTF(response, sizeof(response), "MD%1d;", (int)mode);
        return write_block2((void *)__func__, &my_com, response, strlen(response));
    }
    else if (strcmp(arg, "AG0;") == 0)
    {
        char response[32];

        SNPRINTF(response, sizeof(response), "AG0000;");
        return write_block2((void *)__func__, &my_com, response, strlen(response));
    }
    else if (strcmp(arg, "FA;") == 0)
    {
        freq_t freq = 0;
        char response[32];

        int retval = rig_get_freq(my_rig, vfo_fixup(my_rig, RIG_VFO_A,
                                  my_rig->state.cache.split), &freq);

        if (retval != RIG_OK)
        {
            rig_debug(RIG_DEBUG_ERR, "%s: get freqA failed: %s\n", __func__,
                      rigerror(retval));
            return retval;
        }

        SNPRINTF(response, sizeof(response), "FA%011"PRIll";", (uint64_t)freq);
        return write_block2((void *)__func__, &my_com, response, strlen(response));
    }
    else if (strcmp(arg, "FB;") == 0)
    {
        char response[32];
        freq_t freq = 0;
        int retval = rig_get_freq(my_rig, vfo_fixup(my_rig, RIG_VFO_B,
                                  my_rig->state.cache.split), &freq);

        if (retval != RIG_OK)
        {
            rig_debug(RIG_DEBUG_ERR, "%s: get freqB failed: %s\n", __func__,
                      rigerror(retval));
            return retval;
        }

        SNPRINTF(response, sizeof(response), "FB%011"PRIll";", (uint64_t)freq);
        return write_block2((void *)__func__, &my_com, response, strlen(response));
    }
    else if (strcmp(arg, "SA;") == 0)
    {
        char response[32];

        SNPRINTF(response, sizeof(response), "SA0;");
        return write_block2((void *)__func__, &my_com, response, strlen(response));
    }
    else if (strcmp(arg, "RX;") == 0)
    {
        char response[32];

        rig_set_ptt(my_rig, vfo_fixup(my_rig, RIG_VFO_A, my_rig->state.cache.split), 0);
        SNPRINTF(response, sizeof(response), "RX0;");
        return write_block2((void *)__func__, &my_com, response, strlen(response));
    }
    // Now some commands to set things
    else if (strncmp(arg, "SA", 2) == 0)
    {
        if (strcmp(arg, "SA;") == 0)
        {
            // should we silently fail with RIG_OK instead? TBD
            return (-RIG_ENIMPL);
        }

        if (strlen(arg) > 3 && ((char *)arg)[2] == '1')
        {
            if (my_rig->caps->has_set_func)
            {
                int retval = rig_set_func(my_rig, RIG_VFO_CURR, RIG_FUNC_SATMODE,
                                          ((char *)arg)[2] == '1' ? 1 : 0);
                return retval;
            }
            else
            {
                return (-RIG_ENAVAIL);
            }
        }

        return RIG_OK;
    }
    else if (strcmp(arg, "TX;") == 0)
    {
        return rig_set_ptt(my_rig, vfo_fixup(my_rig, RIG_VFO_A,
                                             my_rig->state.cache.split), 1);
    }
    else if (strcmp(arg, "AI0;") == 0)
    {
        // nothing to do
        return RIG_OK;
    }
    else if (strcmp(arg, ";") == 0)
    {
        // nothing to do
        return RIG_OK;
    }
    else if (strcmp(arg, "FR0;") == 0)
    {
        return rig_set_vfo(my_rig, vfo_fixup(my_rig, RIG_VFO_A,
                                             my_rig->state.cache.split));
    }
    else if (strcmp(arg, "FR1;") == 0)
    {
        return rig_set_vfo(my_rig, vfo_fixup(my_rig, RIG_VFO_B,
                                             my_rig->state.cache.split));
    }
    else if (strcmp(arg, "FR;") == 0)
    {
        char response[32];
        vfo_t vfo;
        int retval = rig_get_vfo(my_rig, &vfo);
        int nvfo = 0;

        if (retval != RIG_OK)
        {
            rig_debug(RIG_DEBUG_ERR, "%s: get vfo failed: %s\n", __func__,
                      rigerror(retval));
            return retval;
        }


        if (vfo == vfo_fixup(my_rig, RIG_VFO_A, my_rig->state.cache.split)) { nvfo = 0; }
        else if (vfo == vfo_fixup(my_rig, RIG_VFO_B, my_rig->state.cache.split)) { nvfo = 1; }
        else
        {
            retval = -RIG_EPROTO;
            return retval;
        }

        SNPRINTF(response, sizeof(response), "FR%c;", nvfo + '0');
        return write_block2((void *)__func__, &my_com, response, strlen(response));

        return retval;
    }
    else if (strcmp(arg, "FT;") == 0)
    {
        char response[32];
        vfo_t vfo, vfo_curr = vfo_fixup(my_rig, RIG_VFO_A, my_rig->state.cache.split);
        split_t split;
        int nvfo = 0;
        int retval = rig_get_split_vfo(my_rig, vfo_curr, &split, &vfo);

        if (retval != RIG_OK)
        {
            rig_debug(RIG_DEBUG_ERR, "%s: get split vfo failed: %s\n", __func__,
                      rigerror(retval));
            return retval;
        }


        if (vfo == vfo_fixup(my_rig, RIG_VFO_A, my_rig->state.cache.split)) { nvfo = 0; }
        else if (vfo == vfo_fixup(my_rig, RIG_VFO_B, my_rig->state.cache.split)) { nvfo = 1; }
        else
        {
            retval = -RIG_EPROTO;
            return retval;
        }

        SNPRINTF(response, sizeof(response), "FT%c;", nvfo + '0');
        return write_block2((void *)__func__, &my_com, response, strlen(response));

        return retval;
    }
    else if (strcmp(arg, "TN;") == 0)
    {
        char response[32];
        tone_t val;
        int retval = rig_get_ctcss_tone(my_rig, RIG_VFO_CURR, &val);

        if (retval != RIG_OK)
        {
            rig_debug(RIG_DEBUG_ERR, "%s: get_func XIT failed: %s\n", __func__,
                      rigerror(retval));
            return retval;
        }

        SNPRINTF(response, sizeof(response), "TN%02d;", val);
        return write_block2((void *)__func__, &my_com, response, strlen(response));
    }
    else if (strncmp(arg, "TN", 2) == 0)
    {
        tone_t val;
        int ival = 0;
        int retval;
        sscanf(arg, "TN%d", &ival);
        val = ival;
        retval = rig_set_ctcss_tone(my_rig, RIG_VFO_CURR, val);

        if (retval != RIG_OK)
        {
            rig_debug(RIG_DEBUG_ERR, "%s: set_ctcss_tone failed: %s\n", __func__,
                      rigerror(retval));
        }

        return retval;
    }
    else if (strcmp(arg, "PA;") == 0)
    {
        char response[32];
        int valA;
        int retval = rig_get_func(my_rig, vfo_fixup(my_rig, RIG_VFO_A,
                                  my_rig->state.cache.split), RIG_FUNC_AIP,
                                  &valA);
        int valB;

        if (retval != RIG_OK)
        {
            rig_debug(RIG_DEBUG_ERR, "%s: get_func preamp A failed: %s\n", __func__,
                      rigerror(retval));

            if (retval == -RIG_ENIMPL || retval == -RIG_ENAVAIL)
            {
                char *responsetmp = "?;";
                return write_block2((void *)__func__, &my_com, responsetmp,
                                    strlen(responsetmp));
            }

            return retval;
        }

        retval = rig_get_func(my_rig, vfo_fixup(my_rig, RIG_VFO_B,
                                                my_rig->state.cache.split), RIG_FUNC_AIP,
                              &valB);

        if (retval != RIG_OK)
        {
            rig_debug(RIG_DEBUG_ERR, "%s: get_func preamp B failed: %s\n", __func__,
                      rigerror(retval));
            return retval;
        }

        SNPRINTF(response, sizeof(response), "PA%c%c;", valA + '0', valB + '0');
        return write_block2((void *)__func__, &my_com, response, strlen(response));
    }
    else if (strncmp(arg, "PA", 2) == 0)
    {
        int valA = 0;
        int valB = 0;
        int retval;
        int n = sscanf(arg, "PA%1d%1d", &valA, &valB);

        if (n != 2)
        {
            rig_debug(RIG_DEBUG_ERR, "%s: error parsing preamp cmd '%s'\n", __func__,
                      (char *)arg);
        }

        retval = rig_set_func(my_rig, vfo_fixup(my_rig, RIG_VFO_A,
                                                my_rig->state.cache.split), RIG_FUNC_AIP, valA);

        if (retval != RIG_OK)
        {
            rig_debug(RIG_DEBUG_ERR, "%s: set_func preamp failed: %s\n", __func__,
                      rigerror(retval));
            return retval;
        }

        retval = rig_set_func(my_rig, vfo_fixup(my_rig, RIG_VFO_B,
                                                my_rig->state.cache.split), RIG_FUNC_AIP, valB);

        if (retval != RIG_OK)
        {
            rig_debug(RIG_DEBUG_ERR, "%s: set_func preamp failed: %s\n", __func__,
                      rigerror(retval));
            return retval;
        }

        return retval;
    }
    else if (strcmp(arg, "XT;") == 0)
    {
        char response[32];
        int val;
        int retval = rig_get_func(my_rig, RIG_VFO_CURR, RIG_FUNC_XIT, &val);

        if (retval != RIG_OK)
        {
            rig_debug(RIG_DEBUG_ERR, "%s: get_func XIT failed: %s\n", __func__,
                      rigerror(retval));

            if (retval == -RIG_ENIMPL || retval == -RIG_ENAVAIL)
            {
                char *responsetmp = "?;";
                return write_block2((void *)__func__, &my_com, responsetmp,
                                    strlen(responsetmp));
            }

            return retval;
        }

        SNPRINTF(response, sizeof(response), "XT%c;", val + '0');
        return write_block2((void *)__func__, &my_com, response, strlen(response));
    }
    else if (strncmp(arg, "XT", 2) == 0)
    {
        int val = 0;
        int retval;
        sscanf(arg, "XT%d", &val);
        retval = rig_set_func(my_rig, RIG_VFO_CURR, RIG_FUNC_XIT, val);

        if (retval != RIG_OK)
        {
            rig_debug(RIG_DEBUG_ERR, "%s: set_func XIT failed: %s\n", __func__,
                      rigerror(retval));

            if (retval == -RIG_ENIMPL || retval == -RIG_ENAVAIL)
            {
                char *response = "?;";
                return write_block2((void *)__func__, &my_com, response, strlen(response));
            }
        }

        return retval;
    }
    else if (strcmp(arg, "NR;") == 0)
    {
        char response[32];
        int val;
        int retval = rig_get_func(my_rig, RIG_VFO_CURR, RIG_FUNC_NR, &val);

        if (retval != RIG_OK)
        {
            rig_debug(RIG_DEBUG_ERR, "%s: get NR failed: %s\n", __func__, rigerror(retval));

            if (retval == -RIG_ENIMPL || retval == -RIG_ENAVAIL)
            {
                char *responsetmp = "?;";
                return write_block2((void *)__func__, &my_com, responsetmp,
                                    strlen(responsetmp));
            }

            return retval;
        }

        SNPRINTF(response, sizeof(response), "NR%c;", val + '0');
        return write_block2((void *)__func__, &my_com, response, strlen(response));
    }
    else if (strncmp(arg, "NR", 2) == 0)
    {
        int val = 0;
        int retval;
        sscanf(arg, "NR%d", &val);
        retval = rig_set_func(my_rig, RIG_VFO_CURR, RIG_FUNC_NR, val);

        if (retval != RIG_OK)
        {
            rig_debug(RIG_DEBUG_ERR, "%s: set NR failed: %s\n", __func__, rigerror(retval));

            if (retval == -RIG_ENIMPL || retval == -RIG_ENAVAIL)
            {
                char *responsetmp = "?;";
                return write_block2((void *)__func__, &my_com, responsetmp,
                                    strlen(responsetmp));
            }
        }

        return retval;
    }
    else if (strcmp(arg, "NB;") == 0)
    {
        char response[32];
        int val;
        int retval = rig_get_func(my_rig, RIG_VFO_CURR, RIG_FUNC_NB, &val);

        if (retval != RIG_OK)
        {
            rig_debug(RIG_DEBUG_ERR, "%s: get func NB failed: %s\n", __func__,
                      rigerror(retval));

            if (retval == -RIG_ENIMPL || retval == -RIG_ENAVAIL)
            {
                char *responsetmp = "?;";
                return write_block2((void *)__func__, &my_com, responsetmp,
                                    strlen(responsetmp));
            }

            return retval;
        }

        SNPRINTF(response, sizeof(response), "NB%c;", val + '0');
        return write_block2((void *)__func__, &my_com, response, strlen(response));
    }
    else if (strncmp(arg, "NB", 2) == 0)
    {
        int val = 0;
        int retval = rig_set_func(my_rig, RIG_VFO_CURR, RIG_FUNC_NB, val);
        sscanf(arg, "NB%d", &val);

        if (retval != RIG_OK)
        {
            rig_debug(RIG_DEBUG_ERR, "%s: set_ant failed: %s\n", __func__,
                      rigerror(retval));

            if (retval == -RIG_ENIMPL || retval == -RIG_ENAVAIL)
            {
                char *responsetmp = "?;";
                return write_block2((void *)__func__, &my_com, responsetmp,
                                    strlen(responsetmp));
            }
        }

        return retval;
    }
    else if (strcmp(arg, "AG;") == 0)
    {
        value_t val;
        int retval = rig_get_level(my_rig, RIG_VFO_CURR, RIG_LEVEL_AF, &val);
        char response[32];
        int level;

        if (retval != RIG_OK)
        {
            rig_debug(RIG_DEBUG_ERR, "%s: get_level AF failed: %s\n", __func__,
                      rigerror(retval));

            if (retval == -RIG_ENIMPL || retval == -RIG_ENAVAIL)
            {
                char *responsetmp = "?;";
                return write_block2((void *)__func__, &my_com, responsetmp,
                                    strlen(responsetmp));
            }

            return retval;
        }

        level = val.f * 255;
        SNPRINTF(response, sizeof(response), "AG0%03d;", level);
        return write_block2((void *)__func__, &my_com, response, strlen(response));
    }
    else if (strncmp(arg, "AG", 2) == 0)
    {
        int level = 0;
        int n = sscanf(arg, "AG%d", &level);
        int retval;
        value_t val;

        if (n != 1)
        {
            rig_debug(RIG_DEBUG_ERR, "%s: af level cmd parse failed: %s\n", __func__,
                      (char *)arg);
            return -RIG_EPROTO;
        }

        val.f = level / 255.0;
        retval = rig_set_level(my_rig, RIG_VFO_CURR, RIG_LEVEL_AF, val);

        if (retval != RIG_OK)
        {
            rig_debug(RIG_DEBUG_ERR, "%s: set_level AF failed: %s\n", __func__,
                      rigerror(retval));
        }

        return retval;
    }
    else if (strcmp(arg, "PR;") == 0)
    {
        char response[32];
        value_t val;
        int retval = rig_get_level(my_rig, RIG_VFO_CURR, RIG_LEVEL_COMP, &val);
        int speechLevel;

        if (retval != RIG_OK)
        {
            rig_debug(RIG_DEBUG_ERR, "%s: get_level speech processor failed: %s\n",
                      __func__, rigerror(retval));

            if (retval == -RIG_ENIMPL || retval == -RIG_ENAVAIL)
            {
                char *responsetmp = "?;";
                return write_block2((void *)__func__, &my_com, responsetmp,
                                    strlen(responsetmp));
            }

            return retval;
        }

        speechLevel = val.f * 255;
        SNPRINTF(response, sizeof(response), "PR%03d;", speechLevel);
        return write_block2((void *)__func__, &my_com, response, strlen(response));
    }
    else if (strncmp(arg, "PR", 2) == 0)
    {
        value_t val;
        int speechLevel = 0;
        int n = sscanf(arg, "PR%d", &speechLevel);
        int retval;

        if (n != 1)
        {
            rig_debug(RIG_DEBUG_ERR, "%s: speech level cmd parse failed: %s\n", __func__,
                      (char *)arg);
            return -RIG_EPROTO;
        }

        val.f = speechLevel / 255.0;
        retval = rig_set_level(my_rig, RIG_VFO_CURR, RIG_LEVEL_COMP, val);

        if (retval != RIG_OK)
        {
            rig_debug(RIG_DEBUG_ERR, "%s: set_level AGC failed: %s\n", __func__,
                      rigerror(retval));
        }

        if (retval == -RIG_ENIMPL || retval == -RIG_ENAVAIL)
        {
            char *responsetmp = "?;";
            return write_block2((void *)__func__, &my_com, responsetmp,
                                strlen(responsetmp));
        }

        return retval;
    }
    else if (strcmp(arg, "GT;") == 0)
    {
        value_t val;
        int retval = rig_get_level(my_rig, RIG_VFO_CURR, RIG_LEVEL_AGC, &val);
        char response[32];
        int agcLevel;

        if (retval != RIG_OK)
        {
            rig_debug(RIG_DEBUG_ERR, "%s: get_level AGC failed: %s\n", __func__,
                      rigerror(retval));

            if (retval == -RIG_ENIMPL || retval == -RIG_ENAVAIL)
            {
                char *responsetmp = "?;";
                return write_block2((void *)__func__, &my_com, responsetmp,
                                    strlen(responsetmp));
            }

            return retval;
        }

        agcLevel = val.f * 255;
        SNPRINTF(response, sizeof(response), "GT%03d;", agcLevel);
        return write_block2((void *)__func__, &my_com, response, strlen(response));
    }
    else if (strncmp(arg, "GT", 2) == 0)
    {
        int agcLevel = 0;
        int n = sscanf(arg, "GT%d", &agcLevel);
        int retval;
        value_t val;

        if (n != 1)
        {
            rig_debug(RIG_DEBUG_ERR, "%s: AGC cmd parse failed: %s\n", __func__,
                      (char *)arg);
            return -RIG_EPROTO;
        }

        val.f = agcLevel / 255.0;
        retval = rig_set_level(my_rig, RIG_VFO_CURR, RIG_LEVEL_AGC, val);

        if (retval != RIG_OK)
        {
            rig_debug(RIG_DEBUG_ERR, "%s: set_level AGC failed: %s\n", __func__,
                      rigerror(retval));
        }

        return retval;
    }
    else if (strcmp(arg, "SQ;") == 0)
    {
        char response[32];
        value_t val;
        int retval = rig_get_level(my_rig, RIG_VFO_CURR, RIG_LEVEL_SQL, &val);
        int sqlev;

        if (retval != RIG_OK)
        {
            rig_debug(RIG_DEBUG_ERR, "%s: get_level squelch failed: %s\n", __func__,
                      rigerror(retval));

            if (retval == -RIG_ENIMPL || retval == -RIG_ENAVAIL)
            {
                char *responsetmp = "?;";
                return write_block2((void *)__func__, &my_com, responsetmp,
                                    strlen(responsetmp));
            }

            return retval;
        }

        sqlev = val.f * 255;
        SNPRINTF(response, sizeof(response), "SQ%03d;", sqlev);
        return write_block2((void *)__func__, &my_com, response, strlen(response));
    }
    else if (strncmp(arg, "SQ", 2) == 0)
    {
        int sqlev = 0;
        int n = sscanf(arg, "SQ%d", &sqlev);
        int retval;
        value_t val;

        if (n != 1)
        {
            rig_debug(RIG_DEBUG_ERR, "%s: squelch cmd parse failed: %s\n", __func__,
                      (char *)arg);
            return -RIG_EPROTO;
        }

        val.f = sqlev / 255.0;
        retval = rig_set_level(my_rig, RIG_VFO_CURR, RIG_LEVEL_SQL, val);

        if (retval != RIG_OK)
        {
            rig_debug(RIG_DEBUG_ERR, "%s: set_level squelch failed: %s\n", __func__,
                      rigerror(retval));

            if (retval == -RIG_ENIMPL || retval == -RIG_ENAVAIL)
            {
                char *responsetmp = "?;";
                return write_block2((void *)__func__, &my_com, responsetmp,
                                    strlen(responsetmp));
            }
        }

        return retval;
    }
    else if (strcmp(arg, "DC;") == 0)
    {
        vfo_t vfo, vfo_curr = vfo_fixup(my_rig, RIG_VFO_A, my_rig->state.cache.split);
        split_t split;
        char response[32];
        int retval = rig_get_split_vfo(my_rig, vfo_curr, &split, &vfo);

        if (retval != RIG_OK)
        {
            rig_debug(RIG_DEBUG_ERR, "%s: get split vfo failed: %s\n", __func__,
                      rigerror(retval));
            return retval;
        }

        SNPRINTF(response, sizeof(response), "DC%c;", split + '0');
        return write_block2((void *)__func__, &my_com, response, strlen(response));

        return retval;
    }
    else if (strncmp(arg, "DC", 2) == 0)
    {
        vfo_t vfo_curr = vfo_fixup(my_rig, RIG_VFO_A, my_rig->state.cache.split);
        split_t split;
        int isplit;
        int retval;
        char response[32];
        // Expecting DCnn -- but we dont' care about the control param
        int n = sscanf(arg, "DC%d", &isplit);

        if (n != 1)
        {
            rig_debug(RIG_DEBUG_ERR, "%s: error parsing '%s'\n", __func__, (char *)arg);
            return -RIG_EPROTO;
        }

        split = isplit;
        retval = rig_set_split_vfo(my_rig, vfo_curr, split, RIG_VFO_SUB);

        if (retval != RIG_OK)
        {
            rig_debug(RIG_DEBUG_ERR, "%s: rig set split vfo failed '%s'\n", __func__,
                      rigerror(retval));
            return retval;
        }

        SNPRINTF(response, sizeof(response), "DC%c;", split + '0');
        return write_block2((void *)__func__, &my_com, response, strlen(response));

        return retval;
    }
    else if (strcmp(arg, "FT0;") == 0)
    {
        return rig_set_split_vfo(my_rig, vfo_fixup(my_rig, RIG_VFO_A,
                                 my_rig->state.cache.split), vfo_fixup(my_rig,
                                         RIG_VFO_A, my_rig->state.cache.split), 0);
    }
    else if (strcmp(arg, "FT1;") == 0)
    {
        return rig_set_split_vfo(my_rig, vfo_fixup(my_rig, RIG_VFO_B,
                                 my_rig->state.cache.split), vfo_fixup(my_rig,
                                         RIG_VFO_B, my_rig->state.cache.split), 0);
    }
    else if (strncmp(arg, "FA0", 3) == 0)
    {
        freq_t freq;
        vfo_t vfo = RIG_VFO_A;

        if (mapa2b) { vfo = RIG_VFO_B; }

        sscanf((char *)arg + 2, "%"SCNfreq, &freq);
        return rig_set_freq(my_rig, vfo_fixup(my_rig, vfo,
                                              my_rig->state.cache.split), freq);
    }
    else if (strncmp(arg, "FB0", 3) == 0)
    {
        freq_t freq;

        sscanf((char *)arg + 2, "%"SCNfreq, &freq);
        return rig_set_freq(my_rig, vfo_fixup(my_rig, RIG_VFO_B,
                                              my_rig->state.cache.split), freq);
    }
    else if (strncmp(arg, "MD", 2) == 0)
    {
        char response[32];
        mode_t mode = 0;
        int imode = 0;

        sscanf((char *)arg + 2, "%d", &imode);

        switch (imode)
        {
        case 0: mode = RIG_MODE_NONE; break;

        case 1: mode = RIG_MODE_LSB ; break;

        case 2: mode = RIG_MODE_USB; break;

        case 3: mode = RIG_MODE_CW; break;

        case 4: mode = RIG_MODE_AM; break;

        case 5: mode = RIG_MODE_FM; break;

        case 6: mode = RIG_MODE_RTTY; break;

        case 7: mode = RIG_MODE_CWR; break;

        case 8: mode = RIG_MODE_NONE; break;

        case 9: mode = RIG_MODE_RTTYR; break;
        }

        SNPRINTF(response, sizeof(response), "MD%c;", mode + '0');
        return write_block2((void *)__func__, &my_com, response, strlen(response));
    }
    else if (strcmp(arg, "PS1;") == 0)
    {
        return RIG_OK;
    }
    else if (strcmp(arg, "PS0;") == 0)
    {
        return RIG_OK;
    }
    else if (strncmp(arg, "PS", 2) == 0)
    {
        char response[32];

        SNPRINTF(response, sizeof(response), "PS1;");
        return write_block2((void *)__func__, &my_com, response, strlen(response));
    }
    else if (strcmp(arg, "SM0;") == 0)
    {
        int retval;
        value_t value;
        char response[32];
        retval = rig_get_level(my_rig, RIG_VFO_A, RIG_LEVEL_STRENGTH, &value);

        if (retval != RIG_OK)
        {
            SNPRINTF(response, sizeof(response), "SM00000;");
        }
        else
        {
            SNPRINTF(response, sizeof(response), "SM0%04d;", value.i * 3);
            rig_debug(RIG_DEBUG_ERR, "SM response=%d\n", value.i);
        }

        return write_block2((void *)__func__, &my_com, response, strlen(response));
    }
    else if (strcmp(arg, "SM1;") == 0)
    {
        int retval;
        value_t value;
        char response[32];
        retval = rig_get_level(my_rig, RIG_VFO_B, RIG_LEVEL_STRENGTH, &value);

        if (retval != RIG_OK)
        {
            SNPRINTF(response, sizeof(response), "SM10000;");
        }
        else
        {
            SNPRINTF(response, sizeof(response), "SM1%04d;", value.i * 3);
            rig_debug(RIG_DEBUG_ERR, "SM response=%d\n", value.i);
        }

        return write_block2((void *)__func__, &my_com, response, strlen(response));
    }
    else if (strcmp(arg, "KS;") == 0)
    {
        int retval;
        value_t value;
        char response[32];
        retval = rig_get_level(my_rig, RIG_VFO_CURR, RIG_LEVEL_KEYSPD, &value);

        if (retval != RIG_OK)
        {
            SNPRINTF(response, sizeof(response), "KS010;");
        }
        else
        {
            rig_debug(RIG_DEBUG_ERR, "KS response=%d\n", value.i);
        }

        return write_block2((void *)__func__, &my_com, response, strlen(response));
    }
    else if (strcmp(arg, "SL;") == 0)
    {
        char response[32];
        SNPRINTF(response, sizeof(response), "SL%02d;", kwidth);
        return write_block2((void *)__func__, &my_com, response, strlen(response));
    }
    else if (strncmp(arg, "SB", 2) == 0
             || strncmp(arg, "AC", 2) == 0
             || strncmp(arg, "AM", 2) == 0
             || strncmp(arg, "AN", 2) == 0
             || strncmp(arg, "BC", 2) == 0
             || strncmp(arg, "CA", 2) == 0
             || strncmp(arg, "CT", 2) == 0
             || strncmp(arg, "DQ", 2) == 0
             || strncmp(arg, "FS", 2) == 0
             || strncmp(arg, "LT", 2) == 0
             || strncmp(arg, "NL", 2) == 0
             || strncmp(arg, "NT", 2) == 0
             || strncmp(arg, "LK", 2) == 0
             || strncmp(arg, "MF", 2) == 0
             || strncmp(arg, "MG", 2) == 0
             || strncmp(arg, "PC", 2) == 0
             || strncmp(arg, "RA", 2) == 0
             || strncmp(arg, "RG", 2) == 0
             || strncmp(arg, "RL", 2) == 0
             || strncmp(arg, "SC", 2) == 0
             || strncmp(arg, "SH", 2) == 0
             || strncmp(arg, "TN", 2) == 0
             || strncmp(arg, "TO", 2) == 0
             || strncmp(arg, "TS", 2) == 0
             || strncmp(arg, "VX", 2) == 0
            )
    {
        char response[32];

        SNPRINTF(response, sizeof(response), "?;");
        return write_block2((void *)__func__, &my_com, response, strlen(response));
    }
    else
    {
        rig_debug(RIG_DEBUG_ERR,
                  "*********************************\n%s: unknown cmd='%s'\n",
                  __func__,
                  (char *)arg);
        return -RIG_EINVAL;
    }

    // Handle all the settings
    return RIG_OK;
}


void usage()
{
    char *name = "rigctlcom";
    printf("Usage: %s -m rignumber -r comport -s baud -R comport [OPTIONS]...\n\n"
           "A TS-2000 emulator for rig sharing with programs that don't support Hamlib or FLRig to be able\n"
           "to use a connected radio transceiver or receiver with FLRig or rigctld via Hamlib.\n\n",
           name);

    printf("Example: Using FLRig with virtual COM5/COM6 and other program:\n");
    printf("\t%s -m 4 -R COM5 -S 115200\n\n", name);
    printf("Other program would connect to COM6 and use TS-2000 115200 8N1\n\n");
    printf("See the %s.1 manual page for complete details.\n\n", name);

    printf(
        "  -m, --model=ID                select radio model number. See model list (-l)\n"
        "  -r, --rig-file=DEVICE         set device of the radio to operate on\n"
        "  -R, --rig-file2=DEVICE        set device of the virtual com port to operate on\n"
        "  -p, --ptt-file=DEVICE         set device of the PTT device to operate on\n"
        "  -d, --dcd-file=DEVICE         set device of the DCD device to operate on\n"
        "  -P, --ptt-type=TYPE           set type of the PTT device to operate on\n"
        "  -D, --dcd-type=TYPE           set type of the DCD device to operate on\n"
        "  -s, --serial-speed=BAUD       set serial speed of the serial port\n"
        "  -S, --serial-speed2=BAUD      set serial speed of the virtual com port [default=115200]\n"
        "  -c, --civaddr=ID              set CI-V address, decimal (for Icom rigs only)\n"
        "  -C, --set-conf=PARM=VAL       set config parameters\n"
        "  -B, --mapa2b                  maps set_freq on VFOA to VFOB -- useful for CW Skimmer\n"
        "  -L, --show-conf               list all config parameters\n"
        "  -l, --list                    list all model numbers and exit\n"
        "  -u, --dump-caps               dump capabilities and exit\n"
        "  -v, --verbose                 set verbose mode, cumulative (-v to -vvvvv)\n"
        "  -Z, --debug-time-stamps       enable time stamps for debug messages\n"
        "  -h, --help                    display this help and exit\n"
        "  -V, --version                 output version information and exit\n\n"
    );

    printf("\nReport bugs to <hamlib-developer@lists.sourceforge.net>.\n");

}
