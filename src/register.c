/* hamlib - Ham Radio Control Libraries
   register.c  - Copyright (C) 2000 Stephane Fillod and Frank Singleton
   Provides registering for dynamically loadable backends.

   $Id: register.c,v 1.4 2001-06-02 17:54:43 f4cfe Exp $

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

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <errno.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <stdio.h>
#include <sys/types.h>

/* This is libtool's dl wrapper */
#include <ltdl.h>

#include <hamlib/rig.h>
#include <hamlib/riglist.h>


#ifndef PATH_MAX
# define PATH_MAX       1024
#endif


struct rig_list {
		const struct rig_caps *caps;
		lt_dlhandle handle;			/* handle returned by lt_dlopen() */
		struct rig_list *next;
};

#define RIGLSTHASHSZ 16
#define HASH_FUNC(a) ((a)%RIGLSTHASHSZ)

/*
 * The rig_hash_table is a hash table pointing to a list of next==NULL
 * 	terminated caps.
 */
static struct rig_list *rig_hash_table[RIGLSTHASHSZ] = { NULL, };

/*
 * Basically, this is a hash insert function that doesn't check for dup!
 */
int rig_register(const struct rig_caps *caps)
{
		int hval;
		struct rig_list *p;

		if (!caps)
				return -RIG_EINVAL;

		rig_debug(RIG_DEBUG_VERBOSE, "rig_register (%d)\n",caps->rig_model);

#ifdef WANT_DUP_CHECK
		if (rig_get_caps(caps->rig_model)!=NULL)
				return -RIG_EINVAL;
#endif

		p = (struct rig_list*)malloc(sizeof(struct rig_list));
		if (!p)
				return -RIG_ENOMEM;

		hval = HASH_FUNC(caps->rig_model);
		p->caps = caps;
		p->handle = NULL;
		p->next = rig_hash_table[hval];
		rig_hash_table[hval] = p;

		return RIG_OK;
}

/*
 * Get rig capabilities.
 * ie. rig_hash_table lookup
 */

const struct rig_caps *rig_get_caps(rig_model_t rig_model)
{
		struct rig_list *p;

		for (p = rig_hash_table[HASH_FUNC(rig_model)]; p; p=p->next) {
				if (p->caps->rig_model == rig_model)
						return p->caps;
		}
		return NULL;	/* sorry, caps not registered! */
}


int rig_unregister(rig_model_t rig_model)
{
		int hval;
		struct rig_list *p,*q;

		hval = HASH_FUNC(rig_model);
		q = NULL;
		for (p = rig_hash_table[hval]; p; p=p->next) {
				if (p->caps->rig_model == rig_model) {
						if (q == NULL)
								rig_hash_table[hval] = p->next;
						else
								q->next = p->next;
						free(p);
						return RIG_OK;
				}
				q = p;
		}
		return -RIG_EINVAL;	/* sorry, caps not registered! */
}

/*
 * rig_list_foreach
 * executes cfunc on all the elements stored in the rig hash list
 */
int rig_list_foreach(int (*cfunc)(const struct rig_caps*, void *),void *data)
{
	struct rig_list *p;
	int i;

	if (!cfunc)
			return -RIG_EINVAL;

	for (i=0; i<RIGLSTHASHSZ; i++) {
				for (p=rig_hash_table[i]; p; p=p->next)
						if ((*cfunc)(p->caps,data) == 0)
								return RIG_OK;
	}
	return RIG_OK;
}

/*
 * rig_load_backend
 * Dynamically load a rig backend through dlopen mechanism
 */
int rig_load_backend(const char *be_name)
{
# define PREFIX "libhamlib-"
# define POSTFIX ".so" /* ".so.%u" */
		lt_dlhandle be_handle;
	    int (*be_init)(void *);
		int status;
		char libname[PATH_MAX];
		char initfuncname[64]="init_";

	/* lt_dlinit may be called several times */
	status = lt_dlinit();
	if (status) {
    		rig_debug(RIG_DEBUG_ERR, "rig_backend_load: lt_dlinit for %s failed: %d\n",
					      be_name, status);
    		return -RIG_EINTERNAL;
	}

		rig_debug(RIG_DEBUG_VERBOSE, "rig: loading backend %s\n",be_name);

		/*
		 * add hamlib directory here
		 */
		snprintf (libname, sizeof (libname), PREFIX"%s"POSTFIX, be_name);

		be_handle = lt_dlopen (libname);

		if (!be_handle) {
			rig_debug(RIG_DEBUG_ERR, "rig: lt_dlopen(\"%s\") failed (%s)\n",
							libname, lt_dlerror());
			return -RIG_EINVAL;
	    }


	    strcat(initfuncname, be_name);
	    be_init = (int (*)(void *)) lt_dlsym (be_handle, initfuncname);
		if (!be_init) {
				rig_debug(RIG_DEBUG_ERR, "rig: dlsym(%s) failed (%s)\n",
							initfuncname, lt_dlerror());
				lt_dlclose(be_handle);
				return -RIG_EINVAL;
		}

		status = (*be_init)(be_handle);

	 	return status;
}

