/*
 *  Hamlib Interface - provides registering for dynamically loadable backends.
 *  Copyright (c) 2000-2015 by Stephane Fillod
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
 * \brief Dynamic registration of backends
 * \file register.c
 *
 * doc todo: Let's explain what's going on here!
 */

#include <hamlib/config.h>

#include <errno.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <sys/types.h>

#include <register.h>

#include <hamlib/rig.h>
#include "misc.h"

//! @cond Doxygen_Suppress
#ifndef PATH_MAX
#  define PATH_MAX       1024
#endif

#define RIG_BACKEND_MAX 32

#define DEFINE_INITRIG_BACKEND(backend) \
    int MAKE_VERSIONED_FN(PREFIX_INITRIG, ABI_VERSION, backend(void *be_handle)); \
    rig_model_t MAKE_VERSIONED_FN(PREFIX_PROBERIG, ABI_VERSION, backend(hamlib_port_t *port, rig_probe_func_t cfunc, rig_ptr_t data))

#define RIG_FUNCNAMA(backend) MAKE_VERSIONED_FN(PREFIX_INITRIG, ABI_VERSION, backend)
#define RIG_FUNCNAMB(backend) MAKE_VERSIONED_FN(PREFIX_PROBERIG, ABI_VERSION, backend)

#define RIG_FUNCNAM(backend) RIG_FUNCNAMA(backend),RIG_FUNCNAMB(backend)

/*
 * RIG_BACKEND_LIST is defined here, please keep it up to date,
 *  i.e. each time you implement a new backend.
 */
DEFINE_INITRIG_BACKEND(dummy);
DEFINE_INITRIG_BACKEND(yaesu);
DEFINE_INITRIG_BACKEND(kenwood);
DEFINE_INITRIG_BACKEND(icom);
DEFINE_INITRIG_BACKEND(icmarine);
DEFINE_INITRIG_BACKEND(pcr);
DEFINE_INITRIG_BACKEND(aor);
DEFINE_INITRIG_BACKEND(jrc);
DEFINE_INITRIG_BACKEND(uniden);
DEFINE_INITRIG_BACKEND(drake);
DEFINE_INITRIG_BACKEND(lowe);
DEFINE_INITRIG_BACKEND(racal);
DEFINE_INITRIG_BACKEND(wj);
DEFINE_INITRIG_BACKEND(skanti);
DEFINE_INITRIG_BACKEND(tentec);
DEFINE_INITRIG_BACKEND(alinco);
DEFINE_INITRIG_BACKEND(kachina);
DEFINE_INITRIG_BACKEND(tapr);
DEFINE_INITRIG_BACKEND(flexradio);
DEFINE_INITRIG_BACKEND(rft);
DEFINE_INITRIG_BACKEND(kit);
DEFINE_INITRIG_BACKEND(tuner);
DEFINE_INITRIG_BACKEND(rs);
DEFINE_INITRIG_BACKEND(prm80);
DEFINE_INITRIG_BACKEND(adat);
DEFINE_INITRIG_BACKEND(dorji);
DEFINE_INITRIG_BACKEND(barrett);
DEFINE_INITRIG_BACKEND(elad);
DEFINE_INITRIG_BACKEND(codan);
DEFINE_INITRIG_BACKEND(gomspace);
//! @endcond

#ifdef HAVE_WINRADIO
//! @cond Doxygen_Suppress
DEFINE_INITRIG_BACKEND(winradio);
//! @endcond
#endif


/**
 *  \def rig_backend_list
 *  \brief Static list of rig models.
 *
 *  This is a NULL terminated list of available rig backends. Each entry in
 *  the list consists of two fields: The branch number, which is an integer,
 *  and the branch name, which is a character string.
 */
