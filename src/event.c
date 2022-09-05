/*
 *  Hamlib Interface - event handling
 *  Copyright (c) 2021 by Mikael Nousiainen
 *  Copyright (c) 2000-2010 by Stephane Fillod
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

/* Doc todo: Verify assignment to rig group.  Consider doc of internal rtns. */
/**
 * \addtogroup rig
 * @{
 */

/**
 * \file event.c
 * \brief Event handling
 */

#include <hamlib/config.h>

#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <sys/types.h>
#include <errno.h>

#ifdef HAVE_PTHREAD
#  include <pthread.h>
#endif

#include <hamlib/rig.h>
#include "event.h"
#include "misc.h"
#include "cache.h"
#include "network.h"

#define CHECK_RIG_ARG(r) (!(r) || !(r)->caps || !(r)->state.comm_state)

#ifdef HAVE_PTHREAD
typedef struct rig_poll_routine_args_s
{
    RIG *rig;
} rig_poll_routine_args;

typedef struct rig_poll_routine_priv_data_s
{
    pthread_t thread_id;
    rig_poll_routine_args args;
} rig_poll_routine_priv_data;

// TODO: Where to start/stop rig poll routine?

void *rig_poll_routine(void *arg)
{
    rig_poll_routine_args *args = (rig_poll_routine_args *)arg;
    RIG *rig = args->rig;
    struct rig_state *rs = &rig->state;
    int result;
    int update_occurred;

    vfo_t vfo = RIG_VFO_NONE, vfo_prev = RIG_VFO_NONE;
    freq_t freq_main = 0, freq_sub = 0, freq_main_prev = 0, freq_sub_prev = 0;
    rmode_t mode_main = RIG_MODE_NONE, mode_sub = RIG_MODE_NONE,
            mode_main_prev = RIG_MODE_NONE, mode_sub_prev = RIG_MODE_NONE;
    pbwidth_t width_main = 0, width_sub = 0, width_main_prev = 0,
              width_sub_prev = 0;
    split_t split, split_prev = -1;

    rig_debug(RIG_DEBUG_VERBOSE, "%s(%d): Starting rig poll routine thread\n",
              __FILE__, __LINE__);

    // Rig cache time should be equal to rig poll interval (should be set automatically by rigctld at least)
    rig_set_cache_timeout_ms(rig, HAMLIB_CACHE_ALL, rs->poll_interval);

    update_occurred = 0;

    while (rs->poll_routine_thread_run)
    {
        if (rig->caps->get_vfo)
        {
            result = rig_get_vfo(rig, &vfo);

            if (result != RIG_OK)
            {
                rig_debug(RIG_DEBUG_ERR, "%s(%d): rig_get_vfo error %s\n", __FILE__, __LINE__,
                          rigerror(result));
            }

            if (vfo != vfo_prev)
            {
                rig_fire_vfo_event(rig, vfo);
            }

            if (vfo != vfo_prev)
            {
                rig_debug(RIG_DEBUG_CACHE,
                          "%s(%d) vfo=%s was %s\n", __FILE__, __LINE__,
                          rig_strvfo(vfo), rig_strvfo(vfo_prev));
                update_occurred = 1;
                vfo_prev = vfo;
            }
        }

        if (rig->caps->get_freq)
        {
            result = rig_get_freq(rig, RIG_VFO_A, &freq_main);

            if (result != RIG_OK)
            {
                rig_debug(RIG_DEBUG_ERR, "%s(%d): rig_get_freqA error %s\n", __FILE__, __LINE__,
                          rigerror(result));
            }

            result = rig_get_freq(rig, RIG_VFO_B, &freq_sub);

            if (result != RIG_OK)
            {
                rig_debug(RIG_DEBUG_ERR, "%s(%d): rig_get_freqB error %s\n", __FILE__, __LINE__,
                          rigerror(result));
            }

            if (freq_main != freq_main_prev)
            {
                rig_fire_freq_event(rig, RIG_VFO_A, freq_main);
            }

            if (freq_sub != freq_sub_prev)
            {
                rig_fire_freq_event(rig, RIG_VFO_B, freq_sub);
            }

            if (freq_main != freq_main_prev || freq_sub != freq_sub_prev)
            {
                rig_debug(RIG_DEBUG_CACHE,
                          "%s(%d) freq_main=%.0f was %.0f, freq_sub=%.0f was %.0f\n", __FILE__, __LINE__,
                          freq_main, freq_main_prev, freq_sub, freq_sub_prev);
                update_occurred = 1;
                freq_main_prev = freq_main;
                freq_sub_prev = freq_sub;
            }
        }

        if (rig->caps->get_mode)
        {
            result = rig_get_mode(rig, RIG_VFO_A, &mode_main, &width_main);

            if (result != RIG_OK)
            {
                rig_debug(RIG_DEBUG_ERR, "%s(%d): rig_get_modeA error %s\n", __FILE__, __LINE__,
                          rigerror(result));
            }

            result = rig_get_mode(rig, RIG_VFO_B, &mode_sub, &width_sub);

            if (result != RIG_OK)
            {
                rig_debug(RIG_DEBUG_ERR, "%s(%d): rig_get_modeB error %s\n", __FILE__, __LINE__,
                          rigerror(result));
            }

            if (mode_main != mode_main_prev || width_main != width_main_prev)
            {
                rig_fire_mode_event(rig, RIG_VFO_A, mode_main, width_main);
            }

            if (mode_sub != mode_sub_prev || width_sub != width_sub_prev)
            {
                rig_fire_mode_event(rig, RIG_VFO_B, mode_sub, width_sub);
            }

            if (mode_main != mode_main_prev || mode_sub != mode_sub_prev)
            {
                rig_debug(RIG_DEBUG_CACHE, "%s(%d) mode_main=%s was %s, mode_sub=%s was %s\n",
                          __FILE__, __LINE__, rig_strrmode(mode_main), rig_strrmode(mode_main_prev),
                          rig_strrmode(mode_sub), rig_strrmode(mode_sub_prev));
                update_occurred = 1;
                mode_main_prev = mode_main;
                mode_sub_prev = mode_sub;
            }

            if (width_main != width_main_prev || width_sub != width_sub_prev)
            {
                rig_debug(RIG_DEBUG_CACHE,
                          "%s(%d) width_main=%ld was %ld, width_sub=%ld was %ld\n", __FILE__, __LINE__,
                          width_main, width_main_prev, width_sub, width_sub_prev);
                update_occurred = 1;
                width_main_prev = width_main;
                width_sub_prev = width_sub;
            }
        }

        if (rig->caps->get_split_vfo)
        {
            vfo_t tx_vfo;
            result = rig_get_split_vfo(rig, RIG_VFO_A, &split, &tx_vfo);

            if (result != RIG_OK)
            {
                rig_debug(RIG_DEBUG_ERR, "%s(%d): rig_get_modeA error %s\n", __FILE__, __LINE__,
                          rigerror(result));
            }

            if (split != split_prev)
            {
                rig_debug(RIG_DEBUG_CACHE, "%s(%d) split=%d was %d\n", __FILE__, __LINE__,
                          split,
                          split_prev);
                update_occurred = 1;
                split_prev = split;
            }
        }

        if (update_occurred)
        {
            network_publish_rig_poll_data(rig);
        }

        hl_usleep(rs->poll_interval * 1000);
    }

    rig_debug(RIG_DEBUG_VERBOSE, "%s(%d): Stopping rig poll routine thread\n",
              __FILE__,
              __LINE__);

    return NULL;
}

