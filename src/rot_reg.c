/*
 *  Hamlib Interface - provides registering for dynamically loadable backends.
 *  Copyright (c) 2000,2001 by Stephane Fillod and Frank Singleton
 *
 *		$Id: rot_reg.c,v 1.3 2002-01-02 23:41:05 fillods Exp $
 *
 *   This library is free software; you can redistribute it and/or modify
 *   it under the terms of the GNU Library General Public License as
 *   published by the Free Software Foundation; either version 2 of
 *   the License, or (at your option) any later version.
 *
 *   This program is distributed in the hope that it will be useful,
 *   but WITHOUT ANY WARRANTY; without even the implied warranty of
 *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *   GNU Library General Public License for more details.
 *
 *   You should have received a copy of the GNU Library General Public
 *   License along with this library; if not, write to the Free Software
 *   Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 *
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

#include <hamlib/rotator.h>


#ifndef PATH_MAX
# define PATH_MAX       1024
#endif

#define ROT_BACKEND_MAX 32

/*
 * ROT_BACKEND_LIST is defined in rotlist.h, please keep it up to data,
 * 	ie. each time you give birth to a new backend
 * Also, it should be possible to register "external" backend,
 * that is backend that were not known by Hamlib at compile time.
 * Maybe, rotlist.h should reserve some numbers for them? --SF
 */
static struct {
	int be_num;
	const char *be_name;
    rot_model_t (*be_probe)(port_t *);
} rot_backend_list[ROT_BACKEND_MAX] = ROT_BACKEND_LIST;


/*
 * This struct to keep track of known rot models.
 * It is chained, and used in a hash table, see below.
 */
struct rot_list {
		const struct rot_caps *caps;
		lt_dlhandle handle;			/* handle returned by lt_dlopen() */
		struct rot_list *next;
};

#define ROTLSTHASHSZ 16
#define HASH_FUNC(a) ((a)%ROTLSTHASHSZ)

/*
 * The rot_hash_table is a hash table pointing to a list of next==NULL
 * 	terminated caps.
 */
static struct rot_list *rot_hash_table[ROTLSTHASHSZ] = { NULL, };


static int rot_lookup_backend(rot_model_t rot_model);

/*
 * Basically, this is a hash insert function that doesn't check for dup!
 */
int rot_register(const struct rot_caps *caps)
{
		int hval;
		struct rot_list *p;

		if (!caps)
				return -RIG_EINVAL;

		rot_debug(RIG_DEBUG_VERBOSE, "rot_register (%d)\n",caps->rot_model);

#ifndef DONT_WANT_DUP_CHECK
		if (rot_get_caps(caps->rot_model)!=NULL)
				return -RIG_EINVAL;
#endif

		p = (struct rot_list*)malloc(sizeof(struct rot_list));
		if (!p)
				return -RIG_ENOMEM;

		hval = HASH_FUNC(caps->rot_model);
		p->caps = caps;
		p->handle = NULL;
		p->next = rot_hash_table[hval];
		rot_hash_table[hval] = p;

		return RIG_OK;
}

/*
 * Get rot capabilities.
 * ie. rot_hash_table lookup
 */

const struct rot_caps *rot_get_caps(rot_model_t rot_model)
{
		struct rot_list *p;

		for (p = rot_hash_table[HASH_FUNC(rot_model)]; p; p=p->next) {
				if (p->caps->rot_model == rot_model)
						return p->caps;
		}
		return NULL;	/* sorry, caps not registered! */
}

/*
 * lookup for backend index in rot_backend_list table,
 * according to BACKEND_NUM
 * return -1 if not found.
 */
static int rot_lookup_backend(rot_model_t rot_model)
{
		int i;

		for (i=0; i<ROT_BACKEND_MAX && rot_backend_list[i].be_name; i++) {
				if (ROT_BACKEND_NUM(rot_model) == 
								rot_backend_list[i].be_num)
						return i;
		}
		return -1;
}

/*
 * rot_check_backend
 * check the backend declaring this model has been loaded
 * and if not loaded already, load it!
 * This permits seamless operation in rot_init.
 */
int rot_check_backend(rot_model_t rot_model)
{
		const struct rot_caps *caps;
		int be_idx;
		int retval;
		
		/* already loaded ? */
		caps = rot_get_caps(rot_model);
		if (caps)
				return RIG_OK;

		be_idx = rot_lookup_backend(rot_model);

		/*
		 * Never heard about this backend family!
		 */
		if (be_idx == -1) {
			rot_debug(RIG_DEBUG_VERBOSE, "rot_check_backend: unsupported "
							"backend %d for model %d\n", 
							ROT_BACKEND_NUM(rot_model), rot_model
							);
			return -RIG_ENAVAIL;
		}
				
		retval = rot_load_backend(rot_backend_list[be_idx].be_name);

		return retval;
}



