/* hamlib - Ham Radio Control Libraries
   register.c  - Copyright (C) 2000 Stephane Fillod and Frank Singleton
   Provides registering for dynamically loadable backends.

   $Id: register.c,v 1.2 2001-02-11 23:15:38 f4cfe Exp $

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


#if defined(HAVE_DLOPEN) && defined(HAVE_DLFCN_H)

#include <dlfcn.h>

  /* Older versions of dlopen() don't define RTLD_NOW and RTLD_LAZY.
   *      They all seem to use a mode of 1 to indicate RTLD_NOW and some do
   *           not support RTLD_LAZY at all.  Hence, unless defined, we define
   *                both macros as 1 to play it safe.  */
# ifndef RTLD_NOW
#  define RTLD_NOW      1
# endif
# ifndef RTLD_LAZY
#  define RTLD_LAZY     1
# endif
#endif


#include <hamlib/rig.h>
#include <hamlib/riglist.h>


#ifndef PATH_MAX
# define PATH_MAX       1024
#endif


struct rig_list {
		const struct rig_caps *caps;
		void *handle;				/* handle returned by dlopen() */
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
#ifdef HAVE_DLOPEN

# define PREFIX "libhamlib-"
# define POSTFIX ".so" /* ".so.%u" */
		void *be_handle;
	    int (*be_init)(void *);
		int status;
		int mode = getenv ("LD_BIND_NOW") ? RTLD_NOW : RTLD_LAZY;
		char libname[PATH_MAX];
		char initfuncname[64]="init_";

		rig_debug(RIG_DEBUG_VERBOSE, "rig: loading backend %s\n",be_name);

		/*
		 * add hamlib directory here
		 */
		snprintf (libname, sizeof (libname), PREFIX"%s"POSTFIX,
							                  be_name /* , V_MAJOR */);

		be_handle = dlopen (libname, mode);
		if (!be_handle) {
			rig_debug(RIG_DEBUG_ERR, "rig: dlopen() failed (%s)\n",
							dlerror());
			return -RIG_EINVAL;
	    }


		strcat(initfuncname, be_name);
	    be_init = (int (*)(void *)) dlsym (be_handle, initfuncname);
		if (!be_init) {
				rig_debug(RIG_DEBUG_ERR, "rig: dlsym(%s) failed (%s)\n",
							initfuncname, strerror (errno));
				dlclose(be_handle);
				return -RIG_EINVAL;
		}

		status = (*be_init)(be_handle);

	 	return status;

# undef PREFIX
# undef POSTFIX
#else /* HAVE_DLOPEN */
    rig_debug(RIG_DEBUG_ERR, "rig_backend_load: ignoring attempt to load `%s'; no dl support\n",
					      be_name);
	  return -RIG_ENOIMPL;
#endif /* HAVE_DLOPEN */
}

