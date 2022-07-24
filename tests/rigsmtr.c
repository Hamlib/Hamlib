/*
 * rigsmtr.c - (C) Stephane Fillod 2007
 *
 * This program output S-meter vs. Azimuth curve using Hamlib.
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
#include <math.h>

#include <getopt.h>

#include <hamlib/rig.h>
#include <hamlib/rotator.h>
#include "misc.h"


/*
 * Prototypes
 */
static void usage();
static void version();
static int set_conf_rig(RIG *rig, char *conf_parms);
static int set_conf_rot(ROT *rot, char *conf_parms);

/*
 * Reminder: when adding long options,
 *   keep up to date SHORT_OPTIONS, usage()'s output and man page. thanks.
 * NB: do NOT use -W since it's reserved by POSIX.
 */
#define SHORT_OPTIONS "m:r:s:c:C:M:R:S:N:vhV"
static struct option long_options[] =
{
    {"model",               1, 0, 'm'},
    {"rig-file",            1, 0, 'r'},
    {"serial-speed",        1, 0, 's'},
    {"civaddr",             1, 0, 'c'},
    {"set-conf",            1, 0, 'C'},
    {"rot-model",           1, 0, 'M'},
    {"rot-file",            1, 0, 'R'},
    {"rot-serial-speed",    1, 0, 'S'},
    {"rot-set-conf",        1, 0, 'N'},
    {"verbose",             0, 0, 'v'},
    {"help",                0, 0, 'h'},
    {"version",             0, 0, 'V'},
    {0, 0, 0, 0}
};

#define MAXCONFLEN 1024


