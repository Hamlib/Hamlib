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

    printf("%d\t%-10s\t%-14s\t%-8s\t",
           caps->rig_model,
           caps->mfg_name,
           caps->model_name,
           caps->version);

    printf("%.10s\t", rig_strstatus(caps->status));

    switch (caps->rig_type & RIG_TYPE_MASK)
    {
    case RIG_TYPE_TRANSCEIVER:
        printf("%.16s\t","Transceiver");
        break;

    case RIG_TYPE_HANDHELD:
        printf("%.16s\t","Handheld");
        break;

    case RIG_TYPE_MOBILE:
        printf("%.16s\t","Mobile");
        break;

    case RIG_TYPE_RECEIVER:
        printf("%.16s\t","Receiver");
        break;

    case RIG_TYPE_PCRECEIVER:
        printf("%.16s\t","PC Receiver");
        break;

    case RIG_TYPE_SCANNER:
        printf("%.16s\t","Scanner");
        break;

    case RIG_TYPE_TRUNKSCANNER:
        printf("%.16s\t","Trunking scanner");
        break;

    case RIG_TYPE_COMPUTER:
        printf("%.16s\t","Computer");
        break;

    case RIG_TYPE_OTHER:
        printf("%.16s\t","Other");
        break;

    default:
        printf("%.16s\t","Unknown");
    }
    printf("%s\n",caps->rig_model_macro_name==NULL?"Unknown":caps->rig_model_macro_name);
    return -1;  /* !=0, we want them all ! */
}


int main(int argc, char *argv[])
{
    int status;

    rig_load_all_backends();

    printf("Rig#\tMfg       \tModel       \tVers.   \tStatus    \tType            \tMacro\n");

    status = rig_list_foreach(print_caps_sum, NULL);

    if (status != RIG_OK)
    {
        printf("rig_list_foreach: error = %s \n", rigerror(status));
        exit(3);
    }

    return 0;
}
