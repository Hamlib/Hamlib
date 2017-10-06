/*
 * dumpcaps_rot.c - Copyright (C) 2000-2012 Stephane Fillod
 * This programs dumps the capabilities of a backend rig.
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

#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#include <hamlib/rig.h>
#include "misc.h"

#include "sprintflst.h"
#include "rotctl_parse.h"


/*
 * the rot may be in rot_init state, but not openned
 */
int dumpcaps_rot(ROT *rot, FILE *fout)
{
    const struct rot_caps *caps;
    int backend_warnings = 0;

    if (!rot || !rot->caps)
    {
        return -RIG_EINVAL;
    }

    caps = rot->caps;

    fprintf(fout, "Caps dump for model:\t%d\n", caps->rot_model);
    fprintf(fout, "Model name:\t\t%s\n", caps->model_name);
    fprintf(fout, "Mfg name:\t\t%s\n", caps->mfg_name);
    fprintf(fout, "Backend version:\t%s\n", caps->version);
    fprintf(fout, "Backend copyright:\t%s\n", caps->copyright);
    fprintf(fout, "Backend status:\t\t%s\n", rig_strstatus(caps->status));
    fprintf(fout, "Rot type:\t\t");

    switch (caps->rot_type & ROT_TYPE_MASK)
    {
    case ROT_TYPE_OTHER:
        fprintf(fout, "Other\n");
        break;

    case ROT_TYPE_AZIMUTH:
        fprintf(fout, "Azimuth\n");
        break;

    case ROT_TYPE_ELEVATION:
        fprintf(fout, "Elevation\n");
        break;

    case ROT_TYPE_AZEL:
        fprintf(fout, "Az-El\n");
        break;

    default:
        fprintf(fout, "Unknown\n");
        backend_warnings++;
    }

    fprintf(fout, "Port type:\t\t");

    switch (caps->port_type)
    {
    case RIG_PORT_SERIAL:
        fprintf(fout, "RS-232\n");
        fprintf(fout,
                "Serial speed:\t\t%d..%d bauds, %d%c%d%s\n",
                caps->serial_rate_min,
                caps->serial_rate_max,
                caps->serial_data_bits,
                caps->serial_parity == RIG_PARITY_NONE ? 'N' :
                caps->serial_parity == RIG_PARITY_ODD ? 'O' :
                caps->serial_parity == RIG_PARITY_EVEN ? 'E' :
                caps->serial_parity == RIG_PARITY_MARK ? 'M' : 'S',
                caps->serial_stop_bits,
                caps->serial_handshake == RIG_HANDSHAKE_NONE ? "" :
                (caps->serial_handshake == RIG_HANDSHAKE_XONXOFF ? " XONXOFF" : " CTS/RTS")
               );
        break;

    case RIG_PORT_PARALLEL:
        fprintf(fout, "Parallel\n");
        break;

    case RIG_PORT_DEVICE:
        fprintf(fout, "Device driver\n");
        break;

    case RIG_PORT_USB:
        fprintf(fout, "USB\n");
        break;

    case RIG_PORT_NETWORK:
        fprintf(fout, "Network link\n");
        break;

    case RIG_PORT_UDP_NETWORK:
        fprintf(fout, "UDP Network link\n");
        break;

    case RIG_PORT_NONE:
        fprintf(fout, "None\n");
        break;

    default:
        fprintf(fout, "Unknown\n");
        backend_warnings++;
    }

    fprintf(fout,
            "Write delay:\t\t%dmS, timeout %dmS, %d retr%s\n",
            caps->write_delay,
            caps->timeout, caps->retry,
            (caps->retry == 1) ? "y" : "ies");

    fprintf(fout,
            "Post Write delay:\t%dmS\n",
            caps->post_write_delay);

    fprintf(fout, "Min Azimuth:\t\t%.2f\n", caps->min_az);
    fprintf(fout, "Max Azimuth:\t\t%.2f\n", caps->max_az);

    fprintf(fout, "Min Elevation:\t\t%.2f\n", caps->min_el);
    fprintf(fout, "Max Elevation:\t\t%.2f\n", caps->max_el);

    fprintf(fout, "Has priv data:\t\t%c\n", caps->priv != NULL ? 'Y' : 'N');

    /*
     * Status is either 'Y'es, 'E'mulated, 'N'o
     *
     * TODO: keep me up-to-date with API call list!
     */
    fprintf(fout, "Has Init:\t\t%c\n", caps->rot_init != NULL ? 'Y' : 'N');
    fprintf(fout, "Has Cleanup:\t\t%c\n", caps->rot_cleanup != NULL ? 'Y' : 'N');
    fprintf(fout, "Has Open:\t\t%c\n", caps->rot_open != NULL ? 'Y' : 'N');
    fprintf(fout, "Has Close:\t\t%c\n", caps->rot_close != NULL ? 'Y' : 'N');
    fprintf(fout, "Can set Conf:\t\t%c\n", caps->set_conf != NULL ? 'Y' : 'N');
    fprintf(fout, "Can get Conf:\t\t%c\n", caps->get_conf != NULL ? 'Y' : 'N');

    fprintf(fout,
            "Can set Position:\t%c\n",
            caps->set_position != NULL ? 'Y' : 'N');

    fprintf(fout,
            "Can get Position:\t%c\n",
            caps->get_position != NULL ? 'Y' : 'N');

    fprintf(fout, "Can Stop:\t\t%c\n", caps->stop != NULL ? 'Y' : 'N');
    fprintf(fout, "Can Park:\t\t%c\n", caps->park != NULL ? 'Y' : 'N');
    fprintf(fout, "Can Reset:\t\t%c\n", caps->reset != NULL ? 'Y' : 'N');
    fprintf(fout, "Can Move:\t\t%c\n", caps->move != NULL ? 'Y' : 'N');
    fprintf(fout, "Can get Info:\t\t%c\n", caps->get_info != NULL ? 'Y' : 'N');

    fprintf(fout, "\nOverall backend warnings: %d\n", backend_warnings);

    return backend_warnings;
}
