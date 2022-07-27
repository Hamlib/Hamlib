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
 * \brief Dynamic registration of rotator backends
 * \file rot_reg.c
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

#include <hamlib/rotator.h>

#include "register.h"

//! @cond Doxygen_Suppress
#ifndef PATH_MAX
# define PATH_MAX       1024
#endif

#define ROT_BACKEND_MAX 32

#define DEFINE_INITROT_BACKEND(backend)                                 \
    int MAKE_VERSIONED_FN(PREFIX_INITROTS,                              \
                          ABI_VERSION,                                  \
                          backend(void *be_handle));                    \
    rig_model_t MAKE_VERSIONED_FN(PREFIX_PROBEROTS,                     \
                                  ABI_VERSION,                          \
                                  backend(hamlib_port_t *port,          \
                                          rig_probe_func_t cfunc,       \
                                          rig_ptr_t data))

#define ROT_FUNCNAMA(backend) MAKE_VERSIONED_FN(PREFIX_INITROTS, ABI_VERSION, backend)
#define ROT_FUNCNAMB(backend) MAKE_VERSIONED_FN(PREFIX_PROBEROTS, ABI_VERSION, backend)

#define ROT_FUNCNAM(backend) ROT_FUNCNAMA(backend),ROT_FUNCNAMB(backend)


DEFINE_INITROT_BACKEND(dummy);
DEFINE_INITROT_BACKEND(easycomm);
DEFINE_INITROT_BACKEND(fodtrack);
DEFINE_INITROT_BACKEND(rotorez);
DEFINE_INITROT_BACKEND(sartek);
DEFINE_INITROT_BACKEND(gs232a);
DEFINE_INITROT_BACKEND(kit);
DEFINE_INITROT_BACKEND(heathkit);
DEFINE_INITROT_BACKEND(spid);
DEFINE_INITROT_BACKEND(m2);
DEFINE_INITROT_BACKEND(ars);
DEFINE_INITROT_BACKEND(amsat);
DEFINE_INITROT_BACKEND(ts7400);
DEFINE_INITROT_BACKEND(celestron);
DEFINE_INITROT_BACKEND(ether6);
DEFINE_INITROT_BACKEND(cnctrk);
DEFINE_INITROT_BACKEND(prosistel);
DEFINE_INITROT_BACKEND(meade);
DEFINE_INITROT_BACKEND(ioptron);
DEFINE_INITROT_BACKEND(satel);
DEFINE_INITROT_BACKEND(radant);
#if HAVE_LIBINDI
DEFINE_INITROT_BACKEND(indi);
#endif
#if defined(ANDROID) || defined(__ANDROID__)
DEFINE_INITROT_BACKEND(androidsensor);
#endif
DEFINE_INITROT_BACKEND(grbltrk);
//! @endcond