/**
 * \brief Start rig poll routine
 *
 * Start rig poll routine
 *
 * \return RIG_OK or < 0 if error
 */
int rig_poll_routine_start(RIG *rig)
{
    struct rig_state *rs = &rig->state;
    rig_poll_routine_priv_data *poll_routine_priv;

    ENTERFUNC;

    if (rs->poll_interval < 1)
    {
        rig_debug(RIG_DEBUG_ERR,
                  "%s(%d): rig poll routine disabled, poll interval set to zero\n", __FILE__,
                  __LINE__);
        RETURNFUNC(RIG_OK);
    }

    if (rs->poll_routine_priv_data != NULL)
    {
        rig_debug(RIG_DEBUG_ERR, "%s(%d): rig poll routine already running\n", __FILE__,
                  __LINE__);
        RETURNFUNC(-RIG_EINVAL);
    }

    rs->poll_routine_thread_run = 1;
    rs->poll_routine_priv_data = calloc(1, sizeof(rig_poll_routine_priv_data));

    if (rs->poll_routine_priv_data == NULL)
    {
        RETURNFUNC(-RIG_ENOMEM);
    }

    poll_routine_priv = (rig_poll_routine_priv_data *) rs->poll_routine_priv_data;
    poll_routine_priv->args.rig = rig;
    int err = pthread_create(&poll_routine_priv->thread_id, NULL,
                             rig_poll_routine, &poll_routine_priv->args);

    if (err)
    {
        rig_debug(RIG_DEBUG_ERR, "%s(%d) pthread_create error: %s\n", __FILE__,
                  __LINE__,
                  strerror(errno));
        RETURNFUNC(-RIG_EINTERNAL);
    }

    RETURNFUNC(RIG_OK);
}

