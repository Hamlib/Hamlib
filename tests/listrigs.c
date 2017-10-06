/*
 * listrigs.c - Copyright (C) 2000-2008 Stephane Fillod
 * This programs list all the available the rig capabilities.
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


#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#include <hamlib/rig.h>


int print_caps_sum(const struct rig_caps *caps, void *data)
{

    printf("%d\t%-10s\t%-12s\t%s\t",
           caps->rig_model,
           caps->mfg_name,
           caps->model_name,
           caps->version);

    printf("%-10s\t", rig_strstatus(caps->status));

    switch (caps->rig_type & RIG_TYPE_MASK)
    {
    case RIG_TYPE_TRANSCEIVER:
        printf("Transceiver\n");
        break;

    case RIG_TYPE_HANDHELD:
        printf("Handheld\n");
        break;

    case RIG_TYPE_MOBILE:
        printf("Mobile\n");
        break;

    case RIG_TYPE_RECEIVER:
        printf("Receiver\n");
        break;

    case RIG_TYPE_PCRECEIVER:
        printf("PC Receiver\n");
        break;

    case RIG_TYPE_SCANNER:
        printf("Scanner\n");
        break;

    case RIG_TYPE_TRUNKSCANNER:
        printf("Trunking scanner\n");
        break;

    case RIG_TYPE_COMPUTER:
        printf("Computer\n");
        break;

    case RIG_TYPE_OTHER:
        printf("Other\n");
        break;

    default:
        printf("Unknown\n");
    }

    return -1;  /* !=0, we want them all ! */
}


int main(int argc, char *argv[])
{
    int status;

    rig_load_all_backends();

    printf("Rig#\tMfg       \tModel       \tVers.\tStatus    \tType\n");

    status = rig_list_foreach(print_caps_sum, NULL);

    if (status != RIG_OK)
    {
        printf("rig_list_foreach: error = %s \n", rigerror(status));
        exit(3);
    }

    return 0;
}