int main(int argc, char *argv[])
{
    RIG *rig;       /* handle to rig (instance) */
    ROT *rot;       /* handle to rotator (instance) */
    rig_model_t rig_model = RIG_MODEL_DUMMY;
    rot_model_t rot_model = ROT_MODEL_DUMMY;

    int retcode;        /* generic return code from functions */

    int verbose = 0;
    const char *rig_file = NULL, *rot_file = NULL;
    int serial_rate = 0;
    int rot_serial_rate = 0;
    char *civaddr = NULL;   /* NULL means no need to set conf */
    char rig_conf_parms[MAXCONFLEN] = "";
    char rot_conf_parms[MAXCONFLEN] = "";

    /* int with_rot = 1; */
    azimuth_t azimuth;
    elevation_t elevation;
    unsigned step = 1000000;    /* 1e6 us */

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

            rig_model = atoi(optarg);
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

            if (*rig_conf_parms != '\0')
            {
                strcat(rig_conf_parms, ",");
            }

            if (strlen(rig_conf_parms) + strlen(optarg) > MAXCONFLEN - 24)
            {
                printf("Length of conf_parms exceeds internal maximum of %d\n",
                       MAXCONFLEN - 24);
                return 1;
            }

            strncat(rig_conf_parms, optarg, MAXCONFLEN - strlen(rig_conf_parms));
            break;

        case 'M':
            if (!optarg)
            {
                usage();    /* wrong arg count */
                exit(1);
            }

            rot_model = atoi(optarg);
            break;

        case 'R':
            if (!optarg)
            {
                usage();    /* wrong arg count */
                exit(1);
            }

            rot_file = optarg;
            break;

        case 'S':
            if (!optarg)
            {
                usage();    /* wrong arg count */
                exit(1);
            }

            rot_serial_rate = atoi(optarg);
            break;

        case 'N':
            if (!optarg)
            {
                usage();    /* wrong arg count */
                exit(1);
            }

            if (*rot_conf_parms != '\0')
            {
                strcat(rot_conf_parms, ",");
            }

            strncat(rot_conf_parms, optarg, MAXCONFLEN - strlen(rot_conf_parms));
            break;

        case 'v':
            verbose++;
            break;

        default:
            usage();    /* unknown option? */
            exit(1);
        }
    }

    rig_set_debug(verbose < 2 ? RIG_DEBUG_WARN : verbose);

    rig_debug(RIG_DEBUG_VERBOSE, "rigsmtr, %s\n", hamlib_version2);
    rig_debug(RIG_DEBUG_VERBOSE, "%s",
              "Report bugs to <hamlib-developer@lists.sourceforge.net>\n\n");

    /*
     * The radio
     */
    rig = rig_init(rig_model);

    if (!rig)
    {
        fprintf(stderr,
                "Unknown rig num %u, or initialization error.\n",
                rig_model);

        fprintf(stderr, "Please check with --list option.\n");
        exit(2);
    }

    retcode = set_conf_rig(rig, rig_conf_parms);

    if (retcode != RIG_OK)
    {
        fprintf(stderr, "Config parameter error: %s\n", rigerror(retcode));
        exit(2);
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


    if (!rig_has_get_level(rig, RIG_LEVEL_STRENGTH))
    {
        fprintf(stderr,
                "rig backend for %s could not get S-Meter"
                "or has insufficient capability\nSorry\n",
                rig->caps->model_name);
        exit(3);
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

    /*
     * The rotator
     */
    rot = rot_init(rot_model);

    if (!rot)
    {
        fprintf(stderr,
                "Unknown rot num %d, or initialization error.\n",
                rot_model);

        fprintf(stderr, "Please check with --list option.\n");
        exit(2);
    }

    retcode = set_conf_rot(rot, rot_conf_parms);

    if (retcode != RIG_OK)
    {
        fprintf(stderr, "Config parameter error: %s\n", rigerror(retcode));
        exit(2);
    }

    if (rot_file)
    {
        strncpy(rot->state.rotport.pathname, rot_file, HAMLIB_FILPATHLEN - 1);
    }

    /* FIXME: bound checking and port type == serial */
    if (rot_serial_rate != 0)
    {
        rot->state.rotport.parm.serial.rate = rot_serial_rate;
    }

    retcode = rot_open(rot);

    if (retcode != RIG_OK && rot_model != ROT_MODEL_DUMMY)
    {
        fprintf(stderr, "rot_open: error = %s \n", rigerror(retcode));
        exit(2);
    }

    /* Commenting out to quell "set but not used" warning.
     * Enable when needed for further functionality.  -N0NB
     */
    /* if (rot_model == ROT_MODEL_DUMMY) */
    /*  with_rot = 1; */

    if (verbose > 0)
    {
        printf("Opened rotator model %d, '%s'\n",
               rot->caps->rot_model,
               rot->caps->model_name);
    }

    /*******************************/
    if (optind < argc)
    {
        step = atof(argv[optind]) * 1e6;
    }

    fprintf(stderr, "Setting rotator to azimuth %.1f°\n", rot->state.min_az);
    rot_set_position(rot, rot->state.min_az, 0);
    fprintf(stderr, "Wait for rotator to move...\n");
    rot_get_position(rot, &azimuth, &elevation);

    while (fabs(azimuth - rot->state.min_az) > 1.)
    {
        rot_get_position(rot, &azimuth, &elevation);
        hl_usleep(step);
    }

    fprintf(stderr, "Now initiating full 360° rotation...\n");
    rot_set_position(rot, rot->state.max_az, 0);

    /* TODO: check CW or CCW */
    /* disable AGC? */

    while (fabs(rot->state.max_az - azimuth) > 1.)
    {
        value_t strength;

        rig_get_level(rig, RIG_VFO_CURR, RIG_LEVEL_STRENGTH, &strength);

        rot_get_position(rot, &azimuth, &elevation);

        printf("%.1f %d\n", azimuth, strength.i);
    }

    rig_close(rig);
    rot_close(rot);

    return 0;
}



void version()
{
    printf("rigsmtr, %s\n\n", hamlib_version2);
    printf("%s\n", hamlib_copyright);
}


void usage()
{
    printf("Usage: rigsmtr [OPTION]... [time]\n"
           "Input S-Meter vs Azimuth.\n\n");

    printf(
        "  -m, --model=ID                select radio model number. See model list\n"
        "  -r, --rig-file=DEVICE         set device of the radio to operate on\n"
        "  -s, --serial-speed=BAUD       set serial speed of the serial port\n"
        "  -c, --civaddr=ID              set CI-V address, decimal (for Icom rigs only)\n"
        "  -C, --set-conf=PARM=VAL       set config parameters\n"
        "  -M, --rot-model=ID            select rotator model number. See model list\n"
        "  -R, --rot-file=DEVICE         set device of the rotator to operate on\n"
        "  -S, --rot-serial-speed=BAUD   set serial speed of the serial port\n"
        "  -N, --rot-set-conf=PARM=VAL   set rotator config parameters\n"
        "  -v, --verbose                 set verbose mode, cumulative\n"
        "  -h, --help                    display this help and exit\n"
        "  -V, --version                 output version information and exit\n\n"
    );

    printf("\nReport bugs to <hamlib-developer@lists.sourceforge.net>.\n");

}


int set_conf_rig(RIG *rig, char *conf_parms)
{
    char *p;

    p = conf_parms;

    while (p && *p != '\0')
    {
        int ret;
        char *q, *n = NULL;
        /* FIXME: left hand value of = cannot be null */
        q = strchr(p, '=');

        if (!q)
        {
            return RIG_EINVAL;

            *q++ = '\0';
            n = strchr(q, ',');

            if (n)
            {
                *n++ = '\0';
            }
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


int set_conf_rot(ROT *rot, char *conf_parms)
{
    char *p;

    p = conf_parms;

    while (p && *p != '\0')
    {
        char *q, *n = NULL;
        int ret;
        /* FIXME: left hand value of = cannot be null */
        q = strchr(p, '=');

        if (q)
        {
            *q++ = '\0';

            n = strchr(q, ',');

            if (n)
            {
                *n++ = '\0';
            }
        }

        ret = rot_set_conf(rot, rot_token_lookup(rot, p), q);

        if (ret != RIG_OK)
        {
            return ret;
        }

        p = n;
    }

    return RIG_OK;
}