int rot_unregister(rot_model_t rot_model)
{
		int hval;
		struct rot_list *p,*q;

		hval = HASH_FUNC(rot_model);
		q = NULL;
		for (p = rot_hash_table[hval]; p; p=p->next) {
				if (p->caps->rot_model == rot_model) {
						if (q == NULL)
								rot_hash_table[hval] = p->next;
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
 * rot_list_foreach
 * executes cfunc on all the elements stored in the rot hash list
 */
int rot_list_foreach(int (*cfunc)(const struct rot_caps*, rig_ptr_t),rig_ptr_t data)
{
	struct rot_list *p;
	int i;

	if (!cfunc)
			return -RIG_EINVAL;

	for (i=0; i<ROTLSTHASHSZ; i++) {
				for (p=rot_hash_table[i]; p; p=p->next)
						if ((*cfunc)(p->caps,data) == 0)
								return RIG_OK;
	}
	return RIG_OK;
}

/*
 * rot_probe_all
 * called straight by rot_probe
 */
rot_model_t rot_probe_all(port_t *p)
{
	int i;
	rot_model_t rot_model;

	for (i=0; i<ROT_BACKEND_MAX && rot_backend_list[i].be_name; i++) {
			if (rot_backend_list[i].be_probe) {
					rot_model = (*rot_backend_list[i].be_probe)(p);
					if (rot_model != ROT_MODEL_NONE)
							return rot_model;
			}
	}
	return ROT_MODEL_NONE;
}


int rot_load_all_backends()
{
	int i;

	for (i=0; i<ROT_BACKEND_MAX && rot_backend_list[i].be_name; i++) {
			rot_load_backend(rot_backend_list[i].be_name);
	}
	return RIG_OK;
}


#define MAXFUNCNAMELEN 64
/*
 * rot_load_backend
 * Dynamically load a rot backend through dlopen mechanism
 */
int rot_load_backend(const char *be_name)
{
		/*
		 * determine PREFIX and POSTFIX values from configure script
		 */
#ifdef __CYGWIN__
# define PREFIX "cyghamlib-"
# define POSTFIX ".dll"
#else
# define PREFIX "libhamlib-"
# define POSTFIX ".la"
#endif

	lt_dlhandle be_handle;
    int (*be_init)(rig_ptr_t);
	int status;
	char libname[PATH_MAX];
	char initfname[MAXFUNCNAMELEN]  = "initrots_";
	char probefname[MAXFUNCNAMELEN] = "proberots_";
	int i;

	/*
	 * lt_dlinit may be called several times
	 */
#if 0
	LTDL_SET_PRELOADED_SYMBOLS();
#endif

	status = lt_dlinit();
	if (status) {
    		rot_debug(RIG_DEBUG_ERR, "rot_backend_load: lt_dlinit for %s "
							"failed: %s\n", be_name, lt_dlerror());
    		return -RIG_EINTERNAL;
	}

	rot_debug(RIG_DEBUG_VERBOSE, "rot: loading backend %s\n",be_name);

	/*
	 * add hamlib directory here
	 */
	snprintf (libname, sizeof (libname), PREFIX"%s", be_name);

	be_handle = lt_dlopenext (libname);

	/*
	 * external module not found? try dlopenself for backends 
	 * compiled in static
	 */
	if (!be_handle) {
		rig_debug(RIG_DEBUG_VERBOSE, "rig:  lt_dlopen(\"%s\") failed (%s),
										trying static symbols...\n",
										libname, lt_dlerror());
		be_handle = lt_dlopen (NULL);
	}

	if (!be_handle) {
		rot_debug(RIG_DEBUG_ERR, "rot:  lt_dlopen(\"%s\") failed (%s)\n",
						libname, lt_dlerror());
		return -RIG_EINVAL;
    }

    strncat(initfname, be_name, MAXFUNCNAMELEN);
    be_init = (int (*)(rig_ptr_t)) lt_dlsym (be_handle, initfname);
	if (!be_init) {
			rot_debug(RIG_DEBUG_ERR, "rot: dlsym(%s) failed (%s)\n",
						initfname, lt_dlerror());
			lt_dlclose(be_handle);
			return -RIG_EINVAL;
	}


	/*
	 * register probe function if present
	 * NOTE: rot_load_backend might have been called upon a backend
	 * 	not in rotlist.h! In this case, do nothing.
	 */
	for (i=0; i<ROT_BACKEND_MAX && rot_backend_list[i].be_name; i++) {
		if (!strncmp(be_name, rot_backend_list[i].be_name, 64)) {
    			strncat(probefname, be_name, MAXFUNCNAMELEN);
    			rot_backend_list[i].be_probe = (rot_model_t (*)(port_t *))
						lt_dlsym (be_handle, probefname);
				break;
		}
	}

	status = (*be_init)(be_handle);

 	return status;
}

