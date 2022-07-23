/*
 * rigmem.c - (C) Thierry Leconte 2003-2005
 *            (C) Stephane Fillod 2003-2009
 *
 * This program exercises the backup and restore of a radio
 * using Hamlib.
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

#include <hamlib/rig.h>
#include "misc.h"
#include "sprintflst.h"

#define MAXNAMSIZ 32
#define MAXNBOPT 100    /* max number of different options */


/*
 * external prototype
 */

extern int xml_save(RIG *rig, const char *outfilename);
extern int xml_load(RIG *rig, const char *infilename);
extern int xml_parm_save(RIG *rig, const char *outfilename);
extern int xml_parm_load(RIG *rig, const char *infilename);

extern int csv_save(RIG *rig, const char *outfilename);
extern int csv_load(RIG *rig, const char *infilename);
extern int csv_parm_save(RIG *rig, const char *outfilename);
extern int csv_parm_load(RIG *rig, const char *infilename);

/*
 * Prototypes
 */
void usage();
void version();
int set_conf(RIG *rig, char *conf_parms);

int clear_chans(RIG *rig, const char *infilename);

/*
 * Reminder: when adding long options,
 *      keep up to date SHORT_OPTIONS, usage()'s output and man page. thanks.
 * NB: do NOT use -W since it's reserved by POSIX.
 */
#define SHORT_OPTIONS "m:r:s:c:C:p:axvhV"
static struct option long_options[] =
{
    {"model",           1, 0, 'm'},
    {"rig-file",        1, 0, 'r'},
    {"serial-speed",    1, 0, 's'},
    {"civaddr",         1, 0, 'c'},
    {"set-conf",        1, 0, 'C'},
    {"set-separator",   1, 0, 'p'},
    {"all",             0, 0, 'a'},
#ifdef HAVE_XML2
    {"xml",             0, 0, 'x'},
#endif
    {"verbose",         0, 0, 'v'},
    {"help",            0, 0, 'h'},
    {"version",         0, 0, 'V'},
    {0, 0, 0, 0}
};

#define MAXCONFLEN 1024

int all;

int main(int argc, char *argv[])
{
    RIG *rig;       /* handle to rig (nstance) */
    rig_model_t my_model = RIG_MODEL_DUMMY;

    int retcode;        /* generic return code from functions */

    int verbose = 0;
#ifdef HAVE_XML2
    int xml = 0;
#endif
    const char *rig_file = NULL;
    int serial_rate = 0;
    char *civaddr = NULL;   /* NULL means no need to set conf */
    char conf_parms[MAXCONFLEN] = "";
    extern char csv_sep;

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

            rig_file = optarg;
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

        case 'p':
            if (!optarg)
            {
                usage();    /* wrong arg count */
                exit(1);
            }

            csv_sep = optarg[0];
            break;

        case 'a':
            all++;
            break;
#ifdef HAVE_XML2

        case 'x':
            xml++;
            break;
#endif

        case 'v':
            verbose++;
            break;

        default:
            usage();    /* unknown option? */
            exit(1);
        }
    }

    rig_set_debug(verbose < 2 ? RIG_DEBUG_WARN : verbose);

    rig_debug(RIG_DEBUG_VERBOSE, "rigmem, %s\n", hamlib_version2);
    rig_debug(RIG_DEBUG_VERBOSE, "%s",
              "Report bugs to "
              "<hamlib-developer@lists.sourceforge.net>\n\n");

    if (optind + 1 >= argc)
    {
        usage();
        exit(1);
    }

    rig = rig_init(my_model);

    if (!rig)
    {
        fprintf(stderr,
                "Unknown rig num %u, or initialization error.\n",
                my_model);

        fprintf(stderr, "Please check with --list option.\n");
        exit(2);
    }

    retcode = set_conf(rig, conf_parms);

    if (retcode != RIG_OK)
    {
        fprintf(stderr, "Config parameter error: %s\n", rigerror(retcode));
        exit(2);
    }

    /* check channel support */
    if (rig->caps->set_channel == NULL
            && rig->caps->get_channel == NULL
            && rig->caps->set_chan_all_cb == NULL
            && rig->caps->get_chan_all_cb == NULL
            && (rig->caps->set_mem == NULL || rig->caps->set_vfo == NULL))
    {

        fprintf(stderr,
                "Error: rig num %u has no memory support implemented/available.\n",
                my_model);
        exit(3);
    }

    /* check channel description */
    if (rig->caps->chan_list[0].type == 0)
    {
        fprintf(stderr, "Error: rig num %u has no channel list.\n",
                my_model);
        exit(3);
    }

    if (rig_file)
    {
        strncpy(rig->state.rigport.pathname, rig_file, HAMLIB_FILPATHLEN - 1);
    }

    /* FIXME: bound checking and port type == serial */
    if (serial_rate != 0)
    {
        rig->state.rigport.parm.serial.rate = serial_rate;
    }

    if (civaddr)
    {
        rig_set_conf(rig, rig_token_lookup(rig, "civaddr"), civaddr);
    }

    retcode = rig_open(rig);

    if (retcode != RIG_OK)
    {
        fprintf(stderr, "rig_open: error = %s \n", rigerror(retcode));
        exit(2);
    }

    if (verbose > 0)
    {
        printf("Opened rig model %u, '%s'\n",
               rig->caps->rig_model,
               rig->caps->model_name);
    }

    rig_debug(RIG_DEBUG_VERBOSE,
              "Backend version: %s, Status: %s\n",
              rig->caps->version,
              rig_strstatus(rig->caps->status));

    /* on some rigs, this accelerates the backup/restore */
    rig_set_vfo(rig, RIG_VFO_MEM);

    if (!strcmp(argv[optind], "save"))
    {
#ifdef HAVE_XML2

        if (xml)
        {
            retcode = xml_save(rig, argv[optind + 1]);
        }
        else
#endif
        {
            retcode = csv_save(rig, argv[optind + 1]);
        }
    }
    else if (!strcmp(argv[optind], "load"))
    {
#ifdef HAVE_XML2

        if (xml)
        {
            retcode = xml_load(rig, argv[optind + 1]);
        }
        else
#endif
        {
            retcode = csv_load(rig, argv[optind + 1]);
        }
    }
    else if (!strcmp(argv[optind], "save_parm"))
    {
#ifdef HAVE_XML2

        if (xml)
        {
            retcode = xml_parm_save(rig, argv[optind + 1]);
        }
        else
#endif
        {
            retcode = csv_parm_save(rig, argv[optind + 1]);
        }
    }
    else if (!strcmp(argv[optind], "load_parm"))
    {
#ifdef HAVE_XML2

        if (xml)
        {
            retcode = xml_parm_load(rig, argv[optind + 1]);
        }
        else
#endif
        {
            retcode = csv_parm_load(rig, argv[optind + 1]);
        }
    }
    else if (!strcmp(argv[optind], "clear"))
    {
        retcode = clear_chans(rig, argv[optind + 1]);
    }
    else
    {
        usage();
        exit(1);
    }

    rig_close(rig);     /* close port */
    rig_cleanup(rig);   /* if you care about memory */

    if (retcode != 0)
    {
        fprintf(stderr, "Hamlib error: %s\n", rigerror(retcode));
        exit(2);
    }

    return 0;
}


