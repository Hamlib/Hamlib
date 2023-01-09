/*
 *  Hamlib Interface - main file
 *  Copyright (c) 2021 by Mikael Nousiainen
 *  Copyright (c) 2000-2012 by Stephane Fillod
 *  Copyright (c) 2000-2003 by Frank Singleton
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
 * \addtogroup rig
 * @{
 */

/**
 * \file src/rig.c
 * \brief Ham Radio Control Libraries interface
 * \author Stephane Fillod
 * \author Frank Singleton
 * \date 2000-2012
 *
 * Hamlib provides a user-callable API, a set of "front-end" routines that
 * call rig-specific "back-end" routines which actually communicate with
 * the physical rig.
 */

/**
 * \page rig Rig (radio) interface
 *
 * For us, a "rig" is an item of general remote controllable radio equipment.
 * Generally, there are a VFO settings, gain controls, etc.
 */

/**
 * \example ../tests/testrig.c
 */

#include "hamlib/rig.h"
#include "hamlib/config.h"

#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <stdio.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <errno.h>
#ifdef HAVE_PTHREAD
#include <pthread.h>
#endif


#include <hamlib/rig.h>
#include "serial.h"
#include "parallel.h"
#include "usb_port.h"
#include "network.h"
#include "event.h"
#include "cm108.h"
#include "gpio.h"
#include "misc.h"
#include "sprintflst.h"
#include "hamlibdatetime.h"
#include "cache.h"

/**
 * \brief Hamlib release number
 *
 * The version number has the format x.y.z
 */
/*
 * Careful: The hamlib 1.2 ABI implicitly specifies a size of 21 bytes for
 * the hamlib_version string.  Changing the size provokes a warning from the
 * dynamic loader.
 */
const char *hamlib_license = "LGPL";
//! @cond Doxygen_Suppress
const char hamlib_version[21] = "Hamlib " PACKAGE_VERSION;
const char *hamlib_version2 = "Hamlib " PACKAGE_VERSION " " HAMLIBDATETIME;
HAMLIB_EXPORT_VAR(int) cookie_use;
HAMLIB_EXPORT_VAR(int) lock_mode; // for use by rigctld
HAMLIB_EXPORT_VAR(powerstat_t) rig_powerstat; // for use by rigctld
//! @endcond

struct rig_caps caps_test;

/**
 * \brief Hamlib copyright notice
 */
const char *hamlib_copyright2 =
    "Copyright (C) 2000-2012 Stephane Fillod\n"
    "Copyright (C) 2000-2003 Frank Singleton\n"
    "Copyright (C) 2014-2020 Michael Black W9MDB\n"
    "This is free software; see the source for copying conditions.  There is NO\n"
    "warranty; not even for MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.";
//! @cond Doxygen_Suppress
const char hamlib_copyright[231] = /* hamlib 1.2 ABI specifies 231 bytes */
    "Copyright (C) 2000-2012 Stephane Fillod\n"
    "Copyright (C) 2000-2003 Frank Singleton\n"
    "This is free software; see the source for copying conditions.  There is NO\n"
    "warranty; not even for MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.";
//! @endcond


#ifndef DOC_HIDDEN

#if defined(WIN32) && !defined(__CYGWIN__)
#  define DEFAULT_SERIAL_PORT "\\\\.\\COM1"
#elif BSD
#  define DEFAULT_SERIAL_PORT "/dev/cuaa0"
#elif MACOSX
#  define DEFAULT_SERIAL_PORT "/dev/cu.usbserial"
#else
#  define DEFAULT_SERIAL_PORT "/dev/ttyS0"
#endif

#if defined(WIN32)
#  define DEFAULT_PARALLEL_PORT "\\\\.\\$VDMLPT1"
#elif defined(HAVE_DEV_PPBUS_PPI_H)
#  define DEFAULT_PARALLEL_PORT "/dev/ppi0"
#else
#  define DEFAULT_PARALLEL_PORT "/dev/parport0"
#endif

#if defined(WIN32) && !defined(__CYGWIN__)
#  define DEFAULT_CM108_PORT "fixme"
#elif BSD
#  define DEFAULT_CM108_PORT "fixme"
#else
#  define DEFAULT_CM108_PORT "/dev/hidraw0"
#endif

#if defined(WIN32) && !defined(__CYGWIN__)
/* FIXME: Determine correct GPIO bit number for W32 using MinGW. */
#  define DEFAULT_CM108_PTT_BITNUM 2
#elif BSD
/* FIXME: Determine correct GPIO bit number for *BSD. */
#  define DEFAULT_CM108_PTT_BITNUM 2
#else
#  define DEFAULT_CM108_PTT_BITNUM 2
#endif

#define DEFAULT_GPIO_PORT "0"

#define CHECK_RIG_ARG(r) (!(r) || !(r)->caps || !(r)->state.comm_state)
#define CHECK_RIG_CAPS(r) (!(r) || !(r)->caps)

#define LOCK \

#ifdef PTHREAD
#define MUTEX(var) static pthread_mutex_t var = PTHREAD_MUTEX_INITIALIZER
#define MUTEX_LOCK(var) pthread_mutex_lock(var)
#define MUTEX_UNLOCK(var)  pthread_mutex_unlock(var)
#else
#define MUTEX(var)
#define MUTEX_LOCK(var)
#define MUTEX_UNLOCK(var)
#endif

/*
 * Data structure to track the opened rig (by rig_open)
 */
struct opened_rig_l
{
    RIG *rig;
    struct opened_rig_l *next;
};
static struct opened_rig_l *opened_rig_list = { NULL };


/*
 * Careful, the order must be the same as their RIG_E* counterpart!
 * TODO: localise the messages..
 */
static const char *const rigerror_table[] =
{
    "Command completed successfully",
    "Invalid parameter",
    "Invalid configuration",
    "Memory shortage",
    "Feature not implemented",
    "Communication timed out",
    "IO error",
    "Internal Hamlib error",
    "Protocol error",
    "Command rejected by the rig",
    "Command performed, but arg truncated, result not guaranteed",
    "Feature not available",
    "Target VFO unaccessible",
    "Communication bus error",
    "Communication bus collision",
    "NULL RIG handle or invalid pointer parameter",
    "Invalid VFO",
    "Argument out of domain of func",
    "Function deprecated",
    "Security error password not provided or crypto failure",
    "Rig is not powered on"
};


#define ERROR_TBL_SZ (sizeof(rigerror_table)/sizeof(char *))

#ifdef HAVE_PTHREAD
typedef struct async_data_handler_args_s
{
    RIG *rig;
} async_data_handler_args;

typedef struct async_data_handler_priv_data_s
{
    pthread_t thread_id;
    async_data_handler_args args;
} async_data_handler_priv_data;

static int async_data_handler_start(RIG *rig);
static int async_data_handler_stop(RIG *rig);
void *async_data_handler(void *arg);
#endif

/*
 * track which rig is opened (with rig_open)
 * needed at least for transceive mode
 */
static int add_opened_rig(RIG *rig)
{
    struct opened_rig_l *p;

    p = (struct opened_rig_l *)calloc(1, sizeof(struct opened_rig_l));

    if (!p)
    {
        RETURNFUNC2(-RIG_ENOMEM);
    }

    p->rig = rig;
    p->next = opened_rig_list;
    opened_rig_list = p;

    RETURNFUNC2(RIG_OK);
}


static int remove_opened_rig(RIG *rig)
{
    struct opened_rig_l *p, *q;
    q = NULL;

    for (p = opened_rig_list; p; p = p->next)
    {
        if (p->rig == rig)
        {
            if (q == NULL)
            {
                opened_rig_list = opened_rig_list->next;
            }
            else
            {
                q->next = p->next;
            }

            free(p);
            return (RIG_OK);
        }

        q = p;
    }

    return (-RIG_EINVAL); /* Not found in list ! */
}


/**
 * \brief execs cfunc() on each opened rig
 * \param cfunc The function to be executed on each rig
 * \param data  Data pointer to be passed to cfunc()
 *
 *  Calls cfunc() function for each opened rig.
 *  The contents of the opened rig table
 *  is processed in random order according to a function
 *  pointed to by \a cfunc, which is called with two arguments,
 *  the first pointing to the RIG handle, the second
 *  to a data pointer \a data.
 *  If \a data is not needed, then it can be set to NULL.
 *  The processing of the opened rig table is stopped
 *  when cfunc() returns 0.
 * \internal
 *
 * \return always RIG_OK.
 */
int foreach_opened_rig(int (*cfunc)(RIG *, rig_ptr_t), rig_ptr_t data)
{
    struct opened_rig_l *p;

    for (p = opened_rig_list; p; p = p->next)
    {
        if ((*cfunc)(p->rig, data) == 0)
        {
            return (RIG_OK);
        }
    }

    return (RIG_OK);
}

#endif /* !DOC_HIDDEN */


char debugmsgsave[DEBUGMSGSAVE_SIZE] = "";
char debugmsgsave2[DEBUGMSGSAVE_SIZE] = ""; // deprecated
char debugmsgsave3[DEBUGMSGSAVE_SIZE] = ""; // deprecated

MUTEX(debugmsgsave);

void add2debugmsgsave(const char *s)
{
    char *p;
    char stmp[DEBUGMSGSAVE_SIZE];
    int i, nlines;
    int maxmsg = DEBUGMSGSAVE_SIZE / 2;
    MUTEX_LOCK(debugmsgsave);
    memset(stmp, 0, sizeof(stmp));
    p = debugmsgsave;

    // we'll keep 20 lines including this one
    // so count the lines
    for (i = 0, nlines = 0; debugmsgsave[i] != 0; ++i)
    {
        if (debugmsgsave[i] == '\n') { ++nlines; }
    }

    // strip the last 19 lines
    p =  debugmsgsave;

    while ((nlines > 19 || strlen(debugmsgsave) > maxmsg) && p != NULL)
    {
        p = strchr(debugmsgsave, '\n');

        if (p && strlen(p + 1) > 0)
        {
            if (strlen(p + 1) > 0)
            {
                strcpy(stmp, p + 1);
                strcpy(debugmsgsave, stmp);
            }
            else
            {
                debugmsgsave[0] = '\0';
            }
        }

        --nlines;

        if (nlines == 0 && strlen(debugmsgsave) > maxmsg) { strcpy(debugmsgsave, "!!!!debugmsgsave too long\n"); }
    }

    if (strlen(stmp) + strlen(s) + 1 < DEBUGMSGSAVE_SIZE)
    {
        strcat(debugmsgsave, s);
    }
    else
    {
        rig_debug(RIG_DEBUG_BUG,
                  "%s: debugmsgsave overflow!! len of debugmsgsave=%d, len of add=%d\n", __func__,
                  (int)strlen(debugmsgsave), (int)strlen(s));
    }

    MUTEX_UNLOCK(debugmsgsave);
}

/**
 * \brief get string describing the error code
 * \param errnum    The error code
 * \return the appropriate description string, otherwise a NULL pointer
 * if the error code is unknown.
 *
 * Returns a string describing the error code passed in the argument \a
 * errnum.
 *
 * \todo support gettext/localization
 */
const char *HAMLIB_API rigerror2(int errnum) // returns single-line message
{
    errnum = abs(errnum);

    if (errnum >= ERROR_TBL_SZ)
    {
        // This should not happen, but if it happens don't return NULL
        return "ERR_OUT_OF_RANGE";
    }

    static char msg[DEBUGMSGSAVE_SIZE];
    snprintf(msg, sizeof(msg), "%s\n", rigerror_table[errnum]);
    return msg;
}

const char *HAMLIB_API rigerror(int errnum)
{
    errnum = abs(errnum);

    if (errnum >= ERROR_TBL_SZ)
    {
        // This should not happen, but if it happens don't return NULL
        return "ERR_OUT_OF_RANGE";
    }

    static char msg[DEBUGMSGSAVE_SIZE];
#if 0
    // we have to remove LF from debugmsgsave since calling function controls LF
    char *p = &debugmsgsave[strlen(debugmsgsave) - 1];

    if (*p == '\n') { *p = 0; }

#endif

#if 0
    SNPRINTF(msg, sizeof(msg), "%.80s\n%.15000s%.15000s%.15000s",
             rigerror_table[errnum],
             debugmsgsave3, debugmsgsave2, debugmsgsave);
#else
    snprintf(msg, sizeof(msg), "%s\n", rigerror_table[errnum]);
    add2debugmsgsave(msg);
    snprintf(msg, sizeof(msg), "%s", debugmsgsave);
#endif
    return msg;
}

// We use a couple of defined pointer to determine if the shared library changes
void *caps_test_rig_model = &caps_test.rig_model;
void *caps_test_macro_name = &caps_test.macro_name;

// check and show WARN if rig_caps structure doesn't match
// this tests for shared library incompatibility
static int rig_check_rig_caps()
{
    int rc = RIG_OK;

    if (&caps_test.rig_model != caps_test_rig_model)
    {
        rc = -RIG_EINTERNAL;
        rig_debug(RIG_DEBUG_WARN, "%s: shared library change#1\n", __func__);
    }

    if (&caps_test.macro_name != caps_test_macro_name)
    {
        rc = -RIG_EINTERNAL;
        rig_debug(RIG_DEBUG_WARN, "%s: shared library change#2\n", __func__);
    }

    //if (rc != RIG_OK)
    {
        rig_debug(RIG_DEBUG_TRACE, "%s: p1=%p, p2=%p, rig_model=%p, macro_name=%p\n",
                  __func__, caps_test_rig_model, caps_test_macro_name, &caps_test.rig_model,
                  &caps_test.macro_name);
    }

    return (rc);
}

/**
 * \brief allocate a new RIG handle
 * \param rig_model The rig model for this new handle
 *
 * Allocates a new RIG handle and initializes the associated data
 * for \a rig_model.
 *
 * \return a pointer to the #RIG handle otherwise NULL if memory allocation
 * failed or \a rig_model is unknown (e.g. backend autoload failed).
 *
 * \sa rig_cleanup(), rig_open()
 */
RIG *HAMLIB_API rig_init(rig_model_t rig_model)
{
    RIG *rig;
    const struct rig_caps *caps;
    struct rig_state *rs;
    int i;

    rig_check_rig_caps();

    rig_check_backend(rig_model);

    caps = rig_get_caps(rig_model);

    if (!caps)
    {
        return (NULL);
    }

    rig_debug(RIG_DEBUG_VERBOSE, "%s: rig_model=%s %s\n", __func__, caps->mfg_name,
              caps->model_name);

    if (caps->hamlib_check_rig_caps != NULL)
    {
        if (caps->hamlib_check_rig_caps[0] != 'H'
                || strncmp(caps->hamlib_check_rig_caps, HAMLIB_CHECK_RIG_CAPS,
                           strlen(caps->hamlib_check_rig_caps)) != 0)
        {
            rig_debug(RIG_DEBUG_ERR,
                      "%s: Error validating integrity of rig_caps\nPossible hamlib DLL incompatibility\n",
                      __func__);
            return (NULL);
        }
    }
    else
    {
        rig_debug(RIG_DEBUG_WARN,
                  "%s: backend for %s does not contain hamlib_check_rig_caps\n", __func__,
                  caps->model_name);
    }

    /*
     * okay, we've found it. Allocate some memory and set it to zeros,
     * and especially  the callbacks
     */
    rig = calloc(1, sizeof(RIG));

    if (rig == NULL)
    {
        /*
         * FIXME: how can the caller know it's a memory shortage,
         *        and not "rig not found" ?
         */
        return (NULL);
    }

    /* caps is const, so we need to tell compiler
       that we know what we are doing */
    rig->caps = (struct rig_caps *) caps;

    /*
     * populate the rig->state
     * TODO: read the Preferences here!
     */
    rs = &rig->state;
#ifdef HAVE_PTHREAD
    pthread_mutex_init(&rs->mutex_set_transaction, NULL);
#endif

    rs->priv = NULL;
    rs->async_data_enabled = 0;
    rs->rigport.fd = -1;
    rs->pttport.fd = -1;
    rs->comm_state = 0;
#if 0 // extra debug if needed
    rig_debug(RIG_DEBUG_VERBOSE, "%s(%d): %p rs->comm_state==0?=%d\n", __func__,
              __LINE__, &rs->comm_state,
              rs->comm_state);
#endif
    rs->rigport.type.rig = caps->port_type; /* default from caps */
#if defined(HAVE_PTHREAD)
    rs->rigport.asyncio = 0;
#endif

    switch (caps->port_type)
    {
    case RIG_PORT_SERIAL:
        strncpy(rs->rigport.pathname, DEFAULT_SERIAL_PORT, HAMLIB_FILPATHLEN - 1);
        rs->rigport.parm.serial.rate = caps->serial_rate_max;   /* fastest ! */
        rs->rigport.parm.serial.data_bits = caps->serial_data_bits;
        rs->rigport.parm.serial.stop_bits = caps->serial_stop_bits;
        rs->rigport.parm.serial.parity = caps->serial_parity;
        rs->rigport.parm.serial.handshake = caps->serial_handshake;
        break;

    case RIG_PORT_PARALLEL:
        strncpy(rs->rigport.pathname, DEFAULT_PARALLEL_PORT, HAMLIB_FILPATHLEN - 1);
        break;

    /* Adding support for CM108 GPIO.  This is compatible with CM108 series
     * USB audio chips from CMedia and SSS1623 series USB audio chips from 3S
     */
    case RIG_PORT_CM108:
        strncpy(rs->rigport.pathname, DEFAULT_CM108_PORT, HAMLIB_FILPATHLEN);

        if (rs->rigport.parm.cm108.ptt_bitnum == 0)
        {
            rs->rigport.parm.cm108.ptt_bitnum = DEFAULT_CM108_PTT_BITNUM;
            rs->pttport.parm.cm108.ptt_bitnum = DEFAULT_CM108_PTT_BITNUM;
        }

        break;

    case RIG_PORT_GPIO:
        strncpy(rs->rigport.pathname, DEFAULT_GPIO_PORT, HAMLIB_FILPATHLEN);
        break;

    case RIG_PORT_NETWORK:
    case RIG_PORT_UDP_NETWORK:
        strncpy(rs->rigport.pathname, "127.0.0.1:4532", HAMLIB_FILPATHLEN - 1);
        break;

    default:
        strncpy(rs->rigport.pathname, "", HAMLIB_FILPATHLEN - 1);
    }

    rs->rigport.write_delay = caps->write_delay;
    rs->rigport.post_write_delay = caps->post_write_delay;
    rs->rigport.timeout = caps->timeout;
    rs->rigport.retry = caps->retry;
    rs->pttport.type.ptt = caps->ptt_type;
    rs->dcdport.type.dcd = caps->dcd_type;

    rs->vfo_comp = 0.0; /* override it with preferences */
    rs->current_vfo = RIG_VFO_CURR; /* we don't know yet! */
    rs->tx_vfo = RIG_VFO_CURR;  /* we don't know yet! */
    rs->poll_interval = 0; // disable polling by default
    rs->lo_freq = 0;
    rs->cache.timeout_ms = 500;  // 500ms cache timeout by default
    rs->cache.ptt = 0;

    // We are using range_list1 as the default
    // Eventually we will have separate model number for different rig variations
    // So range_list1 will become just range_list (per model)
    // See ic9700.c for a 5-model example
    // Every rig should have a rx_range
    // Rig backends need updating for new range_list format
    memcpy(rs->rx_range_list, caps->rx_range_list1,
           sizeof(struct freq_range_list)*HAMLIB_FRQRANGESIZ);
    memcpy(rs->tx_range_list, caps->tx_range_list1,
           sizeof(struct freq_range_list)*HAMLIB_FRQRANGESIZ);

    // if we don't have list1 we'll try list2
    if (rs->rx_range_list[0].startf == 0)
    {
        rig_debug(RIG_DEBUG_TRACE,
                  "%s: rx_range_list1 is empty, using rx_range_list2\n", __func__);
        memcpy(rs->tx_range_list, caps->rx_range_list2,
               sizeof(struct freq_range_list)*HAMLIB_FRQRANGESIZ);
        memcpy(rs->rx_range_list, caps->tx_range_list2,
               sizeof(struct freq_range_list)*HAMLIB_FRQRANGESIZ);
    }

    if (rs->tx_range_list[0].startf == 0)
    {
        rig_debug(RIG_DEBUG_VERBOSE, "%s: rig does not have tx_range!!\n", __func__);
        //return(NULL); // this is not fatal
    }

#if 0 // this is no longer applicable -- replace it with something?

// we need to be able to figure out what model radio we have
// before we can set up the rig_state with the rig's specific freq range
// if we can't figure out what model rig we have this is impossible
// so we will likely have to make this a parameter the user provides
// or eliminate this logic entirely and make specific RIG_MODEL entries
    switch (rs->itu_region)
    {
    case RIG_ITU_REGION1:
        memcpy(rs->tx_range_list, caps->tx_range_list1,
               sizeof(struct freq_range_list)*HAMLIB_FRQRANGESIZ);
        memcpy(rs->rx_range_list, caps->rx_range_list1,
               sizeof(struct freq_range_list)*HAMLIB_FRQRANGESIZ);
        break;

    case RIG_ITU_REGION2:
    case RIG_ITU_REGION3:
    default:
        memcpy(rs->tx_range_list, caps->tx_range_list2,
               sizeof(struct freq_range_list)*HAMLIB_FRQRANGESIZ);
        memcpy(rs->rx_range_list, caps->rx_range_list2,
               sizeof(struct freq_range_list)*HAMLIB_FRQRANGESIZ);
        break;
    }

#endif
    rs->vfo_list = 0;
    rs->mode_list = 0;

    for (i = 0; i < HAMLIB_FRQRANGESIZ
            && !RIG_IS_FRNG_END(caps->rx_range_list1[i]); i++)
    {
        rs->vfo_list |= caps->rx_range_list1[i].vfo;
        rs->mode_list |= caps->rx_range_list1[i].modes;
    }

    for (i = 0; i < HAMLIB_FRQRANGESIZ
            && !RIG_IS_FRNG_END(caps->tx_range_list1[i]); i++)
    {
        rs->vfo_list |= caps->tx_range_list1[i].vfo;
        rs->mode_list |= caps->tx_range_list1[i].modes;
    }

    for (i = 0; i < HAMLIB_FRQRANGESIZ
            && !RIG_IS_FRNG_END(caps->rx_range_list2[i]); i++)
    {
        rs->vfo_list |= caps->rx_range_list2[i].vfo;
        rs->mode_list |= caps->rx_range_list2[i].modes;
    }

    for (i = 0; i < HAMLIB_FRQRANGESIZ
            && !RIG_IS_FRNG_END(caps->tx_range_list2[i]); i++)
    {
        rs->vfo_list |= caps->tx_range_list2[i].vfo;
        rs->mode_list |= caps->tx_range_list2[i].modes;
    }

    if (rs->vfo_list & RIG_VFO_A) { rig_debug(RIG_DEBUG_VERBOSE, "%s: rig has VFO_A\n", __func__); }

    if (rs->vfo_list & RIG_VFO_B) { rig_debug(RIG_DEBUG_VERBOSE, "%s: rig has VFO_B\n", __func__); }

    if (rs->vfo_list & RIG_VFO_C) { rig_debug(RIG_DEBUG_VERBOSE, "%s: rig has VFO_C\n", __func__); }

    if (rs->vfo_list & RIG_VFO_SUB_A) { rig_debug(RIG_DEBUG_VERBOSE, "%s: rig has VFO_SUB_A\n", __func__); }

    if (rs->vfo_list & RIG_VFO_SUB_B) { rig_debug(RIG_DEBUG_VERBOSE, "%s: rig has VFO_SUB_B\n", __func__); }

    if (rs->vfo_list & RIG_VFO_MAIN_A) { rig_debug(RIG_DEBUG_VERBOSE, "%s: rig has VFO_MAIN_A\n", __func__); }

    if (rs->vfo_list & RIG_VFO_MAIN_B) { rig_debug(RIG_DEBUG_VERBOSE, "%s: rig has VFO_MAIN_B\n", __func__); }

    if (rs->vfo_list & RIG_VFO_SUB) { rig_debug(RIG_DEBUG_VERBOSE, "%s: rig has VFO_SUB\n", __func__); }

    if (rs->vfo_list & RIG_VFO_MAIN) { rig_debug(RIG_DEBUG_VERBOSE, "%s: rig has VFO_MAIN\n", __func__); }

    if (rs->vfo_list & RIG_VFO_MEM) { rig_debug(RIG_DEBUG_VERBOSE, "%s: rig has VFO_MEM\n", __func__); }

    memcpy(rs->preamp, caps->preamp, sizeof(int)*HAMLIB_MAXDBLSTSIZ);
    memcpy(rs->attenuator, caps->attenuator, sizeof(int)*HAMLIB_MAXDBLSTSIZ);
    memcpy(rs->tuning_steps, caps->tuning_steps,
           sizeof(struct tuning_step_list)*HAMLIB_TSLSTSIZ);
    memcpy(rs->filters, caps->filters,
           sizeof(struct filter_list)*HAMLIB_FLTLSTSIZ);
    memcpy(&rs->str_cal, &caps->str_cal,
           sizeof(cal_table_t));

    memcpy(rs->chan_list, caps->chan_list, sizeof(chan_t)*HAMLIB_CHANLSTSIZ);

    rs->has_get_func = caps->has_get_func;
    rs->has_set_func = caps->has_set_func;
    rs->has_get_level = caps->has_get_level;
    rs->has_set_level = caps->has_set_level;
    rs->has_get_parm = caps->has_get_parm;
    rs->has_set_parm = caps->has_set_parm;

    /* emulation by frontend */
    if ((caps->has_get_level & RIG_LEVEL_STRENGTH) == 0
            && (caps->has_get_level & RIG_LEVEL_RAWSTR) == RIG_LEVEL_RAWSTR)
    {
        rs->has_get_level |= RIG_LEVEL_STRENGTH;
    }

    memcpy(rs->level_gran, caps->level_gran, sizeof(gran_t)*RIG_SETTING_MAX);
    memcpy(rs->parm_gran, caps->parm_gran, sizeof(gran_t)*RIG_SETTING_MAX);

    rs->max_rit = caps->max_rit;
    rs->max_xit = caps->max_xit;
    rs->max_ifshift = caps->max_ifshift;
    rs->announces = caps->announces;

    rs->rigport.fd = rs->pttport.fd = rs->dcdport.fd = -1;
    rs->powerstat = RIG_POWER_ON; // default to power on

    /*
     * let the backend a chance to setup his private data
     * This must be done only once defaults are setup,
     * so the backend init can override rig_state.
     */
    if (caps->rig_init != NULL)
    {
        int retcode = caps->rig_init(rig);

        if (retcode != RIG_OK)
        {
            rig_debug(RIG_DEBUG_VERBOSE,
                      "%s: backend_init failed!\n",
                      __func__);
            /* cleanup and exit */
            free(rig);
            return (NULL);
        }
    }

    return (rig);
}


/**
 * \brief open the communication to the rig
 * \param rig   The #RIG handle of the radio to be opened
 *
 * Opens communication to a radio which \a RIG handle has been passed
 * by argument.
 *
 * \return RIG_OK if the operation has been successful, otherwise
 * a negative value if an error occurred (in which case, cause is
 * set appropriately).
 *
 * \retval RIG_EINVAL   \a rig is NULL or inconsistent.
 * \retval RIG_ENIMPL   port type communication is not implemented yet.
 *
 * \sa rig_init(), rig_close()
 */