static struct
{
    int be_num;
    const char *be_name;
    int (* be_init_all)(void *handle);
    rig_model_t (* be_probe_all)(hamlib_port_t *, rig_probe_func_t, rig_ptr_t);
} rig_backend_list[RIG_BACKEND_MAX] =
{
    { RIG_DUMMY, RIG_BACKEND_DUMMY, RIG_FUNCNAMA(dummy) },
    { RIG_YAESU, RIG_BACKEND_YAESU, RIG_FUNCNAM(yaesu) },
    { RIG_KENWOOD, RIG_BACKEND_KENWOOD, RIG_FUNCNAM(kenwood) },
    { RIG_ICOM, RIG_BACKEND_ICOM, RIG_FUNCNAM(icom) },
    { RIG_ICMARINE, RIG_BACKEND_ICMARINE, RIG_FUNCNAMA(icmarine) },
    { RIG_PCR, RIG_BACKEND_PCR, RIG_FUNCNAMA(pcr) },
    { RIG_AOR, RIG_BACKEND_AOR, RIG_FUNCNAMA(aor) },
    { RIG_JRC, RIG_BACKEND_JRC, RIG_FUNCNAMA(jrc) },
    { RIG_UNIDEN, RIG_BACKEND_UNIDEN, RIG_FUNCNAM(uniden) },
    { RIG_DRAKE, RIG_BACKEND_DRAKE, RIG_FUNCNAM(drake) },
    { RIG_LOWE, RIG_BACKEND_LOWE, RIG_FUNCNAM(lowe) },
    { RIG_RACAL, RIG_BACKEND_RACAL, RIG_FUNCNAMA(racal) },
    { RIG_WJ, RIG_BACKEND_WJ, RIG_FUNCNAMA(wj) },
    { RIG_SKANTI, RIG_BACKEND_SKANTI, RIG_FUNCNAMA(skanti) },
#ifdef HAVE_WINRADIO
    { RIG_WINRADIO, RIG_BACKEND_WINRADIO, RIG_FUNCNAMA(winradio) },
#endif /* HAVE_WINRADIO */
    { RIG_TENTEC, RIG_BACKEND_TENTEC, RIG_FUNCNAMA(tentec) },
    { RIG_ALINCO, RIG_BACKEND_ALINCO, RIG_FUNCNAMA(alinco) },
    { RIG_KACHINA, RIG_BACKEND_KACHINA, RIG_FUNCNAMA(kachina) },
    { RIG_TAPR, RIG_BACKEND_TAPR, RIG_FUNCNAMA(tapr) },
    { RIG_FLEXRADIO, RIG_BACKEND_FLEXRADIO, RIG_FUNCNAMA(flexradio) },
    { RIG_RFT, RIG_BACKEND_RFT, RIG_FUNCNAMA(rft) },
    { RIG_KIT, RIG_BACKEND_KIT, RIG_FUNCNAMA(kit) },
    { RIG_TUNER, RIG_BACKEND_TUNER, RIG_FUNCNAMA(tuner) },
    { RIG_RS, RIG_BACKEND_RS, RIG_FUNCNAMA(rs) },
    { RIG_PRM80, RIG_BACKEND_PRM80, RIG_FUNCNAMA(prm80) },
    { RIG_ADAT, RIG_BACKEND_ADAT, RIG_FUNCNAM(adat) },
    { RIG_DORJI, RIG_BACKEND_DORJI, RIG_FUNCNAMA(dorji) },
    { RIG_BARRETT, RIG_BACKEND_BARRETT, RIG_FUNCNAMA(barrett) },
    { RIG_ELAD, RIG_BACKEND_ELAD, RIG_FUNCNAMA(elad) },
    { RIG_CODAN, RIG_BACKEND_CODAN, RIG_FUNCNAMA(codan) },
    { RIG_GOMSPACE, RIG_BACKEND_GOMSPACE, RIG_FUNCNAM(gomspace) },
    { 0, NULL }, /* end */
};


/*
 * This struct to keep track of known rig models.
 * It is chained, and used in a hash table, see below.
 */
//! @cond Doxygen_Suppress
struct rig_list
{
    const struct rig_caps *caps;
    struct rig_list *next;
};
//! @endcond


// This size has to be > than the max# of rigs for any manufacturer
// A fatal error will occur when running rigctl if this value is too small
//! @cond Doxygen_Suppress
#define RIGLSTHASHSZ 65535
#define HASH_FUNC(a) ((a)%RIGLSTHASHSZ)
//! @endcond


/*
 * The rig_hash_table is a hash table pointing to a list of next==NULL
 *  terminated caps.
 */
static struct rig_list *rig_hash_table[RIGLSTHASHSZ] = { NULL, };


static int rig_lookup_backend(rig_model_t rig_model);


/*
 * Basically, this is a hash insert function that doesn't check for dup!
 */
