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

#include <sys/types.h>

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
#define SHORT_OPTIONS "m:r:R:p:d:P:D:s:S:c:C:lLuovhVZ"
static struct option long_options[] =
{
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
    {"vfo",             0, 0, 'o'},
    {"verbose",         0, 0, 'v'},
    {"help",            0, 0, 'h'},
    {"version",         0, 0, 'V'},
    {"debug-time-stamps", 0, 0, 'Z'},
    {0, 0, 0, 0}
};


struct handle_data
{
    RIG *rig;
    int sock;
    struct sockaddr_storage cli_addr;
    socklen_t clilen;
};


void usage(void);
static int handle_ts2000(void *arg);

static RIG *my_rig;             /* handle to rig */
static hamlib_port_t my_com;           /* handle to virtual COM port */
static int verbose;

#ifdef HAVE_SIG_ATOMIC_T
static sig_atomic_t volatile ctrl_c;
#else
static int volatile ctrl_c;
#endif

int interactive = 1;    /* no cmd because of daemon */
int prompt = 0;         /* Daemon mode for rigparse return string */
int vfo_mode = 0;       /* vfo_mode=0 means target VFO is current VFO */

char send_cmd_term = '\r';  /* send_cmd termination char */

const char *portno = "4532";
const char *src_addr = NULL; /* INADDR_ANY */

#define MAXCONFLEN 128

#if 0
#ifdef WIN32
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
#endif

#if 0
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

        rig_debug(lvl, "%s: Network error %d: %s\n", msg, e, lpMsgBuf);
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
#endif


int main(int argc, char *argv[])
{
    rig_model_t my_model = RIG_MODEL_DUMMY;

    int retcode;        /* generic return code from functions */

    int show_conf = 0;
    int dump_caps_opt = 0;
    const char *rig_file = NULL, *rig_file2 = NULL, *ptt_file = NULL,
                *dcd_file = NULL;
    ptt_type_t ptt_type = RIG_PTT_NONE;
    dcd_type_t dcd_type = RIG_DCD_NONE;
    int serial_rate = 0;
    int serial_rate2 = 115200 /* virtual com port default speed */;
    char *civaddr = NULL;   /* NULL means no need to set conf */
    char conf_parms[MAXCONFLEN] = "";

    printf("%s Version 1.0\n",argv[0]);

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

        case 'R':
            if (!optarg)
            {
                usage();    /* wrong arg count */
                exit(1);
            }

            rig_file2 = optarg;
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

        case 'S':
printf("yup\n");
            if (!optarg)
            {
printf("nope\n");
                usage();    /* wrong arg count */
                exit(1);
            }

            serial_rate2 = atoi(optarg);
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

            strncat(conf_parms, optarg, MAXCONFLEN - strlen(conf_parms));
            break;

        case 'o':
            vfo_mode++;
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
            usage();    /* unknown option? */
            exit(1);
        }
    }

    rig_set_debug(verbose);

    rig_debug(RIG_DEBUG_VERBOSE, "%s, %s\n", argv[0], hamlib_version);
    rig_debug(RIG_DEBUG_VERBOSE,
              "Report bugs to <hamlib-developer@lists.sourceforge.net>\n\n");

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

    strncpy(my_rig->state.rigport.pathname, rig_file, FILPATHLEN - 1);

    if (!rig_file2)
    {
        fprintf(stderr, "-R com port not provided\n");
        exit(2);
    }

    strncpy(my_com.pathname, rig_file2, FILPATHLEN - 1);

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
    my_com.type.rig=RIG_PORT_SERIAL;
    my_com.parm.serial.data_bits = 8;
    my_com.parm.serial.stop_bits = 1;
    my_com.timeout = 5000;
    my_com.parm.serial.parity = RIG_PARITY_NONE;
    my_com.parm.serial.handshake = RIG_HANDSHAKE_NONE;
    int status = port_open(&my_com);

    if (status != 0)
    {
        rig_debug(RIG_DEBUG_ERR,"Unable to open %s\n",my_com.pathname);
        return status;
    }
    if (verbose > 0)
    {
        fprintf(stderr, " %s opened for application program\n", my_com.pathname);
    }

    do
    {
        char ts2000[1024];
        char *stop_set = ";\n\r";
        memset(ts2000,0,sizeof(ts2000));
        serial_flush(&my_com); // get rid of anything in the queue
        status = read_string(&my_com, ts2000, sizeof(ts2000), stop_set, strlen(stop_set));
        rig_debug(RIG_DEBUG_TRACE,"%s: status=%d\n",__func__,status);
        if (strlen(ts2000) > 0)
        {
            int retval = handle_ts2000(ts2000);
            if (retval != RIG_OK) {
              rig_debug(RIG_DEBUG_ERR,"%s: %s\n",__func__,rigerror(retval));
            }
        }

    }
    while (retcode == 0 && !ctrl_c);

    rig_close(my_rig); /* close port */
    rig_cleanup(my_rig); /* if you care about memory */

    return 0;
}