/**
 * \brief Stop rig poll routine
 *
 * Stop rig poll routine
 *
 * \return RIG_OK or < 0 if error
 */
int rig_poll_routine_stop(RIG *rig)
{
    struct rig_state *rs = &rig->state;
    rig_poll_routine_priv_data *poll_routine_priv;

    ENTERFUNC;

    if (rs->poll_interval < 1)
    {
        RETURNFUNC(RIG_OK);
    }

    if (rs->poll_routine_priv_data == NULL)
    {
        RETURNFUNC(-RIG_EINVAL);
    }

    rs->poll_routine_thread_run = 0;

    poll_routine_priv = (rig_poll_routine_priv_data *) rs->poll_routine_priv_data;

    if (poll_routine_priv->thread_id != 0)
    {
        int err = pthread_join(poll_routine_priv->thread_id, NULL);

        if (err)
        {
            rig_debug(RIG_DEBUG_ERR, "%s(%d): pthread_join error %s\n", __FILE__, __LINE__,
                      strerror(errno));
            // just ignore it
        }

        poll_routine_priv->thread_id = 0;
    }

    free(rs->poll_routine_priv_data);
    rs->poll_routine_priv_data = NULL;

    RETURNFUNC(RIG_OK);
}

#endif

/**
 * \brief set the callback for freq events
 * \param rig   The rig handle
 * \param cb    The callback to install
 * \param arg   A Pointer to some private data to pass later on to the callback
 *
 *  Install a callback for freq events, to be called when in async mode.
 *
 * \return RIG_OK if the operation has been successful, otherwise
 * a negative value if an error occurred (in which case, cause is
 * set appropriately).
 */
int HAMLIB_API rig_set_freq_callback(RIG *rig, freq_cb_t cb, rig_ptr_t arg)
{
    ENTERFUNC;

    if (CHECK_RIG_ARG(rig))
    {
        RETURNFUNC(-RIG_EINVAL);
    }

    rig->callbacks.freq_event = cb;
    rig->callbacks.freq_arg = arg;

    RETURNFUNC(RIG_OK);
}


/**
 * \brief set the callback for mode events
 * \param rig   The rig handle
 * \param cb    The callback to install
 * \param arg   A Pointer to some private data to pass later on to the callback
 *
 *  Install a callback for mode events, to be called when in async mode.
 *
 * \return RIG_OK if the operation has been successful, otherwise
 * a negative value if an error occurred (in which case, cause is
 * set appropriately).
 */
