/*
 *  Hamlib Interface - provides registering for dynamically loadable backends.
 *  Copyright (c) 2000-2004 by Stephane Fillod
 *
 *
 *   This library is free software; you can redistribute it and/or
 *   modify it under the terms of the GNU Lesser General Public
 *   License as published by the Free Software Foundation; either
 *   version 2.1 of the License, or (at your option) any later version.
 *
 *   This library is distributed in the hope that it will be useful,
 *   but WITHOUT ANY WARRANTY; without even the implied warranty of
 *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 *   Lesser General Public License for more details.
 *
 *   You should have received a copy of the GNU Lesser General Public
 *   License along with this library; if not, write to the Free Software
 *   Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA
 *
 */

/**
 * \brief Dynamic registration of amplifier backends
 * \file amp_reg.c
 *
 * Similar to register.c
 * doc todo: Let's explain what's going on here!
 */

#include <hamlib/config.h>

#include <errno.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <sys/types.h>

#include <hamlib/amplifier.h>

#include "register.h"

//! @cond Doxygen_Suppress
#ifndef PATH_MAX
# define PATH_MAX       1024
#endif

#define AMP_BACKEND_MAX 32

#define DEFINE_INITAMP_BACKEND(backend)                                 \
    int MAKE_VERSIONED_FN(PREFIX_INITAMPS,                              \
                          ABI_VERSION,                                  \
                          backend(void *be_handle));                    \
    rig_model_t MAKE_VERSIONED_FN(PREFIX_PROBEAMPS,                     \
                                  ABI_VERSION,                          \
                                  backend(hamlib_port_t *port,          \
                                          rig_probe_func_t cfunc,       \
                                          rig_ptr_t data))

#define AMP_FUNCNAMA(backend) MAKE_VERSIONED_FN(PREFIX_INITAMPS, ABI_VERSION, backend)
#define AMP_FUNCNAMB(backend) MAKE_VERSIONED_FN(PREFIX_PROBEAMPS, ABI_VERSION, backend)

#define AMP_FUNCNAM(backend) AMP_FUNCNAMA(backend),AMP_FUNCNAMB(backend)


DEFINE_INITAMP_BACKEND(dummy);
DEFINE_INITAMP_BACKEND(kpa1500);
DEFINE_INITAMP_BACKEND(gemini);
//! @endcond

/**
 *  \def amp_backend_list
 *  \brief Static list of amplifier models.
 *
 *  This is a NULL terminated list of available amplifier backends. Each entry
 *  in the list consists of two fields: The branch number, which is an integer,
 *  and the branch name, which is a character string.
 *  An external library, loaded dynamically, could add its own functions
 *  pointers in this array.
 */
static struct
{
    int be_num;
    const char *be_name;
    int (*be_init)(void *);
    amp_model_t (*be_probe)(hamlib_port_t *);
} amp_backend_list[AMP_BACKEND_MAX] =
{
    { AMP_DUMMY, AMP_BACKEND_DUMMY, AMP_FUNCNAMA(dummy) },
    { AMP_ELECRAFT, AMP_BACKEND_ELECRAFT, AMP_FUNCNAMA(kpa1500) },
    { AMP_GEMINI, AMP_BACKEND_GEMINI, AMP_FUNCNAMA(gemini) },
    { 0, NULL }, /* end */
};


// Apparently, no amplifier can be probed.

/*
 * AMP_BACKEND_LIST is here, please keep it up to date,
 *  i.e. each time you implement a new backend.
 */


/*
 * This struct to keep track of known amp models.
 * It is chained, and used in a hash table, see below.
 */
//! @cond Doxygen_Suppress
struct amp_list
{
    const struct amp_caps *caps;
    struct amp_list *next;
};


#define AMPLSTHASHSZ 16
#define HASH_FUNC(a) ((a)%AMPLSTHASHSZ)
//! @endcond


/*
 * The amp_hash_table is a hash table pointing to a list of next==NULL
 *  terminated caps.
 */
static struct amp_list *amp_hash_table[AMPLSTHASHSZ] = { NULL, };


static int amp_lookup_backend(amp_model_t amp_model);


/*
 * Basically, this is a hash insert function that doesn't check for dup!
 */
//! @cond Doxygen_Suppress
int HAMLIB_API amp_register(const struct amp_caps *caps)
{
    int hval;
    struct amp_list *p;

    if (!caps)
    {
        return -RIG_EINVAL;
    }

    amp_debug(RIG_DEBUG_VERBOSE, "amp_register (%d)\n", caps->amp_model);

#ifndef DONT_WANT_DUP_CHECK

    if (amp_get_caps(caps->amp_model) != NULL)
    {
        return -RIG_EINVAL;
    }

#endif

    p = (struct amp_list *)calloc(1, sizeof(struct amp_list));

    if (!p)
    {
        return -RIG_ENOMEM;
    }

    hval = HASH_FUNC(caps->amp_model);
    p->caps = caps;
    // p->handle = NULL;
    p->next = amp_hash_table[hval];
    amp_hash_table[hval] = p;

    return RIG_OK;
}
//! @endcond


