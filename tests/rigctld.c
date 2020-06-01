/*
 * rigctld.c - (C) Stephane Fillod 2000-2011
 *             (C) Nate Bargmann 2008,2010,2011,2012,2013
 *             (C) The Hamlib Group 2012
 *
 * This program test/control a radio using Hamlib.
 * It takes commands from network connection.
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

#ifdef WIN32
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#endif

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <ctype.h>
#include <errno.h>
#include <signal.h>

#include <getopt.h>

#include <sys/types.h>          /* See NOTES */

#ifdef HAVE_NETINET_IN_H
#  include <netinet/in.h>
#endif

#ifdef HAVE_ARPA_INET_H
#  include <arpa/inet.h>
#endif

#ifdef HAVE_SYS_SOCKET_H
#  include <sys/socket.h>
#elif HAVE_WS2TCPIP_H
#  include <ws2tcpip.h>
#  include <fcntl.h>
#  if defined(HAVE_WSPIAPI_H)
#    include <wspiapi.h>
#  endif
#endif

#ifdef HAVE_NETDB_H
#  include <netdb.h>
#endif

#ifdef HAVE_PTHREAD
#  include <pthread.h>
#endif

#include <hamlib/rig.h>
#include "hamlibdatetime.h"
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
#define SHORT_OPTIONS "m:r:p:d:P:D:s:c:T:t:C:X:lLuovhVZ"
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
    {"listen-addr",     1, 0, 'T'},
    {"port",            1, 0, 't'},
    {"set-conf",        1, 0, 'C'},
    {"list",            0, 0, 'l'},
    {"show-conf",       0, 0, 'L'},
    {"dump-caps",       0, 0, 'u'},
    {"vfo",             0, 0, 'o'},
    {"verbose",         0, 0, 'v'},
    {"help",            0, 0, 'h'},
    {"version",         0, 0, 'V'},
    {"twiddle_timeout", 1, 0, 'X'},
    {"debug-time-stamps", 0, 0, 'Z'},
    {0, 0, 0, 0}
};


struct handle_data
{
    RIG *rig;
    int sock;
    struct sockaddr_storage cli_addr;
    socklen_t clilen;
    int vfo_mode;
};


void *handle_socket(void *arg);
void usage(void);


#ifdef HAVE_PTHREAD
static unsigned client_count;
#endif

static RIG *my_rig;             /* handle to rig (instance) */
static int verbose;

#ifdef HAVE_SIG_ATOMIC_T
static sig_atomic_t volatile ctrl_c;
#else
static int volatile ctrl_c;
#endif

const char *portno = "4532";
const char *src_addr = NULL; /* INADDR_ANY */

#define MAXCONFLEN 1024

static void sync_callback(int lock)
{
#ifdef HAVE_PTHREAD
    static pthread_mutex_t client_lock = PTHREAD_MUTEX_INITIALIZER;

    if (lock)
    {
        pthread_mutex_lock(&client_lock);
        rig_debug(RIG_DEBUG_VERBOSE, "%s: client lock engaged\n", __func__);
    }
    else
    {
        rig_debug(RIG_DEBUG_VERBOSE, "%s: client lock disengaged\n", __func__);
        pthread_mutex_unlock(&client_lock);
    }

#endif
}

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

    int show_conf = 0;
    int dump_caps_opt = 0;
    const char *rig_file = NULL, *ptt_file = NULL, *dcd_file = NULL;
    ptt_type_t ptt_type = RIG_PTT_NONE;
    dcd_type_t dcd_type = RIG_DCD_NONE;
    int serial_rate = 0;
    char *civaddr = NULL;   /* NULL means no need to set conf */
    char conf_parms[MAXCONFLEN] = "";

    struct addrinfo hints, *result, *saved_result;
    int sock_listen;
    int reuseaddr = 1;
    int twiddle = 0;
    char host[NI_MAXHOST];
    char serv[NI_MAXSERV];
#if HAVE_SIGACTION
    struct sigaction act;
#endif

#ifdef HAVE_PTHREAD
    pthread_t thread;
    pthread_attr_t attr;