int HAMLIB_API rig_set_mode_callback(RIG *rig, mode_cb_t cb, rig_ptr_t arg)
{
    ENTERFUNC;

    if (CHECK_RIG_ARG(rig))
    {
        RETURNFUNC(-RIG_EINVAL);
    }

    rig->callbacks.mode_event = cb;
    rig->callbacks.mode_arg = arg;

    RETURNFUNC(RIG_OK);
}


/**
 * \brief set the callback for vfo events
 * \param rig   The rig handle
 * \param cb    The callback to install
 * \param arg   A Pointer to some private data to pass later on to the callback
 *
 *  Install a callback for vfo events, to be called when in async mode.
 *
 * \return RIG_OK if the operation has been successful, otherwise
 * a negative value if an error occurred (in which case, cause is
 * set appropriately).
 */
int HAMLIB_API rig_set_vfo_callback(RIG *rig, vfo_cb_t cb, rig_ptr_t arg)
{
    ENTERFUNC;

    if (CHECK_RIG_ARG(rig))
    {
        RETURNFUNC(-RIG_EINVAL);
    }

    rig->callbacks.vfo_event = cb;
    rig->callbacks.vfo_arg = arg;

    RETURNFUNC(RIG_OK);
}


/**
 * \brief set the callback for ptt events
 * \param rig   The rig handle
 * \param cb    The callback to install
 * \param arg   A Pointer to some private data to pass later on to the callback
 *
 *  Install a callback for ptt events, to be called when in async mode.
 *
 * \return RIG_OK if the operation has been successful, otherwise
 * a negative value if an error occurred (in which case, cause is
 * set appropriately).
 */
int HAMLIB_API rig_set_ptt_callback(RIG *rig, ptt_cb_t cb, rig_ptr_t arg)
{
    ENTERFUNC;

    if (CHECK_RIG_ARG(rig))
    {
        RETURNFUNC(-RIG_EINVAL);
    }

    rig->callbacks.ptt_event = cb;
    rig->callbacks.ptt_arg = arg;

    RETURNFUNC(RIG_OK);
}


/**
 * \brief set the callback for dcd events
 * \param rig   The rig handle
 * \param cb    The callback to install
 * \param arg   A Pointer to some private data to pass later on to the callback
 *
 *  Install a callback for dcd events, to be called when in async mode.
 *
 * \return RIG_OK if the operation has been successful, otherwise
 * a negative value if an error occurred (in which case, cause is
 * set appropriately).
 */
int HAMLIB_API rig_set_dcd_callback(RIG *rig, dcd_cb_t cb, rig_ptr_t arg)
{
    ENTERFUNC;

    if (CHECK_RIG_ARG(rig))
    {
        RETURNFUNC(-RIG_EINVAL);
    }

    rig->callbacks.dcd_event = cb;
    rig->callbacks.dcd_arg = arg;

    RETURNFUNC(RIG_OK);
}


/**
 * \brief set the callback for pipelined tuning module
 * \param rig   The rig handle
 * \param cb    The callback to install
 * \param arg   A Pointer to some private data to pass later on to the callback
 * used to maintain state during pipelined tuning.
 *
 *  Install a callback for pipelined tuning module, to be called when the
 *  rig_scan( SCAN_PLT ) loop needs a new frequency, mode and width.
 *
 * \return RIG_OK if the operation has been successful, otherwise
 * a negative value if an error occurred (in which case, cause is
 * set appropriately).
 */
int HAMLIB_API rig_set_pltune_callback(RIG *rig, pltune_cb_t cb, rig_ptr_t arg)
{
    ENTERFUNC;

    if (CHECK_RIG_ARG(rig))
    {
        RETURNFUNC(-RIG_EINVAL);
    }

    rig->callbacks.pltune = cb;
    rig->callbacks.pltune_arg = arg;

    RETURNFUNC(RIG_OK);
}


/**
 * \brief set the callback for spectrum line reception events
 * \param rig   The rig handle
 * \param cb    The callback to install
 * \param arg   A Pointer to some private data to pass later on to the callback
 *
 *  Install a callback for spectrum line reception events, to be called when in async mode.
 *
 * \return RIG_OK if the operation has been successful, otherwise
 * a negative value if an error occurred (in which case, cause is
 * set appropriately).
 */