int HAMLIB_API rig_open(RIG *rig)
{
    const struct rig_caps *caps;
    struct rig_state *rs;
    int status = RIG_OK;
    value_t parm_value;
    //unsigned int net1, net2, net3, net4, net5, net6, net7, net8, port;
    int is_network = 0;

    ENTERFUNC;

    if (!rig || !rig->caps)
    {
        RETURNFUNC(-RIG_EINVAL);
    }

    caps = rig->caps;
    rs = &rig->state;
    rs->rigport.rig = rig;
    rs->rigport_deprecated.rig = rig;

    if (strcmp(rs->rigport.pathname, "USB") == 0)
    {
        rig_debug(RIG_DEBUG_ERR, "%s: 'USB' is not a valid COM port name\n", __func__);
        errno = 2;
        RETURNFUNC(-RIG_EINVAL);
    }

    // rigctl/rigctld may have deprecated values -- backwards compatility
    if (rs->rigport_deprecated.pathname[0] != 0)
    {
        strcpy(rs->rigport.pathname, rs->rigport_deprecated.pathname);
    }

    if (rs->pttport_deprecated.type.ptt != RIG_PTT_NONE)
    {
        rs->pttport.type.ptt = rs->pttport_deprecated.type.ptt;
    }

    if (rs->dcdport_deprecated.type.dcd != RIG_DCD_NONE)
    {
        rs->dcdport.type.dcd = rs->dcdport_deprecated.type.dcd;
    }

    if (rs->pttport_deprecated.pathname[0] != 0)
    {
        strcpy(rs->pttport.pathname, rs->pttport_deprecated.pathname);
    }

    if (rs->dcdport_deprecated.pathname[0] != 0)
    {
        strcpy(rs->dcdport.pathname, rs->dcdport_deprecated.pathname);
    }

    rig_settings_load_all(NULL); // load default .hamlib_settings
    // Read in our settings
    char *cwd = calloc(1, 4096);

    if (getcwd(cwd, 4096) == NULL)
    {
        rig_debug(RIG_DEBUG_ERR, "%s: getcwd: %s\n", __func__, strerror(errno));
    }
    else
    {
        rig_debug(RIG_DEBUG_VERBOSE, "%s: cwd=%s\n", __func__, cwd);
        char *path = calloc(1, 4096);
        extern char *settings_file;
        char *xdgpath = getenv("XDG_CONFIG_HOME");

        settings_file = "hamlib_settings";

        if (xdgpath)
        {
            sprintf(path, "%s/%s/%s", xdgpath, cwd, settings_file);
        }
        else
        {
            sprintf(path, "%s/%s", cwd, settings_file);
        }

        FILE *fp = fopen(path, "r");

        if (fp == NULL)
        {
            rig_debug(RIG_DEBUG_VERBOSE, "%s: %s does not exist\n", __func__, path);
        }
        else
        {
            rig_debug(RIG_DEBUG_VERBOSE, "%s: reading settings from %s\n", __func__, path);
        }

        free(path);
    }

    free(cwd);

    // Enable async data only if it's enabled through conf settings *and* supported by the backend
    rig_debug(RIG_DEBUG_TRACE,
              "%s: async_data_enable=%d, async_data_supported=%d\n", __func__,
              rs->async_data_enabled, caps->async_data_supported);
    rs->async_data_enabled = rs->async_data_enabled && caps->async_data_supported;
    rs->rigport.asyncio = rs->async_data_enabled;

    if (strlen(rs->rigport.pathname) > 0)
    {
        char hoststr[256], portstr[6];
        status = parse_hoststr(rs->rigport.pathname, sizeof(rs->rigport.pathname),
                               hoststr, portstr);

        if (status == RIG_OK) { is_network = 1; }
    }

#if 0
    // determine if we have a network address
    //
    is_network |= sscanf(rs->rigport.pathname, "%u.%u.%u.%u:%u", &net1, &net2,
                         &net3, &net4, &port) == 5;
    is_network |= sscanf(rs->rigport.pathname, ":%u", &port) == 1;
    is_network |= sscanf(rs->rigport.pathname, "%u::%u:%u:%u:%u:%u", &net1, &net2,
                         &net3, &net4, &net5, &port) == 6;
    is_network |= sscanf(rs->rigport.pathname, "%u:%u:%u:%u:%u:%u:%u:%u:%u", &net1,
                         &net2, &net3, &net4, &net5, &net6, &net7, &net8, &port) == 9;

    // if we haven't met one of the condition above then we must have a hostname
    if (!is_network && (token = strtok_r(rs->rigport.pathname, ":", &strtokp)))
    {
        rig_debug(RIG_DEBUG_TRACE, "%s: token1=%s\n", __func__, token);
        token = strtok_r(strtokp, ":", &strtokp);

        if (token)
        {
            rig_debug(RIG_DEBUG_TRACE, "%s: token2=%s\n",  __func__, token);

            if (sscanf(token, "%u", &port)) { is_network |= 1; }
        }
    }

#endif

    if (is_network)
    {
        rig_debug(RIG_DEBUG_TRACE, "%s: using network address %s\n", __func__,
                  rs->rigport.pathname);
        rs->rigport.type.rig = RIG_PORT_NETWORK;

        if (RIG_BACKEND_NUM(rig->caps->rig_model) == RIG_ICOM)
        {
            // Xiegu X6100 does TCP and does not support UDP spectrum that I know of
            if (rig->caps->rig_model != RIG_MODEL_X6100)
            {
                rig_debug(RIG_DEBUG_TRACE, "%s(%d): Icom rig UDP network enabled\n", __FILE__,
                          __LINE__);
                rs->rigport.type.rig = RIG_PORT_UDP_NETWORK;
            }

        }
    }

    if (rs->comm_state)
    {
        rig_debug(RIG_DEBUG_VERBOSE, "%s(%d): %p rs->comm_state==1?=%d\n", __func__,
                  __LINE__, &rs->comm_state,
                  rs->comm_state);
        port_close(&rs->rigport, rs->rigport.type.rig);
        rs->comm_state = 0;
        RETURNFUNC(-RIG_EINVAL);
    }

    rs->rigport.fd = -1;

    if (rs->rigport.type.rig == RIG_PORT_SERIAL)
    {
        if (rs->rigport.parm.serial.rts_state != RIG_SIGNAL_UNSET
                && rs->rigport.parm.serial.handshake == RIG_HANDSHAKE_HARDWARE)
        {
            rig_debug(RIG_DEBUG_ERR,
                      "%s: cannot set RTS with hardware handshake \"%s\"\n",
                      __func__,
                      rs->rigport.pathname);
            RETURNFUNC(-RIG_ECONF);
        }

        if ('\0' == rs->pttport.pathname[0]
                || !strcmp(rs->pttport.pathname, rs->rigport.pathname))
        {
            /* check for control line conflicts */
            if (rs->rigport.parm.serial.rts_state != RIG_SIGNAL_UNSET
                    && rs->pttport.type.ptt == RIG_PTT_SERIAL_RTS)
            {
                rig_debug(RIG_DEBUG_ERR,
                          "%s: cannot set RTS with PTT by RTS \"%s\"\n",
                          __func__,
                          rs->rigport.pathname);
                RETURNFUNC(-RIG_ECONF);
            }

            if (rs->rigport.parm.serial.dtr_state != RIG_SIGNAL_UNSET
                    && rs->pttport.type.ptt == RIG_PTT_SERIAL_DTR)
            {
                rig_debug(RIG_DEBUG_ERR,
                          "%s: cannot set DTR with PTT by DTR \"%s\"\n",
                          __func__,
                          rs->rigport.pathname);
                RETURNFUNC(-RIG_ECONF);
            }
        }
    }

    status = port_open(&rs->rigport);

    if (status < 0)
    {
        rig_debug(RIG_DEBUG_VERBOSE, "%s: rs->comm_state==0?=%d\n", __func__,
                  rs->comm_state);
        rs->comm_state = 0;
        RETURNFUNC(status);
    }

    switch (rs->pttport.type.ptt)
    {
    case RIG_PTT_NONE:
    case RIG_PTT_RIG:
    case RIG_PTT_RIG_MICDATA:
        break;

    case RIG_PTT_SERIAL_RTS:
    case RIG_PTT_SERIAL_DTR:
        if (rs->pttport.pathname[0] == '\0'
                && rs->rigport.type.rig == RIG_PORT_SERIAL)
        {
            strcpy(rs->pttport.pathname, rs->rigport.pathname);
        }

        if (!strcmp(rs->pttport.pathname, rs->rigport.pathname))
        {
            rs->pttport.fd = rs->rigport.fd;

            /* Needed on Linux because the serial port driver sets RTS/DTR
               on open - only need to address the PTT line as we offer
               config parameters to control the other (dtr_state &
               rts_state) */
            if (rs->pttport.type.ptt == RIG_PTT_SERIAL_DTR)
            {
                status = ser_set_dtr(&rs->pttport, 0);
            }

            if (rs->pttport.type.ptt == RIG_PTT_SERIAL_RTS)
            {
                status = ser_set_rts(&rs->pttport, 0);
            }
        }
        else
        {
            rs->pttport.fd = ser_open(&rs->pttport);

            if (rs->pttport.fd < 0)
            {
                rig_debug(RIG_DEBUG_ERR,
                          "%s: cannot open PTT device \"%s\"\n",
                          __func__,
                          rs->pttport.pathname);
                status = -RIG_EIO;
            }

            if (RIG_OK == status
                    && (rs->pttport.type.ptt == RIG_PTT_SERIAL_DTR
                        || rs->pttport.type.ptt == RIG_PTT_SERIAL_RTS))
            {
                /* Needed on Linux because the serial port driver sets
                   RTS/DTR high on open - set both low since we offer no
                   control of the non-PTT line and low is better than
                   high */
                status = ser_set_dtr(&rs->pttport, 0);

                if (RIG_OK == status)
                {
                    status = ser_set_rts(&rs->pttport, 0);
                }
            }

            ser_close(&rs->pttport);
        }

        break;

    case RIG_PTT_PARALLEL:
        rs->pttport.fd = par_open(&rs->pttport);

        if (rs->pttport.fd < 0)
        {
            rig_debug(RIG_DEBUG_ERR,
                      "%s: cannot open PTT device \"%s\"\n",
                      __func__,
                      rs->pttport.pathname);
            status = -RIG_EIO;
        }
        else
        {
            par_ptt_set(&rs->pttport, RIG_PTT_OFF);
        }

        break;

    case RIG_PTT_CM108:
        rs->pttport.fd = cm108_open(&rs->pttport);

        strncpy(rs->rigport.pathname, DEFAULT_CM108_PORT, HAMLIB_FILPATHLEN);
        rs->rigport.parm.cm108.ptt_bitnum = DEFAULT_CM108_PTT_BITNUM;
        rs->pttport.parm.cm108.ptt_bitnum = DEFAULT_CM108_PTT_BITNUM;

        if (rs->pttport.fd < 0)
        {
            rig_debug(RIG_DEBUG_ERR,
                      "%s: cannot open PTT device \"%s\"\n",
                      __func__,
                      rs->pttport.pathname);
            status = -RIG_EIO;
        }
        else
        {
            cm108_ptt_set(&rs->pttport, RIG_PTT_OFF);
        }

        break;

    case RIG_PTT_GPIO:
    case RIG_PTT_GPION:
        rs->pttport.fd = gpio_open(&rs->pttport, 1,
                                   RIG_PTT_GPION == rs->pttport.type.ptt ? 0 : 1);

        if (rs->pttport.fd < 0)
        {
            rig_debug(RIG_DEBUG_ERR,
                      "%s: cannot open PTT device \"GPIO%s\"\n",
                      __func__,
                      rs->pttport.pathname);
            status = -RIG_EIO;
        }
        else
        {
            gpio_ptt_set(&rs->pttport, RIG_PTT_OFF);
        }

        break;

    default:
        rig_debug(RIG_DEBUG_ERR,
                  "%s: unsupported PTT type %d\n",
                  __func__,
                  rs->pttport.type.ptt);
        status = -RIG_ECONF;
    }

    switch (rs->dcdport.type.dcd)
    {
    case RIG_DCD_NONE:
    case RIG_DCD_RIG:
        break;

    case RIG_DCD_SERIAL_DSR:
    case RIG_DCD_SERIAL_CTS:
    case RIG_DCD_SERIAL_CAR:
        if (rs->dcdport.pathname[0] == '\0'
                && rs->rigport.type.rig == RIG_PORT_SERIAL)
        {
            strcpy(rs->dcdport.pathname, rs->rigport.pathname);
        }

        if (strcmp(rs->dcdport.pathname, rs->rigport.pathname) == 0)
        {
            rs->dcdport.fd = rs->rigport.fd;
        }
        else
        {
            rs->dcdport.fd = ser_open(&rs->dcdport);
        }

        if (rs->dcdport.fd < 0)
        {
            rig_debug(RIG_DEBUG_ERR,
                      "%s: cannot open DCD device \"%s\"\n",
                      __func__,
                      rs->dcdport.pathname);
            status = -RIG_EIO;
        }

        break;

    case RIG_DCD_PARALLEL:
        rs->dcdport.fd = par_open(&rs->dcdport);

        if (rs->dcdport.fd < 0)
        {
            rig_debug(RIG_DEBUG_ERR,
                      "%s: cannot open DCD device \"%s\"\n",
                      __func__,
                      rs->dcdport.pathname);
            status = -RIG_EIO;
        }

        break;

    case RIG_DCD_GPIO:
    case RIG_DCD_GPION:
        rs->dcdport.fd = gpio_open(&rs->dcdport, 0,
                                   RIG_DCD_GPION == rs->dcdport.type.dcd ? 0 : 1);

        if (rs->dcdport.fd < 0)
        {
            rig_debug(RIG_DEBUG_ERR,
                      "%s: cannot open DCD device \"GPIO%s\"\n",
                      __func__,
                      rs->dcdport.pathname);
            status = -RIG_EIO;
        }

        break;

    default:
        rig_debug(RIG_DEBUG_ERR,
                  "%s: unsupported DCD type %d\n",
                  __func__,
                  rs->dcdport.type.dcd);
        status = -RIG_ECONF;
    }

    if (status < 0)
    {
        port_close(&rs->rigport, rs->rigport.type.rig);
        RETURNFUNC(status);
    }

    status = async_data_handler_start(rig);

    if (status < 0)
    {
        port_close(&rs->rigport, rs->rigport.type.rig);
        RETURNFUNC(status);
    }

    add_opened_rig(rig);

    rs->comm_state = 1;
    rig_debug(RIG_DEBUG_VERBOSE, "%s: %p rs->comm_state==1?=%d\n", __func__,
              &rs->comm_state,
              rs->comm_state);

    /*
     * Maybe the backend has something to initialize
     * In case of failure, just close down and report error code.
     */
    int retry_save = rs->rigport.retry;
    rs->rigport.retry = 1;

    if (caps->rig_open != NULL)
    {
        status = caps->rig_open(rig);

        if (status != RIG_OK)
        {
            remove_opened_rig(rig);
            async_data_handler_stop(rig);
            port_close(&rs->rigport, rs->rigport.type.rig);
            memcpy(&rs->rigport_deprecated, &rs->rigport, sizeof(hamlib_port_t_deprecated));
            rs->comm_state = 0;
            RETURNFUNC(status);
        }
    }

    /*
     * trigger state->current_vfo first retrieval
     */
    HAMLIB_TRACE;

    if (caps->get_vfo && rig_get_vfo(rig, &rs->current_vfo) == RIG_OK)
    {
        rs->tx_vfo = rs->current_vfo;
    }
    else // no get_vfo so set some sensible defaults
    {
        //int backend_num = RIG_BACKEND_NUM(rig->caps->rig_model);
        rs->tx_vfo = RIG_VFO_TX;

        // If we haven't gotten the vfo by now we will default to VFO_CURR
        if (rs->current_vfo == RIG_VFO_NONE) { rs->current_vfo = RIG_VFO_CURR; }

        rig_debug(RIG_DEBUG_TRACE, "%s: vfo_curr=%s, tx_vfo=%s\n", __func__,
                  rig_strvfo(rs->current_vfo), rig_strvfo(rs->tx_vfo));

#if 0 // done in the back end

        if (backend_num == RIG_ICOM)
        {
            HAMLIB_TRACE;
            rig_set_vfo(rig, RIG_VFO_A); // force VFOA as our startup VFO
            rig_debug(RIG_DEBUG_TRACE, "%s: Icom rig so default vfo = %s\n", __func__,
                      rig_strvfo(rs->current_vfo));
        }

#endif

        if (rig->caps->set_vfo == NULL)
        {
            // for non-Icom rigs if there's no set_vfo then we need to set one
            rs->current_vfo = vfo_fixup(rig, RIG_VFO_A, rig->state.cache.split);
            rig_debug(RIG_DEBUG_TRACE, "%s: No set_vfo function rig so default vfo = %s\n",
                      __func__, rig_strvfo(rs->current_vfo));
        }
        else
        {
            rig_debug(RIG_DEBUG_TRACE, "%s: default vfo = %s\n", __func__,
                      rig_strvfo(rs->current_vfo));
        }
    }

    if (rs->auto_disable_screensaver)
    {
        // try to turn off the screensaver if possible
        // don't care about the return here...it's just a nice-to-have
        parm_value.i = 0;
        HAMLIB_TRACE;
        rig_set_parm(rig, RIG_PARM_SCREENSAVER, parm_value);
    }

    // read frequency to update internal status
//    freq_t freq;
//    if (caps->get_freq) rig_get_freq(rig, RIG_VFO_A, &freq);
//    if (caps->get_freq) rig_get_freq(rig, RIG_VFO_B, &freq);

    // prime the freq and mode settings
    // don't care about the return here -- if it doesn't work so be it
    freq_t freq;

    if (rig->caps->get_freq)
    {
        int retval = rig_get_freq(rig, RIG_VFO_A, &freq);

        if (retval == RIG_OK && rig->caps->rig_model != RIG_MODEL_F6K)
        {
            split_t split = RIG_SPLIT_OFF;
            vfo_t tx_vfo = RIG_VFO_NONE;
            rig_get_freq(rig, RIG_VFO_B, &freq);
            rig_get_split_vfo(rig, RIG_VFO_RX, &split, &tx_vfo);
            rig_debug(RIG_DEBUG_VERBOSE, "%s(%d): Current split=%d, tx_vfo=%s\n", __func__,
                      __LINE__, split, rig_strvfo(tx_vfo));
            rmode_t mode;
            pbwidth_t width = 2400; // we'll use 2400Hz as default width

            if (rig->caps->get_mode)
            {
                rig_get_mode(rig, RIG_VFO_A, &mode, &width);

                if (split)
                {
                    rig_debug(RIG_DEBUG_VERBOSE, "xxxsplit=%d\n", split);
                    HAMLIB_TRACE;
                    rig_get_mode(rig, RIG_VFO_B, &mode, &width);
                }
            }
        }
    }

    rs->rigport.retry = retry_save;

    memcpy(&rs->rigport_deprecated, &rs->rigport, sizeof(hamlib_port_t_deprecated));
    memcpy(&rs->pttport_deprecated, &rs->pttport, sizeof(hamlib_port_t_deprecated));
    memcpy(&rs->dcdport_deprecated, &rs->dcdport, sizeof(hamlib_port_t_deprecated));
    RETURNFUNC(RIG_OK);
}


/**
 * \brief close the communication to the rig
 * \param rig   The #RIG handle of the radio to be closed
 *
 * Closes communication to a radio which \a RIG handle has been passed
 * by argument that was previously open with rig_open().
 *
 * \return RIG_OK if the operation has been successful, otherwise
 * a negative value if an error occurred (in which case, cause is
 * set appropriately).
 *
 * \sa rig_cleanup(), rig_open()
 */
int HAMLIB_API rig_close(RIG *rig)
{
    const struct rig_caps *caps;
    struct rig_state *rs;

    ENTERFUNC;

    if (!rig || !rig->caps)
    {
        RETURNFUNC(-RIG_EINVAL);
    }

    caps = rig->caps;
    rs = &rig->state;

    if (!rs->comm_state)
    {
        RETURNFUNC(-RIG_EINVAL);
    }

    /*
     * Let the backend say 73s to the rig.
     * and ignore the return code.
     */
    if (caps->rig_close)
    {
        caps->rig_close(rig);
    }

    async_data_handler_stop(rig);

    /*
     * FIXME: what happens if PTT and rig ports are the same?
     *          (eg. ptt_type = RIG_PTT_SERIAL)
     */
    switch (rs->pttport.type.ptt)
    {
    case RIG_PTT_NONE:
    case RIG_PTT_RIG:
    case RIG_PTT_RIG_MICDATA:
        break;

    case RIG_PTT_SERIAL_RTS:

        // If port is already closed, do nothing
        if (rs->pttport.fd > -1)
        {
            ser_set_rts(&rs->pttport, 0);

            if (rs->pttport.fd != rs->rigport.fd)
            {
                port_close(&rs->pttport, RIG_PORT_SERIAL);
                memcpy(&rs->rigport_deprecated, &rs->rigport, sizeof(hamlib_port_t_deprecated));
            }
        }

        break;

    case RIG_PTT_SERIAL_DTR:

        // If port is already closed, do nothing
        if (rs->pttport.fd > -1)
        {
            ser_set_dtr(&rs->pttport, 0);

            if (rs->pttport.fd != rs->rigport.fd)
            {
                port_close(&rs->pttport, RIG_PORT_SERIAL);
                memcpy(&rs->rigport_deprecated, &rs->rigport, sizeof(hamlib_port_t_deprecated));
            }
        }

        break;

    case RIG_PTT_PARALLEL:
        par_ptt_set(&rs->pttport, RIG_PTT_OFF);
        par_close(&rs->pttport);
        break;

    case RIG_PTT_CM108:
        cm108_ptt_set(&rs->pttport, RIG_PTT_OFF);
        cm108_close(&rs->pttport);
        break;

    case RIG_PTT_GPIO:
    case RIG_PTT_GPION:
        gpio_ptt_set(&rs->pttport, RIG_PTT_OFF);
        gpio_close(&rs->pttport);
        break;

    default:
        rig_debug(RIG_DEBUG_ERR,
                  "%s: unsupported PTT type %d\n",
                  __func__,
                  rs->pttport.type.ptt);
    }

    switch (rs->dcdport.type.dcd)
    {
    case RIG_DCD_NONE:
    case RIG_DCD_RIG:
        break;

    case RIG_DCD_SERIAL_DSR:
    case RIG_DCD_SERIAL_CTS:
    case RIG_DCD_SERIAL_CAR:
        if (rs->dcdport.fd != rs->rigport.fd)
        {
            port_close(&rs->dcdport, RIG_PORT_SERIAL);
            memcpy(&rs->rigport_deprecated, &rs->rigport, sizeof(hamlib_port_t_deprecated));
        }

        break;

    case RIG_DCD_PARALLEL:
        par_close(&rs->dcdport);
        break;

    case RIG_DCD_GPIO:
    case RIG_DCD_GPION:
        gpio_close(&rs->dcdport);
        break;

    default:
        rig_debug(RIG_DEBUG_ERR,
                  "%s: unsupported DCD type %d\n",
                  __func__,
                  rs->dcdport.type.dcd);
    }

    rs->dcdport.fd = rs->pttport.fd = -1;

    port_close(&rs->rigport, rs->rigport.type.rig);

    remove_opened_rig(rig);

    // zero split so it will allow it to be set again on open for rigctld
    rig->state.cache.split = 0;
    rs->comm_state = 0;
    rig_debug(RIG_DEBUG_VERBOSE, "%s(%d): %p rs->comm_state==0?=%d\n", __func__,
              __LINE__, &rs->comm_state,
              rs->comm_state);

    RETURNFUNC(RIG_OK);
}


/**
 * \brief release a rig handle and free associated memory
 * \param rig   The #RIG handle of the radio to be closed
 *
 * Releases a rig struct which port has eventually been closed already
 * with rig_close().
 *
 * \return RIG_OK if the operation has been successful, otherwise
 * a negative value if an error occurred (in which case, cause is
 * set appropriately).
 *
 * \sa rig_init(), rig_close()
 */
int HAMLIB_API rig_cleanup(RIG *rig)
{
    if (!rig || !rig->caps)
    {
        return (-RIG_EINVAL);
    }

    /*
     * check if they forgot to close the rig
     */
    if (rig->state.comm_state)
    {
        rig_close(rig);
    }

    /*
     * basically free up the priv struct
     */
    if (rig->caps->rig_cleanup)
    {
        rig->caps->rig_cleanup(rig);
    }

    free(rig);

    return (RIG_OK);
}

/**
 * \brief timeout (secs) to stop rigctld when VFO is manually changed
 * \param rig   The rig handle
 * \param seconds    The timeout to set to
 *
 * timeout seconds to stop rigctld when VFO is manually changed
 * turns on/off the radio.
 *
 * \return RIG_OK if the operation has been successful, otherwise
 * a negative value if an error occurred (in which case, cause is
 * set appropriately).
 *
 * \sa rig_set_twiddle()
 */
int HAMLIB_API rig_set_twiddle(RIG *rig, int seconds)
{
    ENTERFUNC;

    if (CHECK_RIG_ARG(rig))
    {
        RETURNFUNC(-RIG_EINVAL);
    }

    rig->state.twiddle_timeout = seconds;

    RETURNFUNC(RIG_OK);
}

/**
 * \brief For GPredict to avoid reading frequency on uplink VFO
 * \param rig   The rig handle
 * \param seconds    1=Ignore Sub, 2=Ignore Main
 *
 * \return RIG_OK if the operation has been successful, otherwise
 * a negative value if an error occurred (in which case, cause is
 * set appropriately).
 *
 * \sa rig_set_uplink()
 */
int HAMLIB_API rig_set_uplink(RIG *rig, int val)
{
    ENTERFUNC;

    if (CHECK_RIG_ARG(rig))
    {
        RETURNFUNC(-RIG_EINVAL);
    }

    rig->state.uplink = val;

    RETURNFUNC(RIG_OK);
}


/**
 * \brief get the twiddle timeout value (secs)
 * \param rig   The rig handle
 * \param seconds    The timeout value
 *
 * \return RIG_OK if the operation has been successful, otherwise
 * a negative value if an error occurred (in which case, cause is
 * set appropriately).
 *
 * \sa rig_set_twiddle()
 */
int HAMLIB_API rig_get_twiddle(RIG *rig, int *seconds)
{
    ENTERFUNC;

    if (CHECK_RIG_ARG(rig) || !seconds)
    {
        RETURNFUNC(-RIG_EINVAL);
    }

    *seconds = rig->state.twiddle_timeout;
    RETURNFUNC(RIG_OK);
}

// detect if somebody is twiddling the VFO
// indicator is last set freq doesn't match current freq
// so we have to query freq every time we set freq or vfo to handle this
static int twiddling(RIG *rig)
{
    const struct rig_caps *caps;

    if (rig->state.twiddle_timeout == 0) { return 0; } // don't detect twiddling

    caps = rig->caps;

    if (caps->get_freq)    // gotta have get_freq of course
    {
        freq_t curr_freq = 0;
        int retval2;
        int elapsed;

        HAMLIB_TRACE;
        retval2 = caps->get_freq(rig, RIG_VFO_CURR, &curr_freq);

        if (retval2 == RIG_OK && rig->state.current_freq != curr_freq)
        {
            rig_debug(RIG_DEBUG_TRACE,
                      "%s: Somebody twiddling the VFO? last_freq=%.0f, curr_freq=%.0f\n", __func__,
                      rig->state.current_freq, curr_freq);

            if (rig->state.current_freq == 0)
            {
                rig->state.current_freq = curr_freq;
                RETURNFUNC2(0); // not twiddling as first time freq is being set
            }

            rig->state.twiddle_time = time(NULL); // update last twiddle time
            rig->state.current_freq = curr_freq; // we have a new freq to remember
            rig_set_cache_freq(rig, RIG_VFO_CURR, curr_freq);
        }

        elapsed = time(NULL) - rig->state.twiddle_time;

        if (elapsed < rig->state.twiddle_timeout)
        {
            rig_debug(RIG_DEBUG_TRACE, "%s: Twiddle elapsed < %d, elapsed=%d\n", __func__,
                      rig->state.twiddle_timeout, elapsed);
            rig->state.twiddle_state = TWIDDLE_ON; // gets turned off in rig_set_freq;
            RETURNFUNC(1); // would be better as error but other software won't handle it
        }
    }

    RETURNFUNC2(0);
}

/**
 * \brief set the frequency of the target VFO
 * \param rig   The rig handle
 * \param vfo   The target VFO
 * \param freq  The frequency to set to
 *
 * Sets the frequency of the target VFO.
 *
 * \return RIG_OK if the operation has been successful, otherwise
 * a negative value if an error occurred (in which case, cause is
 * set appropriately).
 *
 * \sa rig_get_freq()
 */
