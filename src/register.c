/* hamlib - Ham Radio Control Libraries
   register.c  - Copyright (C) 2000 Stephane Fillod and Frank Singleton
   Provides registering for dynamically loadable backends.

   $Id: register.c,v 1.5 2001-06-04 21:17:52 f4cfe Exp $

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

#define RIG_BACKEND_MAX 32

/*
 * RIG_BACKEND_LIST is defined in riglist.h, please keep it up to data,
 * 	ie. each time you give birth to a new backend
 * Also, it should be possible to register "external" backend,
 * that is backend that were not known by Hamlib at compile time.
 * Maybe, riglist.h should reserve some numbers for them? --SF
 */
static struct {
	int be_num;
	const char *be_name;
    rig_model_t (*be_probe)(port_t *);
} rig_backend_list[RIG_BACKEND_MAX] = RIG_BACKEND_LIST;


/*
 * This struct to keep track of known rig models.
 * It is chained, and used in a hash table, see below.
 */
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


static int rig_lookup_backend(rig_model_t rig_model);

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

#ifndef DONT_WANT_DUP_CHECK
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

/*
 * lookup for backend index in rig_backend_list table,
 * according to BACKEND_NUM
 * return -1 if not found.
 */
static int rig_lookup_backend(rig_model_t rig_model)
{
		int i;

		for (i=0; i<RIG_BACKEND_MAX && rig_backend_list[i].be_name; i++) {
				if (RIG_BACKEND_NUM(rig_model) == 
								rig_backend_list[i].be_num)
						return i;
		}
		return -1;
}

/*
 * rig_check_backend
 * check the backend declaring this model has been loaded
 * and if not loaded already, load it!
 * This permits seamless operation in rig_init.
 */
int rig_check_backend(rig_model_t rig_model)
{
		const struct rig_caps *caps;
		int be_idx;
		int retval;
		
		/* already loaded ? */
		caps = rig_get_caps(rig_model);
		if (caps)
				return RIG_OK;

		be_idx = rig_lookup_backend(rig_model);

		/*
		 * Never heard about this backend family!
		 */
		if (be_idx == -1) {
			rig_debug(RIG_DEBUG_VERBOSE, "rig_check_backend: unsupported "
							"backend %d for model %d\n", 
							RIG_BACKEND_NUM(rig_model), rig_model
							);
			return -RIG_ENAVAIL;
		}
				
		retval = rig_load_backend(rig_backend_list[be_idx].be_name);

		return retval;
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
 * rig_probe_all
 * called straight by rig_probe
 */
rig_model_t rig_probe_all(port_t *p)
{
	int i;
	rig_model_t rig_model;

	for (i=0; i<RIG_BACKEND_MAX && rig_backend_list[i].be_name; i++) {
			if (rig_backend_list[i].be_probe) {
					rig_model = (*rig_backend_list[i].be_probe)(p);
					if (rig_model != RIG_MODEL_NONE)
							return rig_model;
			}
	}
	return RIG_MODEL_NONE;
}


int rig_load_all_backends()
{
	int i;

	for (i=0; i<RIG_BACKEND_MAX && rig_backend_list[i].be_name; i++) {
			rig_load_backend(rig_backend_list[i].be_name);
	}
	return RIG_OK;
}


#define MAXFUNCNAMELEN 64
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
	char initfname[MAXFUNCNAMELEN]  = "init_";
	char probefname[MAXFUNCNAMELEN] = "probe_";
	int i;

	/*
	 * lt_dlinit may be called several times
	 */
	status = lt_dlinit();
	if (status) {
    		rig_debug(RIG_DEBUG_ERR, "rig_backend_load: lt_dlinit for %s "
							"failed: %d\n", be_name, status);
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


    strncat(initfname, be_name, MAXFUNCNAMELEN);
    be_init = (int (*)(void *)) lt_dlsym (be_handle, initfname);
	if (!be_init) {
			rig_debug(RIG_DEBUG_ERR, "rig: dlsym(%s) failed (%s)\n",
						initfname, lt_dlerror());
			lt_dlclose(be_handle);
			return -RIG_EINVAL;
	}


	/*
	 * register probe function if present
	 * NOTE: rig_load_backend might have been called upon a backend
	 * 	not in riglist.h! In this case, do nothing.
	 */
	for (i=0; i<RIG_BACKEND_MAX && rig_backend_list[i].be_name; i++) {
		if (!strncmp(be_name, rig_backend_list[i].be_name, 64)) {
    			strncat(probefname, be_name, MAXFUNCNAMELEN);
    			rig_backend_list[i].be_probe = (rig_model_t (*)(port_t *))
						lt_dlsym (be_handle, probefname);
				break;
		}
	}

	status = (*be_init)(be_handle);

 	return status;
}