/**
 *  \def rot_backend_list
 *  \brief Static list of rotator models.
 *
 *  This is a NULL terminated list of available rotator backends. Each entry
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
    rot_model_t (*be_probe)(hamlib_port_t *);
} rot_backend_list[ROT_BACKEND_MAX] =
{
    { ROT_DUMMY, ROT_BACKEND_DUMMY, ROT_FUNCNAMA(dummy) },
    { ROT_EASYCOMM, ROT_BACKEND_EASYCOMM, ROT_FUNCNAMA(easycomm) },
    { ROT_FODTRACK, ROT_BACKEND_FODTRACK, ROT_FUNCNAMA(fodtrack) },
    { ROT_ROTOREZ, ROT_BACKEND_ROTOREZ, ROT_FUNCNAMA(rotorez) },
    { ROT_SARTEK, ROT_BACKEND_SARTEK, ROT_FUNCNAMA(sartek) },
    { ROT_GS232A, ROT_BACKEND_GS232A, ROT_FUNCNAMA(gs232a) },
    { ROT_KIT, ROT_BACKEND_KIT, ROT_FUNCNAMA(kit) },
    { ROT_HEATHKIT, ROT_BACKEND_HEATHKIT, ROT_FUNCNAMA(heathkit) },
    { ROT_SPID, ROT_BACKEND_SPID, ROT_FUNCNAMA(spid) },
    { ROT_M2, ROT_BACKEND_M2, ROT_FUNCNAMA(m2) },
    { ROT_ARS, ROT_BACKEND_ARS, ROT_FUNCNAMA(ars) },
    { ROT_AMSAT, ROT_BACKEND_AMSAT, ROT_FUNCNAMA(amsat) },
    { ROT_TS7400, ROT_BACKEND_TS7400, ROT_FUNCNAMA(ts7400) },
    { ROT_CELESTRON, ROT_BACKEND_CELESTRON, ROT_FUNCNAMA(celestron) },
    { ROT_ETHER6, ROT_BACKEND_ETHER6, ROT_FUNCNAMA(ether6) },
    { ROT_CNCTRK, ROT_BACKEND_CNCTRK, ROT_FUNCNAMA(cnctrk) },
    { ROT_PROSISTEL, ROT_BACKEND_PROSISTEL, ROT_FUNCNAMA(prosistel) },
    { ROT_MEADE, ROT_BACKEND_MEADE, ROT_FUNCNAMA(meade) },
    { ROT_IOPTRON, ROT_BACKEND_IOPTRON, ROT_FUNCNAMA(ioptron) },
    { ROT_SATEL, ROT_BACKEND_SATEL, ROT_FUNCNAMA(satel) },
    { ROT_RADANT, ROT_BACKEND_RADANT, ROT_FUNCNAMA(radant)},
#if HAVE_LIBINDI
    { ROT_INDI, ROT_BACKEND_INDI, ROT_FUNCNAMA(indi) },
#endif
#if defined(ANDROID) || defined(__ANDROID__)
    { ROT_ANDROIDSENSOR, ROT_BACKEND_ANDROIDSENSOR, ROT_FUNCNAMA(androidsensor) },
#endif
    { ROT_GRBLTRK, ROT_BACKEND_GRBLTRK, ROT_FUNCNAMA(grbltrk) },
    { 0, NULL }, /* end */
};


// Apparently, no rotator can be probed.

/*
 * ROT_BACKEND_LIST is here, please keep it up to date,
 *  i.e. each time you implement a new backend.
 */


/*
 * This struct to keep track of known rot models.
 * It is chained, and used in a hash table, see below.
 */
//! @cond Doxygen_Suppress
struct rot_list
{
    const struct rot_caps *caps;
    struct rot_list *next;
};
//! @endcond


//! @cond Doxygen_Suppress
#define ROTLSTHASHSZ 16
#define HASH_FUNC(a) ((a)%ROTLSTHASHSZ)
//! @endcond


/*
 * The rot_hash_table is a hash table pointing to a list of next==NULL
 *  terminated caps.
 */
static struct rot_list *rot_hash_table[ROTLSTHASHSZ] = { NULL, };


static int rot_lookup_backend(rot_model_t rot_model);


/*
 * Basically, this is a hash insert function that doesn't check for dup!
 */
//! @cond Doxygen_Suppress
int HAMLIB_API rot_register(const struct rot_caps *caps)
{
    int hval;
    struct rot_list *p;

    if (!caps)
    {
        return -RIG_EINVAL;
    }

    rot_debug(RIG_DEBUG_VERBOSE, "rot_register (%d)\n", caps->rot_model);

#ifndef DONT_WANT_DUP_CHECK

    if (rot_get_caps(caps->rot_model) != NULL)
    {
        return -RIG_EINVAL;
    }

#endif

    p = (struct rot_list *)calloc(1, sizeof(struct rot_list));

    if (!p)
    {
        return -RIG_ENOMEM;
    }

    hval = HASH_FUNC(caps->rot_model);
    p->caps = caps;
    // p->handle = NULL;
    p->next = rot_hash_table[hval];
    rot_hash_table[hval] = p;

    return RIG_OK;
}
//! @endcond


/*
 * Get rot capabilities.
 * i.e. rot_hash_table lookup
 */
//! @cond Doxygen_Suppress
const struct rot_caps *HAMLIB_API rot_get_caps(rot_model_t rot_model)
{
    struct rot_list *p;

    for (p = rot_hash_table[HASH_FUNC(rot_model)]; p; p = p->next)
    {
        if (p->caps->rot_model == rot_model)
        {
            return p->caps;
        }
    }

    return NULL;    /* sorry, caps not registered! */
}
//! @endcond