#if BUILTINFUNC
#undef rig_set_freq
int HAMLIB_API rig_set_freq(RIG *rig, vfo_t vfo, freq_t freq, const char *func)
#else
int HAMLIB_API rig_set_freq(RIG *rig, vfo_t vfo, freq_t freq)
#endif
{
    const struct rig_caps *caps;
    int retcode;
    freq_t freq_new = freq;
    vfo_t vfo_save;

    ELAPSED1;
#if BUILTINFUNC
    rig_debug(RIG_DEBUG_VERBOSE, "%s called vfo=%s, freq=%.0f, called from %s\n",
              __func__,
              rig_strvfo(vfo), freq, func);
#else
    rig_debug(RIG_DEBUG_VERBOSE, "%s called vfo=%s, freq=%.0f\n", __func__,
              rig_strvfo(vfo), freq);
#endif

    if (CHECK_RIG_ARG(rig))
    {
        ELAPSED2;
        RETURNFUNC2(-RIG_EINVAL);
    }

    if (rig->state.twiddle_state == TWIDDLE_ON)
    {
        // we keep skipping set_freq while the vfo knob is in motion
        rig_debug(RIG_DEBUG_VERBOSE,
                  "%s: Twiddle on so skipping this set_freq request one time\n", __func__);
        rig->state.twiddle_state = TWIDDLE_OFF;
    }

    caps = rig->caps;

    if (rig->state.lo_freq != 0.0)
    {
        freq -= rig->state.lo_freq;
    }


    if (rig->state.vfo_comp != 0.0)
    {
        freq += (freq_t)((double)rig->state.vfo_comp * freq);
    }

    if (caps->set_freq == NULL)
    {
        ELAPSED2;
        RETURNFUNC2(-RIG_ENAVAIL);
    }

    vfo_save = rig->state.current_vfo;
    vfo = vfo_fixup(rig, vfo, rig->state.cache.split);

    if ((caps->targetable_vfo & RIG_TARGETABLE_FREQ)
            || vfo == RIG_VFO_CURR || vfo == rig->state.current_vfo)
    {
        if (twiddling(rig))
        {
            rig_debug(RIG_DEBUG_TRACE, "%s: Ignoring set_freq due to VFO twiddling\n",
                      __func__);

            if (vfo != vfo_save && vfo != RIG_VFO_CURR)
            {
                HAMLIB_TRACE;
                rig_set_vfo(rig, vfo_save);
            }

            ELAPSED2;
            RETURNFUNC2(
                RIG_OK); // would be better as error but other software won't handle errors
        }

        rig_debug(RIG_DEBUG_TRACE, "%s: TARGETABLE_FREQ vfo=%s\n", __func__,
                  rig_strvfo(vfo));
        int retry = 3;
        freq_t tfreq = 0;

        do
        {
            HAMLIB_TRACE;
            retcode = caps->set_freq(rig, vfo, freq);

            // some rig will return -RIG_ENTARGET if cannot set ptt while transmitting
            // we will just return RIG_OK and the frequency set will be ignored
            if (retcode == -RIG_ENTARGET) { RETURNFUNC(RIG_OK); }

            if (retcode != RIG_OK) { RETURNFUNC(retcode); }

            // Unidirectional rigs do not reset cache
            if (rig->caps->rig_model != RIG_MODEL_FT736R)
            {
                rig_set_cache_freq(rig, vfo, (freq_t)0);
            }

#if 0 // this verification seems to be causing bad behavior on some rigs

            if (caps->get_freq)
            {
                retcode = rig_get_freq(rig, vfo, &tfreq);

                // WSJT-X does a 55Hz check so we can stop early if that's the case
                if ((long long)freq % 100 == 55) { retry = 0; }

                if (retcode != RIG_OK) { RETURNFUNC(retcode); }

                if (tfreq != freq)
                {
                    hl_usleep(50 * 1000);
                    rig_debug(RIG_DEBUG_WARN,
                              "%s: freq not set correctly?? got %.0f asked for %.0f, retry=%d\n", __func__,
                              (double)tfreq, (double)freq, retry);
                }
            }
            else { retry = 0; }

#else
            tfreq = freq;
#endif
        }
        while (tfreq != freq && retry-- > 0);

        if (retry == 0 && tfreq != freq)
        {
            rig_debug(RIG_DEBUG_ERR,
                      "%s: unable to set frequency!!, asked for %.0f, got %.0f\n", __func__, freq,
                      tfreq);
        }
    }
    else
    {
        rig_debug(RIG_DEBUG_TRACE, "%s: not a TARGETABLE_FREQ vfo=%s\n", __func__,
                  rig_strvfo(vfo));

        if (!caps->set_vfo)
        {
            ELAPSED2;
            RETURNFUNC2(-RIG_ENAVAIL);
        }

        retcode = rig_set_vfo(rig, vfo);

        if (retcode != RIG_OK)
        {
            rig_debug(RIG_DEBUG_ERR, "%s: set_vfo failed: %s\n", __func__,
                      rigerror(retcode));
        }


        if (twiddling(rig))
        {
            rig_debug(RIG_DEBUG_TRACE, "%s: Ignoring set_freq due to VFO twiddling\n",
                      __func__);

            if (vfo != vfo_save && vfo != RIG_VFO_CURR)
            {
                HAMLIB_TRACE;
                rig_set_vfo(rig, vfo_save);
            }

            ELAPSED2;
            RETURNFUNC2(
                RIG_OK); // would be better as error but other software won't handle errors
        }

        HAMLIB_TRACE;
        retcode = caps->set_freq(rig, vfo, freq);
    }

    if (retcode == RIG_OK && caps->get_freq != NULL)
    {

        // verify our freq to ensure HZ mods are seen
        // some rigs truncate or round e.g. 1,2,5,10,20,100Hz intervals
        // we'll try this all the time and if it works out OK eliminate the #else

        if ((unsigned long long)freq % 100 != 0 // only need to do if < 100Hz interval
                || freq > 100e6  // or if we are in the VHF and up range
#if 0
                // do we need to only do this when cache is turned on? 2020-07-02 W9MDB
                && rig->state.cache.timeout_ms > 0
#endif
           )
        {
            // Unidirectional rigs do not reset cache
            if (rig->caps->rig_model != RIG_MODEL_FT736R)
            {
                rig_set_cache_freq(rig, RIG_VFO_ALL, (freq_t)0);
            }

            HAMLIB_TRACE;
            retcode = rig_get_freq(rig, vfo, &freq_new);

            if (retcode != RIG_OK)
            {
                ELAPSED2;
                RETURNFUNC(retcode);
            }
        }

        if (freq_new != freq)
        {
            rig_debug(RIG_DEBUG_TRACE, "%s: Asked freq=%.0f, got freq=%.0f\n", __func__,
                      freq,
                      freq_new);
        }

    }

    // update our current freq too
    if (vfo == RIG_VFO_CURR || vfo == rig->state.current_vfo) { rig->state.current_freq = freq_new; }

    rig_set_cache_freq(rig, vfo, freq_new);

    if (vfo != RIG_VFO_CURR)
    {
        HAMLIB_TRACE;
        rig_set_vfo(rig, vfo_save);
    }

    ELAPSED2;
    RETURNFUNC2(retcode);
}


/**
 * \brief get the frequency of the target VFO
 * \param rig   The rig handle
 * \param vfo   The target VFO
 * \param freq  The location where to store the current frequency
 *
 *  Retrieves the frequency of the target VFO.
 *  The value stored at \a freq location equals RIG_FREQ_NONE when the current
 *  frequency of the VFO is not defined (e.g. blank memory).
 *
 * \return RIG_OK if the operation has been successful, otherwise
 * a negative value if an error occurred (in which case, cause is
 * set appropriately).
 *
 * \sa rig_set_freq()
 */
int HAMLIB_API rig_get_freq(RIG *rig, vfo_t vfo, freq_t *freq)
{
    const struct rig_caps *caps;
    int retcode;
    vfo_t curr_vfo;
    rmode_t mode;
    pbwidth_t width;

    if (CHECK_RIG_ARG(rig))
    {
        RETURNFUNC2(-RIG_EINVAL);
    }

    ELAPSED1;

    if (!freq)
    {
        rig_debug(RIG_DEBUG_TRACE, "%s: freq ptr invalid\n", __func__);
        RETURNFUNC2(-RIG_EINVAL);
    }

    rig_debug(RIG_DEBUG_VERBOSE, "%s(%d) called vfo=%s\n", __func__, __LINE__,
              rig_strvfo(vfo));
    rig_cache_show(rig, __func__, __LINE__);


    curr_vfo = rig->state.current_vfo; // save vfo for restore later

    vfo = vfo_fixup(rig, vfo, rig->state.cache.split);

    rig_debug(RIG_DEBUG_VERBOSE, "%s(%d) vfo=%s, curr_vfo=%s\n", __FILE__, __LINE__,
              rig_strvfo(vfo), rig_strvfo(curr_vfo));

    if (vfo == RIG_VFO_CURR) { vfo = curr_vfo; }

    // we ignore get_freq for the uplink VFO for gpredict to behave better
    if ((rig->state.uplink == 1 && vfo == RIG_VFO_SUB)
            || (rig->state.uplink == 2 && vfo == RIG_VFO_MAIN))
    {
        rig_debug(RIG_DEBUG_TRACE, "%s: uplink=%d, ignoring get_freq\n", __func__,
                  rig->state.uplink);
        rig_debug(RIG_DEBUG_TRACE, "%s: split=%d, satmode=%d, tx_vfo=%s\n", __func__,
                  rig->state.cache.split, rig->state.cache.satmode,
                  rig_strvfo(rig->state.tx_vfo));
        // always return the cached freq for this clause
        int cache_ms_freq, cache_ms_mode, cache_ms_width;
        rig_get_cache(rig, vfo, freq, &cache_ms_freq, &mode, &cache_ms_mode, &width,
                      &cache_ms_width);
        ELAPSED2;
        return (RIG_OK);
    }

    rig_cache_show(rig, __func__, __LINE__);

    // there are some rigs that can't get VFOA freq while VFOB is transmitting
    // so we'll return the cached VFOA freq for them
    // should we use the cached ptt maybe? No -- we have to be 100% sure we're in PTT to ignore this request
    if ((vfo == RIG_VFO_A || vfo == RIG_VFO_MAIN) && rig->state.cache.split &&
            (rig->caps->rig_model == RIG_MODEL_FTDX101D
             || rig->caps->rig_model == RIG_MODEL_IC910))
    {
        // if we're in PTT don't get VFOA freq -- otherwise we interrupt transmission
        ptt_t ptt;
        HAMLIB_TRACE;
        retcode = rig_get_ptt(rig, RIG_VFO_CURR, &ptt);

        if (retcode != RIG_OK)
        {
            ELAPSED2;
            RETURNFUNC2(retcode);
        }

        if (ptt)
        {
            rig_debug(RIG_DEBUG_TRACE,
                      "%s: split is on so returning VFOA last known freq\n",
                      __func__);
            *freq = rig->state.cache.freqMainA;
            ELAPSED2;
            return (RIG_OK);
        }
    }

    int cache_ms_freq, cache_ms_mode, cache_ms_width;
    rig_get_cache(rig, vfo, freq, &cache_ms_freq, &mode, &cache_ms_mode, &width,
                  &cache_ms_width);
    //rig_debug(RIG_DEBUG_TRACE, "%s: cache check1 age=%dms\n", __func__, cache_ms_freq);

    rig_cache_show(rig, __func__, __LINE__);

    if (*freq != 0 && (cache_ms_freq < rig->state.cache.timeout_ms
                       || (rig->state.cache.timeout_ms == HAMLIB_CACHE_ALWAYS
                           || rig->state.use_cached_freq)))
    {
        rig_debug(RIG_DEBUG_TRACE,
                  "%s: %s cache hit age=%dms, freq=%.0f, use_cached_freq=%d\n", __func__,
                  rig_strvfo(vfo), cache_ms_freq, *freq, rig->state.use_cached_freq);
        ELAPSED2;
        return (RIG_OK);
    }
    else
    {
        rig_debug(RIG_DEBUG_TRACE,
                  "%s: cache miss age=%dms, cached_vfo=%s, asked_vfo=%s, use_cached_freq=%d\n",
                  __func__,
                  cache_ms_freq,
                  rig_strvfo(vfo), rig_strvfo(vfo), rig->state.use_cached_freq);
    }

    caps = rig->caps;

    if (caps->get_freq == NULL)
    {
        ELAPSED2;
        RETURNFUNC2(-RIG_ENAVAIL);
    }

    rig_debug(RIG_DEBUG_VERBOSE, "%s(%d): vfo_opt=%d, model=%d\n", __func__,
              __LINE__, rig->state.vfo_opt, rig->caps->rig_model);

    // If we're in vfo_mode then rigctld will do any VFO swapping we need
    if ((caps->targetable_vfo & RIG_TARGETABLE_FREQ)
            || vfo == RIG_VFO_CURR || vfo == rig->state.current_vfo
            || (rig->state.vfo_opt == 1 && rig->caps->rig_model == RIG_MODEL_NETRIGCTL))
    {
        // If rig does not have set_vfo we need to change vfo
        if (vfo == RIG_VFO_CURR && caps->set_vfo == NULL)
        {
            vfo = vfo_fixup(rig, RIG_VFO_A, rig->state.cache.split);
            rig_debug(RIG_DEBUG_TRACE, "%s: no set_vfo so vfo=%s\n", __func__,
                      rig_strvfo(vfo));
        }

        retcode = caps->get_freq(rig, vfo, freq);

        rig_cache_show(rig, __func__, __LINE__);

        // sometimes a network rig like FLRig will return freq=0
        // so we'll just reuse the cache for that condition
        if (*freq == 0)
        {
            int freq_ms, mode_ms, width_ms;
            rig_get_cache(rig, vfo, freq, &freq_ms, &mode, &mode_ms, &width, &width_ms);
        }

        if (retcode == RIG_OK)
        {
            rig_set_cache_freq(rig, vfo, *freq);
            rig_cache_show(rig, __func__, __LINE__);
        }
    }
    else
    {
        int rc2;

        if (!caps->set_vfo)
        {
            ELAPSED2;
            RETURNFUNC2(-RIG_ENAVAIL);
        }

        HAMLIB_TRACE;
        retcode = caps->set_vfo(rig, vfo);

        if (retcode != RIG_OK)
        {
            ELAPSED2;
            RETURNFUNC2(retcode);
        }

        rig_cache_show(rig, __func__, __LINE__);

        HAMLIB_TRACE;
        retcode = caps->get_freq(rig, vfo, freq);
        /* try and revert even if we had an error above */
        rc2 = RIG_OK;

        if (curr_vfo != RIG_VFO_NONE)
        {
            rc2 = caps->set_vfo(rig, curr_vfo);
        }

        if (RIG_OK == retcode)
        {
            rig_cache_show(rig, __func__, __LINE__);
            rig_set_cache_freq(rig, vfo, *freq);
            rig_cache_show(rig, __func__, __LINE__);
            /* return the first error code */
            retcode = rc2;
        }
    }

    /* VFO compensation */
    if (rig->state.vfo_comp != 0.0)
    {
        *freq = (freq_t)(*freq / (1.0 + (double)rig->state.vfo_comp));
    }

    if (retcode == RIG_OK
            && (vfo == RIG_VFO_CURR || vfo == rig->state.current_vfo))
    {
        rig->state.current_freq = *freq;
    }

    if (rig->state.lo_freq != 0.0)
    {
        *freq += rig->state.lo_freq;
    }

    if (retcode == RIG_OK)
    {
        rig_cache_show(rig, __func__, __LINE__);
    }

    rig_set_cache_freq(rig, vfo, *freq);

    if (retcode == RIG_OK)
    {
        rig_cache_show(rig, __func__, __LINE__);
    }

    ELAPSED2;
    return (retcode);
}

/**
 * \brief get the frequency of VFOA and VFOB
 * \param rig   The rig handle
 * \param freqA  The location where to store the VFOA/Main frequency
 * \param freqB  The location where to store the VFOB/Sub frequency
 *
 *  Retrieves the frequency of  VFOA/Main and VFOB/Sub
 *  The value stored at \a freq location equals RIG_FREQ_NONE when the current
 *  frequency of the VFO is not defined (e.g. blank memory).
 *
 * \return RIG_OK if the operation has been successful, otherwise
 * a negative value if an error occurred (in which case, cause is
 * set appropriately).
 *
 * \sa rig_set_freq()
 */
int HAMLIB_API rig_get_freqs(RIG *rig, freq_t *freqA, freq_t freqB)
{
    // we will attempt to avoid vfo swapping in this routine

    return (-RIG_ENIMPL);

}


/**
 * \brief set the mode of the target VFO
 * \param rig   The rig handle
 * \param vfo   The target VFO
 * \param mode  The mode to set to
 * \param width The passband width to set to
 *
 * Sets the mode and associated passband of the target VFO.  The
 * passband \a width must be supported by the backend of the rig or
 * the special value RIG_PASSBAND_NOCHANGE which leaves the passband
 * unchanged from the current value or default for the mode determined
 * by the rig.
 *
 * \return RIG_OK if the operation has been successful, otherwise
 * a negative value if an error occurred (in which case, cause is
 * set appropriately).
 *
 * \sa rig_get_mode()
 */
int HAMLIB_API rig_set_mode(RIG *rig, vfo_t vfo, rmode_t mode, pbwidth_t width)
{
    const struct rig_caps *caps;
    int retcode;
    int locked_mode;

    ELAPSED1;

    rig_debug(RIG_DEBUG_VERBOSE,
              "%s called, vfo=%s, mode=%s, width=%d, curr_vfo=%s\n", __func__,
              rig_strvfo(vfo), rig_strrmode(mode), (int)width,
              rig_strvfo(rig->state.current_vfo));

    if (CHECK_RIG_ARG(rig))
    {
        ELAPSED2;
        RETURNFUNC2(-RIG_EINVAL);
    }

    rig_get_lock_mode(rig, &locked_mode);

    if (locked_mode)
    {
        ELAPSED2;
        return (RIG_OK);
    }

    // do not mess with mode while PTT is on
    if (rig->state.cache.ptt)
    {
        rig_debug(RIG_DEBUG_VERBOSE, "%s PTT on so set_mode ignored\n", __func__);
        ELAPSED2;
        return RIG_OK;
    }

    caps = rig->caps;

    if (caps->set_mode == NULL)
    {
        ELAPSED2;
        RETURNFUNC2(-RIG_ENAVAIL);
    }

    if (vfo == RIG_VFO_CURR)
    {
        vfo = rig->state.current_vfo;
    }

    if (mode == RIG_MODE_NONE) // the we just use the current mode to set width
    {
        pbwidth_t twidth;
        rig_get_mode(rig, vfo, &mode, &twidth);
    }

    vfo = vfo_fixup(rig, vfo, rig->state.cache.split);

    // if we're not asking for bandwidth and the mode is already set we don't need to do it
    // this will prevent flashing on some rigs like the TS-870
    if (caps->get_mode && width == RIG_PASSBAND_NOCHANGE)
    {
        rmode_t mode_curr;
        pbwidth_t width_curr;
        retcode = caps->get_mode(rig, vfo, &mode_curr, &width_curr);

        if (retcode == RIG_OK && mode == mode_curr)
        {
            rig_debug(RIG_DEBUG_VERBOSE,
                      "%s: mode already %s and bw change not requested\n", __func__,
                      rig_strrmode(mode));
            ELAPSED2;
            RETURNFUNC2(RIG_OK);
        }
    }

    if ((caps->targetable_vfo & RIG_TARGETABLE_MODE)
            || vfo == rig->state.current_vfo)
    {
        HAMLIB_TRACE;
        retcode = caps->set_mode(rig, vfo, mode, width);
        rig_debug(RIG_DEBUG_TRACE, "%s: targetable retcode after set_mode(%s)=%d\n",
                  __func__, rig_strrmode(mode), retcode);
    }
    else
    {
        int rc2;
        vfo_t curr_vfo;

        // if not a targetable rig we will only set mode on VFOB if it is changing
        if (rig->state.cache.modeMainB == mode)
        {
            rig_debug(RIG_DEBUG_TRACE, "%s: VFOB mode not changing so ignoring\n",
                      __func__);
            ELAPSED2;
            RETURNFUNC2(RIG_OK);
        }

        rig_debug(RIG_DEBUG_TRACE, "%s: not targetable need vfo swap\n", __func__);

        if (!caps->set_vfo)
        {
            ELAPSED2;
            RETURNFUNC2(-RIG_ENAVAIL);
        }

        curr_vfo = rig->state.current_vfo;
        rig_debug(RIG_DEBUG_VERBOSE, "%s(%d): curr_vfo=%s, vfo=%s\n", __func__,
                  __LINE__, rig_strvfo(curr_vfo), rig_strvfo(vfo));
        HAMLIB_TRACE;
        retcode = caps->set_vfo(rig, vfo);

        if (retcode != RIG_OK)
        {
            ELAPSED2;
            RETURNFUNC2(retcode);
        }

        retcode = caps->set_mode(rig, vfo, mode, width);
        /* try and revert even if we had an error above */
        rc2 = caps->set_vfo(rig, curr_vfo);

        /* return the first error code */
        if (RIG_OK == retcode)
        {
            retcode = rc2;
        }
    }

    if (retcode != RIG_OK)
    {
        rig_debug(RIG_DEBUG_TRACE, "%s: failed set_mode(%s)=%s\n",
                  __func__, rig_strrmode(mode), rigerror(retcode));
        ELAPSED2;
        RETURNFUNC(retcode);
    }

    rig_set_cache_mode(rig, vfo, mode, width);

    ELAPSED2;
    RETURNFUNC2(retcode);
}

/*
 * \brief get the mode of the target VFO
 * \param rig   The rig handle
 * \param vfo   The target VFO
 * \param mode  The location where to store the current mode
 * \param width The location where to store the current passband width
 *
 *  Retrieves the mode and passband of the target VFO.
 *  If the backend is unable to determine the width, the \a width
 *  will be set to RIG_PASSBAND_NORMAL as a default.
 *  The value stored at \a mode location equals RIG_MODE_NONE when the current
 *  mode of the VFO is not defined (e.g. blank memory).
 *
 *  Note that if either \a mode or \a width is NULL, -RIG_EINVAL is returned.
 *  Both must be given even if only one is actually wanted.
 *
 * \return RIG_OK if the operation has been successful, otherwise
 * a negative value if an error occurred (in which case, cause is
 * set appropriately).
 *
 * \sa rig_set_mode()
 */
int HAMLIB_API rig_get_mode(RIG *rig,
                            vfo_t vfo,
                            rmode_t *mode,
                            pbwidth_t *width)
{
    const struct rig_caps *caps;
    int retcode;
    freq_t freq;

    ENTERFUNC;
    ELAPSED1;

    if (CHECK_RIG_ARG(rig))
    {
        ELAPSED2;
        RETURNFUNC(-RIG_EINVAL);
    }

    if (!mode || !width)
    {
        ELAPSED2;
        RETURNFUNC(-RIG_EINVAL);
    }

    caps = rig->caps;

    if (caps->get_mode == NULL)
    {
        ELAPSED2;
        RETURNFUNC(-RIG_ENAVAIL);
    }

    vfo = vfo_fixup(rig, vfo, rig->state.cache.split);

    *mode = RIG_MODE_NONE;
    rig_cache_show(rig, __func__, __LINE__);
    int cache_ms_freq, cache_ms_mode, cache_ms_width;
    rig_get_cache(rig, vfo, &freq, &cache_ms_freq, mode, &cache_ms_mode, width,
                  &cache_ms_width);
    rig_debug(RIG_DEBUG_TRACE, "%s: %s cache check age=%dms\n", __func__,
              rig_strvfo(vfo), cache_ms_mode);

    rig_cache_show(rig, __func__, __LINE__);

    if (rig->state.cache.timeout_ms == HAMLIB_CACHE_ALWAYS
            || rig->state.use_cached_mode)
    {
        rig_debug(RIG_DEBUG_TRACE, "%s: cache hit age mode=%dms, width=%dms\n",
                  __func__, cache_ms_mode, cache_ms_width);

        ELAPSED2;
        RETURNFUNC(RIG_OK);
    }

    if (vfo == RIG_VFO_B && !(caps->targetable_vfo & RIG_TARGETABLE_MODE))
    {
        *mode = rig->state.cache.modeMainA;
        *width = rig->state.cache.widthMainA;
        return RIG_OK;
    }

    if ((*mode != RIG_MODE_NONE && cache_ms_mode < rig->state.cache.timeout_ms)
            && cache_ms_width < rig->state.cache.timeout_ms)
    {
        rig_debug(RIG_DEBUG_TRACE, "%s: cache hit age mode=%dms, width=%dms\n",
                  __func__, cache_ms_mode, cache_ms_width);

        ELAPSED2;
        RETURNFUNC(RIG_OK);
    }
    else
    {
        rig_debug(RIG_DEBUG_TRACE, "%s: cache miss age mode=%dms, width=%dms\n",
                  __func__, cache_ms_mode, cache_ms_width);
    }

    if ((caps->targetable_vfo & RIG_TARGETABLE_MODE)
            || vfo == RIG_VFO_CURR
            || vfo == rig->state.current_vfo)
    {
        HAMLIB_TRACE;
        retcode = caps->get_mode(rig, vfo, mode, width);
        rig_debug(RIG_DEBUG_TRACE, "%s: retcode after get_mode=%d\n", __func__,
                  retcode);
        rig_cache_show(rig, __func__, __LINE__);
    }
    else
    {
        int rc2;
        vfo_t curr_vfo;

        if (!caps->set_vfo)
        {
            ELAPSED2;
            RETURNFUNC(-RIG_ENAVAIL);
        }

        curr_vfo = rig->state.current_vfo;
        rig_debug(RIG_DEBUG_TRACE, "%s(%d): vfo=%s, curr_vfo=%s\n", __func__, __LINE__,
                  rig_strvfo(vfo), rig_strvfo(curr_vfo));
        HAMLIB_TRACE;
        retcode = caps->set_vfo(rig, vfo == RIG_VFO_CURR ? RIG_VFO_A : vfo);

        rig_cache_show(rig, __func__, __LINE__);

        if (retcode != RIG_OK)
        {
            ELAPSED2;
            RETURNFUNC(retcode);
        }

        HAMLIB_TRACE;
        retcode = caps->get_mode(rig, vfo, mode, width);
        /* try and revert even if we had an error above */
        rc2 = caps->set_vfo(rig, curr_vfo);

        if (RIG_OK == retcode)
        {
            /* return the first error code */
            retcode = rc2;
        }
    }

    if (retcode == RIG_OK
            && (vfo == RIG_VFO_CURR || vfo == rig->state.current_vfo))
    {
        rig_debug(RIG_DEBUG_TRACE, "%s(%d): debug\n", __func__, __LINE__);
        rig->state.current_mode = *mode;
        rig->state.current_width = *width;
        rig_cache_show(rig, __func__, __LINE__);
    }

    if (*width == RIG_PASSBAND_NORMAL && *mode != RIG_MODE_NONE)
    {
        rig_debug(RIG_DEBUG_TRACE, "%s(%d): debug\n", __func__, __LINE__);
        *width = rig_passband_normal(rig, *mode);
    }

    rig_set_cache_mode(rig, vfo, *mode, *width);
    rig_cache_show(rig, __func__, __LINE__);

    ELAPSED2;
    RETURNFUNC(retcode);
}


/**
 * \brief get the normal passband of a mode
 * \param rig   The rig handle
 * \param mode  The mode to get the passband
 *
 *  Returns the normal (default) passband for the given \a mode.
 *
 * \return the passband in Hz if the operation has been successful,
 * or a 0 if an error occurred (passband not found, whatever).
 *
 * \sa rig_passband_narrow(), rig_passband_wide()
 */
pbwidth_t HAMLIB_API rig_passband_normal(RIG *rig, rmode_t mode)
{
    const struct rig_state *rs;
    int i;

    ENTERFUNC;

    if (!rig)
    {
        RETURNFUNC(RIG_PASSBAND_NORMAL);    /* huhu! */
    }

    rs = &rig->state;

    // return CW for CWR and RTTY for RTTYR
    if (mode == RIG_MODE_CWR) { mode = RIG_MODE_CW; }

    if (mode == RIG_MODE_RTTYR) { mode = RIG_MODE_RTTY; }

    for (i = 0; i < HAMLIB_FLTLSTSIZ && rs->filters[i].modes; i++)
    {
        if (rs->filters[i].modes & mode)
        {
            rig_debug(RIG_DEBUG_VERBOSE, "%.*s%d:%s: return filter#%d, width=%d\n",
                      rig->state.depth, spaces(), rig->state.depth, __func__, i,
                      (int)rs->filters[i].width);
            RETURNFUNC(rs->filters[i].width);
        }
    }

    rig_debug(RIG_DEBUG_VERBOSE,
              "%s: filter not found...return %d\n", __func__,
              0);
    RETURNFUNC(0);
}


/**
 * \brief get the narrow passband of a mode
 * \param rig   The rig handle
 * \param mode  The mode to get the passband
 *
 *  Returns the narrow (closest) passband for the given \a mode.
 *  EXAMPLE: rig_set_mode(my_rig, RIG_MODE_LSB,
 *                          rig_passband_narrow(my_rig, RIG_MODE_LSB) );
 *
 * \return the passband in Hz if the operation has been successful,
 * or a 0 if an error occurred (passband not found, whatever).
 *
 * \sa rig_passband_normal(), rig_passband_wide()
 */
pbwidth_t HAMLIB_API rig_passband_narrow(RIG *rig, rmode_t mode)
{
    const struct rig_state *rs;
    pbwidth_t normal;
    int i;

    ENTERFUNC;

    if (!rig)
    {
        RETURNFUNC(0);   /* huhu! */
    }

    rs = &rig->state;

    for (i = 0; i < HAMLIB_FLTLSTSIZ - 1 && rs->filters[i].modes; i++)
    {
        if (rs->filters[i].modes & mode)
        {
            normal = rs->filters[i].width;

            for (i++; i < HAMLIB_FLTLSTSIZ && rs->filters[i].modes; i++)
            {
                if ((rs->filters[i].modes & mode) &&
                        (rs->filters[i].width < normal))
                {
                    RETURNFUNC(rs->filters[i].width);
                }
            }

            RETURNFUNC(0);
        }
    }

    RETURNFUNC(0);
}


/**
 * \brief get the wide passband of a mode
 * \param rig   The rig handle
 * \param mode  The mode to get the passband
 *
 *  Returns the wide (default) passband for the given \a mode.
 *  EXAMPLE: rig_set_mode(my_rig, RIG_MODE_AM,
 *                          rig_passband_wide(my_rig, RIG_MODE_AM) );
 *
 * \return the passband in Hz if the operation has been successful,
 * or a 0 if an error occurred (passband not found, whatever).
 *
 * \sa rig_passband_narrow(), rig_passband_normal()
 */
pbwidth_t HAMLIB_API rig_passband_wide(RIG *rig, rmode_t mode)
{
    const struct rig_state *rs;
    pbwidth_t normal;
    int i;

    ENTERFUNC;

    if (!rig)
    {
        RETURNFUNC(0);   /* huhu! */
    }

    rs = &rig->state;

    for (i = 0; i < HAMLIB_FLTLSTSIZ - 1 && rs->filters[i].modes; i++)
    {
        if (rs->filters[i].modes & mode)
        {
            normal = rs->filters[i].width;

            for (i++; i < HAMLIB_FLTLSTSIZ && rs->filters[i].modes; i++)
            {
                if ((rs->filters[i].modes & mode) &&
                        (rs->filters[i].width > normal))
                {
                    RETURNFUNC(rs->filters[i].width);
                }
            }

            RETURNFUNC(0);
        }
    }

    RETURNFUNC(0);
}


/**
 * \brief set the current VFO
 * \param rig   The rig handle
 * \param vfo   The VFO to set to
 *
 *  Sets the current VFO. The VFO can be RIG_VFO_A, RIG_VFO_B, RIG_VFO_C
 *  for VFOA, VFOB, VFOC respectively or RIG_VFO_MEM for Memory mode.
 *  Supported VFOs depends on rig capabilities.
 *
 * \return RIG_OK if the operation has been successful, otherwise
 * a negative value if an error occurred (in which case, cause is
 * set appropriately).
 *
 * \sa rig_get_vfo()
 */