//! @cond Doxygen_Suppress
int HAMLIB_API rig_register(const struct rig_caps *caps)
{
    int hval;
    struct rig_list *p;

    //rig_debug(RIG_DEBUG_VERBOSE, "%s called\n", __func__);

    if (!caps)
    {
        return -RIG_EINVAL;
    }

#if 0
    rig_debug(RIG_DEBUG_VERBOSE,
              "%s: rig_register (%u)\n",
              __func__,
              caps->rig_model);
#endif

    p = (struct rig_list *)calloc(1, sizeof(struct rig_list));

    if (!p)
    {
        return -RIG_ENOMEM;
    }

    hval = HASH_FUNC(caps->rig_model);

    if (rig_hash_table[hval])
    {
        printf("Hash collision!!! Fatal error!!\n");
        exit(1);
    }

    p->caps = caps;
    // p->handle = NULL;
    p->next = rig_hash_table[hval];
    rig_hash_table[hval] = p;

    //RETURNFUNC(RIG_OK);
    return RIG_OK;
}
//! @endcond

/*
 * Get rig capabilities.
 * ie. rig_hash_table lookup
 */

//! @cond Doxygen_Suppress
const struct rig_caps *HAMLIB_API rig_get_caps(rig_model_t rig_model)
{
    struct rig_list *p;

    for (p = rig_hash_table[HASH_FUNC(rig_model)]; p; p = p->next)
    {
        if (p->caps->rig_model == rig_model)
        {
            return p->caps;
        }
    }

    return NULL;    /* sorry, caps not registered! */
}
//! @endcond

/*
 * lookup for backend index in rig_backend_list table,
 * according to BACKEND_NUM
 * return -1 if not found.
 */
//! @cond Doxygen_Suppress
static int rig_lookup_backend(rig_model_t rig_model)
{
    int i;

    for (i = 0; i < RIG_BACKEND_MAX && rig_backend_list[i].be_name; i++)
    {
        if (RIG_BACKEND_NUM(rig_model) ==
                rig_backend_list[i].be_num)

        {
            return i;
        }
    }

    return -1;
}
//! @endcond

/*
 * rig_check_backend
 * check the backend declaring this model has been loaded
 * and if not loaded already, load it!
 * This permits seamless operation in rig_init.
 */
//! @cond Doxygen_Suppress
int HAMLIB_API rig_check_backend(rig_model_t rig_model)
{
    const struct rig_caps *caps;
    int be_idx;
    int retval;
    int i, n;

    /* already loaded ? */
    caps = rig_get_caps(rig_model);

    if (caps)
    {
        return RIG_OK;
    }

    // hmmm...no caps so did we already load the rigs?
    for (n = 0, i = 0; i < RIGLSTHASHSZ; i++)
    {
        if (rig_hash_table[i]) { ++n; }
    }

#if 0 // this stopped a 2nd rig_init call with a valid model to fail -- reversing

    if (n > 1)
    {
        rig_debug(RIG_DEBUG_ERR, "%s: rig model %u not found and rig count=%d\n",
                  __func__, rig_model, n);
        return -RIG_ENAVAIL;
    }

#endif

    be_idx = rig_lookup_backend(rig_model);

    /*
     * Never heard about this backend family!
     */
    if (be_idx == -1)
    {
        rig_debug(RIG_DEBUG_VERBOSE,
                  "rig_check_backend: unsupported backend %u for model %u\n",
                  RIG_BACKEND_NUM(rig_model),
                  rig_model);
        return -RIG_ENAVAIL;
    }

    // do we need to load the backend?
//    if (rig_backend_list[be_idx].be_init_all == 0)
    {
        retval = rig_load_backend(rig_backend_list[be_idx].be_name);
    }
#if 0
    else
    {
        retval = RIG_OK;
    }

#endif

    return retval;
}
//! @endcond