int HAMLIB_API rig_set_spectrum_callback(RIG *rig, spectrum_cb_t cb,
        rig_ptr_t arg)
{
    ENTERFUNC;

    if (CHECK_RIG_ARG(rig))
    {
        RETURNFUNC(-RIG_EINVAL);
    }

    rig->callbacks.spectrum_event = cb;
    rig->callbacks.spectrum_arg = arg;

    RETURNFUNC(RIG_OK);
}


/**
 * \brief control the transceive mode
 * \param rig   The rig handle
 * \param trn   The transceive status to set to
 *
 *  Enable/disable the transceive handling of a rig and kick off async mode.
 *
 * \return RIG_OK if the operation has been successful, otherwise
 * a negative value if an error occurred (in which case, cause is
 * set appropriately).
 *
 * \sa rig_get_trn()
 *
 * \deprecated This functionality has never worked correctly and it is now disabled in favor of new async data handling capabilities.
 * The command will always return RIG_EDEPRECATED until the command will be removed eventually.
 */
int HAMLIB_API rig_set_trn(RIG *rig, int trn)
{
    ENTERFUNC;
    RETURNFUNC(-RIG_EDEPRECATED);
}


/**
 * \brief get the current transceive mode
 * \param rig   The rig handle
 * \param trn   The location where to store the current transceive mode
 *
 *  Retrieves the current status of the transceive mode, i.e. if radio
 *  sends new status automatically when some changes happened on the radio.
 *
 * \return RIG_OK if the operation has been successful, otherwise
 * a negative value if an error occurred (in which case, cause is
 * set appropriately).
 *
 * \sa rig_set_trn()
 *
 * \deprecated This functionality has never worked correctly and it is now disabled in favor of new async data handling capabilities.
 * The command will always return RIG_EDEPRECATED until the command will be removed eventually.
 */
int HAMLIB_API rig_get_trn(RIG *rig, int *trn)
{
    ENTERFUNC;
    RETURNFUNC(-RIG_EDEPRECATED);
}

int rig_fire_freq_event(RIG *rig, vfo_t vfo, freq_t freq)
{
    ENTERFUNC;

    rig_debug(RIG_DEBUG_TRACE, "Event: freq changed to %"PRIll"Hz on %s\n",
              (int64_t)freq, rig_strvfo(vfo));

    rig_set_cache_freq(rig, vfo, freq);
    rig->state.use_cached_freq = 1;

    network_publish_rig_transceive_data(rig);

    if (rig->callbacks.freq_event)
    {
        rig->callbacks.freq_event(rig, vfo, freq, rig->callbacks.freq_arg);
    }

    RETURNFUNC(0);
}


int rig_fire_mode_event(RIG *rig, vfo_t vfo, rmode_t mode, pbwidth_t width)
{
    ENTERFUNC;

    rig_debug(RIG_DEBUG_TRACE, "Event: mode changed to %s, width %liHz on %s\n",
              rig_strrmode(mode), width, rig_strvfo(vfo));

    rig_set_cache_mode(rig, vfo, mode, width);
    rig->state.use_cached_mode = 1;

    network_publish_rig_transceive_data(rig);

    if (rig->callbacks.mode_event)
    {
        rig->callbacks.mode_event(rig, vfo, mode, width, rig->callbacks.mode_arg);
    }

    RETURNFUNC(0);
}


int rig_fire_vfo_event(RIG *rig, vfo_t vfo)
{
    ENTERFUNC;

    rig_debug(RIG_DEBUG_TRACE, "Event: vfo changed to %s\n", rig_strvfo(vfo));

    rig->state.cache.vfo = vfo;
    elapsed_ms(&rig->state.cache.time_vfo, HAMLIB_ELAPSED_SET);

    network_publish_rig_transceive_data(rig);

    if (rig->callbacks.vfo_event)
    {
        rig->callbacks.vfo_event(rig, vfo, rig->callbacks.vfo_arg);
    }

    RETURNFUNC(0);
}