#if BUILTINFUNC
#undef rig_set_vfo
int HAMLIB_API rig_set_vfo(RIG *rig, vfo_t vfo, const char *func)
#else
int HAMLIB_API rig_set_vfo(RIG *rig, vfo_t vfo)
#endif
{
    const struct rig_caps *caps;
    int retcode;
    freq_t curr_freq;
    vfo_t curr_vfo = RIG_VFO_CURR, tmp_vfo;

    ELAPSED1;
    ENTERFUNC;
#if BUILTINFUNC
    rig_debug(RIG_DEBUG_VERBOSE, "%s called vfo=%s, called from %s\n", __func__,
              rig_strvfo(vfo), func);
#else
    rig_debug(RIG_DEBUG_VERBOSE, "%s called vfo=%s\n", __func__, rig_strvfo(vfo));
#endif

    if (vfo == RIG_VFO_B || vfo == RIG_VFO_SUB)
    {
        rig_debug(RIG_DEBUG_VERBOSE, "%s ********************** called vfo=%s\n",
                  __func__, rig_strvfo(vfo));
    }

    if (CHECK_RIG_ARG(rig))
    {
        ELAPSED2;
        RETURNFUNC(-RIG_EINVAL);
    }

    vfo = vfo_fixup(rig, vfo, rig->state.cache.split);

    if (vfo == RIG_VFO_CURR)
    {
        ELAPSED2;
        RETURNFUNC(RIG_OK);
    }

    if (rig->caps->get_vfo)
    {
        retcode = rig_get_vfo(rig, &curr_vfo);

        if (retcode != RIG_OK)
        {
            rig_debug(RIG_DEBUG_WARN, "%s: rig_get_vfo error=%s\n", __func__,
                      rigerror(retcode));
        }

        if (curr_vfo == vfo) { RETURNFUNC(RIG_OK); }
    }

#if 0 // removing this check 20210801 -- should be mapped already

    // make sure we are asking for a VFO that the rig actually has
    if ((vfo == RIG_VFO_A || vfo == RIG_VFO_B) && !VFO_HAS_A_B)
    {
        rig_debug(RIG_DEBUG_ERR, "%s: rig does not have %s\n", __func__,
                  rig_strvfo(vfo));
        ELAPSED2;
        RETURNFUNC(-RIG_EINVAL);
    }

    if ((vfo == RIG_VFO_MAIN || vfo == RIG_VFO_SUB) && !VFO_HAS_MAIN_SUB)
    {
        rig_debug(RIG_DEBUG_ERR, "%s: rig does not have %s\n", __func__,
                  rig_strvfo(vfo));
        ELAPSED2;
        RETURNFUNC(-RIG_EINVAL);
    }

#endif

    vfo = vfo_fixup(rig, vfo, rig->state.cache.split);

    caps = rig->caps;

    if (caps->set_vfo == NULL)
    {
        ELAPSED2;
        RETURNFUNC(-RIG_ENAVAIL);
    }

    if (twiddling(rig))
    {
        rig_debug(RIG_DEBUG_TRACE, "%s: Ignoring set_vfo due to VFO twiddling\n",
                  __func__);
        ELAPSED2;
        RETURNFUNC(
            RIG_OK); // would be better as error but other software won't handle errors
    }

    HAMLIB_TRACE;
    vfo_t vfo_save = rig->state.current_vfo;

    if (vfo != RIG_VFO_CURR) { rig->state.current_vfo = vfo; }

    retcode = caps->set_vfo(rig, vfo);

    if (retcode == RIG_OK)
    {
        vfo = rig->state.current_vfo; // vfo may change in the rig backend
        rig->state.cache.vfo = vfo;
        elapsed_ms(&rig->state.cache.time_vfo, HAMLIB_ELAPSED_SET);
        rig_debug(RIG_DEBUG_TRACE, "%s: rig->state.current_vfo=%s\n", __func__,
                  rig_strvfo(vfo));
    }
    else
    {
        rig_debug(RIG_DEBUG_ERR, "%s: set_vfo %s failed with '%.10000s'\n", __func__,
                  rig_strvfo(vfo), rigerror(retcode));
        rig->state.current_vfo = vfo_save;
    }

    // we need to update our internal freq to avoid getting detected as twiddling
    // we only get the freq if we set the vfo OK
    if (retcode == RIG_OK && caps->get_freq)
    {
        HAMLIB_TRACE;
        retcode = caps->get_freq(rig, vfo, &curr_freq);
        rig_debug(RIG_DEBUG_TRACE, "%s: retcode from rig_get_freq = %d\n",
                  __func__,
                  retcode);
        rig_set_cache_freq(rig, vfo, curr_freq);
    }
    else
    {
        // if no get_freq clear all cache to be sure we refresh whatever we can
        rig_set_cache_freq(rig, RIG_VFO_ALL, (freq_t)0);
    }

    if (vfo != rig->state.current_vfo && rig_get_vfo(rig, &tmp_vfo) == -RIG_ENAVAIL)
    {
        rig_debug(RIG_DEBUG_TRACE,
                  "%s: Expiring all cache due to VFO change and no get_vfo\n", __func__);
        // expire all cached items when we switch VFOs and get_vfo does not work
        rig_set_cache_freq(rig, RIG_VFO_ALL, 0);
    }

    rig_debug(RIG_DEBUG_TRACE, "%s: return %d, vfo=%s, curr_vfo=%s\n", __func__,
              retcode,
              rig_strvfo(vfo), rig_strvfo(rig->state.current_vfo));
    ELAPSED2;
    RETURNFUNC(retcode);
}


/**
 * \brief get the current VFO
 * \param rig   The rig handle
 * \param vfo   The location where to store the current VFO
 *
 *  Retrieves the current VFO. The VFO can be RIG_VFO_A, RIG_VFO_B, RIG_VFO_C
 *  for VFOA, VFOB, VFOC respectively or RIG_VFO_MEM for Memory mode.
 *  Supported VFOs depends on rig capabilities.
 *
 * \return RIG_OK if the operation has been successful, otherwise
 * a negative value if an error occurred (in which case, cause is
 * set appropriately).
 *
 * \sa rig_set_vfo()
 */
int HAMLIB_API rig_get_vfo(RIG *rig, vfo_t *vfo)
{
    const struct rig_caps *caps;
    int retcode;
    int cache_ms;

    ENTERFUNC;
    ELAPSED1;

    if (CHECK_RIG_ARG(rig) || !vfo)
    {
        ELAPSED2;
        RETURNFUNC(-RIG_EINVAL);
    }

    if (!vfo)
    {
        rig_debug(RIG_DEBUG_ERR, "%s: no vfo?  rig=%p, vfo=%p\n", __func__,
                  rig, vfo);
        ELAPSED2;
        RETURNFUNC(-RIG_EINVAL);
    }

    caps = rig->caps;

    if (caps->get_vfo == NULL)
    {
        rig_debug(RIG_DEBUG_ERR, "%s: no get_vfo\n", __func__);
        ELAPSED2;
        RETURNFUNC(-RIG_ENAVAIL);
    }

    cache_ms = elapsed_ms(&rig->state.cache.time_vfo, HAMLIB_ELAPSED_GET);
    rig_debug(RIG_DEBUG_TRACE, "%s: cache check age=%dms\n", __func__, cache_ms);

    if (cache_ms < rig->state.cache.timeout_ms)
    {
        *vfo = rig->state.cache.vfo;
        rig_debug(RIG_DEBUG_TRACE, "%s: cache hit age=%dms, vfo=%s\n", __func__,
                  cache_ms, rig_strvfo(*vfo));
        ELAPSED2;
        RETURNFUNC(RIG_OK);
    }
    else
    {
        rig_debug(RIG_DEBUG_TRACE, "%s: cache miss age=%dms\n", __func__, cache_ms);
    }

    HAMLIB_TRACE;
    retcode = caps->get_vfo(rig, vfo);

    if (retcode == RIG_OK)
    {
        rig->state.current_vfo = *vfo;
        rig->state.cache.vfo = *vfo;
        cache_ms = elapsed_ms(&rig->state.cache.time_vfo, HAMLIB_ELAPSED_SET);
    }
    else
    {
        cache_ms = elapsed_ms(&rig->state.cache.time_vfo, HAMLIB_ELAPSED_INVALIDATE);
    }

    if (retcode != RIG_OK)
    {
        rig_debug(RIG_DEBUG_ERR, "%s: returning %d(%.10000s)\n", __func__, retcode,
                  rigerror(retcode));
    }

    ELAPSED2;
    RETURNFUNC(retcode);
}


/**
 * \brief set PTT on/off
 * \param rig   The rig handle
 * \param vfo   The target VFO
 * \param ptt   The PTT status to set to
 *
 *  Sets "Push-To-Talk" on/off.
 *
 * \return RIG_OK if the operation has been successful, otherwise
 * a negative value if an error occurred (in which case, cause is
 * set appropriately).
 *
 * \sa rig_get_ptt()
 */
int HAMLIB_API rig_set_ptt(RIG *rig, vfo_t vfo, ptt_t ptt)
{
    const struct rig_caps *caps;
    struct rig_state *rs = &rig->state;
    int retcode = RIG_OK;

    ELAPSED1;
    ENTERFUNC;

    if (CHECK_RIG_ARG(rig))
    {
        ELAPSED2;
        RETURNFUNC(-RIG_EINVAL);
    }

    caps = rig->caps;

    switch (rig->state.pttport.type.ptt)
    {
    case RIG_PTT_RIG:
        if (ptt == RIG_PTT_ON_MIC || ptt == RIG_PTT_ON_DATA)
        {
            ptt = RIG_PTT_ON;
        }

    /* fall through */
    case RIG_PTT_RIG_MICDATA:
        if (caps->set_ptt == NULL)
        {
            ELAPSED2;
            RETURNFUNC(-RIG_ENIMPL);
        }

        if ((caps->targetable_vfo & RIG_TARGETABLE_PTT)
                || vfo == RIG_VFO_CURR
                || vfo == rig->state.current_vfo)
        {
            int retry = 3;
            ptt_t tptt;

            do
            {
                HAMLIB_TRACE;
                retcode = caps->set_ptt(rig, vfo, ptt);

                if (retcode != RIG_OK)
                {
                    ELAPSED2;
                    RETURNFUNC(retcode);
                }

#if 0
                hl_usleep(50 * 1000); // give PTT a chance to do it's thing

                // don't use the cached value and check to see if it worked
                elapsed_ms(&rig->state.cache.time_ptt, HAMLIB_ELAPSED_INVALIDATE);

                tptt = -1;
                // IC-9700 is failing on get_ptt right after set_ptt in split mode
                retcode = rig_get_ptt(rig, vfo, &tptt);

                if (retcode != RIG_OK)
                {
                    rig_debug(RIG_DEBUG_ERR, "%s: rig_get_ptt failed: %s\b", __func__,
                              rigerror(retcode));
                    retcode = RIG_OK; // fake the retcode so we retry
                }

                if (tptt != ptt) { rig_debug(RIG_DEBUG_WARN, "%s: failed, retry=%d\n", __func__, retry); }

#else
                tptt = ptt;
#endif
            }
            while (tptt != ptt && retry-- > 0 && retcode == RIG_OK);
        }
        else
        {
            vfo_t curr_vfo;
            int backend_num;
            int targetable_ptt;

            if (!caps->set_vfo)
            {
                ELAPSED2;
                RETURNFUNC(-RIG_ENAVAIL);
            }

            curr_vfo = rig->state.current_vfo;
            HAMLIB_TRACE;
            backend_num = RIG_BACKEND_NUM(rig->caps->rig_model);

            switch (backend_num)
            {
            // most rigs have only one PTT VFO so we can set that flag here
            case RIG_ICOM:
            case RIG_KENWOOD:
            case RIG_YAESU:
                targetable_ptt = 1;
            }


            if (!targetable_ptt)
            {
                retcode = caps->set_vfo(rig, vfo);
            }

            if (retcode == RIG_OK)
            {
                int rc2;
                int retry = 3;
                ptt_t tptt;

                do
                {
                    HAMLIB_TRACE;
                    retcode = caps->set_ptt(rig, vfo, ptt);

                    if (retcode != RIG_OK)
                    {
                        ELAPSED2;
                        RETURNFUNC(retcode);
                    }

#if 0
                    retcode = rig_get_ptt(rig, vfo, &tptt);

                    if (tptt != ptt) { rig_debug(RIG_DEBUG_WARN, "%s: failed, retry=%d\n", __func__, retry); }

#else
                    tptt = ptt;
#endif
                }
                while (tptt != ptt && retry-- > 0 && retcode == RIG_OK);

                /* try and revert even if we had an error above */
                HAMLIB_TRACE;

                rc2 = RIG_OK;

                if (!targetable_ptt)
                {
                    rc2 = caps->set_vfo(rig, curr_vfo);
                }

                /* return the first error code */
                if (RIG_OK == retcode)
                {
                    retcode = rc2;
                }
            }
        }

        break;

    case RIG_PTT_SERIAL_DTR:

        /* when the PTT port is not the control port we want to free the
           port when PTT is reset and seize the port when PTT is set,
           this allows limited sharing of the PTT port between
           applications so long as there is no contention */
        if (strcmp(rs->pttport.pathname, rs->rigport.pathname)
                && rs->pttport.fd < 0
                && RIG_PTT_OFF != ptt)
        {

            rs->pttport.fd = ser_open(&rs->pttport);

            if (rs->pttport.fd < 0)
            {
                rig_debug(RIG_DEBUG_ERR,
                          "%s: cannot open PTT device \"%s\"\n",
                          __func__,
                          rs->pttport.pathname);
                ELAPSED2;
                RETURNFUNC(-RIG_EIO);
            }

            /* Needed on Linux because the serial port driver sets RTS/DTR
               high on open - set both since we offer no control of
               the non-PTT line and low is better than high */
            retcode = ser_set_rts(&rs->pttport, 0);

            if (RIG_OK != retcode)
            {
                ELAPSED2;
                RETURNFUNC(retcode);
            }
        }

        retcode = ser_set_dtr(&rig->state.pttport, ptt != RIG_PTT_OFF);

        rig_debug(RIG_DEBUG_TRACE, "%s:  rigport=%s, pttport=%s, ptt_share=%d\n",
                  __func__, rs->pttport.pathname, rs->rigport.pathname, rs->ptt_share);

        if (strcmp(rs->pttport.pathname, rs->rigport.pathname)
                && ptt == RIG_PTT_OFF && rs->ptt_share != 0)
        {
            /* free the port */
            ser_close(&rs->pttport);
        }

        break;

    case RIG_PTT_SERIAL_RTS:

        /* when the PTT port is not the control port we want to free the
           port when PTT is reset and seize the port when PTT is set,
           this allows limited sharing of the PTT port between
           applications so long as there is no contention */
        if (strcmp(rs->pttport.pathname, rs->rigport.pathname)
                && rs->pttport.fd < 0
                && RIG_PTT_OFF != ptt)
        {
            rig_debug(RIG_DEBUG_TRACE, "%s: PTT RTS debug#1\n", __func__);

            rs->pttport.fd = ser_open(&rs->pttport);

            if (rs->pttport.fd < 0)
            {
                rig_debug(RIG_DEBUG_ERR,
                          "%s: cannot open PTT device \"%s\"\n",
                          __func__,
                          rs->pttport.pathname);
                ELAPSED2;
                RETURNFUNC(-RIG_EIO);
            }

            /* Needed on Linux because the serial port driver sets RTS/DTR
               high on open - set both since we offer no control of the
               non-PTT line and low is better than high */
            retcode = ser_set_dtr(&rs->pttport, 0);

            if (RIG_OK != retcode)
            {
                rig_debug(RIG_DEBUG_ERR, "%s: ser_set_dtr retcode=%d\n", __func__, retcode);
                ELAPSED2;
                RETURNFUNC(retcode);
            }
        }

        retcode = ser_set_rts(&rig->state.pttport, ptt != RIG_PTT_OFF);

        rig_debug(RIG_DEBUG_TRACE, "%s:  rigport=%s, pttport=%s, ptt_share=%d\n",
                  __func__, rs->pttport.pathname, rs->rigport.pathname, rs->ptt_share);

        if (strcmp(rs->pttport.pathname, rs->rigport.pathname)
                && ptt == RIG_PTT_OFF && rs->ptt_share != 0)
        {
            /* free the port */
            ser_close(&rs->pttport);
        }

        break;

    case RIG_PTT_PARALLEL:
        retcode = par_ptt_set(&rig->state.pttport, ptt);
        break;

    case RIG_PTT_CM108:
        retcode = cm108_ptt_set(&rig->state.pttport, ptt);
        break;

    case RIG_PTT_GPIO:
    case RIG_PTT_GPION:
        retcode = gpio_ptt_set(&rig->state.pttport, ptt);
        break;

    default:
        ELAPSED2;
        RETURNFUNC(-RIG_EINVAL);
    }

    if (RIG_OK == retcode)
    {
        rs->transmit = ptt != RIG_PTT_OFF;
    }

    // some rigs like the FT-2000 with the SCU-17 need just a bit of time to let the relays work
    // can affect fake it mode in WSJT-X when the rig is still in transmit and freq change
    // is requested on a rig that can't change freq on a transmitting VFO
    if (ptt != RIG_PTT_ON) { hl_usleep(50 * 1000); }

    rig->state.cache.ptt = ptt;
    elapsed_ms(&rig->state.cache.time_ptt, HAMLIB_ELAPSED_SET);

    if (retcode != RIG_OK) { rig_debug(RIG_DEBUG_ERR, "%s: return code=%d\n", __func__, retcode); }

    memcpy(&rig->state.pttport_deprecated, &rig->state.pttport,
           sizeof(rig->state.pttport_deprecated));
    ELAPSED2;

    RETURNFUNC(retcode);
}


/**
 * \brief get the status of the PTT
 * \param rig   The rig handle
 * \param vfo   The target VFO
 * \param ptt   The location where to store the status of the PTT
 *
 *  Retrieves the status of PTT (are we on the air?).
 *
 * \return RIG_OK if the operation has been successful, otherwise
 * a negative value if an error occurred (in which case, cause is
 * set appropriately).
 *
 * \sa rig_set_ptt()
 */
int HAMLIB_API rig_get_ptt(RIG *rig, vfo_t vfo, ptt_t *ptt)
{
    const struct rig_caps *caps;
    struct rig_state *rs = &rig->state;
    int retcode = RIG_OK;
    int status;
    vfo_t curr_vfo;
    int cache_ms;
    int targetable_ptt = 0;
    int backend_num;

    ELAPSED1;
    ENTERFUNC;

    if (CHECK_RIG_ARG(rig))
    {
        ELAPSED2;
        RETURNFUNC(-RIG_EINVAL);
    }

    if (!ptt)
    {
        ELAPSED2;
        RETURNFUNC(-RIG_EINVAL);
    }

    cache_ms = elapsed_ms(&rig->state.cache.time_ptt, HAMLIB_ELAPSED_GET);
    rig_debug(RIG_DEBUG_TRACE, "%s: cache check age=%dms\n", __func__, cache_ms);

    if (cache_ms < rig->state.cache.timeout_ms)
    {
        rig_debug(RIG_DEBUG_TRACE, "%s: cache hit age=%dms\n", __func__, cache_ms);
        *ptt = rig->state.cache.ptt;
        ELAPSED2;
        RETURNFUNC(RIG_OK);
    }
    else
    {
        rig_debug(RIG_DEBUG_TRACE, "%s: cache miss age=%dms\n", __func__, cache_ms);
    }

    caps = rig->caps;

    switch (rig->state.pttport.type.ptt)
    {
    case RIG_PTT_RIG:
    case RIG_PTT_RIG_MICDATA:
        if (!caps->get_ptt)
        {
            *ptt = rs->transmit ? RIG_PTT_ON : RIG_PTT_OFF;
            ELAPSED2;
            RETURNFUNC(RIG_OK);
        }

        if ((caps->targetable_vfo & RIG_TARGETABLE_PTT)
                || vfo == RIG_VFO_CURR
                || vfo == rig->state.current_vfo)
        {
            HAMLIB_TRACE;
            retcode = caps->get_ptt(rig, vfo, ptt);

            if (retcode == RIG_OK)
            {
                rig->state.cache.ptt = *ptt;
                elapsed_ms(&rig->state.cache.time_ptt, HAMLIB_ELAPSED_SET);
            }

            ELAPSED2;
            RETURNFUNC(retcode);
        }

        if (!caps->set_vfo)
        {
            ELAPSED2;
            RETURNFUNC(-RIG_ENAVAIL);
        }

        curr_vfo = rig->state.current_vfo;
        HAMLIB_TRACE;
        backend_num = RIG_BACKEND_NUM(rig->caps->rig_model);

        switch (backend_num)
        {
        // most rigs have only one PTT VFO so we can set that flag here
        case 0:
        case RIG_ICOM:
        case RIG_KENWOOD:
        case RIG_YAESU:
            targetable_ptt = 1;
        }

        if (!targetable_ptt)
        {
            retcode = caps->set_vfo(rig, vfo);
        }

        if (retcode != RIG_OK)
        {
            ELAPSED2;
            RETURNFUNC(retcode);
        }

        HAMLIB_TRACE;
        retcode = caps->get_ptt(rig, vfo, ptt);

        /* try and revert even if we had an error above */
        if (!targetable_ptt)
        {
            int rc2 = caps->set_vfo(rig, curr_vfo);

            if (RIG_OK == retcode)
            {
                /* return the first error code */
                retcode = rc2;
                rig->state.cache.ptt = *ptt;
                elapsed_ms(&rig->state.cache.time_ptt, HAMLIB_ELAPSED_SET);
            }
        }

        ELAPSED2;
        RETURNFUNC(retcode);

    case RIG_PTT_SERIAL_RTS:
        if (caps->get_ptt)
        {
            HAMLIB_TRACE;
            retcode = caps->get_ptt(rig, vfo, ptt);

            if (retcode == RIG_OK)
            {
                elapsed_ms(&rig->state.cache.time_ptt, HAMLIB_ELAPSED_SET);
                rig->state.cache.ptt = *ptt;
            }

            ELAPSED2;
            RETURNFUNC(retcode);
        }

        if (strcmp(rs->pttport.pathname, rs->rigport.pathname)
                && rs->pttport.fd < 0)
        {

            /* port is closed so assume PTT off */
            *ptt = RIG_PTT_OFF;
        }
        else
        {
            retcode = ser_get_rts(&rig->state.pttport, &status);
            *ptt = status ? RIG_PTT_ON : RIG_PTT_OFF;
        }

        rig->state.cache.ptt = *ptt;
        elapsed_ms(&rig->state.cache.time_ptt, HAMLIB_ELAPSED_SET);
        ELAPSED2;
        RETURNFUNC(retcode);

    case RIG_PTT_SERIAL_DTR:
        if (caps->get_ptt)
        {
            HAMLIB_TRACE;
            retcode = caps->get_ptt(rig, vfo, ptt);

            if (retcode == RIG_OK)
            {
                elapsed_ms(&rig->state.cache.time_ptt, HAMLIB_ELAPSED_SET);
                rig->state.cache.ptt = *ptt;
            }

            ELAPSED2;
            RETURNFUNC(retcode);
        }

        if (strcmp(rs->pttport.pathname, rs->rigport.pathname)
                && rs->pttport.fd < 0)
        {

            /* port is closed so assume PTT off */
            *ptt = RIG_PTT_OFF;
        }
        else
        {
            retcode = ser_get_dtr(&rig->state.pttport, &status);
            *ptt = status ? RIG_PTT_ON : RIG_PTT_OFF;
        }

        rig->state.cache.ptt = *ptt;
        elapsed_ms(&rig->state.cache.time_ptt, HAMLIB_ELAPSED_SET);
        ELAPSED2;
        RETURNFUNC(retcode);

    case RIG_PTT_PARALLEL:
        if (caps->get_ptt)
        {
            HAMLIB_TRACE;
            retcode = caps->get_ptt(rig, vfo, ptt);

            if (retcode == RIG_OK)
            {
                elapsed_ms(&rig->state.cache.time_ptt, HAMLIB_ELAPSED_SET);
                rig->state.cache.ptt = *ptt;
            }

            ELAPSED2;
            RETURNFUNC(retcode);
        }

        retcode = par_ptt_get(&rig->state.pttport, ptt);

        if (retcode == RIG_OK)
        {
            elapsed_ms(&rig->state.cache.time_ptt, HAMLIB_ELAPSED_SET);
            rig->state.cache.ptt = *ptt;
        }

        ELAPSED2;
        RETURNFUNC(retcode);

    case RIG_PTT_CM108:
        if (caps->get_ptt)
        {
            HAMLIB_TRACE;
            retcode = caps->get_ptt(rig, vfo, ptt);

            if (retcode == RIG_OK)
            {
                elapsed_ms(&rig->state.cache.time_ptt, HAMLIB_ELAPSED_SET);
                rig->state.cache.ptt = *ptt;
            }

            ELAPSED2;
            RETURNFUNC(retcode);
        }

        retcode = cm108_ptt_get(&rig->state.pttport, ptt);

        if (retcode == RIG_OK)
        {
            elapsed_ms(&rig->state.cache.time_ptt, HAMLIB_ELAPSED_SET);
            rig->state.cache.ptt = *ptt;
        }

        ELAPSED2;
        RETURNFUNC(retcode);

    case RIG_PTT_GPIO:
    case RIG_PTT_GPION:
        if (caps->get_ptt)
        {
            HAMLIB_TRACE;
            retcode = caps->get_ptt(rig, vfo, ptt);

            if (retcode == RIG_OK)
            {
                elapsed_ms(&rig->state.cache.time_ptt, HAMLIB_ELAPSED_SET);
                rig->state.cache.ptt = *ptt;
            }

            ELAPSED2;
            RETURNFUNC(retcode);
        }

        elapsed_ms(&rig->state.cache.time_ptt, HAMLIB_ELAPSED_SET);
        retcode = gpio_ptt_get(&rig->state.pttport, ptt);
        ELAPSED2;
        RETURNFUNC(retcode);

    case RIG_PTT_NONE:
        ELAPSED2;
        RETURNFUNC(-RIG_ENAVAIL);    /* not available */

    default:
        ELAPSED2;
        RETURNFUNC(-RIG_EINVAL);
    }

    elapsed_ms(&rig->state.cache.time_ptt, HAMLIB_ELAPSED_SET);
    ELAPSED2;
    RETURNFUNC(RIG_OK);
}


/**
 * \brief get the status of the DCD
 * \param rig   The rig handle
 * \param vfo   The target VFO
 * \param dcd   The location where to store the status of the DCD
 *
 *  Retrieves the status of DCD (is squelch open?).
 *
 * \return RIG_OK if the operation has been successful, otherwise
 * a negative value if an error occurred (in which case, cause is
 * set appropriately).
 *
 */
int HAMLIB_API rig_get_dcd(RIG *rig, vfo_t vfo, dcd_t *dcd)
{
    const struct rig_caps *caps;
    int retcode, rc2, status;
    vfo_t curr_vfo;

    ELAPSED1;
    ENTERFUNC;

    if (CHECK_RIG_ARG(rig))
    {
        ELAPSED2;
        RETURNFUNC(-RIG_EINVAL);
    }

    if (!dcd)
    {
        ELAPSED2;
        RETURNFUNC(-RIG_EINVAL);
    }

    caps = rig->caps;

    switch (rig->state.dcdport.type.dcd)
    {
    case RIG_DCD_RIG:
        if (caps->get_dcd == NULL)
        {
            ELAPSED2;
            RETURNFUNC(-RIG_ENIMPL);
        }

        if (vfo == RIG_VFO_CURR
                || vfo == rig->state.current_vfo)
        {
            HAMLIB_TRACE;
            retcode = caps->get_dcd(rig, vfo, dcd);
            ELAPSED2;
            RETURNFUNC(retcode);
        }

        if (!caps->set_vfo)
        {
            ELAPSED2;
            RETURNFUNC(-RIG_ENAVAIL);
        }

        curr_vfo = rig->state.current_vfo;
        HAMLIB_TRACE;
        retcode = caps->set_vfo(rig, vfo);

        if (retcode != RIG_OK)
        {
            ELAPSED2;
            RETURNFUNC(retcode);
        }

        HAMLIB_TRACE;
        retcode = caps->get_dcd(rig, vfo, dcd);
        /* try and revert even if we had an error above */
        rc2 = caps->set_vfo(rig, curr_vfo);

        if (RIG_OK == retcode)
        {
            /* return the first error code */
            retcode = rc2;
        }

        ELAPSED2;
        RETURNFUNC(retcode);

        break;

    case RIG_DCD_SERIAL_CTS:
        retcode = ser_get_cts(&rig->state.dcdport, &status);
        memcpy(&rig->state.dcdport_deprecated, &rig->state.dcdport,
               sizeof(rig->state.dcdport_deprecated));
        *dcd = status ? RIG_DCD_ON : RIG_DCD_OFF;
        ELAPSED2;
        RETURNFUNC(retcode);

    case RIG_DCD_SERIAL_DSR:
        retcode = ser_get_dsr(&rig->state.dcdport, &status);
        memcpy(&rig->state.dcdport_deprecated, &rig->state.dcdport,
               sizeof(rig->state.dcdport_deprecated));
        *dcd = status ? RIG_DCD_ON : RIG_DCD_OFF;
        ELAPSED2;
        RETURNFUNC(retcode);

    case RIG_DCD_SERIAL_CAR:
        retcode = ser_get_car(&rig->state.dcdport, &status);
        memcpy(&rig->state.dcdport_deprecated, &rig->state.dcdport,
               sizeof(rig->state.dcdport_deprecated));
        *dcd = status ? RIG_DCD_ON : RIG_DCD_OFF;
        ELAPSED2;
        RETURNFUNC(retcode);


    case RIG_DCD_PARALLEL:
        retcode = par_dcd_get(&rig->state.dcdport, dcd);
        memcpy(&rig->state.dcdport_deprecated, &rig->state.dcdport,
               sizeof(rig->state.dcdport_deprecated));
        ELAPSED2;
        RETURNFUNC(retcode);

    case RIG_DCD_GPIO:
    case RIG_DCD_GPION:
        retcode = gpio_dcd_get(&rig->state.dcdport, dcd);
        memcpy(&rig->state.dcdport_deprecated, &rig->state.dcdport,
               sizeof(rig->state.dcdport_deprecated));
        ELAPSED2;
        RETURNFUNC(retcode);

    case RIG_DCD_NONE:
        ELAPSED2;
        RETURNFUNC(-RIG_ENAVAIL);    /* not available */

    default:
        ELAPSED2;
        RETURNFUNC(-RIG_EINVAL);
    }

    ELAPSED2;
    RETURNFUNC(RIG_OK);
}