/*
 * Get amp capabilities.
 * i.e. amp_hash_table lookup
 */
//! @cond Doxygen_Suppress
const struct amp_caps *HAMLIB_API amp_get_caps(amp_model_t amp_model)
{
    struct amp_list *p;

    for (p = amp_hash_table[HASH_FUNC(amp_model)]; p; p = p->next)
    {
        if (p->caps->amp_model == amp_model)
        {
            return p->caps;
        }
    }

    return NULL;    /* sorry, caps not registered! */
}
//! @endcond


/*
 * lookup for backend index in amp_backend_list table,
 * according to BACKEND_NUM
 * return -1 if not found.
 */
//! @cond Doxygen_Suppress
static int amp_lookup_backend(amp_model_t amp_model)
{
    int i;

    for (i = 0; i < AMP_BACKEND_MAX && amp_backend_list[i].be_name; i++)
    {
        if (AMP_BACKEND_NUM(amp_model) ==
                amp_backend_list[i].be_num)
        {
            return i;
        }
    }

    return -1;
}
//! @endcond


/*
 * amp_check_backend
 * check the backend declaring this model has been loaded
 * and if not loaded already, load it!
 * This permits seamless operation in amp_init.
 */
//! @cond Doxygen_Suppress
int HAMLIB_API amp_check_backend(amp_model_t amp_model)
{
    const struct amp_caps *caps;
    int be_idx;
    int retval;

    /* already loaded ? */
    caps = amp_get_caps(amp_model);

    if (caps)
    {
        return RIG_OK;
    }

    be_idx = amp_lookup_backend(amp_model);

    /*
     * Never heard about this backend family!
     */
    if (be_idx == -1)
    {
        amp_debug(RIG_DEBUG_VERBOSE,
                  "%s: unsupported backend %d for model %d\n",
                  __func__,
                  AMP_BACKEND_NUM(amp_model),
                  amp_model);

        return -RIG_ENAVAIL;
    }

    retval = amp_load_backend(amp_backend_list[be_idx].be_name);

    return retval;
}
//! @endcond


//! @cond Doxygen_Suppress
int HAMLIB_API amp_unregister(amp_model_t amp_model)
{
    int hval;
    struct amp_list *p, *q;

    hval = HASH_FUNC(amp_model);
    q = NULL;

    for (p = amp_hash_table[hval]; p; p = p->next)
    {
        if (p->caps->amp_model == amp_model)
        {
            if (q == NULL)
            {
                amp_hash_table[hval] = p->next;
            }
            else
            {
                q->next = p->next;
            }

            free(p);
            return RIG_OK;
        }

        q = p;
    }

    return -RIG_EINVAL; /* sorry, caps not registered! */
}
//! @endcond


/*
 * amp_list_foreach
 * executes cfunc on all the elements stored in the amp hash list
 */
//! @cond Doxygen_Suppress
int HAMLIB_API amp_list_foreach(int (*cfunc)(const struct amp_caps *,
                                rig_ptr_t),
                                rig_ptr_t data)
{
    struct amp_list *p;
    int i;

    if (!cfunc)
    {
        return -RIG_EINVAL;
    }

    for (i = 0; i < AMPLSTHASHSZ; i++)
    {
        for (p = amp_hash_table[i]; p; p = p->next)
            if ((*cfunc)(p->caps, data) == 0)
            {
                return RIG_OK;
            }
    }

    return RIG_OK;
}
//! @endcond


/*
 * amp_probe_all
 * called straight by amp_probe
 */
//! @cond Doxygen_Suppress
amp_model_t HAMLIB_API amp_probe_all(hamlib_port_t *p)
{
    int i;
    amp_model_t amp_model;

    for (i = 0; i < AMP_BACKEND_MAX && amp_backend_list[i].be_name; i++)
    {
        if (amp_backend_list[i].be_probe)
        {
            amp_model = (*amp_backend_list[i].be_probe)(p);

            if (amp_model != AMP_MODEL_NONE)
            {
                return amp_model;
            }
        }
    }

    return AMP_MODEL_NONE;
}
//! @endcond


//! @cond Doxygen_Suppress
int amp_load_all_backends()
{
    int i;

    for (i = 0; i < AMP_BACKEND_MAX && amp_backend_list[i].be_name; i++)
    {
        amp_load_backend(amp_backend_list[i].be_name);
    }

    return RIG_OK;
}
//! @endcond


/*
 * amp_load_backend
 * Dynamically load a amp backend through dlopen mechanism
 */
//! @cond Doxygen_Suppress
int HAMLIB_API amp_load_backend(const char *be_name)
{
    int status;
    int (*be_init)(rig_ptr_t);
    int i;

    for (i = 0; i < AMP_BACKEND_MAX && amp_backend_list[i].be_name; i++)
    {
        if (!strcmp(be_name, amp_backend_list[i].be_name))
        {
            be_init = amp_backend_list[i].be_init;

            if (be_init == NULL)
            {
                printf("Null\n");
                return -EINVAL;
            }

            status = (*be_init)(NULL);
            return status;
        }
    }

    return -EINVAL;

}
//! @endcond
