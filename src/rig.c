/*
 *  Hamlib Interface - main file
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
#ifdef HAVE_CONFIG_H
#  include "config.h"
#endif

#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <stdio.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>


#include <hamlib/rig.h>
#include "serial.h"
#include "parallel.h"
#include "usb_port.h"
#include "network.h"
#include "event.h"
#include "cm108.h"
#include "gpio.h"
#include "misc.h"

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
const char *hamlib_version2 = "Hamlib " PACKAGE_VERSION;
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
static const char *rigerror_table[] =
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
    "Argument out of domain of func"
};


#define ERROR_TBL_SZ (sizeof(rigerror_table)/sizeof(char *))

/*
 * track which rig is opened (with rig_open)
 * needed at least for transceive mode
 */
static int add_opened_rig(RIG *rig)
{
    struct opened_rig_l *p;

    p = (struct opened_rig_l *)malloc(sizeof(struct opened_rig_l));

    if (!p)
    {
        RETURNFUNC(-RIG_ENOMEM);
    }

    p->rig = rig;
    p->next = opened_rig_list;
    opened_rig_list = p;

    RETURNFUNC(RIG_OK);
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
            RETURNFUNC(RIG_OK);
        }

        q = p;
    }

    RETURNFUNC(-RIG_EINVAL); /* Not found in list ! */
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
            RETURNFUNC(RIG_OK);
        }
    }

    RETURNFUNC(RIG_OK);
}

#endif /* !DOC_HIDDEN */


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
char debugmsgsave[DEBUGMSGSAVE_SIZE] = "No message";
char debugmsgsave2[DEBUGMSGSAVE_SIZE] = "No message";

const char *HAMLIB_API rigerror(int errnum)
{
    errnum = abs(errnum);

    if (errnum >= ERROR_TBL_SZ)
    {
        // This should not happen, but if it happens don't return NULL
        return "ERR_OUT_OF_RANGE";
    }

    static char msg[DEBUGMSGSAVE_SIZE*2];
    // we have to remove LF from debugmsgsave since calling function controls LF
    char *p = &debugmsgsave[strlen(debugmsgsave)-1];
    if (*p=='\n') *p=0;
    snprintf(msg, sizeof(msg), "%.80s\n%.15000s%.15000s", rigerror_table[errnum], debugmsgsave2, debugmsgsave);
    return msg;
}

// We use a couple of defined pointer to determine if the shared library changes
void *caps_test_rig_model = &caps_test.rig_model;
void *caps_test_macro_name = &caps_test.macro_name;

// check and show WARN if rig_caps structure doesn't match
// this tests for shared library incompatibility
int rig_check_rig_caps()
{
    int rc = RIG_OK;

    if (&caps_test.rig_model != caps_test_rig_model)
    {
        rc = -RIG_EINTERNAL;
        rig_debug(RIG_DEBUG_WARN, "%s: shared libary change#1\n", __func__);
    }

    if (&caps_test.macro_name != caps_test_macro_name)
    {
        rc = -RIG_EINTERNAL;
        rig_debug(RIG_DEBUG_WARN, "%s: shared libary change#2\n", __func__);
    }

    //if (rc != RIG_OK)
    {
        rig_debug(RIG_DEBUG_TRACE, "%s: p1=%p, p2=%p, rig_model=%p, macro_name=%p\n",
                  __func__, caps_test_rig_model, caps_test_macro_name, &caps_test.rig_model,
                  &caps_test.macro_name);
    }

    return rc;
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

    ENTERFUNC;

    rig_check_rig_caps();

    rig_check_backend(rig_model);

    caps = rig_get_caps(rig_model);

    if (!caps)
    {
        RETURNFUNC(NULL);
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
        RETURNFUNC(NULL);
    }

    /* caps is const, so we need to tell compiler
       that we know what we are doing */
    rig->caps = (struct rig_caps *) caps;

    /*
     * populate the rig->state
     * TODO: read the Preferences here!
     */
    rs = &rig->state;

    rs->rigport.fd = -1;
    rs->pttport.fd = -1;
    rs->comm_state = 0;
    rs->rigport.type.rig = caps->port_type; /* default from caps */

    switch (caps->port_type)
    {
    case RIG_PORT_SERIAL:
        strncpy(rs->rigport.pathname, DEFAULT_SERIAL_PORT, FILPATHLEN - 1);
        rs->rigport.parm.serial.rate = caps->serial_rate_max;   /* fastest ! */
        rs->rigport.parm.serial.data_bits = caps->serial_data_bits;
        rs->rigport.parm.serial.stop_bits = caps->serial_stop_bits;
        rs->rigport.parm.serial.parity = caps->serial_parity;
        rs->rigport.parm.serial.handshake = caps->serial_handshake;
        break;

    case RIG_PORT_PARALLEL:
        strncpy(rs->rigport.pathname, DEFAULT_PARALLEL_PORT, FILPATHLEN - 1);
        break;

    /* Adding support for CM108 GPIO.  This is compatible with CM108 series
     * USB audio chips from CMedia and SSS1623 series USB audio chips from 3S
     */
    case RIG_PORT_CM108:
        strncpy(rs->rigport.pathname, DEFAULT_CM108_PORT, FILPATHLEN);
        rs->rigport.parm.cm108.ptt_bitnum = DEFAULT_CM108_PTT_BITNUM;
        rs->pttport.parm.cm108.ptt_bitnum = DEFAULT_CM108_PTT_BITNUM;
        break;

    case RIG_PORT_GPIO:
        strncpy(rs->rigport.pathname, DEFAULT_GPIO_PORT, FILPATHLEN);
        break;

    case RIG_PORT_NETWORK:
    case RIG_PORT_UDP_NETWORK:
        strncpy(rs->rigport.pathname, "127.0.0.1:4532", FILPATHLEN - 1);
        break;

    default:
        strncpy(rs->rigport.pathname, "", FILPATHLEN - 1);
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
    rs->transceive = RIG_TRN_OFF;
    rs->poll_interval = 500;
    rs->lo_freq = 0;
    rs->cache.timeout_ms = 500;  // 500ms cache timeout by default

    // We are using range_list1 as the default
    // Eventually we will have separate model number for different rig variations
    // So range_list1 will become just range_list (per model)
    // See ic9700.c for a 5-model example
    // Every rig should have a rx_range
    // Rig backends need updating for new range_list format
    memcpy(rs->rx_range_list, caps->rx_range_list1,
           sizeof(struct freq_range_list)*FRQRANGESIZ);
    memcpy(rs->tx_range_list, caps->tx_range_list1,
           sizeof(struct freq_range_list)*FRQRANGESIZ);

    // if we don't have list1 we'll try list2
    if (rs->rx_range_list[0].startf == 0)
    {
        rig_debug(RIG_DEBUG_TRACE,
                  "%s: rx_range_list1 is empty, using rx_range_list2\n", __func__);
        memcpy(rs->tx_range_list, caps->rx_range_list2,
               sizeof(struct freq_range_list)*FRQRANGESIZ);
        memcpy(rs->rx_range_list, caps->tx_range_list2,
               sizeof(struct freq_range_list)*FRQRANGESIZ);
    }

    if (rs->tx_range_list[0].startf == 0)
    {
        rig_debug(RIG_DEBUG_ERR, "%s: rig does not have tx_range!!\n", __func__);
        //RETURNFUNC(NULL); // this is not fatal
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
               sizeof(struct freq_range_list)*FRQRANGESIZ);
        memcpy(rs->rx_range_list, caps->rx_range_list1,
               sizeof(struct freq_range_list)*FRQRANGESIZ);
        break;

    case RIG_ITU_REGION2:
    case RIG_ITU_REGION3:
    default:
        memcpy(rs->tx_range_list, caps->tx_range_list2,
               sizeof(struct freq_range_list)*FRQRANGESIZ);
        memcpy(rs->rx_range_list, caps->rx_range_list2,
               sizeof(struct freq_range_list)*FRQRANGESIZ);
        break;
    }

#endif
    rs->vfo_list = 0;
    rs->mode_list = 0;

    for (i = 0; i < FRQRANGESIZ && !RIG_IS_FRNG_END(caps->rx_range_list1[i]); i++)
    {
        rs->vfo_list |= caps->rx_range_list1[i].vfo;
        rs->mode_list |= caps->rx_range_list1[i].modes;
    }

    for (i = 0; i < FRQRANGESIZ && !RIG_IS_FRNG_END(caps->tx_range_list1[i]); i++)
    {
        rs->vfo_list |= caps->tx_range_list1[i].vfo;
        rs->mode_list |= caps->tx_range_list1[i].modes;
    }

    for (i = 0; i < FRQRANGESIZ && !RIG_IS_FRNG_END(caps->rx_range_list2[i]); i++)
    {
        rs->vfo_list |= caps->rx_range_list2[i].vfo;
        rs->mode_list |= caps->rx_range_list2[i].modes;
    }

    for (i = 0; i < FRQRANGESIZ && !RIG_IS_FRNG_END(caps->tx_range_list2[i]); i++)
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

    memcpy(rs->preamp, caps->preamp, sizeof(int)*MAXDBLSTSIZ);
    memcpy(rs->attenuator, caps->attenuator, sizeof(int)*MAXDBLSTSIZ);
    memcpy(rs->tuning_steps, caps->tuning_steps,
           sizeof(struct tuning_step_list)*TSLSTSIZ);
    memcpy(rs->filters, caps->filters,
           sizeof(struct filter_list)*FLTLSTSIZ);
    memcpy(&rs->str_cal, &caps->str_cal,
           sizeof(cal_table_t));

    memcpy(rs->chan_list, caps->chan_list, sizeof(chan_t)*CHANLSTSIZ);

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
            RETURNFUNC(NULL);
        }
    }

    RETURNFUNC(rig);
}


/**
 * \brief open the communication to the rig
 * \param rig   The #RIG handle of the radio to be opened
 *
 * Opens communication to a radio which \a RIG handle has been passed
 * by argument.
 *
 * \RETURNFUNC(RIG_OK) if the operation has been successful, otherwise
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

    if (strlen(rs->rigport.pathname) > 0)
    {
        char hoststr[256], portstr[6];
        status = parse_hoststr(rs->rigport.pathname, hoststr, portstr);

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
    }

    if (rs->comm_state)
    {
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

    add_opened_rig(rig);

    rs->comm_state = 1;

    /*
     * Maybe the backend has something to initialize
     * In case of failure, just close down and report error code.
     */
    if (caps->rig_open != NULL)
    {
        status = caps->rig_open(rig);

        if (status != RIG_OK)
        {
            RETURNFUNC(status);
        }
    }

    /*
     * trigger state->current_vfo first retrieval
     */
    if (rig_get_vfo(rig, &rs->current_vfo) == RIG_OK)
    {
        rs->tx_vfo = rs->current_vfo;
    }
    else // vfo fails so set some sensible defaults
    {
        int backend_num = RIG_BACKEND_NUM(rig->caps->rig_model);
        rs->tx_vfo = RIG_VFO_TX;
        rs->current_vfo = RIG_VFO_CURR;

        if (backend_num == RIG_ICOM)
        {
            rig_debug(RIG_DEBUG_TRACE, "%s: Icom rig so default vfo = %s\n", __func__,
                      rig_strvfo(rs->current_vfo));
        }
        else if (rig->caps->set_vfo == NULL)
        {
            // for non-Icom rigs if there's no set_vfo then we need to set one
            rs->current_vfo = vfo_fixup(rig, RIG_VFO_A);
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
        rig_set_parm(rig, RIG_PARM_SCREENSAVER, parm_value);
    }

#if 0

    /*
     * Check the current tranceive state of the rig
     */
    if (rs->transceive == RIG_TRN_RIG)
    {
        int retval, trn;
        retval = rig_get_trn(rig, &trn);

        if (retval == RIG_OK && trn == RIG_TRN_RIG)
        {
            add_trn_rig(rig);
        }
    }

#endif
    // read frequency to update internal status
//    freq_t freq;
//    if (caps->get_freq) rig_get_freq(rig, RIG_VFO_A, &freq);
//    if (caps->get_freq) rig_get_freq(rig, RIG_VFO_B, &freq);
    RETURNFUNC(RIG_OK);
}