/**
 * \brief set the repeater shift
 * \param rig   The rig handle
 * \param vfo   The target VFO
 * \param rptr_shift    The repeater shift to set to
 *
 *  Sets the current repeater shift.
 *
 * \return RIG_OK if the operation has been successful, otherwise
 * a negative value if an error occurred (in which case, cause is
 * set appropriately).
 *
 * \sa rig_get_rptr_shift()
 */
int HAMLIB_API rig_set_rptr_shift(RIG *rig, vfo_t vfo, rptr_shift_t rptr_shift)
{
    const struct rig_caps *caps;
    int retcode, rc2;
    vfo_t curr_vfo;

    ELAPSED1;
    ENTERFUNC;

    if (CHECK_RIG_ARG(rig))
    {
        ELAPSED2;
        RETURNFUNC(-RIG_EINVAL);
    }

    caps = rig->caps;

    if (caps->set_rptr_shift == NULL)
    {
        ELAPSED2;
        RETURNFUNC(-RIG_ENAVAIL);
    }

    if (vfo == RIG_VFO_CURR
            || vfo == rig->state.current_vfo)
    {
        HAMLIB_TRACE;
        retcode = caps->set_rptr_shift(rig, vfo, rptr_shift);
        ELAPSED2;
        RETURNFUNC(retcode);
    }

    if (!caps->set_vfo)
    {
        ELAPSED2;
        RETURNFUNC(-RIG_ENAVAIL);
    }

    curr_vfo = rig->state.current_vfo;
    HAMLIB_TRACE;
    retcode = caps->set_vfo(rig, vfo);

    if (retcode != RIG_OK)
    {
        ELAPSED2;
        RETURNFUNC(retcode);
    }

    HAMLIB_TRACE;
    retcode = caps->set_rptr_shift(rig, vfo, rptr_shift);
    /* try and revert even if we had an error above */
    rc2 = caps->set_vfo(rig, curr_vfo);

    if (RIG_OK == retcode)
    {
        /* return the first error code */
        retcode = rc2;
    }

    ELAPSED2;
    RETURNFUNC(retcode);
}


/**
 * \brief get the current repeater shift
 * \param rig   The rig handle
 * \param vfo   The target VFO
 * \param rptr_shift    The location where to store the current repeater shift
 *
 *  Retrieves the current repeater shift.
 *
 * \return RIG_OK if the operation has been successful, otherwise
 * a negative value if an error occurred (in which case, cause is
 * set appropriately).
 *
 * \sa rig_set_rptr_shift()
 */
int HAMLIB_API rig_get_rptr_shift(RIG *rig, vfo_t vfo, rptr_shift_t *rptr_shift)
{
    const struct rig_caps *caps;
    int retcode, rc2;
    vfo_t curr_vfo;

    ELAPSED1;
    ENTERFUNC;

    if (CHECK_RIG_ARG(rig))
    {
        ELAPSED2;
        RETURNFUNC(-RIG_EINVAL);
    }

    if (!rptr_shift)
    {
        ELAPSED2;
        RETURNFUNC(-RIG_EINVAL);
    }

    caps = rig->caps;

    if (caps->get_rptr_shift == NULL)
    {
        ELAPSED2;
        RETURNFUNC(-RIG_ENAVAIL);
    }

    if (vfo == RIG_VFO_CURR
            || vfo == rig->state.current_vfo)
    {
        HAMLIB_TRACE;
        retcode = caps->get_rptr_shift(rig, vfo, rptr_shift);
        ELAPSED2;
        RETURNFUNC(retcode);
    }

    if (!caps->set_vfo)
    {
        ELAPSED2;
        RETURNFUNC(-RIG_ENAVAIL);
    }

    curr_vfo = rig->state.current_vfo;
    HAMLIB_TRACE;
    retcode = caps->set_vfo(rig, vfo);

    if (retcode != RIG_OK)
    {
        ELAPSED2;
        RETURNFUNC(retcode);
    }

    HAMLIB_TRACE;
    retcode =  caps->get_rptr_shift(rig, vfo, rptr_shift);
    /* try and revert even if we had an error above */
    rc2 = caps->set_vfo(rig, curr_vfo);

    if (RIG_OK == retcode)
    {
        /* return the first error code */
        retcode = rc2;
    }

    ELAPSED2;
    RETURNFUNC(retcode);
}


/**
 * \brief set the repeater offset
 * \param rig   The rig handle
 * \param vfo   The target VFO
 * \param rptr_offs The VFO to set to
 *
 *  Sets the current repeater offset.
 *
 * \return RIG_OK if the operation has been successful, otherwise
 * a negative value if an error occurred (in which case, cause is
 * set appropriately).
 *
 * \sa rig_get_rptr_offs()
 */
int HAMLIB_API rig_set_rptr_offs(RIG *rig, vfo_t vfo, shortfreq_t rptr_offs)
{
    const struct rig_caps *caps;
    int retcode, rc2;
    vfo_t curr_vfo;

    ELAPSED1;
    ENTERFUNC;

    if (CHECK_RIG_ARG(rig))
    {
        ELAPSED2;
        RETURNFUNC(-RIG_EINVAL);
    }

    caps = rig->caps;

    if (caps->set_rptr_offs == NULL)
    {
        ELAPSED2;
        RETURNFUNC(-RIG_ENAVAIL);
    }

    if (vfo == RIG_VFO_CURR
            || vfo == rig->state.current_vfo)
    {
        HAMLIB_TRACE;
        retcode = caps->set_rptr_offs(rig, vfo, rptr_offs);
        ELAPSED2;
        RETURNFUNC(retcode);
    }

    if (!caps->set_vfo)
    {
        RETURNFUNC(-RIG_ENAVAIL);
    }

    ELAPSED2;

    curr_vfo = rig->state.current_vfo;
    HAMLIB_TRACE;
    retcode = caps->set_vfo(rig, vfo);

    if (retcode != RIG_OK)
    {
        ELAPSED2;
        RETURNFUNC(retcode);
    }

    retcode = caps->set_rptr_offs(rig, vfo, rptr_offs);
    /* try and revert even if we had an error above */
    rc2 = caps->set_vfo(rig, curr_vfo);

    if (RIG_OK == retcode)
    {
        /* return the first error code */
        retcode = rc2;
    }

    ELAPSED2;
    RETURNFUNC(retcode);
}


/**
 * \brief get the current repeater offset
 * \param rig   The rig handle
 * \param vfo   The target VFO
 * \param rptr_offs The location where to store the current repeater offset
 *
 *  Retrieves the current repeater offset.
 *
 * \return RIG_OK if the operation has been successful, otherwise
 * a negative value if an error occurred (in which case, cause is
 * set appropriately).
 *
 * \sa rig_set_rptr_offs()
 */
int HAMLIB_API rig_get_rptr_offs(RIG *rig, vfo_t vfo, shortfreq_t *rptr_offs)
{
    const struct rig_caps *caps;
    int retcode, rc2;
    vfo_t curr_vfo;

    ELAPSED1;
    ENTERFUNC;

    if (CHECK_RIG_ARG(rig))
    {
        ELAPSED2;
        RETURNFUNC(-RIG_EINVAL);
    }

    if (!rptr_offs)
    {
        ELAPSED2;
        RETURNFUNC(-RIG_EINVAL);
    }

    caps = rig->caps;

    if (caps->get_rptr_offs == NULL)
    {
        ELAPSED2;
        RETURNFUNC(-RIG_ENAVAIL);
    }

    if (vfo == RIG_VFO_CURR
            || vfo == rig->state.current_vfo)
    {
        HAMLIB_TRACE;
        retcode = caps->get_rptr_offs(rig, vfo, rptr_offs);
        ELAPSED2;
        RETURNFUNC(retcode);
    }

    if (!caps->set_vfo)
    {
        ELAPSED2;
        RETURNFUNC(-RIG_ENAVAIL);
    }

    curr_vfo = rig->state.current_vfo;
    HAMLIB_TRACE;
    retcode = caps->set_vfo(rig, vfo);

    if (retcode != RIG_OK)
    {
        ELAPSED2;
        RETURNFUNC(retcode);
    }

    retcode = caps->get_rptr_offs(rig, vfo, rptr_offs);
    /* try and revert even if we had an error above */
    rc2 = caps->set_vfo(rig, curr_vfo);

    if (RIG_OK == retcode)
    {
        /* return the first error code */
        retcode = rc2;
    }

    ELAPSED2;
    RETURNFUNC(retcode);
}


/**
 * \brief set the split frequencies
 * \param rig   The rig handle
 * \param vfo   The target VFO
 * \param tx_freq   The transmit split frequency to set to
 *
 *  Sets the split(TX) frequency.
 *
 * \return RIG_OK if the operation has been successful, otherwise
 * a negative value if an error occurred (in which case, cause is
 * set appropriately).
 *
 * \sa rig_get_split_freq(), rig_set_split_vfo()
 */
int HAMLIB_API rig_set_split_freq(RIG *rig, vfo_t vfo, freq_t tx_freq)
{
    const struct rig_caps *caps;
    int retcode, rc2;
    vfo_t curr_vfo, tx_vfo;
    freq_t tfreq = 0;

    ELAPSED1;

    if (CHECK_RIG_ARG(rig))
    {
        ELAPSED2;
        RETURNFUNC2(-RIG_EINVAL);
    }


    rig_debug(RIG_DEBUG_VERBOSE, "%s called vfo=%s, curr_vfo=%s, tx_freq=%.0f\n",
              __func__,
              rig_strvfo(vfo), rig_strvfo(rig->state.current_vfo), tx_freq);

    caps = rig->caps;

    /* Use previously setup TxVFO */
    if (vfo == RIG_VFO_CURR || vfo == RIG_VFO_TX)
    {
        tx_vfo = rig->state.tx_vfo;
    }
    else
    {
        tx_vfo = vfo_fixup(rig, vfo, rig->state.cache.split);
    }

    rig_get_freq(rig, tx_vfo, &tfreq);

    if (tfreq == tx_freq)
    {
        rig_debug(RIG_DEBUG_TRACE, "%s: freq set not needed\n", __func__);
        ELAPSED2;
        RETURNFUNC2(RIG_OK);
    }

    if (caps->set_split_freq
            && (vfo == RIG_VFO_CURR
                || vfo == RIG_VFO_TX
                || tx_vfo == rig->state.current_vfo
                || (caps->targetable_vfo & RIG_TARGETABLE_FREQ)))
    {
        HAMLIB_TRACE;
        retcode = caps->set_split_freq(rig, vfo, tx_freq);
        ELAPSED2;
        RETURNFUNC2(retcode);
    }

    vfo = vfo_fixup(rig, vfo, rig->state.cache.split);


    /* Assisted mode */
    curr_vfo = rig->state.current_vfo;

    if (caps->set_freq)
    {
        int retry = 3;
        freq_t tfreq;

        do
        {
            HAMLIB_TRACE;
            retcode = rig_set_freq(rig, tx_vfo, tx_freq);

            if (retcode != RIG_OK) { RETURNFUNC(retcode); }

#if 0 // this verification seems to be causing bad behavior on some rigs
            retcode = rig_get_freq(rig, tx_vfo, &tfreq);
#else
            tfreq = tx_freq;
#endif
        }
        while (tfreq != tx_freq && retry-- > 0 && retcode == RIG_OK);

        ELAPSED2;
        RETURNFUNC2(retcode);
    }

    if (caps->set_vfo)
    {
        HAMLIB_TRACE;
        retcode = RIG_OK;

        retcode = caps->set_vfo(rig, tx_vfo);
    }
    else if (rig_has_vfo_op(rig, RIG_OP_TOGGLE) && caps->vfo_op)
    {
        retcode = caps->vfo_op(rig, vfo, RIG_OP_TOGGLE);
    }
    else
    {
        ELAPSED2;
        RETURNFUNC2(-RIG_ENAVAIL);
    }

    if (retcode != RIG_OK)
    {
        ELAPSED2;
        RETURNFUNC2(retcode);
    }

    int retry = 3;

    do
    {
        // doing get_freq seems to break on some rigs that can't read freq immediately after set
        if (caps->set_split_freq)
        {
            HAMLIB_TRACE;
            retcode = caps->set_split_freq(rig, vfo, tx_freq);
            //rig_get_freq(rig, vfo, &tfreq);
        }
        else
        {
            HAMLIB_TRACE;
            retcode = rig_set_freq(rig, RIG_VFO_CURR, tx_freq);
            //rig_get_freq(rig, vfo, &tfreq);
        }

        tfreq = tx_freq;
    }
    while (tfreq != tx_freq && retry-- > 0 && retcode == RIG_OK);

    /* try and revert even if we had an error above */
    if (caps->set_vfo)
    {
        HAMLIB_TRACE;
        rc2 = RIG_OK;

        if (!(caps->targetable_vfo & RIG_TARGETABLE_FREQ))
        {
            rc2 = caps->set_vfo(rig, curr_vfo);
        }
    }
    else
    {
        rc2 = caps->vfo_op(rig, vfo, RIG_OP_TOGGLE);
    }

    if (RIG_OK == retcode)
    {
        /* return the first error code */
        retcode = rc2;
    }

    ELAPSED2;
    RETURNFUNC2(retcode);
}


/**
 * \brief get the current split frequencies
 * \param rig   The rig handle
 * \param vfo   The target VFO
 * \param tx_freq   The location where to store the current transmit split frequency
 *
 *  Retrieves the current split(TX) frequency.
 *
 * \return RIG_OK if the operation has been successful, otherwise
 * a negative value if an error occurred (in which case, cause is
 * set appropriately).
 *
 * \sa rig_set_split_freq()
 */
int HAMLIB_API rig_get_split_freq(RIG *rig, vfo_t vfo, freq_t *tx_freq)
{
    const struct rig_caps *caps;
    int retcode = -RIG_EPROTO, rc2;
    vfo_t save_vfo, tx_vfo;

    ELAPSED1;
    ENTERFUNC;

    if (CHECK_RIG_ARG(rig))
    {
        ELAPSED2;
        RETURNFUNC(-RIG_EINVAL);
    }

    if (!tx_freq)
    {
        ELAPSED2;
        RETURNFUNC(-RIG_EINVAL);
    }

    vfo = vfo_fixup(rig, vfo, rig->state.cache.split);

    caps = rig->caps;

    if (caps->get_split_freq
            && (vfo == RIG_VFO_CURR
                || vfo == RIG_VFO_TX
                || vfo == rig->state.current_vfo))
    {
        HAMLIB_TRACE;
        retcode = caps->get_split_freq(rig, vfo, tx_freq);
        ELAPSED2;
        RETURNFUNC(retcode);
    }

    /* Assisted mode */
    save_vfo = rig->state.current_vfo;

    /* Use previously setup TxVFO */
    if (vfo == RIG_VFO_CURR || vfo == RIG_VFO_TX)
    {
        tx_vfo = rig->state.tx_vfo;
    }
    else
    {
        tx_vfo = vfo;
    }

    if (caps->get_freq && (caps->targetable_vfo & RIG_TARGETABLE_FREQ))
    {
        HAMLIB_TRACE;
        retcode = caps->get_freq(rig, tx_vfo, tx_freq);
        ELAPSED2;
        RETURNFUNC(retcode);
    }


    if (caps->set_vfo)
    {
        // if the underlying rig has OP_XCHG we don't need to set VFO
        if (!rig_has_vfo_op(rig, RIG_OP_XCHG)
                && !(caps->targetable_vfo & RIG_TARGETABLE_FREQ))
        {
            HAMLIB_TRACE;
            retcode = caps->set_vfo(rig, tx_vfo);

            if (retcode != RIG_OK) { RETURNFUNC(retcode); }
        }

        retcode = RIG_OK;
    }
    else if (rig_has_vfo_op(rig, RIG_OP_TOGGLE) && caps->vfo_op)
    {
        retcode = caps->vfo_op(rig, vfo, RIG_OP_TOGGLE);
    }
    else
    {
        ELAPSED2;
        RETURNFUNC(-RIG_ENAVAIL);
    }

    if (retcode != RIG_OK)
    {
        ELAPSED2;
        RETURNFUNC(retcode);
    }

    if (caps->get_split_freq)
    {
        HAMLIB_TRACE;
        retcode = caps->get_split_freq(rig, vfo, tx_freq);
    }
    else
    {
        HAMLIB_TRACE;
        retcode = caps->get_freq ? caps->get_freq(rig, RIG_VFO_CURR,
                  tx_freq) : -RIG_ENIMPL;
    }

    /* try and revert even if we had an error above */
    if (caps->set_vfo)
    {
        // If we started with RIG_VFO_CURR we need to choose VFO_A/MAIN as appropriate to return to
        //rig_debug(RIG_DEBUG_TRACE, "%s: save_vfo=%s, hasmainsub=%d\n",__func__, rig_strvfo(save_vfo), VFO_HAS_MAIN_SUB);
        save_vfo = VFO_HAS_MAIN_SUB ? RIG_VFO_MAIN : RIG_VFO_A;

        rig_debug(RIG_DEBUG_TRACE, "%s: restoring vfo=%s\n", __func__,
                  rig_strvfo(save_vfo));
        HAMLIB_TRACE;

        if (!rig_has_vfo_op(rig, RIG_OP_XCHG)
                && !(caps->targetable_vfo & RIG_TARGETABLE_FREQ))
        {
            rc2 = caps->set_vfo(rig, save_vfo);
        }
        else
        {
            rc2 = RIG_OK;
        }
    }
    else
    {
        rc2 = caps->vfo_op(rig, vfo, RIG_OP_TOGGLE);
    }

    if (RIG_OK == retcode)
    {
        /* return the first error code */
        retcode = rc2;
    }

    rig_debug(RIG_DEBUG_TRACE, "%s: tx_freq=%.0f\n", __func__, *tx_freq);

    ELAPSED2;
    RETURNFUNC(retcode);
}


/**
 * \brief set the split modes
 * \param rig   The rig handle
 * \param vfo   The target VFO
 * \param tx_mode   The transmit split mode to set to
 * \param tx_width The transmit split width to set to or the special
 * value RIG_PASSBAND_NOCHANGE which leaves the passband unchanged
 * from the current value or default for the mode determined by the
 * rig.
 *
 *  Sets the split(TX) mode.
 *
 * \return RIG_OK if the operation has been successful, otherwise
 * a negative value if an error occurred (in which case, cause is
 * set appropriately).
 *
 * \sa rig_get_split_mode()
 */
int HAMLIB_API rig_set_split_mode(RIG *rig,
                                  vfo_t vfo,
                                  rmode_t tx_mode,
                                  pbwidth_t tx_width)
{
    const struct rig_caps *caps;
    int retcode, rc2;
    vfo_t curr_vfo, tx_vfo, rx_vfo;

    ELAPSED1;
    ENTERFUNC;

    if (CHECK_RIG_ARG(rig))
    {
        ELAPSED2;
        RETURNFUNC(-RIG_EINVAL);
    }

    // we check both VFOs are in the same tx mode -- then we can ignore
    // this could be make more intelligent but this should cover all cases where we can skip this
    if (tx_mode == rig->state.cache.modeMainA
            && tx_mode == rig->state.cache.modeMainB)
    {
        rig_debug(RIG_DEBUG_TRACE, "%s: mode already %s so no change required\n",
                  __func__, rig_strrmode(tx_mode));
        ELAPSED2;
        RETURNFUNC(RIG_OK);
    }
    else
    {
        rig_debug(RIG_DEBUG_TRACE, "%s: mode %s is different from A=%s and B=%s\n",
                  __func__, rig_strrmode(tx_mode), rig_strrmode(rig->state.cache.modeMainA),
                  rig_strrmode(rig->state.cache.modeMainB));
    }

    // do not mess with mode while PTT is on
    if (rig->state.cache.ptt)
    {
        rig_debug(RIG_DEBUG_VERBOSE, "%s PTT on so set_split_mode ignored\n", __func__);
        ELAPSED2;
        RETURNFUNC(RIG_OK);
    }

    caps = rig->caps;

    if (caps->set_split_mode
            && (vfo == RIG_VFO_CURR
                || vfo == RIG_VFO_TX
                || vfo == rig->state.current_vfo
                || rig->caps->rig_model == RIG_MODEL_NETRIGCTL))
    {
        HAMLIB_TRACE;
        retcode = caps->set_split_mode(rig, vfo, tx_mode, tx_width);
        ELAPSED2;
        RETURNFUNC(retcode);
    }

    /* Assisted mode */
    curr_vfo = rig->state.current_vfo;

    /* Use previously setup TxVFO */
    if (vfo == RIG_VFO_CURR || vfo == RIG_VFO_TX
            || rig->state.tx_vfo != RIG_VFO_NONE)
    {
        HAMLIB_TRACE;
        tx_vfo = rig->state.tx_vfo;
    }
    else
    {
        HAMLIB_TRACE;
        tx_vfo = vfo;
    }

    rig_debug(RIG_DEBUG_VERBOSE, "%s: curr_vfo=%s, tx_vfo=%s\n", __func__,
              rig_strvfo(curr_vfo), rig_strvfo(tx_vfo));

    if (caps->set_mode && ((caps->targetable_vfo & RIG_TARGETABLE_MODE)
                           || (rig->caps->rig_model == RIG_MODEL_NETRIGCTL)))
    {
        HAMLIB_TRACE;
        retcode = caps->set_mode(rig, tx_vfo, tx_mode, tx_width);
        ELAPSED2;
        RETURNFUNC(retcode);
    }

    // some rigs exhibit undesirable flashing when swapping vfos in split
    // so we turn it off, do our thing, and turn split back on
    rx_vfo = vfo;

    if (tx_vfo == RIG_VFO_B) { rx_vfo = RIG_VFO_A; }

    if (vfo == RIG_VFO_CURR && tx_vfo == RIG_VFO_B) { rx_vfo = RIG_VFO_A; }
    else if (vfo == RIG_VFO_CURR && tx_vfo == RIG_VFO_A) { rx_vfo = RIG_VFO_B; }
    else if (vfo == RIG_VFO_CURR && tx_vfo == RIG_VFO_MAIN) { rx_vfo = RIG_VFO_SUB; }
    else if (vfo == RIG_VFO_CURR && tx_vfo == RIG_VFO_SUB) { rx_vfo = RIG_VFO_MAIN; }

    rig_debug(RIG_DEBUG_VERBOSE, "%s(%d): rx_vfo=%s, tx_vfo=%s\n", __func__,
              __LINE__, rig_strvfo(rx_vfo), rig_strvfo(tx_vfo));

    // we will reuse cached mode instead of trying to set mode again
    if ((tx_vfo & (RIG_VFO_A | RIG_VFO_MAIN | RIG_VFO_MAIN_A | RIG_VFO_SUB_A))
            && (tx_mode == rig->state.cache.modeMainA))
    {
        rig_debug(RIG_DEBUG_VERBOSE, "%s(%d): VFOA mode=%s already set...ignoring\n",
                  __func__, __LINE__, rig_strrmode(tx_mode));
        ELAPSED2;
        RETURNFUNC(RIG_OK);
    }
    else if ((tx_vfo & (RIG_VFO_B | RIG_VFO_SUB | RIG_VFO_MAIN_B | RIG_VFO_SUB_B))
             && (tx_mode == rig->state.cache.modeMainB))
    {
        rig_debug(RIG_DEBUG_VERBOSE, "%s(%d): VFOB mode=%s already set...ignoring\n",
                  __func__, __LINE__, rig_strrmode(tx_mode));
        ELAPSED2;
        RETURNFUNC(RIG_OK);
    }

    if (tx_vfo & (RIG_VFO_CURR | RIG_VFO_TX))
    {
        rig_debug(RIG_DEBUG_WARN, "%s(%d): Unhandled TXVFO=%s, tx_mode=%s\n", __func__,
                  __LINE__, rig_strvfo(tx_vfo), rig_strrmode(tx_mode));
    }

    // code below here should be dead code now -- but maybe we have  VFO situatiuon we need to handle
    if (caps->rig_model == RIG_MODEL_NETRIGCTL)
    {
        // special handlingt for netrigctl to avoid set_vfo
        retcode = caps->set_split_mode(rig, vfo, tx_mode, tx_width);
        ELAPSED2;
        RETURNFUNC(retcode);
    }

    rig_set_split_vfo(rig, rx_vfo, RIG_SPLIT_OFF, rx_vfo);

    if (caps->set_vfo)
    {
        HAMLIB_TRACE;
        retcode = caps->set_vfo(rig, tx_vfo);
    }
    else if (rig_has_vfo_op(rig, RIG_OP_TOGGLE) && caps->vfo_op)
    {
        retcode = caps->vfo_op(rig, vfo, RIG_OP_TOGGLE);
    }
    else
    {
        rig_debug(RIG_DEBUG_WARN,
                  "%s: rig does not have set_vfo or vfo_op. Assuming mode already set\n",
                  __func__);
        ELAPSED2;
        RETURNFUNC(RIG_OK);
    }

    if (retcode != RIG_OK)
    {
        ELAPSED2;
        RETURNFUNC(retcode);
    }

    if (caps->set_split_mode)
    {
        HAMLIB_TRACE;
        retcode = caps->set_split_mode(rig, vfo, tx_mode, tx_width);
    }
    else
    {
        HAMLIB_TRACE;
        retcode = caps->set_mode ? caps->set_mode(rig, RIG_VFO_CURR, tx_mode,
                  tx_width) : -RIG_ENIMPL;
    }

    /* try and revert even if we had an error above */
    if (caps->set_vfo)
    {
        HAMLIB_TRACE;
        rc2 = caps->set_vfo(rig, rx_vfo);
    }
    else
    {
        rc2 = caps->vfo_op(rig, vfo, RIG_OP_TOGGLE);
    }

    if (RIG_OK == retcode)
    {
        /* return the first error code */
        retcode = rc2;
    }

    rig_set_split_vfo(rig, rx_vfo, RIG_SPLIT_ON, tx_vfo);

    if (vfo == RIG_VFO_A || vfo == RIG_VFO_MAIN)
    {
        rig->state.cache.modeMainA = tx_mode;
    }
    else
    {
        rig->state.cache.modeMainB = tx_mode;
    }


    ELAPSED2;
    RETURNFUNC(retcode);
}


/**
 * \brief get the current split modes
 * \param rig   The rig handle
 * \param vfo   The target VFO
 * \param tx_mode   The location where to store the current transmit split mode
 * \param tx_width  The location where to store the current transmit split width
 *
 *  Retrieves the current split(TX) mode and passband.
 *  If the backend is unable to determine the width, the \a tx_width
 *  will be set to RIG_PASSBAND_NORMAL as a default.
 *  The value stored at \a tx_mode location equals RIG_MODE_NONE
 *  when the current mode of the VFO is not defined (e.g. blank memory).
 *
 * \return RIG_OK if the operation has been successful, otherwise
 * a negative value if an error occurred (in which case, cause is
 * set appropriately).
 *
 * \sa rig_set_split_mode()
 */
int HAMLIB_API rig_get_split_mode(RIG *rig, vfo_t vfo, rmode_t *tx_mode,
                                  pbwidth_t *tx_width)
{
    const struct rig_caps *caps;
    int retcode, rc2;
    vfo_t curr_vfo, tx_vfo;

    ELAPSED1;
    ENTERFUNC;

    if (CHECK_RIG_ARG(rig))
    {
        ELAPSED2;
        RETURNFUNC(-RIG_EINVAL);
    }

    if (!tx_mode || !tx_width)
    {
        ELAPSED2;
        RETURNFUNC(-RIG_EINVAL);
    }

    caps = rig->caps;

    if (caps->get_split_mode
            && (vfo == RIG_VFO_CURR
                || vfo == RIG_VFO_TX
                || vfo == rig->state.current_vfo))
    {
        HAMLIB_TRACE;
        retcode = caps->get_split_mode(rig, vfo, tx_mode, tx_width);
        ELAPSED2;
        RETURNFUNC(retcode);
    }

    /* Assisted mode */
    curr_vfo = rig->state.current_vfo;

    /* Use previously setup TxVFO */
    if (vfo == RIG_VFO_CURR || vfo == RIG_VFO_TX)
    {
        tx_vfo = rig->state.tx_vfo;
    }
    else
    {
        tx_vfo = vfo;
    }

    if (caps->get_mode && (caps->targetable_vfo & RIG_TARGETABLE_MODE))
    {
        HAMLIB_TRACE;
        retcode = caps->get_mode(rig, tx_vfo, tx_mode, tx_width);
        ELAPSED2;
        RETURNFUNC(retcode);
    }


    if (caps->set_vfo)
    {
        HAMLIB_TRACE;
        retcode = caps->set_vfo(rig, tx_vfo);
    }
    else if (rig_has_vfo_op(rig, RIG_OP_TOGGLE) && caps->vfo_op)
    {
        HAMLIB_TRACE;
        retcode = caps->vfo_op(rig, vfo, RIG_OP_TOGGLE);
    }
    else
    {
        ELAPSED2;
        RETURNFUNC(-RIG_ENAVAIL);
    }

    if (retcode != RIG_OK)
    {
        ELAPSED2;
        RETURNFUNC(retcode);
    }

    if (caps->get_split_mode)
    {
        HAMLIB_TRACE;
        retcode = caps->get_split_mode(rig, vfo, tx_mode, tx_width);
    }
    else
    {
        HAMLIB_TRACE;
        retcode = caps->get_mode ? caps->get_mode(rig, RIG_VFO_CURR, tx_mode,
                  tx_width) : -RIG_ENIMPL;
    }

    /* try and revert even if we had an error above */
    if (caps->set_vfo)
    {
        HAMLIB_TRACE;
        rc2 = caps->set_vfo(rig, curr_vfo);
    }
    else
    {
        rc2 = caps->vfo_op(rig, vfo, RIG_OP_TOGGLE);
    }

    if (RIG_OK == retcode)
    {
        /* return the first error code */
        retcode = rc2;
    }

    if (*tx_width == RIG_PASSBAND_NORMAL && *tx_mode != RIG_MODE_NONE)
    {
        *tx_width = rig_passband_normal(rig, *tx_mode);
    }

    ELAPSED2;
    RETURNFUNC(retcode);
}