#endif
    struct handle_data *arg;
    int vfo_mode = 0; /* vfo_mode=0 means target VFO is current VFO */

    while (1)
    {
        int c;
        int option_index = 0;

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

        case 't':
            if (!optarg)
            {
                usage();    /* wrong arg count */
                exit(1);
            }

            portno = optarg;
            break;

        case 'T':
            if (!optarg)
            {
                usage();    /* wrong arg count */
                exit(1);
            }

            src_addr = optarg;
            break;

        case 'o':
            vfo_mode++;
            rig_debug(RIG_DEBUG_ERR, "%s: #0 vfo_mode=%d\n", __func__, vfo_mode);
            break;

        case 'v':
            verbose++;
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

        case 'X':
            if (!optarg)
            {
                usage();    /* wrong arg count */
                exit(1);
            }

            twiddle = atoi(optarg);
            break;

        case 'Z':
            rig_set_debug_time_stamp(1);
            break;

        default:
            usage();    /* unknown option? */
            exit(1);
        }
    }

    if (!vfo_mode)
    {
        printf("Recommend using --vfo switch for rigctld if client supports it\n");
        printf("rigctl and netrigctl will automatically detect vfo mode\n");
    }

    rig_set_debug(verbose);

    rig_debug(RIG_DEBUG_VERBOSE, "rigctld %s\nLast commit was %s\n", hamlib_version,
              HAMLIBDATETIME);
    rig_debug(RIG_DEBUG_VERBOSE, "%s",
              "Report bugs to <hamlib-developer@lists.sourceforge.net>\n\n");

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

    my_rig->state.twiddle_timeout = twiddle;

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
        rig_token_foreach(my_rig, print_conf_list, (rig_ptr_t)my_rig);
    }

    /*
     * print out conf parameters, and exits immediately
     * We may be interested only in only caps, and rig_open may fail.
     */
    if (dump_caps_opt)
    {
        dumpcaps(my_rig, stdout);
        rig_cleanup(my_rig); /* if you care about memory */
        exit(0);
    }

    /* open and close rig connection to check early for issues */
    retcode = rig_open(my_rig);

    if (retcode != RIG_OK)
    {
        fprintf(stderr, "rig_open: error = %s \n", rigerror(retcode));
        exit(2);
    }

    if (verbose > RIG_DEBUG_ERR)
    {
        printf("Opened rig model %u, '%s'\n",
               my_rig->caps->rig_model,
               my_rig->caps->model_name);
    }

    rig_debug(RIG_DEBUG_VERBOSE, "Backend version: %s, Status: %s\n",
              my_rig->caps->version, rig_strstatus(my_rig->caps->status));

#if 0
    rig_close(my_rig);          /* we will reopen for clients */

    if (verbose > RIG_DEBUG_ERR)
    {
        printf("Closed rig model %d, '%s - will reopen for clients'\n",
               my_rig->caps->rig_model,
               my_rig->caps->model_name);
    }

#endif

#ifdef __MINGW32__
#  ifndef SO_OPENTYPE
#    define SO_OPENTYPE     0x7008
#  endif
#  ifndef SO_SYNCHRONOUS_NONALERT
#    define SO_SYNCHRONOUS_NONALERT 0x20
#  endif
#  ifndef INVALID_SOCKET
#    define INVALID_SOCKET -1
#  endif

    WSADATA wsadata;

    if (WSAStartup(MAKEWORD(1, 1), &wsadata) == SOCKET_ERROR)
    {
        fprintf(stderr, "WSAStartup socket error\n");
        exit(1);
    }

    {
        int sockopt = SO_SYNCHRONOUS_NONALERT;
        setsockopt(INVALID_SOCKET, SOL_SOCKET, SO_OPENTYPE, (char *)&sockopt,
                   sizeof(sockopt));
    }