void version()
{
    printf("rigmem, %s\n\n", hamlib_version2);
    printf("%s\n", hamlib_copyright);
}


void usage()
{
    printf("Usage: rigmem [OPTION]... COMMAND...FILE\n"
           "Backup/restore COMMANDs to a connected radio transceiver or receiver.\n\n");


    printf(
        "  -m, --model=ID                select radio model number. See model list\n"
        "  -r, --rig-file=DEVICE         set device of the radio to operate on\n"
        "  -s, --serial-speed=BAUD       set serial speed of the serial port\n"
        "  -c, --civaddr=ID              set CI-V address, decimal (for Icom rigs only)\n"
        "  -C, --set-conf=PARM=VAL       set config parameters\n"
        "  -p, --set-separator=SEP       set character separator instead of the CSV comma\n"
        "  -a, --all                     bypass mem_caps, apply to all fields of channel_t\n"
#ifdef HAVE_XML2
        "  -x, --xml                     use XML format instead of CSV\n"
#endif
        "  -v, --verbose                 set verbose mode, cumulative\n"
        "  -h, --help                    display this help and exit\n"
        "  -V, --version                 output version information and exit\n\n"
    );

    printf(
        "COMMANDs:\n"
        "  load\n"
        "  save\n"
        "  load_parm\n"
        "  save_parm\n"
        "  clear\n\n"
    );

    printf("\nReport bugs to <hamlib-developer@lists.sourceforge.net>.\n");

}


int set_conf(RIG *rig, char *conf_parms)
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
            return RIG_EINVAL;
        }

        *q++ = '\0';
        n = strchr(q, ',');

        if (n)
        {
            *n++ = '\0';
        }

        ret = rig_set_conf(rig, rig_token_lookup(rig, p), q);

        if (ret != RIG_OK)
        {
            return ret;
        }

        p = n;
    }

    return RIG_OK;
}


/*
 * Pretty nasty, clears everything you have in rig memory
 */
int clear_chans(RIG *rig, const char *infilename)
{
    int i, j, ret;
    channel_t chan;

    memset(&chan, 0, sizeof(chan));
    chan.freq = RIG_FREQ_NONE;
    chan.tx_freq = RIG_FREQ_NONE;
    chan.mode = RIG_MODE_NONE;
    chan.tx_mode = RIG_MODE_NONE;
    chan.vfo = RIG_VFO_MEM;

    for (i = 0; rig->state.chan_list[i].type; i++)
    {
        for (j = rig->state.chan_list[i].startc;
                j <= rig->state.chan_list[i].endc; j++)
        {

            chan.channel_num = j;
            ret = rig_set_channel(rig, RIG_VFO_NONE, &chan);

            if (ret != RIG_OK)
            {
                return ret;
            }
        }
    }

    return 0;
}