/**
 * \brief set the split frequency and mode
 * \param rig   The rig handle
 * \param vfo   The target VFO
 * \param tx_freq The transmit frequency to set to
 * \param tx_mode   The transmit split mode to set to
 * \param tx_width The transmit split width to set to or the special
 * value RIG_PASSBAND_NOCHANGE which leaves the passband unchanged
 * from the current value or default for the mode determined by the
 * rig.
 *
 *  Sets the split(TX) frequency and mode.
 *
 *  This function maybe optimized on some rig back ends, where the TX
 *  VFO cannot be directly addressed, to reduce the number of times
 *  the rig VFOs have to be exchanged or swapped to complete this
 *  combined function.
 *
 * \return RIG_OK if the operation has been successful, otherwise
 * a negative value if an error occurred (in which case, cause is
 * set appropriately).
 *
 * \sa rig_set_split_freq(), rig_set_split_mode(), rig_get_split_freq_mode()
 */
int HAMLIB_API rig_set_split_freq_mode(RIG *rig,
                                       vfo_t vfo,
                                       freq_t tx_freq,
                                       rmode_t tx_mode,
                                       pbwidth_t tx_width)
{
    const struct rig_caps *caps;
    int retcode;

    ELAPSED1;
    ENTERFUNC;

    if (CHECK_RIG_ARG(rig))
    {
        ELAPSED2;
        RETURNFUNC(-RIG_EINVAL);
    }

    caps = rig->caps;

    // if split is off we'll turn it on
    if (rig->state.cache.split == 0)
    {
        if (rig->state.current_vfo & (RIG_VFO_A | RIG_VFO_MAIN))
        {
            rig_set_split_vfo(rig, RIG_VFO_A, 1, RIG_VFO_B);
        }
        else
        {
            rig_set_split_vfo(rig, RIG_VFO_B, 1, RIG_VFO_A);
        }
    }

    vfo = vfo_fixup(rig, RIG_VFO_TX, rig->state.cache.split); // get the TX VFO
    rig_debug(RIG_DEBUG_VERBOSE,
              "%s: vfo=%s, tx_freq=%.0f, tx_mode=%s, tx_width=%d\n", __func__,
              rig_strvfo(vfo), tx_freq, rig_strrmode(tx_mode), (int)tx_width);

    if (caps->set_split_freq_mode)
    {
#if 0
        freq_t tfreq;
        int retry = 3;
        int retcode2;
#endif

        HAMLIB_TRACE;
        retcode = caps->set_split_freq_mode(rig, vfo, tx_freq, tx_mode, tx_width);
#if 0 // this verification seems to be causing bad behavior on some reigs

        // we query freq after set to ensure it really gets done
        do
        {
            retcode = caps->set_split_freq_mode(rig, vfo, tx_freq, tx_mode, tx_width);
            retcode2 = rig_get_split_freq(rig, vfo, &tfreq);

            if (tfreq != tx_freq)
            {
                rig_debug(RIG_DEBUG_ERR,
                          "%s: txfreq!=tfreq %.0f!=%.0f, retry=%d, rc1=%d, rc2=%d\n", __func__, tx_freq,
                          tfreq, retry, retcode, retcode2);
                hl_usleep(50 * 1000); // 50ms sleep may help here
            }

            tfreq = tx_freq;
            retcode2 = RIG_OK;
        }
        while (tfreq != tx_freq && retry-- > 0 && retcode == RIG_OK
                && retcode2 == RIG_OK);

        if (tfreq != tx_freq) { retcode = -RIG_EPROTO; }

#endif

        ELAPSED2;
        RETURNFUNC(retcode);
    }
    else
    {
        HAMLIB_TRACE;
        retcode = rig_set_split_freq(rig, vfo, tx_freq);
    }

    if (RIG_OK == retcode)
    {
        HAMLIB_TRACE;
        retcode = rig_set_split_mode(rig, vfo, tx_mode, tx_width);
    }

    ELAPSED2;
    RETURNFUNC(retcode);
}


/**
 * \brief get the current split frequency and mode
 * \param rig   The rig handle
 * \param vfo   The target VFO
 * \param tx_freq The location where to store the current transmit frequency
 * \param tx_mode   The location where to store the current transmit split mode
 * \param tx_width  The location where to store the current transmit split width
 *
 *  Retrieves the current split(TX) frequency, mode and passband.
 *  If the backend is unable to determine the width, the \a tx_width
 *  will be set to RIG_PASSBAND_NORMAL as a default.
 *  The value stored at \a tx_mode location equals RIG_MODE_NONE
 *  when the current mode of the VFO is not defined (e.g. blank memory).
 *
 *  This function maybe optimized on some rig back ends, where the TX
 *  VFO cannot be directly addressed, to reduce the number of times
 *  the rig VFOs have to be exchanged or swapped to complete this
 *  combined function.
 *
 * \return RIG_OK if the operation has been successful, otherwise
 * a negative value if an error occurred (in which case, cause is
 * set appropriately).
 *
 * \sa rig_get_split_freq(), rig_get_split_mode(), rig_set_split_freq_mode()
 */
int HAMLIB_API rig_get_split_freq_mode(RIG *rig,
                                       vfo_t vfo,
                                       freq_t *tx_freq,
                                       rmode_t *tx_mode,
                                       pbwidth_t *tx_width)
{
    const struct rig_caps *caps;
    int retcode;

    ELAPSED1;
    ENTERFUNC;

    if (CHECK_RIG_ARG(rig))
    {
        ELAPSED2;
        RETURNFUNC(-RIG_EINVAL);
    }

    if (!tx_freq || !tx_mode || !tx_width)
    {
        ELAPSED2;
        RETURNFUNC(-RIG_EINVAL);
    }

    caps = rig->caps;

    if (caps->get_split_freq_mode)
    {
        retcode = caps->get_split_freq_mode(rig, vfo, tx_freq, tx_mode, tx_width);
        ELAPSED2;
        RETURNFUNC(retcode);
    }

    HAMLIB_TRACE;
    retcode = rig_get_split_freq(rig, vfo, tx_freq);

    if (RIG_OK == retcode)
    {
        HAMLIB_TRACE;
        retcode = rig_get_split_mode(rig, vfo, tx_mode, tx_width);
    }

    ELAPSED2;
    RETURNFUNC(retcode);
}


/**
 * \brief set the split mode
 * \param rig   The rig handle
 * \param vfo   The target VFO
 * \param split The split mode to set to
 * \param tx_vfo    The transmit VFO
 *
 *  Sets the current split mode.
 *
 * \return RIG_OK if the operation has been successful, otherwise
 * a negative value if an error occurred (in which case, cause is
 * set appropriately).
 *
 * \sa rig_get_split_vfo()
 */
int HAMLIB_API rig_set_split_vfo(RIG *rig,
                                 vfo_t rx_vfo,
                                 split_t split,
                                 vfo_t tx_vfo)
{
    const struct rig_caps *caps;
    int retcode, rc2;
    vfo_t curr_vfo;

    ELAPSED1;
    ENTERFUNC;
    rig_debug(RIG_DEBUG_VERBOSE,
              "%s: rx_vfo=%s, split=%d, tx_vfo=%s, cache.split=%d\n", __func__,
              rig_strvfo(rx_vfo), split, rig_strvfo(tx_vfo), rig->state.cache.split);

    if (CHECK_RIG_ARG(rig))
    {
        ELAPSED2;
        RETURNFUNC(-RIG_EINVAL);
    }

    caps = rig->caps;

    if (caps->set_split_vfo == NULL)
    {
        ELAPSED2;
        RETURNFUNC(-RIG_ENAVAIL);
    }

    if (rig->state.cache.ptt)
    {
        rig_debug(RIG_DEBUG_WARN, "%s: cannot execute when PTT is on\n", __func__);
        ELAPSED2;
        return RIG_OK;
    }

    // We fix up vfos for non-satmode rigs only
    if (rig->caps->has_get_func & RIG_FUNC_SATMODE)
    {
        rig_debug(RIG_DEBUG_TRACE, "%s: satmode rig...not fixing up vfos rx=%s tx=%s\n",
                  __func__, rig_strvfo(rx_vfo), rig_strvfo(tx_vfo));
    }
    else
    {
        switch (tx_vfo)
        {
        case RIG_VFO_A: rx_vfo = split == 1 ? RIG_VFO_B : RIG_VFO_A; break;

        case RIG_VFO_B: rx_vfo = split == 1 ? RIG_VFO_A : RIG_VFO_B; break;
        }

        //rig->state.cache.split = split; // this gets set later
        //rig->state.cache.split_vfo = tx_vfo;
        rx_vfo = vfo_fixup(rig, rx_vfo, split);
        tx_vfo = vfo_fixup(rig, tx_vfo, split);
        rig->state.rx_vfo = rx_vfo;
        rig->state.tx_vfo = tx_vfo;
        rig_debug(RIG_DEBUG_VERBOSE, "%s: final rxvfo=%s, txvfo=%s, split=%d\n",
                  __func__,
                  rig_strvfo(rx_vfo), rig_strvfo(tx_vfo), rig->state.cache.split);
    }

    // set rig to the the requested RX VFO
    HAMLIB_TRACE;

    if ((!(caps->targetable_vfo & RIG_TARGETABLE_FREQ))
            && (!(rig->caps->rig_model == RIG_MODEL_NETRIGCTL)))
#if BUILTINFUNC
        rig_set_vfo(rig, rx_vfo == RIG_VFO_B ? RIG_VFO_B : RIG_VFO_A,
                    __builtin_FUNCTION());

#else
        rig_set_vfo(rig, rx_vfo == RIG_VFO_B ? RIG_VFO_B : RIG_VFO_A);
#endif

    if (rx_vfo == RIG_VFO_CURR
            || rx_vfo == rig->state.current_vfo)
    {
        // for non-targetable VFOs we will not set split again
        if (rig->state.cache.split == split && rig->state.cache.split_vfo == tx_vfo)
        {
            rig_debug(RIG_DEBUG_VERBOSE, "%s(%d): split already set...ignoring\n", __func__,
                      __LINE__);
            RETURNFUNC(RIG_OK);
        }

        HAMLIB_TRACE;
        retcode = caps->set_split_vfo(rig, rx_vfo, split, tx_vfo);

        if (retcode == RIG_OK)
        {
            rig->state.tx_vfo = tx_vfo;
        }

        rig->state.cache.split = split;
        rig->state.cache.split_vfo = tx_vfo;
        elapsed_ms(&rig->state.cache.time_split, HAMLIB_ELAPSED_SET);
        ELAPSED2;
        RETURNFUNC(retcode);
    }

    if (!caps->set_vfo)
    {
        ELAPSED2;
        RETURNFUNC(-RIG_ENAVAIL);
    }

    curr_vfo = rig->state.current_vfo;
    HAMLIB_TRACE;

    if (!(caps->targetable_vfo & RIG_TARGETABLE_FREQ))
    {
        retcode = caps->set_vfo(rig, rx_vfo);

        if (retcode != RIG_OK)
        {
            ELAPSED2;
            RETURNFUNC(retcode);
        }
    }

    HAMLIB_TRACE;
    retcode = caps->set_split_vfo(rig, rx_vfo, split, tx_vfo);

    /* try and revert even if we had an error above */
    if (!(caps->targetable_vfo & RIG_TARGETABLE_FREQ))
    {
        rc2 = caps->set_vfo(rig, curr_vfo);

        if (RIG_OK == retcode)
        {
            /* return the first error code */
            retcode = rc2;
        }
    }

    if (retcode == RIG_OK)
    {
        rig->state.tx_vfo = tx_vfo;
    }

    rig->state.cache.split = split;
    rig->state.cache.split_vfo = tx_vfo;
    elapsed_ms(&rig->state.cache.time_split, HAMLIB_ELAPSED_SET);
    ELAPSED2;
    RETURNFUNC(retcode);
}


/**
 * \brief get the current split mode
 * \param rig   The rig handle
 * \param vfo   The target VFO
 * \param split The location where to store the current split mode
 * \param tx_vfo    The transmit VFO
 *
 *  Retrieves the current split mode.
 *
 * \return RIG_OK if the operation has been successful, otherwise
 * a negative value if an error occurred (in which case, cause is
 * set appropriately).
 *
 * \sa rig_set_split_vfo()
 */
int HAMLIB_API rig_get_split_vfo(RIG *rig,
                                 vfo_t vfo,
                                 split_t *split,
                                 vfo_t *tx_vfo)
{
    const struct rig_caps *caps;
#if 0
    int retcode, rc2;
#else
    int retcode;
#endif
#if 0
    vfo_t curr_vfo;
#endif
    int cache_ms;

    ELAPSED1;
    ENTERFUNC;

    if (CHECK_RIG_ARG(rig))
    {
        ELAPSED2;
        RETURNFUNC(-RIG_EINVAL);
    }

    if (!split || !tx_vfo)
    {
        rig_debug(RIG_DEBUG_ERR, "%s: split or tx_vfo is null, split=%p, tx_vfo=%p\n",
                  __func__, split, tx_vfo);
        ELAPSED2;
        RETURNFUNC(-RIG_EINVAL);
    }

    caps = rig->caps;

    if (caps->get_split_vfo == NULL)
    {
        // if we can't get the vfo we will return whatever we have cached
        *split = rig->state.cache.split;
        *tx_vfo = rig->state.cache.split_vfo;
        rig_debug(RIG_DEBUG_VERBOSE,
                  "%s: no get_split_vfo so returning split=%d, tx_vfo=%s\n", __func__, *split,
                  rig_strvfo(*tx_vfo));
        ELAPSED2;
        RETURNFUNC(RIG_OK);
    }

    cache_ms = elapsed_ms(&rig->state.cache.time_split, HAMLIB_ELAPSED_GET);
    rig_debug(RIG_DEBUG_TRACE, "%s: cache check age=%dms\n", __func__, cache_ms);

    if (cache_ms < rig->state.cache.timeout_ms)
    {
        *split = rig->state.cache.split;
        *tx_vfo = rig->state.cache.split_vfo;
        rig_debug(RIG_DEBUG_TRACE, "%s: cache hit age=%dms, split=%d, tx_vfo=%s\n",
                  __func__, cache_ms, *split, rig_strvfo(*tx_vfo));
        ELAPSED2;
        RETURNFUNC(RIG_OK);
    }
    else
    {
        rig_debug(RIG_DEBUG_TRACE, "%s: cache miss age=%dms\n", __func__, cache_ms);
    }

    /* overridden by backend at will */
    *tx_vfo = rig->state.tx_vfo;

    if ((vfo == RIG_VFO_CURR) || (vfo == rig->state.current_vfo))
    {
        HAMLIB_TRACE;
        retcode = RIG_OK;
        //if (rig->caps->rig_model != RIG_MODEL_NETRIGCTL)
        {
            // rigctld doesn't like nested calls
            retcode = caps->get_split_vfo(rig, vfo, split, tx_vfo);
            rig->state.cache.split = *split;
            rig->state.cache.split_vfo = *tx_vfo;
            elapsed_ms(&rig->state.cache.time_split, HAMLIB_ELAPSED_SET);
            rig_debug(RIG_DEBUG_TRACE, "%s: cache.split=%d\n", __func__,
                      rig->state.cache.split);
        }
        ELAPSED2;
        RETURNFUNC(retcode);
    }

    if (!caps->set_vfo)
    {
        ELAPSED2;
        RETURNFUNC(-RIG_ENAVAIL);
    }

#if 0 // why were we doing this?  Shouldn't need to set_vfo to figure out tx_vfo
    curr_vfo = rig->state.current_vfo;
    retcode = caps->set_vfo(rig, vfo);

    if (retcode != RIG_OK)
    {
        ELAPSED2;
        RETURNFUNC(retcode);
    }

#endif

    HAMLIB_TRACE;
    retcode = caps->get_split_vfo(rig, vfo, split, tx_vfo);
#if 0 // see above
    /* try and revert even if we had an error above */
    rc2 = caps->set_vfo(rig, curr_vfo);

    if (RIG_OK == retcode)
    {
        /* return the first error code */
        retcode = rc2;
    }

#endif

    if (retcode == RIG_OK)  // only update cache on success
    {
        rig->state.cache.split = *split;
        rig->state.cache.split_vfo = *tx_vfo;
        elapsed_ms(&rig->state.cache.time_split, HAMLIB_ELAPSED_SET);
        rig_debug(RIG_DEBUG_TRACE, "%s(%d): cache.split=%d\n", __func__, __LINE__,
                  rig->state.cache.split);
    }

    ELAPSED2;
    RETURNFUNC(retcode);
}


/**
 * \brief set the RIT
 * \param rig   The rig handle
 * \param vfo   The target VFO
 * \param rit   The RIT offset to adjust to
 *
 *  Sets the current RIT offset. A value of 0 for \a rit disables RIT.
 *
 * \return RIG_OK if the operation has been successful, otherwise
 * a negative value if an error occurred (in which case, cause is
 * set appropriately).
 *
 * \sa rig_get_rit()
 */
int HAMLIB_API rig_set_rit(RIG *rig, vfo_t vfo, shortfreq_t rit)
{
    const struct rig_caps *caps;
    int retcode, rc2;
    vfo_t curr_vfo;

    ENTERFUNC;

    if (CHECK_RIG_ARG(rig))
    {
        RETURNFUNC(-RIG_EINVAL);
    }

    caps = rig->caps;

    if (caps->set_rit == NULL)
    {
        RETURNFUNC(-RIG_ENAVAIL);
    }

    if ((caps->targetable_vfo & RIG_TARGETABLE_RITXIT)
            || vfo == RIG_VFO_CURR
            || vfo == rig->state.current_vfo)
    {
        HAMLIB_TRACE;
        retcode = caps->set_rit(rig, vfo, rit);
        RETURNFUNC(retcode);
    }

    if (!caps->set_vfo)
    {
        RETURNFUNC(-RIG_ENAVAIL);
    }

    curr_vfo = rig->state.current_vfo;
    HAMLIB_TRACE;
    retcode = caps->set_vfo(rig, vfo);

    if (retcode != RIG_OK)
    {
        RETURNFUNC(retcode);
    }

    retcode = caps->set_rit(rig, vfo, rit);
    /* try and revert even if we had an error above */
    rc2 = caps->set_vfo(rig, curr_vfo);

    if (RIG_OK == retcode)
    {
        /* return the first error code */
        retcode = rc2;
    }

    RETURNFUNC(retcode);
}


/**
 * \brief get the current RIT offset
 * \param rig   The rig handle
 * \param vfo   The target VFO
 * \param rit   The location where to store the current RIT offset
 *
 *  Retrieves the current RIT offset.
 *
 * \return RIG_OK if the operation has been successful, otherwise
 * a negative value if an error occurred (in which case, cause is
 * set appropriately).
 *
 * \sa rig_set_rit()
 */
int HAMLIB_API rig_get_rit(RIG *rig, vfo_t vfo, shortfreq_t *rit)
{
    const struct rig_caps *caps;
    int retcode, rc2;
    vfo_t curr_vfo;

    ENTERFUNC;

    if (CHECK_RIG_ARG(rig))
    {
        RETURNFUNC(-RIG_EINVAL);
    }

    if (!rit)
    {
        RETURNFUNC(-RIG_EINVAL);
    }

    caps = rig->caps;

    if (caps->get_rit == NULL)
    {
        RETURNFUNC(-RIG_ENAVAIL);
    }

    if ((caps->targetable_vfo & RIG_TARGETABLE_RITXIT)
            || vfo == RIG_VFO_CURR
            || vfo == rig->state.current_vfo)
    {
        HAMLIB_TRACE;
        retcode = caps->get_rit(rig, vfo, rit);
        RETURNFUNC(retcode);
    }

    if (!caps->set_vfo)
    {
        RETURNFUNC(-RIG_ENAVAIL);
    }

    curr_vfo = rig->state.current_vfo;
    HAMLIB_TRACE;
    retcode = caps->set_vfo(rig, vfo);

    if (retcode != RIG_OK)
    {
        RETURNFUNC(retcode);
    }

    HAMLIB_TRACE;
    retcode = caps->get_rit(rig, vfo, rit);
    /* try and revert even if we had an error above */
    rc2 = caps->set_vfo(rig, curr_vfo);

    if (RIG_OK == retcode)
    {
        /* return the first error code */
        retcode = rc2;
    }

    RETURNFUNC(retcode);
}


/**
 * \brief set the XIT
 * \param rig   The rig handle
 * \param vfo   The target VFO
 * \param xit   The XIT offset to adjust to
 *
 *  Sets the current XIT offset. A value of 0 for \a xit disables XIT.
 *
 * \return RIG_OK if the operation has been successful, otherwise
 * a negative value if an error occurred (in which case, cause is
 * set appropriately).
 *
 * \sa rig_get_xit()
 */
int HAMLIB_API rig_set_xit(RIG *rig, vfo_t vfo, shortfreq_t xit)
{
    const struct rig_caps *caps;
    int retcode, rc2;
    vfo_t curr_vfo;

    ENTERFUNC;

    if (CHECK_RIG_ARG(rig))
    {
        RETURNFUNC(-RIG_EINVAL);
    }

    caps = rig->caps;

    if (caps->set_xit == NULL)
    {
        RETURNFUNC(-RIG_ENAVAIL);
    }

    if ((caps->targetable_vfo & RIG_TARGETABLE_RITXIT)
            || vfo == RIG_VFO_CURR
            || vfo == rig->state.current_vfo)
    {
        HAMLIB_TRACE;
        retcode = caps->set_xit(rig, vfo, xit);
        RETURNFUNC(retcode);
    }

    if (!caps->set_vfo)
    {
        RETURNFUNC(-RIG_ENAVAIL);
    }

    curr_vfo = rig->state.current_vfo;
    HAMLIB_TRACE;
    retcode = caps->set_vfo(rig, vfo);

    if (retcode != RIG_OK)
    {
        RETURNFUNC(retcode);
    }

    retcode = caps->set_xit(rig, vfo, xit);
    /* try and revert even if we had an error above */
    rc2 = caps->set_vfo(rig, curr_vfo);

    if (RIG_OK == retcode)
    {
        /* return the first error code */
        retcode = rc2;
    }

    RETURNFUNC(retcode);
}


/**
 * \brief get the current XIT offset
 * \param rig   The rig handle
 * \param vfo   The target VFO
 * \param xit   The location where to store the current XIT offset
 *
 *  Retrieves the current XIT offset.
 *
 * \return RIG_OK if the operation has been successful, otherwise
 * a negative value if an error occurred (in which case, cause is
 * set appropriately).
 *
 * \sa rig_set_xit()
 */
int HAMLIB_API rig_get_xit(RIG *rig, vfo_t vfo, shortfreq_t *xit)
{
    const struct rig_caps *caps;
    int retcode, rc2;
    vfo_t curr_vfo;

    ENTERFUNC;

    if (CHECK_RIG_ARG(rig))
    {
        RETURNFUNC(-RIG_EINVAL);
    }

    if (!xit)
    {
        RETURNFUNC(-RIG_EINVAL);
    }

    caps = rig->caps;

    if (caps->get_xit == NULL)
    {
        RETURNFUNC(-RIG_ENAVAIL);
    }

    if ((caps->targetable_vfo & RIG_TARGETABLE_RITXIT)
            || vfo == RIG_VFO_CURR
            || vfo == rig->state.current_vfo)
    {
        HAMLIB_TRACE;
        retcode = caps->get_xit(rig, vfo, xit);
        RETURNFUNC(retcode);
    }

    if (!caps->set_vfo)
    {
        RETURNFUNC(-RIG_ENAVAIL);
    }

    curr_vfo = rig->state.current_vfo;
    HAMLIB_TRACE;
    retcode = caps->set_vfo(rig, vfo);

    if (retcode != RIG_OK)
    {
        RETURNFUNC(retcode);
    }

    HAMLIB_TRACE;
    retcode = caps->get_xit(rig, vfo, xit);
    /* try and revert even if we had an error above */
    rc2 = caps->set_vfo(rig, curr_vfo);

    if (RIG_OK == retcode)
    {
        /* return the first error code */
        retcode = rc2;
    }

    RETURNFUNC(retcode);
}


/**
 * \brief set the Tuning Step
 * \param rig   The rig handle
 * \param vfo   The target VFO
 * \param ts    The tuning step to set to
 *
 *  Sets the Tuning Step.
 *
 * \return RIG_OK if the operation has been successful, otherwise
 * a negative value if an error occurred (in which case, cause is
 * set appropriately).
 *
 * \sa rig_get_ts()
 */
int HAMLIB_API rig_set_ts(RIG *rig, vfo_t vfo, shortfreq_t ts)
{
    const struct rig_caps *caps;
    int retcode, rc2;
    vfo_t curr_vfo;

    ENTERFUNC;

    if (CHECK_RIG_ARG(rig))
    {
        RETURNFUNC(-RIG_EINVAL);
    }

    caps = rig->caps;

    if (caps->set_ts == NULL)
    {
        RETURNFUNC(-RIG_ENAVAIL);
    }

    if (vfo == RIG_VFO_CURR
            || vfo == rig->state.current_vfo)
    {
        HAMLIB_TRACE;
        retcode = caps->set_ts(rig, vfo, ts);
        RETURNFUNC(retcode);
    }

    if (!caps->set_vfo)
    {
        RETURNFUNC(-RIG_ENAVAIL);
    }

    curr_vfo = rig->state.current_vfo;
    HAMLIB_TRACE;
    retcode = caps->set_vfo(rig, vfo);

    if (retcode != RIG_OK)
    {
        RETURNFUNC(retcode);
    }

    HAMLIB_TRACE;
    retcode = caps->set_ts(rig, vfo, ts);
    /* try and revert even if we had an error above */
    rc2 = caps->set_vfo(rig, curr_vfo);

    if (RIG_OK == retcode)
    {
        /* return the first error code */
        retcode = rc2;
    }

    RETURNFUNC(retcode);
}


/**
 * \brief get the current Tuning Step
 * \param rig   The rig handle
 * \param vfo   The target VFO
 * \param ts    The location where to store the current tuning step
 *
 *  Retrieves the current tuning step.
 *
 * \return RIG_OK if the operation has been successful, otherwise
 * a negative value if an error occurred (in which case, cause is
 * set appropriately).
 *
 * \sa rig_set_ts()
 */
int HAMLIB_API rig_get_ts(RIG *rig, vfo_t vfo, shortfreq_t *ts)
{
    const struct rig_caps *caps;
    int retcode, rc2;
    vfo_t curr_vfo;

    ENTERFUNC;

    if (CHECK_RIG_ARG(rig))
    {
        RETURNFUNC(-RIG_EINVAL);
    }

    if (!ts)
    {
        RETURNFUNC(-RIG_EINVAL);
    }

    caps = rig->caps;

    if (caps->get_ts == NULL)
    {
        RETURNFUNC(-RIG_ENAVAIL);
    }

    if (vfo == RIG_VFO_CURR
            || vfo == rig->state.current_vfo)
    {
        HAMLIB_TRACE;
        retcode = caps->get_ts(rig, vfo, ts);
        RETURNFUNC(retcode);
    }

    if (!caps->set_vfo)
    {
        RETURNFUNC(-RIG_ENAVAIL);
    }

    curr_vfo = rig->state.current_vfo;
    HAMLIB_TRACE;
    retcode = caps->set_vfo(rig, vfo);

    if (retcode != RIG_OK)
    {
        RETURNFUNC(retcode);
    }

    HAMLIB_TRACE;
    retcode = caps->get_ts(rig, vfo, ts);
    /* try and revert even if we had an error above */
    rc2 = caps->set_vfo(rig, curr_vfo);

    if (RIG_OK == retcode)
    {
        /* return the first error code */
        retcode = rc2;
    }

    RETURNFUNC(retcode);
}


/**
 * \brief set the antenna
 * \param rig   The rig handle
 * \param vfo   The target VFO
 * \param ant   The anntena to select
 * \param option An option that the ant command for the rig recognizes
 *
 *  Select the antenna connector.
\code
    rig_set_ant(rig, RIG_VFO_CURR, RIG_ANT_1);  // apply to both TX&RX
    rig_set_ant(rig, RIG_VFO_RX, RIG_ANT_2);
\endcode
 *
 * \return RIG_OK if the operation has been successful, otherwise
 * a negative value if an error occurred (in which case, cause is
 * set appropriately).
 *
 * \sa rig_get_ant()
 */
int HAMLIB_API rig_set_ant(RIG *rig, vfo_t vfo, ant_t ant, value_t option)
{
    const struct rig_caps *caps;
    int retcode, rc2;
    vfo_t curr_vfo;

    ENTERFUNC;

    if (CHECK_RIG_ARG(rig))
    {
        RETURNFUNC(-RIG_EINVAL);
    }

    caps = rig->caps;

    if (caps->set_ant == NULL)
    {
        RETURNFUNC(-RIG_ENAVAIL);
    }

    if ((caps->targetable_vfo & RIG_TARGETABLE_ANT)
            || vfo == RIG_VFO_CURR
            || vfo == rig->state.current_vfo)
    {
        HAMLIB_TRACE;
        retcode = caps->set_ant(rig, vfo, ant, option);
        RETURNFUNC(retcode);
    }

    if (!caps->set_vfo)
    {
        RETURNFUNC(-RIG_ENAVAIL);
    }

    curr_vfo = rig->state.current_vfo;
    HAMLIB_TRACE;
    retcode = caps->set_vfo(rig, vfo);

    if (retcode != RIG_OK)
    {
        RETURNFUNC(retcode);
    }

    HAMLIB_TRACE;
    retcode = caps->set_ant(rig, vfo, ant, option);
    /* try and revert even if we had an error above */
    rc2 = caps->set_vfo(rig, curr_vfo);

    if (RIG_OK == retcode)
    {
        /* return the first error code */
        retcode = rc2;
    }

    RETURNFUNC(retcode);
}