int rig_fire_ptt_event(RIG *rig, vfo_t vfo, ptt_t ptt)
{
    ENTERFUNC;

    rig_debug(RIG_DEBUG_TRACE, "Event: PTT changed to %i on %s\n", ptt,
              rig_strvfo(vfo));

    rig->state.cache.ptt = ptt;
    elapsed_ms(&rig->state.cache.time_ptt, HAMLIB_ELAPSED_SET);

    network_publish_rig_transceive_data(rig);

    if (rig->callbacks.ptt_event)
    {
        rig->callbacks.ptt_event(rig, vfo, ptt, rig->callbacks.ptt_arg);
    }

    RETURNFUNC(0);
}


int rig_fire_dcd_event(RIG *rig, vfo_t vfo, dcd_t dcd)
{
    ENTERFUNC;

    rig_debug(RIG_DEBUG_TRACE, "Event: DCD changed to %i on %s\n", dcd,
              rig_strvfo(vfo));

    network_publish_rig_transceive_data(rig);

    if (rig->callbacks.dcd_event)
    {
        rig->callbacks.dcd_event(rig, vfo, dcd, rig->callbacks.dcd_arg);
    }

    RETURNFUNC(0);
}


int rig_fire_pltune_event(RIG *rig, vfo_t vfo, freq_t *freq, rmode_t *mode,
                          pbwidth_t *width)
{
    ENTERFUNC;

    rig_debug(RIG_DEBUG_TRACE, "Event: Pipelined tuning event, vfo=%s\n",
              rig_strvfo(vfo));

    network_publish_rig_transceive_data(rig);

    if (rig->callbacks.pltune)
    {
        rig->callbacks.pltune(rig, vfo, freq, mode, width, rig->callbacks.pltune_arg);
    }

    RETURNFUNC(RIG_OK);
}


static int print_spectrum_line(char *str, size_t length,
                               struct rig_spectrum_line *line)
{
    int data_level_max = line->data_level_max / 2;
    int aggregate_count = line->spectrum_data_length / 120;
    int aggregate_value = 0;
    int i, c;
    int charlen = strlen("█");

    str[0] = '\0';

    for (i = 0, c = 0; i < line->spectrum_data_length; i++)
    {
        int current = line->spectrum_data[i];
        aggregate_value = current > aggregate_value ? current : aggregate_value;

        if (i > 0 && i % aggregate_count == 0)
        {
            if (c + charlen >= length)
            {
                break;
            }

            int level = aggregate_value * 10 / data_level_max;

            if (level >= 8)
            {
                strcpy(str + c, "█");
                c += charlen;
            }
            else if (level >= 6)
            {
                strcpy(str + c, "▓");
                c += charlen;
            }
            else if (level >= 4)
            {
                strcpy(str + c, "▒");
                c += charlen;
            }
            else if (level >= 2)
            {
                strcpy(str + c, "░");
                c += charlen;
            }
            else if (level >= 0)
            {
                strcpy(str + c, " ");
                c += 1;
            }

            aggregate_value = 0;
        }
    }

    return c;
}


int rig_fire_spectrum_event(RIG *rig, struct rig_spectrum_line *line)
{
    ENTERFUNC;

    if (rig_need_debug(RIG_DEBUG_TRACE))
    {
        char spectrum_debug[line->spectrum_data_length * 4];
        print_spectrum_line(spectrum_debug, sizeof(spectrum_debug), line);
        rig_debug(RIG_DEBUG_TRACE, "%s: ASCII Spectrum Scope: %s\n", __func__,
                  spectrum_debug);
    }

    network_publish_rig_spectrum_data(rig, line);

    if (rig->callbacks.spectrum_event)
    {
        rig->callbacks.spectrum_event(rig, line, rig->callbacks.spectrum_arg);
    }

    RETURNFUNC(RIG_OK);
}

/** @} */