/*
 * lookup for backend index in rot_backend_list table,
 * according to BACKEND_NUM
 * return -1 if not found.
 */
static int rot_lookup_backend(rot_model_t rot_model)
{
    int i;

    for (i = 0; i < ROT_BACKEND_MAX && rot_backend_list[i].be_name; i++)
    {
        if (ROT_BACKEND_NUM(rot_model) ==
                rot_backend_list[i].be_num)
        {
            return i;
        }
    }

    return -1;
}


/*
 * rot_check_backend
 * check the backend declaring this model has been loaded
 * and if not loaded already, load it!
 * This permits seamless operation in rot_init.
 */
//! @cond Doxygen_Suppress
int HAMLIB_API rot_check_backend(rot_model_t rot_model)
{
    const struct rot_caps *caps;
    int be_idx;
    int retval;

    /* already loaded ? */
    caps = rot_get_caps(rot_model);

    if (caps)
    {
        return RIG_OK;
    }

    be_idx = rot_lookup_backend(rot_model);

    /*
     * Never heard about this backend family!
     */
    if (be_idx == -1)
    {
        rot_debug(RIG_DEBUG_VERBOSE,
                  "%s: unsupported backend %d for model %d\n",
                  __func__,
                  ROT_BACKEND_NUM(rot_model),
                  rot_model);

        return -RIG_ENAVAIL;
    }

    retval = rot_load_backend(rot_backend_list[be_idx].be_name);

    return retval;
}
//! @endcond


//! @cond Doxygen_Suppress
int HAMLIB_API rot_unregister(rot_model_t rot_model)
{
    int hval;
    struct rot_list *p, *q;

    hval = HASH_FUNC(rot_model);
    q = NULL;

    for (p = rot_hash_table[hval]; p; p = p->next)
    {
        if (p->caps->rot_model == rot_model)
        {
            if (q == NULL)
            {
                rot_hash_table[hval] = p->next;
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
 * rot_list_foreach
 * executes cfunc on all the elements stored in the rot hash list
 */
//! @cond Doxygen_Suppress
int HAMLIB_API rot_list_foreach(int (*cfunc)(const struct rot_caps *,
                                rig_ptr_t),
                                rig_ptr_t data)
{
    struct rot_list *p;
    int i;

    if (!cfunc)
    {
        return -RIG_EINVAL;
    }

    for (i = 0; i < ROTLSTHASHSZ; i++)
    {
        for (p = rot_hash_table[i]; p; p = p->next)
            if ((*cfunc)(p->caps, data) == 0)
            {
                return RIG_OK;
            }
    }

    return RIG_OK;
}
//! @endcond


/*
 * rot_probe_all
 * called straight by rot_probe
 */
//! @cond Doxygen_Suppress
rot_model_t HAMLIB_API rot_probe_all(hamlib_port_t *p)
{
    int i;
    rot_model_t rot_model;

    for (i = 0; i < ROT_BACKEND_MAX && rot_backend_list[i].be_name; i++)
    {
        if (rot_backend_list[i].be_probe)
        {
            rot_model = (*rot_backend_list[i].be_probe)(p);

            if (rot_model != ROT_MODEL_NONE)
            {
                return rot_model;
            }
        }
    }

    return ROT_MODEL_NONE;
}
//! @endcond


//! @cond Doxygen_Suppress
int rot_load_all_backends()
{
    int i;

    for (i = 0; i < ROT_BACKEND_MAX && rot_backend_list[i].be_name; i++)
    {
        rot_load_backend(rot_backend_list[i].be_name);
    }

    return RIG_OK;
}
//! @endcond


/*
 * rot_load_backend
 * Dynamically load a rot backend through dlopen mechanism
 */
//! @cond Doxygen_Suppress
int HAMLIB_API rot_load_backend(const char *be_name)
{
    int status;
    int (*be_init)(rig_ptr_t);
    int i;

    for (i = 0; i < ROT_BACKEND_MAX && rot_backend_list[i].be_name; i++)
    {
        if (!strcmp(be_name, rot_backend_list[i].be_name))
        {
            be_init = rot_backend_list[i].be_init;

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
