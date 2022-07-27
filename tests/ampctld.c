/*
 * ampctld.c - (C) Stephane Fillod 2000-2011
 *             (C) Nate Bargmann 2010,2011,2012,2013
 *
 * This program test/control an amplifier using Hamlib.
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

#include <hamlib/config.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <ctype.h>

#include <getopt.h>
#include <errno.h>
#include <signal.h>

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

#include <hamlib/amplifier.h>
#include "misc.h"

#include "ampctl_parse.h"

struct handle_data
{
    AMP *amp;
    int sock;
    struct sockaddr_storage cli_addr;
    socklen_t clilen;
};

void *handle_socket(void *arg);

void usage();

/*
 * Reminder: when adding long options,
 * keep up to date SHORT_OPTIONS, usage()'s output and man page. thanks.
 * NB: do NOT use -W since it's reserved by POSIX.
 * TODO: add an option to read from a file
 */
#define SHORT_OPTIONS "m:r:s:C:t:T:LuvhVlZ"
static struct option long_options[] =
{
    {"model",           1, 0, 'm'},
    {"amp-file",        1, 0, 'r'},
    {"serial-speed",    1, 0, 's'},
    {"port",            1, 0, 't'},
    {"listen-addr",     1, 0, 'T'},
    {"list",            0, 0, 'l'},
    {"set-conf",        1, 0, 'C'},
    {"show-conf",       0, 0, 'L'},
    {"dump-caps",       0, 0, 'u'},
    {"debug-time-stamps", 0, 0, 'Z'},
    {"verbose",         0, 0, 'v'},
    {"help",            0, 0, 'h'},
    {"version",         0, 0, 'V'},
    {0, 0, 0, 0}
};

int interactive = 1;    /* no cmd because of daemon */
int prompt = 0 ;        /* Daemon mode for rigparse return string */

const char *portno = "4531";
const char *src_addr = NULL;    /* INADDR_ANY */

char send_cmd_term = '\r';      /* send_cmd termination char */

#define MAXCONFLEN 1024


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
                      NULL,
                      e,
                      // Default language
                      MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT),
                      (LPTSTR)&lpMsgBuf,
                      0,
                      NULL))
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
    AMP *my_amp;        /* handle to amp (instance) */
    amp_model_t my_model = AMP_MODEL_DUMMY;

    int retcode;        /* generic return code from functions */

    int verbose = 0;
    int show_conf = 0;
    int dump_caps_opt = 0;
    const char *amp_file = NULL;
    int serial_rate = 0;
    char conf_parms[MAXCONFLEN] = "";

    struct addrinfo hints, *result, *saved_result;
    int sock_listen;
    int reuseaddr = 1;
    char host[NI_MAXHOST];
    char serv[NI_MAXSERV];

#ifdef HAVE_PTHREAD
    pthread_t thread;
    pthread_attr_t attr;
#endif
    struct handle_data *arg;
#ifdef SIGPIPE
#if HAVE_SIGACTION
    struct sigaction act;
#endif
#endif

    while (1)
    {
        int c;
        int option_index = 0;
        char dummy[2];

        c = getopt_long(argc, argv, SHORT_OPTIONS, long_options, &option_index);

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

            amp_file = optarg;
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

        case 'Z':
            rig_set_debug_time_stamp(1);
            break;

        default:
            usage();    /* unknown option? */
            exit(1);
        }
    }

    rig_set_debug(verbose);

    rig_debug(RIG_DEBUG_VERBOSE, "ampctld, %s\n", hamlib_version2);
    rig_debug(RIG_DEBUG_VERBOSE, "%s",
              "Report bugs to <hamlib-developer@lists.sourceforge.net>\n\n");

    my_amp = amp_init(my_model);

    if (!my_amp)
    {
        fprintf(stderr,
                "Unknown amp num %d, or initialization error.\n",
                my_model);

        fprintf(stderr, "Please check with --list option.\n");
        exit(2);
    }

#if 0
    retcode = set_conf(my_amp, conf_parms);

    if (retcode != RIG_OK)
    {
        fprintf(stderr, "Config parameter error: %s\n", rigerror(retcode));
        exit(2);
    }

#endif

    if (amp_file)
    {
        strncpy(my_amp->state.ampport.pathname, amp_file, HAMLIB_FILPATHLEN - 1);
    }

    /* FIXME: bound checking and port type == serial */
    if (serial_rate != 0)
    {
        my_amp->state.ampport.parm.serial.rate = serial_rate;
    }

#if 0

    /*
     * print out conf parameters
     */
    if (show_conf)
    {
        amp_token_foreach(my_amp, print_conf_list, (rig_ptr_t)my_amp);
    }

#endif

    /*
     * Print out conf parameters, and exits immediately as we may be
     * interested only in only caps, and rig_open may fail.
     */
    if (dump_caps_opt)
    {
        dumpcaps_amp(my_amp, stdout);
        amp_cleanup(my_amp);    /* if you care about memory */
        exit(0);
    }

    retcode = amp_open(my_amp);

    if (retcode != RIG_OK)
    {
        fprintf(stderr, "amp_open: error = %s \n", rigerror(retcode));
        exit(2);
    }

    if (verbose > 0)
    {
        printf("Opened amp model %d, '%s'\n",
               my_amp->caps->amp_model,
               my_amp->caps->model_name);
    }

    rig_debug(RIG_DEBUG_VERBOSE,
              "Backend version: %s, Status: %s\n",
              my_amp->caps->version,
              rig_strstatus(my_amp->caps->status));

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
        setsockopt(INVALID_SOCKET,
                   SOL_SOCKET,
                   SO_OPENTYPE,
                   (char *)&sockopt,
                   sizeof(sockopt));
    }

