/* hamlib - Control your rig!
   Copyright (C) 2000 Stephane Fillod
   This file is part of the hamlib package.

   Hamlib is free software; you can redistribute it and/or modify it
   under the terms of the GNU General Public License as published by
   the Free Software Foundation; either version 2 of the License, or
   (at your option) any later version.

   Hamlib is distributed in the hope that it will be useful, but WITHOUT
   ANY WARRANTY; without even the implied warranty of MERCHANTABILITY
   or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public
   License for more details.

   You should have received a copy of the GNU General Public License
   along with sane; see the file COPYING.  If not, write to the Free
   Software Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 */

#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <stdio.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>


#include <rig.h>


/* It would be nice to have an automatic way of referencing all the backends
 * supported by hamlib. Maybe this array should be placed in a separate file..
 */
extern const struct rig_caps ft847_caps;
extern const struct rig_caps ic706_caps;
extern const struct rig_caps ic706mkiig_caps;

static const struct rig_caps *rig_base[] = { 
	&ft847_caps, &ic706_caps, &ic706mkiig_caps, /* ... */ NULL, };


RIG *rig_open(const char *rig_path, rig_model_t rig_model)
{
		RIG *rig;
		int i;

		/* lookup for this rig */
		for (i=0; rig_base[i]; i++) {
				if (rig_base[i]->rig_model == rig_model)
						break;
		}
		if (rig_base[i] == NULL) {
				/* rig not supported, sorry! */
				return NULL;
		}

		/* okay, we've found it. Allocate some memory and set it to zeros, */
		/*							and especially the callbacks */
		rig = calloc(1, sizeof(RIG));
		if (rig == NULL) {
				return NULL;
		}

		/* remember, rig->caps is readonly */
		rig->caps = rig_base[i];

		/* populate the rig->state here */
		switch (port_type) {
		case SERIAL_PORT:
		rig->state.serial_rate = get_from_preference_or_default();
		rig->state.serial_data_bits = get_from_preference_or_default();
		rig->state.serial_stop_bits = get_from_preference_or_default();
		rig->state.serial_parity = get_from_preference_or_default();
		strncpy(rig->state.rig_path, rig_path, MAXRIGPATHLEN);
		/* serial_open would be cool */
		rig->state.fd = serial_open(rig_path, O_RDWR /* serial parms... */);
		/* chech return code.. */

		/* let the backend a chance to setup his private data */
		if (rig->caps->rig_init != NULL)
				rig->caps->rig_init(rig);	
		break;

		case TCP_PORT:
		/* get hostname, port name, create socket, connect, etc. */
		break;

		default: /* bail out, port not supported! */
		break;
		} 

#define PROBE_OK 0

		/* check we haven't been fooled */
		if (rig->caps->rig_probe != NULL)
				if (rig->caps->rig_probe(rig) != PROBE_OK) {
						/* not what we expected! release the rig */
					rig_close(rig);
					return NULL;
				}	
		break;
		return rig;
}


/* typicall cmd_* wrapper */
int cmd_set_freq_main_vfo_hz(RIG *rig, freq_t freq, rig_mode_t mode)
{
		if (rig == NULL || rig->caps)
			return -1; /* EINVAL */
		if (rig->caps->set_freq_main_vfo_hz == NULL)
			return -2; /* not implemented */
		else
			return rig->caps->set_freq_main_vfo_hz(&rig->state, freq, mode);
}


/*
 * close port and release memory
 */
int rig_close(RIG *rig)
{
		if (rig == NULL || rig->caps)
				return -1;

		if (rig->caps->rig_cleanup)
				rig->caps->rig_cleanup(rig);

		free(rig);

		return 0;
}

/* CAUTION: this is really Experimental, It never worked!!
 * try to guess a rig, can be very buggy! (but fun if it works!)
 * FIXME: finish me and handle nicely errors
 */
RIG *rig_probe(const char *port_path)
{
		RIG *rig;
		int i;

		for (i = 0; rig_base[i]; i++) {
			if (rig_base[i]->rig_probe != NULL) {
				rig = rig_open(port_path, rig_base[i]->rig_model);
				if (rig && rig_base[i]->rig_probe(rig) == PROBE_OK)
					return rig;
				else
						rig_close(rig);
			}
		}
		return NULL;
}


/*
 * Gets rig capabilities.
 * 
 */

const struct rig_caps *rig_get_caps(rig_model_t rig_model) {

	int i;
	const struct rig_caps *rc;

	/* lookup for this rig */
	for (i=0; rig_base[i]; i++) {
			if (rig_base[i]->rig_model == rig_model)
					break;
	}
	if (rig_base[i] == NULL) {
			/* rig not supported, sorry! */
			return NULL;
	}

	rc = rig_base[i];
	printf("rig = %s \n", rc->model_name);

	printf("rig serial_rate_min = = %u \n", rc->serial_rate_min);
	printf("rig serial_rate_max = = %u \n", rc->serial_rate_max);

	return  rc;
}