/**
 * \brief get the current antenna
 * \param rig   The rig handle
 * \param vfo   The target VFO
 * \param ant   The antenna to query option for
 * \param option  The option value for the antenna, rig specific.
 * \param ant_curr  The currently selected antenna
 * \param ant_tx  The currently selected TX antenna
 * \param ant_rx  The currently selected RX antenna
 *
 *  Retrieves the current antenna.
 *
 *  If \a ant_tx and/or \a ant_rx are unused by the rig they will be set to
 *  RIG_ANT_UNKNOWN and only \a ant_curr will be set.
 *
 * \return RIG_OK if the operation has been successful, otherwise
 * a negative value if an error occurred (in which case, cause is
 * set appropriately).
 *
 * \sa rig_set_ant()
 */
int HAMLIB_API rig_get_ant(RIG *rig, vfo_t vfo, ant_t ant, value_t *option,
                           ant_t *ant_curr, ant_t *ant_tx, ant_t *ant_rx)
{
    const struct rig_caps *caps;
    int retcode, rc2;
    vfo_t curr_vfo;

    ENTERFUNC;

    if (CHECK_RIG_ARG(rig))
    {
        RETURNFUNC(-RIG_EINVAL);
    }

    if (ant_curr == NULL || ant_tx == NULL || ant_rx == NULL)
    {
        rig_debug(RIG_DEBUG_ERR,
                  "%s: null pointer in ant_curr=%p, ant_tx=%p, ant_rx=%p\n", __func__, ant_curr,
                  ant_tx, ant_rx);
        RETURNFUNC(-RIG_EINVAL);
    }

    caps = rig->caps;

    if (caps->get_ant == NULL)
    {
        RETURNFUNC(-RIG_ENAVAIL);
    }

    /* Set antenna default to unknown and clear option.
     * So we have sane defaults for all backends */
    *ant_tx = *ant_rx = *ant_curr = RIG_ANT_UNKNOWN;
    option->i = 0;

    if ((caps->targetable_vfo & RIG_TARGETABLE_ANT)
            || vfo == RIG_VFO_CURR
            || vfo == rig->state.current_vfo)
    {
        HAMLIB_TRACE;
        retcode = caps->get_ant(rig, vfo, ant, option, ant_curr, ant_tx, ant_rx);
        RETURNFUNC(retcode);
    }

    if (!caps->set_vfo)
    {
        RETURNFUNC(-RIG_ENAVAIL);
    }

    curr_vfo = rig->state.current_vfo;
    HAMLIB_TRACE;
    retcode = caps->set_vfo(rig, vfo);

    if (retcode != RIG_OK)
    {
        RETURNFUNC(retcode);
    }

    HAMLIB_TRACE;
    retcode = caps->get_ant(rig, vfo, ant, option, ant_curr, ant_tx, ant_rx);
    /* try and revert even if we had an error above */
    rc2 = caps->set_vfo(rig, curr_vfo);

    if (RIG_OK == retcode)
    {
        /* return the first error code */
        retcode = rc2;
    }

    RETURNFUNC(retcode);
}


/**
 * \brief conversion utility from relative range to absolute in mW
 * \param rig   The rig handle
 * \param mwpower   The location where to store the converted power in mW
 * \param power The relative power
 * \param freq  The frequency where the conversion should take place
 * \param mode  The mode where the conversion should take place
 *
 *  Converts a power value expressed in a range on a [0.0 .. 1.0] relative
 *  scale to the real transmit power in milli Watts the radio would emit.
 *  The \a freq and \a mode where the conversion should take place must be
 *  also provided since the relative power is peculiar to a specific
 *  freq and mode range of the radio.
 *
 * \return RIG_OK if the operation has been successful, otherwise
 * a negative value if an error occurred (in which case, cause is
 * set appropriately).
 *
 * \sa rig_mW2power()
 */
int HAMLIB_API rig_power2mW(RIG *rig,
                            unsigned int *mwpower,
                            float power,
                            freq_t freq,
                            rmode_t mode)
{
    const freq_range_t *txrange;

    ENTERFUNC;

    if (!rig || !rig->caps || !mwpower || power < 0.0 || power > 1.0)
    {
        RETURNFUNC(-RIG_EINVAL);
    }

    if (rig->caps->power2mW != NULL)
    {
        RETURNFUNC(rig->caps->power2mW(rig, mwpower, power, freq, mode));
    }

    txrange = rig_get_range(rig->state.tx_range_list, freq, mode);

    // check all the range lists
    if (txrange == NULL) { rig_get_range(rig->caps->tx_range_list1, freq, mode); }

    if (txrange == NULL) { rig_get_range(rig->caps->tx_range_list2, freq, mode); }

    if (txrange == NULL) { rig_get_range(rig->caps->tx_range_list3, freq, mode); }

    if (txrange == NULL) { rig_get_range(rig->caps->tx_range_list4, freq, mode); }

    if (txrange == NULL) { rig_get_range(rig->caps->tx_range_list5, freq, mode); }

    if (txrange == NULL)
    {
        /*
         * freq is not on the tx range!
         */
        rig_debug(RIG_DEBUG_ERR, "%s: freq not in tx range\n", __func__);
        RETURNFUNC(-RIG_EINVAL);
    }

    *mwpower = (unsigned int)(power * txrange->high_power);

    RETURNFUNC(RIG_OK);
}


/**
 * \brief conversion utility from absolute in mW to relative range
 * \param rig   The rig handle
 * \param power The location where to store the converted relative power
 * \param mwpower   The power in mW
 * \param freq  The frequency where the conversion should take place
 * \param mode  The mode where the conversion should take place
 *
 * Converts a power value expressed in the real transmit power in milli Watts
 * the radio would emit to a range on a [0.0 .. 1.0] relative scale.
 * The \a freq and \a mode where the conversion should take place must be
 * also provided since the relative power is peculiar to a specific
 * freq and mode range of the radio.
 *
 * \return RIG_OK if the operation has been successful, otherwise
 * a negative value if an error occurred (in which case, cause is
 * set appropriately).
 *
 * \sa rig_power2mW()
 */
int HAMLIB_API rig_mW2power(RIG *rig,
                            float *power,
                            unsigned int mwpower,
                            freq_t freq,
                            rmode_t mode)
{
    const freq_range_t *txrange;

    if (!rig || !rig->caps || !power || mwpower == 0)
    {
        RETURNFUNC2(-RIG_EINVAL);
    }

    if (rig->caps->mW2power != NULL)
    {
        RETURNFUNC2(rig->caps->mW2power(rig, power, mwpower, freq, mode));
    }

    txrange = rig_get_range(rig->state.tx_range_list, freq, mode);

    if (!txrange)
    {
        /*
         * freq is not on the tx range!
         */
        RETURNFUNC2(-RIG_EINVAL);  /* could be RIG_EINVAL ? */
    }

    if (txrange->high_power == 0)
    {
        *power = 0.0;
        RETURNFUNC2(RIG_OK);
    }

    *power = (float)mwpower / txrange->high_power;

    if (*power > 1.0)
    {
        *power = 1.0;
    }

    RETURNFUNC2(mwpower > txrange->high_power ? RIG_OK : -RIG_ETRUNC);
}


/**
 * \brief get the best frequency resolution of the rig
 * \param rig   The rig handle
 * \param mode  The mode where the conversion should take place
 *
 *  Returns the best frequency resolution of the rig, for a given \a mode.
 *
 * \return the frequency resolution in Hertz if the operation h
 * has been successful, otherwise a negative value if an error occurred.
 *
 */
shortfreq_t HAMLIB_API rig_get_resolution(RIG *rig, rmode_t mode)
{
    const struct rig_state *rs;
    int i;

    ENTERFUNC;

    if (!rig || !rig->caps || !mode)
    {
        RETURNFUNC(-RIG_EINVAL);
    }

    rs = &rig->state;

    for (i = 0; i < HAMLIB_TSLSTSIZ && rs->tuning_steps[i].ts; i++)
    {
        if (rs->tuning_steps[i].modes & mode)
        {
            RETURNFUNC(rs->tuning_steps[i].ts);
        }
    }

    RETURNFUNC(-RIG_EINVAL);
}


/**
 * \brief turn on/off the radio
 * \param rig   The rig handle
 * \param status    The status to set to
 *
 * turns on/off the radio.
 * See #RIG_POWER_ON, #RIG_POWER_OFF and #RIG_POWER_STANDBY defines
 * for the \a status.
 *
 * \return RIG_OK if the operation has been successful, ortherwise
 * a negative value if an error occurred (in which case, cause is
 * set appropriately).
 *
 * \sa rig_get_powerstat()
 */
int HAMLIB_API rig_set_powerstat(RIG *rig, powerstat_t status)
{
    int retcode;

    rig_debug(RIG_DEBUG_VERBOSE, "%s called status=%d\n", __func__, status);

    if (CHECK_RIG_ARG(rig))
    {
        RETURNFUNC(-RIG_EINVAL);
    }

    if (rig->caps->set_powerstat == NULL)
    {
        rig_debug(RIG_DEBUG_WARN, "%s set_powerstat not implemented\n", __func__);
        RETURNFUNC(-RIG_ENAVAIL);
    }

    HAMLIB_TRACE;
    retcode = rig->caps->set_powerstat(rig, status);
    rig_flush(&rig->state.rigport); // if anything is queued up flush it
    RETURNFUNC(retcode);
}


/**
 * \brief get the on/off status of the radio
 * \param rig   The rig handle
 * \param status    The location where to store the current status
 *
 *  Retrieve the status of the radio. See RIG_POWER_ON, RIG_POWER_OFF and
 *  RIG_POWER_STANDBY defines for the \a status.
 *
 * \return RIG_OK if the operation has been successful, otherwise
 * a negative value if an error occurred (in which case, cause is
 * set appropriately).
 *
 * \sa rig_set_powerstat()
 */
int HAMLIB_API rig_get_powerstat(RIG *rig, powerstat_t *status)
{
    int retcode;

    ENTERFUNC;

    if (CHECK_RIG_ARG(rig))
    {
        *status = RIG_POWER_ON; // default to power on if not available
        RETURNFUNC(-RIG_EINVAL);
    }

    if (!status)
    {
        RETURNFUNC(-RIG_EINVAL);
    }

    if (rig->caps->get_powerstat == NULL)
    {
        *status = RIG_POWER_ON; // default to power if not available
        RETURNFUNC(RIG_OK);
    }

    *status = RIG_POWER_OFF; // default now to power off until proven otherwise in get_powerstat
    HAMLIB_TRACE;
    retcode = rig->caps->get_powerstat(rig, status);
    RETURNFUNC(retcode);
}


/**
 * \brief reset the radio
 * \param rig   The rig handle
 * \param reset The reset operation to perform
 *
 *  Resets the radio.
 *  See #RIG_RESET_NONE, #RIG_RESET_SOFT and #RIG_RESET_MCALL defines
 *  for the \a reset.
 *
 * \return RIG_OK if the operation has been successful, otherwise
 * a negative value if an error occurred (in which case, cause is
 * set appropriately).
 *
 */
int HAMLIB_API rig_reset(RIG *rig, reset_t reset)
{
    int retcode;

    ENTERFUNC;

    if (CHECK_RIG_ARG(rig))
    {
        RETURNFUNC(-RIG_EINVAL);
    }

    if (rig->caps->reset == NULL)
    {
        RETURNFUNC(-RIG_ENAVAIL);
    }

    retcode = rig->caps->reset(rig, reset);
    RETURNFUNC(retcode);
}


//! @cond Doxygen_Suppress
extern int rig_probe_first(hamlib_port_t *p);

extern int rig_probe_all_backends(hamlib_port_t *p,
                                  rig_probe_func_t cfunc,
                                  rig_ptr_t data);
//! @endcond


/**
 * \brief try to guess a rig
 * \param port      A pointer describing a port linking the host to the rig
 *
 *  Try to guess what is the model of the first rig attached to the port.
 *  It can be very buggy, and mess up the radio at the other end.
 *  (but fun if it works!)
 *
 * \warning this is really Experimental, It has been tested only
 * with IC-706MkIIG. any feedback welcome! --SF
 *
 * \return the rig model id according to the rig_model_t type if found,
 * otherwise RIG_MODEL_NONE if unable to determine rig model.
 */
rig_model_t HAMLIB_API rig_probe(hamlib_port_t *port)
{
    if (!port)
    {
        return (RIG_MODEL_NONE);
    }

    return rig_probe_first(port);
}


/**
 * \brief try to guess rigs
 * \param port  A pointer describing a port linking the host to the rigs
 * \param cfunc Function to be called each time a rig is found
 * \param data  Arbitrary data passed to cfunc
 *
 *  Try to guess what are the model of all rigs attached to the port.
 *  It can be very buggy, and mess up the radio at the other end.
 *  (but fun if it works!)
 *
 * \warning this is really Experimental, It has been tested only
 * with IC-706MkIIG. any feedback welcome! --SF
 *
 * \return RIG_OK if the operation has been successful, otherwise
 * a negative value if an error occurred (in which case, cause is
 * set appropriately).
 */
int HAMLIB_API rig_probe_all(hamlib_port_t *port,
                             rig_probe_func_t cfunc,
                             rig_ptr_t data)
{
    if (!port)
    {
        return (-RIG_EINVAL);
    }

    return rig_probe_all_backends(port, cfunc, data);
}


/**
 * \brief check retrieval ability of VFO operations
 * \param rig   The rig handle
 * \param op    The VFO op
 *
 *  Checks if a rig is capable of executing a VFO operation.
 *  Since the \a op is an OR'ed bitmap argument, more than
 *  one op can be checked at the same time.
 *
 *  EXAMPLE: if (rig_has_vfo_op(my_rig, RIG_OP_CPY)) disp_VFOcpy_btn();
 *
 * \return a bit map mask of supported op settings that can be retrieved,
 * otherwise 0 if none supported.
 *
 * \sa rig_vfo_op()
 */
vfo_op_t HAMLIB_API rig_has_vfo_op(RIG *rig, vfo_op_t op)
{
    int retcode;

    ENTERFUNC;

    if (!rig || !rig->caps)
    {
        RETURNFUNC(0);
    }

    retcode = rig->caps->vfo_ops & op;
    RETURNFUNC(retcode);
}


/**
 * \brief perform Memory/VFO operations
 * \param rig   The rig handle
 * \param vfo   The target VFO
 * \param op    The Memory/VFO operation to perform
 *
 *  Performs Memory/VFO operation.
 *  See #vfo_op_t for more information.
 *
 * \return RIG_OK if the operation has been successful, otherwise
 * a negative value if an error occurred (in which case, cause is
 * set appropriately).
 *
 * \sa rig_has_vfo_op()
 */
int HAMLIB_API rig_vfo_op(RIG *rig, vfo_t vfo, vfo_op_t op)
{
    const struct rig_caps *caps;
    int retcode, rc2;
    vfo_t curr_vfo;

    ENTERFUNC;
    ELAPSED1;

    if (CHECK_RIG_ARG(rig))
    {
        ELAPSED2;
        RETURNFUNC(-RIG_EINVAL);
    }

    caps = rig->caps;

    if (caps->vfo_op == NULL || rig_has_vfo_op(rig, op) == 0)
    {
        rig_debug(RIG_DEBUG_WARN, "%s: vfo_op=%p, has_vfo_op=%d\n", __func__,
                  caps->vfo_op, rig_has_vfo_op(rig, op));
        ELAPSED2;
        RETURNFUNC(-RIG_ENAVAIL);
    }

    if (vfo == RIG_VFO_CURR
            || vfo == rig->state.current_vfo)
    {
        retcode = caps->vfo_op(rig, vfo, op);
        ELAPSED2;
        RETURNFUNC(retcode);
    }

    if (!caps->set_vfo)
    {
        rig_debug(RIG_DEBUG_WARN, "%s: no set_vfo\n", __func__);
        ELAPSED2;
        RETURNFUNC(-RIG_ENAVAIL);
    }

    curr_vfo = rig->state.current_vfo;
    HAMLIB_TRACE;
    retcode = caps->set_vfo(rig, vfo);

    if (retcode != RIG_OK)
    {
        ELAPSED2;
        RETURNFUNC(retcode);
    }

    retcode = caps->vfo_op(rig, vfo, op);
    /* try and revert even if we had an error above */
    HAMLIB_TRACE;
    rc2 = caps->set_vfo(rig, curr_vfo);

    if (RIG_OK == retcode)
    {
        /* return the first error code */
        retcode = rc2;
    }

    ELAPSED2;
    RETURNFUNC(retcode);
}


/**
 * \brief check availability of scanning functions
 * \param rig   The rig handle
 * \param scan  The scan op
 *
 *  Checks if a rig is capable of performing a scan operation.
 *  Since the \a scan parameter is an OR'ed bitmap argument, more than
 *  one op can be checked at the same time.
 *
 *  EXAMPLE: if (rig_has_scan(my_rig, RIG_SCAN_PRIO)) disp_SCANprio_btn();
 *
 * \return a bit map of supported scan settings that can be retrieved,
 * otherwise 0 if none supported.
 *
 * \sa rig_scan()
 */
scan_t HAMLIB_API rig_has_scan(RIG *rig, scan_t scan)
{
    int retcode;

    ENTERFUNC;

    if (!rig || !rig->caps)
    {
        RETURNFUNC(0);
    }

    retcode = rig->caps->scan_ops & scan;
    RETURNFUNC(retcode);
}


/**
 * \brief perform Memory/VFO operations
 * \param rig   The rig handle
 * \param vfo   The target VFO
 * \param scan  The scanning operation to perform
 * \param ch    Optional channel argument used for the scan.
 *
 *  Performs scanning operation.
 *  See #scan_t for more information.
 *
 * \return RIG_OK if the operation has been successful, otherwise
 * a negative value if an error occurred (in which case, cause is
 * set appropriately).
 *
 * \sa rig_has_scan()
 */
int HAMLIB_API rig_scan(RIG *rig, vfo_t vfo, scan_t scan, int ch)
{
    const struct rig_caps *caps;
    int retcode, rc2;
    vfo_t curr_vfo;

    ENTERFUNC;

    if (CHECK_RIG_ARG(rig))
    {
        RETURNFUNC(-RIG_EINVAL);
    }

    caps = rig->caps;

    if (caps->scan == NULL
            || (scan != RIG_SCAN_STOP && !rig_has_scan(rig, scan)))
    {
        RETURNFUNC(-RIG_ENAVAIL);
    }

    if (vfo == RIG_VFO_CURR
            || vfo == rig->state.current_vfo)
    {
        retcode = caps->scan(rig, vfo, scan, ch);
        RETURNFUNC(retcode);
    }

    if (!caps->set_vfo)
    {
        RETURNFUNC(-RIG_ENAVAIL);
    }

    curr_vfo = rig->state.current_vfo;
    HAMLIB_TRACE;
    retcode = caps->set_vfo(rig, vfo);

    if (retcode != RIG_OK)
    {
        RETURNFUNC(retcode);
    }

    retcode = caps->scan(rig, vfo, scan, ch);
    /* try and revert even if we had an error above */
    HAMLIB_TRACE;
    rc2 = caps->set_vfo(rig, curr_vfo);

    if (RIG_OK == retcode)
    {
        /* return the first error code */
        retcode = rc2;
    }

    RETURNFUNC(retcode);
}


/**
 * \brief send DTMF digits
 * \param rig   The rig handle
 * \param vfo   The target VFO
 * \param digits    Digits to be send
 *
 *  Sends DTMF digits.
 *  See DTMF change speed, etc. (TODO).
 *
 * \return RIG_OK if the operation has been successful, otherwise
 * a negative value if an error occurred (in which case, cause is
 * set appropriately).
 *
 */
int HAMLIB_API rig_send_dtmf(RIG *rig, vfo_t vfo, const char *digits)
{
    const struct rig_caps *caps;
    int retcode, rc2;
    vfo_t curr_vfo;

    ENTERFUNC;

    if (CHECK_RIG_ARG(rig))
    {
        RETURNFUNC(-RIG_EINVAL);
    }

    if (!digits)
    {
        RETURNFUNC(-RIG_EINVAL);
    }

    caps = rig->caps;

    if (caps->send_dtmf == NULL)
    {
        RETURNFUNC(-RIG_ENAVAIL);
    }

    if (vfo == RIG_VFO_CURR
            || vfo == rig->state.current_vfo)
    {
        retcode = caps->send_dtmf(rig, vfo, digits);
        RETURNFUNC(retcode);
    }

    if (!caps->set_vfo)
    {
        RETURNFUNC(-RIG_ENAVAIL);
    }

    curr_vfo = rig->state.current_vfo;
    HAMLIB_TRACE;
    retcode = caps->set_vfo(rig, vfo);

    if (retcode != RIG_OK)
    {
        RETURNFUNC(retcode);
    }

    retcode = caps->send_dtmf(rig, vfo, digits);
    /* try and revert even if we had an error above */
    HAMLIB_TRACE;
    rc2 = caps->set_vfo(rig, curr_vfo);

    if (RIG_OK == retcode)
    {
        /* return the first error code */
        retcode = rc2;
    }

    RETURNFUNC(retcode);
}


/**
 * \brief receive DTMF digits
 * \param rig   The rig handle
 * \param vfo   The target VFO
 * \param digits    Location where the digits are to be stored
 * \param length    in: max length of buffer, out: number really read.
 *
 *  Receives DTMF digits (not blocking).
 *  See DTMF change speed, etc. (TODO).
 *
 * \return RIG_OK if the operation has been successful, otherwise
 * a negative value if an error occurred (in which case, cause is
 * set appropriately).
 *
 */
int HAMLIB_API rig_recv_dtmf(RIG *rig, vfo_t vfo, char *digits, int *length)
{
    const struct rig_caps *caps;
    int retcode, rc2;
    vfo_t curr_vfo;

    ENTERFUNC;

    if (CHECK_RIG_ARG(rig))
    {
        RETURNFUNC(-RIG_EINVAL);
    }

    if (!digits || !length)
    {
        RETURNFUNC(-RIG_EINVAL);
    }

    caps = rig->caps;

    if (caps->recv_dtmf == NULL)
    {
        RETURNFUNC(-RIG_ENAVAIL);
    }

    if (vfo == RIG_VFO_CURR
            || vfo == rig->state.current_vfo)
    {
        retcode = caps->recv_dtmf(rig, vfo, digits, length);
        RETURNFUNC(retcode);
    }

    if (!caps->set_vfo)
    {
        RETURNFUNC(-RIG_ENAVAIL);
    }

    curr_vfo = rig->state.current_vfo;
    HAMLIB_TRACE;
    retcode = caps->set_vfo(rig, vfo);

    if (retcode != RIG_OK)
    {
        RETURNFUNC(retcode);
    }

    retcode = caps->recv_dtmf(rig, vfo, digits, length);
    /* try and revert even if we had an error above */
    HAMLIB_TRACE;
    rc2 = caps->set_vfo(rig, curr_vfo);

    if (RIG_OK == retcode)
    {
        /* return the first error code */
        retcode = rc2;
    }

    RETURNFUNC(retcode);
}


/**
 * \brief send morse code
 * \param rig   The rig handle
 * \param vfo   The target VFO
 * \param msg   Message to be sent
 *
 *  Sends morse message.
 *  See keyer change speed, etc. (TODO).
 *
 * \return RIG_OK if the operation has been successful, otherwise
 * a negative value if an error occurred (in which case, cause is
 * set appropriately).
 *
 */
int HAMLIB_API rig_send_morse(RIG *rig, vfo_t vfo, const char *msg)
{
    const struct rig_caps *caps;
    int retcode, rc2;
    vfo_t curr_vfo;

    ENTERFUNC;

    if (CHECK_RIG_ARG(rig))
    {
        RETURNFUNC(-RIG_EINVAL);
    }

    if (!msg)
    {
        RETURNFUNC(-RIG_EINVAL);
    }

    caps = rig->caps;

    if (caps->send_morse == NULL)
    {
        RETURNFUNC(-RIG_ENAVAIL);
    }

    if (vfo == RIG_VFO_CURR
            || vfo == rig->state.current_vfo)
    {
        retcode = caps->send_morse(rig, vfo, msg);
        RETURNFUNC(retcode);
    }

    if (!caps->set_vfo)
    {
        RETURNFUNC(-RIG_ENAVAIL);
    }

    curr_vfo = rig->state.current_vfo;
    HAMLIB_TRACE;
    retcode = caps->set_vfo(rig, vfo);

    if (retcode != RIG_OK)
    {
        RETURNFUNC(retcode);
    }

    retcode = caps->send_morse(rig, vfo, msg);
    /* try and revert even if we had an error above */
    HAMLIB_TRACE;
    rc2 = caps->set_vfo(rig, curr_vfo);

    if (RIG_OK == retcode)
    {
        /* return the first error code */
        retcode = rc2;
    }

    RETURNFUNC(retcode);
}

/**
 * \brief stop morse code
 * \param rig   The rig handle
 * \param vfo   The target VFO
 *
 *  Stops the send morse message.
 *
 * \return RIG_OK if the operation has been successful, otherwise
 * a negative value if an error occurred (in which case, cause is
 * set appropriately).
 *
 */
int HAMLIB_API rig_stop_morse(RIG *rig, vfo_t vfo)
{
    const struct rig_caps *caps;
    int retcode, rc2;
    vfo_t curr_vfo;

    ENTERFUNC;

    if (CHECK_RIG_ARG(rig))
    {
        RETURNFUNC(-RIG_EINVAL);
    }

    caps = rig->caps;

    if (caps->stop_morse == NULL)
    {
        RETURNFUNC(-RIG_ENAVAIL);
    }

    if (vfo == RIG_VFO_CURR
            || vfo == rig->state.current_vfo)
    {
        RETURNFUNC(caps->stop_morse(rig, vfo));
    }

    if (!caps->set_vfo)
    {
        RETURNFUNC(-RIG_ENAVAIL);
    }

    curr_vfo = rig->state.current_vfo;
    HAMLIB_TRACE;
    retcode = caps->set_vfo(rig, vfo);

    if (retcode != RIG_OK)
    {
        RETURNFUNC(retcode);
    }

    retcode = caps->stop_morse(rig, vfo);
    /* try and revert even if we had an error above */
    HAMLIB_TRACE;
    rc2 = caps->set_vfo(rig, curr_vfo);

    if (RIG_OK == retcode)
    {
        /* return the first error code */
        retcode = rc2;
    }

    RETURNFUNC(retcode);
}

/*
 * wait_morse_ptt
 * generic routine to wait for ptt=0
 * should work on any full breakin CW morse send
 * Assumes rig!=NULL, msg!=NULL
 */
static int wait_morse_ptt(RIG *rig, vfo_t vfo)
{
    ptt_t pttStatus = RIG_PTT_OFF;
    int loops = 0;

    ENTERFUNC;

    hl_usleep(200 * 1000); // give little time for CW to start PTT

    do
    {
        int retval;
        rig_debug(RIG_DEBUG_TRACE, "%s: loop#%d until ptt=0, ptt=%d\n", __func__, loops,
                  pttStatus);
        elapsed_ms(&rig->state.cache.time_ptt, HAMLIB_ELAPSED_INVALIDATE);
        HAMLIB_TRACE;
        retval = rig_get_ptt(rig, vfo, &pttStatus);

        if (retval != RIG_OK)
        {
            RETURNFUNC(retval);
        }

        // every 25ms should be short enough
        hl_usleep(25 * 1000);
        ++loops;
    }
    while (pttStatus == RIG_PTT_ON && loops <= 600);

    RETURNFUNC(RIG_OK);
}

/**
 * \brief wait morse code
 * \param rig   The rig handle
 * \param vfo   The target VFO
 *
 *  waits for the end of the morse message to be sent.
 *
 * \return RIG_OK if the operation has been successful, otherwise
 * a negative value if an error occurred (in which case, cause is
 * set appropriately).
 *
 */
int HAMLIB_API rig_wait_morse(RIG *rig, vfo_t vfo)
{
    const struct rig_caps *caps;
    int retcode, rc2;
    vfo_t curr_vfo;

    ENTERFUNC;

    if (CHECK_RIG_ARG(rig))
    {
        RETURNFUNC(-RIG_EINVAL);
    }

    caps = rig->caps;

    if (vfo == RIG_VFO_CURR
            || vfo == rig->state.current_vfo)
    {
        RETURNFUNC(wait_morse_ptt(rig, vfo));
    }

    if (!caps->set_vfo)
    {
        RETURNFUNC(-RIG_ENAVAIL);
    }

    curr_vfo = rig->state.current_vfo;
    HAMLIB_TRACE;
    retcode = caps->set_vfo(rig, vfo);

    if (retcode != RIG_OK)
    {
        RETURNFUNC(retcode);
    }

    retcode = wait_morse_ptt(rig, vfo);
    /* try and revert even if we had an error above */
    HAMLIB_TRACE;
    rc2 = caps->set_vfo(rig, curr_vfo);

    if (RIG_OK == retcode)
    {
        /* return the first error code */
        retcode = rc2;
    }

    RETURNFUNC(retcode);
}


/**
 * \brief send voice memory content
 * \param rig   The rig handle
 * \param vfo   The target VFO
 * \param ch    Voice memory number to be sent
 *
 *  Sends voice memory content.
 *
 * \return RIG_OK if the operation has been successful, otherwise
 * a negative value if an error occurred (in which case, cause is
 * set appropriately).
 *
 */

