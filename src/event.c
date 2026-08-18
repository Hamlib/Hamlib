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

#include "hamlib/config.h"

#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <time.h>

#ifdef HAVE_SYS_TIME_H
#  include <sys/time.h>
#endif

#if defined(__APPLE__)
#  include <mach/mach_time.h>
#endif

#include <pthread.h>

#include "hamlib/rig.h"
#include "hamlib/rig_state.h"
#include "event.h"
#include "misc.h"
#include "cache.h"
#include "network.h"

#define CHECK_RIG_ARG(r) (!(r) || !(r)->caps || !STATE(r)->comm_state)

typedef struct rig_poll_routine_args_s
{
    RIG *rig;
} rig_poll_routine_args;

typedef struct rig_poll_routine_priv_data_s
{
    pthread_t thread_id;
    pthread_mutex_t mutex;
    pthread_cond_t cond;
    int ready;
    int start_waiting;
    int stopped;
    rig_poll_routine_args args;
} rig_poll_routine_priv_data;

static int rig_poll_routine_is_running(const struct rig_state *rs);

static int64_t rig_poll_monotonic_ms(void)
{
#if defined(__APPLE__)
    mach_timebase_info_data_t timebase;
    uint64_t ticks = mach_absolute_time();

    mach_timebase_info(&timebase);
    return (int64_t)((long double) ticks * timebase.numer / timebase.denom
                     / 1000000.0L);
#elif defined(_WIN32)
    return (int64_t) GetTickCount64();
#else
    struct timespec now;

    clock_gettime(CLOCK_MONOTONIC, &now);
    return (int64_t) now.tv_sec * 1000 + now.tv_nsec / 1000000;
#endif
}

static void rig_poll_wait_until(rig_poll_routine_priv_data *poll_routine_priv,
                                struct rig_state *rs,
                                int64_t deadline_ms)
{
    int64_t delay_ms = deadline_ms - rig_poll_monotonic_ms();
    struct timespec timeout;
    struct timeval now;

    if (delay_ms <= 0)
    {
        return;
    }

    gettimeofday(&now, NULL);
    timeout.tv_sec = now.tv_sec + delay_ms / 1000;
    timeout.tv_nsec = now.tv_usec * 1000
                      + (delay_ms % 1000) * 1000000;

    if (timeout.tv_nsec >= 1000000000)
    {
        timeout.tv_sec++;
        timeout.tv_nsec -= 1000000000;
    }

    pthread_mutex_lock(&poll_routine_priv->mutex);

    if (rig_poll_routine_is_running(rs))
    {
        pthread_cond_timedwait(&poll_routine_priv->cond,
                               &poll_routine_priv->mutex, &timeout);
    }

    pthread_mutex_unlock(&poll_routine_priv->mutex);
}

static int rig_poll_routine_is_running(const struct rig_state *rs)
{
    return __atomic_load_n(&rs->poll_routine_thread_run, __ATOMIC_ACQUIRE);
}

static void rig_poll_routine_set_running(struct rig_state *rs, int running)
{
    __atomic_store_n(&rs->poll_routine_thread_run, running, __ATOMIC_RELEASE);
}

int rig_get_poll_interval(RIG *rig)
{
    return __atomic_load_n(&STATE(rig)->poll_interval, __ATOMIC_ACQUIRE);
}

void rig_set_poll_interval(RIG *rig, int interval_ms)
{
    __atomic_store_n(&STATE(rig)->poll_interval, interval_ms,
                     __ATOMIC_RELEASE);
}