static rmode_t ts2000_get_mode()
{
  rmode_t mode;
  pbwidth_t width;
  rig_get_mode(my_rig,RIG_VFO_A,&mode,&width);
        // Perhaps we should emulate a rig that has PKT modes instead??
        switch(mode) {
          case RIG_MODE_LSB:   mode = 1;break;
          case RIG_MODE_USB:   mode = 2;break;
          case RIG_MODE_CW:    mode = 3;break;
          case RIG_MODE_FM:    mode = 4;break;
          case RIG_MODE_AM:    mode = 5;break;
          case RIG_MODE_RTTY:  mode = 6;break;
          case RIG_MODE_CWR:   mode = 7;break;
          case RIG_MODE_NONE:  mode = 8;break;
          case RIG_MODE_RTTYR: mode = 9;break;
          default: mode = 0; break;
        }
  return mode;
}

static int write_block2(void *func, hamlib_port_t *p, const char *txbuffer, size_t count)
{
  int retval = write_block(p,txbuffer,count);
  usleep(5000);
  if (retval != RIG_OK) {
    rig_debug(RIG_DEBUG_ERR,"%s: %s\n",func,rigerror(retval));
  }
  return retval;
}

/*
 * This handles the TS-2000 emulation
 */
static int handle_ts2000(void *arg)
{
    // Handle all the queries
    if (strcmp(arg, "ID;") == 0)
    {
        char *reply = "ID019;";
        return write_block2((void*)__func__,&my_com,reply,strlen(reply));
    }
    if (strcmp(arg, "AI;") == 0)
    {
        char *reply = "AI0;";
        return write_block2((void*)__func__,&my_com,reply,strlen(reply));
    }
    else if (strcmp(arg, "IF;") == 0)
    {
        freq_t freq;          // P1
        int freq_step = 10;    // P2 just use default value for now
        int rit_xit_freq = 0; // P3 dummy value for now
        int rit = 0;          // P4 dummy value for now
        int xit = 0;          // P5 dummy value for now
        int bank1 = 0;        // P6 dummy value for now
        int bank2 =0;         // P7 dummy value for now
        ptt_t ptt;            // P8 
        rmode_t mode;         // P9
        vfo_t vfo;            // P10
        int scan=0;           // P11 dummy value for now
        split_t split=0;      // P12
        int p13=0;            // P13 Tone dummy value for now
        int p14=0;            // P14 Tone Freq dummy value for now
        int p15=0;            // P15 Shift status dummy value for now
        char response[64];
        int retval = rig_get_freq(my_rig,RIG_VFO_A,&freq);
        if (retval != RIG_OK) return retval;
        mode = ts2000_get_mode();
        retval = rig_get_ptt(my_rig,RIG_VFO_A,&ptt);
        if (retval != RIG_OK) return retval;
        if (ptt) 
        retval = rig_get_split_vfo(my_rig,RIG_VFO_CURR,&split,&vfo);
        else 
        retval = rig_get_vfo(my_rig,&vfo);

        if (retval != RIG_OK) return retval;
        switch(vfo) {
          case RIG_VFO_A: vfo=0;break;
          case RIG_VFO_B: vfo=1;break;
          default: rig_debug(RIG_DEBUG_ERR,"%s: unexpected vfo=%d\n",__func__,vfo);
        }
        snprintf(response,sizeof(response),"IF%011"PRIll"%04d+%05d%1d%1d%1d%02d%1d%1"PRIll"%1d%1d%1d%1d%02d%1d;",(uint64_t)freq,freq_step,rit_xit_freq,rit,xit,bank1,bank2,ptt,mode,vfo,scan,split,p13,p14,p15);
        return write_block2((void*)__func__,&my_com,response,strlen(response));
    }
    else if (strcmp(arg,"MD;") == 0) {
        rmode_t mode = ts2000_get_mode();
        char response[32];
        snprintf(response,sizeof(response),"MD%1d;",(int)mode);
        return write_block2((void*)__func__,&my_com,response,strlen(response));
    }
    else if (strcmp(arg,"AG0;") == 0) {
        char response[32];
        snprintf(response,sizeof(response),"AG0000;");
        return write_block2((void*)__func__,&my_com,response,strlen(response));
    }
    else if (strcmp(arg, "FA;") == 0)
    {
        freq_t freq=0;
        int retval = rig_get_freq(my_rig,RIG_VFO_A,&freq);
        if (retval != RIG_OK) return retval;
        char response[32];
        snprintf(response,sizeof(response),"FA%011"PRIll";",(uint64_t)freq);
        return write_block2((void*)__func__,&my_com,response,strlen(response));
    }
    else if (strcmp(arg, "FB;") == 0)
    {
        freq_t freq=0;
        int retval = rig_get_freq(my_rig,RIG_VFO_B,&freq);
        if (retval != RIG_OK) return retval;
        char response[32];
        snprintf(response,sizeof(response),"FB%011"PRIll";",(uint64_t)freq);
        return write_block2((void*)__func__,&my_com,response,strlen(response));
    }
    else if (strcmp(arg, "SA;") == 0)
    {
      char response[32];
      snprintf(response,sizeof(response),"SA0;");
      return write_block2((void*)__func__,&my_com,response,strlen(response));
    }
    else if (strcmp(arg, "RX;") == 0)
    {
      char response[32];
      snprintf(response,sizeof(response),"RX0;");
      return write_block2((void*)__func__,&my_com,response,strlen(response));
    }
    // Now some commands to set things
    else if (strcmp(arg, "TX;") == 0)
    {
      return rig_set_ptt(my_rig,RIG_VFO_A,1);
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
      return rig_set_vfo(my_rig,RIG_VFO_A);
    }
    else if (strcmp(arg, "FR1;") == 0)
    {
      return rig_set_vfo(my_rig,RIG_VFO_B);
    }
    else if (strcmp(arg, "FT0;") == 0)
    {
      return rig_set_split_vfo(my_rig,RIG_VFO_A,RIG_VFO_A,0);
    }
    else if (strcmp(arg, "FT1;") == 0)
    {
      return rig_set_split_vfo(my_rig,RIG_VFO_B,RIG_VFO_B,0);
    }
    else if (strncmp(arg, "FA0", 3) == 0)
    {
      freq_t freq;
      sscanf(arg+2, "%"SCNfreq, &freq);
      return rig_set_freq(my_rig,RIG_VFO_A,freq);
    }
    else if (strncmp(arg, "FB0", 3) == 0)
    {
      freq_t freq;
      sscanf(arg+2, "%"SCNfreq, &freq);
      return rig_set_freq(my_rig,RIG_VFO_A,freq);
    }
    else if (strncmp(arg, "MD", 2) == 0)
    {
      mode_t mode = 0;
      int imode = 0;
      sscanf(arg+2, "%d", &imode);
      switch(imode) {
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
      char response[32];
      snprintf(response,sizeof(response),"MD%c;",mode+'0');
      return write_block2((void*)__func__,&my_com,response,strlen(response));
    }
    else {
        rig_debug(RIG_DEBUG_ERR,"*********************************\n%s: unknown cmd='%s'\n",__func__,arg);
        //exit(1);
        return -RIG_EINVAL;
    }
    // Handle all the settings
    return RIG_OK;
}


void usage(void)
{
    printf("Usage: rigctlcom [OPTION]...\n"
           "TS-2000 emulator for programs that don't know hamlib to a connected radio transceiver or receiver.\n\n");
    printf("Example: Using FLRig with virtual COM5/COM6 and other program\n");
    printf("         rigctlcom -m 4 -R COM5 -S 115200\n");
    printf("         Other program would connect to COM6 and use TS-2000 115200 8N1\n\n");

    printf(
        "  -m, --model=ID                select radio model number. See model list\n"
        "  -r, --rig-file=DEVICE         set device of the radio to operate on\n"
        "  -R, --rig-file2=DEVICE        set device of the virtual com port to operate on\n"
        "  -p, --ptt-file=DEVICE         set device of the PTT device to operate on\n"
        "  -d, --dcd-file=DEVICE         set device of the DCD device to operate on\n"
        "  -P, --ptt-type=TYPE           set type of the PTT device to operate on\n"
        "  -D, --dcd-type=TYPE           set type of the DCD device to operate on\n"
        "  -s, --serial-speed=BAUD       set serial speed of the serial port\n"
        "  -S, --serial-speed=BAUD       set serial speed of the virtual com port\n"
        "  -c, --civaddr=ID              set CI-V address, decimal (for Icom rigs only)\n"
        "  -t, --port=NUM                set TCP listening port, default %s\n"
        "  -T, --listen-addr=IPADDR      set listening IP address, default ANY\n"
        "  -C, --set-conf=PARM=VAL       set config parameters\n"
        "  -L, --show-conf               list all config parameters\n"
        "  -l, --list                    list all model numbers and exit\n"
        "  -u, --dump-caps               dump capabilities and exit\n"
        "  -o, --vfo                     do not default to VFO_CURR, require extra vfo arg\n"
        "  -v, --verbose                 set verbose mode, cumulative (-v to -vvvvv)\n"
        "  -Z, --debug-time-stamps       enable time stamps for debug messages\n"
        "  -h, --help                    display this help and exit\n"
        "  -V, --version                 output version information and exit\n\n",
        portno);

    printf("\nReport bugs to <hamlib-developer@lists.sourceforge.net>.\n");

}