int HAMLIB_API rig_send_voice_mem(RIG *rig, vfo_t vfo, int ch)
{
    const struct rig_caps *caps;
    int retcode, rc2;
    vfo_t curr_vfo;

    ENTERFUNC;

    if CHECK_RIG_ARG(rig)
    {
        RETURNFUNC(-RIG_EINVAL);
    }

    caps = rig->caps;

    if (caps->send_voice_mem == NULL)
    {
        RETURNFUNC(-RIG_ENAVAIL);
    }

    if (vfo == RIG_VFO_CURR
            || vfo == rig->state.current_vfo)
    {
        retcode = caps->send_voice_mem(rig, vfo, ch);
        RETURNFUNC(retcode);
    }

    if (!caps->set_vfo)
    {
        RETURNFUNC(-RIG_ENAVAIL);
    }

    curr_vfo = rig->state.current_vfo;
    HAMLIB_TRACE;
    retcode = caps->set_vfo(rig, vfo);

    if (retcode != RIG_OK)
    {
        RETURNFUNC(retcode);
    }

    retcode = caps->send_voice_mem(rig, vfo, ch);
    /* try and revert even if we had an error above */
    HAMLIB_TRACE;
    rc2 = caps->set_vfo(rig, curr_vfo);

    if (RIG_OK == retcode)
    {
        /* return the first error code */
        retcode = rc2;
    }

    RETURNFUNC(retcode);
}


/**
 * \brief find the freq_range of freq/mode
 * \param range_list    The range list to search from
 * \param freq  The frequency that will be part of this range
 * \param mode  The mode that will be part of this range
 *
 *  Returns a pointer to the #freq_range_t including \a freq and \a mode.
 *  Works for rx and tx range list as well.
 *
 * \return the location of the #freq_range_t if found,
 * otherwise NULL if not found or if \a range_list is invalid.
 *
 */
const freq_range_t *HAMLIB_API rig_get_range(const freq_range_t *range_list,
        freq_t freq,
        rmode_t mode)
{
    int i;

    if (range_list == NULL)
    {
        return (NULL);
    }

    for (i = 0; i < HAMLIB_FRQRANGESIZ; i++)
    {
        if (range_list[i].startf == 0 && range_list[i].endf == 0)
        {
            return (NULL);
        }

        if (freq >= range_list[i].startf && freq <= range_list[i].endf &&
                (range_list[i].modes & mode))
        {
            const freq_range_t *f = &range_list[i];
            return (f);
        }
    }

    return (NULL);
}

/**
 * \brief set the vfo option for rigctld
 * \param status 1=On, 0=Off
 *
 *  Returns RIG_OK or -RIG_EPROTO;
 *
 */
int HAMLIB_API rig_set_vfo_opt(RIG *rig, int status)
{
    int retcode;

    ENTERFUNC;
    ELAPSED1;

    if CHECK_RIG_ARG(rig)
    {
        ELAPSED2;
        RETURNFUNC(-RIG_EINVAL);
    }

    if (rig->caps->set_vfo_opt == NULL)
    {
        ELAPSED2;
        RETURNFUNC(-RIG_ENAVAIL);
    }

    retcode = rig->caps->set_vfo_opt(rig, status);
    ELAPSED2;
    RETURNFUNC(retcode);
}

/**
 * \brief get general information from the radio
 * \param rig   The rig handle
 *
 * Retrieves some general information from the radio.
 * This can include firmware revision, exact model name, or just nothing.
 *
 * \return a pointer to memory containing the ASCIIZ string
 * if the operation has been successful, otherwise NULL if an error occurred
 * or if get_info is not part of the capabilities.
 */
const char *HAMLIB_API rig_get_info(RIG *rig)
{
    if (CHECK_RIG_ARG(rig))
    {
        return (NULL);
    }

    if (rig->caps->get_info == NULL)
    {
        return (NULL);
    }

    HAMLIB_TRACE;
    return (rig->caps->get_info(rig));
}


static void make_crc_table(unsigned long crcTable[])
{
    unsigned long POLYNOMIAL = 0xEDB88320;
    unsigned char b = 0;

    do
    {
        // Start with the data byte
        unsigned long remainder = b;

        unsigned long bit;

        for (bit = 8; bit > 0; --bit)
        {
            if (remainder & 1)
            {
                remainder = (remainder >> 1) ^ POLYNOMIAL;
            }
            else
            {
                remainder = (remainder >> 1);
            }
        }

        crcTable[(size_t)b] = remainder;
    }
    while (0 != ++b);
}

static unsigned long crcTable[256];

static unsigned long gen_crc(unsigned char *p, size_t n)
{
    unsigned long crc = 0xfffffffful;
    size_t i;

    if (crcTable[0] == 0) { make_crc_table(crcTable); }

    for (i = 0; i < n; i++)
    {
        crc = crcTable[*p++ ^ (crc & 0xff)] ^ (crc >> 8);
    }

    return ((~crc) & 0xffffffff);
}

/**
 * \brief get freq/mode/width for requested VFO
 * \param rig   The rig handle
 *
 * returns a string for all known VFOs plus rig split status and satellite mode status
 */
int HAMLIB_API rig_get_rig_info(RIG *rig, char *response, int max_response_len)
{
    vfo_t vfoA, vfoB;
    freq_t freqA, freqB;
    rmode_t modeA, modeB;
    char *modeAstr, *modeBstr;
    pbwidth_t widthA, widthB;
    split_t split;
    int satmode;
    int ret;
    int rxa, txa, rxb, txb;
    response[0] = 0;

    if (CHECK_RIG_ARG(rig) || !response)
    {
        RETURNFUNC2(-RIG_EINVAL);
    }

    ELAPSED1;

    vfoA = vfo_fixup(rig, RIG_VFO_A, rig->state.cache.split);
    vfoB = vfo_fixup(rig, RIG_VFO_B, rig->state.cache.split);
    ret = rig_get_vfo_info(rig, vfoA, &freqA, &modeA, &widthA, &split, &satmode);

    if (ret != RIG_OK)
    {
        ELAPSED2;
        RETURNFUNC2(ret);
    }

    // we need both vfo and mode targetable to avoid vfo swapping
    if ((rig->caps->targetable_vfo & RIG_TARGETABLE_FREQ)
            && (rig->caps->targetable_vfo & RIG_TARGETABLE_MODE))
    {
        ret = rig_get_vfo_info(rig, vfoB, &freqB, &modeB, &widthB, &split, &satmode);

        if (ret != RIG_OK)
        {
            ELAPSED2;
            RETURNFUNC2(ret);
        }
    }
    else
    {
        // we'll use cached info instead of doing the vfo swapping
        int cache_ms_freq, cache_ms_mode, cache_ms_width;
        rig_get_cache(rig, vfoB, &freqB, &cache_ms_freq, &modeB, &cache_ms_mode,
                      &widthB,
                      &cache_ms_width);
    }

    modeAstr = (char *)rig_strrmode(modeA);
    modeBstr = (char *)rig_strrmode(modeB);

    if (modeAstr[0] == 0) { modeAstr = "None"; }

    if (modeBstr[0] == 0) { modeBstr = "None"; }

    rxa = 1;
    txa = split == 0;
    rxb = !rxa;
    txb = split == 1;
    SNPRINTF(response, max_response_len - strlen("CRC=0x00000000\n"),
             "VFO=%s Freq=%.0f Mode=%s Width=%d RX=%d TX=%d\nVFO=%s Freq=%.0f Mode=%s Width=%d RX=%d TX=%d\nSplit=%d SatMode=%d\nRig=%s\nApp=Hamlib\nVersion=20210506 1.0.0\n",
             rig_strvfo(vfoA), freqA, modeAstr, (int)widthA, rxa, txa, rig_strvfo(vfoB),
             freqB, modeBstr, (int)widthB, rxb, txb, split, satmode, rig->caps->model_name);
    unsigned long crc = gen_crc((unsigned char *)response, strlen(response));
    char tmpstr[32];
    SNPRINTF(tmpstr, sizeof(tmpstr), "CRC=0x%08lx\n", crc);
    strcat(response, tmpstr);


    if (strlen(response) >= max_response_len - 1)
    {
        rig_debug(RIG_DEBUG_ERR, "%s(%d): response len exceeded max %d chars\n",
                  __FILE__, __LINE__, max_response_len);
        ELAPSED2;
        RETURNFUNC2(RIG_EINTERNAL);
    }

    ELAPSED2;
    RETURNFUNC2(RIG_OK);
}

/**
 * \brief get freq/mode/width for requested VFO
 * \param rig   The rig handle
 * \param vfo   The VFO to get
 * \param *freq frequency answer
 * \param *mode mode answer
 * \param *width bandwidth answer
 *
 *  Gets the current VFO information. The VFO can be RIG_VFO_A, RIG_VFO_B, RIG_VFO_C
 *  for VFOA, VFOB, VFOC respectively or RIG_VFO_MEM for Memory mode.
 *  Supported VFOs depends on rig capabilities.
 *
 * \return RIG_OK if the operation has been successful, otherwise
 * a negative value if an error occurred (in which case use rigerror(return)
 * for error message).
 *
 */
int HAMLIB_API rig_get_vfo_info(RIG *rig, vfo_t vfo, freq_t *freq,
                                rmode_t *mode, pbwidth_t *width, split_t *split, int *satmode)
{
    int retval;

    ELAPSED1;
    ENTERFUNC;
    rig_debug(RIG_DEBUG_VERBOSE, "%s called vfo=%s\n", __func__, rig_strvfo(vfo));

    if (CHECK_RIG_ARG(rig))
    {
        ELAPSED2;
        RETURNFUNC(-RIG_EINVAL);
    }

    //if (vfo == RIG_VFO_CURR) { vfo = rig->state.current_vfo; }

    vfo = vfo_fixup(rig, vfo, rig->state.cache.split);
    // we can't use the cached values as some clients may only call this function
    // like Log4OM which mostly does polling
    HAMLIB_TRACE;
    retval = rig_get_freq(rig, vfo, freq);

    if (retval != RIG_OK) { RETURNFUNC(retval); }

    // we will ask for other vfo mode just once if not targetable
    int allTheTimeA = vfo & (RIG_VFO_A | RIG_VFO_CURR | RIG_VFO_MAIN_A |
                             RIG_VFO_SUB_A);
    int allTheTimeB = (vfo & (RIG_VFO_B | RIG_VFO_SUB))
                      && (rig->caps->targetable_vfo & RIG_TARGETABLE_MODE);
    int justOnceB = (vfo & (RIG_VFO_B | RIG_VFO_SUB))
                    && (rig->state.cache.modeMainB == RIG_MODE_NONE);

    if (allTheTimeA || allTheTimeB || justOnceB)
    {
        HAMLIB_TRACE;
        retval = rig_get_mode(rig, vfo, mode, width);

        if (retval != RIG_OK)
        {
            ELAPSED2;
            RETURNFUNC(retval);
        }
    }
    else // we'll just us VFOA so we don't swap vfos -- freq is what's important
    {
        *mode = rig->state.cache.modeMainA;
        *width = rig->state.cache.widthMainA;
    }

    *satmode = rig->state.cache.satmode;
    // we should only need to ask for VFO_CURR to minimize display swapping
    HAMLIB_TRACE;
    vfo_t tx_vfo;
    retval = rig_get_split_vfo(rig, RIG_VFO_CURR, split, &tx_vfo);

    if (retval != RIG_OK)
    {
        ELAPSED2;
        RETURNFUNC(retval);
    }

    ELAPSED2;
    RETURNFUNC(RIG_OK);
}

/**
 * \brief get list of available vfos
 * \param rig   The rig handle
 * \param char*  char buffer[SPRINTF_MAX_SIZE] to hold result
 * \param len   max length of char buffer
 *
 * Retrieves all usable vfo entries for the rig
 *
 * \return a pointer to a string, e.g. "VFOA VFOB Mem"
 * if the operation has been successful, otherwise NULL if an error occurred
 */
int HAMLIB_API rig_get_vfo_list(RIG *rig, char *buf, int buflen)
{
    ENTERFUNC;

    if (CHECK_RIG_CAPS(rig))
    {
        RETURNFUNC(-RIG_EINVAL);
    }

    rig_sprintf_vfo(buf, buflen - 1, rig->state.vfo_list);

    RETURNFUNC(RIG_OK);
}

/**
 * \brief set the rig's clock
 *
 */
int HAMLIB_API rig_set_clock(RIG *rig, int year, int month, int day, int hour,
                             int min, int sec, double msec, int utc_offset)
{
    if (rig->caps->set_clock == NULL)
    {
        return -RIG_ENIMPL;
    }

    RETURNFUNC2(rig->caps->set_clock(rig, year, month, day, hour, min, sec,
                                     msec, utc_offset));
}

/**
 * \brief get the rig's clock
 *
 */
int HAMLIB_API rig_get_clock(RIG *rig, int *year, int *month, int *day,
                             int *hour,
                             int *min, int *sec, double *msec, int *utc_offset)
{
    int retval;

    if (rig->caps->get_clock == NULL)
    {
        return -RIG_ENIMPL;
    }

    retval = rig->caps->get_clock(rig, year, month, day, hour, min, sec,
                                  msec, utc_offset);
    RETURNFUNC2(retval);
}

/**
 * \brief get the Hamlib license
 *
 */
const char *HAMLIB_API rig_license()
{
    return hamlib_license;
}


/**
 * \brief get the Hamlib version
 *
 */
const char *HAMLIB_API rig_version()
{
    return hamlib_version2;
}


/**
 * \brief get the Hamlib copyright
 *
 */
const char *HAMLIB_API rig_copyright()
{
    return hamlib_copyright2;
}

/**
 * \brief get a cookie to grab rig control
 * \param rig   Not used
 * \param cookie_cmd The command to execute on \a cookie.
 * \param cookie     The cookie to operate on, cannot be NULL or RIG_EINVAL will be returned.
 * \param cookie_len The length of the cookie, must be #HAMLIB_COOKIE_SIZE or larger.
 *
 * #RIG_COOKIE_GET will set \a cookie with a cookie.
 * #RIG_COOKIE_RENEW will update the timeout with 1 second.
 * #RIG_COOKIE_RELEASE will release the cookie and allow a new one to be grabbed.
 *
 * Cookies should only be used when needed to keep commands sequenced correctly
 * For example, when setting both VFOA and VFOB frequency and mode
 * Example to wait for cookie, do rig commands, and release
 * \code
 *  while((rig_cookie(NULL, RIG_COOKIE_GET, cookie, sizeof(cookie))) != RIG_OK)
 *      hl_usleep(10*1000);
 *
 *  //Pseudo code
 *  set_freq A;set mode A;set freq B;set modeB;
 *
 *  rig_cookie(NULL, RIG_COOKIE_RELEASE, cookie, sizeof(cookie)));
 * \endcode
 */
int HAMLIB_API rig_cookie(RIG *rig, enum cookie_e cookie_cmd, char *cookie,
                          int cookie_len)
{
    // only 1 client can have the cookie so these can be static
    // this should also prevent problems with DLLs & shared libraries
    // the debug_msg is another non-thread-safe which this will help fix
    static char
    cookie_save[HAMLIB_COOKIE_SIZE];  // only one client can have the cookie
    static double time_last_used;
    struct timespec tp;
    int ret;
    MUTEX(mutex_rig_cookie);

    /* This is not needed for RIG_COOKIE_RELEASE but keep it simple. */
    if (cookie_len < HAMLIB_COOKIE_SIZE)
    {
        rig_debug(RIG_DEBUG_ERR, "%s(%d): cookie_len < %d\n",
                  __FILE__, __LINE__, HAMLIB_COOKIE_SIZE);
        return -RIG_EINVAL;
    }

    if (!cookie)
    {
        rig_debug(RIG_DEBUG_ERR, "%s(%d): cookie == NULL\n",
                  __FILE__, __LINE__);
        return -RIG_EINVAL; // nothing to do
    }

    /* Accessing cookie_save and time_last_used must be done with lock held.
     * So keep code simple and lock it during the whole operation. */
    MUTEX_LOCK(mutex_rig_cookie);

    switch (cookie_cmd)
    {
    case RIG_COOKIE_RELEASE:
        if (cookie_save[0] != 0
                && strcmp(cookie, cookie_save) == 0) // matching cookie so we'll clear it
        {
            rig_debug(RIG_DEBUG_VERBOSE, "%s(%d): %s cookie released\n",
                      __FILE__, __LINE__, cookie_save);
            memset(cookie_save, 0, sizeof(cookie_save));
            ret = RIG_OK;
        }
        else // not the right cookie!!
        {
            rig_debug(RIG_DEBUG_ERR,
                      "%s(%d): %s can't release cookie as cookie %s is active\n", __FILE__, __LINE__,
                      cookie, cookie_save);
            ret = -RIG_BUSBUSY;
        }

        break;

    case RIG_COOKIE_RENEW:
        rig_debug(RIG_DEBUG_VERBOSE, "%s(%d): %s comparing renew request to %s==%d\n",
                  __FILE__, __LINE__, cookie, cookie_save, strcmp(cookie, cookie_save));

        if (cookie_save[0] != 0
                && strcmp(cookie, cookie_save) == 0) // matching cookie so we'll renew it
        {
            rig_debug(RIG_DEBUG_VERBOSE, "%s(%d) %s renew request granted\n", __FILE__,
                      __LINE__, cookie);
            clock_gettime(CLOCK_REALTIME, &tp);
            time_last_used = tp.tv_sec + tp.tv_nsec / 1e9;
            ret = RIG_OK;
        }
        else
        {
            rig_debug(RIG_DEBUG_ERR,
                      "%s(%d): %s renew request refused %s is active\n",
                      __FILE__, __LINE__, cookie, cookie_save);
            ret = -RIG_EINVAL; // wrong cookie
        }

        break;

    case RIG_COOKIE_GET:

        // the way we expire cookies is if somebody else asks for one and the last renewal is > 1 second ago
        // a polite client will have released the cookie
        // we are just allow for a crashed client that fails to release:q

        clock_gettime(CLOCK_REALTIME, &tp);
        double time_curr = tp.tv_sec + tp.tv_nsec / 1e9;

        if (cookie_save[0] != 0 && (strcmp(cookie_save, cookie) == 0)
                && (time_curr - time_last_used < 1))  // then we will deny the request
        {
            rig_debug(RIG_DEBUG_ERR, "%s(%d): %s cookie is in use\n", __FILE__, __LINE__,
                      cookie_save);
            ret = -RIG_BUSBUSY;
        }
        else
        {
            if (cookie_save[0] != 0)
            {
                rig_debug(RIG_DEBUG_ERR,
                          "%s(%d): %s cookie has expired after %.3f seconds....overriding with new cookie\n",
                          __FILE__, __LINE__, cookie_save, time_curr - time_last_used);
            }

            date_strget(cookie, cookie_len, 0);
            size_t len = strlen(cookie);
            // add on our random number to ensure uniqueness
            // The cookie should never be longer then HAMLIB_COOKIE_SIZE
            SNPRINTF(cookie + len, HAMLIB_COOKIE_SIZE - len, " %d\n", rand());
            strcpy(cookie_save, cookie);
            time_last_used = time_curr;
            rig_debug(RIG_DEBUG_VERBOSE, "%s(%d): %s new cookie request granted\n",
                      __FILE__, __LINE__, cookie_save);
            ret = RIG_OK;
        }

        break;

    default:
        rig_debug(RIG_DEBUG_ERR, "%s(%d): unknown cmd!!\n'", __FILE__, __LINE__);
        ret = -RIG_EPROTO;
        break;
    }

    MUTEX_UNLOCK(mutex_rig_cookie);
    return ret;
}

HAMLIB_EXPORT(void) sync_callback(int lock)
{
#ifdef HAVE_PTHREAD
    static pthread_mutex_t client_lock = PTHREAD_MUTEX_INITIALIZER;

    if (lock)
    {
        pthread_mutex_lock(&client_lock);
        rig_debug(RIG_DEBUG_VERBOSE, "%s: client lock engaged\n", __func__);
    }
    else
    {
        rig_debug(RIG_DEBUG_VERBOSE, "%s: client lock disengaged\n", __func__);
        pthread_mutex_unlock(&client_lock);
    }

#endif
}


/*! @} */

#ifdef HAVE_PTHREAD

#define MAX_FRAME_LENGTH 1024

static int async_data_handler_start(RIG *rig)
{
    struct rig_state *rs = &rig->state;
    async_data_handler_priv_data *async_data_handler_priv;

    ENTERFUNC;

    if (!rs->async_data_enabled)
    {
        rig_debug(RIG_DEBUG_TRACE,
                  "%s: async data support disabled since async_data_enabled=%d\n", __func__,
                  rs->async_data_enabled);
        RETURNFUNC(RIG_OK);
    }

#ifdef HAVE_PTHREAD

    rs->async_data_handler_thread_run = 1;
    rs->async_data_handler_priv_data = calloc(1,
                                       sizeof(async_data_handler_priv_data));

    if (rs->async_data_handler_priv_data == NULL)
    {
        RETURNFUNC(-RIG_ENOMEM);
    }

    async_data_handler_priv = (async_data_handler_priv_data *)
                              rs->async_data_handler_priv_data;
    async_data_handler_priv->args.rig = rig;
    int err = pthread_create(&async_data_handler_priv->thread_id, NULL,
                             async_data_handler, &async_data_handler_priv->args);

    if (err)
    {
        rig_debug(RIG_DEBUG_ERR, "%s: pthread_create error: %s\n", __func__,
                  strerror(errno));
        RETURNFUNC(-RIG_EINTERNAL);
    }

#endif // HAVE_PTHREAD

    RETURNFUNC(RIG_OK);
}

static int async_data_handler_stop(RIG *rig)
{
    struct rig_state *rs = &rig->state;
    async_data_handler_priv_data *async_data_handler_priv;

    ENTERFUNC;

#ifdef HAVE_PTHREAD
    rs->async_data_handler_thread_run = 0;

    async_data_handler_priv = (async_data_handler_priv_data *)
                              rs->async_data_handler_priv_data;

    if (async_data_handler_priv != NULL)
    {
        if (async_data_handler_priv->thread_id != 0)
        {
            int err = pthread_join(async_data_handler_priv->thread_id, NULL);

            if (err)
            {
                rig_debug(RIG_DEBUG_ERR, "%s: pthread_join error: %s\n", __func__,
                          strerror(errno));
                // just ignore the error
            }

            async_data_handler_priv->thread_id = 0;
        }

        free(rs->async_data_handler_priv_data);
        rs->async_data_handler_priv_data = NULL;
    }

#endif

    RETURNFUNC(RIG_OK);
}

void *async_data_handler(void *arg)
{
    struct async_data_handler_args_s *args = (struct async_data_handler_args_s *)
            arg;
    RIG *rig = args->rig;
    unsigned char frame[MAX_FRAME_LENGTH];
    struct rig_state *rs = &rig->state;
    int result;

    rig_debug(RIG_DEBUG_VERBOSE, "%s: Starting async data handler thread\n",
              __func__);

    // TODO: check how to enable "transceive" on recent Kenwood/Yaesu rigs
    // TODO: add initial support for async in Kenwood kenwood_transaction (+one) functions -> add transaction_active flag usage
    // TODO: add initial support for async in Yaesu newcat_get_cmd/set_cmd (+validate) functions -> add transaction_active flag usage

    while (rs->async_data_handler_thread_run)
    {
        int frame_length;
        int async_frame;

        result = rig->caps->read_frame_direct(rig, sizeof(frame), frame);

        if (result < 0)
        {
            // Timeouts occur always if there is nothing to receive, so they are not really errors in this case
            if (result != -RIG_ETIMEOUT)
            {
                // TODO: it may be necessary to have mutex locking on transaction_active flag
                if (rs->transaction_active)
                {
                    unsigned char data = (unsigned char) result;
                    write_block_sync_error(&rs->rigport, &data, 1);
                }

                // TODO: error handling -> store errors in rig state -> to be exposed in async snapshot packets
                rig_debug(RIG_DEBUG_ERR, "%s: read_frame_direct() failed, result=%d\n",
                          __func__, result);
                hl_usleep(500 * 1000);
            }

            continue;
        }

        frame_length = result;

        async_frame = rig->caps->is_async_frame(rig, frame_length, frame);

        rig_debug(RIG_DEBUG_VERBOSE, "%s: received frame: len=%d async=%d\n", __func__,
                  frame_length, async_frame);

        if (async_frame)
        {
            result = rig->caps->process_async_frame(rig, frame_length, frame);

            if (result < 0)
            {
                // TODO: error handling -> store errors in rig state -> to be exposed in async snapshot packets
                rig_debug(RIG_DEBUG_ERR, "%s: process_async_frame() failed, result=%d\n",
                          __func__, result);
                continue;
            }
        }
        else
        {
            result = write_block_sync(&rs->rigport, frame, frame_length);

            if (result < 0)
            {
                // TODO: error handling? can writing to a pipe really fail in ways we can recover from?
                rig_debug(RIG_DEBUG_ERR, "%s: write_block_sync() failed, result=%d\n", __func__,
                          result);
                continue;
            }
        }
    }

    rig_debug(RIG_DEBUG_VERBOSE, "%s: Stopping async data handler thread\n",
              __func__);

    return NULL;
}
#endif

HAMLIB_EXPORT(int) rig_password(RIG *rig, const char *key1)
{
    int retval = -RIG_EPROTO;
    ENTERFUNC;

    if (rig->caps->password != NULL)
    {
        retval = rig->caps->password(rig, key1);
        //retval = RIG_OK;
    }

    RETURNFUNC(retval);
}

extern int read_icom_frame(hamlib_port_t *p, const unsigned char rxbuffer[],
                           size_t rxbuffer_len);


// Returns # of bytes read
// reply_len should be max bytes expected + 1
// if term is null then will read reply_len bytes exactly and reply will not be null terminated
HAMLIB_EXPORT(int) rig_send_raw(RIG *rig, const unsigned char *send,
                                int send_len, unsigned char *reply, int reply_len, unsigned char *term)
{
    struct rig_state *rs = &rig->state;
    unsigned char buf[200];
    int nbytes;
    ENTERFUNC;

    if (rig->caps->rig_model == RIG_MODEL_DUMMY
            || rig->caps->rig_model == RIG_MODEL_NONE)
    {
        rig_debug(RIG_DEBUG_ERR, "%s: not implemented for model %s\n", __func__,
                  rig->caps->model_name);
        return -RIG_ENAVAIL;
    }

    ELAPSED1;

    rig_debug(RIG_DEBUG_VERBOSE, "%s: writing %d bytes\n", __func__, send_len);
    int retval = write_block(&rs->rigport, send, send_len);

    if (retval < 0)
    {
        // TODO: error handling? can writing to a pipe really fail in ways we can recover from?
        rig_debug(RIG_DEBUG_ERR, "%s: write_block_sync() failed, result=%d\n", __func__,
                  retval);
    }

    if (reply)
    {
        if (term ==
                NULL) // we have to have terminating char or else we can't read the response
        {
            rig_debug(RIG_DEBUG_ERR, "%s: term==NULL, must have terminator to read reply\n",
                      __func__);
            RETURNFUNC(-RIG_EINVAL);
        }

        if (*term == 0xfd) // then we want an Icom frame
        {
            rig_debug(RIG_DEBUG_VERBOSE, "%s: reading icom frame\n", __func__);
            retval = read_icom_frame(&rs->rigport, buf, sizeof(buf));
            nbytes = retval;
        }
        else if (term == NULL)
        {
            rig_debug(RIG_DEBUG_VERBOSE, "%s: reading binary frame\n", __func__);
            nbytes = read_string_direct(&rs->rigport, buf, reply_len, (const char *)term,
                                        1, 0, 1);
        }
        else // we'll assume the provided terminator works
        {
            rig_debug(RIG_DEBUG_VERBOSE, "%s: reading frame terminated by '%s'\n", __func__,
                      term);
            nbytes = read_string_direct(&rs->rigport, buf, sizeof(buf), (const char *)term,
                                        1, 0, 1);
        }

        if (retval < RIG_OK)
        {
            rig_debug(RIG_DEBUG_ERR, "%s: read_string_direct, result=%d\n", __func__,
                      retval);
            RETURNFUNC(retval);
        }

        if (nbytes >= reply_len)
        {
            rig_debug(RIG_DEBUG_ERR, "%s: reply_len(%d) less than reply from rig(%d)\n",
                      __func__, reply_len, nbytes);
            return -RIG_EINVAL;
        }

        memcpy(reply, buf, reply_len - 1);
    }
    else
    {
        RETURNFUNC(retval);
    }

    ELAPSED2;

    RETURNFUNC(nbytes > 0 ? nbytes : -RIG_EPROTO);
}

HAMLIB_EXPORT(int) rig_set_lock_mode(RIG *rig, int mode)
{
    int retcode = -RIG_ENAVAIL;

    if (rig->caps->set_lock_mode)
    {
        retcode = rig->caps->set_lock_mode(rig, mode);
    }
    else
    {
        rig->state.lock_mode = mode;
        retcode = RIG_OK;
    }

    return (retcode);
}

HAMLIB_EXPORT(int) rig_get_lock_mode(RIG *rig, int *mode)
{
    int retcode = -RIG_ENAVAIL;

    if (rig->caps->get_lock_mode)
    {
        retcode = rig->caps->get_lock_mode(rig, mode);
    }
    else
    {
        *mode = rig->state.lock_mode;
        retcode = RIG_OK;
    }

    return (retcode);
}

HAMLIB_EXPORT(int) rig_is_model(RIG *rig, rig_model_t model)
{
    int is_rig;

    //a bit too verbose so disable this unless needed
    //rig_debug(RIG_DEBUG_TRACE, "%s(%d):%s called\n", __FILE__, __LINE__, __func__);
    is_rig = (model == rig->caps->rig_model) ? 1 : 0;

    return (is_rig); // RETURN is too verbose here
}