static void *rig_poll_routine(void *arg)
{
    rig_poll_routine_args *args = (rig_poll_routine_args *)arg;
    RIG *rig = args->rig;
    struct rig_state *rs = STATE(rig);
    rig_poll_routine_priv_data *poll_routine_priv =
        (rig_poll_routine_priv_data *) rs->poll_routine_priv_data;
    struct rig_cache_snapshot snapshot;
    struct rig_poll_schedule schedule;
    struct rig_poll_schedule_result schedule_result;
    int update_occurred;

    vfo_t vfo = RIG_VFO_NONE, tx_vfo = RIG_VFO_NONE;
    freq_t freq_main_a = 0, freq_main_b = 0, freq_main_c = 0, freq_sub_a = 0,
           freq_sub_b = 0, freq_sub_c = 0;
    rmode_t mode_main_a = 0, mode_main_b = 0, mode_main_c = 0, mode_sub_a = 0,
            mode_sub_b = 0, mode_sub_c = 0;
    pbwidth_t width_main_a = 0, width_main_b = 0, width_main_c = 0, width_sub_a = 0,
              width_sub_b = 0, width_sub_c = 0;
    ptt_t ptt = RIG_PTT_OFF;
    split_t split = RIG_SPLIT_OFF;

    rig_debug(RIG_DEBUG_VERBOSE, "%s(%d): Starting rig poll routine thread\n",
              __FILE__, __LINE__);

    update_occurred = 0;
    rig_poll_schedule_init(&schedule, rig_poll_monotonic_ms(),
                           rig_get_poll_interval(rig));

    while (rig_poll_routine_is_running(rs))
    {
        schedule_result = rig_poll_schedule_advance(
                              &schedule, rig_poll_monotonic_ms(),
                              rig_get_poll_interval(rig));

        if (schedule_result.scan_cache)
        {
            rig_get_cache_snapshot(rig, &snapshot);

            if (snapshot.current_vfo != vfo)
            {
                vfo = snapshot.current_vfo;
                update_occurred = 1;
            }

            if (snapshot.tx_vfo != tx_vfo)
            {
                tx_vfo = snapshot.tx_vfo;
                update_occurred = 1;
            }

            if (snapshot.freqMainA != freq_main_a)
            {
                freq_main_a = snapshot.freqMainA;
                update_occurred = 1;
            }

            if (snapshot.freqMainB != freq_main_b)
            {
                freq_main_b = snapshot.freqMainB;
                update_occurred = 1;
            }

            if (snapshot.freqMainC != freq_main_c)
            {
                freq_main_c = snapshot.freqMainC;
                update_occurred = 1;
            }

            if (snapshot.freqSubA != freq_sub_a)
            {
                freq_sub_a = snapshot.freqSubA;
                update_occurred = 1;
            }

            if (snapshot.freqSubB != freq_sub_b)
            {
                freq_sub_b = snapshot.freqSubB;
                update_occurred = 1;
            }

            if (snapshot.freqSubC != freq_sub_c)
            {
                freq_sub_c = snapshot.freqSubC;
                update_occurred = 1;
            }

            if (snapshot.ptt != ptt)
            {
                ptt = snapshot.ptt;
                update_occurred = 1;
            }

            if (snapshot.split != split)
            {
                split = snapshot.split;
                update_occurred = 1;
            }

            if (snapshot.modeMainA != mode_main_a)
            {
                mode_main_a = snapshot.modeMainA;
                update_occurred = 1;
            }

            if (snapshot.modeMainB != mode_main_b)
            {
                mode_main_b = snapshot.modeMainB;
                update_occurred = 1;
            }

            if (snapshot.modeMainC != mode_main_c)
            {
                mode_main_c = snapshot.modeMainC;
                update_occurred = 1;
            }

            if (snapshot.modeSubA != mode_sub_a)
            {
                mode_sub_a = snapshot.modeSubA;
                update_occurred = 1;
            }

            if (snapshot.modeSubB != mode_sub_b)
            {
                mode_sub_b = snapshot.modeSubB;
                update_occurred = 1;
            }

            if (snapshot.modeSubC != mode_sub_c)
            {
                mode_sub_c = snapshot.modeSubC;
                update_occurred = 1;
            }

            if (snapshot.widthMainA != width_main_a)
            {
                width_main_a = snapshot.widthMainA;
                update_occurred = 1;
            }

            if (snapshot.widthMainB != width_main_b)
            {
                width_main_b = snapshot.widthMainB;
                update_occurred = 1;
            }

            if (snapshot.widthMainC != width_main_c)
            {
                width_main_c = snapshot.widthMainC;
                update_occurred = 1;
            }

            if (snapshot.widthSubA != width_sub_a)
            {
                width_sub_a = snapshot.widthSubA;
                update_occurred = 1;
            }

            if (snapshot.widthSubB != width_sub_b)
            {
                width_sub_b = snapshot.widthSubB;
                update_occurred = 1;
            }

            if (snapshot.widthSubC != width_sub_c)
            {
                width_sub_c = snapshot.widthSubC;
                update_occurred = 1;
            }
        }

        if (update_occurred || schedule_result.publish)
        {
            network_publish_rig_poll_data(rig);
            rig_poll_schedule_published(&schedule, rig_poll_monotonic_ms());
            update_occurred = 0;
        }

        pthread_mutex_lock(&poll_routine_priv->mutex);

        if (!poll_routine_priv->ready)
        {
            poll_routine_priv->ready = 1;
            pthread_cond_signal(&poll_routine_priv->cond);
        }

        pthread_mutex_unlock(&poll_routine_priv->mutex);
        rig_poll_wait_until(poll_routine_priv, rs,
                            schedule_result.next_deadline_ms);
    }

    network_publish_rig_poll_data(rig);

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
    struct rig_state *rs = STATE(rig);
    rig_poll_routine_priv_data *poll_routine_priv;
    int err;
    int stopped;

    ENTERFUNC;

    if (rig_get_poll_interval(rig) < 1)
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

    rs->poll_routine_priv_data = calloc(1, sizeof(rig_poll_routine_priv_data));

    if (rs->poll_routine_priv_data == NULL)
    {
        RETURNFUNC(-RIG_ENOMEM);
    }

    poll_routine_priv = (rig_poll_routine_priv_data *) rs->poll_routine_priv_data;
    poll_routine_priv->args.rig = rig;
    poll_routine_priv->start_waiting = 1;

    err = pthread_mutex_init(&poll_routine_priv->mutex, NULL);

    if (err)
    {
        rig_debug(RIG_DEBUG_ERR, "%s(%d) pthread_mutex_init error: %s\n",
                  __FILE__, __LINE__, strerror(err));
        free(rs->poll_routine_priv_data);
        rs->poll_routine_priv_data = NULL;
        RETURNFUNC(-RIG_EINTERNAL);
    }

    err = pthread_cond_init(&poll_routine_priv->cond, NULL);

    if (err)
    {
        rig_debug(RIG_DEBUG_ERR, "%s(%d) pthread_cond_init error: %s\n",
                  __FILE__, __LINE__, strerror(err));
        pthread_mutex_destroy(&poll_routine_priv->mutex);
        free(rs->poll_routine_priv_data);
        rs->poll_routine_priv_data = NULL;
        RETURNFUNC(-RIG_EINTERNAL);
    }

    rig_poll_routine_set_running(rs, 1);
    err = pthread_create(&poll_routine_priv->thread_id, NULL,
                         rig_poll_routine, &poll_routine_priv->args);

    if (err)
    {
        rig_debug(RIG_DEBUG_ERR, "%s(%d) pthread_create error: %s\n", __FILE__,
                  __LINE__,
                  strerror(err));
        rig_poll_routine_set_running(rs, 0);
        pthread_cond_destroy(&poll_routine_priv->cond);
        pthread_mutex_destroy(&poll_routine_priv->mutex);
        free(rs->poll_routine_priv_data);
        rs->poll_routine_priv_data = NULL;
        RETURNFUNC(-RIG_EINTERNAL);
    }

    pthread_mutex_lock(&poll_routine_priv->mutex);

    while (!poll_routine_priv->ready && !poll_routine_priv->stopped)
    {
        pthread_cond_wait(&poll_routine_priv->cond,
                          &poll_routine_priv->mutex);
    }

    stopped = poll_routine_priv->stopped;
    poll_routine_priv->start_waiting = 0;
    pthread_cond_broadcast(&poll_routine_priv->cond);
    pthread_mutex_unlock(&poll_routine_priv->mutex);

    if (stopped)
    {
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
    struct rig_state *rs = STATE(rig);
    rig_poll_routine_priv_data *poll_routine_priv;

    ENTERFUNC;

    if (rs->poll_routine_priv_data == NULL)
    {
        RETURNFUNC(RIG_OK);
    }

    poll_routine_priv = (rig_poll_routine_priv_data *) rs->poll_routine_priv_data;
    rig_poll_routine_set_running(rs, 0);
    pthread_mutex_lock(&poll_routine_priv->mutex);
    poll_routine_priv->stopped = 1;
    pthread_cond_broadcast(&poll_routine_priv->cond);
    pthread_mutex_unlock(&poll_routine_priv->mutex);

    if (poll_routine_priv->thread_id != 0)
    {
        int err = pthread_join(poll_routine_priv->thread_id, NULL);

        if (err)
        {
            rig_debug(RIG_DEBUG_ERR, "%s(%d): pthread_join error %s\n", __FILE__, __LINE__,
                      strerror(err));
            RETURNFUNC(-RIG_EINTERNAL);
        }

        poll_routine_priv->thread_id = 0;
    }

    pthread_mutex_lock(&poll_routine_priv->mutex);

    while (poll_routine_priv->start_waiting)
    {
        pthread_cond_wait(&poll_routine_priv->cond,
                          &poll_routine_priv->mutex);
    }

    pthread_mutex_unlock(&poll_routine_priv->mutex);
    pthread_cond_destroy(&poll_routine_priv->cond);
    pthread_mutex_destroy(&poll_routine_priv->mutex);
    free(rs->poll_routine_priv_data);
    rs->poll_routine_priv_data = NULL;

    RETURNFUNC(RIG_OK);
}

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
 * The command will always return -RIG_EDEPRECATED until the command will be removed eventually.
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
 * The command will always return -RIG_EDEPRECATED until the command will be removed eventually.
 */
int HAMLIB_API rig_get_trn(RIG *rig, int *trn)
{
    ENTERFUNC;
    RETURNFUNC(-RIG_EDEPRECATED);
}

int rig_fire_freq_event(RIG *rig, vfo_t vfo, freq_t freq)
{
    ENTERFUNC;

    struct rig_state *rs = STATE(rig);
    double dfreq = freq;
    rig_debug(RIG_DEBUG_TRACE, "Event: freq changed to %.0f Hz on %s\n",
              dfreq, rig_strvfo(vfo));

    rig_set_cache_freq(rig, vfo, freq);

    // This doesn't work well for Icom rigs -- no way to tell which VFO we're on
    // Should work for most other rigs using AI1; mode
    if (RIG_BACKEND_NUM(rig->caps->rig_model) != RIG_ICOM)
    {
        rs->use_cached_freq = 1;
    }

    if (rs->freq_event_elapsed.tv_sec == 0)
    {
        elapsed_ms(&rs->freq_event_elapsed, HAMLIB_ELAPSED_SET);
    }

    double e = elapsed_ms(&rs->freq_event_elapsed, HAMLIB_ELAPSED_GET);

    if (e >= 250) // throttle events to 4 per sec
    {
        elapsed_ms(&rs->freq_event_elapsed, HAMLIB_ELAPSED_SET);
        network_publish_rig_transceive_data(rig);

        if (rig->callbacks.freq_event)
        {
            rig->callbacks.freq_event(rig, vfo, freq, rig->callbacks.freq_arg);
        }
    }

    RETURNFUNC(0);
}

int rig_fire_mode_event(RIG *rig, vfo_t vfo, rmode_t mode, pbwidth_t width)
{
    ENTERFUNC;

    rig_debug(RIG_DEBUG_TRACE, "Event: mode changed to %s, width %liHz on %s\n",
              rig_strrmode(mode), width, rig_strvfo(vfo));

    rig_set_cache_mode(rig, vfo, mode, width);

    // This doesn't work well for Icom rigs -- no way to tell which VFO we're on
    // Should work for most other rigs using AI1; mode
    if (RIG_BACKEND_NUM(rig->caps->rig_model) != RIG_ICOM)
    {
        STATE(rig)->use_cached_mode = 1;
    }

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

    rig_observe_current_vfo(rig, vfo);

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

    rig_set_cache_ptt(rig, ptt);

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