#endif

    /*
     * Prepare listening socket
     */
    memset(&hints, 0, sizeof(struct addrinfo));
    hints.ai_family = AF_UNSPEC;        /* Allow IPv4 or IPv6 */
    hints.ai_socktype = SOCK_STREAM;    /* TCP socket */
    hints.ai_flags = AI_PASSIVE;        /* For wildcard IP address */
    hints.ai_protocol = 0;              /* Any protocol */

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
            freeaddrinfo(result);   /* No longer needed */
            exit(1);
        }

        if (setsockopt(sock_listen, SOL_SOCKET, SO_REUSEADDR,
                       (char *)&reuseaddr, sizeof(reuseaddr)) < 0)
        {

            handle_error(RIG_DEBUG_ERR, "setsockopt");
            freeaddrinfo(result);   /* No longer needed */
            exit(1);
        }

#ifdef IPV6_V6ONLY

        if (AF_INET6 == result->ai_family)
        {
            /* allow IPv4 mapped to IPv6 clients, MS & BSD default this
               to 1 i.e. disallowed */
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

#ifdef SIGPIPE
    /* Ignore SIGPIPE as we will handle it at the write()/send() calls
       that will consequently fail with EPIPE. All child threads will
       inherit this disposition which is what we want. */
#if HAVE_SIGACTION
    memset(&act, 0, sizeof act);
    act.sa_handler = SIG_IGN;
    act.sa_flags = SA_RESTART;

    if (sigaction(SIGPIPE, &act, NULL))
    {
        handle_error(RIG_DEBUG_ERR, "sigaction");
    }

#elif HAVE_SIGNAL

    if (SIG_ERR == signal(SIGPIPE, SIG_IGN))
    {
        handle_error(RIG_DEBUG_ERR, "signal");
    }

#endif
#endif

    /*
     * main loop accepting connections
     */
    do
    {
        arg = calloc(1, sizeof(struct handle_data));

        if (!arg)
        {
            rig_debug(RIG_DEBUG_ERR, "calloc: %s\n", strerror(errno));
            exit(1);
        }

        arg->amp = my_amp;
        arg->clilen = sizeof(arg->cli_addr);
        arg->sock = accept(sock_listen,
                           (struct sockaddr *) &arg->cli_addr,
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
                                   NI_NUMERICHOST | NI_NUMERICSERV))
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

    while (retcode == 0);

    amp_close(my_amp); /* close port */
    amp_cleanup(my_amp); /* if you care about memory */

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
    FILE *fsockin;
    FILE *fsockout;
    int retcode;
    char host[NI_MAXHOST];
    char serv[NI_MAXSERV];

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
        rig_debug(RIG_DEBUG_ERR, "fdopen in: %s\n", strerror(errno));
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

    do
    {
        retcode = ampctl_parse(handle_data_arg->amp, fsockin, fsockout, NULL, 0);

        if (ferror(fsockin) || ferror(fsockout))
        {
            retcode = 1;
        }
    }
    while (retcode == 0 || retcode == 2);

    if ((retcode = getnameinfo((struct sockaddr const *)&handle_data_arg->cli_addr,
                               handle_data_arg->clilen,
                               host,
                               sizeof(host),
                               serv,
                               sizeof(serv),
                               NI_NUMERICHOST | NI_NUMERICSERV))
            < 0)
    {

        rig_debug(RIG_DEBUG_WARN,
                  "Peer lookup error: %s",
                  gai_strerror(retcode));
    }

    rig_debug(RIG_DEBUG_VERBOSE,
              "Connection closed from %s:%s\n",
              host,
              serv);

    fclose(fsockin);
#ifndef __MINGW32__
    fclose(fsockout);
#endif

handle_exit:
#ifdef __MINGW32__
    closesocket(handle_data_arg->sock);
#else
    close(handle_data_arg->sock);
#endif
    free(arg);

#ifdef HAVE_PTHREAD
    pthread_exit(NULL);
#endif
    return NULL;
}


void usage()
{
    printf("Usage: ampctld [OPTION]... [COMMAND]...\n"
           "Daemon serving COMMANDs to a connected amplifier.\n\n");

    printf(
        "  -m, --model=ID                select amplifier model number. See model list\n"
        "  -r, --amp-file=DEVICE         set device of the amplifier to operate on\n"
        "  -s, --serial-speed=BAUD       set serial speed of the serial port\n"
        "  -t, --port=NUM                set TCP listening port, default %s\n"
        "  -T, --listen-addr=IPADDR      set listening IP address, default ANY\n"
        "  -C, --set-conf=PARM=VAL       set config parameters\n"
        "  -L, --show-conf               list all config parameters\n"
        "  -l, --list                    list all model numbers and exit\n"
        "  -u, --dump-caps               dump capabilities and exit\n"
        "  -v, --verbose                 set verbose mode, cumulative\n"
        "  -Z, --debug-time-stamps       enable time stamps for debug messages\n"
        "  -h, --help                    display this help and exit\n"
        "  -V, --version                 output version information and exit\n\n",
        portno);

    usage_amp(stdout);

    printf("\nReport bugs to <hamlib-developer@lists.sourceforge.net>.\n");
}