//! @cond Doxygen_Suppress
int HAMLIB_API rig_unregister(rig_model_t rig_model)
{
    int hval;
    struct rig_list *p, *q;

    hval = HASH_FUNC(rig_model);
    q = NULL;

    for (p = rig_hash_table[hval]; p; p = p->next)
    {
        if (p->caps->rig_model == rig_model)
        {
            if (q == NULL)
            {
                rig_hash_table[hval] = p->next;
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
 * rig_list_foreach
 * executes cfunc on all the elements stored in the rig hash list
 */
//! @cond Doxygen_Suppress
int HAMLIB_API rig_list_foreach(int (*cfunc)(const struct rig_caps *,
                                rig_ptr_t),
                                rig_ptr_t data)
{
    struct rig_list *p;
    int i;

    if (!cfunc)
    {
        return -RIG_EINVAL;
    }

    for (i = 0; i < RIGLSTHASHSZ; i++)
    {
        struct rig_list *next = NULL;

        for (p = rig_hash_table[i]; p; p = next)
        {
            next = p->next;       /* read before call in case it is unregistered */

            if ((*cfunc)(p->caps, data) == 0)
            {
                return RIG_OK;
            }
        }
    }

    return RIG_OK;
}
//! @endcond

/*
 * rig_list_foreach_model
 * executes cfunc on all the elements stored in the rig hash list
 */
//! @cond Doxygen_Suppress
int HAMLIB_API rig_list_foreach_model(int (*cfunc)(const rig_model_t rig_model,
                                      rig_ptr_t),
                                      rig_ptr_t data)
{
    struct rig_list *p;
    int i;

    if (!cfunc)
    {
        return -RIG_EINVAL;
    }

    for (i = 0; i < RIGLSTHASHSZ; i++)
    {
        struct rig_list *next = NULL;

        for (p = rig_hash_table[i]; p; p = next)
        {
            next = p->next;       /* read before call in case it is unregistered */

            if ((*cfunc)(p->caps->rig_model, data) == 0)
            {
                return RIG_OK;
            }
        }
    }

    return RIG_OK;
}
//! @endcond

//! @cond Doxygen_Suppress
static int dummy_rig_probe(const hamlib_port_t *p,
                           rig_model_t model,
                           rig_ptr_t data)
{
    rig_debug(RIG_DEBUG_TRACE, "Found rig, model %u\n", model);

    return RIG_OK;
}
//! @endcond


/*
 * rig_probe_first
 * called straight by rig_probe
 */
//! @cond Doxygen_Suppress
rig_model_t rig_probe_first(hamlib_port_t *p)
{
    int i;
    rig_model_t model;

    for (i = 0; i < RIG_BACKEND_MAX && rig_backend_list[i].be_name; i++)
    {
        if (rig_backend_list[i].be_probe_all)
        {
            model = (*rig_backend_list[i].be_probe_all)(p, dummy_rig_probe,
                    (rig_ptr_t)NULL);

            /* stop at first one found */
            if (model != RIG_MODEL_NONE)
            {
                return model;
            }
        }
    }

    return RIG_MODEL_NONE;
}
//! @endcond


/*
 * rig_probe_all_backends
 * called straight by rig_probe_all
 */
//! @cond Doxygen_Suppress
int rig_probe_all_backends(hamlib_port_t *p,
                           rig_probe_func_t cfunc,
                           rig_ptr_t data)
{
    int i;

    for (i = 0; i < RIG_BACKEND_MAX && rig_backend_list[i].be_name; i++)
    {
        if (rig_backend_list[i].be_probe_all)
        {
            (*rig_backend_list[i].be_probe_all)(p, cfunc, data);
        }
    }

    return RIG_OK;
}
//! @endcond


//! @cond Doxygen_Suppress
int rig_load_all_backends()
{
    int i;

    memset(rig_hash_table, 0, sizeof(rig_hash_table));

    for (i = 0; i < RIG_BACKEND_MAX && rig_backend_list[i].be_name; i++)
    {
        rig_load_backend(rig_backend_list[i].be_name);
    }

    return RIG_OK;
}
//! @endcond


//! @cond Doxygen_Suppress
typedef int (*backend_init_t)(rig_ptr_t);
//! @endcond


/*
 * rig_load_backend
 */
//! @cond Doxygen_Suppress
int HAMLIB_API rig_load_backend(const char *be_name)
{
    int i;
    backend_init_t be_init;

    for (i = 0; i < RIG_BACKEND_MAX && rig_backend_list[i].be_name; i++)
    {
        if (!strcmp(be_name, rig_backend_list[i].be_name))
        {
            be_init = rig_backend_list[i].be_init_all ;

            if (be_init)
            {
                return (*be_init)(NULL);
            }
            else
            {
                return -RIG_EINVAL;
            }
        }
    }

    return -RIG_EINVAL;
}
//! @endcond