#endif

    /*
     * Prepare listening socket
     */
    memset(&hints, 0, sizeof(struct addrinfo));
    hints.ai_family = AF_UNSPEC;    /* Allow IPv4 or IPv6 */
    hints.ai_socktype = SOCK_STREAM;/* TCP socket */
    hints.ai_flags = AI_PASSIVE;    /* For wildcard IP address */
    hints.ai_protocol = 0;          /* Any protocol */

    retcode = getaddrinfo(src_addr, portno, &hints, &result);

    if (retcode != 0)
    {
        fprintf(stderr, "getaddrinfo: %s\n", gai_strerror(retcode));
        exit(2);
    }

    saved_result = result;

    do
    {
        sock_listen = socket(result->ai_family,
                             result->ai_socktype,
                             result->ai_protocol);

        if (sock_listen < 0)
        {
            handle_error(RIG_DEBUG_ERR, "socket");
            freeaddrinfo(saved_result);     /* No longer needed */
            exit(2);
        }

        if (setsockopt(sock_listen,
                       SOL_SOCKET,
                       SO_REUSEADDR,
                       (char *)&reuseaddr,
                       sizeof(reuseaddr))
                < 0)
        {

            handle_error(RIG_DEBUG_ERR, "setsockopt");
            freeaddrinfo(saved_result);     /* No longer needed */
            exit(1);
        }

#ifdef IPV6_V6ONLY

        if (AF_INET6 == result->ai_family)
        {
            /* allow IPv4 mapped to IPv6 clients Windows and BSD default
               this to 1 (i.e. disallowed) and we prefer it off */
            int sockopt = 0;

            if (setsockopt(sock_listen,
                           IPPROTO_IPV6,
                           IPV6_V6ONLY,
                           (char *)&sockopt,
                           sizeof(sockopt))
                    < 0)
            {

                handle_error(RIG_DEBUG_ERR, "setsockopt");
                freeaddrinfo(saved_result);     /* No longer needed */
                exit(1);
            }
        }

#endif

        if (0 == bind(sock_listen, result->ai_addr, result->ai_addrlen))
        {
            break;
        }

        handle_error(RIG_DEBUG_WARN, "binding failed (trying next interface)");
#ifdef __MINGW32__
        closesocket(sock_listen);
#else
        close(sock_listen);
#endif
    }
    while ((result = result->ai_next) != NULL);

    freeaddrinfo(saved_result);     /* No longer needed */

    if (NULL == result)
    {
        rig_debug(RIG_DEBUG_ERR, "%s: bind error - no available interface\n", __func__);
        exit(1);
    }

    if (listen(sock_listen, 4) < 0)
    {
        handle_error(RIG_DEBUG_ERR, "listening");
        exit(1);
    }

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
#endif

    /*
     * main loop accepting connections
     */
    do
    {
        fd_set set;
        struct timeval timeout;

        arg = malloc(sizeof(struct handle_data));

        if (!arg)
        {
            rig_debug(RIG_DEBUG_ERR, "malloc: %s\n", strerror(errno));
            exit(1);
        }

        /* use select to allow for periodic checks for CTRL+C */
        FD_ZERO(&set);
        FD_SET(sock_listen, &set);
        timeout.tv_sec = 5;
        timeout.tv_usec = 0;
        retcode = select(sock_listen + 1, &set, NULL, NULL, &timeout);

        if (-1 == retcode)
        {
            rig_debug(RIG_DEBUG_ERR, "%s: select\n", __func__);
        }
        else if (!retcode)
        {
            if (ctrl_c)
            {
                break;
            }
        }
        else
        {
            arg->rig = my_rig;
            arg->clilen = sizeof(arg->cli_addr);
            arg->vfo_mode = vfo_mode;
            arg->sock = accept(sock_listen,
                               (struct sockaddr *)&arg->cli_addr,
                               &arg->clilen);

            if (arg->sock < 0)
            {
                handle_error(RIG_DEBUG_ERR, "accept");
                break;
            }

            if ((retcode = getnameinfo((struct sockaddr const *)&arg->cli_addr,
                                       arg->clilen,
                                       host,
                                       sizeof(host),
                                       serv,
                                       sizeof(serv),
                                       NI_NOFQDN))
                    < 0)
            {
                rig_debug(RIG_DEBUG_WARN,
                          "Peer lookup error: %s",
                          gai_strerror(retcode));
            }

            rig_debug(RIG_DEBUG_VERBOSE,
                      "Connection opened from %s:%s\n",
                      host,
                      serv);

#ifdef HAVE_PTHREAD
            pthread_attr_init(&attr);
            pthread_attr_setdetachstate(&attr, PTHREAD_CREATE_DETACHED);

            retcode = pthread_create(&thread, &attr, handle_socket, arg);

            if (retcode != 0)
            {
                rig_debug(RIG_DEBUG_ERR, "pthread_create: %s\n", strerror(retcode));
                break;
            }

#else
            handle_socket(arg);
#endif
        }
    }
    while (retcode == 0 && !ctrl_c);

