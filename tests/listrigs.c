/*
 * listrigs.c - Copyright (C) 2000 Stephane Fillod
 * This programs list all the available the rig capabilities.
 *
 *
 *    $Id: listrigs.c,v 1.1 2000-10-08 21:20:44 f4cfe Exp $  
 *
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 * 
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 * 
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.
 * 
 */


#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#include <hamlib/rig.h>


int print_caps_sum(const struct rig_caps *caps, void *data)
{

	printf("%d\t%s\t%-12s\t%s\t",caps->rig_model,caps->mfg_name,
					caps->model_name, caps->version);

	switch (caps->status) {
	case RIG_STATUS_ALPHA:
			printf("Alpha\t");
			break;
	case RIG_STATUS_UNTESTED:
			printf("Untested\t");
			break;
	case RIG_STATUS_BETA:
			printf("Beta\t");
			break;
	case RIG_STATUS_STABLE:
			printf("Stable\t");
			break;
	case RIG_STATUS_BUGGY:
			printf("Buggy\t");
			break;
	case RIG_STATUS_NEW:
			printf("New\t");
			break;
	default:
			printf("Unknown\t");
	}
	switch (caps->rig_type) {
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
	case RIG_TYPE_SCANNER:
			printf("Scanner\n");
			break;
	case RIG_TYPE_COMPUTER:
			printf("Computer\n");
			break;
	default:
			printf("Unknown\n");
	}
	return -1;	/* !=0, we want them all ! */
}


int main (int argc, char *argv[])
{ 
	int status;

	status = rig_load_backend("icom");
	if (status != RIG_OK ) {
		printf("rig_load_backend: error = %s \n", rigerror(status));
		exit(3);
	}
	status = rig_load_backend("ft747");
	if (status != RIG_OK ) {
		printf("rig_load_backend: ft747 error = %s \n", rigerror(status));
		exit(3);
	}
	status = rig_load_backend("ft847");
	if (status != RIG_OK ) {
		printf("rig_load_backend: ft847 error = %s \n", rigerror(status));
		exit(3);
	}

	printf("Rig#\tMfg\tModel       \tVers.\tStatus\tType\n");
	status = rig_list_foreach(print_caps_sum,NULL);
	if (status != RIG_OK ) {
		printf("rig_list_foreach: error = %s \n", rigerror(status));
		exit(3);
	}

	return 0;
}