/**
 * \brief close the communication to the rig
 * \param rig   The #RIG handle of the radio to be closed
 *
 * Closes communication to a radio which \a RIG handle has been passed
 * by argument that was previously open with rig_open().
 *
 * \RETURNFUNC(RIG_OK) if the operation has been successful, otherwise
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

    if (rs->transceive != RIG_TRN_OFF)
    {
        rig_set_trn(rig, RIG_TRN_OFF);
    }

    /*
     * Let the backend say 73s to the rig.
     * and ignore the return code.
     */
    if (caps->rig_close)
    {
        caps->rig_close(rig);
    }

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

    rs->comm_state = 0;

    RETURNFUNC(RIG_OK);
}


/**
 * \brief release a rig handle and free associated memory
 * \param rig   The #RIG handle of the radio to be closed
 *
 * Releases a rig struct which port has eventually been closed already
 * with rig_close().
 *
 * \RETURNFUNC(RIG_OK) if the operation has been successful, otherwise
 * a negative value if an error occurred (in which case, cause is
 * set appropriately).
 *
 * \sa rig_init(), rig_close()
 */
int HAMLIB_API rig_cleanup(RIG *rig)
{
    ENTERFUNC;

    if (!rig || !rig->caps)
    {
        RETURNFUNC(-RIG_EINVAL);
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

    RETURNFUNC(RIG_OK);
}

/**
 * \brief timeout (secs) to stop rigctld when VFO is manually changed
 * \param rig   The rig handle
 * \param seconds    The timeout to set to
 *
 * timeout seconds to stop rigctld when VFO is manually changed
 * turns on/off the radio.
 *
 * \RETURNFUNC(RIG_OK) if the operation has been successful, otherwise
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
 * \RETURNFUNC(RIG_OK) if the operation has been successful, otherwise
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
 * \RETURNFUNC(RIG_OK) if the operation has been successful, otherwise
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
int twiddling(RIG *rig)
{
    const struct rig_caps *caps;

    if (rig->state.twiddle_timeout == 0) { return 0; } // don't detect twiddling

    caps = rig->caps;

    if (caps->get_freq)    // gotta have get_freq of course
    {
        freq_t curr_freq = 0;
        int retval2;
        int elapsed;

        retval2 = caps->get_freq(rig, RIG_VFO_CURR, &curr_freq);

        if (retval2 == RIG_OK && rig->state.current_freq != curr_freq)
        {
            rig_debug(RIG_DEBUG_TRACE,
                      "%s: Somebody twiddling the VFO? last_freq=%.0f, curr_freq=%.0f\n", __func__,
                      rig->state.current_freq, curr_freq);

            if (rig->state.current_freq == 0)
            {
                rig->state.current_freq = curr_freq;
                RETURNFUNC(0); // not twiddling as first time freq is being set
            }

            rig->state.twiddle_time = time(NULL); // update last twiddle time
            rig->state.current_freq = curr_freq; // we have a new freq to remember
        }

        elapsed = time(NULL) - rig->state.twiddle_time;

        if (elapsed < rig->state.twiddle_timeout)
        {
            rig_debug(RIG_DEBUG_TRACE, "%s: Twiddle elapsed < 3, elapsed=%d\n", __func__,
                      elapsed);
            RETURNFUNC(1); // would be better as error but other software won't handle it
        }
    }

    RETURNFUNC(0);
}

/* caching prototype to be fully implemented in 4.1 */
static int set_cache_freq(RIG *rig, vfo_t vfo, freq_t freq)
{
    rig_debug(RIG_DEBUG_TRACE, "%s:  vfo=%s, current_vfo=%s\n", __func__,
              rig_strvfo(vfo), rig_strvfo(rig->state.current_vfo));

    if (vfo == RIG_VFO_CURR) 
    { 
        // if CURR then update this before we figure out the real VFO
        rig->state.cache.freqCurr = freq;
        elapsed_ms(&rig->state.cache.time_freqCurr, HAMLIB_ELAPSED_SET);
        vfo = rig->state.current_vfo; 
    }

    rig_debug(RIG_DEBUG_TRACE, "%s: set vfo=%s to freq=%.0f\n", __func__, rig_strvfo(vfo), freq);

    switch (vfo)
    {
    case RIG_VFO_ALL: // we'll use NONE to reset all VFO caches
        elapsed_ms(&rig->state.cache.time_freqCurr, HAMLIB_ELAPSED_INVALIDATE);
        elapsed_ms(&rig->state.cache.time_freqMainA, HAMLIB_ELAPSED_INVALIDATE);
        elapsed_ms(&rig->state.cache.time_freqMainB, HAMLIB_ELAPSED_INVALIDATE);
        elapsed_ms(&rig->state.cache.time_freqSubA, HAMLIB_ELAPSED_INVALIDATE);
        elapsed_ms(&rig->state.cache.time_freqSubB, HAMLIB_ELAPSED_INVALIDATE);
        elapsed_ms(&rig->state.cache.time_freqMem, HAMLIB_ELAPSED_INVALIDATE);
        break;
    case RIG_VFO_CURR:
        rig->state.cache.freqCurr = freq;
        elapsed_ms(&rig->state.cache.time_freqCurr, HAMLIB_ELAPSED_SET);
        break;

    case RIG_VFO_A:
    case RIG_VFO_MAIN:
    case RIG_VFO_MAIN_A:
        rig->state.cache.freqMainA = freq;
        elapsed_ms(&rig->state.cache.time_freqMainA, HAMLIB_ELAPSED_SET);
        break;

    case RIG_VFO_B:
    case RIG_VFO_MAIN_B:
    case RIG_VFO_SUB:
        rig->state.cache.freqMainB = freq;
        elapsed_ms(&rig->state.cache.time_freqMainB, HAMLIB_ELAPSED_SET);
        break;

#if 0 // 5.0
    case RIG_VFO_C: // is there a MainC/SubC we need to cover?
        rig->state.cache.freqMainC = freq;
        elapsed_ms(&rig->state.cache.time_freqMainC, HAMLIB_ELAPSED_SET);
        break;
#endif

    case RIG_VFO_SUB_A:
        rig->state.cache.freqSubA = freq;
        elapsed_ms(&rig->state.cache.time_freqSubA, HAMLIB_ELAPSED_SET);
        break;

    case RIG_VFO_SUB_B:
        rig->state.cache.freqSubB = freq;
        elapsed_ms(&rig->state.cache.time_freqSubB, HAMLIB_ELAPSED_SET);
        break;

    case RIG_VFO_MEM:
        rig->state.cache.freqMem = freq;
        elapsed_ms(&rig->state.cache.time_freqMem, HAMLIB_ELAPSED_SET);
        break;

    default:
        rig_debug(RIG_DEBUG_ERR, "%s: unknown vfo?, vfo=%s\n", __func__,
                  rig_strvfo(vfo));
        RETURNFUNC(-RIG_EINVAL);
    }

    RETURNFUNC(RIG_OK);
}

/* caching prototype to be fully implemented in 4.1 */
static int get_cache_freq(RIG *rig, vfo_t vfo, freq_t *freq, int *cache_ms)
{
    rig_debug(RIG_DEBUG_TRACE, "%s:  vfo=%s, current_vfo=%s\n", __func__,
              rig_strvfo(vfo), rig_strvfo(rig->state.current_vfo));

    if (vfo == RIG_VFO_CURR) { vfo = rig->state.current_vfo; }

    rig_debug(RIG_DEBUG_TRACE, "%s: get vfo=%s\n", __func__, rig_strvfo(vfo));

    // VFO_C to be implemented
    switch (vfo)
    {
    case RIG_VFO_CURR:
        *freq = rig->state.cache.freqCurr;
        *cache_ms = elapsed_ms(&rig->state.cache.time_freqCurr, HAMLIB_ELAPSED_GET);
        break;

    case RIG_VFO_A:
    case RIG_VFO_MAIN:
    case RIG_VFO_MAIN_A:
        *freq = rig->state.cache.freqMainA;
        *cache_ms = elapsed_ms(&rig->state.cache.time_freqMainA, HAMLIB_ELAPSED_GET);
        break;

    case RIG_VFO_B:
    case RIG_VFO_SUB:
        *freq = rig->state.cache.freqMainB;
        *cache_ms = elapsed_ms(&rig->state.cache.time_freqMainB, HAMLIB_ELAPSED_GET);
        break;

    case RIG_VFO_SUB_A:
        *freq = rig->state.cache.freqSubA;
        *cache_ms = elapsed_ms(&rig->state.cache.time_freqSubA, HAMLIB_ELAPSED_GET);
        break;

    case RIG_VFO_SUB_B:
        *freq = rig->state.cache.freqSubB;
        *cache_ms = elapsed_ms(&rig->state.cache.time_freqSubB, HAMLIB_ELAPSED_GET);
        break;

#if 0 // 5.0
    case RIG_VFO_C:
    //case RIG_VFO_MAINC: // not used by any rig yet
        *freq = rig->state.cache.freqMainC;
        *cache_ms = elapsed_ms(&rig->state.cache.time_freqMainC, HAMLIB_ELAPSED_GET);
        break;
#endif

#if 0 // no known rigs use this yet
    case RIG_VFO_SUBC:
        *freq = rig->state.cache.freqSubC;
        *cache_ms = rig->state.cache.time_freqSubC;
        break;
#endif

    case RIG_VFO_MEM:
        *freq = rig->state.cache.freqMem;
        *cache_ms = elapsed_ms(&rig->state.cache.time_freqMem, HAMLIB_ELAPSED_GET);
        break;

    default:
        rig_debug(RIG_DEBUG_ERR, "%s: unknown vfo?, vfo=%s\n", __func__,
                  rig_strvfo(vfo));
        RETURNFUNC(-RIG_EINVAL);
    }

    rig_debug(RIG_DEBUG_TRACE, "%s: vfo=%s, freq=%.0f\n", __func__, rig_strvfo(vfo),
              (double)*freq);
    RETURNFUNC(RIG_OK);
}

/**
 * \brief set the frequency of the target VFO
 * \param rig   The rig handle
 * \param vfo   The target VFO
 * \param freq  The frequency to set to
 *
 * Sets the frequency of the target VFO.
 *
 * \RETURNFUNC(RIG_OK) if the operation has been successful, otherwise
 * a negative value if an error occurred (in which case, cause is
 * set appropriately).
 *
 * \sa rig_get_freq()
 */
int HAMLIB_API rig_set_freq(RIG *rig, vfo_t vfo, freq_t freq)
{
    const struct rig_caps *caps;
    int retcode;
    freq_t freq_new = freq;

    rig_debug(RIG_DEBUG_VERBOSE, "%s called vfo=%s, freq=%.0f\n", __func__,
              rig_strvfo(vfo), freq);

    if (CHECK_RIG_ARG(rig))
    {
        RETURNFUNC(-RIG_EINVAL);
    }

    caps = rig->caps;

    vfo = vfo_fixup(rig, vfo);

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
        RETURNFUNC(-RIG_ENAVAIL);
    }

    if ((caps->targetable_vfo & RIG_TARGETABLE_FREQ)
            || vfo == RIG_VFO_CURR || vfo == rig->state.current_vfo)
    {
        if (twiddling(rig))
        {
            rig_debug(RIG_DEBUG_TRACE, "%s: Ignoring set_freq due to VFO twiddling\n",
                      __func__);
            RETURNFUNC(
                RIG_OK); // would be better as error but other software won't handle errors
        }

        rig_debug(RIG_DEBUG_TRACE, "%s: TARGETABLE_FREQ vfo=%s\n", __func__,
                  rig_strvfo(vfo));
        int retry=5;
        freq_t tfreq;
        do {
            retcode = caps->set_freq(rig, vfo, freq);
            if (retcode != RIG_OK) RETURNFUNC(retcode);
            set_cache_freq(rig, RIG_VFO_ALL, (freq_t)0);
            if (caps->set_freq)
            {
                retcode = rig_get_freq(rig, vfo, &tfreq);
                if (retcode != RIG_OK) RETURNFUNC(retcode);
                if (tfreq != freq)
                {
                    hl_usleep(50*1000);
                    rig_debug(RIG_DEBUG_WARN, "%s: freq not set correctly?? got %.0f asked for %.0f\n", __func__, (double)tfreq, (double)freq);
                }
            }
            else { retry = 1; }
        } while (tfreq != freq && --retry > 0);
        if (retry == 0)
        {
            rig_debug(RIG_DEBUG_ERR, "%s: unable to set frequency!!\n", __func__);
        }
    }
    else
    {
        rig_debug(RIG_DEBUG_TRACE, "%s: not a TARGETABLE_FREQ vfo=%s\n", __func__,
                  rig_strvfo(vfo));
        int rc2;
        vfo_t curr_vfo;

        if (!caps->set_vfo)
        {
            RETURNFUNC(-RIG_ENAVAIL);
        }

        if (twiddling(rig))
        {
            rig_debug(RIG_DEBUG_TRACE, "%s: Ignoring set_freq due to VFO twiddling\n",
                      __func__);
            RETURNFUNC(
                RIG_OK); // would be better as error but other software won't handle errors
        }

        curr_vfo = rig->state.current_vfo;
        retcode = caps->set_vfo(rig, vfo);
        // why is the line below here?
        // it's causing set_freq on the wrong vfo
        //vfo = rig->state.current_vfo; // can't call get_vfo since Icoms don't have it

        if (retcode != RIG_OK)
        {
            rig_debug(RIG_DEBUG_ERR, "%s: set_vfo(%s) err %.10000s\n", __func__, rig_strvfo(vfo), rigerror(retcode));
            RETURNFUNC(retcode);
        }


        retcode = caps->set_freq(rig, vfo, freq);
        /* try and revert even if we had an error above */
        rc2 = caps->set_vfo(rig, curr_vfo);

        if (RIG_OK == retcode)
        {
            /* return the first error code */
            retcode = rc2;
        }
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
            elapsed_ms(&rig->state.cache.time_freq, HAMLIB_ELAPSED_INVALIDATE);
            set_cache_freq(rig, RIG_VFO_ALL, (freq_t)0);
            retcode = rig_get_freq(rig, vfo, &freq_new);

            if (retcode != RIG_OK) { RETURNFUNC(retcode); }
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

    elapsed_ms(&(rig->state.cache.time_freq), HAMLIB_ELAPSED_SET);
    rig->state.cache.freq = freq_new;
    //future 4.1 caching
    set_cache_freq(rig, vfo, freq_new);
    rig->state.cache.vfo_freq = vfo;

    RETURNFUNC(retcode);
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
 * \RETURNFUNC(RIG_OK) if the operation has been successful, otherwise
 * a negative value if an error occurred (in which case, cause is
 * set appropriately).
 *
 * \sa rig_set_freq()
 */
int HAMLIB_API rig_get_freq(RIG *rig, vfo_t vfo, freq_t *freq)
{
    const struct rig_caps *caps;
    int retcode;
    int cache_ms;
    vfo_t curr_vfo;

    rig_debug(RIG_DEBUG_VERBOSE, "%s called vfo=%s\n", __func__, rig_strvfo(vfo));

#if 0 // don't think we really need this check

    if (CHECK_RIG_ARG(rig) || !freq)
    {
        rig_debug(RIG_DEBUG_TRACE, "%s: rig or freq ptr invalid\n", __func__);
        RETURNFUNC(-RIG_EINVAL);
    }

#endif

    curr_vfo = rig->state.current_vfo; // save vfo for restore later

    vfo = vfo_fixup(rig, vfo);

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
        get_cache_freq(rig, vfo, freq, &cache_ms);
        RETURNFUNC(RIG_OK);
    }

    // there are some rigs that can't get VFOA freq while VFOB is transmitting
    // so we'll return the cached VFOA freq for them
    // should we use the cached ptt maybe? No -- we have to be 100% sure we're in PTT to ignore this request
    if ((vfo == RIG_VFO_A || vfo == RIG_VFO_MAIN) && rig->state.cache.split &&
            (rig->caps->rig_model == RIG_MODEL_FTDX101D
             || rig->caps->rig_model == RIG_MODEL_IC910))
    {
        // if we're in PTT don't get VFOA freq -- otherwise we interrupt transmission
        ptt_t ptt;
        retcode = rig_get_ptt(rig, RIG_VFO_CURR, &ptt);

        if (retcode != RIG_OK)
        {
            RETURNFUNC(retcode);
        }

        if (ptt)
        {
            rig_debug(RIG_DEBUG_TRACE,
                      "%s: split is on so returning VFOA last known freq\n",
                      __func__);
            *freq = rig->state.cache.freqMainA;
            RETURNFUNC(RIG_OK);
        }
    }


    //future 4.1 caching
    cache_ms = 10000;
    get_cache_freq(rig, vfo, freq, &cache_ms);
    rig_debug(RIG_DEBUG_TRACE, "%s: cache check1 age=%dms\n", __func__, cache_ms);
    //future 4.1 caching needs to check individual VFO timeouts
    //cache_ms = elapsed_ms(&rig->state.cache.time_freq, HAMLIB_ELAPSED_GET);
    //rig_debug(RIG_DEBUG_TRACE, "%s: cache check2 age=%dms\n", __func__, cache_ms);

    if (freq != 0 && cache_ms < rig->state.cache.timeout_ms)
    {
        rig_debug(RIG_DEBUG_TRACE, "%s: %s cache hit age=%dms, freq=%.0f\n", __func__,
                  rig_strvfo(vfo), cache_ms, *freq);
        RETURNFUNC(RIG_OK);
    }
    else
    {
        rig_debug(RIG_DEBUG_TRACE,
                  "%s: cache miss age=%dms, cached_vfo=%s, asked_vfo=%s\n", __func__, cache_ms,
                  rig_strvfo(rig->state.cache.vfo_freq), rig_strvfo(vfo));
    }

    caps = rig->caps;

    if (caps->get_freq == NULL)
    {
        RETURNFUNC(-RIG_ENAVAIL);
    }

    // If we're in vfo_mode then rigctld will do any VFO swapping we need
    if ((caps->targetable_vfo & RIG_TARGETABLE_FREQ)
            || vfo == RIG_VFO_CURR || vfo == rig->state.current_vfo
            || (rig->state.vfo_opt == 1 && rig->caps->rig_model == RIG_MODEL_NETRIGCTL))
    {
        // If rig does not have set_vfo we need to change vfo
        if (vfo == RIG_VFO_CURR && caps->set_vfo == NULL)
        {
            vfo = vfo_fixup(rig, RIG_VFO_A);
            rig_debug(RIG_DEBUG_TRACE, "%s: no set_vfo so vfo=%s\n", __func__,
                      rig_strvfo(vfo));
        }

        retcode = caps->get_freq(rig, vfo, freq);

        if (retcode == RIG_OK)
        {
            rig->state.cache.freq = *freq;
            //future 4.1 caching
            set_cache_freq(rig, vfo, *freq);
            rig->state.cache.vfo_freq = vfo;
        }
    }
    else
    {
        int rc2;

        if (!caps->set_vfo)
        {
            RETURNFUNC(-RIG_ENAVAIL);
        }

        retcode = caps->set_vfo(rig, vfo);

        if (retcode != RIG_OK)
        {
            RETURNFUNC(retcode);
        }

        retcode = caps->get_freq(rig, vfo, freq);
        /* try and revert even if we had an error above */
        rc2 = caps->set_vfo(rig, curr_vfo);

        if (RIG_OK == retcode)
        {
            cache_ms = elapsed_ms(&(rig->state.cache.time_freq), HAMLIB_ELAPSED_SET);
            rig_debug(RIG_DEBUG_TRACE, "%s: cache reset age=%dms, vfo=%s, freq=%.0f\n",
                      __func__, cache_ms, rig_strvfo(vfo), *freq);
            rig->state.cache.freq = *freq;
            //future 4.1 caching
            set_cache_freq(rig, vfo, *freq);
            rig->state.cache.vfo_freq = vfo;
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


    cache_ms = elapsed_ms(&(rig->state.cache.time_freq), HAMLIB_ELAPSED_SET);
    rig_debug(RIG_DEBUG_TRACE, "%s: cache reset age=%dms, vfo=%s, freq=%.0f\n",
              __func__, cache_ms, rig_strvfo(vfo), *freq);
    rig->state.cache.freq = *freq;
    //future 4.1 caching
    set_cache_freq(rig, vfo, *freq);
    rig->state.cache.vfo_freq = vfo;

    RETURNFUNC(retcode);
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
 * \RETURNFUNC(RIG_OK) if the operation has been successful, otherwise
 * a negative value if an error occurred (in which case, cause is
 * set appropriately).
 *
 * \sa rig_set_freq()
 */
int HAMLIB_API rig_get_freqs(RIG *rig, freq_t *freqA, freq_t freqB)
{
    // we will attempt to avoid vfo swapping in this routine

    return -RIG_ENIMPL;

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
 * \RETURNFUNC(RIG_OK) if the operation has been successful, otherwise
 * a negative value if an error occurred (in which case, cause is
 * set appropriately).
 *
 * \sa rig_get_mode()
 */
int HAMLIB_API rig_set_mode(RIG *rig, vfo_t vfo, rmode_t mode, pbwidth_t width)
{
    const struct rig_caps *caps;
    int retcode;

    rig_debug(RIG_DEBUG_VERBOSE, "%s called, vfo=%s, mode=%s, width=%d\n", __func__,
              rig_strvfo(vfo), rig_strrmode(mode), (int)width);

    if (CHECK_RIG_ARG(rig))
    {
        RETURNFUNC(-RIG_EINVAL);
    }

    caps = rig->caps;

    if (caps->set_mode == NULL)
    {
        RETURNFUNC(-RIG_ENAVAIL);
    }

    if ((caps->targetable_vfo & RIG_TARGETABLE_MODE)
            || vfo == RIG_VFO_CURR
            || vfo == rig->state.current_vfo)
    {
        retcode = caps->set_mode(rig, vfo, mode, width);
        rig_debug(RIG_DEBUG_TRACE, "%s: retcode after set_mode=%d\n", __func__,
                  retcode);
    }
    else
    {
        int rc2;
        vfo_t curr_vfo;

        if (!caps->set_vfo)
        {
            RETURNFUNC(-RIG_ENAVAIL);
        }

        curr_vfo = rig->state.current_vfo;
        retcode = caps->set_vfo(rig, vfo);

        if (retcode != RIG_OK)
        {
            RETURNFUNC(retcode);
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

    if (retcode == RIG_OK
            && (vfo == RIG_VFO_CURR || vfo == rig->state.current_vfo))
    {
        rig->state.current_mode = mode;
        rig->state.current_width = width;
    }

    rig->state.cache.mode = mode;
    rig->state.cache.vfo_mode = vfo;
    elapsed_ms(&rig->state.cache.time_mode, HAMLIB_ELAPSED_SET);

    RETURNFUNC(retcode);
}


/**
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
 * \RETURNFUNC(RIG_OK) if the operation has been successful, otherwise
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
    int cache_ms;

    ENTERFUNC;

    if (CHECK_RIG_ARG(rig) || !mode || !width)
    {
        RETURNFUNC(-RIG_EINVAL);
    }

    caps = rig->caps;

    if (caps->get_mode == NULL)
    {
        RETURNFUNC(-RIG_ENAVAIL);
    }

    cache_ms = elapsed_ms(&rig->state.cache.time_mode, HAMLIB_ELAPSED_GET);
    rig_debug(RIG_DEBUG_TRACE, "%s: cache check age=%dms\n", __func__, cache_ms);

    if (cache_ms < rig->state.cache.timeout_ms && rig->state.cache.vfo_mode == vfo)
    {
        rig_debug(RIG_DEBUG_TRACE, "%s: cache hit age=%dms\n", __func__, cache_ms);
        *mode = rig->state.cache.mode;
        *width = rig->state.cache.width;
        RETURNFUNC(RIG_OK);
    }
    else
    {
        rig_debug(RIG_DEBUG_TRACE, "%s: cache miss age=%dms\n", __func__, cache_ms);
    }

    if ((caps->targetable_vfo & RIG_TARGETABLE_MODE)
            || vfo == RIG_VFO_CURR
            || vfo == rig->state.current_vfo)
    {
        retcode = caps->get_mode(rig, vfo, mode, width);
        rig_debug(RIG_DEBUG_TRACE, "%s: retcode after get_mode=%d\n", __func__,
                  retcode);
    }
    else
    {
        int rc2;
        vfo_t curr_vfo;
        rig_debug(RIG_DEBUG_TRACE, "%s(%d): debug\n", __func__, __LINE__);

        if (!caps->set_vfo)
        {
            RETURNFUNC(-RIG_ENAVAIL);
        }

        curr_vfo = rig->state.current_vfo;
        retcode = caps->set_vfo(rig, vfo);

        if (retcode != RIG_OK)
        {
            RETURNFUNC(retcode);
        }

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
    }

    if (*width == RIG_PASSBAND_NORMAL && *mode != RIG_MODE_NONE)
    {
        rig_debug(RIG_DEBUG_TRACE, "%s(%d): debug\n", __func__, __LINE__);
        *width = rig_passband_normal(rig, *mode);
    }

    rig->state.cache.mode = *mode;
    rig->state.cache.width = *width;
    rig->state.cache.vfo_mode = vfo;
    cache_ms = elapsed_ms(&rig->state.cache.time_mode, HAMLIB_ELAPSED_SET);

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

    for (i = 0; i < FLTLSTSIZ && rs->filters[i].modes; i++)
    {
        if (rs->filters[i].modes & mode)
        {
            rig_debug(RIG_DEBUG_VERBOSE, "%s: return filter#%d, width=%d\n", __func__, i,
                      (int)rs->filters[i].width);
            RETURNFUNC(rs->filters[i].width);
        }
    }

    rig_debug(RIG_DEBUG_VERBOSE,
              "%s: filter not found...return RIG_PASSBAND_NORMAL=%d\n", __func__,
              (int)RIG_PASSBAND_NORMAL);
    RETURNFUNC(RIG_PASSBAND_NORMAL);
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

    for (i = 0; i < FLTLSTSIZ - 1 && rs->filters[i].modes; i++)
    {
        if (rs->filters[i].modes & mode)
        {
            normal = rs->filters[i].width;

            for (i++; i < FLTLSTSIZ && rs->filters[i].modes; i++)
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

    for (i = 0; i < FLTLSTSIZ - 1 && rs->filters[i].modes; i++)
    {
        if (rs->filters[i].modes & mode)
        {
            normal = rs->filters[i].width;

            for (i++; i < FLTLSTSIZ && rs->filters[i].modes; i++)
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
 * \RETURNFUNC(RIG_OK) if the operation has been successful, otherwise
 * a negative value if an error occurred (in which case, cause is
 * set appropriately).
 *
 * \sa rig_get_vfo()
 */
int HAMLIB_API rig_set_vfo(RIG *rig, vfo_t vfo)
{
    const struct rig_caps *caps;
    int retcode;
    freq_t curr_freq;

    ENTERFUNC;
    rig_debug(RIG_DEBUG_VERBOSE, "%s called vfo=%s\n", __func__, rig_strvfo(vfo));

    if (CHECK_RIG_ARG(rig))
    {
        RETURNFUNC(-RIG_EINVAL);
    }

    if (vfo == RIG_VFO_CURR) { RETURNFUNC(RIG_OK); }

    // make sure we are asking for a VFO that the rig actually has
    if ((vfo == RIG_VFO_A || vfo == RIG_VFO_B) && !VFO_HAS_A_B)
    {
        rig_debug(RIG_DEBUG_ERR, "%s: rig does not have %s\n", __func__,
                  rig_strvfo(vfo));
        RETURNFUNC(-RIG_EINVAL);
    }

    if ((vfo == RIG_VFO_MAIN || vfo == RIG_VFO_SUB) && !VFO_HAS_MAIN_SUB)
    {
        rig_debug(RIG_DEBUG_ERR, "%s: rig does not have %s\n", __func__,
                  rig_strvfo(vfo));
        RETURNFUNC(-RIG_EINVAL);
    }

    vfo = vfo_fixup(rig, vfo);

    caps = rig->caps;

    if (caps->set_vfo == NULL)
    {
        RETURNFUNC(-RIG_ENAVAIL);
    }

    if (twiddling(rig))
    {
        rig_debug(RIG_DEBUG_TRACE, "%s: Ignoring set_vfo due to VFO twiddling\n",
                  __func__);
        RETURNFUNC(
            RIG_OK); // would be better as error but other software won't handle errors
    }

    retcode = caps->set_vfo(rig, vfo);

    if (retcode == RIG_OK)
    {
        rig->state.current_vfo = vfo;
        rig->state.cache.vfo = vfo;
        rig_debug(RIG_DEBUG_TRACE, "%s: rig->state.current_vfo=%s\n", __func__,
                  rig_strvfo(vfo));
    }
    else
    {
        rig_debug(RIG_DEBUG_ERR, "%s: set_vfo %s failed with '%.10000s'\n", __func__,
                  rig_strvfo(vfo), rigerror(retcode));
    }

    // we need to update our internal freq to avoid getting detected as twiddling
    // we only get the freq if we set the vfo OK
    if (retcode == RIG_OK && caps->get_freq)
    {
        retcode = caps->get_freq(rig, vfo, &curr_freq);
        rig_debug(RIG_DEBUG_TRACE, "%s: retcode from rig_get_freq = %.10000s\n", __func__,
                  rigerror(retcode));
    }
    else // don't expire cache if we just read it
    {
        elapsed_ms(&rig->state.cache.time_freq, HAMLIB_ELAPSED_INVALIDATE);
    }

    // expire several cached items when we switch VFOs
    elapsed_ms(&rig->state.cache.time_vfo, HAMLIB_ELAPSED_INVALIDATE);
    elapsed_ms(&rig->state.cache.time_mode, HAMLIB_ELAPSED_INVALIDATE);

    rig_debug(RIG_DEBUG_TRACE, "%s: return %d, vfo=%s\n", __func__, retcode,
              rig_strvfo(vfo));
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
 * \RETURNFUNC(RIG_OK) if the operation has been successful, otherwise
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

    if (CHECK_RIG_ARG(rig) || !vfo)
    {
        rig_debug(RIG_DEBUG_ERR, "%s: no rig and/or vfo?  rig=%p, vfo=%p\n", __func__,
                  rig, vfo);
        RETURNFUNC(-RIG_EINVAL);
    }

    caps = rig->caps;

    if (caps->get_vfo == NULL)
    {
        rig_debug(RIG_DEBUG_ERR, "%s: no get_vfo\n", __func__);
        RETURNFUNC(-RIG_ENAVAIL);
    }

    cache_ms = elapsed_ms(&rig->state.cache.time_vfo, HAMLIB_ELAPSED_GET);
    rig_debug(RIG_DEBUG_TRACE, "%s: cache check age=%dms\n", __func__, cache_ms);

    if (cache_ms < rig->state.cache.timeout_ms)
    {
        rig_debug(RIG_DEBUG_TRACE, "%s: cache hit age=%dms\n", __func__, cache_ms);
        *vfo = rig->state.cache.vfo;
        RETURNFUNC(RIG_OK);
    }
    else
    {
        rig_debug(RIG_DEBUG_TRACE, "%s: cache miss age=%dms\n", __func__, cache_ms);
    }

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
 * \RETURNFUNC(RIG_OK) if the operation has been successful, otherwise
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

    ENTERFUNC;

    if (CHECK_RIG_ARG(rig))
    {
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
            RETURNFUNC(-RIG_ENIMPL);
        }

        if ((caps->targetable_vfo & RIG_TARGETABLE_PTT)
                || vfo == RIG_VFO_CURR
                || vfo == rig->state.current_vfo)
        {

            retcode = caps->set_ptt(rig, vfo, ptt);
        }
        else
        {
            vfo_t curr_vfo;

            if (!caps->set_vfo)
            {
                RETURNFUNC(-RIG_ENAVAIL);
            }

            curr_vfo = rig->state.current_vfo;
            retcode = caps->set_vfo(rig, vfo);

            if (retcode == RIG_OK)
            {
                int rc2;
                retcode = caps->set_ptt(rig, vfo, ptt);
                /* try and revert even if we had an error above */
                rc2 = caps->set_vfo(rig, curr_vfo);

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
                RETURNFUNC(-RIG_EIO);
            }

            /* Needed on Linux because the serial port driver sets RTS/DTR
               high on open - set both since we offer no control of
               the non-PTT line and low is better than high */
            retcode = ser_set_rts(&rs->pttport, 0);

            if (RIG_OK != retcode)
            {
                RETURNFUNC(retcode);
            }
        }

        retcode = ser_set_dtr(&rig->state.pttport, ptt != RIG_PTT_OFF);

        if (strcmp(rs->pttport.pathname, rs->rigport.pathname)
                && ptt == RIG_PTT_OFF && rs->ptt_share != 0)
        {
            rig_debug(RIG_DEBUG_TRACE, "%s: ptt_share=%d\n", __func__, rs->ptt_share);
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
                RETURNFUNC(-RIG_EIO);
            }

            /* Needed on Linux because the serial port driver sets RTS/DTR
               high on open - set both since we offer no control of the
               non-PTT line and low is better than high */
            retcode = ser_set_dtr(&rs->pttport, 0);

            if (RIG_OK != retcode)
            {
                rig_debug(RIG_DEBUG_ERR, "%s: ser_set_dtr retcode=%d\n", __func__, retcode);
                RETURNFUNC(retcode);
            }
        }

        retcode = ser_set_rts(&rig->state.pttport, ptt != RIG_PTT_OFF);

        if (strcmp(rs->pttport.pathname, rs->rigport.pathname)
                && ptt == RIG_PTT_OFF && rs->ptt_share != 0)
        {
            rig_debug(RIG_DEBUG_TRACE, "%s: ptt_share=%d\n", __func__, rs->ptt_share);
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
        RETURNFUNC(-RIG_EINVAL);
    }

    if (RIG_OK == retcode)
    {
        rs->transmit = ptt != RIG_PTT_OFF;
    }

    rig->state.cache.ptt = ptt;
    elapsed_ms(&rig->state.cache.time_ptt, HAMLIB_ELAPSED_SET);

    if (retcode != RIG_OK) { rig_debug(RIG_DEBUG_ERR, "%s: return code=%d\n", __func__, retcode); }

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
 * \RETURNFUNC(RIG_OK) if the operation has been successful, otherwise
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
    int rc2, status;
    vfo_t curr_vfo;
    int cache_ms;

    ENTERFUNC;

    if (CHECK_RIG_ARG(rig) || !ptt)
    {
        RETURNFUNC(-RIG_EINVAL);
    }

    cache_ms = elapsed_ms(&rig->state.cache.time_ptt, HAMLIB_ELAPSED_GET);
    rig_debug(RIG_DEBUG_TRACE, "%s: cache check age=%dms\n", __func__, cache_ms);

    if (cache_ms < rig->state.cache.timeout_ms)
    {
        rig_debug(RIG_DEBUG_TRACE, "%s: cache hit age=%dms\n", __func__, cache_ms);
        *ptt = rig->state.cache.ptt;
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
            RETURNFUNC(RIG_OK);
        }

        if ((caps->targetable_vfo & RIG_TARGETABLE_PTT)
                || vfo == RIG_VFO_CURR
                || vfo == rig->state.current_vfo)
        {
            retcode = caps->get_ptt(rig, vfo, ptt);

            if (retcode == RIG_OK)
            {
                rig->state.cache.ptt = *ptt;
                elapsed_ms(&rig->state.cache.time_ptt, HAMLIB_ELAPSED_SET);
            }

            RETURNFUNC(retcode);
        }

        if (!caps->set_vfo)
        {
            RETURNFUNC(-RIG_ENAVAIL);
        }

        curr_vfo = rig->state.current_vfo;
        retcode = caps->set_vfo(rig, vfo);

        if (retcode != RIG_OK)
        {
            RETURNFUNC(retcode);
        }

        retcode = caps->get_ptt(rig, vfo, ptt);
        /* try and revert even if we had an error above */
        rc2 = caps->set_vfo(rig, curr_vfo);

        if (RIG_OK == retcode)
        {
            /* return the first error code */
            retcode = rc2;
            rig->state.cache.ptt = *ptt;
            elapsed_ms(&rig->state.cache.time_ptt, HAMLIB_ELAPSED_SET);
        }

        RETURNFUNC(retcode);

    case RIG_PTT_SERIAL_RTS:
        if (caps->get_ptt)
        {
            retcode = caps->get_ptt(rig, vfo, ptt);

            if (retcode == RIG_OK)
            {
                elapsed_ms(&rig->state.cache.time_ptt, HAMLIB_ELAPSED_SET);
                rig->state.cache.ptt = *ptt;
            }

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
        RETURNFUNC(retcode);

    case RIG_PTT_SERIAL_DTR:
        if (caps->get_ptt)
        {
            retcode = caps->get_ptt(rig, vfo, ptt);

            if (retcode == RIG_OK)
            {
                elapsed_ms(&rig->state.cache.time_ptt, HAMLIB_ELAPSED_SET);
                rig->state.cache.ptt = *ptt;
            }

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
        RETURNFUNC(retcode);

    case RIG_PTT_PARALLEL:
        if (caps->get_ptt)
        {
            retcode = caps->get_ptt(rig, vfo, ptt);

            if (retcode == RIG_OK)
            {
                elapsed_ms(&rig->state.cache.time_ptt, HAMLIB_ELAPSED_SET);
                rig->state.cache.ptt = *ptt;
            }

            RETURNFUNC(retcode);
        }

        retcode = par_ptt_get(&rig->state.pttport, ptt);

        if (retcode == RIG_OK)
        {
            elapsed_ms(&rig->state.cache.time_ptt, HAMLIB_ELAPSED_SET);
            rig->state.cache.ptt = *ptt;
        }

        RETURNFUNC(retcode);

    case RIG_PTT_CM108:
        if (caps->get_ptt)
        {
            retcode = caps->get_ptt(rig, vfo, ptt);

            if (retcode == RIG_OK)
            {
                elapsed_ms(&rig->state.cache.time_ptt, HAMLIB_ELAPSED_SET);
                rig->state.cache.ptt = *ptt;
            }

            RETURNFUNC(retcode);
        }

        retcode = cm108_ptt_get(&rig->state.pttport, ptt);

        if (retcode == RIG_OK)
        {
            elapsed_ms(&rig->state.cache.time_ptt, HAMLIB_ELAPSED_SET);
            rig->state.cache.ptt = *ptt;
        }

        RETURNFUNC(retcode);

    case RIG_PTT_GPIO:
    case RIG_PTT_GPION:
        if (caps->get_ptt)
        {
            retcode = caps->get_ptt(rig, vfo, ptt);

            if (retcode == RIG_OK)
            {
                elapsed_ms(&rig->state.cache.time_ptt, HAMLIB_ELAPSED_SET);
                rig->state.cache.ptt = *ptt;
            }

            RETURNFUNC(retcode);
        }

        elapsed_ms(&rig->state.cache.time_ptt, HAMLIB_ELAPSED_SET);
        RETURNFUNC(gpio_ptt_get(&rig->state.pttport, ptt));

    case RIG_PTT_NONE:
        RETURNFUNC(-RIG_ENAVAIL);    /* not available */

    default:
        RETURNFUNC(-RIG_EINVAL);
    }

    elapsed_ms(&rig->state.cache.time_ptt, HAMLIB_ELAPSED_SET);
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
 * \RETURNFUNC(RIG_OK) if the operation has been successful, otherwise
 * a negative value if an error occurred (in which case, cause is
 * set appropriately).
 *
 */
int HAMLIB_API rig_get_dcd(RIG *rig, vfo_t vfo, dcd_t *dcd)
{
    const struct rig_caps *caps;
    int retcode, rc2, status;
    vfo_t curr_vfo;

    ENTERFUNC;

    if (CHECK_RIG_ARG(rig) || !dcd)
    {
        RETURNFUNC(-RIG_EINVAL);
    }

    caps = rig->caps;

    switch (rig->state.dcdport.type.dcd)
    {
    case RIG_DCD_RIG:
        if (caps->get_dcd == NULL)
        {
            RETURNFUNC(-RIG_ENIMPL);
        }

        if ((caps->targetable_vfo & RIG_TARGETABLE_PURE)
                || vfo == RIG_VFO_CURR
                || vfo == rig->state.current_vfo)
        {
            RETURNFUNC(caps->get_dcd(rig, vfo, dcd));
        }

        if (!caps->set_vfo)
        {
            RETURNFUNC(-RIG_ENAVAIL);
        }

        curr_vfo = rig->state.current_vfo;
        retcode = caps->set_vfo(rig, vfo);

        if (retcode != RIG_OK)
        {
            RETURNFUNC(retcode);
        }

        retcode = caps->get_dcd(rig, vfo, dcd);
        /* try and revert even if we had an error above */
        rc2 = caps->set_vfo(rig, curr_vfo);

        if (RIG_OK == retcode)
        {
            /* return the first error code */
            retcode = rc2;
        }

        RETURNFUNC(retcode);

        break;

    case RIG_DCD_SERIAL_CTS:
        retcode = ser_get_cts(&rig->state.dcdport, &status);
        *dcd = status ? RIG_DCD_ON : RIG_DCD_OFF;
        RETURNFUNC(retcode);

    case RIG_DCD_SERIAL_DSR:
        retcode = ser_get_dsr(&rig->state.dcdport, &status);
        *dcd = status ? RIG_DCD_ON : RIG_DCD_OFF;
        RETURNFUNC(retcode);

    case RIG_DCD_SERIAL_CAR:
        retcode = ser_get_car(&rig->state.dcdport, &status);
        *dcd = status ? RIG_DCD_ON : RIG_DCD_OFF;
        RETURNFUNC(retcode);


    case RIG_DCD_PARALLEL:
        RETURNFUNC(par_dcd_get(&rig->state.dcdport, dcd));

    case RIG_DCD_GPIO:
    case RIG_DCD_GPION:
        RETURNFUNC(gpio_dcd_get(&rig->state.dcdport, dcd));

    case RIG_DCD_NONE:
        RETURNFUNC(-RIG_ENAVAIL);    /* not available */

    default:
        RETURNFUNC(-RIG_EINVAL);
    }

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
 * \RETURNFUNC(RIG_OK) if the operation has been successful, otherwise
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

    ENTERFUNC;

    if (CHECK_RIG_ARG(rig))
    {
        RETURNFUNC(-RIG_EINVAL);
    }

    caps = rig->caps;

    if (caps->set_rptr_shift == NULL)
    {
        RETURNFUNC(-RIG_ENAVAIL);
    }

    if ((caps->targetable_vfo & RIG_TARGETABLE_PURE)
            || vfo == RIG_VFO_CURR
            || vfo == rig->state.current_vfo)
    {
        RETURNFUNC(caps->set_rptr_shift(rig, vfo, rptr_shift));
    }

    if (!caps->set_vfo)
    {
        RETURNFUNC(-RIG_ENAVAIL);
    }

    curr_vfo = rig->state.current_vfo;
    retcode = caps->set_vfo(rig, vfo);

    if (retcode != RIG_OK)
    {
        RETURNFUNC(retcode);
    }

    retcode = caps->set_rptr_shift(rig, vfo, rptr_shift);
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
 * \brief get the current repeater shift
 * \param rig   The rig handle
 * \param vfo   The target VFO
 * \param rptr_shift    The location where to store the current repeater shift
 *
 *  Retrieves the current repeater shift.
 *
 * \RETURNFUNC(RIG_OK) if the operation has been successful, otherwise
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

    ENTERFUNC;

    if (CHECK_RIG_ARG(rig) || !rptr_shift)
    {
        RETURNFUNC(-RIG_EINVAL);
    }

    caps = rig->caps;

    if (caps->get_rptr_shift == NULL)
    {
        RETURNFUNC(-RIG_ENAVAIL);
    }

    if ((caps->targetable_vfo & RIG_TARGETABLE_PURE)
            || vfo == RIG_VFO_CURR
            || vfo == rig->state.current_vfo)
    {
        RETURNFUNC(caps->get_rptr_shift(rig, vfo, rptr_shift));
    }

    if (!caps->set_vfo)
    {
        RETURNFUNC(-RIG_ENAVAIL);
    }

    curr_vfo = rig->state.current_vfo;
    retcode = caps->set_vfo(rig, vfo);

    if (retcode != RIG_OK)
    {
        RETURNFUNC(retcode);
    }

    retcode = caps->get_rptr_shift(rig, vfo, rptr_shift);
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
 * \brief set the repeater offset
 * \param rig   The rig handle
 * \param vfo   The target VFO
 * \param rptr_offs The VFO to set to
 *
 *  Sets the current repeater offset.
 *
 * \RETURNFUNC(RIG_OK) if the operation has been successful, otherwise
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

    ENTERFUNC;

    if (CHECK_RIG_ARG(rig))
    {
        RETURNFUNC(-RIG_EINVAL);
    }

    caps = rig->caps;

    if (caps->set_rptr_offs == NULL)
    {
        RETURNFUNC(-RIG_ENAVAIL);
    }

    if ((caps->targetable_vfo & RIG_TARGETABLE_PURE)
            || vfo == RIG_VFO_CURR
            || vfo == rig->state.current_vfo)
    {
        RETURNFUNC(caps->set_rptr_offs(rig, vfo, rptr_offs));
    }

    if (!caps->set_vfo)
    {
        RETURNFUNC(-RIG_ENAVAIL);
    }

    curr_vfo = rig->state.current_vfo;
    retcode = caps->set_vfo(rig, vfo);

    if (retcode != RIG_OK)
    {
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
 * \RETURNFUNC(RIG_OK) if the operation has been successful, otherwise
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

    ENTERFUNC;

    if (CHECK_RIG_ARG(rig) || !rptr_offs)
    {
        RETURNFUNC(-RIG_EINVAL);
    }

    caps = rig->caps;

    if (caps->get_rptr_offs == NULL)
    {
        RETURNFUNC(-RIG_ENAVAIL);
    }

    if ((caps->targetable_vfo & RIG_TARGETABLE_PURE)
            || vfo == RIG_VFO_CURR
            || vfo == rig->state.current_vfo)
    {
        RETURNFUNC(caps->get_rptr_offs(rig, vfo, rptr_offs));
    }

    if (!caps->set_vfo)
    {
        RETURNFUNC(-RIG_ENAVAIL);
    }

    curr_vfo = rig->state.current_vfo;
    retcode = caps->set_vfo(rig, vfo);

    if (retcode != RIG_OK)
    {
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
 * \RETURNFUNC(RIG_OK) if the operation has been successful, otherwise
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

    if (CHECK_RIG_ARG(rig))
    {
        RETURNFUNC(-RIG_EINVAL);
    }

    rig_debug(RIG_DEBUG_VERBOSE, "%s called vfo=%s, curr_vfo=%s\n", __func__,
              rig_strvfo(vfo), rig_strvfo(rig->state.current_vfo));

    caps = rig->caps;

    if (caps->set_split_freq
            && ((caps->targetable_vfo & RIG_TARGETABLE_PURE)
                || vfo == RIG_VFO_CURR
                || vfo == RIG_VFO_TX
                || vfo == rig->state.current_vfo))
    {
        RETURNFUNC(caps->set_split_freq(rig, vfo, tx_freq));
    }

    vfo = vfo_fixup(rig, vfo);


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

    if (caps->set_freq && (caps->targetable_vfo & RIG_TARGETABLE_FREQ))
    {
        int retry = 3;
        freq_t tfreq;
        do {
            retcode = rig_set_freq(rig, tx_vfo, tx_freq);
            if (retcode != RIG_OK) RETURNFUNC(retcode);
            retcode = rig_get_freq(rig, tx_vfo, &tfreq);
        } while (tfreq != tx_freq && retry-- > 0 && retcode == RIG_OK);
        RETURNFUNC(retcode);
    }

    if (caps->set_vfo)
    {
        retcode = caps->set_vfo(rig, tx_vfo);
    }
    else if (rig_has_vfo_op(rig, RIG_OP_TOGGLE) && caps->vfo_op)
    {
        retcode = caps->vfo_op(rig, vfo, RIG_OP_TOGGLE);
    }
    else
    {
        RETURNFUNC(-RIG_ENAVAIL);
    }

    if (retcode != RIG_OK)
    {
        RETURNFUNC(retcode);
    }

    int retry = 3;
    freq_t tfreq;
    do {
        if (caps->set_split_freq)
        {
            retcode = caps->set_split_freq(rig, vfo, tx_freq);
            rig_get_freq(rig, vfo, &tfreq);
        }
        else
        {
            retcode = rig_set_freq(rig, RIG_VFO_CURR, tx_freq);
            rig_get_freq(rig, vfo, &tfreq);
        }
    } while(tfreq != tx_freq && retry-- > 0 && retcode == RIG_OK);

    /* try and revert even if we had an error above */
    if (caps->set_vfo)
    {
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

    RETURNFUNC(retcode);
}


/**
 * \brief get the current split frequencies
 * \param rig   The rig handle
 * \param vfo   The target VFO
 * \param tx_freq   The location where to store the current transmit split frequency
 *
 *  Retrieves the current split(TX) frequency.
 *
 * \RETURNFUNC(RIG_OK) if the operation has been successful, otherwise
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

    ENTERFUNC;

    if (CHECK_RIG_ARG(rig) || !tx_freq)
    {
        RETURNFUNC(-RIG_EINVAL);
    }

    vfo = vfo_fixup(rig, vfo);

    caps = rig->caps;

    if (caps->get_split_freq
            && ((caps->targetable_vfo & RIG_TARGETABLE_PURE)
                || vfo == RIG_VFO_CURR
                || vfo == RIG_VFO_TX
                || vfo == rig->state.current_vfo))
    {
        RETURNFUNC(caps->get_split_freq(rig, vfo, tx_freq));
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
        RETURNFUNC(caps->get_freq(rig, tx_vfo, tx_freq));
    }


    if (caps->set_vfo)
    {
        // if the underlying rig has OP_XCHG we don't need to set VFO
        if (!rig_has_vfo_op(rig, RIG_OP_XCHG))
        {
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
        RETURNFUNC(-RIG_ENAVAIL);
    }

    if (retcode != RIG_OK)
    {
        RETURNFUNC(retcode);
    }

    if (caps->get_split_freq)
    {
        retcode = caps->get_split_freq(rig, vfo, tx_freq);
    }
    else
    {
        retcode = caps->get_freq(rig, RIG_VFO_CURR, tx_freq);
    }

    /* try and revert even if we had an error above */
    if (caps->set_vfo)
    {
        // If we started with RIG_VFO_CURR we need to choose VFO_A/MAIN as appropriate to return to
        if (save_vfo == RIG_VFO_CURR)
        {
            save_vfo = VFO_HAS_A_B_ONLY ? RIG_VFO_A : RIG_VFO_MAIN;
        }

        rig_debug(RIG_DEBUG_TRACE, "%s: restoring vfo=%s\n", __func__,
                  rig_strvfo(save_vfo));
        rc2 = caps->set_vfo(rig, save_vfo);
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
 * \RETURNFUNC(RIG_OK) if the operation has been successful, otherwise
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
    vfo_t curr_vfo, tx_vfo;

    ENTERFUNC;

    if (CHECK_RIG_ARG(rig))
    {
        RETURNFUNC(-RIG_EINVAL);
    }

    caps = rig->caps;

    if (caps->set_split_mode
            && ((caps->targetable_vfo & RIG_TARGETABLE_PURE)
                || vfo == RIG_VFO_CURR
                || vfo == RIG_VFO_TX
                || vfo == rig->state.current_vfo))
    {
        RETURNFUNC(caps->set_split_mode(rig, vfo, tx_mode, tx_width));
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

    if (caps->set_mode && (caps->targetable_vfo & RIG_TARGETABLE_MODE))
    {
        RETURNFUNC(caps->set_mode(rig, tx_vfo, tx_mode, tx_width));
    }


    if (caps->set_vfo)
    {
        retcode = caps->set_vfo(rig, tx_vfo);
    }
    else if (rig_has_vfo_op(rig, RIG_OP_TOGGLE) && caps->vfo_op)
    {
        retcode = caps->vfo_op(rig, vfo, RIG_OP_TOGGLE);
    }
    else
    {
        RETURNFUNC(-RIG_ENAVAIL);
    }

    if (retcode != RIG_OK)
    {
        RETURNFUNC(retcode);
    }

    if (caps->set_split_mode)
    {
        retcode = caps->set_split_mode(rig, vfo, tx_mode, tx_width);
    }
    else
    {
        retcode = caps->set_mode(rig, RIG_VFO_CURR, tx_mode, tx_width);
    }

    /* try and revert even if we had an error above */
    if (caps->set_vfo)
    {
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
 * \RETURNFUNC(RIG_OK) if the operation has been successful, otherwise
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

    ENTERFUNC;

    if (CHECK_RIG_ARG(rig) || !tx_mode || !tx_width)
    {
        RETURNFUNC(-RIG_EINVAL);
    }

    caps = rig->caps;

    if (caps->get_split_mode
            && ((caps->targetable_vfo & RIG_TARGETABLE_PURE)
                || vfo == RIG_VFO_CURR
                || vfo == RIG_VFO_TX
                || vfo == rig->state.current_vfo))
    {
        RETURNFUNC(caps->get_split_mode(rig, vfo, tx_mode, tx_width));
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
        RETURNFUNC(caps->get_mode(rig, tx_vfo, tx_mode, tx_width));
    }


    if (caps->set_vfo)
    {
        retcode = caps->set_vfo(rig, tx_vfo);
    }
    else if (rig_has_vfo_op(rig, RIG_OP_TOGGLE) && caps->vfo_op)
    {
        retcode = caps->vfo_op(rig, vfo, RIG_OP_TOGGLE);
    }
    else
    {
        RETURNFUNC(-RIG_ENAVAIL);
    }

    if (retcode != RIG_OK)
    {
        RETURNFUNC(retcode);
    }

    if (caps->get_split_mode)
    {
        retcode = caps->get_split_mode(rig, vfo, tx_mode, tx_width);
    }
    else
    {
        retcode = caps->get_mode(rig, RIG_VFO_CURR, tx_mode, tx_width);
    }

    /* try and revert even if we had an error above */
    if (caps->set_vfo)
    {
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
 * \RETURNFUNC(RIG_OK) if the operation has been successful, otherwise
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

    ENTERFUNC;

    if (CHECK_RIG_ARG(rig))
    {
        RETURNFUNC(-RIG_EINVAL);
    }

    caps = rig->caps;

    if (caps->set_split_freq_mode)
    {
        RETURNFUNC(caps->set_split_freq_mode(rig, vfo, tx_freq, tx_mode, tx_width));
    }

    retcode = rig_set_split_freq(rig, vfo, tx_freq);

    if (RIG_OK == retcode)
    {
        retcode = rig_set_split_mode(rig, vfo, tx_mode, tx_width);
    }

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
 * \RETURNFUNC(RIG_OK) if the operation has been successful, otherwise
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

    ENTERFUNC;

    if (CHECK_RIG_ARG(rig) || !tx_freq || !tx_mode || !tx_width)
    {
        RETURNFUNC(-RIG_EINVAL);
    }

    caps = rig->caps;

    if (caps->get_split_freq_mode)
    {
        RETURNFUNC(caps->get_split_freq_mode(rig, vfo, tx_freq, tx_mode, tx_width));
    }

    retcode = rig_get_split_freq(rig, vfo, tx_freq);

    if (RIG_OK == retcode)
    {
        retcode = rig_get_split_mode(rig, vfo, tx_mode, tx_width);
    }

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
 * \RETURNFUNC(RIG_OK) if the operation has been successful, otherwise
 * a negative value if an error occurred (in which case, cause is
 * set appropriately).
 *
 * \sa rig_get_split_vfo()
 */
int HAMLIB_API rig_set_split_vfo(RIG *rig,
                                 vfo_t vfo,
                                 split_t split,
                                 vfo_t tx_vfo)
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

    if (caps->set_split_vfo == NULL)
    {
        RETURNFUNC(-RIG_ENAVAIL);
    }

    vfo = vfo_fixup(rig, vfo);

    if ((caps->targetable_vfo & RIG_TARGETABLE_PURE)
            || vfo == RIG_VFO_CURR
            || vfo == rig->state.current_vfo)
    {
        retcode = caps->set_split_vfo(rig, vfo, split, tx_vfo);

        if (retcode == RIG_OK)
        {
            rig->state.tx_vfo = tx_vfo;
        }

        rig->state.cache.split = split;
        rig->state.cache.split_vfo = tx_vfo;
        elapsed_ms(&rig->state.cache.time_split, HAMLIB_ELAPSED_SET);
        RETURNFUNC(retcode);
    }

    if (!caps->set_vfo)
    {
        RETURNFUNC(-RIG_ENAVAIL);
    }

    curr_vfo = rig->state.current_vfo;
    retcode = caps->set_vfo(rig, vfo);

    if (retcode != RIG_OK)
    {
        RETURNFUNC(retcode);
    }

    retcode = caps->set_split_vfo(rig, vfo, split, tx_vfo);

    /* try and revert even if we had an error above */
    rc2 = caps->set_vfo(rig, curr_vfo);

    if (RIG_OK == retcode)
    {
        /* return the first error code */
        retcode = rc2;
    }

    if (retcode == RIG_OK)
    {
        rig->state.tx_vfo = tx_vfo;
    }

    rig->state.cache.split = split;
    rig->state.cache.split_vfo = tx_vfo;
    elapsed_ms(&rig->state.cache.time_split, HAMLIB_ELAPSED_SET);
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
 * \RETURNFUNC(RIG_OK) if the operation has been successful, otherwise
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

    ENTERFUNC;

    if (CHECK_RIG_ARG(rig) || !split || !tx_vfo)
    {
        RETURNFUNC(-RIG_EINVAL);
    }

    caps = rig->caps;

    if (caps->get_split_vfo == NULL)
    {
        RETURNFUNC(-RIG_ENAVAIL);
    }

    cache_ms = elapsed_ms(&rig->state.cache.time_split, HAMLIB_ELAPSED_GET);
    rig_debug(RIG_DEBUG_TRACE, "%s: cache check age=%dms\n", __func__, cache_ms);

    if (cache_ms < rig->state.cache.timeout_ms)
    {
        rig_debug(RIG_DEBUG_TRACE, "%s: cache hit age=%dms\n", __func__, cache_ms);
        *split = rig->state.cache.split;
        *tx_vfo = rig->state.cache.split_vfo;
        RETURNFUNC(RIG_OK);
    }
    else
    {
        rig_debug(RIG_DEBUG_TRACE, "%s: cache miss age=%dms\n", __func__, cache_ms);
    }

    /* overridden by backend at will */
    *tx_vfo = rig->state.tx_vfo;

    if ((caps->targetable_vfo & RIG_TARGETABLE_PURE)
            || vfo == RIG_VFO_CURR
            || vfo == rig->state.current_vfo)
    {
        retcode = caps->get_split_vfo(rig, vfo, split, tx_vfo);
        rig->state.cache.split = *split;
        rig->state.cache.split_vfo = *tx_vfo;
        elapsed_ms(&rig->state.cache.time_split, HAMLIB_ELAPSED_SET);
        RETURNFUNC(retcode);
    }

    if (!caps->set_vfo)
    {
        RETURNFUNC(-RIG_ENAVAIL);
    }

#if 0 // why were we doing this?  Shouldn't need to set_vfo to figure out tx_vfo
    curr_vfo = rig->state.current_vfo;
    retcode = caps->set_vfo(rig, vfo);

    if (retcode != RIG_OK)
    {
        RETURNFUNC(retcode);
    }

#endif

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
    }

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
 * \RETURNFUNC(RIG_OK) if the operation has been successful, otherwise
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
        RETURNFUNC(caps->set_rit(rig, vfo, rit));
    }

    if (!caps->set_vfo)
    {
        RETURNFUNC(-RIG_ENAVAIL);
    }

    curr_vfo = rig->state.current_vfo;
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
 * \RETURNFUNC(RIG_OK) if the operation has been successful, otherwise
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

    if (CHECK_RIG_ARG(rig) || !rit)
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
        RETURNFUNC(caps->get_rit(rig, vfo, rit));
    }

    if (!caps->set_vfo)
    {
        RETURNFUNC(-RIG_ENAVAIL);
    }

    curr_vfo = rig->state.current_vfo;
    retcode = caps->set_vfo(rig, vfo);

    if (retcode != RIG_OK)
    {
        RETURNFUNC(retcode);
    }

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
 * \RETURNFUNC(RIG_OK) if the operation has been successful, otherwise
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
        RETURNFUNC(caps->set_xit(rig, vfo, xit));
    }

    if (!caps->set_vfo)
    {
        RETURNFUNC(-RIG_ENAVAIL);
    }

    curr_vfo = rig->state.current_vfo;
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
 * \RETURNFUNC(RIG_OK) if the operation has been successful, otherwise
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

    if (CHECK_RIG_ARG(rig) || !xit)
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
        RETURNFUNC(caps->get_xit(rig, vfo, xit));
    }

    if (!caps->set_vfo)
    {
        RETURNFUNC(-RIG_ENAVAIL);
    }

    curr_vfo = rig->state.current_vfo;
    retcode = caps->set_vfo(rig, vfo);

    if (retcode != RIG_OK)
    {
        RETURNFUNC(retcode);
    }

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
 * \RETURNFUNC(RIG_OK) if the operation has been successful, otherwise
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

    if ((caps->targetable_vfo & RIG_TARGETABLE_PURE)
            || vfo == RIG_VFO_CURR
            || vfo == rig->state.current_vfo)
    {
        RETURNFUNC(caps->set_ts(rig, vfo, ts));
    }

    if (!caps->set_vfo)
    {
        RETURNFUNC(-RIG_ENAVAIL);
    }

    curr_vfo = rig->state.current_vfo;
    retcode = caps->set_vfo(rig, vfo);

    if (retcode != RIG_OK)
    {
        RETURNFUNC(retcode);
    }

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
 * \RETURNFUNC(RIG_OK) if the operation has been successful, otherwise
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

    if (CHECK_RIG_ARG(rig) || !ts)
    {
        RETURNFUNC(-RIG_EINVAL);
    }

    caps = rig->caps;

    if (caps->get_ts == NULL)
    {
        RETURNFUNC(-RIG_ENAVAIL);
    }

    if ((caps->targetable_vfo & RIG_TARGETABLE_PURE)
            || vfo == RIG_VFO_CURR
            || vfo == rig->state.current_vfo)
    {
        RETURNFUNC(caps->get_ts(rig, vfo, ts));
    }

    if (!caps->set_vfo)
    {
        RETURNFUNC(-RIG_ENAVAIL);
    }

    curr_vfo = rig->state.current_vfo;
    retcode = caps->set_vfo(rig, vfo);

    if (retcode != RIG_OK)
    {
        RETURNFUNC(retcode);
    }

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
 * \RETURNFUNC(RIG_OK) if the operation has been successful, otherwise
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
        RETURNFUNC(caps->set_ant(rig, vfo, ant, option));
    }

    if (!caps->set_vfo)
    {
        RETURNFUNC(-RIG_ENAVAIL);
    }

    curr_vfo = rig->state.current_vfo;
    retcode = caps->set_vfo(rig, vfo);

    if (retcode != RIG_OK)
    {
        RETURNFUNC(retcode);
    }

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
 * \param ant   The location where to store the current antenna
 * \param option  The option value for the antenna
 * \param ant_curr  The currently selected antenna
 * \param ant_tx  The currently selected TX antenna
 * \param ant_rx  The currently selected RX antenna
 *
 *  Retrieves the current antenna.
 *
 * \RETURNFUNC(RIG_OK) if the operation has been successful, otherwise
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

    *ant_tx = *ant_rx = RIG_ANT_UNKNOWN;

    if (CHECK_RIG_ARG(rig) || !ant_curr)
    {
        RETURNFUNC(-RIG_EINVAL);
    }

    caps = rig->caps;

    if (caps->get_ant == NULL)
    {
        RETURNFUNC(-RIG_ENAVAIL);
    }

    if ((caps->targetable_vfo & RIG_TARGETABLE_ANT)
            || vfo == RIG_VFO_CURR
            || vfo == rig->state.current_vfo)
    {
        RETURNFUNC(caps->get_ant(rig, vfo, ant, option, ant_curr, ant_tx, ant_rx));
    }

    if (!caps->set_vfo)
    {
        RETURNFUNC(-RIG_ENAVAIL);
    }

    curr_vfo = rig->state.current_vfo;
    retcode = caps->set_vfo(rig, vfo);

    if (retcode != RIG_OK)
    {
        RETURNFUNC(retcode);
    }

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
 * \RETURNFUNC(RIG_OK) if the operation has been successful, otherwise
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

    if (!txrange)
    {
        /*
         * freq is not on the tx range!
         */
        RETURNFUNC(-RIG_ECONF); /* could be RIG_EINVAL ? */
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
 * \RETURNFUNC(RIG_OK) if the operation has been successful, otherwise
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

    ENTERFUNC;

    if (!rig || !rig->caps || !power || mwpower == 0)
    {
        RETURNFUNC(-RIG_EINVAL);
    }

    if (rig->caps->mW2power != NULL)
    {
        RETURNFUNC(rig->caps->mW2power(rig, power, mwpower, freq, mode));
    }

    txrange = rig_get_range(rig->state.tx_range_list, freq, mode);

    if (!txrange)
    {
        /*
         * freq is not on the tx range!
         */
        RETURNFUNC(-RIG_ECONF); /* could be RIG_EINVAL ? */
    }

    if (txrange->high_power == 0)
    {
        *power = 0.0;
        RETURNFUNC(RIG_OK);
    }

    *power = (float)mwpower / txrange->high_power;

    if (*power > 1.0)
    {
        *power = 1.0;
    }

    RETURNFUNC(mwpower > txrange->high_power ? RIG_OK : -RIG_ETRUNC);
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

    for (i = 0; i < TSLSTSIZ && rs->tuning_steps[i].ts; i++)
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
 * \RETURNFUNC(RIG_OK) if the operation has been successful, ortherwise
 * a negative value if an error occurred (in which case, cause is
 * set appropriately).
 *
 * \sa rig_get_powerstat()
 */
int HAMLIB_API rig_set_powerstat(RIG *rig, powerstat_t status)
{
    ENTERFUNC;

    if (CHECK_RIG_ARG(rig))
    {
        RETURNFUNC(-RIG_EINVAL);
    }

    if (rig->caps->set_powerstat == NULL)
    {
        rig_debug(RIG_DEBUG_WARN, "%s set_powerstat not implemented\n", __func__);
        RETURNFUNC(-RIG_ENAVAIL);
    }

    RETURNFUNC(rig->caps->set_powerstat(rig, status));
}


/**
 * \brief get the on/off status of the radio
 * \param rig   The rig handle
 * \param status    The location where to store the current status
 *
 *  Retrieve the status of the radio. See RIG_POWER_ON, RIG_POWER_OFF and
 *  RIG_POWER_STANDBY defines for the \a status.
 *
 * \RETURNFUNC(RIG_OK) if the operation has been successful, otherwise
 * a negative value if an error occurred (in which case, cause is
 * set appropriately).
 *
 * \sa rig_set_powerstat()
 */
int HAMLIB_API rig_get_powerstat(RIG *rig, powerstat_t *status)
{
    ENTERFUNC;

    if (CHECK_RIG_ARG(rig) || !status)
    {
        RETURNFUNC(-RIG_EINVAL);
    }

    if (rig->caps->get_powerstat == NULL)
    {
        RETURNFUNC(-RIG_ENAVAIL);
    }

    RETURNFUNC(rig->caps->get_powerstat(rig, status));
}


/**
 * \brief reset the radio
 * \param rig   The rig handle
 * \param reset The reset operation to perform
 *
 *  Resets the radio.
 *  See RIG_RESET_NONE, RIG_RESET_SOFT and RIG_RESET_MCALL defines
 *  for the \a reset.
 *
 * \RETURNFUNC(RIG_OK) if the operation has been successful, otherwise
 * a negative value if an error occurred (in which case, cause is
 * set appropriately).
 *
 */
int HAMLIB_API rig_reset(RIG *rig, reset_t reset)
{
    ENTERFUNC;

    if (CHECK_RIG_ARG(rig))
    {
        RETURNFUNC(-RIG_EINVAL);
    }

    if (rig->caps->reset == NULL)
    {
        RETURNFUNC(-RIG_ENAVAIL);
    }

    RETURNFUNC(rig->caps->reset(rig, reset));
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
    ENTERFUNC;

    if (!port)
    {
        RETURNFUNC(RIG_MODEL_NONE);
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
 * \RETURNFUNC(RIG_OK) if the operation has been successful, otherwise
 * a negative value if an error occurred (in which case, cause is
 * set appropriately).
 */
int HAMLIB_API rig_probe_all(hamlib_port_t *port,
                             rig_probe_func_t cfunc,
                             rig_ptr_t data)
{
    ENTERFUNC;

    if (!port)
    {
        RETURNFUNC(-RIG_EINVAL);
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
    ENTERFUNC;

    if (!rig || !rig->caps)
    {
        RETURNFUNC(0);
    }

    RETURNFUNC(rig->caps->vfo_ops & op);
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
 * \RETURNFUNC(RIG_OK) if the operation has been successful, otherwise
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

    if (CHECK_RIG_ARG(rig))
    {
        RETURNFUNC(-RIG_EINVAL);
    }

    caps = rig->caps;

    if (caps->vfo_op == NULL || !rig_has_vfo_op(rig, op))
    {
        RETURNFUNC(-RIG_ENAVAIL);
    }

    if ((caps->targetable_vfo & RIG_TARGETABLE_PURE)
            || vfo == RIG_VFO_CURR
            || vfo == rig->state.current_vfo)
    {
        RETURNFUNC(caps->vfo_op(rig, vfo, op));
    }

    if (!caps->set_vfo)
    {
        RETURNFUNC(-RIG_ENAVAIL);
    }

    curr_vfo = rig->state.current_vfo;
    retcode = caps->set_vfo(rig, vfo);

    if (retcode != RIG_OK)
    {
        RETURNFUNC(retcode);
    }

    retcode = caps->vfo_op(rig, vfo, op);
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
    ENTERFUNC;

    if (!rig || !rig->caps)
    {
        RETURNFUNC(0);
    }

    RETURNFUNC(rig->caps->scan_ops & scan);
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
 * \RETURNFUNC(RIG_OK) if the operation has been successful, otherwise
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

    if ((caps->targetable_vfo & RIG_TARGETABLE_PURE)
            || vfo == RIG_VFO_CURR
            || vfo == rig->state.current_vfo)
    {
        RETURNFUNC(caps->scan(rig, vfo, scan, ch));
    }

    if (!caps->set_vfo)
    {
        RETURNFUNC(-RIG_ENAVAIL);
    }

    curr_vfo = rig->state.current_vfo;
    retcode = caps->set_vfo(rig, vfo);

    if (retcode != RIG_OK)
    {
        RETURNFUNC(retcode);
    }

    retcode = caps->scan(rig, vfo, scan, ch);
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
 * \brief send DTMF digits
 * \param rig   The rig handle
 * \param vfo   The target VFO
 * \param digits    Digits to be send
 *
 *  Sends DTMF digits.
 *  See DTMF change speed, etc. (TODO).
 *
 * \RETURNFUNC(RIG_OK) if the operation has been successful, otherwise
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

    if (CHECK_RIG_ARG(rig) || !digits)
    {
        RETURNFUNC(-RIG_EINVAL);
    }

    caps = rig->caps;

    if (caps->send_dtmf == NULL)
    {
        RETURNFUNC(-RIG_ENAVAIL);
    }

    if ((caps->targetable_vfo & RIG_TARGETABLE_PURE)
            || vfo == RIG_VFO_CURR
            || vfo == rig->state.current_vfo)
    {
        RETURNFUNC(caps->send_dtmf(rig, vfo, digits));
    }

    if (!caps->set_vfo)
    {
        RETURNFUNC(-RIG_ENAVAIL);
    }

    curr_vfo = rig->state.current_vfo;
    retcode = caps->set_vfo(rig, vfo);

    if (retcode != RIG_OK)
    {
        RETURNFUNC(retcode);
    }

    retcode = caps->send_dtmf(rig, vfo, digits);
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
 * \brief receive DTMF digits
 * \param rig   The rig handle
 * \param vfo   The target VFO
 * \param digits    Location where the digits are to be stored
 * \param length    in: max length of buffer, out: number really read.
 *
 *  Receives DTMF digits (not blocking).
 *  See DTMF change speed, etc. (TODO).
 *
 * \RETURNFUNC(RIG_OK) if the operation has been successful, otherwise
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

    if (CHECK_RIG_ARG(rig) || !digits || !length)
    {
        RETURNFUNC(-RIG_EINVAL);
    }

    caps = rig->caps;

    if (caps->recv_dtmf == NULL)
    {
        RETURNFUNC(-RIG_ENAVAIL);
    }

    if ((caps->targetable_vfo & RIG_TARGETABLE_PURE)
            || vfo == RIG_VFO_CURR
            || vfo == rig->state.current_vfo)
    {
        RETURNFUNC(caps->recv_dtmf(rig, vfo, digits, length));
    }

    if (!caps->set_vfo)
    {
        RETURNFUNC(-RIG_ENAVAIL);
    }

    curr_vfo = rig->state.current_vfo;
    retcode = caps->set_vfo(rig, vfo);

    if (retcode != RIG_OK)
    {
        RETURNFUNC(retcode);
    }

    retcode = caps->recv_dtmf(rig, vfo, digits, length);
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
 * \brief send morse code
 * \param rig   The rig handle
 * \param vfo   The target VFO
 * \param msg   Message to be sent
 *
 *  Sends morse message.
 *  See keyer change speed, etc. (TODO).
 *
 * \RETURNFUNC(RIG_OK) if the operation has been successful, otherwise
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

    if (CHECK_RIG_ARG(rig) || !msg)
    {
        RETURNFUNC(-RIG_EINVAL);
    }

    caps = rig->caps;

    if (caps->send_morse == NULL)
    {
        RETURNFUNC(-RIG_ENAVAIL);
    }

    if ((caps->targetable_vfo & RIG_TARGETABLE_PURE)
            || vfo == RIG_VFO_CURR
            || vfo == rig->state.current_vfo)
    {
        RETURNFUNC(caps->send_morse(rig, vfo, msg));
    }

    if (!caps->set_vfo)
    {
        RETURNFUNC(-RIG_ENAVAIL);
    }

    curr_vfo = rig->state.current_vfo;
    retcode = caps->set_vfo(rig, vfo);

    if (retcode != RIG_OK)
    {
        RETURNFUNC(retcode);
    }

    retcode = caps->send_morse(rig, vfo, msg);
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
 * \brief stop morse code
 * \param rig   The rig handle
 * \param vfo   The target VFO
 *
 *  Stops the send morse message.
 *
 * \RETURNFUNC(RIG_OK) if the operation has been successful, otherwise
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
    caps = rig->caps;

    if (caps->stop_morse == NULL)
    {
        RETURNFUNC(-RIG_ENAVAIL);
    }

    if ((caps->targetable_vfo & RIG_TARGETABLE_PURE)
            || vfo == RIG_VFO_CURR
            || vfo == rig->state.current_vfo)
    {
        RETURNFUNC(caps->stop_morse(rig, vfo));
    }

    if (!caps->set_vfo)
    {
        RETURNFUNC(-RIG_ENAVAIL);
    }

    curr_vfo = rig->state.current_vfo;
    retcode = caps->set_vfo(rig, vfo);

    if (retcode != RIG_OK)
    {
        RETURNFUNC(retcode);
    }

    retcode = caps->stop_morse(rig, vfo);
    /* try and revert even if we had an error above */
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
 * \RETURNFUNC(RIG_OK) if the operation has been successful, otherwise
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
    caps = rig->caps;

    if ((caps->targetable_vfo & RIG_TARGETABLE_PURE)
            || vfo == RIG_VFO_CURR
            || vfo == rig->state.current_vfo)
    {
        RETURNFUNC(wait_morse_ptt(rig, vfo));
    }

    if (!caps->set_vfo)
    {
        RETURNFUNC(-RIG_ENAVAIL);
    }

    curr_vfo = rig->state.current_vfo;
    retcode = caps->set_vfo(rig, vfo);

    if (retcode != RIG_OK)
    {
        RETURNFUNC(retcode);
    }

    retcode = wait_morse_ptt(rig, vfo);
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
 * \brief send voice memory content
 * \param rig   The rig handle
 * \param vfo   The target VFO
 * \param ch    Voice memory number to be sent
 *
 *  Sends voice memory content.
 *
 * \RETURNFUNC(RIG_OK) if the operation has been successful, otherwise
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

    if ((caps->targetable_vfo & RIG_TARGETABLE_PURE)
            || vfo == RIG_VFO_CURR
            || vfo == rig->state.current_vfo)
    {
        RETURNFUNC(caps->send_voice_mem(rig, vfo, ch));
    }

    if (!caps->set_vfo)
    {
        RETURNFUNC(-RIG_ENAVAIL);
    }

    curr_vfo = rig->state.current_vfo;
    retcode = caps->set_vfo(rig, vfo);

    if (retcode != RIG_OK)
    {
        RETURNFUNC(retcode);
    }

    retcode = caps->send_voice_mem(rig, vfo, ch);
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

    ENTERFUNC;

    for (i = 0; i < FRQRANGESIZ; i++)
    {
        if (range_list[i].startf == 0 && range_list[i].endf == 0)
        {
            RETURNFUNC(NULL);
        }

        if (freq >= range_list[i].startf && freq <= range_list[i].endf &&
                (range_list[i].modes & mode))
        {
            const freq_range_t *f = &range_list[i];
            RETURNFUNC(f);
        }
    }

    RETURNFUNC(NULL);
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
    ENTERFUNC;

    if (rig->caps->set_vfo_opt == NULL)
    {
        RETURNFUNC(-RIG_ENAVAIL);
    }

    RETURNFUNC(rig->caps->set_vfo_opt(rig, status));
}

/**
 * \brief get general information from the radio
 * \param rig   The rig handle
 *
 * Retrieves some general information from the radio.
 * This can include firmware revision, exact model name, or just nothing.
 *
 * \return a pointer to freshly allocated memory containing the ASCIIZ string
 * if the operation has been successful, otherwise NULL if an error occurred
 * or get_info not part of capabilities.
 */
const char *HAMLIB_API rig_get_info(RIG *rig)
{
    ENTERFUNC;

    if (CHECK_RIG_ARG(rig))
    {
        RETURNFUNC(NULL);
    }

    if (rig->caps->get_info == NULL)
    {
        RETURNFUNC(NULL);
    }

    RETURNFUNC(rig->caps->get_info(rig));
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
 * \RETURNFUNC(RIG_OK) if the operation has been successful, otherwise
 * a negative value if an error occurred (in which case use rigerror(return)
 * for error message).
 *
 */
int HAMLIB_API rig_get_vfo_info(RIG *rig, vfo_t vfo, freq_t *freq, rmode_t *mode, pbwidth_t *width)
{
    int retcode;

    ENTERFUNC;
    rig_debug(RIG_DEBUG_VERBOSE, "%s called vfo=%s\n", __func__, rig_strvfo(vfo));

    if (CHECK_RIG_ARG(rig))
    {
        RETURNFUNC(-RIG_EINVAL);
    }

    retcode = rig_get_freq(rig,vfo,freq);
    if (retcode != RIG_OK) RETURNFUNC(retcode);
    retcode = rig_get_mode(rig,vfo,mode,width);
    RETURNFUNC(retcode);
}

/**
 * \brief get list of available vfos 
 * \param rig   The rig handle
 *
 * Retrieves all usable vfo entries for the rig
 *
 * \return a pointer to a string, e.g. "VFOA VFOB Mem"
 * if the operation has been successful, otherwise NULL if an error occurred
 */
const char *HAMLIB_API rig_get_vfo_list(RIG *rig)
{
    ENTERFUNC;

    if (CHECK_RIG_ARG(rig))
    {
        RETURNFUNC(NULL);
    }

    RETURNFUNC(RIG_OK);
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

/*! @} */