#ifdef HAVE_PTHREAD
    /* allow threads to finish current action */
    sync_callback(1);

    if (client_count)
    {
        rig_debug(RIG_DEBUG_WARN, "%u outstanding client(s)\n", client_count);
    }

    rig_close(my_rig);
    sync_callback(0);
#else
    rig_close(my_rig); /* close port */
#endif
    rig_cleanup(my_rig); /* if you care about memory */

#ifdef __MINGW32__
    WSACleanup();
#endif

    return 0;
}


/*
 * This is the function run by the threads
 */
void *handle_socket(void *arg)
{
    struct handle_data *handle_data_arg = (struct handle_data *)arg;
    FILE *fsockin = NULL;
    FILE *fsockout = NULL;
    int retcode = RIG_OK;
    char host[NI_MAXHOST];
    char serv[NI_MAXSERV];
    char send_cmd_term = '\r';  /* send_cmd termination char */
    int ext_resp = 0;
    char resp_sep = '\n';

#ifdef __MINGW32__
    int sock_osfhandle = _open_osfhandle(handle_data_arg->sock, _O_RDONLY);

    if (sock_osfhandle == -1)
    {
        rig_debug(RIG_DEBUG_ERR, "_open_osfhandle error: %s\n", strerror(errno));
        goto handle_exit;
    }

    fsockin = _fdopen(sock_osfhandle,  "rb");
#else
    fsockin = fdopen(handle_data_arg->sock, "rb");
#endif

    if (!fsockin)
    {
        rig_debug(RIG_DEBUG_ERR, "fdopen(0x%d) in: %s\n", handle_data_arg->sock,
                  strerror(errno));
        goto handle_exit;
    }

#ifdef __MINGW32__
    fsockout = _fdopen(sock_osfhandle, "wb");
#else
    fsockout = fdopen(handle_data_arg->sock, "wb");
#endif

    if (!fsockout)
    {
        rig_debug(RIG_DEBUG_ERR, "fdopen out: %s\n", strerror(errno));
        fclose(fsockin);

        goto handle_exit;
    }

#ifdef HAVE_PTHREAD
    sync_callback(1);

//    ++client_count;
#if 0

    if (!client_count++)
    {
        retcode = rig_open(my_rig);

        if (RIG_OK == retcode && verbose > RIG_DEBUG_ERR)
        {
            printf("Opened rig model %d, '%s'\n",
                   my_rig->caps->rig_model,
                   my_rig->caps->model_name);
        }
    }

#endif

    sync_callback(0);
#else
    retcode = rig_open(my_rig);

    if (RIG_OK == retcode && verbose > RIG_DEBUG_ERR)
    {
        printf("Opened rig model %d, '%s'\n",
               my_rig->caps->rig_model,
               my_rig->caps->model_name);
    }

#endif

    do
    {
        rig_debug(RIG_DEBUG_TRACE, "%s: vfo_mode=%d\n", __func__,
                  handle_data_arg->vfo_mode);
        retcode = rigctl_parse(handle_data_arg->rig, fsockin, fsockout, NULL, 0,
                               sync_callback,
                               1, 0, &handle_data_arg->vfo_mode, send_cmd_term, &ext_resp, &resp_sep);

        if (retcode != 0) { rig_debug(RIG_DEBUG_ERR, "%s: rigctl_parse retcode=%d\n", __func__, retcode); }

        if (retcode == -1)
        {
            sleep(1);
            continue;
        }

        if (ferror(fsockin) || ferror(fsockout))
        {
            rig_debug(RIG_DEBUG_ERR, "%s: socket error in=%d, out=%d\n", __func__,
                      ferror(fsockin), ferror(fsockout));

            retcode = rig_close(my_rig);
            rig_debug(RIG_DEBUG_ERR, "%s: rig_close retcode=%d\n", __func__, retcode);
            retcode = rig_open(my_rig);
            rig_debug(RIG_DEBUG_ERR, "%s: rig_open retcode=%d\n", __func__, retcode);
        }
    }

    while (retcode == 0 || retcode == 2 || retcode == -RIG_ENAVAIL);

#ifdef HAVE_PTHREAD
#if 0
    sync_callback(1);

    /* Release rig if there are no clients */
    if (!--client_count)
    {
        rig_close(my_rig);

        if (verbose > RIG_DEBUG_ERR)
        {
            printf("Closed rig model %d, '%s - no clients, will reopen for new clients'\n",
                   my_rig->caps->rig_model,
                   my_rig->caps->model_name);
        }
    }

    sync_callback(0);
#endif
#else
    rig_close(my_rig);

    if (verbose > RIG_DEBUG_ERR)
    {
        printf("Closed rig model %d, '%s - will reopen for new clients'\n",
               my_rig->caps->rig_model,
               my_rig->caps->model_name);
    }

#endif

    if ((retcode = getnameinfo((struct sockaddr const *)&handle_data_arg->cli_addr,
                               handle_data_arg->clilen,
                               host,
                               sizeof(host),
                               serv,
                               sizeof(serv),
                               NI_NOFQDN))
            < 0)
    {

        rig_debug(RIG_DEBUG_WARN, "Peer lookup error: %s", gai_strerror(retcode));
    }

    rig_debug(RIG_DEBUG_VERBOSE,
              "Connection closed from %s:%s\n",
              host,
              serv);

handle_exit:

// for MINGW we close the handle before fclose
#ifdef __MINGW32__
    retcode = closesocket(handle_data_arg->sock);

    if (retcode != 0) { rig_debug(RIG_DEBUG_ERR, "%s: fclose(fsockin) %s\n", __func__, strerror(retcode)); }

#endif
    fclose(fsockin);
    fclose(fsockout);

// for everybody else we close the handle after fclose
#ifndef __MINGW32__
    retcode = close(handle_data_arg->sock);

    if (retcode != 0 && errno != EBADF) { rig_debug(RIG_DEBUG_ERR, "%s: close(handle_data_arg->sock) %s\n", __func__, strerror(errno)); }

#endif

    free(arg);

#ifdef HAVE_PTHREAD
    pthread_exit(NULL);
#endif
    return NULL;
}


void usage(void)
{
    printf("Usage: rigctld [OPTION]...\n"
           "Daemon serving COMMANDs to a connected radio transceiver or receiver.\n\n");


    printf(
        "  -m, --model=ID                select radio model number. See model list\n"
        "  -r, --rig-file=DEVICE         set device of the radio to operate on\n"
        "  -p, --ptt-file=DEVICE         set device of the PTT device to operate on\n"
        "  -d, --dcd-file=DEVICE         set device of the DCD device to operate on\n"
        "  -P, --ptt-type=TYPE           set type of the PTT device to operate on\n"
        "  -D, --dcd-type=TYPE           set type of the DCD device to operate on\n"
        "  -s, --serial-speed=BAUD       set serial speed of the serial port\n"
        "  -c, --civaddr=ID              set CI-V address, decimal (for Icom rigs only)\n"
        "  -t, --port=NUM                set TCP listening port, default %s\n"
        "  -T, --listen-addr=IPADDR      set listening IP address, default ANY\n"
        "  -C, --set-conf=PARM=VAL       set config parameters\n"
        "  -L, --show-conf               list all config parameters\n"
        "  -l, --list                    list all model numbers and exit\n"
        "  -u, --dump-caps               dump capabilities and exit\n"
        "  -o, --vfo                     do not default to VFO_CURR, require extra vfo arg\n"
        "  -v, --verbose                 set verbose mode, cumulative (-v to -vvvvv)\n"
        "  -W, --twiddle_timeout         timeout after detecting vfo manual change\n"
        "  -Z, --debug-time-stamps       enable time stamps for debug messages\n"
        "  -h, --help                    display this help and exit\n"
        "  -V, --version                 output version information and exit\n\n",
        portno);

    usage_rig(stdout);

    printf("\nReport bugs to <hamlib-developer@lists.sourceforge.net>.\n");

}
